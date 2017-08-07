// vdr-classes
#ifndef VDR_CL_H
#define VDR_CL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#include <signal.h>
#endif
#include "tools.h"

typedef unsigned char uchar;
#define DIR_DELIM "/"
#define DIR_DELIMC '/'
#define INDEXFILESUFFIX     DIR_DELIM"index"
#define INDEXFILESUFFIXEX   DIR_DELIM"indexEx.vdr"
#define RECORDFILESUFFIX    DIR_DELIM"%03d.vdr"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...
//#define RESUMEFILESUFFIX  DIR_DELIM ## "resume.vdr"
//#define MARKSFILESUFFIX   DIR_DELIM ## "marks.vdr"
#ifdef VNOAD
#define NOADMARKSFILESUFFIX   DIR_DELIM"noadmarks.vdr"
#endif

#define SUMMARYFALLBACK

#define RESUMEFILESUFFIX  DIR_DELIM"resume%s%s"
#ifdef SUMMARYFALLBACK
#define SUMMARYFILESUFFIX  DIR_DELIM"summary.vdr"
#endif
#define INFOFILESUFFIX    DIR_DELIM"info"
#define MARKSFILESUFFIX   DIR_DELIM"marks"

#define MAXFILESPERRECORDING 255
#define DEFAULTFRAMESPERSECOND 25.0

// The maximum time to wait before giving up while catching up on an index file:
#define MAXINDEXCATCHUP   8 // seconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

#define KILOBYTE(n) ((n) * 1024)
#define TS_SIZE               188
// The maximum size of a single frame (up to HDTV 1920x1080):
#define MAXFRAMESIZE  (KILOBYTE(1024) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE to avoid breaking up TS packets
// The maximum size of a single frame:

#define FRAMESPERSEC 25
#define FRAMESPERMIN (FRAMESPERSEC*60)

extern int SysLogLevel;
extern char MarksfileSuffix[];

bool setMarkfileSuffix(bool bIsPESFile);
bool setMarkfileName(const char *name, bool bIsPESFile = false);
void releaseMarkfileName();

class cReadLine 
{
	private:
		size_t size;
		//char *buffer;
	public:
		cReadLine(void);
		~cReadLine();
		char *Read(FILE *f);
}; 
// --- cFileName -------------------------------------------------------------
class cFileName
{
private:
  cUnbufferedFile *file;
  int fileNumber;
  char *fileName, *pFileNumber;
  bool record;
  bool blocking;
  bool isPesRecording;
public:
  cFileName(const char *FileName, bool Record, bool Blocking = false, bool IsPesRecording = false);
  ~cFileName();
  const char *Name(void) { return fileName; }
  int Number(void) { return fileNumber; }
  cUnbufferedFile *Open(void);
  void Close(void);
  cUnbufferedFile *SetOffset(int Number, off_t Offset = 0);
  cUnbufferedFile *NextFile(void);
  int File() { return file->get_fd(); }
  bool isPES() { return isPesRecording; }
};

// --- cResumeFile ------------------------------------------------------------
class cResumeFile
{
private:
  char *fileName;
  bool isPesRecording;
public:
  cResumeFile(const char *FileName, bool IsPesRecording);
  ~cResumeFile();
  int Read(void);
  bool Save(int Index);
  void Delete(void);
};

struct tIndexPes 
{
  uint32_t offset;
  uchar type;
  uchar number;
  uint16_t reserved;
};

struct tIndexTs 
{
	// for MS all items must be same type
	// else ms aligns each item on a boundary, giving a structure size >8!!!
  uint64_t offset:40; // up to 1TB per file (not using off_t here - must definitely be exactly 64 bit!)
  uint64_t reserved:7;     // reserved for future use
  uint64_t independent:1;  // marks frames that can be displayed by themselves (for trick modes)
  uint64_t number:16; // up to 64K files per recording
  tIndexTs(off_t Offset, bool Independent, uint16_t Number)
  {
    offset = Offset;
    reserved = 0;
    independent = Independent;
    number = Number;
  }
  tIndexTs(){}
};


