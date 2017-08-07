#ifndef __AUDIOTOOLS_H__
#define __AUDIOTOOLS_H__
#include "config.h"

#ifdef HAVE_LIBAVCODEC

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <byteswap.h>
#include <ctype.h>
#include <unistd.h>

void initAVCodec();
void exitAVCodec();
int scan_audio_stream_0(unsigned char *mbuf, int count);
void resetAudioBuffer();

#endif // HAVE_LIBAVCODEC
#endif // __AUDIOTOOLS_H__

