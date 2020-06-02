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

#include "vc1decapi.h"
#include "version.h"
#include "vc1hwd_container.h"
#include "vc1hwd_decoder.h"
#include "vc1hwd_util.h"
#include "vc1hwd_pp_pipeline.h"
#include "vc1hwd_regdrv.h"
#include "vc1hwd_asic.h"
#include "dwl.h"
#include "deccfg.h"
#include "refbuffer.h"
#include "tiledref.h"
#include "bqueue.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "ppu.h"
#include "sw_util.h"

/*------------------------------------------------------------------------------
    Version Information
------------------------------------------------------------------------------*/

#define VC1DEC_MAJOR_VERSION 1
#define VC1DEC_MINOR_VERSION 2

/*------------------------------------------------------------------------------
    External compiler flags
--------------------------------------------------------------------------------

_VC1DEC_TRACE         Trace VC1 Decoder API function calls.

--------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#ifdef _VC1DEC_TRACE
#define DEC_API_TRC(str)    VC1DecTrace(str)
#else
#define DEC_API_TRC(str)
#endif
#ifndef HANTRO_OK
#define HANTRO_OK 0
#endif

#ifndef HANTRO_NOK
#define HANTRO_NOK 1
#endif

static void WriteBitPlaneCtrl(u32 *bit_plane_ctrl, u8 *mb_flags, u32 num_mbs);
static u32 Vc1CheckFormatSupport(void);
extern u16x AllocateMemories( decContainer_t *dec_cont,
                              swStrmStorage_t *storage,
                              const void *dwl );
static void VC1SetExternalBufferInfo(VC1DecInst dec_inst);

static VC1DecRet VC1DecNextPicture_INTERNAL( VC1DecInst dec_inst,
    VC1DecPicture *picture,
    u32 end_of_stream);

static void VC1EnterAbortState(decContainer_t *dec_cont);
static void VC1ExistAbortState(decContainer_t *dec_cont);
static void VC1EmptyBufferQueue(decContainer_t *dec_cont);
static void VC1CheckBufferRealloc(decContainer_t *dec_cont);
#define DEC_DPB_NOT_INITIALIZED      -1

/*------------------------------------------------------------------------------

    Function name: VC1DecInit

        Functional description:
            Initializes decoder software. Function reserves memory for the
            instance and calls vc1hwdInit to initialize the instance data.

        Inputs:
            dec_inst    Pointer to decoder instance.
            p_meta_data   Pointer to stream metadata container.
            error_handling
                        Flag to determine which error concealment method to use.

        Return values:
            VC1DEC_OK           Initialization is successful.
            VC1DEC_PARAM_ERROR  Error with function parameters.
            VC1DEC_MEMFAIL      Memory allocation failed.
            VC1DEC_INITFAIL     Initialization failed.
            VC1DEC_DWL_ERROR    Error initializing the system interface

------------------------------------------------------------------------------*/
VC1DecRet VC1DecInit( VC1DecInst* dec_inst,
                      const void *dwl,
                      const VC1DecMetaData* p_meta_data,
                      enum DecErrorHandling error_handling,
                      u32 num_frame_buffers,
                      enum DecDpbFlags dpb_flags,
                      u32 use_adaptive_buffers,
                      u32 n_guard_size) {

  u32 i;
  u32 rv;
  decContainer_t *dec_cont;
  u32 reference_frame_format;
  DWLHwConfig config;
  u32 asic_id, hw_build_id;
  struct DecHwFeatures hw_feature;

  DEC_API_TRC("VC1DecInit#");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if ( ((-1)>>1) != (-1) ) {
    DEC_API_TRC("VC1DecInit# ERROR: Right shift is not signed");
    return(VC1DEC_INITFAIL);
  }
  /*lint -restore */

  if (dec_inst == NULL || p_meta_data == NULL) {
    DEC_API_TRC("VC1DecInit# ERROR: NULL argument");
    return(VC1DEC_PARAM_ERROR);
  }

  *dec_inst = NULL; /* return NULL instance for any error */

  /* check that VC-1 decoding supported in HW */
  if(Vc1CheckFormatSupport()) {
    DEC_API_TRC("VC1DecInit# ERROR: VC-1 not supported in HW\n");
    return VC1DEC_FORMAT_NOT_SUPPORTED;
  }

  /* create decoder container */
  dec_cont = (decContainer_t *)DWLmalloc(sizeof(decContainer_t));
  if (dec_cont == NULL) {
    DEC_API_TRC("VC1DecInit# ERROR: Memory allocation failed");
    return(VC1DEC_MEMFAIL);
  }

  (void) DWLmemset(dec_cont, 0, sizeof(decContainer_t));

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->dwl = dwl;

  /* init stream storage structure */
  (void)DWLmemset(&dec_cont->storage, 0, sizeof(swStrmStorage_t));
  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */
  if (dec_cont->tiled_mode_support) {
    dec_cont->storage.min_coded_width = VC1_MIN_WIDTH_EN_DTRC;
    dec_cont->storage.min_coded_height = VC1_MIN_HEIGHT_EN_DTRC;
  }
  else {
    dec_cont->storage.min_coded_width = VC1_MIN_WIDTH;
    dec_cont->storage.min_coded_height = VC1_MIN_HEIGHT;
  }

  /* Initialize decoder */
  rv = vc1hwdInit(dwl, &dec_cont->storage, p_meta_data,
                  num_frame_buffers);
  if ( rv != VC1HWD_OK) {
    DEC_API_TRC("VC1DecInit# ERROR: Invalid initialization metadata");
    DWLfree(dec_cont);
    return (VC1DEC_PARAM_ERROR);
  }

  dec_cont->dec_stat  = VC1DEC_INITIALIZED;
  dec_cont->pic_number = 0;
  dec_cont->asic_running = 0;

  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_VC1_DEC);
  if ((asic_id >> 16) == 0x8170U)
    error_handling = 0;

  dec_cont->vc1_regs[0] = asic_id;
  for (i = 1; i < TOTAL_X170_REGISTERS; i++)
    dec_cont->vc1_regs[i] = 0;

  SetCommonConfigRegs(dec_cont->vc1_regs);

  SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_MODE, DEC_X170_MODE_VC1);

  /* Set prediction filter taps */
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_0, (u32)-4);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_1, 53);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_2, 18);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_3, (u32)-3);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_0, (u32)-1);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_1,  9);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_2,  9);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_3, (u32)-1);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_0, (u32)-3);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_1, 18);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_2, 53);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_3, (u32)-4);

  ASSERT(dec_cont->dwl != NULL);

  (void)DWLmemset(&config, 0, sizeof(DWLHwConfig));

  DWLReadAsicConfig(&config, DWL_CLIENT_TYPE_VC1_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VC1_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
    DEC_API_TRC("VC1DecInit# ERROR: HW not support 64bit address!\n");
    return (VC1DEC_PARAM_ERROR);
  }

  if (hw_feature.ref_frame_tiled_only)
    dpb_flags = DEC_REF_FRM_TILED_DEFAULT | DEC_DPB_ALLOW_FIELD_ORDERING;

  dec_cont->ref_buf_support = hw_feature.ref_buf_support;
  reference_frame_format = dpb_flags & DEC_REF_FRM_FMT_MASK;
  if(reference_frame_format == DEC_REF_FRM_TILED_DEFAULT) {
    /* Assert support in HW before enabling.. */
    if(!hw_feature.tiled_mode_support) {
      return VC1DEC_FORMAT_NOT_SUPPORTED;
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
    return (VC1DEC_PARAM_ERROR);
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
    return (VC1DEC_MEMFAIL);
  }
  dec_cont->storage.release_buffer = 0;
  dec_cont->storage.dec_cont = dec_cont;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->allow_dpb_field_ordering = 0;
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;
  if( dpb_flags & DEC_DPB_ALLOW_FIELD_ORDERING )
    dec_cont->allow_dpb_field_ordering = hw_feature.field_dpb_support;

  if( dec_cont->ref_buf_support ) {
    RefbuInit( &dec_cont->ref_buffer_ctrl, DEC_X170_MODE_VC1,
               dec_cont->storage.pic_width_in_mbs,
               dec_cont->storage.pic_height_in_mbs,
               dec_cont->ref_buf_support );
  }

  if (dec_cont->tiled_mode_support && dec_cont->allow_dpb_field_ordering) {
    dec_cont->tiled_stride_enable = 1;
  } else {
    dec_cont->tiled_stride_enable = 0;
  }

  dec_cont->storage.intra_freeze = error_handling == DEC_EC_VIDEO_FREEZE;
  if (error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->storage.partial_freeze = 1;
  else if (error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->storage.partial_freeze = 2;

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK)
    return VC1DEC_MEMFAIL;

  dec_cont->use_adaptive_buffers = use_adaptive_buffers;
  dec_cont->n_guard_size = n_guard_size;

  DEC_API_TRC("VC1DecInit# OK");

  *dec_inst = (VC1DecInst)dec_cont;

  return(VC1DEC_OK);
}

/*------------------------------------------------------------------------------

    Function name: VC1DecDecode

    Functional description:
        Decodes VC-1 stream.

    Inputs:
        dec_inst Reference to decoder instance.
        input  Decoder input structure.

    Output:
        output Decoder output structure.

    Return values:
        VC1DEC_PIC_DECODED        Picture is decoded.
        VC1DEC_STRM_PROCESSED     Stream handled, no picture ready for display.
        VC1DEC_PARAM_ERROR        Error with function parameters.
        VC1DEC_NOT_INITIALIZED    Attempt to decode with uninitalized decoder.
        VC1DEC_MEMFAIL            Memory allocation failed.
        VC1DEC_HW_BUS_ERROR       A bus error
        VC1DEC_HW_TIMEOUT         Timeout occurred while waiting for HW
        VC1DEC_SYSTEM_ERROR       Wait for hardware failed
        VC1DEC_HW_RESERVED        HW reservation attempt failed
        VC1DEC_HDRS_RDY           Stream headeres decoded.
        VC1DEC_STRM_ERROR         Stream decoding failed.
        VC1DEC_RESOLUTION_CHANGED Picture dimensions has been changed.

------------------------------------------------------------------------------*/
VC1DecRet VC1DecDecode( VC1DecInst dec_inst,
                        const VC1DecInput* input,
                        VC1DecOutput* output ) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  picture_t *p_pic;
  strmData_t stream_data;
  u32 asic_status;
  u32 first_frame;
  VC1DecRet return_value = VC1DEC_PIC_DECODED;
  u32 dec_result = 0;
  u16x tmp_ret_val;
#ifdef NDEBUG
  UNUSED(tmp_ret_val);
#endif
  u32 work_index;
  u32 ff_index;
  u32 error_concealment = HANTRO_FALSE;
  u16x is_bpic = HANTRO_FALSE;
  u32 is_intra;

  DEC_API_TRC("VC1DecDecode#");

  /* Check that function input parameters are valid */
  if (input == NULL || dec_inst == NULL) {
    DEC_API_TRC("VC1DecDecode# ERROR: NULL argument");
    return(VC1DEC_PARAM_ERROR);
  }

  if ((X170_CHECK_VIRTUAL_ADDRESS( input->stream ) ||
       X170_CHECK_BUS_ADDRESS( input->stream_bus_address ) ||
       (input->stream_size == 0) ||
       input->stream_size > dec_cont->max_strm_len)) {
    DEC_API_TRC("VC1DecDecode# ERROR: Invalid input parameters");
    return(VC1DEC_PARAM_ERROR);
  }

  /* Check if decoder is in an incorrect mode */
  if (dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecDecode# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }
  if(dec_cont->abort) {
    return VC1DEC_ABORTED;
  }

  /* init strmData_t structure */
  stream_data.p_strm_buff_start = (u8*)input->stream;
  stream_data.strm_curr_pos = stream_data.p_strm_buff_start;
  stream_data.bit_pos_in_word = 0;
  stream_data.strm_buff_size = input->stream_size;
  stream_data.strm_buff_read_bits = 0;
  stream_data.strm_exhausted = HANTRO_FALSE;

  if (dec_cont->storage.profile == VC1_ADVANCED)
    stream_data.remove_emul_prev_bytes = 1;
  else
    stream_data.remove_emul_prev_bytes = 0;

  first_frame = (dec_cont->storage.first_frame) ? 1 : 0;

  if((dec_cont->dec_stat == VC1DEC_INITIALIZED) &&
      (dec_cont->storage.profile != VC1_ADVANCED)) {
    u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
    VC1DecRet rv;
    if(tmp != HANTRO_OK) {
      rv = VC1DEC_MEMFAIL;
    } else {
      dec_cont->dec_stat = VC1DEC_STREAMDECODING;
      VC1SetExternalBufferInfo(dec_cont);
      dec_cont->buffer_index = 0;
      rv = VC1DEC_WAITING_FOR_BUFFER;
    }

    /* update stream position */
    output->p_stream_curr_pos = stream_data.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    output->data_left = stream_data.strm_buff_size -
                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    if(dec_cont->abort)
      return(VC1DEC_ABORTED);
    else
      return (rv);
  }

  if (dec_cont->dec_stat == VC1DEC_HEADERSDECODED &&
      (dec_cont->storage.profile == VC1_ADVANCED)) {
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    VC1CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->storage.bq);
#endif
        if (dec_cont->storage.ext_buffer_added) {
          dec_cont->storage.release_buffer = 1;
          return_value = VC1DEC_WAITING_FOR_BUFFER;
        }

        /* Allocate memories */
        (void)vc1hwdRelease(dec_cont->dwl, &dec_cont->storage);
        if(dec_cont->bit_plane_ctrl.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->bit_plane_ctrl);
        if(dec_cont->direct_mvs.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->direct_mvs);

        u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
        if (tmp != HANTRO_OK)
          return (VC1DEC_MEMFAIL);
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->storage.ext_buffer_added) {
          dec_cont->storage.release_buffer = 1;
          return_value = VC1DEC_WAITING_FOR_BUFFER;
        }
      }

      if (dec_cont->realloc_int_buf) {
        /* Allocate memories */
        (void)vc1hwdRelease(dec_cont->dwl, &dec_cont->storage);
        if(dec_cont->bit_plane_ctrl.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->bit_plane_ctrl);
        if(dec_cont->direct_mvs.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->direct_mvs);

        u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
        if (tmp != HANTRO_OK)
          return (VC1DEC_MEMFAIL);
      }
    }
  }

  if (dec_cont->dec_stat == VC1DEC_HEADERSDECODED) {
    dec_cont->dec_stat = VC1DEC_STREAMDECODING;
    if (dec_cont->realloc_ext_buf) {
      dec_cont->buffer_index = 0;
      VC1SetExternalBufferInfo(dec_cont);
      return (VC1DEC_WAITING_FOR_BUFFER);
    }
  } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
    return (VC1DEC_WAITING_FOR_BUFFER);
  }
  /* decode SW part (picture layer) */
  if (dec_cont->storage.resolution_changed == HANTRO_FALSE) {
    dec_result = vc1hwdDecode(dec_cont, &dec_cont->storage, &stream_data);
    if (dec_cont->storage.resolution_changed) {
      /* save stream position and dec_result */
      dec_cont->storage.tmp_strm_data = stream_data;
      dec_cont->storage.prev_dec_result = dec_result;
    }
#if 0
    /* Enable PP to handle range map. */
    if (dec_cont->storage.range_map_yflag || dec_cont->storage.range_map_uv_flag)
      dec_cont->pp_enabled = 1;
#endif

    /* if decoder was not released after field decoding finished (PP still running for another frame) and
     * SW either detected missing field or found something else but field header -> release HW and wait PP
     * to finish */
    if (!dec_cont->asic_running &&
        ( (dec_result != VC1HWD_FIELD_HDRS_RDY &&
           dec_result != VC1HWD_USER_DATA_RDY) ||
          dec_cont->storage.missing_field ) ) {
      if (dec_cont->storage.keep_hw_reserved) {
        dec_cont->storage.keep_hw_reserved = 0;
        (void)DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
      }
    }
  } else {
    if( dec_cont->ref_buf_support ) {
      RefbuInit( &dec_cont->ref_buffer_ctrl, DEC_X170_MODE_VC1,
                 dec_cont->storage.pic_width_in_mbs,
                 dec_cont->storage.pic_height_in_mbs,
                 dec_cont->ref_buf_support );
    }

    /* continue and handle errors in pic layer decoding */
    dec_result = dec_cont->storage.prev_dec_result;
    /* restore stream position */
    stream_data = dec_cont->storage.tmp_strm_data;
    dec_cont->storage.resolution_changed = HANTRO_FALSE;
    if (dec_cont->pp_enabled) {
      PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
      (void)CalcPpUnitBufferSize(ppu_cfg, 0);
    }
  }

  output->p_stream_curr_pos = stream_data.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
  output->data_left = stream_data.strm_buff_size -
                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

  /* return if resolution changed so that user can check PP
   * maximum scaling ratio */
  if (dec_cont->storage.resolution_changed) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_RESOLUTION_CHANGED");

    u32 tmpret;
    VC1DecPicture tmp_output;
    do {
      tmpret = VC1DecNextPicture_INTERNAL(dec_cont, &tmp_output, 1);
      if(tmpret == VC1DEC_ABORTED)
        return (VC1DEC_ABORTED);
    } while( tmpret == VC1DEC_PIC_RDY);

    if(dec_cont->abort)
      return(VC1DEC_ABORTED);
    else
      return VC1DEC_RESOLUTION_CHANGED;
  }

  is_intra = HANTRO_FALSE;
  if(dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) {
    if( dec_cont->storage.pic_layer.field_pic_type == FP_I_I &&
        !dec_cont->storage.pic_layer.is_ff ) {
      is_intra = HANTRO_TRUE;
    }
  } else if ( dec_cont->storage.pic_layer.pic_type == PTYPE_I ) {
    is_intra = HANTRO_TRUE;
  }

  if((dec_cont->storage.pic_layer.pic_type == PTYPE_B ||
      dec_cont->storage.pic_layer.pic_type == PTYPE_BI) &&
      (dec_result != VC1HWD_SEQ_HDRS_RDY &&
       dec_result != VC1HWD_ENTRY_POINT_HDRS_RDY))  {
    is_bpic = HANTRO_TRUE;

    /* Skip B frames after corrupted anchor frame */
    if (dec_cont->storage.skip_b ||
        input->skip_non_reference) {
      if( dec_cont->storage.intra_freeze &&
          dec_result == VC1HWD_PIC_HDRS_RDY &&
          !dec_cont->storage.slice) {
        /* skip B frame and seek next frame start */
        (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
        output->p_stream_curr_pos = stream_data.strm_curr_pos;
        output->strm_curr_bus_address = input->stream_bus_address +
                                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
        output->data_left = stream_data.strm_buff_size -
                            (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

        if(dec_cont->storage.profile != VC1_ADVANCED)
          output->data_left = 0;

        tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                           dec_cont->storage.work_out,
                                           1, input->pic_id, 0);
        return VC1DEC_PIC_DECODED;
      } else {
        /* skip B frame and seek next frame start */
        (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
        output->p_stream_curr_pos = stream_data.strm_curr_pos;
        output->strm_curr_bus_address = input->stream_bus_address +
                                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
        output->data_left = stream_data.strm_buff_size -
                            (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

        if(dec_cont->storage.profile != VC1_ADVANCED)
          output->data_left = 0;

        DEC_API_TRC("VC1DecDecode# VC1DEC_NONREF_PIC_SKIPPED (Skip B-frame)");
        return VC1DEC_NONREF_PIC_SKIPPED; /*VC1DEC_STRM_PROCESSED;*/
      }
    }
  }

  /* Initialize DPB mode */
  if( dec_cont->storage.interlace &&
      dec_cont->allow_dpb_field_ordering )
    dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
  else
    dec_cont->dpb_mode = DEC_DPB_FRAME;

  /* Initialize tiled mode */
  if( dec_cont->tiled_mode_support) {
    /* Check mode validity */
    if(DecCheckTiledMode( dec_cont->tiled_mode_support,
                          dec_cont->dpb_mode,
                          dec_cont->storage.interlace ) !=
        HANTRO_OK ) {
      DEC_API_TRC("VC1DecDecode# ERROR: DPB mode does not support tiled "\
                  "reference pictures");
      return VC1DEC_PARAM_ERROR;
    }

    dec_cont->tiled_reference_enable =
      DecSetupTiledReference( dec_cont->vc1_regs,
                              dec_cont->tiled_mode_support,
                              dec_cont->dpb_mode,
                              dec_cont->storage.interlace );
  } else {
    dec_cont->tiled_reference_enable = 0;
  }

  /* Check dec_result */
  if (dec_result == VC1HWD_SEQ_HDRS_RDY) {
    /* Force raster scan output for interlaced sequences */
    /*
    if (dec_cont->storage.interlace)
        SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_OUT_TILED_E, 0);
        */

    DEC_API_TRC("VC1DecDecode# VC1DEC_HDRS_RDY (Sequence layer)");
    dec_cont->dec_stat = VC1DEC_HEADERSDECODED;
    if (dec_cont->pp_enabled) {
      dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
      dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
    }

    /* for a case, it should be go here again due to the first allocation is calulated by max_coded width,
       this is only for error concealment*/
    dec_cont->storage.first_frame = 1;
    //dec_cont->storage.outp_count = 0;
    FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
    if(dec_cont->abort)
      return(VC1DEC_ABORTED);
    else
      return (VC1DEC_HDRS_RDY);
  } else if (dec_result == VC1HWD_ENTRY_POINT_HDRS_RDY) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_HDRS_RDY (Entry-Point layer");
    dec_cont->dec_stat = VC1DEC_HEADERSDECODED;
    if (dec_cont->pp_enabled) {
      dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
      dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
    }

    dec_cont->storage.first_frame = 1;
    //dec_cont->storage.outp_count = 0;
    FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
    if(dec_cont->abort)
      return(VC1DEC_ABORTED);
    else
      return (VC1DEC_HDRS_RDY);
  } else if (dec_result == VC1HWD_USER_DATA_RDY) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_STRM_PROCESSED (User data)");
    return (VC1DEC_STRM_PROCESSED);
  } else if (dec_result == VC1HWD_METADATA_ERROR ) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_STRM_ERROR");
    return (VC1DEC_STRM_ERROR);
  } else if (dec_result == VC1HWD_END_OF_SEQ) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_END_OF_SEQ");
    return (VC1DEC_END_OF_SEQ);
  } else if (dec_result == VC1HWD_MEMORY_FAIL) {
    DEC_API_TRC("VC1DecDecode# VC1DEC_MEMFAIL");
    return (VC1DEC_MEMFAIL);
  } else if (dec_result == VC1HWD_HDRS_ERROR) {
    /* Handle case where sequence or entry-point headers are not
     * decoded before the first frame (all resources not allocated) */
    DEC_API_TRC("VC1DecDecode# VC1DEC_STRM_ERROR (header error)");
    return (VC1DEC_STRM_ERROR);
  } else if (dec_result == VC1HWD_ERROR ||
             (first_frame && dec_result == VC1HWD_NOT_CODED_PIC)) {
    /* Perform error concealment */
    EPRINT(("Picture layer decoding failed!"));
    error_concealment = HANTRO_TRUE;

    /* force output index to zero if it is first frame
     * and picture is skipped */
    if (first_frame) {
      dec_cont->storage.work_out_prev =
        dec_cont->storage.work_out = BqueueNext2(
                                       &dec_cont->storage.bq,
                                       dec_cont->storage.work0,
                                       dec_cont->storage.work1,
                                       BQUEUE_UNUSED,
                                       0);
      if(dec_cont->storage.work_out == (u32)0xFFFFFFFF) {
        if (dec_cont->abort)
          return VC1DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->p_stream_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->same_pic_header = 1;
          return VC1DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].first_show = 1;

      if (dec_cont->pp_enabled) {
        dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data =
            InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
        if (dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data == NULL)
          return VC1DEC_ABORTED;
      }
    }

    vc1hwdErrorConcealment( first_frame, &dec_cont->storage );

    /* skip corrupted B frame or orphan field */
    if (first_frame || (is_bpic && !dec_cont->storage.intra_freeze) || dec_cont->storage.missing_field) {
      (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
      if (dec_cont->pp_enabled && first_frame) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
      }
      return_value = VC1DEC_STRM_PROCESSED;
    } else
      return_value = VC1DEC_PIC_DECODED;

    if(return_value == VC1DEC_PIC_DECODED) {
      /* set non coded picture for stand alone post-processing */
      p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
      p_pic[ dec_cont->storage.work_out ].field[0].dec_pp_stat = STAND_ALONE;
      p_pic[ dec_cont->storage.work_out ].field[1].dec_pp_stat = STAND_ALONE;

      tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                         dec_cont->storage.work_out,
                                         is_bpic, input->pic_id, dec_cont->storage.num_of_mbs );
      ASSERT( tmp_ret_val == HANTRO_OK );
#ifdef _DEC_PP_USAGE
      PrintDecPpUsage(dec_cont,
                      dec_cont->storage.pic_layer.is_ff,
                      dec_cont->storage.work_out,
                      HANTRO_TRUE,
                      input->pic_id);
#endif
    }
  } else if (dec_result == VC1HWD_NOT_CODED_PIC) {
    if( dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE ) {
      u16x work_out_prev_tmp, work_out_tmp;
      work_out_prev_tmp = dec_cont->storage.work_out_prev;
      work_out_tmp = dec_cont->storage.work_out;
      dec_cont->storage.work_out_prev = dec_cont->storage.work_out;
      dec_cont->storage.work0 = dec_cont->storage.work_out;
      dec_cont->storage.work_out = BqueueNext2(
                                     &dec_cont->storage.bq,
                                     dec_cont->storage.work0,
                                     dec_cont->storage.work1,
                                     BQUEUE_UNUSED, 0 );
      if(dec_cont->storage.work_out == (u32)0xFFFFFFFF) {
        if (dec_cont->abort)
          return VC1DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->p_stream_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->storage.work0 = INVALID_ANCHOR_PICTURE;
          dec_cont->storage.work_out_prev = work_out_prev_tmp;
          dec_cont->storage.work_out = work_out_tmp;
          dec_cont->same_pic_header = 1;
          return VC1DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].first_show = 1;
    }
    /* Set picture specific information to picture_t struct */
    vc1hwdSetPictureInfo( dec_cont, input->pic_id );

    /* Clear key-frame flag from output picture, skipped pictures
     * are P pictures and therefore not key-frames... */
    p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
    p_pic[ dec_cont->storage.work_out ].key_frame = HANTRO_FALSE;
    p_pic[ dec_cont->storage.work_out ].fcm =
      dec_cont->storage.pic_layer.fcm;

    /* set non coded picture for stand alone post-processing */
    p_pic[ dec_cont->storage.work_out ].field[0].dec_pp_stat = STAND_ALONE;
    p_pic[ dec_cont->storage.work_out ].field[1].dec_pp_stat = STAND_ALONE;

    return_value = VC1DEC_PIC_DECODED;
    tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                       dec_cont->storage.work_out,
                                       is_bpic, input->pic_id, 0);
    ASSERT( tmp_ret_val == HANTRO_OK );

    /* update stream position */
    output->p_stream_curr_pos = stream_data.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    output->data_left = stream_data.strm_buff_size -
                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

    if(dec_cont->storage.profile != VC1_ADVANCED)
      output->data_left = 0;

