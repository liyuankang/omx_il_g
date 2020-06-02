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
#include "dwl.h"
#include "vpufeature.h"
#include "ppu.h"
#include "deccfg.h"
#include "regdrv.h"
#include "sw_util.h"
#include "commonconfig.h"
#include "cacheapi.h"
#include "dwl_linux_cache.h"
#include "assert.h"

#ifdef ASIC_TRACE_SUPPORT
#include "tb_defs.h"
extern u8 *dpb_base_address;
extern FILE *cache_fid;
extern u32 hw_dec_pic_count;
extern u32 stream_buffer_id;
extern struct TBCfg tb_cfg;
extern u32 shaper_flag[10];
#endif

#define  H264  0
#define  MPEG4 1
#define  H263  2
#define  JPEG  3
#define  VC1   4
#define  MPEG2 5
#define  MPEG1 6
#define  VP6   7
#define  HUKKA 8
#define  VP7   9
#define  VP8   10
#define  AVS   11
#define  HEVC  12
#define  VP9   13
#define  PP    14
#define  H264_HIGH10 15
#define  AVS2  16

#define JPEGDEC_YUV400 0
#define JPEGDEC_YUV420 2
#define JPEGDEC_YUV422 3
#define JPEGDEC_YUV444 4
#define JPEGDEC_YUV440 5
#define JPEGDEC_YUV411 6
/* tile border coefficients of filter */
#define ASIC_VERT_FILTER_RAM_SIZE 8 /* bytes per pixel row */
/* BSD control data of current picture at tile border
 * 128 bits per 4x4 tile = 128/(8*4) bytes per row */
#define ASIC_BSD_CTRL_RAM_SIZE 4 /* bytes per pixel row */
#define ASIC_VERT_SAO_RAM_SIZE 48 /* bytes per pixel */
#define MAX3(A,B,C) ((A)>(B)&&(A)>(C)?(A):((B)>(C)?(B):(C)))

void *cache[2] = {NULL, NULL};
u32 channel_idx = 0;
u32 cache_version = 0;
u32 vcmd_used = 0;
struct buffer_index_cache {
  u8 *pp_base;
  u32 offset;
  u32 valid;
};

#ifdef ASIC_TRACE_SUPPORT
static struct buffer_index_cache buf_offset[18];
static u32 buf_index = 0;
static u32 pic_buf_count = 0;
static u32 pic_count = 0;
#endif
double CEIL(double a) {
  u32 tmp;
  tmp = (u32) a;
  if ((double) (tmp) < a)
    return ((double) (tmp + 1));
  else
    return ((double) (tmp));
}

u32 DWLReadCacheVersion() {
  u32 version;
  if (!vcmd_used) {
    version = ReadCacheVersion();
  } else
    version = cache_version;
  return (version);
}
void DWLEnableCacheChannel(struct ChannelConf *cfg) {
  CWLChannelConf_t channel_cfg;
  client_type type;
  memset(&channel_cfg,0,sizeof(CWLChannelConf_t));
  i32 ret = CACHE_OK;
  cache_dir dir;
  u32 i;

  /*cache read channel config*/
  if (cfg->mode == RD)
    dir = CACHE_RD;
  else if (cfg->mode == WR)
    dir = CACHE_WR;
  else
    dir = CACHE_BI;
  if (cfg->decoder_type == 0) {
    if (cfg->core_id == 0)
      type = DECODER_G1_0;
    else
      type = DECODER_G1_1;
  } else {
    if (cfg->core_id == 0)
      type = DECODER_G2_0;
    else
      type = DECODER_G2_1;
  }
  channel_cfg.tile_by_tile = cfg->tile_by_tile;
#ifdef ASIC_TRACE_SUPPORT
  channel_cfg.file_fid = cfg->file_fid;
  channel_cfg.hw_dec_pic_count = cfg->hw_dec_pic_count;
  channel_cfg.stream_buffer_id = cfg->stream_buffer_id;
  channel_cfg.trace_start_pic = cfg->trace_start_pic;
#endif
  channel_cfg.cache_enable = cfg->cache_enable;
  channel_cfg.shaper_enable = cfg->shaper_enable;
  channel_cfg.cache_version = cache_version;
  channel_cfg.first_tile = cfg->first_tile;
  if (cfg->mode == RD || cfg->mode == BI) {
    channel_cfg.start_addr = (u64)cfg->start_addr;
    channel_cfg.base_offset = cfg->base_offset;
    channel_cfg.num_tile_cols = cfg->num_tile_cols;
    for (i = 0; i < 16; i++)
      channel_cfg.mc_offset[i] = cfg->mc_offset[i];
    channel_cfg.tb_offset = cfg->tb_offset;
    channel_cfg.prefetch_enable = cfg->prefetch_enable;
    channel_cfg.prefetch_threshold = cfg->prefetch_threshold;
    channel_cfg.shift_h = cfg->shift_h;
    channel_cfg.range_h = cfg->range_h;
    channel_cfg.cache_all = cfg->cache_all;
    channel_cfg.end_addr = cfg->end_addr;
    channel_cfg.dec_mode = cfg->dec_mode;
  }
  if (cfg->mode == WR || cfg->mode == BI) {
    channel_cfg.no_chroma = cfg->no_chroma;
    channel_cfg.hw_id = cfg->hw_id;
    channel_cfg.shaper_bypass = cfg->shaper_bypass;
    channel_cfg.start_addr = (u64)cfg->start_addr;
    channel_cfg.base_offset = cfg->base_offset;
    channel_cfg.line_size = cfg->line_size;
    channel_cfg.line_stride = cfg->line_stride;
    channel_cfg.line_cnt = cfg->line_cnt;
    channel_cfg.stripe_e = cfg->stripe_e;
    channel_cfg.pad_e = cfg->pad_e;
    channel_cfg.rfc_e = cfg->rfc_e;
    channel_cfg.block_e = cfg->block_e;
    channel_cfg.max_h = cfg->max_h;
    channel_cfg.ln_cnt_start = cfg->ln_cnt_start;
    channel_cfg.ln_cnt_mid = cfg->ln_cnt_mid;
    channel_cfg.ln_cnt_end = cfg->ln_cnt_end;
    channel_cfg.ln_cnt_step = cfg->ln_cnt_step;
    channel_cfg.tile_id = cfg->tile_id;
    channel_cfg.tile_num = cfg->tile_num;
    channel_cfg.width = cfg->width;
    channel_cfg.height = cfg->height;
    channel_cfg.num_tile_cols = cfg->num_tile_cols;
    channel_cfg.num_tile_rows = cfg->num_tile_rows;
    channel_cfg.pp_buffer = cfg->pp_buffer;
    channel_cfg.ppu_index = cfg->ppu_index;
    channel_cfg.ppu_sub_index = cfg->ppu_sub_index;
    for (i = 0; i < 2048; i++)
    channel_cfg.tile_size[i] = cfg->tile_size[i];
  }
  ret = EnableCacheChannel(&(cache[cfg->core_id]), &channel_idx, &channel_cfg, type, dir);
  (void) ret;
  return;
}

void DWLEnableCacheWork(u32 core_id) {
  if (cache[core_id] == NULL)
    return;
  EnableCacheWork(cache[core_id]);
}

void DWLRefreshCacheRegs(u32 *cache_regs, u32 *cache_reg_num, u32 *shaper_regs, u32 *shaper_reg_num) {
  *cache_reg_num = 0;
  *shaper_reg_num = 0;
  if (cache[0] == NULL)
    return;
  EnableCacheWorkDumpRegs(cache[0], CACHE_RD, cache_regs, cache_reg_num, shaper_regs, shaper_reg_num);
  EnableCacheWorkDumpRegs(cache[0], CACHE_WR, cache_regs, cache_reg_num, shaper_regs, shaper_reg_num);
}

i32 DWLDisableCacheChannelALL(enum cache_mode mode, u32 core_id) {
  cache_dir dir;
  i32 ret;
  if (cache[core_id] == NULL)
    return -1;
  /*cache read channel config*/
  if (mode == RD)
    dir = CACHE_RD;
  else if (mode == WR)
    dir = CACHE_WR;
  else
    dir = CACHE_BI;
  ret = DisableCacheChannelALL(&(cache[core_id]), dir);
  return ret;
}

u32 cache_exception_lists[64];
u32 cache_exception_regs_num = 0;

void DWLSetCacheExpAddr(addr_t start_addr, addr_t end_addr, u32 core_id) {
  if (cache[core_id] == NULL)
    return;
  SetCacheExpAddr(cache[core_id], (u64)start_addr, (u64)end_addr);

  cache_exception_lists[cache_exception_regs_num++] = (u32)(start_addr);
  cache_exception_lists[cache_exception_regs_num++] = (u32)(end_addr);
  cache_exception_lists[cache_exception_regs_num++] = (u32)((u64)(start_addr) >> 32);
  cache_exception_lists[cache_exception_regs_num++] = (u32)((u64)(end_addr) >> 32);
}

void DWLPrintfInfo(struct ChannelConf *cfg, u32 core_id) {
  CWLChannelConf_t channel_cfg;
  memset(&channel_cfg,0,sizeof(CWLChannelConf_t));
  if (cache[core_id] == NULL)
    return;
  channel_cfg.hw_dec_pic_count = cfg->hw_dec_pic_count;
  channel_cfg.stream_buffer_id = cfg->stream_buffer_id;
  printInfo(cache[core_id], &channel_cfg);
}

void DWLConfigureCacheChannel(const void *instance, i32 core_id, struct CacheParams *cache_cfg, u32 pjpeg_coeff_size_for_cache) {
  struct ChannelConf channel_cfg;
  u32 dec_mode;
  addr_t buff_start_bus;
  u32 len, buffer_offset;
  u32 value;
  u32 muti_stream;
  u32 num_tile_cols = 0;
  u32 pic_height_in_cbs, min_cb_size;
  u32 bit_depth_luma, bit_depth_chroma;
  u32 height64, size;
  u32 pixel_width;
  u32 hw_build_id;
  u32 muti_core_support;
  u32 index[16] = {135,137,139,141,143,145,147,149,151,153,155,157,159,161,163,165};
  u32 i = 0;
  struct DecHwFeatures hw_feature;
  /*shaper read channel config*/
  (void) DWLmemset(&channel_cfg,0,sizeof(struct ChannelConf));
  value = DWLReadReg(instance, core_id, 4 * 3);
  dec_mode = ((value >> 27) & 0x1F);
  if (dec_mode == H264 || dec_mode == H263 || dec_mode == H264_HIGH10)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  else if (dec_mode == MPEG4)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  else if (dec_mode == JPEG)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  else if (dec_mode == VC1)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VC1_DEC);
  else if (dec_mode == MPEG2 || dec_mode == MPEG1)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG2_DEC);
  else if (dec_mode == VP6)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP6_DEC);
  else if (dec_mode == VP7 || dec_mode == VP8)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
  else if (dec_mode == AVS)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  else if (dec_mode == HUKKA)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  else if (dec_mode == VP9)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  else if (dec_mode == HEVC)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  else
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_ST_PP);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  value = DWLReadReg(instance, core_id, 4 * 5);
  muti_stream = ((value >> 16) & 0x1);
  buffer_offset = DWLReadReg(instance, core_id, 4 * 259);
