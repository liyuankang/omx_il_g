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
#include "dwl.h"
#include "basetype.h"
#include "jpegdecapi.h"
#include "jpegdeccontainer.h"
#include "jpegdecmarkers.h"
#include "jpegdecinternal.h"
#include "jpegdecutils.h"
#include "jpegdechdrs.h"
#include "jpegdecscan.h"
#include "jpegregdrv.h"
#include "jpeg_pp_pipeline.h"
#include "commonconfig.h"
#include "sw_util.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include <string.h>
#ifdef JPEGDEC_ASIC_TRACE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jpegasicdbgtrace.h"
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

#ifdef JPEGDEC_PP_TRACE
#include "ppapi.h"
#endif /* #ifdef JPEGDEC_PP_TRACE */

extern volatile struct strmInfo stream_info;
static void JpegDecPreparePp(JpegDecContainer * dec_cont);
static JpegDecRet JpegDecSetInfo_INTERNAL(JpegDecContainer * dec_cont);
static u32 InitList(FrameBufferList *fb_list);
static void ReleaseList(FrameBufferList *fb_list);
static void ClearHWOutput(FrameBufferList *fb_list, u32 id, u32 type);
static void MarkHWOutput(FrameBufferList *fb_list, u32 id, u32 type);
static void PushOutputPic(FrameBufferList *fb_list, const JpegDecOutput *pic, const JpegDecImageInfo *info);
static u32 PeekOutputPic(FrameBufferList *fb_list, JpegDecOutput *pic, JpegDecImageInfo *info);
static void JpegMCSetHwRdyCallback(JpegDecInst dec_inst);
static void JpegRiMCSetHwRdyCallback(JpegDecInst dec_inst);

#ifdef ASIC_TRACE_SUPPORT
extern u32 stream_buffer_id;
#endif
/*------------------------------------------------------------------------------
       Version Information - DO NOT CHANGE!
------------------------------------------------------------------------------*/

#define JPG_MAJOR_VERSION 1
#define JPG_MINOR_VERSION 1

/*------------------------------------------------------------------------------
    2. External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
#ifdef JPEGDEC_TRACE
#define JPEGDEC_API_TRC(str)    JpegDecTrace((str))
#else
#define JPEGDEC_API_TRC(str)
#endif

#define JPEGDEC_CLEAR_IRQ  SetDecRegister(dec_cont->jpeg_regs, \
                                          HWIF_DEC_IRQ_STAT, 0); \
                           SetDecRegister(dec_cont->jpeg_regs, \
                                          HWIF_DEC_IRQ, 0);
#define ABORT_MARKER                 3

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static u32 JpegParseRST(JpegDecInst dec_inst, u8 *img_buf, u32 img_len,
                             u8 **ri_array);

/*------------------------------------------------------------------------------
    5. Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: JpegDecInit

        Functional description:
            Init jpeg decoder

        Inputs:
            JpegDecInst * dec_inst     a reference to the jpeg decoder instance is
                                         stored here

        Outputs:
            JPEGDEC_OK
            JPEGDEC_INITFAIL
            JPEGDEC_PARAM_ERROR
            JPEGDEC_DWL_ERROR
            JPEGDEC_MEMFAIL

------------------------------------------------------------------------------*/
JpegDecRet JpegDecInit(JpegDecInst * dec_inst,
                       const void *dwl,
		       enum DecDecoderMode decoder_mode,
                       JpegDecMCConfig *p_mcinit_cfg) {
  JpegDecContainer *dec_cont;
  u32 i = 0;
  u32 asic_id, hw_build_id;
  u32 fuse_status = 0;
  u32 extensions_supported;
  u32 webp_support;

  DWLHwConfig hw_cfg;
  struct DecHwFeatures hw_feature;
  u32 low_latency_sim = 0;

  JPEGDEC_API_TRC("JpegDecInit#");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if(((-1) >> 1) != (-1)) {
    JPEGDEC_API_TRC("JpegDecInit# ERROR: Right shift is not signed");
    return (JPEGDEC_INITFAIL);
  }

  /*lint -restore */
  if(dec_inst == NULL || dwl == NULL) {
    JPEGDEC_API_TRC("JpegDecInit# ERROR: dec_inst == NULL");
    return (JPEGDEC_PARAM_ERROR);
  }
  *dec_inst = NULL;   /* return NULL instance for any error */

  /* check for proper hardware */
  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_JPEG_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  {
    /* check that JPEG decoding supported in HW */

    DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_JPEG_DEC);
    if(!hw_feature.jpeg_support) {
      JPEGDEC_API_TRC(("JpegDecInit# ERROR: JPEG not supported in HW\n"));
      return JPEGDEC_FORMAT_NOT_SUPPORTED;
    }

    if(!hw_feature.addr64_support && sizeof(addr_t) == 8) {
      JPEGDEC_API_TRC("JpegDecInit# ERROR: HW not support 64bit address!\n");
      return (JPEGDEC_PARAM_ERROR);
    }

    /* check progressive support */
    if((asic_id >> 16) != 0x8170U) {
      /* progressive decoder */
      if(!hw_feature.jpeg_prog_support)
        fuse_status = 0;
    }

    extensions_supported = hw_feature.jpeg_esupport;
    webp_support = hw_feature.webp_support;

  }

  dec_cont = (JpegDecContainer *) DWLmalloc(sizeof(JpegDecContainer));
  if(dec_cont == NULL) {
    return (JPEGDEC_MEMFAIL);
  }

  (void)DWLmemset(dec_cont, 0, sizeof(JpegDecContainer));

  dec_cont->dwl = dwl;

  /* reset internal structures */
  JpegDecClearStructs(dec_cont, 0);

  /* Reset shadow registers */
  dec_cont->jpeg_regs[0] = asic_id;
  for(i = 1; i < TOTAL_X170_REGISTERS; i++) {
    dec_cont->jpeg_regs[i] = 0;
  }

#if 0
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
    return (JPEGDEC_PARAM_ERROR);
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
    return (JPEGDEC_MEMFAIL);
  }

  /* G1V8 (major/minor version: 8/01):
     JPEG always outputs through PP instead of decoder. */
  if (hw_feature.jpeg_pp_force_e) {  /* MINOR_VERSION */
    dec_cont->pp_enabled = 1;
  }
//  dec_cont->hw_major_ver = (asic_id & 0x0000F000) >> 12;
//  dec_cont->hw_minor_ver = (asic_id & 0x00000FF0) >> 4;

  if (hw_feature.pp_version == G1_NATIVE_PP) {
    dec_cont->align = DEC_ALIGN_16B;
  } else {
    dec_cont->align = DEC_ALIGN_128B;
  }

  SetCommonConfigRegs(dec_cont->jpeg_regs);

  /* save HW version so we dont need to check it all
   * the time when deciding the control stuff */
  dec_cont->is8190 = (asic_id >> 16) != 0x8170U ? 1 : 0;
  /* set HW related config's */
  dec_cont->max_supported_width = hw_feature.img_max_dec_width;
  dec_cont->max_supported_height = hw_feature.img_max_dec_height;
  dec_cont->max_supported_pixel_amount = hw_feature.img_max_dec_width *
                                           hw_feature.img_max_dec_height;
  if(dec_cont->is8190) {
    dec_cont->fuse_burned = fuse_status;
    /* max */
    if(webp_support) { /* webp implicates 256Mpix support */
      dec_cont->max_supported_slice_size = JPEGDEC_MAX_SLICE_SIZE_WEBP;
    } else {
      dec_cont->max_supported_slice_size = JPEGDEC_MAX_SLICE_SIZE_8190;
    }
  } else {
    /* max */
    dec_cont->max_supported_slice_size = JPEGDEC_MAX_SLICE_SIZE;
  }

  /* min */
  dec_cont->min_supported_width = JPEGDEC_MIN_WIDTH;
  dec_cont->min_supported_height = JPEGDEC_MIN_HEIGHT;

  dec_cont->extensions_supported = extensions_supported;

  if (decoder_mode == DEC_LOW_LATENCY) {
    dec_cont->low_latency = 1;
  } else if (decoder_mode == DEC_LOW_LATENCY_RTL) {
    low_latency_sim = 1;
  }

  if(dec_cont->low_latency || low_latency_sim) {
    SetDecRegister(dec_cont->jpeg_regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
  } else {
    SetDecRegister(dec_cont->jpeg_regs, HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }

  dec_cont->n_cores = 1;

  dec_cont->stream_consumed_callback.fn = p_mcinit_cfg->stream_consumed_callback;
  dec_cont->b_mc = p_mcinit_cfg->mc_enable;
  dec_cont->ri_mc_enabled = RI_MC_UNDETERMINED;
#ifdef RI_MC_SW_PARSE
  dec_cont->allow_sw_ri_parse = 1;
#endif

  dec_cont->n_cores = DWLReadAsicCoreCount();

  /* check how many cores support JPEG */
  u32 tmp = dec_cont->n_cores;
  for(i = 0; i < dec_cont->n_cores; i++) {
    hw_build_id = DWLReadCoreHwBuildID(i);
    GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
    if (!hw_feature.jpeg_support)
      tmp--;
    if (hw_feature.has_2nd_pipeline) {
      if (!hw_feature.has_2nd_jpeg_pipeline)
        tmp--;
      i++;  /* the second pipeline is treated as another core in driver */
    }
  }
  dec_cont->n_cores_available = tmp;

  if (dec_cont->b_mc) {
    if(dec_cont->n_cores > 1) {
      SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_MULTICORE_E, 0);
    }
  }
  /* Init frame buffer list */
  InitList(&dec_cont->fb_list);

  *dec_inst = (JpegDecContainer *) dec_cont;

  dec_cont->hw_feature = hw_feature;

  /* TODO(min): set this flag based on feature list, or user configuration. */
#ifdef SUPPORT_VCMD
    dec_cont->vcmd_used = 1;
#endif

  JPEGDEC_API_TRC("JpegDecInit# OK\n");
  return (JPEGDEC_OK);
}

/*------------------------------------------------------------------------------

    Function name: JpegDecRelease

        Functional description:
            Release Jpeg decoder

        Inputs:
            JpegDecInst dec_inst    jpeg decoder instance

            void

------------------------------------------------------------------------------*/
void JpegDecRelease(JpegDecInst dec_inst) {

  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
  const void *dwl;
  u32 i=0;

  JPEGDEC_API_TRC("JpegDecRelease#");

  if(dec_cont == NULL) {
    JPEGDEC_API_TRC("JpegDecRelease# ERROR: dec_inst == NULL");
    return;
  }

  dwl = dec_cont->dwl;

  if(dec_cont->asic_running) {
    /* Release HW */
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }

  if(dec_cont->vlc.ac_table0.vals) {
    DWLfree(dec_cont->vlc.ac_table0.vals);
  }
  if(dec_cont->vlc.ac_table1.vals) {
    DWLfree(dec_cont->vlc.ac_table1.vals);
  }
  if(dec_cont->vlc.ac_table2.vals) {
    DWLfree(dec_cont->vlc.ac_table2.vals);
  }
  if(dec_cont->vlc.ac_table3.vals) {
    DWLfree(dec_cont->vlc.ac_table3.vals);
  }
  if(dec_cont->vlc.dc_table0.vals) {
    DWLfree(dec_cont->vlc.dc_table0.vals);
  }
  if(dec_cont->vlc.dc_table1.vals) {
    DWLfree(dec_cont->vlc.dc_table1.vals);
  }
  if(dec_cont->vlc.dc_table2.vals) {
    DWLfree(dec_cont->vlc.dc_table2.vals);
  }
  if(dec_cont->vlc.dc_table3.vals) {
    DWLfree(dec_cont->vlc.dc_table3.vals);
  }
  if(dec_cont->frame.p_buffer) {
    DWLfree(dec_cont->frame.p_buffer);
  }
  if (dec_cont->frame.ri_array) {
    DWLfree(dec_cont->frame.ri_array);
  }
  /* progressive */
  if(dec_cont->info.p_coeff_base.virtual_address) {
    DWLFreeLinear(dwl, &(dec_cont->info.p_coeff_base));
    dec_cont->info.p_coeff_base.virtual_address = NULL;
  }
  if(dec_cont->info.tmp_strm.virtual_address) {
    DWLFreeLinear(dwl, &(dec_cont->info.tmp_strm));
    dec_cont->info.tmp_strm.virtual_address = NULL;
  }
  if(dec_cont->frame.p_table_base[0].virtual_address) {
    for (i =0 ; i < dec_cont->n_cores; i++) {
      DWLFreeLinear(dwl, &(dec_cont->frame.p_table_base[i]));
      dec_cont->frame.p_table_base[i].virtual_address = NULL;
    }
  }

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  ReleaseList(&dec_cont->fb_list);
  if(dec_inst) {
    DWLfree(dec_cont);
  }

  JPEGDEC_API_TRC("JpegDecRelease# OK\n");

  return;

}

/*------------------------------------------------------------------------------

    Function name: JpegDecGetImageInfo

        Functional description:
            Get image information of the JFIF

        Inputs:
            JpegDecInst dec_inst     jpeg decoder instance
            JpegDecInput *p_dec_in    input stream information
            JpegDecImageInfo *p_image_info
                    structure where the image info is written

        Outputs:
            JPEGDEC_OK
            JPEGDEC_ERROR
            JPEGDEC_UNSUPPORTED
            JPEGDEC_PARAM_ERROR
            JPEGDEC_INCREASE_INPUT_BUFFER
            JPEGDEC_INVALID_STREAM_LENGTH
            JPEGDEC_INVALID_INPUT_BUFFER_SIZE

------------------------------------------------------------------------------*/

/* Get image information of the JFIF */
JpegDecRet JpegDecGetImageInfo(JpegDecInst dec_inst, JpegDecInput * p_dec_in,
                               JpegDecImageInfo * p_image_info) {

  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
  u32 Nf = 0;
  u32 Ns = 0;
  u32 NsThumb = 0;
  u32 i, j = 0;
  u32 init = 0;
  u32 init_thumb = 0;
  u32 H[MAX_NUMBER_OF_COMPONENTS];
  u32 V[MAX_NUMBER_OF_COMPONENTS];
  u32 Htn[MAX_NUMBER_OF_COMPONENTS];
  u32 Vtn[MAX_NUMBER_OF_COMPONENTS];
  u32 Hmax = 0;
  u32 Vmax = 0;
  u32 header_length = 0;
  u32 current_byte = 0;
  u32 current_bytes = 0;
  u32 app_length = 0;
  u32 app_bits = 0;
  u32 thumbnail = 0;
  u32 error_code = 0;
  u32 new_header_value = 0;
  u32 marker_byte = 0;
  u32 output_width = 0;
  u32 output_height = 0;
  u32 output_width_thumb = 0;
  u32 output_height_thumb = 0;

#ifdef JPEGDEC_ERROR_RESILIENCE
  u32 error_resilience = 0;
  u32 error_resilience_thumb = 0;
#endif /* JPEGDEC_ERROR_RESILIENCE */

  StreamStorage stream;

  JPEGDEC_API_TRC("JpegDecGetImageInfo#");

  /* check pointers & parameters */
  if(dec_inst == NULL || p_dec_in == NULL || p_image_info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(p_dec_in->stream_buffer.virtual_address) ||
      X170_CHECK_BUS_ADDRESS(p_dec_in->stream_buffer.bus_address)) {
    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: NULL parameter");
    return (JPEGDEC_PARAM_ERROR);
  }

  /* Check the stream lenth */
  if(p_dec_in->stream_length < 1) {
    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: p_dec_in->stream_length");
    return (JPEGDEC_INVALID_STREAM_LENGTH);
  }

  /* Check the stream lenth */
  if((p_dec_in->stream_length > DEC_X170_MAX_STREAM_VC8000D) &&
      (p_dec_in->buffer_size < JPEGDEC_X170_MIN_BUFFER ||
       p_dec_in->buffer_size > JPEGDEC_X170_MAX_BUFFER)) {
    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: p_dec_in->buffer_size");
    return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
  }

  /* Check the stream buffer size */
  if(p_dec_in->buffer_size && (p_dec_in->buffer_size < JPEGDEC_X170_MIN_BUFFER ||
                               p_dec_in->buffer_size > JPEGDEC_X170_MAX_BUFFER)) {
    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: p_dec_in->buffer_size");
    return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
  }

  /* Check the stream buffer size */
  if(p_dec_in->buffer_size && ((p_dec_in->buffer_size % 8) != 0)) {
    JPEGDEC_API_TRC
    ("JpegDecGetImageInfo# ERROR: p_dec_in->buffer_size % 8) != 0");
    return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
  }

  /* reset sampling factors */
  for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
    H[i] = 0;
    V[i] = 0;
    Htn[i] = 0;
    Vtn[i] = 0;
  }

  /* imageInfo initialization */
  p_image_info->display_width = 0;
  p_image_info->display_height = 0;
  p_image_info->output_width = 0;
  p_image_info->output_height = 0;
  p_image_info->version = 0;
  p_image_info->units = 0;
  p_image_info->x_density = 0;
  p_image_info->y_density = 0;
  p_image_info->output_format = 0;

  /* Default value to "Thumbnail" */
  p_image_info->thumbnail_type = JPEGDEC_NO_THUMBNAIL;
  p_image_info->display_width_thumb = 0;
  p_image_info->display_height_thumb = 0;
  p_image_info->output_width_thumb = 0;
  p_image_info->output_height_thumb = 0;
  p_image_info->output_format_thumb = 0;

  /* utils initialization */
  stream.bit_pos_in_byte = 0;
  stream.p_curr_pos = (u8 *) p_dec_in->stream_buffer.virtual_address;
  stream.p_start_of_stream = (u8 *) p_dec_in->stream_buffer.virtual_address;
  stream.read_bits = 0;
  stream.appn_flag = 0;

  /* stream length */
  if(!p_dec_in->buffer_size)
    stream.stream_length = p_dec_in->stream_length;
  else
    stream.stream_length = p_dec_in->buffer_size;

  /* Read decoding parameters */
  for(stream.read_bits = 0; (stream.read_bits / 8) < stream.stream_length;
      stream.read_bits++) {
    /* Look for marker prefix byte from stream */
    marker_byte = JpegDecGetByte(&(stream));
    if(marker_byte == 0xFF) {
      current_byte = JpegDecGetByte(&(stream));
      while (current_byte == 0xFF)
        current_byte = JpegDecGetByte(&(stream));

      /* switch to certain header decoding */
      switch (current_byte) {
      /* baseline marker */
      case SOF0:
      /* progresive marker */
      case SOF2:
        if(current_byte == SOF0)
          p_image_info->coding_mode = dec_cont->info.operation_type =
                                        JPEGDEC_BASELINE;
        else
          p_image_info->coding_mode = dec_cont->info.operation_type =
                                        JPEGDEC_PROGRESSIVE;
        /* Frame header */
        i++;
        Hmax = 0;
        Vmax = 0;

        /* SOF0/SOF2 length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }

        /* Sample precision (only 8 bits/sample supported) */
        current_byte = JpegDecGetByte(&(stream));
        if(current_byte != 8) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Sample precision");
          return (JPEGDEC_UNSUPPORTED);
        }

        /* Number of Lines */
        p_image_info->output_height = JpegDecGet2Bytes(&(stream));
        p_image_info->display_height = p_image_info->output_height;

        if(p_image_info->output_height < 1) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_height Unsupported");
          return (JPEGDEC_UNSUPPORTED);
        }

        /* check if height changes (MJPEG) */
        if(dec_cont->frame.Y != 0 &&
            (dec_cont->frame.Y != p_image_info->output_height)) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_height changed (MJPEG)");
          new_header_value = 1;
        }

