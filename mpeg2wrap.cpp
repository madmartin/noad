/***************************************************************************
                          mpeg2wrap.cpp  -  description
                             -------------------
    begin                : Son Nov 16 23:25:00 CET 2003
    copyright            : (C) 2003/2004 by theNoad #709GRW
    email                : theNoad@ulmail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/*
   this is the wrapper to the mpeg2-library (libmpeg2)
   the demuxer is based on the original demuxer from mpeg2dec.c
    Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
    Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#ifdef HAVE_IO_H
#include <fcntl.h>
#include <io.h>
#endif
#include <inttypes.h>
#include "mpeg2wrap.h"
#include "tools.h"
#include "audiotools.h"

#ifdef VNOAD
#define DECODE_AUDIO
#define PLAY_AUDIO
#endif

mpeg2dec_t *mpeg2dec = NULL;
int demux_pid = 0;
int demux_track = 0;
int audio_demux_track = 0x80;
int iFrameDisplayed = 0;
bool iframeDisplay = false;
bool streamHasAC3 = false;
int ac3mode = 0;
bool ignorevideo = false;
bool dodumpts=false;
bool isOnlinescan=false;

//bool doConversion = false;
cbfunc current_cbf = NULL;
audiocbfunc current_audiocbf = NULL;
playaudiocbfunc current_playaudiocbf = NULL;
//conversionCallback conv_cbf = NULL;
int iCurrentDecodedFrame = 0;

uint_64 ac3pts = 0;
uint_64 audiopts = 0;
uint_64 videopts = 0;

typedef struct _frmsize
{
    uint_16 bit_rate;
    uint_16 frame_size[3];
} frmsize_t;

static const frmsize_t frmsizecod_tbl[64] = {
    { 32  ,{64   ,69   ,96   } }, { 32  ,{64   ,70   ,96   } },
    { 40  ,{80   ,87   ,120  } }, { 40  ,{80   ,88   ,120  } },
    { 48  ,{96   ,104  ,144  } }, { 48  ,{96   ,105  ,144  } },
    { 56  ,{112  ,121  ,168  } }, { 56  ,{112  ,122  ,168  } },
    { 64  ,{128  ,139  ,192  } }, { 64  ,{128  ,140  ,192  } },
    { 80  ,{160  ,174  ,240  } }, { 80  ,{160  ,175  ,240  } },
    { 96  ,{192  ,208  ,288  } }, { 96  ,{192  ,209  ,288  } },
    { 112 ,{224  ,243  ,336  } }, { 112 ,{224  ,244  ,336  } },
    { 128 ,{256  ,278  ,384  } }, { 128 ,{256  ,279  ,384  } },
    { 160 ,{320  ,348  ,480  } }, { 160 ,{320  ,349  ,480  } },
    { 192 ,{384  ,417  ,576  } }, { 192 ,{384  ,418  ,576  } },
    { 224 ,{448  ,487  ,672  } }, { 224 ,{448  ,488  ,672  } },
    { 256 ,{512  ,557  ,768  } }, { 256 ,{512  ,558  ,768  } },
    { 320 ,{640  ,696  ,960  } }, { 320 ,{640  ,697  ,960  } },
    { 384 ,{768  ,835  ,1152 } }, { 384 ,{768  ,836  ,1152 } },
    { 448 ,{896  ,975  ,1344 } }, { 448 ,{896  ,976  ,1344 } },
    { 512 ,{1024 ,1114 ,1536 } }, { 512 ,{1024 ,1115 ,1536 } },
    { 576 ,{1152 ,1253 ,1728 } }, { 576 ,{1152 ,1254 ,1728 } },
    { 640 ,{1280 ,1393 ,1920 } }, { 640 ,{1280 ,1394 ,1920 } }
};

static const uint_16 nfchans[8] = {2,1,2,3,3,4,4,5};

void printbytes(unsigned char *buf, int len)
{
  for(int i = 0; i < len; i++ )
    printf("%02X ",buf[i]);
  printf("\n");
}

static void save_ppm (int width, int height, uint8_t * buf, int num, int picType, int temporalReference)
//static void save_ppm (int width, int height, uint8_t * const * buf, int num)
{
  char filename[100];
  char cType = 'U';
  FILE * ppmfile;

  if( picType == PIC_FLAG_CODING_TYPE_I )
    cType = 'I';
  else if( picType == PIC_FLAG_CODING_TYPE_P )
    cType = 'P';
  else if( picType == PIC_FLAG_CODING_TYPE_B )
    cType = 'B';
  sprintf (filename, "%08d_%02d_%c.ppm", num, temporalReference, cType);
  ppmfile = fopen (filename, "wb");
  if (!ppmfile)
    return;
  fprintf (ppmfile, "P6\n%d %d\n255\n", width, height);
  fwrite (buf, 3 * width, height, ppmfile);
  fclose (ppmfile);
}

static void save_pgm (int width, int height, uint8_t * const * buf, int num)
{
    char filename[100];
    FILE * pgmfile;
    int i;

    sprintf (filename, "%d.pgm", num);
    pgmfile = fopen (filename, "wb");
    if (!pgmfile)
	return;
    fprintf (pgmfile, "P5\n%d %d\n255\n", width, height * 3 / 2);
    fwrite (buf[0], width, height, pgmfile);
    width >>= 1;
    height >>= 1;
    for (i = 0; i < height; i++) {
	fwrite (buf[1] + i * width, width, 1, pgmfile);
	fwrite (buf[2] + i * width, width, 1, pgmfile);
    }
    fclose (pgmfile);
}

// from mpeg2dec.cpp
void decode_mpeg2 (uint8_t * current, uint8_t * end)
{
  if(ignorevideo)
    return;
    
  const mpeg2_info_t * info;
//  int state;
    mpeg2_state_t state;

  mpeg2_buffer (mpeg2dec, current, end);

  info = mpeg2_info (mpeg2dec);
  //info->user_data = iCurrentDecodedFrame;
//  while (1)
    while ((state = (mpeg2_state_t)mpeg2_parse (mpeg2dec)) != STATE_BUFFER) {
//  {
//    state = mpeg2_parse (mpeg2dec);
/*
    if( state >= 0)
      dsyslog(LOG_INFO, "mpeg2state is %d(%s) pictype is %s",state, (state >= 0 && state <= 9) ? states[state] : states[0],(mpeg2dec->decoder.coding_type >= 0 && mpeg2dec->decoder.coding_type <= 4) ? pictype[mpeg2dec->decoder.coding_type] : pictype[0]);
*/
    switch (state)
    {
      case -1:
        return;
      case STATE_SEQUENCE:
        /*
        if(doConversion)
	        mpeg2_convert (mpeg2dec, convert_rgb24, NULL);
        */
      break;

      case STATE_PICTURE:
        /*
        // in this state, we know the frame (picture) type
        {
        int picType = ( info->current_picture->flags & PIC_MASK_CODING_TYPE );
        if( picType != PIC_FLAG_CODING_TYPE_I )
           mpeg2_skip( mpeg2dec, / *skip* / true );
        }
        */
	    break;

      case STATE_SLICE:
      case STATE_END:
        if (info->display_fbuf)
        {
          //save_ppm (info->sequence->width, info->sequence->height, info->display_fbuf->buf[0], iCurrentDecodedFrame);
          iFrameDisplayed++;
          int picType = info->current_picture ? ( info->current_picture->flags & PIC_MASK_CODING_TYPE ):0;
          if( picType != PIC_FLAG_CODING_TYPE_I )
            iframeDisplay = true;
          else
            iframeDisplay = false;
          if (current_cbf)
          {
            current_cbf ((void *)info->display_fbuf->buf[0],info->sequence->width, info->sequence->height, (void *)info->display_fbuf->buf);
          }
          //save_ppm (info->sequence->width, info->sequence->height,
          //  info->display_fbuf->buf[0], info->user_data, picType,info->display_picture->temporal_reference);
          //save_pgm (info->sequence->width, info->sequence->height, info->display_fbuf->buf, iCurrentDecodedFrame);
        }
      break;

      default:
      break;
    }
  }
}

