/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------*/

#ifndef INSTANCE_H
#define INSTANCE_H

#include "container.h"
#include "enccommon.h"
#include "encpreprocess.h"
#include "encasiccontroller.h"

#include "hevcencapi.h"     /* Callback type from API is reused */
#include "rate_control_picture.h"
#include "hash.h"
#include "sw_cu_tree.h"
#include "av1_obu.h"

#include "vp9_bitstream.h"
#include "vp9_api.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif


struct av1_ref_buf {
    u32 refFrameId;
    u32 refOrderHint;
    u32 refValid;
};

typedef struct _SW_RefCntBuffer{
    i32 poc;
    i32 ref_count;
    u32 frame_num;
    u32 ref_frame_num[SW_INTER_REFS_PER_FRAME];
    i32 showable_frame;  // frame can be used as show existing frame in future
    VCEncPictureCodingType codingType;
    i8 temporalId;       // temporal ID
} SW_RefCntBuffer;

typedef struct _vcenc_instance_av1
{
  true_e show_existing_frame;
  u32  frame_to_show_map_idx;
  u32  display_frame_id;
  true_e show_frame;
  true_e showable_frame;
  true_e error_resilient_mode;
  true_e disable_cdf_update;
  true_e allow_screen_content_tools;
  true_e force_integer_mv;
  u32  order_hint;
  u8   primary_ref_frame;
  u8   refresh_frame_flags;
  u8   last_frame_idx;
  u8   gold_frame_idx;
  true_e allow_ref_frame_mvs;
  true_e switchable_motion_mode;
  true_e cur_frame_force_integer_mv;
  true_e allow_high_precision_mv;
  true_e disable_frame_end_update_cdf;
  true_e allow_warped_motion;
  true_e refresh_last_frame;
  true_e refresh_bwd_ref_frame;
  true_e refresh_alt2_ref_frame;
  true_e refresh_golden_frame;
  true_e refresh_alt_ref_frame;
  true_e refresh_ext_ref_frame;
  true_e preserve_arf_as_gld;
  true_e render_and_frame_size_different;
  true_e frame_size_override_flag;
  true_e allow_intrabc;
  true_e large_scale_tile;                   // should = false
  true_e using_qmatrix;
  true_e segmentation_enable;
  true_e delta_q_present_flag;
  true_e delta_lf_present_flag;
  true_e delta_lf_multi;
  true_e all_lossless;
  true_e coded_lossless;
  true_e seg_enable;
  SW_TX_MODE tx_mode;
  true_e is_skip_mode_allowed;
  true_e skip_mode_flag;
  true_e reduced_tx_set_used;
  true_e uniform_tile_spacing_flag;
  true_e equal_picture_interval;
  true_e decoder_model_info_present_flag;
  REFERENCE_MODE reference_mode;
  u8   frame_context_idx;                      /* Context to use/update */
  u32  fb_of_context_type[AV1_REF_FRAME_NUM];
  i32  superres_upscaled_width;
  i32  delta_q_res;
  i32  delta_lf_res;

  i32 filter_level[2];  //0-63
  i32 filter_level_u;   //0-63
  i32 filter_level_v;   //0-63
  i32 sharpness_level;  //0-7
  u8  mode_ref_delta_enabled;
  u8  mode_ref_delta_update;
  u8  cdef_damping;     //from 3 to 6
  u8  cdef_bits;
  u8  cdef_strengths[AV1_CDEF_STRENGTH_NUM];   // pri 4 bit + sec 2bit :6bit
  u8  cdef_uv_strengths[AV1_CDEF_STRENGTH_NUM];// pri 4 bit + sec 2bit :6bit

  i8  list0_ref_frame; // LAST_FRAME LAST2_FRAME ... ALTREF_FRAME
  i8  list1_ref_frame; // LAST_FRAME LAST2_FRAME ... ALTREF_FRAME
  u32 mi_cols;
  u32 mi_rows;
  u32 max_tile_width_sb;
  u32 min_log2_tile_cols;
  u32 max_log2_tile_cols;
  u32 max_log2_tile_rows;
  u32 min_log2_tiles;
  u32 tile_cols;
  u32 tile_rows;
  u32 min_log2_tile_rows;
  u32 max_tile_height_sb;
  u32 log2_tile_cols;
  u32 log2_tile_rows;

  i32 LumaDcQpOffset;
  i32 ChromaDcQpOffset;

  Av1_InterpFilter interp_filter;

  SW_RefCntBuffer ref_frame_map[AV1_REF_FRAME_NUM];
  // For encoder, we have a two-level mapping from reference frame type to the corresponding buffer:
  // * 'remapped_ref_idx[i - 1]' maps reference type ��i�� (range: LAST_FRAME ...
  // EXTREF_FRAME) to a remapped index ��j�� (in range: 0 ... REF_FRAMES - 1)
  // * Later, 'cm->ref_frame_map[j]' maps the remapped index ��j�� to a pointer to
  // the reference counted buffer structure RefCntBuffer, taken from the buffer
  // pool cm->buffer_pool->frame_bufs.
  //
  // LAST_FRAME,                        ...,      EXTREF_FRAME
  //      |                                           |
  //      v                                           v
  // remapped_ref_idx[LAST_FRAME - 1],  ...,  remapped_ref_idx[EXTREF_FRAME - 1]
  //      |                                           |
  //      v                                           v
  // ref_frame_map[],                   ...,     ref_frame_map[]
  //
  // Note: INTRA_FRAME always refers to the current frame, so there's no need to have a remapped index for the same.
  i32 remapped_ref_idx[SW_REF_FRAMES];
  i32 encoded_ref_idx[SW_REF_FRAMES];
  true_e is_ref_globle[SW_REF_FRAMES];
  true_e is_valid_idx[SW_REF_FRAMES];
  true_e is_av1_TmpId;
  true_e is_bwd_last_frame;
  true_e is_arf_last_frame;
  true_e is_arf2_last_frame;
  true_e is_preserve_last_as_gld;
  true_e is_preserve_last3_as_gld;
  i32    preserve_last3_as;

  i32 POCtobeDisplayAV1;
  i32 MaxPocDisplayAV1;
  true_e is_RefIdx_inited;
  u8  upDateRefFrame[8][8];  // [gopsize][gopIndex]

  true_e bTxTypeSearch;
}vcenc_instance_av1;

