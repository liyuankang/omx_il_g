/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif

#include "base_type.h"
#include "error.h"
#include "hevcencapi.h"
#include "HevcTestBench.h"
#include "enccommon.h"
#include "test_bench_utils.h"
#include "test_bench_pic_config.h"
#ifdef INTERNAL_TEST
#include "sw_test_id.h"
#endif
#ifdef TEST_DATA
#include "enctrace.h"
#endif

#define MAX_LINE_LENGTH_BLOCK 512*8

//           Type POC QPoffset  QPfactor  num_ref_pics ref_pics  used_by_cur
char *RpsDefault_GOPSize_1[] = {
    "Frame1:  P    1   0        0.578     0      1        -1         1",
    NULL,
};
char *RpsDefault_H264_GOPSize_1[] = {
    "Frame1:  P    1   0        0.4     0      1        -1         1",
    NULL,
};

char *RpsDefault_GOPSize_2[] = {
    "Frame1:  P    2   0        0.6     0      1        -2         1",
    "Frame2:  B    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

char *RpsDefault_GOPSize_3[] = {
    "Frame1:  P    3   0        0.5     0      1        -3         1   ",
    "Frame2:  B    1   0        0.5     0      2        -1 2       1 1 ",
    "Frame3:  B    2   0        0.68    0      2        -1 1       1 1 ",
    NULL,
};


char *RpsDefault_GOPSize_4[] = {
    "Frame1:  P    4   0        0.5      0     1       -4         1 ",
    "Frame2:  B    2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  B    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  B    3   0        0.5      0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_5[] = {
    "Frame1:  P    5   0        0.442    0     1       -5         1 ",
    "Frame2:  B    2   0        0.3536   0     2       -2 3       1 1",
    "Frame3:  B    1   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame4:  B    3   0        0.3536   0     2       -1 2       1 1 ",
    "Frame5:  B    4   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_6[] = {
    "Frame1:  P    6   0        0.442    0     1       -6         1 ",
    "Frame2:  B    3   0        0.3536   0     2       -3 3       1 1",
    "Frame3:  B    1   0        0.3536   0     3       -1 2 5     1 1 0",
    "Frame4:  B    2   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame5:  B    4   0        0.3536   0     2       -1 2       1 1 ",
    "Frame6:  B    5   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_7[] = {
    "Frame1:  P    7   0        0.442    0     1       -7         1 ",
    "Frame2:  B    3   0        0.3536   0     2       -3 4       1 1",
    "Frame3:  B    1   0        0.3536   0     3       -1 2 6     1 1 0",
    "Frame4:  B    2   0        0.68     0     3       -1 1 5     1 1 0",
    "Frame5:  B    5   0        0.3536   0     2       -2 2       1 1 ",
    "Frame6:  B    4   0        0.68     0     3       -1 1 3     1 1 0",
    "Frame7:  B    6   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_8[] = {
    "Frame1:  P    8   0        0.442    0  1           -8        1 ",
    "Frame2:  B    4   0        0.3536   0  2           -4 4      1 1 ",
    "Frame3:  B    2   0        0.3536   0  3           -2 2 6    1 1 0 ",
    "Frame4:  B    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  B    3   0        0.68     0  3           -1 1 5    1 1 0",
    "Frame6:  B    6   0        0.3536   0  2           -2 2      1 1",
    "Frame7:  B    5   0        0.68     0  3           -1 1 3    1 1 0",
    "Frame8:  B    7   0        0.68     0  2           -1 1      1 1",
    NULL,
};

char *RpsDefault_Interlace_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8       0   2           -1 -2     0 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_1[] = {
    "Frame1:  B    1   0        0.65      0     2       -1 -2         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_2[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -3         1 1",
    "Frame2:  B    2   0        0.578     0     2       -1 -2         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_3[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -4         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.578     0     2       -1 -3         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_4[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -5         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.4624    0     2       -1 -3         1 1",
    "Frame4:  B    4   0        0.578     0     2       -1 -4         1 1",
    NULL,
};

char *RpsPass2_GOPSize_4[] = {
    "Frame1:  B    4   0        0.5      0     2       -4 -8      1 1",
    "Frame2:  B    2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  B    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  B    3   0        0.5      0     3       -1 -3 1    1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_8[] = {
    "Frame1:  B    8   0        0.442    0  2           -8 -16    1 1",
    "Frame2:  B    4   0        0.3536   0  2           -4 4      1 1",
    "Frame3:  B    2   0        0.3536   0  3           -2 2 6    1 1 0",
    "Frame4:  B    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  B    3   0        0.68     0  4           -1 -3 1 5 1 0 1 0",
    "Frame6:  B    6   0        0.3536   0  3           -2 -6 2   1 0 1",
    "Frame7:  B    5   0        0.68     0  4           -1 -5 1 3 1 0 1 0",
    "Frame8:  B    7   0        0.68     0  3           -1 -7 1   1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_2[] = {
    "Frame1:  B    2   0        0.6     0      2        -2 -4      1 1",
    "Frame2:  B    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

static FILE *yuv_out = NULL;

/*------------------------------------------------------------------------------
  open_file
------------------------------------------------------------------------------*/
FILE *open_file(char *name, char *mode)
{
  FILE *fp;

  if (!(fp = fopen(name, mode)))
  {
    Error(4, ERR, name, ", ", SYSERR);
  }

  return fp;
}


/*------------------------------------------------------------------------------

    WriteStrm
        Write encoded stream to file

    Params:
        fout    - file to write
        strbuf  - data to be written
        size    - amount of data to write
        endian  - data endianess, big or little

------------------------------------------------------------------------------*/
void WriteNals(FILE *fout, u32 *strmbuf, const u32 *pNaluSizeBuf, u32 numNalus, u32 hdrSize, u32 endian)
{
  const u8 start_code_prefix[4] = {0x0, 0x0, 0x0, 0x1};
  u32 count = 0;
  u32 offset = 0;
  u32 size;
  u8  *stream;

#ifdef NO_OUTPUT_WRITE
  return;
#endif

  ASSERT(endian == 0); //FIXME: not support abnormal endian now.

  stream = (u8 *)strmbuf;
  while (count++ < numNalus && *pNaluSizeBuf != 0)
  {
    fwrite(start_code_prefix, 1, 4, fout);
    size = *pNaluSizeBuf++;
    fwrite(stream + offset + size-hdrSize, 1, hdrSize, fout);
    fwrite(stream + offset, 1, size-hdrSize, fout);
    offset += size;
  }

}

#if 0
/*------------------------------------------------------------------------------
    WriteNalSizesToFile
        Dump NAL size to a file

    Params:
        file         - file name where toi dump
        pNaluSizeBuf - buffer where the individual NAL size are (return by API)
        numNalus     - amount of NAL units in the above buffer
------------------------------------------------------------------------------*/
void WriteNalSizesToFile(FILE *file, const u32 *pNaluSizeBuf,
                         u32 numNalus)
{
  u32 offset = 0;


  while (offset++ < numNalus && *pNaluSizeBuf != 0)
  {
    fprintf(file, "%d\n", *pNaluSizeBuf++);
  }


}
#endif


/*------------------------------------------------------------------------------

    WriteStrm
        Write encoded stream to file

    Params:
        fout    - file to write
        strbuf  - data to be written
        size    - amount of data to write
        endian  - data endianess, big or little

------------------------------------------------------------------------------*/
void WriteStrm(FILE *fout, u32 *strmbuf, u32 size, u32 endian)
{

#ifdef NO_OUTPUT_WRITE
  return;
#endif

  if (!fout || !strmbuf || !size) return;

  /* Swap the stream endianess before writing to file if needed */
  if (endian == 1)
  {
    u32 i = 0, words = (size + 3) / 4;

    while (words)
    {
      u32 val = strmbuf[i];
      u32 tmp = 0;

      tmp |= (val & 0xFF) << 24;
      tmp |= (val & 0xFF00) << 8;
      tmp |= (val & 0xFF0000) >> 8;
      tmp |= (val & 0xFF000000) >> 24;
      strmbuf[i] = tmp;
      words--;
      i++;
    }

  }
  //printf("Write out stream size %d \n", size);
  /* Write the stream to file */
  fwrite(strmbuf, 1, size, fout);
}

void writeIvfHeader(FILE *fout, i32 width, i32 height, i32 rateNum, i32 rateDenom, i32 vp9)
{
	u8 data[32] = {0}; //Ivf stream headre
	/*IVF file header signature*/
    data[0] = 'D';
    data[1] = 'K';
    data[2] = 'I';
    data[3] = 'F';

    /*Header size*/
    data[6] = 32;

    /*Codec ForrCC*/
	if(!vp9){
	  data[8] = 'A';
      data[9] = 'V';
      data[10] = '0';
      data[11] = '1';
	}else{
      data[8] = 'V';
      data[9] = 'P';
      data[10] = '9';
      data[11] = '0';
	}

    /*Video width and height*/
    data[12] = width & 0xff;
    data[13] = (width >> 8) & 0xff;
    data[14] = height & 0xff;
    data[15] = (height >> 8) & 0xff;

    /*Frame rate*/
    data[16] = rateNum & 0xff;
    data[17] = (rateNum >> 8) & 0xff;
    data[18] = (rateNum >> 16) & 0xff;
    data[19] = (rateNum >> 24) & 0xff;

    /*Frame rate scale*/
    data[20] = rateDenom & 0xff;
    data[21] = (rateDenom >> 8) & 0xff;
    data[22] = (rateDenom >> 16) & 0xff;
    data[23] = (rateDenom >> 24) & 0xff;

    /*Video length in frames*/
    /*data[24] = (pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) & 0xff;
    data[25] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 8) & 0xff;
    data[26] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 16) & 0xff;
    data[27] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 24) & 0xff;*/
    /*It's OK to not provide useful info*/
    data[24] = 0;
    data[25] = 0;
    data[26] = 0;
    data[27] = 0;

	WriteStrm(fout, (u32 *)data, 32, 0);
}

void writeIvfFrame(FILE *fout, VCEncStrmBufs *bufs, u32 streamSize, u32 offset, u64 frameCnt)
{
    i32 byteCnt = 0;
    u64 frameCntOut = frameCnt;
    u8 data[12]; //Ivf frame header size

    byteCnt = streamSize;
    data[0] = byteCnt & 0xff;
    data[1] = (byteCnt >> 8) & 0xff;
    data[2] = (byteCnt >> 16) & 0xff;
    data[3] = (byteCnt >> 24) & 0xff;

    /*Time stamp*/
    data[4]  =  (frameCntOut)        & 0xff;
    data[5]  = ((frameCntOut) >> 8)  & 0xff;
    data[6]  = ((frameCntOut) >> 16) & 0xff;
    data[7]  = ((frameCntOut) >> 24) & 0xff;
    data[8]  = ((frameCntOut) >> 32) & 0xff;
    data[9]  = ((frameCntOut) >> 40) & 0xff;
    data[10] = ((frameCntOut) >> 48) & 0xff;
    data[11] = ((frameCntOut) >> 56) & 0xff;

    /*Write out ivf frame header*/
    WriteStrm(fout, (u32 *)data, 12, 0);
    /*Write out frame buf*/
    writeStrmBufs(fout, bufs, offset, streamSize, 0);
}

/*------------------------------------------------------------------------------

    WriteStrmBufs
        Write encoded stream to file

    Params:
        fout - file to write
        bufs - stream buffers
        offset - stream buffer offset
        size - amount of data to write
        endian - data endianess, big or little

------------------------------------------------------------------------------*/
void writeStrmBufs (FILE *fout, VCEncStrmBufs *bufs, u32 offset, u32 size, u32 endian)
{
  u8 *buf0 = bufs->buf[0];
  u8 *buf1 = bufs->buf[1];
  u32 buf0Len = bufs->bufLen[0];

  if (!buf0) return;

  if (offset < buf0Len)
  {
    u32 size0 = MIN(size, buf0Len-offset);
    WriteStrm(fout, (u32 *)(buf0 + offset), size0, endian);
    if ((size0 < size) && buf1)
      WriteStrm(fout, (u32 *)buf1, size-size0, endian);
  }
  else if (buf1)
  {
    WriteStrm(fout, (u32 *)(buf1 + offset - buf0Len), size, endian);
  }
}

/*------------------------------------------------------------------------------

    WriteNalsBufs
        Write encoded stream to file

    Params:
        fout - file to write
        bufs - stream buffers
        pNaluSizeBuf - nalu size buffer
        numNalus - the number of nalu
        offset - stream buffer offset
        hdrSize - header size
        endian - data endianess, big or little

------------------------------------------------------------------------------*/
void writeNalsBufs (FILE *fout, VCEncStrmBufs *bufs, const u32 *pNaluSizeBuf, u32 numNalus, u32 offset, u32 hdrSize, u32 endian)
{
  const u8 start_code_prefix[4] = {0x0, 0x0, 0x0, 0x1};
  u32 count = 0;
  u32 size;
  u8  *stream;

#ifdef NO_OUTPUT_WRITE
  return;
#endif

  ASSERT(endian == 0); //FIXME: not support abnormal endian now.

  while (count++ < numNalus && *pNaluSizeBuf != 0)
  {
    fwrite(start_code_prefix, 1, 4, fout);
    size = *pNaluSizeBuf++;

    writeStrmBufs (fout, bufs, offset + size - hdrSize, hdrSize,        endian);
    writeStrmBufs (fout, bufs, offset,                  size - hdrSize, endian);

    offset += size;
  }
}

void getStreamBufs (VCEncStrmBufs *bufs, struct test_bench *tb, commandLine_s *cml, bool encoding)
{
  i32 i;
  for (i = 0; i < MAX_STRM_BUF_NUM; i ++)
  {
    bufs->buf[i]    = tb->outbufMem[i] ? (u8 *)tb->outbufMem[i]->virtualAddress : NULL;
    bufs->bufLen[i] = tb->outbufMem[i] ? tb->outbufMem[i]->size : 0;
  }

#ifdef INTERNAL_TEST
  if (encoding && (cml->testId == TID_INT) && cml->streamBufChain && (cml->parallelCoreNum <= 1))
    HevcStreamBufferLimitTest(NULL, bufs);
#endif
}

/*------------------------------------------------------------------------------

    WriteScaled
        Write scaled image to file "scaled_wxh.yuyv", support interleaved yuv422 only.

    Params:
        strbuf  - data to be written
        inst    - encoder instance

------------------------------------------------------------------------------*/
void WriteScaled(u32 *strmbuf, struct commandLine_s *cml)
{

#ifdef NO_OUTPUT_WRITE
  return;
#endif

  if (!strmbuf)
    return;

  u32 format = cml->scaledOutputFormat;
  u32 width = (cml->rotation&&cml->rotation!=3 ? cml->scaledHeight : cml->scaledWidth);
  u32 height = (cml->rotation&&cml->rotation!=3 ? cml->scaledWidth : cml->scaledHeight);
  u32 size =  width * height * 2;
  if (format == 1)
    size = width * height * 3 / 2;

  if (!size)
    return;

  char fname[100];
  if (format == 0)
    sprintf(fname, "scaled_%dx%d.yuyv", width, height);
  else
    sprintf(fname, "scaled_%dx%d.yuv", width, height);

  FILE *fout = fopen(fname, "ab+");
  if (fout == NULL)
    return;

  fwrite(strmbuf, 1, size, fout);

  fclose(fout);
}

void writeFlags2Memory(char flag, u8* memory,u16 column,u16 row,u16 blockunit,u16 width, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  u32 blks_per_ctb = ctb_size/8;
  u32 blks_per_unit = 1<<(3-blockunit);
  u32 ctb_row_number    = row*blks_per_unit    / blks_per_ctb;
  u32 ctb_column_number = column*blks_per_unit / blks_per_ctb;
  u32 ctb_row_stride = ctb_per_row * blks_per_ctb * blks_per_ctb;
  u32 xoffset = (column*blks_per_unit) % blks_per_ctb;
  u32 yoffset = (row*blks_per_unit) % blks_per_ctb;
  u32 stride = blks_per_ctb;
  u32 columns, rows, r, c;

  rows = columns = blks_per_unit;
  if(blks_per_ctb < blks_per_unit)
  {
    rows = MIN(rows, ctb_per_column*blks_per_ctb - row*blks_per_unit);
    columns = MIN(columns, ctb_per_row*blks_per_ctb - column*blks_per_unit);
    rows /= blks_per_ctb;
    columns *= blks_per_ctb;
    stride = ctb_row_stride;
  }

  // ctb addr --> blk addr
  memory += ctb_row_number*ctb_row_stride + ctb_column_number*(blks_per_ctb*blks_per_ctb);
  memory += yoffset * stride + xoffset;
  for(r=0;r<rows;r++)
  {
    u8 *dst = memory + r*stride;
    u8 val;
    for(c=0;c<columns;c++)
    {
      val = *dst;
      *dst++ = (val&0x7f) | (flag << 7);
    }
  }
}

void writeFlagsRowData2Memory(char* rowStartAddr,u8* memory,u16 width,u16 rowNumber,u16 blockunit, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  int i;
  i=0;
  while(i<width)
  {
    writeFlags2Memory(*rowStartAddr,memory,i,rowNumber,blockunit,width, ctb_size, ctb_per_row, ctb_per_column);
    rowStartAddr++;
    i++;
  }
}

void writeQpValue2Memory(char qpDelta, u8* memory,u16 column,u16 row,u16 blockunit,u16 width, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  u32 blks_per_ctb = ctb_size/8;
  u32 blks_per_unit = 1<<(3-blockunit);
  u32 ctb_row_number    = row*blks_per_unit    / blks_per_ctb;
  u32 ctb_column_number = column*blks_per_unit / blks_per_ctb;
  u32 ctb_row_stride = ctb_per_row * blks_per_ctb * blks_per_ctb;
  u32 xoffset = (column*blks_per_unit) % blks_per_ctb;
  u32 yoffset = (row*blks_per_unit) % blks_per_ctb;
  u32 stride = blks_per_ctb;
  u32 columns, rows, r, c;

  rows = columns = blks_per_unit;
  if(blks_per_ctb < blks_per_unit)
  {
    rows = MIN(rows, ctb_per_column*blks_per_ctb - row*blks_per_unit);
    columns = MIN(columns, ctb_per_row*blks_per_ctb - column*blks_per_unit);
    rows /= blks_per_ctb;
    columns *= blks_per_ctb;
    stride = ctb_row_stride;
  }

  // ctb addr --> blk addr
  memory += ctb_row_number*ctb_row_stride + ctb_column_number*(blks_per_ctb*blks_per_ctb);
  memory += yoffset * stride + xoffset;
  for(r=0;r<rows;r++)
  {
    u8 *dst = memory + r*stride;
    for(c=0;c<columns;c++)
      *dst++ = qpDelta;
  }
}


void writeQpDeltaData2Memory(char qpDelta,u8* memory,u16 column,u16 row,u16 blockunit,u16 width, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  u8 twoBlockDataCombined;
  int r,c;
  u32 blks_per_ctb = ctb_size/8;
  u32 blks_per_unit = (1<<(3-blockunit));
  u32 ctb_row_number=row*blks_per_unit/blks_per_ctb;
  u32 ctb_row_stride=ctb_per_row*(blks_per_ctb*blks_per_ctb)/2;
  u32 ctb_row_offset=(blks_per_ctb < blks_per_unit ? 0 : row-(row/(blks_per_ctb/blks_per_unit))*(blks_per_ctb/blks_per_unit))*blks_per_unit;
  u32 internal_ctb_stride=blks_per_ctb/2;
  u32 ctb_column_number;
  u8* rowMemoryStartPtr=memory+ctb_row_number*ctb_row_stride;
  u8* ctbMemoryStartPtr, *curMemoryStartPtr;
  u32 columns,rows;
  u32 xoffset;

  {
    ctb_column_number=column*blks_per_unit/blks_per_ctb;
    ctbMemoryStartPtr=rowMemoryStartPtr+ctb_column_number*(blks_per_ctb*blks_per_ctb)/2;
    switch(blockunit)
    {
      case 0:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=8;
        columns=4;
        xoffset=0;
        break;
      case 1:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=4;
        columns=2;
        xoffset=column%((ctb_size+31)/32);
        xoffset=xoffset<<1;
        break;
      case 2:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=2;
        columns=1;
        xoffset=column%((ctb_size+15)/16);
        break;
      case 3:
        xoffset=column>>1;
        xoffset=xoffset%((ctb_size+15)/16);
        curMemoryStartPtr=ctbMemoryStartPtr+ctb_row_offset*internal_ctb_stride + xoffset;
        twoBlockDataCombined=*curMemoryStartPtr;
        if(column%2)
        {
          twoBlockDataCombined=(twoBlockDataCombined&0x0f)|(((-qpDelta&0X0F))<<4);
        }
        else
        {
          twoBlockDataCombined=(twoBlockDataCombined&0xf0)|(-qpDelta&0X0F);
        }
        rows=1;
        columns=1;
        break;
      default:
        rows=0;
        twoBlockDataCombined=0;
        columns=0;
        xoffset=0;
        break;
    }
    u32 stride = internal_ctb_stride;
    if(blks_per_ctb < blks_per_unit) {
      rows = MIN(rows, ctb_per_column*blks_per_ctb-row*blks_per_unit);
      columns = MIN(columns, (ctb_per_row*blks_per_ctb-column*blks_per_unit)/2);
      rows /= blks_per_ctb;
      columns *= blks_per_ctb;
      stride = ctb_row_stride;
    }
    for(r=0;r<rows;r++)
    {
      curMemoryStartPtr=ctbMemoryStartPtr+(ctb_row_offset+r)*stride + xoffset;
      for(c=0;c<columns;c++)
      {
        *curMemoryStartPtr++=twoBlockDataCombined;
      }
    }
  }
}

void writeQpDeltaRowData2Memory(char* qpDeltaRowStartAddr,u8* memory,u16 width,u16 rowNumber,u16 blockunit, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column, i32 roiMapVersion)
{
  i32 i = 0;
  while(i<width)
  {
    if (roiMapVersion >= 1)
      writeQpValue2Memory(*qpDeltaRowStartAddr,memory,i,rowNumber,blockunit,width, ctb_size, ctb_per_row, ctb_per_column);
    else
      writeQpDeltaData2Memory(*qpDeltaRowStartAddr,memory,i,rowNumber,blockunit,width, ctb_size, ctb_per_row, ctb_per_column);

    qpDeltaRowStartAddr++;
    i++;
  }
}

i32 copyQPDelta2Memory (commandLine_s *cml, VCEncInst enc, struct test_bench *tb, i32 roiMapVersion)
{
#define MAX_LINE_LENGTH_BLOCK 512*8

  i32 ret = OK;
  u32 ctb_per_row = ((cml->width+cml->max_cu_size-1)/ (cml->max_cu_size));
  u32 ctb_per_column = ((cml->height+cml->max_cu_size-1)/ (cml->max_cu_size));
  FILE *roiMapFile = tb->roiMapFile;
  u8* memory = (u8*)tb->roiMapDeltaQpMem->virtualAddress;
  u16 block_unit;
  switch(cml->roiMapDeltaQpBlockUnit)
  {
    case 0:
      block_unit=64;
      break;
    case 1:
      block_unit=32;
      break;
    case 2:
      block_unit=16;
      break;
    case 3:
      block_unit=8;
      break;
    default:
      block_unit=64;
      break;
  }
  u16 width = (((cml->width+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))+block_unit-1)/block_unit;
  u16 height = (((cml->height+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))+block_unit-1)/block_unit;
  u16 blockunit = cml->roiMapDeltaQpBlockUnit;
  u16 ctb_size = cml->max_cu_size;
  char rowbuffer[1024];
  i32 line_idx = 0, i;
  i32 qpdelta, qptype, qpdelta_num=0;
  char* rowbufferptr;
  char *achParserBuffer = (char *)malloc(sizeof(char) * MAX_LINE_LENGTH_BLOCK);

  if (achParserBuffer == NULL)
  {
    printf ("Qp delta config Error: fail to alloc buffer!\n");
    ret = NOK;
    goto copyEnd;
  }

  if (roiMapFile == NULL)
  {
    //printf("Qp delta config: Error, Can Not Open File %s\n", fname );
    ret = NOK;
    goto copyEnd;
  }

  while (line_idx < height)
  {
    achParserBuffer[0] = '\0';
    // Read one line
    char *line = fgets ((char *) achParserBuffer, MAX_LINE_LENGTH_BLOCK, roiMapFile);
    if (feof (roiMapFile))
    {
        fseek(roiMapFile,0L,SEEK_SET);
        if (!line)
          continue;
    }

    if (!line) break;
    //handle line end
    char* s = strpbrk(line, "#\n");
    if(s) *s = '\0';

    if (line)
    {
      i=0;
      rowbufferptr=rowbuffer;
      memset(rowbufferptr, 0, 1024);
      while(i<width)
      {
        // read data from file
        if (roiMapVersion)
        {
          //format: a%d: absolute QP; %d: QP delta
          qptype = 0;
          if (*line == 'a')
          {
            qptype = 1;
            line ++;
          }
          sscanf (line, "%d", &qpdelta);
          if (qptype)
          {
            qpdelta = CLIP3(0,51,qpdelta);
#if 0
            if (qpdelta<0 || qpdelta>51)
            {
              printf("ROI Map File Error: Absolute Qp out of range: %d\n", qpdelta);
              ret = NOK;
              goto copyEnd;
            }
#endif
          }
          else
          {
            qpdelta = CLIP3(-31,32,qpdelta);
#if 0
            if (qpdelta<-31 || qpdelta>32)
            {
              printf("ROI Map File Error: Qp Delta out of range: %d\n", qpdelta);
              ret = NOK;
              goto copyEnd;
            }
#endif
            qpdelta = -qpdelta;
          }
          /* setup the map value */
          qpdelta &= 0x3f;
          if (roiMapVersion == 1)
          {
            qpdelta = (qpdelta << 1) | qptype;
          }
          else if (roiMapVersion == 2)
          {
            qpdelta |= (qptype ? 0 : 0x40);
          }
        }
        else
        {
          sscanf (line, "%d", &qpdelta);
          qpdelta = CLIP3(-15,0,qpdelta);
#if 0
          if(qpdelta>0||qpdelta<-15)
          {
            printf("Qp Delta out of range.\n");
              ret = NOK;
              goto copyEnd;
          }
#endif
        }

        // get qpdelta
        *(rowbufferptr++)=(char)qpdelta;
        i++;
        qpdelta_num++;

        // find next qpdelta
        line= strchr(line, ',');
        if (line)
        {
          while ((*line == ',') || (*line == ' ')) line ++;
          if (*line == '\0') break;
        }
        else
          break;
      }
      writeQpDeltaRowData2Memory(rowbuffer, memory, width, line_idx,
                                 blockunit, ctb_size, ctb_per_row, ctb_per_column,
                                 roiMapVersion);
      line_idx ++;
    }
  }
  //fclose(fIn);
  if (qpdelta_num != width*height)
  {
    printf ("QP delta Config: Error, Parsing File Failed\n");
    ret = NOK;
  }

copyEnd:
  if(achParserBuffer) free (achParserBuffer);
  return ret;
}


void clearQpDeltaMap(commandLine_s *cml, VCEncInst enc, struct test_bench *tb)
{
  const EWLHwConfig_t asicCfg = VCEncGetAsicConfig(cml->codecFormat);
  i32 block_size=((cml->width+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))*((cml->height+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))/(8*8*2);
  if (asicCfg.roiMapVersion >= 1)
    block_size *= 2;
  memset(tb->roiMapDeltaQpMem->virtualAddress, 0, block_size);
}

int copyFlagsMap2Memory(commandLine_s *cml, VCEncInst enc, struct test_bench *tb)
{
  u32 ctb_per_row = ((cml->width+cml->max_cu_size-1)/ (cml->max_cu_size));
  u32 ctb_per_column = ((cml->height+cml->max_cu_size-1)/ (cml->max_cu_size));
  u8* memory = (u8*)tb->roiMapDeltaQpMem->virtualAddress;
  u16 ctb_size = cml->max_cu_size;
  u16 blockunit;
  char achParserBuffer[MAX_LINE_LENGTH_BLOCK];
  char rowbuffer[MAX_LINE_LENGTH_BLOCK];
  int line_idx = 0,i;
  int flag, flag_num=0;
  char* rowbufferptr;
  u16 width, height, block_unit_size;
  const EWLHwConfig_t asicCfg = VCEncGetAsicConfig(cml->codecFormat);
  i32 roiMapVersion = asicCfg.roiMapVersion;
  FILE *mapFile = NULL;

  if(roiMapVersion == 3)
    roiMapVersion = cml->RoiQpDeltaVer;

  if (roiMapVersion == 1)
  {
    mapFile = tb->ipcmMapFile;
    blockunit = (ctb_size == 64 ? 0 : 2);
  }
  else if (roiMapVersion == 2)
  {
    i32 blockUnitMax = IS_H264(cml->codecFormat) ? 2 : 1;
    mapFile = tb->skipMapFile;
    blockunit = cml->skipMapBlockUnit;
    if (blockunit > blockUnitMax)
    {
      printf ("SKIP Map Config: Error, Block size too small, changed to %dx%d\n", 8<<(3-blockUnitMax), 8<<(3-blockUnitMax));
      blockunit = blockUnitMax;
    }
  }

  if (mapFile == NULL)
  {
    //printf("Qp delta config: Error, Can Not Open File %s\n", fname );
    return -1;
  }

  block_unit_size = 8 << (3-blockunit);
  width = (((cml->width+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))+block_unit_size-1)/block_unit_size;
  height = (((cml->height+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))+block_unit_size-1)/block_unit_size;

  if (!tb->roiMapFile)
  {
    i32 block_size = ((cml->width+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))*((cml->height+cml->max_cu_size-1)& (~(cml->max_cu_size - 1)))/(8*8*2);
    if (roiMapVersion)
      block_size *= 2;
    memset(tb->roiMapDeltaQpMem->virtualAddress, 0, block_size);
  }

  while (line_idx < height)
  {
    achParserBuffer[0] = '\0';
    // Read one line
    char *line = fgets ((char *) achParserBuffer, MAX_LINE_LENGTH_BLOCK, mapFile);
    if (feof (mapFile))
    {
        fseek(mapFile,0L,SEEK_SET);
        if (!line)
          continue;
    }

    if (!line) break;
    //handle line end
    char* s = strpbrk(line, "#\n");
    if(s) *s = '\0';

    if (line)
    {
      i=0;
      rowbufferptr=rowbuffer;
      memset(rowbufferptr, 0, MAX_LINE_LENGTH_BLOCK);

      while(i<width)
      {
        sscanf (line, "%d", &flag);
        if(flag < 0 || flag > 1)
        {
          printf("Invalid IPCM/SKIP map entry.\n");
          return -1;
        }
        *(rowbufferptr++)=(char)flag;
        i++;
        flag_num++;
        line= strchr(line, ',');
        if (line)
        {
          while (*line == ',') line ++;
          if (*line == '\0') break;
        }
        else
          break;
      }
      writeFlagsRowData2Memory(rowbuffer,memory,width,line_idx,blockunit, ctb_size, ctb_per_row, ctb_per_column);
      line_idx ++;
    }
  }

  if (flag_num != width*height)
  {
    printf ("IPCM/SKIP Map Config: Error, Parsing File Failed\n");
    return -1;
  }

  return 0;
}

float getPixelWidthInByte(VCEncPictureType type)
{
  switch (type)
  {
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      return 1.25;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
      return 2;
    case VCENC_YUV420_PLANAR_10BIT_I010:
    case VCENC_YUV420_PLANAR_10BIT_P010:
      return 2;
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
    case VCENC_YUV420_SEMIPLANAR_101010:
      return 1;
    default:
      return 1;
  }
}

void getAlignedPicSizebyFormat(VCEncPictureType type, u32 width, u32 height, u32 alignment,
                                       u32 *luma_Size, u32 *chroma_Size, u32 *picture_Size)
{
  u32 luma_stride = 0, chroma_stride = 0;
  u32 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  VCEncGetAlignedStride(width, type, &luma_stride, &chroma_stride, alignment);
  switch(type)
  {
    case VCENC_YUV420_PLANAR:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
     lumaSize = luma_stride * height;
     chromaSize = 0;
     break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case VCENC_YUV420_PLANAR_10BIT_P010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
     lumaSize = luma_stride *10/8 * height;
     chromaSize = chroma_stride *10/8* height/2*2;
     break;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
     lumaSize = luma_stride *2*2* height/2;
     chromaSize = 0;
     break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC:
     lumaSize = luma_stride * ((height + 32 - 1) & (~(32 - 1)));
     chromaSize = lumaSize/2;
     break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_H264:
     lumaSize = luma_stride * height * 2* 12/ 8;
     chromaSize = 0;
     break;
    case VCENC_YUV420_SEMIPLANAR_8BIT_FB:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_PLANAR_10BIT_P010_FB:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_SEMIPLANAR_101010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
     lumaSize = luma_stride *  ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_32_4:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
     lumaSize = luma_stride * ((height+1)/2);
     chromaSize = chroma_stride * (((height/2)+1)/2);
     break;
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
     lumaSize = luma_stride * ((height+1)/2);
     chromaSize = chroma_stride * (((height/2)+1)/2);
     break;
    case VCENC_YUV420_8BIT_TILE_8_8:
     lumaSize = luma_stride * ((height+7)/8);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_8_8:
     lumaSize = luma_stride * ((height+7)/8);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    default:
     printf("not support this format\n");
     chromaSize = lumaSize = 0;
     break;
  }

  pictureSize = lumaSize + chromaSize;
  if (luma_Size != NULL)
    *luma_Size = lumaSize;
  if (chroma_Size != NULL)
    *chroma_Size = chromaSize;
  if (picture_Size != NULL)
    *picture_Size = pictureSize;
}

void getDec400CompTablebyFormat(VCEncPictureType type,u32 width, u32 height, u32 alignment,
                                       u32 *luma_Size,u32 *chroma_Size,u32 *picture_Size)
{
  u32 luma_stride=0, chroma_stride = 0;
  u32 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  if (alignment == 0)
    alignment = 256;

  VCEncGetAlignedStride(width,type,&luma_stride,&chroma_stride,alignment);
  switch(type)
  {
    case VCENC_YUV420_PLANAR:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8*2;
     break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8;
     break;
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = 0;
     break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8*2;
     break;
    case VCENC_YUV420_PLANAR_10BIT_P010:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8;
     break;
    default:
     printf("not support this format\n");
     chromaSize = lumaSize = 0;
     break;
  }

  pictureSize = lumaSize + chromaSize;
  if (luma_Size != NULL)
    *luma_Size = lumaSize;
  if (chroma_Size != NULL)
    *chroma_Size = chromaSize;
  if (picture_Size != NULL)
    *picture_Size = pictureSize;

}


void ChangeToCustomizedFormat(commandLine_s *cml,VCEncPreProcessingCfg *preProcCfg)
{
  if ((cml->formatCustomizedType == 0)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    if (IS_HEVC(cml->codecFormat))
      preProcCfg->inputType = VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC;
    else
      preProcCfg->inputType = VCENC_YUV420_PLANAR_8BIT_DAHUA_H264;
    preProcCfg->origWidth = ((preProcCfg->origWidth + 16 - 1) & (~(16 - 1)));
  }

  if ((cml->formatCustomizedType == 1)&&
     ((cml->inputFormat==VCENC_YUV420_SEMIPLANAR)||(cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)||(cml->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010)))
  {
   if (cml->inputFormat==VCENC_YUV420_SEMIPLANAR)
     preProcCfg->inputType = VCENC_YUV420_SEMIPLANAR_8BIT_FB;
   else if (cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)
     preProcCfg->inputType = VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB;
   else
     preProcCfg->inputType = VCENC_YUV420_PLANAR_10BIT_P010_FB;
  }

  if ((cml->formatCustomizedType == 2)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_SEMIPLANAR_101010;
    preProcCfg->xOffset = preProcCfg->xOffset /6 *6;
  }

  if ((cml->formatCustomizedType == 3)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_8BIT_TILE_64_4;
    preProcCfg->xOffset = preProcCfg->xOffset /64 *64;
    preProcCfg->yOffset = preProcCfg->yOffset /4 *4;
  }

  if ((cml->formatCustomizedType == 4)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_UV_8BIT_TILE_64_4;
    preProcCfg->xOffset = preProcCfg->xOffset /64 *64;
    preProcCfg->yOffset = preProcCfg->yOffset /4 *4;
  }

  if ((cml->formatCustomizedType == 5)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_10BIT_TILE_32_4;
    preProcCfg->xOffset = preProcCfg->xOffset /32 *32;
    preProcCfg->yOffset = preProcCfg->yOffset /4 *4;
  }

  if ((cml->formatCustomizedType == 6)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_10BIT_TILE_48_4;
    preProcCfg->xOffset = preProcCfg->xOffset /48 *48;
    preProcCfg->yOffset = preProcCfg->yOffset /4 *4;
  }

  if ((cml->formatCustomizedType == 7)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_VU_10BIT_TILE_48_4;
    preProcCfg->xOffset = preProcCfg->xOffset /48 *48;
    preProcCfg->yOffset = preProcCfg->yOffset /4 *4;
  }

  if ((cml->formatCustomizedType == 8)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_8BIT_TILE_128_2;
    preProcCfg->xOffset = preProcCfg->xOffset /128 *128;
  }

  if ((cml->formatCustomizedType == 9)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_UV_8BIT_TILE_128_2;
    preProcCfg->xOffset = preProcCfg->xOffset /128 *128;
  }

  if ((cml->formatCustomizedType == 10)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_10BIT_TILE_96_2;
    preProcCfg->xOffset = preProcCfg->xOffset /96 *96;
  }

  if ((cml->formatCustomizedType == 11)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_VU_10BIT_TILE_96_2;
    preProcCfg->xOffset = preProcCfg->xOffset /96 *96;
  }

  if ((cml->formatCustomizedType == 12)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_8BIT_TILE_8_8;
    preProcCfg->xOffset = 0;
    preProcCfg->yOffset = 0;
  }

  if ((cml->formatCustomizedType == 13)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    preProcCfg->inputType = VCENC_YUV420_10BIT_TILE_8_8;
    preProcCfg->xOffset = 0;
    preProcCfg->yOffset = 0;
  }

}

void ChangeCmlCustomizedFormat(commandLine_s *cml)
{
  if ((cml->formatCustomizedType == 0)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    if (IS_H264(cml->codecFormat))
    {
      if (cml->verOffsetSrc!=DEFAULT)
        cml->verOffsetSrc = cml->verOffsetSrc&(~(16-1));
      if (cml->horOffsetSrc!=DEFAULT)
        cml->horOffsetSrc = cml->horOffsetSrc&(~(16-1));
    }
    else
    {
      if (cml->verOffsetSrc!=DEFAULT)
        cml->verOffsetSrc = cml->verOffsetSrc&(~(32-1));
      if (cml->horOffsetSrc!=DEFAULT)
        cml->horOffsetSrc = cml->horOffsetSrc&(~(32-1));
    }
    cml->width = cml->width&(~(16-1));
    cml->height = cml->height&(~(16-1));
    cml->scaledWidth = cml->scaledWidth&(~(16-1));
    cml->scaledHeight = cml->scaledHeight&(~(16-1));
  }

  if ((cml->formatCustomizedType == 1)&&
       ((cml->inputFormat==VCENC_YUV420_SEMIPLANAR)||(cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)||(cml->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010)))
  {
    cml->verOffsetSrc = 0;
    cml->horOffsetSrc = 0;
    if (cml->testId == 16)
      cml->testId = 0;

    cml->rotation = 0;
  }
  else if (cml->formatCustomizedType == 1)
  {
    cml->formatCustomizedType = -1;
  }

  if (((cml->formatCustomizedType >= 2) && (cml->formatCustomizedType <= 11))&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    if (cml->testId == 16)
      cml->testId = 0;
    cml->rotation = 0;
  }
  else if (((cml->formatCustomizedType >= 12) && (cml->formatCustomizedType <= 13))&&(cml->inputFormat==VCENC_YUV420_PLANAR))
  {
    if (cml->testId == 16)
      cml->testId = 0;
  }
  else
    cml->formatCustomizedType = -1;

}

void transYUVtoDHformat(struct test_bench *tb, commandLine_s *cml)
{
   u8 *transform_buf;
   u32 i,j,k,col,row,w,h;
   u8 *start_add,*src_addr,*tile_addr,*cb_start_add,*cr_start_add,*cb_tile_add,*cr_tile_add,*cb_src_add,*cr_src_add;
   VCEncIn *pEncIn = &(tb->encIn);
   transform_buf = (u8 *)tb->transformMem->virtualAddress;

   if (IS_HEVC(cml->codecFormat))//hevc format
     {
      printf("transform YUV to DH HEVC\n");
      u32 row_32 = ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1)))/32;
      u32 num32_per_row = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1)))/32;
      //luma
      for (i=0;i<row_32;i++)
      {
       start_add = tb->lum+i*32*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
       for (j=0;j<num32_per_row*2;j++)
        {
         tile_addr = start_add+j*16;
         if (j<(((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/16))
         {
           for(k=0;k<32;k++)
           {
             if ((i*32+k)>=cml->lumHeightSrc)
             {
               for(col=0;col<16;col++)
               {
                 *transform_buf++ = 0;
               }
             }
             else
             {
               src_addr = tile_addr;
               for(col=0;col<16;col++)
               {
                 *transform_buf++ = *(src_addr+col);
               }
               tile_addr = tile_addr + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
             }
           }
         }
         else
         {
           for(k=0;k<32;k++)
           {
            for(col=0;col<16;col++)
             {
              *transform_buf++ = 0;
             }
           }
         }
        }
      }
      //chroma
      for (i=0;i<row_32;i++)
      {
       cb_start_add = tb->cb+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
       cr_start_add = tb->cr+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;

       for (j=0;j<num32_per_row;j++)
       {
         cb_tile_add = cb_start_add+j*16;
         cr_tile_add = cr_start_add+j*16;

         for(k=0;k<16;k++)
         {
           if ((i*16+k)>=(cml->lumHeightSrc/2))
           {
             for(col=0;col<32;col++)
             {
              *transform_buf++ = 0;
             }
           }
           else
           {
             cb_src_add = cb_tile_add;
             cr_src_add = cr_tile_add;
             //cb
             for(col=0;col<16;col++)
             {
              if (16*j+col>=(((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2))
                *transform_buf++ = 0;
              else
                *transform_buf++ = *(cb_src_add+col);
             }
             //cr
             for(col=0;col<16;col++)
             {
              if (16*j+col>=(((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2))
                *transform_buf++ = 0;
              else
                *transform_buf++ = *(cr_src_add+col);
             }
             cb_tile_add = cb_tile_add + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
             cr_tile_add = cr_tile_add + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
           }
         }
        }
       }
      }
      else//h264 format
      {
       printf("transform YUV to DH H264\n");
       u32 row_16 = ((cml->lumHeightSrc + 16 - 1) & (~(16 - 1)))/16;
       u32 mb_per_row = ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/16;
       u32 mb_total = 0;
       for (i=0;i<row_16;i++)
       {
         start_add = tb->lum+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
         cb_start_add = tb->cb+i*8*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
         cr_start_add = tb->cr+i*8*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
         for(j=0;j<mb_per_row;j++)
         {
          tile_addr = start_add+16*j;
          cb_tile_add = cb_start_add+8*j;
          cr_tile_add = cr_start_add+8*j;
          for(k=0;k<4;k++)
          {
            for (row=0;row<4;row++)
            {
              //luma
              if ((i*16+k*4+row)<cml->lumHeightSrc)
              {
                src_addr = tile_addr+(k*4+row)*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
                memcpy(transform_buf,src_addr,16);
                transform_buf+=16;
              }
              else
              {
                memset(transform_buf,0,16);
                transform_buf+=16;
              }
            }

            //cb
            for (row=0;row<2;row++)
            {
              if ((i*8+k*2+row)<cml->lumHeightSrc/2)
              {
                src_addr = cb_tile_add+(k*2+row)*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
                memcpy(transform_buf,src_addr,8);
                transform_buf+=8;
              }
              else
              {
                memset(transform_buf,0,8);
                transform_buf+=8;
              }
            }

            //cr
            for (row=0;row<2;row++)
            {
              if ((i*8+k*2+row)<cml->lumHeightSrc/2)
              {
                src_addr = cr_tile_add+(k*2+row)*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
                memcpy(transform_buf,src_addr,8);
                transform_buf+=8;
              }
              else
              {
                memset(transform_buf,0,8);
                transform_buf+=8;
              }
            }
          }
          memset(transform_buf,0,16);
          transform_buf+=16;
          mb_total++;
          if (mb_total%5==0)
          {
           memset(transform_buf,0,48);
           transform_buf+=48;
  }

        }
      }
  }
   if (IS_HEVC(cml->codecFormat))//hevc
    {
      u32 size_lum;
      u32 size_ch;
      //cml->inputFormat = VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC;
      getAlignedPicSizebyFormat(VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC,cml->lumWidthSrc,cml->lumHeightSrc,0,&size_lum,&size_ch,NULL);

      pEncIn->busLuma = tb->transformMem->busAddress;
      //tb->lum = (u8 *)tb->transformMem.virtualAddress;

      pEncIn->busChromaU = pEncIn->busLuma + (u32)size_lum;
      //tb->cb = tb->lum + (u32)((float)size_lum*getPixelWidthInByte(cml->inputFormat));
      pEncIn->busChromaV = pEncIn->busChromaU + (u32)size_ch/2;
      //tb->cr = tb->cb + (u32)((float)size_ch*getPixelWidthInByte(cml->inputFormat));
    }
   else//h264
    {
      pEncIn->busLuma = tb->transformMem->busAddress;
      //tb->lum = (u8 *)tb->transformMem.virtualAddress;
    }
}


void transYUVtoFBformat(struct test_bench *tb, commandLine_s *cml)
{
   u8 *transform_buf;
   u32 x,y;
   VCEncIn *pEncIn = &(tb->encIn);
   transform_buf = (u8 *)tb->transformMem->virtualAddress;
   u32 alignment = (tb->input_alignment == 0 ? 1 : tb->input_alignment);
   u32 byte_per_compt = 0;

   if (cml->inputFormat==VCENC_YUV420_SEMIPLANAR || cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)
     byte_per_compt = 1;
   else if (cml->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010)
     byte_per_compt = 2;

   printf("transform YUV to FB format\n");

   if ((cml->inputFormat==VCENC_YUV420_SEMIPLANAR)||(cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)||
       (cml->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010) )
   {
     u32 stride = (cml->lumWidthSrc*4*byte_per_compt + alignment - 1) & (~(alignment - 1));

     //luma
     for (x = 0; x < cml->lumWidthSrc/4; x++)
     {
       for (y = 0; y < cml->lumHeightSrc; y++)
         memcpy(transform_buf + y%4*4*byte_per_compt + stride*(y/4) + x*16*byte_per_compt, tb->lum + y*((cml->lumWidthSrc+15)&(~15))*byte_per_compt + x*4*byte_per_compt, 4*byte_per_compt);
     }

     transform_buf += stride * cml->lumHeightSrc/4;

     //chroma
     for (x = 0; x < cml->lumWidthSrc/4; x++)
     {
       for (y = 0; y < ((cml->lumHeightSrc/2)+3)/4*4; y++)
         memcpy(transform_buf + y%4*4*byte_per_compt + stride*(y/4) + x*16*byte_per_compt, tb->cb + y*((cml->lumWidthSrc+15)&(~15))*byte_per_compt + x*4*byte_per_compt, 4*byte_per_compt);
     }
   }

    {
      u32 size_lum = ((cml->lumWidthSrc*4*byte_per_compt + alignment - 1) & (~(alignment - 1))) * cml->lumHeightSrc/4;
      //u32 size_ch = ((cml->lumWidthSrc*4*byte_per_compt + tb->alignment - 1) & (~(tb->alignment - 1))) * cml->lumHeightSrc/4/2;

      pEncIn->busLuma = tb->transformMem->busAddress;
      //tb->lum = (u8 *)tb->transformMem.virtualAddress;

      pEncIn->busChromaU = pEncIn->busLuma + (u32)size_lum;
      //tb->cb = tb->lum + (u32)((float)size_lum*getPixelWidthInByte(cml->inputFormat));
      //pEncIn->busChromaV = pEncIn->busChromaU + (u32)size_ch;
      //tb->cr = tb->cb + (u32)((float)size_ch*getPixelWidthInByte(cml->inputFormat));
    }

}

void transYUVtoCommDataformat(struct test_bench *tb, commandLine_s *cml)
{
   u8 *transform_buf;
   u32 x,y,ix,iy,i,j;
   u32 alignment = 0;
   u32 byte_per_compt = 0;
   u8 *tile_start_addr,*dst_8_addr = NULL,*cb_start,*cr_start;
   u16 *dst_16_addr = NULL;
   u32 luma_size = 0;

   VCEncIn *pEncIn = &(tb->encIn);
   transform_buf = (u8 *)tb->transformMem->virtualAddress;

   if (cml->formatCustomizedType == 12)
   {
     byte_per_compt = 1;
     dst_8_addr = transform_buf;
   }
   else
   {
     byte_per_compt = 2;
     dst_16_addr = (u16 *)transform_buf;
   }

   printf("transform YUV to CommData format\n");

   u32 orig_stride = cml->lumWidthSrc;

   //luma
   for (y = 0; y < ((cml->lumHeightSrc+7)/8); y++)
     for (x = 0; x < ((cml->lumWidthSrc+7)/8); x++)
     {
       tile_start_addr = tb->lum + 8*x + orig_stride*8*y;

       for (iy = 0; iy < 2; iy++)
         for (ix = 0; ix < 2; ix++)
            for (i = 0; i < 4; i++)
              if (cml->formatCustomizedType == 12)
                {
                  memcpy(dst_8_addr, tile_start_addr + orig_stride*i+4*ix+orig_stride*4*iy, 4);
                  dst_8_addr += 4;
                }
              else
                {
                  u8 *tmp_addr = tile_start_addr + orig_stride*i+4*ix+orig_stride*4*iy;
                  u64 tmp = 0;
                  for (j = 0; j < 4; j++)
                    tmp = tmp | (((u64)(*(tmp_addr+j)) << 8) << (16*j));
                  memcpy(dst_16_addr, &tmp, 4*byte_per_compt);
                  dst_16_addr += 4;
                }
     }

  if (cml->formatCustomizedType == 12)
    luma_size = dst_8_addr - transform_buf;
  else
    luma_size = (u8 *)dst_16_addr - (u8 *)transform_buf;

  //chroma
  for (y = 0; y < ((cml->lumHeightSrc/2+3)/4); y++)
  {
    for (x = 0; x < ((cml->lumWidthSrc+15)/16); x++)
      {
        cb_start = tb->cb + 8*x + orig_stride/2*4*y;
        cr_start = tb->cr + 8*x + orig_stride/2*4*y;

        for (i = 0; i < 4; i++)
        {
          for (j = 0; j < 16; j++)
          {
            if (j%2 == 0)
            {
              if (cml->formatCustomizedType == 12)
                *dst_8_addr++ = *(cb_start + (j%4)/2 + orig_stride/2*(j/4)+i*2);
              else
                *dst_16_addr++ = *(cb_start + (j%4)/2 + orig_stride/2*(j/4)+i*2)<<8;
            }
            else
            {
              if (cml->formatCustomizedType == 12)
                *dst_8_addr++ = *(cr_start + (j%4)/2 + orig_stride/2*(j/4)+i*2);
              else
                *dst_16_addr++ = *(cr_start + (j%4)/2 + orig_stride/2*(j/4)+i*2)<<8;
            }
          }
        }
      }
    }

    pEncIn->busLuma = tb->transformMem->busAddress;

    pEncIn->busChromaU = pEncIn->busLuma + (u32)luma_size;

}

FILE* transYUVtoP101010format(struct test_bench *tb, commandLine_s *cml)
{
   u32 *transform_buf;
   u32 x,y,i;
   u32 tmp = 0;
   u8 *data = NULL,*cb_data = NULL,*cr_data = NULL;
   VCEncIn *pEncIn = &(tb->encIn);
   transform_buf = (u32 *)tb->transformMem->virtualAddress;

   printf("transform YUV to P101010 format\n");

   if (cml->inputFormat==VCENC_YUV420_PLANAR)
   {
     //luma
     for (y = 0; y < cml->lumHeightSrc; y++)
     {
       data = tb->lum + y*cml->lumWidthSrc;
       for (x = 0; x < (cml->lumWidthSrc+2)/3; x++)
        {
          tmp = 0;
          for (i = 0; i < 3; i++)
            {
              if ((x*3 + i) < cml->lumWidthSrc)
                tmp = (((u32)*(data+i)<<2)<<(10*i))|tmp;
            }
          memcpy(transform_buf++ , &tmp , 4);
          data += 3;
        }
     }

     //chroma
     for (y = 0; y < cml->lumHeightSrc/2; y++)
     {
       cb_data = tb->cb + y*cml->lumWidthSrc/2;
       cr_data = tb->cr + y*cml->lumWidthSrc/2;
       i = 0;
       tmp = 0;
       while (1)
       {
         if (i%2==0)
           tmp = (((u32)*(cb_data+i/2)<<2)<<(10*(i%3)))|tmp;
         else
           tmp = (((u32)*(cr_data+i/2)<<2)<<(10*(i%3)))|tmp;
         i++;

         if (i%3 == 0 || i == cml->lumWidthSrc)
         {
           memcpy(transform_buf++ , &tmp , 4);
           tmp = 0;
         }

         if (i == cml->lumWidthSrc)
           break;
       }
     }
   }

   if (cml->exp_of_input_alignment != 0)
   {
     //static FILE *yuv_out = NULL;
     if (yuv_out == NULL)
       yuv_out = fopen("daj_transformed_format.yuv","wb");
     u32 transform_size = 0;

     transform_size = (cml->lumWidthSrc + 2)/3*4 * (cml->lumHeightSrc + cml->lumHeightSrc/2);

     for (i=0;i<transform_size;i++)
        fwrite((u8 *)(tb->transformMem->virtualAddress)+i, sizeof(u8), 1, yuv_out);
   }
   else
   {
     u32 size_lum = (cml->lumWidthSrc + 2)/3*4 * cml->lumHeightSrc;

     pEncIn->busLuma = tb->transformMem->busAddress;

     pEncIn->busChromaU = pEncIn->busLuma + (u32)size_lum;

   }

    return yuv_out;
}

FILE* transYUVto256BTileformat(struct test_bench *tb, commandLine_s *cml)
{
   u32 x,y,i,k,j;
   u8 *luma_data = NULL,*cb_data = NULL,*cr_data = NULL,*data_out = NULL,*data = NULL;
   u8 *cb_data_tmp = NULL,*cr_data_tmp = NULL;
   u16 *data_out_16 = NULL;
   u32 *data_out_32 = NULL;
   u32 tile_width =0,tile_height=0;
   u32 tmp = 0;
   u32 size_lum =0;
   VCEncIn *pEncIn = &(tb->encIn);
   data_out = (u8 *)tb->transformMem->virtualAddress;
   data_out_16 = (u16 *)tb->transformMem->virtualAddress;
   data_out_32 = (u32 *)tb->transformMem->virtualAddress;
   u32 w = cml->lumWidthSrc;

   printf("transform YUV to 8/10bit 256-byte Tile format\n");
   if (cml->formatCustomizedType == 3 || cml->formatCustomizedType == 4)
   {
    tile_width = 64;
    tile_height= 4;
   }
   else if (cml->formatCustomizedType == 5)
   {
    tile_width = 32;
    tile_height= 4;
   }
   else if (cml->formatCustomizedType == 6 || cml->formatCustomizedType == 7)
   {
    tile_width = 48;
    tile_height= 4;
   }
   else if (cml->formatCustomizedType == 8 || cml->formatCustomizedType == 9)
   {
    tile_width = 128;
    tile_height= 2;
   }
   else if (cml->formatCustomizedType == 10 || cml->formatCustomizedType == 11)
   {
    tile_width = 96;
    tile_height= 2;
   }

   if (cml->inputFormat==VCENC_YUV420_PLANAR && ((cml->formatCustomizedType >= 3 && cml->formatCustomizedType <= 5)||
       (cml->formatCustomizedType >= 8 && cml->formatCustomizedType <= 9)))
   {
     //luma
     luma_data = tb->lum;
     for (y = 0; y < (cml->lumHeightSrc+tile_height-1)/tile_height; y++)
     {
       for (x = 0; x < (w + tile_width-1)/tile_width; x++)
         for (i = 0; i < tile_height; i++)
          {
           if (cml->formatCustomizedType == 5)
           {
             for (k = 0; k < tile_width; k++)
               *data_out_16++ = ((u16)*(luma_data+i*w+tile_width*x+y*tile_height*w+k))<<8;
           }
           else
           {
             memcpy(data_out, luma_data+i*w+tile_width*x+y*tile_height*w, tile_width);
             data_out += tile_width;
           }
          }
     }

     //chroma
     if (cml->formatCustomizedType == 3 || cml->formatCustomizedType == 8)
     {
       cb_data = tb->cb;
       cr_data = tb->cr;
     }
     else if (cml->formatCustomizedType == 4 || cml->formatCustomizedType == 5 || cml->formatCustomizedType == 9)
     {
       cb_data = tb->cr;
       cr_data = tb->cb;
     }

     for (y = 0; y < (cml->lumHeightSrc/2+tile_height-1)/tile_height; y++)
     {
       for (x = 0; x < (w + tile_width-1)/tile_width; x++)
         for (i = 0; i < tile_height; i++)
            for (k = 0; k < tile_width/2; k++)
              {
               if (cml->formatCustomizedType == 5)
               {
                 *data_out_16++ = ((u16)*(cr_data+k+i*w/2+y*tile_height*w/2+x*(tile_width/2)))<<8;
                 *data_out_16++ = ((u16)*(cb_data+k+i*w/2+y*tile_height*w/2+x*(tile_width/2)))<<8;
               }
               else
               {
                 *data_out++ = *(cr_data+k+i*w/2+y*tile_height*w/2+x*(tile_width/2));
                 *data_out++ = *(cb_data+k+i*w/2+y*tile_height*w/2+x*(tile_width/2));
               }
              }
     }
   }
   else
   {
     //luma
     luma_data = tb->lum;
     for (y = 0; y < (cml->lumHeightSrc+tile_height-1)/tile_height; y++)
     {
       for (x = 0; x < (w + tile_width-1)/tile_width; x++)
         for (i = 0; i < tile_height; i++)
          {
            for (k = 0; k < tile_width/3; k++)
            {
              tmp = 0;
              data = luma_data+i*w+tile_width*x+y*tile_height*w+k*3;
              for (j = 0; j < 3; j++)
              {
                tmp = (((u32)*(data+j)<<2)<<(10*j))|tmp;
              }
              *data_out_32++ = tmp;
            }
          }
      }

     //chroma
     if (cml->formatCustomizedType == 6 || cml->formatCustomizedType == 10)
     {
       cb_data_tmp = tb->cb;
       cr_data_tmp = tb->cr;
     }
     else if (cml->formatCustomizedType == 7 || cml->formatCustomizedType == 11)
     {
       cb_data_tmp = tb->cr;
       cr_data_tmp = tb->cb;
     }

     for (y = 0; y < (cml->lumHeightSrc/2+tile_height-1)/tile_height; y++)
     {
       for (x = 0; x < (w + tile_width-1)/tile_width; x++)
         for (i = 0; i < tile_height; i++)
         {
           j = 0;
           tmp = 0;
           cb_data = cb_data_tmp+i*w/2+y*tile_height*w/2+x*(tile_width/2);
           cr_data = cr_data_tmp+i*w/2+y*tile_height*w/2+x*(tile_width/2);
           for (k = 0; k < tile_width; k++)
           {
             if (j%2==0)
               tmp = (((u32)*(cb_data+j/2)<<2)<<(10*(j%3)))|tmp;
             else
               tmp = (((u32)*(cr_data+j/2)<<2)<<(10*(j%3)))|tmp;
             j++;

             if (j%3 == 0)
             {
               *data_out_32++ = tmp;
               tmp = 0;
             }
           }
         }
     }
   }

   if (cml->exp_of_input_alignment != 0)
   {
     if (yuv_out == NULL)
      yuv_out = fopen("daj_transformed_format.yuv","wb");
     u32 transform_size = tb->transformedSize;

     for (i=0;i<transform_size;i++)
       fwrite((u8 *)(tb->transformMem->virtualAddress)+i, sizeof(u8), 1, yuv_out);
   }
   else
   {
     if ((cml->formatCustomizedType >= 3 && cml->formatCustomizedType <= 5) || (cml->formatCustomizedType >= 8 && cml->formatCustomizedType <= 9))
       size_lum = STRIDE(cml->lumWidthSrc,tile_width) * STRIDE(cml->lumHeightSrc,tile_height) * (cml->formatCustomizedType == 5?2:1);
     else
       size_lum = (cml->lumWidthSrc + tile_width -1)/tile_width*tile_width/3* 4 * STRIDE(cml->lumHeightSrc,tile_height);

     pEncIn->busLuma = tb->transformMem->busAddress;

     pEncIn->busChromaU = pEncIn->busLuma + (u32)size_lum;

   }

   return yuv_out;
}

void ChangeInputToTransYUV(struct test_bench *tb, VCEncInst encoder, commandLine_s *cml,VCEncIn *pEncIn)
{
  u32 frameCntTotal = 0,src_img_size = 0;
  FILE *transfile = NULL;
  VCEncRet ret;
  const void *ewl_inst = VCEncGetEwl(encoder);

  while((cml->exp_of_input_alignment > 0)&& (((cml->formatCustomizedType >= 2)&&(cml->formatCustomizedType <= 11))&&(cml->inputFormat==VCENC_YUV420_PLANAR)))
  {
    GetFreeIOBuffer(tb);
    src_img_size = SetupInputBuffer(tb,cml, pEncIn);
    if (read_picture(tb, cml->inputFormat, src_img_size, cml->lumWidthSrc, cml->lumHeightSrc, 0) == 0)
    {
      transfile = FormatCustomizedYUV(tb, cml);
      frameCntTotal++;
      tb->encIn.picture_cnt++;
    }
    else
      break;
  }

  if (transfile != NULL)
  {
    fclose(transfile);
    fclose(tb->in);
    tb->in = fopen("daj_transformed_format.yuv","rb");
    printf("change input file into daj_transformed_format.yuv\n");
    frameCntTotal = 0;
    tb->encIn.picture_cnt = 0;
    if ((cml->formatCustomizedType == 2)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_SEMIPLANAR_101010;
    }

    if ((cml->formatCustomizedType == 3)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_8BIT_TILE_64_4;
    }

    if ((cml->formatCustomizedType == 4)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_UV_8BIT_TILE_64_4;
    }

    if ((cml->formatCustomizedType == 5)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_10BIT_TILE_32_4;
    }

    if ((cml->formatCustomizedType == 6)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_10BIT_TILE_48_4;
    }

    if ((cml->formatCustomizedType == 7)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_VU_10BIT_TILE_48_4;
    }

    if ((cml->formatCustomizedType == 8)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_8BIT_TILE_128_2;
    }

    if ((cml->formatCustomizedType == 9)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_UV_8BIT_TILE_128_2;
    }

    if ((cml->formatCustomizedType == 10)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_10BIT_TILE_96_2;
    }

    if ((cml->formatCustomizedType == 11)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      cml->inputFormat = VCENC_YUV420_VU_10BIT_TILE_96_2;
    }


    for(u32 coreIdx =0; coreIdx < tb->parallelCoreNum; coreIdx++)
      if (tb->pictureMemFactory[coreIdx].virtualAddress != NULL)
        EWLFreeLinear(ewl_inst, &tb->pictureMemFactory[coreIdx]);

    u32 lumaSize = 0,chromaSize = 0,pictureSize = 0;
    getAlignedPicSizebyFormat(cml->inputFormat,cml->lumWidthSrc,cml->lumHeightSrc,tb->input_alignment,&lumaSize,&chromaSize,&pictureSize);

    tb->lumaSize = lumaSize;
    tb->chromaSize = chromaSize;

    cml->formatCustomizedType = -1;
    tb->formatCustomizedType = -1;

    for(u32 coreIdx =0; coreIdx < tb->parallelCoreNum; coreIdx++)
    {
      ret = EWLMallocLinear(ewl_inst, pictureSize,tb->input_alignment, &tb->pictureMemFactory[coreIdx]);
      if (ret != EWL_OK)
      {
        tb->pictureMemFactory[coreIdx].virtualAddress = NULL;
      }
    }
  }

}

i32 readConfigFiles(struct test_bench *tb, commandLine_s *cml)
{
  i32 i;

  if ((cml->roiMapDeltaQpEnable)&&(cml->roiMapDeltaQpFile != NULL))
  {
    tb->roiMapFile = fopen (cml->roiMapDeltaQpFile, "r");
    if (tb->roiMapFile == NULL)
    {
      printf("ROI map Qp delta config: Error, Can Not Open File %s\n", cml->roiMapDeltaQpFile );
      //return -1;
    }
  }

  if (cml->roiMapDeltaQpEnable && cml->roiMapDeltaQpBinFile)
  {
    tb->roiMapBinFile = fopen (cml->roiMapDeltaQpBinFile, "rb");
    if (tb->roiMapBinFile == NULL)
    {
      printf("ROI map Qp delta config: Error, Can Not Open File %s\n", cml->roiMapDeltaQpBinFile );
    }
  }

  if ((cml->ipcmMapEnable)&&(cml->ipcmMapFile != NULL))
  {
    tb->ipcmMapFile = fopen (cml->ipcmMapFile, "r");
    if (tb->ipcmMapFile == NULL)
    {
      printf("IPCM map config: Error, Can Not Open File %s\n", cml->ipcmMapFile );
      //return -1;
    }
  }

  for (i = 0; i < 2; i ++)
  {
    if (cml->gmvFileName[i])
    {
      tb->gmvFile[i] = fopen (cml->gmvFileName[i], "r");
      if (!tb->gmvFile[i])
        printf("GMV config: Error, Can Not Open File %s\n", cml->gmvFileName[i]);
    }
  }

  if (cml->skipMapEnable && cml->skipMapFile && (tb->skipMapFile == NULL))
  {
    tb->skipMapFile = fopen (cml->skipMapFile, "r");
    if (tb->skipMapFile == NULL)
    {
      printf("SKIP map config: Error, Can Not Open File %s\n", cml->skipMapFile );
    }
  }

  if (cml->roiMapInfoBinFile && (tb->roiMapInfoBinFile == NULL))
  {
    tb->roiMapInfoBinFile = fopen (cml->roiMapInfoBinFile, "rb");
    if (tb->roiMapInfoBinFile == NULL)
    {
      printf("ROI map config: Error, Can Not Open File %s\n", cml->roiMapInfoBinFile );
    }
  }

  if (cml->RoimapCuCtrlInfoBinFile && (tb->RoimapCuCtrlInfoBinFile == NULL))
  {
    tb->RoimapCuCtrlInfoBinFile = fopen (cml->RoimapCuCtrlInfoBinFile, "rb");
    if (tb->RoimapCuCtrlInfoBinFile == NULL)
    {
      printf("ROI map cu ctrl config: Error, Can Not Open File %s\n", cml->RoimapCuCtrlInfoBinFile );
    }
  }

  if (cml->RoimapCuCtrlIndexBinFile && (tb->RoimapCuCtrlIndexBinFile == NULL))
  {
    tb->RoimapCuCtrlIndexBinFile = fopen (cml->RoimapCuCtrlIndexBinFile, "rb");
    if (tb->RoimapCuCtrlIndexBinFile == NULL)
    {
      printf("ROI map cu ctrl index config: Error, Can Not Open File %s\n", cml->RoimapCuCtrlIndexBinFile );
    }
  }

  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    if((cml->overlayEnables >> i) & 1){
			u32 frameByte = 1;
      tb->overlayFile[i] = fopen(cml->olInput[i], "rb");
			//Get number of input overlay frames
			fseeko(tb->overlayFile[i], 0, SEEK_END);
			if(cml->olFormat[i] == 0)
			{
				frameByte = (cml->olWidth[i] * cml->olHeight[i])*4;
			}
			else if(cml->olFormat[i] == 1)
			{
				frameByte = (cml->olWidth[i] * cml->olHeight[i]) / 2 * 3;
			}
			else if(cml->olFormat[i] == 2)
			{
				frameByte = (cml->olWidth[i] / 8 * cml->olHeight[i]);
			}
		  tb->olFrameNume[i] = ftello(tb->overlayFile[i]) / frameByte;
		  if(tb->olFrameNume[i] == 0)
		  {
		    printf("Overlay file %d: Error, file %s size is not suitable for region size\n", i, cml->olInput[i]);
		    return -1;
		  }
    }
		else
		{
			tb->olFrameNume[i] = 0;
		}
  }

  return 0;
}

/*------------------------------------------------------------------------------
    WriteCuInformation
        Write cu encoding information into a file.
        Information format is defined by VCEncCuInfo struct.
------------------------------------------------------------------------------*/
void WriteCuInformation (struct test_bench *tb, VCEncInst encoder, VCEncOut *pEncOut, commandLine_s *cml, i32 frame, i32 poc)
{
    bool bWrite = cml->enableOutputCuInfo == 2;
    const EWLHwConfig_t asicCfg = VCEncGetAsicConfig(cml->codecFormat);
#ifdef TEST_DATA
    if (cml->enableOutputCuInfo == 1 && EncTraceCuInfoCheck())
      bWrite = HANTRO_TRUE;
#endif
    if (!bWrite)
      return;

    if (!tb->fmv)
    {
      if(!(tb->fmv = open_file("cuInfo.txt", "wb")))
        return;
    }

    FILE *file = tb->fmv;
    i32 width = cml->width;
    i32 height = cml->interlacedFrame ? cml->height/2 : cml->height;
    i32 ctuPerRow = (width+cml->max_cu_size-1)/cml->max_cu_size;
    i32 ctuPerCol = (height+cml->max_cu_size-1)/cml->max_cu_size;
    i32 ctuPerFrame = ctuPerRow * ctuPerCol;
    VCEncCuInfo cuInfo;
    u32 *ctuTable = pEncOut->cuOutData.ctuOffset;
    char *picType[] = {"I", "P", "B", "Not-Known"};
    char *cuType[] = {"INTER", "INTRA", "IPCM"};
    char *intraPart[] = {"2Nx2N", "NxN"};
    char *intraPartH264[] = {"16x16", "8x8", "4x4"};
    char *interDir[] = {"PRED_L0","PRED_L1","PRED_BI"};
    i32 iCtuX, iCtuY, iCu, iPu = 0;
    i32 iCtu = 0;

    if (!file || !pEncOut || !cml)
        return;

    if (frame == 0)
      fprintf(file, "Encoding Information of %s. MV in 1/4 pixel.\n", cml->input);

    fprintf(file, "\n#Pic %d %s-Frame have %d CTU(%dx%d). Poc %d.\n",
        frame, picType[pEncOut->codingType], ctuPerFrame, ctuPerRow, ctuPerCol, poc);

    for (iCtuY = 0; iCtuY < ctuPerCol; iCtuY ++)
      for (iCtuX = 0; iCtuX < ctuPerRow; iCtuX ++)
      {
        i32 nCu = ctuTable[iCtu];
        if (iCtu)
          nCu -= ctuTable[iCtu - 1];

        /*
            HEVC: HW generate ctuIdx table
                       sw reads num of CUs from the table
            H264:   HW not write anything to ctuIdx table(C-Model write 1 CU per CTU),
                        HW is not same as C-Model, ctr sw need fill nCu itself, not dependent on
                        ctuIdx
        */
        if(IS_H264(cml->codecFormat))
            nCu =1;

        fprintf(file, "\n*CTU %d at (%2d,%2d) have %2d CU:\n", iCtu, iCtuX*cml->max_cu_size, iCtuY*cml->max_cu_size, nCu);

        for (iCu = 0; iCu < nCu; iCu ++)
        {
          VCEncGetCuInfo(encoder, &pEncOut->cuOutData, &cuInfo, iCtu, iCu);
          fprintf(file, "  CU %2dx%-2d at (%2d,%2d)  %s %-7s",
            cuInfo.cuSize, cuInfo.cuSize, cuInfo.cuLocationX, cuInfo.cuLocationY, cuType[cuInfo.cuMode],
            cuInfo.cuMode ? (IS_H264(cml->codecFormat) ? intraPartH264[cuInfo.intraPartMode] : intraPart[cuInfo.intraPartMode]) : interDir[cuInfo.interPredIdc]);
          fprintf (file, "  Cost=%-8d",cuInfo.cost);

          // 0:inter; 1:intra; 2:IPCM
          if (cuInfo.cuMode == 1)
          {
            if(IS_H264(cml->codecFormat)) {
              fprintf(file, "   Intra_Mode:");
              for(iPu = 0; iPu < (1<<(2*cuInfo.intraPartMode)); iPu++)
                fprintf(file, " %2d", cuInfo.intraPredMode[iPu]);
            } else
            if (cuInfo.intraPartMode)
              fprintf(file, "   Intra_Mode: %2d %2d %2d %2d",
                cuInfo.intraPredMode[0],cuInfo.intraPredMode[1],cuInfo.intraPredMode[2],cuInfo.intraPredMode[3]);
            else
              fprintf(file, "   Intra_Mode: %2d         ",cuInfo.intraPredMode[0]);
          }
          else if (cuInfo.cuMode == 0)
          {
            char mvstring[20];
            if (cuInfo.interPredIdc != 1)
            {
              sprintf(mvstring, "(%d,%d)", cuInfo.mv[0].mvX,cuInfo.mv[0].mvY);
              fprintf(file, " List0_Motion: refIdx=%-2d MV=%-14s", cuInfo.mv[0].refIdx, mvstring);
            }

            if (cuInfo.interPredIdc != 0)
            {
              sprintf(mvstring, "(%d,%d)", cuInfo.mv[1].mvX,cuInfo.mv[1].mvY);
              fprintf(file, " List1_Motion: refIdx=%-2d MV=%-14s", cuInfo.mv[1].refIdx, mvstring);
            }
          }

          if(asicCfg.cuInforVersion >= 1)
          {
            u32 interCost = cuInfo.cuMode ? cuInfo.costOfOtherMode : cuInfo.cost;
            u32 intraCost = cuInfo.cuMode ? cuInfo.cost : cuInfo.costOfOtherMode;
            fprintf(file, " Mean %-4d Variance %-8d Qp %-2d INTRA Cost %-8d INTER Cost %-8d INTRA Satd %-8d INTER Satd %-8d",
                cuInfo.mean, cuInfo.variance, cuInfo.qp, intraCost, interCost, cuInfo.costIntraSatd, cuInfo.costInterSatd);
          }
          fprintf(file, "\n");
        }

        iCtu ++;
      }
}

char *nextIntToken_t (char *str, i16 *ret)
{
  char *p = str;
  i32 tmp;

  if (!p) return NULL;

  while ((*p < '0') || (*p > '9'))
  {
    if (*p == '\0') return NULL;
    p ++;
  }
  sscanf (p, "%d", &tmp);
  if ((p != str) && (*(p-1) == '-'))
    tmp = -tmp;

  while ((*p >= '0') && (*p <= '9'))
    p ++;

  *ret = tmp;
  return p;
}

static i32 parseGmvFile (FILE *fIn, i16 *hor, i16 *ver, i32 list)
{
  char achParserBuffer[4096];
  char *buf = &(achParserBuffer[0]);

  if (!fIn) return -1;
  if (feof (fIn)) return -1;

  // Read one line
  buf[0] = '\0';
  char *line = fgets (buf, 4096, fIn);
  if (!line) return -1;
  //handle line end
  char* s = strpbrk(line, "#\n");
  if(s) *s = '\0';

  line = nextIntToken_t(line, hor);
  if (!line)
    return -1;

  line = nextIntToken_t(line, ver);
  if (!line)
    return -1;

#ifdef SEARCH_RANGE_ROW_OFFSET_TEST
  extern char gmvConfigLine[2][4096];
  memcpy(&(gmvConfigLine[list][0]), &(achParserBuffer[0]), 4096);
#endif

  return 0;
}


i32 readGmv(struct test_bench *tb, VCEncIn *pEncIn, commandLine_s *cml)
{
  i16 mvx, mvy, i;

  for (i = 0; i < 2; i ++)
  {
    if (tb->gmvFile[i])
    {
      if (parseGmvFile (tb->gmvFile[i], &mvx, &mvy, i) == 0)
      {
        pEncIn->gmv[i][0] = mvx;
        pEncIn->gmv[i][1] = mvy;
      }
    }
    else
    {
      pEncIn->gmv[i][0] = cml->gmv[i][0];
      pEncIn->gmv[i][1] = cml->gmv[i][1];
    }
  }

  return 0;
}

static i32 getSmartOpt (char *line, char *opt, i32 *val)
{
  char *p = strstr(line, opt);
  if (!p)
    return NOK;

  p = strchr(line, '=');
  if (!p)
    return NOK;

  p ++;
  while (*p == ' ') p ++;
  if (*p == '\0')
    return NOK;

  sscanf(p, "%d", val);
  return OK;
}

i32 ParsingSmartConfig (char *fname, commandLine_s *cml)
{
#define MAX_LINE_LENGTH 1024
  char achParserBuffer[MAX_LINE_LENGTH];
  FILE *fIn = fopen (fname, "r");
  if (fIn == NULL)
  {
    printf("Smart Config: Error, Can Not Open File %s\n", fname );
    return -1;
  }

  while (1)
  {
    if (feof (fIn)) break;

    achParserBuffer[0] = '\0';
    // Read one line
    char *line = fgets ((char *) achParserBuffer, MAX_LINE_LENGTH, fIn);
    if (!line) break;
    //handle line end
    char* s = strpbrk(line, "#\n");
    if(s) *s = '\0';

    if (getSmartOpt(line, "smart_Mode_Enable",                &cml->smartModeEnable) == OK) continue;

    if (getSmartOpt(line, "smart_H264_Qp",                    &cml->smartH264Qp) == OK) continue;
    if (getSmartOpt(line, "smart_H264_Luma_DC_Th",            &cml->smartH264LumDcTh) == OK) continue;
    if (getSmartOpt(line, "smart_H264_Cb_DC_Th",              &cml->smartH264CbDcTh) == OK) continue;
    if (getSmartOpt(line, "smart_H264_Cr_DC_Th",              &cml->smartH264CrDcTh) == OK) continue;

    if (getSmartOpt(line, "smart_Hevc_Luma_Qp",               &cml->smartHevcLumQp) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_Chroma_Qp",             &cml->smartHevcChrQp) == OK) continue;

    if (getSmartOpt(line, "smart_Hevc_CU8_Luma_DC_Th",        &(cml->smartHevcLumDcTh[0])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU16_Luma_DC_Th",       &(cml->smartHevcLumDcTh[1])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU32_Luma_DC_Th",       &(cml->smartHevcLumDcTh[2])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU8_Chroma_DC_Th",      &(cml->smartHevcChrDcTh[0])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU16_Chroma_DC_Th",     &(cml->smartHevcChrDcTh[1])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU32_Chroma_DC_Th",     &(cml->smartHevcChrDcTh[2])) == OK) continue;

    if (getSmartOpt(line, "smart_Hevc_CU8_Luma_AC_Num_Th",    &(cml->smartHevcLumAcNumTh[0])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU16_Luma_AC_Num_Th",   &(cml->smartHevcLumAcNumTh[1])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU32_Luma_AC_Num_Th",   &(cml->smartHevcLumAcNumTh[2])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU8_Chroma_AC_Num_Th",  &(cml->smartHevcChrAcNumTh[0])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU16_Chroma_AC_Num_Th", &(cml->smartHevcChrAcNumTh[1])) == OK) continue;
    if (getSmartOpt(line, "smart_Hevc_CU32_Chroma_AC_Num_Th", &(cml->smartHevcChrAcNumTh[2])) == OK) continue;

    if (getSmartOpt(line, "smart_Mean_Th0",                   &(cml->smartMeanTh[0])) == OK) continue;
    if (getSmartOpt(line, "smart_Mean_Th1",                   &(cml->smartMeanTh[1])) == OK) continue;
    if (getSmartOpt(line, "smart_Mean_Th2",                   &(cml->smartMeanTh[2])) == OK) continue;
    if (getSmartOpt(line, "smart_Mean_Th3",                   &(cml->smartMeanTh[3])) == OK) continue;
    if (getSmartOpt(line, "smart_Pixel_Num_Cnt_Th",           &cml->smartPixNumCntTh) == OK) continue;
  }

  fclose(fIn);

  printf ("======== Smart Algorithm Configuration ========\n");
  printf ("smart_Mode_Enable = %d\n",                cml->smartModeEnable);

  printf ("smart_H264_Qp = %d\n",                    cml->smartH264Qp);
  printf ("smart_H264_Luma_DC_Th = %d\n",            cml->smartH264LumDcTh);
  printf ("smart_H264_Cb_DC_Th = %d\n",              cml->smartH264CbDcTh);
  printf ("smart_H264_Cr_DC_Th = %d\n",              cml->smartH264CrDcTh);

  printf ("smart_Hevc_Luma_Qp = %d\n",               cml->smartHevcLumQp);
  printf ("smart_Hevc_Chroma_Qp = %d\n",             cml->smartHevcChrQp);

  printf ("smart_Hevc_CU8_Luma_DC_Th = %d\n",        (cml->smartHevcLumDcTh[0]));
  printf ("smart_Hevc_CU16_Luma_DC_Th = %d\n",       (cml->smartHevcLumDcTh[1]));
  printf ("smart_Hevc_CU32_Luma_DC_Th = %d\n",       (cml->smartHevcLumDcTh[2]));
  printf ("smart_Hevc_CU8_Chroma_DC_Th = %d\n",      (cml->smartHevcChrDcTh[0]));
  printf ("smart_Hevc_CU16_Chroma_DC_Th = %d\n",     (cml->smartHevcChrDcTh[1]));
  printf ("smart_Hevc_CU32_Chroma_DC_Th = %d\n",     (cml->smartHevcChrDcTh[2]));

  printf ("smart_Hevc_CU8_Luma_AC_Num_Th = %d\n",    (cml->smartHevcLumAcNumTh[0]));
  printf ("smart_Hevc_CU16_Luma_AC_Num_Th = %d\n",   (cml->smartHevcLumAcNumTh[1]));
  printf ("smart_Hevc_CU32_Luma_AC_Num_Th = %d\n",   (cml->smartHevcLumAcNumTh[2]));
  printf ("smart_Hevc_CU8_Chroma_AC_Num_Th = %d\n",  (cml->smartHevcChrAcNumTh[0]));
  printf ("smart_Hevc_CU16_Chroma_AC_Num_Th = %d\n", (cml->smartHevcChrAcNumTh[1]));
  printf ("smart_Hevc_CU32_Chroma_AC_Num_Th = %d\n", (cml->smartHevcChrAcNumTh[2]));

  printf ("smart_Mean_Th0 = %d\n",                   (cml->smartMeanTh[0]));
  printf ("smart_Mean_Th1 = %d\n",                   (cml->smartMeanTh[1]));
  printf ("smart_Mean_Th2 = %d\n",                   (cml->smartMeanTh[2]));
  printf ("smart_Mean_Th3 = %d\n",                   (cml->smartMeanTh[3]));
  printf ("smart_Pixel_Num_Cnt_Th = %d\n",           cml->smartPixNumCntTh);
  printf ("===============================================\n");

  return 0;
}




/*------------------------------------------------------------------------------
  file_read
------------------------------------------------------------------------------*/
i32 file_read(FILE *file, u8 *data, u64 seek, size_t size)
{
  if ((file == NULL) || (data == NULL)) return NOK;

  fseeko(file, seek, SEEK_SET);
  if (fread(data, sizeof(u8), size, file) < size)
  {
    if (!feof(file))
    {
      Error(2, ERR, SYSERR);
    }
    return NOK;
  }

  return OK;
}

/*------------------------------------------------------------------------------
  next_picture calculates next input picture depending input and output
  frame rates.
------------------------------------------------------------------------------*/
u64 next_picture(struct test_bench *tb, int picture_cnt)
{
  u64 numer, denom;

  numer = (u64)tb->inputRateNumer * (u64)tb->outputRateDenom;
  denom = (u64)tb->inputRateDenom * (u64)tb->outputRateNumer;

  return numer * (picture_cnt / (1 << tb->interlacedFrame)) / denom;
}

i32 read_dec400Data(struct test_bench *tb, u32 inputFormat, u32 src_width, u32 src_height)
{
  i32 num;
  u64 seek;
  u8 *lum;
  u8 *cb;
  u8 *cr;
  u32 lumaSize;
  u32 chrSize;
  u32 src_img_size;

  num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;

  /*dec400 compress table*/
  lumaSize = tb->dec400LumaTableSize;
  chrSize = tb->dec400ChrTableSize;
  src_img_size = lumaSize + chrSize;

  if (num > tb->lastPic || tb->dec400Table == NULL) return NOK;
  seek     = ((u64)num) * ((u64)src_img_size);

  lum = (u8 *)tb->Dec400CmpTableMem.virtualAddress;
  cb = lum + lumaSize;
  cr = cb + chrSize/2;

  switch(inputFormat)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_PLANAR_10BIT_I010:
     if (file_read(tb->dec400Table, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(tb->dec400Table, cb , seek, chrSize/2)) return NOK;
     seek += chrSize/2;
     if (file_read(tb->dec400Table, cr , seek, chrSize/2)) return NOK;
     break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV420_PLANAR_10BIT_P010:
     if (file_read(tb->dec400Table, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(tb->dec400Table, cb , seek, chrSize)) return NOK;
     break;
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
     if (file_read(tb->dec400Table, lum , seek, lumaSize)) return NOK;
     break;
    default:
     printf("DEC400 not support this format\n");
     return NOK;
  }

  /*dec400 compress data*/
  lumaSize = tb->lumaSize;
  chrSize = tb->chromaSize;
  src_img_size = lumaSize + chrSize;

  seek     = ((u64)num) * ((u64)src_img_size);

  lum = tb->lum;
  cb = tb->cb;
  cr = tb->cr;

  switch(inputFormat)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_PLANAR_10BIT_I010:
     if (file_read(tb->in, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(tb->in, cb , seek, chrSize/2)) return NOK;
     seek += chrSize/2;
     if (file_read(tb->in, cr , seek, chrSize/2)) return NOK;
     break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV420_PLANAR_10BIT_P010:
     if (file_read(tb->in, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(tb->in, cb , seek, chrSize)) return NOK;
     break;
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
     if (file_read(tb->in, lum , seek, lumaSize)) return NOK;
     break;
    default:
     printf("DEC400 not support this format\n");
     return NOK;
  }

  return OK;
}

/*------------------------------------------------------------------------------
  read_picture
------------------------------------------------------------------------------*/
i32 read_picture(struct test_bench *tb, u32 inputFormat, u32 src_img_size, u32 src_width, u32 src_height, int bDS)
{
  i32 num;
  u64 seek;
  i32 w;
  i32 h;
  u8 *lum;
  u8 *cb;
  u8 *cr;
  i32 i;
  u32 luma_stride;
  u32 chroma_stride;
  u32 alignment;
  FILE *in = bDS ? tb->inDS : tb->in;

  num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;

  if (num > tb->lastPic) return NOK;
  seek     = ((u64)num) * ((u64)src_img_size);
  lum = tb->lum;
  cb = tb->cb;
  cr = tb->cr;
  if(bDS) {
    lum = tb->lumDS;
    cb = tb->cbDS;
    cr = tb->crDS;
  }
  alignment = (tb->formatCustomizedType != -1? 0 : tb->input_alignment);
  VCEncGetAlignedStride(src_width,inputFormat,&luma_stride,&chroma_stride,alignment);

  if (tb->formatCustomizedType == 0 && inputFormat==VCENC_YUV420_PLANAR)
    memset(lum,0,luma_stride*src_height*3/2);

  switch(inputFormat)
  {
    case VCENC_YUV420_PLANAR:
    {
      /* Lum */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width)) return NOK;
        seek += src_width;
        lum += luma_stride;
      }

      w = ((src_width + 1) >> 1);
      h = ((src_height + 1) >> 1);
      /* Cb */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cb, seek, w)) return NOK;
        seek += w;
        cb += chroma_stride;
      }

      /* Cr */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cr, seek, w)) return NOK;

        seek += w;
        cr += chroma_stride;
      }
      break;
    }
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    {
      /* Lum */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width)) return NOK;
        seek += src_width;
        lum += luma_stride;
      }
      /* CbCr */
      for (i = 0; i < (src_height / 2); i++)
      {
        if (file_read(in, cb, seek, src_width)) return NOK;
        seek += src_width;
        cb += chroma_stride;
      }
      break;
    }
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    {
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width * 2)) return NOK;
        seek += src_width * 2;
        lum += luma_stride;
      }
      break;
    }
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
    {
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width * 4)) return NOK;
        seek += src_width * 4;
        lum += luma_stride;
      }
      break;
    }
    case VCENC_YUV420_PLANAR_10BIT_I010:
    {
      /* Lum */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width*2)) return NOK;
        seek += src_width*2;
        lum += luma_stride;
      }

      w = ((src_width + 1) >> 1);
      h = ((src_height + 1) >> 1);
      /* Cb */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cb, seek, w*2)) return NOK;
        seek += w*2;
        cb += chroma_stride;
      }

      /* Cr */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cr, seek, w*2)) return NOK;
        seek += w*2;
        cr += chroma_stride;
      }
      break;
    }
    case VCENC_YUV420_PLANAR_10BIT_P010:
    {
      /* Lum */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width*2)) return NOK;
        seek += src_width*2;
        lum += luma_stride;
      }

      h = ((src_height + 1) >> 1);
      /* CbCr */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cb, seek, src_width*2)) return NOK;
        seek += src_width*2;
        cb += chroma_stride;
      }
      break;
    }
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
    {
      /* Lum */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(in, lum , seek, src_width*10/8)) return NOK;
        seek += src_width*10/8;
        lum += luma_stride*10/8;
      }
      w = ((src_width + 1) >> 1);
      h = ((src_height + 1) >> 1);
      /* Cb */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cb, seek, w*10/8)) return NOK;
        seek += w*10/8;
        cb += chroma_stride*10/8;
      }

      /* Cr */
      for (i = 0; i < h; i++)
      {
        if (file_read(in, cr, seek, w*10/8)) return NOK;

        seek += w*10/8;
        cr += chroma_stride*10/8;
      }
      break;
    }
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
    {
      for (i = 0; i < src_height/2; i++)
      {

        if (file_read(in, lum , seek, src_width * 2*2)) return NOK;
        seek += src_width * 2*2;
        lum += luma_stride * 2*2;
      }
      break;
    }
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC:
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_H264:
    {
      if (file_read(in, lum , seek, src_img_size)) return NOK;
          break;
    }
    case VCENC_YUV420_SEMIPLANAR_8BIT_FB:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB:
    case VCENC_YUV420_PLANAR_10BIT_P010_FB:
    {
      if (file_read(in, lum , seek, src_img_size)) return NOK;
      break;
    }
    case VCENC_YUV420_SEMIPLANAR_101010:
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
    case VCENC_YUV420_10BIT_TILE_32_4:
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
    {
      u32 tile_stride = 0,tile_h = 0;
      if (inputFormat == VCENC_YUV420_SEMIPLANAR_101010)
      {
        tile_stride = (src_width + 2)/3*4;
        tile_h = 1;
      }
      else if (inputFormat == VCENC_YUV420_8BIT_TILE_64_4 || inputFormat == VCENC_YUV420_UV_8BIT_TILE_64_4)
      {
        tile_stride = STRIDE(src_width,64)*4;
        tile_h = 4;
      }
      else if (inputFormat == VCENC_YUV420_10BIT_TILE_32_4)
      {
        tile_stride = STRIDE(src_width,32)*2*4;
        tile_h = 4;
      }
      else if (inputFormat == VCENC_YUV420_10BIT_TILE_48_4 || inputFormat == VCENC_YUV420_VU_10BIT_TILE_48_4)
      {
        tile_stride = (src_width + 47)/48*48/3* 4*4;
        tile_h = 4;
      }
      else if (inputFormat == VCENC_YUV420_8BIT_TILE_128_2 || inputFormat == VCENC_YUV420_UV_8BIT_TILE_128_2)
      {
        tile_stride = STRIDE(src_width,128)*2;
        tile_h = 2;
      }
      else if (inputFormat == VCENC_YUV420_10BIT_TILE_96_2 || inputFormat == VCENC_YUV420_VU_10BIT_TILE_96_2)
      {
        tile_stride = (src_width + 95)/96*96/3* 4*2;
        tile_h = 2;
      }

      /* Lum */
      for (i = 0; i < STRIDE(src_height,tile_h)/tile_h; i++)
      {
        if (file_read(in, lum , seek, tile_stride)) return NOK;
        seek += tile_stride;
        lum += luma_stride;
      }

      /* Cb */
      for (i = 0; i < STRIDE(src_height/2,tile_h)/tile_h; i++)
      {
        if (file_read(in, cb, seek, tile_stride)) return NOK;
        seek += tile_stride;
        cb += chroma_stride;
      }

      break;
    }
    case VCENC_YUV420_8BIT_TILE_8_8:
    case VCENC_YUV420_10BIT_TILE_8_8:
    {
      u32 tile_stride = 0;

      if (inputFormat == VCENC_YUV420_8BIT_TILE_8_8)
      {
        tile_stride = STRIDE(src_width,8)*8;
      }
      else
      {
        tile_stride = STRIDE(src_width,8)*8*2;
      }

      /* Lum */
      for (i = 0; i < STRIDE(src_height,8)/8; i++)
      {
        if (file_read(in, lum , seek, tile_stride)) return NOK;
        seek += tile_stride;
        lum += luma_stride;
      }

      if (inputFormat == VCENC_YUV420_8BIT_TILE_8_8)
      {
        tile_stride = STRIDE(src_width,8)*4;
      }
      else
      {
        tile_stride = STRIDE(src_width,8)*4*2;
      }

      /* Cb */
      for (i = 0; i < STRIDE(src_height/2,4)/4; i++)
      {
        if (file_read(in, cb, seek, tile_stride)) return NOK;
        seek += tile_stride;
        cb += chroma_stride;
      }

      break;
    }

    default:
      break;
  }

  return OK;
}


