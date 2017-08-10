#include "ffmpeg_decoder.h"

bool logSeeking;
bool logReading = false;
volatile bool ffmpegerror;

extern void my_av_log(void*, int level,const char *fmt,...);

static bool doSeekPos = false;
#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080

#ifdef FORCE_CODECWIDTH
#define FRAMEWIDTH pCodecCtx->width
#define FRAMEHEIGHT pCodecCtx->height
#else
#define FRAMEWIDTH pFrame->width
#define FRAMEHEIGHT pFrame->height
#endif

void my_av_dolog(void * /*ptr*/, int level, const char *fmt,va_list vl)
{
   if( level >= 16 /*AV_LOG_ERROR*/ )
      return;
   static char line[1024];
   vsnprintf( line,sizeof(line),fmt,vl);
   int iLen = strlen(line);
   if( line[iLen-1] == '\n' )
      line[iLen-1] = '\0';
   //fprintf(stderr,line);
   esyslog("ffmpeg(%d): %s",level,line);
}

#ifdef USE_FFMPEG
extern cFileName *cfn;
extern cNoadIndexFile *cIF;

#ifdef USE_AVIOCONTEXT
static int noad_read_packet(void *h, uint8_t *buf, int size)
{
  FFMPegDecoder *decoder = (FFMPegDecoder*)(h);
  if( decoder->__bufBytes <= 0 )
  {
    if( decoder->ffmpeg_index >= 0 )
    {
      if( !decoder->cont_reading )
			  if( decoder->ffmpeg_Lastindex == decoder->ffmpeg_index )
				  return 0;
		bool Independent;
		if( !cIF->Get(decoder->ffmpeg_index,&decoder->ffmpegFileNumber, &decoder->ffmpegFileOffset, &Independent, &decoder->ffmpegLength) )
			return 0;
      if( decoder->ffmpegLength <= 0 )
        ffmpegerror = true;
      if( logReading )
         dsyslog("noad_read read from file, ffmpeg_index = %d (iframe: %s), length = %d",decoder->ffmpeg_index, Independent?"true":"false", decoder->ffmpegLength);
		  if( decoder->cont_reading )
			  decoder->ffmpeg_index++;
		  decoder->ffmpeg_Lastindex = decoder->ffmpeg_index;
    }
    else
    {
      if( logReading )
         dsyslog("noad_read read from file, ffmpegFileOffset = %d, length = %d",decoder->ffmpegFileOffset, decoder->ffmpegLength);
      if( decoder->ffmpegFileOffset == decoder->ffmpegLastFileOffset )
        return 0;
    }
    cfn->SetOffset( decoder->ffmpegFileNumber, decoder->ffmpegFileOffset);
    ReadFrame (cfn->File(), readBuffer, decoder->ffmpegLength, MAXFRAMESIZE);
    decoder->__bufBytes = decoder->ffmpegLength;
    decoder->__bufPos = readBuffer; 
    decoder->ffmpegLastFileOffset = decoder->ffmpegFileOffset;
  }
  if( decoder->__bufBytes > 0 )
  {
    int cpBytes = size > decoder->__bufBytes?decoder->__bufBytes:size;
    memcpy(buf,decoder->__bufPos, cpBytes);
		//dsyslog("noad_read --> return %d bytes of %d bytes",cpBytes,__bufBytes);
    decoder->__bufPos += cpBytes;
    decoder->__bufBytes -= cpBytes;
    return cpBytes;
  }
	return 0;
}

static int noad_write_packet(void */*h*/, uint8_t */*buf*/, int /*size*/)
{
  //FFMPegDecoder *decoder = (FFMPegDecoder*)(h->priv_data);
	/*
	BUF*	af = h->priv_data;
	int		n;
	n = min(size, af->buflen-af->bufused);
	if(n<0)
		return -1;
	memcpy(af->buf+af->bufused, buf, n);
	af->buf_used+=size;
	return n;
	*/
	return 0;
}


static int64_t noad_seek(void *h, int64_t pos, int whence)
{
  FFMPegDecoder *decoder = (FFMPegDecoder*)(h);
	// To implement this you need a bufcur element in BUF too
	//...
	if( whence == AVSEEK_SIZE )
	{
		return cIF->getVideoFileSize();
	}
	else if ( whence == SEEK_SET )
	{
      if( doSeekPos )
         decoder->ffmpeg_index = cIF->getIndexForFilepos(pos);
      if( logSeeking )
         dsyslog("noad-protocol seek to %lld --> index %d",pos,decoder->ffmpeg_index);
		decoder->__bufBytes = 0;
	}
	return pos;
}
#endif

