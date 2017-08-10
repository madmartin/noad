#ifndef __MPEG_DECODER__
#define __MPEG_DECODER__

#include "vdr_cl.h"
#include "yuvbuf.h"

#define YUVBUF_IS_VOID
#ifdef YUVBUF_IS_VOID
typedef void * YUVBUF;
#else
typedef const mpeg2_fbuf_t *YUVBUF;
#endif
typedef int (* noadCallback)(noadYUVBuf *buf);
typedef int (* audiocbfunc)(int mode);
typedef int (* playaudiocbfunc)(unsigned char *mbuf, int count);
typedef void (*positioncbfunc )( int iCurrentFrame );

typedef uint8_t *gopyuvbuf[3];

#define DEMUX_HEADER 0
#define DEMUX_DATA 1
#define DEMUX_SKIP 2

#define DEMUX_PAYLOAD_START 1
#define DEMUX_RESET 2

#define PICS_IN_GOP 40

#define FRAMES_FOR_AC51DETECT (framespersec*2000)
#define AC3_51DETECT_SKIP 250
#define AC3ACTIVECUTOFF 60

extern uchar readBuffer[MAXFRAMESIZE]; // frame-buffer
extern int secsForAC3Detection;

class gop_buffer
{
public:
	uint32_t startindex;
	int width;
	int height;
	int numpics;
	noadYUVBuf gop_buf[PICS_IN_GOP];
	gop_buffer();
	void init(int width, int height);
	void init(int width, int height, int linewidth1, int linewidth2);
   void releaseBuffers();
	void addPic(uint8_t * const[3]);
	void addPic(uint8_t * const[3], int linesize1, int linesize2);
};

typedef bool(*stopFunction)(void);
class MPEGDecoder
{
	int state;
	int state_bytes;
	bool bTSFoundPayload;
protected:
	cFileName *cfn;
	cNoadIndexFile *cIF;
	bool initialised;
   noadCallback callBackFunc;
   positioncbfunc pcbf;
	gop_buffer GOP_Buffer;
public:
	bool ac3Decoded;
	MPEGDecoder();
	~MPEGDecoder();

	// pure virtual functions for picture-decoding
	virtual int decoder_init() = 0;
	virtual int openFile(cFileName *cfn, cNoadIndexFile *cIF)=0;
	virtual int decoder_exit() = 0;
	virtual bool getNextPicture(int startIndex, int flags ) = 0;
	virtual bool getPictures(int &startIndex, int flags, stopFunction stop ) = 0;
	virtual bool getGOP(int &startIndex ) = 0;
	virtual void resetDecoder() = 0;
	virtual void resetDecoder(int iFrame)=0;
	//...

   void setCallback(noadCallback cb) { callBackFunc = cb; }
   noadCallback getCallback() { return callBackFunc; }

	void setPositionCB_Func( positioncbfunc f )  { pcbf = f; }
	positioncbfunc getPositionCB_Func(void) { return pcbf; }

	// functions for audio-scanning
	bool detect_ac3_51(int StartFrame);
	bool ac3Pass1(cMarks *pmarks);
	int findVideoByPTS(uint64_t apts, int iStartFrame);
	int find_ac3start( int iStart, int iEnd, int mode2Find);
	bool demuxFrame_audio(int index, int flags );
	//...


private:
	// demuxer, for audio only
	void demux_reset(void);
	int demuxPES_audio (uint8_t * buf, uint8_t * end, int flags);
	int demuxTS_audio(uint8_t * buffer, uint8_t * end, int flags );
	bool demuxFrame_audio(uint16_t FileNumber, off_t FileOffset, int Length, int flags );
	void scan_private_stream_1(unsigned char *buf, int len);
};
#endif // __MPEG_DECODER__
