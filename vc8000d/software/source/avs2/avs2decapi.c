/*------------------------------------------------------------------------------
--       Copyright (c) 2018-2019, VeriSilicon Inc. All rights reserved        --
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
#include "avs2_container.h"
#include "avs2decapi.h"
#include "avs2_decoder.h"
#include "avs2_dpb.h"
#include "avs2hwd_asic.h"
#include "regdrv.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "dwl.h"
#include "version.h"
#include "decapicommon.h"
#include "vpufeature.h"
#include "ppu.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#ifdef USE_RANDOM_TEST
#include "string.h"
#include "stream_corrupt.h"
#endif

static void Avs2ResetSeqParam(struct Avs2DecContainer *dec_cont);
static void Avs2UpdateAfterPictureDecode(struct Avs2DecContainer *dec_cont);
static u32 Avs2SpsSupported(const struct Avs2DecContainer *dec_cont);
static u32 Avs2PpsSupported(const struct Avs2DecContainer *dec_cont);

static u32 Avs2AllocateResources(struct Avs2DecContainer *dec_cont);
static void Avs2InitPicFreezeOutput(struct Avs2DecContainer *dec_cont,
                                    u32 from_old_dpb);
static void Avs2GetSarInfo(const struct Avs2Storage *storage, u32 *sar_width,
                           u32 *sar_height);
extern void Avs2PreparePpRun(struct Avs2DecContainer *dec_cont);

static enum DecRet Avs2DecNextPictureInternal(
    struct Avs2DecContainer *dec_cont);
static void Avs2CycleCount(struct Avs2DecContainer *dec_cont);
static void Avs2DropCurrentPicutre(struct Avs2DecContainer *dec_cont);

static void Avs2EnterAbortState(struct Avs2DecContainer *dec_cont);
static void Avs2ExistAbortState(struct Avs2DecContainer *dec_cont);

#ifdef RANDOM_CORRUPT_RFC
u32 Avs2CorruptRFC(struct Avs2DecContainer *dec_cont);
#endif

#define DEC_DPB_NOT_INITIALIZED -1
#define DEC_MODE_HEVC 12
#define CHECK_TAIL_BYTES 16
extern volatile struct strmInfo stream_info;

#ifdef PERFORMANCE_TEST
extern u32 hw_time_use;
#endif

static void updateHwStream(struct Avs2DecContainer *dec_cont) {
  struct Avs2StreamParam *stream = dec_cont->hwdec.stream;

  /* consider all buffer processed */
  dec_cont->hw_stream_start = stream->stream;
  dec_cont->hw_stream_start_bus = stream->stream_bus_addr;
  dec_cont->hw_length = stream->stream_length;
  dec_cont->hw_bit_pos = stream->stream_offset;
  dec_cont->stream_pos_updated = 1;
}

/**
 * Initializes decoder software. Function reserves memory for the
 * decoder instance and calls HevcInit to initialize the
 * instance data.
 */
enum DecRet Avs2DecInit(Avs2DecInst *dec_inst, const void *dwl,
                        struct Avs2DecConfig *dec_cfg) {

  /*@null@ */ struct Avs2DecContainer *dec_cont;

  struct DWLInitParam dwl_init;
  HwdRet hwd_ret;
  DWLHwConfig hw_cfg;
  struct DecHwFeatures hw_feature;
  u32 id, hw_build_id;
  u32 is_legacy = 0;
  u32 low_latency_sim = 0;
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
  id = DWLReadAsicID(DWL_CLIENT_TYPE_AVS2_DEC);
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_AVS2_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
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
  } else if ((id & 0x0000F000) >> 12 == 0 &&  /* MAJOR_VERSION */
             (id & 0x00000FF0) >> 4 == 0x18) {/* MINOR_VERSION */
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
  if (!hw_feature.avs2_support) {
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

  if ((!hw_feature.fmt_p010_support &&
       dec_cfg->pixel_format == DEC_OUT_PIXEL_P010) ||
      (!hw_feature.fmt_customer1_support &&
       dec_cfg->pixel_format == DEC_OUT_PIXEL_CUSTOMER1) ||
      (!hw_feature.addr64_support && sizeof(addr_t) == 8))
    return DEC_PARAM_ERROR;

  /* TODO: ? */
  dwl_init.client_type = DWL_CLIENT_TYPE_AVS2_DEC;

  dec_cont =
      (struct Avs2DecContainer *)DWLmalloc(sizeof(struct Avs2DecContainer));

  if (dec_cont == NULL) {
    return (DEC_MEMFAIL);
  }

  (void)DWLmemset(dec_cont, 0, sizeof(struct Avs2DecContainer));
  dec_cont->dwl = dwl;
  dec_cont->hwdec.regs[0] = id;
  dec_cont->tile_by_tile = dec_cfg->tile_by_tile;

#ifdef PERFORMANCE_TEST
  SwActivityTraceInit(&dec_cont->activity);
#endif
  Avs2Init(dec_cont, dec_cfg->no_output_reordering);

  dec_cont->dec_state = AVS2DEC_INITIALIZED;

#ifndef _DISABLE_PIC_FREEZE
#ifndef USE_FAST_EC
  dec_cont->storage.intra_freeze = dec_cfg->use_video_freeze_concealment;
#else
  dec_cont->storage.intra_freeze =
      1;  // dec_cfg->use_video_freeze_concealment & 2;
  dec_cont->storage.fast_freeze = 1;
#endif
#endif
  dec_cont->storage.picture_broken = HANTRO_FALSE;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);

  /* max decodable picture width and height*/
  //FIXME: use avs2 config instead
  dec_cont->max_dec_pic_width = hw_feature.hevc_max_dec_pic_width;
  dec_cont->max_dec_pic_height = hw_feature.hevc_max_dec_pic_height;

  dec_cont->checksum = dec_cont; /* save instance as a checksum */

  *dec_inst = (Avs2DecInst)dec_cont;
  /*  default single core */
//  dec_cont->n_cores = 1;
//  dec_cont->n_cores_available = 1;

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
  dec_cont->use_8bits_output =
      (dec_cfg->pixel_format == DEC_OUT_PIXEL_CUT_8BIT) ? 1 : 0;
  dec_cont->use_p010_output =
      (dec_cfg->pixel_format == DEC_OUT_PIXEL_P010) ? 1 : 0;
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
  dec_cont->buffer_num_added = 0;
  dec_cont->ext_buffer_config = 0;
#if 0
  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_cont->ext_buffer_config |= 1 << RASTERSCAN_OUT_BUFFER;
  else if (dec_cfg->output_format == DEC_OUT_FRM_TILED_4X4)
    dec_cont->ext_buffer_config  = 1 << REFERENCE_BUFFER;
#endif
  dec_cont->main10_support = hw_feature.avs2_main10_support;
  dec_cont->use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->use_ringbuffer = dec_cfg->use_ringbuffer;
  dec_cont->use_fetch_one_pic = 0;
  dec_cont->storage.use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->legacy_regs = 0;  // is_legacy;
  dec_cont->align = dec_cont->hwdec.align = dec_cont->storage.align = dec_cfg->align;
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  /* set some hw config */
  dec_cont->hwcfg.use_video_compressor = dec_cfg->use_video_compressor;
  dec_cont->hwcfg.disable_out_writing = 0;
  Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_CFG, &dec_cont->hwcfg);

  if (dec_cfg->decoder_mode == DEC_LOW_LATENCY_RTL) {
    low_latency_sim = 1;
  }
  if(low_latency_sim) {
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
  } else {
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }

// dec_cont->in_buffers = InputQueueInit(MAX_FRAME_BUFFER_NUMBER);
// if (dec_cont->in_buffers == NULL)
//  return DEC_MEMFAIL;

#if defined(USE_RANDOM_TEST) || defined(RANDOM_CORRUPT_RFC)
  /*********************************************************/
  /** Developers can change below parameters to generate  **/
  /** different kinds of error stream.                    **/
  /*********************************************************/
  dec_cont->error_params.seed = 66;
  strcpy(dec_cont->error_params.truncate_stream_odds, "1 : 6");
  strcpy(dec_cont->error_params.swap_bit_odds, "1 : 100000");
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
  dec_cont->ferror_stream = fopen("random_error.avs2", "wb");
  if(dec_cont->ferror_stream == NULL) {
    DEBUG_PRINT(("Unable to open file error.avs2\n"));
    return DEC_MEMFAIL;
  }
#endif
  /* allocate internal buffers */
  hwd_ret = Avs2HwdAllocInternals(&dec_cont->hwdec, &dec_cont->cmems);
  if (hwd_ret != HWD_OK) {
    DEBUG_PRINT("[avs2dec] Cannot Get Intenal Buffers.\n");
    return DEC_MEMFAIL;
  }
  //dec_cont->dec_state = AVS2DEC_WAIT_HEADER;

  (void)dwl_init;
  (void)is_legacy;

  return (DEC_OK);
}