#ifdef _DEC_PP_USAGE
    PrintDecPpUsage(dec_cont,
                    dec_cont->storage.pic_layer.is_ff,
                    dec_cont->storage.work_out,
                    HANTRO_TRUE,
                    input->pic_id);
#endif
  }
  /* If we are using intra freeze concealment and picture is broken */
  else if( !is_intra && dec_cont->storage.picture_broken &&
           dec_cont->storage.intra_freeze &&
           !dec_cont->storage.slice ) {
    error_concealment = HANTRO_TRUE;
    if( dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE ) {
      u16x work_out_prev_tmp, work_out_tmp;
      work_out_prev_tmp = dec_cont->storage.work_out_prev;
      work_out_tmp = dec_cont->storage.work_out;
      dec_cont->storage.work_out_prev = dec_cont->storage.work_out;
      dec_cont->storage.work0 = dec_cont->storage.work_out;
      dec_cont->storage.work_out = BqueueNext2(
                                     &dec_cont->storage.bq,
                                     dec_cont->storage.work0,
                                     dec_cont->storage.work1,
                                     BQUEUE_UNUSED, 0 );
      if(dec_cont->storage.work_out == (u32)0xFFFFFFFF) {
        if (dec_cont->abort)
          return VC1DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->p_stream_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->storage.work0 = INVALID_ANCHOR_PICTURE;
          dec_cont->storage.work_out_prev = work_out_prev_tmp;
          dec_cont->storage.work_out = work_out_tmp;
          dec_cont->same_pic_header = 1;
          return VC1DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;

      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].first_show = 1;
    }
    /* Set picture specific information to picture_t struct */
    vc1hwdSetPictureInfo( dec_cont, input->pic_id );

    /* Clear key-frame flag from output picture, skipped pictures
     * are P pictures and therefore not key-frames... */
    p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
    p_pic[ dec_cont->storage.work_out ].key_frame = HANTRO_FALSE;
    p_pic[ dec_cont->storage.work_out ].fcm =
      dec_cont->storage.pic_layer.fcm;

    /* set non coded picture for stand alone post-processing */
    p_pic[ dec_cont->storage.work_out ].field[0].dec_pp_stat = STAND_ALONE;
    p_pic[ dec_cont->storage.work_out ].field[1].dec_pp_stat = STAND_ALONE;

    return_value = VC1DEC_PIC_DECODED;
    tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                       dec_cont->storage.work_out,
                                       is_bpic, input->pic_id, 0);
    ASSERT( tmp_ret_val == HANTRO_OK );

    /* update stream position */
    (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
    output->p_stream_curr_pos = stream_data.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    output->data_left = stream_data.strm_buff_size -
                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

    if(dec_cont->storage.profile != VC1_ADVANCED)
      output->data_left = 0;

  } else { /* dec_result == VC1HWD_OK */
    if(!dec_cont->storage.slice) {
      /* update picture buffer work indexes and
       * check anchor availability */
      u16x work_out_prev_tmp, work_out_tmp;
      work_out_prev_tmp = dec_cont->storage.work_out_prev;
      work_out_tmp = dec_cont->storage.work_out;
      vc1hwdUpdateWorkBufferIndexes( dec_cont, is_bpic );
      if(dec_cont->storage.work_out == (u32)0xFFFFFFFF || (dec_cont->pp_enabled &&
         dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data == NULL)) {
        if (dec_cont->abort)
          return VC1DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->p_stream_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->storage.work_out_prev = work_out_prev_tmp;
          dec_cont->storage.work_out = work_out_tmp;
          dec_cont->same_pic_header = 1;
          return VC1DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;

#ifdef _DEC_PP_USAGE
      PrintDecPpUsage(dec_cont,
                      dec_cont->storage.pic_layer.is_ff,
                      dec_cont->storage.work_out,
                      HANTRO_TRUE,
                      input->pic_id);
#endif
      /* Set picture specific information to picture_t struct */
      vc1hwdSetPictureInfo( dec_cont, input->pic_id );

      /* write bit plane control for HW */
      WriteBitPlaneCtrl(dec_cont->bit_plane_ctrl.virtual_address,
                        dec_cont->storage.p_mb_flags, dec_cont->storage.num_of_mbs);
    }

    if (!dec_cont->asic_running && dec_cont->storage.partial_freeze) {
      PreparePartialFreeze(
        (u8*)dec_cont->storage.p_pic_buf[
          dec_cont->storage.work_out].data.virtual_address,
        dec_cont->storage.pic_width_in_mbs,
        dec_cont->storage.pic_height_in_mbs);
    }

    /* Start HW */
    asic_status = VC1RunAsic(dec_cont, &stream_data,
                             input->stream_bus_address);

    if (asic_status == X170_DEC_TIMEOUT) {
      error_concealment = HANTRO_TRUE;
      if (dec_cont->pp_enabled) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
      }
      vc1hwdErrorConcealment(0, &dec_cont->storage);
      DEC_API_TRC("VC1DecDecode# VC1DEC_HW_TIMEOUT");
      return VC1DEC_HW_TIMEOUT;
    } else if (asic_status == X170_DEC_SYSTEM_ERROR) {
      error_concealment = HANTRO_TRUE;
      if (dec_cont->pp_enabled) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
      }
      vc1hwdErrorConcealment(0, &dec_cont->storage);
      DEC_API_TRC("VC1DecDecode# VC1DEC_SYSTEM_ERROR");
      return VC1DEC_SYSTEM_ERROR;
    } else if (asic_status == X170_DEC_HW_RESERVED) {
      error_concealment = HANTRO_TRUE;
      if (dec_cont->pp_enabled) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
      }
      vc1hwdErrorConcealment(0, &dec_cont->storage);
      DEC_API_TRC("VC1DecDecode# VC1DEC_HW_RESERVED");
      return VC1DEC_HW_RESERVED;
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);

    if (asic_status & DEC_X170_IRQ_BUS_ERROR) {
      error_concealment = HANTRO_TRUE;
      if (dec_cont->pp_enabled) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
      }
      vc1hwdErrorConcealment(0, &dec_cont->storage);
      DEC_API_TRC("VC1DecDecode# VC1DEC_HW_BUS_ERROR");
      return VC1DEC_HW_BUS_ERROR;
    } else if (asic_status & DEC_X170_IRQ_BUFFER_EMPTY) {
      /* update stream position */
      output->p_stream_curr_pos = stream_data.strm_curr_pos;
      output->strm_curr_bus_address = input->stream_bus_address +
                                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
      output->data_left = stream_data.strm_buff_size -
                          (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

      if(dec_cont->storage.profile != VC1_ADVANCED)
        output->data_left = 0;

      DEC_API_TRC("VC1DecDecode# VC1DEC_BUF_EMPTY (Buffer empty)");
      return (VC1DEC_BUF_EMPTY);
    } else if (asic_status &
               (DEC_X170_IRQ_STREAM_ERROR |
                DEC_X170_IRQ_ASO |
                DEC_X170_IRQ_TIMEOUT |
                DEC_8190_IRQ_ABORT) ) {
      if (!dec_cont->storage.partial_freeze ||
          !ProcessPartialFreeze(
            (u8*)dec_cont->storage.p_pic_buf[
              (i32)dec_cont->storage.work_out].data.virtual_address,
            dec_cont->storage.work0 != INVALID_ANCHOR_PICTURE ?
            (u8*)dec_cont->storage.p_pic_buf[
              (i32)dec_cont->storage.work0].data.virtual_address :
            NULL,
            dec_cont->storage.pic_width_in_mbs,
            dec_cont->storage.pic_height_in_mbs,
            dec_cont->storage.partial_freeze == 1)) {
        /* check if really needed for ADV? */
        if (!dec_cont->storage.partial_freeze) {
          error_concealment = HANTRO_TRUE;
          if (!first_frame) {
            if (dec_cont->pp_enabled) {
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
            }
          }
          vc1hwdErrorConcealment( first_frame, &dec_cont->storage );

          if (first_frame || (is_bpic && !dec_cont->storage.intra_freeze)
              || dec_cont->storage.slice) {
            (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
            if (dec_cont->pp_enabled && first_frame) {
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue, dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data->virtual_address);
            }
            return_value = VC1DEC_STRM_PROCESSED;
          } else
            return_value = VC1DEC_PIC_DECODED;
        } else {
          if (!dec_cont->storage.partial_freeze ||
              !ProcessPartialFreeze(
                (u8*)dec_cont->storage.p_pic_buf[
                  (i32)dec_cont->storage.work0].data.virtual_address,
                dec_cont->storage.work1 != INVALID_ANCHOR_PICTURE ?
                (u8*)dec_cont->storage.p_pic_buf[
                  (i32)dec_cont->storage.work1].data.virtual_address :
                NULL,
                dec_cont->storage.pic_width_in_mbs,
                dec_cont->storage.pic_height_in_mbs,
                dec_cont->storage.partial_freeze == 1)) {
          }
        }

      } else {
        asic_status &= ~DEC_X170_IRQ_STREAM_ERROR;
        asic_status &= ~DEC_X170_IRQ_TIMEOUT;
        asic_status |= DEC_X170_IRQ_DEC_RDY;
        error_concealment = HANTRO_FALSE;
      }

      if(return_value == VC1DEC_PIC_DECODED ) {
        /* buffer concealed frame
         * (individual fields are not concealed) */
        tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                           dec_cont->storage.work_out,
                                           is_bpic, input->pic_id, dec_cont->storage.num_of_mbs);
        ASSERT( tmp_ret_val == HANTRO_OK );
      }
    } else { /*(DEC_X170_IRQ_RDY)*/
      tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                         dec_cont->storage.work_out,
                                         is_bpic, input->pic_id, 0 );
      ASSERT( tmp_ret_val == HANTRO_OK );
      return_value = VC1DEC_PIC_DECODED;
      if( is_intra ) {
        dec_cont->storage.picture_broken = HANTRO_FALSE;
      }
    }

    if(error_concealment && dec_cont->storage.pic_layer.pic_type != PTYPE_B ) {
      dec_cont->storage.picture_broken = HANTRO_TRUE;
    }
  }

  /* update stream position */
  output->p_stream_curr_pos = stream_data.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
  output->data_left = stream_data.strm_buff_size -
                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

  if(dec_cont->storage.profile != VC1_ADVANCED)
    output->data_left = 0;

  /* Update picture buffer work indexes */
  if ( (!is_bpic && (dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE)) ||
       (((dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) &&
         (dec_cont->storage.pic_layer.is_ff == HANTRO_FALSE)) && !is_bpic) ) {
    if( dec_cont->storage.max_bframes > 0 ) {
      dec_cont->storage.work1 = dec_cont->storage.work0;
    }
    dec_cont->storage.work0 = dec_cont->storage.work_out;
  }
  dec_cont->pic_number++;

  /* force index to 2 for B frames
   * (prevents overwriting return value of successfully decoded pic) */
  work_index = (is_bpic) ? dec_cont->storage.prev_bidx : dec_cont->storage.work_out;
  p_pic = (picture_t*)dec_cont->storage.p_pic_buf;

  /* anchor picture succesfully decoded */
  if ( !is_bpic && !error_concealment) {
    /* reset B picture skip after succesfully decoded anchor picture */
    if(dec_cont->storage.skip_b)
      dec_cont->storage.skip_b--;
  }

  ff_index = (dec_cont->storage.pic_layer.is_ff) ? 0 : 1;

  /* store return_value to field structure */
  if (return_value == VC1DEC_PIC_DECODED)
    p_pic[ work_index ].field[ff_index].return_value = VC1DEC_PIC_RDY;
  else
    p_pic[ work_index ].field[ff_index].return_value = return_value;

  /* Copy return value etc. to second field structure too... */
  if (dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE)
    p_pic[ work_index ].field[1] = p_pic[ work_index ].field[0];

  u32 tmpret;
  VC1DecPicture tmp_output;
  do {
    tmpret = VC1DecNextPicture_INTERNAL(dec_cont, &tmp_output, 0);
    if(tmpret == VC1DEC_ABORTED)
      return (VC1DEC_ABORTED);
  } while( tmpret == VC1DEC_PIC_RDY);

  DEC_API_TRC("VC1DecDecode# OK");
  if(dec_cont->abort)
    return(VC1DEC_ABORTED);
  else
    return(return_value);

  /*lint --e(550) Symbol 'tmp_ret_val' not accessed */
}

