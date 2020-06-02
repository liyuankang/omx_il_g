/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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

#ifndef __PPU__
#define __PPU__

#include "basetype.h"
#include "dwl.h"
#include "dwlthread.h"
#include "decapicommon.h"
#include "vpufeature.h"

#define PP_OUT_FMT_YUV420PACKED 0
#define PP_OUT_FMT_YUV420_P010 1
#define PP_OUT_FMT_YUV420_BIGE 2
#define PP_OUT_FMT_YUV420_8BIT 3
#define PP_OUT_FMT_YUV400 4       /* A.k.a., Monochrome/luma only*/
#define PP_OUT_FMT_YUV400_P010 5
#define PP_OUT_FMT_YUV400_8BIT 6
#define PP_OUT_FMT_IYUVPACKED 7
#define PP_OUT_FMT_IYUV_P010 8
#define PP_OUT_FMT_IYUV_8BIT 9
#define PP_OUT_FMT_YUV420_10 10
#define PP_OUT_FMT_RGB       11

#define PP_OUT_RGB888 0
#define PP_OUT_BGR888 1
#define PP_OUT_R16G16B16 2
#define PP_OUT_B16G16R16 3
#define PP_OUT_ARGB888 4
#define PP_OUT_ABGR888 5
#define PP_OUT_A2R10G10B10 6
#define PP_OUT_A2B10G10R10 7

#define IS_PACKED_RGB(fmt) \
          ((fmt == PP_OUT_RGB888) || \
          (fmt == PP_OUT_ARGB888) || \
          (fmt == PP_OUT_A2R10G10B10) || \
          (fmt == PP_OUT_R16G16B16)) 

#define IS_PLANAR_RGB(fmt) \
          ((fmt == PP_OUT_RGB888) || \
          (fmt == PP_OUT_BGR888) || \
          (fmt == PP_OUT_R16G16B16) || \
          (fmt == PP_OUT_B16G16R16))

#define IS_ALPHA_FORMAT(fmt) \
         ((fmt == PP_OUT_ARGB888) || \
          (fmt == PP_OUT_ABGR888) || \
          (fmt == PP_OUT_A2R10G10B10) || \
          (fmt == PP_OUT_A2B10G10R10))
struct PPUnitRegs {
  u32 PP_OUT_E_U;
  u32 PP_OUT_TILE_E_U;
  u32 PP_OUT_MODE;
  u32 PP_CR_FIRST;
  u32 PP_OUT_RGB_FMT_U;
  u32 PP_RGB_PLANAR_U;
  u32 PP_OUT_SWAP_U;
  u32 HOR_SCALE_MODE_U;
  u32 VER_SCALE_MODE_U;
  u32 OUT_FORMAT_U;
  u32 SCALE_HRATIO_U;
  u32 SCALE_WRATIO_U;
  u32 WSCALE_INVRA_U;
  u32 WSCALE_INVRA_EXT_U;
  u32 HSCALE_INVRA_U;
  u32 HCALE_INVRA_EXT_U;
  u32 PP_OUT_LU_BASE_U_MSB;
  u32 PP_OUT_LU_BASE_U_LSB;
  u32 PP_OUT_CH_BASE_U_MSB;
  u32 PP_OUT_CH_BASE_U_LSB;
  u32 PP_OUT_Y_STRIDE;
  u32 PP_OUT_C_STRIDE;
  u32 FLIP_MODE_U;
  u32 CROP_STARTX_U;
  u32 ROTATION_MODE_U;
  u32 CROP_STARTY_U;
  u32 PP_IN_WIDTH_U;
  u32 PP_IN_HEIGHT_U;
  u32 PP_OUT_WIDTH_U;
  u32 PP_OUT_HEIGHT_U;
  u32 PP_OUT_P010_FMT_U;
  u32 PP_OUT_LU_BOT_BASE_U_MSB;
  u32 PP_OUT_LU_BOT_BASE_U_LSB;
  u32 PP_OUT_CH_BOT_BASE_U_MSB;
  u32 PP_OUT_CH_BOT_BASE_U_LSB;
  u32 PP_CROP2_STARTX_U;
  u32 PP_CROP2_STARTY_U;
  u32 PP_CROP2_OUT_WIDTH_U;
  u32 PP_CROP2_OUT_HEIGHT_U;
  u32 PP_OUT_R_BASE_U_MSB;
  u32 PP_OUT_R_BASE_U_LSB;
  u32 PP_OUT_G_BASE_U_MSB;
  u32 PP_OUT_G_BASE_U_LSB;
  u32 PP_OUT_B_BASE_U_MSB;
  u32 PP_OUT_B_BASE_U_LSB;
  u32 PP_OUT_LANCZOS_TBL_BASE_U_MSB;
  u32 PP_OUT_LANCZOS_TBL_BASE_U_LSB;
};

extern struct PPUnitRegs ppu_regs[DEC_MAX_OUT_COUNT];

/* PPU internal config */
/* Compared with PpUnitConfig, more internal member variables are added,
   which should not be exposed to external user. */
