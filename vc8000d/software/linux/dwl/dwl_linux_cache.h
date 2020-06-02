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
#ifndef DWL_CACHE
#define DWL_CACHE
#include "stdio.h"
#include "cacheapi.h"
enum cache_mode {
  RD = 0,
  WR = 1,
  BI = 2
};

struct ChannelConf {
 enum cache_mode mode;
 u32 decoder_type;/*0 for H264 and 1 for HEVC*/
 u32 dec_mode;
 u64 base_offset;
 u64 mc_offset[16];
 u64 tb_offset;
 u32 no_chroma;
 i32 core_id;
 u32 shaper_bypass;
 u32 cache_enable;
 u32 shaper_enable;
 u32 first_tile;
  /*both for cache read and write*/
 addr_t start_addr;//equal to physical addr>>4 and the start_addr<<4 must be aligned to 1<< LOG2_ALIGNMENT which is fixed in HW
 /*only for cache read*/
 u32 prefetch_enable;
 u32 prefetch_threshold;
 u32 shift_h;
 u32 range_h;
 u32 cache_all;
 addr_t end_addr;
 /*only for cache write*/
 u32 line_size;//the valid width of the input frame/reference frame
 u32 line_stride;//equal to (width aligned to 1<< LOG2_ALIGNMENT)>>4
 u32 line_cnt;//the valid height of the input frame/reference frame
 u32 stripe_e;
 u32 pad_e;
 u32 rfc_e;
 u32 block_e;
 u32 max_h;
 u32 ln_cnt_start;
 u32 ln_cnt_mid;
 u32 ln_cnt_end;
 u32 ln_cnt_step;
 u32 tile_id;
 u32 tile_num;
 u32 tile_by_tile;
 u32 width;
 u32 height;
 u32 num_tile_cols;
 u32 num_tile_rows;
 u32 tile_size[2048];
 FILE *file_fid;
 u32 hw_dec_pic_count;
 u32 stream_buffer_id;
 u32 hw_id;
 u32 cache_version;
 u32 pp_buffer;
 u32 trace_start_pic;
 u32 ppu_index;
 u32 ppu_sub_index;
};

void DWLEnableCacheChannel(struct ChannelConf *cfg);
void DWLEnableCacheWork(u32 core_id);
i32 DWLEnableCacheWorkDumpRegs(enum cache_mode mode, u32 core_id,
                               u32 *cache_regs, u32 *cache_reg_size,
                               u32 *shaper_regs, u32 *shaper_reg_size);
i32  DWLDisableCacheChannelALL(enum cache_mode mode, u32 core_id);
void DWLSetCacheExpAddr(addr_t start_addr, addr_t end_addr, u32 core_id);
void DWLPrintfInfo(struct ChannelConf *cfg, u32 core_id);
u32 DWLReadCacheVersion();
void DWLConfigureCacheChannel(const void *instance, i32 core_id, struct CacheParams *cache_cfg, u32 pjpeg_coeff_size_for_cache);
void DWLConfigureShaperChannel(const void *instance, i32 core_id, PpUnitIntConfig *ppu_cfg, u32 pp_enabled,
                               u32 tile_by_tile, u32 tile_id, u32 *num_tiles, u16 *tiles, u32* chroma_exit);
void DWLRefreshCacheRegs(u32 *cache_regs, u32 *cache_reg_num, u32 *shaper_regs, u32 *shaper_reg_num);
#endif
