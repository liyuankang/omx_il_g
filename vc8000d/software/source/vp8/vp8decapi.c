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

#include "basetype.h"
#include "decapicommon.h"
#include "vp8decapi.h"
#include "vp8decmc_internals.h"
#include "version.h"
#include "dwl.h"
#include "vp8hwd_buffer_queue.h"
#include "vp8hwd_container.h"
#include "vp8hwd_debug.h"
#include "tiledref.h"
#include "vp8hwd_asic.h"
#include "regdrv.h"
#include "refbuffer.h"
#include "vp8hwd_asic.h"
#include "vp8hwd_headers.h"
#include "errorhandling.h"
#include "vpufeature.h"
#include "ppu.h"
#include <stdio.h>
#include "sw_util.h"
#define VP8DEC_MAJOR_VERSION 1
#define VP8DEC_MINOR_VERSION 0

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#include <stdio.h>
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

#ifdef VP8DEC_TRACE
#define DEC_API_TRC(str)    VP8DecTrace(str)
#else
#define DEC_API_TRC(str)    do{}while(0)
#endif

#define MB_MULTIPLE(x)  (((x)+15)&~15)

#define VP8_MIN_WIDTH  48
#define VP8_MIN_HEIGHT 48
#define VP8_MIN_WIDTH_EN_DTRC  48
#define VP8_MIN_HEIGHT_EN_DTRC 48
#define MAX_PIC_SIZE   4096*4096

#define EOS_MARKER (-1)
#define FLUSH_MARKER (-2)

static u32 vp8hwdCheckSupport( VP8DecContainer_t *dec_cont );
static void vp8hwdFreeze(VP8DecContainer_t *dec_cont);
static u32 CheckBitstreamWorkaround(vp8_decoder_t* dec);
#if 0
static void DoBitstreamWorkaround(vp8_decoder_t* dec, DecAsicBuffers_t *p_asic_buff, vpBoolCoder_t*bc);
#endif
void vp8hwdErrorConceal(VP8DecContainer_t *dec_cont, addr_t bus_address,
                        u32 conceal_everything);
static struct DWLLinearMem* GetPrevRef(VP8DecContainer_t *dec_cont);
void ConcealRefAvailability(u32 * output, u32 height, u32 width, u32 tiled_stride_enable, u32 align);

i32 FindIndex(VP8DecContainer_t* dec_cont, const u32* address);
i32 FindPpIndex(VP8DecContainer_t* dec_cont, const u32* address);
static VP8DecRet VP8DecNextPicture_INTERNAL(VP8DecInst dec_inst,
    VP8DecPicture * output, u32 end_of_stream);
static VP8DecRet VP8PushOutput(VP8DecContainer_t* dec_cont);

static void VP8SetExternalBufferInfo(VP8DecInst dec_inst);

static void VP8EnterAbortState(VP8DecContainer_t* dec_cont);
static void VP8ExistAbortState(VP8DecContainer_t* dec_cont);
static void VP8EmptyBufferQueue(VP8DecContainer_t* dec_cont);
static void VP8CheckBufferRealloc(VP8DecContainer_t* dec_cont);
extern void VP8HwdBufferQueueSetAbort(BufferQueue queue);
extern void VP8HwdBufferQueueClearAbort(BufferQueue queue);
extern void VP8HwdBufferQueueEmptyRef(BufferQueue queue, i32 buffer);


/*------------------------------------------------------------------------------
    Function name : VP8DecGetAPIVersion
    Description   : Return the API version information

    Return type   : VP8DecApiVersion
    Argument      : void
------------------------------------------------------------------------------*/
VP8DecApiVersion VP8DecGetAPIVersion(void) {
  VP8DecApiVersion ver;

  ver.major = VP8DEC_MAJOR_VERSION;
  ver.minor = VP8DEC_MINOR_VERSION;

  DEC_API_TRC("VP8DecGetAPIVersion# OK\n");

  return ver;
}

/*------------------------------------------------------------------------------
    Function name : VP8DecGetBuild
    Description   : Return the SW and HW build information

    Return type   : VP8DecBuild
    Argument      : void
------------------------------------------------------------------------------*/
VP8DecBuild VP8DecGetBuild(void) {
  VP8DecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_VP8_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_VP8_DEC);

  DEC_API_TRC("VP8DecGetBuild# OK\n");

  return build_info;
}

/*------------------------------------------------------------------------------
    Function name   : vp8decinit
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst * dec_inst
                      enum DecErrorHandling error_handling
------------------------------------------------------------------------------*/
VP8DecRet VP8DecInit(VP8DecInst * dec_inst,
                     const void *dwl,
                     VP8DecFormat dec_format,
                     enum DecErrorHandling error_handling,
                     u32 num_frame_buffers,
                     enum DecDpbFlags dpb_flags,
                     u32 use_adaptive_buffers,
                     u32 n_guard_size) {
  VP8DecContainer_t *dec_cont;
  u32 asic_id, hw_build_id;
  u32 reference_frame_format;
  struct DecHwFeatures hw_feature;

  DEC_API_TRC("VP8DecInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if(dec_inst == NULL) {
    DEC_API_TRC("VP8DecInit# ERROR: dec_inst == NULL");
    return (VP8DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;   /* return NULL instance for any error */

  /* check that decoding supported in HW */
  {

    DWLHwConfig hw_cfg;

    DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_VP8_DEC);
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
    GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
    if(dec_format == VP8DEC_VP7 && !hw_feature.vp7_support) {
      DEC_API_TRC("VP8DecInit# ERROR: VP7 not supported in HW\n");
      return VP8DEC_FORMAT_NOT_SUPPORTED;
    }

    if((dec_format == VP8DEC_VP8 || dec_format == VP8DEC_WEBP) &&
        !hw_feature.vp8_support) {
      DEC_API_TRC("VP8DecInit# ERROR: VP8 not supported in HW\n");
      return VP8DEC_FORMAT_NOT_SUPPORTED;
    }

  }

  /* allocate instance */
  dec_cont = (VP8DecContainer_t *) DWLmalloc(sizeof(VP8DecContainer_t));

  if(dec_cont == NULL) {
    DEC_API_TRC("VP8DecInit# ERROR: Memory allocation failed\n");
    return VP8DEC_MEMFAIL;
  }

  (void) DWLmemset(dec_cont, 0, sizeof(VP8DecContainer_t));
  dec_cont->dwl = dwl;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  /* initial setup of instance */

  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_VP8_DEC);
  if((asic_id >> 16) == 0x8170U)
    error_handling = 0;

  dec_cont->dec_stat = VP8DEC_INITIALIZED;
  dec_cont->checksum = dec_cont;  /* save instance as a checksum */

  if( num_frame_buffers > VP8DEC_MAX_PIC_BUFFERS)
    num_frame_buffers = VP8DEC_MAX_PIC_BUFFERS;
  switch(dec_format) {
  case VP8DEC_VP7:
    dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP7;
    if(num_frame_buffers < 3)
      num_frame_buffers = 3;
    break;
  case VP8DEC_VP8:
    dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP8;
    if(num_frame_buffers < 4)
      num_frame_buffers = 4;
    break;
  case VP8DEC_WEBP:
    dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP8;
    dec_cont->intra_only = HANTRO_TRUE;
    num_frame_buffers = 1;
    break;
  }
  dec_cont->num_buffers = num_frame_buffers;
  dec_cont->num_buffers_reserved = num_frame_buffers;
  VP8HwdAsicInit(dec_cont);   /* Init ASIC */

  if(!DWLReadAsicCoreCount()) {
    return (VP8DEC_DWL_ERROR);
  }
  dec_cont->num_cores = 1;

  if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
    DEC_API_TRC("VP8DecInit# ERROR: HW not support 64bit address!\n");
    return (VP8DEC_PARAM_ERROR);
  }

  if (hw_feature.ref_frame_tiled_only)
    dpb_flags = DEC_REF_FRM_TILED_DEFAULT;

  dec_cont->ref_buf_support = hw_feature.ref_buf_support;
  reference_frame_format = dpb_flags & DEC_REF_FRM_FMT_MASK;
  if(reference_frame_format == DEC_REF_FRM_TILED_DEFAULT) {
    /* Assert support in HW before enabling.. */
    if(!hw_feature.tiled_mode_support) {
      DEC_API_TRC("VP8DecInit# ERROR: Tiled reference picture format not supported in HW\n");
      DWLfree(dec_cont);
      return VP8DEC_FORMAT_NOT_SUPPORTED;
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
    return (VP8DEC_PARAM_ERROR);
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
    return (VP8DEC_MEMFAIL);
  }
  dec_cont->asic_buff->release_buffer = 0;
  dec_cont->intra_freeze = error_handling == DEC_EC_VIDEO_FREEZE;
  if (error_handling == DEC_EC_PARTIAL_FREEZE)
    dec_cont->partial_freeze = 1;
  else if (error_handling == DEC_EC_PARTIAL_IGNORE)
    dec_cont->partial_freeze = 2;
  dec_cont->picture_broken = 0;
  dec_cont->decoder.refbu_pred_hits = 0;

  if (!error_handling && dec_format == VP8DEC_VP8)
    dec_cont->hw_ec_support = hw_feature.ec_support;

  if ((dec_format == VP8DEC_VP8) || (dec_format == VP8DEC_WEBP))
    dec_cont->stride_support = hw_feature.dec_stride_support;
  if (FifoInit(VP8DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_out) != FIFO_OK)
    return VP8DEC_MEMFAIL;

  dec_cont->use_adaptive_buffers = use_adaptive_buffers;
  dec_cont->n_guard_size = n_guard_size;

  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */
  if (dec_cont->tiled_mode_support) {
    dec_cont->min_dec_pic_width = VP8_MIN_WIDTH_EN_DTRC;
    dec_cont->min_dec_pic_height = VP8_MIN_HEIGHT_EN_DTRC;
  }
  else {
    dec_cont->min_dec_pic_width = VP8_MIN_WIDTH;
    dec_cont->min_dec_pic_height = VP8_MIN_HEIGHT;
  }

  /* return new instance to application */
  *dec_inst = (VP8DecInst) dec_cont;

  DEC_API_TRC("VP8DecInit# OK\n");
  return (VP8DEC_OK);

}