#ifdef ASIC_TRACE_SUPPORT
  muti_core_support = tb_cfg.dec_params.muti_core_support;
#else 
  muti_core_support = (((DWLReadReg(instance, core_id, 4 * 58)) >> 30) & 0x1);
#endif
#ifdef ASIC_TRACE_SUPPORT
  cache_version = tb_cfg.dec_params.cache_version;
#else
  cache_version = DWLReadCacheVersion();
#endif
  if (dec_mode == HEVC || dec_mode == VP9 || dec_mode == PP || dec_mode == AVS2)
    channel_cfg.decoder_type = 1;
  else
    channel_cfg.decoder_type = 0;
#ifdef ASIC_TRACE_SUPPORT
  channel_cfg.file_fid = cache_fid;
  channel_cfg.hw_dec_pic_count = hw_dec_pic_count;
  channel_cfg.stream_buffer_id = stream_buffer_id;
  if(stream_buffer_id)
    channel_cfg.first_tile = 0;
  else
    channel_cfg.first_tile = 1;
  channel_cfg.cache_enable = tb_cfg.cache_enable;
  channel_cfg.shaper_enable = tb_cfg.shaper_enable;
  channel_cfg.trace_start_pic = tb_cfg.tb_params.first_trace_frame;
  channel_cfg.base_offset = buffer_offset;
  if ((dec_mode == H264_HIGH10 || dec_mode == HEVC || dec_mode == H264) && muti_core_support) {
    for (i = 0; i < cache_cfg[core_id].dpb_size; i++) {
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * index[i]) |
                       (((u64)DWLReadReg(instance, core_id, 4 * (index[i] - 1))) << 32));
      if (buff_start_bus) {
        channel_cfg.mc_offset[i] = (u8*)buff_start_bus - dpb_base_address;
      }
    }
  }
#endif
  if (dec_mode == HEVC || dec_mode == VP9) {
    if (hw_feature.pic_size_reg_unified) {
      num_tile_cols = ((DWLReadReg(instance, core_id, 4 * 10)) >> 17) & 0x7F;
    } else {
      num_tile_cols = ((DWLReadReg(instance, core_id, 4 * 10)) >> 19) & 0x1F;
    }
    channel_cfg.num_tile_cols = num_tile_cols;
  }
  
  /*cache read channel config*/
  channel_cfg.core_id = core_id;
  channel_cfg.cache_all = 1;
  channel_cfg.mode = RD;
  channel_cfg.dec_mode = dec_mode;
  DWLEnableCacheChannel(&channel_cfg);
  if (dec_mode == HEVC || dec_mode == VP9) {
    if (num_tile_cols >= 2) {
      bit_depth_luma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 6) & 0x03) + 8;
      bit_depth_chroma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 4) & 0x03) + 8;
      pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ? 8 : 10;
      pic_height_in_cbs = ((DWLReadReg(instance, core_id, 4 * 4)) >> 6) & 0x1FFF;
      min_cb_size = ((DWLReadReg(instance, core_id, 4 * 12)) >> 13) & 0x07;
      height64 = ((pic_height_in_cbs << min_cb_size) + 63) & (~63);
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 179) |
                       (((u64)DWLReadReg(instance, core_id, 4 * 178)) << 32));
      if (dec_mode == HEVC)
        size = ASIC_VERT_FILTER_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width / 8;
      else
        size = 24 * height64 * (num_tile_cols - 1) * pixel_width / 8;
      DWLSetCacheExpAddr(buff_start_bus, buff_start_bus + size, core_id);
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 183) |
                       (((u64)DWLReadReg(instance, core_id, 4 * 182)) << 32));
      if (dec_mode == HEVC)
        size = ASIC_BSD_CTRL_RAM_SIZE * height64 * (num_tile_cols - 1);
      else
        size = 16 * (height64 / 4) * (num_tile_cols - 1);
      DWLSetCacheExpAddr(buff_start_bus, buff_start_bus + size, core_id);
      if (dec_mode == HEVC) {
        buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 181) |
                         (((u64)DWLReadReg(instance, core_id, 4 * 180)) << 32));
        size = ASIC_VERT_SAO_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width / 8;
        DWLSetCacheExpAddr(buff_start_bus, buff_start_bus + size, core_id);
      }
    }
  }
  if (dec_mode == VP6 || dec_mode == VP7 ||
      dec_mode == VP8 ) {
    if( dec_mode == VP6 && muti_stream == 0) {
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 169) |
                       (((u64)DWLReadReg(instance, core_id, 4 * 168)) << 32));
      len = DWLReadReg(instance, core_id, 4 * 6);
    } else {
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 93) |
                       (((u64)DWLReadReg(instance, core_id, 4 * 92)) << 32));
      len = DWLReadReg(instance, core_id, 4 * 24);
    }
  } else {
    buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 169) |
                     (((u64)DWLReadReg(instance, core_id, 4 * 168)) << 32));
    len = DWLReadReg(instance, core_id, 4 * 6);
  }
  if (dec_mode == H264)
    DWLSetCacheExpAddr(buff_start_bus + buffer_offset, buff_start_bus + len + buffer_offset, core_id);
  if ((dec_mode == H264_HIGH10 || dec_mode == HEVC || dec_mode == H264) && muti_core_support) {
    for (i = 0; i < cache_cfg[core_id].dpb_size; i++) {
      buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * index[i]) |
                       (((u64)DWLReadReg(instance, core_id, 4 * (index[i] - 1))) << 32));
      if (buff_start_bus)
        DWLSetCacheExpAddr(buff_start_bus - 32, buff_start_bus + cache_cfg[core_id].buf_size, core_id);
    }
  }
  if (dec_mode == JPEG) {
    buff_start_bus = (addr_t)(DWLReadReg(instance, core_id, 4 * 133) |
        		     (((u64)DWLReadReg(instance, core_id, 4 * 132)) << 32));
    len = pjpeg_coeff_size_for_cache;
    DWLSetCacheExpAddr(buff_start_bus, buff_start_bus + len, core_id);
  }
  /*enable work channel*/
  if (!vcmd_used)
    DWLEnableCacheWork(core_id);
}

#ifdef ASIC_TRACE_SUPPORT
static u8* Avs2FieldPpBufOffset(const void *instance, i32 core_id, PpUnitIntConfig *ppu_cfg, u32 *offset) {
  u8* pp_base_virtual = NULL;
  u32 sim_buf_offset = 0;
  u32 found = 0;
  u32 pic_ready = 0;
  u32 pp_lu_size, pp_ch_size, pp_buff_size = 0;
  u32 pp_h_luma, pp_h_chroma;
  u32 i;
  u32 field_sequence,top_field_first,is_top_field; 
  PpUnitIntConfig *ppu_cfg_temp = ppu_cfg;
  field_sequence = ((DWLReadReg(instance, core_id, 4 * 3)) >> 23) & 0x01;
  top_field_first = ((DWLReadReg(instance, core_id, 4 * 4)) >> 5) & 0x01;
  is_top_field = ((DWLReadReg(instance, core_id, 4 * 3)) >> 19) & 0x01;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (ppu_cfg->enabled) {
      pp_h_luma = ppu_cfg->scale.height;
      pp_h_chroma = ppu_cfg->scale.height / 2;
      if (ppu_cfg->tiled_e) {
        pp_h_luma = NEXT_MULTIPLE(pp_h_luma, 4) / 4;
        pp_h_chroma = NEXT_MULTIPLE(pp_h_chroma, 4) / 4;
      }
      if (ppu_cfg->planar)
        pp_h_chroma = ppu_cfg->scale.height;
      pp_lu_size = ppu_cfg->ystride * pp_h_luma;
      pp_ch_size = ppu_cfg->cstride * pp_h_chroma;
      pp_buff_size += (pp_lu_size + (ppu_cfg->monochrome ? 0 : pp_ch_size));
    }
  }
  pp_buff_size = NEXT_MULTIPLE(pp_buff_size, 16);
  ppu_cfg = ppu_cfg_temp;
  u32 index[DEC_MAX_PPU_COUNT] = {325, 347, 364, 381, 452};
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (ppu_cfg->enabled) {
      pp_lu_size = ppu_cfg->ystride;
      if (ppu_cfg->tiled_e) {
        pp_h_luma = NEXT_MULTIPLE(pp_h_luma, 4) / 4;
        pp_lu_size = ppu_cfg->ystride * pp_h_luma / 2;
      }
      u8* dscale_y = (u8*)((addr_t)(DWLReadReg(instance, core_id, 4 * (index[i] + 1)) |
                          (((u64)DWLReadReg(instance, core_id, 4 * index[i])) << 32)));
      if (!is_top_field) {
        pp_base_virtual = dscale_y - pp_lu_size;
      } else {
        pp_base_virtual = dscale_y;
      }
      break;
    }
  }
  pic_ready = (!field_sequence || (field_sequence &&
               ((top_field_first && !is_top_field) ||
               (!top_field_first && is_top_field))));
  if (!pic_ready) {
    found = 0;
    for(i = 0; i < buf_index; i++) {
      if (buf_offset[i].pp_base == pp_base_virtual) {
        assert(buf_offset[i].valid == 0);
        buf_offset[i].valid = 1;
        buf_offset[i].offset = sim_buf_offset = (pic_buf_count % 9) * pp_buff_size;
        pic_buf_count++;
        found = 1;
      }
    }
    if (!found) {
      buf_offset[buf_index].pp_base = pp_base_virtual;
      buf_offset[buf_index].valid = 1;
      buf_offset[buf_index].offset = sim_buf_offset = (pic_buf_count % 9) * pp_buff_size;
      pic_buf_count++;
      buf_index++;
    }
  } else {
    found = 0;
    for(i = 0; i < buf_index; i++) {
      if (buf_offset[i].pp_base == pp_base_virtual) {
        assert(buf_offset[i].valid == 1);
        buf_offset[i].valid = 0;
        sim_buf_offset = buf_offset[i].offset;
        found = 1;
      }
    }
    if (!found)
      assert(0);
  }
  *offset = sim_buf_offset;
  return (pp_base_virtual);
}
#endif
void DWLConfigureShaperChannel(const void *instance, i32 core_id, PpUnitIntConfig *ppu_cfg, u32 pp_enabled,
                               u32 tile_by_tile, u32 tile_id, u32 *num_tiles, u16 *tiles, u32 *chroma_exit) {
  struct ChannelConf channel_cfg;
  u32 dec_mode;
  u32 mono_chrome = 0;
  addr_t pic_start_luma_bus;
  addr_t pic_start_chroma_bus = 0;
  u32 pic_width_in_mbs, pic_height_in_mbs;
  u32 frame_mbs_only_flag = 0, field_pic_flag = 0, mb_adaptive_frame_field_flag = 0, bottom_flag = 0;
  (void) DWLmemset(&channel_cfg,0,sizeof(struct ChannelConf));
  u32 stride, line_cnt;
  u32 value;
  u32 num_tile_cols = 1, num_tile_rows = 1;
  u32 pixel_width;
  u32 tile_exit = 0;
  u32 i, j;
  u32 ctb_size;
  u32 bit_depth_luma, bit_depth_chroma;
  u32 pic_width_in_cbs, pic_height_in_cbs;
  u32 min_cb_size;
  u32 hw_build_id;
  u32 use_video_compressor = 0;
  u32 no_chroma = 0;
  u32 tile_info_width[64];
  u32 tile_info_height[64];
  u32 tile_width, tile_height;
  u32 count = 0;
  u32 pic_height, pic_width;
#ifdef ASIC_TRACE_SUPPORT
  u64 tmp;
  u32 hw_id;
  u32 sim_buf_offset = 0;
  u8* pp_base_virtual = NULL;
  addr_t pp_base_address = 0;
#endif
  u32 rgb_flag = 0;
  u32 max_lu_height, max_ch_height;
  u32 is_top_field = ((DWLReadReg(instance, core_id, 4 * 3)) >> 19) & 0x01;
  u32 pp_field_offset, pp_field_offset_ch;
  struct DecHwFeatures hw_feature;
  u32 cache_version;
  /*shaper write channel config*/
#ifdef ASIC_TRACE_SUPPORT
  hw_id = tb_cfg.dec_params.hw_version;
  cache_version = tb_cfg.dec_params.cache_version;
#else
  cache_version = DWLReadCacheVersion();
#endif
  value = DWLReadReg(instance, core_id, 4 * 3);
  dec_mode = ((value >> 27) & 0x1F);
  if (dec_mode == H264 || dec_mode == H263 || dec_mode == H264_HIGH10)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_H264_DEC);
  else if (dec_mode == MPEG4)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  else if (dec_mode == JPEG)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  else if (dec_mode == VC1)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VC1_DEC);
  else if (dec_mode == MPEG2 || dec_mode == MPEG1)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG2_DEC);
  else if (dec_mode == VP6)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP6_DEC);
  else if (dec_mode == VP7 || dec_mode == VP8)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP8_DEC);
  else if (dec_mode == AVS)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  else if (dec_mode == HUKKA)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_RV_DEC);
  else if (dec_mode == VP9)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_VP9_DEC);
  else if (dec_mode == HEVC)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  else if (dec_mode == AVS2)
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  else
    hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_ST_PP);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  channel_cfg.core_id = core_id;
