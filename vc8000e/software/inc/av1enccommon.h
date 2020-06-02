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
--------------------------------------------------------------------------------
--
--  Description : AV1 Encoder common definitions for control code and system model
--
------------------------------------------------------------------------------*/

#ifndef __AV1_ENC_COMMON_H__
#define __AV1_ENC_COMMON_H__

#include "base_type.h"
#include "enums.h"

typedef enum {
  AV1_EIGHTTAP,
  AV1_EIGHTTAP_SMOOTH,
  AV1_EIGHTTAP_SHARP,
  AV1_BILINEAR,
  AV1_SWITCHABLE,
  AV1_INTERP_FILTER_END = AV1_BILINEAR+1,
} Av1_InterpFilter;

#define CDF_SIZE(x) ((x) + 1)
#define KF_MODE_CONTEXTS 5
#define BLOCK_SIZE_GROUPS 4
#define TXB_SKIP_CONTEXTS 13
#define EOB_COEF_CONTEXTS 9
#define DC_SIGN_CONTEXTS 3
#define SIG_COEF_CONTEXTS_EOB 4
#define SIG_COEF_CONTEXTS_2D 26
#define SIG_COEF_CONTEXTS_1D 16
#define SIG_COEF_CONTEXTS (SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D)
#define COEFF_BASE_CONTEXTS (SIG_COEF_CONTEXTS)
#define LEVEL_CONTEXTS 21
#define BR_CDF_SIZE (4)
#define COEFF_BASE_RANGE (4 * (BR_CDF_SIZE - 1))
#define TOKEN_CDF_Q_CTXS 4
#define NUM_BASE_LEVELS 2

#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)
#define CDF_INIT_TOP 32768
#define CDF_SHIFT (15 - CDF_PROB_BITS)

#define AV1_CDEF_STRENGTH_NUM 8

// Number of possible contexts for a color index.
// As can be seen from av1_get_palette_color_index_context(), the possible
// contexts are (2,0,0), (2,2,1), (3,2,0), (4,1,0), (5,0,0). These are mapped to
// a value from 0 to 4 using 'palette_color_index_context_lookup' table.
#define PALETTE_COLOR_INDEX_CONTEXTS 5

// Palette Y mode context for a block is determined by number of neighboring
// blocks (top and/or left) using a palette for Y plane. So, possible Y mode'
// context values are:
// 0 if neither left nor top block uses palette for Y plane,
// 1 if exactly one of left or top block uses palette for Y plane, and
// 2 if both left and top blocks use palette for Y plane.
#define PALETTE_Y_MODE_CONTEXTS 3

// Palette UV mode context for a block is determined by whether this block uses
// palette for the Y plane. So, possible values are:
// 0 if this block doesn't use palette for Y plane.
// 1 if this block uses palette for Y plane (i.e. Y palette size > 0).
#define PALETTE_UV_MODE_CONTEXTS 2

// Map the number of pixels in a block size to a context
//   64(BLOCK_8X8, BLOCK_4x16, BLOCK_16X4)  -> 0
//  128(BLOCK_8X16, BLOCK_16x8)             -> 1
//   ...
// 4096(BLOCK_64X64)                        -> 6
#define PALATTE_BSIZE_CTXS 7

#define SWITCHABLE_FILTER_CONTEXTS ((AV1_BILINEAR + 1) * 4)
#define TX_SIZE_CONTEXTS 3

#define MV_CLASSES 11
#define CLASS0_BITS 1 /* bits at integer precision for class 0 */
#define CLASS0_SIZE (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)
#define MV_BITS_CONTEXTS 6
#define MV_FP_SIZE 4

#define MV_MAX_BITS (MV_CLASSES + CLASS0_BITS + 2)
#define MV_MAX ((1 << MV_MAX_BITS) - 1)
#define MV_VALS ((MV_MAX << 1) + 1)

