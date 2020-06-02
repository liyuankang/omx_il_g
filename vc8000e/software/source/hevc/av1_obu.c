
#include "base_type.h"
#include "sw_put_bits.h"
#include "sw_parameter_set.h"
#include "av1_obu.h"
#include "instance.h"
#include "tools.h"
#include "enccommon.h"
#include "hevcencapi.h"

const u64 kMaximumLeb128Size = 8;
const u64 kMaximumLeb128Value  = 0x7FFFFFFF;
const u8  kLeb128ByteMask      = 0x7f;  // Binary: 01111111

extern void VCEncFindPicToDisplay(VCEncInst inst, const VCEncIn *pEncIn);

void add_trailing_bits(struct buffer *b)
{
    if (buffer_full(b)) return;

    //COMMENT(b, "rbsp_stop_one_bit");
    put_bit_av1(b, 1, 1);
    while (b->bit_cnt % 8)
    {
        //COMMENT(b, "rbsp_alignment_zero_bit");
        put_bit_av1(b, 0, 1);
    }

    while (b->bit_cnt)
    {
        *b->stream++ = b->cache >> 24;
        (*b->cnt)++;
        b->cache <<= 8;
        b->bit_cnt -= 8;
    }
}

u32 write_obu_header(OBU_TYPE obu_type, bool bExtension, i32 obu_extension, struct buffer *b) {
  u32 size;

  size = 1;
  put_bit_av1(b, 0, 1);                      // forbidden bit.
  put_bit_av1(b, (i32)obu_type, 4);
  put_bit_av1(b, bExtension, 1);
  put_bit_av1(b, 1, 1);                      // obu_has_payload_length_field
  put_bit_av1(b, 0, 1);                      // reserved

  if (bExtension) {
      size = 2;
      put_bit_av1(b, obu_extension, 8);
  }

  return size;
}

void write_temporal_delimiter_obu(struct buffer *b)
{
    write_obu_header(OBU_TEMPORAL_DELIMITER, false, 0, b);
    put_bit_av1(b, 0, 8);// TDsize = 0
}

void write_sequence_header_av1(struct sps *s, struct buffer *b) {
  i32 max_frame_width = s->width   - 1;
  i32 max_frame_height = s->height - 1;
  i32 num_bits_width;
  i32 num_bits_height;

  log2i(max_frame_width, &num_bits_width);
  log2i(max_frame_height, &num_bits_height);
  num_bits_width++;
  num_bits_height++;

  ASSERT(num_bits_width <= 16);
  ASSERT(num_bits_height <= 16);

  put_bit_av1(b, num_bits_width - 1, 4);
  put_bit_av1(b, num_bits_height - 1, 4);
  put_bit_av1_32(b, max_frame_width, num_bits_width);
  put_bit_av1_32(b, max_frame_height, num_bits_height);

  /* Placeholder for actually writing to the bitstream */
  if (!s->reduced_still_picture_hdr) {
    s->frame_id_length       = FRAME_ID_LENGTH;
    s->delta_frame_id_length = DELTA_FRAME_ID_LENGTH;

    put_bit_av1(b, s->frame_id_numbers_present_flag, 1);
    if (s->frame_id_numbers_present_flag) {
        put_bit_av1(b, s->delta_frame_id_length - 2, 4);
        put_bit_av1( b, s->frame_id_length - s->delta_frame_id_length - 1, 3);
    }
  }

  // use_128x128_superblock
  put_bit_av1(b, s->use_128x128_superblock, 1);
  put_bit_av1(b, s->enable_filter_intra, 1);
  put_bit_av1(b, s->enable_intra_edge_filter, 1);

  if (!s->reduced_still_picture_hdr) {
      put_bit_av1(b, s->enable_interintra_compound, 1);
      put_bit_av1(b, s->enable_masked_compound, 1);
      put_bit_av1(b, s->enable_warped_motion, 1);
      put_bit_av1(b, s->enable_dual_filter, 1);
      put_bit_av1(b, s->enable_order_hint, 1);

      if (s->enable_order_hint) {
          put_bit_av1(b, s->enable_jnt_comp, 1);
          put_bit_av1(b, s->enable_ref_frame_mvs, 1);
      }

      put_bit_av1(b, s->choose_screen_content_tools, 1);
      ASSERT(0 == s->choose_screen_content_tools);

      if (!s->choose_screen_content_tools)
          put_bit_av1(b, s->force_screen_content_tools, 1);
      ASSERT(0 == s->force_screen_content_tools);

      if (s->enable_order_hint)
          put_bit_av1(b, s->order_hint_bits-1, 3);
  }

  put_bit_av1(b, s->enable_superres, 1);
  put_bit_av1(b, s->enable_cdef, 1);
  put_bit_av1(b, s->enable_restoration, 1);
}


