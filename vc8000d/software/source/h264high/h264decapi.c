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

#include <string.h>
#include "basetype.h"
#include "version.h"
#include "h264hwd_container.h"
#include "h264decapi.h"
#include "h264hwd_decoder.h"
#include "h264hwd_util.h"
#include "h264hwd_exports.h"
#include "h264hwd_dpb.h"
#include "h264hwd_neighbour.h"
#include "h264hwd_asic.h"
#include "h264hwd_regdrv.h"
#include "h264hwd_byte_stream.h"
#include "deccfg.h"
#include "tiledref.h"
#include "workaround.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#include "dwl.h"
#include "h264hwd_dpb_lock.h"
#include "h264decmc_internals.h"

/*------------------------------------------------------------------------------
       Version Information - DO NOT CHANGE!
------------------------------------------------------------------------------*/

#define DEC_MAJOR_VERSION 1
#define DEC_MINOR_VERSION 1

/*
 * DEC_TRACE         Trace H264 Decoder API function calls. H264DecTrace
 *                       must be implemented externally.
 * DEC_EVALUATION    Compile evaluation version, restricts number of frames
 *                       that can be decoded
 */

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

#ifdef DEC_TRACE
#define DEC_API_TRC(str)    H264DecTrace(str)
#else
#define DEC_API_TRC(str)    do{}while(0)
#endif

#ifdef USE_RANDOM_TEST
#include "string.h"
#include "stream_corrupt.h"
#endif

static void h264UpdateAfterPictureDecode(struct H264DecContainer * dec_cont);
static u32 h264SpsSupported(const struct H264DecContainer * dec_cont);
static u32 h264PpsSupported(const struct H264DecContainer * dec_cont);
static u32 h264StreamIsBaseline(const struct H264DecContainer * dec_cont);

static u32 h264AllocateResources(struct H264DecContainer * dec_cont);
static void bsdDecodeReturn(u32 retval);
extern void h264InitPicFreezeOutput(struct H264DecContainer * dec_cont, u32 from_old_dpb);

static void h264GetSarInfo(const storage_t * storage,
                           u32 * sar_width, u32 * sar_height);
static void H264CycleCount(struct H264DecContainer * dec_cont);

extern void h264PreparePpRun(struct H264DecContainer * dec_cont);

static void h264CheckReleasePpAndHw(struct H264DecContainer *dec_cont);

static enum DecRet H264DecNextPicture_INTERNAL(H264DecInst dec_inst,
    H264DecPicture * output,
    u32 end_of_stream);

static void h264SetExternalBufferInfo(H264DecInst dec_inst, storage_t *storage) ;
static void h264SetMVCExternalBufferInfo(H264DecInst dec_inst, storage_t *storage);
static void h264EnterAbortState(struct H264DecContainer *dec_cont);
static void h264ExistAbortState(struct H264DecContainer *dec_cont);
static void h264StateReset(struct H264DecContainer *dec_cont);
static void h264CheckBufferRealloc(struct H264DecContainer *dec_cont);
#define DEC_DPB_NOT_INITIALIZED      -1

/* A global structure used to record stream information in low latency mode */
extern volatile struct strmInfo stream_info;

u32 H264DecMCGetCoreCount(void) {
  return DWLReadAsicCoreCount();
}

/*------------------------------------------------------------------------------

    Function: H264DecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance and calls h264bsdInit to initialize the
            instance data.

        Inputs:
            dec_cfg->no_output_reordering
                                flag to indicate decoder that it doesn't have
                                to try to provide output pictures in display
                                order, saves memory
            dec_cfg->error_handling
                                Flag to determine which error concealment
                                method to use.
            dec_cfg->use_display_smoothing
                                flag to enable extra buffering in DPB output
                                so that application may read output pictures
                                one by one
        Outputs:
            dec_inst             pointer to initialized instance is stored here

        Returns:
            DEC_OK        successfully initialized the instance
            DEC_INITFAIL  initialization failed
            DEC_PARAM_ERROR invalid parameters
            DEC_MEMFAIL   memory allocation failed
            DEC_DWL_ERROR error initializing the system interface
------------------------------------------------------------------------------*/

enum DecRet H264DecInit(H264DecInst * dec_inst,
                       const void *dwl,
                       struct H264DecConfig *dec_cfg) {

  /*@null@ */ struct H264DecContainer *dec_cont;

  DWLHwConfig hw_cfg;
  u32 asic_id, i, tmp, hw_build_id;
  u32 reference_frame_format;
  u32 low_latency_sim = 0;
  u32 secure = 0;
  struct DecHwFeatures hw_feature;

  DEC_API_TRC("H264DecInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecInit# ERROR: dec_inst == NULL");
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;   /* return NULL instance for any error */

  /* check for proper hardware */
  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_H264_DEC);

  if ((asic_id >> 16) != 0x6731U &&
      (asic_id >> 16) != 0x8001U) {
    DEC_API_TRC("H264DecInit# ERROR: HW not recognized/unsupported!\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  /* check that H.264 decoding supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_H264_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  if(!hw_feature.h264_support && !hw_feature.h264_high10_support) {
    DEC_API_TRC("H264DecInit# ERROR: H264 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if (!hw_feature.ring_buffer_support && dec_cfg->use_ringbuffer) {
    return DEC_PARAM_ERROR;
  }

  if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
    DEC_API_TRC("H264DecInit# ERROR: HW not support 64bit address!\n");
    return (DEC_PARAM_ERROR);
  }

  if (!hw_feature.low_latency_mode_support &&
      (dec_cfg->decoder_mode == DEC_LOW_LATENCY ||
       dec_cfg->decoder_mode == DEC_LOW_LATENCY_RTL)) {
    /* low lantency mode is NOT supported */
    return (DEC_PARAM_ERROR);
  }

  if (hw_feature.ref_frame_tiled_only)
    dec_cfg->dpb_flags = DEC_REF_FRM_TILED_DEFAULT | DEC_DPB_ALLOW_FIELD_ORDERING;

  dec_cont = (struct H264DecContainer *) DWLmalloc(sizeof(struct H264DecContainer));

  if(dec_cont == NULL) {
    DEC_API_TRC("H264DecInit# ERROR: Memory allocation failed\n");
    return (DEC_MEMFAIL);
  }

  (void) DWLmemset(dec_cont, 0, sizeof(struct H264DecContainer));
  dec_cont->dwl = dwl;

  h264bsdInit(&dec_cont->storage, dec_cfg->no_output_reordering,
              dec_cfg->use_display_smoothing);

  dec_cont->dec_stat = DEC_INITIALIZED;

  dec_cont->h264_regs[0] = asic_id;

  if (!hw_feature.h264_high10_support) {
    if (hw_feature.is_legacy_dec_mode)
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MODE_G1V6, DEC_X170_MODE_H264);
    else
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MODE, DEC_X170_MODE_H264);
  } else
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MODE, DEC_X170_MODE_H264_HIGH10);

  dec_cont->high10p_mode = hw_feature.h264_high10_support;
  dec_cont->h264_high10p_support = hw_feature.h264_high10_support;

  if (dec_cfg->rlc_mode) {
    /* Force the decoder into RLC mode */
    dec_cont->force_rlc_mode = 1;
    dec_cont->rlc_mode = 1;
    dec_cont->try_vlc = 0;
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MODE, DEC_X170_MODE_H264);
  }

  SetCommonConfigRegs(dec_cont->h264_regs);

  /* Set prediction filter taps */
  SetDecRegister(dec_cont->h264_regs, HWIF_PRED_BC_TAP_0_0, 1);
  SetDecRegister(dec_cont->h264_regs, HWIF_PRED_BC_TAP_0_1, (u32)(-5));
  SetDecRegister(dec_cont->h264_regs, HWIF_PRED_BC_TAP_0_2, 20);
  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  if (dec_cfg->decoder_mode == DEC_LOW_LATENCY) {
    dec_cont->low_latency = 1;
    sem_init(&(dec_cont->updated_reg_sem), 0, 0);
  }
  if (dec_cfg->decoder_mode == DEC_LOW_LATENCY_RTL) {
    low_latency_sim = 1;
  } else if (dec_cfg->decoder_mode == DEC_SECURITY) {
    secure = 1;
  }

  if(dec_cont->low_latency || low_latency_sim) {
    SetDecRegister(dec_cont->h264_regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
  } else if (secure) {
    SetDecRegister(dec_cont->h264_regs, HWIF_DRM_E, 1);
  } else {
    SetDecRegister(dec_cont->h264_regs, HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->h264_regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }
  if (dec_cfg->decoder_mode == DEC_PARTIAL_DECODING)
    dec_cont->partial_decoding = 1;
  /* save HW version so we dont need to check it all the time when deciding the control stuff */
  dec_cont->is8190 = (asic_id >> 16) != 0x8170U ? 1 : 0;
  dec_cont->h264_profile_support = hw_feature.h264_support;

  if((asic_id >> 16)  == 0x8170U)
    dec_cfg->error_handling = 0;

  if (hw_feature.dec_stride_support || hw_feature.pp_stride_support ) {
    /* Aligned to 128 bytes since g1v8_2. */
    dec_cont->align = dec_cont->storage.align = DEC_ALIGN_128B;
    dec_cfg->dpb_flags = DEC_REF_FRM_TILED_DEFAULT | DEC_DPB_ALLOW_FIELD_ORDERING;
  } else {
    dec_cont->align = dec_cont->storage.align = DEC_ALIGN_16B;
  }
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  /* for VC8000D, the max suporrted stream length is 32bit, consider SW memory allocation capability,
     the limitation is set to 2^30 Bytes length */
  if (hw_feature.strm_len_32bits)
    dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VC8000D;
  else
    dec_cont->max_strm_len = DEC_X170_MAX_STREAM;

  /* save ref buffer support status */
  dec_cont->ref_buf_support = hw_feature.ref_buf_support;
  reference_frame_format = dec_cfg->dpb_flags & DEC_REF_FRM_FMT_MASK;
  if(reference_frame_format == DEC_REF_FRM_TILED_DEFAULT) {
    /* Assert support in HW before enabling.. */
    if(!hw_feature.tiled_mode_support) {
      return DEC_FORMAT_NOT_SUPPORTED;
    }
    dec_cont->tiled_mode_support = hw_feature.tiled_mode_support;
  } else
    dec_cont->tiled_mode_support = 0;

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
    return (DEC_PARAM_ERROR);
  } else {
    u32 scale_table[9] = {0, 0, 1, 0, 2, 0, 0, 0, 3};

    dec_cont->pp_enabled = 1;
    dec_cont->down_scale_x = dscale_cfg->down_scale_x;
    dec_cont->down_scale_y = dscale_cfg->down_scale_y;

    dec_cont->storage.pp_enabled = 1;
    dec_cont->storage.down_scale_x_shift = scale_table[dscale_cfg->down_scale_x];
    dec_cont->storage.down_scale_y_shift = scale_table[dscale_cfg->down_scale_y];
    dec_cont->dscale_shift_x = dec_cont->storage.down_scale_x_shift;
    dec_cont->dscale_shift_y = dec_cont->storage.down_scale_y_shift;
  }
#endif
  dec_cont->storage.release_buffer = 0;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    return (DEC_MEMFAIL);
  }
  dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->allow_dpb_field_ordering = 0;
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;
  if( dec_cfg->dpb_flags & DEC_DPB_ALLOW_FIELD_ORDERING ) {
    dec_cont->allow_dpb_field_ordering = hw_feature.field_dpb_support;
  }
  dec_cont->storage.intra_freeze = dec_cfg->error_handling == DEC_EC_VIDEO_FREEZE;
#ifndef _DISABLE_PIC_FREEZE
  if (dec_cfg->error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->storage.partial_freeze = 1;
  else if (dec_cfg->error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->storage.partial_freeze = 2;
#endif
  dec_cont->error_handling = dec_cfg->error_handling;
  dec_cont->use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->use_ringbuffer = dec_cfg->use_ringbuffer;
  dec_cont->storage.use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->storage.picture_broken = HANTRO_FALSE;

  /* max decodable picture width */
  dec_cont->max_dec_pic_width = hw_feature.h264_max_dec_pic_width;
  dec_cont->max_dec_pic_height = hw_feature.h264_max_dec_pic_height;

  //dec_cont->cr_first = cr_first;

  dec_cont->checksum = dec_cont;  /* save instance as a checksum */

#ifdef _ENABLE_2ND_CHROMA
  dec_cont->storage.enable2nd_chroma = 1;
#endif

  InitWorkarounds(DEC_X170_MODE_H264, &dec_cont->workarounds);
  if (dec_cont->workarounds.h264.frame_num)
    dec_cont->frame_num_mask = 0x1000;

  /*  default single core */
  dec_cont->n_cores = 1;

  /* Init frame buffer list */
  InitList(&dec_cont->fb_list);

  dec_cont->storage.dpbs[0]->fb_list = &dec_cont->fb_list;
  dec_cont->storage.dpbs[1]->fb_list = &dec_cont->fb_list;

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  dec_cont->secure_mode = dec_cfg->decoder_mode == DEC_SECURITY ? 1 : 0;
  if (dec_cont->secure_mode)
    dec_cont->ref_buf_support = 0;

#ifdef USE_RANDOM_TEST
  /*********************************************************/
  /** Developers can change below parameters to generate  **/
  /** different kinds of error stream.                    **/
  /*********************************************************/
  dec_cont->error_params.seed = 66;
  strcpy(dec_cont->error_params.truncate_stream_odds , "1 : 6");
  strcpy(dec_cont->error_params.swap_bit_odds, "1 : 100000");
  strcpy(dec_cont->error_params.packet_loss_odds, "1 : 6");
  /*********************************************************/

  if (strcmp(dec_cont->error_params.swap_bit_odds, "0") != 0)
    dec_cont->error_params.swap_bits_in_stream = 0;

  if (strcmp(dec_cont->error_params.packet_loss_odds, "0") != 0)
    dec_cont->error_params.lose_packets = 1;

  if (strcmp(dec_cont->error_params.truncate_stream_odds, "0") != 0)
    dec_cont->error_params.truncate_stream = 1;

  if (dec_cont->error_params.swap_bits_in_stream ||
      dec_cont->error_params.lose_packets ||
      dec_cont->error_params.truncate_stream) {
    dec_cont->error_params.random_error_enabled = 1;
    InitializeRandom(dec_cont->error_params.seed);
  }
#endif

#ifdef DUMP_INPUT_STREAM
  dec_cont->ferror_stream = fopen("random_error.h264", "wb");
  if(dec_cont->ferror_stream == NULL) {
    DEBUG_PRINT(("Unable to open file error.h264\n"));
    return DEC_MEMFAIL;
  }
#endif

  if (dec_cfg->mcinit_cfg.mc_enable) {
    dec_cont->b_mc = 1;
    dec_cont->n_cores = DWLReadAsicCoreCount();

    /* check how many cores support H264 */
    tmp = dec_cont->n_cores;
    for(i = 0; i < dec_cont->n_cores; i++) {
      hw_build_id = DWLReadCoreHwBuildID(i);
      GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
      if (!hw_feature.h264_support && !hw_feature.h264_high10_support)
        tmp--;
      if (hw_feature.has_2nd_pipeline) {
        if (!hw_feature.has_2nd_h264_pipeline)
          tmp--;
        i++;  /* the second pipeline is treated as another core in driver */
      }
    }
    dec_cont->n_cores_available = tmp;

    dec_cont->stream_consumed_callback.fn = dec_cfg->mcinit_cfg.stream_consumed_callback;

    /* enable synchronization writing in multi-core HW */
    if(dec_cont->n_cores > 1) {
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MULTICORE_E, 1);
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_WRITESTAT_E, 1);
    }
  }

  *dec_inst = (H264DecInst) dec_cont;

#ifdef SUPPORT_VCMD
  dec_cont->vcmd_used = DWLVcmdCores();
  if (dec_cont->b_mc) {
    int i;
    /* Allows the maximum cmd buffers as real cores number. */
    FifoInit(dec_cont->n_cores_available, &dec_cont->fifo_core);
    for (i = 0; i < dec_cont->n_cores_available; i++) {
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
    }
  }
#endif

  DEC_API_TRC("H264DecInit# OK\n");

  return (DEC_OK);
}

/*------------------------------------------------------------------------------

    Function: H264DecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before H264DecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            DEC_OK            success
            DEC_PARAM_ERROR     invalid parameters
            DEC_HDRS_NOT_RDY  information not available yet

------------------------------------------------------------------------------*/
enum DecRet H264DecGetInfo(H264DecInst dec_inst, H264DecInfo * dec_info) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 max_dpb_size,no_reorder;
  storage_t *storage;

  DEC_API_TRC("H264DecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    DEC_API_TRC("H264DecGetInfo# ERROR: dec_inst or dec_info is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecGetInfo# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;

  if(storage->active_sps == NULL || storage->active_pps == NULL) {
    DEC_API_TRC("H264DecGetInfo# ERROR: Headers not decoded yet\n");
    return (DEC_HDRS_NOT_RDY);
  }

  /* h264bsdPicWidth and -Height return dimensions in macroblock units,
   * pic_width and -Height in pixels */
  dec_info->pic_width = h264bsdPicWidth(storage) << 4;
  dec_info->pic_height = h264bsdPicHeight(storage) << 4;

  dec_info->video_range = h264bsdVideoRange(storage);
  dec_info->matrix_coefficients = h264bsdMatrixCoefficients(storage);
  dec_info->colour_primaries = h264bsdColourPrimaries(storage);
  dec_info->transfer_characteristics = h264bsdTransferCharacteristics(storage);
  dec_info->colour_description_present_flag = h264bsdColourDescPresent(storage);
  dec_info->pp_enabled = storage->pp_enabled;

  dec_info->mono_chrome = h264bsdIsMonoChrome(storage);
  dec_info->interlaced_sequence = storage->active_sps->frame_mbs_only_flag == 0 ? 1 : 0;
  if(storage->no_reordering ||
      storage->active_sps->pic_order_cnt_type == 2 ||
      (storage->active_sps->vui_parameters_present_flag &&
       storage->active_sps->vui_parameters->bitstream_restriction_flag &&
       !storage->active_sps->vui_parameters->num_reorder_frames))
    no_reorder = HANTRO_TRUE;
  else
    no_reorder = HANTRO_FALSE;
  if(storage->view == 0)
    max_dpb_size = storage->active_sps->max_dpb_size;
  else {
    /* stereo view dpb size at least equal to base view size (to make sure
     * that base view pictures get output in correct display order) */
    max_dpb_size = MAX(storage->active_sps->max_dpb_size,
                       storage->active_view_sps[0]->max_dpb_size);
  }
  /* restrict max dpb size of mvc (stereo high) streams, make sure that
   * base address 15 is available/restricted for inter view reference use */
  if(storage->mvc_stream)
    max_dpb_size = MIN(max_dpb_size, 8);
  if(no_reorder)
    dec_info->pic_buff_size = MAX(storage->active_sps->num_ref_frames,1) + 1;
  else
    dec_info->pic_buff_size = max_dpb_size + 1;
  dec_info->multi_buff_pp_size =  storage->dpb->no_reordering? 2 : dec_info->pic_buff_size;
  dec_info->dpb_mode = dec_cont->dpb_mode;

  dec_info->bit_depth = (storage->active_sps->bit_depth_luma == 8 &&
                         storage->active_sps->bit_depth_chroma == 8) ? 8 : 10;

  if (storage->mvc)
    dec_info->multi_buff_pp_size *= 2;

  h264GetSarInfo(storage, &dec_info->sar_width, &dec_info->sar_height);

  h264bsdCroppingParams(storage, &dec_info->crop_params);

  if(dec_cont->tiled_mode_support) {
    if(dec_info->interlaced_sequence &&
        (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
      if(dec_info->mono_chrome)
        dec_info->output_format = DEC_OUT_FRM_MONOCHROME;
      else
        dec_info->output_format = DEC_OUT_FRM_RASTER_SCAN;
    } else
      dec_info->output_format = DEC_OUT_FRM_TILED_4X4;
  } else if(dec_info->mono_chrome)
    dec_info->output_format = DEC_OUT_FRM_MONOCHROME;
  else
    dec_info->output_format = DEC_OUT_FRM_RASTER_SCAN;

  dec_cont->ppb_mode = DEC_DPB_FRAME;

  DEC_API_TRC("H264DecGetInfo# OK\n");

  /* for interlace and baseline, using non-high10 mode */
  if (dec_cont->baseline_mode == 1)
    dec_info->base_mode = 1;
  else
    dec_info->base_mode = 0;

  return (DEC_OK);
}

void h264CheckBufferRealloc(struct H264DecContainer *dec_cont) {
  storage_t *storage = &dec_cont->storage;
  dpbStorage_t *dpb = storage->dpb;
  seqParamSet_t *p_sps = storage->active_sps;
  u32 is_high_supported = (dec_cont->h264_profile_support == H264_HIGH_PROFILE) ? 1 : 0;
  u32 is_high10_supported = dec_cont->high10p_mode;
  u32 n_cores = dec_cont->n_cores;
  u32 max_dpb_size = 0, new_pic_size = 0, new_tot_buffers = 0, dpb_size = 0, max_ref_frames = 0;
  u32 no_reorder = 0;
  struct dpbInitParams dpb_params;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 new_pic_size_in_mbs = 0;
  u32 new_pic_width_in_mbs = 0;
  u32 new_pic_height_in_mbs = 0;
  u32 out_w = 0, out_h = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  if(dec_cont->b_mvc == 0) {
    new_pic_size_in_mbs = p_sps->pic_width_in_mbs * p_sps->pic_height_in_mbs;
    new_pic_width_in_mbs = p_sps->pic_width_in_mbs;
    new_pic_height_in_mbs = p_sps->pic_height_in_mbs;
  }
  else if(dec_cont->b_mvc == 1) {
    if(storage->sps[1] != 0) {
      new_pic_size_in_mbs = MAX(storage->sps[0]->pic_width_in_mbs * storage->sps[0]->pic_height_in_mbs,
                                storage->sps[1]->pic_width_in_mbs * storage->sps[1]->pic_height_in_mbs);
      new_pic_width_in_mbs = MAX(storage->sps[0]->pic_width_in_mbs, storage->sps[1]->pic_width_in_mbs);
      new_pic_height_in_mbs = MAX(storage->sps[0]->pic_height_in_mbs, storage->sps[1]->pic_height_in_mbs);
    }
    else {
      new_pic_size_in_mbs = storage->sps[0]->pic_width_in_mbs * storage->sps[0]->pic_height_in_mbs;
      new_pic_width_in_mbs = storage->sps[0]->pic_width_in_mbs;
      new_pic_height_in_mbs = storage->sps[0]->pic_height_in_mbs;
    }
  }

  /* dpb output reordering disabled if
   * 1) application set no_reordering flag
   * 2) POC type equal to 2
   * 3) num_reorder_frames in vui equal to 0 */
  if(storage->no_reordering ||
      p_sps->pic_order_cnt_type == 2 ||
      (p_sps->vui_parameters_present_flag &&
       p_sps->vui_parameters->bitstream_restriction_flag &&
       !p_sps->vui_parameters->num_reorder_frames))
    no_reorder = HANTRO_TRUE;
  else
    no_reorder = HANTRO_FALSE;

  if (storage->view == 0)
    max_dpb_size = p_sps->max_dpb_size;
  else {
    /* stereo view dpb size at least equal to base view size (to make sure
     * that base view pictures get output in correct display order) */
    max_dpb_size = MAX(p_sps->max_dpb_size, storage->active_view_sps[0]->max_dpb_size);
  }
  /* restrict max dpb size of mvc (stereo high) streams, make sure that
   * base address 15 is available/restricted for inter view reference use */
  if (storage->mvc_stream)
    max_dpb_size = MIN(max_dpb_size, 8);

  dpb_params.pic_size_in_mbs = new_pic_size_in_mbs;
  dpb_params.pic_width_in_mbs = new_pic_width_in_mbs;
  dpb_params.pic_height_in_mbs = new_pic_height_in_mbs;
  dpb_params.dpb_size = max_dpb_size;
  dpb_params.max_ref_frames = p_sps->num_ref_frames;
  dpb_params.max_frame_num = p_sps->max_frame_num;
  dpb_params.no_reordering = no_reorder;
  dpb_params.display_smoothing = storage->use_smoothing;
  dpb_params.mono_chrome = p_sps->mono_chrome;
  dpb_params.is_high_supported = is_high_supported;
  dpb_params.is_high10_supported = is_high10_supported;
  dpb_params.enable2nd_chroma = storage->enable2nd_chroma && !p_sps->mono_chrome;
  dpb_params.n_cores = n_cores;
  dpb_params.mvc_view = storage->view;
  dpb_params.pixel_width = (p_sps->bit_depth_luma == 8 &&
                            p_sps->bit_depth_chroma ==8) ? 8 : 10;


  H264GetRefFrmSize(storage, &rfc_luma_size, &rfc_chroma_size);
  dpb_params.tbl_sizey = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  dpb_params.tbl_sizec = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  if (!storage->tiled_stride_enable) {
    if(dpb_params.is_high_supported || dpb_params.is_high10_supported) {
      /* yuv picture + direct mode motion vectors */
      new_pic_size = NEXT_MULTIPLE(dpb_params.pic_size_in_mbs * 256 * dpb_params.pixel_width / 8, ref_buffer_align)
                     + (dpb_params.mono_chrome ? 0 :
                       NEXT_MULTIPLE(dpb_params.pic_size_in_mbs * 128 * dpb_params.pixel_width / 8, ref_buffer_align))
                     + NEXT_MULTIPLE(dpb_params.pic_size_in_mbs * 64, ref_buffer_align);
      /* allocate 32 bytes for multicore status fields */
      /* locate it after picture and direct MV */
      new_pic_size += NEXT_MULTIPLE(32, ref_buffer_align);
      new_pic_size += dpb_params.tbl_sizey + dpb_params.tbl_sizec;
    } else
      new_pic_size = NEXT_MULTIPLE(dpb_params.pic_size_in_mbs * 256 * dpb_params.pixel_width / 8, ref_buffer_align)
                     + NEXT_MULTIPLE(dpb_params.pic_size_in_mbs * 128 * dpb_params.pixel_width / 8, ref_buffer_align);
  } else {
     if (dpb_params.is_high_supported || dpb_params.is_high10_supported) {
      /* yuv picture + direct mode motion vectors */
      out_w = NEXT_MULTIPLE(4 * dpb_params.pic_width_in_mbs * 16 *
                            dpb_params.pixel_width / 8,
                            ALIGN(storage->align));
      out_h = dpb_params.pic_height_in_mbs * 4;
      new_pic_size = NEXT_MULTIPLE(out_w * out_h, ref_buffer_align)
                     + (dpb_params.mono_chrome ? 0 :
                       NEXT_MULTIPLE(out_w * out_h / 2, ref_buffer_align));
      new_pic_size += NEXT_MULTIPLE(dpb_params.pic_size_in_mbs *
                                    (dpb_params.is_high10_supported? 80 : 64), ref_buffer_align);

      new_pic_size += NEXT_MULTIPLE(32, ref_buffer_align);
      /* compression table */
      new_pic_size += dpb_params.tbl_sizey + dpb_params.tbl_sizec;
    } else {
      out_w = NEXT_MULTIPLE(4 * dpb_params.pic_width_in_mbs * 16,
                            ALIGN(storage->align));
      out_h = dpb_params.pic_height_in_mbs * 4;
      new_pic_size = NEXT_MULTIPLE(out_w * out_h, ref_buffer_align)
                     + NEXT_MULTIPLE(out_w * out_h / 2, ref_buffer_align);
    }
  }

  dpb->n_new_pic_size = new_pic_size;
  max_ref_frames = MAX(dpb_params.max_ref_frames, 1);

  if(dpb_params.no_reordering)
    dpb_size = max_ref_frames;
  else
    dpb_size = dpb_params.dpb_size;

  /* max DPB size is (16 + 1) buffers */
  new_tot_buffers = dpb_size + 1;

  /* figure out extra buffers for smoothing, pp, multicore, etc... */
  if (n_cores == 1) {
    /* single core configuration */
    if (storage->use_smoothing)
      new_tot_buffers += no_reorder ? 1 : dpb_size + 1;
  } else {
    /* multi core configuration */

    if (storage->use_smoothing && !no_reorder) {
      /* at least double buffers for smooth output */
      if (new_tot_buffers > n_cores) {
        new_tot_buffers *= 2;
      } else {
        new_tot_buffers += n_cores;
      }
    } else {
      /* one extra buffer for each core */
      /* do not allocate twice for multiview */
      if(!storage->view)
        new_tot_buffers += n_cores;
    }
  }

  storage->realloc_int_buf = 0;
  storage->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (dpb->n_new_pic_size > dec_cont->n_ext_buf_size ||
          new_tot_buffers + dpb->n_guard_size > dec_cont->ext_buffer_num)
        storage->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (new_pic_size_in_mbs != dpb->pic_size_in_mbs ||
          new_tot_buffers != dpb->tot_buffers)
        storage->realloc_ext_buf = 1;
    }
    storage->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if ((CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size) ||
          new_tot_buffers + dpb->n_guard_size > dec_cont->ext_buffer_num)
        storage->realloc_ext_buf = 1;
      if (dpb->n_new_pic_size > dpb->n_int_buf_size ||
          new_tot_buffers > dpb->tot_buffers)
        storage->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if ((dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height) ||
          new_tot_buffers != dpb->tot_buffers)
        storage->realloc_ext_buf = 1;
      if (dpb->n_new_pic_size != dpb->n_int_buf_size ||
          new_tot_buffers != dpb->tot_buffers)
        storage->realloc_int_buf = 1;
    }
  }
}

