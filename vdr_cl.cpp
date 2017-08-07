#include "vdr_cl.h"
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include "config.h"

#define RECEXT       ".rec"
#define DELEXT       ".del"
// start of implementation for brain dead systems
#define DATAFORMAT   "%4d-%02d-%02d.%02d%*c%02d.%02d.%02d" RECEXT

#define DATAFORMATPES   "%4d-%02d-%02d.%02d%*c%02d.%02d.%02d" RECEXT
#define NAMEFORMATPES   "%s" ## DIR_DELIM ## "%s" ## DIR_DELIM "%4d-%02d-%02d.%02d.%02d.%02d.%02d" RECEXT
#define DATAFORMATTS    "%4d-%02d-%02d.%02d.%02d.%d-%d" RECEXT
#define NAMEFORMATTS    "%s"##DIR_DELIM"%s"##DIR_DELIM DATAFORMATTS

#ifdef VFAT
#define nameFORMAT   "%4d-%02d-%02d.%02d.%02d.%02d.%02d" RECEXT
#else
#define nameFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#endif
#define NAMEFORMAT   "%s/%s/" nameFORMAT
// end of implementation for brain dead systems


#define FINDCMD      "cd '%s' && find '%s' -follow -type d -name '%s' 2> /dev/null"

#define VIDEODIR "/video0"
const char *VideoDirectory = VIDEODIR;

char MarksfileSuffix[256] = MARKSFILESUFFIX;
 
bool setMarkfileSuffix(bool bIsPESFile)
{
  if(MarksfileSuffix!=NULL)
    releaseMarkfileName();
  sprintf(MarksfileSuffix,"%s%s","marks", bIsPESFile ? ".vdr" :"");
  return 0;
}

bool setMarkfileName(const char *mname, bool bIsPESFile)
{
  const char *name;
  if(MarksfileSuffix!=NULL)
    releaseMarkfileName();
  name = strrchr(mname,'/');
  if(name==NULL)
    name=mname;
  if(name!=NULL && strlen(name) > 0 && strlen(name) <= 254)
  {
     if(*name=='/')
		  sprintf(MarksfileSuffix,"%s%s",name, bIsPESFile ? ".vdr" :"");
     else
       sprintf(MarksfileSuffix,"/%s%s",name, bIsPESFile ? ".vdr" :"");
  }
  return 0;
}

void releaseMarkfileName()
{
  sprintf(MarksfileSuffix,"%s",MARKSFILESUFFIX);
}

// --- cFileName -------------------------------------------------------------
#define MAXFILESPERRECORDINGPES 255
#define RECORDFILESUFFIXPES     "/%03d.vdr"
#define MAXFILESPERRECORDINGTS  65535
#define RECORDFILESUFFIXTS      "/%05d.ts"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...

cFileName::cFileName(const char *FileName, bool Record, bool Blocking, bool IsPesRecording)
{
  file = NULL;
  fileNumber = 0;
  record = Record;
  blocking = Blocking;
  isPesRecording = IsPesRecording;
  // Prepare the file name:
  fileName = new char[strlen(FileName) + RECORDFILESUFFIXLEN];
  if (!fileName)
  {
    esyslog("ERROR: can't copy file name '%s'", fileName);
    return;
  }
  strcpy(fileName, FileName);
  pFileNumber = fileName + strlen(fileName);
  SetOffset(1);
}

cFileName::~cFileName()
{
  Close();
  delete [] fileName;
}

cUnbufferedFile *cFileName::Open(void)
{
  if (!file) 
  {
    int BlockingFlag = blocking ? 0 : O_NONBLOCK;
	  BlockingFlag |= O_LARGEFILE;
    if (record)
    {
        dsyslog("recording to '%s'", fileName);
        //file = open(fileName, BlockingFlag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        file = OpenVideoFile(fileName, O_RDWR | O_CREAT | BlockingFlag);
        if (!file)
           LOG_ERROR_STR(fileName);
        }
     else {
        if (access(fileName, R_OK) == 0) {
           dsyslog("playing '%s'", fileName);
           file = cUnbufferedFile::Create(fileName, O_RDONLY | BlockingFlag);
           if (!file)
              LOG_ERROR_STR(fileName);
           }
        else if (errno != ENOENT)
           LOG_ERROR_STR(fileName);
        }
     }
  return file;
}

void cFileName::Close(void)
{
  if (file) {
     if (CloseVideoFile(file) < 0)
        LOG_ERROR_STR(fileName);
     file = NULL;
     }
}

cUnbufferedFile *cFileName::SetOffset(int Number, off_t Offset)
{
  if (fileNumber != Number)
     Close();
  int MaxFilesPerRecording = isPesRecording ? MAXFILESPERRECORDINGPES : MAXFILESPERRECORDINGTS;
  if (0 < Number && Number <= MaxFilesPerRecording) {
     fileNumber = Number;
     sprintf(pFileNumber, isPesRecording ? RECORDFILESUFFIXPES : RECORDFILESUFFIXTS, fileNumber);
     if (record) {
        if (access(fileName, F_OK) == 0) {
           // files exists, check if it has non-zero size
           struct stat buf;
           if (stat(fileName, &buf) == 0) {
              if (buf.st_size != 0)
                 return SetOffset(Number + 1); // file exists and has non zero size, let's try next suffix
              else {
                 // zero size file, remove it
                 dsyslog("cFileName::SetOffset: removing zero-sized file %s", fileName);
                 unlink(fileName);
                 }
              }
           else
              return SetOffset(Number + 1); // error with fstat - should not happen, just to be on the safe side
           }
        else if (errno != ENOENT) { // something serious has happened
           LOG_ERROR_STR(fileName);
           return NULL;
           }
        // found a non existing file suffix
        }
     if (Open() >= 0) {
        if (!record && Offset >= 0 && file && file->Seek(Offset, SEEK_SET) != Offset) {
           LOG_ERROR_STR(fileName);
           return NULL;
           }
        }
     return file;
     }
  esyslog("ERROR: max number of files (%d) exceeded", MaxFilesPerRecording);
  return NULL;
}

cUnbufferedFile *cFileName::NextFile(void)
{
  return SetOffset(fileNumber + 1);
}

// --- cResumeFile ------------------------------------------------------------

cResumeFile::cResumeFile(const char *FileName, bool IsPesRecording)
{
  isPesRecording = IsPesRecording;
  const char *Suffix = isPesRecording ? RESUMEFILESUFFIX ".vdr" : RESUMEFILESUFFIX;
  fileName = new char[strlen(FileName) + strlen(Suffix) + 1];
  if (fileName) {
     strcpy(fileName, FileName);
     sprintf(fileName + strlen(fileName), Suffix, /*Setup.ResumeID ? "." :*/ "", /*Setup.ResumeID ? *itoa(Setup.ResumeID0) :*/ "");
     }
  else
     esyslog("ERROR: can't allocate memory for resume file name");

  if (fileName)
  {
    strcpy(fileName, FileName);
    strcat(fileName, RESUMEFILESUFFIX);
  }
  else
    esyslog("ERROR: can't allocate memory for resume file name");
}

cResumeFile::~cResumeFile()
{
  delete fileName;
}

