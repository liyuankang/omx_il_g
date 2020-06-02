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

#include "basetype.h"
#include "dwl.h"
#include "regdrv.h"
#include "vp9hwd_asic.h"
#include "vp9hwd_container.h"
#include "vp9hwd_output.h"
#include "vp9hwd_probs.h"
#include "commonconfig.h"
#include "vp9_entropymv.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include <string.h>

#define DEC_MODE_VP9 13
#define MAX3(A,B,C) ((A)>(B)&&(A)>(C)?(A):((B)>(C)?(B):(C)))
extern struct TBCfg tb_cfg;

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
static void Vp9SetEmptyPictureData(struct Vp9DecContainer *dec_cont);
#endif
static i32 Vp9MallocRefFrm(struct Vp9DecContainer *dec_cont, u32 index);
static i32 Vp9AllocateSegmentMap(struct Vp9DecContainer *dec_cont);
static i32 Vp9FreeSegmentMap(struct Vp9DecContainer *dec_cont);
static i32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetTileInfo(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetReferenceFrames(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetSegmentation(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetLoopFilter(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetPictureDimensions(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetOutput(struct Vp9DecContainer *dec_cont);
static u32 RequiredBufferCount(struct Vp9DecContainer *dec_cont);

u32 RequiredBufferCount(struct Vp9DecContainer *dec_cont) {
  return (Vp9BufferQueueCountReferencedBuffers(dec_cont->bq) + 2);
}

void Vp9AsicInit(struct Vp9DecContainer *dec_cont) {

  u32 asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_VP9_DEC);
  DWLmemset(dec_cont->vp9_regs, 0, sizeof(dec_cont->vp9_regs));
  dec_cont->vp9_regs[0] = asic_id;

  SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_MODE, DEC_MODE_VP9);

  SetCommonConfigRegs(dec_cont->vp9_regs);
  SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_EC_BYPASS, dec_cont->use_video_compressor ? 0 : 1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_APF_ONE_PID, dec_cont->use_fetch_one_pic? 1 : 0);
}


i32 Vp9AsicAllocateMem(struct Vp9DecContainer *dec_cont) {

//  const void *dwl = dec_cont->dwl;
//  i32 dwl_ret;
  u32 size, i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

#if 0
  if (asic_buff->prob_tbl.virtual_address == NULL) {
    dec_cont->next_buf_size = sizeof(struct Vp9EntropyProbs);
    return DEC_WAITING_FOR_BUFFER;
  }

  if (asic_buff->ctx_counters.virtual_address == NULL) {
    dec_cont->next_buf_size = sizeof(struct Vp9EntropyCounts);
    return DEC_WAITING_FOR_BUFFER;
  }

  /* max number of tiles times width and height (2 bytes each),
   * rounding up to next 16 bytes boundary + one extra 16 byte
   * chunk (HW guys wanted to have this) */
  if (asic_buff->tile_info.virtual_address == NULL) {
    dec_cont->next_buf_size = (MAX_TILE_COLS * MAX_TILE_ROWS * 2 * sizeof(u16) + 15 + 16) & ~0xF;
    return DEC_WAITING_FOR_BUFFER;
  }
#else
  asic_buff->prob_tbl_offset = 0;
  asic_buff->ctx_counters_offset = asic_buff->prob_tbl_offset
                                   + NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 16);
  asic_buff->tile_info_offset = asic_buff->ctx_counters_offset
                                + NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16);

  size = NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 16)
         + NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16)
         + NEXT_MULTIPLE((MAX_TILE_COLS * MAX_TILE_ROWS * 4 * sizeof(u16) + 15 + 16) & ~0xF, 16);

  if (asic_buff->misc_linear[0].virtual_address == NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, MISC_LINEAR_BUFFER)) {
      dec_cont->next_buf_size = size;
      dec_cont->buf_to_free = NULL;
      dec_cont->buf_type = MISC_LINEAR_BUFFER;
      dec_cont->buf_num = 1;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      i32 ret = 0;
      for (i = 0; i < dec_cont->n_cores; i++) {
        asic_buff->misc_linear[i].mem_type = DWL_MEM_TYPE_VPU_WORKING;
        ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->misc_linear[i]);
        if (ret) return DEC_MEMFAIL;
      }
    }
  }
#endif

  return DEC_OK;
}

i32 Vp9AsicReleaseMem(struct Vp9DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->misc_linear[0].virtual_address != NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, MISC_LINEAR_BUFFER)) {
      dec_cont->buf_to_free = &asic_buff->misc_linear[0];
      dec_cont->next_buf_size = 0;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      for (i = 0; i < MAX_ASIC_CORES; i++) {
        if (asic_buff->misc_linear[i].virtual_address != NULL) {
          DWLFreeLinear(dec_cont->dwl, &asic_buff->misc_linear[i]);
          asic_buff->misc_linear[i].virtual_address = NULL;
          asic_buff->misc_linear[i].size = 0;
        }
      }
    }
  }

  return DWL_OK;
}

/* Allocate filter memories that are dependent on tile structure and height. */
i32 Vp9AsicAllocateFilterBlockMem(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_tile_cols = 1 << dec_cont->decoder.log2_tile_columns;
  u32 pixel_width = dec_cont->decoder.bit_depth;
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
  i32 dwl_ret;
  if (num_tile_cols < 2) return HANTRO_OK;

  u32 height32 = NEXT_MULTIPLE(asic_buff->height, 64);
  u32 size = 8 * height32 * (num_tile_cols - 1);    /* Luma filter mem */
  size += (8 + 8) * height32 * (num_tile_cols - 1); /* Chroma */
  size = size * pixel_width / 8;
  asic_buff->filter_mem_offset[core_id] = 0;
  asic_buff->bsd_control_mem_offset[core_id] = size;
  size += 16 * (height32 / 4) * (num_tile_cols - 1);

  if (asic_buff->tile_edge[core_id].logical_size >= size) return HANTRO_OK;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, TILE_EDGE_BUFFER)) {
    // Release old buffer.
    if (asic_buff->tile_edge[core_id].virtual_address != NULL) {
      dec_cont->buf_to_free = &asic_buff->tile_edge[core_id];
      dwl_ret = DEC_WAITING_FOR_BUFFER;
    } else {
      dec_cont->buf_to_free = NULL;
      dwl_ret = HANTRO_OK;
    }

    if (asic_buff->tile_edge[core_id].virtual_address == NULL) {
      dec_cont->next_buf_size = size;
      dec_cont->buf_type = TILE_EDGE_BUFFER;
      dec_cont->buf_num = 1;
      asic_buff->realloc_tile_edge_mem = 1;
      return DEC_WAITING_FOR_BUFFER;
    }
  } else {
    /* If already allocated, release the old, too small buffers. */
    Vp9AsicReleaseFilterBlockMem(dec_cont, core_id);

    asic_buff->tile_edge[core_id].mem_type = DWL_MEM_TYPE_VPU_ONLY;
    i32 dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->tile_edge[core_id]);
    if (dwl_ret != DWL_OK) {
      Vp9AsicReleaseFilterBlockMem(dec_cont, core_id);
      return HANTRO_NOK;
    }
  }

  (void)dwl_ret;

  return HANTRO_OK;
}

i32 Vp9AsicReleaseFilterBlockMem(struct Vp9DecContainer *dec_cont, u32 core_id) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, TILE_EDGE_BUFFER)) {
    if (asic_buff->tile_edge[core_id].virtual_address != NULL) {
      DWLFreeLinear(dec_cont->dwl, &asic_buff->tile_edge[core_id]);
      asic_buff->tile_edge[core_id].virtual_address = NULL;
      asic_buff->tile_edge[core_id].size = 0;
    }
  }

  return DWL_OK;
}

i32 Vp9AsicAllocatePictures(struct Vp9DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  dec_cont->active_segment_map = 0;
  if (Vp9AllocateSegmentMap(dec_cont) != HANTRO_OK)
    return DEC_WAITING_FOR_BUFFER;

  /*
    dec_cont->num_buffers += dec_cont->n_extra_frm_buffers;
    dec_cont->n_extra_frm_buffers = 0;
    if (dec_cont->num_buffers > VP9DEC_MAX_PIC_BUFFERS)
      dec_cont->num_buffers = VP9DEC_MAX_PIC_BUFFERS;
  */

  /* Require external picture buffers. */
  for (i = 0; i < dec_cont->num_buffers; i++) {
    if (Vp9MallocRefFrm(dec_cont, i) != HANTRO_OK)
      return DEC_WAITING_FOR_BUFFER;
  }

  //DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));

  /*
    dec_cont->bq = Vp9BufferQueueInitialize(dec_cont->num_buffers);
    if (dec_cont->bq == NULL) {
      Vp9AsicReleasePictures(dec_cont);
      return -1;
    }
  */

  ASSERT(asic_buff->width / 4 < 0x1FFF);
  ASSERT(asic_buff->height / 4 < 0x1FFF);
  SetDecRegister(dec_cont->vp9_regs, HWIF_MAX_CB_SIZE, 6); /* 64x64 */
  SetDecRegister(dec_cont->vp9_regs, HWIF_MIN_CB_SIZE, 3); /* 8x8 */

  asic_buff->out_buffer_i = -1;

  return HANTRO_OK;

}

void Vp9AsicReleasePictures(struct Vp9DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  //for (i = 0; i < dec_cont->num_buffers; i++) {
  for (i = 0; i < VP9DEC_MAX_PIC_BUFFERS; i++) {
    if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
        asic_buff->pictures[i].virtual_address != NULL)
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[i]);
  }


  if (dec_cont->bq) {
    Vp9BufferQueueRelease(dec_cont->bq, !IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER));
    dec_cont->bq = NULL;
  }

  if (dec_cont->pp_bq) {
    Vp9BufferQueueRelease(dec_cont->pp_bq,
                         (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) &&
                          !IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)));
    dec_cont->pp_bq = NULL;
  }

  DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));

  Vp9FreeSegmentMap(dec_cont);
}