typedef struct _vcenc_instance_vp9
{
    true_e show_existing_frame;
	u32 show_existing_idx;
    u32  frame_to_show_map_idx;
    u32  display_frame_id;
    true_e show_frame;
	true_e showable_frame;
    true_e error_resilient_mode;
    true_e render_and_frame_size_different;

	i32 POCtobeDisplayVP9;
	i32 MaxPocDisplayVP9;

	true_e is_RefIdx_inited;

	u8  upDateRefFrame[8][8];  // [gopsize][gopIndex]

	VP9_REFERENCE_MODE reference_mode;

    vp9_color_space_t color_space;
    vp9_color_range_t color_range;

    int subsampling_x;
    int subsampling_y;

    true_e use_svc;

    u8 intra_only;
	true_e key_frame;

    true_e lossless;
    // Flag signaling that the frame context should be reset to default values.
    // 0 or 1 implies don't reset, 2 reset just the context specified in the
    // frame header, 3 reset all contexts.
    int reset_frame_context;

    int ref_frame_sign_bias[MAX_REF_FRAMES];
	SW_RefCntBuffer ref_frame_map[AV1_REF_FRAME_NUM];

    int lst_fb_idx;
    int gld_fb_idx;
    int alt_fb_idx;

    int refresh_last_frame;
    int refresh_golden_frame;
    int refresh_alt_ref_frame;
    unsigned int frame_context_idx;

    //Compound ref mode
	int comp_fixed_ref;
	int comp_var_ref[2];

    true_e refresh_frame_context; /*Two state: 0 = NO, 1 = Yes*/
    true_e frame_parallel_decoding_mode;

    u8 interp_filter;

    true_e multi_arf_allowed;
    true_e allow_high_precision_mv;

    vp9_loopfilter lf;

    int base_qindex;
    int y_dc_delta_q;
    int uv_dc_delta_q;
    int uv_ac_delta_q;

    true_e segmentation_enable;

    // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
    // MODE_INFO (8-pixel) units.
    int MBs;
    u32 mb_rows, mi_rows;
    u32 mb_cols, mi_cols;
    u32 tile_cols, tile_rows;
    u32 min_log2_tiles;
    u32 max_tile_width_sb;
    int mi_stride;

    u32 log2_tile_cols, log2_tile_rows;
    u32 min_log2_tile_cols, max_log2_tile_cols;
    u32 min_log2_tile_rows, max_log2_tile_rows;

    VP9_TX_MODE tx_mode;

    //FRAME_COUNTS *counts;
    //FRAME_CONTEXT *fc;
    vp9_hbuffer hd_buffer;

    struct Vp9EntropyProbs entropy;
    struct Vp9EntropyProbs entropy_last[VP9_FRAME_CONTEXTS];

	true_e allow_comp_inter_inter;
}vcenc_instance_vp9;

