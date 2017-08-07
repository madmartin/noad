#include "mpeg_decoder.h"
#include <time.h>

//todo: eliminate this:
uint64_t ac3pts = 0;
uint64_t audiopts = 0;
uint64_t videopts = 0;

audiocbfunc current_audiocbf = NULL;
playaudiocbfunc current_playaudiocbf = NULL;
int iCurrentDecodedFrame = 0;
bool streamHasAC3;
int ac3mode;
int demux_pid;
int demux_track;
int LastDemuxIndex;
bool dodumpts;
bool isOnlinescan;
cNoadIndexFile *cCurrentCIF;

void MarkToggle(cMarks *marks, int index, const char *Comment=NULL);

// GOP_Buffer
gop_buffer::gop_buffer()
{
	startindex = -1;
	width = 0;
	height = 0;
	numpics = 0;
}

void gop_buffer::init(int _width, int _height)
{
  if( width != _width || height != _height )
  {
	  width = _width;
	  height = _height;
	  for(int i = 0; i < PICS_IN_GOP; i++)
	  {
         gop_buf[i].alloc(width,height);
      }
   }
}

void gop_buffer::init(int _width, int _height, int linewidth1, int linewidth2)
{
   if( width != _width || height != _height )
   {
      width = _width;
      height = _height;
      for(int i = 0; i < PICS_IN_GOP; i++)
      {
         gop_buf[i].alloc(width,height,linewidth1,linewidth2);
      }
   }
}

void gop_buffer::releaseBuffers()
{
	for(int i = 0; i < PICS_IN_GOP; i++)
	{
      gop_buf[i].release();
   }
  width = height = 0;
}

void gop_buffer::addPic(uint8_t * const buf[3])
{
	if( numpics < PICS_IN_GOP )
	{
      gop_buf[numpics].copyData(buf);
	}
	numpics++;
}
void gop_buffer::addPic(uint8_t * const buf[3], int linesize1, int linesize2)
{
	if( numpics < PICS_IN_GOP )
      gop_buf[numpics].copyData(buf,linesize1, linesize2);
	numpics++;
}
// GOP_Buffer end

// MPEGDecoder
static const uint16_t nfchans[11] = {2,1,2,3,3,4,4,5,1,1,2};
MPEGDecoder::MPEGDecoder()
{
	cfn = NULL;
	cIF = NULL;
	initialised = false;
	callBackFunc = NULL;
	pcbf = NULL;
	state = DEMUX_SKIP;
	state_bytes = 0;
	bTSFoundPayload = false;
	ac3Decoded = false;
}

MPEGDecoder::~MPEGDecoder()
{
   GOP_Buffer.releaseBuffers();
}

bool MPEGDecoder::detect_ac3_51(int StartFrame)
{
	dsyslog( "start ac3-detection");
	time_t tstart, tend;
	tstart = time(NULL);
   noadCallback oldCallback = getCallback();
//	setCB_Func(simpleCallback);
//	current_audiocbf = audiocallback;
	bool hasAC3 = streamHasAC3 = false;
	ac3mode = 0;
	iCurrentDecodedFrame = 0;
	//checkedFrames = 0;
	int flags = DEMUX_RESET;
	uint16_t FileNumber;         // current file-number
	off_t FileOffset;           // current file-offset
	int Length;
	if( !cIF->Get( StartFrame, &FileNumber, &FileOffset, NULL, &Length) )
		return false;
	int iCurrentFrame = StartFrame;
	int ac3mode51count = 0;
	while(  iCurrentFrame < cIF->Last() && iCurrentFrame < StartFrame+FRAMES_FOR_AC51DETECT && ac3mode51count < 50 )
	{
		//cfn->SetOffset( FileNumber, FileOffset);
		//end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
		iCurrentDecodedFrame = iCurrentFrame;
    if( pcbf )
      pcbf(iCurrentFrame);
		//demux (readBuffer, end, 0);
		//demuxFrame(cfn, FileNumber, FileOffset, Length, flags );
		demuxFrame_audio(iCurrentFrame, flags );
		flags = 0;
		if( ac3mode == 51 )
		{
			while(  iCurrentFrame < cIF->Last() && iCurrentFrame < StartFrame+FRAMES_FOR_AC51DETECT && ac3mode51count < 50 && ac3mode == 51)
			{
				iCurrentFrame++;
				if (iCurrentFrame < cIF->Last())
				{
					iCurrentDecodedFrame = iCurrentFrame;
					demuxFrame_audio(iCurrentFrame, flags );
					if( ac3mode == 51 )
						ac3mode51count++;
				}
			}
		}
		else
			ac3mode51count = 0;
		dsyslog( "ac3-detection at %d ac3mode is %d", iCurrentFrame, ac3mode);
		iCurrentFrame += AC3_51DETECT_SKIP;
		flags = DEMUX_RESET;
		if (iCurrentFrame < cIF->Last())
			cIF->Get( iCurrentFrame, &FileNumber, &FileOffset, NULL, &Length);
	}
   setCallback(oldCallback);
	current_audiocbf = NULL;

	tend = time(NULL);
	secsForAC3Detection = (int)(tend - tstart);
	dsyslog( "ac3-detection done, hasAC3 is %s", (ac3mode == 51)?"true":"false");
	if( ac3mode51count >= 50 )
		return true;
	return false;
}