/*------------------------------------------------------------------------------
    Function name   : VP8DecRelease
    Description     :
    Return type     : void
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
void VP8DecRelease(VP8DecInst dec_inst) {

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  const void *dwl;

  DEC_API_TRC("VP8DecRelease#\n");

  if(dec_cont == NULL) {
    DEC_API_TRC("VP8DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecRelease# ERROR: Decoder not initialized\n");
    return;
  }

  dwl = dec_cont->dwl;

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if(dec_cont->asic_running) {
    DWLDisableHw(dwl, dec_cont->core_id, 1 * 4, 0);    /* stop HW */
    DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  VP8HwdAsicReleaseMem(dec_cont);
  VP8HwdAsicReleasePictures(dec_cont);
  if (dec_cont->hw_ec_support)
    vp8hwdReleaseEc(&dec_cont->ec);

  if (dec_cont->fifo_out)
    FifoRelease(dec_cont->fifo_out);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  DEC_API_TRC("VP8DecRelease# OK\n");

  return;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecGetInfo
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecInfo * dec_info
------------------------------------------------------------------------------*/
VP8DecRet VP8DecGetInfo(VP8DecInst dec_inst, VP8DecInfo * dec_info) {
  const VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;

  DEC_API_TRC("VP8DecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    DEC_API_TRC("VP8DecGetInfo# ERROR: dec_inst or dec_info is NULL\n");
    return VP8DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecGetInfo# ERROR: Decoder not initialized\n");
    return VP8DEC_NOT_INITIALIZED;
  }

  if (dec_cont->dec_stat == VP8DEC_INITIALIZED) {
    return VP8DEC_HDRS_NOT_RDY;
  }

  dec_info->vp_version = dec_cont->decoder.vp_version;
  dec_info->vp_profile = dec_cont->decoder.vp_profile;
  dec_info->pic_buff_size = dec_cont->buf_num;

  if(dec_cont->tiled_mode_support) {
    dec_info->output_format = VP8DEC_TILED_YUV420;
  } else {
    dec_info->output_format = VP8DEC_SEMIPLANAR_YUV420;
  }

  /* Fragments have 8 pixels */
  dec_info->coded_width = dec_cont->decoder.width;
  dec_info->coded_height = dec_cont->decoder.height;
  dec_info->frame_width = (dec_cont->decoder.width + 15) & ~15;
  dec_info->frame_height = (dec_cont->decoder.height + 15) & ~15;
  dec_info->scaled_width = dec_cont->decoder.scaled_width;
  dec_info->scaled_height = dec_cont->decoder.scaled_height;
  dec_info->dpb_mode = DEC_DPB_FRAME;

  return VP8DEC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecDecode
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : const VP8DecInput * input
    Argument        : VP8DecFrame * output
------------------------------------------------------------------------------*/
VP8DecRet VP8DecDecode(VP8DecInst dec_inst,
                       const VP8DecInput * input, VP8DecOutput * output) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;
  i32 ret;
  u32 asic_status;
  u32 error_concealment = 0;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  DEC_API_TRC("VP8DecDecode#\n");

  /* Check that function input parameters are valid */
  if(input == NULL || output == NULL || dec_inst == NULL) {
    DEC_API_TRC("VP8DecDecode# ERROR: NULL arg(s)\n");
    return (VP8DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecDecode# ERROR: Decoder not initialized\n");
    return (VP8DEC_NOT_INITIALIZED);
  }

  DWLmemset(output, 0, sizeof(VP8DecOutput));

  if (dec_cont->abort) {
    return (VP8DEC_ABORTED);
  }

  if(((input->data_len > dec_cont->max_strm_len) && !dec_cont->intra_only) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address)) {
    DEC_API_TRC("VP8DecDecode# ERROR: Invalid arg value\n");
    return VP8DEC_PARAM_ERROR;
  }

  if ((input->p_pic_buffer_y != NULL && input->pic_buffer_bus_address_y == 0) ||
      (input->p_pic_buffer_y == NULL && input->pic_buffer_bus_address_y != 0) ||
      (input->p_pic_buffer_c != NULL && input->pic_buffer_bus_address_c == 0) ||
      (input->p_pic_buffer_c == NULL && input->pic_buffer_bus_address_c != 0) ||
      (input->p_pic_buffer_y == NULL && input->p_pic_buffer_c != 0) ||
      (input->p_pic_buffer_y != NULL && input->p_pic_buffer_c == 0)) {
    DEC_API_TRC("VP8DecDecode# ERROR: Invalid arg value\n");
    return VP8DEC_PARAM_ERROR;
  }

#ifdef VP8DEC_EVALUATION
  if(dec_cont->pic_number > VP8DEC_EVALUATION) {
    DEC_API_TRC("VP8DecDecode# VP8DEC_EVALUATION_LIMIT_EXCEEDED\n");
    return VP8DEC_EVALUATION_LIMIT_EXCEEDED;
  }
#endif

  if(!input->data_len && dec_cont->stream_consumed_callback ) {
    dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
    return VP8DEC_OK;
  }
  /* aliases */
  p_asic_buff = dec_cont->asic_buff;

  if (dec_cont->no_decoding_buffer || dec_cont->get_buffer_after_abort) {
    p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
    if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
      if (dec_cont->abort)
        return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else {
        output->data_left = input->data_len;
        dec_cont->no_decoding_buffer = 1;
        return VP8DEC_NO_DECODING_BUFFER;
      }
#endif
    }
    p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
    if (dec_cont->pp_enabled && !dec_cont->intra_only &&
       !p_asic_buff->strides_used) {
      p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
      if (p_asic_buff->pp_buffer == NULL) {
        return VP8DEC_ABORTED;
      }
      p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
      p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
    }

    p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
    dec_cont->no_decoding_buffer = 0;

    if (dec_cont->get_buffer_after_abort) {
      dec_cont->get_buffer_after_abort = 0;
      VP8HwdBufferQueueUpdateRef(dec_cont->bq,
          BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
          p_asic_buff->out_buffer_i);

      if(dec_cont->intra_only != HANTRO_TRUE) {
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
      }
    }

    if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
      return VP8DEC_ABORTED;
  }

  if((!dec_cont->intra_only) && input->slice_height ) {
    DEC_API_TRC("VP8DecDecode# ERROR: Invalid arg value\n");
    return VP8DEC_PARAM_ERROR;
  }
  if(dec_cont->intra_only) {
    u32 index = p_asic_buff->out_buffer_i;
    if (dec_cont->pp_enabled)
      index = p_asic_buff->pp_buffer_map[index];
    while(dec_cont->asic_buff->not_displayed[index])
      sched_yield();
  }

  /* application indicates that slice mode decoding should be used ->
   * disabled unless WebP not used */
  if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC &&
      dec_cont->intra_only && input->slice_height) {
    DWLHwConfig hw_config;
    u32 tmp;
    /* Slice mode can only be enabled if image width is larger
     * than supported video decoder maximum width. */
    DWLReadAsicConfig(&hw_config, DWL_CLIENT_TYPE_VP8_DEC);
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
    GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
    if( input->data_len >= 5 ) {
      /* Peek frame width. We make shortcuts and assumptions:
       *  -always keyframe
       *  -always VP8 (i.e. not VP7)
       *  -keyframe start code and frame tag skipped
       *  -if keyframe start code invalid, handle it later */
      tmp = (input->stream[7] << 8)|
            (input->stream[6]); /* Read 16-bit chunk */
      tmp = tmp & 0x3fff;
      if( tmp > hw_feature.vp8_max_dec_pic_width ) {
        if(input->slice_height > 255) {
          DEC_API_TRC("VP8DecDecode# ERROR: Slice height > max\n");
          return VP8DEC_PARAM_ERROR;
        }

        dec_cont->slice_height = input->slice_height;
      } else {
        dec_cont->slice_height = 0;
      }
    } else {
      /* Too little data in buffer, let later error management
       * handle it. Disallow slice mode. */
    }
  }

  if (dec_cont->intra_only && input->p_pic_buffer_y) {
    dec_cont->user_mem = 1;
    dec_cont->asic_buff->user_mem.p_pic_buffer_y[0] = input->p_pic_buffer_y;
    dec_cont->asic_buff->user_mem.pic_buffer_bus_addr_y[0] =
      input->pic_buffer_bus_address_y;
    dec_cont->asic_buff->user_mem.p_pic_buffer_c[0] = input->p_pic_buffer_c;
    dec_cont->asic_buff->user_mem.pic_buffer_bus_addr_c[0] =
      input->pic_buffer_bus_address_c;
  }

  if (dec_cont->dec_stat == VP8DEC_NEW_HEADERS) {
#ifdef SLICE_MODE_LARGE_PIC
    if((dec_cont->asic_buff->width*
        dec_cont->asic_buff->height > WEBP_MAX_PIXEL_AMOUNT_NONSLICE) &&
        dec_cont->intra_only &&
        (dec_cont->slice_height == 0)) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      return VP8DEC_STREAM_NOT_SUPPORTED;
    }
