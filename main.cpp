/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Son Okt 20 11:24:44 CEST 2002
    copyright            : (C) 2002/2004 by theNoad #709
    email                : theNoad@SoftHome.net
                           G7R0W9
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <execinfo.h>
#include "noad.h"
#include "svdrpc.h"
#include "tnoad.h"

const char *recDir = NULL;
bool bFork = false;

#define noMARK_CHECK
#define INITIAL_WAIT_TIME 60
void doNoad(bool isLive, const char *fname)
{
  time_t              start;
  time_t              end;
  int iNumFrames = -1;
  int iNewNumFrames = -1;
  #ifdef MARK_CHECK
    start = time(NULL);
    syslog(LOG_INFO, "%s start noad-%s for %s ", myTime(start), getVersion(), fname);
    fprintf(stderr,"%s start noad-%s for %s\n", myTime(start), getVersion(), fname);
    noadData *pdata = new noadData();
    pdata->initBuffer();
    checkMarksOnIFrames(pdata, fname);
    end = time(NULL);
    fprintf(stderr,"%s noad done for %s (%ld secs)\n", myTime(end), fname,end-start);
    syslog(LOG_INFO, "%s noad done for %s (%ld secs)", myTime(end), fname,end-start);
    delete pdata;
  return;
  #endif
  noadData *pdata = new noadData();
  pdata->initBuffer();
  if( isLive )
  {
    start = time(NULL);
    syslog(LOG_INFO, "%s start noad-%s online for %s ", myTime(start), getVersion(), fname);
    fprintf(stderr,"%s start noad-%s online for %s\n", myTime(start), getVersion(), fname);
    //syslog(LOG_INFO, "noad online wait %d secs for index-file ", INITIAL_WAIT_TIME);
    //sleep(INITIAL_WAIT_TIME);
    doOnlineScan(pdata, fname);
    end = time(NULL);
    fprintf(stderr,"%s noad online done for %s (%s)\n", myTime(end), fname,secToTime(end-start));
    syslog(LOG_INFO, "%s noad online done for %s (%s)", myTime(end), fname,secToTime(end-start));
  }
  else
  {
    start = time(NULL);
    syslog(LOG_INFO, "%s start noad-%s for %s ", myTime(start), getVersion(), fname);
    fprintf(stderr,"%s start noad-%s for %s\n", myTime(start), getVersion(), fname);
    iNumFrames = iNewNumFrames;
    cctrl = NULL;
    iNewNumFrames = doX11Scan(pdata, fname, iNumFrames);
    //if( cctrl != NULL )
    //  delete cctrl;
    //cctrl = NULL;
    end = time(NULL);
    fprintf(stderr,"%s noad done for %s (%s)\n", myTime(end), fname,secToTime(end-start));
    syslog(LOG_INFO, "%s noad done for %s (%s)", myTime(end), fname,secToTime(end-start));
  }
  delete pdata;
}

void writeStatistic(const char *statfilename, const char *recording)
{
  FILE *f = fopen(statfilename, "a+");
  if (!f)
  {
    LOG_ERROR_STR(statfilename);
    return;
  }
  time_t start = time(NULL);
  fprintf(f,"%s;%s;%s,%s,%s;%dx%d;%d (%s);%d;%d;%d;%d;%d;%d;%s;%s\n",
//  fprintf(f,"%s;%s;%s;%dx%d;%d (%s);%d;%d;%d;%d;%d;%d;%s;%s\n",
      myTime(start),
      getVersion(),
      hasBlackLines?"yes":"no",
      hasAC3?"yes":"no",
      hasOverlaps?"yes":"no",
      picWidth,picHeight,
      totalFrames,
      (const char *)IndexToHMSF(totalFrames,false),
      totalDecodedFrames,
      decodedFramesForLogoDetection,
      secsForLogoDetection,
      decodedFramesForLogoCheck,
      secsForLogoCheck,
      secsForScan,
      recording,
      "add your comment here");
  fclose(f);
}

static int close_files(void)
{
  struct rlimit file_limit;
  int file_max;

  if (getrlimit(RLIMIT_NOFILE, &file_limit) == -1)
  {
    perror("getrlimit");
    return !0;
  }
  file_max = (int)file_limit.rlim_cur;

  for (int f = 0; f < file_max; f++)
  {
    if (f != fileno(stdin) && f != fileno(stdout) && f != fileno(stderr))
    {
      /* f is probably not open, so ignore errors */
      (void)close(f);
    }
  }
  errno = 0;
  return 0;
}

#ifndef HAVE_STRSIGNAL
static char signumbuf[10];
char *strsignal(int sig)
{
  sprintf(signumbuf,"%d",sig);
  return signumbuf;
}
#endif

