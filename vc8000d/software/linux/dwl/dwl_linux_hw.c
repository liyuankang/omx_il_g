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
#include "decapicommon.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"
#include "hantrovcmd.h"
#include "dwl_vcmd_common.h"
#ifdef __linux__
#include "hantrodec.h"
#include "memalloc.h"
#elif defined(__FREERTOS__)
#include "hantrodec_freertos.h"
#include "memalloc_freertos.h"
#else //For other os
//TODO...
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef SUPPORT_CACHE
#include "dwl_linux_cache.h"
#endif
#ifdef SUPPORT_DEC400
#include "dwl_linux_dec400.h"
#endif
#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define DEC_MODE_JPEG 3
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2 16

u32 instances = 0;

#ifdef SUPPORT_CACHE
extern u32 cache_enable[MAX_ASIC_CORES];
#endif
/* the decoder device driver nod */
const char *dec_dev = DEC_MODULE_PATH;

/* the memalloc device driver nod */
const char *mem_dev = MEMALLOC_MODULE_PATH;

/* a mutex protecting the wrapper init */
static pthread_mutex_t x170_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int n_dwl_instance_count = 0;

static struct MCListenerThreadParams listener_thread_params = {0};
static pthread_t mc_listener_thread;
#if (defined(SUPPORT_VCMD) || defined(DWL_USE_DEC_IRQ)) && !defined(VCMD_REAL_INT)
static pthread_t vcmd_polling_thread;
#endif

extern u32 dwl_shadow_regs[MAX_ASIC_CORES][512];
extern u32 hw_enable[MAX_ASIC_CORES];
extern u32 cache_version;

#ifdef SUPPORT_CACHE

#define SHAPER_REG_OFFSET 0x8
#define CACHE_REG_OFFSET 0x80

u32 cache_shadow_regs[512];
u32 shaper_shadow_regs[128];
extern u32 cache_exception_lists[];
extern u32 cache_exception_regs_num;

#endif

/* 8KB for each command buffer */
/* FIXME(min): find a more flexible approach. */
#define VCMD_BUF_SIZE (8*1024)

#ifdef PERFORMANCE_TEST
extern u32 hw_time_use;
#endif

#ifdef _DWL_DEBUG
void PrintIrqType(u32 core_id, u32 status);
#endif

void *ThreadMCListener(void *args) {
  struct MCListenerThreadParams *params = (struct MCListenerThreadParams *)args;
#ifndef DWL_USE_DEC_IRQ
  u32 count = 0;
#endif
#if !defined(SUPPORT_VCMD) || !defined(DWL_USE_DEC_IRQ)
  u32 irq_stats;
  u32 id;
  struct core_desc core;
#endif

  while (!params->b_stopped) {

#ifdef DWL_USE_DEC_IRQ

#ifndef SUPPORT_VCMD
    DWL_DEBUG("%s", "ioctl wait for interrupt\n");
    if (ioctl(params->fd, HANTRODEC_IOCG_CORE_WAIT, &id)) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCG_CORE_WAIT failed\n");
      continue;
    }
#else
    u16 cmdbuf_id = ANY_CMDBUF_ID;
    if (ioctl(params->fd, HANTRO_VCMD_IOCH_WAIT_CMDBUF, &cmdbuf_id)) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_VCMD_IOCH_WAIT_CMDBUF failed\n");
      continue;
    }
#endif

    if (params->b_stopped) break;

