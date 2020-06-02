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
#include "dwl_defs.h"
#include "dwl_syncobj_osfree.h"
#include "dwl.h"
#include "dwlthread.h"
#include "hantrodec_osfree.h"
#include "memalloc.h"
#include "deccfg.h"
#include "asic.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#ifdef _USE_VSI_STRING_
#include "vsi_string.h"
#else
#include <string.h>
#endif
#include <malloc.h>   /* for memalign() */

/* macro for assertion, used only when _ASSERT_USED is defined */
#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

/*
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <unistd.h>
*/

/* the decoder device driver nod */
const char *dec_dev = DEC_MODULE_PATH;
/* the memalloc device driver nod */
const char *mem_dev = MEMALLOC_MODULE_PATH;


#ifdef _SYSTEM_MODEL_
#include "tb_cfg.h"
extern struct TBCfg tb_cfg;
extern u32 g_hw_ver;
extern u32 h264_high_support;

#include "asic.h"
void *g_sysmodel_core[MAX_ASIC_CORES];
#endif

#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#define DWL_PJPEG_E         22  /* 1 bit */
#define DWL_REF_BUFF_E      20  /* 1 bit */

#define DWL_JPEG_EXT_E          31  /* 1 bit */
#define DWL_REF_BUFF_ILACE_E    30  /* 1 bit */
#define DWL_MPEG4_CUSTOM_E      29  /* 1 bit */
#define DWL_REF_BUFF_DOUBLE_E   28  /* 1 bit */

#define DWL_MVC_E           20  /* 2 bits */

#define DWL_DEC_TILED_L     17  /* 2 bits */
#define DWL_DEC_PIC_W_EXT   14  /* 2 bits */
#define DWL_EC_E            12  /* 2 bits */
#define DWL_STRIDE_E        11  /* 1 bit */
#define DWL_FIELD_DPB_E     10  /* 1 bit */
#define DWL_AVS_PLUS_E       6  /* 1 bit */
#define DWL_64BIT_ENV_E      5  /* 1 bit */

#define DWL_CFG_E           24  /* 4 bits */
#define DWL_PP_IN_TILED_L   14  /* 2 bits */

#define DWL_SORENSONSPARK_E 11  /* 1 bit */

#define DWL_H264_FUSE_E          31 /* 1 bit */
#define DWL_MPEG4_FUSE_E         30 /* 1 bit */
#define DWL_MPEG2_FUSE_E         29 /* 1 bit */
#define DWL_SORENSONSPARK_FUSE_E 28 /* 1 bit */
#define DWL_JPEG_FUSE_E          27 /* 1 bit */
#define DWL_VP6_FUSE_E           26 /* 1 bit */
#define DWL_VC1_FUSE_E           25 /* 1 bit */
#define DWL_PJPEG_FUSE_E         24 /* 1 bit */
#define DWL_CUSTOM_MPEG4_FUSE_E  23 /* 1 bit */
#define DWL_RV_FUSE_E            22 /* 1 bit */
#define DWL_VP7_FUSE_E           21 /* 1 bit */
#define DWL_VP8_FUSE_E           20 /* 1 bit */
#define DWL_AVS_FUSE_E           19 /* 1 bit */
#define DWL_MVC_FUSE_E           18 /* 1 bit */

#define DWL_DEC_MAX_1920_FUSE_E  15 /* 1 bit */
#define DWL_DEC_MAX_1280_FUSE_E  14 /* 1 bit */
#define DWL_DEC_MAX_720_FUSE_E   13 /* 1 bit */
#define DWL_DEC_MAX_352_FUSE_E   12 /* 1 bit */
#define DWL_REF_BUFF_FUSE_E       7 /* 1 bit */


#define DWL_PP_FUSE_E       31  /* 1 bit */
#define DWL_PP_DEINTERLACE_FUSE_E   30  /* 1 bit */
#define DWL_PP_ALPHA_BLEND_FUSE_E   29  /* 1 bit */
#define DWL_PP_MAX_4096_FUSE_E    16  /* 1 bit */
#define DWL_PP_MAX_1920_FUSE_E    15  /* 1 bit */
#define DWL_PP_MAX_1280_FUSE_E    14  /* 1 bit */
#define DWL_PP_MAX_720_FUSE_E   13  /* 1 bit */
#define DWL_PP_MAX_352_FUSE_E   12  /* 1 bit */

#ifdef _DWL_FAKE_HW_TIMEOUT
static void DWLFakeTimeout(u32 * status);
#endif

#define IS_PIPELINE_ENABLED(val)    ((val) & 0x02)

/* shadow HW registers */
u32 dwl_shadow_regs[MAX_ASIC_CORES][512] = {0};

/* shadow id/config registers */
u32 dwl_shadow_config_regs[MAX_ASIC_CORES][512] = {0};
u32 hw_enable[MAX_ASIC_CORES] = {0};

static av_unused void PrintIrqType(u32 is_pp, u32 core_id, u32 status) {
  if(is_pp) {
    printf("PP[%d] IRQ %s\n", core_id,
           status & PP_IRQ_RDY ? "READY" : "BUS ERROR");
  } else {
    if(status & DEC_IRQ_ABORT)
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
    else
      printf("DEC[%d] IRQ UNKNOWN 0x%08x\n", core_id, status);
  }
}

/*------------------------------------------------------------------------------*/
int __init hantrodec_init(void);
int g1_sim_open(const char* name, int flag) {
  if(!strcmp(name, DEC_MODULE_PATH)) {
    hantrodec_init();
  }
  return 1;
}

void g1_sim_close(int fd) {
}

long g1_sim_ioctl(/*struct file * */ int filp, unsigned int cmd, unsigned long arg) {
  return hantrodec_ioctl(filp, cmd, arg);
}

