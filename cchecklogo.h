/***************************************************************************
                          cchecklogo.h  -  description                              
                             -------------------                                         
    begin                : Sat Aug 7 1999                                           
    copyright            : (C) 1999 by Thorsten Janke                         
    email                : janke@studST.fh-muenster.de   
                         : http://www.ktet.fh-muenster.de/ina                 
            
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#ifndef CCHECKLOGO_H
#define CCHECKLOGO_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>

/**
  *@author Thorsten Janke
  */

class noadData;

/** The CCheckLogo class is able to check the status of tv logo
  * emittes the logo, nologo and unknown signal
  */
class CCheckLogo
{
public:
  CCheckLogo( noadData* data );
  ~CCheckLogo();
   
  void newData(); // called from parent object to show that there are new data available 
  void reset();	// called to delete possible testlines 

  void setLineHook( struct testlines* );
  void setCornerData( char* );
  bool isLogo;

  testlines* new_testline( int line );
  void save( FILE *fd );
  void log();
  void load( FILE *fd );
  void setTreshold(int i ) { iTreshold = i; }
  int getTreshold() { return iTreshold; }
  void getLogoRect(int &left, int &top, int &right, int &bottom);
private:
  int iLineOffset;
  int iXOffset;
  // check all reference diversions in the updated picture,
  // decide the logo status at the moment and emit the membership signal
  int checkTestlines( char* chSrc, struct testlines*	 tl, int lineOffset=0, int xOffset=0 );

  noadData* m_pData;
  #ifdef VNOAD
  //  temp buffer of data to show 
  unsigned char* m_chAnalyseData;
  unsigned char* m_chAnalyseData2;
  unsigned char* m_chAnalyseData3;
  int  m_chAnalyseDataSize;
  #endif
  
  int m_nNoLogo;						// value of detected nologo one after another 
  int m_nLogo;							// value of detected logo one after another 
  struct testlines* m_linehook;	// pointer to the testpoints 
  char* m_chCornerData;				// pointer to the picture corner which is set from ccontrol 
  int iTreshold;
  
  long totalPairsOk;
  int deltaPairsOk;
  int totalChecks;

public:
  void logo();		// signal logo emitted if logo is detected 
  void nologo();	// signal nologo emitted of no logo detected 
  void unknown(); // signal unknown emitted if there are not enough test pairs available at the moment
};

#endif