#ifdef ASIC_TRACE_SUPPORT
  if (pic_count == 0) {
    memset(buf_offset, 0x0, 18 * sizeof(struct buffer_index_cache));
    pic_count++;
  }
  channel_cfg.shaper_bypass = tb_cfg.shaper_bypass;
  channel_cfg.cache_enable = tb_cfg.cache_enable;
  channel_cfg.shaper_enable = tb_cfg.shaper_enable;
  channel_cfg.trace_start_pic = tb_cfg.tb_params.first_trace_frame;
  channel_cfg.file_fid = cache_fid;
  channel_cfg.hw_dec_pic_count = hw_dec_pic_count;
  channel_cfg.stream_buffer_id = stream_buffer_id;
  channel_cfg.hw_id = hw_id;
#endif
  if (dec_mode == PP) {
    channel_cfg.decoder_type = 1;
    channel_cfg.mode = WR;
    channel_cfg.stripe_e = 0;
    if (cache_version < 0x3)
      channel_cfg.block_e = 0;
    channel_cfg.pp_buffer = 0;
    channel_cfg.first_tile = 1;
    if (pp_enabled) {
      PpUnitIntConfig *ppu_cfg_temp = ppu_cfg;
      addr_t ppu_out_bus_addr = 0;
      channel_cfg.pp_buffer = 1;
      if (cache_version < 0x3)
        channel_cfg.block_e = 1;
      u32 first = 0;
      u32 pre_fetch_height = (((DWLReadReg(instance, core_id, 4 * 320)) >> 8) & 0x07) == 0 ? 16 : 64;
      pixel_width = (((DWLReadReg(instance, core_id, 4 * 322)) >> 27) & 0x1F) == 1 ? 8 : 16;
      u32 index[DEC_MAX_PPU_COUNT] = {325, 347, 364, 381, 452};
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (ppu_cfg->enabled && !first) {
          ppu_out_bus_addr = (addr_t)(DWLReadReg(instance, core_id, 4 * (index[i] + 1)) |
                             (((u64)DWLReadReg(instance, core_id, 4 * index[i])) << 32));
          if (ppu_cfg->rgb_planar && !IS_PACKED_RGB(ppu_cfg->rgb_format))
            ppu_out_bus_addr -= 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
          first = 1;
        }
        if (ppu_cfg->enabled && (ppu_cfg->rgb || ppu_cfg->rgb_planar))
          rgb_flag = 1;

        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled) continue;
      }

      if (rgb_flag) {
        max_lu_height = 80;
        max_ch_height = 40;
      } else {
        max_lu_height = 72;
        max_ch_height = 40;
      }
      ppu_cfg = ppu_cfg_temp;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {

        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled) continue;
        u32 scaled_y_height, scaled_c_height;
        if (ppu_cfg->crop.height < ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL((double)pre_fetch_height * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1)))+1;
          scaled_c_height = (int)(CEIL((double)pre_fetch_height / 2 * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1)))+1;
        } else if (ppu_cfg->crop.height > ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL(pre_fetch_height * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536))))+1;
          scaled_c_height = (int)(CEIL(pre_fetch_height / 2 * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536))))+1;
          if (scaled_y_height > max_lu_height)
            scaled_y_height = max_lu_height;
          if (scaled_c_height > max_ch_height)
            scaled_c_height = max_ch_height;
        } else {
          scaled_y_height = pre_fetch_height;
          scaled_c_height = pre_fetch_height / 2;
        }
        /*luma*/
	channel_cfg.ppu_index = i;
        channel_cfg.pad_e = 1;
        channel_cfg.rfc_e = 0;
	if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
          if (hw_feature.rgb_line_stride_support)
  	    channel_cfg.pad_e = 1;
          else
            channel_cfg.pad_e = 0;
	  channel_cfg.ppu_sub_index = 0;
          channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = ppu_cfg->luma_offset;
