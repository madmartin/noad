#include <string.h>
#include <iostream>
#include <list>

#include "vdr_cl.h"

using namespace std;

static int SysLogLevel=3;
bool showGOPsize = false;
#define BYTES_TO_READ 30
int doShowIndex(const char * filename)
{
  cNoadIndexFile *cIF = NULL;
  cFileName *cfn = NULL;
  fprintf(stdout,"try to open index.vdr in %s\n",filename);
  cIF = new cNoadIndexFile(filename,false);
  if( cIF == NULL )
  {
    fprintf(stdout,"opening index.vdr failed\n");
    return -1;
  }
  
  if( !cIF->Ok() )
  {
    fprintf(stdout,"opening index.vdr failed\n");
    delete cIF;
    return -1;
  }

  cfn = new cFileName(filename, false);
  fprintf(stdout,"try to open %s\n",cfn->Name());
  if( cfn->Open() < 0 )
  {
    fprintf(stdout,"opening %s failed\n",cfn->Name());
    delete cIF;
    delete cfn;
    return -1;
  }
  
  fprintf(stdout,"index.vdr opened, # of entries is %d\n",  cIF->Last() );
  int iIndex = 0;
  int iMax = cIF->Last();
  uchar FileNumber;         // current file-number
  int FileOffset;           // current file-offset
  uchar PictureType;        // current picture-type
  int Length;               // frame-lenght of current frame
  char pictypes[]= { 'U','I','P','B' };
  char *indents[]= { "","","  ","    " };
  char *indents2[]= { "","    ","  ","" };
  int lastFile=0;
  int lastOffset = 0;
  unsigned char readBuffer[BYTES_TO_READ+1];
  while( iIndex < iMax )
  {
    cIF->Get( iIndex, &FileNumber, &FileOffset, &PictureType, &Length);
    if( showGOPsize )
    {
      if( PictureType == I_FRAME )
      {
        fprintf(stdout,"%s%06d %02d %10d %c %06d",
          indents[PictureType],iIndex, FileNumber, FileOffset, pictypes[PictureType], Length);
        int nextIFrame = cIF->GetNextIFrame( iIndex, true, &FileNumber, &FileOffset, &Length, false);
        if( nextIFrame < 0 )
          nextIFrame = cIF->Last();
        fprintf(stdout," GOP-Size is %02d", nextIFrame - iIndex);
        fprintf(stdout,"\n");
      }
    }
    else
    {
      // check the index-entry
      fprintf(stdout,"%s%06d %02d %10d %c %06d",
        indents[PictureType],iIndex, FileNumber, FileOffset, pictypes[PictureType], Length);
      if( Length < 0 )
      {
        uchar fn;        // current file-number
        int fo;          // current file-offset
        uchar pt;        // current picture-type
        int le;          // frame-lenght of current frame
        cIF->Get( iIndex+1, &fn, &fo, &pt, &le);
        if( fn > FileNumber && Length == -1)
        {
          fprintf(stdout," length %d is ok (last index-entry for file %d)",Length,FileNumber);
        }
        else
          fprintf(stdout, " Error: illegal length %d",Length);
      }
      if( Length > MAXFRAMESIZE )
        fprintf(stdout, " Warning: length %d > MAXFRAMESIZE(%d)",Length,MAXFRAMESIZE);
      if( FileNumber < lastFile )
        fprintf(stdout, " Error: FileNumber jumps backwards (%d %d)", FileNumber, lastFile);
      if( lastFile!=FileNumber )
        lastOffset = 0;
      if( FileOffset < lastOffset )
        fprintf(stdout, " Error: Offset < Offset of last Frame ");
      
      // check the byte-sequence where this index-entry points to
      cfn->SetOffset( FileNumber, FileOffset);
      memset(readBuffer,0xff,BYTES_TO_READ);
      int r = safe_read(cfn->File(), readBuffer, BYTES_TO_READ);
      if (r < 5)
      {
        fprintf(stdout," error reading file");
      }
      else
      {
        //fprintf(stdout,"%s %02x %02x %02x %02x %02x %02x",indents2[PictureType],readBuffer[0],readBuffer[1],readBuffer[2],readBuffer[3],readBuffer[4],readBuffer[5]);
        fprintf(stdout,"%s",indents2[PictureType]);
        for(int j = 0; j < BYTES_TO_READ; j++)
           fprintf(stdout," %02x",readBuffer[j]);
      }
      
      fprintf(stdout,"\n");
      lastFile=FileNumber;
      lastOffset=FileOffset;
    }
    iIndex++;
  }
  delete cIF;
  delete cfn;
  return 0;
}

void usage(void)
{
  printf("usage: showindex [--gopsize] <vdr-recording-dir>\n");
}

int main(int argc, char *argv[])
{
  int c;
  while (1)
  {
    int option_index = 0;
    static struct option long_options[] =
    {
      {"gopsize",0,0,1},
      {0, 0, 0, 0}
    };

    c = getopt_long  (argc, argv, "", long_options, &option_index);
    if (c == -1)
    break;

    switch (c)
    {
      case 1:
        showGOPsize = true;
        break;

      default:
        printf ("?? getopt returned character code 0%o ??(option_index %d)\n", c,option_index);
    }
  }

  if (optind < argc)
  {
    doShowIndex(argv[optind]);
  }
  else
    usage();
}

