/***************************************************************************
                          noaddata.h  -  description
                             -------------------
    begin                : Sun Mar 10 2002
    copyright            : (C) 2002/2004 by theNoad #709GRW
    email                : theNoad@ulmail.net
 ***************************************************************************/
 /*
   this is the main part of noad
   most of the work is done here
*/
#include "config.h"
//#ifndef _GNU_SOURCE
//#define _GNU_SOURCE
//#endif

bool bOnMarksOnly = false;
#ifdef VNOAD
  bool bPlayAudio = false;
  int ossfd = 0;
  bool useAudioDetection = true;
  #define noCHECK_ONMARKS
  #define noDO_PASS2
  #define DO_NEWPASS2
  #define AUTO_CLEANUP
#else
  bool useAudioDetection = false;
  #define noDO_PASS2
  #define DO_NEWPASS2
  #define AUTO_CLEANUP
#endif

#include "noad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#include <signal.h>
#endif
#include <inttypes.h>
#include "svdrpc.h"
#include "audiotools.h"

extern "C"
{
  static const char *VERSIONSTRING = "0.7.2";
}

#ifdef VNOAD
  #include <qobject.h>
  #include <qwidget.h>
  QWidget *widget = NULL;
  #include "vnoad_f.h"
#endif

#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include "vdr_cl.h"
#include "tools.h"
#include "videodir.h"
#include "noaddata.h"
#include "ccontrol.h"
#ifdef VNOAD
#include "ctoolbox.h"
fcbfunc fCallBack = NULL;
#endif

typedef unsigned char uchar;

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

extern int demux_track;
extern int SysLogLevel;
noadData *ndata = NULL;
CControl* cctrl = NULL;
char *filename = NULL;
bool bReUseLogo = true;


void setCB_Func( cbfunc f )
{
  current_cbf = f;
}
cbfunc getCB_Func(void)
{
  return current_cbf;
}

cNoadIndexFile *cIF = NULL;
cFileName *cfn = NULL;
bool bMarkChanged = false;
extern int bFrameDisplayed;

//statistic data
int totalFrames = 0;
int totalDecodedFrames = 0;
int decodedFramesForLogoDetection = 0;
int decodedFramesForLogoCheck = 0;
int decodedFramesForAC3Detection = 0;
int decodedFramesForAC3Check = 0;
int secsForLogoDetection = 0;
int secsForLogoCheck = 0;
int secsForAC3Detection = 0;
int secsForAC3Scan = 0;
int secsForScan = 0;
bool hasBlackLines = false;
bool hasAC3 = false;
bool hasOverlaps = false;
int picWidth = 0;
int picHeight = 0;

// control-data
bool backupMarks = false;
bool isNelonen = false;
bool doPass2 = true;
#ifdef VNOAD
  bool detectOverlaps = true;
  bool saveLogo = true;
  bool useSceneChangeDetection = false;
  int osdMsg=1;
  bool logoJumpDetection = true;
  bool markComments = true;
  bool doPass3 = true;
#else
  bool detectOverlaps = false;
  bool saveLogo = false;
  bool useSceneChangeDetection = false;
  int osdMsg=0;
  bool logoJumpDetection = false;
  bool markComments = false;
  bool doPass3 = false;
#endif
bool pass3only = false;

int iLastIFrame = 0;
int iIgnoreFrames = 0;
unsigned long ulTopBlackLines;
unsigned long ulBotBlackLines;
int checkedFrames;

uchar readBuffer[MAXFRAMESIZE]; // frame-buffer
int curIndex = 1;         // current index
uint16_t FileNumber;         // current file-number
off_t FileOffset;           // current file-offset
bool Independent;        // current picture-type
int Length;               // frame-lenght of current frame
uint8_t * end;            // pointer to frame-end
YUVBUF volatile lastYUVBuf = NULL;  // last yuvbuf from StdCallBack


// Standard-Callback-Funktion
// parameter:
// buffer: zeiger auf rgb24/rgb32 image (wenn RGB-image verwendet wird)
//          oder y-buffer (wenn yuv verwendet wird)
// width:  aktuelle bildbreite
// width:  aktuelle bildh�he
// yufbuf: array von pointern auf die y/u/v-daten
bool StdCallBack(void *buffer, int width, int height, void *yuvbuf )
{
  totalDecodedFrames++;
  lastYUVBuf = yuvbuf;
  
  // soll der Frame ignoriert werden ?
  if( iIgnoreFrames )
  {
    iIgnoreFrames--;
    return false;
  }

  if( ndata && buffer && yuvbuf)
  {
    // bei �nderung der Bildgr�sse daten zur�cksetzen
    if( width != ndata->m_nGrabWidth || height != ndata->m_nGrabHeight )
    {
      delete cctrl;
      cctrl = NULL;
      ndata->setGrabSize( width, height );
      /*
      delete [] ndata->video_buffer_mem;
      ndata->video_buffer_mem = NULL;
      */
    }
    ndata->setExternalMem(buffer,(mpeg2_fbuf_t *)yuvbuf);
    //usleep(100);
    return true;
  }
  return false;
}

int simpleCallback( void *buffer, int width, int height, void *yufbuf )
{
  if( StdCallBack( buffer, width, height, yufbuf) )
  {
    #ifdef VNOAD
    if( fCallBack )
      fCallBack(iCurrentDecodedFrame,buffer,width,height, yufbuf);
    #endif
  }
  return 0;
}

int drawCallback( void *buffer, int width, int height, void *yufbuf )
{
  if( StdCallBack( buffer, width, height, yufbuf) )
  {
    if(cctrl == NULL )
    {
      cctrl = new CControl(ndata);
    }
    ndata->detectBlackLines((unsigned char *)yufbuf);
    ulTopBlackLines += ndata->m_nBlackLinesTop;
    ulBotBlackLines += ndata->m_nBlackLinesBottom;
    checkedFrames++;
    ndata->setCorners();
    if( ndata->m_bFound == false )
    {
      cctrl->newData();
    }
    else
    {
      bool bOldState = ndata->m_pCheckLogo->isLogo;
      ndata->m_pCheckLogo->newData();
      if( bOldState != ndata->m_pCheckLogo->isLogo )
      {
        dsyslog("LogoDetection: logo %s at %d", ndata->m_pCheckLogo->isLogo ? "appears" : "disappears", iLastIFrame);
      }
    }
    #ifdef VNOAD
    if( fCallBack )
      fCallBack(iCurrentDecodedFrame,buffer,width,height, yufbuf);
    #endif
  }
  return 0;
}

int BlacklineCallback( void *buffer, int width, int height, void *yufbuf )
{
  if( StdCallBack( buffer, width, height, yufbuf) )
  {
    ndata->detectBlackLines((unsigned char *)yufbuf);
    ulTopBlackLines += ndata->m_nBlackLinesTop;
    ulBotBlackLines += ndata->m_nBlackLinesBottom;
    checkedFrames++;
    #ifdef VNOAD
    if( fCallBack )
      fCallBack(iCurrentDecodedFrame,buffer,width,height, yufbuf);
    #endif
  }
  return 0;
}

int BlackframeCallback( void *buffer, int width, int height, void *yufbuf )
{
  if( StdCallBack( buffer, width, height, yufbuf) )
  {
    //ndata->checkMonoFrame(iCurrentDecodedFrame,(unsigned char **)yufbuf);
    #ifdef VNOAD
    if( fCallBack )
      fCallBack(iCurrentDecodedFrame,buffer,width,height, yufbuf);
    #endif
    ndata->CheckFrameIsBlank(iCurrentDecodedFrame,(unsigned char **)yufbuf);
    ulTopBlackLines += ndata->m_nBlackLinesTop;
    ulBotBlackLines += ndata->m_nBlackLinesBottom;
    checkedFrames++;
  }
  return 0;
}

int checkCallback( void *buffer, int width, int height, void *yufbuf )
{
  if( StdCallBack( buffer, width, height, yufbuf) )
  {
    checkedFrames++;
    ndata->setCorners();
    ndata->m_pCheckLogo->newData();
    #ifdef VNOAD
    if( fCallBack )
      fCallBack(iCurrentDecodedFrame,buffer,width,height,yufbuf);
    #endif
  }
  return 0;
}


// check for logo in some parts of the recording
// check CHECKLOGO_BLOCKS*CHECKLOGO_BLOCKFRAMES frames
// with a distance of CHECKLOGO_DIST between the blocks
// logo should be visible in at least #CHECKLOGO_FRAMES frames
bool checkLogo(cFileName *cfn, int startpos)
{
  dsyslog("checklogo for %d frames starting at frame %d", CHECKLOGO_FRAMES, startpos);
  int iLogosFound = 0;
  cbfunc cbf_old = getCB_Func();
  setCB_Func(checkCallback);

  int index = startpos;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  index = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
  for( int i = 0; i < CHECKLOGO_BLOCKS && iLogosFound < CHECKLOGO_FRAMES; i++ )
  {
    for( int ii = 1; ii <= CHECKLOGO_BLOCKFRAMES && iLogosFound < CHECKLOGO_FRAMES; ii++ )
    {
      demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
      iLogosFound += ndata->m_pCheckLogo->isLogo ? 1 : 0;
      cIF->Get( index+ii, &FileNumber, &FileOffset, &Independent, &Length);
      iCurrentDecodedFrame = index+ii;
    }
    index += CHECKLOGO_DIST;
    index = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
  }
  setCB_Func(cbf_old);
  dsyslog("checklogo for %d frames gives %d", CHECKLOGO_FRAMES, iLogosFound >= CHECKLOGO_FRAMES);
  return iLogosFound >= CHECKLOGO_FRAMES;
}

#define CHECKLOGOSHORT_FRAMES 600
bool checkLogoShort(cFileName *cfn, int startpos)
{
  dsyslog("checklogo for %d frames", CHECKLOGO_FRAMES);
  int iLogosFound = 0;
  cbfunc cbf_old = getCB_Func();
  setCB_Func(checkCallback);

  int index = startpos;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  index = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
  for( int i = 0; i < CHECKLOGO_BLOCKS && iLogosFound < CHECKLOGO_FRAMES; i++ )
  {
    for( int ii = 1; ii <= CHECKLOGO_BLOCKFRAMES && iLogosFound < CHECKLOGO_FRAMES; ii++ )
    {
      demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
		if( bTSFoundPayload )
			iLogosFound += ndata->m_pCheckLogo->isLogo ? 1 : 0;
      cIF->Get( index+ii, &FileNumber, &FileOffset, &Independent, &Length);
      iCurrentDecodedFrame = index+ii;
    }
    index += CHECKLOGO_SHORTDIST;
    index = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
  }
  setCB_Func(cbf_old);
  dsyslog("checklogo for %d frames gives %d", CHECKLOGO_FRAMES, iLogosFound >= CHECKLOGO_FRAMES);
  return iLogosFound >= CHECKLOGO_FRAMES;
/*
  dsyslog( "checklogo for %d frames from %d", CHECKLOGOSHORT_FRAMES,startpos);
  int iLogosFound = 0;
  cbfunc cbf_old = getCB_Func();
  setCB_Func(checkCallback);

  int index = startpos;
  index = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
  for( int i = 0; i < CHECKLOGOSHORT_FRAMES+200 && iLogosFound < CHECKLOGOSHORT_FRAMES; i++ )
  {
    cfn->SetOffset( FileNumber, FileOffset);
    end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
    demux (readBuffer, end, 0);
    iLogosFound += ndata->m_pCheckLogo->isLogo ? 1 : 0;
    cIF->Get( ++index, &FileNumber, &FileOffset, &PictureType, &Length);
    iCurrentDecodedFrame = index+i;
  }
  setCB_Func(cbf_old);
  dsyslog( "checklogo for %d frames gives %d", CHECKLOGOSHORT_FRAMES, iLogosFound >= CHECKLOGOSHORT_FRAMES);
  return iLogosFound >= CHECKLOGOSHORT_FRAMES;
*/
}

// try to detect a Logo within the next #FRAMES_TO_CHECK frames
bool doLogoDetection(cFileName *cfn, int startIndex)
{
  cbfunc cbf_old = getCB_Func();
  setCB_Func(drawCallback);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  // go to the next I-Frame near curIndex
  curIndex = cIF->GetNextIFrame( startIndex, true, &FileNumber, &FileOffset, &Length, true);
  dsyslog("doLogoDetection %d %d", startIndex, curIndex);
  while( curIndex >= 0 && !ndata->m_bFound && checkedFrames < FRAMES_TO_CHECK )
  {
    iLastIFrame = curIndex;
    iCurrentDecodedFrame = curIndex;
    demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
    flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    curIndex++;
    if( !cIF->Get( curIndex, &FileNumber, &FileOffset, &Independent, &Length) )
    {
      setCB_Func(cbf_old);
      return false;
    }
  }
  setCB_Func(cbf_old);
  return ndata->m_bFound;
}

void reInitNoad(int top, int bottom )
{
  dsyslog("reInitNoad %d %d", top, bottom);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;
  delete cctrl;
  cctrl = NULL;
  ndata->m_nTopLinesToIgnore = top;
  ndata->m_nBottomLinesToIgnore = bottom;
  ndata->initBuffer();
  //  ndata->m_pCheckLogo->reset();
  ndata->m_bFound = false;
  ndata->m_nLogoCorner = UNKNOWN;
  cctrl = new CControl(ndata);
}

