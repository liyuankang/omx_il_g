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
#ifndef _JPEGTESTBENCH_H_
#define _JPEGTESTBENCH_H_

#include "base_type.h"

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Maximum lenght of the file path */
#ifndef MAX_PATH
#define MAX_PATH   256
#endif

#define DEFAULT -255

/* Structure for command line options */
typedef struct
{
    char input[MAX_PATH];
    char output[MAX_PATH];
    char inputThumb[MAX_PATH];
    i32 firstPic;
    i32 lastPic;
    i32 width;
    i32 height;
    i32 lumWidthSrc;
    i32 lumHeightSrc;
    i32 horOffsetSrc;
    i32 verOffsetSrc;
    i32 restartInterval;
    i32 frameType;
    i32 colorConversion;
    i32 rotation;
    i32 partialCoding;
    i32 codingMode;
    i32 markerType;
    i32 qLevel;
    i32 unitsType;
    i32 xdensity;
    i32 ydensity;
    i32 thumbnail;
    i32 widthThumb;
    i32 heightThumb;
    i32 lumWidthSrcThumb;
    i32 lumHeightSrcThumb;
    i32 horOffsetSrcThumb;
    i32 verOffsetSrcThumb;
    i32 write;
    i32 comLength;
    char com[MAX_PATH];
    i32 inputLineBufMode;
    i32 inputLineBufDepth;
    u32 amountPerLoopBack;
    u32 hashtype;
    i32 mirror;
    i32 formatCustomizedType;
    i32 constChromaEn;
    u32 constCb;
    u32 constCr;
    i32 losslessEnable;
    i32 predictMode;
    i32 ptransValue;
    u32 bitPerSecond;
    u32 mjpeg;
    u32 frameRateNum;
    u32 frameRateDenom;
    i32 rcMode;
    i32 picQpDeltaMin;
    i32 picQpDeltaMax;
    u32 qpmin;
    u32 qpmax;
    i32 fixedQP;
    u32 exp_of_input_alignment;
    u32 streamBufChain;
    u32 streamMultiSegmentMode;
    u32 streamMultiSegmentAmount;
    char dec400CompTableinput[MAX_PATH];

    char olInput[8][MAX_PATH];
    u32 overlayEnables;
    u32 olFormat[8];
    u32 olAlpha[8];
    u32 olWidth[8];
    u32 olCropWidth[8];
    u32 olHeight[8];
    u32 olCropHeight[8];
    u32 olXoffset[8];
    u32 olCropXoffset[8];
    u32 olYoffset[8];
    u32 olCropYoffset[8];
    u32 olYStride[8];
    u32 olUVStride[8];
}
commandLine_s;

typedef struct {
  u32 streamRDCounter;
  u32 streamMultiSegEn;
  u8 *streamBase;
  u32 segmentSize;
  u32 segmentAmount;
  FILE *outStreamFile;
} SegmentCtl_s;

#endif
