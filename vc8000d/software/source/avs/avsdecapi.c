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
#include "avsdecapi.h"
#include "basetype.h"
#include "avs_cfg.h"
#include "avs_container.h"
#include "avs_utils.h"
#include "avs_strm.h"
#include "avsdecapi_internal.h"
#include "dwl.h"
#include "regdrv.h"
#include "avs_headers.h"
#include "deccfg.h"
#include "refbuffer.h"
#include "tiledref.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "ppu.h"
#include "string.h"
#include "sw_util.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#ifdef AVSDEC_TRACE
#define AVS_API_TRC(str)    AvsDecTrace((str))
#else
#define AVS_API_TRC(str)
#endif

#define AVS_BUFFER_UNDEFINED    16

#define ID8170_DEC_TIMEOUT        0xFFU
#define ID8170_DEC_SYSTEM_ERROR   0xFEU
#define ID8170_DEC_HW_RESERVED    0xFDU

#define AVSDEC_IS_FIELD_OUTPUT \
    !dec_cont->Hdrs.progressive_sequence && !dec_cont->pp_config_query.deinterlace

#define AVSDEC_NON_PIPELINE_AND_B_PICTURE \
    ((!dec_cont->pp_config_query.pipeline_accepted || !dec_cont->Hdrs.progressive_sequence) \
    && dec_cont->Hdrs.pic_coding_type == BFRAME)
void AvsRefreshRegs(DecContainer * dec_cont);
void AvsFlushRegs(DecContainer * dec_cont);
static u32 AvsHandleVlcModeError(DecContainer * dec_cont, u32 pic_num);
static void AvsHandleFrameEnd(DecContainer * dec_cont);
static u32 RunDecoderAsic(DecContainer * dec_cont, addr_t strm_bus_address);
static void AvsFillPicStruct(AvsDecPicture * picture,
                             DecContainer * dec_cont, u32 pic_index);
static u32 AvsSetRegs(DecContainer * dec_cont, addr_t strm_bus_address);
static u32 AvsCheckFormatSupport(void);
static AvsDecRet AvsDecNextPicture_INTERNAL(AvsDecInst dec_inst,
    AvsDecPicture * picture, u32 end_of_stream);

static void AvsSetExternalBufferInfo(AvsDecInst dec_inst);

static void AvsEnterAbortState(DecContainer *dec_cont);
static void AvsExistAbortState(DecContainer *dec_cont);
static void AvsEmptyBufferQueue(DecContainer *dec_cont);
static void AvsCheckBufferRealloc(DecContainer *dec_cont);

#define DEC_X170_MODE_AVS   11
#define DEC_DPB_NOT_INITIALIZED      -1

/*------------------------------------------------------------------------------
       Version Information - DO NOT CHANGE!
------------------------------------------------------------------------------*/

#define AVSDEC_MAJOR_VERSION 1
#define AVSDEC_MINOR_VERSION 1

/*------------------------------------------------------------------------------

    Function: AvsDecGetAPIVersion

        Functional description:
            Return version information of API

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

AvsDecApiVersion AvsDecGetAPIVersion() {
  AvsDecApiVersion ver;

  ver.major = AVSDEC_MAJOR_VERSION;
  ver.minor = AVSDEC_MINOR_VERSION;

  return (ver);
}

/*------------------------------------------------------------------------------

    Function: AvsDecGetBuild

        Functional description:
            Return build information of SW and HW

        Returns:
            AvsDecBuild

------------------------------------------------------------------------------*/

AvsDecBuild AvsDecGetBuild(void) {
  AvsDecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_AVS_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_AVS_DEC);

  AVS_API_TRC("AvsDecGetBuild# OK\n");

  return build_info;
}