FFMPegDecoder::FFMPegDecoder() :
	MPEGDecoder()
{
	pFormatCtx = NULL;
	//i, videoStream;
	pCodecCtx = NULL;
	pCodec = NULL;
	pFrame = NULL;
	__bufBytes = 0;
	ffmpeg_index=0;
	ffmpeg_Lastindex=0;
	cont_reading = false;
   av_log_set_callback(my_av_dolog);
   av_log_set_level(AV_LOG_ERROR);
#ifdef USE_AVIOCONTEXT
   pIOContext = NULL;
#endif
}

FFMPegDecoder::~FFMPegDecoder()
{
	decoder_exit();
}

int FFMPegDecoder::decoder_init()
{
	// Register all formats and codecs
	av_register_all();
#ifdef USE_AVIOCONTEXT
#define IOBUFSIZE (1024L*200L)
   iobuffer = (unsigned char*)av_malloc(IOBUFSIZE);
   pIOContext =  avio_alloc_context(
                  iobuffer,
                  IOBUFSIZE,
                  0,
                  this,
                  noad_read_packet,
                  noad_write_packet,
                  noad_seek);
#endif
	   return 0;
}

int FFMPegDecoder::openFile(cFileName *_cfn, cNoadIndexFile *_cIF)
{
	cfn =_cfn;
	cIF = _cIF;
	// Open video file
	ffmpeg_index=0;
	cont_reading = true;
   doSeekPos = true;

#ifdef USE_AVIOCONTEXT
   pFormatCtx = avformat_alloc_context();
   pFormatCtx->pb = pIOContext;
	char somebuf[1024];
   sprintf(somebuf, "noad:0x%08lx", (long unsigned int)this);
   int openCode = avformat_open_input(&pFormatCtx,"name",NULL,NULL);
   if( openCode != 0 )
		return -1; // Couldn't open file
#endif

	// Retrieve stream information
	resetDecoder();
   int openCode2 = av_find_stream_info(pFormatCtx);
	if(openCode2<0)
		return -1; // Couldn't find stream information

	// Find the first video stream
	videoStream=-1;
	for(i=0; i < (int)pFormatCtx->nb_streams; i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoStream=i;
			break;
		}
	}
	if(videoStream==-1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
	pCodecCtx->flags|=CODEC_FLAG_EMU_EDGE;

	if( pCodecCtx->codec_id == CODEC_ID_PROBE )
	{
		esyslog("can't detect codec for video!");
		return -1;
	}
	//av_close_input_file(pFormatCtx);

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
		return -1; // Codec not found

	// Inform the codec that we can handle truncated bitstreams -- i.e.,
	// bitstreams where frame boundaries can fall in the middle of packets
	if(pCodec->capabilities & CODEC_CAP_TRUNCATED)
		pCodecCtx->flags|=CODEC_FLAG_TRUNCATED;

#if LIBAVCODEC_VERSION_MAJOR > 54
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec,&avDictionary) < 0)
#else
	// Open codec
	if(avcodec_open(pCodecCtx, pCodec)<0)
#endif
		return -1; // Could not open codec



	// Allocate video frame
   pFrame=avcodec_alloc_frame();
	cont_reading = true;
   doSeekPos = false;
   dsyslog("init_ffmpeg done" );
	return false;
}

int FFMPegDecoder::decoder_exit()
{
	// Free the YUV frame
	if( pFrame )
	{
		av_free(pFrame);
		pFrame = NULL;
	}

	// close the file
	if( pFormatCtx )
	{
		av_close_input_file(pFormatCtx);
		pFormatCtx = NULL;
	}
	// Close the codec
	if( pCodecCtx )
	{
		//avcodec_close(pCodecCtx);
		pCodecCtx = NULL;
	}
#ifdef USE_AVIOCONTEXT
   if( pIOContext )
   {
      //av_free(iobuffer);
      av_free(pIOContext);
      pIOContext = NULL;
	}
#endif
#if LIBAVCODEC_VERSION_MAJOR > 54
   av_dict_free(&avDictionary);
   avDictionary = NULL;
#endif
	return 0;
}