int cResumeFile::Read(void)
{
  int resume = -1;
  if (fileName)
  {
    int f = open(fileName, O_RDONLY);
    if (f >= 0)
    {
      if (safe_read(f, &resume, sizeof(resume)) != sizeof(resume))
      {
        resume = -1;
        LOG_ERROR_STR(fileName);
      }
      close(f);
    }
    else if (errno != ENOENT)
      LOG_ERROR_STR(fileName);
  }
  return resume;
}

bool cResumeFile::Save(int Index)
{
  if (fileName)
  {
    int f = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (f >= 0)
    {
      if (safe_write(f, &Index, sizeof(Index)) < 0)
        LOG_ERROR_STR(fileName);
      close(f);
      return true;
    }
  }
  return false;
}

void cResumeFile::Delete(void)
{
  if (fileName)
  {
    if (remove(fileName) < 0 && errno != ENOENT)
      LOG_ERROR_STR(fileName);
  }
}


// --- cIndexFile ------------------------------------------------------------

// The number of frames to stay off the end in case of time shift:
#define INDEXSAFETYLIMIT 150 // frames

// The minimum age of an index file for considering it no longer to be written:
#define MININDEXAGE    3600 // seconds

cIndexFile::cIndexFile(const char *FileName, bool Record, bool IsPesRecording)
    :resumeFile(FileName,IsPesRecording)
{
  f = -1;
  fileName = NULL;
  size = 0;
  last = -1;
  index = NULL;
  isPesRecording = IsPesRecording;
  if (FileName)
  {
     const char *Suffix = isPesRecording ? INDEXFILESUFFIX ".vdr" : INDEXFILESUFFIX;
    fileName = new char[strlen(FileName) + strlen(Suffix) + 1];
    if (fileName)
    {
      strcpy(fileName, FileName);
      char *pFileExt = fileName + strlen(fileName);
      strcpy(pFileExt, Suffix);
      int delta = 0;
      if (access(fileName, R_OK) == 0)
      {
        struct stat buf;
        if (stat(fileName, &buf) == 0)
        {
          delta = buf.st_size % sizeof(tIndexTs);
          if (delta)
          {
            delta = sizeof(tIndexTs) - delta;
            esyslog("ERROR: invalid file size (%ld) in '%s'", buf.st_size, fileName);
          }
          //esyslog("INFO--->: sizeof(tIndexTs) %d, sizeof(tIndexPes) %d", sizeof(tIndexTs), sizeof(tIndexPes));
          last = (buf.st_size + delta) / sizeof(tIndexTs) - 1;
          if (!Record && last >= 0)
          {
            size = last + 1;
            index = new tIndexTs[size];
            if (index)
            {
              f = open(fileName, O_RDONLY);
              if (f >= 0)
              {
                if ((int)safe_read(f, index, buf.st_size) != buf.st_size)
                {
                  esyslog("ERROR: can't read from file '%s'", fileName);
                  delete [] index;
                  index = NULL;
                  close(f);
                  f = -1;
                }
					 // we don't close f here, see CatchUp()!
                //check();
                else if (isPesRecording)
						ConvertFromPes(index, size);
              }
              else
                LOG_ERROR_STR(fileName);
            }
            else
              esyslog("ERROR: can't allocate %d bytes for index '%s'", size * sizeof(tIndexTs), fileName);
          }
        }
        else
          LOG_ERROR;
      }
      else if (!Record)
        isyslog("missing index file %s", fileName);
      if (Record)
      {
        if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
        {
          if (delta)
          {
            esyslog("ERROR: padding index file with %d '0' bytes", delta);
            while (delta--)
              writechar(f, 0);
          }
        }
        else
          LOG_ERROR_STR(fileName);
      }
    }
    else
      esyslog("ERROR: can't copy file name '%s'", FileName);
  }
}

cIndexFile::~cIndexFile()
{
  if (f >= 0)
    close(f);
  delete [] fileName;
  delete [] index;
}

void cIndexFile::ConvertFromPes(tIndexTs *IndexTs, int Count)
{
	tIndexPes IndexPes;
	//while (Count-- > 0) 
	for(int i = 0; i < Count; i++ )
	{
		memcpy(&IndexPes, IndexTs, sizeof(IndexPes));
		IndexTs->offset = IndexPes.offset;
		IndexTs->independent = IndexPes.type == 1; // I_FRAME
		IndexTs->number = IndexPes.number;
		IndexTs->reserved = IndexPes.type;
		IndexTs++;
	}
}

void cIndexFile::ConvertToPes(tIndexTs *IndexTs, int Count)
{
	tIndexPes IndexPes;
	while (Count-- > 0) 
	{
		IndexPes.offset = uint32_t(IndexTs->offset);
		IndexPes.type = IndexTs->independent ? 1 : 2; // I_FRAME : "not I_FRAME" (exact frame type doesn't matter)
		IndexPes.number = IndexTs->number;
		IndexPes.reserved = 0;
		memcpy(IndexTs, &IndexPes, sizeof(*IndexTs));
		IndexTs++;
	}
}

bool cIndexFile::CatchUp(int Index)
{
  // returns true unless something really goes wrong, so that 'index' becomes NULL
  if (index && f >= 0)
  {
    for (int i = 0; i <= MAXINDEXCATCHUP && (Index < 0 || Index >= last); i++)
    {
      struct stat buf;
      if (fstat(f, &buf) == 0)
      {
        int newLast = buf.st_size / sizeof(tIndexTs) - 1;
        if (newLast > last)
        {
          if (size <= newLast)
          {
            size *= 2;
            if (size <= newLast)
              size = newLast + 1;
          }
          index = (tIndexTs *)realloc(index, size * sizeof(tIndexTs));
          if (index)
          {
            int offset = (last + 1) * sizeof(tIndexTs);
            int delta = (newLast - last) * sizeof(tIndexTs);
            if (lseek(f, offset, SEEK_SET) == offset)
            {
              if (safe_read(f, &index[last + 1], delta) != delta)
              {
                esyslog("ERROR: can't read from index");
                delete index;
                index = NULL;
                close(f);
                f = -1;
                break;
              }
                     if (isPesRecording)
                        ConvertFromPes(&index[last + 1], newLast - last);
              last = newLast;
            }
            else
              LOG_ERROR_STR(fileName);
          }
          else
            esyslog("ERROR: can't realloc() index");
        }
      }
      else
        LOG_ERROR_STR(fileName);
      if (Index >= last)
        sleep(1);
      else
        return true;
    }
  }
  return false;
}

bool cIndexFile::Write(bool Independent, uint16_t FileNumber, off_t FileOffset)
{
  if (f >= 0)
  {
     tIndexTs i(FileOffset, Independent, FileNumber);
     if (isPesRecording)
        ConvertToPes(&i, 1);
     if (safe_write(f, &i, sizeof(i)) < 0) {
        LOG_ERROR_STR(fileName);
        close(f);
        f = -1;
        return false;
        }
     last++;
     }
  return f >= 0;
}