#endif

    dec_cont->dec_stat = VP8DEC_DECODING;
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    VP8CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {

        VP8HwdAsicReleaseMem(dec_cont);
        if (dec_cont->hw_ec_support)
          vp8hwdReleaseEc(&dec_cont->ec);

      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        VP8HwdBufferQueueWaitNotInUse(dec_cont->bq);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
          //return VP8DEC_WAITING_FOR_BUFFER;
        }
        VP8HwdAsicReleasePictures(dec_cont);
      } else {
        if(dec_cont->stream_consumed_callback == NULL &&
            dec_cont->bq && dec_cont->intra_only != HANTRO_TRUE) {
          i32 index;
          /* Legacy single core: remove references made for the next decode. */
          if (( index = VP8HwdBufferQueueGetPrevRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetAltRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetGoldenRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                     dec_cont->asic_buff->out_buffer_i);
        }

        if(p_asic_buff->mvs[0].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[0]);

        if(p_asic_buff->mvs[1].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[1]);
      }

      if((ret = VP8HwdAsicAllocatePictures(dec_cont)) != 0 ||
        (dec_cont->hw_ec_support &&
        vp8hwdInitEc(&dec_cont->ec, dec_cont->width, dec_cont->height, 16))) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        DEC_API_TRC
        ("VP8DecDecode# ERROR: Picture memory allocation failed\n");
        if (ret == -2)
          return VP8DEC_ABORTED;
        else
          return VP8DEC_MEMFAIL;
      }

      if(VP8HwdAsicAllocateMem(dec_cont) != 0) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        DEC_API_TRC("VP8DecInit# ERROR: ASIC Memory allocation failed\n");
        return VP8DEC_MEMFAIL;
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
          //return VP8DEC_WAITING_FOR_BUFFER;
        }
      }

      VP8HwdAsicReleaseMem(dec_cont);
      if (dec_cont->hw_ec_support)
        vp8hwdReleaseEc(&dec_cont->ec);

      if (dec_cont->realloc_int_buf) {
        VP8HwdAsicReleasePictures(dec_cont);
      } else {
        if(p_asic_buff->mvs[0].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[0]);

        if(p_asic_buff->mvs[1].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[1]);
      }

      if((ret = VP8HwdAsicAllocatePictures(dec_cont)) != 0 ||
        (dec_cont->hw_ec_support &&
        vp8hwdInitEc(&dec_cont->ec, dec_cont->width, dec_cont->height, 16))) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        DEC_API_TRC
        ("VP8DecDecode# ERROR: Picture memory allocation failed\n");
        if (ret == -2)
          return VP8DEC_ABORTED;
        else
          return VP8DEC_MEMFAIL;
      }

      if(VP8HwdAsicAllocateMem(dec_cont) != 0) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        DEC_API_TRC("VP8DecInit# ERROR: ASIC Memory allocation failed\n");
        return VP8DEC_MEMFAIL;
      }

      if (dec_cont->realloc_int_buf && !dec_cont->realloc_ext_buf) {
        if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
          /* Id for first frame. */
          p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
          if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
            if ( dec_cont->abort)
                return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
            else {
              output->data_left = 0;
              dec_cont->no_decoding_buffer = 1;
              return VP8DEC_NO_DECODING_BUFFER;
            }
#endif
          }

          p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
          p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
          p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
          if (p_asic_buff->pp_buffer == NULL)
            return VP8DEC_ABORTED;
          p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
          p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;

          /* These need to point at something so use the output buffer */
          VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                     BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                     p_asic_buff->out_buffer_i);
          if(dec_cont->intra_only != HANTRO_TRUE) {
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
          }
        }
      }
    }

    if (dec_cont->realloc_ext_buf) {
      VP8SetExternalBufferInfo(dec_cont);
      dec_cont->buffer_index = 0;
      dec_cont->dec_stat = VP8DEC_WAITING_BUFFER;
      return VP8DEC_WAITING_FOR_BUFFER;
    }
  }
  else if (dec_cont->dec_stat == VP8DEC_WAITING_BUFFER) {
    if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
      return VP8DEC_WAITING_FOR_BUFFER;

    if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
      /* Id for first frame. */
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
        if ( dec_cont->abort)
            return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->data_left = 0;
          dec_cont->no_decoding_buffer = 1;
          return VP8DEC_NO_DECODING_BUFFER;
        }