#endif
	  channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = (ppu_cfg->rgb_planar ? 1 : 3) * ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
	  channel_cfg.line_stride = ppu_cfg->ystride >> 4;
          channel_cfg.line_cnt = ppu_cfg->scale.height;
          channel_cfg.max_h = scaled_y_height;
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
	  if (ppu_cfg->rgb_planar) {
            channel_cfg.ppu_sub_index = 1;
	    channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
            channel_cfg.line_stride = ppu_cfg->ystride >> 4;
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->luma_offset + NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
            channel_cfg.max_h = scaled_y_height;
            DWLEnableCacheChannel(&channel_cfg);
            channel_cfg.ppu_sub_index = 2;
	    channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
            channel_cfg.line_stride = ppu_cfg->ystride >> 4;
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->luma_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
            channel_cfg.max_h = scaled_y_height;
            DWLEnableCacheChannel(&channel_cfg);
	  }
	} else {
	  channel_cfg.ppu_sub_index = 0;
          channel_cfg.line_stride = ppu_cfg->tiled_e ? NEXT_MULTIPLE(ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) >> 7
                                                     : NEXT_MULTIPLE((((ppu_cfg->scale.width * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> 4;
          channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset /*+
                                 (dec_cont->tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = ppu_cfg->luma_offset /*+
                                    (dec_cont->tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                   : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
          channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height);
          channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_y_height + 3 + 2) / 4 : scaled_y_height;
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
          if (!ppu_cfg->monochrome) {
            /*chroma*/
	    channel_cfg.ppu_sub_index = 1;
            channel_cfg.pad_e = 1;
            channel_cfg.rfc_e = 0;
            channel_cfg.line_stride = ppu_cfg->tiled_e ? NEXT_MULTIPLE(ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) >> 7
                                                       : (ppu_cfg->planar ? NEXT_MULTIPLE((((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> 4
                                                       : NEXT_MULTIPLE(((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> 4);
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset /*+
                                   (dec_cont->tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->chroma_offset /*+
                                    (dec_cont->tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                     : (ppu_cfg->planar ? (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15
                                                     : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15);
            channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height)/2;
            channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height;
#ifdef ASIC_TRACE_SUPPORT
            shaper_flag[2*i+2] = 1;
#endif
            DWLEnableCacheChannel(&channel_cfg);
            if (ppu_cfg->planar) {
	      channel_cfg.ppu_sub_index = 2;
              channel_cfg.line_stride = NEXT_MULTIPLE((((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> 4;
              channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset + ppu_cfg->cstride * ppu_cfg->scale.height/2;
#ifdef ASIC_TRACE_SUPPORT
              channel_cfg.base_offset = ppu_cfg->chroma_offset + ppu_cfg->cstride * ppu_cfg->scale.height/2;
#endif
              channel_cfg.start_addr = channel_cfg.start_addr >> 4;
              channel_cfg.line_size = (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
              channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : ppu_cfg->scale.height/2;
              channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height;
              DWLEnableCacheChannel(&channel_cfg);
	    }
          }
        }
      }
    }
    /*enable work channel*/
    if (!vcmd_used)
      DWLEnableCacheWork(core_id);
  } else if (dec_mode == HEVC || dec_mode == VP9 || dec_mode == AVS2) {
    if (hw_feature.pic_size_reg_unified) {
      num_tile_cols = ((DWLReadReg(instance, core_id, 4 * 10)) >> 17) & 0x7F;
      num_tile_rows = ((DWLReadReg(instance, core_id, 4 * 10)) >> 12) & 0x1F;
    } else {
      num_tile_cols = ((DWLReadReg(instance, core_id, 4 * 10)) >> 19) & 0x1F;
      num_tile_rows = ((DWLReadReg(instance, core_id, 4 * 10)) >> 14) & 0x1F;
    }
    bit_depth_luma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 6) & 0x03) + 8;
    bit_depth_chroma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 4) & 0x03) + 8;
    pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ? 8 : 10;
    pic_width_in_cbs = ((DWLReadReg(instance, core_id, 4 * 4)) >> 19) & 0x1FFF;
    pic_height_in_cbs = ((DWLReadReg(instance, core_id, 4 * 4)) >> 6) & 0x1FFF;
    min_cb_size = ((DWLReadReg(instance, core_id, 4 * 12)) >> 13) & 0x07;
    ctb_size = 1 << (((DWLReadReg(instance, core_id, 4 * 12)) >> 10) & 0x07);
    pic_width = pic_width_in_cbs << min_cb_size;
    pic_height = pic_height_in_cbs << min_cb_size;
    field_pic_flag = ((DWLReadReg(instance, core_id, 4 * 3)) >> 23) & 0x01;
    mono_chrome = (dec_mode == HEVC && (((DWLReadReg(instance, core_id, 4 * 7)) >> 30) & 0x01));
    use_video_compressor = ((((DWLReadReg(instance, core_id, 4 * 3)) >> 8) & 0x01) == 0);
    pic_start_luma_bus = ((addr_t)(DWLReadReg(instance, core_id, 4 * 65) |
                         (((u64)DWLReadReg(instance, core_id, 4 * 64)) << 32))) & (~0x3);
    pic_start_chroma_bus = ((addr_t)(DWLReadReg(instance, core_id, 4 * 99) |
                           (((u64)DWLReadReg(instance, core_id, 4 * 98)) << 32))) & (~0x3);
    for (i = 0; i < num_tile_rows; i++) {
      for (j = 0; j < num_tile_cols; j++) {
        tile_info_width[j] = *tiles++;
        tile_info_height[i] = *tiles++;
      }
    }
    if (dec_mode == VP9) {
      for (i = 0; i < num_tile_rows; i++) {
        if (tile_info_height[i] != 0)
          count++;
      }
      if (count != num_tile_rows)
        num_tile_rows = count;
    }
    *num_tiles = num_tile_rows * num_tile_cols;
    if (dec_mode == HEVC && ctb_size == 16) {
      if (tile_info_height[0] == 1 || tile_info_width[0] == 1)
        no_chroma = 1;
    }
#ifdef ASIC_TRACE_SUPPORT
    if (no_chroma) {
      if (!tb_cfg.cache_enable)
        tb_cfg.dec_params.cache_support = 0;
    }
#endif
    *chroma_exit = no_chroma;
    if (no_chroma)
      return;
    if (dec_mode != AVS2 && tile_id >= num_tile_cols * num_tile_rows)
      return;
    channel_cfg.decoder_type = 1;
    channel_cfg.tile_by_tile = tile_by_tile;
    /*shaper write channel config*/
    channel_cfg.mode = WR;
    channel_cfg.no_chroma = no_chroma;
    channel_cfg.tile_id = tile_id;
    if (cache_version < 0x3)
      channel_cfg.block_e = 0;
    channel_cfg.pp_buffer = 0;
    if (dec_mode == AVS2)
      num_tile_cols = num_tile_rows = 1;
    if (num_tile_cols == 0 || num_tile_rows == 0 || !tile_by_tile) {
      channel_cfg.tile_num = 1;
      channel_cfg.num_tile_cols = 1;
      channel_cfg.num_tile_rows = 1;
      channel_cfg.tile_size[0] = pic_width;
      channel_cfg.tile_size[1] = pic_height;
    } else {
      channel_cfg.tile_num = num_tile_cols * num_tile_rows;
      channel_cfg.num_tile_cols = num_tile_cols;
      channel_cfg.num_tile_rows = num_tile_rows;
      u32 *p = channel_cfg.tile_size;

      for (i = 0; i < num_tile_rows; i++) {
        for (j = 0; j < num_tile_cols; j++) {
          *p++ = tile_info_width[j] * ctb_size;
          *p++ = tile_info_height[i] * ctb_size;
        }
      }
    }
    if (!((channel_cfg.num_tile_cols < 2) && (channel_cfg.num_tile_rows < 2)))
      tile_exit = 1;
    if (no_chroma)
      tile_exit = 0;
    u32 stride = ((DWLReadReg(instance, core_id, 4 * 314)) >> 16) & 0xFFFF;
    u32 luma_stride = ((DWLReadReg(instance, core_id, 4 * 314)) >> 16) & 0xFFFF;
    u32 chroma_stride = (DWLReadReg(instance, core_id, 4 * 314)) & 0xFFFF;
    u32 col, row;
    u32 top_tile = 1;
    u32 bottom_tile = 1;
    u32 left_tile = 0;
    u32 right_tile = 0;
    u32 tile_col_width = pic_width;
    u32 tile_row_height = pic_height;
    u32 rem;
    tile_width = tile_height = 0;
    if (tile_exit) {
      col = tile_id % num_tile_cols;
      row = tile_id / num_tile_cols;
      top_tile = (tile_id <= num_tile_cols - 1);
      bottom_tile = (tile_id >= num_tile_cols * (num_tile_rows - 1));
      left_tile = ((tile_id % num_tile_cols) == 0);
      right_tile = ((tile_id % num_tile_cols) == (num_tile_cols - 1));
      for (i = 0; i < col; i++)
        tile_width += tile_info_width[i];
      for (i = 0; i < row; i++)
        tile_height += tile_info_height[i];
      if (right_tile)
        tile_col_width = (pic_width_in_cbs << min_cb_size) - tile_width * ctb_size;
      else
        tile_col_width = tile_info_width[col] * ctb_size;
      if (bottom_tile)
        tile_row_height = (pic_height_in_cbs << min_cb_size) - tile_height * ctb_size;
      else
        tile_row_height = tile_info_height[row] * ctb_size;
    }
    if (tile_exit) {
      if (!tile_id)
        channel_cfg.first_tile = 1;
      else
        channel_cfg.first_tile = 0;
    } else
      channel_cfg.first_tile = 1;
    channel_cfg.width = tile_col_width;
    channel_cfg.height = tile_row_height;
    rem = tile_row_height % 64;
    if (0) {
      if (ctb_size == 16) {
        /*luma*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
          channel_cfg.rfc_e = 0;
        } else {
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
        }
        channel_cfg.stripe_e = 0;
        channel_cfg.start_addr = pic_start_luma_bus +
                                 (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                 (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 4;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = pic_width * 4;
        else
          channel_cfg.line_size = (tile_col_width -
                                   (left_tile ? 8 : 0)  + (right_tile ? 8 : 0)) * 4;
        channel_cfg.line_stride = stride>>4;
        if (!tile_exit)
          channel_cfg.line_cnt = pic_height / 4;
        else {
          if (tile_row_height < (top_tile ? 8 : 0))
            return;
          else
            channel_cfg.line_cnt = (tile_row_height - (top_tile ? 8 : 0)
                                    + (bottom_tile ? 8 : 0)) / 4;
        }
        channel_cfg.max_h = (16 + 8) / 4;
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        if (!(tile_exit && (((tile_row_height == 16) && (tile_height == 0)) || ((tile_col_width == 16) && (tile_width == 0))))) {
          if (!use_video_compressor) {
            channel_cfg.pad_e = 1;
            channel_cfg.rfc_e = 0;
          } else {
            channel_cfg.pad_e = 0;
            channel_cfg.rfc_e = 1;
          }
          channel_cfg.stripe_e = 0;
          channel_cfg.start_addr = (pic_start_chroma_bus +
                                    (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                    (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 2*8) : 0)) * 4);
#ifdef ASIC_TRACE_SUPPORT
          tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
          channel_cfg.base_offset = tmp;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          if (!tile_exit)
            channel_cfg.line_size = pic_width * 4;
          else
            channel_cfg.line_size = (tile_col_width  -
                                    (left_tile ? 2*8 : 0)  + (right_tile ? 2*8 : 0)) * 4;
          channel_cfg.line_stride = stride>>4;
          if (!tile_exit)
            channel_cfg.line_cnt = pic_height / 8;
          else {
            if (tile_row_height / 2 < (top_tile ? 8 : 0))
              return;
            else
              channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0)
                                     + (bottom_tile ? 8 : 0)) / 4;
          }
          channel_cfg.max_h = (8 + 8) / 4;
          DWLEnableCacheChannel(&channel_cfg);
        }
      } else if (ctb_size == 32) {
        /*luma*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
          channel_cfg.rfc_e = 0;
        } else {
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
        }
        channel_cfg.stripe_e = 0;
        channel_cfg.start_addr = pic_start_luma_bus +
                                 (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                 (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 4;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif 
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = pic_width * 4;
        else
          channel_cfg.line_size = (tile_col_width  -
                                   (left_tile ? 8 : 0)  + (right_tile ? 8 : 0)) * 4;
        channel_cfg.line_stride = stride>>4;
        if (!tile_exit)
          channel_cfg.line_cnt = pic_height / 4;
        else {
          if (tile_row_height < (top_tile ? 8 : 0))
            return;
          else
            channel_cfg.line_cnt = (tile_row_height - (top_tile ? 8 : 0)
                                   + (bottom_tile ? 8 : 0)) / 4;
        }
        channel_cfg.max_h = (32 + 8) / 4;
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
          channel_cfg.rfc_e = 0;
        } else {
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
        }
        channel_cfg.stripe_e = 0;
        channel_cfg.start_addr = (pic_start_chroma_bus +
                                  (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                  (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 2*8) : 0)) * 4);
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = pic_width * 4;
        else
          channel_cfg.line_size = (tile_col_width -
                                   (left_tile ? 2*8 : 0)  + (right_tile ? 2*8 : 0)) * 4;
        channel_cfg.line_stride = stride>>4;
        if (!tile_exit)
          channel_cfg.line_cnt = pic_height / 8;
        else {
          if (tile_row_height / 2 < (top_tile ? 8 : 0))
            return;
          else
            channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0)
                                   + (bottom_tile ? 8 : 0)) / 4;
        }
        channel_cfg.max_h = (16 + 8) / 4;
        DWLEnableCacheChannel(&channel_cfg);
      } else {
        /*luma top 32x32*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
          channel_cfg.rfc_e = 0;
        } else {
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
        }
        channel_cfg.stripe_e = 1;
        channel_cfg.start_addr = pic_start_luma_bus +
                                 (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                 (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 4;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = pic_width * 4;
        else
          channel_cfg.line_size = (tile_col_width  -
                                   (left_tile ? 8 : 0)  + (right_tile ? 8 : 0)) * 4;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = (tile_row_height % 64 == 0) ? tile_row_height / 64 :tile_row_height / 64 + 1;
        channel_cfg.ln_cnt_start = tile_row_height > 64 ? (32 - (top_tile ? 8 : 0)) / 4 :
                                   ((rem > 32 || rem == 0) ? ((32 - (top_tile ? 8 : 0)) / 4) : ((rem + 8) / 4));
        channel_cfg.ln_cnt_mid = tile_row_height > 128 ? 32 / 4 : 0;
        channel_cfg.ln_cnt_end = tile_row_height > 64 ? ((rem > 32 || rem == 0) ? 32 / 4 : ((rem + 8) / 4)) : 0;
        channel_cfg.ln_cnt_step = 32 / 4;
        channel_cfg.max_h = MAX3(channel_cfg.ln_cnt_start, channel_cfg.ln_cnt_mid, channel_cfg.ln_cnt_end);
        DWLEnableCacheChannel(&channel_cfg);
        /*luma bottom 32x32*/
        if (!((rem <= 32) && (tile_row_height < 64))) {
          if (!use_video_compressor) {
            channel_cfg.pad_e = 1;
            channel_cfg.rfc_e = 0;
          } else {
            channel_cfg.pad_e = 0;
            channel_cfg.rfc_e = 1;
          }
          channel_cfg.stripe_e = 1;
          if (!tile_exit)
            channel_cfg.line_size = pic_width * 4;
          else
            channel_cfg.line_size = (tile_col_width  -
                                    (left_tile ? 8 : 0)  + (right_tile ? 8 : 0)) * 4;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.start_addr = pic_start_luma_bus +
                                   (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                   (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 4 +
                                   stride * (32 - (top_tile ? 8 : 0)) / 4;
#ifdef ASIC_TRACE_SUPPORT
          tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
          channel_cfg.base_offset = tmp;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_cnt = (tile_row_height % 64 == 0) ? tile_row_height / 64 :tile_row_height / 64 + 1;
          channel_cfg.ln_cnt_start = tile_row_height > 64 ? 32 / 4 : ((rem > 32 || rem == 0) ? ((rem == 0 ? 64 : rem) - 32 + 8) / 4 : 0);
          channel_cfg.ln_cnt_mid = tile_row_height > 128 ? 32 / 4 : 0;
          channel_cfg.ln_cnt_end = tile_row_height > 64 ? ((rem > 32 || rem == 0) ? ((rem == 0 ? 64 : rem) - 32 + 8) / 4 : 0) : 0;
          channel_cfg.ln_cnt_step = 32 / 4;
          channel_cfg.max_h = MAX3(channel_cfg.ln_cnt_start, channel_cfg.ln_cnt_mid, channel_cfg.ln_cnt_end);
          DWLEnableCacheChannel(&channel_cfg);
        }
        /*chroma top 32x32*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
          channel_cfg.rfc_e = 0;
        } else {
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
        }
        channel_cfg.stripe_e = 1;
        channel_cfg.start_addr = pic_start_chroma_bus +
                                 (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride +
                                 (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 2*8) : 0)) * 4;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif 
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = pic_width * 4;
        else
          channel_cfg.line_size = (tile_col_width -
                                   (left_tile ? 2*8 : 0)  + (right_tile ? 8*2 : 0))* 4;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = (tile_row_height % 64 == 0) ? tile_row_height / 64 :tile_row_height / 64 + 1;
        channel_cfg.ln_cnt_start = tile_row_height > 64 ? (16 - (top_tile ? 8 : 0)) / 4 :
                                   ((rem > 32 || rem == 0) ? ((16 - (top_tile ? 8 : 0)) / 4) : ((rem / 2 + 8) / 4));
        channel_cfg.ln_cnt_mid = tile_row_height > 128 ? 16 / 4 : 0;
        channel_cfg.ln_cnt_end = tile_row_height > 64 ? ((rem > 32 || rem == 0) ? 16 / 4 : ((rem / 2 + 8) / 4)) : 0;
        channel_cfg.ln_cnt_step = 16 / 4;
        channel_cfg.max_h = MAX3(channel_cfg.ln_cnt_start, channel_cfg.ln_cnt_mid, channel_cfg.ln_cnt_end);
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma bottom 32x32*/
        if (!((rem <= 32) && (tile_row_height < 64))) {
          if (!use_video_compressor) {
            channel_cfg.pad_e = 1;
            channel_cfg.rfc_e = 0;
          } else {
            channel_cfg.pad_e = 0;
            channel_cfg.rfc_e = 1;
          }
          channel_cfg.stripe_e = 1;
          if (!tile_exit)
            channel_cfg.line_size = pic_width * 4;
          else
            channel_cfg.line_size = (tile_col_width -
                                     (left_tile ? 2*8 : 0)  + (right_tile ? 8*2 : 0)) * 4;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.start_addr = pic_start_chroma_bus + stride * (16 - (top_tile ? 8 : 0)) / 4 +
                                   (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * stride + (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 2*8) : 0)) * 4;
#ifdef ASIC_TRACE_SUPPORT
          tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
          channel_cfg.base_offset = tmp;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_cnt = (tile_row_height % 64 == 0) ? tile_row_height / 64 :tile_row_height / 64 + 1;
          channel_cfg.ln_cnt_start = tile_row_height > 64 ? 16 / 4 : ((rem > 32 || rem == 0) ? (((rem == 0 ? 64 : rem) - 32) / 2 + 8) / 4 : 0);
          channel_cfg.ln_cnt_mid = tile_row_height > 128 ? 16 / 4 : 0;
          channel_cfg.ln_cnt_end = tile_row_height > 64 ? ((rem > 32 || rem == 0) ? (((rem == 0 ? 64 : rem) - 32) / 2 + 8) / 4 : 0) : 0;
          channel_cfg.ln_cnt_step = 16 / 4;
          channel_cfg.max_h = MAX3(channel_cfg.ln_cnt_start, channel_cfg.ln_cnt_mid, channel_cfg.ln_cnt_end);
          DWLEnableCacheChannel(&channel_cfg);
        }
      }
    } else {
      if (!use_video_compressor) {
        /*luma*/
        channel_cfg.stripe_e = 0;
        channel_cfg.pad_e = 1;
        if (cache_version < 0x3)
          channel_cfg.rfc_e = 0;
        else
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = pic_start_luma_bus +
                                 (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * luma_stride +
                                 (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 4 * pixel_width / 8;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        tmp = tmp / 16 * 16;
        channel_cfg.base_offset = tmp;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!tile_exit)
          channel_cfg.line_size = (pic_width * 4 * pixel_width) / 8;
        else
          channel_cfg.line_size = (tile_col_width -
                                   (left_tile ? 8 : 0)  + (right_tile ? 8 : 0)) * 4 * pixel_width / 8;
        channel_cfg.line_stride = luma_stride>>4;
        if (!tile_exit)
          channel_cfg.line_cnt = pic_height / 4;
        else {
          if (tile_row_height < (top_tile ? 8 : 0))
            return;
          else
            channel_cfg.line_cnt = (tile_row_height - (top_tile ? 8 : 0)
                                    + (bottom_tile ? 8 : 0)) / 4;
        }
        channel_cfg.max_h = (ctb_size + 8) / 4;
#ifdef ASIC_TRACE_SUPPORT
        shaper_flag[0] = 1;
#endif
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        if ((!(tile_exit && (((tile_row_height == 16) && (tile_height == 0)) || ((tile_col_width == 16) && (tile_width == 0))))) && !mono_chrome) {
          channel_cfg.stripe_e = 0;
          channel_cfg.pad_e = 1;
          if (cache_version < 0x3)
            channel_cfg.rfc_e = 0;
          else
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = (pic_start_chroma_bus +
                                    (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * chroma_stride +
                                    (tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 2*8) : 0)) * 4 * pixel_width / 8);
#ifdef ASIC_TRACE_SUPPORT
          tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
          tmp = tmp / 16 * 16;
          channel_cfg.base_offset = tmp;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          if (!tile_exit)
            channel_cfg.line_size = pic_width * 4 * pixel_width / 8;
          else
            channel_cfg.line_size = (tile_col_width  -
                                    (left_tile ? 2*8 : 0)  + (right_tile ? 2*8 : 0)) * 4 * pixel_width / 8;
          channel_cfg.line_stride = chroma_stride>>4;
          if (!tile_exit)
            channel_cfg.line_cnt = pic_height / 8;
          else {
            if (tile_row_height / 2 < (top_tile ? 8 : 0))
              return;
            else
              channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0)
                                     + (bottom_tile ? 8 : 0)) / 4;
          }
          channel_cfg.max_h = (ctb_size / 2 + 8) / 4;
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[1] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
        }
      } else {
        /*luma*/
        channel_cfg.stripe_e = 0;
        channel_cfg.pad_e = 0;
        channel_cfg.rfc_e = 1;
        if (cache_version < 0x3) 
          channel_cfg.line_size = (pic_width * 8 * pixel_width) / 8;
        else
           channel_cfg.line_size = 0;
        if (!hw_feature.rfc_stride_support)
          channel_cfg.line_stride = ((((pic_width * 8 * pixel_width) / 8) + 15) & ~15) >> 4;
        else
          channel_cfg.line_stride = luma_stride >> 1; 
        if (cache_version < 0x3)
          channel_cfg.start_addr = pic_start_luma_bus +
                                   (tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 8 * channel_cfg.line_stride * 16;// +
                                   //(dec_cont->tile_width * ctb_size - (tile_exit ? (left_tile ? 0 : 8) : 0)) * 8;
        else
          channel_cfg.start_addr = pic_start_luma_bus;
#ifdef ASIC_TRACE_SUPPORT
        tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
        channel_cfg.base_offset = tmp;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (cache_version < 0x3) {
          if (!tile_exit)
            channel_cfg.line_cnt = pic_height / 8;
          else {
            if (tile_row_height < (top_tile ? 8 : 0))
              return;
            else
              channel_cfg.line_cnt = (tile_row_height - (top_tile ? 8 : 0) +
                                      (bottom_tile ? 8 : 0)) / 8;
          }
        } else
          channel_cfg.line_cnt = pic_height / 8; 
        channel_cfg.max_h = 9;
#ifdef ASIC_TRACE_SUPPORT
        shaper_flag[0] = 1;
#endif
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        /*if (!(!width16 && pixel_width == 10)) {*/
        if (!mono_chrome) {
          channel_cfg.stripe_e = 0;
          channel_cfg.pad_e = 0;
          channel_cfg.rfc_e = 1;
          if (cache_version < 0x3)
            channel_cfg.line_size = (pic_width * 4 * pixel_width) / 8;
          else
            channel_cfg.line_size = 0;
          if (!hw_feature.rfc_stride_support)
            channel_cfg.line_stride = ((((pic_width * 4 * pixel_width) / 8) + 15) & ~15) >> 4;
          else
            channel_cfg.line_stride = chroma_stride>> 1;
          if (cache_version < 0x3)
            channel_cfg.start_addr = (pic_start_chroma_bus +
                                      (tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16);
          else
            channel_cfg.start_addr = pic_start_chroma_bus;
#ifdef ASIC_TRACE_SUPPORT
          tmp = (u8*)channel_cfg.start_addr - dpb_base_address;
          channel_cfg.base_offset = tmp;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          if (cache_version < 0x3) {
            if (!tile_exit)
              channel_cfg.line_cnt = pic_height / 8;
            else {
              if (tile_row_height / 2 < (top_tile ? 8 : 0))
                return;
              else
                channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0) +
                                        (bottom_tile ? 8 : 0)) / 4;
            }
          } else
            channel_cfg.line_cnt = pic_height / 8;
          channel_cfg.max_h = 10;
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[1] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
        }
      }
    }
    /* pp channel configure*/
    if (pp_enabled) {
      PpUnitIntConfig *ppu_cfg_temp = ppu_cfg;
      addr_t ppu_out_bus_addr = 0;
      channel_cfg.pp_buffer = 1;
      if (cache_version < 0x3)
        channel_cfg.block_e = 1;
      u32 first = 0;
      u32 index[DEC_MAX_PPU_COUNT] = {325, 347, 364, 381, 452};
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (ppu_cfg->enabled && !first) {
          ppu_out_bus_addr = (addr_t)(DWLReadReg(instance, core_id, 4 * (index[i] + 1)) |
                             (((u64)DWLReadReg(instance, core_id, 4 * index[i])) << 32));
          if (ppu_cfg->rgb_planar && !IS_PACKED_RGB(ppu_cfg->rgb_format))
            ppu_out_bus_addr -= 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
          first = 1;
          if (field_pic_flag && !is_top_field)
            ppu_out_bus_addr -= ppu_cfg->ystride;
        }
        if (ppu_cfg->enabled && (ppu_cfg->rgb || ppu_cfg->rgb_planar))
          rgb_flag = 1;

        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled) continue;
      }
#ifdef ASIC_TRACE_SUPPORT
      ppu_cfg = ppu_cfg_temp;
      if (dec_mode == AVS2 && field_pic_flag) {
        pp_base_virtual = Avs2FieldPpBufOffset(instance, core_id, ppu_cfg, &sim_buf_offset);
        pp_base_address = (addr_t) pp_base_virtual;
      }
#endif
      if (rgb_flag) {
        max_lu_height = 2 * (32 + 8);
        max_ch_height = 32 + 8;
      } else {
        max_lu_height = 64 + 8;
        max_ch_height = 32 + 8;
      }
      ppu_cfg = ppu_cfg_temp;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {

        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled || field_pic_flag) continue;
        if ((num_tile_cols > 1) && (cache_version < 0x3)) continue;
        if (field_pic_flag && !is_top_field) {
          pp_field_offset = ppu_cfg->ystride;
          pp_field_offset_ch = ppu_cfg->cstride;
        } else {
          pp_field_offset = pp_field_offset_ch = 0;
        }
        u32 scaled_y_height, scaled_c_height;
        if (ppu_cfg->crop.height < ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL((double)max_lu_height * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1))); 
          scaled_c_height = (int)(CEIL((double)max_ch_height * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1)));
        } else if (ppu_cfg->crop.height > ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL(max_lu_height * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536))));
          scaled_c_height = (int)(CEIL(max_ch_height * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536))));
          if (scaled_y_height > max_lu_height)
            scaled_y_height = max_lu_height;
          if (scaled_c_height > max_ch_height)
            scaled_c_height = max_ch_height;
        } else {
          scaled_y_height = max_lu_height;
          scaled_c_height = max_ch_height;
        }
        /*luma*/
        if (num_tile_cols == 1)
          channel_cfg.pad_e = 1;
        else
          channel_cfg.pad_e = 0;
        channel_cfg.rfc_e = 0;
	channel_cfg.ppu_index = i;
	if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
          if (hw_feature.rgb_line_stride_support)
            channel_cfg.pad_e = 1;
          else
            channel_cfg.pad_e = 0;
	  channel_cfg.ppu_sub_index = 0;
	  channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = ppu_cfg->luma_offset + pp_field_offset;