/*------------------------------------------------------------------------------
    Function name   : DWLMapRegisters
    Description     :

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
static av_unused u32 *DWLMapRegisters(int mem_dev, unsigned long base,
                            unsigned int reg_size, u32 write) {
  u32* io;

  io = (u32*)base;

  return io;
}

void DWLUnmapRegisters(const void *io, unsigned int reg_size) {
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicCoreCount
    Description     : Return the number of hardware cores available
------------------------------------------------------------------------------*/
u32 DWLReadAsicCoreCount(void) {
  int fd_dec;
  unsigned int cores = 0;

  /* open driver */
  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if (fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    return 0;
  }

  /* ask module for cores */
  if (ioctl(fd_dec, HANTRODEC_IOC_MC_CORES, &cores) == -1) {
    DWL_DEBUG("ioctl failed\n","");
    cores = 0;
  }

  if (fd_dec != -1)
    close(fd_dec);

  return (u32)cores;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadAsicID(u32 client_type) {

#ifdef _SYSTEM_MODEL_
  u32 build = 0;

  /* Set HW info from TB config */
#if defined(DWL_EVALUATION_8170)
  g_hw_ver = 8170;
#elif defined(DWL_EVALUATION_8190)
  g_hw_ver = 8190;
#elif defined(DWL_EVALUATION_9170)
  g_hw_ver = 9170;
#elif defined(DWL_EVALUATION_9190)
  g_hw_ver = 9190;
#elif defined(DWL_EVALUATION_G1)
  g_hw_ver = 10000;
#else
  g_hw_ver = g_hw_ver ? g_hw_ver : 18001;
#endif
  build = g_hw_id;

  build = ( build/1000)     *0x1000 +
          ((build/100)%10) *0x100 +
          ((build/10)%10)  *0x10 +
          ((build)%10)     *0x1;

  switch (g_hw_ver) {
  case 8190:
    return 0x81900000 + build;
  case 9170:
    return 0x91700000 + build;
  case 9190:
    return 0x91900000 + build;
  case 10000:
    return 0x67310000 + build;  /* G1 */
  case 10001:
    return 0x67320000 + build;  /* G2 */
  default:
    return 0x80010000 + build;  /* VC8000D */
  }
#else
  u32 *io = MAP_FAILED, id = ~0;
  int fd_dec = -1, fd = 0;
  unsigned long base = 0;
  unsigned int reg_size = 0;

  DWL_DEBUG("\n");

  /* id from shadow regs */
  if(dwl_shadow_config_regs[0][0] != 0x00000000)
    return (u32)dwl_shadow_config_regs[0][0];

  fd = open("/dev/mem", O_RDONLY);
  if(fd == -1) {
    DWL_DEBUG("failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if(fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  /* ask module for base */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWOFFSET, &base) == -1) {
    DWL_DEBUG("ioctl failed\n");
    goto end;
  }

  /* ask module for reg size */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("ioctl failed\n");
    goto end;
  }

  io = DWLMapRegisters(fd, base, reg_size, 0);

  if(io == MAP_FAILED) {
    DWL_DEBUG("failed to mmap regs.");
    goto end;
  }

  /* to shadow regs */
  id = dwl_shadow_config_regs[0][0] = io[0];

  DWLUnmapRegisters(io, reg_size);

end:
  if(fd != -1)
    close(fd);
  if(fd_dec != -1)
    close(fd_dec);

  return id;
#endif
}

/* HW Build ID is for feature list query */
/* For G1/G2, HwBuildId is reg0; otherwise it's reg309. */
u32 DWLReadHwBuildID(u32 client_type) {
  g_hw_ver = g_hw_ver ? g_hw_ver : 18001;

  if (g_hw_ver == 10000 || g_hw_ver == 10001)
    return DWLReadAsicID(client_type);
  else
    return g_hw_build_id;
}

u32 DWLReadCoreHwBuildID(u32 core_id) {
  return g_hw_build_id;
}

static void ReadCoreFuse(const u32 *io, struct DWLHwFuseStatus *hw_fuse_sts) {
 
#ifdef _SYSTEM_MODEL_
  ASSERT(hw_fuse_sts != NULL);
  /* Maximum decoding width supported by the HW (fuse) */
  hw_fuse_sts->max_dec_pic_width_fuse = 4096;
  /* Maximum output width of Post-Processor (fuse) */
  hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;

  hw_fuse_sts->h264_support_fuse = H264_FUSE_ENABLED;    /* HW supports H264 */
  hw_fuse_sts->mpeg4_support_fuse = MPEG4_FUSE_ENABLED;  /* HW supports MPEG-4 */
  /* HW supports MPEG-2/MPEG-1 */
  hw_fuse_sts->mpeg2_support_fuse = MPEG2_FUSE_ENABLED;
  /* HW supports Sorenson Spark */
  hw_fuse_sts->sorenson_spark_support_fuse = SORENSON_SPARK_ENABLED;
  /* HW supports baseline JPEG */
  hw_fuse_sts->jpeg_support_fuse = JPEG_FUSE_ENABLED;
  hw_fuse_sts->vp6_support_fuse = VP6_FUSE_ENABLED;  /* HW supports VP6 */
  hw_fuse_sts->vc1_support_fuse = VC1_FUSE_ENABLED;  /* HW supports VC-1 */
  /* HW supports progressive JPEG */
  hw_fuse_sts->jpeg_prog_support_fuse = JPEG_PROGRESSIVE_FUSE_ENABLED;
  hw_fuse_sts->ref_buf_support_fuse = REF_BUF_FUSE_ENABLED;
  hw_fuse_sts->avs_support_fuse = AVS_FUSE_ENABLED;
  hw_fuse_sts->rv_support_fuse = RV_FUSE_ENABLED;
  hw_fuse_sts->vp7_support_fuse = VP7_FUSE_ENABLED;
  hw_fuse_sts->vp8_support_fuse = VP8_FUSE_ENABLED;
  hw_fuse_sts->mvc_support_fuse = MVC_FUSE_ENABLED;

  /* PP fuses */
  hw_fuse_sts->pp_support_fuse = PP_FUSE_ENABLED;    /* HW supports PP */
  /* PP fuse has all optional functions */
  hw_fuse_sts->pp_config_fuse = PP_FUSE_DEINTERLACING_ENABLED |
                                PP_FUSE_ALPHA_BLENDING_ENABLED |
                                MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED;
#else
  u32 config_reg = 0, fuse_reg = 0, fuse_reg_pp = 0;

  /* Decoder configuration */
  config_reg = io[HANTRODEC_SYNTH_CFG];

  /* Decoder fuse configuration */
  fuse_reg = io[HANTRODEC_FUSE_CFG];

  hw_fuse_sts->h264_support_fuse = (fuse_reg >> DWL_H264_FUSE_E) & 0x01U;
  hw_fuse_sts->mpeg4_support_fuse = (fuse_reg >> DWL_MPEG4_FUSE_E) & 0x01U;
  hw_fuse_sts->mpeg2_support_fuse = (fuse_reg >> DWL_MPEG2_FUSE_E) & 0x01U;
  hw_fuse_sts->sorenson_spark_support_fuse =
    (fuse_reg >> DWL_SORENSONSPARK_FUSE_E) & 0x01U;
  hw_fuse_sts->jpeg_support_fuse = (fuse_reg >> DWL_JPEG_FUSE_E) & 0x01U;
  hw_fuse_sts->vp6_support_fuse = (fuse_reg >> DWL_VP6_FUSE_E) & 0x01U;
  hw_fuse_sts->vc1_support_fuse = (fuse_reg >> DWL_VC1_FUSE_E) & 0x01U;
  hw_fuse_sts->jpeg_prog_support_fuse = (fuse_reg >> DWL_PJPEG_FUSE_E) & 0x01U;
  hw_fuse_sts->rv_support_fuse = (fuse_reg >> DWL_RV_FUSE_E) & 0x01U;
  hw_fuse_sts->avs_support_fuse = (fuse_reg >> DWL_AVS_FUSE_E) & 0x01U;
  hw_fuse_sts->vp7_support_fuse = (fuse_reg >> DWL_VP7_FUSE_E) & 0x01U;
  hw_fuse_sts->vp8_support_fuse = (fuse_reg >> DWL_VP8_FUSE_E) & 0x01U;
  hw_fuse_sts->custom_mpeg4_support_fuse = (fuse_reg >> DWL_CUSTOM_MPEG4_FUSE_E) & 0x01U;
  hw_fuse_sts->mvc_support_fuse = (fuse_reg >> DWL_MVC_FUSE_E) & 0x01U;

  /* check max. decoder output width */
  if(fuse_reg & 0x10000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 4096;
  else if(fuse_reg & 0x8000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 1920;
  else if(fuse_reg & 0x4000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 1280;
  else if(fuse_reg & 0x2000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 720;
  else if(fuse_reg & 0x1000U)
    hw_fuse_sts->max_dec_pic_width_fuse = 352;

  hw_fuse_sts->ref_buf_support_fuse = (fuse_reg >> DWL_REF_BUFF_FUSE_E) & 0x01U;

  /* Pp configuration */
  config_reg = io[HANTROPP_SYNTH_CFG];

  if((config_reg >> DWL_G1_PP_E) & 0x01U) {
    /* Pp fuse configuration */
    fuse_reg_pp = io[HANTROPP_FUSE_CFG];

    if((fuse_reg_pp >> DWL_PP_FUSE_E) & 0x01U) {
      hw_fuse_sts->pp_support_fuse = 1;

      /* check max. pp output width */
      if(fuse_reg_pp & 0x10000U)
        hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;
      else if(fuse_reg_pp & 0x8000U)
        hw_fuse_sts->max_pp_out_pic_width_fuse = 1920;
      else if(fuse_reg_pp & 0x4000U)
        hw_fuse_sts->max_pp_out_pic_width_fuse = 1280;
      else if(fuse_reg_pp & 0x2000U)
        hw_fuse_sts->max_pp_out_pic_width_fuse = 720;
      else if(fuse_reg_pp & 0x1000U)
        hw_fuse_sts->max_pp_out_pic_width_fuse = 352;

      hw_fuse_sts->pp_config_fuse = fuse_reg_pp;
    } else {
      hw_fuse_sts->pp_support_fuse = 0;
      hw_fuse_sts->max_pp_out_pic_width_fuse = 0;
      hw_fuse_sts->pp_config_fuse = 0;
    }
  }
#endif
}

static av_unused void ReadCoreConfig(const u32 *io, DWLHwConfig *hw_cfg) {
  u32 config_reg;
  const u32 asic_id = io[0];

  /* Decoder configuration */
  config_reg = io[HANTRODEC_SYNTH_CFG];

  hw_cfg->h264_support = (config_reg >> DWL_H264_E) & 0x3U;
  /* check jpeg */
  hw_cfg->jpeg_support = (config_reg >> DWL_JPEG_E) & 0x01U;
  if(hw_cfg->jpeg_support && ((config_reg >> DWL_PJPEG_E) & 0x01U))
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
  if(DEC_X170_APF_DISABLE) {
    hw_cfg->tiled_mode_support = 0;
  }
#endif /* DEC_X170_APF_DISABLE */

  hw_cfg->max_dec_pic_width = config_reg & 0x07FFU;

  /* 2nd Config register */
  config_reg = io[HANTRODEC_SYNTH_CFG_2];
  if(hw_cfg->ref_buf_support) {
    if((config_reg >> DWL_REF_BUFF_ILACE_E) & 0x01U)
      hw_cfg->ref_buf_support |= 2;
    if((config_reg >> DWL_REF_BUFF_DOUBLE_E) & 0x01U)
      hw_cfg->ref_buf_support |= 4;
  }

  hw_cfg->custom_mpeg4_support = (config_reg >> DWL_MPEG4_CUSTOM_E) & 0x01U;
  hw_cfg->vp7_support = (config_reg >> DWL_VP7_E) & 0x01U;
  hw_cfg->vp8_support = (config_reg >> DWL_VP8_E) & 0x01U;
  hw_cfg->avs_support = (config_reg >> DWL_AVS_E) & 0x01U;

  /* JPEG extensions */
  if(((asic_id >> 16) >= 0x8190U) || ((asic_id >> 16) == 0x6731U))
    hw_cfg->jpeg_esupport = (config_reg >> DWL_JPEG_EXT_E) & 0x01U;
  else
    hw_cfg->jpeg_esupport = JPEG_EXT_NOT_SUPPORTED;

  if(((asic_id >> 16) >= 0x9170U) || ((asic_id >> 16) == 0x6731U))
    hw_cfg->rv_support = (config_reg >> DWL_RV_E) & 0x03U;
  else
    hw_cfg->rv_support = RV_NOT_SUPPORTED;

  hw_cfg->mvc_support = (config_reg >> DWL_MVC_E) & 0x03U;
  hw_cfg->webp_support = (config_reg >> DWL_WEBP_E) & 0x01U;
  hw_cfg->tiled_mode_support = (config_reg >> DWL_DEC_TILED_L) & 0x03U;
  hw_cfg->max_dec_pic_width += (( config_reg >> DWL_DEC_PIC_W_EXT) & 0x03U ) << 11;

  hw_cfg->ec_support = (config_reg >> DWL_EC_E) & 0x03U;
  hw_cfg->stride_support = (config_reg >> DWL_STRIDE_E) & 0x01U;
  hw_cfg->field_dpb_support = (config_reg >> DWL_FIELD_DPB_E) & 0x01U;
  hw_cfg->avs_plus_support = (config_reg >> DWL_AVS_PLUS_E) & 0x01U;
  hw_cfg->addr64_support = (config_reg >> DWL_64BIT_ENV_E) & 0x01U;

  if(hw_cfg->ref_buf_support && ((asic_id >> 16) == 0x6731U)) {
    hw_cfg->ref_buf_support |= 8; /* enable HW support for offset */
  }

  /* Pp configuration */
  config_reg = io[HANTROPP_SYNTH_CFG];

  if((config_reg >> DWL_G1_PP_E) & 0x01U) {
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
  if(((asic_id >> 16) >= 0x8190U) || ((asic_id >> 16) == 0x6731U)) {
    u32 de_interlace;
    u32 alpha_blend;
    u32 de_interlace_fuse;
    u32 alpha_blend_fuse;
    struct DWLHwFuseStatus hw_fuse_sts;

    /* check fuse status */
    ReadCoreFuse(io, &hw_fuse_sts);

    /* Maximum decoding width supported by the HW */
    if(hw_cfg->max_dec_pic_width > hw_fuse_sts.max_dec_pic_width_fuse)
      hw_cfg->max_dec_pic_width = hw_fuse_sts.max_dec_pic_width_fuse;
    /* Maximum output width of Post-Processor */
    if(hw_cfg->max_pp_out_pic_width > hw_fuse_sts.max_pp_out_pic_width_fuse)
      hw_cfg->max_pp_out_pic_width = hw_fuse_sts.max_pp_out_pic_width_fuse;
    /* h264 */
    if(!hw_fuse_sts.h264_support_fuse)
      hw_cfg->h264_support = H264_NOT_SUPPORTED;
    /* mpeg-4 */
    if(!hw_fuse_sts.mpeg4_support_fuse)
      hw_cfg->mpeg4_support = MPEG4_NOT_SUPPORTED;
    /* custom mpeg-4 */
    if(!hw_fuse_sts.custom_mpeg4_support_fuse)
      hw_cfg->custom_mpeg4_support = MPEG4_CUSTOM_NOT_SUPPORTED;
    /* jpeg (baseline && progressive) */
    if(!hw_fuse_sts.jpeg_support_fuse)
      hw_cfg->jpeg_support = JPEG_NOT_SUPPORTED;
    if((hw_cfg->jpeg_support == JPEG_PROGRESSIVE) &&
        !hw_fuse_sts.jpeg_prog_support_fuse)
      hw_cfg->jpeg_support = JPEG_BASELINE;
    /* mpeg-2 */
    if(!hw_fuse_sts.mpeg2_support_fuse)
      hw_cfg->mpeg2_support = MPEG2_NOT_SUPPORTED;
    /* vc-1 */
    if(!hw_fuse_sts.vc1_support_fuse)
      hw_cfg->vc1_support = VC1_NOT_SUPPORTED;
    /* vp6 */
    if(!hw_fuse_sts.vp6_support_fuse)
      hw_cfg->vp6_support = VP6_NOT_SUPPORTED;
    /* vp7 */
    if(!hw_fuse_sts.vp7_support_fuse)
      hw_cfg->vp7_support = VP7_NOT_SUPPORTED;
    /* vp8 */
    if(!hw_fuse_sts.vp8_support_fuse)
      hw_cfg->vp8_support = VP8_NOT_SUPPORTED;
    /* webp */
    if(!hw_fuse_sts.vp8_support_fuse)
      hw_cfg->webp_support = WEBP_NOT_SUPPORTED;
    /* pp */
    if(!hw_fuse_sts.pp_support_fuse)
      hw_cfg->pp_support = PP_NOT_SUPPORTED;
    /* check the pp config vs fuse status */
    if((hw_cfg->pp_config & 0xFC000000) &&
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
      if(de_interlace && !de_interlace_fuse)
        hw_cfg->pp_config &= 0xFD000000;
      if(alpha_blend && !alpha_blend_fuse)
        hw_cfg->pp_config &= 0xFE000000;
    }
    /* sorenson */
    if(!hw_fuse_sts.sorenson_spark_support_fuse)
      hw_cfg->sorenson_spark_support = SORENSON_SPARK_NOT_SUPPORTED;
    /* ref. picture buffer */
    if(!hw_fuse_sts.ref_buf_support_fuse)
      hw_cfg->ref_buf_support = REF_BUF_NOT_SUPPORTED;

    /* rv */
    if(!hw_fuse_sts.rv_support_fuse)
      hw_cfg->rv_support = RV_NOT_SUPPORTED;
    /* avs */
    if(!hw_fuse_sts.avs_support_fuse)
      hw_cfg->avs_support = AVS_NOT_SUPPORTED;
    /* mvc */
    if(!hw_fuse_sts.mvc_support_fuse)
      hw_cfg->mvc_support = MVC_NOT_SUPPORTED;
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicConfig
    Description     : Read HW configuration. Does not need a DWL instance to run

    Return type     : DWLHwConfig_t - structure with HW configuration
------------------------------------------------------------------------------*/
void DWLReadAsicConfig(DWLHwConfig *hw_cfg, u32 client_type) {

#ifdef _SYSTEM_MODEL_
  ASSERT(hw_cfg != NULL);

  struct DecHwFeatures hw_feature;
  GetReleaseHwFeaturesByID(g_hw_build_id, &hw_feature);

  TBGetHwConfig(&hw_feature, hw_cfg);

  if(g_hw_ver < 8190 || hw_cfg->h264_support == H264_BASELINE_PROFILE) {
    h264_high_support = 0;
  }

  /* if HW config (or tb.cfg here) says that SupportH264="10" -> keep it
   * like that. This does not mean MAIN profile support but is used to
   * cut down bus load by disabling direct mv writing in certain
   * applications */
  if( hw_cfg->h264_support && hw_cfg->h264_support != 2) {
    hw_cfg->h264_support =
      h264_high_support ? H264_HIGH_PROFILE : H264_BASELINE_PROFILE;
  }

  /* Apply fuse limitations */
  {
    u32 asic_id;
    u32 de_interlace;
    u32 alpha_blend;
    u32 de_interlace_fuse;
    u32 alpha_blend_fuse;

    /* check the HW version */
    asic_id = DWLReadAsicID(client_type);
    if(((asic_id >> 16) >= 0x8190U) ||
        ((asic_id >> 16) == 0x6731U) ) {
      struct DWLHwFuseStatus hw_fuse_sts;
      /* check fuse status */
      DWLReadAsicFuseStatus(&hw_fuse_sts);

      /* Maximum decoding width supported by the HW */
      if(hw_cfg->max_dec_pic_width > hw_fuse_sts.max_dec_pic_width_fuse)
        hw_cfg->max_dec_pic_width = hw_fuse_sts.max_dec_pic_width_fuse;
      /* Maximum output width of Post-Processor */
      if(hw_cfg->max_pp_out_pic_width > hw_fuse_sts.max_pp_out_pic_width_fuse)
        hw_cfg->max_pp_out_pic_width = hw_fuse_sts.max_pp_out_pic_width_fuse;
      /* h264 */
      if(!hw_fuse_sts.h264_support_fuse)
        hw_cfg->h264_support = H264_NOT_SUPPORTED;
      /* mpeg-4 */
      if(!hw_fuse_sts.mpeg4_support_fuse)
        hw_cfg->mpeg4_support = MPEG4_NOT_SUPPORTED;
      /* jpeg (baseline && progressive) */
      if(!hw_fuse_sts.jpeg_support_fuse)
        hw_cfg->jpeg_support = JPEG_NOT_SUPPORTED;
      if(hw_cfg->jpeg_support == JPEG_PROGRESSIVE &&
          !hw_fuse_sts.jpeg_prog_support_fuse)
        hw_cfg->jpeg_support = JPEG_BASELINE;
      /* mpeg-2 */
      if(!hw_fuse_sts.mpeg2_support_fuse)
        hw_cfg->mpeg2_support = MPEG2_NOT_SUPPORTED;
      /* vc-1 */
      if(!hw_fuse_sts.vc1_support_fuse)
        hw_cfg->vc1_support = VC1_NOT_SUPPORTED;
      /* vp6 */
      if(!hw_fuse_sts.vp6_support_fuse)
        hw_cfg->vp6_support = VP6_NOT_SUPPORTED;
      /* vp7 */
      if(!hw_fuse_sts.vp7_support_fuse)
        hw_cfg->vp7_support = VP7_NOT_SUPPORTED;
      /* vp8 */
      if(!hw_fuse_sts.vp8_support_fuse)
        hw_cfg->vp8_support = VP8_NOT_SUPPORTED;
      /* webp */
      if(!hw_fuse_sts.vp8_support_fuse)
        hw_cfg->webp_support = WEBP_NOT_SUPPORTED;
      /* avs */
      if(!hw_fuse_sts.avs_support_fuse)
        hw_cfg->avs_support = AVS_NOT_SUPPORTED;
      /* rv */
      if(!hw_fuse_sts.rv_support_fuse)
        hw_cfg->rv_support = RV_NOT_SUPPORTED;
      /* mvc */
      if(!hw_fuse_sts.mvc_support_fuse)
        hw_cfg->mvc_support = MVC_NOT_SUPPORTED;
      /* pp */
      if(!hw_fuse_sts.pp_support_fuse)
        hw_cfg->pp_support = PP_NOT_SUPPORTED;
      /* check the pp config vs fuse status */
      if((hw_cfg->pp_config & 0xFC000000) &&
          ((hw_fuse_sts.pp_config_fuse & 0xF0000000) >> 5)) {
        /* config */
        de_interlace = ((hw_cfg->pp_config & PP_DEINTERLACING) >> 25);
        alpha_blend = ((hw_cfg->pp_config & PP_ALPHA_BLENDING) >> 24);
        /* fuse */
        de_interlace_fuse =
          (((hw_fuse_sts.pp_config_fuse >> 5) & PP_DEINTERLACING) >> 25);
        alpha_blend_fuse =
          (((hw_fuse_sts.pp_config_fuse >> 5) & PP_ALPHA_BLENDING) >> 24);

        /* check fuse */
        if(de_interlace && !de_interlace_fuse)
          hw_cfg->pp_config &= 0xFD000000;
        if(alpha_blend && !alpha_blend_fuse)
          hw_cfg->pp_config &= 0xFE000000;
      }
      /* sorenson */
      if(!hw_fuse_sts.sorenson_spark_support_fuse)
        hw_cfg->sorenson_spark_support = SORENSON_SPARK_NOT_SUPPORTED;
      /* ref. picture buffer */
      if(!hw_fuse_sts.ref_buf_support_fuse)
        hw_cfg->ref_buf_support = REF_BUF_NOT_SUPPORTED;
    }
  }

#else
  const u32 *io = MAP_FAILED;
  unsigned int reg_size = 0;
  unsigned long base = 0;

  int fd = (-1), fd_dec = (-1);

  DWL_DEBUG("\n");

  /* config from shadow regs */
  if(dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG_2] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTROPP_SYNTH_CFG] != 0x00000000) {
    ReadCoreConfig(dwl_shadow_config_regs[0], hw_cfg);
    goto end;
  }

  fd = open("/dev/mem", O_RDONLY);
  if(fd == -1) {
    DWL_DEBUG("failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if(fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  /* ask module for base */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWOFFSET, &base) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOCGHWOFFSET failed\n");
    goto end;
  }

  /* ask module for reg size */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
    goto end;
  }

  io = DWLMapRegisters(fd, base, reg_size, 0);
  if(io == MAP_FAILED) {
    DWL_DEBUG("failed to mmap registers\n");
    goto end;
  }

  /* Decoder configuration */
  memset(hw_cfg, 0, sizeof(*hw_cfg));

  ReadCoreConfig(io, hw_cfg);

  /* to shadow regs */
  dwl_shadow_config_regs[0][0] = io[0];
  dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG] = io[HANTRODEC_SYNTH_CFG];
  dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG_2] = io[HANTRODEC_SYNTH_CFG_2];
  dwl_shadow_config_regs[0][HANTROPP_SYNTH_CFG] = io[HANTROPP_SYNTH_CFG];
  dwl_shadow_config_regs[0][HANTRODEC_FUSE_CFG] = io[HANTRODEC_FUSE_CFG];
  dwl_shadow_config_regs[0][HANTROPP_FUSE_CFG] = io[HANTROPP_FUSE_CFG];

  DWLUnmapRegisters(io, reg_size);

end:
  if(fd != -1)
    close(fd);
  if(fd_dec != -1)
    close(fd_dec);

#endif
}

void DWLReadMCAsicConfig(DWLHwConfig hw_cfg[MAX_ASIC_CORES]) {
#ifdef _SYSTEM_MODEL_
  u32 i, cores = DWLReadAsicCoreCount();
  ASSERT(cores <= MAX_ASIC_CORES);

  /* read core 0  cfg */
  DWLReadAsicConfig(hw_cfg, 0);

  /* ... and replicate first core cfg to all other cores */
  for( i = 1;  i < cores; i++)
    DWLmemcpy(hw_cfg + i, hw_cfg, sizeof(DWLHwConfig));
#else
  const u32 *io = MAP_FAILED;
  unsigned int reg_size;
  unsigned int n_cores, i;
  unsigned long mc_reg_base[MAX_ASIC_CORES];

  int fd = (-1), fd_dec = (-1);

  DWL_DEBUG("\n");

  /* config from shadow regs */
  if(dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG]   != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG_2] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTROPP_SYNTH_CFG] != 0x00000000) {
    for (i = 0; i < n_cores; i++) {
      ReadCoreConfig(dwl_shadow_config_regs[i], hw_cfg + i);
      goto end;
    }
  }

  fd = open("/dev/mem", O_RDONLY);
  if(fd == -1) {
    DWL_DEBUG("failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if(fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  if(ioctl(fd_dec, HANTRODEC_IOC_MC_CORES,  &n_cores) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOC_MC_CORES failed\n");
    goto end;
  }

  ASSERT(n_cores <= MAX_ASIC_CORES);

  if(ioctl(fd_dec, HANTRODEC_IOC_MC_OFFSETS, mc_reg_base) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
    goto end;
  }

  /* ask module for reg size */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("ioctl failed\n");
    goto end;
  }

  /* Decoder configuration */
  memset(hw_cfg, 0, MAX_ASIC_CORES * sizeof(*hw_cfg));

  for (i = 0; i < n_cores; i++) {
    io = DWLMapRegisters(fd, mc_reg_base[i], reg_size, 0);
    if(io == MAP_FAILED) {
      DWL_DEBUG("failed to mmap registers\n");
      goto end;
    }

    ReadCoreConfig(io, hw_cfg + i);

    /* to shadow regs */
    dwl_shadow_config_regs[i][0] = io[0];
    dwl_shadow_config_regs[i][HANTRODEC_SYNTH_CFG] = io[HANTRODEC_SYNTH_CFG];
    dwl_shadow_config_regs[i][HANTRODEC_SYNTH_CFG_2] = io[HANTRODEC_SYNTH_CFG_2];
    dwl_shadow_config_regs[i][HANTROPP_SYNTH_CFG] = io[HANTROPP_SYNTH_CFG];
    dwl_shadow_config_regs[i][HANTRODEC_FUSE_CFG] = io[HANTRODEC_FUSE_CFG];
    dwl_shadow_config_regs[i][HANTROPP_FUSE_CFG] = io[HANTROPP_FUSE_CFG];

    DWLUnmapRegisters(io, reg_size);
  }