void write_bitdepth_colorspace_sampling(struct sps *s, struct buffer *b) {
    if ((s->bit_depth_luma_minus8 > 0) && (s->general_profile_idc == SW_PROFILE_2))
    {
        put_bit_av1(b, 1, 1);  // high_bitdepth
        if(s->bit_depth_luma_minus8 == AOM_BITS_12)
            put_bit_av1(b, 1, 1);  // twlve_bit
    }
    else
    {
        put_bit_av1(b, s->bit_depth_luma_minus8>0?1:0, 1);
    }

    put_bit_av1(b, s->chroma_format_idc == 0, 1);

    put_bit_av1(b, s->vui_parameters_present_flag && s->vui.ColorDescripPresentFlag, 1);
    if (s->vui_parameters_present_flag && s->vui.ColorDescripPresentFlag)
    {
        put_bit_av1(b, s->vui.ColorPrimaries, 8);
        put_bit_av1(b, s->vui.TransferCharacteristics, 8);
        put_bit_av1(b, s->vui.MatrixCoefficients, 8);
    }

  if (s->chroma_format_idc == 0) {
      put_bit_av1(b, s->vui_parameters_present_flag && s->vui.videoFullRange, 1);
    return;
  }
  
  if (s->vui.ColorPrimaries == AOM_CICP_CP_BT_709 && s->vui.TransferCharacteristics == AOM_CICP_TC_SRGB && s->vui.MatrixCoefficients == AOM_CICP_MC_IDENTITY) {
    // 444
    ASSERT(s->subsampling_x == 0 && s->subsampling_y == 0);
    ASSERT(s->general_profile_idc == SW_PROFILE_1 || (s->general_profile_idc == SW_PROFILE_2 && s->bit_depth_luma_minus8 == 4));
  } else {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
      put_bit_av1(b, s->vui_parameters_present_flag && s->vui.videoFullRange, 1);
    if (s->general_profile_idc == SW_PROFILE_0) {
      // 420 only
      ASSERT(s->subsampling_x == 1 && s->subsampling_y == 1);
    } else if (s->general_profile_idc == SW_PROFILE_1) {
      // 444 only
      ASSERT(s->subsampling_x == 0 && s->subsampling_y == 0);
    } else if (s->general_profile_idc == SW_PROFILE_2) {
      if (s->bit_depth_luma_minus8 == AOM_BITS_12) {
        // 420, 444 or 422, todo
      } else {
        // 422 only
        ASSERT(s->subsampling_x == 1 && s->subsampling_y == 0);
      }
    }

    if (s->subsampling_x == 1 && s->subsampling_y == 1) {
        put_bit_av1(b, s->chroma_sample_position, 2);
    }
  }
  put_bit_av1(b, s->separate_uv_delta_q, 1);
}

u32 write_sequence_header_obu(struct sps *s, struct buffer *b, u8 number_spatial_layers, u8 number_temporal_layers) {
  ASSERT(1 == number_spatial_layers);
  ASSERT(1 <= number_temporal_layers);

  // profile
  put_bit_av1(b, s->general_profile_idc, 3);

  // Still picture
  put_bit_av1(b, s->still_picture, 1);

  // reduced still picture header
  put_bit_av1(b, s->reduced_still_picture_hdr, 1);
  ASSERT(0 == s->reduced_still_picture_hdr);
  {
      ASSERT(0 == s->timing_info_present_flag);
      put_bit_av1(b, s->timing_info_present_flag, 1);

      ASSERT(0 == s->initial_display_delay_present_flag);
      put_bit_av1(b, s->initial_display_delay_present_flag, 1);

      put_bit_av1(b, (number_temporal_layers-1), 5);
      for (u32 i=1; i <= number_temporal_layers; i++ )
      {
          // operating_points_idx
          if(1 == number_temporal_layers)
            put_bit_av1_32(b, 0, 12);
          else
            put_bit_av1_32(b, 0x100 | ((1<<i)-1), 12);

          // seq_level_idx
          put_bit_av1(b, s->general_level_idc, 5);
          ASSERT(s->general_level_idc < 8);
      }
  }

  // frame_width and below
  write_sequence_header_av1(s, b);

  // color_config
  write_bitdepth_colorspace_sampling(s, b);
                          
  // 0 == film_grain_params_present
  put_bit_av1(b, s->film_grain_params_present, 1);

  add_trailing_bits(b);

  return (*b->cnt);
}

int aom_uleb_encode(struct buffer *b, u32 value) {
    u32 tmp;
    if ( (value > kMaximumLeb128Value) || !b || (value == 0) ) {
        return -1;
    }

    do{
        u8 byte = value & 0x7f;
        value >>= 7;

        if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

        put_bit_av1(b, byte, 8);
    } while (value != 0);

    return 0;
}

int write_uleb_obu_size(struct buffer *b, u32 obu_payload_size) {
  if (aom_uleb_encode(b, obu_payload_size) != 0) {
    return VCENC_ERROR;
  }

  return VCENC_OK;
}


static INLINE int frame_is_sframe(const VCEncIn *pEncIn) {
    return false;// cm->frame_type == S_FRAME;
}

static INLINE int av1_preserve_existing_gf(struct vcenc_instance *vcenc_instance) {
    return 0; // !cpi->multi_arf_allowed && cpi->rc.is_src_frame_alt_ref && !cpi->rc.is_src_frame_ext_arf;
}

i32 get_ref_frame_map_idx(struct vcenc_instance *vcenc_instance, const i32 ref_frame) {
    return (ref_frame >= SW_LAST_FRAME && ref_frame <= SW_EXTREF_FRAME) ? vcenc_instance->av1_inst.remapped_ref_idx[ref_frame - SW_LAST_FRAME] : SW_NONE_FRAME;
}

i32 get_encoded_ref_frame_map_idx(struct vcenc_instance *vcenc_instance, const i32 ref_frame) {
    return (ref_frame >= SW_LAST_FRAME && ref_frame <= SW_EXTREF_FRAME) ? vcenc_instance->av1_inst.encoded_ref_idx[ref_frame - SW_LAST_FRAME] : SW_NONE_FRAME;
}