/* This function provides read access to decoder information. This
 * function should not be called before Avs2DecDecode function has
 * indicated that headers are ready. */
enum DecRet Avs2DecGetInfo(Avs2DecInst dec_inst, struct Avs2DecInfo *dec_info) {
  u32 cropping_flag;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  struct Avs2Storage *storage;

  if (dec_inst == NULL || dec_info == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;

  if (storage->sps.cnt == 0 || storage->pps.cnt == 0) {
    return (DEC_HDRS_NOT_RDY);
  }

  dec_info->pic_width = Avs2PicWidth(storage);
  dec_info->pic_height = Avs2PicHeight(storage);
  // FIXME: dec_info->matrix_coefficients = Avs2MatrixCoefficients(storage);
  // dec_info->video_range = Avs2VideoRange(storage);
  // dec_info->mono_chrome = Avs2IsMonoChrome(storage);
  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_info->pic_buff_size = Avs2GetMinDpbSize(storage) + 1 + 1;
  else
    dec_info->pic_buff_size = Avs2GetMinDpbSize(storage) + 1 + 2;
  dec_info->multi_buff_pp_size =
      storage->dpb->no_reordering ? 2 : dec_info->pic_buff_size;
  dec_info->dpb_mode = dec_cont->dpb_mode;

  Avs2GetSarInfo(storage, &dec_info->sar_width, &dec_info->sar_height);

  Avs2CroppingParams(storage, &cropping_flag,
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

  dec_info->output_format = dec_cont->output_format;
  dec_info->bit_depth = Avs2SampleBitDepth(storage);
  dec_info->out_bit_depth = Avs2OutputBitDepth(storage);
  dec_info->interlaced_sequence = dec_cont->storage.sps.is_field_sequence;

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN ||
      dec_cont->pp_enabled) {
    if (dec_cont->use_p010_output && dec_info->bit_depth > 8) {
      dec_info->pixel_format = DEC_OUT_PIXEL_P010;
      dec_info->bit_depth = 16;
    } else if (dec_cont->pixel_format == DEC_OUT_PIXEL_CUSTOMER1) {
      dec_info->pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
    } else if (dec_cont->use_8bits_output) {
      dec_info->pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
      dec_info->bit_depth = 8;
    } else {
      dec_info->pixel_format = DEC_OUT_PIXEL_DEFAULT;
    }
  } else {
    /* Reference buffer. */
    dec_info->pixel_format = DEC_OUT_PIXEL_DEFAULT;
  }

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    dec_info->pic_stride =
        NEXT_MULTIPLE(dec_info->pic_width * dec_info->bit_depth, 128) / 8;
  else
    /* Reference buffer. */
    dec_info->pic_stride = dec_info->pic_width * dec_info->bit_depth / 8;

  /* for HDR */
  // dec_info->transfer_characteristics =
  // storage->sps[storage->active_sps_id]->vui_parameters.transfer_characteristics;

  return (DEC_OK);
}

/* Releases the decoder instance. Function calls HevcShutDown to
 * release instance data and frees the memory allocated for the
 * instance. */
void Avs2DecRelease(Avs2DecInst dec_inst) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

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
  //dwl = dec_cont->dwl;

/* make sure all in sync in multicore mode, hw idle, output empty */
  {
    u32 i;
    const struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;

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
    Avs2HwdStopHw(&dec_cont->hwdec, dec_cont->core_id);
    dec_cont->asic_running = 0;

    /* Decrement usage for DPB buffers */
    DecrementDPBRefCount(dec_cont->storage.dpb);
  }
  Avs2Shutdown(&dec_cont->storage);

  Avs2FreeDpb(dec_cont, dec_cont->storage.dpb);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->storage.raster_buffer_mgr)
    RbmRelease(dec_cont->storage.raster_buffer_mgr);
#if 0
#ifndef USE_EXTERNAL_BUFFER
    ReleaseAsicBuffers(dwl, dec_cont->asic_buff);
#else
    ReleaseAsicBuffers(dec_cont, dec_cont->asic_buff);
#endif
#endif
  // release internal buffers
  Avs2HwdRelease(&dec_cont->hwdec);
  // Avs2ReleaseAsicTileEdgeMems(dec_cont);

  Avs2ReleaseList(&dec_cont->fb_list);

#ifdef PERFORMANCE_TEST
  SwActivityTraceRelease(&dec_cont->activity);
  printf("SW consumed time = %lld ms\n", dec_cont->activity.active_time/100 - hw_time_use);
#endif
  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}