i32 Vp9AllocateFrame(struct Vp9DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;

  if (Vp9MallocRefFrm(dec_cont, index)) ret = HANTRO_NOK;

  /* Following code will be excuted in Vp9DecAddBuffer(...) when the frame buffer is allocated externally. (bottom half) */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    dec_cont->num_buffers++;
    Vp9BufferQueueAddBuffer(dec_cont->bq);
  }

  return ret;
}

i32 Vp9ReallocateFrame(struct Vp9DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 out_index = 0;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    out_index = index;
  else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
           IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
    out_index = dec_cont->asic_buff->pp_buffer_map[index];

  pthread_mutex_lock(&dec_cont->sync_out);
  while (dec_cont->asic_buff->display_index[out_index])
    pthread_cond_wait(&dec_cont->sync_out_cv, &dec_cont->sync_out);

  /* Reallocate larger picture buffer into current index */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
      asic_buff->pictures[asic_buff->out_buffer_i].logical_size < asic_buff->picture_size) {
    if (asic_buff->pictures[index].virtual_address != NULL)
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[index]);
    asic_buff->pictures[index].mem_type = DWL_MEM_TYPE_DPB;
    ret |= DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size, &asic_buff->pictures[index]);
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
    // All the external buffer free/malloc will be done in bottom half in AddBuffer()...
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      dec_cont->buf_to_free = &asic_buff->pictures[index];
      dec_cont->next_buf_size = asic_buff->picture_size;
      dec_cont->buf_type = REFERENCE_BUFFER;
      dec_cont->buffer_index = asic_buff->out_buffer_i;
      asic_buff->realloc_out_buffer = 1;
      dec_cont->buf_num = 1;
      ret = DEC_WAITING_FOR_BUFFER;
    } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) &&
               asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size) {
      dec_cont->buf_to_free = &asic_buff->pp_pictures[asic_buff->pp_buffer_map[index]];
      dec_cont->next_buf_size = asic_buff->pp_size;
      dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
      dec_cont->buffer_index = asic_buff->out_pp_buffer_i;
      asic_buff->realloc_out_buffer = 1;
      dec_cont->buf_num = 1;
      ret = DEC_WAITING_FOR_BUFFER;
    }
    else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) &&
             asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size) {
      dec_cont->buf_to_free = &asic_buff->pp_pictures[asic_buff->pp_buffer_map[index]];
      dec_cont->next_buf_size = asic_buff->pp_size;
      dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
      dec_cont->buffer_index = asic_buff->out_pp_buffer_i;
      asic_buff->realloc_out_buffer = 1;
      dec_cont->buf_num = 1;
      ret = DEC_WAITING_FOR_BUFFER;
    }
  }

  pthread_mutex_unlock(&dec_cont->sync_out);

  return ret;
}

i32 Vp9MallocRefFrm(struct Vp9DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size;
  u32 pp_luma_size, pp_chroma_size, pp_size = 0;
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth, rs_bit_depth;
  i32 dwl_ret = DWL_OK;
  u32 out_w, out_h, i, out_w_2;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct DecHwFeatures hw_feature;
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  PpUnitIntConfig *ppu_cfg;

  bit_depth = dec_cont->decoder.bit_depth;
  rs_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8) ? 8 :
                 (dec_cont->use_p010_output) ? 16 : bit_depth;

  /* No stride when compression is used. */
  if (dec_cont->use_video_compressor)
    out_w = 4 * asic_buff->width * bit_depth / 8;
  else
    out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  out_h = asic_buff->height / 4;
  luma_size = out_w * out_h;
  chroma_size = luma_size / 2;
  asic_buff->out_stride[index] = out_w;
  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
    out_w = NEXT_MULTIPLE(asic_buff->width * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_h = asic_buff->height;
    pp_luma_size = out_w * out_h;
    pp_chroma_size = NEXT_MULTIPLE(pp_luma_size / 2, 16);
    asic_buff->rs_stride[index] = out_w;
    pp_size = pp_luma_size + pp_chroma_size;
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) && dec_cont->pp_enabled) {
    if (!hw_feature.flexible_scale_support) {
      out_w = asic_buff->width >> dec_cont->down_scale_x_shift;
      out_h = asic_buff->height >> dec_cont->down_scale_y_shift;
      out_w = NEXT_MULTIPLE(out_w * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      pp_luma_size = out_w * out_h;
      pp_chroma_size = NEXT_MULTIPLE(pp_luma_size / 2, ALIGN(dec_cont->align));
      asic_buff->ds_stride[index][0] = out_w;
      pp_size = NEXT_MULTIPLE(pp_luma_size + pp_chroma_size, 16);
    } else {
      pp_luma_size = pp_chroma_size = 0;
      ppu_cfg = &dec_cont->ppu_cfg[0];
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!dec_cont->ppu_cfg[i].enabled)
          continue;
        out_w = dec_cont->ppu_cfg[i].ystride;
        out_w_2 = dec_cont->ppu_cfg[i].cstride;
        if (!dec_cont->ppu_cfg[i].tiled_e)
          out_h = dec_cont->ppu_cfg[i].scale.height;
        else
          out_h = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.height, 4) / 4;
        pp_luma_size = out_w * out_h;
        if (!dec_cont->ppu_cfg[i].monochrome) {
          if (dec_cont->ppu_cfg[i].planar)
            out_h = dec_cont->ppu_cfg[i].scale.height;
          else {
            if (!dec_cont->ppu_cfg[i].tiled_e)
              out_h = dec_cont->ppu_cfg[i].scale.height / 2;
            else
              out_h = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.height/2, 4) / 4;
          }
          pp_chroma_size = out_w_2 * out_h;
        }
        asic_buff->pp_y_offset[index][i] = pp_size;
        asic_buff->pp_c_offset[index][i] = pp_size + pp_luma_size;
        asic_buff->ds_stride[index][i] = out_w;
        asic_buff->ds_stride_ch[index][i] = out_w_2;
        pp_size += NEXT_MULTIPLE(pp_luma_size + pp_chroma_size, 16);
      }
    }
  }

  if (dec_cont->use_video_compressor) {
    pic_width_in_cbsy = (asic_buff->width + 8 - 1)/8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1)/16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1)/8;
    pic_height_in_cbsc = (asic_buff->height/2 + 4 - 1)/4;

    /* luma table size */
    luma_table_size = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, ref_buffer_align);
    /* chroma table size */
    chroma_table_size = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, ref_buffer_align);
  } else {
    luma_table_size = chroma_table_size = 0;
  }

  /* luma */
  asic_buff->pictures_c_offset[index] = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  asic_buff->dir_mvs_offset[index] = asic_buff->pictures_c_offset[index]
                                     + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                                     + NEXT_MULTIPLE(32, ref_buffer_align);//32 byts for MC sync mem

  asic_buff->sync_mc_offset[index] = asic_buff->dir_mvs_offset[index] - 32;
  if (dec_cont->use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] = asic_buff->dir_mvs_offset[index]
                                         + dir_mvs_size;
    asic_buff->cbs_c_tbl_offset[index] = asic_buff->cbs_y_tbl_offset[index]
                                         + luma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
  }
  /* 32 bytes for MC sync mem */
  asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                            + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                            + NEXT_MULTIPLE(32, ref_buffer_align)
                            + dir_mvs_size + luma_table_size + chroma_table_size;
  asic_buff->pp_size = pp_size;

  if (asic_buff->pictures[index].virtual_address == NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      dec_cont->next_buf_size = asic_buff->picture_size;
      dec_cont->buf_type = REFERENCE_BUFFER;
      if (index == 0)
        dec_cont->buf_num = dec_cont->num_buffers;
      else
        dec_cont->buf_num = 1;
      dec_cont->buffer_index = index;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      asic_buff->pictures[index].mem_type = DWL_MEM_TYPE_DPB;
      dwl_ret |= DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size, &asic_buff->pictures[index]);
    }
  }

  if (index < dec_cont->min_buffer_num) {
    if (asic_buff->pp_pictures[index].virtual_address == NULL
        && dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
        dec_cont->next_buf_size = asic_buff->pp_size;
        dec_cont->buf_type = RASTERSCAN_OUT_BUFFER;
        if (index == 0)
          dec_cont->buf_num = dec_cont->min_buffer_num;
        else dec_cont->buf_num = 1;
        return DEC_WAITING_FOR_BUFFER;
      }
    }

    if (asic_buff->pp_pictures[index].virtual_address == NULL && dec_cont->pp_enabled) {
      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
        dec_cont->next_buf_size = asic_buff->pp_size;
        dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
        if (index == 0)
          dec_cont->buf_num = dec_cont->min_buffer_num;
        else dec_cont->buf_num = 1;
        return DEC_WAITING_FOR_BUFFER;
      }
    }
  }

  if (dwl_ret != DWL_OK) {
    Vp9AsicReleasePictures(dec_cont);
    return HANTRO_NOK;
  }

  return HANTRO_OK;
}

#if 0
i32 Vp9FreeRefFrm(struct Vp9DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->pictures[index].virtual_address != NULL) {
    /* Reallocate bigger picture buffer into current index */
    dec_cont->buffer_index = index;
    dec_cont->buf_to_free = &asic_buff->pictures[index];
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 1;
    dec_cont->buf_type = REFERENCE_BUFFER;
    return DEC_WAITING_FOR_BUFFER;
  }
  return HANTRO_OK;
}
#endif

