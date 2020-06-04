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

#include "deccfg.h"
#include "ppu.h"
#include "regdrv.h"
#include "commonconfig.h"
#include "sw_util.h"
#include "sw_debug.h"
#include <math.h>
#define PP_MAX_STRIDE 65536
#define DOWN_SCALE_SIZE(w, ds) (((w)/(ds)) & ~0x1)
#define TOFIX(d, q) ((u64)( (d) * ((u64)1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))
struct PPUnitRegs ppu_regs[DEC_MAX_PPU_COUNT] = {
  {
    /* pp unit 0 */
    .PP_OUT_E_U = HWIF_PP_OUT_E_U,
    .PP_OUT_TILE_E_U = HWIF_PP_OUT_TILE_E_U,
    .PP_OUT_MODE = HWIF_PP_OUT_MODE,
    .PP_CR_FIRST = HWIF_PP_CR_FIRST,
    .PP_OUT_RGB_FMT_U = HWIF_PP_OUT_RGB_FMT_U,
    .PP_RGB_PLANAR_U = HWIF_PP_RGB_PLANAR_U,
    .PP_OUT_SWAP_U = HWIF_PP_OUT_SWAP_U,
    .HOR_SCALE_MODE_U = HWIF_HOR_SCALE_MODE_U,
    .VER_SCALE_MODE_U = HWIF_VER_SCALE_MODE_U,
    .OUT_FORMAT_U = HWIF_PP_OUT_FORMAT_U,
    .SCALE_HRATIO_U = HWIF_SCALE_HRATIO_U,
    .SCALE_WRATIO_U = HWIF_SCALE_WRATIO_U,
    .WSCALE_INVRA_U = HWIF_WSCALE_INVRA_U,
    .WSCALE_INVRA_EXT_U = HWIF_PP0_WSCALE_INVRA_EXT_U,
    .HSCALE_INVRA_U = HWIF_HSCALE_INVRA_U,
    .HCALE_INVRA_EXT_U = HWIF_PP0_HCALE_INVRA_EXT_U,
    .PP_OUT_LU_BASE_U_MSB = HWIF_PP_OUT_LU_BASE_U_MSB,
    .PP_OUT_LU_BASE_U_LSB = HWIF_PP_OUT_LU_BASE_U_LSB,
    .PP_OUT_CH_BASE_U_MSB = HWIF_PP_OUT_CH_BASE_U_MSB,
    .PP_OUT_CH_BASE_U_LSB = HWIF_PP_OUT_CH_BASE_U_LSB,
    .PP_OUT_Y_STRIDE = HWIF_PP_OUT_Y_STRIDE,
    .PP_OUT_C_STRIDE = HWIF_PP_OUT_C_STRIDE,
    .FLIP_MODE_U = HWIF_FLIP_MODE_U,
    .CROP_STARTX_U = HWIF_CROP_STARTX_U,
    .ROTATION_MODE_U = HWIF_ROTATION_MODE_U,
    .CROP_STARTY_U = HWIF_CROP_STARTY_U,
    .PP_IN_WIDTH_U = HWIF_PP_IN_WIDTH_U,
    .PP_IN_HEIGHT_U = HWIF_PP_IN_HEIGHT_U,
    .PP_OUT_WIDTH_U = HWIF_PP_OUT_WIDTH_U,
    .PP_OUT_HEIGHT_U = HWIF_PP_OUT_HEIGHT_U,
    .PP_OUT_P010_FMT_U = HWIF_PP_OUT_P010_FMT_U,
    .PP_OUT_LU_BOT_BASE_U_MSB = HWIF_PP_OUT_LU_BOT_BASE_U_MSB,
    .PP_OUT_LU_BOT_BASE_U_LSB = HWIF_PP_OUT_LU_BOT_BASE_U_LSB,
    .PP_OUT_CH_BOT_BASE_U_MSB = HWIF_PP_OUT_CH_BOT_BASE_U_MSB,
    .PP_OUT_CH_BOT_BASE_U_LSB = HWIF_PP_OUT_CH_BOT_BASE_U_LSB,
    .PP_CROP2_STARTX_U = HWIF_PP_CROP2_STARTX_U,
    .PP_CROP2_STARTY_U = HWIF_PP_CROP2_STARTY_U,
    .PP_CROP2_OUT_WIDTH_U = HWIF_PP_CROP2_OUT_WIDTH_U,
    .PP_CROP2_OUT_HEIGHT_U = HWIF_PP_CROP2_OUT_HEIGHT_U,
    .PP_OUT_R_BASE_U_MSB = HWIF_PP_OUT_R_BASE_U_MSB,
    .PP_OUT_R_BASE_U_LSB = HWIF_PP_OUT_R_BASE_U_LSB,
    .PP_OUT_G_BASE_U_MSB = HWIF_PP_OUT_G_BASE_U_MSB,
    .PP_OUT_G_BASE_U_LSB = HWIF_PP_OUT_G_BASE_U_LSB,
    .PP_OUT_B_BASE_U_MSB = HWIF_PP_OUT_B_BASE_U_MSB,
    .PP_OUT_B_BASE_U_LSB = HWIF_PP_OUT_B_BASE_U_LSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_MSB = HWIF_PP0_LANCZOS_TBL_BASE_U_MSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_LSB = HWIF_PP0_LANCZOS_TBL_BASE_U_LSB,
  },
  {
    /* pp unit 1 */
    .PP_OUT_E_U = HWIF_PP1_OUT_E_U,
    .PP_OUT_TILE_E_U = HWIF_PP1_OUT_TILE_E_U,
    .PP_OUT_MODE = HWIF_PP1_OUT_MODE,
    .PP_CR_FIRST = HWIF_PP1_CR_FIRST,
    .PP_OUT_RGB_FMT_U = HWIF_PP1_OUT_RGB_FMT_U,
    .PP_RGB_PLANAR_U = HWIF_PP1_RGB_PLANAR_U,
    .PP_OUT_SWAP_U = HWIF_PP1_OUT_SWAP_U,
    .HOR_SCALE_MODE_U = HWIF_PP1_HOR_SCALE_MODE_U,
    .VER_SCALE_MODE_U = HWIF_PP1_VER_SCALE_MODE_U,
    .OUT_FORMAT_U = HWIF_PP1_OUT_FORMAT_U,
    .SCALE_HRATIO_U = HWIF_PP1_SCALE_HRATIO_U,
    .SCALE_WRATIO_U = HWIF_PP1_SCALE_WRATIO_U,
    .WSCALE_INVRA_U = HWIF_PP1_WSCALE_INVRA_U,
    .WSCALE_INVRA_EXT_U = HWIF_PP1_WSCALE_INVRA_EXT_U,
    .HSCALE_INVRA_U = HWIF_PP1_HSCALE_INVRA_U,
    .HCALE_INVRA_EXT_U = HWIF_PP1_HCALE_INVRA_EXT_U,
    .PP_OUT_LU_BASE_U_MSB = HWIF_PP1_OUT_LU_BASE_U_MSB,
    .PP_OUT_LU_BASE_U_LSB = HWIF_PP1_OUT_LU_BASE_U_LSB,
    .PP_OUT_CH_BASE_U_MSB = HWIF_PP1_OUT_CH_BASE_U_MSB,
    .PP_OUT_CH_BASE_U_LSB = HWIF_PP1_OUT_CH_BASE_U_LSB,
    .PP_OUT_Y_STRIDE = HWIF_PP1_OUT_Y_STRIDE,
    .PP_OUT_C_STRIDE = HWIF_PP1_OUT_C_STRIDE,
    .FLIP_MODE_U = HWIF_PP1_FLIP_MODE_U,
    .CROP_STARTX_U = HWIF_PP1_CROP_STARTX_U,
    .ROTATION_MODE_U = HWIF_PP1_ROTATION_MODE_U,
    .CROP_STARTY_U = HWIF_PP1_CROP_STARTY_U,
    .PP_IN_WIDTH_U = HWIF_PP1_IN_WIDTH_U,
    .PP_IN_HEIGHT_U = HWIF_PP1_IN_HEIGHT_U,
    .PP_OUT_WIDTH_U = HWIF_PP1_OUT_WIDTH_U,
    .PP_OUT_HEIGHT_U = HWIF_PP1_OUT_HEIGHT_U,
    .PP_OUT_P010_FMT_U = HWIF_PP1_OUT_P010_FMT_U,
    .PP_OUT_LU_BOT_BASE_U_MSB = HWIF_PP1_OUT_LU_BOT_BASE_U_MSB,
    .PP_OUT_LU_BOT_BASE_U_LSB = HWIF_PP1_OUT_LU_BOT_BASE_U_LSB,
    .PP_OUT_CH_BOT_BASE_U_MSB = HWIF_PP1_OUT_CH_BOT_BASE_U_MSB,
    .PP_OUT_CH_BOT_BASE_U_LSB = HWIF_PP1_OUT_CH_BOT_BASE_U_LSB,
    .PP_CROP2_STARTX_U = HWIF_PP1_CROP2_STARTX_U,
    .PP_CROP2_STARTY_U = HWIF_PP1_CROP2_STARTY_U,
    .PP_CROP2_OUT_WIDTH_U = HWIF_PP1_CROP2_OUT_WIDTH_U,
    .PP_CROP2_OUT_HEIGHT_U = HWIF_PP1_CROP2_OUT_HEIGHT_U,
    .PP_OUT_R_BASE_U_MSB = HWIF_PP1_OUT_R_BASE_U_MSB,
    .PP_OUT_R_BASE_U_LSB = HWIF_PP1_OUT_R_BASE_U_LSB,
    .PP_OUT_G_BASE_U_MSB = HWIF_PP1_OUT_G_BASE_U_MSB,
    .PP_OUT_G_BASE_U_LSB = HWIF_PP1_OUT_G_BASE_U_LSB,
    .PP_OUT_B_BASE_U_MSB = HWIF_PP1_OUT_B_BASE_U_MSB,
    .PP_OUT_B_BASE_U_LSB = HWIF_PP1_OUT_B_BASE_U_LSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_MSB = HWIF_PP1_LANCZOS_TBL_BASE_U_MSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_LSB = HWIF_PP1_LANCZOS_TBL_BASE_U_LSB,
  },
  {
    /* pp unit 2 */
    .PP_OUT_E_U = HWIF_PP2_OUT_E_U,
    .PP_OUT_TILE_E_U = HWIF_PP2_OUT_TILE_E_U,
    .PP_OUT_MODE = HWIF_PP2_OUT_MODE,
    .PP_CR_FIRST = HWIF_PP2_CR_FIRST,
    .PP_OUT_RGB_FMT_U = HWIF_PP2_OUT_RGB_FMT_U,
    .PP_RGB_PLANAR_U = HWIF_PP2_RGB_PLANAR_U,
    .PP_OUT_SWAP_U = HWIF_PP2_OUT_SWAP_U,
    .HOR_SCALE_MODE_U = HWIF_PP2_HOR_SCALE_MODE_U,
    .VER_SCALE_MODE_U = HWIF_PP2_VER_SCALE_MODE_U,
    .OUT_FORMAT_U = HWIF_PP2_OUT_FORMAT_U,
    .SCALE_HRATIO_U = HWIF_PP2_SCALE_HRATIO_U,
    .SCALE_WRATIO_U = HWIF_PP2_SCALE_WRATIO_U,
    .WSCALE_INVRA_U = HWIF_PP2_WSCALE_INVRA_U,
    .WSCALE_INVRA_EXT_U = HWIF_PP2_WSCALE_INVRA_EXT_U,
    .HSCALE_INVRA_U = HWIF_PP2_HSCALE_INVRA_U,
    .HCALE_INVRA_EXT_U = HWIF_PP2_HCALE_INVRA_EXT_U,
    .PP_OUT_LU_BASE_U_MSB = HWIF_PP2_OUT_LU_BASE_U_MSB,
    .PP_OUT_LU_BASE_U_LSB = HWIF_PP2_OUT_LU_BASE_U_LSB,
    .PP_OUT_CH_BASE_U_MSB = HWIF_PP2_OUT_CH_BASE_U_MSB,
    .PP_OUT_CH_BASE_U_LSB = HWIF_PP2_OUT_CH_BASE_U_LSB,
    .PP_OUT_Y_STRIDE = HWIF_PP2_OUT_Y_STRIDE,
    .PP_OUT_C_STRIDE = HWIF_PP2_OUT_C_STRIDE,
    .FLIP_MODE_U = HWIF_PP2_FLIP_MODE_U,
    .CROP_STARTX_U = HWIF_PP2_CROP_STARTX_U,
    .ROTATION_MODE_U = HWIF_PP2_ROTATION_MODE_U,
    .CROP_STARTY_U = HWIF_PP2_CROP_STARTY_U,
    .PP_IN_WIDTH_U = HWIF_PP2_IN_WIDTH_U,
    .PP_IN_HEIGHT_U = HWIF_PP2_IN_HEIGHT_U,
    .PP_OUT_WIDTH_U = HWIF_PP2_OUT_WIDTH_U,
    .PP_OUT_HEIGHT_U = HWIF_PP2_OUT_HEIGHT_U,
    .PP_OUT_P010_FMT_U = HWIF_PP2_OUT_P010_FMT_U,
    .PP_OUT_LU_BOT_BASE_U_MSB = HWIF_PP2_OUT_LU_BOT_BASE_U_MSB,
    .PP_OUT_LU_BOT_BASE_U_LSB = HWIF_PP2_OUT_LU_BOT_BASE_U_LSB,
    .PP_OUT_CH_BOT_BASE_U_MSB = HWIF_PP2_OUT_CH_BOT_BASE_U_MSB,
    .PP_OUT_CH_BOT_BASE_U_LSB = HWIF_PP2_OUT_CH_BOT_BASE_U_LSB,
    .PP_CROP2_STARTX_U = HWIF_PP2_CROP2_STARTX_U,
    .PP_CROP2_STARTY_U = HWIF_PP2_CROP2_STARTY_U,
    .PP_CROP2_OUT_WIDTH_U = HWIF_PP2_CROP2_OUT_WIDTH_U,
    .PP_CROP2_OUT_HEIGHT_U = HWIF_PP2_CROP2_OUT_HEIGHT_U,
    .PP_OUT_R_BASE_U_MSB = HWIF_PP2_OUT_R_BASE_U_MSB,
    .PP_OUT_R_BASE_U_LSB = HWIF_PP2_OUT_R_BASE_U_LSB,
    .PP_OUT_G_BASE_U_MSB = HWIF_PP2_OUT_G_BASE_U_MSB,
    .PP_OUT_G_BASE_U_LSB = HWIF_PP2_OUT_G_BASE_U_LSB,
    .PP_OUT_B_BASE_U_MSB = HWIF_PP2_OUT_B_BASE_U_MSB,
    .PP_OUT_B_BASE_U_LSB = HWIF_PP2_OUT_B_BASE_U_LSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_MSB = HWIF_PP2_LANCZOS_TBL_BASE_U_MSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_LSB = HWIF_PP2_LANCZOS_TBL_BASE_U_LSB,
  },
  {
    /* pp unit 3 */
    .PP_OUT_E_U = HWIF_PP3_OUT_E_U,
    .PP_OUT_TILE_E_U = HWIF_PP3_OUT_TILE_E_U,
    .PP_OUT_MODE = HWIF_PP3_OUT_MODE,
    .PP_CR_FIRST = HWIF_PP3_CR_FIRST,
    .PP_OUT_RGB_FMT_U = HWIF_PP3_OUT_RGB_FMT_U,
    .PP_RGB_PLANAR_U = HWIF_PP3_RGB_PLANAR_U,
    .PP_OUT_SWAP_U = HWIF_PP3_OUT_SWAP_U,
    .HOR_SCALE_MODE_U = HWIF_PP3_HOR_SCALE_MODE_U,
    .VER_SCALE_MODE_U = HWIF_PP3_VER_SCALE_MODE_U,
    .OUT_FORMAT_U = HWIF_PP3_OUT_FORMAT_U,
    .SCALE_HRATIO_U = HWIF_PP3_SCALE_HRATIO_U,
    .SCALE_WRATIO_U = HWIF_PP3_SCALE_WRATIO_U,
    .WSCALE_INVRA_U = HWIF_PP3_WSCALE_INVRA_U,
    .WSCALE_INVRA_EXT_U = HWIF_PP3_WSCALE_INVRA_EXT_U,
    .HSCALE_INVRA_U = HWIF_PP3_HSCALE_INVRA_U,
    .HCALE_INVRA_EXT_U = HWIF_PP3_HCALE_INVRA_EXT_U,
    .PP_OUT_LU_BASE_U_MSB = HWIF_PP3_OUT_LU_BASE_U_MSB,
    .PP_OUT_LU_BASE_U_LSB = HWIF_PP3_OUT_LU_BASE_U_LSB,
    .PP_OUT_CH_BASE_U_MSB = HWIF_PP3_OUT_CH_BASE_U_MSB,
    .PP_OUT_CH_BASE_U_LSB = HWIF_PP3_OUT_CH_BASE_U_LSB,
    .PP_OUT_Y_STRIDE = HWIF_PP3_OUT_Y_STRIDE,
    .PP_OUT_C_STRIDE = HWIF_PP3_OUT_C_STRIDE,
    .FLIP_MODE_U = HWIF_PP3_FLIP_MODE_U,
    .CROP_STARTX_U = HWIF_PP3_CROP_STARTX_U,
    .ROTATION_MODE_U = HWIF_PP3_ROTATION_MODE_U,
    .CROP_STARTY_U = HWIF_PP3_CROP_STARTY_U,
    .PP_IN_WIDTH_U = HWIF_PP3_IN_WIDTH_U,
    .PP_IN_HEIGHT_U = HWIF_PP3_IN_HEIGHT_U,
    .PP_OUT_WIDTH_U = HWIF_PP3_OUT_WIDTH_U,
    .PP_OUT_HEIGHT_U = HWIF_PP3_OUT_HEIGHT_U,
    .PP_OUT_P010_FMT_U = HWIF_PP3_OUT_P010_FMT_U,
    .PP_OUT_LU_BOT_BASE_U_MSB = HWIF_PP3_OUT_LU_BOT_BASE_U_MSB,
    .PP_OUT_LU_BOT_BASE_U_LSB = HWIF_PP3_OUT_LU_BOT_BASE_U_LSB,
    .PP_OUT_CH_BOT_BASE_U_MSB = HWIF_PP3_OUT_CH_BOT_BASE_U_MSB,
    .PP_OUT_CH_BOT_BASE_U_LSB = HWIF_PP3_OUT_CH_BOT_BASE_U_LSB,
    .PP_CROP2_STARTX_U = HWIF_PP3_CROP2_STARTX_U,
    .PP_CROP2_STARTY_U = HWIF_PP3_CROP2_STARTY_U,
    .PP_CROP2_OUT_WIDTH_U = HWIF_PP3_CROP2_OUT_WIDTH_U,
    .PP_CROP2_OUT_HEIGHT_U = HWIF_PP3_CROP2_OUT_HEIGHT_U,
    .PP_OUT_R_BASE_U_MSB = HWIF_PP3_OUT_R_BASE_U_MSB,
    .PP_OUT_R_BASE_U_LSB = HWIF_PP3_OUT_R_BASE_U_LSB,
    .PP_OUT_G_BASE_U_MSB = HWIF_PP3_OUT_G_BASE_U_MSB,
    .PP_OUT_G_BASE_U_LSB = HWIF_PP3_OUT_G_BASE_U_LSB,
    .PP_OUT_B_BASE_U_MSB = HWIF_PP3_OUT_B_BASE_U_MSB,
    .PP_OUT_B_BASE_U_LSB = HWIF_PP3_OUT_B_BASE_U_LSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_MSB = HWIF_PP3_LANCZOS_TBL_BASE_U_MSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_LSB = HWIF_PP3_LANCZOS_TBL_BASE_U_LSB,
  },
  {
    /* pp unit 4 */
    .PP_OUT_E_U = HWIF_PP4_OUT_E_U,
    .PP_OUT_TILE_E_U = HWIF_PP4_OUT_TILE_E_U,
    .PP_OUT_MODE = HWIF_PP4_OUT_MODE,
    .PP_CR_FIRST = HWIF_PP4_CR_FIRST,
    .PP_OUT_RGB_FMT_U = HWIF_PP4_OUT_RGB_FMT_U,
    .PP_RGB_PLANAR_U = HWIF_PP4_RGB_PLANAR_U,
    .PP_OUT_SWAP_U = HWIF_PP4_OUT_SWAP_U,
    .HOR_SCALE_MODE_U = HWIF_PP4_HOR_SCALE_MODE_U,
    .VER_SCALE_MODE_U = HWIF_PP4_VER_SCALE_MODE_U,
    .OUT_FORMAT_U = HWIF_PP4_OUT_FORMAT_U,
    .SCALE_HRATIO_U = HWIF_PP4_SCALE_HRATIO_U,
    .SCALE_WRATIO_U = HWIF_PP4_SCALE_WRATIO_U,
    .WSCALE_INVRA_U = HWIF_PP4_WSCALE_INVRA_U,
    .WSCALE_INVRA_EXT_U = HWIF_PP4_WSCALE_INVRA_EXT_U,
    .HSCALE_INVRA_U = HWIF_PP4_HSCALE_INVRA_U,
    .HCALE_INVRA_EXT_U = HWIF_PP4_HCALE_INVRA_EXT_U,
    .PP_OUT_LU_BASE_U_MSB = HWIF_PP4_OUT_LU_BASE_U_MSB,
    .PP_OUT_LU_BASE_U_LSB = HWIF_PP4_OUT_LU_BASE_U_LSB,
    .PP_OUT_CH_BASE_U_MSB = HWIF_PP4_OUT_CH_BASE_U_MSB,
    .PP_OUT_CH_BASE_U_LSB = HWIF_PP4_OUT_CH_BASE_U_LSB,
    .PP_OUT_Y_STRIDE = HWIF_PP4_OUT_Y_STRIDE,
    .PP_OUT_C_STRIDE = HWIF_PP4_OUT_C_STRIDE,
    .FLIP_MODE_U = HWIF_PP4_FLIP_MODE_U,
    .CROP_STARTX_U = HWIF_PP4_CROP_STARTX_U,
    .ROTATION_MODE_U = HWIF_PP4_ROTATION_MODE_U,
    .CROP_STARTY_U = HWIF_PP4_CROP_STARTY_U,
    .PP_IN_WIDTH_U = HWIF_PP4_IN_WIDTH_U,
    .PP_IN_HEIGHT_U = HWIF_PP4_IN_HEIGHT_U,
    .PP_OUT_WIDTH_U = HWIF_PP4_OUT_WIDTH_U,
    .PP_OUT_HEIGHT_U = HWIF_PP4_OUT_HEIGHT_U,
    .PP_OUT_P010_FMT_U = HWIF_PP4_OUT_P010_FMT_U,
    .PP_OUT_LU_BOT_BASE_U_MSB = HWIF_PP4_OUT_LU_BOT_BASE_U_MSB,
    .PP_OUT_LU_BOT_BASE_U_LSB = HWIF_PP4_OUT_LU_BOT_BASE_U_LSB,
    .PP_OUT_CH_BOT_BASE_U_MSB = HWIF_PP4_OUT_CH_BOT_BASE_U_MSB,
    .PP_OUT_CH_BOT_BASE_U_LSB = HWIF_PP4_OUT_CH_BOT_BASE_U_LSB,
    .PP_CROP2_STARTX_U = HWIF_PP4_CROP2_STARTX_U,
    .PP_CROP2_STARTY_U = HWIF_PP4_CROP2_STARTY_U,
    .PP_CROP2_OUT_WIDTH_U = HWIF_PP4_CROP2_OUT_WIDTH_U,
    .PP_CROP2_OUT_HEIGHT_U = HWIF_PP4_CROP2_OUT_HEIGHT_U,
    .PP_OUT_R_BASE_U_MSB = HWIF_PP4_OUT_R_BASE_U_MSB,
    .PP_OUT_R_BASE_U_LSB = HWIF_PP4_OUT_R_BASE_U_LSB,
    .PP_OUT_G_BASE_U_MSB = HWIF_PP4_OUT_G_BASE_U_MSB,
    .PP_OUT_G_BASE_U_LSB = HWIF_PP4_OUT_G_BASE_U_LSB,
    .PP_OUT_B_BASE_U_MSB = HWIF_PP4_OUT_B_BASE_U_MSB,
    .PP_OUT_B_BASE_U_LSB = HWIF_PP4_OUT_B_BASE_U_LSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_MSB = HWIF_PP4_LANCZOS_TBL_BASE_U_MSB,
    .PP_OUT_LANCZOS_TBL_BASE_U_LSB = HWIF_PP4_LANCZOS_TBL_BASE_U_LSB,
  }
};