/*------------------------------------------------------------------------------

    Function name: VC1DecGetAPIVersion

        Functional description:
            Returns current API version.

        Inputs:
            none

        Return value:
            VC1DecApiVersion  structure containing API version information.

------------------------------------------------------------------------------*/
VC1DecApiVersion VC1DecGetAPIVersion(void) {
  VC1DecApiVersion ver;

  ver.major = VC1DEC_MAJOR_VERSION;
  ver.minor = VC1DEC_MINOR_VERSION;

  DEC_API_TRC("VC1DecGetAPIVersion# OK");

  return(ver);
}

/*------------------------------------------------------------------------------

    Function name: VC1DecGetBuild

        Functional description:
            Returns current API version.

        Inputs:
            none

        Return value:
            VC1DecApiVersion  structure containing API version information.

------------------------------------------------------------------------------*/
VC1DecBuild VC1DecGetBuild(void) {
  VC1DecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_VC1_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_VC1_DEC);

  DEC_API_TRC("VC1DecGetBuild# OK");

  return build_info;
}

/*------------------------------------------------------------------------------

    Function name: VC1DecRelease

        Functional description:
            releases decoder resources.

        Input:
            dec_inst Reference to decoder instance.

        Return values:
            none

------------------------------------------------------------------------------*/
void VC1DecRelease(VC1DecInst dec_inst) {
  decContainer_t *dec_cont;
  const void *dwl;

  DEC_API_TRC("VC1DecRelease#");

  if (dec_inst == NULL) {
    DEC_API_TRC("VC1DecRelease# ERROR: dec_inst == NULL");
    return;
  }

  dec_cont = (decContainer_t*)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecRelease# ERROR: Decoder not initialized");
    return;
  }

  pthread_mutex_destroy(&dec_cont->protect_mutex);
  dwl = dec_cont->dwl;

  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  if(dec_cont->asic_running) {
    DWLWriteReg(dwl, dec_cont->core_id, 1 * 4, 0); /* stop HW */
    DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  (void)vc1hwdRelease(dwl,
                      &dec_cont->storage);

  if(dec_cont->bit_plane_ctrl.virtual_address)
    DWLFreeLinear(dwl, &dec_cont->bit_plane_ctrl);
  if(dec_cont->direct_mvs.virtual_address)
    DWLFreeLinear(dwl, &dec_cont->direct_mvs);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  if (dec_cont->storage.hrd_rate)
    DWLfree(dec_cont->storage.hrd_rate);
  if (dec_cont->storage.hrd_buffer)
    DWLfree(dec_cont->storage.hrd_buffer);
  if (dec_cont->storage.hrd_fullness)
    DWLfree(dec_cont->storage.hrd_fullness);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  dec_cont->storage.hrd_rate = NULL;
  dec_cont->storage.hrd_buffer = NULL;
  dec_cont->storage.hrd_fullness = NULL;
  DWLfree(dec_cont);

  DEC_API_TRC("VC1DecRelease# OK");

}