/*------------------------------------------------------------------------------

    Function: H264DecRelease()

        Functional description:
            Release the decoder instance. Function calls h264bsdShutDown to
            release instance data and frees the memory allocated for the
            instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void H264DecRelease(H264DecInst dec_inst) {

  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  const void *dwl;
  u32 i;
  DEC_API_TRC("H264DecRelease#\n");

  if(dec_cont == NULL) {
    DEC_API_TRC("H264DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecRelease# ERROR: Decoder not initialized\n");
    return;
  }

#ifdef DUMP_INPUT_STREAM
  if (dec_cont->ferror_stream)
    fclose(dec_cont->ferror_stream);
#endif

  dwl = dec_cont->dwl;

  /* make sure all in sync in multicore mode, hw idle, output empty */
  if(dec_cont->b_mc) {
    h264MCWaitPicReadyAll(dec_cont);
  } else {
    u32 i;
    const dpbStorage_t *dpb = dec_cont->storage.dpb;

    /* Empty the output list. This is just so that fb_list does not
     * complaint about still referenced pictures
     */
    for(i = 0; i < dpb->tot_buffers; i++) {
      if(dpb->pic_buff_id[i] != FB_NOT_VALID_ID &&
        IsBufferOutput(&dec_cont->fb_list, dpb->pic_buff_id[i])) {
        ClearOutput(&dec_cont->fb_list, dpb->pic_buff_id[i]);
      }
    }
  }

  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->h264_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;

    /* Decrement usage for DPB buffers */
    DecrementDPBRefCount(&dec_cont->storage.dpb[1]);
  } else if (dec_cont->keep_hw_reserved)
    DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
  pthread_mutex_destroy(&dec_cont->protect_mutex);

  h264bsdShutdown(&dec_cont->storage);

  h264bsdFreeDpb(
    dwl,
    dec_cont->storage.dpbs[0]);
  if (dec_cont->storage.dpbs[1]->dpb_size)
    h264bsdFreeDpb(
      dwl,
      dec_cont->storage.dpbs[1]);

  ReleaseAsicBuffers(dwl, dec_cont->asic_buff);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  ReleaseList(&dec_cont->fb_list);
  dec_cont->checksum = NULL;
  DWLfree(dec_cont);
  DEC_API_TRC("H264DecRelease# OK\n");
  return;
}

/*------------------------------------------------------------------------------

    Function: H264DecDecode

        Functional description:
            Decode stream data. Calls h264bsdDecode to do the actual decoding.

        Input:
            dec_inst     decoder instance
            input      pointer to input struct

        Outputs:
            output     pointer to output struct

        Returns:
            DEC_NOT_INITIALIZED   decoder instance not initialized yet
            DEC_PARAM_ERROR         invalid parameters

            DEC_STRM_PROCESSED    stream buffer decoded
            DEC_HDRS_RDY    headers decoded, stream buffer not finished
            DEC_PIC_DECODED decoding of a picture finished
            DEC_STRM_ERROR  serious error in decoding, no valid parameter
                                sets available to decode picture data
            DEC_PENDING_FLUSH   next invocation of H264DecDecode() function
                                    flushed decoded picture buffer, application
                                    needs to read all output pictures (using
                                    H264DecNextPicture function) before calling
                                    H264DecDecode() again. Used only when
                                    use_display_smoothing was enabled in init.

            DEC_HW_BUS_ERROR    decoder HW detected a bus error
            DEC_SYSTEM_ERROR    wait for hardware has failed
            DEC_MEMFAIL         decoder failed to allocate memory
            DEC_DWL_ERROR   System wrapper failed to initialize
            DEC_HW_TIMEOUT  HW timeout
            DEC_HW_RESERVED HW could not be reserved
------------------------------------------------------------------------------*/
enum DecRet H264DecDecode(H264DecInst dec_inst, const H264DecInput * input,
                         H264DecOutput * output) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 strm_len;
  u32 input_data_len;  // used to generate error stream
  u8 *tmp_stream;
  u32 index = 0;
  const u8 *ref_data = NULL;
  enum DecRet return_value = DEC_STRM_PROCESSED;

  DEC_API_TRC("H264DecDecode#\n");
  /* Check that function input parameters are valid */
  if(input == NULL || output == NULL || dec_inst == NULL) {
    DEC_API_TRC("H264DecDecode# ERROR: NULL arg(s)\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecDecode# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  input_data_len = input->data_len;

  if(dec_cont->abort) {
    return (DEC_ABORTED);
  }

#ifdef USE_RANDOM_TEST
  if (dec_cont->error_params.random_error_enabled) {
    // error type: lose packets;
    if (dec_cont->error_params.lose_packets && !dec_cont->stream_not_consumed) {
      u8 lose_packet = 0;
      if (RandomizePacketLoss(dec_cont->error_params.packet_loss_odds,
                              &lose_packet)) {
        DEBUG_PRINT(("Packet loss simulator error (wrong config?)\n"));
      }
      if (lose_packet) {
        input_data_len = 0;
        output->data_left = 0;
        dec_cont->stream_not_consumed = 0;
        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                                                  (void*)dec_cont->stream_consumed_callback.p_user_data);
        }
        return DEC_STRM_PROCESSED;
      }
    }

    // error type: truncate stream(random len for random packet);
    if (dec_cont->error_params.truncate_stream && !dec_cont->stream_not_consumed) {
      u8 truncate_stream = 0;
      if (RandomizePacketLoss(dec_cont->error_params.truncate_stream_odds,
                              &truncate_stream)) {
        DEBUG_PRINT(("Truncate stream simulator error (wrong config?)\n"));
      }
      if (truncate_stream) {
        u32 random_size = input_data_len;
        if (RandomizeU32(&random_size)) {
          DEBUG_PRINT(("Truncate randomizer error (wrong config?)\n"));
        }
        input_data_len = random_size;
      }

      dec_cont->prev_input_len = input_data_len;

      if (input_data_len == 0) {
        output->data_left = 0;
        dec_cont->stream_not_consumed = 0;
        /* release buffer fully processed by SW */
        if(dec_cont->stream_consumed_callback.fn)
          dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                                                (void*)dec_cont->stream_consumed_callback.p_user_data);
        return DEC_STRM_PROCESSED;
      }
    }

    /*  stream is truncated but not consumed at first time, the same truncated length
        at the second time */
    if (dec_cont->error_params.truncate_stream && dec_cont->stream_not_consumed)
      input_data_len = dec_cont->prev_input_len;

    // error type: swap bits;
    if (dec_cont->error_params.swap_bits_in_stream && !dec_cont->stream_not_consumed) {
      u8 *p_tmp = (u8 *)input->stream;
      if (RandomizeBitSwapInStream(p_tmp, input->buffer, input->buff_len, input_data_len,
                                   dec_cont->error_params.swap_bit_odds,
                                   dec_cont->use_ringbuffer)) {
        DEBUG_PRINT(("Bitswap randomizer error (wrong config?)\n"));
      }
    }
  }
