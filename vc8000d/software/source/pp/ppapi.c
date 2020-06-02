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
#include "version.h"
#include "basetype.h"
#include "ppapi.h"
#include "ppinternal.h"
#include "regdrv.h"
#include "dwl.h"
#include "decppif.h"
#include "ppcfg.h"
#include "ppu.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "sw_util.h"
#include "sw_debug.h"
#include <string.h>

/*------------------------------------------------------------------------------
    Function name   : PPInit
    Description     : initialize pp
    Return type     : PPResult
    Argument        : PPInst * post_pinst
------------------------------------------------------------------------------*/
PPResult PPInit(PPInst * p_post_pinst, const void *dwl) {
  PPContainer *p_pp_cont;
  u32 *pp_regs;
  u32 asic_id;

  if(p_post_pinst == NULL || dwl == NULL) {
    return (PP_PARAM_ERROR);
  }

  *p_post_pinst = NULL; /* return NULL instance for any error */
  /* check for proper hardware */
  asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_ST_PP);

  if ((asic_id >> 16) != 0x6731U &&
      (asic_id >> 16) != 0x8001U) {
    return PP_PARAM_ERROR;
  }

  p_pp_cont = (PPContainer *) DWLmalloc(sizeof(PPContainer));

  if(p_pp_cont == NULL) {
    return (PP_MEMFAIL);
  }

  pp_regs = p_pp_cont->pp_regs;
  (void) DWLmemset(p_pp_cont, 0, sizeof(PPContainer));
  (void) DWLmemset(pp_regs, 0, TOTAL_X170_REGISTERS * sizeof(*pp_regs));
  p_pp_cont->dwl = dwl;

  p_pp_cont->pp_regs[0] = asic_id;
  SetDecRegister(p_pp_cont->pp_regs, HWIF_DEC_MODE, 14);
  SetCommonConfigRegs(p_pp_cont->pp_regs);
  *p_post_pinst = p_pp_cont;

  return (PP_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPSetInfo
    Description     :
    Return type     : PPResult
    Argument        : PPInst post_pinst
    Argument        : PPConfig * p_pp_conf
------------------------------------------------------------------------------*/
PPResult PPSetInfo(PPInst post_pinst, PPConfig * p_pp_conf) {

  PPContainer *pp_c;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  PpUnitIntConfig *ppu_cfg;

  if(post_pinst == NULL || p_pp_conf == NULL) {
    return (PP_PARAM_ERROR);
  }

  if ((!p_pp_conf->in_format && p_pp_conf->in_stride < p_pp_conf->in_width) ||
      (p_pp_conf->in_format && p_pp_conf->in_stride < 2 * p_pp_conf->in_width)) {
    ERROR_PRINT("Input stride illegal.");
    return (PP_PARAM_ERROR);
  }

  pp_c = (PPContainer *) post_pinst;
  ppu_cfg = pp_c->ppu_cfg;
  pp_c->in_format = p_pp_conf->in_format;
  pp_c->in_stride = p_pp_conf->in_stride;
  pp_c->in_height = p_pp_conf->in_height;
  pp_c->pp_in_buffer = p_pp_conf->pp_in_buffer;
  pp_c->pp_out_buffer = p_pp_conf->pp_out_buffer;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_ST_PP);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  PpUnitSetIntConfig(ppu_cfg, p_pp_conf->ppu_config, p_pp_conf->in_format ? 10 : 8, 1, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (hw_feature.pp_lanczos[i] && (pp_c->ppu_cfg[i].lanczos_table.virtual_address == NULL)) {
      u32 size = LANCZOS_BUFFER_SIZE * sizeof(i32);
      i32 ret = DWLMallocLinear(pp_c->dwl, size, &pp_c->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(PP_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(&hw_feature, p_pp_conf->in_width, p_pp_conf->in_height, 0,  ppu_cfg))
    return PP_PARAM_ERROR;

  pp_c->pp_enabled = ppu_cfg[0].enabled |
                     ppu_cfg[1].enabled |
                     ppu_cfg[2].enabled |
                     ppu_cfg[3].enabled |
                     ppu_cfg[4].enabled;
  CalcPpUnitBufferSize(ppu_cfg, 0);
  return (PP_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPRelease
    Description     :
    Return type     : void
    Argument        : PPInst post_pinst
------------------------------------------------------------------------------*/
void PPRelease(PPInst post_pinst) {
  PPContainer *pp_c;
  u32 i;
  if(post_pinst == NULL) {
    return;
  }

  pp_c = (PPContainer *) post_pinst;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (pp_c->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(pp_c->dwl, &pp_c->ppu_cfg[i].lanczos_table);
      pp_c->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  DWLfree(pp_c);
}

static const u32 coeff[6][5] = {{16384,22970,11700,5638,29032},
                                {19077,26149,13320,6419,33050},
                                {16384,25802,7670,3069,30402},
                                {19077,29372,8731,3494,34610},
                                {16384,24160,9361,2696,30825},
                                {19077,27504,10646,3069,35091}};
/*------------------------------------------------------------------------------
    Function name   : PPDecode
    Description     : set up and run pp
    Return type     : void
    Argument        : pp instance
------------------------------------------------------------------------------*/
PPResult PPDecode(PPInst post_pinst) {
  PPContainer *pp_c;
  i32 ret = 0;
  u32 crop_step_rshift = 1;
  u32 i;

  pp_c = (PPContainer *) post_pinst;

  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_PP);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(pp_c == NULL) {
    return(PP_PARAM_ERROR);
  }
  SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_FORMAT_U, (pp_c->in_format ? 10 : 1));
  /* Set PP-standalone mode related register */
  SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride);
  SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride);
  SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
  SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CH_BASE_U_LSB,
                HWIF_PP_IN_CH_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height);

  PpUnitIntConfig *ppu_cfg = &pp_c->ppu_cfg[0];
  addr_t ppu_out_bus_addr = pp_c->pp_out_buffer.bus_address;

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_E_U, ppu_cfg->enabled);

    if (!ppu_cfg->enabled) continue;

    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFA2_U, coeff[ppu_cfg->rgb_stan][0]);
    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFA1_U, coeff[ppu_cfg->rgb_stan][0]);
    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFB_U, coeff[ppu_cfg->rgb_stan][1]);
    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFC_U, coeff[ppu_cfg->rgb_stan][2]);
    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFD_U, coeff[ppu_cfg->rgb_stan][3]);
    SetDecRegister(pp_c->pp_regs, HWIF_COLOR_COEFFE_U, coeff[ppu_cfg->rgb_stan][4]);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_CR_FIRST, ppu_cfg->cr_first);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_TILE_E_U, ppu_cfg->tiled_e);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].OUT_FORMAT_U, ppu_cfg->out_format);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_RGB_FMT_U, ppu_cfg->rgb_format);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_RGB_PLANAR_U, ppu_cfg->rgb_planar);
    SetDecRegister(pp_c->pp_regs, HWIF_YCBCR_RANGE_U, ppu_cfg->video_range);
    SetDecRegister(pp_c->pp_regs, HWIF_RGB_RANGE_MAX_U, ppu_cfg->range_max);
    SetDecRegister(pp_c->pp_regs, HWIF_RGB_RANGE_MIN_U, ppu_cfg->range_min);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_P010_FMT_U, (ppu_cfg->out_L010 ? 2 :
                                                                  (ppu_cfg->out_I010 ? 1 : 0)));