bool MPEGDecoder::ac3Pass1(cMarks *pmarks)
{
	bool bHasAC3_51 = false;
	int cframe = 0;
	int iLastLogoFrame = 0;
	time_t tstart, tend;
	//current_audiocbf = audiocallback;
	bHasAC3_51 = detect_ac3_51(0);
	//current_audiocbf = NULL;
	if( !bHasAC3_51 )
		return false;
	tstart = time(NULL);
	//current_audiocbf = audiocallback;
	//cbfunc cbf_old = getCB_Func();
	//setCB_Func(simpleCallback);

	ac3mode = 0;
	int oldac3mode = ac3mode;
	iCurrentDecodedFrame = 0;
	int checkedFrames = 0;
	uint16_t FileNumber;         // current file-number
	off_t FileOffset;           // current file-offset
	int Length;
	int iCurrentFrame = cIF->Get( 0, &FileNumber, &FileOffset, NULL, &Length);

	dsyslog( "start ac3 pass1");
	//if( backupMarks )
	//	pmarks->Backup(filename);
	pmarks->Load(cfn->dirName(),DEFAULTFRAMESPERSECOND,cfn->isPES());
	pmarks->ClearList();
	bool bMarkChanged = true;
	int flags = DEMUX_RESET;

	while(  iCurrentFrame < cIF->Last() )
	{
		ac3Decoded = false;
		while(  iCurrentFrame < cIF->Last() && !ac3Decoded )
		{
			iCurrentDecodedFrame = iCurrentFrame;
      if( pcbf )
        pcbf(iCurrentFrame);
			demuxFrame_audio(iCurrentFrame, flags );
			flags = 0;
			if( !ac3Decoded )
				iCurrentFrame++;
		}
		dsyslog( "Audio: oldac3mode = %d ac3mode = %d at %d", oldac3mode, ac3mode,iCurrentFrame);
		if( oldac3mode != ac3mode && ((oldac3mode==51 && ac3mode !=51 )||(oldac3mode!=51 && ac3mode==51)) )
		{
			int ac3mode_help = ac3mode;
			int iStart = iCurrentFrame - AC3_51DETECT_SKIP;
			if( iStart < 1 )
				iStart = 1;
			cframe = find_ac3start( iStart, iCurrentFrame, ac3mode);
			if( oldac3mode == 51 || oldac3mode == 0 ||ac3mode == 51 )
			{
				cframe = findVideoByPTS(ac3pts, cframe);
				iLastLogoFrame = cIF->GetNextIFrame( cframe, ac3mode_help == 51, &FileNumber, &FileOffset, &Length, true);
				dsyslog( "AudioChanged: iLastLogoFrame=%6d, audio changed from %d to %d ", iLastLogoFrame, oldac3mode,ac3mode_help);
				MarkToggle(pmarks, iLastLogoFrame, ac3mode_help != 51 ? "ac3 lost" : "ac3 start");
				bMarkChanged = true;
			}
			oldac3mode = ac3mode = ac3mode_help;
		}
		iCurrentFrame += AC3_51DETECT_SKIP;
		flags = DEMUX_RESET;
	}
	//setCB_Func(cbf_old);
	//current_audiocbf = NULL;
	tend = time(NULL);
	int secsForAC3Scan = (int)(tend - tstart);
	dsyslog( "ac3 pass1 done");
	int iaframes = pmarks->getActiveFrames(cIF->Last());
	dsyslog( "ac3 pass1 marks %d frames of %d as active",iaframes,cIF->Last());
	if( iaframes < ( cIF->Last() * AC3ACTIVECUTOFF / 100 ) )
	{
		dsyslog( "ac3 pass1 kicks out more than %d%%, marks dropped", 100-AC3ACTIVECUTOFF);
		pmarks->ClearList();
		return false;
	}
	return true;
}