end:
  if(fd != -1)
    close(fd);
  if(fd_dec != -1)
    close(fd_dec);
#endif
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicFuseStatus
    Description     : Read HW fuse configuration. Does not need a DWL instance to run

    Returns     : DWLHwFuseStatus_t * hw_fuse_sts - structure with HW fuse configuration
------------------------------------------------------------------------------*/
void DWLReadAsicFuseStatus(struct DWLHwFuseStatus * hw_fuse_sts) {

#ifdef _SYSTEM_MODEL_
  ASSERT(hw_fuse_sts != NULL);
  /* Maximum decoding width supported by the HW (fuse) */
  hw_fuse_sts->max_dec_pic_width_fuse = 4096;
  /* Maximum output width of Post-Processor (fuse) */
  hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;

  hw_fuse_sts->h264_support_fuse = H264_FUSE_ENABLED;    /* HW supports H264 */
  hw_fuse_sts->mpeg4_support_fuse = MPEG4_FUSE_ENABLED;  /* HW supports MPEG-4 */
  /* HW supports MPEG-2/MPEG-1 */
  hw_fuse_sts->mpeg2_support_fuse = MPEG2_FUSE_ENABLED;
  /* HW supports Sorenson Spark */
  hw_fuse_sts->sorenson_spark_support_fuse = SORENSON_SPARK_ENABLED;
  /* HW supports baseline JPEG */
  hw_fuse_sts->jpeg_support_fuse = JPEG_FUSE_ENABLED;
  hw_fuse_sts->vp6_support_fuse = VP6_FUSE_ENABLED;  /* HW supports VP6 */
  hw_fuse_sts->vc1_support_fuse = VC1_FUSE_ENABLED;  /* HW supports VC-1 */
  /* HW supports progressive JPEG */
  hw_fuse_sts->jpeg_prog_support_fuse = JPEG_PROGRESSIVE_FUSE_ENABLED;
  hw_fuse_sts->ref_buf_support_fuse = REF_BUF_FUSE_ENABLED;
  hw_fuse_sts->avs_support_fuse = AVS_FUSE_ENABLED;
  hw_fuse_sts->rv_support_fuse = RV_FUSE_ENABLED;
  hw_fuse_sts->vp7_support_fuse = VP7_FUSE_ENABLED;
  hw_fuse_sts->vp8_support_fuse = VP8_FUSE_ENABLED;
  hw_fuse_sts->mvc_support_fuse = MVC_FUSE_ENABLED;

  /* PP fuses */
  hw_fuse_sts->pp_support_fuse = PP_FUSE_ENABLED;    /* HW supports PP */
  /* PP fuse has all optional functions */
  hw_fuse_sts->pp_config_fuse = PP_FUSE_DEINTERLACING_ENABLED |
                                PP_FUSE_ALPHA_BLENDING_ENABLED |
                                MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED;
#else
  const u32 *io = MAP_FAILED;
  unsigned long base = 0;
  unsigned int reg_size = 0;
  int fd = (-1), fd_dec = (-1);

  DWL_DEBUG("\n");

  memset(hw_fuse_sts, 0, sizeof(*hw_fuse_sts));

  /* id from shadow regs */
  if(dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTRODEC_FUSE_CFG] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTROPP_SYNTH_CFG] != 0x00000000 &&
      dwl_shadow_config_regs[0][HANTROPP_FUSE_CFG] != 0x00000000) {
    /* Decoder fuse configuration */
    ReadCoreFuse(dwl_shadow_config_regs[0], hw_fuse_sts);
  }

  fd = open("/dev/mem", O_RDONLY);
  if(fd == -1) {
    DWL_DEBUG("failed to open /dev/mem\n");
    goto end;
  }

  fd_dec = open(DEC_MODULE_PATH, O_RDONLY);
  if(fd_dec == -1) {
    DWL_DEBUG("failed to open %s\n", DEC_MODULE_PATH);
    goto end;
  }

  /* ask module for base */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWOFFSET, &base) == -1) {
    DWL_DEBUG("ioctl failed\n");
    goto end;
  }

  /* ask module for reg size */
  if(ioctl(fd_dec, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
    DWL_DEBUG("ioctl failed\n");
    goto end;
  }

  io = DWLMapRegisters(fd, base, reg_size, 0);

  if(io == MAP_FAILED) {
    DWL_DEBUG("failed to mmap\n");
    goto end;
  }

  /* Decoder fuse configuration */
  ReadCoreFuse(io, hw_fuse_sts);

  dwl_shadow_config_regs[0][HANTRODEC_SYNTH_CFG] = io[HANTRODEC_SYNTH_CFG];
  dwl_shadow_config_regs[0][HANTRODEC_FUSE_CFG] = io[HANTRODEC_FUSE_CFG];
  dwl_shadow_config_regs[0][HANTROPP_SYNTH_CFG] = io[HANTROPP_SYNTH_CFG];
  dwl_shadow_config_regs[0][HANTROPP_FUSE_CFG] = io[HANTROPP_FUSE_CFG];

  DWLUnmapRegisters(io, reg_size);

end:
  if(fd != -1)
    close(fd);
  if(fd_dec != -1)
    close(fd_dec);
#endif
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
i32 DWLMallocRefFrm(const void *instance, u32 size, struct DWLLinearMem * info) {

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
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem * info) {
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
i32 DWLMallocLinear(const void *instance, u32 size, struct DWLLinearMem * info) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;

  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
  info->virtual_address = (u32 *)memalign(DEC_X170_BUS_ADDR_ALIGNMENT, size);
#ifdef MEMORY_USAGE_TRACE
  printf("DWLMallocLinear: %8d\n", size);
#endif
  if(info->virtual_address == NULL)
    return DWL_ERROR;
#ifdef _DWL_MEMSET_BUFFER
  DWLmemset(info->virtual_address, 0x80, size);
#endif
  info->bus_address = (addr_t) info->virtual_address;
  info->size = size;

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
void DWLFreeLinear(const void *instance, struct DWLLinearMem * info) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;

  ASSERT(dec_dwl != NULL);
  ASSERT(info != NULL);

  if(info->bus_address != 0) {
    free(info->virtual_address);
    info->size = 0;
  }
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
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;

#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n",
            core_id, offset/4, offset, value);
#endif

#if 0
  /*some MSB regs added to support 64bit address accress,
    so this assert should be ingored */
#ifndef USE_64BIT_ENV
  assert((dec_dwl->client_type != DWL_CLIENT_TYPE_PP &&
          offset < HANTROPP_REG_START) ||
         (dec_dwl->client_type == DWL_CLIENT_TYPE_PP &&
          offset >= HANTROPP_REG_START));
#endif
#endif

  offset = offset / 4;

  ASSERT(dec_dwl != NULL);
  ASSERT(offset < dec_dwl->reg_size);
  ASSERT(core_id < (i32)dec_dwl->num_cores);


  dwl_shadow_regs[core_id][offset] = value;

#ifdef INTERNAL_TEST
  InternalTestDumpWriteSwReg(core_id, offset, value, dwl_shadow_regs[core_id]);
#endif
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  u32 val;

  ASSERT(dec_dwl != NULL);
  /*some MSB regs added to support 64bit address accress,
    so this assert should be ingored */
#if 0
#ifndef USE_64BIT_ENV
  ASSERT((dec_dwl->client_type != DWL_CLIENT_TYPE_PP &&
          offset < HANTROPP_REG_START) ||
         (dec_dwl->client_type == DWL_CLIENT_TYPE_PP &&
          offset >= HANTROPP_REG_START) || (offset == 0) ||
         (offset == HANTROPP_SYNTH_CFG));
#endif
#endif
  ASSERT(offset < dec_dwl->reg_size);
  ASSERT(core_id < (i32)dec_dwl->num_cores);

  offset = offset / 4;

  val = dwl_shadow_regs[core_id][offset];

#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n",
            core_id, offset, offset * 4, val);