#endif

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->buffer) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(input->buffer_bus_address)) {
    DEC_API_TRC("H264DecDecode# ERROR: Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }

#ifdef DEC_EVALUATION
  if(dec_cont->pic_number > DEC_EVALUATION) {
    DEC_API_TRC("H264DecDecode# DEC_EVALUATION_LIMIT_EXCEEDED\n");
    return DEC_EVALUATION_LIMIT_EXCEEDED;
  }
#endif

  dec_cont->stream_pos_updated = 0;
  output->strm_curr_pos = NULL;
  dec_cont->hw_stream_start_bus = input->stream_bus_address;
  dec_cont->p_hw_stream_start = (u8 *)input->stream;
  dec_cont->buff_start_bus = input->buffer_bus_address;
  dec_cont->hw_buffer = input->buffer;
  strm_len = dec_cont->hw_length = input_data_len;
  dec_cont->buff_length = input->buff_len;
  /* For low latency mode, strmLen is set as a large value */
  if(dec_cont->low_latency && !stream_info.last_flag)
    strm_len = dec_cont->hw_length = DEC_X170_MAX_STREAM;
  tmp_stream = (u8 *)input->stream;
  dec_cont->stream_consumed_callback.p_strm_buff = input->stream;
  dec_cont->stream_consumed_callback.p_user_data = input->p_user_data;

  dec_cont->skip_non_reference = input->skip_non_reference;

  dec_cont->force_nal_mode = 0;

  /* If there are more buffers to be allocated or to be freed, waiting for buffers ready. */
  if (dec_cont->buf_to_free != NULL ||
      (dec_cont->next_buf_size != 0  && dec_cont->ext_buffer_num < dec_cont->buf_num)) {
    return_value = DEC_WAITING_FOR_BUFFER;
  }


  /* Switch to RLC mode, i.e. sw performs entropy decoding */
  if(dec_cont->reallocate) {
    DEBUG_PRINT(("H264DecDecode: Reallocate\n"));
    dec_cont->rlc_mode = 1;
    dec_cont->reallocate = 0;

    /* Reallocate only once */
    if(dec_cont->asic_buff->mb_ctrl.virtual_address == NULL) {
      if(h264AllocateResources(dec_cont) != 0) {
        /* signal that decoder failed to init parameter sets */
        dec_cont->storage.active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        DEC_API_TRC("H264DecDecode# ERROR: Reallocation failed\n");
        return DEC_MEMFAIL;
      }

      h264bsdResetStorage(&dec_cont->storage);
    }

  }

  do {
    u32 dec_result;
    u32 num_read_bytes = 0;
    storage_t *storage = &dec_cont->storage;
    storage->tiled_stride_enable = dec_cont->tiled_stride_enable;
    storage->align = dec_cont->align;

    DEBUG_PRINT(("H264DecDecode: mode is %s\n",
                 dec_cont->rlc_mode ? "RLC" : "VLC"));
    if(dec_cont->dec_stat == DEC_NEW_HEADERS) {
      dec_result = H264BSD_HDRS_RDY;
      dec_cont->dec_stat = DEC_INITIALIZED;
    } else if(dec_cont->dec_stat == DEC_BUFFER_EMPTY) {
      DEBUG_PRINT(("H264DecDecode: Skip h264bsdDecode\n"));
      DEBUG_PRINT(("H264DecDecode: Jump to H264BSD_PIC_RDY\n"));
      /* update stream pointers */
      strmData_t *strm = dec_cont->storage.strm;
      strm->p_strm_buff_start = tmp_stream;
      strm->strm_curr_pos = tmp_stream;
      strm->strm_data_size = strm_len;

      dec_result = H264BSD_PIC_RDY;
    }
    else if(dec_cont->dec_stat == DEC_WAIT_FOR_BUFFER) {
      dec_result = H264BSD_BUFFER_NOT_READY;
    } else {
      if(!dec_cont->error_frame) {
        if (!dec_cont->no_decoding_buffer)
          dec_result = h264bsdDecode(dec_cont, tmp_stream, strm_len,
                                    input->pic_id, &num_read_bytes);

        /* "no_decoding_buffer" was set in previous loop, so do not
        enter h264bsdDecode() twice for the same slice */
        else {
          num_read_bytes = dec_cont->num_read_bytes;
          dec_result = dec_cont->dec_result;
        }
      } else {
        dec_result = H264BSD_PIC_RDY;
      }

      if(dec_cont->storage.active_sps &&
        !dec_cont->storage.active_sps->frame_mbs_only_flag) {
        /* Only High10 progressive supported now. */
        dec_cont->high10p_mode = 0;
        dec_cont->use_video_compressor = 0;
        dec_cont->storage.use_video_compressor = 0;
        SetDecRegister(dec_cont->h264_regs, HWIF_DEC_MODE, DEC_X170_MODE_H264);
      }

      /* "alloc_buffer" is set in h264bsdDecode() to indicate if
         new output buffer is needed */

      /* no need to allocate buffer if H264BSD_RDY/H264BSD_ERROR returned */
      if (((dec_result == H264BSD_RDY || dec_result == H264BSD_ERROR) && !dec_cont->rlc_mode) &&
          (storage->active_pps && storage->active_pps->num_slice_groups == 1)) {
        dec_cont->alloc_buffer = 0;
      }
      /* when decode one pic, if stream error and need to re-allocate buffer, return the pp buffer*/
      if ((dec_cont->pp_enabled) && (dec_cont->alloc_buffer) && (storage->curr_image->data) &&
          (storage->dpb->current_out) && (storage->dpb->current_out->to_be_displayed == HANTRO_FALSE) &&
          (storage->dpb->current_out->status[0] == EMPTY && storage->dpb->current_out->status[1] == EMPTY) &&
           InputQueueCheckBufUsed(dec_cont->pp_buffer_queue, dec_cont->storage.dpb->current_out->ds_data->virtual_address)) {
        dec_cont->alloc_buffer = 0;
      }
      if (dec_cont->alloc_buffer == 1) {
        storage->curr_image->data =
            h264bsdAllocateDpbImage(storage->dpb);

        if(storage->curr_image->data == NULL) {
          if (dec_cont->abort)
            return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          dec_cont->no_decoding_buffer = 1;
          dec_cont->dec_result = dec_result;
          dec_cont->num_read_bytes = num_read_bytes;
          dec_cont->stream_pos_updated = 0;
          return_value = DEC_NO_DECODING_BUFFER;
          break;
#endif
        }
        storage->curr_image->pp_data = storage->dpb->current_out->ds_data;
        storage->dpb->current_out->data = storage->curr_image->data;
        dec_cont->alloc_buffer = 0;
        dec_cont->pre_alloc_buffer[dec_cont->storage.view] = 1;
        dec_cont->no_decoding_buffer = 0;
      }

      /* In low latency mode or some error streams, numReadBytes may be greater than strmLen */
      if(num_read_bytes > strm_len || strm_len == DEC_X170_MAX_STREAM) {
        if (stream_info.low_latency && stream_info.last_flag) {
          strm_len = stream_info.send_len;
        }
        if(num_read_bytes > strm_len)
          num_read_bytes = strm_len;
      }
      ASSERT(num_read_bytes <= strm_len);

      bsdDecodeReturn(dec_result);
    }

    tmp_stream += num_read_bytes;
    if(tmp_stream >= dec_cont->hw_buffer + dec_cont->buff_length &&
       dec_cont->use_ringbuffer)
      tmp_stream -= dec_cont->buff_length;
    strm_len -= num_read_bytes;
    switch (dec_result) {
    case H264BSD_HDRS_RDY: {
      dec_cont->storage.dpb->b_updated = 0;
      dec_cont->storage.dpb->n_ext_buf_size_added = dec_cont->n_ext_buf_size;
      dec_cont->storage.dpb->use_adaptive_buffers = dec_cont->use_adaptive_buffers;
      dec_cont->storage.dpb->n_guard_size = dec_cont->n_guard_size;

      if (dec_cont->storage.num_views &&
          dec_cont->storage.view == 0) {
        /* sps changes on base view */
        dec_cont->pic_number = 0;
      }

      u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
      struct DecHwFeatures hw_feature;
      GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

      /* interlace stream is not supported in muti pipeline mode */
      if (((dec_cont->storage.active_sps->frame_mbs_only_flag == 0) ? 1 : 0)
          && hw_feature.has_2nd_h264_pipeline)
        dec_cont->b_mc = 0;

      /* reset decode mode for baseline and interlace, not change high10 profile */
      if ((h264StreamIsBaseline(dec_cont) ||
           storage->active_sps->frame_mbs_only_flag == 0 ? 1 : 0) &&
          hw_feature.h264_support &&
          dec_cont->storage.active_sps->bit_depth_chroma <= 8 &&
          dec_cont->storage.active_sps->bit_depth_luma <= 8) {
        dec_cont->baseline_mode = 1;
        dec_cont->high10p_mode = 0;
      }

      h264CheckReleasePpAndHw(dec_cont);

      if(storage->dpb->flushed && storage->dpb->num_out) {
        /* output first all DPB stored pictures */
        storage->dpb->flushed = 0;
        dec_cont->dec_stat = DEC_NEW_HEADERS;
        /* if display smoothing used -> indicate that all pictures
        * have to be read out */
        if(storage->dpb->tot_buffers >
            storage->dpb->dpb_size + 1) {
          return_value = DEC_PENDING_FLUSH;
        } else {
          return_value = DEC_PIC_DECODED;
        }

        /* base view -> flush another view dpb */
        if (dec_cont->storage.num_views &&
            dec_cont->storage.view == 0) {
          h264bsdFlushDpb(storage->dpbs[1]);
        }
        DEC_API_TRC
        ("H264DecDecode# DEC_PIC_DECODED (DPB flush caused by new SPS)\n");
        strm_len = 0;

        break;
      } else if ((storage->dpb->tot_buffers >
                  storage->dpb->dpb_size + 1) && storage->dpb->num_out) {
        /* flush buffered output for display smoothing */
        storage->dpb->delayed_out = 0;
        storage->second_field = 0;
        dec_cont->dec_stat = DEC_NEW_HEADERS;
        return_value = DEC_PENDING_FLUSH;
        strm_len = 0;

        break;
      } else if (storage->pending_flush) {
        dec_cont->dec_stat = DEC_NEW_HEADERS;
        return_value = DEC_PENDING_FLUSH;
        strm_len = 0;

        if(dec_cont->b_mc) {
          h264MCWaitPicReadyAll(dec_cont);
        }
#if 0
        int ret;
#ifdef _HAVE_PTHREAD_H
        WaitListNotInUse(&dec_cont->fb_list);
#else
        ret = WaitListNotInUse(&dec_cont->fb_list);
        if(ret) {
          strm_len = 0;
          dec_cont->dec_stat = DEC_NEW_HEADERS;
          return_value = DEC_PENDING_FLUSH;
          break;
        }
#endif
        if (dec_cont->pp_enabled) {
          ret = InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
          if (ret) {
            strm_len = 0;
            dec_cont->dec_stat = DEC_NEW_HEADERS;
            return_value = DEC_PENDING_FLUSH;
            break;
          }
        }
#endif
        h264bsdFlushDpb(storage->dpb);
        storage->pending_flush = 0;
        DEC_API_TRC
        ("H264DecDecode# DEC_PIC_DECODED (DPB flush caused by new SPS)\n");

        break;
      }

#ifndef USE_OMXIL_BUFFER
      /* Make sure that all frame buffers are not in use before
      * reseting DPB (i.e. all HW cores are idle and all output
      * processed) */
      if(dec_cont->b_mc)
        h264MCWaitPicReadyAll(dec_cont);
      int ret;
#ifdef _HAVE_PTHREAD_H
      WaitListNotInUse(&dec_cont->fb_list);
#else
      ret = WaitListNotInUse(&dec_cont->fb_list);
      if(ret) {
        strm_len = 0;
        dec_cont->dec_stat = DEC_NEW_HEADERS;
        return_value = DEC_PENDING_FLUSH;
        break;
      }
#endif
      if (dec_cont->pp_enabled) {
        ret = InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
        if (ret) {
          strm_len = 0;
          dec_cont->dec_stat = DEC_NEW_HEADERS;
          //return_value = DEC_PENDING_FLUSH;
          break;
        }
      }
#endif

      if(!h264SpsSupported(dec_cont) ||
        (!h264StreamIsBaseline(dec_cont) && dec_cont->force_rlc_mode)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_view_sps_id[0] =
          storage->active_view_sps_id[1] =
            MAX_NUM_SEQ_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_stat = DEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;
        output->data_left = 0;

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                                                  (void*)dec_cont->stream_consumed_callback.p_user_data);
        }

#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 0;
#endif
#ifdef DUMP_INPUT_STREAM
        fwrite(input->stream, 1, input_data_len, dec_cont->ferror_stream);
#endif

        DEC_API_TRC
        ("H264DecDecode# DEC_STREAM_NOT_SUPPORTED\n");
        return DEC_STREAM_NOT_SUPPORTED;
      } else {
        dec_result = H264BSD_BUFFER_NOT_READY;
        dec_cont->dec_stat = DEC_WAIT_FOR_BUFFER;
        strm_len = 0;
        PushOutputPic(&dec_cont->fb_list, NULL, -2);
        return_value = DEC_HDRS_RDY;
      }

      if( !dec_cont->storage.active_sps->frame_mbs_only_flag &&
          dec_cont->allow_dpb_field_ordering )
        dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
      else
        dec_cont->dpb_mode = DEC_DPB_FRAME;
      break;
    }
    case H264BSD_BUFFER_NOT_READY: {
      if( dec_cont->tiled_mode_support && dec_cont->allow_dpb_field_ordering) {
        dec_cont->tiled_stride_enable = 1;
      } else {
        dec_cont->tiled_stride_enable = 0;
      }

      storage->tiled_stride_enable = dec_cont->tiled_stride_enable;
      storage->align = dec_cont->align;

      h264CheckBufferRealloc(dec_cont);

      if (storage->realloc_ext_buf) {
        if(dec_cont->b_mvc == 0)
          h264SetExternalBufferInfo(dec_cont, storage);
        else if(dec_cont->b_mvc == 1)
          h264SetMVCExternalBufferInfo(dec_cont, storage);
      }

      if (dec_cont->pp_enabled) {
        dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
        dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
      }

      u32 ret = HANTRO_OK;
      if(dec_cont->b_mvc == 0)
        ret = h264bsdAllocateSwResources(dec_cont->dwl, storage,
                                         (dec_cont->
                                          h264_profile_support == H264_HIGH_PROFILE) ? 1 :
                                         0,
                                         dec_cont->high10p_mode,
                                         dec_cont->n_cores);
      else if(dec_cont->b_mvc == 1) {
        ret = h264bsdMVCAllocateSwResources(dec_cont->dwl, storage,
                                            (dec_cont->
                                             h264_profile_support == H264_HIGH_PROFILE) ? 1 :
                                            0,
                                            dec_cont->high10p_mode,
                                            dec_cont->n_cores);
        dec_cont->b_mvc = 2;
      }
      if (ret != HANTRO_OK) {
        goto RESOURCE_NOT_READY;
      }
      ret =  h264AllocateResources(dec_cont);
      if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

RESOURCE_NOT_READY:
      if(ret) {
        if(ret == DEC_WAITING_FOR_BUFFER) {
          dec_cont->buffer_index[0] = dec_cont->buffer_index[1] = 0;
          return_value = ret;
          strm_len = 0;
          break;
        } else {
          /* signal that decoder failed to init parameter sets */
          /* TODO: what about views? */
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;

          /* reset strm_len to force memfail return also for secondary
           * view */
          strm_len = 0;

          return_value = DEC_MEMFAIL;
          DEC_API_TRC("H264DecDecode# DEC_MEMFAIL\n");
        }
        strm_len = 0;
      } else {
        dec_cont->asic_buff->enable_dmv_and_poc = 0;
        storage->dpb->interlaced = (storage->active_sps->frame_mbs_only_flag == 0) ? 1 : 0;

        if((storage->active_pps->num_slice_groups != 1) &&
            (h264StreamIsBaseline(dec_cont) == 0)) {
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;

          return_value = DEC_STREAM_NOT_SUPPORTED;
          DEC_API_TRC
          ("H264DecDecode# DEC_STREAM_NOT_SUPPORTED, FMO in Main/High Profile\n");
        } else if ((storage->active_pps->num_slice_groups != 1) && dec_cont->secure_mode) {
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          return_value = DEC_STRM_ERROR;
          DEC_API_TRC
              ("H264DecDecode# DEC_STREAM_ERROR, FMO in Secure Mode\n");
        }

        /* FMO always decoded in rlc mode */
        else if((storage->active_pps->num_slice_groups != 1) &&
            (dec_cont->rlc_mode == 0)) {
          /* set to uninit state */
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_view_sps_id[0] =
            storage->active_view_sps_id[1] =
              MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          storage->pic_started = HANTRO_FALSE;
          dec_cont->dec_stat = DEC_INITIALIZED;

          dec_cont->rlc_mode = 1;
          storage->prev_buf_not_finished = HANTRO_FALSE;
          DEC_API_TRC
          ("H264DecDecode# DEC_ADVANCED_TOOLS, FMO\n");

          return_value = DEC_ADVANCED_TOOLS;
        } else {
          /* enable direct MV writing and POC tables for
           * high/main streams.
           * enable it also for any "baseline" stream which have
           * main/high tools enabled */
          if((storage->active_sps->profile_idc > 66 &&
              storage->active_sps->constrained_set0_flag == 0) ||
              (h264StreamIsBaseline(dec_cont) == 0) ||
             (dec_cont->b_mc && dec_cont->h264_profile_support != 2)) {
            dec_cont->asic_buff->enable_dmv_and_poc = 1;
          }

          DEC_API_TRC("H264DecDecode# DEC_HDRS_RDY\n");
        }
      }

      if (!storage->view) {
        /* reset strm_len only for base view -> no HDRS_RDY to
         * application when param sets activated for stereo view */
        strm_len = 0;
        dec_cont->storage.second_field = 0;
      }

      /* Initialize DPB mode */
      if( !dec_cont->storage.active_sps->frame_mbs_only_flag &&
          dec_cont->allow_dpb_field_ordering )
        dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
      else
        dec_cont->dpb_mode = DEC_DPB_FRAME;

      /* Initialize tiled mode */
      if( dec_cont->tiled_mode_support &&
          DecCheckTiledMode( dec_cont->tiled_mode_support,
                             dec_cont->dpb_mode,
                             !dec_cont->storage.active_sps->frame_mbs_only_flag ) !=
          HANTRO_OK ) {
        return_value = DEC_PARAM_ERROR;
        DEC_API_TRC
        ("H264DecDecode# DEC_PARAM_ERROR, tiled reference "\
         "mode invalid\n");
      }

      /* reset reference addresses, this is important for multicore
       * as we use this list to track ref picture usage
       */
      DWLmemset(dec_cont->asic_buff->ref_pic_list, 0,
                sizeof(dec_cont->asic_buff->ref_pic_list));
      dec_cont->dec_stat = DEC_INITIALIZED;
      break;
    }
    case H264BSD_PIC_RDY: {
      u32 asic_status;
      u32 picture_broken;
      u32 prev_irq_buffer = dec_cont->dec_stat == DEC_BUFFER_EMPTY; /* entry due to IRQ_BUFFER */
      DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;

      picture_broken = (storage->picture_broken && ((storage->intra_freeze &&
                        !IS_IDR_NAL_UNIT(storage->prev_nal_unit)) ||
                        ((dec_cont->error_handling & DEC_EC_REF_NEXT_I) &&
                        !IS_I_SLICE(storage->slice_header->slice_type))));

      /* frame number workaround */
      if (!dec_cont->rlc_mode && dec_cont->workarounds.h264.frame_num &&
          !dec_cont->secure_mode) {
        u32 tmp;
        dec_cont->force_nal_mode =
          h264bsdFixFrameNum((u8*)tmp_stream - num_read_bytes,
                             strm_len + num_read_bytes,
                             storage->slice_header->frame_num,
                             storage->active_sps->max_frame_num, &tmp);

        /* stuff skipped before slice start code */
        if (dec_cont->force_nal_mode && tmp > 0) {
          dec_cont->p_hw_stream_start += tmp;
          dec_cont->hw_stream_start_bus += tmp;
          dec_cont->hw_length -= tmp;
        }
      }

      if( !((dec_cont->dec_stat == DEC_BUFFER_EMPTY || dec_cont->error_frame)) && !picture_broken) {
        /* setup the reference frame list; just at picture start */
        dpbStorage_t *dpb = storage->dpb;
        dpbPicture_t *buffer = dpb->buffer;

        /* list in reorder */
        u32 i;

        if(!h264PpsSupported(dec_cont)) {
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;

          return_value = DEC_STREAM_NOT_SUPPORTED;
          DEC_API_TRC
          ("H264DecDecode# DEC_STREAM_NOT_SUPPORTED, Main/High Profile tools detected\n");
          goto end;
        }

        if((dec_cont->is8190 == 0) && (dec_cont->rlc_mode == 0)) {
          for(i = 0; i < dpb->dpb_size; i++) {
            p_asic_buff->ref_pic_list[i] =
              buffer[dpb->list[i]].data->bus_address;
          }
        } else {
          for(i = 0; i < dpb->dpb_size; i++) {
            p_asic_buff->ref_pic_list[i] =
              buffer[i].data->bus_address;
          }
        }

        /* Multicore: increment usage for DPB buffers */
        IncrementDPBRefCount(dpb);

        p_asic_buff->max_ref_frm = dpb->max_ref_frames;
        p_asic_buff->out_buffer = storage->curr_image->data;
        p_asic_buff->out_pp_buffer = storage->curr_image->pp_data;

        p_asic_buff->chroma_qp_index_offset =
          storage->active_pps->chroma_qp_index_offset;
        p_asic_buff->chroma_qp_index_offset2 =
          storage->active_pps->chroma_qp_index_offset2;
        p_asic_buff->filter_disable = 0;

        h264bsdDecodePicOrderCnt(storage->poc,
                                 storage->active_sps,
                                 storage->slice_header,
                                 storage->prev_nal_unit);

#ifdef ENABLE_DPB_RECOVER
        if (IS_IDR_NAL_UNIT(storage->prev_nal_unit))
          storage->dpb->try_recover_dpb = HANTRO_FALSE;

        if (storage->dpb->try_recover_dpb
            /* && storage->slice_header->firstMbInSlice */
            && IS_I_SLICE(storage->slice_header->slice_type)
            && 2 * storage->dpb->max_ref_frames <= storage->dpb->max_frame_num)
          h264DpbRecover(storage->dpb, storage->slice_header->frame_num,
                         MIN(storage->poc->pic_order_cnt[0], storage->poc->pic_order_cnt[1]),
                         dec_cont->error_handling);
#endif
        if(dec_cont->rlc_mode) {
          if(storage->num_concealed_mbs == storage->pic_size_in_mbs) {
            p_asic_buff->whole_pic_concealed = 1;
            p_asic_buff->filter_disable = 1;
            p_asic_buff->chroma_qp_index_offset = 0;
            p_asic_buff->chroma_qp_index_offset2 = 0;
          } else {
            p_asic_buff->whole_pic_concealed = 0;
          }

          PrepareIntra4x4ModeData(storage, p_asic_buff);
          PrepareMvData(storage, p_asic_buff);
          PrepareRlcCount(storage, p_asic_buff);
        } else {
          H264SetupVlcRegs(dec_cont);
        }

        H264ErrorRecover(dec_cont);

        DEBUG_PRINT(("Save DPB status\n"));
        /* we trust our memcpy; ignore return value */
        (void) DWLmemcpy(&storage->dpb[1], &storage->dpb[0],
                         sizeof(*storage->dpb));

        DEBUG_PRINT(("Save POC status\n"));
        (void) DWLmemcpy(&storage->poc[1], &storage->poc[0],
                         sizeof(*storage->poc));

        h264bsdCroppingParams(storage,
                              &storage->dpb->current_out->crop);

        /* create output picture list */
        if (!dec_cont->error_frame)
          h264UpdateAfterPictureDecode(dec_cont);

        /* enable output writing by default */
        dec_cont->asic_buff->disable_out_writing = 0;
      } else {
        dec_cont->dec_stat = DEC_INITIALIZED;
      }

      /* disallow frame-mode DPB and tiled mode when decoding interlaced
       * content */
      if( dec_cont->dpb_mode == DEC_DPB_FRAME &&
          dec_cont->storage.active_sps &&
          !dec_cont->storage.active_sps->frame_mbs_only_flag &&
          dec_cont->tiled_reference_enable ) {
        DEC_API_TRC("DPB mode does not support tiled reference "\
                    "pictures");
        return DEC_STRM_ERROR;
      }

      if (dec_cont->storage.partial_freeze) {
        PreparePartialFreeze((u8*)storage->curr_image->data->virtual_address,
                             h264bsdPicWidth(&dec_cont->storage),
                             h264bsdPicHeight(&dec_cont->storage));
      }

      /* run asic and react to the status */
      if( !picture_broken ) {
        asic_status = H264RunAsic(dec_cont, p_asic_buff);
      } else {
        if( dec_cont->storage.pic_started ) {
          if( !storage->slice_header->field_pic_flag ||
              !storage->second_field ) {
            h264InitPicFreezeOutput(dec_cont, 0);
            h264UpdateAfterPictureDecode(dec_cont);
          }
        }
        asic_status = DEC_8190_IRQ_ERROR;
      }

      if (storage->view)
        storage->non_inter_view_ref = 0;

      dec_cont->error_frame = 0;
      /* Handle system error situations */
      if(asic_status == X170_DEC_TIMEOUT) {
        /* This timeout is DWL(software/os) generated */
        DEC_API_TRC
        ("H264DecDecode# DEC_HW_TIMEOUT, SW generated\n");
        return DEC_HW_TIMEOUT;
      } else if(asic_status == X170_DEC_SYSTEM_ERROR) {
        DEC_API_TRC("H264DecDecode# DEC_SYSTEM_ERROR\n");
        return DEC_SYSTEM_ERROR;
      } else if(asic_status == X170_DEC_FATAL_SYSTEM_ERROR) {
        DEC_API_TRC("H264DecDecode# DEC_FATAL_SYSTEM_ERROR\n");
        return DEC_FATAL_SYSTEM_ERROR;
      } else if(asic_status == X170_DEC_HW_RESERVED) {
        DEC_API_TRC("H264DecDecode# DEC_HW_RESERVED\n");
        return DEC_HW_RESERVED;
      }

      /* Handle possible common HW error situations */
      if(asic_status & DEC_8190_IRQ_BUS) {
        output->strm_curr_pos = (u8 *) input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        if (stream_info.low_latency) {
          while (!stream_info.last_flag)
            sched_yield();
          input_data_len = stream_info.send_len;
        }
        output->data_left = input_data_len;

#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 1;
#endif

        DEC_API_TRC("H264DecDecode# DEC_HW_BUS_ERROR\n");
        return DEC_HW_BUS_ERROR;
      } else if(asic_status &
                (DEC_8190_IRQ_TIMEOUT | DEC_8190_IRQ_ABORT)) {
        /* This timeout is HW generated */
        DEBUG_PRINT(("IRQ: HW %s\n",
                     (asic_status & DEC_8190_IRQ_TIMEOUT) ?
                     "TIMEOUT" : "ABORT"));

#ifdef H264_TIMEOUT_ASSERT
        ASSERT(0);
#endif
        if(dec_cont->packet_decoded != HANTRO_TRUE) {
          DEBUG_PRINT(("reset pic_started\n"));
          dec_cont->storage.pic_started = HANTRO_FALSE;
        }

        /* for concealment after a HW error report we use the saved reference list */
        if (dec_cont->storage.partial_freeze) {
          dpbStorage_t *dpb_partial = &dec_cont->storage.dpb[1];
          do {
            ref_data = h264bsdGetRefPicDataVlcMode(dpb_partial,
                                                   dpb_partial->list[index], 0);
            index++;
          } while(index < 16 && ref_data == NULL);
        }

        if (!dec_cont->storage.partial_freeze ||
            !ProcessPartialFreeze((u8*)storage->curr_image->data->virtual_address,
                                  ref_data,
                                  h264bsdPicWidth(&dec_cont->storage),
                                  h264bsdPicHeight(&dec_cont->storage),
                                  dec_cont->storage.partial_freeze == 1)) {
          dec_cont->storage.picture_broken = HANTRO_TRUE;
          h264InitPicFreezeOutput(dec_cont, 1);
        } else {
          asic_status &= ~DEC_8190_IRQ_TIMEOUT;
          asic_status |= DEC_8190_IRQ_RDY;
          dec_cont->storage.picture_broken = HANTRO_FALSE;
        }

        if(!dec_cont->rlc_mode) {
          strmData_t *strm = dec_cont->storage.strm;
          u32 next =
            H264NextStartCode(strm);

          if(next != END_OF_STREAM) {
            u32 consumed;

            tmp_stream -= num_read_bytes;
            if(tmp_stream < dec_cont->hw_buffer && dec_cont->use_ringbuffer)
              tmp_stream += dec_cont->buff_length;
            strm_len += num_read_bytes;

            if(strm->strm_curr_pos < tmp_stream && dec_cont->use_ringbuffer)
              consumed = (u32) (strm->strm_curr_pos - tmp_stream + dec_cont->buff_length);
            else
              consumed = (u32) (strm->strm_curr_pos - tmp_stream);
            tmp_stream += consumed;
            if(tmp_stream >= dec_cont->hw_buffer + dec_cont->buff_length &&
               dec_cont->use_ringbuffer)
              tmp_stream -= dec_cont->buff_length;
            strm_len -= consumed;
          }
        }

        dec_cont->stream_pos_updated = 0;
        dec_cont->pic_number++;

        dec_cont->packet_decoded = HANTRO_FALSE;
        storage->skip_redundant_slices = HANTRO_TRUE;

        /* Remove this NAL unit from stream */
        strm_len = 0;
        DEC_API_TRC("H264DecDecode# DEC_PIC_DECODED\n");
        return_value = DEC_PIC_DECODED;
        break;
      }

      if(dec_cont->rlc_mode) {
        if(asic_status & DEC_8190_IRQ_ERROR) {
          DEBUG_PRINT
          (("H264DecDecode# IRQ_STREAM_ERROR in RLC mode)!\n"));
        }

        /* It was rlc mode, but switch back to vlc when allowed */
        if(dec_cont->try_vlc) {
          storage->prev_buf_not_finished = HANTRO_FALSE;
          DEBUG_PRINT(("H264DecDecode: RLC mode used, but try VLC again\n"));
          /* start using VLC mode again */
          dec_cont->rlc_mode = 0;
          dec_cont->try_vlc = 0;
          dec_cont->mode_change = 0;
        }

        dec_cont->pic_number++;

#ifdef FFWD_WORKAROUND
        storage->prev_idr_pic_ready =
          IS_IDR_NAL_UNIT(storage->prev_nal_unit);
#endif /* FFWD_WORKAROUND */



        DEC_API_TRC("H264DecDecode# DEC_PIC_DECODED\n");
        return_value = DEC_PIC_DECODED;
        strm_len = 0;

        break;
      }

      /* from down here we handle VLC mode */

      /* in High/Main streams if HW model returns ASO interrupt, it
       * really means that there is a generic stream error. */
      if((asic_status & DEC_8190_IRQ_ASO) &&
          (p_asic_buff->enable_dmv_and_poc != 0 ||
           (h264StreamIsBaseline(dec_cont) == 0))) {
        DEBUG_PRINT(("ASO received in High/Main stream => STREAM_ERROR\n"));
        asic_status &= ~DEC_8190_IRQ_ASO;
        asic_status |= DEC_8190_IRQ_ERROR;
      }

      /* in Secure mode if HW model returns ASO interrupt, decoder treat
         it as error */
      if ((asic_status & DEC_8190_IRQ_ASO) && dec_cont->secure_mode) {
        DEBUG_PRINT(("ASO received in secure mode => STREAM_ERROR\n"));
        asic_status &= ~DEC_8190_IRQ_ASO;
        asic_status |= DEC_8190_IRQ_ERROR;
      }

      /* Check for CABAC zero words here */
      if( asic_status & DEC_8190_IRQ_BUFFER) {
        if( dec_cont->storage.active_pps->entropy_coding_mode_flag &&
            !dec_cont->secure_mode) {
          u32 tmp;
          u32 check_words = 1;
          strmData_t strm_tmp = *dec_cont->storage.strm;
          tmp = dec_cont->p_hw_stream_start - strm_tmp.p_strm_buff_start;
          strm_tmp.strm_curr_pos = dec_cont->p_hw_stream_start;
          strm_tmp.strm_buff_read_bits = 8*tmp;
          strm_tmp.bit_pos_in_word = 0;
          if (stream_info.low_latency) {
            while (!stream_info.last_flag)
              sched_yield();
            input_data_len = stream_info.send_len;
          }
          strm_tmp.strm_data_size = input_data_len -
                                    (strm_tmp.p_strm_buff_start - input->stream);

          tmp = GetDecRegister(dec_cont->h264_regs, HWIF_START_CODE_E );
          /* In NAL unit mode, if NAL unit was of type
           * "reserved" or sth other unsupported one, we need
           * to skip zero word check. */
          if( tmp == 0 ) {
            tmp = DWLLowLatencyReadByte(input->stream, strm_tmp.strm_data_size) & 0x1F;
            if( tmp != NAL_CODED_SLICE &&
                tmp != NAL_CODED_SLICE_IDR )
              check_words = 0;
          }

          if(check_words) {
            tmp = h264CheckCabacZeroWords( &strm_tmp );
            if( tmp != HANTRO_OK ) {
              DEBUG_PRINT(("CABAC_ZERO_WORD error after packet => STREAM_ERROR\n"));
            } /* cabacZeroWordError */
          }
        }
      }

      /* Handle ASO */
      if(asic_status & DEC_8190_IRQ_ASO) {
        DEBUG_PRINT(("IRQ: ASO dedected\n"));
        ASSERT(dec_cont->rlc_mode == 0);

        dec_cont->reallocate = 1;
        dec_cont->try_vlc = 1;
        dec_cont->mode_change = 1;

        /* restore DPB status */
        DEBUG_PRINT(("Restore DPB status\n"));

        /* remove any pictures marked for output */
        if (storage->dpb[0].num_out >= storage->dpb[1].num_out)
          h264RemoveNoBumpOutput(&storage->dpb[0], storage->dpb[0].num_out - storage->dpb[1].num_out);
        else {
          /*this is must call the h264bsdMarkDecRefPic and find it is IDR, and clean the buffer*/
          storage->dpb[1].num_out = 0;
          storage->dpb[1].out_index_w = storage->dpb[1].out_index_r = 0;
        }
        /* we trust our memcpy; ignore return value */
        (void) DWLmemcpy(&storage->dpb[0], &storage->dpb[1],
                         sizeof(dpbStorage_t));
        DEBUG_PRINT(("Restore POC status\n"));
        (void) DWLmemcpy(&storage->poc[0], &storage->poc[1],
                         sizeof(*storage->poc));

        storage->skip_redundant_slices = HANTRO_FALSE;
        storage->aso_detected = 1;

        DEC_API_TRC("H264DecDecode# DEC_ADVANCED_TOOLS, ASO\n");
        return_value = DEC_ADVANCED_TOOLS;

        goto end;
      } else if(asic_status & DEC_8190_IRQ_BUFFER) {
        DEBUG_PRINT(("IRQ: BUFFER EMPTY\n"));

        /* a packet successfully decoded, don't reset pic_started flag if
         * there is a need for rlc mode */
        dec_cont->dec_stat = DEC_BUFFER_EMPTY;
        dec_cont->packet_decoded = HANTRO_TRUE;
        output->data_left = 0;
#if 0
        if (dec_cont->force_nal_mode) {
          u32 tmp;
          const u8 *next;

          next =
            h264bsdFindNextStartCode(dec_cont->p_hw_stream_start-1,
                                     dec_cont->hw_length+1);

          if(next != NULL) {
            tmp = next - dec_cont->p_hw_stream_start;
            dec_cont->p_hw_stream_start += tmp;
            dec_cont->hw_stream_start_bus += tmp;
            dec_cont->hw_length -= tmp;
            tmp_stream = dec_cont->p_hw_stream_start;
            strm_len = dec_cont->hw_length;
            continue;
          }
        }
#endif
#ifdef DUMP_INPUT_STREAM
        fwrite(input->stream, 1, input_data_len, dec_cont->ferror_stream);
#endif
#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 0;
#endif

        DEC_API_TRC
        ("H264DecDecode# DEC_STRM_PROCESSED, give more data\n");
        return DEC_BUF_EMPTY;
      }
      /* Handle stream error detected in HW */
      else if(asic_status & DEC_8190_IRQ_ERROR) {
        DEBUG_PRINT(("IRQ: STREAM ERROR detected\n"));
        if(dec_cont->packet_decoded != HANTRO_TRUE) {
          DEBUG_PRINT(("reset pic_started\n"));
          dec_cont->storage.pic_started = HANTRO_FALSE;
        }
        if (dec_cont->error_conceal)
          dec_cont->error_frame = 1;
        if (!dec_cont->error_conceal)
        {
          strmData_t *strm = dec_cont->storage.strm;

          if (prev_irq_buffer) {
            /* Call H264DecDecode() due to DEC_8190_IRQ_ERROR,
               reset strm to input buffer. */
            strm->p_strm_buff_start = input->buffer;
            strm->strm_curr_pos = input->stream;
            strm->strm_buff_size = input->buff_len;
            if (stream_info.low_latency && stream_info.last_flag)
              input_data_len = stream_info.send_len;
            strm->strm_data_size = input_data_len;
            strm->strm_buff_read_bits = (u32)(strm->strm_curr_pos - strm->p_strm_buff_start) * 8;
            strm->is_rb = dec_cont->use_ringbuffer;;
            strm->remove_emul3_byte = 0;
            strm->bit_pos_in_word = 0;
          }

          u32 next =
            H264NextStartCode(strm);

          if(next == HANTRO_OK) {
            u32 consumed;

            tmp_stream -= num_read_bytes;
            if(tmp_stream < dec_cont->hw_buffer && dec_cont->use_ringbuffer)
              tmp_stream += dec_cont->buff_length;
            strm_len += num_read_bytes;

            if(strm->strm_curr_pos < tmp_stream)
              consumed = (u32) (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
            else
              consumed = (u32) (strm->strm_curr_pos - tmp_stream);

            tmp_stream += consumed;
            if(tmp_stream >= dec_cont->hw_buffer + dec_cont->buff_length &&
               dec_cont->use_ringbuffer)
              tmp_stream -= dec_cont->buff_length;
            strm_len -= consumed;
          }
        } else {
          tmp_stream = dec_cont->p_hw_stream_start;
          strm_len = dec_cont->hw_length;
        }

        /* REMEMBER TO UPDATE(RESET) STREAM POSITIONS */
        ASSERT(dec_cont->rlc_mode == 0);

        /* for concealment after a HW error report we use the saved reference list */
        if (dec_cont->storage.partial_freeze) {
          dpbStorage_t *dpb_partial = &dec_cont->storage.dpb[1];
          do {
            ref_data = h264bsdGetRefPicDataVlcMode(dpb_partial,
                                                   dpb_partial->list[index], 0);
            index++;
          } while(index < 16 && ref_data == NULL);
        }

        if (!dec_cont->storage.partial_freeze ||
            !ProcessPartialFreeze((u8*)storage->curr_image->data->virtual_address,
                                  ref_data,
                                  h264bsdPicWidth(&dec_cont->storage),
                                  h264bsdPicHeight(&dec_cont->storage),
                                  dec_cont->storage.partial_freeze == 1)) {
          dec_cont->storage.picture_broken = HANTRO_TRUE;
          dec_cont->storage.dpb->try_recover_dpb = HANTRO_TRUE;
          if (!dec_cont->error_frame)
            h264InitPicFreezeOutput(dec_cont, 1);
        } else {
          asic_status &= ~DEC_8190_IRQ_ERROR;
          asic_status |= DEC_8190_IRQ_RDY;
          dec_cont->storage.picture_broken = HANTRO_FALSE;
        }

        /* HW returned stream position is not valid in this case */
        dec_cont->stream_pos_updated = 0;
        if (dec_cont->error_conceal && !strm_len) {
          output->data_left = 0;
          return (return_value);
        }
      } else if((asic_status & DEC_8190_IRQ_NONESLICE) ||(asic_status & DEC_8190_IRQ_NOLASTSLICE)) {
        dec_cont->stream_pos_updated = 1;
        dec_cont->storage.pic_started = HANTRO_FALSE;
        return_value = DEC_PIC_DECODED;
        strm_len = 0;
        break;
      } else { /* OK in here */
        if( IS_IDR_NAL_UNIT(storage->prev_nal_unit) ) {
          dec_cont->storage.picture_broken = HANTRO_FALSE;
        }
      }

      if( dec_cont->storage.active_pps->entropy_coding_mode_flag &&
          (((asic_status & DEC_8190_IRQ_ERROR) == 0) &&
           ((asic_status & DEC_8190_IRQ_NONESLICE) == 0) &&
           ((asic_status & DEC_8190_IRQ_NOLASTSLICE) == 0)) &&
          !dec_cont->b_mc &&
          !dec_cont->secure_mode) {
        u32 tmp;

        strmData_t strm_tmp = *dec_cont->storage.strm;
        tmp = dec_cont->p_hw_stream_start - strm_tmp.p_strm_buff_start;
        strm_tmp.strm_curr_pos = dec_cont->p_hw_stream_start;
        strm_tmp.strm_buff_read_bits = 8*tmp;
        strm_tmp.bit_pos_in_word = 0;
        if (stream_info.low_latency) {
          while (!stream_info.last_flag)
            sched_yield();
          input_data_len = strm_len = stream_info.send_len;
        }
        strm_tmp.strm_data_size = input_data_len -
                                  (strm_tmp.p_strm_buff_start - input->stream);

        /* In low latency mode, do not use input_data_len to compute buffer size */
        //  strm_tmp.strm_buff_size = (stream_info.strm_bus_addr - stream_info.strm_bus_start_addr) -
        //      (strm_tmp.p_strm_buff_start - input->stream);
        tmp = h264CheckCabacZeroWords( &strm_tmp );
        if( tmp != HANTRO_OK ) {
          DEBUG_PRINT(("Error decoding CABAC zero words\n"));
          {
            strmData_t *strm = dec_cont->storage.strm;
            u32 next =
              H264NextStartCode(strm);

            if(next != END_OF_STREAM) {
              u32 consumed;

              tmp_stream -= num_read_bytes;
              if(tmp_stream < dec_cont->hw_buffer && dec_cont->use_ringbuffer)
                tmp_stream += dec_cont->buff_length;
              strm_len += num_read_bytes;
              if(strm->strm_curr_pos < tmp_stream && dec_cont->use_ringbuffer)
                consumed = (u32) (strm->strm_curr_pos - tmp_stream + dec_cont->buff_length);
              else
                consumed = (u32) (strm->strm_curr_pos - tmp_stream);
              tmp_stream += consumed;
              if(tmp_stream >= dec_cont->hw_buffer + input->buff_len &&
                 dec_cont->use_ringbuffer)
                tmp_stream -= dec_cont->buff_length;
              strm_len -= consumed;
            }
          }

          ASSERT(dec_cont->rlc_mode == 0);
        } else {
          i32 remain = input_data_len - (strm_tmp.strm_curr_pos -
                       strm_tmp.p_strm_buff_start );

          /* byte stream format if starts with 0x000001 or 0x000000 */
          if (remain > 3 && (h264bsdShowBits(&strm_tmp, 24) == 0x000000 ||
              h264bsdShowBits(&strm_tmp, 24) == 0x000001)) {
            u32 next =
                H264NextStartCode(&strm_tmp);

            u32 consumed;
            if(next != END_OF_STREAM)
              if(strm_tmp.strm_curr_pos < input->stream && dec_cont->use_ringbuffer)
                consumed = (u32) (strm_tmp.strm_curr_pos - input->stream +
                                  dec_cont->buff_length);
              else
                consumed = (u32) (strm_tmp.strm_curr_pos - input->stream);
            else
              consumed = input_data_len;

            if(consumed != 0) {
              dec_cont->stream_pos_updated = 0;
              tmp_stream = (u8 *)input->stream + consumed;
              if(tmp_stream >= dec_cont->hw_buffer + dec_cont->buff_length &&
                 dec_cont->use_ringbuffer)
                tmp_stream -= dec_cont->buff_length;
            }
          }
        }
      }

      if (dec_cont->b_mc) {
        tmp_stream = (u8 *)input->stream + input_data_len;
        strm_len = 0;
        dec_cont->stream_pos_updated = 0;
      }

      /* For the switch between modes */
      /* this is a sign for RLC mode + mb error conceal NOT to reset pic_started flag */
      dec_cont->packet_decoded = HANTRO_FALSE;

      if (!dec_cont->error_frame) {
        DEBUG_PRINT(("Skip redundant VLC\n"));
        storage->skip_redundant_slices = HANTRO_TRUE;
        dec_cont->pic_number++;
        if (dec_cont->high10p_mode)
          H264CycleCount(dec_cont);

#ifdef FFWD_WORKAROUND
        storage->prev_idr_pic_ready =
          IS_IDR_NAL_UNIT(storage->prev_nal_unit);
#endif /* FFWD_WORKAROUND */

        return_value = DEC_PIC_DECODED;
        strm_len = 0;
      }
      break;
    }
    case H264BSD_PARAM_SET_ERROR: {
      if(!h264bsdCheckValidParamSets(&dec_cont->storage) &&
          strm_len == 0) {
        DEC_API_TRC
        ("H264DecDecode# DEC_STRM_ERROR, Invalid parameter set(s)\n");
        return_value = DEC_STRM_ERROR;
      }

      /* update HW buffers if VLC mode */
      if(!dec_cont->rlc_mode) {
        dec_cont->hw_length -= num_read_bytes;
        if(tmp_stream >= input->stream) {
           dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
        } else {
          dec_cont->hw_stream_start_bus =
            input->buffer_bus_address + (u32)(tmp_stream - input->buffer);
        }
        dec_cont->p_hw_stream_start = tmp_stream;
      }

      /* If no active sps, no need to check */
      if(storage->active_sps && !h264SpsSupported(dec_cont)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_view_sps_id[0] =
          storage->active_view_sps_id[1] =
            MAX_NUM_SEQ_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_stat = DEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                                                  (void*)dec_cont->stream_consumed_callback.p_user_data);
        }
        DEC_API_TRC
        ("H264DecDecode# DEC_STREAM_NOT_SUPPORTED\n");
        return_value = DEC_STREAM_NOT_SUPPORTED;
        goto end;
      }

      break;
    }
    case H264BSD_NEW_ACCESS_UNIT: {
      h264CheckReleasePpAndHw(dec_cont);

      dec_cont->stream_pos_updated = 0;

      dec_cont->storage.picture_broken = HANTRO_TRUE;
      if (dec_cont->storage.curr_image->data && dec_cont->storage.dpb->current_out)
        h264InitPicFreezeOutput(dec_cont, 0);

      h264UpdateAfterPictureDecode(dec_cont);
      if(dec_cont->pp_enabled && dec_cont->storage.dpb->current_out)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.dpb->current_out->ds_data->virtual_address);

      /* PP will run in H264DecNextPicture() for this concealed picture */

      DEC_API_TRC("H264DecDecode# DEC_PIC_DECODED, NEW_ACCESS_UNIT\n");
      return_value = DEC_PIC_DECODED;

      dec_cont->pic_number++;
      strm_len = 0;

      break;
    }
    case H264BSD_FMO: { /* If picture parameter set changed and FMO detected */
      DEBUG_PRINT(("FMO detected\n"));

      ASSERT(dec_cont->rlc_mode == 0);
      ASSERT(dec_cont->reallocate == 1);

      /* tmp_stream = input->stream; */

      DEC_API_TRC("H264DecDecode# DEC_ADVANCED_TOOLS, FMO\n");
      return_value = DEC_ADVANCED_TOOLS;

      strm_len = 0;
      break;
    }
    case H264BSD_UNPAIRED_FIELD: {
      /* unpaired field detected and PP still running (wait after
       * second field decoded) -> wait here */
      h264CheckReleasePpAndHw(dec_cont);

      DEC_API_TRC("H264DecDecode# DEC_PIC_DECODED, UNPAIRED_FIELD\n");
      return_value = DEC_PIC_DECODED;

      strm_len = 0;
      break;
    }
    case H264BSD_ABORTED:
      dec_cont->dec_stat = DEC_ABORTED;
      return DEC_ABORTED;
    case H264BSD_ERROR_DETECTED: {
      return_value = DEC_STREAM_ERROR_DEDECTED;
      strm_len = 0;
      break;
    }
    case H264BSD_NONREF_PIC_SKIPPED:
      return_value = DEC_NONREF_PIC_SKIPPED;
    /* fall through */
    default: { /* case H264BSD_ERROR, H264BSD_RDY */
      dec_cont->hw_length -= num_read_bytes;
      if(tmp_stream >= input->stream) {
        dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
      } else {
        dec_cont->hw_stream_start_bus =
          input->buffer_bus_address + (u32)(tmp_stream - input->buffer);
      }
      dec_cont->p_hw_stream_start = tmp_stream;
    }
    }
  } while(strm_len);

