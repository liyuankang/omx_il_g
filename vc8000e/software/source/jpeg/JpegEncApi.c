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
--  Abstract  :  JPEG Encoder API
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/

#define JPEGENC_MAJOR_VERSION 1
#define JPEGENC_MINOR_VERSION 1

#define JPEGENC_BUILD_MAJOR 1
#define JPEGENC_BUILD_MINOR 46
#define JPEGENC_BUILD_REVISION 0
#define JPEGENC_SW_BUILD ((JPEGENC_BUILD_MAJOR * 1000000) + \
(JPEGENC_BUILD_MINOR * 1000) + JPEGENC_BUILD_REVISION)

#define JPEGENC_MAX_SIZE 4194304     /* 2048x2048 = 4194304 macroblocks */

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "jpegencapi.h"

#include "EncJpegInit.h"
#include "EncJpegInstance.h"
#include "EncJpegCodeFrame.h"
#include "EncJpegQuantTables.h"
#include "EncJpegMarkers.h"

#include "EncJpegPutBits.h"
#include "encasiccontroller.h"
#include "ewl.h"
#include "rate_control_picture.h"
#include "sw_slice.h"
#include "enccommon.h"
#include "encdec400.h"
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Tracing macro */
#ifdef JPEGENC_TRACE
#define APITRACE(str) JpegEnc_Trace(str)
#else
#define APITRACE(str) { printf(str); printf("\n"); }
#endif

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static bool_e CheckJpegCfg(const JpegEncCfg * pEncCfg);
static i32 CheckThumbnailCfg(const JpegEncThumb * pCfgThumb);
static i32 CheckFullSize(const JpegEncCfg * pCfgFull);

/*******************************************************************************
 Function name : JpegEncGetApiVersion
 Description   :
 Return type   : JpegEncApiVersion
*******************************************************************************/
JpegEncApiVersion JpegEncGetApiVersion(void)
{
    JpegEncApiVersion ver;

    ver.major = JPEGENC_MAJOR_VERSION;
    ver.minor = JPEGENC_MINOR_VERSION;
    APITRACE("JpegEncGetVersion# OK\n");
    return ver;
}

/*******************************************************************************
 Function name : JpegEncGetBuild
 Description   :
 Return type   : JpegEncBuild
*******************************************************************************/
JpegEncBuild JpegEncGetBuild(u32 core_id)
{
    JpegEncBuild ver;

    ver.swBuild = JPEGENC_SW_BUILD;
    ver.hwBuild = EWLReadAsicID(core_id);
    APITRACE("JpegEncGetBuild# OK\n");
    return (ver);
}

static i32 InitialJpegQp(i32 bits, i32 pels)
{
  const i32 qp_tbl[2][139] =
  {
    {508, 514, 524, 535, 546, 557, 569, 580, 594, 606, 616, 628, 641, 655, 668, 681, 695, 710, 723, 737, 751, 767, 780, 796, 812, 828, 842, 858, 874, 892, 911, 927, 944, 963, 982, 1000, 1021, 1040, 1061, 1081, 1103, 1124, 1146, 1171, 1194, 1219, 1244, 1261, 1283, 1303, 1325, 1351, 1376, 1408, 1435, 1461, 1493, 1526, 1545, 1575, 1602, 1632, 1658, 1689, 1724, 1761, 1800, 1838, 1873, 1911, 1951, 1980, 2020, 2063, 2106, 2149, 2198, 2234, 2284, 2336, 2380, 2420, 2475, 2524, 2574, 2601, 2658, 2705, 2746, 2803, 2848, 2915, 2980, 3046, 3129, 3206, 3282, 3353, 3421, 3454, 3519, 3592, 3655, 3750, 3816, 3876, 3939, 4010, 4119, 4233, 4320, 4428, 4532, 4636, 4739, 4972, 5021, 5145, 5247, 5435, 5544, 5737, 5945, 6074, 6297, 6509, 6862, 7131, 7515, 7963, 8448, 9085, 9446, 10189, 10605, 11883, 12621, 15170, 0x7fffffff},
    {138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}
  };
  const i32 upscale = 20000;
  i32 i = -1;

  //avoid overflow
  i64 bits64=(i64)bits;

  /* prevents overflow, QP would anyway be 17 with this high bitrate
     for all resolutions under and including 1920x1088 */
  //    if (bits > 1000000)
  //      return 17;

  /* Make room for multiplication */
  pels >>= 8;
  bits64 >>= 5;

  /* Use maximum QP if bitrate way too low. */
  if (!bits64)
    return 51<<QP_FRACTIONAL_BITS;

  /* Adjust the bits value for the current resolution */
  bits64 *= pels + 250;
  ASSERT(pels > 0);
  ASSERT(bits64 > 0);
  bits64 /= 350 + (3 * pels) / 4;
  bits64 = rcCalculate(bits64, upscale, pels << 6);

  
  while ((qp_tbl[0][++i])  < bits64);

  return ((qp_tbl[1][i]<<QP_FRACTIONAL_BITS)*51 + 69)/138;
}

JpegEncRet JpegEncInitRC(jpegInstance_s *pEncInst, const JpegEncCfg * pEncCfg)
{
    JpegEncRet ret;
    
    pEncInst->rateControl.picRc = pEncCfg->targetBitPerSecond ? ENCHW_YES : ENCHW_NO;
    
    if(pEncInst->rateControl.picRc)
    {
       //avoid VBV buffer
       pEncInst->timeIncrement = 0;
    
       pEncInst->rateControl.outRateDenom = pEncCfg->frameRateDenom;
       pEncInst->rateControl.outRateNum = pEncCfg->frameRateNum;
       pEncInst->rateControl.codingType = (pEncCfg->rcMode == JPEGENC_SINGLEFRAME)? ASIC_JPEG:ASIC_MJPEG;
       pEncInst->rateControl.monitorFrames = MAX(LEAST_MONITOR_FRAME, pEncInst->rateControl.outRateNum / pEncInst->rateControl.outRateDenom);
       pEncInst->rateControl.picSkip = ENCHW_NO;
       pEncInst->rateControl.hrd = ENCHW_NO;
       pEncInst->rateControl.ctbRc = ENCHW_NO; 
    
       //control bit accurate, default 105% accurate
       pEncInst->rateControl.tolMovingBitRate = 103; 
    
       pEncInst->rateControl.picArea = ((pEncCfg->codingWidth+ 7) & (~7)) * ((pEncCfg->codingHeight+ 7) & (~7));
    
       pEncInst->rateControl.ctbSize = 16;
       pEncInst->rateControl.ctbPerPic = pEncInst->rateControl.picArea/16/16;
       pEncInst->rateControl.ctbRows = ((pEncCfg->codingHeight+ 7) / 16);
    
       pEncInst->rateControl.qpHdr = -1<<QP_FRACTIONAL_BITS; //default init qp
       pEncInst->rateControl.qpMin = pEncCfg->qpmin<<QP_FRACTIONAL_BITS;
       pEncInst->rateControl.qpMax = pEncCfg->qpmax<<QP_FRACTIONAL_BITS;
       pEncInst->rateControl.virtualBuffer.bitRate = pEncCfg->targetBitPerSecond;
       pEncInst->rateControl.virtualBuffer.bufferSize = 0xffffffff;
       pEncInst->rateControl.bitrateWindow = 1;
       
       pEncInst->rateControl.intraQpDelta = 0 << QP_FRACTIONAL_BITS;
       pEncInst->rateControl.fixedIntraQp = 0 << QP_FRACTIONAL_BITS;
       pEncInst->rateControl.longTermQpDelta = 0;
       pEncInst->rateControl.f_tolMovingBitRate = (float)pEncInst->rateControl.tolMovingBitRate;
       pEncInst->rateControl.smooth_psnr_in_gop = 0;
       pEncInst->rateControl.rcQpDeltaRange = 10;
       pEncInst->rateControl.rcBaseMBComplexity= 15;       
       pEncInst->rateControl.sei.enabled = ENCHW_NO;
       pEncInst->rateControl.hierarchial_bit_allocation_GOP_size = 1;
       rcVirtualBuffer_s *vb = &pEncInst->rateControl.virtualBuffer;

       pEncInst->rateControl.picQpDeltaMax = pEncCfg->picQpDeltaMax;
       pEncInst->rateControl.picQpDeltaMin = pEncCfg->picQpDeltaMin;
       vb->unitsInTic = pEncInst->rateControl.outRateDenom;
       vb->timeScale = pEncInst->rateControl.outRateNum;
       vb->bitPerPic = rcCalculate(vb->bitRate, pEncInst->rateControl.outRateDenom, pEncInst->rateControl.outRateNum);
       
       pEncInst->rateControl.maxPicSizeI = rcCalculate(pEncInst->rateControl.virtualBuffer.bitRate, pEncInst->rateControl.outRateDenom, pEncInst->rateControl.outRateNum)*(1+20) ;
       pEncInst->rateControl.minPicSizeI = rcCalculate(pEncInst->rateControl.virtualBuffer.bitRate, pEncInst->rateControl.outRateDenom, pEncInst->rateControl.outRateNum) /(1+20);
    
       //init qp
       pEncInst->rateControl.qpHdr = InitialJpegQp(vb->bitPerPic, pEncInst->rateControl.picArea);

       pEncInst->rateControl.qpHdr = MIN(pEncInst->rateControl.qpMax, MAX(pEncInst->rateControl.qpMin, pEncInst->rateControl.qpHdr));
       if (pEncCfg->rcMode != JPEGENC_SINGLEFRAME) {
           pEncInst->rateControl.vbr = (pEncCfg->rcMode == JPEGENC_VBR)? HANTRO_TRUE:HANTRO_FALSE;
           pEncInst->rateControl.i32MaxPicSize = pEncInst->rateControl.maxPicSizeI;
       }
    
       if (VCEncInitRc(&pEncInst->rateControl, 1) != ENCHW_OK)
       {
         ret = JPEGENC_ERROR;
         return ret;
       }

       pEncInst->rateControl.sliceTypePrev = I_SLICE;
    }

    return JPEGENC_OK;
}
    
