/*-----------------------------------------------------------------------------
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

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <OMX_Types.h>

#include "vp9_parse_header.h"
#include "basetype.h"
#include "util.h"
#include "dbgtrace.h"

#define DBGT_DECLARE_AUTOVAR
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "VIDEO_DECODER"


struct Vp9Header vp9header;

static u32 SwGetBits(struct StrmData *stream, u32 num_bits);
static u32 SwGetBitsUnsignedMax(struct StrmData *stream, u32 max_value);
static u32 SwShowBits(const struct StrmData *stream, u32 num_bits);
static u32 SwFlushBits(struct StrmData *stream, u32 num_bits);
static u32 SwIsByteAligned(const struct StrmData *);


static u8* SwTurnAround(const u8 * strm, const u8* buf, u8* tmp_buf, u32 buf_size, u32 num_bits) {
  u32 bytes = (num_bits+7)/8;
  u32 is_turn_around = 0;
  if((strm + bytes) > (buf + buf_size))
    is_turn_around = 1;

  if(buf + 2 > strm)  {
    DBGT_ASSERT(is_turn_around == 0);
    is_turn_around = 2;
  }

  if(is_turn_around == 0) {
    return NULL;
  } else if(is_turn_around == 1){
    i32 i;
    u32 bytes_left = (u32)((long)(buf + buf_size) - (long)strm);

    /* turn around */
    for(i = -3; i < (i32)bytes_left; i++) {
      tmp_buf[3 + i] = strm[i];
    }
  
    /*turn around point*/
    for(i = 0; i < (i32)(bytes - bytes_left); i++) {
      tmp_buf[3 + bytes_left + i] = buf[i];
    }
  
    return tmp_buf+3;
  } else {
    i32 i;
    u32 left_byte = (u32)((long) strm - (long) buf);
    /* turn around */
    for(i = 0; i < 2; i++) {
      tmp_buf[i] = buf[buf_size - 2 + i];
    }

    /*turn around point*/
    for(i = 0; i < (i32)(bytes + left_byte); i++) {
      tmp_buf[i + 2] = buf[i];
    }

    return (tmp_buf + 2 + left_byte);
  }
}



static u32 SwGetBits(struct StrmData *stream, u32 num_bits) {

  u32 out;

  DBGT_ASSERT(stream);
  DBGT_ASSERT(num_bits < 32);

  if (num_bits == 0) return 0;

  out = SwShowBits(stream, 32) >> (32 - num_bits);

  if (SwFlushBits(stream, num_bits) == HANTRO_OK) {
    return (out);
  } else {
    return (END_OF_STREAM);
  }
}

static u32 SwGetBitsUnsignedMax(struct StrmData *stream, u32 max_value) {
  i32 bits = 0;
  u32 num_values = max_value;
  u32 value;

  /* Bits for unsigned value */
  if (num_values > 1) {
    num_values--;
    while (num_values > 0) {
      bits++;
      num_values >>= 1;
    }
  }

  value = SwGetBits(stream, bits);
  return (value > max_value) ? max_value : value;
}