struct vcenc_instance
{
  u32 encStatus;
  asicData_s asic;
  i32 vps_id;     /* Video parameter set id */
  i32 sps_id;     /* Sequence parameter set id */
  i32 pps_id;     /* Picture parameter set id */
  i32 rps_id;     /* Reference picture set id */

  struct buffer stream;
  struct buffer streams[MAX_CORE_NUM];

  EWLHwConfig_t featureToSupport;
  EWLHwConfig_t asic_core_cfg[MAX_SUPPORT_CORE_NUM];
  u32 asic_hw_id[MAX_SUPPORT_CORE_NUM];
  u32 reserve_core_info;

  u8 *temp_buffer;      /* Data buffer, user set */
  u32 temp_size;      /* Size (bytes) of buffer */
  u32 temp_bufferBusAddress;

  // SPS&PPS parameters
  i32 max_cu_size;    /* Max coding unit size in pixels */
  i32 min_cu_size;    /* Min coding unit size in pixels */
  i32 max_tr_size;    /* Max transform size in pixels */
  i32 min_tr_size;    /* Min transform size in pixels */
  i32 tr_depth_intra;   /* Max transform hierarchy depth */
  i32 tr_depth_inter;   /* Max transform hierarchy depth */
  i32 width;
  i32 height;
  i32 enableScalingList;  /* */
  VCEncVideoCodecFormat codecFormat;     /* Video Codec Format: HEVC/H264/AV1 */
  i32 pcm_enabled_flag;  /*enable pcm for HEVC*/
  i32 pcm_loop_filter_disabled_flag;  /*pcm filter*/

  // intra setup
  u32 strong_intra_smoothing_enabled_flag;
  u32 constrained_intra_pred_flag;
  i32 enableDeblockOverride;
  i32 disableDeblocking;

  i32 tc_Offset;

  i32 beta_Offset;

  i32 ctbPerFrame;
  i32 ctbPerRow;
  i32 ctbPerCol;

  /* Minimum luma coding block size of coding units that convey
   * cu_qp_delta_abs and cu_qp_delta_sign and default quantization
   * parameter */
  i32 min_qp_size;
  i32 qpHdr;

  i32 levelIdx;   /*level 5.1 =8*/
  i32 level;   /*level 5.1 =8*/

  i32 profile;   /**/
  i32 tier;

  preProcess_s preProcess;

  /* Rate control parameters */
  vcencRateControl_s rateControl;


  struct vps *vps;
  struct sps *sps;

  i32 poc;      /* encoded Picture order count */
  i32 frameNum; /* frame number in decoding order, 0 for IDR, +1 for each reference frame; used for H.264 */
  i32 idrPicId; /* idrPicId in H264, to distinguish subsequent idr pictures */
  u8 *lum;
  u8 *cb;
  u8 *cr;
  i32 chromaQpOffset;
  i32 enableSao;
  i32 output_buffer_over_flow;
  const void *inst;

  /* H.264 MMO */
  i32 h264_mmo_nops;
  i32 h264_mmo_unref[VCENC_MAX_REF_FRAMES];
  i32 h264_mmo_ltIdx[VCENC_MAX_REF_FRAMES];
  i32 h264_mmo_long_term_flag[VCENC_MAX_REF_FRAMES];

