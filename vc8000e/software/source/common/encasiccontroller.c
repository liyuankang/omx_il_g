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
--  Description : ASIC low level controller
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/
#include <string.h>
#include "enccommon.h"
#include "encasiccontroller.h"
#include "encpreprocess.h"
#include "ewl.h"
#include "encswhwregisters.h"

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

extern void EncTraceRegs(const void *ewl, u32 readWriteFlag, u32 mbNum);

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#ifdef ASIC_WAVE_TRACE_TRIGGER
extern i32 trigger_point;    /* picture which will be traced */
#endif

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_7b         (u32)0x0000007F
#define mask_8b         (u32)0x000000FF
#define mask_9b         (u32)0x000001FF
#define mask_10b        (u32)0x000003FF
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF
#define mask_21b        (u32)0x001FFFFF

#define HSWREG(n)       ((n)*4)

/* JPEG QUANT table order */
static const u32 qpReorderTable[64] =
    { 0,  8, 16, 24,  1,  9, 17, 25, 32, 40, 48, 56, 33, 41, 49, 57,
      2, 10, 18, 26,  3, 11, 19, 27, 34, 42, 50, 58, 35, 43, 51, 59,
      4, 12, 20, 28,  5, 13, 21, 29, 36, 44, 52, 60, 37, 45, 53, 61,
      6, 14, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

/* Global variables for test data trace file configuration.
 * These are set in testbench and used in system model test data generation. */
char *H2EncTraceFileConfig = NULL;
int H2EncTraceFirstFrame    = 0;

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Initialize empty structure with default values.
------------------------------------------------------------------------------*/
i32 EncAsicControllerInit(asicData_s *asic,u32 client_type)
{
  i32 i;
  ASSERT(asic != NULL);

  /* Initialize default values from defined configuration */
  asic->regs.irqDisable = ENCH2_IRQ_DISABLE;
  asic->regs.inputReadChunk = ENCH2_INPUT_READ_CHUNK;
  asic->regs.asic_axi_readID = ENCH2_AXI_READ_ID;
  asic->regs.asic_axi_writeID = ENCH2_AXI_WRITE_ID;
  asic->regs.asic_stream_swap = ENCH2_OUTPUT_SWAP_8 | (ENCH2_OUTPUT_SWAP_16 << 1) | (ENCH2_OUTPUT_SWAP_32 << 2) | (ENCH2_OUTPUT_SWAP_64 << 3);
  asic->regs.scaledOutSwap = (ENCH2_SCALE_OUTPUT_SWAP_64 << 3) | (ENCH2_SCALE_OUTPUT_SWAP_32 << 2) | (ENCH2_SCALE_OUTPUT_SWAP_16 << 1) | ENCH2_SCALE_OUTPUT_SWAP_8;
  asic->regs.nalUnitSizeSwap = (ENCH2_NALUNIT_SIZE_SWAP_64<< 3) | (ENCH2_NALUNIT_SIZE_SWAP_32 << 2) | (ENCH2_NALUNIT_SIZE_SWAP_16 << 1) | ENCH2_NALUNIT_SIZE_SWAP_8;
  asic->regs.asic_roi_map_qp_delta_swap = ENCH2_ROIMAP_QPDELTA_SWAP_8 | (ENCH2_ROIMAP_QPDELTA_SWAP_16 << 1) | (ENCH2_ROIMAP_QPDELTA_SWAP_32 << 2) | (ENCH2_ROIMAP_QPDELTA_SWAP_64 << 3);
  asic->regs.asic_ctb_rc_mem_out_swap = ENCH2_CTBRC_OUTPUT_SWAP_8 | (ENCH2_CTBRC_OUTPUT_SWAP_16 << 1) | (ENCH2_CTBRC_OUTPUT_SWAP_32 << 2) | (ENCH2_CTBRC_OUTPUT_SWAP_64 << 3);
  asic->regs.asic_cu_info_mem_out_swap = ENCH2_CUINFO_OUTPUT_SWAP_8 | (ENCH2_CUINFO_OUTPUT_SWAP_16 << 1) | (ENCH2_CUINFO_OUTPUT_SWAP_32 << 2) | (ENCH2_CUINFO_OUTPUT_SWAP_64 << 3);
  asic->regs.asic_burst_length = ENCH2_BURST_LENGTH & (63);
  asic->regs.asic_burst_scmd_disable = ENCH2_BURST_SCMD_DISABLE & (1);
  asic->regs.asic_burst_incr = (ENCH2_BURST_INCR_TYPE_ENABLED & (1));

  asic->regs.asic_data_discard = ENCH2_BURST_DATA_DISCARD_ENABLED & (1);
  asic->regs.asic_clock_gating_encoder = ENCH2_ASIC_CLOCK_GATING_ENCODER_ENABLED & (1);
  asic->regs.asic_clock_gating_encoder_h265 = ENCH2_ASIC_CLOCK_GATING_ENCODER_H265_ENABLED & (1);
  asic->regs.asic_clock_gating_encoder_h264 = ENCH2_ASIC_CLOCK_GATING_ENCODER_H264_ENABLED & (1);
  asic->regs.asic_clock_gating_inter = ENCH2_ASIC_CLOCK_GATING_INTER_ENABLED & (1);
  asic->regs.asic_clock_gating_inter_h265 = ENCH2_ASIC_CLOCK_GATING_INTER_H265_ENABLED & (1);
  asic->regs.asic_clock_gating_inter_h264 = ENCH2_ASIC_CLOCK_GATING_INTER_H264_ENABLED & (1);
  asic->regs.asic_axi_dual_channel = ENCH2_AXI_2CH_DISABLE;



  //asic->regs.recWriteBuffer = ENCH2_REC_WRITE_BUFFER & 1;

  /* User must set these */
  asic->regs.inputLumBase = 0;
  asic->regs.inputCbBase = 0;
  asic->regs.inputCrBase = 0;
  asic->cuInfoTableSize = 0;
  for (i = 0; i < ASIC_FRAME_BUF_LUM_MAX; i ++)
  {
    asic->internalreconLuma[i].virtualAddress = NULL;
    asic->internalreconChroma[i].virtualAddress = NULL;
    asic->compressTbl[i].virtualAddress = NULL;
    asic->colBuffer[i].virtualAddress = NULL;
  }
  for (i = 0; i < ASIC_FRAME_BUF_CUINFO_MAX; i ++)
  {
    asic->cuInfoMem[i].virtualAddress = NULL;
  }
  asic->scaledImage.virtualAddress = NULL;
  for (i = 0; i < MAX_CORE_NUM; i++)
    asic->sizeTbl[i].virtualAddress = NULL;
  asic->cabacCtx.virtualAddress = NULL;
  asic->mvOutput.virtualAddress = NULL;
  asic->probCount.virtualAddress = NULL;
  asic->segmentMap.virtualAddress = NULL;
  for (i = 0; i < MAX_CORE_NUM; i++)
    asic->compress_coeff_SACN[i].virtualAddress = NULL;
  asic->loopbackLineBufMem.virtualAddress = NULL;

  /* get ASIC ID value */
  asic->regs.asicHwId = EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type));

  /* get ASIC Config */
  asic->regs.asicCfg = EncAsicGetAsicConfig(EncAsicGetCoreIdByFormat(client_type));

  /* we do NOT reset hardware at this point because */
  /* of the multi-instance support                  */

  return ENCHW_OK;
}



/*------------------------------------------------------------------------------

    EncAsicSetQuantTable

    Set new jpeg quantization table to be used by ASIC

------------------------------------------------------------------------------*/
void EncAsicSetQuantTable(asicData_s * asic,
                          const u8 * lumTable, const u8 * chTable)
{
    i32 i;

    ASSERT(lumTable);
    ASSERT(chTable);

    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[i] = lumTable[qpReorderTable[i]];
    }
    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[64 + i] = chTable[qpReorderTable[i]];
    }
}


/*------------------------------------------------------------------------------
    When the frame is successfully encoded the internal image is recycled
    so that during the next frame the current internal image is read and
    the new reconstructed image is written. Note that this is done for
    the NEXT frame.
------------------------------------------------------------------------------*/
#define NEXTID(currentId, maxId) ((currentId == maxId) ? 0 : currentId+1)
#define PREVID(currentId, maxId) ((currentId == 0) ? maxId : currentId-1)


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void CheckRegisterValues(regValues_s *val)
{
  ASSERT(val->irqDisable <= 1);
  ASSERT(val->filterDisable <= 2);
  ASSERT(val->qp <= 51);
  ASSERT(val->frameCodingType <= 2);
  ASSERT(val->codingType <= 4 || val->codingType == 7);
  //ASSERT(val->pixelsOnRow >= 16 && val->pixelsOnRow <= (val->codingType==ASIC_JPEG?16384:8192)); /* max input for cropping */
  ASSERT(val->xFill <= 7);
  ASSERT(val->yFill <= 14 && ((val->yFill & 0x01) == 0));
  ASSERT(val->inputLumaBaseOffset <= 15);
  ASSERT(val->inputChromaBaseOffset <= 15);
  ASSERT(val->inputImageFormat <= ASIC_INPUT_P010_TILE8);
  ASSERT(val->inputImageRotation <= 3);
  ASSERT(val->inputImageMirror<= 1);
  ASSERT(val->stabMode <= 2);
  ASSERT(val->outputBitWidthLuma <= 2);
  ASSERT(val->outputBitWidthChroma<= 2);

  if (val->codingType == ASIC_HEVC || 
      val->codingType == ASIC_H264 ||
      val->codingType == ASIC_AV1)
  {
    if (val->asicCfg.roiAbsQpSupport)
    {
      ASSERT(val->roi1DeltaQp >= -51 && val->roi1DeltaQp <= 51);
      ASSERT(val->roi2DeltaQp >= -51 && val->roi2DeltaQp <= 51);      
      ASSERT(val->roi1Qp <= 51);
      ASSERT(val->roi2Qp <= 51);
    }
    else
    {
      ASSERT(val->roi1DeltaQp >= 0 && val->roi1DeltaQp <= 30);
      ASSERT(val->roi2DeltaQp >= 0 && val->roi2DeltaQp <= 30);
    }
  }
  ASSERT(val->cirStart <= INVALID_AREA);
  ASSERT(val->cirInterval <= INVALID_AREA);
  ASSERT(val->intraAreaTop <= INVALID_POS);
  ASSERT(val->intraAreaLeft <= INVALID_POS);
  ASSERT(val->intraAreaBottom <= INVALID_POS);
  ASSERT(val->intraAreaRight <= INVALID_POS);
  ASSERT(val->roi1Top <= INVALID_POS);
  ASSERT(val->roi1Left <= INVALID_POS);
  ASSERT(val->roi1Bottom <= INVALID_POS);
  ASSERT(val->roi1Right <= INVALID_POS);
  ASSERT(val->roi2Top <= INVALID_POS);
  ASSERT(val->roi2Left <= INVALID_POS);
  ASSERT(val->roi2Bottom <= INVALID_POS);
  ASSERT(val->roi2Right <= INVALID_POS);
  (void) val;
}