/*------------------------------------------------------------------------------

    Function: AvsDecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance.

        Inputs:
            enum DecErrorHandling error_handling
                            Flag to determine which error concealment method to use.

        Outputs:
            dec_inst         pointer to initialized instance is stored here

        Returns:
            AVSDEC_OK       successfully initialized the instance
            AVSDEC_MEM_FAIL memory allocation failed

------------------------------------------------------------------------------*/
AvsDecRet AvsDecInit(AvsDecInst * dec_inst,
                     const void *dwl,
                     enum DecErrorHandling error_handling,
                     u32 num_frame_buffers,
                     enum DecDpbFlags dpb_flags,
                     u32 use_adaptive_buffers,
                     u32 n_guard_size) {
  /*@null@ */ DecContainer *dec_cont;
  u32 i = 0;
  //u32 version = 0;

  DWLHwConfig config;
  u32 reference_frame_format;
  u32 asic_id, hw_build_id;
  struct DecHwFeatures hw_feature;

  AVS_API_TRC("AvsDecInit#");
  AVSDEC_DEBUG(("AvsAPI_DecoderInit#"));

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if(((-1) >> 1) != (-1)) {
    AVSDEC_DEBUG(("AVSDecInit# ERROR: Right shift is not signed"));
    return (AVSDEC_INITFAIL);
  }
  /*lint -restore */

  if(dec_inst == NULL) {
    AVSDEC_DEBUG(("AVSDecInit# ERROR: dec_inst == NULL"));
    return (AVSDEC_PARAM_ERROR);
  }

  *dec_inst = NULL;

  /* check that AVS decoding supported in HW */
  if(AvsCheckFormatSupport()) {
    AVSDEC_DEBUG(("AVSDecInit# ERROR: AVS not supported in HW\n"));
    return AVSDEC_FORMAT_NOT_SUPPORTED;
  }

  AVSDEC_DEBUG(("size of DecContainer %d \n", sizeof(DecContainer)));
  dec_cont = (DecContainer *) DWLmalloc(sizeof(DecContainer));

  if(dec_cont == NULL) {
    return (AVSDEC_MEMFAIL);
  }

  /* set everything initially zero */
  (void) DWLmemset(dec_cont, 0, sizeof(DecContainer));

  dec_cont->dwl = dwl;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);

  if(num_frame_buffers > 16)   num_frame_buffers = 16;

  dec_cont->StrmStorage.max_num_buffers = num_frame_buffers;

  AvsAPI_InitDataStructures(dec_cont);

  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;
  dec_cont->StrmStorage.unsupported_features_present = 0;

  *dec_inst = (DecContainer *) dec_cont;

  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_AVS_DEC);
  if((asic_id >> 16) == 0x8170U)
    error_handling = 0;

  dec_cont->avs_regs[0] = asic_id;
  for(i = 1; i < TOTAL_X170_REGISTERS; i++)
    dec_cont->avs_regs[i] = 0;

  SetCommonConfigRegs(dec_cont->avs_regs);

  /* Set prediction filter taps */
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_0,-1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_1, 5);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_2, 5);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_3,-1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_0, 1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_1, 7);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_2, 7);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_3, 1);

  (void)DWLmemset(&config, 0, sizeof(DWLHwConfig));

  DWLReadAsicConfig(&config, DWL_CLIENT_TYPE_AVS_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
    AVSDEC_DEBUG("AVSDecInit# ERROR: HW not support 64bit address!\n");
    return (AVSDEC_PARAM_ERROR);
  }

  if (hw_feature.ref_frame_tiled_only)
    dpb_flags = DEC_REF_FRM_TILED_DEFAULT | DEC_DPB_ALLOW_FIELD_ORDERING;

  AVSDEC_DEBUG(("AVS Plus supported: %s\n", hw_feature.avs_plus_support? "YES" : "NO"));
  dec_cont->avs_plus_support = hw_feature.avs_plus_support;

  dec_cont->ref_buf_support = hw_feature.ref_buf_support;
  reference_frame_format = dpb_flags & DEC_REF_FRM_FMT_MASK;

  if(reference_frame_format == DEC_REF_FRM_TILED_DEFAULT) {
    /* Assert support in HW before enabling.. */
    if(!hw_feature.tiled_mode_support) {
      return AVSDEC_FORMAT_NOT_SUPPORTED;
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
    return (AVSDEC_PARAM_ERROR);
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
    return (AVSDEC_MEMFAIL);
  }
  dec_cont->StrmStorage.release_buffer = 0;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->allow_dpb_field_ordering = 0;
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;

  if( dpb_flags & DEC_DPB_ALLOW_FIELD_ORDERING )
    dec_cont->allow_dpb_field_ordering = hw_feature.field_dpb_support;

  dec_cont->StrmStorage.intra_freeze = error_handling == DEC_EC_VIDEO_FREEZE;

  if (error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->StrmStorage.partial_freeze = 1;
  else if (error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->StrmStorage.partial_freeze = 2;

  dec_cont->StrmStorage.picture_broken = 0;

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK)
    return AVSDEC_MEMFAIL;

  dec_cont->use_adaptive_buffers = use_adaptive_buffers;
  dec_cont->n_guard_size = n_guard_size;

  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */
  if (dec_cont->tiled_mode_support) {
    dec_cont->min_dec_pic_width = AVS_MIN_WIDTH_EN_DTRC;
    dec_cont->min_dec_pic_height = AVS_MIN_HEIGHT_EN_DTRC;
  } else {
    dec_cont->min_dec_pic_width = AVS_MIN_WIDTH;
    dec_cont->min_dec_pic_height = AVS_MIN_HEIGHT;
  }

  AVSDEC_DEBUG(("Container 0x%x\n", (u32) dec_cont));
  AVS_API_TRC("AvsDecInit: OK\n");

  return (AVSDEC_OK);
}

/*------------------------------------------------------------------------------

    Function: AvsDecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before AvsDecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            AVSDEC_OK            success
            AVSDEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
AvsDecRet AvsDecGetInfo(AvsDecInst dec_inst, AvsDecInfo * dec_info) {

#define API_STOR ((DecContainer *)dec_inst)->ApiStorage
#define DEC_STST ((DecContainer *)dec_inst)->StrmStorage
#define DEC_HDRS ((DecContainer *)dec_inst)->Hdrs
#define DEC_REGS ((DecContainer *)dec_inst)->avs_regs
  AVS_API_TRC("AvsDecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    return AVSDEC_PARAM_ERROR;
  }

  dec_info->multi_buff_pp_size = 2;

  if(API_STOR.DecStat == UNINIT || API_STOR.DecStat == INITIALIZED) {
    return AVSDEC_HDRS_NOT_RDY;
  }

  dec_info->frame_width = DEC_STST.frame_width << 4;
  dec_info->frame_height = DEC_STST.frame_height << 4;

  dec_info->coded_width = DEC_HDRS.horizontal_size;
  dec_info->coded_height = DEC_HDRS.vertical_size;

  dec_info->profile_id = DEC_HDRS.profile_id;
  dec_info->level_id = DEC_HDRS.level_id;
  dec_info->video_range = DEC_HDRS.sample_range;
  dec_info->video_format = DEC_HDRS.video_format;
  dec_info->interlaced_sequence = !DEC_HDRS.progressive_sequence;
  dec_info->dpb_mode = ((DecContainer *)dec_inst)->dpb_mode;
  dec_info->pic_buff_size = ((DecContainer *)dec_inst)->buf_num;

  AvsDecAspectRatio((DecContainer *) dec_inst, dec_info);

  if(((DecContainer *)dec_inst)->tiled_mode_support) {
    if(!DEC_HDRS.progressive_sequence &&
        (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
      dec_info->output_format = AVSDEC_SEMIPLANAR_YUV420;
    } else {
      dec_info->output_format = AVSDEC_TILED_YUV420;
    }
  } else {
    dec_info->output_format = AVSDEC_SEMIPLANAR_YUV420;
  }

  AVS_API_TRC("AvsDecGetInfo: OK");
  return (AVSDEC_OK);

#undef API_STOR
#undef DEC_STST
#undef DEC_HDRS
#undef DEC_REGS

}

/*------------------------------------------------------------------------------

    Function: AvsDecDecode

        Functional description:
            Decode stream data. Calls StrmDec_Decode to do the actual decoding.

        Input:
            dec_inst     decoder instance
            input      pointer to input struct

        Outputs:
            output     pointer to output struct

        Returns:
            AVSDEC_NOT_INITIALIZED   decoder instance not initialized yet
            AVSDEC_PARAM_ERROR       invalid parameters

            AVSDEC_STRM_PROCESSED    stream buffer decoded
            AVSDEC_HDRS_RDY          headers decoded
            AVSDEC_PIC_DECODED       decoding of a picture finished
            AVSDEC_STRM_ERROR        serious error in decoding, no
                                       valid parameter sets available
                                       to decode picture data

------------------------------------------------------------------------------*/

AvsDecRet AvsDecDecode(AvsDecInst dec_inst,
                       AvsDecInput * input, AvsDecOutput * output) {
#define API_STOR ((DecContainer *)dec_inst)->ApiStorage
#define DEC_STRM ((DecContainer *)dec_inst)->StrmDesc

  DecContainer *dec_cont;
  AvsDecRet internal_ret;
  u32 strm_dec_result;
  u32 asic_status;
  i32 ret = 0;
  u32 field_rdy = 0;
  u32 error_concealment = 0;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  AVS_API_TRC("\nAvs_dec_decode#");

  if(input == NULL || output == NULL || dec_inst == NULL) {
    AVS_API_TRC("AvsDecDecode# PARAM_ERROR\n");
    return AVSDEC_PARAM_ERROR;
  }

  dec_cont = ((DecContainer *) dec_inst);

  if(dec_cont->StrmStorage.unsupported_features_present) {
    return (AVSDEC_FORMAT_NOT_SUPPORTED);
  }

  /*
   *  Check if decoder is in an incorrect mode
   */
  if(API_STOR.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecDecode: NOT_INITIALIZED\n");
    return AVSDEC_NOT_INITIALIZED;
  }
  if(dec_cont->abort) {
    return (AVSDEC_ABORTED);
  }

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      input->stream == NULL || input->stream_bus_address == 0) {
    AVS_API_TRC("AvsDecDecode# PARAM_ERROR\n");
    return AVSDEC_PARAM_ERROR;
  }

  /* If we have set up for delayed resolution change, do it here */
  if(dec_cont->StrmStorage.new_headers_change_resolution) {
#ifndef USE_OMXIL_BUFFER
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
    if (dec_cont->pp_enabled)
      InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
    dec_cont->StrmStorage.new_headers_change_resolution = 0;
    dec_cont->Hdrs.horizontal_size = dec_cont->tmp_hdrs.horizontal_size;
    dec_cont->Hdrs.vertical_size = dec_cont->tmp_hdrs.vertical_size;
    /* Set rest of parameters just in case */
    dec_cont->Hdrs.aspect_ratio = dec_cont->tmp_hdrs.aspect_ratio;
    dec_cont->Hdrs.frame_rate_code = dec_cont->tmp_hdrs.frame_rate_code;
    dec_cont->Hdrs.bit_rate_value = dec_cont->tmp_hdrs.bit_rate_value;

    dec_cont->StrmStorage.frame_width =
      (dec_cont->Hdrs.horizontal_size + 15) >> 4;
    if(dec_cont->Hdrs.progressive_sequence)
      dec_cont->StrmStorage.frame_height =
        (dec_cont->Hdrs.vertical_size + 15) >> 4;
    else
      dec_cont->StrmStorage.frame_height =
        2 * ((dec_cont->Hdrs.vertical_size + 31) >> 5);
    dec_cont->StrmStorage.total_mbs_in_frame =
      (dec_cont->StrmStorage.frame_width *
       dec_cont->StrmStorage.frame_height);
  }

  if(API_STOR.DecStat == HEADERSDECODED) {
    /* check if buffer need to be realloced,
       both external buffer and internal buffer */
    AvsCheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = AVSDEC_WAITING_FOR_BUFFER;
        }

        AvsFreeBuffers(dec_cont);
        if(!dec_cont->StrmStorage.direct_mvs.virtual_address) {
          AVSDEC_DEBUG(("Allocate buffers\n"));
          internal_ret = AvsAllocateBuffers(dec_cont);
          if(internal_ret != AVSDEC_OK) {
            AVSDEC_DEBUG(("ALLOC BUFFER FAIL\n"));
            AVS_API_TRC("AvsDecDecode# MEMFAIL\n");
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
          ret = AVSDEC_WAITING_FOR_BUFFER;
        }
      }

      if (dec_cont->realloc_int_buf) {
        AvsFreeBuffers(dec_cont);
        if(!dec_cont->StrmStorage.direct_mvs.virtual_address) {
          AVSDEC_DEBUG(("Allocate buffers\n"));
          internal_ret = AvsAllocateBuffers(dec_cont);
          if(internal_ret != AVSDEC_OK) {
            AVSDEC_DEBUG(("ALLOC BUFFER FAIL\n"));
            AVS_API_TRC("AvsDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    }
  }

  /*
   *  Update stream structure
   */
  DEC_STRM.p_strm_buff_start = input->stream;
  DEC_STRM.strm_curr_pos = input->stream;
  DEC_STRM.bit_pos_in_word = 0;
  DEC_STRM.strm_buff_size = input->data_len;
  DEC_STRM.strm_buff_read_bits = 0;

#ifdef _DEC_PP_USAGE
  dec_cont->StrmStorage.latest_id = input->pic_id;
#endif
  do {
    AVSDEC_DEBUG(("Start Decode\n"));
    /* run SW if HW is not in the middle of processing a picture
     * (indicated as HW_PIC_STARTED decoder status) */
    if(API_STOR.DecStat == HEADERSDECODED) {
      API_STOR.DecStat = STREAMDECODING;
      if(dec_cont->realloc_ext_buf) {
        dec_cont->buffer_index = 0;
        AvsSetExternalBufferInfo(dec_cont);
        ret =  AVSDEC_WAITING_FOR_BUFFER;
      }
    } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
      ret = AVSDEC_WAITING_FOR_BUFFER;
    } else if(API_STOR.DecStat != HW_PIC_STARTED) {
      strm_dec_result = AvsStrmDec_Decode(dec_cont);
      /* TODO: Could it be odd field? If, so release PP and Hw. */
      switch (strm_dec_result) {
      case DEC_PIC_HDR_RDY:
        /* if type inter predicted and no reference -> error */
        if((dec_cont->Hdrs.pic_coding_type == PFRAME &&
            dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE) ||
            (dec_cont->Hdrs.pic_coding_type == BFRAME &&
             (dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE ||
              dec_cont->StrmStorage.skip_b ||
              input->skip_non_reference)) ||
            (dec_cont->Hdrs.pic_coding_type == PFRAME &&
             dec_cont->StrmStorage.picture_broken &&
             dec_cont->StrmStorage.intra_freeze) ) {
          if(dec_cont->StrmStorage.skip_b ||
              input->skip_non_reference) {
            AVS_API_TRC("AvsDecDecode# AVSDEC_NONREF_PIC_SKIPPED\n");
          }
          if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
            InputQueueReturnBuffer(dec_cont->pp_buffer_queue,
                dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].
                pp_data->virtual_address);
          ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
          error_concealment = 1;
        } else
          API_STOR.DecStat = HW_PIC_STARTED;
        break;

      case DEC_PIC_SUPRISE_B:
        /* Handle suprise B */
        dec_cont->Hdrs.low_delay = 0;

        AvsDecBufferPicture(dec_cont,
                            input->pic_id, 1, 0,
                            AVSDEC_PIC_DECODED, 0);

        ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = 1;
        break;

      case DEC_PIC_HDR_RDY_ERROR:
        if(dec_cont->StrmStorage.unsupported_features_present) {
          dec_cont->StrmStorage.unsupported_features_present = 0;
          return AVSDEC_STREAM_NOT_SUPPORTED;
        }
        if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
          InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data->virtual_address);
        ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = 1;
        break;

      case DEC_HDRS_RDY:

        internal_ret = AvsDecCheckSupport(dec_cont);
        if(internal_ret != AVSDEC_OK) {
          dec_cont->StrmStorage.strm_dec_ready = FALSE;
          dec_cont->StrmStorage.valid_sequence = 0;
          API_STOR.DecStat = INITIALIZED;
          return internal_ret;
        }

        if(dec_cont->ApiStorage.first_headers) {
          dec_cont->ApiStorage.first_headers = 0;

          if (!hw_feature.pic_size_reg_unified)
            SetDecRegister(dec_cont->avs_regs, HWIF_PIC_MB_WIDTH,
                           dec_cont->StrmStorage.frame_width);
          else
            SetDecRegister(dec_cont->avs_regs, HWIF_PIC_WIDTH_IN_CBS,
                           dec_cont->StrmStorage.frame_width << 1);
          SetDecRegister(dec_cont->avs_regs, HWIF_DEC_MODE,
                         DEC_X170_MODE_AVS);

          if (dec_cont->ref_buf_support) {
            RefbuInit(&dec_cont->ref_buffer_ctrl,
                      DEC_X170_MODE_AVS,
                      dec_cont->StrmStorage.frame_width,
                      dec_cont->StrmStorage.frame_height,
                      dec_cont->ref_buf_support);
          }
        }

        /* Initialize DPB mode */
        if( !dec_cont->Hdrs.progressive_sequence &&
            dec_cont->allow_dpb_field_ordering )
          dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
        else
          dec_cont->dpb_mode = DEC_DPB_FRAME;

        /* Initialize tiled mode */
        if( dec_cont->tiled_mode_support) {
          /* Check mode validity */
          if(DecCheckTiledMode( dec_cont->tiled_mode_support,
                                dec_cont->dpb_mode,
                                !dec_cont->Hdrs.progressive_sequence ) !=
              HANTRO_OK ) {
            AVS_API_TRC("AvsDecDecode# ERROR: DPB mode does not "\
                        "support tiled reference pictures");
            return AVSDEC_PARAM_ERROR;
          }
        }

        API_STOR.DecStat = HEADERSDECODED;

        if (dec_cont->pp_enabled) {
          dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
          dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
        }

        FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
        AVSDEC_DEBUG(("HDRS_RDY\n"));
        ret = AVSDEC_HDRS_RDY;
        break;

      default:
        ASSERT(strm_dec_result == DEC_END_OF_STREAM);
        if (dec_cont->StrmStorage.new_headers_change_resolution)
          ret = AVSDEC_PIC_DECODED;
        else
          ret = AVSDEC_STRM_PROCESSED;
        break;
      }
    }

    /* picture header properly decoded etc -> start HW */
    if(API_STOR.DecStat == HW_PIC_STARTED) {
      if(dec_cont->ApiStorage.first_field &&
          !dec_cont->asic_running) {
        dec_cont->StrmStorage.work_out = BqueueNext2(
                                           &dec_cont->StrmStorage.bq,
                                           dec_cont->StrmStorage.work0,
                                           dec_cont->StrmStorage.work1,
                                           BQUEUE_UNUSED,
                                           dec_cont->Hdrs.pic_coding_type == BFRAME );
        if(dec_cont->StrmStorage.work_out == (u32)0xFFFFFFFFU) {
          if (dec_cont->abort)
            return AVSDEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          else {
            ret = AVSDEC_NO_DECODING_BUFFER;
            break;
          }
#endif
        }
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 1;

        if (dec_cont->pp_enabled) {
          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data =
              InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
          if (dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data == NULL)
            return AVSDEC_ABORTED;
        }
      }

      if (!dec_cont->asic_running && dec_cont->StrmStorage.partial_freeze) {
        PreparePartialFreeze(
          (u8*)dec_cont->StrmStorage.p_pic_buf[
            (i32)dec_cont->StrmStorage.work_out].data.virtual_address,
          dec_cont->StrmStorage.frame_width,
          dec_cont->StrmStorage.frame_height);
      }

      asic_status = RunDecoderAsic(dec_cont, input->stream_bus_address);

      if(asic_status == ID8170_DEC_TIMEOUT) {
        ret = AVSDEC_HW_TIMEOUT;
      } else if(asic_status == ID8170_DEC_SYSTEM_ERROR) {
        ret = AVSDEC_SYSTEM_ERROR;
      } else if(asic_status == ID8170_DEC_HW_RESERVED) {
        ret = AVSDEC_HW_RESERVED;
      } else if(asic_status & DEC_8190_IRQ_ABORT) {
        AVSDEC_DEBUG(("IRQ ABORT IN HW\n"));
        ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = 1;
      } else if( (asic_status & AVS_DEC_X170_IRQ_STREAM_ERROR) ||
                 (asic_status & AVS_DEC_X170_IRQ_TIMEOUT) ) {
        if (!dec_cont->StrmStorage.partial_freeze ||
            !ProcessPartialFreeze(
              (u8*)dec_cont->StrmStorage.p_pic_buf[
                (i32)dec_cont->StrmStorage.work_out].data.virtual_address,
              dec_cont->StrmStorage.work0 != INVALID_ANCHOR_PICTURE ?
              (u8*)dec_cont->StrmStorage.p_pic_buf[
                (i32)dec_cont->StrmStorage.work0].data.virtual_address :
              NULL,
              dec_cont->StrmStorage.frame_width,
              dec_cont->StrmStorage.frame_height,
              dec_cont->StrmStorage.partial_freeze == 1)) {
          if (asic_status & AVS_DEC_X170_IRQ_STREAM_ERROR) {
            AVSDEC_DEBUG(("STREAM ERROR IN HW\n"));
          } else {
            AVSDEC_DEBUG(("IRQ TIMEOUT IN HW\n"));
          }

          if (dec_cont->pp_enabled) {
            InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data->virtual_address);
          }
          ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
          error_concealment = 1;
          if (asic_status & AVS_DEC_X170_IRQ_STREAM_ERROR) {
            dec_cont->StrmStorage.prev_pic_coding_type =
              dec_cont->Hdrs.pic_coding_type;
          }
        } else {
          asic_status &= ~AVS_DEC_X170_IRQ_STREAM_ERROR;
          asic_status &= ~AVS_DEC_X170_IRQ_TIMEOUT;
          asic_status |= AVS_DEC_X170_IRQ_DEC_RDY;
          error_concealment = HANTRO_FALSE;
        }
      } else if(asic_status & AVS_DEC_X170_IRQ_BUFFER_EMPTY) {
        AvsDecPreparePicReturn(dec_cont);
        ret = AVSDEC_BUF_EMPTY;

      }
      /* HW finished decoding a picture */
      else if(asic_status & AVS_DEC_X170_IRQ_DEC_RDY) {
      } else {
        ASSERT(0);
      }

      /* HW finished decoding a picture */
      if(asic_status & AVS_DEC_X170_IRQ_DEC_RDY) {
        if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
            !dec_cont->ApiStorage.first_field) {
          field_rdy = 0;
          dec_cont->StrmStorage.frame_number++;

          AvsHandleFrameEnd(dec_cont);

          AvsDecBufferPicture(dec_cont,
                              input->pic_id,
                              dec_cont->Hdrs.
                              pic_coding_type == BFRAME,
                              dec_cont->Hdrs.
                              pic_coding_type == PFRAME,
                              AVSDEC_PIC_DECODED, 0);

          ret = AVSDEC_PIC_DECODED;

          dec_cont->ApiStorage.first_field = 1;
          if(dec_cont->Hdrs.pic_coding_type != BFRAME) {
            dec_cont->StrmStorage.work1 =
              dec_cont->StrmStorage.work0;
            dec_cont->StrmStorage.work0 =
              dec_cont->StrmStorage.work_out;
            if( dec_cont->StrmStorage.skip_b )
              dec_cont->StrmStorage.skip_b--;
          }
          dec_cont->StrmStorage.prev_pic_coding_type =
            dec_cont->Hdrs.pic_coding_type;
          if( dec_cont->Hdrs.pic_coding_type != BFRAME )
            dec_cont->StrmStorage.prev_pic_structure =
              dec_cont->Hdrs.picture_structure;

          if( dec_cont->Hdrs.pic_coding_type == IFRAME )
            dec_cont->StrmStorage.picture_broken = 0;
        } else {
          field_rdy = 1;
          AvsHandleFrameEnd(dec_cont);
          dec_cont->ApiStorage.first_field = 0;
          if((u32) (dec_cont->StrmDesc.strm_curr_pos -
                    dec_cont->StrmDesc.p_strm_buff_start) >=
              input->data_len) {
            ret = AVSDEC_BUF_EMPTY;
          }
        }
        dec_cont->StrmStorage.valid_pic_header = HANTRO_FALSE;

        /* handle first field indication */
        if(!dec_cont->Hdrs.progressive_sequence) {
          if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE)
            dec_cont->StrmStorage.field_index++;
          else
            dec_cont->StrmStorage.field_index = 1;
        }

        AvsDecPreparePicReturn(dec_cont);
      }

      if(ret != AVSDEC_STRM_PROCESSED && ret != AVSDEC_BUF_EMPTY && !field_rdy) {
        API_STOR.DecStat = STREAMDECODING;
      }
    }
  } while(ret == 0);

  if( error_concealment && dec_cont->Hdrs.pic_coding_type != BFRAME ) {
    dec_cont->StrmStorage.picture_broken = 1;
  }

  AVS_API_TRC("AvsDecDecode: Exit\n");
  output->strm_curr_pos = dec_cont->StrmDesc.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);
  output->data_left = dec_cont->StrmDesc.strm_buff_size -
                      (output->strm_curr_pos - DEC_STRM.p_strm_buff_start);

  u32 tmpret;
  AvsDecPicture tmp_output;
  do {
    tmpret = AvsDecNextPicture_INTERNAL(dec_cont, &tmp_output, 0);
    if(tmpret == AVSDEC_ABORTED)
      return (AVSDEC_ABORTED);
  } while( tmpret == AVSDEC_PIC_RDY);

  if(dec_cont->abort)
    return(AVSDEC_ABORTED);
  else
    return ((AvsDecRet) ret);

