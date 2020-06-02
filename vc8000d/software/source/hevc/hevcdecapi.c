/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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
#include "hevc_container.h"
#include "hevcdecapi.h"
#include "hevc_decoder.h"
#include "hevc_util.h"
#include "hevc_dpb.h"
#include "hevc_asic.h"
#include "regdrv.h"
#include "hevc_byte_stream.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "dwl.h"
#include "version.h"
#include "decapicommon.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include "hevcdecmc_internals.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#if defined(USE_RANDOM_TEST) || defined(RANDOM_CORRUPT_RFC)
#include "string.h"
#include "stream_corrupt.h"
#endif

static void HevcUpdateAfterPictureDecode(struct HevcDecContainer *dec_cont);
static u32 HevcSpsSupported(const struct HevcDecContainer *dec_cont);
static u32 HevcPpsSupported(const struct HevcDecContainer *dec_cont);

static u32 HevcAllocateResources(struct HevcDecContainer *dec_cont);
static void HevcInitPicFreezeOutput(struct HevcDecContainer *dec_cont,
                                    u32 from_old_dpb);
static void HevcGetSarInfo(const struct Storage *storage, u32 *sar_width,
                           u32 *sar_height);
extern void HevcPreparePpRun(struct HevcDecContainer *dec_cont);

static enum DecRet HevcDecNextPictureInternal(
  struct HevcDecContainer *dec_cont);
static void HevcCycleCount(struct HevcDecContainer *dec_cont);
static void HevcDropCurrentPicutre(struct HevcDecContainer *dec_cont);

static void HevcEnterAbortState(struct HevcDecContainer *dec_cont);
static void HevcExistAbortState(struct HevcDecContainer *dec_cont);

#ifdef RANDOM_CORRUPT_RFC
u32 HevcCorruptRFC(struct HevcDecContainer *dec_cont);
#endif

#define DEC_DPB_NOT_INITIALIZED -1
#define DEC_MODE_HEVC 12
#define CHECK_TAIL_BYTES 16
extern volatile struct strmInfo stream_info;

#ifdef PERFORMANCE_TEST
extern u32 hw_time_use;
#endif

/* Initializes decoder software. Function reserves memory for the
 * decoder instance and calls HevcInit to initialize the
 * instance data. */
enum DecRet HevcDecInit(HevcDecInst *dec_inst, const void *dwl, struct HevcDecConfig *dec_cfg) {

  /*@null@ */ struct HevcDecContainer *dec_cont;

  struct DWLInitParam dwl_init;
  DWLHwConfig hw_cfg;
  struct DecHwFeatures hw_feature;
  u32 id, hw_build_id, i, tmp;
  u32 is_legacy = 0;
  u32 low_latency_sim = 0;
  u32 secure = 0;
  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if (dec_inst == NULL || dwl == NULL || dec_cfg == NULL) {
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL; /* return NULL instance for any error */

  (void)DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  id = DWLReadAsicID(DWL_CLIENT_TYPE_HEVC_DEC);
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_HEVC_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  if ((id & 0x0000F000) >> 12 == 0 && /* MAJOR_VERSION */
      (id & 0x00000FF0) >> 4 == 0) {  /* MINOR_VERSION */
    is_legacy = 1;
    hw_cfg.hevc_support = 1;
    hw_cfg.vp9_support = 1;
    hw_cfg.hevc_main10_support = 0;
    hw_cfg.vp9_10bit_support = 0;
    hw_cfg.ds_support = 0;
    hw_cfg.rfc_support = 0;
    hw_cfg.ring_buffer_support = 0;
    hw_cfg.fmt_p010_support = 0;
    hw_cfg.fmt_customer1_support = 0;
    hw_cfg.addr64_support = 0;
    hw_cfg.mrb_prefetch = 0;
  } else if ((id & 0x0000F000) >> 12 == 0 && /* MAJOR_VERSION */
             (id & 0x00000FF0) >> 4 == 0x18) { /* MINOR_VERSION */
    /* Legacy release without correct config register */
    hw_cfg.hevc_support = 1;
    hw_cfg.vp9_support = 1;
    hw_cfg.hevc_main10_support = 1;
    hw_cfg.vp9_10bit_support = 1;
    hw_cfg.ds_support = 1;
    hw_cfg.rfc_support = 1;
    hw_cfg.ring_buffer_support = 1;
    hw_cfg.fmt_p010_support = 0;
    hw_cfg.fmt_customer1_support = 0;
    hw_cfg.addr64_support = 0;
    hw_cfg.mrb_prefetch = 0;
  }
  /* check that hevc decoding supported in HW */
  if (!hw_feature.hevc_support) {
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if (!hw_feature.rfc_support && dec_cfg->use_video_compressor) {
    return DEC_PARAM_ERROR;
  }

#if 0
  if (!hw_feature.dscale_support && (dec_cfg->ppu_cfg[0].scale.enabled ||
                                     dec_cfg->ppu_cfg[1].scale.enabled ||
                                     dec_cfg->ppu_cfg[2].scale.enabled ||
                                     dec_cfg->ppu_cfg[3].scale.enabled)) {
    return DEC_PARAM_ERROR;
  }
#endif

  if (!hw_feature.ring_buffer_support && dec_cfg->use_ringbuffer) {
    return DEC_PARAM_ERROR;
  }

  if ((!hw_feature.fmt_p010_support && dec_cfg->pixel_format == DEC_OUT_PIXEL_P010) ||
      (!hw_feature.fmt_customer1_support && dec_cfg->pixel_format == DEC_OUT_PIXEL_CUSTOMER1) ||
      (!hw_feature.addr64_support && sizeof(addr_t) == 8))
    return DEC_PARAM_ERROR;

  /* TODO: ? */
  dwl_init.client_type = DWL_CLIENT_TYPE_HEVC_DEC;

  dec_cont =
    (struct HevcDecContainer *)DWLmalloc(sizeof(struct HevcDecContainer));

  if (dec_cont == NULL) {
    return (DEC_MEMFAIL);
  }

  (void)DWLmemset(dec_cont, 0, sizeof(struct HevcDecContainer));
  dec_cont->dwl = dwl;
  dec_cont->hevc_regs[0] = id;
  dec_cont->tile_by_tile = dec_cfg->tile_by_tile;

#ifdef PERFORMANCE_TEST
  SwActivityTraceInit(&dec_cont->activity);
#endif

  HevcInit(&dec_cont->storage, dec_cfg->no_output_reordering);

  dec_cont->dec_state = HEVCDEC_INITIALIZED;

  SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MODE, DEC_MODE_HEVC);

  SetCommonConfigRegs(dec_cont->hevc_regs);

#ifndef _DISABLE_PIC_FREEZE
#ifndef USE_FAST_EC
  dec_cont->storage.intra_freeze = dec_cfg->use_video_freeze_concealment;
#else
  dec_cont->storage.intra_freeze = 1;   //dec_cfg->use_video_freeze_concealment & 2;
  dec_cont->storage.fast_freeze = 1;
#endif
#endif
  dec_cont->storage.picture_broken = HANTRO_FALSE;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);

  /* max decodable picture width and height*/
  dec_cont->max_dec_pic_width = hw_feature.hevc_max_dec_pic_width;
  dec_cont->max_dec_pic_height = hw_feature.hevc_max_dec_pic_height;

  dec_cont->checksum = dec_cont; /* save instance as a checksum */

  *dec_inst = (HevcDecInst)dec_cont;

  /*  default single core */
  dec_cont->n_cores = 1;
  dec_cont->n_cores_available = 1;

  /* Init frame buffer list */
  InitList(&dec_cont->fb_list);

  dec_cont->storage.dpb[0].fb_list = &dec_cont->fb_list;
  dec_cont->storage.dpb[1].fb_list = &dec_cont->fb_list;
  dec_cont->output_format = dec_cfg->output_format;

  if (dec_cfg->output_format == DEC_OUT_FRM_RASTER_SCAN) {
    dec_cont->storage.raster_enabled = 1;
  }

  dec_cont->pp_enabled = 0;
  dec_cont->storage.pp_enabled = 0;
  dec_cont->use_8bits_output = (dec_cfg->pixel_format == DEC_OUT_PIXEL_CUT_8BIT) ? 1 : 0;
  dec_cont->use_p010_output = (dec_cfg->pixel_format == DEC_OUT_PIXEL_P010) ? 1 : 0;
  dec_cont->pixel_format = dec_cfg->pixel_format;
  dec_cont->storage.use_8bits_output = dec_cont->use_8bits_output;
  dec_cont->storage.use_p010_output = dec_cont->use_p010_output;
#if 0
  /* Down scaler ratio */
  if ((dec_cfg->dscale_cfg.down_scale_x == 1) || (dec_cfg->dscale_cfg.down_scale_y == 1)) {
    dec_cont->pp_enabled = 0;
    dec_cont->down_scale_x = 1;
    dec_cont->down_scale_y = 1;
  } else if ((dec_cfg->dscale_cfg.down_scale_x != 2 &&
              dec_cfg->dscale_cfg.down_scale_x != 4 &&
              dec_cfg->dscale_cfg.down_scale_x != 8 ) ||
             (dec_cfg->dscale_cfg.down_scale_y != 2 &&
              dec_cfg->dscale_cfg.down_scale_y != 4 &&
              dec_cfg->dscale_cfg.down_scale_y != 8 )) {
    return (DEC_PARAM_ERROR);
  } else {
    u32 scale_table[9] = {0, 0, 1, 0, 2, 0, 0, 0, 3};

    dec_cont->pp_enabled = 1;
    dec_cont->down_scale_x = dec_cfg->dscale_cfg.down_scale_x;
    dec_cont->down_scale_y = dec_cfg->dscale_cfg.down_scale_y;

    dec_cont->storage.pp_enabled = 1;
    //dec_cont->storage.down_scale_x_shift = (dec_cont->down_scale_x >> 2) + 1;
    //dec_cont->storage.down_scale_y_shift = (dec_cont->down_scale_y >> 2) + 1;
    dec_cont->storage.down_scale_x_shift = scale_table[dec_cont->down_scale_x];
    dec_cont->storage.down_scale_y_shift = scale_table[dec_cont->down_scale_y];
  }
#endif
  dec_cont->guard_size = dec_cfg->guard_size;
  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->ext_buffer_num = 0;
  dec_cont->ext_buffer_config  = 0;
#if 0
  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_cont->ext_buffer_config |= 1 << RASTERSCAN_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_TILED_4X4)
    dec_cont->ext_buffer_config  = 1 << REFERENCE_BUFFER;