#ifndef SUPPORT_VCMD
    DWL_DEBUG("DEC IRQ by Core %d\n", id);

    core.id = id;
    core.regs = &dwl_shadow_regs[id][1];
    core.size = 1 * 4;
    core.reg_id = 1;
    core.type = HW_VC8000D;

    if (ioctl(params->fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
    }

    irq_stats = dwl_shadow_regs[id][1];

#ifdef _DWL_DEBUG
    PrintIrqType(id, irq_stats);
#endif

    /* If DEC_IRQ_EXT_TIMEOUT is reported or SW timeout is triggered, ABORT HW.*/
    if ((irq_stats & DEC_IRQ_EXT_TIMEOUT) ||
       (((irq_stats & DEC_ENABLE) && ((irq_stats >> 11) & 0xFF) == 0))) {
      dwl_shadow_regs[id][1] = dwl_shadow_regs[id][1] | DEC_ABORT;
      core.regs = &dwl_shadow_regs[id][1];
      if (ioctl(params->fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_WRITE_REG failed\n");
      }

      if (ioctl(params->fd, HANTRODEC_IOCG_CORE_WAIT, &id)) {
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCG_CORE_WAIT failed\n");
      }
    }

    PERFORMANCE_STATIC_START(decode_pull_reg);
    core.regs = dwl_shadow_regs[id];
    core.size = 256 * 4;
    if (ioctl(params->fd, HANTRODEC_IOCS_DEC_PULL_REG, &core)) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_DEC_PULL_REG failed\n");
      ASSERT(0);
    }
    PERFORMANCE_STATIC_END(decode_pull_reg);
    DWL_DEBUG("DEC IRQ by Core %d\n", id);
    if (params->callback[id] != NULL) {
      params->callback[id](params->callback_arg[id], id);
    }
    sem_post(params->sc_dec_rdy_sem + id);
#else
    if (!cmdbuf_id) continue;

    DWL_DEBUG("VCMD IRQ by cmd buf %d\n", cmdbuf_id);
    u32 *status = (u32 *)(params->vcmd[cmdbuf_id].status_buf +
                          params->vcmd_params->submodule_main_addr/2);

    /* VCMD multicore decoding: updated registers to dwl_shadow_regs[1]. */
    dwl_shadow_regs[1][0] = *status++;
    dwl_shadow_regs[1][1] = *status++;
    dwl_shadow_regs[1][168] = *status++;
    dwl_shadow_regs[1][169] = *status++;
    dwl_shadow_regs[1][62] = *status++;
    dwl_shadow_regs[1][63] = *status++;

    if (params->callback[cmdbuf_id] != NULL) {
      params->callback[cmdbuf_id](params->callback_arg[cmdbuf_id], cmdbuf_id);
    }
    sem_post(params->sc_dec_rdy_sem + cmdbuf_id);
#endif

#else
    core.size = 2 * 4;

    for (id = 0; id < params->n_dec_cores; id++) {
      const unsigned int usec = 1000; /* 1 ms polling interval */

      /* Skip cores that are not part of multicore, they call directly
       * DWLWaitHwReady(), which does its own polling.
       */
      if (params->callback[id] == NULL) {
        continue;
      }

#if 0
      /* Skip not enabled cores also */
      if ((dwl_shadow_regs[id][1] & 0x01) == 0) {
        continue;
      }
#endif
      if (!hw_enable[id]) {
        continue;
      }


      core.id = id;
      core.regs = &dwl_shadow_regs[id][1];
      core.reg_id = 1;
      core.size = 4;

      if (ioctl(params->fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
        continue;
      }

      irq_stats = dwl_shadow_regs[id][1];

      /* If DEC_IRQ_EXT_TIMEOUT is reported or SW timeout is triggered, ABORT HW.*/
      if ((irq_stats & DEC_IRQ_EXT_TIMEOUT) ||
         (((irq_stats & DEC_ENABLE) && ((irq_stats >> 11) & 0xFF) == 0)
           && count++ >= 100000)) {
        dwl_shadow_regs[id][1] = dwl_shadow_regs[id][1] | DEC_ABORT;
        if (ioctl(params->fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
          DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_WRITE_REG failed\n");
          ASSERT(0);
        }

        if (ioctl(params->fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
          DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
          ASSERT(0);
        }
        irq_stats = dwl_shadow_regs[id][1];
      }

      irq_stats = (irq_stats >> 11) & 0xFF;

      if (irq_stats != 0) {

#ifdef PERFORMANCE_TEST
        u32 i;
        for (i = 0; i < MAX_ASIC_CORES; i++) {
          if (dwl_shadow_regs[i][1] & 0x01)
            break;
        }

        if (i == MAX_ASIC_CORES) {
          ActivityTraceStopDec(params->activity);
          hw_time_use = params->activity->active_time / 100;
        }
#endif
        hw_enable[id] = 0;
        count = 0;

        if (ioctl(params->fd, HANTRODEC_IOCS_DEC_PULL_REG, &core)) {
          DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PULL_REG failed\n");
          continue;
        }

#ifdef _DWL_DEBUG
        {
          u32 irq_stats = dwl_shadow_regs[id][HANTRODEC_IRQ_STAT_DEC];

          PrintIrqType(id, irq_stats);
        }
#endif

        DWL_DEBUG("DEC IRQ by Core %d\n", id);
        params->callback[id](params->callback_arg[id], id);
      }

      usleep(usec);
    }
#endif
  }

  return NULL;
}

#if (defined(SUPPORT_VCMD) || defined(DWL_USE_DEC_IRQ)) && !defined(VCMD_REAL_INT)
/* Use polling command to simulate interrupt mode. */
void* VcmdInterruptSimWithPoll(void *inst)
{
  struct MCListenerThreadParams *params = (struct MCListenerThreadParams *)inst;
  u16 dummy = 0;

  while (!params->b_stopped) {
    if (params->vcmd_params)
      /* VCMD used */
      ioctl(params->fd, HANTRO_VCMD_IOCH_POLLING_CMDBUF, &dummy);
    else
      /* Non-VCMD */
      ioctl(params->fd, HANTRODEC_IOCX_POLL, &dummy);

    if (params->b_stopped) break;
    usleep(10 * 1000); //10ms
    dummy++;
    if (params->vcmd_params && dummy >= params->vcmd_params->vcmd_core_num) dummy = 0;
  }
  return NULL;
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : void * param - not in use, application passes NULL
------------------------------------------------------------------------------*/
const void *DWLInit(struct DWLInitParam *param) {
  struct HANTRODWL *dec_dwl;
  unsigned long multicore_base[MAX_ASIC_CORES];
  unsigned int i;
  struct subsys_desc subsys;

  DWL_DEBUG("%s","DWLInit INITIALIZE\n");

  dec_dwl = (struct HANTRODWL *)calloc(1, sizeof(struct HANTRODWL));

  if (dec_dwl == NULL) {
    DWL_DEBUG("%s","failed to alloc struct HANTRODWL struct\n");
    return NULL;
  }
  memset(dec_dwl, 0, sizeof(struct HANTRODWL));

  dec_dwl->client_type = param->client_type;

  if(dec_dwl->client_type != DWL_CLIENT_TYPE_PP)
    pthread_mutex_init(&dec_dwl->shadow_mutex, NULL);

  pthread_mutex_lock(&x170_init_mutex);
#ifdef INTERNAL_TEST
  InternalTestInit();
#endif

  dec_dwl->fd = -1;
  dec_dwl->fd_mem = -1;
  dec_dwl->fd_memalloc = -1;
  dec_dwl->vcmd_used = 1;

  /* open the device */
  /* Check whether vcmd driver supported firstly. */
  dec_dwl->fd = open(dec_dev, O_RDWR);
  if (dec_dwl->fd == -1) {
    DWL_DEBUG("failed to open '%s'\n", dec_dev);
    goto err;
  }
  if (ioctl(dec_dwl->fd, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto err;
  }
  if (!subsys.subsys_vcmd_num) {
    dec_dwl->vcmd_used = 0;
  }

  /* Linear memories not needed in pp */
  if (dec_dwl->client_type != DWL_CLIENT_TYPE_PP) {
    /* open memalloc for linear memory allocation */
    dec_dwl->fd_memalloc = open(mem_dev, O_RDWR | O_SYNC);

    if (dec_dwl->fd_memalloc == -1) {
      DWL_DEBUG("failed to open: %s\n", mem_dev);
      goto err;
    }
  }

  dec_dwl->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);

  if (dec_dwl->fd_mem == -1) {
    DWL_DEBUG("failed to open: %s\n", "/dev/mem");
    goto err;
  }

  switch (dec_dwl->client_type) {
  case DWL_CLIENT_TYPE_H264_DEC:
  case DWL_CLIENT_TYPE_MPEG4_DEC:
  case DWL_CLIENT_TYPE_JPEG_DEC:
  case DWL_CLIENT_TYPE_VC1_DEC:
  case DWL_CLIENT_TYPE_MPEG2_DEC:
  case DWL_CLIENT_TYPE_VP6_DEC:
  case DWL_CLIENT_TYPE_VP8_DEC:
  case DWL_CLIENT_TYPE_RV_DEC:
  case DWL_CLIENT_TYPE_AVS_DEC:
  case DWL_CLIENT_TYPE_HEVC_DEC:
  case DWL_CLIENT_TYPE_VP9_DEC:
  case DWL_CLIENT_TYPE_AVS2_DEC:
  case DWL_CLIENT_TYPE_ST_PP: {
    break;
  }
  default: {
    DWL_DEBUG("Unknown client type no. %d\n", dec_dwl->client_type);
    goto err;
  }
  }

  if (dec_dwl->vcmd_used) {
    pthread_mutex_init(&dec_dwl->vcmd_mutex, NULL);

    /* Get VCMD configuration. */
    dec_dwl->vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(dec_dwl->fd, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER,
              &dec_dwl->vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto err;
    }
    dec_dwl->num_cores = dec_dwl->vcmd_params.vcmd_core_num;
    dec_dwl->reg_size = 512 * 4;

    if (ioctl(dec_dwl->fd, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
              &dec_dwl->vcmd_mem_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto err;
    }

    for (i = 0; i < MAX_VCMD_ENTRIES; i++) {
      dec_dwl->vcmdb[i].valid = 1;
    }
    dec_dwl->vcmdb[dec_dwl->vcmd_params.config_status_cmdbuf_id].valid = 0;

    dec_dwl->vcmd_mem_params.virt_cmdbuf_addr =
      (u32 *) mmap(0, dec_dwl->vcmd_mem_params.cmdbuf_total_size, PROT_READ|PROT_WRITE,
                   MAP_SHARED, dec_dwl->fd_mem,
                   dec_dwl->vcmd_mem_params.phy_cmdbuf_addr);
    dec_dwl->vcmd_mem_params.virt_status_cmdbuf_addr =
      (u32 *) mmap(0, dec_dwl->vcmd_mem_params.status_cmdbuf_total_size, PROT_READ|PROT_WRITE,
                   MAP_SHARED, dec_dwl->fd_mem,
                   dec_dwl->vcmd_mem_params.phy_status_cmdbuf_addr);

#ifdef SUPPORT_CACHE
    {
    u32 *reg;
    reg = dec_dwl->vcmd_mem_params.virt_status_cmdbuf_addr +
            dec_dwl->vcmd_params.config_status_cmdbuf_id * dec_dwl->vcmd_mem_params.status_cmdbuf_unit_size / 4 +
            dec_dwl->vcmd_params.submodule_L2Cache_addr/4/2;

    cache_version = (reg[0] & 0xF000) >> 12;
    }
#endif
  } else {
    /* TODO(min): these command aren't supported in VCMD driver yet. */
    if (ioctl(dec_dwl->fd, HANTRODEC_IOC_MC_CORES, &dec_dwl->num_cores) == -1) {
      DWL_DEBUG("%s","ioctl HANTRODEC_IOC_MC_CORES failed\n");
      goto err;
    }

    ASSERT(dec_dwl->num_cores <= MAX_ASIC_CORES);

    if (ioctl(dec_dwl->fd, HANTRODEC_IOC_MC_OFFSETS, multicore_base) == -1) {
      DWL_DEBUG("%s","ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
      goto err;
    }

    if (ioctl(dec_dwl->fd, HANTRODEC_IOCGHWIOSIZE, &dec_dwl->reg_size) == -1) {
      DWL_DEBUG("%s","ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      goto err;
    }
  }

  /* Allocate the signal handling and cores just once */
  if (n_dwl_instance_count++ == 0) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    listener_thread_params.fd = dec_dwl->fd;
    listener_thread_params.n_dec_cores = dec_dwl->num_cores;

    for (i = 0; i < listener_thread_params.n_dec_cores; i++) {
      sem_init(listener_thread_params.sc_dec_rdy_sem + i, 0, 0);

      listener_thread_params.callback[i] = NULL;
    }

    listener_thread_params.b_stopped = 0;
#ifdef PERFORMANCE_TEST
    listener_thread_params.activity = &dec_dwl->activity;
#endif

    if (dec_dwl->vcmd_used) {
      listener_thread_params.vcmd = dec_dwl->vcmdb;
      listener_thread_params.vcmd_params = &dec_dwl->vcmd_params;
    }

    if (pthread_create(&mc_listener_thread, &attr, ThreadMCListener,
                       &listener_thread_params) != 0)
      goto err;

#if (defined(SUPPORT_VCMD) || defined(DWL_USE_DEC_IRQ)) && !defined(VCMD_REAL_INT)
    if (pthread_create(&vcmd_polling_thread, &attr, VcmdInterruptSimWithPoll,
                       &listener_thread_params) != 0)
      goto err;
#endif
  }

#ifdef PERFORMANCE_TEST
  ActivityTraceInit(&dec_dwl->activity);
#endif
  dec_dwl->sync_params = &listener_thread_params;

  DWL_DEBUG("%s","DWLInit SUCCESS\n");

  pthread_mutex_unlock(&x170_init_mutex);
#ifdef SUPPORT_CACHE
  pthread_mutex_init(&dec_dwl->shaper_mutex, NULL);
#endif

#ifdef SUPPORT_MMU
  unsigned int enable = 1;
  int ioctl_req;
  ioctl_req = (int)HANTRO_IOCS_MMU_ENABLE;
  if (ioctl(dec_dwl->fd, ioctl_req, &enable)) {
    DWL_DEBUG("%s","ioctl HANTRO_IOCS_MMU_ENABLE failed\n");
    ASSERT(0);
  }
#endif

#ifdef SUPPORT_CACHE
  extern u32 vcmd_used;
  vcmd_used = dec_dwl->vcmd_used;
#endif

  return dec_dwl;

err:

  DWL_DEBUG("%s","FAILED\n");
  pthread_mutex_unlock(&x170_init_mutex);
  DWLRelease(dec_dwl);

  return NULL;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
i32 DWLRelease(const void *instance) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  unsigned int i = 0;

  DWL_DEBUG("%s","DWLRelease RELEASE\n");

  if (dec_dwl == NULL) return DWL_OK;

  if(dec_dwl->client_type != DWL_CLIENT_TYPE_PP)
    pthread_mutex_destroy(&dec_dwl->shadow_mutex);

  PERFORMANCE_STATIC_REPORT(decode_push_reg);
  PERFORMANCE_STATIC_REPORT(decode_pull_reg);

  pthread_mutex_lock(&x170_init_mutex);

  n_dwl_instance_count--;

  /* Release the signal handling and cores just when
   * nobody is referencing them anymore
   */
  if(!dec_dwl->vcmd_used) {
    if (!n_dwl_instance_count) {
      listener_thread_params.b_stopped = 1;
    }

    for (i = 0; i < dec_dwl->num_cores; i++) {
      sem_destroy(listener_thread_params.sc_dec_rdy_sem + i);
    }
  }

  if (dec_dwl->fd_mem != -1) close(dec_dwl->fd_mem);

  if (dec_dwl->fd != -1) close(dec_dwl->fd);

  /* linear memory allocator */
  if (dec_dwl->fd_memalloc != -1) close(dec_dwl->fd_memalloc);

#ifdef SUPPORT_CACHE
  pthread_mutex_destroy(&dec_dwl->shaper_mutex);
#endif
  /* print core usage stats */
  if (dec_dwl->client_type != DWL_CLIENT_TYPE_PP) {
    u32 total_usage = 0;
    u32 cores = dec_dwl->num_cores;
    for (i = 0; i < cores; i++) {
      total_usage += dec_dwl->core_usage_counts[i];
    }
    /* avoid zero division */
    total_usage = total_usage ? total_usage : 1;

    printf("\nMulti-core usage statistics:\n");
    for (i = 0; i < cores; i++)
      printf("\tCore[%2d] used %6d times (%2d%%)\n", i,
             dec_dwl->core_usage_counts[i],
             (dec_dwl->core_usage_counts[i] * 100) / total_usage);

    printf("\n");
  }

#ifdef INTERNAL_TEST
  InternalTestFinalize();
#endif

  if (dec_dwl->vcmd_used) {
    if (dec_dwl->vcmd_mem_params.virt_cmdbuf_addr != MAP_FAILED)
      munmap(dec_dwl->vcmd_mem_params.virt_cmdbuf_addr,
             dec_dwl->vcmd_mem_params.cmdbuf_total_size);
    if (dec_dwl->vcmd_mem_params.virt_status_cmdbuf_addr != MAP_FAILED)
      munmap(dec_dwl->vcmd_mem_params.virt_cmdbuf_addr,
             dec_dwl->vcmd_mem_params.status_cmdbuf_total_size);
  }

#ifdef PERFORMANCE_TEST
  ActivityTraceRelease(&dec_dwl->activity);
#endif
  free(dec_dwl);

  pthread_mutex_unlock(&x170_init_mutex);

  DWL_DEBUG("%s","DWLRelease SUCCESS\n");

  return (DWL_OK);
}

/* HW locking */

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHwPipe
    Description     :
    Return type     : i32
    Argument        : const void *instance
    Argument        : i32 *core_id - ID of the reserved HW core
------------------------------------------------------------------------------*/
i32 DWLReserveHwPipe(const void *instance, i32 *core_id) {

  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  ASSERT(dec_dwl != NULL);
  ASSERT(dec_dwl->client_type != DWL_CLIENT_TYPE_PP);

  DWL_DEBUG("%s","Start\n");

  /* reserve decoder */
  *core_id =
    ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE, dec_dwl->client_type);

  if (*core_id != 0) {
    return DWL_ERROR;
  }

  DWL_DEBUG("Reserved DEC core %d\n", *core_id);

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
    Argument        : i32 *core_id - ID of the reserved HW core
------------------------------------------------------------------------------*/
i32 DWLReserveHw(const void *instance, i32 *core_id, u32 client_type) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  ASSERT(dec_dwl != NULL);

  DWL_DEBUG(" %s\n", "DEC");

  *core_id =
    ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE, client_type);

  /* negative value signals an error */
  if (*core_id < 0) {
    DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_RESERVE failed, %d\n",
              "DEC", *core_id);
    return DWL_ERROR;
  }

  DWL_DEBUG("Reserved %s core %d\n", "DEC", *core_id);

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
u32 DWLReleaseHw(const void *instance, i32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  ASSERT((u32)core_id < dec_dwl->num_cores);
  ASSERT(dec_dwl != NULL);
  u32 ret = DWL_CACHE_OK;
#ifdef SUPPORT_CACHE
  i32 irq = 0;
  u32 asic_status;
  asic_status = (DWLReadReg(instance, core_id, 4 * 1) >> 11) & 0x1FFF;
#endif
  if ((u32)core_id >= dec_dwl->num_cores) {
    ASSERT(0);
    return DWL_CACHE_OK;
  }

  DWL_DEBUG(" %s core %d\n", "DEC", core_id);

  ioctl(dec_dwl->fd, HANTRODEC_IOCT_DEC_RELEASE, core_id);
#ifdef SUPPORT_CACHE
  pthread_mutex_lock(&dec_dwl->shaper_mutex);
  dec_dwl->tile_id = 0;
  irq = DWLDisableCacheChannelALL(BI, core_id);
  dec_dwl->shaper_enable[core_id] = 0;
  cache_enable[core_id] = 0;
  if ((asic_status & DEC_HW_IRQ_EXT_TIMEOUT) && (irq == CACHE_HW_FETAL_RECOVERY))
    ret = DWL_CACHE_FATAL_RECOVERY;
  if ((asic_status & DEC_HW_IRQ_EXT_TIMEOUT) && (irq == CACHE_HW_FETAL_UNRECOVERY))
    ret = DWL_CACHE_FATAL_UNRECOVERY;
  pthread_mutex_unlock(&dec_dwl->shaper_mutex);
#endif
  (void)dec_dwl;
  return ret;
}

void DWLReleaseL2(const void *instance, i32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  assert((u32)core_id < dec_dwl->num_cores);
  assert(dec_dwl != NULL);
#ifdef SUPPORT_CACHE
  pthread_mutex_lock(&dec_dwl->shaper_mutex);
  i32 irq = DWLDisableCacheChannelALL(RD, core_id);
  (void)irq;
  dec_dwl->shaper_enable[core_id] = 1;
  cache_enable[core_id] = 0;
  pthread_mutex_unlock(&dec_dwl->shaper_mutex);
#endif
  (void)dec_dwl;
}

void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  dec_dwl->sync_params->callback[core_id] = callback_fn;
  dec_dwl->sync_params->callback_arg[core_id] = arg;
}

/* Reserve one valid command buffer. */
i32 DWLReserveCmdBuf(const void *instance, u32 client_type, u32 width, u32 height, u32 *cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 ret;
  struct exchange_parameter params = {0};
  params.executing_time = width * height;
  params.module_type = 2;   /* VC8000D */
  params.cmdbuf_size = VCMD_BUF_SIZE;

  ret = ioctl(dwl_inst->fd, HANTRO_VCMD_IOCH_RESERVE_CMDBUF, &params);
  if (ret < 0) {
    DWL_DEBUG("%s", "DWLReserveCmdBuf failed\n");
    return DWL_ERROR;
  }
  else{
    int cmdbuf_id = params.cmdbuf_id;
    ASSERT(cmdbuf_id < MAX_VCMD_ENTRIES);
    ASSERT(dwl_inst->vcmdb[cmdbuf_id].valid);
    pthread_mutex_lock(&dwl_inst->vcmd_mutex);
    dwl_inst->vcmdb[cmdbuf_id].fd = dwl_inst->fd;
    dwl_inst->vcmdb[cmdbuf_id].valid = 0;
    dwl_inst->vcmdb[cmdbuf_id].cmd_buf_size = params.cmdbuf_size;
    dwl_inst->vcmdb[cmdbuf_id].cmd_buf_used = 0;
    dwl_inst->vcmdb[cmdbuf_id].cmd_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmdb[cmdbuf_id].status_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_status_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmdb[cmdbuf_id].status_bus_addr = dwl_inst->vcmd_mem_params.phy_status_cmdbuf_addr -
                            dwl_inst->vcmd_mem_params.base_ddr_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
    *cmd_buf_id = cmdbuf_id;
  }

  return DWL_OK;
}

struct RegFmtDef {
  int start_index;
  int continous_num;
};

/* Enable one command buffer: link and enable. */
i32 DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 instr_size;
  struct VcmdBuf *vcmd = &dwl_inst->vcmdb[cmd_buf_id];
  struct exchange_parameter params;
  i32 ret;
#ifdef SUPPORT_CACHE
  u32 cache_reg_num, shaper_reg_num;
  u32 version = DWLReadCacheVersion();
#endif

  pthread_mutex_lock(&dwl_inst->vcmd_mutex);

#ifdef SUPPORT_CACHE
  {
  /* Prepare instructions into command buffer. */
  u32 core_id = 0;
  u32 no_chroma = 0;
  u32 tile_by_tile;
  cache_exception_regs_num = 0;
  if (((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_HEVC ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_VP9 ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_H264_H10P ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_AVS2)
    DWLWriteReg(dwl_inst, core_id, 4*3, (dwl_shadow_regs[core_id][3] | 0x08));
  /* If PJPEG and not the last scan, set sw_axi_wr_4k_dis to 0 to disallow AXI wr crossing 4k boundary. */
  if (IS_PJPEG_INTER_SCAN(dwl_shadow_regs[0][3]))
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] & ~0x80000000));
  else
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] | 0x80000000));

  if ((DWLReadReg(dwl_inst, core_id, 4*58) & 0xFF) > 16)
    DWLWriteReg(dwl_inst, core_id, 4*58, ((dwl_shadow_regs[core_id][58] & 0xFFFFFF00) | 0x10));
  if (version >= 0x5)
    DWLWriteReg(dwl_inst, core_id, 4*58, (dwl_shadow_regs[core_id][58] | 0x4000));
  tile_by_tile = dwl_inst->tile_by_tile;
  if (tile_by_tile) {
    if (dwl_inst->tile_id) {
      if (dwl_inst->tile_id < dwl_inst->num_tiles)
        DWLDisableCacheChannelALL(WR, core_id);
      cache_enable[core_id] = 0;
    }
  }