#undef API_STOR
#undef DEC_STRM

}

/*------------------------------------------------------------------------------

    Function: AvsDecRelease()

        Functional description:
            Release the decoder instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void AvsDecRelease(AvsDecInst dec_inst) {
  DecContainer *dec_cont = NULL;

  AVS_API_TRC("AvsDecRelease#");
  if(dec_inst == NULL) {
    AVS_API_TRC("AvsDecRelease# ERROR: dec_inst == NULL");
    return;
  }

  dec_cont = ((DecContainer *) dec_inst);

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if(dec_cont->asic_running) {
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  AvsFreeBuffers(dec_cont);

  DWLfree(dec_cont);

  AVS_API_TRC("AvsDecRelease: OK");
}

/*------------------------------------------------------------------------------
    Function name   : AvsRefreshRegs
    Description     :
    Return type     : void
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsRefreshRegs(DecContainer * dec_cont) {
  i32 i;
  u32 *pp_regs = dec_cont->avs_regs;

  for(i = 0; i < DEC_X170_REGISTERS; i++) {
    pp_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
  }
}

/*------------------------------------------------------------------------------
    Function name   : AvsFlushRegs
    Description     :
    Return type     : void
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsFlushRegs(DecContainer * dec_cont) {
  i32 i;
  u32 *pp_regs = dec_cont->avs_regs;

  for(i = 2; i < DEC_X170_REGISTERS; i++) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, pp_regs[i]);
    pp_regs[i] = 0;
  }
#if 0
#ifdef USE_64BIT_ENV
  offset = TOTAL_X170_ORIGIN_REGS * 0x04;
  pp_regs = dec_cont->avs_regs + TOTAL_X170_ORIGIN_REGS;
  for(i = DEC_X170_EXPAND_REGS; i > 0; --i) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
    *pp_regs = 0;
    pp_regs++;
    offset += 4;
  }
#endif
  offset = 314 * 0x04;
  pp_regs =  dec_cont->avs_regs + 314;
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
  offset = PP_START_UNIFIED_REGS * 0x04;
  pp_regs =  dec_cont->avs_regs + PP_START_UNIFIED_REGS;
  for(i = PP_UNIFIED_REGS; i > 0; --i) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
    pp_regs++;
    offset += 4;
  }
#endif
}

/*------------------------------------------------------------------------------
    Function name   : AvsHandleVlcModeError
    Description     :
    Return type     : u32
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
u32 AvsHandleVlcModeError(DecContainer * dec_cont, u32 pic_num) {
  u32 ret = 0, tmp;

  ASSERT(dec_cont->StrmStorage.strm_dec_ready);

  tmp = AvsStrmDec_NextStartCode(dec_cont);
  if(tmp != END_OF_STREAM) {
    dec_cont->StrmDesc.strm_curr_pos -= 4;
    dec_cont->StrmDesc.strm_buff_read_bits -= 32;
  }

  /* error in first picture -> set reference to grey */
  if(!dec_cont->StrmStorage.frame_number) {
    if (!dec_cont->tiled_stride_enable) {
      (void) DWLmemset(dec_cont->StrmStorage.
                       p_pic_buf[(i32)dec_cont->StrmStorage.work_out].data.
                       virtual_address, 128,
                       384 * dec_cont->StrmStorage.total_mbs_in_frame);
    }
    AvsDecPreparePicReturn(dec_cont);

    /* no pictures finished -> return STRM_PROCESSED */
    if(tmp == END_OF_STREAM)
      ret = AVSDEC_STRM_PROCESSED;
    else
      ret = 0;
    dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
    dec_cont->StrmStorage.skip_b = 2;
  } else {
    if(dec_cont->Hdrs.pic_coding_type != BFRAME) {
      dec_cont->StrmStorage.frame_number++;

      BqueueDiscard( &dec_cont->StrmStorage.bq,
                     dec_cont->StrmStorage.work_out );
      dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work0;

      AvsDecBufferPicture(dec_cont,
                          pic_num,
                          dec_cont->Hdrs.pic_coding_type == BFRAME,
                          1, (AvsDecRet) FREEZED_PIC_RDY,
                          dec_cont->StrmStorage.total_mbs_in_frame);

      ret = AVSDEC_PIC_DECODED;

      dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
      dec_cont->StrmStorage.skip_b = 2;
    } else {
      if(dec_cont->StrmStorage.intra_freeze) {
        dec_cont->StrmStorage.frame_number++;

        AvsDecBufferPicture(dec_cont,
                            pic_num,
                            dec_cont->Hdrs.pic_coding_type == BFRAME,
                            1, (AvsDecRet) FREEZED_PIC_RDY,
                            dec_cont->StrmStorage.total_mbs_in_frame);

        ret = AVSDEC_PIC_DECODED;

      }
    }
  }

  dec_cont->ApiStorage.first_field = 1;

  dec_cont->ApiStorage.DecStat = STREAMDECODING;
  dec_cont->StrmStorage.valid_pic_header = HANTRO_FALSE;
  dec_cont->Hdrs.picture_structure = FRAMEPICTURE;

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : AvsHandleFrameEnd
    Description     :
    Return type     : u32
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsHandleFrameEnd(DecContainer * dec_cont) {

  u32 tmp;

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos -
         dec_cont->StrmDesc.p_strm_buff_start);
  dec_cont->StrmDesc.bit_pos_in_word = 0;

  do {
    tmp = AvsStrmDec_ShowBits(dec_cont, 32);
    if((tmp >> 8) == 0x1)
      break;
  } while(AvsStrmDec_FlushBits(dec_cont, 8) == HANTRO_OK);

}

