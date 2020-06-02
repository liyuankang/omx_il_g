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
#include "regdrv.h"
#include "hevc_asic.h"
#include "hevc_container.h"
#include "hevc_util.h"
#include "dwl.h"
#include "commonconfig.h"
#include "hevcdecmc_internals.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include <string.h>

static void HevcStreamPosUpdate(struct HevcDecContainer *dec_cont);
u32 no_chroma = 0;
#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...) \
  do {                     \
  } while (0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...) printf(__VA_ARGS__)
#endif

const u32 ref_ybase[16] = {
  HWIF_REFER0_YBASE_LSB,  HWIF_REFER1_YBASE_LSB,  HWIF_REFER2_YBASE_LSB,
  HWIF_REFER3_YBASE_LSB,  HWIF_REFER4_YBASE_LSB,  HWIF_REFER5_YBASE_LSB,
  HWIF_REFER6_YBASE_LSB,  HWIF_REFER7_YBASE_LSB,  HWIF_REFER8_YBASE_LSB,
  HWIF_REFER9_YBASE_LSB,  HWIF_REFER10_YBASE_LSB, HWIF_REFER11_YBASE_LSB,
  HWIF_REFER12_YBASE_LSB, HWIF_REFER13_YBASE_LSB, HWIF_REFER14_YBASE_LSB,
  HWIF_REFER15_YBASE_LSB
};

const u32 ref_cbase[16] = {
  HWIF_REFER0_CBASE_LSB,  HWIF_REFER1_CBASE_LSB,  HWIF_REFER2_CBASE_LSB,
  HWIF_REFER3_CBASE_LSB,  HWIF_REFER4_CBASE_LSB,  HWIF_REFER5_CBASE_LSB,
  HWIF_REFER6_CBASE_LSB,  HWIF_REFER7_CBASE_LSB,  HWIF_REFER8_CBASE_LSB,
  HWIF_REFER9_CBASE_LSB,  HWIF_REFER10_CBASE_LSB, HWIF_REFER11_CBASE_LSB,
  HWIF_REFER12_CBASE_LSB, HWIF_REFER13_CBASE_LSB, HWIF_REFER14_CBASE_LSB,
  HWIF_REFER15_CBASE_LSB
};

const u32 ref_dbase[16] = {
  HWIF_REFER0_DBASE_LSB,  HWIF_REFER1_DBASE_LSB,  HWIF_REFER2_DBASE_LSB,
  HWIF_REFER3_DBASE_LSB,  HWIF_REFER4_DBASE_LSB,  HWIF_REFER5_DBASE_LSB,
  HWIF_REFER6_DBASE_LSB,  HWIF_REFER7_DBASE_LSB,  HWIF_REFER8_DBASE_LSB,
  HWIF_REFER9_DBASE_LSB,  HWIF_REFER10_DBASE_LSB, HWIF_REFER11_DBASE_LSB,
  HWIF_REFER12_DBASE_LSB, HWIF_REFER13_DBASE_LSB, HWIF_REFER14_DBASE_LSB,
  HWIF_REFER15_DBASE_LSB
};
const u32 ref_tybase[16] = {
  HWIF_REFER0_TYBASE_LSB, HWIF_REFER1_TYBASE_LSB, HWIF_REFER2_TYBASE_LSB,
  HWIF_REFER3_TYBASE_LSB, HWIF_REFER4_TYBASE_LSB, HWIF_REFER5_TYBASE_LSB,
  HWIF_REFER6_TYBASE_LSB, HWIF_REFER7_TYBASE_LSB, HWIF_REFER8_TYBASE_LSB,
  HWIF_REFER9_TYBASE_LSB, HWIF_REFER10_TYBASE_LSB, HWIF_REFER11_TYBASE_LSB,
  HWIF_REFER12_TYBASE_LSB, HWIF_REFER13_TYBASE_LSB, HWIF_REFER14_TYBASE_LSB,
  HWIF_REFER15_TYBASE_LSB
};
const u32 ref_tcbase[16] = {
  HWIF_REFER0_TCBASE_LSB, HWIF_REFER1_TCBASE_LSB, HWIF_REFER2_TCBASE_LSB,
  HWIF_REFER3_TCBASE_LSB, HWIF_REFER4_TCBASE_LSB, HWIF_REFER5_TCBASE_LSB,
  HWIF_REFER6_TCBASE_LSB, HWIF_REFER7_TCBASE_LSB, HWIF_REFER8_TCBASE_LSB,
  HWIF_REFER9_TCBASE_LSB, HWIF_REFER10_TCBASE_LSB, HWIF_REFER11_TCBASE_LSB,
  HWIF_REFER12_TCBASE_LSB, HWIF_REFER13_TCBASE_LSB, HWIF_REFER14_TCBASE_LSB,
  HWIF_REFER15_TCBASE_LSB
};

const u32 ref_ybase_msb[16] = {
  HWIF_REFER0_YBASE_MSB,  HWIF_REFER1_YBASE_MSB,  HWIF_REFER2_YBASE_MSB,
  HWIF_REFER3_YBASE_MSB,  HWIF_REFER4_YBASE_MSB,  HWIF_REFER5_YBASE_MSB,
  HWIF_REFER6_YBASE_MSB,  HWIF_REFER7_YBASE_MSB,  HWIF_REFER8_YBASE_MSB,
  HWIF_REFER9_YBASE_MSB,  HWIF_REFER10_YBASE_MSB, HWIF_REFER11_YBASE_MSB,
  HWIF_REFER12_YBASE_MSB, HWIF_REFER13_YBASE_MSB, HWIF_REFER14_YBASE_MSB,
  HWIF_REFER15_YBASE_MSB
};

const u32 ref_cbase_msb[16] = {
  HWIF_REFER0_CBASE_MSB,  HWIF_REFER1_CBASE_MSB,  HWIF_REFER2_CBASE_MSB,
  HWIF_REFER3_CBASE_MSB,  HWIF_REFER4_CBASE_MSB,  HWIF_REFER5_CBASE_MSB,
  HWIF_REFER6_CBASE_MSB,  HWIF_REFER7_CBASE_MSB,  HWIF_REFER8_CBASE_MSB,
  HWIF_REFER9_CBASE_MSB,  HWIF_REFER10_CBASE_MSB, HWIF_REFER11_CBASE_MSB,
  HWIF_REFER12_CBASE_MSB, HWIF_REFER13_CBASE_MSB, HWIF_REFER14_CBASE_MSB,
  HWIF_REFER15_CBASE_MSB
};

const u32 ref_dbase_msb[16] = {
  HWIF_REFER0_DBASE_MSB,  HWIF_REFER1_DBASE_MSB,  HWIF_REFER2_DBASE_MSB,
  HWIF_REFER3_DBASE_MSB,  HWIF_REFER4_DBASE_MSB,  HWIF_REFER5_DBASE_MSB,
  HWIF_REFER6_DBASE_MSB,  HWIF_REFER7_DBASE_MSB,  HWIF_REFER8_DBASE_MSB,
  HWIF_REFER9_DBASE_MSB,  HWIF_REFER10_DBASE_MSB, HWIF_REFER11_DBASE_MSB,
  HWIF_REFER12_DBASE_MSB, HWIF_REFER13_DBASE_MSB, HWIF_REFER14_DBASE_MSB,
  HWIF_REFER15_DBASE_MSB
};
const u32 ref_tybase_msb[16] = {
  HWIF_REFER0_TYBASE_MSB, HWIF_REFER1_TYBASE_MSB, HWIF_REFER2_TYBASE_MSB,
  HWIF_REFER3_TYBASE_MSB, HWIF_REFER4_TYBASE_MSB, HWIF_REFER5_TYBASE_MSB,
  HWIF_REFER6_TYBASE_MSB, HWIF_REFER7_TYBASE_MSB, HWIF_REFER8_TYBASE_MSB,
  HWIF_REFER9_TYBASE_MSB, HWIF_REFER10_TYBASE_MSB, HWIF_REFER11_TYBASE_MSB,
  HWIF_REFER12_TYBASE_MSB, HWIF_REFER13_TYBASE_MSB, HWIF_REFER14_TYBASE_MSB,
  HWIF_REFER15_TYBASE_MSB
};
const u32 ref_tcbase_msb[16] = {
  HWIF_REFER0_TCBASE_MSB, HWIF_REFER1_TCBASE_MSB, HWIF_REFER2_TCBASE_MSB,
  HWIF_REFER3_TCBASE_MSB, HWIF_REFER4_TCBASE_MSB, HWIF_REFER5_TCBASE_MSB,
  HWIF_REFER6_TCBASE_MSB, HWIF_REFER7_TCBASE_MSB, HWIF_REFER8_TCBASE_MSB,
  HWIF_REFER9_TCBASE_MSB, HWIF_REFER10_TCBASE_MSB, HWIF_REFER11_TCBASE_MSB,
  HWIF_REFER12_TCBASE_MSB, HWIF_REFER13_TCBASE_MSB, HWIF_REFER14_TCBASE_MSB,
  HWIF_REFER15_TCBASE_MSB
};
/*------------------------------------------------------------------------------
    Reference list initialization
------------------------------------------------------------------------------*/
#define IS_SHORT_TERM_FRAME(a) ((a).status == SHORT_TERM)
#define IS_LONG_TERM_FRAME(a) ((a).status == LONG_TERM)
#define IS_REF_FRAME(a) ((a).status &&(a).status != EMPTY)

