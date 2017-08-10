/*
 * tools.c: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 1.62 2002/03/31 20:51:06 kls Exp $
 */

/*
using parts of stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
*/

#include "tools.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <cxxabi.h>

extern int SysLogLevel;

ssize_t safe_read(int filedes, void *buffer, size_t size)
{
  for (;;)
  {
    ssize_t p = read(filedes, buffer, size);
    if (p < 0 && errno == EINTR)
    {
      dsyslog("EINTR while reading from file handle %d - retrying", filedes);
      continue;
    }
    return p;
  }
}

ssize_t safe_write(int filedes, const void *buffer, size_t size)
{
  ssize_t p = 0;
  ssize_t written = size;
  const unsigned char *ptr = (const unsigned char *)buffer;
  while (size > 0)
  {
    p = write(filedes, ptr, size);
    if (p < 0)
    {
      if (errno == EINTR)
      {
        dsyslog("EINTR while writing to file handle %d - retrying", filedes);
        continue;
      }
      break;
    }
    ptr  += p;
    size -= p;
  }
  return p < 0 ? p : written;
}

void writechar(int filedes, char c)
{
  safe_write(filedes, &c, sizeof(c));
}

char *readline(FILE *f)
{
  static char buffer[MAXPARSEBUFFER];
  if (fgets(buffer, sizeof(buffer), f) > 0)
  {
    int l = strlen(buffer) - 1;
    if (l >= 0 && buffer[l] == '\n')
      buffer[l] = 0;
    return buffer;
  }
  return NULL;
}

char *strcpyrealloc(char *dest, const char *src)
{
  if (src)
  {
    int l = max(dest ? strlen(dest) : 0, strlen(src)) + 1; // don't let the block get smaller!
    dest = (char *)realloc(dest, l);
    if (dest)
      strcpy(dest, src);
    else
      esyslog( "ERROR: out of memory");
  }
  else
  {
    delete dest;
    dest = NULL;
  }
  return dest;
}

char *strn0cpy(char *dest, const char *src, size_t n)
{
  char *s = dest;
  for ( ; --n && (*dest = *src) != 0; dest++, src++) ;
  *dest = 0;
  return s;
}

char *strreplace(char *s, char c1, char c2)
{
  char *p = s;

  while (p && *p)
  {
    if (*p == c1)
      *p = c2;
    p++;
  }
  return s;
}

char *strreplace(char *s, const char *s1, const char *s2)
{
  char *p = strstr(s, s1);
  if (p)
  {
    int of = p - s;
    int l  = strlen(s);
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    if (l2 > l1)
      s = (char *)realloc(s, strlen(s) + l2 - l1 + 1);
    if (l2 != l1)
      memmove(s + of + l2, s + of + l1, l - of - l1 + 1);
    strncpy(s + of, s2, l2);
  }
  return s;
}

char *skipspace(const char *s)
{
  while (*s && isspace(*s))
    s++;
  return (char *)s;
}

char *stripspace(char *s)
{
  if (s && *s)
  {
    for (char *p = s + strlen(s) - 1; p >= s; p--)
    {
      if (!isspace(*p))
        break;
      *p = 0;
    }
  }
  return s;
}

char *compactspace(char *s)
{
  if (s && *s)
  {
    char *t = stripspace(skipspace(s));
    char *p = t;
    while (p && *p)
    {
      char *q = skipspace(p);
      if (q - p > 1)
        memmove(p + 1, q, strlen(q) + 1);
      p++;
    }
    if (t != s)
      memmove(s, t, strlen(t) + 1);
  }
  return s;
}

const char *strescape(const char *s, const char *chars)
{
  static char *buffer = NULL;
  const char *p = s;
  char *t = NULL;
  while (*p)
  {
    if (strchr(chars, *p))
    {
      if (!t)
      {
        buffer = (char *)realloc(buffer, 2 * strlen(s) + 1);
        t = buffer + (p - s);
        s = strcpy(buffer, s);
      }
      *t++ = '\\';
    }
    if (t)
      *t++ = *p;
    p++;
  }
  if (t)
    *t = 0;
  return s;
}