static int get_refresh_mask(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn) {
  if (pEncIn->bIsIDR)
    return 0xFF;

  int refresh_mask = 0;
  i32 remapped_ref_idx;
  // When LAST_FRAME is to get refreshed, the decoder will be notified to get LAST3_FRAME refreshed and then the virtual indexes for all
  // the 3 LAST reference frames will be updated accordingly, i.e.:
  // (1) The original virtual index for LAST3_FRAME will become the new virtual index for LAST_FRAME; and
  // (2) The original virtual indexes for LAST_FRAME and LAST2_FRAME will be shifted and become the new virtual indexes for LAST2_FRAME and LAST3_FRAME.
  remapped_ref_idx = SW_LAST3_FRAME;
  if(( ENCHW_NO == vcenc_instance->av1_inst.is_preserve_last_as_gld) && (vcenc_instance->av1_inst.is_preserve_last3_as_gld) && (COMMON_NO_UPDATE != vcenc_instance->av1_inst.preserve_last3_as)){
        
        if( BRF_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_BWDREF_FRAME;
        else if( ARF2_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_ALTREF2_FRAME;
        else if( ARF_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_ALTREF_FRAME;
        else
            remapped_ref_idx = SW_GOLDEN_FRAME;
  }

  refresh_mask |= (vcenc_instance->av1_inst.refresh_last_frame << get_ref_frame_map_idx(vcenc_instance, remapped_ref_idx));
  refresh_mask |= (vcenc_instance->av1_inst.refresh_bwd_ref_frame << get_ref_frame_map_idx(vcenc_instance, SW_BWDREF_FRAME));
  refresh_mask |= (vcenc_instance->av1_inst.refresh_alt2_ref_frame << get_ref_frame_map_idx(vcenc_instance, SW_ALTREF2_FRAME));

  if (av1_preserve_existing_gf(vcenc_instance)) {
      if (!vcenc_instance->av1_inst.preserve_arf_as_gld) {
          refresh_mask |= (vcenc_instance->av1_inst.refresh_golden_frame << get_ref_frame_map_idx(vcenc_instance, SW_ALTREF_FRAME));
      }
  }
  else {
     refresh_mask |= (vcenc_instance->av1_inst.refresh_golden_frame << get_ref_frame_map_idx(vcenc_instance, SW_GOLDEN_FRAME));
     refresh_mask |= (vcenc_instance->av1_inst.refresh_alt_ref_frame << get_ref_frame_map_idx(vcenc_instance, SW_ALTREF_FRAME));
  }

  return refresh_mask;
}

static INLINE int frame_is_intra_only(VCEncPictureCodingType codingType) {
  return codingType == VCENC_INTRA_FRAME;
}

// Returns 1 if a superres upscaled frame is unscaled and 0 otherwise.
static INLINE int av1_superres_unscaled(struct vcenc_instance *vcenc_instance) {
  // Note: for some corner cases (e.g. cm->width of 1), there may be no scaling
  // required even though cm->superres_scale_denominator != SCALE_NUMERATOR.
  // So, the following check is more accurate.
  return (vcenc_instance->width == vcenc_instance->av1_inst.superres_upscaled_width);
}


static void write_tile_info(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  ASSERT(vcenc_instance->av1_inst.large_scale_tile == 0);
  ASSERT(vcenc_instance->av1_inst.uniform_tile_spacing_flag);

  put_bit_av1(b, vcenc_instance->av1_inst.uniform_tile_spacing_flag, 1);
  ASSERT(vcenc_instance->av1_inst.uniform_tile_spacing_flag);

  if (vcenc_instance->av1_inst.uniform_tile_spacing_flag) {
      // Uniform spaced tiles with power-of-two number of rows and columns
      // tile columns
      int ones = vcenc_instance->av1_inst.log2_tile_cols - vcenc_instance->av1_inst.min_log2_tile_cols;
      while (ones--) {
          put_bit_av1(b, 1, 1);
      }
      if (vcenc_instance->av1_inst.log2_tile_cols < vcenc_instance->av1_inst.max_log2_tile_cols) {
          put_bit_av1(b, 0, 1);
      }

      // rows
      ones = vcenc_instance->av1_inst.log2_tile_rows - vcenc_instance->av1_inst.min_log2_tile_rows;
      while (ones--) {
          put_bit_av1(b, 1, 1);
      }
      if (vcenc_instance->av1_inst.log2_tile_rows < vcenc_instance->av1_inst.max_log2_tile_rows) {
          put_bit_av1(b, 0, 1);
      }
  }

  if (vcenc_instance->av1_inst.tile_rows * vcenc_instance->av1_inst.tile_cols > 1) {
    // todo
      ASSERT((vcenc_instance->av1_inst.tile_rows * vcenc_instance->av1_inst.tile_cols) == 1);
  }
}
/*
void aom_wb_write_literal(struct buffer *b, i32 data, i32 bits) {
    int bit;
    for (bit = bits - 1; bit >= 0; bit--) put_bit_av1(b, (data >> bit) & 1, 1);
}
void aom_wb_write_inv_signed_literal(struct buffer *b, i32 data, i32 bits) {
    aom_wb_write_literal(b, data, bits + 1);
}
*/
static void write_delta_q(i32 QpDelta, i32 bits, struct buffer *b) {
  i32 value;
  value = (QpDelta>=0)? QpDelta : (QpDelta + (1<<bits)) ;
  ASSERT(value >= 0);
  ASSERT(ABS(QpDelta) <= ((1 << bits)-1));
  if (QpDelta != 0) {
      put_bit_av1(b, 1, 1);
      put_bit_av1(b, value, bits);
  } else {
      put_bit_av1(b, 0, 1);
  }
}

static void encode_quantization(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  const int num_planes = vcenc_instance->sps->chroma_format_idc ? 3:1;
  int qp_y = (vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS);
  int qp_uv = MAX(0, MIN(51, qp_y + vcenc_instance->chromaQpOffset));
  int delta_uv_idx = quantizer_to_qindex[qp_uv] - quantizer_to_qindex[qp_y];
  delta_uv_idx = CLIP3(-64, 63, delta_uv_idx);

  put_bit_av1(b, quantizer_to_qindex[(vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS)], 8); // luma_AC
  write_delta_q(vcenc_instance->av1_inst.LumaDcQpOffset, 7, b);                      // luma_DC
  if (num_planes > 1) {
      ASSERT(vcenc_instance->sps->separate_uv_delta_q == ENCHW_NO);
      if (vcenc_instance->sps->separate_uv_delta_q)
          put_bit_av1(b, 0, 1);
      write_delta_q(delta_uv_idx, 7, b);  // chroma_DC
      write_delta_q(delta_uv_idx, 7, b);  // chroma_AC
  }
  ASSERT(vcenc_instance->av1_inst.using_qmatrix == 0);
  put_bit_av1(b, vcenc_instance->av1_inst.using_qmatrix, 1);
  if (vcenc_instance->av1_inst.using_qmatrix) {
      // todo
  }
}

static void encode_segmentation(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  ASSERT(vcenc_instance->av1_inst.segmentation_enable == ENCHW_NO);
  put_bit_av1(b, vcenc_instance->av1_inst.segmentation_enable,1);
  if (!vcenc_instance->av1_inst.segmentation_enable) return;

  // todo
#if 0
  // Write update flags
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    assert(seg->update_map == 1);
    seg->temporal_update = 0;
    assert(seg->update_data == 1);
  } else {
    aom_wb_write_bit(wb, seg->update_map);
    if (seg->update_map) {
      // Select the coding strategy (temporal or spatial)
      av1_choose_segmap_coding_method(cm, xd);
      aom_wb_write_bit(wb, seg->temporal_update);
    }
    aom_wb_write_bit(wb, seg->update_data);
  }

  // Segmentation data
  if (seg->update_data) {
    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = segfeature_active(seg, i, j);
        aom_wb_write_bit(wb, active);
        if (active) {
          const int data_max = av1_seg_feature_data_max(j);
          const int data_min = -data_max;
          const int ubits = get_unsigned_bits(data_max);
          const int data = clamp(get_segdata(seg, i, j), data_min, data_max);

          if (av1_is_segfeature_signed(j)) {
            aom_wb_write_inv_signed_literal(wb, data, ubits);
          } else {
            aom_wb_write_literal(wb, data, ubits);
          }
        }
      }
    }
  }
#endif
}