#endif
  dec_cont->hevc_main10_support = hw_feature.hevc_main10_support;
  dec_cont->use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->use_ringbuffer = dec_cfg->use_ringbuffer;
  dec_cont->use_fetch_one_pic = 0;
  dec_cont->storage.use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->legacy_regs = 0; //is_legacy;

  dec_cont->secure_mode = dec_cfg->decoder_mode == DEC_SECURITY ? 1 : 0;

  dec_cont->align = DEC_ALIGN_16B;
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
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
    SetDecRegister(dec_cont->hevc_regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
  } else if (secure) {
    SetDecRegister(dec_cont->hevc_regs, HWIF_DRM_E, 1);
  } else {
    SetDecRegister(dec_cont->hevc_regs, HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->hevc_regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }
  if (dec_cfg->decoder_mode == DEC_PARTIAL_DECODING)
    dec_cont->partial_decoding = 1;
  //dec_cont->in_buffers = InputQueueInit(MAX_FRAME_BUFFER_NUMBER);
  //if (dec_cont->in_buffers == NULL)
  //  return DEC_MEMFAIL;

#if defined(USE_RANDOM_TEST) || defined(RANDOM_CORRUPT_RFC)
  /*********************************************************/
  /** Developers can change below parameters to generate  **/
  /** different kinds of error stream.                    **/
  /*********************************************************/
  dec_cont->error_params.seed = 66;
  strcpy(dec_cont->error_params.truncate_stream_odds , "1 : 6");
  strcpy(dec_cont->error_params.swap_bit_odds , "1 : 100000");
  strcpy(dec_cont->error_params.packet_loss_odds, "1 : 6");
  /*********************************************************/

  if (strcmp(dec_cont->error_params.swap_bit_odds, "0") != 0) {
    dec_cont->error_params.swap_bits_in_stream = 0;
  }

  if (strcmp(dec_cont->error_params.packet_loss_odds, "0") != 0) {
    dec_cont->error_params.lose_packets = 1;
  }

  if (strcmp(dec_cont->error_params.truncate_stream_odds, "0") != 0) {
    dec_cont->error_params.truncate_stream = 1;
  }

  if (dec_cont->error_params.swap_bits_in_stream ||
      dec_cont->error_params.lose_packets ||
      dec_cont->error_params.truncate_stream) {
    dec_cont->error_params.random_error_enabled = 1;
    InitializeRandom(dec_cont->error_params.seed);
  }
#endif

#ifdef DUMP_INPUT_STREAM
  dec_cont->ferror_stream = fopen("random_error.hevc", "wb");
  if(dec_cont->ferror_stream == NULL) {
    DEBUG_PRINT(("Unable to open file error.hevc\n"));
    return DEC_MEMFAIL;
  }
#endif

  if (dec_cfg->mcinit_cfg.mc_enable) {
    dec_cont->b_mc = 1;
    dec_cont->n_cores = DWLReadAsicCoreCount();

    /* check how many cores support HEVC */
    tmp = dec_cont->n_cores;
    for(i = 0; i < dec_cont->n_cores; i++) {
      hw_build_id = DWLReadCoreHwBuildID(i);
      GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
      if (!hw_feature.hevc_support)
        tmp--;
      if (hw_feature.has_2nd_pipeline) {
        tmp--;
        i++;  /* the second pipeline is treated as another core in driver */
      }
    }
    dec_cont->n_cores_available = tmp;

    dec_cont->stream_consumed_callback.fn = dec_cfg->mcinit_cfg.stream_consumed_callback;

    /* enable synchronization writing in multi-core HW */
    if(dec_cont->n_cores > 1) {
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MULTICORE_E, 1);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_WRITESTAT_E, 1);
    }
  }

#ifdef SUPPORT_VCMD
  dec_cont->vcmd_used = DWLVcmdCores();
  if (dec_cont->b_mc) {
    int i;
    FifoInit(dec_cont->n_cores_available, &dec_cont->fifo_core);
    for (i = 0; i < dec_cont->n_cores_available; i++) {
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
    }
  }
#endif

  (void)dwl_init;
  (void)is_legacy;

  return (DEC_OK);
}

/* This function provides read access to decoder information. This
 * function should not be called before HevcDecDecode function has
 * indicated that headers are ready. */
enum DecRet HevcDecGetInfo(HevcDecInst dec_inst, struct HevcDecInfo *dec_info) {
  u32 cropping_flag;
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  struct Storage *storage;

  if (dec_inst == NULL || dec_info == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;

  if (storage->active_sps == NULL || storage->active_pps == NULL) {
    return (DEC_HDRS_NOT_RDY);
  }

  dec_info->pic_width = HevcPicWidth(storage);
  dec_info->pic_height = HevcPicHeight(storage);
  dec_info->video_range = HevcVideoRange(storage);
  dec_info->matrix_coefficients = HevcMatrixCoefficients(storage);
  dec_info->colour_primaries = HevcColourPrimaries(storage);
  dec_info->transfer_characteristics = HevcTransferCharacteristics(storage);

  dec_info->mono_chrome = HevcIsMonoChrome(storage);
  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_info->pic_buff_size = storage->active_sps->max_dpb_size + 1 + 1;
  else
    dec_info->pic_buff_size = storage->active_sps->max_dpb_size + 1 + 2;
  dec_info->multi_buff_pp_size =
    storage->dpb->no_reordering ? 2 : dec_info->pic_buff_size;
  dec_info->dpb_mode = dec_cont->dpb_mode;

  HevcGetSarInfo(storage, &dec_info->sar_width, &dec_info->sar_height);

  HevcCroppingParams(storage, &cropping_flag,
                     &dec_info->crop_params.crop_left_offset,
                     &dec_info->crop_params.crop_out_width,
                     &dec_info->crop_params.crop_top_offset,
                     &dec_info->crop_params.crop_out_height);

  if (cropping_flag == 0) {
    dec_info->crop_params.crop_left_offset = 0;
    dec_info->crop_params.crop_top_offset = 0;
    dec_info->crop_params.crop_out_width = dec_info->pic_width;
    dec_info->crop_params.crop_out_height = dec_info->pic_height;
  }

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN && dec_info->mono_chrome)
    dec_info->output_format = DEC_OUT_FRM_MONOCHROME;
  else
    dec_info->output_format = dec_cont->output_format;

  dec_info->bit_depth = ((HevcLumaBitDepth(storage) != 8) || (HevcChromaBitDepth(storage) != 8)) ? 10 : 8;

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN || dec_cont->pp_enabled) {
    if (dec_cont->use_p010_output && dec_info->bit_depth > 8) {
      dec_info->bit_depth = 16;
    } else if (dec_cont->use_8bits_output)
      dec_info->bit_depth = 8;
  }

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_info->pic_stride = NEXT_MULTIPLE(dec_info->pic_width * dec_info->bit_depth, 128) / 8;
  else
    /* Reference buffer. */
    dec_info->pic_stride = dec_info->pic_width * dec_info->bit_depth / 8;

  /* for HDR */
  dec_info->transfer_characteristics = storage->sps[storage->active_sps_id]->vui_parameters.transfer_characteristics;

  return (DEC_OK);
}

/* Releases the decoder instance. Function calls HevcShutDown to
 * release instance data and frees the memory allocated for the
 * instance. */
void HevcDecRelease(HevcDecInst dec_inst) {

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  const void *dwl;
  u32 i;

  if (dec_cont == NULL) {
    return;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return;
  }

#ifdef DUMP_INPUT_STREAM
  if (dec_cont->ferror_stream)
    fclose(dec_cont->ferror_stream);
#endif

  pthread_mutex_destroy(&dec_cont->protect_mutex);
  dwl = dec_cont->dwl;

  /* make sure all in sync in multicore mode, hw idle, output empty */
  if(dec_cont->b_mc) {
    HevcMCWaitPicReadyAll(dec_cont);
  } else {
    u32 i;
    const struct DpbStorage *dpb = dec_cont->storage.dpb;

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

  if (dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->hevc_regs[1]);
    DWLReleaseHw(dwl, dec_cont->core_id); /* release HW lock */
    dec_cont->asic_running = 0;

    /* Decrement usage for DPB buffers */
    DecrementDPBRefCount(dec_cont->storage.dpb);
  }
  HevcShutdown(&dec_cont->storage);

  HevcFreeDpb(dec_cont, dec_cont->storage.dpb);
  if (dec_cont->storage.raster_buffer_mgr)
    RbmRelease(dec_cont->storage.raster_buffer_mgr);

  ReleaseAsicBuffers(dec_cont, dec_cont->asic_buff);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  for (i = 0; i <  dec_cont->n_cores; i++)
    ReleaseAsicTileEdgeMems(dec_cont, i);

  ReleaseList(&dec_cont->fb_list);

#ifdef PERFORMANCE_TEST
  SwActivityTraceRelease(&dec_cont->activity);
  printf("SW consumed time = %lld ms\n", dec_cont->activity.active_time/100 - hw_time_use);
#endif

  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}

/* Decode stream data. Calls HevcDecode to do the actual decoding. */
enum DecRet HevcDecDecode(HevcDecInst dec_inst,
                          const struct HevcDecInput *input,
                          struct HevcDecOutput *output) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 strm_len;
  u32 input_data_len = input->data_len; // used to generate error stream
  const u8 *tmp_stream;
  enum DecRet return_value = DEC_STRM_PROCESSED;

  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if(dec_cont->abort) {
    return (DEC_ABORTED);
  }

#ifdef PERFORMANCE_TEST
  SwActivityTraceStartDec(&dec_cont->activity);
#endif

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
        if(dec_cont->stream_consumed_callback.fn && dec_cont->b_mc)
          dec_cont->stream_consumed_callback.fn((u8*)input->stream,
              (void*)dec_cont->stream_consumed_callback.p_user_data);
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
        if(dec_cont->stream_consumed_callback.fn && dec_cont->b_mc)
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
      if (RandomizeBitSwapInStream(input->stream, input->buffer, input->buff_len,
                                   input_data_len,
                                   dec_cont->error_params.swap_bit_odds,
                                   dec_cont->use_ringbuffer)) {
        DEBUG_PRINT(("Bitswap randomizer error (wrong config?)\n"));
      }
    }
  }
