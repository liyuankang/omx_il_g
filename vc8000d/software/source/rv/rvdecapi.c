/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include "version.h"
#include "basetype.h"
#include "rv_container.h"
#include "rv_utils.h"
#include "rv_strm.h"
#include "rvdecapi.h"
#include "rvdecapi_internal.h"
#include "dwl.h"
#include "regdrv.h"
#include "rv_headers.h"
#include "deccfg.h"
#include "refbuffer.h"
#include "rv_rpr.h"
#include "ppu.h"
#include "rv_debug.h"

#include "tiledref.h"
#include "commonconfig.h"
#include "errorhandling.h"
#include "vpufeature.h"
#include "sw_util.h"

#ifdef RVDEC_TRACE
#define RV_API_TRC(str)    RvDecTrace((str))
#else
#define RV_API_TRC(str)

#endif

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

#define RV_BUFFER_UNDEFINED    16

#define RV_DEC_X170_MODE 8 /* TODO: What's the right mode for Hukka */

#define ID8170_DEC_TIMEOUT        0xFFU
#define ID8170_DEC_SYSTEM_ERROR   0xFEU
#define ID8170_DEC_HW_RESERVED    0xFDU

#define RVDEC_UPDATE_POUTPUT

#define RVDEC_NON_PIPELINE_AND_B_PICTURE \
        ((!dec_cont->pp_config_query.pipeline_accepted) \
             && dec_cont->FrameDesc.pic_coding_type == RV_B_PIC)

static u32 RvCheckFormatSupport(void);
static u32 rvHandleVlcModeError(RvDecContainer * dec_cont, u32 pic_num);
static void rvHandleFrameEnd(RvDecContainer * dec_cont);
static u32 RunDecoderAsic(RvDecContainer * dec_cont, addr_t strm_bus_address);
static u32 RvSetRegs(RvDecContainer * dec_cont, addr_t strm_bus_address);
static void RvFillPicStruct(RvDecPicture * picture,
                            RvDecContainer * dec_cont, u32 pic_index);
static void RvSetExternalBufferInfo(RvDecContainer * dec_cont);

static RvDecRet RvDecNextPicture_INTERNAL(RvDecInst dec_inst,
                                   RvDecPicture * picture, u32 end_of_stream);

static void RvEnterAbortState(RvDecContainer *dec_cont);
static void RvExistAbortState(RvDecContainer *dec_cont);
static void RvEmptyBufferQueue(RvDecContainer *dec_cont);
static void RvCheckBufferRealloc(RvDecContainer *dec_cont);
/*------------------------------------------------------------------------------
       Version Information - DO NOT CHANGE!
------------------------------------------------------------------------------*/

#define RVDEC_MAJOR_VERSION 0
#define RVDEC_MINOR_VERSION 0

/*------------------------------------------------------------------------------

    Function: RvDecGetAPIVersion

        Functional description:
            Return version information of API

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

RvDecApiVersion RvDecGetAPIVersion() {
  RvDecApiVersion ver;

  ver.major = RVDEC_MAJOR_VERSION;
  ver.minor = RVDEC_MINOR_VERSION;

  return (ver);
}

/*------------------------------------------------------------------------------

    Function: RvDecGetBuild

        Functional description:
            Return build information of SW and HW

        Returns:
            RvDecBuild

------------------------------------------------------------------------------*/

RvDecBuild RvDecGetBuild(void) {
  RvDecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_RV_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_RV_DEC);

  RV_API_TRC("RvDecGetBuild# OK\n");

  return build_info;
}

/*------------------------------------------------------------------------------

    Function: RvDecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance.

        Inputs:
            error_handling
                            Flag to determine which error concealment method to use.

        Outputs:
            dec_inst        pointer to initialized instance is stored here

        Returns:
            RVDEC_OK       successfully initialized the instance
            RVDEC_MEM_FAIL memory allocation failed

------------------------------------------------------------------------------*/
RvDecRet RvDecInit(RvDecInst * dec_inst,
                   const void *dwl,
                   enum DecErrorHandling error_handling,
                   u32 frame_code_length, u32 *frame_sizes, u32 rv_version,
                   u32 max_frame_width, u32 max_frame_height,
                   u32 num_frame_buffers, enum DecDpbFlags dpb_flags,
                   u32 use_adaptive_buffers, u32 n_guard_size) {
  /*@null@ */ RvDecContainer *dec_cont;
  u32 i = 0;
  u32 ret;
  u32 reference_frame_format;
  u32 asic_id, hw_build_id;

  DWLHwConfig config;
  struct DecHwFeatures hw_feature;

  RV_API_TRC("RvDecInit#");
  RVDEC_API_DEBUG(("RvAPI_DecoderInit#"));

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if(((-1) >> 1) != (-1)) {
    RVDEC_API_DEBUG(("RVDecInit# ERROR: Right shift is not signed"));
    return (RVDEC_INITFAIL);
  }
  /*lint -restore */

  if(dec_inst == NULL) {
    RVDEC_API_DEBUG(("RVDecInit# ERROR: dec_inst == NULL"));
    return (RVDEC_PARAM_ERROR);
  }

  *dec_inst = NULL;

  /* check that RV decoding supported in HW */
  if(RvCheckFormatSupport()) {
    RVDEC_API_DEBUG(("RVDecInit# ERROR: RV not supported in HW\n"));
    return RVDEC_FORMAT_NOT_SUPPORTED;
  }

  RVDEC_API_DEBUG(("size of RvDecContainer %d \n", sizeof(RvDecContainer)));
  dec_cont = (RvDecContainer *) DWLmalloc(sizeof(RvDecContainer));

  if(dec_cont == NULL) {
    return (RVDEC_MEMFAIL);
  }

  /* set everything initially zero */
  (void) DWLmemset(dec_cont, 0, sizeof(RvDecContainer));

  ret = DWLMallocLinear(dwl,
                        RV_DEC_X170_MAX_NUM_SLICES*sizeof(u32),
                        &dec_cont->StrmStorage.slices);

  if( ret == HANTRO_NOK ) {
    DWLfree(dec_cont);
    return (RVDEC_MEMFAIL);
  }

  dec_cont->dwl = dwl;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);

  dec_cont->ApiStorage.DecStat = INITIALIZED;

  *dec_inst = (RvDecContainer *) dec_cont;

  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_RV_DEC);
  if((asic_id >> 16) == 0x8170U)
    error_handling = 0;

  dec_cont->rv_regs[0] = asic_id;
  for(i = 1; i < TOTAL_X170_REGISTERS; i++)
    dec_cont->rv_regs[i] = 0;

  SetCommonConfigRegs(dec_cont->rv_regs);

  (void) DWLmemset(&config, 0, sizeof(DWLHwConfig));

  DWLReadAsicConfig(&config, DWL_CLIENT_TYPE_RV_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
    RVDEC_API_DEBUG("RVDecInit# ERROR: HW not support 64bit address!\n");
    return (RVDEC_PARAM_ERROR);
  }

  if (hw_feature.ref_frame_tiled_only)
    dpb_flags = DEC_REF_FRM_TILED_DEFAULT | DEC_DPB_ALLOW_FIELD_ORDERING;

  dec_cont->ref_buf_support = hw_feature.ref_buf_support;
  reference_frame_format = dpb_flags & DEC_REF_FRM_FMT_MASK;
  if(reference_frame_format == DEC_REF_FRM_TILED_DEFAULT) {
    /* Assert support in HW before enabling.. */
    if(!hw_feature.tiled_mode_support) {
      return RVDEC_FORMAT_NOT_SUPPORTED;
    }
    dec_cont->tiled_mode_support = hw_feature.tiled_mode_support;
  } else
    dec_cont->tiled_mode_support = 0;
  if (hw_feature.strm_len_32bits)
    dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VC8000D;
  else
    dec_cont->max_strm_len = DEC_X170_MAX_STREAM;
#if 0
  /* Down scaler ratio */
  if ((dscale_cfg->down_scale_x == 0) || (dscale_cfg->down_scale_y == 0)) {
    dec_cont->pp_enabled = 0;
    dec_cont->down_scale_x = 0;   /* Meaningless when pp not enabled. */
    dec_cont->down_scale_y = 0;
  } else if ((dscale_cfg->down_scale_x != 1 &&
              dscale_cfg->down_scale_x != 2 &&
              dscale_cfg->down_scale_x != 4 &&
              dscale_cfg->down_scale_x != 8 ) ||
             (dscale_cfg->down_scale_y != 1 &&
              dscale_cfg->down_scale_y != 2 &&
              dscale_cfg->down_scale_y != 4 &&
              dscale_cfg->down_scale_y != 8 )) {
    return (RVDEC_PARAM_ERROR);
  } else {
    u32 scale_table[9] = {0, 0, 1, 0, 2, 0, 0, 0, 3};

    dec_cont->pp_enabled = 1;
    dec_cont->down_scale_x = dscale_cfg->down_scale_x;
    dec_cont->down_scale_y = dscale_cfg->down_scale_y;
    dec_cont->dscale_shift_x = scale_table[dscale_cfg->down_scale_x];
    dec_cont->dscale_shift_y = scale_table[dscale_cfg->down_scale_y];
  }