/*------------------------------------------------------------------------------

    Function: VC1DecGetInfo()

        Functional description:
            This function provides read access to decoder information.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            VC1DEC_OK             success
            VC1DEC_PARAM_ERROR    invalid parameters

------------------------------------------------------------------------------*/
VC1DecRet VC1DecGetInfo(VC1DecInst dec_inst, VC1DecInfo * dec_info) {
  decContainer_t *dec_cont;

  DEC_API_TRC("VC1DecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    DEC_API_TRC("VC1DecGetInfo# ERROR: dec_inst or dec_info is NULL");
    return (VC1DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t*)dec_inst;

  if(dec_cont->tiled_reference_enable) {
    dec_info->output_format = VC1DEC_TILED_YUV420;
  } else {
    dec_info->output_format = VC1DEC_SEMIPLANAR_YUV420;
  }

  dec_info->max_coded_width  = dec_cont->storage.max_coded_width;
  dec_info->max_coded_height = dec_cont->storage.max_coded_height;
  dec_info->coded_width     = dec_cont->storage.cur_coded_width;
  dec_info->coded_height    = dec_cont->storage.cur_coded_height;
  dec_info->par_width       = dec_cont->storage.aspect_horiz_size;
  dec_info->par_height      = dec_cont->storage.aspect_vert_size;
  dec_info->frame_rate_numerator    = dec_cont->storage.frame_rate_nr;
  dec_info->frame_rate_denominator  = dec_cont->storage.frame_rate_dr;
  dec_info->interlaced_sequence = dec_cont->storage.interlace;
  dec_info->dpb_mode        = dec_cont->dpb_mode;
  dec_info->multi_buff_pp_size = 2; /*dec_cont->storage.interlace ? 1 : 2;*/

  if(dec_cont->tiled_mode_support) {
    if(dec_info->interlaced_sequence &&
        (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
      dec_info->output_format = VC1DEC_SEMIPLANAR_YUV420;
    } else {
      dec_info->output_format = VC1DEC_TILED_YUV420;
    }
  } else {
    dec_info->output_format = VC1DEC_SEMIPLANAR_YUV420;
  }

  DEC_API_TRC("VC1DecGetInfo# OK");

  return (VC1DEC_OK);

}

/*------------------------------------------------------------------------------

    Function: VC1DecUnpackMetadata

        Functional description:
            Unpacks metadata elements for sequence header C from buffer,
            when metadata is packed according to SMPTE VC-1 Standard Annex J.

        Inputs:
            p_buffer     Pointer to buffer containing packed metadata. Buffer
                        must contain at least 4 bytes.
            buffer_size  Buffer size in bytes.
            p_meta_data   Pointer to stream metadata container.

        Return values:
            VC1DEC_OK             Metadata successfully unpacked.
            VC1DEC_PARAM_ERROR    Error with function parameters.
            VC1DEC_METADATA_FAIL  Meta data is in wrong format or indicates
                                  unsupported tools

------------------------------------------------------------------------------*/
VC1DecRet VC1DecUnpackMetaData(const u8* p_buffer, u32 buffer_size,
                               VC1DecMetaData* p_meta_data ) {
  DEC_API_TRC("VC1DecUnpackMetadata#");

  if (buffer_size < 4) {
    DEC_API_TRC("VC1DecUnpackMetadata# ERROR: buffer_size < 4");
    return(VC1DEC_PARAM_ERROR);
  }
  if (p_buffer == NULL) {
    DEC_API_TRC("VC1DecUnpackMetadata# ERROR: p_buffer == NULL");
    return(VC1DEC_PARAM_ERROR);
  }
  if (p_meta_data == NULL) {
    DEC_API_TRC("VC1DecUnpackMetadata# ERROR: p_meta_data == NULL");
    return(VC1DEC_PARAM_ERROR);
  }

  if ( vc1hwdUnpackMetaData( p_buffer, p_meta_data ) != HANTRO_OK ) {
    DEC_API_TRC("VC1DecUnpackMetadata# ERROR: Metadata failure");
    return(VC1DEC_METADATA_FAIL);
  }

  DEC_API_TRC("VC1DecUnpackMetadata# OK");

  return(VC1DEC_OK);
}


/*------------------------------------------------------------------------------

    Function name: WriteBitPlaneCtrl

        Functional description:
            Write bit plane control information to SW-HW shared memory.

        Inputs:
            mb_flags     pointer to data decoded from bitplane
            num_mbs      number of macroblocks in picture

        Outputs:
            bit_plane_ctrl    pointer to SW-HW shared memory where bit plane
                            control data shall be written

        Return values:

------------------------------------------------------------------------------*/
void WriteBitPlaneCtrl(u32 *bit_plane_ctrl, u8 *mb_flags, u32 num_mbs) {

  u32 i, j;
  u32 tmp;
  u8 tmp1;
  u32 *p_tmp = bit_plane_ctrl;

  for (i = (num_mbs+9)/10; i--;) {
    tmp = 0;
    for (j = 0; j < 10; j++) {
      tmp1 = *mb_flags++;
      tmp = (tmp<<3) | (tmp1 & 0x7);
    }
    tmp <<= 2;
    *p_tmp++ = tmp;
  }

}

/*------------------------------------------------------------------------------

    Function name: VC1DecNextPicture

    Functional description:
        Retrieve next decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        VC1DEC_OK               No picture available.
        VC1DEC_PIC_RDY          Picture ready.
        VC1DEC_PARAM_ERROR      Error with function parameters.
        VC1DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
VC1DecRet VC1DecNextPicture( VC1DecInst     dec_inst,
                             VC1DecPicture  *picture,
                             u32            end_of_stream) {

  /* Variables */
  decContainer_t *dec_cont;
  i32 ret;

  /* Code */

  DEC_API_TRC("VC1DecNextPicture#");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    DEC_API_TRC("VC1DecNextPicture# ERROR: picture is NULL");
    return(VC1DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1SwDecNextPicture# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
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
    if (ret == FIFO_EMPTY) return VC1DEC_OK;
#endif

    if ((i32)i == -1) {
      DEC_API_TRC("VC1DecNextPicture# VC1DEC_END_OF_SEQ\n");
      return VC1DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      DEC_API_TRC("VC1DecNextPicture# VC1DEC_FLUSHED\n");
      return VC1DEC_FLUSHED;
    }

    *picture = dec_cont->storage.picture_info[i];
    //for (j = 0; j < 4; j++)
    //dec_cont->storage.picture_info[i].pictures[j].output_picture = NULL;

    DEC_API_TRC("VC1DecNextPicture# VC1DEC_PIC_RDY\n");
    return (VC1DEC_PIC_RDY);
  } else
    return VC1DEC_ABORTED;
}

/*------------------------------------------------------------------------------

    Function name: VC1DecNextPicture_INTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        VC1DEC_OK               No picture available.
        VC1DEC_PIC_RDY          Picture ready.
        VC1DEC_PARAM_ERROR      Error with function parameters.
        VC1DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
VC1DecRet VC1DecNextPicture_INTERNAL( VC1DecInst     dec_inst,
                                      VC1DecPicture  *picture,
                                      u32            end_of_stream) {

  /* Variables */

  VC1DecRet return_value = VC1DEC_PIC_RDY;
  decContainer_t *dec_cont;
  picture_t *p_pic;
  u32 pic_index;
  u32 field_to_return = 0;
  u32 err_mbs, pic_id;
  u32 decode_id[2];
  PpUnitIntConfig *ppu_cfg;
  u32 i;
  /* Code */

  DEC_API_TRC("VC1DecNextPicture_INTERNAL#");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    DEC_API_TRC("VC1DecNextPicture_INTERNAL# ERROR: picture is NULL");
    return(VC1DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1SwDecNextPicture_INTERNAL# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  (void) DWLmemset(picture, 0, sizeof(VC1DecPicture));
  /* Check that asic is ready */
  if(dec_cont->asic_running) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 1 * 4, 0);   /* stop HW */

    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);    /* release HW lock */

    dec_cont->asic_running = 0;
  }

  /* Query next picture */
  if( vc1hwdNextPicture( &dec_cont->storage, &pic_index, &field_to_return,
                         end_of_stream, 0,
                         &pic_id, decode_id, &err_mbs ) == HANTRO_NOK ) {
    picture->pictures[0].output_picture = NULL;
    picture->pictures[0].output_picture_bus_address = 0;
    picture->pictures[0].coded_width = 0;
    picture->pictures[0].coded_height = 0;
    picture->pictures[0].frame_width = 0;
    picture->pictures[0].frame_height = 0;
    picture->key_picture = 0;
    picture->range_red_frm = 0;
    picture->range_map_yflag = 0;
    picture->range_map_y = 0;
    picture->range_map_uv_flag = 0;
    picture->range_map_uv = 0;
    picture->pic_id = 0;
    picture->decode_id[0] = picture->decode_id[1] = -1;
    picture->pic_coding_type[0] = 0;
    picture->pic_coding_type[1] = 0;
    picture->interlaced = 0;
    picture->field_picture = 0;
    picture->top_field = 0;
    picture->first_field = 0;
    picture->anchor_picture = 0;
    picture->repeat_first_field = 0;
    picture->repeat_frame_count = 0;
    picture->number_of_err_mbs = 0;
    picture->pictures[0].output_format = DEC_OUT_FRM_RASTER_SCAN;
    return_value = VC1DEC_OK;
  } else {
    p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
    if (!dec_cont->pp_enabled) {
      picture->pictures[0].output_picture    = (u8*)p_pic[pic_index].data.virtual_address;
      picture->pictures[0].output_picture_bus_address = p_pic[pic_index].data.bus_address;
      picture->pictures[0].coded_width        = p_pic[pic_index].coded_width;
      picture->pictures[0].coded_height       = p_pic[pic_index].coded_height;
      picture->pictures[0].frame_width        = ( picture->pictures[0].coded_width + 15 ) & ~15;
      picture->pictures[0].frame_height       = ( picture->pictures[0].coded_height + 15 ) & ~15;
      if (dec_cont->tiled_stride_enable)
        picture->pictures[0].pic_stride       = NEXT_MULTIPLE(picture->pictures[0].frame_width * 4,
                                                              ALIGN(dec_cont->align));
      else
        picture->pictures[0].pic_stride       = picture->pictures[0].frame_width * 4;
      picture->pictures[0].pic_stride_ch         = picture->pictures[0].pic_stride;
      switch( p_pic[pic_index].tiled_mode ) {
      case 0:
        picture->pictures[0].output_format = DEC_OUT_FRM_RASTER_SCAN;
        break;
      case 1:
        picture->pictures[0].output_format = DEC_OUT_FRM_TILED_4X4;
        break;
      }
    } else {
      ppu_cfg = p_pic[pic_index].ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        picture->pictures[i].output_picture    = (u8*)((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->luma_offset);
        picture->pictures[i].output_picture_bus_address = p_pic[pic_index].pp_data->bus_address + ppu_cfg->luma_offset;
        picture->pictures[i].coded_width        = ppu_cfg->scale.width;
        picture->pictures[i].coded_height       = ppu_cfg->scale.height;
        picture->pictures[i].frame_width        = NEXT_MULTIPLE(ppu_cfg->scale.width, ALIGN(ppu_cfg->align));
        picture->pictures[i].frame_height       = ppu_cfg->scale.height;
        picture->pictures[i].pic_stride         = ppu_cfg->ystride;
        picture->pictures[i].pic_stride_ch      = ppu_cfg->cstride;
        CheckOutputFormat(dec_cont->ppu_cfg, &picture->pictures[i].output_format, i);
      }
    }
    picture->key_picture        = p_pic[pic_index].key_frame;
    picture->range_red_frm       = p_pic[pic_index].range_red_frm;
    picture->range_map_yflag     = p_pic[pic_index].range_map_yflag;
    picture->range_map_y         = p_pic[pic_index].range_map_y;
    picture->range_map_uv_flag    = p_pic[pic_index].range_map_uv_flag;
    picture->range_map_uv        = p_pic[pic_index].range_map_uv;

    picture->interlaced        =
      (p_pic[pic_index].fcm == PROGRESSIVE) ? 0 : 1;
    picture->field_picture      =
      (p_pic[pic_index].fcm == FIELD_INTERLACE) ? 1 : 0;
    picture->top_field          = ((1-field_to_return) ==
                                   p_pic[pic_index].is_top_field_first) ? 1 : 0;
    picture->first_field        = (field_to_return == 0) ? 1 : 0;
    picture->anchor_picture     =
      ((p_pic[pic_index].field[field_to_return].type == PTYPE_I) ||
       (p_pic[pic_index].field[field_to_return].type == PTYPE_P)) ? 1 : 0;
    picture->repeat_first_field  = p_pic[pic_index].rff;
    picture->repeat_frame_count  = p_pic[pic_index].rptfrm;
    picture->pic_id             = pic_id;
    picture->pic_coding_type[0]  = p_pic[pic_index].pic_code_type[0];
    picture->pic_coding_type[1]  = p_pic[pic_index].pic_code_type[1];
    picture->decode_id[0]       = decode_id[0];
    picture->decode_id[1]       = decode_id[1];
    picture->number_of_err_mbs    = err_mbs;
    return_value                 =
      p_pic[pic_index].field[field_to_return].return_value;

    if(return_value == VC1DEC_PIC_RDY) {
#ifdef USE_PICTURE_DISCARD
      if (dec_cont->storage.p_pic_buf[pic_index].first_show)
#endif
      {
        dec_cont->storage.picture_info[dec_cont->fifo_index] = *picture;
        if (BqueueWaitBufNotInUse( &dec_cont->storage.bq, pic_index) != HANTRO_OK)
          return VC1DEC_ABORTED;
        if(dec_cont->pp_enabled) {
          InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,dec_cont->storage.p_pic_buf[pic_index].pp_data->virtual_address);
        }

        if((field_to_return && picture->interlaced) || !picture->interlaced) {
          BqueueSetBufferAsUsed(&dec_cont->storage.bq, pic_index);
          if (dec_cont->storage.p_pic_buf[pic_index].buffered &&
               ((dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE &&
               !dec_cont->storage.pic_layer.is_ff) ||
               dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE) &&
               dec_cont->storage.p_pic_buf[pic_index].first_show) {
            dec_cont->storage.p_pic_buf[pic_index].buffered--;
          }
          dec_cont->storage.p_pic_buf[pic_index].first_show = 0;
          if(dec_cont->pp_enabled) {
            InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,dec_cont->storage.p_pic_buf[pic_index].pp_data->virtual_address);
            BqueuePictureRelease(&dec_cont->storage.bq, pic_index);
          }
        }

        FifoPush(dec_cont->fifo_display, (FifoObject)(addr_t)dec_cont->fifo_index, FIFO_EXCEPTION_DISABLE);
        dec_cont->fifo_index++;
        if(dec_cont->fifo_index == 32)
          dec_cont->fifo_index = 0;
      }
    }
  }

  return return_value;
}
/*------------------------------------------------------------------------------

    Function name: VC1DecPictureConsumed

    Functional description:
        Release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        VC1DEC_PARAM_ERROR         Decoder instance or picture is null
        VC1DEC_NOT_INITIALIZED     Decoder instance isn't initialized
        VC1DEC_OK                          picture release success
------------------------------------------------------------------------------*/
VC1DecRet VC1DecPictureConsumed(VC1DecInst dec_inst, VC1DecPicture * picture) {
  /* Variables */
  decContainer_t *dec_cont;
  u32 i;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */

  DEC_API_TRC("VC1DecPictureConsumed#");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    DEC_API_TRC("VC1DecPictureConsumed# ERROR: picture is NULL");
    return(VC1DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecPictureConsumed# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->storage.work_buf_amount; i++) {
      if(picture->pictures[0].output_picture_bus_address == dec_cont->storage.p_pic_buf[i].data.bus_address
          && (addr_t)picture->pictures[0].output_picture
          == (addr_t)dec_cont->storage.p_pic_buf[i].data.virtual_address) {
        BqueuePictureRelease(&dec_cont->storage.bq, i);
        return (VC1DEC_OK);
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
    return (VC1DEC_OK);
  }
  return (VC1DEC_PARAM_ERROR);
}

VC1DecRet VC1DecEndOfStream(VC1DecInst dec_inst, u32 strm_end_flag) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;
  VC1DecRet ret;
  VC1DecPicture output;

  DEC_API_TRC("VC1DecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecEndOfStream# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  /* Do not do it twice */
  pthread_mutex_lock(&dec_cont->protect_mutex);
  if(dec_cont->dec_stat == VC1DEC_STREAM_END) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (VC1DEC_OK);
  }

  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->vc1_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  while((ret = VC1DecNextPicture_INTERNAL(dec_inst, &output, 1)) == VC1DEC_PIC_RDY);
  if(ret == VC1DEC_ABORTED) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (VC1DEC_ABORTED);
  }

  if(strm_end_flag) {
    dec_cont->dec_stat = VC1DEC_STREAM_END;
    FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);
  }

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  DEC_API_TRC("VC1DecEndOfStream# VC1DEC_OK\n");
  return (VC1DEC_OK);
}


