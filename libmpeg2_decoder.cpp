#include "libmpeg2_decoder.h"
#if defined(USE_LIBMPGE2)

bool ignorevideo = false;
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF

static const uint16_t nfchans[8] = {2,1,2,3,3,4,4,5};

bool havepic = false;
LibMPeg2Decoder::LibMPeg2Decoder() :
	MPEGDecoder()
{
	mpeg2dec = NULL;
	info = NULL;
	state = DEMUX_SKIP;
	state_bytes = 0;
	__bufBytes = 0;
   __bufPos = 0;
   __bufEnd = 0;
	__tsBytes = 0;
	libmpeg2_index=0;
	libmpeg2_Lastindex=-1;
   libmpeg2FileNumber=1;		// current file-number
   libmpeg2FileOffset=0;      // current file-offset
   libmpeg2LastFileOffset=0;  // last file-offset
   libmpeg2Length=0;          // frame-lenght of current frame
	cont_reading = true;
}

LibMPeg2Decoder::~LibMPeg2Decoder()
{
	decoder_exit();
}

int LibMPeg2Decoder::decoder_init()
{
	demux_track = 0xe0;
	mpeg2dec = mpeg2_init();
	return 0;
}

int LibMPeg2Decoder::openFile(cFileName *_cfn, cNoadIndexFile *_cIF)
{
	cfn =_cfn;
	cIF = _cIF;
	demux_track = getVStreamID(cfn->File());
	if( cfn->isPES() )
		demux_pid = 0;
	else
		demux_pid = getTSPID(cfn->File());
	return false;
}

int LibMPeg2Decoder::decoder_exit()
{
	mpeg2_close (mpeg2dec);
	return 0;
}

bool LibMPeg2Decoder::GetVideoFrame(bool restart)
{
	if( cfn->isPES() )
		return GetPESVideoFrame(restart);
	return GetTSVideoFrame(restart);
}

bool LibMPeg2Decoder::GetPESVideoFrame(bool restart)
{
	bool bRet = false;
	bool needMorData = false;
	libmpeg2error = false;
	int flags;
	flags = restart?DEMUX_RESET:0;
	do
	{
		if( __bufBytes <= 0 )
		{
			if( libmpeg2_index >= 0 )
			{
				if( !cont_reading )
					if( libmpeg2_Lastindex == libmpeg2_index )
						return 0;
				bool Independent;
				if( !cIF->Get(libmpeg2_index,&libmpeg2FileNumber, &libmpeg2FileOffset, &Independent, &libmpeg2Length) )
					return 0;
				//if( libmpeg2Length <= 0 )
				//	libmpeg2error = true;
				//if(		logReading )
				//dsyslog("noad_read read from file, libmpeg2_index = %d (iframe: %s), length = %d, offset: %ld",libmpeg2_index, Independent?"true":"false", libmpeg2Length,libmpeg2FileOffset);
				if( cont_reading )
					libmpeg2_index++;
				libmpeg2_Lastindex = libmpeg2_index;
			}
			//else
			//{
			//	if( logReading )
			//		dsyslog("noad_read read from file, ffmpegFileOffset = %d, length = %d",ffmpegFileOffset, ffmpegLength);
			//	if( ffmpegFileOffset == ffmpegLastFileOffset )
			//		return 0;
			//}
			cfn->SetOffset( libmpeg2FileNumber, libmpeg2FileOffset);
			unsigned char *readDest = readBuffer;
         int iMax = MAXFRAMESIZE;
			if( needMorData )
         {
				readDest = __bufEnd;
            iMax = __bufEnd - readBuffer; 
         }
			int bytesRed = ReadFrame (cfn->File(), readDest, libmpeg2Length, iMax);
         libmpeg2Length = bytesRed;
			if( restart )
				flags = DEMUX_RESET;
			__bufBytes += bytesRed;
			__bufPos = readBuffer; 
			__bufEnd = readBuffer + __bufBytes;
			libmpeg2LastFileOffset = libmpeg2FileOffset;
		}
		if( __bufBytes > 0 )
		{
			havepic = false;
			int iDemux = demuxPES (__bufPos, __bufEnd, flags);
         if( iDemux < 0 )
         {
            libmpeg2error = true;
            return false;
         }
			bRet = havepic;
			__bufBytes = __bufEnd-__bufPos;
			restart = false;
			flags = 0;
		}
	} while (!libmpeg2error && !bRet);
	return bRet;
}