/*------------------------------------------------------------------------------
    Function name   : EncAsicFrameStart
    Description     :
    Return type     : void
    Argument        : const void *ewl
    Argument        : regValues_s * val
------------------------------------------------------------------------------*/
void EncAsicFrameStart(const void *ewl, regValues_s *val,u32 dumpRegister)
{
  i32 i;
  u32 timeOutCycles;

  if (val->inputImageFormat < ASIC_INPUT_RGB565||(val->inputImageFormat >= ASIC_INPUT_DAHUA_HEVC_YUV420&&val->inputImageFormat <= ASIC_INPUT_P010_TILE8)) /* YUV input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_YUV | (ENCH2_INPUT_SWAP_16_YUV << 1) | (ENCH2_INPUT_SWAP_32_YUV << 2) | (ENCH2_INPUT_SWAP_64_YUV << 3);
  else if (val->inputImageFormat < ASIC_INPUT_RGB888) /* 16-bit RGB input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_RGB16 | (ENCH2_INPUT_SWAP_16_RGB16 << 1) | (ENCH2_INPUT_SWAP_32_RGB16 << 2) | (ENCH2_INPUT_SWAP_64_RGB16 << 3);
  else if(val->inputImageFormat < ASIC_INPUT_I010)   /* 32-bit RGB input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_RGB32 | (ENCH2_INPUT_SWAP_16_RGB32 << 1) | (ENCH2_INPUT_SWAP_32_RGB32 << 2) | (ENCH2_INPUT_SWAP_64_RGB32 << 3);
  else if(val->inputImageFormat >= ASIC_INPUT_I010&&val->inputImageFormat <= ASIC_INPUT_PACKED_10BIT_Y0L2)
     val->asic_pic_swap = ENCH2_INPUT_SWAP_8_YUV | (ENCH2_INPUT_SWAP_16_YUV << 1) | (ENCH2_INPUT_SWAP_32_YUV << 2) | (ENCH2_INPUT_SWAP_64_YUV << 3);

  //after H2V4, prp swap changed
#if ENCH2_INPUT_SWAP_64_YUV==1
  if (HW_ID_MAJOR_NUMBER(val->asicHwId) >= 4) /* H2V4+ */
    val->asic_pic_swap = ((~val->asic_pic_swap) & 0xf);
#endif

  CheckRegisterValues(val);

  EWLmemset(val->regMirror, 0, sizeof(val->regMirror));
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WR_ID_E, 0);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_RD_ID_E, 0);

  /* clear all interrupt */
  if (HW_ID_MAJOR_NUMBER(val->asicHwId) >= 0x61 || (HW_ID_MAJOR_NUMBER(val->asicHwId) == 0x60 && HW_ID_MINOR_NUMBER(val->asicHwId) >= 1))
  {
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_SLICE_RDY_STATUS, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_IRQ_LINE_BUFFER, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_TIMEOUT, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_BUFFER_FULL, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_SW_RESET, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_BUS_ERROR_STATUS, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_FRAME_RDY_STATUS, 1);
    EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_IRQ, 1);
  }

  /* encoder interrupt */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IRQ_DIS, val->irqDisable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_READ_ID, val->asic_axi_readID);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WRITE_ID, val->asic_axi_writeID);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_E, val->asic_clock_gating_encoder);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_H265_E, val->asic_clock_gating_encoder_h265);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_H264_E, val->asic_clock_gating_encoder_h264);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_E, val->asic_clock_gating_inter);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_H265_E, val->asic_clock_gating_inter_h265);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_H264_E, val->asic_clock_gating_inter_h264);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MODE, val->codingType);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICE_SIZE, (val->sliceSize & 0x7f));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICE_SIZE_MSB, (val->sliceSize>>7)&3);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICE_SIZE_MSB2, (val->sliceSize>>9)&1);

  /* output stream buffer */
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_STRM_BASE, val->outputStrmBase[0]);
  EncAsicSetAddrRegisterValue(val->regMirror,  HWIF_ENC_AV1_PRECARRY_BUFFER_BASE, val->Av1PreoutputStrmBase[0]);
  /* stream buffer limits */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT, val->outputStrmSize[0]);
  if (val->asicCfg.streamBufferChain)
  {
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_STRM_BUF1_BASE, val->outputStrmBase[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_STRM_BUFFER1_LIMIT, val->outputStrmSize[1]);
  }

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SWAP, val->asic_stream_swap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_SWAP, val->asic_pic_swap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI_MAP_QP_DELTA_MAP_SWAP, val->asic_roi_map_qp_delta_swap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_MEM_OUT_SWAP, val->asic_ctb_rc_mem_out_swap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUT_SWAP, val->scaledOutSwap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CU_INFO_MEM_OUT_SWAP, val->asic_cu_info_mem_out_swap);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NALUNITSIZE_SWAP, val->nalUnitSizeSwap);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_FRAME_CODING_TYPE, val->frameCodingType);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRONG_INTRA_SMOOTHING_ENABLED_FLAG, val->strong_intra_smoothing_enabled_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_POC, val->poc);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_FRAMENUM, val->frameNum);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IDR_PIC_ID, val->idrPicId);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NAL_REF_IDC, val->nalRefIdc);
  //Support only layerInRefIdc=1
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NAL_REF_IDC_2BIT, val->nalRefIdc_2bit);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_TRANSFORM8X8_ENABLE, val->transform8x8Enable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ENTROPY_CODING_MODE, val->entropy_coding_mode_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LOG2_MAX_PIC_ORDER_CNT_LSB, val->log2_max_pic_order_cnt_lsb);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LOG2_MAX_FRAME_NUM, val->log2_max_frame_num);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_ORDER_CNT_TYPE, val->pic_order_cnt_type);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_STRM_MODE, val->streamMode);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_WRITE_REC_TO_DDR, val->writeReconToDDR);

  /* Input picture buffers */
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_INPUT_Y_BASE, val->inputLumBase);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_INPUT_CB_BASE, val->inputCbBase);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_INPUT_CR_BASE, val->inputCrBase);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_RECON_Y_BASE, val->reconLumBase);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_RECON_CHROMA_BASE, val->reconCbBase);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RECON_CR_BASE, val->reconCrBase);

  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MIN_CB_SIZE, val->minCbSize - 3);


    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_CB_SIZE, val->maxCbSize - 3);


    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MIN_TRB_SIZE, val->minTrbSize - 2);

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_TRB_SIZE, val->maxTrbSize - 2);
  }


  if (val->codingType != ASIC_JPEG) {
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_WIDTH, (val->picWidth / 8) & 0x3ff);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_WIDTH_MSB, ((val->picWidth / 8) >> 10) & 3);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_WIDTH_MSB2, (val->picWidth / 8) >> 12);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_HEIGHT, val->picHeight / 8);
  }

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PPS_DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG, val->pps_deblocking_filter_override_enabled_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICE_DEBLOCKING_FILTER_OVERRIDE_FLAG, val->slice_deblocking_filter_override_flag);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_INIT_QP, val->picInitQp);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_QP, val->qp);

  //   EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_QP_MAX, val->qpMax);

  //  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PIC_QP_MIN, val->qpMin);


  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DIFF_CU_QP_DELTA_DEPTH, val->diffCuQpDeltaDepth);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CHROMA_QP_OFFSET, val->cbQpOffset & mask_5b);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SAO_ENABLE, val->saoEnable);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_TRANS_HIERARCHY_DEPTH_INTER, val->maxTransHierarchyDepthInter);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_TRANS_HIERARCHY_DEPTH_INTRA, val->maxTransHierarchyDepthIntra);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CU_QP_DELTA_ENABLED, val->cuQpDeltaEnabled);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LOG2_PARALLEL_MERGE_LEVEL, val->log2ParellelMergeLevel);
  switch(HW_ID_MAJOR_NUMBER(val->asicHwId))
  {
  case 1: /* H2V1 rev01 */
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_SHORT_TERM_REF_PIC_SETS, val->numShortTermRefPicSets);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_ID, val->rpsId);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_PIC4X4, val->intraPenaltyPic4x4);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_PIC1, val->intraMPMPenaltyPic1);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_PIC2, val->intraMPMPenaltyPic2);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_PIC8X8, val->intraPenaltyPic8x8);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_PIC16X16, val->intraPenaltyPic16x16);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_PIC32X32, val->intraPenaltyPic32x32);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_PIC3, val->intraMPMPenaltyPic3);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI14X4, val->intraPenaltyRoi14x4);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI11, val->intraMPMPenaltyRoi11);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI12, val->intraMPMPenaltyRoi12);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI18X8, val->intraPenaltyRoi18x8);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI116X16, val->intraPenaltyRoi116x16);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI132X32, val->intraPenaltyRoi132x32);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI13, val->intraMPMPenaltyRoi13);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI24X4, val->intraPenaltyRoi24x4);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI21, val->intraMPMPenaltyRoi21);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI22, val->intraMPMPenaltyRoi22);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI28X8, val->intraPenaltyRoi28x8);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI216X16, val->intraPenaltyRoi216x16);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_PENALTY_ROI232X32, val->intraPenaltyRoi232x32);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MPM_PENALTY_ROI23, val->intraMPMPenaltyRoi23);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMBDA_MOTIONSAD, val->lambda_motionSAD);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_MOTION_SSE_ROI1, val->lamda_motion_sse_roi1);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_MOTION_SSE_ROI2, val->lamda_motion_sse_roi2);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SKIP_CHROMA_DC_THREADHOLD, val->skip_chroma_dc_threadhold);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMBDA_MOTIONSAD_ROI1, val->lambda_motionSAD_ROI1);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMBDA_MOTIONSAD_ROI2, val->lambda_motionSAD_ROI2);

    break;
  case 2: /* H2V2 rev01 */
  case 3: /* H2V3 rev01 */
  case 4: /* H2V4 rev01 */
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_SHORT_TERM_REF_PIC_SETS_V2, val->numShortTermRefPicSets);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_ID_V2, val->rpsId);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_0, val->intra_size_factor[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_1, val->intra_size_factor[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_2, val->intra_size_factor[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_3, val->intra_size_factor[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_0, val->intra_mode_factor[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_1, val->intra_mode_factor[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_2, val->intra_mode_factor[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_0, val->lambda_satd_me[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_1, val->lambda_satd_me[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_2, val->lambda_satd_me[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_3, val->lambda_satd_me[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_4, val->lambda_satd_me[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_5, val->lambda_satd_me[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_6, val->lambda_satd_me[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_7, val->lambda_satd_me[7]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_8, val->lambda_satd_me[8]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_9, val->lambda_satd_me[9]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_10, val->lambda_satd_me[10]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_11, val->lambda_satd_me[11]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_12, val->lambda_satd_me[12]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_13, val->lambda_satd_me[13]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_14, val->lambda_satd_me[14]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_15, val->lambda_satd_me[15]);

    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_0, val->lambda_sse_me[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_1, val->lambda_sse_me[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_2, val->lambda_sse_me[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_3, val->lambda_sse_me[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_4, val->lambda_sse_me[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_5, val->lambda_sse_me[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_6, val->lambda_sse_me[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_7, val->lambda_sse_me[7]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_8, val->lambda_sse_me[8]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_9, val->lambda_sse_me[9]);  
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_10, val->lambda_sse_me[10]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_11, val->lambda_sse_me[11]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_12, val->lambda_sse_me[12]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_13, val->lambda_sse_me[13]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_14, val->lambda_sse_me[14]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_15, val->lambda_sse_me[15]);
  
    

  case 5: /* H2V5 rev01 */
  case 6: /* H2V6 rev01 */
  case 7: /* H2V7 rev01 */
  case 0x60: /* VC8000E 6.0 */
  case 0x61: /* VC8000E 6.1 */
  case 0x62:
  case 0x81:
  case 0x82:    
  default:    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_SHORT_TERM_REF_PIC_SETS_V2, val->numShortTermRefPicSets);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_ID_V2, val->rpsId);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_0, val->intra_size_factor[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_1, val->intra_size_factor[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_2, val->intra_size_factor[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SIZE_FACTOR_3, val->intra_size_factor[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_0, val->intra_mode_factor[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_1, val->intra_mode_factor[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_MODE_FACTOR_2, val->intra_mode_factor[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_0_EXPAND5BIT, val->lambda_satd_me[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_1_EXPAND5BIT, val->lambda_satd_me[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_2_EXPAND5BIT, val->lambda_satd_me[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_3_EXPAND5BIT, val->lambda_satd_me[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_4_EXPAND5BIT, val->lambda_satd_me[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_5_EXPAND5BIT, val->lambda_satd_me[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_6_EXPAND5BIT, val->lambda_satd_me[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_7_EXPAND5BIT, val->lambda_satd_me[7]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_8_EXPAND5BIT, val->lambda_satd_me[8]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_9_EXPAND5BIT, val->lambda_satd_me[9]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_10_EXPAND5BIT, val->lambda_satd_me[10]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_11_EXPAND5BIT, val->lambda_satd_me[11]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_12_EXPAND5BIT, val->lambda_satd_me[12]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_13_EXPAND5BIT, val->lambda_satd_me[13]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_14_EXPAND5BIT, val->lambda_satd_me[14]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_15_EXPAND5BIT, val->lambda_satd_me[15]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NAL_UNIT_TYPE, val->nal_unit_type);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUH_TEMPORAL_ID, val->nuh_temporal_id);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PREFIXNAL_SVC_EXT, val->prefixnal_svc_ext);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PPS_ID, val->pps_id);
   
#ifdef CTBRC_STRENGTH
    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_QP_FRACTIONAL, val->qpfrac);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_QP_DELTA_GAIN, val->qpDeltaMBGain);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_COMPLEXITY_OFFSET, val->rcBaseMBComplexity);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RC_BLOCK_SIZE, val->rcBlockSize);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RC_CTBRC_SLICEQPOFFSET, val->offsetSliceQp & mask_6b);

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_16, val->lambda_satd_me[16]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_17, val->lambda_satd_me[17]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_18, val->lambda_satd_me[18]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_19, val->lambda_satd_me[19]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_20, val->lambda_satd_me[20]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_21, val->lambda_satd_me[21]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_22, val->lambda_satd_me[22]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_23, val->lambda_satd_me[23]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_24, val->lambda_satd_me[24]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_25, val->lambda_satd_me[25]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_26, val->lambda_satd_me[26]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_27, val->lambda_satd_me[27]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_28, val->lambda_satd_me[28]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_29, val->lambda_satd_me[29]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_30, val->lambda_satd_me[30]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SATD_ME_31, val->lambda_satd_me[31]);
#endif
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_0_EXPAND6BIT, val->lambda_sse_me[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_1_EXPAND6BIT, val->lambda_sse_me[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_2_EXPAND6BIT, val->lambda_sse_me[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_3_EXPAND6BIT, val->lambda_sse_me[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_4_EXPAND6BIT, val->lambda_sse_me[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_5_EXPAND6BIT, val->lambda_sse_me[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_6_EXPAND6BIT, val->lambda_sse_me[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_7_EXPAND6BIT, val->lambda_sse_me[7]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_8_EXPAND6BIT, val->lambda_sse_me[8]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_9_EXPAND6BIT, val->lambda_sse_me[9]);   
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_10_EXPAND6BIT, val->lambda_sse_me[10]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_11_EXPAND6BIT, val->lambda_sse_me[11]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_12_EXPAND6BIT, val->lambda_sse_me[12]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_13_EXPAND6BIT, val->lambda_sse_me[13]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_14_EXPAND6BIT, val->lambda_sse_me[14]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_15_EXPAND6BIT, val->lambda_sse_me[15]);
#ifdef CTBRC_STRENGTH
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_16, val->lambda_sse_me[16]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_17, val->lambda_sse_me[17]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_18, val->lambda_sse_me[18]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_19, val->lambda_sse_me[19]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_20, val->lambda_sse_me[20]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_21, val->lambda_sse_me[21]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_22, val->lambda_sse_me[22]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_23, val->lambda_sse_me[23]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_24, val->lambda_sse_me[24]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_25, val->lambda_sse_me[25]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_26, val->lambda_sse_me[26]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_27, val->lambda_sse_me[27]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_28, val->lambda_sse_me[28]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_29, val->lambda_sse_me[29]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_30, val->lambda_sse_me[30]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SSE_ME_31, val->lambda_sse_me[31]);  

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_16, val->lambda_satd_ims[16]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_17, val->lambda_satd_ims[17]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_18, val->lambda_satd_ims[18]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_19, val->lambda_satd_ims[19]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_20, val->lambda_satd_ims[20]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_21, val->lambda_satd_ims[21]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_22, val->lambda_satd_ims[22]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_23, val->lambda_satd_ims[23]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_24, val->lambda_satd_ims[24]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_25, val->lambda_satd_ims[25]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_26, val->lambda_satd_ims[26]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_27, val->lambda_satd_ims[27]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_28, val->lambda_satd_ims[28]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_29, val->lambda_satd_ims[29]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_30, val->lambda_satd_ims[30]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_31, val->lambda_satd_ims[31]);
        
#endif

    break;
  }
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_NEGATIVE_PICS, val->numNegativePics);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_POSITIVE_PICS, val->numPositivePics);


  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_POC0, val->l0_delta_poc[0] & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_POC0_MSB, (val->l0_delta_poc[0] >> 10) & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_LONG_TERM_FLAG0, val->l0_long_term_flag[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_USED_BY_CURR_PIC0, val->l0_used_by_curr_pic[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_FRAMENUM0, val->l0_delta_framenum[0] & mask_11b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_FRAMENUM0_MSB, (val->l0_delta_framenum[0] >> 11) & mask_9b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_USED_BY_NEXT_PIC0, val->l0_used_by_next_pic[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_POC1, val->l0_delta_poc[1] & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_POC1_MSB, (val->l0_delta_poc[1] >> 10) & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_LONG_TERM_FLAG1, val->l0_long_term_flag[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_USED_BY_CURR_PIC1, val->l0_used_by_curr_pic[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_FRAMENUM1, val->l0_delta_framenum[1] & mask_11b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_DELTA_FRAMENUM1_MSB, (val->l0_delta_framenum[1] >> 11) & mask_9b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_USED_BY_NEXT_PIC1, val->l0_used_by_next_pic[1]);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MARK_CURRENT_LONGTERM, val->markCurrentLongTerm);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_LONGTERMIDX_PLUS1, val->max_long_term_frame_idx_plus1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CUR_LONGTERMIDX, val->currentLongTermIdx & mask_3b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_LONGTERMIDX0, val->l0_referenceLongTermIdx[0] & mask_3b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_LONGTERMIDX1, val->l0_referenceLongTermIdx[1] & mask_3b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_LONGTERMIDX0, val->l1_referenceLongTermIdx[0] & mask_3b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_LONGTERMIDX1, val->l1_referenceLongTermIdx[1] & mask_3b);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_POC0, val->l1_delta_poc[0] & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_POC0_MSB, (val->l1_delta_poc[0] >> 10) & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_LONG_TERM_FLAG0, val->l1_long_term_flag[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_USED_BY_CURR_PIC0, val->l1_used_by_curr_pic[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_FRAMENUM0, val->l1_delta_framenum[0] & mask_11b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_FRAMENUM0_MSB, (val->l1_delta_framenum[0] >> 11) & mask_9b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_USED_BY_NEXT_PIC0, val->l1_used_by_next_pic[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_POC1, val->l1_delta_poc[1] & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_POC1_MSB, (val->l1_delta_poc[1] >> 10) & mask_10b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_LONG_TERM_FLAG1, val->l1_long_term_flag[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_USED_BY_CURR_PIC1, val->l1_used_by_curr_pic[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_FRAMENUM1, val->l1_delta_framenum[1] & mask_11b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_DELTA_FRAMENUM1_MSB, (val->l1_delta_framenum[1] >> 11) & mask_9b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_USED_BY_NEXT_PIC1, val->l1_used_by_next_pic[1]);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LONG_TERM_REF_PICS_PRESENT_FLAG, val->long_term_ref_pics_present_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_LONG_TERM_PICS, val->num_long_term_pics);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_Y0, val->pRefPic_recon_l0[0][0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_CHROMA0, val->pRefPic_recon_l0[1][0]);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_CR0, val->pRefPic_recon_l0[2][0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALING_LIST_ENABLED_FLAG, val->scaling_list_enabled_flag);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_DELTA_POC_0, val->rps_delta_poc[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_DELTA_POC_1, val->rps_delta_poc[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_DELTA_POC_2, val->rps_delta_poc[2]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_DELTA_POC_3, val->rps_delta_poc[3]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_USED_BY_CUR_0, val->rps_used_by_cur[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_USED_BY_CUR_1, val->rps_used_by_cur[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_USED_BY_CUR_2, val->rps_used_by_cur[2]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_USED_BY_CUR_3, val->rps_used_by_cur[3]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_NEG_PIC_NUM, val->rps_neg_pic_num);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RPS_POS_PIC_NUM, val->rps_pos_pic_num);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SHORT_TERM_REF_PIC_SET_SPS_FLAG, val->short_term_ref_pic_set_sps_flag);



  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_Y1, val->pRefPic_recon_l0[0][1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_CHROMA1, val->pRefPic_recon_l0[1][1]);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_CR1, val->pRefPic_recon_l0[2][1]);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_Y0, val->pRefPic_recon_l1[0][0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_CHROMA0, val->pRefPic_recon_l1[1][0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_Y1, val->pRefPic_recon_l1[0][1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_CHROMA1, val->pRefPic_recon_l1[1][1]);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ACTIVE_L0_CNT, val->active_l0_cnt);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ACTIVE_L1_CNT, val->active_l1_cnt);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ACTIVE_OVERRIDE_FLAG, val->active_override_flag);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_START, (val->cirStart & 0x3fff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_START_MSB, (val->cirStart >> 14)&0xf);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_START_MSB2, (val->cirStart >> 18));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_INTERVAL, (val->cirInterval & 0x3fff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_INTERVAL_MSB, (val->cirInterval >> 14)&0xf);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CIR_INTERVAL_MSB2, (val->cirInterval >> 18));
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RCROI_ENABLE, val->rcRoiEnable);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_ROIMAPDELTAQPADDR, val->roiMapDeltaQpAddr);
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_CUCTRL_INDEX_ENABLE, val->RoimapCuCtrl_index_enable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_CUCTRL_ENABLE, val->RoimapCuCtrl_enable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_CUCTRL_VER, val->RoimapCuCtrl_ver);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_QPDELTA_VER, val->RoiQpDelta_ver);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_CUCTRL_INDEX_ADDR, val->RoimapCuCtrlIndexAddr);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_ROIMAP_CUCTRL_ADDR, val->RoimapCuCtrlAddr);
  if (val->asicCfg.roiMapVersion == 3)
  {
      if(val->RoimapCuCtrl_ver > 3 )
      {
          EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM_MAP_ENABLE, 1);
          EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SKIP_MAP_ENABLE, 1);
      }
      else if (val->RoiQpDelta_ver == 1)
      {
          EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM_MAP_ENABLE, (val->ipcmMapEnable & 0x1));
      }
      else if (val->RoiQpDelta_ver == 2)
      {
          EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SKIP_MAP_ENABLE, 1);
      }
  }

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_LEFT, (val->intraAreaLeft & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_LEFT_MSB, (val->intraAreaLeft >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_LEFT_MSB2, (val->intraAreaLeft >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_RIGHT, (val->intraAreaRight & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_RIGHT_MSB, (val->intraAreaRight >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_RIGHT_MSB2, (val->intraAreaRight >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_TOP, (val->intraAreaTop & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_TOP_MSB, (val->intraAreaTop >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_TOP_MSB2, (val->intraAreaTop >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_BOTTOM, (val->intraAreaBottom & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_BOTTOM_MSB, (val->intraAreaBottom >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_AREA_BOTTOM_MSB2, (val->intraAreaBottom >> 9)&1);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_LEFT, (val->roi1Left & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_LEFT_MSB, (val->roi1Left >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_LEFT_MSB2, (val->roi1Left >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_RIGHT, (val->roi1Right & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_RIGHT_MSB, (val->roi1Right >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_RIGHT_MSB2, (val->roi1Right >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_TOP, (val->roi1Top & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_TOP_MSB, (val->roi1Top >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_TOP_MSB2, (val->roi1Top >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_BOTTOM, (val->roi1Bottom & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_BOTTOM_MSB, (val->roi1Bottom >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_BOTTOM_MSB2, (val->roi1Bottom >> 9)&1);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_LEFT, (val->roi2Left & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_LEFT_MSB, (val->roi2Left >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_LEFT_MSB2, (val->roi2Left >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_RIGHT, (val->roi2Right & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_RIGHT_MSB, (val->roi2Right >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_RIGHT_MSB2, (val->roi2Right >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_TOP, (val->roi2Top & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_TOP_MSB, (val->roi2Top >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_TOP_MSB2, (val->roi2Top >> 9)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_BOTTOM, (val->roi2Bottom & 0xff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_BOTTOM_MSB, (val->roi2Bottom >> 8)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_BOTTOM_MSB2, (val->roi2Bottom >> 9)&1);

  if (val->asicCfg.ROI8Support)
  {
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_LEFT, (val->roi3Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_RIGHT, (val->roi3Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_TOP, (val->roi3Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_BOTTOM, (val->roi3Bottom & 0x3ff));

      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_LEFT, (val->roi4Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_RIGHT, (val->roi4Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_TOP, (val->roi4Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_BOTTOM, (val->roi4Bottom & 0x3ff));

      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_LEFT, (val->roi5Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_RIGHT, (val->roi5Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_TOP, (val->roi5Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_BOTTOM, (val->roi5Bottom & 0x3ff));

      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_LEFT, (val->roi6Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_RIGHT, (val->roi6Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_TOP, (val->roi6Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_BOTTOM, (val->roi6Bottom & 0x3ff));

      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_LEFT, (val->roi7Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_RIGHT, (val->roi7Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_TOP, (val->roi7Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_BOTTOM, (val->roi7Bottom & 0x3ff));

      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_LEFT, (val->roi8Left & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_RIGHT, (val->roi8Right & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_TOP, (val->roi8Top & 0x3ff));
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_BOTTOM, (val->roi8Bottom & 0x3ff));
  }

#ifndef CTBRC_STRENGTH
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_DELTA_QP, MIN((i32)val->qp, val->roi1DeltaQp));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_DELTA_QP, MIN((i32)val->qp, val->roi2DeltaQp));
#else
  if (val->asicCfg.roiAbsQpSupport)
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_QP_VALUE, val->roi1Qp >= 0 ? val->roi1Qp : (val->roi1DeltaQp&0x7f));
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_QP_VALUE, val->roi2Qp >= 0 ? val->roi2Qp : (val->roi2DeltaQp&0x7f));
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_QP_TYPE, val->roi1Qp >= 0 ? 1 : 0);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_QP_TYPE, val->roi2Qp >= 0 ? 1 : 0);

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SSE_QP_FACTOR, val->qpFactorSse);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SAD_QP_FACTOR, val->qpFactorSad);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMBDA_DEPTH, val->lambdaDepth ? 1 : 0);

    if (val->asicCfg.ROI8Support)
    {
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_QP_VALUE, val->roi3Qp >= 0 ? val->roi3Qp : (val->roi3DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI3_QP_TYPE, val->roi3Qp >= 0 ? 1 : 0);

        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_QP_VALUE, val->roi4Qp >= 0 ? val->roi4Qp : (val->roi4DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI4_QP_TYPE, val->roi4Qp >= 0 ? 1 : 0);

        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_QP_VALUE, val->roi5Qp >= 0 ? val->roi5Qp : (val->roi5DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI5_QP_TYPE, val->roi5Qp >= 0 ? 1 : 0);

        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_QP_VALUE, val->roi6Qp >= 0 ? val->roi6Qp : (val->roi6DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI6_QP_TYPE, val->roi6Qp >= 0 ? 1 : 0);

        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_QP_VALUE, val->roi7Qp >= 0 ? val->roi7Qp : (val->roi7DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI7_QP_TYPE, val->roi7Qp >= 0 ? 1 : 0);

        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_QP_VALUE, val->roi8Qp >= 0 ? val->roi8Qp : (val->roi8DeltaQp & 0x7f));
        EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI8_QP_TYPE, val->roi8Qp >= 0 ? 1 : 0);
    }
  }
  else
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI1_DELTA_QP_RC, val->roi1DeltaQp);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI2_DELTA_QP_RC, val->roi2DeltaQp);
  }
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_QP_MIN, val->qpMin);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_QP_MAX, val->qpMax);
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTBRC_QPDELTA_FLAG_REVERSE, val->ctbRcQpDeltaReverse);

  if (val->asicCfg.ctbRcVersion > 1)
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RC_QPDELTA_RANGE, val->rcQpDeltaRange & 0xf);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RC_QPDELTA_RANGE_MSB, (val->rcQpDeltaRange >> 4) & 0x3);
  }
  else
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RC_QPDELTA_RANGE, MIN(val->rcQpDeltaRange, 0xf));

  if (val->asicCfg.ctbRcVersion && (val->rcRoiEnable&0x08))
  {  
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM0, val->ctbRcModelParam0);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM1, val->ctbRcModelParam1);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM_MIN, val->ctbRcModelParamMin);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_ROW_FACTOR, val->ctbRcRowFactor);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_QP_STEP, val->ctbRcQpStep);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PREV_PIC_LUM_MAD, val->prevPicLumMad);    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_PREV_MAD_VALID, val->ctbRcPrevMadValid);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_CURRENT_CTB_MAD_BASE,  val->ctbRcMemAddrCur);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_PREVIOUS_CTB_MAD_BASE, val->ctbRcMemAddrPre);    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTB_RC_DELAY, val->ctbRcDelay);
  }
#endif

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DEBLOCKING_FILTER_CTRL, val->filterDisable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DEBLOCKING_TC_OFFSET, (val->tc_Offset / 2)&mask_4b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DEBLOCKING_BETA_OFFSET, (val->beta_Offset / 2)&mask_4b);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_SIZE_TBL_BASE, val->sizeTblBase);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NAL_SIZE_WRITE, val->sizeTblBase != 0);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SAO_LUMA, val->lamda_SAO_luma);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_SAO_CHROMA, val->lamda_SAO_chroma);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LAMDA_MOTION_SSE, val->lamda_motion_sse);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_TU_SPLIT_PENALTY, val->bits_est_tu_split_penalty);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_BIAS_INTRA_CU_8, val->bits_est_bias_intra_cu_8);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_BIAS_INTRA_CU_16, val->bits_est_bias_intra_cu_16);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_BIAS_INTRA_CU_32, val->bits_est_bias_intra_cu_32);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_BIAS_INTRA_CU_64, val->bits_est_bias_intra_cu_64);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTER_SKIP_BIAS, val->inter_skip_bias);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITS_EST_1N_CU_PENALTY, val->bits_est_1n_cu_penalty);


  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_FORMAT, val->inputImageFormat & 0xf);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_FORMAT_MSB, val->inputImageFormat >> 4);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_BITWIDTH_CHROMA, val->outputBitWidthChroma);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_BITWIDTH_LUM, val->outputBitWidthLuma);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_ROTATION, val->inputImageRotation);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MIRROR, val->inputImageMirror);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFA, val->colorConversionCoeffA);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFB, val->colorConversionCoeffB);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFC, val->colorConversionCoeffC);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFE, val->colorConversionCoeffE);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFF, val->colorConversionCoeffF);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFG, val->colorConversionCoeffG);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBCOEFFH, val->colorConversionCoeffH);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RMASKMSB, val->rMaskMsb);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_GMASKMSB, val->gMaskMsb);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BMASKMSB, val->bMaskMsb);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RGBLUMAOFFSET, val->colorConversionLumaOffset);
  /*stride*/
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_LU_STRIDE, val->input_luma_stride);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_CH_STRIDE, val->input_chroma_stride);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_LU_STRIDE, val->ref_frame_stride);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_CH_STRIDE, val->ref_frame_stride_ch);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_DS_LU_STRIDE, val->ref_ds_luma_stride);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CHROFFSET, val->inputChromaBaseOffset);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LUMOFFSET, val->inputLumaBaseOffset);
  if (val->codingType != ASIC_JPEG)
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROWLENGTH, val->pixelsOnRow);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_XFILL, (val->xFill&3));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_YFILL, (val->yFill&7));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_XFILL_MSB, (val->xFill>>2));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_YFILL_MSB, (val->yFill>>3));

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DUMMYREADEN, val->dummyReadEnable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DUMMYREADADDR, val->dummyReadAddr&0xffffffff);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALE_MODE, val->scaledWidth > 0 ? 2 : 0);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_BASESCALEDOUTLUM, val->scaledLumBase);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUTWIDTH, (val->scaledWidth & 0x1fff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUTWIDTHMSB, (val->scaledWidth >> 13)&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUTWIDTHRATIO, val->scaledWidthRatio);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUTHEIGHT, val->scaledHeight);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUTHEIGHTRATIO, val->scaledHeightRatio);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDSKIPLEFTPIXELCOLUMN, val->scaledSkipLeftPixelColumn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDSKIPTOPPIXELROW, val->scaledSkipTopPixelRow);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_VSCALE_WEIGHT_EN, val->scaledVertivalWeightEn);
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDHORIZONTALCOPY, val->scaledHorizontalCopy);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDVERTICALCOPY, val->scaledVerticalCopy);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SCALEDOUT_FORMAT, val->scaledOutputFormat);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CHROMA_SWAP, val->chromaSwap);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_COMPRESSEDCOEFF_BASE, val->compress_coeff_scan_base);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CABAC_INIT_FLAG, val->cabac_init_flag);

  //compress
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RECON_LUMA_COMPRESSOR_ENABLE, val->recon_luma_compress);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RECON_CHROMA_COMPRESSOR_ENABLE, val->recon_chroma_compress);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_REF0_LUMA_COMPRESSOR_ENABLE, val->ref_l0_luma_compressed[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_REF0_CHROMA_COMPRESSOR_ENABLE, val->ref_l0_chroma_compressed[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_REF1_LUMA_COMPRESSOR_ENABLE, val->ref_l0_luma_compressed[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L0_REF1_CHROMA_COMPRESSOR_ENABLE, val->ref_l0_chroma_compressed[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_REF0_LUMA_COMPRESSOR_ENABLE, val->ref_l1_luma_compressed[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_REF0_CHROMA_COMPRESSOR_ENABLE, val->ref_l1_chroma_compressed[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_REF1_LUMA_COMPRESSOR_ENABLE, val->ref_l1_luma_compressed[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_L1_REF1_CHROMA_COMPRESSOR_ENABLE, val->ref_l1_chroma_compressed[1]);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_RECON_LUMA_COMPRESS_TABLE_BASE, val->recon_luma_compress_tbl_base);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_RECON_CHROMA_COMPRESS_TABLE_BASE, val->recon_chroma_compress_tbl_base);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L0_REF0_LUMA_COMPRESS_TABLE_BASE, val->ref_l0_luma_compress_tbl_base[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L0_REF0_CHROMA_COMPRESS_TABLE_BASE, val->ref_l0_chroma_compress_tbl_base[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L0_REF1_LUMA_COMPRESS_TABLE_BASE, val->ref_l0_luma_compress_tbl_base[1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L0_REF1_CHROMA_COMPRESS_TABLE_BASE,  val->ref_l0_chroma_compress_tbl_base[1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L1_REF0_LUMA_COMPRESS_TABLE_BASE, val->ref_l1_luma_compress_tbl_base[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L1_REF0_CHROMA_COMPRESS_TABLE_BASE, val->ref_l1_chroma_compress_tbl_base[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L1_REF1_LUMA_COMPRESS_TABLE_BASE, val->ref_l1_luma_compress_tbl_base[1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_L1_REF1_CHROMA_COMPRESS_TABLE_BASE,  val->ref_l1_chroma_compress_tbl_base[1]);

  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_RECON_LUMA_4N_BASE,  val->reconL4nBase);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_4N0_BASE,  val->pRefPic_recon_l0_4n[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L0_4N1_BASE,  val->pRefPic_recon_l0_4n[1]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_4N0_BASE,  val->pRefPic_recon_l1_4n[0]);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_REFPIC_RECON_L1_4N1_BASE,  val->pRefPic_recon_l1_4n[1]);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_TIMEOUT_INT, ENCH2_TIMEOUT_INTERRUPT&1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICE_INT, val->sliceReadyInterrupt);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_TARGETPICSIZE, val->targetPicSize);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MINPICSIZE, val->minPicSize);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAXPICSIZE, val->maxPicSize);

  /* JPEG specific control */
  if (val->codingType == ASIC_JPEG)
  {
    if (val->inputImageRotation && val->inputImageRotation != 3) {
      EncAsicSetRegisterValue(val->regMirror,  HWIF_ENC_JPEG_PIC_WIDTH,  (val->mbsInCol*2)&0xfff);
      EncAsicSetRegisterValue(val->regMirror,  HWIF_ENC_JPEG_PIC_WIDTH_MSB,  (val->mbsInCol*2)>>12);
      EncAsicSetRegisterValue(val->regMirror,  HWIF_ENC_JPEG_PIC_HEIGHT,  (val->mbsInRow*2)&0xfff);
      EncAsicSetRegisterValue(val->regMirror,  HWIF_ENC_JPEG_PIC_HEIGHT_MSB,  (val->mbsInRow*2)>>12);
    } else {
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_PIC_WIDTH, (val->mbsInRow*2)&0xfff);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_PIC_WIDTH_MSB, (val->mbsInRow*2)>>12);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_PIC_HEIGHT, (val->mbsInCol*(val->jpegMode == 1 && val->ljpegFmt == 1 ? 1 : 2))&0xfff);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_PIC_HEIGHT_MSB, (val->mbsInCol*(val->jpegMode == 1 && val->ljpegFmt == 1 ? 1 : 2))>>12);
    }
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_ROWLENGTH, val->pixelsOnRow&0x7fff);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_ROWLENGTH_MSB, val->pixelsOnRow>>15);
    /*stride*/
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_LU_STRIDE, val->input_luma_stride);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INPUT_CH_STRIDE, val->input_chroma_stride);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_LU_STRIDE, val->ref_frame_stride);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_DS_LU_STRIDE, val->ref_ds_luma_stride);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_CH_STRIDE, val->ref_frame_stride_ch);
    

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_MODE, val->jpegMode);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_SLICE, val->jpegSliceEnable);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_RSTINT, val->jpegRestartInterval);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_JPEG_RST, val->jpegRestartMarker);

    /* lossless */
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LJPEG_EN, val->ljpegEn);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LJPEG_FORMAT, val->ljpegFmt);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LJPEG_PSV, val->ljpegPsv);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LJPEG_PT, val->ljpegPt);

    /* Stream start offset */
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_STARTOFFSET, val->firstFreeBit);

    /* stream buffer limits */
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_HDRREM1, val->strmStartMSB);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_HDRREM2, val->strmStartLSB);
  }

  if(val->codingType == ASIC_H264) {
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_COLCTBS_STORE_BASE, val->colctbs_store_base);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_COLCTBS_LOAD_BASE, val->colctbs_load_base);
  }
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTBRCTHRDMIN, val->ctbRcThrdMin);
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTBRCTHRDMAX, val->ctbRcThrdMax);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTBBITSMIN, val->ctbBitsMin);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CTBBITSMAX, val->ctbBitsMax);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_TOTALLCUBITS, val->totalLcuBits);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_BITSRATIO, val->bitsRatio);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_BURST, ENCH2_AXI40_BURST_LENGTH);
  EncAsicSetRegisterValue(val->regMirror, HWIF_TIMEOUT_OVERRIDE_E, ENCH2_ASIC_TIMEOUT_OVERRIDE_ENABLE);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_READ_OUTSTANDING_NUM, ENCH2_ASIC_AXI_READ_OUTSTANDING_NUM);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WRITE_OUTSTANDING_NUM, ENCH2_ASIC_AXI_WRITE_OUTSTANDING_NUM);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RD_URGENT_ENABLE_THRESHOLD, ENCH2_ASIC_AXI_RD_URGENT_ENABLE_THRESHOLD);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RD_URGENT_DISABLE_THRESHOLD, ENCH2_ASIC_AXI_RD_URGENT_DISABLE_THRESHOLD);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_WR_URGENT_ENABLE_THRESHOLD, ENCH2_ASIC_AXI_WR_URGENT_ENABLE_THRESHOLD);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_WR_URGENT_DISABLE_THRESHOLD, ENCH2_ASIC_AXI_WR_URGENT_DISABLE_THRESHOLD);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_EXTLINEBUFFER_LINECNT_LUM_FWD, val->picWidth<256?0:val->sram_linecnt_lum_fwd);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_EXTLINEBUFFER_LINECNT_LUM_BWD, val->picWidth<256?0:val->sram_linecnt_lum_bwd);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_EXTLINEBUFFER_LINECNT_CHR_FWD, val->picWidth<256?0:val->sram_linecnt_chr_fwd);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_EXTLINEBUFFER_LINECNT_CHR_BWD, val->picWidth<256?0:val->sram_linecnt_chr_bwd);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_EXT_SRAM_LUM_FWD_BASE, val->sram_base_lum_fwd);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_EXT_SRAM_LUM_BWD_BASE, val->sram_base_lum_bwd);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_EXT_SRAM_CHR_FWD_BASE, val->sram_base_chr_fwd);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_EXT_SRAM_CHR_BWD_BASE, val->sram_base_chr_bwd);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_WR_COMMON, val->AXI_burst_align_wr_common);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_WR_STREAM, val->AXI_burst_align_wr_stream);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_WR_CHROMA_REF, val->AXI_burst_align_wr_chroma_ref);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_WR_LUMA_REF, val->AXI_burst_align_wr_luma_ref);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_RD_COMMON, val->AXI_burst_align_rd_common);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_RD_PRP, val->AXI_burst_align_rd_prp);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_RD_CH_REF_PREFETCH, val->AXI_burst_align_rd_ch_ref_prefetch);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_BURST_ALIGN_RD_LU_REF_PREFETCH, val->AXI_burst_align_rd_lu_ref_prefetch);

  timeOutCycles = ENCH2_ASIC_TIMEOUT_CYCLES;
  if (val->lineBufferEn)
  {
    /* low latency */
    EncAsicSetRegisterValue(val->regMirror, HWIF_TIMEOUT_OVERRIDE_E, 1);
    timeOutCycles = 0xFFFFFFFF;
  }
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_TIMEOUT_CYCLES, timeOutCycles & 0x7fffff);
  EncAsicSetRegisterValue(val->regMirror, HWIF_TIMEOUT_CYCLES_MSB, timeOutCycles >> 23);

  //reference pic lists modification
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LISTS_MODI_PRESENT_FLAG, val->lists_modification_present_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_PIC_LIST_MODI_FLAG_L0, val->ref_pic_list_modification_flag_l0);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LIST_ENTRY_L0_PIC0, val->list_entry_l0[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LIST_ENTRY_L0_PIC1, val->list_entry_l0[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_PIC_LIST_MODI_FLAG_L1, val->ref_pic_list_modification_flag_l1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LIST_ENTRY_L1_PIC0, val->list_entry_l1[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LIST_ENTRY_L1_PIC1, val->list_entry_l1[1]);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_0, val->lambda_satd_ims[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_1, val->lambda_satd_ims[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_2, val->lambda_satd_ims[2]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_3, val->lambda_satd_ims[3]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_4, val->lambda_satd_ims[4]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_5, val->lambda_satd_ims[5]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_6, val->lambda_satd_ims[6]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_7, val->lambda_satd_ims[7]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_8, val->lambda_satd_ims[8]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_9, val->lambda_satd_ims[9]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_10, val->lambda_satd_ims[10]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_11, val->lambda_satd_ims[11]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_12, val->lambda_satd_ims[12]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_13, val->lambda_satd_ims[13]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_14, val->lambda_satd_ims[14]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_INTRA_SATD_LAMDA_15, val->lambda_satd_ims[15]);

  /* low latency */
  EncAsicSetRegisterValue(val->regMirror, HWIF_CTB_ROW_WR_PTR, val->mbWrPtr&0x3ff);
  EncAsicSetRegisterValue(val->regMirror, HWIF_CTB_ROW_RD_PTR, val->mbRdPtr&0x3ff);
  if (val->enc_mode==ASIC_JPEG)
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_CTB_ROW_WR_PTR_JPEG_MSB, val->mbWrPtr>>10);
    EncAsicSetRegisterValue(val->regMirror, HWIF_CTB_ROW_RD_PTR_JPEG_MSB, val->mbRdPtr>>10);
  }
  EncAsicSetRegisterValue(val->regMirror, HWIF_NUM_CTB_ROWS_PER_SYNC, (val->lineBufferDepth & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_NUM_CTB_ROWS_PER_SYNC_MSB, ((val->lineBufferDepth>>9) & 0x3f));
  EncAsicSetRegisterValue(val->regMirror, HWIF_LOW_LATENCY_EN, val->lineBufferEn);    
  EncAsicSetRegisterValue(val->regMirror, HWIF_LOW_LATENCY_HW_SYNC_EN, val->lineBufferHwHandShake);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LINE_BUFFER_INT, val->lineBufferInterruptEn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_INPUT_BUF_LOOPBACK_EN, val->lineBufferLoopBackEn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SYN_AMOUNT_PER_LOOPBACK, val->amountPerLoopBack);

  /* for noise reduction */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NOISE_REDUCTION_ENABLE, val->noiseReductionEnable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NOISE_LOW, val->noiseLow);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_FIRST_FRAME_SIGMA, val->firstFrameSigma);
  #ifdef ME_MEMSHARE_CHANGE
  val->nrMbNumInvert = ((1<<MB_NUM_BIT_WIDTH)/(((((val->picWidth + 31) >> 5) << 5) * (((val->picHeight + 31) >> 5) << 5))/(16*16))) & 0xffff;
  #else  
  val->nrMbNumInvert = ((1<<MB_NUM_BIT_WIDTH)/(((((val->picWidth + 63) >> 6) << 6) * (((val->picHeight + 63) >> 6) << 6))/(16*16))) & 0xffff;
  #endif
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NR_MBNUM_INVERT_REG, val->nrMbNumInvert);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SLICEQP_PREV, val->nrSliceQPPrev& mask_6b);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THRESH_SIGMA_CUR, val->nrThreshSigmaCur& mask_21b);
  
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SIGMA_CUR, val->nrSigmaCur& mask_16b);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_FRAME_SIGMA_CALCED, val->nrThreshSigmaCalced);
  //EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THRESH_SIGMA_CALCED, val->nrFrameSigmaCalced);

  //cu information output
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OUTPUT_CU_INFO_ENABLED, val->enableOutputCuInfo);
  if (val->enableOutputCuInfo)
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CUINFOVERSION, val->cuInfoVersion);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_CU_INFORMATION_TABLE_BASE, val->cuInfoTableBase);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_CU_INFORMATION_BASE, val->cuInfoDataBase);
  }
  //hash
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_HASH_TYPE, val->hashtype);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_HASH_OFFSET, val->hashoffset);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_HASH_VAL, val->hashval);
  //smart
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ENABLE_SMART, val->smartModeEnable);
  if(val->codingType == ASIC_H264) {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LUM_DC_SUM_THR, val->smartH264LumDcTh);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CB_DC_SUM_THR, val->smartH264CbDcTh);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CR_DC_SUM_THR, val->smartH264CrDcTh);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SMART_QP, val->smartH264Qp);
  } else {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_LUM_8X8, val->smartHevcLumDcTh[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_LUM_16X16, val->smartHevcLumDcTh[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_LUM_32X32, val->smartHevcLumDcTh[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_CHROMA_8X8, val->smartHevcChrDcTh[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_CHROMA_16X16, val->smartHevcChrDcTh[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_DC_CHROMA_32X32, val->smartHevcChrDcTh[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_LUM_8X8, val->smartHevcLumAcNumTh[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_LUM_16X16, val->smartHevcLumAcNumTh[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_LUM_32X32, val->smartHevcLumAcNumTh[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_CHROMA_8X8, val->smartHevcChrAcNumTh[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_CHROMA_16X16, val->smartHevcChrAcNumTh[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_THR_AC_NUM_CHROMA_32X32, val->smartHevcChrAcNumTh[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MDQPY, val->smartHevcLumQp);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MDQPC, val->smartHevcChrQp);
  }
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MEAN_THR0, val->smartMeanTh[0]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MEAN_THR1, val->smartMeanTh[1]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MEAN_THR2, val->smartMeanTh[2]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MEAN_THR3, val->smartMeanTh[3]);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_FOREGROUND_PIXEL_THX, val->smartPixNumCntTh);

  /* Tile */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_TILES_ENABLED_FLAG, val->tiles_enabled_flag);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_TILE_COLUMNS, val->num_tile_columns);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_NUM_TILE_ROWS, val->num_tile_rows);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_LOOP_FILTER_ACROSS_TILES_ENABLED_FLAG, val->loop_filter_across_tiles_enabled_flag);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CHROMA_CONST_EN, val->constChromaEn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CB_CONST_PIXEL, val->constCb);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CR_CONST_PIXEL, val->constCr);

  //IPCM rectangle
  if(val->codingType != ASIC_JPEG) { // register reused in JPEG mode
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PCM_FILTER_DISABLE, (val->ipcmFilterDisable& 0x1));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_LEFT, (val->ipcm1AreaLeft& 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_LEFT_MSB, (val->ipcm1AreaLeft >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_RIGHT, (val->ipcm1AreaRight & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_RIGHT_MSB, (val->ipcm1AreaRight  >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_TOP, (val->ipcm1AreaTop & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_TOP_MSB, (val->ipcm1AreaTop  >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_BOTTOM, (val->ipcm1AreaBottom & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM1_BOTTOM_MSB, (val->ipcm1AreaBottom  >> 9));
  }
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_LEFT, (val->ipcm2AreaLeft& 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_LEFT_MSB, (val->ipcm2AreaLeft >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_RIGHT, (val->ipcm2AreaRight & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_RIGHT_MSB, (val->ipcm2AreaRight  >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_TOP, (val->ipcm2AreaTop & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_TOP_MSB, (val->ipcm2AreaTop  >> 9));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_BOTTOM, (val->ipcm2AreaBottom & 0x1ff));
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM2_BOTTOM_MSB, (val->ipcm2AreaBottom  >> 9));

  if(val->asicCfg.roiMapVersion == 1)
  {
    //IPCM Map
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IPCM_MAP_ENABLE, (val->ipcmMapEnable & 0x1));
  }
  else if(val->asicCfg.roiMapVersion == 2)
  {
    //SKIP Map  
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SKIP_MAP_ENABLE, (val->skipMapEnable & 0x1));
  }

  //rdoLevel
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RDO_LEVEL, val->rdoLevel);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SKIPFRAME_EN, val->skip_frame_enabled);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_P010_REF_ENABLE, val->P010RefEnable);

  /* SSIM */
  if (val->asicCfg.ssimSupport)
  {
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_SSIM_EN, val->ssim);
  }

  /* GMV */
  if (val->asicCfg.gmvSupport)
  {
    i32 i, j;
    i32 align[2] = {GMV_STEP_X, GMV_STEP_Y};
    for (j = 0; j < 2; j ++)
      for (i = 0; i < 2; i ++)
        val->gmv[j][i] = ((val->gmv[j][i]>=0) ? (val->gmv[j][i]+align[i]/2) : (val->gmv[j][i]-align[i]/2)) / align[i] * align[i];

    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_GLOBAL_HORIZONTAL_MV_L0, val->gmv[0][0] & mask_14b);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_GLOBAL_VERTICAL_MV_L0,   val->gmv[0][1] & mask_14b);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_GLOBAL_HORIZONTAL_MV_L1, val->gmv[1][0] & mask_14b);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_GLOBAL_VERTICAL_MV_L1,   val->gmv[1][1] & mask_14b);
  }
  /* multi-core sync */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MULTI_CORE_EN, val->mc_sync_enable);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_MULTICORE_SYNC_L0_ADDR, val->mc_sync_l0_addr);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_MULTICORE_SYNC_L1_ADDR, val->mc_sync_l1_addr);
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_MULTICORE_SYNC_REC_ADDR, val->mc_sync_rec_addr);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_REF_READY_THRESHOLD, val->mc_ref_ready_threshold);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DDR_POLLING_INTERVAL, val->mc_ddr_polling_interval);

  /* multi-pass */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PASS1_SKIP_CABAC, val->bSkipCabacEnable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_RDOQ_ENABLE, val->bRDOQEnable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_PRP_IN_LOOP_DS_RATIO, val->inLoopDSRatio);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MOTION_SCORE_ENABLE, val->bMotionScoreEnable);

  /*stream multi-segment*/
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SEGMENT_EN, val->streamMultiSegEn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SEGMENT_SW_SYNC_EN, val->streamMultiSegSWSyncEn);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SEGMENT_SIZE, val->streamMultiSegSize);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SEGMENT_RD_PTR, val->streamMultiSegRD);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STRM_SEGMENT_INT, val->streamMultiSegIRQEn);

  /* on-the-fly max tu size */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CURRENT_MAX_TU_SIZE_DECREASE, val->current_max_tu_size_decrease);

  /* assign ME vertical search range */
  if (val->asicCfg.meVertRangeProgramable)
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ME_ASSIGNED_VERT_SEARCH_RANGE, val->meAssignedVertRange);

  /* AV1 */
  if(val->codingType == ASIC_AV1 || val->codingType == ASIC_VP9)
  {   
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ALLOW_INTRABC, val->sw_av1_allow_intrabc);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CODED_LOSSLESS, val->sw_av1_coded_lossless);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_DELTA_Q_RES, val->sw_av1_delta_q_res);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_TX_MODE, val->sw_av1_tx_mode);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_REDUCED_TX_SET_USED, val->sw_av1_reduced_tx_set_used);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ALLOW_HIGH_PRECISION_MV, val->sw_av1_allow_high_precision_mv);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ENABLE_INTERINTRA_COMPOUND, val->sw_av1_enable_interintra_compound);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ENABLE_DUAL_FILTER, val->sw_av1_enable_dual_filter);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ENABLE_FILTER_INTRA, val->sw_av1_enable_filter_intra);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CUR_FRAME_FORCE_INTEGER_MV, val->sw_av1_cur_frame_force_integer_mv);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_SWITCHABLE_MOTION_MODE, val->sw_av1_switchable_motion_mode);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_INTERP_FILTER, val->sw_av1_interp_filter);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_SEG_ENABLE, val->sw_av1_seg_enable);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ALLOW_UPDATE_CDF, val->sw_av1_allow_update_cdf);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_SKIP_MODE_FLAG, val->sw_av1_skip_mode_flag);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_REFERENCE_MODE, val->sw_av1_reference_mode);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_LIST0_REF_FRAME, val->sw_av1_list0_ref_frame);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_LIST1_REF_FRAME, val->sw_av1_list1_ref_frame);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_DB_FILTER_LVL0, val->sw_av1_db_filter_lvl[0]);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_DB_FILTER_LVL1, val->sw_av1_db_filter_lvl[1]);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_DB_FILTER_LVL_U, val->sw_av1_db_filter_lvl_u);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_DB_FILTER_LVL_V, val->sw_av1_db_filter_lvl_v);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_SHARPNESS_LVL, val->sw_av1_sharpness_lvl);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CDEF_DAMPING, val->sw_cdef_damping);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CDEF_BITS, val->sw_cdef_bits);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CDEF_STRENGTHS, val->sw_cdef_strengths);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_CDEF_UV_STRENGTHS, val->sw_cdef_uv_strengths);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_ENABLE_ORDER_HINT, val->sw_av1_enable_order_hint);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_BTXTYPESEARCH, val->av1_bTxTypeSearch);

      EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_AV1_FRAMECTX_BASE, val->frameCtx_base);
      EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AV1_PRIMARY_REF_FRAME, val->sw_primary_ref_frame);
  }

  /* Setup overlay registers*/
  if(val->asicCfg.roiAbsQpSupport)
  {

    //OSD enable
    for(i = 0; i < MAX_OVERLAY_NUM; i++)
    {
      if(val->overlayEnable[i] != 0)
      {
        EncAsicSetRegisterValue(val->regMirror,HWIF_ENC_OSD_ALPHABLEND_ENABLE, 1);
        break;
      }
    }
    //Input buffer address
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR1, val->overlayYAddr[0]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR2, val->overlayYAddr[1]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR3, val->overlayYAddr[2]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR4, val->overlayYAddr[3]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR5, val->overlayYAddr[4]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR6, val->overlayYAddr[5]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR7, val->overlayYAddr[6]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_YADDR8, val->overlayYAddr[7]);
    
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR1, val->overlayUAddr[0]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR2, val->overlayUAddr[1]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR3, val->overlayUAddr[2]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR4, val->overlayUAddr[3]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR5, val->overlayUAddr[4]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR6, val->overlayUAddr[5]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR7, val->overlayUAddr[6]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_UADDR8, val->overlayUAddr[7]);
    
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR1, val->overlayVAddr[0]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR2, val->overlayVAddr[1]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR3, val->overlayVAddr[2]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR4, val->overlayVAddr[3]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR5, val->overlayVAddr[4]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR6, val->overlayVAddr[5]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR7, val->overlayVAddr[6]);
    EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_INPUT_VADDR8, val->overlayVAddr[7]);
    
    //Overlay region top left pixel offset
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET1, val->overlayXoffset[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET2, val->overlayXoffset[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET3, val->overlayXoffset[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET4, val->overlayXoffset[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET5, val->overlayXoffset[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET6, val->overlayXoffset[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET7, val->overlayXoffset[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_XOFFSET8, val->overlayXoffset[7]);
    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET1, val->overlayYoffset[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET2, val->overlayYoffset[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET3, val->overlayYoffset[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET4, val->overlayYoffset[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET5, val->overlayYoffset[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET6, val->overlayYoffset[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET7, val->overlayYoffset[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YOFFSET8, val->overlayYoffset[7]);
    
    //Overlay region size
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH1, val->overlayWidth[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH2, val->overlayWidth[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH3, val->overlayWidth[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH4, val->overlayWidth[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH5, val->overlayWidth[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH6, val->overlayWidth[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH7, val->overlayWidth[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_WIDTH8, val->overlayWidth[7]);
    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT1, val->overlayHeight[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT2, val->overlayHeight[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT3, val->overlayHeight[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT4, val->overlayHeight[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT5, val->overlayHeight[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT6, val->overlayHeight[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT7, val->overlayHeight[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_HEIGHT8, val->overlayHeight[7]);
    
    //Overlay region enable
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE1, val->overlayEnable[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE2, val->overlayEnable[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE3, val->overlayEnable[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE4, val->overlayEnable[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE5, val->overlayEnable[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE6, val->overlayEnable[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE7, val->overlayEnable[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ENABLE8, val->overlayEnable[7]);
    
    //Overlay region format
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT1, val->overlayFormat[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT2, val->overlayFormat[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT3, val->overlayFormat[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT4, val->overlayFormat[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT5, val->overlayFormat[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT6, val->overlayFormat[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT7, val->overlayFormat[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_FORMAT8, val->overlayFormat[7]);
    
    //Overlay region alpha
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA1, val->overlayAlpha[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA2, val->overlayAlpha[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA3, val->overlayAlpha[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA4, val->overlayAlpha[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA5, val->overlayAlpha[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA6, val->overlayAlpha[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA7, val->overlayAlpha[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_ALPHA8, val->overlayAlpha[7]);
    
    //Overlay region Y stride
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE1, val->overlayYStride[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE2, val->overlayYStride[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE3, val->overlayYStride[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE4, val->overlayYStride[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE5, val->overlayYStride[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE6, val->overlayYStride[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE7, val->overlayYStride[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_YSTRIDE8, val->overlayYStride[7]);
    
    //Overlay region UV stride
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE1, val->overlayUVStride[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE2, val->overlayUVStride[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE3, val->overlayUVStride[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE4, val->overlayUVStride[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE5, val->overlayUVStride[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE6, val->overlayUVStride[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE7, val->overlayUVStride[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_UVSTRIDE8, val->overlayUVStride[7]);
    
    //Overlay region bitmap value
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY1, val->overlayBitmapY[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY2, val->overlayBitmapY[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY3, val->overlayBitmapY[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY4, val->overlayBitmapY[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY5, val->overlayBitmapY[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY6, val->overlayBitmapY[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY7, val->overlayBitmapY[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPY8, val->overlayBitmapY[7]);
    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU1, val->overlayBitmapU[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU2, val->overlayBitmapU[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU3, val->overlayBitmapU[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU4, val->overlayBitmapU[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU5, val->overlayBitmapU[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU6, val->overlayBitmapU[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU7, val->overlayBitmapU[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPU8, val->overlayBitmapU[7]);
    
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV1, val->overlayBitmapV[0]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV2, val->overlayBitmapV[1]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV3, val->overlayBitmapV[2]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV4, val->overlayBitmapV[3]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV5, val->overlayBitmapV[4]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV6, val->overlayBitmapV[5]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV7, val->overlayBitmapV[6]);
    EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_OVERLAY_BITMAPV8, val->overlayBitmapV[7]);
  }
	
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CHROMA_FORMAT_IDC, val->codedChromaIdc);

  /* video stabilization */
  EncAsicSetAddrRegisterValue(val->regMirror, HWIF_ENC_STAB_NEXT_LUMA_BASE, val->stabNextLumaBase);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MODE, val->stabMode);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MINIMUM, val->stabMotionMin);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MOTION_SUM, val->stabMotionSum);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_GMVX, val->stabGmvX);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_GMVY, val->stabGmvY);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX1, val->stabMatrix1);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX2, val->stabMatrix2);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX3, val->stabMatrix3);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX4, val->stabMatrix4);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX5, val->stabMatrix5);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX6, val->stabMatrix6);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX7, val->stabMatrix7);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX8, val->stabMatrix8);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_STAB_MATRIX9, val->stabMatrix9);

  /* latency of DDR's RAW(read after write) */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_DDRRAWLATENCY, val->LatencyOfDdrRAW);
  
  val->regMirror[HWIF_REG_CFG1] = EWLReadReg(ewl, HWIF_REG_CFG1*4);
  val->regMirror[HWIF_REG_CFG2] = EWLReadReg(ewl, HWIF_REG_CFG2*4);
  val->regMirror[HWIF_REG_CFG3] = EWLReadReg(ewl, HWIF_REG_CFG3*4);
  val->regMirror[HWIF_REG_CFG4] = EWLReadReg(ewl, HWIF_REG_CFG4*4);

  for (i = 1; i < ASIC_SWREG_AMOUNT; i++)
    EWLWriteReg(ewl, HSWREG(i), val->regMirror[i]);

  /* Write JPEG quantization tables to regs if needed (JPEG) */
  if(val->codingType == ASIC_JPEG)
  {
      for(i = 0; i < 128; i += 4)
      {
          /* swreg[83] to swreg[114] */
          EWLWriteReg(ewl, HSWREG(83 + (i/4)),
                      (val->quantTable[i    ] << 24) |
                      (val->quantTable[i + 1] << 16) |
                      (val->quantTable[i + 2] << 8) |
                      (val->quantTable[i + 3]));
      }
  }

#ifdef TRACE_REGS
  if (dumpRegister == 1)  
    EncTraceRegs(ewl, 0, 0);
#endif

  /* Register with enable bit is written last */
  val->regMirror[ASIC_REG_INDEX_STATUS] |= ASIC_STATUS_ENABLE;

  EWLEnableHW(ewl, HSWREG(ASIC_REG_INDEX_STATUS), val->regMirror[ASIC_REG_INDEX_STATUS]);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicFrameContinue(const void *ewl, regValues_s *val)
{
  /* clear status bits, clear IRQ => HW restart */
  u32 status = val->regMirror[1];

  status &= (~ASIC_STATUS_ALL);
  status &= ~ASIC_IRQ_LINE;

  val->regMirror[1] = status;

  /*CheckRegisterValues(val); */

  /* Write only registers which may be updated mid frame */
  // EWLWriteReg(ewl, HSWREG(24), (val->rlcLimitSpace / 2));

  //val->regMirror[5] = val->rlcBase;
  EWLWriteReg(ewl, HSWREG(5), val->regMirror[5]);

#ifdef TRACE_REGS
  EncTraceRegs(ewl, 0, (EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER)|(EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER_MSB)<<13)|(EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER_MSB2)<<17)));
#endif

  /* Register with status bits is written last */
  EWLEnableHW(ewl, HSWREG(ASIC_REG_INDEX_STATUS), val->regMirror[ASIC_REG_INDEX_STATUS]);

}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicGetRegisters(const void *ewl, regValues_s *val,u32 dumpRegister)
{
  (void)ewl;
  (void)val;

  /* HW output stream size, bits to bytes */
  if(val->codingType == ASIC_JPEG)
    val->outputStrmSize[0] =   EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
  val->hashoffset = EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_HASH_OFFSET);
  val->hashval = EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_HASH_VAL);


#ifdef TRACE_REGS
  int i;

  for (i = 1; i < ASIC_SWREG_AMOUNT; i++)
  {
    val->regMirror[i] = EWLReadReg(ewl, HSWREG(i));
  }

  if (dumpRegister == 1)
    EncTraceRegs(ewl, 1, (EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER)|(EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER_MSB)<<13)|(EncAsicGetRegisterValue(ewl, val->regMirror, HWIF_ENC_ENCODED_CTB_NUMBER_MSB)<<17)));
#endif

}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicStop(const void *ewl)
{
  EWLDisableHW(ewl, HSWREG(ASIC_REG_INDEX_STATUS), 0);
}
/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 EncAsicGetPerformance(const void *ewl)
{
    return EWLGetPerformance(ewl);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 EncAsicGetStatus(const void *ewl)
{
  return EWLReadReg(ewl, HSWREG(1));
}
/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

void EncAsicClearStatus(const void *ewl,u32 value)
{
  EWLWriteReg(ewl, HSWREG(1),value);
}

/*------------------------------------------------------------------------------
    EncAsicGetMvOutput
        Return encOutputMbInfo_s pointer to beginning of macroblock mbNum
------------------------------------------------------------------------------*/
u32 *EncAsicGetMvOutput(asicData_s *asic, u32 mbNum)
{
  u32 *mvOutput = asic->mvOutput.virtualAddress;
  u32 traceMbTiming = 0;//asic->regs.traceMbTiming;

  if (traceMbTiming)    /* encOutputMbInfoDebug_s */
  {
    if (asic->regs.asicHwId <= ASIC_ID_EVERGREEN_PLUS)
    {
      mvOutput += mbNum * 40 / 4;
    }
    else
    {
      mvOutput += mbNum * 56 / 4;
    }
  }
  else        /* encOutputMbInfo_s */
  {
    if (asic->regs.asicHwId <= ASIC_ID_EVERGREEN_PLUS)
    {
      mvOutput += mbNum * 32 / 4;
    }
    else
    {
      mvOutput += mbNum * 48 / 4;
    }
  }

  return mvOutput;
}


u32 EncAsicGetAsicHWid(u32 core_id)
{
  u32 i = 0;
  u32 hw_id = 0;
  static u32 asic_hw_id[MAX_SUPPORT_CORE_NUM];

  if (core_id > EWLGetCoreNum()-1)
    return hw_id;

  if (asic_hw_id[core_id] == 0)
  {
    for (i=0; i < EWLGetCoreNum(); i++)
    {
      asic_hw_id[i] = EWLReadAsicID(i);
    }
  }

  return asic_hw_id[core_id];
}


u32 EncAsicGetCoreIdByFormat(u32 client_type)
{
  u32 i = 0;
  u32 core_has_found = 0;
  EWLHwConfig_t config;
  
  for (i=0; i<EWLGetCoreNum(); i++)
  {
    config = EncAsicGetAsicConfig(i);
    
    switch (client_type)
    {
      case EWL_CLIENT_TYPE_H264_ENC:
        if (config.h264Enabled == 1)
          return i;
        break;

      case EWL_CLIENT_TYPE_HEVC_ENC:
        if (config.hevcEnabled == 1)
          return i;
        break;

      case EWL_CLIENT_TYPE_JPEG_ENC:
        if (config.jpegEnabled == 1)
          return i;
        break;
      case EWL_CLIENT_TYPE_VIDEOSTAB:
        if (config.vsSupport == 1)
          return i;
        break;
        
      default:
        break;
    }
  }

  return i;
}


EWLHwConfig_t EncAsicGetAsicConfig(u32 core_id)
{
  u32 i = 0;
  static EWLHwConfig_t asic_core_cfg[MAX_SUPPORT_CORE_NUM];
  EWLHwConfig_t config;
  
  memset(&config, 0, sizeof(config));

  if (core_id > EWLGetCoreNum()-1)
    return config;

  if (memcmp(&asic_core_cfg[core_id], &config, sizeof(EWLHwConfig_t)) == 0)
  {
    for (i=0; i < EWLGetCoreNum(); i++)
    {
      asic_core_cfg[i] = EWLReadAsicConfig(i);
    }
  }

  return asic_core_cfg[core_id];
}