  VCEncSliceReadyCallBackFunc sliceReadyCbFunc;
  void *pAppData[MAX_CORE_NUM];         /* User given application specific data */
  u32 frameCnt;
  u32 videoFullRange;
  u32 sarWidth;
  u32 sarHeight;
  i32 fieldOrder;     /* 1=top first, 0=bottom first */
  u32 interlaced;
#ifdef VIDEOSTAB_ENABLED
  HWStabData vsHwData;
  SwStbData vsSwData;
#endif
  i32 gdrEnabled;
  i32 gdrStart;
  i32 gdrDuration;
  i32 gdrCount;
  i32 gdrAverageMBRows;
  i32 gdrMBLeft;
  i32 gdrFirstIntraFrame;
  u32 roi1Enable;
  u32 roi2Enable;
  u32 roi3Enable;
  u32 roi4Enable;
  u32 roi5Enable;
  u32 roi6Enable;
  u32 roi7Enable;
  u32 roi8Enable;
  u32 ctbRCEnable;
  i32 blockRCSize;
  u32 roiMapEnable;
  u32 RoimapCuCtrl_index_enable;
  u32 RoimapCuCtrl_enable;
  u32 RoimapCuCtrl_ver;
  u32 RoiQpDelta_ver;
  u32 numNalus[MAX_CORE_NUM];        /* Amount of NAL units */
  u32 testId;
  i32 created_pic_num; /* number of pictures currently created */
  /* low latency */
  inputLineBuf_s inputLineBuf;
#if USE_TOP_CTRL_DENOISE
    unsigned int uiFrmNum;
    unsigned int uiNoiseReductionEnable;
    int FrmNoiseSigmaSmooth[5];
    int iFirstFrameSigma;
    int iNoiseL;
    int iSigmaCur;
    int iThreshSigmaCur;
    int iThreshSigmaPrev;
    int iSigmaCalcd;
    int iThreshSigmaCalcd;
    int iSliceQPPrev;
#endif
    i32 insertNewPPS;
    i32 insertNewPPSId;
    i32 maxPPSId;

    u32 maxTLayers; /*max temporal layers*/

    u32 rdoLevel;
    hashctx hashctx;

  /* for smart */
  i32 smartModeEnable;
  i32 smartH264Qp;
  i32 smartHevcLumQp;
  i32 smartHevcChrQp;
  i32 smartH264LumDcTh;
  i32 smartH264CbDcTh;
  i32 smartH264CrDcTh;
  /* threshold for hevc cu8x8/16x16/32x32 */
  i32 smartHevcLumDcTh[3];
  i32 smartHevcChrDcTh[3];
  i32 smartHevcLumAcNumTh[3];
  i32 smartHevcChrAcNumTh[3];
  /* back ground */
  i32 smartMeanTh[4];
  /* foreground/background threashold: maximum foreground pixels in background block */
  i32 smartPixNumCntTh;

  u32 verbose; /* Log printing mode */
  u32 dumpRegister;

  /* for tile*/
  i32 tiles_enabled_flag;
  i32 num_tile_columns;
  i32 num_tile_rows;
  i32 loop_filter_across_tiles_enabled_flag;

  /* L2-cache */
  void *cache;
  u32 channel_idx;

  u32 input_alignment;
  u32 ref_alignment;
  u32 ref_ch_alignment;

  Hdr10DisplaySei    Hdr10Display;
  Hdr10LightLevelSei Hdr10LightLevel;
  Hdr10ColorVui      Hdr10Color;

  u32 RpsInSliceHeader;
  HWRPS_CONFIG sHwRps;

  bool rasterscan;

  /* cu tree look ahead queue */
  u32 pass;
  struct cuTreeCtr cuTreeCtl;
  bool bSkipCabacEnable;
  u32 lookaheadDepth;
  VCEncLookahead lookahead;
  u32 numCuInfoBuf;
  u32 cuInfoBufIdx;
  bool bMotionScoreEnable;
  u32 extDSRatio; /*0=1:1, 1=1:2*/

  /* Multi-core parallel ctr */
  struct sw_picture *pic[MAX_CORE_NUM];
  u32 parallelCoreNum;
  u32 jobCnt;
  u32 reservedCore;
  VCEncStrmBufs streamBufs[MAX_CORE_NUM];  /* output stream buffers */

  /*stream Multi-segment ctrl*/
  streamMultiSeg_s streamMultiSegment;

  /* AV1 frame header */
  vcenc_instance_av1 av1_inst;
  u32 strmHeaderLen;

  /*VP9 inst*/
  vcenc_instance_vp9 vp9_inst;
  //for extra parameters.
  struct queue extraParaQ;

  u32 writeReconToDDR;

  u32 layerInRefIdc;
};

struct instance
{
  struct vcenc_instance vcenc_instance; /* Visible to user */
  struct vcenc_instance *instance;   /* Instance sanity check */
  struct container container;   /* Encoder internal store */
};

struct container *get_container(struct vcenc_instance *instance);

#endif
