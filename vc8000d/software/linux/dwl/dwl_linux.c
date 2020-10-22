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
#include "dwl_activity_trace.h"
#include "dwl_defs.h"
#include "dwl_linux.h"
#include "dwl.h"
#ifdef __linux__
#include "hantrodec.h"
#include "memalloc.h"
#elif defined(__FREERTOS__)
#include "hantrodec_freertos.h"
#include "memalloc_freertos.h"
#else //For other os
//TODO...
#endif
#include "sw_util.h"
#include "deccfg.h"
#include "ppu.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef SUPPORT_CACHE
#include "dwl_linux_cache.h"
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

#define DWL_PJPEG_E 22    /* 1 bit */
#define DWL_REF_BUFF_E 20 /* 1 bit */

#define DWL_JPEG_EXT_E 31        /* 1 bit */
#define DWL_REF_BUFF_ILACE_E 30  /* 1 bit */
#define DWL_MPEG4_CUSTOM_E 29    /* 1 bit */
#define DWL_REF_BUFF_DOUBLE_E 28 /* 1 bit */

#define DWL_MVC_E 20 /* 2 bits */

#define DWL_DEC_TILED_L 17   /* 2 bits */
#define DWL_DEC_PIC_W_EXT 14 /* 2 bits */
#define DWL_EC_E 12          /* 2 bits */
#define DWL_STRIDE_E 11      /* 1 bit */
#define DWL_FIELD_DPB_E 10   /* 1 bit */
#define DWL_AVS_PLUS_E       6  /* 1 bit */
#define DWL_64BIT_ENV_E      5  /* 1 bit */

#define DWL_CFG_E 24         /* 4 bits */
#define DWL_PP_IN_TILED_L 14 /* 2 bits */

#define DWL_SORENSONSPARK_E 11 /* 1 bit */

#define DWL_DOUBLEBUFFER_E 1 /* 1 bit */

#define DWL_H264_FUSE_E 31          /* 1 bit */
#define DWL_MPEG4_FUSE_E 30         /* 1 bit */
#define DWL_MPEG2_FUSE_E 29         /* 1 bit */
#define DWL_SORENSONSPARK_FUSE_E 28 /* 1 bit */
#define DWL_JPEG_FUSE_E 27          /* 1 bit */
#define DWL_VP6_FUSE_E 26           /* 1 bit */
#define DWL_VC1_FUSE_E 25           /* 1 bit */
#define DWL_PJPEG_FUSE_E 24         /* 1 bit */
#define DWL_CUSTOM_MPEG4_FUSE_E 23  /* 1 bit */
#define DWL_RV_FUSE_E 22            /* 1 bit */
#define DWL_VP7_FUSE_E 21           /* 1 bit */
#define DWL_VP8_FUSE_E 20           /* 1 bit */
#define DWL_AVS_FUSE_E 19           /* 1 bit */
#define DWL_MVC_FUSE_E 18           /* 1 bit */
#define DWL_G2_HEVC_FUSE_E 17          /* 1 bit */
#define DWL_HEVC_FUSE_E 11          /* 1 bit */
#define DWL_G2_VP9_FUSE_E 6            /* 1 bit */
#define DWL_VP9_FUSE_E 10            /* 1 bit */

#define DWL_DEC_MAX_4K_FUSE_E 16   /* 1 bit */
#define DWL_DEC_MAX_1920_FUSE_E 15 /* 1 bit */
#define DWL_DEC_MAX_1280_FUSE_E 14 /* 1 bit */
#define DWL_DEC_MAX_720_FUSE_E 13  /* 1 bit */
#define DWL_DEC_MAX_352_FUSE_E 12  /* 1 bit */
#define DWL_REF_BUFF_FUSE_E 7      /* 1 bit */

#define DWL_PP_FUSE_E 31             /* 1 bit */
#define DWL_PP_DEINTERLACE_FUSE_E 30 /* 1 bit */
#define DWL_PP_ALPHA_BLEND_FUSE_E 29 /* 1 bit */
#define DWL_PP_MAX_4096_FUSE_E 16    /* 1 bit */
#define DWL_PP_MAX_1920_FUSE_E 15    /* 1 bit */
#define DWL_PP_MAX_1280_FUSE_E 14    /* 1 bit */
#define DWL_PP_MAX_720_FUSE_E 13     /* 1 bit */
#define DWL_PP_MAX_352_FUSE_E 12     /* 1 bit */

#define DWL_MRB_PREFETCH_E 7         /* 1 bit */
#define DWL_G2_FORMAT_CUSTOMER1_E 6     /* 1 bit */
#define DWL_G2_FORMAT_P010_E 5          /* 1 bit */

#define DWL_FORMAT_CUSTOMER1_E 26     /* 1 bit */
#define DWL_FORMAT_P010_E 25          /* 1 bit */
#define DWL_ADDR_64_E 4              /* 1 bit */

#define DEC_MODE_JPEG 3
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2 16

#ifdef PERFORMANCE_TEST
u32 hw_time_use;
#endif

DECLARE_PERFORMANCE_STATIC(decode_push_reg)
DECLARE_PERFORMANCE_STATIC(decode_pull_reg)

#ifdef SUPPORT_CACHE
u32 cache_enable[MAX_ASIC_CORES];
extern u32 cache_exception_regs_num;
#endif

u32 pjpeg_coeff_size_for_cache = 0;
struct asic_id_info {
  int id;
  int is_read;
};

struct asic_cfg_info {
  DWLHwConfig cfg;
  int is_read;
};

#define G1_ASIC_ID_IDX 0
#define G2_ASIC_ID_IDX 1

static pthread_mutex_t dwl_asic_read_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _DWL_FAKE_HW_TIMEOUT
static void DWLFakeTimeout(u32 *status);
#endif

/* shadow HW registers */
u32 dwl_shadow_regs[MAX_ASIC_CORES][512];
u32 hw_enable[MAX_ASIC_CORES];

static inline u32 CheckRegOffset(struct HANTRODWL *dec_dwl, u32 offset) {
  if (dec_dwl->client_type == DWL_CLIENT_TYPE_PP)
    return offset < dec_dwl->reg_size && offset >= HANTRODECPP_REG_START;
  else
    return offset < dec_dwl->reg_size;
}

#ifdef _DWL_DEBUG
void PrintIrqType(u32 core_id, u32 status) {
  if (status & DEC_IRQ_ABORT)
    printf("DEC[%d] IRQ ABORT\n", core_id);
  else if (status & DEC_IRQ_RDY)
    printf("DEC[%d] IRQ READY\n", core_id);
  else if (status & DEC_IRQ_BUS)
    printf("DEC[%d] IRQ BUS ERROR\n", core_id);
  else if (status & DEC_IRQ_BUFFER)
    printf("DEC[%d] IRQ BUFFER\n", core_id);
  else if (status & DEC_IRQ_ASO)
    printf("DEC[%d] IRQ ASO\n", core_id);
  else if (status & DEC_IRQ_ERROR)
    printf("DEC[%d] IRQ STREAM ERROR\n", core_id);
  else if (status & DEC_IRQ_SLICE)
    printf("DEC[%d] IRQ SLICE\n", core_id);
  else if (status & DEC_IRQ_TIMEOUT)
    printf("DEC[%d] IRQ TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_LAST_SLICE_INT)
    printf("DEC[%d] IRQ LAST_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_NO_SLICE_INT)
    printf("DEC[%d] IRQ NO_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_EXT_TIMEOUT)
    printf("DEC[%d] IRQ EXT_TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_SCAN_RDY)
    printf("DEC[%d] IRQ SCAN RDY\n", core_id);
  else
    printf("DEC[%d] IRQ UNKNOWN 0x%08x\n", core_id, status);
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLMapRegisters
    Description     :

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 *DWLMapRegisters(int mem_dev, /*unsigned long*/ addr_t base, unsigned int reg_size,
                     u32 write) {
  const int page_size = getpagesize();
  const int page_alignment = page_size - 1;

  size_t map_size;
  const char *io = MAP_FAILED;

  /* increase mapping size with unaligned part */
  map_size = reg_size + (base & page_alignment);

  /* map page aligned base */
  if (write)
    io = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, mem_dev,
                      base & ~page_alignment);
  else
    io = (char *)mmap(0, map_size, PROT_READ, MAP_ANONYMOUS|MAP_SHARED, mem_dev,
                      base & ~page_alignment);

  /* add offset from alignment to the io start address */
  if (io != MAP_FAILED) io += (base & page_alignment);

  return (u32 *)io;
}

void DWLUnmapRegisters(const void *io, unsigned int reg_size) {
  const int page_size = getpagesize();
  const int page_alignment = page_size - 1;

  munmap((void *)((long)io & (~page_alignment)),
         reg_size + ((long)io & page_alignment));
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicCoreCount
    Description     : Return the number of hardware cores available
------------------------------------------------------------------------------*/
u32 DWLReadAsicCoreCount(void) {
  int fd_dec;
  unsigned int cores = 0;
  struct subsys_desc subsys;

  /* open driver */
  fd_dec = open(DEC_MODULE_PATH, O_RDWR);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    return 0;
  }
  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }
  if (!subsys.subsys_vcmd_num) {
    /* ask module for cores */
    if (ioctl(fd_dec, HANTRODEC_IOC_MC_CORES, &cores) == -1) {
      DWL_DEBUG("%s", "ioctl failed\n");
      cores = 0;
    }
  } else {
    /* VCMD */
    struct config_parameter vcmd_params;

    vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
    }
    cores = vcmd_params.vcmd_core_num;
  }

end:
  if (fd_dec != -1) close(fd_dec);

  return (u32)cores;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadAsicID(u32 client_type) {
  int fd_dec = -1;
  int fd_mem = -1;
  int core_id = 0, hw_id = 0, idx = 0;
  static struct asic_id_info asic_id[2];/*idx 0:G1, idx 1:G2*/
  u32 ret_value = 0;
  struct subsys_desc subsys;

  DWL_DEBUG("client_type=%d\n", client_type);

  pthread_mutex_lock(&dwl_asic_read_mutex);
  if ((client_type == DWL_CLIENT_TYPE_PP) && (asic_id[G1_ASIC_ID_IDX].is_read != 0)) {
    ret_value = asic_id[G1_ASIC_ID_IDX].id;
    goto end;
  } else if (client_type == DWL_CLIENT_TYPE_PP)
    client_type = DWL_CLIENT_TYPE_H264_DEC;

  if ((client_type >= DWL_CLIENT_TYPE_H264_DEC) && (client_type <= DWL_CLIENT_TYPE_VP8_DEC))
    idx = 0;
  else if ((client_type == DWL_CLIENT_TYPE_VP9_DEC) ||
           (client_type == DWL_CLIENT_TYPE_HEVC_DEC) ||
           (client_type == DWL_CLIENT_TYPE_AVS2_DEC) ||
           (client_type == DWL_CLIENT_TYPE_ST_PP))
    idx = 1;
  else {
    ret_value = 0;
    goto end;
  }

  /*if G1 or G2 asic id has been attained, just return the value*/
  if (asic_id[idx].is_read != 0) {
    ret_value = asic_id[idx].id;
    goto end;
  }

  asic_id[idx].is_read = 1;

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }
  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }
  if (!subsys.subsys_vcmd_num) {
    /* Without VCMD */

    /*need to get core id*/
    core_id =
      ioctl(fd_dec, HANTRODEC_IOCG_CORE_ID, client_type);

    /* negative value signals an error */
    if (core_id < 0) {
      DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_reserve failed, %d\n",
                "DEC", core_id);
      goto end;
    }

    /*input cord_id and output asic_id*/
    hw_id = core_id;
    if (ioctl(fd_dec, HANTRODEC_IOX_ASIC_ID, &hw_id) < 0) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      hw_id = 0;
      goto end;
    }
  } else {
    /* VCMD */
    struct config_parameter vcmd_params;
    struct cmdbuf_mem_parameter vcmd_mem_params;
    u32 *reg;

    fd_mem = open("/dev/mem", O_RDONLY);
    if (fd_mem < 0) {
        DWL_DEBUG("DWLReadAsicID: failed to open: %s\n", "/dev/mem");
        goto end;
    }

    vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
    }

    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
              &vcmd_mem_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto end;
    }

    vcmd_mem_params.virt_status_cmdbuf_addr = (u32 *) mmap(0, vcmd_mem_params.status_cmdbuf_total_size, PROT_READ,
                                                MAP_ANONYMOUS|MAP_SHARED, fd_mem,
                                                vcmd_mem_params.phy_status_cmdbuf_addr);

    if(vcmd_mem_params.virt_status_cmdbuf_addr != MAP_FAILED) {
      /* Read from swreg0 */
      reg = vcmd_mem_params.virt_status_cmdbuf_addr +
            vcmd_params.config_status_cmdbuf_id * vcmd_mem_params.status_cmdbuf_unit_size / 4 +
            vcmd_params.submodule_main_addr/2/4;
      hw_id = reg[0];

      munmap(vcmd_mem_params.virt_status_cmdbuf_addr,
             vcmd_mem_params.status_cmdbuf_total_size);
    }
  }

  asic_id[idx].id = hw_id;

  ret_value = hw_id;

