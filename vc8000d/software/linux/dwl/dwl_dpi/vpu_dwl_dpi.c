
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

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "malloc.h"
#include "defines.h"
#include "vpu_dwl_dpi.h"
#include "tb_cfg.h"
#include "assert.h"
#include "ppu.h"
#include "dwl.h"
#include "dwl_activity_trace.h"
#include "dwl_hw_core_array.h"
#include "dwl_swhw_sync.h"

//#include "dwl.h"
//#include "regdrv.h"
//#include "sw_util.h"
//#include "vpufeature.h"
//#include "dwl_hw_core.h"

/*------------------------------------------------------------------------------
    variable for HW TB
------------------------------------------------------------------------------*/

long int *HW_TB_DPB_BASE;
u64  DWL_HW_TB_DPB_SIZE = 0x80000000; // 8096x8096*1.5*5/4*17 , then align  to a big enough value -> 2GB.
u64  HW_TB_DPB_LUMA_BASE;
u64  HW_TB_DPB_CHROMA_BASE;
u64  HW_TB_TILE_BASE;
u64  HW_TB_SCALELIST_BASE;
u64  HW_TB_STRM_BASE;
u64  HW_TB_QTABLE_BASE;
u64  HW_TB_FILT_VER_BASE;
u64  HW_TB_SAO_VER_BASE;
u64  HW_TB_BSD_CTRL_BASE;
u64  HW_TB_ALFTABLE_BASE;
u64  HW_TB_PP_LUMA_BASE;
u64  HW_TB_PROBTABS_BASE;
u64  HW_TB_CTX_COUNTER_BASE;
u64  HW_TB_SEGMENT_IN_BASE;
u64  HW_TB_SEGMENT_OUT_BASE;
u64  HW_TB_CMDBUF_BASE;

u64  HW_TB_TILE_BASE_C1;
u64  HW_TB_SCALELIST_BASE_C1;
u64  HW_TB_STRM_BASE_C1;
u64  HW_TB_QTABLE_BASE_C1;
u64  HW_TB_FILT_VER_BASE_C1;
u64  HW_TB_SAO_VER_BASE_C1;
u64  HW_TB_BSD_CTRL_BASE_C1;
u64  HW_TB_ALFTABLE_BASE_C1;
u64  HW_TB_PP_LUMA_BASE_C1;
u64  HW_TB_PROBTABS_BASE_C1;
u64  HW_TB_CTX_COUNTER_BASE_C1;
u64  HW_TB_SEGMENT_IN_BASE_C1;
u64  HW_TB_SEGMENT_OUT_BASE_C1;

u8   DWL_DPB_allocated = 0;

char dwl_checkname[100]="";
u64  tmp,bytes_y,bytes_c;
u32  tmp32;

extern u32 g_hw_ver, g_hw_id;
extern struct TBCfg tb_cfg;
extern u8 *dpb_base_address;

extern u8 l2_allocate_buf;

extern struct DWLInstance DWL_Instance;
extern u32 cmodel_ref_buf_alignment;

// for interlaced case, pp buffer allocate
struct buffer_index {
  u8 *pp_base;
  u32 offset;
  u32 valid;
};
static struct buffer_index buf_offset[18];
static buf_index = 0;
static pic_buf_count = 0;
u32 dwl_sim_buf_offset = 0;
addr_t dwl_pp_base_address = 0;