bool FFMPegDecoder::GetVideoFrame(bool restart)
{
  static AVPacket packet;
  static int      bytesRemaining=0;
  static uint8_t  *rawData;
  static bool     fFirstTime=true;
  int             bytesDecoded;
  int             frameFinished;

  if( restart )
  {
    bytesRemaining=0;
    if(packet.data!=NULL)
      av_free_packet(&packet);
    packet.data=NULL;
  }
  // First time we're called, set packet.data to NULL to indicate it
  // doesn't have to be freed
  if(fFirstTime)
  {
    fFirstTime=false;
    packet.data=NULL;
  }

  // Decode packets until we have decoded a complete frame
  while(true)
  {
    // Work on the current packet until we have decoded all of it
    while(bytesRemaining > 0)
    {
      // Decode the next chunk of data
      bytesDecoded=avcodec_decode_video2(pCodecCtx, pFrame,&frameFinished, &packet);
		//dsyslog("ffmpeg decoded %d bytes from %d bytes",bytesDecoded,bytesRemaining);

      // Was there an error?
      if(bytesDecoded < 0)
      {
          esyslog("Error while decoding frame");
		    //Mpeg1Context *s1 = pCodecCtx->priv_data;
			 //MpegEncContext *s = &s1->mpeg_enc_ctx; 
			 av_free_packet(&packet);
          packet.data=NULL;
          bytesRemaining=0;
          return false;
      }

      bytesRemaining-=bytesDecoded;
      rawData+=bytesDecoded;

      // Did we finish the current frame? Then we can return
      if(frameFinished)
      {
#ifdef FFMPEG_SUPPRESS_NOPICTURE_ERROR
			if( pFrame->data[0] && pFrame->data[1] && pFrame->data[2] )
				return true;
			else
				esyslog("ffmpeg get picture without data!");
#else
			  //dsyslog("ffmpeg get picture #%d, leaving %d bytes in buffer", pFrame->coded_picture_number,bytesRemaining);
			  //gotPicture = true;
			  return true;
#endif
		}
    }

    // Read the next packet, skipping all packets that aren't for this
    // stream
    do
    {
      // Free old packet
      if(packet.data!=NULL)
          av_free_packet(&packet);

      // Read new packet
			if(av_read_frame(pFormatCtx, &packet)<0)
        goto loop_exit;
    } while(packet.stream_index!=videoStream);

    //dsyslog("found packet with %d bytes of data", packet.size);
		  
    bytesRemaining=packet.size;
    rawData=packet.data;
  }

loop_exit:

  // Decode the rest of the last frame
  bytesDecoded=avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

  // Free last packet
  if(packet.data!=NULL)
      av_free_packet(&packet);

  ffmpegerror = true;

  return frameFinished!=0;
}


bool FFMPegDecoder::getNextPicture(int startIndex, int flags )
{
	bool bRet = false;

	bool restart = false;
	ffmpegerror = false;
	if (flags & DEMUX_RESET)
	{
		ffmpeg_index = startIndex;
		resetDecoder(startIndex);
		restart = true;
	}
//	iCurrentDecodedFrame = startIndex;
	while( !bRet && ffmpeg_index < cIF->getLast() && !ffmpegerror )
	{
		bRet = GetVideoFrame(restart);
      if( bRet && callBackFunc )
      {
         if( (FRAMEWIDTH <= MAX_WIDTH && FRAMEWIDTH > 0) && (FRAMEHEIGHT <= MAX_HEIGHT && FRAMEHEIGHT > 0 ) )
         {
            noadYUVBuf lbuf(pFrame->data,pFrame->linesize,FRAMEWIDTH,FRAMEHEIGHT);
            callBackFunc(&lbuf);
         }
         else
            esyslog("ERROR: faulty width (%d) or height(%d)",FRAMEWIDTH,FRAMEHEIGHT);
      }
	}
	startIndex = ffmpeg_index; 
	return bRet;
}