// try to detect the Logo of the Record and verfiy it
// in some parts of the video
bool detectLogo( cFileName *cfn, char* logoname, int iStartFrame )
{
  time_t start;
  time_t end;
  reInitNoad( 0, 0 );
  if( bReUseLogo && iStartFrame == 0 )
  {
    // try to reuse a saved logo
    if( ndata->loadCheckData(logoname, true) )
    {
      start = time(NULL);
      // we have a logo
      // just check that it's ok
      if( checkLogo( cfn,0 ) )
      {
        decodedFramesForLogoCheck = totalDecodedFrames;
        end = time(NULL);
        secsForLogoCheck = end - start;
        return true;
      }
    }
  }

  // start a new logo-scan
  start = time(NULL);

  int iAStartpos[] =
  {
    0, 8*FRAMESPERMIN, 16*FRAMESPERMIN, 32*FRAMESPERMIN,
       5*FRAMESPERMIN, 10*FRAMESPERMIN, 15*FRAMESPERMIN,
      20*FRAMESPERMIN, 25*FRAMESPERMIN, 30*FRAMESPERMIN,
    -1
  };

  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;

  int iPart = 0;
  do
  {
    // detect from start
    dsyslog("detectLogo part%d",iPart+1);
    reInitNoad( 0,0 );
    if( doLogoDetection(cfn, iAStartpos[iPart]+iStartFrame ) )
      if( checkLogo( cfn, iAStartpos[iPart]+iStartFrame ) )
      //if( checkLogoShort( cfn, curIndex ) )
      {
        end = time(NULL);
        secsForLogoDetection = end - start;
        decodedFramesForLogoDetection = totalDecodedFrames;
        dsyslog("logo detected %s", 
          ndata->m_nLogoCorner == TOP_LEFT ? "top left ":
           ndata->m_nLogoCorner == TOP_RIGHT ? "top right ":
            ndata->m_nLogoCorner == BOT_LEFT ? "bottom left ":
             ndata->m_nLogoCorner == BOT_RIGHT ? "bottom right ": "unknown"
          );
        dsyslog("logo data:");
        ndata->logCheckData();
        dsyslog("logo data end");
        return true;
      }
    iPart++;
  } while( iAStartpos[iPart] >= 0);

  end = time(NULL);
  secsForLogoDetection = end - start;
  decodedFramesForLogoDetection = totalDecodedFrames;
  dsyslog("detectLogo: no Logo found, give up");
  return false;
}

#define FRAMESTOCHECK 10
int checkLogoState(cFileName *cfn, int iState, int iCurrentFrame, int /*FramesToSkip*/, int FramesToCheck)
{
  int iLastIFrame = 0;

  bool extLogoSearch = ndata->extLogoSearch;
  ndata->extLogoSearch = false;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  dsyslog("checkLogoState for %d Frames starting at Frame %d",
          FramesToCheck,iCurrentFrame);
  cbfunc cbf_old = getCB_Func();
  setCB_Func(checkCallback);

  iCurrentFrame = cIF->GetNextIFrame( iCurrentFrame, true, &FileNumber, &FileOffset, &Length, true);
  while( iCurrentFrame < cIF->Last() && iCurrentFrame >= 0 && FramesToCheck > 0 )
  {
    cIF->Get( iCurrentFrame, &FileNumber, &FileOffset, &Independent, &Length);
    iCurrentDecodedFrame = iCurrentFrame;
    checkedFrames = 0;
    iLastIFrame = iCurrentFrame;
    demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    if( Independent == 1 /*I_FRAME*/ )
    {
      iLastIFrame = iCurrentFrame;
    }
    if( bTSFoundPayload && (iState != ndata->m_pCheckLogo->isLogo) )
    {
      dsyslog("checkLogoState Logo lost, iLastIFrame is %d ", iLastIFrame);
      ndata->extLogoSearch = extLogoSearch;
      return( iLastIFrame );
    }
    iCurrentFrame ++;
    FramesToCheck --;
  }
  setCB_Func(cbf_old);
  ndata->extLogoSearch = extLogoSearch;
  dsyslog("checkLogoState ends, iLastIFrame %d ", iLastIFrame);
  return( 0 );
}

//#define CHANGE_SEARCH_BREAK FRAMESPERMIN*12
#define CHANGE_SEARCH_BREAK FRAMESPERMIN*15
int findLogoChange(cFileName *cfn, int iState, int& iCurrentFrame,
                   int FramesToSkip, int repeatCheckframes)
{
  dsyslog("findLogoChange: iState %d, iCurrentFrame %d, iTotalFrames %d, FramesToSkip %d, repeatCheckframes=%d",
          iState, iCurrentFrame, cIF->Last(), FramesToSkip, repeatCheckframes);

  if(logoJumpDetection)
  {
    if( iState == 1 )
      ndata->extLogoSearch = false;
    else
      ndata->extLogoSearch = true;
  }
    
  cbfunc cbf_old = getCB_Func();
  setCB_Func(checkCallback);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  iCurrentFrame = cIF->GetNextIFrame( iCurrentFrame, true, &FileNumber, &FileOffset, &Length, true);
  int iEndFrame = iState == 0 ? iCurrentFrame+CHANGE_SEARCH_BREAK:cIF->Last();
  Independent = 1; //I_FRAME
  #ifdef VNOAD
  bool bFirst = true;
  #endif
  while( iCurrentFrame < cIF->Last() && iCurrentFrame >= 0 && iCurrentFrame < iEndFrame )
  {
    checkedFrames = 0;
    iLastIFrame = iCurrentFrame;
    iIgnoreFrames = 5;

    while( checkedFrames < FRAMESTOCHECK && iCurrentFrame < cIF->Last() )
    {
      iCurrentDecodedFrame = iCurrentFrame;
      demuxFrame(cfn, FileNumber, FileOffset, Length,flags);
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
      if( Independent == 1 /*I_FRAME*/ )
      {
        iLastIFrame = iCurrentFrame;
      }
      cIF->Get( ++iCurrentFrame, &FileNumber, &FileOffset, &Independent, &Length);
    }
    #ifdef VNOAD
    bFirst = false;
    #endif
    if( iState != ndata->m_pCheckLogo->isLogo )
    {
      dsyslog("StateChanged: iLastIFrame %d ", iLastIFrame);
      if( repeatCheckframes > 0 )
      {
        // new found state must be stable over repeatCheckframes frames
        int i = checkLogoState(cfn, ndata->m_pCheckLogo->isLogo, iCurrentFrame, FRAMESPERSEC*1, repeatCheckframes);
        dsyslog("checkLogoState: i = %d",i);
        if( i == 0 )
          return( ndata->m_pCheckLogo->isLogo );
        else
		  {
          iCurrentFrame = i-FramesToSkip;
			 flags = DEMUX_RESET;
		  }
      }
      else
        return( ndata->m_pCheckLogo->isLogo );
    }
    if( iCurrentFrame+FramesToSkip >= cIF->Last() )
    {
      setCB_Func(cbf_old);
      //return ndata->m_pCheckLogo->isLogo;
      return iState;
    }
    if( FramesToSkip > 1 )
    {
      iCurrentFrame += FramesToSkip;
      iCurrentFrame = cIF->GetNextIFrame( iCurrentFrame, true, &FileNumber, &FileOffset, &Length, true);
      Independent = 1;//I_FRAME;
		flags = DEMUX_RESET;
    }
  }
  setCB_Func(cbf_old);
  if( iCurrentFrame >= iEndFrame )
     return -2;
  return( -1 );
}

void MarkToggle(cMarks *marks, int index, const char *Comment)
{
  dsyslog( "ToggleMark: %d ", index);
  cMark *m = marks->Get(index);
  if (m)
  {
    dsyslog( "del Mark: %d ", m->position);
    marks->Del(m);
  }
  else
  {
    dsyslog( "add Mark: %d ", index);
    marks->Add(index);
    if(markComments && Comment)
    {
      m = marks->Get(index);
      if(m)
        m->comment = strdup(Comment);
    }
  }
  marks->Save();
  bMarkChanged = true;
}

void moveMark( cMarks *marks, cMark *m, int iNewPos, const char *Comment)
{
  char *cpOldPos, *cpNewPos, *newComment;
  asprintf(&cpOldPos, "%s",(const char *)m->ToText(false));
  asprintf(&cpNewPos, "%s",(const char *)IndexToHMSF(iNewPos, false));
  asprintf(&newComment, Comment?Comment:"(%s)",(const char *)m->ToText(false) );
  dsyslog( "moveMark from %d to %d (%s-->%s)",m->position, iNewPos, cpOldPos, cpNewPos );
  marks->Del(m);
  marks->Add(iNewPos);
  cMark *mNew = marks->Get(iNewPos);
  if(markComments && mNew && newComment)
    mNew->comment = strdup(newComment);
  marks->Save();
  bMarkChanged = true;

  free(cpOldPos);
  free(cpNewPos);
  free(newComment);
}

void checkOnMarkBlacklines( cMarks *marks, cMark *m, cFileName *cfn)
{
  dsyslog( "checkOnMark at %d",m->position );
  if( m->position < BACKFRAMES )
    return;
  int index = cIF->GetNextIFrame( m->position, true, &FileNumber, &FileOffset, &Length, true);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  demuxFrame(cfn, FileNumber, FileOffset, Length, flags);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;
  for( int i = 0; i < 10; i++ )
  {
    demuxFrame(cfn, cIF, index+i, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
  }

  ulTopBlackLines /= checkedFrames;
  ulBotBlackLines /= checkedFrames;
  dsyslog( "checkOnMark TopBlackLines = %ld, BotBlackLines = %ld",ulTopBlackLines, ulBotBlackLines );
  if( ulTopBlackLines < MINBLACKLINES || ulBotBlackLines < MINBLACKLINES )
    return;

  // we have blacklines, check backward to the first frame with diff blacklines
  int iTopBlackLines = ulTopBlackLines;
  int iBotBlackLines = ulBotBlackLines;
  dsyslog( "checkOnMark ulTopBlackLines = %d, ulBotBlackLines = %d",iTopBlackLines, iBotBlackLines );
  for( int i = 0; i < BACKFRAMES; i++ )
  {
    demuxFrame(cfn, cIF, index-i,flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
	 if( bTSFoundPayload )
	 {
		 if( ndata->m_nBlackLinesTop > iTopBlackLines+MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesTop < iTopBlackLines-MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesBottom > iBotBlackLines+MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesBottom < iBotBlackLines-MAXBLACKLINEDIFF )
		 {
			int newindex = cIF->GetNextIFrame( index-i+1, true, &FileNumber, &FileOffset, &Length, true);
			if( newindex > 0 )
			{
			  dsyslog( "do moveMark from %d to %d ",index, newindex );
			  moveMark( marks, m, newindex, "moved from [%s] by checkOnMarkBlacklines");
			}
			return;
		 }
	 }
  }
}

void checkOffMarkBlacklines( cMarks *marks, cMark *m, cFileName *cfn)
{
  dsyslog( "checkOffMark at %d",m->position );
  if( m->position < OFFBACKFRAMES )
    return;
  int index = cIF->GetNextIFrame( m->position-OFFBACKFRAMES, true, &FileNumber, &FileOffset, &Length, true);
  //cfn->SetOffset( FileNumber, FileOffset);
  //end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
  //demux (readBuffer, end, 0);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  demuxFrame(cfn, FileNumber, FileOffset, Length, flags );
  flags = bTSFoundPayload ? 0 : DEMUX_RESET;
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;
  for( int i = 0; i < 10; i++ )
  {
    demuxFrame(cfn, cIF, index+i, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
  }

  ulTopBlackLines /= checkedFrames;
  ulBotBlackLines /= checkedFrames;
  dsyslog( "checkOffMark TopBlackLines = %ld, BotBlackLines = %ld",ulTopBlackLines, ulBotBlackLines );
  if( ulTopBlackLines < MINBLACKLINES || ulBotBlackLines < MINBLACKLINES )
    return;

  // we have blacklines, check forward to the first frame with diff blacklines
  int iTopBlackLines = ulTopBlackLines;
  int iBotBlackLines = ulBotBlackLines;
  dsyslog( "checkOffMark ulTopBlackLines = %d, ulBotBlackLines = %d",iTopBlackLines, iBotBlackLines );
  for( int i = 0; i < OFFBACKFRAMES; i++ )
  {
    demuxFrame(cfn, cIF, index+i, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
	 if( bTSFoundPayload )
	 {
		 if( ndata->m_nBlackLinesTop > iTopBlackLines+MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesTop < iTopBlackLines-MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesBottom > iBotBlackLines+MAXBLACKLINEDIFF ||
			  ndata->m_nBlackLinesBottom < iBotBlackLines-MAXBLACKLINEDIFF )
		 {
			int newindex = cIF->GetNextIFrame( index+i-1, true, &FileNumber, &FileOffset, &Length, true);
			if( newindex > 0 )
			{
			  dsyslog( "do moveMark from %d to %d ",index, newindex );
			  moveMark( marks, m, newindex, "moved from [%s] by checkOffMarkBlacklines");
			}
			return;
		 }
	 }
  }
}


#define BACK_TIME 90
bool checkOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime)
{
  dsyslog( "checkOnMark at %d",m->position );
  int iDiff;
  int iOffset = 0;
  if( bForward )
  {
    if( m->position+iCheckTime*FRAMESPERSEC > cIF->Last() )
      return false;
    iDiff = FRAMESPERSEC;
    iOffset = iCheckTime*FRAMESPERSEC;
  }
  else
  {
    if( m->position < iCheckTime*FRAMESPERSEC )
      return false;
    iDiff = -FRAMESPERSEC;
  }
  iDiff = -FRAMESPERSEC;
  int index = cIF->GetNextIFrame( m->position+iOffset, true, &FileNumber, &FileOffset, &Length, true);
  //cfn->SetOffset( FileNumber, FileOffset);
  //end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
  //demux (readBuffer, end, 0);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  demuxFrame(cfn, FileNumber, FileOffset, Length, flags );
  flags = bTSFoundPayload ? 0 : DEMUX_RESET;
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;

  for( int i = 0; i < iCheckTime; i++ )
  {
    for( int ii = 0; ii < FRAMESPERSEC; ii++)
    {
      demuxFrame(cfn, cIF, index+ii, flags );
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
		if( bTSFoundPayload )
		{
			if( ndata->m_nBlackLinesTop > ndata->m_nGrabHeight/2 ||
				 ndata->m_nBlackLinesBottom > ndata->m_nGrabHeight/2 )
			{
			  int newIndex = cIF->GetNextIFrame( index, true, &FileNumber, &FileOffset, &Length, true);
			  if( newIndex > 0 )
			  {
				 dsyslog( "do moveMark from %d to %d ",index, newIndex );
				 moveMark( marks, m, newIndex, "moved from [%s] by checkOnMark");
			  }
			  return true;
			}
		}
    }
    index = cIF->GetNextIFrame( index+iDiff, true, &FileNumber, &FileOffset, &Length, true);
	 flags = DEMUX_RESET;
  }
  return false;
}

int displayIFrame(int iFrameIndex)
{
  int index = iFrameIndex;
  int flags = DEMUX_RESET;
  lastYUVBuf = NULL;
  mpeg2_close (mpeg2dec);
  mpeg2dec= mpeg2_init ();
  demux_reset();
  cIF->Get( iFrameIndex, &FileNumber, &FileOffset, &Independent, &Length);
  if( Independent != I_FRAME )
    index = cIF->GetNextIFrame( iFrameIndex, true, &FileNumber, &FileOffset, &Length, true);
  do
  {
	//  int previndex = cIF->GetNextIFrame( iFrameIndex, false, &FileNumber, &FileOffset, &Length, true);
	  //cIF->Get( previndex, &FileNumber, &FileOffset, &PictureType, &Length);
	  cIF->Get( index, &FileNumber, &FileOffset, &Independent, &Length);
	  cfn->SetOffset( FileNumber, FileOffset);
	  end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
	  //for(int i = 0; i < 3 ; i++)
	//		  demux (readBuffer, end, i == 0 ? DEMUX_RESET : DEMUX_PAYLOAD_START);
	  if( cfn->isPES() )
			demuxPES (readBuffer, end, flags);
	  else
		  demuxTS (readBuffer, end, flags);
	   flags = bTSFoundPayload ? 0 : DEMUX_RESET;
		index++;
  } while (lastYUVBuf == NULL && index < cIF->Last() );
  return index;
}

bool checkBlacklineOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime, int iTopLines, int iBottomLines)
{
  #define POS_OK (ndata->m_nBlackLinesTop >= iTopMin \
    && ndata->m_nBlackLinesTop <= iTopMax \
    && ndata->m_nBlackLinesBottom >= ibotmin \
    && ndata->m_nBlackLinesBottom <= ibotmax)
  #define POS_NOT_OK (ndata->m_nBlackLinesTop < iTopMin \
    || ndata->m_nBlackLinesBottom < ibotmin)
  #define POS_MAYBE_OK (ndata->m_nBlackLinesTop > iTopMin \
    && ndata->m_nBlackLinesBottom > ibotmin)

  #define PRE_POS_OK (ndata->m_nBlackLinesTop >= iTopLines && ndata->m_nBlackLinesBottom >= iBottomLines)
  #define PRE_POS_RESET (ndata->m_nBlackLinesTop + ndata->m_nBlackLinesBottom < (iTopLines+iBottomLines)-(((iTopLines+iBottomLines)*20)/100))
  // MAX_BEST_POS_RESET_COUNT - Anzahl Frames mit POS_NOT_OK == true,
  // nach denen iBestPos zur�ckgesetzt wird
  // 20.9.03: ge�ndert auf 25 (vorher 3) wegen RTL/The 6th Day
  #define MAX_BEST_POS_RESET_COUNT 90

  dsyslog( "checkBlacklineOnMark at %d %s %d %d for %d frames",m->position, bForward ? "forward":"backward",iTopLines, iBottomLines, iCheckTime );
  int iDiff;
  int iOffset = 0;
  int iBestPos = 0;
  int iBestPosResetCount = 0;
  int iPrePos = 0;
  int iPrePosResetCount = 0;
  int iEnd = m->position;