/*------------------------------------------------------------------------------
  read_stab_picture
------------------------------------------------------------------------------*/
i32 read_stab_picture(struct test_bench *tb, u32 inputFormat, u32 src_img_size, u32 src_width, u32 src_height)
{
  i32 num;   /* frame number */
  u64 seek;  /* seek position in bytes */
  u8 *lum_next;
  i32 i;
  u32 luma_stride;
  u32 chroma_stride;

  num = next_picture(tb, tb->encIn.picture_cnt + 1) + tb->firstPic;  /* "+ 1": next frame for stabilization */

  if (num > tb->lastPic)
    return NOK;

  seek = ((u64)num) * ((u64)src_img_size);
  lum_next = (u8*)tb->pictureStabMem.virtualAddress;
  VCEncGetAlignedStride(src_width, inputFormat, &luma_stride, &chroma_stride, 0);  /* no alignment requirement */

  if (tb->formatCustomizedType == 0)
    memset(lum_next, 0, luma_stride*src_height);

  switch(inputFormat)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
      /* Luma */
      for (i = 0; i < src_height; i++)
      {
        if (file_read(tb->in, lum_next, seek, src_width)) return NOK;
        seek += src_width;
        lum_next += luma_stride;
      }
      break;

    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
      for (i = 0; i < src_height; i++)
      {
        if (file_read(tb->in, lum_next, seek, src_width * 2)) return NOK;
        seek += src_width * 2;
        lum_next += luma_stride;
      }
      break;

    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
      for (i = 0; i < src_height; i++)
      {
        if (file_read(tb->in, lum_next, seek, src_width * 4)) return NOK;
        seek += src_width * 4;
        lum_next += luma_stride;
      }
      break;

    default:
      break;
  }

  return OK;
}