// --- cIndexFile ------------------------------------------------------------
class cIndexFile
{
protected:
  int f;
  char *fileName;
  int size, last;
  tIndexTs *index;
  bool isPesRecording;
  cResumeFile resumeFile;
  void ConvertFromPes(tIndexTs *IndexTs, int Count);
  void ConvertToPes(tIndexTs *IndexTs, int Count);
  bool CatchUp(int Index = -1);
public:
  cIndexFile(const char *FileName, bool Record, bool IsPesRecording = false);
  ~cIndexFile();
  bool Ok(void) { return index != NULL; }
  bool Write(bool Independent, uint16_t FileNumber, off_t FileOffset);
  bool Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent = NULL, int *Length = NULL);
  int GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber = NULL, off_t *FileOffset = NULL, int *Length = NULL, bool StayOffEnd = false);
  int Get(uint16_t FileNumber, off_t FileOffset);
  int Last(void) { CatchUp(); return last; }
  int GetResume(void) { return resumeFile.Read(); }
  bool StoreResume(int Index) { return resumeFile.Save(Index); }
  void check(int start=0);
#ifdef NOAD
  int getLast() { return( last ); }
#endif

};

// --- cNoadIndexFile ------------------------------------------------------------
class cNoadIndexFile : public cIndexFile
{
  struct tIndexEx{ int isLogo; int blackTop; int blackBottom; };
  int fEx;
  char *fileNameEx;
  int sizeEx, lastEx;
  tIndexEx *indexEx;
//  #ifdef VNOAD
  int interval;
  int lastGetValue;
//  #endif
public:
  cNoadIndexFile(const char *FileName, bool Record, bool IsPesRecording);
  ~cNoadIndexFile();
  // ohne CatchUp !!!
  bool Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent = NULL, int *Length = NULL);
  // ohne CatchUp !!!
  int GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber = NULL, off_t *FileOffset = NULL, int *Length = NULL, bool StayOffEnd = false);
  //int Get(uint16_t FileNumber, off_t FileOffset);
  bool setIndexEx( int index, int _isLogo, int _blackTop, int _blackBottom);
  void logIndexEx();
//  #ifdef VNOAD
  bool CatchUp(int Index = -1);
  void setInterval(int newInterval) { interval = newInterval; }
  int Last(void) { return getLast(); }
  int getLast();
/*  
  #else
  int Last(void) { return last; }
  bool CatchUp(int Index = -1) { return cIndexFile::CatchUp(Index); }
  #endif
*/
};


// --- cMark ------------------------------------------------------------
class cMark : public cListObject
{
private:
  double framesPerSecond;
  static char *buffer;
  bool checked;
public:
  int position;
  char *comment;
  cMark(int Position = 0, const char *Comment = NULL, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
  virtual ~cMark();
  cString ToText(bool bWithNewline = true);
  bool Parse(const char *s);
  bool Save(FILE *f);
  bool isChecked() { return checked; }
  void setChecked(bool b) { checked = b; }
};

template<class T> class cConfig : public cList<T>
{
private:
  char *fileName;
  bool allowComments;
public:
  void Clear(void)
  {
    free(fileName);
    fileName = NULL;
    cList<T>::Clear();
  }
public:
  cConfig(void) { fileName = NULL; }
  virtual ~cConfig() { free(fileName); }
  const char *FileName(void) { return fileName; }
  bool Load(const char *FileName = NULL, bool AllowComments = false, bool MustExist = false)
  {
    cConfig<T>::Clear();
    if (FileName) {
       free(fileName);
       fileName = strdup(FileName);
       allowComments = AllowComments;
       }
    bool result = !MustExist;
    if (fileName && access(fileName, F_OK) == 0) {
       isyslog("loading %s", fileName);
       FILE *f = fopen(fileName, "r");
       if (f) 
		 {
          char *s;
          int line = 0;
          cReadLine ReadLine;
          result = true;
          while ((s = ReadLine.Read(f)) != NULL) 
			 {
				line++;
            if (allowComments) 
				{
					char *p = strchr(s, '#');
					if (p)
						*p = 0;
				}
            stripspace(s);
            if (!isempty(s)) 
				{
					T *l = new T;
               if (l->Parse(s))
						Add(l);
               else 
					{
						esyslog("ERROR: error in %s, line %d", fileName, line);
                  delete l;
                  result = false;
               }
             }
          }
		   fclose(f);
       }
       else 
		 {
          LOG_ERROR_STR(fileName);
          result = false;
       }
    }
    return result;
  }
  bool Save(void)
  {
    bool result = true;
    T *l = (T *)this->First();
    cSafeFile f(fileName);
    if (f.Open())
    {
      while (l)
      {
        if (!l->Save(f))
        {
          result = false;
          break;
        }
        l = (T *)l->Next();
      }
      if (!f.Close())
        result = false;
    }
    else
    {
      LOG_ERROR_STR(fileName);
      result = false;
    }
    return result;
  }
  //#ifdef NOAD
public:
  void ClearList(void)
  {
    cList<T>::Clear();
  }
  const char *getFilename() { return fileName; }
  //#endif
};


// --- cMarks -------------------------------------------------------------
class cMarks : public cConfig<cMark>
{
private:
  double framesPerSecond;
public:
  bool Load(const char *RecordingFileName, double FramesPerSecond = DEFAULTFRAMESPERSECOND, bool IsPesRecording = false);
  bool ReLoad();
  void Sort(void);
  cMark *Add(int Position);
  cMark *Get(int Position);
  cMark *GetPrev(int Position);
  cMark *GetNext(int Position);