/*******************************************************************************
 Function name : JpegEncInit
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncCfg * pEncCfg
 Argument      : JpegEncInst * instAddr
*******************************************************************************/
JpegEncRet JpegEncInit(const JpegEncCfg * pEncCfg, JpegEncInst * instAddr)
{
    JpegEncRet ret;
    jpegInstance_s *pEncInst = NULL;

    APITRACE("JpegEncInit#");

    /* check that right shift on negative numbers is performed signed */
    /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
    /*lint -restore */

    /* Check for illegal inputs */
    if(pEncCfg == NULL || instAddr == NULL)
    {
        APITRACE("JpegEncInit: ERROR null argument");
        return JPEGENC_NULL_ARGUMENT;
    }

    /* Check that configuration is valid */
    if(CheckJpegCfg(pEncCfg) == ENCHW_NOK)
    {
        APITRACE("JpegEncInit: ERROR invalid argument");
        return JPEGENC_INVALID_ARGUMENT;
    }

    /* Initialize encoder instance and allocate memories */
    ret = JpegInit(pEncCfg, &pEncInst);
    if(ret != JPEGENC_OK)
    {
        APITRACE("JpegEncInit: ERROR Initialization failed");
        return ret;
    }

    if (pEncCfg->frameType > JPEGENC_YUV422_INTERLEAVED_UYVY && pEncCfg->frameType < JPEGENC_YUV420_I010)
       pEncInst->featureToSupport.rgbEnabled = 1;
    else
       pEncInst->featureToSupport.rgbEnabled = 0;
    
    pEncInst->featureToSupport.jpegEnabled = 1;

    /*Get the supported features of all cores*/
    EWLHwConfig_t asic_cfg;
    u32 core_info = 0; /*mode[1bit](1:all 0:specified)+amount[3bit](the needing amount -1)+reserved+core_mapping[8bit]*/
    u32 valid_num =0;
    u32 i=0;
    
    for (i=0;i<EWLGetCoreNum();i++)
    {
     asic_cfg = EWLReadAsicConfig(i);
     if (JpegEncCoreHasFeatures(&asic_cfg,&pEncInst->featureToSupport))
     {
      //core_info |= 1<<i;
      valid_num++;
     }
     else
      continue;
    }
    
    if (valid_num == 0) /*none of cores is supported*/
      return VCENC_INVALID_ARGUMENT;
  
    core_info |= 1<< CORE_INFO_MODE_OFFSET; //now just support 1 core,so mode is all.
  
    core_info |= 0<< CORE_INFO_AMOUNT_OFFSET;//now just support 1 core

    pEncInst->reserve_core_info = core_info;
    printf("VC8000EJ reserve core info:%x\n",pEncInst->reserve_core_info);

    /* low latency */
    pEncInst->inputLineBuf.inputLineBufEn = pEncCfg->inputLineBufEn;
    pEncInst->inputLineBuf.inputLineBufLoopBackEn = pEncCfg->inputLineBufLoopBackEn;
    pEncInst->inputLineBuf.inputLineBufDepth = pEncCfg->inputLineBufDepth;
    pEncInst->inputLineBuf.amountPerLoopBack = pEncCfg->amountPerLoopBack;
    pEncInst->inputLineBuf.inputLineBufHwModeEn = pEncCfg->inputLineBufHwModeEn;
    pEncInst->inputLineBuf.cbFunc = pEncCfg->inputLineBufCbFunc;
    pEncInst->inputLineBuf.cbData = pEncCfg->inputLineBufCbData;

    /*stream multi-segment*/
    pEncInst->streamMultiSegment.streamMultiSegmentMode = pEncCfg->streamMultiSegmentMode;
    pEncInst->streamMultiSegment.streamMultiSegmentAmount = pEncCfg->streamMultiSegmentAmount;
    pEncInst->streamMultiSegment.cbFunc = pEncCfg->streamMultiSegCbFunc;
    pEncInst->streamMultiSegment.cbData = pEncCfg->streamMultiSegCbData;

    /* hardware config */
    pEncInst->asic.regs.qp = 0;
    pEncInst->asic.regs.constrainedIntraPrediction = 0;
    pEncInst->asic.regs.frameCodingType = ASIC_INTRA;
    pEncInst->asic.regs.roundingCtrl = 0;
    pEncInst->asic.regs.codingType = ASIC_JPEG;

    /* stride*/
    pEncInst->input_alignment = 1<<pEncCfg->exp_of_input_alignment;

    /* Pre processing */
    pEncInst->preProcess.lumWidthSrc = 0;
    pEncInst->preProcess.lumHeightSrc = 0;
    pEncInst->preProcess.lumWidth = 0;
    pEncInst->preProcess.lumHeight = 0;
    pEncInst->preProcess.horOffsetSrc = 0;
    pEncInst->preProcess.verOffsetSrc = 0;
    pEncInst->preProcess.rotation = 0;
    pEncInst->preProcess.videoStab = 0;
    pEncInst->preProcess.input_alignment = pEncInst->input_alignment;

    pEncInst->preProcess.inputFormat = pEncCfg->frameType;
    pEncInst->preProcess.colorConversionType = pEncCfg->colorConversion.type;
    pEncInst->preProcess.colorConversionCoeffA = pEncCfg->colorConversion.coeffA;
    pEncInst->preProcess.colorConversionCoeffB = pEncCfg->colorConversion.coeffB;
    pEncInst->preProcess.colorConversionCoeffC = pEncCfg->colorConversion.coeffC;
    pEncInst->preProcess.colorConversionCoeffE = pEncCfg->colorConversion.coeffE;
    pEncInst->preProcess.colorConversionCoeffF = pEncCfg->colorConversion.coeffF;
    pEncInst->preProcess.colorConversionCoeffG = pEncCfg->colorConversion.coeffG;
    pEncInst->preProcess.colorConversionCoeffH = pEncCfg->colorConversion.coeffH;
    pEncInst->preProcess.colorConversionLumaOffset = pEncCfg->colorConversion.LumaOffset;

    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      pEncInst->preProcess.overlayEnable[i] = pEncCfg->olEnable[i];
      pEncInst->preProcess.overlayFormat[i] = pEncCfg->olFormat[i];
      pEncInst->preProcess.overlayAlpha[i] = pEncCfg->olAlpha[i];
      pEncInst->preProcess.overlayWidth[i] = pEncCfg->olWidth[i];
      pEncInst->preProcess.overlayCropWidth[i] = pEncCfg->olCropWidth[i];
      pEncInst->preProcess.overlayHeight[i] = pEncCfg->olHeight[i];
      pEncInst->preProcess.overlayCropHeight[i] = pEncCfg->olCropHeight[i];
      pEncInst->preProcess.overlayXoffset[i] = pEncCfg->olXoffset[i];
      pEncInst->preProcess.overlayCropXoffset[i] = pEncCfg->olCropXoffset[i];
      pEncInst->preProcess.overlayYoffset[i] = pEncCfg->olYoffset[i];
      pEncInst->preProcess.overlayCropYoffset[i] = pEncCfg->olCropYoffset[i];
      pEncInst->preProcess.overlayYStride[i] = pEncCfg->olYStride[i];
      pEncInst->preProcess.overlayUVStride[i] = pEncCfg->olUVStride[i];
    }
    

    EncSetColorConversion(&pEncInst->preProcess, &pEncInst->asic);

    /* constant chroma control */
    pEncInst->preProcess.constChromaEn = pEncCfg->constChromaEn;
    pEncInst->preProcess.constCb = pEncCfg->constCb;
    pEncInst->preProcess.constCr = pEncCfg->constCr;

    //default registers to avoid assert
    pEncInst->asic.regs.minCbSize = 3;
    pEncInst->asic.regs.maxCbSize = 6;
    pEncInst->asic.regs.minTrbSize = 2;
    pEncInst->asic.regs.maxTrbSize = 4;

    // lossless jpeg.
    pEncInst->asic.regs.ljpegEn = pEncInst->jpeg.losslessEn;
    pEncInst->asic.regs.ljpegFmt = pEncInst->jpeg.codingMode;
    pEncInst->asic.regs.ljpegPsv = pEncInst->jpeg.predictMode;
    pEncInst->asic.regs.ljpegPt  = pEncInst->jpeg.ptransValue;

    if (pEncInst->jpeg.codingMode==JPEGENC_MONO_MODE)
    {
        pEncInst->jpeg.frame.Nf = 1;
    }

