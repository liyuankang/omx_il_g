/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract  :  MJPEG Encoder API
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mjpegencapi.h"

typedef struct
{
  u32 fcc; //'avih'
  u32 length;
  u32 MicroSecPerFrame; 
  u32 MaxBytesPerSec; // the max byte rate per second
  u32 PaddingGranularity; 
  u32 Flages; //indicate idx block contains or not, interaced or not
  u32 TotalFrame;
  u32 InitialFrames;
  u32 Streams; //which kind of data in stream
  u32 SuggestedBufferSize;
  u32 Width; //width in the unit of pixel
  u32 Height; //heigh in the unit of pixel
  u32 Reserved[4]; //reserved
}MainAVIHeader;

typedef struct
{
  u32 fcc; //'strh'
  u32 length;
  u32 fccType; // stream type:'auds'(audio),'vids'(video)
  u32 fccHandler; // decoder type: 'MJPG', 'H264' etc
  u32 Flags; 
  u16 Priority;
  u16 Language;
  u32 InitialFrames; 
  u32 Scale; // time scale
  u32 Rate;
  u32 Start; // start time
  u32 Length;  
  u32 SuggestedBufferSize;
  u32 Quality; 
  u32 SampleSize; // audio sample size
  struct 
    {
      u16 left;
      u16 top;
      u16 right;
      u16 bottom;
    } rcFrame;  //the display position of video
  u32 width;
  u32 height;
} AVISTREAMHEADER;

typedef struct
{
  u32 biSize;
  u32 biWidth;
  u32 biHeight;
  u16 biPlanes;
  u16 biBitCount;
  u32 biCompression;
  u32 biSizeImage;
  u32 biXPelsPerMeter;
  u32 biYPelsPerMeter;
  u32 biClrUsed;
  u32 biClrImportant;
}BITMAPINFOHEADER; 


typedef struct
{
  BITMAPINFOHEADER bmiHeader;
  u32 bmiColors[7]; 
}BITMAPINFO;

void MjpegAVIchunkheader(u8 **buf,char *type,char *name,u32 length)
{
  u32 *tmp = (u32 *)*buf;

  if (type!=NULL)
  {
    memcpy(tmp,type,4);
    tmp ++;
  }

  *tmp = length;
  tmp ++;

  if (name!=NULL)
  {
    memcpy(tmp,name,4);
    tmp ++;
  }
  
  *buf = (u8 *)tmp;
}

u32 MjpegEncodeAVIHeader(u32 *outbuf,u32 width,u32 height,u32 frameRateNum, u32 frameRateDenom, u32 frameNum)
{
  MainAVIHeader aviheader;
  AVISTREAMHEADER avistreamheader;
  BITMAPINFO bitInfo;
  u8 *buf = (u8 *)outbuf;
  
  MjpegAVIchunkheader(&buf,"RIFF","AVI ",0); //RIFF:AVI +length
  MjpegAVIchunkheader(&buf,"LIST","hdrl",228); //LIST:hdrl+length

  memcpy(&aviheader.fcc,"avih",4);
  aviheader.length = 56;
  aviheader.MicroSecPerFrame = 1000000 * frameRateDenom / frameRateNum;
  aviheader.MaxBytesPerSec = 0; // the max byte rate per second
  aviheader.PaddingGranularity = 0; 
  aviheader.Flages = 0x910; //indicate idx block contains or not, interaced or not
  aviheader.TotalFrame = frameNum;
  aviheader.InitialFrames = 0;
  aviheader.Streams = 1; //which kind of data in stream
  aviheader.SuggestedBufferSize = 0;
  aviheader.Width = width; //width in the unit of pixel
  aviheader.Height = height; //heigh in the unit of pixel
  memset(&aviheader.Reserved[0],0,sizeof(aviheader.Reserved));
  memcpy(buf,&aviheader,sizeof(MainAVIHeader));
  buf += sizeof(MainAVIHeader);

  MjpegAVIchunkheader(&buf,"LIST","strl",152); //LIST:strl +152

  memcpy(&avistreamheader.fcc,"strh",4);
  avistreamheader.length = 64;
  memcpy(&avistreamheader.fccType,"vids",4); // stream type:'auds'(audio),'vids'(video)
  memcpy(&avistreamheader.fccHandler,"MJPG",4); // decoder type: 'MJPG', 'H264' etc
  avistreamheader.Flags = 0; 
  avistreamheader.Priority = 0;
  avistreamheader.Language = 0;
  avistreamheader.InitialFrames = 0; 
  avistreamheader.Scale = 1000000; // time scale
  avistreamheader.Rate = 1000000 * frameRateNum / frameRateDenom;
  avistreamheader.Start = 0; // start time
  avistreamheader.Length = frameNum;  
  avistreamheader.SuggestedBufferSize = 921600;
  avistreamheader.Quality = 0xFFFFFFFF; 
  avistreamheader.SampleSize = 0; // audio sample size
  avistreamheader.rcFrame.left = 0;
  avistreamheader.rcFrame.top = 0;
  avistreamheader.rcFrame.right = 0;
  avistreamheader.rcFrame.bottom = 0;
  avistreamheader.width = width;
  avistreamheader.height = height;
  memcpy(buf,&avistreamheader,sizeof(AVISTREAMHEADER));
  buf += sizeof(AVISTREAMHEADER);

  MjpegAVIchunkheader(&buf,"strf",NULL,68); //strf+68
  bitInfo.bmiHeader.biSize = 68;
  bitInfo.bmiHeader.biWidth = width;
  bitInfo.bmiHeader.biHeight = height;
  bitInfo.bmiHeader.biPlanes = 1;
  bitInfo.bmiHeader.biBitCount = 24;
  memcpy(&bitInfo.bmiHeader.biCompression,"MJPG",4);
  bitInfo.bmiHeader.biSizeImage = 921600;
  bitInfo.bmiHeader.biXPelsPerMeter = 0;
  bitInfo.bmiHeader.biYPelsPerMeter = 0;
  bitInfo.bmiHeader.biClrUsed = 0;
  bitInfo.bmiHeader.biClrImportant = 0;
  bitInfo.bmiColors[0] = 44;
  bitInfo.bmiColors[1] = 24;
  bitInfo.bmiColors[2] = 0;
  bitInfo.bmiColors[3] = 2;
  bitInfo.bmiColors[4] = 8;
  bitInfo.bmiColors[5] = 2;
  bitInfo.bmiColors[6] = 1;
  memcpy(buf,&bitInfo,sizeof(BITMAPINFO));
  buf += sizeof(BITMAPINFO);

  MjpegAVIchunkheader(&buf,"JUNK",NULL,8); //JUNK+8
  memset(buf,0,8);
  buf += 8;

  return buf-(u8*)outbuf;

}

u32 MjpegEncodeAVIidx(u32 *outbuf,IDX_CHUNK *idx,u32 movi_idx, u32 frameNum)
{
  u32 i = 0;
  memcpy(outbuf,"idx1",4);
  outbuf++;
  *outbuf = sizeof(IDX_CHUNK)*frameNum;
  outbuf++;

  for (i=0;i<frameNum;i++)
  {
   memcpy(&(idx[i].id),"00dc",4);
   idx[i].flags = 0x10;
   idx[i].offset = idx[i].offset - movi_idx;
   
   memcpy(outbuf,&idx[i],sizeof(IDX_CHUNK));
   outbuf += 4;
  }
  return sizeof(IDX_CHUNK)*frameNum + 8;

}