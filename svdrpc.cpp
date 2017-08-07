/***************************************************************************
                          svdrpc.cpp  -  description
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
//#define _GNU_SOURCE

#include "svdrpc.h"
int SVDRPPort = 2001;
char *SVDRPHost = "127.0.0.1";


#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include "tools.h"


// --- cSocket ---------------------------------------------------------------

cSocket::~cSocket()
{
  Close();
}

void cSocket::Close(void)
{
  if (sock >= 0) 
  {
    close(sock);
    sock = -1;
  }
}

bool cSocket::Open(int Port)
{
  dsyslog( "cSocket::Open, port = %d",Port );
  port = Port;
  sock = -1;
  if (sock < 0) 
  {
     // create socket:
     sock = socket(PF_INET, SOCK_STREAM, 0);
     if (sock < 0) 
     {
        esyslog("cSocket::Open (socket) %s",strerror(errno));
        port = 0;
        return false;
     }
     // make it non-blocking:
     int oldflags = fcntl(sock, F_GETFL, 0);
     if (oldflags < 0) 
     {
        esyslog("cSocket::Open (fcntl-GET): %s",strerror(errno));
        return false;
     }
     oldflags |= O_NONBLOCK;
     if (fcntl(sock, F_SETFL, oldflags) < 0) 
     {
        esyslog("cSocket::Open (fcntl-SET): %s",strerror(errno));
        return false;
     }
     //ToDo: make Socket-keep-alive...
  }
  dsyslog("cSocket::Open ok" );
  return true;
}

int cSocket::Connect(char *Host)
{
  dsyslog("cSocket::Connect to %s", Host );
  if (Open(port)) 
  {
     struct sockaddr_in name;
     name.sin_family = AF_INET;
     name.sin_port = htons(port);
     name.sin_addr.s_addr = inet_addr(Host);
     uint size = sizeof(name);
     errno=0;
     int result = connect(sock, (struct sockaddr *)&name, size);
     if ((result == -1) && (errno==EINPROGRESS))//yes, thats like spec!
     {
        return sock;
     }
     else
     {
        esyslog("cSocket::Connect: %s",strerror(errno));
     }
  }
  return -1;
}

// --- cSVDRPC ----------------------------------------------------------------

bool cSVDRPC::Connected()
{
  if(filedes==-1)
    return false;
  else
    return true;
}

cSVDRPC::~cSVDRPC()
{
  Close();
}

void cSVDRPC::Open(char *Host ,int Port)
{
  dsyslog("cSVDRPC::Open port is %d",Port );
  name[0]=0;
  if (socket.Open(Port))
    filedes = socket.Connect(Host);
  else
    filedes = -1;
  outstandingReply=1;
  ReadReply();//lese Greeting...
}

void cSVDRPC::Close(void)
{
  dsyslog("cSVDRPC::Close" );
  close(filedes);
  socket.Close();
  filedes=-1;
}

bool cSVDRPC::Send(const char *s, int length)
{
  dsyslog("cSVDRPC::Send %s %d",s,length );
  int ret=0;
  if(filedes>0)
  {
    if (length < 0) 
      length = strlen(s);
    outstandingReply++;
    {
      int wbytes=write(filedes, s, length);
      if (wbytes == length)
        ret=true;
      else if (wbytes < 0)
      {
        esyslog("cSVDRPC::Send: %s",strerror(errno));
        //perror("svdrpc-write:");
	Close();//reopen connection in the case something went wrong:
			  //if (socket.Open())
			  //  filedes = socket.Connect();
			  //else
	filedes = -1;
	outstandingReply=1;
	ReadReply();//lese Greeting...
	ret=false;
      }
      else
      {
        esyslog("cSVDRPC::Send: Wrote %d bytes instead of %d", wbytes, length);
        ret=false;
      }
    }
  }
  else
  {
  	ret=false;
  }
  return ret;
}

bool cSVDRPC::ReadReply()
{
  dsyslog("cSVDRPC::ReadReply" );
  int n=0,i=0,rbytes=0,size=MAXCMDBUFFER-1;
  buf[0]=0;
  if (filedes >= 0)
  {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(filedes, &set);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    do
    {
      select(1, &set, NULL, NULL, &timeout);
      n = read(filedes, buf + rbytes, 1);
      if(n<0) 
      {
        filedes=-1;
        break;
      }
      rbytes += n;
      if (rbytes == size)
         break;
    }
    while ((n>=0) && (buf[rbytes-1]!='\n') && (++i<MAXCMDBUFFER));
    if(rbytes>0)
    {
      buf[rbytes]=0;
      if( (outstandingReply > 0) && (buf[rbytes-1] == '\n') )
        outstandingReply--;
      return true;
    }
    else
    {
      buf[0]=0;
    }
  }
  return false;
}

bool cSVDRPC::ReadCompleteReply()
{
  int i = 0;
  while( i < 100 && outstandingReply > 0 )
  {
    usleep(10000);
    if( !ReadReply() )
      break;
    i++;
  }
  return( outstandingReply <= 0 );
}

bool cSVDRPC::CmdQuit()
{
  dsyslog("cSVDRPC::CmdQuit" );
  char *Option=NULL;
  asprintf(&Option,"Quit\r\n");
  bool result=Send(Option);
  ReadReply();
  delete Option;
  return result;
}

void VDRMessage(const char *s)
{
  cSVDRPC *svdrpc=new cSVDRPC();
  svdrpc->Open(SVDRPHost,SVDRPPort);
  usleep(10000);
  svdrpc->ReadCompleteReply();
  svdrpc->Send(s);
  usleep(10000);
  svdrpc->ReadCompleteReply();
  svdrpc->CmdQuit();
  usleep(10000);
  svdrpc->ReadCompleteReply();
  delete svdrpc;
}

void noadMsg(const char *msg, const char *filename)
{
  char *baseName = NULL;
  char *cp = NULL;
  const char *vend = strchr(filename,'/');
  if( vend )
    vend = strchr(vend+1,'/');
  if( vend )
    asprintf(&baseName,"mesg %s %s",msg,vend+1);
  else
    asprintf(&baseName,"mesg %s %s",msg, filename);
  if( baseName[strlen(baseName)-1] == '/' )
    baseName[strlen(baseName)-1] = '\0';
  char *vend1 = strrchr(baseName, '/');
  if( vend1 )
    *vend1 = '\0';
  asprintf(&cp,"%s\r\n",baseName);

  VDRMessage(cp);

  free(baseName);
  free(cp);
}

void noadStartMessage( const char *s)
{
  noadMsg("start noad for ",s);
}

void noadEndMessage( const char *s)
{
  noadMsg("noad done for ",s);
}