static void encode_loopfilter(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  ASSERT(!vcenc_instance->av1_inst.coded_lossless);
  if (vcenc_instance->av1_inst.allow_intrabc) return;
  const int num_planes = vcenc_instance->sps->chroma_format_idc ? 3:1;
  int i;

  // Encode the loop filter level and type
  put_bit_av1(b, vcenc_instance->av1_inst.filter_level[0], 6);
  put_bit_av1(b, vcenc_instance->av1_inst.filter_level[1], 6);
  if (num_planes > 1) {
      if (vcenc_instance->av1_inst.filter_level[0] || vcenc_instance->av1_inst.filter_level[1]) {
          put_bit_av1(b, vcenc_instance->av1_inst.filter_level_u, 6);
          put_bit_av1(b, vcenc_instance->av1_inst.filter_level_v, 6);
      }
  }
  put_bit_av1(b, vcenc_instance->av1_inst.sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  put_bit_av1(b, vcenc_instance->av1_inst.mode_ref_delta_enabled, 1);
  ASSERT(vcenc_instance->av1_inst.mode_ref_delta_enabled == false);
  if (vcenc_instance->av1_inst.mode_ref_delta_enabled) {
      // todo
  }
}

static void encode_cdef(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  ASSERT(!vcenc_instance->av1_inst.coded_lossless);
  if (!vcenc_instance->sps->enable_cdef) return;
  if (vcenc_instance->av1_inst.allow_intrabc) return;
  const int num_planes = vcenc_instance->sps->chroma_format_idc ? 3 : 1;

  int i;
  ASSERT(vcenc_instance->av1_inst.cdef_damping > 2);
  put_bit_av1(b, vcenc_instance->av1_inst.cdef_damping -3, 2);
  put_bit_av1(b, vcenc_instance->av1_inst.cdef_bits, 2);

  for (i = 0; i < (1<<vcenc_instance->av1_inst.cdef_bits); i++) {
    put_bit_av1(b, vcenc_instance->av1_inst.cdef_strengths[i], 6);
    if (num_planes > 1)
      put_bit_av1(b, vcenc_instance->av1_inst.cdef_uv_strengths[i], 6);
  }
}

static void encode_restoration_mode(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  ASSERT(!vcenc_instance->av1_inst.all_lossless);
  if (!vcenc_instance->sps->enable_restoration == ENCHW_NO) return;
  if (vcenc_instance->av1_inst.allow_intrabc) return;
  const int num_planes = vcenc_instance->sps->chroma_format_idc ? 3 : 1;

  // todo
#if 0
  int all_none = 1, chroma_none = 1;
  for (int p = 0; p < num_planes; ++p) {
    RestorationInfo *rsi = &cm->rst_info[p];
    if (rsi->frame_restoration_type != RESTORE_NONE) {
      all_none = 0;
      chroma_none &= p == 0;
    }
    switch (rsi->frame_restoration_type) {
      case RESTORE_NONE:
        aom_wb_write_bit(wb, 0);
        aom_wb_write_bit(wb, 0);
        break;
      case RESTORE_WIENER:
        aom_wb_write_bit(wb, 1);
        aom_wb_write_bit(wb, 0);
        break;
      case RESTORE_SGRPROJ:
        aom_wb_write_bit(wb, 1);
        aom_wb_write_bit(wb, 1);
        break;
      case RESTORE_SWITCHABLE:
        aom_wb_write_bit(wb, 0);
        aom_wb_write_bit(wb, 1);
        break;
      default: assert(0);
    }
  }
  if (!all_none) {
    assert(cm->seq_params.sb_size == BLOCK_64X64 ||
           cm->seq_params.sb_size == BLOCK_128X128);
    const int sb_size = cm->seq_params.sb_size == BLOCK_128X128 ? 128 : 64;

    RestorationInfo *rsi = &cm->rst_info[0];

    assert(rsi->restoration_unit_size >= sb_size);
    assert(RESTORATION_UNITSIZE_MAX == 256);

    if (sb_size == 64) {
      aom_wb_write_bit(wb, rsi->restoration_unit_size > 64);
    }
    if (rsi->restoration_unit_size > 64) {
      aom_wb_write_bit(wb, rsi->restoration_unit_size > 128);
    }
  }

  if (num_planes > 1) {
    int s = MIN(cm->subsampling_x, cm->subsampling_y);
    if (s && !chroma_none) {
      aom_wb_write_bit(wb, cm->rst_info[1].restoration_unit_size !=
                               cm->rst_info[0].restoration_unit_size);
      assert(cm->rst_info[1].restoration_unit_size ==
                 cm->rst_info[0].restoration_unit_size ||
             cm->rst_info[1].restoration_unit_size ==
                 (cm->rst_info[0].restoration_unit_size >> s));
      assert(cm->rst_info[2].restoration_unit_size ==
             cm->rst_info[1].restoration_unit_size);
    } else if (!s) {
      assert(cm->rst_info[1].restoration_unit_size ==
             cm->rst_info[0].restoration_unit_size);
      assert(cm->rst_info[2].restoration_unit_size ==
             cm->rst_info[1].restoration_unit_size);
    }
  }
#endif
}

static void write_tx_mode(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  if (vcenc_instance->av1_inst.coded_lossless) {
      return;
  }
  ASSERT(vcenc_instance->av1_inst.tx_mode == SW_TX_MODE_SELECT);
  put_bit_av1(b, vcenc_instance->av1_inst.tx_mode == SW_TX_MODE_SELECT, 1);
}

static void write_global_motion(struct vcenc_instance *vcenc_instance, struct buffer *b) {
  int frame;
  for (frame = SW_LAST_FRAME; frame <= SW_ALTREF_FRAME; ++frame) {
    put_bit_av1(b, vcenc_instance->av1_inst.is_ref_globle[frame], 1);
    // todo
    /*
    const WarpedMotionParams *ref_params = cm->prev_frame ? &cm->prev_frame->global_motion[frame] : &default_warp_params;
    write_global_motion_params(&cm->global_motion[frame], ref_params, b, cm->allow_high_precision_mv);
    */
  }
}

void write_frame_size(struct vcenc_instance *vcenc_instance, i32 frame_size_override, struct buffer *b) {
    ASSERT(frame_size_override == 0);
    ASSERT(vcenc_instance->sps->enable_superres == ENCHW_NO);
}

static bool check_frame_refs_short_signaling(struct vcenc_instance *vcenc_instance) {
    return false;
}

static void write_frame_interp_filter(SW_InterpFilter filter,  struct buffer *b) {
    put_bit_av1(b, filter == SW_SWITCHABLE, 1);
    if (filter != SW_SWITCHABLE)
        put_bit_av1(b, filter, SW_LOG_SWITCHABLE_FILTERS);
}

static INLINE int frame_might_allow_ref_frame_mvs(struct vcenc_instance *vcenc_instance, VCEncPictureCodingType codingType) {
    return !vcenc_instance->av1_inst.error_resilient_mode &&
        vcenc_instance->sps->enable_ref_frame_mvs &&
        vcenc_instance->sps->enable_order_hint &&
        !frame_is_intra_only(codingType);
}

static INLINE int sw_get_msb(unsigned int n) {
  int log = 0;
  unsigned int value = n;
  int i;

  ASSERT(n != 0);

  for (i = 4; i >= 0; --i) {
    const int shift = (1 << i);
    const unsigned int x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  return log;
}

static void write_uncompressed_header_obu(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn, struct buffer *b, VCEncPictureCodingType codingType) {
  // NOTE: By default all coded frames to be used as a reference
  if (vcenc_instance->sps->still_picture) {
    ASSERT(vcenc_instance->av1_inst.show_existing_frame == ENCHW_NO);
    ASSERT(vcenc_instance->av1_inst.show_frame == 1);
    ASSERT(pEncIn->bIsIDR);
  }

  if (!vcenc_instance->sps->reduced_still_picture_hdr) {
    put_bit_av1(b, vcenc_instance->av1_inst.show_existing_frame, 1);  // show_existing_frame
    if (vcenc_instance->av1_inst.show_existing_frame) {
      put_bit_av1(b, vcenc_instance->av1_inst.frame_to_show_map_idx, 3);

      if (vcenc_instance->av1_inst.decoder_model_info_present_flag &&
          vcenc_instance->av1_inst.equal_picture_interval == ENCHW_NO) {
          ASSERT(vcenc_instance->av1_inst.decoder_model_info_present_flag == ENCHW_NO);
          // todo
          // write_tu_pts_info(cm, wb);
      }
      if (vcenc_instance->sps->frame_id_numbers_present_flag) {
          int frame_id_len = vcenc_instance->sps->frame_id_length;
          int display_frame_id = vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.frame_to_show_map_idx].frame_num;
          put_bit_av1_32(b, display_frame_id, frame_id_len);
      }

      return;
    }

    ASSERT(codingType < VCENC_NOTCODED_FRAME);
    VCENC_AV1_FRAME_TYPE frame_type;
    if (codingType == VCENC_INTRA_FRAME)
    {
        frame_type = pEncIn->bIsIDR ? VCENC_AV1_KEY_FRAME : VCENC_AV1_INTRA_ONLY_FRAME;
    }
    else
        frame_type = VCENC_AV1_INTER_FRAME;
    put_bit_av1(b, frame_type, 2);
    put_bit_av1(b, vcenc_instance->av1_inst.show_frame,1);

    if (vcenc_instance->av1_inst.show_frame && vcenc_instance->av1_inst.decoder_model_info_present_flag && vcenc_instance->av1_inst.equal_picture_interval == ENCHW_NO) {
        ASSERT(vcenc_instance->av1_inst.decoder_model_info_present_flag == ENCHW_NO);
        // todo
        // write_tu_pts_info(vcenc_instance, wb);
    }

    if (!vcenc_instance->av1_inst.show_frame) {
        put_bit_av1(b, vcenc_instance->av1_inst.showable_frame, 1);
    }
    if (!(codingType == VCENC_INTRA_FRAME && pEncIn->bIsIDR && vcenc_instance->av1_inst.show_frame)) {
        put_bit_av1(b, vcenc_instance->av1_inst.error_resilient_mode, 1);
    }
  }
  put_bit_av1(b, vcenc_instance->av1_inst.disable_cdf_update, 1);

  if (vcenc_instance->sps->force_screen_content_tools == 2) {
      put_bit_av1(b, vcenc_instance->av1_inst.allow_screen_content_tools, 1);
  } else {
    ASSERT(vcenc_instance->av1_inst.allow_screen_content_tools == vcenc_instance->sps->force_screen_content_tools);
  }

  if (vcenc_instance->av1_inst.allow_screen_content_tools) {
    if (vcenc_instance->sps->force_integer_mv == 2) {
        put_bit_av1(b, vcenc_instance->av1_inst.force_integer_mv, 1);
    } else {
      ASSERT(vcenc_instance->av1_inst.force_integer_mv == vcenc_instance->sps->force_integer_mv);
    }
  } else {
    ASSERT(vcenc_instance->av1_inst.force_integer_mv == 0);
  }

  if (vcenc_instance->sps->frame_id_numbers_present_flag) {
      i32 frame_id_len = vcenc_instance->sps->frame_id_length;
      put_bit_av1_32(b, vcenc_instance->frameNum, frame_id_len);
  }

  put_bit_av1(b, vcenc_instance->av1_inst.frame_size_override_flag, 1);

  if(vcenc_instance->sps->enable_order_hint)
      put_bit_av1(b, vcenc_instance->av1_inst.order_hint, vcenc_instance->sps->order_hint_bits);

  if ((!vcenc_instance->av1_inst.error_resilient_mode) && (codingType != VCENC_INTRA_FRAME)) {
        put_bit_av1(b, vcenc_instance->av1_inst.primary_ref_frame, 3);
  }

  if (vcenc_instance->sps->decoder_model_info_present_flag) {
      ASSERT(vcenc_instance->sps->decoder_model_info_present_flag == 0);
      // todo
  }

  vcenc_instance->av1_inst.refresh_frame_flags = get_refresh_mask(vcenc_instance, pEncIn);
  if (!(pEncIn->bIsIDR && vcenc_instance->av1_inst.show_frame)) {
    put_bit_av1(b, vcenc_instance->av1_inst.refresh_frame_flags, AV1_REF_FRAME_NUM);
  }

  // ref_order_hint
  if (codingType != VCENC_INTRA_FRAME || vcenc_instance->av1_inst.refresh_frame_flags != 0xFF) {
    if (vcenc_instance->av1_inst.error_resilient_mode && vcenc_instance->sps->enable_order_hint) {
        for (int ref_idx = SW_INTRA_FRAME; ref_idx < SW_REF_FRAMES; ref_idx++) {
            put_bit_av1_32(b, vcenc_instance->av1_inst.ref_frame_map[ref_idx].poc, vcenc_instance->sps->order_hint_bits);
        }
    }
  }

  if (pEncIn->bIsIDR) {
    write_frame_size(vcenc_instance, vcenc_instance->av1_inst.frame_size_override_flag, b);
    // render_and_frame_size_different
    ASSERT(vcenc_instance->av1_inst.render_and_frame_size_different == ENCHW_NO);
    put_bit_av1(b, vcenc_instance->av1_inst.render_and_frame_size_different, 1);

    ASSERT(av1_superres_unscaled(vcenc_instance) || !vcenc_instance->av1_inst.allow_intrabc);
    if (vcenc_instance->av1_inst.allow_screen_content_tools && av1_superres_unscaled(vcenc_instance))
        put_bit_av1(b, vcenc_instance->av1_inst.allow_intrabc, 1);
  } else {
    if (codingType == VCENC_INTRA_FRAME) {
      write_frame_size(vcenc_instance, vcenc_instance->av1_inst.frame_size_override_flag, b);
      // render_and_frame_size_different
      ASSERT(vcenc_instance->av1_inst.render_and_frame_size_different == ENCHW_NO);
      put_bit_av1(b, vcenc_instance->av1_inst.render_and_frame_size_different, 1);

      ASSERT(av1_superres_unscaled(vcenc_instance) || !vcenc_instance->av1_inst.allow_intrabc);
      if (vcenc_instance->av1_inst.allow_screen_content_tools && av1_superres_unscaled(vcenc_instance))
          put_bit_av1(b, vcenc_instance->av1_inst.allow_intrabc, 1);
    } else if (codingType != VCENC_INTRA_FRAME || frame_is_sframe(pEncIn)) {
      i8 ref_frame;

      // NOTE: Error resilient mode turns off frame_refs_short_signaling automatically.
      bool frame_refs_short_signaling = false;
      if (vcenc_instance->sps->enable_order_hint)
      {
          frame_refs_short_signaling = check_frame_refs_short_signaling(vcenc_instance);
          put_bit_av1(b, frame_refs_short_signaling, 1);
      }
      else
      {
        ASSERT(frame_refs_short_signaling == false);
      }

      if (frame_refs_short_signaling) {
        const i32 lst_ref = get_ref_frame_map_idx(vcenc_instance, SW_LAST_FRAME);
        const i32 gld_ref = get_ref_frame_map_idx(vcenc_instance, SW_GOLDEN_FRAME);
        put_bit_av1(b, lst_ref, SW_REF_FRAMES_LOG2);
        put_bit_av1(b, gld_ref, SW_REF_FRAMES_LOG2);
      }

      for (ref_frame = SW_LAST_FRAME; ref_frame <= SW_ALTREF_FRAME; ++ref_frame) {
        ASSERT(get_ref_frame_map_idx(vcenc_instance, ref_frame) != SW_NONE_FRAME);
        if (!frame_refs_short_signaling)
          put_bit_av1(b, get_encoded_ref_frame_map_idx(vcenc_instance, ref_frame), SW_REF_FRAMES_LOG2);
        if (vcenc_instance->sps->frame_id_numbers_present_flag) {
          i32 i = get_encoded_ref_frame_map_idx(vcenc_instance, ref_frame);
          i32 frame_id_len = vcenc_instance->sps->frame_id_length;
          i32 diff_len = vcenc_instance->sps->delta_frame_id_length;
          i32 delta_frame_id_minus1 = ((vcenc_instance->frameNum - vcenc_instance->av1_inst.ref_frame_map[i].frame_num +  (1 << frame_id_len)) % (1 << frame_id_len)) - 1;
          if (delta_frame_id_minus1 < 0 || delta_frame_id_minus1 >= (1 << diff_len))
          {
              ASSERT(0);
          }
          put_bit_av1_32(b, delta_frame_id_minus1, diff_len);
        }
      }

      write_frame_size(vcenc_instance, vcenc_instance->av1_inst.frame_size_override_flag, b);
      // render_and_frame_size_different
      ASSERT(vcenc_instance->av1_inst.render_and_frame_size_different == ENCHW_NO);
      put_bit_av1(b, vcenc_instance->av1_inst.render_and_frame_size_different, 1);

      if (vcenc_instance->av1_inst.cur_frame_force_integer_mv) {
          vcenc_instance->av1_inst.allow_high_precision_mv = false;
      } else {
        put_bit_av1(b, vcenc_instance->av1_inst.allow_high_precision_mv, 1);
      }

      write_frame_interp_filter(vcenc_instance->av1_inst.interp_filter, b);

      put_bit_av1(b, vcenc_instance->av1_inst.switchable_motion_mode, 1);
      if (frame_might_allow_ref_frame_mvs(vcenc_instance, codingType)) {
        put_bit_av1(b, vcenc_instance->av1_inst.allow_ref_frame_mvs, 1);
      } else {
        ASSERT(vcenc_instance->av1_inst.allow_ref_frame_mvs == 0);
      }
    }
  }

  const i32 might_bwd_adapt = !(vcenc_instance->sps->reduced_still_picture_hdr) &&
                              !(vcenc_instance->av1_inst.large_scale_tile) &&
                              !(vcenc_instance->av1_inst.disable_cdf_update);

  if (might_bwd_adapt) {
      put_bit_av1(b, vcenc_instance->av1_inst.disable_frame_end_update_cdf, 1);// refresh_frame_context
  }

  write_tile_info(vcenc_instance, b);

  encode_quantization(vcenc_instance, b);    // slice_qp/ slice_delta_qp
  encode_segmentation(vcenc_instance, b);    // slice

  if (quantizer_to_qindex[vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS] > 0) {
      put_bit_av1(b, vcenc_instance->av1_inst.delta_q_present_flag, 1);
      if (vcenc_instance->av1_inst.delta_q_present_flag) {
          put_bit_av1(b, sw_get_msb(vcenc_instance->av1_inst.delta_q_res), 2);
        // delta_lf_parameter
        if (vcenc_instance->av1_inst.allow_intrabc)
        {
          ASSERT(vcenc_instance->av1_inst.delta_lf_present_flag == ENCHW_NO);
        }
        else
            put_bit_av1(b, vcenc_instance->av1_inst.delta_lf_present_flag, 1);
        if (vcenc_instance->av1_inst.delta_lf_present_flag) {
            put_bit_av1(b, sw_get_msb(vcenc_instance->av1_inst.delta_lf_res), 2);
            put_bit_av1(b, vcenc_instance->av1_inst.delta_lf_multi, 1);
        }
      }
  }

  if (vcenc_instance->av1_inst.all_lossless) {
    ASSERT(av1_superres_unscaled(vcenc_instance));
  } else {
    if (!vcenc_instance->av1_inst.coded_lossless) {
      encode_loopfilter(vcenc_instance, b);
      encode_cdef(vcenc_instance, b);
    }
    encode_restoration_mode(vcenc_instance, b);
  }

  write_tx_mode(vcenc_instance, b);

  // frame_reference_mode
  if (codingType != VCENC_INTRA_FRAME) {
    const i32 use_hybrid_pred = vcenc_instance->av1_inst.reference_mode == REFERENCE_MODE_SELECT;
    put_bit_av1(b, use_hybrid_pred, 1);
  }

  // skip_mode_params
  if (codingType != VCENC_INTRA_FRAME && vcenc_instance->av1_inst.is_skip_mode_allowed) 
      put_bit_av1(b, vcenc_instance->av1_inst.skip_mode_flag, 1);

  // allow_warped_motion
  if (!vcenc_instance->av1_inst.error_resilient_mode && !frame_is_intra_only(codingType) && vcenc_instance->sps->enable_warped_motion)
      put_bit_av1(b, vcenc_instance->av1_inst.allow_warped_motion, 1);
  else
  {
    ASSERT(!vcenc_instance->av1_inst.allow_warped_motion);
  }

  put_bit_av1(b, vcenc_instance->av1_inst.reduced_tx_set_used, 1);

  // global_motion_params
  if (!frame_is_intra_only(codingType)) 
      write_global_motion(vcenc_instance, b);

  // file_grain_params
  ASSERT(vcenc_instance->sps->film_grain_params_present == ENCHW_NO);
}

u32 write_frame_header_obu(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn, struct buffer *b, i32 append_trailing_bits, VCEncPictureCodingType codingType) {

  write_uncompressed_header_obu(vcenc_instance, pEncIn, b, codingType);
  if (append_trailing_bits) 
      rbsp_trailing_bits_av1(b);
  return (*b->cnt);
}

i32 AV1RefreshPic(VCEncInst inst, const VCEncIn *pEncIn, u32 *pStrmLen){
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

    struct buffer data;
    u32 data_size;
    u32 obu_header_size = 0;
    u32 obu_payload_size = 0; 
    u8 obu_extension_header;

    struct buffer p, *b;
    p.bit_cnt = 0;
    p.byteCnt = 0;
    p.cache = 0;
    p.cnt = &p.byteCnt;
    p.stream = vcenc_instance->stream.stream + (*pStrmLen);
    p.size = vcenc_instance->stream.size;
    b = &p;

    if (vcenc_instance->av1_inst.show_existing_frame) {
      write_temporal_delimiter_obu(b);
      
      obu_extension_header = vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.frame_to_show_map_idx].temporalId << 5 | 0 << 3 | 0;
      obu_header_size = write_obu_header(OBU_FRAME_HEADER, true, obu_extension_header, b);
      rbsp_flush_bits_av1(b);
      data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
      data.size = b->size - 8 - *b->cnt;
      data.bit_cnt = 0;
      data.byteCnt = 0;
      data_size = 0;
      data.cache = 0;
      data.cnt = &data_size;
      obu_payload_size = write_frame_header_obu(vcenc_instance, pEncIn, &data, 1, VCENC_NOTCODED_FRAME);
      data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
      if (write_uleb_obu_size(b, obu_payload_size) != VCENC_OK) {
        return VCENC_ERROR;
      }
      for (i32 i = 0; i < obu_payload_size; i++)
          put_bit_av1(b, data.stream[i], 8);
      rbsp_flush_bits_av1(b);
    }
    vcenc_instance->av1_inst.show_existing_frame = ENCHW_NO;

    *pStrmLen += p.byteCnt;

    return VCENC_OK;
}