end:

  /*  If Hw decodes stream, update stream buffers from "storage" */
  if(dec_cont->stream_pos_updated) {
    if (dec_cont->secure_mode)
      output->data_left = 0;
    else {
      output->strm_curr_pos = (u8 *) dec_cont->p_hw_stream_start;
      output->strm_curr_bus_address = dec_cont->hw_stream_start_bus;
      output->data_left = dec_cont->hw_length;
    }
  } else {
    /* else update based on SW stream decode stream values */
    u32 data_consumed = (u32) (tmp_stream - input->stream);
    if(tmp_stream >= input->stream)
      data_consumed = (u32)(tmp_stream - input->stream);
    else
      data_consumed = (u32)(tmp_stream + dec_cont->buff_length - input->stream);

    output->strm_curr_pos = (u8 *) tmp_stream;
    output->strm_curr_bus_address = input->stream_bus_address + data_consumed;
    if(output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len))
      output->strm_curr_bus_address -= input->buff_len;

    output->data_left = input_data_len - data_consumed;
  }
  ASSERT(output->strm_curr_bus_address <= (input->buffer_bus_address + input->buff_len));

  if (dec_cont->storage.sei.buffering_period_info.exist_flag && dec_cont->storage.sei.pic_timing_info.exist_flag) {
    if(return_value == DEC_PIC_DECODED && dec_cont->dec_stat != DEC_NEW_HEADERS) {
      dec_cont->storage.sei.compute_time_info.access_unit_size = output->strm_curr_pos - input->stream;
      dec_cont->storage.sei.bumping_flag = 1;
    }
  }

  /* Workaround for too big data_left value from error stream */
  if (output->data_left > input_data_len) {
    output->data_left = input_data_len;
  }

#ifdef DUMP_INPUT_STREAM
  fwrite(input->stream, 1, (input_data_len - output->data_left), dec_cont->ferror_stream);
#endif
#ifdef USE_RANDOM_TEST
  /*fwrite(input->stream, 1, (input_data_len - output->data_left), dec_cont->ferror_stream);


  if (output->strm_curr_pos >= input->stream)
    fwrite(input->stream, 1, (input_data_len - output->data_left), dec_cont->ferror_stream);
  else {
    fwrite(input->stream, 1, (u32)(input->buffer_bus_address + input->buff_len - input->stream_bus_address),
           dec_cont->ferror_stream);

    fwrite(input->buffer, 1, (u32)(output->strm_curr_bus_address - input->buffer_bus_address),
           dec_cont->ferror_stream);
  }*/

  if (output->data_left == input_data_len)
    dec_cont->stream_not_consumed = 1;
  else
    dec_cont->stream_not_consumed = 0;
#endif

  FinalizeOutputAll(&dec_cont->fb_list);

  if(return_value == DEC_PIC_DECODED || return_value == DEC_STREAM_ERROR_DEDECTED) {
    dec_cont->gaps_checked_for_this = HANTRO_FALSE;
  }
  if (!dec_cont->b_mc) {
    u32 ret;
    H264DecPicture output;
    u32 flush_all = 0;

    if (return_value == DEC_PENDING_FLUSH)
      flush_all = 1;

    //if(return_value == DEC_PIC_DECODED || return_value == DEC_PENDING_FLUSH)
    {
      do {
        memset(&output, 0, sizeof(H264DecPicture));
        ret = H264DecNextPicture_INTERNAL(dec_cont, &output, flush_all);
      } while( ret == DEC_PIC_RDY);
    }
  }
  if(dec_cont->b_mc) {
    if(return_value == DEC_PIC_DECODED || return_value == DEC_PENDING_FLUSH) {
      h264MCPushOutputAll(dec_cont);
    } else if(output->data_left == 0) {
      /* release buffer fully processed by SW */
      if(dec_cont->stream_consumed_callback.fn)
        dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                                              (void*)dec_cont->stream_consumed_callback.p_user_data);

    }
  }
  if(dec_cont->abort)
    return(DEC_ABORTED);
  else
    return (return_value);
}

/*------------------------------------------------------------------------------
    Function name : H264DecGetAPIVersion
    Description   : Return the API version information

    Return type   : H264DecApiVersion
    Argument      : void
------------------------------------------------------------------------------*/
H264DecApiVersion H264DecGetAPIVersion(void) {
  H264DecApiVersion ver;

  ver.major = DEC_MAJOR_VERSION;
  ver.minor = DEC_MINOR_VERSION;

  DEC_API_TRC("H264DecGetAPIVersion# OK\n");

  return ver;
}

/*------------------------------------------------------------------------------
    Function name : H264DecGetBuild
    Description   : Return the SW and HW build information

    Return type   : H264DecBuild
    Argument      : void
------------------------------------------------------------------------------*/
H264DecBuild H264DecGetBuild(void) {
  H264DecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_H264_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_H264_DEC);

  DEC_API_TRC("H264DecGetBuild# OK\n");

  return build_info;
}

/*------------------------------------------------------------------------------

    Function: H264DecNextPicture

        Functional description:
            Get next picture in display order if any available.

        Input:
            dec_inst     decoder instance.
            end_of_stream force output of all buffered pictures

        Output:
            output     pointer to output structure

        Returns:
            DEC_OK            no pictures available for display
            DEC_PIC_RDY       picture available for display
            DEC_PARAM_ERROR     invalid parameters
            DEC_NOT_INITIALIZED   decoder instance not initialized yet

------------------------------------------------------------------------------*/
enum DecRet H264DecNextPicture(H264DecInst dec_inst,
                              H264DecPicture * output, u32 end_of_stream) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  DEC_API_TRC("H264DecNextPicture#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("H264DecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecNextPicture# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  u32 ret = DEC_OK;

  if(dec_cont->dec_stat == DEC_END_OF_STREAM &&
      IsOutputEmpty(&dec_cont->fb_list)) {
    DEC_API_TRC("H264DecNextPicture# DEC_END_OF_STREAM\n");
    return (DEC_END_OF_STREAM);
  }

  if((ret = PeekOutputPic(&dec_cont->fb_list, output))) {
    if(ret == ABORT_MARKER) {
      DEC_API_TRC("H264DecNextPicture# DEC_ABORTED\n");
      return (DEC_ABORTED);
    }
    if(ret == FLUSH_MARKER) {
      DEC_API_TRC("H264DecNextPicture# DEC_FLUSHED\n");
      return (DEC_FLUSHED);
    }

    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);    /* hw build id for register */
    GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

    if (dec_cont->pp_enabled && hw_feature.crop_support) {
      output->crop_params.crop_left_offset = dec_cont->ppu_cfg[0].crop.x;
      output->crop_params.crop_top_offset = dec_cont->ppu_cfg[0].crop.y;
      output->crop_params.crop_out_width = dec_cont->ppu_cfg[0].crop.width;
      output->crop_params.crop_out_height = dec_cont->ppu_cfg[0].crop.height;
    }

    DEC_API_TRC("H264DecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else {
    DEC_API_TRC("H264DecNextPicture# DEC_OK\n");
    return (DEC_OK);
  }
  (void) end_of_stream;
  return (DEC_OK);
}

