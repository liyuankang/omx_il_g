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

#ifndef SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_
#define SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_

#include "dwlthread.h"
#include "dwl.h"
#include "deccfg.h"
#include "vwl_pc.h"

struct VcmdBuf {
  u32 valid;
  u32 core_id;  /* core allocated for this vcmd. */
  u32 client_type;
  u8 *cmd_buf;  /* pointer to cmd buffer */
  u8 *status_buf;
  addr_t status_bus_addr;  /* status offset to VCMD base */
  u32 cmd_buf_size;   /* cmd buffer bytes allocated */
  u32 cmd_buf_used;   /* bytes used in current buffer */
  u32 status;         /* DEC HW status (swreg1) after decoding */
  addr_t stream_base; /* DEC stream base (swreg168/169) after decoding */
};

struct McListenerThreadParams {
  int fd;
  int b_stopped;
  unsigned int n_dec_cores;
  sem_t sc_dec_rdy_sem[MAX_MC_CB_ENTRIES];
  volatile u32 *reg_base[MAX_ASIC_CORES];
  DWLIRQCallbackFn *callback[MAX_MC_CB_ENTRIES];
  void *callback_arg[MAX_MC_CB_ENTRIES];
  struct VcmdBuf *vcmd;  /* Pointer to dwl->vcmd. Used in mc callbacks. */
  struct config_parameter *vcmd_params;
};

void *ThreadMcListener(void *args);

#endif /* SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_ */