int MPEGDecoder::findVideoByPTS(uint64_t apts, int iStartFrame)
{
	//dsyslog( "findVideoByPTS from frame %d", iStartFrame);
	int flags = DEMUX_RESET;
	do
	{
		demuxFrame_audio(iStartFrame, flags );
		flags = 0;
		iStartFrame--;
	} while( iStartFrame > 0 && videopts > apts );
	return iStartFrame+1;
}

int MPEGDecoder::find_ac3start( int iStart, int iEnd, int mode2Find)
{
	int iCurrentFrame = 0;
	int ipos;
	int diff = iEnd-iStart;
	int flags = DEMUX_RESET;
	while( diff > 10 )
	{
		ac3Decoded = false;
		ipos = iStart+((iEnd - iStart + 1) / 2);
		if( ipos == iCurrentFrame )
			ipos--;
		while( !ac3Decoded && ipos < iEnd )
		{
			iCurrentFrame = ipos;
			iCurrentDecodedFrame = iCurrentFrame;
			demuxFrame_audio(ipos++, flags );
			flags = 0;
		}
		if( ac3mode == mode2Find )
		{
			iEnd = iCurrentFrame;
		}
		else
		{
			iStart = iCurrentFrame;
		}
		diff = iEnd-iStart;
	}
	iCurrentFrame = iStart;
	ac3mode = 0;
	flags = DEMUX_RESET;
	while( iCurrentFrame <= iEnd && ac3mode != mode2Find)
	{
		iCurrentFrame++;
		iCurrentDecodedFrame = iCurrentFrame;
		ac3mode = 0;
		demuxFrame_audio(iCurrentFrame, flags );
		flags = 0;
	}
	return iCurrentFrame-1;
}

void MPEGDecoder::demux_reset(void)
{
	state = DEMUX_SKIP;
	state_bytes = 0;
}

