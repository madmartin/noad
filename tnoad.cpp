/***************************************************************************
 *   Copyright (C) 2004 by the Noad                                        *
 *   theNoad@ulmail.net                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "tnoad.h"
#include "noad.h"
//#include "mpeg2wrap.h"
#include "svdrpc.h"

extern int SysLogLevel;
extern noadData *ndata;
extern char *filename;
extern cNoadIndexFile *cIF;
extern cFileName *cfn;

extern int iLastIFrame;
extern int iIgnoreFrames;
extern unsigned long ulTopBlackLines;
extern unsigned long ulBotBlackLines;
extern int checkedFrames;

extern uchar readBuffer[];    // frame-buffer
extern int curIndex;          // current index
extern uint16_t FileNumber;   // current file-number
extern off_t FileOffset;      // current file-offset
extern bool Independent;      // current picture-type
extern int Length;            // frame-lenght of current frame
extern uint8_t * end;         // pointer to frame-end
extern bool bMarkChanged;
#ifdef VNOAD
extern bool volatile bStop;
bool bTestMode = true;
#else
bool bStop = false;
bool bTestMode = false;
#endif

#define SLEEPTIME 15
#define NEWFRAMES (SLEEPTIME*framespersec)
#define MINFRAMESTOSTART 3000
#define MINFORWARDFRAMES 1000
#define LOGORESETFRAMES (FRAMESPERMIN*12)
void scanLoop(cMarks *marks)
{
   int lastFrameCount = 0;
   int logoDetectStart = 0;
   int ac3DetectStart = 0;
   bool scanDone = false;
   bool haveLogo = false;
   bool haveAC3 = false;
   int iLastLogoPos = 0;
   int oldac3mode = 0;
   int lastMarkCount = 0;
   int marksChecked = 0;
   ac3mode = 0;

   int iCurrentFrame, iLastStartFrame, iStopFrame = 0;
   //int iRestartFrame = 0;
   int iState = 0;
   int iOldState = -1;
   iCurrentFrame = 0;
   iStopFrame = 0;

#ifdef USE_FFMPEG
    if( default_Decoder == FFMPEG_DECODER )
       decoder = new FFMPegDecoder();
    else
#endif
#ifdef USE_LIBMPGE2
       if( default_Decoder == LIBMPEG2_DECODER )
         decoder = new LibMPeg2Decoder();
#endif

   decoder->decoder_init();
   decoder->openFile(cfn,cIF);

   if( bTestMode )
   {
      cIF->setInterval(NEWFRAMES);
      while(cIF->Last() < MINFRAMESTOSTART)
         cIF->CatchUp();
   }
   xsyslog("doOnlineScan: start scanLoop with %d frames", cIF->Last());
   marks->ClearList();
   bMarkChanged = true;
   if(cIF->Last() < MINFRAMESTOSTART)
   {
      int secs = (MINFRAMESTOSTART - cIF->Last()) / framespersec;
      xsyslog("scanLoop: go sleep for %d seconds",secs);
      sleep(secs);
   }
   do
   {
      // wait for new frames recorded
      if( bTestMode )
         cIF->CatchUp();
      if(cIF->Last()-lastFrameCount < NEWFRAMES)
      {
         xsyslog("scanLoop: go sleep for %d seconds",SLEEPTIME);
         // sllep requested time - 2secs
         sleep(SLEEPTIME-1);
         // catchUp index-data
         cIF->CatchUp();
         //sleep again 5 secs (vdr writes data after index!)
         sleep(5);
         if( lastFrameCount == cIF->Last() )
            scanDone = true;
         lastFrameCount = cIF->Last();
      }
#ifdef VNOAD
      lastFrameCount += NEWFRAMES;
      if( lastFrameCount > cIF->Last() )
         lastFrameCount =  cIF->Last();
#else
      lastFrameCount =  cIF->Last();
#endif

      xsyslog("scanLoop: lastFrameCount %d logoDetectStart %d haveLogo %d haveAC3 %d iCurrentFrame %d",lastFrameCount, logoDetectStart,haveLogo, haveAC3, iCurrentFrame);
      // do logo-detection if necessary
      if( !haveLogo && !haveAC3 )
      {
         //xsyslog("scanLoop: ac3detection ac3DetectStart %d",ac3DetectStart);
         if(useAudioDetection)
            haveAC3 = decoder->detect_ac3_51(ac3DetectStart);
         ac3DetectStart = cIF->Last();
         //xsyslog("scanLoop: ac3detection done, haveAC3= %d ac3DetectStart=%d",haveAC3,ac3DetectStart);
         if( !haveAC3 )
         {
            if( lastFrameCount - logoDetectStart >= FRAMES_TO_CHECK )
            {
               //---demux_reset();
               reInitNoad( 0,0 );
               xsyslog("scanLoop: start logodetection at frame %d",logoDetectStart);
               dodumpts=false;
               if( doLogoDetection( logoDetectStart ) )
               {
                  //check logo from iLastIFrame for a least 100 frames!!
                  if( !checkLogoState( 1, iLastIFrame, 0, 3000) )
                  {
                     haveLogo = true;
                     // scan should start at lst Logo-position
                     // but avoid looping by going back more than LOGORESETFRAMES frames
                     //if( (iCurrentFrame-iLastIFrame) < LOGORESETFRAMES )
                     if( (iLastIFrame-iLastLogoPos) < LOGORESETFRAMES )
                        iCurrentFrame = iLastLogoPos;
                  }
                  else
                     logoDetectStart += FRAMES_TO_CHECK;
               }  
               else
                  logoDetectStart += FRAMES_TO_CHECK;
               dodumpts=false;
               xsyslog("scanLoop: haveLogo %d",haveLogo);
            }
         }
      }
      if( haveAC3 )
      {
         ac3mode = oldac3mode;
         int iLastLogoFrame = 0;
         int cframe = 0;
         iCurrentDecodedFrame = 0;
         //extern int iCurrentDecodedAudioFrame;
         //iCurrentDecodedAudioFrame = 0;
         checkedFrames = 0;
         //current_audiocbf = audiocallback;
         noadCallback oldCallback = decoder->getCallback();
         decoder->setCallback(simpleCallback);
         xsyslog("scan audio from %d to %d",iCurrentFrame,cIF->Last());
         while(  iCurrentFrame < cIF->Last() )
         {
            int flags = DEMUX_RESET;
            decoder->ac3Decoded = false;
            while(  iCurrentFrame < cIF->Last() && !decoder->ac3Decoded )
            {
               iCurrentDecodedFrame = iCurrentFrame;
               if( decoder->getPositionCB_Func() )
                  (decoder->getPositionCB_Func())(iCurrentFrame);
               decoder->demuxFrame_audio(iCurrentFrame, flags );
               flags = 0;
               if( !decoder->ac3Decoded )
                  iCurrentFrame++;
            }
            dsyslog( "Audio: oldac3mode = %d ac3mode = %d at %d", oldac3mode, ac3mode,iCurrentFrame);
            //iCurrentDecodedAudioFrame = 0;
            /*
            while(  iCurrentFrame < cIF->Last() && iCurrentDecodedAudioFrame == 0 )
            {
               cIF->Get(iCurrentFrame, &FileNumber, &FileOffset, &Independent, &Length);
               cfn->SetOffset( FileNumber, FileOffset);
               end = readBuffer + ReadFrame (cfn->File(), readBuffer, Length, MAXFRAMESIZE);
               iCurrentDecodedFrame = iCurrentFrame;
               if( cfn->isPES() )
                  demuxPES (readBuffer, end, 0);
               else
                  demuxTS (readBuffer, end, 0);
               if( !iCurrentDecodedAudioFrame )
                  iCurrentFrame++;
            }
            */
            if( oldac3mode != ac3mode && ((oldac3mode==51 && ac3mode !=51 )||(oldac3mode!=51 && ac3mode==51)) )
            {
               int ac3mode_help = ac3mode;
               int iStart = iCurrentFrame - AC3_51DETECT_SKIP;
               if( iStart < 0 )
                  iStart = 0;
               if( (iCurrentFrame - iStart) > 5 )
                  cframe = decoder->find_ac3start( iStart, iCurrentFrame, ac3mode);
               if( oldac3mode == 51 || oldac3mode == 0 ||ac3mode == 51 )
               {
                  cframe = decoder->findVideoByPTS(ac3pts, cframe);
                  int iNextIframe = cIF->GetNextIFrame( cframe, ac3mode_help == 51, &FileNumber, &FileOffset, &Length, true);
                  if( iNextIframe > 0 )
                  {
                     iLastLogoFrame = iNextIframe;
                     xsyslog("AudioChanged: iLastLogoFrame=%6d, audio changed from %d to %d ", iLastLogoFrame, oldac3mode,ac3mode_help);
                     MarkToggle(marks, iLastLogoFrame, ac3mode_help != 51 ? "ac3 lost" : "ac3 start");
                     bMarkChanged = true;
                  }
                  else
                  {
                     // there is no iframe in the future
                     // reset states and let the scan continue
                     iCurrentFrame = iLastLogoFrame;
                     break;
                  }
               }
               oldac3mode = ac3mode = ac3mode_help;
            }
            if(ac3mode==51)
               iLastLogoPos = iCurrentFrame;
            iCurrentFrame += AC3_51DETECT_SKIP;
         }
         decoder->setCallback(oldCallback);
         //current_audiocbf = NULL;
      }
      else if( haveLogo )
      {
         xsyslog("scanLoopx: iCurrentFrame %d cIF->Last() %d iState  %d iLastLogoPos %d",iCurrentFrame,cIF->Last(),iState,iLastLogoPos);
         while(  iCurrentFrame < cIF->Last() && iCurrentFrame >= 0 && iState != -1 )
         {
            iOldState = iState;
            iLastStartFrame = iCurrentFrame;
            xsyslog("findLogoChange with iState=%d oldstate=%d currentFrame=%d", iState, iOldState, iCurrentFrame);
            iState = findLogoChange(iState, iCurrentFrame, BIGSTEP );
            xsyslog("StateChanged: newstate=%d oldstate=%d currentFrame=%d", iState, iOldState, iCurrentFrame);
            if( iState < 0 )
            {
               iState = iOldState;
               break;
            }
            if( iState != iOldState && iState != -1 && iState != -2 && iOldState >= 0 )
            {
               iCurrentFrame -= (BIGSTEP*2);
               if( iCurrentFrame < iStopFrame )
                  iCurrentFrame += (BIGSTEP/2);
               if( iCurrentFrame < iStopFrame )
                  iCurrentFrame = iStopFrame;
               int logostabletime = LOGOSTABLETIME;
               if( !iState )
               {
                  if( isNelonen )
                     logostabletime = LOGOSTABLETIME/2;
               }
               else
                  logostabletime = 0;
               if( iCurrentFrame + MINFORWARDFRAMES > cIF->Last() )
               {
                  iState = iOldState;
                  break;
               }
               iState = findLogoChange(iOldState, iCurrentFrame, SMALLSTEP, logostabletime );
               if( iState >= 0 && iState != iOldState )
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
                  MarkToggle(marks, iLastLogoFrame, iState == 0 ? "Logo lost" : "Logo start");
                  if( iState == 1 )
                     iLastLogoPos = iCurrentFrame;
                  if( iState == 0 )
                     iLastLogoPos = iLastLogoFrame-1;
                  if( iLastLogoPos < 0 ) 
                     iLastLogoPos = 0;

                  iStopFrame = iLastLogoFrame;
                  xsyslog("toggle mark at %d",iLastLogoFrame);
               }
            }
         }
         if( iCurrentFrame < 0 )
            iCurrentFrame = cIF->Last();
         xsyslog("nach detect iState %d",iState);
         if( iState == 1 )
            iLastLogoPos = iCurrentFrame;
      }
      if( marks->Count() != lastMarkCount )
      {
         xsyslog("scanloop: %d new marks",marks->Count()-lastMarkCount);
         lastMarkCount = marks->Count();
         int oldloglevel = SysLogLevel;
         SysLogLevel=3;
         listMarks(marks);
         int n = marks->Count();
         cleanInactiveMarks(marks);
         cleanActiveMarks(marks);
         if ( marks->Count() != n )
            marks->Save();
         listMarks(marks);
         SysLogLevel = oldloglevel;
         lastMarkCount = marks->Count();
         if( marksChecked != lastMarkCount )
         {
            xsyslog("scanloop: %d marks to check",lastMarkCount-marksChecked);
            marksChecked = lastMarkCount;
         }
      }
      if( detectOverlaps && marks->hasUncheckedMarks() )
      {
         cMark *m = NULL;
         m = marks->GetLast();
         if( !m->isChecked() && (!(m->Index() & 1)) &&  ((cIF->Last() - m->position ) > PICS_READ_AFTER) )
         {
            if( m->Prev() == NULL )
            {
               cMark *dummy = new cMark();
               dummy->position = -1;
               checkMarkPair(&dummy, marks);
               m->setChecked(true);
            }
            else
            {
               m = (cMark *)m->Prev();
               checkMarkPair(&m, marks);
               m->setChecked(true);
               m = (cMark *)m->Next();
               m->setChecked(true);
            }
         }
      }
      if( iState < 0 && iOldState != 0 ) 
         iState = iOldState; // not good here ?
      if( (haveLogo||haveAC3) && cIF->Last() - iLastLogoPos > LOGORESETFRAMES )
      {
         haveLogo = haveAC3 = false;
         logoDetectStart = ac3DetectStart = iLastLogoPos+15;
         xsyslog("scanloop: set haveLogo/haveAC3 to false cause timeout, iLastLogoPos=%d",iLastLogoPos);
      }
   } while( !scanDone && !bStop );
   if( bTestMode )
      cIF->setInterval(0);
   xsyslog("doOnlineScan: end scanLoop");
}

