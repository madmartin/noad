#include "vdr_cl.h"
char MarksfileSuffix[256] = MARKSFILESUFFIX;
 
bool setMarkfileName(const char *mname)
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
       sprintf(MarksfileSuffix,"%s",name);
     else
       sprintf(MarksfileSuffix,"/%s",name);
  }
  return 0;
}

void releaseMarkfileName()
{
  sprintf(MarksfileSuffix,"%s",MARKSFILESUFFIX);
}

// --- cFileName -------------------------------------------------------------
cFileName::cFileName(const char *FileName, bool Record, bool Blocking)
{
  file = -1;
  fileNumber = 0;
  record = Record;
  blocking = Blocking;
  // Prepare the file name:
  fileName = new char[strlen(FileName) + RECORDFILESUFFIXLEN];
  if (!fileName)
  {
    esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", fileName);
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

int cFileName::Open(void)
{
  if (file < 0)
  {
    int BlockingFlag = blocking ? 0 : O_NONBLOCK;
    if (record)
    {
      dsyslog(LOG_INFO, "recording to '%s'", fileName);
      file = open(fileName, BlockingFlag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      //file = OpenVideoFile(fileName, O_RDWR | O_CREAT | BlockingFlag);
      if (file < 0)
        LOG_ERROR_STR(fileName);
    }
    else
    {
      if (access(fileName, R_OK) == 0)
      {
        dsyslog(LOG_INFO, "playing '%s'", fileName);
        file = open(fileName, O_RDONLY | BlockingFlag);
        if (file < 0)
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
  if (file >= 0)
  {
    //if ((record && CloseVideoFile(file) < 0) || (!record && close(file) < 0))
    if( close(file) < 0 )
      LOG_ERROR_STR(fileName);
    file = -1;
  }
}

int cFileName::SetOffset(int Number, int Offset)
{
  if (fileNumber != Number)
    Close();
  if (0 < Number && Number <= MAXFILESPERRECORDING)
  {
    fileNumber = Number;
    sprintf(pFileNumber, RECORDFILESUFFIX, fileNumber);
    if (record)
    {
      if (access(fileName, F_OK) == 0) // file exists, let's try next suffix
        return SetOffset(Number + 1);
      else if (errno != ENOENT)
      { // something serious has happened
        LOG_ERROR_STR(fileName);
        return -1;
      }
      // found a non existing file suffix
    }
    if (Open() >= 0)
    {
      if (!record && Offset >= 0 && lseek(file, Offset, SEEK_SET) != Offset)
      {
        LOG_ERROR_STR(fileName);
        return -1;
      }
    }
    return file;
  }
  esyslog(LOG_ERR, "ERROR: max number of files (%d) exceeded", MAXFILESPERRECORDING);
  return -1;
}

int cFileName::NextFile(void)
{
  return SetOffset(fileNumber + 1);
}

// --- cResumeFile ------------------------------------------------------------

cResumeFile::cResumeFile(const char *FileName)
{
  fileName = new char[strlen(FileName) + strlen(RESUMEFILESUFFIX) + 1];
  if (fileName)
  {
    strcpy(fileName, FileName);
    strcat(fileName, RESUMEFILESUFFIX);
  }
  else
    esyslog(LOG_ERR, "ERROR: can't allocate memory for resume file name");
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

cIndexFile::cIndexFile(const char *FileName, bool Record)
    :resumeFile(FileName)
{
  f = -1;
  fileName = NULL;
  size = 0;
  last = -1;
  index = NULL;
  if (FileName)
  {
    fileName = new char[strlen(FileName) + strlen(INDEXFILESUFFIX) + 1];
    if (fileName)
    {
      strcpy(fileName, FileName);
      char *pFileExt = fileName + strlen(fileName);
      strcpy(pFileExt, INDEXFILESUFFIX);
      int delta = 0;
      if (access(fileName, R_OK) == 0)
      {
        struct stat buf;
        if (stat(fileName, &buf) == 0)
        {
          delta = buf.st_size % sizeof(tIndex);
          if (delta)
          {
            delta = sizeof(tIndex) - delta;
            esyslog(LOG_ERR, "ERROR: invalid file size (%ld) in '%s'", buf.st_size, fileName);
          }
          last = (buf.st_size + delta) / sizeof(tIndex) - 1;
          if (!Record && last >= 0)
          {
            size = last + 1;
            index = new tIndex[size];
            if (index)
            {
              f = open(fileName, O_RDONLY);
              if (f >= 0)
              {
                if ((int)safe_read(f, index, buf.st_size) != buf.st_size)
                {
                  esyslog(LOG_ERR, "ERROR: can't read from file '%s'", fileName);
                  delete [] index;
                  index = NULL;
                  close(f);
                  f = -1;
                }
                // we don't close f here, see CatchUp()!
              }
              else
                LOG_ERROR_STR(fileName);
            }
            else
              esyslog(LOG_ERR, "ERROR: can't allocate %d bytes for index '%s'", size * sizeof(tIndex), fileName);
          }
        }
        else
          LOG_ERROR;
      }
      else if (!Record)
        isyslog(LOG_INFO, "missing index file %s", fileName);
      if (Record)
      {
        if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
        {
          if (delta)
          {
            esyslog(LOG_ERR, "ERROR: padding index file with %d '0' bytes", delta);
            while (delta--)
              writechar(f, 0);
          }
        }
        else
          LOG_ERROR_STR(fileName);
      }
    }
    else
      esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", FileName);
  }
}

cIndexFile::~cIndexFile()
{
  if (f >= 0)
    close(f);
  delete [] fileName;
  delete [] index;
}

bool cIndexFile::CatchUp(int Index)
{
//  return false;
  if (index && f >= 0)
  {
    for (int i = 0; i <= MAXINDEXCATCHUP && (Index < 0 || Index >= last); i++)
    {
      struct stat buf;
      if (fstat(f, &buf) == 0)
      {
        int newLast = buf.st_size / sizeof(tIndex) - 1;
        if (newLast > last)
        {
          if (size <= newLast)
          {
            size *= 2;
            if (size <= newLast)
              size = newLast + 1;
          }
          index = (tIndex *)realloc(index, size * sizeof(tIndex));
          if (index)
          {
            int offset = (last + 1) * sizeof(tIndex);
            int delta = (newLast - last) * sizeof(tIndex);
            if (lseek(f, offset, SEEK_SET) == offset)
            {
              if (safe_read(f, &index[last + 1], delta) != delta)
              {
                esyslog(LOG_ERR, "ERROR: can't read from index");
                delete index;
                index = NULL;
                close(f);
                f = -1;
                break;
              }
              last = newLast;
            }
            else
              LOG_ERROR_STR(fileName);
          }
          else
            esyslog(LOG_ERR, "ERROR: can't realloc() index");
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

bool cIndexFile::Write(uchar PictureType, uchar FileNumber, int FileOffset)
{
  if (f >= 0)
  {
    tIndex i = { FileOffset, PictureType, FileNumber, 0 };
    if (safe_write(f, &i, sizeof(i)) < 0)
    {
      LOG_ERROR_STR(fileName);
      close(f);
      f = -1;
      return false;
    }
    last++;
  }
  return f >= 0;
}

bool cIndexFile::Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType, int *Length)
{
  if (index)
  {
    CatchUp(Index);
    if (Index >= 0 && Index <= last)
    {
      *FileNumber = index[Index].number;
      *FileOffset = index[Index].offset;
      if (PictureType)
        *PictureType = index[Index].type;
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
  dsyslog(LOG_INFO, "cIndexFile::Get returns false at %d (last is %d)",Index,last );
  return false;
}

int cIndexFile::GetNextIFrame(int Index, bool Forward, uchar *FileNumber, int *FileOffset, int *Length, bool StayOffEnd)
{
  if (index)
  {
    CatchUp();
    int d = Forward ? 1 : -1;
    for (;;)
    {
      Index += d;
      if (Index >= 0 && Index < last - ((Forward && StayOffEnd) ? 100 : 0))
      {
        if (index[Index].type == I_FRAME)
        {
          if (FileNumber)
            *FileNumber = index[Index].number;
          else
            FileNumber = &index[Index].number;
          if (FileOffset)
            *FileOffset = index[Index].offset;
          else
            FileOffset = &index[Index].offset;
          if (Length)
          {
            // all recordings end with a non-I_FRAME, so the following should be safe:
            int fn = index[Index + 1].number;
            int fo = index[Index + 1].offset;
            if (fn == *FileNumber)
              *Length = fo - *FileOffset;
            else
            {
              esyslog(LOG_ERR, "ERROR: 'I' frame at end of file #%d", *FileNumber);
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

int cIndexFile::Get(uchar FileNumber, int FileOffset)
{
  if (index)
  {
    CatchUp();
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

// --- cNoadIndexFile ------------------------------------------------------------
cNoadIndexFile::cNoadIndexFile(const char *FileName, bool Record):cIndexFile(FileName, Record)
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
            esyslog(LOG_ERR, "ERROR: invalid file size (%ld) in '%s'", buf.st_size, fileNameEx);
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
                  esyslog(LOG_ERR, "ERROR: can't read from file '%s'", fileName);
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
              esyslog(LOG_ERR, "ERROR: can't allocate %d bytes for index '%s'", size * sizeof(tIndexEx), fileNameEx);
          }
        }
        else
          LOG_ERROR;
      }
      else if (!Record)
        isyslog(LOG_INFO, "missing index file %s", fileNameEx);
      if (Record)
      {
        if ((fEx = open(fileNameEx, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
        {
          if (delta)
          {
            esyslog(LOG_ERR, "ERROR: padding index file with %d '0' bytes", delta);
            while (delta--)
              writechar(fEx, 0);
          }
        }
        else
          LOG_ERROR_STR(fileNameEx);
      }
    }
    else
      esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", FileName);
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

bool cNoadIndexFile::Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType, int *Length)
{
  if (index)
  {
    if (Index >= 0 && Index <= last)
    {
      *FileNumber = index[Index].number;
      *FileOffset = index[Index].offset;
      if (PictureType)
        *PictureType = index[Index].type;
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
  dsyslog(LOG_INFO, "cNoadIndexFile::Get returns false at %d (last is %d)",Index,last );
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

int cNoadIndexFile::GetNextIFrame(int Index, bool Forward, uchar *FileNumber, int *FileOffset, int *Length, bool StayOffEnd)
{
  if (index)
  {
    int d = Forward ? 1 : -1;
    for (;;)
    {
      Index += d;
      if (Index >= 0 && Index < last - ((Forward && StayOffEnd) ? 100 : 0))
      {
        if (index[Index].type == I_FRAME)
        {
          if (FileNumber)
            *FileNumber = index[Index].number;
          else
            FileNumber = &index[Index].number;
          if (FileOffset)
            *FileOffset = index[Index].offset;
          else
            FileOffset = &index[Index].offset;
          if (Length)
          {
            // all recordings end with a non-I_FRAME, so the following should be safe:
            int fn = index[Index + 1].number;
            int fo = index[Index + 1].offset;
            if (fn == *FileNumber)
              *Length = fo - *FileOffset;
            else
            {
              esyslog(LOG_ERR, "ERROR: 'I' frame at end of file #%d", *FileNumber);
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

   dsyslog(LOG_INFO, "cNoadIndexFile::logIndexEx() to file %s last is %d", logFileName, Last());

   if ((fLog = open(logFileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
   {
     //dsyslog(LOG_INFO, "cNoadIndexFile::logIndexEx() fLog is %d", fLog);
     for( int i = 0; i < Last(); i++)
     {
       char *buffer;
       tIndexEx *ti = &indexEx[i];
       asprintf(&buffer, "%06d %2d %3d %3d\n",i,ti->isLogo,ti->blackTop, ti->blackBottom);
       if (safe_write(fLog, buffer, strlen(buffer)) < 0)
       {
         LOG_ERROR_STR(logFileName);
       }
       //dsyslog(LOG_INFO, "cNoadIndexFile::save %p %d %d %d %d --> %s",ti, i,ti->isLogo,ti->blackTop, ti->blackBottom, buffer);
       delete buffer;
     }
     close(fLog);
   }
   else
     LOG_ERROR_STR(logFileName);
   dsyslog(LOG_INFO, "cNoadIndexFile::logIndexEx() fLog is %d", fLog);
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

char *cMark::buffer = NULL;

cMark::cMark(int Position, const char *Comment)
{
  position = Position;
  comment = Comment ? strdup(Comment) : NULL;
  checked = false;
}

cMark::~cMark()
{
  //delete comment;
  free(comment);
}

const char *cMark::ToText(bool bWithNewline)
{
  //delete buffer;
  free(buffer);
  asprintf(&buffer, "%s%s%s%s", IndexToHMSF(position, true), comment ? " " : "", comment ? comment : "",bWithNewline?"\n":"" );
  return buffer;
}

bool cMark::Parse(const char *s)
{
  delete comment;
  comment = NULL;
  position = HMSFToIndex(s);
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
  return fprintf(f, "%s", ToText()) > 0;
}


// --- cMarks ----------------------------------------------------------------

bool cMarks::Load(const char *RecordingFileName, bool AllowComments)
{
  const char *MarksFile = AddDirectory(RecordingFileName, MarksfileSuffix);
  if (cConfig<cMark>::Load(MarksFile))
  {
    Sort();
    return true;
  }
  return false;
}

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
    cConfig<cMark>::Add(m = new cMark(Position));
    dsyslog(LOG_INFO, "cMarks::Add(%d) is %s",Position, m->ToText());
    Sort();
  }
  //  dsyslog(LOG_INFO, "cMarks::Add(%d) is %s",Position, m->ToText());
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

const char *IndexToHMSF(int Index, bool WithFrame)
{
  static char buffer[16];
  int f = (Index % FRAMESPERSEC) + 1;
  int s = (Index / FRAMESPERSEC);
  int m = s / 60 % 60;
  int h = s / 3600;
  s %= 60;
  snprintf(buffer, sizeof(buffer), WithFrame ? "%d:%02d:%02d.%02d" : "%d:%02d:%02d", h, m, s, f);
  return buffer;
}

int HMSFToIndex(const char *HMSF)
{
  int h, m, s, f = 0;
  if (3 <= sscanf(HMSF, "%d:%d:%d.%d", &h, &m, &s, &f))
    return (h * 3600 + m * 60 + s) * FRAMESPERSEC + f - 1;
  return 0;
}