int MPEGDecoder::demuxPES_audio (uint8_t * buf, uint8_t * end, int flags)
{
	int iRet = 0;
  static int mpeg1_skip_table[16] =
  {
    0, 0, 4, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  /*
   * the demuxer keeps some state between calls:
   * if "state" = DEMUX_HEADER, then "head_buf" contains the first
   *     "bytes" bytes from some header.
   * if "state" == DEMUX_DATA, then we need to copy "bytes" bytes
   *     of ES data before the next header.
   * if "state" == DEMUX_SKIP, then we need to skip "bytes" bytes
   *     of data before the next header.
   *
   * NEEDBYTES makes sure we have the requested number of bytes for a
   * header. If we dont, it copies what we have into head_buf and returns,
   * so that when we come back with more data we finish decoding this header.
   *
   * DONEBYTES updates "buf" to point after the header we just parsed.
   */


  static uint8_t head_buf[264];

  uint8_t * header;
  int bytes;
  int len;

#define NEEDBYTES(x)					\
  do {							\
    int missing;					\
                                                        \
    missing = (x) - bytes;				\
    if (missing > 0) {					\
      if (header == head_buf) {				\
        if (missing <= end - buf) {			\
          memcpy (header + bytes, buf, missing);	\
          buf += missing;				\
          bytes = (x);				        \
        } else {					\
          memcpy (header + bytes, buf, end - buf);	\
          state_bytes = bytes + end - buf;		\
          return iRet;					\
        }						\
      } else {						\
        memcpy (head_buf, header, bytes);		\
        state = DEMUX_HEADER;				\
        state_bytes = bytes;				\
        return iRet;					\
      }							\
    }							\
  } while (0)

#define DONEBYTES(x)		\
  do {		        	\
    if (header != head_buf)	\
      buf = header + (x);	\
  } while (0)

  if (flags & DEMUX_RESET)
  {
	  state = DEMUX_SKIP;
	  state_bytes = 0;
  }
  if (flags & DEMUX_PAYLOAD_START)
  {
    state = DEMUX_SKIP;
    goto payload_start;
  }
  switch (state)
  {
  case DEMUX_HEADER:
	  if (state_bytes > 0)
	  {
		  header = head_buf;
		  bytes = state_bytes;
		  goto continue_header;
	  }
	  break;

  case DEMUX_DATA:
	  if (demux_pid || (state_bytes > end - buf))
	  {
		  //decode_mpeg2 (buf, end);
		  //just sip this
		  state_bytes -= end - buf;
		  return iRet;
	  }
	  //decode_mpeg2 (buf, buf + state_bytes);
	  //just sip this
	  buf += state_bytes;
	  break;

  case DEMUX_SKIP:
	  if (demux_pid || (state_bytes > end - buf))
	  {
		  state_bytes -= end - buf;
		  return iRet;
	  }
	  buf += state_bytes;
	  break;
  }

  while (1)
  {
    if (demux_pid)
    {
        state = DEMUX_SKIP;
        return iRet;
    }

    payload_start:
    header = buf;
    bytes = end - buf;

    continue_header:
    NEEDBYTES (4);
    if (header[0] || header[1] || (header[2] != 1))
    {
      if (demux_pid)
      {
      state = DEMUX_SKIP;
      return iRet;
      }
      else if (header != head_buf)
      {
      buf++;
      goto payload_start;
      }
      else
      {
        header[0] = header[1];
        header[1] = header[2];
        header[2] = header[3];
        bytes = 3;
        goto continue_header;
      }
    }
    if (demux_pid)
    {
      if ((header[3] >= 0xe0) && (header[3] <= 0xef))
        goto pes;
      fprintf (stderr, "bad stream id %x\n", header[3]);
      exit (1);
    }
    switch (header[3])
    {
      case 0xb9:	/* program end code */
        /* DONEBYTES (4); */
        /* break;         */
        return 1;

      case 0xba:	/* pack header */
        NEEDBYTES (12);
      if ((header[4] & 0xc0) == 0x40)
      {	/* mpeg2 */
          NEEDBYTES (14);
          len = 14 + (header[13] & 7);
          NEEDBYTES (len);
          DONEBYTES (len);
          /* header points to the mpeg2 pack header */
      }
      else if ((header[4] & 0xf0) == 0x20)
      {	/* mpeg1 */
          DONEBYTES (12);
          /* header points to the mpeg1 pack header */
      }
      else
      {
          fprintf (stderr, "weird pack header\n");
          //exit (1);
          return -1;
        }
      break;

      case 0xbd:	// private stream 1
        NEEDBYTES (9);
        //printbytes(header,9);
        //printbytes(buf,9+10);
  
        len = header[4]*256+header[5]+6;
        NEEDBYTES(len);
        if ((buf[7] & 0x80) && (buf[8] >= 5))
        {
          ac3pts = 0;
          ac3pts  = (((uint64_t)buf[9]) & 0x0e) << 29;
          ac3pts |= ( (uint64_t)buf[10])         << 22;
          ac3pts |= (((uint64_t)buf[11]) & 0xfe) << 14;
          ac3pts |= ( (uint64_t)buf[12])         <<  7;
          ac3pts |= (((uint64_t)buf[13]) & 0xfe) >>  1;
        }
        scan_private_stream_1(buf+9+buf[8],len-9-buf[8]);
        DONEBYTES(len);
      break;
      
      case 0xc0:	// audio stream 0
        NEEDBYTES (9);
        //printbytes(header,9);
        //printbytes(buf,9+10);
  
        len = header[4]*256+header[5]+6;
        NEEDBYTES(len);
        if ((buf[7] & 0x80) && (buf[8] >= 5))
        {
          audiopts = 0;
          audiopts  = (((uint64_t)buf[9]) & 0x0e) << 29;
          audiopts |= ( (uint64_t)buf[10])         << 22;
          audiopts |= (((uint64_t)buf[11]) & 0xfe) << 14;
          audiopts |= ( (uint64_t)buf[12])         <<  7;
          audiopts |= (((uint64_t)buf[13]) & 0xfe) >>  1;
          //printf(" len total=%d, ac3pts: %llums\n", len,ac3pts/90);
        }
        #if defined(DECODE_AUDIO) && defined (HAVE_LIBAVCODEC)
        scan_audio_stream_0(buf+9+buf[8],len-9-buf[8]);
        #endif
        if( current_playaudiocbf )
          current_playaudiocbf(buf+9+buf[8],len-9-buf[8]);
        DONEBYTES(len);
      break;
/*
    case AUDIO_STREAM_S:
//    case PRIVATE_STREAM1:
          NEEDBYTES (7);
          if ((header[6] & 0xc0) == 0x80)
          {	// mpeg2
            NEEDBYTES (9);
            len = 9 + header[8];
            NEEDBYTES (len);
            // header points to the mpeg2 pes header
            if (header[7] & 0x80)
            {
              uint32_t pts;

              pts = (((header[9] >> 1) << 30) |
              (header[10] << 22) | ((header[11] >> 1) << 15) |
              (header[12] << 7) | (header[13] >> 1));
              mpeg2_pts (mpeg2dec, pts);
              fprintf(stderr, "%s pes PTS = %u\n",header[3] == AUDIO_STREAM_S ? "audio" : "ac3" ,pts/900);
            }
          }
          if( bPlayAudio )
          {
            //if( ossfd <= 0)
            //  ossfd = oss_open();
            mad_decode(ossfd, buf, end-buf);
          }
          // fall-through
*/
    default:
      if (header[3] == demux_track)
      {
      pes:
        NEEDBYTES (7);
        if ((header[6] & 0xc0) == 0x80)
        {	/* mpeg2 */
          NEEDBYTES (9);
          len = 9 + header[8];
          NEEDBYTES (len);
          /* header points to the mpeg2 pes header */
          if (header[7] & 0x80)
          {
            videopts  = (((uint64_t)header[9]) & 0x0e) << 29;
            videopts |= ( (uint64_t)header[10])         << 22;
            videopts |= (((uint64_t)header[11]) & 0xfe) << 14;
            videopts |= ( (uint64_t)header[12])         <<  7;
            videopts |= (((uint64_t)header[13]) & 0xfe) >>  1;

            /*
            videopts = (((header[9] >> 1) << 30) |
                   (header[10] << 22) |
                   ((header[11] >> 1) << 15) |
                   (header[12] << 7) |
                   (header[13] >> 1));
            */
            //mpeg2_pts (mpeg2dec, pts);
            //fprintf(stderr, "mpeg pes PTS = %u\n",pts/900);
          }
        }
        else
        {	/* mpeg1 */
          int len_skip;
          uint8_t * ptsbuf;

          len = 7;
          while (header[len - 1] == 0xff)
          {
            len++;
            NEEDBYTES (len);
            if (len == 23)
            {
              fprintf (stderr, "too much stuffing\n");
              break;
            }
          }
          if ((header[len - 1] & 0xc0) == 0x40)
          {
            len += 2;
            NEEDBYTES (len);
          }
          len_skip = len;
          len += mpeg1_skip_table[header[len - 1] >> 4];
          NEEDBYTES (len);
          /* header points to the mpeg1 pes header */
          ptsbuf = header + len_skip;
          if (ptsbuf[-1] & 0x20)
          {
            videopts = (((ptsbuf[-1] >> 1) << 30) |
                       (ptsbuf[0] << 22) |
                       ((ptsbuf[1] >> 1) << 15) |
                       (ptsbuf[2] << 7) |
                       (ptsbuf[3] >> 1));
            //mpeg2_pts (mpeg2dec, pts);
          }
        }
        DONEBYTES (len);
        bytes = 6 + (header[4] << 8) + header[5] - len;
        if (demux_pid || (bytes > end - buf))
        {
          //decode_mpeg2 (buf, end);
			  // just skip the datat
          state = DEMUX_DATA;
          state_bytes = bytes - (end - buf);
          return 0;
        }
        else if (bytes > 0)
        {
          //decode_mpeg2 (buf, buf + bytes);
			  // just skip the data
          buf += bytes;
        }
      }
      else if (header[3] < 0xb9)
      {
        //fprintf (stderr,
        //         "looks like a video stream, not system stream\n");
        esyslog("ERROR: looks like a video stream, not system stream");
        dump2log (buf, end );
        syslog(LOG_ERR,"header byte follows");
        dump2log (header, header+40 );
        //exit (1);
        return -1;
      }
      else
      {
        //fprintf(stderr, "header[3] is %02X\n",header[3]);
        NEEDBYTES (6);
        DONEBYTES (6);
        bytes = (header[4] << 8) + header[5];
        //skip:
        if (bytes > end - buf)
        {
          state = DEMUX_SKIP;
          state_bytes = bytes - (end - buf);
          return iRet;
        }
        buf += bytes;
      }
    }
  }
  return iRet;
}

int MPEGDecoder::demuxTS_audio(uint8_t * buffer, uint8_t * end, int flags )
{
	uint8_t * buf;
	uint8_t * nextbuf;
	uint8_t * data;
	int pid;
	buf = buffer;
	int demuxflag = flags;
	int pCount = 0;
//	lastYUVBuf = NULL;

   if( dodumpts )
   {
      dump2log(buffer,end);
   }
	for (; (nextbuf = buf + 188) <= end; buf = nextbuf) 
	{
		pCount++;
		if (*buf != 0x47) 
		{
			fprintf (stderr, "bad sync byte\n");
			nextbuf = buf + 1;
			continue;
		}
		pid = ((buf[1] << 8) + buf[2]) & 0x1fff;
		if (pid != demux_pid)
			continue;
		data = buf + 4;
		if (buf[3] & 0x20) 
		{	// buf contains an adaptation field 
			data = buf + 5 + buf[4];
			if (data > nextbuf)
				continue;
		}
		if (buf[3] & 0x10)
		{
			if( (demuxflag & DEMUX_RESET) == DEMUX_RESET )
			{
				if( buf[1] & 0x40 )
				{
					demuxPES_audio (data, nextbuf,DEMUX_PAYLOAD_START|demuxflag);
					bTSFoundPayload = true;
					demuxflag = 0;
				}
			}
			else
			{
				int iFlag = demuxflag;
				if( buf[1] & 0x40 )
					iFlag |= DEMUX_PAYLOAD_START;
				demuxPES_audio (data, nextbuf,iFlag);
				demuxflag = 0;
			}
		}
	}
	return true;
}

bool MPEGDecoder::demuxFrame_audio(int index, int flags )
{
  bool ret = false;
  uint16_t FileNumber;         // current file-number
  off_t FileOffset;           // current file-offset
  int Length;               // frame-lenght of current frame
  
  cIF->Get(index,&FileNumber, &FileOffset, NULL, &Length);
  LastDemuxIndex = index;
  if( index < 0 )
  {
    show_stackframe(false);
    return false;
  }
  cCurrentCIF = cIF;
  ret = demuxFrame_audio(FileNumber, FileOffset, Length,flags);
  //if( !cfn->isPES() )
  //{
		//if( lastYUVBuf == NULL )
		//{
		//	dsyslog("!!!!!!!!!!!!!!!!!!!no YUVBuf after demuxTS for frame %d",index );
		//}
  //}
  return ret;
}

bool MPEGDecoder::demuxFrame_audio(uint16_t FileNumber, off_t FileOffset, int Length, int flags )
{
  uint8_t * end;            // pointer to frame-end
  
  cfn->SetOffset( FileNumber, FileOffset);
  end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);

  if( cfn->isPES() )
  {
	  bTSFoundPayload = true;
	  int demuxret = demuxPES_audio (readBuffer, end, flags);
	  if( demuxret < 0 )
	  {
		 syslog(LOG_ERR,"last read was from index %d, file %d, offset %ld, length %d",LastDemuxIndex,FileNumber,FileOffset,Length );
		 cfn->SetOffset( FileNumber, FileOffset);
		 end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
		 demuxret = demuxPES_audio (readBuffer, end, DEMUX_RESET);
		 if( demuxret < 0 )
		 {
			 syslog(LOG_ERR,"last read was from index %d",LastDemuxIndex);
			exit(1);
		 }
	  }
  }
  else
  {
      if( dodumpts )
      {
         syslog(LOG_ERR, "demuxTS");
      }
		demuxTS_audio(readBuffer, end, flags );
  }
  return true;
}

