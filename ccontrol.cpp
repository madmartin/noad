/***************************************************************************
                          ccontrol.cpp  -  description                              
                             -------------------                                         
    begin                : Tue Aug 3 1999                                           
    copyright            : (C) 1999 by Thorsten Janke                         
    email                : janke@studST.fh-muenster.de                                     
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/


#include <syslog.h>
#include <stdlib.h>

#include "ccontrol.h"

CControl::CControl( noadData* data )
{
//  dsyslog(LOG_INFO, "CControl::CControl");
  m_pData = data;	

  m_nPictureCounter = 0;
  m_bFound = false;
  m_nLogoCorner = UNKNOWN;

  // create CGetLogos 
  m_pGetLogo0 = new CGetLogo( this, m_pData, m_pData->m_chGreyCorner0, TOP_LEFT );
  m_pGetLogo1 = new CGetLogo( this, m_pData, m_pData->m_chGreyCorner1, TOP_RIGHT );
  m_pGetLogo2 = new CGetLogo( this, m_pData, m_pData->m_chGreyCorner2, BOT_RIGHT );
  m_pGetLogo3 = new CGetLogo( this, m_pData, m_pData->m_chGreyCorner3, BOT_LEFT );

  bCheckLogo0 = true;
  bCheckLogo1 = true;
  bCheckLogo2 = true;
  bCheckLogo3 = true;
}

#define DELETENULL(p) (delete (p), p = NULL)

CControl::~CControl()
{
  DELETENULL(m_pGetLogo3);
  DELETENULL(m_pGetLogo2);
  DELETENULL(m_pGetLogo1);
  DELETENULL(m_pGetLogo0);
}

void CControl::foundLogo0( struct testlines* tl ) 
{
  m_pData->m_nLogoCorner = m_nLogoCorner = TOP_LEFT;
  m_pData->m_bFound = m_bFound = true;
	
  m_pData->m_pCheckLogo->setLineHook(tl);
  m_pData->m_pCheckLogo->setCornerData(m_pData->m_chGreyCorner0);
}

void CControl::foundLogo1( struct testlines* tl )
{
  m_pData->m_nLogoCorner = m_nLogoCorner = TOP_RIGHT;
  m_pData->m_bFound = m_bFound = true;

  m_pData->m_pCheckLogo->setLineHook(tl);
  m_pData->m_pCheckLogo->setCornerData(m_pData->m_chGreyCorner1);
}

void CControl::foundLogo2( struct testlines* tl )
{
  m_pData->m_nLogoCorner = m_nLogoCorner = BOT_RIGHT;
  m_pData->m_bFound = m_bFound = true;
  m_pData->m_pCheckLogo->setLineHook(tl);
  m_pData->m_pCheckLogo->setCornerData(m_pData->m_chGreyCorner2);
}

void CControl::foundLogo3( struct testlines* tl )
{
  m_pData->m_nLogoCorner = m_nLogoCorner = BOT_LEFT;
  m_pData->m_bFound = m_bFound = true;
  m_pData->m_pCheckLogo->setLineHook(tl);
  m_pData->m_pCheckLogo->setCornerData(m_pData->m_chGreyCorner3);
}

void CControl::foundLogo( struct testlines* tl, int iCornerID )
{
	switch(iCornerID)
	{
		case TOP_LEFT:		foundLogo0(tl); break;
		case TOP_RIGHT:	foundLogo1(tl); break;
		case BOT_RIGHT:	foundLogo2(tl); break;
		case BOT_LEFT:		foundLogo3(tl); break;
	}
}


void CControl::CtrlReset() 
{
  m_pGetLogo0->reset();
  m_pGetLogo1->reset();
  m_pGetLogo2->reset();
  m_pGetLogo3->reset();

  if ( m_bFound )
    m_pData->m_pCheckLogo->reset();

  m_bFound = false;
}

// called to stop logo detection  
void CControl::stop()
{
  m_pGetLogo0->reset();
  m_pGetLogo1->reset();
  m_pGetLogo2->reset();
  m_pGetLogo3->reset();

  if ( m_bFound )
     m_pData->m_pCheckLogo->reset();

  m_bFound = false;
}

void CControl::newData()
{
  //dsyslog(LOG_INFO, "CControl::GrabImage");
  m_pData->setCorners();
  m_nPictureCounter++ ;

  // call the get logo object after soft filter has init 
  if ( m_nPictureCounter > 2 && !m_bFound ) 
  {
         if( bCheckLogo0 ) m_pGetLogo0->newData();
         if( bCheckLogo1 ) m_pGetLogo1->newData();
         if( bCheckLogo2 ) m_pGetLogo2->newData();
         if( bCheckLogo3 ) m_pGetLogo3->newData();
  }
  if ( m_bFound )
    m_pData->m_pCheckLogo->newData();

}

void CControl::forceCorner( int iNewCorner )
{
   dsyslog("CControl::forceCorner %d", iNewCorner);
	if( iNewCorner >= 0 && iNewCorner <= 3 )
	{
		bCheckLogo0 = iNewCorner == 0;
	  	bCheckLogo1 = iNewCorner == 1;
	  	bCheckLogo2 = iNewCorner == 2;
	  	bCheckLogo3 = iNewCorner == 3;
		CtrlReset();
	}
}