#endif
      }

      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
      if (p_asic_buff->pp_buffer == NULL)
        return VP8DEC_ABORTED;
      p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
      p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
      /* These need to point at something so use the output buffer */
      VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                 BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                 p_asic_buff->out_buffer_i);
      if(dec_cont->intra_only != HANTRO_TRUE) {
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
      }
    }
    dec_cont->dec_stat = VP8DEC_DECODING;
  }
  else if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC && input->data_len) {
    dec_cont->prev_is_key = dec_cont->decoder.key_frame;
    dec_cont->decoder.probs_decoded = 0;

    /* decode frame tag */
    vp8hwdDecodeFrameTag( input->stream, &dec_cont->decoder );

    /* When on key-frame, reset probabilities and such */
    if( dec_cont->decoder.key_frame ) {
      vp8hwdResetDecoder( &dec_cont->decoder);
    }
    /* intra only and non key-frame */
    else if (dec_cont->intra_only) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      return VP8DEC_STRM_ERROR;
    }

    if (dec_cont->decoder.key_frame || dec_cont->decoder.vp_version > 0) {
      p_asic_buff->dc_pred[0] = p_asic_buff->dc_pred[1] =
                                  p_asic_buff->dc_match[0] = p_asic_buff->dc_match[1] = 0;
    }

    /* Decode frame header (now starts bool coder as well) */
    ret = vp8hwdDecodeFrameHeader(
            input->stream + dec_cont->decoder.frame_tag_size,
            input->data_len - dec_cont->decoder.frame_tag_size,
            &dec_cont->bc, &dec_cont->decoder );
    if( ret != HANTRO_OK ) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      DEC_API_TRC("VP8DecDecode# ERROR: Frame header decoding failed\n");
      if (!dec_cont->pic_number || dec_cont->dec_stat != VP8DEC_DECODING) {
        return VP8DEC_STRM_ERROR;
      } else {
        vp8hwdFreeze(dec_cont);
        if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
          return VP8DEC_ABORTED;
        DEC_API_TRC("VP8DecDecode# VP8DEC_PIC_DECODED\n");
        return VP8DEC_PIC_DECODED;
      }
    }
    /* flag the stream as non "error-resilient" */
    else if (dec_cont->decoder.refresh_entropy_probs)
      dec_cont->prob_refresh_detected = 1;

    if(CheckBitstreamWorkaround(&dec_cont->decoder)) {
      /* do bitstream workaround */
      /*DoBitstreamWorkaround(&dec_cont->decoder, p_asic_buff, &dec_cont->bc);*/
    }

    ret = vp8hwdSetPartitionOffsets(input->stream, input->data_len,
                                    &dec_cont->decoder);
    /* ignore errors in partition offsets if HW error concealment used
     * (assuming parts of stream missing -> partition start offsets may
     * be larger than amount of stream in the buffer) */
    if (ret != HANTRO_OK && !dec_cont->hw_ec_support) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      if (!dec_cont->pic_number || dec_cont->dec_stat != VP8DEC_DECODING) {
        return VP8DEC_STRM_ERROR;
      } else {
        vp8hwdFreeze(dec_cont);
        if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
          return VP8DEC_ABORTED;
        DEC_API_TRC("VP8DecDecode# VP8DEC_PIC_DECODED\n");
        return VP8DEC_PIC_DECODED;
      }
    }

    /* check for picture size change */
    if((dec_cont->width != (dec_cont->decoder.width)) ||
        (dec_cont->height != (dec_cont->decoder.height))) {

      if (dec_cont->stream_consumed_callback != NULL && dec_cont->bq) {
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   dec_cont->asic_buff->out_buffer_i);
        dec_cont->asic_buff->out_buffer_i = VP8_UNDEFINED_BUFFER;
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        /* Wait for output processing to finish before releasing. */
        VP8HwdBufferQueueWaitPending(dec_cont->bq);
        if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
          /* Id for first frame. */
          p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
          if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
            if ( dec_cont->abort)
              return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
            else {
              output->data_left = 0;
              dec_cont->no_decoding_buffer = 1;
              return VP8DEC_NO_DECODING_BUFFER;
            }
#endif
          }
          p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
          p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
          p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
          if (p_asic_buff->pp_buffer == NULL)
            return VP8DEC_ABORTED;
          p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
          p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
          /* These need to point at something so use the output buffer */
          VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                     BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                     p_asic_buff->out_buffer_i);
          if(dec_cont->intra_only != HANTRO_TRUE) {
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
          }
        }
      }

      /* reallocate picture buffers */
      p_asic_buff->width = ( dec_cont->decoder.width + 15 ) & ~15;
      p_asic_buff->height = ( dec_cont->decoder.height + 15 ) & ~15;

      if (vp8hwdCheckSupport(dec_cont) != HANTRO_OK) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        dec_cont->dec_stat = VP8DEC_INITIALIZED;
        return VP8DEC_STREAM_NOT_SUPPORTED;
      }

      if (dec_cont->pp_enabled) {
        dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
        dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
      }

      dec_cont->prev_width = dec_cont->width;
      dec_cont->prev_height = dec_cont->height;
      dec_cont->width = dec_cont->decoder.width;
      dec_cont->height = dec_cont->decoder.height;

      dec_cont->dec_stat = VP8DEC_NEW_HEADERS;

      if( dec_cont->ref_buf_support && !dec_cont->intra_only ) {
        RefbuInit( &dec_cont->ref_buffer_ctrl, 10,
                   MB_MULTIPLE(dec_cont->decoder.width)>>4,
                   MB_MULTIPLE(dec_cont->decoder.height)>>4,
                   dec_cont->ref_buf_support);
      }


      DEC_API_TRC("VP8DecDecode# VP8DEC_HDRS_RDY\n");

      FifoPush(dec_cont->fifo_out, (FifoObject)FLUSH_MARKER, FIFO_EXCEPTION_DISABLE);
      if(dec_cont->abort)
        return(VP8DEC_ABORTED);
      else
        return VP8DEC_HDRS_RDY;
    }

    /* If we are here and dimensions are still 0, it means that we have
     * yet to decode a valid keyframe, in which case we must give up. */
    if( dec_cont->width == 0 || dec_cont->height == 0 ) {
      return VP8DEC_STRM_PROCESSED;
    }

    if (dec_cont->decoder.key_frame && dec_cont->dec_stat == VP8DEC_INITIALIZED) {
      dec_cont->dec_stat = VP8DEC_NEW_HEADERS;
      return VP8DEC_HDRS_RDY;
    }
    /* If output picture is broken and we are not decoding a base frame,
     * don't even start HW, just output same picture again. */
    if( !dec_cont->decoder.key_frame &&
        dec_cont->picture_broken &&
        (dec_cont->intra_freeze || dec_cont->force_intra_freeze) ) {
      vp8hwdFreeze(dec_cont);
      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
      DEC_API_TRC("VP8DecDecode# VP8DEC_PIC_DECODED\n");
      return VP8DEC_PIC_DECODED;
    }

  }
  /* missing picture, conceal */
  else if (!input->data_len) {
    if (!dec_cont->hw_ec_support || dec_cont->force_intra_freeze ||
        dec_cont->prev_is_key) {
      dec_cont->decoder.probs_decoded = 0;
      vp8hwdFreeze(dec_cont);
      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
      DEC_API_TRC("VP8DecDecode# VP8DEC_PIC_DECODED\n");
      return VP8DEC_PIC_DECODED;
    } else {
      dec_cont->conceal_start_mb_x = dec_cont->conceal_start_mb_y = 0;
      vp8hwdErrorConceal(dec_cont, input->stream_bus_address,
                         /*conceal_everything*/ 1);
      /* Assume that broken picture updated the last reference only and
       * also addref it since it will be outputted one more time. */
      VP8HwdBufferQueueAddRef(dec_cont->bq, p_asic_buff->out_buffer_i);
      VP8HwdBufferQueueUpdateRef(dec_cont->bq, BQUEUE_FLAG_PREV,
                                 p_asic_buff->out_buffer_i);
      p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
      p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
      p_asic_buff->out_buffer = NULL;
      dec_cont->pic_number++;
      dec_cont->out_count++;
      if (dec_cont->prob_refresh_detected) {
        dec_cont->picture_broken = 1;
        dec_cont->force_intra_freeze = 1;
      }

      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
        if (dec_cont->abort)
          return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->data_left = 0;
          dec_cont->no_decoding_buffer = 1;
          return VP8DEC_PIC_DECODED;
          //return VP8DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      if (dec_cont->pp_enabled && !dec_cont->intra_only && !p_asic_buff->strides_used) {
        p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
        if (p_asic_buff->pp_buffer == NULL)
          return VP8DEC_ABORTED;
        p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
        p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
      }
      p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
#if 0
      dec_cont->pic_number++;
      dec_cont->out_count++;
      ASSERT(p_asic_buff->out_buffer != NULL);
      if (dec_cont->prob_refresh_detected) {
        dec_cont->picture_broken = 1;
        dec_cont->force_intra_freeze = 1;
      }

      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
#endif
      return VP8DEC_PIC_DECODED;
    }
  }

  if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC) {
    dec_cont->ref_to_out = 0;

    ret = DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_VP8_DEC);

    if(ret != DWL_OK) {
      ERROR_PRINT("DWLReserveHw Failed");
      return VP8HWDEC_HW_RESERVED;
    }

    /* prepare asic */

    VP8HwdAsicProbUpdate(dec_cont);

    VP8HwdSegmentMapUpdate(dec_cont);

    VP8HwdAsicInitPicture(dec_cont);

    VP8HwdAsicStrmPosUpdate(dec_cont, input->stream_bus_address);

    /* Store the needed data for callback setup. */
    /* TODO(vmr): Consider parametrizing this. */
    dec_cont->stream = input->stream;
    dec_cont->p_user_data = input->p_user_data;
  } else
    VP8HwdAsicContPicture(dec_cont);

  if (dec_cont->partial_freeze) {
    PreparePartialFreeze((u8*)p_asic_buff->out_buffer->virtual_address,
                         (dec_cont->width >> 4),(dec_cont->height >> 4));
  }

  /* run the hardware */
  asic_status = VP8HwdAsicRun(dec_cont);

  /* Rollback entropy probabilities if refresh is not set */
  if(dec_cont->decoder.refresh_entropy_probs == HANTRO_FALSE) {
    DWLmemcpy( &dec_cont->decoder.entropy, &dec_cont->decoder.entropy_last,
               sizeof(vp8EntropyProbs_t));
    DWLmemcpy( dec_cont->decoder.vp7_scan_order, dec_cont->decoder.vp7_prev_scan_order,
               sizeof(dec_cont->decoder.vp7_scan_order));
  }
  /* If in asynchronous mode, just return OK  */
  if (asic_status == VP8HWDEC_ASYNC_MODE) {
    /* find first free buffer and use it as next output */
    if (!error_concealment || dec_cont->intra_only) {
      p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
      p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
      p_asic_buff->out_buffer = NULL;

      /* If we are never going to output the current buffer, we can release
      * the ref for output on the buffer. */
      if (!dec_cont->decoder.show_frame) {
        VP8HwdBufferQueueRemoveRef(dec_cont->bq, p_asic_buff->out_buffer_i);
        if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
          InputQueueReturnBuffer(dec_cont->pp_buffer_queue, p_asic_buff->pp_pictures[p_asic_buff->out_pp_buffer_i]->virtual_address);
        }
      }
      /* If WebP, we will be recycling only one buffer and no update is
       * necessary. */
      if (!dec_cont->intra_only) {
        p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
        if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
          if ( dec_cont->abort)
            return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          else {
            output->data_left = 0;
            dec_cont->no_decoding_buffer = 1;
            return VP8DEC_OK;
          }
#endif
        }
        p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      }
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      if (dec_cont->pp_enabled && !dec_cont->intra_only && !p_asic_buff->strides_used) {
        p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
        if (p_asic_buff->pp_buffer == NULL)
          return VP8DEC_ABORTED;
        p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
        p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
      }
      p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
      ASSERT(p_asic_buff->out_buffer != NULL);
    }
    return VP8DEC_OK;
  }

  /* Handle system error situations */
  if(asic_status == VP8HWDEC_SYSTEM_TIMEOUT) {
    /* This timeout is DWL(software/os) generated */
    DEC_API_TRC("VP8DecDecode# VP8DEC_HW_TIMEOUT, SW generated\n");
    return VP8DEC_HW_TIMEOUT;
  } else if(asic_status == VP8HWDEC_SYSTEM_ERROR) {
    DEC_API_TRC("VP8DecDecode# VP8HWDEC_SYSTEM_ERROR\n");
    return VP8DEC_SYSTEM_ERROR;
  } else if(asic_status == VP8HWDEC_HW_RESERVED) {
    DEC_API_TRC("VP8DecDecode# VP8HWDEC_HW_RESERVED\n");
    return VP8DEC_HW_RESERVED;
  }

  /* Handle possible common HW error situations */
  if(asic_status & DEC_8190_IRQ_BUS) {
    DEC_API_TRC("VP8DecDecode# VP8DEC_HW_BUS_ERROR\n");
    return VP8DEC_HW_BUS_ERROR;
  }

  /* for all the rest we will output a picture (concealed or not) */
  if((asic_status & DEC_8190_IRQ_TIMEOUT) ||
      (asic_status & DEC_8190_IRQ_ERROR) ||
      (asic_status & DEC_8190_IRQ_ASO) || /* to signal lost residual */
      (asic_status & DEC_8190_IRQ_ABORT)) {
    u32 conceal_everything = 0;

    if (!dec_cont->partial_freeze ||
        !ProcessPartialFreeze((u8*)p_asic_buff->out_buffer->virtual_address,
                              (u8*)GetPrevRef(dec_cont)->virtual_address,
                              (dec_cont->width >> 4),
                              (dec_cont->height >> 4),
                              dec_cont->partial_freeze == 1)) {
      /* This timeout is HW generated */
      if(asic_status & DEC_8190_IRQ_TIMEOUT) {
#ifdef VP8HWTIMEOUT_ASSERT
        ASSERT(0);
#endif
        DEBUG_PRINT(("IRQ: HW TIMEOUT\n"));
      } else {
        DEBUG_PRINT(("IRQ: STREAM ERROR\n"));
      }

      /* keyframe -> all mbs concealed */
      if (dec_cont->decoder.key_frame ||
          (asic_status & DEC_8190_IRQ_TIMEOUT) ||
          (asic_status & DEC_8190_IRQ_ABORT)) {
        dec_cont->conceal_start_mb_x = 0;
        dec_cont->conceal_start_mb_y = 0;
        conceal_everything = 1;
      } else {
        /* concealment start point read from sw/hw registers */
        dec_cont->conceal_start_mb_x =
          GetDecRegister(dec_cont->vp8_regs, HWIF_STARTMB_X);
        dec_cont->conceal_start_mb_y =
          GetDecRegister(dec_cont->vp8_regs, HWIF_STARTMB_Y);
        /* error in control partition -> conceal all mbs from start point
         * onwards, otherwise only intra mbs get concealed */
        conceal_everything = !(asic_status & DEC_8190_IRQ_ASO);
      }

      if (dec_cont->slice_height && dec_cont->intra_only) {
        dec_cont->slice_concealment = 1;
      }

      /* HW error concealment not used if
       * 1) previous frame was key frame (no ref mvs available) AND
       * 2) whole control partition corrupted (no current mvs available) */
      if (dec_cont->hw_ec_support &&
          (!dec_cont->prev_is_key || !conceal_everything ||
           dec_cont->conceal_start_mb_y || dec_cont->conceal_start_mb_x))
        vp8hwdErrorConceal(dec_cont, input->stream_bus_address,
                           conceal_everything);
      else /* normal picture freeze */
        error_concealment = 1;
    } else {
      asic_status &= ~DEC_8190_IRQ_ERROR;
      asic_status &= ~DEC_8190_IRQ_TIMEOUT;
      asic_status &= ~DEC_8190_IRQ_ASO;
      asic_status |= DEC_8190_IRQ_RDY;
      error_concealment = 0;
    }
  } else if(asic_status & DEC_8190_IRQ_RDY) {
  } else if (asic_status & DEC_8190_IRQ_SLICE) {
  } else {
    ASSERT(0);
  }

  if(asic_status & DEC_8190_IRQ_RDY) {
    DEBUG_PRINT(("IRQ: PICTURE RDY\n"));

    if (dec_cont->decoder.key_frame) {
      dec_cont->picture_broken = 0;
      dec_cont->force_intra_freeze = 0;
    }

    if (dec_cont->slice_height) {
      dec_cont->output_rows = p_asic_buff->height -  dec_cont->tot_decoded_rows*16;
      /* Below code not needed; slice mode always disables loop-filter -->
       * output 16 rows multiple */
      /*
      if (dec_cont->tot_decoded_rows)
          dec_cont->output_rows += 8;
          */
      dec_cont->last_slice = 1;
      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
      return VP8DEC_PIC_DECODED;
    }

  } else if (asic_status & DEC_8190_IRQ_SLICE) {
    dec_cont->dec_stat = VP8DEC_MIDDLE_OF_PIC;

    dec_cont->output_rows = dec_cont->slice_height * 16;
    /* Below code not needed; slice mode always disables loop-filter -->
     * output 16 rows multiple */
    /*if (!dec_cont->tot_decoded_rows)
        dec_cont->output_rows -= 8;*/

    dec_cont->tot_decoded_rows += dec_cont->slice_height;

    if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
      return VP8DEC_ABORTED;
    return VP8DEC_SLICE_RDY;
  }

  if(dec_cont->intra_only != HANTRO_TRUE)
    VP8HwdUpdateRefs(dec_cont, error_concealment);

  if (dec_cont->decoder.show_frame)
    dec_cont->out_count++;
  /* find first free buffer and use it as next output */
  if (!error_concealment || dec_cont->intra_only) {
    p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
    p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
    p_asic_buff->out_buffer = NULL;
    if(dec_cont->decoder.show_frame) {
      if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
        return VP8DEC_ABORTED;
    }

    /* If WebP, we will be recycling only one buffer and no update is
     * necessary. */
    if (!dec_cont->intra_only) {
      if(dec_cont->decoder.show_frame) {
        if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
          return VP8DEC_ABORTED;
      }
      VP8HwdBufferQueueRemoveRef(dec_cont->bq, p_asic_buff->out_buffer_i);
      if (!dec_cont->decoder.show_frame && dec_cont->pp_enabled && !p_asic_buff->strides_used) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, p_asic_buff->pp_pictures[p_asic_buff->out_pp_buffer_i]->virtual_address);
      }
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
        if (dec_cont->abort)
          return VP8DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->data_left = 0;
          dec_cont->no_decoding_buffer = 1;
          return VP8DEC_PIC_DECODED;
          //return VP8DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
    }

    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
    if (dec_cont->pp_enabled && !dec_cont->intra_only && !p_asic_buff->strides_used) {
      p_asic_buff->pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
      if (p_asic_buff->pp_buffer == NULL)
        return VP8DEC_ABORTED;
      p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, p_asic_buff->pp_buffer->virtual_address);
      p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
    }
    p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
    ASSERT(p_asic_buff->out_buffer != NULL);
  } else {
    dec_cont->ref_to_out = 1;
    dec_cont->picture_broken = 1;
    if (!dec_cont->pic_number) {
      (void) DWLmemset( GetPrevRef(dec_cont)->virtual_address, 128,
                        p_asic_buff->width * p_asic_buff->height * 3 / 2);
    }
  }
  dec_cont->pic_number++;
  if (VP8PushOutput(dec_cont) == VP8DEC_ABORTED)
    return VP8DEC_ABORTED;

  DEC_API_TRC("VP8DecDecode# VP8DEC_PIC_DECODED\n");
  return VP8DEC_PIC_DECODED;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecNextPicture
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecPicture * output
    Argument        : u32 end_of_stream