/* Decode stream data. Calls Avs2Decode to do the actual decoding. */
enum DecRet Avs2DecDecode(Avs2DecInst dec_inst,
                          const struct Avs2DecInput *input,
                          struct Avs2DecOutput *output) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 strm_len;
  u32 input_data_len = input->data_len;  // used to generate error stream
  const u8 *tmp_stream;
  enum DecRet return_value = DEC_STRM_PROCESSED;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->abort) {
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
        return DEC_STRM_PROCESSED;
      }
    }

    // error type: truncate stream(random len for random packet);
    if (dec_cont->error_params.truncate_stream &&
        !dec_cont->stream_not_consumed) {

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
        return DEC_STRM_PROCESSED;
      }
    }

    /*  stream is truncated but not consumed at first time, the same truncated
    length
    at the second time */
    if (dec_cont->error_params.truncate_stream && dec_cont->stream_not_consumed)
      input_data_len = dec_cont->prev_input_len;

    // error type: swap bits;
    if (dec_cont->error_params.swap_bits_in_stream &&
        !dec_cont->stream_not_consumed) {
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

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  dec_cont->stream_pos_updated = 0;
  output->strm_curr_pos = NULL;
  dec_cont->hw_stream_start_bus = input->stream_bus_address;
  dec_cont->hw_buffer_start_bus = input->buffer_bus_address;
  dec_cont->hw_stream_start = input->stream;
  dec_cont->hw_buffer = input->buffer;
  strm_len = dec_cont->hw_length = input_data_len;
  /* For low latency mode, strmLen is set as a large value */
  dec_cont->hw_buffer_length = input->buff_len;
  tmp_stream = input->stream;
//  dec_cont->stream_consumed_callback.p_strm_buff = input->stream;
  //dec_cont->stream_consumed_callback.p_user_data = input->p_user_data;

  /* If there are more buffers to be allocated or to be freed, waiting for
   * buffers ready. */
  if (dec_cont->buf_to_free != NULL ||
      (dec_cont->next_buf_size != 0 &&
       dec_cont->buffer_num_added < dec_cont->min_buffer_num) ||
      dec_cont->rbm_release) {
    return_value = DEC_WAITING_FOR_BUFFER;
    goto end;
  }

  do {
    u32 dec_result;
    u32 num_read_bytes = 0;
    struct Avs2Storage *storage = &dec_cont->storage;
    dec_cont->storage.dpb->storage = &dec_cont->storage;
    if (dec_cont->dec_state == AVS2DEC_NEW_HEADERS) {
    //MEANS new headers(sps) come, need to check if buffers need to be reallocated.
      dec_result = AVS2_HDRS_RDY;
      dec_cont->dec_state = AVS2DEC_INITIALIZED;

    }
     else if (dec_cont->dec_state == AVS2DEC_BUFFER_EMPTY) {
     //to release dpb buffer?
      DEBUG_PRINT(("Avs2DecDecode: Skip Avs2DecDecode\n"));
      DEBUG_PRINT(("Avs2DecDecode: Jump to AVS2_PIC_RDY\n"));

      dec_result = AVS2_PIC_RDY;
    }
    else if (dec_cont->dec_state == AVS2DEC_WAITING_FOR_BUFFER) {
      DEBUG_PRINT(("Avs2DecDecode: Skip HevcDecode\n"));
      DEBUG_PRINT(("Avs2DecDecode: Jump to HEVC_PIC_RDY\n"));

      dec_result = AVS2_BUFFER_NOT_READY;

    }
    else {
#if 0
    assert((dec_cont->dec_state == AVS2DEC_INITIALIZED) ||
           (dec_cont->dec_state == AVS2DEC_READY));
#endif
    dec_result = Avs2Decode(dec_cont, tmp_stream, strm_len, input->pic_id,
                            &num_read_bytes);
    if (num_read_bytes > strm_len) num_read_bytes = strm_len;

    ASSERT(num_read_bytes <= strm_len);
  }

    tmp_stream += num_read_bytes;
    if (tmp_stream >= dec_cont->hw_buffer + dec_cont->hw_buffer_length &&
        dec_cont->use_ringbuffer)
      tmp_stream -= dec_cont->hw_buffer_length;
    strm_len -= num_read_bytes;

    switch (dec_result) {
      /* Split the old case AVS2_HDRS_RDY into case AVS2_HDRS_RDY &
       * AVS2_BUFFER_NOT_READY. */
      /* In case AVS2_BUFFER_NOT_READY, we will allocate resources. */
      case AVS2_HDRS_RDY: {
#if 0
        if(dec_cont->storage.sps_old.cnt&&(dec_cont->storage.sps_old.horizontal_size!=dec_cont->storage.sps.horizontal_size
          || dec_cont->storage.sps_old.vertical_size!=dec_cont->storage.sps.vertical_size))
        {
          struct Avs2SeqParam sps_temp;
          sps_temp = dec_cont->storage.sps;
          dec_cont->storage.sps = dec_cont->storage.sps_old;
          Avs2FlushBuffer(&dec_cont->storage);
          FinalizeOutputAll(&dec_cont->fb_list);
          while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
            ;
          WaitListNotInUse(&dec_cont->fb_list);
          WaitOutputEmpty(&dec_cont->fb_list);
          if (dec_cont->storage.raster_buffer_mgr) {
            ret = RbmWaitPpBufferNotUsed(dec_cont->storage.raster_buffer_mgr);
            if (ret) {
              if (ret) {
                strm_len = 0;
                dec_cont->dec_state = AVS2DEC_NEW_HEADERS;
                ret = DEC_PENDING_FLUSH;
                break;
              }
            }
          }
          PushOutputPic(&dec_cont->fb_list, NULL, -2);
          dec_cont->storage.sps = sps_temp;
        }


#endif
        Avs2DeriveBufferSpec(storage, &storage->buff_spec, dec_cont->align);
        /* If both the the size and number of buffers allocated are enough,
         * decoding will continue as normal.
         */
        dec_cont->reset_dpb_done = 0;
        if (Avs2IsExternalBuffersRealloc(dec_cont, storage)) {
          /* flush and reallocate buffers */
          if (storage->dpb->flushed && storage->dpb->num_out) {
            /* output first all DPB stored pictures */
            storage->dpb->flushed = 0;
            dec_cont->dec_state = AVS2DEC_NEW_HEADERS;
            return_value = DEC_PENDING_FLUSH;
            strm_len = 0;
            break;
          }

/* Make sure that all frame buffers are not in use before
 * reseting DPB (i.e. all HW cores are idle and all output
 * processed) */

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
            if (ret) {
              strm_len = 0;
              dec_cont->dec_state = AVS2DEC_NEW_HEADERS;
              return_value = DEC_PENDING_FLUSH;
              break;
            }
          }
        }
#endif
        PushOutputPic(&dec_cont->fb_list, NULL, -2);

          if (!Avs2SpsSupported(dec_cont)) {
            Avs2ResetSeqParam(dec_cont);
            storage->pic_started = HANTRO_FALSE;
            dec_cont->dec_state = AVS2DEC_INITIALIZED;
            storage->prev_buf_not_finished = HANTRO_FALSE;
            output->data_left = 0;

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
            /* FIXME: Remove it... If raster out, raster manager hasn't been
             * initialized yet. */
            Avs2SetExternalBufferInfo(dec_cont, storage);
            dec_result = AVS2_BUFFER_NOT_READY;
            dec_cont->dec_state = AVS2DEC_WAITING_FOR_BUFFER;
            strm_len = 0;
            return_value = DEC_HDRS_RDY;
          }

          strm_len = 0;
          dec_cont->dpb_mode = DEC_DPB_DEFAULT;
        } else {
          dec_result = AVS2_BUFFER_NOT_READY;
          dec_cont->dec_state = AVS2DEC_WAITING_FOR_BUFFER;
          /* Need to exit the loop give a chance to call FinalizeOutputAll() */
          /* to output all the pending frames even when there is no need to */
          /* re-allocate external buffers. */
          strm_len = 0;
          return_value = DEC_STRM_PROCESSED;
          break;
        }

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
#if 0
      case AVS2_SEQ_END: {
        Avs2FlushBuffer(&dec_cont->storage);
        FinalizeOutputAll(&dec_cont->fb_list);
        while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
          ;
        WaitListNotInUse(&dec_cont->fb_list);
        WaitOutputEmpty(&dec_cont->fb_list);
        if (dec_cont->storage.raster_buffer_mgr) {
          u32 ret = RbmWaitPpBufferNotUsed(dec_cont->storage.raster_buffer_mgr);
          if (ret) {
            if (ret) {
              strm_len = 0;
              dec_cont->dec_state = AVS2DEC_FLUSHING;
              return_value = DEC_PENDING_FLUSH;
              break;
            }
          }
        }
        PushOutputPic(&dec_cont->fb_list, NULL, -2);
      }
#endif
      case AVS2_BUFFER_NOT_READY: {
        i32 ret;
        ret = Avs2AllocateSwResources(dec_cont->dwl, storage, dec_cont);
        if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

        ret = Avs2AllocateResources(dec_cont);
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
            Avs2ResetSeqParam(dec_cont);
            return_value = DEC_MEMFAIL; /* signal that decoder failed to init
                                           parameter sets */
          }

          strm_len = 0;

          // dec_cont->dpb_mode = DEC_DPB_DEFAULT;

        } else {
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
          // return_value = DEC_HDRS_RDY;
        }

/* Reset strm_len only for base view -> no HDRS_RDY to
 * application when param sets activated for stereo view */
// strm_len = 0;

// dec_cont->dpb_mode = DEC_DPB_DEFAULT;

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
      case AVS2_NO_FREE_BUFFER:
        tmp_stream = input->stream;
        strm_len = 0;
        return_value = DEC_NO_DECODING_BUFFER;
        break;
#endif

      case AVS2_PIC_RDY: {
        u32 asic_status;
        u32 picture_broken;
        u32 prev_irq_buffer =
            dec_cont->dec_state ==
            AVS2DEC_BUFFER_EMPTY; /* entry due to IRQ_BUFFER */
       // struct Avs2AsicBuffers *asic_buff = &dec_cont->storage.cmems;
        struct Avs2StreamParam *stream = &dec_cont->storage.input;

        HwdRet hwd_ret;

        picture_broken =
            (storage->picture_broken && storage->intra_freeze /*&&
                        !IS_RAP_NAL_UNIT(storage->prev_nal_unit)*/);

        if (dec_cont->dec_state != AVS2DEC_BUFFER_EMPTY && !picture_broken) {
          /* setup the reference frame list; just at picture start */
          if (!Avs2PpsSupported(dec_cont)) {
            Avs2ResetSeqParam(dec_cont);

            return_value = DEC_STREAM_NOT_SUPPORTED;
            goto end;
          }

          stream->is_rb = dec_cont->use_ringbuffer;
          stream->stream = (u8 *)dec_cont->hw_stream_start;
          stream->stream_bus_addr = dec_cont->hw_stream_start_bus;
          stream->stream_length = dec_cont->hw_length;
          // stream->stream_offset = 3; //dec_cont->hw_bit_pos/8;
          stream->stream_offset = storage->strm[0].strm_buff_read_bits/8;
          stream->ring_buffer.bus_address = dec_cont->hw_buffer_start_bus;
          stream->ring_buffer.virtual_address = (u32 *)dec_cont->hw_buffer;
          stream->ring_buffer.size = stream->ring_buffer.logical_size =
              dec_cont->hw_buffer_length;
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_STREAM, stream);

          // asic_buff->out_buffer = storage->curr_image->data;
          Avs2SetRecon(storage, &storage->recon, storage->curr_image->data);
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_RECON, &storage->recon);

// asic_buff->out_pp_buffer = storage->curr_image->pp_data;
// storage->ppout.pic.nv12.y = *storage->curr_image->pp_data;
// Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PP, &storage->ppout);

          IncrementDPBRefCount(dec_cont->storage.dpb);

// Avs2SetRegs(dec_cont);
#if 1  // for test, don't delete
          {
            Avs2SetRef(storage, &storage->refs, dec_cont->storage.dpb);

          }
#endif
          /* determine initial reference picture lists */
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_REFS,
                           &dec_cont->storage.refs);

          DEBUG_PRINT(("Save DPB status\n"));
          /* we trust our memcpy; ignore return value */
          (void)DWLmemcpy(&storage->dpb[1], &storage->dpb[0],
                          sizeof(*storage->dpb));

          DEBUG_PRINT(("Save POC status\n"));
          (void)DWLmemcpy(&storage->poc[1], &storage->poc[0],
                          sizeof(*storage->poc));

          /* create output picture list */
          Avs2UpdateAfterPictureDecode(dec_cont);

          /* remove unused buffer */
          Avs2DpbRemoveUnused(dec_cont->storage.dpb);

          /* set ppu cfg*/
          if (dec_cont->pp_enabled) {
            Avs2SetPp(storage, &storage->ppout, &dec_cont->ppu_cfg[0],
                      &hw_feature);
            Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PP, &storage->ppout);
          }
          /* prepare PP if needed */
          /*HevcPreparePpRun(dec_cont);*/
        } else {
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
        }

        /* run asic and react to the status */
        if (!picture_broken) {
          DEBUG_PRINT(("DECODING DOI=%d, POI=%d %s\n", dec_cont->storage.pps.coding_order,
                   dec_cont->storage.pps.poc,
                   dec_cont->storage.pps.progressive_frame?"FRAME":
                   (dec_cont->storage.pps.is_top_field?"TOP":"BOTTOM")));
          //DEBUG_PRINT(" !!!!!!RUN HARDWARE !!!!!!\n");
          dec_cont->hwdec.cfg->start_code_detected = dec_cont->start_code_detected;
          hwd_ret = Avs2HwdRun(&dec_cont->hwdec);
          hwd_ret = Avs2HwdSync(&dec_cont->hwdec, 0);
          (void)hwd_ret;
          updateHwStream(dec_cont);
          asic_status = dec_cont->hwdec.status;
          DecrementDPBRefCount(dec_cont->storage.dpb);
          // return DEC_PIC_RDY;
        } else {
          if (dec_cont->storage.pic_started) {
#ifdef USE_FAST_EC
            if (!dec_cont->storage.fast_freeze)
#endif
              Avs2InitPicFreezeOutput(dec_cont, 0);
            Avs2UpdateAfterPictureDecode(dec_cont);
          }
          asic_status = DEC_HW_IRQ_ERROR;
        }

        /* remove unused buffer */
        //Avs2DpbRemoveUnused(dec_cont->storage.dpb);

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
        Avs2CorruptRFC(dec_cont);