class bitgetter
{
	uint8_t *bufstart, *bufend;
	uint32_t cvalue;
	int leftbits;
	int startpos;
public:
	void setBuffer(uint8_t *src, int len)
	{
		bufstart = src;
		bufend = src+len;
		cvalue = 0;
		cvalue = (*bufstart++)<<24;
		cvalue |= (*bufstart++)<<16;
		cvalue |= (*bufstart++)<<8;
		cvalue |= (*bufstart++);
		leftbits = 32;
		startpos = 0;
	}
	void needbits(int n)
	{
		while(n > 0 )
		{
			cvalue = (cvalue<<8)| (*bufstart++);
			leftbits += 8;
			startpos -= 8;
			n-=8;
		}
	}
	unsigned int getbits(int n) 
	{
		if( leftbits < n )
			needbits(32-leftbits);
		int iRet = (cvalue >> (leftbits - n)) & ~(~0 << n); 
		leftbits -= n;
		startpos += n;
		return iRet;
	} 
	bool byteAligned()
	{
		return  startpos == 0 || startpos == 8 || startpos == 16 || startpos == 24 || startpos == 32;
	}
	bool nextStartCode()
	{
		while( !byteAligned() )
			getbits(1);
		while( getbits(24) != 1 && haveBits(24) );
		return true;
	}
	int haveBytes()
	{
		return bufend-bufstart;
	}
	bool haveBits(int i)
	{
		return (startpos < 32-i) | (haveBytes() > 2);
	}
};