#endif
	  channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = (ppu_cfg->rgb_planar ? 1 : 3) * ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
	  channel_cfg.line_stride =  ppu_cfg->ystride >> 4;
          channel_cfg.line_cnt = ppu_cfg->scale.height;
          channel_cfg.max_h = scaled_y_height;
#ifdef ASIC_TRACE_SUPPORT
	  shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
	  if (ppu_cfg->rgb_planar) {
	    channel_cfg.ppu_sub_index = 1;
	    channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
            channel_cfg.line_stride = ppu_cfg->ystride >> 4;
	    channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset + NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
	    channel_cfg.base_offset = ppu_cfg->luma_offset + pp_field_offset + NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
	    channel_cfg.max_h = scaled_y_height; 
	    DWLEnableCacheChannel(&channel_cfg);
	    channel_cfg.ppu_sub_index = 2;
	    channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
	    channel_cfg.line_stride = ppu_cfg->ystride >> 4;
	    channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
	    channel_cfg.base_offset = ppu_cfg->luma_offset + pp_field_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
            channel_cfg.max_h = scaled_y_height;
	    DWLEnableCacheChannel(&channel_cfg);
	  }
        } else {
          channel_cfg.ppu_sub_index = 0;
          channel_cfg.line_stride = ppu_cfg->tiled_e ? NEXT_MULTIPLE(ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) >> 7
                                                     : NEXT_MULTIPLE(((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4);
          channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset /* +
                                   (dec_cont->tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#ifdef ASIC_TRACE_SUPPORT
          if (dec_mode == AVS2 && field_pic_flag) {
            channel_cfg.base_offset =  sim_buf_offset + ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset - pp_base_address;
          } else 
            channel_cfg.base_offset = ppu_cfg->luma_offset + pp_field_offset/*+
                                    (dec_cont->tile_height * ctb_size - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          if (cache_version < 0x3) {
            if (!tile_exit)
              channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                       : (((ppu_cfg->scale.width * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
            else
              channel_cfg.line_size = ppu_cfg->tiled_e ? (tile_col_width - (left_tile ? 8 : 0) + (right_tile ? 8 : 0)) * 4 * ppu_cfg->pixel_width / 8
                                                       : (tile_col_width - (left_tile ? 8 : 0) + (right_tile ? 8 : 0)) * ppu_cfg->pixel_width / 8;
          } else {
            channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                     : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
          }
          if (cache_version < 0x3) {
            if (!tile_exit)
              channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height, 4)) / 4 : ppu_cfg->scale.height / (field_pic_flag ? 2 : 1);
            else {
              if (tile_row_height < (top_tile ? 8 : 0))
                return;
              else
                channel_cfg.line_cnt = (tile_row_height - (top_tile ? 8 : 0) + (bottom_tile ? 8 : 0)) / (ppu_cfg->tiled_e ? 4 : 1);
            }
          } else {
            channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height) / (field_pic_flag ? 2 : 1);
          }
          channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_y_height + 3 + 2) / 4 : scaled_y_height; 
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
          if (!ppu_cfg->monochrome) {
            /*chroma*/
            if (num_tile_cols == 1)
              channel_cfg.pad_e = 1;
            else
              channel_cfg.pad_e = 0;
            channel_cfg.rfc_e = 0;
	    channel_cfg.ppu_sub_index = 1;
            channel_cfg.line_stride = ppu_cfg->tiled_e ? NEXT_MULTIPLE(ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) >> 7
                                                       : (ppu_cfg->planar ? NEXT_MULTIPLE((((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4)
                                                       : NEXT_MULTIPLE(((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4));
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset + pp_field_offset_ch/*+
                                   (dec_cont->tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#ifdef ASIC_TRACE_SUPPORT
            if (dec_mode == AVS2 && field_pic_flag) {
              channel_cfg.base_offset =  sim_buf_offset + ppu_out_bus_addr + ppu_cfg->chroma_offset + pp_field_offset_ch - pp_base_address;
            }else
              channel_cfg.base_offset = ppu_cfg->chroma_offset + pp_field_offset_ch/*+
                                    (dec_cont->tile_height * ctb_size / 2 - (tile_exit ? (top_tile ? 0 : 8) : 0)) / 4 * channel_cfg.line_stride * 16*/;
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            if (cache_version < 0x3) {
              if (!tile_exit)
                channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                         : (ppu_cfg->planar ? (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15
                                                         : (((ppu_cfg->scale.width * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15);
              else
                channel_cfg.line_size = ppu_cfg->tiled_e ? (tile_col_width - (left_tile ? 2*8 : 0) + (right_tile ? 2*8 : 0)) * 4 * ppu_cfg->pixel_width / 8
                                                         : (ppu_cfg->planar ? (tile_col_width - (left_tile ? 2*8 : 0) + (right_tile ? 2*8 : 0)) * ppu_cfg->pixel_width / 8
                                                         : (tile_col_width / 2 - (left_tile ? 8 : 0) + (right_tile ? 8 : 0)) * ppu_cfg->pixel_width / 8);
            } else {
              channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                       : (ppu_cfg->planar ? (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15
                                                       : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15);
            }
            if (cache_version < 0x3) {
              if (!tile_exit)
                channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : ppu_cfg->scale.height/(field_pic_flag ? 4 : 2);
              else {
                if (tile_row_height / 2 < (top_tile ? 8 : 0))
                  return;
                else
                  channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0) + (bottom_tile ? 8 : 0)) / (ppu_cfg->tiled_e ? 4 : 1);
              }
            } else {
              channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height)/(field_pic_flag ? 4 : 2);
            }
            channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height;
#ifdef ASIC_TRACE_SUPPORT
            shaper_flag[2*i+3] = 1;
#endif
            DWLEnableCacheChannel(&channel_cfg);
            if (ppu_cfg->planar) {
	      channel_cfg.ppu_sub_index = 2;
              channel_cfg.line_stride = NEXT_MULTIPLE((((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4);
              channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset + pp_field_offset_ch + ppu_cfg->cstride * ppu_cfg->scale.height/2;
#ifdef ASIC_TRACE_SUPPORT
              if (dec_mode == AVS2 && field_pic_flag) {
                channel_cfg.base_offset =  sim_buf_offset + ppu_out_bus_addr + ppu_cfg->chroma_offset + pp_field_offset_ch + ppu_cfg->cstride * ppu_cfg->scale.height/2 - pp_base_address;
              } else
                channel_cfg.base_offset = ppu_cfg->chroma_offset + pp_field_offset_ch + ppu_cfg->cstride * ppu_cfg->scale.height/2;
#endif
              channel_cfg.start_addr = channel_cfg.start_addr >> 4;
              if (cache_version < 0x3) {
                if (!tile_exit)
                  channel_cfg.line_size = (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
                else
                  channel_cfg.line_size = (tile_col_width - (left_tile ? 2*8 : 0) + (right_tile ? 2*8 : 0)) * ppu_cfg->pixel_width / 8;
              } else {
                channel_cfg.line_size = (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
              }
              if (cache_version < 0x3) {
                if (!tile_exit)
                  channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : ppu_cfg->scale.height/ (field_pic_flag ? 4 : 2);
                else {
                  if (tile_row_height / 2 < (top_tile ? 8 : 0))
                    return;
                  else
                    channel_cfg.line_cnt = (tile_row_height / 2 - (top_tile ? 8 : 0) + (bottom_tile ? 8 : 0)) / (ppu_cfg->tiled_e ? 4 : 1);
                }
              } else {
                channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : ppu_cfg->scale.height/(field_pic_flag ? 4 : 2);
              }
              channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height; 
              DWLEnableCacheChannel(&channel_cfg);
	    }
          }
        }
      }
    }
    /*enable work channel*/
    if (!vcmd_used)
      DWLEnableCacheWork(core_id);
  } else {
    if (!hw_feature.pic_size_reg_unified) {
      if (dec_mode != JPEG || dec_mode != VP8) {
        pic_width_in_mbs = (DWLReadReg(instance, core_id, 4 * 4)) >> 23;
        pic_height_in_mbs = ((DWLReadReg(instance, core_id, 4 * 4)) >> 11) & 0xFF;
        if (dec_mode == H264 || dec_mode == AVS) {
          pic_height_in_mbs |= (((DWLReadReg(instance, core_id, 4 * 7) >> 25) & 0x01) << 8);
        } else if (dec_mode == H264_HIGH10) {
          pic_height_in_mbs = pic_height_in_mbs;
        } else if (dec_mode == VC1) {
          pic_height_in_mbs |= (((DWLReadReg(instance, core_id, 4 * 7) >> 13) & 0x01) << 8);
        } else {
          pic_height_in_mbs |= ((DWLReadReg(instance, core_id, 4 * 4) & 0x01) << 8);
        }
      } else {
        pic_width_in_mbs = ((((DWLReadReg(instance, core_id, 4 * 4)) >> 3) & 0x07) << 9);
        pic_width_in_mbs |= ((DWLReadReg(instance, core_id, 4 * 4)) >> 23);
        pic_height_in_mbs = ((DWLReadReg(instance, core_id, 4 * 4)) & 0x07);
        pic_height_in_mbs|= (((DWLReadReg(instance, core_id, 4 * 4)) >> 11) & 0xFF);
      }
    } else {
      pic_width_in_mbs = (((DWLReadReg(instance, core_id, 4 * 4)) >> 19) + 1) >> 1;
      pic_height_in_mbs = ((((DWLReadReg(instance, core_id, 4 * 4)) >> 6) & 0x1FFF) + 1) >> 1;
    }
    mono_chrome = ((dec_mode == H264 || dec_mode == H264_HIGH10) && (((DWLReadReg(instance, core_id, 4 * 7)) >> 30) & 0x01));
    u32 dpb_ilace_mode = ((DWLReadReg(instance, core_id, 4 * 65)) >> 1) & 0x01;
    u32 luma_stride = ((DWLReadReg(instance, core_id, 4 * 314)) >> 16) & 0xFFFF;
    u32 chroma_stride = (DWLReadReg(instance, core_id, 4 * 314)) & 0xFFFF;
    field_pic_flag = ((DWLReadReg(instance, core_id, 4 * 3)) >> 22) & 0x01;
    if (dec_mode == JPEG)
      field_pic_flag = 0;
    bottom_flag = ((((DWLReadReg(instance, core_id, 4 * 3)) >> 19) & 0x01) == 0);
    if (dec_mode == H264 || dec_mode == H264_HIGH10) {
      frame_mbs_only_flag = ((DWLReadReg(instance, core_id, 4 * 5) & 0x01) == 0);
      mb_adaptive_frame_field_flag = ((DWLReadReg(instance, core_id, 4 * 3)) >> 10) & 0x01;
    } else if (dec_mode == JPEG || dec_mode == VP6 || dec_mode == VP8) {
      frame_mbs_only_flag = 1;
    } else {
      if (field_pic_flag == 0)
        frame_mbs_only_flag = 1;
    }
    pic_start_luma_bus = ((addr_t)(DWLReadReg(instance, core_id, 4 * 65) |
                         (((u64)DWLReadReg(instance, core_id, 4 * 64)) << 32))) & (~0x3);
    if (dec_mode == H264_HIGH10)
      pic_start_chroma_bus = ((addr_t)(DWLReadReg(instance, core_id, 4 * 99) |
                             (((u64)DWLReadReg(instance, core_id, 4 * 98)) << 32))) & (~0x3);
    stride = ((DWLReadReg(instance, core_id, 4 * 314)) >> 16);
    if (dec_mode == H264_HIGH10) {
      bit_depth_luma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 6) & 0x03) + 8;
      bit_depth_chroma = (((DWLReadReg(instance, core_id, 4 * 8)) >> 4) & 0x03) + 8;
      pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ? 8 : 10;
      use_video_compressor = ((((DWLReadReg(instance, core_id, 4 * 3)) >> 8) & 0x01) == 0);
    } else
      pixel_width = 8;
    line_cnt = pic_height_in_mbs * 4;
    channel_cfg.mode = WR;
    channel_cfg.stripe_e = 0;
    if (cache_version < 0x3) 
      channel_cfg.block_e = 0;
    channel_cfg.pp_buffer = 0;
    channel_cfg.first_tile = 1;
    if (dec_mode == H264 || dec_mode == H264_HIGH10) {
      if (frame_mbs_only_flag) {
        /*luma*/
        if (!use_video_compressor) {
          channel_cfg.pad_e = 1;
        } else {
          channel_cfg.pad_e = 0;
        }
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = pic_start_luma_bus;
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        if (!use_video_compressor) {
          channel_cfg.line_size = pic_width_in_mbs * 64 * pixel_width / 8;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs * 4;
        } else {
          channel_cfg.line_size = 0;
          if (!hw_feature.rfc_stride_support)
            channel_cfg.line_stride = pic_width_in_mbs * 8 * pixel_width / 8;
          else
            channel_cfg.line_stride = luma_stride >> 1;
          channel_cfg.line_cnt = pic_height_in_mbs * 2;
        }
        channel_cfg.max_h = (16 + ((dec_mode == H264_HIGH10) ? 8 : 4)) / 4;
#ifdef ASIC_TRACE_SUPPORT
        shaper_flag[0] = 1;
#endif
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        if (!mono_chrome) {
          if (!use_video_compressor) {
            channel_cfg.pad_e = 1;
          } else {
            channel_cfg.pad_e = 0;
          }
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          if (dec_mode == H264_HIGH10)
            channel_cfg.start_addr = pic_start_chroma_bus;
          else
            channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt);
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          if (!use_video_compressor) {
            channel_cfg.line_size = pic_width_in_mbs * 64 * pixel_width / 8;
            channel_cfg.line_stride = stride>>4;
          } else {
            channel_cfg.line_size = 0;
            if (!hw_feature.rfc_stride_support)
              channel_cfg.line_stride = pic_width_in_mbs * 4 * pixel_width / 8;
            else
              channel_cfg.line_stride = chroma_stride >> 1;
          }
          channel_cfg.line_cnt = pic_height_in_mbs * 2;
          channel_cfg.max_h = (8 + ((dec_mode == H264_HIGH10) ? 8 : 4)) / 4;
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[1] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
        }
      } else {
        /*PICAFF or MBAFF*/
        if (!field_pic_flag) {
          /*top luma*/
          channel_cfg.pad_e = 1;
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = pic_start_luma_bus;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = pic_width_in_mbs * 64;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs * 2;
          if (mb_adaptive_frame_field_flag)
            channel_cfg.max_h = 20 / 4;
          else
            channel_cfg.max_h = 12 / 4;
          DWLEnableCacheChannel(&channel_cfg);
          /*bottom luma*/
          channel_cfg.pad_e = 1;
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt / 2);
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = pic_width_in_mbs * 64;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs * 2;
          if (mb_adaptive_frame_field_flag)
            channel_cfg.max_h = 20 / 4;
          else
            channel_cfg.max_h = 12 / 4;
          DWLEnableCacheChannel(&channel_cfg);
          /*top chroma*/
          if (!mono_chrome) {
            channel_cfg.pad_e = 1;
            if (cache_version >= 0x3)
              channel_cfg.rfc_e = 1;
            channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            channel_cfg.line_size = pic_width_in_mbs * 64;
            channel_cfg.line_stride = stride>>4;
            channel_cfg.line_cnt = pic_height_in_mbs;
            if (mb_adaptive_frame_field_flag)
              channel_cfg.max_h = 12 / 4;
            else
              channel_cfg.max_h = 8 / 4;
            DWLEnableCacheChannel(&channel_cfg);
            /*bottom chroma*/
            channel_cfg.pad_e = 1;
            if (cache_version >= 0x3)
              channel_cfg.rfc_e = 1;
            channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt * 5 / 4);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            channel_cfg.line_size = pic_width_in_mbs * 64;
            channel_cfg.line_stride = stride>>4;
            channel_cfg.line_cnt = pic_height_in_mbs;
            if (mb_adaptive_frame_field_flag)
              channel_cfg.max_h = 12 / 4;
            else
              channel_cfg.max_h = 8 / 4;
            DWLEnableCacheChannel(&channel_cfg);
          }
        } else {
          /*luma*/
          channel_cfg.pad_e = 1;
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt / 2 * (bottom_flag ? 1 : 0));
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = pic_width_in_mbs * 64;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs * 2;
          channel_cfg.max_h = (16 + 4) / 4;
          DWLEnableCacheChannel(&channel_cfg);
          /*chroma*/
          if (!mono_chrome) {
            channel_cfg.pad_e = 1;
            if (cache_version >= 0x3)
              channel_cfg.rfc_e = 1;
            channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt * (bottom_flag ? 5 : 4) / 4);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            channel_cfg.line_size = pic_width_in_mbs * 64;
            channel_cfg.line_stride = stride>>4;
            channel_cfg.line_cnt = pic_height_in_mbs;
            channel_cfg.max_h = (8 + 4) / 4;
            DWLEnableCacheChannel(&channel_cfg);
          }
        }
      }
    } else if (dec_mode == JPEG) {
     ;
    } else {
      if (!dpb_ilace_mode) {
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = pic_start_luma_bus;
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64 * pixel_width / 8;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs * 4;
        channel_cfg.max_h = (/*16*/20 + 4) / 4;
        DWLEnableCacheChannel(&channel_cfg);
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt);
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64 * pixel_width / 8;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs * 2;
        channel_cfg.max_h = (/*8*/12 + 4) / 4;
        DWLEnableCacheChannel(&channel_cfg);
      } else if (dpb_ilace_mode && !field_pic_flag){
        /*top luma*/
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = pic_start_luma_bus;
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs * 2;
        channel_cfg.max_h = 12 / 4;
        DWLEnableCacheChannel(&channel_cfg);
        /*bottom luma*/
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt / 2);
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs * 2;
        channel_cfg.max_h = 12 / 4;
        DWLEnableCacheChannel(&channel_cfg);
        /*top chroma*/
        if (!mono_chrome) {
          channel_cfg.pad_e = 1;
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt);
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = pic_width_in_mbs * 64;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs;
          channel_cfg.max_h = 8 / 4;
          DWLEnableCacheChannel(&channel_cfg);
          /*bottom chroma*/
          channel_cfg.pad_e = 1;
          if (cache_version >= 0x3)
            channel_cfg.rfc_e = 1;
          channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt * 5 / 4);
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
          channel_cfg.line_size = pic_width_in_mbs * 64;
          channel_cfg.line_stride = stride>>4;
          channel_cfg.line_cnt = pic_height_in_mbs;
          channel_cfg.max_h = 8 / 4;
          DWLEnableCacheChannel(&channel_cfg);
        }
      } else {
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt / 2 * (bottom_flag ? 1 : 0));
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs * 2;
        channel_cfg.max_h = (/*16*/20 + 4) / 4;
        DWLEnableCacheChannel(&channel_cfg);
        /*chroma*/
        channel_cfg.pad_e = 1;
        if (cache_version >= 0x3)
          channel_cfg.rfc_e = 1;
        channel_cfg.start_addr = (pic_start_luma_bus + stride * line_cnt * (bottom_flag ? 5 : 4) / 4);
#ifdef ASIC_TRACE_SUPPORT
        channel_cfg.base_offset = (u8*)(channel_cfg.start_addr) - dpb_base_address;
#endif
        channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        channel_cfg.line_size = pic_width_in_mbs * 64;
        channel_cfg.line_stride = stride>>4;
        channel_cfg.line_cnt = pic_height_in_mbs;
        channel_cfg.max_h = (/*8*/12 + 4) / 4;
        DWLEnableCacheChannel(&channel_cfg);
      }
    }
    /* pp channel configure*/
    if (pp_enabled) {
      PpUnitIntConfig *ppu_cfg_temp = ppu_cfg;
      addr_t ppu_out_bus_addr = 0;
      channel_cfg.pp_buffer = 1;
      if (cache_version < 0x3)
        channel_cfg.block_e = 1;
      if (cache_version >= 0x3)
        channel_cfg.rfc_e = 0;
      u32 first = 0;
      u32 pp_height, pp_stride, pp_buff_size, mvc_offset = 0;
      u32 mvc_enable = (DWLReadReg(instance, core_id, 4 * 3) >> 13) & 0x01;
      u32 jpeg_mode = (((DWLReadReg(instance, core_id, 4 * 5)) >> 8) & 0x07); 
      if (dec_mode != H264 && dec_mode != H264_HIGH10)
        mvc_enable = 0;
      u32 index[DEC_MAX_PPU_COUNT] = {325, 347, 364, 381, 452};
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (ppu_cfg->enabled && !first) {
          ppu_out_bus_addr = (addr_t)(DWLReadReg(instance, core_id, 4 * (index[i] + 1)) |
                             (((u64)DWLReadReg(instance, core_id, 4 * index[i])) << 32));
          if (ppu_cfg->rgb_planar && !IS_PACKED_RGB(ppu_cfg->rgb_format))
            ppu_out_bus_addr -= 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
          first = 1;
        }
        if (ppu_cfg->enabled && (ppu_cfg->rgb || ppu_cfg->rgb_planar))
          rgb_flag = 1;
        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled) continue;
      }
      if (mvc_enable) {
        ppu_cfg = ppu_cfg_temp;
        for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
          if (!ppu_cfg->enabled) continue;
          if (ppu_cfg->tiled_e)
            pp_height = NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
          else
            pp_height = ppu_cfg->scale.height;
          pp_stride = ppu_cfg->ystride;
          pp_buff_size = pp_stride * pp_height;
          /* chroma */
          if (!ppu_cfg->monochrome && !mono_chrome) {
            if (ppu_cfg->tiled_e)
              pp_height = NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4) / 4;
            else {
              if (ppu_cfg->planar)
                pp_height = ppu_cfg->scale.height;
              else
                pp_height = ppu_cfg->scale.height / 2;
            }
            pp_stride = ppu_cfg->cstride;
            pp_buff_size += pp_stride * pp_height;
          }
          mvc_offset += pp_buff_size;
        }
        mvc_offset = NEXT_MULTIPLE(mvc_offset, 16);
      }
