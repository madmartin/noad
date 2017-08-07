#include "audiotools.h"

bool havesilence = false;
#ifdef HAVE_LIBAVCODEC

#include "mpeg2wrap.h"
extern "C"
{
   #include "libavcodec/avcodec.h"
}

// from libavutil/intreadwrite.h:
#define AV_RB32(x)  ((((const uint8_t*)(x))[0] << 24) | \
                     (((const uint8_t*)(x))[1] << 16) | \
                     (((const uint8_t*)(x))[2] <<  8) | \
                      ((const uint8_t*)(x))[3]) 
//...
// from libavcodec/mpegaudio.h:
/* fast header check for resync */
static inline int ff_mpa_check_header(uint32_t header){
    /* header */
    if ((header & 0xffe00000) != 0xffe00000)
        return -1;
    /* layer check */
    if ((header & (3<<17)) == 0)
        return -1;
    /* bit rate */
    if ((header & (0xf<<12)) == 0xf<<12)
        return -1;
    /* frequency */
    if ((header & (3<<10)) == 3<<10)
        return -1;
    return 0;
} 
//...
#define MIN_LOWVALS 3
#define CUT_VAL 10
int lowvalcount = 0;
double lastsampleoffset=0.0;
uint8_t *inbuf_ptr;
int out_size, size, len;
AudioInfo _ai;
AudioInfo *ai=&_ai;
int pr = 1;

uint8_t *outbuf=NULL;
AVCodec *codec=NULL;
AVCodecContext *codecContext= NULL;
int64_t basepts=0;
int64_t audiobasepts=0;
//int64_t audiopts=0;
extern uint_64 audiopts;
int64_t audiosamples=0;
uint8_t audio_in_buffer[8192];
int in_buf_count = 0;
static bool av_codec_initialised = false;

