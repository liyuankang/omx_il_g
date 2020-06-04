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
--  Abstract : Jpeg Encoder testbench
--
------------------------------------------------------------------------------*/

/* For parameter parsing */
#include "EncGetOption.h"
#include "JpegTestBench.h"

/* For SW/HW shared memory allocation */
#include "ewl.h"

/* For accessing the EWL instance inside the encoder */
//#include "EncJpegInstance.h"

/* For compiler flags, test data, debug and tracing */
#include "enccommon.h"

/* For Hantro Jpeg encoder */
#include "jpegencapi.h"
#include "mjpegencapi.h"


/* For printing and file IO */
#include <stdio.h>
#include <stddef.h>

/* For dynamic memory allocation */
#include <stdlib.h>

/* For memset, strcpy and strlen */
#include <string.h>

#include <assert.h>

#include <sys/time.h>

#include "encinputlinebuffer.h"

/*--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* User selectable testbench configuration */

/* Define this if you want to save each frame of motion jpeg
 * into frame%d.jpg */
/* #define SEPARATE_FRAME_OUTPUT */

/* Define this if yuv don't want to use debug printf */
/*#define ASIC_WAVE_TRACE_TRIGGER*/

/* Output stream is not written to file. This should be used
   when running performance simulations. */
/*#define NO_OUTPUT_WRITE */

/* Define these if you want to use testbench defined
 * comment header */
/*#define TB_DEFINED_COMMENT */

#define USER_DEFINED_QTABLE 10

#define MOVING_AVERAGE_FRAMES    30

#ifdef _WIN32
#define putw _putw
#endif

/* Global variables */

/* Command line options */

static option_s options[] = {
    {"help", 'H', 0},
    {"inputThumb", 'I', 1},
    {"thumbnail", 'T', 1},
    {"widthThumb", 'K', 1},
    {"heightThumb", 'L', 1},
    {"output", 'o', 1},
    {"firstPic", 'a', 1},
    {"lastPic", 'b', 1},
    {"lumWidthSrc", 'w', 1},
    {"lumHeightSrc", 'h', 1},
    {"width", 'x', 1},
    {"height", 'y', 1},
    {"horOffsetSrc", 'X', 1},
    {"verOffsetSrc", 'Y', 1},
    {"restartInterval", 'R', 1},
    {"qLevel", 'q', 1},
    {"frameType", 'g', 1},
    {"colorConversion", 'v', 1},
    {"rotation", 'G', 1},
    {"codingType", 'p', 1},
    {"codingMode", 'm', 1},
    {"markerType", 't', 1},
    {"units", 'u', 1},
    {"xdensity", 'k', 1},
    {"ydensity", 'l', 1},
    {"write", 'W', 1},
    {"comLength", 'c', 1},
    {"comFile", 'C', 1},
    {"trigger", 'P', 1},
    {"inputLineBufferMode", 'S', 1},
    {"inputLineBufferDepth", 'N', 1},
    {"inputLineBufferAmountPerLoopback", 's', 1},
    {"inputAlignmentExp", 'Q', 1},
    {"input", 'i', 1},
    {"hashtype", 'A', 1},  /* hash frame data, 0--disable, 1--crc32, 2--checksum */
    {"mirror", 'M', 1},
    {"XformCustomerPrivateFormat", 'D', 1},
    {"enableConstChroma", 'd', 1},
    {"constCb", 'e', 1},
    {"constCr", 'f', 1},
    {"lossless", '1', 1},
    {"ptrans", '2', 1},
    {"bitPerSecond", 'B', 1},
    {"mjpeg", 'J', 1},
    {"frameRateNum", 'n', 1},
    {"frameRateDenom", 'r', 1},
    {"qpMin", 'E', 1},
    {"qpMax", 'F', 1},
    {"rcMode", 'V', 1 },
    {"picQpDeltaRange", 'U', 1 },
    {"fixedQP", 'O', 1},
    {"streamBufChain", '0', 1},
    {"streamMultiSegmentMode", '0', 1},
    {"streamMultiSegmentAmount", '0', 1},
    {"dec400TableInput", '0', 1},
    {"overlayEnables", '0', 1},
    {"olInput1", '0', 1},
    {"olFormat1", '0', 1},
    {"olAlpha1", '0', 1},
    {"olWidth1", '0', 1},
    {"olCropWidth1", '0', 1},
    {"olHeight1", '0', 1},
    {"olCropHeight1", '0', 1},
    {"olXoffset1", '0', 1},
    {"olCropXoffset1", '0', 1},
    {"olYoffset1", '0', 1},
    {"olCropYoffset1", '0', 1},
    {"olYStride1", '0', 1},
    {"olUVStride1", '0', 1},
    {"olInput2", '0', 1},
    {"olFormat2", '0', 1},
    {"olAlpha2", '0', 1},
    {"olWidth2", '0', 1},
    {"olCropWidth2", '0', 1},
    {"olHeight2", '0', 1},
    {"olCropHeight2", '0', 1},
    {"olXoffset2", '0', 1},
    {"olCropXoffset2", '0', 1},
    {"olYoffset2", '0', 1},
    {"olCropYoffset2", '0', 1},
    {"olYStride2", '0', 1},
    {"olUVStride2", '0', 1},
    {"olInput3", '0', 1},
    {"olFormat3", '0', 1},
    {"olAlpha3", '0', 1},
    {"olWidth3", '0', 1},
    {"olCropWidth3", '0', 1},
    {"olHeight3", '0', 1},
    {"olCropHeight3", '0', 1},
    {"olXoffset3", '0', 1},
    {"olCropXoffset3", '0', 1},
    {"olYoffset3", '0', 1},
    {"olCropYoffset3", '0', 1},
    {"olYStride3", '0', 1},
    {"olUVStride3", '0', 1},
    {"olInput4", '0', 1},
    {"olFormat4", '0', 1},
    {"olAlpha4", '0', 1},
    {"olWidth4", '0', 1},
    {"olCropWidth4", '0', 1},
    {"olHeight4", '0', 1},
    {"olCropHeight4", '0', 1},
    {"olXoffset4", '0', 1},
    {"olCropXoffset4", '0', 1},
    {"olYoffset4", '0', 1},
    {"olCropYoffset4", '0', 1},
    {"olYStride4", '0', 1},
    {"olUVStride4", '0', 1},
    {"olInput5", '0', 1},
    {"olFormat5", '0', 1},
    {"olAlpha5", '0', 1},
    {"olWidth5", '0', 1},
    {"olCropWidth5", '0', 1},
    {"olHeight5", '0', 1},
    {"olCropHeight5", '0', 1},
    {"olXoffset5", '0', 1},
    {"olCropXoffset5", '0', 1},
    {"olYoffset5", '0', 1},
    {"olCropYoffset5", '0', 1},
    {"olYStride5", '0', 1},
    {"olUVStride5", '0', 1},
    {"olInput6", '0', 1},
    {"olFormat6", '0', 1},
    {"olAlpha6", '0', 1},
    {"olWidth6", '0', 1},
    {"olCropWidth6", '0', 1},
    {"olHeight6", '0', 1},
    {"olCropHeight6", '0', 1},
    {"olXoffset6", '0', 1},
    {"olCropXoffset6", '0', 1},
    {"olYoffset6", '0', 1},
    {"olCropYoffset6", '0', 1},
    {"olYStride6", '0', 1},
    {"olUVStride6", '0', 1},
    {"olInput7", '0', 1},
    {"olFormat7", '0', 1},
    {"olAlpha7", '0', 1},
    {"olWidth7", '0', 1},
    {"olCropWidth7", '0', 1},
    {"olHeight7", '0', 1},
    {"olCropHeight7", '0', 1},
    {"olXoffset7", '0', 1},
    {"olCropXoffset7", '0', 1},
    {"olYoffset7", '0', 1},
    {"olCropYoffset7", '0', 1},
    {"olYStride7", '0', 1},
    {"olUVStride7", '0', 1},
    {"olInput8", '0', 1},
    {"olFormat8", '0', 1},
    {"olAlpha8", '0', 1},
    {"olWidth8", '0', 1},
    {"olCropWidth8", '0', 1},
    {"olHeight8", '0', 1},
    {"olCropHeight8", '0', 1},
    {"olXoffset8", '0', 1},
    {"olCropXoffset8", '0', 1},
    {"olYoffset8", '0', 1},
    {"olCropYoffset8", '0', 1},
    {"olYStride8", '0', 1},
    {"olUVStride8", '0', 1},
    {NULL, 0, 0}
};

typedef struct {
    i32 frame[MOVING_AVERAGE_FRAMES];
    i32 length;
    i32 count;
    i32 pos;
    i32 frameRateNumer;
    i32 frameRateDenom;
} ma_s;


/* SW/HW shared memories for input/output buffers */
EWLLinearMem_t pictureMem;
EWLLinearMem_t dec400CompTblMem;
EWLLinearMem_t outbufMem[MAX_STRM_BUF_NUM];
EWLLinearMem_t overlayMem[MAX_OVERLAY_NUM];


/* Test bench definition of comment header */
#ifdef TB_DEFINED_COMMENT
    /* COM data */
static u32 comLen = 38;
static u8 comment[39] = "This is Hantro's test COM data header.";
#endif

static JpegEncCfg cfg;

static u32 writeOutput = 1;

/* Logic Analyzer trigger point */
i32 trigger_point = -1;

u32 thumbDataLength;
u8 * thumbData = NULL; /* thumbnail data buffer */

/* input mb line buffer struct */
static inputLineBufferCfg inputMbLineBuf;
static SegmentCtl_s streamSegCtl;

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void FreeRes(JpegEncInst enc);
static int AllocRes(commandLine_s * cmdl, JpegEncInst encoder);
static int OpenEncoder(commandLine_s * cml, JpegEncInst * encoder);
static void CloseEncoder(JpegEncInst encoder);
static int ReadPic(u8 * image, i32 width, i32 height, i32 sliceNum,
                   i32 sliceRows, i32 frameNum, char *name, u32 inputMode, u32 alignment);
static int Parameter(i32 argc, char **argv, commandLine_s * ep);
static void Help(void);
static void WriteStrm(FILE * fout, u32 * outbuf, u32 size, u32 endian);
static void writeStrmBufs (FILE *fout, EWLLinearMem_t *bufs, u32 offset, u32 size, u32 invalid_size, u32 endian);
static u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight);

#ifdef SEPARATE_FRAME_OUTPUT
static void WriteFrame(char *filename, u32 * strmbuf, u32 size);
static void writeFrameBufs (char *filename, EWLLinearMem_t *bufs, u32 offset, u32 size);
#endif
static i32 InitInputLineBuffer(inputLineBufferCfg * lineBufCfg, JpegEncCfg * encCfg, JpegEncIn * encIn, JpegEncInst inst);
static void SetInputLineBuffer(inputLineBufferCfg * lineBufCfg, JpegEncCfg * encCfg, JpegEncIn * encIn, JpegEncInst inst, i32 sliceIdx);
static void EncStreamSegmentReady(void *cb_data);
static void InitStreamSegmentCrl(SegmentCtl_s *ctl, commandLine_s *cml, FILE *out, JpegEncIn * encIn);
static void getAlignedPicSizebyFormat(JpegEncFrameType type,u32 width, u32 height, u32 alignment,
                                             u64 *luma_Size,u64 *chroma_Size,u64 *picture_Size);
static i32 JpegReadDEC400Data(JpegEncInst encoder, u8 *compDataBuf, u8 *compTblBuf, u32 inputFormat,u32 src_width,
                               u32 src_height,char *inputDataFile,FILE *dec400Table,i32 num);

static i32 file_read(FILE *file, u8 *data, u64 seek, size_t size)
{
  if ((file == NULL) || (data == NULL)) return NOK;

  fseeko(file, seek, SEEK_SET);
  if (fread(data, sizeof(u8), size, file) < size)
  {
    if (!feof(file))
    {
      return NOK;
    }
    return NOK;
  }

  return OK;
}

// Helper function to calculate time diffs.
unsigned int uTimeDiff(struct timeval end, struct timeval start)
{
   return (end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec);
}

void JPEGtransYUVtoDHformat(commandLine_s *cml)
{
   u8 *transform_buf = NULL;
   u8 *picture_buf = NULL;
   u32 i,j,k,col,row,p;
   u8 *start_add,*src_addr,*tile_addr,*cb_start_add,*cr_start_add,*cb_tile_add,*cr_tile_add,*cb_src_add,*cr_src_add;
   u32 transform_size,pictureSize;
   u64 seek;
   u8 *lum,*cb,*cr,*out;
   FILE *yuv_out = NULL;
   FILE *yuv_in = NULL;
   u32 mb_total = 0;

   /* Default resolution, try parsing input file name */
   if(cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
   {
       if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
       {
           /* No dimensions found in filename, using default QCIF */
           cml->lumWidthSrc = 176;
           cml->lumHeightSrc = 144;
       }
   }

   if (cml->formatCustomizedType==0)//hevc
       transform_size = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1))) * ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1))) *
                          JpegEncGetBitsPerPixel(cml->frameType) / 8;
   else//h264
    {
       mb_total = ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/16 * ((cml->lumHeightSrc + 16 - 1) & (~(16 - 1)))/16;
       transform_size = mb_total/5*2048+mb_total%5*400;
    }

   out = transform_buf = malloc(transform_size);

   pictureSize = (cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc *
                    JpegEncGetBitsPerPixel(cml->frameType)/8;
   picture_buf = malloc(pictureSize);

   lum = picture_buf;
   cb = lum+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc;
   cr = cb+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc/4;

   if (yuv_out==NULL)
     yuv_out = fopen("trans_to_dahua_format","w");

   if (yuv_in==NULL)
     yuv_in = fopen(cml->input,"r");

   for (p=cml->firstPic;p<=cml->lastPic;p++)
   {
     seek  = ((u64)p) * ((u64)pictureSize);
     if (file_read(yuv_in, lum , seek, pictureSize)) goto end;

     if (cml->formatCustomizedType==0)//hevc format
     {
      printf("transform YUV to DH HEVC\n");
      u32 row_32 = ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1)))/32;
      u32 num32_per_row = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1)))/32;
      //luma
      for (i=0;i<row_32;i++)
      {
       start_add = lum+i*32*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
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
       cb_start_add = cb+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
       cr_start_add = cr+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;

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
         start_add = lum+i*16*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
         cb_start_add = cb+i*8*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
         cr_start_add = cr+i*8*((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)))/2;
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

      u32 output_size = 0;

      if (cml->formatCustomizedType==0)
         output_size = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1))) * ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1))) *
                      JpegEncGetBitsPerPixel(JPEGENC_YUV420_8BIT_DAHUA_HEVC) / 8;
      else
         output_size = mb_total/5*2048+mb_total%5*400;

      for (i=0;i<output_size;i++)
         fwrite((u8 *)(out)+i, sizeof(u8), 1, yuv_out);

      transform_buf = out;

    }