int doOnlineScan(noadData *thedata, const char *fName, cMarks *_marks )
{
  xsyslog("doOnlineScan %s", fName);
  cMarks *pmarks = NULL;
  cMarks *localMarks = NULL;
  isOnlinescan=true;

  #ifdef VNOAD
  if( make_pidfile(fName) < 0 )
  {
    xsyslog("doOnlineScan: PidFile-error, delete PidFile and try again");
    rm_pidfile(fName);
    if( make_pidfile(fName) < 0 )
    {
      xsyslog("doOnlineScan: PidFile-error");
      return(-1);
    }
  }
  #endif
  filename = (char *)fName;
  ndata = thedata;
  #ifndef VNOAD
  demux_track = 0xe0;
  #ifndef FFMPEGDECODING
  mpeg2dec = mpeg2_init();
  #endif

  #endif
  
  if(osdMsg)
  {
    noadStartMessage(filename);
  }

  clearStats();
  #ifndef VNOAD
  // open the cIndexFile for the record
  bool isPES = isPESRecording(filename);
  setMarkfileSuffix(isPES);
  cIF = new cNoadIndexFile(filename,false, isPES);
  if( cIF == NULL )
    return -1;
  #endif
  totalFrames = cIF->Last();
  #ifndef VNOAD
  // open the record
  cfn = new cFileName(filename, false, false, isPES );
  if( cfn->Open() < 0 )
  {
    delete cfn;
    delete cIF;
    isOnlinescan=false;
    return -1;
  }
  cRecordingInfo recInfo(filename);
  if( recInfo.Read() )
     framespersec = recInfo.FramesPerSecond();
  else
  {
      esyslog("can't read Info-File for %s", filename);
      esyslog("assume %f fps", framespersec);
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
  {
    pmarks = localMarks = new cMarks();
    pmarks->Load(filename,framespersec,cfn->isPES());
  }  
  scanLoop(pmarks);
  
  //cLogoDetectionThread *detThread = new cLogoDetectionThread(thedata, fName);
  
  #ifndef VNOAD
  #ifndef FFMPEGDECODING
  mpeg2_close (mpeg2dec);
  #else
  decoder->decoder_exit();
  #endif
  #endif
  
  #ifndef VNOAD
  delete cfn;
  delete cIF;
  #endif
  delete localMarks;
  if(osdMsg)
  {
    noadEndMessage(filename);
  }
  isOnlinescan=false;
  #ifdef VNOAD
  rm_pidfile(fName);
  #endif
  return 0;
}