#endif

  if (input->data_len == 0 || input->data_len > DEC_X170_MAX_STREAM_G2 ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->buffer) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(input->buffer_bus_address)) {
    return DEC_PARAM_ERROR;
  }

  dec_cont->stream_pos_updated = 0;
  output->strm_curr_pos = NULL;
  dec_cont->hw_stream_start_bus = input->stream_bus_address;
  dec_cont->hw_buffer_start_bus = input->buffer_bus_address;
  dec_cont->hw_stream_start = input->stream;
  dec_cont->hw_buffer = input->buffer;
  strm_len = dec_cont->hw_length = input_data_len;
  if(dec_cont->low_latency && !stream_info.last_flag)
    strm_len = dec_cont->hw_length = DEC_X170_MAX_STREAM;
  /* For low latency mode, strmLen is set as a large value */
  dec_cont->hw_buffer_length = input->buff_len;
  tmp_stream = input->stream;
  dec_cont->stream_consumed_callback.p_strm_buff = input->stream;
  dec_cont->stream_consumed_callback.p_user_data = input->p_user_data;

  /* If there are more buffers to be allocated or to be freed, waiting for buffers ready. */
  if (dec_cont->buf_to_free != NULL ||
      (dec_cont->next_buf_size != 0  && dec_cont->ext_buffer_num < dec_cont->min_buffer_num) ||
      dec_cont->rbm_release) {
    return_value = DEC_WAITING_FOR_BUFFER;
    goto end;
  }

  do {
    u32 dec_result;
    u32 num_read_bytes = 0;
    struct Storage *storage = &dec_cont->storage;

    if (dec_cont->dec_state == HEVCDEC_NEW_HEADERS) {
      dec_result = HEVC_HDRS_RDY;
      dec_cont->dec_state = HEVCDEC_INITIALIZED;
    } else if (dec_cont->dec_state == HEVCDEC_BUFFER_EMPTY) {
      DEBUG_PRINT(("HevcDecDecode: Skip HevcDecode\n"));
      DEBUG_PRINT(("HevcDecDecode: Jump to HEVC_PIC_RDY\n"));
      /* update stream pointers */
      struct StrmData *strm = dec_cont->storage.strm;
      strm->strm_buff_start = tmp_stream;
      strm->strm_buff_size = strm_len;
      strm->strm_curr_pos = tmp_stream;
      strm->strm_data_size = strm_len;

      dec_result = HEVC_PIC_RDY;
    }
    else if (dec_cont->dec_state == HEVCDEC_WAITING_FOR_BUFFER) {
      DEBUG_PRINT(("HevcDecDecode: Skip HevcDecode\n"));
      DEBUG_PRINT(("HevcDecDecode: Jump to HEVC_PIC_RDY\n"));

      dec_result = HEVC_BUFFER_NOT_READY;

    } else {
      dec_result = HevcDecode(dec_cont, tmp_stream, strm_len, input->pic_id,
                              &num_read_bytes);
      if (stream_info.low_latency && stream_info.last_flag) {
        input_data_len = stream_info.send_len;
        strm_len = stream_info.send_len;
        dec_cont->hw_length = stream_info.send_len;
      }
      if (num_read_bytes > strm_len)
        num_read_bytes = strm_len;

      ASSERT(num_read_bytes <= strm_len);
    }

    tmp_stream += num_read_bytes;
    if(tmp_stream >= dec_cont->hw_buffer + dec_cont->hw_buffer_length && dec_cont->use_ringbuffer)
      tmp_stream -= dec_cont->hw_buffer_length;
    strm_len -= num_read_bytes;

    switch (dec_result) {
    /* Split the old case HEVC_HDRS_RDY into case HEVC_HDRS_RDY & HEVC_BUFFER_NOT_READY. */
    /* In case HEVC_BUFFER_NOT_READY, we will allocate resources. */
    case HEVC_HDRS_RDY: {
      /* If both the the size and number of buffers allocated are enough,
       * decoding will continue as normal.
       */
      dec_cont->reset_dpb_done = 0;

      if (storage->dpb->flushed && storage->dpb->num_out) {
        /* output first all DPB stored pictures */
        storage->dpb->flushed = 0;
        dec_cont->dec_state = HEVCDEC_NEW_HEADERS;
        return_value = DEC_PENDING_FLUSH;
        strm_len = 0;
        break;
      }

      /* Make sure that all frame buffers are not in use before
       * reseting DPB (i.e. all HW cores are idle and all output
       * processed) */
      if(dec_cont->b_mc)
        HevcMCWaitPicReadyAll(dec_cont);

#ifdef USE_OMXIL_BUFFER
      if (dec_cont->output_format == DEC_OUT_FRM_TILED_4X4) {
        MarkListNotInUse(&dec_cont->fb_list);
      }
#endif
      WaitListNotInUse(&dec_cont->fb_list);
      WaitOutputEmpty(&dec_cont->fb_list);

#ifndef USE_OMXIL_BUFFER
      if (dec_cont->storage.raster_buffer_mgr) {
        u32 ret = RbmWaitPpBufferNotUsed(dec_cont->storage.raster_buffer_mgr);
        if (ret) {
          strm_len = 0;
          dec_cont->dec_state = HEVCDEC_NEW_HEADERS;
          return_value = DEC_PENDING_FLUSH;
          break;
        }
      }
#endif
      PushOutputPic(&dec_cont->fb_list, NULL, -2);

      if (!HevcSpsSupported(dec_cont)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;
        output->data_left = 0;

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                (void*)dec_cont->stream_consumed_callback.p_user_data);
        }

        return_value = DEC_STREAM_NOT_SUPPORTED;

        strm_len = 0;
        dec_cont->dpb_mode = DEC_DPB_DEFAULT;
#ifdef DUMP_INPUT_STREAM
        fwrite(input->stream, 1, input_data_len, dec_cont->ferror_stream);
#endif
#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 0;
#endif
        return return_value;
      } else {
        dec_result = HEVC_BUFFER_NOT_READY;
        dec_cont->dec_state = HEVCDEC_WAITING_FOR_BUFFER;
        strm_len = 0;
        return_value = DEC_HDRS_RDY;
      }

      strm_len = 0;
      dec_cont->dpb_mode = DEC_DPB_DEFAULT;
      break;
    }
    case HEVC_BUFFER_NOT_READY: {
      i32 ret;

      HevcCheckBufferRealloc(dec_cont, storage);

      if (storage->realloc_ext_buf) {
        HevcSetExternalBufferInfo(dec_cont, storage);
      }

      if (dec_cont->pp_enabled) {
        dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
        dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
      }

      struct HevcDecAsic *asic = dec_cont->asic_buff;

      /* Move ReleaseAsicBuffers from HevcAllocateResources() to here, for when
         enabling ASIC_TRACE_SUPPORT, misc_linear buffer is allocated by DWLMallocRefFrm,
         which needs to be freed before freeing reference buffer. Otherwise, DPB
         linear buffer may be not enough for some cases. */
      ReleaseAsicBuffers(dec_cont, asic);

      ret = HevcAllocateSwResources(dec_cont->dwl, storage, dec_cont, dec_cont->n_cores);
      if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

      ret = HevcAllocateResources(dec_cont);
      if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

#if 0
      u32 max_id = dec_cont->storage.dpb->no_reordering ? 1 :
                   dec_cont->storage.dpb->dpb_size;

      /* Reset multibuffer status */
      HevcPpMultiInit(dec_cont, max_id);
#endif

RESOURCE_NOT_READY:
      if (ret) {
        if (ret == DEC_WAITING_FOR_BUFFER)
          return_value = ret;
        else {
          /* TODO: miten viewit */
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;

          return_value = DEC_MEMFAIL;          /* signal that decoder failed to init parameter sets */
        }

        strm_len = 0;

        //dec_cont->dpb_mode = DEC_DPB_DEFAULT;

      } else {
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        //return_value = DEC_HDRS_RDY;
      }

      /* Reset strm_len only for base view -> no HDRS_RDY to
       * application when param sets activated for stereo view */
      //strm_len = 0;

      //dec_cont->dpb_mode = DEC_DPB_DEFAULT;

      /* Initialize tiled mode */
#if 0
      if( dec_cont->tiled_mode_support &&
          DecCheckTiledMode( dec_cont->tiled_mode_support,
                             dec_cont->dpb_mode, 0) != HANTRO_OK ) {
        return_value = DEC_PARAM_ERROR;
      }
#endif

      break;
    }

#ifdef GET_FREE_BUFFER_NON_BLOCK
    case HEVC_NO_FREE_BUFFER:
      tmp_stream = input->stream;
      strm_len = 0;
      return_value = DEC_NO_DECODING_BUFFER;
      break;
