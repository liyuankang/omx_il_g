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
#include "sw_util.h"
#include "regdrv.h"

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

u32 dec_tab0_swap = HANTRODEC_STREAM_SWAP;
u32 dec_tab1_swap = HANTRODEC_STREAM_SWAP;
u32 dec_tab2_swap = HANTRODEC_STREAM_SWAP;
u32 dec_tab3_swap = HANTRODEC_STREAM_SWAP;
u32 dec_rscan_swap = HANTRODEC_STREAM_SWAP;
u32 dec_comp_tab_swap = HANTRODEC_STREAM_SWAP;

extern u32 dec_bus_width;
extern u32 dec_burst_length;
extern u32 dec_ref_double_buffer;
extern u32 dec_apf_treshold;
extern u32 dec_apf_disable;
extern u32 dec_clock_gating;
extern u32 dec_clock_gating_runtime;
extern u32 dec_ref_double_buffer;
extern u32 dec_timeout_cycles;
extern u32 dec_axi_id_rd;
extern u32 dec_axi_id_rd_unique_enable;
extern u32 dec_axi_id_wr;
extern u32 dec_axi_id_wr_unique_enable;

void SetLegacyG1CommonConfigRegs(u32 *regs) {

  u32 asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_H264_DEC);
  /* set parameters defined in deccfg.h */
  SetDecRegister(regs, HWIF_DEC_OUT_ENDIAN,
                 DEC_X170_OUTPUT_PICTURE_ENDIAN);

  SetDecRegister(regs, HWIF_DEC_IN_ENDIAN,
                 DEC_X170_INPUT_DATA_ENDIAN);

  SetDecRegister(regs, HWIF_DEC_STRENDIAN_E,
                 DEC_X170_INPUT_STREAM_ENDIAN);

  SetDecRegister(regs, HWIF_DEC_MAX_BURST,
                 DEC_X170_BUS_BURST_LENGTH);


  SetDecRegister(regs, HWIF_DEC_SCMD_DIS,
                 DEC_X170_SCMD_DISABLE);

#if (DEC_X170_APF_DISABLE)
  SetDecRegister(regs, HWIF_DEC_ADV_PRE_DIS, 1);
  SetDecRegister(regs, HWIF_APF_THRESHOLD, 0);
#else
  {
    u32 apf_tmp_threshold = 0;

    SetDecRegister(regs, HWIF_DEC_ADV_PRE_DIS, 0);

    if(DEC_X170_REFBU_SEQ)
      apf_tmp_threshold = DEC_X170_REFBU_NONSEQ/DEC_X170_REFBU_SEQ;
    else
      apf_tmp_threshold = DEC_X170_REFBU_NONSEQ;

    if( apf_tmp_threshold > 63 )
      apf_tmp_threshold = 63;

    SetDecRegister(regs, HWIF_APF_THRESHOLD, apf_tmp_threshold);
  }
#endif

  SetDecRegister(regs, HWIF_DEC_LATENCY,
                 DEC_X170_LATENCY_COMPENSATION);

  SetDecRegister(regs, HWIF_DEC_DATA_DISC_E,
                 DEC_X170_DATA_DISCARD_ENABLE);

  SetDecRegister(regs, HWIF_DEC_OUTSWAP32_E,
                 DEC_X170_OUTPUT_SWAP_32_ENABLE);

  SetDecRegister(regs, HWIF_DEC_INSWAP32_E,
                 DEC_X170_INPUT_DATA_SWAP_32_ENABLE);

  SetDecRegister(regs, HWIF_DEC_STRSWAP32_E,
                 DEC_X170_INPUT_STREAM_SWAP_32_ENABLE);

#if( DEC_X170_HW_TIMEOUT_INT_ENA  != 0)
  SetDecRegister(regs, HWIF_DEC_TIMEOUT_E, 1);
#else
  SetDecRegister(regs, HWIF_DEC_TIMEOUT_E, 0);
#endif

#if( DEC_X170_INTERNAL_CLOCK_GATING != 0)
  SetDecRegister(regs, HWIF_DEC_CLK_GATE_E, 1);