/*------------------------------------------------------------------------------

         Function name: RunDecoderAsic

         Purpose:       Set Asic run lenght and run Asic

         Input:         DecContainer *dec_cont

         Output:        void

------------------------------------------------------------------------------*/
u32 RunDecoderAsic(DecContainer * dec_cont, addr_t strm_bus_address) {
  i32 ret;
  addr_t tmp = 0;
  u32 asic_status = 0;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  struct DecHwFeatures hw_feature;
  addr_t mask;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  ASSERT(dec_cont->StrmStorage.
         p_pic_buf[(i32)dec_cont->StrmStorage.work_out].data.bus_address != 0);
  ASSERT(strm_bus_address != 0);

  if (hw_feature.g1_strm_128bit_align)
    mask = 15;
  else
    mask = 7;

  /* Save frame/Hdr info for current picture. */
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].frame_width
    = dec_cont->StrmStorage.frame_width;
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].frame_height
    = dec_cont->StrmStorage.frame_height;
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].Hdrs
    = dec_cont->Hdrs;

  if(!dec_cont->asic_running) {
    tmp = AvsSetRegs(dec_cont, strm_bus_address);
    if(tmp == HANTRO_NOK)
      return 0;

    if (!dec_cont->keep_hw_reserved)
      (void) DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_AVS_DEC);

    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_DIS, 0);

    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);
    AvsFlushRegs(dec_cont);
    /* Enable HW */
    DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, NULL, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 1);
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id,
                4 * 1, dec_cont->avs_regs[1]);
  } else { /* in the middle of decoding, continue decoding */
    /* tmp is strm_bus_address + number of bytes decoded by SW */
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
    tmp = strm_bus_address + tmp;

    /* pointer to start of the stream, mask to get the pointer to
     * previous 64/128-bit aligned position */
    if(!(tmp & ~(mask))) {
      return 0;
    }

    SET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));
    /* amount of stream (as seen by the HW), obtained as amount of stream
     * given by the application subtracted by number of bytes decoded by
     * SW (if strm_bus_address is not 64/128-bit aligned -> adds number of bytes
     * from previous 64/128-bit aligned boundary) */
    SetDecRegister(dec_cont->avs_regs, HWIF_STREAM_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));

    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_BUFFER_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));
    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_OFFSET, 0);

    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

    /* This depends on actual register allocation */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->avs_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->avs_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->avs_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->avs_regs[259]);
    if (IS_LEGACY(dec_cont->avs_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->avs_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->avs_regs[169]);
    if(sizeof(addr_t) == 8) {
      if (hw_feature.addr64_support) {
        if (IS_LEGACY(dec_cont->avs_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->avs_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->avs_regs[168]);
      } else {
        ASSERT(dec_cont->avs_regs[122] == 0);
        ASSERT(dec_cont->avs_regs[168] == 0);
      }
    } else {
      if (hw_feature.addr64_support) {
        if (IS_LEGACY(dec_cont->avs_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id,
                4 * 1, dec_cont->avs_regs[1]);
  }

  /* Wait for HW ready */
  ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                       (u32) DEC_X170_TIMEOUT_LENGTH);

  AvsRefreshRegs(dec_cont);

  if(ret == DWL_HW_WAIT_OK) {
    asic_status =
      GetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT);
  } else if(ret == DWL_HW_WAIT_TIMEOUT) {
    asic_status = ID8170_DEC_TIMEOUT;
  } else {
    asic_status = ID8170_DEC_SYSTEM_ERROR;
  }

  if(!(asic_status & AVS_DEC_X170_IRQ_BUFFER_EMPTY) ||
      (asic_status & AVS_DEC_X170_IRQ_STREAM_ERROR) ||
      (asic_status & AVS_DEC_X170_IRQ_BUS_ERROR) ||
      (asic_status == ID8170_DEC_TIMEOUT) ||
      (asic_status == ID8170_DEC_SYSTEM_ERROR)) {
    /* reset HW */
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->avs_regs[1]);

    dec_cont->keep_hw_reserved = 0;

    dec_cont->asic_running = 0;
    if (!dec_cont->keep_hw_reserved)
      (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }

  /* if HW interrupt indicated either BUFFER_EMPTY or
   * DEC_RDY -> read stream end pointer and update StrmDesc structure */
  if((asic_status &
      (AVS_DEC_X170_IRQ_BUFFER_EMPTY | AVS_DEC_X170_IRQ_DEC_RDY))) {
    tmp = GET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE);

    if(((tmp - strm_bus_address) <= dec_cont->max_strm_len) &&
        ((tmp - strm_bus_address) <= dec_cont->StrmDesc.strm_buff_size)) {
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
  if( dec_cont->Hdrs.pic_coding_type != BFRAME &&
      dec_cont->ref_buf_support &&
      (asic_status & AVS_DEC_X170_IRQ_DEC_RDY) &&
      dec_cont->asic_running == 0) {
    RefbuMvStatistics( &dec_cont->ref_buffer_ctrl,
                       dec_cont->avs_regs,
                       NULL,
                       HANTRO_FALSE,
                       dec_cont->Hdrs.pic_coding_type == IFRAME );
  }

  SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);

  return asic_status;

}

/*------------------------------------------------------------------------------

    Function name: AvsDecNextPicture

    Functional description:
        Retrieve next decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        AVSDEC_OK         No picture available.
        AVSDEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
AvsDecRet AvsDecNextPicture(AvsDecInst dec_inst,
                            AvsDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  DecContainer *dec_cont;
  i32 ret;

  /* Code */
  AVS_API_TRC("\nAvs_dec_next_picture#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    AVS_API_TRC("AvsDecNextPicture# ERROR: picture is NULL");
    return (AVSDEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecNextPicture# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
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
    if (ret == FIFO_EMPTY) return AVSDEC_OK;
#endif

    if ((i32)i == -1) {
      AVS_API_TRC("AvsDecNextPicture# AVSDEC_END_OF_STREAM\n");
      return AVSDEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      AVS_API_TRC("AvsDecNextPicture# AVSDEC_FLUSHED\n");
      return AVSDEC_FLUSHED;
    }

    *picture = dec_cont->StrmStorage.picture_info[i];

    AVS_API_TRC("AvsDecNextPicture# AVSDEC_PIC_RDY\n");
    return (AVSDEC_PIC_RDY);
  } else
    return AVSDEC_ABORTED;
}

/*------------------------------------------------------------------------------

    Function name: AvsDecNextPicture_INTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        AVSDEC_OK               No picture available.
        AVSDEC_PIC_RDY          Picture ready.
        AVSDEC_PARAM_ERROR      invalid parameters
        AVSDEC_NOT_INITIALIZED  decoder instance not initialized yet

------------------------------------------------------------------------------*/
AvsDecRet AvsDecNextPicture_INTERNAL(AvsDecInst dec_inst,
                                     AvsDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  AvsDecRet return_value = AVSDEC_PIC_RDY;
  DecContainer *dec_cont;
  u32 pic_index = AVS_BUFFER_UNDEFINED;
  u32 min_count;

  /* Code */
  AVS_API_TRC("\nAvs_dec_next_picture_INTERNAL#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    AVS_API_TRC("AvsDecNextPicture_INTERNAL# ERROR: picture is NULL");
    return (AVSDEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecNextPicture_INTERNAL# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }

  memset(picture, 0, sizeof(AvsDecPicture));
  min_count = 0;
  if(dec_cont->StrmStorage.sequence_low_delay == 0 && !end_of_stream &&
      !dec_cont->StrmStorage.new_headers_change_resolution)
    min_count = 1;

  /* this is to prevent post-processing of non-finished pictures in the
   * end of the stream */
  if(end_of_stream && dec_cont->Hdrs.pic_coding_type == BFRAME) {
    dec_cont->Hdrs.pic_coding_type = PFRAME;
  }

  /* Nothing to send out */
  if(dec_cont->StrmStorage.out_count <= min_count) {
    (void) DWLmemset(picture, 0, sizeof(AvsDecPicture));
    picture->pictures[0].output_picture = NULL;
    picture->interlaced = !dec_cont->Hdrs.progressive_sequence;
    return_value = AVSDEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.out_index;
    pic_index = dec_cont->StrmStorage.out_buf[pic_index];

    AvsFillPicStruct(picture, dec_cont, pic_index);

    /* field output */
    //if(AVSDEC_IS_FIELD_OUTPUT)
    if(!dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_sequence) {
      picture->interlaced = 1;
      picture->field_picture = 1;

      if(!dec_cont->ApiStorage.output_other_field) {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[(i32)pic_index].tf ? 1 : 0;
        dec_cont->ApiStorage.output_other_field = 1;
      } else {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[(i32)pic_index].tf ? 0 : 1;
        dec_cont->ApiStorage.output_other_field = 0;
        dec_cont->StrmStorage.out_count--;
        dec_cont->StrmStorage.out_index++;
        dec_cont->StrmStorage.out_index &= 15;
      }
    } else {
      /* progressive or deinterlaced frame output */
      picture->interlaced = !dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_sequence;
      picture->top_field = 0;
      picture->field_picture = 0;
      dec_cont->StrmStorage.out_count--;
      dec_cont->StrmStorage.out_index++;
      dec_cont->StrmStorage.out_index &= 15;
    }

#ifdef USE_PICTURE_DISCARD
    if (dec_cont->StrmStorage.p_pic_buf[pic_index].first_show)
#endif
    {
      /* wait this buffer as unused */
      if(BqueueWaitBufNotInUse(&dec_cont->StrmStorage.bq, pic_index) != HANTRO_OK)
        return AVSDEC_ABORTED;
      if(dec_cont->pp_enabled) {
        InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data->virtual_address);
      }

      //dec_cont->StrmStorage.p_pic_buf[pic_index].notDisplayed = 1;

      /* set this buffer as used */
      if((!dec_cont->ApiStorage.output_other_field &&
          picture->interlaced) || !picture->interlaced) {
        BqueueSetBufferAsUsed(&dec_cont->StrmStorage.bq, pic_index);
        dec_cont->StrmStorage.p_pic_buf[pic_index].first_show = 0;
        if(dec_cont->pp_enabled)
          InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data->virtual_address);
      }

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

    Function name: AvsDecPictureConsumed

    Functional description:
        release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        AVSDEC_PARAM_ERROR         Decoder instance or picture is null
        AVSDEC_NOT_INITIALIZED     Decoder instance isn't initialized
        AVSDEC_OK                  picture release success
------------------------------------------------------------------------------*/
AvsDecRet AvsDecPictureConsumed(AvsDecInst dec_inst, AvsDecPicture * picture) {
  /* Variables */
  DecContainer *dec_cont;
  u32 i;
  u8 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */
  AVS_API_TRC("\nAvs_dec_picture_consumed#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    AVS_API_TRC("AvsDecPictureConsumed# ERROR: picture is NULL");
    return (AVSDEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecPictureConsumed# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address == dec_cont->StrmStorage.p_pic_buf[i].data.bus_address
          && (addr_t)picture->pictures[0].output_picture
          == (addr_t)dec_cont->StrmStorage.p_pic_buf[i].data.virtual_address) {
        BqueuePictureRelease(&dec_cont->StrmStorage.bq, i);
        return (AVSDEC_OK);
      }
    }
  } else {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = picture->pictures[i].output_picture;
        break;
      }
    }
    InputQueueReturnBuffer(dec_cont->pp_buffer_queue, (const u32 *)output_picture);
    return (AVSDEC_OK);
  }
  return (AVSDEC_PARAM_ERROR);
}

AvsDecRet AvsDecEndOfStream(AvsDecInst dec_inst, u32 strm_end_flag) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;
  AvsDecPicture output;
  AvsDecRet ret;

  AVS_API_TRC("AvsDecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecPictureConsumed# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }
  pthread_mutex_lock(&dec_cont->protect_mutex);
  if(dec_cont->dec_stat == AVSDEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (AVSDEC_OK);
  }

  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->avs_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  } else if (dec_cont->keep_hw_reserved) {
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->keep_hw_reserved = 0;
  }

  while((ret = AvsDecNextPicture_INTERNAL(dec_inst, &output, 1)) == AVSDEC_PIC_RDY);
  if(ret == AVSDEC_ABORTED) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (AVSDEC_ABORTED);
  }

  if(strm_end_flag) {
    dec_cont->dec_stat = AVSDEC_END_OF_STREAM;
    FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);
  }

  /* Wait all buffers as unused */
  if(!strm_end_flag)
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);

  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  AVS_API_TRC("AvsDecEndOfStream# AVSDEC_OK\n");
  return (AVSDEC_OK);
}