/*------------------------------------------------------------------------------

    Function name: Vc1CheckFormatSupport

    Functional description:
        Check if VC-1 supported

    Input:
        container

    Return values:
        return zero for OK

------------------------------------------------------------------------------*/
u32 Vc1CheckFormatSupport(void) {
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VC1_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  return (hw_feature.vc1_support == VC1_NOT_SUPPORTED);
}

/*------------------------------------------------------------------------------

    Function name: VC1DecPeek

    Functional description:
        Retrieve last decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct

    Output:
        picture Decoder output picture.

    Return values:
        VC1DEC_OK               No picture available.
        VC1DEC_PIC_RDY          Picture ready.
        VC1DEC_PARAM_ERROR      Error with function parameters.
        VC1DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
VC1DecRet VC1DecPeek( VC1DecInst     dec_inst,
                      VC1DecPicture  *picture ) {

  /* Variables */

  decContainer_t *dec_cont;
  picture_t *p_pic;

  /* Code */

  DEC_API_TRC("VC1DecPeek#");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    DEC_API_TRC("VC1DecPeek# ERROR: picture is NULL");
    return(VC1DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecPeek# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  /* Query next picture */
  /* when output release thread enabled, VC1DecNextPicture_INTERNAL() called in
     VC1DecDecode(), and so dec_cont->fullness used to sample the real outCount
     in case of VC1DecNextPicture_INTERNAL() called before than VC1DecPeek() */
  u32 tmp = dec_cont->fullness;

  if( tmp == 0 ||
      ( dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE &&
        dec_cont->storage.pic_layer.is_ff ) ) {
    DWLmemset(picture, 0, sizeof(VC1DecPicture));
    return VC1DEC_OK;
  }

  p_pic = (picture_t*)dec_cont->storage.p_pic_buf +
          dec_cont->storage.work_out;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].output_picture    = (u8*)p_pic->data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->data.bus_address;
    picture->pictures[0].coded_width        = p_pic->coded_width;
    picture->pictures[0].coded_height       = p_pic->coded_height;
    picture->pictures[0].frame_width        = ( picture->pictures[0].coded_width + 15 ) & ~15;
    picture->pictures[0].frame_height       = ( picture->pictures[0].coded_height + 15 ) & ~15;
  } else {
    picture->pictures[0].output_picture = (u8 *) p_pic->pp_data->virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->pp_data->bus_address;
    picture->pictures[0].coded_width        = p_pic->coded_width >> dec_cont->dscale_shift_x;
    picture->pictures[0].coded_height       = p_pic->coded_height >> dec_cont->dscale_shift_y;
    picture->pictures[0].frame_width        = ( (picture->pictures[0].coded_width + 15 ) & ~15) >> dec_cont->dscale_shift_x;
    picture->pictures[0].frame_height       = ( (picture->pictures[0].coded_height + 15 ) & ~15) >> dec_cont->dscale_shift_y;
  }
  picture->key_picture        = p_pic->key_frame;
  picture->range_red_frm       = p_pic->range_red_frm;
  picture->range_map_yflag     = p_pic->range_map_yflag;
  picture->range_map_y         = p_pic->range_map_y;
  picture->range_map_uv_flag    = p_pic->range_map_uv_flag;
  picture->range_map_uv        = p_pic->range_map_uv;
  switch( p_pic->tiled_mode ) {
  case 0:
    picture->pictures[0].output_format = DEC_OUT_FRM_RASTER_SCAN;
    break;
  case 1:
    picture->pictures[0].output_format = DEC_OUT_FRM_TILED_4X4;
    break;
  }

  picture->interlaced        = (p_pic->fcm == PROGRESSIVE) ? 0 : 1;
  picture->field_picture      = 0;
  picture->top_field          = 0;
  picture->first_field        = 0;
  picture->anchor_picture     =
    ((p_pic->field[0].type == PTYPE_I) ||
     (p_pic->field[0].type == PTYPE_P)) ? 1 : 0;
  picture->repeat_first_field  = p_pic->rff;
  picture->repeat_frame_count  = p_pic->rptfrm;
  picture->pic_id             =
    dec_cont->storage.out_pic_id[0][dec_cont->storage.prev_outp_idx];
  picture->pic_coding_type[0]  = p_pic->pic_code_type[0];
  picture->pic_coding_type[1]  = p_pic->pic_code_type[1];
  picture->number_of_err_mbs    =
    dec_cont->storage.out_err_mbs[dec_cont->storage.prev_outp_idx];

  return VC1DEC_PIC_RDY;

}