static const u32 coeff[6][5] = {{16384,22970,11700,5638,29032},
                                {19077,26149,13320,6419,33050},
                                {16384,25802,7670,3069,30402},
                                {19077,29372,8731,3494,34610},
                                {16384,24160,9361,2696,30825},
                                {19077,27504,10646,3069,35091}};


static void CalOutTableIndex(u32 table_in_index, u32 coeff_in_index, u32 filter_size, 
                             u32* table_out_index, u32* coeff_out_index) {
  if (table_in_index <= 16) {
    *table_out_index = table_in_index;
    *coeff_out_index = coeff_in_index;
  } else {
    *table_out_index = 32 - table_in_index;
    *coeff_out_index = filter_size - 1 - coeff_in_index;
  }
}

static void InitPpUnitLanczosData(struct DecHwFeatures *hw_feature,
                                  PpUnitIntConfig *ppu_cfg) {
  u32 i = 0, j, m;
  i32 k;
  i32 xDstInSrc, yDstInSrc;
  i32 scale_ratio_x_inv, scale_ratio_y_inv;
  i32 start_pos, end_pos, table_index;
  i32 pos, x_pos, y_pos;
  u32 start_count, end_count;
  u32 out_index, coeff_index;
  i32 value;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (i >= hw_feature->max_ppu_count)
      ppu_cfg->enabled = 0;
    if (!ppu_cfg->enabled) continue;
    if (hw_feature->pp_lanczos[i] && ppu_cfg->lanczos_table.virtual_address) {
      u32 x_filter_size, y_filter_size;
      u32 x_filter_stride, y_filter_stride;
      if (ppu_cfg->crop.width > ppu_cfg->scale.width)
        x_filter_size =((LAN_SIZE_HOR - 1) * ppu_cfg->crop.width + ppu_cfg->scale.width - 1) / ppu_cfg->scale.width;
      else
        x_filter_size = (LAN_SIZE_HOR - 1);
      if (ppu_cfg->crop.height > ppu_cfg->scale.height)
        y_filter_size = ((LAN_SIZE_VER - 1) * ppu_cfg->crop.height + ppu_cfg->scale.height - 1) / ppu_cfg->scale.height;
      else
        y_filter_size = (LAN_SIZE_VER - 1);
      x_filter_size = 1 + x_filter_size / 2 * 2;
      y_filter_size = 1 + y_filter_size / 2 * 2;
      if (x_filter_size > MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR))
        x_filter_size = MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR);
      if (y_filter_size > MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER))
        y_filter_size = MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER);
      x_filter_stride = ((x_filter_size+3)/4)*4;
      y_filter_stride = ((y_filter_size+3)/4)*4;
      double ratio = 0;
      double distance = LAN_WINDOW_HOR;
      i32 horizontal_weight[MAX_COEFF_SIZE_HOR];
      i32 verizontal_weight[MAX_COEFF_SIZE_VER];
      i32 *table_data = (i32*)ppu_cfg->lanczos_table.virtual_address;
      if (ppu_cfg->scale.width != ppu_cfg->crop.width) {
        if (ppu_cfg->scale.width > ppu_cfg->crop.width)
          ratio = 1.0;
        else
          ratio = (double)ppu_cfg->scale.width / ppu_cfg->crop.width;
        for (k = 0; k <= TABLE_SIZE/2; k++) {
          for (m = 0; m < x_filter_size; m++) {
            double value = (double)(m)-x_filter_size/2 - ((double)k - TABLE_SIZE/2)/TABLE_SIZE;
            if (value < 0)
              value *= -1.0;
            else
              value *= 1.0;
            double d = value * ratio;
            if (d==0) {
              horizontal_weight[m]=65535;
            } else if (d > distance || d < -distance) {
              horizontal_weight[m]= 0;
            } else {
              horizontal_weight[m]=
              (i32)((distance*sin(PI*d)*sin(PI*d/distance)/(PI*PI*d*d))*65536);
            }
          }
          i32 total = 0;
          for (m = 0; m < x_filter_size; m++) {
            total += horizontal_weight[m];
          }
          for (m = 0; m < x_filter_size; m++) {
            *table_data++ = (i32) ((double)horizontal_weight[m] / total * 65536);
          }
          for (m = 0; m < x_filter_stride - x_filter_size; m++) {
            *table_data++ = 0;
          }
        }
      }
      distance = LAN_WINDOW_VER;
      if (ppu_cfg->scale.height != ppu_cfg->crop.height) {
        if (ppu_cfg->scale.height > ppu_cfg->crop.height)
          ratio = 1.0;
        else
          ratio = (double)ppu_cfg->scale.height / ppu_cfg->crop.height;
        for (k = 0; k <= TABLE_SIZE/2; k++) {
          for (m = 0; m < y_filter_size; m++) {
            double value = (double)(m)-y_filter_size/2 - ((double)k - TABLE_SIZE/2)/TABLE_SIZE;
            if (value < 0)
              value *= -1.0;
            else
              value *= 1.0;
            double d = value * ratio;
            if (d==0) {
              verizontal_weight[m]=65535;
            } else if (d > distance || d < -distance) {
              verizontal_weight[m]= 0;
            } else {
              verizontal_weight[m]=
              (i32)((distance*sin(PI*d)*sin(PI*d/distance)/(PI*PI*d*d))*65536);
            }
          }
          i32 total = 0;
          for (m = 0; m < y_filter_size; m++) {
            total += verizontal_weight[m];
          }
          for (m = 0; m < y_filter_size; m++) {
            *table_data++ = (i32) ((double)verizontal_weight[m] / total * 65536);
          }
          for (m = 0; m < y_filter_stride - y_filter_size; m++) {
            *table_data++ = 0;
          }
        }
      }
      scale_ratio_x_inv = FDIVI(TOFIX(ppu_cfg->crop.width, 16) + ppu_cfg->scale.width / 2, ppu_cfg->scale.width);
      scale_ratio_y_inv = FDIVI(TOFIX(ppu_cfg->crop.height, 16) + ppu_cfg->scale.height / 2, ppu_cfg->scale.height); 
      xDstInSrc =scale_ratio_x_inv - 65536;
      pos = xDstInSrc / 2 - scale_ratio_x_inv;
      x_pos = 0;
      start_count = 0;
      end_count = 0;
      i32 start_value[4]={0,0,0,0};
      i32 end_value[4]={0,0,0,0};
      if (ppu_cfg->crop.width != ppu_cfg->scale.width) {
        for (j = 0; j<ppu_cfg->scale.width; j++) {
          pos += scale_ratio_x_inv;
          x_pos = FLOOR(pos+32768);
          start_pos = x_pos - x_filter_size/2;
          end_pos = x_pos + x_filter_size/2;
          table_index = ((pos+32768 - MAKE_FIXED_UNI(x_pos)) >> (FIXED_BITS_UNI-TABLE_LENGTH));
          value = 0;
          if (start_pos < 0) {
            for (k = start_pos; k <= 0; k++) {
              CalOutTableIndex(table_index, k-start_pos, x_filter_size, &out_index, &coeff_index);
              value += *(ppu_cfg->lanczos_table.virtual_address + out_index * x_filter_stride + coeff_index);
            }
            start_value[start_count++] = value;
          }
          value = 0;
          if (end_pos >= ppu_cfg->crop.width) {
            for (k = ppu_cfg->crop.width - 1; k <= end_pos; k++) {
              CalOutTableIndex(table_index, k-start_pos, x_filter_size, &out_index, &coeff_index);
              value += *(ppu_cfg->lanczos_table.virtual_address + out_index * x_filter_stride + coeff_index);
            }
            end_value[end_count++] = value;
          }
        }
        for (j = 0; j < 4; j++) {
          *table_data++ = start_value[j];
           start_value[j] = 0;
        }
        for (j = 0; j < 4; j++) {
          *table_data++ = end_value[j];
          end_value[j] = 0;
        }
      }
      yDstInSrc =scale_ratio_y_inv - 65536;
      pos = yDstInSrc / 2 - scale_ratio_y_inv;
      y_pos = 0;
      start_count = 0;
      end_count = 0;
      if (ppu_cfg->scale.height != ppu_cfg->crop.height) {
        for (j = 0; j<ppu_cfg->scale.height; j++) {
          pos += scale_ratio_y_inv;
          y_pos = FLOOR(pos+32768);
          start_pos = y_pos - y_filter_size/2;
          end_pos = y_pos + y_filter_size/2;
          table_index = ((pos+32768 - MAKE_FIXED_UNI(y_pos)) >> (FIXED_BITS_UNI-TABLE_LENGTH));
          value = 0;
          if (start_pos < 0) {
            for (k = start_pos; k <= 0; k++) {
              CalOutTableIndex(table_index, k-start_pos, y_filter_size, &out_index, &coeff_index);
              value += *(ppu_cfg->lanczos_table.virtual_address + (TABLE_SIZE/2+1)* x_filter_stride + out_index * y_filter_stride + coeff_index);
            }
            start_value[start_count++] = value;
          }
          value = 0;
          if (end_pos >= ppu_cfg->crop.height) {
            for (k = ppu_cfg->crop.height - 1; k <= end_pos; k++) {
              CalOutTableIndex(table_index, k-start_pos, y_filter_size, &out_index, &coeff_index);
              value += *(ppu_cfg->lanczos_table.virtual_address + (TABLE_SIZE/2+1)* x_filter_stride + out_index * y_filter_stride + coeff_index);
            }
            end_value[end_count++] = value;
          }
        }
        for (j = 0; j < 4; j++)
          *table_data++ = start_value[j];
        for (j = 0; j < 4; j++)
          *table_data++ = end_value[j];
      }
    }
  }
}