bool cIndexFile::Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent, int *Length)
{
  if (CatchUp(Index)) {
     if (Index >= 0 && Index < last) {
        *FileNumber = index[Index].number;
        *FileOffset = index[Index].offset;
        if (Independent != NULL)
           *Independent = index[Index].independent;
        if (Length) {
           int fn = index[Index + 1].number;
           int fo = index[Index + 1].offset;
           if (fn == *FileNumber)
              *Length = fo - *FileOffset;
           else
              *Length = -1; // this means "everything up to EOF" (the buffer's Read function will act accordingly)
           }
        return true;
        }
     }
  return false;
}

int cIndexFile::GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber, off_t *FileOffset, int *Length, bool StayOffEnd)
{
  if (CatchUp()) {
     int d = Forward ? 1 : -1;
     for (;;) {
         Index += d;
         if (Index >= 0 && Index < last - ((Forward && StayOffEnd) ? INDEXSAFETYLIMIT : 0)) {
            if (index[Index].independent) {
               uint16_t fn;
               if (!FileNumber)
                  FileNumber = &fn;
               off_t fo;
               if (!FileOffset)
                  FileOffset = &fo;
               *FileNumber = index[Index].number;
               *FileOffset = index[Index].offset;
               if (Length) {
                  // all recordings end with a non-independent frame, so the following should be safe:
                  int fn = index[Index + 1].number;
                  int fo = index[Index + 1].offset;
                  if (fn == *FileNumber)
                     *Length = fo - *FileOffset;
                  else {
                     esyslog("ERROR: 'I' frame at end of file #%d", *FileNumber);
                     *Length = -1;
                     }
                  }
               return Index;
               }
            }
         else
            break;
         }
     }
  return -1;
}

int cIndexFile::Get(uint16_t FileNumber, off_t FileOffset)
{
  if (CatchUp()) {
     //TODO implement binary search!
     int i;
     for (i = 0; i < last; i++) {
         if (index[i].number > FileNumber || (index[i].number == FileNumber) && off_t(index[i].offset) >= FileOffset)
            break;
         }
     return i;
     }
  return -1;
}

void cIndexFile::check(int start)
{
   int i;
   int Length = -1;
   for (i = start; i < last; i++)
   {
      int fn = index[i + 1].number;
      int fo = index[i + 1].offset;
      if (fn == index[i].number)
          Length = fo - index[i].offset;
      if( Length < 0 )
        fprintf(stdout, " Error: illegal length %d",Length);
   }
}