#endif
  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    return (RVDEC_MEMFAIL);
  }
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.intra_freeze = error_handling == DEC_EC_VIDEO_FREEZE;
  if (error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->StrmStorage.partial_freeze = 1;
  else if (error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->StrmStorage.partial_freeze = 2;
  dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;
  if(num_frame_buffers > 16)
    num_frame_buffers = 16;
  dec_cont->StrmStorage.max_num_buffers = num_frame_buffers;

  dec_cont->StrmStorage.is_rv8 = rv_version == 0;
  if (rv_version == 0 && frame_sizes != NULL) {
    dec_cont->StrmStorage.frame_code_length = frame_code_length;
    DWLmemcpy(dec_cont->StrmStorage.frame_sizes, frame_sizes,
              18*sizeof(u32));
    SetDecRegister(dec_cont->rv_regs, HWIF_FRAMENUM_LEN,
                   frame_code_length);
  }

  /* prediction filter taps */
  if (rv_version == 0) {
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_0, -1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_1, 12);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_2,  6);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_0,  6);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_1,  9);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_2,  1);
  } else {
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_0,  1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_1, -5);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_2, 20);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_0,  1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_1, -5);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_2, 52);
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_MODE, RV_DEC_X170_MODE);
  SetDecRegister(dec_cont->rv_regs, HWIF_RV_PROFILE, rv_version != 0);

  dec_cont->StrmStorage.max_frame_width = max_frame_width;
  dec_cont->StrmStorage.max_frame_height = max_frame_height;
  dec_cont->StrmStorage.max_mbs_per_frame =
    ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
    ((dec_cont->StrmStorage.max_frame_height+15)>>4);

  InitWorkarounds(RV_DEC_X170_MODE, &dec_cont->workarounds);

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK)
    return RVDEC_MEMFAIL;

  dec_cont->use_adaptive_buffers = use_adaptive_buffers;
  dec_cont->n_guard_size = n_guard_size;

  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */
  if (dec_cont->tiled_mode_support) {
    dec_cont->min_dec_pic_width = RV_MIN_WIDTH_EN_DTRC;
    dec_cont->min_dec_pic_height = RV_MIN_HEIGHT_EN_DTRC;
  }
  else {
    dec_cont->min_dec_pic_width = RV_MIN_WIDTH;
    dec_cont->min_dec_pic_height = RV_MIN_HEIGHT;
  }

  RVDEC_API_DEBUG(("Container 0x%x\n", (u32) dec_cont));
  RV_API_TRC("RvDecInit: OK\n");

  return (RVDEC_OK);
}

/*------------------------------------------------------------------------------

    Function: RvDecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before RvDecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            RVDEC_OK            success
            RVDEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
RvDecRet RvDecGetInfo(RvDecInst dec_inst, RvDecInfo * dec_info) {

  DecFrameDesc *p_frame_d;
  DecApiStorage *p_api_stor;
  DecHdrs *p_hdrs;

  RV_API_TRC("RvDecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    return RVDEC_PARAM_ERROR;
  }

  p_api_stor = &((RvDecContainer*)dec_inst)->ApiStorage;
  p_frame_d = &((RvDecContainer*)dec_inst)->FrameDesc;
  p_hdrs = &((RvDecContainer*)dec_inst)->Hdrs;

  dec_info->multi_buff_pp_size = 2;
  dec_info->pic_buff_size = ((RvDecContainer *)dec_inst)->buf_num;

  if(p_api_stor->DecStat == UNINIT || p_api_stor->DecStat == INITIALIZED) {
    return RVDEC_HDRS_NOT_RDY;
  }

  dec_info->frame_width = p_frame_d->frame_width;
  dec_info->frame_height = p_frame_d->frame_height;

  dec_info->coded_width = p_hdrs->horizontal_size;
  dec_info->coded_height = p_hdrs->vertical_size;

  dec_info->dpb_mode = DEC_DPB_FRAME;

  if(((RvDecContainer *)dec_inst)->tiled_mode_support)
    dec_info->output_format = RVDEC_TILED_YUV420;
  else
    dec_info->output_format = RVDEC_SEMIPLANAR_YUV420;

  RV_API_TRC("RvDecGetInfo: OK");
  return (RVDEC_OK);

}

/*------------------------------------------------------------------------------

    Function: RvDecDecode

        Functional description:
            Decode stream data. Calls StrmDec_Decode to do the actual decoding.

        Input:
            dec_inst     decoder instance
            input      pointer to input struct

        Outputs:
            output     pointer to output struct

        Returns:
            RVDEC_NOT_INITIALIZED   decoder instance not initialized yet
            RVDEC_PARAM_ERROR       invalid parameters

            RVDEC_STRM_PROCESSED    stream buffer decoded
            RVDEC_HDRS_RDY          headers decoded
            RVDEC_PIC_DECODED       decoding of a picture finished
            RVDEC_STRM_ERROR        serious error in decoding, no
                                       valid parameter sets available
                                       to decode picture data

------------------------------------------------------------------------------*/