void dpi_l2_update_buf_base(u32 *reg_base)
{
  /*-----------------------
 *   ============
 *  |            |
 *  |            |
 *  |    DPB     |
 *  |            |
 *  |            |
 *  ==============
 *  |            |
 *  | Input TILE |
 *  |            |
 *  ==============
 *  |            |
 *  | Input STRM |
 *  |            |
 *  ==============
 *  |            |
 *  |   QTABLE   |
 *  |            |
 *  ==============
 *  |            |
 *  | ALF TABLE  |
 *  |            |
 *  ==============
 *  |            |
 *  | PP0/1/2/3  |
 *  |            |
 *  ==============
 *-------------------------*/
  if ( DWL_DPB_allocated == 0 ){
    HW_TB_DPB_BASE = malloc(sizeof(long int));
    DWL_DPB_allocated = 1;
  }
  dpi_get_buf_base(HW_TB_DPB_BASE, tb_cfg.tb_params.ref_frm_buffer_size);
  if(reg_base[3]>>27 == 3){  // JPEG has no DPB
    DWL_HW_TB_DPB_SIZE = 0;
  }
  else{
    DWL_HW_TB_DPB_SIZE = tb_cfg.tb_params.ref_frm_buffer_size;
  }

  //printf("HW_TB_TILE_BASE=%lx\n",HW_TB_TILE_BASE);

  // get register info here. 
  u32 reg_;
  reg_ = reg_base[10];
  u32 num_tile_cols = reg_ >> 17 & 0x3f;
  u32 num_tile_rows = reg_ >> 12 & 0x1f;
  u32 strm_buf_len = reg_base[258];
 
#ifdef USE_64BIT_ENV
  u32 reg_tmp;
  u64 reg_tmp_;
  reg_tmp_ = reg_base[64];
  reg_tmp = reg_base[65];
  u64 luma_ = (u64)((reg_tmp_ << 32 | reg_tmp) & ~0x3);
  reg_tmp_ = reg_base[98];
  reg_tmp = reg_base[99];
  u64 ch_ = (u64)((reg_tmp_ << 32 | reg_tmp) & ~0x3);
#else
  reg_ = reg_base[65];
  u32 luma_ = (u32)(reg_ & ~0x3);
  reg_ = reg_base[99];
  u32 ch_ = (u32)(reg_ & ~0x3);
#endif 

  u32 pp_stride;
  if(reg_base[3]>>27 == 3){  // JPEG use pp out stride
    pp_stride = (reg_base[329] >> 16) & 0xffff; //GetDecRegister(reg_base,HWIF_PP_OUT_Y_STRIDE);
  }
  else {
    pp_stride = reg_base[314] & 0xffff;
  }

  HW_TB_DPB_LUMA_BASE = *HW_TB_DPB_BASE;
  HW_TB_DPB_CHROMA_BASE = NEXT_MULTIPLE(HW_TB_DPB_LUMA_BASE + ch_ - luma_,cmodel_ref_buf_alignment);

  HW_TB_STRM_BASE = NEXT_MULTIPLE(*HW_TB_DPB_BASE+DWL_HW_TB_DPB_SIZE,cmodel_ref_buf_alignment);
  HW_TB_STRM_BASE_C1 = NEXT_MULTIPLE(HW_TB_STRM_BASE + NEXT_MULTIPLE(strm_buf_len,16),cmodel_ref_buf_alignment);

  HW_TB_QTABLE_BASE = NEXT_MULTIPLE(HW_TB_STRM_BASE_C1 + NEXT_MULTIPLE(strm_buf_len,16),cmodel_ref_buf_alignment);
  HW_TB_QTABLE_BASE_C1 = NEXT_MULTIPLE(HW_TB_QTABLE_BASE + NEXT_MULTIPLE(4096,16),cmodel_ref_buf_alignment);

  HW_TB_ALFTABLE_BASE = NEXT_MULTIPLE(HW_TB_QTABLE_BASE_C1 + NEXT_MULTIPLE(4096,16),cmodel_ref_buf_alignment);
  HW_TB_ALFTABLE_BASE_C1 = NEXT_MULTIPLE(HW_TB_ALFTABLE_BASE + NEXT_MULTIPLE(20*16,16),cmodel_ref_buf_alignment);

  HW_TB_TILE_BASE = NEXT_MULTIPLE(HW_TB_ALFTABLE_BASE + NEXT_MULTIPLE(20*16,16),cmodel_ref_buf_alignment);
  HW_TB_TILE_BASE_C1 = NEXT_MULTIPLE(HW_TB_TILE_BASE + NEXT_MULTIPLE(num_tile_cols*num_tile_rows*4,16),cmodel_ref_buf_alignment);

  HW_TB_SCALELIST_BASE = NEXT_MULTIPLE(HW_TB_TILE_BASE_C1 + NEXT_MULTIPLE(num_tile_cols*num_tile_rows*4,16),cmodel_ref_buf_alignment);
  HW_TB_SCALELIST_BASE_C1 = NEXT_MULTIPLE(HW_TB_SCALELIST_BASE + 32768,cmodel_ref_buf_alignment);
  
  HW_TB_PROBTABS_BASE = NEXT_MULTIPLE(HW_TB_SCALELIST_BASE_C1 + 32768,cmodel_ref_buf_alignment);// constant scalelist buffer size: 5*16bytes, hevc scalelist=1008
  HW_TB_PROBTABS_BASE_C1 = NEXT_MULTIPLE(HW_TB_PROBTABS_BASE + 4096,cmodel_ref_buf_alignment);

  HW_TB_CTX_COUNTER_BASE = NEXT_MULTIPLE(HW_TB_PROBTABS_BASE_C1 + 4096,cmodel_ref_buf_alignment);
  HW_TB_CTX_COUNTER_BASE_C1 = NEXT_MULTIPLE(HW_TB_CTX_COUNTER_BASE + 13264,cmodel_ref_buf_alignment);

  HW_TB_SEGMENT_IN_BASE = NEXT_MULTIPLE(HW_TB_CTX_COUNTER_BASE_C1 + 13264,cmodel_ref_buf_alignment);
  HW_TB_SEGMENT_IN_BASE_C1 = NEXT_MULTIPLE(HW_TB_SEGMENT_IN_BASE + 278528,cmodel_ref_buf_alignment);

  HW_TB_SEGMENT_OUT_BASE = NEXT_MULTIPLE(HW_TB_SEGMENT_IN_BASE_C1 + 278528,cmodel_ref_buf_alignment); // 278528 is 8k segment_size_max
  HW_TB_SEGMENT_OUT_BASE_C1 = NEXT_MULTIPLE(HW_TB_SEGMENT_OUT_BASE + 4096,cmodel_ref_buf_alignment);

  HW_TB_FILT_VER_BASE = NEXT_MULTIPLE(HW_TB_SEGMENT_OUT_BASE_C1 + 4096,cmodel_ref_buf_alignment);
  HW_TB_FILT_VER_BASE_C1 = NEXT_MULTIPLE(HW_TB_FILT_VER_BASE + 1382400,cmodel_ref_buf_alignment);

  HW_TB_SAO_VER_BASE = NEXT_MULTIPLE(HW_TB_FILT_VER_BASE_C1 + 1382400,cmodel_ref_buf_alignment);
  HW_TB_SAO_VER_BASE_C1 = NEXT_MULTIPLE(HW_TB_SAO_VER_BASE + 1382400,cmodel_ref_buf_alignment);

  HW_TB_BSD_CTRL_BASE = NEXT_MULTIPLE(HW_TB_SAO_VER_BASE_C1 + 1382400,cmodel_ref_buf_alignment);
  HW_TB_BSD_CTRL_BASE_C1 = NEXT_MULTIPLE(HW_TB_BSD_CTRL_BASE + NEXT_MULTIPLE(20*16,16),cmodel_ref_buf_alignment);

  if (tb_cfg.cmdbuf_params.cmd_en) {
  HW_TB_CMDBUF_BASE = NEXT_MULTIPLE(HW_TB_BSD_CTRL_BASE_C1 + NEXT_MULTIPLE(20*16,16),cmodel_ref_buf_alignment);
  HW_TB_PP_LUMA_BASE = NEXT_MULTIPLE(HW_TB_CMDBUF_BASE + 2097153,cmodel_ref_buf_alignment);//cmdbuf addr space:2M, 8k*256
  } else {
  HW_TB_PP_LUMA_BASE = NEXT_MULTIPLE(HW_TB_BSD_CTRL_BASE_C1 + NEXT_MULTIPLE(20*16,16),cmodel_ref_buf_alignment);
  }

/*
  printf("HW_TB_DPB_BASE:%x\n",*HW_TB_DPB_BASE);
  printf("HW_TB_DPB_LUMA_BASE:%x\n",HW_TB_DPB_LUMA_BASE);
  printf("HW_TB_DPB_CHROMA_BASE:%x\n",HW_TB_DPB_CHROMA_BASE);
  printf("HW_TB_TILE_BASE:%x\n",HW_TB_TILE_BASE);
  printf("HW_TB_SCALELIST_BASE:%x\n",HW_TB_SCALELIST_BASE);
  printf("HW_TB_STRM_BASE:%x\n",HW_TB_STRM_BASE);
  printf("HW_TB_QTABLE_BASE:%x\n",HW_TB_QTABLE_BASE);
  printf("HW_TB_FILT_VER_BASE:%x\n",HW_TB_FILT_VER_BASE);
  printf("HW_TB_SAO_VER_BASE:%x\n",HW_TB_SAO_VER_BASE);
  printf("HW_TB_BSD_CTRL_BASE:%x\n",HW_TB_BSD_CTRL_BASE);
  printf("HW_TB_ALFTABLE_BASE:%x\n",HW_TB_ALFTABLE_BASE);
  printf("HW_TB_PP_LUMA_BASE:%x\n",HW_TB_PP_LUMA_BASE);
  */
}

#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
void dpi_mc_load_input_stream(u32 *reg_base,i32 core_id)
{
  u64 stream_base;
  if (core_id == 0){
    stream_base = HW_TB_STRM_BASE;
  } else {
    stream_base = HW_TB_STRM_BASE_C1;
  }
  u8* dpi_strm_addr;
  u32 len = reg_base[258];
  dpi_strm_addr = reg_base[169];
  sprintf(dwl_checkname, "WRITE_DEC_INDATA %llx %lx",stream_base,NEXT_MULTIPLE(len,16));
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(len,16);i++){
    if (i < len){
      dpi_mem_wr(stream_base++,*dpi_strm_addr++);
  	  //printf("write addr:%lx,data:%x;core:%x\n",stream_base,*dpi_strm_addr,core_id);
    }
    else if (len < NEXT_MULTIPLE(len,16)){
      dpi_mem_wr(stream_base++,0x0);
      //printf("write addr:%lx,data:%x;core:%x\n",stream_base,0x0,core_id);
    }
  }
}

void dpi_mc_load_input_alftable(u32 *reg_base,i32 core_id)
{
  u64 alftable_base;
  if (core_id == 0){
    alftable_base = HW_TB_ALFTABLE_BASE;
  }
  else{
    alftable_base = HW_TB_ALFTABLE_BASE_C1;
  }
  u8* dpi_alftable_addr;
  u32 size = 19*16;
  dpi_alftable_addr = (u8*)(MAKE_ADDR(reg_base[172],reg_base[173]));;

  sprintf(dwl_checkname, "WRITE_DEC_ALFTABLE %llx %x",alftable_base,size);
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(size,16);i++){
    if (i < size){
      dpi_mem_wr(alftable_base++,*dpi_alftable_addr++);
         //printf("write addr:%x,data:%x\n",alftable_base,*dpi_alftable_addr);
    }
    else if (size < NEXT_MULTIPLE(size,16)){
      dpi_mem_wr(alftable_base++,0x0);
      //printf("write addr:%x,data:%x\n",alftable_base,0x0);
    }
  }
}

void dpi_mc_load_input_scalelist(u32 *reg_base,i32 core_id)
{
  u64 scalelist_base;
  if (core_id == 0){
    scalelist_base = HW_TB_SCALELIST_BASE;
  }
  else{
    scalelist_base = HW_TB_SCALELIST_BASE_C1;
  }
  u8* dpi_scalelist_addr;
  u32 size;
  if (reg_base[3]>>27 == 15) {// g2 h264
    dpi_scalelist_addr = (u8*)(MAKE_ADDR(reg_base[174],reg_base[175]))+3680+144;
  } // qtable_base
  else{
  	dpi_scalelist_addr = (u8*)(MAKE_ADDR(reg_base[170],reg_base[171])); // scale_list_base
  }

  if(reg_base[3]>>27 == 15) size = 224; // g2 h264
  else if(reg_base[3]>>27 == 16)    size = 16+64; // avs2
  else                                      size = NextMultiple(16 + 6 * 16 + 2 * 6 * 64 + 2 * 64, TRACING_BUS_WIDTH);

  sprintf(dwl_checkname, "WRITE_DEC_SCALELIST %llx",scalelist_base);
  sprintf(dwl_checkname, "%s %x",dwl_checkname,size);
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(size,16);i++){
    dpi_mem_wr(scalelist_base++,*dpi_scalelist_addr++);
  }
}