bool LibMPeg2Decoder::GetTSVideoFrame(bool restart)
{
	bool bRet = false;
	libmpeg2error = false;
	int flags;
	flags = restart?DEMUX_RESET:0;
//	static int tsflags = 0;
//	static int tsPacketCount = 0;
	static int currentTsPacket = 0;
	do
	{
		if( __bufBytes <= 0 )
		{
			if( libmpeg2_index >= 0 )
			{
				if( !cont_reading )
					if( libmpeg2_Lastindex == libmpeg2_index )
						return 0;
				bool Independent;
				if( !cIF->Get(libmpeg2_index,&libmpeg2FileNumber, &libmpeg2FileOffset, &Independent, &libmpeg2Length) )
					return 0;
				//if( libmpeg2Length <= 0 )
				//	libmpeg2error = true;
				//if(		logReading )
				//dsyslog("noad_read read from file, libmpeg2_index = %d (iframe: %s), length = %d, offset: %ld",libmpeg2_index, Independent?"true":"false", libmpeg2Length,libmpeg2FileOffset);
				if( cont_reading )
					libmpeg2_index++;
				libmpeg2_Lastindex = libmpeg2_index;
			}
			//else
			//{
			//	if( logReading )
			//		dsyslog("noad_read read from file, ffmpegFileOffset = %d, length = %d",ffmpegFileOffset, ffmpegLength);
			//	if( ffmpegFileOffset == ffmpegLastFileOffset )
			//		return 0;
			//}
			cfn->SetOffset( libmpeg2FileNumber, libmpeg2FileOffset);
			unsigned char *readDest = readBuffer;
			int bytesRed = ReadFrame (cfn->File(), readDest, libmpeg2Length, MAXFRAMESIZE);
         libmpeg2Length = bytesRed;
			if( restart )
				flags = DEMUX_RESET;
				__bufBytes += bytesRed;
//				tsflags = 0;
			//}
			__bufPos = readBuffer; 
			__bufEnd = readBuffer + __bufBytes;
			libmpeg2LastFileOffset = libmpeg2FileOffset;

//			tsPacketCount = libmpeg2Length/TS_SIZE;
			currentTsPacket = 0;
			__tsBytes = 0;
		}
		if( __bufBytes > 0 )
		{
			havepic = false;
			if( __tsBytes <= 0 )
			{
				__tsBytes = getTSPacket(readBuffer,libmpeg2Length,currentTsPacket,flags);
				__bufBytes = libmpeg2Length - TS_SIZE * currentTsPacket;  
				__bufPos = readBuffer;
				__bufEnd = __bufPos + __tsBytes;
			}

			if( __tsBytes )
			{
//				uint8_t *pos =	 readBuffer;
				bRet = demuxPES(__bufPos, __bufEnd, flags);
				bRet = havepic;
				flags = 0;
				restart = false;
			}
			__tsBytes = __bufEnd-__bufPos;
			flags = 0;
		}
	} while (!libmpeg2error && !bRet);
	return bRet;
}


bool LibMPeg2Decoder::getNextPicture(int startIndex, int flags )
{
	bool bRet = false;
	bool restart = false;
   libmpeg2error = false;
	if (flags & DEMUX_RESET)
	{
		libmpeg2_index = startIndex;
		resetDecoder(startIndex);
		restart = true;
	}
	while( !bRet && libmpeg2_index < cIF->getLast() && !libmpeg2error )
	{
		bRet = GetVideoFrame(restart);
		if( bRet && callBackFunc )
      {
         noadYUVBuf lbuf(info->display_fbuf->buf,info->sequence->width, info->sequence->height);
         callBackFunc(&lbuf);
      }
	}
	startIndex = libmpeg2_index; 
	return bRet;
}

