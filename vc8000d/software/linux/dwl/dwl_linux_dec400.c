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
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"
#include "dwl_vcmd_common.h"
#include "dwl_linux_dec400.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif
#define DEC_MODE_JPEG 3
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2 16

extern u32 dwl_shadow_regs[MAX_ASIC_CORES][512];
/* 8KB for each command buffer */
/* FIXME(min): find a more flexible approach. */
#define VCMD_BUF_SIZE (8*1024)
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
i32 DWLConfigureCmdBufForDec400(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 instr_size;
  u32 core_id = 0;
  struct VcmdBuf *vcmd = &dwl_inst->vcmdb[cmd_buf_id];
  u32 i = 0;
  u64 y_addr = 0;
  u64 uv_addr = 0;
  u32 y_size = 0;
  u32 uv_size = 0;
  u32 mode = 0;
  u32 mono_chroma = 0;
  u32 w_offset = 0;
  u32 pic_interlace=0;
  u32 frame_mbs_only_flag = 0;
  u32 num_tile_column = 0;
  u32 reg_control[3]={0,0,0};
  u32 reg_config[8]={0,0,0,0,0,0,0,0};
  u32 reg_config_ex[8]={0,0,0,0,0,0,0,0};
  u32 reg_base[8]={0,0,0,0,0,0,0,0};
  u32 reg_base_ex[8]={0,0,0,0,0,0,0,0};
  u32 reg_base_end[8]={0,0,0,0,0,0,0,0};
  u32 reg_base_end_ex[8]={0,0,0,0,0,0,0,0};
  mode = (dwl_shadow_regs[core_id][3]>>27)&0x1F;

  mono_chroma = (dwl_shadow_regs[core_id][7]>>30)&0x01;
  if(mode == DEC_MODE_H264_H10P) {
    frame_mbs_only_flag = ((dwl_shadow_regs[core_id][5] & 0x01) == 0);
    pic_interlace = (dwl_shadow_regs[core_id][3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      printf("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return DWL_ERROR;
    }
  }
  if((mode == DEC_MODE_HEVC) || (mode == DEC_MODE_VP9)) {
    num_tile_column = (dwl_shadow_regs[core_id][10]>>17)&0x7F;
    if(num_tile_column > 1)
      return DWL_ERROR;
  }
  reg_control[0] = 0x00810000;
  reg_control[1] = 0x000A0000;
  reg_control[2] = 0x003FC81F;
  CWLCollectWriteRegData(&reg_control[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x200, /* dec400 */
                         3, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  reg_control[0] = 0xFFFFFFFF;
  CWLCollectWriteRegData(&reg_control[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x203, /* dec400 */
                         1, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_control[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x205, /* dec400 */
                         1, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  u32 pp_y_addr_reg[DEC_MAX_PPU_COUNT] = {326,348,365,382,453};
  u32 pp_c_addr_reg[DEC_MAX_PPU_COUNT] = {328,350,367,384,455};
  u32 pp_total_buffer_size=0;
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[i].enabled == 1){
      reg_addr_y = pp_y_addr_reg[i];
      reg_addr_c = pp_c_addr_reg[i];
      y_addr = (dwl_shadow_regs[core_id][reg_addr_y] | ((u64)dwl_shadow_regs[core_id][reg_addr_y - 1] << 32));
      uv_addr = (dwl_shadow_regs[core_id][reg_addr_c] | ((u64)dwl_shadow_regs[core_id][reg_addr_c - 1] << 32));

      if(dwl_inst->ppu_cfg[i].tiled_e == 1) {
        y_size = dwl_inst->ppu_cfg[i].ystride * NEXT_MULTIPLE(dwl_inst->ppu_cfg[i].scale.height,4)/4;
        uv_size = dwl_inst->ppu_cfg[i].cstride * NEXT_MULTIPLE((dwl_inst->ppu_cfg[i].scale.height/ 2),4)/4;
        if(pp_bus_address_start == 0)
          pp_bus_address_start = y_addr;
        if(dwl_inst->ppu_cfg[i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x0E020029;
          reg_config_ex[w_offset / 4] = 0<<16;
          if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x10020031;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x10020029;
          reg_config_ex[w_offset / 4] = 1<<16;
          if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x04020031;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if (dwl_inst->ppu_cfg[i].planar) {
        y_size = dwl_inst->ppu_cfg[i].ystride * dwl_inst->ppu_cfg[i].scale.height;
        uv_size = dwl_inst->ppu_cfg[i].cstride * dwl_inst->ppu_cfg[i].scale.height;
        if(pp_bus_address_start == 0)
          pp_bus_address_start = y_addr;
        if(dwl_inst->ppu_cfg[i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12020029;
          reg_config_ex[w_offset / 4] = 0<<16;
          if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x12020029;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14020029;
          reg_config_ex[w_offset / 4] = 1<<16;
          if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14020029;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      }
      reg_base[w_offset / 4] = (y_addr & 0xFFFFFFFF);
      reg_base_ex[w_offset / 4] = (y_addr >> 32);
      reg_base_end[w_offset / 4] = ((y_addr + y_size - 1) & 0xFFFFFFFF);
      reg_base_end_ex[w_offset / 4] = ((y_addr + y_size - 1) >> 32);
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
        reg_base[w_offset / 4 + 1] = (uv_addr & 0xFFFFFFFF);
        reg_base_ex[w_offset / 4 + 1] = (uv_addr  >> 32);
        reg_base_end[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) & 0xFFFFFFFF);
        reg_base_end_ex[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) >> 32);
      }
      w_offset += 0x8;

      pp_total_buffer_size += y_size;
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome))
        pp_total_buffer_size += uv_size;

      pp_enable_channel++;
    }
  }
  CWLCollectWriteRegData(&reg_config[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x260, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_config_ex[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x280, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_base[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x360, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_base_ex[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x380, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_base_end[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x3A0, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_base_end_ex[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x3C0, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  w_offset = 0;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[i].enabled == 1) {
      reg_base[w_offset / 4] = ((pp_bus_address_start + pp_total_buffer_size + DEC400_PPn_Y_TABLE_OFFSET(i)) & 0xFFFFFFFF);
      reg_base_ex[w_offset / 4] = ((pp_bus_address_start + pp_total_buffer_size + DEC400_PPn_Y_TABLE_OFFSET(i)) >> 32);
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
        reg_base[w_offset / 4 + 1] = ((pp_bus_address_start + pp_total_buffer_size + DEC400_PPn_UV_TABLE_OFFSET(i)) & 0xFFFFFFFF);
        reg_base_ex[w_offset / 4 + 1] = ((pp_bus_address_start + pp_total_buffer_size + DEC400_PPn_UV_TABLE_OFFSET(i)) >> 32);
      }
      w_offset += 0x8;
    }
  }
  CWLCollectWriteRegData(&reg_base[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x460, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  CWLCollectWriteRegData(&reg_base_ex[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x480, /* dec400 */
                         8, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  return DWL_OK;
}

void DWLFuseCmdBufForDec400(const void *instance, u32 cmd_buf_id, u32 index) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 instr_size;
  struct VcmdBuf *vcmd = &dwl_inst->vcmdb[cmd_buf_id];
  u32 reg_control[3]={0,0,0};
  u32 core_id = 0;
  u32 mode = (dwl_shadow_regs[core_id][3]>>27)&0x1F;

  if(mode == DEC_MODE_H264_H10P) {
    u32 frame_mbs_only_flag = ((dwl_shadow_regs[core_id][5] & 0x01) == 0);
    u32 pic_interlace = (dwl_shadow_regs[core_id][3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      printf("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return;
    }
  }
  if((mode == DEC_MODE_HEVC) || (mode == DEC_MODE_VP9)) {
    u32 num_tile_column = (dwl_shadow_regs[core_id][10]>>17)&0x7F;
    if(num_tile_column > 1)
      return;
  }
  reg_control[0] = 0x00810001;
  CWLCollectWriteRegData(&reg_control[0],
                         (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x200, /* flush dec400 */
                         1, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
  /* Wait interrupt from DEC400 */
  CWLCollectStallData((u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                      &instr_size,
                      (dwl_inst->vcmd_params.vcmd_hw_version_id == HW_ID_1_0_C ?
                       VC8000D_DEC400_INT_MASK_1_0_C : VC8000D_DEC400_INT_MASK));
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                        dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x206, 1, /* Acknowledge */
                        &instr_size,
                        vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2 + 4 * index);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                        dwl_inst->vcmd_params.submodule_dec400_addr/4 + 0x208, 1, /* AcknowledgeEx2 */
                        &instr_size,
                        vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2 + 4 * (index + 1));
  vcmd->cmd_buf_used += instr_size * 4;
}
