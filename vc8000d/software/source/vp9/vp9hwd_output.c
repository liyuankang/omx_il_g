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

#include "dwlthread.h"
#include "basetype.h"
#include "decapicommon.h"
#include "dwl.h"
#include "fifo.h"
#include "regdrv.h"
#include "vp9decapi.h"
#include "vp9hwd_asic.h"
#include "vp9hwd_container.h"
#include "vp9hwd_output.h"
#include "vpufeature.h"

#define EOS_MARKER   (-1)
#define ABORT_MARKER (-2)
#define FLUSH_MARKER (-3)
#define NO_OUTPUT_MARKER (-4)


static u32 CycleCount(struct Vp9DecContainer *dec_cont);
static i32 FindIndex(struct Vp9DecContainer *dec_cont, const u32 *address, u32 buffer_type);
static i32 NextOutput(struct Vp9DecContainer *dec_cont);
static i32 Vp9ProcessAsicStatus(struct Vp9DecContainer *dec_cont,
                                u32 asic_status, u32 *error_concealment);
static void Vp9ConstantConcealment(struct Vp9DecContainer *dec_cont, u8 value);

u32 CycleCount(struct Vp9DecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
             NEXT_MULTIPLE(dec_cont->width, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->vp9_regs, HWIF_PERF_CYCLE_COUNT) / mbs;
  DEBUG_PRINT(("Pic %3d cycles/mb %4d\n", dec_cont->pic_number, cycles));

  return cycles;
}

i32 FindIndex(struct Vp9DecContainer *dec_cont, const u32 *address, u32 buffer_type) {
  i32 i;
  struct DWLLinearMem *pictures = NULL;
  i32 num_buffers;

  if (buffer_type == REFERENCE_BUFFER) {
    pictures = dec_cont->asic_buff->pictures;
    num_buffers = dec_cont->num_buffers;
  } else if (buffer_type == RASTERSCAN_OUT_BUFFER || buffer_type ==  DOWNSCALE_OUT_BUFFER) {
    pictures = dec_cont->asic_buff->pp_pictures;
    num_buffers = dec_cont->num_pp_buffers;
  }

  for (i = 0; i < (i32)num_buffers; i++)
    if ((*(pictures + i)).virtual_address == address) break;
  ASSERT((u32)i < num_buffers);
  return i;
}

i32 NextOutput(struct Vp9DecContainer *dec_cont) {
  i32 i;
  u32 j;
  i32 output_i = -1;
  u32 size;
  FifoObject tmp;
  i32 ret;

  if (dec_cont->abort)
    return ABORT_MARKER;

  size = FifoCount(dec_cont->fifo_display);

  /* If there are pictures in the display reordering buffer, check them
   * first to see if our next output is there. */
  for (j = 0; j < size; j++) {
    if ((ret = FifoPop(dec_cont->fifo_display, &tmp,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
                       FIFO_EXCEPTION_ENABLE
#else
                       FIFO_EXCEPTION_DISABLE
#endif
                      )) != FIFO_ABORT) {
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
      if (ret == FIFO_EMPTY) break;
#endif
      i = (i32)((addr_t)tmp);
      if (dec_cont->asic_buff->display_index[i] == dec_cont->pic_number) {
        /*  fifo_display had the right output. */
        output_i = i;
        break;
      } else {
        tmp = (FifoObject)(addr_t)i;
        FifoPush(dec_cont->fifo_display, tmp, FIFO_EXCEPTION_DISABLE);
      }
    } else
      return ABORT_MARKER;
  }

  /* Look for output in decode ordered out_fifo. */
  while (output_i < 0) {
    /* Blocks until next output is available */
    if ((ret = FifoPop(dec_cont->fifo_out, &tmp,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
                       FIFO_EXCEPTION_ENABLE
#else
                       FIFO_EXCEPTION_DISABLE
#endif
                      )) != FIFO_ABORT) {
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
      if (ret == FIFO_EMPTY) return NO_OUTPUT_MARKER;
#endif

      i = (i32)((addr_t)tmp);
      if (i == EOS_MARKER || i == FLUSH_MARKER) return i;

      if (dec_cont->asic_buff->display_index[i] == dec_cont->pic_number) {
        /*  fifo_out had the right output. */
        output_i = i;
      } else {
        /* Until we get the next picture in display order, push the outputs
        * to the display reordering fifo */
        tmp = (FifoObject)(addr_t)i;
        FifoPush(dec_cont->fifo_display, tmp, FIFO_EXCEPTION_DISABLE);
      }
    } else
      return ABORT_MARKER;
  }

  return output_i;
}