RvDecRet RvDecDecode(RvDecInst dec_inst,
                     RvDecInput * input, RvDecOutput * output) {

  RvDecContainer *dec_cont;
  RvDecRet internal_ret;
  DecApiStorage *p_api_stor;
  DecStrmDesc *p_strm_desc;
  u32 strm_dec_result;
  u32 asic_status;
  i32 ret = 0;
  u32 error_concealment = 0;
  u32 i;
  u32 align;
  u32 *slice_info;
  u32 contains_invalid_slice = HANTRO_FALSE;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  RV_API_TRC("\nRv_dec_decode#");

  if(input == NULL || output == NULL || dec_inst == NULL) {
    RV_API_TRC("RvDecDecode# PARAM_ERROR\n");
    return RVDEC_PARAM_ERROR;
  }

  dec_cont = ((RvDecContainer *) dec_inst);
  p_api_stor = &dec_cont->ApiStorage;
  p_strm_desc = &dec_cont->StrmDesc;

  /*
   *  Check if decoder is in an incorrect mode
   */
  if(p_api_stor->DecStat == UNINIT) {

    RV_API_TRC("RvDecDecode: NOT_INITIALIZED\n");
    return RVDEC_NOT_INITIALIZED;
  }

  if(dec_cont->abort) {
    return (RVDEC_ABORTED);
  }

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      input->stream == NULL || input->stream_bus_address == 0) {
    RV_API_TRC("RvDecDecode# PARAM_ERROR\n");
    return RVDEC_PARAM_ERROR;
  }

  /* If we have set up for delayed resolution change, do it here */
  if(dec_cont->StrmStorage.rpr_detected) {
    u32 new_width, new_height = 0;
    u32 num_pics_resampled = 0;
    u32 resample_pics[2] = {0};
    struct DWLLinearMem tmp_data;
#ifndef USE_OMXIL_BUFFER
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
    if (dec_cont->pp_enabled) {
      InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
    }
#endif

    dec_cont->StrmStorage.rpr_detected = 0;

    num_pics_resampled = 0;
    if( dec_cont->StrmStorage.rpr_next_pic_type == RV_P_PIC) {
      resample_pics[0] = dec_cont->StrmStorage.work0;
      num_pics_resampled = 1;
    } else if ( dec_cont->StrmStorage.rpr_next_pic_type == RV_B_PIC ) {
      /* B picture resampling not supported (would affect picture output
       * order and co-located MB data). So let's skip B frames until
       * next reference picture. */
      dec_cont->StrmStorage.skip_b = 1;
    }

    /* Allocate extra picture buffer for resampling */
    if( num_pics_resampled ) {
      internal_ret = rvAllocateRprBuffer( dec_cont );
      if( internal_ret != RVDEC_OK ) {
        RVDEC_DEBUG(("ALLOC RPR BUFFER FAIL\n"));
        RV_API_TRC("RvDecDecode# MEMFAIL\n");
        return (internal_ret);
      }
    }

    new_width = dec_cont->tmp_hdrs.horizontal_size;
    new_height = dec_cont->tmp_hdrs.vertical_size;

    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    for(i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;

      /* if user does not set crop but set scale, should modify
       * crop.width and crop.height when dynamic resolution*/
      if(!ppu_cfg->crop.set_by_user) {
        ppu_cfg->crop.width = new_width;
        ppu_cfg->crop.height = new_height;
        if(ppu_cfg->scale.set_by_user && ppu_cfg->scale.ratio_x) {
          /* behavior consistent with other formats in fixed scale mode,
           * chroma_offset also need reset when resolution change.*/
          ppu_cfg->scale.width = ppu_cfg->crop.width / ppu_cfg->scale.ratio_x;
          ppu_cfg->scale.height = ppu_cfg->crop.height / ppu_cfg->scale.ratio_y;
        } else if (ppu_cfg->scale.set_by_user && !ppu_cfg->scale.ratio_x) {
          /* not reset when flexible scale mode */
        } else {
          ppu_cfg->scale.width = new_width;
          ppu_cfg->scale.height = new_height;
        }
      }
    }
    if (CheckPpUnitConfig(&hw_feature, new_width, new_height, 0, dec_cont->ppu_cfg))
      return RVDEC_PARAM_ERROR;
    CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0);

    /* Resample ref picture(s). Should be safe to do at this point; all
     * original size pictures are output before this point. */
    for( i = 0 ; i < num_pics_resampled ; ++i ) {
      u32 j = resample_pics[i];
      picture_t * p_ref_pic;

      p_ref_pic = &dec_cont->StrmStorage.p_pic_buf[j];

      if( p_ref_pic->coded_width == new_width &&
          p_ref_pic->coded_height == new_height )
        continue;
#if 0
      PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
      for(i = 0; i < 4; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;

        /* if user does not set crop but set scale, should modify
         * crop.width and crop.height when dynamic resolution*/
        if(!ppu_cfg->crop.set_by_user) {
          ppu_cfg->crop.width = new_width;
          ppu_cfg->crop.height = new_height;
          if(ppu_cfg->scale.set_by_user && ppu_cfg->scale.ratio_x) {
            /* behavior consistent with other formats in fixed scale mode,
             * chroma_offset also need reset when resolution change.*/
            ppu_cfg->scale.width = new_width / ppu_cfg->scale.ratio_x;
            ppu_cfg->scale.height = new_height / ppu_cfg->scale.ratio_y;
            if(ppu_cfg->tiled_e)
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height / 4;
            else
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height;
          } else if (ppu_cfg->scale.set_by_user && !ppu_cfg->scale.ratio_x) {
            /* not reset when flexible scale mode */
          } else {
            ppu_cfg->scale.width = new_width;
            ppu_cfg->scale.height = new_height;
            if(ppu_cfg->tiled_e)
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height / 4;
            else
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height;
          }
        }
      }
#endif
      align = 1 << dec_cont->align;
      rvRpr( p_ref_pic,
             &dec_cont->StrmStorage.p_rpr_buf,
             &dec_cont->StrmStorage.rpr_work_buffer,
             0 /*round*/,
             new_width,
             new_height,
             dec_cont->tiled_reference_enable, align);

      p_ref_pic->coded_width = new_width;
      p_ref_pic->frame_width = ( 15 + new_width ) & ~15;
      p_ref_pic->coded_height = new_height;
      p_ref_pic->frame_height = ( 15 + new_height ) & ~15;

      tmp_data = dec_cont->StrmStorage.p_rpr_buf.data;
      dec_cont->StrmStorage.p_rpr_buf.data = p_ref_pic->data;
      p_ref_pic->data = tmp_data;
    }

    dec_cont->Hdrs.horizontal_size = new_width;
    dec_cont->Hdrs.vertical_size = new_height;

    if (!hw_feature.pic_size_reg_unified) {
      SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_WIDTH,
                     dec_cont->FrameDesc.frame_width);
      SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_HEIGHT_P,
                     dec_cont->FrameDesc.frame_height);
      SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_H_EXT,
                     dec_cont->FrameDesc.frame_height >> 8);
    } else {
      SetDecRegister(dec_cont->rv_regs, HWIF_PIC_WIDTH_IN_CBS,
                     dec_cont->FrameDesc.frame_width << 1);
      SetDecRegister(dec_cont->rv_regs, HWIF_PIC_HEIGHT_IN_CBS,
                     dec_cont->FrameDesc.frame_height << 1);
    }

    if(dec_cont->ref_buf_support) {
      RefbuInit(&dec_cont->ref_buffer_ctrl,
                RV_DEC_X170_MODE,
                dec_cont->FrameDesc.frame_width,
                dec_cont->FrameDesc.frame_height,
                dec_cont->ref_buf_support);
    }

    dec_cont->StrmStorage.strm_dec_ready = HANTRO_TRUE;
  }

  if (p_api_stor->DecStat == HEADERSDECODED) {
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    RvCheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = RVDEC_WAITING_FOR_BUFFER;
        }
        rvFreeBuffers(dec_cont);

        if( dec_cont->StrmStorage.max_frame_width == 0 ) {
          dec_cont->StrmStorage.max_frame_width =
            dec_cont->Hdrs.horizontal_size;
          dec_cont->StrmStorage.max_frame_height =
            dec_cont->Hdrs.vertical_size;
          dec_cont->StrmStorage.max_mbs_per_frame =
            ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
            ((dec_cont->StrmStorage.max_frame_height+15)>>4);
        }

        if(!dec_cont->StrmStorage.direct_mvs.virtual_address)
        {
          RVDEC_DEBUG(("Allocate buffers\n"));
          internal_ret = rvAllocateBuffers(dec_cont);
          if(internal_ret != RVDEC_OK) {
            RVDEC_DEBUG(("ALLOC BUFFER FAIL\n"));
            RV_API_TRC("RvDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = RVDEC_WAITING_FOR_BUFFER;
        }
      }

      if (dec_cont->realloc_int_buf) {

        rvFreeBuffers(dec_cont);

        if( dec_cont->StrmStorage.max_frame_width == 0 ) {
          dec_cont->StrmStorage.max_frame_width =
            dec_cont->Hdrs.horizontal_size;
          dec_cont->StrmStorage.max_frame_height =
            dec_cont->Hdrs.vertical_size;
          dec_cont->StrmStorage.max_mbs_per_frame =
            ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
            ((dec_cont->StrmStorage.max_frame_height+15)>>4);
        }

        if(!dec_cont->StrmStorage.direct_mvs.virtual_address)
        {
          RVDEC_DEBUG(("Allocate buffers\n"));
          internal_ret = rvAllocateBuffers(dec_cont);
          if(internal_ret != RVDEC_OK) {
            RVDEC_DEBUG(("ALLOC BUFFER FAIL\n"));
            RV_API_TRC("RvDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    }
  }

  /*
   *  Update stream structure
   */
  p_strm_desc->p_strm_buff_start = input->stream;
  p_strm_desc->strm_curr_pos = input->stream;
  p_strm_desc->bit_pos_in_word = 0;
  p_strm_desc->strm_buff_size = input->data_len;
  p_strm_desc->strm_buff_read_bits = 0;

  dec_cont->StrmStorage.num_slices = input->slice_info_num+1;
  /* Limit maximum n:o of slices
   * (TODO, should we report an error?) */
  if(dec_cont->StrmStorage.num_slices > RV_DEC_X170_MAX_NUM_SLICES)
    dec_cont->StrmStorage.num_slices = RV_DEC_X170_MAX_NUM_SLICES;
  slice_info = dec_cont->StrmStorage.slices.virtual_address;

#ifdef RV_RAW_STREAM_SUPPORT
  dec_cont->StrmStorage.raw_mode = input->slice_info_num == 0;
#endif

  /* convert slice offsets into slice sizes, TODO: check if memory given by application is writable */
  if (p_api_stor->DecStat == STREAMDECODING
#ifdef RV_RAW_STREAM_SUPPORT
      && !dec_cont->StrmStorage.raw_mode
#endif
     )
    /* Copy offsets to HW external memory */
    for( i = 0 ; i < input->slice_info_num ; ++i ) {
      i32 tmp;
      if( i == input->slice_info_num-1 )
        tmp = input->data_len;
      else
        tmp = input->slice_info[i+1].offset;
      slice_info[i] = tmp - input->slice_info[i].offset;
      if(!input->slice_info[i].is_valid) {
        contains_invalid_slice = HANTRO_TRUE;
      }
    }

#ifdef _DEC_PP_USAGE
  dec_cont->StrmStorage.latest_id = input->pic_id;
#endif

  if(contains_invalid_slice) {
    /* If stream contains even one invalid slice, we freeze the whole
     * picture. At this point we could try to do some advanced
     * error concealment stuff */
    RVDEC_API_DEBUG(("STREAM ERROR; LEAST ONE SLICE BROKEN\n"));
    RVFLUSH;
    dec_cont->FrameDesc.pic_coding_type = RV_P_PIC;
    ret = rvHandleVlcModeError(dec_cont, input->pic_id);
    error_concealment = HANTRO_TRUE;
    RVDEC_UPDATE_POUTPUT;
    ret = RVDEC_PIC_DECODED;
  } else { /* All slices OK */
    /* TODO: do we need loop? (maybe if many slices?) */
    do {
      RVDEC_API_DEBUG(("Start Decode\n"));
      /* run SW if HW is not in the middle of processing a picture
       * (indicated as HW_PIC_STARTED decoder status) */
      if(p_api_stor->DecStat == HEADERSDECODED) {
        p_api_stor->DecStat = STREAMDECODING;
        if (dec_cont->realloc_ext_buf) {
          RvSetExternalBufferInfo(dec_cont);
          dec_cont->buffer_index = 0;
          ret = RVDEC_WAITING_FOR_BUFFER;
        }
      } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
        ret =  RVDEC_WAITING_FOR_BUFFER;
      } else if(p_api_stor->DecStat != HW_PIC_STARTED) {
        strm_dec_result = rv_StrmDecode(dec_cont);
        switch (strm_dec_result) {
        case DEC_PIC_HDR_RDY:
          /* if type inter predicted and no reference -> error */
          if((dec_cont->FrameDesc.pic_coding_type == RV_P_PIC &&
              dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE) ||
              (dec_cont->FrameDesc.pic_coding_type == RV_B_PIC &&
               (dec_cont->StrmStorage.work1 == INVALID_ANCHOR_PICTURE ||
                dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE ||
                dec_cont->StrmStorage.skip_b ||
                input->skip_non_reference)) ||
              (dec_cont->FrameDesc.pic_coding_type == RV_P_PIC &&
               dec_cont->StrmStorage.picture_broken &&
               dec_cont->StrmStorage.intra_freeze)) {
            if(dec_cont->StrmStorage.skip_b ||
                input->skip_non_reference ) {
              RV_API_TRC("RvDecDecode# RVDEC_NONREF_PIC_SKIPPED\n");
            }
            ret = rvHandleVlcModeError(dec_cont, input->pic_id);
            error_concealment = HANTRO_TRUE;
          } else {
            p_api_stor->DecStat = HW_PIC_STARTED;
          }
          break;

        case DEC_PIC_HDR_RDY_ERROR:
          ret = rvHandleVlcModeError(dec_cont, input->pic_id);
          error_concealment = HANTRO_TRUE;
          /* copy output parameters for this PIC */
          RVDEC_UPDATE_POUTPUT;
          break;

        case DEC_PIC_HDR_RDY_RPR:
          dec_cont->StrmStorage.strm_dec_ready = FALSE;
          p_api_stor->DecStat = STREAMDECODING;

          if(dec_cont->ref_buf_support) {
            RefbuInit(&dec_cont->ref_buffer_ctrl,
                      RV_DEC_X170_MODE,
                      dec_cont->FrameDesc.frame_width,
                      dec_cont->FrameDesc.frame_height,
                      dec_cont->ref_buf_support);
          }
          ret = RVDEC_STRM_PROCESSED;
          break;

        case DEC_HDRS_RDY:
          internal_ret = rvDecCheckSupport(dec_cont);
          if(internal_ret != RVDEC_OK) {
            dec_cont->StrmStorage.strm_dec_ready = FALSE;
            p_api_stor->DecStat = INITIALIZED;
            return internal_ret;
          }

          dec_cont->ApiStorage.first_headers = 0;

          if (!hw_feature.pic_size_reg_unified) {
            SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_WIDTH,
                           dec_cont->FrameDesc.frame_width);
            SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_HEIGHT_P,
                           dec_cont->FrameDesc.frame_height);
            SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_H_EXT,
                           dec_cont->FrameDesc.frame_height >> 8);
          } else {
            SetDecRegister(dec_cont->rv_regs, HWIF_PIC_WIDTH_IN_CBS,
                           dec_cont->FrameDesc.frame_width << 1);
            SetDecRegister(dec_cont->rv_regs, HWIF_PIC_HEIGHT_IN_CBS,
                           dec_cont->FrameDesc.frame_height << 1);
          }

          if(dec_cont->ref_buf_support) {
            RefbuInit(&dec_cont->ref_buffer_ctrl,
                      RV_DEC_X170_MODE,
                      dec_cont->FrameDesc.frame_width,
                      dec_cont->FrameDesc.frame_height,
                      dec_cont->ref_buf_support);
          }

          p_api_stor->DecStat = HEADERSDECODED;
          if (dec_cont->pp_enabled) {
            dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
            dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
          }
          FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
          BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
          RVDEC_API_DEBUG(("HDRS_RDY\n"));
          ret = RVDEC_HDRS_RDY;

          output->data_left = input->data_len;
          if(dec_cont->StrmStorage.rpr_detected)
          ret = RVDEC_STRM_PROCESSED;
          return ret;

        default:
          output->data_left = 0;
          //ASSERT(strm_dec_result == DEC_END_OF_STREAM);
          if(dec_cont->StrmStorage.rpr_detected) {
            ret = RVDEC_PIC_DECODED;
          } else {
            ret = RVDEC_STRM_PROCESSED;
          }
          return ret;
        }
      }

      /* picture header properly decoded etc -> start HW */
      if(p_api_stor->DecStat == HW_PIC_STARTED) {
        if (!dec_cont->asic_running) {
          dec_cont->StrmStorage.work_out =
            BqueueNext2( &dec_cont->StrmStorage.bq,
                         dec_cont->StrmStorage.work0,
                         dec_cont->StrmStorage.work1,
                         BQUEUE_UNUSED,
                         dec_cont->FrameDesc.pic_coding_type == RV_B_PIC );
          if(dec_cont->StrmStorage.work_out == (u32)0xFFFFFFFFU) {
            if (dec_cont->abort)
              return RVDEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
            else {
              output->strm_curr_pos = input->stream;
              output->strm_curr_bus_address = input->stream_bus_address;
              output->data_left = input->data_len;
              p_api_stor->DecStat = STREAMDECODING;
              dec_cont->same_slice_header = 1;
              return RVDEC_NO_DECODING_BUFFER;
            }
#endif
          }
          else if (dec_cont->same_slice_header)
            dec_cont->same_slice_header = 0;

          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 1;

          if (dec_cont->pp_enabled) {
            dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data =
              InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
            if (dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data == NULL)
              return RVDEC_ABORTED;
          }

          if (dec_cont->StrmStorage.partial_freeze) {
            PreparePartialFreeze((u8*)dec_cont->StrmStorage.p_pic_buf[
                                   dec_cont->StrmStorage.work_out].data.virtual_address,
                                 dec_cont->FrameDesc.frame_width,
                                 dec_cont->FrameDesc.frame_height);
          }
        }
        asic_status = RunDecoderAsic(dec_cont, input->stream_bus_address);

        if(asic_status == ID8170_DEC_TIMEOUT) {
          ret = RVDEC_HW_TIMEOUT;
        } else if(asic_status == ID8170_DEC_SYSTEM_ERROR) {
          ret = RVDEC_SYSTEM_ERROR;
        } else if(asic_status == ID8170_DEC_HW_RESERVED) {
          ret = RVDEC_HW_RESERVED;
        } else if(asic_status & RV_DEC_X170_IRQ_BUS_ERROR) {
          ret = RVDEC_HW_BUS_ERROR;
        } else if( (asic_status & RV_DEC_X170_IRQ_STREAM_ERROR) ||
                   (asic_status & RV_DEC_X170_IRQ_TIMEOUT) ||
                   (asic_status & RV_DEC_X170_IRQ_ABORT)) {
          if (!dec_cont->StrmStorage.partial_freeze ||
              !ProcessPartialFreeze(
                (u8*)dec_cont->StrmStorage.p_pic_buf[
                  dec_cont->StrmStorage.work_out].data.virtual_address,
                dec_cont->StrmStorage.work0 != INVALID_ANCHOR_PICTURE ?
                (u8*)dec_cont->StrmStorage.p_pic_buf[
                  dec_cont->StrmStorage.work0].data.virtual_address :
                NULL,
                dec_cont->FrameDesc.frame_width,
                dec_cont->FrameDesc.frame_height,
                dec_cont->StrmStorage.partial_freeze == 1)) {
            if(asic_status & RV_DEC_X170_IRQ_TIMEOUT ||
                asic_status & RV_DEC_X170_IRQ_ABORT) {
              RVDEC_API_DEBUG(("IRQ TIMEOUT IN HW\n"));
            } else {
              RVDEC_API_DEBUG(("STREAM ERROR IN HW\n"));
              RVFLUSH;
            }
            if (dec_cont->pp_enabled) {
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue,
                  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data->virtual_address);
            }
            ret = rvHandleVlcModeError(dec_cont, input->pic_id);
            error_concealment = HANTRO_TRUE;
            RVDEC_UPDATE_POUTPUT;
          } else {
            asic_status &= ~RV_DEC_X170_IRQ_STREAM_ERROR;
            asic_status &= ~RV_DEC_X170_IRQ_TIMEOUT;
            asic_status &= ~RV_DEC_X170_IRQ_ABORT;
            asic_status |= RV_DEC_X170_IRQ_DEC_RDY;
            error_concealment = HANTRO_FALSE;
          }
        } else if(asic_status & RV_DEC_X170_IRQ_BUFFER_EMPTY) {
          rvDecPreparePicReturn(dec_cont);
          ret = RVDEC_BUF_EMPTY;
        } else if(asic_status & RV_DEC_X170_IRQ_DEC_RDY) {
        } else {
          ASSERT(0);
        }

        /* HW finished decoding a picture */
        if(asic_status & RV_DEC_X170_IRQ_DEC_RDY) {
          dec_cont->FrameDesc.frame_number++;

          rvHandleFrameEnd(dec_cont);

          rvDecBufferPicture(dec_cont,
                             input->pic_id,
                             dec_cont->FrameDesc.
                             pic_coding_type == RV_B_PIC,
                             dec_cont->FrameDesc.
                             pic_coding_type == RV_P_PIC,
                             RVDEC_PIC_DECODED, 0);

          ret = RVDEC_PIC_DECODED;

          if(dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
            dec_cont->StrmStorage.work1 =
              dec_cont->StrmStorage.work0;
            dec_cont->StrmStorage.work0 =
              dec_cont->StrmStorage.work_out;
            if(dec_cont->StrmStorage.skip_b)
              dec_cont->StrmStorage.skip_b--;
          }

          if(dec_cont->FrameDesc.pic_coding_type == RV_I_PIC)
            dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;

          rvDecPreparePicReturn(dec_cont);
        }

        if(ret != RVDEC_STRM_PROCESSED && ret != RVDEC_BUF_EMPTY) {
          p_api_stor->DecStat = STREAMDECODING;
        }

        if(ret == RVDEC_PIC_DECODED || ret == RVDEC_STRM_PROCESSED || ret == RVDEC_BUF_EMPTY) {
          /* copy output parameters for this PIC (excluding stream pos) */
          dec_cont->MbSetDesc.out_data.strm_curr_pos =
            output->strm_curr_pos;
          RVDEC_UPDATE_POUTPUT;
        }
      }
    } while(ret == 0);
  }

  if(error_concealment && dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
    dec_cont->StrmStorage.picture_broken = HANTRO_TRUE;
  }

  RV_API_TRC("RvDecDecode: Exit\n");
  if(!dec_cont->StrmStorage.rpr_detected) {
    output->strm_curr_pos = dec_cont->StrmDesc.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);
    output->data_left = dec_cont->StrmDesc.strm_buff_size -
                        (output->strm_curr_pos - p_strm_desc->p_strm_buff_start);
  } else {
    output->data_left = input->data_len;
    ret = RVDEC_STRM_PROCESSED;
  }

  dec_cont->output_stat = RvDecNextPicture_INTERNAL(dec_cont, &dec_cont->out_pic, 0);
  if(dec_cont->output_stat == RVDEC_ABORTED)
    return (RVDEC_ABORTED);

  if(dec_cont->abort)
    return(RVDEC_ABORTED);
  else
    return ((RvDecRet) ret);
}

