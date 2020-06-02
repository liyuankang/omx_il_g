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
#include "encpreprocess.h"
#include "encasiccontroller.h"
#include "enccommon.h"
#include "ewl.h"


/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF

#define HSWREG(n)       ((n)*4)

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    EncAsicMemAlloc_V2

    Allocate HW/SW shared memory

    Input:
        asic        asicData structure
        width       width of encoded image, multiple of four
        height      height of encoded image
        numRefBuffs amount of reference luma frame buffers to be allocated

    Output:
        asic        base addresses point to allocated memory areas

    Return:
        ENCHW_OK        Success.
        ENCHW_NOK       Error: memory allocation failed, no memories allocated
                        and EWL instance released

------------------------------------------------------------------------------*/
i32 EncAsicMemAlloc_V2(asicData_s *asic, asicMemAlloc_s *allocCfg)
{
  u32  i;
  u32 width = allocCfg->width;
  u32 height = allocCfg->height;
  bool codecH264 = allocCfg->encodingType == ASIC_H264;
  u32 alignment = allocCfg->ref_alignment;
  u32 alignment_ch = allocCfg->ref_ch_alignment;
  regValues_s *regs;
  EWLLinearMem_t *buff = NULL;
  u32 internalImageLumaSize, u32FrameContextSize;
  u32 total_allocated_buffers;
  u32 width_4n, height_4n;
  u32 block_unit,block_size;
  u32 maxSliceNals;
  u32 log2_ctu_size = (codecH264?4:6);
  u32 ctu_size = (1<<log2_ctu_size);
  i32 ctb_per_row = ((width + ctu_size - 1) / ctu_size);
  i32 ctb_per_column = ((height + ctu_size - 1) / ctu_size);
  i32 ctb_per_picture = ctb_per_row * ctb_per_column;

  ASSERT(asic != NULL);
  ASSERT(width != 0);
  ASSERT(height != 0);
  ASSERT((height % 2) == 0);
  ASSERT((width % 2) == 0);
  //scaledWidth=((scaledWidth+7)/8)*8;
  regs = &asic->regs;

  regs->codingType = allocCfg->encodingType;

 if(allocCfg->encodingType == ASIC_JPEG || allocCfg->encodingType == ASIC_CUTREE)
    return ENCHW_OK;

  /*  
    allocate scan coeff buffer before reference frame to avoid roimap overeading(64~1536 bytes) hit reference frame:
    reference frame could be in DEC400(1024B burst size)
   */
  for (i = 0; i < MAX_CORE_NUM; i++)
  { 
    if (EWLMallocLinear(asic->ewl, 288 * 1024 / 8,alignment,
          &asic->compress_coeff_SACN[i]) != EWL_OK)
    {
      EncAsicMemFree_V2(asic);
      return ENCHW_NOK;
    }
  }


  ASSERT(allocCfg->numRefBuffsLum < ASIC_FRAME_BUF_LUM_MAX);
  total_allocated_buffers = allocCfg->numRefBuffsLum + 1;

  width = ((width + 63) >> 6) << 6;
  height = ((height + 63) >> 6) << 6;
  //width_4n = width / 4;
  width_4n = ((allocCfg->width + 15) / 16) * 4;
  height_4n = height / 4;
    
  asic->regs.recon_chroma_half_size = ((width * height)+(width * height)*(allocCfg->bitDepthLuma-8)/8) / 4;

  {
    #ifdef RECON_REF_1KB_BURST_RW
	  asic->regs.ref_frame_stride = STRIDE((STRIDE(width,128)*8),alignment); 
      u32 lumaSize = asic->regs.ref_frame_stride * (height/(8/(allocCfg->bitDepthLuma/8)));
	#else  
	  asic->regs.ref_frame_stride = STRIDE(STRIDE(width*4*allocCfg->bitDepthLuma/8,16),alignment); 
      u32 lumaSize = asic->regs.ref_frame_stride * height/4;
	#endif
    u32 lumaSize4N = STRIDE(STRIDE(width_4n*4*allocCfg->bitDepthLuma/8,16),alignment) * height_4n/4;

    u32FrameContextSize = (regs->codingType == ASIC_AV1 || regs->codingType == ASIC_VP9) ? FRAME_CONTEXT_LENGTH : 0;
	internalImageLumaSize = ((lumaSize + lumaSize4N + u32FrameContextSize + 0xf) & (~0xf)) + 64;
    internalImageLumaSize = STRIDE(internalImageLumaSize, alignment);
    u32 chromaSize = lumaSize/2;
#ifdef RECON_REF_1KB_BURST_RW
    if(allocCfg->compressor & 2) {
      chromaSize = STRIDE((width*4*allocCfg->bitDepthChroma/8), alignment_ch) * (height/8);
      asic->regs.recon_chroma_half_size = chromaSize/2;
    }
#endif
  
    for (i = 0; i < total_allocated_buffers; i++)
    {
      if(!allocCfg->exteralReconAlloc)
      {
        if (EWLMallocRefFrm(asic->ewl, internalImageLumaSize,alignment,
                            &asic->internalreconLuma[i]) != EWL_OK)
        {
          EncAsicMemFree_V2(asic);
          return ENCHW_NOK;
        }
    
        asic->internalreconLuma_4n[i].busAddress = asic->internalreconLuma[i].busAddress + lumaSize;
        asic->internalreconLuma_4n[i].size = lumaSize4N;
        asic->internalAv1FrameContext[i].busAddress = asic->internalreconLuma[i].busAddress + lumaSize + lumaSize4N;
        asic->internalAv1FrameContext[i].size       = u32FrameContextSize;
      }
    }
  
    for (i = 0; i < total_allocated_buffers; i++)
    {
      if(!allocCfg->exteralReconAlloc)
      {
        if (EWLMallocRefFrm(asic->ewl, chromaSize,alignment_ch,
                            &asic->internalreconChroma[i]) != EWL_OK)
        {
          EncAsicMemFree_V2(asic);
          return ENCHW_NOK;
        }
      }
      
    }
  
    asic->regs.ref_ds_luma_stride = STRIDE(STRIDE(width_4n*4*allocCfg->bitDepthLuma/8,16),alignment);
    asic->regs.ref_frame_stride_ch = STRIDE((width*4*allocCfg->bitDepthChroma/8), alignment_ch);
  
    /*  VP9: The table is used for probability tables, 1208 bytes. */
    if (regs->codingType == ASIC_VP9) 
    {
      i = 8 * 55 + 8 * 96;
      if (EWLMallocLinear(asic->ewl, i,0, &asic->cabacCtx) != EWL_OK)
      {
        EncAsicMemFree_V2(asic);
        return ENCHW_NOK;
      }
    }

    regs->cabacCtxBase = asic->cabacCtx.busAddress;

    if (regs->codingType == ASIC_VP9)
    {
      /* VP9: Table of counter for probability updates. */
      if (EWLMallocLinear(asic->ewl, ASIC_VP9_PROB_COUNT_SIZE,0,
                          &asic->probCount) != EWL_OK)
      {
        EncAsicMemFree_V2(asic);
        return ENCHW_NOK;
      }
      regs->probCountBase = asic->probCount.busAddress;

    }


    /* NAL size table, table size must be 64-bit multiple,
     * space for SEI, MVC prefix, filler and zero at the end of table.
     * Atleast 1 macroblock row in every slice.
     */
    maxSliceNals = (height + 15) / 16;
    if (codecH264 && (allocCfg->maxTemporalLayers > 1))
      maxSliceNals *= 2;
    asic->sizeTblSize = STRIDE((((sizeof(u32) * (maxSliceNals + 1) + 7) & (~7)) + (sizeof(u32) * 10)), alignment);

    for (i = 0; i < MAX_CORE_NUM; i++)
    {
      buff = &asic->sizeTbl[i];
      if (EWLMallocLinear(asic->ewl, asic->sizeTblSize,alignment, buff) != EWL_OK)
      {
        EncAsicMemFree_V2(asic);
        return ENCHW_NOK;
      }
    }

    /* Ctb RC*/
    if(allocCfg->ctbRcMode & 2)
    {
      i32 ctbRcMadsize = width*height/ctu_size/ctu_size; // 1 byte per ctb
      for (i = 0; i < CTB_RC_BUF_NUM; i ++)
      {
        if (EWLMallocLinear(asic->ewl, ctbRcMadsize, alignment, &(asic->ctbRcMem[i])) != EWL_OK)
        {
          EncAsicMemFree_V2(asic);
          return ENCHW_NOK;
        }
      }
    }

    //allocate compressor table
    if (allocCfg->compressor)
    {
      u32 tblLumaSize = 0;
      u32 tblChromaSize = 0;
      u32 tblSize = 0;
      if (allocCfg->compressor & 1)
      {
        tblLumaSize = ((width + 63) / 64) * ((height + 63) / 64) * 8; //ctu_num * 8
        tblLumaSize = ((tblLumaSize + 15) >> 4) << 4;
      }
      if (allocCfg->compressor & 2)
      {
        int cbs_w = ((width >> 1) + 7) / 8;
        int cbs_h = ((height >> 1) + 3) / 4;
        int cbsg_w = (cbs_w + 15) / 16;
        tblChromaSize = cbsg_w * cbs_h * 16;
      }

      tblSize = tblLumaSize + tblChromaSize;

      for (i = 0; i < total_allocated_buffers; i++)
      {
        if(!allocCfg->exteralReconAlloc)
        {
          if (EWLMallocLinear(asic->ewl, tblSize, alignment,&asic->compressTbl[i]) != EWL_OK)
          {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
          }
        }
      }
    }

    //allocate collocated buffer for H.264
    if (codecH264)
    {
      // 4 bits per mb, 2 mbs per struct h264_mb_col
      u32 bufSize = (ctb_per_picture+1)/2*sizeof(struct h264_mb_col);

      for (i = 0; i < total_allocated_buffers; i++)
      {
        if(!allocCfg->exteralReconAlloc)
        {
          if (EWLMallocLinear(asic->ewl, bufSize, alignment,&asic->colBuffer[i]) != EWL_OK)
          {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
          }
        }
      }
    }

    //allocation CU infomation memory
    if (allocCfg->outputCuInfo)
    {
      i32 cuInfoSizes[] = {CU_INFO_OUTPUT_SIZE, CU_INFO_OUTPUT_SIZE_V1, CU_INFO_OUTPUT_SIZE_V2, CU_INFO_OUTPUT_SIZE_V3};
      i32 ctuNum = (width >> log2_ctu_size) * (height >> log2_ctu_size);
      i32 maxCuNum = ctuNum * (ctu_size/8)*(ctu_size/8);
      i32 cuInfoVersion = regs->asicCfg.cuInforVersion;
      if(cuInfoVersion == 2 || cuInfoVersion == 3)
        cuInfoVersion = (regs->asicCfg.bMultiPassSupport && allocCfg->pass == 1 ? cuInfoVersion : 1);
      i32 infoSizePerCu = cuInfoSizes[cuInfoVersion];
      i32 cuInfoTblSize = ctuNum * CU_INFO_TABLE_ITEM_SIZE;
      i32 cuInfoSize = maxCuNum * infoSizePerCu;
      i32 cuInfoTotalSize = 0;

      cuInfoTblSize = ((cuInfoTblSize + 63) >> 6) << 6;
      cuInfoSize = ((cuInfoSize + 63) >> 6) << 6;
      cuInfoTotalSize = cuInfoTblSize + cuInfoSize;
      asic->cuInfoTableSize = cuInfoTblSize;

      if(!allocCfg->exteralReconAlloc)
      {
        if (EWLMallocLinear(asic->ewl, cuInfoTotalSize*allocCfg->numCuInfoBuf, alignment,&asic->cuInfoMem[0]) != EWL_OK)
        {
          EncAsicMemFree_V2(asic);
          return ENCHW_NOK;
        }
        i32 total_size = asic->cuInfoMem[0].size;
        for (i = 0; i < allocCfg->numCuInfoBuf; i++)
        {
          asic->cuInfoMem[i].virtualAddress = (u32*)((ptr_t)asic->cuInfoMem[0].virtualAddress + i*cuInfoTotalSize);
          asic->cuInfoMem[i].busAddress = asic->cuInfoMem[0].busAddress + i*cuInfoTotalSize;
          asic->cuInfoMem[i].size = (i < allocCfg->numCuInfoBuf-1 ? cuInfoTotalSize : total_size - (allocCfg->numCuInfoBuf-1)*cuInfoTotalSize);
        }
      }
    }
  }
  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    EncAsicMemFree_V2

    Free HW/SW shared memory

------------------------------------------------------------------------------*/
void EncAsicMemFree_V2(asicData_s *asic)
{
  i32 i;

  ASSERT(asic != NULL);
  ASSERT(asic->ewl != NULL);

  for (i = 0; i < ASIC_FRAME_BUF_LUM_MAX; i++)
  {
    if (asic->internalreconLuma[i].virtualAddress != NULL)
      EWLFreeRefFrm(asic->ewl, &asic->internalreconLuma[i]);
    asic->internalreconLuma[i].virtualAddress = NULL;

    if (asic->internalreconChroma[i].virtualAddress != NULL)
      EWLFreeRefFrm(asic->ewl, &asic->internalreconChroma[i]);
    asic->internalreconChroma[i].virtualAddress = NULL;

    if (asic->compressTbl[i].virtualAddress != NULL)
      EWLFreeRefFrm(asic->ewl, &asic->compressTbl[i]);
    asic->compressTbl[i].virtualAddress = NULL;

    if (asic->colBuffer[i].virtualAddress != NULL)
      EWLFreeRefFrm(asic->ewl, &asic->colBuffer[i]);
    asic->colBuffer[i].virtualAddress = NULL;
  }

  if (asic->cuInfoMem[0].virtualAddress != NULL) {
    for (i = 1; i < ASIC_FRAME_BUF_CUINFO_MAX; i ++)
    {
      if(asic->cuInfoMem[i].virtualAddress)
        asic->cuInfoMem[0].size += asic->cuInfoMem[i].size;
      asic->cuInfoMem[i].virtualAddress = NULL;
    }
    EWLFreeRefFrm(asic->ewl, &asic->cuInfoMem[0]);
    asic->cuInfoMem[0].virtualAddress = NULL;
  }

  if (asic->cabacCtx.virtualAddress != NULL)
    EWLFreeLinear(asic->ewl, &asic->cabacCtx);

  if (asic->probCount.virtualAddress != NULL)
    EWLFreeLinear(asic->ewl, &asic->probCount);

  for (i = 0; i < MAX_CORE_NUM; i++)
  {
    if(asic->compress_coeff_SACN[i].virtualAddress != NULL)
      EWLFreeLinear(asic->ewl, &asic->compress_coeff_SACN[i]);

    if (asic->sizeTbl[i].virtualAddress != NULL)
      EWLFreeLinear(asic->ewl, &asic->sizeTbl[i]);
    asic->sizeTbl[i].virtualAddress = NULL;
  }

  for (i = 0; i < 4; i ++)
  {
    if (asic->ctbRcMem[i].virtualAddress != NULL)
      EWLFreeLinear(asic->ewl, &(asic->ctbRcMem[i]));
    asic->ctbRcMem[i].virtualAddress = NULL;
  }

  if (asic->loopbackLineBufMem.virtualAddress != NULL)
    EWLFreeLinear(asic->ewl, &asic->loopbackLineBufMem);
  asic->loopbackLineBufMem.virtualAddress = NULL;

  asic->cabacCtx.virtualAddress = NULL;
  asic->mvOutput.virtualAddress = NULL;
  asic->probCount.virtualAddress = NULL;
  asic->segmentMap.virtualAddress = NULL;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
i32 EncAsicCheckStatus_V2(asicData_s *asic,u32 status)
{
  i32 ret;
  u32 dumpRegister = asic->dumpRegister;

  status &= ASIC_STATUS_ALL;

  if (status & ASIC_STATUS_ERROR)
  {
    /* Get registers for debugging */
    EncAsicGetRegisters(asic->ewl, &asic->regs, dumpRegister);
    ret = ASIC_STATUS_ERROR;
  }
  else if (status & ASIC_STATUS_FUSE_ERROR)
  {
    /* Get registers for debugging */
    EncAsicGetRegisters(asic->ewl, &asic->regs, dumpRegister);
    ret = ASIC_STATUS_ERROR;
  }
  else if (status & ASIC_STATUS_HW_TIMEOUT)
  {
    /* Get registers for debugging */
    EncAsicGetRegisters(asic->ewl, &asic->regs, dumpRegister);
    ret = ASIC_STATUS_HW_TIMEOUT;
  }
  else if (status & ASIC_STATUS_FRAME_READY)
  {
    /* read out all register */
    EncAsicGetRegisters(asic->ewl, &asic->regs, dumpRegister);
    ret = ASIC_STATUS_FRAME_READY;
  }
  else if (status & ASIC_STATUS_BUFF_FULL)
  {
    /* ASIC doesn't support recovery from buffer full situation,
     * at the same time with buff full ASIC also resets itself. */
    ret = ASIC_STATUS_BUFF_FULL;
  }
  else if (status & ASIC_STATUS_HW_RESET)
  {
    ret = ASIC_STATUS_HW_RESET;
  }
  else if (status & ASIC_STATUS_SEGMENT_READY)
  {
    ret = ASIC_STATUS_SEGMENT_READY;
  }
  else
  {
    ret = status;
  }
 
  return ret;
}