void dpi_mc_load_input_seg_in(u32 *reg_base,i32 core_id)
{
  u64 seg_in_base;
  if (core_id == 0){
    seg_in_base = HW_TB_SEGMENT_IN_BASE;
  }
  else{
    seg_in_base = HW_TB_SEGMENT_IN_BASE_C1;
  }
  u8* dpi_seg_in_base;
  u32 pic_width_in_ctbs,pic_height_in_ctbs,tmp;
  tmp = (reg_base[12]>>10&0x7) - (reg_base[12]>>13&0x7);//max_cb_size - min_cb_size;
  pic_width_in_ctbs=(reg_base[4]>>19&0x1fff+(1<<tmp)-1)>>tmp;//(pic_width_in_cbs +(1<<tmp)-1)>>tmp
  pic_height_in_ctbs=(reg_base[4]>>6&0x1fff+(1<<tmp)-1)>>tmp;//(pic_height_in_cbs +(1<<tmp)-1)>>tmp
  u32 size = pic_width_in_ctbs * pic_height_in_ctbs * 32;
  dpi_seg_in_base = (u8*)(MAKE_ADDR(reg_base[80],reg_base[81]));

  sprintf(dwl_checkname, "WRITE_DEC_SEG_IN %llx",seg_in_base);
  sprintf(dwl_checkname, "%s %x",dwl_checkname,size);
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(size,16);i++){
    dpi_mem_wr(seg_in_base++,*dpi_seg_in_base++);
  }
}


void dpi_mc_load_input_probtabs(u32 *reg_base,i32 core_id)
{
  u64 probtabs_base;
  if (core_id == 0){
    probtabs_base = HW_TB_PROBTABS_BASE;
  }
  else{
    probtabs_base = HW_TB_PROBTABS_BASE_C1;
  }
  u8* dpi_probtabs_base;
  u32 size = sizeof(struct Vp9EntropyProbs);
  if (reg_base[3]>>27 == 15) {// g2 h264
    dpi_probtabs_base = (u8*)(MAKE_ADDR(reg_base[174],reg_base[175]));
  }
  else{
  	dpi_probtabs_base = (u8*)(MAKE_ADDR(reg_base[172],reg_base[173]));
  }

  sprintf(dwl_checkname, "WRITE_DEC_PROBTABS %llx",probtabs_base);
  sprintf(dwl_checkname, "%s %x",dwl_checkname,size);
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(size,16);i++){
    if (i < size){
      dpi_mem_wr(probtabs_base++,*dpi_probtabs_base++);
    }
    else if (size < NEXT_MULTIPLE(size,16)){
      dpi_mem_wr(probtabs_base++,0x0);
    }
  }
}

void dpi_mc_load_input_tiles(u32 *reg_base,i32 core_id)
{
  u64 tile_base;
  if (core_id == 0){
    tile_base = HW_TB_TILE_BASE;
  }
  else{
    tile_base = HW_TB_TILE_BASE_C1;
  }
  u8* dpi_tile_base;
  u32 size = NextMultiple((reg_base[10]>>12&0x1f) * (reg_base[10]>>17&0x7f) * 4, 16);// num_tile_rows_8k,num_tile_cols_8k
  dpi_tile_base = (u8*)(MAKE_ADDR(reg_base[166],reg_base[167]));

  sprintf(dwl_checkname, "WRITE_DEC_TILES %llx",tile_base);
  sprintf(dwl_checkname, "%s %x",dwl_checkname,size);
  dpi_display_info(dwl_checkname);

  for(int i=0;i<NEXT_MULTIPLE(size,16);i++){
    dpi_mem_wr(tile_base++,*dpi_tile_base++);
  }
}

void dpi_mc_load_input_qtable(u32 *reg_base,i32 core_id)
{
  u64 table_base;
  if (core_id == 0){
    table_base = HW_TB_QTABLE_BASE;
  }
  else{
  	table_base = HW_TB_QTABLE_BASE_C1;
  }
  u32* dpi_qtable_addr;
  u32 num_qtables = 0;
  if (reg_base[3]>>27 == 15) {// g2 h264
    dpi_qtable_addr = (u32*)(MAKE_ADDR(reg_base[174],reg_base[175]));
  }
  else{
  	dpi_qtable_addr = (u32*)(MAKE_ADDR(reg_base[172],reg_base[173]));
  }

  // cabac_e, poc_e,scaling_list_enable
  if ((reg_base[7]>>31) || (reg_base[3]>>9&0x1) ||
     (reg_base[5]>>24&0x1) ) {
     // 144 is for BUS width is 16 bytes
     num_qtables = (3680 + 6*16 + 2*64 + 144/* 34*4 */)/4;
  }

  for(int i=0;i < num_qtables;i++){
     u32 hex;
     hex = SWAP_U32(dpi_qtable_addr[i]);
  	 //printf("write addr:%x,data:%x\n",table_base,hex&0xFF);
     dpi_mem_wr(table_base++,hex);
  	 //printf("write addr:%x,data:%x\n",table_base,(hex&0xFF00)>>8);
     dpi_mem_wr(table_base++,hex >> 8);
  	 //printf("write addr:%x,data:%x\n",table_base,(hex&0xFF0000)>>16);
     dpi_mem_wr(table_base++,hex >> 16);
  	 //printf("write addr:%x,data:%x\n",table_base,(hex&0xFF000000)>>24);
     dpi_mem_wr(table_base++,hex >> 24);
  }
  if (num_qtables & 3) {
     u8 left;
     left = 4 - (num_qtables & 3);
     for (int i=0;i<left*4;i++){
        dpi_mem_wr(table_base++,0x0);
        //printf("write addr:%x,data:%x\n",table_base,0x0);
     }
  }
}