#ifdef JPEGDEC_ERROR_RESILIENCE
        if((p_image_info->output_height & 0xF) &&
            (p_image_info->output_height & 0xF) <= 8)
          error_resilience = 1;
#endif /* JPEGDEC_ERROR_RESILIENCE */

        /* Height only round up to next multiple-of-8 instead of 16 */
        p_image_info->output_height += 0x7;
        p_image_info->output_height &= ~(0x7);

        /* Number of Samples per Line */
        p_image_info->output_width = JpegDecGet2Bytes(&(stream));
        p_image_info->display_width = p_image_info->output_width;
        if(p_image_info->output_width < 1) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_width unsupported");
          return (JPEGDEC_UNSUPPORTED);
        }

        /* check if width changes (MJPEG) */
        if(dec_cont->frame.X != 0 &&
            (dec_cont->frame.X != p_image_info->output_width)) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_width changed (MJPEG)");
          new_header_value = 1;
        }

        p_image_info->output_width += 0xF;
        p_image_info->output_width &= ~(0xF);

#if 0
        /* check if height changes (MJPEG) */
        if(dec_cont->frame.hw_y != 0 &&
            (dec_cont->frame.hw_y != p_image_info->output_height)) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_height changed (MJPEG)");
          new_header_value = 1;
        }

        /* check if width changes (MJPEG) */
        if(dec_cont->frame.hw_x != 0 &&
            (dec_cont->frame.hw_x != p_image_info->output_width)) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: p_image_info->output_width changed (MJPEG)");
          new_header_value = 1;
        }
#endif

        /* check for minimum and maximum dimensions */
        if(p_image_info->output_width < dec_cont->min_supported_width ||
            p_image_info->output_height < dec_cont->min_supported_height ||
            p_image_info->output_width > dec_cont->max_supported_width ||
            p_image_info->output_height > dec_cont->max_supported_height ||
            (p_image_info->output_width * p_image_info->output_height) >
            dec_cont->max_supported_pixel_amount) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Unsupported size");
          return (JPEGDEC_UNSUPPORTED);
        }

        /* Number of Image Components per Frame */
        Nf = JpegDecGetByte(&(stream));
        if(Nf != 3 && Nf != 1) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Number of Image Components per Frame");
          return (JPEGDEC_UNSUPPORTED);
        }
        /* length 8 + 3 x Nf */
        if(header_length != (8 + (3 * Nf))) {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Incorrect SOF0 header length");
          return (JPEGDEC_UNSUPPORTED);
        }
        for(j = 0; j < Nf; j++) {
          /* jump over component identifier */
          if(JpegDecFlushBits(&(stream), 8) == STRM_ERROR) {
            error_code = 1;
            break;
          }

          /* Horizontal sampling factor */
          current_byte = JpegDecGetByte(&(stream));
          H[j] = (current_byte >> 4);

          /* Vertical sampling factor */
          V[j] = (current_byte & 0xF);

          /* jump over Tq */
          if(JpegDecFlushBits(&(stream), 8) == STRM_ERROR) {
            error_code = 1;
            break;
          }

          if(H[j] > Hmax)
            Hmax = H[j];
          if(V[j] > Vmax)
            Vmax = V[j];
        }
        if(Hmax == 0 || Vmax == 0) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Hmax == 0 || Vmax == 0");
          return (JPEGDEC_UNSUPPORTED);
        }
#ifdef JPEGDEC_ERROR_RESILIENCE
        if(H[0] == 2 && V[0] == 2 &&
            H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          p_image_info->output_format = JPEGDEC_YCbCr420_SEMIPLANAR;
        } else {
          /* check if fill needed */
          if(error_resilience) {
            p_image_info->output_height -= 16;
            p_image_info->display_height = p_image_info->output_height;
          }
        }
#endif /* JPEGDEC_ERROR_RESILIENCE */

        /* check format */
        if(H[0] == 2 && V[0] == 2 &&
            H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          p_image_info->output_format = JPEGDEC_YCbCr420_SEMIPLANAR;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 16);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              256);
          dec_cont->frame.mcu_height = 16;
        } else if(H[0] == 2 && V[0] == 1 &&
                  H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          p_image_info->output_format = JPEGDEC_YCbCr422_SEMIPLANAR;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 16);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              128);
          dec_cont->frame.mcu_height = 8;
        } else if(H[0] == 1 && V[0] == 2 &&
                  H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          p_image_info->output_format = JPEGDEC_YCbCr440;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 8);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              128);
          dec_cont->frame.mcu_height = 16;
        } else if(H[0] == 1 && V[0] == 1 &&
                  H[1] == 0 && V[1] == 0 && H[2] == 0 && V[2] == 0) {
          p_image_info->output_format = JPEGDEC_YCbCr400;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 8);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              64);
          dec_cont->frame.mcu_height = 8;
        } else if(dec_cont->extensions_supported &&
                  H[0] == 4 && V[0] == 1 &&
                  H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          /* YUV411 output has to be 32 pixel multiple */
          if(p_image_info->output_width & 0x1F) {
            p_image_info->output_width += 16;
          }

          /* check for maximum dimensions */
          if(p_image_info->output_width > dec_cont->max_supported_width ||
              (p_image_info->output_width * p_image_info->output_height) >
              dec_cont->max_supported_pixel_amount) {
            JPEGDEC_API_TRC
            ("JpegDecGetImageInfo# ERROR: Unsupported size");
            return (JPEGDEC_UNSUPPORTED);
          }

          p_image_info->output_format = JPEGDEC_YCbCr411_SEMIPLANAR;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 32);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              256);
          dec_cont->frame.mcu_height = 8;
        } else if(dec_cont->extensions_supported &&
                  H[0] == 1 && V[0] == 1 &&
                  H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1) {
          p_image_info->output_format = JPEGDEC_YCbCr444_SEMIPLANAR;
          dec_cont->frame.num_mcu_in_row = (p_image_info->output_width / 8);
          dec_cont->frame.num_mcu_in_frame = ((p_image_info->output_width *
                                               p_image_info->output_height) /
                                              64);
          dec_cont->frame.mcu_height = 8;
        } else {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Unsupported YCbCr format");
          return (JPEGDEC_UNSUPPORTED);
        }
        output_width = p_image_info->output_width;
        output_height = p_image_info->output_height;

        /* check if output format changes (MJPEG) */
        if(dec_cont->info.get_info_ycb_cr_mode != 0 &&
            (dec_cont->info.get_info_ycb_cr_mode != p_image_info->output_format)) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: YCbCr format changed (MJPEG)");
          new_header_value = 1;
        }
        break;
      case SOS:
        /* SOS length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }

        /* check if interleaved or non-ibnterleaved */
        Ns = JpegDecGetByte(&(stream));
        if(Ns == MIN_NUMBER_OF_COMPONENTS &&
            p_image_info->output_format != JPEGDEC_YCbCr400 &&
            p_image_info->coding_mode == JPEGDEC_BASELINE) {
          p_image_info->coding_mode = dec_cont->info.operation_type =
                                        JPEGDEC_NONINTERLEAVED;
        }
        /* Number of Image Components */
        if(Ns != 3 && Ns != 1) {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Number of Image Components");
          return (JPEGDEC_UNSUPPORTED);
        }
        /* length 6 + 2 x Ns */
        if(header_length != (6 + (2 * Ns))) {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Incorrect SOS header length");
          return (JPEGDEC_UNSUPPORTED);
        }
        /* jump over SOS header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }

        if((stream.read_bits + 8) < (8 * stream.stream_length)) {
          dec_cont->info.init = 1;
          init = 1;
        } else {
          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Needs to increase input buffer");
          return (JPEGDEC_INCREASE_INPUT_BUFFER);
        }
        break;
      case DQT:
        /* DQT length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length >= (2 + 65) (baseline) */
        if(header_length < (2 + 65)) {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Incorrect DQT header length");
          return (JPEGDEC_UNSUPPORTED);
        }
        /* jump over DQT header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
        break;
      case DHT:
        /* DHT length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length >= 2 + 17 */
        if(header_length < 19) {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Incorrect DHT header length");
          return (JPEGDEC_UNSUPPORTED);
        }
        /* jump over DHT header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
        break;
      case DRI:
        /* DRI length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length == 4 */
        if(header_length != 4) {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Incorrect DRI header length");
          return (JPEGDEC_UNSUPPORTED);
        }