static void signal_handler(int sig)
{
  if (!bFork)
    fprintf(stderr, "Aborted by signal %s...\n", strsignal(sig));
  rm_pidfile(recDir);
  syslog(LOG_INFO, "noad aborted by signal %s",strsignal(sig));
  show_stackframe(bFork);
  exit(EXIT_FAILURE);
}

void cleanUp(void)
{
  rm_pidfile(recDir);
}

int main(int argc, char *argv[], char *envp[])
{
  openlog("noad", LOG_PID | LOG_CONS, LOG_USER);
  //
  for(int i = 0; i < argc; i++)
    syslog(LOG_INFO, "noad arg[%d]: %s", i, argv[i]);
  syslog(LOG_INFO, "noad args done");

  //syslog(LOG_INFO, "noad pid: %d", getpid());

  int c;
  bool bImmediateCall = false;
  bool bAfter = false;
  bool bBefore = false;
  bool bEdited = false;
  bool bNice = false;
  bool bOnline = false;
  int onlinemode = 1;
  bool bNoPid = false;
  int verbosity = 1;
  int niceLevel = 19; // changed to 19 (0.4.2)
  const char *logoCacheDir = NULL;
  const char *statFile = NULL;

  while (1)
  {
    int option_index = 0;
    static struct option long_options[] =
    {
      {"statisticfile", 1, 0, 's'},
      {"logocachedir", 1, 0, 'l'},
      {"verbose", 0, 0, 'v'},
      {"background", 0, 0, 'b'},
      {"priority",1,0,'p'},
      {"comments", 0, 0, 'c'},
      {"jumplogo",0,0,'j'},
      {"overlap",0,0,'o' },
      {"ac3",0,0,'a' },
      {"OSD",0,0,'O' },
      {"savelogo", 0, 0, 'S'},
      {"backupmarks", 0, 0, 'B'},
      {"scenechangedetection", 0, 0, 'C'},
      {"version", 0, 0, 'V'},
      {"nelonen",0,0,'n'},
      {"markfile",1,0,1},
      {"loglevel",1,0,2},
      {"testmode",0,0,3},
      {"online",2,0,4},
      {"nopid",0,0,5},
      {"asd",0,0,6},
      {"pass3only",0,0,7},
		{"svdrphost",1,0,8},
      {"svdrpport",1,0,9},      
      {0, 0, 0, 0}
    };

    c = getopt_long  (argc, argv, "s:l:vbp:cjoaOSBCV",
      long_options, &option_index);
    if (c == -1)
    break;

    switch (c)
    {
      case 's':
        statFile = optarg;
      break;

      case 'l':
        logoCacheDir = optarg;
      break;

      case 'v':
        //verbosity++;
        verbosity = verbosity <<1 | 1;
      break;

      case 'b':
        bFork = true;
      break;

      case 'p':
        if (isnumber(optarg) || *optarg=='-')
          niceLevel = atoi(optarg);
        else
        {
          fprintf(stderr, "noad: invalid priority level: %s\n", optarg);
          return 2;
        }
        bNice = true;
      break;

      case 'c':
        markComments = true;
      break;

      case 'j':
        logoJumpDetection = true;
      break;

      case 'o':
        detectOverlaps = true;
      break;

      case 'a':
        useAudioDetection = true;
      break;

      case 'O':
        osdMsg = 1;
      break;

      case 'S':
        saveLogo = true;
      break;

      case 'B':
        backupMarks = true;
      break;

      case 'C':
        useSceneChangeDetection = true;
      break;

      case 'V':
        printf("noad %s - no advertising\n",getVersion());
        return 0;

      case 'n':
        isNelonen = true;
      break;

      case '?':
        printf("unknow option ?\n");
      break;

      case 0:
        printf ("option %s", long_options[option_index].name);
        if (optarg)
            printf (" with arg %s", optarg);
        printf ("\n");
        break;

      case 1:
        /*
        printf ("option %d %s", option_index, long_options[option_index].name);
        if (optarg)
            printf (" with arg %s", optarg);
        printf ("\n");
        */
        setMarkfileName(optarg);
        break;

      case 2:
        if (isnumber(optarg))
          verbosity = atoi(optarg);
      break; 
      
      case 3:
        bTestMode = true;
      break;
      
      case 4:
        bOnline = true;
        if( optarg )
        {
          if (isnumber(optarg))
            onlinemode = atoi(optarg);
          if( onlinemode != 1 && onlinemode != 2 )
            onlinemode = 1; 
        }
      break;
      
      case 5:
        bNoPid = true;
      break;
      
      case 6:
      #ifdef HAVE_LIBAVCODEC
        doPass3 = true;
      #else
        fprintf(stderr,"--asd given, but not compiled with ffmpeg-support\n--asd will be ignored\n");
        esyslog("--asd given, but not compiled with ffmpeg-support");
        esyslog("--asd will be ignored");
      #endif
      break;
      
      case 7:
      #ifdef HAVE_LIBAVCODEC
        pass3only = true;
      #else
        fprintf(stderr,"--pass3only given, but not compiled with ffmpeg-support\n--pass3only will be ignored\n");
        esyslog("--pass3only given, but not compiled with ffmpeg-support");
        esyslog("--pass3only will be ignored");
      #endif
      break;
      
      case 8:
          SVDRPHost = optarg;
      break;

      case 9:
        if (isnumber(optarg) && atoi(optarg) > 0 && atoi(optarg) < 65536 )
          SVDRPPort = atoi(optarg);
        else
        {
          fprintf(stderr, "noad: invalid SVDRP Port: %s\n", optarg);
          return 2;
        }
      break;
      default:
        printf ("? getopt returned character code 0%o ?(option_index %d)\n", c,option_index);
    }
  }

  if (optind < argc)
  {
    while (optind < argc)
    {
      if(strcmp(argv[optind], "after" ) == 0 )
      {
        bAfter = bFork = bNice = true;
      }
      else if(strcmp(argv[optind], "before" ) == 0 )
      {
        bBefore = bFork = bNice = true;
      }
      else if(strcmp(argv[optind], "edited" ) == 0 )
      {
        bEdited = true;
      }
      else if(strcmp(argv[optind], "nice" ) == 0 )
      {
        bNice = true;
      }
      else if(strcmp(argv[optind], "-" ) == 0 )
      {
        bImmediateCall = true;
      }
      else
      {
        if( strstr(argv[optind],".rec") != NULL )
          recDir = argv[optind];
      }
      optind++;
    }
  }

  // set the log-Level
  SysLogLevel = verbosity;

  // we can run, if one of bImmediateCall, bAfter, bBefore or bNice is true
  // and recDir is given
  if( (bImmediateCall || bAfter || bBefore || bEdited || bNice) && recDir )
  {
    // do nothing if called from vdr after the video is cutted
    if( bEdited )
      return 0;

    // if online is set to 1, check that the rec is a live-rec
    // else switch online-mode off
    if( onlinemode == 1 && bBefore )
    {
      if( !strchr(recDir,'@') )
        bOnline = false;
    }
    // do nothing if called from vdr before the recording has startet
    // and online is not set
    if( bBefore && !bOnline )
    {
      syslog(LOG_INFO,"noad called with 'before' and online=%d and liverecording is %s",onlinemode,(strchr(recDir,'@')?"yes":"no"));
      syslog(LOG_INFO,"nothing to do yet");
      return 0;
    }

    // if bFork is given go in background
    if( bFork )
    {
      (void)umask((mode_t)0011);
      close_files();
      pid_t pid = fork();
      if (pid < 0)
      {
        fprintf(stderr, "%m\n");
        esyslog("fork ERROR: %m");
        return 2;
      }
      if (pid != 0)
      {
        syslog(LOG_INFO,"noad forked to pid %d",pid);
        return 0; // initial program immediately returns
      }
    }
    if(bBefore)
    {
      syslog(LOG_INFO,"wait %d secs for vdr creating directory",INITIAL_WAIT_TIME);
      sleep(INITIAL_WAIT_TIME);
    }
    if( !bNoPid )
    {
      int i = 0;
      bool bHavePid = false;
      while( !bHavePid )
      {
        int pidError = make_pidfile(recDir);
        if( pidError == -1 )
        {
          pid_t olPid = processInfo(recDir);
          if( bAfter && bOnline )
          {
            i++;
            if( i > 3 )
            {
              syslog(LOG_INFO,"online-noad didn't stop, give up");
              exit(-1);
            }
            if( i > 2 )
            {
              int killerr = kill(olPid,SIGUSR1);
              syslog(LOG_INFO,"online-noad didn't stop, try to kill it(result:%d)",killerr);
            }
            syslog(LOG_INFO,"wait 120 secs for stop of online-noad");
            sleep(120);
          }
          else
            exit(-1);
        }
        else if( pidError == -2 )
        {
          exit(-1);
        }
        else if( pidError == 0 )
          bHavePid = true;
      }
    }
    if( bFork )
    {
      //syslog(LOG_INFO, "noad (forked) pid: %d", getpid());
      chdir("/");
      if (setsid() == (pid_t)(-1))
      {
          perror("setsid");
          exit(EXIT_FAILURE);
      }
      if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
      {
          perror("signal(SIGHUP, SIG_IGN)");
          errno = 0;
      }
      int f;
      
      f = open("/dev/null", O_RDONLY);
      if (f == -1)
      {
          perror("/dev/null");
          errno = 0;
      }
      else
      {
          if (dup2(f, fileno(stdin)) == -1)
          {
              perror("dup2");
              errno = 0;
          }
          (void)close(f);
      }
      
      f = open("/dev/null", O_WRONLY);
      if (f == -1)
      {
          perror("/dev/null");
          errno = 0;
      }
      else
      {
          if (dup2(f, fileno(stdout)) == -1)
          {
              perror("dup2");
              errno = 0;
          }
          if (dup2(f, fileno(stderr)) == -1)
          {
              perror("dup2");
              errno = 0;
          }
          (void)close(f);
      }
    }


    int MaxPossibleFileDescriptors = getdtablesize();
    for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
        close(i); //close all dup'ed filedescriptors


    // should we renice ?
    if( bNice )
    {
      int niceErr = nice(niceLevel);
      int oldErrno = errno;
      if( errno == EPERM || errno == EACCES )
      {
        esyslog("ERROR: nice %d: no super-user rights",niceLevel);
        errno = oldErrno;
        fprintf(stderr, "nice %d: no super-user rights\n",niceLevel);
      }
      else if( niceErr != niceLevel )
      {
        esyslog("nice ERROR(%d,%d): %m",niceLevel,niceErr);
        errno = oldErrno;
        fprintf(stderr, "%d %d %m\n",niceErr,errno);
      }
    }

    // catch some signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGUSR1, signal_handler);
    
    // do cleanup at exit...
    atexit(cleanUp);
    
    // now do the work..,
    doNoad(bBefore, recDir);

    // write statistic
    if( statFile != NULL )
       writeStatistic(statFile,recDir);

    if( !bNoPid )
      rm_pidfile(recDir);
    return 0;
  }

  // nothing done, give the user some help
  printf("Usage: noad [options] cmd <record>\n"
         "options:\n"
         "-a,             --ac3\n"
         "                  use ac3-detection\n"
         "-b,             --background\n"
         "                  noad runs as a background-process\n"
         "                  this will be automatic set if called with \"after\" or \"before\"\n"
         "-c,             --comments\n"
         "                  add comments to the marks\n"
         "-j,             --jumplogo\n"
         "                  detect jumping logos\n"
         "-n              --nelonen\n"
         "                  special behavior for finish stations\n"
         "-o,             --overlap\n"
         "                  detect overlaps\n"
         "-p,             --priority\n"
         "                  priority-level of noad when running in background\n"
         "                  [19...-19] default 19\n"
         "-s <filename>,  --statisticfile=<file>\n"
         "                  filename where some statistic datas are stored\n"
         "-v,             --verbose\n"
         "                  increments loglevel by one, can be given multiple\n"
         "-B              --backupmarks\n"
         "                  move the marks.vdr to marks0.vdr\n"
         "-O,             --OSD\n"
         "                  noad sends an OSD-Message for start and end \n"
         "                  (default: to localhost:2001)\n"         
         "-S              --savelogo\n"
         "                  save the detected logo\n"
         "-V              --version\n"
         "                  print version-info and exits\n"
         "--svdrphost=<ip-address>\n"
         "                  set the IP-address used for OSD Messages\n"
         "                  (default: localhost)\n"
         "--svdrpport=<tcp-port>\n"
         "                  set the TCP-Port used for OSD Messages\n"
         "                  (default: 2001)\n"
		   "--markfile=<markfilename>\n"
         "  set a different markfile-name\n"
         "--online[=1|2] (default is 1)\n"
         "  start noad immediately when called with \"before\" as cmd\n"
         "  if online is 1, noad starts online for live-recordings\n"
         "  only, online=2 starts noad online for every recording\n"
         "  live-recordings are identified by having a '@' in the filename\n"
         "  so the entry 'Mark instant recording' in the menu 'Setup - Recording'\n"
         "  of the vdr should be set to 'yes'\n"
         "--asd\n"
         "  use audio silence detection for mark-refinement\n"
         "  you need to have noad configured with \"--with-ffmpeg\"\n"
         "  to use this parameter\n"
         "--pass3only\n"
         "  this is a parameter for testing only and you need\n"
         "  to give \"--asd\" also to let this work\n"
         "  if given, only the third pass is done, which\n"
         "  is the pass with audio silence detection\n"
         "  this parameter is only usefull if there are already\n"
         "  some marks in the \"marks.vdr\" for this recording\n"
         //"-C              --scenechangedetection\n"
         //"                  use scene-change-detection\n"
         "\ncmd: one of\n"
         "-                            dummy-parameter if called directly\n"
         "after                        from vdr if used in the -r option of vdr\n"
         "before                       from vdr if used in the -r option of vdr\n"
         "                             noad exits immediately if called with \"before\"\n"
         "                             and --online is not given\n"
         "edited                       from vdr if used in the -r option of vdr\n"
         "                             noad exits immediately if called with \"edited\"\n"
         "nice                         runs noad with nice(19)\n"
         "\n<record>                     is the name of the directory where the recording\n"
         "                             is stored\n\n"
         );
	return 0;
}
