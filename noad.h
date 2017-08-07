/***************************************************************************
                          noaddata.h  -  description
                             -------------------
    begin                : Sun Mar 10 2002
    copyright            : (C) 2002/2004 by theNoad #709GRW
    email                : theNoad@ulmail.net
 ***************************************************************************/
 /*
   this is tha main part of noad
   most of the work is done here
*/
#ifndef __NOAD_H__
#define __NOAD_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#include <signal.h>
#endif
#include <inttypes.h>


#include "mpeg2wrap.h"
extern YUVBUF volatile lastYUVBuf;  // last yuvbuf from StdCallBack
#include "vdr_cl.h"
#include "noaddata.h"
#include "ccontrol.h"

// in vdr_cl.h #define FRAMESPERSEC 25
// in vdr_cl.h #define FRAMESPERMIN (FRAMESPERSEC*60)
#define BIGSTEP (15 * FRAMESPERSEC)
#define SMALLSTEP 1
#define LOGOSTABLETIME 40*FRAMESPERSEC

#define FRAMES_FOR_AC51DETECT (FRAMESPERSEC*2000)
#define AC3_51DETECT_SKIP 250
#define AC3ACTIVECUTOFF 60

#define FRAMES_TO_CHECK 3000
#define CHECKLOGO_FRAMES 250
#define CHECKLOGO_BLOCKS 10
#define CHECKLOGO_BLOCKFRAMES 50
#define CHECKLOGO_DIST  (5*FRAMESPERMIN)
#define CHECKLOGO_SHORTDIST  (1*FRAMESPERMIN)
#define MINBLACKLINES 15
#define MAXBLACKLINEDIFF 10
#define BACKFRAMES (FRAMESPERSEC*90)
#define OFFBACKFRAMES (FRAMESPERSEC*30)

// some defines for overlap-detection
#define PICS_READ_BEFORE (22*FRAMESPERSEC)
#define PICS_READ_AFTER (250*FRAMESPERSEC)
#define PICS_BEFORE_ONMARK (60*FRAMESPERSEC)
#define MIN_PICS_SIMILAR (2*FRAMESPERSEC)
#define OVERLAP_OK_FRAMES 75

// some defines for AUDIO-SCAN
#define AUDIO_CHECK_RANGE (FRAMESPERMIN * 2 + FRAMESPERSEC * 30)
//#define AUDIO_CHECK_RANGE (FRAMESPERMIN * 3)

extern noadData *ndata;
extern CControl* cctrl;

//statistic data
extern int totalFrames;
extern int totalDecodedFrames;
extern int decodedFramesForLogoDetection;
extern int decodedFramesForLogoCheck;
extern int decodedFramesForAC3Detection;
extern int decodedFramesForAC3Check;
extern int secsForLogoDetection;
extern int secsForLogoCheck;
extern int secsForAC3Detection;
extern int secsForAC3Scan;
extern int secsForScan;
extern bool hasBlackLines;
extern bool hasAC3;
extern bool hasOverlaps;
extern int picWidth;
extern int picHeight;
extern int osdMsg;
extern bool markComments;
// control-data
extern bool saveLogo;
extern bool backupMarks;
extern bool doPass2;
extern bool doPass3;
extern bool pass3only;
extern bool useSceneChangeDetection;
extern bool detectOverlaps;
extern bool useAudioDetection;
extern bool logoJumpDetection;
extern bool isNelonen;

bool detectBlacklines(int _index, int iFramesToCheck, cFileName *cfn, int& iTopLines, int& iBottomLines);
bool detectBlacklines(cMarks *marks, cFileName *cfn, int& iTopLines, int& iBottomLines);
void setCB_Func( cbfunc f );
cbfunc getCB_Func(void);

bool StdCallBack(void *buffer, int width, int height, YUVBUF );
int simpleCallback( void *buffer, int width, int height, YUVBUF );
int drawCallback( void *buffer, int width, int height, YUVBUF );
int BlacklineCallback( void *buffer, int width, int height, YUVBUF );
int BlackframeCallback( void *buffer, int width, int height, YUVBUF );
int checkCallback( void *buffer, int width, int height, YUVBUF );

bool checkLogo(cFileName *cfn, int startpos);
bool checkLogoShort(cFileName *cfn, int startpos);
bool doLogoDetection(cFileName *cfn, int curIndex);

void reInitNoad(int top, int bottom );
bool detectLogo( cFileName *cfn, char* logoname, int iStartFrame = 0 );
int checkLogoState(cFileName *cfn, int iState, int iCurrentFrame, int FramesToSkip, int FramesToCheck);
int findLogoChange(cFileName *cfn, int iState, int& iCurrentFrame,
                   int FramesToSkip, int repeatCheckframes=0);
void MarkToggle(cMarks *marks, int index, const char *Comment=NULL);
void moveMark( cMarks *marks, cMark *m, int iNewPos, const char *Comment=NULL);
void listMarks(cMarks *marks);
void checkOnMarkBlacklines( cMarks *marks, cMark *m, cFileName *cfn);
void checkOffMarkBlacklines( cMarks *marks, cMark *m, cFileName *cfn);
bool checkOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime);
int displayIFrame(int iFrameIndex);
bool checkBlacklineOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime, int iTopLines, int iBottomLines);
bool checkBlackFrameOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime );
void checkOnMarks(cMarks *marks, cFileName *cfn);
bool checkBlackFramesOnMarks(cMarks *marks, cFileName *cfn);
bool detectBlacklines(cMarks *marks, cFileName *cfn, int& iTopLines, int& iBottomLines);
bool detectBlacklines(cMarks *marks, cFileName *cfn, int& iTopLines, int& iBottomLines);
bool detectBlacklines(int _index, int iFramesToCheck, cFileName *cfn, int& iTopLines, int& iBottomLines);
bool checkBlacklineOnMarks(cMarks *marks, cFileName *cfn);
void cleanInactiveMarks(cMarks *marks);
void cleanActiveMarks(cMarks *marks);
void MarkCleanup(cMarks *marks, cFileName *cfn);
void pass2a(cMarks *marks, cFileName *cfn);
const char *getVersion();

int audiocallback(int mode);
bool detect_ac3_51(int StartFrame);
int find_ac3start( int iStart, int iEnd, int mode2Find);
int findVideoByPTS(uint_64 apts, int iStartFrame);
#ifdef HAVE_LIBAVCODEC
void pass3(cMarks *marks, cFileName *cfn);
#endif

int doX11Scan(noadData *thedata, const char *fName, int iNumFrames );
const char *myTime(time_t tim);
void clearStats();

int checkScenecheckOnMark( cMarks *marks, cMark *m );
void checkMarksOnIFrames(noadData *thedata, const char *fName);
void checkMarkPair(cMark **m_org, cMarks *marks, cFileName *cfn);

#ifdef VNOAD
int scanRecord( int iNumFrames, cMarks *marks = NULL );
#endif

#endif