#if 0
        /* jump over DRI header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
#endif
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        dec_cont->frame.Ri = header_length;
        if (dec_cont->frame.num_mcu_in_row && dec_cont->frame.Ri > 0 &&
            dec_cont->frame.Ri % dec_cont->frame.num_mcu_in_row == 0 &&
            dec_cont->n_cores_available > 1 &&
            !RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          FrameInfo *frame = &dec_cont->frame;
          dec_cont->ri_mc_enabled = RI_MC_ENA;
          frame->num_mcu_rows_in_interval = frame->Ri / frame->num_mcu_in_row;
          frame->num_mcu_rows_in_frame = frame->num_mcu_in_frame / frame->num_mcu_in_row;
          frame->intervals = (frame->num_mcu_in_frame + frame->Ri - 1) / frame->Ri;
        }
        break;
      /* application segments */
      case APP0:
        JPEGDEC_API_TRC("JpegDecGetImageInfo# APP0 in GetImageInfo");
        /* reset */
        app_bits = 0;
        app_length = 0;
        stream.appn_flag = 0;

        /* APP0 length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length > 2 */
        if(header_length < 2)
          break;
        app_length = header_length;
        if(app_length < 16) {
          stream.appn_flag = 1;
          if(JpegDecFlushBits(&(stream), ((app_length * 8) - 16)) ==
              STRM_ERROR) {
            error_code = 1;
            break;
          }
          break;
        }
        app_bits += 16;

        /* check identifier */
        current_bytes = JpegDecGet2Bytes(&(stream));
        app_bits += 16;
        if(current_bytes != 0x4A46) {
          stream.appn_flag = 1;
          if(JpegDecFlushBits(&(stream), ((app_length * 8) - app_bits))
              == STRM_ERROR) {
            error_code = 1;
            break;
          }
          break;
        }
        current_bytes = JpegDecGet2Bytes(&(stream));
        app_bits += 16;
        if(current_bytes != 0x4946 && current_bytes != 0x5858) {
          stream.appn_flag = 1;
          if(JpegDecFlushBits(&(stream), ((app_length * 8) - app_bits))
              == STRM_ERROR) {
            error_code = 1;
            break;
          }
          break;
        }

        /* APP0 Extended */
        if(current_bytes == 0x5858) {
          thumbnail = 1;
        }
        current_byte = JpegDecGetByte(&(stream));
        app_bits += 8;
        if(current_byte != 0x00) {
          stream.appn_flag = 1;
          if(JpegDecFlushBits(&(stream), ((app_length * 8) - app_bits))
              == STRM_ERROR) {
            error_code = 1;
            break;
          }
          stream.appn_flag = 0;
          break;
        }

        /* APP0 Extended thumb type */
        if(thumbnail) {
          /* extension code */
          current_byte = JpegDecGetByte(&(stream));
          if(current_byte == JPEGDEC_THUMBNAIL_JPEG) {
            p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_JPEG;
            app_bits += 8;
            stream.appn_flag = 1;

            /* check thumbnail data */
            Hmax = 0;
            Vmax = 0;

            /* Read decoding parameters */
            for(; (stream.read_bits / 8) < stream.stream_length;
                stream.read_bits++) {
              /* Look for marker prefix byte from stream */
              app_bits += 8;
              marker_byte = JpegDecGetByte(&(stream));
              /* check if APP0 decoded */
              if( ((app_bits + 8) / 8) >= app_length)
                break;
              if(marker_byte == 0xFF) {
                /* switch to certain header decoding */
                app_bits += 8;

                current_byte = JpegDecGetByte(&(stream));
                switch (current_byte) {
                /* baseline marker */
                case SOF0:
                /* progresive marker */
                case SOF2:
                  if(current_byte == SOF0)
                    p_image_info->coding_mode_thumb =
                      dec_cont->info.operation_type_thumb =
                        JPEGDEC_BASELINE;
                  else
                    p_image_info->coding_mode_thumb =
                      dec_cont->info.operation_type_thumb =
                        JPEGDEC_PROGRESSIVE;
                  /* Frame header */
                  i++;

                  /* jump over Lf field */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR ||
                      ((stream.read_bits + ((header_length * 8) - 16)) >
                       (8 * stream.stream_length))) {
                    error_code = 1;
                    break;
                  }
                  app_bits += 16;

                  /* Sample precision (only 8 bits/sample supported) */
                  current_byte = JpegDecGetByte(&(stream));
                  app_bits += 8;
                  if(current_byte != 8) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Sample precision");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }

                  /* Number of Lines */
                  p_image_info->output_height_thumb =
                    JpegDecGet2Bytes(&(stream));
                  app_bits += 16;
                  p_image_info->display_height_thumb =
                    p_image_info->output_height_thumb;
                  if(p_image_info->output_height_thumb < 1) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: p_image_info->output_height_thumb unsupported");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
#ifdef JPEGDEC_ERROR_RESILIENCE
                  if((p_image_info->output_height_thumb & 0xF) &&
                      (p_image_info->output_height_thumb & 0xF) <=
                      8)
                    error_resilience_thumb = 1;
#endif /* JPEGDEC_ERROR_RESILIENCE */

                  /* round up to next multiple-of-7 */
                  p_image_info->output_height_thumb += 0x7;
                  p_image_info->output_height_thumb &= ~(0x7);

                  /* Number of Samples per Line */
                  p_image_info->output_width_thumb =
                    JpegDecGet2Bytes(&(stream));
                  app_bits += 16;
                  p_image_info->display_width_thumb =
                    p_image_info->output_width_thumb;
                  if(p_image_info->output_width_thumb < 1) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: p_image_info->output_width_thumb unsupported");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  p_image_info->output_width_thumb += 0xF;
                  p_image_info->output_width_thumb &= ~(0xF);

                  /* check if height changes (MJPEG) */
                  if(dec_cont->frame.hw_ytn != 0 &&
                      (dec_cont->frame.hw_ytn != p_image_info->output_height_thumb)) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: p_image_info->output_height_thumb changed (MJPEG)");
                    new_header_value = 1;
                  }

                  /* check if width changes (MJPEG) */
                  if(dec_cont->frame.hw_xtn != 0 &&
                      (dec_cont->frame.hw_xtn != p_image_info->output_width_thumb)) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: p_image_info->output_width_thumb changed (MJPEG)");
                    new_header_value = 1;
                  }

                  if(p_image_info->output_width_thumb <
                      dec_cont->min_supported_width ||
                      p_image_info->output_height_thumb <
                      dec_cont->min_supported_height ||
                      p_image_info->output_width_thumb >
                      JPEGDEC_MAX_WIDTH_TN ||
                      p_image_info->output_height_thumb >
                      JPEGDEC_MAX_HEIGHT_TN) {

                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Unsupported size");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }

                  /* Number of Image Components per Frame */
                  Nf = JpegDecGetByte(&(stream));
                  app_bits += 8;
                  if(Nf != 3 && Nf != 1) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Number of Image Components per Frame");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  /* length 8 + 3 x Nf */
                  if(header_length != (8 + (3 * Nf))) {
                    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail Incorrect SOF0 header length");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  for(j = 0; j < Nf; j++) {

                    /* jump over component identifier */
                    if(JpegDecFlushBits(&(stream), 8) ==
                        STRM_ERROR) {
                      error_code = 1;
                      break;
                    }
                    app_bits += 8;

                    /* Horizontal sampling factor */
                    current_byte = JpegDecGetByte(&(stream));
                    app_bits += 8;
                    Htn[j] = (current_byte >> 4);

                    /* Vertical sampling factor */
                    Vtn[j] = (current_byte & 0xF);

                    /* jump over Tq */
                    if(JpegDecFlushBits(&(stream), 8) ==
                        STRM_ERROR) {
                      error_code = 1;
                      break;
                    }
                    app_bits += 8;

                    if(Htn[j] > Hmax)
                      Hmax = Htn[j];
                    if(Vtn[j] > Vmax)
                      Vmax = Vtn[j];
                  }
                  if(Hmax == 0 || Vmax == 0) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Hmax == 0 || Vmax == 0");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
#ifdef JPEGDEC_ERROR_RESILIENCE
                  if(Htn[0] == 2 && Vtn[0] == 2 &&
                      Htn[1] == 1 && Vtn[1] == 1 &&
                      Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr420_SEMIPLANAR;
                  } else {
                    /* check if fill needed */
                    if(error_resilience_thumb) {
                      p_image_info->output_height_thumb -= 16;
                      p_image_info->display_height_thumb =
                        p_image_info->output_height_thumb;
                    }
                  }
#endif /* JPEGDEC_ERROR_RESILIENCE */

                  /* check format */
                  if(Htn[0] == 2 && Vtn[0] == 2 &&
                      Htn[1] == 1 && Vtn[1] == 1 &&
                      Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr420_SEMIPLANAR;
                  } else if(Htn[0] == 2 && Vtn[0] == 1 &&
                            Htn[1] == 1 && Vtn[1] == 1 &&
                            Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr422_SEMIPLANAR;
                  } else if(Htn[0] == 1 && Vtn[0] == 2 &&
                            Htn[1] == 1 && Vtn[1] == 1 &&
                            Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr440;
                  } else if(Htn[0] == 1 && Vtn[0] == 1 &&
                            Htn[1] == 0 && Vtn[1] == 0 &&
                            Htn[2] == 0 && Vtn[2] == 0) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr400;
                  } else if(dec_cont->is8190 &&
                            Htn[0] == 4 && Vtn[0] == 1 &&
                            Htn[1] == 1 && Vtn[1] == 1 &&
                            Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr411_SEMIPLANAR;
                  } else if(dec_cont->is8190 &&
                            Htn[0] == 1 && Vtn[0] == 1 &&
                            Htn[1] == 1 && Vtn[1] == 1 &&
                            Htn[2] == 1 && Vtn[2] == 1) {
                    p_image_info->output_format_thumb =
                      JPEGDEC_YCbCr444_SEMIPLANAR;
                  } else {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Unsupported YCbCr format");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  output_width_thumb = p_image_info->output_width_thumb;
                  output_height_thumb = p_image_info->output_height_thumb;
#if 0
                  if(dec_cont->pp_enabled && !(dec_cont->info.user_alloc_mem) &&
                     (dec_cont->hw_major_ver == 7 && dec_cont->hw_minor_ver >= 1)) {
                    p_image_info->output_width_thumb = p_image_info->output_width_thumb >> dec_cont->dscale_shift_x;
                    p_image_info->output_height_thumb = p_image_info->output_height_thumb >> dec_cont->dscale_shift_y;
                    p_image_info->display_width_thumb = p_image_info->display_width_thumb >> dec_cont->dscale_shift_x;
                    p_image_info->display_height_thumb = p_image_info->display_height_thumb >> dec_cont->dscale_shift_y;
                  }
#endif
                  /* check if output format changes (MJPEG) */
                  if(dec_cont->info.get_info_ycb_cr_mode_tn != 0 &&
                      (dec_cont->info.get_info_ycb_cr_mode_tn != p_image_info->output_format_thumb)) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail YCbCr format changed (MJPEG)");
                    new_header_value = 1;
                  }
                  break;
                case SOS:
                  /* SOS length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR ||
                      ((stream.read_bits +
                        ((header_length * 8) - 16)) >
                       (8 * stream.stream_length))) {
                    error_code = 1;
                    break;
                  }

                  /* check if interleaved or non-ibnterleaved */
                  NsThumb = JpegDecGetByte(&(stream));
                  if(NsThumb == MIN_NUMBER_OF_COMPONENTS &&
                      p_image_info->output_format_thumb !=
                      JPEGDEC_YCbCr400 &&
                      p_image_info->coding_mode_thumb ==
                      JPEGDEC_BASELINE) {
                    p_image_info->coding_mode_thumb =
                      dec_cont->info.operation_type_thumb =
                        JPEGDEC_NONINTERLEAVED;
                  }
                  if(NsThumb != 3 && NsThumb != 1) {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Thumbnail Number of Image Components");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  /* length 6 + 2 x NsThumb */
                  if(header_length != (6 + (2 * NsThumb))) {
                    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail Incorrect SOS header length");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                  }
                  /* jump over SOS header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }

                  if((stream.read_bits + 8) <
                      (8 * stream.stream_length)) {
                    dec_cont->info.init_thumb = 1;
                    init_thumb = 1;
                  } else {
                    JPEGDEC_API_TRC
                    ("JpegDecGetImageInfo# ERROR: Needs to increase input buffer");
                    return (JPEGDEC_INCREASE_INPUT_BUFFER);
                  }
                  break;
                case DQT:
                  /* DQT length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* length >= (2 + 65) (baseline) */
                  if(header_length < (2 + 65)) {
                    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail Incorrect DQT header length");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                    break;
                  }
                  /* jump over DQT header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                case DHT:
                  /* DHT length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* length >= 2 + 17 */
                  if(header_length < 19) {
                    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail Incorrect DHT header length");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                    break;
                  }
                  /* jump over DHT header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                case DRI:
                  /* DRI length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* length == 4 */
                  if(header_length != 4) {
                    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail Incorrect DRI header length");
                    p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                    break;
                  }
                  /* jump over DRI header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                case APP0:
                case APP1:
                case APP2:
                case APP3:
                case APP4:
                case APP5:
                case APP6:
                case APP7:
                case APP8:
                case APP9:
                case APP10:
                case APP11:
                case APP12:
                case APP13:
                case APP14:
                case APP15:
                  /* APPn length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* header_length > 2 */
                  if(header_length < 2)
                    break;
                  /* jump over APPn header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                case DNL:
                  /* DNL length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* length == 4 */
                  if(header_length != 4)
                    break;
                  /* jump over DNL header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                case COM:
                  /* COM length */
                  header_length = JpegDecGet2Bytes(&(stream));
                  if(header_length == STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  /* length > 2 */
                  if(header_length < 2)
                    break;
                  /* jump over COM header */
                  if(header_length != 0) {
                    stream.read_bits +=
                      ((header_length * 8) - 16);
                    stream.p_curr_pos +=
                      (((header_length * 8) - 16) / 8);
                  }
                  app_bits += (header_length * 8);
                  break;
                /* unsupported coding styles */
                case SOF1:
                case SOF3:
                case SOF5:
                case SOF6:
                case SOF7:
                case SOF9:
                case SOF10:
                case SOF11:
                case SOF13:
                case SOF14:
                case SOF15:
                case DAC:
                case DHP:
                  JPEGDEC_API_TRC
                  ("JpegDecGetImageInfo# ERROR: Unsupported coding styles");
                  return (JPEGDEC_UNSUPPORTED);
                default:
                  break;
                }
                if(dec_cont->info.init_thumb && init_thumb) {
                  /* flush the rest of thumbnail data */
                  if(JpegDecFlushBits
                      (&(stream),
                       ((app_length * 8) - app_bits)) ==
                      STRM_ERROR) {
                    error_code = 1;
                    break;
                  }
                  stream.appn_flag = 0;
                  break;
                }
              } else {
                if(!dec_cont->info.init_thumb &&
                    ((stream.read_bits + 8) >= (stream.stream_length * 8)) &&
                    p_dec_in->buffer_size)
                  return (JPEGDEC_INCREASE_INPUT_BUFFER);

                if(marker_byte == STRM_ERROR )
                  return (JPEGDEC_STRM_ERROR);
              }
            }
            if(!dec_cont->info.init_thumb && !init_thumb) {
              JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Thumbnail contains no data");
              p_image_info->thumbnail_type = JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
            }
            break;
          } else {
            app_bits += 8;
            p_image_info->thumbnail_type =
              JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
            stream.appn_flag = 1;
            if(JpegDecFlushBits
                (&(stream),
                 ((app_length * 8) - app_bits)) == STRM_ERROR) {
              error_code = 1;
              break;
            }
            stream.appn_flag = 0;
            break;
          }
        } else {
          /* version */
          p_image_info->version = JpegDecGet2Bytes(&(stream));
          app_bits += 16;

          /* units */
          current_byte = JpegDecGetByte(&(stream));
          if(current_byte == 0) {
            p_image_info->units = JPEGDEC_NO_UNITS;
          } else if(current_byte == 1) {
            p_image_info->units = JPEGDEC_DOTS_PER_INCH;
          } else if(current_byte == 2) {
            p_image_info->units = JPEGDEC_DOTS_PER_CM;
          }
          app_bits += 8;

          /* Xdensity */
          p_image_info->x_density = JpegDecGet2Bytes(&(stream));
          app_bits += 16;

          /* Ydensity */
          p_image_info->y_density = JpegDecGet2Bytes(&(stream));
          app_bits += 16;

          /* jump over rest of header data */
          stream.appn_flag = 1;
          if(JpegDecFlushBits(&(stream), ((app_length * 8) - app_bits))
              == STRM_ERROR) {
            error_code = 1;
            break;
          }
          stream.appn_flag = 0;
          break;
        }
      case APP1:
      case APP2:
      case APP3:
      case APP4:
      case APP5:
      case APP6:
      case APP7:
      case APP8:
      case APP9:
      case APP10:
      case APP11:
      case APP12:
      case APP13:
      case APP14:
      case APP15:
        /* APPn length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length > 2 */
        if(header_length < 2)
          break;
        /* jump over APPn header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
        break;
      case DNL:
        /* DNL length */
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length == 4 */
        if(header_length != 4)
          break;
        /* jump over DNL header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
        break;
      case COM:
        header_length = JpegDecGet2Bytes(&(stream));
        if(header_length == STRM_ERROR ||
            ((stream.read_bits + ((header_length * 8) - 16)) >
             (8 * stream.stream_length))) {
          error_code = 1;
          break;
        }
        /* length > 2 */
        if(header_length < 2)
          break;
        /* jump over COM header */
        if(header_length != 0) {
          stream.read_bits += ((header_length * 8) - 16);
          stream.p_curr_pos += (((header_length * 8) - 16) / 8);
        }
        break;
      /* unsupported coding styles */
      case SOF1:
      case SOF3:
      case SOF5:
      case SOF6:
      case SOF7:
      case SOF9:
      case SOF10:
      case SOF11:
      case SOF13:
      case SOF14:
      case SOF15:
      case DAC:
      case DHP:
        JPEGDEC_API_TRC
        ("JpegDecGetImageInfo# ERROR: Unsupported coding styles");
        return (JPEGDEC_UNSUPPORTED);
      default:
        break;
      }
      if(dec_cont->info.init && init) {
        dec_cont->frame.hw_y = dec_cont->frame.full_y = output_height;
        dec_cont->frame.hw_x = dec_cont->frame.full_x = output_width;
        /* restore output format */
        dec_cont->info.y_cb_cr_mode = dec_cont->info.get_info_ycb_cr_mode =
                                        p_image_info->output_format;
        if(thumbnail) {
          dec_cont->frame.hw_ytn = output_height_thumb;
          dec_cont->frame.hw_xtn = output_width_thumb;
          /* restore output format for thumb */
          dec_cont->info.get_info_ycb_cr_mode_tn = p_image_info->output_format_thumb;
        }
        break;
      }

      if(error_code) {
        if(p_dec_in->buffer_size) {
          /* reset to ensure that big enough buffer will be allocated for decoding */
          if(new_header_value) {
            p_image_info->output_height = dec_cont->frame.hw_y;
            p_image_info->output_width = dec_cont->frame.hw_x;
            p_image_info->output_format = dec_cont->info.get_info_ycb_cr_mode;
            if(thumbnail) {
              p_image_info->output_height_thumb = dec_cont->frame.hw_ytn;
              p_image_info->output_width_thumb = dec_cont->frame.hw_xtn;
              p_image_info->output_format_thumb = dec_cont->info.get_info_ycb_cr_mode_tn;
            }
          }

          JPEGDEC_API_TRC
          ("JpegDecGetImageInfo# ERROR: Image info failed, Needs to increase input buffer");
          return (JPEGDEC_INCREASE_INPUT_BUFFER);
        } else {
          JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR: Stream error");
          return (JPEGDEC_STRM_ERROR);
        }
      }
    } else {
      if(!dec_cont->info.init && (stream.read_bits + 8 >= (stream.stream_length * 8)) )
        return (JPEGDEC_INCREASE_INPUT_BUFFER);

      if(marker_byte == STRM_ERROR )
        return (JPEGDEC_STRM_ERROR);
    }
  }
  if(dec_cont->info.init) {
    if(p_dec_in->buffer_size)
      dec_cont->info.init_buffer_size = p_dec_in->buffer_size;

    p_image_info->output_width = NEXT_MULTIPLE(p_image_info->output_width, ALIGN(dec_cont->align));
    p_image_info->output_width_thumb = NEXT_MULTIPLE(p_image_info->output_width_thumb, ALIGN(dec_cont->align));

    p_image_info->align = dec_cont->align;

    dec_cont->p_image_info = *p_image_info;

    JPEGDEC_API_TRC("JpegDecGetImageInfo# OK\n");
    return (JPEGDEC_OK);
  } else {
    JPEGDEC_API_TRC("JpegDecGetImageInfo# ERROR\n");
    return (JPEGDEC_ERROR);
  }
}

/*------------------------------------------------------------------------------

    Function name: JpegDecDecode

        Functional description:
            Decode JFIF

        Inputs:
            JpegDecInst dec_inst     jpeg decoder instance
            JpegDecInput *p_dec_in    pointer to structure where the decoder
                                    stores input information
            JpegDecOutput *p_dec_out  pointer to structure where the decoder
                                    stores output frame information

        Outputs:
            JPEGDEC_FRAME_READY
            JPEGDEC_PARAM_ERROR
            JPEGDEC_INVALID_STREAM_LENGTH
            JPEGDEC_INVALID_INPUT_BUFFER_SIZE
            JPEGDEC_UNSUPPORTED
            JPEGDEC_ERROR
            JPEGDEC_STRM_ERROR
            JPEGDEC_HW_BUS_ERROR
            JPEGDEC_DWL_HW_TIMEOUT
            JPEGDEC_SYSTEM_ERROR
            JPEGDEC_HW_RESERVED
            JPEGDEC_STRM_PROCESSED

  ------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecode(JpegDecInst dec_inst, JpegDecInput * p_dec_in,
                         JpegDecOutput * p_dec_out) {

  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
#define JPG_FRM  dec_cont->frame
#define JPG_INFO  dec_cont->info

  i32 dwlret = -1;
  u32 i = 0;
  u32 current_byte = 0;
  u32 marker_byte = 0;
  u32 current_bytes = 0;
  u32 app_length = 0;
  u32 app_bits = 0;
  u32 asic_status = 0;
  u32 HINTdec = 0;
  u32 asic_slice_bit = 0;
  u32 int_dec = 0;
  addr_t current_pos = 0;
  u32 end_of_image = 0;
  u32 non_interleaved_rdy = 0;
  JpegDecRet info_ret;
  JpegDecRet ret_code; /* Returned code container */
  JpegDecImageInfo info_tmp;
  u32 mcu_size_divider = 0;
  u32 DHTfromStream = 0;
#ifdef SUPPORT_DEC400
  u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address=0;
  u32 tile_status_address_offset = 0;
#endif
  u32 asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DWLLinearMem *pp_buffer = NULL;

  JPEGDEC_API_TRC("JpegDecDecode#");

  /* check null */
  if(dec_inst == NULL || p_dec_in == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(p_dec_in->stream_buffer.virtual_address) ||
      X170_CHECK_BUS_ADDRESS(p_dec_in->stream_buffer.bus_address) ||
      p_dec_out == NULL) {
    JPEGDEC_API_TRC("JpegDecDecode# ERROR: NULL parameter");
    return (JPEGDEC_PARAM_ERROR);
  }

  /* check image decoding type */
  if(p_dec_in->dec_image_type != 0 && p_dec_in->dec_image_type != 1) {
    JPEGDEC_API_TRC("JpegDecDecode# ERROR: dec_image_type");
    return (JPEGDEC_PARAM_ERROR);
  }

  /* check user allocated null */
  if((p_dec_in->picture_buffer_y.virtual_address == NULL &&
      p_dec_in->picture_buffer_y.bus_address != 0) ||
      (p_dec_in->picture_buffer_y.virtual_address != NULL &&
       p_dec_in->picture_buffer_y.bus_address == 0) ||
      (p_dec_in->picture_buffer_cb_cr.virtual_address == NULL &&
       p_dec_in->picture_buffer_cb_cr.bus_address != 0) ||
      (p_dec_in->picture_buffer_cb_cr.virtual_address != NULL &&
       p_dec_in->picture_buffer_cb_cr.bus_address == 0)) {
    JPEGDEC_API_TRC("JpegDecDecode# ERROR: NULL parameter");
    return (JPEGDEC_PARAM_ERROR);
  }

  if(dec_cont->abort)
    return (JPEGDEC_ABORTED);

  /* Check the stream lenth */
  if(p_dec_in->stream_length < 1) {
    JPEGDEC_API_TRC("JpegDecDecode# ERROR: p_dec_in->stream_length");
    return (JPEGDEC_INVALID_STREAM_LENGTH);
  }

  /* check the input buffer settings ==>
   * checks are discarded for last buffer */
  if(!dec_cont->info.stream_end_flag) {
    /* Check the stream lenth */
    if(!dec_cont->info.input_buffer_empty &&
        (p_dec_in->stream_length > DEC_X170_MAX_STREAM_VC8000D) &&
        (p_dec_in->buffer_size < JPEGDEC_X170_MIN_BUFFER ||
         p_dec_in->buffer_size > JPEGDEC_X170_MAX_BUFFER)) {
      JPEGDEC_API_TRC("JpegDecDecode# ERROR: p_dec_in->buffer_size");
      return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    /* Check the stream buffer size */
    if(!dec_cont->info.input_buffer_empty &&
        p_dec_in->buffer_size && (p_dec_in->buffer_size < JPEGDEC_X170_MIN_BUFFER
                                  || p_dec_in->buffer_size >
                                  JPEGDEC_X170_MAX_BUFFER)) {
      JPEGDEC_API_TRC("JpegDecDecode# ERROR: p_dec_in->buffer_size");
      return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    /* Check the stream buffer size */
    if(!dec_cont->info.input_buffer_empty &&
        p_dec_in->buffer_size && ((p_dec_in->buffer_size % 256) != 0)) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: p_dec_in->buffer_size % 256) != 0");
      return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    if(!dec_cont->info.input_buffer_empty &&
        dec_cont->info.init &&
        (p_dec_in->buffer_size < dec_cont->info.init_buffer_size)) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: Increase input buffer size!\n");
      return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }
  }

  if(!dec_cont->info.init && !dec_cont->info.SliceReadyForPause &&
      !dec_cont->info.input_buffer_empty) {
    JPEGDEC_API_TRC(("JpegDecDecode#: Get Image Info!\n"));

    info_ret = JpegDecGetImageInfo(dec_inst, p_dec_in, &info_tmp);

    if(info_ret != JPEGDEC_OK) {
      JPEGDEC_API_TRC(("JpegDecDecode# ERROR: Image info failed!"));
      return (info_ret);
    }
  }

  /* Store the stream parameters */
  if(dec_cont->info.progressive_scan_ready == 0 &&
      dec_cont->info.non_interleaved_scan_ready == 0) {
    dec_cont->stream.bit_pos_in_byte = 0;
    dec_cont->stream.p_curr_pos = (u8 *) p_dec_in->stream_buffer.virtual_address;
    dec_cont->stream.stream_bus = p_dec_in->stream_buffer.bus_address;
    dec_cont->stream.p_start_of_stream =
      (u8 *) p_dec_in->stream_buffer.virtual_address;
    dec_cont->stream.read_bits = 0;
    dec_cont->stream.stream_length = p_dec_in->stream_length;
    dec_cont->stream.appn_flag = 0;
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      p_dec_out->pictures[i].output_picture_y.virtual_address = NULL;
      p_dec_out->pictures[i].output_picture_y.bus_address = 0;
      p_dec_out->pictures[i].output_picture_cb_cr.virtual_address = NULL;
      p_dec_out->pictures[i].output_picture_cb_cr.bus_address = 0;
      p_dec_out->pictures[i].output_picture_cr.virtual_address = NULL;
      p_dec_out->pictures[i].output_picture_cr.bus_address = 0;
      p_dec_out->pictures[i].output_width = 0;
      p_dec_out->pictures[i].output_height = 0;
      p_dec_out->pictures[i].display_width = 0;
      p_dec_out->pictures[i].display_height = 0;
      p_dec_out->pictures[i].output_width_thumb = 0;
      p_dec_out->pictures[i].output_height_thumb = 0;
      p_dec_out->pictures[i].display_width_thumb = 0;
      p_dec_out->pictures[i].display_height_thumb = 0;
      p_dec_out->pictures[i].pic_stride = 0;
      p_dec_out->pictures[i].pic_stride_ch = 0;
    }
  } else {
    dec_cont->image.header_ready = 0;
  }

  dec_cont->stream_consumed_callback.p_strm_buff = (u8 *)p_dec_in->stream_buffer.virtual_address;
  dec_cont->stream_consumed_callback.p_user_data = p_dec_in->p_user_data;
  /* set mcu/slice value */
  dec_cont->info.slice_mb_set_value = p_dec_in->slice_mb_set;

  /* check HW supported features */
  if(!dec_cont->is8190) {
    /* return if not valid HW and unsupported operation type */
    if(dec_cont->info.operation_type == JPEGDEC_NONINTERLEAVED ||
        dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: Operation type not supported");
      return (JPEGDEC_UNSUPPORTED);
    }

    if(dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr400 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr440 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr444_SEMIPLANAR)
      mcu_size_divider = 2;
    else
      mcu_size_divider = 1;

    /* check slice config */
    if((p_dec_in->slice_mb_set * (JPG_FRM.num_mcu_in_row / mcu_size_divider)) >
        dec_cont->max_supported_slice_size) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: slice_mb_set  > JPEGDEC_MAX_SLICE_SIZE");
      return (JPEGDEC_PARAM_ERROR);
    }

    /* check frame size */
    if((!p_dec_in->slice_mb_set) &&
        JPG_FRM.num_mcu_in_frame > dec_cont->max_supported_slice_size) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: mcu_in_frame > JPEGDEC_MAX_SLICE_SIZE");
      return (JPEGDEC_PARAM_ERROR);
    }
  } else {
    /* check if fuse was burned */
    if(dec_cont->fuse_burned) {
      /* return if not valid HW and unsupported operation type */
      if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
        JPEGDEC_API_TRC
        ("JpegDecDecode# ERROR: Operation type not supported");
        return (JPEGDEC_UNSUPPORTED);
      }
    }

    /* check slice config */
    if((p_dec_in->slice_mb_set && p_dec_in->dec_image_type == JPEGDEC_IMAGE &&
        dec_cont->info.operation_type != JPEGDEC_BASELINE) ||
        (p_dec_in->slice_mb_set && p_dec_in->dec_image_type == JPEGDEC_THUMBNAIL &&
         dec_cont->info.operation_type_thumb != JPEGDEC_BASELINE)) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: Slice mode not supported for this operation type");
      return (JPEGDEC_SLICE_MODE_UNSUPPORTED);
    }