// --- cNoadIndexFile ------------------------------------------------------------
cNoadIndexFile::cNoadIndexFile(const char *FileName, bool Record, bool IsPesRecording):cIndexFile(FileName, Record, IsPesRecording)
{
  fEx = -1;
  fileNameEx = NULL;
  sizeEx = 0;
  lastEx = -1;
  indexEx = NULL;
  interval = 0;
  lastGetValue = 0;
  #ifdef VNOAD
  if (FileName)
  {
    fileNameEx = new char[strlen(FileName) + strlen(INDEXFILESUFFIXEX) + 1];
    if (fileNameEx)
    {
      strcpy(fileNameEx, FileName);
      char *pFileExt = fileNameEx + strlen(fileNameEx);
      strcpy(pFileExt, INDEXFILESUFFIXEX);
      int delta = 0;
      if (access(fileNameEx, R_OK) == 0)
      {
        struct stat buf;
        if (stat(fileNameEx, &buf) == 0)
        {
          delta = buf.st_size % sizeof(tIndexEx);
          if (delta)
          {
            delta = sizeof(tIndexEx) - delta;
            esyslog("ERROR: invalid file size (%ld) in '%s'", buf.st_size, fileNameEx);
          }
          last = (buf.st_size + delta) / sizeof(tIndexEx) - 1;
          if (!Record && last >= 0)
          {
            size = last + 1;
            indexEx = new tIndexEx[size];
            if (indexEx)
            {
              fEx = open(fileNameEx, O_RDONLY);
              if (fEx >= 0)
              {
                if ((int)safe_read(fEx, indexEx, buf.st_size) != buf.st_size)
                {
                  esyslog("ERROR: can't read from file '%s'", fileName);
                  delete indexEx;
                  indexEx = NULL;
                  close(fEx);
                  fEx = -1;
                }
                // we don't close f here, see CatchUp()!
              }
              else
                LOG_ERROR_STR(fileNameEx);
            }
            else
              esyslog("ERROR: can't allocate %d bytes for index '%s'", size * sizeof(tIndexEx), fileNameEx);
          }
        }
        else
          LOG_ERROR;
      }
      else if (!Record)
        isyslog("missing index file %s", fileNameEx);
      if (Record)
      {
        if ((fEx = open(fileNameEx, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
        {
          if (delta)
          {
            esyslog("ERROR: padding index file with %d '0' bytes", delta);
            while (delta--)
              writechar(fEx, 0);
          }
        }
        else
          LOG_ERROR_STR(fileNameEx);
      }
    }
    else
      esyslog("ERROR: can't copy file name '%s'", FileName);
  }
  #endif
  if(indexEx == NULL)
    indexEx = new tIndexEx[Last()>0?Last():1];

}

cNoadIndexFile::~cNoadIndexFile()
{
  if (fEx >= 0)
    close(fEx);
  delete [] fileNameEx;
  delete [] indexEx;
}

bool cNoadIndexFile::Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent, int *Length)
{
  if (index >= 0)
  {
     if (Index >= 0 && Index < last) 
	  {
        *FileNumber = index[Index].number;
        *FileOffset = index[Index].offset;
        if (Independent)
           *Independent = index[Index].independent;
        if (Length) 
		  {
           int fn = index[Index + 1].number;
           int fo = index[Index + 1].offset;
           if (fn == *FileNumber)
              *Length = fo - *FileOffset;
           else
              *Length = -1; // this means "everything up to EOF" (the buffer's Read function will act accordingly)
        }
        return true;
     }
  }
  dsyslog( "cNoadIndexFile::Get returns false at %d (last is %d)",Index,last );
  return false;
}

//#ifdef VNOAD
int cNoadIndexFile::getLast() 
{ 
  if( interval )
  {
    return lastGetValue;
  }
  return( last ); 
}
//#endif

int cNoadIndexFile::GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber, off_t *FileOffset, int *Length, bool StayOffEnd)
{
  if (index)
  {
    int d = Forward ? 1 : -1;
    for (;;)
    {
      Index += d;
      if (Index >= 0 && Index < last - ((Forward && StayOffEnd) ? INDEXSAFETYLIMIT : 0))
      {
		  if (index[Index].independent)
        {
          uint16_t fn;
          if (!FileNumber)
             FileNumber = &fn;
          off_t fo;
          if (!FileOffset)
             FileOffset = &fo;
          *FileNumber = index[Index].number;
          *FileOffset = index[Index].offset;
          if (Length)
          {
            // all recordings end with a non-I_FRAME, so the following should be safe:
            int fn = index[Index + 1].number;
            int fo = index[Index + 1].offset;
            if (fn == *FileNumber)
              *Length = fo - *FileOffset;
            else
            {
              esyslog("ERROR: 'I' frame at end of file #%d", *FileNumber);
              *Length = -1;
            }
          }
          return Index;
        }
      }
      else
        break;
    }
  }
  return -1;
}

/*
int cNoadIndexFile::Get(uchar FileNumber, int FileOffset)
{
  if (index)
  {
    //TODO implement binary search!
    int i;
    for (i = 0; i < last; i++)
    {
      if (index[i].number > FileNumber || (index[i].number == FileNumber) && index[i].offset >= FileOffset)
        break;
    }
    return i;
  }
  return -1;
}
*/

bool cNoadIndexFile::setIndexEx( int _index, int _isLogo, int _blackTop, int _blackBottom)
{
  bool bRet = false;
  if( _index > 0 && _index <= Last() )
  {
    //dsyslog(LOG_INFO, "cNoadIndexFile::setIndexEx %d %d %d %d", _index, _isLogo, _blackTop, _blackBottom);
    tIndexEx *ti = &indexEx[_index];
    //dsyslog(LOG_INFO, "cNoadIndexFile::setIndexEx %p %d %d %d %d", ti, _index, _isLogo, _blackTop, _blackBottom);
    ti->isLogo = _isLogo;
    ti->blackTop = _blackTop;
    ti->blackBottom = _blackBottom;
    bRet = true;
  }
  return bRet;
}

void cNoadIndexFile::logIndexEx()
{
   int fLog = -1;
   char *logFileName = new char[strlen(fileNameEx) + 4 + 1];
   strcpy(logFileName, fileNameEx);
   char *pFileExt = logFileName + strlen(logFileName);
   strcpy(pFileExt, ".log");

   dsyslog( "cNoadIndexFile::logIndexEx() to file %s last is %d", logFileName, Last());

   if ((fLog = open(logFileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
   {
     //dsyslog( "cNoadIndexFile::logIndexEx() fLog is %d", fLog);
     for( int i = 0; i < Last(); i++)
     {
       char *buffer;
       tIndexEx *ti = &indexEx[i];
       asprintf(&buffer, "%06d %2d %3d %3d\n",i,ti->isLogo,ti->blackTop, ti->blackBottom);
       if (safe_write(fLog, buffer, strlen(buffer)) < 0)
       {
         LOG_ERROR_STR(logFileName);
       }
       //dsyslog( "cNoadIndexFile::save %p %d %d %d %d --> %s",ti, i,ti->isLogo,ti->blackTop, ti->blackBottom, buffer);
       delete buffer;
     }
     close(fLog);
   }
   else
     LOG_ERROR_STR(logFileName);
   dsyslog( "cNoadIndexFile::logIndexEx() fLog is %d", fLog);
   delete logFileName;
}

//#ifdef VNOAD
bool cNoadIndexFile::CatchUp(int Index)
{
  bool bRet = cIndexFile::CatchUp(Index);
  if( interval )
  {
    lastGetValue += interval;
    lastGetValue = GetNextIFrame( lastGetValue+interval, true, NULL, NULL, NULL, true) - 1;
    if( lastGetValue > last || lastGetValue < 0 )
      lastGetValue = last;
  }
  return bRet;
}
//#endif

// --- cMark -----------------------------------------------------------------

double MarkFramesPerSecond = DEFAULTFRAMESPERSECOND;
char *cMark::buffer = NULL;

cMark::cMark(int Position, const char *Comment, double FramesPerSecond)
{
  position = Position;
  framesPerSecond = FramesPerSecond;
  comment = Comment ? strdup(Comment) : NULL;
  checked = false;
}

cMark::~cMark()
{
  //delete comment;
  free(comment);
}

cString cMark::ToText(bool bWithNewline)
{
  return cString::sprintf("%s%s%s%s", (const char *)IndexToHMSF(position, true, framesPerSecond), comment ? " " : "", comment ? comment : "",bWithNewline?"\n":"");
  /*
  free(buffer);
  asprintf(&buffer, "%s%s%s%s", (const char *)IndexToHMSF(position, true), comment ? " " : "", comment ? comment : "",bWithNewline?"\n":"" );
  return buffer;
  */
}

bool cMark::Parse(const char *s)
{
  delete comment;
  comment = NULL;
  framesPerSecond = MarkFramesPerSecond;
  position = HMSFToIndex(s, framesPerSecond);
  const char *p = strchr(s, ' ');
  if (p) 
  {
     p = skipspace(p);
     if (*p)
	  {
        comment = strdup(p);
	     comment[strlen(comment) - 1] = 0; // strips trailing newline
     }
  }
  return true;
}

bool cMark::Save(FILE *f)
{
  return fprintf(f, "%s", (const char *)ToText()) > 0;
}


// --- cMarks ----------------------------------------------------------------

//cMutex MutexMarkFramesPerSecond;

bool cMarks::Load(const char *RecordingFileName, double FramesPerSecond, bool IsPesRecording)
{
//  cMutexLock MutexLock(&MutexMarkFramesPerSecond);
  framesPerSecond = FramesPerSecond;
  MarkFramesPerSecond = framesPerSecond;
  
  //if (cConfig<cMark>::Load(AddDirectory(RecordingFileName, IsPesRecording ? MARKSFILESUFFIX ".vdr" : MARKSFILESUFFIX).c_str())) 
  if (cConfig<cMark>::Load(AddDirectory(RecordingFileName, MarksfileSuffix)))
  {
     Sort();
     return true;
  }
  return false;
}

/*
bool cMarks::ReLoad()
{
	if (cConfig<cMark>::Load()) 
	{
		Sort();
		return true;
	}
	return false;
}
*/

bool bMarksBackupDone = false;
bool cMarks::Backup(const char *RecordingFileName)
{
  const char *MarksFile = AddDirectory(RecordingFileName, MarksfileSuffix);
  char *oldname = NULL;
  asprintf(&oldname,"%s",MarksFile);
  const char *BackupMarksFile = AddDirectory(RecordingFileName, "/marks0.vdr");
  if (rename(oldname, BackupMarksFile) == -1) {
     LOG_ERROR_STR(oldname);
     delete oldname;
     return false;
     }
  delete oldname;
  bMarksBackupDone = true;
  return true;
}

void cMarks::Sort(void)
{
  for (cMark *m1 = First(); m1; m1 = Next(m1)) 
  {
      for (cMark *m2 = Next(m1); m2; m2 = Next(m2)) 
		{
          if (m2->position < m1->position) 
			 {
             swap(m1->position, m2->position);
             swap(m1->comment, m2->comment);
          }
      }
	}
}

cMark *cMarks::Add(int Position)
{
  cMark *m = Get(Position);
  if (!m) 
  {
     cConfig<cMark>::Add(m = new cMark(Position, NULL, framesPerSecond));
     cString s = m->ToText();
     dsyslog( "cMarks::Add(%d) is %s",Position, (const char *)s);
     Sort();
  }
  return m;
}

cMark *cMarks::Get(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi))
  {
    if (mi->position == Position)
      return mi;
  }
  return NULL;
}

cMark *cMarks::GetPrev(int Position)
{
  for (cMark *mi = Last(); mi; mi = Prev(mi))
  {
    if (mi->position < Position)
      return mi;
  }
  return NULL;
}

cMark *cMarks::GetNext(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi))
  {
    if (mi->position > Position)
      return mi;
  }
  return NULL;
}

