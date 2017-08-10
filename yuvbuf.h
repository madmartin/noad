#ifndef _NOADYUVBUF_H
#define _NOADYUVBUF_H
#include <string.h> // memcpy

class noadYUVBuf
{
   noadYUVBuf(noadYUVBuf&){}
public:
	uint8_t * buf[3];
   int linewidth[3];
   int width,height;
   bool bAlloced;
   noadYUVBuf() 
   { 
      bAlloced=false; 
      buf[0] = 0;
      buf[1] = 0;
      buf[2] = 0;
      linewidth[0] = 0;
      linewidth[1] = 0;
      linewidth[2] = 0;
      width = 0;
      height = 0;
   }
   ~noadYUVBuf()
   {
      if(bAlloced)
         release();
   }
   void assign(const noadYUVBuf rhs)
   {
      if( width != rhs.width || height != rhs.height || linewidth[0] != rhs.linewidth[0] 
         || linewidth[1] != rhs.linewidth[1] || linewidth[2] != rhs.linewidth[2])
      {
         width = rhs.width;
         height = rhs.height;
         linewidth[0] = rhs.linewidth[0];
         linewidth[1] = rhs.linewidth[1];
         linewidth[2] = rhs.linewidth[2];
         if(bAlloced)
            release();
      }
      if( rhs.bAlloced )
      {
         if(!bAlloced)
            alloc(width , height, linewidth[0], linewidth[1]);
         copyData(rhs.buf, linewidth[0], linewidth[1]);
      }
      else
      {
         buf[0] = rhs.buf[0];
         buf[1] = rhs.buf[1];
         buf[2] = rhs.buf[2];
      }
      //return *this;
   }
   noadYUVBuf& operator=(noadYUVBuf *rhs)
   {
      if( width != rhs->width || height != rhs->height || linewidth[0] != rhs->linewidth[0] 
         || linewidth[1] != rhs->linewidth[1] || linewidth[2] != rhs->linewidth[2])
      {
         width = rhs->width;
         height = rhs->height;
         linewidth[0] = rhs->linewidth[0];
         linewidth[1] = rhs->linewidth[1];
         linewidth[2] = rhs->linewidth[2];
         if(bAlloced)
            release();
      }
      if( rhs->bAlloced )
      {
         if(!bAlloced)
            alloc(width , height, linewidth[0], linewidth[1]);
         copyData(rhs->buf, linewidth[0], linewidth[1]);
      }
      else
      {
         if(bAlloced)
            release();
         buf[0] = rhs->buf[0];
         buf[1] = rhs->buf[1];
         buf[2] = rhs->buf[2];
      }
      return *this;
   }

   bool isNull() { return (width==0 || height== 0); }
   noadYUVBuf(uint8_t *const data[3], const unsigned int w, const unsigned int h)
   {
      bAlloced=false; 
      width = w;
      height = h;
      buf[0] = data[0];
      buf[1] = data[1];
      buf[2] = data[2];
      linewidth[0] = w;
      linewidth[1] = w/2;
      linewidth[2] = w/2;
   }
   noadYUVBuf(uint8_t **data, int *linesizes, int w, int h)
   {
      bAlloced=false; 
      width = w;
      height = h;
      buf[0] = data[0];
      buf[1] = data[1];
      buf[2] = data[2];
      linewidth[0] = linesizes[0];
      linewidth[1] = linesizes[1];
      linewidth[2] = linesizes[2];
   }
   void alloc(int w, int h)
   {
      width = w;
      height = h;
      buf[0] = new uint8_t[width*height];
      buf[1] = new uint8_t[(width*height)/4];
      buf[2] = new uint8_t[(width*height)/4];
      linewidth[0] = w;
      linewidth[1] = w/2;
      linewidth[2] = w/2;
      bAlloced=true; 
   }
   void alloc(int w, int h, int linesize1, int linesize2)
   {
      width = w;
      height = h;
      buf[0] = new uint8_t[linesize1*height];
      buf[1] = new uint8_t[linesize2*(height/2)];
      buf[2] = new uint8_t[linesize2*(height/2)];
      linewidth[0] = linesize1;
      linewidth[1] = linesize2;
      linewidth[2] = linesize2;
      bAlloced=true; 
   }
   void release()
   {
      if(bAlloced)
      {
         delete [] buf[0];
         delete [] buf[1];
         delete [] buf[2];
         buf[0] = 0;
         buf[1] = 0;
         buf[2] = 0;
      }
      bAlloced=false; 
   }
   void copyData(uint8_t * const _buf[3])
   {
      if( bAlloced )
      {
		   memcpy(buf[0],_buf[0],width*height);
		   memcpy(buf[1],_buf[1],(width*height)/4);
		   memcpy(buf[2],_buf[2],(width*height)/4);
      }
   }
   void copyData(uint8_t * const _buf[3], int linesize1, int linesize2)
   {
      if( bAlloced )
      {
		   memcpy(buf[0],_buf[0],linesize1*height);
		   memcpy(buf[1],_buf[1],linesize2*(height/2));
		   memcpy(buf[2],_buf[2],linesize2*(height/2));
      }
   }
};
typedef noadYUVBuf noadYUVBuf;

#endif //_NOADYUVBUF_H

