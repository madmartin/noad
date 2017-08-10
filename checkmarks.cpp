
#include "vdr_cl.h"
int SysLogLevel = 3;
bool logcon=false;

int marksInfo(const char *recDir, int Info)
{
   if( Info )
     fprintf(stdout, "check marks in %s\n", recDir);
   int iRet = 0;
   bool bIsPes = isPESRecording(recDir);
   double framesPerSec = DEFAULTFRAMESPERSECOND;
   if( !bIsPes)
   {
      cRecordingInfo cri(recDir);
      if( cri.Read() )
         framesPerSec = cri.FramesPerSecond();
      else
         fprintf(stdout,"can't read info-File in %s, assume 25.0 FPS\n",recDir);
   }
   cMarks marks;
   setMarkfileSuffix(bIsPes);
   if( !marks.Load(recDir,framesPerSec,bIsPes) )
   {
     if( Info )
	fprintf(stdout, "no marks for %s\n", recDir);
     return 0;
   }
   cIndexFile cif(recDir,false,bIsPes);
   cMark *m = marks.First();
   while( m )
   {
      uint16_t FileNumber = 0;
      off_t FileOffset = 0;
      bool Independent = false;
      int Length = 0;
      if( !cif.Get(m->position, &FileNumber, &FileOffset, &Independent, &Length) )
      {
	if( Info )
	   fprintf(stdout, "can't get index for mark, position is %d\n",m->position);
      }
      if( !Independent )
         iRet = 1;
      //qint64 rpos = 0;
      //int i = 1;
      //while( i < FileNumber )
      //{
      //   rpos += l[i-1].size();
      //   i++;
      //}
      //rpos += FileOffset; 
      //qDebug() << m->Index() << " " << m->position << " " << FileNumber << " " << FileOffset << "  ==> " << rpos;
      if(Info>1)
      {
         fprintf(stdout, "%s\n", (const char *)(m->ToText(false,true)));
         fprintf(stdout, "   File  : %u\n", FileNumber);
         fprintf(stdout, "   Offset: %ld\n", FileOffset);
         fprintf(stdout, "   Index#: %d\n", m->position);
         fprintf(stdout, "   Independed Frame: %s\n", (Independent?"yes":"no"));;
      }
      m = marks.Next(m);
   }
   if( Info )
   {
      if( iRet == 1 )
         fprintf(stdout, "marks in %s are faulty\n", recDir);
      else
         fprintf(stdout, "marks in %s are ok\n", recDir);
   }
   return iRet;
}


void usage()
{
  printf("Usage: checkMarks <record> [0|1|2]\n"
         "  0 (default) - no infos print to stdout\n"                       
	 "  1 show info about marks-state (ok or faulty)\n"
	 "  2 show detailed info for each mark found\n"
	 " returns 0 if marks are ok\n"
	 " returns 1 if marks are faulty\n"
	);
}

int main(int argc, char *argv[])
{
   openlog("checkMarks", LOG_PID | LOG_CONS, LOG_USER);
   if( argc < 2 )
   {
      usage();
      return 0;
   }
#ifdef _MSC_VER
	_set_fmode( O_BINARY ); 
#endif
   char *cp = argv[1];
   if( cp[strlen(cp)-1] == '/' )
      cp[strlen(cp)-1] = '\0';
   if( !isRecording(argv[1]) )
   {
      fprintf(stdout, "%s is not a vdr-dir\n", argv[1]);
      return 2;
   }
   int i = 0;
   if( argc > 2 )
     i = atoi(argv[2]);
   return marksInfo(argv[1], i);
}