/*
int cMarks::nextActivePosition(int Position)
{
  bool bActive = false;
  cMark *mi;
  for (mi = First(); mi; mi = Next(mi))
  {
    if (mi->position > Position)
      break;
    bActive = !bActive;
  }
  if( !bActive )
    if(mi)
      return mi->position;
  return ++Position;
}
*/
bool cMarks::hasUncheckedMarks()
{
  cMark *mi;
  for (mi = First(); mi; mi = Next(mi))
  {
    if (!mi->isChecked() )
      return true;
  }
  return false;
}

int cMarks::getActiveFrames(int totalFrames)
{
  cMark *m = GetNext(-1);
  if( m == NULL )
    return 0;
  int iRet=0;
  int iLastPos = m->position;
  m = Next(m);
  while( m )
  {
    iRet += (m->position - iLastPos);
    iLastPos = 0;
    m = Next(m);
    if( m )
    {
      iLastPos = m->position;
      m = Next(m);
    }
  }
  if( iLastPos )
    iRet += (totalFrames-iLastPos);
  return iRet;
}

#ifdef VNOAD
// --- cNoAdMarks ----------------------------------------------------------------

bool cNoAdMarks::Load(const char *RecordingFileName, bool AllowComments)
{
  const char *MarksFile = AddDirectory(RecordingFileName, NOADMARKSFILESUFFIX);
  if (cConfig<cMark>::Load(MarksFile))
  {
    Sort();
    return true;
  }
  return false;
}
#endif

cString IndexToHMSF(int Index, bool WithFrame, double FramesPerSecond)
{
  static char buffer[16];
  double Seconds;
  int f = int(modf((Index + 0.5) / FramesPerSecond, &Seconds) * FramesPerSecond + 1);
  int s = int(Seconds);
  int m = s / 60 % 60;
  int h = s / 3600;
  s %= 60;
  snprintf(buffer, sizeof(buffer), WithFrame ? "%d:%02d:%02d.%02d" : "%d:%02d:%02d", h, m, s, f);
  return buffer;
}

int HMSFToIndex(const char *HMSF, double FramesPerSecond)
{
  int h, m, s, f = 1;
  int n = sscanf(HMSF, "%d:%d:%d.%d", &h, &m, &s, &f);
  if (n == 1)
     return h - 1; // plain frame number
  if (n >= 3)
     return int( ceil( ((h * 3600 + m * 60 + s) * FramesPerSecond)+0.5) ) + f - 1;
  return 0;
}

// --- cRecording ------------------------------------------------------------

#define RESUME_NOT_INITIALIZED (-2)

struct tCharExchange { char a; char b; };
tCharExchange CharExchange[] = {
  { '~',  '/'    },
  { ' ',  '_'    },
  { '\'', '\x01' },
  { '/',  '\x02' },
  { 0, 0 }
  };

static char *ExchangeChars(char *s, bool ToFileSystem)
{
  char *p = s;
  while (*p) {
#ifdef VFAT
        // The VFAT file system can't handle all characters, so we
        // have to take extra efforts to encode/decode them:
        if (ToFileSystem) {
           switch (*p) {
                  // characters that can be used "as is":
                  case '!':
                  case '@':
                  case '$':
                  case '%':
                  case '&':
                  case '(':
                  case ')':
                  case '+':
                  case ',':
                  case '-':
                  case ';':
                  case '=':
                  case '0' ... '9':
                  case 'a' ... 'z':
                  case 'A' ... 'Z':
                  case '�': case '�':
                  case '�': case '�':
                  case '�': case '�':
                  case '�':
                       break;
                  // characters that can be mapped to other characters:
                  case ' ': *p = '_'; break;
                  case '~': *p = '/'; break;
                  // characters that have to be encoded:
                  default:
                    if (*p != '.' || !*(p + 1) || *(p + 1) == '~') { // Windows can't handle '.' at the end of directory names
                       int l = p - s;
                       s = (char *)realloc(s, strlen(s) + 10);
                       p = s + l;
                       char buf[4];
                       sprintf(buf, "#%02X", (unsigned char)*p);
                       memmove(p + 2, p, strlen(p) + 1);
                       strncpy(p, buf, 3);
                       p += 2;
                       }
                  }
           }
        else {
           switch (*p) {
             // mapped characters:
             case '_': *p = ' '; break;
             case '/': *p = '~'; break;
             // encodes characters:
             case '#': {
                  if (strlen(p) > 2) {
                     char buf[3];
                     sprintf(buf, "%c%c", *(p + 1), *(p + 2));
                     unsigned char c = strtol(buf, NULL, 16);
                     *p = c;
                     memmove(p + 1, p + 3, strlen(p) - 2);
                     }
                  }
                  break;
             // backwards compatibility:
             case '\x01': *p = '\''; break;
             case '\x02': *p = '/';  break;
             case '\x03': *p = ':';  break;
             }
           }
#else
        for (struct tCharExchange *ce = CharExchange; ce->a && ce->b; ce++) {
            if (*p == (ToFileSystem ? ce->a : ce->b)) {
               *p = ToFileSystem ? ce->b : ce->a;
               break;
               }
            }
#endif
        p++;
        }
  return s;
}

/*
cRecording::cRecording(cTimer *Timer, const char *Title, const char *Subtitle, const char *Summary)
{
  resume = RESUME_NOT_INITIALIZED;
  titleBuffer = NULL;
  sortBuffer = NULL;
  fileName = NULL;
  name = NULL;
  // set up the actual name:
  const char *OriginalSubtitle = Subtitle;
  char SubtitleBuffer[MAX_SUBTITLE_LENGTH];
  if (isempty(Title))
     Title = Timer->Channel()->Name();
  if (isempty(Subtitle))
     Subtitle = " ";
  else if (strlen(Subtitle) > MAX_SUBTITLE_LENGTH) {
     // let's make sure the Subtitle doesn't produce too long a file name:
     strn0cpy(SubtitleBuffer, Subtitle, MAX_SUBTITLE_LENGTH);
     Subtitle = SubtitleBuffer;
     }
  char *macroTITLE   = strstr(Timer->File(), TIMERMACRO_TITLE);
  char *macroEPISODE = strstr(Timer->File(), TIMERMACRO_EPISODE);
  if (macroTITLE || macroEPISODE) {
     name = strdup(Timer->File());
     name = strreplace(name, TIMERMACRO_TITLE, Title);
     name = strreplace(name, TIMERMACRO_EPISODE, Subtitle);
     if (Timer->IsSingleEvent()) {
        Timer->SetFile(name); // this was an instant recording, so let's set the actual data
        Timers.Save();
        }
     }
  else if (Timer->IsSingleEvent() || !Setup.UseSubtitle)
     name = strdup(Timer->File());
  else
     asprintf(&name, "%s~%s", Timer->File(), Subtitle);
  // substitute characters that would cause problems in file names:
  strreplace(name, '\n', ' ');
  start = Timer->StartTime();
  priority = Timer->Priority();
  lifetime = Timer->Lifetime();
  // handle summary:
  summary = !isempty(Timer->Summary()) ? strdup(Timer->Summary()) : NULL;
  if (!summary) {
     Subtitle = OriginalSubtitle;
     if (isempty(Subtitle))
        Subtitle = "";
     if (isempty(Summary))
        Summary = "";
     if (*Subtitle || *Summary)
        asprintf(&summary, "%s\n\n%s%s%s", Title, Subtitle, (*Subtitle && *Summary) ? "\n\n" : "", Summary);
     }
}
*/

