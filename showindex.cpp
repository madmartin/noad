#include <string.h>
#include <iostream>
#include <list>

#include "vdr_cl.h"

using namespace std;

int SysLogLevel=0;
int picWidth=300;
int picHeight=10;
char *odir=NULL;
bool showGOPsize = true;

int doShowIndex(const char * filename)
{
  cNoadIndexFile *cIF = NULL;
  cIF = new cNoadIndexFile(filename,false);
  if( cIF == NULL )
    return -1;
    
  int iIndex = 0;
  int iMax = cIF->Last();
  uchar FileNumber;         // current file-number
  int FileOffset;           // current file-offset
  uchar PictureType;        // current picture-type
  int Length;               // frame-lenght of current frame
  char pictypes[]= { 'U','I','P','B' };
  char *indents[]= { "",""," ","    " };
  while( iIndex < iMax )
  {
    cIF->Get( iIndex, &FileNumber, &FileOffset, &PictureType, &Length);
    if( showGOPsize && PictureType == I_FRAME )
    {
      fprintf(stdout,"%s%05d %02d %08d %c %08d",
        indents[PictureType],iIndex, FileNumber, FileOffset, pictypes[PictureType], Length);
      if( showGOPsize && PictureType == I_FRAME )
      {
        int nextIFrame = cIF->GetNextIFrame( iIndex, true, &FileNumber, &FileOffset, &Length, false);
        if( nextIFrame < 0 )
          nextIFrame = cIF->Last();
        fprintf(stdout," GOP-Size is %02d", nextIFrame - iIndex);
      }
      fprintf(stdout,"\n");
    }
    iIndex++;
  }
  delete cIF;
}

int main(int argc, char *argv[])
{
  int c;
  while (1)
  {
    int option_index = 0;
    static struct option long_options[] =
    {
      {"markfile",1,0,1},
      {"width",1,0,2},
      {"height", 1, 0, 3},
      {"outputdir", 1, 0, 4},
      {0, 0, 0, 0}
    };

    c = getopt_long  (argc, argv, "", long_options, &option_index);
    if (c == -1)
    break;

    switch (c)
    {
      case 1:
        setMarkfileName(optarg);
        break;

      case 2:
        if (isnumber(optarg))
          picWidth = atoi(optarg);
        else
        {
          fprintf(stderr, "markpics: invalid width: %s\n", optarg);
          return 2;
        }
      break;

      case 3:
        if (isnumber(optarg))
          picHeight = atoi(optarg);
        else
        {
          fprintf(stderr, "markpics: invalid height: %s\n", optarg);
          return 2;
        }
      break;

      case 4:
        asprintf(&odir,"%s/",optarg);
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
    fprintf(stderr, "showindex: no recording-dir given\n");
  if( odir )
    free(odir);
}

