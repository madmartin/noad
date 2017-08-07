/***************************************************************************
                          ccontrol.h  -  description                              
                             -------------------                                         
    begin                : Tue Aug 3 1999                                           
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


#ifndef CCONTROL_H
#define CCONTROL_H
/**
  *@author Thorsten Janke
  */
#include "noaddata.h"
#include "cgetlogo.h"
#include "cchecklogo.h"


class CControl
{

public:
  CControl( noadData* data );
  ~CControl();
  noadData* m_pData;
  
  CGetLogo* m_pGetLogo0;	// pointer to get logo object top left 
  CGetLogo* m_pGetLogo1;	// pointer to get logo object top right 
  CGetLogo* m_pGetLogo2;	// pointer to get logo object bottom right 
  CGetLogo* m_pGetLogo3;	// pointer to get logo object bottom left 
  
  bool bCheckLogo0;
  bool bCheckLogo1;
  bool bCheckLogo2;
  bool bCheckLogo3;
  
  int m_nPictureCounter;	// counter for sent out picture 
  testlines* m_testhook;	// struct of detected logo informations 
  int m_nLogoCorner;			// detected logo corner 
  bool m_bFound;				//  is set to true if logo has found 

	void newData();			// get new data and call newData() in cgetlogo objects
	void forceCorner( int iNewCorner );
   void foundLogo( struct testlines*, int iCornerID );

private:

  void foundLogo0( struct testlines* );	// called if logo has found in corner top left 
  void foundLogo1( struct testlines* );	// called if logo has found in corner top right
  void foundLogo2( struct testlines* );	// called if logo has found in corner bottom right
  void foundLogo3( struct testlines* );	// called if logo has found in corner bottom left 
  void logo();										// slot called from checklogo if logo is available 
  void nologo();									// slot called from checklogo if there is no logo 
  void CtrlReset();
  void stop();

};

#endif