------------------------------------------------------------------------------*/
VP8DecRet VP8DecNextPicture(VP8DecInst dec_inst,
                            VP8DecPicture * output, u32 end_of_stream) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  i32 ret;

  DEC_API_TRC("VP8DecNextPicture#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("VP8DecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (VP8DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecNextPicture# ERROR: Decoder not initialized\n");
    return (VP8DEC_NOT_INITIALIZED);
  }
  /*  NextOutput will block until there is an output. */
  addr_t i;
  if ((ret = FifoPop(dec_cont->fifo_out, (FifoObject)&i,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
        FIFO_EXCEPTION_ENABLE
#else
        FIFO_EXCEPTION_DISABLE
#endif
        )) != FIFO_ABORT) {

#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
    if (ret == FIFO_EMPTY) return VP8DEC_OK;
#endif

    if ((i32)i == EOS_MARKER) {
      DEC_API_TRC("VP8DecNextPicture# VP8DEC_END_OF_STREAM\n");
      return VP8DEC_END_OF_STREAM;
    }
    if ((i32)i == FLUSH_MARKER) {
      DEC_API_TRC("VP8DecNextPicture# VP8DEC_FLUSHED\n");
      return VP8DEC_FLUSHED;
    }

    *output = dec_cont->asic_buff->picture_info[i];

    DEC_API_TRC("VP8DecNextPicture# VP8DEC_PIC_RDY\n");
    return VP8DEC_PIC_RDY;
  } else
    return VP8DEC_ABORTED;

  DEC_API_TRC("VP8DecNextPicture# VP8DEC_OK\n");
  return (VP8DEC_OK);
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecNextPicture_INTERNAL
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecPicture * output
    Argument        : u32 end_of_stream
------------------------------------------------------------------------------*/
VP8DecRet VP8DecNextPicture_INTERNAL(VP8DecInst dec_inst,
                                     VP8DecPicture * output, u32 end_of_stream) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 pic_for_output = 0;
  u32 ref_index, index;
  PpUnitIntConfig *ppu_cfg;
  u32 i;

  DEC_API_TRC("VP8DecNextPicture_INTERNAL#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("VP8DecNextPicture_INTERNAL# ERROR: dec_inst or output is NULL\n");
    return (VP8DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return (VP8DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->out_count && !dec_cont->output_rows)
    return VP8DEC_OK;

  if(dec_cont->slice_concealment)
    return (VP8DEC_OK);

  (void) DWLmemset(output, 0, sizeof(VP8DecPicture));
  /* slice for output */
  if (dec_cont->output_rows) {
    output->num_slice_rows = dec_cont->output_rows;
    output->last_slice = dec_cont->last_slice;

    if (dec_cont->user_mem) {
      output->pictures[0].p_output_frame = p_asic_buff->user_mem.p_pic_buffer_y[0];
      output->pictures[0].p_output_frame_c = p_asic_buff->user_mem.p_pic_buffer_c[0];
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->user_mem.pic_buffer_bus_addr_y[0];
      output->pictures[0].output_frame_bus_address_c =
        p_asic_buff->user_mem.pic_buffer_bus_addr_c[0];
    } else {
      if (!dec_cont->pp_enabled) {
        u32 stride, offset;
        if (!dec_cont->tiled_stride_enable) {
          offset = 16 * (dec_cont->slice_height + 1) * p_asic_buff->width;
        } else {
          stride = NEXT_MULTIPLE(p_asic_buff->width * 4, ALIGN(dec_cont->align));
          offset = 16 * (dec_cont->slice_height + 1) * stride / 4;
        }
        output->pictures[0].p_output_frame = p_asic_buff->pictures[0].virtual_address;
        output->pictures[0].output_frame_bus_address =
          p_asic_buff->pictures[0].bus_address;

        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame_c = p_asic_buff->pictures_c[0].virtual_address;
          output->pictures[0].output_frame_bus_address_c =
            p_asic_buff->pictures_c[0].bus_address;
        } else {
          output->pictures[0].p_output_frame_c =
            (u32*)((addr_t)output->pictures[0].p_output_frame + offset);
          output->pictures[0].output_frame_bus_address_c =
            output->pictures[0].output_frame_bus_address + offset;
        }
      } else {
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame = p_asic_buff->pictures[0].virtual_address;
          output->pictures[0].output_frame_bus_address =
            p_asic_buff->pictures[0].bus_address;
          output->pictures[0].p_output_frame_c = p_asic_buff->pictures_c[0].virtual_address;
          output->pictures[0].output_frame_bus_address_c =
            p_asic_buff->pictures_c[0].bus_address;
        } else {
          ppu_cfg = dec_cont->ppu_cfg;
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled) continue;
            output->pictures[i].p_output_frame = (u32*)((addr_t)p_asic_buff->pp_pictures[0]->virtual_address + ppu_cfg->luma_offset);
            output->pictures[i].output_frame_bus_address =
              p_asic_buff->pp_pictures[0]->bus_address + ppu_cfg->luma_offset;
            output->pictures[i].p_output_frame_c =
              (u32*)((addr_t)p_asic_buff->pp_pictures[0]->virtual_address + ppu_cfg->chroma_offset);
            output->pictures[i].output_frame_bus_address_c =
              p_asic_buff->pp_pictures[0]->bus_address + ppu_cfg->chroma_offset;
          }
        }
      }
    }

    output->pic_id = 0;
    output->decode_id = p_asic_buff->picture_info[0].decode_id;
    output->is_intra_frame = dec_cont->decoder.key_frame;
    output->is_golden_frame = 0;
    output->nbr_of_err_mbs = 0;
    if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
      output->pictures[0].frame_width = (dec_cont->width + 15) & ~15;
      output->pictures[0].frame_height = (dec_cont->height + 15) & ~15;
      output->pictures[0].coded_width = dec_cont->width;
      output->pictures[0].coded_height = dec_cont->height;
      output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                            p_asic_buff->luma_stride : p_asic_buff->width;
      output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                              p_asic_buff->chroma_stride : p_asic_buff->width;
      if (dec_cont->tiled_stride_enable)
        output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].frame_width * 4,
                                                       ALIGN(dec_cont->align));
      else
        output->pictures[0].pic_stride = output->pictures[0].frame_width * 4;
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
      output->pictures[0].output_format = dec_cont->tiled_reference_enable ?
                            DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;
    } else {
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
        output->pictures[i].frame_width = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
        output->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].luma_stride = output->pictures[i].frame_width;
        output->pictures[i].chroma_stride = output->pictures[i].frame_width;
        output->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
        output->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
        CheckOutputFormat(dec_cont->ppu_cfg, &output->pictures[i].output_format, i);
      }
    }

    dec_cont->output_rows = 0;

    while(dec_cont->asic_buff->not_displayed[0])
      sched_yield();

    VP8HwdBufferQueueSetBufferAsUsed(dec_cont->bq, 0);
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
      InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,dec_cont->asic_buff->pp_pictures[0]->virtual_address);
    }
    dec_cont->asic_buff->not_displayed[0] = 1;
    dec_cont->asic_buff->picture_info[0] = *output;
    if (dec_cont->pp_enabled)
      VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, 0);
    FifoPush(dec_cont->fifo_out, 0, FIFO_EXCEPTION_DISABLE);
    return (VP8DEC_PIC_RDY);
  }

  output->num_slice_rows = 0;

  pic_for_output = 1;

  if (pic_for_output) {
    const struct DWLLinearMem *out_pic = NULL;
    const struct DWLLinearMem *out_pic_c = NULL;

    /* no pp -> current output (ref if concealed) */
    if (!dec_cont->ref_to_out) {
      out_pic = p_asic_buff->prev_out_buffer;
      out_pic_c = &p_asic_buff->pictures_c[ p_asic_buff->prev_out_buffer_i ];
    } else {
      out_pic = GetPrevRef(dec_cont);
      out_pic_c = &p_asic_buff->pictures_c[
                    VP8HwdBufferQueueGetPrevRef(dec_cont->bq)];
    }

    dec_cont->out_count--;

    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;

    if (dec_cont->user_mem) {
      output->pictures[0].p_output_frame = p_asic_buff->user_mem.p_pic_buffer_y[0];
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->user_mem.pic_buffer_bus_addr_y[0];
      output->pictures[0].p_output_frame_c = p_asic_buff->user_mem.p_pic_buffer_c[0];
      output->pictures[0].output_frame_bus_address_c =
        p_asic_buff->user_mem.pic_buffer_bus_addr_c[0];

    } else {
      if (!dec_cont->pp_enabled) {
        output->pictures[0].p_output_frame = out_pic->virtual_address;
        output->pictures[0].output_frame_bus_address = out_pic->bus_address;
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
          output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
        } else {
          u32 stride, chroma_buf_offset;
          if (!dec_cont->tiled_stride_enable) {
            stride = p_asic_buff->chroma_stride ?
                     p_asic_buff->chroma_stride : p_asic_buff->width;
            chroma_buf_offset = stride * p_asic_buff->height;
          } else {
            stride = p_asic_buff->chroma_stride ?
                     p_asic_buff->chroma_stride : p_asic_buff->width;
            stride = NEXT_MULTIPLE(stride * 4, ALIGN(dec_cont->align));
            chroma_buf_offset = stride * p_asic_buff->height / 4;
          }
          output->pictures[0].p_output_frame_c = output->pictures[0].p_output_frame +
                                     chroma_buf_offset / 4;
          output->pictures[0].output_frame_bus_address_c = output->pictures[0].output_frame_bus_address +
                                               chroma_buf_offset;
        }
      } else {
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame = out_pic->virtual_address;
          output->pictures[0].output_frame_bus_address = out_pic->bus_address;
          output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
          output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
        } else {
          if (!dec_cont->ref_to_out)
            index = p_asic_buff->prev_out_buffer_i;
          else
            index = VP8HwdBufferQueueGetPrevRef(dec_cont->bq);
	  index = p_asic_buff->pp_buffer_map[index];
          ppu_cfg = dec_cont->ppu_cfg;
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled) continue;
            output->pictures[i].p_output_frame = (u32*)((addr_t)p_asic_buff->pp_pictures[index]->virtual_address + ppu_cfg->luma_offset);
            output->pictures[i].output_frame_bus_address =
              p_asic_buff->pp_pictures[index]->bus_address + ppu_cfg->luma_offset;
            output->pictures[i].p_output_frame_c =
              (u32*)((addr_t)p_asic_buff->pp_pictures[index]->virtual_address + ppu_cfg->chroma_offset);
            output->pictures[i].output_frame_bus_address_c =
              p_asic_buff->pp_pictures[index]->bus_address + ppu_cfg->chroma_offset;
          }
        }
      }
    }
    output->pic_id = 0;
    output->is_intra_frame = dec_cont->decoder.key_frame;
    output->is_golden_frame = 0;
    output->nbr_of_err_mbs = 0;