#endif

#ifdef INTERNAL_TEST
  InternalTestDumpReadSwReg(core_id, offset, val, dwl_shadow_regs[core_id]);
#endif

  return val;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegToHw
    Description     : Write a value to a hardware IO register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value)
{
    DWLWriteReg(instance, core_id, offset, value);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadRegFromHW
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadRegFromHw(const void *instance, i32 core_id, u32 offset)
{
    return DWLReadReg(instance, core_id, offset);
}

/*------------------------------------------------------------------------------
    Function name   : DWLEnableHW
    Description     : Enable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  struct core_desc core;
  int ioctl_req = 0;
#ifdef SUPPORT_CACHE
  u32 tile_by_tile = 0;
  int max_wait_time = 10000; /* 10s in ms */
#endif
  ASSERT(dec_dwl);
#ifdef SUPPORT_CACHE
  if (((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_HEVC ||
      ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_VP9 ||
      ((DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_H264_H10P)
    DWLWriteReg(dec_dwl, core_id, 4*3, (dwl_shadow_regs[core_id][3] | 0x08));
  DWLWriteReg(dec_dwl, core_id, 4*265, (dwl_shadow_regs[core_id][265] | 0x80000000));
  if ((DWLReadReg(dec_dwl, core_id, 4*58) & 0xFF) > 16)
    DWLWriteReg(dec_dwl, core_id, 4*58,
                ((dwl_shadow_regs[core_id][58] & 0xFFFFFF00) | 0x10));
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
    DWLConfigureCacheChannel(instance, core_id, dec_dwl->buf_size);
      if (!dec_dwl->shaper_enable[core_id]) {
      u32 pp_enabled = dec_dwl->ppu_cfg[0].enabled |
                       dec_dwl->ppu_cfg[1].enabled |
                       dec_dwl->ppu_cfg[2].enabled |
                       dec_dwl->ppu_cfg[3].enabled |
                       dec_dwl->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dec_dwl->ppu_cfg, pp_enabled, tile_by_tile, dec_dwl->tile_id, &(dec_dwl->num_tiles), dec_dwl->tiles);
    }
  } else {
    if (!dec_dwl->tile_id)
      DWLConfigureCacheChannel(instance, core_id, dec_dwl->buf_size);
    if (!dec_dwl->shaper_enable[core_id]) {
      u32 pp_enabled = dec_dwl->ppu_cfg[0].enabled |
                       dec_dwl->ppu_cfg[1].enabled |
                       dec_dwl->ppu_cfg[2].enabled |
                       dec_dwl->ppu_cfg[3].enabled |
                       dec_dwl->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dec_dwl->ppu_cfg, pp_enabled, tile_by_tile, dec_dwl->tile_id, &(dec_dwl->num_tiles), dec_dwl->tiles);
      dec_dwl->tile_id++;
    }
  }
  cache_enable[core_id] = 1;
#endif
  pthread_mutex_lock(&dec_dwl->shadow_mutex);
  ioctl_req = HANTRODEC_IOCS_DEC_PUSH_REG;

  DWLWriteReg(dec_dwl, core_id, offset, value);

  DWL_DEBUG("%s %d enabled by previous DWLWriteReg\n",
            is_pp ? "PP" : "DEC", core_id);

  core.id = core_id;
  core.regs = dwl_shadow_regs[core_id];
  core.size = dec_dwl->reg_size;

  if(ioctl(dec_dwl->fd, ioctl_req, &core)) {
    DWL_DEBUG("ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n", "");
    ASSERT(0);
  }
  hw_enable[core_id] = 1;

  dec_dwl->core_usage_counts[core_id]++;

  pthread_mutex_unlock(&dec_dwl->shadow_mutex);
#ifdef _SYSTEM_MODEL_
  AsicHwCoreRun(g_sysmodel_core[core_id]);
#endif
}

/*------------------------------------------------------------------------------
    Function name   : DWLDisableHW
    Description     : Disable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  struct core_desc core;
  int ioctl_req = 0;

  ASSERT(dec_dwl);

  ioctl_req = HANTRODEC_IOCS_DEC_PUSH_REG;

  DWLWriteReg(dec_dwl, core_id, offset, value);

  DWL_DEBUG("%s %d disabled by previous DWLWriteReg\n", "DEC", core_id);

  core.id = core_id;
  core.regs = dwl_shadow_regs[core_id];
  core.size = dec_dwl->reg_size;

  if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
    DWL_DEBUG("ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n", "");
    ASSERT(0);
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
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  struct core_desc core;
  int ioctl_req = 0;
  i32 ret = DWL_HW_WAIT_OK;

  UNUSED(timeout);

#ifndef DWL_USE_DEC_IRQ
  int max_wait_time = 10000; /* 10s in ms */
#endif

  ASSERT(dec_dwl);

  DWL_DEBUG("%s %d\n", "DEC", core_id);

  core.id = core_id;
  core.regs = dwl_shadow_regs[core_id];
  core.size = dec_dwl->reg_size;

#ifdef DWL_USE_DEC_IRQ
#if 0
    ioctl_req = HANTRODEC_IOCX_DEC_WAIT;

    if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
      DWL_DEBUG("ioctl HANTRODEC_IOCG_*_WAIT failed\n");
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
      DWL_DEBUG("ioctl HANTRODEC_IOCS_*_PULL_REG failed\n", "");
      ret = DWL_HW_WAIT_ERROR;
      break;
    }

    irq_stats = dwl_shadow_regs[core_id][HANTRO_IRQ_STAT_DEC];
    pthread_mutex_unlock(&dec_dwl->shadow_mutex);
    irq_stats = (irq_stats >> 11) & 0x1FFF;

    if(irq_stats != 0) {
      hw_enable[core_id] = 0;
#ifdef PERFORMANCE_TEST
      ActivityTraceStopDec(&dec_dwl->activity);
      hw_time_use = dec_dwl->activity.active_time / 100;
#endif
      ret = DWL_HW_WAIT_OK;
      ioctl_req = (int)HANTRODEC_IOCS_DEC_PULL_REG;
      pthread_mutex_lock(&dec_dwl->shadow_mutex);

      if (ioctl(dec_dwl->fd, ioctl_req, &core)) {
        pthread_mutex_unlock(&dec_dwl->shadow_mutex);
        DWL_DEBUG("%s", "ioctl HANTRODEC_IOCS_*_PULL_REG failed\n");
        ret = DWL_HW_WAIT_ERROR;
        break;
      }
      pthread_mutex_unlock(&(dec_dwl->shadow_mutex));
      break;
    }

    usleep(usec);

    max_wait_time--;
  } while (max_wait_time > 0);