//  int topmaxdiff = iTopLines * 20 / 100;
//  int botmaxdiff = iBottomLines * 20 / 100;
  int iTopMax = iTopLines + iTopLines*30/100;
  int iTopMin = iTopLines - iTopLines*20/100;
  int ibotmax = iBottomLines + iBottomLines*30/100;
  int ibotmin = iBottomLines - iBottomLines*30/100;
  dsyslog( "check-range top %d %d, bott %d %d",iTopMin,iTopMax,ibotmin,ibotmax );
  iOffset = 0;//iCheckTime*FRAMESPERSEC;
  if( bForward )
  {
    if( m->position+iCheckTime > cIF->Last() )
      return false;
    iDiff = FRAMESPERSEC;
    iEnd = m->position+iCheckTime;
    iBestPos = iPrePos = m->position;
  }
  else
  {
    if( m->position < iCheckTime )
      return false;
    iOffset = iCheckTime;
    iOffset *= -1;
    iDiff = FRAMESPERSEC;
  }
  int index = displayIFrame(m->position+iOffset-1);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;

  while( index < iEnd )
  {
    // get the next frame
    ++index;
    iCurrentDecodedFrame = index;
    ulTopBlackLines = 0;
    ulBotBlackLines = 0;
    demuxFrame(cfn, cIF, index,0 );

    if( iPrePos == 0 && PRE_POS_OK )
    {
      iPrePos = index;
      iPrePosResetCount = 0;
      dsyslog( "checkOnMark set prepos %d",iPrePos );
    }
    if( iPrePos != 0 && PRE_POS_RESET )
    {
      iPrePosResetCount++;
      if(iPrePosResetCount > 3)
      {
        iPrePos = 0;
        dsyslog( "checkOnMark preposreset at %d",index );
      }
    }
    if( (iBestPos == 0) && (checkedFrames > 3) && POS_OK )
    {
      iBestPos = index;
      dsyslog( "checkOnMark best pos = %d, prepos = %d",iBestPos, iPrePos );
      iBestPosResetCount = 0;
    }
    else if( (iBestPos > 0) && (checkedFrames > 3)  && POS_NOT_OK )
    {
      iBestPosResetCount++;
      dsyslog( "checkOnMark pos not ok at frame %3d top %3d topmin %3d bot %3d botmin %3d resetcount %3d", index,ndata->m_nBlackLinesTop ,iTopMin,ndata->m_nBlackLinesBottom , ibotmin, iBestPosResetCount  );
      if(iBestPosResetCount > MAX_BEST_POS_RESET_COUNT)
      {
        dsyslog( "checkOnMark reset best pos to 0 at %d", index );
        iBestPos = 0;
        checkedFrames = 0;
      }
    }
  }
  dsyslog( "checkOnMark bestPos %d prepos %d",iBestPos,iPrePos );
  if( (iBestPos == 0 || (iPrePos - iBestPos) < 200) && POS_MAYBE_OK && iPrePos > 0 )
  {
    iBestPos = iPrePos;
    dsyslog( "checkOnMark use prepos to set newmark" );
  }
  if( iBestPos > 0 )
  {
    int newIndex = cIF->GetNextIFrame( iBestPos-1, true, &FileNumber, &FileOffset, &Length, true);
    if( newIndex > 0 && newIndex != m->position)
    {
      dsyslog( "do moveMark from %d to %d, prepos = %d ",m->position, newIndex, iPrePos );
      if(bForward)
        moveMark( marks, m, newIndex,"moved from [%s] by checkBlacklineOnMark(forward)");
      else
        moveMark( marks, m, newIndex,"moved from [%s] by checkBlacklineOnMark(backward)");
      return true;
    }
    if( newIndex == m->position)
    {
      dsyslog( "mark is on same position" );
      if(!bForward)
        return false;
    }
  }
  return false;
#undef POS_OK
#undef POS_NOT_OK
#undef POS_MAYBE_OK
#undef PRE_POS_OK
#undef PRE_POS_RESET
}

bool checkBlacklineOffMark( cMarks *marks, cMark *m, cFileName *cfn, int iCheckTime, int iTopLines, int iBottomLines)
{
    #define POS_OK (ndata->m_nBlackLinesTop >= iTopMin \
     && ndata->m_nBlackLinesTop <= iTopMax \
     && ndata->m_nBlackLinesBottom >= ibotmin \
     && ndata->m_nBlackLinesBottom <= ibotmax)
    #define POS_NOT_OK (ndata->m_nBlackLinesTop < iTopMin \
     || ndata->m_nBlackLinesBottom < ibotmin)
    #define POS_MAYBE_OK (ndata->m_nBlackLinesTop > iTopMin \
     && ndata->m_nBlackLinesBottom > ibotmin)

   #define PRE_POS_OK (ndata->m_nBlackLinesTop >= iTopLines && ndata->m_nBlackLinesBottom >= iBottomLines)
   #define PRE_POS_RESET (ndata->m_nBlackLinesTop + ndata->m_nBlackLinesBottom < (iTopLines+iBottomLines)-(((iTopLines+iBottomLines)*20)/100))

  dsyslog( "checkBlacklineOffMark at %d %d %d for %d frames",m->position, iTopLines, iBottomLines, iCheckTime );
  int iDiff;
  int iOffset = 0;
  int iBestPos = 0;
  int iPrePos = 0;
  int iEnd = m->position;
//  int topmaxdiff = iTopLines * 20 / 100;
//  int botmaxdiff = iBottomLines * 20 / 100;
  int iTopMax = iTopLines + iTopLines*30/100;
  int iTopMin = iTopLines - iTopLines*20/100;
  int ibotmax = iBottomLines + iBottomLines*30/100;
  int ibotmin = iBottomLines - iBottomLines*30/100;
  dsyslog( "check-range top %d %d, bott %d %d",iTopMin,iTopMax,ibotmin,ibotmax );
  iOffset = 0;

  if( m->position < iCheckTime )
    return false;
  iOffset = iCheckTime;
  iOffset *= -1;
  iDiff = FRAMESPERSEC;

  int index = displayIFrame(m->position+iOffset-1);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;

  while( index < iEnd )
  {
    // get the next frame
    ++index;
    iCurrentDecodedFrame = index;
    ulTopBlackLines = 0;
    ulBotBlackLines = 0;
    demuxFrame(cfn, cIF, index, 0 );

    if( (checkedFrames > 3) && POS_OK )
    {
      iBestPos = index;
      //dsyslog( "checkOffMark best pos = %d, prepos = %d",iBestPos, iPrePos );
    }
    /*
    else if( (iBestPos > 0) && (checkedFrames > 3)  && POS_NOT_OK )
    {
      dsyslog( "checkOffMark reset best pos to 0" );
      iBestPos = 0;
      checkedFrames = 0;
    }
    */
  }
  dsyslog( "checkOffMark bestPos %d prepos %d",iBestPos,iPrePos );
  if( (iBestPos == 0 || (iPrePos - iBestPos) < 200) && POS_MAYBE_OK && iPrePos > 0 )
  {
    iBestPos = iPrePos;
    dsyslog( "checkOffMark use prepos to set newmark" );
  }
  if( iBestPos > 0 )
  {
    int newIndex = cIF->GetNextIFrame( iBestPos-1, true, &FileNumber, &FileOffset, &Length, true);
    if( newIndex > 0 && newIndex != m->position)
    {
      dsyslog( "do moveMark from %d to %d, prepos = %d ",m->position, newIndex, iPrePos );
      moveMark( marks, m, newIndex, "moved from [%s] by checkBlacklineOffMark");
      return true;
    }
    if( newIndex == m->position)
    {
      dsyslog( "mark is on same position" );
    }
  }
  return false;
#undef POS_OK
#undef POS_NOT_OK
#undef POS_MAYBE_OK
#undef PRE_POS_OK
#undef PRE_POS_RESET
}

#define MONOFRAME_CUTOFF 65
bool checkBlackFrameOnMark( cMarks *marks, cMark *m, cFileName *cfn, bool bForward, int iCheckTime )
{
  dsyslog( "checkBlackFrameOnMark at %d %s",m->position, bForward ? "forward":"backward" );
  int iOffset = 0;
  int iBestPos = 0;
  int iEnd = m->position;
//  int minlines = ndata->m_nGrabHeight*80/100;
  bool bPosReset = true;
  if( bForward )
  {
    if( m->position+iCheckTime*FRAMESPERSEC > cIF->Last() )
      return false;
    iEnd = m->position+iCheckTime*FRAMESPERSEC;
    // don't walk over the next mark! (0.4.2)
    cMark *mnext = marks->GetNext(m->position);
    if( mnext != NULL && iEnd > mnext->position )
      iEnd = mnext->position;
  }
  else
  {
    if( m->position < iCheckTime*FRAMESPERSEC )
      return false;
    iOffset = iCheckTime*FRAMESPERSEC;
    iOffset *= -1;
  }
  int index = displayIFrame(m->position+iOffset-1);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;

  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlackframeCallback);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  while( index < iEnd )
  {
    index++;
    iCurrentDecodedFrame = index;
    ulTopBlackLines = 0;
    ulBotBlackLines = 0;
    demuxFrame(cfn, cIF, index, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    //dsyslog( "checkBlackFrameOnMark frame %d bPosReset %d isBlankFrame %d",index, bPosReset, ndata->isBlankFrame() );
	 if( bTSFoundPayload )
	 {
		 if( bPosReset && ndata->isBlankFrame() )
		 {
			dsyslog( "set iBestPos to %d",index);
			iBestPos = index;
			bPosReset = false;
		 }
		 if( !ndata->isBlankFrame() )
			bPosReset = true;
	 }
    /*
    if( bPosReset && ndata->m_nMonoFrameValue < MONOFRAME_CUTOFF )
    {
      dsyslog( "set iBestPos to %d",index);
      iBestPos = index;
      bPosReset = false;
    }
    if( ndata->m_nMonoFrameValue > MONOFRAME_CUTOFF )
      bPosReset = true;
    */
  }
  setCB_Func(cbf_old);
  if( iBestPos > 0 )
  {
    int newIndex = cIF->GetNextIFrame( iBestPos-1, true, &FileNumber, &FileOffset, &Length, true);
    if( newIndex > 0 && newIndex < iEnd)
    {
      dsyslog( "do moveMark from %d to %d ",m->position, newIndex );
      moveMark( marks, m, newIndex, "moved from[%s] by checkBlackFrameOnMark");
      listMarks( marks);
    }
    return true;
  }
  return false;
}