#else
  SetDecRegister(regs, HWIF_DEC_CLK_GATE_E, 0);
#endif

  /* set AXI RW IDs */
  SetDecRegister(regs, HWIF_DEC_AXI_RD_ID,
                 (DEC_X170_AXI_ID_R & 0xFFU));
  SetDecRegister(regs, HWIF_DEC_AXI_WR_ID,
                 (DEC_X170_AXI_ID_W & 0xFFU));

  SetDecRegister(regs, HWIF_SWAP_64BIT_W, 0);
  SetDecRegister(regs, HWIF_SWAP_64BIT_R, 0);

  /* set PP out swap register */
  SetDecRegister(regs, HWIF_PP_OUT_SWAP_U, DEC_X170_PP_OUTPUT_SWAP);

  if (MAJOR_VERSION(asic_id) == 7) {
    if (MINOR_VERSION(asic_id) == 1) {
      /* g1v8_1 */
      SetDecRegister(regs, HWIF_BUSBUSY_CYCLES,
                     DEC_X170_TIMEOUT_CYCLES);
    } else if (MINOR_VERSION(asic_id) > 1) {
      SetDecRegister(regs, HWIF_EXTERNAL_TIMEOUT_OVERRIDE_E, 1);
      SetDecRegister(regs, HWIF_EXTERNAL_TIMEOUT_CYCLES, 0x500000);
      SetDecRegister(regs, HWIF_TIMEOUT_OVERRIDE_E, 1);
      SetDecRegister(regs, HWIF_TIMEOUT_CYCLES, 0xA00000);
    }
  }
}

void SetLegacyG2CommonConfigRegs(u32 *regs) {

  /* Check that the register count define DEC_X170_REGISTERS is
   * big enough for the defined HW registers in hw_dec_reg_spec */
  ASSERT(hw_dec_reg_spec[HWIF_LAST_REG - 3][0] < DEC_X170_REGISTERS);

  /* set parameters defined in deccfg.h */
  SetDecRegister(regs, HWIF_DEC_TAB0_SWAP, dec_tab0_swap);
  SetDecRegister(regs, HWIF_DEC_TAB1_SWAP, dec_tab1_swap);
  SetDecRegister(regs, HWIF_DEC_TAB2_SWAP, dec_tab2_swap);
  SetDecRegister(regs, HWIF_DEC_TAB3_SWAP, dec_tab3_swap);
  SetDecRegister(regs, HWIF_DEC_RSCAN_SWAP, dec_rscan_swap);
  SetDecRegister(regs, HWIF_DEC_COMP_TABLE_SWAP, dec_comp_tab_swap);

  SetDecRegister(regs, HWIF_DEC_OUT_EC_BYPASS, 0);
  SetDecRegister(regs, HWIF_APF_ONE_PID, 0);

  /* Clock gating parameters */
  SetDecRegister(regs, HWIF_DEC_CLK_GATE_E, dec_clock_gating);
  SetDecRegister(regs, HWIF_DEC_CLK_GATE_IDLE_E, dec_clock_gating_runtime);

  /* set AXI RW IDs */
  SetDecRegister(regs, HWIF_DEC_AXI_RD_ID_E,
                 (dec_axi_id_rd_unique_enable & 0x1));
  SetDecRegister(regs, HWIF_DEC_AXI_WD_ID_E,
                 (dec_axi_id_wr_unique_enable & 0x1));
  SetDecRegister(regs, HWIF_DEC_AXI_RD_ID,
                 (dec_axi_id_rd & 0xFFU));
  SetDecRegister(regs, HWIF_DEC_AXI_WR_ID,
                 (dec_axi_id_wr & 0xFFU));

  /* Set timeouts. Value of 0 implies use of hardware default. */
  SetDecRegister(regs, HWIF_EXT_TIMEOUT_CYCLES, 0x500000);
  SetDecRegister(regs, HWIF_BUSBUSY_CYCLES, 0x1900000);
}