#if 0
  do {
    const unsigned int usec = 1000; /* 1 ms polling interval */
    if (cache_enable[core_id] == 0)
      break;
    usleep(usec);

    max_wait_time--;
  } while (max_wait_time > 0);
#endif
  if (!tile_by_tile) {
    DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
      if (!dwl_inst->shaper_enable[core_id]) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
    }
  } else {
    if (!dwl_inst->tile_id)
      DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
    if (!dwl_inst->shaper_enable[core_id]) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
      dwl_inst->tile_id++;
    }
  }
  cache_enable[core_id] = 1;
  if (no_chroma || dwl_inst->shaper_enable[core_id]) {
    DWLWriteReg(dwl_inst, core_id, 4*3, (dwl_shadow_regs[core_id][3] & 0xFFFFFFF7));
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));
  }
  if (IS_DECMODE_JPEG(dwl_shadow_regs[0][3]) &&
      !dwl_inst->ppu_cfg[0].shaper_enabled && !dwl_inst->ppu_cfg[1].shaper_enabled &&
      !dwl_inst->ppu_cfg[2].shaper_enabled && !dwl_inst->ppu_cfg[3].shaper_enabled &&
      !dwl_inst->ppu_cfg[4].shaper_enabled)
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));

  DWLRefreshCacheRegs(cache_shadow_regs, &cache_reg_num, shaper_shadow_regs, &shaper_reg_num);