bool LibMPeg2Decoder::getPictures(int &startIndex, int flags, stopFunction stop )
{
	bool bRet = false;
	//dsyslog("getPicture from index %d", startIndex );
	bool restart = false;
	bool bStop = false;
	libmpeg2error = false;
	if (flags & DEMUX_RESET)
	{
		resetDecoder(startIndex);
		restart = true;
	}
	else
		if( startIndex < libmpeg2_index )
			return false;
	libmpeg2_index = startIndex;
	while( libmpeg2_index < cIF->getLast() && !libmpeg2error && !bStop )
	{
		bRet = GetVideoFrame(restart);
		restart = false;
      //iCurrentDecodedFrame = libmpeg2_index;
		if( bRet && callBackFunc )
      {
         noadYUVBuf lbuf(info->display_fbuf->buf,info->sequence->width, info->sequence->height);
         callBackFunc(&lbuf);
			bStop = stop();
		}
	}
	startIndex = libmpeg2_index; 
	return bRet;
}

bool LibMPeg2Decoder::getGOP(int &startIndex )
{
	bool bRet = false;
	uint16_t FileNumber;
	off_t FileOffset;
	bool Independent;
	int gopStartFrame = startIndex; 
	int preGopStartFrame;
	int ignoreFrames = 0;
	cIF->Get(startIndex, &FileNumber, &FileOffset, &Independent);
	if( !Independent )
		gopStartFrame = cIF->GetNextIFrame( startIndex, false, NULL, NULL, NULL, true);
	preGopStartFrame = cIF->GetNextIFrame( gopStartFrame, false, NULL, NULL, NULL, true);
	preGopStartFrame = cIF->GetNextIFrame( preGopStartFrame, false, NULL, NULL, NULL, true);
	if( preGopStartFrame < 0 )
		preGopStartFrame = 0;
	ignoreFrames = gopStartFrame-preGopStartFrame;
	//dsyslog("getGOP from index %d (%d)", startIndex, gopStartFrame );
	bool restart = true;
	libmpeg2error = false;


	libmpeg2_index = preGopStartFrame;
	bool bFirst = true;
	if( GOP_Buffer.startindex != gopStartFrame )
	{
		resetDecoder(preGopStartFrame);
		GOP_Buffer.numpics = 0;
		while( libmpeg2_index < cIF->getLast() && !libmpeg2error && GOP_Buffer.numpics < PICS_IN_GOP )
		{
			bRet = GetVideoFrame(restart);
			restart = false;
			if( bRet )
				ignoreFrames--;
			if( bRet && ignoreFrames <= 0)
			{
				if( bFirst )
					GOP_Buffer.init(info->sequence->width, info->sequence->height);
				bFirst = false;
				GOP_Buffer.addPic(info->display_fbuf->buf);
			}
			if( info->sequence->width > 1200 )
				esyslog("ERROR: faulty width");
			if( info->sequence->height > 1200 )
				esyslog("ERROR: faulty height");
		}
		GOP_Buffer.startindex = gopStartFrame;
	}
	if( GOP_Buffer.numpics && callBackFunc )
	{
		int offset = startIndex - gopStartFrame;
      callBackFunc(&GOP_Buffer.gop_buf[offset]);
	}
	startIndex = libmpeg2_index; 
	return bRet;
}

void LibMPeg2Decoder::resetDecoder()
{
	mpeg2_reset (mpeg2dec, 0);
	state = DEMUX_SKIP;
	state_bytes = 0;
	__bufBytes = 0;
}

void LibMPeg2Decoder::resetDecoder(int iFrame)
{
	libmpeg2_index = iFrame;
	mpeg2_reset (mpeg2dec, 0);
	state = DEMUX_SKIP;
	state_bytes = 0;
	__bufBytes = 0;
}