static unsigned int bitrates[3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static uint32_t freq[4] = {441, 480, 320, 0};

void my_av_dolog(void *ptr, int level, const char *fmt,va_list vl)
{
  static char line[1024];
  //va_list vl;
  //va_start(vl,fmt);
  vsnprintf( line,sizeof(line),fmt,vl);
  //fprintf(stderr,line);
  esyslog("ERROR decoding audio: %s (ignored)",line);
  //va_end(vl);
}

void my_av_log(void*, int level,const char *fmt,...)
{
  va_list vl;
  va_start(vl,fmt);
  my_av_dolog(NULL,0,fmt,vl);
  va_end(vl);
}

void initAVCodec()
{
  if( av_codec_initialised )
	  return;
  // init libavcodec(ffmpeg)
  /* must be called before using avcodec lib */
  avcodec_init();

  av_log_set_callback(my_av_dolog);
  /* register all the codecs (you can also register only the codec
     you wish to have smaller code */
  avcodec_register_all();
    
  /* find the mpeg audio decoder */
  codec = avcodec_find_decoder(CODEC_ID_MP3);
  
  if (!codec) 
  {
    fprintf(stdout, "codec not found\n");
  }

  codecContext = avcodec_alloc_context();

  /* open it */
  if (avcodec_open(codecContext, codec) < 0)
  {
    fprintf(stderr, "could not open codec\n");
  }

  outbuf = (uint8_t *)malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

  av_codec_initialised = true;
        
}

void exitAVCodec()
{
  free(outbuf);
  avcodec_close(codecContext);
  av_free(codecContext);
  av_codec_initialised = false;
}

int scan_audio_stream_0(unsigned char *mbuf, int count)
{
/*
  StreamReader asr;
  asr.addBytes(sr.curBuffPos(),packetSize);
  int ID = 0;
  int layer = 0;
  int protection_bit = 0;
  int bitrate_index = 0;
  int sampling_frequency = 0;
  int padding_bit = 0;
  int private_bit = 0;
  int mode = 0;
  int mode_extension = 0;
  int copyright = 0;
  int original_home = 0;
  int emphasis = 0;
*/  
  int16_t chan1 = 0;
  int16_t chan2 = 0;
//  int16_t chan1t = 0;
//  int16_t chan2t = 0;
  uint8_t *buf = mbuf;
  uint32_t header;
  if( in_buf_count == 0 )
  {
	 header = AV_RB32(buf); 
	 while( count-- && ff_mpa_check_header(header) < 0 )
	 {
		 buf++;
		 header = AV_RB32(buf); 
	 }
	 if( count <= 0 )
		 return -1;
  }
  memcpy(&audio_in_buffer[in_buf_count],buf,count);
  in_buf_count += count;
  long size = in_buf_count;
  inbuf_ptr = audio_in_buffer;

  while (size > 0) 
  {
    out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    len = avcodec_decode_audio2(codecContext, (short *)outbuf, &out_size,
                                  inbuf_ptr, 576/*size*/);
    if (len < 0) 
    {
      //fprintf(stderr, "Error while decoding audio\n\n");
		esyslog("Error while decoding audio (ignored)");
      resetAudioBuffer();
      size=0;
      break;
    }
//    SDL_PauseAudio(0);
    if (out_size > 0) 
    {
      audiosamples += out_size/4;
      //fprintf(stdout,"decode audio out_size=%d\n",out_size);
      chan1 = 0;
      chan2 = 0;
      //lowvalcount = 0;
      uint8_t *src=outbuf;
      for( int i = 0; i < out_size && !havesilence; i+=4)
      {
        //if(*src == 0 || *src == 0xFF )
        {
          chan1 = bswap_16(*((int16_t *)src));
          chan2 = bswap_16(*((int16_t *)(src+2)));
          //chan1 = *((int16_t *)&outbuf[i]);
          //chan2 = *((int16_t *)&outbuf[i+2]);
          /*
          chan1 = (outbuf[i]<<8)|outbuf[i+1];
          chan2 = (outbuf[i+2]<<8)|outbuf[i+3];
          chan1t = bswap_16(*((int16_t *)&outbuf[i]));
          chan2t = bswap_16(*((int16_t *)&outbuf[i+2]));
          if( chan1 != chan1t )
            fprintf(stdout,"ungleich %d %d     %d %d\n",chan1,chan1t,chan2,chan2t);
          */
          if( (abs(chan1)+abs(chan2)) < CUT_VAL )
          {
            lowvalcount++;
            if( lowvalcount > MIN_LOWVALS )
              havesilence = true;
          }
          else
          {
            if( lowvalcount > MIN_LOWVALS )
              havesilence = true;
            lowvalcount = 0;
          }
        }
        src += 4;
        //fprintf(stdout,"%.2X %.2X  %.2X %.2X\n",outbuf[i],outbuf[i+1],outbuf[i+2],outbuf[i+3]);
      }
      /*
      if( havesilince || (lowvalcount > 3) )
      {
        double videooffset;
        videooffset = audiopts-audiobasepts;
        videooffset /= 90000.0;
        double sampleoffset;
        sampleoffset = audiosamples;
        sampleoffset /=48000.0;
        int min = sampleoffset/60;
        int sec=sampleoffset;
        sec%=60;
        int min2 = (sampleoffset-lastsampleoffset)/60;
        int sec2 = (sampleoffset-lastsampleoffset);
        sec2%=60;
        lastsampleoffset = sampleoffset;
        fprintf(stdout,"found %d low values within %d total values at %5.2f sampleofset %8.2f (%0.2d:%0.2d) diff %0.2d:%0.2d\n",lowvalcount,out_size/4,videooffset,sampleoffset,min,sec,min2,sec2);
      }
      */
          /* if a frame has been decoded, output it */
          //fwrite(outbuf, 1, out_size, outfile);
#ifdef PLAY_AUDIO
      audioRingBuffer->Put(outbuf,out_size);
//        SDL_PauseAudio(0);
        while( audioRingBuffer->Available() > 12000 )
          SDL_Delay(10);
#endif      
		 /*
      if( (audio_len+out_size) > AUDIO_BUF_SIZE )
      {
        // play audio
        audio_offset = 0;
        SDL_PauseAudio(0);
        while( audio_len > 0 )
          SDL_Delay(10);
        SDL_PauseAudio(1);
      }
      // move data to audio_buffer
      memmove(audio_buff+audio_len,outbuf,out_size);
      audio_len += out_size;
      //SDL_Delay(100);
      */
    }
    size -= len;
    inbuf_ptr += len;
  }
  if( size > 0 )
  {
     in_buf_count = size;
     memmove(audio_in_buffer,inbuf_ptr,size);
  }
  else
     in_buf_count = 0;
  if( havesilence || (lowvalcount > MIN_LOWVALS) )
  {
    lowvalcount = 0;
    double videooffset;
    videooffset = audiopts-audiobasepts;
    videooffset /= 90000.0;
    double sampleoffset;
    sampleoffset = audiosamples;
    sampleoffset /=48000.0;
    int min = (int)(sampleoffset/60);
    int sec=(int)sampleoffset;
    sec%=60;
    int min2 = (int)((sampleoffset-lastsampleoffset)/60);
    int sec2 = (int)((sampleoffset-lastsampleoffset));
    sec2%=60;
    lastsampleoffset = sampleoffset;
    //fprintf(stderr,"found %4d low values within %4d total values at %8.2f sampleofset %8.2f (%0.2d:%0.2d) diff %0.2d:%0.2d %c\n",lowvalcount,out_size/4,videooffset,sampleoffset,min,sec,min2,sec2, ((min2*60+sec2)>=60)?'<':' ');
    //fprintf(stderr,"current audio_pts is %lld\n",audiopts);
  }
  
  
  uint8_t *headr;
  int found = 0;
  int c = 0;
  int fr =0;
  
  while (!found && c < count)
  {
    uint8_t *b = mbuf+c;

    if ( b[0] == 0xff && (b[1] & 0xf8) == 0xf8)
            found = 1;
    else {
            c++;
    }
  }	

  if (!found) return -1;

  if (c+3 >= count) return -1;
  headr = mbuf+c;

  ai->layer = (headr[1] & 0x06) >> 1;

  //if (pr)
  //  fprintf(stderr,"Audiostream: Layer: %d", 4-ai->layer);


  ai->bit_rate = bitrates[(3-ai->layer)][(headr[2] >> 4 )]*1000;

  /*
  if (pr)
  {
    if (ai->bit_rate == 0)
      fprintf (stderr,"  Bit rate: free");
    else if (ai->bit_rate == 0xf)
      fprintf (stderr,"  BRate: reserved");
    else
      fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);
  }
  */
  fr = (headr[2] & 0x0c ) >> 2;
  ai->frequency = freq[fr]*100;
  
  /*
  if (pr)
  {
    if (ai->frequency == 3)
      fprintf (stderr, "  Freq: reserved\n");
    else
      fprintf (stderr,"  Freq: %2.1f kHz\n", 
                ai->frequency/1000.);
  }
  */
  ai->off = c;
  ai->mode = headr[3]>>6;
  //fprintf(stderr,"mode: %d\n", ai->mode);
  return c;
}

void resetAudioBuffer()
{
   in_buf_count = 0;
}

#endif // HAVE_LIBAVCODEC