void Vp9SetExternalBufferInfo(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size;
  u32 raster_luma_size, raster_chroma_size;
  u32 pp_luma_size, pp_chroma_size, pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth, rs_bit_depth;
  u32 picture_size, raster_size, dscale_size, buff_size;
  u32 min_buffer_num;
  enum BufferType buf_type;
  u32 out_w, out_h;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct DecHwFeatures hw_feature;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  bit_depth = dec_cont->decoder.bit_depth;
  rs_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8) ? 8 :
                 (dec_cont->use_p010_output) ? 16 : bit_depth;

  Vp9GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                     &rfc_luma_size, &rfc_chroma_size);

  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */
  out_w = NEXT_MULTIPLE(asic_buff->width * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  out_h = asic_buff->height;
  raster_luma_size = out_w * out_h;
  raster_chroma_size = raster_luma_size / 2;
  if (dec_cont->pp_enabled) {
    if (!hw_feature.flexible_scale_support) {
      out_w = asic_buff->width >> dec_cont->down_scale_x_shift;
      out_h = asic_buff->height >> dec_cont->down_scale_y_shift;
      out_w = NEXT_MULTIPLE(out_w * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      pp_luma_size = out_w * out_h;
      pp_chroma_size = NEXT_MULTIPLE(pp_luma_size / 2, ALIGN(dec_cont->align));
      pp_size = pp_luma_size + pp_chroma_size;
    } else {
      pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);
    }
  }

  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  // Add 32bytes for MCsync mem
  picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                 + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                 + NEXT_MULTIPLE(32, ref_buffer_align)
                 + dir_mvs_size + luma_table_size + chroma_table_size;
  raster_size = raster_luma_size + raster_chroma_size;
  if (dec_cont->pp_enabled)
    dscale_size = pp_size;
  else
    dscale_size = 0;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dec_cont->min_buffer_num;
    buff_size = picture_size;
    buf_type = REFERENCE_BUFFER;
  } else {
    min_buffer_num = dec_cont->min_buffer_num;
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      buff_size = dscale_size;
      buf_type = DOWNSCALE_OUT_BUFFER;
    } else
    {
      buff_size = raster_size;
      buf_type = RASTERSCAN_OUT_BUFFER;
    }
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = buff_size;
  dec_cont->buf_type = buf_type;
}

void Vp9CalculateBufSize(struct Vp9DecContainer *dec_cont, i32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size;
  u32 pp_luma_size, pp_chroma_size, pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth, rs_bit_depth;
  u32 out_w, out_w_2, out_h, i;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct DecHwFeatures hw_feature;
  PpUnitIntConfig *ppu_cfg;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  bit_depth = dec_cont->decoder.bit_depth;
  rs_bit_depth = (dec_cont->use_8bits_output || bit_depth == 8) ? 8 :
                 (dec_cont->use_p010_output) ? 16 : bit_depth;

  Vp9GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                     &rfc_luma_size, &rfc_chroma_size);

  if (dec_cont->use_video_compressor)
    out_w = 4 * asic_buff->width * bit_depth / 8;
  else
    out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  asic_buff->out_stride[index] = out_w;
  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER)) {
    out_w = NEXT_MULTIPLE(asic_buff->width * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_h = asic_buff->height;
    pp_luma_size = out_w * out_h;
    pp_chroma_size = NEXT_MULTIPLE(pp_luma_size / 2, ALIGN(dec_cont->align));
    asic_buff->rs_stride[index] = out_w;
    pp_size = pp_luma_size + pp_chroma_size;
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) && dec_cont->pp_enabled) {
    if (!hw_feature.flexible_scale_support) {
      out_w = asic_buff->width >> dec_cont->down_scale_x_shift;
      out_h = asic_buff->height >> dec_cont->down_scale_y_shift;
      out_w = NEXT_MULTIPLE(out_w * rs_bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      pp_luma_size = out_w * out_h;
      pp_chroma_size = NEXT_MULTIPLE(pp_luma_size / 2, ALIGN(dec_cont->align));
      asic_buff->ds_stride[index][0] = out_w;
      pp_size = NEXT_MULTIPLE(pp_luma_size + pp_chroma_size, 16);
    } else {
      pp_luma_size = pp_chroma_size = 0;
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!dec_cont->ppu_cfg[i].enabled)
          continue;
        out_w = dec_cont->ppu_cfg[i].ystride;
        out_w_2 = dec_cont->ppu_cfg[i].cstride;
        if (!dec_cont->ppu_cfg[i].tiled_e)
          out_h = dec_cont->ppu_cfg[i].scale.height;
        else
          out_h = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.height, 4) / 4;
        pp_luma_size = out_w * out_h;
        if (!dec_cont->ppu_cfg[i].monochrome) {
          if (dec_cont->ppu_cfg[i].planar)
            out_h = dec_cont->ppu_cfg[i].scale.height;
          else {
            if (!dec_cont->ppu_cfg[i].tiled_e)
              out_h = dec_cont->ppu_cfg[i].scale.height / 2;
            else
              out_h = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.height/2, 4) / 4;
          }
          pp_chroma_size = out_w_2 * out_h;
        }
        asic_buff->pp_y_offset[index][i] = pp_size;
        asic_buff->pp_c_offset[index][i] = pp_size + pp_luma_size;
        asic_buff->ds_stride[index][i] = out_w;
        asic_buff->ds_stride_ch[index][i] = out_w_2;
        pp_size += NEXT_MULTIPLE(pp_luma_size + pp_chroma_size, 16);
      }
    }
  }

  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                            + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                            + NEXT_MULTIPLE(32, ref_buffer_align)
                            + dir_mvs_size + luma_table_size + chroma_table_size;
  asic_buff->pp_size = pp_size;

  asic_buff->pictures_c_offset[index] = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  asic_buff->dir_mvs_offset[index] = asic_buff->pictures_c_offset[index]
                                     + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                                     + NEXT_MULTIPLE(32, ref_buffer_align); //32 byts for MC sync mem
  asic_buff->sync_mc_offset[index] = asic_buff->dir_mvs_offset[index] - 32;

  if (dec_cont->use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] = asic_buff->dir_mvs_offset[index] + dir_mvs_size;
    asic_buff->cbs_c_tbl_offset[index] = asic_buff->cbs_y_tbl_offset[index] + luma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
  }
}

i32 Vp9GetRefFrm(struct Vp9DecContainer *dec_cont, u32 id) {
  u32 limit = dec_cont->dynamic_buffer_limit;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (RequiredBufferCount(dec_cont) < limit)
    limit = RequiredBufferCount(dec_cont);

  if (asic_buff->realloc_tile_edge_mem) return HANTRO_OK;

  if (!asic_buff->realloc_out_buffer && !asic_buff->realloc_seg_map_buffer) {
    if (!dec_cont->no_decoding_buffer || asic_buff->out_buffer_i == EMPTY_MARKER) {
      asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);
      if (asic_buff->out_buffer_i >= 0 && asic_buff->out_buffer_i < VP9DEC_MAX_PIC_BUFFERS)
        asic_buff->first_show[asic_buff->out_buffer_i] = 0;
      if (asic_buff->out_buffer_i == ABORT_MARKER) {
        return DEC_ABORTED;
      }
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else if (asic_buff->out_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = EMPTY_MARKER;
        return DEC_NO_DECODING_BUFFER;
      }
#endif
      else if (asic_buff->out_buffer_i < 0) {
        if (Vp9AllocateFrame(dec_cont, dec_cont->num_buffers)) {
          /* Request for a new buffer. */
          asic_buff->realloc_out_buffer = 0;
          return DEC_WAITING_FOR_BUFFER;
        }
        asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);
      }

#ifdef USE_VP9_EC
      asic_buff->picture_info[asic_buff->out_buffer_i].nbr_of_err_mbs = 0;
#endif
      /* Caculate the buffer size required for current picture. */
      Vp9CalculateBufSize(dec_cont, asic_buff->out_buffer_i);
    }

    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
        IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      if (!dec_cont->no_decoding_buffer || asic_buff->out_pp_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->pp_bq, 0);
        if(asic_buff->out_pp_buffer_i == ABORT_MARKER) {
          return DEC_ABORTED;
        }
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else if (asic_buff->out_pp_buffer_i == EMPTY_MARKER) {
          return DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      asic_buff->pp_buffer_map[asic_buff->out_buffer_i] = asic_buff->out_pp_buffer_i;
    }
  }

  /* Reallocate picture memories if needed */
  if (asic_buff->pictures[asic_buff->out_buffer_i].logical_size < asic_buff->picture_size ||
      (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) &&
       asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size)
      || (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) &&
          asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size)
     ) {
    i32 dwl_ret = Vp9ReallocateFrame(dec_cont, asic_buff->out_buffer_i);
    if (dwl_ret) return dwl_ret;
  }

  asic_buff->realloc_out_buffer = 0;

  if (Vp9ReallocateSegmentMap(dec_cont) != HANTRO_OK) {
    asic_buff->realloc_seg_map_buffer = 1;
    return DEC_WAITING_FOR_BUFFER;
  }
  asic_buff->realloc_seg_map_buffer = 0;
  return HANTRO_OK;
}

/* Reallocates segment maps if resolution changes bigger than initial
   resolution. Needs synchronization if SW is running parallel with HW */
i32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont) {
  i32 dwl_ret;
  u32 num_ctbs, memory_size;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */

  /* Do nothing if we have big enough buffers for segment maps,
     Actually we allocate largest segment maps at the beginning,
     so the reallocating should never be happen here  */
  if (memory_size <= asic_buff->segment_map_size) return HANTRO_OK;

  /* Allocate new segment maps for larger resolution */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, SEGMENT_MAP_BUFFER)) {
    dec_cont->buf_to_free = NULL;
    dec_cont->next_buf_size = memory_size * 2;
    asic_buff->segment_map_size_new = memory_size;
    dec_cont->buf_type = SEGMENT_MAP_BUFFER;
    return DEC_WAITING_FOR_BUFFER;
  } else {
    /* Allocate new segment maps for larger resolution */
    struct DWLLinearMem new_segment_map;
    i32 new_segment_size = memory_size;

    new_segment_map.mem_type = DWL_MEM_TYPE_VPU_ONLY;
    dwl_ret = DWLMallocLinear(dec_cont->dwl, memory_size * 2, &new_segment_map);

    if (dwl_ret != DWL_OK) {
      //Vp9AsicReleasePictures(dec_cont);
      return HANTRO_NOK;
    }
    /* Copy existing segment maps into new buffers */
    DWLmemcpy(new_segment_map.virtual_address,
              asic_buff->segment_map[0].virtual_address,
              asic_buff->segment_map_size);
    DWLmemcpy((u8 *)new_segment_map.virtual_address + new_segment_size,
              (u8 *)asic_buff->segment_map[0].virtual_address + asic_buff->segment_map_size,
              asic_buff->segment_map_size);
    /* Free old segment maps */
    Vp9FreeSegmentMap(dec_cont);

    asic_buff->segment_map_size = new_segment_size;
    asic_buff->segment_map[0] = new_segment_map;
  }

  return HANTRO_OK;
}