void VC1SetExternalBufferInfo(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  u32 buffers = 0;
  u32 ext_buffer_size;

  ext_buffer_size = VC1GetRefFrmSize(dec_cont);

  if( dec_cont->storage.max_bframes > 0 ) {
    buffers = 3;
  } else {
    buffers = 2;
  }

  u32 newbuffers = dec_cont->storage.max_num_buffers;
  if(newbuffers > buffers)
    buffers = newbuffers;

 if (dec_cont->pp_enabled) {
    ext_buffer_size = CalcPpUnitBufferSize(&dec_cont->ppu_cfg[0], 0);
  }

  dec_cont->storage.prev_num_mbs = dec_cont->storage.num_of_mbs;

  dec_cont->ext_min_buffer_num = dec_cont->buf_num =  buffers;
  dec_cont->next_buf_size = ext_buffer_size;
}

VC1DecRet VC1DecGetBufferInfo(VC1DecInst dec_inst, VC1DecBufferInfo *mem_info) {
  decContainer_t  * dec_cont = (decContainer_t *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;

  if(dec_cont == NULL || mem_info == NULL) {
    return VC1DEC_PARAM_ERROR;
  }

  if (dec_cont->storage.release_buffer) {
    /* Release old buffers from input queue. */
    //buffer = InputQueueGetBuffer(decCont->pp_buffer_queue, 0);
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
        return (VC1DEC_MEMFAIL);
      }
      dec_cont->storage.ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return VC1DEC_OK;
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return VC1DEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return VC1DEC_OK;
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

  return VC1DEC_WAITING_FOR_BUFFER;
}

VC1DecRet VC1DecAddBuffer(VC1DecInst dec_inst, struct DWLLinearMem *info) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  VC1DecRet dec_ret = VC1DEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return VC1DEC_PARAM_ERROR;
  }

  u32 i = dec_cont->buffer_index;

  if (dec_cont->buffer_index >= 16)
    /* Too much buffers added. */
    return VC1DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = VC1DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    dec_cont->storage.p_pic_buf[i].data = *info;
    dec_cont->storage.p_pic_buf[i].coded_width = dec_cont->storage.max_coded_width;
    dec_cont->storage.p_pic_buf[i].coded_height = dec_cont->storage.max_coded_height;
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->storage.bq.queue_size++;
      dec_cont->storage.work_buf_amount++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->storage.ext_buffer_added = 1;
  return dec_ret;
}