#endif

    case HEVC_PIC_RDY: {
      u32 asic_status;
      u32 picture_broken;
      u32 prev_irq_buffer = dec_cont->dec_state == HEVCDEC_BUFFER_EMPTY; /* entry due to IRQ_BUFFER */
      struct HevcDecAsic *asic_buff = dec_cont->asic_buff;

      picture_broken = (storage->picture_broken && storage->intra_freeze &&
                        !IS_RAP_NAL_UNIT(storage->prev_nal_unit));

      if (dec_cont->dec_state != HEVCDEC_BUFFER_EMPTY && !picture_broken) {
        /* setup the reference frame list; just at picture start */
        if (!HevcPpsSupported(dec_cont)) {
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;

          return_value = DEC_STREAM_NOT_SUPPORTED;
          goto end;
        }

        asic_buff->out_buffer = storage->curr_image->data;
        asic_buff->out_pp_buffer = storage->curr_image->pp_data;
        asic_buff->chroma_qp_index_offset = storage->active_pps->cb_qp_offset;
        asic_buff->chroma_qp_index_offset2 =
          storage->active_pps->cr_qp_offset;

        IncrementDPBRefCount(dec_cont->storage.dpb);

        /* Move below preocess after core_id reserved */
#if 0
        /* Allocate memory for asic filter or reallocate in case old
           one is too small. */
        if (AllocateAsicTileEdgeMems(dec_cont) != HANTRO_OK) {
          return_value = DEC_MEMFAIL;
          goto end;
        }
#endif

        HevcSetRegs(dec_cont);

        /* determine initial reference picture lists */
        HevcInitRefPicList(dec_cont);

        DEBUG_PRINT(("Save DPB status\n"));
        /* we trust our memcpy; ignore return value */
        (void)DWLmemcpy(&storage->dpb[1], &storage->dpb[0],
                        sizeof(*storage->dpb));

        DEBUG_PRINT(("Save POC status\n"));
        (void)DWLmemcpy(&storage->poc[1], &storage->poc[0],
                        sizeof(*storage->poc));

        /* create output picture list */
        HevcUpdateAfterPictureDecode(dec_cont);

        /* enable output writing by default */
        dec_cont->asic_buff->disable_out_writing = 0;

        /* prepare PP if needed */
        /*HevcPreparePpRun(dec_cont);*/
      } else {
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
      }

      /* run asic and react to the status */
      if (!picture_broken) {
        asic_status = HevcRunAsic(dec_cont, asic_buff);
        if (asic_status == DEC_MEMFAIL) {
          return_value = DEC_MEMFAIL;
          goto end;
        }
      } else {
        if (dec_cont->storage.pic_started) {
#ifdef USE_FAST_EC
          if(!dec_cont->storage.fast_freeze)
#endif
            HevcInitPicFreezeOutput(dec_cont, 0);
          HevcUpdateAfterPictureDecode(dec_cont);
        }
        asic_status = DEC_HW_IRQ_ERROR;
      }

#if 0
      printf("asic_status = %2d, slice type = %c, pic_order_cnt_lsb = %d\n",
             asic_status,
             IS_I_SLICE(storage->slice_header->slice_type) ? 'I' :
             IS_B_SLICE(storage->slice_header->slice_type) ? 'B' :
             IS_P_SLICE(storage->slice_header->slice_type) ? 'P' : '-',
             storage->slice_header->pic_order_cnt_lsb);
#endif

#ifdef RANDOM_CORRUPT_RFC
      /* Only corrupt "good" picture. */
      if (asic_status == DEC_HW_IRQ_RDY)
        HevcCorruptRFC(dec_cont);
#endif

      if (!dec_cont->asic_running && !picture_broken && !dec_cont->b_mc)
        DecrementDPBRefCount(dec_cont->storage.dpb);

      /* Handle system error situations */
      if (asic_status == X170_DEC_TIMEOUT) {
        /* This timeout is DWL(software/os) generated */
        return DEC_HW_TIMEOUT;
      } else if (asic_status == X170_DEC_SYSTEM_ERROR) {
        return DEC_SYSTEM_ERROR;
      } else if (asic_status == X170_DEC_FATAL_SYSTEM_ERROR) {
        return DEC_FATAL_SYSTEM_ERROR;
      } else if (asic_status == X170_DEC_HW_RESERVED) {
        return DEC_HW_RESERVED;
      }

      /* Handle possible common HW error situations */
      if (asic_status & DEC_HW_IRQ_BUS) {
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        if (stream_info.low_latency) {
          while (!stream_info.last_flag)
            sched_yield();
          input_data_len = stream_info.send_len;
        }
        output->data_left = input_data_len;
        return DEC_HW_BUS_ERROR;
      }
      /* Handle stream error dedected in HW */
      else if ((asic_status & DEC_HW_IRQ_TIMEOUT) ||
               (asic_status & DEC_HW_IRQ_ERROR)) {
        /* This timeout is HW generated */
        if (asic_status & DEC_HW_IRQ_TIMEOUT) {
          DEBUG_PRINT(("IRQ: HW TIMEOUT\n"));
#ifdef TIMEOUT_ASSERT
          ASSERT(0);
#endif
        } else {
          DEBUG_PRINT(("IRQ: STREAM ERROR dedected\n"));
        }

        if (dec_cont->packet_decoded != HANTRO_TRUE) {
          DEBUG_PRINT(("Reset pic_started\n"));
          dec_cont->storage.pic_started = HANTRO_FALSE;
        }

        dec_cont->storage.picture_broken = HANTRO_TRUE;
#ifndef USE_FAST_EC
        HevcInitPicFreezeOutput(dec_cont, 1);
#else
        if(!dec_cont->storage.fast_freeze)
          HevcInitPicFreezeOutput(dec_cont, 1);
        else
          HevcDropCurrentPicutre(dec_cont);
#endif

        {
          struct StrmData *strm = dec_cont->storage.strm;

          if (prev_irq_buffer) {
            /* Call HevcDecDecode() due to DEC_HW_IRQ_BUFFER,
               reset strm to input buffer. */
            strm->strm_buff_start = input->buffer;
            strm->strm_curr_pos = input->stream;
            strm->strm_buff_size = input->buff_len;
            if (stream_info.low_latency && stream_info.last_flag)
              input_data_len = stream_info.send_len;
            strm->strm_data_size = input_data_len;
            strm->strm_buff_read_bits = (u32)(strm->strm_curr_pos - strm->strm_buff_start) * 8;
            strm->is_rb = dec_cont->use_ringbuffer;;
            strm->remove_emul3_byte = 0;
            strm->bit_pos_in_word = 0;
          }
#ifdef HEVC_INPUT_MULTI_FRM
          if (HevcNextStartCode(strm) == HANTRO_OK) {
            if (stream_info.low_latency && stream_info.last_flag)
              strm_len = stream_info.send_len;
            if(strm->strm_curr_pos >= tmp_stream)
              strm_len -= (strm->strm_curr_pos - tmp_stream);
            else
              strm_len -= (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
            tmp_stream = strm->strm_curr_pos;
          }
#else
          if (stream_info.low_latency) {
            while (!stream_info.last_flag)
              sched_yield();
            input_data_len = stream_info.send_len;
          }
          tmp_stream = input->stream + input_data_len;
#endif
        }

        dec_cont->stream_pos_updated = 0;
      } else if (asic_status & DEC_HW_IRQ_BUFFER) {
        /* TODO: Need to check for CABAC zero words here? */
        DEBUG_PRINT(("IRQ: BUFFER EMPTY\n"));

        /* a packet successfully decoded, don't Reset pic_started flag if
         * there is a need for rlc mode */
        dec_cont->dec_state = HEVCDEC_BUFFER_EMPTY;
        dec_cont->packet_decoded = HANTRO_TRUE;
        output->data_left = 0;

#ifdef DUMP_INPUT_STREAM
        fwrite(input->stream, 1, input_data_len, dec_cont->ferror_stream);
#endif
#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 0;
#endif
        return DEC_BUF_EMPTY;
      } else { /* OK in here */
        if (IS_RAP_NAL_UNIT(storage->prev_nal_unit)) {
          dec_cont->storage.picture_broken = HANTRO_FALSE;
        }
#if 1
        if (!dec_cont->b_mc && !dec_cont->secure_mode) {
          /* CHECK CABAC WORDS */
          struct StrmData strm_tmp = *dec_cont->storage.strm;
          u32 consumed = dec_cont->hw_stream_start -
                  (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8);

          strm_tmp.strm_curr_pos = dec_cont->hw_stream_start;
          strm_tmp.strm_buff_read_bits = 8*consumed;
          strm_tmp.bit_pos_in_word = 0;
          if(dec_cont->low_latency && stream_info.last_flag)
            strm_tmp.strm_data_size = stream_info.send_len;
          if (strm_tmp.strm_data_size - consumed > CHECK_TAIL_BYTES) {
            /* Do not check CABAC zero words if remaining bytes are too few. */
            u32 tmp = HevcCheckCabacZeroWords( &strm_tmp );
            if( tmp != HANTRO_OK ) {
              if (dec_cont->packet_decoded != HANTRO_TRUE) {
                DEBUG_PRINT(("Reset pic_started\n"));
                dec_cont->storage.pic_started = HANTRO_FALSE;
              }

              dec_cont->storage.picture_broken = HANTRO_TRUE;
#ifndef USE_FAST_EC
              HevcInitPicFreezeOutput(dec_cont, 1);
#else
              if(!dec_cont->storage.fast_freeze)
                HevcInitPicFreezeOutput(dec_cont, 1);
              else {
                if (dec_cont->storage.dpb->current_out->to_be_displayed &&
                    dec_cont->storage.dpb->current_out->pic_output_flag)
                  dec_cont->storage.dpb->num_out_pics_buffered--;
                if(dec_cont->storage.dpb->fullness > 0)
                  dec_cont->storage.dpb->fullness--;
                dec_cont->storage.dpb->num_ref_frames--;
                dec_cont->storage.dpb->current_out->to_be_displayed = 0;
                dec_cont->storage.dpb->current_out->status = UNUSED;
                dec_cont->storage.dpb->current_out->pic_order_cnt = 0;
                dec_cont->storage.dpb->current_out->pic_order_cnt_lsb = 0;
                if (dec_cont->storage.raster_buffer_mgr)
                  RbmReturnPpBuffer(storage->raster_buffer_mgr,
                      dec_cont->storage.dpb->current_out->pp_data->virtual_address);
              }
#endif
              {
#ifdef HEVC_INPUT_MULTI_FRM
                struct StrmData *strm = dec_cont->storage.strm;

                if (HevcNextStartCode(strm) == HANTRO_OK) {
                  if (stream_info.low_latency && stream_info.last_flag)
                    strm_len = stream_info.send_len;
                  if(strm->strm_curr_pos >= tmp_stream)
                    strm_len -= (strm->strm_curr_pos - tmp_stream);
                  else
                    strm_len -= (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
                  tmp_stream = strm->strm_curr_pos;
                }
#else
                if (stream_info.low_latency) {
                  while (!stream_info.last_flag)
                    sched_yield();
                  input_data_len = stream_info.send_len;
                }
                tmp_stream = input->stream + input_data_len;
#endif
              }
              dec_cont->stream_pos_updated = 0;
            }
          }
        } else { /* dec_cont->b_mc = 1*/
          tmp_stream = (u8 *)input->stream + input_data_len;
          strm_len = 0;
          dec_cont->stream_pos_updated = 0;
        }
#endif
      }

#if 0
      CHECK CABAC WORDS
      struct StrmData strm_tmp = *dec_cont->storage.strm;
      tmp = dec_cont->hw_stream_start-input->stream;
      strm_tmp.strm_curr_pos = dec_cont->hw_stream_start;
      strm_tmp.strm_buff_read_bits = 8*tmp;
      strm_tmp.bit_pos_in_word = 0;
      strm_tmp.strm_buff_size = input->data_len;
      tmp = HevcCheckCabacZeroWords( &strm_tmp );
      if( tmp != HANTRO_OK ) {
        DEBUG_PRINT(("Error decoding CABAC zero words\n"));
        {
          struct StrmData *strm = dec_cont->storage.strm;
          const u8 *next =
            HevcFindNextStartCode(strm->strm_buff_start,
                                  strm->strm_buff_size);

          if(next != NULL) {
            u32 consumed;

            tmp_stream -= num_read_bytes;
            strm_len += num_read_bytes;

            consumed = (u32) (next - tmp_stream);
            tmp_stream += consumed;
            strm_len -= consumed;
          }
        }

      }
#endif

      /* For the switch between modes */
      dec_cont->packet_decoded = HANTRO_FALSE;
      dec_cont->pic_number++;
      HevcCycleCount(dec_cont);

#ifdef FFWD_WORKAROUND
      storage->prev_idr_pic_ready = IS_IDR_NAL_UNIT(storage->prev_nal_unit);
#endif /* FFWD_WORKAROUND */

      {
        /* C.5.2.3 Additional bumping */
        /* TODO:
         * sps_max_latency_increase_plus1[ HighestTid ] is not equal to 0 and
         * there is at least one picture in the DPB that is marked as "needed
         * for output" for which the associated variable PicLatencyCount that is
         * greater than or equal to SpsMaxLatencyPictures[ HighestTid ].
         */
        u32 sublayer = storage->active_sps->max_sub_layers - 1;
        if (dec_cont->storage.active_sps->max_latency_increase[sublayer]) {
          u32 max_latency =
            dec_cont->storage.active_sps->max_num_reorder_pics[sublayer] +
            dec_cont->storage.active_sps->max_latency_increase[sublayer] - 1;
          HevcDpbCheckMaxLatency2(dec_cont->storage.dpb, max_latency);
        }
#if 0
        u32 max_latency =
          dec_cont->storage.active_sps->max_num_reorder_pics[sublayer] +
          (dec_cont->storage.active_sps->max_latency_increase[sublayer]
           ? dec_cont->storage.active_sps
           ->max_latency_increase[sublayer] -
           1
           : 0);
#else
        u32 max_latency =
          dec_cont->storage.active_sps->max_num_reorder_pics[sublayer];
#endif
        HevcDpbCheckMaxLatency(dec_cont->storage.dpb, max_latency);
      }

      return_value = DEC_PIC_DECODED;
      strm_len = 0;
      break;
    }
    case HEVC_PARAM_SET_ERROR: {
      if (!HevcValidParamSets(&dec_cont->storage) && strm_len == 0) {
        return_value = DEC_STRM_ERROR;
      }

      /* update HW buffers if VLC mode */
      dec_cont->hw_length -= num_read_bytes;
      if(tmp_stream >= input->stream)
        dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
      else
        dec_cont->hw_stream_start_bus =
          input->buffer_bus_address + (u32)(tmp_stream - input->buffer);

      dec_cont->hw_stream_start = tmp_stream;

      /* check active sps is valid or not */
      if (dec_cont->storage.active_sps && !HevcSpsSupported(dec_cont)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                (void*)dec_cont->stream_consumed_callback.p_user_data);
        }

        return_value = DEC_STREAM_NOT_SUPPORTED;
        dec_cont->dpb_mode = DEC_DPB_DEFAULT;
        goto end;
      }

      break;
    }
    case HEVC_NEW_ACCESS_UNIT: {
      dec_cont->stream_pos_updated = 0;

      dec_cont->storage.picture_broken = HANTRO_TRUE;
#ifndef USE_FAST_EC
      HevcInitPicFreezeOutput(dec_cont, 0);
#else
      if(!dec_cont->storage.fast_freeze)
        HevcInitPicFreezeOutput(dec_cont, 0);
      else
        HevcDropCurrentPicutre(dec_cont);
#endif

      HevcUpdateAfterPictureDecode(dec_cont);

      /* PP will run in HevcDecNextPicture() for this concealed picture */
      return_value = DEC_PIC_DECODED;

      dec_cont->pic_number++;
      strm_len = 0;

      break;
    }
    case HEVC_ABORTED:
      dec_cont->dec_state = HEVCDEC_ABORTED;
      return DEC_ABORTED;
    case HEVC_NONREF_PIC_SKIPPED:
      return_value = DEC_NONREF_PIC_SKIPPED;
    /* fall through */
    default: { /* case HEVC_ERROR, HEVC_RDY */
      dec_cont->hw_length -= num_read_bytes;
      if(tmp_stream >= input->stream)
        dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
      else
        dec_cont->hw_stream_start_bus =
          input->buffer_bus_address + (u32)(tmp_stream - input->buffer);

      dec_cont->hw_stream_start = tmp_stream;
    }
    }
  } while (strm_len);

end:

  /*  If Hw decodes stream, update stream buffers from "storage" */
  if (dec_cont->stream_pos_updated) {
    if (dec_cont->secure_mode)
      output->data_left = 0;
    else {
      output->strm_curr_pos = (u8 *)dec_cont->hw_stream_start;
      output->strm_curr_bus_address = dec_cont->hw_stream_start_bus;
      output->data_left = dec_cont->hw_length;
    }
  } else {
    /* else update based on SW stream decode stream values */
    u32 data_consumed = (u32)(tmp_stream - input->stream);
    if(tmp_stream >= input->stream)
      data_consumed = (u32)(tmp_stream - input->stream);
    else
      data_consumed = (u32)(tmp_stream + input->buff_len - input->stream);

    output->strm_curr_pos = (u8 *)tmp_stream;
    output->strm_curr_bus_address = input->stream_bus_address + data_consumed;
    if(output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len))
      output->strm_curr_bus_address -= input->buff_len;

    output->data_left = input_data_len - data_consumed;
  }
  ASSERT(output->strm_curr_bus_address <= (input->buffer_bus_address + input->buff_len));

#ifdef DUMP_INPUT_STREAM
  if (output->strm_curr_pos >= input->stream)
    fwrite(input->stream, 1, (input_data_len - output->data_left), dec_cont->ferror_stream);
  else {
    fwrite(input->stream, 1, (u32)(input->buffer_bus_address + input->buff_len - input->stream_bus_address),
           dec_cont->ferror_stream);

    fwrite(input->buffer, 1, (u32)(output->strm_curr_bus_address - input->buffer_bus_address),
           dec_cont->ferror_stream);
  }
#endif

#ifdef USE_RANDOM_TEST
  if (output->data_left == input_data_len)
    dec_cont->stream_not_consumed = 1;
  else
    dec_cont->stream_not_consumed = 0;
#endif

  if (dec_cont->storage.sei_param.bufperiod_present_flag &&
      dec_cont->storage.sei_param.pictiming_present_flag) {

    if (return_value == DEC_PIC_DECODED) {
      if(output->strm_curr_pos > input->stream)
        dec_cont->storage.sei_param.stream_len = output->strm_curr_pos - input->stream;
      else
        dec_cont->storage.sei_param.stream_len = output->strm_curr_pos + input->buff_len - input->stream;
      dec_cont->storage.sei_param.bumping_flag = 1;
    }
  }

  FinalizeOutputAll(&dec_cont->fb_list);

  if (!dec_cont->b_mc) {
    while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY);
  } else {
    if (return_value == DEC_PIC_DECODED ||
        return_value == DEC_PENDING_FLUSH) {
      HevcMCPushOutputAll(dec_cont);
    } else if(output->data_left == 0) {
      /* release buffer fully processed by SW */
      if(dec_cont->stream_consumed_callback.fn)
        dec_cont->stream_consumed_callback.fn((u8*)input->stream,
            (void*)dec_cont->stream_consumed_callback.p_user_data);
    }
  }

#ifdef PERFORMANCE_TEST
  SwActivityTraceStopDec(&dec_cont->activity);
#endif

    if (dec_cont->abort)
      return (DEC_ABORTED);
    else
      return (return_value);
}