/*
void ResetBuffer(struct Vp9DecContainer *dec_cont, u32 buffer) {
  struct DWLLinearMem *pictures;
  struct DWLLinearMem empty = {0};

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN &&
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
    pictures = dec_cont->asic_buff->raster;
    pictures[buffer] = empty;
  }
}
*/

enum DecRet Vp9DecPictureConsumed(Vp9DecInst dec_inst,
                                  const struct Vp9DecPicture *picture) {
  if (dec_inst == NULL || picture == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  struct Vp9DecPicture pic = *picture;
  u32 buffer, i;
  const u32 *output_picture = NULL;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  if (!picture->pp_enabled) {
    output_picture = pic.pictures[0].output_luma_base;
  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = picture->pictures[i].output_luma_base;
        break;
      }
    }
  }
  /* For raster/dscale output buffer, return it to input buffer queue. */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER))
      buffer = FindIndex(dec_cont, output_picture, RASTERSCAN_OUT_BUFFER);
    else
      buffer = FindIndex(dec_cont, output_picture, DOWNSCALE_OUT_BUFFER);

    Vp9BufferQueueRemoveRef(dec_cont->pp_bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  /* FIXME: here only external buffer will be consumed, since only external buffer
   * bases addresses will be set when Vp9DecPictureConsumed() is called. */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    u32 buffer = FindIndex(dec_cont, output_picture, REFERENCE_BUFFER);

    /* Remove the reference to the buffer. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  return DEC_OK;
}

enum DecRet Vp9DecNextPicture(Vp9DecInst dec_inst,
                              struct Vp9DecPicture *output) {
  i32 i;
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  if (dec_inst == NULL || output == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return DEC_NOT_INITIALIZED;
  }

  /*  NextOutput will block until there is an output. */
  i = NextOutput(dec_cont);
  if (i == EOS_MARKER) {
    return DEC_END_OF_STREAM;
  }
  if (i == ABORT_MARKER) {
    return DEC_ABORTED;
  }

  if (i == FLUSH_MARKER) {
    return DEC_FLUSHED;
  }
  if (i == NO_OUTPUT_MARKER)
    return DEC_OK;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    ASSERT(i >= 0 && (u32)i < dec_cont->num_buffers);
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    ASSERT(i >= 0 && (u32)i < dec_cont->num_pp_buffers);
  }

  *output = dec_cont->asic_buff->picture_info[i];
#if 0
  output->pic_id = dec_cont->pic_number++;
#else
  dec_cont->pic_number++;
#endif

  /* FIXME: if tiled buffers are output while not to be used (not external buffers) ,
   * we need remove the reference to it here, since in ZTE's framework, this channel
   * won't be used and won't be consumed neither. That will cause these buffers
   * unavailable when tile+ds are configured. */
#if 0
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
      dec_cont->output_format == DEC_OUT_FRM_TILED_4X4) {
    u32 buffer = FindIndex(dec_cont, output->output_luma_base, REFERENCE_BUFFER);

    /* Remove the reference to the buffer. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, buffer);

    //pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    //dec_cont->asic_buff->display_index[buffer] = 0;

    //pthread_cond_signal(&dec_cont->sync_out_cv);
    //pthread_mutex_unlock(&dec_cont->sync_out);
  }
#endif

  return DEC_PIC_RDY;
}

enum DecRet Vp9DecEndOfStream(Vp9DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  u32 i;

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* Don't do end of stream twice. This is not thread-safe, so it must be
   * called from the single input thread that is also used to call
   * Vp9DecDecode. */
  if (dec_cont->dec_stat == VP9DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return DEC_END_OF_STREAM;
  }

  (void)VP9SyncAndOutput(dec_cont);

  /* Make sure the last pic pushed to output fifo in MC mode */
  for (i = 0; i < VP9DEC_MAX_PIC_BUFFERS; i++) {
    while (dec_cont->asic_buff->asic_busy[i])
      sched_yield();
  }


  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != VP9_UNDEFINED_BUFFER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;

      for (i = 0; i < VP9_REF_LIST_SIZE; i++) {
        i32 ref_buffer_i = Vp9BufferQueueGetRef(dec_cont->bq, i);
        if (ref_buffer_i != VP9_UNDEFINED_BUFFER) {
          Vp9BufferQueueRemoveRef(dec_cont->bq, ref_buffer_i);
        }
      }
    }
  }
  dec_cont->dec_stat = VP9DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_out, (void *)EOS_MARKER, FIFO_EXCEPTION_DISABLE);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  return DEC_OK;
}

