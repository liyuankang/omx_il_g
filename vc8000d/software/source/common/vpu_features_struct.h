#ifndef __VPU_FEATURES_STRUCT_H__
#define __VPU_FEATURES_STRUCT_H__
struct DecHwFeatures {
  u32 id;
  u32 id_mask;
  /* decoding formats related */
  u32 hevc_support;
  u32 hevc_main10_support;
  u32 vp9_support;
  u32 vp9_profile2_support;
  /* G1_H264 - 0: H264 not supported; 1 - Baseline; 2 - Main; 3 - High */
  u32 h264_support;
  u32 h264_high10_support;
  u32 mvc_support;
  u32 mpeg4_support;
  u32 custom_mpeg4_support;
  u32 mpeg2_support;
  u32 sorenson_spark_support;
  u32 vc1_support;
  u32 jpeg_support;
  u32 jpeg_esupport;
  u32 jpeg_prog_support;
  u32 rv_support;
  u32 vp8_support;
  u32 webp_support;
  u32 vp7_support;
  u32 vp6_support;
  u32 avs_support;
  u32 avs_plus_support;
  u32 avs2_support;
  u32 avs2_main10_support;
  /* resolution related */
  u32 hevc_max_dec_pic_width;
  u32 hevc_max_dec_pic_height;
  u32 vp9_max_dec_pic_width;
  u32 vp9_max_dec_pic_height;
  u32 h264_max_dec_pic_width;
  u32 h264_max_dec_pic_height;
  u32 mpeg4_max_dec_pic_width;
  u32 mpeg4_max_dec_pic_height;
  u32 mpeg2_max_dec_pic_width;
  u32 mpeg2_max_dec_pic_height;
  u32 sspark_max_dec_pic_width;
  u32 sspark_max_dec_pic_height;
  u32 vc1_max_dec_pic_width;
  u32 vc1_max_dec_pic_height;
  u32 rv_max_dec_pic_width;
  u32 rv_max_dec_pic_height;
  u32 vp8_max_dec_pic_width;
  u32 vp8_max_dec_pic_height;
  u32 vp7_max_dec_pic_width;
  u32 vp7_max_dec_pic_height;
  u32 vp6_max_dec_pic_width;
  u32 vp6_max_dec_pic_height;
  u32 avs_max_dec_pic_width;
  u32 avs_max_dec_pic_height;
  u32 divx_max_dec_pic_width;
  u32 divx_max_dec_pic_height;
  u32 img_max_dec_width;
  u32 img_max_dec_height;
  /* Other features */
  u32 g1_strm_128bit_align;
  u32 addr64_support;
  u32 ref_buf_support;
  u32 tiled_mode_support;
  u32 ref_frame_tiled_only;
  u32 ec_support;
  u32 dec_stride_support;
  u32 pp_stride_support;
  u32 field_dpb_support;
  u32 double_buffer_support;
  u32 rfc_support;
  u32 rfc_stride_support;
  u32 ring_buffer_support;
  u32 mrb_prefetch;
  u32 statistical_level;
  /* decoding flow supported */
  u32 h264_slice_ec_support;
  u32 tile_decoding_support;
  u32 jpeg_slice_decoding_support;
  u32 low_latency_mode_support;
  u32 has_2nd_pipeline;
  u32 has_2nd_h264_pipeline;
  u32 has_2nd_jpeg_pipeline;
  u32 partial_decoding_support;
  /* registers compatibility */
  u32 is_legacy_dec_mode;
  u32 strm_len_32bits;
  u32 pic_size_reg_unified;
  /* pp related features */
  u32 max_ppu_count;
  u32 pp_support;
  enum {
    NO_PP = 0,
    G1_NATIVE_PP,
    G2_NATIVE_PP,
    FIXED_DS_PP,
    UNFIED_PP
  } pp_version;
  u32 max_pp_out_pic_width[5];
  u32 max_pp_out_pic_height[5];
  /* down scaling is also supported when pp input size (i.e., cropping output) exceeds pp max output size */
  u32 pp_dscaling_exceeding_max_size;
  u32 jpeg_pp_force_e;
  u32 dscale_support[5];
  u32 uscale_support[5];
  u32 flexible_scale_support;
  u32 crop_support;
  u32 crop_step_unit;
  u32 crop_step_rshift;
  u32 rotation_support;
  u32 dither_support;
  /* output format supported */
  u32 fmt_tile_support[5];
  u32 fmt_p010_support;
  u32 fmt_customer1_support;
  u32 fmt_rgb_support;
  u32 pp_pad_e;
  u32 pp_planar_support;
  u32 pp_yuv420_101010_support;
  u32 pp_dscale_with_less_linebuf;
  u32 pp_p010_msb;
  u32 pp_delogo;
  u32 second_crop_support;
  u32 pp_lanczos[5];
  u32 rgb_line_stride_support;
};
#endif