#define INVALID_IDX 0xFFFFFFFF
#define MIN_POC 0x80000000
#define MAX_POC 0x7FFFFFFF
#define MAX3(A,B,C) ((A)>(B)&&(A)>(C)?(A):((B)>(C)?(B):(C)))

const u32 ref_pic_list0[16] = {
  HWIF_INIT_RLIST_F0,  HWIF_INIT_RLIST_F1,  HWIF_INIT_RLIST_F2,
  HWIF_INIT_RLIST_F3,  HWIF_INIT_RLIST_F4,  HWIF_INIT_RLIST_F5,
  HWIF_INIT_RLIST_F6,  HWIF_INIT_RLIST_F7,  HWIF_INIT_RLIST_F8,
  HWIF_INIT_RLIST_F9,  HWIF_INIT_RLIST_F10, HWIF_INIT_RLIST_F11,
  HWIF_INIT_RLIST_F12, HWIF_INIT_RLIST_F13, HWIF_INIT_RLIST_F14,
  HWIF_INIT_RLIST_F15
};

const u32 ref_pic_list1[16] = {
  HWIF_INIT_RLIST_B0,  HWIF_INIT_RLIST_B1,  HWIF_INIT_RLIST_B2,
  HWIF_INIT_RLIST_B3,  HWIF_INIT_RLIST_B4,  HWIF_INIT_RLIST_B5,
  HWIF_INIT_RLIST_B6,  HWIF_INIT_RLIST_B7,  HWIF_INIT_RLIST_B8,
  HWIF_INIT_RLIST_B9,  HWIF_INIT_RLIST_B10, HWIF_INIT_RLIST_B11,
  HWIF_INIT_RLIST_B12, HWIF_INIT_RLIST_B13, HWIF_INIT_RLIST_B14,
  HWIF_INIT_RLIST_B15
};

static const u32 ref_poc_regs[16] = {
  HWIF_CUR_POC_00, HWIF_CUR_POC_01, HWIF_CUR_POC_02, HWIF_CUR_POC_03,
  HWIF_CUR_POC_04, HWIF_CUR_POC_05, HWIF_CUR_POC_06, HWIF_CUR_POC_07,
  HWIF_CUR_POC_08, HWIF_CUR_POC_09, HWIF_CUR_POC_10, HWIF_CUR_POC_11,
  HWIF_CUR_POC_12, HWIF_CUR_POC_13, HWIF_CUR_POC_14, HWIF_CUR_POC_15
};

#ifdef USE_FAKE_RFC_TABLE
void GenerateFakeRFCTable(u8 *cmp_tble_addr,
                          u32 pic_width_in_cbsy,
                          u32 pic_height_in_cbsy,
                          u32 pic_width_in_cbsc,
                          u32 pic_height_in_cbsc,
                          u32 bit_depth) {
  u8 cbs_size_y = 0, cbs_size_c = 0;
#ifdef FAKE_RFC_TBL_LITTLE_ENDIAN
  u8 cbs_sizes_8bit[14] = {64, 32, 16, 8, 4, 2, 129, 64, 32, 16, 8, 4, 2, 129};
  u8 cbs_sizes_10bit[14] = {80, 40, 20, 10, 0x85, 0x42, 0xa1, 80, 40, 20, 10, 0x85, 0x42, 0xa1};
#else
  u8 cbs_sizes_8bit[14] = {129, 2, 4, 8, 16, 32, 64, 129, 2, 4, 8, 16, 32, 64};
  u8 cbs_sizes_10bit[14] = {0xa1, 0x42, 0x85, 10, 20, 40, 80, 0xa1, 0x42, 0x85, 10, 20, 40, 80};
#endif
  u32 i, j, offset;
  u8 *pcbs, *ptbl = NULL;
  if (bit_depth == 8) {
    cbs_size_y = cbs_size_c = 64;
    ptbl = cbs_sizes_8bit;
  } else if (bit_depth == 10) {
    cbs_size_y = cbs_size_c = 80;
    ptbl = cbs_sizes_10bit;
  }
  /*
  LOG(SDK_DEBUG_OUT, "%s#  0x%08x Y: (%d x %d), C: (%d x %d) bitdepth %d\n",
              __FUNCTION__,
              cmp_tble_addr,
              pic_width_in_cbsy,
              pic_height_in_cbsy,
              pic_width_in_cbsc,
              pic_height_in_cbsc,
              bit_depth);
  */

  // Compression table for Y
  pcbs = cmp_tble_addr;
  for (i = 0; i < pic_height_in_cbsy; i++) {
    offset = 0;
    for (j = 0; j < pic_width_in_cbsy/16; j++) {
#ifdef FAKE_RFC_TBL_LITTLE_ENDIAN
      *(u16 *)pcbs = offset;
      memcpy(pcbs+2, ptbl, 14);
#else
      memcpy(pcbs, ptbl, 14);
      *(pcbs+14) = offset >> 8;
      *(pcbs+15) = offset & 0xff;
#endif
      pcbs += 16;
      offset += 16 * cbs_size_y;
    }
  }

  // Compression table for C
  for (i = 0; i < pic_height_in_cbsc; i++) {
    offset = 0;
    for (j = 0; j < pic_width_in_cbsc/16; j++) {
#ifdef FAKE_RFC_TBL_LITTLE_ENDIAN
      *(u16 *)pcbs = offset;
      memcpy(pcbs+2, ptbl, 14);
#else
      memcpy(pcbs, ptbl, 14);
      *(pcbs+14) = offset >> 8;
      *(pcbs+15) = offset & 0xff;
#endif
      pcbs += 16;
      offset += 16 * cbs_size_c;
    }
  }

  /*
    u8 *p = cmp_tble_addr;
    printf("Fake RFC table:\n");
    for (i = 0; i < 8; i++) {
      printf("0x%02x 0x%02x 0x%02x 0x%02x ", p[0], p[1], p[2], p[3]);
      p += 4;
    }
    printf("\n");
  */
}
#endif

u32 AllocateAsicBuffers(struct HevcDecContainer *dec_cont,
                        struct HevcDecAsic *asic_buff) {
//  i32 ret = 0;
  u32 size, i;

  /* cabac tables and scaling lists in separate bases, but memory allocated
   * in one chunk */
  size = NEXT_MULTIPLE(ASIC_SCALING_LIST_SIZE, MAX(16, ALIGN(dec_cont->align)));
  /* max number of tiles times width and height (2 bytes each),
   * rounding up to next 16 bytes boundary + one extra 16 byte
   * chunk (HW guys wanted to have this) */
  size += NEXT_MULTIPLE((MAX_TILE_COLS * MAX_TILE_ROWS * 4 * sizeof(u16) + 15 +
                         16) & ~0xF, MAX(16, ALIGN(dec_cont->align)));
#ifdef USE_FAKE_RFC_TABLE
  asic_buff->fake_table_offset = size;
  size += asic_buff->fake_tbly_size + asic_buff->fake_tblc_size;
#endif
  asic_buff->scaling_lists_offset = 0;
  asic_buff->tile_info_offset = NEXT_MULTIPLE(ASIC_SCALING_LIST_SIZE,
                                              MAX(16, ALIGN(dec_cont->align)));

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
#ifdef ASIC_TRACE_SUPPORT
        ret = DWLMallocRefFrm(dec_cont->dwl, size, &asic_buff->misc_linear[i]);
#else
        ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->misc_linear[i]);