#ifdef TRACE_STREAM
    /* Open stream tracing */
    EncOpenStreamTrace("stream.trc");
    traceStream.frameNum = 0;
#endif
    hash_init(&pEncInst->jpeg.hashctx, pEncCfg->hashType);

    /* Status == INIT   Initialization succesful */
    pEncInst->encStatus = ENCSTAT_INIT;
    pEncInst->fixedQP = pEncCfg->fixedQP;

    JpegEncInitRC(pEncInst, pEncCfg);

    pEncInst->inst = pEncInst;  /* used as checksum */

    *instAddr = (JpegEncInst) pEncInst;

    APITRACE("JpegEncInit: OK");
    return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncRelease
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
*******************************************************************************/
JpegEncRet JpegEncRelease(JpegEncInst inst)
{
    jpegInstance_s *pEncInst = (jpegInstance_s *) inst;

    APITRACE("JpegEncRelease#");

    /* Check for illegal inputs */
    if(pEncInst == NULL)
    {
        APITRACE("JpegEncRelease: ERROR null argument");
        return JPEGENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("JpegEncRelease: ERROR Invalid instance");
        return JPEGENC_INSTANCE_ERROR;
    }

#ifdef TRACE_STREAM
    EncCloseStreamTrace();
#endif

    JpegShutdown(pEncInst);

    APITRACE("JpegEncRelease: OK");

    return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncSetThumbnail
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncCfg * pEncCfg
*******************************************************************************/
JpegEncRet JpegEncSetThumbnail(JpegEncInst inst, const JpegEncThumb *pJpegThumb)
{
    jpegInstance_s *pEncInst = (jpegInstance_s *) inst;

    APITRACE("JpegEncSetThumbnail#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pJpegThumb == NULL))
    {
        APITRACE("JpegEncSetThumbnail: ERROR null argument");
        return JPEGENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("JpegEncSetThumbnail: ERROR Invalid instance");
        return JPEGENC_INSTANCE_ERROR;
    }

    if(CheckThumbnailCfg(pJpegThumb) != JPEGENC_OK)
    {
        APITRACE("JpegEncSetThumbnail: ERROR Invalid thumbnail");
        return JPEGENC_INVALID_ARGUMENT;
    }

    pEncInst->jpeg.appn.thumbEnable = 1;

    /* save the thumbnail config */
    (void)EWLmemcpy(&pEncInst->jpeg.thumbnail, pJpegThumb, sizeof(JpegEncThumb));
    
    APITRACE("JpegEncSetThumbnail: OK");

    return JPEGENC_OK;
}

int JpegEncCoreHasFeatures(EWLHwConfig_t *feature,EWLHwConfig_t *cfg) 
{
  if (cfg->jpegEnabled == 1 && cfg->jpegEnabled == feature->jpegEnabled)
    return  1;
  else
    return  0;
}


/*******************************************************************************
 Function name : JpegEncSetPictureSize
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncCfg * pEncCfg
*******************************************************************************/
JpegEncRet JpegEncSetPictureSize(JpegEncInst inst, const JpegEncCfg * pEncCfg)
{
    u32 mbTotal = 0;
    u32 height = 0;
    u32 heightMcu, widthMcus;
    asicMemAlloc_s allocCfg;
    jpegInstance_s *pEncInst = (jpegInstance_s *) inst;

    APITRACE("JpegEncSetPictureSize#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pEncCfg == NULL))
    {
        APITRACE("JpegEncSetPictureSize: ERROR null argument");
        return JPEGENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("JpegEncSetPictureSize: ERROR Invalid instance");
        return JPEGENC_INSTANCE_ERROR;
    }

    if(CheckFullSize(pEncCfg) != JPEGENC_OK)
    {
        APITRACE
            ("JpegEncSetPictureSize: ERROR Out of range image dimension(s)");
        return JPEGENC_INVALID_ARGUMENT;
    }

    if(pEncCfg->losslessEn)
    {
        if (pEncCfg->rotation)
        {
            APITRACE
                ("JpegEncSetPictureSize: ERROR Not allow rotation for lossless");
            printf("JpegEncSetPictureSize: ERROR Not allow rotation for lossless\n");
            return JPEGENC_INVALID_ARGUMENT;
        }
        if(pEncCfg->frameType>=JPEGENC_YUV420_8BIT_DAHUA_HEVC)
        {
            APITRACE
                ("JpegEncSetPictureSize: ERROR Not allow such format for lossless");
            printf("JpegEncSetPictureSize: ERROR Not allow such format for lossless\n");
            return JPEGENC_INVALID_ARGUMENT;
        }
    }

    if(pEncCfg->losslessEn)
    {
        widthMcus = (pEncCfg->codingWidth + 1) / 2;
        heightMcu = 2; 
    }
    else
    {
        widthMcus = (pEncCfg->codingWidth + 15) / 16;
        heightMcu = (pEncCfg->codingMode==JPEGENC_422_MODE ? 8 : 16);
    }

    if (pEncCfg->codingMode==JPEGENC_MONO_MODE)
    {
        heightMcu /= 2;
        widthMcus *= 2;
    }
    
    if(((pEncCfg->restartInterval * heightMcu) > pEncCfg->codingHeight) ||
       ((pEncCfg->restartInterval * widthMcus) > 0xFFFF))
    {
        APITRACE("JpegEncSetPictureSize: ERROR restart interval too big");
        printf("JpegEncSetPictureSize: ERROR restart interval too big\n");
        return JPEGENC_INVALID_ARGUMENT;
    }

    if (((pEncCfg->xOffset & 1) != 0) || ((pEncCfg->yOffset & 1) != 0))
    {
        APITRACE("JpegEncSetPictureSize: ERROR Invalid offset");
        return JPEGENC_INVALID_ARGUMENT;
    }

    /* Restart interval must be enabled for sliced encoding */
    if(pEncCfg->codingType == JPEGENC_SLICED_FRAME)
    {
        if(pEncCfg->rotation != JPEGENC_ROTATE_0)
        {
            APITRACE
                ("JpegEncSetPictureSize: ERROR rotation not allowed in sliced mode");
            return JPEGENC_INVALID_ARGUMENT;
        }

        if(pEncCfg->restartInterval == 0)
        {
            APITRACE
                ("JpegEncSetPictureSize: ERROR restart interval not set");
            return JPEGENC_INVALID_ARGUMENT;
        }

        /* Extra limitation for partial encoding: yOffset must be 
         * multiple of slice height in rows */
        if((pEncCfg->yOffset % (pEncCfg->restartInterval * heightMcu)) != 0)
        {
            APITRACE("JpegEncSetPictureSize: ERROR yOffset not valid");
            return JPEGENC_INVALID_ARGUMENT;
        }
    }

    mbTotal =
        (u32) (((pEncCfg->codingWidth + 15) / 16) * ((pEncCfg->codingHeight +
                                                      (!pEncCfg->losslessEn && pEncCfg->codingMode==JPEGENC_422_MODE?heightMcu:16)-1) / (!pEncCfg->losslessEn && pEncCfg->codingMode==JPEGENC_422_MODE?heightMcu:16)));

    pEncInst->jpeg.header = ENCHW_YES;
    pEncInst->jpeg.width = pEncCfg->codingWidth;
    pEncInst->jpeg.height = pEncCfg->codingHeight;
    /*pEncInst->jpeg.lastColumn = ((pEncInst->jpeg.width + 15) / 16);*/
    pEncInst->jpeg.mbPerFrame = mbTotal;

    /* Pre processing */
    pEncInst->preProcess.lumWidthSrc = pEncCfg->inputWidth;
    pEncInst->preProcess.lumHeightSrc = pEncCfg->inputHeight;
    pEncInst->preProcess.lumWidth = pEncCfg->codingWidth;
    pEncInst->preProcess.lumHeight = pEncCfg->codingHeight;
    pEncInst->preProcess.horOffsetSrc = pEncCfg->xOffset;
    pEncInst->preProcess.verOffsetSrc = pEncCfg->yOffset;
    pEncInst->preProcess.rotation = pEncCfg->rotation;
    pEncInst->preProcess.mirror = pEncCfg->mirror;
    pEncInst->preProcess.input_alignment = (1 << pEncCfg->exp_of_input_alignment);

    /* Restart interval (MCU rows converted to macroblocks) */
    pEncInst->jpeg.rstMbRows = pEncCfg->restartInterval;
    pEncInst->jpeg.restart.Ri = (u32)(pEncCfg->restartInterval * widthMcus);

    /* Coding type */
    if(pEncCfg->codingType == JPEGENC_WHOLE_FRAME)
    {
        pEncInst->jpeg.codingType = ENC_WHOLE_FRAME;
        height = pEncInst->jpeg.height;
    }
    else
    {
        /* Sliced mode */
        pEncInst->jpeg.codingType = ENC_PARTIAL_FRAME;
        pEncInst->jpeg.sliceRows = (u32) pEncCfg->restartInterval;
        height = (pEncCfg->restartInterval * (pEncCfg->losslessEn?16:heightMcu));
    }

#ifdef JPEGENC_422_MODE_SUPPORTED
    if(pEncCfg->codingMode == JPEGENC_420_MODE)
        pEncInst->jpeg.codingMode = JPEGENC_420_MODE;
    else
    {
        if ((pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_YUYV) &&
            (pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_UYVY) &&
            (pEncInst->preProcess.inputFormat != JPEGENC_YUV422_888))
        {
            APITRACE("JpegEncSetPictureSize: ERROR 4:2:0 input in 4:2:2 mode");
            return JPEGENC_INVALID_ARGUMENT;
        }
        if (pEncInst->preProcess.rotation != JPEGENC_ROTATE_0)
        {
            APITRACE("JpegEncSetPictureSize: ERROR rotation in 4:2:2 mode");
            return JPEGENC_INVALID_ARGUMENT;
        }
        pEncInst->jpeg.codingMode = JPEGENC_422_MODE;
    }
#else
    //FIXME: check fuse if support other format
    pEncInst->jpeg.codingMode = pEncCfg->codingMode;
#endif

    /* Check that configuration is valid */
    if(EncPreProcessCheck(&pEncInst->preProcess) == ENCHW_NOK)
    {
        APITRACE
            ("JpegEncSetPictureSize: ERROR invalid pre-processing argument");
        return JPEGENC_INVALID_ARGUMENT;
    }

    /* Allocate internal SW/HW shared memories */
    memset(&allocCfg, 0, sizeof (asicMemAlloc_s));
    allocCfg.width = (u32) pEncInst->jpeg.width; 
    allocCfg.height = height;
    allocCfg.encodingType = ASIC_JPEG;
    if(EncAsicMemAlloc_V2(&pEncInst->asic, &allocCfg) != ENCHW_OK)
    {
        APITRACE("JpegEncSetPictureSize: ERROR ewl memory allocation");
        return JPEGENC_EWL_MEMORY_ERROR;
    }

    APITRACE("JpegEncSetPictureSize: OK");

    return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncGetOverlaySlice
 Description   :
 Return type   : void
*******************************************************************************/
void JpegEncGetOverlaySlice(JpegEncInst inst, JpegEncIn * pEncIn, i32 restartInterval, i32 partialCoding, i32 slice, i32 sliceRows)
{
  jpegInstance_s *pEncInst = (jpegInstance_s *) inst;
  preProcess_s *preProcess = &pEncInst->preProcess;
  u32 minY = slice * 16 * restartInterval;
  u32 maxY = minY + sliceRows;
  u32 i;
  
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    preProcess->overlayVerOffset[i] = preProcess->overlayCropYoffset[i];
    preProcess->overlaySliceHeight[i] = preProcess->overlayCropHeight[i];
    preProcess->overlaySliceYoffset[i] = preProcess->overlayYoffset[i];

    //Get slice info
    if(pEncIn->overlayEnable[i] && partialCoding)
    {
      //We do not do any overlay if region is outside slice
      if(preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] < minY ||
         preProcess->overlayYoffset[i] >= maxY)
      {
        preProcess->overlayEnable[i] = 0;
        continue;
      }
      preProcess->overlayEnable[i] = 1;

      //Only two case for slice y offset
      preProcess->overlaySliceYoffset[i] = 0;
      
      /*Only need to chop if part of overlay region is within slice*/
      //First case: Top part; Only need to modify height and slice y offset
      if(preProcess->overlayYoffset[i] >= minY && preProcess->overlayYoffset[i] < maxY &&
         preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] >= maxY)
      {
        preProcess->overlaySliceHeight[i] = maxY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceYoffset[i] = preProcess->overlayYoffset[i] - minY;
      }
      //Second case: Middle part; Need to take care of cropping y offset
      else if(preProcess->overlayYoffset[i] <= minY  &&
              preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] >= maxY)
      {
        preProcess->overlayVerOffset[i] += minY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceHeight[i] = sliceRows;
      }
      //Third case: bottom part;
      else if(preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] <= maxY &&
              preProcess->overlayYoffset[i] <= minY)
      {
        preProcess->overlayVerOffset[i] += minY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceHeight[i] = preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i]
                                            - minY;
      }
    }
  }
}