static u8* dpi_mc_Avs2FieldPpBufOffset(u32 *reg_base, u32 *offset) {
  u8* pp_base_virtual = NULL;
  u32 found = 0;
  u32 pic_ready = 0;
  u32 pp_lu_size, pp_ch_size, pp_buff_size = 0;
  u32 pp_h_luma, pp_h_chroma, in_stride = 0;
  u32 i;
  u32 ppu_enable_idx[MAX_NUM_PPUS] = {320,342,359,376};
  u32 ppu_height_idx[MAX_NUM_PPUS] = {332,354,371,388};
  u32 ppu_tiled_idx[MAX_NUM_PPUS] = {320,342,359,376};
  u32 ppu_out_stride_idx[MAX_NUM_PPUS] = {329,351,368,385};
  u32 ppu_out_format_idx[MAX_NUM_PPUS] = {322,344,361,378};
  u32 ppu_out_lu_idx[MAX_NUM_PPUS] = {326,348,365,382};
  for (i = 0; i < MAX_NUM_PPUS; i++) {
    if ((reg_base[ppu_enable_idx[i]]&0x1) == 1) {
      pp_h_luma = reg_base[ppu_height_idx[i]]&0xffff;
      pp_h_chroma = (reg_base[ppu_height_idx[i]]&0xffff) / 2;
      if ((reg_base[ppu_tiled_idx[i]]>>3 & 0x1) == 1) {
        pp_h_luma = NEXT_MULTIPLE(pp_h_luma, 4) / 4;
        pp_h_chroma = NEXT_MULTIPLE(pp_h_chroma, 4) / 4;
      }
      pp_lu_size = (reg_base[ppu_out_stride_idx[i]]&0xffff0000) * pp_h_luma;
      pp_ch_size = (reg_base[ppu_out_stride_idx[i]]&0xffff) * pp_h_chroma;
      pp_buff_size += (pp_lu_size + ((IS_PP_OUT_MONOCHROME(reg_base[ppu_out_format_idx[i]]>>18 & 0x1f)) ? 0 : pp_ch_size));
    }
  }
  pp_buff_size = NEXT_MULTIPLE(pp_buff_size, 16);
  for (i = 0; i < MAX_NUM_PPUS; i++) {
    if ((reg_base[ppu_enable_idx[i]]&0x1) == 1) {
      pp_lu_size = reg_base[ppu_out_stride_idx[i]]&0xffff0000;
      if ((reg_base[ppu_tiled_idx[i]]>>3 & 0x1) == 1) {
        pp_h_luma = NEXT_MULTIPLE(pp_h_luma, 4) / 4;
        pp_lu_size = (reg_base[ppu_out_stride_idx[i]]&0xffff0000) * pp_h_luma / 2;
      }
      if (!(reg_base[3]>>19&0x1)) {/*pic_topfield_e*/
        pp_base_virtual = (u64)(MAKE_ADDR(reg_base[ppu_out_lu_idx[i]],reg_base[ppu_out_lu_idx[i]])) - pp_lu_size;
      } else {
        pp_base_virtual = (u64)(MAKE_ADDR(reg_base[ppu_out_lu_idx[i]],reg_base[ppu_out_lu_idx[i]]));
      }
      break;
    }
  }
  pic_ready = (!(reg_base[3]>>23&0x1) || ((reg_base[3]>>23&0x1) && /*pic_interlace_e*/
               (((reg_base[4]>>5&0x1) && !(reg_base[3]>>19&0x1)) || /*top_fieldfirst_e && is_top_field*/
               (!(reg_base[4]>>5&0x1) && (reg_base[3]>>19&0x1)))));/*top_fieldfirst_e && is_top_field*/
  if (!pic_ready) {
    found = 0;
    for(i = 0; i < buf_index; i++) {
      if (buf_offset[i].pp_base == pp_base_virtual) {
        assert(buf_offset[i].valid == 0);
        buf_offset[i].valid = 1;
        buf_offset[i].offset = dwl_sim_buf_offset = (pic_buf_count % 9) * pp_buff_size;
        pic_buf_count++;
        found = 1;
      }
    }
    if (!found) {
      buf_offset[buf_index].pp_base = pp_base_virtual;
      buf_offset[buf_index].valid = 1;
      buf_offset[buf_index].offset = dwl_sim_buf_offset = (pic_buf_count % 9) * pp_buff_size;
      pic_buf_count++;
      buf_index++;
    }
  } else {
    found = 0;
    for(i = 0; i < buf_index; i++) {
      if (buf_offset[i].pp_base == pp_base_virtual) {
        assert(buf_offset[i].valid == 1);
        buf_offset[i].valid = 0;
        dwl_sim_buf_offset = buf_offset[i].offset;
        found = 1;
      }
    }
    if (!found)
      assert(0);
  }
  *offset = dwl_sim_buf_offset;
  return (pp_base_virtual);
}