#endif
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
          if (!dec_cont->storage.fast_freeze)
            Avs2InitPicFreezeOutput(dec_cont, 1);
          else
            Avs2DropCurrentPicutre(dec_cont);
#endif

          {
            struct StrmData *strm = dec_cont->storage.strm;

            if (prev_irq_buffer) {
              /* Call Avs2DecDecode() due to DEC_HW_IRQ_BUFFER,
                 reset strm to input buffer. */
              strm->strm_buff_start = input->buffer;
              strm->strm_curr_pos = input->stream;
              strm->strm_buff_size = input->buff_len;
              strm->strm_data_size = input_data_len;
              strm->strm_buff_read_bits =
                  (u32)(strm->strm_curr_pos - strm->strm_buff_start) * 8;
              strm->is_rb = dec_cont->use_ringbuffer;
              ;
              strm->remove_emul3_byte = 0;
              strm->bit_pos_in_word = 0;
            }
#ifdef AVS2_INPUT_MULTI_FRM
            if (Avs2NextStartCode(strm) == HANTRO_OK) {
              if (strm->strm_curr_pos >= tmp_stream)
                strm_len -= (strm->strm_curr_pos - tmp_stream);
              else
                strm_len -=
                    (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
              tmp_stream = strm->strm_curr_pos;
            }
#else
            tmp_stream = input->stream + input_data_len;
#endif
          }

          dec_cont->stream_pos_updated = 0;
        } else if (asic_status & DEC_HW_IRQ_BUFFER) {
          /* TODO: Need to check for CABAC zero words here? */
          DEBUG_PRINT(("IRQ: BUFFER EMPTY\n"));

          /* a packet successfully decoded, don't Reset pic_started flag if
           * there is a need for rlc mode */
          dec_cont->dec_state = AVS2DEC_BUFFER_EMPTY;
          dec_cont->packet_decoded = HANTRO_TRUE;
          output->data_left = 0;

#ifdef DUMP_INPUT_STREAM
        fwrite(input->stream, 1, input_data_len, dec_cont->ferror_stream);
#endif
#ifdef USE_RANDOM_TEST
        dec_cont->stream_not_consumed = 0;
#endif
          return DEC_BUF_EMPTY;
        } else {/* OK in here */
          if (storage->pps.type == I_IMG) {
            dec_cont->storage.picture_broken = HANTRO_FALSE;
          }
#if 1
          {
            /* CHECK CABAC WORDS */
            struct StrmData strm_tmp = *dec_cont->storage.strm;
            u32 consumed =
                dec_cont->hwdec.stream->stream -
                (strm_tmp.strm_curr_pos - strm_tmp.strm_buff_read_bits / 8);

            strm_tmp.strm_curr_pos = (u8 *)dec_cont->hwdec.stream->stream_bus_addr;
            strm_tmp.strm_buff_read_bits = 8 * consumed;
            strm_tmp.bit_pos_in_word = 0;
            if (0) {  // if (strm_tmp.strm_data_size - consumed >
                      // CHECK_TAIL_BYTES) {
              /* Do not check CABAC zero words if remaining bytes are too few.
               */
              u32 tmp = HANTRO_OK;  // HevcCheckCabacZeroWords( &strm_tmp );
              if (tmp != HANTRO_OK) {
                if (dec_cont->packet_decoded != HANTRO_TRUE) {
                  DEBUG_PRINT(("Reset pic_started\n"));
                  dec_cont->storage.pic_started = HANTRO_FALSE;
                }

                dec_cont->storage.picture_broken = HANTRO_TRUE;
#ifndef USE_FAST_EC
                Avs2InitPicFreezeOutput(dec_cont, 1);
#else
                if (!dec_cont->storage.fast_freeze)
                  Avs2InitPicFreezeOutput(dec_cont, 1);
                else {
                  if (dec_cont->storage.dpb->current_out->to_be_displayed)
                    dec_cont->storage.dpb->num_out_pics_buffered--;
                  if (dec_cont->storage.dpb->fullness > 0)
                    dec_cont->storage.dpb->fullness--;
                  dec_cont->storage.dpb->num_ref_frames--;
                  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
                  dec_cont->storage.dpb->current_out->status = UNUSED;
                  dec_cont->storage.dpb->current_out->img_poi = 0;
                  dec_cont->storage.dpb->current_out->img_coi = 0;
                  if (dec_cont->storage.raster_buffer_mgr) {
                    if (!dec_cont->storage.dpb->current_out->first_field)
                      RbmReturnPpBuffer(storage->raster_buffer_mgr,
                                        dec_cont->storage.dpb->current_out
                                            ->pp_data->virtual_address);
                  }
                }
#endif
                {
#ifdef AVS2_INPUT_MULTI_FRM
                  struct StrmData *strm = dec_cont->storage.strm;

                  if (Avs2NextStartCode(strm) == HANTRO_OK) {
                    if (strm->strm_curr_pos >= tmp_stream)
                      strm_len -= (strm->strm_curr_pos - tmp_stream);
                    else
                      strm_len -= (strm->strm_curr_pos + strm->strm_buff_size -
                                   tmp_stream);
                    tmp_stream = strm->strm_curr_pos;
                  }
#else
                  tmp_stream = input->stream + input_data_len;
#endif
                }
                dec_cont->stream_pos_updated = 0;
              }
            }
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
        Avs2CycleCount(dec_cont);

#ifdef FFWD_WORKAROUND
        storage->prev_idr_pic_ready = IS_IDR_NAL_UNIT(storage->prev_nal_unit);
#endif /* FFWD_WORKAROUND */
        {
#if 0
        u32 sublayer = storage->active_sps->max_sub_layers - 1;
        u32 max_latency =
          dec_cont->storage.active_sps->max_num_reorder_pics[sublayer] +
          (dec_cont->storage.active_sps->max_latency_increase[sublayer]
           ? dec_cont->storage.active_sps
           ->max_latency_increase[sublayer] -
           1
           : 0);

        Avs2DpbCheckMaxLatency(dec_cont->storage.dpb, max_latency);
#endif
        }

        return_value = DEC_PIC_DECODED;
        strm_len = 0;
        break;
      }
      case AVS2_PARAM_SET_ERROR: {
        if (!Avs2ValidParamSets(&dec_cont->storage) && strm_len == 0) {
          return_value = DEC_STRM_ERROR;
        }

        /* update HW buffers if VLC mode */
        dec_cont->hw_length -= num_read_bytes;
        if (tmp_stream >= input->stream)
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream - input->stream);
        else
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address +
              (u32)(tmp_stream + input->buff_len - input->stream);

        dec_cont->hw_stream_start = tmp_stream;

        /* check active sps is valid or not */
        if (!Avs2SpsSupported(dec_cont)) {
          Avs2ResetSeqParam(dec_cont);
          storage->pic_started = HANTRO_FALSE;
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
          storage->prev_buf_not_finished = HANTRO_FALSE;

          return_value = DEC_STREAM_NOT_SUPPORTED;
          dec_cont->dpb_mode = DEC_DPB_DEFAULT;
          goto end;
        }

        break;
      }
#if 1
    case AVS2_NEW_ACCESS_UNIT: {
      dec_cont->stream_pos_updated = 0;

      dec_cont->storage.picture_broken = HANTRO_TRUE;
#ifndef USE_FAST_EC
      HevcInitPicFreezeOutput(dec_cont, 0);
#else
      if(!dec_cont->storage.fast_freeze)
        Avs2InitPicFreezeOutput(dec_cont, 0);
      else
        Avs2DropCurrentPicutre(dec_cont);
#endif

      Avs2UpdateAfterPictureDecode(dec_cont);

      /* PP will run in Avs2DecNextPicture() for this concealed picture */
      return_value = DEC_PIC_DECODED;

      dec_cont->pic_number++;
      strm_len = 0;

      break;
    }