/*------------------------------------------------------------------------------
    GetResolution
        Parse image resolution from file name
------------------------------------------------------------------------------*/
u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight)
{
  i32 i;
  u32 w, h;
  i32 len = strlen(filename);
  i32 filenameBegin = 0;

  /* Find last '/' in the file name, it marks the beginning of file name */
  for (i = len - 1; i; --i)
    if (filename[i] == '/')
    {
      filenameBegin = i + 1;
      break;
    }

  /* If '/' found, it separates trailing path from file name */
  for (i = filenameBegin; i <= len - 3; ++i)
  {
    if ((strncmp(filename + i, "subqcif", 7) == 0) ||
        (strncmp(filename + i, "sqcif", 5) == 0))
    {
      *pWidth = 128;
      *pHeight = 96;
      printf("Detected resolution SubQCIF (128x96) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qcif", 4) == 0)
    {
      *pWidth = 176;
      *pHeight = 144;
      printf("Detected resolution QCIF (176x144) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "4cif", 4) == 0)
    {
      *pWidth = 704;
      *pHeight = 576;
      printf("Detected resolution 4CIF (704x576) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "cif", 3) == 0)
    {
      *pWidth = 352;
      *pHeight = 288;
      printf("Detected resolution CIF (352x288) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qqvga", 5) == 0)
    {
      *pWidth = 160;
      *pHeight = 120;
      printf("Detected resolution QQVGA (160x120) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qvga", 4) == 0)
    {
      *pWidth = 320;
      *pHeight = 240;
      printf("Detected resolution QVGA (320x240) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "vga", 3) == 0)
    {
      *pWidth = 640;
      *pHeight = 480;
      printf("Detected resolution VGA (640x480) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "720p", 4) == 0)
    {
      *pWidth = 1280;
      *pHeight = 720;
      printf("Detected resolution 720p (1280x720) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "1080p", 5) == 0)
    {
      *pWidth = 1920;
      *pHeight = 1080;
      printf("Detected resolution 1080p (1920x1080) from file name.\n");
      return 0;
    }
    if (filename[i] == 'x')
    {
      if (sscanf(filename + i - 4, "%ux%u", &w, &h) == 2)
      {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %dx%d from file name.\n", w, h);
        return 0;
      }
      else if (sscanf(filename + i - 3, "%ux%u", &w, &h) == 2)
      {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %dx%d from file name.\n", w, h);
        return 0;
      }
      else if (sscanf(filename + i - 2, "%ux%u", &w, &h) == 2)
      {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %dx%d from file name.\n", w, h);
        return 0;
      }
    }
    if (filename[i] == 'w')
    {
      if (sscanf(filename + i, "w%uh%u", &w, &h) == 2)
      {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %dx%d from file name.\n", w, h);
        return 0;
      }
    }
  }

  return 1;   /* Error - no resolution found */
}



char *nextToken (char *str)
{
  char *p = strchr(str, ' ');
  if (p)
  {
    while (*p == ' ') p ++;
    if (*p == '\0') p = NULL;
  }
  return p;
}

int ParseGopConfigString (char *line, VCEncGopConfig *gopCfg, int frame_idx, int gopSize)
{
  if (!line)
    return -1;

  //format: FrameN Type POC QPoffset QPfactor  num_ref_pics ref_pics  used_by_cur
  int frameN, poc, num_ref_pics, i;
  char type;
  VCEncGopPicConfig *cfg = NULL;
  VCEncGopPicSpecialConfig *scfg = NULL;

  //frame idx
  sscanf (line, "Frame%d", &frameN);
  if ((frameN != (frame_idx + 1)) && (frameN != 0)) return -1;

  if (frameN > gopSize)
      return 0;

  if (0 == frameN)
  {
      //format: FrameN Type  QPoffset  QPfactor   TemporalId  num_ref_pics   ref_pics  used_by_cur  LTR    Offset   Interval
      scfg = &(gopCfg->pGopPicSpecialCfg[gopCfg->special_size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%c", &type);
      if (type == 'I' || type == 'i')
          scfg->codingType = VCENC_INTRA_FRAME;
      else if (type == 'P' || type == 'p')
          scfg->codingType = VCENC_PREDICTED_FRAME;
      else if (type == 'B' || type == 'b')
          scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      else
          scfg->codingType = FRAME_TYPE_RESERVED;

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &(scfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%lf", &(scfg->QpFactor));
      scfg->QpFactor = sqrt(scfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &(scfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &num_ref_pics);
      if (num_ref_pics > VCENC_MAX_REF_FRAMES) /* NUMREFPICS_RESERVED -1 */
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }
      scfg->numRefPics = num_ref_pics;

      if ((scfg->codingType == VCENC_INTRA_FRAME) && (0 == num_ref_pics))
          num_ref_pics = 1;
      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0))
          {
              sscanf(line, "%c%d", &type, &(scfg->refPics[i].ref_pic));
              scfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC( scfg->refPics[i].ref_pic - 1 );
          }
          else
          {
              sscanf(line, "%d", &(scfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf(line, "%u", &(scfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      // LTR
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &scfg->i32Ltr);
      if(VCENC_MAX_LT_REF_FRAMES < scfg->i32Ltr)
          return -1;

      // Offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &scfg ->i32Offset );

      // Interval
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &scfg ->i32Interval );

      if (0 != scfg->i32Ltr)
      {
          gopCfg->u32LTR_idx[ gopCfg->ltrcnt ] = LONG_TERM_REF_ID2DELTAPOC(scfg->i32Ltr-1);
          gopCfg->ltrcnt++;
          if (VCENC_MAX_LT_REF_FRAMES < gopCfg->ltrcnt)
              return -1;
      }

      // short_change
      scfg->i32short_change = 0;
      if (0 == scfg->i32Ltr)
      {
          /* not long-term ref */
          scfg->i32short_change = 1;
          for (i = 0; i < num_ref_pics; i++)
          {
              if (IS_LONG_TERM_REF_DELTAPOC(scfg->refPics[i].ref_pic)&& (0!=scfg->refPics[i].used_by_cur))
              {
                  scfg->i32short_change = 0;
                  break;
              }
          }
      }
  }
  else
  {
      //format: FrameN Type  POC  QPoffset    QPfactor   TemporalId  num_ref_pics  ref_pics  used_by_cur
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%c", &type);
      if (type == 'P' || type == 'p')
          cfg->codingType = VCENC_PREDICTED_FRAME;
      else if (type == 'B' || type == 'b')
          cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      else
          return -1;

      //poc
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &poc);
      if (poc < 1 || poc > gopSize) return -1;
      cfg->poc = poc;

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &(cfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%lf", &(cfg->QpFactor));
      // sqrt(QpFactor) is used in calculating lambda
      cfg->QpFactor = sqrt(cfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &(cfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf(line, "%d", &num_ref_pics);
      if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES)
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }

      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0))
          {
              sscanf(line, "%c%d", &type, &(cfg->refPics[i].ref_pic));
              cfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(cfg->refPics[i].ref_pic - 1);
          }
          else
          {
              sscanf(line, "%d", &(cfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf(line, "%u", &(cfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      cfg->numRefPics = num_ref_pics;
  }

  return 0;
}



int ParseGopConfigFile (int gopSize, char *fname, VCEncGopConfig *gopCfg)
{
#define MAX_LINE_LENGTH 1024
  int frame_idx = 0, line_idx = 0, addTmp;
  char achParserBuffer[MAX_LINE_LENGTH];
  FILE *fIn = fopen (fname, "r");
  if (fIn == NULL)
  {
    printf("GOP Config: Error, Can Not Open File %s\n", fname );
    return -1;
  }

  while ( 0 == feof(fIn))
  {
    if (feof (fIn)) break;
    line_idx ++;
    achParserBuffer[0] = '\0';
    // Read one line
    char *line = fgets ((char *) achParserBuffer, MAX_LINE_LENGTH, fIn);
    if (!line) break;
    //handle line end
    char* s = strpbrk(line, "#\n");
    if(s) *s = '\0';

    addTmp = 1;
    line = strstr(line, "Frame");
    if (line)
    {
      if( 0 == strncmp(line, "Frame0", 6))
          addTmp = 0;

      if (ParseGopConfigString(line, gopCfg, frame_idx, gopSize) < 0)
      {
          printf("Invalid gop configure!\n");
          return -1;
      }

      frame_idx += addTmp;
    }
  }

  fclose(fIn);
  if (frame_idx != gopSize)
  {
    printf ("GOP Config: Error, Parsing File %s Failed at Line %d\n", fname, line_idx);
    return -1;
  }
  return 0;
}


int ReadGopConfig (char *fname, char **config, VCEncGopConfig *gopCfg, int gopSize, u8 *gopCfgOffset)
{
  int ret = -1;

  if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
    return -1;

  if (gopCfgOffset)
    gopCfgOffset[gopSize] = gopCfg->size;
  if(fname)
  {
    ret = ParseGopConfigFile (gopSize, fname, gopCfg);
  }
  else if(config)
  {
    int id = 0;
    while (config[id])
    {
      ParseGopConfigString (config[id], gopCfg, id, gopSize);
      id ++;
    }
    ret = 0;
  }
  return ret;
}


int InitGopConfigs (int gopSize, commandLine_s *cml, VCEncGopConfig *gopCfg, u8 *gopCfgOffset, bool bPass2)
{
  int i, pre_load_num;
  char *fname = cml->gopCfg;
  char **default_configs[8] = {
              cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_1: (IS_H264(cml->codecFormat)?RpsDefault_H264_GOPSize_1:RpsDefault_GOPSize_1),
              cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_2: RpsDefault_GOPSize_2,
              cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_3: RpsDefault_GOPSize_3,
              cml->gopLowdelay ? RpsLowdelayDefault_GOPSize_4: RpsDefault_GOPSize_4,
              RpsDefault_GOPSize_5,
              RpsDefault_GOPSize_6,
              RpsDefault_GOPSize_7,
              RpsDefault_GOPSize_8 };

  if (gopSize < 0 || gopSize > MAX_GOP_SIZE)
  {
    printf ("GOP Config: Error, Invalid GOP Size\n");
    return -1;
  }

  if (bPass2)
  {
    default_configs[1] = RpsPass2_GOPSize_2;
    default_configs[3] = RpsPass2_GOPSize_4;
    default_configs[7] = RpsPass2_GOPSize_8;
  }

  //Handle Interlace
  if (cml->interlacedFrame && gopSize==1)
  {
    default_configs[0] = RpsDefault_Interlace_GOPSize_1;
  }

  // GOP size in rps array for gopSize=N
  // N<=4:      GOP1, ..., GOPN
  // 4<N<=8:   GOP1, GOP2, GOP3, GOP4, GOPN
  // N > 8:       GOP1, GOPN
  // Adaptive:  GOP1, GOP2, GOP3, GOP4, GOP6, GOP8
  if (gopSize > 8)
    pre_load_num = 1;
  else if (gopSize>=4 || gopSize==0)
    pre_load_num = 4;
  else
    pre_load_num = gopSize;

  gopCfg->special_size = 0;
  gopCfg->ltrcnt       = 0;

  for (i = 1; i <= pre_load_num; i ++)
  {
    if (ReadGopConfig (gopSize==i ? fname : NULL, default_configs[i-1], gopCfg, i, gopCfgOffset))
      return -1;
  }

  if (gopSize == 0)
  {
    //gop6
    if (ReadGopConfig (NULL, default_configs[5], gopCfg, 6, gopCfgOffset))
      return -1;
    //gop8
    if (ReadGopConfig (NULL, default_configs[7], gopCfg, 8, gopCfgOffset))
      return -1;
  }
  else if (gopSize > 4)
  {
    //gopSize
    if (ReadGopConfig (fname, default_configs[gopSize-1], gopCfg, gopSize, gopCfgOffset))
      return -1;
  }

  if ((DEFAULT != cml->ltrInterval) && (gopCfg->special_size == 0))
  {
      if (cml->gopSize != 1)
      {
          printf("GOP Config: Error, when using --LTR configure option, the gopsize alse should be set to 1!\n");
          return -1;
      }
      gopCfg->pGopPicSpecialCfg[0].poc = 0;
      gopCfg->pGopPicSpecialCfg[0].QpOffset = cml->longTermQpDelta;
      gopCfg->pGopPicSpecialCfg[0].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].numRefPics = NUMREFPICS_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].i32Ltr = 1;
      gopCfg->pGopPicSpecialCfg[0].i32Offset = 0;
      gopCfg->pGopPicSpecialCfg[0].i32Interval = cml->ltrInterval;
      gopCfg->pGopPicSpecialCfg[0].i32short_change = 0;
      gopCfg->u32LTR_idx[0]                    = LONG_TERM_REF_ID2DELTAPOC(0);

      gopCfg->pGopPicSpecialCfg[1].poc = 0;
      gopCfg->pGopPicSpecialCfg[1].QpOffset = QPOFFSET_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].numRefPics = 2;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].ref_pic     = -1;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].refPics[1].ref_pic     = LONG_TERM_REF_ID2DELTAPOC(0);
      gopCfg->pGopPicSpecialCfg[1].refPics[1].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].i32Ltr = 0;
      gopCfg->pGopPicSpecialCfg[1].i32Offset = cml->longTermGapOffset;
      gopCfg->pGopPicSpecialCfg[1].i32Interval = cml->longTermGap;
      gopCfg->pGopPicSpecialCfg[1].i32short_change = 0;

      gopCfg->special_size = 2;
      gopCfg->ltrcnt = 1;
  }

  if (0)
    for(i = 0; i < (gopSize == 0 ? gopCfg->size : gopCfgOffset[gopSize]); i++)
    {
      // when use long-term, change P to B in default configs (used for last gop)
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_PREDICTED_FRAME)
        cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    }

  //Compatible with old bFrameQpDelta setting
  if (cml->bFrameQpDelta >= 0 && fname == NULL)
  {
    for (i = 0; i < gopCfg->size; i++)
    {
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_BIDIR_PREDICTED_FRAME)
        cfg->QpOffset = cml->bFrameQpDelta;
    }
  }

  // lowDelay auto detection
  VCEncGopPicConfig *cfgStart = &(gopCfg->pGopPicCfg[gopCfgOffset[gopSize]]);
  if (gopSize == 1)
  {
    cml->gopLowdelay = 1;
  }
  else if ((gopSize > 1) && (cml->gopLowdelay == 0))
  {
     cml->gopLowdelay = 1;
     for (i = 1; i < gopSize; i++)
     {
        if (cfgStart[i].poc < cfgStart[i-1].poc)
        {
          cml->gopLowdelay = 0;
          break;
        }
     }
  }

#ifdef INTERNAL_TEST
  if ((cml->testId == TID_POC && gopSize == 1) && !IS_H264(cml->codecFormat) && !IS_AV1(cml->codecFormat))
  {
    VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[0]);
    if (cfg->numRefPics == 2)
      cfg->refPics[1].ref_pic = -(cml->intraPicRate-1);
  }
#endif

  {
      i32 i32LtrPoc[VCENC_MAX_LT_REF_FRAMES];

      for (i = 0; i < VCENC_MAX_LT_REF_FRAMES; i++)
          i32LtrPoc[i] = -1;
      for (i = 0; i < gopCfg->special_size; i++)
      {
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > VCENC_MAX_LT_REF_FRAMES)
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > 0)
              i32LtrPoc[i] = gopCfg->pGopPicSpecialCfg[i].i32Ltr - 1;
      }

      for (i = 0; i < gopCfg->ltrcnt; i++)
      {
          if ((0 != i32LtrPoc[0]) || (-1 == i32LtrPoc[i]) || ((i>0) && i32LtrPoc[i] != (i32LtrPoc[i - 1] + 1)))
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
      }
  }

  //For lowDelay, Handle the first few frames that miss reference frame
  if (1)
  {
    int nGop;
    int idx = 0;
    int maxErrFrame = 0;
    VCEncGopPicConfig *cfg;

    // Find the max frame number that will miss its reference frame defined in rps
    while ((idx - maxErrFrame) < gopSize)
    {
      nGop = (idx / gopSize) * gopSize;
      cfg = &(cfgStart[idx % gopSize]);

      for (i = 0; i < cfg->numRefPics; i ++)
      {
        //POC of this reference frame
        int refPoc = cfg->refPics[i].ref_pic + cfg->poc + nGop;
        if (refPoc < 0)
        {
          maxErrFrame = idx + 1;
        }
      }
      idx ++;
    }

    // Try to config a new rps for each "error" frame by modifying its original rps
    for (idx = 0; idx < maxErrFrame; idx ++)
    {
      int j, iRef, nRefsUsedByCur, nPoc;
      VCEncGopPicConfig *cfgCopy;

      if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
        break;

      // Add to array end
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size]);
      cfgCopy = &(cfgStart[idx % gopSize]);
      memcpy (cfg, cfgCopy, sizeof (VCEncGopPicConfig));
      gopCfg->size ++;

      // Copy reference pictures
      nRefsUsedByCur = iRef = 0;
      nPoc = cfgCopy->poc + ((idx / gopSize) * gopSize);
      for (i = 0; i < cfgCopy->numRefPics; i ++)
      {
        int newRef = 1;
        int used_by_cur = cfgCopy->refPics[i].used_by_cur;
        int ref_pic = cfgCopy->refPics[i].ref_pic;
        // Clip the reference POC
        if ((cfgCopy->refPics[i].ref_pic + nPoc) < 0)
          ref_pic = 0 - (nPoc);

        // Check if already have this reference
        for (j = 0; j < iRef; j ++)
        {
          if (cfg->refPics[j].ref_pic == ref_pic)
          {
             newRef = 0;
             if (used_by_cur)
               cfg->refPics[j].used_by_cur = used_by_cur;
             break;
          }
        }

        // Copy this reference
        if (newRef)
        {
          cfg->refPics[iRef].ref_pic = ref_pic;
          cfg->refPics[iRef].used_by_cur = used_by_cur;
          iRef ++;
        }
      }
      cfg->numRefPics = iRef;
      // If only one reference frame, set P type.
      for (i = 0; i < cfg->numRefPics; i ++)
      {
        if (cfg->refPics[i].used_by_cur)
          nRefsUsedByCur ++;
      }
      if (nRefsUsedByCur == 1)
        cfg->codingType = VCENC_PREDICTED_FRAME;
    }
  }

#if 0
      //print for debug
      int idx;
      printf ("====== REF PICTURE SETS from %s ======\n",fname ? fname : "DEFAULT");
      for (idx = 0; idx < gopCfg->size; idx ++)
      {
        int i;
        VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[idx]);
        char type = cfg->codingType==VCENC_PREDICTED_FRAME ? 'P' : cfg->codingType == VCENC_INTRA_FRAME ? 'I' : 'B';
        printf (" FRAME%2d:  %c %d %d %f %d", idx, type, cfg->poc, cfg->QpOffset, cfg->QpFactor, cfg->numRefPics);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].ref_pic);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].used_by_cur);
        printf("\n");
      }
      printf ("===========================================\n");