/* To be moved to a comomn source file for all formats. */
u32 CheckPpUnitConfig(struct DecHwFeatures *hw_feature,
                            u32 in_width,
                            u32 in_height,
                            u32 interlace,
                            PpUnitIntConfig *ppu_cfg) {
  u32 i = 0;
  u32 pp_width, ystride=0, cstride=0;
  PpUnitIntConfig *ppu_cfg_temp = ppu_cfg;
  
  u32 crop_align = (1 << hw_feature->crop_step_rshift) - 1;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (i >= hw_feature->max_ppu_count)
      ppu_cfg->enabled = 0;
    if (!ppu_cfg->enabled) continue;

    if (in_width == 0 && in_height == 0)
      return 0;

    if (!hw_feature->pp_planar_support && ppu_cfg->planar) {
      ERROR_PRINT("PLANAR output is not supuported.\n");
      return 1;
    }

    if (!hw_feature->pp_yuv420_101010_support && ppu_cfg->out_1010) {
      ERROR_PRINT("101010 output is not supuported.\n");
      return 1;
    }
    
    if (!hw_feature->fmt_p010_support &&
        (ppu_cfg->out_p010 || ppu_cfg->out_I010 || ppu_cfg->out_L010)) {
      ERROR_PRINT("P010 output is not supuported.\n");
      return 1;
    }
    if (!hw_feature->second_crop_support && ppu_cfg->crop2.enabled) {
      ERROR_PRINT("second crop  is not supuported.\n");
      return 1;
    }

#if 0
    if (!hw_feature->fmt_rgb_support && (ppu_cfg->rgb || ppu_cfg->rgb_planar)) {
      ERROR_PRINT("RGB is not supported.\n");
      return 1;
    }
#endif
    if (ppu_cfg->planar && ppu_cfg->monochrome) {
      ppu_cfg->planar = 0;
    }
    if (!ppu_cfg->crop.width || !ppu_cfg->crop.height) {
      ppu_cfg->crop.enabled = 0;
    }
    if (!ppu_cfg->crop.enabled) {
      ppu_cfg->crop.x = ppu_cfg->crop.y = 0;
      ppu_cfg->crop.width = in_width;
      ppu_cfg->crop.height = in_height;
      ppu_cfg->crop.set_by_user = 0;
      ppu_cfg->crop.enabled = 1;
    }

    if (!ppu_cfg->scale.width || !ppu_cfg->scale.height) {
      ppu_cfg->scale.enabled = 0;
    }

    /* reset PP0 scaling w/h when enable '-d' and tb.cfg*/
    if (i == 0 && ppu_cfg->scale.ratio_x && ppu_cfg->scale.ratio_y &&
        !ppu_cfg->scale.width && !ppu_cfg->scale.height) {
      ppu_cfg->scale.width = DOWN_SCALE_SIZE(in_width, ppu_cfg->scale.ratio_x);
      ppu_cfg->scale.height = DOWN_SCALE_SIZE(in_height, ppu_cfg->scale.ratio_y);
      /* To resolve last 'if' which sets ppu_cfg->scale.enabled as 0 */
      ppu_cfg->scale.enabled = 1;
    }

    if (!ppu_cfg->scale.enabled) {
      ppu_cfg->scale.width = ppu_cfg->crop.width;
      ppu_cfg->scale.height = ppu_cfg->crop.height;
      ppu_cfg->scale.ratio_x = 0;
      ppu_cfg->scale.ratio_y = 0;
      ppu_cfg->scale.set_by_user = 0;
      ppu_cfg->scale.enabled = 1;
    }

    if (ppu_cfg->tiled_e) {
      pp_width = NEXT_MULTIPLE(ppu_cfg->scale.width, 4);
      ystride = cstride = NEXT_MULTIPLE(4 * pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
    } else if (ppu_cfg->out_1010) {
      pp_width = ((ppu_cfg->scale.width + 2) / 3) * 3;
      ystride = cstride = NEXT_MULTIPLE(4 * pp_width / 3, ALIGN(ppu_cfg->align));
    } else if (ppu_cfg->rgb) {
      if (ppu_cfg->rgb_format == PP_OUT_RGB888 || ppu_cfg->rgb_format == PP_OUT_BGR888 ||
          ppu_cfg->rgb_format == PP_OUT_R16G16B16 || ppu_cfg->rgb_format == PP_OUT_B16G16R16) {
        pp_width = ppu_cfg->scale.width * 3;
        if (hw_feature->rgb_line_stride_support)
          ystride = cstride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
        else
          ystride = cstride = pp_width * ppu_cfg->pixel_width / 8;
      } else if (ppu_cfg->rgb_format == PP_OUT_ARGB888 || ppu_cfg->rgb_format == PP_OUT_ABGR888 ||
                 ppu_cfg->rgb_format == PP_OUT_A2R10G10B10 || ppu_cfg->rgb_format == PP_OUT_A2B10G10R10) {
        pp_width = ppu_cfg->scale.width * 4;
        if (hw_feature->rgb_line_stride_support)
          ystride = cstride = NEXT_MULTIPLE(pp_width * 8, ALIGN(ppu_cfg->align) * 8) / 8;
        else
          ystride = cstride = pp_width;
      }
    } else if (ppu_cfg->rgb_planar) {
      pp_width = ppu_cfg->scale.width;
      if (hw_feature->rgb_line_stride_support)
        ystride = cstride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
      else
        ystride = cstride = pp_width * ppu_cfg->pixel_width / 8;
    } else if (ppu_cfg->crop2.enabled) {
      pp_width = ppu_cfg->crop2.width;
      ystride = cstride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
    } else {
      pp_width = ppu_cfg->scale.width;
      ystride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
      if (ppu_cfg->planar)
        cstride = NEXT_MULTIPLE(pp_width / 2 * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
      else
        cstride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width, ALIGN(ppu_cfg->align) * 8) / 8;
    }
    if (ppu_cfg->ystride == 0) {
      if (ystride >= PP_MAX_STRIDE) {
        ERROR_PRINT("Too large Y stride.\n");
        return 1;
      }

      ppu_cfg->ystride = ystride;
      ppu_cfg->false_ystride = 1;
    } else {
      if (ppu_cfg->false_ystride)
        ppu_cfg->ystride = ystride;
      else {
        if (ppu_cfg->ystride < ystride) {
          return 1;
        }
      }
    }
    if (ppu_cfg->cstride == 0) {
      if (cstride >= PP_MAX_STRIDE) {
        ERROR_PRINT("Too large C stride.\n");
        return 1;
      }
      ppu_cfg->cstride = cstride;
      ppu_cfg->false_cstride = 1;
    } else {
      if (ppu_cfg->false_cstride)
        ppu_cfg->cstride = cstride;
      else {
        if (ppu_cfg->cstride < cstride)
          return 1;
      }
    }
    if ((ppu_cfg->rgb || ppu_cfg->rgb_planar) &&
        ((ppu_cfg->pixel_width == 8 && (ppu_cfg->rgb_format == PP_OUT_R16G16B16 || ppu_cfg->rgb_format == PP_OUT_B16G16R16)) ||
        (ppu_cfg->pixel_width == 16 && (ppu_cfg->rgb_format == PP_OUT_RGB888 || ppu_cfg->rgb_format == PP_OUT_BGR888)))) {
      if (ppu_cfg->pixel_width == 8) {
        if (ppu_cfg->rgb_format == PP_OUT_R16G16B16) {
          ppu_cfg->rgb_format = PP_OUT_RGB888;
        } else if (ppu_cfg->rgb_format == PP_OUT_B16G16R16){
          ppu_cfg->rgb_format = PP_OUT_BGR888;
        }
      } else if (ppu_cfg->pixel_width == 16) {
        if (ppu_cfg->rgb_format == PP_OUT_RGB888) {
          ppu_cfg->rgb_format = PP_OUT_R16G16B16;
        } else if (ppu_cfg->rgb_format == PP_OUT_BGR888){
          ppu_cfg->rgb_format = PP_OUT_B16G16R16;
        }
      }
      ERROR_PRINT("Illegal RGB parameters, software fixed it automatically .\n");
    }
    if (ppu_cfg->rgb_planar && (ppu_cfg->rgb_format == PP_OUT_ARGB888 || ppu_cfg->rgb_format == PP_OUT_ABGR888 ||
                                ppu_cfg->rgb_format == PP_OUT_A2R10G10B10 || ppu_cfg->rgb_format == PP_OUT_A2B10G10R10)) {
      ERROR_PRINT("Illegal RGB parameters.\n");
      return 1;
    }
    if ((ppu_cfg->crop.x & crop_align) ||
        (ppu_cfg->crop.y & crop_align) ||
        (ppu_cfg->crop.width & crop_align) ||
        (ppu_cfg->crop.height & crop_align) ||
        (ppu_cfg->crop.width  < PP_CROP_MIN_WIDTH && ppu_cfg->crop.width != in_width) ||
        (ppu_cfg->crop.height < PP_CROP_MIN_HEIGHT && ppu_cfg->crop.height != in_height) ||
        (ppu_cfg->crop.x + ppu_cfg->crop.width > in_width) ||
        (ppu_cfg->crop.y + ppu_cfg->crop.height > in_height)) {
      ERROR_PRINT("Illegal cropping parameters.\n");
      return 1;
    }
    if ((ppu_cfg->crop2.x & crop_align) ||
        (ppu_cfg->crop2.y & crop_align) ||
        (ppu_cfg->crop2.width & crop_align) ||
        (ppu_cfg->crop2.height & crop_align) ||
        (ppu_cfg->crop2.x + ppu_cfg->crop2.width > ppu_cfg->scale.width) ||
        (ppu_cfg->crop2.y + ppu_cfg->crop2.height > ppu_cfg->scale.height) ||
        (ppu_cfg->crop2.enabled && ((ppu_cfg->crop2.width < 48) || (ppu_cfg->scale.height < 48)))) {
      ERROR_PRINT("Illegal cropping parameters.\n");
      return 1;
    }
    /* If cropping output greater thatn SCALE_IN_MAX size, no scaling. */
    if (ppu_cfg->crop.width  <= hw_feature->max_pp_out_pic_width[i] &&
        ppu_cfg->crop.height <= hw_feature->max_pp_out_pic_height[i]) {
      if (!ppu_cfg->scale.width || !ppu_cfg->scale.height ||
          (!hw_feature->uscale_support[i] &&
           (ppu_cfg->scale.width > ppu_cfg->crop.width ||
            ppu_cfg->scale.height > ppu_cfg->crop.height)) ||
          (((ppu_cfg->scale.width > ppu_cfg->crop.width) && ((ppu_cfg->crop.x & 3) || (ppu_cfg->crop.width & 3))) ||
            ((ppu_cfg->scale.height > ppu_cfg->crop.height) && ((ppu_cfg->crop.y & 3) || (ppu_cfg->crop.height & 3)))) ||
          (!hw_feature->dscale_support[i] &&
           (ppu_cfg->scale.width < ppu_cfg->crop.width ||
            ppu_cfg->scale.height < ppu_cfg->crop.height)) ||
          (ppu_cfg->scale.width  > MIN(hw_feature->max_pp_out_pic_width[i],  3 * ppu_cfg->crop.width)) ||
          (ppu_cfg->scale.height > MIN(hw_feature->max_pp_out_pic_height[i], 3 * ppu_cfg->crop.height)) ||
          (ppu_cfg->scale.width  & 1) ||
          (ppu_cfg->scale.height & (interlace ? 3 : 1)) ||
          (ppu_cfg->scale.width  > ppu_cfg->crop.width &&
           ppu_cfg->scale.height < ppu_cfg->crop.height) ||
          (ppu_cfg->scale.width  < ppu_cfg->crop.width &&
           ppu_cfg->scale.height > ppu_cfg->crop.height)) {
        ERROR_PRINT("Illegal scaling output parameters.\n");
        return 1;
      }
    } else if (!hw_feature->pp_dscaling_exceeding_max_size &&
               (ppu_cfg->scale.width  != ppu_cfg->crop.width ||
                ppu_cfg->scale.height != ppu_cfg->crop.height)) {
      ERROR_PRINT("Illegal scaling output parameters:\n"
                  "HW DOESN'T support scaling when scaling input size greater than the maximum output size.\n");
      return 1;
    }
    if (ppu_cfg->tiled_e && ppu_cfg->planar) {
      ERROR_PRINT("PLANAR is not supported with TILED4x4 output.\n");
      return 1;
    }
  }
  InitPpUnitLanczosData(hw_feature, ppu_cfg_temp);
  return 0; //return DEC_OK;
}