end:

  if (fd_dec != -1) close(fd_dec);
  pthread_mutex_unlock(&dwl_asic_read_mutex);
  DWL_DEBUG("ret_value=%x\n", ret_value);
  return ret_value;
}


/*------------------------------------------------------------------------------
    Function name   : DWLReadHwBuildID
    Description     : Read the HW build ID. Does not need a DWL instance to run

    Return type     : u32 - the HW Build ID
------------------------------------------------------------------------------*/
u32 DWLReadHwBuildID(u32 client_type) {
  int fd_dec = -1;
  int fd_mem  = -1;
  int core_id = 0, hw_id = 0, idx = 0;
  static struct asic_id_info hw_build_id[2];/*idx 0:G1, idx 1:G2*/
  u32 ret_value = 0;
  struct subsys_desc subsys;

  DWL_DEBUG("client_type=%d\n", client_type);

  pthread_mutex_lock(&dwl_asic_read_mutex);
  if ((client_type == DWL_CLIENT_TYPE_PP) && (hw_build_id[G1_ASIC_ID_IDX].is_read != 0)) {
    ret_value = hw_build_id[G1_ASIC_ID_IDX].id;
    goto end;
  } else if (client_type == DWL_CLIENT_TYPE_PP)
    client_type = DWL_CLIENT_TYPE_H264_DEC;

  if ((client_type >= DWL_CLIENT_TYPE_H264_DEC) && (client_type <= DWL_CLIENT_TYPE_VP8_DEC))
    idx = 0;
  else if ((client_type == DWL_CLIENT_TYPE_VP9_DEC) ||
           (client_type == DWL_CLIENT_TYPE_HEVC_DEC) ||
           (client_type == DWL_CLIENT_TYPE_AVS2_DEC) ||
           (client_type == DWL_CLIENT_TYPE_ST_PP))
    idx = 1;
  else {
    ret_value = 0;
    goto end;
  }

  /*if G1 or G2 asic id has been attained, just return the value*/
  if (hw_build_id[idx].is_read != 0) {
    ret_value = hw_build_id[idx].id;
    goto end;
  }

  hw_build_id[idx].is_read = 1;

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }
  if (!subsys.subsys_vcmd_num) {

    /* need to get core id */
    core_id =
      ioctl(fd_dec, HANTRODEC_IOCG_CORE_ID, client_type);

    /* negative value signals an error */
    if (core_id < 0) {
      DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_reserve failed, %d\n",
                "DEC", core_id);
      goto end;
    }

    /* Input cord_id and output asic build_id. */
    hw_id = core_id;
    if (ioctl(fd_dec, HANTRODEC_IOX_ASIC_BUILD_ID, &hw_id) < 0) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      hw_id = 0;
      goto end;
    }
  } else {
    /* VCMD */
    struct config_parameter vcmd_params;
    struct cmdbuf_mem_parameter vcmd_mem_params;
    u32 *reg;

    fd_mem = open("/dev/mem", O_RDONLY);
    if (fd_mem < 0) {
        DWL_DEBUG("DWLReadAsicID: failed to open: %s\n", "/dev/mem");
        goto end;
    }

    vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
    }

    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
              &vcmd_mem_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto end;
    }

    vcmd_mem_params.virt_status_cmdbuf_addr = (u32 *) mmap(0, vcmd_mem_params.status_cmdbuf_total_size, PROT_READ,
                                                MAP_ANONYMOUS|MAP_SHARED, fd_mem,
                                                vcmd_mem_params.phy_status_cmdbuf_addr);

    /* Read from swreg309 */
    reg = vcmd_mem_params.virt_status_cmdbuf_addr +
          vcmd_params.config_status_cmdbuf_id * vcmd_mem_params.status_cmdbuf_unit_size / 4 +
          vcmd_params.submodule_main_addr/4/2;  /*Main IP occupies /2 bytes in status buffer*/
    hw_id = reg[309];

    if(vcmd_mem_params.virt_status_cmdbuf_addr != MAP_FAILED)
      munmap(vcmd_mem_params.virt_status_cmdbuf_addr,
             vcmd_mem_params.status_cmdbuf_total_size);
  }

  hw_build_id[idx].id = hw_id;

  ret_value = hw_id;

end:

  if (fd_dec != -1) close(fd_dec);
  pthread_mutex_unlock(&dwl_asic_read_mutex);
  DWL_DEBUG("ret_value=%x\n", ret_value);
  return ret_value;
}



/*------------------------------------------------------------------------------
    Function name   : DWLReadCoreAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadCoreAsicID(u32 core_id) {
  int fd_dec = -1;
  u32 ret_value = 0;
  int hw_id = 0;


  pthread_mutex_lock(&dwl_asic_read_mutex);

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  /*input cord_id and output asic_id*/
  hw_id = core_id;
  if (ioctl(fd_dec, HANTRODEC_IOX_ASIC_ID, &hw_id) < 0) {
    DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
    hw_id = 0;
    goto end;
  }

  ret_value = hw_id;

end:
  if (fd_dec != -1) close(fd_dec);
  pthread_mutex_unlock(&dwl_asic_read_mutex);
  DWL_DEBUG("ret_value=%x\n", ret_value);
  return ret_value;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadCoreHwBuildID
    Description     : Read the HW build ID. Does not need a DWL instance to run

    Return type     : u32 - the HW Build ID