#define DCTSIZE2 64

static const unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
  16,  11,  10,  16,  24,  40,  51,  61,
  12,  12,  14,  19,  26,  58,  60,  55,
  14,  13,  16,  24,  40,  57,  69,  56,
  14,  17,  22,  29,  51,  87,  80,  62,
  18,  22,  37,  56,  68, 109, 103,  77,
  24,  35,  55,  64,  81, 104, 113,  92,
  49,  64,  78,  87, 103, 121, 120, 101,
  72,  92,  95,  98, 112, 100, 103,  99
};
static const unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
  17,  18,  24,  47,  99,  99,  99,  99,
  18,  21,  26,  66,  99,  99,  99,  99,
  24,  26,  56,  99,  99,  99,  99,  99,
  47,  66,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99
};

uint8_t quant_div_tbl[2][64];

/*
 * Quantization table setup routines
 */
static void jpeg_set_quant_table (uint8_t quant_tbl[64],
              const unsigned int *basic_table,
              int scale_factor, int force_baseline)
/* Define a quantization table equal to the basic_table times
 * a scale factor (given as a percentage).
 * If force_baseline is TRUE, the computed quantization table entries
 * are limited to 1..255 for JPEG baseline compatibility.
 */
{
  int i;
  long temp;

  for (i = 0; i < 64; i++) {
    temp = ((long) basic_table[i] * scale_factor + 50L) / 100L;
    /* limit the values to the valid range */
    if (temp <= 0L) temp = 1L;
    if (temp > 32767L) temp = 32767L; /* max quantizer needed for 12 bits */
    if (force_baseline && temp > 255L)
      temp = 255L;      /* limit to baseline range if requested */

    //we support only baseline
    //quant_tbl[i] = (uint16_t) temp; 
    quant_tbl[i] = (uint8_t) temp;
  }
}