/*------------------------------------------------------------------------------
    Function name :
    Description   :

    Return type   : None
    Argument      : DecAsicBuffers_t * p_asic_buff
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

    Function: PPSetRegs

        Functional description:
            Set registers based on pp configurations.

        Input:
            pp_regs     register array base pointer (swreg0)
            p_hw_feature  hw feature struct pointer
            ppu_out_bus_addr  base address of pp buffer
            mono_chrome   whether input picture is mono-chrome
            bottom_field_flag set 1 if current is the bottom field of a picture,
                              otherwise (top field or frame) 0
            align       alignment setting

        Output:

        Returns:

------------------------------------------------------------------------------*/
void PPSetRegs(u32 *pp_regs,
               struct DecHwFeatures *p_hw_feature,
               PpUnitIntConfig *ppu_cfg,
               addr_t ppu_out_bus_addr,
               u32 mono_chrome,
               u32 bottom_field_flag) {
  u32 i;
  u32 ppb_mode = DEC_DPB_FRAME;
  struct DecHwFeatures hw_feature = *p_hw_feature;

  if (p_hw_feature->pp_support && p_hw_feature->pp_version) {
    u32 ppw, ppw_c;
    u32 pp_field_offset = 0;
    u32 pp_field_offset_ch = 0;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_E_U, ppu_cfg->enabled);

      if (!ppu_cfg->enabled) continue;
      SetDecRegister(pp_regs, ppu_regs[i].PP_CR_FIRST, ppu_cfg->cr_first);
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_TILE_E_U, ppu_cfg->tiled_e);
      SetDecRegister(pp_regs, ppu_regs[i].OUT_FORMAT_U, ppu_cfg->out_format);
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_RGB_FMT_U, ppu_cfg->rgb_format);
      SetDecRegister(pp_regs, ppu_regs[i].PP_RGB_PLANAR_U, ppu_cfg->rgb_planar);
      if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFA2_U, coeff[ppu_cfg->rgb_stan][0]);
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFA1_U, coeff[ppu_cfg->rgb_stan][0]);
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFB_U, coeff[ppu_cfg->rgb_stan][1]);
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFC_U, coeff[ppu_cfg->rgb_stan][2]);
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFD_U, coeff[ppu_cfg->rgb_stan][3]);
        SetDecRegister(pp_regs, HWIF_COLOR_COEFFE_U, coeff[ppu_cfg->rgb_stan][4]);
        SetDecRegister(pp_regs, HWIF_YCBCR_RANGE_U, ppu_cfg->video_range);
        SetDecRegister(pp_regs, HWIF_RGB_RANGE_MAX_U, ppu_cfg->range_max);
        SetDecRegister(pp_regs, HWIF_RGB_RANGE_MIN_U, ppu_cfg->range_min);
      }
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_P010_FMT_U, (ppu_cfg->out_L010 ? 2 :
                                                              (ppu_cfg->out_I010 ? 1 : 0)));

      {
      /* flexible scale ratio */
      u32 in_width = ppu_cfg->crop.width;
      u32 in_height = ppu_cfg->crop.height;
      u32 out_width = ppu_cfg->scale.width;
      u32 out_height = ppu_cfg->scale.height;

      ppw = ppu_cfg->ystride;
      ppw_c = ppu_cfg->cstride;
      SetDecRegister(pp_regs, ppu_regs[i].CROP_STARTX_U,
                     ppu_cfg->crop.x >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].CROP_STARTY_U,
                     ppu_cfg->crop.y >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_IN_WIDTH_U,
                     ppu_cfg->crop.width >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_IN_HEIGHT_U,
                     ppu_cfg->crop.height >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_CROP2_STARTX_U,
                     ppu_cfg->crop2.x >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_CROP2_STARTY_U,
                     ppu_cfg->crop2.y >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_CROP2_OUT_WIDTH_U,
                     ppu_cfg->crop2.width >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_CROP2_OUT_HEIGHT_U,
                     ppu_cfg->crop2.height >> p_hw_feature->crop_step_rshift);
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_WIDTH_U,
                     ppu_cfg->scale.width);
      SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_HEIGHT_U,
                     ppu_cfg->scale.height);

      if(in_width < out_width) {
        /* upscale */
        if (hw_feature.pp_lanczos[i]) {
          u32 W, inv_w;

          SetDecRegister(pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

          W = FDIVI(TOFIX(out_width, 32), (TOFIX(in_width, 16) + out_width / 2));
          inv_w = FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
        } else {
          u32 W, inv_w;

          SetDecRegister(pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

          W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
          inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_U, inv_w);
        }
      } else if(in_width > out_width) {
        /* downscale */
        if (hw_feature.pp_lanczos[i]) {
          u32 W, inv_w;

          SetDecRegister(pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

          W = FDIVI(TOFIX(out_width, 32), (TOFIX(in_width, 16) + out_width / 2));
          inv_w = FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
        } else {
          u32 hnorm;

          SetDecRegister(pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

          hnorm = FDIVI(TOFIX(out_width, 16), in_width);
          if (TOFIX(out_width, 16) % in_width) hnorm++;

          SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_U, hnorm);
        }
      } else {
        SetDecRegister(pp_regs, ppu_regs[i].WSCALE_INVRA_U, 0);
        SetDecRegister(pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 0);
      }

      if(in_height < out_height) {
        /* upscale */
        if (hw_feature.pp_lanczos[i]) {
          u32 H, inv_h;

          SetDecRegister(pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

          H = FDIVI(TOFIX(out_height, 32), (TOFIX(in_height, 16) + out_height / 2));
          inv_h = FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);
          SetDecRegister(pp_regs, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
          SetDecRegister(pp_regs, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
        } else {
          u32 H, inv_h;

          SetDecRegister(pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

          H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);

          inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

          SetDecRegister(pp_regs, ppu_regs[i].HSCALE_INVRA_U, inv_h);
        }
      } else if(in_height > out_height) {
        /* downscale */
        if (hw_feature.pp_lanczos[i]) {
          u32 H, inv_h;

          SetDecRegister(pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

          H = FDIVI(TOFIX(out_height, 32), (TOFIX(in_height, 16) + out_height / 2));
          inv_h = FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

          SetDecRegister(pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);
          SetDecRegister(pp_regs, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
          SetDecRegister(pp_regs, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
        } else {
          u32 Cv;

          Cv = FDIVI(TOFIX(out_height, 16), in_height);
          if (TOFIX(out_height, 16) % in_height) Cv++;

          SetDecRegister(pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 2);

          SetDecRegister(pp_regs, ppu_regs[i].HSCALE_INVRA_U, Cv);
        }
      } else {
        SetDecRegister(pp_regs, ppu_regs[i].HSCALE_INVRA_U, 0);
        SetDecRegister(pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 0);
      }
    }
      if(bottom_field_flag &&
         ppb_mode == DEC_DPB_FRAME) {
        pp_field_offset = ppw;
        pp_field_offset_ch = ppw_c;
      }

      if (p_hw_feature->pp_lanczos[i]) {
        SET_ADDR_REG2(pp_regs,
                      ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_MSB,
                      ppu_cfg->lanczos_table.bus_address);
      }
      if (p_hw_feature->pp_stride_support) {
        SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_Y_STRIDE, ppw);
        SetDecRegister(pp_regs, ppu_regs[i].PP_OUT_C_STRIDE, ppw_c);
      }
      if (!ppu_cfg->rgb_planar) {
        SET_ADDR_REG2(pp_regs,
                      ppu_regs[i].PP_OUT_LU_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_LU_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset);
        SET_ADDR_REG2(pp_regs,
                      ppu_regs[i].PP_OUT_CH_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_CH_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->chroma_offset + pp_field_offset_ch);
      } else {
        u32 offset = NEXT_MULTIPLE(ppw * ppu_cfg->scale.height, HABANA_OFFSET); 
        if (IS_PACKED_RGB(ppu_cfg->rgb_format)) {
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_R_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_R_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset);
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_G_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_G_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + offset + pp_field_offset);
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_B_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_B_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * offset + pp_field_offset);
        } else {
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_B_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_B_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + pp_field_offset);
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_G_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_G_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + offset + pp_field_offset);
          SET_ADDR_REG2(pp_regs,
                        ppu_regs[i].PP_OUT_R_BASE_U_LSB,
                        ppu_regs[i].PP_OUT_R_BASE_U_MSB,
                        ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * offset + pp_field_offset);
        }
      }
    }
  }
}

