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
--------------------------------------------------------------------------------
--
--  Abstract : Encoder initialization and setup
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vp9init.h"
#include "vp9macroblocktools.h"
#include "vp9codeframe.h"
#include "enccommon.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
#define VP9ENC_MIN_ENC_WIDTH       130     /* 144 - 12 pixels overfill */
#define VP9ENC_MAX_ENC_WIDTH       4096
#define VP9ENC_MIN_ENC_HEIGHT      130
#define VP9ENC_MAX_ENC_HEIGHT      4096

#define VP9ENC_MAX_MBS_PER_PIC     65536   /* 4096x4096 */

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static bool_e SetParameter(vp9Instance_s *inst,
                           const VP9EncConfig *pEncCfg);
static bool_e CheckParameter(vp9Instance_s *inst);

static i32 SetPictureBuffer(vp9Instance_s *inst);

static void   StatFree(vp9Instance_s *inst);
static bool_e StatAlloc(vp9Instance_s *inst);

/*------------------------------------------------------------------------------

    VP9CheckCfg

    Function checks that the configuration is valid.

    Input   pEncCfg Pointer to configuration structure.

    Return  ENCHW_OK      The configuration is valid.
            ENCHW_NOK     Some of the parameters in configuration are not valid.

------------------------------------------------------------------------------*/
bool_e VP9CheckCfg(const VP9EncConfig *pEncCfg)
{
  ASSERT(pEncCfg);
  EWLHwConfig_t cfg = EWLReadAsicConfig();

  /* Encoded image width limits, multiple of 4 */
  if (pEncCfg->width < VP9ENC_MIN_ENC_WIDTH ||
      pEncCfg->width > VP9ENC_MAX_ENC_WIDTH || (pEncCfg->width & 0x3) != 0)
    return ENCHW_NOK;

  /* Encoded image height limits, multiple of 2 */
  if (pEncCfg->height < VP9ENC_MIN_ENC_HEIGHT ||
      pEncCfg->height > VP9ENC_MAX_ENC_HEIGHT || (pEncCfg->height & 0x1) != 0)
    return ENCHW_NOK;
#if 0
  /* Scaled image width limits, multiple of 4 (YUYV) and smaller than input */
  if ((pEncCfg->scaledWidth > pEncCfg->width) ||
      (pEncCfg->scaledWidth & 0x3) != 0)
    return ENCHW_NOK;

  if ((pEncCfg->scaledHeight > pEncCfg->height) ||
      (pEncCfg->scaledHeight & 0x1) != 0)
    return ENCHW_NOK;

  if ((pEncCfg->scaledWidth == pEncCfg->width) &&
      (pEncCfg->scaledHeight == pEncCfg->height))
    return ENCHW_NOK;
#endif

  /* total macroblocks per picture limit */
  if (((pEncCfg->height + 15) / 16) * ((pEncCfg->width + 15) / 16) >
      VP9ENC_MAX_MBS_PER_PIC)
  {
    return ENCHW_NOK;
  }

  /* Check frame rate */
  if (pEncCfg->frameRateNum < 1 || pEncCfg->frameRateNum > ((1 << 20) - 1))
    return ENCHW_NOK;

  if (pEncCfg->frameRateDenom < 1)
  {
    return ENCHW_NOK;
  }

  /* special allowal of 1000/1001, 0.99 fps by customer request */
  if (pEncCfg->frameRateDenom > pEncCfg->frameRateNum &&
      !(pEncCfg->frameRateDenom == 1001 && pEncCfg->frameRateNum == 1000))
  {
    return ENCHW_NOK;
  }

  /* check HW limitations */
  if (0) // not check config at first
  {
    /* is VP9 encoding supported */
    if (cfg.vp9Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      return ENCHW_NOK;
    }
  }

  /* max width supported */
  if (cfg.maxEncodedWidth < pEncCfg->width)
  {
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    StatAlloc

    Allocate memories for statistics stuff.

    Input   inst  Pointer to instance.

    Return  ENCHW_OK      The configuration is valid.
            ENCHW_NOK     Some of the parameters in configuration are not valid.

------------------------------------------------------------------------------*/
bool_e StatAlloc(vp9Instance_s *inst)
{

  ASSERT(inst);

  /* Allocate mv accumulators; 1 per MB */
  inst->statPeriod.mvSumX = EWLcalloc(inst->mbPerFrame, sizeof(i32));
  inst->statPeriod.mvSumY = EWLcalloc(inst->mbPerFrame, sizeof(i32));
  if (inst->statPeriod.mvSumX == NULL ||
      inst->statPeriod.mvSumY == NULL)
  {
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    StatFree

    Free memories used by statistics stuff.

    Input   inst  Pointer to instance.

------------------------------------------------------------------------------*/
void StatFree(vp9Instance_s *inst)
{
  if (inst->statPeriod.mvSumX)
  {
    EWLfree(inst->statPeriod.mvSumX);
    inst->statPeriod.mvSumX = NULL;
  }

  if (inst->statPeriod.mvSumY)
  {
    EWLfree(inst->statPeriod.mvSumY);
    inst->statPeriod.mvSumY = NULL;
  }
}

/*------------------------------------------------------------------------------

    VP9Init

    Function initializes the Encoder and create new encoder instance.

    Input   pEncCfg     Encoder configuration.
            instAddr    Pointer to instance will be stored in this address

    Return  VP9ENC_OK
            VP9ENC_MEMORY_ERROR
            VP9ENC_EWL_ERROR
            VP9ENC_EWL_MEMORY_ERROR
            VP9ENC_INVALID_ARGUMENT

------------------------------------------------------------------------------*/
VP9EncRet VP9Init(const VP9EncConfig *pEncCfg, vp9Instance_s **instAddr)
{
  vp9Instance_s *inst = NULL;
  const void *ewl = NULL;
  VP9EncRet ret = VP9ENC_OK;
  EWLInitParam_t param;
  u32 i;

  ASSERT(pEncCfg);
  ASSERT(instAddr);

  *instAddr = NULL;

  param.clientType = EWL_CLIENT_TYPE_VP9_ENC;

  /* Init EWL */
  if ((ewl = EWLInit(&param)) == NULL)
    return VP9ENC_EWL_ERROR;

  /* Encoder instance */
  inst = (vp9Instance_s *) EWLcalloc(1, sizeof(vp9Instance_s));

  if (inst == NULL)
  {
    ret = VP9ENC_MEMORY_ERROR;
    goto err;
  }

  /* Set parameters depending on user config */
  if (SetParameter(inst, pEncCfg) != ENCHW_OK)
  {
    ret = VP9ENC_INVALID_ARGUMENT;
    goto err;
  }

  /* Check and init the rest of parameters */
  if (CheckParameter(inst) != ENCHW_OK)
  {
    ret = VP9ENC_INVALID_ARGUMENT;
    goto err;
  }

  if (SetPictureBuffer(inst) != ENCHW_OK)
  {
    ret = VP9ENC_INVALID_ARGUMENT;
    goto err;
  }

  VP9InitRc(&inst->rateControl, 1);

  /* Initialize ASIC */
  inst->asic.ewl = ewl;
  (void) EncAsicControllerInit(&inst->asic);

  /* Allocate internal SW/HW shared memories */
  if (EncAsicMemAlloc_V2(&inst->asic,
                         (u32) inst->preProcess.lumWidth,
                         (u32) inst->preProcess.lumHeight,
                         (u32) 0,
                         (u32) 0,
                         ASIC_VP9, inst->numRefBuffsLum,
                         inst->numRefBuffsLum,
                         pEncCfg->compressor,0,0,0,0) != ENCHW_OK)
  {
    ret = VP9ENC_EWL_MEMORY_ERROR;
    goto err;
  }

  /* Allocate MV accumulators for statistics */
  if (StatAlloc(inst) != ENCHW_OK)
  {
    ret = VP9ENC_EWL_MEMORY_ERROR;
    goto err;
  }
  EWLmemset(inst->statPeriod.mvSumX, 0, inst->mbPerFrame * sizeof(i32));
  EWLmemset(inst->statPeriod.mvSumY, 0, inst->mbPerFrame * sizeof(i32));

  /* Assign allocated HW frame buffers into picture buffer */
  inst->picBuffer.size = inst->numRefBuffsLum;
  for (i = 0; i < (inst->numRefBuffsLum + 1); i++)
    inst->picBuffer.refPic[i].picture.lum = inst->asic.internalreconLuma[i].busAddress;
  for (i = 0; i < (inst->numRefBuffsLum + 1); i++)
    inst->picBuffer.refPic[i].picture.cb = inst->asic.internalreconChroma[i].busAddress;

  *instAddr = inst;

  //inst->asic.regs.zeroMvFavorDiv2 = 0; /* No favor for VP9 */
  //    inst->asic.regs.deadzoneEnable = 1;  /* Enabled by default. */

  /* Disable intra and ROI areas by default */
  inst->asic.regs.intraAreaTop = inst->asic.regs.intraAreaBottom =
                                   inst->asic.regs.intraAreaLeft = inst->asic.regs.intraAreaRight =
                                         inst->asic.regs.roi1Top = inst->asic.regs.roi1Bottom =
                                             inst->asic.regs.roi1Left = inst->asic.regs.roi1Right =
                                                 inst->asic.regs.roi2Top = inst->asic.regs.roi2Bottom =
                                                     inst->asic.regs.roi2Left = inst->asic.regs.roi2Right = 255;

  //    VP9InitPenalties(inst);

  return ret;

err:

  if (inst != NULL)
    VP9Shutdown(inst);
  else if (ewl != NULL)
    (void) EWLRelease(ewl);

  return ret;
}

/*------------------------------------------------------------------------------

    VP9Shutdown

    Function frees the encoder instance.

    Input   vp9Instance_s *    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void VP9Shutdown(vp9Instance_s *data)
{
  const void *ewl;

  ASSERT(data);

  ewl = data->asic.ewl;

  StatFree(data);

  EncAsicMemFree_V2(&data->asic);

  PictureBufferFree(&data->picBuffer);

  PicParameterSetFree(&data->ppss);

  EncPreProcessFree(&data->preProcess);

  EWLfree(data);

  (void) EWLRelease(ewl);
}

/*------------------------------------------------------------------------------

    SetParameter

    Set all parameters in instance to valid values depending on user config.

------------------------------------------------------------------------------*/
bool_e SetParameter(vp9Instance_s *inst, const VP9EncConfig *pEncCfg)
{
  i32 width, height;
  sps *sps = &inst->sps;
  EWLHwConfig_t cfg = EWLReadAsicConfig();

  ASSERT(inst);

  /* Internal images, next macroblock boundary */
  width = 16 * ((pEncCfg->width + 15) / 16);
  height = 16 * ((pEncCfg->height + 15) / 16);

  /* Luma ref buffers can be read and written at the same time,
   * but chroma buffers must be one for reading and one for writing */
  inst->numRefBuffsLum    = pEncCfg->refFrameAmount;
  inst->numRefBuffsChr    = inst->numRefBuffsLum;

  /* Macroblock */
  inst->mbPerFrame        = width / 16 * height / 16;
  inst->mbPerRow          = width / 16;
  inst->mbPerCol          = height / 16;

  /* Sequence parameter set */
  sps->picWidthInPixel    = pEncCfg->width;
  sps->picHeightInPixel   = pEncCfg->height;
  sps->picWidthInMbs      = width / 16;
  sps->picHeightInMbs     = height / 16;

  sps->horizontalScaling = 0; /* TODO, not supported yet */
  sps->verticalScaling   = 0; /* TODO, not supported yet */
  sps->colorType         = 0; /* TODO, not supported yet */
  sps->clampType         = 0; /* TODO, not supported yet */
  sps->dctPartitions     = 0; /* Dct data partitions 0=1, 1=2, 2=4, 3=8 */
  sps->partitionCnt      = 2 + (1 << sps->dctPartitions);
  sps->profile           = 0; /* 0=bicubic, 1=bilinear */
  sps->filterType        = 0;
  sps->filterLevel       = 0;
  sps->filterSharpness   = 0;
  sps->autoFilterLevel     = 1; /* Automatic filter values by default. */
  sps->autoFilterSharpness = 1;
  sps->quarterPixelMv    = 1; /* 1=adaptive by default */
  sps->splitMv           = 1; /* 1=adaptive by default */
  sps->refreshEntropy    = 1; /* 0=default probs, 1=prev frame probs */
  sps->filterDeltaEnable = true;

  /* Rate control */
  inst->rateControl.virtualBuffer[0].bitRate = 1000000;
  inst->rateControl.virtualBuffer[0].outRateDenom = pEncCfg->frameRateDenom;
  inst->rateControl.qpHdr         = -1;
  inst->rateControl.picRc         = ENCHW_YES;
  inst->rateControl.picSkip       = ENCHW_NO;
  inst->rateControl.qpMin         = 0;
  inst->rateControl.qpMax         = 127;
  inst->rateControl.windowLen     = 150;
  inst->rateControl.mbPerPic      = inst->mbPerFrame;
  inst->rateControl.mbRows        = height / 16;
  inst->rateControl.outRateDenom  = pEncCfg->frameRateDenom;
  inst->rateControl.outRateNum    = pEncCfg->frameRateNum;

  inst->rateControl.goldenPictureRate = 15;
  inst->rateControl.adaptiveGoldenBoost = 25;
  inst->rateControl.adaptiveGoldenUpdate = 1;
  if (pEncCfg->refFrameAmount == 1) inst->rateControl.goldenPictureRate = 0;

  /* Simulated thresholds etc */
  inst->rateControl.goldenRefreshThreshold = 10;
  inst->rateControl.goldenBoostThreshold = 25;

  inst->statPeriod.skipDiv = 0;
  inst->statPeriod.skipCnt = 0;
  inst->statPeriod.intraCnt = 0;
  inst->statPeriod.goldenDiv = 0;
  inst->statPeriod.goldenCnt = 0;

  inst->maxNumPasses = 1;
  inst->qualityMetric = VP9ENC_QM_SSIM;

  /* Pre processing */
  inst->preProcess.lumWidth       = pEncCfg->width;
  inst->preProcess.lumWidthSrc    =
    VP9GetAllowedWidth(pEncCfg->width, VP9ENC_YUV420_PLANAR);

  inst->preProcess.lumHeight      = pEncCfg->height;
  inst->preProcess.lumHeightSrc   = pEncCfg->height;
#if 0
  inst->preProcess.scaledWidth    = pEncCfg->scaledWidth;
  inst->preProcess.scaledHeight   = pEncCfg->scaledHeight;
#endif
  /* Is HW scaling supported */
  if (cfg.scalingEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
    inst->preProcess.scaledWidth = inst->preProcess.scaledHeight = 0;

  inst->preProcess.scaledOutput   =
    (inst->preProcess.scaledWidth * inst->preProcess.scaledHeight ? 1 : 0);
  inst->preProcess.horOffsetSrc   = 0;
  inst->preProcess.verOffsetSrc   = 0;
  inst->preProcess.rotation       = ROTATE_0;
  inst->preProcess.inputFormat    = VP9ENC_YUV420_PLANAR;
  inst->preProcess.videoStab      = 0;
  inst->preProcess.adaptiveRoi    = 0;
  inst->preProcess.adaptiveRoiColor  = 0;
  inst->preProcess.adaptiveRoiMotion = -5;

  inst->preProcess.colorConversionType = 0;
  EncSetColorConversion(&inst->preProcess, &inst->asic);

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    CheckParameter

------------------------------------------------------------------------------*/
bool_e CheckParameter(vp9Instance_s *inst)
{
  /* Check crop */
  if (EncPreProcessCheck(&inst->preProcess) != ENCHW_OK)
    return ENCHW_NOK;

  /* Initialize quant tables */
  InitQuantTables(inst);

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    SetPictureBuffer

------------------------------------------------------------------------------*/
i32 SetPictureBuffer(vp9Instance_s *inst)
{
  picBuffer *picBuffer = &inst->picBuffer;
  sps *sps = &inst->sps;
  i32 width, height;

  width = sps->picWidthInMbs * 16;
  height = sps->picHeightInMbs * 16;
  if (PictureBufferAlloc(picBuffer, width, height) != ENCHW_OK)
    return ENCHW_NOK;

  width = sps->picWidthInMbs;
  height = sps->picHeightInMbs;
  if (PicParameterSetAlloc(&inst->ppss, width * height) != ENCHW_OK)
    return ENCHW_NOK;

  inst->ppss.pps = inst->ppss.store;
  inst->ppss.pps->segmentEnabled  = 0; /* Segmentation disabled by default. */
  inst->ppss.pps->sgm.mapModified = 0;
  inst->ppss.pps->sgmQpMapping[0] = 0;
  inst->ppss.pps->sgmQpMapping[1] = 1;
  inst->ppss.pps->sgmQpMapping[2] = 2;
  inst->ppss.pps->sgmQpMapping[3] = 3;

  if (EncPreProcessAlloc(&inst->preProcess, width * height) != ENCHW_OK)
    return ENCHW_NOK;

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    Round the width to the next multiple of 8 or 16 depending on YUV type.

------------------------------------------------------------------------------*/
i32 VP9GetAllowedWidth(i32 width, VP9EncPictureType inputType)
{
  if (inputType == VP9ENC_YUV420_PLANAR)
  {
    /* Width must be multiple of 16 to make
     * chrominance row 64-bit aligned */
    return ((width + 15) / 16) * 16;
  }
  else
  {
    /* VP9ENC_YUV420_SEMIPLANAR */
    /* VP9ENC_YUV422_INTERLEAVED_YUYV */
    /* VP9ENC_YUV422_INTERLEAVED_UYVY */
    return ((width + 7) / 8) * 8;
  }
}