#endif
  return 0;
}

/*------------------------------------------------------------------------------

    ReadUserData
        Read user data from file and pass to encoder

    Params:
        name - name of file in which user data is located

    Returns:
        NULL - when user data reading failed
        pointer - allocated buffer containing user data

------------------------------------------------------------------------------*/
u8 *ReadUserData(VCEncInst encoder, char *name)
{
  FILE *file = NULL;
  i32 byteCnt;
  u8 *data;

  if (name == NULL)
    return NULL;

  if (strcmp("0", name) == 0)
    return NULL;

  /* Get user data length from file */
  file = fopen(name, "rb");
  if (file == NULL)
  {
    printf("Unable to open User Data file: %s\n", name);
    return NULL;
  }
  fseeko(file, 0L, SEEK_END);
  byteCnt = ftell(file);
  rewind(file);

  /* Minimum size of user data */
  if (byteCnt < 16)
    byteCnt = 16;

  /* Maximum size of user data */
  if (byteCnt > 2048)
    byteCnt = 2048;

  /* Allocate memory for user data */
  if ((data = (u8 *) malloc(sizeof(u8) * byteCnt)) == NULL)
  {
    fclose(file);
    printf("Unable to alloc User Data memory\n");
    return NULL;
  }

  /* Read user data from file */
  i32 ret = fread(data, sizeof(u8), byteCnt, file);
  fclose(file);

  printf("User data: %d bytes [%d %d %d %d ...]\n",
         byteCnt, data[0], data[1], data[2], data[3]);

  /* Pass the data buffer to encoder
   * The encoder reads the buffer during following VCEncStrmEncode() calls.
   * User data writing must be disabled (with VCEncSetSeiUserData(enc, 0, 0)) */
  VCEncSetSeiUserData(encoder, data, byteCnt);

  return data;
}