#define MV_IN_USE_BITS 14
#define MV_UPP (1 << MV_IN_USE_BITS)
#define MV_LOW (-(1 << MV_IN_USE_BITS))
#define MV_JOINTS 4

#define MAX_SEGMENTS 8
#define SEG_TREE_PROBS (MAX_SEGMENTS - 1)

#define SEG_TEMPORAL_PRED_CTXS 3
#define SPATIAL_PREDICTION_PROBS 3

/* ****** Some still developing features ****** */ 
#define AV1_USE_ROUGH_SEARCH 0

#ifdef BETA
#undef AV1_USE_ROUGH_SEARCH
#define AV1_USE_ROUGH_SEARCH 1
#define AV1_INTERP_FILTER_SWITCH_EN
//#define AV1_INTERP_FILTER_DUAL_EN
//#define AV1_INTRA_MODE_IMPROVE
#define CDEF_BLOCK_STRENGTH_ENABLE
//#define DELTA_LF_ENABLE
#define AV1_Q_MAPPING_SMALL_QFACTOR

#if defined(AV1_INTRA_MODE_IMPROVE) && !defined(AV1_Q_MAPPING_SMALL_QFACTOR)
  #define AV1_Q_MAPPING_SMALL_QFACTOR
#endif
#endif
/******************************/

typedef struct {
  u16 class0_fp_cdf[CLASS0_SIZE][CDF_SIZE(MV_FP_SIZE)];
  u16 fp_cdf[CDF_SIZE(MV_FP_SIZE)];
  u16 class0_hp_cdf[CDF_SIZE(2)];
  u16 hp_cdf[CDF_SIZE(2)];
  u16 sign_cdf[CDF_SIZE(2)];
  u16 bits_cdf[MV_OFFSET_BITS][CDF_SIZE(2)];
  u16 class0_cdf[CDF_SIZE(CLASS0_SIZE)];
} nmv_component;

typedef struct {
  u16 joints_cdf[CDF_SIZE(MV_JOINTS)];
  nmv_component comps[2];
} nmv_context;

struct segmentation_probs {
  u16 tree_cdf[CDF_SIZE(MAX_SEGMENTS)];
  u16 pred_cdf[SEG_TEMPORAL_PRED_CTXS][CDF_SIZE(2)];
  u16 spatial_pred_seg_cdf[SPATIAL_PREDICTION_PROBS][CDF_SIZE(MAX_SEGMENTS)];
};