#ifdef ASIC_TRACE_SUPPORT
    if (dec_mode == JPEG && !tb_cfg.cache_enable && tb_cfg.dec_params.cache_support) {
      ppu_cfg = ppu_cfg_temp;
      u32 flag = 0;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled) continue;
        flag = 1;
      }
      if (!flag)
        tb_cfg.dec_params.cache_support = 0;
    }
#endif
      ppu_cfg = ppu_cfg_temp;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {

        if (!ppu_cfg->enabled || !ppu_cfg->shaper_enabled || !frame_mbs_only_flag) continue;
        u32 scaled_y_height, scaled_c_height;
        if (dec_mode == H264_HIGH10) {
          if (rgb_flag) {
            max_lu_height = 2 * (8 + 8);
            max_ch_height = 8 + 8;
          } else {
            max_lu_height = 16 + 8;
            max_ch_height = 8 + 8;
          }
        } else if (dec_mode == JPEG) {
          if (jpeg_mode == JPEGDEC_YUV400) {
            max_lu_height = 8;
            max_ch_height = 4;
          } else if (jpeg_mode == JPEGDEC_YUV420) {
            max_lu_height = 16;
            max_ch_height = 8;
          } else if (jpeg_mode == JPEGDEC_YUV422) {
            max_lu_height = 8;
            max_ch_height = 4;
          } else if (jpeg_mode == JPEGDEC_YUV444) {
            max_lu_height = 8;
            max_ch_height = 4;
          } else if (jpeg_mode == JPEGDEC_YUV440) {
            max_lu_height = 16;
            max_ch_height = 8;
          } else if (jpeg_mode == JPEGDEC_YUV411) {
            max_lu_height = 32;
            max_ch_height = 16;
          }
        } else {
          max_lu_height = 16 + 8;
          max_ch_height = 8 + 4;
        }
        if (ppu_cfg->crop.height < ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL((double)max_lu_height * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1))) + 1;
          scaled_c_height = (int)(CEIL((double)max_ch_height * (ppu_cfg->scale.height - 1) / (ppu_cfg->crop.height - 1))) + 1;
        } else if (ppu_cfg->crop.height > ppu_cfg->scale.height) {
          scaled_y_height = (int)(CEIL(max_lu_height * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536)))) + 1;
          scaled_c_height = (int)(CEIL(max_ch_height * ((double)ppu_cfg->scale.height / ppu_cfg->crop.height + (1.0 / 65536)))) + 1;
          if (scaled_y_height > max_lu_height)
            scaled_y_height = max_lu_height;
          if (scaled_c_height > max_ch_height)
            scaled_c_height = max_ch_height;
        } else {
          scaled_y_height = max_lu_height;
          scaled_c_height = max_ch_height;
        }
        /*luma*/
	channel_cfg.ppu_index = i;
        channel_cfg.pad_e = 1;
	if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
          if (hw_feature.rgb_line_stride_support)
            channel_cfg.pad_e = 1;
          else
            channel_cfg.pad_e = 0;
          channel_cfg.ppu_sub_index = 0;
	  channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = ppu_cfg->luma_offset + (mvc_enable ? mvc_offset : 0);
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	  channel_cfg.line_size = (ppu_cfg->rgb_planar ? 1 : 3) * ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
	  channel_cfg.line_stride = ppu_cfg->ystride >> 4;
	  channel_cfg.line_cnt = ppu_cfg->scale.height;
          channel_cfg.max_h = scaled_y_height * (mb_adaptive_frame_field_flag ? 2 : 1);
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
	  if (ppu_cfg->rgb_planar) {
            channel_cfg.ppu_sub_index = 1;
	    channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
	    channel_cfg.line_stride = ppu_cfg->ystride >> 4;
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->luma_offset + (mvc_enable ? mvc_offset : 0) +  NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
            channel_cfg.max_h = scaled_y_height * (mb_adaptive_frame_field_flag ? 2 : 1);
            DWLEnableCacheChannel(&channel_cfg);
            channel_cfg.ppu_sub_index = 2;
            channel_cfg.line_size = ppu_cfg->scale.width * ppu_cfg->pixel_width / 8;
            channel_cfg.line_stride = ppu_cfg->ystride >> 4;
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->luma_offset + 2 * NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
#endif
	    channel_cfg.start_addr = channel_cfg.start_addr >> 4;
	    channel_cfg.line_cnt = ppu_cfg->scale.height;
            channel_cfg.max_h = scaled_y_height * (mb_adaptive_frame_field_flag ? 2 : 1);
            DWLEnableCacheChannel(&channel_cfg);
          }
        } else {
          channel_cfg.ppu_sub_index = 0;
          channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->luma_offset;
#ifdef ASIC_TRACE_SUPPORT
          channel_cfg.base_offset = ppu_cfg->luma_offset + (mvc_enable ? mvc_offset : 0);
#endif
          channel_cfg.start_addr = channel_cfg.start_addr >> 4;
        //channel_cfg.line_size = ppu_cfg->tiled_e ? 4 * NEXT_MULTIPLE(ppu_cfg->scale.width, 4) : NEXT_MULTIPLE(ppu_cfg->scale.width, 16);
          channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                   : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width : ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
          channel_cfg.line_stride = NEXT_MULTIPLE(channel_cfg.line_size, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4);
          channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height) / (field_pic_flag ? 2 : 1);
          channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_y_height + 3 + 2) / 4 : scaled_y_height * (mb_adaptive_frame_field_flag ? 2 : 1);