bool FFMPegDecoder::getPictures(int &startIndex, int flags, stopFunction stop )
{
	bool bRet = false;
	bool bStop = false;

	//dsyslog("getPicture from index %d", startIndex );
	bool restart = false;
	ffmpegerror = false;
	if (flags & DEMUX_RESET)
	{
		resetDecoder(startIndex);
		restart = true;
	}
	else
		if( startIndex < ffmpeg_index )
			return false;
	ffmpeg_index = startIndex;
	while( ffmpeg_index < cIF->getLast() && !ffmpegerror && !bStop )
	{
		bRet = GetVideoFrame(restart);
		restart = false;
      if( bRet && callBackFunc )
      {
         if( (FRAMEWIDTH <= MAX_WIDTH && FRAMEWIDTH > 0) && (FRAMEHEIGHT <= MAX_HEIGHT && FRAMEHEIGHT > 0 ) )
         {
            noadYUVBuf lbuf(pFrame->data,pFrame->linesize,FRAMEWIDTH,FRAMEHEIGHT);
            callBackFunc(&lbuf);
            bStop = stop();
         }
         else
            esyslog("ERROR: faulty width (%d) or height(%d)",FRAMEWIDTH,FRAMEHEIGHT);
      }
	}
	startIndex = ffmpeg_index; 
	return bRet;
}

bool FFMPegDecoder::getGOP(int &startIndex )
{
	bool bRet = false;
	uint16_t FileNumber;
	off_t FileOffset;
	bool Independent;
	int gopStartFrame = startIndex; 
	cIF->Get(startIndex, &FileNumber, &FileOffset, &Independent);
	if( !Independent )
		gopStartFrame = cIF->GetNextIFrame( startIndex, false, NULL, NULL, NULL, true);
	//dsyslog("getGOP from index %d (%d)", startIndex, gopStartFrame );
	bool restart = true;
	ffmpegerror = false;


	ffmpeg_index = gopStartFrame;
	bool bFirst = true;
	if( GOP_Buffer.startindex != gopStartFrame )
	{
		resetDecoder(gopStartFrame);
		GOP_Buffer.numpics = 0;
		while( ffmpeg_index < cIF->getLast() && !ffmpegerror && GOP_Buffer.numpics < PICS_IN_GOP )
		{
			bRet = GetVideoFrame(restart);
			restart = false;
			if( bRet )
			{
            if( (FRAMEWIDTH <= MAX_WIDTH && FRAMEWIDTH > 0) && (FRAMEHEIGHT <= MAX_HEIGHT && FRAMEHEIGHT > 0 ) )
            {
               if( bFirst )
                  GOP_Buffer.init(FRAMEWIDTH,FRAMEHEIGHT,pFrame->linesize[0],pFrame->linesize[1]);
               bFirst = false;
               GOP_Buffer.addPic(pFrame->data,pFrame->linesize[0],pFrame->linesize[1]);
            }
            else
               esyslog("ERROR: faulty width (%d) or height(%d)",FRAMEWIDTH,FRAMEHEIGHT);
         }
		}
		GOP_Buffer.startindex = gopStartFrame;
	}
	if( GOP_Buffer.numpics && callBackFunc )
	{
		int offset = startIndex - gopStartFrame;
        callBackFunc(&GOP_Buffer.gop_buf[offset]);
	}
	startIndex = ffmpeg_index; 
	return bRet;
}

extern "C" void av_read_frame_flush(AVFormatContext *s);
void FFMPegDecoder::resetDecoder()
{
	//if( pFormatCtx )
	//	av_read_frame_flush(pFormatCtx);
	if( pCodecCtx )
	{
		pCodecCtx->codec->flush(pCodecCtx);
		avcodec_flush_buffers(pCodecCtx);
	}
	if( pFrame )
		av_free(pFrame);
	pFrame=avcodec_alloc_frame();
	__bufBytes = 0;
}

void FFMPegDecoder::resetDecoder(int iFrame)
{
	__bufBytes = 0;
	ffmpeg_index = iFrame;
	cIF->Get(ffmpeg_index,&ffmpegFileNumber, &ffmpegFileOffset);
	avformat_seek_file(pFormatCtx, videoStream, ffmpegFileOffset, ffmpegFileOffset, ffmpegFileOffset, AVSEEK_FLAG_BYTE);
   if( pCodecCtx && pCodecCtx->codec )
      avcodec_flush_buffers(pCodecCtx);
}

#endif //USE_FFMPEG