static u32 SwShowBits(const struct StrmData *stream, u32 num_bits) {

  i32 bits;
  u32 out, out_bits;
  u32 tmp_read_bits;
  const u8 *strm;
  /* used to copy stream data when ringbuffer turnaround */
  u8 tmp_strm_buf[32], *tmp;

  DBGT_ASSERT(stream);
  DBGT_ASSERT(stream->strm_curr_pos);
  DBGT_ASSERT(stream->bit_pos_in_word < 8);
  DBGT_ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));
  DBGT_ASSERT(num_bits <= 32);

  strm = stream->strm_curr_pos;

  /* bits left in the buffer */
  bits = (i32)stream->strm_data_size * 8 - (i32)stream->strm_buff_read_bits;

  if (!bits) {
    return (0);
  }

  tmp = SwTurnAround(stream->strm_curr_pos, stream->strm_buff_start,
                    tmp_strm_buf, stream->strm_buff_size,
                    num_bits + stream->bit_pos_in_word + 32);

  if(tmp != NULL) strm = tmp;

  if (!stream->remove_emul3_byte) {

    out = out_bits = 0;
    tmp_read_bits = stream->strm_buff_read_bits;

    if (stream->bit_pos_in_word) {
      out = strm[0] << (24 + stream->bit_pos_in_word);
      strm++;
      out_bits = 8 - stream->bit_pos_in_word;
      bits -= out_bits;
      tmp_read_bits += out_bits;
    }

    while (bits && out_bits < num_bits) {

      /* check emulation prevention byte */
      if (tmp_read_bits >= 16 && strm[-2] == 0x0 &&
          strm[-1] == 0x0 &&
          strm[0] == 0x3) {
        strm++;
        tmp_read_bits += 8;
        bits -= 8;
        /* emulation prevention byte shall not be the last byte of the
         * stream */
        if (bits <= 0) break;
      }

      tmp_read_bits += 8;

      if (out_bits <= 24)
      {
        out |= (u32)strm[0] << (24 - out_bits);
        strm++;
      }
      else
      {
        out |= (u32)strm[0] >> (out_bits - 24);
        strm++;
      }

      out_bits += 8;
      bits -= 8;
    }

    return (out >> (32 - num_bits));

  } else {
    u32 shift;

    /* at least 32-bits in the buffer */
    if (bits >= 32) {
      u32 bit_pos_in_word = stream->bit_pos_in_word;

      out = ((u32)strm[3]) |
            ((u32)strm[2] << 8) |
            ((u32)strm[1] << 16) |
            ((u32)strm[0] << 24);

      if (bit_pos_in_word) {
        out <<= bit_pos_in_word;
        out |= (u32)strm[4] >> (8 - bit_pos_in_word);
      }

      return (out >> (32 - num_bits));
    }
    /* at least one bit in the buffer */
    else if (bits > 0) {
      shift = (i32)(24 + stream->bit_pos_in_word);
      out = (u32)strm[0] << shift;
      strm++;
      bits -= (i32)(8 - stream->bit_pos_in_word);
      while (bits > 0) {
        shift -= 8;
        out |= (u32)strm[0] << shift;
        strm++;
        bits -= 8;
      }
      return (out >> (32 - num_bits));
    } else
      return (0);
  }
}

static u32 SwFlushBits(struct StrmData *stream, u32 num_bits) {

  u32 bytes_left;
  const u8 *strm, *strm_bak;
  u8 tmp_strm_buf[32], *tmp;

  DBGT_ASSERT(stream);
  DBGT_ASSERT(stream->strm_buff_start);
  DBGT_ASSERT(stream->strm_curr_pos);
  DBGT_ASSERT(stream->bit_pos_in_word < 8);
  DBGT_ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));

  /* used to copy stream data when ringbuffer turnaround */
  tmp = SwTurnAround(stream->strm_curr_pos, stream->strm_buff_start,
                     tmp_strm_buf, stream->strm_buff_size,
                     num_bits + stream->bit_pos_in_word + 32);

  if (!stream->remove_emul3_byte) {
    if ((stream->strm_buff_read_bits + num_bits) >
        (8 * stream->strm_data_size)) {
      stream->strm_buff_read_bits = 8 * stream->strm_data_size;
      stream->bit_pos_in_word = 0;
      stream->strm_curr_pos = stream->strm_buff_start + stream->strm_buff_size;
      return (END_OF_STREAM);
    } else {
      bytes_left =
          (8 * stream->strm_data_size - stream->strm_buff_read_bits) / 8;
      if(tmp != NULL)
        strm = tmp;
      else
        strm = stream->strm_curr_pos;
      strm_bak = strm;

      if (stream->bit_pos_in_word) {
        if (num_bits < 8 - stream->bit_pos_in_word) {
          stream->strm_buff_read_bits += num_bits;
          stream->bit_pos_in_word += num_bits;
          return (HANTRO_OK);
        }
        num_bits -= 8 - stream->bit_pos_in_word;
        stream->strm_buff_read_bits += 8 - stream->bit_pos_in_word;
        stream->bit_pos_in_word = 0;
        strm++;

        if (stream->strm_buff_read_bits >= 16 && bytes_left &&
            strm[-2] == 0x0 &&
            strm[-1] == 0x0 &&
            strm[0] == 0x3) {
          strm++;
          stream->strm_buff_read_bits += 8;
          bytes_left--;
          stream->emul_byte_count++;
        }
      }

      while (num_bits >= 8 && bytes_left) {
        if (bytes_left > 2 && strm[0] == 0 &&
            strm[1] == 0 &&
            strm[2] <= 1) {
          /* trying to flush part of start code prefix -> error */
          return (HANTRO_NOK);
        }

        strm++;
        stream->strm_buff_read_bits += 8;
        bytes_left--;

        /* check emulation prevention byte */
        if (stream->strm_buff_read_bits >= 16 && bytes_left &&
            strm[-2] == 0x0 &&
            strm[-1] == 0x0 &&
            strm[0] == 0x3) {
          strm++;
          stream->strm_buff_read_bits += 8;
          bytes_left--;
          stream->emul_byte_count++;
        }
        num_bits -= 8;
      }

      if (num_bits && bytes_left) {
        if (bytes_left > 2 && strm[0] == 0 &&
            strm[1] == 0 &&
            strm[2] <= 1) {
          /* trying to flush part of start code prefix -> error */
          return (HANTRO_NOK);
        }

        stream->strm_buff_read_bits += num_bits;
        stream->bit_pos_in_word = num_bits;
        num_bits = 0;
      }

      stream->strm_curr_pos += strm - strm_bak;
      if (stream->is_rb && stream->strm_curr_pos >= (stream->strm_buff_start + stream->strm_buff_size))
        stream->strm_curr_pos -= stream->strm_buff_size;

      if (num_bits)
        return (END_OF_STREAM);
      else
        return (HANTRO_OK);
    }
  } else {
    u32 bytes_shift = (stream->bit_pos_in_word + num_bits) >> 3;
    stream->strm_buff_read_bits += num_bits;
    stream->bit_pos_in_word = stream->strm_buff_read_bits & 0x7;
    if ((stream->strm_buff_read_bits) <= (8 * stream->strm_data_size)) {
      stream->strm_curr_pos += bytes_shift;
      if (stream->is_rb && stream->strm_curr_pos >= (stream->strm_buff_start + stream->strm_buff_size))
        stream->strm_curr_pos -= stream->strm_buff_size;
      return (HANTRO_OK);
    } else
      return (END_OF_STREAM);
  }
}