#endif
        if (ret) return 1;
      }
    }
  }

#ifdef USE_FAKE_RFC_TABLE
  if (dec_cont->use_video_compressor) {
    u32 pic_width_in_cbsy, pic_height_in_cbsy;
    u32 pic_width_in_cbsc, pic_height_in_cbsc;
    const struct SeqParamSet *sps = dec_cont->storage.active_sps;
    u32 bit_depth = (sps->bit_depth_luma == 8 && sps->bit_depth_chroma == 8) ? 8 : 10;

    pic_width_in_cbsy = ((sps->pic_width + 8 - 1)/8);
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = ((sps->pic_width + 16 - 1)/16);
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (sps->pic_height + 8 - 1)/8;
    pic_height_in_cbsc = (sps->pic_height/2 + 4 - 1)/4;

    for (i = 0; i < dec_cont->n_cores; i++)
      GenerateFakeRFCTable((u8 *)asic_buff->misc_linear[i].virtual_address+asic_buff->fake_table_offset,
                           pic_width_in_cbsy,
                           pic_height_in_cbsy,
                           pic_width_in_cbsc,
                           pic_height_in_cbsc,
                           bit_depth);
  }
#endif
  return 0;
}

i32 ReleaseAsicBuffers(struct HevcDecContainer *dec_cont, struct HevcDecAsic *asic_buff) {
  u32 i;
  const void *dwl = dec_cont->dwl;
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, MISC_LINEAR_BUFFER)) {
    for(i = 0; i < MAX_ASIC_CORES; i++) {
      if (asic_buff->misc_linear[i].virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
        DWLFreeRefFrm(dwl, &asic_buff->misc_linear[i]);
#else
        DWLFreeLinear(dwl, &asic_buff->misc_linear[i]);
#endif
        asic_buff->misc_linear[i].virtual_address = NULL;
        asic_buff->misc_linear[i].size = 0;
      }
    }
  }
  return 0;
}

u32 AllocateAsicTileEdgeMems(struct HevcDecContainer *dec_cont) {
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
  u32 num_tile_cols = dec_cont->storage.active_pps->tile_info.num_tile_columns;
  struct SeqParamSet *sps = dec_cont->storage.active_sps;
  u32 pixel_width = (sps->bit_depth_luma == 8 && sps->bit_depth_chroma == 8) ? 8 : 10;
  u32 core_id = dec_cont->vcmd_used ? dec_cont->mc_buf_id :
                  dec_cont->b_mc ? dec_cont->core_id : 0;
  if (num_tile_cols < 2) return HANTRO_OK;

  u32 height64 = (dec_cont->storage.active_sps->pic_height + 63) & ~63;
  u32 size = ASIC_VERT_FILTER_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width / 8
             + ASIC_BSD_CTRL_RAM_SIZE * height64 * (num_tile_cols - 1)
             + ASIC_VERT_SAO_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width / 8;
  if (asic_buff->tile_edge[core_id].size >= size) return HANTRO_OK;

  asic_buff->filter_mem_offset[core_id] = 0;
  asic_buff->bsd_control_mem_offset[core_id] = asic_buff->filter_mem_offset[core_id]
                                      + ASIC_VERT_FILTER_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width / 8;
  asic_buff->sao_mem_offset[core_id] = asic_buff->bsd_control_mem_offset[core_id] + ASIC_BSD_CTRL_RAM_SIZE * height64 * (num_tile_cols - 1);

  ReleaseAsicTileEdgeMems(dec_cont, core_id);

  asic_buff->tile_edge[core_id].mem_type = DWL_MEM_TYPE_VPU_ONLY;
  i32 dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->tile_edge[core_id]);
  if (dwl_ret != DWL_OK) {
    return HANTRO_NOK;
  }
  return HANTRO_OK;
}

void ReleaseAsicTileEdgeMems(struct HevcDecContainer *dec_cont, u32 core_id) {
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, TILE_EDGE_BUFFER)) {
    if (asic_buff->tile_edge[core_id].virtual_address != NULL) {
      DWLFreeLinear(dec_cont->dwl, &asic_buff->tile_edge[core_id]);
      asic_buff->tile_edge[core_id].virtual_address = NULL;
      asic_buff->tile_edge[core_id].size = 0;
    }
  }
}