i32 AV1EndOfSequence(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut, u32 *pStrmLen){
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    u32 NalSize = 0, tmp;
    while(1){
        VCEncFindPicToDisplay(vcenc_instance, pEncIn);
        if(vcenc_instance->av1_inst.show_existing_frame == ENCHW_NO)
            return VCENC_OK;
        if(VCENC_ERROR != AV1RefreshPic(vcenc_instance, pEncIn, pStrmLen)){
            tmp = *pStrmLen - NalSize;
            if(tmp > 0){
                pEncOut->pNaluSizeBuf[pEncOut->numNalus++] = tmp;
                pEncOut->pNaluSizeBuf[pEncOut->numNalus] = 0;
                NalSize = *pStrmLen;
            }
        }
        else
            return VCENC_ERROR;
    }
}

i32 VCEncStreamHeaderAV1(VCEncInst inst, const VCEncIn *pEncIn, u32 *pStrmLen, VCEncPictureCodingType codingType, u32 *pNalLen) {
  struct buffer data;
  u32 data_size;
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  u32 obu_header_size = 0;
  u32 obu_payload_size = 0; 
  u8 obu_extension_header;

  struct buffer p, *b;
  p.bit_cnt = 0;
  p.byteCnt = 0;
  p.cache = 0;
  p.cnt = &p.byteCnt;
  p.stream = vcenc_instance->stream.stream + (*pStrmLen);
  p.size = vcenc_instance->stream.size;
  b = &p;

  if (vcenc_instance->av1_inst.is_av1_TmpId == ENCHW_YES)
      write_temporal_delimiter_obu(b);

  // 1 write sequence header obu if KEY_FRAME
  if (pEncIn->bIsIDR) {
    obu_header_size = write_obu_header(OBU_SEQUENCE_HEADER, false, 0, b);
    rbsp_flush_bits_av1(b);
    data.stream = b->stream + 8;// offset 8 bytes to write obu_size
    data.size = b->size - 8;
    
    data.bit_cnt = 0;
    data.byteCnt = 0;
    data_size    = 0;
    data.cache   = 0;
    data.cnt     = &data_size;
    obu_payload_size = write_sequence_header_obu(vcenc_instance->sps, &data, 1, vcenc_instance->sps->max_num_sub_layers);
    data.stream = b->stream + 8;
    if (write_uleb_obu_size(b, obu_payload_size) != VCENC_OK) {
      return VCENC_ERROR;
    }
    //
    for (i32 i = 0; i < obu_payload_size;i++)
        put_bit_av1(b, data.stream[i], 8);
    rbsp_flush_bits_av1(b);
  }

  if (vcenc_instance->av1_inst.show_existing_frame) {
    // 2 Write Frame Header OBU. show_existing_frame = 1
    obu_extension_header = (vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.frame_to_show_map_idx].temporalId << 5) | 0 << 3 | 0;
    obu_header_size = write_obu_header(OBU_FRAME_HEADER, true, obu_extension_header, b);
    rbsp_flush_bits_av1(b);
    data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
    data.size = b->size - 8 - *b->cnt;
    data.bit_cnt = 0;
    data.byteCnt = 0;
    data_size = 0;
    data.cache = 0;
    data.cnt = &data_size;
    obu_payload_size = write_frame_header_obu(vcenc_instance, pEncIn, &data, 1, codingType);
    data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
    if (write_uleb_obu_size(b, obu_payload_size) != VCENC_OK) {
      return VCENC_ERROR;
    }
    for (i32 i = 0; i < obu_payload_size; i++)
        put_bit_av1(b, data.stream[i], 8);
    rbsp_flush_bits_av1(b);
    vcenc_instance->av1_inst.show_existing_frame = ENCHW_NO;
    
    *pNalLen = p.byteCnt;
  }

  // 3 Write Frame Header OBU. show_existing_frame = 0
  obu_extension_header = (pEncIn->gopCurrPicConfig.temporalId << 5) | (0 << 3) | 0;
  obu_header_size = write_obu_header(OBU_FRAME_HEADER, true, obu_extension_header, b);
  rbsp_flush_bits_av1(b);
  data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
  data.size = b->size - 8 - *b->cnt;
  data.bit_cnt = 0;
  data.byteCnt = 0;
  data_size = 0;
  data.cache = 0;
  data.cnt = &data_size;
  obu_payload_size = write_frame_header_obu(vcenc_instance, pEncIn, &data, 1, codingType);
  data.stream = b->stream + *b->cnt + 8;// offset 8 bytes to write obu_size
  if (write_uleb_obu_size(b, obu_payload_size) != VCENC_OK) {
      return VCENC_ERROR;
  }
  for (i32 i = 0; i < obu_payload_size; i++)
      put_bit_av1(b, data.stream[i], 8);
  rbsp_flush_bits_av1(b);

  *pStrmLen += p.byteCnt;
  hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, p.byteCnt);
  
  return VCENC_OK;
}