static u32 SwIsByteAligned(const struct StrmData *stream) {

  if (!stream->bit_pos_in_word)
    return (HANTRO_TRUE);
  else
    return (HANTRO_FALSE);
}



#define VP9_KEY_FRAME_START_CODE_0 0x49
#define VP9_KEY_FRAME_START_CODE_1 0x83
#define VP9_KEY_FRAME_START_CODE_2 0x42

#define MIN_TILE_WIDTH 256
#define MAX_TILE_WIDTH 4096
#define MIN_TILE_WIDTH_SBS (MIN_TILE_WIDTH >> 6)
#define MAX_TILE_WIDTH_SBS (MAX_TILE_WIDTH >> 6)

static u32 CheckSyncCode(struct StrmData *rb) {
  if (SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_0 ||
      SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_1 ||
      SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_2) {
    DBGT_CRITICAL("VP9 Key-frame start code missing or invalid!");
    return HANTRO_NOK;
  }
  return HANTRO_OK;
}


static void SetupFrameSizeWithRefs(struct StrmData *rb,
                            struct Vp9Header *dec) {
  u32 tmp;
  i32 found, i;
  u32 prev_width, prev_height;

  found = 0;
  dec->resolution_change = 0;
  prev_width = dec->width;
  prev_height = dec->height;
  for (i = 0; i < ALLOWED_REFS_PER_FRAME; ++i) {
    tmp = SwGetBits(rb, 1);
    DBGT_PDEBUG("use_prev_frame_size = %d", tmp);
    if (tmp) {
      found = 1;
      dec->width = dec->ref_size[dec->active_ref_idx[i]].ref_width;
      dec->height = dec->ref_size[dec->active_ref_idx[i]].ref_height;
      break;
    }
  }

  if (!found) {
    /* Frame width */
    dec->width = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("frame_width = %d", dec->width);

    /* Frame height */
    dec->height = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("frame_height = %d", dec->height);
  }

  if (dec->width != prev_width || dec->height != prev_height)
    dec->resolution_change = 1; /* Signal resolution change for this frame */

  /* render size */
  dec->scaling_active = SwGetBits(rb, 1);
  DBGT_PDEBUG("scaling active = %d", dec->scaling_active);

  if (dec->scaling_active) {
    /* Scaled frame width */
    dec->scaled_width = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("scaled_frame_width = %d", dec->scaled_width);

    /* Scaled frame height */
    dec->scaled_height = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("scaled_frame_height = %d", dec->scaled_height);
  }
}