u32 HevcRunAsic(struct HevcDecContainer *dec_cont,
                struct HevcDecAsic *asic_buff) {

  u32 asic_status = 0;
  i32 ret = 0;
  i32 irq = 0;
  u32 core_id;
  u32 i, tmp;
  u32 long_term_flags;
  u32 tile_enable = 0;
  struct PicParamSet *pps = dec_cont->storage.active_pps;
  struct SeqParamSet *sps = dec_cont->storage.active_sps;
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  struct Storage *storage = &dec_cont->storage;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  u32 ctb_size = 1 << sps->log_max_coding_block_size;
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  no_chroma = 0;

  /* start new picture */
  if (!dec_cont->asic_running) {
    u32 reserve_ret = 0;

    if (dec_cont->vcmd_used) {
      dec_cont->cmdbuf_id = 0;
      if (dec_cont->b_mc) {
        FifoObject obj;
        FifoPop(dec_cont->fifo_core, &obj, FIFO_EXCEPTION_DISABLE);
        dec_cont->mc_buf_id = (i32)(addr_t)obj;
      }
      reserve_ret = DWLReserveCmdBuf(dec_cont->dwl, DWL_CLIENT_TYPE_HEVC_DEC,
                                     sps->pic_width, sps->pic_height,
                                     &dec_cont->cmdbuf_id);
    } else {
      reserve_ret = DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_HEVC_DEC);
    }
    if (reserve_ret != DWL_OK) {
      return X170_DEC_HW_RESERVED;
    }

    core_id = dec_cont->vcmd_used ? dec_cont->mc_buf_id :
                dec_cont->b_mc ? dec_cont->core_id : 0;
    dec_cont->asic_running = 1;

    /* Allocate memory for asic filter or reallocate in case old
       one is too small. */
    if (AllocateAsicTileEdgeMems(dec_cont) != HANTRO_OK) {
      if (!dec_cont->vcmd_used)
        DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
      else
        DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      return DEC_MEMFAIL;
    }

    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_TILE_BASE,
                 asic_buff->misc_linear[core_id].bus_address + asic_buff->tile_info_offset);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_VERT_FILT_BASE,
                 asic_buff->tile_edge[core_id].bus_address + asic_buff->filter_mem_offset[core_id]);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_VERT_SAO_BASE,
                 asic_buff->tile_edge[core_id].bus_address + asic_buff->sao_mem_offset[core_id]);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_BSD_CTRL_BASE,
                 asic_buff->tile_edge[core_id].bus_address + asic_buff->bsd_control_mem_offset[core_id]);

    SetDecRegister(dec_cont->hevc_regs, HWIF_TILE_ENABLE,
               pps->tiles_enabled_flag);
    if (pps->tiles_enabled_flag) {
      u32 j, h;
      u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
      if (dec_cont->legacy_regs) {
        SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS_V0,
                       pps->tile_info.num_tile_columns);
        SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS_V0,
                       pps->tile_info.num_tile_rows);
      } else {
        if (!hw_feature.pic_size_reg_unified) {
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS,
                         pps->tile_info.num_tile_columns);
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS,
                         pps->tile_info.num_tile_rows);
        } else {
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS_8K,
                         pps->tile_info.num_tile_columns);
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS_8K,
                         pps->tile_info.num_tile_rows);
        }
      }

      /* write width + height for each tile in pic */
      if (!pps->tile_info.uniform_spacing) {
        u32 tmp_w = 0, tmp_h = 0;

        for (i = 0; i < pps->tile_info.num_tile_rows; i++) {
          if (i == pps->tile_info.num_tile_rows - 1)
            h = storage->pic_height_in_ctbs - tmp_h;
          else
            h = pps->tile_info.row_height[i];
          tmp_h += h;
          if (i == 0 && h == 1 && ctb_size == 16)
            no_chroma = 1;
          for (j = 0, tmp_w = 0; j < pps->tile_info.num_tile_columns - 1; j++) {
            tmp_w += pps->tile_info.col_width[j];
            *p++ = pps->tile_info.col_width[j];
            *p++ = h;
            if (i == 0 && h == 1 && ctb_size == 16)
            no_chroma = 1;
          }
          /* last column */
          *p++ = storage->pic_width_in_ctbs - tmp_w;
          *p++ = h;
        }
      } else { /* uniform spacing */
        u32 prev_h, prev_w;

        for (i = 0, prev_h = 0; i < pps->tile_info.num_tile_rows; i++) {
          tmp = (i + 1) * storage->pic_height_in_ctbs /
                pps->tile_info.num_tile_rows;
          h = tmp - prev_h;
          prev_h = tmp;
          if (i == 0 && h == 1 && ctb_size == 16)
            no_chroma = 1;
          for (j = 0, prev_w = 0; j < pps->tile_info.num_tile_columns; j++) {
            tmp = (j + 1) * storage->pic_width_in_ctbs /
                  pps->tile_info.num_tile_columns;
            *p++ = tmp - prev_w;
            *p++ = h;
            if (j == 0 && pps->tile_info.col_width[0] == 1 && ctb_size == 16)
              no_chroma = 1;
            prev_w = tmp;
          }
        }
      }
    } else {
      /* just one "tile", dimensions equal to pic size */
      u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
      if(dec_cont->legacy_regs) {
        SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS_V0, 1);
        SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS_V0, 1);
      } else {
        if (hw_feature.pic_size_reg_unified) {
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS_8K, 1);
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS_8K, 1);
        } else {
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_COLS, 1);
          SetDecRegister(dec_cont->hevc_regs, HWIF_NUM_TILE_ROWS, 1);
        }
      }
      p[0] = storage->pic_width_in_ctbs;
      p[1] = storage->pic_height_in_ctbs;
    }

    /* write all bases */
    long_term_flags = 0;
    for (i = 0; i < dpb->dpb_size; i++) {
      if (i < dpb->tot_buffers) {
        SET_ADDR_REG2(dec_cont->hevc_regs, ref_ybase[i], ref_ybase_msb[i],
                      dpb->buffer[i].data->bus_address);
        SET_ADDR_REG2(dec_cont->hevc_regs, ref_cbase[i], ref_cbase_msb[i],
                      dpb->buffer[i].data->bus_address + storage->pic_size);
        long_term_flags |= (IS_LONG_TERM_FRAME(dpb->buffer[i]))<<(15-i);
        SET_ADDR_REG2(dec_cont->hevc_regs, ref_dbase[i], ref_dbase_msb[i],
                      dpb->buffer[i].data->bus_address + dpb->dir_mv_offset);

        if (dec_cont->use_video_compressor) {
#ifndef USE_FAKE_RFC_TABLE
          SET_ADDR_REG2(dec_cont->hevc_regs, ref_tybase[i], ref_tybase_msb[i],
                        dpb->buffer[i].data->bus_address + dpb->cbs_tbl_offsety);
          SET_ADDR_REG2(dec_cont->hevc_regs, ref_tcbase[i], ref_tcbase_msb[i],
                        dpb->buffer[i].data->bus_address + dpb->cbs_tbl_offsetc);
#else
          if (IS_RAP_NAL_UNIT(dec_cont->storage.prev_nal_unit) ||
              asic_buff->out_buffer->bus_address == dpb->buffer[i].data->bus_address) {
            SET_ADDR_REG2(dec_cont->hevc_regs, ref_tybase[i], ref_tybase_msb[i],
                          asic_buff->misc_linear[core_id].bus_address + asic_buff->fake_table_offset);
            SET_ADDR_REG2(dec_cont->hevc_regs, ref_tcbase[i], ref_tcbase_msb[i],
                          asic_buff->misc_linear[core_id].bus_address + asic_buff->fake_table_offset + asic_buff->fake_tbly_size);
          } else {
            SET_ADDR_REG2(dec_cont->hevc_regs, ref_tybase[i], ref_tybase_msb[i],
                          dpb->buffer[i].data->bus_address + dpb->cbs_tbl_offsety);
            SET_ADDR_REG2(dec_cont->hevc_regs, ref_tcbase[i], ref_tcbase_msb[i],
                          dpb->buffer[i].data->bus_address + dpb->cbs_tbl_offsetc);
          }
#endif
        } else {
          SET_ADDR_REG2(dec_cont->hevc_regs, ref_tybase[i], ref_tybase_msb[i], 0L);
          SET_ADDR_REG2(dec_cont->hevc_regs, ref_tcbase[i], ref_tcbase_msb[i], 0L);
        }
      }
    }

#ifdef USE_FAKE_RFC_TABLE
    for (i = dpb->dpb_size; i < 16; i++) {
      if (dec_cont->use_video_compressor) {
        SET_ADDR_REG2(dec_cont->hevc_regs, ref_tybase[i], ref_tybase_msb[i],
                      asic_buff->misc_linear[core_id].bus_address + asic_buff->fake_table_offset);
        SET_ADDR_REG2(dec_cont->hevc_regs, ref_tcbase[i], ref_tcbase_msb[i],
                      asic_buff->misc_linear[core_id].bus_address + asic_buff->fake_table_offset + asic_buff->fake_tbly_size);
      }
    }
#endif

    SetDecRegister(dec_cont->hevc_regs, HWIF_REFER_LTERM_E, long_term_flags);

    /* scaling lists */
    SetDecRegister(dec_cont->hevc_regs, HWIF_SCALING_LIST_E,
                   sps->scaling_list_enable);
    if (sps->scaling_list_enable) {
      u32 s, j, k;
      u8 *p;

      u8(*scaling_list)[6][64];

      /* determine where to read lists from */
      if (pps->scaling_list_present_flag)
        scaling_list = pps->scaling_list;
      else {
        if (!sps->scaling_list_present_flag)
          DefaultScalingList(sps->scaling_list);
        scaling_list = sps->scaling_list;
      }

      p = (u8 *)dec_cont->asic_buff->misc_linear[core_id].virtual_address;
      ASSERT(!((addr_t)p & 0x7));
      SET_ADDR_REG(dec_cont->hevc_regs, HWIF_SCALE_LIST_BASE,
                   dec_cont->asic_buff->misc_linear[core_id].bus_address);

      /* dc coeffs of 16x16 and 32x32 lists, stored after 16 coeffs of first
       * 4x4 list */
      for (i = 0; i < 8; i++) *p++ = scaling_list[0][0][16 + i];
      /* next 128b boundary */
      p += 8;

      /* write scaling lists column by column */

      /* 4x4 */
      for (i = 0; i < 6; i++) {
        for (j = 0; j < 4; j++)
          for (k = 0; k < 4; k++) *p++ = scaling_list[0][i][4 * k + j];
      }
      /* 8x8 -> 32x32 */
      for (s = 1; s < 4; s++) {
        for (i = 0; i < (s == 3 ? 2 : 6); i++) {
          for (j = 0; j < 8; j++)
            for (k = 0; k < 8; k++) *p++ = scaling_list[s][i][8 * k + j];
        }
      }
    }

  if (no_chroma)
    dec_cont->tile_by_tile = 0;
  if (!pps->tiles_enabled_flag || (pps->tile_info.num_tile_columns < 2)) {
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_TILE_INT_E, 0);
    tile_enable = 0;
  } else {
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_TILE_INT_E, dec_cont->tile_by_tile);
    tile_enable = dec_cont->tile_by_tile;
  }

    if (!dec_cont->partial_decoding)
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_DIS,
                     asic_buff->disable_out_writing);
    else {
      SetDecRegister(dec_cont->hevc_regs, HWIF_REF_READ_DIS, 1);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_DIS, 1);
      SetDecRegister(dec_cont->hevc_regs, HWIF_WRITE_MVS_E, 1);
    }

    u16 *p = (u16 *)((u8 *)asic_buff->misc_linear[core_id].virtual_address + asic_buff->tile_info_offset);
    DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, p, tile_enable);
#ifndef ASIC_TRACE_SUPPORT
    if(dec_cont->b_mc)