int LibMPeg2Decoder::decode_mpeg2 (uint8_t * current, uint8_t * end, uint32_t &bytesConsumed)
{
	bytesConsumed = end-current;
	if(ignorevideo)
		return 0;

	//const mpeg2_info_t * info;
	mpeg2_state_t state;

	// dsyslog(LOG_INFO,"decode_mpeg2");
	//dump2log(current,end);
	mpeg2_buffer (mpeg2dec, current, end);
//	int iRet = 0;
	info = mpeg2_info (mpeg2dec);
	while ((state = (mpeg2_state_t)mpeg2_parse (mpeg2dec)) != STATE_BUFFER) 
	{
		switch (state)
		{
		case -1:
			return 0;
		case STATE_SEQUENCE:
			break;

		case STATE_PICTURE:
			break;

		case STATE_SLICE:
		case STATE_END:
			if (info->display_fbuf)
			{
				int pos = mpeg2_getpos(mpeg2dec);
				bytesConsumed = end-current-pos;
				havepic = true;
				//dsyslog("found picture");
				return 1;
			}
			break;

		default:
			break;
		}
	}
	bytesConsumed = end-current-mpeg2_getpos(mpeg2dec);
	return 0;
}

// from mpeg2dec.cpp
int LibMPeg2Decoder::demuxPES (uint8_t * &buf, uint8_t * end, int flags)
{
	int iRet = 0;
	uint32_t bytesConsumed;
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


   static uint8_t head_buf[2048];

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
			iRet = decode_mpeg2 (buf, end,bytesConsumed);
			state_bytes -= bytesConsumed;//end - buf;
			buf += bytesConsumed;//state_bytes;
			//if( iRet == 1 )
				return iRet;
		}
		iRet = decode_mpeg2 (buf, buf + state_bytes,bytesConsumed);
		state_bytes -= bytesConsumed;
		buf += bytesConsumed;//state_bytes;
		if( iRet == 1 )
			return iRet;
		break;

	case DEMUX_SKIP:
		if (demux_pid || (state_bytes > end - buf))
		{
			state_bytes -= (end - buf);

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
				buf=end;
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
			state = DEMUX_SKIP;
			buf += 4;
			return 0;

			//fprintf (stderr, "bad stream id %x\n", header[3]);
			//exit (1);
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
				return -1;
			}
			break;

		case 0xbd:	// private stream 1
			NEEDBYTES (9);

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
			//if( current_playaudiocbf )
			//	current_playaudiocbf(buf+9+buf[8],len-9-buf[8]);
			DONEBYTES(len);
			break;
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
				 int iRet = decode_mpeg2 (buf, end,bytesConsumed);
				 buf += bytesConsumed;//bytes;
				 //if( (bytes - bytesConsumed) > 0 )
				 {
					 state_bytes = bytes - bytesConsumed;
					 state = DEMUX_DATA;
				 }
				 if( iRet == 1 )
					 return 1;
				 return 0;
			 }
			 else if (bytes > 0)
			 {
				 int iRet = decode_mpeg2 (buf, buf + bytes,bytesConsumed);
				 buf += bytesConsumed;//bytes;
				 if( (bytes - bytesConsumed) > 0 )
				 {
					 state_bytes = bytes - bytesConsumed;
					 state = DEMUX_DATA;
				 }
				 if( iRet == 1 )
					 return 1;
			 }
			}
			else if (header[3] < 0xb9)
			{
				esyslog("ERROR: looks like a video stream, not system stream");
				// dump2log (buf, end );
				syslog(LOG_ERR,"header byte follows");
				dump2log (header, header+40 );
				return -1;
			}
			else
			{
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
 
int LibMPeg2Decoder::demuxTS(uint8_t * buffer, uint8_t * end, int flags )
{
	uint8_t * buf;
	uint8_t * nextbuf;
	uint8_t * data;
	int pid;
	buf = buffer;
	int demuxflag = flags;
	//	lastYUVBuf = NULL;

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
					//bTSFoundPayload = true;
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

void LibMPeg2Decoder::scan_private_stream_1(unsigned char *buf, int len)
{
	int m;
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

				// Get the audio coding mode (ie how many channels)
				int acmod = (((uint8_t)buf[6])>>5)&7;
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
				//if( current_audiocbf )
				//	current_audiocbf(ac3mode);
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


int LibMPeg2Decoder::getVStreamID(int f)
{
	unsigned char buf[32768];
	int r = read( f,buf,32768);
	int i = 0;
	int found = 0;
	int iRet = VIDEO_STREAM_S;
	while( i < r )
	{
		switch(found)
		{
		  case 0: if( buf[i] == 0 ) found++; break;
		  case 1: if( buf[i] == 0 ) found++; else found=0; break;
		  case 2: if( buf[i] == 1 ) found++; else found=0; break;
		case 3:
			if( buf[i] >= VIDEO_STREAM_S && buf[i] <= VIDEO_STREAM_E )
			{
				iRet = buf[i];
				return iRet;
			}
			else
				found = 0;
			break;

		default:
			found = 0;
			break;
		}
		i++;
	}
	return iRet;
}

int LibMPeg2Decoder::getTSPID(int f)
{
	unsigned char buf[9400];// 50 TS-Pakete
	lseek(f,0,SEEK_SET);
	int r = read( f,buf,9400);
	int i = 0;
	int found = 0;
	int iRet = 0;
	while( i < r )
	{
		if (buf[i] != 0x47) 
		{
			fprintf (stderr, "bad sync byte\n");
			i++;
			continue;
		}
		iRet = ((buf[i+1] << 8) + buf[i+2]) & 0x1fff;
		i+=188;
	}
	return iRet;
}


int LibMPeg2Decoder::TS2PES(uint8_t * buffer, uint8_t * end, int flags)
{
	uint8_t * buf;
	uint8_t * nextbuf;
	uint8_t * data;
	int pid;
	buf = buffer;
	int demuxflag = flags;
	uint8_t * destbuf = buf;
	int retSize = 0;

	for (; (nextbuf = buf + 188) <= end; buf = nextbuf) 
	{
		if (*buf != 0x47) 
		{
			//fprintf (stderr, "bad sync byte\n");
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
					//demuxPES (data, nextbuf,DEMUX_PAYLOAD_START|demuxflag);
					//bTSFoundPayload = true;
					int size = nextbuf-data;
					memmove(destbuf,data,size);
					destbuf += size;
					retSize += size;
					demuxflag = 0;
				}
			}
			else
			{
				//demuxPES (data, nextbuf,(buf[1] & 0x40) ? DEMUX_PAYLOAD_START|demuxflag : demuxflag);
				int size = nextbuf-data;
				memmove(destbuf,data,size);
				destbuf += size;
				retSize += size;
				demuxflag = 0;
			}
		}
	}
	return retSize;
}

int LibMPeg2Decoder::getTSPacket(uchar *buffer, int size, int &PacketNum,int &flags)
{
	uint8_t * buf;
	uint8_t * nextbuf;
	uint8_t * data;
	uint8_t * end;
	int pid;
	buf = buffer;
	uint8_t * destbuf = buffer;
	int retSize = 0;
	end = buffer+size;
	buf = buffer + PacketNum * TS_SIZE;

	for (; (nextbuf = buf + 188) <= end; buf = nextbuf) 
	{

		if (*buf != 0x47) 
		{
			//fprintf (stderr, "bad sync byte\n");
			nextbuf = buf + 1;
			continue;
		}
		PacketNum++;
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
			if( (flags & DEMUX_RESET) == DEMUX_RESET )
			{
				if( buf[1] & 0x40 )
				{
					int size = nextbuf-data;
					memmove(destbuf,data,size);
					destbuf += size;
					retSize += size;
					flags |= DEMUX_PAYLOAD_START; 
					break;
				}
			}
			else
			{
				//demuxPES (data, nextbuf,(buf[1] & 0x40) ? DEMUX_PAYLOAD_START|demuxflag : demuxflag);
				int size = nextbuf-data;
				//dsyslog("ts2pes size is %d",size);
				memmove(destbuf,data,size);
				destbuf += size;
				retSize += size;
				if( buf[1] & 0x40 )
					flags |= DEMUX_PAYLOAD_START; 
				break;
			}
		}
	}
	return retSize;
}
#endif
