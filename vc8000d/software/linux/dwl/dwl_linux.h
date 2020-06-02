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
#ifndef _DWL_LINUX_H_
#define _DWL_LINUX_H_

#include "basetype.h"
#include "dwl_defs.h"
#include "dwl.h"
#include "dwl_activity_trace.h"
#include "ppu.h"
#include "sw_performance.h"
#include "hantrovcmd.h"
#ifdef SUPPORT_CACHE
#include "dwl_linux_cache.h"
#endif
#ifdef __linux__
#include "memalloc.h"
#elif defined(__FREERTOS__)
#else //For other os
//TODO...
#endif
#include "deccfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>
#endif
#ifndef DEC_MODULE_PATH
#define DEC_MODULE_PATH "/tmp/dev/hantrodec"
#endif

#ifndef MEMALLOC_MODULE_PATH
#define MEMALLOC_MODULE_PATH "/tmp/dev/memalloc"
#endif

#define HANTRODECPP_REG_START 0x400
#define HANTRODEC_REG_START 0x4

#define HANTRODECPP_FUSE_CFG_G1 99
#define HANTRODECPP_FUSE_CFG_G2 99
#define HANTRODECPP_FUSE_CFG 61
#define HANTRODEC_FUSE_CFG 57

#define OFFSET_NOT_EXIST  0xFFFF  /* offset indicating submodule does not exist */

#define DWL_DECODER_INT \
  ((DWLReadReg(dec_dwl, HANTRODEC_REG_START) >> 11) & 0xFFU)
#define DWL_PP_INT      ((DWLReadReg(dec_dwl, HANTRODECPP_REG_START) >> 11) & 0xFFU)

#define IS_DECMODE_JPEG(swreg3) (((swreg3)>>27) == 3)
#define IS_PJPEG(swreg3) ((((swreg3)>>27) == 3) && (((swreg3)>>24) & 0x1))
#define IS_PJPEG_LAST_SCAN(swreg3) (IS_PJPEG(swreg3) && (((swreg3)>>23) & 0x1))
#define IS_PJPEG_INTER_SCAN(swreg3) (IS_PJPEG(swreg3) && !(((swreg3)>>23) & 0x1))
#define IS_PP0_RGB(swreg322) ((((swreg322)>>18) & 0x1F) == 11)


#define DEC_IRQ_ABORT (1 << 11)
#define DEC_IRQ_RDY (1 << 12)
#define DEC_IRQ_BUS (1 << 13)
#define DEC_IRQ_BUFFER (1 << 14)
#define DEC_IRQ_ASO (1 << 15)
#define DEC_IRQ_ERROR (1 << 16)
#define DEC_IRQ_SLICE (1 << 17)
#define DEC_IRQ_TIMEOUT (1 << 18)
#define DEC_IRQ_LAST_SLICE_INT (1 << 19)
#define DEC_IRQ_NO_SLICE_INT (1 << 20)
#define DEC_IRQ_EXT_TIMEOUT (1 << 21)
#define DEC_IRQ_SCAN_RDY (1 << 25)

#define DEC_HW_IRQ_BUFFER 0x08
#define DEC_HW_IRQ_EXT_TIMEOUT 0x400

#define PP_IRQ_RDY             (1 << 12)
#define PP_IRQ_BUS             (1 << 13)
#define DEC_ABORT              0x20
#define DEC_ENABLE             0x01
#define DWL_HW_ENABLE_BIT 0x000001 /* 0th bit */

#ifdef _DWL_HW_PERFORMANCE
/* signal that decoder/pp is enabled */
void DwlDecoderEnable(void);
#endif

struct VcmdBuf {
  u32 fd;       /* file descriptor */
  u32 valid;
  u32 client_type;
  u32 wait_polling_break;   /* a flag indicating that the vcmd interrupt generated. */
  u8 *cmd_buf;  /* pointer to cmd buffer */
  u32 cmd_buf_size;   /* cmd buffer bytes allocated */
  u32 cmd_buf_used;   /* bytes used in current buffer */
  u8 *status_buf;     /* pointer to status buffer */
  addr_t status_bus_addr;  /* status offset to VCMD base */
  u32 status;         /* DEC HW status (swreg1) after decoding */
  addr_t stream_base; /* DEC stream base (swreg168/169) after decoding */
  u16 core_id;  /* core allocated for this vcmd. */
};

struct MCListenerThreadParams {
  int fd;
  int b_stopped;
  unsigned int n_dec_cores;
  sem_t sc_dec_rdy_sem[MAX_MC_CB_ENTRIES];
  DWLIRQCallbackFn *callback[MAX_MC_CB_ENTRIES];
  void *callback_arg[MAX_MC_CB_ENTRIES];
  struct ActivityTrace *activity;
  struct VcmdBuf *vcmd;  /* Pointer to dwl->vcmd. Used in mc callbacks. */
  struct config_parameter *vcmd_params;
};

/* wrapper information */
struct HANTRODWL {
  u32 client_type;
  int fd;          /* decoder/vcmd device file */
  int fd_mem;      /* /dev/mem for mapping */
  int fd_memalloc; /* linear memory allocator */
  int vcmd_used;   /* vcmd used instead of decoder*/
  u32 num_cores;
  u32 reg_size;         /* IO mem size */
  addr_t free_lin_mem;     /* Start address of free linear memory */
  addr_t free_ref_frm_mem; /* Start address of free reference frame memory */
  int semid;
  struct MCListenerThreadParams *sync_params;
  struct ActivityTrace activity;
  u32 b_ppreserved;
  pthread_mutex_t shadow_mutex;
  PpUnitIntConfig ppu_cfg[DEC_MAX_PPU_COUNT];
  u32 tile_id;
  u32 num_tiles;
  u32 shaper_enable[MAX_ASIC_CORES];
  struct CacheParams cache_cfg[MAX_ASIC_CORES];
  u64 buf_size[MAX_ASIC_CORES];
  u16 *tiles;
  u32 tile_by_tile;
  u32 pjpeg_coeff_buffer_size;
  pthread_mutex_t shaper_mutex;
  /* counters for core usage statistics */
  u32 core_usage_counts[MAX_ASIC_CORES];

  /* Submodule address/command buffer parameters when VCMD is used. */
  struct config_parameter vcmd_params;
  struct cmdbuf_mem_parameter vcmd_mem_params;
  pthread_mutex_t vcmd_mutex;
  struct VcmdBuf vcmdb[MAX_VCMD_ENTRIES];
};

i32 DWLWaitDecHwReady(const void *instance, i32 core_id, u32 timeout);
u32 *DWLMapRegisters(int mem_dev, /*unsigned long*/ addr_t base, unsigned int reg_size, u32 write);
void DWLUnmapRegisters(const void *io, unsigned int reg_size);

#endif /* _DWL_LINUX_H_ */