static int jpeg_quality_scaling (int quality)
/* Convert a user-specified quality rating to a percentage scaling factor
 * for an underlying quantization table, using our recommended scaling curve.
 * The input 'quality' factor should be 0 (terrible) to 100 (very good).
 */
{
  /* Safety limit on quality factor.  Convert 0 to 1 to avoid zero divide. */
  if (quality <= 0) quality = 1;
  if (quality > 100) quality = 100;

  /* The basic table is used as-is (scaling 100) for a quality of 50.
   * Qualities 50..100 are converted to scaling percentage 200 - 2*Q;
   * note that at Q=100 the scaling is 0, which will cause jpeg_set_quant_table
   * to make all the table entries 1 (hence, minimum quantization loss).
   * Qualities 1..50 are converted to scaling percentage 5000/Q.
   */
  if (quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - quality*2;

  return quality;
}


void JpegEncQuantTab(uint8_t quant_div_tbl[2][64], int quality, int force_baseline)
{
  /* Convert user 0-100 rating to percentage scaling */
  //int scale_factor = jpeg_quality_scaling(quality);

  /* Convert user 0-138 scaling level*/
  const int scale_table[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 32, 33, 35, 36, 37, 38, 39, 40, 41, 44, 45, 46, 47, 48, 50, 53, 55, 57, 60, 62, 64, 66, 68, 70, 72, 74, 77, 79, 82, 84, 86, 90, 94, 96, 98, 103, 105, 110, 114, 117, 119, 124, 129, 133, 138, 140, 146, 153, 156, 160, 164, 168, 174, 179, 185, 189, 196, 204, 208, 212, 219, 223, 234, 239, 240, 248, 261, 269, 280, 284, 295, 311, 320, 325, 335, 353, 362, 375, 392, 408, 415, 425, 443, 471, 482, 496, 520, 540, 563, 572, 609, 635, 654, 687, 727, 755, 797, 842, 897, 955, 986, 1043, 1130, 1227, 1297, 1412, 1515, 1663, 1930, 2395};
  int scale_factor = scale_table[quality];

  /* Set up standard quality tables */
  jpeg_set_quant_table( (uint8_t *)quant_div_tbl[0], std_luminance_quant_tbl, scale_factor, force_baseline);
  jpeg_set_quant_table( (uint8_t *)quant_div_tbl[1], std_chrominance_quant_tbl, scale_factor, force_baseline);  
}


/* Run HW without block */
JpegEncRet JpegEncEncodeRun(JpegEncInst inst,
                               const JpegEncIn * pEncIn, 
                               JpegEncOut * pEncOut)
{
    jpegInstance_s *pEncInst = (jpegInstance_s *) inst;
    jpegData_s *jpeg;
    asicData_s *asic;
    preProcess_s *preProcess;
    jpegEncodeFrame_e ret;
    u32 qp=0;
    i32 heightMcu;
    u32 buf0MinSize = JPEGENC_STREAM_MIN_BUF0_SIZE;
    i32 i;

    /* Check for illegal inputs */
    if(pEncInst == NULL)
    {
        APITRACE("JpegEncEncode: ERROR null instance");
        return JPEGENC_NULL_ARGUMENT;
    }

    if((pEncIn == NULL) || (pEncOut == NULL))
    {
        APITRACE("JpegEncEncode: ERROR null arguments");
        ret = JPEGENC_NULL_ARGUMENT;
        goto enc_err;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("JpegEncEncode: ERROR Invalid instance");
        ret = JPEGENC_INSTANCE_ERROR;
        goto enc_err;
    }
    
    asic = &pEncInst->asic;
    jpeg = &pEncInst->jpeg;
    preProcess = &pEncInst->preProcess;
    if (jpeg->appn.thumbEnable)
        buf0MinSize += jpeg->thumbnail.dataLength;

    /* Check for invalid input values */
    if(pEncIn->pOutBuf[0] == NULL)
    {
        APITRACE("JpegEncEncode: ERROR Invalid output buffer0");
        ret = JPEGENC_INVALID_ARGUMENT;
        goto enc_err;
    }

    if(pEncIn->outBufSize[0] < buf0MinSize && pEncInst->streamMultiSegment.streamMultiSegmentMode == 0)
    {
        APITRACE("JpegEncEncode: ERROR Too small output buffer0");
        ret = JPEGENC_INVALID_ARGUMENT;
        goto enc_err;
    }

    if(pEncIn->outBufSize[1] && ((asic->regs.asicCfg.streamBufferChain == 0) || pEncInst->streamMultiSegment.streamMultiSegmentMode != 0))
    {
        APITRACE("JpegEncEncode: ERROR Two stream buffer not supported");
        ret = JPEGENC_INVALID_ARGUMENT;
        goto enc_err;
    }

    if(pEncIn->outBufSize[1] && (pEncIn->pOutBuf[1] == NULL))
    {
        APITRACE("JpegEncEncode: ERROR Invalid output buffer1");
        ret = JPEGENC_INVALID_ARGUMENT;
        goto enc_err;
    }

    /* Clear the output structure */
    pEncOut->jfifSize = 0;

    /* todo: check that thumbnail fits also */
    /* Set stream buffer, the size has been checked */
    if(EncJpegSetBuffer(&pEncInst->stream, (u8 *) pEncIn->pOutBuf[0], (u32) pEncIn->outBufSize[0]) == ENCHW_NOK)
    {
        APITRACE("JpegEncEncode: ERROR Invalid output buffer");
        ret = JPEGENC_INVALID_ARGUMENT;
        goto enc_err;
    }

    /* Setup input line buffer */
    pEncInst->inputLineBuf.wrCnt = pEncIn->lineBufWrCnt;

    /* Set ASIC input image */
    asic->regs.inputLumBase = pEncIn->busLum;
    asic->regs.inputCbBase = pEncIn->busCb;
    asic->regs.inputCrBase = pEncIn->busCr;
    asic->regs.pixelsOnRow = (u32) preProcess->lumWidthSrc;
    asic->regs.jpegMode = (jpeg->codingMode == JPEGENC_420_MODE) ? 0 : 1;
    asic->regs.ljpegFmt = jpeg->codingMode;
    for (i = 0; i < MAX_STRM_BUF_NUM; i ++)
    {
      asic->regs.outputStrmSize[i] = pEncIn->outBufSize[i];
      asic->regs.outputStrmBase[i] = pEncIn->busOutBuf[i];
    }

    /*stream multi-segment*/
    asic->regs.streamMultiSegEn = pEncInst->streamMultiSegment.streamMultiSegmentMode != 0;
    asic->regs.streamMultiSegSWSyncEn = pEncInst->streamMultiSegment.streamMultiSegmentMode == 2;
    asic->regs.streamMultiSegRD = 0;
    asic->regs.streamMultiSegIRQEn = 0;
    pEncInst->streamMultiSegment.rdCnt = 0;
    if (asic->regs.streamMultiSegEn)
    {
      //make sure the stream size is equal to N*segment size
      asic->regs.streamMultiSegSize = asic->regs.outputStrmSize[0]/pEncInst->streamMultiSegment.streamMultiSegmentAmount;
      asic->regs.streamMultiSegSize = ((asic->regs.streamMultiSegSize + 16 - 1) & (~(16 - 1)));//segment size must be aligned to 16byte
      asic->regs.outputStrmSize[0] = asic->regs.streamMultiSegSize*pEncInst->streamMultiSegment.streamMultiSegmentAmount;
      asic->regs.streamMultiSegIRQEn = 1;
      printf("segment size = %d\n",asic->regs.streamMultiSegSize);
    }

    heightMcu = (jpeg->codingMode==JPEGENC_422_MODE ? 8 : 16);

    /* slice/restart information */
    if(jpeg->codingType == ENC_WHOLE_FRAME)
    {
        asic->regs.jpegSliceEnable = 0;
        asic->regs.jpegRestartInterval = jpeg->rstMbRows;
        asic->regs.jpegRestartMarker = 0;
    }
    else
    {
        asic->regs.jpegSliceEnable = 1;
        //asic->regs.jpegRestartInterval = 0;
        asic->regs.jpegRestartInterval = jpeg->rstMbRows;
    }

    /* Check if this is start of a new frame */
    if(jpeg->sliceNum == 0)
    {
        jpeg->mbNum = 0;
        /*jpeg->column = 0;*/
        jpeg->row = 0;
    }

    /* For sliced frame, check if this slice should be encoded.
     * Vertical offset is multiple of slice height */
    if((jpeg->codingType == ENC_PARTIAL_FRAME) &&
       (preProcess->verOffsetSrc >
        (u32) (jpeg->sliceNum * jpeg->sliceRows * heightMcu)))
    {
        jpeg->sliceNum++;
        APITRACE("JpegEncEncode: OK  restart interval");
        return JPEGENC_RESTART_INTERVAL;
    }

    if(pEncInst->rateControl.picRc || pEncInst->fixedQP!= -1)
    {
       if (pEncInst->rateControl.picRc)
       {
         VCEncBeforePicRc(&pEncInst->rateControl, pEncInst->timeIncrement, I_SLICE, 0, NULL);
         jpeg->qp = CLIP3(0, 138, (((pEncInst->rateControl.qpHdr*138 + 25)/51) >> QP_FRACTIONAL_BITS) );
       }
       else if (pEncInst->fixedQP!= -1)
         jpeg->qp = CLIP3(0, 138, ((((pEncInst->fixedQP << QP_FRACTIONAL_BITS)*138 + 25)/51) >> QP_FRACTIONAL_BITS) );
	   
       JpegEncQuantTab(quant_div_tbl, jpeg->qp, HANTRO_TRUE);

       /* Set parameters depending on user config */
       /* Choose quantization tables */
       pEncInst->jpeg.qTable.pQlumi = quant_div_tbl[0];
       pEncInst->jpeg.qTable.pQchromi = quant_div_tbl[1];

       /* Copy quantization tables to ASIC internal memories */
       EncAsicSetQuantTable(&pEncInst->asic, pEncInst->jpeg.qTable.pQlumi,  pEncInst->jpeg.qTable.pQchromi);        
    }

    /* Check if HW resource is available */
    if(EWLReserveHw(pEncInst->asic.ewl,&pEncInst->reserve_core_info) == EWL_ERROR)
    {
        APITRACE("JpegEncEncode: ERROR hw resource unavailable");
        ret = JPEGENC_HW_RESERVED;        
        goto enc_err;
    }

    /* set the rst value for HW if RST wanted */
    if(jpeg->restart.Ri)
    {
        i32 rstCount = jpeg->rstCount;
        if (jpeg->losslessEn)  
        {
            //when heighMcu = 2 => mcuInMb = 16/2=8
            if (jpeg->codingMode==JPEGENC_MONO_MODE)
            {
                rstCount = (jpeg->rstCount*16)%8;
            }
            else
            {
                rstCount = (jpeg->rstCount*8)%8;
            }
        }
        else
        {
            if (jpeg->codingMode==JPEGENC_MONO_MODE)
            {
                rstCount = (jpeg->rstCount*2)%8;
            }
        }
        
        switch (rstCount)
        {
        case 0:
            asic->regs.jpegRestartMarker = RST0;
            break;
        case 1:
            asic->regs.jpegRestartMarker = RST1;
            break;
        case 2:
            asic->regs.jpegRestartMarker = RST2;
            break;
        case 3:
            asic->regs.jpegRestartMarker = RST3;
            break;
        case 4:
            asic->regs.jpegRestartMarker = RST4;
            break;
        case 5:
            asic->regs.jpegRestartMarker = RST5;
            break;
        case 6:
            asic->regs.jpegRestartMarker = RST6;
            break;
        case 7:
            asic->regs.jpegRestartMarker = RST7;
            break;
        default:
            ASSERT(0);
        }

        printf("RST=%x\n", asic->regs.jpegRestartMarker);
        jpeg->rstCount++;

        if(jpeg->rstCount > 7)
            jpeg->rstCount = 0;
    }

    /* Set up overlay asic regs */
    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      //Offsets are set for after cropping, so we don't need to adjust them
      asic->regs.overlayEnable[i] = preProcess->overlayEnable[i];
      asic->regs.overlayFormat[i] = preProcess->overlayFormat[i];
      asic->regs.overlayAlpha[i] = preProcess->overlayAlpha[i];
      asic->regs.overlayXoffset[i] = preProcess->overlayXoffset[i];
      asic->regs.overlayYoffset[i] = preProcess->overlaySliceYoffset[i];
      asic->regs.overlayYStride[i] = preProcess->overlayYStride[i];
      asic->regs.overlayUVStride[i] = preProcess->overlayUVStride[i];

      //May be edited in EncPreProcess fro chropping
      asic->regs.overlayYAddr[i] = pEncIn->busOlLum[i];
      asic->regs.overlayUAddr[i] = pEncIn->busOlCb[i];
      asic->regs.overlayVAddr[i] = pEncIn->busOlCr[i];
      asic->regs.overlayWidth[i] = preProcess->overlayWidth[i];
      asic->regs.overlayHeight[i] = preProcess->overlayHeight[i];
    }

    if(jpeg->codingType == ENC_WHOLE_FRAME)
    {
        /* Adjust ASIC input image with pre-processing */
		preProcess->sliced_frame = 0;
        EncPreProcess(asic, preProcess);
    }
    else
    {
        /* Set frame dimensions in slice mode for pre-processing */
        if((jpeg->row + jpeg->sliceRows) <= (jpeg->height / heightMcu))
        {
            preProcess->lumHeight = (heightMcu * jpeg->sliceRows);
        }
        else
        {
            preProcess->lumHeight = (jpeg->height % (jpeg->sliceRows * heightMcu));
        }

        if (preProcess->inputFormat!=JPEGENC_YUV420_8BIT_DAHUA_H264)
            preProcess->verOffsetSrc = 0;
        else
        {
            preProcess->verOffsetSrc = jpeg->sliceNum * jpeg->sliceRows * heightMcu;
            preProcess->sliced_frame = 1;
        }

        /* Check if we need to update height for last slice (for ASIC) */
        if(((jpeg->height / heightMcu) - jpeg->row) < jpeg->sliceRows)
        {
            asic->regs.mbsInCol = (((jpeg->height + heightMcu-1) / heightMcu) - jpeg->row);
        }

        /* enable EOI writing in last slice */
        if((jpeg->row + jpeg->sliceRows) >= ((jpeg->height + heightMcu-1) / heightMcu))
        {
            asic->regs.jpegSliceEnable = 0;
        }

        /* Adjust ASIC input image with pre-processing */
        EncPreProcess(asic, preProcess);
    }

    asic->regs.picWidth = asic->regs.mbsInRow*16;
    asic->regs.picHeight = asic->regs.mbsInCol*heightMcu;

#ifdef TRACE_STREAM
    traceStream.id = 0; /* Stream generated by SW */
    traceStream.bitCnt = 0;  /* New frame */
#endif

    /* Enable/disable jfif header generation */
    if(pEncIn->frameHeader)
        jpeg->frame.header = ENCHW_YES;
    else
        jpeg->frame.header = ENCHW_NO;

    VCDec400data dec400_data;
    dec400_data.format = preProcess->inputFormat;
    dec400_data.lumWidthSrc = preProcess->lumWidthSrc;
    dec400_data.lumHeightSrc = preProcess->lumHeightSrc;
    dec400_data.input_alignment = preProcess->input_alignment;
    dec400_data.dec400LumTableBase = pEncIn->dec400TableBusLum;
    dec400_data.dec400CbTableBase = pEncIn->dec400TableBusCb;
    dec400_data.dec400CrTableBase = pEncIn->dec400TableBusCr;
    pEncInst->dec400Enable = pEncIn->dec400Enable;

    if (pEncIn->dec400Enable == 1 && VCEncEnableDec400(asic,&dec400_data) == JPEGENC_INVALID_ARGUMENT)
    {
      printf("JpegEncEncodeRun: DEC400 doesn't exist or format not supported\n");
      EWLReleaseHw(pEncInst->asic.ewl);
      return JPEGENC_INVALID_ARGUMENT;
    }

    /* Encode one image or one slice */
    EncJpegCodeFrameRun(pEncInst);
    pEncOut->invalidBytesInBuf0Tail = pEncInst->invalidBytesInBuf0Tail;

    return JPEGENC_OK;

enc_err:    
    hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
    return ret;
}