#if 0
    output->frame_width = (dec_cont->width + 15) & ~15;
    output->frame_height = (dec_cont->height + 15) & ~15;
    output->coded_width = dec_cont->width;
    output->coded_height = dec_cont->height;
#endif
    ref_index = index = FindIndex(dec_cont, out_pic->virtual_address);
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used)
      index = p_asic_buff->pp_buffer_map[index];
    if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
      output->pictures[0].frame_width = dec_cont->asic_buff->frame_width[ref_index];
      output->pictures[0].frame_height = dec_cont->asic_buff->frame_height[ref_index];
      output->pictures[0].coded_width = dec_cont->asic_buff->coded_width[ref_index];
      output->pictures[0].coded_height = dec_cont->asic_buff->coded_height[ref_index];
      if (dec_cont->tiled_stride_enable)
        output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].frame_width * 4,
                                                       ALIGN(dec_cont->align));
      else
        output->pictures[0].pic_stride = output->pictures[0].frame_width * 4;
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
      output->pictures[0].output_format = dec_cont->tiled_reference_enable ?
                            DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;
    } else {
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
        output->pictures[i].frame_width = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
        output->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].luma_stride = output->pictures[i].frame_width;
        output->pictures[i].chroma_stride = output->pictures[i].frame_width;
        output->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
        output->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
        CheckOutputFormat(dec_cont->ppu_cfg, &output->pictures[i].output_format, i);
      }
    }

    DEC_API_TRC("VP8DecNextPicture_INTERNAL# VP8DEC_PIC_RDY\n");
    output->decode_id = p_asic_buff->decode_id[ref_index];
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[ref_index])
#endif
    {
    if (VP8HwdBufferQueueWaitBufNotInUse(dec_cont->bq, ref_index) == HANTRO_NOK)
      return VP8DEC_ABORTED;
    VP8HwdBufferQueueSetBufferAsUsed(dec_cont->bq, ref_index);
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
      InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,dec_cont->asic_buff->pp_pictures[index]->virtual_address);
      InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,dec_cont->asic_buff->pp_pictures[index]->virtual_address);
    }
    dec_cont->asic_buff->not_displayed[index] = 1;
    dec_cont->asic_buff->picture_info[index] = *output;
    if (dec_cont->pp_enabled)
      VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, ref_index);
    FifoPush(dec_cont->fifo_out, (FifoObject)(addr_t)index, FIFO_EXCEPTION_DISABLE);
      dec_cont->asic_buff->first_show[ref_index] = 0;
    }
    return (VP8DEC_PIC_RDY);
  }

  DEC_API_TRC("VP8DecNextPicture_INTERNAL# VP8DEC_OK\n");
  return (VP8DEC_OK);

}


/*------------------------------------------------------------------------------
    Function name   : VP8DecPictureConsumed
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
VP8DecRet VP8DecPictureConsumed(VP8DecInst dec_inst,
                                const VP8DecPicture * picture) {
  u32 buffer_id;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg;
  u32 i;
  if (dec_inst == NULL || picture == NULL) {
    return VP8DEC_PARAM_ERROR;
  }

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  if (dec_cont->pp_enabled && !dec_cont->asic_buff->strides_used) {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = picture->pictures[i].p_output_frame;
        break;
      }
    }
    buffer_id = FindPpIndex(dec_cont, output_picture);
    if (buffer_id >= dec_cont->num_pp_buffers)
      return VP8DEC_PARAM_ERROR;
  } else {
    buffer_id = FindIndex(dec_cont, picture->pictures[0].p_output_frame);
    if (buffer_id >= dec_cont->num_buffers)
      return VP8DEC_PARAM_ERROR;

  }

  /* Remove the reference to the buffer. */
  if(dec_cont->asic_buff->not_displayed[buffer_id]) {
    dec_cont->asic_buff->not_displayed[buffer_id] = 0;
    if(picture->num_slice_rows == 0 || picture->last_slice) {
      if (dec_cont->pp_enabled && !dec_cont->asic_buff->strides_used) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
      }
      else
        VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, buffer_id);
    }
  }
  DEC_API_TRC("VP8DecPictureConsumed# VP8DEC_OK\n");
  return VP8DEC_OK;
}


/*------------------------------------------------------------------------------
    Function name   : VP8DecEndOfStream
    Description     : Used for signalling stream end from the input thread
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
VP8DecRet VP8DecEndOfStream(VP8DecInst dec_inst, u32 strm_end_flag) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  VP8DecPicture output;
  VP8DecRet ret;

  if (dec_inst == NULL) {
    return VP8DEC_PARAM_ERROR;
  }

  /* Don't do end of stream twice. This is not thread-safe, so it must be
   * called from the single input thread that is also used to call
   * VP8DecDecode. */
  pthread_mutex_lock(&dec_cont->protect_mutex);
  if (dec_cont->dec_stat == VP8DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return VP8DEC_END_OF_STREAM;
  }

  while((ret = VP8DecNextPicture_INTERNAL(dec_inst, &output, 1)) == VP8DEC_PIC_RDY);
  if(ret == VP8DEC_ABORTED) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (VP8DEC_ABORTED);
  }

  if(strm_end_flag) {
    dec_cont->dec_stat = VP8DEC_END_OF_STREAM;
    FifoPush(dec_cont->fifo_out, (FifoObject)EOS_MARKER, FIFO_EXCEPTION_DISABLE);
  }
  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return VP8DEC_OK;
}