void Vp9PicToOutput(struct Vp9DecContainer *dec_cont) {
  struct PicCallbackArg info = dec_cont->pic_callback_arg[dec_cont->core_id];

  FifoObject tmp;
  u32 ref_index = info.index;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
    info.index = dec_cont->asic_buff->pp_buffer_map[info.index];

  pthread_mutex_lock(&dec_cont->sync_out);
  while (dec_cont->asic_buff->display_index[info.index])
    pthread_cond_wait(&dec_cont->sync_out_cv, &dec_cont->sync_out);
  pthread_mutex_unlock(&dec_cont->sync_out);

  info.pic.cycles_per_mb = CycleCount(dec_cont);
#ifdef USE_VP9_EC
  info.pic.nbr_of_err_mbs = dec_cont->asic_buff->picture_info[ref_index].nbr_of_err_mbs;
#endif
  dec_cont->asic_buff->picture_info[info.index] = info.pic;
  /* MC not enabled or MC enabled but force to SC or show existing frame */
  if (info.show_frame && ((!dec_cont->b_mc ||
     (dec_cont->b_mc && info.force_to_sc)) ||
     (dec_cont->b_mc && info.show_existing_frame))) {
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[ref_index] == 0)
#endif
    {
      //dec_cont->asic_buff->display_index[info.index] = dec_cont->display_number++;
      dec_cont->asic_buff->display_index[info.index] = info.display_number;
      tmp = (FifoObject)(addr_t)info.index;
      FifoPush(dec_cont->fifo_out, tmp, FIFO_EXCEPTION_DISABLE);
      dec_cont->asic_buff->first_show[ref_index] = 1;
    }
#ifdef USE_PICTURE_DISCARD
    else {
      /* Remove the reference to the buffer. */

      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
          IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
        Vp9BufferQueueRemoveRef(dec_cont->pp_bq, info.index);

      if (dec_cont->output_format == DEC_OUT_FRM_TILED_4X4)
        Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);

      //dec_cont->asic_buff->display_index[info.index] = 0;
    }
#endif

    if (/*dec_cont->output_format != DEC_OUT_FRM_TILED_4X4*/!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);

#if 0
      pthread_mutex_lock(&dec_cont->sync_out);
      // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
      // be in the output queue once at a time.
      //dec_cont->asic_buff->display_index[info.index] = 0;

      pthread_cond_signal(&dec_cont->sync_out_cv);
      pthread_mutex_unlock(&dec_cont->sync_out);
#endif
    }
  }

}