------------------------------------------------------------------------------*/
u32 DWLReadCoreHwBuildID(u32 core_id) {
  int fd_dec = -1;
  int fd_mem = -1;

  int hw_id = 0, idx = 0;
  static struct asic_id_info hw_build_id[2];/*idx 0:G1, idx 1:G2*/
  u32 ret_value = 0;
  struct subsys_desc subsys;

  DWL_DEBUG("core_id=%d\n", core_id);

  pthread_mutex_lock(&dwl_asic_read_mutex);
  idx = core_id;

  /*if G1 or G2 asic id has been attained, just return the value*/
  if (hw_build_id[idx].is_read != 0) {
    ret_value = hw_build_id[idx].id;
    goto end;
  }

  hw_build_id[idx].is_read = 1;

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }
  if (!subsys.subsys_vcmd_num) {

    /* Input cord_id and output asic build_id. */
    hw_id = core_id;
    if (ioctl(fd_dec, HANTRODEC_IOX_ASIC_BUILD_ID, &hw_id) < 0) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      hw_id = 0;
      goto end;
    }
  } else {
    /* VCMD */
    struct config_parameter vcmd_params;
    struct cmdbuf_mem_parameter vcmd_mem_params;
    u32 *reg;

    fd_mem = open("/dev/mem", O_RDONLY);
    if (fd_mem < 0) {
        DWL_DEBUG("DWLReadAsicID: failed to open: %s\n", "/dev/mem");
        goto end;
    }

    vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
    }

    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
              &vcmd_mem_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto end;
    }

    vcmd_mem_params.virt_status_cmdbuf_addr = (u32 *) mmap(0, vcmd_mem_params.status_cmdbuf_total_size, PROT_READ,
                                                MAP_ANONYMOUS|MAP_SHARED, fd_mem,
                                                vcmd_mem_params.phy_status_cmdbuf_addr);

    /* Read from swreg309 */
    reg = vcmd_mem_params.virt_status_cmdbuf_addr +
          vcmd_params.config_status_cmdbuf_id * vcmd_mem_params.status_cmdbuf_unit_size / 4 +
          vcmd_params.submodule_main_addr/4/2;  /*Main IP occupies /2 bytes in status buffer*/
    hw_id = reg[309];

    if(vcmd_mem_params.virt_status_cmdbuf_addr != MAP_FAILED)
      munmap(vcmd_mem_params.virt_status_cmdbuf_addr,
             vcmd_mem_params.status_cmdbuf_total_size);
  }

  hw_build_id[idx].id = hw_id;

  ret_value = hw_id;

end:

  if (fd_dec != -1) close(fd_dec);
  pthread_mutex_unlock(&dwl_asic_read_mutex);
  DWL_DEBUG("ret_value=%x\n", ret_value);
  return ret_value;
}


