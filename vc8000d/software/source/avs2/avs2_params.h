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

#ifndef _AVS2_PARAMS_H_
#define _AVS2_PARAMS_H_

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "basetype.h"
#include "dwl.h"

#include "avs2_cfg.h"
#include "global.h"
#include "sw_stream.h"

/* alf const */
#define ALF_MAX_NUM_COEF 9
#define NO_VAR_BINS 16
#define LOG2_VAR_SIZE_H 2
#define LOG2_VAR_SIZE_W 2
#define NO_VAR_BINS 16
#define ALF_FOOTPRINT_SIZE 7
#define DF_CHANGED_SIZE 3
#define ALF_NUM_BIT_SHIFT 6

/* sequence header */
struct Avs2SeqParam {
  /* syntax */
  i32 profile_id;
  i32 level_id;
  bool progressive_sequence;
  bool is_field_sequence;
  i32 horizontal_size;
  i32 vertical_size;
  i32 chroma_format;
  i32 output_bit_depth;
  i32 sample_bit_depth;
  i32 aspect_ratio_information;
  i32 frame_rate_code;
  i32 bit_rate_lower;
  i32 bit_rate_upper;
  bool low_delay;
  bool temporal_id_exist_flag;
  i32 lcu_size_in_bit;
  bool weight_quant_enable_flag;
  bool load_seq_weight_quant_data_flag;
  bool background_picture_enable;
  bool b_dmh_enabled;
  bool b_mhpskip_enabled;
  bool dhp_enabled;
  bool wsm_enabled;
  bool inter_amp_enable;
  bool useNSQT;
  bool useSDIP;
  bool b_secT_enabled;
  bool sao_enable;
  bool alf_enable;
  bool b_pmvr_enabled;
  bool bcbr_enable;
  i32 num_of_rps;
  i32 picture_reorder_delay;
  bool cross_slice_loop_filter;

  short seq_wq_matrix[2][64];
  ref_man rps[32];

  /* utils, anchor of derived fields */
  int cnt;

  int new_sps_flag;

  /* derived */
  int pic_width_in_ctbs;
  int pic_height_in_ctbs;
  int pic_width_in_cbs;
  int pic_height_in_cbs;
  int max_dpb_size; /* derived according to B.2~14 according to profile/level */
  int new_sequence_flag;
  int pic_width;
  int pic_height;
};

#if 0
struct Avs2AlfTables {
  int alf_merge_ytable[16];                     // u4 (0~16)
  int alf_coeff_ytable[16 * ALF_MAX_NUM_COEF];  // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)
  int alf_coeff_utable[ALF_MAX_NUM_COEF];       // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)
  int alf_coeff_vtable[ALF_MAX_NUM_COEF];       // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)
};
#endif
struct Avs2AlfParams {
  u8 alf_table[8];
  u8 _fill0[8];
  u8 alf_coeff_y[16][16];
  u8 alf_coeff_u[16];
  u8 alf_coeff_v[16];
};

/* picture header */
struct Avs2PicParam {
  int type;
  int typeb;
  bool time_code_flag;
  int time_code;
  bool background_picture_flag;
  bool background_picture_output_flag;
  bool background_pred_flag;        /* Inter */
  bool background_reference_enable; /* Inter */
  int coding_order;  // coi
  int temporal_id;
  int displaydelay;
  ref_man rps;
  bool progressive_frame;
  bool picture_structure;
  bool top_field_first;
  bool repeat_first_field;
  bool is_top_field;
  bool fixed_picture_qp;
  int picture_qp;
  bool random_access_decodable_flag;
  bool loop_filter_disable;
  int alpha_c_offset;
  int beta_offset;
  int chroma_quant_param_delta_u;
  int chroma_quant_param_delta_v;
  bool pic_weight_quant_enable_flag;
  int pic_weight_quant_data_index;
  bool mb_adapt_wq_disable;
  int weighting_quant_param;
  int weighting_quant_model;
  int quant_param_undetail[6];
  int quant_param_detail[6];
  int wq_matrix[2][64];
  /* alf */
  int alf_flag[3];
  int alf_y_filters;
  struct DWLLinearMem *alf_tables;
  int alf_merge_ytable[16];                     // u4 (0~16)
  int alf_coeff_ytable[16 * ALF_MAX_NUM_COEF];  // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)
  int alf_coeff_utable[ALF_MAX_NUM_COEF];       // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)
  int alf_coeff_vtable[ALF_MAX_NUM_COEF];       // 0~7:s7(-64~63), 8:s12
                                                // (-1088~1071)(maybe u7 is ok)

  /* utils */
  int cnt;

  /* derived */
  int poc;  // poi
};

struct Avs2SeqDisaplay {
  /* syntax */
  int video_format;
  int video_range;
  int color_description;
  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;

  int display_horizontal_size;
  int display_vertical_size;
  int TD_mode;
  int view_packing_mode;
  int view_reverse;

  /* derived */
  int cnt;
};

/*picture_display_extension()*/
struct Avs2PicDisplay {
  int frame_centre_horizontal_offset[4];
  int frame_centre_vertical_offset[4];
};

/* Copyright_extension() header */
struct Avs2Copyright {
  int copyright_flag;
  int copyright_identifier;
  int original_or_copy;
  long long int copyright_number_1;
  long long int copyright_number_2;
  long long int copyright_number_3;
};

/* Camera_parameters_extension */
struct Avs2CameraParams {
  int camera_id;
  int height_of_image_device;
  int focal_length;
  int f_number;
  int vertical_angle_of_view;
  int camera_position_x_upper;
  int camera_position_x_lower;
  int camera_position_y_upper;
  int camera_position_y_lower;
  int camera_position_z_upper;
  int camera_position_z_lower;
  int camera_direction_x;
  int camera_direction_y;
  int camera_direction_z;
  int image_plane_vertical_x;
  int image_plane_vertical_y;
  int image_plane_vertical_z;
};

/* mastering_display_and_content_metadata_extension() header */
struct Avs2DispAndContent {
  int display_primaries_x0;
  int display_primaries_y0;
  int display_primaries_x1;
  int display_primaries_y1;
  int display_primaries_x2;
  int display_primaries_y2;
  int white_point_x;
  int white_point_y;
  int max_display_mastering_luminance;
  int min_display_mastering_luminance;
  int maximum_content_light_level;
  int maximum_frame_average_light_level;
};

struct Avs2ExtData {
  struct Avs2SeqDisaplay display;
  struct Avs2Copyright copyright;
  struct Avs2PicDisplay pic;
  struct Avs2CameraParams cam;
};

int Avs2ParseSequenceHeader(struct StrmData *stream, struct Avs2SeqParam *seq);
int Avs2ParseIntraPictureHeader(struct StrmData *stream,
                                struct Avs2SeqParam *seq,
                                struct Avs2PicParam *pps);
int Avs2ParseInterPictureHeader(struct StrmData *stream,
                                struct Avs2SeqParam *seq,
                                struct Avs2PicParam *pps);
void Avs2ParseExtensionData(struct StrmData *stream, struct Avs2SeqParam *sps,
                            struct Avs2PicParam *pps, struct Avs2ExtData *ext);
void Avs2ParseUserData(struct StrmData *stream);
int Avs2ParseAlfCoeff(struct StrmData *stream, struct Avs2SeqParam *seq,
                       struct Avs2PicParam *pps, struct DWLLinearMem *tbl_mem);

#endif /* #ifdef _AVS2_PARAMS_H_ */
