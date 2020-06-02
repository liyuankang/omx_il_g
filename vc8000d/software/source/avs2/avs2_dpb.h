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

#ifndef AVS2_DPB_H_
#define AVS2_DPB_H_

#include "basetype.h"
#include "avs2decapi.h"

#include "avs2_cfg.h"
#include "avs2_params.h"
#include "defines.h"

#if 0
#include "hevc_slice_header.h"
#include "hevc_image.h"
#include "hevc_sei.h"
#endif

#include "avs2_fb_mngr.h"
#include "dwl.h"

#define INIT_POC 0x7FFFFFFF

/* enumeration to represent status of buffered image */
enum DpbPictureStatus {
  UNUSED = 0,
  NON_EXISTING,
  SHORT_TERM,
  LONG_TERM,
  SHORT_LONG_TERM,
  EMPTY
};

/* structure to represent a buffered picture */
struct Avs2DpbPicture {
  u32 mem_idx;
  struct DWLLinearMem *data;
  struct DWLLinearMem *pp_data;
  i32 pic_num;
  int next_poc;
  u32 first_field;
  // i32 decode_order_cnt;
  // i32 pic_order_cnt;  //poi
  i32 pic_order_cnt_lsb;
  i32 img_poi;
  i32 img_coi;
  enum DpbPictureStatus status;
  u32 to_be_displayed;
  u32 pic_id;
  u32 decode_id;
  u32 num_err_mbs;
  Avs2PicType type;
  u32 is_tsa_ref;
  u32 tiled_mode;
  u32 cycles_per_mb;
  u32 ref_poc[MAX_DPB_SIZE + 1];
  u32 refered_by_others;
  u32 pic_struct;
  u32 is_field_sequence;
  u32 is_top_field;
  u32 top_field_first;
  double dpb_output_time;
};

/* structure to represent display image output from the buffer */
struct Avs2DpbOutPicture {
  u32 mem_idx;
  struct DWLLinearMem *data;
  struct DWLLinearMem *pp_data;
  u32 pic_id;
  u32 decode_id;
  u32 num_err_mbs;
  Avs2PicType type;
  u32 is_tsa_ref;
  u32 tiled_mode;
  u32 pic_width;
  u32 pic_height;
  u32 cycles_per_mb;
  struct Avs2CropParams crop_params;
  u32 pic_struct;
  u32 sample_bit_depth;
  u32 output_bit_depth;
  u32 is_field_sequence;
  u32 is_top_field;
  u32 top_field_first;
};

/* structure to represent DPB */
struct Avs2DpbStorage {
  struct Avs2DpbPicture buffer[MAX_DPB_SIZE + 1];  // reference frame buffer
  u32 list[MAX_DPB_SIZE + 1];
  struct Avs2DpbPicture *current_out;  // current frame pointer in buffer
  u32 current_out_pos;
  double cpb_removal_time;
  u32 bumping_flag;
  struct Avs2DpbOutPicture *out_buf;  // MAX_DPB_SIZE * output frame buffer
  u32 num_out;  // output buffer number
  u32 out_index_w;
  u32 out_index_r;
  u32 max_ref_frames;
  u32 dpb_size;
  u32 real_size;
  u32 dpb_reset;

  u32 max_long_term_frame_idx;
  u32 num_ref_frames;
  u32 fullness;
  u32 num_out_pics_buffered;  // pic buffer num need to output
  u32 prev_ref_frame_num;
  u32 last_contains_mmco5;
  u32 no_reordering;
  u32 flushed;
  u32 pic_size;
  u32 dir_mv_offset;
  u32 sync_mc_offset;
  struct DWLLinearMem poc;
  u32 delayed_out;
  u32 delayed_id;
  u32 interlaced;
  u32 ch2_offset;
  u32 cbs_tbl_offsety;
  u32 cbs_tbl_offsetc;
  u32 cbs_tbl_size;

  u32 tot_buffers;  // >= dpb->dpb_size + 2
  struct DWLLinearMem pic_buffers[MAX_FRAME_BUFFER_NUMBER];  // yuv picture +
                                                             // direct mode
                                                             // motion vectors
                                                             // + tbl
  u32 pic_buff_id[MAX_FRAME_BUFFER_NUMBER];  // each buffer[i]' s id in fb_list