/*------------------------------------------------------------------------------
    Add new frame bits for moving average bitrate calculation
------------------------------------------------------------------------------*/
void MaAddFrame(ma_s *ma, i32 frameSizeBits)
{
    ma->frame[ma->pos++] = frameSizeBits;

    if (ma->pos == ma->length)
        ma->pos = 0;

    if (ma->count < ma->length)
        ma->count++;
}

/*------------------------------------------------------------------------------
    Calculate average bitrate of moving window
------------------------------------------------------------------------------*/
i32 Ma(ma_s *ma)
{
    i32 i;
    unsigned long long sum = 0;     /* Using 64-bits to avoid overflow */

    for (i = 0; i < ma->count; i++)
        sum += ma->frame[i];

    if (!ma->frameRateDenom)
        return 0;

    sum = sum / ma->count;

    return sum * (ma->frameRateNumer+ma->frameRateDenom-1) / ma->frameRateDenom;
}

i32 getNextGopSize(struct test_bench *tb, VCEncIn *pEncIn, VCEncInst encoder, i32 *pNextGopSize, adapGopCtr *agop, u32 lookaheadDepth, VCEncOut *pEncOut)
{
  if (lookaheadDepth)
  {
    i32 updGop = VCEncGetPass1UpdatedGopSize(encoder);
    if (updGop)
      *pNextGopSize = updGop;
  }
  else if (pEncIn->codingType != VCENC_INTRA_FRAME)
    AdaptiveGopDecision(tb, pEncIn, pEncOut, pNextGopSize, agop);

  return *pNextGopSize;
}

