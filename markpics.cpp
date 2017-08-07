#include <string.h>
#include <iostream>
#include <list>

#include <Magick++.h>

#include "vdr_cl.h"

using namespace std;
using namespace Magick;

int SysLogLevel=0;
int picWidth=300;
int picHeight=10;
char *odir=NULL;
bool withAudioMarks=false;

int doPics(const char * filename)
{
  cNoadIndexFile *cIF = NULL;
  cFileName *cfn = NULL;
  int demux_track = 0;
  cIF = new cNoadIndexFile(filename,false);
  if( cIF == NULL )
    return -1;
  cfn = new cFileName(filename, false);
  if( cfn->Open() < 0 )
  {
    delete cfn;
    delete cIF;
    return -1;
  }
  demux_track = getVStreamID(cfn->File());
  delete cfn;
  delete cIF;
}

int *MarkArray = NULL;
int* getMarkPositions(cMarks *pmarks)
{
  int iCount = 0;
  cMark *m = pmarks->GetNext(-1);
  while( m != NULL )
  {
    iCount++;
    m = pmarks->GetNext(m->position);
  }
  if( MarkArray != NULL )
    delete [] MarkArray;
  MarkArray = new int[iCount+1];
  MarkArray[0] = iCount;
  iCount = 1;

  m = pmarks->GetNext(-1);
  while( m != NULL )
  {
    MarkArray[iCount] = m->position;
    iCount++;
    m = pmarks->GetNext(m->position);
  }
  return MarkArray;
}

int scaleMarkPosition( int iPos, int iMax, int iWidth )
{
  syslog(LOG_INFO,"scaleMarkPosition %d %d %d gives %d",iPos, iMax, iWidth,(iWidth * iPos)/iMax);
  return (iWidth * iPos)/iMax;
}

void paintMarks(Image &image, cMarks *pmarks, int iNumFrames, bool bWithAudio=false)
{
  int *ipMarks;
  int iCount;
  //int iNumFrames = vdr_getnumframes();
  image.fillColor("red");
  image.draw( DrawableRectangle(0,0,image.size().width(),bWithAudio ? image.size().height():image.size().height()/2 ) );

  ipMarks = getMarkPositions(pmarks);
  iCount = *ipMarks++;
  image.fillColor("green");
  for( int i = 0; i < iCount; i += 2 )
  {
    int markStart = scaleMarkPosition(*ipMarks++, iNumFrames, image.size().width());
    int markEnd;
    if( i < iCount-1)
      markEnd = scaleMarkPosition(*ipMarks++, iNumFrames,image.size().width());// - markStart;
    else
      markEnd = image.size().width();
    syslog(LOG_INFO, "paintEvent start = %d, end = %d", markStart, markEnd);
    image.draw( DrawableRectangle(markStart,0,markEnd,bWithAudio ? image.size().height():image.size().height()/2) );
    //qpainter.drawRect( markStart, 0, markEnd, image.size.height()  );
  }
}

void paintAudioMarks(Image &image, cMarks *pmarks, int iNumFrames)
{
  int *ipMarks;
  int iCount;
  //int iNumFrames = vdr_getnumframes();
  image.fillColor("white");
  image.draw( DrawableRectangle(0,image.size().height()/2,image.size().width(),image.size().height() ) );

  ipMarks = getMarkPositions(pmarks);
  iCount = *ipMarks++;
  image.fillColor("black");
  for( int i = 0; i < iCount; i++ )
  {
    int markStart = scaleMarkPosition(*ipMarks++, iNumFrames, image.size().width());
    image.draw( DrawableLine(markStart,image.size().height()/2,markStart,image.size().height()) );
    //qpainter.drawRect( markStart, 0, markEnd, image.size.height()  );
  }
}

int doMarkPic(const char * filename)
{
  cNoadIndexFile *cIF = NULL;
  cIF = new cNoadIndexFile(filename,false);
  if( cIF == NULL )
    return -1;
  cMarks *pmarks = new cMarks();
  pmarks->Load(filename);
  Image image;
  image.size( Geometry(picWidth,picHeight) );
  paintMarks(image, pmarks, cIF->Last(),withAudioMarks);
  if(withAudioMarks)
  {
    setMarkfileName("audiomarks");
    cMarks *paudiomarks = new cMarks();
    pmarks->Load(filename);
    paintAudioMarks(image, pmarks, cIF->Last());
  }
  char *picname = NULL;
  char *picFullname = NULL;
  asprintf(&picFullname,"%s/marks.jpg",filename);
  char *pos = ::strchr(picFullname,'/');
  while(pos)
  {
    *pos='_';
    pos = ::strchr(picFullname,'/');
  }
  //asprintf(&picname,"%s/marks.jpg",filename);
  asprintf(&picname,"%s/%s",odir!=NULL?odir:filename,picFullname);
  /*
  char *cp = strchr(picname,'/');
  while( cp )
  {
    *cp = '.';
    cp = strchr(picname,'/');
  }
  if( picname[0] == '.' )
     memmove(picname,picname+1,strlen(picname));
  fprintf(stdout, "%s\n",picname);
  */
  image.write(picname);
  delete picname;
  delete picFullname;
  delete cIF;
  delete pmarks;
}

/*
int main(int argc, char ** argv)
{
  if( argc > 2 )
    setMarkfileName(argv[2]);
  doMarkPic(argv[1]);
  return 0;
}
*/
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
        {"withaudio", 0, 0, 5},
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

        case 5:
        withAudioMarks=true;
        break;
        
        default:
        printf ("?? getopt returned character code 0%o ??(option_index %d)\n", c,option_index);
    }
  }

  if (optind < argc)
  {
    doMarkPic(argv[optind]);
  }
  else
    fprintf(stderr, "markpics: no recording-dir given\n");
  if( odir )
    free(odir);
}