i32 Vp9AllocateSegmentMap(struct Vp9DecContainer *dec_cont) {
  i32 dwl_ret;
  u32 num_ctbs, memory_size, i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id= DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* Set segment map with max width/height */
  num_ctbs = ((hw_feature.vp9_max_dec_pic_width + 63) / 64) * ((hw_feature.vp9_max_dec_pic_height + 63) / 64);
  //num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */

  /* Do nothing if we have big enough buffers for segment maps */
  if (memory_size <= asic_buff->segment_map_size) return HANTRO_OK;

  /* Free old segment maps */
  if (asic_buff->segment_map[0].virtual_address)
    Vp9FreeSegmentMap(dec_cont);

  if (asic_buff->segment_map[0].virtual_address == NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, SEGMENT_MAP_BUFFER)) {
      dec_cont->next_buf_size = memory_size * 2;
      dec_cont->buf_to_free = NULL;
      dec_cont->buf_type = SEGMENT_MAP_BUFFER;
      asic_buff->segment_map_size = memory_size;
      asic_buff->segment_map_size_new = memory_size;
      dec_cont->buf_num = 1;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      for (i = 0; i < dec_cont->n_cores; i++) {
        asic_buff->segment_map[i].mem_type = DWL_MEM_TYPE_VPU_ONLY;
        dwl_ret = DWLMallocLinear(dec_cont->dwl, memory_size * 2, &asic_buff->segment_map[i]);
        if (dwl_ret) return DEC_MEMFAIL;
      }
      asic_buff->segment_map_size = memory_size;
    }
  }

  for (i = 0; i < dec_cont->n_cores; i++)
    DWLmemset(asic_buff->segment_map[i].virtual_address, 0,
              memory_size * 2);

  return HANTRO_OK;
}

i32 Vp9FreeSegmentMap(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 i;

  if (asic_buff->segment_map[0].virtual_address != NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, SEGMENT_MAP_BUFFER)) {
      dec_cont->buf_to_free = &asic_buff->segment_map[0];
      dec_cont->next_buf_size = 0;
      dec_cont->buf_type = SEGMENT_MAP_BUFFER;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      for(i = 0; i < MAX_ASIC_CORES; i++) {
        if (asic_buff->segment_map[i].virtual_address != NULL) {
          DWLFreeLinear(dec_cont->dwl, &asic_buff->segment_map[i]);
          asic_buff->segment_map[i].virtual_address = NULL;
          asic_buff->segment_map[i].size = 0;
        }
      }
    }
  }

  return HANTRO_OK;
}

void Vp9UpdateProbabilities(struct Vp9DecContainer *dec_cont) {
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
#if 0
  /* Read context counters from HW output memory. */
  DWLmemcpy(&dec_cont->decoder.ctx_ctr,
            dec_cont->asic_buff->misc_linear[core_id].virtual_address + dec_cont->asic_buff->ctx_counters_offset / 4,
            sizeof(struct Vp9EntropyCounts));
#endif

  /* Backward adaptation of probs based on context counters. */
  if (!dec_cont->decoder.error_resilient &&
      !dec_cont->decoder.frame_parallel_decoding) {
    /* Read context counters from HW output memory. */
    DWLmemcpy(&dec_cont->decoder.ctx_ctr,
            dec_cont->asic_buff->misc_linear[core_id].virtual_address +
            dec_cont->asic_buff->ctx_counters_offset / 4,
            sizeof(struct Vp9EntropyCounts));

    Vp9AdaptCoefProbs(&dec_cont->decoder);
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only) {
      Vp9AdaptModeProbs(&dec_cont->decoder);
      Vp9AdaptModeContext(&dec_cont->decoder);
      Vp9AdaptNmvProbs(&dec_cont->decoder);
    }
  }

  /* Store the adapted probs as base for following frames. */
  Vp9StoreProbs(&dec_cont->decoder);
}


#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
void Vp9SetEmptyPictureData(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index = asic_buff->out_buffer_i;

  DWLPrivateAreaMemset(asic_buff->pictures[index].virtual_address, SET_EMPTY_PICTURE_DATA,
                       asic_buff->pictures[index].size);
  DWLPrivateAreaMemset(asic_buff->pictures_c[index].virtual_address,
                       SET_EMPTY_PICTURE_DATA, asic_buff->pictures_c[index].size);
  DWLmemset(asic_buff->dir_mvs[index].virtual_address, SET_EMPTY_PICTURE_DATA,
            asic_buff->dir_mvs[index].size);

  if (!dec_cont->compress_bypass) {
    DWLmemset(asic_buff->cbs_luma_table[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->cbs_luma_table[index].size);
    DWLmemset(asic_buff->cbs_chroma_table[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->cbs_chroma_table[index].size);
  }

  if (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
    DWLPrivateAreaMemset(asic_buff->raster_luma[index].virtual_address,
                         SET_EMPTY_PICTURE_DATA, asic_buff->raster_luma[index].size);
    DWLPrivateAreaMemset(asic_buff->raster_chroma[index].virtual_address,
                         SET_EMPTY_PICTURE_DATA, asic_buff->raster_chroma[index].size);
  }
}
#endif

void Vp9AsicSetTileInfo(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 reg_num_tile_cols, reg_num_tile_rows;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  struct DecHwFeatures hw_feature;
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);


  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_TILE_BASE,
               asic_buff->misc_linear[core_id].bus_address + asic_buff->tile_info_offset);
  SetDecRegister(dec_cont->vp9_regs, HWIF_TILE_ENABLE,
                 dec->log2_tile_columns || dec->log2_tile_rows);

  if (dec->log2_tile_columns || dec->log2_tile_rows) {
    u32 tile_rows = (1 << dec->log2_tile_rows);
    u32 tile_cols = (1 << dec->log2_tile_columns);
    u32 i, j, h, tmp, prev_h, prev_w;
    u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
    u32 w_sbs = (asic_buff->width + 63) / 64;
    u32 h_sbs = (asic_buff->height + 63) / 64;

    /* write width + height for each tile in pic */
    if (tile_rows - h_sbs == 1) {
      for (i = 1, prev_h = 0; i < tile_rows; i++) {
        tmp = (i + 1) * h_sbs / tile_rows;
        h = tmp - prev_h;
        prev_h = tmp;

        if (h_sbs >= 3 && !i && !h)
          dec_cont->first_tile_empty = 1;

        for (j = 0, prev_w = 0; j < tile_cols; j++) {
          tmp = (j + 1) * w_sbs / tile_cols;
          *p++ = tmp - prev_w;
          *p++ = h;
          prev_w = tmp;
        }
      }
    } else if (tile_rows - h_sbs == 2) {
      /* When pic height (rounded up) is less than 3 SB rows, more than one
       * tile row may be skipped */
      for (i = 2, prev_h = 0; i < tile_rows; i++) {
        tmp = (i + 1) * h_sbs / tile_rows;
        h = tmp - prev_h;
        prev_h = tmp;

        if (h_sbs >= 3 && !i && !h)
          dec_cont->first_tile_empty = 1;

        for (j = 0, prev_w = 0; j < tile_cols; j++) {
          tmp = (j + 1) * w_sbs / tile_cols;
          *p++ = tmp - prev_w;
          *p++ = h;
          prev_w = tmp;
        }
      }
    } else {
      for (i = 0, prev_h = 0; i < tile_rows; i++) {
        tmp = (i + 1) * h_sbs / tile_rows;
        h = tmp - prev_h;
        prev_h = tmp;

        if (h_sbs >= 3 && !i && !h)
          dec_cont->first_tile_empty = 1;

        for (j = 0, prev_w = 0; j < tile_cols; j++) {
          tmp = (j + 1) * w_sbs / tile_cols;
          *p++ = tmp - prev_w;
          *p++ = h;
          prev_w = tmp;
        }
      }
    }

    if(dec_cont->legacy_regs) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS_V0, tile_cols);
      if (h_sbs >= 3)
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS_V0,
                       MIN(tile_rows, h_sbs));
      else
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS_V0, tile_rows);
    } else {
      if (hw_feature.pic_size_reg_unified) {
        reg_num_tile_cols = HWIF_NUM_TILE_COLS_8K;
        reg_num_tile_rows = HWIF_NUM_TILE_ROWS_8K;
      } else {
        reg_num_tile_cols = HWIF_NUM_TILE_COLS;
        reg_num_tile_rows = HWIF_NUM_TILE_ROWS;
      }
      SetDecRegister(dec_cont->vp9_regs, reg_num_tile_cols, tile_cols);
      if (h_sbs >= 3)
        SetDecRegister(dec_cont->vp9_regs, reg_num_tile_rows,
                       MIN(tile_rows, h_sbs));
      else
        SetDecRegister(dec_cont->vp9_regs, reg_num_tile_rows, tile_rows);
    }
  } else {
    /* just one "tile", dimensions equal to pic size in SBs */
    u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
    p[0] = (asic_buff->width + 63) / 64;
    p[1] = (asic_buff->height + 63) / 64;
    if (dec_cont->legacy_regs) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS_V0, 1);
      SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS_V0, 1);
    } else {
      if (hw_feature.pic_size_reg_unified) {
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS_8K, 1);
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS_8K, 1);
      } else {
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS, 1);
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS, 1);
      }
    }
  }
}