void checkOnMarks(cMarks *marks, cFileName *cfn)
{
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlacklineCallback);
  cMark *m = marks->GetNext(-1);

  while( m != NULL )
  {
    int iPos = m->position;
    if( !checkOnMark( marks, m, cfn, true, 60 ) )
      checkOnMark( marks, m, cfn, false, 90 );
    m = marks->GetNext(iPos);
    if( m != NULL )
      m = marks->GetNext(m->position);
  }
  setCB_Func(cbf_old);
}


bool checkBlackFramesOnMarks(cMarks *marks, cFileName *cfn)
{
  dsyslog( "check video for BlackFrames" );
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlacklineCallback);
  
  #ifdef VNOAD
  cNoAdMarks noadMarks;
  noadMarks.Load(cfn->Name());
  #endif
  
  cMark *m = marks->GetNext(-1);
  while( m != NULL )
  {
    //dsyslog( "Mark: %d %s %d %s", m->position, cpos, iDiff, cdiff);
    int iPos = m->position;
    bool doBlackFrameDetection = true;
    
    if(useSceneChangeDetection)
    {
      int iHow = checkScenecheckOnMark( marks, m );
      doBlackFrameDetection = iHow == 0;
      if( iHow == 2 )
      {
          m = marks->GetNext(iPos);
          iPos = m->position;
      }
    }

    if( doBlackFrameDetection )
    {
      if( !checkBlackFrameOnMark( marks, m, cfn, false, 45 ) )
        if( checkBlackFrameOnMark( marks, m, cfn, true, 120 ) ) // reset this!
        {
          m = marks->GetNext(iPos);
          iPos = m->position;
        }
    }
    m = marks->GetNext(iPos);
    if( m != NULL )
      m = marks->GetNext(m->position);
  }
  setCB_Func(cbf_old);
  dsyslog( "check for Blackframes done" );
  return true;
}

bool detectBlacklines(cMarks *marks, cFileName *cfn, int& iTopLines, int& iBottomLines)
{
#define RESETBLACKLINES  ulTopBlackLines = 0; ulBotBlackLines = 0; checkedFrames = 0; iBlacklineResets++;

  bool bStartpos = false;
  int index = 0;
  dsyslog( "detectBlacklines" );
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlacklineCallback);

  // Startposition finden
  cMark *m = marks->GetNext(-1);
  // erste Unterbrechung �berspringen
  // wenn mehrere Unterbrechungen vorhanden sind...
  if( marks->Count() > 2 )
  {
    if( m != NULL )
      m = marks->GetNext(m->position);
    if( m != NULL )
      m = marks->GetNext(m->position);
  }
  while( !bStartpos && m != NULL )
  {
    // zum n�chsten I-Frame
    index = cIF->GetNextIFrame( m->position, true, &FileNumber, &FileOffset, &Length, true);
    // 5 Minuten weiter
    index += 5*FRAMESPERMIN;
    if( m != NULL )
      m = marks->GetNext(m->position);
    // pr�fen, ob mind. 2000 Frames verf�gbar sind
    if( m != NULL )
    {
      if( index + 2000 < m->position )
        bStartpos = true;
      else
        m = marks->GetNext(m->position);
    }
  }
  if( !bStartpos )
  {
    setCB_Func(cbf_old);
    return false;
  }
  index = displayIFrame(index);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;
  int iNumFramesNoBlacklines = 0;
  int iBlacklineResets = 0;
  bool bOk = false;
  while( !bOk )
  {
    // Abbruch bei der n�chsten Unterbrechung
    if( index > m->position )
    {
      dsyslog( "detectBlacklines out of range" );
      setCB_Func(cbf_old);
      return( false );
    }
    // Abbruch, wenn 50 Frames ohne Blacklines gefunden wurden
    if( iNumFramesNoBlacklines > 50 )
    {
      dsyslog( "detectBlacklines: video has no Blacklines" );
      setCB_Func(cbf_old);
      return( false );
    }
    // Frame holen und decodieren
    iCurrentDecodedFrame = index;
    demuxFrame(cfn, cIF, index,0 );
    index++;

//    dsyslog( "detectBlacklines (%d) TopBlackLines = %3d, BotBlackLines = %3d (%4d) %d",index,ndata->m_nBlackLinesTop, ndata->m_nBlackLinesBottom,ndata->m_nBlackLinesTop+ndata->m_nBlackLinesBottom,checkedFrames );
    // Reset-Bedingungen:
    // - Anzahl der Blacklines oben oder unten < MINBLACKLINES
    // - Anzahl der Blacklines oben oder unten > 1/3 der Bildh�he
    // - Differenz Top- und BottomBlacklines > 50
    if( ndata->m_nBlackLinesTop < MINBLACKLINES ||
        ndata->m_nBlackLinesBottom < MINBLACKLINES )
    {
      iNumFramesNoBlacklines++;
      RESETBLACKLINES;
    }
    if( ndata->m_nBlackLinesTop > ndata->m_nGrabHeight/3 ||
        ndata->m_nBlackLinesBottom > ndata->m_nGrabHeight/3 ||
        abs(ndata->m_nBlackLinesTop-ndata->m_nBlackLinesBottom) > 50)
    {
      RESETBLACKLINES;
    }
    // wenn iBlacklineResets > 10, dann 1 Min springen
    if( iBlacklineResets == 10 )
    {
      index += FRAMESPERMIN;
      iBlacklineResets = 0;
    }
    if( checkedFrames > 50 )
      bOk = true;
//    dsyslog( "detectBlacklines (%d) TopBlackLines = %3d, BotBlackLines = %3d (%4d) %d",index,ndata->m_nBlackLinesTop, ndata->m_nBlackLinesBottom,ndata->m_nBlackLinesTop+ndata->m_nBlackLinesBottom,checkedFrames );
  }
  iTopLines = (ulTopBlackLines / checkedFrames)*90/100;
  iBottomLines = (ulBotBlackLines / checkedFrames)*90/100;
  dsyslog( "TopBlackLines = %d, BotBlackLines = %d",iTopLines, iBottomLines );
  setCB_Func(cbf_old);
  return true;
#undef RESETBLACKLINES
}

bool detectBlacklines(int _index, int iFramesToCheck, cFileName *cfn, int& iTopLines, int& iBottomLines)
{
#define RESETBLACKLINES  ulTopBlackLines = 0; ulBotBlackLines = 0; checkedFrames = 0; iBlacklineResets++;

  dsyslog( "detectBlacklines from %d over %d frames", _index, iFramesToCheck );
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlacklineCallback);

  int index = displayIFrame(_index);
  ulTopBlackLines = 0;
  ulBotBlackLines = 0;
  checkedFrames = 0;
  int iNumFramesNoBlacklines = 0;
  int iBlacklineResets = 0;
  int endFrame = _index + iFramesToCheck;
  bool bOk = false;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  while( !bOk )
  {
    // Abbruch, wenn 50 Frames ohne Blacklines gefunden wurden
    if( iNumFramesNoBlacklines > 50 || index > endFrame )
    {
      dsyslog( "detectBlacklines: video has no Blacklines in range %d %d", _index, iFramesToCheck );
      setCB_Func(cbf_old);
      return( false );
    }
    // Frame holen und decodieren
    iCurrentDecodedFrame = index;
    demuxFrame(cfn, cIF, index, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    index++;

	 if( bTSFoundPayload )
	 {
		 dsyslog( "detectBlacklines (%d) TopBlackLines = %3d, BotBlackLines = %3d (%4d) %d",index,ndata->m_nBlackLinesTop, ndata->m_nBlackLinesBottom,ndata->m_nBlackLinesTop+ndata->m_nBlackLinesBottom,checkedFrames );
		 // Reset-Bedingungen:
		 // - Anzahl der Blacklines oben oder unten < MINBLACKLINES
		 // - Anzahl der Blacklines oben oder unten > 1/3 der Bildh�he
		 // - Differenz Top- und BottomBlacklines > 50
		 if( ndata->m_nBlackLinesTop < MINBLACKLINES ||
			  ndata->m_nBlackLinesBottom < MINBLACKLINES )
		 {
			iNumFramesNoBlacklines++;
			RESETBLACKLINES;
		 }
		 if( ndata->m_nBlackLinesTop > ndata->m_nGrabHeight/3 ||
			  ndata->m_nBlackLinesBottom > ndata->m_nGrabHeight/3 ||
			  abs(ndata->m_nBlackLinesTop-ndata->m_nBlackLinesBottom) > 50)
		 {
			RESETBLACKLINES;
		 }
		 // wenn iBlacklineResets > 10, dann 1 Min springen
		 if( iBlacklineResets == 10 )
		 {
			index += FRAMESPERMIN;
			flags = DEMUX_RESET;
			iBlacklineResets = 0;
		 }
		 if( checkedFrames > 50 )
			bOk = true;
		 dsyslog( "detectBlacklines (%d) TopBlackLines = %3d, BotBlackLines = %3d (%4d) %d",index,ndata->m_nBlackLinesTop, ndata->m_nBlackLinesBottom,ndata->m_nBlackLinesTop+ndata->m_nBlackLinesBottom,checkedFrames );
	 }
  }
  iTopLines = (ulTopBlackLines / checkedFrames);//*95/100;
  iBottomLines = (ulBotBlackLines / checkedFrames);//*95/100;
  dsyslog( "TopBlackLines = %d, BotBlackLines = %d",iTopLines, iBottomLines );
  setCB_Func(cbf_old);
  return true;
}

/*
*  checkBlacklineOnMarks
*  pr�ft, ob die Aufnahme schwarze R�nder oben und unten hat
*  ist dies der Fall, werden die Einschaltmarken nach Untersuchung
*  der umliegenden Bilder ggf. angepasst
*  im Normalfall wird das Senderlogo erst einige sekunden nach
*  Filmbeginn eingeblendet, daher wird zuerst r�ckw�rts gesucht
*  ist die r�ckw�rtssuche erfolglos, wird vorw�rts gesucht
*  wg. RTL wird vorher von der Schnittmarke+3min r�ckw�rts gesucht
*/
bool checkBlacklineOnMarks(cMarks *marks, cFileName *cfn)
{
  int iTopLines = 0;
  int iBottomLines = 0;
  // pr�fen auf R�nder
  detectBlacklines(marks, cfn, iTopLines, iBottomLines);
  if( iTopLines < MINBLACKLINES || iBottomLines < MINBLACKLINES )
  {
    dsyslog( "not enough Blacklines for further inspection" );
    return false;
  }

  hasBlackLines = true;
  // callback-funktion setzen
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlacklineCallback);
  // auf 1. Schnittmarke
  cMark *m = marks->GetNext(-1);
  while( m != NULL )
  {
    int iPos = m->position;
    int iBackFrames = 60*FRAMESPERSEC;      // Anzahl Frames f�r normale r�ckw�rtssuche
    int iForwardFrames = 200*FRAMESPERSEC;  // Anzahl Frames f�r vorw�rtssuche
    // r�ckw�rtssuche bis max zur vorherigen Schnittmarke
    cMark *m2 = marks->GetPrev(iPos);
    if( m2 != NULL )
    {
      if( m->position - m2->position < iBackFrames )
        iBackFrames = m->position - m2->position -10;
      if( iBackFrames < 0 )
        iBackFrames = 0;
    }
    // vorw�rtssuche bis max zur n�chsten Schnittmarke
    m2 = marks->GetNext(iPos);
    if( m2 != NULL )
    {
      if( m2->position - m->position < iForwardFrames )
        iForwardFrames = m2->position - m->position -10;
      if( iForwardFrames < 0 )
        iForwardFrames = 0;
    }

    m->position = iPos;
    if( checkBlacklineOnMark( marks, m, cfn, false, iBackFrames, iTopLines, iBottomLines ) )
    {
      m = marks->GetPrev(iPos+1);
      iPos = m->position;
    }
    if( checkBlacklineOnMark( marks, m, cfn, true, iForwardFrames, iTopLines, iBottomLines ) )
    {
      m = marks->GetNext(iPos);
      iPos = m->position;
    }
/*
      if( !checkBlacklineOnMark( marks, m, cfn, false, iBackFrames, iTopLines, iBottomLines ) )
      {
        if( checkBlacklineOnMark( marks, m, cfn, true, iForwardFrames, iTopLines, iBottomLines ) )
        {
          m = marks->GetNext(iPos);
          iPos = m->position;
        }
      }
*/
    m = marks->GetNext(iPos);
    if( m != NULL )
    {
      iPos = m->position;
      if( checkBlacklineOffMark( marks, m, cfn, OFFBACKFRAMES, iTopLines, iBottomLines ) )
      ;
      m = marks->GetNext(iPos+1);
    }
    listMarks( marks);
  }
  setCB_Func(cbf_old);
  dsyslog( "check for Blacklines done" );
  return true;
}