void MPEGDecoder::scan_private_stream_1(unsigned char *buf, int len)
{
	int m;
	bitgetter b;
	b.setBuffer(buf,len);
	//printf("scan_private_stream_1, len=%d ",len);
	//printbytes(buf,10);
	unsigned char c = b.getbits(8);//buf[0];
	//if( 
	//	(c >= 0x20 && c <= 0x3f)    // Subpictures
	//	//|| (c >= 0x80 && c <= 0x87) // Dolby Digital Audio
	//	|| (c >= 0x88 && c <= 0x8f) // DTS Audio
	//	|| (c >= 0xa0 && c <= 0xa7) // Linear PCM Audio
	//	)
	//	return;
  if( c != 0x80 ) 
    return;
	ac3Decoded = true;
	// not handled yet, seems to be AC3 stream
	if(len >= 6)
	{
		int l = len;
		while (l > 5)
		{
			m = buf[0]*256+buf[1];
			if (m == 0x0b77)
			{
				streamHasAC3 = true;
				//printbytes(buf,30);

				// Get the audio coding mode (ie how many channels)
				int acmod = (((uint8_t)buf[6])>>5)&7;
				//printf(" acmod %d %0X ", acmod,buf[6]);
				int infchans = nfchans[acmod];
				int shift=4;
				// If it is in use, skip the centre channel mix level
				if ((acmod & 0x1) && (acmod != 0x1))
					shift -=2;
				// If it is in use, skip the surround channel mix level
				if (acmod & 0x4)
					shift -=2;

				// skip the dolby surround mode if in 2/0 mode
				if(acmod == 0x2)
					shift -=2;
				//		bsi->dsurmod= bitstream_get(2);
				// Is the low frequency effects channel on?
				int lfeon = (((uint8_t)buf[6])>>shift)&1;
				ac3mode = infchans*10+lfeon;
				if( current_audiocbf )
					current_audiocbf(ac3mode);
				//printf("chans %d.%d\n", infchans,lfeon);
				return;
			}
			else
			{
				l--;
				buf++;
			}
		}
	}
}

//...