void dpi_mc_load_input_swreg(u32 *reg_base,int core_id)
{
  int reg_idx=1;
  u64 buffer_base_addr;

  dpi_apb_op(reg_idx*4,reg_base[reg_idx] & ~0x1001,1); // clean bit 12 , dec int.
  for ( reg_idx = 2; reg_idx < 21; reg_idx++){
    if (reg_idx == 2)
      dpi_apb_op(reg_idx*4,(reg_base[reg_idx]|0x80) & 0xffff0fff,1); // tiled reference enable.disable tab swap.
    else if (reg_idx == 3 && tb_cfg.shaper_enable == 1)
      dpi_apb_op(reg_idx*4,reg_base[reg_idx]|0x8,1); // set sw_l2_shaper_e to 1
    else
      dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);
  }
  /* swreg 21 -> 29 not used */

  for (reg_idx = 30; reg_idx <= 39; reg_idx++) {
    dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);
  }

  /* swreg 40 -> 41 not used */

  for (reg_idx = 42; reg_idx < 50; reg_idx++) {
    dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);
  }

  for (reg_idx = 51; reg_idx < 54; reg_idx++) {
  	/*dec_mode == avs2*/
    if (reg_base[3]>>27 == 16) {
      dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);
    }
  }

  /* swreg 51 -> 54 not used */
  reg_idx = 55;
  dpi_apb_op(reg_idx*4,(reg_base[reg_idx]&0x0)|0x1,1);

  reg_idx = 58;
  dpi_apb_op(reg_idx*4,(reg_base[reg_idx] & ~0xff)|0x620f,1); //set max burst=16/bus width=128bit/axi id enable=1 

  reg_idx = 59;
  dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);

  reg_idx = 60;
  dpi_apb_op(reg_idx*4,0x0,1); // axi rd/wr id

  /* bases as offset from dpb start , luma */
  for (reg_idx = 64; reg_idx < 98; reg_idx += 2) {
    //dpi_apb_op(reg_idx*4,0x0,1); /* MSB not in use curently */

    /*dec_mode == vp9(13)*/
    if (reg_base[3]>>27 == 13 && reg_idx == 78) {
      if(core_id == 1 ) buffer_base_addr = HW_TB_SEGMENT_OUT_BASE_C1;
      else buffer_base_addr =  HW_TB_SEGMENT_OUT_BASE;
      dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
      //sprintf(comment, "BASE_DEC_SEGMENT_OUT");
    } else if (reg_base[3]>>27 == 13 && reg_idx == 80) {/*dec_mode == vp9(13)*/
      if(core_id == 1 ) buffer_base_addr = HW_TB_SEGMENT_IN_BASE_C1;
      else buffer_base_addr =  HW_TB_SEGMENT_IN_BASE;
      dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
      //sprintf(comment, "BASE_DEC_SEGMENT_IN");
    } else {
      tmp = (u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1]) - (addr_t)dpb_base_address);
      if ((u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1])) == 0)
        tmp = 0;

      //dpi_apb_op((reg_idx+1)*4,*HW_TB_DPB_BASE+tmp,1);
      dpi_dwl_write_swreg_64b(reg_idx,*HW_TB_DPB_BASE+tmp);
    }
  }

  /* bases as offset from dpb start , chroma */
  for (reg_idx = 98; reg_idx < 132; reg_idx += 2) {

    /* LSB */
    tmp = (u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1]) - (addr_t)dpb_base_address);
    /* avoid the confusion, the buffer regs not set, will directly set its offet to 0,
             otherwise the address offset will be strange minus value */
    if ((u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1])) == 0)
      tmp = 0;

    dpi_dwl_write_swreg_64b(reg_idx,*HW_TB_DPB_BASE+tmp);
  }

  /* bases as offset from dpb start , dmv */
  for (reg_idx = 132; reg_idx < 166; reg_idx += 2) {

    /* LSB */
    tmp = (u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1]) - (addr_t)dpb_base_address);
    /* avoid the confusion, the buffer regs not set, will directly set its offet to 0
             otherwise the address offset will be strange minus value */
    if ((u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1])) == 0)
      tmp = 0;

    dpi_dwl_write_swreg_64b(reg_idx,*HW_TB_DPB_BASE+tmp);
  }

  /* MSB not in use curently */
  reg_idx = 166;
  if(core_id == 1 ) buffer_base_addr = HW_TB_TILE_BASE_C1;
  else buffer_base_addr =  HW_TB_TILE_BASE;
  dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);

  /* MSB not in use curently */
  reg_idx = 168;
  if(core_id == 1 ) buffer_base_addr = HW_TB_STRM_BASE_C1;
  else buffer_base_addr =  HW_TB_STRM_BASE;
  dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);

  /* MSB not in use curently */
  reg_idx = 170;
  if(core_id == 1 ) buffer_base_addr = HW_TB_CTX_COUNTER_BASE_C1;
  else buffer_base_addr =  HW_TB_CTX_COUNTER_BASE;
  if (reg_base[3]>>27 == 13) {/*dec_mode == vp9(13)*/
    dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
    //sprintf(comment, "BASE_DEC_CTX_COUNTER");
  } else if (reg_base[3]>>27 == 12 || reg_base[3]>>27 == 16 ) {/*dec_mode == hevc(12) || dec_mode == avs2(16)*/
    if(core_id == 1 ) buffer_base_addr = HW_TB_SCALELIST_BASE_C1;
    else buffer_base_addr =  HW_TB_SCALELIST_BASE;
    dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
  } else {
    dpi_apb_op(reg_idx*4,0x0,1);
  }

  /* MSB not in use curently */
  reg_idx = 172;
  if (reg_base[3]>>27 == 13) {/*VP9*/
  	if(core_id == 1 ) buffer_base_addr = HW_TB_PROBTABS_BASE_C1;
    else buffer_base_addr =  HW_TB_PROBTABS_BASE;
    dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
    //sprintf(comment, "BASE_DEC_PROBTABS");
  } else if(reg_base[3]>>27 == 16){/*AVS2*/
    if(core_id == 1 ) buffer_base_addr = HW_TB_ALFTABLE_BASE_C1;
    else buffer_base_addr =  HW_TB_ALFTABLE_BASE;
    dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);
  } else {
    dpi_apb_op(reg_idx*4,0x0,1);
  }

  /* MSB not in use curently */
  reg_idx = 174;
  if(core_id == 1 ) buffer_base_addr = HW_TB_QTABLE_BASE_C1;
  else buffer_base_addr =  HW_TB_QTABLE_BASE;
  dpi_dwl_write_swreg_64b(reg_idx,buffer_base_addr);

  if (reg_base[3]>>27 == 15) {/*G2_H264*/
    //sprintf(comment, "BASE_DEC_QTABLES");
    //fprintf(fid, fmt, "W", 175, comment);
  } else {
    //sprintf(comment, "BASE_DEC_RASTER_Y");
    //fprintf(fid, fmt, "W", 175, comment);

    /* MSB not in use curently */
    //sprintf(comment, "%08X", 0);
    //fprintf(fid, fmt, "W", 176, comment);

    //sprintf(comment, "BASE_DEC_RASTER_C");
    //fprintf(fid, fmt, "W", 177, comment);

  /* MSB not in use curently */
    //sprintf(comment, "%08X", 0);
    //fprintf(fid, fmt, "W", 178, comment);

    //sprintf(comment, "BASE_DEC_FILT_VER+%x", 0);
    //fprintf(fid, fmt, "W", 179, comment);

    /* MSB not in use curently */
    //sprintf(comment, "%08X", 0);
    //fprintf(fid, fmt, "W", 180, comment);

    //sprintf(comment, "BASE_DEC_SAO_VER+%x", 0);
    //fprintf(fid, fmt, "W", 181, comment);

    /* MSB not in use curently */
    //sprintf(comment, "%08X", 0);
    //fprintf(fid, fmt, "W", 182, comment);

    //sprintf(comment, "BASE_DEC_BSD_CTRL+%x", 0);
    //fprintf(fid, fmt, "W", 183, comment);
  }

  if((reg_base[3]>>8 & 0x1) == 0) {/*ec_bypass*/
    /* EC compressor registers. */
    for (reg_idx = 189; reg_idx < 257; reg_idx+=2) {

      /* LSB */
      tmp = (u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1]) - (addr_t)dpb_base_address);
      /* avoid the confusion, the buffer regs not set, will directly set its offet to 0
                otherwise the address offset will be strange minus value */
      if ((u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1])) == 0)
        tmp = 0;

      dpi_dwl_write_swreg_64b(reg_idx,*HW_TB_DPB_BASE+tmp);
    }
  }

  /* Stream buffer length */
  reg_idx = 258;
  dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);

  /* stream start offset */
  reg_idx = 259;
  dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);

  /* axi_wr_4k_dis and outstanding depth */
  reg_idx = 265;
  if (tb_cfg.shaper_enable)
    dpi_apb_op(reg_idx*4,reg_base[reg_idx]|0x80000000,1);
  else
    dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);

  reg_idx = 266;
  dpi_apb_op(reg_idx*4,0x0,1); // needs let tb setting this reg.


  /* decoder stride register */
  reg_idx = 314;
  dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);

  for (reg_idx = 317; reg_idx < 320; reg_idx++) {
    dpi_apb_op(reg_idx*4,reg_base[reg_idx],1);
  }

  /* 4 channel PP regs:
  PP0: reg320-312(j=0~12), PP1: reg342-reg354(j=0~12), PP1: reg359-reg371(j=0~12), PP1: reg376-reg388(j=0~12), no FIELD mode regs*/
  u32 base=0;u8 i,j,allocate_pp_offset=1;
  addr_t dwl_pp_base_address = 0;
  if (reg_base[320]&0x1)/*pp_out_e*/
    dwl_pp_base_address = (addr_t)MAKE_ADDR(reg_base[325],reg_base[326]);/*PP_OUT_LU_BASE*/
  else if ((reg_base[342]&0x1) == 1)/*pp1_out_e*/
    dwl_pp_base_address = (addr_t)MAKE_ADDR(reg_base[347],reg_base[348]);/*PP1_OUT_LU_BASE*/
  else if ((reg_base[359]&0x1) == 1)/*pp2_out_e*/
    dwl_pp_base_address = (addr_t)MAKE_ADDR(reg_base[364],reg_base[365]);/*PP2_OUT_LU_BASE*/
  else if ((reg_base[376]&0x1) == 1)/*pp3_out_e*/
    dwl_pp_base_address = (addr_t)MAKE_ADDR(reg_base[381],reg_base[382]);/*PP3_OUT_LU_BASE*/
  if (reg_base[3]>>27 == 16 && (reg_base[3]>>23&0x1) == 1) {/*AVS2 & interlace_e*/
    //allocate_pp_offset = 0;
    u8* pp_base_virtual = NULL;
    pp_base_virtual = dpi_mc_Avs2FieldPpBufOffset(reg_base, &dwl_sim_buf_offset);
    dwl_pp_base_address = (addr_t) pp_base_virtual;
  }
  //printf("pp_base_address=%lx sim_buf_offset=%0x filed enable=%0d \n",pp_base_address,sim_buf_offset, in->field_sequence);

  for(i=0;i<4;i++){
    if(tb_cfg.pp_units_params[i].unit_enabled){
      if(i==0) base=320;         //pp0 base reg320
      else base=342+(i-1)*17;

      for(j=0;j<=12;j++){
        reg_idx=base+j;
        if(j==5 || j==7){        // pp out addr reg
          j=j+1;
          tmp = (u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1]));
          if ((u64)(MAKE_ADDR(reg_base[reg_idx], reg_base[reg_idx + 1])) == 0) tmp = 0;
          dpi_dwl_write_swreg_64b(reg_idx,HW_TB_PP_LUMA_BASE+tmp-dwl_pp_base_address+dwl_sim_buf_offset);
          //printf("j=%0d HW_TB_PP_LUMA_BASE=%lx tmp=%lx\n",j,HW_TB_PP_LUMA_BASE,tmp);
        }
        else if(j!=1){           // not swap reg
          dpi_apb_op((reg_idx)*4,reg_base[reg_idx],1);
        }
      }
    }
  }

  /* enable decoder */
  dpi_apb_op(4,(reg_base[1] & ~0x1001)|0x1,1); // clean bit 12, dec int.
  
}

void dpi_dwl_write_swreg_64b(u32 reg_num,u64 data)
{
  tmp32=data >> 32;
  dpi_apb_op(reg_num*4,tmp32,1);
  tmp32=data & 0xffffffff;
  dpi_apb_op((reg_num+1)*4,tmp32,1);
}