void Vp9AsicSetReferenceFrames(struct Vp9DecContainer *dec_cont) {
  u32 tmp1, tmp2, tmp3, i;
  u32 cur_height, cur_width;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index_ref[VP9_ACTIVE_REFS];
  u32 index_info[VP9_ACTIVE_REFS] = {0};
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  for (i = 0; i < VP9_ACTIVE_REFS; i++) {
    index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++) {
      index_info[i] = index_ref[i];
    }
  } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
             IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++)
      index_info[i] = Vp9BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
  }

  /* Add reference count of prev_out_buffer and active ref buffer and
     decrease reference count in pic ready callback */
  if (dec_cont->b_mc) {
    Vp9BufferQueueAddRef(dec_cont->bq, dec_cont->asic_buff->prev_out_buffer_i);
    for (i = 0; i < VP9_ACTIVE_REFS; i++) {
      Vp9BufferQueueAddRef(dec_cont->bq, index_ref[i]);
    }
  }

  /* unrounded picture dimensions */
  cur_width = dec_cont->decoder.width;
  cur_height = dec_cont->decoder.height;

  /* last reference */
  tmp1 = asic_buff->picture_info[index_info[0]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[0]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[0]].ref_pic_stride;
  SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_HEIGHT, tmp2);
  if (hw_feature.dec_stride_support) {
    if (!dec_cont->use_video_compressor) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_Y_STRIDE, tmp3);
      SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_C_STRIDE, tmp3);
    } else {
      u32 ystride, cstride;
      if (hw_feature.rfc_stride_support) {
        ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
        cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      } else {
        ystride = cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      }
      SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_Y_STRIDE, ystride);
      SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_C_STRIDE, cstride);
    }
  }

  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_VER_SCALE, tmp2);

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER0_YBASE,
               asic_buff->pictures[index_ref[0]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER0_CBASE,
               asic_buff->pictures[index_ref[0]].bus_address + asic_buff->pictures_c_offset[index_ref[0]]);

  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[0]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[0]] == 0);
  } else {
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER0_TYBASE,
                 asic_buff->pictures[index_ref[0]].bus_address + asic_buff->cbs_y_tbl_offset[index_ref[0]]);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER0_TCBASE,
                 asic_buff->pictures[index_ref[0]].bus_address + asic_buff->cbs_c_tbl_offset[index_ref[0]]);
  }

  /* Colocated MVs are always from previous decoded frame */
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER0_DBASE,
               asic_buff->pictures[asic_buff->prev_out_buffer_i].bus_address +
                 asic_buff->dir_mvs_offset[asic_buff->prev_out_buffer_i]);
  /* refer0_dbase is used for reference status base address calc for last reference. */
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER1_DBASE,
               asic_buff->pictures[index_ref[0]].bus_address +
                 asic_buff->dir_mvs_offset[index_ref[0]]);
  SetDecRegister(dec_cont->vp9_regs, HWIF_LAST_SIGN_BIAS,
                 dec->ref_frame_sign_bias[LAST_FRAME]);

  /* golden reference */
  tmp1 = asic_buff->picture_info[index_info[1]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[1]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[1]].ref_pic_stride;
  SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_HEIGHT, tmp2);
  if (hw_feature.dec_stride_support) {
    if (!dec_cont->use_video_compressor) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_Y_STRIDE, tmp3);
      SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_C_STRIDE, tmp3);
    } else {
      u32 ystride, cstride;
      if (hw_feature.rfc_stride_support) {
        ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
        cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      } else {
        ystride = cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      }
      SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_Y_STRIDE, ystride);
      SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_C_STRIDE, cstride);
    }
  }

  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_VER_SCALE, tmp2);

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER4_YBASE,
               asic_buff->pictures[index_ref[1]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER4_CBASE,
               asic_buff->pictures[index_ref[1]].bus_address + asic_buff->pictures_c_offset[index_ref[1]]);

  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[1]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[1]] == 0);
  } else {
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER4_TYBASE,
                 asic_buff->pictures[index_ref[1]].bus_address + asic_buff->cbs_y_tbl_offset[index_ref[1]]);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER4_TCBASE,
                 asic_buff->pictures[index_ref[1]].bus_address + asic_buff->cbs_c_tbl_offset[index_ref[1]]);
  }

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER4_DBASE,
               asic_buff->pictures[index_ref[1]].bus_address +
                 asic_buff->dir_mvs_offset[index_ref[1]]);
  SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[GOLDEN_FRAME]);

  /* alternate reference */
  tmp1 = asic_buff->picture_info[index_info[2]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[2]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[2]].ref_pic_stride;
  SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_HEIGHT, tmp2);
  if (hw_feature.dec_stride_support) {
    if (!dec_cont->use_video_compressor) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_Y_STRIDE, tmp3);
      SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_C_STRIDE, tmp3);
    } else {
      u32 ystride, cstride;
      if (hw_feature.rfc_stride_support) {
        ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
        cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      } else {
        ystride = cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      }
      SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_Y_STRIDE, ystride);
      SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_C_STRIDE, cstride);
    }
  }

  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_VER_SCALE, tmp2);

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER5_YBASE,
               asic_buff->pictures[index_ref[2]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER5_CBASE,
               asic_buff->pictures[index_ref[2]].bus_address + asic_buff->pictures_c_offset[index_ref[2]]);

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER5_DBASE,
               asic_buff->pictures[index_ref[2]].bus_address +
                 asic_buff->dir_mvs_offset[index_ref[2]]);
  SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[ALTREF_FRAME]);

  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[2]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[2]] == 0);
  } else {
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER5_TYBASE,
                 asic_buff->pictures[index_ref[2]].bus_address + asic_buff->cbs_y_tbl_offset[index_ref[2]]);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_REFER5_TCBASE,
                 asic_buff->pictures[index_ref[2]].bus_address + asic_buff->cbs_c_tbl_offset[index_ref[2]]);
  }
}