i32 AdaptiveGopDecision(struct test_bench *tb, VCEncIn *pEncIn, VCEncOut *pEncOut, i32 *pNextGopSize, adapGopCtr * agop)
{
    i32 nextGopSize =-1;

    unsigned int uiIntraCu8Num = pEncOut->cuStatis.intraCu8Num;
    unsigned int uiSkipCu8Num = pEncOut->cuStatis.skipCu8Num;
    unsigned int uiPBFrameCost = pEncOut->cuStatis.PBFrame4NRdCost;
    double dIntraVsInterskip = (double)uiIntraCu8Num/(double)((tb->width/8) * (tb->height/8));
    double dSkipVsInterskip = (double)uiSkipCu8Num/(double)((tb->width/8) * (tb->height/8));

    agop->gop_frm_num++;
    agop->sum_intra_vs_interskip += dIntraVsInterskip;
    agop->sum_skip_vs_interskip += dSkipVsInterskip;
    agop->sum_costP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? uiPBFrameCost:0;
    agop->sum_costB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? uiPBFrameCost:0;
    agop->sum_intra_vs_interskipP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? dIntraVsInterskip:0;
    agop->sum_intra_vs_interskipB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? dIntraVsInterskip:0;

    if(pEncIn->gopPicIdx == pEncIn->gopSize-1)//last frame of the current gop. decide the gopsize of next gop.
    {
      dIntraVsInterskip = agop->sum_intra_vs_interskip/agop->gop_frm_num;
      dSkipVsInterskip = agop->sum_skip_vs_interskip/agop->gop_frm_num;
      agop->sum_costB = (agop->gop_frm_num>1)?(agop->sum_costB/(agop->gop_frm_num-1)):0xFFFFFFF;
      agop->sum_intra_vs_interskipB = (agop->gop_frm_num>1)?(agop->sum_intra_vs_interskipB/(agop->gop_frm_num-1)):0xFFFFFFF;
      //Enabled adaptive GOP size for large resolution
      if (((tb->width * tb->height) >= (1280 * 720)) || ((MAX_ADAPTIVE_GOP_SIZE >3)&&((tb->width * tb->height) >= (416 * 240))))
      {
          if ((((double)agop->sum_costP/(double)agop->sum_costB)<1.1)&&(dSkipVsInterskip >= 0.95))
          {
              agop->last_gopsize = nextGopSize = 1;
          }
          else if (((double)agop->sum_costP/(double)agop->sum_costB)>5)
          {
              nextGopSize = agop->last_gopsize;
          }
          else
          {
              if( ((agop->sum_intra_vs_interskipP > 0.40) && (agop->sum_intra_vs_interskipP < 0.70)&& (agop->sum_intra_vs_interskipB < 0.10)) )
              {
                  agop->last_gopsize++;
                  if(agop->last_gopsize==5 || agop->last_gopsize==7)
                  {
                      agop->last_gopsize++;
                  }
                  agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                  nextGopSize = agop->last_gopsize; //
              }
              else if (dIntraVsInterskip >= 0.30)
              {
                  agop->last_gopsize = nextGopSize = 1; //No B
              }
              else if (dIntraVsInterskip >= 0.20)
              {
                  agop->last_gopsize = nextGopSize = 2; //One B
              }
              else if (dIntraVsInterskip >= 0.10)
              {
                  agop->last_gopsize--;
                  if(agop->last_gopsize == 5 || agop->last_gopsize==7)
                  {
                      agop->last_gopsize--;
                  }
                  agop->last_gopsize = MAX(agop->last_gopsize, 3);
                  nextGopSize = agop->last_gopsize; //
              }
              else
              {
                  agop->last_gopsize++;
                  if(agop->last_gopsize==5 || agop->last_gopsize==7)
                  {
                      agop->last_gopsize++;
                  }
                  agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                  nextGopSize = agop->last_gopsize; //
              }
          }
      }
      else
      {
        nextGopSize = 3;
      }
      agop->gop_frm_num = 0;
      agop->sum_intra_vs_interskip = 0;
      agop->sum_skip_vs_interskip = 0;
      agop->sum_costP = 0;
      agop->sum_costB = 0;
      agop->sum_intra_vs_interskipP = 0;
      agop->sum_intra_vs_interskipB = 0;

      nextGopSize = MIN(nextGopSize, MAX_ADAPTIVE_GOP_SIZE);

    }

    if(nextGopSize != -1)
        *pNextGopSize = nextGopSize;

    return nextGopSize;
}