/* Wait HW status */
JpegEncRet JpegEncEncodeWait(JpegEncInst inst, 
                               JpegEncOut * pEncOut)
{
    jpegInstance_s *pEncInst = (jpegInstance_s *) inst;
    jpegData_s *jpeg;
    asicData_s *asic;
    jpegEncodeFrame_e ret;

    /* Encode one image or one slice */
    ret = EncJpegCodeFrameWait(pEncInst);

    if(ret != JPEGENCODE_OK)
    {
        /* Error has occured and the frame is invalid */
        JpegEncRet to_user;

        /* Error has occured and the image is invalid.
         * The image size is passed to the user and can be used for debugging */
        pEncOut->jfifSize = (i32) pEncInst->stream.byteCnt;

        switch (ret)
        {
        case JPEGENCODE_TIMEOUT:
            APITRACE("JpegEncEncode: ERROR HW timeout");
            to_user = JPEGENC_HW_TIMEOUT;
            break;
        case JPEGENCODE_HW_RESET:
            APITRACE("JpegEncEncode: ERROR HW reset detected");
            to_user = JPEGENC_HW_RESET;
            break;
        case JPEGENCODE_HW_ERROR:
            APITRACE("JpegEncEncode: ERROR HW failure");
            to_user = JPEGENC_HW_BUS_ERROR;
            break;
        case JPEGENCODE_SYSTEM_ERROR:
        default:
            /* System error has occured, encoding can't continue */
            pEncInst->encStatus = ENCSTAT_ERROR;
            APITRACE("JpegEncEncode: ERROR Fatal system error");
            to_user = JPEGENC_SYSTEM_ERROR;
        }

        hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
        return to_user;
    }

    asic = &pEncInst->asic;
    jpeg = &pEncInst->jpeg;
    
    /* Store the stream size in output structure */
    pEncOut->jfifSize = (i32) pEncInst->stream.byteCnt;

    /* Check for stream buffer overflow */
    if(pEncInst->stream.overflow == ENCHW_YES)
    {
        /* The rest of the frame is lost */
        jpeg->sliceNum = 0;
        APITRACE("JpegEncEncode: ERROR stream buffer overflow");
        hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
        return JPEGENC_OUTPUT_BUFFER_OVERFLOW;
    }

    if(pEncInst->rateControl.picRc)
    {
      VCEncAfterPicRc(&pEncInst->rateControl, 0, pEncInst->stream.byteCnt, jpeg->qp, 1);
      if(pEncInst->rateControl.codingType  == ASIC_MJPEG)
         pEncInst->timeIncrement = pEncInst->rateControl.outRateDenom;
    }

    hash_reset(&pEncInst->jpeg.hashctx, asic->regs.hashval, asic->regs.hashoffset);

    /* Check if this is end of slice or end of frame */
    if(jpeg->mbNum < jpeg->mbPerFrame)
    {
        jpeg->sliceNum++;
        APITRACE("JpegEncEncode: OK  restart interval");
        return JPEGENC_RESTART_INTERVAL;
    }
    asic->regs.hashval = hash_finalize(&pEncInst->jpeg.hashctx);
    hash_init(&pEncInst->jpeg.hashctx,  pEncInst->jpeg.hashctx.hash_type);
    jpeg->sliceNum = 0;
    jpeg->rstCount = 0;

    APITRACE("JpegEncEncode: OK  frame ready");
    if(asic->regs.hashtype == 1)
      printf("crc32 %08x\n", asic->regs.hashval);
    else if(asic->regs.hashtype == 2)
      printf("checksum %08x\n", asic->regs.hashval);
    return JPEGENC_FRAME_READY;
}

/*******************************************************************************
 Function name : JpegEncEncode
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncIn * pEncIn
 Argument      : JpegEncOut *pEncOut
*******************************************************************************/
JpegEncRet JpegEncEncode(JpegEncInst inst, const JpegEncIn * pEncIn,
                               JpegEncOut * pEncOut)
{
    JpegEncRet ret;

    APITRACE("JpegEncEncode#");
    ret = JpegEncEncodeRun(inst, pEncIn, pEncOut);

    if (ret!=JPEGENC_OK)
    {
        return ret;
    }

    return JpegEncEncodeWait(inst, pEncOut);
}