/*------------------------------------------------------------------------------

    Function: RvDecRelease()

        Functional description:
            Release the decoder instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void RvDecRelease(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = NULL;

  RVDEC_DEBUG(("1\n"));
  RV_API_TRC("RvDecRelease#");
  if(dec_inst == NULL) {
    RV_API_TRC("RvDecRelease# ERROR: dec_inst == NULL");
    return;
  }

  dec_cont = ((RvDecContainer *) dec_inst);

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if (dec_cont->asic_running)
    (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);

  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  rvFreeBuffers(dec_cont);

  if (dec_cont->StrmStorage.slices.virtual_address != NULL)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.slices);

  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  DWLfree(dec_cont);

  RV_API_TRC("RvDecRelease: OK");
}


/*------------------------------------------------------------------------------
    Function name   : rvRefreshRegs
    Description     :
    Return type     : void
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvRefreshRegs(RvDecContainer * dec_cont) {
  i32 i;
  u32 *pp_regs = dec_cont->rv_regs;

  for(i = 0; i < DEC_X170_REGISTERS; i++) {
    pp_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
  }
}

/*------------------------------------------------------------------------------
    Function name   : rvFlushRegs
    Description     :
    Return type     : void
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvFlushRegs(RvDecContainer * dec_cont) {
  i32 i;
  u32 *pp_regs = dec_cont->rv_regs;

  for(i = 2; i < DEC_X170_REGISTERS; i++) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, pp_regs[i]);
    pp_regs[i] = 0;
  }
#if 0
#ifdef USE_64BIT_ENV
  offset = TOTAL_X170_ORIGIN_REGS * 0x04;
  pp_regs =  dec_cont->rv_regs + TOTAL_X170_ORIGIN_REGS;
  for(i = DEC_X170_EXPAND_REGS; i > 0; --i) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
    pp_regs++;
    offset += 4;
  }
#endif
  offset = 314 * 0x04;
  pp_regs =  dec_cont->rv_regs + 314;
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
  offset = PP_START_UNIFIED_REGS * 0x04;
  offset = PP_START_UNIFIED_REGS * 0x04;
  pp_regs =  dec_cont->rv_regs + PP_START_UNIFIED_REGS;
  for(i = PP_UNIFIED_REGS; i > 0; --i) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
    pp_regs++;
    offset += 4;
  }
#endif
}

/*------------------------------------------------------------------------------
    Function name   : rvHandleVlcModeError
    Description     :
    Return type     : u32
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
u32 rvHandleVlcModeError(RvDecContainer * dec_cont, u32 pic_num) {
  u32 ret = RVDEC_STRM_PROCESSED;
  ASSERT(dec_cont->StrmStorage.strm_dec_ready);

  /*
  tmp = rvStrmDec_NextStartCode(dec_cont);
  if(tmp != END_OF_STREAM)
  {
      dec_cont->StrmDesc.strm_curr_pos -= 4;
      dec_cont->StrmDesc.strm_buff_read_bits -= 32;
  }
  */

  /* error in first picture -> set reference to grey */
  if(!dec_cont->FrameDesc.frame_number) {
    if (!dec_cont->tiled_stride_enable) {
      (void) DWLmemset(dec_cont->StrmStorage.
                       p_pic_buf[dec_cont->StrmStorage.work_out].data.
                       virtual_address, 128,
                       384 * dec_cont->FrameDesc.total_mb_in_frame);
    } else {
      u32 out_w, out_h, size;
      out_w = NEXT_MULTIPLE(4 * dec_cont->FrameDesc.frame_width * 16, ALIGN(dec_cont->align));
      out_h = dec_cont->FrameDesc.frame_height * 4;
      size = out_w * out_h * 3 / 2;
      (void) DWLmemset(dec_cont->StrmStorage.
                       p_pic_buf[dec_cont->StrmStorage.work_out].data.
                       virtual_address, 128, size);
    }

    rvDecPreparePicReturn(dec_cont);

    /* no pictures finished -> return STRM_PROCESSED */
    ret = RVDEC_STRM_PROCESSED;
    dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
    dec_cont->StrmStorage.skip_b = 2;
  } else {
    if(dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
      dec_cont->FrameDesc.frame_number++;

      BqueueDiscard(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.work_out );
      dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work0;

      rvDecBufferPicture(dec_cont,
                         pic_num,
                         dec_cont->FrameDesc.pic_coding_type == RV_B_PIC,
                         1, (RvDecRet) FREEZED_PIC_RDY,
                         dec_cont->FrameDesc.total_mb_in_frame);

      ret = RVDEC_PIC_DECODED;

      dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
      dec_cont->StrmStorage.skip_b = 2;
    } else {
      if(dec_cont->StrmStorage.intra_freeze) {
        dec_cont->FrameDesc.frame_number++;
        rvDecBufferPicture(dec_cont,
                           pic_num,
                           dec_cont->FrameDesc.pic_coding_type ==
                           RV_B_PIC, 1, (RvDecRet) FREEZED_PIC_RDY,
                           dec_cont->FrameDesc.total_mb_in_frame);

        ret = RVDEC_PIC_DECODED;

      } else {
        ret = RVDEC_NONREF_PIC_SKIPPED;
      }
    }
  }

  dec_cont->ApiStorage.DecStat = STREAMDECODING;

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : rvHandleFrameEnd
    Description     :
    Return type     : u32
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvHandleFrameEnd(RvDecContainer * dec_cont) {

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos -
         dec_cont->StrmDesc.p_strm_buff_start);
  dec_cont->StrmDesc.bit_pos_in_word = 0;

}