  /* flag to prevent output when display smoothing is used and second field
   * of a picture was just decoded */
  u32 no_output;

  u32 prev_out_idx;

  i32 ref_poc_list[MAX_DPB_SIZE + 1]; /* TODO: check dimenstions */
  i32 poc_st_curr[MAX_DPB_SIZE + 1];
  i32 poc_st_foll[MAX_DPB_SIZE + 1];
  i32 poc_lt_curr[MAX_DPB_SIZE + 1];
  i32 poc_lt_foll[MAX_DPB_SIZE + 1];
  u32 ref_pic_set_st[MAX_DPB_SIZE + 1];
  u32 ref_pic_set_lt[MAX_DPB_SIZE + 1];
  u32 num_poc_st_curr;
  u32 num_poc_st_curr_before;
  u32 num_poc_st_foll;
  u32 num_poc_lt_curr;
  u32 num_poc_lt_foll;

  struct FrameBufferList *fb_list;
  u32 ref_id[MAX_DPB_SIZE + 1];
  u32 pic_width;
  u32 pic_height;
  u32 sample_bit_depth;
  u32 output_bit_depth;
  struct Avs2CropParams crop_params;

  // u32 buffer_index;   /* Next buffer to be released or allocated. */
  struct Avs2Storage *storage; /* TODO(min): temp use for raster buffer
                                  allocation. */

  /* avs2 */
  int prev_coi;  // last frame doi
  int refered_by_others;
  i32 poi;
  i32 coi;
  i32 curr_idr_poi;
  // i32 next_idr_poi;
  i32 curr_idr_coi;
  // i32 next_idr_coi;
  u32 num_of_ref;
  u32 num_to_remove;
  u32 output_cur_dec_pic;
  i32 curr_RPS_ref_pic[MAXREF];
  u32 remove_pic[MAXREF];
  i32 last_poi;

  u32 type;
  u32 typeb;
  u32 background_picture_enable;
  u32 background_picture_output_flag;
  u32 low_delay;
  i32 displaydelay;
  i32 picture_reorder_delay;

  /* error cancealment */
  i32 dpb_overflow_cnt;
  i32 no_free_buffer_cnt;
};

struct Avs2DpbInitParams {
  u32 pic_size;
  u32 buff_size;
  u32 dpb_size;
  u32 tbl_sizey;
  u32 tbl_sizec;
  u32 n_extra_frm_buffers;
  u32 no_reordering;
};

u32 Avs2ResetDpb(const void *dec_inst, struct Avs2DpbStorage *dpb,
                 struct Avs2DpbInitParams *dpb_params);

u32 Avs2ClearRefPics(struct Avs2DpbStorage *dpb);
u32 Avs2SetRefPics(struct Avs2DpbStorage *dpb, struct Avs2SeqParam *sps,
                   struct Avs2PicParam *pps);

void *Avs2AllocateDpbImage(struct Avs2DpbStorage *dpb, i32 pic_order_cnt,
                           u32 current_pic_id);

u8 *Avs2GetRefPicData(const struct Avs2DpbStorage *dpb, u32 index);

/*@null@*/
struct Avs2DpbOutPicture *Avs2DpbOutputPicture(struct Avs2DpbStorage *dpb);

void Avs2FlushDpb(struct Avs2DpbStorage *dpb);

i32 Avs2FreeDpb(const void *inst, struct Avs2DpbStorage *dpb);
i32 Avs2FreeDpbExt(const void *dec_inst, struct Avs2DpbStorage *dpb);


void Avs2DpbMarkOlderUnused(struct Avs2DpbStorage *dpb, i32 pic_order_cnt,
                            u32 hrd_present);
void Avs2EmptyDpb(const void *dec_inst, struct Avs2DpbStorage *dpb);

void Avs2DpbUpdateOutputList(struct Avs2DpbStorage *dpb,
                             struct Avs2PicParam *pps);
void Avs2DpbCheckMaxLatency(struct Avs2DpbStorage *dpb, u32 max_latency);
u32 Avs2DpbHrdBumping(struct Avs2DpbStorage *dpb);
void Avs2DpbRemoveUnused(struct Avs2DpbStorage *dpb);


#endif /* #ifdef HEVC_DPB_H_ */