#endif
      DWLReadMcRefBuffer(dec_cont->dwl, dec_cont->core_id, asic_buff->out_buffer->logical_size - dpb->sync_mc_offset - 32, dpb->dpb_size);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 1);
    FlushDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->hevc_regs);

    if(dec_cont->b_mc)
      HevcMCSetHwRdyCallback(dec_cont);
    else
      DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id, NULL, NULL);

    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->hevc_regs[1]);

    if(dec_cont->b_mc) {
      /* reset shadow HW status reg values so that we dont end up writing
       * some garbage to next core regs */
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);

      dec_cont->asic_running = 0;
      return DEC_HW_IRQ_RDY;
    }

  } else { /* buffer was empty and now we restart with new stream values */
    ASSERT(!dec_cont->b_mc);
    HevcStreamPosUpdate(dec_cont);

    /* HWIF_STRM_START_BIT */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->hevc_regs[5]);
    /* HWIF_STREAM_DATA_LEN */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->hevc_regs[6]);
    /* HWIF_STREAM_BASE_MSB */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168,
                dec_cont->hevc_regs[168]);
    /* HWIF_STREAM_BASE_LSB */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169,
                dec_cont->hevc_regs[169]);
    /* HWIF_STREAM_BUFFER_LEN */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->hevc_regs[258]);
    /* HWIF_STREAM_START_OFFSET */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->hevc_regs[259]);

    /* start HW by clearing IRQ_BUFFER_EMPTY status bit */
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                dec_cont->hevc_regs[1]);
  }

  if (dec_cont->vcmd_used) {
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  } else {
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                         (u32)DEC_X170_TIMEOUT_LENGTH);
  }

  if (ret != DWL_HW_WAIT_OK) {
    ERROR_PRINT("DWLWaitHwReady");
    DEBUG_PRINT(("DWLWaitHwReady returned: %d\n", ret));
    if(dec_cont->low_latency)
      sem_wait(&dec_cont->updated_reg_sem);

    /* Reset HW */
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);

    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                 dec_cont->hevc_regs[1]);

    dec_cont->asic_running = 0;
    if (!dec_cont->vcmd_used)
      irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    else
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    if (irq == DWL_CACHE_FATAL_RECOVERY)
      return X170_DEC_SYSTEM_ERROR;
    if (irq == DWL_CACHE_FATAL_UNRECOVERY)
      return X170_DEC_FATAL_SYSTEM_ERROR;

    return (ret == DWL_HW_WAIT_ERROR) ? X170_DEC_SYSTEM_ERROR
           : X170_DEC_TIMEOUT;
  }
  RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->hevc_regs);
  asic_status = GetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT);
  while (asic_status & DEC_HW_IRQ_TILE) {
    dec_cont->tile_id++;
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0); /* just in case */
    if (dec_cont->vcmd_used) {
      FlushDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->hevc_regs);
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    } else {
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->hevc_regs[1]);
    }

    if (dec_cont->vcmd_used) {
      ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
    } else {
      ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id,
                         (u32)DEC_X170_TIMEOUT_LENGTH);
    }
    if (ret != DWL_HW_WAIT_OK) {
      ERROR_PRINT("DWLWaitHwReady");
      DEBUG_PRINT(("DWLWaitHwReady returned: %d\n", ret));
      if(dec_cont->low_latency)
        sem_wait(&dec_cont->updated_reg_sem);

      /* Reset HW */
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);

      if (!dec_cont->vcmd_used) {
        DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                     dec_cont->hevc_regs[1]);
      }

      dec_cont->asic_running = 0;
      if (!dec_cont->vcmd_used)
        irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
      else
        DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      if (irq == DWL_CACHE_FATAL_RECOVERY)
        return X170_DEC_SYSTEM_ERROR;
      if (irq == DWL_CACHE_FATAL_UNRECOVERY)
        return X170_DEC_FATAL_SYSTEM_ERROR;
      return (ret == DWL_HW_WAIT_ERROR) ? X170_DEC_SYSTEM_ERROR
             : X170_DEC_TIMEOUT;
    }
    RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->hevc_regs);
    asic_status = GetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT);
  }
  dec_cont->tile_id = 0;
  if(dec_cont->low_latency)
    sem_wait(&dec_cont->updated_reg_sem);

  SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0); /* just in case */

  if (!(asic_status & DEC_HW_IRQ_BUFFER)) {
    /* HW done, release it! */
    dec_cont->asic_running = 0;

    if (!dec_cont->vcmd_used)
      irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    else
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    if (irq == DWL_CACHE_FATAL_RECOVERY)
      return X170_DEC_SYSTEM_ERROR;
    if (irq == DWL_CACHE_FATAL_UNRECOVERY)
      return X170_DEC_FATAL_SYSTEM_ERROR;
  }

  if (!dec_cont->secure_mode) {
    addr_t last_read_address;
    u32 bytes_processed;
    const addr_t start_address =
      dec_cont->hw_stream_start_bus & (~DEC_HW_ALIGN_MASK);
    const u32 offset_bytes = dec_cont->hw_stream_start_bus & DEC_HW_ALIGN_MASK;

    last_read_address =
      GetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_BASE_LSB);
    if(sizeof(addr_t) == 8)
      last_read_address |= ((u64)GetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_BASE_MSB))<<32;

    if (asic_status == DEC_HW_IRQ_RDY && last_read_address == dec_cont->hw_stream_start_bus) {
      last_read_address = start_address + dec_cont->hw_buffer_length;
    }

    if(last_read_address <= start_address)
      bytes_processed = dec_cont->hw_buffer_length - (u32)(start_address - last_read_address);
    else
      bytes_processed = (u32)(last_read_address - start_address);

    DEBUG_PRINT(
      ("HW updated stream position: %08x\n"
       "           processed bytes: %8d\n"
       "     of which offset bytes: %8d\n",
       last_read_address, bytes_processed, offset_bytes));

    /* from start of the buffer add what HW has decoded */
    /* end - start smaller or equal than maximum */
    if(dec_cont->low_latency)
        dec_cont->hw_length = dec_cont->tmp_length;
    if ((bytes_processed - offset_bytes) > dec_cont->hw_length) {

      if ((asic_status & DEC_HW_IRQ_RDY) || (asic_status & DEC_HW_IRQ_BUFFER)) {
        DEBUG_PRINT(("New stream position out of range!\n"));
        // ASSERT(0);
        dec_cont->hw_stream_start += dec_cont->hw_length;
        dec_cont->hw_stream_start_bus += dec_cont->hw_length;
        dec_cont->hw_length = 0; /* no bytes left */
        dec_cont->stream_pos_updated = 1;

        /* Though asic_status returns DEC_HW_IRQ_BUFFER, the stream consumed is abnormal,
         * so we consider it's an errorous stream and release HW. */
        if (asic_status & DEC_HW_IRQ_BUFFER) {
          dec_cont->asic_running = 0;
          if (!dec_cont->vcmd_used)
            irq = DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
          else
            irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
          if (irq == DWL_CACHE_FATAL_RECOVERY)
            return X170_DEC_SYSTEM_ERROR;
          if (irq == DWL_CACHE_FATAL_UNRECOVERY)
            return X170_DEC_FATAL_SYSTEM_ERROR;
        }

        return DEC_HW_IRQ_ERROR;
      }

      /* consider all buffer processed */
      dec_cont->hw_stream_start += dec_cont->hw_length;
      dec_cont->hw_stream_start_bus += dec_cont->hw_length;
      dec_cont->hw_length = 0; /* no bytes left */
    } else {
      dec_cont->hw_length -= (bytes_processed - offset_bytes);
      dec_cont->hw_stream_start += (bytes_processed - offset_bytes);
      dec_cont->hw_stream_start_bus += (bytes_processed - offset_bytes);
    }

    /* if turnaround */
    if(dec_cont->hw_stream_start > (dec_cont->hw_buffer + dec_cont->hw_buffer_length)) {
      dec_cont->hw_stream_start -= dec_cont->hw_buffer_length;
      dec_cont->hw_stream_start_bus -= dec_cont->hw_buffer_length;
    }
    /* else will continue decoding from the beginning of buffer */
  } else {
    dec_cont->hw_stream_start += dec_cont->hw_length;
    dec_cont->hw_stream_start_bus += dec_cont->hw_length;
    dec_cont->hw_length = 0; /* no bytes left */
  }

  dec_cont->stream_pos_updated = 1;

  return asic_status;
}