void VC1EnterAbortState(decContainer_t *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->storage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}


void VC1ExistAbortState(decContainer_t *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->storage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void VC1EmptyBufferQueue(decContainer_t *dec_cont) {
  BqueueEmpty(&dec_cont->storage.bq);
  dec_cont->storage.work_out = 0;
  dec_cont->storage.work_out_prev = 0;
  dec_cont->storage.work0 =
    dec_cont->storage.work1 = INVALID_ANCHOR_PICTURE;
}

void VC1StateReset(decContainer_t *dec_cont) {
  u32 buffers = 0;

  if( dec_cont->storage.max_bframes > 0 ) {
    buffers = 3;
  } else {
    buffers = 2;
  }
  u32 newbuffers = dec_cont->storage.max_num_buffers;
  if(newbuffers > buffers)
    buffers = newbuffers;

  /* Clear parameters in decContainer_t */
  dec_cont->dec_stat = VC1DEC_STREAMDECODING;
  dec_cont->pic_number = 0;
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
  dec_cont->same_pic_header = 0;

  /* Clear parameters in swStrmStorage_t */
  dec_cont->storage.first_frame = HANTRO_TRUE;
  dec_cont->storage.picture_broken = HANTRO_FALSE;
  dec_cont->storage.prev_bidx = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->storage.work_buf_amount = buffers;
  dec_cont->storage.bq.queue_size = buffers;
#endif
  dec_cont->storage.field_to_return = 0;
  dec_cont->storage.outp_idx = 0;
  dec_cont->storage.prev_outp_idx = 0;
  dec_cont->storage.outp_count = 0;
  dec_cont->storage.field_count = 0;
  dec_cont->storage.rnd = 0;
  dec_cont->storage.skip_b = 0;
  dec_cont->storage.keep_hw_reserved = 0;
  dec_cont->storage.resolution_changed = 0;
  dec_cont->storage.prev_dec_result = 0;
  dec_cont->storage.slice = HANTRO_FALSE;
  dec_cont->storage.missing_field = HANTRO_FALSE;
  dec_cont->storage.release_buffer = 0;
  dec_cont->storage.ext_buffer_added = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->storage.hdrs_decoded = 0;
#endif

#ifdef USE_OMXIL_BUFFER
  if (!dec_cont->pp_enabled)
    (void) DWLmemset(dec_cont->storage.p_pic_buf, 0, 16 * sizeof(picture_t));
  (void) DWLmemset(dec_cont->storage.picture_info, 0, 32 * sizeof(VC1DecPicture));
#endif
  (void) DWLmemset(&dec_cont->storage.pic_layer, 0, sizeof(pictureLayer_t));
  (void) DWLmemset(&dec_cont->storage.tmp_strm_data, 0, sizeof(strmData_t));
  (void) DWLmemset(dec_cont->storage.outp_buf, 0, 16 * sizeof(u16x));
  (void) DWLmemset(dec_cont->storage.out_pic_id, 0, 32 * sizeof(u16x));
  (void) DWLmemset(dec_cont->storage.out_err_mbs, 0, 16 * sizeof(u16x));
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

VC1DecRet VC1DecAbort(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;

  DEC_API_TRC("VC1DecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecAbort# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  VC1EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  DEC_API_TRC("VC1DecAbort# VC1DEC_OK\n");
  return (VC1DEC_OK);
}

VC1DecRet VC1DecAbortAfter(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;

  DEC_API_TRC("VC1DecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    DEC_API_TRC("VC1DecAbortAfter# ERROR: Decoder not initialized");
    return(VC1DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_stat == VC1DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (VC1DEC_OK);
  }
#endif

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->vc1_regs[1] | DEC_IRQ_DISABLE);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  } else if(dec_cont->storage.keep_hw_reserved) {
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->storage.keep_hw_reserved = 0;
  }

  /* Clear any remaining pictures from DPB */
  VC1EmptyBufferQueue(dec_cont);

  VC1StateReset(dec_cont);

  VC1ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  DEC_API_TRC("VC1DecAbortAfter# VC1DEC_OK\n");
  return (VC1DEC_OK);
}

VC1DecRet VC1DecSetInfo(VC1DecInst dec_inst,
                        struct VC1DecConfig *dec_cfg) {
  /*@null@ */ decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  u32 pic_width = dec_cont->storage.cur_coded_width;
  u32 pic_height = dec_cont->storage.cur_coded_height;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VC1_DEC);
  struct DecHwFeatures hw_feature;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (!hw_feature.dec_stride_support) {
    /* For verion earlier than g1v8_2, reset alignment to 16B. */
    dec_cont->align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = dec_cfg->align;
  }
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, 8, !dec_cont->storage.interlace, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(VC1DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, dec_cont->storage.interlace, dec_cont->ppu_cfg))
    return VC1DEC_PARAM_ERROR;
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
  return (VC1DEC_OK);
}


void VC1CheckBufferRealloc(decContainer_t *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (VC1GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->storage.prev_coded_width != dec_cont->storage.cur_coded_width ||
          dec_cont->storage.prev_coded_height != dec_cont->storage.cur_coded_height)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (VC1GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (VC1GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}