void dpi_mc_check_recon_buf(u32 *reg_base, const char* op)
{
  u32 pixel_width;
  u64 hw_recon_base;
  u8 *sw_recon_base;
  pixel_width = (((reg_base[8]>>6&0x3)+8) == 8 && ((reg_base[8]>>4&0x3)+8) == 8) ? 8 : 10;

  tmp = (u64)(MAKE_ADDR(reg_base[64], reg_base[65])) - (addr_t)dpb_base_address;
  if ((u64)(MAKE_ADDR(reg_base[64], reg_base[65])) == 0) tmp = 0;

  hw_recon_base = NEXT_MULTIPLE(*HW_TB_DPB_BASE+tmp,cmodel_ref_buf_alignment);

  bytes_y = ((reg_base[314]&0xffff0000)>>16) * (((reg_base[4]>>6)&0x1fff) << ((reg_base[12]>>13)&0x7));
  if (!(reg_base[3]>>8 & 0x1)) /*ec_bypass*/
  	bytes_y = bytes_y / 8;
  else
  	bytes_y = bytes_y / 4;
  
  if (!(reg_base[3]>>8 & 0x1)) { /*ec_bypass*/
    if(op=="CHECK"){
      sprintf(dwl_checkname, "CHECK_DEC_PIC_RFC_Y,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_y,16));
	  dpi_display_info(dwl_checkname);
    }
  }
  else {
    
    if(op=="CHECK"){
      sprintf(dwl_checkname, "CHECK_DEC_PIC_FRM_Y,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_y,16));
      dpi_display_info(dwl_checkname);
    }
  }
  sw_recon_base = (u64)(MAKE_ADDR(reg_base[64], reg_base[65]));
  if(op=="FLUSH") { 
    sprintf(dwl_checkname, "FLUSH RECON LU,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_y,16));
    dpi_display_info(dwl_checkname);
    dpi_mem_flush(hw_recon_base,NEXT_MULTIPLE(bytes_y,16)); 
  }
  else { // do check
    for(int i=0;i<NEXT_MULTIPLE(bytes_y,16);i++){
      if (i < bytes_y){
        dpi_mem_check(hw_recon_base++,*sw_recon_base++,"RECON LUMA");
         //printf("write addr:%x,data:%x\n",hw_recon_base,*sw_recon_base);
      }
      else if (bytes_y < NEXT_MULTIPLE(bytes_y,16)){
        dpi_mem_check(hw_recon_base++,0x0,"RECON LUMA");
        //printf("write addr:%x,data:%x\n",stream_base,0x0);
      }
    }
  }
  if((reg_base[7]>>30&0x1) == 1){ // blackwhite_e
    return;
  }

  tmp = (u64)(MAKE_ADDR(reg_base[98], reg_base[99])) - (addr_t)dpb_base_address;
  if ((u64)(MAKE_ADDR(reg_base[98], reg_base[99])) == 0) tmp = 0;
  hw_recon_base = NEXT_MULTIPLE(*HW_TB_DPB_BASE+tmp,cmodel_ref_buf_alignment);

  bytes_c = (reg_base[314]&0xffff) * (((reg_base[4]>>6)&0x1fff) << ((reg_base[12]>>13)&0x7));
  if (!(reg_base[3]>>8 & 0x1)) /*ec_bypass*/
  	bytes_c = bytes_c / 16;
  else
  	bytes_c = bytes_c / 8;

  if (op=="CHECK"){
    if(!(reg_base[3]>>8 & 0x1)){/*ec_bypass*/
      sprintf(dwl_checkname, "CHECK_DEC_PIC_RFC_C,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_c,16));
      dpi_display_info(dwl_checkname);
    } else {
      sprintf(dwl_checkname, "CHECK_DEC_PIC_FRM_C,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_c,16));
      dpi_display_info(dwl_checkname);
    }
  }
  sw_recon_base = (u64)(MAKE_ADDR(reg_base[98], reg_base[99]));
  if(op=="FLUSH") { 
    sprintf(dwl_checkname, "FLUSH RECON CH,addr: 0x%llx,size: 0x%x",hw_recon_base,NEXT_MULTIPLE(bytes_c,16));
    dpi_display_info(dwl_checkname);
    dpi_mem_flush(hw_recon_base,NEXT_MULTIPLE(bytes_c,16)); 
  }
  else {
    for(int i=0;i<NEXT_MULTIPLE(bytes_c,16);i++){
      if (i < bytes_c){
        dpi_mem_check(hw_recon_base++,*sw_recon_base++,"RECON CHROMA");
         //printf("write addr:%x,data:%x\n",stream_base,*dpi_strm_addr);
      }
      else if (bytes_c < NEXT_MULTIPLE(bytes_c,16)){
        dpi_mem_check(hw_recon_base++,0x0,"RECON CHROMA");
        //printf("write addr:%x,data:%x\n",stream_base,0x0);
      }
    }
  }

  // rfc_tbl check
  if (g_hw_ver > 10001 && !(reg_base[3]>>8 & 0x1)) {/*ec_bypass*/
    u64 hw_rfc_tbl_base;
    u8 *sw_rfc_tbl_base;
    u32 pic_width, pic_height;
    u32 pic_width_in_cbs, pic_height_in_cb;
    u32 table_stride;
    u32 mono_chroma = (reg_base[7]>>30&0x1 == 1);
    pic_width = (reg_base[4]>>19&0x1fff) << (reg_base[12]>>13&0x7);//in->pic_width_in_cbs << in->min_cb_size;
    pic_height = (reg_base[4]>>6&0x1fff) << (reg_base[12]>>13&0x7);//in->pic_height_in_cbs << in->min_cb_size;

    //fidt = trace_files[VC8000D_OUTPUT_RFC_TBL_BIN].fid;
    /* compress luma frame */
    tmp = (u64)(MAKE_ADDR(reg_base[189], reg_base[190]) - (addr_t)dpb_base_address);
    if ((u64)(MAKE_ADDR(reg_base[189], reg_base[190])) == 0) tmp = 0;
    hw_rfc_tbl_base = NEXT_MULTIPLE(*HW_TB_DPB_BASE+tmp,cmodel_ref_buf_alignment);

    pic_width_in_cbs = (pic_width + LUM_CBS_WIDTH - 1) / LUM_CBS_WIDTH;
    pic_height_in_cb = pic_height / LUM_CB_HEIGHT;
    table_stride = NEXT_MULTIPLE(pic_width_in_cbs, 16);

    //fwrite(tbl_luma, 1, table_stride * pic_height_in_cb, fidt);
    sw_rfc_tbl_base = MAKE_ADDR(reg_base[189],reg_base[190]);//in->tbl_luma;
    bytes_y =NEXT_MULTIPLE(table_stride*pic_height_in_cb,cmodel_ref_buf_alignment);
    if(op=="CHECK"){
      sprintf(dwl_checkname, "CHECK_DEC_FRM_TBL_Y,addr: 0x%llx,size: 0x%x",hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_y,16));
      dpi_display_info(dwl_checkname);
    }
	if(op=="FLUSH") {
	  sprintf(dwl_checkname, "FLUSH RFC LU TBL,addr: 0x%llx,size: 0x%x",hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_y,16));
      dpi_display_info(dwl_checkname);
	  dpi_mem_flush(hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_y,16)); 
	}
	else{
      for(int i=0;i<NEXT_MULTIPLE(bytes_y,16);i++){
        if (i < bytes_y)                             dpi_mem_check(hw_rfc_tbl_base++,*sw_rfc_tbl_base++,"RFC LUMA TBL");
        else if (bytes_y < NEXT_MULTIPLE(bytes_y,16))  dpi_mem_check(hw_rfc_tbl_base++,0x0,"RFC LUMA TBL");
      }
	}

    /* compress chroma frame */
    pic_width_in_cbs = (pic_width + CH_CBS_WIDTH - 1)/ CH_CBS_WIDTH;
    pic_height_in_cb = (pic_height/2) / CH_CB_HEIGHT;
    table_stride = NEXT_MULTIPLE(pic_width_in_cbs, 16);

    tmp = (u64)(MAKE_ADDR(reg_base[223], reg_base[224]) - (addr_t)dpb_base_address);
    if ((u64)(MAKE_ADDR(reg_base[223], reg_base[224])) == 0) tmp = 0;
    hw_rfc_tbl_base = NEXT_MULTIPLE(*HW_TB_DPB_BASE+tmp,cmodel_ref_buf_alignment);

    if (!mono_chroma){
      //fwrite(tbl_chroma, 1, table_stride * pic_height_in_cb, fidt);
      sw_rfc_tbl_base = MAKE_ADDR(reg_base[223],reg_base[224]);//in->tbl_chroma;
      bytes_c =NEXT_MULTIPLE(table_stride*pic_height_in_cb,cmodel_ref_buf_alignment);
      if(op=="CHECK"){
        sprintf(dwl_checkname, "CHECK_DEC_FRM_TBL_C,addr: 0x%llx,size: 0x%x",hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_c,16));
        dpi_display_info(dwl_checkname);
      }
	  if(op=="FLUSH") { 
	  	 sprintf(dwl_checkname, "FLUSH RFC CH TBL,addr: 0x%llx,size: 0x%x",hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_c,16));
        dpi_display_info(dwl_checkname);
	  	 dpi_mem_flush(hw_rfc_tbl_base,NEXT_MULTIPLE(bytes_c,16)); 
	  }
	  else{
        for(int i=0;i<NEXT_MULTIPLE(bytes_c,16);i++){
          if (i < bytes_c)                             dpi_mem_check(hw_rfc_tbl_base++,*sw_rfc_tbl_base++,"RFC CHROMA TBL");
          else if (bytes_c < NEXT_MULTIPLE(bytes_c,16))  dpi_mem_check(hw_rfc_tbl_base++,0x0,"RFC CHROMA TBL");
        }
	  }
    }
  }
}