void listMarks(cMarks *marks)
{
  dsyslog( "current Marks:" );
  int iDiff;
  int iLastPos = 0;
  char *cpos = NULL;
  char *cdiff = NULL;

  cMark *m = marks->GetNext(-1);
//  cMark *mprev = NULL;

  while( m != NULL )
  {
    iDiff = m->position - iLastPos;
    asprintf(&cpos, "%s", (const char *)m->ToText(false));
    asprintf(&cdiff, "%s", (const char *)IndexToHMSF(iDiff, false));
    dsyslog( "Mark: %6d %s #frames %6d duration %s", m->position, cpos, iDiff, cdiff);
    iLastPos = m->position;
    m = marks->GetNext(m->position);
  }
  dsyslog( "current Marks end" );
}

#define MINMARKDURATION (17*FRAMESPERSEC)
#define MINMARKDURATION2 (70*FRAMESPERSEC)
#define ACTIVEMINMARKDURATION (60*FRAMESPERSEC)
void cleanInactiveMarks(cMarks *marks)
{
  int iDiff;
  int iLastPos = 0;
  cMark *m = marks->GetNext(-1);
  cMark *mprev = NULL;
  while( m != NULL )
  {
    iDiff = m->position - iLastPos;
    iLastPos = m->position;
    if( iDiff > 0 && iDiff < MINMARKDURATION )
    {
      if( mprev != NULL )
      {
        dsyslog( "del Mark: %d ", mprev->position );
        marks->Del(mprev);
        marks->Del(m);
        m = marks->GetNext(-1);
        mprev = NULL;
        iLastPos = 0;
        bMarkChanged = true;
      }
    }
    else
    {
      mprev = m;
      m = marks->GetNext(m->position);
    }
  }
}

void cleanActiveMarks(cMarks *marks)
{
  int iDiff;
  int iLastPos = 0;
  char *cpos = NULL;
  char *cdiff = NULL;
  cMark *m = marks->GetNext(-1);
  if( m == NULL )
    return;
    
  cMark *mprev = NULL;
  mprev = m;
  iLastPos = m->position;
  if( m != NULL )
    m = marks->GetNext(m->position);

  while( m != NULL )
  {
    iDiff = m->position - iLastPos;
    asprintf(&cpos, "%s", (const char *)m->ToText(false));
    asprintf(&cdiff, "%s", (const char *)IndexToHMSF(iDiff, false));
    dsyslog( "Mark: %d %s #frames %d duration %s", m->position, cpos, iDiff, cdiff);
    iLastPos = m->position;
    if( iDiff > 0 && iDiff < ACTIVEMINMARKDURATION )
    {
      if( mprev != NULL )
      {
        dsyslog( "del Marks: %d %d", mprev->position, m->position );
        marks->Del(mprev);
        marks->Del(m);
        m = marks->GetNext(-1);
        mprev = NULL;
        iLastPos = 0;
        bMarkChanged = true;
      }
    }
    else
    {
      m = marks->GetNext(m->position);
      mprev = m;
      if( m != NULL )
      {
        iLastPos = m->position;
        m = marks->GetNext(m->position);
      }
    }
  }
}

// MarkCleanup
// delete all marks where the difference between
// the marks is shorter than MINMARKDURATION frames
void MarkCleanup(cMarks *marks, cFileName *cfn)
{
  dsyslog( "MarkCleanup: %p %p ", (void *)marks, (void *)cfn );
  int iDiff;
  int iLastPos = 0;
  char *cpos = NULL;
  char *cdiff = NULL;

  listMarks( marks);
  cMark *m = marks->GetNext(-1);
  cMark *mprev = m;
  listMarks( marks);
  cleanInactiveMarks(marks);
/*
  m = marks->GetNext(-1);
  mprev = NULL;
  while( m != NULL )
  {
    iDiff = m->position - iLastPos;
    iLastPos = m->position;
    if( iDiff > 0 && iDiff < MINMARKDURATION )
    {
      if( mprev != NULL )
      {
        dsyslog( "del Mark: %d ", mprev->position );
        marks->Del(mprev);
        marks->Del(m);
        m = marks->GetNext(-1);
        mprev = NULL;
        iLastPos = 0;
        bMarkChanged = true;
      }
    }
    else
    {
      mprev = m;
      m = marks->GetNext(m->position);
    }
  }
*/
  listMarks( marks);

#ifdef CHECK_ONMARKS
  marks->Save();
  checkOnMarks(marks, cfn);
#endif
  if( !detectOverlaps )
  {
    if( !checkBlacklineOnMarks(marks, cfn) )
      checkBlackFramesOnMarks(marks, cfn);
    listMarks( marks);
  
    m = marks->GetNext(-1);
    mprev = m;
    iLastPos = m->position;
    if( m != NULL )
      m = marks->GetNext(m->position);
  
    while( m != NULL )
    {
      iDiff = m->position - iLastPos;
      asprintf(&cpos, "%s", (const char *)m->ToText(false));
      asprintf(&cdiff, "%s", (const char *)IndexToHMSF(iDiff, false));
      dsyslog( "Mark: %d %s #frames %d duration %s", m->position, cpos, iDiff, cdiff);
      iLastPos = m->position;
      if( iDiff > 0 && iDiff < ACTIVEMINMARKDURATION )
      {
        if( mprev != NULL )
        {
          dsyslog( "del Marks: %d %d", mprev->position, m->position );
          marks->Del(mprev);
          marks->Del(m);
          m = marks->GetNext(-1);
          mprev = NULL;
          iLastPos = 0;
          bMarkChanged = true;
        }
      }
      else
      {
        m = marks->GetNext(m->position);
        mprev = m;
        if( m != NULL )
        {
          iLastPos = m->position;
          m = marks->GetNext(m->position);
        }
      }
    }
    listMarks( marks);
  
    m = marks->GetNext(-1);
    mprev = NULL;
    while( m != NULL )
    {
      iDiff = m->position - iLastPos;
      asprintf(&cpos, "%s", (const char*)m->ToText(false));
      asprintf(&cdiff, "%s", (const char *)IndexToHMSF(iDiff, false));
      dsyslog( "Mark: %d %s #frames %d duration %s", m->position, cpos, iDiff, cdiff);
      iLastPos = m->position;
      if( iDiff > 0 && iDiff < MINMARKDURATION )
      {
        if( mprev != NULL )
        {
          dsyslog( "del Mark: %d ", mprev->position );
          marks->Del(mprev);
          marks->Del(m);
          m = marks->GetNext(-1);
          mprev = NULL;
          iLastPos = 0;
          bMarkChanged = true;
        }
      }
      else
      {
        mprev = m;
        m = marks->GetNext(m->position);
      }
    }
  
    listMarks( marks);
  }
  else
  {
    pass2a(marks, cfn);
  }
  #ifdef HAVE_LIBAVCODEC
  if( doPass3 )
    pass3(marks, cfn);
  #endif
}



int iCurrentDecodedAudioFrame = 0;
int audiocallback(int /*mode*/)
{
  iCurrentDecodedFrame++;
  iCurrentDecodedAudioFrame++;
  hasAC3 = streamHasAC3;
  return 0;
}