/* Returns the SW and HW build information. */
HevcDecBuild HevcDecGetBuild(void) {
  HevcDecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_HEVC_DEC);

  DWLReadAsicConfig(&build_info.hw_config[0], DWL_CLIENT_TYPE_HEVC_DEC);

  return build_info;
}

/* Updates decoder instance after decoding of current picture */
void HevcUpdateAfterPictureDecode(struct HevcDecContainer *dec_cont) {

  struct Storage *storage = &dec_cont->storage;

  HevcResetStorage(storage);

  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
}

/* Checks if active SPS is valid, i.e. supported in current profile/level */
u32 HevcSpsSupported(const struct HevcDecContainer *dec_cont) {
  const struct SeqParamSet *sps = dec_cont->storage.active_sps;

  /* check picture size (minimum defined in decapicommon.h) */
  if (sps->pic_width > dec_cont->max_dec_pic_width ||
      sps->pic_height > dec_cont->max_dec_pic_height ||
      sps->pic_width < MIN_PIC_WIDTH || sps->pic_height < MIN_PIC_HEIGHT) {
    DEBUG_PRINT(("Picture size not supported!\n"));
    return 0;
  }

  /* check hevc main 10 profile supported or not*/
  if (((sps->bit_depth_luma != 8) || (sps->bit_depth_chroma != 8)) &&
      !dec_cont->hevc_main10_support) {
    DEBUG_PRINT(("Hevc main 10 profile not supported!\n"));
    return 0;
  }

  return 1;
}

/* Checks if active PPS is valid, i.e. supported in current profile/level */
u32 HevcPpsSupported(const struct HevcDecContainer *dec_cont) {
  return dec_cont ? 1 : 0;
}

/* Allocates necessary memory buffers. */
u32 HevcAllocateResources(struct HevcDecContainer *dec_cont) {
  u32 ret;
  struct HevcDecAsic *asic = dec_cont->asic_buff;
  struct Storage *storage = &dec_cont->storage;
  const struct SeqParamSet *sps = storage->active_sps;

  SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_WIDTH_IN_CBS,
                 storage->pic_width_in_cbs);
  SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_HEIGHT_IN_CBS,
                 storage->pic_height_in_cbs);

  {
    u32 ctb_size = 1 << sps->log_max_coding_block_size;
    u32 pic_width_in_ctbs = storage->pic_width_in_ctbs * ctb_size;
    u32 pic_height_in_ctbs = storage->pic_height_in_ctbs * ctb_size;

    u32 partial_ctb_h = sps->pic_width != pic_width_in_ctbs ? 1 : 0;
    u32 partial_ctb_v = sps->pic_height != pic_height_in_ctbs ? 1 : 0;

    SetDecRegister(dec_cont->hevc_regs, HWIF_PARTIAL_CTB_X, partial_ctb_h);
    SetDecRegister(dec_cont->hevc_regs, HWIF_PARTIAL_CTB_Y, partial_ctb_v);

    u32 min_cb_size = 1 << sps->log_min_coding_block_size;
    SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_WIDTH_4X4,
                   (storage->pic_width_in_cbs * min_cb_size) >> 2);

    SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_HEIGHT_4X4,
                   (storage->pic_height_in_cbs * min_cb_size) >> 2);
  }

  ret = AllocateAsicBuffers(dec_cont, asic);
#if 0
  if (ret == HANTRO_OK) {
    ret = AllocateAsicTileEdgeMems(dec_cont);
  }
#endif

  return ret;
}

/* Performs picture freeze for output. */
void HevcInitPicFreezeOutput(struct HevcDecContainer *dec_cont,
                             u32 from_old_dpb) {

  u32 index = 0;
  const u8 *ref_data;
  struct Storage *storage = &dec_cont->storage;
  const u32 dvm_mem_size = storage->dmv_mem_size;
  void *dvm_base = (u8 *)storage->curr_image->data->virtual_address +
                   dec_cont->storage.dpb->dir_mv_offset;

#ifdef _DISABLE_PIC_FREEZE
  return;
#endif

  /* for concealment after a HW error report we use the saved reference list */
  struct DpbStorage *dpb = &storage->dpb[from_old_dpb];

  do {
    ref_data = HevcGetRefPicData(dpb, dpb->list[index]);
    index++;
  } while (index < dpb->dpb_size && ref_data == NULL);

  ASSERT(dpb->current_out->data != NULL);

  /* Reset DMV storage for erroneous pictures */
  (void)DWLmemset(dvm_base, 0, dvm_mem_size);

  if (ref_data == NULL) {
    DEBUG_PRINT(("HevcInitPicFreezeOutput: pic freeze memset\n"));
    //(void)DWLPrivateAreaMemset(storage->curr_image->data->virtual_address, 128,
    //                storage->pic_size * 3 / 2);
    {
      if (dec_cont->storage.dpb->current_out->to_be_displayed &&
          dec_cont->storage.dpb->current_out->pic_output_flag)
        dec_cont->storage.dpb->num_out_pics_buffered--;
      if(dec_cont->storage.dpb->fullness > 0)
        dec_cont->storage.dpb->fullness--;
      dec_cont->storage.dpb->num_ref_frames--;
      dec_cont->storage.dpb->current_out->to_be_displayed = 0;
      dec_cont->storage.dpb->current_out->status = UNUSED;
      if (storage->raster_buffer_mgr)
        RbmReturnPpBuffer(storage->raster_buffer_mgr, dec_cont->storage.dpb->current_out->pp_data->virtual_address);
    }
  } else {
    DEBUG_PRINT(("HevcInitPicFreezeOutput: pic freeze memcopy\n"));
    (void)DWLPrivateAreaMemcpy(storage->curr_image->data->virtual_address, ref_data,
                               storage->pic_size +
                               NEXT_MULTIPLE(storage->pic_size / 2, MAX(16, ALIGN(dec_cont->align))));
    /* Copy compression table when existing. */
    if (dec_cont->use_video_compressor) {
      (void)DWLPrivateAreaMemcpy((u8 *)storage->curr_image->data->virtual_address + dpb->cbs_tbl_offsety,
                                 ref_data + dpb->cbs_tbl_offsety,
                                 dpb->cbs_tbl_size);
    }
  }

  dpb = &storage->dpb[0]; /* update results for current output */

  {
    i32 i = dpb->num_out;
    u32 tmp = dpb->out_index_r;

    while ((i--) > 0) {
      if (tmp == MAX_DPB_SIZE + 1) tmp = 0;

      if (dpb->out_buf[tmp].data == storage->curr_image->data) {
        dpb->out_buf[tmp].num_err_mbs = /* */ 1;
        break;
      }
      tmp++;
    }

    i = dpb->dpb_size + 1;

    while ((i--) > 0) {
      if (dpb->buffer[i].data == storage->curr_image->data) {
        ASSERT(&dpb->buffer[i] == dpb->current_out);
        break;
      }
    }
  }
}

/* Returns the sample aspect ratio info */
void HevcGetSarInfo(const struct Storage *storage, u32 *sar_width,
                    u32 *sar_height) {
  switch (HevcAspectRatioIdc(storage)) {
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
    HevcSarSize(storage, sar_width, sar_height);
    break;
  default:
    *sar_width = 0;
    *sar_height = 0;
  }
}

/* Get last decoded picture if any available. No pictures are removed
 * from output nor DPB buffers. */
enum DecRet HevcDecPeek(HevcDecInst dec_inst, struct HevcDecPicture *output) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  struct DpbPicture *current_out = dec_cont->storage.dpb->current_out;

  if (dec_inst == NULL || output == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state != HEVCDEC_NEW_HEADERS &&
      dec_cont->storage.dpb->fullness && current_out != NULL &&
      current_out->status != EMPTY) {

    u32 cropping_flag;

    output->pictures[0].output_picture = current_out->data->virtual_address;
    output->pictures[0].output_picture_bus_address = current_out->data->bus_address;
    output->pic_id = current_out->pic_id;
    output->decode_id = current_out->decode_id;
    output->is_idr_picture = current_out->is_idr;

    output->pictures[0].pic_width = HevcPicWidth(&dec_cont->storage);
    output->pictures[0].pic_height = HevcPicHeight(&dec_cont->storage);

    HevcCroppingParams(&dec_cont->storage, &cropping_flag,
                       &output->crop_params.crop_left_offset,
                       &output->crop_params.crop_out_width,
                       &output->crop_params.crop_top_offset,
                       &output->crop_params.crop_out_height);

    if (cropping_flag == 0) {
      output->crop_params.crop_left_offset = 0;
      output->crop_params.crop_top_offset = 0;
      output->crop_params.crop_out_width = output->pictures[0].pic_width;
      output->crop_params.crop_out_height = output->pictures[0].pic_height;
    }

    return (DEC_PIC_RDY);
  } else {
    return (DEC_OK);
  }
}

enum DecRet HevcDecNextPictureInternal(struct HevcDecContainer *dec_cont) {
  struct HevcDecPicture out_pic;
  struct HevcDecInfo dec_info;
  const struct DpbOutPicture *dpb_out = NULL;
  u32 bit_depth, out_bit_depth;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  //u32 pic_width, pic_height;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  const u32 *output_picture = NULL;
  u32 i;
  memset(&out_pic, 0, sizeof(struct HevcDecPicture));

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  dpb_out = HevcNextOutputPicture(&dec_cont->storage);

  if (dpb_out == NULL) return DEC_OK;