/*------------------------------------------------------------------------------
    Function name : h264UpdateAfterPictureDecode
    Description   :

    Return type   : void
    Argument      : struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
void h264UpdateAfterPictureDecode(struct H264DecContainer * dec_cont) {

  u32 tmp_ret = HANTRO_OK;
  storage_t *storage = &dec_cont->storage;
  sliceHeader_t *slice_header = storage->slice_header;
  u32 interlaced;
  u32 pic_code_type;
  u32 second_field = 0;
#ifdef SKIP_OPENB_FRAME
  i32 poc;
#endif

  h264bsdResetStorage(storage);

  ASSERT((storage));

  /* determine initial reference picture lists */
  H264InitRefPicList(dec_cont);

  if(storage->slice_header->field_pic_flag == 0)
    storage->curr_image->pic_struct = FRAME;
  else
    storage->curr_image->pic_struct = storage->slice_header->bottom_field_flag;

  if (storage->curr_image->pic_struct < FRAME) {
    if (storage->dpb->current_out->status[(u32)!storage->curr_image->pic_struct] != EMPTY)
      second_field = 1;
  }

  h264GetSarInfo(storage, &storage->curr_image->sar_width, &storage->curr_image->sar_height);

  if(storage->poc->contains_mmco5) {
    u32 tmp;

    tmp = MIN(storage->poc->pic_order_cnt[0], storage->poc->pic_order_cnt[1]);
    storage->poc->pic_order_cnt[0] -= tmp;
    storage->poc->pic_order_cnt[1] -= tmp;
  }

  storage->current_marked = storage->valid_slice_in_access_unit;

  /* Setup tiled mode for picture before updating DPB */
  interlaced = !dec_cont->storage.active_sps->frame_mbs_only_flag;
  if( dec_cont->tiled_mode_support) {
    dec_cont->tiled_reference_enable =
      DecSetupTiledReference( dec_cont->h264_regs,
                              dec_cont->tiled_mode_support,
                              dec_cont->dpb_mode,
                              interlaced );
  } else {
    dec_cont->tiled_reference_enable = 0;
  }
  if(storage->valid_slice_in_access_unit) {
    if(IS_I_SLICE(slice_header->slice_type))
      pic_code_type = DEC_PIC_TYPE_I;
    else if(IS_P_SLICE(slice_header->slice_type))
      pic_code_type = DEC_PIC_TYPE_P;
    else
      pic_code_type = DEC_PIC_TYPE_B;

#ifdef SKIP_OPENB_FRAME
    if (!dec_cont->first_entry_point) {
      if(storage->curr_image->pic_struct < FRAME) {
        if (!second_field)
          dec_cont->entry_is_IDR = IS_IDR_NAL_UNIT(storage->prev_nal_unit);
        else {
          dec_cont->entry_POC = MIN(storage->poc->pic_order_cnt[0],
                                   storage->poc->pic_order_cnt[1]);
          dec_cont->first_entry_point = 1;
        }
      } else {
        dec_cont->entry_is_IDR = IS_IDR_NAL_UNIT(storage->prev_nal_unit);
        dec_cont->entry_POC = MIN(storage->poc->pic_order_cnt[0],
                                 storage->poc->pic_order_cnt[1]);
        dec_cont->first_entry_point = 1;
      }
    }
    if (dec_cont->skip_b < 2) {
      if (storage->curr_image->pic_struct < FRAME) {
        if(second_field) {
          if((pic_code_type == DEC_PIC_TYPE_I) || (pic_code_type == DEC_PIC_TYPE_P) ||
             (storage->dpb->current_out->pic_code_type[(u32)!storage->curr_image->pic_struct] == DEC_PIC_TYPE_I) ||
             (storage->dpb->current_out->pic_code_type[(u32)!storage->curr_image->pic_struct] == DEC_PIC_TYPE_P))
            dec_cont->skip_b++;
          else {
            poc = MIN(storage->poc->pic_order_cnt[0],
                      storage->poc->pic_order_cnt[1]);
            if(!dec_cont->entry_is_IDR && (poc < dec_cont->entry_POC))
              storage->dpb->current_out->openB_flag = 1;
          }
        } else {
          /* In case the open B frame is output before the second field is ready,
             that is, before openB_flag is set, it may be output wrongly. */
          poc = storage->poc->pic_order_cnt[storage->curr_image->pic_struct];
          if(!dec_cont->entry_is_IDR && (poc < dec_cont->entry_POC))
            storage->dpb->current_out->openB_flag = 1;
        }
      } else {
        if((pic_code_type == DEC_PIC_TYPE_I) || (pic_code_type == DEC_PIC_TYPE_P))
          dec_cont->skip_b++;
        else {
          poc = MIN(storage->poc->pic_order_cnt[0],
                    storage->poc->pic_order_cnt[1]);
          if(!dec_cont->entry_is_IDR && (poc < dec_cont->entry_POC))
            storage->dpb->current_out->openB_flag = 1;
        }
      }
    }
#endif

    if(storage->prev_nal_unit->nal_ref_idc) {
      tmp_ret = h264bsdMarkDecRefPic(storage->dpb,
                                     &slice_header->dec_ref_pic_marking,
                                     storage->curr_image,
                                     slice_header->frame_num,
                                     storage->poc->pic_order_cnt,
                                     IS_IDR_NAL_UNIT(storage->prev_nal_unit) ?
                                     HANTRO_TRUE : HANTRO_FALSE,
                                     storage->current_pic_id,
                                     storage->num_concealed_mbs,
                                     dec_cont->tiled_reference_enable,
                                     pic_code_type);
    } else {
      /* non-reference picture, just store for possible display
       * reordering */
      tmp_ret = h264bsdMarkDecRefPic(storage->dpb, NULL,
                                     storage->curr_image,
                                     slice_header->frame_num,
                                     storage->poc->pic_order_cnt,
                                     HANTRO_FALSE,
                                     storage->current_pic_id,
                                     storage->num_concealed_mbs,
                                     dec_cont->tiled_reference_enable,
                                     pic_code_type);
    }

    if (tmp_ret != HANTRO_OK && storage->view == 0)
      storage->second_field = 0;

    if(storage->dpb->delayed_out == 0) {
      h264DpbUpdateOutputList(storage->dpb);

      if (storage->view == 0)
        storage->last_base_num_out = storage->dpb->num_out;
      /* make sure that there are equal number of output pics available
       * for both views */
      else if (storage->dpb->num_out < storage->last_base_num_out)
        h264DpbAdjStereoOutput(storage->dpb, storage->last_base_num_out);
      else if (storage->last_base_num_out &&
               storage->last_base_num_out + 1 < storage->dpb->num_out)
        h264DpbAdjStereoOutput(storage->dpbs[0],
                               storage->dpb->num_out - 1);
      else if (storage->last_base_num_out == 0 && storage->dpb->num_out)
        h264DpbAdjStereoOutput(storage->dpbs[0],
                               storage->dpb->num_out);

      /* check if current_out already in output buffer and second
       * field to come -> delay output */
      if(storage->curr_image->pic_struct != FRAME &&
          (storage->view == 0 ? storage->second_field :
           !storage->base_opposite_field_pic)) {
        u32 i, tmp;

        tmp = storage->dpb->out_index_r;
        for (i = 0; i < storage->dpb->num_out; i++, tmp++) {
          if (tmp == storage->dpb->dpb_size + 1)
            tmp = 0;

          if(storage->dpb->current_out->data ==
              storage->dpb->out_buf[tmp].data) {
            storage->dpb->delayed_id = tmp;
            DEBUG_PRINT(
              ("h264UpdateAfterPictureDecode: Current frame in output list; "));
            storage->dpb->delayed_out = 1;
            break;
          }
        }
      }
    } else {
      if (!storage->dpb->no_reordering)
        h264DpbUpdateOutputList(storage->dpb);
      DEBUG_PRINT(
        ("h264UpdateAfterPictureDecode: Output all delayed pictures!\n"));
      storage->dpb->delayed_out = 0;
      storage->dpb->current_out->to_be_displayed = 0;   /* remove it from output list */
    }

  } else {
    storage->dpb->delayed_out = 0;
    storage->second_field = 0;
  }

  /*after the mark, if the pic index can be re-use((!dpb->current_out->to_be_displayed && !IS_REFERENCE_F(*(dpb->current_out))), the pp buffer should return*/
  if (storage->dpb->current_out)
    RemoveUnmarkedPpBuffer(storage->dpb);
  if ((storage->valid_slice_in_access_unit && tmp_ret == HANTRO_OK) ||
      storage->view)
    storage->next_view ^= 0x1;
  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
  storage->aso_detected = 0;
}

/*------------------------------------------------------------------------------
    Function name : h264SpsSupported
    Description   :

    Return type   : u32
    Argument      : const struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
u32 h264SpsSupported(const struct H264DecContainer * dec_cont) {
  const seqParamSet_t *sps = dec_cont->storage.active_sps;

  if(sps == NULL)
    return 0;

  /* check picture size */
  if(sps->pic_width_in_mbs * 16 > dec_cont->max_dec_pic_width ||
     sps->pic_height_in_mbs * 16 > dec_cont->max_dec_pic_height ||
     sps->pic_width_in_mbs < 3 || sps->pic_height_in_mbs < 3) {
    DEBUG_PRINT(("Picture size not supported!\n"));
    return 0;
  }

#ifdef DTRC_MIN_SIZE_LIMITED
  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */
  if(dec_cont->tiled_mode_support) {
    if (sps->pic_width_in_mbs < 6) {
      DEBUG_PRINT(("Picture size not supported!\n"));
      return 0;
    }
  }
#endif

  if(dec_cont->h264_profile_support == H264_BASELINE_PROFILE) {
    if(sps->frame_mbs_only_flag != 1) {
      DEBUG_PRINT(("INTERLACED!!! Not supported in baseline decoder\n"));
      return 0;
    }
    if(sps->chroma_format_idc != 1) {
      DEBUG_PRINT(("CHROMA!!! Only 4:2:0 supported in baseline decoder\n"));
      return 0;
    }
    if(sps->scaling_matrix_present_flag != 0) {
      DEBUG_PRINT(("SCALING Matrix!!! Not supported in baseline decoder\n"));
      return 0;
    }
  }
  if (sps->bit_depth_luma > 8 || sps->bit_depth_chroma > 8) {
    if (!dec_cont->h264_high10p_support) {
      DEBUG_PRINT(("BITDEPTH > 8!!! Only supported in High10 progressive decoder\n"));
      return 0;
    }
    if (sps->frame_mbs_only_flag != 1) {
      DEBUG_PRINT(("INTERLACED!!! Not supported in High10 progressive decoder\n"));
      return 0;
    }
  }
  if (sps->frame_mbs_only_flag != 1) {
    if (dec_cont->h264_profile_support <= H264_BASELINE_PROFILE) {
      DEBUG_PRINT(("INTERLACED!!! Not supported in baseline or High10 progressive decoder\n"));
      return 0;
    }
  }

  return 1;
}

/*------------------------------------------------------------------------------
    Function name : h264PpsSupported
    Description   :

    Return type   : u32
    Argument      : const struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
u32 h264PpsSupported(const struct H264DecContainer * dec_cont) {
  const picParamSet_t *pps = dec_cont->storage.active_pps;

  if(dec_cont->h264_profile_support == H264_BASELINE_PROFILE) {
    if(pps->entropy_coding_mode_flag != 0) {
      DEBUG_PRINT(("CABAC!!! Not supported in baseline decoder\n"));
      return 0;
    }
    if(pps->weighted_pred_flag != 0 || pps->weighted_bi_pred_idc != 0) {
      DEBUG_PRINT(("WEIGHTED Pred!!! Not supported in baseline decoder\n"));
      return 0;
    }
    if(pps->transform8x8_flag != 0) {
      DEBUG_PRINT(("TRANSFORM 8x8!!! Not supported in baseline decoder\n"));
      return 0;
    }
    if(pps->scaling_matrix_present_flag != 0) {
      DEBUG_PRINT(("SCALING Matrix!!! Not supported in baseline decoder\n"));
      return 0;
    }
  }
  return 1;
}

/*------------------------------------------------------------------------------
    Function name   : h264StreamIsBaseline
    Description     :
    Return type     : u32
    Argument        : const struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
u32 h264StreamIsBaseline(const struct H264DecContainer * dec_cont) {
  const picParamSet_t *pps = dec_cont->storage.active_pps;
  const seqParamSet_t *sps = dec_cont->storage.active_sps;

  if(sps->frame_mbs_only_flag != 1) {
    return 0;
  }
  if(sps->chroma_format_idc != 1) {
    return 0;
  }
  if(sps->scaling_matrix_present_flag != 0) {
    return 0;
  }
  if(pps->entropy_coding_mode_flag != 0) {
    return 0;
  }
  if(pps->weighted_pred_flag != 0 || pps->weighted_bi_pred_idc != 0) {
    return 0;
  }
  if(pps->transform8x8_flag != 0) {
    return 0;
  }
  if(pps->scaling_matrix_present_flag != 0) {
    return 0;
  }
  return 1;
}

/*------------------------------------------------------------------------------
    Function name : h264AllocateResources
    Description   :

    Return type   : u32
    Argument      : struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
u32 h264AllocateResources(struct H264DecContainer * dec_cont) {
  u32 ret, mbs_in_pic;
  DecAsicBuffers_t *asic = dec_cont->asic_buff;
  storage_t *storage = &dec_cont->storage;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  const seqParamSet_t *sps = storage->active_sps;
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (!hw_feature.pic_size_reg_unified) {
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_MB_WIDTH, sps->pic_width_in_mbs);
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_MB_HEIGHT_P,
                   sps->pic_height_in_mbs);
    SetDecRegister(dec_cont->h264_regs, HWIF_AVS_H264_H_EXT,
                   sps->pic_height_in_mbs >> 8);
  } else {
    SetDecRegister(dec_cont->h264_regs, HWIF_MIN_CB_SIZE, 3);
    SetDecRegister(dec_cont->h264_regs, HWIF_MAX_CB_SIZE, 4);
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_WIDTH_IN_CBS,
                   sps->pic_width_in_mbs << 1);
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_HEIGHT_IN_CBS,
                   sps->pic_height_in_mbs << 1);

#if 1
    /* Not used in G2_H264. */
    SetDecRegister(dec_cont->h264_regs, HWIF_PARTIAL_CTB_X, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_PARTIAL_CTB_Y, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_WIDTH_4X4,
                   sps->pic_width_in_mbs << 2);
    SetDecRegister(dec_cont->h264_regs, HWIF_PIC_HEIGHT_4X4,
                   sps->pic_height_in_mbs << 2);
#endif
  }

  ReleaseAsicBuffers(dec_cont->dwl, asic);

  ret = AllocateAsicBuffers(dec_cont, asic, storage->pic_size_in_mbs);
  if(ret == 0) {
    SET_ADDR_REG(dec_cont->h264_regs, HWIF_INTRA_4X4_BASE,
                 asic->intra_pred.bus_address);
    SET_ADDR_REG(dec_cont->h264_regs, HWIF_DIFF_MV_BASE,
                 asic->mv.bus_address);

    if(dec_cont->rlc_mode) {
      /* release any previously allocated stuff */
      FREE(storage->mb);
      FREE(storage->slice_group_map);

      mbs_in_pic = sps->pic_width_in_mbs * sps->pic_height_in_mbs;

      DEBUG_PRINT(("ALLOCATE storage->mb            - %8d bytes\n",
                   mbs_in_pic * sizeof(mbStorage_t)));
      storage->mb = DWLcalloc(mbs_in_pic, sizeof(mbStorage_t));

      DEBUG_PRINT(("ALLOCATE storage->slice_group_map - %8d bytes\n",
                   mbs_in_pic * sizeof(u32)));

      ALLOCATE(storage->slice_group_map, mbs_in_pic, u32);

      if(storage->mb == NULL || storage->slice_group_map == NULL) {
        ret = MEMORY_ALLOCATION_ERROR;
      } else {
        h264bsdInitMbNeighbours(storage->mb, sps->pic_width_in_mbs,
                                storage->pic_size_in_mbs);
      }
    } else {
      storage->mb = NULL;
      storage->slice_group_map = NULL;
    }
  }

  return ret;
}

/*------------------------------------------------------------------------------
    Function name : h264InitPicFreezeOutput
    Description   :

    Return type   : u32
    Argument      : struct H264DecContainer * dec_cont
------------------------------------------------------------------------------*/
void h264InitPicFreezeOutput(struct H264DecContainer * dec_cont, u32 from_old_dpb) {

  storage_t *storage = &dec_cont->storage;

  /* for concealment after a HW error report we use the saved reference list */
  dpbStorage_t *dpb = &storage->dpb[from_old_dpb];

  /* update status of decoded image (relevant only for  multi-core) */
  /* current out is always in dpb[0] */
  {
    dpbPicture_t *current_out = storage->dpb->current_out;

    u8 *p_sync_mem = (u8*)current_out->data->virtual_address +
                     dpb->sync_mc_offset;
    h264MCSetRefPicStatus(p_sync_mem, current_out->is_field_pic,
                          current_out->is_bottom_field);
  }

#ifndef _DISABLE_PIC_FREEZE
  u32 index = 0;
  const u8 *ref_data;
  do {
    ref_data = h264bsdGetRefPicDataVlcMode(dpb, dpb->list[index], 0);
    index++;
  } while(index < 16 && ref_data == NULL);
#endif

  /* "freeze" whole picture if not field pic or if opposite field of the
   * field pic does not exist in the buffer */
  if(dec_cont->storage.slice_header->field_pic_flag == 0 ||
      storage->dpb[0].current_out->status[
        !dec_cont->storage.slice_header->bottom_field_flag] == EMPTY) {
    storage->dpb[0].current_out->corrupted_first_field_or_frame = 1;
#ifndef _DISABLE_PIC_FREEZE
    /* reset DMV storage for erroneous pictures */
    if(dec_cont->asic_buff->enable_dmv_and_poc != 0) {
      const u32 dvm_mem_size = storage->pic_size_in_mbs *
                               (dec_cont->high10p_mode ? 80 : 64);
      void * dvm_base = (u8*)storage->curr_image->data->virtual_address +
                        dec_cont->storage.dpb->dir_mv_offset;

      (void) DWLmemset(dvm_base, 0, dvm_mem_size);
    }
    u32 out_w, out_h, size;
    if (dec_cont->tiled_stride_enable) {
      out_w = NEXT_MULTIPLE(4 * storage->active_sps->pic_width_in_mbs * 16, ALIGN(dec_cont->align));
      out_h = storage->active_sps->pic_height_in_mbs * 4;
      size = out_w * out_h * (storage->active_sps->mono_chrome ? 2 : 3) / 2;
    } else {
      out_w = storage->active_sps->pic_width_in_mbs * 16;
      out_h = storage->active_sps->pic_height_in_mbs * 16;
      size = storage->pic_size_in_mbs * (storage->active_sps->mono_chrome ? 256 : 384);
    }
    if(ref_data == NULL) {
      DEBUG_PRINT(("h264InitPicFreezeOutput: pic freeze memset\n"));
      (void) DWLmemset(storage->curr_image->data->
                       virtual_address, 128, size);
      if (storage->enable2nd_chroma &&
          !storage->active_sps->mono_chrome)
        (void) DWLmemset((u8*)storage->curr_image->data->virtual_address +
                         dpb->ch2_offset, 128, out_w * out_h / 2);
    } else {
      DEBUG_PRINT(("h264InitPicFreezeOutput: pic freeze memcopy\n"));
      (void) DWLmemcpy(storage->curr_image->data->virtual_address,
                       ref_data, size);
      if (storage->enable2nd_chroma &&
          !storage->active_sps->mono_chrome)
        (void) DWLmemcpy((u8*)storage->curr_image->data->virtual_address +
                         dpb->ch2_offset, ref_data + dpb->ch2_offset,
                         out_w * out_h / 2);
    }
#endif
  } else {
    if (!storage->dpb[0].current_out->corrupted_first_field_or_frame) {
      storage->dpb[0].current_out->corrupted_second_field = 1;
      if (dec_cont->storage.slice_header->bottom_field_flag != 0)
        storage->dpb[0].current_out->pic_struct = TOPFIELD;
      else
        storage->dpb[0].current_out->pic_struct = BOTFIELD;
    }

#ifndef _DISABLE_PIC_FREEZE
    u32 i;
    u32 field_offset = storage->active_sps->pic_width_in_mbs * 16;
    u8 *lum_base = (u8*)storage->curr_image->data->virtual_address;
    u8 *ch_base = (u8*)storage->curr_image->data->virtual_address + storage->pic_size_in_mbs * 256;
    u8 *ch2_base = (u8*)storage->curr_image->data->virtual_address + dpb->ch2_offset;
    const u8 *ref_ch_data = ref_data + storage->pic_size_in_mbs * 256;
    const u8 *ref_ch2_data = ref_data + dpb->ch2_offset;

    /*Base for second field*/
    u8 *lum_base1 = lum_base;
    u8 *ch_base1 = ch_base;
    u8 *ch2_base1 = ch2_base;

    if (dec_cont->dpb_mode == DEC_DPB_INTERLACED_FIELD) {
      if (dec_cont->storage.slice_header->bottom_field_flag != 0) {
        lum_base1 = lum_base + 256 * storage->pic_size_in_mbs / 2;
        ch_base1 = ch_base + 128 * storage->pic_size_in_mbs / 2;
        ch2_base1 = ch2_base + 128 * storage->pic_size_in_mbs / 2;
      }
      else {
        lum_base = lum_base + 256 * storage->pic_size_in_mbs / 2;
        ch_base = ch_base + 128 * storage->pic_size_in_mbs / 2;
        ch2_base = ch2_base + 128 * storage->pic_size_in_mbs / 2;
      }

      if (ref_data == NULL) {
        (void) DWLmemcpy(lum_base1, lum_base,
                         256 * storage->pic_size_in_mbs / 2);
        if (!storage->active_sps->mono_chrome)
          (void) DWLmemcpy(ch_base1, ch_base,
                           128 * storage->pic_size_in_mbs / 2);
         if (storage->enable2nd_chroma &&
            !storage->active_sps->mono_chrome)
          (void) DWLmemcpy(ch2_base, ch2_base1,
                           128 * storage->pic_size_in_mbs);
      }
      else {
        if (dec_cont->storage.slice_header->bottom_field_flag != 0) {
          ref_data = ref_data + 256 * storage->pic_size_in_mbs / 2;
          ref_ch_data = ref_ch_data + 128 * storage->pic_size_in_mbs / 2;
          ref_ch2_data = ref_ch2_data + 128 * storage->pic_size_in_mbs / 2;
        }
        (void) DWLmemcpy(lum_base1, ref_data,
                         256 * storage->pic_size_in_mbs / 2);
        if (!storage->active_sps->mono_chrome)
          (void) DWLmemcpy(ch_base1, ref_ch_data,
                           128 * storage->pic_size_in_mbs / 2);

        if (storage->enable2nd_chroma &&
            !storage->active_sps->mono_chrome)
          (void) DWLmemcpy(ch2_base1, ref_ch2_data,
                           128 * storage->pic_size_in_mbs);
      }
    }
    else {
    if(dec_cont->storage.slice_header->bottom_field_flag != 0) {
      lum_base += field_offset;
      ch_base += field_offset;
      ch2_base += field_offset;
    }

    if(ref_data == NULL) {
      DEBUG_PRINT(("h264InitPicFreezeOutput: pic freeze memset, one field\n"));

      for(i = 0; i < (storage->active_sps->pic_height_in_mbs*8); i++) {
        (void) DWLmemset(lum_base, 128, field_offset);
        if((dec_cont->storage.active_sps->mono_chrome == 0) && (i & 0x1U)) {
          (void) DWLmemset(ch_base, 128, field_offset);
          ch_base += 2*field_offset;
          if (storage->enable2nd_chroma) {
            (void) DWLmemset(ch2_base, 128, field_offset);
            ch2_base += 2*field_offset;
          }
        }
        lum_base += 2*field_offset;
      }
    } else {
      if(dec_cont->storage.slice_header->bottom_field_flag != 0) {
        ref_data += field_offset;
        ref_ch_data += field_offset;
        ref_ch2_data += field_offset;
      }

      DEBUG_PRINT(("h264InitPicFreezeOutput: pic freeze memcopy, one field\n"));
      for(i = 0; i < (storage->active_sps->pic_height_in_mbs*8); i++) {
        (void) DWLmemcpy(lum_base, ref_data, field_offset);
        if((dec_cont->storage.active_sps->mono_chrome == 0) && (i & 0x1U)) {
          (void) DWLmemcpy(ch_base, ref_ch_data, field_offset);
          ch_base += 2*field_offset;
          ref_ch_data += 2*field_offset;
          if (storage->enable2nd_chroma) {
            (void) DWLmemcpy(ch2_base, ref_ch2_data, field_offset);
            ch2_base += 2*field_offset;
            ref_ch2_data += 2*field_offset;
          }
        }
        lum_base += 2*field_offset;
        ref_data += 2*field_offset;
      }
    }
    }
#endif
  }

  dpb = &storage->dpb[0]; /* update results for current output */

  {
    i32 i = dpb->num_out;
    u32 tmp = dpb->out_index_r;

    while((i--) > 0) {
      if (tmp == dpb->dpb_size + 1)
        tmp = 0;

      if(dpb->out_buf[tmp].data == storage->curr_image->data) {
        dpb->out_buf[tmp].num_err_mbs = storage->pic_size_in_mbs;
        break;
      }
      tmp++;
    }

    i = dpb->dpb_size + 1;

    while((i--) > 0) {
      if(dpb->buffer[i].data == storage->curr_image->data) {
        dpb->buffer[i].num_err_mbs = storage->pic_size_in_mbs;
        ASSERT(&dpb->buffer[i] == dpb->current_out);
        break;
      }
    }

#ifdef SKIP_OPENB_FRAME
    if (IS_I_SLICE(storage->slice_header->slice_type) &&
        (dec_cont->error_handling & DEC_EC_REF_NEXT_I)) {
      /* Decode at next I slice, but there is error in I slice. */
      /* Stop openB procedure. */
      dec_cont->skip_b = 2;
    }
#endif
  }

  dec_cont->storage.num_concealed_mbs = storage->pic_size_in_mbs;

}