bool detect_ac3_51(int StartFrame)
{
  dsyslog( "start ac3-detection");
  time_t tstart, tend;
  tstart = time(NULL);
  cbfunc cbf_old = getCB_Func();
  setCB_Func(simpleCallback);
  current_audiocbf = audiocallback;
  hasAC3 = streamHasAC3 = false;
  ac3mode = 0;
  iCurrentDecodedFrame = 0;
  checkedFrames = 0;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  //int iCurrentFrame = cIF->Get( StartFrame, &FileNumber, &FileOffset, &PictureType, &Length);
  if( !cIF->Get( StartFrame, &FileNumber, &FileOffset, &Independent, &Length) )
    return false;
  int iCurrentFrame = StartFrame;
  int ac3mode51count = 0;
  while(  iCurrentFrame < cIF->Last() && iCurrentFrame < StartFrame+FRAMES_FOR_AC51DETECT && ac3mode51count < 50 )
  {
    //cfn->SetOffset( FileNumber, FileOffset);
    //end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
    iCurrentDecodedFrame = iCurrentFrame;
    //demux (readBuffer, end, 0);
    demuxFrame(cfn, FileNumber, FileOffset, Length, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    if( ac3mode == 51 )
    {
      while(  iCurrentFrame < cIF->Last() && iCurrentFrame < StartFrame+FRAMES_FOR_AC51DETECT && ac3mode51count < 50 && ac3mode == 51)
      {
        iCurrentFrame++;
        if (iCurrentFrame < cIF->Last())
        {
          iCurrentDecodedFrame = iCurrentFrame;
          demuxFrame(cfn, cIF, iCurrentFrame, flags );
          if( ac3mode == 51 )
            ac3mode51count++;
        }
      }
    }
    else
      ac3mode51count = 0;
    dsyslog( "ac3-detection at %d ac3mode is %d", iCurrentFrame, ac3mode);
    iCurrentFrame += AC3_51DETECT_SKIP;
	 flags = DEMUX_RESET;
    if (iCurrentFrame < cIF->Last())
      cIF->Get( iCurrentFrame, &FileNumber, &FileOffset, &Independent, &Length);
  }
  setCB_Func(cbf_old);
  current_audiocbf = NULL;

  tend = time(NULL);
  secsForAC3Detection = tend - tstart;
  dsyslog( "ac3-detection done, hasAC3 is %s", (ac3mode == 51)?"true":"false");
  if( ac3mode51count >= 50 )
    return true;
  return false;
}

int find_ac3start( int iStart, int iEnd, int mode2Find)
{
  int iCurrentFrame = 0;
  int ipos;
  int diff = iEnd-iStart;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  while( diff > 10 )
  {
    iCurrentDecodedAudioFrame = 0;
    ipos = iStart+((iEnd - iStart + 1) / 2);
    if( ipos == iCurrentFrame )
      ipos--;
    while( iCurrentDecodedAudioFrame == 0 && ipos < iEnd )
    {
      iCurrentFrame = ipos;
      iCurrentDecodedFrame = iCurrentFrame;
      demuxFrame(cfn, cIF, ipos++, flags );
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    }
    if( ac3mode == mode2Find )
    {
      iEnd = iCurrentFrame;
    }
    else
    {
      iStart = iCurrentFrame;
    }
    diff = iEnd-iStart;
  }
  iCurrentFrame = iStart;
  ac3mode = 0;
  bTSFoundPayload = false;
  flags = DEMUX_RESET;
  while( iCurrentFrame <= iEnd && ac3mode != mode2Find)
  {
    iCurrentFrame++;
    iCurrentDecodedFrame = iCurrentFrame;
    iCurrentDecodedAudioFrame = 0;
    ac3mode = 0;
    demuxFrame(cfn, cIF, iCurrentFrame, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
  }
  //printf("vmode changed in frame %d to %d\n",iCurrentFrame-1,ac3mode);
  return iCurrentFrame-1;
}

int findVideoByPTS(uint_64 apts, int iStartFrame)
{
  //dsyslog( "findVideoByPTS from frame %d", iStartFrame);
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  do
  {
    demuxFrame(cfn, cIF, iStartFrame, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    iStartFrame--;
  } while( iStartFrame > 0 && videopts > apts );
  //dsyslog( "findVideoByPTS returns %d", iStartFrame+1);
  return iStartFrame+1;
}


bool ac3Pass1(cMarks *pmarks/*,fcbfunc f*/)
{
  bool bHasAC3_51 = false;
  int cframe = 0;
  int iLastLogoFrame = 0;
  time_t tstart, tend;
  current_audiocbf = audiocallback;
  bHasAC3_51 = detect_ac3_51(0);
  current_audiocbf = NULL;
  if( !bHasAC3_51 )
    return false;
  tstart = time(NULL);
  current_audiocbf = audiocallback;
  cbfunc cbf_old = getCB_Func();
  setCB_Func(simpleCallback);

  ac3mode = 0;
  int oldac3mode = ac3mode;
  iCurrentDecodedFrame = 0;
  iCurrentDecodedAudioFrame = 0;
  checkedFrames = 0;
  int iCurrentFrame = cIF->Get( 0, &FileNumber, &FileOffset, &Independent, &Length);

  dsyslog( "start ac3 pass1");
  if( backupMarks )
    pmarks->Backup(filename);
  pmarks->Load(filename,DEFAULTFRAMESPERSECOND,cfn->isPES());
  pmarks->ClearList();
  bMarkChanged = true;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  while(  iCurrentFrame < cIF->Last() /*&& !bStopAction*/ )
  {
    iCurrentDecodedAudioFrame = 0;
    while(  iCurrentFrame < cIF->Last() /*&& !bStopAction*/ && iCurrentDecodedAudioFrame == 0 )
    {
      iCurrentDecodedFrame = iCurrentFrame;
      demuxFrame(cfn, cIF, iCurrentFrame, flags );
		flags = bTSFoundPayload ? 0 : DEMUX_RESET;
      if( !iCurrentDecodedAudioFrame )
        iCurrentFrame++;
    }
    dsyslog( "Audio: oldac3mode = %d ac3mode = %d at %d", oldac3mode, ac3mode,iCurrentFrame);
    //if( oldac3mode != ac3mode && ((oldac3mode==51)||(oldac3mode==0)||(ac3mode==51)))
	 if( bTSFoundPayload && oldac3mode != ac3mode && ((oldac3mode==51 && ac3mode !=51 )||(oldac3mode!=51 && ac3mode==51)) )
    {
      int ac3mode_help = ac3mode;
      int iStart = iCurrentFrame - AC3_51DETECT_SKIP;
      if( iStart < 1 )
        iStart = 1;
      cframe = find_ac3start( iStart, iCurrentFrame, ac3mode);
      //if( oldac3mode != 0 || ac3mode == 51 )
      if( oldac3mode == 51 || oldac3mode == 0 ||ac3mode == 51 )
      {
          cframe = findVideoByPTS(ac3pts, cframe);
          iLastLogoFrame = cIF->GetNextIFrame( cframe, ac3mode_help == 51, &FileNumber, &FileOffset, &Length, true);
          dsyslog( "AudioChanged: iLastLogoFrame=%6d, audio changed from %d to %d ", iLastLogoFrame, oldac3mode,ac3mode_help);
          //MarkToggle(pmarks, iLastLogoFrame, "audio-change");
          MarkToggle(pmarks, iLastLogoFrame, ac3mode_help != 51 ? "ac3 lost" : "ac3 start");
          bMarkChanged = true;
      }
      oldac3mode = ac3mode = ac3mode_help;
    }
    iCurrentFrame += AC3_51DETECT_SKIP;
	 flags = DEMUX_RESET;
  }
  setCB_Func(cbf_old);
  current_audiocbf = NULL;
  tend = time(NULL);
  secsForAC3Scan = tend - tstart;
  dsyslog( "ac3 pass1 done");
  int iaframes = pmarks->getActiveFrames(cIF->Last());
  dsyslog( "ac3 pass1 marks %d frames of %d as active",iaframes,cIF->Last());
  if( iaframes < ( cIF->Last() * AC3ACTIVECUTOFF / 100 ) )
  {
    dsyslog( "ac3 pass1 kicks out more than %d%%, marks dropped", 100-AC3ACTIVECUTOFF);
    pmarks->ClearList();
    return false;
  }
  return true;
}

// scan the hole record for the Logo and set
// the cutting-marks
int scanRecord( int iNumFrames, cMarks *_marks )
{
  cMarks *pmarks = NULL;
  cMarks *localMarks = NULL;
  time_t start, startall;
  time_t end;

  if(osdMsg)
  {
    noadStartMessage(filename);
  }
  clearStats();

  start = startall = time(NULL);
  #ifndef VNOAD
  // open the cIndexFile for the record
  bool isPES = isPESRecording(filename);
  setMarkfileSuffix(isPES);
  cIF = new cNoadIndexFile(filename,false, isPES);
  if( cIF == NULL )
    return -1;
  #endif
  totalFrames = cIF->Last();
  if(cIF->Last() == iNumFrames)
  {
    #ifndef VNOAD
    delete cIF;
    #endif
    return iNumFrames;
  }

  #ifndef VNOAD
  // open the record
  cfn = new cFileName(filename, false, false, isPES);
  if( cfn->Open() < 0 )
  {
    delete cfn;
    delete cIF;
    return -1;
  }
  int _isHDTV = isHDTV(cfn->File());
  if( _isHDTV )
  {
    esyslog("looks like HDTV, can't handle this file, give up");
    delete cfn;
    delete cIF;
    return -1;
  }

  demux_track = getVStreamID(cfn->File());
  if( isPES )
		demux_pid = 0;
  else
	  demux_pid = getTSPID(cfn->File());
  #endif

  if( _marks )
    pmarks = _marks;
  else
    pmarks = localMarks = new cMarks();
  
#ifdef HAVE_LIBAVCODEC
  if( pass3only )
  {
    pmarks->Load(filename,DEFAULTFRAMESPERSECOND,cfn->isPES());
    pass3(pmarks, cfn);
    #ifndef VNOAD
    delete cfn;
    delete cIF;
    #endif
    delete localMarks;
    return cIF->Last();
  }
#endif

  bool pass1Done = false;
  if( useAudioDetection )
  {
    pass1Done = ac3Pass1(pmarks/*,NULL*/);
  }

  if( !pass1Done )
  {
    char logoname[2048];
    strcpy( logoname, filename);
    strcat( logoname, "/cur.logo" );

    if( !detectLogo(cfn,logoname) )
    {
      #ifndef VNOAD
      delete cfn;
      delete cIF;
      #endif
      delete localMarks;
      return 0;
    }
    if( saveLogo )
      ndata->saveCheckData(logoname, true);

    if( !bOnMarksOnly )
    {
      int iCurrentFrame, iLastStartFrame, iStopFrame;
      int iRestartFrame = 0;
      int iState = 0;
      int iOldState = -1;

      iCurrentFrame = 0;
      iStopFrame = 0;

      if( backupMarks )
        pmarks->Backup(filename);
      pmarks->Load(filename,DEFAULTFRAMESPERSECOND,cfn->isPES());

      pmarks->ClearList();

      while(  iCurrentFrame < cIF->Last() && iCurrentFrame >= 0 && iState != -1 )
      {
        iOldState = iState;
        iLastStartFrame = iCurrentFrame;
        iState = findLogoChange(cfn, iState, iCurrentFrame, BIGSTEP );
        dsyslog( "StateChanged: newstate=%d oldstate=%d currentFrame=%d", iState, iOldState, iCurrentFrame);
        if( iState == -2 ) // means no logo for a long time
        {
          if( iRestartFrame == iLastStartFrame )
          {
            // avoid looping
            iState = iOldState;
            iCurrentFrame = iLastStartFrame+CHANGE_SEARCH_BREAK;
          }
          else
          {
            if( !detectLogo(cfn,logoname,iLastStartFrame+FRAMESPERMIN*5) )
              iState = -1;
            else
            {
              iState = 0;
              ndata->m_pCheckLogo->isLogo = false;
              iCurrentFrame = iRestartFrame = iLastStartFrame;
              continue;
            }
          }
        }
        
        if( iState != iOldState && iState != -1 )
        {
            //iCurrentFrame -= (BIGSTEP+BIGSTEP/2);
            iCurrentFrame -= (BIGSTEP*2);
          if( iCurrentFrame < iStopFrame )
            iCurrentFrame += (BIGSTEP/2);
          if( iCurrentFrame < iStopFrame )
            iCurrentFrame = iStopFrame;
          if( isNelonen )
            iState = findLogoChange(cfn, iOldState, iCurrentFrame, SMALLSTEP, iState==1?LOGOSTABLETIME:LOGOSTABLETIME/2 );
          else
            iState = findLogoChange(cfn, iOldState, iCurrentFrame, SMALLSTEP, iState==1?LOGOSTABLETIME:0 );
          if( iState >= 0 && iState != iOldState ) // changed 04/11/04 (@Autobahnpolizei...)
          {
            int iLastLogoFrame;
            if( isNelonen )
            {
              if (iState == 0) {
                iLastLogoFrame = cIF->GetNextIFrame( iLastIFrame+72, false, &FileNumber, &FileOffset, &Length, true);
              } else {
                iLastLogoFrame = cIF->GetNextIFrame( iLastIFrame-1, false, &FileNumber, &FileOffset, &Length, true);
              }
            }
            else
              iLastLogoFrame = cIF->GetNextIFrame( iLastIFrame-1, false, &FileNumber, &FileOffset, &Length, true);
            MarkToggle(pmarks, iLastLogoFrame, iState == 0 ? "Logo lost" : "Logo start");
          }
        }
        iStopFrame = iCurrentFrame = iCurrentFrame+1;
      }
    }
  }
  if( pmarks->Count() & 1 )
  {
    int iLastIFrame = cIF->GetNextIFrame( cIF->Last(), false, &FileNumber, &FileOffset, &Length, true);
    pmarks->Add(iLastIFrame);
    if( markComments )
    {
      cMark *m = pmarks->Get(iLastIFrame);
      m->comment = strdup("noad mark on last IFrame");
    }
  }

  if( doPass2 )
    MarkCleanup(pmarks, cfn);
  
  dsyslog( "Marks before Save:" );
  listMarks(pmarks);
  pmarks->Save();
  #ifndef VNOAD
  setCB_Func(NULL);
  #endif

  end = time(NULL);
  secsForScan = end - startall;
  picWidth = ndata->m_nGrabWidth;
  picHeight = ndata->m_nGrabHeight;

  dsyslog( "totalFrames %d",totalFrames );
  dsyslog( "totalDecodedFrames %d",totalDecodedFrames );
  dsyslog( "decodedFramesForLogoDetection %d",decodedFramesForLogoDetection );
  dsyslog( "decodedFramesForLogoCheck %d",decodedFramesForLogoCheck );
  dsyslog( "decodedFramesForAC3Detection %d",decodedFramesForAC3Detection );
  dsyslog( "decodedFramesForAC3Check %d",decodedFramesForAC3Check );
  dsyslog( "secsForLogoDetection %d",secsForLogoDetection );
  dsyslog( "secsForLogoCheck %d",secsForLogoCheck );
  dsyslog( "secsForAC3Detection %d",secsForAC3Detection );
  dsyslog( "secsForAC3Scan %d",secsForAC3Scan );
  dsyslog( "secsForScan %d",secsForScan );
  dsyslog( "hasBlackLines %s",hasBlackLines?"yes":"no" );
  dsyslog( "hasAC3 %s",hasAC3?"yes":"no" );
  dsyslog( "hasOverlaps %s",hasOverlaps?"yes":"no" );
  dsyslog( "picSize %dx%d",picWidth,picHeight );

  int iRet = cIF->Last();
  #ifndef VNOAD
  delete cfn;
  delete cIF;
  #endif
  delete localMarks;
  if(osdMsg)
  {
    noadEndMessage(filename);
  }
  return iRet;
}

const char *getVersion()
{
  return VERSIONSTRING;
}


int doX11Scan(noadData *thedata, const char *fName, int iNumFrames )
{
  if( fName == NULL )
    return -1;
  else
    filename = (char *)fName;
  ndata = thedata;
  demux_track = 0xe0;
  mpeg2dec = mpeg2_init();
  int iRet = scanRecord(iNumFrames,NULL);
  mpeg2_close (mpeg2dec);
  return iRet;
}

const char *myTime(time_t tim)
{
  static char t_buf[2048];
  //strftime(t_buf,2048,"%A,%d.%m.%Y %T",localtime(&tim));
  strftime(t_buf,2048,"%A,%d.%m.%Y %X",localtime(&tim));
  return t_buf;
}

void clearStats()
{
  totalFrames = 0;
  totalDecodedFrames = 0;
  decodedFramesForLogoDetection = 0;
  decodedFramesForLogoCheck = 0;
  secsForLogoDetection = 0;
  secsForLogoCheck = 0;
  secsForScan = 0;
  hasBlackLines = false;
  hasAC3 = false;
  hasOverlaps = false;
  picWidth = 0;
  picHeight = 0;
}

//#include "frame.h"
//#include "commercial_skip.h"
#define SCENECHECKFRAMESBEFORE (45*FRAMESPERSEC)
#define SCENECHECKFRAMESAFTER  (130*FRAMESPERSEC)
#define SCENECHECKFRAMES (SCENECHECKFRAMESBEFORE+SCENECHECKFRAMESAFTER)
#define SCENECHANGECOUNTRANGE (30*FRAMESPERSEC)
typedef struct
{
  int count;
  int lastscenechange;
  int blackcount;
  int lastblack;
} scs;

#define SCFRAMESCHECK 20
#define SCCUTOFF      20
#define SCRESET       30
// return 0 if no pos found
// return 1 if mark is not moved or moved backwards
// return 2 if mark is moved forward
int checkScenecheckOnMark( cMarks *marks, cMark *m )
{
  dsyslog( "checkScenecheckOnMark at %d",m->position );
  //int iSceneCheckFrames = SCENECHECKFRAMES;
  scs ChangeFreq[SCENECHECKFRAMES/FRAMESPERSEC];

  int iStart = m->position-SCENECHECKFRAMESBEFORE;
  int iEnd = m->position+SCENECHECKFRAMESAFTER;
  int sceneChangesAfter = 0;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  
  if( iEnd > cIF->Last() )
      return 0;
  if( marks )
  {
    cMark *m2 = marks->GetPrev(m->position);
    if( m2 )
      if( iStart < m2->position )
        iStart = m2->position;
  }
  if( iStart < 0 )
    iStart = 0;

  int index = displayIFrame(iStart);
  ndata->resetSceneChangeDetection();

  memset(ChangeFreq,0,SCENECHECKFRAMES/FRAMESPERSEC*sizeof(scs));
  checkedFrames = 0;
  cbfunc cbf_old = getCB_Func();
  setCB_Func(BlackframeCallback);

  while( index < iEnd )
  {
    index++;
    iCurrentDecodedFrame = index;
    demuxFrame(cfn, cIF, index, flags );
	 flags = bTSFoundPayload ? 0 : DEMUX_RESET;
    if(checkedFrames>0)
    {
        int scsindex = (index-iStart)/FRAMESPERSEC;
        ndata->CheckSceneHasChanged();
        if( ndata->SceneHasChanged() )
        {
          ChangeFreq[scsindex].count++;
          ChangeFreq[scsindex].lastscenechange = index;
          if( index > m->position && index <= (m->position+SCENECHANGECOUNTRANGE) )
            sceneChangesAfter++;
        }
        if( ndata->isBlankFrame() )
        {
          ChangeFreq[scsindex].blackcount++;
          ChangeFreq[scsindex].lastblack = index;
        }
    }
  }
  dsyslog( "checkScenecheckOnMark at %d, sceneChangesAfter %d ",m->position, sceneChangesAfter );

  int iScPos = 0;
  int iFirst5SecBlock=0;
  int noChangeCount = 0;
  for( int i = 0 ; i < (SCENECHECKFRAMES/FRAMESPERSEC); i++)
  {
    #ifdef VNOAD
    dsyslog("freq at %3d(%6d) is %3d %6d %3d %6d",i,i*FRAMESPERSEC+iStart,ChangeFreq[i].count, ChangeFreq[i].lastscenechange,ChangeFreq[i].blackcount,ChangeFreq[i].lastblack);
    #endif
    if( i < (SCENECHECKFRAMES/FRAMESPERSEC)-1 )
    {
      if(ChangeFreq[i].lastblack > 0 && ChangeFreq[i+1].count <= 2 )
        iScPos = ChangeFreq[i].lastscenechange;
    }
    if( ChangeFreq[i].lastblack )
      iFirst5SecBlock = noChangeCount = 0;
    if(ChangeFreq[i].count)
      noChangeCount = 0;
    else
      noChangeCount++;
    if( noChangeCount == 5 && iFirst5SecBlock == 0 )
      iFirst5SecBlock = i;
  }

  int iFirst10SecBlock=0;
  int iFirst10SecBlockBlackBack=0;
  for( int i = 0 ; i < (SCENECHECKFRAMES/FRAMESPERSEC)-SCFRAMESCHECK; i++)
  {
    if(iFirst10SecBlock == 0 )
    {
      int i10secChangeCount = 0;
      int j = i;
      while( j < i+SCFRAMESCHECK && i10secChangeCount < SCCUTOFF )
        i10secChangeCount += ChangeFreq[j++].count;
      if( i10secChangeCount < SCCUTOFF )
      {
        iFirst10SecBlock = i;
        for( j = i; j > 0 && j > i-5 && iFirst10SecBlockBlackBack==0; j-- )
            iFirst10SecBlockBlackBack = ChangeFreq[j].lastblack;
      }
    }
    if(ChangeFreq[i].count > SCRESET )
      iFirst10SecBlock = 0;
  }
  dsyslog("iFirst10SecBlock %d (%d) --> %d",iFirst10SecBlock, iFirst10SecBlock*FRAMESPERSEC+iStart,iFirst10SecBlockBlackBack);
  
  int sOfMark = (m->position-iStart)/FRAMESPERSEC;
  int iBackBlack = 0;
  while(iBackBlack == 0 && sOfMark >= 0)
    iBackBlack = ChangeFreq[sOfMark--].lastblack;
  sOfMark = (m->position-iStart)/FRAMESPERSEC;
  int iForwardBlack = 0;
  int iMarkpos = 0;
  while(iForwardBlack == 0 && sOfMark < (SCENECHECKFRAMES/FRAMESPERSEC) )
    iForwardBlack = ChangeFreq[sOfMark++].lastblack;
  if(iFirst5SecBlock > 0 )
  {
    
    for(int i = iFirst5SecBlock; i > 0 && iMarkpos == 0; i-- )
      iMarkpos = ChangeFreq[i].lastblack;
  }
  setCB_Func(cbf_old);
  dsyslog("iScPos %d iFirst5SecBlock %d iBackBlack %d iForwardBlack %d lastindex %d",iScPos, iFirst5SecBlock, iBackBlack,iForwardBlack, index);
  dsyslog("iMarkpos %d ",iMarkpos);
  if( marks && iMarkpos )
  {
    int iRet=1;
    if( iMarkpos > m->position )
      iRet = 2;

    int iNextIFrame = cIF->GetNextIFrame( iMarkpos, true, &FileNumber, &FileOffset, &Length, true);
    moveMark( marks, m, iNextIFrame, "moved from [%s] by checkScenecheckOnMark");
    return iRet;
  }
  return 0;
}

void checkMarksOnIFrames(noadData * /*thedata*/, const char *fName)
{
  bool isPES = isPESRecording(fName);
  setMarkfileSuffix(isPES);
  cIF = new cNoadIndexFile(fName,false,isPES);
  cMarks marks;
  marks.Load(fName);
  cMark *m = marks.GetNext(-1);

  if( m == NULL )
    dsyslog("no marks!");
  while( m != NULL )
  {
    Independent = false;
    cIF->Get( m->position, &FileNumber, &FileOffset, &Independent, &Length);
    if( Independent != I_FRAME )
      dsyslog( "mark not on iframe %d",m->position);
    m = marks.GetNext(m->position);
  }
  delete cIF;
  cIF = NULL;
}

typedef struct {
  int framenum;
  int topBlacklines;
  int botBlacklines;
  int isBlackFrame;
  simpleHistogram histogramm;
} pic_info;

int getPicInfo(pic_info *info, int iStartFrame, int framesToRead,
                bool bGetHisto, bool bGetBlacklines, bool bGetBlackframe)
{
  int iCount = 0;
  bool independent = false;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;

  cIF->Get(iStartFrame,  &FileNumber, &FileOffset, &independent, &Length);
  int iCurrentFrame = iStartFrame;
  if( independent != I_FRAME )
    iCurrentFrame = cIF->GetNextIFrame( iStartFrame, true, &FileNumber, &FileOffset, &Length, true);
  displayIFrame(iCurrentFrame);
  dsyslog("getPicInfo read frames from %d to %d",iCurrentFrame,iCurrentFrame+framesToRead);
  while(  iCurrentFrame < cIF->Last() && iCount < framesToRead )
  {
    info[iCount].framenum = iCurrentFrame;
    // get the histogramm
    if( bGetHisto )
      ndata->getHistogram(info[iCount].histogramm);
    // get blacklines
    if( bGetBlacklines )
    {
      ndata->detectBlackLines((unsigned char *)lastYUVBuf);
      info[iCount].topBlacklines = ndata->m_nBlackLinesTop;
      info[iCount].botBlacklines = ndata->m_nBlackLinesBottom;
      //dsyslog("index %d frame %d top %d bot %d",iCount,iCurrentFrame,ndata->m_nBlackLinesTop,ndata->m_nBlackLinesBottom);
    }
    // get blackframe
    if( bGetBlackframe )
    {
      ndata->CheckFrameIsBlank(iCurrentDecodedFrame,(unsigned char **)lastYUVBuf);
      info[iCount].isBlackFrame = ndata->isBlankFrame();
    }

    iCount++;
    iCurrentFrame++;
    iCurrentDecodedFrame = iCurrentFrame;
    demuxFrame(cfn, cIF, iCurrentFrame, 0 );
  }
  return iCount;
}
  
bool doOverlapDetection(pic_info *picsbefore, int numPicsBefore, pic_info *picsafter, int numPicsAfter, cMark **m, cMarks *pmarks)
{
  dsyslog("doOverlapDetection with %d pics before and %d pics after mark", numPicsBefore, numPicsAfter);
  int mostOk = 0;
  int mostOkCompStart = 0;
  int mostOkCmpStart = 0;
  int compstart = 0;
  int iCmpStart = PICS_BEFORE_ONMARK;
  bool posFound = false;
  cMark *m2 = NULL;
  int compareCount = 0;

    while(compstart < numPicsBefore-MIN_PICS_SIMILAR && !posFound)
    {
      while( iCmpStart < (numPicsAfter-numPicsBefore) && !posFound )
      {
        int cmpCount = compstart;
        int picsOK = 0;
        bool compareOk = true;
        do
        {
          bool simil = ndata->areSimilar(picsbefore[cmpCount].histogramm,picsafter[iCmpStart+cmpCount-compstart].histogramm);
          //dsyslog( "doOverlapDetection, compare %3d %3d gives %d",picsbefore[cmpCount].framenum,picsafter[iCmpStart+cmpCount-compstart].framenum,simil);
          compareCount++;
          if( !simil )
          {
            compareOk = false;
          }
          else
            picsOK++;
          cmpCount++;
        } while( cmpCount < PICS_READ_BEFORE && compareOk /*&& picsOK < MIN_PICS_SIMILAR*/);
        if(cmpCount-compstart > mostOk )
        {
          mostOk = cmpCount-compstart;
          mostOkCompStart = compstart;
          mostOkCmpStart = iCmpStart;
        }
        if( mostOk >= OVERLAP_OK_FRAMES )
          posFound=true;
        iCmpStart++;
      }
      if(!posFound)
      {
        compstart++;
        iCmpStart = 0;
      }
    }

    dsyslog("doOverlapDetection #of compares %d",compareCount);

  dsyslog( "doOverlapDetection, posFound is %d %6d %6d, mostok is %3d at compstart %d iCmpStart %d",posFound, picsbefore[compstart].framenum,picsafter[iCmpStart].framenum-1, mostOk, mostOkCompStart, iCmpStart );
  if(!posFound && mostOk >= OVERLAP_OK_FRAMES )
  {
    posFound = true;
    compstart = mostOkCompStart;
    iCmpStart = mostOkCmpStart;
  }
  dsyslog( "doOverlapDetection, posFound is %d %6d %6d, mostok is %3d at compstart %d iCmpStart %d",posFound, picsbefore[compstart].framenum,picsafter[iCmpStart].framenum-1, mostOk, mostOkCompStart, iCmpStart );
  if(posFound)
  {
    dsyslog( "doOverlapDetection, %d %d",picsbefore[compstart+mostOk].framenum, picsafter[iCmpStart+mostOk].framenum );
    int newIndex = picsbefore[compstart+mostOk].framenum;
    newIndex = cIF->GetNextIFrame( newIndex, false, &FileNumber, &FileOffset, &Length, true);
    /*cIF->Get( newIndex, &FileNumber, &FileOffset, &PictureType, &Length);
    if( PictureType != I_FRAME )
      newIndex = cIF->GetNextIFrame( newIndex, true, &FileNumber, &FileOffset, &Length, true);*/
    if( newIndex > 0 && newIndex != (*m)->position)
    {
      dsyslog( "do moveMark from %d to %d",(*m)->position, newIndex );
      char *nComment = NULL;
      asprintf(&nComment,"%s (%d)","moved from (%s) by doOverlapDetection",mostOk);

      //moveMark( pmarks, *m, newIndex, "moved from (%s) by doOverlapDetection");
      moveMark( pmarks, *m, newIndex, nComment);
      free(nComment);
      
      *m = pmarks->Get(newIndex);
      m2=pmarks->GetNext(newIndex);
      hasOverlaps = true;
    }
    else
      m2=pmarks->GetNext((*m)->position);
    newIndex = cIF->GetNextIFrame( picsafter[iCmpStart+mostOk].framenum, true, &FileNumber, &FileOffset, &Length, true);
    if( newIndex > 0 && newIndex != m2->position)
    {
      dsyslog( "do moveMark from %d to %d",m2->position, newIndex );
      moveMark( pmarks, m2, newIndex, "moved from [%s] by doOverlapDetection");
    }
  }
  return posFound;
}

int iTopLines = 0;
int iBottomLines = 0;
void checkBlacklineOnmark(cMark *m, cMarks *marks, pic_info *picsafter, int iPics)
{
/*
  #define POS_OK (picsafter[iPics].topBlacklines >= iTopMin \
    && picsafter[iPics].topBlacklines <= iTopMax \
    && picsafter[iPics].botBlacklines >= ibotmin \
    && picsafter[iPics].botBlacklines <= ibotmax)
*/
  #define POS_OK (picsafter[iPics].topBlacklines >= iTopMin \
    && picsafter[iPics].botBlacklines >= ibotmin )
  #define NOTOKCOUNT 10
//  int iTopMax = iTopLines + iTopLines*30/100;
  int iTopMin = iTopLines - iTopLines*20/100;
//  int ibotmax = iBottomLines + iBottomLines*30/100;
  int ibotmin = iBottomLines - iBottomLines*70/100;
//  int index = iPics-1;
  int notOkCount = 0;
  do{
    if( !POS_OK )
      ++notOkCount;
    else
      notOkCount = 0;
    iPics--;
    //dsyslog("iPics is %d (frame %d) %3d %3d",iPics, picsafter[iPics].framenum,picsafter[iPics].topBlacklines,picsafter[iPics].botBlacklines);
  }while( iPics > 0 && notOkCount < NOTOKCOUNT );
  //dsyslog("checkBlacklineOnmark iPics is %d (frame %d)",iPics, picsafter[iPics+NOTOKCOUNT].framenum);
  if( iPics > 1 )
  {
    int iNextIFrame = cIF->GetNextIFrame( picsafter[iPics+5].framenum, true, &FileNumber, &FileOffset, &Length, true);
    if( iNextIFrame >= picsafter[iPics+5].framenum )
      moveMark( marks, m, iNextIFrame, "moved from [%s] by checkBlacklineOnmark(new)");
  }
}


// return 0 if no pos found
// return 1 if mark is not moved or moved backwards
// return 2 if mark is moved forward
int checkScenecheckOnMark( cMarks *marks, cMark *m, pic_info *picsafter, int iPics)
{
  dsyslog( "checkScenecheckOnMark at %d for %d pics",m->position, iPics );
  //int iSceneCheckFrames = SCENECHECKFRAMES;
  if( iPics < 1000 )
     return 0;

  int numscs = iPics/FRAMESPERSEC+1;
  scs *ChangeFreq = new scs[numscs];

  int iStart = 0;
//  int iEnd = iPics;
  int sceneChangesAfter = 0;


  memset(ChangeFreq,0,numscs*sizeof(scs));

  for( int i = 1; i < iPics; i++ )
  {
     int scsindex = i/FRAMESPERSEC;
     if( !ndata->areSimilar(picsafter[i-1].histogramm,picsafter[i].histogramm) )
     {
       ChangeFreq[scsindex].count++;
       ChangeFreq[scsindex].lastscenechange = picsafter[i-1].framenum;
       if( picsafter[i-1].framenum > m->position )
         sceneChangesAfter++;
     }
     if( picsafter[i-1].isBlackFrame )
     {
       ChangeFreq[scsindex].blackcount++;
       ChangeFreq[scsindex].lastblack = picsafter[i-1].framenum;
     }
  }
  dsyslog( "checkScenecheckOnMark at %d, sceneChangesAfter %d ",m->position, sceneChangesAfter );

  int iScPos = 0;
  int iFirst5SecBlock=0;
  int noChangeCount = 0;
  for( int i = 0 ; i < numscs; i++)
  {
    #ifdef VNOAD
    dsyslog("freq at %3d(%6d) is %3d %6d %3d %6d",i,i*FRAMESPERSEC+iStart,ChangeFreq[i].count, ChangeFreq[i].lastscenechange,ChangeFreq[i].blackcount,ChangeFreq[i].lastblack);
    #endif
    if( i < (SCENECHECKFRAMES/FRAMESPERSEC)-1 )
    {
      if(ChangeFreq[i].lastblack > 0 && ChangeFreq[i+1].count <= 2 )
        iScPos = ChangeFreq[i].lastscenechange;
    }
    if( ChangeFreq[i].lastblack )
      iFirst5SecBlock = noChangeCount = 0;
    if(ChangeFreq[i].count)
      noChangeCount = 0;
    else
      noChangeCount++;
    if( noChangeCount == 5 && iFirst5SecBlock == 0 )
      iFirst5SecBlock = i;
  }


  int iFirst10SecBlock=0;
  for( int i = 0 ; i < numscs-10 && iFirst10SecBlock == 0; i++)
  {
    int i10secChangeCount = 0;
    int j = 1;
    while( j < i+10 && i10secChangeCount < 10 )
      i10secChangeCount += ChangeFreq[j--].count;
    if( i10secChangeCount < 10 )
      iFirst10SecBlock = i;
  }
  dsyslog("iFirst10SecBlock %d",iFirst10SecBlock);


  int sOfMark = (m->position-picsafter[0].framenum)/FRAMESPERSEC;
  int iBackBlack = 0;
  while(iBackBlack == 0 && sOfMark >= 0)
    iBackBlack = ChangeFreq[sOfMark--].lastblack;
  sOfMark = (m->position-picsafter[0].framenum)/FRAMESPERSEC;
  int iForwardBlack = 0;
  int iMarkpos = 0;
  while(iForwardBlack == 0 && sOfMark < numscs )
    iForwardBlack = ChangeFreq[sOfMark++].lastblack;
  if(iFirst5SecBlock > 0 )
  {

    for(int i = iFirst5SecBlock; i > 0 && iMarkpos == 0; i-- )
      iMarkpos = ChangeFreq[i].lastblack;
  }
  dsyslog("iScPos %d iFirst5SecBlock %d iBackBlack %d iForwardBlack %d",iScPos, iFirst5SecBlock, iBackBlack,iForwardBlack);
  dsyslog("iMarkpos %d ",iMarkpos);
  
  delete [] ChangeFreq;
  if( marks && iMarkpos )
  {
    int iRet=1;
    if( iMarkpos > m->position )
      iRet = 2;

    int iNextIFrame = cIF->GetNextIFrame( iMarkpos, true, &FileNumber, &FileOffset, &Length, true);
    listMarks(marks);
    moveMark( marks, m, iNextIFrame, "moved from [%s] by checkScenecheckOnMark");
    listMarks(marks);
    return iRet;
  }
  return 0;
}


#define MAXBACK (45*FRAMESPERSEC)
bool checkBlackFrameOnMarks(cMark *m, cMarks *marks, pic_info *picsafter, int iPics)
{
//  dsyslog( "checkBlackFrameOnMark at %d %s",m->position, bForward ? "forward":"backward" );
  bool bForward = false;
  dsyslog( "checkBlackFrameOnMark at %d %s",m->position, bForward ? "forward":"backward" );
//  int iOffset = 0;
  int iBestPos = 0;
//  int iEnd = m->position;
//  int minlines = ndata->m_nGrabHeight*80/100;
  bool bPosReset = true;
  bool doBlackFrameDetection = true;
  int iPos = m->position;

  if(useSceneChangeDetection)
  {
    int iHow = checkScenecheckOnMark( marks, m, picsafter, iPics );
    doBlackFrameDetection = iHow == 0;
    if( iHow == 2 )
    {
        m = marks->GetNext(iPos);
        iPos = m->position;
    }
  }

  if( doBlackFrameDetection )
  {
    int index = -1;
    for(int i = 0; index < 0 && i < iPics; i++)
      if( picsafter[i].framenum == m->position )
        index = i;

    if( !bForward )
    {
      int istop = index - MAXBACK;
      if( istop < 0 )
         istop = 0;
      while( index > istop && !iBestPos )
      {
        index--;
        dsyslog( "checkBlackFrameOnMark frame %d bPosReset %d isBlankFrame %d",index, bPosReset, picsafter[index].isBlackFrame );
        if( picsafter[index].isBlackFrame )
        {
          dsyslog( "set iBestPos to %d",index);
          iBestPos = picsafter[index].framenum;
        }
      }
    }

    if( iBestPos > 1 )
    {
      int newIndex = cIF->GetNextIFrame( iBestPos-1, true, &FileNumber, &FileOffset, &Length, true);
      if( newIndex > 0 )
      {
        dsyslog( "do moveMark from %d to %d ",m->position, newIndex );
        moveMark( marks, m, newIndex, "moved from [%s] by checkBlackFrameOnMark");
      }
      return true;
    }
    return false;
  }
  return true;
}


void checkMarkPair(cMark **m_org, cMarks *marks, cFileName *cfn)
{
  dsyslog("checkMarkPair at %d", (*m_org)->position);

  bool done = false;
  int iCount = 0;
  int numPicsBefore = 0;
  int numPicsAfter = 0;
  int iStartFrame = 0;
  cMark *m2 = NULL;
  pic_info *picsbefore = new pic_info[PICS_READ_BEFORE];
  pic_info *picsafter = new pic_info[PICS_READ_AFTER];
  cMark *m = *m_org;
  
  cbfunc cbf_old = getCB_Func();
  setCB_Func(simpleCallback);
  int iStopFrame = 0;
  if( m->position >= 0 )
  {
    // read the frames before the first mark
    iStartFrame = m->position - (PICS_READ_BEFORE+15);
    // dont go into previous mark-pair
    m2 = marks->GetPrev(m->position);
    if( m2 && iStartFrame < m2->position )
      iStartFrame = m2->position;
    // dont go before start
    if( iStartFrame < 0 )
      iStartFrame = 0;
    int numFrames = PICS_READ_BEFORE;
    cMark *m3 = marks->GetNext(m->position);
    if( m3 && iStartFrame+numFrames > m3->position )
      numFrames = m3->position -iStartFrame - 20;
    dsyslog("checkMarkPair read frames before first mark from %d ",iStartFrame);
    numPicsBefore = getPicInfo(picsbefore, iStartFrame, numFrames, true, true, true );
    iStopFrame = iStartFrame + numFrames;
  }
  // read the frames around the second mark,
  // starting PICS_BEFORE_ONMARK before the mark
  m2 = marks->GetNext(m->position);
  if( m2 != NULL )
  {
    iCount = 0;
    iStartFrame = m2->position - PICS_BEFORE_ONMARK;
    if( iStartFrame < iStopFrame )
       iStartFrame = iStopFrame+20;
    int numFrames = PICS_READ_AFTER;
    cMark *m3 = marks->GetNext(m2->position);
    if( m3 )
      dsyslog("checkMarkPair m3pos is %d",m3->position);
    if( m3 && iStartFrame+numFrames > (m3->position-25) )
      numFrames = m3->position - iStartFrame - 25;
    dsyslog("checkMarkPair read frames around second mark from %d",iStartFrame);
    numPicsAfter = getPicInfo(picsafter, iStartFrame, numFrames, true, true, true );
  }

  // check if there is a part of the video repeated behind the advertising
  if( detectOverlaps )
  {
    done = doOverlapDetection(picsbefore, numPicsBefore, picsafter, numPicsAfter, m_org, marks);
  }

  if( !done && m2 != NULL )
  {
    if(hasBlackLines)
    {
      // needs the BlacklineCallback for checkBlacklineOffMark (0.4.2)
      setCB_Func(BlacklineCallback);
      if( m->position >= 0 )
      {
        int iOldmPosition = m->position;
        if(checkBlacklineOffMark( marks, m, cfn, OFFBACKFRAMES, iTopLines, iBottomLines ))
        {
          // if the marks has moved reread m and m2
          m = marks->GetPrev(iOldmPosition);
          *m_org = marks->Get(m->position);
          m2 = marks->GetNext(m->position);
        }
      }
      checkBlacklineOnmark(m2, marks, picsafter, numPicsAfter);
    }
    else
    {
      // needs the BlackframeCallback for checkBlackFrameOnMarks (0.4.2)
      setCB_Func(BlackframeCallback);
      checkBlackFrameOnMarks(m2, marks, picsafter, numPicsAfter);
    }
  }
  
  delete [] picsafter;
  delete [] picsbefore;
  setCB_Func(cbf_old);
}

void pass2a(cMarks *marks, cFileName *cfn)
{
  // pr�fen auf R�nder
  detectBlacklines(marks, cfn, iTopLines, iBottomLines);
  if( iTopLines < MINBLACKLINES || iBottomLines < MINBLACKLINES )
  {
    dsyslog( "not enough Blacklines for further inspection" );
    hasBlackLines = false;
  }
  else
    hasBlackLines = true;

  cMark *dummy = new cMark();
  dummy->position = -1;
  checkMarkPair(&dummy, marks, cfn);

  cMark *m = marks->GetNext(-1);
  if( m != NULL )
    m = marks->GetNext(m->position);
  while( m != NULL )
  {
    checkMarkPair(&m, marks, cfn);
    m = marks->GetNext(m->position);
    if( m != NULL )
      m = marks->GetNext(m->position);
  }

  listMarks( marks);

  int iDiff;
  int iLastPos = 0;
  char *cpos = NULL;
  char *cdiff = NULL;
  m = marks->GetNext(-1);
  cMark *mprev = NULL;
  while( m != NULL )
  {
    iDiff = m->position - iLastPos;
    asprintf(&cpos, "%s", (const char *)m->ToText(false));
    asprintf(&cdiff, "%s", (const char *)IndexToHMSF(iDiff, false));
    dsyslog( "Mark: %d %s #frames %d duration %s", m->position, cpos, iDiff, cdiff);
    iLastPos = m->position;
    if( iDiff > 0 && iDiff < MINMARKDURATION )
    {
      if( mprev != NULL )
      {
        dsyslog( "del Mark: %d ", mprev->position );
        marks->Del(mprev);
        marks->Del(m);
        m = marks->GetNext(-1);
        mprev = NULL;
        iLastPos = 0;
        bMarkChanged = true;
      }
    }
    else
    {
      mprev = m;
      m = marks->GetNext(m->position);
    }
  }

  listMarks(marks);
}

void calcSaving(cMarks * /*marks*/)
{
/*
  m = marks->GetNext(-1);
  cMark *mprev = NULL;
  while( m != NULL )
  {
    cIF->Get( index+ii, &FileNumber, &FileOffset, &PictureType, &Length);
    m = marks->GetNext(m->position);
  }
*/
}

#ifdef HAVE_LIBAVCODEC

void checkMarkByAudio(cMark **m_org, cMarks *marks, cFileName *cfn)
{
  cMark *m = *m_org;
  int iStartframe = m->position;
  int iEndFrame = iStartframe + AUDIO_CHECK_RANGE;
  int iCurrentFrame = iStartframe;
  int cframe = 0;
  uint_64 lastsilenceaudiopts = 0;
  int iLastSilenceFrame = 0;
  int numsilence = 0;
  bTSFoundPayload = false;
  int flags = DEMUX_RESET;
  
  resetAudioBuffer();
  cMark *mnext = marks->GetNext(m->position);
  if( mnext && iEndFrame > mnext->position )
      iEndFrame = mnext->position - 20;
  setCB_Func(simpleCallback);
  current_playaudiocbf = scan_audio_stream_0;
  havesilence = false;
  lowvalcount = 0;
  bTSFoundPayload = false;
  while(  (iCurrentFrame < cIF->Last() && iCurrentFrame <= iEndFrame) )
  {
    demuxFrame(cfn, cIF, iCurrentFrame++,bTSFoundPayload ? 0 : DEMUX_RESET );
    iCurrentDecodedFrame = iCurrentFrame-1;
    if( havesilence )
    {
      ignorevideo = false;
      current_playaudiocbf = NULL;
      cframe = findVideoByPTS(audiopts, iCurrentFrame);
      if(cframe)
      {
        dsyslog("(loop)found silence in frame %d, videoframe is %d, diff is %d",iCurrentDecodedFrame,cframe,iCurrentFrame-iLastSilenceFrame);
      }
      numsilence++;
      lastsilenceaudiopts = audiopts;
      iLastSilenceFrame = iCurrentFrame;
      havesilence = false;
      current_playaudiocbf = scan_audio_stream_0;
      ignorevideo = true;
    }
  }
  ignorevideo = false;
  current_playaudiocbf = NULL;
  dsyslog("mark at %d checked for audio-silence",m->position);
  dsyslog("num silence = %d",numsilence);
  dsyslog("last silence in frame %d",iLastSilenceFrame);
  if( numsilence > 3 )
  {
      cframe = cIF->GetNextIFrame( iLastSilenceFrame, true );
      if( cframe > 0 && cframe > m->position )
      {
        moveMark( marks, m, cframe, "moved from [%s] by audiosilence");
        *m_org = marks->Get(cframe);
      }
      /*
      cframe = findVideoByPTS(lastsilenceaudiopts, iLastSilenceFrame);
      if(cframe)
      {
        cframe = cIF->GetNextIFrame( cframe, true );
        if( cframe > 0 )
        {
          moveMark( marks, m, cframe, "moved from [%s] by audiosilence");
          *m_org = marks->Get(cframe);
        }
      }
      */
  }
}

void pass3(cMarks *marks, cFileName *cfn)
{
  dsyslog("start pass3 (audio-pass)");
  initAVCodec();
  cMark *m = marks->GetNext(-1);
  while( m != NULL )
  {
    checkMarkByAudio(&m, marks, cfn);
    m = marks->GetNext(m->position);
    if( m != NULL )
      m = marks->GetNext(m->position);
  }
  exitAVCodec();
  marks->Save();
  dsyslog("pass3 done");
}
#endif //HAVE_LIBAVCODEC