#endif

#ifdef _DWL_DEBUG
  {
    u32 irq_stats = dwl_shadow_regs[core_id][HANTRO_IRQ_STAT_DEC];

    PrintIrqType(core_id, irq_stats);
  }
#endif

  DWL_DEBUG("%s %d done\n", "DEC", core_id);

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
  return malloc((size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()

    Return type     : void

    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void DWLfree(void *p) {
  if(p != NULL)
    free(p);
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
  return calloc((size_t) n, (size_t) s);
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
  return memcpy(d, s, (size_t) n);
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
  return memset(d, (int) c, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : void * param - not in use, application passes NULL
------------------------------------------------------------------------------*/
#ifdef _SYSTEM_MODEL_
u32 dwl_instance_num = 0;
#endif

const void *DWLInit(struct DWLInitParam * param) {
  struct HANTRODWL *dec_dwl;

  dec_dwl = (struct HANTRODWL *) calloc(1, sizeof(struct HANTRODWL));

  DWL_DEBUG("INITIALIZE\n", "");

  if(dec_dwl == NULL) {
    DWL_DEBUG("failed to alloc struct HANTRODWL struct\n", "");
    return NULL;
  }

  dec_dwl->client_type = param->client_type;
  dec_dwl->fd = -1;
  dec_dwl->fd_mem = -1;
  dec_dwl->fd_memalloc = -1;

  /* open the device */
  dec_dwl->fd = open(dec_dev, O_RDWR);
  if(dec_dwl->fd == -1) {
    DWL_DEBUG("failed to open '%s'\n", dec_dev);
    goto err;
  }

  /* Linear memories not needed in pp */
  if(dec_dwl->client_type != DWL_CLIENT_TYPE_PP) {
    /* open memalloc for linear memory allocation */
    dec_dwl->fd_memalloc = open(mem_dev, O_RDWR | O_SYNC);

    if(dec_dwl->fd_memalloc == -1) {
      DWL_DEBUG("failed to open: %s\n", mem_dev);
      goto err;
    }
  }

  dec_dwl->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);

  if(dec_dwl->fd_mem == -1) {
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
  case DWL_CLIENT_TYPE_PP: {
    break;
  }
  default: {
    DWL_DEBUG("Unknown client type no. %d\n", dec_dwl->client_type);
    goto err;
  }
  }

  if(ioctl(dec_dwl->fd, HANTRODEC_IOC_MC_CORES,  &dec_dwl->num_cores) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOC_MC_CORES failed\n", "");
    goto err;
  }

  if(ioctl(dec_dwl->fd, HANTRODEC_IOCGHWIOSIZE, &dec_dwl->reg_size) == -1) {
    DWL_DEBUG("ioctl HANTRODEC_IOCGHWIOSIZE failed\n", "");
    goto err;
  }

  DWL_DEBUG("SUCCESS\n", "");

#ifdef _SYSTEM_MODEL_
  dwl_instance_num++;
#endif

  return dec_dwl;

err:

  DWL_DEBUG("FAILED\n", "");

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
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;

  ASSERT(dec_dwl != NULL);

  if(dec_dwl->fd_mem != -1)
    close(dec_dwl->fd_mem);

  if(dec_dwl->fd != -1) {
    extern int hantrodec_release(/*struct inode * */ int *inode, /*struct file * */ int filp);
#ifdef _SYSTEM_MODEL_
    dwl_instance_num--;
    if(dwl_instance_num == 0)
#endif
      hantrodec_release(NULL, dec_dwl->fd);
    close(dec_dwl->fd);
  }

  /* linear memory allocator */
  if(dec_dwl->fd_memalloc != -1)
    close(dec_dwl->fd_memalloc);

  free(dec_dwl);

  DWL_DEBUG("SUCCESS\n", "");

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
  i32 ret;
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;

  ASSERT(dec_dwl != NULL);
  ASSERT(dec_dwl->client_type != DWL_CLIENT_TYPE_PP);

  DWL_DEBUG("Start\n", "");

  /* reserve decoder */
  *core_id = ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE,
                   dec_dwl->client_type);

  if (*core_id != 0) {
    return DWL_ERROR;
  }

  /* reserve PP */
  ret = ioctl(dec_dwl->fd, HANTRODEC_IOCQ_PP_RESERVE, 0);

  /* for pipeline we expect same core for both dec and PP */
  if (ret != *core_id) {
    /* release the decoder */
    ioctl(dec_dwl->fd, HANTRODEC_IOCT_DEC_RELEASE, core_id);
    return DWL_ERROR;
  }

  dec_dwl->b_ppreserved = 1;

  DWL_DEBUG("Reserved DEC+PP core %d\n", *core_id);

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
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  int is_pp;

  ASSERT(dec_dwl != NULL);

  is_pp = client_type == DWL_CLIENT_TYPE_PP ? 1 : 0;

  DWL_DEBUG(" %s\n", is_pp ? "PP" : "DEC");

  if (is_pp) {
    *core_id = ioctl(dec_dwl->fd, HANTRODEC_IOCQ_PP_RESERVE, 0);

    /* PP is single core so we expect a zero return value */
    if (*core_id != 0) {
      return DWL_ERROR;
    }
  } else {
    *core_id = ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE,
                     client_type);
  }

  /* negative value signals an error */
  if (*core_id < 0) {
    DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_RESERVE failed\n",
              is_pp ? "PP" : "DEC");
    return DWL_ERROR;
  }

  DWL_DEBUG("Reserved %s core %d\n", is_pp ? "PP" : "DEC", *core_id);

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
u32 DWLReleaseHw(const void *instance, i32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  int is_pp;

  ASSERT((u32)core_id < dec_dwl->num_cores);
  ASSERT(dec_dwl != NULL);

  is_pp = dec_dwl->client_type == DWL_CLIENT_TYPE_PP ? 1 : 0;

  if ((u32) core_id >= dec_dwl->num_cores)
    return 0;

  DWL_DEBUG(" %s core %d\n", is_pp ? "PP" : "DEC", core_id);

  if (is_pp) {
    ASSERT(core_id == 0);

    ioctl(dec_dwl->fd, HANTRODEC_IOCT_PP_RELEASE, core_id);
  } else {
    if (dec_dwl->b_ppreserved) {
      /* decoder has reserved PP also => release it */
      DWL_DEBUG("DEC released PP core %d\n", core_id);

      dec_dwl->b_ppreserved = 0;

      ASSERT(core_id == 0);

      ioctl(dec_dwl->fd, HANTRODEC_IOCT_PP_RELEASE, core_id);
    }

    ioctl(dec_dwl->fd, HANTRODEC_IOCT_DEC_RELEASE, core_id);
  }
  return 0;
}

void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void* arg) {
  /* not in use with single core only control code */
  UNUSED(instance);
  UNUSED(core_id);
  UNUSED(callback_fn);
  UNUSED(arg);

  //ASSERT(0);
}