#endif

      case AVS2_ABORTED:
        dec_cont->dec_state = AVS2DEC_ABORTED;
        return DEC_ABORTED;
      case AVS2_NONREF_PIC_SKIPPED:
        return_value = DEC_NONREF_PIC_SKIPPED;
      /* fall through */
#if 0
      case AVS2_PPS_RDY: {
        if (dec_cont->dec_state == AVS2DEC_WAIT_HEADER) {
          dec_cont->dec_state = AVS2DEC_WAIT_SETUP;
          strm_len = 0;
          return_value = DEC_HDRS_RDY;
          break;
        }
      }
#endif
      default: {/* case AVS2_ERROR, AVS2_RDY */
        dec_cont->hw_length -= num_read_bytes;
        if (tmp_stream >= input->stream)
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream - input->stream);
        else
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address +
              (u32)(tmp_stream + input->buff_len - input->stream);

        dec_cont->hw_stream_start = tmp_stream;
      }
    }
  } while (strm_len);

end:

  /*  If Hw decodes stream, update stream buffers from "storage" */
  if (dec_cont->stream_pos_updated) {
    output->strm_curr_pos = (u8 *)dec_cont->hw_stream_start;
    output->strm_curr_bus_address = dec_cont->hw_stream_start_bus;
    output->data_left = dec_cont->hw_length;
  } else {
    /* else update based on SW stream decode stream values */
    u32 data_consumed = (u32)(tmp_stream - input->stream);
    if (tmp_stream >= input->stream)
      data_consumed = (u32)(tmp_stream - input->stream);
    else
      data_consumed = (u32)(tmp_stream + input->buff_len - input->stream);

    output->strm_curr_pos = (u8 *)tmp_stream;
    output->strm_curr_bus_address = input->stream_bus_address + data_consumed;
    if (output->strm_curr_bus_address >=
        (input->buffer_bus_address + input->buff_len))
      output->strm_curr_bus_address -= input->buff_len;

    output->data_left = input_data_len - data_consumed;
  }
  ASSERT(output->strm_curr_bus_address <=
         (input->buffer_bus_address + input->buff_len));

#ifdef DUMP_INPUT_STREAM

  if (output->strm_curr_pos >= input->stream)
    fwrite(input->stream, 1, (input_data_len - output->data_left),
           dec_cont->ferror_stream);
  else {
    fwrite(input->stream, 1, (u32)(input->buffer_bus_address + input->buff_len -
                                   input->stream_bus_address),
           dec_cont->ferror_stream);

    fwrite(input->buffer, 1,
           (u32)(output->strm_curr_bus_address - input->buffer_bus_address),
           dec_cont->ferror_stream);
  }
#endif

#ifdef USE_RANDOM_TEST
  if (output->data_left == input_data_len)
    dec_cont->stream_not_consumed = 1;
  else
    dec_cont->stream_not_consumed = 0;
#endif

#if 0  // FIXME: check other informations.
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
#endif
  Avs2DpbUpdateOutputList(dec_cont->storage.dpb, &dec_cont->storage.pps);

  FinalizeOutputAll(&dec_cont->fb_list);

  while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;
#ifdef PERFORMANCE_TEST
  SwActivityTraceStopDec(&dec_cont->activity);
#endif
  if (dec_cont->abort)
    return (DEC_ABORTED);
  else
    return (return_value);
}

/* Returns the SW and HW build information. */
Avs2DecBuild Avs2DecGetBuild(void) {
  Avs2DecBuild build_info;

  (void)DWLmemset(&build_info, 0, sizeof(build_info));

  build_info.sw_build = HANTRO_DEC_SW_BUILD;
  build_info.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_AVS2_DEC);

  DWLReadAsicConfig(&build_info.hw_config[0], DWL_CLIENT_TYPE_AVS2_DEC);

  return build_info;
}

/* Updates decoder instance after decoding of current picture */
void Avs2UpdateAfterPictureDecode(struct Avs2DecContainer *dec_cont) {
#if 1
  struct Avs2Storage *storage = &dec_cont->storage;

  Avs2ResetStorage(storage);

  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
#endif
}

/* Checks if active SPS is valid, i.e. supported in current profile/level */
u32 Avs2SpsSupported(const struct Avs2DecContainer *dec_cont) {
  const struct Avs2SeqParam *sps = &dec_cont->storage.sps;

  if (sps->cnt == 0) return 0;

  /* check picture size (minimum defined in decapicommon.h) */
  if (sps->pic_width_in_cbs * 8 > dec_cont->max_dec_pic_width ||
      sps->pic_height_in_cbs * 8 > dec_cont->max_dec_pic_height ||
      sps->pic_width_in_cbs * 8 < MIN_PIC_WIDTH ||
      sps->pic_height_in_cbs * 8 < MIN_PIC_HEIGHT) {
    DEBUG_PRINT(("Picture size not supported!\n"));
    return 0;
  }

  /* check hevc main 10 profile supported or not*/
  if (((sps->sample_bit_depth != 8)) && !dec_cont->main10_support) {
    DEBUG_PRINT(("AVS2 main 10 profile not supported!\n"));
    return 0;
  }

  return 1;
}

/* Checks if active PPS is valid, i.e. supported in current profile/level */
u32 Avs2PpsSupported(const struct Avs2DecContainer *dec_cont) {
  return dec_cont ? 1 : 0;
}

void Avs2ResetSeqParam(struct Avs2DecContainer *dec_cont) {
  dec_cont->storage.sps.cnt = 0;
  dec_cont->storage.pps.cnt = 0;
}

/* Allocates necessary memory buffers. */
u32 Avs2AllocateResources(struct Avs2DecContainer *dec_cont) {
  u32 ret = HANTRO_OK;
#if 0
  struct Avs2AsicBuffers *asic = &dec_cont->cmems;
  struct Avs2Storage *storage = &dec_cont->storage;

  const struct SeqParamSet *sps = storage->active_sps;

  SetDecRegister(dec_cont->hwdec.regs, HWIF_PIC_WIDTH_IN_CBS,
                 storage->pic_width_in_cbs);
  SetDecRegister(dec_cont->hwdec.regs, HWIF_PIC_HEIGHT_IN_CBS,
                 storage->pic_height_in_cbs);

  {
    u32 ctb_size = 1 << sps->log_max_coding_block_size;
    u32 pic_width_in_ctbs = storage->pic_width_in_ctbs * ctb_size;
    u32 pic_height_in_ctbs = storage->pic_height_in_ctbs * ctb_size;

    u32 partial_ctb_h = sps->pic_width != pic_width_in_ctbs ? 1 : 0;
    u32 partial_ctb_v = sps->pic_height != pic_height_in_ctbs ? 1 : 0;

    SetDecRegister(dec_cont->hwdec.regs, HWIF_PARTIAL_CTB_X, partial_ctb_h);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_PARTIAL_CTB_Y, partial_ctb_v);

    u32 min_cb_size = 1 << sps->log_min_coding_block_size;
    SetDecRegister(dec_cont->hwdec.regs, HWIF_PIC_WIDTH_4X4,
                   (storage->pic_width_in_cbs * min_cb_size) >> 2);

    SetDecRegister(dec_cont->hwdec.regs, HWIF_PIC_HEIGHT_4X4,
                   (storage->pic_height_in_cbs * min_cb_size) >> 2);
  }


  ReleaseAsicBuffers(dec_cont, asic);
  ret = AllocateAsicBuffers(dec_cont, asic);
  if (ret == HANTRO_OK) {
    ret = AllocateAsicTileEdgeMems(dec_cont);
  }
#endif

  return ret;
}