#ifdef SLICE_MODE_LARGE_PIC
    /* check if frame size over 16M */
    if((!p_dec_in->slice_mb_set) &&
        ((JPG_FRM.hw_x * JPG_FRM.hw_y) > dec->hw_feature.img_max_dec_width*
          dec->hw_feature.img_max_dec_height)) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: Resolution > 16M ==> use slice mode!");
      return (JPEGDEC_PARAM_ERROR);
    }
#endif

    if(dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr400 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr440 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr444_SEMIPLANAR)
      mcu_size_divider = 2;
    else
      mcu_size_divider = 1;

    /* check slice config */
    if((p_dec_in->slice_mb_set * (JPG_FRM.num_mcu_in_row / mcu_size_divider)) >
        dec_cont->max_supported_slice_size) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: slice_mb_set  > JPEGDEC_MAX_SLICE_SIZE");
      return (JPEGDEC_PARAM_ERROR);
    }
  }

  if(p_dec_in->slice_mb_set > 255) {
    JPEGDEC_API_TRC
    ("JpegDecDecode# ERROR: slice_mb_set  > Maximum slice size");
    return (JPEGDEC_PARAM_ERROR);
  }

  /* check slice size */
  if(p_dec_in->slice_mb_set &&
      !dec_cont->info.SliceReadyForPause && !dec_cont->info.input_buffer_empty) {
    if(dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr400 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr440 ||
        dec_cont->info.get_info_ycb_cr_mode == JPEGDEC_YCbCr444_SEMIPLANAR)
      mcu_size_divider = 2;
    else
      mcu_size_divider = 1;

    if((p_dec_in->slice_mb_set * (JPG_FRM.num_mcu_in_row / mcu_size_divider)) >
        JPG_FRM.num_mcu_in_frame) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# ERROR: (slice_mb_set * Number of MCU's in row) > Number of MCU's in frame");
      return (JPEGDEC_PARAM_ERROR);
    }
  }

  /* Handle stream/hw parameters after buffer empty */
  if(dec_cont->info.input_buffer_empty) {
    /* Store the stream parameters */
    dec_cont->stream.bit_pos_in_byte = 0;
    dec_cont->stream.p_curr_pos = (u8 *) p_dec_in->stream_buffer.virtual_address;
    dec_cont->stream.stream_bus = p_dec_in->stream_buffer.bus_address;
    dec_cont->stream.p_start_of_stream =
      (u8 *) p_dec_in->stream_buffer.virtual_address;

    /* update hw parameters */
    dec_cont->info.input_streaming = 1;
    if(p_dec_in->buffer_size)
      dec_cont->info.input_buffer_len = p_dec_in->buffer_size;
    else
      dec_cont->info.input_buffer_len = p_dec_in->stream_length;

    /* decoded stream */
    dec_cont->info.decoded_stream_len += dec_cont->info.input_buffer_len;

    if(dec_cont->info.decoded_stream_len > p_dec_in->stream_length) {
      JPEGDEC_API_TRC
      ("JpegDecDecode# Error: All input stream already processed");
      return JPEGDEC_STRM_ERROR;
    }
  }

  /* update user allocated output */
  dec_cont->info.given_out_luma.virtual_address =
    p_dec_in->picture_buffer_y.virtual_address;
  dec_cont->info.given_out_luma.bus_address = p_dec_in->picture_buffer_y.bus_address;
  dec_cont->info.given_out_chroma.virtual_address =
    p_dec_in->picture_buffer_cb_cr.virtual_address;
  dec_cont->info.given_out_chroma.bus_address =
    p_dec_in->picture_buffer_cb_cr.bus_address;
  dec_cont->info.given_out_chroma2.virtual_address =
    p_dec_in->picture_buffer_cr.virtual_address;
  dec_cont->info.given_out_chroma2.bus_address =
    p_dec_in->picture_buffer_cr.bus_address;

  /* check if input streaming used */
  if(!dec_cont->info.SliceReadyForPause &&
      !dec_cont->info.input_buffer_empty && p_dec_in->buffer_size) {
    dec_cont->info.input_streaming = 1;
    dec_cont->info.input_buffer_len = p_dec_in->buffer_size;
    dec_cont->info.decoded_stream_len += dec_cont->info.input_buffer_len;
  }

  /* if use Pre-added Buffer mode */
  if(p_dec_in->picture_buffer_y.virtual_address == NULL) {
    if (dec_cont->ext_buffer_num == 0)
      return (JPEGDEC_WAITING_FOR_BUFFER);

    if (dec_cont->info.operation_type == JPEGDEC_BASELINE ||
       (dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE && dec_cont->scan_num == 0)) {
      if (!dec_cont->realloc_buffer) {
        pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
        if (pp_buffer == NULL) {
          if (dec_cont->abort)
            return JPEGDEC_ABORTED;
          else
            return JPEGDEC_NO_DECODING_BUFFER;
        }

        /* reallocate buffer */
        if (pp_buffer->size < dec_cont->next_buf_size) {
          for (i = 0; i < 16; i++) {
            if (pp_buffer->virtual_address == dec_cont->ext_buffers[i].virtual_address)
              break;
          }
          ASSERT(i < 16);
          dec_cont->buffer_queue_index = InputQueueFindBufferId(dec_cont->pp_buffer_queue, pp_buffer->virtual_address);
          dec_cont->buffer_index = i;
          dec_cont->realloc_buffer = 1;
          dec_cont->buf_to_free = &dec_cont->ext_buffers[i];
          dec_cont->buf_num = 1;
          return JPEGDEC_WAITING_FOR_BUFFER;
        }

        dec_cont->asic_buff.pp_luma_buffer = *pp_buffer;
      } else {
        if (dec_cont->ext_buffers[dec_cont->buffer_index].size < dec_cont->next_buf_size) {
          dec_cont->buf_to_free = &dec_cont->ext_buffers[dec_cont->buffer_index];
          dec_cont->buf_num = 1;
          return JPEGDEC_WAITING_FOR_BUFFER;
        }
        dec_cont->realloc_buffer = 0;
        dec_cont->asic_buff.pp_luma_buffer = dec_cont->ext_buffers[dec_cont->buffer_index];
      }

      u32 pp_width = 0, pp_height_luma = 0, pp_stride = 0, pp_buff_size = 0;
      PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
      if (dec_cont->info.slice_mb_set_value) {
        if (ppu_cfg->tiled_e) {
          pp_width = dec_cont->info.X >> dec_cont->dscale_shift_x;
          pp_height_luma = NEXT_MULTIPLE(dec_cont->info.slice_mb_set_value * 16 >>
                             dec_cont->dscale_shift_y, 4) / 4;
          pp_stride = NEXT_MULTIPLE(4 * pp_width, ALIGN(ppu_cfg->align));
        } else {
          pp_width = dec_cont->info.X >> dec_cont->dscale_shift_x;
          pp_height_luma = dec_cont->info.slice_mb_set_value * 16 >>
                             dec_cont->dscale_shift_y;
          pp_stride = NEXT_MULTIPLE(pp_width, ALIGN(ppu_cfg->align));
          pp_buff_size = pp_stride * pp_height_luma;
        }
      }
      if (dec_cont->info.slice_mb_set_value) {
        dec_cont->asic_buff.pp_chroma_buffer.virtual_address =
          dec_cont->asic_buff.pp_luma_buffer.virtual_address + pp_buff_size/4;
        dec_cont->asic_buff.pp_chroma_buffer.bus_address =
          dec_cont->asic_buff.pp_luma_buffer.bus_address + pp_buff_size;
      } else {
        dec_cont->asic_buff.pp_chroma_buffer.virtual_address =
          dec_cont->asic_buff.pp_luma_buffer.virtual_address;
        dec_cont->asic_buff.pp_chroma_buffer.bus_address =
          dec_cont->asic_buff.pp_luma_buffer.bus_address;
      }
    }
  } else {
    if (p_dec_in->picture_buffer_y.size < dec_cont->next_buf_size)
      return JPEGDEC_PARAM_ERROR;
  }


  /* find markers and go ! */
  do {
    /* if slice mode/slice done return to hw handling */
    if(dec_cont->image.header_ready && dec_cont->info.SliceReadyForPause)
      break;

    /* Look for marker prefix byte from stream */
    marker_byte = JpegDecGetByte(&(dec_cont->stream));
    if (marker_byte == 0xFF) {
      current_byte = JpegDecGetByte(&(dec_cont->stream));
      while (current_byte == 0xFF)
        current_byte = JpegDecGetByte(&(dec_cont->stream));

      /* switch to certain header decoding */
      switch (current_byte) {
      case 0x00:
        break;
      case SOF0:
      case SOF2:
        JPEGDEC_API_TRC("JpegDecDecode# JpegDecDecodeFrameHdr decode");
        /* Baseline/Progressive */
        dec_cont->frame.coding_type = current_byte;
        /* Set operation type */
        if(dec_cont->frame.coding_type == SOF0)
          dec_cont->info.operation_type = JPEGDEC_BASELINE;
        else
          dec_cont->info.operation_type = JPEGDEC_PROGRESSIVE;

        ret_code = JpegDecDecodeFrameHdr(dec_cont);
        if(ret_code != JPEGDEC_OK) {
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            goto end;
          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# ERROR: JpegDecDecodeFrameHdr err");
            goto end;
          }
        }
        ret_code = JpegDecSetInfo_INTERNAL(dec_cont);
        if (ret_code != JPEGDEC_OK) {
          JPEGDEC_API_TRC
          ("JpegDecDecode# JpegDecSetInfo_INTERNAL err\n");
          goto end;
        }
        break;
      /* Start of Scan */
      case SOS:
        /* reset image ready */
        dec_cont->image.image_ready = 0;
        dec_cont->scan_num++;
        JPEGDEC_API_TRC("JpegDecDecode# JpegDecDecodeScan dec");

        ret_code = JpegDecDecodeScan(dec_cont);
        if(ret_code != JPEGDEC_OK) {
          JpegDecClearStructs(dec_inst, 1);
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            goto end;
          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# JpegDecDecodeScan err\n");
            goto end;
          }
        }
        dec_cont->image.header_ready = 1;

        if(dec_cont->stream.bit_pos_in_byte) {
          /* delete stuffing bits */
          current_byte = (8 - dec_cont->stream.bit_pos_in_byte);
          if(JpegDecFlushBits
              (&(dec_cont->stream),
               8 - dec_cont->stream.bit_pos_in_byte) == STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            ret_code = JPEGDEC_STRM_ERROR;
            goto end;
          }
        }
        JPEGDEC_API_TRC("JpegDecDecode# Stuffing bits deleted\n");
        break;
      /* Start of Huffman tables */
      case DHT:
        JPEGDEC_API_TRC
        ("JpegDecDecode# JpegDecDecodeHuffmanTables dec");
        ret_code = JpegDecDecodeHuffmanTables(dec_cont);
        JPEGDEC_API_TRC
        ("JpegDecDecode# JpegDecDecodeHuffmanTables stops");
        if(ret_code != JPEGDEC_OK) {
          JpegDecClearStructs(dec_inst, 1);
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            goto end;
          } else if (ret_code == JPEGDEC_UNSUPPORTED){
            goto end;
          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# ERROR: JpegDecDecodeHuffmanTables err");
            goto end;
          }
        }
        /* mark DHT got from stream */
        DHTfromStream = 1;
        break;
      /* start of Quantisation Tables */
      case DQT:
        JPEGDEC_API_TRC("JpegDecDecode# JpegDecDecodeQuantTables dec");
        ret_code = JpegDecDecodeQuantTables(dec_cont);
        if(ret_code != JPEGDEC_OK) {
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            goto end;
          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# ERROR: JpegDecDecodeQuantTables err");
            goto end;
          }
        }
        break;
      /* Start of Image */
      case SOI:
        /* no actions needed, continue */
        break;
      /* End of Image */
      case EOI:
        if(dec_cont->image.image_ready) {
          JPEGDEC_API_TRC("JpegDecDecode# EOI: OK\n");
          return (JPEGDEC_FRAME_READY);
        } else {
          JPEGDEC_API_TRC("JpegDecDecode# ERROR: EOI: NOK\n");
          ret_code = JPEGDEC_ERROR;
          goto end;
        }
      /* Define Restart Interval */
      case DRI:
        JPEGDEC_API_TRC("JpegDecDecode# DRI");
        current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
        if(current_bytes == STRM_ERROR) {
          JPEGDEC_API_TRC("JpegDecDecode# ERROR: Read bits ");
          ret_code = JPEGDEC_STRM_ERROR;
          goto end;
        }
        dec_cont->frame.Ri = JpegDecGet2Bytes(&(dec_cont->stream));
        if (dec_cont->frame.Ri == 0)
          break;
        dec_cont->frame.intervals = (dec_cont->frame.num_mcu_in_frame +
                                   dec_cont->frame.Ri - 1) / dec_cont->frame.Ri;
        /* TODO: Add a common API to determined whether restart interval based
           multicore is allowed. */
        if (!dec_cont->allow_sw_ri_parse &&
            (p_dec_in->ri_count <=1 || !p_dec_in->ri_array))
          dec_cont->ri_mc_enabled = RI_MC_DISABLED_BY_USER;
        if (p_dec_in->ri_count > 1 &&
            p_dec_in->ri_count != dec_cont->frame.intervals) {
          JPEGDEC_API_TRC("JpegDecDecode# ERROR: input ri_count");
          ret_code = JPEGDEC_PARAM_ERROR;
          goto end;
        }
        if ((dec_cont->frame.Ri % dec_cont->frame.num_mcu_in_row == 0) &&
            dec_cont->n_cores_available > 1 &&
            !RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          FrameInfo *frame = &dec_cont->frame;
          dec_cont->ri_mc_enabled = RI_MC_ENA;
          sem_init(&dec_cont->sem_ri_mc_done, 0, 0);
          frame->num_mcu_rows_in_interval = frame->Ri / frame->num_mcu_in_row;
          frame->num_mcu_rows_in_frame = frame->num_mcu_in_frame / frame->num_mcu_in_row;
          frame->intervals = (frame->num_mcu_in_frame + frame->Ri - 1) / frame->Ri;
        }
        break;
      /* Restart with modulo 8 count m */
      case RST0:
      case RST1:
      case RST2:
      case RST3:
      case RST4:
      case RST5:
      case RST6:
      case RST7:
        /* initialisation of DC predictors to zero value !!! */
        for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
          dec_cont->scan.pred[i] = 0;
        }
        JPEGDEC_API_TRC("JpegDecDecode# DC predictors init");
        break;
      /* unsupported features */
      case SOF1:
      case SOF3:
      case SOF5:
      case SOF6:
      case SOF7:
      case SOF9:
      case SOF10:
      case SOF11:
      case SOF13:
      case SOF14:
      case SOF15:
      case DAC:
      case DHP:
      case TEM:
        JPEGDEC_API_TRC("JpegDecDecode# ERROR: Unsupported Features");
        ret_code = JPEGDEC_UNSUPPORTED;
        goto end;
      /* application data & comments */
      case APP0:
        JPEGDEC_API_TRC("JpegDecDecode# APP0 in Decode");
        /* APP0 Extended Thumbnail */
        if(p_dec_in->dec_image_type == JPEGDEC_THUMBNAIL) {
          /* reset */
          app_bits = 0;
          app_length = 0;

          /* length */
          app_length = JpegDecGet2Bytes(&(dec_cont->stream));
          app_bits += 16;

          /* length > 2 */
          if(app_length < 2)
            break;

          /* check identifier */
          current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
          app_bits += 16;
          if(current_bytes != 0x4A46) {
            dec_cont->stream.appn_flag = 1;
            if(JpegDecFlushBits
                (&(dec_cont->stream),
                 ((app_length * 8) - app_bits)) == STRM_ERROR) {
              JPEGDEC_API_TRC
              ("JpegDecDecode# ERROR: Stream error");
              ret_code = JPEGDEC_STRM_ERROR;
              goto end;
            }
            dec_cont->stream.appn_flag = 0;
            break;
          }
          current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
          app_bits += 16;
          if(current_bytes != 0x5858) {
            dec_cont->stream.appn_flag = 1;
            if(JpegDecFlushBits
                (&(dec_cont->stream),
                 ((app_length * 8) - app_bits)) == STRM_ERROR) {
              JPEGDEC_API_TRC
              ("JpegDecDecode# ERROR: Stream error");
              ret_code = JPEGDEC_STRM_ERROR;
              goto end;
            }
            dec_cont->stream.appn_flag = 0;
            break;
          }
          current_byte = JpegDecGetByte(&(dec_cont->stream));
          app_bits += 8;
          if(current_byte != 0x00) {
            dec_cont->stream.appn_flag = 1;
            if(JpegDecFlushBits
                (&(dec_cont->stream),
                 ((app_length * 8) - app_bits)) == STRM_ERROR) {
              JPEGDEC_API_TRC
              ("JpegDecDecode# ERROR: Stream error");
              ret_code = JPEGDEC_STRM_ERROR;
              goto end;
            }
            dec_cont->stream.appn_flag = 0;
            break;
          }
          /* extension code */
          current_byte = JpegDecGetByte(&(dec_cont->stream));
          dec_cont->stream.appn_flag = 0;
          if(current_byte != JPEGDEC_THUMBNAIL_JPEG) {
            JPEGDEC_API_TRC(("JpegDecDecode# ERROR: thumbnail unsupported"));
            ret_code = JPEGDEC_UNSUPPORTED;
            goto end;
          }
          /* thumbnail mode */
          JPEGDEC_API_TRC("JpegDecDecode# Thumbnail data ok!");
          dec_cont->stream.thumbnail = 1;
          break;
        } else {
          /* Flush unsupported thumbnail */
          current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));

          /* length > 2 */
          if(current_bytes < 2)
            break;

          dec_cont->stream.appn_flag = 1;
          if(JpegDecFlushBits
              (&(dec_cont->stream),
               ((current_bytes - 2) * 8)) == STRM_ERROR) {
            JPEGDEC_API_TRC("JpegDecDecode# ERROR: Stream error");
            ret_code = JPEGDEC_STRM_ERROR;
            goto end;
          }
          dec_cont->stream.appn_flag = 0;
          break;
        }
      case DNL:
        JPEGDEC_API_TRC("JpegDecDecode# DNL ==> flush");
        break;
      case APP1:
      case APP2:
      case APP3:
      case APP4:
      case APP5:
      case APP6:
      case APP7:
      case APP8:
      case APP9:
      case APP10:
      case APP11:
      case APP12:
      case APP13:
      case APP14:
      case APP15:
      case COM:
        JPEGDEC_API_TRC("JpegDecDecode# COM");
        current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
        if(current_bytes == STRM_ERROR) {
          JPEGDEC_API_TRC("JpegDecDecode# ERROR: Read bits ");
          ret_code = JPEGDEC_STRM_ERROR;
          goto end;
        }
        /* length > 2 */
        if(current_bytes < 2)
          break;
        /* jump over not supported header */
        if(current_bytes != 0) {
          dec_cont->stream.read_bits += ((current_bytes * 8) - 16);
          dec_cont->stream.p_curr_pos +=
            (((current_bytes * 8) - 16) / 8);
        }
        break;
      default:
        break;
      }
    } else {
      if(marker_byte == 0xFFFFFFFF) {
        break;
      }
    }

    if(dec_cont->image.header_ready)
      break;
  } while((dec_cont->stream.read_bits >> 3) <= dec_cont->stream.stream_length);

  ret_code = JPEGDEC_OK;

  /* check if no DHT in stream and if already loaded (MJPEG) */
  if(!DHTfromStream && !dec_cont->vlc.default_tables) {
    JPEGDEC_API_TRC("JpegDecDecode# No DHT tables in stream, use tables defined in JPEG Standard\n");
    /* use default tables defined in standard */
    JpegDecDefaultHuffmanTables(dec_cont);
  }

  /* Handle decoded image here */
  if(dec_cont->image.header_ready) {
    if (!dec_cont->frame.intervals)
      dec_cont->ri_mc_enabled = RI_MC_DISABLED_BY_RI;
    if (RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
      dec_cont->frame.ri_array = (u8 **)DWLmalloc(dec_cont->frame.intervals * sizeof(u8*));
      ASSERT(dec_cont->frame.ri_array);
      if (p_dec_in->ri_count > 1 && p_dec_in->ri_array) {
        for (i = 0; i < p_dec_in->ri_count; i++) {
          if (p_dec_in->ri_array[i])
            dec_cont->frame.ri_array[i] = dec_cont->stream.p_start_of_stream +
                                          p_dec_in->ri_array[i];
          else {
            /* Missing restart interval. */
            dec_cont->frame.ri_array[i] = NULL;
          }
        }
      } else if (dec_cont->allow_sw_ri_parse) {
        JpegParseRST(dec_cont, dec_cont->stream.p_start_of_stream,
                    dec_cont->stream.stream_length,
                    dec_cont->frame.ri_array);
      }
      dec_cont->ri_running_cores = 0;
      dec_cont->ri_index = dec_cont->first_ri_index;
    }
    /* loop until decoding control should return for user */
    do {
      /* if pp enabled ==> set pp control */
      if(dec_cont->pp_instance != NULL) {
        dec_cont->pp_config_query.tiled_mode = 0;
        dec_cont->PPConfigQuery(dec_cont->pp_instance,
                                &dec_cont->pp_config_query);

        dec_cont->pp_control.use_pipeline =
          dec_cont->pp_config_query.pipeline_accepted;

        /* set pp for combined mode */
        if(dec_cont->pp_control.use_pipeline)
          JpegDecPreparePp(dec_cont);
      }

      /* check if we had to load imput buffer or not */
      if(!dec_cont->info.input_buffer_empty) {
        /* if slice mode ==> set slice height */
        if(dec_cont->info.slice_mb_set_value &&
            dec_cont->pp_control.use_pipeline == 0) {
          JpegDecSliceSizeCalculation(dec_cont);
        }

        /* Start HW or continue after pause */
        if(!dec_cont->info.SliceReadyForPause) {
          if(!dec_cont->info.progressive_scan_ready ||
              dec_cont->info.non_interleaved_scan_ready) {
            JPEGDEC_API_TRC("JpegDecDecode# Start HW init\n");
            if (dec_cont->ri_index && RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
              dec_cont->stream.p_curr_pos = dec_cont->frame.ri_array[dec_cont->ri_index];
              dec_cont->stream.bit_pos_in_byte = 0;
              if (!dec_cont->stream.p_curr_pos) {
                /* Missing restart interval. */
                dec_cont->ri_index++;
                continue;
              }
            }
            ret_code = JpegDecInitHW(dec_cont);
            dec_cont->info.non_interleaved_scan_ready = 0;
            if(ret_code != JPEGDEC_OK) {
              /* return JPEGDEC_HW_RESERVED */
              goto end;
            }

          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# Continue HW decoding after progressive scan ready\n");
            JpegDecInitHWProgressiveContinue(dec_cont);
            dec_cont->info.progressive_scan_ready = 0;

          }
        } else {
          JPEGDEC_API_TRC
          ("JpegDecDecode# Continue HW decoding after slice ready\n");
          JpegDecInitHWContinue(dec_cont);
        }

        dec_cont->info.SliceCount++;
      } else {
        JPEGDEC_API_TRC
        ("JpegDecDecode# Continue HW decoding after input buffer has been loaded\n");
        JpegDecInitHWInputBuffLoad(dec_cont);

        /* buffer loaded ==> reset flag */
        dec_cont->info.input_buffer_empty = 0;
      }

      if (dec_cont->b_mc || RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
        /* reset shadow HW status reg values so that we dont end up writing
         * some garbage to next core regs */
        JpegRefreshRegs(dec_cont);
        dec_cont->jpeg_regs[0] = asic_id;
        SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_E, 0);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ_STAT, 0);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ, 0);
        if (RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          JpegMCSetHwRdyCallback(dec_cont);
          dec_cont->asic_running = 0;
          dwlret = DWL_HW_WAIT_OK;
          asic_status = JPEGDEC_X170_IRQ_DEC_RDY;
        } else {
          JpegRiMCSetHwRdyCallback(dec_cont);
          dwlret = DWL_HW_WAIT_OK;
          asic_status = JPEGDEC_X170_IRQ_DEC_RDY;
        }
      } else {
#ifdef JPEGDEC_PERFORMANCE
        dwlret = DWL_HW_WAIT_OK;
#else
        /* wait hw ready */
        if (!dec_cont->vcmd_used)
          dwlret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                                  dec_cont->info.timeout);
        else
          dwlret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmd_buf_id);