void Vp9MCHwRdyCallback(void* arg, i32 core_id)
{
  u32 dec_regs[DEC_X170_REGISTERS];
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)arg;
  struct PicCallbackArg info;
  const void *dwl;
  u32 error_concealment;
  u32 core_status;
  u32 ref_index;
  i32 i;

  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES);

  info = dec_cont->pic_callback_arg[core_id];

  dwl = dec_cont->dwl;
  ref_index = info.index;

  /* read all hw regs */
  for (i = 0; i < DEC_X170_REGISTERS; i++) {
    dec_regs[i] = DWLReadReg(dwl, core_id, i * 4);
  }

  /* React to the HW return value */
  core_status = GetDecRegister(dec_regs, HWIF_DEC_IRQ_STAT);

  /* check if DEC_RDY, all other status are errors */
  if (core_status != DEC_HW_IRQ_RDY) {
    /* TODO(vmr): Handle unrecoverable system errors properly. */
    /* reset HW if still enabled */
    if (core_status & 0x01)
      DWLDisableHw(dwl, core_id, 0x04, 0);
    if(dec_cont->n_cores > 1) {
        /* Fake reference availability to allow pending cores to go. */
        DWLmemset(info.p_ref_status, 0xFF, 32);
    }
  }

  /* Let app know we're done with the buffer. */
  if (dec_cont->stream_consumed_callback.fn)
    dec_cont->stream_consumed_callback.fn((u8*)info.stream,
         (void*)info.p_user_data);

  /* Handle asic return status */
  (void)Vp9ProcessAsicStatus(dec_cont, core_status, &error_concealment);

  if (!info.pic.is_intra_frame) {
    Vp9BufferQueueRemoveRef(dec_cont->bq, info.prev_index);
    for (i = 0; i < VP9_ACTIVE_REFS; i++) {
      Vp9BufferQueueRemoveRef(dec_cont->bq, info.index_ref[i]);
    }
  }

  DWLReleaseHw(dwl, core_id);

  if (!info.force_to_sc) {
    FifoObject tmp;

    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
        IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
      info.index = dec_cont->asic_buff->pp_buffer_map[info.index];

    /* If frame is to be outputted, fill in the information before pushing to
     * output fifo. */

    if (info.show_frame) {
#ifdef USE_PICTURE_DISCARD
      if (dec_cont->asic_buff->first_show[ref_index] == 0)
#endif
      {
        tmp = (FifoObject)(addr_t)info.index;
        while(dec_cont->asic_buff->picture_info[info.index].display_id != info.pic.display_id)
          sched_yield();
        dec_cont->asic_buff->display_index[info.index] = info.display_number;
        FifoPush(dec_cont->fifo_out, tmp, FIFO_EXCEPTION_DISABLE);
        dec_cont->asic_buff->first_show[ref_index] = 1;
      }
#ifdef USE_PICTURE_DISCARD
      else {
        /* Remove the reference to the buffer. */

        if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
            IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
          Vp9BufferQueueRemoveRef(dec_cont->pp_bq, info.index);

        if (dec_cont->output_format == DEC_OUT_FRM_TILED_4X4)
          Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);

        dec_cont->asic_buff->display_index[info.index] = 0;
      }
#endif
      if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
        Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);
      }
    }
  }
  dec_cont->asic_buff->asic_busy[ref_index]  = 0;
}