cRecording::cRecording(const char *FileName)
{
  resume = RESUME_NOT_INITIALIZED;
  titleBuffer = NULL;
  sortBuffer = NULL;
  fileName = strdup(FileName);
  FileName += strlen(VideoDirectory) + 1;
  const char *p = strrchr(FileName, '/');

  name = NULL;
  summary = NULL;
  if (p) {
     time_t now = time(NULL);
     struct tm tm_r;
     struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
     t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
     if (7 == sscanf(p + 1, DATAFORMAT, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &priority, &lifetime)) {
        t.tm_year -= 1900;
        t.tm_mon--;
        t.tm_sec = 0;
        start = mktime(&t);
        name = MALLOC(char, p - FileName + 1);
        strncpy(name, FileName, p - FileName);
        name[p - FileName] = 0;
        name = ExchangeChars(name, false);
        }
     // read an optional summary file:
     char *SummaryFileName = NULL;
     asprintf(&SummaryFileName, "%s%s", fileName, SUMMARYFILESUFFIX);
     int f = open(SummaryFileName, O_RDONLY);
     if (f >= 0) {
        struct stat buf;
        if (fstat(f, &buf) == 0) {
           int size = buf.st_size;
           summary = MALLOC(char, size + 1); // +1 for terminating 0
           if (summary) {
              int rbytes = safe_read(f, summary, size);
              if (rbytes >= 0) {
                 summary[rbytes] = 0;
                 if (rbytes != size)
                    esyslog("%s: expected %d bytes but read %d", SummaryFileName, size, rbytes);
                 }
              else {
                 LOG_ERROR_STR(SummaryFileName);
                 free(summary);
                 summary = NULL;
                 }

              }
           else
              esyslog("can't allocate %d byte of memory for summary file '%s'", size + 1, SummaryFileName);
           close(f);
           }
        else
           LOG_ERROR_STR(SummaryFileName);
        }
     else if (errno != ENOENT)
        LOG_ERROR_STR(SummaryFileName);
     free(SummaryFileName);
     }
}

cRecording::~cRecording()
{
  free(titleBuffer);
  free(sortBuffer);
  free(fileName);
  free(name);
  free(summary);
}

char *cRecording::StripEpisodeName(char *s)
{
  char *t = s, *s1 = NULL, *s2 = NULL;
  while (*t) {
        if (*t == '/') {
           if (s1) {
              if (s2)
                 s1 = s2;
              s2 = t;
              }
           else
              s1 = t;
           }
        t++;
        }
  if (s1 && s2)
     memmove(s1 + 1, s2, t - s2 + 1);
  return s;
}

char *cRecording::SortName(void)
{
  if (!sortBuffer) {
     char *s = StripEpisodeName(strdup(FileName() + strlen(VideoDirectory) + 1));
     char *s2 = *s == '\%'? s+1:s;
     int l = strxfrm(NULL, s2, 0);
     sortBuffer = MALLOC(char, l);
     strxfrm(sortBuffer, s2, l);
     free(s);
     }
  return sortBuffer;
}

int cRecording::GetResume(void) const
{
  if (resume == RESUME_NOT_INITIALIZED) {
     cResumeFile ResumeFile(FileName(),isPesRecording);
     resume = ResumeFile.Read();
     }
  return resume;
}

bool cRecording::operator< (const cListObject &ListObject)
{
  cRecording *r = (cRecording *)&ListObject;
  return strcasecmp(SortName(), r->SortName()) < 0;
}

const char *cRecording::FileName(void) const
{
  if (!fileName) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     name = ExchangeChars(name, true);
     asprintf(&fileName, NAMEFORMAT, VideoDirectory, name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, priority, lifetime);
     name = ExchangeChars(name, false);
     }
  return fileName;
}

const char *cRecording::Title(char Delimiter, bool NewIndicator, int Level) const
{
  char New = NewIndicator && IsNew() ? '*' : ' ';
  free(titleBuffer);
  titleBuffer = NULL;
  if (Level < 0 || Level == HierarchyLevels()) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     char *s;
     if (Level > 0 && (s = strrchr(name, '~')) != NULL)
        s++;
     else
        s = name;
     asprintf(&titleBuffer, "%02d.%02d%c%02d:%02d%c%c%s",
                            t->tm_mday,
                            t->tm_mon + 1,
                            Delimiter,
                            t->tm_hour,
                            t->tm_min,
                            New,
                            Delimiter,
                            s);
     // let's not display a trailing '~':
     stripspace(titleBuffer);
     s = &titleBuffer[strlen(titleBuffer) - 1];
     if (*s == '~')
        *s = 0;
     }
  else if (Level < HierarchyLevels()) {
     const char *s = name;
     const char *p = s;
     while (*++s) {
           if (*s == '~') {
              if (Level--)
                 p = s + 1;
              else
                 break;
              }
           }
     titleBuffer = MALLOC(char, s - p + 3);
     *titleBuffer = Delimiter;
     *(titleBuffer + 1) = Delimiter;
     strn0cpy(titleBuffer + 2, p, s - p + 1);
     }
  else
     return "";
  return titleBuffer;
}

const char *PrefixVideoFileName(const char *FileName, char Prefix)
{
  static char *PrefixedName = NULL;

  if (!PrefixedName || strlen(PrefixedName) <= strlen(FileName))
     PrefixedName = (char *)realloc(PrefixedName, strlen(FileName) + 2);
  if (PrefixedName) {
     const char *p = FileName + strlen(FileName); // p points at the terminating 0
     int n = 2;
     while (p-- > FileName && n > 0) {
           if (*p == '/') {
              if (--n == 0) {
                 int l = p - FileName + 1;
                 strncpy(PrefixedName, FileName, l);
                 PrefixedName[l] = Prefix;
                 strcpy(PrefixedName + l + 1, p + 1);
                 return PrefixedName;
                 }
              }
           }
     }
  return NULL;
}

const char *cRecording::PrefixFileName(char Prefix)
{
  const char *p = PrefixVideoFileName(FileName(), Prefix);
  if (p) {
     free(fileName);
     fileName = strdup(p);
     return fileName;
     }
  return NULL;
}

int cRecording::HierarchyLevels(void) const
{
  const char *s = name;
  int level = 0;
  while (*++s) {
        if (*s == '~')
           level++;
        }
  return level;
}

bool cRecording::IsEdited(void) const
{
  const char *s = strrchr(name, '~');
  s = !s ? name : s + 1;
  return *s == '%';
}

bool cRecording::WriteSummary(void)
{
  if (summary) {
     char *SummaryFileName = NULL;
     asprintf(&SummaryFileName, "%s%s", fileName, SUMMARYFILESUFFIX);
     FILE *f = fopen(SummaryFileName, "w");
     if (f) {
        if (fputs(summary, f) < 0)
           LOG_ERROR_STR(SummaryFileName);
        fclose(f);
        }
     else
        LOG_ERROR_STR(SummaryFileName);
     free(SummaryFileName);
     }
  return true;
}

