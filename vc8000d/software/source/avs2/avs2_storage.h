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

#ifndef HEVC_STORAGE_H_
#define HEVC_STORAGE_H_

#include "basetype.h"
#include "sw_stream.h"
#include "avs2_params.h"
#include "avs2hwd_api.h"

#if 0
#include "hevc_cfg.h"
#include "hevc_seq_param_set.h"
#include "hevc_pic_param_set.h"
#include "hevc_video_param_set.h"
#include "hevc_nal_unit.h"
#include "hevc_slice_header.h"
#include "hevc_seq_param_set.h"
#include "hevc_pic_order_cnt.h"
#include "hevc_sei.h"
#endif
#include "avs2_dpb.h"
#include "raster_buffer_mgr.h"

struct Image {
  struct DWLLinearMem *data;
  struct DWLLinearMem *pp_data;
  u32 width;
  u32 height;
  u32 pic_struct;
};

/* structure to store parameters needed for access unit boundary checking */
struct AubCheck {
  // struct NalUnit nu_prev[1];
  u32 prev_idr_pic_id;
  u32 prev_pic_order_cnt_lsb;
  u32 first_call_flag;
  u32 new_picture;
};

/* storage data structure, holds all data of a decoder instance */
struct Avs2Storage {
  /* parameter set */
  struct Avs2SeqParam sps;
  struct Avs2PicParam pps;
  struct Avs2ExtData ext;
  struct Avs2SeqParam sps_old;

  /* buffers */
  struct Avs2AsicBuffers cmems;
  struct Avs2BufferSpec buff_spec;

  /* io */
  struct Avs2ReconParam recon;
  struct Avs2PpoutParam ppout;
  struct Avs2RefsParam refs;
  struct Avs2StreamParam input;

  double frame_rate;

  /* flag to indicate if HW has be started for this picture before.
   * use for error canceal. */
  u32 pic_started;

  /* flag to indicate if current access unit contains any valid slices */
  u32 valid_slice_in_access_unit;

  /* pic_id given by application */
  u32 current_pic_id;

  /* flag to store no_output_reordering flag set by the application */
  u32 no_reordering;

  u32 top_field_first;

  /* pointer to DPB */
  struct Avs2DpbStorage dpb[2];

  /* structure to store picture order count related information */
  i32 poc[2];
  i32 pic_distance;

  /* access unit boundary checking related data */
  // struct AubCheck aub[1];

  /* current processed image */
  struct Image curr_image[1];

  /* last valid NAL unit header is stored here */
  // struct NalUnit prev_nal_unit[1];

  /* slice header, second structure used as a temporary storage while
   * decoding slice header, first one stores last successfully decoded
   * slice header */
  // struct SliceHeader slice_header[2];

  /* fields to store old stream buffer pointers, needed when only part of
   * a stream buffer is processed by HevcDecode function */
  u32 prev_buf_not_finished;
  const u8 *prev_buf_pointer;
  u32 prev_bytes_consumed;
  struct StrmData strm[1];

  u32 checked_aub;        /* signal that AUB was checked already */
  u32 prev_idr_pic_ready; /* for FFWD workaround */

  u32 intra_freeze;
  u32 picture_broken;

  i32 poc_last_display;

  u32 dmv_mem_size;
  u32 n_extra_frm_buffers;
  // const struct DpbOutPicture *pending_out_pic;

  u32 no_rasl_output;

  DecPicAlignment align;

  u32 raster_enabled;
  RasterBufferMgr raster_buffer_mgr;
  u32 pp_enabled;
  u32 down_scale_x_shift;
  u32 down_scale_y_shift;

  u32 use_p010_output;
  u32 use_8bits_output;
  u32 use_video_compressor;
#ifdef USE_FAST_EC
  u32 fast_freeze;
#endif
  u32 pic_size;
};

void Avs2InitStorage(struct Avs2Storage *storage);
void Avs2ResetStorage(struct Avs2Storage *storage);
u32 Avs2IsStartOfPicture(struct Avs2Storage *storage);
int Avs2DeriveBufferSpec(struct Avs2Storage *storage,
                         struct Avs2BufferSpec *info, DecPicAlignment align);
u32 Avs2StoreSeqParamSet(struct Avs2Storage *storage,
                         struct Avs2SeqParam *seq_param_set);
u32 Avs2StorePicParamSet(struct Avs2Storage *storage,
                         struct Avs2PicParam *pic_param_set);
// u32 Avs2StoreVideoParamSet(struct Avs2Storage *storage,
//                           struct VideoParamSet *video_param_set);
// u32 Avs2ActivateParamSets(struct Avs2Storage *storage, u32 pps_id, u32
// is_idr);
//
// u32 Avs2CheckAccessUnitBoundary(struct StrmData *strm, struct NalUnit
// *nu_next,
//                                struct Avs2Storage *storage,
//                                u32 *access_unit_boundary_flag);

u32 Avs2ValidParamSets(struct Avs2Storage *storage);
void Avs2ClearStorage(struct Avs2Storage *storage);
void Avs2SetRecon(struct Avs2Storage *storage, struct Reference *recon,
                  struct DWLLinearMem *data);
void Avs2SetPp(struct Avs2Storage *storage, struct Avs2PpoutParam *pp,
               PpUnitIntConfig *ppu_cfg, struct DecHwFeatures *hw_feature);
u32 Avs2StoreSEIInfoForCurrentPic(struct Avs2Storage *storage);

u32 Avs2IsExternalBuffersRealloc(void *dec_inst, struct Avs2Storage *storage);
void Avs2SetExternalBufferInfo(void *dec_inst, struct Avs2Storage *storage);

u32 Avs2AllocateSwResources(const void *dwl, struct Avs2Storage *storage,
                            void *dec_inst);
void Avs2SetRef(struct Avs2Storage *storage, struct Avs2RefsParam *refs,
                struct Avs2DpbStorage *dpb);

#endif /* #ifdef AVS2_STORAGE_H_ */