/*------------------------------------------------------------------------------
    Function name   : DWLFakeTimeout
    Description     : Testing help function that changes HW stream errors info
                        HW timeouts. You can check how the SW behaves or not.
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/

#ifdef _DWL_FAKE_HW_TIMEOUT
void DWLFakeTimeout(u32 * status) {

  if((*status) & DEC_IRQ_ERROR) {
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
    DWLmemcpy(dec_dwl->ppu_cfg, ppu_cfg, DEC_MAX_PPU_COUNT*sizeof(PpUnitIntConfig));
}

void DWLReadMcRefBuffer(const void *instance, u32 core_id, u64 buf_size, u32 dpb_size) {
    struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
    dec_dwl->buf_size[core_id] = buf_size;
    UNUSED(dpb_size);
}

i32 DWLReserveCmdBuf(const void *instance, u32 client_type, u32 width, u32 height, u32 *cmd_buf_id) {
  UNUSED(instance);
  UNUSED(client_type);
  UNUSED(width);
  UNUSED(height);
  UNUSED(cmd_buf_id);
  return 0;
}


/* Reserve one valid command buffer. */
i32 DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id) {
  UNUSED(instance);
  UNUSED(cmd_buf_id);
  return 0;
}

/* Wait cmd buffer ready. Used only in single core decoding. */
i32 DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id) {
  UNUSED(instance);
  UNUSED(cmd_buf_id);
  return 0;
}

/* Reserve one valid command buffer. */
i32 DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id) {
  UNUSED(instance);
  UNUSED(cmd_buf_id);
  return 0;
}

i32 DWLWaitCmdbufsDone(const void *instance) {
  UNUSED(instance);
  return 0;
}