/*----------------------=-------------------------------------------------------

    Function name: AvsFillPicStruct

    Functional description:
        Fill data to output pic description

    Input:
        dec_cont    Decoder container
        picture    Pointer to return value struct

    Return values:
        void

------------------------------------------------------------------------------*/
static void AvsFillPicStruct(AvsDecPicture * picture,
                             DecContainer * dec_cont, u32 pic_index) {
  picture_t *p_pic;

  p_pic = (picture_t *) dec_cont->StrmStorage.p_pic_buf;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = p_pic[pic_index].frame_width << 4;
    picture->pictures[0].frame_height = p_pic[pic_index].frame_height << 4;
    picture->pictures[0].coded_width = p_pic[pic_index].Hdrs.horizontal_size;
    picture->pictures[0].coded_height = p_pic[pic_index].Hdrs.vertical_size;
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
  picture->interlaced = !p_pic[pic_index].Hdrs.progressive_sequence;

  picture->key_picture = p_pic[pic_index].pic_type;
  picture->pic_id = p_pic[pic_index].pic_id;
  picture->decode_id = p_pic[pic_index].pic_id;
  picture->pic_coding_type = p_pic[pic_index].pic_code_type;

  /* handle first field indication */
  if(!p_pic[pic_index].Hdrs.progressive_sequence) {
    if(dec_cont->StrmStorage.field_out_index)
      dec_cont->StrmStorage.field_out_index = 0;
    else
      dec_cont->StrmStorage.field_out_index = 1;
  }

  picture->first_field = p_pic[pic_index].ff[dec_cont->StrmStorage.field_out_index];
  picture->repeat_first_field = p_pic[pic_index].rff;
  picture->repeat_frame_count = p_pic[pic_index].rfc;
  picture->number_of_err_mbs = p_pic[pic_index].nbr_err_mbs;
  (void) DWLmemcpy(&picture->time_code,
                   &p_pic[pic_index].time_code, sizeof(AvsDecTime));
}