u32 CalcPpUnitBufferSize(PpUnitIntConfig *ppu_cfg, u32 mono_chrome) {
  u32 i = 0;
  u32 pp_height, pp_stride, pp_buff_size;
  u32 ext_buffer_size = 0;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled) continue;
    if (ppu_cfg->tiled_e)
      pp_height = NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
    else {
      if (ppu_cfg->crop2.enabled)
        pp_height = ppu_cfg->crop2.height;
      else
        pp_height = ppu_cfg->scale.height;
    }
    pp_stride = ppu_cfg->ystride;
    if (ppu_cfg->rgb) {
      pp_buff_size = NEXT_MULTIPLE(pp_stride * pp_height, HABANA_OFFSET);
    } else if (ppu_cfg->rgb_planar) {
      pp_buff_size = 3 * NEXT_MULTIPLE(pp_stride * pp_height, HABANA_OFFSET);
    } else
      pp_buff_size = pp_stride * pp_height;
    ppu_cfg->luma_offset = ext_buffer_size;
    ppu_cfg->chroma_offset = ext_buffer_size + pp_buff_size;
    ppu_cfg->luma_size = pp_buff_size;
    /* chroma */
    if (!ppu_cfg->monochrome && !mono_chrome && !ppu_cfg->rgb && !ppu_cfg->rgb_planar) {
      if (ppu_cfg->tiled_e)
        pp_height = NEXT_MULTIPLE(ppu_cfg->scale.height/2, 4) / 4;
      else {
        if (ppu_cfg->planar)
          pp_height = ppu_cfg->scale.height;
        else if (ppu_cfg->crop2.enabled)
          pp_height = ppu_cfg->crop2.height / 2;
        else
          pp_height = ppu_cfg->scale.height / 2;
      }
      pp_stride = ppu_cfg->cstride;
      ppu_cfg->chroma_size = pp_stride * pp_height;
      pp_buff_size += pp_stride * pp_height;
    }
    ext_buffer_size += NEXT_MULTIPLE(pp_buff_size, 16);
  }
  return (ext_buffer_size + DEC_MAX_PPU_COUNT * DEC400_PP_TABLE_SIZE);
}