#define TOFIX(d, q) ((u64)( (d) * ((u64)1<<(q)) ))
#define FDIVI(a, b) ((a)/(b))

    /* flexible scale ratio */
    u32 in_width = ppu_cfg->crop.width;
    u32 in_height = ppu_cfg->crop.height;
    u32 out_width = ppu_cfg->scale.width;
    u32 out_height = ppu_cfg->scale.height;

    SetDecRegister(pp_c->pp_regs, ppu_regs[i].CROP_STARTX_U,
                   ppu_cfg->crop.x >> crop_step_rshift);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].CROP_STARTY_U,
                   ppu_cfg->crop.y >> crop_step_rshift);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_IN_WIDTH_U,
                   ppu_cfg->crop.width >> crop_step_rshift);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_IN_HEIGHT_U,
                   ppu_cfg->crop.height >> crop_step_rshift);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_WIDTH_U,
                   ppu_cfg->scale.width);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_HEIGHT_U,
                   ppu_cfg->scale.height);

    if(in_width < out_width) {
      /* upscale */
      if (hw_feature.pp_lanczos[i]) {
        u32 W, inv_w;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

        W = FDIVI(TOFIX(out_width, 32), (TOFIX(in_width, 16) + out_width / 2));
        inv_w = FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
      } else {
        u32 W, inv_w;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 1);

        W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
        inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_U, inv_w);
      }
    } else if(in_width > out_width) {
      /* downscale */
      if (hw_feature.pp_lanczos[i]) {
        u32 W, inv_w;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

        W = FDIVI(TOFIX(out_width, 32), (TOFIX(in_width, 16) + out_width / 2));
        inv_w = FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_WRATIO_U, W);
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
      } else {
        u32 hnorm;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 2);

        hnorm = FDIVI(TOFIX(out_width, 16), in_width);
        if (TOFIX(out_width, 16) % in_width) hnorm++;
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_U, hnorm);
      }
    } else {
      SetDecRegister(pp_c->pp_regs, ppu_regs[i].WSCALE_INVRA_U, 0);
      SetDecRegister(pp_c->pp_regs, ppu_regs[i].HOR_SCALE_MODE_U, 0);
    }

    if(in_height < out_height) {
    /* upscale */
      if (hw_feature.pp_lanczos[i]) {
        u32 H, inv_h;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

        H = FDIVI(TOFIX(out_height, 32), (TOFIX(in_height, 16) + out_height / 2));
        inv_h = FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
      } else {
        u32 H, inv_h;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

        H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);

        inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HSCALE_INVRA_U, inv_h);
      }
    } else if(in_height > out_height) {
      /* downscale */
      if (hw_feature.pp_lanczos[i]) {
        u32 H, inv_h;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 1);

        H = FDIVI(TOFIX(out_height, 32), (TOFIX(in_height, 16) + out_height / 2));
        inv_h = FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].SCALE_HRATIO_U, H);
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
      } else {
        u32 Cv;

        Cv = FDIVI(TOFIX(out_height, 16), in_height);
        if (TOFIX(out_height, 16) % in_height) Cv++;

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 2);

        SetDecRegister(pp_c->pp_regs, ppu_regs[i].HSCALE_INVRA_U, Cv);
      }
    } else {
      SetDecRegister(pp_c->pp_regs, ppu_regs[i].HSCALE_INVRA_U, 0);
      SetDecRegister(pp_c->pp_regs, ppu_regs[i].VER_SCALE_MODE_U, 0);
    }
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_Y_STRIDE, ppu_cfg->ystride);
    SetDecRegister(pp_c->pp_regs, ppu_regs[i].PP_OUT_C_STRIDE, ppu_cfg->cstride);
    if (hw_feature.pp_lanczos[i]) {
      SET_ADDR_REG2(pp_c->pp_regs,
                    ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_LSB,
                    ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_MSB,
                    ppu_cfg->lanczos_table.bus_address);
    }
    if (!ppu_cfg->rgb_planar) {
      SET_ADDR_REG2(pp_c->pp_regs,
                    ppu_regs[i].PP_OUT_LU_BASE_U_LSB,
                    ppu_regs[i].PP_OUT_LU_BASE_U_MSB,
                    ppu_out_bus_addr + ppu_cfg->luma_offset);
      SET_ADDR_REG2(pp_c->pp_regs,
                    ppu_regs[i].PP_OUT_CH_BASE_U_LSB,
                    ppu_regs[i].PP_OUT_CH_BASE_U_MSB,
                    ppu_out_bus_addr + ppu_cfg->chroma_offset);
    } else {
      u32 offset = NEXT_MULTIPLE(ppu_cfg->ystride * ppu_cfg->scale.height, HABANA_OFFSET);
      if (IS_PACKED_RGB(ppu_cfg->rgb_format)) {
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_R_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_R_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset);
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_G_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_G_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset + offset);
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_B_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_B_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * offset);
      } else {
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_B_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_B_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset);
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_G_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_G_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset + offset);
        SET_ADDR_REG2(pp_c->pp_regs,
                      ppu_regs[i].PP_OUT_R_BASE_U_LSB,
                      ppu_regs[i].PP_OUT_R_BASE_U_MSB,
                      ppu_out_bus_addr + ppu_cfg->luma_offset + 2 * offset);
      }
    }
  }

  if(DWLReserveHw(pp_c->dwl, &pp_c->core_id, DWL_CLIENT_TYPE_ST_PP) != DWL_OK) {
    return PP_BUSY;
  }
  PPFlushRegs(pp_c);

  DWLReadPpConfigure(pp_c->dwl, pp_c->ppu_cfg, NULL, 0);
  SetDecRegister(pp_c->pp_regs, HWIF_DEC_E, 1);
  DWLEnableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
              pp_c->pp_regs[1]);
  ret = DWLWaitHwReady(pp_c->dwl, pp_c->core_id, (u32) (-1));

  if(ret != DWL_HW_WAIT_OK) {
    /* Reset HW */
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_E, 0);
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ, 0);

    DWLDisableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
                 pp_c->pp_regs[1]);
    DWLReleaseHw(pp_c->dwl, pp_c->core_id);
    return PP_HW_TIMEOUT;
  }

  PPRefreshRegs(pp_c);

  SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ, 0); /* just in case */
  DWLDisableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
               pp_c->pp_regs[1]);
  DWLReleaseHw(pp_c->dwl, pp_c->core_id);
  return PP_OK;
}