/*------------------------------------------------------------------------------

    Function name: AvsSetRegs

    Functional description:
        Set registers

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
static u32 AvsSetRegs(DecContainer * dec_cont, addr_t strm_bus_address) {
  addr_t tmp = 0;
  u32 tmp_fwd, tmp_curr;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  struct DecHwFeatures hw_feature;
  addr_t mask;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

#ifdef _DEC_PP_USAGE
  AvsDecPpUsagePrint(dec_cont, DECPP_UNSPECIFIED,
                     dec_cont->StrmStorage.work_out, 1,
                     dec_cont->StrmStorage.latest_id);
#endif

  if (hw_feature.g1_strm_128bit_align)
    mask = 15;
  else
    mask = 7;

  /*
  if(!dec_cont->Hdrs.progressive_sequence)
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_TILED_E, 0);
      */

  AVSDEC_DEBUG(("Decoding to index %d \n",
                dec_cont->StrmStorage.work_out));

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTERLACE_E, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIELDMODE_E, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_TOPFIELD_E, 0);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTERLACE_E, 1);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIELDMODE_E, 1);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_TOPFIELD_E,
                   dec_cont->ApiStorage.first_field);
  }

  if (!hw_feature.pic_size_reg_unified) {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_MB_HEIGHT_P,
                   dec_cont->StrmStorage.frame_height);
    SetDecRegister(dec_cont->avs_regs, HWIF_AVS_H264_H_EXT,
                   dec_cont->StrmStorage.frame_height >> 8);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_HEIGHT_IN_CBS,
                   dec_cont->StrmStorage.frame_height << 1);
  }

  if(dec_cont->Hdrs.pic_coding_type == BFRAME)
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_B_E, 1);
  else
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_B_E, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTER_E,
                 dec_cont->Hdrs.pic_coding_type != IFRAME);

  /* tmp is strm_bus_address + number of bytes decoded by SW */
  tmp = dec_cont->StrmDesc.strm_curr_pos -
        dec_cont->StrmDesc.p_strm_buff_start;
  tmp = strm_bus_address + tmp;

  /* bus address must not be zero */
  if(!(tmp & ~0x7)) {
    return 0;
  }

  /* pointer to start of the stream, mask to get the pointer to
   * previous 64/128-bit aligned position */
  SET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

  /* amount of stream (as seen by the HW), obtained as amount of
   * stream given by the application subtracted by number of bytes
   * decoded by SW (if strm_bus_address is not 64/128-bit aligned -> adds
   * number of bytes from previous 64/128-bit aligned boundary) */
  SetDecRegister(dec_cont->avs_regs, HWIF_STREAM_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));

  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));
  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_OFFSET, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_BIT,
                 dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIXED_QUANT,
                 dec_cont->Hdrs.fixed_picture_qp);
  SetDecRegister(dec_cont->avs_regs, HWIF_INIT_QP,
                 dec_cont->Hdrs.picture_qp);

  /* AVS Plus stuff */
  SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_E,
                 dec_cont->Hdrs.weighting_quant_flag);
  SetDecRegister(dec_cont->avs_regs, HWIF_AVS_AEC_E,
                 dec_cont->Hdrs.aec_enable);
  SetDecRegister(dec_cont->avs_regs, HWIF_NO_FWD_REF_E,
                 dec_cont->Hdrs.no_forward_reference_flag);
  SetDecRegister(dec_cont->avs_regs, HWIF_PB_FIELD_ENHANCED_E,
                 dec_cont->Hdrs.pb_field_enhanced_flag);

  if (dec_cont->Hdrs.profile_id == 0x48) {
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_AVSP_ENA, 1);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_AVSP_ENA, 0);
  }

  if(dec_cont->Hdrs.weighting_quant_flag == 1 &&
      dec_cont->Hdrs.chroma_quant_param_disable == 0x0) {
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CB,
                   dec_cont->Hdrs.chroma_quant_param_delta_cb);
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CR,
                   dec_cont->Hdrs.chroma_quant_param_delta_cr);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CB, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CR, 0);
  }

  if(dec_cont->Hdrs.weighting_quant_flag == 1) {
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_MODEL,
                   dec_cont->Hdrs.weighting_quant_model);

    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_0,
                   dec_cont->Hdrs.weighting_quant_param[0]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_1,
                   dec_cont->Hdrs.weighting_quant_param[1]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_2,
                   dec_cont->Hdrs.weighting_quant_param[2]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_3,
                   dec_cont->Hdrs.weighting_quant_param[3]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_4,
                   dec_cont->Hdrs.weighting_quant_param[4]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_5,
                   dec_cont->Hdrs.weighting_quant_param[5]);
  }
  /* AVS Plus end */

  if (dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
      dec_cont->ApiStorage.first_field) {
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                 dec_cont->StrmStorage.p_pic_buf[(i32)dec_cont->
                     StrmStorage.work_out].
                 data.bus_address);
  } else {

    /* start of bottom field line */
    if(dec_cont->dpb_mode == DEC_DPB_FRAME ) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                   (dec_cont->StrmStorage.p_pic_buf[(i32)dec_cont->
                       StrmStorage.work_out].
                    data.bus_address +
                    ((dec_cont->StrmStorage.frame_width << 4))));
    } else if( dec_cont->dpb_mode == DEC_DPB_INTERLACED_FIELD ) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                   dec_cont->StrmStorage.p_pic_buf[(i32)dec_cont->
                       StrmStorage.work_out].
                   data.bus_address);
    }
  }
  SetDecRegister(dec_cont->avs_regs, HWIF_PP_CR_FIRST, dec_cont->cr_first);
  SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled );
  if (dec_cont->pp_enabled &&
      hw_feature.pp_support && hw_feature.pp_version) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    addr_t ppu_out_bus_addr = dec_cont->StrmStorage.p_pic_buf[dec_cont->
                              StrmStorage.work_out].pp_data->bus_address;
    u32 bottom_flag = !(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
                        dec_cont->ApiStorage.first_field);
    PPSetRegs(dec_cont->avs_regs, &hw_feature, ppu_cfg, ppu_out_bus_addr,
              0, bottom_flag);
#if 0
    u32 ppw, pph;
    u32 pp_field_offset = 0;
#define TOFIX(d, q) ((u32)( (d) * (u32)(1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))
    if (hw_feature.pp_version == FIXED_DS_PP) {
        /* G1V8_1 only supports fixed ratio (1/2/4/8) */
      ppw = NEXT_MULTIPLE((dec_cont->StrmStorage.frame_width * 16 >> dec_cont->dscale_shift_x), ALIGN(dec_cont->align));
      pph = (dec_cont->StrmStorage.frame_height * 16 >> dec_cont->dscale_shift_y);
      if (dec_cont->dscale_shift_x == 0) {
        SetDecRegister(dec_cont->avs_regs, HWIF_HOR_SCALE_MODE_U, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_WSCALE_INVRA_U, 0);
      } else {
        /* down scale */
        SetDecRegister(dec_cont->avs_regs, HWIF_HOR_SCALE_MODE_U, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_WSCALE_INVRA_U,
                       1<<(16-dec_cont->dscale_shift_x));
      }

      if (dec_cont->dscale_shift_y == 0) {
        SetDecRegister(dec_cont->avs_regs, HWIF_VER_SCALE_MODE_U, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_HSCALE_INVRA_U, 0);
      } else {
        /* down scale */
        SetDecRegister(dec_cont->avs_regs, HWIF_VER_SCALE_MODE_U, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_HSCALE_INVRA_U,
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
      SetDecRegister(dec_cont->avs_regs, HWIF_CROP_STARTX_U,
                     dec_cont->crop_startx >> 2);
      SetDecRegister(dec_cont->avs_regs, HWIF_CROP_STARTY_U,
                     dec_cont->crop_starty >> 2);
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_IN_WIDTH_U,
                     dec_cont->crop_width >> 2);
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_IN_HEIGHT_U,
                     dec_cont->crop_height >> 2);
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_WIDTH_U,
                     dec_cont->scaled_width);
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_HEIGHT_U,
                     dec_cont->scaled_height);

      if(in_width < out_width) {
        /* upscale */
        u32 W, inv_w;

        SetDecRegister(dec_cont->avs_regs, HWIF_HOR_SCALE_MODE_U, 1);

        W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
        inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

        SetDecRegister(dec_cont->avs_regs, HWIF_SCALE_WRATIO_U, W);
        SetDecRegister(dec_cont->avs_regs, HWIF_WSCALE_INVRA_U, inv_w);
      } else if(in_width > out_width) {
        /* downscale */
        u32 hnorm;

        SetDecRegister(dec_cont->avs_regs, HWIF_HOR_SCALE_MODE_U, 2);

        hnorm = FDIVI(TOFIX(out_width, 16), in_width);
        SetDecRegister(dec_cont->avs_regs, HWIF_WSCALE_INVRA_U, hnorm);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_WSCALE_INVRA_U, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_HOR_SCALE_MODE_U, 0);
      }

      if(in_height < out_height) {
        /* upscale */
        u32 H, inv_h;

        SetDecRegister(dec_cont->avs_regs, HWIF_VER_SCALE_MODE_U, 1);

        H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

        SetDecRegister(dec_cont->avs_regs, HWIF_SCALE_HRATIO_U, H);

        inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

        SetDecRegister(dec_cont->avs_regs, HWIF_HSCALE_INVRA_U, inv_h);
      } else if(in_height > out_height) {
        /* downscale */
        u32 Cv;

        Cv = FDIVI(TOFIX(out_height, 16), in_height) + 1;

        SetDecRegister(dec_cont->avs_regs, HWIF_VER_SCALE_MODE_U, 2);

        SetDecRegister(dec_cont->avs_regs, HWIF_HSCALE_INVRA_U, Cv);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_HSCALE_INVRA_U, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_VER_SCALE_MODE_U, 0);
      }
    }
    if (!(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
        dec_cont->ApiStorage.first_field) &&
        dec_cont->dpb_mode == DEC_DPB_FRAME){
      pp_field_offset = ppw;
    }
    if (hw_feature.pp_stride_support) {
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_Y_STRIDE, ppw);
      SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_C_STRIDE, ppw);
    }
    SET_ADDR64_REG(dec_cont->avs_regs, HWIF_PP_OUT_LU_BASE_U,
                   dec_cont->StrmStorage.p_pic_buf[dec_cont->
                       StrmStorage.work_out].
                   pp_data->bus_address + pp_field_offset);
    SET_ADDR64_REG(dec_cont->avs_regs, HWIF_PP_OUT_CH_BASE_U,
                   dec_cont->StrmStorage.p_pic_buf[dec_cont->
                       StrmStorage.work_out].
                   pp_data->bus_address + ppw * pph + pp_field_offset);
