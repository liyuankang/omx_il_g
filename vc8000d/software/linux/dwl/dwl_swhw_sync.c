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

#include "basetype.h"
#include "dwl_hw_core_array.h"
#include "dwl_swhw_sync.h"
#include "decapicommon.h"
#include "deccfg.h"
#include "vwl_pc.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__GNUC__)
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif

#ifdef ASIC_ONL_SIM
u8 l2_allocate_buf=1;
#ifdef SUPPORT_MULTI_CORE
#include <vpu_dwl_dpi.h>
extern struct TBCfg tb_cfg;
int dwl_g_frame_num =0; /*count decoded frame number*/
extern struct McListenerThreadParams listener_thread_params;
extern u32* core_reg_base_array[2];
extern int cur_core_id;
extern int start_mc_load;
extern int mc_load_end;
extern int cur_pic;
extern int finished_pic;
extern u32 max_pics_decode;
#endif
#endif

extern u32 dwl_shadow_regs[MAX_ASIC_CORES][512];
#ifdef SUPPORT_CACHE
#ifdef ASIC_ONL_SIM
extern u32 L2_DEC400_configure_done;
#endif
#endif


#ifdef _DWL_DEBUG
void PrintIrqType(u32 core_id, u32 status);
#endif

extern HwCoreArray g_hw_core_array;

void *ThreadMcListener(void *args) {
  struct McListenerThreadParams *params = (struct McListenerThreadParams *)args;
#ifdef SUPPORT_VCMD
  u16 cmdbuf_id;
#endif
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  int* hw_core_rdy_id;
  hw_core_rdy_id = malloc(sizeof(int));
  int sw_core_rdy[2];
#endif
#endif

  while (!params->b_stopped) {
    u32 ret;
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
    u32 j;
    if (cur_pic < params->n_dec_cores) {
      for (j = 0; j < params->n_dec_cores; j++) {
        while(!start_mc_load){
          usleep(100);
        }
        start_mc_load = 0;
        if(l2_allocate_buf == 1){
          dpi_l2_update_buf_base(core_reg_base_array[cur_core_id]);
          l2_allocate_buf=0;
        }
        dwl_g_frame_num++;
        dpi_mc_stimulus_load(core_reg_base_array[cur_core_id],dwl_g_frame_num,cur_core_id);
        mc_load_end = 1;
      }
    }
    else if ( cur_pic < max_pics_decode){
      while(!start_mc_load){
        usleep(100);
      }
      start_mc_load = 0;
      if(l2_allocate_buf == 1){
        dpi_l2_update_buf_base(core_reg_base_array[cur_core_id]);
        l2_allocate_buf=0;
      }
      dwl_g_frame_num++;
      dpi_mc_stimulus_load(core_reg_base_array[cur_core_id],dwl_g_frame_num,cur_core_id);
      mc_load_end = 1;
    }
#endif
#endif

#ifdef SUPPORT_CACHE
#ifdef ASIC_ONL_SIM
    if ( cur_pic > finished_pic || (params->n_dec_cores == 1 && cur_pic < max_pics_decode)){
      printf("DEBUG before l2_dec400 wait\n");
      while(!L2_DEC400_configure_done){
          usleep(50);
      }
      L2_DEC400_configure_done = 0;
    }
#endif
#endif

#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
    if ( cur_pic > finished_pic || (params->n_dec_cores == 1 && cur_pic < max_pics_decode)){
      dpi_wait_mc_int(hw_core_rdy_id);
      finished_pic += 1;
      Core c = GetCoreById(g_hw_core_array, *hw_core_rdy_id);
      while(!ONL_HwCoreIsDecRdy(c)) {
        usleep(100);
      }
    }
#endif
#endif

#ifndef SUPPORT_VCMD
    ret = WaitAnyCoreRdy(g_hw_core_array);
#else
    cmdbuf_id = ANY_CMDBUF_ID;
    ret = CmodelIoctlWaitCmdbuf(&cmdbuf_id);
#endif
    (void)ret;

    if (params->b_stopped) break;

#ifndef SUPPORT_VCMD
    u32 i;
    /* check all decoder IRQ status register */
    for (i = 0; i < params->n_dec_cores; i++) {
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
      i = *hw_core_rdy_id;
#endif
#endif
      Core c = GetCoreById(g_hw_core_array, i);
#ifdef _DWL_DEBUG
      u32* regs = HwCoreGetBaseAddress(c);
#endif
      /* check DEC IRQ status */
      if (HwCoreIsDecRdy(c)) {
#ifdef _DWL_DEBUG
        PrintIrqType(i, regs[1]);
#endif
        DWL_DEBUG("DEC IRQ by Core %d\n", i);
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
        Core c = GetCoreById(g_hw_core_array, i);
        u32 *core_reg_base = HwCoreGetBaseAddress(c);

        dpi_mc_output_check(core_reg_base,i);
#endif
#endif

        if (params->callback[i] != NULL) {
          u32 *core_reg_base = HwCoreGetBaseAddress(c);
          /* Refresh dwl_shadow_regs from hw core registers. */
          for(int k = DEC_X170_REGISTERS; k >= 0; --k) {
            dwl_shadow_regs[i][k] = core_reg_base[k];
          }
          params->callback[i](params->callback_arg[i], i);
        } else {
          /* single core instance => post dec ready */
          HwCorePostDecRdy(c);
        }
        break;
      }
    }
#else
    DWL_DEBUG("VCMD IRQ by cmd buf %d\n", cmdbuf_id);
    u32 *status = (u32 *)(params->vcmd[cmdbuf_id].status_buf +
                          params->vcmd_params->submodule_main_addr);
    DWLWriteReg(NULL, 1, 0 * 4, *status++);
    /* VCMD multicore decoding: updated registers to dwl_shadow_regs[1]. */
    DWLWriteReg(NULL, 1, 1 * 4, *status++);
    DWLWriteReg(NULL, 1, 168 * 4, *status++);
    DWLWriteReg(NULL, 1, 169 * 4, *status++);
    DWLWriteReg(NULL, 1, 63 * 4, *status++);
    DWLWriteReg(NULL, 1, 62 * 4, *status++);

    if (params->callback[cmdbuf_id] != NULL) {
      params->callback[cmdbuf_id](params->callback_arg[cmdbuf_id], cmdbuf_id);
    }
    sem_post(params->sc_dec_rdy_sem + cmdbuf_id);
#endif
  }
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  free(hw_core_rdy_id);
#endif
#endif

  return NULL;
}