void Vp9SetupPicToOutput(struct Vp9DecContainer *dec_cont, u32 pic_id) {
  struct PicCallbackArg *args = &dec_cont->pic_callback_arg[dec_cont->core_id];
  u32 bit_depth = dec_cont->decoder.bit_depth;
  u32 rs_bit_depth;
  u32 pp_index;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  struct DecHwFeatures hw_feature;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  rs_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8) ? 8 :
                 dec_cont->use_p010_output ? 16 : bit_depth;

  args->index = dec_cont->asic_buff->out_buffer_i;
  args->prev_index = dec_cont->asic_buff->prev_out_buffer_i;
  args->fifo_out = dec_cont->fifo_out;
  args->show_existing_frame = dec_cont->decoder.show_existing_frame;
  args->pic.num_tile_columns = (1 << dec_cont->decoder.log2_tile_columns);

  if (dec_cont->decoder.show_existing_frame) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[args->index];
    else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
             IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[dec_cont->asic_buff->pp_buffer_map[args->index]];
    args->pic.decode_id = pic_id;
    args->pic.is_intra_frame = 0;
    args->show_frame = 1;
    args->display_number = dec_cont->display_number++;
    args->pic.display_id = args->display_number;
    args->force_to_sc = dec_cont->force_to_sc;
    return;
  }
  args->show_frame = dec_cont->decoder.show_frame;
  if (args->show_frame) {
    args->display_number = dec_cont->display_number++;
  }
  /* Fill in the picture information for everything we know. */
  args->pic.display_id = args->display_number;
  args->pic.is_intra_frame = dec_cont->decoder.key_frame || dec_cont->decoder.intra_only;
  args->pic.is_golden_frame = 0;
  args->refresh_frame_flags = dec_cont->decoder.refresh_frame_flags;
  args->reset_frame_flags = dec_cont->decoder.reset_frame_flags;
  /* Frame size and format information. */
  args->pic.pictures[0].frame_width = NEXT_MULTIPLE(dec_cont->width, 8);
  args->pic.pictures[0].frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
  args->pic.coded_width = dec_cont->width;
  args->pic.coded_height = dec_cont->height;
  args->pic.pictures[0].output_format = dec_cont->output_format;
  args->pic.bit_depth_luma = args->pic.bit_depth_chroma = bit_depth;
  //args->pic.pic_stride = args->pic.frame_width * bit_depth / 8;
  args->pic.ref_pic_stride = dec_cont->asic_buff->out_stride[args->index];
  args->pic.pictures[0].pic_stride = dec_cont->asic_buff->out_stride[args->index];
  args->pic.pictures[0].pic_stride_ch = args->pic.pictures[0].pic_stride;
  pp_index = dec_cont->asic_buff->pp_buffer_map[args->index];
  args->pic.pp_enabled = 0;
  pp_index = dec_cont->asic_buff->pp_buffer_map[args->index];

  if (dec_cont->pp_enabled) {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
      args->pic.pp_enabled = dec_cont->pp_enabled;
      if (hw_feature.pp_version == G2_NATIVE_PP) {
        args->pic.pictures[i].frame_width = (dec_cont->width / 2 >> dec_cont->down_scale_x_shift) << 1;
        args->pic.pictures[i].frame_height = (dec_cont->height / 2 >> dec_cont->down_scale_y_shift) << 1;
      } else {
        if (dec_cont->ppu_cfg[i].crop2.enabled) {
          args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].crop2.width;
          args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].crop2.height;
        } else {
          args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
          args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        }
      }
      //args->pic.pic_stride = NEXT_MULTIPLE((decoded_width >> dec_cont->down_scale_x_shift) * rs_bit_depth, 16 * 8) / 8;
      args->pic.pictures[i].pic_stride = dec_cont->asic_buff->ds_stride[args->index][i];
      args->pic.pictures[i].pic_stride_ch = dec_cont->asic_buff->ds_stride_ch[args->index][i];
      args->pic.pictures[i].output_luma_base =
        (u32*)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index].virtual_address + ppu_cfg->luma_offset);
      args->pic.pictures[i].output_luma_bus_address =
        dec_cont->asic_buff->pp_pictures[pp_index].bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        args->pic.pictures[i].output_chroma_base =
          (u32*)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index].virtual_address + ppu_cfg->chroma_offset);
        args->pic.pictures[i].output_chroma_bus_address =
          dec_cont->asic_buff->pp_pictures[pp_index].bus_address + ppu_cfg->chroma_offset;
      } else {
        args->pic.pictures[i].output_chroma_base = NULL;
        args->pic.pictures[i].output_chroma_bus_address = 0;
      }
      args->pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      args->pic.pictures[i].out_bit_depth = ppu_cfg->pixel_width;
    }
  } else {
    if (dec_cont->use_video_compressor) {
      args->pic.pictures[0].output_format = DEC_OUT_FRM_RFC;
      if (hw_feature.rfc_stride_support) {
        args->pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(dec_cont->width, 8) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
        args->pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dec_cont->width, 8) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
      } else {
        args->pic.pictures[0].pic_stride = dec_cont->asic_buff->out_stride[args->index];
        args->pic.pictures[0].pic_stride_ch = dec_cont->asic_buff->out_stride[args->index];
      }
    }
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
    args->pic.pictures[0].pic_stride = dec_cont->asic_buff->rs_stride[args->index];
    //args->pic.pic_stride = NEXT_MULTIPLE(args->pic.frame_width * rs_bit_depth, 16 * 8) / 8;
    args->pic.pictures[0].output_luma_base =
      dec_cont->asic_buff->pp_pictures[pp_index].virtual_address;
    args->pic.pictures[0].output_luma_bus_address =
      dec_cont->asic_buff->pp_pictures[pp_index].bus_address;
    args->pic.pictures[0].output_chroma_base =
      dec_cont->asic_buff->pp_pictures[pp_index].virtual_address + dec_cont->asic_buff->pp_c_offset[args->index][0] / 4;
    args->pic.pictures[0].output_chroma_bus_address =
      dec_cont->asic_buff->pp_pictures[pp_index].bus_address + dec_cont->asic_buff->pp_c_offset[args->index][0];
  } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    args->pic.pictures[0].output_luma_base =
      dec_cont->asic_buff->pictures[args->index].virtual_address;
    args->pic.pictures[0].output_luma_bus_address =
      dec_cont->asic_buff->pictures[args->index].bus_address;
    args->pic.pictures[0].output_chroma_base =
      dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->pictures_c_offset[args->index] / 4;
    args->pic.pictures[0].output_chroma_bus_address =
      dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->pictures_c_offset[args->index];

    if (dec_cont->use_video_compressor) {
      /* Compression table info. */
      args->pic.output_rfc_luma_base =
        dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->cbs_y_tbl_offset [args->index] / 4;
      args->pic.output_rfc_luma_bus_address =
        dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->cbs_y_tbl_offset [args->index];
      args->pic.output_rfc_chroma_base =
        dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->cbs_c_tbl_offset [args->index] / 4;
      args->pic.output_rfc_chroma_bus_address =
        dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->cbs_c_tbl_offset [args->index];
    }
  }

  if (!dec_cont->pp_enabled)
    args->pic.pictures[0].out_bit_depth  = rs_bit_depth;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    args->pic.pictures[0].out_bit_depth  = bit_depth;
  args->pic.use_video_compressor = dec_cont->use_video_compressor;

  /* Finally, set the information we don't know yet to 0. */
  args->pic.nbr_of_err_mbs = 0; /* To be set after decoding. */
  args->pic.pic_id = pic_id;         /* To be set after output reordering. */
  args->pic.decode_id = pic_id;

  if (dec_cont->b_mc) {
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only) {
      for (i = 0; i < VP9_ACTIVE_REFS; i++) {
        args->index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec_cont->decoder.active_ref_idx[i]);
      }
    }
    args->stream = dec_cont->stream_consumed_callback.p_strm_buff;
    args->p_user_data = (void *)dec_cont->stream_consumed_callback.p_user_data;
    args->force_to_sc = dec_cont->force_to_sc;

    args->p_ref_status =
        (u8 *)dec_cont->asic_buff->pictures[args->index].virtual_address +
        dec_cont->asic_buff->sync_mc_offset[args->index];
    DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id,
                      Vp9MCHwRdyCallback, dec_cont);
  } else {
    DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id,
                      NULL, NULL);
  }
}