bool startswith(const char *s, const char *p)
{
  while (*p)
  {
    if (*p++ != *s++)
      return false;
  }
  return true;
}

bool endswith(const char *s, const char *p)
{
  const char *se = s + strlen(s) - 1;
  const char *pe = p + strlen(p) - 1;
  while (pe >= p)
  {
    if (*pe-- != *se-- || (se < s && pe >= p))
      return false;
  }
  return true;
}

bool isempty(const char *s)
{
  return !(s && *skipspace(s));
}

int time_ms(void)
{
  static time_t t0 = 0;
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
  {
    if (t0 == 0)
      t0 = t.tv_sec; // this avoids an overflow (we only work with deltas)
    return (t.tv_sec - t0) * 1000 + t.tv_usec / 1000;
  }
  return 0;
}

void delay_ms(int ms)
{
  int t0 = time_ms();
  while (time_ms() - t0 < ms)
    ;
}

bool isnumber(const char *s)
{
  while (*s)
  {
    if (!isdigit(*s))
      return false;
    s++;
  }
  return true;
}

//cString AddDirectory(const char *DirName, const char *FileName)
const char *AddDirectory(const char *DirName, const char *FileName)
{
  static char *buf = NULL;
  delete buf;
  asprintf(&buf, "%s/%s", DirName && *DirName ? DirName : ".", FileName);
  return buf;
}

int FreeDiskSpaceMB(const char *Directory, int *UsedMB)
{
  if (UsedMB)
    *UsedMB = 0;
  int Free = 0;
  struct statfs statFs;
  if (statfs(Directory, &statFs) == 0)
  {
    int blocksPerMeg = 1024 * 1024 / statFs.f_bsize;
    if (UsedMB)
      *UsedMB = (statFs.f_blocks - statFs.f_bfree) / blocksPerMeg;
    Free = statFs.f_bavail / blocksPerMeg;
  }
  else
    LOG_ERROR_STR(Directory);
  return Free;
}

bool DirectoryOk(const char *DirName, bool LogErrors)
{
  struct stat ds;
  if (stat(DirName, &ds) == 0)
  {
    if (S_ISDIR(ds.st_mode))
    {
      if (access(DirName, R_OK | W_OK | X_OK) == 0)
        return true;
      else if (LogErrors)
        esyslog( "ERROR: can't access %s", DirName);
    }
    else if (LogErrors)
      esyslog( "ERROR: %s is not a directory", DirName);
  }
  else if (LogErrors)
    LOG_ERROR_STR(DirName);
  return false;
}