/*------------------------------------------------------------------------------

         Function name: RunDecoderAsic

         Purpose:       Set Asic run lenght and run Asic

         Input:         RvDecContainer *dec_cont

         Output:        void

------------------------------------------------------------------------------*/
u32 RunDecoderAsic(RvDecContainer * dec_cont, addr_t strm_bus_address) {
  i32 ret;
  addr_t tmp = 0;
  u32 asic_status = 0;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  addr_t mask;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  ASSERT(dec_cont->StrmStorage.
         p_pic_buf[dec_cont->StrmStorage.work_out].data.bus_address != 0);
  ASSERT(strm_bus_address != 0);

  if (hw_feature.g1_strm_128bit_align)
    mask = 15;
  else
    mask = 7;

  if(!dec_cont->asic_running) {
    tmp = RvSetRegs(dec_cont, strm_bus_address);
    if(tmp == HANTRO_NOK)
      return 0;

    (void) DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_RV_DEC);

    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_DIS, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_FILTERING_DIS, 1);

    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);

    rvFlushRegs(dec_cont);

    DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, NULL, 0);
    /* Enable HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 1);
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                dec_cont->rv_regs[1]);
  } else { /* in the middle of decoding, continue decoding */
    /* tmp is strm_bus_address + number of bytes decoded by SW */
    /* TODO: alotetaanko aina bufferin alusta? */
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
    tmp = strm_bus_address + tmp;

    /* pointer to start of the stream, mask to get the pointer to
     * previous 64/128-bit aligned position */
    if(!(tmp & ~(mask))) {
      return 0;
    }

    SET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));
    /* amount of stream (as seen by the HW), obtained as amount of stream
     * given by the application subtracted by number of bytes decoded by
     * SW (if strm_bus_address is not 64/128-bit aligned -> adds number of bytes
     * from previous 64/128-bit aligned boundary) */
    SetDecRegister(dec_cont->rv_regs, HWIF_STREAM_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));

    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_BUFFER_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_OFFSET, 0);

    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

    /* This depends on actual register allocation */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->rv_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->rv_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->rv_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->rv_regs[259]);
    if (IS_LEGACY(dec_cont->rv_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->rv_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->rv_regs[169]);
    if (sizeof(addr_t) == 8) {
      if(hw_feature.addr64_support) {
        if (IS_LEGACY(dec_cont->rv_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->rv_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->rv_regs[168]);
      } else {
        ASSERT(dec_cont->rv_regs[122] == 0);
        ASSERT(dec_cont->rv_regs[168] == 0);
      }
    } else {
      if(hw_feature.addr64_support) {
        if (IS_LEGACY(dec_cont->rv_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                dec_cont->rv_regs[1]);
  }

  /* Wait for HW ready */
  RVDEC_API_DEBUG(("Wait for Decoder\n"));
  ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                       (u32) DEC_X170_TIMEOUT_LENGTH);

  rvRefreshRegs(dec_cont);

  if(ret == DWL_HW_WAIT_OK) {
    asic_status =
      GetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT);
  } else if(ret == DWL_HW_WAIT_TIMEOUT) {
    asic_status = ID8170_DEC_TIMEOUT;
  } else {
    asic_status = ID8170_DEC_SYSTEM_ERROR;
  }

  if(!(asic_status & RV_DEC_X170_IRQ_BUFFER_EMPTY) ||
      (asic_status & RV_DEC_X170_IRQ_STREAM_ERROR) ||
      (asic_status & RV_DEC_X170_IRQ_BUS_ERROR) ||
      (asic_status == ID8170_DEC_TIMEOUT) ||
      (asic_status == ID8170_DEC_SYSTEM_ERROR)) {
    /* reset HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 0);

    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->rv_regs[1]);

    dec_cont->asic_running = 0;
    (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }

  /* if HW interrupt indicated either BUFFER_EMPTY or
   * DEC_RDY -> read stream end pointer and update StrmDesc structure */
  if((asic_status &
      (RV_DEC_X170_IRQ_BUFFER_EMPTY | RV_DEC_X170_IRQ_DEC_RDY))) {
    tmp = GET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE);

    if((tmp - strm_bus_address) <= dec_cont->max_strm_len) {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start + (tmp - strm_bus_address);
    } else {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start +
        dec_cont->StrmDesc.strm_buff_size;
    }

    dec_cont->StrmDesc.strm_buff_read_bits =
      8 * (dec_cont->StrmDesc.strm_curr_pos -
           dec_cont->StrmDesc.p_strm_buff_start);
    dec_cont->StrmDesc.bit_pos_in_word = 0;
  }

  if( dec_cont->FrameDesc.pic_coding_type != RV_B_PIC &&
      dec_cont->ref_buf_support &&
      (asic_status & RV_DEC_X170_IRQ_DEC_RDY) &&
      dec_cont->asic_running == 0) {
    RefbuMvStatistics( &dec_cont->ref_buffer_ctrl,
                       dec_cont->rv_regs,
                       dec_cont->StrmStorage.direct_mvs.virtual_address,
                       dec_cont->FrameDesc.pic_coding_type == RV_P_PIC,
                       dec_cont->FrameDesc.pic_coding_type == RV_I_PIC );
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);

  return asic_status;

}

/*------------------------------------------------------------------------------

    Function name: RvDecNextPicture

    Functional description:
        Retrieve next decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        RVDEC_OK         No picture available.
        RVDEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
RvDecRet RvDecNextPicture(RvDecInst dec_inst,
                          RvDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  RvDecRet return_value = RVDEC_PIC_RDY;
  RvDecContainer *dec_cont;
  i32 ret;

  /* Code */
  RV_API_TRC("\nRv_dec_next_picture#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    RV_API_TRC("RvDecNextPicture# ERROR: picture is NULL");
    return (RVDEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecNextPicture# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  addr_t i;
  if ((ret = FifoPop(dec_cont->fifo_display, (FifoObject *)&i,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
          FIFO_EXCEPTION_ENABLE
#else
          FIFO_EXCEPTION_DISABLE
#endif
          )) != FIFO_ABORT) {

#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
    if (ret == FIFO_EMPTY) return RVDEC_OK;
#endif

    if ((i32)i == -1) {
      RV_API_TRC("RvDecNextPicture# RVDEC_END_OF_STREAM\n");
      return RVDEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      RV_API_TRC("RvDecNextPicture# RVDEC_FLUSHED\n");
      return RVDEC_FLUSHED;
    }

    *picture = dec_cont->StrmStorage.picture_info[i];

    RV_API_TRC("RvDecNextPicture# RVDEC_PIC_RDY\n");
    return (RVDEC_PIC_RDY);
  } else
    return RVDEC_ABORTED;

  return return_value;
}

/*------------------------------------------------------------------------------

    Function name: RvDecNextPicture_INTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        RVDEC_OK                No picture available.
        RVDEC_PIC_RDY           Picture ready.
        rvDEC_PARAM_ERROR       invalid parameters.
        RVDEC_NOT_INITIALIZED   decoder instance not initialized yet.

------------------------------------------------------------------------------*/
RvDecRet RvDecNextPicture_INTERNAL(RvDecInst dec_inst,
                                   RvDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  RvDecRet return_value = RVDEC_PIC_RDY;
  RvDecContainer *dec_cont;
  u32 pic_index = RV_BUFFER_UNDEFINED;
  u32 min_count;
  static u32 pic_count = 0;

  /* Code */
  RV_API_TRC("\nRv_dec_next_picture_INTERNAL#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    RV_API_TRC("RvDecNextPicture_INTERNAL# ERROR: picture is NULL");
    return (RVDEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecNextPicture_INTERNAL# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  min_count = 0;
  (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
  if(!end_of_stream && !dec_cont->StrmStorage.rpr_detected)
    min_count = 1;

  /* this is to prevent post-processing of non-finished pictures in the
   * end of the stream */
  if(end_of_stream && dec_cont->FrameDesc.pic_coding_type == RV_B_PIC) {
    dec_cont->FrameDesc.pic_coding_type = RV_P_PIC;
  }

  /* Nothing to send out */
  if(dec_cont->StrmStorage.out_count <= min_count) {
    (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
    picture->pictures[0].output_picture = NULL;
    return_value = RVDEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.out_index;
    pic_index = dec_cont->StrmStorage.out_buf[pic_index];

    RvFillPicStruct(picture, dec_cont, pic_index);

    pic_count++;

    dec_cont->StrmStorage.out_count--;
    dec_cont->StrmStorage.out_index++;
    dec_cont->StrmStorage.out_index &= 0xF;

#ifdef USE_PICTURE_DISCARD
    if (dec_cont->StrmStorage.p_pic_buf[pic_index].first_show)
#endif
    {
      /* wait this buffer as unused */
      if (BqueueWaitBufNotInUse(&dec_cont->StrmStorage.bq, pic_index) != HANTRO_OK)
        return RVDEC_ABORTED;

    if(dec_cont->pp_enabled) {
      InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data->virtual_address);
    }

      dec_cont->StrmStorage.p_pic_buf[pic_index].first_show = 0;

    /* set this buffer as used */
    BqueueSetBufferAsUsed(&dec_cont->StrmStorage.bq, pic_index);

      if(dec_cont->pp_enabled)
        InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data->virtual_address);

      dec_cont->StrmStorage.picture_info[dec_cont->fifo_index] = *picture;
      FifoPush(dec_cont->fifo_display, (FifoObject)(addr_t)dec_cont->fifo_index, FIFO_EXCEPTION_DISABLE);
      dec_cont->fifo_index++;
      if(dec_cont->fifo_index == 32)
        dec_cont->fifo_index = 0;
      if (dec_cont->pp_enabled) {
        BqueuePictureRelease(&dec_cont->StrmStorage.bq, pic_index);
      }
    }
  }

  return return_value;
}

/*------------------------------------------------------------------------------

    Function name: RvDecPictureConsumed

    Functional description:
        release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        RVDEC_PARAM_ERROR         Decoder instance or picture is null
        RVDEC_NOT_INITIALIZED     Decoder instance isn't initialized
        RVDEC_OK                          picture release success
------------------------------------------------------------------------------*/
RvDecRet RvDecPictureConsumed(RvDecInst dec_inst, RvDecPicture * picture) {
  /* Variables */
  RvDecContainer *dec_cont;
  u32 i;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */
  RV_API_TRC("\nRv_dec_picture_consumed#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    RV_API_TRC("RvDecPictureConsumed# ERROR: picture is NULL");
    return (RVDEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecPictureConsumed# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address
          == dec_cont->StrmStorage.p_pic_buf[i].data.bus_address) {
        BqueuePictureRelease(&dec_cont->StrmStorage.bq, i);
        return (RVDEC_OK);
      }
    }
  } else {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (const u32 *)picture->pictures[i].output_picture;
        break;
      }
    }
    InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
    return (RVDEC_OK);
  }
  return (RVDEC_PARAM_ERROR);
}


RvDecRet RvDecEndOfStream(RvDecInst dec_inst, u32 strm_end_flag) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  RV_API_TRC("RvDecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecEndOfStream# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }
  pthread_mutex_lock(&dec_cont->protect_mutex);
  if(dec_cont->dec_stat == RVDEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (RVDEC_OK);
  }

  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->rv_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  dec_cont->output_stat = RvDecNextPicture_INTERNAL(dec_cont, &dec_cont->out_pic, 1);
  if(dec_cont->output_stat == RVDEC_ABORTED) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (RVDEC_ABORTED);
  }

  if(strm_end_flag) {
    dec_cont->dec_stat = RVDEC_END_OF_STREAM;
    FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);
  }

  /* Wait all buffers as unused */
  if(!strm_end_flag)
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);

  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  RV_API_TRC("RvDecEndOfStream# RVDEC_OK\n");
  return RVDEC_OK;
}