/*------------------------------------------------------------------------------
    Function name : bsdDecodeReturn
    Description   :

    Return type   : void
    Argument      : bsd decoder return value
------------------------------------------------------------------------------*/
static void bsdDecodeReturn(u32 retval) {

  DEBUG_PRINT(("H264bsdDecode returned: "));
  switch (retval) {
  case H264BSD_PIC_RDY:
    DEBUG_PRINT(("H264BSD_PIC_RDY\n"));
    break;
  case H264BSD_RDY:
    DEBUG_PRINT(("H264BSD_RDY\n"));
    break;
  case H264BSD_HDRS_RDY:
    DEBUG_PRINT(("H264BSD_HDRS_RDY\n"));
    break;
  case H264BSD_ERROR:
    DEBUG_PRINT(("H264BSD_ERROR\n"));
    break;
  case H264BSD_PARAM_SET_ERROR:
    DEBUG_PRINT(("H264BSD_PARAM_SET_ERROR\n"));
    break;
  case H264BSD_NEW_ACCESS_UNIT:
    DEBUG_PRINT(("H264BSD_NEW_ACCESS_UNIT\n"));
    break;
  case H264BSD_FMO:
    DEBUG_PRINT(("H264BSD_FMO\n"));
    break;
  default:
    DEBUG_PRINT(("UNKNOWN\n"));
    break;
  }
}

/*------------------------------------------------------------------------------
    Function name   : h264GetSarInfo
    Description     : Returns the sample aspect ratio size info
    Return type     : void
    Argument        : storage_t *storage - decoder storage
    Argument        : u32 * sar_width - SAR width returned here
    Argument        : u32 *sar_height - SAR height returned here
------------------------------------------------------------------------------*/
void h264GetSarInfo(const storage_t * storage, u32 * sar_width,
                    u32 * sar_height) {
  switch (h264bsdAspectRatioIdc(storage)) {
  case 0:
    *sar_width = 0;
    *sar_height = 0;
    break;
  case 1:
    *sar_width = 1;
    *sar_height = 1;
    break;
  case 2:
    *sar_width = 12;
    *sar_height = 11;
    break;
  case 3:
    *sar_width = 10;
    *sar_height = 11;
    break;
  case 4:
    *sar_width = 16;
    *sar_height = 11;
    break;
  case 5:
    *sar_width = 40;
    *sar_height = 33;
    break;
  case 6:
    *sar_width = 24;
    *sar_height = 1;
    break;
  case 7:
    *sar_width = 20;
    *sar_height = 11;
    break;
  case 8:
    *sar_width = 32;
    *sar_height = 11;
    break;
  case 9:
    *sar_width = 80;
    *sar_height = 33;
    break;
  case 10:
    *sar_width = 18;
    *sar_height = 11;
    break;
  case 11:
    *sar_width = 15;
    *sar_height = 11;
    break;
  case 12:
    *sar_width = 64;
    *sar_height = 33;
    break;
  case 13:
    *sar_width = 160;
    *sar_height = 99;
    break;
  case 255:
    h264bsdSarSize(storage, sar_width, sar_height);
    break;
  default:
    *sar_width = 0;
    *sar_height = 0;
  }
}

/*------------------------------------------------------------------------------
    Function name   : h264CheckReleasePpAndHw
    Description     : Release HW lock and wait for PP to finish, need to be
                      called if errors/problems after first field of a picture
                      finished and PP left running
    Return type     : void
    Argument        :
    Argument        :
    Argument        :
------------------------------------------------------------------------------*/
void h264CheckReleasePpAndHw(struct H264DecContainer *dec_cont) {
  if (dec_cont->keep_hw_reserved) {
    dec_cont->keep_hw_reserved = 0;
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }

}

/*------------------------------------------------------------------------------

    Function: H264DecPeek

        Functional description:
            Get last decoded picture if any available. No pictures are removed
            from output nor DPB buffers.

        Input:
            dec_inst     decoder instance.

        Output:
            output     pointer to output structure

        Returns:
            DEC_OK            no pictures available for display
            DEC_PIC_RDY       picture available for display
            DEC_PARAM_ERROR   invalid parameters

------------------------------------------------------------------------------*/
enum DecRet H264DecPeek(H264DecInst dec_inst, H264DecPicture * output) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  dpbPicture_t *current_out = dec_cont->storage.dpb->current_out;
  u32 bit_depth = 0;
  u32 i = 0;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 *output_picture = NULL;

  DEC_API_TRC("H264DecPeek#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("H264DecPeek# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_stat != DEC_NEW_HEADERS &&
      dec_cont->storage.dpb->fullness && current_out != NULL &&
      (current_out->status[0] != EMPTY || current_out->status[1] != EMPTY)) {

    bit_depth = (current_out->bit_depth_luma == 8 && current_out->bit_depth_chroma == 8) ? 8 : 10;

    if (!dec_cont->pp_enabled) {
      output->pictures[0].output_picture = current_out->data->virtual_address;
      output->pictures[0].output_picture_bus_address = current_out->data->bus_address;
      output->pictures[0].output_format = dec_cont->use_video_compressor ? DEC_OUT_FRM_RFC :
          (current_out->tiled_mode ? DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN);
      output->pictures[0].pic_width = h264bsdPicWidth(&dec_cont->storage) << 4;
      output->pictures[0].pic_height = h264bsdPicHeight(&dec_cont->storage) << 4;
      if (output->pictures[0].output_format != DEC_OUT_FRM_RFC &&
          dec_cont->tiled_stride_enable)
        output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].pic_width *
                                         bit_depth * 4, ALIGN(dec_cont->align) * 8) / 8;
      else
        output->pictures[0].pic_stride = output->pictures[0].pic_width * bit_depth * 4 / 8;
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
      
      if (current_out->mono_chrome) {
        output->pictures[0].output_picture_chroma = NULL;
        output->pictures[0].output_picture_chroma_bus_address = 0;
      } else {
        output->pictures[0].output_picture_chroma = output->pictures[0].output_picture +
                           output->pictures[0].pic_stride * output->pictures[0].pic_height / 16;
        output->pictures[0].output_picture_chroma_bus_address =
            output->pictures[0].output_picture_bus_address +
            output->pictures[0].pic_stride * output->pictures[0].pic_height / 4;
      }
    } else {
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        output->pictures[i].output_picture = current_out->ds_data->virtual_address + ppu_cfg->luma_offset;
        output->pictures[i].output_picture_bus_address = current_out->ds_data->bus_address + ppu_cfg->luma_offset;
        if (dec_cont->ppu_cfg[i].monochrome) {
          output->pictures[i].output_picture_chroma = NULL;
          output->pictures[i].output_picture_chroma_bus_address = 0;
        } else {
          output->pictures[i].output_picture_chroma = current_out->ds_data->virtual_address + ppu_cfg->chroma_offset;
          output->pictures[i].output_picture_chroma_bus_address = current_out->ds_data->bus_address + ppu_cfg->chroma_offset;
        }
        if (output_picture == NULL)
          output_picture = (u32 *)output->pictures[i].output_picture;
        output->pictures[i].output_format = TransUnitConfig2Format(&dec_cont->ppu_cfg[i]);
        if (ppu_cfg->crop2.enabled) {
          output->pictures[i].pic_width = dec_cont->ppu_cfg[i].crop2.width;
          output->pictures[i].pic_height = dec_cont->ppu_cfg[i].crop2.height;
        } else {
          output->pictures[i].pic_width = dec_cont->ppu_cfg[i].scale.width;
          output->pictures[i].pic_height = dec_cont->ppu_cfg[i].scale.height;
        }
        output->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
        output->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
      }
    }
    
    output->pic_id = current_out->pic_id;
    output->pic_coding_type[0] = current_out->pic_code_type[0];
    output->pic_coding_type[1] = current_out->pic_code_type[1];
    output->is_idr_picture[0] = current_out->is_idr[0];
    output->is_idr_picture[1] = current_out->is_idr[1];
    output->decode_id[0] = current_out->decode_id[0];
    output->decode_id[1] = current_out->decode_id[1];
    output->nbr_of_err_mbs = current_out->num_err_mbs;

    output->interlaced = dec_cont->storage.dpb->interlaced;
    output->field_picture = current_out->is_field_pic;
    
    output->top_field = 0;
    output->pic_struct = current_out->pic_struct;

    if (output->field_picture) {
      /* just one field in buffer -> that is the output */
      if(current_out->status[0] == EMPTY || current_out->status[1] == EMPTY) {
        output->top_field = (current_out->status[0] == EMPTY) ? 0 : 1;
      }
      /* both fields decoded -> check field parity from slice header */
      else
        output->top_field =
          dec_cont->storage.slice_header->bottom_field_flag == 0;
    } else
      output->top_field = 1;

    output->crop_params = current_out->crop;


    DEC_API_TRC("H264DecPeek# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else {
    DEC_API_TRC("H264DecPeek# DEC_OK\n");
    return (DEC_OK);
  }

}
/*------------------------------------------------------------------------------

    Function: H264DecSetMvc()

        Functional description:
            This function configures decoder to decode both views of MVC
            stereo high profile compliant streams.

        Inputs:
            dec_inst     decoder instance

        Outputs:

        Returns:
            DEC_OK            success
            DEC_PARAM_ERROR   invalid parameters
            DEC_NOT_INITIALIZED   decoder instance not initialized yet

------------------------------------------------------------------------------*/
enum DecRet H264DecSetMvc(H264DecInst dec_inst) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  DWLHwConfig hw_cfg;

  DEC_API_TRC("H264DecSetMvc#");

  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecSetMvc# ERROR: dec_inst is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecSetMvc# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_H264_DEC);
  if(!hw_cfg.mvc_support) {
    DEC_API_TRC("H264DecSetMvc# ERROR: H264 MVC not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  dec_cont->storage.mvc = HANTRO_TRUE;

  DEC_API_TRC("H264DecSetMvc# OK\n");

  return (DEC_OK);
}

/*------------------------------------------------------------------------------

    Function: H264DecPictureConsumed()

        Functional description:
            Release the frame displayed and sent by APP

        Inputs:
            dec_inst     Decoder instance

            picture    pointer of picture structure to be released

        Outputs:
            none

        Returns:
            DEC_PARAM_ERROR       Decoder instance or picture is null
            DEC_NOT_INITIALIZED   Decoder instance isn't initialized
            DEC_OK                picture release success

------------------------------------------------------------------------------*/
enum DecRet H264DecPictureConsumed(H264DecInst dec_inst,
                                  const H264DecPicture *picture) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  const dpbStorage_t *dpb;
  u32 id = FB_NOT_VALID_ID, i;
  u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;

  DEC_API_TRC("H264DecPictureConsumed#\n");

  if(dec_inst == NULL || picture == NULL) {
    DEC_API_TRC("H264DecPictureConsumed# ERROR: dec_inst or picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    /* find the mem descriptor for this specific buffer, base view first */
    dpb = dec_cont->storage.dpbs[0];
    for(i = 0; i < dpb->tot_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address == dpb->pic_buffers[i].bus_address &&
          picture->pictures[0].output_picture == dpb->pic_buffers[i].virtual_address) {
        id = i;
        break;
      }
    }

    /* if not found, search other view for MVC mode */
    if(id == FB_NOT_VALID_ID && dec_cont->storage.mvc == HANTRO_TRUE) {
      dpb = dec_cont->storage.dpbs[1];
      /* find the mem descriptor for this specific buffer */
      for(i = 0; i < dpb->tot_buffers; i++) {
        if(picture->pictures[0].output_picture_bus_address == dpb->pic_buffers[i].bus_address &&
            picture->pictures[0].output_picture == dpb->pic_buffers[i].virtual_address) {
          id = i;
          break;
        }
      }
    }

    if(id == FB_NOT_VALID_ID)
      return DEC_PARAM_ERROR;

    PopOutputPic(&dec_cont->fb_list, dpb->pic_buff_id[id]);
  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (u32 *)picture->pictures[i].output_picture;
        break;
      }
    }
    InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
  }

  return DEC_OK;
}


/*------------------------------------------------------------------------------

    Function: H264DecNextPicture_INTERNAL

        Functional description:
            Push next picture in display order into output list if any available.

        Input:
            dec_inst     decoder instance.
            end_of_stream force output of all buffered pictures

        Output:
            output     pointer to output structure

        Returns:
            DEC_OK            no pictures available for display
            DEC_PIC_RDY       picture available for display
            DEC_PARAM_ERROR     invalid parameters
            DEC_NOT_INITIALIZED   decoder instance not initialized yet

------------------------------------------------------------------------------*/
enum DecRet H264DecNextPicture_INTERNAL(H264DecInst dec_inst,
                                       H264DecPicture * output,
                                       u32 end_of_stream) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  const dpbOutPicture_t *out_pic = NULL;
  u32 bit_depth;
  dpbStorage_t *out_dpb;
  storage_t *storage;
  sliceHeader_t *p_slice_hdr;
  u32 i;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 *output_picture = NULL;
#ifdef SUPPORT_DEC400
  const u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address=0;
  u32 tile_status_address_offset = 0;
#endif

  DEC_API_TRC("H264DecNextPicture_INTERNAL#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("H264DecNextPicture_INTERNAL# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;
  p_slice_hdr = storage->slice_header;
  out_dpb = dec_cont->storage.dpbs[dec_cont->storage.out_view];

  /* if display order is the same as decoding order and PP is used and
   * cannot be used in pipeline (rotation) -> do not perform PP here but
   * while decoding next picture (parallel processing instead of
   * DEC followed by PP followed by DEC...) */
  if (dec_cont->storage.pending_out_pic) {
    out_pic = dec_cont->storage.pending_out_pic;
    dec_cont->storage.pending_out_pic = NULL;
  } else if(out_dpb->no_reordering == 0) {
    if(!out_dpb->delayed_out) {
      dpbStorage_t *current_dpb = dec_cont->storage.dpb;
      dec_cont->storage.dpb =
        dec_cont->storage.dpbs[dec_cont->storage.out_view];

      out_pic = h264bsdNextOutputPicture(&dec_cont->storage);

      if ( (dec_cont->storage.num_views ||
            dec_cont->storage.out_view) && out_pic != NULL) {
        output->view_id =
          dec_cont->storage.view_id[dec_cont->storage.out_view];
        dec_cont->storage.out_view ^= 0x1;
      }
      dec_cont->storage.dpb = current_dpb;
    }
  } else {
    /* no reordering of output pics AND stereo was activated after base
     * picture was output -> output stereo view pic if available */
    if (dec_cont->storage.num_views &&
        dec_cont->storage.view && dec_cont->storage.out_view == 0 &&
        out_dpb->num_out == 0 &&
        dec_cont->storage.dpbs[dec_cont->storage.view]->num_out > 0) {
      dec_cont->storage.out_view ^= 0x1;
      out_dpb = dec_cont->storage.dpbs[dec_cont->storage.out_view];
    }

    if(!end_of_stream &&
        ((out_dpb->num_out == 1 && out_dpb->delayed_out) ||
         (p_slice_hdr->field_pic_flag && storage->second_field))) {
    } else {
      dec_cont->storage.dpb =
        dec_cont->storage.dpbs[dec_cont->storage.out_view];

      out_pic = h264bsdNextOutputPicture(&dec_cont->storage);

      output->view_id =
        dec_cont->storage.view_id[dec_cont->storage.out_view];
      if ( (dec_cont->storage.num_views ||
            dec_cont->storage.out_view) && out_pic != NULL)
        dec_cont->storage.out_view ^= 0x1;
    }
  }

  if(out_pic != NULL) {
    if (!dec_cont->storage.num_views)
      output->view_id = 0;

    bit_depth = (out_pic->bit_depth_luma == 8 && out_pic->bit_depth_chroma == 8) ? 8 : 10;

    if (!dec_cont->pp_enabled) {
      output_picture = out_pic->data->virtual_address;
      output->pictures[0].output_picture = out_pic->data->virtual_address;
      output->pictures[0].output_picture_bus_address = out_pic->data->bus_address;
      output->pictures[0].output_format = dec_cont->use_video_compressor ? DEC_OUT_FRM_RFC :
        (out_pic->tiled_mode ? DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN);
      output->pictures[0].pic_width = out_pic->pic_width;
      output->pictures[0].pic_height = out_pic->pic_height;
      if (output->pictures[0].output_format != DEC_OUT_FRM_RFC &&
          dec_cont->tiled_stride_enable)
        output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].pic_width *
                                           bit_depth * 4, ALIGN(dec_cont->align) * 8) / 8;
      else
        output->pictures[0].pic_stride = output->pictures[0].pic_width * bit_depth * 4 / 8;
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
      if (out_pic->mono_chrome) {
        output->pictures[0].output_picture_chroma = NULL;
        output->pictures[0].output_picture_chroma_bus_address = 0;
      } else {
        output->pictures[0].output_picture_chroma = output->pictures[0].output_picture +
                           output->pictures[0].pic_stride * output->pictures[0].pic_height / 16;
        output->pictures[0].output_picture_chroma_bus_address =
          output->pictures[0].output_picture_bus_address +
          output->pictures[0].pic_stride * output->pictures[0].pic_height / 4;
      }
    } else {
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        output->pictures[i].output_picture = (u32*)((addr_t)out_pic->pp_data->virtual_address + ppu_cfg->luma_offset);
        output->pictures[i].output_picture_bus_address = out_pic->pp_data->bus_address + ppu_cfg->luma_offset;
        if (dec_cont->ppu_cfg[i].monochrome) {
          output->pictures[i].output_picture_chroma = NULL;
          output->pictures[i].output_picture_chroma_bus_address = 0;
        } else {
          output->pictures[i].output_picture_chroma = (u32*)((addr_t)out_pic->pp_data->virtual_address + ppu_cfg->chroma_offset);
          output->pictures[i].output_picture_chroma_bus_address = out_pic->pp_data->bus_address + ppu_cfg->chroma_offset;
        }
        if (output_picture == NULL)
          output_picture = (u32 *)output->pictures[i].output_picture;
        output->pictures[i].output_format = TransUnitConfig2Format(&dec_cont->ppu_cfg[i]);
#ifdef SUPPORT_DEC400
        if (output->pictures[i].output_format == DEC_OUT_FRM_TILED_4X4) {
          tile_status_address_offset += ppu_cfg->ystride * (NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4);
          if (!dec_cont->ppu_cfg[i].monochrome)
            tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4) / 4);
        } else if (dec_cont->ppu_cfg[i].planar) {
          tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
          if (!dec_cont->ppu_cfg[i].monochrome)
            tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
        } else {
          tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
          if (!dec_cont->ppu_cfg[i].monochrome)
            tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height/2;
        }
        if(tile_status_bus_address == 0) {
          tile_status_virtual_address = output->pictures[i].output_picture;
          tile_status_bus_address = output->pictures[i].output_picture_bus_address;
        }
#endif
        if (ppu_cfg->crop2.enabled) {
          output->pictures[i].pic_width = dec_cont->ppu_cfg[i].crop2.width;
          output->pictures[i].pic_height = dec_cont->ppu_cfg[i].crop2.height;
        } else {
          output->pictures[i].pic_width = dec_cont->ppu_cfg[i].scale.width;
          output->pictures[i].pic_height = dec_cont->ppu_cfg[i].scale.height;
        }
        output->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
        output->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
      }
#ifdef SUPPORT_DEC400
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        output->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
        output->pictures[i].luma_table.size = NEXT_MULTIPLE((ppu_cfg->ystride * ppu_cfg->scale.height / 256 * 4 + 7) / 8, 16);
        if (!ppu_cfg->monochrome) {
          output->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
          output->pictures[i].chroma_table.size = NEXT_MULTIPLE((ppu_cfg->cstride * ppu_cfg->scale.height / 2 / 256 * 4 + 7) / 8, 16);
        }
      }
#endif
    }

    output->sar_width = out_pic->sar_width;
    output->sar_height = out_pic->sar_height;
    output->pic_id = out_pic->pic_id;
    output->pic_coding_type[0] = out_pic->pic_code_type[0];
    output->pic_coding_type[1] = out_pic->pic_code_type[1];
    output->is_idr_picture[0] = out_pic->is_idr[0];
    output->is_idr_picture[1] = out_pic->is_idr[1];
    output->decode_id[0] = out_pic->decode_id[0];
    output->decode_id[1] = out_pic->decode_id[1];
    output->nbr_of_err_mbs = out_pic->num_err_mbs;

    output->interlaced = out_pic->interlaced;
    output->field_picture = out_pic->field_picture;
    output->top_field = out_pic->top_field;
    output->bit_depth_luma = out_pic->bit_depth_luma;
    output->bit_depth_chroma = out_pic->bit_depth_chroma;
    output->cycles_per_mb = out_pic->cycles_per_mb;

    output->crop_params = out_pic->crop;
    if (output->field_picture)
      output->pic_struct = output->top_field ? TOPFIELD : BOTFIELD;
    else
      output->pic_struct = out_pic->pic_struct;

    DEC_API_TRC("H264DecNextPicture_INTERNAL# DEC_PIC_RDY\n");

    u32 discard_error_pic = (output->nbr_of_err_mbs &&
        ((dec_cont->error_handling & DEC_EC_OUT_FIRST_FIELD_OK) &&
          !out_pic->corrupted_second_field)) ||
        ((output->nbr_of_err_mbs || output->field_picture) &&
         (dec_cont->error_handling & DEC_EC_OUT_NO_ERROR));