  //pic_width = dpb_out->pic_width;
  //pic_height = dpb_out->pic_height;
  bit_depth = (dpb_out->bit_depth_luma == 8 && dpb_out->bit_depth_chroma == 8) ? 8 : 10;
  out_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8) ? 8 :
                  dec_cont->use_p010_output ? 16 : 10;
  out_pic.bit_depth_luma = dpb_out->bit_depth_luma;
  out_pic.bit_depth_chroma = dpb_out->bit_depth_chroma;
  out_pic.pic_id = dpb_out->pic_id;
  out_pic.decode_id = dpb_out->decode_id;
  out_pic.is_idr_picture = dpb_out->is_idr;
  out_pic.pic_corrupt = dpb_out->num_err_mbs ? 1 : 0;
  out_pic.crop_params = dpb_out->crop_params;
  out_pic.cycles_per_mb = dpb_out->cycles_per_mb;
  out_pic.pp_enabled = 0;
  if (dec_cont->storage.pp_enabled) {
    out_pic.pp_enabled = 1;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
      out_pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      out_pic.pictures[i].pic_stride = ppu_cfg->ystride;
      out_pic.pictures[i].pic_stride_ch = ppu_cfg->cstride;
      if (ppu_cfg->crop2.enabled) {
        out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].crop2.width / 2) << 1;
        out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].crop2.height / 2) << 1;
      } else {
        out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].scale.width / 2) << 1;
        out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].scale.height / 2) << 1;
      }
      out_pic.pictures[i].output_picture = (u32*)((addr_t)dpb_out->pp_data->virtual_address + ppu_cfg->luma_offset);
      out_pic.pictures[i].output_picture_bus_address = dpb_out->pp_data->bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        out_pic.pictures[i].output_picture_chroma = (u32*)((addr_t)dpb_out->pp_data->virtual_address + ppu_cfg->chroma_offset);
        out_pic.pictures[i].output_picture_chroma_bus_address = dpb_out->pp_data->bus_address + ppu_cfg->chroma_offset;
      } else {
        out_pic.pictures[i].output_picture_chroma = NULL;
        out_pic.pictures[i].output_picture_chroma_bus_address = 0;
      }
      if (output_picture == NULL)
        output_picture = out_pic.pictures[i].output_picture;
    }
  } else {
    out_pic.pictures[0].output_format =
      dec_cont->use_video_compressor ? DEC_OUT_FRM_RFC : dec_cont->output_format;
    out_pic.pictures[0].pic_height = dpb_out->pic_height;
    out_pic.pictures[0].pic_width = dpb_out->pic_width;
    if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
      out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(dpb_out->pic_width * out_bit_depth,
                                         ALIGN(dec_cont->align) * 8) / 8;
      out_pic.pictures[0].output_picture = dpb_out->pp_data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address = dpb_out->pp_data->bus_address;
      if (dpb_out->mono_chrome) {
	out_pic.pictures[0].output_picture_chroma = NULL;
        out_pic.pictures[0].output_picture_chroma_bus_address = 0;
      } else {
        out_pic.pictures[0].output_picture_chroma = dpb_out->pp_data->virtual_address +
            out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
        out_pic.pictures[0].output_picture_chroma_bus_address = dpb_out->pp_data->bus_address +
            out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height;
      }
    } else {
      out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                                         ALIGN(dec_cont->align) * 8) / 8;
      out_pic.pictures[0].output_picture = dpb_out->data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address = dpb_out->data->bus_address;
      if (dpb_out->mono_chrome) {
        out_pic.pictures[0].output_picture_chroma = NULL;
        out_pic.pictures[0].output_picture_chroma_bus_address = 0;
      } else {
        out_pic.pictures[0].output_picture_chroma = dpb_out->data->virtual_address +
            out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4 / 4;
        out_pic.pictures[0].output_picture_chroma_bus_address = dpb_out->data->bus_address +
            out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
      }
      if (dec_cont->use_video_compressor) {
        /* No alignment when compressor is enabled. */
        if (hw_feature.rfc_stride_support) {
          out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * dpb_out->pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
          out_pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
        } else {
          out_pic.pictures[0].pic_stride = 4 * dpb_out->pic_width * bit_depth / 8;
          out_pic.pictures[0].pic_stride_ch = 4 * dpb_out->pic_width * bit_depth / 8;
        }
      }
    }
    if (out_pic.pictures[0].output_format != DEC_OUT_FRM_RFC)
      out_pic.pictures[0].pic_stride_ch = out_pic.pictures[0].pic_stride;
    out_pic.output_rfc_luma_base = dpb_out->data->virtual_address +
                                   dec_cont->storage.dpb[0].cbs_tbl_offsety / 4;
    out_pic.output_rfc_luma_bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsety;
    if (dpb_out->mono_chrome) {
      out_pic.output_rfc_chroma_base = NULL;
      out_pic.output_rfc_chroma_bus_address = 0;
    } else {
      out_pic.output_rfc_chroma_base = dpb_out->data->virtual_address +
                                       dec_cont->storage.dpb[0].cbs_tbl_offsetc / 4;
      out_pic.output_rfc_chroma_bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsetc;
    }
    ASSERT(out_pic.pictures[0].output_picture);
    ASSERT(out_pic.pictures[0].output_picture_bus_address);
  }

  (void)HevcDecGetInfo((HevcDecInst)dec_cont, &dec_info);
  (void)DWLmemcpy(&out_pic.dec_info, &dec_info, sizeof(struct HevcDecInfo));
  out_pic.dec_info.pic_buff_size = dec_cont->storage.dpb->tot_buffers;

  if (dec_cont->storage.raster_buffer_mgr)
    RbmSetPpBufferUsed(dec_cont->storage.raster_buffer_mgr, output_picture);

  PushOutputPic(&dec_cont->fb_list, &out_pic, dpb_out->mem_idx);

  /* If reference buffer is not external, consume it and return it to DPB list. */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    PopOutputPic(&dec_cont->fb_list, dpb_out->mem_idx);
  return (DEC_PIC_RDY);
}

enum DecRet HevcDecNextPicture(HevcDecInst dec_inst,
                               struct HevcDecPicture *picture) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 ret;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state == HEVCDEC_EOS && IsOutputEmpty(&dec_cont->fb_list)) {
    return (DEC_END_OF_STREAM);
  }

  if ((ret = PeekOutputPic(&dec_cont->fb_list, picture))) {
    /*Abort output fifo */
    if(ret == ABORT_MARKER)
      return (DEC_ABORTED);
    if(ret == FLUSH_MARKER)
      return (DEC_FLUSHED);
    /* For external buffer mode, following pointers are ready: */
    /*
         picture->output_picture
         picture->output_picture_bus_address
         picture->output_downscale_picture
         picture->output_downscale_picture_bus_address
         picture->output_downscale_picture_chroma
         picture->output_downscale_picture_chroma_bus_address
     */
    return (DEC_PIC_RDY);
  } else {
    return (DEC_OK);
  }
}

/*!\brief Mark last output picture consumed
 *
 * Application calls this after it has finished processing the picture
 * returned by HevcDecMCNextPicture.
 */
enum DecRet HevcDecPictureConsumed(HevcDecInst dec_inst,
                                   const struct HevcDecPicture *picture) {
  u32 id, i;
  const struct DpbStorage *dpb;
  struct HevcDecPicture pic;
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  struct Storage *storage = &dec_cont->storage;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  dpb = dec_cont->storage.dpb;
  pic = *picture;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    /* If it's external reference buffer, consumed it as usual.*/
    /* find the mem descriptor for this specific buffer */
    for (id = 0; id < dpb->tot_buffers; id++) {
      if (pic.pictures[0].output_picture_bus_address == dpb->pic_buffers[id].bus_address &&
          pic.pictures[0].output_picture == dpb->pic_buffers[id].virtual_address) {
        break;
      }
    }

    /* check that we have a valid id */
    if (id >= dpb->tot_buffers) return DEC_PARAM_ERROR;

    PopOutputPic(&dec_cont->fb_list, dpb->pic_buff_id[id]);


  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = picture->pictures[i].output_picture;
        break;
      }
    }
    /* For raster/dscale buffer, return to input buffer queue. */
    if (storage->raster_buffer_mgr) {
      if (RbmReturnPpBuffer(storage->raster_buffer_mgr, output_picture) == NULL)
        return DEC_PARAM_ERROR;
    }
  }

  return DEC_OK;
}

enum DecRet HevcDecEndOfStream(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  i32 core_id[MAX_ASIC_CORES], count;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* No need to call endofstream twice */
  if(dec_cont->dec_state == HEVCDEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl);
  } else {
    if (dec_cont->n_cores > 1) {
      /* Check all Core in idle state */
      for(count = 0; count < dec_cont->n_cores_available; count++) {
        DWLReserveHw(dec_cont->dwl, &core_id[count], DWL_CLIENT_TYPE_HEVC_DEC);
      }
      /* All HW Core finished */
      for(count = 0; count < dec_cont->n_cores_available; count++) {
        DWLReleaseHw(dec_cont->dwl, core_id[count]);
      }
    } else if (dec_cont->asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->hevc_regs[1]);

      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id); /* release HW lock */

      DecrementDPBRefCount(dec_cont->storage.dpb);

      dec_cont->asic_running = 0;
    }
  }

  /* flush any remaining pictures form DPB */
  HevcFlushBuffer(&dec_cont->storage);

  FinalizeOutputAll(&dec_cont->fb_list);

  while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;

  /* After all output pictures were pushed, update decoder status to
   * reflect the end-of-stream situation. This way the HevcDecNextPicture
   * will not block anymore once all output was handled.
   */
  dec_cont->dec_state = HEVCDEC_EOS;

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, -1);

#ifndef HEVC_EXT_BUF_SAFE_RELEASE
  if (dec_cont->output_format == DEC_OUT_FRM_TILED_4X4) {

    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_FRAME_BUFFER_NUMBER; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
  }
#endif

  /* block until all output is handled */
  WaitListNotInUse(&dec_cont->fb_list);
#ifndef USE_OMXIL_BUFFER
  if (dec_cont->storage.raster_buffer_mgr)
    RbmWaitPpBufferNotUsed(dec_cont->storage.raster_buffer_mgr);
#endif
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  return (DEC_OK);
}

enum DecRet HevcDecUseExtraFrmBuffers(HevcDecInst dec_inst, u32 n) {

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;

  dec_cont->storage.n_extra_frm_buffers = n;

  return DEC_OK;
}