bool MakeDirs(const char *FileName, bool IsDirectory)
{
  bool result = true;
  char *s = strdup(FileName);
  char *p = s;
  if (*p == '/')
    p++;
  while ((p = strchr(p, '/')) != NULL || IsDirectory)
  {
    if (p)
      *p = 0;
    struct stat fs;
    if (stat(s, &fs) != 0 || !S_ISDIR(fs.st_mode))
    {
      dsyslog( "creating directory %s", s);
      if (mkdir(s, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
      {
        LOG_ERROR_STR(s);
        result = false;
        break;
      }
    }
    if (p)
      *p++ = '/';
    else
      break;
  }
  delete s;
  return result;
}

bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks)
{
  struct stat st;
  if (stat(FileName, &st) == 0)
  {
    if (S_ISDIR(st.st_mode))
    {
      DIR *d = opendir(FileName);
      if (d)
      {
        struct dirent *e;
        while ((e = readdir(d)) != NULL)
        {
          if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
          {
            char *buffer;
            asprintf(&buffer, "%s/%s", FileName, e->d_name);
            if (FollowSymlinks)
            {
              int size = strlen(buffer) * 2; // should be large enough
              char *l = new char[size];
              int n = readlink(buffer, l, size);
              if (n < 0)
              {
                if (errno != EINVAL)
                  LOG_ERROR_STR(buffer);
              }
              else if (n < size)
              {
                l[n] = 0;
                dsyslog( "removing %s", l);
                if (remove(l) < 0)
                  LOG_ERROR_STR(l);
              }
              else
                esyslog( "ERROR: symlink name length (%d) exceeded anticipated buffer size (%d)", n, size);
              delete l;
            }
            dsyslog( "removing %s", buffer);
            if (remove(buffer) < 0)
              LOG_ERROR_STR(buffer);
            delete buffer;
          }
        }
        closedir(d);
      }
      else
      {
        LOG_ERROR_STR(FileName);
        return false;
      }
    }
    dsyslog( "removing %s", FileName);
    if (remove(FileName) < 0)
    {
      LOG_ERROR_STR(FileName);
      return false;
    }
  }
  else if (errno != ENOENT)
  {
    LOG_ERROR_STR(FileName);
    return false;
  }
  return true;
}

bool RemoveEmptyDirectories(const char *DirName, bool RemoveThis)
{
  DIR *d = opendir(DirName);
  if (d)
  {
    bool empty = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
    {
      if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") && strcmp(e->d_name, "lost+found"))
      {
        char *buffer;
        asprintf(&buffer, "%s/%s", DirName, e->d_name);
        struct stat st;
        if (stat(buffer, &st) == 0)
        {
          if (S_ISDIR(st.st_mode))
          {
            if (!RemoveEmptyDirectories(buffer, true))
              empty = false;
          }
          else
            empty = false;
        }
        else
        {
          LOG_ERROR_STR(buffer);
          delete buffer;
          return false;
        }
        delete buffer;
      }
    }
    closedir(d);
    if (RemoveThis && empty)
    {
      dsyslog("removing %s", DirName);
      if (remove(DirName) < 0)
      {
        LOG_ERROR_STR(DirName);
        return false;
      }
    }
    return empty;
  }
  else
    LOG_ERROR_STR(DirName);
  return false;
}

char *ReadLink(const char *FileName)
{
  char RealName[_POSIX_PATH_MAX];
  const char *TargetName = NULL;
  int n = readlink(FileName, RealName, sizeof(RealName) - 1);
  if (n < 0)
  {
    if (errno == ENOENT || errno == EINVAL) // file doesn't exist or is not a symlink
      TargetName = FileName;
    else // some other error occurred
      LOG_ERROR_STR(FileName);
  }
  else if (n < int(sizeof(RealName)))
  { // got it!
    RealName[n] = 0;
    TargetName = RealName;
  }
  else
    esyslog( "ERROR: symlink's target name too long: %s", FileName);
  return TargetName ? strdup(TargetName) : NULL;
}

bool SpinUpDisk(const char *FileName)
{
  static char *buf = NULL;
  for (int n = 0; n < 10; n++)
  {
    delete buf;
    if (DirectoryOk(FileName))
      asprintf(&buf, "%s/vdr-%06d", *FileName ? FileName : ".", n);
    else
      asprintf(&buf, "%s.vdr-%06d", FileName, n);
    if (access(buf, F_OK) != 0)
    { // the file does not exist
      timeval tp1, tp2;
      gettimeofday(&tp1, NULL);
      int f = open(buf, O_WRONLY | O_CREAT, DEFFILEMODE);
      // O_SYNC doesn't work on all file systems
      if (f >= 0)
      {
        close(f);
        system("sync");
        remove(buf);
        gettimeofday(&tp2, NULL);
        double seconds = (((long long)tp2.tv_sec * 1000000 + tp2.tv_usec) - ((long long)tp1.tv_sec * 1000000 + tp1.tv_usec)) / 1000000.0;
        if (seconds > 0.5)
          dsyslog( "SpinUpDisk took %.2f seconds\n", seconds);
        return true;
      }
      else
        LOG_ERROR_STR(buf);
    }
  }
  esyslog( "ERROR: SpinUpDisk failed");
  return false;
}

const char *WeekDayName(int WeekDay)
{
  static char buffer[4];
  WeekDay = WeekDay == 0 ? 6 : WeekDay - 1; // we start with monday==0!
  if (0 <= WeekDay && WeekDay <= 6)
  {
    //     const char *day = tr("MonTueWedThuFriSatSun");
    const char *day = "MonTueWedThuFriSatSun";
    day += WeekDay * 3;
    strncpy(buffer, day, 3);
    return buffer;
  }
  else
    return "???";
}

const char *DayDateTime(time_t t)
{
  static char buffer[32];
  if (t == 0)
    time(&t);
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  snprintf(buffer, sizeof(buffer), "%s %2d.%02d %02d:%02d", WeekDayName(tm->tm_wday), tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min);
  return buffer;
}

const char *secToTime(int sec)
{
  static char buffer[16];
  int m = sec / 60 % 60;
  int h = sec / 3600;
  int s = sec%60;
  snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", h, m, s);
  return buffer;
}

// --- cFile -----------------------------------------------------------------
/*
bool cFile::files[FD_SETSIZE] = { false };
int cFile::maxFiles = 0;

cFile::cFile(void)
{
  f = -1;
}

cFile::~cFile()
{
  Close();
}

bool cFile::Open(const char *FileName, int Flags, mode_t Mode)
{
  if (!IsOpen())
    return Open(open(FileName, Flags, Mode));
  esyslog( "ERROR: attempt to re-open %s", FileName);
  return false;
}

bool cFile::Open(int FileDes)
{
  if (FileDes >= 0)
  {
    if (!IsOpen())
    {
      f = FileDes;
      if (f >= 0)
      {
        if (f < FD_SETSIZE)
        {
          if (f >= maxFiles)
            maxFiles = f + 1;
          if (!files[f])
            files[f] = true;
          else
            esyslog( "ERROR: file descriptor %d already in files[]", f);
          return true;
        }
        else
          esyslog( "ERROR: file descriptor %d is larger than FD_SETSIZE (%d)", f, FD_SETSIZE);
      }
    }
    else
      esyslog( "ERROR: attempt to re-open file descriptor %d", FileDes);
  }
  return false;
}

void cFile::Close(void)
{
  if (f >= 0)
  {
    close(f);
    files[f] = false;
    f = -1;
  }
}

bool cFile::Ready(bool Wait)
{
  return f >= 0 && AnyFileReady(f, Wait ? 1000 : 0);
}

bool cFile::AnyFileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  FD_ZERO(&set);
  for (int i = 0; i < maxFiles; i++)
  {
    if (files[i])
      FD_SET(i, &set);
  }
  if (0 <= FileDes && FileDes < FD_SETSIZE && !files[FileDes])
    FD_SET(FileDes, &set); // in case we come in with an arbitrary descriptor
  if (TimeoutMs == 0)
    TimeoutMs = 10; // load gets too heavy with 0
  struct timeval timeout;
  timeout.tv_sec  = TimeoutMs / 1000;
  timeout.tv_usec = (TimeoutMs % 1000) * 1000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && (FileDes < 0 || FD_ISSET(FileDes, &set));
}

bool cFile::FileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs < 100)
    TimeoutMs = 100;
  timeout.tv_sec  = TimeoutMs / 1000;
  timeout.tv_usec = (TimeoutMs % 1000) * 1000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && FD_ISSET(FileDes, &set);
}

bool cFile::FileReadyForWriting(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs < 100)
    TimeoutMs = 100;
  timeout.tv_sec  = 0;
  timeout.tv_usec = TimeoutMs * 1000;
  return select(FD_SETSIZE, NULL, &set, NULL, &timeout) > 0 && FD_ISSET(FileDes, &set);
}
*/
// --- cSafeFile -------------------------------------------------------------

cSafeFile::cSafeFile(const char *FileName)
{
  f = NULL;
  fileName = ReadLink(FileName);
  tempName = fileName ? new char[strlen(fileName) + 5] : NULL;
  if (tempName)
    strcat(strcpy(tempName, fileName), ".$$$");
}

cSafeFile::~cSafeFile()
{
  if (f)
    fclose(f);
  unlink(tempName);
  //delete fileName;
  free(fileName);
  //delete tempName;
  //free(tempName);
  delete [] tempName;
}

bool cSafeFile::Open(void)
{
  if (!f && fileName && tempName)
  {
    f = fopen(tempName, "w");
    if (!f)
      LOG_ERROR_STR(tempName);
  }
  return f != NULL;
}

bool cSafeFile::Close(void)
{
  bool result = true;
  if (f)
  {
    if (ferror(f) != 0)
    {
      LOG_ERROR_STR(tempName);
      result = false;
    }
    if (fclose(f) < 0)
    {
      LOG_ERROR_STR(tempName);
      result = false;
    }
    f = NULL;
	 if( result )
		 remove(fileName);
    if (result && rename(tempName, fileName) < 0)
    {
      LOG_ERROR_STR(fileName);
      result = false;
    }
  }
  else
    result = false;
  return result;
}

// --- cLockFile -------------------------------------------------------------

/*
#define LOCKFILENAME      ".lock-vdr"
#define LOCKFILESTALETIME 600 // seconds before considering a lock file "stale"

cLockFile::cLockFile(const char *Directory)
{
  fileName = NULL;
  f = -1;
  if (DirectoryOk(Directory))
    asprintf(&fileName, "%s/%s", Directory, LOCKFILENAME);
}

cLockFile::~cLockFile()
{
  Unlock();
  delete fileName;
}

bool cLockFile::Lock(int WaitSeconds)
{
  if (f < 0 && fileName)
  {
    time_t Timeout = time(NULL) + WaitSeconds;
    do
    {
      f = open(fileName, O_WRONLY | O_CREAT | O_EXCL, DEFFILEMODE);
      if (f < 0)
      {
        if (errno == EEXIST)
        {
          struct stat fs;
          if (stat(fileName, &fs) == 0)
          {
            if (time(NULL) - fs.st_mtime > LOCKFILESTALETIME)
            {
              esyslog( "ERROR: removing stale lock file '%s'", fileName);
              if (remove(fileName) < 0)
              {
                LOG_ERROR_STR(fileName);
                break;
              }
              continue;
            }
          }
          else if (errno != ENOENT)
          {
            LOG_ERROR_STR(fileName);
            break;
          }
        }
        else
        {
          LOG_ERROR_STR(fileName);
          break;
        }
        if (WaitSeconds)
          sleep(1);
      }
    }
    while (f < 0 && time(NULL) < Timeout);
  }
  return f >= 0;
}

void cLockFile::Unlock(void)
{
  if (f >= 0)
  {
    close(f);
    remove(fileName);
    f = -1;
  }
  else
    esyslog( "ERROR: attempt to unlock %s without holding a lock!", fileName);
}
*/
// --- cListObject -----------------------------------------------------------

cListObject::cListObject(void)
{
  prev = next = NULL;
}

cListObject::~cListObject()
{}

void cListObject::Append(cListObject *Object)
{
  next = Object;
  Object->prev = this;
}

void cListObject::Unlink(void)
{
  if (next)
    next->prev = prev;
  if (prev)
    prev->next = next;
  next = prev = NULL;
}

int cListObject::Index(void)
{
  cListObject *p = prev;
  int i = 0;

  while (p)
  {
    i++;
    p = p->prev;
  }
  return i;
}

// --- cListBase -------------------------------------------------------------

cListBase::cListBase(void)
{
  objects = lastObject = NULL;
}

cListBase::~cListBase()
{
  Clear();
}

void cListBase::Add(cListObject *Object)
{
  if (lastObject)
    lastObject->Append(Object);
  else
    objects = Object;
  lastObject = Object;
}

void cListBase::Del(cListObject *Object)
{
  if (Object == objects)
    objects = Object->Next();
  if (Object == lastObject)
    lastObject = Object->Prev();
  Object->Unlink();
  delete Object;
}

void cListBase::Move(int From, int To)
{
  Move(Get(From), Get(To));
}

void cListBase::Move(cListObject *From, cListObject *To)
{
  if (From && To)
  {
    if (From->Index() < To->Index())
      To = To->Next();
    if (From == objects)
      objects = From->Next();
    if (From == lastObject)
      lastObject = From->Prev();
    From->Unlink();
    if (To)
    {
      if (To->Prev())
        To->Prev()->Append(From);
      From->Append(To);
    }
    else
    {
      lastObject->Append(From);
      lastObject = From;
    }
    if (!From->Prev())
      objects = From;
  }
}

void cListBase::Clear(void)
{
  while (objects)
  {
    cListObject *object = objects->Next();
    delete objects;
    objects = object;
  }
  objects = lastObject = NULL;
}

cListObject *cListBase::Get(int Index) const
{
  if (Index < 0)
    return NULL;
  cListObject *object = objects;
  while (object && Index-- > 0)
    object = object->Next();
  return object;
}

int cListBase::Count(void) const
{
  int n = 0;
  cListObject *object = objects;

  while (object)
  {
    n++;
    object = object->Next();
  }
  return n;
}

void cListBase::Sort(void)
{
  bool swapped;
  do
  {
    swapped = false;
    cListObject *object = objects;
    while (object)
    {
      if (object->Next() && *object->Next() < *object)
      {
        Move(object->Next(), object);
        swapped = true;
      }
      object = object->Next();
    }
  }
  while (swapped);
}

// --- ReadFrame -------------------------------------------------------------

int ReadFrame(int f, unsigned char *b, int Length, int Max)
{
  if (Length == -1)
    Length = Max; // this means we read up to EOF (see cIndex)
  else if (Length > Max)
  {
    esyslog( "ERROR: frame larger than buffer (%d > %d)", Length, Max);
    Length = Max;
  }
  if( f <= 0 )
  {
    esyslog( "ERROR: illegal file-handle %d", f);
    esyslog( "maybe index.vdr is corrupted");
    esyslog( "noad will be killed now ");
    raise(SIGABRT);
  }
  int r = safe_read(f, b, Length);
  //int r = read( f,b,Length);
  if (r < 0)
  {
    LOG_ERROR;
    // nothing more can be done
    // just kill myself
    esyslog( "noad will be killed now (%d, %p, %d,%d)",f,b,Length,Max);
    raise(SIGABRT);
  }
  return r;
}

#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF

int getVStreamID(int f)
{
  unsigned char buf[8192];
  int r = read( f,buf,8192);
  int i = 0;
  int found = 0;
  int iRet = VIDEO_STREAM_S;
  while( i < r )
  {
    switch(found)
    {
        case 0:
        if( buf[i] == 0 )
          found++;
        break;

        case 1:
        if( buf[i] == 0 )
          found++;
	  else
	    found = 0;
        break;

        case 2:
        if( buf[i] == 1 )
          found++;
	else
	  found = 0;
        break;

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

int getTSPID(int f)
{
  unsigned char buf[9400];// 50 TS-Pakete
  lseek(f,0,SEEK_SET);
  int r = read( f,buf,9400);
  int i = 0;
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

int isHDTV(int f)
{
  unsigned char buf[8192];
  int r = read( f,buf,8192);
  int i = 0;
  int found = 0;
  int iRet = 1;
  while( i < r )
  {
    switch(found)
    {
        case 0:
          if( buf[i] == 0 )
            found++;
	break;
	
        case 1:
          if( buf[i] == 0 )
            found++;
	  else
	    found = 0;
        break;

        case 2:
          if( buf[i] == 1 )
            found++;
	  else 
	    found = 0;
        break;

        case 3:
          if( buf[i] == 0xB3 )
          {
            iRet = 0;
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

int make_pidfile(const char *dirname)
{
  char *pidfilename = new char[ strlen(dirname)+10 ];
  strcpy(pidfilename,dirname);
  strcat(pidfilename, "/noad.pid");
  int pidf = open(pidfilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if( pidf < 0 )
  {
    if( errno == EEXIST )
    {
      fprintf(stderr,"There is already running an instance of noad for this recording!\n");
      fprintf(stderr,"If you feel that this is not true, you have to delete the file\n");
      fprintf(stderr,"%s manually\n",pidfilename);
      esyslog( "There is already running an instance of noad for this recording!");
      esyslog( "If you feel that this is not true, you have to delete the file");
      esyslog( "%s manually",pidfilename);
      delete [] pidfilename;
      return -1;
    }
    else
    {
      int oldErrno = errno;
      esyslog( "can't create pidfile: %m");
      errno = oldErrno;
      perror("can't create pidfile!");
      delete [] pidfilename;
      return -2;
    }
  }
  sprintf(pidfilename,"%d",getpid());
  write(pidf,pidfilename,strlen(pidfilename));
  close(pidf);
  delete [] pidfilename;
  return 0;
}

pid_t processInfo(const char *dirname)
{
  pid_t pidRet = -1;
  char *pidfilename = new char[ strlen(dirname)+10 ];
  strcpy(pidfilename,dirname);
  strcat(pidfilename, "/noad.pid");
  int pidf = open(pidfilename, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if( pidf < 0 )
  {
    int oldErrno = errno;
    esyslog( "can't open pidfile: %m");
    errno = oldErrno;
    perror("can't open pidfile");
    delete [] pidfilename;
    return -1;
  }
  char buffer[20];
  int br = read(pidf,buffer,20);
  close(pidf);
  if( br > 0 )
  {
    pidRet = atol(buffer);
    buffer[br] = 0;
    char *cmd=NULL;
    asprintf(&cmd,"ps -p %s -o start,time,cmd --no-heading",buffer);
    FILE *p = popen(cmd, "r");
    if (p)
    {
      char *s;
      if ((s = readline(p)) != NULL)
      {
        esyslog("pid info for %s: %s",buffer,s);
      }
      pclose(p);
    }
    else
      esyslog( "Error while opening pipe (%s)",cmd);
    free(cmd);
  }
  delete [] pidfilename;
  return pidRet;
}

int rm_pidfile(const char *dirname)
{
  char *pidfilename = new char[ strlen(dirname)+10 ];
  strcpy(pidfilename,dirname);
  strcat(pidfilename, "/noad.pid");
  unlink(pidfilename);
  delete [] pidfilename;
  return 0;
}

void dump2log (unsigned char * buf, unsigned char * end )
{
  char linebuff[120];
  char *dst = linebuff;
  int i = 20;
  while( buf < end )
  {
    int sp = sprintf(dst,"%.2X ",*buf);
    buf++;
    dst += sp;
    if(!--i)
    {
      dst = linebuff;
      i = 20;
      syslog(LOG_ERR, "dump: %s",dst);
    }
  }
  if(i!= 20)
  {
    dst = linebuff;
    i = 20;
    syslog(LOG_ERR, "dump: %s",dst);
  }
}

/** Print a demangled stack backtrace of the caller function to FILE* out. */
static inline void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 32)
{
    fprintf(out, "stack trace:\n");

    // storage array for stack trace address data
    void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if (addrlen == 0) {
   fprintf(out, "  <empty, possibly corrupt>\n");
   return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
   char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

   // find parentheses and +address offset surrounding the mangled name:
   // ./module(function+0x15c) [0x8048a6d]
   for (char *p = symbollist[i]; *p; ++p)
   {
       if (*p == '(')
      begin_name = p;
       else if (*p == '+')
      begin_offset = p;
       else if (*p == ')' && begin_offset) {
      end_offset = p;
      break;
       }
   }

   if (begin_name && begin_offset && end_offset
       && begin_name < begin_offset)
   {
       *begin_name++ = '\0';
       *begin_offset++ = '\0';
       *end_offset = '\0';

       // mangled name is now in [begin_name, begin_offset) and caller
       // offset in [begin_offset, end_offset). now apply
       // __cxa_demangle():

       int status;
       char* ret = abi::__cxa_demangle(begin_name,
                   funcname, &funcnamesize, &status);
       if (status == 0) {
      funcname = ret; // use possibly realloc()-ed string
      fprintf(out, "  %s : %s+%s\n",
         symbollist[i], funcname, begin_offset);
       }
       else {
      // demangling failed. Output function name as a C function with
      // no arguments.
      fprintf(out, "  %s : %s()+%s\n",
         symbollist[i], begin_name, begin_offset);
       }
   }
   else
   {
       // couldn't parse the line? print the whole line.
       fprintf(out, "  %s\n", symbollist[i]);
   }
    }

    free(funcname);
    free(symbollist);
}

void show_stackframe(bool bFork)
{
  void *trace[32];
  char **messages = (char **)NULL;
  int i, trace_size = 0;

  trace_size = backtrace(trace, 32);
  messages = backtrace_symbols(trace, trace_size);
  if (!bFork)
    fprintf(stderr, "[bt] Execution path:\n");
  syslog(LOG_INFO, "[bt] Execution path:");

  // allocate string which will be filled with the demangled function name
  size_t funcnamesize = 256;
  char* funcname = (char*)malloc(funcnamesize);

  // iterate over the returned symbol lines. skip the first, it is the
  // address of this function.
  for (int i = 1; i < trace_size; i++)
  {
     char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

    // find parentheses and +address offset surrounding the mangled name:
    // ./module(function+0x15c) [0x8048a6d]
    for (char *p = messages[i]; *p; ++p)
    {
        if (*p == '(')
          begin_name = p;
        else if (*p == '+')
          begin_offset = p;
        else if (*p == ')' && begin_offset)
        {
          end_offset = p;
          break;
        }
    }

    if (begin_name && begin_offset && end_offset && begin_name < begin_offset)
    {
       *begin_name++ = '\0';
       *begin_offset++ = '\0';
       *end_offset = '\0';

       // mangled name is now in [begin_name, begin_offset) and caller
       // offset in [begin_offset, end_offset). now apply
       // __cxa_demangle():

       int status;
       char* ret = abi::__cxa_demangle(begin_name,
                 funcname, &funcnamesize, &status);
       if (status == 0)
       {
         funcname = ret; // use possibly realloc()-ed string
         if (!bFork)
           fprintf(stderr, "[bt] %s: %s+%s\n", messages[i], funcname, begin_offset);
         syslog(LOG_INFO, "[bt] %s: %s+%s\n", messages[i], funcname, begin_offset);
     }
     else
     {
        // demangling failed. Output function name as a C function with
        // no arguments.
        if (!bFork)
           fprintf(stderr, "[bt]  %s : %s()+%s\n", messages[i], begin_name, begin_offset);
        syslog(LOG_INFO, "[bt] %s: %s()+%s\n", messages[i], begin_name, begin_offset);
     }
    }
    else
    {
        // couldn't parse the line? print the whole line.
        if (!bFork)
           fprintf(stderr, "[bt] %s\n", messages[i]);
        syslog(LOG_INFO, "[bt] %s\n", messages[i]);
    }
  }

  free(funcname);
  free(messages);
  /*
  for (i=0; i<trace_size; ++i)
  {
    if (!bFork)
      fprintf(stderr, "[bt] %s\n", messages[i]);
    syslog(LOG_INFO, "[bt] %s\n", messages[i]);
  }
  */
}

#define MAXSYSLOGBUF 256

void syslog_with_tid(int priority, const char *format, ...)
{
  va_list ap;
  char fmt[MAXSYSLOGBUF];
  //snprintf(fmt, sizeof(fmt), "[%d] %s", cThread::ThreadId(), format);
  snprintf(fmt, sizeof(fmt), "[%d] %s", getpid(), format);
  va_start(ap, format);
  vsyslog(priority, fmt, ap);
  va_end(ap);
}