#endif
    SetPpRegister(dec_cont->avs_regs, HWIF_PP_IN_FORMAT_U, 1);
  }
  if (hw_feature.dec_stride_support) {
    /* Stride registers only available since g1v8_2 */
    if (dec_cont->tiled_stride_enable) {
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_Y_STRIDE,
                     NEXT_MULTIPLE(dec_cont->StrmStorage.frame_width * 4 * 16, ALIGN(dec_cont->align)));
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_C_STRIDE,
                     NEXT_MULTIPLE(dec_cont->StrmStorage.frame_width * 4 * 16, ALIGN(dec_cont->align)));
    } else {
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_Y_STRIDE,
                     dec_cont->StrmStorage.frame_width * 64);
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_C_STRIDE,
                     dec_cont->StrmStorage.frame_width * 64);
    }
  }
  SetDecRegister( dec_cont->avs_regs, HWIF_DPB_ILACE_MODE,
                  dec_cont->dpb_mode );

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    if(dec_cont->Hdrs.pic_coding_type == BFRAME) {
      /* past anchor set to future anchor if past is invalid (second
       * picture in sequence is B) */
      tmp_fwd =
        dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
        dec_cont->StrmStorage.work1 :
        dec_cont->StrmStorage.work0;

      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE,
                   dec_cont->StrmStorage.p_pic_buf[(i32)tmp_fwd].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE,
                   dec_cont->StrmStorage.p_pic_buf[(i32)tmp_fwd].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);

      /* block distances */

      /* current to future anchor */
      tmp = (2*dec_cont->StrmStorage.
             p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
             2*dec_cont->Hdrs.picture_distance + 512) & 0x1FF;
      /* prevent division by zero */
      if (!tmp) tmp = 2;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                     512/tmp);

      /* current to past anchor */
      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1,
                     512/tmp);

      /* future anchor to past anchor */
      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               + 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      tmp = 16384/tmp;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, tmp);

      /* future anchor to previous past anchor */
      tmp = dec_cont->StrmStorage.future2prev_past_dist;
      tmp = 16384/tmp;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, tmp);
    } else {
      tmp_fwd =
        dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
        dec_cont->StrmStorage.work1 :
        dec_cont->StrmStorage.work0;

      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)tmp_fwd].data.bus_address);

      /* current to past anchor */
      tmp = (2*dec_cont->Hdrs.picture_distance -
             2*dec_cont->StrmStorage.
             p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
             512) & 0x1FF;
      if (!tmp) tmp = 2;

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1,
                     512/tmp);
      /* current to previous past anchor */
      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               512) & 0x1FF;
        if (!tmp) tmp = 2;
      }

      /* this will become "future to previous past" for next B */
      dec_cont->StrmStorage.future2prev_past_dist = tmp;

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 0);
    }
    /* AVS Plus stuff */
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
    /* AVS Plus end */
  } else { /* field interlaced */
    if(dec_cont->Hdrs.pic_coding_type == BFRAME) {
      /* past anchor set to future anchor if past is invalid (second
       * picture in sequence is B) */
      tmp_fwd =
        dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
        dec_cont->StrmStorage.work1 :
        dec_cont->StrmStorage.work0;

      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE,
                   dec_cont->StrmStorage.p_pic_buf[(i32)tmp_fwd].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE,
                   dec_cont->StrmStorage.p_pic_buf[(i32)tmp_fwd].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE,
                   dec_cont->StrmStorage.
                   p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                   bus_address);

      /* block distances */
      tmp = (2*dec_cont->StrmStorage.
             p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
             2*dec_cont->Hdrs.picture_distance + 512) & 0x1FF;
      /* prevent division by zero */
      if (!tmp) tmp = 2;
      if (dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3,
                       tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                       512/tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                       512/(tmp+1));
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2,
                       tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                       512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                       512/tmp);
      }

      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      if (dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0,
                       tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                       512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1,
                       512/tmp);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1,
                       tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                       512/tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1,
                       512/(tmp+1));
      }

      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               + 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      /* AVS Plus stuff */
      if (dec_cont->Hdrs.pb_field_enhanced_flag &&
          !dec_cont->ApiStorage.first_field) {
        /* in this case, BlockDistanceRef is different with before, the mvRef points to top field */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0,
                       16384/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1,
                       16384/tmp);

        /* future anchor to previous past anchor */
        tmp = dec_cont->StrmStorage.future2prev_past_dist;

        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2,
                       16384/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3,
                       16384/tmp);
      } else {
        if (dec_cont->ApiStorage.first_field) {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0,
                         16384/(tmp-1));
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1,
                         16384/tmp);
        } else {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0,
                         16384/1);
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1,
                         16384/tmp);
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2,
                         16384/(tmp+1));
        }

        /* future anchor to previous past anchor */
        tmp = dec_cont->StrmStorage.future2prev_past_dist;

        if( dec_cont->ApiStorage.first_field) {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2,
                         16384/(tmp-1));
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3,
                         16384/tmp);
        } else
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3,
                         16384/tmp);
      }

      if(dec_cont->ApiStorage.first_field) {
        /* 1 means delta=2, 3 means delta=-2, 0 means delta=0 */
        /* delta1 */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

        /* deltaFw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
        /* deltaBw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
      } else {
        /* delta1 */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

        /* deltaFw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
        /* deltaBw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2,
                       (u32)-2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3,
                       (u32)-2);
      }
      /* AVS Plus end */
    } else {
      tmp_fwd =
        dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
        dec_cont->StrmStorage.work1 :
        dec_cont->StrmStorage.work0;

      tmp_curr = dec_cont->StrmStorage.work_out;
      /* past anchor not available -> use current (this results in using
       * the same top or bottom field as reference and output picture
       * base, utput is probably corrupted) */
      if(tmp_fwd == INVALID_ANCHOR_PICTURE)
        tmp_fwd = tmp_curr;

      if(dec_cont->ApiStorage.first_field) {
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                     bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                     bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)tmp_fwd].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)tmp_fwd].data.bus_address);

      } else {
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE,
                     dec_cont->StrmStorage.p_pic_buf[(i32)tmp_curr].data.
                     bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                     bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE,
                     dec_cont->StrmStorage.
                     p_pic_buf[(i32)dec_cont->StrmStorage.work0].data.
                     bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE,
                     dec_cont->StrmStorage.p_pic_buf[(i32)tmp_fwd].data.
                     bus_address);
      }

      tmp = (2*dec_cont->Hdrs.picture_distance -
             2*dec_cont->StrmStorage.
             p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
             512) & 0x1FF;
      if (!tmp) tmp = 2;

      if(!dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, 1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2,
                       tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                       512/1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                       512/(tmp+1));
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0,
                       tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0,
                       512/(tmp-1));
      }
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1,
                     512/tmp);

      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*dec_cont->StrmStorage.
               p_pic_buf[(i32)dec_cont->StrmStorage.work1].picture_distance -
               512) & 0x1FF;
        if (!tmp) tmp = 2;
      }

      /* this will become "future to previous past" for next B */
      dec_cont->StrmStorage.future2prev_past_dist = tmp;

      if(dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2,
                       tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                       512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                       512/tmp);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3,
                       tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                       512/tmp);
      }

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 0);

      /* AVS Plus stuff */
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
      /* AVS Plus end */
    }

  }

  SetDecRegister(dec_cont->avs_regs, HWIF_STARTMB_X, 0);
  SetDecRegister(dec_cont->avs_regs, HWIF_STARTMB_Y, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_FILTERING_DIS,
                 dec_cont->Hdrs.loop_filter_disable);
  SetDecRegister(dec_cont->avs_regs, HWIF_ALPHA_OFFSET,
                 dec_cont->Hdrs.alpha_offset);
  SetDecRegister(dec_cont->avs_regs, HWIF_BETA_OFFSET,
                 dec_cont->Hdrs.beta_offset);
  SetDecRegister(dec_cont->avs_regs, HWIF_SKIP_MODE,
                 dec_cont->Hdrs.skip_mode_flag);

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_REFER_FLAG,
                 dec_cont->Hdrs.picture_reference_flag);

  if (dec_cont->Hdrs.pic_coding_type == PFRAME ||
      (dec_cont->Hdrs.pic_coding_type == IFRAME /*&&
         !dec_cont->ApiStorage.first_field*/)) { /* AVS Plus change */
    SetDecRegister(dec_cont->avs_regs, HWIF_WRITE_MVS_E, 1);
  } else
    SetDecRegister(dec_cont->avs_regs, HWIF_WRITE_MVS_E, 0);

  if (dec_cont->ApiStorage.first_field ||
      ( dec_cont->Hdrs.pic_coding_type == BFRAME &&
        dec_cont->StrmStorage.prev_pic_structure ))
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE,
                 dec_cont->StrmStorage.direct_mvs.bus_address);
  else
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE,
                 dec_cont->StrmStorage.direct_mvs.bus_address +
                     NEXT_MULTIPLE(((( dec_cont->StrmStorage.frame_width *
                         dec_cont->StrmStorage.frame_height/2 + 1) & ~0x1) *
                         4 * sizeof(u32)), MAX(16, ALIGN(dec_cont->align))));
  /* AVS Plus stuff */
  SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE2,
               dec_cont->StrmStorage.direct_mvs.bus_address);
  /* AVS Plus end */
  SetDecRegister(dec_cont->avs_regs, HWIF_PREV_ANC_TYPE,
                 !dec_cont->StrmStorage.p_pic_buf[(i32)dec_cont->StrmStorage.work0].
                 pic_type ||
                 (!dec_cont->ApiStorage.first_field &&
                  dec_cont->StrmStorage.prev_pic_structure == 0));

  /* b-picture needs to know if future reference is field or frame coded */
  SetDecRegister(dec_cont->avs_regs, HWIF_REFER2_FIELD_E,
                 dec_cont->StrmStorage.prev_pic_structure == 0);
  SetDecRegister(dec_cont->avs_regs, HWIF_REFER3_FIELD_E,
                 dec_cont->StrmStorage.prev_pic_structure == 0);

  /* Setup reference picture buffer */
  if( dec_cont->ref_buf_support )
    RefbuSetup(&dec_cont->ref_buffer_ctrl, dec_cont->avs_regs,
               dec_cont->Hdrs.picture_structure == FIELDPICTURE ?
               REFBU_FIELD : REFBU_FRAME,
               dec_cont->Hdrs.pic_coding_type == IFRAME,
               dec_cont->Hdrs.pic_coding_type == BFRAME, 0, 2,
               0 );

  if( dec_cont->tiled_mode_support) {
    dec_cont->tiled_reference_enable =
      DecSetupTiledReference( dec_cont->avs_regs,
                              dec_cont->tiled_mode_support,
                              dec_cont->dpb_mode,
                              !dec_cont->Hdrs.progressive_sequence );
  } else {
    dec_cont->tiled_reference_enable = 0;
  }

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: AvsCheckFormatSupport

    Functional description:
        Check if avs supported

    Input:
        container

    Return values:
        return zero for OK

------------------------------------------------------------------------------*/
u32 AvsCheckFormatSupport(void) {
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  return (hw_feature.avs_support == AVS_NOT_SUPPORTED);
}

/*------------------------------------------------------------------------------

    Function name: AvsDecPeek

    Functional description:
        Retrieve last decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct

    Output:
        picture Decoder output picture.

    Return values:
        AVSDEC_OK         No picture available.
        AVSDEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
AvsDecRet AvsDecPeek(AvsDecInst dec_inst, AvsDecPicture * picture) {
  /* Variables */

  DecContainer *dec_cont;
  u32 pic_index;
  picture_t *p_pic;

  /* Code */

  AVS_API_TRC("\nAvs_dec_peek#");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    AVS_API_TRC("AvsDecPeek# ERROR: picture is NULL");
    return (AVSDEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecPeek# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }

  /* when output release thread enabled, AvsDecNextPicture_INTERNAL() called in
     AvsDecDecode(), and "dec_cont->StrmStorage.out_count--" may called in
     AvsDecNextPicture() before AvsDecPeek() called, so dec_cont->fullness
     used to sample the real out_count in case of AvsDecNextPicture_INTERNAL() called
     before than AvsDecPeek() */
  u32 tmp = dec_cont->fullness;

  if(!tmp ||
      dec_cont->StrmStorage.new_headers_change_resolution) {
    (void) DWLmemset(picture, 0, sizeof(AvsDecPicture));
    return AVSDEC_OK;
  }

  pic_index = dec_cont->StrmStorage.work_out;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = dec_cont->StrmStorage.frame_width << 4;
    picture->pictures[0].frame_height = dec_cont->StrmStorage.frame_height << 4;
    picture->pictures[0].coded_width = dec_cont->Hdrs.horizontal_size;
    picture->pictures[0].coded_height = dec_cont->Hdrs.vertical_size;
  } else {
    picture->pictures[0].frame_width = (dec_cont->StrmStorage.frame_width << 4) >> dec_cont->dscale_shift_x;
    picture->pictures[0].frame_height = (dec_cont->StrmStorage.frame_height << 4) >> dec_cont->dscale_shift_y;
    picture->pictures[0].coded_width = dec_cont->Hdrs.horizontal_size >> dec_cont->dscale_shift_x;
    picture->pictures[0].coded_height = dec_cont->Hdrs.vertical_size >> dec_cont->dscale_shift_y;
  }

  picture->interlaced = !dec_cont->Hdrs.progressive_sequence;

  p_pic = dec_cont->StrmStorage.p_pic_buf + pic_index;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].output_picture = (u8 *) p_pic->data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->data.bus_address;
  } else {
    picture->pictures[0].output_picture = (u8 *) p_pic->pp_data->virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->pp_data->bus_address;
  }
  picture->key_picture = p_pic->pic_type;
  picture->pic_id = p_pic->pic_id;
  picture->decode_id = p_pic->pic_id;
  picture->pic_coding_type = p_pic->pic_code_type;

  picture->repeat_first_field = p_pic->rff;
  picture->repeat_frame_count = p_pic->rfc;
  picture->number_of_err_mbs = p_pic->nbr_err_mbs;
  picture->pictures[0].output_format = p_pic->tiled_mode ?
                           DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;

  (void) DWLmemcpy(&picture->time_code,
                   &p_pic->time_code, sizeof(AvsDecTime));

  /* frame output */
  picture->field_picture = 0;
  picture->top_field = 0;
  picture->first_field = 0;

  return AVSDEC_PIC_RDY;
}

