/***************************************************************************
                          svdrpc.h  -  description
                             -------------------
    begin                : Sat Aug  5 13:32:09 MEST 2000
    copyright            : (C) 2000 by Guido
    email                : gfiala@s.netic.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __SVDRPC_H
#define __SVDRPC_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define MAXCMDBUFFER 10000

class cSocket {
private:
  int port;
  int sock;
  int queue;
public:
  ~cSocket();
  bool Open(int Port);
  int Connect(char *Host);
  void Close(void);
};

class cSVDRPC {
public:
  ~cSVDRPC();
  bool Connected();
  bool CmdQuit();
  char name[22];
  void Open(char *Host, int Port);
  void Close(void);
  bool Send(const char *s, int length = -1);
  bool ReadReply();
  bool ReadCompleteReply();
private:
  cSocket socket;
  int  filedes;
  int  outstandingReply;
  char buf[MAXCMDBUFFER];
};

extern int SVDRPPort;
extern char *SVDRPHost;

void VDRMessage(const char *s);
void noadStartMessage( const char *s );
void noadEndMessage( const char *s );

#endif //__SVDRP_H
