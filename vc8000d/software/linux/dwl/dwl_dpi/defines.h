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

#ifndef DEFINES_H
#define DEFINES_H

#include "basetype.h"

#define CHROMA_400 0  /* H264 only, used to specify mono_chroma*/
#define SUPPORT_H264_FIELD 0
#define MAX_PIC_WIDTH_IN_CTBS (1024)
#define MAX_PIC_HEIGHT_IN_CTBS (256)
#define MAX_CTB_SIZE 64
#define BLOCK_WIDTH (MAX_CTB_SIZE + 4 + 4)
#define BLOCK_HEIGHT (MAX_CTB_SIZE + 4)
#define BLOCK_WIDTH_C (MAX_CTB_SIZE / 2 + 4 + 4)
#define BLOCK_HEIGHT_C (MAX_CTB_SIZE / 2 + 4)
#define MAX_CBS_IN_CTB_DIM 8
#define MAX_MBS_IN_CTB_DIM 4
#define MAX_CB_DEPTH 3
#define MAX_PIC_WIDTH_IN_CBS MAX_PIC_WIDTH_IN_CTBS* MAX_CBS_IN_CTB_DIM
#define MAX_NUM_CTBS MAX_PIC_WIDTH_IN_CTBS * MAX_PIC_HEIGHT_IN_CTBS
#define NUM_SW_REGS 512 /* Currently in use 292 */
#define AMOUNT_OF_CABAC_CONTEXTS 160
#define MAX_TRAFO_DEPTH 4 /* TODO */
#define MAX_DPB_SIZE 16
#define MAX_TILE_COLS 20
#define MAX_CUS_IN_APF_SCALE_NONE 10
#define MAX_CUS_IN_APF_SCALE_DOWN 5
#define MAX_APF_PARTITIONS 128 /* luma fwd+bwd and chroma fwd+bwd */
#define MAX_UNITS_IN_APF_WINDOW 16

#define MAX_PIC_WIDTH (MAX_PIC_WIDTH_IN_CTBS* MAX_CTB_SIZE)
#define MAX_PIC_HEIGHT (MAX_PIC_HEIGHT_IN_CTBS* MAX_CTB_SIZE)

#define ASIC_CABAC_INIT_BUFFER_SIZE 3 * AMOUNT_OF_CABAC_CONTEXTS
#define ASIC_POC_BUFFER_SIZE \
  (2 + MAX_DPB_SIZE + MAX_DPB_SIZE* MAX_DPB_SIZE) * sizeof(u32)

#define MAX_NUM_PPUS 4

/* avs2 */
/*  Picture types */
#define INTRA_IMG                    0   //!< I frame
#define B_IMG                        2   //!< B frame
#define I_IMG                        0   //!< I frame
#define P_IMG                        1   //!< P frame
#define F_IMG                        4   //!< F frame
#define BACKGROUND_IMG               3
#define BP_IMG                       5
/* avs2 end */

struct MV {
  i32 hor;
  i32 ver;
  i32 ref_idx;
};

typedef struct codingUnit_avs2{
int start_x_unit4;	//offset in LCU in unit 4x4
int start_y_unit4;	//offset in LCU in unit 4x4
int CuSize;					//cu size in 4x4
int CuSize_bit;			//cu size bit
int cu_type;
int x;			//offset in pic in unit 4x4
int y;			//offset in pic in unit 4x4
} codingUnit_avs2;


struct CandMv {
  u32 available;
  i32 hor;
  i32 ver;
  i32 ref_idx;
  u32 ref_pic;
  u32 ltr;
  i32 poc_diff;
};

/* Reference picture list */
struct RefPicList {
  u8 * p_ref; /* h264 only */
  u32 index;
  i32 weight_y, offset_y;
  i32 weight_cb, offset_cb;
  i32 weight_cr, offset_cr;
};

struct RlcWord {
  i32 level;
  u32 run;
  u32 last;
};

enum SubBlkPat{
  /* 4x4 blocks */
  SBP_0       = 8,
  SBP_1       = 4,
  SBP_2       = 2,
  SBP_3       = 1,
  /* 8x4 and 4x8 blocks */
  SBP_TOP     = SBP_0 | SBP_1,
  SBP_LEFT    = SBP_0 | SBP_2,
  SBP_RIGHT   = SBP_1 | SBP_3,
  SBP_BOTTOM  = SBP_2 | SBP_3,
  /* Combinations */
  SBP_BOTH    = SBP_0 | SBP_1 | SBP_2 | SBP_3,
  SBP_ALL     = SBP_0 | SBP_1 | SBP_2 | SBP_3,
  SBP_N_A      = 0
};

enum H264ResidualType{
  LUMA_INTRA_16X16_DC,
  LUMA_INTRA_16X16,
  LUMA_8X8,
  LUMA_4X4,
  CHROMA_DC,
  CHROMA
};

enum PredMode {
  PRED_DIRECT,
  PRED_FORWARD,
  PRED_BACKWARD,
  PRED_INTERPOLATED
};

enum IntraPredType {
  INTRA_16x16,
  INTRA_8x8,
  INTRA_4x4
};

enum ScanDir {
  SCAN_ZIGZAG = 0,
  SCAN_HOR,
  SCAN_VER
};

enum BlockType {
  LUMA,
  CB,
  CR,
  LUMA_DC,
  LUMA_AC
};