static void ReadCoreFuse(const u32 *io, struct DWLHwFuseStatus *hw_fuse_sts) {
  u32 config_reg, fuse_reg, fuse_reg_pp;
  const u32 asic_id = io[0];

  /* Decoder configuration */
  config_reg = io[HANTRODEC_SYNTH_CFG];

  /* Decoder fuse configuration */
  fuse_reg = io[HANTRODEC_FUSE_CFG];

  hw_fuse_sts->vp6_support_fuse = (fuse_reg >> DWL_VP6_FUSE_E) & 0x01U;
  hw_fuse_sts->vp7_support_fuse = (fuse_reg >> DWL_VP7_FUSE_E) & 0x01U;
  hw_fuse_sts->vp8_support_fuse = (fuse_reg >> DWL_VP8_FUSE_E) & 0x01U;
  hw_fuse_sts->vp9_support_fuse = (fuse_reg >> DWL_G2_VP9_FUSE_E) & 0x01U;
  hw_fuse_sts->h264_support_fuse = (fuse_reg >> DWL_H264_FUSE_E) & 0x01U;
  hw_fuse_sts->HevcSupportFuse = (fuse_reg >> DWL_G2_HEVC_FUSE_E) & 0x01U;
  hw_fuse_sts->mpeg4_support_fuse = (fuse_reg >> DWL_MPEG4_FUSE_E) & 0x01U;
  hw_fuse_sts->mpeg2_support_fuse = (fuse_reg >> DWL_MPEG2_FUSE_E) & 0x01U;
  hw_fuse_sts->sorenson_spark_support_fuse =
    (fuse_reg >> DWL_SORENSONSPARK_FUSE_E) & 0x01U;
  hw_fuse_sts->jpeg_support_fuse = (fuse_reg >> DWL_JPEG_FUSE_E) & 0x01U;
  hw_fuse_sts->vc1_support_fuse = (fuse_reg >> DWL_VC1_FUSE_E) & 0x01U;
  hw_fuse_sts->jpeg_prog_support_fuse = (fuse_reg >> DWL_PJPEG_FUSE_E) & 0x01U;
  hw_fuse_sts->rv_support_fuse = (fuse_reg >> DWL_RV_FUSE_E) & 0x01U;
  hw_fuse_sts->avs_support_fuse = (fuse_reg >> DWL_AVS_FUSE_E) & 0x01U;
  hw_fuse_sts->custom_mpeg4_support_fuse =
    (fuse_reg >> DWL_CUSTOM_MPEG4_FUSE_E) & 0x01U;
  hw_fuse_sts->mvc_support_fuse = (fuse_reg >> DWL_MVC_FUSE_E) & 0x01U;

  /* check max. decoder output width */
  if (fuse_reg & 0x10000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 4096;
  else if (fuse_reg & 0x8000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 1920;
  else if (fuse_reg & 0x4000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 1280;
  else if (fuse_reg & 0x2000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 720;
  else if (fuse_reg & 0x1000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 352;

  if ((asic_id >> 16) == 0x8001U) {
    hw_fuse_sts->vp9_support_fuse = (fuse_reg >> DWL_VP9_FUSE_E) & 0x01U;
    hw_fuse_sts->HevcSupportFuse = (fuse_reg >> DWL_HEVC_FUSE_E) & 0x01U;
  }

  hw_fuse_sts->ref_buf_support_fuse = (fuse_reg >> DWL_REF_BUFF_FUSE_E) & 0x01U;

  if (((asic_id >> 16) >= 0x8190U) || ((asic_id >> 16) == 0x6731U)) {
    /* Pp configuration */
    config_reg = io[HANTROPP_SYNTH_CFG];

    if ((config_reg >> DWL_G1_PP_E) & 0x01U) {
      /* Pp fuse configuration */
      fuse_reg_pp = io[HANTRODECPP_FUSE_CFG_G1];

      if ((fuse_reg_pp >> DWL_PP_FUSE_E) & 0x01U) {
        hw_fuse_sts->pp_support_fuse = 1;

        /* check max. pp output width */
        if (fuse_reg_pp & 0x10000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;
        else if (fuse_reg_pp & 0x8000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1920;
        else if (fuse_reg_pp & 0x4000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1280;
        else if (fuse_reg_pp & 0x2000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 720;
        else if (fuse_reg_pp & 0x1000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 352;

        hw_fuse_sts->pp_config_fuse = fuse_reg_pp;
      } else {
        hw_fuse_sts->pp_support_fuse = 0;
        hw_fuse_sts->max_pp_out_pic_width_fuse = 0;
        hw_fuse_sts->pp_config_fuse = 0;
      }
    }
  } else if (((asic_id >> 16) == 0x6732U)) {
    /* Pp configuration */
    config_reg = io[HANTRODECPP_SYNTH_CFG];

    if ((config_reg >> DWL_G2_PP_E) & 0x01U) {
      /* Pp fuse configuration */
      fuse_reg_pp = io[HANTRODECPP_FUSE_CFG_G2];

      if ((fuse_reg_pp >> DWL_PP_FUSE_E) & 0x01U) {
        hw_fuse_sts->pp_support_fuse = 1;

        /* check max. pp output width */
        if (fuse_reg_pp & 0x10000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;
        else if (fuse_reg_pp & 0x8000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1920;
        else if (fuse_reg_pp & 0x4000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1280;
        else if (fuse_reg_pp & 0x2000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 720;
        else if (fuse_reg_pp & 0x1000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 352;

        hw_fuse_sts->pp_config_fuse = fuse_reg_pp;
      } else {
        hw_fuse_sts->pp_support_fuse = 0;
        hw_fuse_sts->max_pp_out_pic_width_fuse = 0;
        hw_fuse_sts->pp_config_fuse = 0;
      }
    }
  } else if (((asic_id >> 16) == 0x8001U)) {
      /* Pp configuration */
      config_reg = io[HANTRODECPP_CFG_STAT];

      if ((config_reg >> DWL_PP_E) & 0x01U) {
        /* Pp fuse configuration */
        fuse_reg_pp = io[HANTRODECPP_FUSE_CFG];

      if ((fuse_reg_pp >> DWL_PP_FUSE_E) & 0x01U) {
        hw_fuse_sts->pp_support_fuse = 1;

        /* check max. pp output width */
        if (fuse_reg_pp & 0x10000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;
        else if (fuse_reg_pp & 0x8000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1920;
        else if (fuse_reg_pp & 0x4000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 1280;
        else if (fuse_reg_pp & 0x2000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 720;
        else if (fuse_reg_pp & 0x1000U)
          hw_fuse_sts->max_pp_out_pic_width_fuse = 352;

        hw_fuse_sts->pp_config_fuse = fuse_reg_pp;
      } else {
        hw_fuse_sts->pp_support_fuse = 0;
        hw_fuse_sts->max_pp_out_pic_width_fuse = 0;
        hw_fuse_sts->pp_config_fuse = 0;
      }
    }
  }
}

static void ReadCoreConfig(const u32 *io, DWLHwConfig *hw_cfg) {
  u32 config_reg;
  const u32 asic_id = io[0];

  /* Decoder configuration */
  config_reg = io[HANTRODEC_SYNTH_CFG];

  hw_cfg->h264_support = (config_reg >> DWL_H264_E) & 0x3U;
  /* check jpeg */
  hw_cfg->jpeg_support = (config_reg >> DWL_JPEG_E) & 0x01U;
  if (hw_cfg->jpeg_support && ((config_reg >> DWL_PJPEG_E) & 0x01U))
    hw_cfg->jpeg_support = JPEG_PROGRESSIVE;
  hw_cfg->mpeg4_support = (config_reg >> DWL_MPEG4_E) & 0x3U;
  hw_cfg->vc1_support = (config_reg >> DWL_VC1_E) & 0x3U;
  hw_cfg->mpeg2_support = (config_reg >> DWL_MPEG2_E) & 0x01U;
  hw_cfg->sorenson_spark_support = (config_reg >> DWL_SORENSONSPARK_E) & 0x01U;
#ifndef DWL_REFBUFFER_DISABLE
  hw_cfg->ref_buf_support = (config_reg >> DWL_REF_BUFF_E) & 0x01U;
#else
  hw_cfg->ref_buf_support = 0;
#endif
  hw_cfg->vp6_support = (config_reg >> DWL_VP6_E) & 0x01U;
#ifdef DEC_X170_APF_DISABLE
  if (DEC_X170_APF_DISABLE) {
    hw_cfg->tiled_mode_support = 0;
  }
#endif /* DEC_X170_APF_DISABLE */

  hw_cfg->max_dec_pic_width = config_reg & 0x07FFU;

  if ((asic_id >> 16) == 0x8001U)
    hw_cfg->max_dec_pic_width = config_reg & 0xFFFFU;

  if ((asic_id >> 16) != 0x8001U) {
    /* 2nd Config register */
    config_reg = io[HANTRODEC_SYNTH_CFG_2];
    if (hw_cfg->ref_buf_support) {
      if ((config_reg >> DWL_REF_BUFF_ILACE_E) & 0x01U)
        hw_cfg->ref_buf_support |= 2;
      if ((config_reg >> DWL_REF_BUFF_DOUBLE_E) & 0x01U)
        hw_cfg->ref_buf_support |= 4;
    }
  }
  //hw_cfg->vp9_support = (config_reg >> DWL_VP9_E) & 0x3U;
  //hw_cfg->hevc_support = (config_reg >> DWL_HEVC_E) & 0x3U;
  hw_cfg->custom_mpeg4_support = (config_reg >> DWL_MPEG4_CUSTOM_E) & 0x01U;
  hw_cfg->vp7_support = (config_reg >> DWL_VP7_E) & 0x01U;
  hw_cfg->vp8_support = (config_reg >> DWL_VP8_E) & 0x01U;
  hw_cfg->avs_support = (config_reg >> DWL_AVS_E) & 0x01U;

  /* JPEG extensions */
  if (((asic_id >> 16) >= 0x8190U) || ((asic_id >> 16) == 0x6731U))
    hw_cfg->jpeg_esupport = (config_reg >> DWL_JPEG_EXT_E) & 0x01U;
  else
    hw_cfg->jpeg_esupport = JPEG_EXT_NOT_SUPPORTED;

  if (((asic_id >> 16) >= 0x9170U) || ((asic_id >> 16) == 0x6731U) || ((asic_id >> 16) == 0x8001U))
    hw_cfg->rv_support = (config_reg >> DWL_RV_E) & 0x03U;
  else
    hw_cfg->rv_support = RV_NOT_SUPPORTED;

  hw_cfg->mvc_support = (config_reg >> DWL_MVC_E) & 0x03U;
  hw_cfg->webp_support = (config_reg >> DWL_WEBP_E) & 0x01U;

  if ((asic_id >> 16) != 0x8001U) {
    hw_cfg->tiled_mode_support = (config_reg >> DWL_DEC_TILED_L) & 0x03U;
    hw_cfg->max_dec_pic_width += ((config_reg >> DWL_DEC_PIC_W_EXT) & 0x03U)
                                 << 11;
  }

  hw_cfg->ec_support = (config_reg >> DWL_EC_E) & 0x03U;

  if ((asic_id >> 16) != 0x8001U) {
    hw_cfg->stride_support = (config_reg >> DWL_STRIDE_E) & 0x01U;
    hw_cfg->field_dpb_support = (config_reg >> DWL_FIELD_DPB_E) & 0x01U;
  }

  hw_cfg->avs_plus_support = (config_reg >> DWL_AVS_PLUS_E) & 0x01U;
  hw_cfg->addr64_support = (config_reg >> DWL_64BIT_ENV_E) & 0x01U;

  if (hw_cfg->ref_buf_support && ((asic_id >> 16) == 0x6731U)) {
    hw_cfg->ref_buf_support |= 8; /* enable HW support for offset */
  }

  hw_cfg->double_buffer_support = (config_reg >> DWL_DOUBLEBUFFER_E) & 0x01U;

  /* 3rd Config register */
  config_reg = io[HANTRODEC_SYNTH_CFG_3];
  hw_cfg->max_dec_pic_height = config_reg & 0x0FFFU;

  if ((asic_id >> 16) == 0x8001U)
    hw_cfg->max_dec_pic_height = config_reg & 0xFFFFU;

  /* Pp configuration */
  config_reg = io[HANTRODECPP_SYNTH_CFG];

  if (((asic_id >> 16) == 0x6731U) && ((config_reg >> DWL_G1_PP_E) & 0x01U)) {
    hw_cfg->pp_support = 1;
    /* Theoretical max range 0...8191; actual 48...4096 */
    hw_cfg->max_pp_out_pic_width = config_reg & 0x1FFFU;
    /*hw_cfg->pp_config = (config_reg >> DWL_CFG_E) & 0x0FU; */
    hw_cfg->pp_config = config_reg;
  } else {
    hw_cfg->pp_support = 0;
    hw_cfg->max_pp_out_pic_width = 0;
    hw_cfg->pp_config = 0;
  }

  /* check the HW version */
  if (((asic_id >> 16) >= 0x8190U) || ((asic_id >> 16) == 0x6731U)) {
    u32 de_interlace;
    u32 alpha_blend;
    u32 de_interlace_fuse;
    u32 alpha_blend_fuse;
    struct DWLHwFuseStatus hw_fuse_sts;

    /* check fuse status */
    ReadCoreFuse(io, &hw_fuse_sts);

    /* Maximum decoding width supported by the HW */
    if (hw_cfg->max_dec_pic_width > hw_fuse_sts.max_dec_pic_width_fuse)
      hw_cfg->max_dec_pic_width = hw_fuse_sts.max_dec_pic_width_fuse;
    /* Maximum output width of Post-Processor */
    if (hw_cfg->max_pp_out_pic_width > hw_fuse_sts.max_pp_out_pic_width_fuse)
      hw_cfg->max_pp_out_pic_width = hw_fuse_sts.max_pp_out_pic_width_fuse;
    /* h264 */
    if (!hw_fuse_sts.h264_support_fuse)
      hw_cfg->h264_support = H264_NOT_SUPPORTED;
    /* mpeg-4 */
    if (!hw_fuse_sts.mpeg4_support_fuse)
      hw_cfg->mpeg4_support = MPEG4_NOT_SUPPORTED;
    /* custom mpeg-4 */
    if (!hw_fuse_sts.custom_mpeg4_support_fuse)
      hw_cfg->custom_mpeg4_support = MPEG4_CUSTOM_NOT_SUPPORTED;
    /* jpeg (baseline && progressive) */
    if (!hw_fuse_sts.jpeg_support_fuse)
      hw_cfg->jpeg_support = JPEG_NOT_SUPPORTED;
    if ((hw_cfg->jpeg_support == JPEG_PROGRESSIVE) &&
        !hw_fuse_sts.jpeg_prog_support_fuse)
      hw_cfg->jpeg_support = JPEG_BASELINE;
    /* mpeg-2 */
    if (!hw_fuse_sts.mpeg2_support_fuse)
      hw_cfg->mpeg2_support = MPEG2_NOT_SUPPORTED;
    /* vc-1 */
    if (!hw_fuse_sts.vc1_support_fuse) hw_cfg->vc1_support = VC1_NOT_SUPPORTED;
    /* vp6 */
    if (!hw_fuse_sts.vp6_support_fuse) hw_cfg->vp6_support = VP6_NOT_SUPPORTED;
    /* vp7 */
    if (!hw_fuse_sts.vp7_support_fuse) hw_cfg->vp7_support = VP7_NOT_SUPPORTED;
    /* vp8 */
    if (!hw_fuse_sts.vp8_support_fuse) hw_cfg->vp8_support = VP8_NOT_SUPPORTED;
    /* webp */
    if (!hw_fuse_sts.vp8_support_fuse)
      hw_cfg->webp_support = WEBP_NOT_SUPPORTED;
    /* pp */
    if (!hw_fuse_sts.pp_support_fuse) hw_cfg->pp_support = PP_NOT_SUPPORTED;
    /* check the pp config vs fuse status */
    if ((hw_cfg->pp_config & 0xFC000000) &&
        ((hw_fuse_sts.pp_config_fuse & 0xF0000000) >> 5)) {
      /* config */
      de_interlace = ((hw_cfg->pp_config & PP_DEINTERLACING) >> 25);
      alpha_blend = ((hw_cfg->pp_config & PP_ALPHA_BLENDING) >> 24);
      /* fuse */
      de_interlace_fuse =
        (((hw_fuse_sts.pp_config_fuse >> 5) & PP_DEINTERLACING) >> 25);
      alpha_blend_fuse =
        (((hw_fuse_sts.pp_config_fuse >> 5) & PP_ALPHA_BLENDING) >> 24);

      /* check if */
      if (de_interlace && !de_interlace_fuse) hw_cfg->pp_config &= 0xFD000000;
      if (alpha_blend && !alpha_blend_fuse) hw_cfg->pp_config &= 0xFE000000;
    }
    /* sorenson */
    if (!hw_fuse_sts.sorenson_spark_support_fuse)
      hw_cfg->sorenson_spark_support = SORENSON_SPARK_NOT_SUPPORTED;
    /* ref. picture buffer */
    if (!hw_fuse_sts.ref_buf_support_fuse)
      hw_cfg->ref_buf_support = REF_BUF_NOT_SUPPORTED;

    /* rv */
    if (!hw_fuse_sts.rv_support_fuse) hw_cfg->rv_support = RV_NOT_SUPPORTED;
    /* avs */
    if (!hw_fuse_sts.avs_support_fuse) hw_cfg->avs_support = AVS_NOT_SUPPORTED;
    /* mvc */
    if (!hw_fuse_sts.mvc_support_fuse) hw_cfg->mvc_support = MVC_NOT_SUPPORTED;
  } else if ((asic_id >> 16) == 0x6732U) {
    /* G2 */
    config_reg = io[HANTRODEC_CFG_STAT];

    hw_cfg->hevc_support = (config_reg >> DWL_G2_HEVC_E) & 0x1U;
    hw_cfg->vp9_support = (config_reg >> DWL_G2_VP9_E) & 0x1U;

    hw_cfg->hevc_main10_support = hw_cfg->hevc_support && (((config_reg >> DWL_HEVC_VER) & 0xFU) == 1);
    hw_cfg->vp9_10bit_support = hw_cfg->vp9_support && (((config_reg >> DWL_VP9_PROFILE) & 0xFU) == 1);
    hw_cfg->ds_support = (config_reg >> DWL_G2_DS_E) & 0x1U;
    hw_cfg->rfc_support = (config_reg >> DWL_G2_RFC_E) & 0x1U;
    hw_cfg->ring_buffer_support = 1; /* Always enabled as default. */

    hw_cfg->mrb_prefetch = (config_reg >> DWL_MRB_PREFETCH_E) & 0x1U;
    //hw_cfg->addr64_support = (config_reg >> DWL_ADDR_64_E) & 0x1U;
    /* our 64b SW support both 32b/64b HW. */
    hw_cfg->addr64_support = 1;
    hw_cfg->fmt_p010_support = (config_reg >> DWL_G2_FORMAT_P010_E) & 0x1U;
    hw_cfg->fmt_customer1_support = (config_reg >> DWL_G2_FORMAT_CUSTOMER1_E) & 0x1U;
  } else if ((asic_id >> 16) == 0x8001U) {
    u32 de_interlace;
    u32 alpha_blend;
    u32 de_interlace_fuse;
    u32 alpha_blend_fuse;
    struct DWLHwFuseStatus hw_fuse_sts;

    /* check fuse status */
    ReadCoreFuse(io, &hw_fuse_sts);

    /* Maximum decoding width supported by the HW */
    if (hw_cfg->max_dec_pic_width > hw_fuse_sts.max_dec_pic_width_fuse)
      hw_cfg->max_dec_pic_width = hw_fuse_sts.max_dec_pic_width_fuse;
    /* Maximum output width of Post-Processor */
    if (hw_cfg->max_pp_out_pic_width > hw_fuse_sts.max_pp_out_pic_width_fuse)
      hw_cfg->max_pp_out_pic_width = hw_fuse_sts.max_pp_out_pic_width_fuse;
    /* h264 */
    if (!hw_fuse_sts.h264_support_fuse)
      hw_cfg->h264_support = H264_NOT_SUPPORTED;
    /* mpeg-4 */
    if (!hw_fuse_sts.mpeg4_support_fuse)
      hw_cfg->mpeg4_support = MPEG4_NOT_SUPPORTED;
    /* custom mpeg-4 */
    if (!hw_fuse_sts.custom_mpeg4_support_fuse)
      hw_cfg->custom_mpeg4_support = MPEG4_CUSTOM_NOT_SUPPORTED;
    /* jpeg (baseline && progressive) */
    if (!hw_fuse_sts.jpeg_support_fuse)
      hw_cfg->jpeg_support = JPEG_NOT_SUPPORTED;
    if ((hw_cfg->jpeg_support == JPEG_PROGRESSIVE) &&
        !hw_fuse_sts.jpeg_prog_support_fuse)
      hw_cfg->jpeg_support = JPEG_BASELINE;
    /* mpeg-2 */
    if (!hw_fuse_sts.mpeg2_support_fuse)
      hw_cfg->mpeg2_support = MPEG2_NOT_SUPPORTED;
    /* vc-1 */
    if (!hw_fuse_sts.vc1_support_fuse) hw_cfg->vc1_support = VC1_NOT_SUPPORTED;
    /* vp6 */
    if (!hw_fuse_sts.vp6_support_fuse) hw_cfg->vp6_support = VP6_NOT_SUPPORTED;
    /* vp7 */
    if (!hw_fuse_sts.vp7_support_fuse) hw_cfg->vp7_support = VP7_NOT_SUPPORTED;
    /* vp8 */
    if (!hw_fuse_sts.vp8_support_fuse) hw_cfg->vp8_support = VP8_NOT_SUPPORTED;
    /* webp */
    if (!hw_fuse_sts.vp8_support_fuse)
      hw_cfg->webp_support = WEBP_NOT_SUPPORTED;
    /* pp */
    if (!hw_fuse_sts.pp_support_fuse) hw_cfg->pp_support = PP_NOT_SUPPORTED;
    /* check the pp config vs fuse status */
    if ((hw_cfg->pp_config & 0xFC000000) &&
        ((hw_fuse_sts.pp_config_fuse & 0xF0000000) >> 5)) {
      /* config */
      de_interlace = ((hw_cfg->pp_config & PP_DEINTERLACING) >> 25);
      alpha_blend = ((hw_cfg->pp_config & PP_ALPHA_BLENDING) >> 24);
      /* fuse */
      de_interlace_fuse =
        (((hw_fuse_sts.pp_config_fuse >> 5) & PP_DEINTERLACING) >> 25);
      alpha_blend_fuse =
        (((hw_fuse_sts.pp_config_fuse >> 5) & PP_ALPHA_BLENDING) >> 24);

      /* check if */
      if (de_interlace && !de_interlace_fuse) hw_cfg->pp_config &= 0xFD000000;
      if (alpha_blend && !alpha_blend_fuse) hw_cfg->pp_config &= 0xFE000000;
    }
    /* sorenson */
    if (!hw_fuse_sts.sorenson_spark_support_fuse)
      hw_cfg->sorenson_spark_support = SORENSON_SPARK_NOT_SUPPORTED;
    /* ref. picture buffer */
    if (!hw_fuse_sts.ref_buf_support_fuse)
      hw_cfg->ref_buf_support = REF_BUF_NOT_SUPPORTED;

    /* rv */
    if (!hw_fuse_sts.rv_support_fuse) hw_cfg->rv_support = RV_NOT_SUPPORTED;
    /* avs */
    if (!hw_fuse_sts.avs_support_fuse) hw_cfg->avs_support = AVS_NOT_SUPPORTED;
    /* mvc */
    if (!hw_fuse_sts.mvc_support_fuse) hw_cfg->mvc_support = MVC_NOT_SUPPORTED;

    config_reg = io[HANTRODEC_SYNTH_CFG_3];
    hw_cfg->hevc_support = (((config_reg >> DWL_HEVC_E) & 0x3U) > 0);
    hw_cfg->vp9_support = (((config_reg >> DWL_VP9_E) & 0x3U) > 0);
    hw_cfg->hevc_main10_support = (((config_reg >> DWL_HEVC_E) & 0x03U) == 2);
    hw_cfg->vp9_10bit_support = (((config_reg >> DWL_VP9_E) & 0x03U) == 2);

    config_reg = io[HANTRODEC_SYNTH_CFG_2];
    hw_cfg->rfc_support = (config_reg >> DWL_RFC_E) & 0x1U;

    config_reg = io[HANTRODECPP_CFG_STAT];
    hw_cfg->pp_support = (config_reg >> DWL_PP_E) & 0x1U;
    hw_cfg->ds_support = (config_reg >> DWL_DS_E) & 0x3U;
    hw_cfg->fmt_p010_support = (config_reg >> DWL_FORMAT_P010_E) & 0x1U;
    hw_cfg->fmt_customer1_support = (config_reg >> DWL_FORMAT_CUSTOMER1_E) & 0x1U;
    hw_cfg->ring_buffer_support = 1; /* Always enabled as default. */
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicConfig
    Description     : Read HW configuration. Does not need a DWL instance to run

    Return type     : DWLHwConfig - structure with HW configuration
------------------------------------------------------------------------------*/
void DWLReadAsicConfig(DWLHwConfig *hw_cfg, u32 client_type) {
  const u32 *io = MAP_FAILED;
  unsigned int reg_size;
  /*unsigned long*/ addr_t base;
  i32 core_id, idx = 0;
  static struct asic_cfg_info asic_cfg_info[2]; /*idx 0:G1, idx 1:G2*/
  struct subsys_desc subsys;

  int fd = (-1), fd_dec = (-1);
#ifdef SUPPORT_VCMD
  struct config_parameter vcmd_params = {0};
  struct cmdbuf_mem_parameter vcmd_mem_params = {0};
  const u32 *tmp = MAP_FAILED;
#endif

  DWL_DEBUG("client_type=%d\n", client_type);
  pthread_mutex_lock(&dwl_asic_read_mutex);

  if ((client_type == DWL_CLIENT_TYPE_PP) && (asic_cfg_info[G1_ASIC_ID_IDX].is_read != 0)) {
    *hw_cfg = asic_cfg_info[idx].cfg;
    goto end;
  } else if (client_type == DWL_CLIENT_TYPE_PP)
    client_type = DWL_CLIENT_TYPE_H264_DEC;

  if ((client_type >= DWL_CLIENT_TYPE_H264_DEC) && (client_type <= DWL_CLIENT_TYPE_VP8_DEC))
    idx = 0;
  else if ((client_type == DWL_CLIENT_TYPE_VP9_DEC) ||
           (client_type == DWL_CLIENT_TYPE_HEVC_DEC) ||
           (client_type == DWL_CLIENT_TYPE_AVS2_DEC) ||
           (client_type == DWL_CLIENT_TYPE_ST_PP))
    idx = 1;
  else
    goto end;

  /*if G1 or G2 asic id has been attained, just return the value*/
  if (asic_cfg_info[idx].is_read != 0) {
    *hw_cfg = asic_cfg_info[idx].cfg;
    goto end;
  }

  asic_cfg_info[idx].is_read = 1;

  fd = open("/dev/mem", O_RDONLY);
  if (fd == -1) {
    DWL_DEBUG("%s", "failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }
  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }
  if (!subsys.subsys_vcmd_num) {
    /*need to get core id*/
    core_id =
      ioctl(fd_dec, HANTRODEC_IOCG_CORE_ID, client_type);

    /* negative value signals an error */
    if (core_id < 0) {
      DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_reserve failed, %d\n",
                "DEC", core_id);
      goto end;
    }

    /* ask module for base */
    base = core_id;
    if (ioctl(fd_dec, HANTRODEC_IOCGHWOFFSET, &base) == -1) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWOFFSET failed\n");
      goto end;
    }

    /* ask module for reg size */
    reg_size = core_id;
    if (ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
      DWL_DEBUG("%s", "ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      goto end;
    }
  } else {
  #ifdef SUPPORT_VCMD
    vcmd_params.module_type = 2; /* VC8000D */
    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &vcmd_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
    }

    if (ioctl(fd_dec, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
              &vcmd_mem_params) == -1) {
      DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto end;
    }

    reg_size = vcmd_mem_params.status_cmdbuf_total_size;
    base = vcmd_mem_params.phy_status_cmdbuf_addr;
#endif
  }

  io = DWLMapRegisters(fd, base, reg_size, 0);
  if (io == MAP_FAILED) {
    DWL_DEBUG("%s", "failed to mmap registers\n");
    goto end;
  }
#ifdef SUPPORT_VCMD
  tmp = io;
  io += vcmd_params.config_status_cmdbuf_id * vcmd_mem_params.status_cmdbuf_unit_size / 4 +
        vcmd_params.submodule_main_addr/2/4;
#endif

  /* Decoder configuration */
  memset(hw_cfg, 0, sizeof(*hw_cfg));

  ReadCoreConfig(io, hw_cfg);
  asic_cfg_info[idx].cfg = *hw_cfg; /*store the value*/

#ifdef SUPPORT_VCMD
  io = tmp;
#endif
  DWLUnmapRegisters(io, reg_size);

end:
  if (fd != -1) close(fd);
  if (fd_dec != -1) close(fd_dec);
  pthread_mutex_unlock(&dwl_asic_read_mutex);
  return;
}

void DWLReadMCAsicConfig(DWLHwConfig hw_cfg[MAX_ASIC_CORES]) {
  const u32 *io = MAP_FAILED;
  unsigned int reg_size;
  unsigned int n_cores, i;
  unsigned long mc_reg_base[MAX_ASIC_CORES];

  int fd = (-1), fd_dec = (-1);

  DWL_DEBUG("%s", "\n");

  fd = open("/dev/mem", O_RDONLY);
  if (fd == -1) {
    DWL_DEBUG("%s", "failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  if (ioctl(fd_dec, HANTRODEC_IOC_MC_CORES, &n_cores) == -1) {
    DWL_DEBUG("%s", "ioctl HANTRODEC_IOC_MC_CORES failed\n");
    goto end;
  }

  assert(n_cores <= MAX_ASIC_CORES);

  if (ioctl(fd_dec, HANTRODEC_IOC_MC_OFFSETS, mc_reg_base) == -1) {
    DWL_DEBUG("%s", "ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
    goto end;
  }

  /* ask module for reg size */
  if (ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("%s", "ioctl failed\n");
    goto end;
  }

  /* Decoder configuration */
  memset(hw_cfg, 0, MAX_ASIC_CORES * sizeof(*hw_cfg));

  for (i = 0; i < n_cores; i++) {
    io = DWLMapRegisters(fd, mc_reg_base[i], reg_size, 0);
    if (io == MAP_FAILED) {
      DWL_DEBUG("%s", "failed to mmap registers\n");
      goto end;
    }

    ReadCoreConfig(io, hw_cfg + i);

    DWLUnmapRegisters(io, reg_size);
  }

end:
  if (fd != -1) close(fd);
  if (fd_dec != -1) close(fd_dec);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicFuseStatus
    Description     : Read HW fuse configuration. Does not need a DWL instance
to run

    Returns     : struct DWLHwFuseStatus * hw_fuse_sts - structure with HW fuse
configuration
------------------------------------------------------------------------------*/
void DWLReadAsicFuseStatus(struct DWLHwFuseStatus *hw_fuse_sts) {
  const u32 *io = MAP_FAILED;

  unsigned long base;
  unsigned int reg_size;

  int fd = (-1), fd_dec = (-1);

  DWL_DEBUG("%s", "\n");

  memset(hw_fuse_sts, 0, sizeof(*hw_fuse_sts));

  fd = open("/dev/mem", O_RDONLY);
  if (fd == -1) {
    DWL_DEBUG("%s", "failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  /* ask module for base */
  if (ioctl(fd_dec, HANTRODEC_IOCGHWOFFSET, &base) == -1) {
    DWL_DEBUG("%s", "ioctl failed\n");
    goto end;
  }

  /* ask module for reg size */
  if (ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("%s", "ioctl failed\n");
    goto end;
  }

  io = DWLMapRegisters(fd, base, reg_size, 0);

  if (io == MAP_FAILED) {
    DWL_DEBUG("%s", "failed to mmap\n");
    goto end;
  }

  /* Decoder fuse configuration */
  ReadCoreFuse(io, hw_fuse_sts);

  DWLUnmapRegisters(io, reg_size);

end:
  if (fd != -1) close(fd);
  if (fd_dec != -1) close(fd_dec);
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : void *info - place where the allocated memory buffer
                        parameters are returned
------------------------------------------------------------------------------*/
i32 DWLMallocRefFrm(const void *instance, u32 size, struct DWLLinearMem *info) {

#ifdef MEMORY_USAGE_TRACE
  printf("DWLMallocRefFrm\t%8d bytes\n", size);
#endif

  return DWLMallocLinear(instance, size, info);
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        DWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : void *info - frame buffer memory information
------------------------------------------------------------------------------*/
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem *info) {
#ifdef MEMORY_USAGE_TRACE
  printf("DWLFreeRefFrm: %8d\n", info->size);
#endif

  DWLFreeLinear(instance, info);
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : void *info - place where the allocated memory buffer
                        parameters are returned
------------------------------------------------------------------------------*/
i32 DWLMallocLinear(const void *instance, u32 size, struct DWLLinearMem *info) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
#ifdef SUPPORT_MMU
  int ioctl_req;
  struct addr_desc addr;
#endif

  u32 pgsize = getpagesize();
  MemallocParams params;

  assert(dec_dwl != NULL);
  assert(info != NULL);

#ifdef MEMORY_USAGE_TRACE
  printf("DWLMallocLinear\t%8d bytes \n", size);
#endif

  info->logical_size = size;
  size = NEXT_MULTIPLE(size, MAX(DEC_X170_BUS_ADDR_ALIGNMENT, pgsize));
  info->size = size;
  info->virtual_address = MAP_FAILED;
  info->bus_address = 0;

  params.size = info->size;

  /* get memory linear memory buffers */
  ioctl(dec_dwl->fd_memalloc, MEMALLOC_IOCXGETBUFFER, &params);
  if (params.bus_address == 0) {
    DWL_DEBUG("%s", "ERROR! No linear buffer available\n");
    return DWL_ERROR;
  }

  /* The bus address for mmap and HW may be different. translation_offset
   * is used to calculate the bus address for HW access. If no translation is
   * needed memalloc-driver sets it to 0 */
  info->bus_address = params.bus_address - params.translation_offset;

  /* Map the bus address to virtual address */
  info->virtual_address =
    (u32 *)mmap(0, info->size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED,
                dec_dwl->fd_mem, params.bus_address);

#ifdef SUPPORT_MMU
  addr.virtual_address = info->virtual_address;
  addr.size            = info->size;

  mlock(addr.virtual_address, info->size);
  ioctl_req = (int)HANTRO_IOCS_MMU_MEM_MAP;
  ioctl(dec_dwl->fd, ioctl_req, &addr);
  info->bus_address = addr.bus_address;
#endif

#ifdef MEMORY_USAGE_TRACE
#ifdef __FREERTOS__
  printf("DWLMallocLinear 0x%llx virtual_address: 0x%p (type %d)\n", info->bus_address,
         info->virtual_address, info->mem_type);
#else
  printf("DWLMallocLinear 0x%zx virtual_address: 0x%p (type %d)\n", info->bus_address,
         info->virtual_address, info->mem_type);
#endif
#endif

  if (info->virtual_address == MAP_FAILED) return DWL_ERROR;

#ifdef _DWL_MEMSET_BUFFER
  DWLmemset(info->virtual_address, 0x80, info->size);
#endif

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        DWLMallocLinear.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : void *info - linear buffer memory information
------------------------------------------------------------------------------*/
void DWLFreeLinear(const void *instance, struct DWLLinearMem *info) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
#ifdef SUPPORT_MMU
  struct addr_desc addr;
  int ioctl_req;
#endif
  assert(dec_dwl != NULL);
  assert(info != NULL);

#ifdef MEMORY_USAGE_TRACE
  printf("DWLFreeLinear: %8d\n", info->size);
#endif

#ifdef SUPPORT_MMU
  addr.virtual_address = info->virtual_address;

  ioctl_req = (int)HANTRO_IOCS_MMU_MEM_UNMAP;
  ioctl(dec_dwl->fd, ioctl_req, &addr);
#endif

  if (info->bus_address != 0)
    ioctl(dec_dwl->fd_memalloc, MEMALLOC_IOCSFREEBUFFER, &info->bus_address);

  if (info->virtual_address != MAP_FAILED)
    munmap(info->virtual_address, info->size);
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteReg
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/

void DWLWriteReg(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, value);
#endif
  assert(dec_dwl != NULL);
  assert(CheckRegOffset(dec_dwl, offset));
  assert(core_id < (i32)dec_dwl->num_cores);

  offset = offset / 4;

  dwl_shadow_regs[core_id][offset] = value;

#ifdef INTERNAL_TEST
  InternalTestDumpWriteSwReg(core_id, offset, value, dwl_shadow_regs[core_id]);
#endif
  (void)dec_dwl;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 val;

  assert(dec_dwl != NULL);

  assert(CheckRegOffset(dec_dwl, offset));
  assert(core_id < (i32)dec_dwl->num_cores);

  offset = offset / 4;

  val = dwl_shadow_regs[core_id][offset];

#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset,
            offset * 4, val);
#endif
#ifdef INTERNAL_TEST
  InternalTestDumpReadSwReg(core_id, offset, val, dwl_shadow_regs[core_id]);
#endif
  (void)dec_dwl;
  return val;
}

#ifdef SUPPORT_CACHE

#define SHAPER_REG_OFFSET 0x8
#define CACHE_REG_OFFSET 0x80

extern u32 cache_shadow_regs[];
extern u32 shaper_shadow_regs[];
extern u32 cache_exception_lists[];
extern u32 cache_exception_regs_num;

#endif

void DWLWriteCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  core.id = subsys_id;
  core.regs = regs;
  core.reg_id = reg_id;
  core.size = count * 4;
  core.type = type;

  ASSERT(dec_dwl);
  ASSERT(regs);

  if (ioctl(dec_dwl->fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOCS_DEC_WRITE_REG failed\n");
    ASSERT(0);
  }
}

void DWLReadCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  core.id = subsys_id;
  core.regs = regs;
  core.reg_id = reg_id;
  core.size = count * 4;
  core.type = type;

  ASSERT(dec_dwl);
  ASSERT(regs);

  if (ioctl(dec_dwl->fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
    ASSERT(0);
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLEnableHw
    Description     : Enable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  int ioctl_req;
#ifdef SUPPORT_CACHE
  u32 tile_by_tile;
  u32 no_chroma = 0;
  int max_wait_time = 10000; /* 10s in ms */
  u32 version = DWLReadCacheVersion();
  u32 cache_reg_num, shaper_reg_num;
#endif

  assert(dec_dwl);
#ifdef SUPPORT_CACHE
  cache_exception_regs_num = 0;
  if (((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_HEVC ||
      ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_VP9 ||
      ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_H264_H10P ||
      ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_AVS2)
    DWLWriteReg(dec_dwl, core_id, 4*3, (dwl_shadow_regs[core_id][3] | 0x08));
  DWLWriteReg(dec_dwl, core_id, 4*265, (dwl_shadow_regs[core_id][265] | 0x80000000));
  if ((DWLReadReg(dec_dwl, core_id, 4*58) & 0xFF) > 16)
    DWLWriteReg(dec_dwl, core_id, 4*58, ((dwl_shadow_regs[core_id][58] & 0xFFFFFF00) | 0x10));
  if (version >= 0x5)
    DWLWriteReg(dec_dwl, core_id, 4*58, (dwl_shadow_regs[core_id][58] | 0x4000));
  tile_by_tile = dec_dwl->tile_by_tile;
  if (tile_by_tile) {
    if (dec_dwl->tile_id) {
      if (dec_dwl->tile_id < dec_dwl->num_tiles)
        DWLDisableCacheChannelALL(WR, core_id);
      cache_enable[core_id] = 0;
    }
  }
  do {
    const unsigned int usec = 1000; /* 1 ms polling interval */
    if (cache_enable[core_id] == 0)
      break;
    usleep(usec);

    max_wait_time--;
  } while (max_wait_time > 0);
  if (!tile_by_tile) {
    DWLConfigureCacheChannel(instance, core_id, dec_dwl->cache_cfg, dec_dwl->pjpeg_coeff_buffer_size);
      if (!dec_dwl->shaper_enable[core_id]) {
      u32 pp_enabled = dec_dwl->ppu_cfg[0].enabled |
                       dec_dwl->ppu_cfg[1].enabled |
                       dec_dwl->ppu_cfg[2].enabled |
                       dec_dwl->ppu_cfg[3].enabled |
                       dec_dwl->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dec_dwl->ppu_cfg, pp_enabled, tile_by_tile, dec_dwl->tile_id, &(dec_dwl->num_tiles), dec_dwl->tiles, &no_chroma);
    }
  } else {
    if (!dec_dwl->tile_id)
      DWLConfigureCacheChannel(instance, core_id, dec_dwl->cache_cfg, dec_dwl->pjpeg_coeff_buffer_size);
    if (!dec_dwl->shaper_enable[core_id]) {
      u32 pp_enabled = dec_dwl->ppu_cfg[0].enabled |
                       dec_dwl->ppu_cfg[1].enabled |
                       dec_dwl->ppu_cfg[2].enabled |
                       dec_dwl->ppu_cfg[3].enabled |
                       dec_dwl->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dec_dwl->ppu_cfg, pp_enabled, tile_by_tile, dec_dwl->tile_id, &(dec_dwl->num_tiles), dec_dwl->tiles, &no_chroma);
      dec_dwl->tile_id++;
    }
  }
  cache_enable[core_id] = 1;
  if (no_chroma || dec_dwl->shaper_enable[core_id]) {
    DWLWriteReg(dec_dwl, core_id, 4*3, (dwl_shadow_regs[core_id][3] & 0xFFFFFFF7));
    DWLWriteReg(dec_dwl, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));
  }
  if ((((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_JPEG) &&
      !dec_dwl->ppu_cfg[0].shaper_enabled && !dec_dwl->ppu_cfg[1].shaper_enabled &&
      !dec_dwl->ppu_cfg[2].shaper_enabled && !dec_dwl->ppu_cfg[3].shaper_enabled &&
      !dec_dwl->ppu_cfg[4].shaper_enabled)
    DWLWriteReg(dec_dwl, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));

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
  {
    DWLDisableCacheChannelALL(BI, core_id);
    dec_dwl->shaper_enable[core_id] = 0;
    cache_enable[core_id] = 0;

    /* Append write L2Cache/shaper instructions. */
    /* For PJPEG interim scan, disable cache/shaper. */
    if (IS_PJPEG_INTER_SCAN(dwl_shadow_regs[0][3])) {
      cache_shadow_regs[1] = 0;
      DWLWriteCoreRegs(dec_dwl, core_id, &cache_shadow_regs[1], CACHE_REG_OFFSET + 1, 1, HW_L2CACHE);
      cache_reg_num = 0;
    } else if (cache_reg_num) {
        cache_shadow_regs[1] &= ~1;
        if (cache_exception_regs_num)
          cache_shadow_regs[2] |= 0x2;
        DWLWriteCoreRegs(dec_dwl, core_id, &cache_shadow_regs[1], CACHE_REG_OFFSET + 1, 2, HW_L2CACHE);

      if (cache_exception_regs_num) {
        /* exception list */
        int i = 0;

        for (i = 0; i < cache_exception_regs_num; i++) {
          DWLWriteCoreRegs(dec_dwl, core_id, &cache_exception_lists[i], CACHE_REG_OFFSET + 3, 1, HW_L2CACHE);
        }
      }
      /* Enable cache */
      cache_shadow_regs[1] |= 1;
      DWLWriteCoreRegs(dec_dwl, core_id, &cache_shadow_regs[1], CACHE_REG_OFFSET + 1, 1, HW_L2CACHE);
    }

    /* Configure shaper instruction */
    if (IS_PJPEG_INTER_SCAN(dwl_shadow_regs[0][3])) {
      /* Disable shaper for pjpeg interim scan. */
      shaper_shadow_regs[0] = 0;
      DWLWriteCoreRegs(dec_dwl, core_id, &shaper_shadow_regs[0], SHAPER_REG_OFFSET, 1, HW_L2CACHE);
      shaper_reg_num = 0;
    } else if (shaper_reg_num) {
      DWLWriteCoreRegs(dec_dwl, core_id, &shaper_shadow_regs[1], SHAPER_REG_OFFSET + 1,
                       shaper_reg_num - 1, HW_L2CACHE);

      /* Enable shaper */
      DWLWriteCoreRegs(dec_dwl, core_id, &shaper_shadow_regs[0], SHAPER_REG_OFFSET, 1, HW_L2CACHE);
    }
  }
#endif

  pthread_mutex_lock(&dec_dwl->shadow_mutex);
  ioctl_req = HANTRODEC_IOCS_DEC_PUSH_REG;

  //DWLWriteReg(dec_dwl, core_id, 8, 0x400);
  //DWLWriteReg(dec_dwl, core_id, 4*58, 0x6210);
  DWLWriteReg(dec_dwl, core_id, offset, value);
  //dwl_shadow_regs[core_id][13] = dwl_shadow_regs[core_id][326];

  DWL_DEBUG("%s %d enabled by previous DWLWriteReg\n", "DEC", core_id);

  core.id = core_id;
  core.regs = dwl_shadow_regs[core_id];
  core.size = dec_dwl->reg_size;
  core.type = HW_VC8000D;
  core.reg_id = 0;

#ifdef PERFORMANCE_TEST
  u32 i;
  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (hw_enable[i])
      break;
  }

  if (i == MAX_ASIC_CORES)
    ActivityTraceStartDec(&dec_dwl->activity);
#endif

  PERFORMANCE_STATIC_END(decode_pre_hw);
  PERFORMANCE_STATIC_START(decode_push_reg);
  if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    assert(0);
  }
  PERFORMANCE_STATIC_END(decode_push_reg);
  hw_enable[core_id] = 1;

  dec_dwl->core_usage_counts[core_id]++;

  pthread_mutex_unlock(&dec_dwl->shadow_mutex);
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegToHw
    Description     : Write a value to a hardware IO register(when hardware is running)

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value)
{
    struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
    struct core_desc core;
    int ioctl_req;

    assert(dec_dwl);

    ioctl_req = HANTRODEC_IOCS_DEC_WRITE_REG;

    pthread_mutex_lock(&dec_dwl->shadow_mutex);
#ifdef SUPPORT_CACHE
    if ((((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_HEVC ||
       ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_VP9 ||
       ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_H264_H10P ||
       ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_AVS2) &&
       (offset == 3 * 4))
      value |= 0x08;
#endif

    DWLWriteReg(dec_dwl, core_id, offset, value);

    core.id = core_id;
    core.reg_id = offset / 4;
    core.regs = &dwl_shadow_regs[core_id][core.reg_id];
    core.size = 4;
    core.type = HW_VC8000D;

    if(ioctl(dec_dwl->fd, ioctl_req, &core))
    {
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
        assert(0);
    }
    pthread_mutex_unlock(&dec_dwl->shadow_mutex);
}


/*------------------------------------------------------------------------------
    Function name   : DWLReadRegFromHw
    Description     : Read the value of a hardware IO register(when hardware is running)

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/

u32 DWLReadRegFromHw(const void *instance, i32 core_id, u32 offset)
{
    struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
    struct core_desc core;
    int ioctl_req;
    u32 value;

    assert(dec_dwl);

    ioctl_req = (int)HANTRODEC_IOCS_DEC_READ_REG;

    pthread_mutex_lock(&dec_dwl->shadow_mutex);

    core.id = core_id;
    core.reg_id = offset / 4;
    core.regs = &dwl_shadow_regs[core_id][core.reg_id];
    core.size = 4;
    core.type = HW_VC8000D;

    if(ioctl(dec_dwl->fd, ioctl_req, &core))
    {
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
        assert(0);
    }

    value = DWLReadReg(instance, core_id, offset);
    pthread_mutex_unlock(&dec_dwl->shadow_mutex);
    return value;
}


/*------------------------------------------------------------------------------
    Function name   : DWLDisableHw
    Description     : Disable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  int ioctl_req;

  assert(dec_dwl);

  ioctl_req = HANTRODEC_IOCS_DEC_PUSH_REG;

  DWLWriteReg(dec_dwl, core_id, offset, value);

  DWL_DEBUG("%s %d disabled by previous DWLWriteReg\n", "DEC", core_id);

  core.id = core_id;
  core.regs = dwl_shadow_regs[core_id];
  core.size = dec_dwl->reg_size;
  core.type = HW_VC8000D;
  core.reg_id = 0;

  if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
    DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    assert(0);
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitHwReady
    Description     : Wait until hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.

    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
i32 DWLWaitHwReady(const void *instance, i32 core_id, u32 timeout) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  i32 ret = DWL_HW_WAIT_OK;

#ifndef DWL_USE_DEC_IRQ
  struct core_desc core;
  int ioctl_req;
  /* XXX: when decoing large res JPEG/HEVC on FPGA platform in polling mode,
  this value maybe not enough, setting to 60000(60s) for FPGA */
  int max_wait_time = 60000; /* 60s in ms */

  core.id = core_id;
  core.regs = &dwl_shadow_regs[core_id][1];
  core.size = 4;  /* only read 1 register */
#endif

  assert(dec_dwl);

  DWL_DEBUG("%s %d\n", "DEC", core_id);

#ifdef DWL_USE_DEC_IRQ
#if 0
  /*wait for interrupt from specified core*/
  ioctl_req = (int) HANTRODEC_IOCX_DEC_WAIT;
  DWL_DEBUG("%s", "DWLWaitHwReady wait irq\n");
  if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
    DWL_DEBUG("%s", "ioctl HANTRODEC_IOCG_*_WAIT failed\n");
    ret = DWL_HW_WAIT_ERROR;
  }
#else
  sem_wait(dec_dwl->sync_params->sc_dec_rdy_sem + core_id);
#endif
#else /* Polling */

  ret = DWL_HW_WAIT_TIMEOUT;

  ioctl_req = (int)HANTRODEC_IOCS_DEC_READ_REG;
  core.reg_id = 1;

  do {
    u32 irq_stats;
    const unsigned int usec = 1000; /* 1 ms polling interval */

    /* Just read swreg[1] to check hw status */
    pthread_mutex_lock(&dec_dwl->shadow_mutex);

    if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
      pthread_mutex_unlock(&dec_dwl->shadow_mutex);
      DWL_DEBUG("%s","ioctl HANTRODEC_IOCS_*_READ_REG failed\n");
      ret = DWL_HW_WAIT_ERROR;
      break;
    }

    irq_stats =  dwl_shadow_regs[core_id][HANTRODEC_IRQ_STAT_DEC];
    pthread_mutex_unlock(&dec_dwl->shadow_mutex);
    irq_stats = (irq_stats >> 11) & 0x5FFF;

    /* Pull all registers when hw fininshed */
    if (irq_stats != 0) {
      hw_enable[core_id] = 0;
#ifdef PERFORMANCE_TEST
      ActivityTraceStopDec(&dec_dwl->activity);
      hw_time_use = dec_dwl->activity.active_time / 100;
#endif
      ret = DWL_HW_WAIT_OK;
      core.regs = &dwl_shadow_regs[core_id][0];
      ioctl_req = (int)HANTRODEC_IOCS_DEC_PULL_REG;
      pthread_mutex_lock(&dec_dwl->shadow_mutex);
      PERFORMANCE_STATIC_START(decode_pull_reg);
      if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
        pthread_mutex_unlock(&dec_dwl->shadow_mutex);
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PULL_REG failed\n");
        ret = DWL_HW_WAIT_ERROR;
        break;
      }
      PERFORMANCE_STATIC_END(decode_pull_reg);
      pthread_mutex_unlock(&(dec_dwl->shadow_mutex));
      if (irq_stats & DEC_HW_IRQ_BUFFER)
        DWLReleaseL2(dec_dwl, core_id);
      break;
    }

    usleep(usec);

    max_wait_time--;
  } while (max_wait_time > 0);

#endif

#ifdef SUPPORT_CACHE
  {
  u32 cache_shadow_regs[3];
  u32 shaper_shadow_regs[3];

  /* Disable cache */
  cache_shadow_regs[1] = 0;
  cache_shadow_regs[2] = 0;
  DWLWriteCoreRegs(dec_dwl, core_id, &cache_shadow_regs[1], CACHE_REG_OFFSET+1, 2, HW_L2CACHE);

  /* Disable shaper to flush it only when it's enabled. */
  shaper_shadow_regs[0] = 0;
  DWLWriteCoreRegs(dec_dwl, core_id, &shaper_shadow_regs[0], SHAPER_REG_OFFSET, 1, HW_L2CACHE);

  /* Wait interrupt from shaper */
  while (1) {
    DWLReadCoreRegs(dec_dwl, core_id, &shaper_shadow_regs[1], SHAPER_REG_OFFSET+3, 1, HW_L2CACHE);
    if (shaper_shadow_regs[1] & 0x2)  /* Check bit1 of shaper_swreg[3]*/
      break;
    usleep(10);
  }
}
#endif

  PERFORMANCE_STATIC_START(decode_post_hw);
#ifdef _DWL_DEBUG
  {
    u32 irq_stats =  dwl_shadow_regs[core_id][HANTRODEC_IRQ_STAT_DEC];

    PrintIrqType(core_id, irq_stats);
  }
#endif

  DWL_DEBUG("%s %d done\n", "DEC", core_id);

  (void) timeout;

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : DWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *DWLmalloc(u32 n) {
#ifdef MEMORY_USAGE_TRACE
  printf("DWLmalloc\t%8d bytes\n", n);
#endif
  return malloc((size_t)n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()

    Return type     : void

    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void DWLfree(void *p) {
  if (p != NULL) free(p);
}

/*------------------------------------------------------------------------------
    Function name   : DWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

}
    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *DWLcalloc(u32 n, u32 s) {
#ifdef MEMORY_USAGE_TRACE
  printf("DWLcalloc\t%8d bytes\n", n * s);
#endif
  return calloc((size_t)n, (size_t)s);
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()

    Return type     : The value of destination d

    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *DWLmemcpy(void *d, const void *s, u32 n) {
  return memcpy(d, s, (size_t)n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()

    Return type     : The value of destination d

    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
void *DWLmemset(void *d, i32 c, u32 n) {
  return memset(d, (int)c, (size_t)n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLFakeTimeout
    Description     : Testing help function that changes HW stream errors info
                        HW timeouts. You can check how the SW behaves or not.
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/

#ifdef _DWL_FAKE_HW_TIMEOUT
void DWLFakeTimeout(u32 *status) {

  if ((*status) & DEC_IRQ_ERROR) {
    *status &= ~DEC_IRQ_ERROR;
    *status |= DEC_IRQ_TIMEOUT;
    printf("\nDwl: Change stream error to hw timeout\n");
  }
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLReadPpConfigure
    Description     : Read the pp configure
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : PpUnitIntConfig *ppu_cfg
------------------------------------------------------------------------------*/
void DWLReadPpConfigure(const void *instance, void *ppu_cfg, u16* tiles, u32 tile_enable)
{
    struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
    dec_dwl->tiles = tiles;
    if (dec_dwl->client_type == DWL_CLIENT_TYPE_JPEG_DEC) {
      dec_dwl->pjpeg_coeff_buffer_size = tile_enable;
      dec_dwl->tile_by_tile = 0;
    } else
      dec_dwl->tile_by_tile = tile_enable;
    DWLmemcpy(dec_dwl->ppu_cfg, ppu_cfg, DEC_MAX_OUT_COUNT*sizeof(PpUnitIntConfig));
}

void DWLReadMcRefBuffer(const void *instance, u32 core_id, u64 buf_size, u32 dpb_size) {
    struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
#if SUPPORT_CACHE
    dec_dwl->cache_cfg[core_id].buf_size = buf_size;
    dec_dwl->cache_cfg[core_id].dpb_size = dpb_size;
#endif
    UNUSED(dec_dwl);
}

u32 DWLVcmdCores(void) {
  int fd_dec;
  struct subsys_desc subsys;

  /* open driver */
  fd_dec = open(DEC_MODULE_PATH, O_RDWR);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    return 0;
  }
  if (ioctl(fd_dec, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DWL_DEBUG("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    goto end;
  }

end:
  if (fd_dec != -1) close(fd_dec);

  return subsys.subsys_vcmd_num;
}