typedef struct frame_contexts {
  u16 wedge_idx_cdf[BLOCK_SIZES_ALL][CDF_SIZE(16)];
  u16 cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)];
  u16 inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(TX_TYPES)];
  u16 uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)];
  u16 kf_y_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][CDF_SIZE(INTRA_MODES)];
  u16 y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)];
  u16 mv_classes_cdf[2][CDF_SIZE(MV_CLASSES)]; // 0 -mvy, 1 -mvx
  u16 eob_flag_cdf1024[PLANE_TYPES][2][CDF_SIZE(11)];
  u16 partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)];
  u16 eob_flag_cdf512[PLANE_TYPES][2][CDF_SIZE(10)];
  u16 eob_flag_cdf256[PLANE_TYPES][2][CDF_SIZE(9)];
  u16 eob_flag_cdf128[PLANE_TYPES][2][CDF_SIZE(8)];
  u16 inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(INTER_COMPOUND_MODES)];
  u16 cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)];
  u16 eob_flag_cdf64[PLANE_TYPES][2][CDF_SIZE(7)];
  u16 angle_delta_cdf[DIRECTIONAL_MODES][CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)];
  u16 intra_ext_tx_cdf[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES][CDF_SIZE(TX_TYPES)];
  u16 eob_flag_cdf16[PLANE_TYPES][2][CDF_SIZE(5)];
  u16 filter_intra_mode_cdf[CDF_SIZE(FILTER_INTRA_MODES)];
  u16 eob_flag_cdf32[PLANE_TYPES][2][CDF_SIZE(6)];
  u16 tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH + 1)];
  u16 switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(AV1_BILINEAR)];
  u16 motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)];
  u16 switchable_restore_cdf[CDF_SIZE(RESTORE_SWITCHABLE_TYPES)];
  u16 coeff_base_eob_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB][CDF_SIZE(3)];
  u16 coeff_br_cdf[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS][CDF_SIZE(BR_CDF_SIZE)];
  u16 coeff_base_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS][CDF_SIZE(4)];
  u16 delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)];
  u16 delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(DELTA_LF_PROBS + 1)];
  u16 delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)];
  u16 interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTERINTRA_MODES)];
  nmv_context nmvc;
  u16 intrabc_cdf[CDF_SIZE(2)];
  u16 txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)];
  u16 filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)];
  u16 newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)];
  u16 zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)];
  u16 refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)];
  u16 drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
  u16 intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)];
  u16 comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)];
  u16 skip_mode_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)];
  u16 skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)];
  u16 comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)];
  u16 comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)];
  u16 single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)];
  u16 compound_index_cdf[COMP_INDEX_CONTEXTS][CDF_SIZE(2)];
  u16 comp_group_idx_cdf[COMP_GROUP_IDX_CONTEXTS][CDF_SIZE(2)];
  u16 compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES - 1)];
  u16 interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(2)];
  u16 wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
  u16 obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
  u16 comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)];
  u16 uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1][CDF_SIZE(2)];
  u16 wiener_restore_cdf[CDF_SIZE(2)];
  u16 sgrproj_restore_cdf[CDF_SIZE(2)];
  u16 txb_skip_cdf[TX_SIZES][TXB_SKIP_CONTEXTS][CDF_SIZE(2)];
  u16 eob_extra_cdf[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][CDF_SIZE(2)];
  u16 dc_sign_cdf[PLANE_TYPES][DC_SIGN_CONTEXTS][CDF_SIZE(2)];
  
  //
  //u16 palette_y_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)];
  //u16 palette_uv_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)];
  //u16 palette_y_color_index_cdf[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)];
  //u16 palette_uv_color_index_cdf[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)];
  //u16 palette_y_mode_cdf[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][CDF_SIZE(2)];
  //u16 palette_uv_mode_cdf[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)];
  //nmv_context ndvc;
  //struct segmentation_probs seg;
  //u16 delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(DELTA_LF_PROBS + 1)];

  int initialized;
} FRAME_CONTEXT;

// Table that converts 0-51 Q-range values passed in outside to the Qindex
// range used internally.
#ifdef AV1_Q_MAPPING_SMALL_QFACTOR
static const int quantizer_to_qindex[] = {
  1, 1, 1, 1, 2, 3, 4, 5, 7, 9,
  11, 13, 15, 18, 21, 25, 29, 33, 37, 43,
  50, 56, 64, 73, 82, 93, 101, 107, 115, 123,
  129, 136, 143, 150, 156, 163, 168, 175, 182, 188,
  194, 200, 206, 212, 219, 224, 230, 237, 242, 248,
  254, 254};
#else
static const int quantizer_to_qindex[] = {
  1,  1,  2,  3,  4,  5,  6,  7,  8,  9,
  10, 12, 13, 16, 18, 21, 24, 27, 31, 35,
  40, 45, 51, 57, 64, 72, 82, 92, 99, 106,
  110,118,124,131,140,144,151,155,163,167,
  174,178,184,189,196,203,209,216,224,230,
  237,244};
#endif

static const int qp_to_lf[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  1,  1,  1,  2,  3,  4,
  4,  4,  5,  6,  7,  7,  9,  9,  11, 11,
  12, 15, 16, 19,  21, 23, 25, 30, 32, 36,
  39, 44, 49, 56,  62, 63, 63, 63, 63, 63,
  63, 63};

#endif
