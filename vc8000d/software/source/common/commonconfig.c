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

#include "commonconfig.h"
#include "dwl.h"
#include "deccfg.h"
#include "regdrv.h"
#include "sw_util.h"

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9  13
#define DEC_MODE_AVS2 16


/* These are globals, so that test code can meddle with them. On production
 * they will always retain their values. */
u32 dec_stream_swap = HANTRODEC_STREAM_SWAP;
u32 dec_pic_swap = HANTRODEC_STREAM_SWAP;
u32 dec_dirmv_swap = HANTRODEC_STREAM_SWAP;

/* For VC8000D, HW need little endian but all G1 format write big endian
  As for HEVC and VP9, this value will be overwritten to 0. */
u32 dec_tab_swap = HANTRODEC_STREAM_SWAP_8 | HANTRODEC_STREAM_SWAP_16;
u32 dec_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 dec_bus_width = DEC_X170_BUS_WIDTH;
#ifdef DEC_X170_REFBU_SEQ
u32 dec_apf_treshold = DEC_X170_REFBU_NONSEQ / DEC_X170_REFBU_SEQ;
#else
u32 dec_apf_treshold = DEC_X170_REFBU_NONSEQ;
#endif /* DEC_X170_REFBU_SEQ */
u32 dec_apf_disable = DEC_X170_APF_DISABLE;

u32 dec_clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 dec_clock_gating_runtime = DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME;

u32 dec_ref_double_buffer = HANTRODEC_INTERNAL_DOUBLE_REF_BUFFER;

u32 dec_timeout_cycles = HANTRODEC_TIMEOUT_OVERRIDE;
u32 dec_axi_id_rd = DEC_X170_AXI_ID_R;
u32 dec_axi_id_rd_unique_enable = DEC_X170_AXI_ID_R_E;
u32 dec_axi_id_wr = DEC_X170_AXI_ID_W;
u32 dec_axi_id_wr_unique_enable = DEC_X170_AXI_ID_W_E;
u32 dec_pp_in_blk_size = HANTRODEC_PP_IN_BLK_SIZE_64X16;

void SetCommonConfigRegs(u32 *regs) {
  DWLHwConfig hw_config;
  (void)DWLmemset(&hw_config, 0, sizeof(hw_config));
  if (IS_G1_LEGACY(regs[0]))
    DWLReadAsicConfig(&hw_config, DWL_CLIENT_TYPE_H264_DEC);
  else
    DWLReadAsicConfig(&hw_config, DWL_CLIENT_TYPE_HEVC_DEC);

  /* Check that the register count define DEC_X170_REGISTERS is
   * big enough for the defined HW registers in hw_dec_reg_spec */
  ASSERT(hw_dec_reg_spec[HWIF_LAST_REG - 3][0] < DEC_X170_REGISTERS);

  /* set parameters defined in deccfg.h */
  SetDecRegister(regs, HWIF_DEC_STRM_SWAP, dec_stream_swap);
  SetDecRegister(regs, HWIF_DEC_PIC_SWAP, dec_pic_swap);
  SetDecRegister(regs, HWIF_DEC_DIRMV_SWAP, dec_dirmv_swap);

  if (GetDecRegister(regs, HWIF_DEC_MODE) == DEC_MODE_HEVC ||
      GetDecRegister(regs, HWIF_DEC_MODE) == DEC_MODE_VP9 ||
      GetDecRegister(regs, HWIF_DEC_MODE) == DEC_MODE_AVS2)
    SetDecRegister(regs, HWIF_DEC_TAB_SWAP, 0);
  else
    SetDecRegister(regs, HWIF_DEC_TAB_SWAP, dec_tab_swap);

  SetDecRegister(regs, HWIF_DEC_BUSWIDTH, dec_bus_width);
  SetDecRegister(regs, HWIF_DEC_MAX_BURST, dec_burst_length);
  if (hw_config.double_buffer_support == DOUBLE_BUFFER_SUPPORTED)
    SetDecRegister(regs, HWIF_DEC_REFER_DOUBLEBUFFER_E, dec_ref_double_buffer);
  else
    SetDecRegister(regs, HWIF_DEC_REFER_DOUBLEBUFFER_E, 0);
#if (HWIF_APF_SINGLE_PU_MODE)
  SetDecRegister(regs, HWIF_APF_SINGLE_PU_MODE, 1);
  SetDecRegister(regs, HWIF_APF_DISABLE, 1);
  SetDecRegister(regs, HWIF_APF_THRESHOLD, 0);
#elif(DEC_X170_APF_DISABLE)
  SetDecRegister(regs, HWIF_APF_SINGLE_PU_MODE, 0);
  SetDecRegister(regs, HWIF_APF_DISABLE, 1);
  SetDecRegister(regs, HWIF_APF_THRESHOLD, 0);
#else
  {
    u32 apf_tmp_threshold = dec_apf_treshold;
    SetDecRegister(regs, HWIF_APF_DISABLE, dec_apf_disable);
    if (apf_tmp_threshold > 63) apf_tmp_threshold = 63;
    SetDecRegister(regs, HWIF_APF_THRESHOLD, apf_tmp_threshold);
  }
#endif

#if (DEC_X170_USING_IRQ == 0)
  SetDecRegister(regs, HWIF_DEC_IRQ_DIS, 1);
#else
  SetDecRegister(regs, HWIF_DEC_IRQ_DIS, 0);
#endif

  SetDecRegister(regs, HWIF_DEC_OUT_EC_BYPASS, 0);
  SetDecRegister(regs, HWIF_APF_ONE_PID, 0);

  /* Clock gating parameters */
  SetDecRegister(regs, HWIF_DEC_CLK_GATE_E, dec_clock_gating);

  /* set AXI RW IDs */
  SetDecRegister(regs, HWIF_DEC_AXI_RD_ID_E,
                 (dec_axi_id_rd_unique_enable & 0x1));
  SetDecRegister(regs, HWIF_DEC_AXI_WD_ID_E,
                 (dec_axi_id_wr_unique_enable & 0x1));
  SetDecRegister(regs, HWIF_DEC_AXI_RD_ID,
                 (dec_axi_id_rd & 0xFFU));
  SetDecRegister(regs, HWIF_DEC_AXI_WR_ID,
                 (dec_axi_id_wr & 0xFFU));

  SetDecRegister(regs, HWIF_EXT_TIMEOUT_OVERRIDE_E, dec_timeout_cycles ? 1 : 0);
  SetDecRegister(regs, HWIF_EXT_TIMEOUT_CYCLES, dec_timeout_cycles);

  SetDecRegister(regs, HWIF_TIMEOUT_OVERRIDE_E, dec_timeout_cycles ? 1 : 0);
  SetDecRegister(regs, HWIF_TIMEOUT_CYCLES, dec_timeout_cycles);
  SetDecRegister(regs, HWIF_PP_IN_BLK_SIZE_U, dec_pp_in_blk_size);

  if (IS_G1_LEGACY(regs[0]))
    SetLegacyG1CommonConfigRegs(regs);
  else if (IS_G2_LEGACY(regs[0]))
    SetLegacyG2CommonConfigRegs(regs);
  else {
    /* AXI outstanding threshold */
    SetDecRegister(regs, HWIF_AXI_RD_OSTD_THRESHOLD,
                   DEC_X170_MAX_RD_OUTSTANDING_THRESHOLD);
    SetDecRegister(regs, HWIF_AXI_WR_OSTD_THRESHOLD,
                   DEC_X170_MAX_WR_OUTSTANDING_THRESHOLD);
  }
}

