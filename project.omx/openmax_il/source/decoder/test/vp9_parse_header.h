/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description :  Video/Image decoder test client file reader functions.
--
------------------------------------------------------------------------------*/

#ifndef VP9_PARSE_HEADER_H
#define VP9_PARSE_HEADER_H

#define HANTRO_TRUE 1
#define HANTRO_FALSE 0

#define HANTRO_OK 0
#define HANTRO_NOK 1
#define END_OF_STREAM 0xFFFFFFFFU

#define NUM_REF_FRAMES 8
#define NUM_REF_FRAMES_LG2 3

#define ALLOWED_REFS_PER_FRAME 3

#define NUM_FRAME_CONTEXTS_LG2 2

#define PREDICTION_PROBS 3

#define MAX_MB_SEGMENTS 8
#define MB_SEG_TREE_PROBS (MAX_MB_SEGMENTS - 1)
#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2

enum InterpolationFilterType {
  EIGHTTAP_SMOOTH,
  EIGHTTAP,
  EIGHTTAP_SHARP,
  BILINEAR,
  SWITCHABLE /* should be the last one */
};

struct StrmData {
  const unsigned char *strm_buff_start; /* pointer to start of stream buffer */
  const unsigned char *strm_curr_pos;   /* current read address in stream buffer */
  unsigned int bit_pos_in_word;       /* bit position in stream buffer byte */
  unsigned int strm_buff_size;        /* size of stream buffer (bytes) */
  unsigned int strm_data_size;        /* size of stream data (bytes) */
  unsigned int strm_buff_read_bits;   /* number of bits read from stream buffer */
  unsigned int remove_emul3_byte;     /* signal the pre-removal of emulation prevention 3
                    bytes */
  unsigned int emul_byte_count; /* counter incremented for each removed byte */
  unsigned int is_rb;               /* ring buffer used? */
};

static const int vp9_seg_feature_data_signed[4] = {1, 1, 0, 0};
static const int vp9_seg_feature_data_max[4] = {255, 63, 3, 0};

enum Vp9ColorSpace {
  VP9_YCbCr_BT601,
  VP9_CUSTOM
};

struct Vp9RefSize {
  unsigned int ref_width;
  unsigned int ref_height;
};

struct Vp9Header {

  /* Current frame dimensions */
  unsigned int width;
  unsigned int height;
  unsigned int scaled_width;
  unsigned int scaled_height;

  unsigned int vp_version;
  unsigned int vp_profile;

  unsigned int bit_depth;
  unsigned int key_frame;
  unsigned int scaling_active;
  unsigned int resolution_change;

  /* DCT coefficient partitions */
  unsigned int offset_to_dct_parts;

  enum Vp9ColorSpace color_space;
  unsigned int error_resilient;
  unsigned int show_frame;
  unsigned int show_existing_frame;
  unsigned int intra_only;
  unsigned int subsampling_x;
  unsigned int subsampling_y;

  unsigned int frame_context_idx;
  unsigned int active_ref_idx[ALLOWED_REFS_PER_FRAME];
  unsigned int refresh_frame_flags;
  unsigned int refresh_entropy_probs;
  unsigned int frame_parallel_decoding;
  unsigned int reset_frame_context;

  unsigned int ref_frame_sign_bias[4];
  int loop_filter_level;
  unsigned int loop_filter_sharpness;
  /* Quantization parameters */
  int qp_yac, qp_ydc, qp_y2_ac, qp_y2_dc, qp_ch_ac, qp_ch_dc;

  /* From here down, frame-to-frame persisting stuff */

  unsigned int lossless;
  unsigned int allow_high_precision_mv;
  unsigned int mcomp_filter_type;
  unsigned int log2_tile_columns;
  unsigned int log2_tile_rows;

  /* Segment and macroblock specific values */
  unsigned int segment_enabled;
  unsigned int segment_map_update;
  unsigned int segment_map_temporal_update;
  unsigned int segment_feature_mode; /* ABS data or delta data */
  unsigned int segment_feature_enable[MAX_MB_SEGMENTS][4];
  int segment_feature_data[MAX_MB_SEGMENTS][4];
  unsigned int mode_ref_lf_enabled;
  int mb_ref_lf_delta[MAX_REF_LF_DELTAS];
  int mb_mode_lf_delta[MAX_MODE_LF_DELTAS];
  int ref_frame_map[NUM_REF_FRAMES];

  unsigned int frame_tag_size;

  struct Vp9RefSize ref_size[NUM_REF_FRAMES];
};







#endif