void PpUnitSetIntConfig(PpUnitIntConfig *ppu_int_cfg,
                        PpUnitConfig *ppu_ext_cfg,
                        u32 pixel_width, u32 frame_only, u32 mono_chrome) {
  u32 i;
  if (ppu_int_cfg == NULL || ppu_ext_cfg == NULL)
    return;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    ppu_int_cfg->enabled = ppu_ext_cfg->enabled;
    ppu_int_cfg->tiled_e = ppu_ext_cfg->tiled_e;
    ppu_int_cfg->rgb = ppu_ext_cfg->rgb;
    ppu_int_cfg->rgb_planar = ppu_ext_cfg->rgb_planar;
    ppu_int_cfg->cr_first = ppu_ext_cfg->cr_first;
    ppu_int_cfg->shaper_enabled = ppu_ext_cfg->shaper_enabled;
    ppu_int_cfg->crop.enabled = ppu_ext_cfg->crop.enabled;
    ppu_int_cfg->crop.set_by_user = ppu_ext_cfg->crop.set_by_user;
    ppu_int_cfg->crop.x = ppu_ext_cfg->crop.x;
    ppu_int_cfg->crop.y = ppu_ext_cfg->crop.y;
    ppu_int_cfg->crop.width = ppu_ext_cfg->crop.width;
    ppu_int_cfg->crop.height = ppu_ext_cfg->crop.height;
    ppu_int_cfg->crop2.enabled = ppu_ext_cfg->crop2.enabled;
    ppu_int_cfg->crop2.x = ppu_ext_cfg->crop2.x;
    ppu_int_cfg->crop2.y = ppu_ext_cfg->crop2.y;
    ppu_int_cfg->crop2.width = ppu_ext_cfg->crop2.width;
    ppu_int_cfg->crop2.height = ppu_ext_cfg->crop2.height;
    ppu_int_cfg->scale.enabled = ppu_ext_cfg->scale.enabled;
    ppu_int_cfg->scale.set_by_user = ppu_ext_cfg->scale.set_by_user;
    ppu_int_cfg->scale.ratio_x = ppu_ext_cfg->scale.ratio_x;
    ppu_int_cfg->scale.ratio_y = ppu_ext_cfg->scale.ratio_y;
    ppu_int_cfg->scale.width= ppu_ext_cfg->scale.width;
    ppu_int_cfg->scale.height = ppu_ext_cfg->scale.height;
    ppu_int_cfg->monochrome = ppu_ext_cfg->monochrome;
    ppu_int_cfg->out_p010 = ppu_ext_cfg->out_p010;
    ppu_int_cfg->out_I010 = ppu_ext_cfg->out_I010;
    ppu_int_cfg->out_L010 = ppu_ext_cfg->out_L010;
    ppu_int_cfg->out_1010 = ppu_ext_cfg->out_1010;
    ppu_int_cfg->out_cut_8bits = ppu_ext_cfg->out_cut_8bits;
    ppu_int_cfg->planar = ppu_ext_cfg->planar;
    ppu_int_cfg->align = ppu_ext_cfg->align;
    ppu_int_cfg->ystride = ppu_ext_cfg->ystride;
    ppu_int_cfg->cstride = ppu_ext_cfg->cstride;
    ppu_int_cfg->video_range = ppu_ext_cfg->video_range;
    if (ppu_ext_cfg->rgb || ppu_ext_cfg->rgb_planar) {
      switch (ppu_ext_cfg->rgb_format) {
      case DEC_OUT_FRM_RGB888:
          ppu_int_cfg->rgb_format = PP_OUT_RGB888;
        break;
      case DEC_OUT_FRM_BGR888:
        ppu_int_cfg->rgb_format = PP_OUT_BGR888;
        break;
      case DEC_OUT_FRM_R16G16B16:
        ppu_int_cfg->rgb_format = PP_OUT_R16G16B16;
        break;
      case DEC_OUT_FRM_B16G16R16:
        ppu_int_cfg->rgb_format = PP_OUT_B16G16R16;
        break;
      case DEC_OUT_FRM_ARGB888:
        ppu_int_cfg->rgb_format = PP_OUT_ARGB888;
        break;
      case DEC_OUT_FRM_ABGR888:
        ppu_int_cfg->rgb_format = PP_OUT_ABGR888;
        break;
      case DEC_OUT_FRM_A2R10G10B10:
        ppu_int_cfg->rgb_format = PP_OUT_A2R10G10B10;
        break;
      default:
        ASSERT(0);
        break;
      }
      ppu_int_cfg->rgb_stan = ppu_ext_cfg->rgb_stan;
      if (ppu_ext_cfg->video_range) {
        if (pixel_width == 8) {
          ppu_int_cfg->range_max = 255;
          ppu_int_cfg->range_min = 0;
        } else {
          ppu_int_cfg->range_max = 1023;
          ppu_int_cfg->range_min = 0;
        }
      } else {
        if (pixel_width == 8) {
          ppu_int_cfg->range_max = 235;
          ppu_int_cfg->range_min = 16;
        } else {
          ppu_int_cfg->range_max = 940;
          ppu_int_cfg->range_min = 64;
        }
      }
    }
    if (ppu_ext_cfg->tiled_e && pixel_width > 8)
      ppu_int_cfg->out_cut_8bits = 0;
    if (pixel_width == 8 || ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
      ppu_int_cfg->out_cut_8bits = 0;
      ppu_int_cfg->out_p010 = 0;
      ppu_int_cfg->out_1010 = 0;
      ppu_int_cfg->out_I010 = 0;
      ppu_int_cfg->out_L010 = 0;
    }
    if (ppu_int_cfg->out_cut_8bits) {
      ppu_int_cfg->out_p010 = 0;
      ppu_int_cfg->out_1010 = 0;
      ppu_int_cfg->out_I010 = 0;
      ppu_int_cfg->out_L010 = 0;
    }
    if (!IS_PLANAR_RGB(ppu_int_cfg->rgb_format)) {
      ppu_int_cfg->rgb_planar = 0;
    }
    if (ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
      ppu_int_cfg->out_format = PP_OUT_FMT_RGB;
    } else if (ppu_int_cfg->monochrome || mono_chrome) {
      if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 ||
          ppu_int_cfg->out_L010 || (ppu_ext_cfg->tiled_e && pixel_width > 8))
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV400_P010;
      else if (ppu_int_cfg->out_cut_8bits)
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV400_8BIT;
      else
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV400;
      ppu_int_cfg->monochrome = 1;
    } else if (ppu_int_cfg->planar) {
      if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 || ppu_int_cfg->out_L010)
        ppu_int_cfg->out_format = PP_OUT_FMT_IYUV_P010;
      else if (ppu_int_cfg->out_cut_8bits)
        ppu_int_cfg->out_format = PP_OUT_FMT_IYUV_8BIT;
      else
        ppu_int_cfg->out_format = PP_OUT_FMT_IYUVPACKED;
    } else if (ppu_int_cfg->out_1010) {
      ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_10;
    } else {
      if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 ||
          ppu_int_cfg->out_L010 || (ppu_ext_cfg->tiled_e && pixel_width > 8))
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_P010;
      else if (ppu_int_cfg->out_cut_8bits)
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_8BIT;
      else
        ppu_int_cfg->out_format = PP_OUT_FMT_YUV420PACKED;
    }
    if (!frame_only)
      ppu_int_cfg->tiled_e = 0;
    ppu_int_cfg->pixel_width = (ppu_int_cfg->out_cut_8bits ||
                                pixel_width == 8) ? 8 :
                               ((ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 || ppu_int_cfg->out_L010 ||
                                 ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar ||
                                (ppu_ext_cfg->tiled_e && pixel_width > 8)) ? 16 : pixel_width);
    ppu_ext_cfg++;
    ppu_int_cfg++;
  }
}