static void SetupFrameSize(struct StrmData *rb, struct Vp9Header *dec) {
  /* Frame width */
  dec->width = SwGetBits(rb, 16) + 1;
  DBGT_PDEBUG("frame_width = %d", dec->width);

  /* Frame height */
  dec->height = SwGetBits(rb, 16) + 1;
  DBGT_PDEBUG("frame_height = %d", dec->height);

  /* render size */
  dec->scaling_active = SwGetBits(rb, 1);
  DBGT_PDEBUG("Scaling active = %d", dec->scaling_active);

  if (dec->scaling_active) {
    /* Scaled frame width */
    dec->scaled_width = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("scaled_frame_width = %d", dec->scaled_width);

    /* Scaled frame height */
    dec->scaled_height = SwGetBits(rb, 16) + 1;
    DBGT_PDEBUG("scaled_frame_height = %d", dec->scaled_height);
  }
}


static void DecodeLfParams(struct StrmData *rb, struct Vp9Header *dec) {
  u32 sign, tmp, j;

  if (dec->key_frame || dec->error_resilient || dec->intra_only) {
    memset(dec->mb_ref_lf_delta, 0, sizeof(dec->mb_ref_lf_delta));
    memset(dec->mb_mode_lf_delta, 0, sizeof(dec->mb_mode_lf_delta));
    dec->mb_ref_lf_delta[0] = 1;
    dec->mb_ref_lf_delta[1] = 0;
    dec->mb_ref_lf_delta[2] = -1;
    dec->mb_ref_lf_delta[3] = -1;
  }

  /* Loop filter adjustments */
  dec->loop_filter_level = SwGetBits(rb, 6);
  dec->loop_filter_sharpness = SwGetBits(rb, 3);
  DBGT_PDEBUG("loop_filter_level = %d", dec->loop_filter_level);
  DBGT_PDEBUG("loop_filter_sharpness = %d", dec->loop_filter_sharpness);

  /* Adjustments enabled? */
  dec->mode_ref_lf_enabled = SwGetBits(rb, 1);
  DBGT_PDEBUG("loop_filter_adj_enable = %d", dec->mode_ref_lf_enabled);

  if (dec->mode_ref_lf_enabled) {
    /* Mode update? */
    tmp = SwGetBits(rb, 1);
    DBGT_PDEBUG("loop_filter_adj_update = %d", tmp);
    if (tmp) {
      /* Reference frame deltas */
      for (j = 0; j < MAX_REF_LF_DELTAS; j++) {
        tmp = SwGetBits(rb, 1);
        DBGT_PDEBUG("ref_frame_delta_update = %d", tmp);
        if (tmp) {
          /* Payload */
          tmp = SwGetBits(rb, 6);
          /* Sign */
          sign = SwGetBits(rb, 1);
          DBGT_PDEBUG("loop_filter_payload = %d", tmp);
          DBGT_PDEBUG("loop_filter_sign = %d", sign);

          dec->mb_ref_lf_delta[j] = tmp;
          if (sign) dec->mb_ref_lf_delta[j] = -dec->mb_ref_lf_delta[j];
        }
      }

      /* Mode deltas */
      for (j = 0; j < MAX_MODE_LF_DELTAS; j++) {
        tmp = SwGetBits(rb, 1);
        DBGT_PDEBUG("mb_type_delta_update = %d", tmp);
        if (tmp) {
          /* Payload */
          tmp = SwGetBits(rb, 6);
          /* Sign */
          sign = SwGetBits(rb, 1);
          DBGT_PDEBUG("loop_filter_payload = %d", tmp);
          DBGT_PDEBUG("loop_filter_sign = %d", sign);

          dec->mb_mode_lf_delta[j] = tmp;
          if (sign) dec->mb_mode_lf_delta[j] = -dec->mb_mode_lf_delta[j];
        }
      }
    }
  } /* Mb mode/ref lf adjustment */
}

static i32 DecodeQuantizerDelta(struct StrmData *rb) {
  u32 sign;
  i32 delta;

  if (SwGetBits(rb, 1)) {
    DBGT_PDEBUG("qp_delta_present = 1");
    delta = SwGetBits(rb, 4);
    DBGT_PDEBUG("qp_delta  = %d", delta);
    sign = SwGetBits(rb, 1);
    DBGT_PDEBUG("qp_delta_sign = %d", sign);
    if (sign) delta = -delta;
    return delta;
  } else {
    DBGT_PDEBUG("qp_delta_present = 0");
    return 0;
  }
}