#endif /* #ifdef JPEGDEC_PERFORMANCE */

        /* Refresh regs */
        JpegRefreshRegs(dec_cont);
      }

      if(dwlret == DWL_HW_WAIT_OK) {
        JPEGDEC_API_TRC("JpegDecDecode# DWL_HW_WAIT_OK");

#ifdef JPEGDEC_ASIC_TRACE
        {
          JPEGDEC_TRACE_INTERNAL(("\nJpeg_dec_decode# AFTER DWL_HW_WAIT_OK\n"));
          PrintJPEGReg(dec_cont->jpeg_regs);
        }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */
        /* check && reset status */
        if (!dec_cont->b_mc && RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          asic_status =
            GetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ_STAT);
        }

        if(asic_status & JPEGDEC_X170_IRQ_BUS_ERROR) {
          /* check PP status... and go */
          if(dec_cont->pp_instance != NULL) {
            /* End PP co-operation */
            if(dec_cont->pp_control.pp_status == DECPP_RUNNING) {
              JPEGDEC_API_TRC("JpegDecDecode# PP END CALL");
              dec_cont->PPEndCallback(dec_cont->pp_instance);
              dec_cont->pp_control.pp_status = DECPP_PIC_READY;
            }
          }

          JPEGDEC_API_TRC
          ("JpegDecDecode# JPEGDEC_X170_IRQ_BUS_ERROR");
          /* clear interrupts */
          JPEGDEC_CLEAR_IRQ;

          SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_E, 0);
          DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                       dec_cont->jpeg_regs[1]);

          /* Release HW */
          (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);

          /* update asic_running */
          dec_cont->asic_running = 0;

          ret_code = JPEGDEC_HW_BUS_ERROR;
          goto end;
        } else if(asic_status & JPEGDEC_X170_IRQ_STREAM_ERROR ||
                  asic_status & JPEGDEC_X170_IRQ_TIMEOUT ||
                  asic_status & DEC_8190_IRQ_ABORT ) {
          /* check PP status... and go */
          if(dec_cont->pp_instance != NULL) {
            /* End PP co-operation */
            if(dec_cont->pp_control.pp_status == DECPP_RUNNING) {
              JPEGDEC_API_TRC("JpegDecDecode# PP END CALL");
              dec_cont->PPEndCallback(dec_cont->pp_instance);
            }

            dec_cont->pp_control.pp_status = DECPP_PIC_READY;
          }

          if(asic_status & JPEGDEC_X170_IRQ_STREAM_ERROR) {
            JPEGDEC_API_TRC
            ("JpegDecDecode# JPEGDEC_X170_IRQ_STREAM_ERROR");
          } else if(asic_status & JPEGDEC_X170_IRQ_TIMEOUT) {
            JPEGDEC_API_TRC
            ("JpegDecDecode# JPEGDEC_X170_IRQ_TIMEOUT");
          } else {
            JPEGDEC_API_TRC
            ("JpegDecDecode# JPEGDEC_X170_IRQ_ABORT");
          }

          /* clear interrupts */
          JPEGDEC_CLEAR_IRQ;

          SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_E, 0);
          /* Release HW */
          if (!dec_cont->vcmd_used) {
            DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                         dec_cont->jpeg_regs[1]);

            (void)DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
          } else
            DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);

          /* update asic_running */
          dec_cont->asic_running = 0;

          if ((asic_status & JPEGDEC_X170_IRQ_TIMEOUT) &&
              dec_cont->vcmd_used &&
              dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE &&
              !dec_cont->last_scan) {
            /* For progressive jpeg, when vcmd is used, timeout may be the last
               scan if it has not been detected by sw. */
            int i;
            for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
              dec_cont->ppu_cfg[i].enabled = dec_cont->ppu_enabled_orig[i];
            }
            dec_cont->info.progressive_scan_ready = 1;
            dec_cont->last_scan = 1;
            JpegDecInitHWEmptyScan(dec_cont, 0);
            continue;
          }

          /* output set */
          if(dec_cont->pp_instance == NULL) {
            if (dec_cont->info.user_alloc_mem ||
                (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.virtual_address =
                  (u32*)((u8*)dec_cont->info.out_luma.virtual_address + ppu_cfg->luma_offset);
              }
            }
            else {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.virtual_address =
                (u32*)((addr_t)dec_cont->info.out_pp_luma.virtual_address + ppu_cfg->luma_offset);
              }
            }

            /* output set */
            if (dec_cont->info.user_alloc_mem ||
                (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.bus_address =
                  dec_cont->info.out_luma.bus_address + ppu_cfg->luma_offset;
              }
            }
            else {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.bus_address =
                  dec_cont->info.out_pp_luma.bus_address + ppu_cfg->luma_offset;
#ifdef SUPPORT_DEC400
                if(tile_status_bus_address == 0) {
                  tile_status_virtual_address = p_dec_out->pictures[i].output_picture_y.virtual_address;
                  tile_status_bus_address = p_dec_out->pictures[i].output_picture_y.bus_address;
                }
#endif
              }
            }

            /* if not grayscale */
            if(dec_cont->image.size_chroma || (dec_cont->pp_enabled && !(dec_cont->info.user_alloc_mem || (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)))) {
              if (dec_cont->info.user_alloc_mem ||
                  (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                    (u32*)((u8*)dec_cont->info.out_chroma.virtual_address + ppu_cfg->chroma_offset);
                }
              }
              else {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                   p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                   (u32*)((addr_t)dec_cont->info.out_pp_chroma.virtual_address + ppu_cfg->chroma_offset);
                }
              }

              if (dec_cont->info.user_alloc_mem ||
                  (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                    dec_cont->info.out_chroma.bus_address + ppu_cfg->chroma_offset;
                }
              }
              else {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                   p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                     dec_cont->info.out_pp_chroma.bus_address + ppu_cfg->chroma_offset;
                }
              }

              p_dec_out->pictures[0].output_picture_cr.virtual_address =
                dec_cont->info.out_chroma2.virtual_address;
              p_dec_out->pictures[0].output_picture_cr.bus_address =
                dec_cont->info.out_chroma2.bus_address;
            }
            // if(dec_cont->pp_enabled)
            {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                if (!p_dec_in->dec_image_type) {
                  if (dec_cont->ppu_cfg[i].crop2.enabled) {
                    p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                    p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].crop2.height;
                    p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].crop2.width;
                    p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].crop2.height;
                  } else {
                    p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                    p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].scale.height;
                    p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                    p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].scale.height;
                  }
                  p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                  p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                  CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                } else {
                  if (ppu_cfg->crop2.enabled) {
                    p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                    p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                    p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].crop2.width;
                    p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                  } else {
                    p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                    p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                    p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                    p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                  }
                  p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                  p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                  CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                }
              }
            }
#ifdef SUPPORT_DEC400
            u32 luma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
            u32 chroma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
            PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
            tile_status_address_offset = 0;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
              if (!ppu_cfg->enabled) continue;
              if (ppu_cfg->tiled_e) {
                tile_status_address_offset += ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                luma_size[i] = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                if (!ppu_cfg->monochrome) {
                  tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                  chroma_size[i] = ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                }
              } else if (ppu_cfg->planar) {
                tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                luma_size[i] = ppu_cfg->ystride * ppu_cfg->scale.height;
                if (!ppu_cfg->monochrome) {
                  tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
                  chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height;
                }
              } else {
                tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                luma_size[i] =  ppu_cfg->ystride * ppu_cfg->scale.height;
                if (!ppu_cfg->monochrome) {
                  tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                  chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                }
              }
            }
            ppu_cfg = dec_cont->ppu_cfg;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
              if (!ppu_cfg->enabled) continue;
              p_dec_out->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
              p_dec_out->pictures[i].luma_table.size = NEXT_MULTIPLE((luma_size[i] / 256 * 4 + 7) / 8, 16);
              if (!ppu_cfg->monochrome) {
                p_dec_out->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
                p_dec_out->pictures[i].chroma_table.size = NEXT_MULTIPLE((chroma_size[i] / 256 * 4 + 7) / 8, 16);
              }
            }
#endif
          }

          if(dec_cont->b_mc ||
             (p_dec_in->picture_buffer_y.virtual_address == NULL &&
             ((dec_cont->ri_mc_enabled == RI_MC_ENA &&
             dec_cont->ri_index == dec_cont->last_ri_index) ||
             dec_cont->ri_mc_enabled != RI_MC_ENA)))
            PushOutputPic(&dec_cont->fb_list, p_dec_out, &(dec_cont->p_image_info));

          JpegDecClearStructs(dec_inst, 1);
          return (JPEGDEC_STRM_ERROR);
        } else if(asic_status & JPEGDEC_X170_IRQ_BUFFER_EMPTY) {
          /* check if frame is ready */
          if(!(asic_status & JPEGDEC_X170_IRQ_DEC_RDY)) {
            JPEGDEC_API_TRC
            ("JpegDecDecode# JPEGDEC_X170_IRQ_BUFFER_EMPTY/STREAM PROCESSED");

            /* clear interrupts */
            SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_BUFFER_INT,
                           0);

            /* flag to load buffer */
            dec_cont->info.input_buffer_empty = 1;

            /* check if all stream should be processed with the
             * next buffer ==> may affect to some API checks */
            if((dec_cont->info.decoded_stream_len +
                dec_cont->info.input_buffer_len) >=
                dec_cont->stream.stream_length) {
              dec_cont->info.stream_end_flag = 1;
            }

            /* output set */
            if(dec_cont->pp_instance == NULL) {
              if (dec_cont->info.user_alloc_mem ||
                  (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_y.virtual_address =
                    (u32*)((u8*)dec_cont->info.out_luma.virtual_address + ppu_cfg->luma_offset);
                }
              }
              else {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_y.virtual_address =
                  (u32*)((addr_t)dec_cont->info.out_pp_luma.virtual_address + ppu_cfg->luma_offset);
                }
              }

              /* output set */
              if (dec_cont->info.user_alloc_mem ||
                  (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_y.bus_address =
                    dec_cont->info.out_luma.bus_address + ppu_cfg->luma_offset;
                }
              }
              else {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].output_picture_y.bus_address =
                    dec_cont->info.out_pp_luma.bus_address + ppu_cfg->luma_offset;
#ifdef SUPPORT_DEC400
                  if(tile_status_bus_address == 0) {
                    tile_status_virtual_address = p_dec_out->pictures[i].output_picture_y.virtual_address;
                    tile_status_bus_address = p_dec_out->pictures[i].output_picture_y.bus_address;
                  }
#endif
                }
              }

              /* if not grayscale */
              if(dec_cont->image.size_chroma || (dec_cont->pp_enabled && !(dec_cont->info.user_alloc_mem || (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)))) {
                if (dec_cont->info.user_alloc_mem ||
                    (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                      (u32*)((u8*)dec_cont->info.out_chroma.virtual_address + ppu_cfg->chroma_offset);
                  }
                }
                else {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                     p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                     (u32*)((addr_t)dec_cont->info.out_pp_chroma.virtual_address + ppu_cfg->chroma_offset);
                  }
                }

                if (dec_cont->info.user_alloc_mem ||
                    (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                      dec_cont->info.out_chroma.bus_address + ppu_cfg->chroma_offset;
                  }
                }
                else {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                      dec_cont->info.out_pp_chroma.bus_address + ppu_cfg->chroma_offset;
                  }
                }

                p_dec_out->pictures[0].output_picture_cr.virtual_address =
                  dec_cont->info.out_chroma2.virtual_address;
                p_dec_out->pictures[0].output_picture_cr.bus_address =
                  dec_cont->info.out_chroma2.bus_address;
              }
              // if(dec_cont->pp_enabled)
              {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  if (!p_dec_in->dec_image_type) {
                    if (dec_cont->ppu_cfg[i].crop2.enabled) {
                      p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                      p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].crop2.height;
                      p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].crop2.width;
                      p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].crop2.height;
                    } else {
                      p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                      p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].scale.height;
                      p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                      p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].scale.height;
                    }
                    p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                    p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                    CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                  } else {
                    if (dec_cont->ppu_cfg[i].crop2.enabled) {
                      p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                      p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                      p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].crop2.width;
                      p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                    } else {
                      p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                      p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                      p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                      p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                    }
                    p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                    p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                    CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                  }
                }
              }