/*******************************************************************************
 Function name : JpegEncGetBitsPerPixel
 Description   : Returns the amount of bits per pixel for given format.
 Return type   : u32 bitsPerPixel
 Argument      : JpegEncFrameType
*******************************************************************************/
u32 JpegEncGetBitsPerPixel(JpegEncFrameType type)
{
    switch (type)
    {
        case JPEGENC_YUV420_PLANAR:
        case JPEGENC_YUV420_SEMIPLANAR:
        case JPEGENC_YUV420_SEMIPLANAR_VU:
        case JPEGENC_YUV420_8BIT_DAHUA_HEVC:
        case JPEGENC_YUV420_8BIT_DAHUA_H264:
            return 12;
        case JPEGENC_YUV420_I010:
        case JPEGENC_YUV420_MS_P010:
            return 24;
        case JPEGENC_YUV422_INTERLEAVED_YUYV:
        case JPEGENC_YUV422_INTERLEAVED_UYVY:
        case JPEGENC_RGB565:
        case JPEGENC_BGR565:
        case JPEGENC_RGB555:
        case JPEGENC_BGR555:
        case JPEGENC_RGB444:
        case JPEGENC_BGR444:
        case JPEGENC_YUV422_888:
            return 16;
        case JPEGENC_RGB888:
        case JPEGENC_BGR888:
        case JPEGENC_RGB101010:
        case JPEGENC_BGR101010:
            return 32;
        default:
            return 0;
    }
}