enum DecPictureFormat TransUnitConfig2Format(PpUnitIntConfig *ppu_int_cfg) {
  enum DecPictureFormat output_format = DEC_OUT_FRM_TILED_4X4;
  if (ppu_int_cfg->tiled_e) {
    if (ppu_int_cfg->monochrome) {
      if (ppu_int_cfg->pixel_width != 8) {
        if (ppu_int_cfg->out_cut_8bits == 1)
          output_format = DEC_OUT_FRM_YUV400TILE;
        else if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 || ppu_int_cfg->out_L010)
          output_format = DEC_OUT_FRM_YUV400TILE_P010;
        else if (ppu_int_cfg->out_1010 == 1)
          output_format = DEC_OUT_FRM_YUV400TILE_1010;
      } else
        output_format = DEC_OUT_FRM_YUV400TILE;
    } else
      output_format = DEC_OUT_FRM_TILED_4X4;
  } else if (ppu_int_cfg->rgb ||
             ppu_int_cfg->rgb_planar) {
    switch (ppu_int_cfg->rgb_format) {
    case PP_OUT_RGB888:
      if (ppu_int_cfg->rgb_planar)
        output_format = DEC_OUT_FRM_RGB888_P;
      else
        output_format = DEC_OUT_FRM_RGB888;
      break;
    case PP_OUT_BGR888:
      if (ppu_int_cfg->rgb_planar)
        output_format = DEC_OUT_FRM_BGR888_P;
      else
        output_format = DEC_OUT_FRM_BGR888;
      break;
    case PP_OUT_R16G16B16:
      if (ppu_int_cfg->rgb_planar)
        output_format = DEC_OUT_FRM_R16G16B16_P;
      else
        output_format = DEC_OUT_FRM_R16G16B16;
      break;
    case PP_OUT_B16G16R16:
      if (ppu_int_cfg->rgb_planar)
        output_format = DEC_OUT_FRM_B16G16R16_P;
      else
        output_format = DEC_OUT_FRM_B16G16R16;
      break;
    case PP_OUT_ARGB888:
        output_format = DEC_OUT_FRM_ARGB888;
      break;
    case PP_OUT_ABGR888:
        output_format = DEC_OUT_FRM_ABGR888;
      break;
    case PP_OUT_A2R10G10B10:
        output_format = DEC_OUT_FRM_A2R10G10B10;
      break;
    case PP_OUT_A2B10G10R10:
        output_format = DEC_OUT_FRM_A2B10G10R10;
      break;
    default:
      ASSERT(0);
      break;
    }
  } else if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010 || ppu_int_cfg->out_L010) {
    if (ppu_int_cfg->monochrome)
      output_format = DEC_OUT_FRM_YUV400_P010;
    else if(ppu_int_cfg->planar)
      output_format = DEC_OUT_FRM_YUV420P_P010;
    else
      output_format = DEC_OUT_FRM_YUV420SP_P010;
  } else if (ppu_int_cfg->out_1010) {
    if (ppu_int_cfg->monochrome)
      output_format = DEC_OUT_FRM_YUV400_1010;
    else
      output_format = DEC_OUT_FRM_YUV420SP_1010;
  } else if (ppu_int_cfg->out_cut_8bits) {
    if (ppu_int_cfg->monochrome)
      output_format = DEC_OUT_FRM_YUV400;
    else {
      if(ppu_int_cfg->planar)
        output_format = DEC_OUT_FRM_YUV420P;
      else if(ppu_int_cfg->cr_first)
        output_format = DEC_OUT_FRM_NV21SP;
      else
        output_format = DEC_OUT_FRM_YUV420SP;
    }
  } else {
    if (ppu_int_cfg->monochrome)
      output_format = DEC_OUT_FRM_YUV400;
    else {
      if(ppu_int_cfg->planar)
        output_format = DEC_OUT_FRM_PLANAR_420;
      else if(ppu_int_cfg->cr_first)
        output_format = DEC_OUT_FRM_NV21SP;
      else
        output_format = DEC_OUT_FRM_RASTER_SCAN;
    }
  }
  return output_format;
}