/*----------------------=-------------------------------------------------------

    Function name: RvFillPicStruct

    Functional description:
        Fill data to output pic description

    Input:
        dec_cont    Decoder container
        picture    Pointer to return value struct

    Return values:
        void

------------------------------------------------------------------------------*/
void RvFillPicStruct(RvDecPicture * picture,
                     RvDecContainer * dec_cont, u32 pic_index) {
  picture_t *p_pic;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;
  p_pic = (picture_t *) dec_cont->StrmStorage.p_pic_buf;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = p_pic[pic_index].frame_width;
    picture->pictures[0].frame_height = p_pic[pic_index].frame_height;
    picture->pictures[0].coded_width = p_pic[pic_index].coded_width;
    picture->pictures[0].coded_height = p_pic[pic_index].coded_height;
    if (dec_cont->tiled_stride_enable)
      picture->pictures[0].pic_stride = NEXT_MULTIPLE(picture->pictures[0].frame_width * 4,
                                                      ALIGN(dec_cont->align));
    else
      picture->pictures[0].pic_stride = picture->pictures[0].frame_width * 4;

    picture->pictures[0].pic_stride_ch = picture->pictures[0].pic_stride;
    picture->pictures[0].output_picture = (u8 *) p_pic[pic_index].data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic[pic_index].data.bus_address;
    picture->pictures[0].output_format = p_pic[pic_index].tiled_mode ?
                           DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;
  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
      picture->pictures[i].frame_width = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
      picture->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
      picture->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
      picture->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
      picture->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
      picture->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
      picture->pictures[i].output_picture = (u8 *) ((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->luma_offset);
      picture->pictures[i].output_picture_bus_address = p_pic[pic_index].pp_data->bus_address + ppu_cfg->luma_offset;
      CheckOutputFormat(dec_cont->ppu_cfg, &picture->pictures[i].output_format, i);
    }
  }
  picture->key_picture = p_pic[pic_index].pic_type;
  picture->pic_id = p_pic[pic_index].pic_id;
  picture->decode_id = p_pic[pic_index].decode_id;
  picture->pic_coding_type = p_pic[pic_index].pic_code_type;
  picture->number_of_err_mbs = p_pic[pic_index].nbr_err_mbs;

}

/*------------------------------------------------------------------------------

    Function name: RvSetRegs

    Functional description:
        Set registers

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
u32 RvSetRegs(RvDecContainer * dec_cont, addr_t strm_bus_address) {
  addr_t tmp = 0;
  u32 tmp_fwd;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  struct DecHwFeatures hw_feature;
  addr_t mask;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

#ifdef _DEC_PP_USAGE
  RvDecPpUsagePrint(dec_cont, DECPP_UNSPECIFIED,
                    dec_cont->StrmStorage.work_out, 1,
                    dec_cont->StrmStorage.latest_id);
#endif

  RVDEC_API_DEBUG(("Decoding to index %d \n",
                   dec_cont->StrmStorage.work_out));

  if (hw_feature.g1_strm_128bit_align)
    mask = 15;
  else
    mask = 7;

  /* swReg3 */
  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_INTERLACE_E, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_FIELDMODE_E, 0);

  /*
  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_MB_HEIGHT_P,
                 dec_cont->FrameDesc.frame_height);
  */

  if(dec_cont->FrameDesc.pic_coding_type == RV_B_PIC)
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_B_E, 1);
  else
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_B_E, 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_INTER_E,
                 dec_cont->FrameDesc.pic_coding_type == RV_P_PIC ||
                 dec_cont->FrameDesc.pic_coding_type == RV_B_PIC ? 1 : 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_WRITE_MVS_E,
                 dec_cont->FrameDesc.pic_coding_type == RV_P_PIC);


  SetDecRegister(dec_cont->rv_regs, HWIF_INIT_QP,
                 dec_cont->FrameDesc.qp);

  SetDecRegister(dec_cont->rv_regs, HWIF_RV_FWD_SCALE,
                 dec_cont->StrmStorage.fwd_scale);
  SetDecRegister(dec_cont->rv_regs, HWIF_RV_BWD_SCALE,
                 dec_cont->StrmStorage.bwd_scale);

  /* swReg5 */

  /* tmp is strm_bus_address + number of bytes decoded by SW */
#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
  else
#endif
    tmp = 0;

  tmp = strm_bus_address + tmp;

  /* bus address must not be zero */
  if(!(tmp & ~(mask))) {
    return 0;
  }

  /* pointer to start of the stream, mask to get the pointer to
   * previous 64-bit aligned position */
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

  /* amount of stream (as seen by the HW), obtained as amount of
   * stream given by the application subtracted by number of bytes
   * decoded by SW (if strm_bus_address is not 64-bit aligned -> adds
   * number of bytes from previous 64-bit aligned boundary) */
  SetDecRegister(dec_cont->rv_regs, HWIF_STREAM_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));

  SetDecRegister(dec_cont->rv_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));
  SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_OFFSET, 0);

#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));
  else
#endif
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT, 0);

  /* swReg13 */
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_DEC_OUT_BASE,
               dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].
               data.bus_address);

  SetDecRegister(dec_cont->rv_regs, HWIF_PP_CR_FIRST, dec_cont->cr_first);
  SetDecRegister( dec_cont->rv_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled );
  if (dec_cont->pp_enabled &&
      hw_feature.pp_support && hw_feature.pp_version) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    addr_t ppu_out_bus_addr = dec_cont->StrmStorage.p_pic_buf[dec_cont->
                              StrmStorage.work_out].pp_data->bus_address;
    PPSetRegs(dec_cont->rv_regs, &hw_feature, ppu_cfg, ppu_out_bus_addr,
              0, 0);