i32 Vp9ProcessAsicStatus(struct Vp9DecContainer *dec_cont, u32 asic_status,
                         u32 *error_concealment) {
  /* Handle system error situations */
  if (asic_status == VP9HWDEC_SYSTEM_TIMEOUT) {
    /* This timeout is DWL(software/os) generated */
    return DEC_HW_TIMEOUT;
  } else if (asic_status == VP9HWDEC_SYSTEM_ERROR) {
    return DEC_SYSTEM_ERROR;
  } else if (asic_status == VP9HWDEC_FATAL_SYSTEM_ERROR) {
    return DEC_FATAL_SYSTEM_ERROR;
  } else if (asic_status == VP9HWDEC_HW_RESERVED) {
    return DEC_HW_RESERVED;
  }

  /* Handle possible common HW error situations */
  if (asic_status & DEC_HW_IRQ_BUS) {
    return DEC_HW_BUS_ERROR;
  }

  /* for all the rest we will output a picture (concealed or not) */
  if ((asic_status & DEC_HW_IRQ_TIMEOUT) || (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_ASO) /* to signal lost residual */) {
    /* This timeout is HW generated */
    if (asic_status & DEC_HW_IRQ_TIMEOUT) {
#ifdef VP9HWTIMEOUT_ASSERT
      ASSERT(0);
#endif
      DEBUG_PRINT(("IRQ: HW TIMEOUT\n"));
    } else {
      DEBUG_PRINT(("IRQ: STREAM ERROR\n"));
    }

    /* normal picture freeze */
    *error_concealment = 1;
  } else if (asic_status & DEC_HW_IRQ_RDY) {
    DEBUG_PRINT(("IRQ: PICTURE RDY\n"));

    if (dec_cont->decoder.key_frame) {
      dec_cont->picture_broken = 0;
      dec_cont->force_intra_freeze = 0;
    }
  } else if (asic_status & DEC_HW_IRQ_EXT_TIMEOUT) {
    DEBUG_PRINT(("IRQ: EXT_TIMEOUT\n"));
  } else {
    ASSERT(0);
  }

  return DEC_OK;
}