void dpi_mc_stimulus_load(u32 *reg_base,int g_frame_num,i32 core_id)
{
  dpi_store_frame_info(g_frame_num,"cur_pic_num");
  // store MBs info to HW TB
  //sw_ctrl.pic_width_in_cbs << sw_ctrl.min_cb_size * sw_ctrl.pic_height_in_cbs << sw_ctrl.min_cb_size
  int tmp=(NextMultiple((reg_base[4]>>19&0x1fff) << (reg_base[12]>>13&0x7), 16)/16)*(NextMultiple((reg_base[4]>>6&0x1fff) << (reg_base[12]>>13&0x7), 16)/16);
  dpi_store_frame_info(tmp,"MBs");
  tmp=(reg_base[4]>>19&0x1fff) << (reg_base[12]>>13&0x7);
  dpi_store_frame_info(tmp,"pic_width");
  tmp=(reg_base[4]>>6&0x1fff) << (reg_base[12]>>13&0x7);
  dpi_store_frame_info(tmp,"pic_height");

  dpi_store_frame_info(core_id,"cur_core_no");
  if(core_id == 0) dpi_hs_status(1);
  else dpi_hs_status_c1(1);

  if(l2_allocate_buf == 1){
    dpi_l2_update_buf_base(reg_base);
    l2_allocate_buf=0;
  }

  // flush recon buffer and pp buffer if needed
  if(!(reg_base[3]>>8 & 0x1)) dpi_mc_check_recon_buf(reg_base,"FLUSH"); // only rfc mdoe, ec_bypass

  // load input strm to HW TB.
  dpi_mc_load_input_stream(reg_base,core_id);

  // load qtable, g2 h264
  if ((reg_base[7]>>31 & 0x1 == 1) && (reg_base[3]>>27 == 15)){
    dpi_mc_load_input_qtable(reg_base,core_id);
  }

  // load input tiles
  if (reg_base[10]>>1 & 0x1){
    dpi_mc_load_input_tiles(reg_base,core_id);
  }

  // load input probtabs
  if(reg_base[3]>>27 == 13){ // vp9
    dpi_mc_load_input_probtabs(reg_base,core_id);
  }

  if((reg_base[3]>>27 == 13) && (reg_base[13]&0x1) && ~(reg_base[8]>>16&0x1)){ // vp9 , segment_e, idr_pic_e
    dpi_mc_load_input_seg_in(reg_base,core_id);
  }

  if(reg_base[5]>>24&0x1){// scale_list_enable, sw_qscale_type
    dpi_mc_load_input_scalelist(reg_base,core_id);
  }

  if((reg_base[3]>>27 == 16) && (reg_base[7]>>26&0x7 != 0)){
    dpi_mc_load_input_alftable(reg_base,core_id);
  }

  // set swregs to HW TB.
  dpi_mc_load_input_swreg(reg_base,core_id);

}

void dpi_mc_output_check(const u32 *core_reg_base,i32 core_id)
{
  int i;
  dpi_store_frame_info(core_id,"cur_core_no");
  dpi_wait_int(core_reg_base[1]|1<<8); // dec rdy int has been clean after Cmodel finish.
  // check recon buffer
  dpi_mc_check_recon_buf(core_reg_base,"CHECK");

  for (i = 0; i < 4; i++){
    if(tb_cfg.pp_units_params[i].unit_enabled){
      dpi_mc_check_pp_buf(core_reg_base, i,"CHECK");
      #ifdef SUPPORT_DEC400
      dpi_mc_check_pp_buf(core_reg_base, i,"FLUSH");
      #endif
    }
  }

  if (core_id == 0) dpi_hs_status(3);
  else dpi_hs_status_c1(3);
}