#ifdef SUPPORT_DEC400
              u32 luma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
              u32 chroma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              tile_status_address_offset = 0;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                if (ppu_cfg->tiled_e) {
                  tile_status_address_offset += ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                  luma_size[i] = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                  if (!ppu_cfg->monochrome) {
                    tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                    chroma_size[i] = ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                  }
                } else if (ppu_cfg->planar) {
                  tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                  luma_size[i] = ppu_cfg->ystride * ppu_cfg->scale.height;
                  if (!ppu_cfg->monochrome) {
                    tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
                    chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height;
                  }
                } else {
                  tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                  luma_size[i] =  ppu_cfg->ystride * ppu_cfg->scale.height;
                  if (!ppu_cfg->monochrome) {
                    tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                    chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                  }
                }
              }
              ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
                p_dec_out->pictures[i].luma_table.size = NEXT_MULTIPLE((luma_size[i] / 256 * 4 + 7) / 8, 16);
                if (!ppu_cfg->monochrome) {
                  p_dec_out->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
                  p_dec_out->pictures[i].chroma_table.size = NEXT_MULTIPLE((chroma_size[i] / 256 * 4 + 7) / 8, 16);
                }
              }
#endif
            }

            ret_code = JPEGDEC_STRM_PROCESSED;
            goto end;
          }
        }

        SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_RDY_INT, 0);
        HINTdec = GetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ);
        if(HINTdec) {
          JPEGDEC_API_TRC("JpegDecDecode# CLEAR interrupt");
          SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ, 0);
        }
        SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_IRQ_STAT, 0);

        /* check if slice ready */
        asic_slice_bit = GetDecRegister(dec_cont->jpeg_regs,
                                        HWIF_JPEG_SLICE_H);
        int_dec = GetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_SLICE_INT);

        /* slice ready ==> reset interrupt */
        if(asic_slice_bit && int_dec && !dec_cont->b_mc &&
           RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          /* if x170 pp not in use */
          if(dec_cont->pp_instance == NULL)
            dec_cont->info.SliceReadyForPause = 1;
          else
            dec_cont->info.SliceReadyForPause = 0;

          if(dec_cont->pp_instance != NULL &&
              !dec_cont->pp_control.use_pipeline) {
            dec_cont->info.SliceReadyForPause = 1;
          }

          /* if user allocated memory return given base */
          if(dec_cont->info.user_alloc_mem == 1 &&
              dec_cont->info.SliceReadyForPause == 1) {
            /* output addresses */
            p_dec_out->pictures[0].output_picture_y.virtual_address =
              dec_cont->info.given_out_luma.virtual_address;
            p_dec_out->pictures[0].output_picture_y.bus_address =
              dec_cont->info.given_out_luma.bus_address;
            if(dec_cont->image.size_chroma) {
              p_dec_out->pictures[0].output_picture_cb_cr.virtual_address =
                dec_cont->info.given_out_chroma.virtual_address;
              p_dec_out->pictures[0].output_picture_cb_cr.bus_address =
                dec_cont->info.given_out_chroma.bus_address;
              p_dec_out->pictures[0].output_picture_cr.virtual_address =
                dec_cont->info.given_out_chroma2.virtual_address;
              p_dec_out->pictures[0].output_picture_cr.bus_address =
                dec_cont->info.given_out_chroma2.bus_address;
            }
          }

          /* if not user allocated memory return slice base */
          if(dec_cont->info.user_alloc_mem == 0 &&
              dec_cont->info.SliceReadyForPause == 1) {
            /* output addresses */
            if (dec_cont->info.user_alloc_mem ||
                (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.virtual_address =
                  (u32*)((u8*)dec_cont->info.out_luma.virtual_address + ppu_cfg->luma_offset);
                p_dec_out->pictures[i].output_picture_y.bus_address =
                  dec_cont->info.out_luma.bus_address + ppu_cfg->luma_offset;
              }
            }
            else {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_y.virtual_address =
                (u32*)((addr_t)dec_cont->info.out_pp_luma.virtual_address + ppu_cfg->luma_offset);
                p_dec_out->pictures[i].output_picture_y.bus_address = dec_cont->info.out_pp_luma.bus_address + ppu_cfg->luma_offset;
#ifdef SUPPORT_DEC400
                if(tile_status_bus_address == 0) {
                  tile_status_virtual_address = p_dec_out->pictures[i].output_picture_y.virtual_address;
                  tile_status_bus_address = p_dec_out->pictures[i].output_picture_y.bus_address;
                }
#endif
              }
            }
            if(dec_cont->image.size_chroma || (dec_cont->pp_enabled && !(dec_cont->hw_feature.pp_version == G1_NATIVE_PP))) {
              if (!dec_cont->pp_enabled) {
                p_dec_out->pictures[0].output_picture_cb_cr.virtual_address =
                  dec_cont->info.out_chroma.virtual_address;
                p_dec_out->pictures[0].output_picture_cb_cr.bus_address =
                  dec_cont->info.out_chroma.bus_address;
              }
              else {
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                   p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                   (u32*)((addr_t)dec_cont->info.out_pp_chroma.virtual_address + ppu_cfg->chroma_offset);
                   p_dec_out->pictures[i].output_picture_cb_cr.bus_address = dec_cont->info.out_pp_chroma.bus_address + ppu_cfg->chroma_offset;
                }
#ifdef SUPPORT_DEC400
                u32 luma_size[5]={0,0,0,0,0};
                u32 chroma_size[5]={0,0,0,0,0};
                ppu_cfg = dec_cont->ppu_cfg;
                tile_status_address_offset = 0;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  if (ppu_cfg->tiled_e) {
                    tile_status_address_offset += ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                    luma_size[i] = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                      chroma_size[i] = ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                    }
                  } else if (ppu_cfg->planar) {
                    tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                    luma_size[i] = ppu_cfg->ystride * ppu_cfg->scale.height;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
                      chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height;
                    }
                  } else {
                    tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                    luma_size[i] =  ppu_cfg->ystride * ppu_cfg->scale.height;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                      chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                    }
                  }
                }
                ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
                  p_dec_out->pictures[i].luma_table.size = NEXT_MULTIPLE((luma_size[i] / 256 * 4 + 7) / 8, 16);
                  if (!ppu_cfg->monochrome) {
                    p_dec_out->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
                    p_dec_out->pictures[i].chroma_table.size = NEXT_MULTIPLE((chroma_size[i] / 256 * 4 + 7) / 8, 16);
                  }
                }
#endif
              }
              p_dec_out->pictures[0].output_picture_cr.virtual_address =
                dec_cont->info.out_chroma2.virtual_address;
              p_dec_out->pictures[0].output_picture_cr.bus_address =
                dec_cont->info.out_chroma2.bus_address;
            }
          }

          /* No slice output in case decoder + PP (no pipeline) */
          if(dec_cont->pp_instance != NULL &&
              dec_cont->pp_control.use_pipeline == 0) {
            /* output addresses */
            p_dec_out->pictures[0].output_picture_y.virtual_address = NULL;
            p_dec_out->pictures[0].output_picture_y.bus_address = 0;
            if(dec_cont->image.size_chroma) {
              p_dec_out->pictures[0].output_picture_cb_cr.virtual_address = NULL;
              p_dec_out->pictures[0].output_picture_cb_cr.bus_address = 0;
              p_dec_out->pictures[0].output_picture_cr.virtual_address = NULL;
              p_dec_out->pictures[0].output_picture_cr.bus_address = 0;
            }

            JPEGDEC_API_TRC(("JpegDecDecode# Decoder + PP (Rotation/Flip), Slice ready"));
            /* PP not in pipeline, continue do <==> while */
            dec_cont->info.no_slice_irq_for_user = 1;
          } else {
            JPEGDEC_API_TRC(("JpegDecDecode# Slice ready"));
            return JPEGDEC_SLICE_READY;
          }
        } else {
          if((dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE ||
              dec_cont->info.operation_type == JPEGDEC_NONINTERLEAVED) && !dec_cont->b_mc &&
              RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
            current_pos =
              GET_ADDR_REG(dec_cont->jpeg_regs,
                           HWIF_RLC_VLC_BASE);

            u32 hw_consumed = current_pos - dec_cont->jpeg_hw_start_bus;
            if (hw_consumed == 0)
              hw_consumed = GetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_BIT)/8;

	    /* EOI is eaten by H/W, pointer go back 4 bytes here to detect EOI */
	    if (hw_consumed > 4)
	      dec_cont->stream.p_curr_pos = dec_cont->stream.p_curr_pos + hw_consumed - 4;
	    else
	      dec_cont->stream.p_curr_pos = dec_cont->stream.p_curr_pos + hw_consumed;

            dec_cont->stream.bit_pos_in_byte = 0;
            dec_cont->stream.read_bits =
              ((dec_cont->stream.p_curr_pos -
                dec_cont->stream.p_start_of_stream) * 8);

            /* default if data ends */
            end_of_image = 1;
            i32 temp = -1;

            /* check if last scan is decoded */
            for(i = 0;
                i <
                ((dec_cont->stream.stream_length -
                  (dec_cont->stream.read_bits / 8))); i++) {
              current_byte = dec_cont->stream.p_curr_pos[i];
              if(current_byte == 0xFF) {
                current_byte = dec_cont->stream.p_curr_pos[i + 1];
                if(current_byte == 0xD9) {
                  end_of_image = 1;
                  break;
                } else if(current_byte == 0xC4 ||
                          current_byte == 0xDA) {
                  end_of_image = 0;
                  break;
                } else if(current_byte == 0xDD)
                  temp = i;
              }
            }

            if (temp >= 0) i = temp;
            dec_cont->stream.p_curr_pos += i;
            dec_cont->stream.read_bits += 8*i;
            current_byte = 0;
            dec_cont->info.SliceCount = 0;
            dec_cont->info.SliceReadyForPause = 0;

            /* if not the last scan of the stream */
            if(end_of_image == 0) {
              /* output set */
              if(dec_cont->pp_instance == NULL &&
                  !dec_cont->info.no_slice_irq_for_user) {
                if (dec_cont->info.user_alloc_mem ||
                    (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_y.virtual_address =
                      (u32*)((u8*)dec_cont->info.out_luma.virtual_address + ppu_cfg->luma_offset);
                  }
                }
                else {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_y.virtual_address =
                    (u32*)((addr_t)dec_cont->info.out_pp_luma.virtual_address + ppu_cfg->luma_offset);
                  }
                }

                /* output set */
                if (dec_cont->info.user_alloc_mem) {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_y.bus_address =
                      dec_cont->info.out_luma.bus_address + ppu_cfg->luma_offset;
                  }
                }
                else {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    p_dec_out->pictures[i].output_picture_y.bus_address =
                      dec_cont->info.out_pp_luma.bus_address + ppu_cfg->luma_offset;
#ifdef SUPPORT_DEC400
                   if(tile_status_bus_address == 0) {
                     tile_status_virtual_address = p_dec_out->pictures[i].output_picture_y.virtual_address;
                     tile_status_bus_address = p_dec_out->pictures[i].output_picture_y.bus_address;
                   }
#endif
                  }
                }

                /* if not grayscale */
                if(dec_cont->image.size_chroma || (dec_cont->pp_enabled && !(dec_cont->info.user_alloc_mem || (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)))) {
                  if (dec_cont->info.user_alloc_mem ||
                      (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                    for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                      if (!ppu_cfg->enabled) continue;
                      p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                        (u32*)((u8*)dec_cont->info.out_chroma.virtual_address + ppu_cfg->chroma_offset);
                    }
                  }
                  else {
                    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                    for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                      if (!ppu_cfg->enabled) continue;
                      p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                      (u32*)((addr_t)dec_cont->info.out_pp_chroma.virtual_address + ppu_cfg->chroma_offset);
                    }
                  }

                  if (dec_cont->info.user_alloc_mem ||
                      (dec_cont->hw_feature.pp_version == G1_NATIVE_PP)) {
                    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                    for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                      if (!ppu_cfg->enabled) continue;
                      p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                        dec_cont->info.out_chroma.bus_address + ppu_cfg->chroma_offset;
                    }
                  }
                  else {
                    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                    for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                      if (!ppu_cfg->enabled) continue;
                      p_dec_out->pictures[i].output_picture_cb_cr.bus_address =
                        dec_cont->info.out_pp_chroma.bus_address + ppu_cfg->chroma_offset;
                    }
                  }

                  p_dec_out->pictures[0].output_picture_cr.virtual_address =
                    dec_cont->info.out_chroma2.
                    virtual_address;
                  p_dec_out->pictures[0].output_picture_cr.bus_address =
                    dec_cont->info.out_chroma2.bus_address;
                }
                // if(dec_cont->pp_enabled)
                {
                  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                    if (!ppu_cfg->enabled) continue;
                    if (!p_dec_in->dec_image_type) {
                      if (dec_cont->ppu_cfg[i].crop2.enabled) {
                        p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                        p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].crop2.height;
                        p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].crop2.width;
                        p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].crop2.height;
                      } else {
                        p_dec_out->pictures[i].output_width = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                        p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].scale.height;
                        p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                        p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].scale.height;
                      }
                      p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                      p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                      CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                    } else {
                      if (dec_cont->ppu_cfg[i].crop2.enabled) {
                        p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].crop2.width + 15) >> 4) << 4;
                        p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                        p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].crop2.width;
                        p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                      } else {
                        p_dec_out->pictures[i].output_width_thumb = ((dec_cont->ppu_cfg[i].scale.width + 15) >> 4) << 4;
                        p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                        p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                        p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                      }
                      p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                      p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                      CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
                    }
                  }
                }
#ifdef SUPPORT_DEC400
                u32 luma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
                u32 chroma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
                PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
                tile_status_address_offset = 0;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  if (ppu_cfg->tiled_e) {
                    tile_status_address_offset += ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                    luma_size[i] = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                      chroma_size[i] = ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                    }
                  } else if (ppu_cfg->planar) {
                    tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                    luma_size[i] = ppu_cfg->ystride * ppu_cfg->scale.height;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
                      chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height;
                    }
                  } else {
                    tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
                    luma_size[i] =  ppu_cfg->ystride * ppu_cfg->scale.height;
                    if (!ppu_cfg->monochrome) {
                      tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                      chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                    }
                  }
                }
                ppu_cfg = dec_cont->ppu_cfg;
                for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                  if (!ppu_cfg->enabled) continue;
                  p_dec_out->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
                  p_dec_out->pictures[i].luma_table.size = NEXT_MULTIPLE((luma_size[i] / 256 * 4 + 7) / 8, 16);
                  if (!ppu_cfg->monochrome) {
                    p_dec_out->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
                    p_dec_out->pictures[i].chroma_table.size = NEXT_MULTIPLE((chroma_size[i] / 256 * 4 + 7) / 8, 16);
                  }
                }