typedef struct _PpUnitIntConfig {
  u32 enabled;    /* PP unit enabled */
  u32 tiled_e;    /* PP unit tiled4x4 output enabled */
  u32 rgb;
  u32 rgb_planar;
  u32 cr_first;   /* CrCb instead of CbCr */
  u32 luma_offset;   /* luma offset of current PPU to pp buffer start address */
  u32 chroma_offset;
  u32 luma_size;     /* size of luma/chroma for ppu buffer */
  u32 chroma_size;
  u32 pixel_width;   /* pixel bit depth store in external memory */
  u32 shaper_enabled;
  u32 planar;        /* Planar output */
  DecPicAlignment align;    /* alignment for current PPU */
  u32 ystride;
  u32 cstride;
  u32 false_ystride;
  u32 false_cstride;
  struct {
    u32 enabled;  /* whether cropping is enabled */
    u32 set_by_user;   /* cropping set by user */
    u32 x;        /* cropping start x */
    u32 y;        /* cropping start y */
    u32 width;    /* cropping width */
    u32 height;   /* cropping height */
  } crop;
  struct {
    u32 enabled;
    u32 x;        /* cropping start x */
    u32 y;        /* cropping start y */
    u32 width;    /* cropping width */
    u32 height;   /* cropping height */
  } crop2;
  struct {
    u32 enabled;  /* whether scaling is enabled */
    u32 set_by_user;   /* scaling set by user */
    u32 ratio_x;  /* 0 indicate flexiable scale */
    u32 ratio_y;
    u32 width;    /* scaled output width */
    u32 height;   /* scaled output height */
  } scale;
  u32 monochrome; /* PP output monochrome (luma only) */
  u32 out_format;
  u32 out_p010;
  u32 out_1010;
  u32 out_I010;
  u32 out_L010;
  u32 out_cut_8bits;
  u32 video_range;
  u32 range_max;
  u32 range_min;
  u32 rgb_format;
  u32 rgb_stan;
  struct DWLLinearMem lanczos_table;
} PpUnitIntConfig;

u32 CheckPpUnitConfig(struct DecHwFeatures *hw_feature,
                      u32 in_width,
                      u32 in_height,
                      u32 interlace,
                      PpUnitIntConfig *ppu_cfg);
u32 CalcPpUnitBufferSize(PpUnitIntConfig *ppu_cfg, u32 mono_chrome);

void PpUnitSetIntConfig(PpUnitIntConfig *ppu_int_cfg,
                        PpUnitConfig *ppu_ext_cfg,
                        u32 pixel_width,
                        u32 frame_only,
                        u32 mono_chrome);

void PPSetRegs(u32 *pp_regs,
               struct DecHwFeatures *p_hw_feature,
               PpUnitIntConfig *ppu_cfg,
               addr_t ppu_out_bus_addr,
               u32 mono_chrome,
               u32 bottom_field_flag);

enum DecPictureFormat TransUnitConfig2Format(PpUnitIntConfig *ppu_int_cfg);

void CheckOutputFormat(PpUnitIntConfig *ppu_cfg, enum DecPictureFormat *output_format, u32 index);
#define PI 3.141592653589793
#define TABLE_LENGTH 5
#define TABLE_SIZE  32
#define LAN_WINDOW_HOR 4
#define LAN_WINDOW_VER 3
#define LAN_SIZE_HOR (2*LAN_WINDOW_HOR+1)
#define LAN_SIZE_VER (2*LAN_WINDOW_VER+1)
#define LANCZOS_MAX_DOWN_RATIO 32
#define MAX_COEFF_SIZE_HOR_REAL  (2*LAN_WINDOW_HOR*LANCZOS_MAX_DOWN_RATIO+1)
#define MAX_COEFF_SIZE_VER_REAL  (2*LAN_WINDOW_VER*LANCZOS_MAX_DOWN_RATIO+1)
#define MAX_COEFF_SIZE_HOR  (((MAX_COEFF_SIZE_HOR_REAL+3)/4)*4)
#define MAX_COEFF_SIZE_VER  (((MAX_COEFF_SIZE_VER_REAL+3)/4)*4)
#define LANCZOS_TABLE_SIZE  (TABLE_SIZE / 2 + 1)*(MAX_COEFF_SIZE_HOR + MAX_COEFF_SIZE_VER)
#define LANCZOS_EDGE_SIZE    (2*(((LAN_WINDOW_HOR+3)/4)*4+((LAN_WINDOW_VER+3)/4)*4))
#define LANCZOS_BUFFER_SIZE  (LANCZOS_TABLE_SIZE+LANCZOS_EDGE_SIZE)
#define FIXED_BITS_UNI 16
#define MAKE_FIXED_UNI(x) ((x)<<FIXED_BITS_UNI)
#define FIXED_RADIX MAKE_FIXED_UNI(1)
#define FLOOR(x) ((x)/(FIXED_RADIX))
#define DEC400_YUV_TABLE_SIZE (131072)
#define DEC400_PP_TABLE_SIZE (2*DEC400_YUV_TABLE_SIZE)
#define DEC400_PPn_TABLE_OFFSET(n) (((n)*(DEC400_PP_TABLE_SIZE)))
#define DEC400_PPn_Y_TABLE_OFFSET(n) (DEC400_PPn_TABLE_OFFSET(n))
#define DEC400_PPn_UV_TABLE_OFFSET(n) (DEC400_PPn_TABLE_OFFSET(n) + DEC400_YUV_TABLE_SIZE)
#endif /* __PPU__ */
