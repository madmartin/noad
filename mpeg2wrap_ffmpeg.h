/***************************************************************************
                          mpeg2wrap_ffmpeg.h  -  description
                             -------------------
    begin                : Son Nov 16 23:25:00 CET 2003
    copyright            : (C) 2003 by theNoad #709GRW
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
 this is the wrapper to the ffmpeg-library (libavcodec)
*/
#ifndef __MPEG2WRAP_FFMPEG_H__
#define __MPEG2WRAP_FFMPEG_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <inttypes.h>
#include "vdr_cl.h"
#ifdef USE_FFMPEG
extern "C"
{
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
}
#endif
#include "yuvbuf.h"

#define DEMUX_PAYLOAD_START 1
#define DEMUX_RESET 2

typedef struct audio_i
{
  int layer;
  uint32_t bit_rate;
  uint32_t frequency;
  uint32_t mode;
  uint32_t mode_extension;
  uint32_t emphasis;
  uint32_t framesize;
  uint32_t off;
} AudioInfo;

#define YUVBUF_IS_VOID
#ifdef YUVBUF_IS_VOID
typedef void * YUVBUF;
#else
typedef const mpeg2_fbuf_t *YUVBUF;
#endif
typedef int (* cbfunc)(void *rgb_buf, int width, int height, YUVBUF buf);
typedef int (* audiocbfunc)(int mode);
typedef int (* playaudiocbfunc)(unsigned char *mbuf, int count);

extern cbfunc current_cbf;
extern audiocbfunc current_audiocbf;
extern playaudiocbfunc current_playaudiocbf;

extern int demux_pid;
extern int demux_track;
extern int iCurrentDecodedFrame;
extern bool doConversion;
extern bool streamHasAC3;
extern int ac3mode;
extern bool ignorevideo;
extern uint64_t ac3pts;
extern uint64_t videopts;
extern bool havesilence;
extern int lowvalcount;
extern uint64_t audiopts;
extern bool bTSFoundPayload;
extern bool dodumpts;
extern bool isOnlinescan;

typedef uint8_t *gopyuvbuf[3];
//#define PICS_IN_GOP 20

void decode_mpeg2 (uint8_t * current, uint8_t * end);
void demux_reset(void);
int demuxPES (uint8_t * buf, uint8_t * end, int flags);
int demuxTS(uint8_t * buffer, uint8_t * end, int flags=0 );
bool demuxFrame(cFileName *cfn, cNoadIndexFile *cIF, int index, int flags );
typedef bool(*stopFunction)(void);
bool getPictures(cFileName *cfn, cNoadIndexFile *cIF, int &startIndex, int flags, stopFunction stop );
bool getGOP(cFileName *cfn, cNoadIndexFile *cIF, int &startIndex );

// ff_mpeg
int init_ffmpeg();
int exit_ffmpeg();
extern volatile bool ffmpegerror;
// ... ff_mpeg
#endif //__MPEG2WRAP_FFMPEG_H__