#if 0
    u32 ppw, pph;
#define TOFIX(d, q) ((u32)( (d) * (u32)(1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))
    if (hw_feature.pp_version == FIXED_DS_PP)  {
        /* G1V8_1 only supports fixed ratio (1/2/4/8) */
      ppw = NEXT_MULTIPLE((dec_cont->FrameDesc.frame_width * 16 >> dec_cont->dscale_shift_x), ALIGN(dec_cont->align));
      pph = (dec_cont->FrameDesc.frame_height * 16 >> dec_cont->dscale_shift_y);
      if (dec_cont->dscale_shift_x == 0) {
        SetDecRegister(dec_cont->rv_regs, HWIF_HOR_SCALE_MODE_U, 0);
        SetDecRegister(dec_cont->rv_regs, HWIF_WSCALE_INVRA_U, 0);
      } else {
        /* down scale */
        SetDecRegister(dec_cont->rv_regs, HWIF_HOR_SCALE_MODE_U, 2);
        SetDecRegister(dec_cont->rv_regs, HWIF_WSCALE_INVRA_U,
                       1<<(16-dec_cont->dscale_shift_x));
      }

      if (dec_cont->dscale_shift_y == 0) {
        SetDecRegister(dec_cont->rv_regs, HWIF_VER_SCALE_MODE_U, 0);
        SetDecRegister(dec_cont->rv_regs, HWIF_HSCALE_INVRA_U, 0);
      } else {
        /* down scale */
        SetDecRegister(dec_cont->rv_regs, HWIF_VER_SCALE_MODE_U, 2);
        SetDecRegister(dec_cont->rv_regs, HWIF_HSCALE_INVRA_U,
                       1<<(16-dec_cont->dscale_shift_y));
      }
    } else {
      /* flexible scale ratio */
      u32 in_width = dec_cont->crop_width;
      u32 in_height = dec_cont->crop_height;
      u32 out_width = dec_cont->scaled_width;
      u32 out_height = dec_cont->scaled_height;

      ppw = NEXT_MULTIPLE(dec_cont->scaled_width,  ALIGN(dec_cont->align));
      pph = dec_cont->scaled_height;
      SetDecRegister(dec_cont->rv_regs, HWIF_CROP_STARTX_U,
                     dec_cont->crop_startx >> hw_feature.crop_step_rshift);
      SetDecRegister(dec_cont->rv_regs, HWIF_CROP_STARTY_U,
                     dec_cont->crop_starty >> hw_feature.crop_step_rshift);
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_IN_WIDTH_U,
                     dec_cont->crop_width >> hw_feature.crop_step_rshift);
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_IN_HEIGHT_U,
                     dec_cont->crop_height >> hw_feature.crop_step_rshift);
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_OUT_WIDTH_U,
                     dec_cont->scaled_width);
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_OUT_HEIGHT_U,
                     dec_cont->scaled_height);

      if(in_width < out_width) {
        /* upscale */
        u32 W, inv_w;

        SetDecRegister(dec_cont->rv_regs, HWIF_HOR_SCALE_MODE_U, 1);

        W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
        inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

        SetDecRegister(dec_cont->rv_regs, HWIF_SCALE_WRATIO_U, W);
        SetDecRegister(dec_cont->rv_regs, HWIF_WSCALE_INVRA_U, inv_w);
      } else if(in_width > out_width) {
        /* downscale */
        u32 hnorm;

        SetDecRegister(dec_cont->rv_regs, HWIF_HOR_SCALE_MODE_U, 2);

        hnorm = FDIVI(TOFIX(out_width, 16), in_width);
        SetDecRegister(dec_cont->rv_regs, HWIF_WSCALE_INVRA_U, hnorm);
      } else {
        SetDecRegister(dec_cont->rv_regs, HWIF_WSCALE_INVRA_U, 0);
        SetDecRegister(dec_cont->rv_regs, HWIF_HOR_SCALE_MODE_U, 0);
      }

      if(in_height < out_height) {
        /* upscale */
        u32 H, inv_h;

        SetDecRegister(dec_cont->rv_regs, HWIF_VER_SCALE_MODE_U, 1);

        H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

        SetDecRegister(dec_cont->rv_regs, HWIF_SCALE_HRATIO_U, H);

        inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

        SetDecRegister(dec_cont->rv_regs, HWIF_HSCALE_INVRA_U, inv_h);
      } else if(in_height > out_height) {
        /* downscale */
        u32 Cv;

        Cv = FDIVI(TOFIX(out_height, 16), in_height) + 1;

        SetDecRegister(dec_cont->rv_regs, HWIF_VER_SCALE_MODE_U, 2);

        SetDecRegister(dec_cont->rv_regs, HWIF_HSCALE_INVRA_U, Cv);
      } else {
        SetDecRegister(dec_cont->rv_regs, HWIF_HSCALE_INVRA_U, 0);
        SetDecRegister(dec_cont->rv_regs, HWIF_VER_SCALE_MODE_U, 0);
      }
    }
    if (hw_feature.pp_stride_support) {
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_OUT_Y_STRIDE, ppw);
      SetDecRegister(dec_cont->rv_regs, HWIF_PP_OUT_C_STRIDE, ppw);
    }
    SET_ADDR64_REG(dec_cont->rv_regs, HWIF_PP_OUT_LU_BASE_U,
                   dec_cont->StrmStorage.p_pic_buf[dec_cont->
                       StrmStorage.work_out].
                   pp_data->bus_address);
    SET_ADDR64_REG(dec_cont->rv_regs, HWIF_PP_OUT_CH_BASE_U,
                   dec_cont->StrmStorage.p_pic_buf[dec_cont->
                       StrmStorage.work_out].
                   pp_data->bus_address + ppw * pph);
#endif
    SetPpRegister(dec_cont->rv_regs, HWIF_PP_IN_FORMAT_U, 1);
  }
  if (hw_feature.dec_stride_support) {
    /* Stride registers only available since g1v8_2 */
    if (dec_cont->tiled_stride_enable) {
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_Y_STRIDE,
                     NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_C_STRIDE,
                     NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));
    } else {
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_Y_STRIDE,
                     dec_cont->FrameDesc.frame_width * 64);
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_C_STRIDE,
                     dec_cont->FrameDesc.frame_width * 64);
    }
  }

  if(dec_cont->FrameDesc.pic_coding_type == RV_B_PIC) { /* ? */
    /* past anchor set to future anchor if past is invalid (second
     * picture in sequence is B) */
    tmp_fwd =
      dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
      dec_cont->StrmStorage.work1 :
      dec_cont->StrmStorage.work0;

    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER0_BASE,
                 dec_cont->StrmStorage.p_pic_buf[tmp_fwd].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER1_BASE,
                 dec_cont->StrmStorage.p_pic_buf[tmp_fwd].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER2_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER3_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
  } else {
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER0_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER1_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_STARTMB_X, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_STARTMB_Y, 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_DIS, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_FILTERING_DIS, 1);

  SET_ADDR_REG(dec_cont->rv_regs, HWIF_DIR_MV_BASE,
               dec_cont->StrmStorage.direct_mvs.bus_address);
  SetDecRegister(dec_cont->rv_regs, HWIF_PREV_ANC_TYPE,
                 dec_cont->StrmStorage.p_pic_buf[
                   dec_cont->StrmStorage.work0].is_inter);

#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_SLICE_AM, 0);
  else
#endif
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_SLICE_AM,
                   dec_cont->StrmStorage.num_slices-1);
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_QTABLE_BASE,
               dec_cont->StrmStorage.slices.bus_address);

  if (!dec_cont->StrmStorage.is_rv8)
    SetDecRegister(dec_cont->rv_regs, HWIF_FRAMENUM_LEN,
                   dec_cont->StrmStorage.frame_size_bits);

  /* Setup reference picture buffer */
  if(dec_cont->ref_buf_support) {
    RefbuSetup(&dec_cont->ref_buffer_ctrl, dec_cont->rv_regs,
               REFBU_FRAME,
               dec_cont->FrameDesc.pic_coding_type == RV_I_PIC,
               dec_cont->FrameDesc.pic_coding_type == RV_B_PIC, 0, 2,
               0 );
  }

  if( dec_cont->tiled_mode_support) {
    dec_cont->tiled_reference_enable =
      DecSetupTiledReference( dec_cont->rv_regs,
                              dec_cont->tiled_mode_support,
                              DEC_DPB_FRAME,
                              0 /* interlaced content not present */ );
  } else {
    dec_cont->tiled_reference_enable = 0;
  }


  if (dec_cont->StrmStorage.raw_mode) {
    SetDecRegister(dec_cont->rv_regs, HWIF_RV_OSV_QUANT,
                   dec_cont->FrameDesc.vlc_set );
  }
  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: RvCheckFormatSupport

    Functional description:
        Check if RV supported

    Input:
        container

    Return values:
        return zero for OK

------------------------------------------------------------------------------*/
u32 RvCheckFormatSupport(void) {
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  return (hw_feature.rv_support == RV_NOT_SUPPORTED);
}