void HevcCycleCount(struct HevcDecContainer *dec_cont) {
  struct DpbPicture *current_out = dec_cont->storage.dpb->current_out;
  u32 cycles = 0;
  u32 mbs = (HevcPicWidth(&dec_cont->storage) *
             HevcPicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->hevc_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  current_out->cycles_per_mb = cycles;
}

#ifdef USE_FAST_EC
void HevcDropCurrentPicutre(struct HevcDecContainer *dec_cont) {
  struct Storage *storage = &dec_cont->storage;
  if (dec_cont->storage.dpb->current_out->to_be_displayed &&
      dec_cont->storage.dpb->current_out->pic_output_flag)
    dec_cont->storage.dpb->num_out_pics_buffered--;
  if(dec_cont->storage.dpb->fullness > 0)
    dec_cont->storage.dpb->fullness--;
  dec_cont->storage.dpb->num_ref_frames--;
  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
  dec_cont->storage.dpb->current_out->status = UNUSED;
  if (storage->raster_buffer_mgr)
    RbmReturnPpBuffer(storage->raster_buffer_mgr, dec_cont->storage.dpb->current_out->pp_data->virtual_address);
  if (dec_cont->storage.no_reordering) {
    dec_cont->storage.dpb->num_out--;
    if (dec_cont->storage.dpb->out_index_w == 0)
      dec_cont->storage.dpb->out_index_w = MAX_DPB_SIZE;
    else
      dec_cont->storage.dpb->out_index_w--;
    ClearOutput(dec_cont->storage.dpb->fb_list, dec_cont->storage.dpb->current_out->mem_idx);
  }
  (void)storage;
}
#endif

enum DecRet HevcDecAddBuffer(HevcDecInst dec_inst,
                             struct DWLLinearMem *info) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
  enum DecRet dec_ret = DEC_OK;
  struct Storage *storage = &dec_cont->storage;

  if (dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->logical_size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  //if (dec_cont->buf_num == 0)
  //  return DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffer_size = info->size;

  switch (dec_cont->buf_type) {
  case MISC_LINEAR_BUFFER:
    asic_buff->misc_linear[0] = *info;
    dec_cont->buf_to_free = NULL;
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 0;
    break;
  case TILE_EDGE_BUFFER:
    asic_buff->tile_edge[0] = *info;
    dec_cont->buf_to_free = NULL;
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 0;
    break;
  case REFERENCE_BUFFER: {
    i32 i = dec_cont->buffer_index;
    u32 id;
    struct DpbStorage *dpb = dec_cont->storage.dpb;
#if 1
    if (i < dpb->tot_buffers) {
      dpb->pic_buffers[i] = *info;
      if (i < dpb->dpb_size + 1) {
        u32 id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
        if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

        dpb->buffer[i].data = dpb->pic_buffers + i;
        dpb->buffer[i].mem_idx = id;
        dpb->pic_buff_id[i] = id;
      } else {
        id = AllocateIdFree(dpb->fb_list, dpb->pic_buffers + i);
        if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

        dpb->pic_buff_id[i] = id;
      }

      {
        void *base =
          (char *)(dpb->pic_buffers[i].virtual_address) + dpb->dir_mv_offset;
        (void)DWLmemset(base, 0, info->logical_size - dpb->dir_mv_offset);
      }

      dec_cont->buffer_index++;
      dec_cont->buf_num--;
    } else {
      /* Adding extra buffers. */
      if (i >= MAX_FRAME_BUFFER_NUMBER) {
        /* Too much buffers added. */
        return DEC_EXT_BUFFER_REJECTED;
      }

      dpb->pic_buffers[i] = *info;
      /* Here we need the allocate a USED id, so that this buffer will be added as free buffer in SetFreePicBuffer. */
      id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
      if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;
      dpb->pic_buff_id[i] = id;

      {
        void *base =
          (char *)(dpb->pic_buffers[i].virtual_address) + dpb->dir_mv_offset;
        (void)DWLmemset(base, 0, info->logical_size - dpb->dir_mv_offset);
      }

      dec_cont->buffer_index++;
      dec_cont->buf_num = 0;
      /* TODO: protect this variable, which may be changed in two threads. */
      dpb->tot_buffers++;

      SetFreePicBuffer(dpb->fb_list, id);
    }
#else
    InputQueueAddBuffer(dec_cont->in_buffers, info);
#endif
    dec_cont->ext_buffer_num++;

    if (dec_cont->buffer_index < dpb->tot_buffers)
      dec_ret = DEC_WAITING_FOR_BUFFER;
    else if (storage->raster_enabled) {
      ASSERT(0);  // NOT allowed yet!!!
      /* Reference buffer done. */
      /* Continue to allocate raster scan / down scale buffer if needed. */
      const struct SeqParamSet *sps = storage->active_sps;
//      struct DpbInitParams dpb_params;
      struct RasterBufferParams params;
      u32 pixel_width = (sps->bit_depth_luma == 8 && sps->bit_depth_chroma == 8) ? 8 : 10;
      u32 rs_pixel_width = (dec_cont->use_8bits_output || pixel_width == 8) ? 8 :
                           (dec_cont->use_p010_output ? 16 : 10);

      params.width = 0;
      params.height = 0;
      params.dwl = dec_cont->dwl;
      params.num_buffers = storage->dpb->tot_buffers;
      for (int i = 0; i < params.num_buffers; i++)
        dec_cont->tiled_buffers[i] = storage->dpb[0].pic_buffers[i];
      params.tiled_buffers = dec_cont->tiled_buffers;
      /* Raster out writes to modulo-16 widths. */
      params.width = NEXT_MULTIPLE(sps->pic_width * rs_pixel_width, ALIGN(dec_cont->align) * 8) / 8;;
      params.height = sps->pic_height;
      params.ext_buffer_config = dec_cont->ext_buffer_config;
      if (storage->pp_enabled) {
        params.width = NEXT_MULTIPLE(dec_cont->ppu_cfg[0].scale.width * rs_pixel_width,
                                     ALIGN(dec_cont->ppu_cfg[0].align) * 8) / 8;
        params.height = dec_cont->ppu_cfg[0].scale.height;
      }
      dec_cont->params = params;

      if (storage->raster_buffer_mgr) {
        dec_cont->_buf_to_free = RbmNextReleaseBuffer(storage->raster_buffer_mgr);

        if (dec_cont->_buf_to_free.virtual_address != 0) {
          dec_cont->buf_to_free = &dec_cont->_buf_to_free;
          dec_cont->next_buf_size = 0;
          dec_cont->buf_num = 1;
          dec_ret = DEC_WAITING_FOR_BUFFER;
          dec_cont->rbm_release = 1;
          break;
        }
        RbmRelease(storage->raster_buffer_mgr);
        storage->raster_buffer_mgr = NULL;
        dec_cont->rbm_release = 0;
      }
      storage->raster_buffer_mgr = RbmInit(params);
      if (storage->raster_buffer_mgr == NULL) return HANTRO_NOK;

      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
        /* Allocate raster scan / down scale buffers. */
        dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
        dec_cont->next_buf_size = params.width * params.height * 3 / 2;
        dec_cont->buf_to_free = NULL;
        dec_cont->buf_num = dec_cont->params.num_buffers;

        dec_cont->buffer_index = 0;
        dec_ret = DEC_WAITING_FOR_BUFFER;
      }
      else if (dec_cont->pp_enabled &&
               IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
        dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
        dec_cont->next_buf_size = dec_cont->params.width * dec_cont->params.height * 3 / 2;
        dec_cont->buf_to_free = NULL;
        dec_cont->buf_num = dec_cont->params.num_buffers;
        dec_ret = DEC_WAITING_FOR_BUFFER;
      } else {
        dec_cont->buf_to_free = NULL;
        dec_cont->next_buf_size = 0;
        dec_cont->buffer_index = 0;
        dec_cont->buf_num = 0;
      }
    }
    /* Since only one type of buffer will be set as external, we don't need to switch to adding next buffer type. */
    /*
    else {
      dec_cont->buf_to_free = NULL;
      dec_cont->next_buf_size = 0;
      dec_cont->buffer_index = 0;
      dec_cont->buf_num = 0;
    }
    */
    break;
  }
  case RASTERSCAN_OUT_BUFFER: {
    u32 i = dec_cont->buffer_index;

    ASSERT(storage->raster_enabled);
    ASSERT(dec_cont->buffer_index < dec_cont->params.num_buffers);

    /* TODO(min): we don't add rs buffer now, instead the external rs buffer is added */
    /*            to a queue. The decoder will get a rs buffer from fifo when needed. */
    RbmAddPpBuffer(storage->raster_buffer_mgr, info, i);
    dec_cont->buffer_index++;
    dec_cont->buf_num--;
    dec_cont->ext_buffer_num++;

    /* Need to add all the picture buffers in state HEVCDEC_NEW_HEADERS. */
    /* Reference buffer always allocated along with raster/dscale buffer. */
    /* Step for raster buffer. */
    if (dec_cont->buffer_index < dec_cont->min_buffer_num)
      dec_ret = DEC_WAITING_FOR_BUFFER;
    else {
      if (dec_cont->pp_enabled &&
          IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
        ASSERT(0);  // NOT allowed!
        dec_cont->buffer_index = 0;
        dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
        dec_cont->next_buf_size = dec_cont->params.width * dec_cont->params.height * 3 / 2;
        dec_cont->buf_to_free = NULL;
        dec_cont->buf_num = dec_cont->params.num_buffers;
        dec_ret = DEC_WAITING_FOR_BUFFER;
      } else {
        dec_cont->next_buf_size = 0;
        dec_cont->buf_to_free = NULL;
        dec_cont->buffer_index = 0;
        dec_cont->buf_num = 0;
      }
    }
    break;
  }
  case DOWNSCALE_OUT_BUFFER: {
    u32 i = dec_cont->buffer_index;

    ASSERT(storage->pp_enabled);
    ASSERT(dec_cont->buffer_index < dec_cont->params.num_buffers);

    RbmAddPpBuffer(storage->raster_buffer_mgr, info, i);

    dec_cont->buffer_index++;
    dec_cont->buf_num--;
    dec_cont->ext_buffer_num++;

    if (dec_cont->buffer_index == dec_cont->params.num_buffers) {
      dec_cont->next_buf_size = 0;
      dec_cont->buf_to_free = NULL;
      dec_cont->buffer_index = 0;
    } else {
      dec_cont->buf_to_free = NULL;
      dec_ret = DEC_WAITING_FOR_BUFFER;
    }
    break;
  }
  default:
    break;
  }

  return dec_ret;
}


enum DecRet HevcDecGetBufferInfo(HevcDecInst dec_inst, struct HevcDecBufferInfo *mem_info) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
//  enum DecRet dec_ret = DEC_OK;
  struct DWLLinearMem empty = {0};

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    if (dec_cont->rbm_release == 1) {
      /* All the raster buffers should be freed before being reallocated. */
      struct Storage *storage = &dec_cont->storage;

      ASSERT(storage->raster_buffer_mgr);
      if (storage->raster_buffer_mgr) {
        dec_cont->_buf_to_free = RbmNextReleaseBuffer(storage->raster_buffer_mgr);

        if (dec_cont->_buf_to_free.virtual_address != 0) {
          dec_cont->buf_to_free = &dec_cont->_buf_to_free;
          dec_cont->next_buf_size = 0;
          dec_cont->rbm_release = 1;
          dec_cont->buf_num = 0;
        } else {
          RbmRelease(storage->raster_buffer_mgr);
          dec_cont->rbm_release = 0;

          storage->raster_buffer_mgr = RbmInit(dec_cont->params);
          if (storage->raster_buffer_mgr == NULL) return DEC_OK;

          /* Allocate raster scan / down scale buffers. */
          if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
            dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
            dec_cont->next_buf_size = dec_cont->params.size;
          } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
            dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
            dec_cont->next_buf_size = dec_cont->params.size;
          }
          dec_cont->buf_to_free = NULL;
          dec_cont->buf_num = dec_cont->params.num_buffers;
          dec_cont->buffer_index = 0;

          mem_info->buf_to_free = empty;
          mem_info->next_buf_size = dec_cont->next_buf_size;
          mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;
          return DEC_OK;
        }
      }

    } else {
      /* External reference buffer: release done. */
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = dec_cont->next_buf_size;
      mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;
      return DEC_OK;
    }
  }

  if (dec_cont->buf_to_free) {
    mem_info->buf_to_free = *dec_cont->buf_to_free;

    // TODO(min): here we assume that the buffer should be freed externally.
    dec_cont->buf_to_free->virtual_address = NULL;
    dec_cont->buf_to_free = NULL;
  } else
    mem_info->buf_to_free = empty;
  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;

  ASSERT((mem_info->buf_num && mem_info->next_buf_size) || (mem_info->buf_to_free.virtual_address != NULL));

  return DEC_WAITING_FOR_BUFFER;
}

void HevcEnterAbortState(struct HevcDecContainer *dec_cont) {
  SetAbortStatusInList(&dec_cont->fb_list);
  RbmSetAbortStatus(dec_cont->storage.raster_buffer_mgr);
  dec_cont->abort = 1;
}

void HevcExistAbortState(struct HevcDecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  RbmClearAbortStatus(dec_cont->storage.raster_buffer_mgr);
  dec_cont->abort = 0;
}

enum DecRet HevcDecAbort(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting and rs/ds buffer waiting */
  HevcEnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet HevcDecAbortAfter(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  i32 core_id, i;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_state == HEVCDEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  if (dec_cont->asic_running && !dec_cont->b_mc) {
    /* stop HW */
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->hevc_regs[1]);

    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id); /* release HW lock */

    DecrementDPBRefCount(dec_cont->storage.dpb);

    dec_cont->asic_running = 0;
  }

  /* In multi-Core senario, waithwready is executed through listener thread,
     here to check whether HW is finished */
  if(dec_cont->b_mc) {
    for(i = 0; i < dec_cont->n_cores; i++) {
      DWLReserveHw(dec_cont->dwl, &core_id, DWL_CLIENT_TYPE_HEVC_DEC);
    }
    /* All HW Core finished */
    for(i = 0; i < dec_cont->n_cores; i++) {
      DWLReleaseHw(dec_cont->dwl, i);
    }
  }


  /* Clear any remaining pictures and internal parameters in DPB */
  HevcEmptyDpb(dec_cont, dec_cont->storage.dpb);

  /* Clear any internal parameters in storage */
  HevcClearStorage(&(dec_cont->storage));

  /* Clear internal parameters in HevcDecContainer */
  dec_cont->dec_state = HEVCDEC_INITIALIZED;
  dec_cont->start_code_detected = 0;
  dec_cont->pic_number = 0;
  dec_cont->packet_decoded = 0;

#ifdef USE_OMXIL_BUFFER
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 2;   /* We need at least (dpb_size+2) output buffers before starting decoding. */
  else
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 1;
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->ext_buffer_num = 0;
#endif

  HevcExistAbortState(dec_cont);

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->output_format == DEC_OUT_FRM_TILED_4X4) {
    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_FRAME_BUFFER_NUMBER; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
  }
#endif

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

void HevcDecSetNoReorder(HevcDecInst dec_inst, u32 no_reorder) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  dec_cont->storage.no_reordering = no_reorder;
  if(dec_cont->storage.dpb != NULL)
    dec_cont->storage.dpb->no_reordering = no_reorder;
}

enum DecRet HevcDecSetInfo(HevcDecInst dec_inst,
                          struct HevcDecConfig *dec_cfg) {

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  u32 pic_width = dec_cont->storage.active_sps->pic_width;
  u32 pic_height = dec_cont->storage.active_sps->pic_height;
  struct DecHwFeatures hw_feature;
  u32 i = 0;
  
  PpUnitConfig *ppu_cfg = &dec_cfg->ppu_cfg[0];
  u32 pixel_width = (dec_cont->storage.active_sps->bit_depth_luma == 8 &&
                     dec_cont->storage.active_sps->bit_depth_chroma == 8) ? 8 : 10;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }
  if (hw_feature.dec_stride_support)
    dec_cont->align = dec_cfg->align;
  else
    dec_cont->align = DEC_ALIGN_16B;
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg,
                        pixel_width, 1, dec_cont->storage.active_sps->mono_chrome);

  dec_cont->pp_enabled = dec_cont->ppu_cfg[0].enabled |
                         dec_cont->ppu_cfg[1].enabled |
                         dec_cont->ppu_cfg[2].enabled |
                         dec_cont->ppu_cfg[3].enabled |
                         dec_cont->ppu_cfg[4].enabled;

  dec_cont->storage.pp_enabled = dec_cont->pp_enabled;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }

  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height, 0, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  memcpy(dec_cont->delogo_params, dec_cfg->delogo_params, sizeof(dec_cont->delogo_params));
  if (CheckDelogo(dec_cont->delogo_params, dec_cont->storage.active_sps->bit_depth_luma, dec_cont->storage.active_sps->bit_depth_chroma))
    return DEC_PARAM_ERROR;
  if (hw_feature.dscale_support && !hw_feature.flexible_scale_support) {
    // x -> 1.5 ->  3  ->  6
    //    1  |  2   |  4   |  8
    u32 scaled_width = ppu_cfg[0].scale.width;
    u32 scaled_height = ppu_cfg[1].scale.height;
    if (scaled_width * 6 <= pic_width)
      dec_cont->dscale_shift_x = 3;
    else if (scaled_width * 3 <= pic_width)
      dec_cont->dscale_shift_x = 2;
    else if (scaled_width * 3 / 2 <= pic_width)
      dec_cont->dscale_shift_x = 1;
    else
      dec_cont->dscale_shift_x = 0;

    if (scaled_height * 6 <= pic_height)
      dec_cont->dscale_shift_y = 3;
    else if (scaled_height * 3 <= pic_height)
      dec_cont->dscale_shift_y = 2;
    else if (scaled_height * 3 / 2 <= pic_height)
      dec_cont->dscale_shift_y = 1;
    else
      dec_cont->dscale_shift_y = 0;

    dec_cont->storage.down_scale_x_shift = dec_cont->dscale_shift_x;
    dec_cont->storage.down_scale_y_shift = dec_cont->dscale_shift_y;
    dec_cont->down_scale_x = 1 << dec_cont->dscale_shift_x;
    dec_cont->down_scale_y = 1 << dec_cont->dscale_shift_y;
  }

  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_cont->ext_buffer_config |= 1 << RASTERSCAN_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_TILED_4X4)
    dec_cont->ext_buffer_config  = 1 << REFERENCE_BUFFER;
  return (DEC_OK);
}