#endif
              }

              /* PP not in pipeline, continue do <==> while */
              dec_cont->info.no_slice_irq_for_user = 0;

              if(dec_cont->info.operation_type ==
                  JPEGDEC_PROGRESSIVE)
                dec_cont->info.progressive_scan_ready = 1;
              else
                dec_cont->info.non_interleaved_scan_ready = 1;

              /* return control to application if progressive */
              if(dec_cont->info.operation_type !=
                  JPEGDEC_NONINTERLEAVED) {
#if 0
               if(dec_cont->b_mc ||
                   (p_dec_in->picture_buffer_y.virtual_address == NULL &&
                   ((dec_cont->ri_mc_enabled == RI_MC_ENA &&
                   dec_cont->ri_index == dec_cont->last_ri_index) ||
                   dec_cont->ri_mc_enabled != RI_MC_ENA)))
                  //PushOutputPic(&dec_cont->fb_list, p_dec_out, &(dec_cont->p_image_info));
#endif
                /* Release HW */
                if (!dec_cont->b_mc && RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
                  if (!dec_cont->vcmd_used)
                    (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
                  else
                    DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);
                  /* update asic_running */
                  dec_cont->asic_running = 0;
                }

                JPEGDEC_API_TRC
                ("JpegDecDecode# SCAN PROCESSED");
                return (JPEGDEC_SCAN_PROCESSED);
              } else {
                /* set decoded component */
                dec_cont->info.components[dec_cont->info.
                                          component_id] = 1;

                /* check if we have decoded all components */
                if(dec_cont->info.components[0] == 1 &&
                    dec_cont->info.components[1] == 1 &&
                    dec_cont->info.components[2] == 1) {
                  /* continue decoding next scan */
                  dec_cont->info.no_slice_irq_for_user = 0;
                  non_interleaved_rdy = 0;
                } else {
                  /* continue decoding next scan */
                  dec_cont->info.no_slice_irq_for_user = 1;
                  non_interleaved_rdy = 0;
                }
              }
            } else {
              if(dec_cont->info.operation_type ==
                  JPEGDEC_NONINTERLEAVED) {
                /* set decoded component */
                dec_cont->info.components[dec_cont->info.
                                          component_id] = 1;

                /* check if we have decoded all components */
                if(dec_cont->info.components[0] == 1 &&
                    dec_cont->info.components[1] == 1 &&
                    dec_cont->info.components[2] == 1) {
                  /* continue decoding next scan */
                  dec_cont->info.no_slice_irq_for_user = 0;
                  non_interleaved_rdy = 1;
                } else {
                  /* continue decoding next scan */
                  dec_cont->info.no_slice_irq_for_user = 1;
                  non_interleaved_rdy = 0;
                }
              }
            }
          } else {
            /* PP not in pipeline, continue do <==> while */
            dec_cont->info.no_slice_irq_for_user = 0;
          }
        }

        if(dec_cont->info.no_slice_irq_for_user == 0) {
          /* Release HW */
          if (!dec_cont->b_mc && RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
            if (!dec_cont->vcmd_used)
              (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
            else
              (void) DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);
          }
          /* update asic_running */
          dec_cont->asic_running = 0;

          JPEGDEC_API_TRC("JpegDecDecode# FRAME READY");

          /* set image ready */
          dec_cont->image.image_ready = 1;
        }

        /* check PP status... and go */
        if(dec_cont->pp_instance != NULL &&
            !dec_cont->info.no_slice_irq_for_user) {
          /* set pp for stand alone */
          if(!dec_cont->pp_control.use_pipeline) {
            JpegDecPreparePp(dec_cont);

            JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set output write disabled\n"));
            SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_OUT_DIS, 1);

            /* Enable pp */
            JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Enable pp\n"));
            dec_cont->PPRun(dec_cont->pp_instance,
                            &dec_cont->pp_control);

            dec_cont->pp_control.pp_status = DECPP_RUNNING;
            dec_cont->info.pipeline = 1;

            /* Flush regs to hw register */
            JpegFlushRegs(dec_cont);
          }

          /* End PP co-operation */
          if(dec_cont->pp_control.pp_status == DECPP_RUNNING) {
            JPEGDEC_API_TRC("JpegDecDecode# PP END CALL");
            dec_cont->PPEndCallback(dec_cont->pp_instance);
          }

          dec_cont->pp_control.pp_status = DECPP_PIC_READY;
        }

        /* output set */
        if(dec_cont->pp_instance == NULL &&
            !dec_cont->info.no_slice_irq_for_user) {
          /* output set */
          if (dec_cont->info.user_alloc_mem) {
            PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
              if (!ppu_cfg->enabled) continue;
              p_dec_out->pictures[i].output_picture_y.virtual_address =
                (u32*)((u8*)dec_cont->info.out_luma.virtual_address + ppu_cfg->luma_offset);
              p_dec_out->pictures[i].output_picture_y.bus_address =
                dec_cont->info.out_luma.bus_address + ppu_cfg->luma_offset;
            }
          }
          else {
            PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
              if (!ppu_cfg->enabled) continue;
              p_dec_out->pictures[i].output_picture_y.virtual_address =
              (u32*)((addr_t)dec_cont->info.out_pp_luma.virtual_address + ppu_cfg->luma_offset);
              p_dec_out->pictures[i].output_picture_y.bus_address = dec_cont->info.out_pp_luma.bus_address + ppu_cfg->luma_offset;
#ifdef SUPPORT_DEC400
              if(tile_status_bus_address == 0) {
                tile_status_virtual_address = p_dec_out->pictures[i].output_picture_y.virtual_address;
                tile_status_bus_address = p_dec_out->pictures[i].output_picture_y.bus_address;
              }
#endif
            }
          }

          /* if not grayscale */
          if(dec_cont->image.size_chroma || dec_cont->pp_enabled) {
            if (!dec_cont->pp_enabled || dec_cont->info.user_alloc_mem) {
              p_dec_out->pictures[0].output_picture_cb_cr.virtual_address =
                dec_cont->info.out_chroma.virtual_address;
              p_dec_out->pictures[0].output_picture_cb_cr.bus_address =
                dec_cont->info.out_chroma.bus_address;
            }
            else {
              PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
              for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
                if (!ppu_cfg->enabled) continue;
                p_dec_out->pictures[i].output_picture_cb_cr.virtual_address =
                (u32*)((addr_t)dec_cont->info.out_pp_chroma.virtual_address + ppu_cfg->chroma_offset);
                p_dec_out->pictures[i].output_picture_cb_cr.bus_address = dec_cont->info.out_pp_chroma.bus_address + ppu_cfg->chroma_offset;
              }
            }

            p_dec_out->pictures[0].output_picture_cr.virtual_address =
              dec_cont->info.out_chroma2.virtual_address;
            p_dec_out->pictures[0].output_picture_cr.bus_address =
              dec_cont->info.out_chroma2.bus_address;
          }
          // if(dec_cont->pp_enabled)
          {
            PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
              if (!ppu_cfg->enabled) continue;
              if (!p_dec_in->dec_image_type) {
                if (dec_cont->ppu_cfg[i].crop2.enabled) {
                  p_dec_out->pictures[i].output_width = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].crop2.width, ALIGN(ppu_cfg->align));
                  p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].crop2.height;
                  p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].crop2.width;
                  p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].crop2.height;
                } else {
                  p_dec_out->pictures[i].output_width = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
                  p_dec_out->pictures[i].output_height = dec_cont->ppu_cfg[i].scale.height;
                  p_dec_out->pictures[i].display_width = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                  p_dec_out->pictures[i].display_height = dec_cont->ppu_cfg[i].scale.height;
                }
                p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
              } else {
                if (dec_cont->ppu_cfg[i].crop2.enabled) {
                  p_dec_out->pictures[i].output_width_thumb = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].crop2.width, ALIGN(ppu_cfg->align));;
                  p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                  p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].crop2.width;
                  p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].crop2.height;
                } else {
                  p_dec_out->pictures[i].output_width_thumb = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));;
                  p_dec_out->pictures[i].output_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                  p_dec_out->pictures[i].display_width_thumb = dec_cont->ppu_cfg[i].scale.enabled ? dec_cont->ppu_cfg[i].scale.width : dec_cont->frame.X;
                  p_dec_out->pictures[i].display_height_thumb = dec_cont->ppu_cfg[i].scale.height;
                }
                p_dec_out->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
                p_dec_out->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
                CheckOutputFormat(dec_cont->ppu_cfg, &p_dec_out->pictures[i].output_format, i);
              }
            }
          }
#ifdef SUPPORT_DEC400
          u32 luma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
          u32 chroma_size[DEC_MAX_OUT_COUNT]={0,0,0,0,0};
          PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
          tile_status_address_offset = 0;
          for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled) continue;
            if (ppu_cfg->tiled_e) {
              tile_status_address_offset += ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
              luma_size[i] = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
              if (!ppu_cfg->monochrome) {
                tile_status_address_offset += ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
                chroma_size[i] = ppu_cfg->cstride * (NEXT_MULTIPLE(ppu_cfg->scale.height / 2, 4) / 4);
              }
            } else if (ppu_cfg->planar) {
              tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
              luma_size[i] = ppu_cfg->ystride * ppu_cfg->scale.height;
              if (!ppu_cfg->monochrome) {
                tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height;
                chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height;
              }
            } else {
              tile_status_address_offset += ppu_cfg->ystride * ppu_cfg->scale.height;
              luma_size[i] =  ppu_cfg->ystride * ppu_cfg->scale.height;
              if (!ppu_cfg->monochrome) {
                tile_status_address_offset += ppu_cfg->cstride * ppu_cfg->scale.height / 2;
                chroma_size[i] = ppu_cfg->cstride * ppu_cfg->scale.height / 2;
              }
            }
          }
          ppu_cfg = dec_cont->ppu_cfg;
          for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled) continue;
            p_dec_out->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
            p_dec_out->pictures[i].luma_table.size = NEXT_MULTIPLE((luma_size[i] / 256 * 4 + 7) / 8, 16);
            if (!ppu_cfg->monochrome) {
              p_dec_out->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
              p_dec_out->pictures[i].chroma_table.size = NEXT_MULTIPLE((chroma_size[i] / 256 * 4 + 7) / 8, 16);
            }
          }
#endif
          if(dec_cont->b_mc ||
             (p_dec_in->picture_buffer_y.virtual_address == NULL &&
             ((dec_cont->ri_mc_enabled == RI_MC_ENA &&
             dec_cont->ri_index == dec_cont->last_ri_index) ||
             dec_cont->ri_mc_enabled != RI_MC_ENA)))
            PushOutputPic(&dec_cont->fb_list, p_dec_out, &(dec_cont->p_image_info));
        }

#ifdef JPEGDEC_ASIC_TRACE
        {
          JPEGDEC_TRACE_INTERNAL(("\nJpeg_dec_decode# TEST\n"));
          PrintJPEGReg(dec_cont->jpeg_regs);
        }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

        /* get the current stream address  */
        if((dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE ||
            (dec_cont->info.operation_type == JPEGDEC_NONINTERLEAVED &&
             non_interleaved_rdy == 0)) && !dec_cont->b_mc &&
             RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
          ret_code = JpegDecNextScanHdrs(dec_cont);
          if(ret_code != JPEGDEC_OK && ret_code != JPEGDEC_FRAME_READY) {
            /* return */
            return ret_code;
          }
        }
      } else if(dwlret == DWL_HW_WAIT_TIMEOUT) {
        JPEGDEC_API_TRC("SCAN: DWL HW TIMEOUT\n");

        /* Release HW */
		if (dec_cont->vcmd_used)
          (void) DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);          
        else
          (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);

        /* update asic_running */
        dec_cont->asic_running = 0;

        ret_code = JPEGDEC_DWL_HW_TIMEOUT;
        goto end;
      } else if(dwlret == DWL_HW_WAIT_ERROR) {
        JPEGDEC_API_TRC("SCAN: DWL HW ERROR\n");

        /* Release HW */
		if (dec_cont->vcmd_used)
          (void) DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);          
        else
          (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);

        /* update asic_running */
        dec_cont->asic_running = 0;

        ret_code = JPEGDEC_SYSTEM_ERROR;
        goto end;
      }
      dec_cont->ri_index++;
    } while(!dec_cont->image.image_ready ||
              (RI_MC_ENABLED(dec_cont->ri_mc_enabled) &&
               dec_cont->ri_index <= dec_cont->last_ri_index));
    //dec_cont->n_cores_available = 1;
  }

  if (RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
    sem_wait(&dec_cont->sem_ri_mc_done);
    dwlret = DWL_HW_WAIT_OK;
    asic_status = JPEGDEC_X170_IRQ_DEC_RDY;
  }

  if(dec_cont->image.image_ready) {
    JPEGDEC_API_TRC("JpegDecDecode# IMAGE READY");
    JPEGDEC_API_TRC("JpegDecDecode# OK\n");

    /* reset image status */
    dec_cont->image.image_ready = 0;
    dec_cont->image.header_ready = 0;

    /* reset */
    JpegDecClearStructs(dec_cont, 1);

    return (JPEGDEC_FRAME_READY);
  } else {
    JPEGDEC_API_TRC("JpegDecDecode# ERROR\n");
    ret_code = JPEGDEC_ERROR;
  }

end:
  InputQueueReturnBuffer(dec_cont->pp_buffer_queue,
                         dec_cont->asic_buff.pp_luma_buffer.virtual_address);
  return (ret_code);
#undef JPG_FRM
#undef PTR_INFO
}

/*------------------------------------------------------------------------------

    Function name: JpegDecPreparePp

    Functional description:
        Setup PP interface

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
static void JpegDecPreparePp(JpegDecContainer * dec_cont) {
  dec_cont->pp_control.pic_struct = 0;
  dec_cont->pp_control.top_field = 0;

  {
    u32 tmp = GetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_OUT_ENDIAN);

    dec_cont->pp_control.little_endian =
      (tmp == DEC_X170_LITTLE_ENDIAN) ? 1 : 0;
  }
  dec_cont->pp_control.word_swap = GetDecRegister(dec_cont->jpeg_regs,
                                        HWIF_DEC_OUTSWAP32_E) ? 1
                                        : 0;

  /* pipeline */
  if(dec_cont->pp_control.use_pipeline) {
    /* luma */
    dec_cont->pp_control.input_bus_luma = 0;
    /* chroma */
    dec_cont->pp_control.input_bus_chroma = 0;
  } else {
    /* luma */
    dec_cont->pp_control.input_bus_luma =
      dec_cont->asic_buff.out_luma_buffer.bus_address;
    /* chroma */
    dec_cont->pp_control.input_bus_chroma =
      dec_cont->asic_buff.out_chroma_buffer.bus_address;
  }

  /* dimensions */
  dec_cont->pp_control.inwidth =
    dec_cont->pp_control.cropped_w = dec_cont->info.X;

  dec_cont->pp_control.inheight =
    dec_cont->pp_control.cropped_h = dec_cont->info.Y;
}

/*------------------------------------------------------------------------------

    5.6. Function name: JpegGetAPIVersion

         Purpose:       Returns version information about this API

         Input:         void

         Output:        JpegDecApiVersion

------------------------------------------------------------------------------*/
JpegDecApiVersion JpegGetAPIVersion() {
  JpegDecApiVersion ver;

  ver.major = JPG_MAJOR_VERSION;
  ver.minor = JPG_MINOR_VERSION;
  JPEGDEC_API_TRC("JpegGetAPIVersion# OK\n");
  return ver;
}

/*------------------------------------------------------------------------------

    5.7. Function name: JpegDecGetBuild

         Purpose:       Returns the SW and HW build information

         Input:         void

         Output:        JpegDecGetBuild

------------------------------------------------------------------------------*/
JpegDecBuild JpegDecGetBuild(void) {
  JpegDecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_JPEG_DEC);

  DWLReadAsicConfig(build_info.hw_config, DWL_CLIENT_TYPE_JPEG_DEC);

  JPEGDEC_API_TRC("JpegDecGetBuild# OK\n");

  return build_info;
}

/*------------------------------------------------------------------------------
    5.8. Function name   : jpegRegisterPP
         Description     : Called internally by PP to enable the pipeline
         Return type     : i32 - return 0 for success or a negative error code
         Argument        : const void * dec_inst - decoder instance
         Argument        : const void  *pp_inst - post-processor instance
         Argument        : (*PPRun)(const void *) - decoder calls this to start PP
         Argument        : void (*PPEndCallback)(const void *) - decoder calls this
                           to notify PP that a picture was done.
------------------------------------------------------------------------------*/
i32 jpegRegisterPP(const void *dec_inst, const void *pp_inst,
                   void (*PPRun) (const void *, const DecPpInterface *),
                   void (*PPEndCallback) (const void *),
                   void (*PPConfigQuery) (const void *, DecPpQuery *)) {
  JpegDecContainer *dec_cont;

  dec_cont = (JpegDecContainer *) dec_inst;

  if(dec_inst == NULL || dec_cont->pp_instance != NULL ||
      pp_inst == NULL || PPRun == NULL || PPEndCallback == NULL)
    return -1;

  if(dec_cont->asic_running)
    return -2;

  dec_cont->pp_instance = pp_inst;
  dec_cont->PPEndCallback = PPEndCallback;
  dec_cont->PPRun = PPRun;
  dec_cont->PPConfigQuery = PPConfigQuery;

  return 0;
}

/*------------------------------------------------------------------------------
    5.9. Function name   : jpegUnregisterPP
         Description     : Called internally by PP to disable the pipeline
         Return type     : i32 - return 0 for success or a negative error code
         Argument        : const void * dec_inst - decoder instance
         Argument        : const void  *pp_inst - post-processor instance
------------------------------------------------------------------------------*/
i32 jpegUnregisterPP(const void *dec_inst, const void *pp_inst) {
  JpegDecContainer *dec_cont;

  dec_cont = (JpegDecContainer *) dec_inst;

  ASSERT(dec_inst != NULL && pp_inst == dec_cont->pp_instance);

  if(dec_inst == NULL || pp_inst != dec_cont->pp_instance)
    return -1;

  if(dec_cont->asic_running)
    return -2;

  dec_cont->pp_instance = NULL;
  dec_cont->PPEndCallback = NULL;
  dec_cont->PPRun = NULL;

  return 0;
}

JpegDecRet JpegDecSetInfo(JpegDecInst dec_inst,
                            struct JpegDecConfig *dec_cfg) {
  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
  PpUnitConfig *ppu_cfg = &dec_cfg->ppu_config[0];
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;
  u32 i = 0;
  
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  dec_cont->align = dec_cfg->align;
  dec_cont->dec_image_type = dec_cfg->dec_image_type;

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, 8, 1, 0);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(JPEGDEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, 0, 0, 0, dec_cont->ppu_cfg))
    return JPEGDEC_PARAM_ERROR;
  memcpy(dec_cont->delogo_params, dec_cfg->delogo_params, sizeof(dec_cont->delogo_params));
  if (CheckDelogo(dec_cont->delogo_params, 8, 8))
    return JPEGDEC_PARAM_ERROR;
  return(JPEGDEC_OK);
}

JpegDecRet JpegDecSetInfo_INTERNAL(JpegDecContainer *dec_cont) {
  u32 pic_width = (dec_cont->frame.X + 1) & ~0x1;
  u32 pic_height = (dec_cont->frame.Y + 1) & ~0x1;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, 0, dec_cont->ppu_cfg))
    return JPEGDEC_PARAM_ERROR;

  /* Check whether pp height satisfies restart interval based multicore. */
  if (RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
    u32 i = 0;
    u32 ri_height = dec_cont->frame.num_mcu_rows_in_interval *
                       dec_cont->frame.mcu_height;
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    //u32 first_ri_index, last_ri_index;

    dec_cont->first_ri_index = (u32)-1;
    dec_cont->last_ri_index = 0;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      u32 first_slice_height, last_slice_height;
      if (!ppu_cfg->enabled) continue;

      first_slice_height = ri_height - (ppu_cfg->crop.y % ri_height);
      last_slice_height = (ppu_cfg->crop.y + ppu_cfg->crop.height) %
                           ri_height;
      if ((first_slice_height * ppu_cfg->scale.height % ppu_cfg->crop.height) ||
          (ri_height * ppu_cfg->scale.height % ppu_cfg->crop.height) ||
          (last_slice_height * ppu_cfg->scale.height % ppu_cfg->crop.height)) {
        dec_cont->ri_mc_enabled = RI_MC_DISABLED_BY_PP;
        break;
      }

      dec_cont->start_ri_index[i] = ppu_cfg->crop.y / ri_height;
      dec_cont->end_ri_index[i] = (ppu_cfg->crop.y + ppu_cfg->crop.height - 1) /
                                   ri_height;

      if (dec_cont->start_ri_index[i] < dec_cont->first_ri_index)
        dec_cont->first_ri_index = dec_cont->start_ri_index[i];
      if (dec_cont->end_ri_index[i] > dec_cont->last_ri_index)
        dec_cont->last_ri_index = dec_cont->end_ri_index[i];
    }
  }

  return (JPEGDEC_OK);
}

static u32 InitList(FrameBufferList *fb_list) {
  (void) DWLmemset(fb_list, 0, sizeof(*fb_list));

  sem_init(&fb_list->out_count_sem, 0, 0);
  pthread_mutex_init(&fb_list->out_count_mutex, NULL);

  pthread_mutex_init(&fb_list->ref_count_mutex, NULL );

  /* this CV is used to signal the HW has finished processing a picture
   * that is needed for output ( FB_OUTPUT | FB_HW_ONGOING )
   */
  pthread_cond_init(&fb_list->hw_rdy_cv, NULL);

  fb_list->b_initialized = 1;
  return 0;
}

static void ReleaseList(FrameBufferList *fb_list) {
  if (!fb_list->b_initialized)
    return;

  fb_list->b_initialized = 0;

  pthread_mutex_destroy(&fb_list->ref_count_mutex);

  pthread_mutex_destroy(&fb_list->out_count_mutex);
  pthread_cond_destroy(&fb_list->hw_rdy_cv);

  sem_destroy(&fb_list->out_count_sem);
}


static void ClearHWOutput(FrameBufferList *fb_list, u32 id, u32 type) {
  u32 *bs = fb_list->fb_stat + id;

  pthread_mutex_lock(&fb_list->ref_count_mutex);

  ASSERT(*bs & (FB_HW_ONGOING));

  *bs &= ~type;

  if ((*bs & FB_HW_ONGOING) == 0) {
    /* signal that this buffer is done by HW */
    pthread_cond_signal(&fb_list->hw_rdy_cv);
  }

  pthread_mutex_unlock(&fb_list->ref_count_mutex);
}

static void MarkHWOutput(FrameBufferList *fb_list, u32 id, u32 type) {

  pthread_mutex_lock(&fb_list->ref_count_mutex);

  ASSERT( fb_list->fb_stat[id] ^ type );

  fb_list->fb_stat[id] |= type;

  pthread_mutex_unlock(&fb_list->ref_count_mutex);
}