  //noad:
  bool Backup(const char *RecordingFileName);
  int getActiveFrames(int totalFrames);
  bool hasUncheckedMarks();
  cMark *GetLast() { return Last(); }
};
class cRecording : public cListObject {
protected:
  mutable int resume;
  mutable char *titleBuffer;
  char *sortBuffer;
  mutable char *fileName;
  mutable char *name;
  mutable int fileSizeMB;
  int channel;
  int instanceId;
  bool isPesRecording;
  double framesPerSecond;

//  cRecordingInfo *info;
  cRecording(const cRecording&); // can't copy cRecording
  cRecording &operator=(const cRecording &); // can't assign cRecording
  static char *StripEpisodeName(char *s);
  char *SortName(void) /*const*/;
  int GetResume(void) const;

  char *summary; // old
//  char *StripEpisodeName(char *s);
public:
  time_t start;
  int priority;
  int lifetime;
//   cRecording(cTimer *Timer, const char *Title, const char *Subtitle, const char *Summary);
  cRecording(const char *FileName);
  ~cRecording();
  virtual bool operator< (const cListObject &ListObject);
  const char *Name(void) const { return name; }
  const char *FileName(void) const;
  const char *Title(char Delimiter = ' ', bool NewIndicator = false, int Level = -1) const;
  const char *Summary(void) const { return summary; }
  const char *PrefixFileName(char Prefix);
  int HierarchyLevels(void) const;
  bool IsNew(void) const { return GetResume() <= 0; }
  bool IsEdited(void) const;
  bool WriteSummary(void);
//   bool WriteRecInfo(cTimer *timer);
  bool Delete(void);
       // Changes the file name so that it will no longer be visible in the "Recordings" menu
       // Returns false in case of error
  bool Remove(void);
       // Actually removes the file from the disk
       // Returns false in case of error
  };

class cRecordings : public cList<cRecording> {
public:
  bool Load(bool Deleted = false);
  cRecording *GetByName(const char *FileName);
  };


bool isPESRecording(const char *filename);

#ifdef VNOAD
class cNoAdMarks : public cMarks
{
public:
  bool Load(const char *RecordingFileName, bool AllowComments = false);
};
#endif

cString IndexToHMSF(int Index, bool WithFrame = false, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
int HMSFToIndex(const char *HMSF, double FramesPerSecond = DEFAULTFRAMESPERSECOND);

cUnbufferedFile *OpenVideoFile(const char *FileName, int Flags);
int CloseVideoFile(cUnbufferedFile *File);
 
#endif