void dpi_mc_check_pp_buf (u32 *reg_base,int ppu_idx,const char * op)
{
  u32 pixel_width = (((reg_base[8]>>6&0x3)+8) == 8 && ((reg_base[8]>>4&0x3)+8) == 8) ? 8 : 10;
  // sw_pp_out_format == YUV420-8bit or yuv400-8bit or IYUV-8bit
  u32 pp_out_8bit = (reg_base[322]>18&0x1f == 3 || reg_base[322]>18&0x1f == 6 || reg_base[322]>18&0x1f == 9) ? 1 : 0;
  u32 out_bitdepth = ( pp_out_8bit || pixel_width == 8) ? 8 :
                     (reg_base[322]>18&0x1f == 1) ? 16 : pixel_width;
  u32 pic_w = (reg_base[4]>>19&0x1fff) << (reg_base[12]>>13&0x7);//sw_ctrl->pic_width_in_cbs << sw_ctrl->min_cb_size;
  u32 pic_h = (reg_base[4]>>6&0x1fff) << (reg_base[12]>>13&0x7);//sw_ctrl->pic_height_in_cbs << sw_ctrl->min_cb_size;
  u32 ds_x_ratio = 1;
  u32 ds_y_ratio = 1;
  u32 ds_pic_h_luma;
  u32 ds_pic_h_chroma;
  u32 pp_stride, pp_stride_ch = 0;
  u32 chroma_size;
  u8* luma_base;
  u8* chroma_base;

  u32 pp_out_stride[4] = {329,351,368,385};
  u32 pp_out_height[4] = {332,354,371,388};
  u32 pp_out_lu_base[4] = {326,348,365,382};
  u32 pp_out_ch_base[4] = {328,350,367,384};
  u32 pp_out_ctrl[4] = {320,342,359,376};

  //if (g_hw_ver > 10001 || /* VC8000D */
  //    g_hw_id == 0x3090) {  /* G2V4_3090 (ST) */
    u32 planar = (reg_base[322]>18&0x1f >= 7 && reg_base[322]>18&0x1f <= 9); // sw_pp_out_format
    u32 dscale_y = MAKE_ADDR(reg_base[pp_out_lu_base[ppu_idx]-1], reg_base[pp_out_lu_base[ppu_idx]]);//sw_ctrl->dscale_y = sw_ctrl->ppu[index].dscale_y;
    u32 dscale_c = MAKE_ADDR(reg_base[pp_out_ch_base[ppu_idx]-1], reg_base[pp_out_ch_base[ppu_idx]]);//sw_ctrl->dscale_c = sw_ctrl->ppu[index].dscale_c;
    pp_stride = reg_base[pp_out_stride[ppu_idx]]>>16&0xffff;//sw_ctrl->pp_y_stride;
    pp_stride_ch = reg_base[pp_out_stride[ppu_idx]]&0xffff;//sw_ctrl->pp_c_stride;
    ds_pic_h_luma = reg_base[pp_out_height[ppu_idx]]&0xffff;//sw_ctrl->pp_out_height;
    ds_pic_h_chroma = ds_pic_h_luma / 2;
    if (reg_base[pp_out_ctrl[ppu_idx]]>>3&0x1 == 1){ //(sw_ctrl->ppu[index].pp_tiled_e) {
      ds_pic_h_luma = NEXT_MULTIPLE(ds_pic_h_luma, 4) / 4;
      ds_pic_h_chroma = NEXT_MULTIPLE(ds_pic_h_chroma, 4) / 4;
    }
  //} else {  /* Legacy G2V4 */
  /*
    ds_x_ratio = sw_ctrl->dscale_ratio_x + 1;
    ds_y_ratio = sw_ctrl->dscale_ratio_y + 1;
    pp_stride = NextMultiple((pic_w >> ds_x_ratio) * out_bitdepth, 16 * 8) / 8;
    ds_pic_h_luma = pic_h >> ds_y_ratio;
    ds_pic_h_chroma = pic_h >> ds_y_ratio >> 1;
    */
  //}
  if ( !dscale_y || !dscale_c || ds_x_ratio > 3 || ds_y_ratio > 3)
    return;
  if (reg_base[3]>>27 == 16 && reg_base[3]>>23&0x1 == 1 && !(reg_base[3]>>19&0x1)){//(sw_ctrl->dec_mode == DEC_MODE_AVS2 && sw_ctrl->field_sequence && !sw_ctrl->is_top_field) {
    luma_base = dscale_y - pp_stride;
    chroma_base = dscale_c - pp_stride_ch;
  } else {
    luma_base = dscale_y;
    chroma_base = dscale_c;
  }
  chroma_size = NEXT_MULTIPLE((ds_pic_h_luma * pp_stride +
                             (planar ? (ds_pic_h_chroma * pp_stride_ch * 2) : (ds_pic_h_chroma * pp_stride_ch))), 16) -
                             ds_pic_h_luma * pp_stride;
  u64 hw_pp_base;
  u32 pp_reg_idx;
  char bufname[10]="";

  if(ppu_idx==0) pp_reg_idx=325;
  else if(ppu_idx==1) pp_reg_idx=347;
  else if(ppu_idx==2) pp_reg_idx=364;
  else if(ppu_idx==3) pp_reg_idx=381;

  //if (reg_base[pp_out_ctrl[ppu_idx]]&0x1 ==1)//(sw_ctrl->ppu[0].scale_enabled)
  dwl_pp_base_address = MAKE_ADDR(reg_base[325], reg_base[326]); // PPU0 
  
  if (reg_base[3]>>27 == 16 && reg_base[3]>>23&0x1 == 1){//(sw_ctrl->dec_mode == DEC_MODE_AVS2 && sw_ctrl->field_sequence) {
    //allocate_pp_offset = 0;
    u8* pp_base_virtual = NULL;
    pp_base_virtual = dpi_mc_Avs2FieldPpBufOffset(reg_base, &dwl_sim_buf_offset);
    dwl_pp_base_address = (addr_t) pp_base_virtual;
  }

  sprintf(bufname, "PP%0d LUMA", ppu_idx);
  tmp = (u64)(MAKE_ADDR(reg_base[pp_reg_idx], reg_base[pp_reg_idx + 1]));
  if ((u64)(MAKE_ADDR(reg_base[pp_reg_idx], reg_base[pp_reg_idx + 1])) == 0) tmp = 0;
  if (reg_base[3]>>27 == 16 && reg_base[3]>>23&0x1 == 1)//(sw_ctrl->dec_mode == DEC_MODE_AVS2 && sw_ctrl->field_sequence)
    hw_pp_base=HW_TB_PP_LUMA_BASE+tmp-dwl_pp_base_address+dwl_sim_buf_offset-pp_stride;//sw_ctrl->ppu[index].pp_y_stride;
  else
    hw_pp_base=HW_TB_PP_LUMA_BASE+tmp-dwl_pp_base_address+dwl_sim_buf_offset;
  hw_pp_base=NEXT_MULTIPLE(hw_pp_base,cmodel_ref_buf_alignment);
  //printf("check 1) HW_TB_PP_LUMA_BASE=%08x, tmp=%08x pp_base_address=%08x pp_lu_size=%x\n",HW_TB_PP_LUMA_BASE,tmp,pp_base_address,sw_ctrl->ppu[index].pp_y_stride);
  
  //fwrite(sw_ctrl->dscale_y, 1, ds_pic_h_luma * pp_stride, fid);
  bytes_y=NEXT_MULTIPLE(ds_pic_h_luma * pp_stride,16);
  if(op=="FLUSH") { 
    sprintf(dwl_checkname, "FLUSH PPU %0d LUMA,addr: 0x%llx,size: 0x%x", ppu_idx,hw_pp_base,bytes_y);
    dpi_display_info(dwl_checkname);
    dpi_mem_flush(hw_pp_base,bytes_y); 
  }
  else {
    sprintf(dwl_checkname, "CHECK_PP_OUT%0d_LUMA,addr: 0x%llx,size: 0x%x", ppu_idx,hw_pp_base,bytes_y);
    dpi_display_info(dwl_checkname);
    for(int i=0;i<bytes_y;i++){
      if (i < bytes_y) dpi_mem_check(hw_pp_base++,*luma_base++,bufname);
      else if (bytes_y < NEXT_MULTIPLE(bytes_y,16)) dpi_mem_check(hw_pp_base++,0x0,bufname);
    }
  }

  if (reg_base[322]>18&0x1f >= 4 && reg_base[322]>18&0x1f <= 6){//(IS_PP_OUT_MONOCHROME(sw_ctrl->ppu[index].output_format)){
    return;
  }

  sprintf(bufname, "PP%0d CHROMA", ppu_idx);
  pp_reg_idx=pp_reg_idx+2;
  tmp = (u64)(MAKE_ADDR(reg_base[pp_reg_idx], reg_base[pp_reg_idx + 1]));
  if ((u64)(MAKE_ADDR(reg_base[pp_reg_idx], reg_base[pp_reg_idx + 1])) == 0) tmp = 0;
  if ((reg_base[3]>>27 == 16) && (reg_base[3]>>23&0x1 == 1))//(sw_ctrl->dec_mode == DEC_MODE_AVS2 && sw_ctrl->field_sequence)
    hw_pp_base=HW_TB_PP_LUMA_BASE+tmp-dwl_pp_base_address+dwl_sim_buf_offset-pp_stride_ch;//sw_ctrl->ppu[index].pp_c_stride;
  else
    hw_pp_base=HW_TB_PP_LUMA_BASE+tmp-dwl_pp_base_address+dwl_sim_buf_offset;
  hw_pp_base=NEXT_MULTIPLE(hw_pp_base,cmodel_ref_buf_alignment);
  //printf("check 1) HW_TB_PP_LUMA_BASE=%08x, tmp=%08x pp_base_address=%08x pp_ch_size=%x\n",HW_TB_PP_LUMA_BASE,tmp,pp_base_address, sw_ctrl->ppu[index].pp_c_stride);

  //fwrite(sw_ctrl->dscale_c, 1, chroma_size , fid);
  bytes_c=chroma_size;
  if(op=="FLUSH") { 
    sprintf(dwl_checkname, "FLUSH PPU %0d CHROMA,addr: 0x%llx,size: 0x%x",ppu_idx,hw_pp_base,bytes_c);
    dpi_display_info(dwl_checkname);
    dpi_mem_flush(hw_pp_base,NEXT_MULTIPLE(bytes_c,16)); 
  }
  else {
    sprintf(dwl_checkname, "CHECK_PP_OUT%0d_CHROMA,addr: 0x%llx,size: 0x%x",ppu_idx,hw_pp_base,bytes_c);
    dpi_display_info(dwl_checkname);
    for(int i=0;i<NEXT_MULTIPLE(bytes_c,16);i++){
      if (i < bytes_c) dpi_mem_check(hw_pp_base++,*chroma_base++,bufname);
      else if (bytes_c < NEXT_MULTIPLE(bytes_c,16)) dpi_mem_check(hw_pp_base++,0x0,bufname);
    }
  }
}

#endif // SUPPORT_MULTI_CORE
#endif // ASIC_ONL_SIM