i32 VP9SyncAndOutput(struct Vp9DecContainer *dec_cont) {
  i32 ret = 0;
  u32 asic_status;
  u32 error_concealment = 0;
#ifdef USE_VP9_EC
  struct PicCallbackArg *args = &dec_cont->pic_callback_arg[dec_cont->core_id];
#endif
  /* aliases */
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  /* If hw was running, sync with hw and output picture */
  if (dec_cont->asic_running) {
    if(dec_cont->b_mc && !args->force_to_sc) {
      /* reset shadow HW status reg values so that we dont end up writing
       * some garbage to next core regs */
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 0);
      dec_cont->asic_running = 0;
    } else {
      if (dec_cont->b_mc && args->force_to_sc) {
        while (dec_cont->asic_buff->asic_busy[args->index])
          sched_yield();

        /* reset shadow HW status reg values so that we dont end up writing
         * some garbage to next core regs */
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 0);
        dec_cont->asic_running = 0;
      } else {
        asic_status = Vp9AsicSync(dec_cont);

        /* Fix chroma RFC table for small resolution when necessary. */
        //Vp9FixChromaRFCTable(dec_cont);

        /* Handle asic return status */
        ret = Vp9ProcessAsicStatus(dec_cont, asic_status, &error_concealment);
        if (ret) return ret;
      }
    }

    /* Adapt probabilities */
    /* TODO should this be done after error handling? */
    Vp9UpdateProbabilities(dec_cont);
    /* Update reference frame flags */
    Vp9UpdateRefs(dec_cont, error_concealment);

    /* Store prev out info */
#ifdef USE_VP9_EC
    if (!error_concealment)
#else
    if (!error_concealment || dec_cont->intra_only || dec_cont->pic_number==1)
#endif
    {
      if (error_concealment) Vp9ConstantConcealment(dec_cont, 128);
      asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;

      Vp9PicToOutput(dec_cont);
    } else {
      dec_cont->picture_broken = 1;
      /*now dec_cont->display_number++ is put in the function Vp9SetupPicToOutput(), and this function will be called allways if HW is running,
        but when return error interrupt, it will not call Vp9PicToOutput to push into the queue, so need to decrease 1 to keep same with pic_number*/
      if (dec_cont->decoder.show_frame)
        dec_cont->display_number--;
#ifdef USE_VP9_EC
      asic_buff->picture_info[args->index].nbr_of_err_mbs = -1;
    if(((!dec_cont->decoder.error_resilient )&&(!dec_cont->decoder.frame_parallel_decoding))
        ||(dec_cont->decoder.refresh_entropy_probs))
        dec_cont->entropy_broken = 1;
#endif
    }
    asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;
  }
  return ret;
}

void Vp9ConstantConcealment(struct Vp9DecContainer *dec_cont, u8 pixel_value) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  i32 index = asic_buff->out_buffer_i;

  dec_cont->picture_broken = 1;
  // Size of picture (luma & chroma) is just the offset of dir mv,
  // which is stored next to picture buffer.
  DWLPrivateAreaMemset(asic_buff->pictures[index].virtual_address, pixel_value,
                       asic_buff->dir_mvs_offset[index]);

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
    DWLPrivateAreaMemset(asic_buff->pp_pictures[index].virtual_address, pixel_value,
                         asic_buff->pp_pictures[index].size);
  }
}

void Vp9EnterAbortState(struct Vp9DecContainer *dec_cont) {
  Vp9BufferQueueSetAbort(dec_cont->pp_bq);
  Vp9BufferQueueSetAbort(dec_cont->bq);
  FifoSetAbort(dec_cont->fifo_out);
  FifoSetAbort(dec_cont->fifo_display);
  dec_cont->abort = 1;
}

void Vp9ExistAbortState(struct Vp9DecContainer *dec_cont) {
  Vp9BufferQueueClearAbort(dec_cont->pp_bq);
  Vp9BufferQueueClearAbort(dec_cont->bq);
  FifoClearAbort(dec_cont->fifo_out);
  FifoClearAbort(dec_cont->fifo_display);
  dec_cont->abort = 0;
}


void Vp9EmptyBufferQueue(struct Vp9DecContainer *dec_cont) {
#ifdef USE_OMXIL_BUFFER
  u32 i;

  for (i = 0; i < dec_cont->num_buffers; i++) {
    Vp9BufferQueueEmptyRef(dec_cont->bq, i);
    dec_cont->asic_buff->display_index[i] = 0;
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    for (i = 0; i < dec_cont->num_pp_buffers; i++) {
      Vp9BufferQueueEmptyRef(dec_cont->pp_bq, i);
      dec_cont->asic_buff->display_index[i] = 0;
    }
  }
#endif
}