void scan_private_stream_1(unsigned char *buf, int len)
{
  int m;
  //printf("scan_private_stream_1, len=%d ",len);
  //printbytes(buf,10);
  unsigned char c = buf[0];
  if( 
    (c >= 0x20 && c <= 0x3f)    // Subpictures
    //|| (c >= 0x80 && c <= 0x87) // Dolby Digital Audio
    || (c >= 0x88 && c <= 0x8f) // DTS Audio
    || (c >= 0xa0 && c <= 0xa7) // Linear PCM Audio
    )
    return;
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
        int acmod = (((uint_8)buf[6])>>5)&7;
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
        int lfeon = (((uint_8)buf[6])>>shift)&1;
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

static unsigned int bitrates[3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static uint32_t freq[4] = {441, 480, 320, 0};

/*
mpeg audio header
	syncword       12	bits	bslbf
	ID	        1	bit	bslbf
	layer	        2	bits	bslbf
	protection_bit	1	bit	bslbf
        
	bitrate_index	4	bits	bslbf
	sampling_frequency	2	bits	bslbf
	padding_bit	1	bit	bslbf
	private_bit	1	bit	bslbf
        
	mode	        2	bits	bslbf
	mode_extension	2	bits	bslbf
	copyright	1	bit	bslbf
	original/home	1	bit	bslbf
	emphasis	2	bits	bslbf
*/

static AudioInfo _ai;
static AudioInfo *ai=&_ai;
static int pr = 1;
static int oldscan_audio_stream_0(unsigned char *mbuf, int count)
{
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

  if (pr)
    fprintf(stderr,"Audiostream: Layer: %d", 4-ai->layer);


  ai->bit_rate = bitrates[(3-ai->layer)][(headr[2] >> 4 )]*1000;

  if (pr)
  {
    if (ai->bit_rate == 0)
      fprintf (stderr,"  Bit rate: free");
    else if (ai->bit_rate == 0xf)
      fprintf (stderr,"  BRate: reserved");
    else
      fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);
  }

  fr = (headr[2] & 0x0c ) >> 2;
  ai->frequency = freq[fr]*100;
  
  if (pr)
  {
    if (ai->frequency == 3)
      fprintf (stderr, "  Freq: reserved\n");
    else
      fprintf (stderr,"  Freq: %2.1f kHz\n", 
                ai->frequency/1000.);
  }
  ai->off = c;
  ai->mode = headr[3]>>6;
  fprintf(stderr,"mode: %d\n", ai->mode);
  return c;
}


#define DEMUX_HEADER 0
#define DEMUX_DATA 1
#define DEMUX_SKIP 2
static int state = DEMUX_SKIP;
static int state_bytes = 0;

void demux_reset(void)
{
	state = DEMUX_SKIP;
	state_bytes = 0;
}

// from mpeg2dec.cpp
int demuxPES (uint8_t * buf, uint8_t * end, int flags)
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
        decode_mpeg2 (buf, end);
        state_bytes -= end - buf;
        return iRet;
      }
      decode_mpeg2 (buf, buf + state_bytes);
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
          ac3pts  = (((uint_64)buf[9]) & 0x0e) << 29;
          ac3pts |= ( (uint_64)buf[10])         << 22;
          ac3pts |= (((uint_64)buf[11]) & 0xfe) << 14;
          ac3pts |= ( (uint_64)buf[12])         <<  7;
          ac3pts |= (((uint_64)buf[13]) & 0xfe) >>  1;
          //printf(" len total=%d, ac3pts: %llums\n", len,ac3pts/90);
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
          audiopts  = (((uint_64)buf[9]) & 0x0e) << 29;
          audiopts |= ( (uint_64)buf[10])         << 22;
          audiopts |= (((uint_64)buf[11]) & 0xfe) << 14;
          audiopts |= ( (uint_64)buf[12])         <<  7;
          audiopts |= (((uint_64)buf[13]) & 0xfe) >>  1;
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
            videopts  = (((uint_64)header[9]) & 0x0e) << 29;
            videopts |= ( (uint_64)header[10])         << 22;
            videopts |= (((uint_64)header[11]) & 0xfe) << 14;
            videopts |= ( (uint_64)header[12])         <<  7;
            videopts |= (((uint_64)header[13]) & 0xfe) >>  1;

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
          decode_mpeg2 (buf, end);
          state = DEMUX_DATA;
          state_bytes = bytes - (end - buf);
          return 0;
        }
        else if (bytes > 0)
        {
          decode_mpeg2 (buf, buf + bytes);
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
 
extern uchar readBuffer[MAXFRAMESIZE]; // frame-buffer
int LastDemuxIndex = -1;
bool bTSFoundPayload = false;
cNoadIndexFile *cCurrentCIF = NULL;
extern YUVBUF volatile lastYUVBuf;  // last yuvbuf from StdCallBack
bool demuxFrame(cFileName *cfn, cNoadIndexFile *cIF, int index, int flags  )
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
  ret = demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
  if( !cfn->isPES() )
  {
		if( lastYUVBuf == NULL )
		{
			dsyslog("!!!!!!!!!!!!!!!!!!!no YUVBuf after demuxTS for frame %d",index );
			//exit(-1);
		}
  }
  return ret;
}