enum PictureStructure{
  H264_MBAFF_FALSE = 0,
  H264_MBAFF_TRUE = 1,
  H264_TOP_FIELD = 2,
  H264_BOTTOM_FIELD = 3,
};

enum MbPartMode{
  MB_P_16x16 = 0,
  MB_P_16x8,
  MB_P_8x16,
  MB_P_8x8,
  MB_P_4x4
};

enum SubMbPartMode{
  MB_SP_8x8 = 0,
  MB_SP_8x4,
  MB_SP_4x8,
  MB_SP_4x4
};

enum MbType{
  /* H.264 */
  MB_H264_I_16x16     = 0,
  MB_H264_I_4x4       = 1,
  MB_H264_IPCM        = 2,
  MB_H264_Inter       = 3,
  MB_H264_I_8x8       = 4,
};

/* DCT types */
enum H264DctType {
  DCT_8x8 = 0,
  DCT_4x4 = 1
};

/* H264 only */
struct MbStorage{
  struct MV mvs[2][16];/*[direction][mvs]*/
  u32 mb_type;
  i32 ref_idx[2][4];  //TODO may be deleted later, use mvs.ref_idx instead
  enum PredMode pred_mode[4]; /* Prediction mode per block */
};

/* H264 only */
struct ColMbStorage{
  struct MV mvs[4];/*4*luma + 2*chroma*/
  u32 mb_type;
  /*u32 block_type[6];*//* contains intra/inter info for blocks
                              * needed in VC-1*/
};

/* H264 only */
struct Picture{
  //picStruct_e picture_type;
  u32 inter_picture; /*needed in mpeg4 direct mode*/
  u32 b_picture;/*needed in mpeg4 direct mode*/
  u32 vop_num; /*for mpeg4*/
  //u32 mbaff_frame;
  struct ColMbStorage *p_mbs;
};

/* H264 only */
enum VertMvScale{
  ONE_TO_ONE,
  FRM_TO_FLD,
  FLD_TO_FRM
};

enum {
  PART_2Nx2N = 0,
  PART_2NxN,
  PART_Nx2N,
  PART_NxN,
  PART_2NxnU,
  PART_2NxnD,
  PART_nLx2N,
  PART_nRx2N,
};

enum {
  INTRA_PART_2Nx2N = 0,
  INTRA_PART_NxN,
  INTRA_PART_nNxN,
  INTRA_PART_NxnN,
};

enum {
  HW_PRED_FORWARD = 0,
  HW_PRED_BACKWARD,
  HW_PRED_BID,
  HW_PRED_DUAL,
  HW_PRED_SYM,
};

enum {
  MODE_INTRA,
  MODE_INTER,
  MODE_SKIP
};

enum {
  TU_SPLIT_2Nx2N = 0,
  TU_SPLIT_HOR,
  TU_SPLIT_VER,
  TU_SPLIT_NxN,
};


enum {
  DEC_MODE_HEVC = 12,
  DEC_MODE_VP9 = 13,
  DEC_MODE_RAW_PP = 14,
  DEC_MODE_H264_HIGH10P,
  DEC_MODE_AVS2,
  DEC_MODE_MAX,
};

enum {
  VP9_SCALE_NONE = 0,
  VP9_SCALE_DOWN,
  VP9_SCALE_UP
};

enum {
  DEC_OUTPUT_PACKED_10 = 0,
  DEC_OUTPUT_P010 = 1,
  DEC_OUTPUT_CUSTOMER1 = 2,
  DEC_OUTPUT_CUT_8BITS = 3,
};

enum {
  PP_OUT_FMT_YUV420PACKED = 0,
  PP_OUT_FMT_YUV420_P010 = 1,
  PP_OUT_FMT_YUV420_BIGE = 2,
  PP_OUT_FMT_YUV420_8BIT = 3,
  PP_OUT_FMT_YUV400 = 4,
  PP_OUT_FMT_YUV400_P010 = 5,
  PP_OUT_FMT_YUV400_8BIT = 6,
  PP_OUT_FMT_IYUV = 7,
  PP_OUT_FMT_IYUV_P010 = 8,
  PP_OUT_FMT_IYUV_8BIT = 9,
  PP_OUT_FMT_YUV420_10 = 10,
};

#define IS_PP_OUT_PLANAR(fmt) \
  ((fmt) >= PP_OUT_FMT_IYUV && (fmt) <= PP_OUT_FMT_IYUV_8BIT)

#define IS_PP_OUT_MONOCHROME(fmt) \
  ((fmt) >= PP_OUT_FMT_YUV400 && (fmt) <= PP_OUT_FMT_YUV400_8BIT)

#define IS_PP_OUT_SEMIPLANAR(fmt) \
  (((fmt) >= PP_OUT_FMT_YUV420PACKED && (fmt) <= PP_OUT_FMT_YUV420_8BIT) || \
   ((fmt) == PP_OUT_FMT_YUV420_10))

#define IS_PP_OUT_P010(fmt) \
  ((fmt) == PP_OUT_FMT_YUV420_P010 || (fmt) == PP_OUT_FMT_YUV400_P010 || (fmt) == PP_OUT_FMT_IYUV_P010)

#define IS_PP_OUT_8BIT(fmt) \
  ((fmt) == PP_OUT_FMT_YUV420_8BIT || (fmt) == PP_OUT_FMT_YUV400_8BIT || (fmt) == PP_OUT_FMT_IYUV_8BIT)
#endif /* #ifndef DEFINES_H */
