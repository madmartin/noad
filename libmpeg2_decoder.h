#ifndef __LIBMPEG2_DECODER__
#define __LIBMPEG2_DECODER__

#if defined(USE_LIBMPGE2)

#include "mpeg_decoder.h"
extern "C"
{
  #include "mpeg2.h"
}
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

extern bool ignorevideo;
class LibMPeg2Decoder : public MPEGDecoder
{
	mpeg2dec_t *mpeg2dec;
	const mpeg2_info_t * info;
	int demux_pid;
	int demux_track;
	uint64_t ac3pts;
	uint64_t videopts;
	uint64_t audiopts;
	bool streamHasAC3;
	int ac3mode;
	bool libmpeg2error;
	int getTSPacket(uchar *buffer, int size, int &PacketNum,int &flags);
	int state;
	int state_bytes;
	int __bufBytes;
	int __tsBytes;
	uint8_t *__bufPos;
	uint8_t *__bufEnd;
	int libmpeg2_index;
	int libmpeg2_Lastindex;
	uint16_t libmpeg2FileNumber;  // current file-number
	off_t libmpeg2FileOffset;     // current file-offset
	off_t libmpeg2LastFileOffset; // last file-offset
	int libmpeg2Length;           // frame-lenght of current frame
	bool cont_reading;

public:
	LibMPeg2Decoder();
	~LibMPeg2Decoder();

	virtual int decoder_init();
	virtual int openFile(cFileName *cfn, cNoadIndexFile *cIF);
	virtual int decoder_exit();
	virtual bool getNextPicture(int startIndex, int flags );
	virtual bool getPictures(int &startIndex, int flags, stopFunction stop );
	virtual bool getGOP(int &startIndex );
	bool GetVideoFrame(bool restart);
	bool GetPESVideoFrame(bool restart);
	bool GetTSVideoFrame(bool restart);
	virtual void resetDecoder();
	virtual void resetDecoder(int iFrame);

	int decode_mpeg2 (uint8_t * current, uint8_t * end, uint32_t &bytesConsumed);
	int demuxPES (uint8_t * &buf, uint8_t * end, int flags);
	int demuxTS(uint8_t * buffer, uint8_t * end, int flags );
	void scan_private_stream_1(unsigned char *buf, int len);
	int getVStreamID(int f);
	int getTSPID(int f);
	int TS2PES(uint8_t * buffer, uint8_t * end, int flags);
};
#endif // USE_LIBMPGE2

#endif //#define __LIBMPEG2_DECODER__