#ifdef ASIC_TRACE_SUPPORT
          shaper_flag[2*i+2] = 1;
#endif
          DWLEnableCacheChannel(&channel_cfg);
          /*chroma*/
          if (!(mono_chrome || ppu_cfg->monochrome)) {
            channel_cfg.pad_e = 1;
	    channel_cfg.ppu_sub_index = 1;
            channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset;
#ifdef ASIC_TRACE_SUPPORT
            channel_cfg.base_offset = ppu_cfg->chroma_offset + (mvc_enable ? mvc_offset : 0);
#endif
            channel_cfg.start_addr = channel_cfg.start_addr >> 4;
            //channel_cfg.line_size = ppu_cfg->tiled_e ? 4 * NEXT_MULTIPLE(ppu_cfg->scale.width, 4) : 
            //                        (ppu_cfg->planar ? NEXT_MULTIPLE(ppu_cfg->scale.width / 2, 16) : NEXT_MULTIPLE(ppu_cfg->scale.width, 16));
            channel_cfg.line_size = ppu_cfg->tiled_e ? ppu_cfg->scale.width * 4 * ppu_cfg->pixel_width / 8
                                                     : (ppu_cfg->planar ? (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15
                                                     : ((((ppu_cfg->crop2.enabled ? ppu_cfg->crop2.width :ppu_cfg->scale.width) * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15);
            channel_cfg.line_stride = NEXT_MULTIPLE(channel_cfg.line_size, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4);
            channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : (ppu_cfg->crop2.enabled ? ppu_cfg->crop2.height : ppu_cfg->scale.height) / (field_pic_flag ? 4 : 2);
            channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height * (mb_adaptive_frame_field_flag ? 2 : 1);
#ifdef ASIC_TRACE_SUPPORT
            shaper_flag[2*i+3] = 1;
#endif
           DWLEnableCacheChannel(&channel_cfg);
            if (ppu_cfg->planar) {
	      channel_cfg.ppu_sub_index = 2;
              channel_cfg.start_addr = ppu_out_bus_addr + ppu_cfg->chroma_offset + ppu_cfg->cstride * ppu_cfg->scale.height / 2;
#ifdef ASIC_TRACE_SUPPORT
              channel_cfg.base_offset = ppu_cfg->chroma_offset + ppu_cfg->cstride * ppu_cfg->scale.height / 2 + (mvc_enable ? mvc_offset : 0);
#endif
              channel_cfg.start_addr = channel_cfg.start_addr >> 4;
              //channel_cfg.line_size = ppu_cfg->tiled_e ? 4 * NEXT_MULTIPLE(ppu_cfg->scale.width, 4) :
              //                        (ppu_cfg->planar ? NEXT_MULTIPLE(ppu_cfg->scale.width / 2, 16) : NEXT_MULTIPLE(ppu_cfg->scale.width, 16));
              channel_cfg.line_size = (((ppu_cfg->scale.width / 2 * ppu_cfg->pixel_width + 7) & ~7) / 8 + 15) & ~15;
              channel_cfg.line_stride = NEXT_MULTIPLE(channel_cfg.line_size, ALIGN(ppu_cfg->align)) >> (field_pic_flag ? 3 : 4);
              channel_cfg.line_cnt = ppu_cfg->tiled_e ? (NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4)) / 4 : ppu_cfg->scale.height / (field_pic_flag ? 4 : 2);
              channel_cfg.max_h = ppu_cfg->tiled_e ? (scaled_c_height + 3 + 3) / 4 : scaled_c_height * (mb_adaptive_frame_field_flag ? 2 : 1);
              DWLEnableCacheChannel(&channel_cfg);
	    }
          }
        }
      }
    }
    /*enable work channel*/
    if (!vcmd_used)
      DWLEnableCacheWork(core_id);
  }
}
