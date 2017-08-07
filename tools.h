/*
 * tools.h: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.42 2002/02/17 12:57:44 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

//#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
extern int SysLogLevel;

#if (HAVE_POSIX_FADVISE==1 )
#define USE_FADVISE
#endif
#define esyslog(a...) void( (SysLogLevel > 0) ? syslog_with_tid(LOG_ERR, a) : void() )
#define isyslog(a...) void( (SysLogLevel > 1) ? syslog_with_tid(LOG_ERR, a) : void() )
#define dsyslog(a...) void( (SysLogLevel > 2) ? syslog_with_tid(LOG_ERR, a) : void() )
#define xsyslog(a...) void( (SysLogLevel & 8) ? syslog_with_tid(LOG_ERR, a) : void() )
void syslog_with_tid(int priority, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

#define LOG_ERROR         esyslog("ERROR (%s,%d): %m", __FILE__, __LINE__)
#define LOG_ERROR_STR(s)  esyslog("ERROR: %s: %m", s)
//#define LOG_ERROR         
//#define LOG_ERROR_STR(s)  

#define SECSINDAY  86400

#define KILOBYTE(n) ((n) * 1024)
#define MEGABYTE(n) ((n) * 1024 * 1024)

#define MAXPARSEBUFFER KILOBYTE(10)

#define DELETENULL(p) (delete (p), p = NULL)

#define MALLOC(type, size)  (type *)malloc(sizeof(type) * (size))

template<class T> inline T min(T a, T b) { return a <= b ? a : b; }
template<class T> inline T max(T a, T b) { return a >= b ? a : b; }
template<class T> inline void swap(T &a, T &b) { T t = a; a = b; b = t; }


class cString {
private:
  char *s;
public:
  cString(const char *S = NULL, bool TakePointer = false);
  cString(const cString &String);
  virtual ~cString();
  operator const void * () const { return s; } // to catch cases where operator*() should be used
  operator const char * () const { return s; } // for use in (const char *) context
  const char * operator*() const { return s; } // for use in (const void *) context (printf() etc.)
  cString &operator=(const cString &String);
  cString &Truncate(int Index); ///< Truncate the string at the given Index (if Index is < 0 it is counted from the end of the string).
  static cString sprintf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
  static cString sprintf(const char *fmt, va_list &ap);
  };


ssize_t safe_read(int filedes, void *buffer, size_t size);
ssize_t safe_write(int filedes, const void *buffer, size_t size);
void writechar(int filedes, char c);
char *readline(FILE *f);
char *strcpyrealloc(char *dest, const char *src);
char *strn0cpy(char *dest, const char *src, size_t n);
char *strreplace(char *s, char c1, char c2);
char *strreplace(char *s, const char *s1, const char *s2); // re-allocates 's' and deletes the original string if necessary!
char *skipspace(const char *s);
char *stripspace(char *s);
char *compactspace(char *s);
const char *strescape(const char *s, const char *chars); // returns a statically allocated string!
bool startswith(const char *s, const char *p);
bool endswith(const char *s, const char *p);
bool isempty(const char *s);
int time_ms(void);
void delay_ms(int ms);
const char *secToTime(int sec);
bool isnumber(const char *s);
cString AddDirectory(const char *DirName, const char *FileName); // returns a statically allocated string!
int FreeDiskSpaceMB(const char *Directory, int *UsedMB = NULL);
bool DirectoryOk(const char *DirName, bool LogErrors = false);
bool MakeDirs(const char *FileName, bool IsDirectory = false);
bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks = false);
bool RemoveEmptyDirectories(const char *DirName, bool RemoveThis = false);
char *ReadLink(const char *FileName);
bool SpinUpDisk(const char *FileName);
const char *WeekDayName(int WeekDay); // returns a statically allocated string!
const char *DayDateTime(time_t t = 0); // returns a statically allocated string!
int ReadFrame(int f, unsigned char *b, int Length, int Max);
int getVStreamID(int f);
int getTSPID(int f);
int isHDTV(int f);

class cFile {
private:
  static bool files[];
  static int maxFiles;
  int f;
public:
  cFile(void);
  ~cFile();
  operator int () { return f; }
  bool Open(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  bool Open(int FileDes);
  void Close(void);
  bool IsOpen(void) { return f >= 0; }
  bool Ready(bool Wait = true);
  static bool AnyFileReady(int FileDes = -1, int TimeoutMs = 1000);
  static bool FileReady(int FileDes, int TimeoutMs = 1000);
  static bool FileReadyForWriting(int FileDes, int TimeoutMs = 1000);
  };

/// cUnbufferedFile is used for large files that are mainly written or read
/// in a streaming manner, and thus should not be cached.

class cUnbufferedFile {
private:
  int fd;
  off_t curpos;
  off_t cachedstart;
  off_t cachedend;
  off_t begin;
  off_t lastpos;
  off_t ahead;
  size_t readahead;
  size_t written;
  size_t totwritten;
#ifdef USE_FADVISE
  int FadviseDrop(off_t Offset, off_t Len);
#endif
public:
  cUnbufferedFile(void);
  ~cUnbufferedFile();
  int Open(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  int Close(void);
  void SetReadAhead(size_t ra);
  off_t Seek(off_t Offset, int Whence);
  ssize_t Read(void *Data, size_t Size);
  ssize_t Write(const void *Data, size_t Size);
  int get_fd() { return fd; }
  static cUnbufferedFile *Create(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
};


class cSafeFile {
private:
  FILE *f;
  char *fileName;
  char *tempName;
public:
  cSafeFile(const char *FileName);
  ~cSafeFile();
  operator FILE* () { return f; }
  bool Open(void);
  bool Close(void);
  };

class cLockFile {
private:
  char *fileName;
  int f;
public:
  cLockFile(const char *Directory);
  ~cLockFile();
  bool Lock(int WaitSeconds = 0);
  void Unlock(void);
  };

class cListObject {
private:
  cListObject *prev, *next;
public:
  cListObject(void);
  virtual ~cListObject();
  virtual bool operator< (const cListObject &/*ListObject*/) { return false; }
  void Append(cListObject *Object);
  void Unlink(void);
  int Index(void);
  cListObject *Prev(void) const { return prev; }
  cListObject *Next(void) const { return next; }
  };

class cListBase {
protected:
  cListObject *objects, *lastObject;
  cListBase(void);
public:
  virtual ~cListBase();
  void Add(cListObject *Object);
  void Del(cListObject *Object);
  virtual void Move(int From, int To);
  void Move(cListObject *From, cListObject *To);
  virtual void Clear(void);
  cListObject *Get(int Index) const;
  int Count(void) const;
  void Sort(void);
  };

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) const { return (T *)cListBase::Get(Index); }
  T *First(void) const { return (T *)objects; }
  T *Last(void) const { return (T *)lastObject; }
  T *Prev(const T *object) const { return (T *)object->Prev(); }
  T *Next(const T *object) const { return (T *)object->Next(); }
  };

int make_pidfile(const char *dirname);
int rm_pidfile(const char *dirname);
pid_t processInfo(const char *dirname);
void dump2log (unsigned char * buf, unsigned char * end );
void show_stackframe(bool bFork);

#endif //__TOOLS_H
