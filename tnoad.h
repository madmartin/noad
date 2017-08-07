/***************************************************************************
 *   Copyright (C) 2004 by theNoad #709GRW                                 *
 *   theNoad@SoftHome.net                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef __TNOAD_H__
#define __TNOAD_H__
#include "noad.h"
/*
#include "thread.h"

class cLogoDetectionThread : public cThread 
{
private:
  const char *error;
  bool active;
protected:
  virtual void Action(void);
public:
  cLogoDetectionThread(noadData *thedata, const char *FileName);
  virtual ~cLogoDetectionThread();
  const char *Error(void) { return error; }
};

class cLogoFinderThread : public cThread 
{
private:
  const char *error;
  bool active;
protected:
  virtual void Action(void);
public:
  cLogoFinderThread(noadData *thedata, const char *FileName);
  virtual ~cLogoFinderThread();
  const char *Error(void) { return error; }
};
*/
extern bool bTestMode;

int doOnlineScan(noadData *thedata, const char *fName, cMarks *_marks = NULL );
#endif