u32 vp8hwdCheckSupport( VP8DecContainer_t *dec_cont ) {

  DWLHwConfig hw_config;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  DWLReadAsicConfig(&hw_config, DWL_CLIENT_TYPE_VP8_DEC);

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  if ( (dec_cont->asic_buff->width > hw_feature.vp8_max_dec_pic_width) ||
       (dec_cont->asic_buff->width < dec_cont->min_dec_pic_width) ||
       (dec_cont->asic_buff->height > hw_feature.vp8_max_dec_pic_height) ||
       (dec_cont->asic_buff->height < dec_cont->min_dec_pic_height) ||
       (dec_cont->asic_buff->width*dec_cont->asic_buff->height > MAX_PIC_SIZE) ) {
    /* check if webp support */
    if (dec_cont->intra_only && hw_feature.webp_support &&
        dec_cont->asic_buff->width <= hw_feature.img_max_dec_width &&
        dec_cont->asic_buff->height <= hw_feature.img_max_dec_height) {
      return HANTRO_OK;
    } else {
      return HANTRO_NOK;
    }
  }

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecPeek
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
VP8DecRet VP8DecPeek(VP8DecInst dec_inst, VP8DecPicture * output) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buff_id;
  const struct DWLLinearMem *out_pic = NULL;
  const struct DWLLinearMem *out_pic_c = NULL;

  DEC_API_TRC("VP8DecPeek#\n");

  if(dec_inst == NULL || output == NULL) {
    DEC_API_TRC("VP8DecPeek# ERROR: dec_inst or output is NULL\n");
    return (VP8DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    DEC_API_TRC("VP8DecPeek# ERROR: Decoder not initialized\n");
    return (VP8DEC_NOT_INITIALIZED);
  }

  /* Don't allow peek for webp */
  if(dec_cont->intra_only) {
    DWLmemset(output, 0, sizeof(VP8DecPicture));
    return VP8DEC_OK;
  }

  /* when output release thread enabled, VP8DecNextPicture_INTERNAL() called in
     VP8DecDecode(), and so dec_cont->fullness used to sample the real out_count
     in case of VP8DecNextPicture_INTERNAL() called before than VP8DecPeek() */
  u32 tmp = dec_cont->fullness;

  if (!tmp) {
    DWLmemset(output, 0, sizeof(VP8DecPicture));
    return VP8DEC_OK;
  }

  out_pic = p_asic_buff->prev_out_buffer;
  out_pic_c = &p_asic_buff->pictures_c[ p_asic_buff->prev_out_buffer_i ];

  if (!dec_cont->pp_enabled) {
    output->pictures[0].p_output_frame = out_pic->virtual_address;
    output->pictures[0].output_frame_bus_address = out_pic->bus_address;
    if(p_asic_buff->strides_used) {
      output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
      output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
    } else {
      u32 chroma_buf_offset = p_asic_buff->width * p_asic_buff->height;
      output->pictures[0].p_output_frame_c = output->pictures[0].p_output_frame +
                                 chroma_buf_offset / 4;
      output->pictures[0].output_frame_bus_address_c = output->pictures[0].output_frame_bus_address +
                                           chroma_buf_offset;
    }
  } else {
    if(p_asic_buff->strides_used) {
      output->pictures[0].p_output_frame = out_pic->virtual_address;
      output->pictures[0].output_frame_bus_address = out_pic->bus_address;
      output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
      output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
    } else {
      u32 offset = (16 * (dec_cont->slice_height + 1) >> dec_cont->dscale_shift_y) * p_asic_buff->width >> dec_cont->dscale_shift_x;
      output->pictures[0].p_output_frame = p_asic_buff->pp_pictures[p_asic_buff->pp_buffer_map[p_asic_buff->prev_out_buffer_i]]->virtual_address;
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->pp_pictures[p_asic_buff->pp_buffer_map[p_asic_buff->prev_out_buffer_i]]->bus_address;
      output->pictures[0].p_output_frame_c =
        (u32*)((addr_t)output->pictures[0].p_output_frame + offset);
      output->pictures[0].output_frame_bus_address_c =
        output->pictures[0].output_frame_bus_address + offset;
    }
  }

  output->pic_id = 0;
  buff_id = FindIndex(dec_cont, output->pictures[0].p_output_frame);
  output->decode_id = p_asic_buff->decode_id[buff_id];
  output->is_intra_frame = dec_cont->decoder.key_frame;
  output->is_golden_frame = 0;
  output->nbr_of_err_mbs = 0;

  if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
    output->pictures[0].frame_width = (dec_cont->width + 15) & ~15;
    output->pictures[0].frame_height = (dec_cont->height + 15) & ~15;
    output->pictures[0].coded_width = dec_cont->width;
    output->pictures[0].coded_height = dec_cont->height;
    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;
  } else {
    output->pictures[0].frame_width = ((dec_cont->width + 15) & ~15) >> dec_cont->dscale_shift_x;
    output->pictures[0].frame_height = ((dec_cont->height + 15) & ~15) >> dec_cont->dscale_shift_y;
    output->pictures[0].coded_width = dec_cont->width >> dec_cont->dscale_shift_x;
    output->pictures[0].coded_height = dec_cont->height >> dec_cont->dscale_shift_y;
    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;
    output->pictures[0].luma_stride = output->pictures[0].luma_stride >> dec_cont->dscale_shift_x;
    output->pictures[0].chroma_stride = output->pictures[0].chroma_stride >> dec_cont->dscale_shift_x;
  }
  return (VP8DEC_PIC_RDY);


}

void vp8hwdMCFreeze(VP8DecContainer_t *dec_cont) {
  /*TODO (mheikkinen) Error handling/concealment is still under construction.*/
  /* TODO Output reference handling */
  VP8HwdBufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
  dec_cont->stream_consumed_callback((u8*)dec_cont->stream, dec_cont->p_user_data);
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdFreeze
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdFreeze(VP8DecContainer_t *dec_cont) {
  /* for multicore */
  if(dec_cont->stream_consumed_callback) {
    vp8hwdMCFreeze(dec_cont);
    return;
  }
  /* Skip */
  dec_cont->pic_number++;
  dec_cont->ref_to_out = 1;
  if (dec_cont->decoder.show_frame)
    dec_cont->out_count++;

  /* Rollback entropy probabilities if refresh is not set */
  if (dec_cont->decoder.probs_decoded &&
      dec_cont->decoder.refresh_entropy_probs == HANTRO_FALSE) {
    DWLmemcpy( &dec_cont->decoder.entropy, &dec_cont->decoder.entropy_last,
               sizeof(vp8EntropyProbs_t));
    DWLmemcpy( dec_cont->decoder.vp7_scan_order, dec_cont->decoder.vp7_prev_scan_order,
               sizeof(dec_cont->decoder.vp7_scan_order));
  }
  /* lost accumulated coeff prob updates -> force video freeze until next
   * keyframe received */
  else if (dec_cont->hw_ec_support && dec_cont->prob_refresh_detected) {
    dec_cont->force_intra_freeze = 1;
  }

  dec_cont->picture_broken = 1;

  /* reset mv memory to not to use too old mvs in extrapolation */
  if (dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address)
    DWLmemset(
      dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address,
      0, dec_cont->width * dec_cont->height / 256 * 16 * sizeof(u32));


}

/*------------------------------------------------------------------------------
    Function name   : CheckBitstreamWorkaround
    Description     : Check if we need a workaround for a rare bug.
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
u32 CheckBitstreamWorkaround(vp8_decoder_t* dec) {
  /* TODO: HW ID check, P pic stuff */
  if( dec->segmentation_map_update &&
      dec->coeff_skip_mode == 0 &&
      dec->key_frame) {
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : DoBitstreamWorkaround
    Description     : Perform workaround for bug.
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
#if 0
void DoBitstreamWorkaround(vp8_decoder_t* dec, DecAsicBuffers_t *p_asic_buff, vpBoolCoder_t*bc) {
  /* TODO in entirety */
}
#endif

/* TODO(mheikkinen) Work in progress */
void ConcealRefAvailability(u32 * output, u32 height, u32 width, u32 tiled_stride_enable, u32 align) {
  u32 offset = 0;
  if (!tiled_stride_enable) {
    offset = (height * width * 3) / 2;
  } else {
    u32 out_w_luma, out_w_chroma, out_h;
    out_w_luma = NEXT_MULTIPLE(4 * width, ALIGN(align));
    out_w_chroma = NEXT_MULTIPLE(4 * width, ALIGN(align));
    out_h = height / 4;
    offset = out_w_luma * out_h + out_w_chroma * out_h / 2;
  }
  u8 * p_ref_status = (u8 *)(output + offset / 4);

  p_ref_status[1] = height & 0xFFU;
  p_ref_status[0] = (height >> 8) & 0xFFU;
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdErrorConceal
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdErrorConceal(VP8DecContainer_t *dec_cont, addr_t bus_address,
                        u32 conceal_everything) {
  /* force keyframes processed like normal frames (mvs extrapolated for
   * all mbs) */
  if (dec_cont->decoder.key_frame) {
    dec_cont->decoder.key_frame = 0;
  }

  vp8hwdEc(&dec_cont->ec,
           dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address,
           dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_curr].virtual_address,
           dec_cont->conceal_start_mb_y * dec_cont->width/16 + dec_cont->conceal_start_mb_x,
           conceal_everything);

  dec_cont->conceal = 1;
  if (conceal_everything)
    dec_cont->conceal_start_mb_x = dec_cont->conceal_start_mb_y = 0;
  VP8HwdAsicInitPicture(dec_cont);
  VP8HwdAsicStrmPosUpdate(dec_cont, bus_address);

  ConcealRefAvailability(dec_cont->asic_buff->out_buffer->virtual_address,
                         MB_MULTIPLE(dec_cont->asic_buff->height),  MB_MULTIPLE(dec_cont->asic_buff->width),
                         dec_cont->tiled_stride_enable, dec_cont->align);

  dec_cont->conceal = 0;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecSetPictureBuffers
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
VP8DecRet VP8DecSetPictureBuffers( VP8DecInst dec_inst,
                                   VP8DecPictureBufferProperties * p_pbp ) {

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  u32 i;
  u32 ok;
#if DEC_X170_REFBU_WIDTH == 64
  const u32 max_stride = 1<<18;
#else
  const u32 max_stride = 1<<17;
#endif /* DEC_X170_REFBU_WIDTH */
  u32 luma_stride_pow2 = 0;
  u32 chroma_stride_pow2 = 0;

  if(!dec_inst || !p_pbp) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: NULL parameter");
    return VP8DEC_PARAM_ERROR;
  }

  /* Allow this only at stream start! */
  if ( ((dec_cont->dec_stat != VP8DEC_NEW_HEADERS) &&
        (dec_cont->dec_stat != VP8DEC_INITIALIZED)) ||
       (dec_cont->pic_number > 0)) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: Setup allowed at stream"\
                " start only!");
    return VP8DEC_PARAM_ERROR;
  }

  if( !dec_cont->stride_support ) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: Not supported");
    return VP8DEC_FORMAT_NOT_SUPPORTED;
  }

  /* Tiled mode and custom strides not supported yet */
  if( dec_cont->tiled_mode_support &&
      (p_pbp->luma_stride || p_pbp->chroma_stride)) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: tiled mode and picture "\
                "buffer properties conflict");
    return VP8DEC_PARAM_ERROR;
  }

  /* Strides must be 2^N for some N>=4 */
  if(p_pbp->luma_stride || p_pbp->chroma_stride) {
    ok = 0;
    for ( i = 10 ; i < 32 ; ++i ) {
      if(p_pbp->luma_stride == (u32)(1<<i)) {
        luma_stride_pow2 = i;
        ok = 1;
        break;
      }
    }
    if(!ok) {
      DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: luma stride must be a "\
                  "power of 2");
      return VP8DEC_PARAM_ERROR;
    }

    ok = 0;
    for ( i = 10 ; i < 32 ; ++i ) {
      if(p_pbp->chroma_stride == (u32)(1<<i)) {
        chroma_stride_pow2 = i;
        ok = 1;
        break;
      }
    }
    if(!ok) {
      DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: luma stride must be a "\
                  "power of 2");
      return VP8DEC_PARAM_ERROR;
    }
  }

  /* Max luma stride check */
  if(p_pbp->luma_stride > max_stride) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: luma stride exceeds "\
                "maximum");
    return VP8DEC_PARAM_ERROR;
  }

  /* Max chroma stride check */
  if(p_pbp->chroma_stride > max_stride) {
    DEC_API_TRC("VP8DecSetPictureBuffers# ERROR: chroma stride exceeds "\
                "maximum");
    return VP8DEC_PARAM_ERROR;
  }

  dec_cont->asic_buff->luma_stride = p_pbp->luma_stride;
  dec_cont->asic_buff->chroma_stride = p_pbp->chroma_stride;
  dec_cont->asic_buff->luma_stride_pow2 = luma_stride_pow2;
  dec_cont->asic_buff->chroma_stride_pow2 = chroma_stride_pow2;
  dec_cont->asic_buff->strides_used = 0;
  if( dec_cont->asic_buff->luma_stride ||
      dec_cont->asic_buff->chroma_stride ) {
    dec_cont->asic_buff->strides_used = 1;
  }
  VP8SetExternalBufferInfo(dec_cont);

  DEC_API_TRC("VP8DecSetPictureBuffers# OK\n");
  return (VP8DEC_OK);

}