void HevcSetRegs(struct HevcDecContainer *dec_cont) {
  u32 i;
  i8 poc_diff;
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;

  struct SeqParamSet *sps = dec_cont->storage.active_sps;
  struct PicParamSet *pps = dec_cont->storage.active_pps;
  const struct SliceHeader *slice_header = dec_cont->storage.slice_header;
  const struct DpbStorage *dpb = dec_cont->storage.dpb;
  const struct Storage *storage = &dec_cont->storage;
  u32 pixel_width = (sps->bit_depth_luma == 8 && sps->bit_depth_chroma == 8) ? 8 : 10;
  u32 rs_pixel_width = (dec_cont->use_8bits_output || pixel_width == 8) ? 8 :
                       dec_cont->use_p010_output ? 16 : 10;
  u32 id = DWLReadAsicID(DWL_CLIENT_TYPE_HEVC_DEC);
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* mono chrome */
  SetDecRegister(dec_cont->hevc_regs, HWIF_BLACKWHITE_E, sps->chroma_format_idc == 0);

  SetDecRegister(dec_cont->hevc_regs, HWIF_BIT_DEPTH_Y_MINUS8,
                 sps->bit_depth_luma - 8);
  SetDecRegister(dec_cont->hevc_regs, HWIF_BIT_DEPTH_C_MINUS8,
                 sps->bit_depth_chroma - 8);
  if (hw_feature.pp_version == G2_NATIVE_PP) {
    SetDecRegister(dec_cont->hevc_regs, HWIF_OUTPUT_8_BITS, dec_cont->use_8bits_output);
    SetDecRegister(dec_cont->hevc_regs,
                   HWIF_OUTPUT_FORMAT,
                   dec_cont->use_p010_output ? 1 :
                   (dec_cont->pixel_format == DEC_OUT_PIXEL_CUSTOMER1 ? 2 : 0));
  }

  SetDecRegister(dec_cont->hevc_regs, HWIF_PCM_E, sps->pcm_enabled);
  SetDecRegister(dec_cont->hevc_regs, HWIF_PCM_BITDEPTH_Y,
                 sps->pcm_bit_depth_luma);
  SetDecRegister(dec_cont->hevc_regs, HWIF_PCM_BITDEPTH_C,
                 sps->pcm_bit_depth_chroma);

  SetDecRegister(dec_cont->hevc_regs, HWIF_REFPICLIST_MOD_E,
                 pps->lists_modification_present_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_MIN_CB_SIZE,
                 sps->log_min_coding_block_size);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_CB_SIZE,
                 sps->log_max_coding_block_size);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MIN_TRB_SIZE,
                 sps->log_min_transform_block_size);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_TRB_SIZE,
                 sps->log_max_transform_block_size);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MIN_PCM_SIZE,
                 sps->log_min_pcm_block_size);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_PCM_SIZE,
                 sps->log_max_pcm_block_size);

  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_INTER_HIERDEPTH,
                 sps->max_transform_hierarchy_depth_inter);
  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_INTRA_HIERDEPTH,
                 sps->max_transform_hierarchy_depth_intra);

  SetDecRegister(dec_cont->hevc_regs, HWIF_ASYM_PRED_E,
                 sps->asymmetric_motion_partitions_enable);
  SetDecRegister(dec_cont->hevc_regs, HWIF_SAO_E,
                 sps->sample_adaptive_offset_enable);

  SetDecRegister(dec_cont->hevc_regs, HWIF_PCM_FILT_DISABLE,
                 sps->pcm_loop_filter_disable);

  SetDecRegister(
    dec_cont->hevc_regs, HWIF_TEMPOR_MVP_E,
    sps->temporal_mvp_enable && !IS_IDR_NAL_UNIT(storage->prev_nal_unit));

  SetDecRegister(dec_cont->hevc_regs, HWIF_SIGN_DATA_HIDE,
                 pps->sign_data_hiding_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_CABAC_INIT_PRESENT,
                 pps->cabac_init_present);

  SetDecRegister(dec_cont->hevc_regs, HWIF_REFIDX0_ACTIVE,
                 pps->num_ref_idx_l0_active);
  SetDecRegister(dec_cont->hevc_regs, HWIF_REFIDX1_ACTIVE,
                 pps->num_ref_idx_l1_active);

  if (dec_cont->legacy_regs)
    SetDecRegister(dec_cont->hevc_regs, HWIF_INIT_QP_V0, pps->pic_init_qp);
  else
    SetDecRegister(dec_cont->hevc_regs, HWIF_INIT_QP, pps->pic_init_qp);

  SetDecRegister(dec_cont->hevc_regs, HWIF_CONST_INTRA_E,
                 pps->constrained_intra_pred_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_TRANSFORM_SKIP_E,
                 pps->transform_skip_enable);

  SetDecRegister(dec_cont->hevc_regs, HWIF_CU_QPD_E, pps->cu_qp_delta_enabled);

  SetDecRegister(dec_cont->hevc_regs, HWIF_MAX_CU_QPD_DEPTH,
                 pps->diff_cu_qp_delta_depth);

  SetDecRegister(dec_cont->hevc_regs, HWIF_CH_QP_OFFSET,
                 asic_buff->chroma_qp_index_offset);
  SetDecRegister(dec_cont->hevc_regs, HWIF_CH_QP_OFFSET2,
                 asic_buff->chroma_qp_index_offset2);

  SetDecRegister(dec_cont->hevc_regs, HWIF_SLICE_CHQP_FLAG,
                 pps->slice_level_chroma_qp_offsets_present);

  SetDecRegister(dec_cont->hevc_regs, HWIF_WEIGHT_PRED_E,
                 pps->weighted_pred_flag);
  SetDecRegister(dec_cont->hevc_regs, HWIF_WEIGHT_BIPR_IDC,
                 pps->weighted_bi_pred_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_TRANSQ_BYPASS_E,
                 pps->trans_quant_bypass_enable);

  SetDecRegister(dec_cont->hevc_regs, HWIF_DEPEND_SLICE_E,
                 pps->dependent_slice_segments_enabled);

  SetDecRegister(dec_cont->hevc_regs, HWIF_SLICE_HDR_EBITS,
                 pps->num_extra_slice_header_bits);

  SetDecRegister(dec_cont->hevc_regs, HWIF_STRONG_SMOOTH_E,
                 sps->strong_intra_smoothing_enable);

  /* make sure that output pic sync memory is cleared */
  {
    char *sync_base =
      (char *) (asic_buff->out_buffer->virtual_address) +
      dpb->sync_mc_offset;
    u32 sync_size = 32; /* 32 bytes for each field */
    (void) DWLmemset(sync_base, 0, sync_size);
  }

  SetDecRegister(dec_cont->hevc_regs, HWIF_ENTR_CODE_SYNCH_E,
                 pps->entropy_coding_sync_enabled_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_SLICE_BORDER,
                 pps->loop_filter_across_slices_enabled_flag);
  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_TILE_BORDER,
                 pps->loop_filter_across_tiles_enabled_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_CTRL_PRES,
                 pps->deblocking_filter_control_present_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_OVERRIDE_E,
                 pps->deblocking_filter_override_enabled_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_FILTERING_DIS,
                 pps->disable_deblocking_filter_flag);

  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_OFFSET_BETA,
                 pps->beta_offset / 2);
  SetDecRegister(dec_cont->hevc_regs, HWIF_FILT_OFFSET_TC, pps->tc_offset / 2);

  SetDecRegister(dec_cont->hevc_regs, HWIF_PARALLEL_MERGE,
                 pps->log_parallel_merge_level);

  SetDecRegister(dec_cont->hevc_regs, HWIF_IDR_PIC_E,
                 IS_RAP_NAL_UNIT(storage->prev_nal_unit));

  SetDecRegister(dec_cont->hevc_regs, HWIF_HDR_SKIP_LENGTH,
                 slice_header->hw_skip_bits);

#ifdef ENABLE_FPGA_VERIFICATION
#if 0
  DWLPrivateAreaMemset(asic_buff->out_buffer->virtual_address, 0,
                       storage->pic_size * 3 / 2);
#else
  if (!dec_cont->pp_enabled)
    DWLmemset(asic_buff->out_buffer->virtual_address, 0,
              storage->pic_size +
              NEXT_MULTIPLE(storage->pic_size / 2, MAX(16, ALIGN(dec_cont->align))));