/* Performs picture freeze for output. */
void Avs2InitPicFreezeOutput(struct Avs2DecContainer *dec_cont,
                             u32 from_old_dpb) {

  u32 index = 0;
  const u8 *ref_data;
  struct Avs2Storage *storage = &dec_cont->storage;
  const u32 dvm_mem_size = storage->dmv_mem_size;
  void *dvm_base = (u8 *)storage->curr_image->data->virtual_address +
                   dec_cont->storage.dpb->dir_mv_offset;

#ifdef _DISABLE_PIC_FREEZE
  return;
#endif

  /* for concealment after a HW error report we use the saved reference list */
  struct Avs2DpbStorage *dpb = &storage->dpb[from_old_dpb];

  do {
    ref_data = Avs2GetRefPicData(dpb, dpb->list[index]);
    index++;
  } while (index < dpb->dpb_size && ref_data == NULL);

  ASSERT(dpb->current_out->data != NULL);

  /* Reset DMV storage for erroneous pictures */
  (void)DWLmemset(dvm_base, 0, dvm_mem_size);

  if (ref_data == NULL) {
    DEBUG_PRINT(("HevcInitPicFreezeOutput: pic freeze memset\n"));
    //(void)DWLPrivateAreaMemset(storage->curr_image->data->virtual_address,
    //128,
    //                storage->pic_size * 3 / 2);
    {
      if (dec_cont->storage.dpb->current_out->to_be_displayed)
        dec_cont->storage.dpb->num_out_pics_buffered--;
      if (dec_cont->storage.dpb->fullness > 0)
        dec_cont->storage.dpb->fullness--;
      dec_cont->storage.dpb->num_ref_frames--;
      dec_cont->storage.dpb->current_out->to_be_displayed = 0;
      dec_cont->storage.dpb->current_out->status = UNUSED;
      if (storage->raster_buffer_mgr) {
        if (!dec_cont->storage.dpb->current_out->first_field)
          RbmReturnPpBuffer(
              storage->raster_buffer_mgr,
              dec_cont->storage.dpb->current_out->pp_data->virtual_address);
      }
    }
  } else {
    DEBUG_PRINT(("HevcInitPicFreezeOutput: pic freeze memcopy\n"));
    (void)DWLPrivateAreaMemcpy(storage->curr_image->data->virtual_address,
                               ref_data,
                               storage->pic_size
                               +  NEXT_MULTIPLE(storage->pic_size / 2, MAX(16, ALIGN(dec_cont->align))));
    /* Copy compression table when existing. */
    if (dec_cont->use_video_compressor) {
      (void)DWLPrivateAreaMemcpy(
          (u8 *)storage->curr_image->data->virtual_address +
              dpb->cbs_tbl_offsety,
          ref_data + dpb->cbs_tbl_offsety, dpb->cbs_tbl_size);
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
void Avs2GetSarInfo(const struct Avs2Storage *storage, u32 *sar_width,
                    u32 *sar_height) {

  const struct Avs2SeqParam *sps;

  ASSERT(storage);
  sps = &storage->sps;

  if (storage->sps.cnt) {
    *sar_width = 0;
    *sar_height = 0;
  } else {
    i32 width = (storage->ext.display.cnt)
                    ? (storage->ext.display.display_horizontal_size)
                    : (sps->horizontal_size);
    i32 height = (storage->ext.display.cnt)
                     ? (storage->ext.display.display_vertical_size)
                     : (sps->vertical_size);
    switch (sps->aspect_ratio_information) {
      case 1:
        *sar_width = 1;
        *sar_height = 1;
        break;
      case 2:
        *sar_width = 4 * width;
        *sar_height = 3 * height;
        break;
      case 3:
        *sar_width = 16 * width;
        *sar_height = 9 * height;
        break;
      case 4:
        *sar_width = 221 * width / 100;
        *sar_height = height;
        break;
      case 0: /* forbiden */
      default:
        *sar_width = 0;
        *sar_height = 0;
        break;
    }
  }
}

/* Get last decoded picture if any available. No pictures are removed
 * from output nor DPB buffers. */
enum DecRet Avs2DecPeek(Avs2DecInst dec_inst, struct Avs2DecPicture *output) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  struct Avs2DpbPicture *current_out = dec_cont->storage.dpb->current_out;

  if (dec_inst == NULL || output == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state != AVS2DEC_NEW_HEADERS &&
      dec_cont->storage.dpb->fullness && current_out != NULL &&
      current_out->status != EMPTY) {

    u32 cropping_flag;

    output->pictures[0].output_picture = current_out->data->virtual_address;
    output->pictures[0].output_picture_bus_address =
        current_out->data->bus_address;
    output->pic_id = current_out->pic_id;
    output->decode_id = current_out->decode_id;
    output->type = current_out->type;

    output->pictures[0].pic_width = Avs2PicWidth(&dec_cont->storage);
    output->pictures[0].pic_height = Avs2PicHeight(&dec_cont->storage);

    Avs2CroppingParams(&dec_cont->storage, &cropping_flag,
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
  }

  return (DEC_OK);
}

enum DecRet Avs2DecNextPictureInternal(struct Avs2DecContainer *dec_cont) {
  struct Avs2DecPicture out_pic;
  struct Avs2DecInfo dec_info;
  const struct Avs2DpbOutPicture *dpb_out = NULL;
  u32 bit_depth, out_bit_depth;
  //u32 pic_width, pic_height;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 *output_picture = NULL;
//  addr_t output_picture_bus_address;
  u32 i;
  u32 pic_ready;
  memset(&out_pic, 0, sizeof(struct Avs2DecPicture));

  dpb_out = Avs2NextOutputPicture(&dec_cont->storage);

  if (dpb_out == NULL) return DEC_OK;

//  pic_width = dpb_out->pic_width;
//  pic_height = dpb_out->pic_height;
  bit_depth = dpb_out->sample_bit_depth;
  out_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8)
                      ? 8
                      : dec_cont->use_p010_output ? 16 : 10;
  out_pic.sample_bit_depth = dpb_out->sample_bit_depth;
  out_pic.output_bit_depth = dpb_out->output_bit_depth;
  out_pic.pic_id = dpb_out->pic_id;
  out_pic.decode_id = dpb_out->decode_id;
  out_pic.type = dpb_out->type;
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
      out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].scale.width / 2)
                                      << 1;
      out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].scale.height / 2)
                                       << 1;
      out_pic.pictures[i].output_picture =
          (u32 *)((addr_t)dpb_out->pp_data->virtual_address +
                  ppu_cfg->luma_offset);
      out_pic.pictures[i].output_picture_bus_address =
          dpb_out->pp_data->bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        out_pic.pictures[i].output_picture_chroma =
            (u32 *)((addr_t)dpb_out->pp_data->virtual_address +
                    ppu_cfg->chroma_offset);
        out_pic.pictures[i].output_picture_chroma_bus_address =
            dpb_out->pp_data->bus_address + ppu_cfg->chroma_offset;
      } else {
        out_pic.pictures[i].output_picture_chroma = NULL;
        out_pic.pictures[i].output_picture_chroma_bus_address = 0;
      }
      if (output_picture == NULL)
        output_picture = (u32 *)out_pic.pictures[i].output_picture;
      if ((ppu_cfg->out_p010 || ppu_cfg->tiled_e) && bit_depth > 8)
        out_pic.pictures[i].pixel_format = DEC_OUT_PIXEL_P010;
      else if (ppu_cfg->out_1010)
        out_pic.pictures[i].pixel_format = DEC_OUT_PIXEL_1010;
      else if (dec_cont->pixel_format == DEC_OUT_PIXEL_CUSTOMER1)
        out_pic.pictures[i].pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
      else if (ppu_cfg->out_cut_8bits)
        out_pic.pictures[i].pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
      else {
        out_pic.pictures[i].pixel_format = DEC_OUT_PIXEL_DEFAULT;
      }
    }
  } else {
    out_pic.pictures[0].output_format = dec_cont->output_format;
    out_pic.pictures[0].pic_height = dpb_out->pic_height;
    out_pic.pictures[0].pic_width = dpb_out->pic_width;
    if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
      out_pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(dpb_out->pic_width * out_bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
      out_pic.pictures[0].output_picture = dpb_out->pp_data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address =
          dpb_out->pp_data->bus_address;
      out_pic.pictures[0].output_picture_chroma =
          dpb_out->pp_data->virtual_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
      out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->pp_data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height;
    } else {
      out_pic.pictures[0].pic_stride = out_pic.pictures[0].pic_stride_ch =
          NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
      out_pic.pictures[0].output_picture = dpb_out->data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address =
          dpb_out->data->bus_address;
      out_pic.pictures[0].output_picture_chroma =
          dpb_out->data->virtual_address + out_pic.pictures[0].pic_stride *
                                               out_pic.pictures[0].pic_height /
                                               4 / 4;
      out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
      if (dec_cont->use_video_compressor) {
        /* No alignment when compressor is enabled. */
        //out_pic.pictures[0].pic_stride = 4 * dpb_out->pic_width * bit_depth / 8;
         out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * dpb_out->pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
         out_pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                                             ALIGN(dec_cont->align) * 8) / 8;
         out_pic.pictures[0].output_format = DEC_OUT_FRM_RFC;
      }
    }
    //out_pic.pictures[0].pic_stride_ch = out_pic.pictures[0].pic_stride;
    out_pic.output_rfc_luma_base = dpb_out->data->virtual_address +
                                   dec_cont->storage.dpb[0].cbs_tbl_offsety/4;
    out_pic.output_rfc_luma_bus_address =
        dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsety;
    out_pic.output_rfc_chroma_base = dpb_out->data->virtual_address +
                                     dec_cont->storage.dpb[0].cbs_tbl_offsetc/4;
    out_pic.output_rfc_chroma_bus_address =
        dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsetc;
    ASSERT(out_pic.pictures[0].output_picture);
    ASSERT(out_pic.pictures[0].output_picture_bus_address);
  }

  if (!dec_cont->pp_enabled) {
    if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
      if (dec_cont->use_p010_output && bit_depth > 8)
        out_pic.pictures[0].pixel_format = DEC_OUT_PIXEL_P010;
      else if (dec_cont->pixel_format == DEC_OUT_PIXEL_CUSTOMER1)
        out_pic.pictures[0].pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
      else if (dec_cont->use_8bits_output)
        out_pic.pictures[0].pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
      else
        out_pic.pictures[0].pixel_format = DEC_OUT_PIXEL_DEFAULT;
    } else {
      /* Reference buffer. */
      out_pic.pictures[0].pixel_format = DEC_OUT_PIXEL_DEFAULT;
    }
  }
  (void)Avs2DecGetInfo((Avs2DecInst)dec_cont, &dec_info);
  (void)DWLmemcpy(&out_pic.dec_info, &dec_info, sizeof(struct Avs2DecInfo));
  out_pic.dec_info.pic_buff_size = dec_cont->storage.dpb->tot_buffers;
  pic_ready = ((!dec_cont->pp_enabled) || (dec_cont->pp_enabled &&
               (!dpb_out->is_field_sequence ||
               (dpb_out->is_field_sequence &&
               ((dpb_out->top_field_first && !dpb_out->is_top_field) ||
               (!dpb_out->top_field_first && dpb_out->is_top_field))))));

  if (dec_cont->storage.raster_buffer_mgr)
    RbmSetPpBufferUsed(dec_cont->storage.raster_buffer_mgr, output_picture);

  if (pic_ready) {
    PushOutputPic(&dec_cont->fb_list, &out_pic, dpb_out->mem_idx);

  /* If reference buffer is not external, consume it and return it to DPB list.
   */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    PopOutputPic(&dec_cont->fb_list, dpb_out->mem_idx);
  }
  return (DEC_PIC_RDY);
}

