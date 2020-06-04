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

#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>
#include <string.h>
#include "basetype.h"
#include "dwlthread.h"

/* common tools */
enum TraceDecodingMode {
  TRACE_H263 = 0,
  TRACE_H264 = 1,
  TRACE_MPEG1 = 2,
  TRACE_MPEG2 = 3,
  TRACE_MPEG4 = 4,
  TRACE_VC1 = 5,
  TRACE_JPEG = 6,
  TRACE_RV = 8,
  TRACE_H264_H10P = 9,
};

struct TracePicCodingType {
  u8 i_coded; /* intra coded */
  u8 p_coded; /* predictive coded */
  u8 b_coded; /* bidirectionally predictive coded */
};

struct TraceSequenceType {
  u8 interlaced;
  u8 progressive;
};

/* h.264 tools */
struct TraceH264StreamMode {
  u8 nal_unit_strm;
  u8 byte_strm;
};

struct TraceH264EntropyCoding {
  u8 cavlc;
  u8 cabac;
};

struct TraceH264ProfileType {
  u8 baseline;
  u8 main;
  u8 high;
};

struct TraceH264ScalingMatrixPresentType {
  u8 pic;
  u8 seq;
};

struct TraceH264WeightedPredictionType {
  u8 is_explicit;
  u8 explicit_b;
  u8 implicit;
};

struct TraceH264InterlaceType {
  u8 pic_aff;
  u8 mb_aff;
};

struct TraceH264SequenceType {
  u8 ilaced;
  u8 prog;
};

struct DecodingToolsTrc {
  u32 pcm;
  u32 min_pcm_size;
  u32 max_pcm_size;
  u32 cu_qp_delta;
  u32 lossless;
  u32 min_cu_size_i;
  u32 max_cu_size_i;
  u32 min_tu_size_i;
  u32 max_tu_size_i;
  u32 min_cu_size_p;
  u32 max_cu_size_p;
  u32 min_tu_size_p;
  u32 max_tu_size_p;
  u32 l0pred;
  u32 l1pred;
  u32 bipred;
  u32 weighted_pred;
  u32 strong_intra;
  u32 col_mv;
  u32 transform_skip;
  u32 dependent_slices;
  u32 sao;
  u32 amp;
  u32 sign_data_hiding;
  u32 entropy_sync_mem;
  u32 entropy_sync_init;
  u32 slice_types[3];
  u32 lossless_wht;

  /* g1 related tools */
  /* g1 common */
  enum TraceDecodingMode decoding_mode;
  struct TracePicCodingType pic_coding_type;
  struct TraceSequenceType sequence_type;

  /* h264 */
  struct TraceH264StreamMode stream_mode;
  struct TraceH264EntropyCoding entropy_coding;
  struct TraceH264ProfileType profile_type;
  struct TraceH264ScalingMatrixPresentType scaling_matrix_present_type;
  struct TraceH264WeightedPredictionType weighted_prediction_type;
  struct TraceH264InterlaceType interlace_type;
  struct TraceH264SequenceType seq_type;
  u8 slice_groups; /* more thant 1 slice groups */
  u8 arbitrary_slice_order;
  u8 redundant_slices;
  u8 weighted_prediction;
  u8 image_cropping;
  u8 monochrome;
  u8 scaling_list_present;
  u8 transform8x8;
  u8 loop_filter;
  u8 loop_filter_dis;
  u8 error;
  u8 intra_prediction8x8;
  u8 ipcm;
  u8 direct_mode;
  u8 multiple_slices_per_picture;

  /* jpeg tools */
  u8 sampling_4_2_0;
  u8 sampling_4_2_2;
  u8 sampling_4_0_0;
  u8 sampling_4_4_0;
  u8 sampling_4_1_1;
  u8 sampling_4_4_4;
  u8 thumbnail;
  u8 progressive;

  /* mpeg-1 & mpeg-2 tools */
  u8 d_coded; /* MPEG-1 specific */

  /* mpeg-4 & h.263 tools */
  u32 four_mv;
  u32 ac_pred;
  u32 data_partition;
  u32 resync_marker;
  u32 reversible_vlc;
  u32 hdr_extension_code;
  u32 q_method1;
  u32 q_method2;
  u32 half_pel;
  u32 quarter_pel;
  u32 short_video;

  /* vc-1 tools */
  u32 vs_transform;
  u32 overlap_transform;
  u32 qpel_luma;
  u32 qpel_chroma;
  u32 range_reduction;
  u32 intensity_compensation;
  u32 multi_resolution;
  u32 adaptive_mblock_quant;
  u32 range_mapping;
  u32 extended_mv;

  /* avs2 special */
  char frame_type;
  u32 background_ref_e;
  u32 slices;     /* slice number in this picture */
  u32 interlace;  /* 0: frame, 1: top, 2:bottom */
  u32 loopfilter_across_slice_e;
  u32 alf_e;
  u32 weighted_quant_enable;
  u32 pmvr_e;
  u32 multi_hypo_skip_e;
  u32 dual_hypo_pred_e;
  u32 weighted_skip_e;
  u32 nonsquare_transform_e;
  u32 second_transform_e;
};

/* tracing functions */
CMODEL_DLL_API u32 OpenAsicTraceFiles(void);
CMODEL_DLL_API void CloseAsicTraceFiles(void);

#endif /* TRACE_H */