void CheckOutputFormat(PpUnitIntConfig *ppu_cfg, enum DecPictureFormat *output_format,
                         u32 index) {
  u32 i = index;

  if (ppu_cfg[i].tiled_e) {
    if (ppu_cfg[i].monochrome) {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        *output_format = DEC_OUT_FRM_YUV400TILE_P010;
      } else if (ppu_cfg[i].out_1010) {
        *output_format = DEC_OUT_FRM_YUV400TILE_1010;
      } else {
        *output_format = DEC_OUT_FRM_YUV400TILE;
      }
    } else if (ppu_cfg[i].cr_first) {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        *output_format = DEC_OUT_FRM_NV21TILE_P010;
      } else if (ppu_cfg[i].out_1010) {
        *output_format = DEC_OUT_FRM_NV21TILE_1010;
      } else {
        *output_format = DEC_OUT_FRM_NV21TILE;
      }
    } else {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        *output_format = DEC_OUT_FRM_YUV420TILE_P010;
      } else if (ppu_cfg[i].out_1010) {
        *output_format = DEC_OUT_FRM_YUV420TILE_1010;
      } else {
        *output_format = DEC_OUT_FRM_YUV420TILE;
      }
    }
  } else if (ppu_cfg[i].rgb ||
             ppu_cfg[i].rgb_planar) {
    switch (ppu_cfg[i].rgb_format) {
    case PP_OUT_RGB888:
      if (ppu_cfg[i].rgb_planar)
        *output_format = DEC_OUT_FRM_RGB888_P;
      else
        *output_format = DEC_OUT_FRM_RGB888;
      break;
    case PP_OUT_BGR888:
      if (ppu_cfg[i].rgb_planar)
        *output_format = DEC_OUT_FRM_BGR888_P;
      else
        *output_format = DEC_OUT_FRM_BGR888;
      break;
    case PP_OUT_R16G16B16:
      if (ppu_cfg[i].rgb_planar)
        *output_format = DEC_OUT_FRM_R16G16B16_P;
      else
        *output_format = DEC_OUT_FRM_R16G16B16;
      break;
    case PP_OUT_B16G16R16:
      if (ppu_cfg[i].rgb_planar)
        *output_format = DEC_OUT_FRM_B16G16R16_P;
      else
        *output_format = DEC_OUT_FRM_B16G16R16;
      break;
    case PP_OUT_ARGB888:
        *output_format = DEC_OUT_FRM_ARGB888;
      break;
    case PP_OUT_ABGR888:
        *output_format = DEC_OUT_FRM_ABGR888;
      break;
    case PP_OUT_A2R10G10B10:
        *output_format = DEC_OUT_FRM_A2R10G10B10;
      break;
    case PP_OUT_A2B10G10R10:
        *output_format = DEC_OUT_FRM_A2B10G10R10;
      break;
    default:
      ASSERT(0);
      break;
    }
  } else {
    if (ppu_cfg[i].monochrome) {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        *output_format = DEC_OUT_FRM_YUV400_P010;
      } else if (ppu_cfg[i].out_1010) {
        *output_format = DEC_OUT_FRM_YUV400_1010;
      } else {
        *output_format = DEC_OUT_FRM_YUV400;
      }
    } else if (ppu_cfg[i].cr_first) {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_NV21P_P010;
        } else {
          *output_format = DEC_OUT_FRM_NV21SP_P010;
        }
      } else if (ppu_cfg[i].out_1010) {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_NV21P_1010;
        } else {
          *output_format = DEC_OUT_FRM_NV21SP_1010;
        }
      } else {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_NV21P;
        } else {
          *output_format = DEC_OUT_FRM_NV21SP;
        }
      }
    } else {
      if(ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010 || ppu_cfg[i].out_L010) {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_YUV420P_P010;
        } else {
          *output_format = DEC_OUT_FRM_YUV420SP_P010;
        }
      } else if (ppu_cfg[i].out_1010) {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_YUV420P_1010;
        } else {
          *output_format = DEC_OUT_FRM_YUV420SP_1010;
        }
      } else {
        if(ppu_cfg[i].planar) {
          *output_format = DEC_OUT_FRM_YUV420P;
        } else {
          *output_format = DEC_OUT_FRM_YUV420SP;
        }
      }
    }
  }
}