u32 SetupInputBuffer(struct test_bench *tb, commandLine_s *cml, VCEncIn *pEncIn)
{
    u32 src_img_size;

    if ((tb->input_alignment == 0 || cml->formatCustomizedType != -1) && 0 != VCEncGetBitsPerPixel(cml->inputFormat))
    {
      u32 size_lum = 0;
      u32 size_ch  = 0;

      getAlignedPicSizebyFormat(cml->inputFormat, cml->lumWidthSrc, cml->lumHeightSrc, 0, &size_lum, &size_ch, NULL);

      pEncIn->busLuma = tb->pictureMem->busAddress;
      tb->lum = (u8 *)tb->pictureMem->virtualAddress;
      pEncIn->busChromaU = pEncIn->busLuma + size_lum;
      tb->cb = tb->lum + (u32)size_lum;
      pEncIn->busChromaV = pEncIn->busChromaU + (u32)size_ch/2;
      tb->cr = tb->cb + (u32)size_ch/2;
      src_img_size = cml->lumWidthSrc * cml->lumHeightSrc *
                   VCEncGetBitsPerPixel(cml->inputFormat) / 8;
    }
    else
    {
      pEncIn->busLuma = tb->pictureMem->busAddress;
      tb->lum = (u8 *)tb->pictureMem->virtualAddress;
      pEncIn->busChromaU = pEncIn->busLuma + tb->lumaSize;
      tb->cb = tb->lum + tb->lumaSize;

      pEncIn->busChromaV = pEncIn->busChromaU + tb->chromaSize/2;
      tb->cr = tb->cb + tb->chromaSize/2;
      if (0 != VCEncGetBitsPerPixel(cml->inputFormat))
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc *
                   VCEncGetBitsPerPixel(cml->inputFormat) / 8;
      else
      {
        if (cml->inputFormat >= VCENC_YUV420_SEMIPLANAR_101010 && cml->inputFormat <= VCENC_YUV420_10BIT_TILE_8_8)
          getAlignedPicSizebyFormat(cml->inputFormat, cml->lumWidthSrc, cml->lumHeightSrc, 0, NULL, NULL, &src_img_size);
        else
          src_img_size = tb->lumaSize + tb->chromaSize;
      }
    }

    if (cml->halfDsInput) {
      u32 size_lum = 0;
      u32 size_ch  = 0;

      u32 alignment = (cml->formatCustomizedType != -1? 0 : tb->input_alignment);
      getAlignedPicSizebyFormat(cml->inputFormat, cml->lumWidthSrc/2, cml->lumHeightSrc/2, alignment, &size_lum, &size_ch, NULL);

      /* save full resolution yuv for 2nd pass */
      pEncIn->busLumaOrig = pEncIn->busLuma;
      pEncIn->busChromaUOrig = pEncIn->busChromaU;
      pEncIn->busChromaVOrig = pEncIn->busChromaV;
      /* setup down-sampled yuv for 1nd pass */
      pEncIn->busLuma = tb->pictureDSMem->busAddress;
      tb->lumDS = (u8*)tb->pictureDSMem->virtualAddress;
      pEncIn->busChromaU = pEncIn->busLuma + size_lum;
      tb->cbDS = tb->lumDS + (u32)size_lum;
      pEncIn->busChromaV = pEncIn->busChromaU + (u32)size_ch/2;
      tb->crDS = tb->cbDS + (u32)size_ch/2;
      tb->src_img_size_ds = cml->lumWidthSrc/2 * cml->lumHeightSrc/2 * VCEncGetBitsPerPixel(cml->inputFormat) / 8;
    } else {
      pEncIn->busLumaOrig =
      pEncIn->busChromaUOrig =
      pEncIn->busChromaVOrig = (ptr_t)NULL;
      tb->src_img_size_ds = 0;
    }
    if (!tb->picture_enc_cnt)
      printf("Reading input from file <%s>, frame size %d bytes.\n",
              cml->input, src_img_size);

    return src_img_size;
}

void SetupOutputBuffer(struct test_bench *tb, VCEncIn *pEncIn)
{
  i32 iBuf;
  for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
  {
    //find output buffer of multi-cores
    tb->outbufMem[iBuf] = &(tb->outbufMemFactory[tb->picture_enc_cnt % tb->parallelCoreNum][iBuf]);

    pEncIn->busOutBuf[iBuf] = tb->outbufMem[iBuf]->busAddress;
    pEncIn->outBufSize[iBuf] = tb->outbufMem[iBuf]->size;
    pEncIn->pOutBuf[iBuf] = tb->outbufMem[iBuf]->virtualAddress;

    tb->Av1PreoutbufMem[iBuf] = &(tb->Av1PreoutbufMemFactory[tb->picture_enc_cnt % tb->parallelCoreNum][iBuf]);
	pEncIn->busAv1PrecarryOutBuf[iBuf] = tb->Av1PreoutbufMem[iBuf]->busAddress;
  }
}