end:
    fclose(yuv_out);
    fclose(yuv_in);
    free(picture_buf);
    free(out);
    strcpy(cml->input,"trans_to_dahua_format");
    cml->lastPic = cml->lastPic-cml->firstPic;
    cml->firstPic = 0;
    if (cml->formatCustomizedType==0)
    {
      cml->frameType = JPEGENC_YUV420_8BIT_DAHUA_HEVC;
      cml->horOffsetSrc = cml->horOffsetSrc&(~(32-1));
      cml->verOffsetSrc = cml->verOffsetSrc&(~(32-1));
      cml->restartInterval = (cml->restartInterval+1)/2*2;
    }
    else
    {
      cml->frameType = JPEGENC_YUV420_8BIT_DAHUA_H264;
      cml->horOffsetSrc = cml->horOffsetSrc&(~(16-1));
      cml->verOffsetSrc = cml->verOffsetSrc&(~(16-1));
    }

}

void JPEGtransToDJformat(commandLine_s *cml)
{
   u8 *transform_buf = NULL;
   u8 *picture_buf = NULL;
   u32 i,j,k,p;
   u32 transform_size = 0;
   u64 pictureSize;
   u64 seek;
   u8 *lum,*cb,*cr,*out;
   FILE *yuv_out = NULL;
   FILE *yuv_in = NULL;


   /* Default resolution, try parsing input file name */
   if(cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
   {
       if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
       {
           /* No dimensions found in filename, using default QCIF */
           cml->lumWidthSrc = 176;
           cml->lumHeightSrc = 144;
       }
   }

   if (cml->formatCustomizedType==2)//422_888
       transform_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;

   out = transform_buf = malloc(transform_size);

   pictureSize = (u64)(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc *
                    JpegEncGetBitsPerPixel(cml->frameType)/8;
   picture_buf = malloc(pictureSize);

   lum = picture_buf;
   cb = lum+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc;
   cr = cb+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc/4;

   if (yuv_out==NULL)
     yuv_out = fopen("trans_to_422_888_format","w");

   if (yuv_in==NULL)
     yuv_in = fopen(cml->input,"r");

   for (p = cml->firstPic; p <= cml->lastPic; p++)
   {
     seek  = ((u64)p) * ((u64)pictureSize);
     if (file_read(yuv_in, lum , seek, pictureSize)) goto end;

     if (cml->formatCustomizedType==2)
     {
       printf("transform YUV 420 to 422 888\n");

       //luma
       for (i = 0; i < cml->lumWidthSrc * cml->lumHeightSrc; i++)
       {
         memcpy(transform_buf++, lum+i, 1);
       }

       //chroma
       for (i = 0; i < cml->lumHeightSrc/2; i++)
       {
         for (j = 0; j < 2; j++)
         {
           for (k = 0; k < cml->lumWidthSrc/2; k++)
           {
             memcpy(transform_buf++, cb+cml->lumWidthSrc/2*i+k, 1);
             memcpy(transform_buf++, cr+cml->lumWidthSrc/2*i+k, 1);
           }
         }
       }
     }

     for (i = 0; i< transform_size; i++)
        fwrite((u8 *)(out)+i, sizeof(u8), 1, yuv_out);

     transform_buf = out;

    }
end:
    fclose(yuv_out);
    fclose(yuv_in);
    free(picture_buf);
    free(out);
    strcpy(cml->input,"trans_to_422_888_format");
    cml->lastPic = cml->lastPic-cml->firstPic;
    cml->firstPic = 0;
    cml->frameType = JPEGENC_YUV422_888;
}

void JPEGtransToCommdataformat(commandLine_s *cml)
{
   u8 *transform_buf = NULL;
   u8 *picture_buf = NULL;
   u32 x,y,ix,iy,i,j,p;
   u64 transform_size = 0;
   u64 pictureSize;
   u64 seek;
   u8 *lum,*cb,*cr;
   FILE *yuv_out = NULL;
   FILE *yuv_in = NULL;
   u32 trans_format = 0;
   u32 byte_per_compt = 0;
   u8 *tile_start_addr,*dst_8_addr = NULL,*cb_start,*cr_start;
   u16 *dst_16_addr = NULL;

   /* Default resolution, try parsing input file name */
   if(cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
   {
       if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
       {
           /* No dimensions found in filename, using default QCIF */
           cml->lumWidthSrc = 176;
           cml->lumHeightSrc = 144;
       }
   }

   if (cml->formatCustomizedType==3)//tile 4x4 8bit
       trans_format = JPEGENC_YUV420_8BIT_TILE_8_8;
   else if (cml->formatCustomizedType==4)//tile 4x4 10bit
       trans_format = JPEGENC_YUV420_10BIT_TILE_8_8;

   getAlignedPicSizebyFormat(trans_format,cml->lumWidthSrc,cml->lumHeightSrc,0,NULL,NULL,&transform_size);

   transform_buf = malloc(transform_size);

   pictureSize = (u64)(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc *
                    JpegEncGetBitsPerPixel(cml->frameType)/8;
   picture_buf = malloc(pictureSize);

   lum = picture_buf;
   cb = lum+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc;
   cr = cb+(cml->lumWidthSrc+15)/16*16 * cml->lumHeightSrc/4;

   if (yuv_out==NULL)
     yuv_out = fopen("trans_to_tile8x8_commdata_format","w");

   if (yuv_in==NULL)
     yuv_in = fopen(cml->input,"r");

   printf("transform YUV to CommData format\n");

   for (p = cml->firstPic; p <= cml->lastPic; p++)
   {
     seek  = ((u64)p) * ((u64)pictureSize);
     if (file_read(yuv_in, lum , seek, pictureSize)) goto end;

     if (cml->formatCustomizedType == 3)
     {
       byte_per_compt = 1;
       dst_8_addr = transform_buf;
     }
     else
     {
       byte_per_compt = 2;
       dst_16_addr = (u16 *)transform_buf;
     }

     u32 orig_stride = cml->lumWidthSrc;

     //luma
     for (y = 0; y < ((cml->lumHeightSrc+7)/8); y++)
       for (x = 0; x < ((cml->lumWidthSrc+7)/8); x++)
       {
        tile_start_addr = lum + 8*x + orig_stride*8*y;

        for (iy = 0; iy < 2; iy++)
          for (ix = 0; ix < 2; ix++)
             for (i = 0; i < 4; i++)
               if (cml->formatCustomizedType == 3)
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

     //chroma
     for (y = 0; y < ((cml->lumHeightSrc/2+3)/4); y++)
     {
      for (x = 0; x < ((cml->lumWidthSrc+15)/16); x++)
        {
         cb_start = cb + 8*x + orig_stride/2*4*y;
         cr_start = cr + 8*x + orig_stride/2*4*y;

         for (i = 0; i < 4; i++)
         {
           for (j = 0; j < 16; j++)
           {
             if (j%2 == 0)
             {
               if (cml->formatCustomizedType == 3)
                 *dst_8_addr++ = *(cb_start + (j%4)/2 + orig_stride/2*(j/4)+i*2);
               else
                 *dst_16_addr++ = *(cb_start + (j%4)/2 + orig_stride/2*(j/4)+i*2)<<8;
             }
             else
             {
               if (cml->formatCustomizedType == 3)
                 *dst_8_addr++ = *(cr_start + (j%4)/2 + orig_stride/2*(j/4)+i*2);
               else
                 *dst_16_addr++ = *(cr_start + (j%4)/2 + orig_stride/2*(j/4)+i*2)<<8;
             }
           }
         }
      }
     }

      if (cml->formatCustomizedType == 3)
        transform_size = dst_8_addr - transform_buf;
      else
        transform_size = (u8 *)dst_16_addr - (u8 *)transform_buf;

      for (i=0;i<transform_size;i++)
        fwrite(transform_buf+i, sizeof(u8), 1, yuv_out);
   }

end:
    fclose(yuv_out);
    fclose(yuv_in);
    free(picture_buf);
    free(transform_buf);
    strcpy(cml->input,"trans_to_tile8x8_commdata_format");
    cml->lastPic = cml->lastPic-cml->firstPic;
    cml->firstPic = 0;
    if (cml->formatCustomizedType == 3)
      cml->frameType = JPEGENC_YUV420_8BIT_TILE_8_8;
    else
      cml->frameType = JPEGENC_YUV420_10BIT_TILE_8_8;
}

void getAlignedPicSizebyFormat(JpegEncFrameType type,u32 width, u32 height, u32 alignment,
                                       u64 *luma_Size,u64 *chroma_Size,u64 *picture_Size)
{
  u32 luma_stride=0, chroma_stride = 0;
  u64 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  JpegEncGetAlignedStride(width,type,&luma_stride,&chroma_stride,alignment);
  switch(type)
  {
    case JPEGENC_YUV420_PLANAR:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_YUV422_INTERLEAVED_UYVY:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
     lumaSize = luma_stride * height;
     chromaSize = 0;
     break;
    case JPEGENC_YUV420_I010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case JPEGENC_YUV420_MS_P010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case JPEGENC_YUV420_8BIT_DAHUA_HEVC:
     lumaSize = luma_stride * height;
     chromaSize = lumaSize/2;
     break;
    case JPEGENC_YUV420_8BIT_DAHUA_H264:
     lumaSize = luma_stride * height * 2* 12/ 8;
     chromaSize = 0;
     break;
    case JPEGENC_YUV422_888:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height;
     break;
    case JPEGENC_YUV420_8BIT_TILE_8_8:
     lumaSize = luma_stride * ((height+7)/8);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case JPEGENC_YUV420_10BIT_TILE_8_8:
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

void getDec400CompTablebyFormat(JpegEncFrameType type,u32 width, u32 height, u32 alignment,
                                       u64 *luma_Size,u64 *chroma_Size,u64 *picture_Size)
{
  u32 luma_stride=0, chroma_stride = 0;
  u64 lumaSize = 0, chromaSize = 0, pictureSize = 0;
  if (alignment == 0)
    alignment = 256;
  JpegEncGetAlignedStride(width,type,&luma_stride,&chroma_stride,alignment);
  switch(type)
  {
    case JPEGENC_YUV420_PLANAR:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8*2;
     break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8;
     break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = 0;
     break;
    case JPEGENC_YUV420_I010:
     lumaSize = STRIDE(luma_stride/alignment*2 * height,8*16)/ 8;
     chromaSize = STRIDE(chroma_stride/alignment*2 * height/2,8*16)/ 8*2;
     break;
    case JPEGENC_YUV420_MS_P010:
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

/*------------------------------------------------------------------------------
    Read overlay input file and set up overlay buffer
------------------------------------------------------------------------------*/
void SetupOverlayBuffer(JpegEncIn * pEncIn, i32 frameNum)
{
  int i,j;
  u8 *lum, *cb, *cr;
  u32 src_width, size_luma;
  u64 seek;
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    pEncIn->busOlLum[i] = 0;
    pEncIn->busOlCb[i] = 0;
    pEncIn->busOlCr[i] = 0;
    pEncIn->overlayEnable[i] = cfg.olEnable[i];
    if(cfg.olEnable[i])
    {
			int num = frameNum % cfg.olFrameNum[i];
      u32 y_stride = cfg.olYStride[i];
      u32 uv_stride = cfg.olUVStride[i];
      pEncIn->busOlLum[i] = overlayMem[i].busAddress;
      lum = (u8*)overlayMem[i].virtualAddress;
      
      switch(cfg.olFormat[i])
      {
        case 0:  //ARGB
          src_width = cfg.olWidth[i] * 4;
          seek = num * (cfg.olHeight[i]*cfg.olWidth[i]*4);
          for(j = 0; j < cfg.olHeight[i]; j++)
          {
            if(file_read(cfg.olFile[i], lum, seek, src_width))
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
          size_luma = cfg.olYStride[i]*cfg.olHeight[i];
          seek = num * ((cfg.olWidth[i]*cfg.olHeight[i]) + (cfg.olWidth[i])*cfg.olHeight[i]/2);
          src_width = cfg.olWidth[i];
          pEncIn->busOlCb[i] = pEncIn->busOlLum[i] + size_luma; //Cb and Cr
          cb = lum + size_luma;
          //Luma
          for(j = 0; j < cfg.olHeight[i];j++)
          {
            if(file_read(cfg.olFile[i], lum, seek, src_width))
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
          for(j = 0; j < (cfg.olHeight[i]/2);j++)
          {
            if(file_read(cfg.olFile[i], cb, seek, src_width))
            {
              //This situation means corrupted input, so do not rolling back
              pEncIn->overlayEnable[i] = 0;
              break;
            }
            seek += src_width;
            cb += uv_stride;
          }
          break;
        case 2: //Bitmat
          break;
        case 3: //YUV420 PLANAR
          break;
        defualt: break;
      }
    }
  }
}

/*------------------------------------------------------------------------------

    main

------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    JpegEncInst encoder;
    JpegEncRet ret;
    JpegEncIn encIn;
    JpegEncOut encOut;
    JpegEncApiVersion encVer;
    JpegEncBuild encBuild;
    int encodeFail = 0;

    FILE *fout = NULL;
    i32 picBytes = 0;
    u32 i=0;
    u32 mjpeg_length = 0,mjpeg_movi_idx = 0;
    IDX_CHUNK *idx = NULL;
    ma_s ma;
    u32 total_bits = 0;
    long numbersquareoferror = 0;
    i32 maxerrorovertarget = 0;
    i32 maxerrorundertarget = 0;
    float sumsquareoferror = 0;
    float averagesquareoferror = 0;
    struct timeval timeFrameStart;
    struct timeval timeFrameEnd;

    commandLine_s cmdl;

    fprintf(stdout,
	    "\n* * * * * * * * * * * * * * * * * * * * *\n\n"
            "      HANTRO JPEG ENCODER TESTBENCH\n"
            "\n* * * * * * * * * * * * * * * * * * * * *\n\n");

    /* Print API and build version numbers */
    encVer = JpegEncGetApiVersion();
    for (i=0; i < EWLGetCoreNum(); i++)
    {
      encBuild = JpegEncGetBuild(i);

      /* Version */
      fprintf(stdout, "JPEG Encoder API v%d.%d\n", encVer.major, encVer.minor);

      fprintf(stdout, "HW ID:  0x%08x\t SW Build: %u.%u.%u\n\n",
              encBuild.hwBuild, encBuild.swBuild / 1000000,
              (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
    }
    if(argc < 2)
    {
        Help();
        exit(0);
    }

    /* Parse command line parameters */
    if(Parameter(argc, argv, &cmdl) != 0)
    {
        fprintf(stderr, "Input parameter error\n");
        return -1;
    }

    if ((cmdl.formatCustomizedType == 0 || cmdl.formatCustomizedType == 1)&&(cmdl.frameType==JPEGENC_YUV420_PLANAR))
    {
      JPEGtransYUVtoDHformat(&cmdl);
    }

    if ((cmdl.formatCustomizedType == 2)&&(cmdl.frameType==JPEGENC_YUV420_PLANAR))
    {
      JPEGtransToDJformat(&cmdl);
    }

    if (((cmdl.formatCustomizedType == 3)||(cmdl.formatCustomizedType == 4))&&(cmdl.frameType==JPEGENC_YUV420_PLANAR))
    {
      JPEGtransToCommdataformat(&cmdl);
    }

    /* Encoder initialization */
    if((ret = OpenEncoder(&cmdl, &encoder)) != 0)
    {
        return -ret;    /* Return positive value for test scripts */
    }

    /* Allocate input and output buffers */
    if(AllocRes(&cmdl, encoder) != 0)
    {
        fprintf(stderr, "Failed to allocate the external resources!\n");
        FreeRes(encoder);
        CloseEncoder(encoder);
        return 1;
    }

    /* Setup encoder input */
    memset (&encIn, 0, sizeof(JpegEncIn));

    for (i = 0; i < (cmdl.streamBufChain ? 2 : 1); i ++)
    {
      encIn.pOutBuf[i] = (u8 *)outbufMem[i].virtualAddress;
      encIn.busOutBuf[i] =     outbufMem[i].busAddress;
      encIn.outBufSize[i] =    outbufMem[i].size;
      printf("encIn.pOutBuf[%d] %p\n", i, encIn.pOutBuf[i]);
    }
    encIn.frameHeader = 1;

    ma.pos = ma.count = 0;
    ma.frameRateNumer = cmdl.frameRateNum;
    ma.frameRateDenom = cmdl.frameRateDenom;

    ma.length = MOVING_AVERAGE_FRAMES;

    {
        i32 slice = 0, sliceRows = 0;
        i32 next = 0, last = 0, picCnt = 0;
        i32 widthSrc, heightSrc;
        char *input,*dec400TblInput;
        u64 lumaSize = 0,chromaSize = 0,dec400LumaTblSize  = 0,dec400ChrTblSize = 0;
        u32 input_alignment = 1<<cmdl.exp_of_input_alignment;
        i32 mcuh = (cmdl.codingMode == 1 ? 8 : 16);
        FILE *dec400Table = NULL;

        /* If no slice mode, the slice equals whole frame */
        if(cmdl.partialCoding == 0)
            sliceRows = cmdl.lumHeightSrc;
        else
            sliceRows = cmdl.restartInterval * mcuh;

        widthSrc = (cmdl.lumWidthSrc+15)/16*16;
        heightSrc = cmdl.lumHeightSrc;

        if(cmdl.frameType == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
        {
          u32 w,h;

          w = (cmdl.lumWidthSrc + 31) & (~31);
          h = (cmdl.lumHeightSrc + 31) & (~31);

          if(cmdl.partialCoding == 0)
            sliceRows = h;
          else
            sliceRows = cmdl.restartInterval * 16;

          widthSrc = w;
          heightSrc = h;
        }
        else if (cmdl.frameType == JPEGENC_YUV420_8BIT_DAHUA_H264)
        {
          if(cmdl.partialCoding == 0)
            sliceRows = (cmdl.lumHeightSrc + 15) & (~15);
          else
            sliceRows = cmdl.restartInterval * 16;
        }

        input = cmdl.input;

        dec400TblInput = cmdl.dec400CompTableinput;
        last = cmdl.lastPic;

        JpegGetLumaSize(encoder, &lumaSize, &dec400LumaTblSize);
        JpegGetChromaSize(encoder, &chromaSize, &dec400ChrTblSize);
        encIn.busLum = pictureMem.busAddress;
        encIn.busCb = encIn.busLum + lumaSize;
        encIn.busCr = encIn.busCb + chromaSize/2;

        encIn.dec400TableBusLum = dec400CompTblMem.busAddress;
        encIn.dec400TableBusCb = encIn.dec400TableBusLum + dec400LumaTblSize;
        encIn.dec400TableBusCr = encIn.dec400TableBusCb + dec400ChrTblSize/2;
        /* Virtual addresses of input picture, used by software encoder */
        encIn.pLum = (u8 *)pictureMem.virtualAddress;
        encIn.pCb = encIn.pLum + lumaSize;
        encIn.pCr = encIn.pCb + chromaSize/2;

        encIn.dec400TablepLum = (u8 *)dec400CompTblMem.virtualAddress;  
        encIn.dec400TablepCb = encIn.dec400TablepLum + dec400LumaTblSize;
        encIn.dec400TablepCr = encIn.dec400TablepCb  + dec400ChrTblSize/2; 
				
        fout = fopen(cmdl.output, "wb");
        if(fout == NULL)
        {
            fprintf(stderr, "Failed to create the output file.\n");
            FreeRes(encoder);
            CloseEncoder(encoder);
            return -1;
        }

        if (cmdl.inputLineBufMode)
        {
            if (InitInputLineBuffer(&inputMbLineBuf, &cfg, &encIn, encoder))
            {
              fprintf(stderr, "Fail to Init Input Line Buffer: virt_addr=%p, bus_addr=%08x\n",
                      inputMbLineBuf.sram, (u32)(inputMbLineBuf.sramBusAddr));
              goto end;
            }
        }

        if (cmdl.streamMultiSegmentMode != 0)
        {
          InitStreamSegmentCrl(&streamSegCtl, &cmdl, fout, &encIn);
        }

        /* Set Full Resolution mode */
        ret = JpegEncSetPictureSize(encoder, &cfg);
	    /* Handle error situation */
        if(ret != JPEGENC_OK)
        {
#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("FAILED. Error code: %i\n", ret);
#endif
            goto end;
        }

        /* Set up overlay input file pointers and frame numbers */
        for(i = 0; i < MAX_OVERLAY_NUM; i++)
        {
          if((cmdl.overlayEnables >> i) & 1)
          {
            u32 frameByte = 1;
            cfg.olFile[i] = fopen(cmdl.olInput[i], "rb");
            if (!cfg.olFile[i])
            {
              goto end;
            }
            //Get number of input overlay frames
            fseeko(cfg.olFile[i], 0, SEEK_END);
            if(cmdl.olFormat[i] == 0)
            {
              frameByte = (cmdl.olWidth[i] * cmdl.olHeight[i])*4;
            }
            else if(cmdl.olFormat[i] == 1)
            {
              frameByte = (cmdl.olWidth[i] * cmdl.olHeight[i]) / 2 * 3;
            }
            else if(cmdl.olFormat[i] == 2)
            {
              frameByte = (cmdl.olWidth[i] * cmdl.olHeight[i]);
            }
            cfg.olFrameNum[i] = ftello(cfg.olFile[i]) / frameByte;
          }
          else
          {
            cfg.olFrameNum[i] = 0;
          }
        }

        /*if mjpeg is enabled, assemble mjpeg container header*/
        if (cmdl.mjpeg == 1)
        {
          mjpeg_length = MjpegEncodeAVIHeader(outbufMem[0].virtualAddress,cfg.codingWidth,cfg.codingHeight,cmdl.frameRateNum, cmdl.frameRateDenom,cmdl.lastPic-cmdl.firstPic+1);
          if(writeOutput)
            WriteStrm(fout, outbufMem[0].virtualAddress, mjpeg_length, 0);

          mjpeg_movi_idx = mjpeg_length+4;
          u32 *output_buf = outbufMem[0].virtualAddress;
          MjpegAVIchunkheader((u8 **)&output_buf,"LIST","movi",0);
          if(writeOutput)
            WriteStrm(fout, outbufMem[0].virtualAddress, 12, 0);

          mjpeg_length += 12;
          idx = malloc((cmdl.lastPic-cmdl.firstPic+1)*sizeof(IDX_CHUNK));
        }

        /* Main encoding loop */
        ret = JPEGENC_FRAME_READY;
        next = cmdl.firstPic;
        while(next <= last &&
              (ret == JPEGENC_FRAME_READY || ret == JPEGENC_OUTPUT_BUFFER_OVERFLOW))
        {
#ifdef SEPARATE_FRAME_OUTPUT
            char framefile[50];
            sprintf(framefile, "frame%d%s.jpg", picCnt, mode == 1 ? "tn" : "");
            remove(framefile);
#endif
#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("Frame %3d started...\n", picCnt);
#endif
            fflush(stdout);

            if (cmdl.mjpeg == 1)
            {
              u32 *output_buf = outbufMem[0].virtualAddress;

              MjpegAVIchunkheader((u8 **)&output_buf,"00dc",NULL,0);
              if(writeOutput)
                WriteStrm(fout, outbufMem[0].virtualAddress, 8, 0);

              mjpeg_length += 8;
              idx[picCnt].offset = mjpeg_length - 8;
            }

            /* Set up overlay input buffer */
            SetupOverlayBuffer(&encIn, next);

            /* Loop until one frame is encoded */
            do
            {
#ifndef NO_INPUT_YUV
                /* Read next slice */
                if(ReadPic
                   ((u8 *) pictureMem.virtualAddress,
                    cmdl.lumWidthSrc, heightSrc, slice,
                    sliceRows, next, input, cmdl.frameType,input_alignment) != 0)
                    break;
                if ((dec400Table = fopen(dec400TblInput, "rb")) == NULL)
                {
                  encIn.dec400Enable = 0;
                }
                else if (JpegReadDEC400Data(encoder, (u8 *) pictureMem.virtualAddress,(u8 *) dec400CompTblMem.virtualAddress,
                    cmdl.frameType,cmdl.lumWidthSrc,cmdl.lumHeightSrc,input,dec400Table,next) == NOK)
                {
                  break;
                }
                else
                  encIn.dec400Enable = 1;
#endif
                if (cmdl.inputLineBufMode)
                    SetInputLineBuffer (&inputMbLineBuf, &cfg, &encIn, encoder, slice);

                //Get overlay slice info
                JpegEncGetOverlaySlice(encoder, &encIn, cmdl.restartInterval, cmdl.partialCoding, slice, sliceRows);

                gettimeofday(&timeFrameStart, 0);
				
                ret = JpegEncEncode(encoder, &encIn, &encOut);

                switch (ret)
                {
                case JPEGENC_RESTART_INTERVAL:

#ifndef ASIC_WAVE_TRACE_TRIGGER
                    printf("Frame %3d restart interval! %6u bytes\n",
                           picCnt, encOut.jfifSize);
                    fflush(stdout);
#endif

                    if(writeOutput)
                        writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize, encOut.invalidBytesInBuf0Tail, 0);
#ifdef SEPARATE_FRAME_OUTPUT
                    if(writeOutput)
                        writeFrameBufs(framefile, outbufMem, 0, encOut.jfifSize);
#endif
                    picBytes += encOut.jfifSize;
                    slice++;    /* Encode next slice */
                    break;

                case JPEGENC_FRAME_READY:
#ifndef ASIC_WAVE_TRACE_TRIGGER
                    printf("Frame %3d ready! %6u bytes\n",
                           picCnt, encOut.jfifSize);
                    fflush(stdout);
#endif
                    gettimeofday(&timeFrameEnd, 0);

                    if(writeOutput)
                    {
                      if (cmdl.streamMultiSegmentMode == 0)
                        writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize, encOut.invalidBytesInBuf0Tail, 0);
                      else
                      {
                        u8 *streamBase = streamSegCtl.streamBase + (streamSegCtl.streamRDCounter % streamSegCtl.segmentAmount) * streamSegCtl.segmentSize;
                        WriteStrm(streamSegCtl.outStreamFile, (u32 *)streamBase, encOut.jfifSize - streamSegCtl.streamRDCounter * streamSegCtl.segmentSize, 0);
                        streamSegCtl.streamRDCounter = 0;
                      }
                    }
#ifdef SEPARATE_FRAME_OUTPUT
                    if(writeOutput)
                        writeFrameBufs(framefile, outbufMem, 0, encOut.jfifSize);
#endif
                    if (cmdl.mjpeg == 1)
                    {
                      picBytes += encOut.jfifSize;
                      mjpeg_length += picBytes;
                      if (mjpeg_length%4!=0)
                      {
                        memset(outbufMem[0].virtualAddress,0,4-(mjpeg_length%4));
                        if(writeOutput)
                            WriteStrm(fout, outbufMem[0].virtualAddress, 4-(mjpeg_length%4),0);

                        mjpeg_length = (mjpeg_length+3)&(~3);
                        picBytes = (picBytes+3)&(~3);
                      }
                      idx[picCnt].length = picBytes;
                      fseek(fout,-(picBytes+4),SEEK_CUR);
                      putw(picBytes,fout);
                      fseek(fout,0,SEEK_END);

                      /*rate control statistic log*/
                      total_bits += picBytes*8;
                      MaAddFrame(&ma, picBytes*8);

                      #ifndef QP_FRACTIONAL_BITS
                        #ifndef CTBRC_STRENGTH 
                        #define QP_FRACTIONAL_BITS  0
                        #else
                        #define QP_FRACTIONAL_BITS  8
                        #endif
                      #endif
                      printf("=== Encoded %i Qp=%d bits=%d TotalBits=%d averagebitrate=%lld HWCycles=%d Time(us %d HW +SW) \n",  picCnt, (JpegGetQpHdr(encoder) >> QP_FRACTIONAL_BITS), picBytes*8, total_bits, ((unsigned long long)total_bits * cmdl.frameRateNum) / ((picCnt+1) * cmdl.frameRateDenom), JpegEncGetPerformance(encoder), uTimeDiff(timeFrameEnd, timeFrameStart));

                      if(cmdl.bitPerSecond!=0&&(picCnt+1)>=ma.length)
                      {
                        numbersquareoferror++;
                        if(maxerrorovertarget<(Ma(&ma)- cmdl.bitPerSecond))
                          maxerrorovertarget=(Ma(&ma)- cmdl.bitPerSecond);
                        if(maxerrorundertarget<(cmdl.bitPerSecond-Ma(&ma)))
                          maxerrorundertarget=(cmdl.bitPerSecond-Ma(&ma));
                        sumsquareoferror+=((float)(ABS(Ma(&ma)- (i32)cmdl.bitPerSecond))*100/cmdl.bitPerSecond);
                        averagesquareoferror=(sumsquareoferror/numbersquareoferror);
                        printf("++++RateControl(movingBitrate=%d MaxOvertarget=%d%% MaxUndertarget=%d%% AveDeviationPerframe=%f%%) \n",Ma(&ma),maxerrorovertarget*100/cmdl.bitPerSecond,maxerrorundertarget*100/cmdl.bitPerSecond,averagesquareoferror);
                      }
                    }
                    else
                      printf("=== Encoded %i bits=%d HWCycles=%d Time(us %d HW +SW) \n",  picCnt, encOut.jfifSize, JpegEncGetPerformance(encoder), uTimeDiff(timeFrameEnd, timeFrameStart));
                    picBytes = 0;
                    slice = 0;

                    break;

                case JPEGENC_OUTPUT_BUFFER_OVERFLOW:

#ifndef ASIC_WAVE_TRACE_TRIGGER
                    printf("Error: Frame %3d lost! Output buffer overflow.\n",
                           picCnt);
#endif

                    /* For debugging
                    if(writeOutput)
                        WriteStrm(fout, outbufMem.virtualAddress,
                                outbufMem.size, 0);*/
                    /* Rewind the file back this picture's bytes
                    fseek(fout, -picBytes, SEEK_CUR);*/
                    picBytes = 0;
                    slice = 0;
                    break;

                default:

#ifndef ASIC_WAVE_TRACE_TRIGGER
                    printf("FAILED. Error code: %i\n", ret);
#endif
                    encodeFail = (int)ret;
                    /* For debugging */
                    if(writeOutput)
                      writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize, encOut.invalidBytesInBuf0Tail, 0);
                    break;
                }
            }
            while(ret == JPEGENC_RESTART_INTERVAL);

            picCnt++;
            next = picCnt + cmdl.firstPic;
        }   /* End of main encoding loop */

    }   /* End of encoding modes */

end:

#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("Release encoder\n");
#endif
    if (cmdl.mjpeg == 1)
    {
     u32 idx_length;
     fseek(fout,mjpeg_movi_idx,SEEK_SET);
     putw(mjpeg_length - mjpeg_movi_idx -4 ,fout);

     idx_length = MjpegEncodeAVIidx(outbufMem[0].virtualAddress,idx,mjpeg_movi_idx+4, (cmdl.lastPic-cmdl.firstPic+1));
     mjpeg_length += idx_length;

     fseek(fout,0,SEEK_END);
     if(writeOutput)
         WriteStrm(fout, outbufMem[0].virtualAddress, idx_length, 0);

     fseek(fout,4,SEEK_SET);
     putw(mjpeg_length-8,fout);
	 if (idx != NULL)
       free(idx);
    }

    /* Free all resources */
    FreeRes(encoder);
    CloseEncoder(encoder);
    if(fout != NULL)
        fclose(fout);

    return encodeFail;
}

/*------------------------------------------------------------------------------

    AllocRes

    Allocation of the physical memories used by both SW and HW:
    the input picture and the output stream buffer.

    NOTE! The implementation uses the EWL instance from the encoder
          for OS independence. This is not recommended in final environment
          because the encoder will release the EWL instance in case of error.
          Instead, the memories should be allocated from the OS the same way
          as inside EWLMallocLinear().

------------------------------------------------------------------------------*/
int AllocRes(commandLine_s * cmdl, JpegEncInst enc)
{
    i32 sliceRows = 0;
    u64 pictureSize;
    u32 streamBufTotalSize;
    u32 headerSize = JPEGENC_STREAM_MIN_BUF0_SIZE;
    i32 i, ret;
    u64 lumaSize,chromaSize,dec400LumaTblSize,dec400ChrTblSize;
    u32 input_alignment = 1<<cmdl->exp_of_input_alignment;
    i32 mcuh = (cmdl->codingMode == 1 ? 8 : 16);
    i32 strmBufNum = cmdl->streamBufChain ? 2 : 1;
    i32 bufSizes[2] = {0, 0};

    const void* ewl_inst = JpegEncGetEwl(enc);

    /* Set slice size and output buffer size
     * For output buffer size, 1 byte/pixel is enough for most images.
     * Some extra is needed for testing purposes (noise input) */

    if(cmdl->partialCoding == 0)
    {
      if(cmdl->frameType == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
        sliceRows = ((cmdl->lumHeightSrc + 32 - 1) & (~(32 - 1)));
      else if(cmdl->frameType == JPEGENC_YUV420_8BIT_DAHUA_H264)
        sliceRows = ((cmdl->lumHeightSrc + mcuh - 1) & (~(mcuh - 1)));
      else
        sliceRows = cmdl->lumHeightSrc;
    }
    else
    {
      sliceRows = cmdl->restartInterval * mcuh;
    }

    streamBufTotalSize = cmdl->width * sliceRows * 2;
    if(cmdl->thumbnail)
      headerSize += thumbDataLength;

    getAlignedPicSizebyFormat(cmdl->frameType,cmdl->lumWidthSrc,sliceRows,input_alignment,&lumaSize,&chromaSize,&pictureSize);
    getDec400CompTablebyFormat(cmdl->frameType,cmdl->lumWidthSrc,cmdl->lumHeightSrc,input_alignment,&dec400LumaTblSize,&dec400ChrTblSize,NULL);
    JpegSetLumaSize(enc, lumaSize, dec400LumaTblSize);//((jpegInstance_s *)enc)->lumaSize = lumaSize;
    JpegSetChromaSize(enc, chromaSize, dec400ChrTblSize);//((jpegInstance_s *)enc)->chromaSize = chromaSize;

    pictureMem.virtualAddress = NULL;
     /* Here we use the EWL instance directly from the encoder
     * because it is the easiest way to allocate the linear memories */
    ret = EWLMallocLinear(ewl_inst, pictureSize, 0, &pictureMem);
    if (ret != EWL_OK)
    {
        fprintf(stderr, "Failed to allocate input picture!\n");
        pictureMem.virtualAddress = NULL;
        return 1;
    }
	
    dec400CompTblMem.virtualAddress = NULL;
    if (dec400LumaTblSize + dec400ChrTblSize > 0)
    {
      ret = EWLMallocLinear(ewl_inst, dec400LumaTblSize + dec400ChrTblSize, 16, &dec400CompTblMem);
      if (ret != EWL_OK)
      {
          fprintf(stderr, "Failed to allocate dec400 compress table!\n");
          dec400CompTblMem.virtualAddress = NULL;        
          return 1;
      }
    }
	
    if (strmBufNum == 1)
        bufSizes[0] = (cmdl->streamMultiSegmentMode != 0 ? streamBufTotalSize/128 : streamBufTotalSize);
    else
    {
        /* set small stream buffer0 to test two stream buffers */
        bufSizes[0] = streamBufTotalSize / 100;
        bufSizes[1] = streamBufTotalSize - bufSizes[0];
    }
    bufSizes[0] += headerSize;
    memset(outbufMem, 0, sizeof(outbufMem));
    for (i = 0; i < strmBufNum; i ++)
    {
      u32 size = bufSizes[i];

      /* For FPGA testing, smaller size maybe specified. */
      /* Max output buffer size is less than 256MB */
      //comment out outbufSize hard limitation for 16K*16K testing
      //size = size < (1024*1024*64) ? size : (1024*1024*64);

      ret = EWLMallocLinear(ewl_inst, size, 0, &outbufMem[i]);
      if (ret != EWL_OK)
      {
          fprintf(stderr, "Failed to allocate output buffer!\n");
          outbufMem[i].virtualAddress = NULL;
          return 1;
      }
    }

		/*Overlay input buffer*/
    for(i = 0; i < 8; i++)
    {
			u32 block_size = 0;
      if((cmdl->overlayEnables >> i)&1)
      {
        switch(cmdl->olFormat[i])
        {
          case 0: //ARGB
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i];
            break;
          case 1: //NV12
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i] + cmdl->olUVStride[i]*cmdl->olHeight[i]/2;
            break;
          case 2: //Bitmap
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i];
            break;
          default: //3
            block_size = 0;
        }
        if(EWLMallocLinear(ewl_inst, block_size, 0, &overlayMem[i]) != EWL_OK)
        {
          overlayMem[i].virtualAddress = NULL;
          return 1;
        }
        memset(overlayMem[i].virtualAddress, 0, block_size);
      }
      else
      {
        overlayMem[i].virtualAddress = NULL;
      }
    }
		

#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("Input %dx%d + %dx%d encoding at %dx%d + %dx%d ",
               cmdl->lumWidthSrcThumb, cmdl->lumHeightSrcThumb,
               cmdl->lumWidthSrc, cmdl->lumHeightSrc,
               cmdl->widthThumb, cmdl->heightThumb, cmdl->width, cmdl->height);

    if(cmdl->partialCoding != 0)
        printf("in slices of %dx%d", cmdl->width, sliceRows);
    printf("\n");
#endif


#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("Input buffer size:          %u bytes\n", pictureMem.size);
    printf("Input buffer bus address:   %p\n", (void *)pictureMem.busAddress);
    printf("Input buffer user address:  %10p\n", pictureMem.virtualAddress);
    for (i = 0; i < strmBufNum; i ++)
    {
      printf("Output buffer%d size:         %u bytes\n", i, outbufMem[i].size);
      printf("Output buffer%d bus address:  %p\n", i, (void *)outbufMem[i].busAddress);
      printf("Output buffer%d user address: %10p\n", i, outbufMem[i].virtualAddress);
    }
#endif

    return 0;
}

/*------------------------------------------------------------------------------

    FreeRes

------------------------------------------------------------------------------*/
void FreeRes(JpegEncInst enc)
{
    i32 i;
    const void* ewl_inst = JpegEncGetEwl(enc);
    
    if(pictureMem.virtualAddress != NULL)
        EWLFreeLinear(ewl_inst, &pictureMem);
    for (i = 0; i < MAX_STRM_BUF_NUM; i ++)
    {
      if(outbufMem[i].virtualAddress != NULL)
          EWLFreeLinear(ewl_inst, &outbufMem[i]);
    }
    if(dec400CompTblMem.virtualAddress != NULL)
        EWLFreeLinear(ewl_inst, &dec400CompTblMem);
    if(thumbData != NULL)
        free(thumbData);
#ifndef TB_DEFINED_COMMENT
    if(cfg.pCom != NULL)
	    free((void *)cfg.pCom);
#endif

    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
			if(overlayMem[i].virtualAddress != NULL)
				EWLFreeLinear(ewl_inst, &overlayMem[i]);
    }
}

/*------------------------------------------------------------------------------

    OpenEncoder

------------------------------------------------------------------------------*/
int OpenEncoder(commandLine_s * cml, JpegEncInst * pEnc)
{
    JpegEncRet ret;

    /* An example of user defined quantization table */
    const u8 qTable[64] = {1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1};

#ifndef TB_DEFINED_COMMENT
    FILE *fileCom = NULL;
#endif

    /* Default resolution, try parsing input file name */
    if(cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
    {
        if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
        {
            /* No dimensions found in filename, using default QCIF */
            cml->lumWidthSrc = 176;
            cml->lumHeightSrc = 144;
        }
    }

    /* Encoder initialization */
    if(cml->width == DEFAULT)
        cml->width = cml->lumWidthSrc;

    if(cml->height == DEFAULT)
        cml->height = cml->lumHeightSrc;

    if (cml->exp_of_input_alignment == 7)
    {
      if (cml->frameType==JPEGENC_YUV420_PLANAR)
        cml->horOffsetSrc = ((cml->horOffsetSrc + 256 - 1) & (~(256 - 1)));
      else
        cml->horOffsetSrc = ((cml->horOffsetSrc + 128 - 1) & (~(128 - 1)));

      if ((cml->lumWidthSrc - cml->horOffsetSrc)< cml->width)
        cml->horOffsetSrc = 0;
    }

    /* overlay controls */
    u32 i;
    for(i = 0; i< MAX_OVERLAY_NUM; i++)
    {
      if((cml->overlayEnables >> i) & 1)
      {
        if(cml->olYStride[i] == 0)
        {
          switch(cml->olFormat[i])
          {
            case 0: //ARGB
              cml->olYStride[i] = cml->olWidth[i] * 4;
              break;
            case 1: //NV12
              cml->olYStride[i] = cml->olWidth[i];
              break;
            case 2: //Bitmap
              cml->olYStride[i] = cml->olWidth[i] / 8;
              break;
            default: break;
          }
        }
        
        if(cml->olUVStride[i] == 0) cml->olUVStride[i] = cml->olYStride[i];

        if(cml->olCropHeight[i] == 0) cml->olCropHeight[i] = cml->olHeight[i];
        if(cml->olCropWidth[i] == 0) cml->olCropWidth[i] = cml->olWidth[i];
        
        if(cml->olWidth[i] == 0 || cml->olHeight[i] == 0 || cml->olCropWidth == 0 || cml->olCropHeight == 0)
        {
          fprintf(stderr, "\nInvalid overlay region %d size\n", i);
          return -1;
        }
        
        if(cml->olCropWidth[i] + cml->olXoffset[i] > cml->width - cml->horOffsetSrc ||
        cml->olCropHeight[i] + cml->olYoffset[i] > cml->height - cml->verOffsetSrc)
        {
          fprintf(stderr, "\nInvalid overlay region %d offset\n", i);
          return -1;
        }

        if(cml->olCropXoffset[i] + cml->olCropWidth[i] > cml->olWidth[i] ||
           cml->olCropYoffset[i] + cml->olCropHeight[i] > cml->olHeight[i])
        {
          fprintf(stderr, "\nInvalid overlay region %d cropping offset\n", i);
          return -1;
        }
        
      }
    }

    /* lossless mode */
    if (cml->predictMode!=0)
    {
        cfg.losslessEn = 1;
        cfg.predictMode = cml->predictMode;
        cfg.ptransValue = cml->ptransValue;
    }
    else
    {
        cfg.losslessEn = 0;
    }

    cfg.rotation = (JpegEncPictureRotation)cml->rotation;
    cfg.inputWidth = (cml->lumWidthSrc + 15) & (~15);   /* API limitation */
    if (cfg.inputWidth != (u32)cml->lumWidthSrc)
        fprintf(stdout, "Warning: Input width must be multiple of 16!\n");
    cfg.inputHeight = cml->lumHeightSrc;

    if(cfg.rotation && cfg.rotation != JPEGENC_ROTATE_180)
    {
        /* full */
        cfg.xOffset = cml->verOffsetSrc;
        cfg.yOffset = cml->horOffsetSrc;

        cfg.codingWidth = cml->height;
        cfg.codingHeight = cml->width;
        cfg.xDensity = cml->ydensity;
        cfg.yDensity = cml->xdensity;
    }
    else
    {
        /* full */
        cfg.xOffset = cml->horOffsetSrc;
        cfg.yOffset = cml->verOffsetSrc;

        cfg.codingWidth = cml->width;
        cfg.codingHeight = cml->height;
        cfg.xDensity = cml->xdensity;
        cfg.yDensity = cml->ydensity;
    }
    cfg.mirror = cml->mirror;

    if (cml->qLevel == USER_DEFINED_QTABLE)
    {
        cfg.qTableLuma = qTable;
        cfg.qTableChroma = qTable;
    }
    else
        cfg.qLevel = cml->qLevel;

    cfg.restartInterval = cml->restartInterval;
    cfg.codingType = (JpegEncCodingType)cml->partialCoding;
    cfg.frameType = (JpegEncFrameType)cml->frameType;
    cfg.unitsType = (JpegEncAppUnitsType)cml->unitsType;
    cfg.markerType = (JpegEncTableMarkerType)cml->markerType;
    cfg.colorConversion.type = (JpegEncColorConversionType)cml->colorConversion;
    if (cfg.colorConversion.type == JPEGENC_RGBTOYUV_USER_DEFINED)
    {
        /* User defined RGB to YCbCr conversion coefficients, scaled by 16-bits */
        cfg.colorConversion.coeffA = 20000;
        cfg.colorConversion.coeffB = 44000;
        cfg.colorConversion.coeffC = 5000;
        cfg.colorConversion.coeffE = 35000;
        cfg.colorConversion.coeffF = 38000;
        cfg.colorConversion.coeffG = 35000;
        cfg.colorConversion.coeffH = 38000;
        cfg.colorConversion.LumaOffset = 0;
    }
    writeOutput = cml->write;
    cfg.codingMode = (JpegEncCodingMode)cml->codingMode;

    /* low latency */
    cfg.inputLineBufEn = (cml->inputLineBufMode>0) ? 1 : 0;
    cfg.inputLineBufLoopBackEn = (cml->inputLineBufMode==1||cml->inputLineBufMode==2) ? 1 : 0;
    cfg.inputLineBufDepth = cml->inputLineBufDepth;
    cfg.amountPerLoopBack = cml->amountPerLoopBack;
    cfg.inputLineBufHwModeEn = (cml->inputLineBufMode==2||cml->inputLineBufMode==4) ? 1 : 0;
    cfg.inputLineBufCbFunc = VCEncInputLineBufDone;
    cfg.inputLineBufCbData = &inputMbLineBuf;
    cfg.hashType = cml->hashtype;

    /*stream multi-segment*/
    cfg.streamMultiSegmentMode = cml->streamMultiSegmentMode;
    cfg.streamMultiSegmentAmount = cml->streamMultiSegmentAmount;
    cfg.streamMultiSegCbFunc = &EncStreamSegmentReady;
    cfg.streamMultiSegCbData = &streamSegCtl;

    /* constant chroma control */
    cfg.constChromaEn = cml->constChromaEn;
    cfg.constCb = cml->constCb;
    cfg.constCr = cml->constCr;

    /* jpeg rc*/
    cfg.targetBitPerSecond = cml->bitPerSecond;
    cfg.frameRateNum = 1;
    cfg.frameRateDenom = 1;

    //framerate valid only when RC enabled
    if (cml->bitPerSecond)
    {
        cfg.frameRateNum = cml->frameRateNum;
        cfg.frameRateDenom = cml->frameRateDenom;
    }
    cfg.qpmin = cml->qpmin;
    cfg.qpmax = cml->qpmax;
    cfg.fixedQP = cml->fixedQP;
    cfg.rcMode   = cml->rcMode;
    cfg.picQpDeltaMax = cml->picQpDeltaMax;
    cfg.picQpDeltaMin = cml->picQpDeltaMin;

    /*stride*/
    cfg.exp_of_input_alignment = cml->exp_of_input_alignment;

    /* overlay control */
    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      cfg.olEnable[i] = (cml->overlayEnables >> i) & 1;
      cfg.olFormat[i] = cml->olFormat[i];
      cfg.olAlpha[i] = cml->olAlpha[i];
      cfg.olWidth[i] = cml->olWidth[i];
      cfg.olCropWidth[i] = cml->olCropWidth[i];
      cfg.olHeight[i] = cml->olHeight[i];
      cfg.olCropHeight[i] = cml->olCropHeight[i];
      cfg.olXoffset[i] = cml->olXoffset[i];
      cfg.olCropXoffset[i] = cml->olCropXoffset[i];
      cfg.olYoffset[i] = cml->olYoffset[i];
      cfg.olCropYoffset[i] = cml->olCropYoffset[i];
      cfg.olYStride[i] = cml->olYStride[i];
      cfg.olUVStride[i] = cml->olUVStride[i];
    }

#ifdef NO_OUTPUT_WRITE
    writeOutput = 0;
#endif

    if(cml->thumbnail < 0 || cml->thumbnail > 3)
    {
        fprintf(stderr, "\nNot valid thumbnail format!");
	    return -1;
    }

    if(cml->thumbnail != 0)
    {
	FILE *fThumb;
         size_t size;
	fThumb = fopen(cml->inputThumb, "rb");
	if(fThumb == NULL)
	{
	    fprintf(stderr, "\nUnable to open Thumbnail file: %s\n", cml->inputThumb);
	    return -1;
	}

	switch(cml->thumbnail)
	{
	    case 1:
		fseek(fThumb,0,SEEK_END);
		thumbDataLength = ftell(fThumb);
		fseek(fThumb,0,SEEK_SET);
		break;
	    case 2:
		thumbDataLength = 3*256 + cml->widthThumb * cml->heightThumb;
		break;
	    case 3:
		thumbDataLength = cml->widthThumb * cml->heightThumb * 3;
		break;
	    default:
		assert(0);
	}

	thumbData = (u8*)malloc(thumbDataLength);
	size = fread(thumbData,1,thumbDataLength, fThumb);
	fclose(fThumb);
    }

/* use either "hard-coded"/testbench COM data or user specific */
#ifdef TB_DEFINED_COMMENT
    cfg.comLength = comLen;
    cfg.pCom = comment;
#else
    cfg.comLength = cml->comLength;

    if(cfg.comLength)
    {
        /* allocate mem for & read comment data */
        cfg.pCom = (u8 *) malloc(cfg.comLength);

        fileCom = fopen(cml->com, "rb");
        if(fileCom == NULL)
        {
            fprintf(stderr, "\nUnable to open COMMENT file: %s\n", cml->com);
            return -1;
        }

        fread((void *)cfg.pCom, 1, cfg.comLength, fileCom);
        fclose(fileCom);
    }

#endif

#ifndef ASIC_WAVE_TRACE_TRIGGER
    fprintf(stdout, "Init config: %dx%d @ x%dy%d => %dx%d   \n",
            cfg.inputWidth, cfg.inputHeight, cfg.xOffset, cfg.yOffset,
            cfg.codingWidth, cfg.codingHeight);

    fprintf(stdout,
            "\n\t**********************************************************\n");
    fprintf(stdout, "\n\t-JPEG: ENCODER CONFIGURATION\n");
    if (cml->qLevel == USER_DEFINED_QTABLE)
    {
        i32 i;
        fprintf(stdout, "JPEG: qTableLuma \t:");
        for (i = 0; i < 64; i++)
            fprintf(stdout, " %d", cfg.qTableLuma[i]);
        fprintf(stdout, "\n");
        fprintf(stdout, "JPEG: qTableChroma \t:");
        for (i = 0; i < 64; i++)
            fprintf(stdout, " %d", cfg.qTableChroma[i]);
        fprintf(stdout, "\n");
    }
    else
        fprintf(stdout, "\t-JPEG: qp \t\t:%d\n", cfg.qLevel);
    fprintf(stdout, "\t-JPEG: inX \t\t:%d\n", cfg.inputWidth);
    fprintf(stdout, "\t-JPEG: inY \t\t:%d\n", cfg.inputHeight);
    fprintf(stdout, "\t-JPEG: outX \t\t:%d\n", cfg.codingWidth);
    fprintf(stdout, "\t-JPEG: outY \t\t:%d\n", cfg.codingHeight);
    fprintf(stdout, "\t-JPEG: rst \t\t:%d\n", cfg.restartInterval);
    fprintf(stdout, "\t-JPEG: xOff \t\t:%d\n", cfg.xOffset);
    fprintf(stdout, "\t-JPEG: yOff \t\t:%d\n", cfg.yOffset);
    fprintf(stdout, "\t-JPEG: frameType \t:%d\n", cfg.frameType);
    fprintf(stdout, "\t-JPEG: colorConversionType :%d\n", cfg.colorConversion.type);
    fprintf(stdout, "\t-JPEG: colorConversionA    :%d\n", cfg.colorConversion.coeffA);
    fprintf(stdout, "\t-JPEG: colorConversionB    :%d\n", cfg.colorConversion.coeffB);
    fprintf(stdout, "\t-JPEG: colorConversionC    :%d\n", cfg.colorConversion.coeffC);
    fprintf(stdout, "\t-JPEG: colorConversionE    :%d\n", cfg.colorConversion.coeffE);
    fprintf(stdout, "\t-JPEG: colorConversionF    :%d\n", cfg.colorConversion.coeffF);
    fprintf(stdout, "\t-JPEG: rotation \t:%d\n", cfg.rotation);
    fprintf(stdout, "\t-JPEG: codingType \t:%d\n", cfg.codingType);
    fprintf(stdout, "\t-JPEG: codingMode \t:%d\n", cfg.codingMode);
    fprintf(stdout, "\t-JPEG: markerType \t:%d\n", cfg.markerType);
    fprintf(stdout, "\t-JPEG: units \t\t:%d\n", cfg.unitsType);
    fprintf(stdout, "\t-JPEG: xDen \t\t:%d\n", cfg.xDensity);
    fprintf(stdout, "\t-JPEG: yDen \t\t:%d\n", cfg.yDensity);


    fprintf(stdout, "\t-JPEG: thumbnail format\t:%d\n", cml->thumbnail);
    fprintf(stdout, "\t-JPEG: Xthumbnail\t:%d\n", cml->widthThumb);
    fprintf(stdout, "\t-JPEG: Ythumbnail\t:%d\n", cml->heightThumb);

    fprintf(stdout, "\t-JPEG: First picture\t:%d\n", cml->firstPic);
    fprintf(stdout, "\t-JPEG: Last picture\t\t:%d\n", cml->lastPic);
    fprintf(stdout, "\t-JPEG: inputLineBufEn \t\t:%d\n", cfg.inputLineBufEn);
    fprintf(stdout, "\t-JPEG: inputLineBufLoopBackEn \t:%d\n", cfg.inputLineBufLoopBackEn);
    fprintf(stdout, "\t-JPEG: inputLineBufHwModeEn \t:%d\n", cfg.inputLineBufHwModeEn);
    fprintf(stdout, "\t-JPEG: inputLineBufDepth \t:%d\n", cfg.inputLineBufDepth);
    fprintf(stdout, "\t-JPEG: amountPerLoopBack \t:%d\n", cfg.amountPerLoopBack);

    fprintf(stdout, "\t-JPEG: streamMultiSegmentMode \t:%d\n", cfg.streamMultiSegmentMode);
    fprintf(stdout, "\t-JPEG: streamMultiSegmentAmount \t:%d\n", cfg.streamMultiSegmentAmount);

    fprintf(stdout, "\t-JPEG: constChromaEn \t:%d\n", cfg.constChromaEn);
    fprintf(stdout, "\t-JPEG: constCb \t:%d\n", cfg.constCb);
    fprintf(stdout, "\t-JPEG: constCr \t:%d\n", cfg.constCr);

#ifdef TB_DEFINED_COMMENT
    fprintf(stdout, "\n\tNOTE! Using comment values defined in testbench!\n");
#else
    fprintf(stdout, "\t-JPEG: comlen \t\t:%d\n", cfg.comLength);
    fprintf(stdout, "\t-JPEG: COM \t\t:%s\n", cfg.pCom);
#endif
    fprintf(stdout,
            "\n\t**********************************************************\n\n");
#endif

    if((ret = JpegEncInit(&cfg, pEnc)) != JPEGENC_OK)
    {
        fprintf(stderr,
                "Failed to initialize the encoder. Error code: %8i\n", ret);
        return (int)ret;
    }

    if(thumbData != NULL)
    {
	JpegEncThumb jpegThumb;
	jpegThumb.format = cml->thumbnail == 1 ? JPEGENC_THUMB_JPEG : cml->thumbnail == 3 ?
				     JPEGENC_THUMB_RGB24 : JPEGENC_THUMB_PALETTE_RGB8;
	jpegThumb.width = cml->widthThumb;
	jpegThumb.height = cml->heightThumb;
	jpegThumb.data = thumbData;
       	jpegThumb.dataLength = thumbDataLength;

	ret = JpegEncSetThumbnail(*pEnc, &jpegThumb );
	if(ret != JPEGENC_OK )
	{
	    fprintf(stderr,
                "Failed to set thumbnail. Error code: %8i\n", ret);
    	    return -1;
    	}
    }
    return 0;
}

/*------------------------------------------------------------------------------

    CloseEncoder

------------------------------------------------------------------------------*/
void CloseEncoder(JpegEncInst encoder)
{
    JpegEncRet ret;

    if((ret = JpegEncRelease(encoder)) != JPEGENC_OK)
    {
        fprintf(stderr,
                "Failed to release the encoder. Error code: %8i\n", ret);
    }
}

/*------------------------------------------------------------------------------

    Parameter

------------------------------------------------------------------------------*/
int ParseDelim(char *optArg, char delim)
{
    i32 i;

    for (i = 0; i < (i32)strlen(optArg); i++)
        if (optArg[i] == delim)
        {
            optArg[i] = 0;
            return i;
        }

    return -1;
}

int Parameter(i32 argc, char **argv, commandLine_s * cml)
{
    i32 ret,i;
    char *optarg;
    argument_s argument;
    int status = 0;

    memset(cml, 0, sizeof(commandLine_s));
    strcpy(cml->input, "input.yuv");
    strcpy(cml->inputThumb, "thumbnail.jpg");
    strcpy(cml->com, "com.txt");
    strcpy(cml->output, "stream.jpg");
    cml->firstPic = 0;
    cml->lastPic = 0;
    cml->lumWidthSrc = DEFAULT;
    cml->lumHeightSrc = DEFAULT;
    cml->width = DEFAULT;
    cml->height = DEFAULT;
    cml->horOffsetSrc = 0;
    cml->verOffsetSrc = 0;
    cml->qLevel = 1;
    cml->restartInterval = 0;
    cml->thumbnail = 0;
    cml->widthThumb = 32;
    cml->heightThumb = 32;
    cml->frameType = 0;
    cml->colorConversion = 0;
    cml->rotation = 0;
    cml->partialCoding = 0;
    cml->codingMode = 0;
    cml->markerType = 0;
    cml->unitsType = 0;
    cml->xdensity = 1;
    cml->ydensity = 1;
    cml->write = 1;
    cml->comLength = 0;
    cml->inputLineBufMode = 0;
    cml->inputLineBufDepth = 1;
    cml->amountPerLoopBack = 0;
    cml->hashtype = 0;
    cml->mirror = 0;
    cml->formatCustomizedType = -1;
    cml->constChromaEn = 0;
    cml->constCb = 0x80;
    cml->constCr = 0x80;
    cml->predictMode = 0;
    cml->ptransValue = 0;
    cml->bitPerSecond =0;
    cml->mjpeg = 0;
    cml->frameRateNum = 30;
    cml->frameRateDenom = 1;
    cml->rcMode        = 1;
    cml->picQpDeltaMin = -2;
    cml->picQpDeltaMax = 3;
    cml->qpmin = 0;
    cml->qpmax = 51;
    cml->fixedQP = -1;
    cml->exp_of_input_alignment = 4;
    cml->streamBufChain = 0;
    cml->streamMultiSegmentMode = 0;
    cml->streamMultiSegmentAmount = 4;
    strcpy(cml->dec400CompTableinput, "dec400CompTableinput.bin");
    /*Overlay*/
    cml->overlayEnables = 0;
    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      strcpy(cml->olInput[i], "olInput.yuv");
      cml->olFormat[i] = 0;
      cml->olAlpha[i] = 0;
      cml->olWidth[i] = 0;
      cml->olHeight[i] = 0;
      cml->olXoffset[i] = 0;
      cml->olYoffset[i] = 0;
      cml->olYStride[i] = 0;
      cml->olUVStride[i] = 0;
    }

    argument.optCnt = 1;
    while((ret = EncGetOption(argc, argv, options, &argument)) != -1)
    {
        if(ret == -2)
        {
            status = -1;
        }
        optarg = argument.optArg;
        switch (argument.shortOpt)
        {
        case 'H':
            Help();
            exit(0);
        case 'i':
            if(strlen(optarg) < MAX_PATH)
            {
                strcpy(cml->input, optarg);
            }
            else
            {
                status = -1;
            }
            break;
        case 'I':
            if(strlen(optarg) < MAX_PATH)
            {
                strcpy(cml->inputThumb, optarg);
            }
            else
            {
                status = -1;
            }
            break;
        case 'o':
            if(strlen(optarg) < MAX_PATH)
            {
                strcpy(cml->output, optarg);
            }
            else
            {
                status = -1;
            }
            break;
        case 'C':
            if(strlen(optarg) < MAX_PATH)
            {
                strcpy(cml->com, optarg);
            }
            else
            {
                status = -1;
            }
            break;
        case 'a':
            cml->firstPic = atoi(optarg);
            break;
        case 'b':
            cml->lastPic = atoi(optarg);
            break;
        case 'x':
            cml->width = atoi(optarg);
            break;
        case 'y':
            cml->height = atoi(optarg);
            break;
        case 'w':
            cml->lumWidthSrc = atoi(optarg);
            break;
        case 'h':
            cml->lumHeightSrc = atoi(optarg);
            break;
        case 'X':
            cml->horOffsetSrc = atoi(optarg);
            break;
        case 'Y':
            cml->verOffsetSrc = atoi(optarg);
            break;
        case 'R':
            cml->restartInterval = atoi(optarg);
            break;
        case 'q':
            cml->qLevel = atoi(optarg);
            break;
        case 'g':
            cml->frameType = atoi(optarg);
            break;
        case 'v':
            cml->colorConversion = atoi(optarg);
            break;
        case 'G':
            cml->rotation = atoi(optarg);
            break;
        case 'p':
            cml->partialCoding = atoi(optarg);
            break;
        case 'm':
            cml->codingMode = atoi(optarg);
            break;
        case 't':
            cml->markerType = atoi(optarg);
            break;
        case 'u':
            cml->unitsType = atoi(optarg);
            break;
        case 'k':
            cml->xdensity = atoi(optarg);
            break;
        case 'l':
            cml->ydensity = atoi(optarg);
            break;
        case 'T':
            cml->thumbnail = atoi(optarg);
            break;
        case 'K':
            cml->widthThumb = atoi(optarg);
            break;
        case 'L':
            cml->heightThumb = atoi(optarg);
            break;
        case 'W':
            cml->write = atoi(optarg);
            break;
        case 'c':
            cml->comLength = atoi(optarg);
            break;
        case 'P':
            trigger_point = atoi(optarg);
            break;
        case 'S':
            cml->inputLineBufMode = atoi(optarg);
            break;
        case 'N':
            cml->inputLineBufDepth = atoi(optarg);
            break;
        case 's':
            cml->amountPerLoopBack = atoi(optarg);
            break;
        case 'A':
            cml->hashtype = atoi(optarg);
            break;
        case 'M':
            cml->mirror = atoi(optarg);
            break;
        case 'D':
            cml->formatCustomizedType = atoi(optarg);
            break;
        case 'd':
            cml->constChromaEn = atoi(optarg);
            break;
        case 'e':
            cml->constCb = atoi(optarg);
            break;
        case 'f':
            cml->constCr = atoi(optarg);
            break;

		case '1':  /* --lossless [n] */
            cml->predictMode = atoi(optarg);
            break;
		case '2':  /* --ptrans [n] */
            cml->ptransValue = atoi(optarg);
            break;
        case 'B':
            cml->bitPerSecond = atoi(optarg);
            break;
        case 'J':
            cml->mjpeg = atoi(optarg);
            break;
        case 'n':
            cml->frameRateNum = atoi(optarg);
            break;
        case 'r':
            cml->frameRateDenom = atoi(optarg);
            break;
        case 'V':
            cml->rcMode = atoi(optarg);
            if((cml->rcMode < 0) || (cml->rcMode > 2))
                status = -1;
            break;
        case 'E':
            cml->qpmin = atoi(optarg);
            break;
        case 'F':
            cml->qpmax = atoi(optarg);
            break;
        case 'U':
            if ((i = ParseDelim(optarg, ':')) == -1) break;
            cml->picQpDeltaMin = atoi(optarg);
            optarg += i + 1;
            cml->picQpDeltaMax = atoi(optarg);

            if((cml->picQpDeltaMin < -10) || (cml->picQpDeltaMin > -1) || (cml->picQpDeltaMax < 1) || (cml->picQpDeltaMax > 10))
                status = -1;
            break;
        case 'O':
            cml->fixedQP = atoi(optarg);
            break;
        case 'Q':
            cml->exp_of_input_alignment = atoi(optarg);
            break;

        case '0':
            /* Check long option */
            if (strcmp(argument.longOpt, "streamBufChain") == 0)
            {
              cml->streamBufChain = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "streamMultiSegmentMode") == 0)
            {
              cml->streamMultiSegmentMode = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "streamMultiSegmentAmount") == 0)
            {
              cml->streamMultiSegmentAmount = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "dec400TableInput") == 0)
            {
              strcpy(cml->dec400CompTableinput, optarg);
              break;
            }

            if (strcmp(argument.longOpt, "overlayEnables") == 0)
            {
              cml->overlayEnables = atoi(optarg);
							break;
            }
            
            if (strcmp(argument.longOpt, "olInput1") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[0], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat1") == 0)
            {
              cml->olFormat[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha1") == 0)
            {
              cml->olAlpha[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth1") == 0)
            {
              cml->olWidth[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth1") == 0)
            {
              cml->olCropWidth[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight1") == 0)
            {
              cml->olHeight[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight1") == 0)
            {
              cml->olCropHeight[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset1") == 0)
            {
              cml->olXoffset[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset1") == 0)
            {
              cml->olCropXoffset[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset1") == 0)
            {
              cml->olYoffset[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset1") == 0)
            {
              cml->olCropYoffset[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride1") == 0)
            {
              cml->olYStride[0] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride1") == 0)
            {
              cml->olUVStride[0] = atoi(optarg);
              break;
            }
            
            if (strcmp(argument.longOpt, "olInput2") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[1], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat2") == 0)
            {
              cml->olFormat[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha2") == 0)
            {
              cml->olAlpha[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth2") == 0)
            {
              cml->olWidth[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth2") == 0)
            {
              cml->olCropWidth[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight2") == 0)
            {
              cml->olHeight[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight2") == 0)
            {
              cml->olCropHeight[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset2") == 0)
            {
              cml->olXoffset[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset2") == 0)
            {
              cml->olCropXoffset[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset2") == 0)
            {
              cml->olYoffset[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset2") == 0)
            {
              cml->olCropYoffset[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride2") == 0)
            {
              cml->olYStride[1] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride2") == 0)
            {
              cml->olUVStride[1] = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "olInput3") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[2], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat3") == 0)
            {
              cml->olFormat[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha3") == 0)
            {
              cml->olAlpha[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth3") == 0)
            {
              cml->olWidth[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth3") == 0)
            {
              cml->olCropWidth[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight3") == 0)
            {
              cml->olHeight[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight3") == 0)
            {
              cml->olCropHeight[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset3") == 0)
            {
              cml->olXoffset[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset3") == 0)
            {
              cml->olCropXoffset[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset3") == 0)
            {
              cml->olYoffset[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset3") == 0)
            {
              cml->olCropYoffset[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride3") == 0)
            {
              cml->olYStride[2] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride3") == 0)
            {
              cml->olUVStride[2] = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "olInput4") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[3], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat4") == 0)
            {
              cml->olFormat[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha4") == 0)
            {
              cml->olAlpha[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth4") == 0)
            {
              cml->olWidth[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth4") == 0)
            {
              cml->olCropWidth[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight4") == 0)
            {
              cml->olHeight[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight4") == 0)
            {
              cml->olCropHeight[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset4") == 0)
            {
              cml->olXoffset[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset4") == 0)
            {
              cml->olCropXoffset[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset4") == 0)
            {
              cml->olYoffset[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset4") == 0)
            {
              cml->olCropYoffset[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride4") == 0)
            {
              cml->olYStride[3] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride4") == 0)
            {
              cml->olUVStride[3] = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "olInput5") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[4], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat5") == 0)
            {
              cml->olFormat[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha5") == 0)
            {
              cml->olAlpha[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth5") == 0)
            {
              cml->olWidth[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth5") == 0)
            {
              cml->olCropWidth[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight5") == 0)
            {
              cml->olHeight[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight5") == 0)
            {
              cml->olCropHeight[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset5") == 0)
            {
              cml->olXoffset[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset5") == 0)
            {
              cml->olCropXoffset[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset5") == 0)
            {
              cml->olYoffset[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset5") == 0)
            {
              cml->olCropYoffset[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride5") == 0)
            {
              cml->olYStride[4] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride5") == 0)
            {
              cml->olUVStride[4] = atoi(optarg);
              break;
            }


            if (strcmp(argument.longOpt, "olInput6") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[5], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat6") == 0)
            {
              cml->olFormat[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha6") == 0)
            {
              cml->olAlpha[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth6") == 0)
            {
              cml->olWidth[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth6") == 0)
            {
              cml->olCropWidth[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight6") == 0)
            {
              cml->olHeight[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight6") == 0)
            {
              cml->olCropHeight[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset6") == 0)
            {
              cml->olXoffset[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset6") == 0)
            {
              cml->olCropXoffset[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset6") == 0)
            {
              cml->olYoffset[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset6") == 0)
            {
              cml->olCropYoffset[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride6") == 0)
            {
              cml->olYStride[5] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride6") == 0)
            {
              cml->olUVStride[5] = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "olInput7") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[6], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat7") == 0)
            {
              cml->olFormat[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha7") == 0)
            {
              cml->olAlpha[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth7") == 0)
            {
              cml->olWidth[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth7") == 0)
            {
              cml->olCropWidth[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight7") == 0)
            {
              cml->olHeight[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight7") == 0)
            {
              cml->olCropHeight[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset7") == 0)
            {
              cml->olXoffset[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset7") == 0)
            {
              cml->olCropXoffset[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset7") == 0)
            {
              cml->olYoffset[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset7") == 0)
            {
              cml->olCropYoffset[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride7") == 0)
            {
              cml->olYStride[6] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride7") == 0)
            {
              cml->olUVStride[6] = atoi(optarg);
              break;
            }

            if (strcmp(argument.longOpt, "olInput8") == 0)
            {
              if(strlen(optarg) < MAX_PATH)
              {
                strcpy(cml->olInput[7], optarg);
              }
              else
              {
                status = -1;
              }
              break;
            }
            if (strcmp(argument.longOpt, "olFormat8") == 0)
            {
              cml->olFormat[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olAlpha8") == 0)
            {
              cml->olAlpha[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olWidth8") == 0)
            {
              cml->olWidth[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropWidth8") == 0)
            {
              cml->olCropWidth[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olHeight8") == 0)
            {
              cml->olHeight[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropHeight8") == 0)
            {
              cml->olCropHeight[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olXoffset8") == 0)
            {
              cml->olXoffset[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropXoffset8") == 0)
            {
              cml->olCropXoffset[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYoffset8") == 0)
            {
              cml->olYoffset[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olCropYoffset8") == 0)
            {
              cml->olCropYoffset[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olYStride8") == 0)
            {
              cml->olYStride[7] = atoi(optarg);
              break;
            }
            if (strcmp(argument.longOpt, "olUVStride8") == 0)
            {
              cml->olUVStride[7] = atoi(optarg);
              break;
            }
        default:
            break;
        }
    }

    return status;
}

/*------------------------------------------------------------------------------

    ReadPic

    Read raw YUV image data from file
    Image is divided into slices, each slice consists of equal amount of
    image rows except for the bottom slice which may be smaller than the
    others. sliceNum is the number of the slice to be read
    and sliceRows is the amount of rows in each slice (or 0 for all rows).

------------------------------------------------------------------------------*/
int ReadPic(u8 * image, i32 width, i32 height, i32 sliceNum, i32 sliceRows,
            i32 frameNum, char *name, u32 inputMode, u32 input_alignment)
{
    FILE *file = NULL;
    u64 frameSize;
    i64 frameOffset;
    u32 luma_stride,chroma_stride;
    i32 sliceLumOffset = 0;
    i32 sliceCbOffset = 0;
    i32 sliceCrOffset = 0;
    u64 sliceLumSize;        /* The size of one slice in bytes */
    u64 sliceChromaSize;     /* The size of one slice in bytes */
    u64 sliceCbSize;
    u64 sliceCrSize;
    i32 sliceLumWidth;       /* Picture line length to be read */
    i32 sliceCbWidth;
    i32 sliceCrWidth;
    i32 i;
    i32 lumRowBpp, cbRowBpp, crRowBpp; /* Bits per pixel for lum,cb,cr */
    i32 total_mb = 0;
    i32 sliceRowsOrg = sliceRows;
    size_t size;
    u32 lumaStrideSrc,chrStrideSrc;


    if(sliceRows == 0)
      sliceRows = height;

    if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
      width = (width + 31) & (~31);

    if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
    {
      frameSize = (i64)width * height * JpegEncGetBitsPerPixel(inputMode)/8;
    }
    else if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_H264)
    {
      total_mb = (width/16) * (height/16);
      frameSize = (i64)total_mb/5*2048 + total_mb%5*400;
    }
    else
      getAlignedPicSizebyFormat(inputMode,(u32)width,(u32)height,0,NULL,NULL,&frameSize);

    JpegEncGetAlignedStride(width,inputMode,&luma_stride,&chroma_stride,input_alignment);

    JpegEncGetAlignedStride(width,inputMode,&lumaStrideSrc,&chrStrideSrc,0);

    switch(inputMode)
    {
    case JPEGENC_YUV420_PLANAR:
        lumRowBpp = 8;
        cbRowBpp = 4;
        crRowBpp = 4;
        break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV422_888:
        lumRowBpp = 8;
        cbRowBpp = 8;
        crRowBpp = 0;
        break;
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_YUV422_INTERLEAVED_UYVY:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
        lumRowBpp = 16;
        cbRowBpp = 0;
        crRowBpp = 0;
        break;
    case JPEGENC_YUV420_I010:
        lumRowBpp = 16;
        cbRowBpp = 8;
        crRowBpp = 8;
        break;
    case JPEGENC_YUV420_MS_P010:
        lumRowBpp = 16;
        cbRowBpp = 16;
        crRowBpp = 0;
        break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    default:
        lumRowBpp = 32;
        cbRowBpp = 0;
        crRowBpp = 0;
        break;
    }

    sliceLumWidth = width*lumRowBpp/8;  /* Luma bytes per input row */
    sliceCbWidth = width*cbRowBpp/8;    /* Cb bytes per input row */
    sliceCrWidth = width*crRowBpp/8;
    /* Size of complete slice in input file */
    sliceLumSize = sliceLumWidth * sliceRows;
    sliceCbSize = sliceCbWidth * sliceRows / 2;
    sliceCrSize = sliceCrWidth * sliceRows / 2;

    /* Offset for frame start from start of file */
    frameOffset = frameSize * frameNum;
    /* Offset for slice luma start from start of frame */
    sliceLumOffset = sliceLumSize * sliceNum;
    /* Offset for slice cb start from start of frame */
    if(sliceCbSize)
        sliceCbOffset = width * height*(lumRowBpp/8) + sliceCbSize * sliceNum;
    /* Offset for slice cr start from start of frame */
    if(sliceCrSize)
        sliceCrOffset = width * height*(lumRowBpp/8) +
                        width/2 * height/2*(lumRowBpp/8) + sliceCrSize * sliceNum;

    /* Size of completed slice*/
    getAlignedPicSizebyFormat(inputMode,(u32)width,(u32)sliceRows,input_alignment,&sliceLumSize,&sliceChromaSize,NULL);

    if (inputMode == JPEGENC_YUV420_PLANAR || inputMode == JPEGENC_YUV420_I010 || inputMode == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
    {
      sliceCbSize = sliceChromaSize / 2;
      sliceCrSize = sliceChromaSize / 2;
    }
    else
    {
      sliceCbSize = sliceChromaSize;
      sliceCrSize = 0;
    }

    /* The bottom slice may be smaller than the others */
    if(sliceRows * (sliceNum + 1) > height)
      sliceRows = height - sliceRows * sliceNum;

    if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
    {
      sliceLumOffset = width*sliceRowsOrg*sliceNum;
      sliceCbOffset = width * height+width*sliceRowsOrg*sliceNum/2;
    }
    else if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_H264)
    {
      total_mb = (width/16) * sliceRowsOrg/16*sliceNum;
      sliceLumOffset = total_mb/5*2048 + total_mb%5*400;
    }
    else if(inputMode == JPEGENC_YUV422_888)
    {
      sliceLumOffset = width*sliceRowsOrg*sliceNum;
      sliceCbOffset = width * height+width*sliceRowsOrg*sliceNum;
    }
    else if((inputMode == JPEGENC_YUV420_8BIT_TILE_8_8) || (inputMode == JPEGENC_YUV420_10BIT_TILE_8_8))
    {
      sliceLumOffset = lumaStrideSrc * (sliceRowsOrg/8)*sliceNum;
      sliceCbOffset = lumaStrideSrc * ((height+7)/8) + chrStrideSrc* (((sliceRowsOrg/2)+3)/4)*sliceNum;
    }

    if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_HEVC)
    {
      file = fopen(name, "rb");
      if(file == NULL)
      {
          fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
          return -1;
      }
      fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
      size = fread(image, 1, width*sliceRows, file);
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
      size = fread(image+width*sliceRows, 1, width*sliceRows/2, file);
      goto error;
    }
    else if(inputMode == JPEGENC_YUV420_8BIT_DAHUA_H264)
    {
      file = fopen(name, "rb");
      if(file == NULL)
      {
          fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
          return -1;
      }
      fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
      size = fread(image, 1, width*sliceRows*2*3/2, file);
      goto end;
    }
    else if(inputMode == JPEGENC_YUV420_8BIT_TILE_8_8 || inputMode == JPEGENC_YUV420_10BIT_TILE_8_8)
    {
      file = fopen(name, "rb");
      if(file == NULL)
      {
          fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
          return -1;
      }

      //luma
      fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
      for(i = 0; i < ((sliceRows+7)/8); i++)
        size = fread(image + i*luma_stride, 1, lumaStrideSrc, file);

      sliceLumSize = luma_stride * ((sliceRows+7)/8);

      //chroma
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
      for(i = 0; i < (((sliceRows/2)+3)/4); i++)
        size = fread(image + sliceLumSize + i*chroma_stride, 1, chrStrideSrc, file);

      goto end;
    }

    /* Read input from file frame by frame */
#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("Reading frame %d slice %d (%d bytes) from %s... ",
           frameNum, sliceNum,
           sliceLumWidth*sliceRows + sliceCbWidth*sliceRows/2 +
           sliceCrWidth*sliceRows/2, name);
    fflush(stdout);
#endif

    file = fopen(name, "rb");
    if(file == NULL)
    {
        fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
        return -1;
    }

    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);

    i64 scan = luma_stride;

    for(i = 0; i < sliceRows; i++)
        size = fread(image+scan*i, 1, sliceLumWidth, file);

    //cb
    if (sliceCbSize)
    {
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);

      scan = chroma_stride;

      if (inputMode == JPEGENC_YUV422_888)
      {
        for(i = 0; i < sliceRows; i++)
          size = fread(image + sliceLumSize + scan*i, 1, sliceCbWidth, file);
      }
      else
      {
        for(i = 0; i < sliceRows/2; i++)
          size = fread(image + sliceLumSize + scan*i, 1, sliceCbWidth, file);
      }
    }

    //cr
    if (sliceCrSize)
    {
      fseek(file, frameOffset + sliceCrOffset, SEEK_SET);

      scan = chroma_stride;

      for(i = 0; i < sliceRows/2; i++)
          size = fread(image + sliceLumSize + sliceCbSize + scan*i, 1, sliceCrWidth, file);
    }
error:
    /* Stop if last VOP of the file */
    if(feof(file)) {
        fprintf(stderr, "\nI can't read VOP no: %d ", frameNum);
        fprintf(stderr, "from file: %s\n", name);
        fclose(file);
        return -1;
    }
end:

#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("OK\n");
    fflush(stdout);
#endif

    fclose(file);

    return 0;
}
i32 JpegReadDEC400Data(JpegEncInst encoder, u8 *compDataBuf, u8 *compTblBuf, u32 inputFormat,u32 src_width,
                               u32 src_height,char *inputDataFile,FILE *dec400Table,i32 num)
{
  u64 seek;
  u8 *lum;
  u8 *cb;
  u8 *cr;
  u64 lumaSize,chrSize;
  u64 src_img_size;
  u64 SrcLumaSize = 0,srcChrSize = 0,dec400LumaTblSize  = 0,dec400ChrTblSize = 0;
  FILE *dec400Data = NULL;
  dec400Data = fopen(inputDataFile, "rb");
  if(dec400Data == NULL)
  {
    return NOK;
  }
  JpegGetLumaSize(encoder, &SrcLumaSize, &dec400LumaTblSize);
  JpegGetChromaSize(encoder, &srcChrSize, &dec400ChrTblSize);
  lumaSize = dec400LumaTblSize;
  chrSize = dec400ChrTblSize;
  src_img_size = lumaSize + chrSize;
  seek     = ((u64)num) * ((u64)src_img_size);
  lum = compTblBuf;
  cb = lum + lumaSize;
  cr = cb + chrSize/2;
  switch(inputFormat)
  {
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YUV420_I010:
     if (file_read(dec400Table, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(dec400Table, cb , seek, chrSize/2)) return NOK;
     seek += chrSize/2;
     if (file_read(dec400Table, cr , seek, chrSize/2)) return NOK;
     break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
     if (file_read(dec400Table, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(dec400Table, cb , seek, chrSize)) return NOK;
     break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
     if (file_read(dec400Table, lum , seek, lumaSize)) return NOK;
     break;
    default:
     printf("DEC400 not support this format\n");
     return NOK;
  }
  lumaSize = SrcLumaSize;
  chrSize = srcChrSize;
  src_img_size = lumaSize + chrSize;
  seek     = ((u64)num) * ((u64)src_img_size);
  lum = compDataBuf;
  cb = lum + lumaSize;
  cr = cb + chrSize/2;
  switch(inputFormat)
  {
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YUV420_I010:
     if (file_read(dec400Data, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(dec400Data, cb , seek, chrSize/2)) return NOK;
     seek += chrSize/2;
     if (file_read(dec400Data, cr , seek, chrSize/2)) return NOK;
     break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
     if (file_read(dec400Data, lum , seek, lumaSize)) return NOK;
     seek += lumaSize;
     if (file_read(dec400Data, cb , seek, chrSize)) return NOK;
     break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
     if (file_read(dec400Data, lum , seek, lumaSize)) return NOK;
     break;
    default:
     printf("DEC400 not support this format\n");
     return NOK;
  }
  return OK;
}

/*------------------------------------------------------------------------------

    Help

------------------------------------------------------------------------------*/
void Help(void)
{
    fprintf(stdout, "Usage:  %s [options] -i inputfile\n", "jpeg_testenc");
    fprintf(stdout,
            "  -H    --help              Display this help.\n\n"
           );

    fprintf(stdout,
            " Parameters affecting input frame and encoded frame resolutions and cropping:\n"
            "  -i[s] --input             Read input from file. [input.yuv]\n"
            "  -I[s] --inputThumb        Read thumbnail input from file. [thumbnail.jpg]\n"
            "  -o[s] --output            Write output to file. [stream.jpg]\n"
            "  -a[n] --firstPic          First picture of input file. [0]\n"
            "  -b[n] --lastPic           Last picture of input file. [0]\n"
            "  -w[n] --lumWidthSrc       Width of source image. [176]\n"
            "  -h[n] --lumHeightSrc      Height of source image. [144]\n"
            "  -x[n] --width             Width of output image. [--lumWidthSrc]\n"
            "  -y[n] --height            Height of output image. [--lumHeightSrc]\n"
            "  -X[n] --horOffsetSrc      Output image horizontal offset. [0]\n"
            "  -Y[n] --verOffsetSrc      Output image vertical offset. [0]\n"
            "  -W[n] --write             0=NO, 1=YES write output. [1]\n"
                );

    fprintf(stdout,
            "\n Parameters for pre-processing frames before encoding:\n"
            "  -g[n] --frameType         Input YUV format. [0]\n"
            "                               0 - YUV420 planar CbCr (IYUV/I420)\n"
            "                               1 - YUV420 semi-planar CbCr (NV12)\n"
            "                               2 - YUV420 semi-planar CrCb (NV21)\n"
            "                               3 - YUYV422 interleaved (YUYV/YUY2)\n"
            "                               4 - UYVY422 interleaved (UYVY/Y422)\n"
            "                               5 - RGB565 16bpp\n"
            "                               6 - BGR565 16bpp\n"
            "                               7 - RGB555 16bpp\n"
            "                               8 - BGR555 16bpp\n"
            "                               9 - RGB444 16bpp\n"
            "                               10 - BGR444 16bpp\n"
            "                               11 - RGB888 32bpp\n"
            "                               12 - BGR888 32bpp\n"
            "                               13 - RGB101010 32bpp\n"
            "                               15 - YUV420 10 bit planar CbCr (I010)\n"
            "                               16 - YUV420 10 bit planar CbCr (P010)\n"
            "                               19 - YUV420 customer private tile for HEVC\n"
            "                               20 - YUV420 customer private tile for H.264\n"
            "  -v[n] --colorConversion   RGB to YCbCr color conversion type. [0]\n"
            "                               0 - ITU-R BT.601, RGB limited [16...235] (BT601_l.mat)\n"
            "                               1 - ITU-R BT.709, RGB limited [16...235] (BT709_l.mat)\n"
            "                               2 - User defined, coefficients defined in test bench.\n"
            "                               3 - ITU-R BT.2020\n"
            "                               4 - ITU-R BT.601, RGB full [0...255] (BT601_f.mat)\n"
            "                               5 - ITU-R BT.601, RGB limited [0...219] (BT601_219.mat)\n"
            "                               6 - ITU-R BT.709, RGB full [0...255] (BT709_f.mat)\n"
            "  -G[n] --rotation          Rotate input image. [0]\n"
            "                               0 - disabled\n"
            "                               1 - 90 degrees right\n"
            "                               2 - 90 degrees left\n"
            "                               3 - 180 degrees\n"
            "  -M[n]  --mirror           Mirror input image. [0]\n"
            "                               0 - disabled horizontal mirror\n"
            "                               1 - enable horizontal mirror\n"
            "  -Q[n]  --inputAlignmentExp Alignment value of input frame buffer. [4]\n"
            "                               0 = Disable alignment \n"
            "                               4..12 = Base address of input frame buffer and each line are aligned to 2^inputAlignmentExp \n"
        );
    fprintf(stdout,
            "  -d[n] --enableConstChroma   0..1 Enable/Disable setting chroma to a constant pixel value. [0]\n"
            "                                 0 = Disable. \n"
            "                                 1 = Enable. \n"
            "  -e[n] --constCb             0..255. The constant pixel value for Cb. [128]\n"
            "  -f[n] --constCr             0..255. The constant pixel value for Cr. [128]\n"
        );

    fprintf(stdout,
            "\n Parameters affecting the output stream and encoding tools:\n"
            "  -R[n] --restartInterval   Restart interval in MCU rows. [0]\n"
            "  -q[n] --qLevel            0..10, quantization scale. [1]\n"
            "                            10 = use testbench defined qtable\n"
            "  -p[n] --codingType        0=whole frame, 1=partial frame encoding. [0]\n"
            "  -m[n] --codingMode        0=YUV420, 1=YUV422, 2=Monochrome [0]\n"
            "  -t[n] --markerType        Quantization/Huffman table markers. [0]\n"
            "                               0 = Single marker\n"
            "                               1 = Multiple markers\n"
            "  -u[n] --units             Units type of x- and y-density. [0]\n"
            "                               0 = pixel aspect ratio\n"
            "                               1 = dots/inch\n"
            "                               2 = dots/cm\n"
            "  -k[n] --xdensity          Xdensity to APP0 header. [1]\n"
            "  -l[n] --ydensity          Ydensity to APP0 header. [1]\n"
            "        --streamBufChain    Enable two output stream buffers. [0]\n"
            "                               0 - Single output stream buffer.\n"
            "                               1 - Two output stream buffers chained together.\n"
            "                            Note the minimum allowable size of the first stream buffer is 1k bytes + thumbnail data size if any.\n"

            );
    fprintf(stdout,
            "  -T[n] --thumbnail         0=NO, 1=JPEG, 2=RGB8, 3=RGB24 Thumbnail to stream. [0]\n"
            "  -K[n] --widthThumb        Width of thumbnail output image. [32]\n"
            "  -L[n] --heightThumb       Height of thumbnail output image. [32]\n"
            );

    fprintf(stdout,
            "        --hashtype          Hash type for frame data hash. [0]\n"
            "                                 0=disable, 1=CRC32, 2=checksum32. \n"
         );

    fprintf(stdout,
            "\n Parameters affecting stream multi-segment output:\n"
            "        --streamMultiSegmentMode 0..2 Stream multi-segment mode control. [0]\n"
            "                                 0 = Disable stream multi-segment.\n"
            "                                 1 = Enable. No SW handshaking. Loop-back enabled.\n"
            "                                 2 = Enable. SW handshaking. Loop-back enabled.\n"
            "        --streamMultiSegmentAmount 2..16. the total amount of segments to control loopback/sw-handshake/IRQ. [4]\n"
        );

    fprintf(stdout,
            "\n");
    fprintf(stdout,
            "\n Parameters affecting lossless encoding:\n"
            "        --lossless          0=lossy, 1~7 Enalbe lossless with prediction select mode n [0]\n"
            "        --ptrans            0..7 Point transform value for lossless encoding. [0]\n");
#ifdef TB_DEFINED_COMMENT
    fprintf(stdout,
            "\n   Using comment values defined in testbench!\n");
#else
    fprintf(stdout,
            "  -c[n] --comLength         Comment header data length. [0]\n"
            "  -C[s] --comFile           Comment header data file. [com.txt]\n");
#endif
    fprintf(stdout,
           "\n Parameters affecting the low latency mode:\n"
            "  -S[n] --inputLineBufferMode 0..4. Input buffer mode control (Line-Buffer Mode). [0]\n"
            "                                 0 = Disable input line buffer. \n"
            "                                 1 = Enable. SW handshaking. Loopback enabled.\n"
            "                                 2 = Enable. HW handshaking. Loopback enabled.\n"
            "                                 3 = Enable. SW handshaking. Loopback disabled.\n"
            "                                 4 = Enable. HW handshaking. Loopback disabled.\n"
            "  -N[n] --inputLineBufferDepth 0..511 The number of MCU rows to control loop-back/handshaking [1]\n"
            "                                 Control loop-back mode if it is enabled:\n"
            "                                   There are two continuous ping-pong input buffers; each contains inputLineBufferDepth MCU rows.\n"
            "                                 Control hardware handshaking if it is enabled:\n"
            "                                   Handshaking signal is processed per inputLineBufferDepth CTB/MB rows.\n"
            "                                 Control software handshaking if it is enabled:\n"
            "                                   IRQ is sent and Read Count Register is updated every time inputLineBufferDepth MCU rows have been read.\n"
            "                                 0 is only allowed with inputLineBufferMode = 3, IRQ won't be sent and Read Count Register won't be updated.\n"
            "  -s[n] --inputLineBufferAmountPerLoopback 0..1023. Handshake sync amount for every loopback [0]\n");

    fprintf(stdout,
    "\n Parameters affecting Motion JPEG, rate control, JPEG bitsPerPic:\n"
            "  -J[n] --mjpeg             Enable Motion JPEG[0]\n"
            "                               0 = Disable motion JPEG.\n"
            "                               1 = Enable motion JPEG.\n"
            "  -B[n] --bitPerSecond      Target bit per second. [0]\n"
            "                               0 - RC OFF\n"
            "                               none zero - RC ON\n"
            "  -n[n] --frameRateNum      1..1048575 Output picture rate numerator. [30]\n"
            "  -r[n] --frameRateDenom    1..1048575 Output picture rate denominator. [1]\n"
            "  -V[n] --rcMode            0..2, JPEG/MJPEG RC mode. [1]\n"
            "                               0 = single frame RC mode. \n"
            "                               1 = video RC with CBR. \n"
            "                               2 = video RC with VBR. \n"
            "  -U[n:m] --picQpDeltaRange Min:Max. Qp Delta range in picture-level rate control.\n"
            "                               Min: -10..-1 Minimum Qp_Delta in picture RC. [-2]\n"
            "                               Max:  1..10  Maximum Qp_Delta in picture RC. [3]\n"
            "                               This range only applies to two neighboring frames.\n"
            "  -E[n] --qpMin             0..51, Minimum frame qp. [0]\n"
            "  -F[n] --qpMax             0..51, Maxmum frame qp. [51]\n"
            "  -O[n] --fixedQP           -1..51, Fixed qp for every frame. [-1]\n"
            "                               -1 = disable fixed qp mode\n"
            "                               0-51 = value of fixed qp.\n"
           );
		   
	fprintf(stdout,
            "\n Parameters for DEC400 compressed table(tile status):\n"
            "  --dec400TableInput            Read input DEC400 compressed table from file. [dec400CompTableinput.bin]\n"
            );
	  	   
    fprintf(stdout,
            "\nTesting parameters that are not supported for end-user:\n"
            "  -P[n] --trigger           Logic Analyzer trigger at picture <n>. [-1]\n"
            "                            -1 = Disable the trigger.\n"
            "  -D[n] --XformCustomerPrivateFormat    -1..4 Convert YUV420 to customer private format. [-1]\n"
            "                               -1 - No conversion to customer private format\n"
            "                               0 - customer private tile format for HEVC\n"
            "                               1 - customer private tile format for H.264\n"
            "                               2 - customer private YUV422_888\n"
            "                               3 - common data 8-bit tile 4x4\n"
            "                               4 - common data 10-bit tile 4x4\n"
            "\n");

    fprintf(stdout,
            "\n Parameters for OSD overlay controls (i should be a number from 1 to 8):\n"
            "  --overlayEnables             8 bits indicate enable for 8 overlay region. [0]\n"
            "                                   1: region 1 enabled\n"
            "                                   2: region 2 enabled\n"
            "                                   3: region 1 and 2 enabled\n"
            "                                   and so on.\n"
            "  --olInputi                   input file for overlay region 1-8. [olInputi.yuv]\n"
            "                                   for example --olInput1\n"
            "  --olFormati                  0..1 Specify the overlay input format. [0]\n"
            "                                   0: ARGB8888\n"
            "                                   1: NV12\n"
            "  --olAlphai                   0..255 Specify a global alpha value for NV12 overlay format. [0]\n"
            "  --olWidthi                   Width of overlay region. Must be set if region enabled. [0]\n"
            "  --olHeighti                  Height of overlay region. Must be set if region enabled. [0]\n"
            "  --olXoffseti                 Horizontal offset of overlay region top left pixel. [0]\n"
            "                                   must be under 2 aligned condition. [0]\n"
            "  --olYoffseti                 Vertical offset of overlay region top left pixel. [0]\n"
            "                                   must be under 2 aligned condition. [0]\n"
            "  --olYStridei                 Luma stride in bytes. Default value is based on format.\n"
            "                                   [olWidthi * 4] if ARGB888.\n"
            "                                   [olWidthi] if NV12.\n"
            "  --olUVStridei                Chroma stride in bytes. Default value is based on luma stride.\n"
            "  --olCropXoffseti             OSD cropping top left horizontal offset. [0]\n"
            "                                   must be under 2 aligned condition. [0]\n"
            "  --olCropYoffseti             OSD cropping top left vertical offset. [0]\n"
            "                                   must be under 2 aligned condition. [0]\n"
            "  --olCropWidthi               OSD cropping width. [olWidthi]\n"
            "  --olCropHeighti              OSD cropping height. [olHeighti]\n"
            "\n");
}

/*------------------------------------------------------------------------------

    Write encoded stream to file

------------------------------------------------------------------------------*/
void WriteStrm(FILE * fout, u32 * strmbuf, u32 size, u32 endian)
{

    /* Swap the stream endianess before writing to file if needed */
    if(endian == 1)
    {
        u32 i = 0, words = (size + 3) / 4;

        while(words)
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

    /* Write the stream to file */

#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("Writing stream (%i bytes)... ", size);
    fflush(stdout);
#endif

    fwrite(strmbuf, 1, size, fout);

#ifndef ASIC_WAVE_TRACE_TRIGGER
    printf("OK\n");
    fflush(stdout);
#endif
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
void writeStrmBufs (FILE *fout, EWLLinearMem_t *bufs, u32 offset, u32 size, u32 invalid_size, u32 endian)
{
  u8 *buf0 = (u8 *)bufs[0].virtualAddress;
  u8 *buf1 = (u8 *)bufs[1].virtualAddress;
  u32 buf0Len = bufs[0].size;

  if (!buf0) return;

  if (offset < buf0Len)
  {
    u32 size0 = MIN(size, buf0Len-offset-invalid_size);
    WriteStrm(fout, (u32 *)(buf0 + offset), size0, endian);
    if ((size0 < size) && buf1)
      WriteStrm(fout, (u32 *)buf1, size-size0, endian);
  }
  else if (buf1)
  {
    WriteStrm(fout, (u32 *)(buf1 + offset - buf0Len), size, endian);
  }
}

#ifdef SEPARATE_FRAME_OUTPUT
/*------------------------------------------------------------------------------

    Write encoded frame to file

------------------------------------------------------------------------------*/
void WriteFrame(char *filename, u32 * strmbuf, u32 size)
{
    FILE *fp;

    fp = fopen(filename, "ab");

        if(fp)
        {
            fwrite(strmbuf, 1, size, fp);
            fclose(fp);
        }
}

/*------------------------------------------------------------------------------

    WriteFrameBufs
        Write encoded stream to file

    Params:
        bufs - stream buffers
        offset - stream buffer offset
        size - amount of data to write

------------------------------------------------------------------------------*/
void writeFrameBufs (char *filename, EWLLinearMem_t *bufs, u32 offset, u32 size)
{
  u8 *buf0 = (u8 *)bufs[0].virtualAddress;
  u8 *buf1 = (u8 *)bufs[1].virtualAddress;
  u32 buf0Len = bufs[0].size;

  if (!buf0) return;

  if (offset < buf0Len)
  {
    u32 size0 = MIN(size, buf0Len-offset);
    WriteFrame(filename, (u32 *)(buf0 + offset), size0);
    if ((size0 < size) && buf1)
      WriteFrame(filename, (u32 *)buf1, size-size0);
  }
  else if (buf1)
  {
    WriteFrame(filename, (u32 *)(buf1 + offset - buf0Len), size);
  }
}

#endif

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
    for (i = len-1; i; --i)
        if (filename[i] == '/') {
            filenameBegin = i+1;
            break;
        }

    /* If '/' found, it separates trailing path from file name */
    for (i = filenameBegin; i <= len-3; ++i)
    {
        if ((strncmp(filename+i, "subqcif", 7) == 0) ||
            (strncmp(filename+i, "sqcif", 5) == 0))
        {
            *pWidth = 128;
            *pHeight = 96;
            printf("Detected resolution SubQCIF (128x96) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qcif", 4) == 0)
        {
            *pWidth = 176;
            *pHeight = 144;
            printf("Detected resolution QCIF (176x144) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "4cif", 4) == 0)
        {
            *pWidth = 704;
            *pHeight = 576;
            printf("Detected resolution 4CIF (704x576) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "cif", 3) == 0)
        {
            *pWidth = 352;
            *pHeight = 288;
            printf("Detected resolution CIF (352x288) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qqvga", 5) == 0)
        {
            *pWidth = 160;
            *pHeight = 120;
            printf("Detected resolution QQVGA (160x120) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qvga", 4) == 0)
        {
            *pWidth = 320;
            *pHeight = 240;
            printf("Detected resolution QVGA (320x240) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "vga", 3) == 0)
        {
            *pWidth = 640;
            *pHeight = 480;
            printf("Detected resolution VGA (640x480) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "720p", 4) == 0)
        {
            *pWidth = 1280;
            *pHeight = 720;
            printf("Detected resolution 720p (1280x720) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "1080p", 5) == 0)
        {
            *pWidth = 1920;
            *pHeight = 1080;
            printf("Detected resolution 1080p (1920x1080) from file name.\n");
            return 0;
        }
        if (filename[i] == 'x')
        {
            if (sscanf(filename+i-4, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                printf("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
            else if (sscanf(filename+i-3, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                printf("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
            else if (sscanf(filename+i-2, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                printf("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
        }
        if (filename[i] == 'w')
        {
            if (sscanf(filename+i, "w%uh%u", &w, &h) == 2)
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

/*------------------------------------------------------------------------------

    API tracing

------------------------------------------------------------------------------*/
void JpegEnc_Trace(const char *msg)
{
    static FILE *fp = NULL;

    if(fp == NULL)
        fp = fopen("api.trc", "wt");

    if(fp)
        fprintf(fp, "%s\n", msg);
}

/*------------------------------------------------------------------------------

    InitInputLineBuffer
    -get address of input line buffer

------------------------------------------------------------------------------*/
i32 InitInputLineBuffer(inputLineBufferCfg * lineBufCfg, JpegEncCfg * encCfg, JpegEncIn * encIn, JpegEncInst inst)
{
    u32 stride = (encCfg->inputWidth + 15) & (~15); /* 16 pixel multiple stride */

    memset(lineBufCfg, 0, sizeof(inputLineBufferCfg));
    lineBufCfg->inst   = (void *)inst;
    //lineBufCfg->asic   = &(((jpegInstance_s *)inst)->asic);
    lineBufCfg->wrCnt  = 0;
    lineBufCfg->depth  = encCfg->inputLineBufDepth;
    lineBufCfg->inputFormat = encCfg->frameType;
    lineBufCfg->pixOnRow = stride;
    lineBufCfg->encWidth  = encCfg->codingWidth;
    lineBufCfg->encHeight = encCfg->codingHeight;
    lineBufCfg->hwHandShake = encCfg->inputLineBufHwModeEn;
    lineBufCfg->loopBackEn = encCfg->inputLineBufLoopBackEn;
    lineBufCfg->srcHeight = encCfg->codingType ? encCfg->restartInterval*16 : encCfg->inputHeight;
    lineBufCfg->srcVerOffset = encCfg->codingType ? 0 : encCfg->yOffset;
    lineBufCfg->getMbLines = &JpegEncGetEncodedMbLines;
    lineBufCfg->setMbLines = &JpegEncSetInputMBLines;
    lineBufCfg->ctbSize = 16;
    lineBufCfg->lumSrc = (u8 *)encIn->pLum;
    lineBufCfg->cbSrc  = (u8 *)encIn->pCb;
    lineBufCfg->crSrc  = (u8 *)encIn->pCr;

    if (VCEncInitInputLineBuffer(lineBufCfg))
      return -1;

    /* loopback mode */
    if (lineBufCfg->loopBackEn && lineBufCfg->lumBuf.buf)
    {
        encIn->busLum = lineBufCfg->lumBuf.busAddress;
        encIn->busCb = lineBufCfg->cbBuf.busAddress;
        encIn->busCr = lineBufCfg->crBuf.busAddress;

        /* data in SRAM start from the line to be encoded*/
        if(encCfg->codingType == JPEGENC_WHOLE_FRAME)
            encCfg->yOffset = 0;
    }

    return 0;
}

/*------------------------------------------------------------------------------

    SetInputLineBuffer
    -setup inputLineBufferCfg
    -initialize line buffer

------------------------------------------------------------------------------*/
void SetInputLineBuffer(inputLineBufferCfg * lineBufCfg, JpegEncCfg * encCfg, JpegEncIn * encIn, JpegEncInst inst, i32 sliceIdx)
{
    if (encCfg->codingType == JPEGENC_SLICED_FRAME)
    {
        i32 h = encCfg->codingHeight + encCfg->yOffset;
        i32 sliceRows = encCfg->restartInterval * 16;
        i32 rows = sliceIdx * sliceRows;
        if((rows + sliceRows) <= h)
            lineBufCfg->encHeight = sliceRows;
        else
            lineBufCfg->encHeight = h % sliceRows;
    }

    encIn->lineBufWrCnt = VCEncStartInputLineBuffer(lineBufCfg, HANTRO_FALSE);
    return;
}

void InitStreamSegmentCrl(SegmentCtl_s *ctl, commandLine_s *cml, FILE *out, JpegEncIn * encIn)
{
  ctl->streamRDCounter = 0;
  ctl->streamMultiSegEn = cml->streamMultiSegmentMode != 0;
  ctl->streamBase = (u8 *)encIn->pOutBuf[0];
  ctl->segmentSize = encIn->outBufSize[0]/ cml->streamMultiSegmentAmount;
  ctl->segmentSize = ((ctl->segmentSize + 16 - 1) & (~(16 - 1)));//segment size must be aligned to 16byte
  ctl->segmentAmount = cml->streamMultiSegmentAmount;
  ctl->outStreamFile = out;
}

/* Callback function called by the encoder SW after "segment ready"
    interrupt from HW. Note that this function is called after every segment is ready.
------------------------------------------------------------------------------*/
void EncStreamSegmentReady(void *cb_data)
{
  u8 *streamBase;
  SegmentCtl_s *ctl = (SegmentCtl_s*)cb_data;

  if(ctl->streamMultiSegEn)
  {
    streamBase = ctl->streamBase + (ctl->streamRDCounter % ctl->segmentAmount) * ctl->segmentSize;

    printf("receive segment irq %d,length=%d\n",ctl->streamRDCounter,ctl->segmentSize);
    if(writeOutput)
      WriteStrm(ctl->outStreamFile, (u32 *)streamBase, ctl->segmentSize, 0);

    ctl->streamRDCounter++;
  }
}