enum DecRet Avs2DecNextPicture(Avs2DecInst dec_inst,
                               struct Avs2DecPicture *picture) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 ret;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state == AVS2DEC_EOS && IsOutputEmpty(&dec_cont->fb_list)) {
    return (DEC_END_OF_STREAM);
  }

  if ((ret = PeekOutputPic(&dec_cont->fb_list, picture))) {
    /*Abort output fifo */
    if (ret == ABORT_MARKER) return (DEC_ABORTED);
    if (ret == FLUSH_MARKER) return (DEC_FLUSHED);
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

enum DecRet Avs2DecPictureConsumed(Avs2DecInst dec_inst,
                                   const struct Avs2DecPicture *picture) {
  u32 id, i;
  const struct Avs2DpbStorage *dpb;
  struct Avs2DecPicture pic;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  struct Avs2Storage *storage = &dec_cont->storage;
  u32 *output_picture = NULL;
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
      if (pic.pictures[0].output_picture_bus_address ==
              dpb->pic_buffers[id].bus_address &&
          pic.pictures[0].output_picture ==
              dpb->pic_buffers[id].virtual_address) {
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
        output_picture = (u32 *)picture->pictures[i].output_picture;
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


enum DecRet Avs2DecEndOfStream(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* No need to call endofstream twice */
  if (dec_cont->dec_state == AVS2DEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }

  if (dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->hwdec.regs[1]);

    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id); /* release HW lock */

    DecrementDPBRefCount(dec_cont->storage.dpb);

    dec_cont->asic_running = 0;
  }

  /* flush any remaining pictures form DPB */
  Avs2FlushBuffer(&dec_cont->storage);

  FinalizeOutputAll(&dec_cont->fb_list);

  while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;

  /* After all output pictures were pushed, update decoder status to
   * reflect the end-of-stream situation. This way the Avs2DecNextPicture
   * will not block anymore once all output was handled.
   */
  dec_cont->dec_state = AVS2DEC_EOS;

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, -1);

#if !defined(AVS2_EXT_BUF_SAFE_RELEASE)
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

enum DecRet Avs2DecUseExtraFrmBuffers(Avs2DecInst dec_inst, u32 n) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  dec_cont->storage.n_extra_frm_buffers = n;

  return DEC_OK;
}

void Avs2CycleCount(struct Avs2DecContainer *dec_cont) {
  struct Avs2DpbPicture *current_out = dec_cont->storage.dpb->current_out;
  u32 cycles = 0;
  u32 mbs =
      (Avs2PicWidth(&dec_cont->storage) * Avs2PicHeight(&dec_cont->storage)) >>
      8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->hwdec.regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  current_out->cycles_per_mb = cycles;
}

#ifdef USE_FAST_EC
void Avs2DropCurrentPicutre(struct Avs2DecContainer *dec_cont) {
  struct Avs2Storage *storage = &dec_cont->storage;
  if (dec_cont->storage.dpb->current_out->to_be_displayed)
    dec_cont->storage.dpb->num_out_pics_buffered--;
  if (dec_cont->storage.dpb->fullness > 0) dec_cont->storage.dpb->fullness--;
  dec_cont->storage.dpb->num_ref_frames--;
  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
  dec_cont->storage.dpb->current_out->status = UNUSED;
  if (storage->raster_buffer_mgr)
    RbmReturnPpBuffer(
        storage->raster_buffer_mgr,
        dec_cont->storage.dpb->current_out->pp_data->virtual_address);

  if (dec_cont->storage.no_reordering) {
    dec_cont->storage.dpb->num_out--;
    if (dec_cont->storage.dpb->out_index_w == 0)
      dec_cont->storage.dpb->out_index_w = MAX_DPB_SIZE;
    else
      dec_cont->storage.dpb->out_index_w--;
    ClearOutput(dec_cont->storage.dpb->fb_list,
                dec_cont->storage.dpb->current_out->mem_idx);
  }
  (void)storage;
}
#endif