void HevcDecUpdateStrmInfoCtrl(HevcDecInst dec_inst, u32 last_flag, u32 strm_bus_addr) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 pack_size = LOW_LATENCY_PACKET_SIZE;
  u32 hw_data_end = strm_bus_addr;
  u32 id = DWLReadAsicID(DWL_CLIENT_TYPE_HEVC_DEC);
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (dec_cont->update_reg_flag) {
    /* wait for hw ready if it's the first time to update length register */
    if (dec_cont->first_update) {
      while (!HevcCheckHwStatus(dec_cont))
        sched_yield();
      dec_cont->first_update = 0;
      dec_cont->ll_strm_len = 0;
    }

    while (hw_data_end - dec_cont->ll_strm_bus_address >= pack_size) {
      dec_cont->ll_strm_bus_address += pack_size;
      dec_cont->ll_strm_len += pack_size;
      if ((hw_data_end - dec_cont->ll_strm_bus_address) == 0 && last_flag == 1) {
        /* enable the last packet flag , this means the last packet size is 256bytes */
        SetDecRegister(dec_cont->hevc_regs, HWIF_LAST_BUFFER_E, 1);
        if ((id >> 16) == 0x8001)
          DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 3, dec_cont->hevc_regs[3]);
        else
          DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 2, dec_cont->hevc_regs[2]);
      }
    }
    /* update strm length register */
    SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, dec_cont->ll_strm_len);
    DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 6, dec_cont->hevc_regs[6]);
    if ((hw_data_end - dec_cont->ll_strm_bus_address) < pack_size &&
      (hw_data_end != dec_cont->ll_strm_bus_address)
       && last_flag == 1) {
      dec_cont->ll_strm_len += (hw_data_end - dec_cont->ll_strm_bus_address);
      dec_cont->ll_strm_bus_address = hw_data_end;
      /* enable the last packet flag */
      SetDecRegister(dec_cont->hevc_regs, HWIF_LAST_BUFFER_E, 1);
      if ((id >> 16) == 0x8001)
        DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 3, dec_cont->hevc_regs[3]);
      else
       DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 2, dec_cont->hevc_regs[2]);
      /* update strm length register */
      SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, dec_cont->ll_strm_len);
      DWLWriteRegToHw(dec_cont->dwl, dec_cont->core_id, 4 * 6, dec_cont->hevc_regs[6]);
    }
  }

  if(dec_cont->update_reg_flag) {
    /* check hw status */
    if (HevcCheckHwStatus(dec_cont) == 0) {
      dec_cont->tmp_length = dec_cont->ll_strm_len;
      dec_cont->update_reg_flag = 0;
      sem_post(&dec_cont->updated_reg_sem);
    }
  }
}

void HevcDecUpdateStrm(struct strmInfo info) {
    stream_info  = info;
}

void HevcMCPushOutputAll(struct HevcDecContainer *dec_cont) {
  while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;
}

void HevcMCWaitOutFifoEmpty(struct HevcDecContainer *dec_cont) {
  WaitOutputEmpty(&dec_cont->fb_list);
}

void HevcMCWaitPicReadyAll(struct HevcDecContainer *dec_cont) {
  WaitListNotInUse(&dec_cont->fb_list);
}

void HevcMCSetRefPicStatus(volatile u8 *p_sync_mem) {
  /* frame status */
  DWLmemset((void*)p_sync_mem, 0xFF, 32);
}

static u32 MCGetRefPicStatus(const u8 *p_sync_mem) {
  u32 ret;

  /* frame status */
  ret = ( p_sync_mem[1] << 8) + p_sync_mem[0];
  return ret;
}


static void MCValidateRefPicStatus(const u32 *hevc_regs,
                                   struct HevcHwRdyCallbackArg *info) {
  u8* p_ref_stat;
  struct DWLLinearMem *p_out;
  struct DpbStorage *dpb = info->current_dpb;
  u32 status, expected;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  p_out = (struct DWLLinearMem *)GetDataById(dpb->fb_list, info->out_id);

  p_ref_stat = (u8*)p_out->virtual_address + dpb->sync_mc_offset;

  status = MCGetRefPicStatus(p_ref_stat);

  expected = GetDecRegister(hevc_regs, HWIF_PIC_HEIGHT_IN_CBS) <<
      GetDecRegister(hevc_regs, HWIF_MIN_CB_SIZE);

  if(status < expected) {
    ASSERT(status == expected);
    HevcMCSetRefPicStatus((u8*)p_ref_stat);
  }
}


void HevcMCHwRdyCallback(void *args, i32 core_id) {
  u32 dec_regs[DEC_X170_REGISTERS];

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)args;
  struct HevcHwRdyCallbackArg info;

  const void *dwl;
  const struct DpbStorage *dpb;

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
  if (core_status != DEC_HW_IRQ_RDY) {
#ifdef DEC_PRINT_BAD_IRQ
    fprintf(stderr, "\nCore %d \"bad\" IRQ = 0x%08x\n",
            core_id, core_status);
#endif
    /* reset HW if still enabled */
    if (core_status & DEC_HW_IRQ_BUFFER) {
      /*  reset HW; we don't want an IRQ after reset so disable it */
          /* Reset HW */
      SetDecRegister(dec_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_regs, HWIF_DEC_E, 0);
      if (!dec_cont->vcmd_used)
        DWLDisableHw(dwl, core_id, 0x04, dec_regs[1]);
    }
    /* reset DMV storage for erroneous pictures */
    {
      u32 dvm_mem_size = dec_cont->storage.dmv_mem_size;
      u8 *dvm_base = (u8*)p_out->virtual_address;

      dvm_base += dpb->dir_mv_offset;

      (void) DWLmemset(dvm_base, 0, dvm_mem_size);
    }

    HevcMCSetRefPicStatus((u8*)p_out->virtual_address + dpb->sync_mc_offset);

    num_concealed_mbs = 1;
#if 0
    /* mark corrupt picture in output queue */
    MarkOutputPicCorrupt(dpb->fb_list, info.out_id, 1);

    /* ... and in DPB */
    i = dpb->dpb_size + 1;
    while((i--) > 0) {
      struct DpbPicture *dpb_pic = (struct DpbPicture *)dpb->buffer + i;
      if(dpb_pic->data == p_out) {
        dpb_pic->num_err_mbs = 1;
        break;
      }
    }
#ifdef USE_EC_MC
    dec_cont->storage.num_concealed_mbs = 1;
#endif
#endif
  } else {
    MCValidateRefPicStatus(dec_regs, &info);
#ifdef USE_EC_MC

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
      struct DpbPicture *dpb_pic = (struct DpbPicture *)dpb->buffer + i;
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
  u32 mbs = (HevcPicWidth(&dec_cont->storage) *
             HevcPicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  /* mark corrupt picture in output queue */
  MarkOutputPicInfo(dpb->fb_list, info.out_id, num_concealed_mbs, cycles);

  i = dpb->num_out;
  tmp = dpb->out_index_r;

  while((i--) > 0) {
    if (tmp == dpb->dpb_size + 1)
      tmp = 0;

    struct DpbOutPicture *dpb_pic = (struct DpbOutPicture *)dpb->out_buf + tmp;
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
    struct DpbPicture *dpb_pic = (struct DpbPicture *)dpb->buffer + i;
    if(dpb_pic->data == p_out) {
      dpb_pic->num_err_mbs = num_concealed_mbs;
      dpb_pic->cycles_per_mb = cycles;
      break;
    }
  }
  dec_cont->storage.num_concealed_mbs = num_concealed_mbs;

  /* clear IRQ status reg and release HW core */
  if (dec_cont->vcmd_used) {
    DWLReleaseCmdBuf(dwl, core_id);
    if (dec_cont->b_mc)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)info.core_id, FIFO_EXCEPTION_DISABLE);
  } else
    DWLReleaseHw(dwl, info.core_id);

  /* release the stream buffer. Callback provided by app */
  if(dec_cont->stream_consumed_callback.fn)
    dec_cont->stream_consumed_callback.fn((u8*)info.stream,
                                          (void*)info.p_user_data);

  type = FB_HW_OUT_FRAME;

  ClearHWOutput(dpb->fb_list, info.out_id, type, dec_cont->pp_enabled);

  /* decrement buffer usage in our buffer handling */
  DecrementDPBRefCountExt((struct DpbStorage *)dpb, info.ref_id);
}


void HevcMCSetHwRdyCallback(struct HevcDecContainer *dec_cont) {
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  u32 type, i;

  struct HevcHwRdyCallbackArg *arg = &dec_cont->hw_rdy_callback_arg[dec_cont->core_id];
  i32 core_id = dec_cont->core_id;

  if (dec_cont->vcmd_used) {
    arg = &dec_cont->hw_rdy_callback_arg[dec_cont->cmdbuf_id];
    core_id = dec_cont->cmdbuf_id;
  }

  arg->core_id = (dec_cont->vcmd_used && dec_cont->b_mc) ? dec_cont->mc_buf_id : core_id;
  arg->stream = dec_cont->stream_consumed_callback.p_strm_buff;
  arg->p_user_data = dec_cont->stream_consumed_callback.p_user_data;
  arg->out_id = dpb->current_out->mem_idx;
  arg->current_dpb = dpb;
#ifdef USE_EC_MC
  arg->is_idr = IS_IDR_NAL_UNIT(dec_cont->storage.prev_nal_unit);
#endif

  for (i = 0; i < dpb->dpb_size; i++) {
    arg->ref_id[i] = dpb->ref_id[i];

  }

  DWLSetIRQCallback(dec_cont->dwl, core_id, HevcMCHwRdyCallback, dec_cont);

  type = FB_HW_OUT_FRAME;

  MarkHWOutput(&dec_cont->fb_list, dpb->current_out->mem_idx, type);
}

#ifdef RANDOM_CORRUPT_RFC
u32 HevcCorruptRFC(struct HevcDecContainer *dec_cont) {
  u32 luma_size, chroma_size, rfc_luma_size, rfc_chroma_size;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
  struct DpbStorage *dpb = dec_cont->storage.dpb;

  HevcGetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                    &rfc_luma_size, &rfc_chroma_size);

  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)asic_buff->out_buffer->virtual_address,
                               NULL, 0,
                               NEXT_MULTIPLE(luma_size, ref_buffer_align) + 
                                 NEXT_MULTIPLE(chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    DEBUG_PRINT(("Bitswap reference buffer corruption error (wrong config?)\n"));
  }
  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)asic_buff->out_buffer->virtual_address + 
                                dpb->cbs_tbl_offsety, NULL, 0,
                               NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align) +
                                 NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    DEBUG_PRINT(("Bitswap RFC table corruption error (wrong config?)\n"));
  }
  return 0;
}
#endif