/*
bool cRecording::WriteRecInfo(cTimer *timer)
{
  char *RecinfoFileName = NULL;
  asprintf(&RecinfoFileName, "%s%s", fileName, RECINFOFILESUFFIX);
  FILE *f = fopen(RecinfoFileName, "w");
  if (f) {
     if (!timer->Save(f))
        LOG_ERROR_STR(RecinfoFileName);
     cChannel ch = *timer->Channel();
     ch.SetName(timer->Channel()->Name());
     if (fprintf(f, ch.ToText()) > 0)
        LOG_ERROR_STR(RecinfoFileName);
     fclose(f);
     }
  else
     LOG_ERROR_STR(RecinfoFileName);
  free(RecinfoFileName);
  return true;
}
*/

/*
bool cRecording::Delete(void)
{
  bool result = true;
  char *NewName = strdup(FileName());
  char *ext = strrchr(NewName, '.');
  if (strcmp(ext, RECEXT) == 0) {
     strncpy(ext, DELEXT, strlen(ext));
     if (access(NewName, F_OK) == 0) {
        // the new name already exists, so let's remove that one first:
        isyslog("removing recording %s", NewName);
        RemoveVideoFile(NewName);
        }
     isyslog("deleting recording %s", FileName());
     result = RenameVideoFile(FileName(), NewName);
     }
  free(NewName);
  return result;
}

bool cRecording::Remove(void)
{
  // let's do a final safety check here:
  if (!endswith(FileName(), DELEXT)) {
     esyslog("attempt to remove recording %s", FileName());
     return false;
     }
  isyslog("removing recording %s", FileName());
  return RemoveVideoFile(FileName());
}
*/
// --- cRecordings -----------------------------------------------------------

bool cRecordings::Load(bool Deleted)
{
  Clear();
  bool result = false;
  char *cmd = NULL;
  asprintf(&cmd, FINDCMD, VideoDirectory, VideoDirectory, Deleted ? "*" DELEXT : "*" RECEXT);
  FILE *p = popen(cmd, "r");
  if (p) {
     char *s;
     while ((s = readline(p)) != NULL) {
           cRecording *r = new cRecording(s);
           if (r->Name())
              Add(r);
           else
              delete r;
           }
     pclose(p);
     Sort();
     result = Count() > 0;
     }
/*  else
     Skins.Message(mtError, "Error while opening pipe!");*/
  free(cmd);
  return result;
}

cRecording *cRecordings::GetByName(const char *FileName)
{
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (strcmp(recording->FileName(), FileName) == 0)
         return recording;
      }
  return NULL;
}

bool isPESRecording(const char *FileName)
{
  bool isPesRecording = false;
  char *fileName = strdup(FileName);
#ifdef _MSC_VER
  char *cp = strchr(fileName, '/');
  while( cp )
  {
	  *cp = DIR_DELIMC;
		cp = strchr(fileName, '/');
  }
#endif
  const char *p = strrchr(fileName , DIR_DELIMC );
  if (p) 
  {
     time_t now = time(NULL);
#ifdef _MSC_VER
	  struct tm t = *localtime(&now);
#else
     struct tm tm_r;
	  struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
#endif
     t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
	  int channel = -1;
	  int priority = -1;
	  int instanceId = -1;
	  int lifetime = -1;
     if (7 == sscanf(p + 1, DATAFORMATTS, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &channel, &instanceId)
      || 7 == sscanf(p + 1, DATAFORMATPES, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &priority, &lifetime)) 
	  {
        t.tm_year -= 1900;
        t.tm_mon--;
        t.tm_sec = 0;
        isPesRecording = instanceId < 0;
     }
  }
  free(fileName);
  return isPesRecording;
}

cUnbufferedFile *OpenVideoFile(const char *FileName, int Flags)
{
  const char *ActualFileName = FileName;

  // Incoming name must be in base video directory:
  if (strstr(FileName, VideoDirectory) != FileName) {
     esyslog("ERROR: %s not in %s", FileName, VideoDirectory);
     errno = ENOENT; // must set 'errno' - any ideas for a better value?
     return NULL;
     }
/*
// Are we going to create a new file?
  if ((Flags & O_CREAT) != 0) {
     cVideoDirectory Dir;
     if (Dir.IsDistributed()) {
        // Find the directory with the most free space:
        int MaxFree = Dir.FreeMB();
        while (Dir.Next()) {
              int Free = FreeDiskSpaceMB(Dir.Name());
              if (Free > MaxFree) {
                 Dir.Store();
                 MaxFree = Free;
                 }
              }
        if (Dir.Stored()) {
           ActualFileName = Dir.Adjust(FileName);
           if (!MakeDirs(ActualFileName, false))
              return NULL; // errno has been set by MakeDirs()
           if (symlink(ActualFileName, FileName) < 0) {
              LOG_ERROR_STR(FileName);
              return NULL;
              }
           ActualFileName = strdup(ActualFileName); // must survive Dir!
           }
        }
     }
*/
  cUnbufferedFile *File = cUnbufferedFile::Create(ActualFileName, Flags, DEFFILEMODE);
  if (ActualFileName != FileName)
     free((char *)ActualFileName);
  return File;
}

// --- cUnbufferedFile -------------------------------------------------------

#ifndef _MSC_VER
#if (HAVE_POSIX_FADVISE==1 )
#define USE_FADVISE
#endif
#endif

#define WRITE_BUFFER KILOBYTE(800)

cUnbufferedFile::cUnbufferedFile(void)
{
  fd = -1;
}

cUnbufferedFile::~cUnbufferedFile()
{
  Close();
}

int cUnbufferedFile::Open(const char *FileName, int Flags, mode_t Mode)
{
  Close();
  fd = open(FileName, Flags, Mode);
  curpos = 0;
  begin = lastpos = ahead = 0;
#ifdef USE_FADVISE
  cachedstart = 0;
  cachedend = 0;
  readahead = KILOBYTE(128);
  written = 0;
  totwritten = 0;
  if (fd >= 0)
     posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM); // we could use POSIX_FADV_SEQUENTIAL, but we do our own readahead, disabling the kernel one.
#endif
  return fd;
}

int cUnbufferedFile::Close(void)
{
#ifdef USE_FADVISE
  if (fd >= 0) {
     if (totwritten)    // if we wrote anything make sure the data has hit the disk before
        fdatasync(fd);  // calling fadvise, as this is our last chance to un-cache it.
     posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
     }
#endif
  int OldFd = fd;
  int ret = -1;
  fd = -1;
  if( OldFd > 0 )
	  ret = close(OldFd);
  return ret;
}

// When replaying and going e.g. FF->PLAY the position jumps back 2..8M
// hence we do not want to drop recently accessed data at once.
// We try to handle the common cases such as PLAY->FF->PLAY, small
// jumps, moving editing marks etc.

#define FADVGRAN   KILOBYTE(4) // AKA fadvise-chunk-size; PAGE_SIZE or getpagesize(2) would also work.
#define READCHUNK  MEGABYTE(8)

void cUnbufferedFile::SetReadAhead(size_t ra)
{
  readahead = ra;
}

#ifdef USE_FADVISE
int cUnbufferedFile::FadviseDrop(off_t Offset, off_t Len)
{
  // rounding up the window to make sure that not PAGE_SIZE-aligned data gets freed.
  return posix_fadvise(fd, Offset - (FADVGRAN - 1), Len + (FADVGRAN - 1) * 2, POSIX_FADV_DONTNEED);
}
#endif