#ifdef SKIP_OPENB_FRAME
    if (out_pic->is_openb)
      discard_error_pic = 1;
#endif
    if (discard_error_pic) {
      ClearOutput(&dec_cont->fb_list, out_pic->mem_idx);
      if (dec_cont->pp_enabled)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
    } else {
      if (dec_cont->pp_enabled)
        InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue, output_picture);
      PushOutputPic(&dec_cont->fb_list, output, out_pic->mem_idx);
    }

    /* Consume reference buffer when only output pp buffer. */
    if (dec_cont->pp_enabled) {
      if (!discard_error_pic) {
        //InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue, output->output_picture);
        PopOutputPic(&dec_cont->fb_list, out_pic->mem_idx);
      }
    }

    return (DEC_PIC_RDY);
  } else {
    DEC_API_TRC("H264DecNextPicture_INTERNAL# DEC_OK\n");
    return (DEC_OK);
  }
}


void H264CycleCount(struct H264DecContainer * dec_cont) {
  dpbStorage_t *dpb = dec_cont->storage.dpb;
  dpbPicture_t *current_out = dpb->current_out;
  u32 cycles = 0;
  u32 mbs = h264bsdPicWidth(&dec_cont->storage) *
             h264bsdPicHeight(&dec_cont->storage);
  if (mbs)
    cycles = GetDecRegister(dec_cont->h264_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  current_out->cycles_per_mb = cycles;

  if(dpb->no_reordering) {
    dpbOutPicture_t *dpb_out = &dpb->out_buf[(dpb->out_index_w + dpb->dpb_size) % (dpb->dpb_size + 1)];
    dpb_out->cycles_per_mb = cycles;
  }
}


enum DecRet H264DecEndOfStream(H264DecInst dec_inst, u32 strm_end_flag) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 count = 0;
  i32 core_id[MAX_ASIC_CORES];

  DEC_API_TRC("H264DecEndOfStream#\n");

  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecEndOfStream# ERROR: dec_inst is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecEndOfStream# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  pthread_mutex_lock(&dec_cont->protect_mutex);
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl);
  } else {
    if (dec_cont->n_cores > 1) {
      /* Check all Core in idle state */
      for(count = 0; count < dec_cont->n_cores_available; count++) {
        DWLReserveHw(dec_cont->dwl, &core_id[count], DWL_CLIENT_TYPE_H264_DEC);
      }
      /* All HW Core finished */
      for(count = 0; count < dec_cont->n_cores_available; count++) {
        DWLReleaseHw(dec_cont->dwl, core_id[count]);
      }
    } else if(dec_cont->asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->h264_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->h264_regs[1] | DEC_IRQ_DISABLE);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->asic_running = 0;

      /* Decrement usage for DPB buffers */
      DecrementDPBRefCount(&dec_cont->storage.dpb[1]);
      dec_cont->dec_stat = DEC_INITIALIZED;
      h264InitPicFreezeOutput(dec_cont, 1);
    } else if (dec_cont->keep_hw_reserved) {
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->keep_hw_reserved = 0;
    }
  }
  /* flush any remaining pictures form DPB */
  h264bsdFlushBuffer(&dec_cont->storage);

  FinalizeOutputAll(&dec_cont->fb_list);

  count = 0;
  {
    H264DecPicture output;
    memset(&output, 0, sizeof(H264DecPicture));
    while(H264DecNextPicture_INTERNAL(dec_inst, &output, 1) == DEC_PIC_RDY) {
      memset(&output, 0, sizeof(H264DecPicture));
      count++;
    }
  }

  /* After all output pictures were pushed, update decoder status to
   * reflect the end-of-stream situation. This way the H264DecMCNextPicture
   * will not block anymore once all output was handled.
   */
  if(strm_end_flag)
    dec_cont->dec_stat = DEC_END_OF_STREAM;

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, -1);

  /* TODO(atna): should it be enough to wait until all cores idle and
   *             not that output is empty !?
   */
#ifndef H264_EXT_BUF_SAFE_RELEASE
  if (strm_end_flag) {
    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_FRAME_BUFFER_NUMBER; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
    if (dec_cont->pp_enabled)
      InputQueueReturnAllBuffer(dec_cont->pp_buffer_queue);
  }
#endif
#ifdef _HAVE_PTHREAD_H
  WaitListNotInUse(&dec_cont->fb_list);
#else
  int ret = 0;
  ret = WaitListNotInUse(&dec_cont->fb_list);
  (void)ret;
#endif
  if (dec_cont->pp_enabled)
    InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  DEC_API_TRC("H264DecEndOfStream# DEC_OK\n");
  return (DEC_OK);
}


void h264SetExternalBufferInfo(H264DecInst dec_inst, storage_t *storage) {
  u32 pic_size_in_mbs = 0, pic_size = 0, dmv_mem_size = 0, ref_buff_size = 0, out_w = 0, out_h = 0;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 tbl_size = 0;
  struct H264DecContainer *dec_cont = (struct H264DecContainer *)dec_inst;
  seqParamSet_t *sps = storage->active_sps;
  u32 pixel_width = (sps->bit_depth_luma == 8 &&
                     sps->bit_depth_chroma == 8) ? 8 : 10;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  if (!dec_cont->tiled_stride_enable) {
    pic_size_in_mbs = storage->active_sps->pic_width_in_mbs * storage->active_sps->pic_height_in_mbs;
    pic_size = NEXT_MULTIPLE(pic_size_in_mbs * 256 * pixel_width / 8, ref_buffer_align)
               + (storage->active_sps->mono_chrome ? 0 :
                  NEXT_MULTIPLE(pic_size_in_mbs * 128 * pixel_width / 8, ref_buffer_align));

    /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
    dmv_mem_size = NEXT_MULTIPLE(pic_size_in_mbs * (dec_cont->high10p_mode ? 80 : 64), ref_buffer_align);
  } else {
    pic_size_in_mbs = storage->active_sps->pic_width_in_mbs * storage->active_sps->pic_height_in_mbs;
    out_w = NEXT_MULTIPLE(4 * storage->active_sps->pic_width_in_mbs * 16 *
                          pixel_width / 8, ALIGN(dec_cont->align));
    out_h = storage->active_sps->pic_height_in_mbs * 4;
    pic_size = NEXT_MULTIPLE(out_w * out_h, ref_buffer_align)
               + (storage->active_sps->mono_chrome ? 0 :
                  NEXT_MULTIPLE(out_w * out_h / 2, ref_buffer_align));
    dmv_mem_size = NEXT_MULTIPLE(pic_size_in_mbs * (dec_cont->high10p_mode ? 80 : 64), ref_buffer_align);
  }

  H264GetRefFrmSize(storage, &rfc_luma_size, &rfc_chroma_size);
  tbl_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align)
             + NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  ref_buff_size = pic_size  + dmv_mem_size
                  + NEXT_MULTIPLE(32, ref_buffer_align)
                  + tbl_size;
  u32 min_buffer_num, max_dpb_size, no_reorder, tot_buffers;
  u32 ext_buffer_size;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;

  if(storage->no_reordering ||
      storage->active_sps->pic_order_cnt_type == 2 ||
      (storage->active_sps->vui_parameters_present_flag &&
       storage->active_sps->vui_parameters->bitstream_restriction_flag &&
       !storage->active_sps->vui_parameters->num_reorder_frames))
    no_reorder = HANTRO_TRUE;
  else
    no_reorder = HANTRO_FALSE;

  if(storage->view == 0)
    max_dpb_size = storage->active_sps->max_dpb_size;
  else {
    /* stereo view dpb size at least equal to base view size (to make sure
     * that base view pictures get output in correct display order) */
    max_dpb_size = MAX(storage->active_sps->max_dpb_size, storage->active_view_sps[0]->max_dpb_size);
  }
  /* restrict max dpb size of mvc (stereo high) streams, make sure that
   * base address 15 is available/restricted for inter view reference use */
  if(storage->mvc_stream)
    max_dpb_size = MIN(max_dpb_size, 8);

  if(no_reorder)
    tot_buffers = MAX(storage->active_sps->num_ref_frames,1) + 1;
  else
    tot_buffers = max_dpb_size + 1;

  /* TODO(min): add extra 10 buffers for top simulation guard buffers.
     To be commented after verification */
#if defined(ASIC_TRACE_SUPPORT) && defined(SUPPORT_MULTI_CORE)
    tot_buffers += 10;
#endif
  if (dec_cont->n_cores == 1) {
    /* single core configuration */
    if (storage->use_smoothing)
      tot_buffers += no_reorder ? 1 : tot_buffers;
  } else {
    /* multi core configuration */

    if (storage->use_smoothing && !no_reorder) {
      /* at least double buffers for smooth output */
      if (tot_buffers > dec_cont->n_cores) {
        tot_buffers *= 2;
      } else {
        tot_buffers += dec_cont->n_cores;
      }
    } else {
      /* one extra buffer for each core */
      /* do not allocate twice for multiview */
      if(!storage->view)
        tot_buffers += dec_cont->n_cores;
    }
  }
  if(tot_buffers > MAX_FRAME_BUFFER_NUMBER)
    tot_buffers = MAX_FRAME_BUFFER_NUMBER;

  min_buffer_num = tot_buffers;
  ext_buffer_size =  ref_buff_size;

  if (dec_cont->pp_enabled) {
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, storage->active_sps->mono_chrome);
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = ext_buffer_size;
}

void h264SetMVCExternalBufferInfo(H264DecInst dec_inst, storage_t *storage) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *)dec_inst;
  u32 pic_size_in_mbs, pic_size, out_w1, out_h1, out_w2, out_h2, tbl_size;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 min_buffer_num;
  u32 ext_buffer_size, ref_buff_size;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  seqParamSet_t *sps = storage->active_sps;
  u32 pixel_width = (sps->bit_depth_luma == 8 &&
                     sps->bit_depth_chroma == 8) ? 8 : 10;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  if( dec_cont->tiled_mode_support && dec_cont->allow_dpb_field_ordering) {
    dec_cont->tiled_stride_enable = 1;
  } else {
    dec_cont->tiled_stride_enable = 0;
  }
  if(storage->sps[1] != 0)
    pic_size_in_mbs = MAX(storage->sps[0]->pic_width_in_mbs * storage->sps[0]->pic_height_in_mbs,
                          storage->sps[1]->pic_width_in_mbs * storage->sps[1]->pic_height_in_mbs);
  else
    pic_size_in_mbs = storage->sps[0]->pic_width_in_mbs * storage->sps[0]->pic_height_in_mbs;
  if (!dec_cont->tiled_stride_enable) {
    pic_size = NEXT_MULTIPLE(pic_size_in_mbs * 256 * pixel_width / 8, ref_buffer_align)
               + (storage->sps[0]->mono_chrome ? 0 :
                  NEXT_MULTIPLE(pic_size_in_mbs * 128 * pixel_width / 8, ref_buffer_align));
  } else {
    if(storage->sps[1] != 0) {
      out_w1 = NEXT_MULTIPLE(4 * storage->sps[0]->pic_width_in_mbs * 16 *
                             pixel_width / 8, ALIGN(dec_cont->align));
      out_h1 = storage->sps[0]->pic_height_in_mbs * 4;
      out_w2 = NEXT_MULTIPLE(4 * storage->sps[1]->pic_width_in_mbs * 16 *
                             pixel_width / 8, ALIGN(dec_cont->align));
      out_h2 = storage->sps[1]->pic_height_in_mbs * 4;
      pic_size = NEXT_MULTIPLE(MAX(out_w1 * out_h1, out_w2 * out_h2), ref_buffer_align)
                 + (storage->sps[0]->mono_chrome ? 0 :
                    NEXT_MULTIPLE(MAX(out_w1 * out_h1, out_w2 * out_h2) / 2, ref_buffer_align));
    } else {
      out_w1 = NEXT_MULTIPLE(4 * storage->sps[0]->pic_width_in_mbs * 16, ALIGN(dec_cont->align));
      out_h1 = storage->sps[0]->pic_height_in_mbs * 4;
      pic_size = NEXT_MULTIPLE(out_w1 * out_h1, ref_buffer_align)
                 + (storage->sps[0]->mono_chrome ? 0 :
                    NEXT_MULTIPLE(out_w1 * out_h1 / 2, ref_buffer_align));
    }
  }

  H264GetRefFrmSize(storage, &rfc_luma_size, &rfc_chroma_size);
  tbl_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align)
             + NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  u32 dmv_mem_size = NEXT_MULTIPLE(pic_size_in_mbs * (dec_cont->high10p_mode ? 80 : 64), ref_buffer_align);
  ref_buff_size = pic_size  + dmv_mem_size
                  + NEXT_MULTIPLE(32, ref_buffer_align) + tbl_size;

  min_buffer_num = 0;
  u32 j = 0;
  for(u32 i = 0; i < 2; i ++) {
    u32 max_dpb_size, no_reorder, tot_buffers;
    if(storage->no_reordering ||
        storage->sps[j]->pic_order_cnt_type == 2 ||
        (storage->sps[j]->vui_parameters_present_flag &&
         storage->sps[j]->vui_parameters->bitstream_restriction_flag &&
         !storage->sps[j]->vui_parameters->num_reorder_frames))
      no_reorder = HANTRO_TRUE;
    else
      no_reorder = HANTRO_FALSE;

    max_dpb_size = storage->sps[j]->max_dpb_size;

    /* restrict max dpb size of mvc (stereo high) streams, make sure that
    * base address 15 is available/restricted for inter view reference use */
    max_dpb_size = MIN(max_dpb_size, 8);

    if(no_reorder)
      tot_buffers = MAX(storage->sps[j]->num_ref_frames, 1) + 1;
    else
      tot_buffers = max_dpb_size + 1;

    /* TODO(min): add extra 10 buffers for top simulation guard buffers.
       To be commented after verification */
#if defined(ASIC_TRACE_SUPPORT) && defined(SUPPORT_MULTI_CORE)
      tot_buffers += 10;
#endif

    min_buffer_num += tot_buffers;
    if(storage->sps[1] != 0)
      j ++;
  }

  ext_buffer_size =  ref_buff_size;

  if (dec_cont->pp_enabled) {
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, storage->active_sps->mono_chrome);
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = ext_buffer_size;
  if(dec_cont->buf_num > MAX_FRAME_BUFFER_NUMBER)
    dec_cont->buf_num = MAX_FRAME_BUFFER_NUMBER;
}


enum DecRet H264DecGetBufferInfo(H264DecInst dec_inst, H264DecBufferInfo *mem_info) {
  struct H264DecContainer  * dec_cont = (struct H264DecContainer *)dec_inst;

  struct DWLLinearMem empty;
  (void)DWLmemset(&empty, 0, sizeof(struct DWLLinearMem));
  struct DWLLinearMem *buffer = NULL;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->storage.release_buffer) {
    /* Release old buffers from input queue. */
    //buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->storage.release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (DEC_MEMFAIL);
      }
      dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;
      dec_cont->storage.ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      if (!dec_cont->pp_enabled)
        return (DEC_OK);
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return DEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return DEC_OK;
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

  return DEC_WAITING_FOR_BUFFER;
}

enum DecRet H264DecAddBuffer(H264DecInst dec_inst, struct DWLLinearMem *info) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *)dec_inst;
  enum DecRet dec_ret = DEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  dec_cont->n_ext_buf_size = dec_cont->storage.ext_buffer_size = info->size;
  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->storage.ext_buffer_added = 1;
  if (dec_cont->ext_buffer_num < dec_cont->buf_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if(!dec_cont->b_mvc) {
    u32 i = dec_cont->buffer_index[0];
    u32 id;
    dpbStorage_t *dpb = dec_cont->storage.dpbs[0];
    if (dec_cont->pp_enabled == 0) {
      if(i < dpb->tot_buffers) {
        dpb->pic_buffers[i] = *info;
        if(i < dpb->dpb_size + 1) {
          id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
          if(id == FB_NOT_VALID_ID) {
            return MEMORY_ALLOCATION_ERROR;
          }

          dpb->buffer[i].data = dpb->pic_buffers + i;
          dpb->buffer[i].mem_idx = id;
          dpb->buffer[i].num_err_mbs = -1;
          dpb->pic_buff_id[i] = id;
        } else {
          id = AllocateIdFree(dpb->fb_list, dpb->pic_buffers + i);
          if(id == FB_NOT_VALID_ID) {
            return MEMORY_ALLOCATION_ERROR;
          }

          dpb->pic_buff_id[i] = id;
        }

        void *base =
          (char *)(dpb->pic_buffers[i].virtual_address) + dpb->dir_mv_offset;
        (void)DWLmemset(base, 0, info->size - dpb->dir_mv_offset);

        dec_cont->buffer_index[0]++;
        if(dec_cont->buffer_index[0] < dpb->tot_buffers)
          dec_ret = DEC_WAITING_FOR_BUFFER;
      } else {
        /* Adding extra buffers. */
        if(dec_cont->buffer_index[0] >= MAX_FRAME_BUFFER_NUMBER) {
          /* Too much buffers added. */
          dec_cont->ext_buffer_num--;
          return DEC_EXT_BUFFER_REJECTED;
        }

        dpb->pic_buffers[i] = *info;
        dpb[1].pic_buffers[i] = *info;
        /* Need the allocate a USED id to be added as free buffer in SetFreePicBuffer. */
        id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
        if(id == FB_NOT_VALID_ID) {
          return MEMORY_ALLOCATION_ERROR;
        }
        dpb->pic_buff_id[i] = id;
        dpb[1].pic_buff_id[i] = id;

        void *base =
          (char *)(dpb->pic_buffers[i].virtual_address) + dpb->dir_mv_offset;
        (void)DWLmemset(base, 0, info->size - dpb->dir_mv_offset);

        dec_cont->buffer_index[0]++;
        dpb->tot_buffers++;
        dpb[1].tot_buffers++;

        SetFreePicBuffer(dpb->fb_list, id);
      }
    } else {
      /* Add down scale buffer. */
      InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
    }
    dec_cont->storage.ext_buffer_added = 1;
  } else {
    u32 * idx = dec_cont->buffer_index;
    if (dec_cont->pp_enabled == 0) {
      if(idx[0] < dec_cont->storage.dpbs[0]->tot_buffers || idx[1] < dec_cont->storage.dpbs[1]->tot_buffers) {
        for(u32 i = 0; i < 2; i ++) {
          u32 id;
          dpbStorage_t *dpb = dec_cont->storage.dpbs[i];
          if(idx[i] < dpb->tot_buffers) {
            dpb->pic_buffers[idx[i]] = *info;
            if(idx[i] < dpb->dpb_size + 1) {
              id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + idx[i]);
              if(id == FB_NOT_VALID_ID) {
                return MEMORY_ALLOCATION_ERROR;
              }

              dpb->buffer[idx[i]].data = dpb->pic_buffers + idx[i];
              dpb->buffer[idx[i]].mem_idx = id;
              dpb->pic_buff_id[idx[i]] = id;
            } else {
              id = AllocateIdFree(dpb->fb_list, dpb->pic_buffers + idx[i]);
              if(id == FB_NOT_VALID_ID) {
                return MEMORY_ALLOCATION_ERROR;
              }

              dpb->pic_buff_id[idx[i]] = id;
            }

            void *base =
              (char *)(dpb->pic_buffers[idx[i]].virtual_address) + dpb->dir_mv_offset;
            (void)DWLmemset(base, 0, info->size - dpb->dir_mv_offset);

            dec_cont->buffer_index[i]++;
            if(dec_cont->buffer_index[i] < dpb->tot_buffers)
              dec_ret = DEC_WAITING_FOR_BUFFER;
            break;
          }
        }
      } else {
        /* Adding extra buffers. */
        if((idx[0] + idx[1]) >= MAX_FRAME_BUFFER_NUMBER) {
          /* Too much buffers added. */
          dec_cont->ext_buffer_num--;
          return DEC_EXT_BUFFER_REJECTED;
        }
        u32 i = idx[0] < idx[1] ? 0 : 1;
        dpbStorage_t *dpb = dec_cont->storage.dpbs[i];
        dpb->pic_buffers[idx[i]] = *info;
        /* Need the allocate a USED id to be added as free buffer in SetFreePicBuffer. */
        u32 id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + idx[i]);
        if(id == FB_NOT_VALID_ID) {
          return MEMORY_ALLOCATION_ERROR;
        }
        dpb->pic_buff_id[idx[i]] = id;

        void *base =
          (char *)(dpb->pic_buffers[idx[i]].virtual_address) + dpb->dir_mv_offset;
        (void)DWLmemset(base, 0, info->size - dpb->dir_mv_offset);

        dec_cont->buffer_index[i]++;
        dpb->tot_buffers++;

        SetFreePicBuffer(dpb->fb_list, id);
      }
    }else {
      /* Add down scale buffer. */
      InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);

      dec_cont->storage.ext_buffer_added = 1;
    }
  }

  return dec_ret;
}


void h264EnterAbortState(struct H264DecContainer *dec_cont) {
  SetAbortStatusInList(&dec_cont->fb_list);
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
  dec_cont->abort = 1;
}

void h264ExistAbortState(struct H264DecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
  dec_cont->abort = 0;
}

void h264StateReset(struct H264DecContainer *dec_cont) {
  dpbStorage_t *dpb = dec_cont->storage.dpbs[0];

  /* Clear parameters in dpb */
  h264EmptyDpb(dpb);
  if (dec_cont->storage.mvc_stream) {
    dpb = dec_cont->storage.dpbs[1];
    h264EmptyDpb(dpb);
  }

  /* Clear parameters in storage */
  h264bsdClearStorage(&dec_cont->storage);

  /* Clear parameters in decContainer */
  dec_cont->dec_stat = DEC_INITIALIZED;
  dec_cont->start_code_detected = 0;
  dec_cont->pic_number = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->rlc_mode = 0;
  dec_cont->try_vlc = 0;
  dec_cont->mode_change = 0;
#endif
  dec_cont->reallocate = 0;
  dec_cont->gaps_checked_for_this = 0;
  dec_cont->packet_decoded = 0;
  dec_cont->keep_hw_reserved = 0;
  dec_cont->force_nal_mode = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->buffer_index[0] = 0;
  dec_cont->buffer_index[1] = 0;
  dec_cont->ext_buffer_num = 0;
#endif

#ifdef SKIP_OPENB_FRAME
  dec_cont->entry_is_IDR = 0;
  dec_cont->entry_POC = 0;
  dec_cont->first_entry_point = 0;
  dec_cont->skip_b = 0;
#endif

  dec_cont->alloc_buffer = 0;
  dec_cont->pre_alloc_buffer[0] =
    dec_cont->pre_alloc_buffer[1] = 0;
  dec_cont->no_decoding_buffer = 0;
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

enum DecRet H264DecAbort(H264DecInst dec_inst) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecAbort# ERROR: dec_inst is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecSetMvc# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting and rs/ds buffer waiting */
  h264EnterAbortState(dec_cont);
  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet H264DecAbortAfter(H264DecInst dec_inst) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 i;
  i32 core_id[MAX_ASIC_CORES];

  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecAbortAfter# ERROR: dec_inst is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecAbortAfter# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  if(dec_cont->asic_running && !dec_cont->b_mc) {
    /* stop HW */
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->h264_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->h264_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    DecrementDPBRefCount(dec_cont->storage.dpb);
    dec_cont->asic_running = 0;

  }

  /* In multi-Core senario, waithwready is executed through listener thread,
          here to check whether HW is finished */
  if(dec_cont->b_mc) {
    for(i = 0; i < dec_cont->n_cores_available; i++) {
      DWLReserveHw(dec_cont->dwl, &core_id[i], DWL_CLIENT_TYPE_H264_DEC);
    }
    /* All HW Core finished */
    for(i = 0; i < dec_cont->n_cores_available; i++) {
      DWLReleaseHw(dec_cont->dwl, core_id[i]);
    }
  }


  /* Clear internal parameters */
  h264StateReset(dec_cont);
  h264ExistAbortState(dec_cont);