static void DecodeSegmentationData(struct StrmData *rb, struct Vp9Header *dec) {
  u32 tmp, sign, i, j;
  //struct Vp9EntropyProbs *fc = &dec->entropy; /* Frame context */
  unsigned char mb_segment_tree_probs[MB_SEG_TREE_PROBS];
  unsigned char segment_pred_probs[PREDICTION_PROBS];

  /* Segmentation enabled? */
  dec->segment_enabled = SwGetBits(rb, 1);
  DBGT_PDEBUG("segment_enabled = %d", dec->segment_enabled);

  dec->segment_map_update = 0;
  dec->segment_map_temporal_update = 0;
  if (!dec->segment_enabled) return;

  /* Segmentation map update */
  dec->segment_map_update = SwGetBits(rb, 1);
  DBGT_PDEBUG("segment_map_update = %d", dec->segment_map_update);
  if (dec->segment_map_update) {
    for (i = 0; i < MB_SEG_TREE_PROBS; i++) {
      tmp = SwGetBits(rb, 1);
      DBGT_PDEBUG("segment_tree_prob_update = %d", tmp);
      if (tmp) {
        mb_segment_tree_probs[i] = SwGetBits(rb, 8);
        DBGT_PDEBUG("segment_tree_prob = %d", tmp);
      } else {
        mb_segment_tree_probs[i] = 255;
      }
    }

    /* Read the prediction probs needed to decode the segment id */
    dec->segment_map_temporal_update = SwGetBits(rb, 1);
    DBGT_PDEBUG("segment_map_temporal_update = %d",
                 dec->segment_map_temporal_update);
    for (i = 0; i < PREDICTION_PROBS; i++) {
      if (dec->segment_map_temporal_update) {
        tmp = SwGetBits(rb, 1);
        DBGT_PDEBUG("segment_pred_prob_update = %d", tmp);
        if (tmp) {
          segment_pred_probs[i] = SwGetBits(rb, 8);
          DBGT_PDEBUG("segment_pred_prob = %d", tmp);
        } else {
          segment_pred_probs[i] = 255;
        }
      } else {
        segment_pred_probs[i] = 255;
      }
    }
  }
  /* Segment feature data update */
  tmp = SwGetBits(rb, 1);
  DBGT_PDEBUG("segment_data_update = %d", tmp);
  if (tmp) {
    /* Absolute/relative mode */
    dec->segment_feature_mode = SwGetBits(rb, 1);
    DBGT_PDEBUG("segment_abs_delta = %d", dec->segment_feature_mode);

    /* Clear all previous segment data */
    memset(dec->segment_feature_enable, 0,
              sizeof(dec->segment_feature_enable));
    memset(dec->segment_feature_data, 0, sizeof(dec->segment_feature_data));

    for (i = 0; i < MAX_MB_SEGMENTS; i++) {
      for (j = 0; j < 4; j++) {
        dec->segment_feature_enable[i][j] = SwGetBits(rb, 1);
        DBGT_PDEBUG("segment_feature_enable = %d",
                     dec->segment_feature_enable[i][j]);

        if (dec->segment_feature_enable[i][j]) {
          /* Feature data, bits changes for every feature */
          dec->segment_feature_data[i][j] =
              SwGetBitsUnsignedMax(rb, vp9_seg_feature_data_max[j]);
          DBGT_PDEBUG("segment_feature_data = %d", dec->segment_feature_data[i][j]);
          /* Sign if needed */
          if (vp9_seg_feature_data_signed[j]) {
            sign = SwGetBits(rb, 1);
            DBGT_PDEBUG("segment_feature_sign = %d", sign);
            if (sign)
              dec->segment_feature_data[i][j] =
                  -dec->segment_feature_data[i][j];
          }
        }
      }
    }
  }
}

static void GetTileNBits(struct Vp9Header *dec, u32 *min_log2_ntiles_ptr,
                  u32 *delta_log2_ntiles) {
  const int sb_cols = (dec->width + 63) >> 6;
  u32 min_log2_ntiles, max_log2_ntiles;

  for (max_log2_ntiles = 0; (sb_cols >> max_log2_ntiles) >= MIN_TILE_WIDTH_SBS;
       max_log2_ntiles++) {
  }
  if (max_log2_ntiles > 0) max_log2_ntiles--;
  for (min_log2_ntiles = 0; (MAX_TILE_WIDTH_SBS << min_log2_ntiles) < sb_cols;
       min_log2_ntiles++) {
  }

  DBGT_ASSERT(max_log2_ntiles >= min_log2_ntiles);
  *min_log2_ntiles_ptr = min_log2_ntiles;
  *delta_log2_ntiles = max_log2_ntiles - min_log2_ntiles;
}