#ifdef _DWL_DEBUG
  {
    int i;
    /* Then read shaper status registers. */
    printf("%d cache registers set:\n", cache_reg_num);
    for (i = 0; i < cache_reg_num; i++) {
      printf("[%02d] = 0x%08x\n", i, cache_shadow_regs[i]);
    }
    printf("%d exception list set:\n", cache_exception_regs_num);
    for (i = 0; i < cache_exception_regs_num; i++) {
      printf("[%02d] = 0x%08x\n", i, cache_exception_lists[i]);
    }
    printf("%d shaper registers set:\n", shaper_reg_num);
    for (i = 0; i < shaper_reg_num; i++) {
      printf("[%02d] = 0x%08x\n", i, shaper_shadow_regs[i]);
    }
  }
#endif
  }
#endif

  /****************************************************************************/
  /* Start to generate VCMD instructions. */
  if (dwl_inst->vcmd_params.vcmd_hw_version_id > HW_ID_1_0_C) {
    /* Read VCMD buffer ID (last VCMD registers). */
    CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                          26, 1, /* VCMD command buffer ID register */
                          &instr_size, 0);
    vcmd->cmd_buf_used += instr_size * 4;
  }
#ifdef SUPPORT_DEC400
  DWLConfigureCmdBufForDec400(dwl_inst, cmd_buf_id);