#ifdef USE_OMXIL_BUFFER
  pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
  for (i = 0; i < MAX_FRAME_BUFFER_NUMBER; i++) {
    dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
  }
  pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
#endif

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet H264DecSetNoReorder(H264DecInst dec_inst, u32 no_output_reordering) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  if(dec_inst == NULL) {
    DEC_API_TRC("H264DecSetNoReorder# ERROR: dec_inst is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("H264DecSetNoReorder# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  dec_cont->storage.no_reordering = no_output_reordering;
  if(dec_cont->storage.dpb != NULL)
    dec_cont->storage.dpb->no_reordering = no_output_reordering;

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet H264DecSetInfo(H264DecInst dec_inst,
                          struct H264DecConfig *dec_cfg) {

  /*@null@ */ struct H264DecContainer *dec_cont = (struct H264DecContainer *)dec_inst;
  storage_t *storage = &dec_cont->storage;
  const seqParamSet_t *p_sps = dec_cont->storage.active_sps;
  u32 pic_width = h264bsdPicWidth(storage) << 4;
  u32 pic_height = h264bsdPicHeight(storage) << 4;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  u32 i = 0;
  
  struct DecHwFeatures hw_feature;
  seqParamSet_t *sps = storage->active_sps;
  u32 pixel_width = (sps->bit_depth_luma == 8 &&
                     sps->bit_depth_chroma == 8) ? 8 : 10;

  DEC_API_TRC("H264DecSetInfo#\n");

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* TODO: Add valication check here. */

  if (!hw_feature.low_latency_mode_support &&
     (dec_cfg->decoder_mode == DEC_LOW_LATENCY ||
      dec_cfg->decoder_mode == DEC_LOW_LATENCY_RTL)) {
    /* low lantency mode is NOT supported */
    return (DEC_PARAM_ERROR);
  }

  PpUnitSetIntConfig(dec_cont->ppu_cfg, dec_cfg->ppu_config, pixel_width,
                     p_sps->frame_mbs_only_flag,
                     storage->active_sps->mono_chrome);

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height,
                        !p_sps->frame_mbs_only_flag, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  memcpy(dec_cont->delogo_params, dec_cfg->delogo_params, sizeof(dec_cont->delogo_params));
  if (CheckDelogo(dec_cont->delogo_params, sps->bit_depth_luma, sps->bit_depth_chroma))
    return DEC_PARAM_ERROR;
  if (!hw_feature.dec_stride_support) {
    /* For verion earlier than g1v8_2, reset alignment to 16B. */
    dec_cont->align = dec_cont->storage.align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = storage->align = dec_cfg->align;
  }
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  dec_cont->error_conceal = dec_cfg->error_conceal;
  dec_cont->use_ringbuffer = dec_cfg->use_ringbuffer;

  dec_cont->pp_enabled = dec_cont->ppu_cfg[0].enabled |
                         dec_cont->ppu_cfg[1].enabled |
                         dec_cont->ppu_cfg[2].enabled |
                         dec_cont->ppu_cfg[3].enabled |
                         dec_cont->ppu_cfg[4].enabled;

  storage->pp_enabled = dec_cont->pp_enabled;
#if 0
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

  storage->down_scale_x_shift = dec_cont->dscale_shift_x;
  storage->down_scale_y_shift = dec_cont->dscale_shift_y;

  if (hw_feature.pp_version == FIXED_DS_PP) {
    storage->pp_width = pic_width >> storage->down_scale_x_shift;
    storage->pp_height = pic_height >> storage->down_scale_y_shift;
    dec_cont->scaled_width = storage->pp_width;
    dec_cont->scaled_height = storage->pp_height;
    dec_cont->error_conceal = 0;
  } else {
    storage->pp_width = dec_cont->scaled_width;
    storage->pp_height = dec_cont->scaled_height;
  }
#endif


#if 0
  /* Custom DPB modes require tiled support >= 2 */
//  dec_cont->allow_dpb_field_ordering = 0;
//  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;
//  if( dec_cfg->dpb_flags & DEC_DPB_ALLOW_FIELD_ORDERING ) {
//    dec_cont->allow_dpb_field_ordering = hw_cfg.field_dpb_support;
//  }
  storage->no_reordering = dec_cfg->no_output_reordering;

#ifndef _DISABLE_PIC_FREEZE
  dec_cont->storage.intra_freeze = dec_cfg->error_handling == DEC_EC_VIDEO_FREEZE;
  if (dec_cfg->error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->storage.partial_freeze = 1;
  else if (dec_cfg->error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->storage.partial_freeze = 2;
#endif

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;
#endif

#if 0
  /* PP config valication check */
  if (dec_cont->crop_enabled) {
    if ((dec_cont->crop_startx & 0x3) ||
        (dec_cont->crop_starty & 0x3) ||
        (dec_cont->crop_width  < PP_CROP_MIN_WIDTH ||
         dec_cont->crop_height < PP_CROP_MIN_HEIGHT) ||
        (dec_cont->crop_startx + dec_cont->crop_width > pic_width) ||
        (dec_cont->crop_starty + dec_cont->crop_height > pic_height))
    return (DEC_PARAM_ERROR);
  }
  if (dec_cont->scale_enabled) {
    if (dec_cont->crop_width  <= PP_SCALE_IN_MAX_WIDTH &&
        dec_cont->crop_height <= PP_SCALE_IN_MAX_HEIGHT) {
      if ((dec_cont->scaled_width  > MIN(PP_SCALE_IN_MAX_WIDTH,  3 * dec_cont->crop_width)) ||
          (dec_cont->scaled_height > MIN(PP_SCALE_IN_MAX_HEIGHT, 3 * dec_cont->crop_height)) ||
          (dec_cont->scaled_width  & 1) ||
          (dec_cont->scaled_height & 1) ||
          (dec_cont->scaled_width  > dec_cont->crop_width &&
           dec_cont->scaled_height < dec_cont->crop_height) ||
          (dec_cont->scaled_width  < dec_cont->crop_width &&
           dec_cont->scaled_height > dec_cont->crop_height)) {
        return (DEC_PARAM_ERROR);
      }
    } else if (dec_cont->scaled_width  != dec_cont->crop_width ||
               dec_cont->scaled_height != dec_cont->crop_height) {
      return (DEC_PARAM_ERROR);
    }
  }
#endif

  DEC_API_TRC("H264DecSetInfo# OK\n");

  return (DEC_OK);
}

void H264DecUpdateStrmInfoCtrl(H264DecInst dec_inst, u32 last_flag, u32 strm_bus_addr) {
  struct H264DecContainer *dec_cont = (struct H264DecContainer *) dec_inst;
  u32 pack_size = LOW_LATENCY_PACKET_SIZE;
  u32 hw_data_end = strm_bus_addr;
  u32 id = DWLReadAsicID(DWL_CLIENT_TYPE_H264_DEC);
  static u32 len_update = 1;

  if (dec_cont->update_reg_flag) {
    /* wait for hw ready if it's the first time to update length register */
    if (dec_cont->first_update) {
      while (!H264CheckHwStatus(dec_cont))
        sched_yield();
      dec_cont->first_update = 0;
      dec_cont->ll_strm_len = 0;
      len_update = 1;
    }

    while (hw_data_end - dec_cont->ll_strm_bus_address >= pack_size) {
      dec_cont->ll_strm_bus_address += pack_size;
      dec_cont->ll_strm_len += pack_size;
      if ((hw_data_end - dec_cont->ll_strm_bus_address) == 0 && last_flag == 1) {
        /* enable the last packet flag , this means the last packet size is 256bytes */
        SetDecRegister(dec_cont->h264_regs, HWIF_LAST_BUFFER_E, 1);
        if ((id >> 16) == 0x8001)
          DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 3, dec_cont->h264_regs[3]);
        else
          DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 48, dec_cont->h264_regs[48]);
      }
    }
    /* update strm length register */
    if (hw_data_end != dec_cont->ll_strm_bus_address) {
      SetDecRegister(dec_cont->h264_regs, HWIF_STREAM_LEN, dec_cont->ll_strm_len);
      DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 6, dec_cont->h264_regs[6]);
    } else {
      if ((hw_data_end == dec_cont->ll_strm_bus_address) && (last_flag == 1) && len_update) {
        SetDecRegister(dec_cont->h264_regs, HWIF_STREAM_LEN, dec_cont->ll_strm_len);
        DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 6, dec_cont->h264_regs[6]);
        len_update = 0;
      }
    }

    if ((hw_data_end - dec_cont->ll_strm_bus_address) < pack_size &&
      (hw_data_end != dec_cont->ll_strm_bus_address)
       && last_flag == 1) {
      dec_cont->ll_strm_len += (hw_data_end - dec_cont->ll_strm_bus_address);
      dec_cont->ll_strm_bus_address = hw_data_end;
      /* enable the last packet flag */
      SetDecRegister(dec_cont->h264_regs, HWIF_LAST_BUFFER_E, 1);
      if ((id >> 16) == 0x8001)
        DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 3, dec_cont->h264_regs[3]);
      else
        DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 48, dec_cont->h264_regs[48]);
      /* update strm length register */
      SetDecRegister(dec_cont->h264_regs, HWIF_STREAM_LEN, dec_cont->ll_strm_len);
      DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 6, dec_cont->h264_regs[6]);
      len_update = 0;
    }
  }

  if(dec_cont->update_reg_flag) {
    /* check hw status */
    if (H264CheckHwStatus(dec_cont) == 0) {
      dec_cont->tmp_length = dec_cont->ll_strm_len;
      dec_cont->update_reg_flag = 0;
      sem_post(&dec_cont->updated_reg_sem);
    }
  }
}

void H264DecUpdateStrm(struct strmInfo info) {
    stream_info  = info;
}

void h264MCPushOutputAll(struct H264DecContainer *dec_cont) {
  u32 ret;
  H264DecPicture output;
  do {
    memset(&output, 0, sizeof(H264DecPicture));
    ret = H264DecNextPicture_INTERNAL(dec_cont, &output, 0);
  } while( ret == DEC_PIC_RDY );
}

void h264MCWaitOutFifoEmpty(struct H264DecContainer *dec_cont) {
  WaitOutputEmpty(&dec_cont->fb_list);
}

void h264MCWaitPicReadyAll(struct H264DecContainer *dec_cont) {
  WaitListNotInUse(&dec_cont->fb_list);
}

void h264MCSetRefPicStatus(volatile u8 *p_sync_mem, u32 is_field_pic,
                           u32 is_bottom_field) {
  if (is_field_pic == 0) {
    /* frame status */
    DWLmemset((void*)p_sync_mem, 0xFF, 32);
  } else if (is_bottom_field == 0) {
    /* top field status */
    DWLmemset((void*)p_sync_mem, 0xFF, 16);
  } else {
    /* bottom field status */
    p_sync_mem += 16;
    DWLmemset((void*)p_sync_mem, 0xFF, 16);
  }
}

static u32 MCGetRefPicStatus(const u8 *p_sync_mem, u32 is_field_pic,
                             u32 is_bottom_field, u32 high10p_mode) {
  u32 ret;

  /* LE for vc8000d H264 */
  if (high10p_mode)
    ret = ( p_sync_mem[1] << 8) + p_sync_mem[0];
  else {
    if (is_field_pic == 0) {
      /* frame status */
      ret = ( p_sync_mem[0] << 8) + p_sync_mem[1];
    } else if (is_bottom_field == 0) {
      /* top field status */
      ret = ( p_sync_mem[0] << 8) + p_sync_mem[1];
    } else {
      /* bottom field status */
      p_sync_mem += 16;
      ret = ( p_sync_mem[0] << 8) + p_sync_mem[1];
    }
  }
  return ret;
}

static void MCValidateRefPicStatus(const u32 *h264_regs,
                                   H264HwRdyCallbackArg *info) {
  const u8* p_ref_stat;
  const struct DWLLinearMem *p_out;
  const dpbStorage_t *dpb = info->current_dpb;
  u32 status, expected;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  p_out = (struct DWLLinearMem *)GetDataById(dpb->fb_list, info->out_id);

  p_ref_stat = (u8*)p_out->virtual_address + dpb->sync_mc_offset;

  status = MCGetRefPicStatus(p_ref_stat, info->is_field_pic,
      info->is_bottom_field, info->is_high10_mode);

  if (!hw_feature.pic_size_reg_unified) {
    expected = GetDecRegister(h264_regs, HWIF_PIC_MB_HEIGHT_P);
  } else {
    expected = (GetDecRegister(h264_regs, HWIF_PIC_HEIGHT_IN_CBS) + 1) >> 1;
  }

  expected *= 16;

  if(info->is_field_pic)
    expected /= 2;

  if(status < expected) {
    ASSERT(status == expected);
    h264MCSetRefPicStatus((u8*)p_ref_stat,
                          info->is_field_pic, info->is_bottom_field);
  }
}

/* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
void h264MCHwRdyCallback(void *args, i32 core_id) {
  u32 dec_regs[DEC_X170_REGISTERS];

  struct H264DecContainer *dec_cont = (struct H264DecContainer *)args;
  H264HwRdyCallbackArg info;

  const void *dwl;
  const dpbStorage_t *dpb;

  u32 core_status, type;
  u32 i, tmp;
  struct DWLLinearMem *p_out;
  u32 num_concealed_mbs = 0;

  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES || (dec_cont->vcmd_used && core_id < MAX_MC_CB_ENTRIES));

  /* take a copy of the args as after we release the HW they
   * can be overwritten.
   */
  info = dec_cont->hw_rdy_callback_arg[core_id];

  dwl = dec_cont->dwl;
  dpb = info.current_dpb;

  /* read all hw regs */
  for (i = 0; i < DEC_X170_REGISTERS; i++) {
    dec_regs[i] = DWLReadReg(dwl, dec_cont->vcmd_used ? 1 : core_id, i * 4);
  }

#if 0
  printf("Cycles/MB in current frame is %d\n",
      dec_regs[63]/(dec_cont->storage.active_sps->pic_height_in_mbs *
      dec_cont->storage.active_sps->pic_width_in_mbs));
#endif

  /* React to the HW return value */
  core_status = GetDecRegister(dec_regs, HWIF_DEC_IRQ_STAT);

  p_out = (struct DWLLinearMem *)GetDataById(dpb->fb_list,info.out_id);

  /* check if DEC_RDY, all other status are errors */
  if (!(core_status & DEC_8190_IRQ_RDY)) {
#ifdef DEC_PRINT_BAD_IRQ
    fprintf(stderr, "\nCore %d \"bad\" IRQ = 0x%08x\n",
            core_id, core_status);
#endif

    /* reset HW if still enabled */
    if (core_status & DEC_8190_IRQ_BUFFER) {
      /*  reset HW; we don't want an IRQ after reset so disable it */
      DWLDisableHw(dwl, core_id, 0x04,
                   core_status | DEC_IRQ_DISABLE | DEC_ABORT);
    }
    /* reset DMV storage for erroneous pictures */
    {
      u32 dvm_mem_size = dec_cont->storage.pic_size_in_mbs *
                         (dec_cont->high10p_mode? 80 : 64);
      u8 *dvm_base = (u8*)p_out->virtual_address;

      dvm_base += dpb->dir_mv_offset;

      if(info.is_field_pic) {
        dvm_mem_size /= 2;
        if(info.is_bottom_field)
          dvm_base += dvm_mem_size;
      }

      (void) DWLmemset(dvm_base, 0, dvm_mem_size);
    }

    h264MCSetRefPicStatus((u8*)p_out->virtual_address + dpb->sync_mc_offset,
                          info.is_field_pic, info.is_bottom_field);

    if (dec_cont->storage.partial_freeze == 1)
      num_concealed_mbs = GetPartialFreezePos((u8*)p_out->virtual_address,
                                              dec_cont->storage.curr_image[0].width,
                                              dec_cont->storage.curr_image[0].height);
    else
      num_concealed_mbs = dec_cont->storage.pic_size_in_mbs;
#if 0
    /* mark corrupt picture in output queue */
    MarkOutputPicCorrupt(dpb->fb_list, info.out_id,
                         num_concealed_mbs);

    /* ... and in DPB */
    i = dpb->dpb_size + 1;
    while((i--) > 0) {
      dpbPicture_t *dpb_pic = (dpbPicture_t *)dpb->buffer + i;
      if(dpb_pic->data == p_out) {
        dpb_pic->num_err_mbs = num_concealed_mbs;
        break;
      }
    }
#ifdef USE_EC_MC
    dec_cont->storage.num_concealed_mbs = num_concealed_mbs;
#endif
#endif
  } else {
    MCValidateRefPicStatus(dec_regs, &info);
#ifdef USE_EC_MC
    num_concealed_mbs = 0;

    /* if current decoding frame's ref frame has some error,
              this decoding frame will also be treated as the errors from same position */
    for (i = 0; i < dpb->dpb_size; i++) {
      if ( dpb->buffer[dpb->list[i]].num_err_mbs ) {
        num_concealed_mbs = dpb->buffer[dpb->list[i]].num_err_mbs;
        break;
      }
    }
    if ( info.is_idr )
      num_concealed_mbs = 0;
#if 0
    /* mark corrupt picture in output queue */
    MarkOutputPicCorrupt(dpb->fb_list, info.out_id, num_concealed_mbs);

    /* ... and in DPB */
    i = dpb->dpb_size + 1;
    while((i--) > 0) {
      dpbPicture_t *dpb_pic = (dpbPicture_t *)dpb->buffer + i;
      if(dpb_pic->data == p_out) {
        dpb_pic->num_err_mbs = num_concealed_mbs;
        break;
      }
    }
    dec_cont->storage.num_concealed_mbs = num_concealed_mbs;
#endif
#endif
  }

  /* HW cycle */
  u32 cycles = 0;
  u32 mbs = h264bsdPicWidth(&dec_cont->storage) *
             h264bsdPicHeight(&dec_cont->storage);

  if (mbs && dec_cont->high10p_mode)
    cycles = GetDecRegister(dec_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  /* mark corrupt picture in output queue */
  MarkOutputPicInfo(dpb->fb_list, info.out_id, num_concealed_mbs, cycles);

  i = dpb->num_out;
  tmp = dpb->out_index_r;

  while((i--) > 0) {
    if (tmp == dpb->dpb_size + 1)
      tmp = 0;

    dpbOutPicture_t *dpb_pic = (dpbOutPicture_t *)dpb->out_buf + tmp;
    if(dpb_pic->data == p_out) {
      dpb_pic->num_err_mbs = num_concealed_mbs;
      dpb_pic->cycles_per_mb = cycles;
      break;
    }
    tmp++;
  }

  /* ... and in DPB */
  i = dpb->dpb_size + 1;
  while((i--) > 0) {
    dpbPicture_t *dpb_pic = (dpbPicture_t *)dpb->buffer + i;
    if(dpb_pic->data == p_out) {
      dpb_pic->num_err_mbs = num_concealed_mbs;
      dpb_pic->cycles_per_mb = cycles;
      break;
    }
  }
  dec_cont->storage.num_concealed_mbs = num_concealed_mbs;


#if 0
    /* reset HW */
  SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(dec_cont->h264_regs, HWIF_DEC_IRQ, 0);
  SetDecRegister(dec_cont->h264_regs, HWIF_DEC_E, 0);

  DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->h264_regs[1]);
#endif
  /* clear IRQ status reg and release HW core */
  if (dec_cont->vcmd_used) {
    DWLReleaseCmdBuf(dwl, core_id);
    if (dec_cont->b_mc)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)info.core_id, FIFO_EXCEPTION_DISABLE);
  } else {
    DWLReleaseHw(dwl, core_id);
  }

  H264UpdateAfterHwRdy(dec_cont, dec_regs);

  /* release the stream buffer. Callback provided by app */
  if(dec_cont->stream_consumed_callback.fn)
    dec_cont->stream_consumed_callback.fn((u8*)info.stream,
                                          (void*)info.p_user_data);

  if(info.is_field_pic) {
    if(info.is_bottom_field)
      type = FB_HW_OUT_FIELD_BOT;
    else
      type = FB_HW_OUT_FIELD_TOP;
  } else {
    type = FB_HW_OUT_FRAME;
  }

  ClearHWOutput(dpb->fb_list, info.out_id, type, dec_cont->pp_enabled);

  /* decrement buffer usage in our buffer handling */
  DecrementDPBRefCountExt((dpbStorage_t *)dpb, info.ref_id);
}


void h264MCSetHwRdyCallback(struct H264DecContainer *dec_cont) {
  dpbStorage_t *dpb = dec_cont->storage.dpb;
  u32 type, i;

  H264HwRdyCallbackArg *arg = &dec_cont->hw_rdy_callback_arg[dec_cont->core_id];
  i32 core_id = dec_cont->core_id;

  if (dec_cont->vcmd_used) {
    arg = &dec_cont->hw_rdy_callback_arg[dec_cont->cmdbuf_id];
    core_id = dec_cont->cmdbuf_id;
  }

  arg->core_id = (dec_cont->vcmd_used && dec_cont->b_mc) ? dec_cont->mc_buf_id : core_id;
  arg->stream = dec_cont->stream_consumed_callback.p_strm_buff;
  arg->p_user_data = dec_cont->stream_consumed_callback.p_user_data;
  arg->is_field_pic = dpb->current_out->is_field_pic;
  arg->is_bottom_field = dpb->current_out->is_bottom_field;
  arg->out_id = dpb->current_out->mem_idx;
  arg->current_dpb = dpb;
#ifdef USE_EC_MC
  arg->is_idr = IS_IDR_NAL_UNIT(dec_cont->storage.prev_nal_unit);
#endif
  arg->is_high10_mode = dec_cont->high10p_mode;

  for (i = 0; i < dpb->dpb_size; i++) {
    const struct DWLLinearMem *ref;
    ref = (struct DWLLinearMem *)GetDataById(&dec_cont->fb_list, dpb->ref_id[i]);

    //ASSERT(ref->bus_address == (dec_cont->asic_buff->ref_pic_list[i] & (~3)));
    (void)ref;

    arg->ref_id[i] = dpb->ref_id[i];

  }

  DWLSetIRQCallback(dec_cont->dwl, core_id, h264MCHwRdyCallback,
                    dec_cont);

  if(arg->is_field_pic) {
    if(arg->is_bottom_field)
      type = FB_HW_OUT_FIELD_BOT;
    else
      type = FB_HW_OUT_FIELD_TOP;
  } else {
    type = FB_HW_OUT_FRAME;
  }

  MarkHWOutput(&dec_cont->fb_list, dpb->current_out->mem_idx, type);
}