enum DecRet Avs2DecAddBuffer(Avs2DecInst dec_inst, struct DWLLinearMem *info) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
//  struct Avs2AsicBuffers *asic_buff = &dec_cont->storage.cmems;
  enum DecRet dec_ret = DEC_OK;

  struct Avs2Storage *storage = &dec_cont->storage;

  if (dec_inst == NULL || info == NULL ||
      X170_CHECK_VIRTUAL_ADDRESS(info->virtual_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->logical_size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  // if (dec_cont->buf_num == 0)
  //  return DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffer_size = info->size;

  switch (dec_cont->buf_type) {
#if 0
  case MISC_LINEAR_BUFFER:
    asic_buff->misc_linear = *info;
    dec_cont->buf_to_free = NULL;
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 0;
    break;
  case TILE_EDGE_BUFFER:
    asic_buff->tile_edge = *info;
    dec_cont->buf_to_free = NULL;
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 0;
    break;
#endif
    case REFERENCE_BUFFER: {
      i32 i = dec_cont->buffer_index;
      u32 id;
      struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;
#if 1
      if (i < dpb->tot_buffers) {
        dpb->pic_buffers[i] = *info;
        if (i < dpb->dpb_size + 1) {
          u32 id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
          if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

          dpb->buffer[i].data = dpb->pic_buffers + i;
          dpb->buffer[i].mem_idx = id;
          dpb->pic_buff_id[i] = id;
          dpb->buffer[i].to_be_displayed = HANTRO_FALSE;
          dpb->buffer[i].img_coi = -257;
          dpb->buffer[i].img_poi = -256;
          dpb->buffer[i].refered_by_others = 0;
          dpb->buffer[i].next_poc = 0x7FFFFFFF;
          dpb->buffer[i].first_field = 0;

        } else {
          id = AllocateIdFree(dpb->fb_list, dpb->pic_buffers + i);
          if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

          dpb->pic_buff_id[i] = id;
        }

        {
          void *base = (char *)(dpb->pic_buffers[i].virtual_address) +
                       dpb->dir_mv_offset;
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
        /* Here we need the allocate a USED id, so that this buffer will be
         * added as free buffer in SetFreePicBuffer. */
        id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
        if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;
        dpb->pic_buff_id[i] = id;

        {
          void *base = (char *)(dpb->pic_buffers[i].virtual_address) +
                       dpb->dir_mv_offset;
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
      dec_cont->buffer_num_added++;

      if (dec_cont->buffer_index < dpb->tot_buffers)
        dec_ret = DEC_WAITING_FOR_BUFFER;
      else if (storage->raster_enabled) {
        ASSERT(0);  // NOT allowed yet!!!
        /* Reference buffer done. */
        /* Continue to allocate raster scan / down scale buffer if needed. */
        //const struct Avs2SeqParam *sps = &storage->sps;
        //      struct DpbInitParams dpb_params;
        struct RasterBufferParams params;
        u32 pixel_width = Avs2SampleBitDepth(storage);
        u32 rs_pixel_width = (dec_cont->use_8bits_output || pixel_width == 8)
                                 ? 8
                                 : (dec_cont->use_p010_output ? 16 : 10);

        params.width = 0;
        params.height = 0;
        params.dwl = dec_cont->dwl;
        params.num_buffers = storage->dpb->tot_buffers;
        for (int i = 0; i < params.num_buffers; i++)
          dec_cont->tiled_buffers[i] = storage->dpb[0].pic_buffers[i];
        params.tiled_buffers = dec_cont->tiled_buffers;
        /* Raster out writes to modulo-16 widths. */
        params.width = NEXT_MULTIPLE(Avs2PicHeight(storage) * rs_pixel_width,
                                     ALIGN(dec_cont->align) * 8) /
                       8;
        ;
        params.height = Avs2PicWidth(storage);
        params.ext_buffer_config = dec_cont->ext_buffer_config;
        if (storage->pp_enabled) {
          params.width =
              NEXT_MULTIPLE(dec_cont->ppu_cfg[0].scale.width * rs_pixel_width,
                            ALIGN(dec_cont->ppu_cfg[0].align) * 8) /
              8;
          params.height = dec_cont->ppu_cfg[0].scale.height;
        }
        dec_cont->params = params;

        if (storage->raster_buffer_mgr) {
          dec_cont->_buf_to_free =
              RbmNextReleaseBuffer(storage->raster_buffer_mgr);

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

        if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config,
                               RASTERSCAN_OUT_BUFFER)) {
          /* Allocate raster scan / down scale buffers. */
          dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
          dec_cont->next_buf_size = params.width * params.height * 3 / 2;
          dec_cont->buf_to_free = NULL;
          dec_cont->buf_num = dec_cont->params.num_buffers;

          dec_cont->buffer_index = 0;
          dec_ret = DEC_WAITING_FOR_BUFFER;
        } else if (dec_cont->pp_enabled &&
                   IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config,
                                      DOWNSCALE_OUT_BUFFER)) {
          dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
          dec_cont->next_buf_size =
              dec_cont->params.width * dec_cont->params.height * 3 / 2;
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
      /* Since only one type of buffer will be set as external, we don't need to
       * switch to adding next buffer type. */
      /*
      else {
        dec_cont->buf_to_free = NULL;
        dec_cont->next_buf_size = 0;
        dec_cont->buffer_index = 0;
        dec_cont->buf_num = 0;
      #ifdef ASIC_TRACE_SUPPORT
        dec_cont->is_frame_buffer = 0;
      #endif
      }
      */
      break;
    }
    case RASTERSCAN_OUT_BUFFER: {
      u32 i = dec_cont->buffer_index;

      ASSERT(storage->raster_enabled);
      ASSERT(dec_cont->buffer_index < dec_cont->params.num_buffers);

      /* TODO(min): we don't add rs buffer now, instead the external rs buffer
       * is added */
      /*            to a queue. The decoder will get a rs buffer from fifo when
       * needed. */
      RbmAddPpBuffer(storage->raster_buffer_mgr, info, i);
      dec_cont->buffer_index++;
      dec_cont->buf_num--;
      dec_cont->buffer_num_added++;

      /* Need to add all the picture buffers in state AVS2DEC_WAIT_HEADER. */
      /* Reference buffer always allocated along with raster/dscale buffer. */
      /* Step for raster buffer. */
      if (dec_cont->buffer_index < dec_cont->min_buffer_num)
        dec_ret = DEC_WAITING_FOR_BUFFER;
      else {
        if (dec_cont->pp_enabled &&
            IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config,
                               DOWNSCALE_OUT_BUFFER)) {
          ASSERT(0);  // NOT allowed!
          dec_cont->buffer_index = 0;
          dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
          dec_cont->next_buf_size =
              dec_cont->params.width * dec_cont->params.height * 3 / 2;
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
      dec_cont->buffer_num_added++;

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

enum DecRet Avs2DecGetBufferInfo(Avs2DecInst dec_inst,
                                 struct Avs2DecBufferInfo *mem_info) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  //  enum DecRet dec_ret = DEC_OK;
  struct DWLLinearMem empty = {0};

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0) {
    if (dec_cont->rbm_release == 1) {
      /* All the raster buffers should be freed before being reallocated. */
      struct Avs2Storage *storage = &dec_cont->storage;

      ASSERT(storage->raster_buffer_mgr);
      if (storage->raster_buffer_mgr) {
        dec_cont->_buf_to_free =
            RbmNextReleaseBuffer(storage->raster_buffer_mgr);

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
          if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config,
                                 RASTERSCAN_OUT_BUFFER)) {
            dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
            dec_cont->next_buf_size = dec_cont->params.size;
          } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config,
                                        DOWNSCALE_OUT_BUFFER)) {
            dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
            dec_cont->next_buf_size = dec_cont->params.size;
          }
          dec_cont->buf_to_free = NULL;
          dec_cont->buf_num = dec_cont->params.num_buffers;
          dec_cont->buffer_index = 0;

          mem_info->buf_to_free = empty;
          mem_info->next_buf_size = dec_cont->next_buf_size;
          mem_info->buf_num = dec_cont->buf_num;
          return DEC_OK;
        }
      }

    } else {
      /* External reference buffer: release done. */
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = dec_cont->next_buf_size;
      mem_info->buf_num = dec_cont->buf_num;
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
  mem_info->buf_num = dec_cont->buf_num;
  //if (mem_info->buf_num == 1) return DEC_OK;
  ASSERT((mem_info->buf_num && mem_info->next_buf_size) ||
         (mem_info->buf_to_free.virtual_address != NULL));

  return DEC_WAITING_FOR_BUFFER;
}

void Avs2EnterAbortState(struct Avs2DecContainer *dec_cont) {
  SetAbortStatusInList(&dec_cont->fb_list);
  RbmSetAbortStatus(dec_cont->storage.raster_buffer_mgr);
  dec_cont->abort = 1;
}

void Avs2ExistAbortState(struct Avs2DecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  RbmClearAbortStatus(dec_cont->storage.raster_buffer_mgr);
  dec_cont->abort = 0;
}

enum DecRet Avs2DecAbort(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting and rs/ds buffer waiting */
  Avs2EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet Avs2DecAbortAfter(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

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
  if(dec_cont->dec_state == AVS2DEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  if (dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_E, 0);
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->hwdec.regs[1]);

    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id); /* release HW lock */

    DecrementDPBRefCount(dec_cont->storage.dpb);

    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures and internal parameters in DPB */
  Avs2EmptyDpb(dec_cont, dec_cont->storage.dpb);

  /* Clear any internal parameters in storage */
  Avs2ClearStorage(&(dec_cont->storage));

  /* Clear internal parameters in Avs2DecContainer */
  dec_cont->dec_state = AVS2DEC_INITIALIZED;
  dec_cont->start_code_detected = 0;
  dec_cont->pic_number = 0;
  dec_cont->packet_decoded = 0;

#ifdef USE_OMXIL_BUFFER
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size +
                               2; /* We need at least (dpb_size+2) output
                                     buffers before starting decoding. */
  else
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 1;
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->buffer_num_added = 0;
#endif

  Avs2ExistAbortState(dec_cont);

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

void Avs2DecSetNoReorder(Avs2DecInst dec_inst, u32 no_reorder) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  dec_cont->storage.no_reordering = no_reorder;
  if (dec_cont->storage.dpb != NULL)
    dec_cont->storage.dpb->no_reordering = no_reorder;
}

enum DecRet Avs2DecSetInfo(Avs2DecInst dec_inst,
                           struct Avs2DecConfig *dec_cfg) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  u32 pic_width = Avs2PicWidth(&dec_cont->storage);
  u32 pic_height = Avs2PicHeight(&dec_cont->storage);
  struct DecHwFeatures hw_feature;
  PpUnitConfig *ppu_cfg = &dec_cfg->ppu_cfg[0];
  u32 pixel_width = Avs2SampleBitDepth(&dec_cont->storage);
  u32 out_pixel_width = Avs2OutputBitDepth(&dec_cont->storage);

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (dec_cont->storage.sps.is_field_sequence) pic_height *= 2;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }
  if (pixel_width != out_pixel_width) {
    dec_cfg->ppu_cfg[0].out_cut_8bits = dec_cfg->ppu_cfg[1].out_cut_8bits =
        dec_cfg->ppu_cfg[2].out_cut_8bits = dec_cfg->ppu_cfg[3].out_cut_8bits =
            1;
  }
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, pixel_width,
                     !dec_cont->storage.sps.is_field_sequence, 0);

  dec_cont->pp_enabled =
      dec_cont->ppu_cfg[0].enabled | dec_cont->ppu_cfg[1].enabled |
      dec_cont->ppu_cfg[2].enabled | dec_cont->ppu_cfg[3].enabled |
      dec_cont->ppu_cfg[4].enabled;
  dec_cont->storage.pp_enabled = dec_cont->pp_enabled;
  dec_cont->hwdec.pp_enabled = dec_cont->pp_enabled;
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, pic_width, pic_height,
                        dec_cont->storage.sps.is_field_sequence,
                        dec_cont->ppu_cfg))
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
    dec_cont->ext_buffer_config = 1 << REFERENCE_BUFFER;
  Avs2SetExternalBufferInfo(dec_cont, &(dec_cont->storage));

  return (DEC_OK);
}
