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

#include "dwl.h"
#include "regdrv.h"
#include "sw_util.h"
#include "string.h"

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define MAX_REG_COUNT 512

static const u32 reg_mask[33] = {
  0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
  0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
  0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
  0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
  0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
  0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

#define CASE_G1_LEGACY_REG(ID) case (ID): id = ID##_G1; break;
#define CASE_G2_LEGACY_REG(ID) case (ID): id = ID##_G2; break;
#define IGNORE_LEGACY_REG(ID) if (id == ID) { return; }
#define IGNORE_RET_LEGACY_REG(ID) if (id == ID) { return 0; }

typedef u32 (*hw_reg_sepc_ptr)[HWIF_LAST_REG + 1][4];

void SetDecRegister(u32* reg_base, u32 id, u32 value) {

  u32 tmp;
  u32 (*hw_reg_spec)[HWIF_LAST_REG + 1][4] = {NULL};

  if ((reg_base[0] >> 16) == 0x6731) {
    /* G1 legacy registers */
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec_g1;
  } else if ((reg_base[0] >> 16) == 0x6732) {
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec_g2;
  } else if ((reg_base[0] >> 16) == 0x8001) {
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec;
  } else {
    ASSERT(0);
  }

  ASSERT(id < HWIF_LAST_REG);

  if ((*hw_reg_spec)[id][0] == 0) {
    /* Try to set a undefined register -> return directly */
    return;
  }

  tmp = reg_base[(*hw_reg_spec)[id][0]];
  tmp &= ~(reg_mask[(*hw_reg_spec)[id][1]] << (*hw_reg_spec)[id][2]);
  tmp |= (value & reg_mask[(*hw_reg_spec)[id][1]]) << (*hw_reg_spec)[id][2];
  reg_base[(*hw_reg_spec)[id][0]] = tmp;
}

void SetPpRegister(u32 *pp_reg_base, u32 id, u32 value) {
  u32 tmp;

  ASSERT(id < HWIF_LAST_REG);

  /* hw reg spec is based on decoder so we have to adjust the register offset
   * because we are getting PP base register as parameter
   */

  tmp = pp_reg_base[hw_dec_reg_spec_g1[id][0] - PP_REG_START];
  tmp &= ~(reg_mask[hw_dec_reg_spec_g1[id][1]] << hw_dec_reg_spec_g1[id][2]);
  tmp |= (value & reg_mask[hw_dec_reg_spec_g1[id][1]]) << hw_dec_reg_spec_g1[id][2];
  pp_reg_base[hw_dec_reg_spec_g1[id][0] - PP_REG_START] = tmp;
}

u32 GetDecRegister(const u32* reg_base, u32 id) {

  u32 tmp;
  u32 (*hw_reg_spec)[HWIF_LAST_REG + 1][4] = {NULL};

  ASSERT(id < HWIF_LAST_REG);

  if ((reg_base[0] >> 16) == 0x6731) {
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec_g1;
  } else if ((reg_base[0] >> 16) == 0x6732) {
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec_g2;
  } else if ((reg_base[0] >> 16) == 0x8001) {
    hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec;
  } else {
    ASSERT(0);
  }

  /* Try to reading a undefined register -> return 0 */
  if ((*hw_reg_spec)[id][0] == 0)
    return 0;

  tmp = reg_base[(*hw_reg_spec)[id][0]];
  tmp = tmp >> (*hw_reg_spec)[id][2];
  tmp &= reg_mask[(*hw_reg_spec)[id][1]];
  return (tmp);
}

u32 GetPpRegister(const u32 *pp_reg_base, u32 id) {
  u32 tmp;

  ASSERT(id < HWIF_LAST_REG);

  /* hw reg spec is based on decoder so we have to adjust the register offset
   * because we are getting PP base register as parameter
   */

  tmp = pp_reg_base[hw_dec_reg_spec_g1[id][0] - PP_REG_START];
  tmp = tmp >> hw_dec_reg_spec_g1[id][2];
  tmp &= reg_mask[hw_dec_reg_spec_g1[id][1]];
  return (tmp);
}

static void GetDecRegNumbers(u32* reg_count, u32* reg_offsets,
                             u32 writable_only) {
  u32 offset = -1, prev_offset = -1;
  u32 found;
  u32 reg_count_local = 0;
  u32 reg_offsets_local[MAX_REG_COUNT] = {0};
  u32* reg_offsets_local_addr = &reg_offsets_local[0];
  /* Loop through registers (as defined in enum HwIfName enumeration). */
  for (u32 reg = 0; reg != HWIF_LAST_REG; reg++) {
    if (!writable_only || hw_dec_reg_spec[reg][3] == 1) {
      offset = hw_dec_reg_spec[reg][0];
      /* Loop may write single reg multiple time, even if we do this simple
       * check for the same reg multiple times in a row. */
      found = 0;
      for (u32 i = 0; i < reg; i++) {
        if (offset == hw_dec_reg_spec[i][0]) {
          found = 1; break;
        }
      }
      if (found) continue;
      (*reg_offsets_local_addr++) = prev_offset = offset;
      reg_count_local++;
    }
  }
  *reg_count = reg_count_local;
  memcpy(reg_offsets, reg_offsets_local, sizeof(reg_offsets_local));
}

void FlushDecRegisters(const void* dwl, i32 core_id, u32* regs) {
  static u32 reg_count = MAX_REG_COUNT + 1;
  static u32 reg_offsets[MAX_REG_COUNT] = {0};

#ifdef TRACE_START_MARKER
  /* write ID register to trigger logic analyzer */
  DWLWriteReg(dwl, dec_cont->core_id, 0x00, ~0);
#endif

  if (reg_count == MAX_REG_COUNT + 1)
    GetDecRegNumbers(&reg_count, reg_offsets, /* Writable regs only */1);
  for (u32 i = 0; i < reg_count; i++)
    DWLWriteReg(dwl, core_id, reg_offsets[i] * 4, regs[reg_offsets[i]]);
}

void RefreshDecRegisters(const void* dwl, i32 core_id, u32* regs) {
  static u32 reg_count = MAX_REG_COUNT + 1;
  static u32 reg_offsets[MAX_REG_COUNT] = {0};

  if (reg_count == MAX_REG_COUNT + 1)
    GetDecRegNumbers(&reg_count, reg_offsets, /* All regs */0);
  for (u32 i = 0; i < reg_count; i++)
    regs[reg_offsets[i]] = DWLReadReg(dwl, core_id, reg_offsets[i] * 4);
}