#endif
#endif

  SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_YBASE,
               asic_buff->out_buffer->bus_address);

  /* offset to beginning of chroma part of out and ref pics */
  SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_CBASE,
               asic_buff->out_buffer->bus_address + storage->pic_size);

  SetDecRegister(dec_cont->hevc_regs, HWIF_WRITE_MVS_E, 1);

  SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_DBASE,
               asic_buff->out_buffer->bus_address + dpb->dir_mv_offset);

  /* reference pictures */
  /* we always set HWIF_REF_FRAMES as a positive value to
     avoid HW hang when decoding erroneous I frame */
   SetDecRegister(dec_cont->hevc_regs, HWIF_REF_FRAMES,
                 ((dpb->num_poc_st_curr + dpb->num_poc_lt_curr) == 0) ? 1 :
                 (dpb->num_poc_st_curr + dpb->num_poc_lt_curr));

  if (dec_cont->use_video_compressor) {
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_TYBASE,
                 asic_buff->out_buffer->bus_address + dpb->cbs_tbl_offsety);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_TCBASE,
                 asic_buff->out_buffer->bus_address + dpb->cbs_tbl_offsetc);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_EC_BYPASS, 0);
    /* If the size of CBS row >= 64KB, which means it's possible that the offset
       may overflow in EC table, set the EC output in word alignment. */
    if (RFC_MAY_OVERFLOW(sps->pic_width, pixel_width))
      SetDecRegister(dec_cont->hevc_regs, HWIF_EC_WORD_ALIGN, 1);
    else
      SetDecRegister(dec_cont->hevc_regs, HWIF_EC_WORD_ALIGN, 0);
  } else {
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_TYBASE, 0L);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_OUT_TCBASE, 0L);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_EC_BYPASS, 1);
  }

  if (dec_cont->use_fetch_one_pic) {
    SetDecRegister(dec_cont->hevc_regs, HWIF_APF_ONE_PID, 1);
  } else {
    SetDecRegister(dec_cont->hevc_regs, HWIF_APF_ONE_PID, 0);
  }

  SetDecRegister(dec_cont->hevc_regs, HWIF_SLICE_HDR_EXT_E,
                 pps->slice_header_extension_present_flag);

  /* Raster output configuration. */
  {
    u32 rw = storage->pic_width_in_cbs * (1 << sps->log_min_coding_block_size);
    u32 rh = storage->pic_height_in_cbs * (1 << sps->log_min_coding_block_size);
    if ((id & 0x0000F000) >> 12 == 0 && /* MAJOR_VERSION */
        (id & 0x00000FF0) >> 4 == 0)    /* MINOR_VERSION */
      /* On early versions RS_E was RS_DIS and we need to take this into
       * account to support the first hardware version properly. */
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_RS_E /* RS_DIS */,
                     dec_cont->output_format == DEC_OUT_FRM_TILED_4X4);
    else
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_RS_E,
                     dec_cont->output_format != DEC_OUT_FRM_TILED_4X4);

    if (storage->raster_buffer_mgr) {
        u32 ppw, pph;

      struct DWLLinearMem *pp_buffer = asic_buff->out_pp_buffer;
      /* If down_scale is disabled, pp buffer should be raster buffer */
      if (!hw_feature.flexible_scale_support) {
        if (!dec_cont->pp_enabled) {  //pp_buffer
          ppw = NEXT_MULTIPLE(rw * rs_pixel_width, 16 * 8) / 8;
          pph = rh;
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_RSY_BASE,
                       pp_buffer->bus_address);
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_RSC_BASE,
                       pp_buffer->bus_address + ppw * pph);
        } else {
          /* fixed ratio (1/2/4/8) for G2V4 */
          ppw = NEXT_MULTIPLE((rw >> storage->down_scale_x_shift) * rs_pixel_width, 16 * 8) / 8;
          pph = rh >> storage->down_scale_y_shift;
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_E, dec_cont->pp_enabled);
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_X,
                         dec_cont->dscale_shift_x - 1);
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_Y,
                         dec_cont->dscale_shift_y - 1);
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_DSY_BASE,
                       pp_buffer->bus_address);
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_DSC_BASE,
                       pp_buffer->bus_address + ppw * pph);
        }
      } else {
        /* Flexible PP */
        PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
        addr_t ppu_out_bus_addr = pp_buffer->bus_address;
#if 0
#define TOFIX(d, q) ((u32)( (d) * (u32)(1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))
        if (hw_feature.pp_version == G2_NATIVE_PP) { /*(!((id >> 16) == 0x8001))*/
          /* fixed ratio (1/2/4/8) for G2V4 */
          rw = storage->pic_width_in_cbs * (1 << sps->log_min_coding_block_size);
          ppw = NEXT_MULTIPLE((rw >> storage->down_scale_x_shift) * rs_pixel_width, 16 * 8) / 8;
          pph = rh >> storage->down_scale_y_shift;
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_E, dec_cont->pp_enabled);
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_X,
                         dec_cont->dscale_shift_x - 1);
          SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_DS_Y,
                         dec_cont->dscale_shift_y - 1);
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_DSY_BASE,
                       pp_buffer->bus_address);
          SET_ADDR_REG(dec_cont->hevc_regs, HWIF_DEC_DSC_BASE,
                       pp_buffer->bus_address + ppw * pph);
        } else
#endif

        SetDecRegister(dec_cont->hevc_regs, HWIF_PP_IN_FORMAT_U, 1);
        PPSetRegs(dec_cont->hevc_regs, &hw_feature, ppu_cfg, ppu_out_bus_addr, 0, 0);
        DelogoSetRegs(dec_cont->hevc_regs, &hw_feature, dec_cont->delogo_params);
        if (dec_cont->partial_decoding && (!(IS_RAP_NAL_UNIT(storage->prev_nal_unit)))) {
          SetDecRegister(dec_cont->hevc_regs, HWIF_PP_OUT_E_U, 0);
          SetDecRegister(dec_cont->hevc_regs, HWIF_PP1_OUT_E_U, 0);
          SetDecRegister(dec_cont->hevc_regs, HWIF_PP2_OUT_E_U, 0);
          SetDecRegister(dec_cont->hevc_regs, HWIF_PP3_OUT_E_U, 0);
        }
#if 0
        for (i = 0; i < 4; i++, ppu_cfg++) {
          /* flexible scale ratio */
          u32 in_width = ppu_cfg->crop.width;
          u32 in_height = ppu_cfg->crop.height;
          u32 out_width = ppu_cfg->scale.width;
          u32 out_height = ppu_cfg->scale.height;

          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_E_U, ppu_cfg->enabled);
          if (!ppu_cfg->enabled) continue;

          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_CR_FIRST, ppu_cfg->cr_first);
          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].OUT_FORMAT_U, ppu_cfg->out_format);
          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_TILE_E_U, ppu_cfg->tiled_e);

          ppw =ppu_cfg->ystride;
          ppw_ch = ppu_cfg->cstride;
          if (hw_feature.crop_support) {
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].CROP_STARTX_U,
                           ppu_cfg->crop.x >> hw_feature.crop_step_rshift);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].CROP_STARTY_U,
                           ppu_cfg->crop.y >> hw_feature.crop_step_rshift);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_IN_WIDTH_U,
                           ppu_cfg->crop.width >> hw_feature.crop_step_rshift);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_IN_HEIGHT_U,
                           ppu_cfg->crop.height >> hw_feature.crop_step_rshift);
          }
          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_WIDTH_U,
                         ppu_cfg->scale.width);
          SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_HEIGHT_U,
                         ppu_cfg->scale.height);

          if(in_width < out_width) {
            /* upscale */
            u32 W, inv_w;

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

            W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
            inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].SCALE_WRATIO_U, W);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].WSCALE_INVRA_U, inv_w);
          } else if(in_width > out_width) {
            /* downscale */
            u32 hnorm;

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

            hnorm = FDIVI(TOFIX(out_width, 16), in_width) + 1;
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].WSCALE_INVRA_U, hnorm);
          } else {
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].WSCALE_INVRA_U, 0);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HOR_SCALE_MODE_U, 0);
          }

          if(in_height < out_height) {
            /* upscale */
            u32 H, inv_h;

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

            H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].SCALE_HRATIO_U, H);

            inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HSCALE_INVRA_U, inv_h);
          } else if(in_height > out_height) {
            /* downscale */
            u32 Cv;

            Cv = FDIVI(TOFIX(out_height, 16), in_height) + 1;

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].VER_SCALE_MODE_U, 2);

            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HSCALE_INVRA_U, Cv);
          } else {
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].HSCALE_INVRA_U, 0);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].VER_SCALE_MODE_U, 0);
          }
          SET_ADDR_REG2(dec_cont->hevc_regs,
                       ppu_regs[i].PP_OUT_LU_BASE_U_LSB,
                       ppu_regs[i].PP_OUT_LU_BASE_U_MSB,
                       ppu_out_bus_addr + ppu_cfg->luma_offset);
          SET_ADDR_REG2(dec_cont->hevc_regs,
                       ppu_regs[i].PP_OUT_CH_BASE_U_LSB,
                       ppu_regs[i].PP_OUT_CH_BASE_U_MSB,
                       ppu_out_bus_addr + ppu_cfg->chroma_offset);
          if (hw_feature.pp_stride_support) {
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_Y_STRIDE, ppw);
            SetDecRegister(dec_cont->hevc_regs, ppu_regs[i].PP_OUT_C_STRIDE, ppw_ch);
          }
        }