#endif
#ifdef SUPPORT_CACHE
  /* Append write L2Cache/shaper instructions. */
  /* For PJPEG interim scan, disable cache/shaper. */
  if (IS_PJPEG_INTER_SCAN(dwl_shadow_regs[0][3])) {
    cache_shadow_regs[1] = 0;
    CWLCollectWriteRegData(&cache_shadow_regs[1],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                           1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
    cache_reg_num = 0;
  } else if (cache_reg_num) {
      cache_shadow_regs[1] &= ~1;
      if (cache_exception_regs_num)
        cache_shadow_regs[2] |= 0x2;

      CWLCollectWriteRegData(&cache_shadow_regs[1],
                            (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                            dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                             2, &instr_size);
      vcmd->cmd_buf_used += instr_size * 4;

    if (cache_exception_regs_num) {
      /* exception list */
      int i = 0;

      for (i = 0; i < cache_exception_regs_num; i++) {
        CWLCollectWriteRegData(&cache_exception_lists[i],
                               (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                               dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 3, /* cache_swreg3 */
                               1, &instr_size);
        vcmd->cmd_buf_used += instr_size * 4;
      }
    }
    /* Enable cache */
    cache_shadow_regs[1] |= 1;
    CWLCollectWriteRegData(&cache_shadow_regs[1],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                           1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
  }

  /* Configure shaper instruction */
  if (IS_PJPEG_INTER_SCAN(dwl_shadow_regs[0][3])) {
    /* Disable shaper for pjpeg interim scan. */
    shaper_shadow_regs[0] = 0;
    CWLCollectWriteRegData(&shaper_shadow_regs[0],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET, /* shaper reg0 in 4-byte to vcmd base address */
                           1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
    shaper_reg_num = 0;
  } else if (shaper_reg_num) {
    CWLCollectWriteRegData(&shaper_shadow_regs[1],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET + 1, /* shaper reg1 offset in 4-byte to vcmd base address */
                           shaper_reg_num - 1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;

    /* Enable shaper */
    CWLCollectWriteRegData(&shaper_shadow_regs[0],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET, /* shaper reg0 in 4-byte to vcmd base address */
                           1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
  }
#endif

  /* Configure VC8000D instruction */
  if (IS_DECMODE_JPEG(dwl_shadow_regs[0][3])) {
    /* JPEG */
    int reg_seg, reg_segs;
    /* Modify file following register definition for different pp configuration. */
    struct RegFmtDef jpeg_regs_def[] = {{2, 7}, {12, 1}, {16, 11}, {58, 1}, {60, 1},
                                        {168, 2}, {174, 2}, {258, 2}, {265, 2}, {318, 17}, {342, 34}, {410, 5}};
    struct RegFmtDef pjpeg_interim_scan_regs_def[] = {{2, 7}, {12, 1}, {16, 11}, {58, 1}, {60, 1}, {132, 2},
                                        {168, 2}, {174, 2}, {258, 2}, {265, 2}, {318, 3}, {342, 1}};
    struct RegFmtDef pjpeg_last_scan_regs_def[] = {{2, 7}, {12, 1}, {16, 11}, {58, 1}, {60, 1}, {132, 2},
                                        {168, 2}, {174, 2}, {258, 2}, {265, 2}, {318, 17}, {342, 13}, {410, 5}};
    struct RegFmtDef *reg_def;
    if (IS_PJPEG_LAST_SCAN(dwl_shadow_regs[0][3])) {
      reg_segs = sizeof(pjpeg_last_scan_regs_def)/sizeof(pjpeg_last_scan_regs_def[0]);
      reg_def = pjpeg_last_scan_regs_def;
      if (!IS_PP0_RGB(dwl_shadow_regs[0][322])) /* Don't set RGB registers */
        reg_segs--;
    } else if (IS_PJPEG(dwl_shadow_regs[0][3])) {
      reg_segs = sizeof(pjpeg_interim_scan_regs_def)/sizeof(pjpeg_interim_scan_regs_def[0]);
      reg_def = pjpeg_interim_scan_regs_def;
    } else {
      reg_segs = sizeof(jpeg_regs_def)/sizeof(jpeg_regs_def[0]);
      reg_def = jpeg_regs_def;
      if (!IS_PP0_RGB(dwl_shadow_regs[0][322]))
        reg_segs--;
    }

    for (reg_seg = 0; reg_seg < reg_segs; reg_seg++) {
      int si = reg_def[reg_seg].start_index;
      int num = reg_def[reg_seg].continous_num;
      CWLCollectWriteRegData(&dwl_shadow_regs[0][si],
                             (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                             dwl_inst->vcmd_params.submodule_main_addr/4 + si, /* set continous registers sections */
                             num, &instr_size);
      vcmd->cmd_buf_used += instr_size * 4;
    }
  } else {
    /* non-jpeg */
    CWLCollectWriteRegData(&dwl_shadow_regs[0][2],
                           (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                           dwl_inst->vcmd_params.submodule_main_addr/4 + 2 , /* register offset in bytes to vcmd base address */
                           510, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
  }

  /* Write swreg1 to enable dec. */
  CWLCollectWriteRegData(&dwl_shadow_regs[0][0],
                        (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                         dwl_inst->vcmd_params.submodule_main_addr/4 + 0, /* register offset in bytes to vcmd base address */
                         2, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;

  /* Wait for interruption */
  CWLCollectStallData((u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                      &instr_size, VC8000D_FRAME_RDY_INT_MASK);
  vcmd->cmd_buf_used += instr_size * 4;

  /* Read swreg0 for debug */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 0, 1, /* swreg0 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2);
  vcmd->cmd_buf_used += instr_size * 4;
  /* Read status (swreg1) register */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 1, 1, /* swreg1 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2 + 4);
  vcmd->cmd_buf_used += instr_size * 4;
  /* swreg168/169 */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 168, 2, /* swreg168/169 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2 + 4 * 2);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg62/63 - sw_cu_location/sw_perf_cycle_count */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 62, 2, /* swreg62/63 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr/2 + 4 * 4);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);

#ifdef SUPPORT_CACHE
  {
    u32 core_id = 0;
    DWLDisableCacheChannelALL(BI, core_id);
    dwl_inst->shaper_enable[core_id] = 0;
    cache_enable[core_id] = 0;

    /* Disable cache */
    if (cache_reg_num) {
      cache_shadow_regs[1] = 0;
      cache_shadow_regs[2] = 0;
      CWLCollectWriteRegData(&cache_shadow_regs[1],
                             (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                             dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                              2, &instr_size);
      vcmd->cmd_buf_used += instr_size * 4;
    }

    if (shaper_reg_num) {
      /* Disable shaper to flush it only when it's enabled. */
      shaper_shadow_regs[0] = 0;
      CWLCollectWriteRegData(&shaper_shadow_regs[0],
                            (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                            dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET, /* shaper reg0 in 4-byte to vcmd base address */
                             1, &instr_size);
      vcmd->cmd_buf_used += instr_size * 4;

      /* Wait interrupt from shaper */
      CWLCollectStallData((u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          &instr_size,
                          (dwl_inst->vcmd_params.vcmd_hw_version_id == HW_ID_1_0_C ?
                            VC8000D_L2CACHE_INT_MASK_1_0_C : VC8000D_L2CACHE_INT_MASK));
      vcmd->cmd_buf_used += instr_size * 4;
      ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);

      /* Clear shaper int */
      CWLCollectClrIntData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                           CLRINT_OPTYPE_READ_WRITE_1_CLEAR,
                           dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET + 3,  /* shaper reg3 */
                           0xF, &instr_size);
      ASSERT((dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
      vcmd->cmd_buf_used += instr_size * 4;
    }
  }
#endif
#ifdef SUPPORT_DEC400
  DWLFuseCmdBufForDec400(dwl_inst, cmd_buf_id, 5);
#endif
  if (dwl_inst->vcmd_params.vcmd_hw_version_id > HW_ID_1_0_C) {
    /* Dump all vcmd registers. */
    CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                          0, 27, /* VCMD registers count */
                          &instr_size, 0);
    vcmd->cmd_buf_used += instr_size * 4;
  }

  /* Jmp */
  CWLCollectJmpData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used), &instr_size, cmd_buf_id);
  vcmd->cmd_buf_used += instr_size * 4;

  /* End of command buffer instructions. */
  /****************************************************************************/

  params.cmdbuf_size = dwl_inst->vcmdb[cmd_buf_id].cmd_buf_used;
  params.cmdbuf_id = cmd_buf_id;
  ret = ioctl(dwl_inst->fd, HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, &params);
  if (ret < 0) {
    DWL_DEBUG("%s", "DWLEnableCmdBuf failed\n");
    pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
    return DWL_ERROR;
  }

  vcmd->core_id = params.core_id;
  pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
  dwl_inst->core_usage_counts[vcmd->core_id]++;
  
  return DWL_OK;
}

/* Wait cmd buffer ready. */
i32 DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 ret;
  u16 core_info_hw = cmd_buf_id;
  u32 * status = NULL;
  struct VcmdBuf *vcmd = &dwl_inst->vcmdb[cmd_buf_id];

  /* Check invalid parameters */
  if(dwl_inst == NULL)
    return DWL_ERROR;

  DWL_DEBUG("%s", "DWLWaitCmdBufReady\n");
  ret = ioctl(dwl_inst->fd, HANTRO_VCMD_IOCH_WAIT_CMDBUF, &core_info_hw);
  if (ret < 0) {
    DWL_DEBUG("%s", "DWLWaitCmdBufReady failed\n");
    return DWL_HW_WAIT_ERROR;
  } else {
    DWL_DEBUG("%s", "DWLWaitCmdBufReady succeed\n");
    status = (u32 *)(vcmd->status_buf + dwl_inst->vcmd_params.submodule_main_addr/2);
    DWLWriteReg(instance, 0, 1 * 4, status[1]);
    DWLWriteReg(instance, 0, 168 * 4, status[2]);
    DWLWriteReg(instance, 0, 169 * 4, status[3]);
    DWLWriteReg(instance, 0, 62 * 4, status[4]);
    DWLWriteReg(instance, 0, 63 * 4, status[5]);
  }

#ifdef _DWL_DEBUG
  {
    u32 irq_stats =  dwl_shadow_regs[0][HANTRODEC_IRQ_STAT_DEC];

    PrintIrqType(0, irq_stats);
  }
#endif


  return DWL_OK;
}

/* Release one command buffer. */
i32 DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 i = cmd_buf_id, ret;

  ASSERT(cmd_buf_id < MAX_VCMD_ENTRIES);
  ASSERT(!dwl_inst->vcmdb[cmd_buf_id].valid);

  ret = ioctl(dwl_inst->fd, HANTRO_VCMD_IOCH_RELEASE_CMDBUF, &cmd_buf_id);
  if (ret < 0) {
     DWL_DEBUG("%s", "DWLReleaseCmdBuf failed\n");
     return DWL_ERROR;
  } else {
    pthread_mutex_lock(&dwl_inst->vcmd_mutex);
    dwl_inst->vcmdb[i].valid = 1;
    dwl_inst->vcmdb[i].cmd_buf_used = 0;
    pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
  }

  return DWL_OK;
}

i32 DWLWaitCmdbufsDone(const void *instance) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  u32 i;
  /* cmdbuf 0 and 1 not used for user */
  for (i = 2; i < MAX_VCMD_ENTRIES; i++) {
    if( dwl_inst->vcmdb[i].valid == 0) {
      i = 1;
      sched_yield();
    }
  }
  return 0;
}