void Vp9AsicSetSegmentation(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  //struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 *vp9_regs = dec_cont->vp9_regs;
  u32 s;
  u32 segval[MAX_MB_SEGMENTS][SEG_LVL_MAX];

  /* Resolution change will clear any previous segment map. */
  if (dec->resolution_change) {
    //DWLmemset(asic_buff->segment_map.virtual_address, 0,
    //          asic_buff->segment_map.logical_size);
  }
  /* Segmentation */
  SetDecRegister(vp9_regs, HWIF_SEGMENT_E, dec->segment_enabled);
  SetDecRegister(vp9_regs, HWIF_SEGMENT_UPD_E, dec->segment_map_update);
  SetDecRegister(vp9_regs, HWIF_SEGMENT_TEMP_UPD_E,
                 dec->segment_map_temporal_update);
  /* Set filter level and QP for every segment ID. Initialize all
   * segments with default QP and filter level. */
  for (s = 0; s < MAX_MB_SEGMENTS; s++) {
    segval[s][0] = dec->qp_yac;
    segval[s][1] = dec->loop_filter_level;
    segval[s][2] = 0; /* segment ref_frame disabled */
    segval[s][3] = 0; /* segment skip disabled */
  }
  /* If a feature is enabled for a segment, overwrite the default. */
  if (dec->segment_enabled) {
    i32(*segdata)[SEG_LVL_MAX] = dec->segment_feature_data;

    if (dec->segment_feature_mode == VP9_SEG_FEATURE_ABS) {
      for (s = 0; s < MAX_MB_SEGMENTS; s++) {
        if (dec->segment_feature_enable[s][0])
          segval[s][0] = CLIP3(0, 255, segdata[s][0]);
        if (dec->segment_feature_enable[s][1])
          segval[s][1] = CLIP3(0, 63, segdata[s][1]);
        if (!dec->key_frame && dec->segment_feature_enable[s][2])
          segval[s][2] = segdata[s][2] + 1;
        if (dec->segment_feature_enable[s][3]) segval[s][3] = 1;
      }
    } else { /* delta mode */
      for (s = 0; s < MAX_MB_SEGMENTS; s++) {
        if (dec->segment_feature_enable[s][0])
          segval[s][0] = CLIP3(0, 255, dec->qp_yac + segdata[s][0]);
        if (dec->segment_feature_enable[s][1])
          segval[s][1] = CLIP3(0, 63, dec->loop_filter_level + segdata[s][1]);
        if (!dec->key_frame && dec->segment_feature_enable[s][2])
          segval[s][2] = segdata[s][2] + 1;
        if (dec->segment_feature_enable[s][3]) segval[s][3] = 1;
      }
    }
  }
  /* Write QP, filter level, ref frame and skip for every segment */
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG0, segval[0][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG0, segval[0][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG0, segval[0][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG0, segval[0][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG1, segval[1][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG1, segval[1][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG1, segval[1][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG1, segval[1][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG2, segval[2][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG2, segval[2][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG2, segval[2][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG2, segval[2][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG3, segval[3][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG3, segval[3][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG3, segval[3][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG3, segval[3][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG4, segval[4][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG4, segval[4][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG4, segval[4][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG4, segval[4][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG5, segval[5][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG5, segval[5][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG5, segval[5][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG5, segval[5][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG6, segval[6][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG6, segval[6][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG6, segval[6][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG6, segval[6][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG7, segval[7][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG7, segval[7][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG7, segval[7][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG7, segval[7][3]);
}

void Vp9AsicSetLoopFilter(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 *vp9_regs = dec_cont->vp9_regs;
  //struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  /* loop filter */
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL, dec->loop_filter_level);
  SetDecRegister(vp9_regs, HWIF_FILTERING_DIS, dec->loop_filter_level == 0);
  SetDecRegister(vp9_regs, HWIF_FILT_SHARPNESS, dec->loop_filter_sharpness);

  if (dec->mode_ref_lf_enabled) {
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, dec->mb_ref_lf_delta[0]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, dec->mb_ref_lf_delta[1]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, dec->mb_ref_lf_delta[2]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, dec->mb_ref_lf_delta[3]);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, dec->mb_mode_lf_delta[0]);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, dec->mb_mode_lf_delta[1]);
  } else {
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, 0);
  }
  //SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_VERT_FILT_BASE,
  //             asic_buff->tile_edge.bus_address + asic_buff->filter_mem_offset);
  //SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_BSD_CTRL_BASE,
  //             asic_buff->tile_edge.bus_address + asic_buff->bsd_control_mem_offset);
}

void Vp9AsicSetPictureDimensions(struct Vp9DecContainer *dec_cont) {
  /* Write dimensions for the current picture
     (This is needed when scaling is used) */
  SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_WIDTH_IN_CBS,
                 (dec_cont->width + 7) / 8);
  SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_HEIGHT_IN_CBS,
                 (dec_cont->height + 7) / 8);
  SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_WIDTH_4X4,
                 NEXT_MULTIPLE(dec_cont->width, 8) >> 2);
  SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_HEIGHT_4X4,
                 NEXT_MULTIPLE(dec_cont->height, 8) >> 2);
}

void Vp9AsicSetOutput(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_DIS, 0);

  if (hw_feature.pp_version == G2_NATIVE_PP) {
    SetDecRegister(dec_cont->vp9_regs, HWIF_OUTPUT_8_BITS, dec_cont->use_8bits_output);
    SetDecRegister(dec_cont->vp9_regs,
                   HWIF_OUTPUT_FORMAT,
                   dec_cont->use_p010_output ? 1 :
                   (dec_cont->pixel_format == DEC_OUT_PIXEL_CUSTOMER1 ? 2 : 0));
  }

#ifdef CLEAR_OUT_BUFFER
  /*DWLPrivateAreaMemset(asic_buff->pictures[asic_buff->out_buffer_i].virtual_address,
                       0, asic_buff->dir_mvs_offset[asic_buff->out_buffer_i]);*/
  DWLmemset(asic_buff->pictures[asic_buff->out_buffer_i].virtual_address,
                       0, asic_buff->dir_mvs_offset[asic_buff->out_buffer_i]);
#endif

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_OUT_YBASE,
               asic_buff->pictures[asic_buff->out_buffer_i].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_OUT_CBASE,
               asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->pictures_c_offset[asic_buff->out_buffer_i]);

  if (hw_feature.dec_stride_support) {
    if (!dec_cont->use_video_compressor) {
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_Y_STRIDE, asic_buff->out_stride[asic_buff->out_buffer_i]);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_C_STRIDE, asic_buff->out_stride[asic_buff->out_buffer_i]);
    } else {
      u32 ystride, cstride;
      if (hw_feature.rfc_stride_support) {
        ystride = NEXT_MULTIPLE(8 * asic_buff->width * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
        cstride = NEXT_MULTIPLE(4 * asic_buff->width * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      } else {
        ystride = cstride = NEXT_MULTIPLE(4 * asic_buff->width * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      }
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_Y_STRIDE, ystride);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_C_STRIDE, cstride);
    }
  }
  if (dec_cont->use_video_compressor) {
    /* Always set ec word align if bitdepth > 8. */
    SetDecRegister(dec_cont->vp9_regs, HWIF_EC_WORD_ALIGN,
                   dec_cont->decoder.bit_depth > 8);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_OUT_TYBASE,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->cbs_y_tbl_offset[asic_buff->out_buffer_i]);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_OUT_TCBASE,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->cbs_c_tbl_offset[asic_buff->out_buffer_i]);
  }
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_OUT_DBASE,
               asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->dir_mvs_offset[asic_buff->out_buffer_i]);

      struct DWLLinearMem *pp_buffer = &asic_buff->pp_pictures[
                 asic_buff->pp_buffer_map[asic_buff->out_buffer_i]];

  /* Raster/PP output configuration. */

  if (dec_cont->pp_enabled || dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN) {
    if (!hw_feature.flexible_scale_support) {
      /* G2 Native PP: raster output + down scale (fixed-ratio) output */
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_RS_E,
                     dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_DS_E, dec_cont->pp_enabled);
      if (dec_cont->pp_enabled) {
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_DS_X,
                       dec_cont->down_scale_x_shift - 1);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_DS_Y,
                       dec_cont->down_scale_y_shift - 1);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_DSY_BASE,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_DSC_BASE,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address +
                     asic_buff->pp_c_offset[asic_buff->out_buffer_i][0]);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_RSY_BASE, (addr_t)0);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_RSC_BASE, (addr_t)0);
      } else {
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_RSY_BASE,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_RSC_BASE,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address +
                     asic_buff->pp_c_offset[asic_buff->out_buffer_i][0]);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_DSY_BASE, (addr_t)0);
        SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_DSC_BASE, (addr_t)0);
      }
    } else {
      /* Flexible PP */
      PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
      addr_t ppu_out_bus_addr = pp_buffer->bus_address;
#define TOFIX(d, q) ((u32)( (d) * (u32)(1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))

      SetDecRegister(dec_cont->vp9_regs, HWIF_PP_IN_FORMAT_U, 1);

#if 0
      /* flexible scale ratio */
      if (!dec_cont->crop_enabled) {
        dec_cont->crop_width  = dec_cont->asic_buff->width;
        dec_cont->crop_height = dec_cont->asic_buff->height;
      }
      if (dec_cont->scaled_type == FIXED_DOWNSCALE) {
        dec_cont->scaled_width = dec_cont->asic_buff->width >> dec_cont->down_scale_x_shift;
        dec_cont->scaled_height = dec_cont->asic_buff->height >> dec_cont->down_scale_y_shift;
      }
#endif
      PPSetRegs(dec_cont->vp9_regs, &hw_feature, ppu_cfg, ppu_out_bus_addr, 0, 0);
      DelogoSetRegs(dec_cont->vp9_regs, &hw_feature, dec_cont->delogo_params);
#if 0
      for (i = 0; i < 4; i++, ppu_cfg++) {

        /* flexible scale ratio */
        u32 in_width = ppu_cfg->crop.width;
        u32 in_height = ppu_cfg->crop.height;
        u32 out_width = ppu_cfg->scale.width;
        u32 out_height = ppu_cfg->scale.height;

        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_E_U, ppu_cfg->enabled);
        if (!ppu_cfg->enabled) continue;

        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_CR_FIRST, ppu_cfg->cr_first);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].OUT_FORMAT_U, ppu_cfg->out_format);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_TILE_E_U, ppu_cfg->tiled_e);

        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].CROP_STARTX_U,
                       ppu_cfg->crop.x >> hw_feature.crop_step_rshift);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].CROP_STARTY_U,
                       ppu_cfg->crop.y >> hw_feature.crop_step_rshift);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_IN_WIDTH_U,
                       ppu_cfg->crop.width >> hw_feature.crop_step_rshift);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_IN_HEIGHT_U,
                       ppu_cfg->crop.height >> hw_feature.crop_step_rshift);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_WIDTH_U,
                       ppu_cfg->scale.width);
        SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_HEIGHT_U,
                       ppu_cfg->scale.height);

        if(in_width < out_width) {
          /* upscale */
          u32 W, inv_w;

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

          W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
          inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].SCALE_WRATIO_U, W);
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].WSCALE_INVRA_U, inv_w);
        } else if(in_width > out_width) {
          /* downscale */
          u32 hnorm;

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

          hnorm = FDIVI(TOFIX(out_width, 16), in_width) + 1;
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].WSCALE_INVRA_U, hnorm);
        } else {
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].WSCALE_INVRA_U, 0);
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HOR_SCALE_MODE_U, 0);
        }

        if(in_height < out_height) {
          /* upscale */
          u32 H, inv_h;

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

          H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].SCALE_HRATIO_U, H);

          inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HSCALE_INVRA_U, inv_h);
        } else if(in_height > out_height) {
          /* downscale */
          u32 Cv;

          Cv = FDIVI(TOFIX(out_height, 16), in_height) + 1;

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].VER_SCALE_MODE_U, 2);

          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HSCALE_INVRA_U, Cv);
        } else {
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].HSCALE_INVRA_U, 0);
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].VER_SCALE_MODE_U, 0);
        }

        SET_ADDR_REG(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_LU_BASE_U,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address +
                     asic_buff->pp_y_offset[asic_buff->out_buffer_i][i]);
        SET_ADDR_REG(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_CH_BASE_U,
                     asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]].bus_address +
                     asic_buff->pp_c_offset[asic_buff->out_buffer_i][i]);
        if (hw_feature.pp_stride_support) {
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_Y_STRIDE,
                         dec_cont->pp_enabled ? asic_buff->ds_stride[asic_buff->out_buffer_i][i] :
                                                asic_buff->rs_stride[asic_buff->out_buffer_i]);
          SetDecRegister(dec_cont->vp9_regs, ppu_regs[i].PP_OUT_C_STRIDE,
                         dec_cont->pp_enabled ? asic_buff->ds_stride_ch[asic_buff->out_buffer_i][i] :
                                                asic_buff->rs_stride[asic_buff->out_buffer_i]);
        }
      }
#endif
    }
  }
}