/*******************************************************************************
 Function name : CheckFullSize
 Description   : Check that full image size is valid
 Return type   : JPEGENC_OK for success
 Argument      : JpegEncCfg
*******************************************************************************/
i32 CheckFullSize(const JpegEncCfg * pCfgFull)
{
    if((pCfgFull->inputWidth > 32768) || (pCfgFull->inputHeight > 32768))
    {
        return JPEGENC_ERROR;
    }

    if((pCfgFull->codingWidth < 32) || (pCfgFull->codingWidth > (2048 * 16)))
    {
        return JPEGENC_ERROR;
    }

    if((pCfgFull->codingHeight < 32) || (pCfgFull->codingHeight > (2048 * 16)))
    {
        return JPEGENC_ERROR;
    }

    if(((pCfgFull->codingWidth + 15) >> 4) *
       ((pCfgFull->codingHeight + 15) >> 4) > JPEGENC_MAX_SIZE)
    {
        return JPEGENC_ERROR;
    }

    if((pCfgFull->codingWidth & (1)) != 0)
    {
        return JPEGENC_ERROR;
    }

    if((pCfgFull->codingHeight & (1)) != 0)
    {
        return JPEGENC_ERROR;
    }

    if((pCfgFull->inputWidth & (15)) != 0)
    {
        return JPEGENC_ERROR;
    }

    if(pCfgFull->inputWidth < ((pCfgFull->codingWidth + 15) & (~15)))
    {
        return JPEGENC_ERROR;
    }

    return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetAlignedStride
    Description   : Returns the stride in byte by given aligment and format.
    Return type   : u32
------------------------------------------------------------------------------*/
u32 JpegEncGetAlignedStride(int width, i32 input_format, u32 *luma_stride, u32 *chroma_stride,u32 input_alignment)
{
  return EncGetAlignedByteStride(width, input_format, luma_stride, chroma_stride, input_alignment);
}

/*******************************************************************************
 Function name : CheckThumbnailCfg
 Description   : Check that thumbnail data is valid
 Return type   : JPEGENC_OK for success
 Argument      : JpegEncCfg
*******************************************************************************/
i32 CheckThumbnailCfg(const JpegEncThumb * pCfgThumb)
{
    u16 dataLength;
    if(pCfgThumb->width < 16)  /* max size limit by data range, 8-bits */
    {
        return JPEGENC_ERROR;
    }
    if(pCfgThumb->height < 16)  /* max size limit by data range, 8-bits */
    {
        return JPEGENC_ERROR;
    }
    if(pCfgThumb->data == NULL)
    {
        return JPEGENC_ERROR;
    }

    switch(pCfgThumb->format)
    {
    case JPEGENC_THUMB_JPEG:
        {
            dataLength = ((1<<16) - 1) - 8; /* 16 bits minus the APP0 ext field count */
            if(pCfgThumb->dataLength > dataLength)
                return JPEGENC_ERROR;
        }
        break;
    case  JPEGENC_THUMB_PALETTE_RGB8:
        {
            dataLength = 3*256 + (pCfgThumb->width*pCfgThumb->height);
            if((dataLength > (((1<<16) - 1) - 10)) || /* 16 bits minus the APP0 ext field count */
               (pCfgThumb->dataLength != dataLength))
                return JPEGENC_ERROR;
        }
        break;
    case JPEGENC_THUMB_RGB24:
        {
            dataLength = (3*pCfgThumb->width*pCfgThumb->height);
            if((dataLength > (((1<<16) - 1) - 10)) || /* 16 bits minus the APP0 ext field count */
               (pCfgThumb->dataLength != dataLength))
                return JPEGENC_ERROR;
        }
        break;
    default:
        return JPEGENC_ERROR;
    }

    return JPEGENC_OK;
}

/*******************************************************************************
 Function name : CheckJpegCfg
 Description   : Check that all values in the input structure are valid
 Return type   : bool_e
 Argument      : JpegEncCfg
*******************************************************************************/
bool_e CheckJpegCfg(const JpegEncCfg * pEncCfg)
{
    /* check HW limitations */
    u32 i = 0;
    i32 core_id = -1;
 
    for (i=0;i<EWLGetCoreNum();i++)
    {
        EWLHwConfig_t cfg = EWLReadAsicConfig(i);

        /* is JPEG encoding supported */
        if(cfg.jpegEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
        {
            continue;
        }
        /* is color conversion supported */
        if(cfg.rgbEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
           pEncCfg->frameType > JPEGENC_YUV422_INTERLEAVED_UYVY &&
           pEncCfg->frameType < JPEGENC_YUV420_8BIT_DAHUA_HEVC)
        {
            continue;
        }
        if(cfg.jpeg422Support == EWL_HW_CONFIG_NOT_SUPPORTED && pEncCfg->codingMode == JPEGENC_422_MODE && pEncCfg->losslessEn == 0)
        {
            continue;
        }
        if(cfg.streamMultiSegment == EWL_HW_CONFIG_NOT_SUPPORTED && pEncCfg->streamMultiSegmentMode != 0)
        {
            continue;
        }
        if(cfg.cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED && pEncCfg->colorConversion.type >= JPEGENC_RGBTOYUV_BT601_FULL_RANGE)
        {
            continue;
        }
        if ((pEncCfg->colorConversion.type == JPEGENC_RGBTOYUV_USER_DEFINED) && 
            (cfg.cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED) &&
            ((pEncCfg->colorConversion.coeffG != pEncCfg->colorConversion.coeffE) ||
             (pEncCfg->colorConversion.coeffH != pEncCfg->colorConversion.coeffF) ||
             (pEncCfg->colorConversion.LumaOffset != 0)))
        {
            continue;
        }

        core_id = i;
        
        /* is LJPEG supported */
        if(cfg.ljpegSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
           pEncCfg->losslessEn !=0)
        {
            core_id = -1;
        }
        else
          break;
    }
    
    if (core_id < 0)
        return ENCHW_NOK;

    if(pEncCfg->qLevel > 10)
        return ENCHW_NOK;

    if(pEncCfg->frameType > JPEGENC_YUV420_10BIT_TILE_8_8)
        return ENCHW_NOK;

    if(pEncCfg->mirror > 2)
        return ENCHW_NOK;

    if(pEncCfg->codingType != JPEGENC_WHOLE_FRAME &&
       pEncCfg->codingType != JPEGENC_SLICED_FRAME)
        return ENCHW_NOK;

    /* only support internally YUV420 or MONO */
    if(pEncCfg->codingMode != JPEGENC_420_MODE &&
       pEncCfg->codingMode != JPEGENC_422_MODE &&
       pEncCfg->codingMode != JPEGENC_MONO_MODE)
        return ENCHW_NOK;

    /* Units type must be valid */
    if(pEncCfg->unitsType != JPEGENC_NO_UNITS &&
       pEncCfg->unitsType != JPEGENC_DOTS_PER_INCH &&
       pEncCfg->unitsType != JPEGENC_DOTS_PER_CM)
        return ENCHW_NOK;

    /* Xdensity and Ydensity must valid */
    if((pEncCfg->xDensity > 0xFFFFU) ||
       (pEncCfg->yDensity > 0xFFFFU))
        return ENCHW_NOK;

    /* COM header length */
    if((pEncCfg->comLength > 0xFFFDU) ||
       (pEncCfg->comLength  != 0 && pEncCfg->pCom == NULL))
        return ENCHW_NOK;

    /* Marker type must be valid */
    if(pEncCfg->markerType != JPEGENC_SINGLE_MARKER &&
       pEncCfg->markerType != JPEGENC_MULTI_MARKER)
        return ENCHW_NOK;

    /* Input Line buffer check */
    if(pEncCfg->inputLineBufEn)
    {
       /* check zero depth */
       if ((pEncCfg->inputLineBufDepth == 0 || pEncCfg->amountPerLoopBack == 0) &&
           (pEncCfg->inputLineBufLoopBackEn || pEncCfg->inputLineBufHwModeEn))
          return ENCHW_NOK;

       /* not support ratation */
       if (pEncCfg->rotation)
           return ENCHW_NOK;
    }

    if(pEncCfg->streamMultiSegmentMode > 2 || (pEncCfg->streamMultiSegmentMode != 0 && (pEncCfg->streamMultiSegmentAmount <= 1 || 
       pEncCfg->streamMultiSegmentAmount > 16)))
    {
       return ENCHW_NOK;
    }

    if(pEncCfg->streamMultiSegmentMode != 0 && pEncCfg->codingType == JPEGENC_SLICED_FRAME)
    {
       return ENCHW_NOK;
    }

    if (pEncCfg->constChromaEn)
    {
      u32 maxCh = 255;
      if ((pEncCfg->constCb > maxCh) || (pEncCfg->constCr > maxCh))
        return ENCHW_NOK;
    }
    /*stride*/
    if (pEncCfg->exp_of_input_alignment<4 && pEncCfg->exp_of_input_alignment>0)
        return ENCHW_NOK;
    
    /*Motion JPEG and RC check. Not support slice mode*/
    if ((pEncCfg->frameRateNum != pEncCfg->frameRateDenom)&& (pEncCfg->codingType == JPEGENC_SLICED_FRAME))
        return ENCHW_NOK;

    if (pEncCfg->qpmin > 51 || pEncCfg->qpmax > 51 || pEncCfg->fixedQP > 51 || pEncCfg->fixedQP < -1)
        return ENCHW_NOK;

    if (pEncCfg->targetBitPerSecond && pEncCfg->fixedQP != -1)
        return ENCHW_NOK;

    /* Overlay check */
    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      if(pEncCfg->olEnable[i])
      {
        if(pEncCfg->olWidth[i] == 0 || pEncCfg->olHeight[i] == 0)
        {
          return ENCHW_NOK;
        }

        if(pEncCfg->olWidth[i] & 1 || pEncCfg->olHeight[i] & 1 || pEncCfg->olXoffset[i] & 1 ||
           pEncCfg->olYoffset[i] & 1)
        {
          return ENCHW_NOK;
        }

        if(pEncCfg->olXoffset[i] + pEncCfg->olCropWidth[i] > pEncCfg->xOffset + pEncCfg->codingWidth ||
           pEncCfg->olYoffset[i] + pEncCfg->olCropHeight[i] > pEncCfg->yOffset + pEncCfg->codingHeight)
        {
          return ENCHW_NOK;
        }

        if(pEncCfg->olCropXoffset[i] + pEncCfg->olCropWidth[i] > pEncCfg->olWidth[i] ||
           pEncCfg->olCropYoffset[i] + pEncCfg->olCropHeight[i] > pEncCfg->olHeight[i])
        {
          return ENCHW_NOK;
        }
      }
    }

    return ENCHW_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetEncodedMbLines
    Description   : Get how many MB lines has been encoded by encoder.
    Return type   : u32
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 JpegEncGetEncodedMbLines(JpegEncInst inst)
{
	jpegInstance_s *pEncInst = (jpegInstance_s *) inst;
	u32 lines;

	APITRACE("JpegEncGetEncodedMbLines#");

	/* Check for illegal inputs */
	if (!pEncInst) {
		APITRACE("JpegEncGetEncodedMbLines: ERROR Null argument");
		return JPEGENC_NULL_ARGUMENT;
	}

	if (!pEncInst->inputLineBuf.inputLineBufEn) {
		APITRACE("JpegEncGetEncodedMbLines: ERROR Invalid mode for input control");
		return JPEGENC_INVALID_ARGUMENT;
	}

    lines = EncAsicGetRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror, HWIF_CTB_ROW_RD_PTR);
    lines += EncAsicGetRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror, HWIF_CTB_ROW_RD_PTR_JPEG_MSB)<<10;
	return lines;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncSetInputMBLines
    Description   : Set the input buffer lines available of current picture.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance
    Argument      : lines - number of macroblock lines
------------------------------------------------------------------------------*/
JpegEncRet JpegEncSetInputMBLines(JpegEncInst inst, u32 lines)
{
	jpegInstance_s *pEncInst = (jpegInstance_s *) inst;

	APITRACE("JpegEncSetInputMBLines#");

	/* Check for illegal inputs */
	if (!pEncInst) {
		APITRACE("JpegEncSetInputMBLines: ERROR Null argument");
		return JPEGENC_NULL_ARGUMENT;
	}

	if (!pEncInst->inputLineBuf.inputLineBufEn) {
		APITRACE("JpegEncSetInputMBLines: ERROR Invalid mode for input control");
		return JPEGENC_INVALID_ARGUMENT;
	}

	EncAsicWriteRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror, HWIF_CTB_ROW_WR_PTR, lines&0x3ff);
	EncAsicWriteRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror, HWIF_CTB_ROW_WR_PTR_JPEG_MSB, lines>>10);
	return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetLumaSize
    Description   : Get luma size.
    Return type   : luma size
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
JpegEncRet JpegGetLumaSize(JpegEncInst inst, u64 *lumaSize, u64 *dec400LumaTableSize)
{
    /* Check for illegal inputs */
	if (!inst) {
		APITRACE("JpegGetLumaSize: ERROR Null argument");
        return JPEGENC_NULL_ARGUMENT;
	}
    if (lumaSize != NULL)
      *lumaSize = ((jpegInstance_s *)inst)->lumaSize;
    if (dec400LumaTableSize != NULL)
      *dec400LumaTableSize = ((jpegInstance_s *)inst)->dec400LumaTableSize;
    return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegSetLumaSize
    Description   : Set luma size.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance, lumaSize - luma size to set
------------------------------------------------------------------------------*/
JpegEncRet JpegSetLumaSize(JpegEncInst inst, u64 lumaSize, u64 dec400LumaTableSize)
{
     /* Check for illegal inputs */
	if (!inst) {
		APITRACE("JpegSetLumaSize: ERROR Null argument");
		return JPEGENC_NULL_ARGUMENT;
	}
	((jpegInstance_s *)inst)->lumaSize = lumaSize;
    ((jpegInstance_s *)inst)->dec400LumaTableSize = dec400LumaTableSize;
	return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetChromaSize
    Description   : Get chroma size.
    Return type   : chroma size
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
JpegEncRet JpegGetChromaSize(JpegEncInst inst, u64 *chromaSize, u64 *dec400ChrTableSize)
{
    /* Check for illegal inputs */
	if (!inst) {
		APITRACE("JpegGetChromaSize: ERROR Null argument");
		return -1;
	}
    if (chromaSize != NULL)
      *chromaSize = ((jpegInstance_s *)inst)->chromaSize;
    if (dec400ChrTableSize != NULL)
      *dec400ChrTableSize = ((jpegInstance_s *)inst)->dec400ChrTableSize;
    return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegSetChromaSize
    Description   : Set chroma size.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance, chromaSize - chroma size to set
------------------------------------------------------------------------------*/
JpegEncRet JpegSetChromaSize(JpegEncInst inst, u64 chromaSize, u64 dec400ChrTableSize)
{
    /* Check for illegal inputs */
	if (!inst) {
		APITRACE("JpegSetChromaSize: ERROR Null argument");
		return JPEGENC_NULL_ARGUMENT;
	}
	((jpegInstance_s *)inst)->chromaSize = chromaSize;
    ((jpegInstance_s *)inst)->dec400ChrTableSize = dec400ChrTableSize;
    return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetQpHdr
    Description   : Get qp hdr.
    Return type   : qp hdr
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
int JpegGetQpHdr(JpegEncInst inst)
{
    /* Check for illegal inputs */
	if (!inst) {
		APITRACE("JpegGetQpHdr: ERROR Null argument");
		return -1;
	}
	return ((jpegInstance_s *)inst)->rateControl.qpHdr;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetEwl
    Description   : Get the ewl instance.
    Return type   : ewl instance pointer
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
const void *JpegEncGetEwl(JpegEncInst inst)
{
    const void * ewl;
    if(inst == NULL)
    {
      APITRACE("JpegEncGetEwl: ERROR Null argument");
      ASSERT(0);
    }
    ewl = ((jpegInstance_s *)inst)->asic.ewl;
    if(ewl == NULL)
    { 
        APITRACE("JpegEncGetEwl: EWL instance get failed.");
        ASSERT(0);
    }
    return ewl;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetPerformance
    Description   : Get JPEG encoding HW performance.
    Return type   : u32
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 JpegEncGetPerformance(JpegEncInst inst)
{
  const void *ewl;
  u32 performanceData;
  ASSERT(inst);
  APITRACE("JpegEncGetPerformance#");
  /* Check for illegal inputs */
  if (inst == NULL)
  {
    APITRACE("JpegEncGetPerformance: ERROR Null argument");
    return JPEGENC_NULL_ARGUMENT;
  }

  ewl = ((jpegInstance_s *)inst)->asic.ewl;
  performanceData = EncAsicGetPerformance(ewl);
  APITRACE("JpegEncGetPerformance:OK");
  return performanceData;
}