static struct DWLLinearMem* GetPrevRef(VP8DecContainer_t *dec_cont) {
  return dec_cont->asic_buff->pictures +
         VP8HwdBufferQueueGetPrevRef(dec_cont->bq);
}

i32 FindIndex(VP8DecContainer_t* dec_cont, const u32* address) {
  i32 i;

  if(dec_cont->user_mem) {
    for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
      if(dec_cont->asic_buff->user_mem.p_pic_buffer_y[i] == address)
        break;
    }
  } else {
    for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
      if (dec_cont->asic_buff->pictures[i].virtual_address == address)
        break;
    }
  }
  return i;
}

i32 FindPpIndex(VP8DecContainer_t* dec_cont, const u32* address) {
  i32 i;

  for (i = 0; i < (i32)dec_cont->num_pp_buffers; i++) {
    if (dec_cont->asic_buff->pp_pictures[i]->virtual_address == address)
      break;
  }
  return i;
}

static VP8DecRet VP8PushOutput(VP8DecContainer_t* dec_cont) {
  u32 ret=VP8DEC_OK;
  VP8DecPicture output;

  /* Sample dec_cont->out_count for Peek */
  dec_cont->fullness = dec_cont->out_count;
  if(dec_cont->num_cores == 1) {
    do {
      ret = VP8DecNextPicture_INTERNAL(dec_cont, &output, 0);
      if(ret == VP8DEC_ABORTED)
        return (VP8DEC_ABORTED);
    } while( ret == VP8DEC_PIC_RDY);
  }
  return ret;
}

void VP8SetExternalBufferInfo(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buffer;
  u32 ext_buffer_size;

  buffer = dec_cont->num_buffers_reserved;
  switch(dec_cont->dec_mode) {
  case VP8HWD_VP7:
    if(buffer < 3)
      buffer = 3;
    break;
  case VP8HWD_VP8:
    if(dec_cont->intra_only == HANTRO_TRUE)
      buffer = 1;
    else {
      if(buffer < 4)
        buffer = 4;
    }
    break;
  }

  ext_buffer_size = VP8GetRefFrmSize(dec_cont);
  if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0) + 16;
  }

  dec_cont->ext_min_buffer_num = dec_cont->buf_num = buffer;
  dec_cont->next_buf_size = ext_buffer_size;
}

VP8DecRet VP8DecGetBufferInfo(VP8DecInst dec_inst, VP8DecBufferInfo *mem_info) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;

  if(dec_cont == NULL || mem_info == NULL) {
    return VP8DEC_PARAM_ERROR;
  }

  if (dec_cont->asic_buff->release_buffer) {
    /* Release old buffers from input queue. */
    //buffer = InputQueueGetBuffer(decCont->pp_buffer_queue, 0);
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->asic_buff->release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (VP8DEC_MEMFAIL);
      }
      dec_cont->asic_buff->ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      //return VP8DEC_OK;
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return VP8DEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return VP8DEC_OK;
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

  return VP8DEC_OK;
}

VP8DecRet VP8DecAddBuffer(VP8DecInst dec_inst, struct DWLLinearMem *info) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  VP8DecRet dec_ret = VP8DEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return VP8DEC_PARAM_ERROR;
  }
  u32 i = dec_cont->buffer_index;

  if (dec_cont->buffer_index >= 16)
    /* Too much buffers added. */
    return VP8DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = VP8DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    p_asic_buff->pictures[i] = *info;
    p_asic_buff->pictures_c[i].virtual_address =
      p_asic_buff->pictures[i].virtual_address +
      (p_asic_buff->chroma_buf_offset/4);
    p_asic_buff->pictures_c[i].bus_address =
      p_asic_buff->pictures[i].bus_address +
      p_asic_buff->chroma_buf_offset;

    {
      void *base = (char*)p_asic_buff->pictures[i].virtual_address
                   + p_asic_buff->sync_mc_offset;
      (void) DWLmemset(base, ~0, 16);
    }
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->num_buffers++;
      VP8HwdBufferQueueAddBuffer(dec_cont->bq, i);
    }
  } else {
    /* Add down scale buffer. */
    dec_cont->num_pp_buffers++;
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
    p_asic_buff->pp_pictures[i] = &dec_cont->ext_buffers[i];
  }
  dec_cont->asic_buff->ext_buffer_added = 1;
  return dec_ret;
}

void VP8EnterAbortState(VP8DecContainer_t* dec_cont) {
  dec_cont->abort = 1;
  VP8HwdBufferQueueSetAbort(dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void VP8ExistAbortState(VP8DecContainer_t* dec_cont) {
  dec_cont->abort = 0;
  VP8HwdBufferQueueClearAbort(dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}


void VP8EmptyBufferQueue(VP8DecContainer_t* dec_cont) {
  u32 i;

  for (i = 0; i < dec_cont->num_buffers; i++) {
    VP8HwdBufferQueueEmptyRef(dec_cont->bq, i);
  }
}

void VP8StateReset(VP8DecContainer_t* dec_cont) {
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buffers = dec_cont->num_buffers_reserved;

  /* Clear internal parameters in VP8DecContainer_t */
  dec_cont->dec_stat = VP8DEC_INITIALIZED;
  dec_cont->pic_number = 0;
  dec_cont->display_number = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->num_buffers = buffers;
  if (dec_cont->bq)
    VP8HwdBufferQueueRelease(dec_cont->bq);
  dec_cont->bq = VP8HwdBufferQueueInitialize(dec_cont->num_buffers);
  dec_cont->num_pp_buffers = 0;
#endif
  dec_cont->last_slice = 0;
  dec_cont->fullness = 0;
  dec_cont->picture_broken = 0;
  dec_cont->out_count = 0;
  dec_cont->ref_to_out = 0;
  dec_cont->slice_concealment = 0;
  dec_cont->tot_decoded_rows = 0;
  dec_cont->output_rows = 0;
  dec_cont->conceal = 0;
  dec_cont->conceal_start_mb_x = 0;
  dec_cont->conceal_start_mb_y = 0;
  dec_cont->prev_is_key = 0;
  dec_cont->force_intra_freeze = 0;
  dec_cont->prob_refresh_detected = 0;
  dec_cont->get_buffer_after_abort = 1;
  (void) DWLmemset(&dec_cont->bc, 0, sizeof(vpBoolCoder_t));

  /* Clear internal parameters in DecAsicBuffers */
  p_asic_buff->release_buffer = 0;
  p_asic_buff->ext_buffer_added = 0;
  p_asic_buff->prev_out_buffer = NULL;
  p_asic_buff->mvs_curr = p_asic_buff->mvs_ref = 0;
  p_asic_buff->whole_pic_concealed = 0;
  p_asic_buff->dc_pred[0] = p_asic_buff->dc_pred[1] =
  p_asic_buff->dc_match[0] = p_asic_buff->dc_match[1] = 0;
  (void) DWLmemset(p_asic_buff->decode_id, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->first_show, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  (void) DWLmemset(p_asic_buff->not_displayed, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->display_index, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->picture_info, 0, 16 * sizeof(VP8DecPicture));
#endif
  p_asic_buff->out_buffer_i = REFERENCE_NOT_SET;
  p_asic_buff->out_buffer = NULL;
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_out);
  FifoInit(VP8DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
  (void)buffers;
}

VP8DecRet VP8DecAbort(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  DEC_API_TRC("VP8DecAbort#\n");

  if (dec_inst == NULL) {
    return (VP8DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  VP8EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  DEC_API_TRC("VP8DecAbort# VP8DEC_OK\n");
  return (VP8DEC_OK);
}

VP8DecRet VP8DecAbortAfter(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  DEC_API_TRC("VP8DecAbortAfter#\n");

  if (dec_inst == NULL) {
    return (VP8DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_stat == VP8DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (VP8DEC_OK);
  }
#endif

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    /* stop HW */
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 1 * 4, 0);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  VP8EmptyBufferQueue(dec_cont);

  VP8StateReset(dec_cont);

  VP8ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  DEC_API_TRC("VP8DecAbortAfter# VP8DEC_OK\n");
  return (VP8DEC_OK);
}

VP8DecRet VP8DecSetInfo(VP8DecInst dec_inst,
                        struct VP8DecConfig *dec_cfg) {
  /*@null@ */ VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 pic_width;
  u32 pic_height;
  pic_width = p_asic_buff->luma_stride ?
              p_asic_buff->luma_stride : p_asic_buff->width;
  if (!dec_cont->slice_height)
    pic_height = p_asic_buff->height;
  else
    pic_height = (dec_cont->slice_height + 1) * 16;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
  struct DecHwFeatures hw_feature;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (!hw_feature.dec_stride_support) {
    /* For verion earlier than g1v8_2, reset alignment to 16B. */
    dec_cont->align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = dec_cfg->align;
  }
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, 8, 1, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(VP8DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, 0, dec_cont->ppu_cfg))
    return VP8DEC_PARAM_ERROR;
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

  if (dec_cont->pp_enabled == 0 &&
      dec_cont->dec_mode == VP8HWD_VP8 &&
      dec_cont->intra_only == HANTRO_TRUE) {
    /* WebP */
    dec_cont->pp_enabled = 1;
  }

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
  return (VP8DEC_OK);
}



void VP8CheckBufferRealloc(VP8DecContainer_t *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (VP8GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->prev_width != dec_cont->width ||
          dec_cont->prev_height != dec_cont->height)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (VP8GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (VP8GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}