off_t cUnbufferedFile::Seek(off_t Offset, int Whence)
{
  //if (Whence == SEEK_SET && Offset == curpos)
  //   return curpos;
  curpos = lseek(fd, Offset, Whence);
  return curpos;
}

ssize_t cUnbufferedFile::Read(void *Data, size_t Size)
{
  if (fd >= 0) {
#ifdef USE_FADVISE
     off_t jumped = curpos-lastpos; // nonzero means we're not at the last offset
     if ((cachedstart < cachedend) && (curpos < cachedstart || curpos > cachedend)) {
        // current position is outside the cached window -- invalidate it.
        FadviseDrop(cachedstart, cachedend-cachedstart);
        cachedstart = curpos;
        cachedend = curpos;
        }
     cachedstart = min(cachedstart, curpos);
#endif
     ssize_t bytesRead = safe_read(fd, Data, Size);
     if (bytesRead > 0) {
        curpos += bytesRead;
#ifdef USE_FADVISE
        cachedend = max(cachedend, curpos);

        // Read ahead:
        // no jump? (allow small forward jump still inside readahead window).
        if (jumped >= 0 && jumped <= (off_t)readahead) {
           // Trigger the readahead IO, but only if we've used at least
           // 1/2 of the previously requested area. This avoids calling
           // fadvise() after every read() call.
           if (ahead - curpos < (off_t)(readahead / 2)) {
              posix_fadvise(fd, curpos, readahead, POSIX_FADV_WILLNEED);
              ahead = curpos + readahead;
              cachedend = max(cachedend, ahead);
              }
           if (readahead < Size * 32) { // automagically tune readahead size.
              readahead = Size * 32;
              }
           }
        else
           ahead = curpos; // jumped -> we really don't want any readahead, otherwise e.g. fast-rewind gets in trouble.
        }

     if (cachedstart < cachedend) {
        if (curpos - cachedstart > READCHUNK * 2) {
           // current position has moved forward enough, shrink tail window.
           FadviseDrop(cachedstart, curpos - READCHUNK - cachedstart);
           cachedstart = curpos - READCHUNK;
           }
        else if (cachedend > ahead && cachedend - curpos > READCHUNK * 2) {
           // current position has moved back enough, shrink head window.
           FadviseDrop(curpos + READCHUNK, cachedend - (curpos + READCHUNK));
           cachedend = curpos + READCHUNK;
           }
#endif
        }
     lastpos = curpos;
     return bytesRead;
     }
  return -1;
}

ssize_t cUnbufferedFile::Write(const void *Data, size_t Size)
{
  if (fd >=0) {
     ssize_t bytesWritten = safe_write(fd, Data, Size);
#ifdef USE_FADVISE
     if (bytesWritten > 0) {
        begin = min(begin, curpos);
        curpos += bytesWritten;
        written += bytesWritten;
        lastpos = max(lastpos, curpos);
        if (written > WRITE_BUFFER) {
           if (lastpos > begin) {
              // Now do three things:
              // 1) Start writeback of begin..lastpos range
              // 2) Drop the already written range (by the previous fadvise call)
              // 3) Handle nonpagealigned data.
              //    This is why we double the WRITE_BUFFER; the first time around the
              //    last (partial) page might be skipped, writeback will start only after
              //    second call; the third call will still include this page and finally
              //    drop it from cache.
              off_t headdrop = min(begin, off_t(WRITE_BUFFER * 2));
              posix_fadvise(fd, begin - headdrop, lastpos - begin + headdrop, POSIX_FADV_DONTNEED);
              }
           begin = lastpos = curpos;
           totwritten += written;
           written = 0;
           // The above fadvise() works when writing slowly (recording), but could
           // leave cached data around when writing at a high rate, e.g. when cutting,
           // because by the time we try to flush the cached pages (above) the data
           // can still be dirty - we are faster than the disk I/O.
           // So we do another round of flushing, just like above, but at larger
           // intervals -- this should catch any pages that couldn't be released
           // earlier.
           if (totwritten > MEGABYTE(32)) {
              // It seems in some setups, fadvise() does not trigger any I/O and
              // a fdatasync() call would be required do all the work (reiserfs with some
              // kind of write gathering enabled), but the syncs cause (io) load..
              // Uncomment the next line if you think you need them.
              //fdatasync(fd);
              off_t headdrop = min(off_t(curpos - totwritten), off_t(totwritten * 2));
              posix_fadvise(fd, curpos - totwritten - headdrop, totwritten + headdrop, POSIX_FADV_DONTNEED);
              totwritten = 0;
              }
           }
        }
#endif
     return bytesWritten;
     }
  return -1;
}

cUnbufferedFile *cUnbufferedFile::Create(const char *FileName, int Flags, mode_t Mode)
{
  cUnbufferedFile *File = new cUnbufferedFile;
  if (File->Open(FileName, Flags, Mode) < 0) {
     delete File;
     File = NULL;
     }
  return File;
}

int CloseVideoFile(cUnbufferedFile *File)
{
  int Result = File->Close();
  delete File;
  return Result;
}
// --- cReadLine -------------------------------------------------------------

cReadLine::cReadLine(void)
{
  size = 0;
  //buffer = NULL;
}

cReadLine::~cReadLine()
{
  //free(buffer);
}
#define MAXPARSEBUFFER KILOBYTE(10)

char *cReadLine::Read(FILE *f)
{
  static char buffer[MAXPARSEBUFFER];
  char *cp = fgets(buffer, sizeof(buffer), f);
  if (cp > 0) 
  {
     int n = strlen(cp);
     if (buffer[n] == '\n') 
	  {
        buffer[n] = 0;
        if (n > 0) 
		  {
           n--;
           if (buffer[n] == '\r')
              buffer[n] = 0;
        }
     }
     return buffer;
  }
  return NULL;
}
 
// --- cString ---------------------------------------------------------------

cString::cString(const char *S, bool TakePointer)
{
  s = TakePointer ? (char *)S : S ? strdup(S) : NULL;
}

cString::cString(const cString &String)
{
  s = String.s ? strdup(String.s) : NULL;
}

cString::~cString()
{
  free(s);
}

cString &cString::operator=(const cString &String)
{
  if (this == &String)
     return *this;
  free(s);
  s = String.s ? strdup(String.s) : NULL;
  return *this;
}

cString &cString::Truncate(int Index)
{
  int l = strlen(s);
  if (Index < 0)
     Index = l + Index;
  if (Index >= 0 && Index < l)
     s[Index] = 0;
  return *this;
}

cString cString::sprintf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char *buffer;
  if (!fmt || vasprintf(&buffer, fmt, ap) < 0) {
     esyslog("error in vasprintf('%s', ...)", fmt);
     buffer = strdup("???");
     }
  va_end(ap);
  return cString(buffer, true);
}

cString cString::sprintf(const char *fmt, va_list &ap)
{
  char *buffer;
  if (!fmt || vasprintf(&buffer, fmt, ap) < 0) {
     esyslog("error in vasprintf('%s', ...)", fmt);
     buffer = strdup("???");
     }
  return cString(buffer, true);
}