static void PushOutputPic(FrameBufferList *fb_list, const JpegDecOutput *pic, const JpegDecImageInfo *info) {
  if (pic != NULL && info != NULL) {
    pthread_mutex_lock(&fb_list->out_count_mutex);

    while(fb_list->num_out == MAX_FRAME_NUMBER) {
      /* make sure we do not overflow the output */
      /* pthread_cond_signal(&fb_list->out_empty_cv); */
      pthread_mutex_unlock(&fb_list->out_count_mutex);
      sched_yield();
      pthread_mutex_lock(&fb_list->out_count_mutex);
    }

    /* push to tail */
    fb_list->out_fifo[fb_list->wr_id].pic = *pic;
    fb_list->out_fifo[fb_list->wr_id].info = *info;
    fb_list->out_fifo[fb_list->wr_id].mem_idx = fb_list->wr_id;
    fb_list->num_out++;

    ASSERT(fb_list->num_out <= MAX_FRAME_NUMBER);

    fb_list->wr_id++;
    if (fb_list->wr_id >= MAX_FRAME_NUMBER)
      fb_list->wr_id = 0;

    pthread_mutex_unlock(&fb_list->out_count_mutex);
  } else {
    fb_list->end_of_stream = 1;
  }
  sem_post(&fb_list->out_count_sem);
}

static void SetAbortStatusInList(FrameBufferList *fb_list) {
  if(fb_list == NULL || !fb_list->b_initialized)
    return;

  pthread_mutex_lock(&fb_list->ref_count_mutex);
  fb_list->abort = 1;
  pthread_mutex_unlock(&fb_list->ref_count_mutex);
  sem_post(&fb_list->out_count_sem);
}

static void ClearAbortStatusInList(FrameBufferList *fb_list) {
  if(fb_list == NULL || !fb_list->b_initialized)
    return;

  pthread_mutex_lock(&fb_list->ref_count_mutex);
  fb_list->abort = 0;
  pthread_mutex_unlock(&fb_list->ref_count_mutex);
}

static void ResetOutFifoInList(FrameBufferList *fb_list) {
  (void)DWLmemset(fb_list->out_fifo, 0, MAX_FRAME_NUMBER * sizeof(struct OutElement_));
  fb_list->wr_id = 0;
  fb_list->rd_id = 0;
  fb_list->num_out = 0;
}


static u32 PeekOutputPic(FrameBufferList *fb_list, JpegDecOutput *pic, JpegDecImageInfo *info) {
  u32 mem_idx;
  JpegDecOutput *out;
  JpegDecImageInfo *info_tmp;

  sem_wait(&fb_list->out_count_sem);

  if (fb_list->abort)
    return ABORT_MARKER;

  pthread_mutex_lock(&fb_list->out_count_mutex);
  if (!fb_list->num_out) {
    if (fb_list->end_of_stream) {
      pthread_mutex_unlock(&fb_list->out_count_mutex);
      return 2;
    } else {
      pthread_mutex_unlock(&fb_list->out_count_mutex);
      return 0;
    }
  }
  pthread_mutex_unlock(&fb_list->out_count_mutex);

  out = &fb_list->out_fifo[fb_list->rd_id].pic;
  info_tmp = &fb_list->out_fifo[fb_list->rd_id].info;
  mem_idx = fb_list->out_fifo[fb_list->rd_id].mem_idx;

  pthread_mutex_lock(&fb_list->ref_count_mutex);

  while((fb_list->fb_stat[mem_idx] & FB_HW_ONGOING) != 0)
    pthread_cond_wait(&fb_list->hw_rdy_cv, &fb_list->ref_count_mutex);

  pthread_mutex_unlock(&fb_list->ref_count_mutex);

  /* pop from head */
  (void)DWLmemcpy(pic, out, sizeof(JpegDecOutput));
  (void)DWLmemcpy(info, info_tmp, sizeof(JpegDecImageInfo));

  pthread_mutex_lock(&fb_list->out_count_mutex);

  fb_list->num_out--;

  /* go to next output */
  fb_list->rd_id++;
  if (fb_list->rd_id >= MAX_FRAME_NUMBER)
    fb_list->rd_id = 0;

  pthread_mutex_unlock(&fb_list->out_count_mutex);

  return 1;
}

JpegDecRet JpegDecNextPicture(JpegDecInst dec_inst,
                              JpegDecOutput * output,
                              JpegDecImageInfo *info) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  u32 ret;

  if(dec_inst == NULL || output == NULL) {
    JPEGDEC_API_TRC("JpegDecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (JPEGDEC_PARAM_ERROR);
  }

  if((ret = PeekOutputPic(&dec_cont->fb_list, output, info))) {
    if (ret == 3) {
      JPEGDEC_API_TRC("JpegDecNextPicture# JPEGDEC_ABORTED\n");
      return (JPEGDEC_ABORTED);
    }
    if(ret == 2) {
      JPEGDEC_API_TRC("JpegDecNextPicture# JPEGDEC_END_OF_STREAM\n");
      return (JPEGDEC_END_OF_STREAM);
    }
    if(ret == 1) {
     JPEGDEC_API_TRC("JpegDecNextPicture# JPEGDEC_PIC_RDY\n");
      return (JPEGDEC_FRAME_READY);
    }
    if (ret == 0) {
      JPEGDEC_API_TRC("JpegDecNextPicture# JPEGDEC_OK\n");
      return (JPEGDEC_OK);
    }
  }
  return (JPEGDEC_OK);
}

JpegDecRet JpegDecPictureConsumed(JpegDecInst dec_inst, JpegDecOutput * output) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  u32 i;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  JPEGDEC_API_TRC("JpegDecPictureConsumed#\n");

  if(dec_inst == NULL || output == NULL) {
    JPEGDEC_API_TRC("JpegDecPictureConsumed# ERROR: dec_inst or output is NULL\n");
    return (JPEGDEC_PARAM_ERROR);
  }

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled)
      continue;
    else {
      output_picture = output->pictures[i].output_picture_y.virtual_address;
      break;
    }
  }
  InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);

  return (JPEGDEC_OK);
}

JpegDecRet JpegDecEndOfStream(JpegDecInst dec_inst) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  u32 count = 0;
  i32 core_id[MAX_ASIC_CORES];

  if(dec_inst == NULL) {
    JPEGDEC_API_TRC("JpegDecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (JPEGDEC_PARAM_ERROR);
  }

  if(dec_cont->vcmd_used){
#if 0
    DWLReserveCmdBuf(dec_cont->dwl, DWL_CLIENT_TYPE_JPEG_DEC,
                      dec_cont->info.X, dec_cont->info.Y,
                      &dec_cont->cmd_buf_id);
    DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);
#else
    DWLWaitCmdbufsDone(dec_cont->dwl);
#endif
  } else {
    /* Check all Core in idle state */
	for(count = 0; count < dec_cont->n_cores_available; count++) {
      DWLReserveHw(dec_cont->dwl, &core_id[count], DWL_CLIENT_TYPE_JPEG_DEC);
    }
    /* All HW Core finished */
    for(count = 0; count < dec_cont->n_cores_available; count++) {
      DWLReleaseHw(dec_cont->dwl, core_id[count]);
    }
  }

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, NULL);
  return (JPEGDEC_OK);
}

static void JpegMCHwRdyCallback(void *args, i32 core_id) {
  JpegDecContainer *dec_cont = (JpegDecContainer *)args;
  JpegHwRdyCallbackArg info;

  const void *dwl;

  u32 type = FB_HW_ONGOING;

  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES || (dec_cont->vcmd_used && core_id < MAX_MC_CB_ENTRIES));

  /* take a copy of the args as after we release the HW they
   * can be overwritten.
   */
  dwl = dec_cont->dwl;
  info = dec_cont->hw_rdy_callback_arg[core_id];

  /* React to the HW return value */

  /* clear IRQ status reg and release HW core */
  if (!dec_cont->vcmd_used) {
    DWLDisableHw(dwl, core_id, 0x04, 0);
    DWLReleaseHw(dwl, info.core_id);
  } else {
    DWLReleaseCmdBuf(dwl, info.core_id);
  }
  if(dec_cont->stream_consumed_callback.fn)
    dec_cont->stream_consumed_callback.fn((u8*)info.stream,
                                          (void*)info.p_user_data);
  ClearHWOutput(&dec_cont->fb_list, info.out_id, type);
}

static void JpegMCSetHwRdyCallback(JpegDecInst dec_inst) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  u32 type = FB_HW_ONGOING;
  JpegHwRdyCallbackArg *arg = &dec_cont->hw_rdy_callback_arg[dec_cont->core_id];
  i32 core_id = dec_cont->core_id;

  if (dec_cont->vcmd_used) {
    arg = &dec_cont->hw_rdy_callback_arg[dec_cont->cmd_buf_id];
    core_id = dec_cont->cmd_buf_id;
  }
  arg->core_id = core_id;
  arg->stream = dec_cont->stream_consumed_callback.p_strm_buff;
  arg->p_user_data = dec_cont->stream_consumed_callback.p_user_data;
  arg->out_id = dec_cont->fb_list.wr_id;

  DWLSetIRQCallback(dec_cont->dwl, core_id, JpegMCHwRdyCallback,
                    dec_cont);

  MarkHWOutput(&dec_cont->fb_list, arg->out_id, type);
}

static void JpegRiMCHwRdyCallback(void *args, i32 core_id) {
  JpegDecContainer *dec_cont = (JpegDecContainer *)args;
  JpegHwRdyCallbackArg info;
  const void *dwl;

  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES);

  /* take a copy of the args as after we release the HW they
   * can be overwritten.
   */
  dwl = dec_cont->dwl;
  info = dec_cont->hw_rdy_callback_arg[core_id];
  dec_cont->ri_running_cores--;

  /* React to the HW return value */
#if 0
  u32 perf_cycles;
  perf_cycles = DWLReadReg(dwl, core_id, 63 * 4);
  printf("Core[%d] cycles %d/MB\n", core_id,
          perf_cycles /
          (dec_cont->info.X * dec_cont->frame.mcu_height *
           dec_cont->frame.num_mcu_rows_in_interval / 256));
#endif

  /* clear IRQ status reg and release HW core */
  DWLDisableHw(dwl, core_id, 0x04, 0);
  DWLReleaseHw(dwl, info.core_id);

  if (dec_cont->ri_index > dec_cont->last_ri_index &&
      dec_cont->ri_running_cores == 0) {
    sem_post(&dec_cont->sem_ri_mc_done);
  }
}

static void JpegRiMCSetHwRdyCallback(JpegDecInst dec_inst) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  JpegHwRdyCallbackArg *arg = &dec_cont->hw_rdy_callback_arg[dec_cont->core_id];

  arg->core_id = dec_cont->core_id;

  DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id, JpegRiMCHwRdyCallback,
                    dec_cont);
}

/* TODO: consider case with missing restart intervals. */
static u32 JpegParseRST(JpegDecInst dec_inst, u8 *img_buf, u32 img_len,
                             u8 **ri_array) {
  u8 *p = img_buf;
  u32 rst_markers = 0;
  u32 last_rst, i;
  while (p < img_buf + img_len) {
    if (p[0] == 0xFF && p[1] >= 0xD0 && p[1] <= 0xD7) {
      if (!rst_markers) {
        rst_markers++;
      } else {
        /* missing restart intervals */
        u32 missing_rst_count = (p[1] - 0xD7 + 8 - last_rst - 1) % 8;
        for (i = 0; i < missing_rst_count; i++)
          ri_array[++rst_markers] = NULL;
        rst_markers++;
      }
      last_rst = p[1] - 0xD7;
      ri_array[rst_markers]  = p + 2;
#if 0
      printf("RST%d @ offset %d: %02X%02X\n", p[1] - 0xD0,
            (u32)(p - img_buf), p[0], p[1]);
#endif
    }
    p++;
  }
  return (rst_markers);

  rst_markers++;
  return (rst_markers);
}

void JpegSetExternalBufferInfo(JpegDecInst dec_inst) {
  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
  u32 ext_buffer_size = 0;
  u32 pp_width = 0, pp_height_luma = 0,
      pp_stride = 0, pp_buff_size = 0;

  u32 buffers = 1;

  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    if (dec_cont->info.slice_mb_set_value) {
      if (ppu_cfg->tiled_e) {
        pp_width = dec_cont->info.X >> dec_cont->dscale_shift_x;
        pp_height_luma = NEXT_MULTIPLE(dec_cont->info.slice_mb_set_value * 16 >> dec_cont->dscale_shift_y, 4) / 4;
        pp_stride = NEXT_MULTIPLE(4 * pp_width, ALIGN(ppu_cfg->align));
        pp_buff_size = pp_stride * pp_height_luma;
      } else {
        pp_width = dec_cont->info.X >> dec_cont->dscale_shift_x;
        pp_height_luma = dec_cont->info.slice_mb_set_value * 16 >> dec_cont->dscale_shift_y;
        pp_stride = NEXT_MULTIPLE(pp_width, ALIGN(ppu_cfg->align));
        pp_buff_size = pp_stride * pp_height_luma;
      }
      dec_cont->ppu_cfg[0].luma_offset = 0;
      dec_cont->ppu_cfg[0].chroma_offset = pp_buff_size;
    } else {
      ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
      /*for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        if (ppu_cfg->tiled_e)
          pp_height = NEXT_MULTIPLE(dec_cont->frame.b_y, 4) / 4;
        else
          pp_height = dec_cont->frame.b_y;
        pp_stride = NEXT_MULTIPLE(dec_cont->frame.b_x, dec_cont->align);
        pp_buff_size = pp_stride * pp_height;
        ppu_cfg->luma_offset = ext_buffer_size;
        ppu_cfg->chroma_offset = ext_buffer_size + pp_buff_size;
        ppu_cfg->luma_size = pp_buff_size;
        if (ppu_cfg->tiled_e)
          pp_height = NEXT_MULTIPLE(dec_cont->frame.b_y/2, 4) / 4;
        else {
          if (ppu_cfg->planar)
             pp_height = dec_cont->frame.b_y;
          else
             pp_height = dec_cont->frame.b_y / 2;
        }
        pp_stride = ppu_cfg->cstride;
        ppu_cfg->chroma_size = pp_stride * pp_height;
        pp_buff_size += pp_stride * pp_height;
        ext_buffer_size += NEXT_MULTIPLE(pp_buff_size, 16);
      }*/
    }
  }

  dec_cont->tot_buffers = dec_cont->buf_num =  buffers;
  dec_cont->pre_buf_size = dec_cont->next_buf_size;
  dec_cont->next_buf_size = ext_buffer_size;
}

JpegDecRet JpegDecGetBufferInfo(JpegDecInst dec_inst, JpegDecBufferInfo *mem_info) {

  JpegDecContainer  * dec_cont = (JpegDecContainer *)dec_inst;

  dec_cont->frame.X = dec_cont->dec_image_type ?
                      dec_cont->p_image_info.display_width_thumb :
                      dec_cont->p_image_info.display_width;
  dec_cont->frame.Y = dec_cont->dec_image_type ?
                      dec_cont->p_image_info.display_height_thumb :
                      dec_cont->p_image_info.display_height;

  u32 pic_width = (dec_cont->frame.X + 1) & ~0x1;
  u32 pic_height = (dec_cont->frame.Y + 1) & ~0x1;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, 0, dec_cont->ppu_cfg))
    return JPEGDEC_PARAM_ERROR;

  JpegSetExternalBufferInfo(dec_cont);

  struct DWLLinearMem empty = {0, 0, 0};

  if(dec_cont == NULL || mem_info == NULL) {
    return JPEGDEC_PARAM_ERROR;
  }

  if(dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num;
    return JPEGDEC_OK;
  }

  if(dec_cont->buf_to_free) {
    mem_info->buf_to_free = *dec_cont->buf_to_free;
    dec_cont->buf_to_free->virtual_address = NULL;
    dec_cont->buf_to_free = NULL;
  } else
    mem_info->buf_to_free = empty;

  /* Instant buffer mode: if new image size is bigger than previous, reallocate buffer. */
  if (dec_cont->info.user_alloc_mem && dec_cont->pre_buf_size != 0 &&
      dec_cont->next_buf_size > dec_cont->pre_buf_size) {
    mem_info->buf_to_free = dec_cont->info.given_out_luma;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num;
    return JPEGDEC_WAITING_FOR_BUFFER;
  }

  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num;

  ASSERT((mem_info->buf_num && mem_info->next_buf_size) ||
         (mem_info->buf_to_free.virtual_address != NULL));

  return JPEGDEC_OK;
}

JpegDecRet JpegDecAddBuffer(JpegDecInst dec_inst, struct DWLLinearMem *info) {
  JpegDecContainer *dec_cont = (JpegDecContainer *)dec_inst;
  JpegDecRet dec_ret = JPEGDEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return JPEGDEC_PARAM_ERROR;
  }

  dec_cont->n_ext_buf_size = info->size;
  if (!dec_cont->realloc_buffer) {
    dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
    dec_cont->ext_buffer_num++;

    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  } else {
    dec_cont->ext_buffers[dec_cont->buffer_index] = *info;
    InputQueueUpdateBuffer(dec_cont->pp_buffer_queue, info, dec_cont->buffer_queue_index);
  }

  return dec_ret;
}


void JpegExistAbortState(JpegDecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  InputQueueClearAbort(dec_cont->pp_buffer_queue);
  dec_cont->abort = 0;
}


JpegDecRet JpegDecAbort(JpegDecInst dec_inst) {
  if(dec_inst == NULL) {
    JPEGDEC_API_TRC("JpegDecAbort# ERROR: dec_inst or output is NULL\n");
    return (JPEGDEC_PARAM_ERROR);
  }
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;

  SetAbortStatusInList(&dec_cont->fb_list);
  InputQueueSetAbort(dec_cont->pp_buffer_queue);
  dec_cont->abort = 1;

  return (JPEGDEC_OK);
}



JpegDecRet JpegDecAbortAfter(JpegDecInst dec_inst) {
  JpegDecContainer *dec_cont = (JpegDecContainer *) dec_inst;
  u32 i = 0;
  i32 core_id[MAX_ASIC_CORES];

  if(dec_inst == NULL) {
    JPEGDEC_API_TRC("JpegDecAbortAfter# ERROR: dec_inst or output is NULL\n");
    return (JPEGDEC_PARAM_ERROR);
  }

  if(dec_cont->asic_running && !dec_cont->b_mc) {
    /* Release HW */
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0);
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  }

  /* In multi-Core senario, waithwready is executed through listener thread,
          here to check whether HW is finished */
  if(dec_cont->b_mc) {
    for(i = 0; i < dec_cont->n_cores_available; i++) {
      DWLReserveHw(dec_cont->dwl, &core_id[i], DWL_CLIENT_TYPE_JPEG_DEC);
    }
    /* All HW Core finished */
    for(i = 0; i < dec_cont->n_cores_available; i++) {
      DWLReleaseHw(dec_cont->dwl, core_id[i]);
    }
  }

  ResetOutFifoInList(&dec_cont->fb_list);
  InputQueueReset(dec_cont->pp_buffer_queue);
  JpegDecClearStructs(dec_cont, 1);
  dec_cont->ext_buffer_num = 0;
  ClearAbortStatusInList(&dec_cont->fb_list);
  InputQueueClearAbort(dec_cont->pp_buffer_queue);
  return (JPEGDEC_OK);
}
