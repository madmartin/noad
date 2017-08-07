/***************************************************************************
                          mpeg2wrap.h  -  description
                             -------------------
    begin                : Son Nov 16 23:25:00 CET 2003
    copyright            : (C) 2003 by theNoad #709GRW
    email                : theNoad@SoftHome.net
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
*/
#ifndef __MPEG2WRAP_H__
#define __MPEG2WRAP_H__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <inttypes.h>

extern "C"
{
  #include "mpeg2.h"
}
#include "vdr_cl.h"

#define DEMUX_PAYLOAD_START 1

// from mpeg2.h, Version 0.3.2pre:
// workaround for different state-definitions
#if !defined(MPEG2_VERSION)
#undef STATE_SEQUENCE
#undef STATE_SEQUENCE_REPEATED
#undef STATE_GOP
#undef STATE_PICTURE
#undef STATE_SLICE_1ST
#undef STATE_PICTURE_2ND
#undef STATE_SLICE
#undef STATE_END
#undef STATE_INVALID
typedef enum __mpeg2_state_t
{
    STATE_BUFFER = 0,
    STATE_SEQUENCE = 1,
    STATE_SEQUENCE_REPEATED = 2,
    STATE_GOP = 3,
    STATE_PICTURE = 4,
    STATE_SLICE_1ST = 5,
    STATE_PICTURE_2ND = 6,
    STATE_SLICE = 7,
    STATE_END = 8,
    STATE_INVALID = 9
} mpeg2_state_t;
#endif

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

typedef int (* cbfunc)(void *rgb_buf, int width, int height, void *yufbuf);
typedef int (* audiocbfunc)(int mode);
typedef int (* playaudiocbfunc)(unsigned char *mbuf, int count);
//typedef int (* conversionCallback)(mpeg2_info_t * info);
typedef unsigned long long uint_64;
typedef unsigned short uint_16;
typedef unsigned char  uint_8;

extern mpeg2dec_t *mpeg2dec;
extern cbfunc current_cbf;
extern audiocbfunc current_audiocbf;
extern playaudiocbfunc current_playaudiocbf;
//extern conversionCallback conv_cbf;
extern int demux_pid;
extern int demux_track;
extern int iCurrentDecodedFrame;
extern bool doConversion;
extern bool streamHasAC3;
extern int ac3mode;
extern bool ignorevideo;
extern uint_64 ac3pts;
extern uint_64 videopts;
extern bool havesilence;
extern int lowvalcount;
extern uint_64 audiopts;

void decode_mpeg2 (uint8_t * current, uint8_t * end);
int demux (uint8_t * buf, uint8_t * end, int flags);
bool demuxFrame(cFileName *cfn, cNoadIndexFile *cIF, int index );
bool demuxFrame(cFileName *cfn, uchar FileNumber, int FileOffset, int Length );

#endif