PPResult PPNextPicture(PPInst post_pinst, PPDecPicture *output) {
  PPContainer *pp_c;
  u32 i;
  PpUnitIntConfig *ppu_cfg;
  pp_c = (PPContainer *) post_pinst;
  (void) DWLmemset(output, 0, sizeof(PPDecPicture));
  ppu_cfg = pp_c->ppu_cfg;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled) continue;
    output->pictures[i].output_picture = (u32*)((addr_t)pp_c->pp_out_buffer.virtual_address + ppu_cfg->luma_offset);
    output->pictures[i].output_picture_bus_address = pp_c->pp_out_buffer.bus_address + ppu_cfg->luma_offset;
    if (ppu_cfg->monochrome) {
      output->pictures[i].output_picture_chroma = NULL;
      output->pictures[i].output_picture_chroma_bus_address = 0;
    } else {
      output->pictures[i].output_picture_chroma = (u32*)((addr_t)pp_c->pp_out_buffer.virtual_address + ppu_cfg->chroma_offset);
      output->pictures[i].output_picture_chroma_bus_address = pp_c->pp_out_buffer.bus_address + ppu_cfg->chroma_offset;
    }
    output->pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
    output->pictures[i].pic_width = ppu_cfg->scale.width;
    output->pictures[i].pic_height = ppu_cfg->scale.height;
    output->pictures[i].pic_stride = ppu_cfg->ystride;
    output->pictures[i].pic_stride_ch = ppu_cfg->cstride;
  }
  return PP_OK;
}
