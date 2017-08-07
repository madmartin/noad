/***************************************************************************
                          cgetlogo.h  -  description                              
                             -------------------                                         
    begin                : Fri Aug 6 1999                                           
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


#ifndef CGETLOGO_H
#define CGETLOGO_H

// analyse corner and set test points
// @author Thorsten Janke
#include "noaddata.h"

class CControl;

class CGetLogo
{
	int CornerID;
	CControl *caller;
public:
  CGetLogo( CControl* caller, noadData* data, char* chRunData,
  int iCornerID );
  ~CGetLogo();

  void newData();											// called if new data available
  bool checkBlock( char* chNew, char* chRef );	// returns true if user condition for picture change ok 
  void reset( bool bFirst = false );				// reset all buffers for a new run 
  int setTestlines( char* chMask );					// set the testlines of src data to testline struct 
  void setDiversions( char* chSrc );				// called to calculate the numerate diversions of two neighb. points 
  int updateDiversions( char* chMask, char* chSrc );	// called to check the diversions in updated picture and to update the mask 
  testlines* new_testline( int line );				// called to get a new allocated testline 
  noadData* m_pData;										// pointer to the Data-Object 
  char* m_chFilterData;									// main buffer of tp filtered Data 
  char* m_chRefPicture;									// buffer of last accepted picture 
  char* m_chRunData;										// buffer of running soft filtered grey data 
  char* m_chRunColorData;								// buffer of running soft filtered color data 
  int m_nFilteredCorner;								// counter of accepted corners 
  int m_nCheckedCorner;									// counter of check corner 
  char* m_chTestViewData;								// testpoints are drawn in this data buffer  
  int m_nPointsLeft;										// the rest test points 
  testlines* m_linehook;								// the first testline element 
  void foundLogo( struct testlines* );				// called if logo found 
};

#endif




















