void SetupExtSRAM(struct test_bench *tb, VCEncIn *pEncIn)
{
  //find external SRAM of multi-cores
  pEncIn->extSRAMLumBwdBase = tb->extSRAMMemFactory[tb->picture_enc_cnt % tb->parallelCoreNum].busAddress;
  pEncIn->extSRAMLumFwdBase = pEncIn->extSRAMLumBwdBase + tb->extSramLumBwdSize;
  pEncIn->extSRAMChrBwdBase = pEncIn->extSRAMLumFwdBase + tb->extSramLumFwdSize;
  pEncIn->extSRAMChrFwdBase = pEncIn->extSRAMChrBwdBase + tb->extSramChrBwdSize;

}

void SetupDec400CompTable(struct test_bench *tb, VCEncIn *pEncIn)
{
  pEncIn->dec400LumTableBase = tb->Dec400CmpTableMem.busAddress;
  pEncIn->dec400CbTableBase = tb->Dec400CmpTableMem.busAddress + tb->dec400LumaTableSize;
  pEncIn->dec400CrTableBase = pEncIn->dec400CbTableBase + tb->dec400ChrTableSize/2;
}

void SetupROIMapVer3(struct test_bench *tb, commandLine_s *cml, VCEncInst encoder)
{
  u8* memory;
  u8 u8RoiData[24];

  i32 blkSize;
  i32 num;
  i32 size;
  i32 ret = NOK;
  i32 block_num,i,j,m, n, block_width, block_height, mb_width, last_mb_height;
  i32 ctb_block_num, mb_block_num, block_cu8_rnum, block_cu8_cnum, last_block_cu8_cnum, block_cu8_num, ctb_row, ctb_column, read_size;
  u64 block_base_addr;
  u32 ctb_num, blkInCtb, blkRowInCtb, blkColumInCtb, cu8NumInCtb;
  bool roiMapEnable = ((cml->roiMapDeltaQpEnable == 1) || (cml->roiMapInfoBinFile != NULL) || (cml->RoimapCuCtrlInfoBinFile != NULL));

  // V3 ROI
  if ((tb->roiMapFile) && (cml->RoiQpDeltaVer < 3))
  {
    // RoiQpDelta_ver == 0,1,2
    copyQPDelta2Memory(cml, encoder, tb, cml->RoiQpDeltaVer);
  }
  else if(roiMapEnable && (tb->roiMapInfoBinFile != NULL))
  {
    // RoiQpDelta_ver == 1,2,3
    memory = (u8*)tb->roiMapDeltaQpMem->virtualAddress;

    // need fill into buffer of 8x8 block_size
    blkSize = 64 >> (cml->roiMapDeltaQpBlockUnit & 3);
    num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;
    ret = NOK;

    if (num <= tb->lastPic)
    {
      switch(cml->roiMapDeltaQpBlockUnit)
      {
        case 0:
          block_num=64;
          break;
        case 1:
          block_num=16;
          break;
        case 2:
          block_num=4;
          break;
        case 3:
          block_num=1;
          break;
        default:
          block_num=64;
          break;
      }

      block_width     = (((cml->width + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) + blkSize - 1) / blkSize;
      mb_width        = ((cml->width + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1)))/ 8;
      block_height    = (((cml->height + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) + blkSize - 1) / blkSize;
      size            = block_width * block_height;
      block_base_addr = ((u64)num) * ((u64)size);
      block_cu8_cnum  = blkSize / 8;
      last_block_cu8_cnum = (((cml->width + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) - (block_width-1)*blkSize) / 8;
      last_mb_height  = (((cml->height + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) - (block_height-1) * blkSize) / 16;

      if (IS_H264(cml->codecFormat))
      {
          // h264
          ctb_block_num  = blkSize / cml->max_cu_size;
          block_cu8_rnum = block_cu8_cnum / ctb_block_num;
          cu8NumInCtb    = block_cu8_rnum*block_cu8_cnum;
          for (i = 0; i < size; i++)
          {
              ret = file_read(tb->roiMapInfoBinFile, u8RoiData, (block_base_addr + i), 1);
              if (ret == NOK)
                  break;

              ctb_num  = i / block_width;
              blkInCtb = i % block_width;

              if (blkInCtb == (block_width - 1))
                  block_cu8_num = last_block_cu8_cnum;
              else
                  block_cu8_num = block_cu8_cnum;

              if (ctb_num == (block_height - 1))
                  mb_block_num = last_mb_height;
              else
                  mb_block_num = ctb_block_num;

              for (j = 0; j < mb_block_num; j++)
              {
                  for (m = 0; m < block_cu8_rnum; m++)
                  {
                      for (n = 0; n < block_cu8_num; n++)
                      {
                          memory[(ctb_num*ctb_block_num + j) * mb_width*block_cu8_rnum + blkInCtb * cu8NumInCtb + m*block_cu8_num + n] = u8RoiData[0];
                      }
                  }
              }
          }
      }
      else
      {
          // hevc
          ctb_block_num = cml->max_cu_size / blkSize;
          cu8NumInCtb   = ctb_block_num*ctb_block_num*block_cu8_cnum*block_cu8_cnum;

          for (i = 0; i < size; i++)
          {
              ret = file_read(tb->roiMapInfoBinFile, u8RoiData, (block_base_addr + i), 1);
              if (ret == NOK)
                  break;

              ctb_num       = i / (ctb_block_num*ctb_block_num);
              blkInCtb      = i % (ctb_block_num*ctb_block_num);
              blkRowInCtb   = blkInCtb / ctb_block_num;
              blkColumInCtb = blkInCtb % ctb_block_num;

              for (m = 0; m < block_cu8_cnum; m++)
              {
                  for (n = 0; n < block_cu8_cnum; n++)
                  {
                      memory[ctb_num * cu8NumInCtb + (blkRowInCtb*block_cu8_cnum + m)*ctb_block_num*block_cu8_cnum + blkColumInCtb*block_cu8_cnum + n] = u8RoiData[0];
                  }
              }
          }
      }
    }

    if(ret == NOK)
      memset((u8*)tb->roiMapDeltaQpMem->virtualAddress, 0, tb->roiMapDeltaQpMem->size);
  }

  // V3~V7 ROI index
  if(tb->RoimapCuCtrlIndexBinFile != NULL)
  {
    memory = (u8*)tb->RoimapCuCtrlIndexMem->virtualAddress;

    blkSize = IS_H264(cml->codecFormat) ? 16 : 64;
    num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;
    size = (((cml->width + blkSize - 1)/blkSize) * ((cml->height + blkSize - 1)/blkSize) +7) >> 3;
    ret = NOK;
    if (num <= tb->lastPic)
    {
      for(i = 0; i < size; i += read_size)
      {
        if((size - i) > 1024)
          read_size = 1024;
        else
          read_size = size - i;

        ret = file_read(tb->RoimapCuCtrlIndexBinFile, (u8 *)(memory+i), (((u64)num) * ((u64)size) + i), read_size);
        if(ret == NOK)
          break;
      }
    }

    if(ret == NOK)
      memset((u8*)tb->RoimapCuCtrlIndexMem->virtualAddress, 0, tb->RoimapCuCtrlIndexMem->size);
  }

  // V3~V7 ROI
  if(tb->RoimapCuCtrlInfoBinFile != NULL)
  {
    memory = (u8*)tb->RoimapCuCtrlInfoMem->virtualAddress;

    blkSize = 8;
    num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;

    block_width   = (((cml->width + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) + blkSize - 1) / blkSize;
    block_height  = (((cml->height + cml->max_cu_size - 1)& (~(cml->max_cu_size - 1))) + blkSize - 1) / blkSize;
    size          = block_width * block_height;
    ret = NOK;
    switch(cml->RoiCuCtrlVer)
    {
      case 3:
        block_num=1;
        break;
      case 4:
        block_num=2;
        break;
      case 5:
        block_num=6;
        break;
      case 6:
        block_num=12;
        break;
      default:
        block_num=14;
        break;
    }

    size =  size * block_num;
    if (num <= tb->lastPic)
    {
      for(i = 0; i < size; i += read_size)
      {
        if((size - i) > 1024)
          read_size = 1024;
        else
          read_size = size - i;

        ret = file_read(tb->RoimapCuCtrlInfoBinFile, (u8 *)(memory+i), (((u64)num) * ((u64)size) + i), read_size);
        if(ret == NOK)
          break;
      }
    }

    if(ret == NOK)
      memset((u8*)tb->RoimapCuCtrlInfoMem->virtualAddress, 0, tb->RoimapCuCtrlInfoMem->size);
  }
}

void SetupROIMapBuffer(struct test_bench *tb, commandLine_s *cml, VCEncIn *pEncIn, VCEncInst encoder)
{
  const EWLHwConfig_t asicCfg = VCEncGetAsicConfig(cml->codecFormat);
  pEncIn->roiMapDeltaQpAddr = tb->roiMapDeltaQpMem->busAddress;

  /* copy config data to memory. allocate delta qp map memory. */
  if ((cml->lookaheadDepth) && tb->roiMapBinFile)
  {
    pEncIn->pRoiMapDelta = (i8 *)tb->roiMapDeltaQpMem->virtualAddress;
    pEncIn->roiMapDeltaSize = tb->roiMapDeltaQpMem->size;
    i32 blkSize = 64 >> (cml->roiMapDeltaQpBlockUnit&3);
    i32 num = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;
    i32 size = ((cml->width + blkSize - 1)/blkSize) * ((cml->height + blkSize - 1)/blkSize);
    i32 ret = NOK;

    if (num <= tb->lastPic)
      ret = file_read(tb->roiMapBinFile, (u8 *)pEncIn->pRoiMapDelta, ((u64)num) * ((u64)size), size);

    if (ret != OK)
      memset(pEncIn->pRoiMapDelta, 0, size);
  }

  if(asicCfg.roiMapVersion == 3)
  {
      pEncIn->RoimapCuCtrlAddr      = tb->RoimapCuCtrlInfoMem->busAddress;
      pEncIn->RoimapCuCtrlIndexAddr = tb->RoimapCuCtrlIndexMem->busAddress;
      SetupROIMapVer3(tb, cml, encoder);
  }
  else if (tb->roiMapFile)
  {
    copyQPDelta2Memory(cml, encoder, tb, asicCfg.roiMapVersion);
  }

  if (tb->ipcmMapFile || tb->skipMapFile)
    copyFlagsMap2Memory(cml, encoder, tb);
}

void SetupOverlayBuffer(struct test_bench *tb, commandLine_s *cml, VCEncIn *pEncIn)
{
  int i,j;
  u8 *lum, *cb, *cr;
  u32 src_width, size_luma;
  u64 seek;
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    pEncIn->overlayEnable[i] = (cml->overlayEnables >> i) & 1;
    if((cml->overlayEnables >> i) & 1)
    {
			int num = next_picture(tb, tb->encIn.picture_cnt % tb->olFrameNume[i]);
			tb->encIn.overlay_picture_cnt[i] = tb->encIn.picture_cnt % tb->olFrameNume[i];
      //int num = next_picture(tb, tb->encIn.overlay_picture_cnt[i]);
      u32 y_stride = cml->olYStride[i];
      u32 uv_stride = cml->olUVStride[i];
      pEncIn->overlayInputYAddr[i] = tb->overlayInputMem[i]->busAddress;
      lum = (u8*)tb->overlayInputMem[i]->virtualAddress;

      switch(cml->olFormat[i])
      {
        case 0:  //ARGB
          src_width = cml->olWidth[i] * 4;
          seek = num * (cml->olHeight[i]*cml->olWidth[i]*4);
          for(j = 0; j < cml->olHeight[i]; j++)
          {
            if(file_read(tb->overlayFile[i], lum, seek, src_width))
            {
							printf("Error: fail to read overlay\n");
							pEncIn->overlayEnable[i] = 0;
							break;
            }
            else
            {
              seek += src_width;
              lum += y_stride;
            }
          }
          break;
        case 1: //NV12
          size_luma = cml->olYStride[i]*cml->olHeight[i];
          seek = num * ((cml->olWidth[i]*cml->olHeight[i]) + (cml->olWidth[i])*cml->olHeight[i]/2);
          src_width = cml->olWidth[i];
          pEncIn->overlayInputUAddr[i] = pEncIn->overlayInputYAddr[i] + size_luma; //Cb and Cr
          cb = lum + size_luma;
          //Luma
          for(j = 0; j < cml->olHeight[i];j++)
          {
            if(file_read(tb->overlayFile[i], lum, seek, src_width))
            {
							printf("Error: fail to read overlay\n");
							pEncIn->overlayEnable[i] = 0;
							break;
            }
            else
            {
              seek += src_width;
              lum += y_stride;
            }
          }
          //Cb and Cr
          for(j = 0; j < (cml->olHeight[i]/2);j++)
          {
            if(file_read(tb->overlayFile[i], cb, seek, src_width))
            {
              //This situation means corrupted input, so do not rolling back
              pEncIn->overlayEnable[i] = 0;
              break;
            }
            seek += src_width;
            cb += uv_stride;
          }
          break;
        case 2: //Bitmap
          /* Foreground pixels in bitmap need to be 2x2 aligned, otherwise there will be avoidless chroma
             artifact cased by YUV420 coding. This chroma artifact is affected by alpha value, background
             chroma, foreground bitmap chroma. */
          src_width = cml->olWidth[i] / 8;
          seek = num * cml->olHeight[i]*cml->olWidth[i]/8;
          for(j = 0; j < cml->olHeight[i]; j++)
          {
            if(file_read(tb->overlayFile[i], lum, seek, src_width))
            {
							printf("Error: fail to read overlay\n");
							pEncIn->overlayEnable[i] = 0;
							break;
            }
            else
            {
              seek += src_width;
              lum += y_stride;
            }
          }
          break;
        default: break;
      }
    }
  }
}


FILE* FormatCustomizedYUV(struct test_bench *tb, commandLine_s *cml)
{
    if ((cml->formatCustomizedType == 0)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      transYUVtoDHformat(tb,cml);
    }

    if ((cml->formatCustomizedType == 1)&&
       ((cml->inputFormat==VCENC_YUV420_SEMIPLANAR)||(cml->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)||(cml->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010)))
    {
      transYUVtoFBformat(tb,cml);
    }

    if (((cml->formatCustomizedType >= 12)&&(cml->formatCustomizedType <= 13))&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      transYUVtoCommDataformat(tb,cml);
    }

    if ((cml->formatCustomizedType == 2)&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      return transYUVtoP101010format(tb,cml);
    }

    if (((cml->formatCustomizedType >= 3)&&(cml->formatCustomizedType <= 11))&&(cml->inputFormat==VCENC_YUV420_PLANAR))
    {
      return transYUVto256BTileformat(tb,cml);
    }

    return NULL;

}

void GetFreeIOBuffer(struct test_bench *tb)
{
    i32 iBuf;

    //find output buffer of multi-cores
    tb->pictureMem = &(tb->pictureMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);
    tb->pictureDSMem = &(tb->pictureDSMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);

    //find output buffer of multi-cores
    for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++){
      tb->outbufMem[iBuf] = &(tb->outbufMemFactory[tb->picture_enc_cnt % tb->parallelCoreNum][iBuf]);
	  tb->Av1PreoutbufMem[iBuf] = &(tb->Av1PreoutbufMemFactory[tb->picture_enc_cnt % tb->parallelCoreNum][iBuf]);
    }

    //find ROI Map buffer of multi-cores
    tb->roiMapDeltaQpMem = &(tb->roiMapDeltaQpMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);

    //find transform buffer of multi-cores
    tb->transformMem = &(tb->transformMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);
    tb->RoimapCuCtrlInfoMem  = &(tb->RoimapCuCtrlInfoMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);
    tb->RoimapCuCtrlIndexMem = &(tb->RoimapCuCtrlIndexMemFactory[tb->picture_enc_cnt % tb->buffer_cnt]);

	/*find overlay buffer of multi-cores*/
	for(iBuf = 0; iBuf < MAX_OVERLAY_NUM; iBuf++)
	{
		if(tb->olEnable[iBuf])
	    tb->overlayInputMem[iBuf] = &(tb->overlayInputMemFactory[(tb->picture_enc_cnt % tb->olFrameNume[iBuf]) % tb->buffer_cnt][iBuf]);
	}
}

void InitSliceCtl(struct test_bench *tb, struct commandLine_s *cml)
{
  int i;
  for(i = 0; i < MAX_CORE_NUM; i++) {
    tb->sliceCtlFactory[i].multislice_encoding=(cml->sliceSize!=0&&(((cml->height+63)/64)>cml->sliceSize))?1:0;
    tb->sliceCtlFactory[i].output_byte_stream=cml->byteStream?1:0;
    tb->sliceCtlFactory[i].outStreamFile=tb->out;
    tb->sliceCtlFactory[i].streamPos = 0;
  }
}

void InitStreamSegmentCrl(struct test_bench *tb, struct commandLine_s *cml)
{
  tb->streamSegCtl.streamRDCounter = 0;
  tb->streamSegCtl.streamMultiSegEn = cml->streamMultiSegmentMode != 0;
  tb->streamSegCtl.streamBase = (u8 *)tb->outbufMemFactory[0][0].virtualAddress;

  if(tb->streamSegCtl.streamMultiSegEn)
  {
      tb->streamSegCtl.segmentSize = tb->outbufMemFactory[0][0].size / cml->streamMultiSegmentAmount;
      tb->streamSegCtl.segmentSize = ((tb->streamSegCtl.segmentSize + 16 - 1) & (~(16 - 1)));//segment size must be aligned to 16byte
      tb->streamSegCtl.segmentAmount = cml->streamMultiSegmentAmount;
  }
  tb->streamSegCtl.startCodeDone = 0;
  tb->streamSegCtl.output_byte_stream = tb->byteStream;
  tb->streamSegCtl.outStreamFile = tb->out;
}

void SetupSliceCtl(struct test_bench *tb)
{
    //find transform buffer of multi-cores
    tb->sliceCtl = &(tb->sliceCtlFactory[tb->picture_enc_cnt % tb->parallelCoreNum]);
    tb->sliceCtlOut = &(tb->sliceCtlFactory[(tb->picture_enc_cnt+1) % tb->parallelCoreNum]);
}

// Helper function to calculate time diffs.
unsigned int uTimeDiff(struct timeval end, struct timeval start)
{
   return (end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec);
}