void Vp9ResetDecState(struct Vp9DecContainer *dec_cont) {
  dec_cont->dec_stat = VP9DEC_DECODING;
  dec_cont->add_buffer = 0;
  dec_cont->out_count = 0;
  dec_cont->active_segment_map = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->buffer_num_added = 0;
#endif
  dec_cont->picture_broken = 0;
  dec_cont->display_number = 1;
  dec_cont->pic_number = 1;
  dec_cont->intra_only = 0;
  dec_cont->conceal = 0;
  dec_cont->prev_is_key = 0;
  dec_cont->force_intra_freeze = 0;
  dec_cont->prob_refresh_detected = 0;
#ifdef USE_VP9_EC
  dec_cont->entropy_broken = 0;
#endif

  DWLmemset(&dec_cont->decoder, 0, sizeof(struct Vp9Decoder));
  DWLmemset(&dec_cont->bc, 0, sizeof(struct VpBoolCoder));

  Vp9AsicReset(dec_cont);
  DWLmemset(dec_cont->pic_callback_arg, 0, sizeof(struct PicCallbackArg));
  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
  FifoInit(VP9DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_out);
  FifoInit(VP9DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_display);
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->bq && IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
     dec_cont->num_buffers = dec_cont->num_buffers_reserved;
     Vp9BufferQueueRelease(dec_cont->bq, 0);
     dec_cont->bq = Vp9BufferQueueInitialize(dec_cont->num_buffers);
  }
#endif

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
#ifdef USE_OMXIL_BUFFER
    dec_cont->num_pp_buffers = 0;
#endif
    if (dec_cont->pp_bq) {
      Vp9BufferQueueReset(dec_cont->pp_bq);
    }
  }

  dec_cont->asic_buff->out_buffer_i = EMPTY_MARKER;
  dec_cont->asic_buff->out_pp_buffer_i = EMPTY_MARKER;
  dec_cont->no_decoding_buffer = 0;
}

enum DecRet Vp9DecAbort(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  enum FifoRet ret;
  FifoObject tmp;
  BufferQueue queue;
  FifoInst fifo = dec_cont->fifo_display;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* Before entering abort, remove all the pending output buffer from output/display fifo,
     since after Abort, FifoPop always returns FIFO_ABORT. */
  queue = IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) ? dec_cont->bq : dec_cont->pp_bq;

  while (1) {
    i32 i;
    ret = FifoPop(fifo, &tmp, FIFO_EXCEPTION_ENABLE);
    if (ret != FIFO_OK) {
      if (fifo == dec_cont->fifo_display) {
        fifo = dec_cont->fifo_out;
        continue;
      } else break;
    }
    i = (i32)((addr_t)tmp);

    Vp9BufferQueueRemoveRef(queue, i);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[i] = 0;
    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }


  /* Abort frame buffer waiting and rs/ds buffer waiting */
  Vp9EnterAbortState(dec_cont);

  if (dec_cont->no_decoding_buffer) {
    /* Release the buffer that have been got from buffer queue, but not ready for decoding. */
    if (dec_cont->bq && dec_cont->asic_buff->out_buffer_i >= 0) {
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
    }
    if (dec_cont->pp_bq && dec_cont->asic_buff->out_pp_buffer_i >= 0) {
      Vp9BufferQueueRemoveRef(dec_cont->pp_bq, dec_cont->asic_buff->out_pp_buffer_i);
    }
  }

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}

enum DecRet Vp9DecAbortAfter(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If a normal EOS is waited, return directly */
  if (dec_cont->dec_stat == VP9DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return DEC_OK;
  }
#endif

  /* If hw was running, stop and release hw */
  if (dec_cont->asic_running) {
    Vp9AsicSync(dec_cont);

    /* Remove the last picture that is being decoded when aborting. */
    struct PicCallbackArg info = dec_cont->pic_callback_arg[dec_cont->core_id];
    u32 ref_index = info.index;
    u32 pp_index;
    Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);
    pp_index = dec_cont->asic_buff->pp_buffer_map[ref_index];
    if (dec_cont->pp_bq)
      Vp9BufferQueueRemoveRef(dec_cont->pp_bq, pp_index);
  }
#if 0
  /* Stop and release HW */
  (void)VP9SyncAndOutput(dec_cont);

  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != VP9_UNDEFINED_BUFFER &&
        dec_cont->asic_buff->out_buffer_i != ABORT_MARKER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;

      for (i = 0; i < dec_cont->num_buffers; i++) {
        Vp9BufferQueueRemoveRef(dec_cont->bq,
                                Vp9BufferQueueGetRef(dec_cont->bq, i));
      }
    }
  }
#endif
  /* Clear reference count in buffer queue */
  Vp9EmptyBufferQueue(dec_cont);

  Vp9ResetDecState(dec_cont);

  /* Exist abort state */
  Vp9ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}