u32 ReadTileSize(const u8 *cx_size) {
  u32 size;
  size = (u32)cx_size[3] +
         ((u32)(cx_size[2]) << 8) +
         ((u32)(cx_size[1]) << 16) +
         ((u32)(cx_size[0]) << 24);
  return size;
}


u32 Vp9ParseHeader(const u8 *strm, u32 data_len,
                              const u8 *buf, u32 buf_len,
                              u32 *header_len, u32 *slice_len) {
  /* Use common bit parse but don't remove emulation prevention bytes. */
  struct StrmData rb = {buf, strm, 0, buf_len, data_len, 0, 1, 0, 0};
  u32 key_frame = 0;
  u32 err, tmp, i, delta_log2_tiles;
  u32 status = 0;

  struct Vp9Header *dec = &vp9header;
  dec->offset_to_dct_parts = 0;

  tmp = SwGetBits(&rb, 2);
  DBGT_PDEBUG("Frame marker = %d", tmp);
  if (tmp == END_OF_STREAM) goto END;

  dec->vp_version = SwGetBits(&rb, 1);
  DBGT_PDEBUG("VP version = %d", dec->vp_version);
  dec->vp_profile = (SwGetBits(&rb, 1)<<1) + dec->vp_version;
  if (dec->vp_profile>2) {
    dec->vp_profile += SwGetBits(&rb, 1);
  }

  dec->show_existing_frame = SwGetBits(&rb, 1);
  DBGT_PDEBUG("Show existing frame = %d", dec->show_existing_frame);
  if (dec->show_existing_frame) {
    i32 idx_to_show = SwGetBits(&rb, 3);
    dec->offset_to_dct_parts = 0;
    dec->refresh_frame_flags = 0;
    dec->loop_filter_level = 0;
    status = 1;
    goto END;
  }

  key_frame = SwGetBits(&rb, 1);
  dec->key_frame = !key_frame;
  dec->show_frame = SwGetBits(&rb, 1);
  dec->error_resilient = SwGetBits(&rb, 1);
  DBGT_PDEBUG("Key frame = %d", dec->key_frame);
  DBGT_PDEBUG("Show frame = %d", dec->show_frame);
  DBGT_PDEBUG("Error resilient = %d", dec->error_resilient);

  if (dec->error_resilient == END_OF_STREAM) goto END;

  if (dec->key_frame) {
    err = CheckSyncCode(&rb);
    if (err == HANTRO_NOK) goto END;

    /* color_config */
    dec->bit_depth = dec->vp_profile >= 2 ? (SwGetBits(&rb, 1) ? 12 : 10) : 8;
    DBGT_PDEBUG("Bit depth = %d", dec->bit_depth);
    dec->color_space = SwGetBits(&rb, 3);
    DBGT_PDEBUG("Color space = %d", dec->color_space);

    if (dec->color_space != 7) /* != s_rgb */
    {
      tmp = SwGetBits(&rb, 1);
      DBGT_PDEBUG("YUV range = %d", tmp);
      if (dec->vp_version == 1) {
        dec->subsampling_x = SwGetBits(&rb, 1);
        dec->subsampling_y = SwGetBits(&rb, 1);
        tmp = SwGetBits(&rb, 1);
        DBGT_PDEBUG("Subsampling X = %d", dec->subsampling_x);
        DBGT_PDEBUG("Subsampling Y = %d", dec->subsampling_y);
        DBGT_PDEBUG("Alpha plane = %d", tmp);
      } else {
        dec->subsampling_x = dec->subsampling_y = 1;
      }
    } else {
      if (dec->vp_version == 1) {
        dec->subsampling_x = dec->subsampling_y = 0;
        tmp = SwGetBits(&rb, 1);
        DBGT_PDEBUG("Alpha plane = %d", tmp);
      }
    }

    //dec->refresh_frame_flags = (1 << ALLOWED_REFS_PER_FRAME) - 1;
    dec->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;

    for (i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {
      dec->active_ref_idx[i] = 0; /* TODO next free frame buffer */
    }

    SetupFrameSize(&rb, dec);

  } else {
    if (!dec->show_frame) {
      dec->intra_only = SwGetBits(&rb, 1);
    } else {
      dec->intra_only = 0;
    }

    DBGT_PDEBUG("Intra only = %d", dec->intra_only);

    if (!dec->error_resilient) {
      dec->reset_frame_context = SwGetBits(&rb, 2);
    } else {
      dec->reset_frame_context = 0;
    }
    DBGT_PDEBUG("Reset frame context = %d", dec->reset_frame_context);

    if (dec->intra_only) {
      err = CheckSyncCode(&rb);
      if (err == HANTRO_NOK) goto END;

      dec->bit_depth = dec->vp_profile>=2? SwGetBits(&rb, 1)? 12 : 10 : 8;
      if (dec->vp_profile > 0) {
        dec->color_space = SwGetBits(&rb, 3);
        DBGT_PDEBUG("Color space = %d", dec->color_space);
        if (dec->color_space != 7) /* != s_rgb */
        {
          tmp = SwGetBits(&rb, 1);
          DBGT_PDEBUG("YUV range = %d", tmp);
          if (dec->vp_version == 1) {
            dec->subsampling_x = SwGetBits(&rb, 1);
            dec->subsampling_y = SwGetBits(&rb, 1);
            tmp = SwGetBits(&rb, 1);
            DBGT_PDEBUG("Subsampling X = %d", dec->subsampling_x);
            DBGT_PDEBUG("Subsampling Y = %d", dec->subsampling_y);
            DBGT_PDEBUG("Alpha plane = %d", tmp);
          } else {
            dec->subsampling_x = dec->subsampling_y = 1;
          }
        } else {
          if (dec->vp_version == 1) {
            dec->subsampling_x = dec->subsampling_y = 0;
            tmp = SwGetBits(&rb, 1);
            DBGT_PDEBUG("Alpha plane = %d", tmp);
          }
        }
      }
      /* Refresh reference frame flags */
      dec->refresh_frame_flags = SwGetBits(&rb, NUM_REF_FRAMES);
      DBGT_PDEBUG("refresh_frame_flags = %d", dec->refresh_frame_flags);
      SetupFrameSize(&rb, dec);
    } else {
      /* Refresh reference frame flags */
      dec->refresh_frame_flags = SwGetBits(&rb, NUM_REF_FRAMES);
      DBGT_PDEBUG("refresh_frame_flags = %d", dec->refresh_frame_flags);

      for (i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {
        i32 ref_frame_num = SwGetBits(&rb, NUM_REF_FRAMES_LG2);
        i32 mapped_ref = dec->ref_frame_map[ref_frame_num];
        DBGT_PDEBUG("active_reference_frame_idx = %d", ref_frame_num);
        DBGT_PDEBUG("active_reference_frame_num = %d", mapped_ref);
        dec->active_ref_idx[i] = mapped_ref;

        dec->ref_frame_sign_bias[i + 1] = SwGetBits(&rb, 1);
        DBGT_PDEBUG("ref_frame_sign_bias = %d", dec->ref_frame_sign_bias[i + 1]);
      }

      SetupFrameSizeWithRefs(&rb, dec);

      dec->allow_high_precision_mv = SwGetBits(&rb, 1);
      DBGT_PDEBUG("high_precision_mv = %d", dec->allow_high_precision_mv);
      tmp = SwGetBits(&rb, 1);
      DBGT_PDEBUG("mb_switchable_mcomp_filt = %d", tmp);
      if (tmp) {
        dec->mcomp_filter_type = SWITCHABLE;
      } else {
        dec->mcomp_filter_type = SwGetBits(&rb, 2);
        DBGT_PDEBUG("mcomp_filter_type = %d", dec->mcomp_filter_type);
      }
    }
  }

  if (!dec->error_resilient) {
    /* Refresh entropy probs,
     * 0 == this frame probs are used only for this frame decoding,
     * 1 == this frame probs will be stored for future reference */
    dec->refresh_entropy_probs = SwGetBits(&rb, 1);
    dec->frame_parallel_decoding = SwGetBits(&rb, 1);
    DBGT_PDEBUG("Refresh frame context = %d", dec->refresh_entropy_probs);
    DBGT_PDEBUG("Frame parallel = %d", dec->frame_parallel_decoding);
  } else {
    dec->refresh_entropy_probs = 0;
    dec->frame_parallel_decoding = 1;
  }

  dec->frame_context_idx = SwGetBits(&rb, NUM_FRAME_CONTEXTS_LG2);
  DBGT_PDEBUG("Frame context index = %d", dec->frame_context_idx);

  if (dec->key_frame || dec->error_resilient || dec->intra_only) {
    dec->frame_context_idx = 0;
    dec->ref_frame_sign_bias[0] = 
    dec->ref_frame_sign_bias[1] = 
    dec->ref_frame_sign_bias[2] = 
    dec->ref_frame_sign_bias[3] = 0;
    for (i = 0; i < NUM_REF_FRAMES; i++) {
      dec->ref_frame_map[i] = i;
    }
  }

  /* Loop filter */
  DecodeLfParams(&rb, dec);

  /* Quantizers */
  dec->qp_yac = SwGetBits(&rb, 8);
  DBGT_PDEBUG("qp_y_ac = %d", dec->qp_yac);
  dec->qp_ydc = DecodeQuantizerDelta(&rb);
  dec->qp_ch_dc = DecodeQuantizerDelta(&rb);
  dec->qp_ch_ac = DecodeQuantizerDelta(&rb);

  dec->lossless = dec->qp_yac == 0 && dec->qp_ydc == 0 && dec->qp_ch_dc == 0 &&
                  dec->qp_ch_ac == 0;

  /* Setup segment based adjustments */
  DecodeSegmentationData(&rb, dec);

  /* Tile dimensions */
  GetTileNBits(dec, &dec->log2_tile_columns, &delta_log2_tiles);
  while (delta_log2_tiles--) {
    tmp = SwGetBits(&rb, 1);
    if (tmp == END_OF_STREAM) goto END;
    if (tmp) {
      dec->log2_tile_columns++;
    } else {
      break;
    }
  }
  DBGT_PDEBUG("log2_tile_columns = %d", dec->log2_tile_columns);

  dec->log2_tile_rows = SwGetBits(&rb, 1);
  if (dec->log2_tile_rows) dec->log2_tile_rows += SwGetBits(&rb, 1);
  DBGT_PDEBUG("log2_tile_rows = %d", dec->log2_tile_rows);

  /* Size of frame headers */
  dec->offset_to_dct_parts = SwGetBits(&rb, 16);
  if (dec->offset_to_dct_parts == END_OF_STREAM) goto END;

  dec->frame_tag_size = (rb.strm_buff_read_bits + 7) / 8;


  /* When first tile row has zero-height skip it. Cannot be done when pic height
   * smaller than 3 SB rows, in this case more than one tile rows may be skipped
   * and needs to be handled in stream parsing. */
  const u8 *strmTmp = strm + dec->frame_tag_size + dec->offset_to_dct_parts;
  if ((1 << dec->log2_tile_rows) - (dec->height + 63) / 64 == 1) {
    for (i = (1 << dec->log2_tile_columns); i; i--) {
      tmp = ReadTileSize(strmTmp);
      strmTmp += 4 + tmp;
      dec->offset_to_dct_parts += 4 + tmp;
      DBGT_PDEBUG("Tile row with h=0, skipping %d", tmp);
      if (strmTmp > strm + data_len) goto END;
    }
  } else if ((1 << dec->log2_tile_rows) - (dec->height + 63) / 64 == 2) {
    for (i = (2 << dec->log2_tile_columns); i; i--) {
      tmp = ReadTileSize(strmTmp);
      strmTmp += 4 + tmp;
      dec->offset_to_dct_parts += 4 + tmp;
      DBGT_PDEBUG("Tile row with h=0, skipping %d", tmp);
      if (strmTmp > strm + data_len) goto END;
    }
  }

  DBGT_PDEBUG("First partition size = %d", dec->offset_to_dct_parts);
  DBGT_PDEBUG("Frame tag size = %d", dec->frame_tag_size);

  for (i = 0; i < 8; i++)
  {
      tmp = dec->refresh_frame_flags >> i;
      if (tmp & 0x1) {
        dec->ref_size[i].ref_width = dec->width;
        dec->ref_size[i].ref_height = dec->height;
      }
  }

  status = 1;

END:
  if (dec->offset_to_dct_parts && status) {
    *header_len = dec->offset_to_dct_parts + dec->frame_tag_size;
    *slice_len = data_len - *header_len;
    return HANTRO_OK;
  } else {
    *header_len = data_len;
    *slice_len = 0;
    if (status) return HANTRO_OK;
    else return HANTRO_NOK;
  }
}
