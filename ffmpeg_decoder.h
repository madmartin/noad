#ifndef __FFMPEG_DECODER__
#define __FFMPEG_DECODER__


#include "mpeg_decoder.h"
#ifdef USE_FFMPEG
extern "C"
{
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
}
#define DEMUX_RESET 2
#define FFMPEG_SUPPRESS_NOPICTURE_ERROR
class FFMPegDecoder : public MPEGDecoder
{
	AVFormatContext *pFormatCtx;
#ifdef USE_AVIOCONTEXT
   AVIOContext     *pIOContext;
   unsigned char *iobuffer;
#endif

	int             i, videoStream;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame;

public:
	int __bufBytes;
	uint8_t *__bufPos;
	int ffmpeg_index;
	int ffmpeg_Lastindex;
	uint16_t ffmpegFileNumber;  // current file-number
	off_t ffmpegFileOffset;     // current file-offset
	off_t ffmpegLastFileOffset; // last file-offset
	int ffmpegLength;           // frame-lenght of current frame
	bool cont_reading;

public:
	FFMPegDecoder();
	~FFMPegDecoder();

	virtual int decoder_init();
	virtual int openFile(cFileName *cfn, cNoadIndexFile *cIF);
	virtual int decoder_exit();
	virtual bool getNextPicture(int startIndex, int flags );
	virtual bool getPictures(int &startIndex, int flags, stopFunction stop );
	virtual bool getGOP(int &startIndex );
	bool GetVideoFrame(bool restart);
	virtual void resetDecoder();
	virtual void resetDecoder(int iFrame);
};
#endif //USE_FFMPEG
#endif //#define __FFMPEG_DECODER__