/*------------------------------------------------------------------------------

    Function name: RvDecPeek

    Functional description:
        Retrieve last decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct

    Output:
        picture Decoder output picture.

    Return values:
        RVDEC_OK         No picture available.
        RVDEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
RvDecRet RvDecPeek(RvDecInst dec_inst, RvDecPicture * picture) {
  /* Variables */
  RvDecRet return_value = RVDEC_PIC_RDY;
  RvDecContainer *dec_cont;
  u32 pic_index = RV_BUFFER_UNDEFINED;

  /* Code */
  RV_API_TRC("\nRv_dec_peek#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    RV_API_TRC("RvDecPeek# ERROR: picture is NULL");
    return (RVDEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecPeek# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  /* when output release thread enabled, RvDecNextPicture_INTERNAL() called in
     RvDecDecode(), and "dec_cont->StrmStorage.out_count--" may called in
     RvDecNextPicture_INTERNAL() before RvDecPeek() called, so dec_cont->fullness
     used to sample the real out_count in case of RvDecNextPicture_INTERNAL() called
     before than RvDecPeek() */
  u32 tmp = dec_cont->fullness;

  /* Nothing to send out */
  if(!tmp) {
    (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
    return_value = RVDEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.work_out;

    RvFillPicStruct(picture, dec_cont, pic_index);

  }

  return return_value;
}

void RvSetExternalBufferInfo(RvDecContainer * dec_cont) {
  u32 pic_size_in_mbs = 0;
  u32 pic_size;
  u32 ext_buffer_size;
  if( dec_cont->tiled_mode_support) {
    dec_cont->tiled_stride_enable = 1;
  } else {
    dec_cont->tiled_stride_enable = 0;
  }
  if(dec_cont->StrmStorage.max_frame_width) {
    if (!dec_cont->tiled_stride_enable) {
      pic_size_in_mbs = dec_cont->StrmStorage.max_mbs_per_frame;
      pic_size = pic_size_in_mbs * 384;
    } else {
      u32 out_w, out_h;
      out_w = NEXT_MULTIPLE(4 * ((dec_cont->StrmStorage.max_frame_width + 15)>>4) * 16, ALIGN(dec_cont->align));
      out_h = ((dec_cont->StrmStorage.max_frame_height + 15)>>4) * 4;
      pic_size = out_w * out_h * 3 / 2;
    }
  } else {
    if (!dec_cont->tiled_stride_enable) {
      pic_size_in_mbs = ((dec_cont->Hdrs.horizontal_size + 15)>>4) *
                        ((dec_cont->Hdrs.vertical_size + 15)>>4);
      pic_size = pic_size_in_mbs * 384;
    } else {
      u32 out_w, out_h;
      out_w = NEXT_MULTIPLE(4 * ((dec_cont->Hdrs.horizontal_size + 15)>>4) * 16, ALIGN(dec_cont->align));
      out_h = ((dec_cont->Hdrs.vertical_size + 15)>>4) * 4;
      pic_size = out_w * out_h * 3 / 2;
    }
  }

  u32 ref_buff_size = pic_size;
  ext_buffer_size = ref_buff_size;

  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }

  if (dec_cont->pp_enabled)
    dec_cont->buf_num = buffers;
  else
    dec_cont->buf_num =  buffers + 1;
  dec_cont->ext_min_buffer_num = dec_cont->buf_num;
  dec_cont->next_buf_size = ext_buffer_size;
}

RvDecRet RvDecGetBufferInfo(RvDecInst dec_inst, RvDecBufferInfo *mem_info) {
  RvDecContainer  * dec_cont = (RvDecContainer *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;

  if(dec_cont == NULL || mem_info == NULL) {
    return RVDEC_PARAM_ERROR;
  }

  if (dec_cont->StrmStorage.release_buffer) {
    /* Release old buffers from input queue. */
    //buffer = InputQueueGetBuffer(decCont->pp_buffer_queue, 0);
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->StrmStorage.release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (RVDEC_MEMFAIL);
      }
      dec_cont->StrmStorage.ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return RVDEC_OK;
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return RVDEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return RVDEC_OK;
  }

  if(dec_cont->buf_to_free) {
    mem_info->buf_to_free = *dec_cont->buf_to_free;
    dec_cont->buf_to_free->virtual_address = NULL;
    dec_cont->buf_to_free = NULL;
  } else
    mem_info->buf_to_free = empty;

  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;

  ASSERT((mem_info->buf_num && mem_info->next_buf_size) ||
         (mem_info->buf_to_free.virtual_address != NULL));

  return RVDEC_WAITING_FOR_BUFFER;
}

RvDecRet RvDecAddBuffer(RvDecInst dec_inst, struct DWLLinearMem *info) {
  RvDecContainer *dec_cont = (RvDecContainer *)dec_inst;
  RvDecRet dec_ret = RVDEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return RVDEC_PARAM_ERROR;
  }

  u32 i = dec_cont->buffer_index;

  if (dec_cont->buffer_index >= 16)
    /* Too much buffers added. */
    return RVDEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = RVDEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    if(dec_cont->buffer_index <= dec_cont->ext_min_buffer_num) {
      if(dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
        dec_cont->StrmStorage.p_pic_buf[i].data = *info;
      } else {
        dec_cont->StrmStorage.p_rpr_buf.data = *info;
      }
    } else {
      dec_cont->StrmStorage.p_pic_buf[i - 1].data = *info;
      dec_cont->StrmStorage.bq.queue_size++;
      dec_cont->StrmStorage.num_buffers++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->StrmStorage.ext_buffer_added = 1;
  return dec_ret;
}

void RvEnterAbortState(RvDecContainer *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void RvExistAbortState(RvDecContainer *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void RvEmptyBufferQueue(RvDecContainer *dec_cont) {
  BqueueEmpty(&dec_cont->StrmStorage.bq);
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

void RvStateReset(RvDecContainer *dec_cont) {
  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  /* Clear internal parameters in RvDecContainer */
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
  dec_cont->fullness = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->fifo_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->same_slice_header = 0;
  dec_cont->mb_error_conceal = 0;
  dec_cont->dec_stat = RVDEC_OK;
  dec_cont->output_stat = RVDEC_OK;

  /* Clear internal parameters in DecStrmStorage */
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->StrmStorage.strm_dec_ready = 0;
#endif
  dec_cont->StrmStorage.out_index = 0;
  dec_cont->StrmStorage.out_count = 0;
  dec_cont->StrmStorage.skip_b = 0;
  dec_cont->StrmStorage.prev_pic_coding_type = 0;
  dec_cont->StrmStorage.picture_broken = 0;
  dec_cont->StrmStorage.rpr_detected = 0;
  dec_cont->StrmStorage.rpr_next_pic_type = 0;
  dec_cont->StrmStorage.previous_b = 0;
  dec_cont->StrmStorage.previous_mode_full = 0;
  dec_cont->StrmStorage.fwd_scale = 0;
  dec_cont->StrmStorage.bwd_scale = 0;
  dec_cont->StrmStorage.tr = 0;
  dec_cont->StrmStorage.prev_tr = 0;
  dec_cont->StrmStorage.trb = 0;
  dec_cont->StrmStorage.frame_size_bits = 0;
  dec_cont->StrmStorage.pic_id = 0;
  dec_cont->StrmStorage.prev_pic_id = 0;
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.ext_buffer_added = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->StrmStorage.bq.queue_size = buffers;
  dec_cont->StrmStorage.num_buffers = buffers;
#endif

  /* Clear internal parameters in DecApiStorage */
  dec_cont->ApiStorage.DecStat = STREAMDECODING;

  /* Clear internal parameters in DecFrameDesc */
  dec_cont->FrameDesc.frame_number = 0;
  dec_cont->FrameDesc.pic_coding_type = 0;
  dec_cont->FrameDesc.vlc_set = 0;
  dec_cont->FrameDesc.qp = 0;

  (void) DWLmemset(&dec_cont->MbSetDesc, 0, sizeof(DecMbSetDesc));
  (void) DWLmemset(&dec_cont->StrmDesc, 0, sizeof(DecStrmDesc));
  (void) DWLmemset(&dec_cont->out_pic, 0, sizeof(RvDecPicture));
  (void) DWLmemset(dec_cont->StrmStorage.out_buf, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  if (!dec_cont->pp_enabled) {
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf, 0, 16 * sizeof(picture_t));
    (void) DWLmemset(&dec_cont->StrmStorage.p_rpr_buf, 0, sizeof(picture_t));
  }
  (void) DWLmemset(dec_cont->StrmStorage.picture_info, 0, 32 * sizeof(RvDecPicture));
#endif
#ifdef CLEAR_HDRINFO_IN_SEEK
  (void) DWLmemset(&dec_cont->Hdrs, 0, sizeof(DecHdrs));
  (void) DWLmemset(&dec_cont->tmp_hdrs, 0, sizeof(DecHdrs));
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

RvDecRet RvDecAbort(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  RV_API_TRC("RvDecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecAbort# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  RvEnterAbortState(dec_cont);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  RV_API_TRC("RvDecAbort# RVDEC_OK\n");
  return (RVDEC_OK);
}

RvDecRet RvDecAbortAfter(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  RV_API_TRC("RvDecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    RV_API_TRC("RvDecAbortAfter# ERROR: Decoder not initialized");
    return (RVDEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->rv_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  RvEmptyBufferQueue(dec_cont);

  RvStateReset(dec_cont);

  RvExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  RV_API_TRC("RvDecAbortAfter# RVDEC_OK\n");
  return (RVDEC_OK);
}

RvDecRet RvDecSetInfo(RvDecInst dec_inst,
                        struct RvDecConfig *dec_cfg) {
  /*@null@ */ RvDecContainer *dec_cont = (RvDecContainer *)dec_inst;
  u32 pic_width = dec_cont->FrameDesc.frame_width << 4;
  u32 pic_height = dec_cont->FrameDesc.frame_height << 4;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  struct DecHwFeatures hw_feature;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, 8, 1, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(RVDEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, 0, dec_cont->ppu_cfg))
    return RVDEC_PARAM_ERROR;
  if (!hw_feature.dec_stride_support) {
    /* For verion that doesn't support stride, reset alignment to 16B. */
    dec_cont->align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = dec_cfg->align;
  }
  dec_cont->ppu_cfg[0].pixel_width = dec_cont->ppu_cfg[1].pixel_width =
  dec_cont->ppu_cfg[2].pixel_width = dec_cont->ppu_cfg[3].pixel_width = 
  dec_cont->ppu_cfg[4].pixel_width = 8;
  dec_cont->cr_first = dec_cfg->cr_first;
  dec_cont->pp_enabled = dec_cont->ppu_cfg[0].enabled |
                         dec_cont->ppu_cfg[1].enabled |
                         dec_cont->ppu_cfg[2].enabled |
                         dec_cont->ppu_cfg[3].enabled |
                         dec_cont->ppu_cfg[4].enabled;
  return (RVDEC_OK);
}

void RvCheckBufferRealloc(RvDecContainer *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (rvGetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->Hdrs.prev_horizontal_size != dec_cont->Hdrs.horizontal_size ||
          dec_cont->Hdrs.prev_vertical_size != dec_cont->Hdrs.vertical_size)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (rvGetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (rvGetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}