void Vp9AsicInitPicture(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 *vp9_regs = dec_cont->vp9_regs;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
  Vp9SetEmptyPictureData(dec_cont);
#endif

  /* make sure that output pic sync memory is cleared */
  {
    char *sync_base =
      (char *) (asic_buff->pictures[asic_buff->out_buffer_i].virtual_address) +
      asic_buff->sync_mc_offset[asic_buff->out_buffer_i];
    u32 sync_size = 32; /* 32 bytes for each field */
    (void) DWLmemset(sync_base, 0, sync_size);
  }

  Vp9AsicSetOutput(dec_cont);

  if (!dec->key_frame && !dec->intra_only) {
    Vp9AsicSetReferenceFrames(dec_cont);
  }

  //Vp9AsicSetTileInfo(dec_cont);

  Vp9AsicSetSegmentation(dec_cont);

  Vp9AsicSetLoopFilter(dec_cont);

  Vp9AsicSetPictureDimensions(dec_cont);

  SetDecRegister(vp9_regs, HWIF_BIT_DEPTH_Y_MINUS8, dec->bit_depth - 8);
  SetDecRegister(vp9_regs, HWIF_BIT_DEPTH_C_MINUS8, dec->bit_depth - 8);

  /* QP deltas applied after choosing base QP based on segment ID. */
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_Y_DC, dec->qp_ydc);
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_DC, dec->qp_ch_dc);
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_AC, dec->qp_ch_ac);
  SetDecRegister(vp9_regs, HWIF_LOSSLESS_E, dec->lossless);

  /* Mark intra_only frame also a keyframe but copy inter probabilities to
     partition probs for the stream decoding. */
  SetDecRegister(vp9_regs, HWIF_IDR_PIC_E, (dec->key_frame || dec->intra_only));

  SetDecRegister(vp9_regs, HWIF_TRANSFORM_MODE, dec->transform_mode);
  SetDecRegister(vp9_regs, HWIF_MCOMP_FILT_TYPE, dec->mcomp_filter_type);
  SetDecRegister(vp9_regs, HWIF_HIGH_PREC_MV_E,
                 !dec->key_frame && dec->allow_high_precision_mv);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_MODE, dec->comp_pred_mode);
  SetDecRegister(vp9_regs, HWIF_TEMPOR_MVP_E,
                 !dec->error_resilient && !dec->key_frame &&
                 !dec->prev_is_key_frame && !dec->intra_only &&
                 !dec->resolution_change && dec->prev_show_frame);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_FIXED_REF, dec->comp_fixed_ref);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF0, dec->comp_var_ref[0]);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF1, dec->comp_var_ref[1]);

  if (!dec_cont->conceal) {
    if (dec->key_frame)
      SetDecRegister(dec_cont->vp9_regs, HWIF_WRITE_MVS_E, 0);
    else
      SetDecRegister(dec_cont->vp9_regs, HWIF_WRITE_MVS_E, 1);
  }
  if ((!(dec->log2_tile_columns || dec->log2_tile_rows)) || (dec->log2_tile_columns < 1))
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_TILE_INT_E, 0);
  else
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_TILE_INT_E, dec_cont->tile_by_tile);
}


void Vp9AsicStrmPosUpdate(struct Vp9DecContainer *dec_cont,
                          addr_t strm_bus_address, u32 data_len,
                          addr_t buf_bus_address, u32 buf_len) {
  u32 tmp, hw_bit_pos;
  addr_t tmp_addr;
  u32 is_rb = dec_cont->use_ringbuffer;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);


  DEBUG_PRINT(("Vp9AsicStrmPosUpdate:\n"));

  /* Bit position where SW has decoded frame headers.
  tmp = (dec_cont->bc.pos) * 8 + (8 - dec_cont->bc.count);*/

  /* Residual partition after frame header partition. */
  tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

  if(is_rb) {
    u32 turn_around = 0;
    tmp_addr = strm_bus_address + tmp;
    if(tmp_addr >= (buf_bus_address + buf_len)) {
      tmp_addr -= buf_len;
      turn_around = 1;
    }

    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~DEC_HW_ALIGN_MASK); /* align the base */

    SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_BIT, hw_bit_pos);

    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_STREAM_BASE, buf_bus_address);

    /* Total stream length passed to HW. */
    if (turn_around)
      tmp = data_len + strm_bus_address - (tmp_addr + buf_len);
    else
      tmp = data_len + strm_bus_address - tmp_addr;
    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_LEN, tmp);

    /* stream data start offset */
    tmp_addr = tmp_addr - buf_bus_address;
    SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_OFFSET, tmp_addr);

    /* stream buffer size */
    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_BUFFER_LEN, buf_len);
  } else {
    tmp_addr = strm_bus_address + tmp;

    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~DEC_HW_ALIGN_MASK); /* align the base */

    SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_BIT, hw_bit_pos);

    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_STREAM_BASE, tmp_addr);

    /* Total stream length passed to HW. */
    tmp = data_len - (tmp_addr - strm_bus_address);

    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_LEN, tmp);

    /* stream data start offset */
    tmp = (u32)(tmp_addr - buf_bus_address);
    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_OFFSET, 0);

    /* stream buffer size */
    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_BUFFER_LEN, buf_len - tmp);
  }

  DEBUG_PRINT(("STREAM BUS ADDR: 0x%08x\n", strm_bus_address));
  DEBUG_PRINT(("HW STREAM BASE: 0x%08x\n", tmp_addr));
  DEBUG_PRINT(("HW START BIT: %d\n", hw_bit_pos));
  DEBUG_PRINT(("HW STREAM LEN: %d\n", tmp));
}

u32 Vp9AsicRun(struct Vp9DecContainer *dec_cont, u32 pic_id) {
  i32 ret = 0;
  u32 core_id;
  u32 tile_enable = 0;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (!dec_cont->asic_running) {
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      ret = DWLReserveCmdBuf(dec_cont->dwl, DWL_CLIENT_TYPE_VP9_DEC,
                             dec_cont->width, dec_cont->height,
                             &dec_cont->cmdbuf_id);
    } else
      ret = DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_VP9_DEC);
    if (ret != DWL_OK) {
      return VP9HWDEC_HW_RESERVED;
    }

    dec_cont->asic_running = 1;
    core_id = dec_cont->b_mc ? dec_cont->core_id : 0;

    if (dec_cont->b_mc) {
      //while (asic_buff->asic_busy[dec_cont->asic_buff->out_buffer_i])
      //  sched_yield();
      asic_buff->asic_busy[dec_cont->asic_buff->out_buffer_i] = 1;
    }

    /* core_id relative asic process */
    ret = Vp9AsicAllocateFilterBlockMem(dec_cont);
    if (ret) return DEC_MEMFAIL;

    Vp9AsicProbUpdate(dec_cont);
    Vp9AsicSetTileInfo(dec_cont);

    /* TODO(chen): Bad performance, need to be refined */
    if (dec_cont->b_mc && dec_cont->force_to_sc == 0) {
      DWLmemcpy(dec_cont->asic_buff->segment_map[core_id].virtual_address,
                dec_cont->asic_buff->segment_map[dec_cont->pre_core_id].virtual_address,
                dec_cont->asic_buff->segment_map[core_id].size);
    }

    dec_cont->pre_core_id = core_id;
    /* Resolution change will clear any previous segment map. */
    if (dec->resolution_change || dec->key_frame ||
      dec->error_resilient || dec->intra_only) {
      DWLmemset(asic_buff->segment_map[core_id].virtual_address, 0,
                asic_buff->segment_map[core_id].logical_size);
    }

    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_VERT_FILT_BASE,
                 asic_buff->tile_edge[core_id].bus_address + asic_buff->filter_mem_offset[core_id]);
    SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_BSD_CTRL_BASE,
                 asic_buff->tile_edge[core_id].bus_address + asic_buff->bsd_control_mem_offset[core_id]);

    u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
    if ((!(dec_cont->decoder.log2_tile_columns || dec_cont->decoder.log2_tile_rows)) || (dec_cont->decoder.log2_tile_columns < 1))
      tile_enable = 0;
    else
      tile_enable = dec_cont->tile_by_tile;
    DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, p, tile_enable);

    Vp9SetupPicToOutput(dec_cont, pic_id);

    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 1);
    FlushDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs);
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[1]);
  } else {
    Vp9SetupPicToOutput(dec_cont, pic_id);

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 13,
                dec_cont->vp9_regs[13]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 14,
                dec_cont->vp9_regs[14]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 15,
                dec_cont->vp9_regs[15]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[1]);
  }

  //Vp9SetupPicToOutput(dec_cont, pic_id);
  return ret;
}

u32 Vp9AsicSync(struct Vp9DecContainer *dec_cont) {
  u32 asic_status = 0;
  u32 irq = 0;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 tile_rows = (1 << dec->log2_tile_rows);
  i32 ret;
  if (dec_cont->first_tile_empty)
    tile_rows -= 1;

  if (dec_cont->vcmd_used)
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                         (u32)DEC_X170_TIMEOUT_LENGTH);

  if (ret != DWL_HW_WAIT_OK) {
    ERROR_PRINT("DWLWaitHwReady");
    DEBUG_PRINT(("DWLWaitHwReady returned: %d\n", ret));

    /* Reset HW */
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 0);

    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->vp9_regs[1]);

    dec_cont->asic_running = 0;
    if (dec_cont->vcmd_used)
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    if (irq == DWL_CACHE_FATAL_RECOVERY)
      return VP9HWDEC_SYSTEM_ERROR;
    if (irq == DWL_CACHE_FATAL_UNRECOVERY)
      return VP9HWDEC_FATAL_SYSTEM_ERROR;
    return (ret == DWL_HW_WAIT_ERROR) ? VP9HWDEC_SYSTEM_ERROR
           : VP9HWDEC_SYSTEM_TIMEOUT;
  }

  RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs);
  asic_status = GetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT);
  while (asic_status & DEC_HW_IRQ_TILE) {
    dec_cont->tile_id++;
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0); /* just in case */
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->vp9_regs[1]);

    if (dec_cont->vcmd_used)
      ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                       (u32)DEC_X170_TIMEOUT_LENGTH);
    if (ret != DWL_HW_WAIT_OK) {
      ERROR_PRINT("DWLWaitHwReady");
      DEBUG_PRINT(("DWLWaitHwReady returned: %d\n", ret));

      /* Reset HW */
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0);

      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->vp9_regs[1]);

      dec_cont->asic_running = 0;
      if (dec_cont->vcmd_used)
        irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      else
        irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
      if (irq == DWL_CACHE_FATAL_RECOVERY)
        return VP9HWDEC_SYSTEM_ERROR;
      if (irq == DWL_CACHE_FATAL_UNRECOVERY)
        return VP9HWDEC_FATAL_SYSTEM_ERROR;
      return (ret == DWL_HW_WAIT_ERROR) ? VP9HWDEC_SYSTEM_ERROR
           : VP9HWDEC_SYSTEM_TIMEOUT;
    }
    RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs);
    asic_status = GetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT);
  }
  dec_cont->tile_id = 0;
  dec_cont->first_tile_empty = 0;

  /* React to the HW return value */

  asic_status = GetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT);

  SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0); /* just in case */

  /* HW done, release it! */
  DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[1]);
  dec_cont->asic_running = 0;

  if (dec_cont->vcmd_used)
    irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
  if (irq == DWL_CACHE_FATAL_RECOVERY)
    return VP9HWDEC_SYSTEM_ERROR;
  if (irq == DWL_CACHE_FATAL_UNRECOVERY)
    return VP9HWDEC_FATAL_SYSTEM_ERROR;
  return asic_status;
}