static int retries=0;
bool demuxFrame(cFileName *cfn, uint16_t FileNumber, off_t FileOffset, int Length, int flags )
{
  uint8_t * end;            // pointer to frame-end
  
  cfn->SetOffset( FileNumber, FileOffset);
  end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);

  if( cfn->isPES() )
  {
	  bTSFoundPayload = true;
	  int demuxret = demuxPES (readBuffer, end, flags);
	  if( demuxret < 0 )
	  {
		 syslog(LOG_ERR,"last read was from index %d, file %d, offset %ld, length %d",LastDemuxIndex,FileNumber,FileOffset,Length );
		 cfn->SetOffset( FileNumber, FileOffset);
		 end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
		 demuxret = demuxPES (readBuffer, end, DEMUX_RESET);
		 if( demuxret < 0 )
		 {
	#ifdef VNOAD
			 if( retries == 0 )
			 {
				retries++;
				bool bRet = demuxFrame(cfn, cCurrentCIF, LastDemuxIndex+1,0 );
				if( bRet )
				{
					retries--;
					return true;
				}
			 }
	#endif
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
      demuxTS(readBuffer, end, flags );
  }
  return true;
}

int demuxTS(uint8_t * buffer, uint8_t * end, int flags )
{
	uint8_t * buf;
	uint8_t * nextbuf;
	uint8_t * data;
	int pid;
	buf = buffer;
	int demuxflag = flags;
//	lastYUVBuf = NULL;

   if( dodumpts )
   {
      dump2log(buffer,end);
   }
	for (; (nextbuf = buf + 188) <= end; buf = nextbuf) 
	{
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
					demuxPES (data, nextbuf,DEMUX_PAYLOAD_START|demuxflag);
					bTSFoundPayload = true;
					demuxflag = 0;
				}
			}
			else
			{
				demuxPES (data, nextbuf,(buf[1] & 0x40) ? DEMUX_PAYLOAD_START|demuxflag : demuxflag);
				demuxflag = 0;
			}
		}
	}
	return true;
}