void AvsSetExternalBufferInfo(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *)dec_inst;
  u32 ext_buffer_size;

  ext_buffer_size = AvsGetRefFrmSize(dec_cont);

  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }

  dec_cont->ext_min_buffer_num = dec_cont->buf_num =  buffers;
  dec_cont->next_buf_size = ext_buffer_size;
}

AvsDecRet AvsDecGetBufferInfo(AvsDecInst dec_inst, AvsDecBufferInfo *mem_info) {
  DecContainer  * dec_cont = (DecContainer *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;

  if(dec_cont == NULL || mem_info == NULL) {
    return AVSDEC_PARAM_ERROR;
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
        return (AVSDEC_MEMFAIL);
      }
      dec_cont->StrmStorage.ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return AVSDEC_OK;
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return AVSDEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return AVSDEC_OK;
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

  return AVSDEC_WAITING_FOR_BUFFER;
}

AvsDecRet AvsDecAddBuffer(AvsDecInst dec_inst, struct DWLLinearMem *info) {
  DecContainer *dec_cont = (DecContainer *)dec_inst;
  AvsDecRet dec_ret = AVSDEC_OK;
  u32 i = dec_cont->buffer_index;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return AVSDEC_PARAM_ERROR;
  }

  if (dec_cont->buffer_index >= 16)
    /* Too much buffers added. */
    return AVSDEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;


  /* buffer is not enough, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = AVSDEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    dec_cont->StrmStorage.p_pic_buf[i].data = *info;
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->StrmStorage.p_pic_buf[i].data = *info;
      dec_cont->buffer_index++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->StrmStorage.ext_buffer_added = 1;
  return dec_ret;
}


void AvsEnterAbortState(DecContainer *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void AvsExistAbortState(DecContainer *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void AvsEmptyBufferQueue(DecContainer *dec_cont) {
  BqueueEmpty(&dec_cont->StrmStorage.bq);
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

void AvsStateReset(DecContainer *dec_cont) {
  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  /* Clear internal parameters in DecContainer */
  dec_cont->keep_hw_reserved = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
  dec_cont->fullness = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->fifo_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->dec_stat = AVSDEC_OK;

  /* Clear internal parameters in DecStrmStorage */
  dec_cont->StrmStorage.valid_pic_header = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->StrmStorage.strm_dec_ready = 0;
  dec_cont->StrmStorage.valid_sequence = 0;
#endif
  dec_cont->StrmStorage.out_index = 0;
  dec_cont->StrmStorage.out_count = 0;
  dec_cont->StrmStorage.skip_b = 0;
  dec_cont->StrmStorage.prev_pic_coding_type = 0;
  dec_cont->StrmStorage.prev_pic_structure = 0;
  dec_cont->StrmStorage.field_out_index = 1;
  dec_cont->StrmStorage.frame_number = 0;
  dec_cont->StrmStorage.picture_broken = 0;
  dec_cont->StrmStorage.unsupported_features_present = 0;
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.ext_buffer_added = 0;
  dec_cont->StrmStorage.sequence_low_delay = 0;
  dec_cont->StrmStorage.new_headers_change_resolution = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->StrmStorage.bq.queue_size = buffers;
  dec_cont->StrmStorage.num_buffers = buffers;
#endif
  dec_cont->StrmStorage.future2prev_past_dist = 0;

  /* Clear internal parameters in DecApiStorage */
  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;
  dec_cont->ApiStorage.output_other_field = 0;

  (void) DWLmemset(&(dec_cont->StrmDesc), 0, sizeof(DecStrmDesc));
  (void) DWLmemset(&(dec_cont->out_data), 0, sizeof(AvsDecOutput));
  (void) DWLmemset(dec_cont->StrmStorage.out_buf, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
   if (!dec_cont->pp_enabled)
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf, 0, 16 * sizeof(picture_t));
  (void) DWLmemset(dec_cont->StrmStorage.picture_info, 0, 32 * sizeof(AvsDecPicture));
#endif
#ifdef CLEAR_HDRINFO_IN_SEEK
  (void) DWLmemset(&(dec_cont->Hdrs), 0, sizeof(DecHdrs));
  (void) DWLmemset(&(dec_cont->tmp_hdrs), 0, sizeof(DecHdrs));
  AvsAPI_InitDataStructures(dec_cont);
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

AvsDecRet AvsDecAbort(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;

  AVS_API_TRC("AvsDecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecAbort# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  AvsEnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (AVSDEC_OK);
}

AvsDecRet AvsDecAbortAfter(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;

  AVS_API_TRC("AvsDecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    AVS_API_TRC("AvsDecAbortAfter# ERROR: Decoder not initialized");
    return (AVSDEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_stat == AVSDEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (AVSDEC_OK);
  }
#endif

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->avs_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  } else if(dec_cont->keep_hw_reserved) {
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->keep_hw_reserved = 0;
  }

  /* Clear any remaining pictures from DPB */
  AvsEmptyBufferQueue(dec_cont);

  AvsStateReset(dec_cont);

  AvsExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  AVS_API_TRC("AvsDecAbortAfter# AVSDEC_OK\n");
  return (AVSDEC_OK);
}

AvsDecRet AvsDecSetInfo(AvsDecInst dec_inst,
                        struct AvsDecConfig *dec_cfg) {
  /*@null@ */ DecContainer *dec_cont = (DecContainer *)dec_inst;
  u32 pic_width = dec_cont->StrmStorage.frame_width << 4;
  u32 pic_height = dec_cont->StrmStorage.frame_height << 4;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  struct DecHwFeatures hw_feature;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, 8, dec_cont->Hdrs.progressive_sequence, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(AVSDEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, !dec_cont->Hdrs.progressive_sequence, dec_cont->ppu_cfg))
    return AVSDEC_PARAM_ERROR;
  if (!hw_feature.dec_stride_support) {
    /* For verion earlier than g1v8_2, reset alignment to 16B. */
    dec_cont->align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = dec_cfg->align;
  }
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
  dec_cont->ppu_cfg[0].pixel_width = dec_cont->ppu_cfg[1].pixel_width =
  dec_cont->ppu_cfg[2].pixel_width = dec_cont->ppu_cfg[3].pixel_width = 8;
  dec_cont->cr_first = dec_cfg->cr_first;
  dec_cont->pp_enabled = dec_cont->ppu_cfg[0].enabled |
                         dec_cont->ppu_cfg[1].enabled |
                         dec_cont->ppu_cfg[2].enabled |
                         dec_cont->ppu_cfg[3].enabled |
                         dec_cont->ppu_cfg[4].enabled;
#if 0
  dec_cont->crop_enabled = dec_cfg->crop.enabled;
  if (dec_cont->crop_enabled) {
    dec_cont->crop_startx = dec_cfg->crop.x;
    dec_cont->crop_starty = dec_cfg->crop.y;
    dec_cont->crop_width  = dec_cfg->crop.width;
    dec_cont->crop_height = dec_cfg->crop.height;
  } else {
    dec_cont->crop_startx = 0;
    dec_cont->crop_starty = 0;
    dec_cont->crop_width  = pic_width;
    dec_cont->crop_height = pic_height;
  }

  dec_cont->scale_enabled = dec_cfg->scale.enabled;
  if (dec_cont->scale_enabled) {
    dec_cont->scaled_width = dec_cfg->scale.width;
    dec_cont->scaled_height = dec_cfg->scale.height;
  } else {
    dec_cont->scaled_width = dec_cont->crop_width;
    dec_cont->scaled_height = dec_cont->crop_height;
  }
  if (dec_cont->crop_enabled || dec_cont->scale_enabled)
    dec_cont->pp_enabled = 1;
  else
    dec_cont->pp_enabled = 0;
  // x -> 1.5 ->  3  ->  6
  //    1  |  2   |  4   |  8
  if (dec_cont->scaled_width * 6 <= pic_width)
    dec_cont->dscale_shift_x = 3;
  else if (dec_cont->scaled_width * 3 <= pic_width)
    dec_cont->dscale_shift_x = 2;
  else if (dec_cont->scaled_width * 3 / 2 <= pic_width)
    dec_cont->dscale_shift_x = 1;
  else
    dec_cont->dscale_shift_x = 0;

  if (dec_cont->scaled_height * 6 <= pic_height)
    dec_cont->dscale_shift_y = 3;
  else if (dec_cont->scaled_height * 3 <= pic_height)
    dec_cont->dscale_shift_y = 2;
  else if (dec_cont->scaled_height * 3 / 2 <= pic_height)
    dec_cont->dscale_shift_y = 1;
  else
    dec_cont->dscale_shift_y = 0;
  if (hw_feature.pp_version == FIXED_DS_PP) {
    dec_cont->scaled_width = pic_width >> dec_cont->dscale_shift_x;
    dec_cont->scaled_height = pic_height >> dec_cont->dscale_shift_y;
  }
#endif
  return (AVSDEC_OK);
}

void AvsCheckBufferRealloc(DecContainer *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (AvsGetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
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
      if (AvsGetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (AvsGetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}