void Vp9AsicProbUpdate(struct Vp9DecContainer *dec_cont) {
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  struct DWLLinearMem *segment_map = &dec_cont->asic_buff->segment_map[core_id];
  u8 *asic_prob_base = (u8 *)dec_cont->asic_buff->misc_linear[core_id].virtual_address + dec_cont->asic_buff->prob_tbl_offset;

  /* Write probability tables to HW memory */
  DWLmemcpy(asic_prob_base, &dec_cont->decoder.entropy,
            sizeof(struct Vp9EntropyProbs));

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_PROB_TAB_BASE,
               dec_cont->asic_buff->misc_linear[core_id].bus_address + dec_cont->asic_buff->prob_tbl_offset);

  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_CTX_COUNTER_BASE,
               dec_cont->asic_buff->misc_linear[core_id].bus_address + dec_cont->asic_buff->ctx_counters_offset);

#if 0
  SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_READ_BASE_LSB,
                 segment_map[dec_cont->active_segment_map].bus_address);
  SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_WRITE_BASE_LSB,
                 segment_map[1 - dec_cont->active_segment_map].bus_address);
#else
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_SEGMENT_READ_BASE,
               segment_map->bus_address + dec_cont->active_segment_map * dec_cont->asic_buff->segment_map_size);
  SET_ADDR_REG(dec_cont->vp9_regs, HWIF_SEGMENT_WRITE_BASE,
               segment_map->bus_address + (1 - dec_cont->active_segment_map) * dec_cont->asic_buff->segment_map_size);
#endif

  /* Update active segment map for next frame */
  if (dec_cont->decoder.segment_map_update)
    dec_cont->active_segment_map = 1 - dec_cont->active_segment_map;
}

void Vp9UpdateRefs(struct Vp9DecContainer *dec_cont, u32 corrupted) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

#ifndef USE_VP9_EC
  if (!corrupted || (corrupted && dec_cont->pic_number != 1))
#endif
  {
    if (dec_cont->decoder.reset_frame_flags) {
      Vp9BufferQueueUpdateRef(dec_cont->bq, (1 << NUM_REF_FRAMES) - 1,
                              REFERENCE_NOT_SET);
      Vp9BufferQueueUpdateRef(dec_cont->pp_bq, (1 << NUM_REF_FRAMES) - 1,
                              REFERENCE_NOT_SET);
      dec_cont->decoder.reset_frame_flags = 0;
    }
    Vp9BufferQueueUpdateRef(dec_cont->bq, dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_buffer_i);
    Vp9BufferQueueUpdateRef(dec_cont->pp_bq, dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_pp_buffer_i);
  }
  if (!dec_cont->decoder.show_frame ||
#ifdef USE_VP9_EC
      corrupted)
#else
      (dec_cont->pic_number != 1 && corrupted))
#endif
  {
    /* If the picture will not be outputted, we need to remove ref used to
        protect the output. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, asic_buff->out_buffer_i);
    /* For raster/dscale output buffer, return it to input buffer queue. */
    Vp9BufferQueueRemoveRef(dec_cont->pp_bq, asic_buff->out_pp_buffer_i);
  }
}

void Vp9AsicReset(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  asic_buff->realloc_out_buffer = 0;
  asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->prev_out_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->out_pp_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->realloc_seg_map_buffer = 0;
  asic_buff->realloc_tile_edge_mem = 0;
#ifdef USE_OMXIL_BUFFER
  //DWLmemset(asic_buff->pictures, 0, VP9DEC_MAX_PIC_BUFFERS * sizeof(struct DWLLinearMem));
#endif
  DWLmemset(asic_buff->first_show, 0, VP9DEC_MAX_PIC_BUFFERS * sizeof(i32));
  DWLmemset(asic_buff->picture_info, 0, VP9DEC_MAX_PIC_BUFFERS * sizeof(struct Vp9DecPicture));
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, RASTERSCAN_OUT_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    asic_buff->out_pp_buffer_i = VP9_UNDEFINED_BUFFER;
    DWLmemset(asic_buff->pp_buffer_map, 0, VP9DEC_MAX_PIC_BUFFERS * sizeof(i32));
#ifdef USE_OMXIL_BUFFER
    DWLmemset(asic_buff->pp_pictures, 0, VP9DEC_MAX_PIC_BUFFERS * sizeof(struct DWLLinearMem));
#endif
  }
}

/* Fix chroma RFC table when the width/height is (48, 64] */
void Vp9FixChromaRFCTable(struct Vp9DecContainer *dec_cont) {
  u32 frame_width = NEXT_MULTIPLE(dec_cont->width, 8);
  u32 frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 i, j, cbs_size = 0;
  u8 *pch_rfc_tbl, *ptbl = NULL;
  u8 cbs_sizes_8bit[14] = {129, 2, 4, 8, 16, 32, 64, 129, 2, 4, 8, 16, 32, 64};
  u8 cbs_sizes_10bit[14] = {0xa1, 0x42, 0x85, 10, 20, 40, 80, 0xa1, 0x42, 0x85, 10, 20, 40, 80};
  u32 pic_width_in_cbsc  = NEXT_MULTIPLE(frame_width, 256)/16;
  u32 pic_height_in_cbsc = NEXT_MULTIPLE(frame_height/2, 4)/4;
  u32 offset;
  u32 bit_depth = dec_cont->decoder.bit_depth;

  if (!dec_cont->use_video_compressor ||
      ((frame_width <= 48 || frame_width > 64) &&
       (frame_height <= 48 || frame_height > 64)))
    return;

  pch_rfc_tbl = (u8 *)asic_buff->pictures[asic_buff->out_buffer_i].virtual_address +
                asic_buff->cbs_c_tbl_offset[asic_buff->out_buffer_i];

  /* Fill missing 1 CBS in the right edge. */
  if (frame_width > 48 && frame_width <= 64) {
    cbs_size = (frame_width - 48) * 4;
    for (i = 0; i < frame_height / 8; i++) {
#if 1
      pch_rfc_tbl[4] = (pch_rfc_tbl[4] & 0x1f) | ((cbs_size & 0x07) << 5);
      pch_rfc_tbl[5] = cbs_size >> 3;
#else
      pch_rfc_tbl[11] = (pch_rfc_tbl[11] & 0x1f) | ((cbs_size & 0x07) << 5);
      pch_rfc_tbl[10] = cbs_size >> 3;
#endif
      pch_rfc_tbl += 16;
    }
  } else {
    pch_rfc_tbl += pic_width_in_cbsc * 6;
  }

  if (bit_depth == 8) {
    cbs_size = 64;
    ptbl = cbs_sizes_8bit;
  } else if (bit_depth == 10) {
    cbs_size = 80;
    ptbl = cbs_sizes_10bit;
  }

  // Compression table for C
  if (frame_height > 48 && frame_height <= 64) {
    for (i = 0; i < pic_height_in_cbsc - 6; i++) {
      offset = 0;
      for (j = 0; j < pic_width_in_cbsc/16; j++) {
#if 0
        *(u16 *)pcbs = offset;
        memcpy(pcbs+2, ptbl, 14);
#else
        memcpy(pch_rfc_tbl, ptbl, 14);
        *(pch_rfc_tbl+14) = offset >> 8;
        *(pch_rfc_tbl+15) = offset & 0xff;
#endif
        pch_rfc_tbl += 16;
        offset += 16 * cbs_size;
      }
    }
  }
}

void Vp9GetRefFrmSize(struct Vp9DecContainer *dec_cont,
                       u32 *luma_size, u32 *chroma_size,
                       u32 *rfc_luma_size, u32 *rfc_chroma_size) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 tbl_sizey, tbl_sizec;

  u32 bit_depth;
  u32 out_w, out_h;
  u32 ref_size;

  bit_depth = dec_cont->decoder.bit_depth;

  out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  out_h = asic_buff->height / 4;
  ref_size = out_w * out_h;
  if (luma_size) *luma_size = ref_size;
  if (chroma_size) *chroma_size = ref_size / 2;

  if (dec_cont->use_video_compressor) {
    pic_width_in_cbsy = (asic_buff->width + 8 - 1)/8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1)/16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1)/8;
    pic_height_in_cbsc = (asic_buff->height/2 + 4 - 1)/4;

    /* luma table size */
    tbl_sizey = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, 16);
    /* chroma table size */
    tbl_sizec = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, 16);
  } else {
    tbl_sizey = tbl_sizec = 0;
  }

  if (rfc_luma_size) *rfc_luma_size = tbl_sizey;
  if (rfc_chroma_size) *rfc_chroma_size = tbl_sizec;
}
