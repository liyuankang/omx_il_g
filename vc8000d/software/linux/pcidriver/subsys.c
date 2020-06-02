/*------------------------------------------------------------------------------
--       Copyright (c) 2020-    , VeriSilicon Inc. All rights reserved        --
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

#include "subsys.h"

/******************************************************************************/
/* subsystem configuration                                                    */
/******************************************************************************/

/* List of subsystems */
struct SubsysDesc subsys_array[] = {
  /* {slice_index, index, base, iosize} */
  {0, 0, 0x600000},
  {0, 1, 0x700000}
};

/* List of all HW cores. */
struct CoreDesc core_array[] = {
  /* {slice, subsys, core_type, offset, iosize, irq} */
#if 0
  {0, 0, HW_VC8000DJ, 0x600000, 0, 0},
  {0, 0, HW_VC8000D, 0x602000, 0, 0},
  {0, 0, HW_L2CACHE, 0x604000, 0, 0},
  {0, 0, HW_DEC400, 0x606000, 0, 0},
  {0, 0, HW_BIGOCEAN, 0x608000, 0, 0},
  {0, 0, HW_NOC,0x60a000, 0, 0},
  {0, 0, HW_AXIFE, 0x60c000, 0, 0}
#endif
//  {0, 0, HW_VCMD, 0x0, 27*4, -1},
  {0, 0, HW_VC8000D, 0x1000, 472*4, -1},
  {0, 0, HW_L2CACHE, 0x2000, 231*4, -1},
//  {0, 1, HW_VCMD, 0x0, 27*4, -1},
  {0, 1, HW_VC8000D, 0x1000, 472*4, -1},
  {0, 1, HW_L2CACHE, 0x2000, 231*4, -1},
};

extern struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM];
extern int total_vcmd_core_num;
extern unsigned long multicorebase[];
extern int irq[];
extern unsigned int iosize[];
extern int reg_count[];

/*
   If VCMD is used, convert core_array to vcmd_core_array, which are used in
   hantor_vcmd.c.
   Otherwise, covnert core_array to multicore_base/irq/iosize, which are used in
   hantro_dec.c

   VCMD:
        - struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM]
        - total_vcmd_core_num

   NON-VCMD:
        - multicorebase[HXDEC_MAX_CORES]
        - irq[HXDEC_MAX_CORES]
        - iosize[HXDEC_MAX_CORES]
*/
void CheckSubsysCoreArray(struct subsys_config *subsys, int *vcmd) {
  int num = sizeof(subsys_array)/sizeof(subsys_array[0]);
  int i, j;

  for (i = 0; i < num; i++) {
    subsys[i].base_addr = subsys_array[i].base;
    subsys[i].irq = -1;
    for (j = 0; j < HW_CORE_MAX; j++) {
      subsys[i].submodule_offset[j] = 0xffff;
      subsys[i].submodule_iosize[j] = 0;
      subsys[i].submodule_hwregs[j] = NULL;
    }
  }

  total_vcmd_core_num = 0;

  for (i = 0; i < sizeof(core_array)/sizeof(core_array[0]); i++) {
    subsys[core_array[i].subsys].submodule_offset[core_array[i].core_type]
                                  = core_array[i].offset;
    subsys[core_array[i].subsys].submodule_iosize[core_array[i].core_type]
                                  = core_array[i].iosize;
    if (subsys[core_array[i].subsys].irq != -1 && core_array[i].irq != -1) {
      if (subsys[core_array[i].subsys].irq != core_array[i].irq) {
        printk(KERN_INFO "hantrodec: hw core type %d irq %d != subsystem irq %d\n",
                          core_array[i].core_type,
                          core_array[i].irq,
                          subsys[core_array[i].subsys].irq);
        printk(KERN_INFO "hantrodec: hw cores of a subsystem should have same irq\n");
      } else {
        subsys[core_array[i].subsys].irq = core_array[i].irq;
      }
    }
    /* vcmd found */
    if (core_array[i].core_type == HW_VCMD) {
      *vcmd = 1;
      total_vcmd_core_num++;
    }
  }

  printk(KERN_INFO "hantrodec: vcmd = %d\n", *vcmd);

  /* To plug into hantro_vcmd.c */
  if (*vcmd) {
    for (i = 0; i < total_vcmd_core_num; i++) {
      vcmd_core_array[i].vcmd_base_addr = subsys[i].base_addr;
      vcmd_core_array[i].vcmd_iosize = subsys[i].submodule_iosize[HW_VCMD];
      vcmd_core_array[i].vcmd_irq = subsys[i].irq;
      vcmd_core_array[i].sub_module_type = 2; /* TODO(min): to be fixed */
      vcmd_core_array[i].submodule_main_addr = subsys[i].submodule_offset[HW_VC8000D];
      vcmd_core_array[i].submodule_dec400_addr = subsys[i].submodule_offset[HW_DEC400];
      vcmd_core_array[i].submodule_L2Cache_addr = subsys[i].submodule_offset[HW_L2CACHE];
      vcmd_core_array[i].submodule_MMU_addr = subsys[i].submodule_offset[HW_MMU];
    }
  } else {
    for (i = 0; i < num; i++) {
      multicorebase[i] = subsys[i].base_addr + subsys[i].submodule_offset[HW_VC8000D];
      irq[i] = subsys[i].irq;
      iosize[i] = subsys[i].submodule_iosize[HW_VC8000D];
      printk(KERN_INFO "hantrodec: [%d] multicorebase 0x%08lx, iosize %d\n", i, multicorebase[i], iosize[i]);
    }
  }
}