#endif
      }
    }
    if (hw_feature.dec_stride_support) {
      /* No stride when compression is used. */
      u32 out_w = storage->pic_width_in_cbs * (1 << sps->log_min_coding_block_size);
      u32 out_w_y, out_w_c;
      if (!dec_cont->use_video_compressor) {
        out_w_y = out_w_c = NEXT_MULTIPLE(4 * out_w * pixel_width, ALIGN(dec_cont->align) * 8) / 8;
      } else {
        if (hw_feature.rfc_stride_support) {
          out_w_y = NEXT_MULTIPLE(8 * out_w * pixel_width, ALIGN(dec_cont->align) * 8) >> 6;
          out_w_c = NEXT_MULTIPLE(4 * out_w * pixel_width, ALIGN(dec_cont->align) * 8) >> 6;
        } else {
          out_w_y = out_w_c = 4 * out_w * pixel_width / 8;
        }
      }
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_Y_STRIDE, out_w_y);
      SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_OUT_C_STRIDE, out_w_c);
    }
  }

  /* POCs of reference pictures */
  for (i = 0; i < MAX_DPB_SIZE; i++) {
    poc_diff = CLIP3(-128, 127, (i32)storage->poc->pic_order_cnt -
                     (i32)dpb->current_out->ref_poc[i]);
    SetDecRegister(dec_cont->hevc_regs, ref_poc_regs[i], poc_diff);
  }

  HevcStreamPosUpdate(dec_cont);
}

void HevcInitRefPicList(struct HevcDecContainer *dec_cont) {
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  u32 i, j;
  u32 list0[16] = {0};
  u32 list1[16] = {0};

  /* list 0, short term before + short term after + long term */
  for (i = 0, j = 0; i < dpb->num_poc_st_curr; i++)
    list0[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list0[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list0[j++] = list0[i++];

  /* list 1, short term after + short term before + long term */
  /* after */
  for (i = dpb->num_poc_st_curr_before, j = 0; i < dpb->num_poc_st_curr; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  /* before */
  for (i = 0; i < dpb->num_poc_st_curr_before; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list1[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list1[j++] = list1[i++];

  /* TODO: size? */
  for (i = 0; i < MAX_DPB_SIZE; i++) {
    SetDecRegister(dec_cont->hevc_regs, ref_pic_list0[i], list0[i]);
    SetDecRegister(dec_cont->hevc_regs, ref_pic_list1[i], list1[i]);
  }
}

void HevcStreamPosUpdate(struct HevcDecContainer *dec_cont) {
  u32 tmp, is_rb;
  addr_t tmp_addr;

  tmp = 0;
  is_rb = dec_cont->use_ringbuffer;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_HEVC_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* NAL start prefix in stream start is 0 0 0 or 0 0 1 */
  if (dec_cont->hw_stream_start + 2 >= dec_cont->hw_buffer + dec_cont->hw_buffer_length) {
    u8 i, start_prefix[3];
    for(i = 0; i < 3; i++) {
      if (dec_cont->hw_stream_start + i < dec_cont->hw_buffer + dec_cont->hw_buffer_length)
        start_prefix[i] = DWLLowLatencyReadByte(dec_cont->hw_stream_start + i, dec_cont->hw_buffer_length);
      else
        start_prefix[i] = DWLLowLatencyReadByte(dec_cont->hw_stream_start + i - dec_cont->hw_buffer_length, dec_cont->hw_buffer_length);
    }
    if (!(*start_prefix + *(start_prefix + 1))) {
      if (*(start_prefix + 2) < 2) {
        tmp = 1;
      }
    }
  } else {
    if (!(DWLLowLatencyReadByte(dec_cont->hw_stream_start, dec_cont->hw_buffer_length) +
          DWLLowLatencyReadByte(dec_cont->hw_stream_start + 1, dec_cont->hw_buffer_length))) {
      if (DWLLowLatencyReadByte(dec_cont->hw_stream_start + 2, dec_cont->hw_buffer_length) < 2) {
        tmp = 1;
      }
    }
  }

  if (dec_cont->start_code_detected)
      tmp = 1;

  SetDecRegister(dec_cont->hevc_regs, HWIF_START_CODE_E, tmp);

  /* bit offset if base is unaligned */
  tmp = (dec_cont->hw_stream_start_bus & DEC_HW_ALIGN_MASK) * 8;

  SetDecRegister(dec_cont->hevc_regs, HWIF_STRM_START_BIT, tmp);

  dec_cont->hw_bit_pos = tmp;

  if(is_rb) {
    /* stream buffer base address */
    tmp_addr = dec_cont->hw_buffer_start_bus; /* aligned base */
    ASSERT((tmp_addr & 0xF) == 0);
    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_STREAM_BASE, tmp_addr);

    /* stream data start offset */
    tmp = dec_cont->hw_stream_start_bus - dec_cont->hw_buffer_start_bus; /* unaligned base */
    tmp &= (~DEC_HW_ALIGN_MASK);         /* align the base */

    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->hevc_regs, HWIF_STRM_START_OFFSET, tmp);

    /* stream data length */
    tmp = dec_cont->hw_length;       /* unaligned stream */
    tmp += dec_cont->hw_bit_pos / 8; /* add the alignmet bytes */

    if(dec_cont->low_latency) {
      dec_cont->ll_strm_bus_address = dec_cont->hw_stream_start_bus;
      dec_cont->ll_strm_len = tmp;
      dec_cont->first_update = 1;
      dec_cont->update_reg_flag = 1;
      SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_LAST_BUFFER_E, 0);
    } else
      SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, tmp);

    /* stream buffer size */
    tmp = dec_cont->hw_buffer_length;       /* stream buffer size */

    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->hevc_regs, HWIF_STRM_BUFFER_LEN, tmp);
  } else {
    /* stream data base address */
    tmp_addr = dec_cont->hw_stream_start_bus; /* unaligned base */
    tmp_addr &= (~DEC_HW_ALIGN_MASK);         /* align the base */
    ASSERT((tmp_addr & 0xF) == 0);

    SET_ADDR_REG(dec_cont->hevc_regs, HWIF_STREAM_BASE, tmp_addr);

    /* stream data start offset */
    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->hevc_regs, HWIF_STRM_START_OFFSET, 0);

    /* stream data length */
    tmp = dec_cont->hw_length;       /* unaligned stream */
    tmp += dec_cont->hw_bit_pos / 8; /* add the alignmet bytes */

    if(dec_cont->low_latency) {
      dec_cont->ll_strm_bus_address = dec_cont->hw_stream_start_bus;
      dec_cont->ll_strm_len = tmp;
      dec_cont->first_update = 1;
      dec_cont->update_reg_flag = 1;
      SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, 0);
      SetDecRegister(dec_cont->hevc_regs, HWIF_LAST_BUFFER_E, 0);
    } else
      SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN, tmp);

    /* stream buffer size */
    tmp = dec_cont->hw_buffer_length;       /* stream buffer size */

    if (!dec_cont->legacy_regs)
      SetDecRegister(dec_cont->hevc_regs, HWIF_STRM_BUFFER_LEN, tmp);
  }
}

u32 HevcCheckHwStatus(struct HevcDecContainer *dec_cont) {
   u32 tmp =  DWLReadRegFromHw(dec_cont->dwl, dec_cont->core_id, 4 * 1);
   if(tmp & 0x01)
     return 1;
   else
     return 0;
}
