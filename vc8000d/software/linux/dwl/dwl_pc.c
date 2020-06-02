/*------------------------------------------------------------------------------

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

#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "basetype.h"
#include "dwl.h"
#include "dwl_activity_trace.h"
#include "dwl_hw_core_array.h"
#include "dwl_swhw_sync.h"
#include "sw_util.h"
#include "deccfg.h"
#include "ppu.h"
#include "sw_performance.h"
#ifdef SUPPORT_CACHE
#include "dwl_linux_cache.h"
#endif

#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_DEC400
#include "vpu_dwl_dpi.h"
#endif
#endif
#include "tb_cfg.h"

#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#include "dwl_vcmd_common.h"
#include "vwl_pc.h"

#ifdef _DWL_DEBUG
#ifdef _WIN32
#define DWL_DEBUG printf
#else
#define DWL_DEBUG(fmt, args...) \
  printf(__FILE__ ":%d:%s() " fmt, __LINE__, __func__, ##args)
#endif
#else
#ifdef _WIN32
#define DWL_DEBUG(fmt, _VA_ARGS__) \
  do {                          \
  } while (0)
#else
#define DWL_DEBUG(fmt, args...) \
  do {                          \
  } while (0)
#endif
#endif

/* macro for assertion, used only if compiler flag _ASSERT_USED is defined */
#ifdef _ASSERT_USED
#define ASSERT(expr) assert(expr)
#else
#define ASSERT(expr)
#endif

#ifndef UNUSED
#  define UNUSED(x) (void)(x)
#endif

/* Constants related to registers */
#define DEC_X170_REGS 512
#define PP_FIRST_REG 60
#define DEC_STREAM_START_REG 12

#define DEC_MODE_JPEG 3
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2 16

#define VCMD_BUF_SIZE (8*1024)  /* 8KB for each command buffer */

#define IS_PIPELINE_ENABLED(val) ((val) & 0x02)

#ifdef DWL_PRESET_FAILING_ALLOC
#define FAIL_DURING_ALLOC DWL_PRESET_FAILING_ALLOC
#endif

#ifdef ASIC_TRACE_SUPPORT
extern struct TBCfg tb_cfg;
#endif

#ifdef ASIC_ONL_SIM
extern u8 l2_allocate_buf;
extern u64 HW_TB_PP_LUMA_BASE;

#ifdef SUPPORT_MULTI_CORE
u32* core_reg_base_array[2];
int cur_core_id = 0;
int start_mc_load = 0;
int mc_load_end = 0;
int cur_pic = 0;
int finished_pic = 0;
extern u32 max_pics_decode;
#endif
#ifdef SUPPORT_DEC400
u32 dec400_enable[MAX_ASIC_CORES];
#endif
#endif

#ifdef SUPPORT_CACHE
u32 cache_enable[MAX_ASIC_CORES];
extern u32 cache_exception_regs_num;
#ifdef ASIC_ONL_SIM
u32  L2_DEC400_configure_done = 0;
#endif

#define SHAPER_REG_OFFSET 0x8
#define CACHE_REG_OFFSET 0x80

u32 cache_shadow_regs[512];
u32 shaper_shadow_regs[128];
extern u32 cache_exception_lists[];
extern u32 cache_exception_regs_num;

#endif

/* bytes used before freeing and reallocating ref frame buffer */
u32 last_ref_frm_size = 0;
extern FILE *cache_fid;
u32 shaper_flag[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static i32 DWLTestRandomFail(void);

#ifdef _DWL_DEBUG

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
    printf("DEC[%d] IRQ SCAN_RDY\n", core_id);
  else
    printf("DEC[%d] IRQ UNKNOWN 0x%08x\n", core_id, status);
}
#endif

#ifdef PERFORMANCE_TEST
u32 hw_time_use;
#endif

struct DWLInstance {
  u32 client_type;
  u8 *free_ref_frm_mem;
  u8 *frm_base;

  u32 b_reserved_pipe;

  /* Keep track of allocated memories */
  u32 reference_total;
  u32 reference_alloc_count;
  u32 reference_maximum;
  u32 linear_total;
  i32 linear_alloc_count;

  HwCoreArray hw_core_array;
  /* TODO(vmr): Get rid of temporary core "memory" mechanism. */
  Core current_core;
  struct ActivityTrace activity;
  PpUnitIntConfig ppu_cfg[DEC_MAX_PPU_COUNT];
  u32 tile_id;
  u32 num_tiles;
  u32 shaper_enable;
  u16* tiles;
  u32 tile_by_tile;
  u32 pjpeg_coeff_buffer_size;
  /* counters for core usage statistics */
  struct CacheParams cache_cfg[MAX_ASIC_CORES];
  u32 core_usage_counts[MAX_ASIC_CORES];
  u32 frame_count;
  u32 vcmd_enabled;
  /* Submodule address/command buffer parameters when VCMD is used. */
  struct config_parameter vcmd_params;
  struct cmdbuf_mem_parameter vcmd_mem_params;
  pthread_mutex_t vcmd_mutex;
  struct VcmdBuf vcmd[MAX_VCMD_ENTRIES];
};

/* shadow HW registers. */
/* Now cmodel uses similiar registers transferring path as fpga: */
/*   xxxdec_container->xxx_regs[] -> dwl_shadow_regs -> cmodel core regs */
u32 dwl_shadow_regs[MAX_ASIC_CORES][512];

HwCoreArray g_hw_core_array;
struct McListenerThreadParams listener_thread_params;
pthread_t mc_listener_thread;

#ifdef _DWL_PERFORMANCE
u32 reference_total_max = 0;
u32 linear_total_max = 0;
u32 malloc_total_max = 0;
#endif

#ifdef FAIL_DURING_ALLOC
u32 failed_alloc_count = 0;
#endif

/* a mutex protecting the wrapper init */
pthread_mutex_t dwl_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int n_dwl_instance_count = 0;

/* single core PP mutex */
static pthread_mutex_t pp_mutex = PTHREAD_MUTEX_INITIALIZER;

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadAsicID(u32 client_type) {
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

  build = (build / 1000) * 0x1000 + ((build / 100) % 10) * 0x100 +
          ((build / 10) % 10) * 0x10 + ((build) % 10) * 0x1;
  (void)client_type;

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

/*------------------------------------------------------------------------------
    Function name   : DWLReadCoreAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadCoreAsicID(u32 core_id) {
  return DWLReadHwBuildID(core_id);
}

u32 DWLReadCoreHwBuildID(u32 core_id) {
  return g_hw_build_id;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicConfig
    Description     : Read HW configuration. Does not need a DWL instance to run

    Returns     : DWLHwConfig *hw_cfg - structure with HW configuration
------------------------------------------------------------------------------*/
void DWLReadAsicConfig(DWLHwConfig *hw_cfg, u32 client_type) {
  assert(hw_cfg != NULL);
  struct DecHwFeatures hw_feature;

  /* Decoder configuration */
  memset(hw_cfg, 0, sizeof(*hw_cfg));
  GetReleaseHwFeaturesByID(g_hw_build_id, &hw_feature);

  TBGetHwConfig(&hw_feature, hw_cfg);
  /* TODO(vmr): find better way to do this. */
  hw_cfg->vp9_support = 1;
  hw_cfg->hevc_support = 1;

  /* if HW config (or tb.cfg here) says that SupportH264="10" -> keep it
   * like that. This does not mean MAIN profile support but is used to
   * cut down bus load by disabling direct mv writing in certain
   * applications */
  if (hw_cfg->h264_support && hw_cfg->h264_support != 2) {
    hw_cfg->h264_support =
      h264_high_support ? H264_HIGH_PROFILE : H264_BASELINE_PROFILE;
  }

  /* Apply fuse limitations */
  {
    u32 de_interlace;
    u32 alpha_blend;
    u32 de_interlace_fuse;
    u32 alpha_blend_fuse;

    struct DWLHwFuseStatus hw_fuse_sts;
    /* check fuse status */
    DWLReadAsicFuseStatus(&hw_fuse_sts);

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
    /* jpeg (baseline && progressive) */
    if (!hw_fuse_sts.jpeg_support_fuse)
      hw_cfg->jpeg_support = JPEG_NOT_SUPPORTED;
    if (hw_cfg->jpeg_support == JPEG_PROGRESSIVE &&
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
    /* avs */
    if (!hw_fuse_sts.avs_support_fuse) hw_cfg->avs_support = AVS_NOT_SUPPORTED;
    /* rv */
    if (!hw_fuse_sts.rv_support_fuse) hw_cfg->rv_support = RV_NOT_SUPPORTED;
    /* mvc */
    if (!hw_fuse_sts.mvc_support_fuse) hw_cfg->mvc_support = MVC_NOT_SUPPORTED;
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

      /* check fuse */
      if (de_interlace && !de_interlace_fuse) hw_cfg->pp_config &= 0xFD000000;
      if (alpha_blend && !alpha_blend_fuse) hw_cfg->pp_config &= 0xFE000000;
    }
    /* sorenson */
    if (!hw_fuse_sts.sorenson_spark_support_fuse)
      hw_cfg->sorenson_spark_support = SORENSON_SPARK_NOT_SUPPORTED;
    /* ref. picture buffer */
    if (!hw_fuse_sts.ref_buf_support_fuse)
      hw_cfg->ref_buf_support = REF_BUF_NOT_SUPPORTED;
  }
  (void)client_type;
}

void DWLReadMCAsicConfig(DWLHwConfig hw_cfg[MAX_ASIC_CORES]) {
  u32 i, cores = DWLReadAsicCoreCount();
  assert(cores <= MAX_ASIC_CORES);

  /* read core 0  cfg */
  DWLReadAsicConfig(hw_cfg, 0);

  /* ... and replicate first core cfg to all other cores */
  for (i = 1; i < cores; i++)
    DWLmemcpy(hw_cfg + i, hw_cfg, sizeof(DWLHwConfig));
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicFuseStatus
    Description     : Read HW fuse configuration.
                      Does not need a DWL instance to run

    Returns         : *hw_fuse_sts - structure with HW fuse configuration
------------------------------------------------------------------------------*/
void DWLReadAsicFuseStatus(struct DWLHwFuseStatus *hw_fuse_sts) {
  assert(hw_fuse_sts != NULL);
  /* Maximum decoding width supported by the HW (fuse) */
  hw_fuse_sts->max_dec_pic_width_fuse = 4096;
  /* Maximum output width of Post-Processor (fuse) */
  hw_fuse_sts->max_pp_out_pic_width_fuse = 4096;

  hw_fuse_sts->h264_support_fuse = H264_FUSE_ENABLED;   /* HW supports H264 */
  hw_fuse_sts->mpeg4_support_fuse = MPEG4_FUSE_ENABLED; /* HW supports MPEG-4 */
  /* HW supports MPEG-2/MPEG-1 */
  hw_fuse_sts->mpeg2_support_fuse = MPEG2_FUSE_ENABLED;
  /* HW supports Sorenson Spark */
  hw_fuse_sts->sorenson_spark_support_fuse = SORENSON_SPARK_ENABLED;
  /* HW supports baseline JPEG */
  hw_fuse_sts->jpeg_support_fuse = JPEG_FUSE_ENABLED;
  hw_fuse_sts->vp6_support_fuse = VP6_FUSE_ENABLED; /* HW supports VP6 */
  hw_fuse_sts->vc1_support_fuse = VC1_FUSE_ENABLED; /* HW supports VC-1 */
  /* HW supports progressive JPEG */
  hw_fuse_sts->jpeg_prog_support_fuse = JPEG_PROGRESSIVE_FUSE_ENABLED;
  hw_fuse_sts->ref_buf_support_fuse = REF_BUF_FUSE_ENABLED;
  hw_fuse_sts->avs_support_fuse = AVS_FUSE_ENABLED;
  hw_fuse_sts->rv_support_fuse = RV_FUSE_ENABLED;
  hw_fuse_sts->vp7_support_fuse = VP7_FUSE_ENABLED;
  hw_fuse_sts->vp8_support_fuse = VP8_FUSE_ENABLED;
  hw_fuse_sts->mvc_support_fuse = MVC_FUSE_ENABLED;

  /* PP fuses */
  hw_fuse_sts->pp_support_fuse = PP_FUSE_ENABLED; /* HW supports PP */
  /* PP fuse has all optional functions */
  hw_fuse_sts->pp_config_fuse = PP_FUSE_DEINTERLACING_ENABLED |
                                PP_FUSE_ALPHA_BLENDING_ENABLED |
                                MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED;
}

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : struct DWLInitParam * param - initialization params
------------------------------------------------------------------------------*/
const void *DWLInit(struct DWLInitParam *param) {
  struct DWLInstance *dwl_inst;
  unsigned int i;
#ifdef SUPPORT_VCMD
  int ret;
#endif

  dwl_inst = (struct DWLInstance *)calloc(1, sizeof(struct DWLInstance));
  assert(dwl_inst);
  memset(dwl_inst, 0, sizeof(struct DWLInstance));

  switch (param->client_type) {
  case DWL_CLIENT_TYPE_H264_DEC:
    printf("DWL initialized by an H264 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_MPEG4_DEC:
    printf("DWL initialized by an MPEG4 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_JPEG_DEC:
    printf("DWL initialized by a JPEG decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_PP:
    printf("DWL initialized by a PP instance...\n");
    break;
  case DWL_CLIENT_TYPE_VC1_DEC:
    printf("DWL initialized by an VC1 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_MPEG2_DEC:
    printf("DWL initialized by an MPEG2 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_AVS_DEC:
    printf("DWL initialized by an AVS decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_RV_DEC:
    printf("DWL initialized by an RV decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP6_DEC:
    printf("DWL initialized by a VP6 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP8_DEC:
    printf("DWL initialized by a VP8 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_HEVC_DEC:
    printf("DWL initialized by an HEVC decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP9_DEC:
    printf("DWL initialized by a VP9 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_AVS2_DEC:
    printf("DWL initialized by a AVS2 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_ST_PP:
    printf("DWL initialized by a standalone PP instance...\n");
    break;
  default:
    printf("ERROR: DWL client type has to be always specified!\n");
    return NULL;
  }

#ifdef INTERNAL_TEST
  InternalTestInit();
#endif

  dwl_inst->client_type = param->client_type;
  dwl_inst->frm_base = NULL;
  dwl_inst->free_ref_frm_mem = NULL;

#ifdef SUPPORT_VCMD
  /*******************************************/
  /* VCMD related initialization. */
  pthread_mutex_init(&dwl_inst->vcmd_mutex, NULL);

  ret = CmodelVcmdInit();
  if (ret) {
    DWL_DEBUG("%s","CmodelVcmdInit() failed\n");
    goto err;
  }

  /* Get VCMD configuration. */
  dwl_inst->vcmd_params.module_type = 2; /* VC8000D */
  if (CmodelIoctlGetVcmdParameter(&dwl_inst->vcmd_params) == -1) {
    DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
    goto err;
  }

  if (CmodelIoctlGetCmdbufParameter(&dwl_inst->vcmd_mem_params) == -1) {
    DWL_DEBUG("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
    goto err;
  }

  /* vcmd init */
  DWLmemset(dwl_inst->vcmd, 0, sizeof(dwl_inst->vcmd));
  for (i = 1; i < MAX_VCMD_ENTRIES; i++) {
    dwl_inst->vcmd[i].valid = 1;
  }
  /* VCMD initialization done. */
  /*******************************************/
#endif

  pthread_mutex_lock(&dwl_init_mutex);
  /* Allocate cores just once */
  if (!n_dwl_instance_count) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    g_hw_core_array = InitializeCoreArray();
    if (g_hw_core_array == NULL) {
      free(dwl_inst);
      dwl_inst = NULL;
    }

    listener_thread_params.n_dec_cores = GetCoreCount();

    for (i = 0; i < listener_thread_params.n_dec_cores; i++) {
      Core c = GetCoreById(g_hw_core_array, i);

      listener_thread_params.reg_base[i] = HwCoreGetBaseAddress(c);

      listener_thread_params.callback[i] = NULL;
    }

    listener_thread_params.b_stopped = 0;
    listener_thread_params.vcmd = dwl_inst->vcmd;
    listener_thread_params.vcmd_params = &dwl_inst->vcmd_params;

    pthread_create(&mc_listener_thread, &attr, ThreadMcListener,
                   &listener_thread_params);

    pthread_attr_destroy(&attr);
  }

  dwl_inst->hw_core_array = g_hw_core_array;
  n_dwl_instance_count++;
#ifdef PERFORMANCE_TEST
  ActivityTraceInit(&dwl_inst->activity);
#endif

  pthread_mutex_unlock(&dwl_init_mutex);

  return (void *)dwl_inst;

#ifdef SUPPORT_VCMD
err:
#endif
  pthread_join(mc_listener_thread, NULL);
  ReleaseCoreArray(g_hw_core_array);
  free(dwl_inst);
  dwl_inst = NULL;

  return (void *)dwl_inst;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
i32 DWLRelease(const void *instance) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  unsigned int i;

  assert(dwl_inst != NULL);

  if (dwl_inst == NULL) return DWL_OK;

  assert(dwl_inst->reference_total == 0);
  assert(dwl_inst->reference_alloc_count == 0);
  assert(dwl_inst->linear_total == 0);
  assert(dwl_inst->linear_alloc_count == 0);

  pthread_mutex_lock(&dwl_init_mutex);

#ifdef INTERNAL_TEST
  InternalTestFinalize();
#endif

  n_dwl_instance_count--;

  if (!n_dwl_instance_count) {
    listener_thread_params.b_stopped = 1;
  }

#ifdef SUPPORT_VCMD
  /* Release Vcmd cmodel after setting b_stopped to 1, so that listener thread can return. */
  CmodelVcmdRelease();
#endif

  /* Release the signal handling and cores just when
   * nobody is referencing them anymore
   */
  if (!n_dwl_instance_count) {
//    listener_thread_params.b_stopped = 1;

    StopCoreArray(g_hw_core_array);

    pthread_join(mc_listener_thread, NULL);

    ReleaseCoreArray(g_hw_core_array);
  }

  pthread_mutex_unlock(&dwl_init_mutex);

  /* print core usage stats */
  {
    u32 total_usage = 0;
    u32 cores = DWLReadAsicCoreCount();
    for (i = 0; i < cores; i++) {
      total_usage += dwl_inst->core_usage_counts[i];
    }
    /* avoid zero division */
    total_usage = total_usage ? total_usage : 1;

    printf("\nMulti-core usage statistics:\n");
    for (i = 0; i < cores; i++)
      printf("\tCore[%2d] used %6d times (%2d%%)\n", i, dwl_inst->core_usage_counts[i],
             (dwl_inst->core_usage_counts[i] * 100) / total_usage);

    printf("\n");
  }

#ifdef PERFORMANCE_TEST
  ActivityTraceRelease(&dwl_inst->activity);
#endif

#ifdef _DWL_PERFORMANCE
  printf("Total allocated reference mem = %8d\n", reference_total_max);
  printf("Total allocated linear mem    = %8d\n", linear_total_max);
  printf("Total allocated SWSW mem      = %8d\n", malloc_total_max);
#endif

#ifdef SUPPORT_VCMD
  /* Free VCMD buffers. */
  pthread_mutex_lock(&dwl_inst->vcmd_mutex);
  for (i = 0; i < MAX_VCMD_ENTRIES; i++) {
    ASSERT(!dwl_inst->vcmd_enabled || !dwl_inst->vcmd[i].cmd_buf || dwl_inst->vcmd[i].valid);
  }
  pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
#endif

  free((void *)dwl_inst);

  return DWL_OK;
}

void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg) {
  listener_thread_params.callback[core_id] = callback_fn;
  listener_thread_params.callback_arg[core_id] = arg;
  (void)instance;
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : struct DWLLinearMem *info - place where the allocated
memory
                        buffer parameters are returned
------------------------------------------------------------------------------*/
i32 DWLMallocRefFrm(const void *instance, u32 size, struct DWLLinearMem *info) {

  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;

  if (DWLTestRandomFail()) {
    return DWL_ERROR;
  }
#ifdef ASIC_TRACE_SUPPORT
  u32 memory_size;

  printf("DWLMallocRefFrm: %8d\n", size);

  if (dwl_inst->frm_base == NULL) {
    if (tb_cfg.tb_params.ref_frm_buffer_size == -1) {
      /* Max value based on limits set by Level 5.2 */
      /* Use tb.cfg to overwrite this limit */
      u32 cores = DWLReadAsicCoreCount();
      /* Updated to Level6.2. */
      u32 max_frame_buffers = (139264 * (5 + 1 + cores)) * (384 + 64) * 10 / 8;
      /* TODO MIN function of "size" is not working here if we are
         not allocating all buffers in decoder init... */
      /* FIXME(min): extra 10 buffers for multicore top simulation. */
      memory_size = MAX(max_frame_buffers, (16 + 1 + cores + 10) * size);
      if ((16 + 1 + cores + 10) * size <= size) {
        /* overflow 4G size */
        memory_size = 0x7FFFF000;
      }
      memory_size = MIN(memory_size, 0x7FFFF000);
      tb_cfg.tb_params.ref_frm_buffer_size = memory_size;
    } else {
      /* Use tb.cfg to set max size for nonconforming streams */
      memory_size = tb_cfg.tb_params.ref_frm_buffer_size;
    }
    memory_size = NEXT_MULTIPLE(memory_size, DEC_X170_BUS_ADDR_ALIGNMENT);
    dwl_inst->frm_base = (u8 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, memory_size);
    if (dwl_inst->frm_base == NULL) return DWL_ERROR;

#ifdef _DWL_MEMSET_BUFFER
    DWLmemset(dwl_inst->frm_base, 0x80, memory_size);
#endif

    dwl_inst->reference_total = 0;
    dwl_inst->reference_maximum = memory_size;
    dwl_inst->free_ref_frm_mem = dwl_inst->frm_base;

    /* for DPB offset tracing */
    dpb_base_address = real_dpb_base_address = (u8 *)dwl_inst->frm_base;
    dpb_base_address -= last_ref_frm_size;
  }

  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);

  /* Check that we have enough memory to spare */
  if (dwl_inst->free_ref_frm_mem + size > dwl_inst->frm_base + dwl_inst->reference_maximum)
    return DWL_ERROR;

  info->virtual_address = (u32 *)dwl_inst->free_ref_frm_mem;
  info->bus_address = (addr_t)info->virtual_address;
  info->size = size;

  dwl_inst->free_ref_frm_mem += size;
#else
  printf("DWLMallocRefFrm: %8d\n", size);
  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
  info->virtual_address = (u32 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, size);
  if (info->virtual_address == NULL) return DWL_ERROR;
#ifdef _DWL_MEMSET_BUFFER
  DWLmemset(info->virtual_address, 0x80, size);
#endif
  info->bus_address = (addr_t)info->virtual_address;
  info->size = size;
#endif /* ASIC_TRACE_SUPPORT */

#ifdef _DWL_PERFORMANCE
  reference_total_max += size;
#endif /* _DWL_PERFORMANCE */

  dwl_inst->reference_total += size;
  dwl_inst->reference_alloc_count++;
  printf("DWLMallocRefFrm: memory allocated %8d bytes in %2d buffers @ %p (type %d)\n",
         dwl_inst->reference_total, dwl_inst->reference_alloc_count, (void *)info->virtual_address, info->mem_type);
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        DWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : struct DWLLinearMem *info - frame buffer memory
information
------------------------------------------------------------------------------*/
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem *info) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  assert(dwl_inst != NULL);

  printf("DWLFreeRefFrm: %8d\n", info->size);
  dwl_inst->reference_total -= info->size;
  dwl_inst->reference_alloc_count--;
  printf("DWLFreeRefFrm: not freed %8d bytes in %2d buffers @ %p\n",
         dwl_inst->reference_total, dwl_inst->reference_alloc_count, (void *)info->virtual_address);

#ifdef ASIC_TRACE_SUPPORT
  /* Release memory when calling DWLFreeRefFrm for the last buffer */
  if (dwl_inst->frm_base && (dwl_inst->reference_alloc_count == 0)) {
    assert(dwl_inst->reference_total == 0);
    last_ref_frm_size = dwl_inst->free_ref_frm_mem - dwl_inst->frm_base;
	osal_aligned_free(dwl_inst->frm_base);
    dwl_inst->frm_base = NULL;
    dpb_base_address = NULL;
  }
#else
  osal_aligned_free(info->virtual_address);
  info->size = 0;
#endif /* ASIC_TRACE_SUPPORT */
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : struct DWLLinearMem *info - place where the allocated
                        memory buffer parameters are returned
------------------------------------------------------------------------------*/
i32 DWLMallocLinear(const void *instance, u32 size, struct DWLLinearMem *info) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;

  if (DWLTestRandomFail()) {
    return DWL_ERROR;
  }
#ifdef ASIC_TRACE_SUPPORT
  alignment = DEC_X170_BUS_ADDR_ALIGNMENT;
#endif
  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
  info->virtual_address = (u32 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, size);
  printf("DWLMallocLinear: %8d\n", size);
  if (info->virtual_address == NULL) return DWL_ERROR;
  info->bus_address = (addr_t)info->virtual_address;

#ifdef _DWL_MEMSET_BUFFER
  DWLmemset(info->virtual_address, 0x80, size);
#endif

  info->size = size;
  dwl_inst->linear_total += size;
  dwl_inst->linear_alloc_count++;
  printf("DWLMallocLinear: allocated total %8d bytes in %2d buffers @ %p (type %d)\n",
         dwl_inst->linear_total, dwl_inst->linear_alloc_count, (void *)info->virtual_address, info->mem_type);

#ifdef _DWL_PERFORMANCE
  linear_total_max += size;
#endif /* _DWL_PERFORMANCE */
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        DWLMallocLinear.

    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : struct DWLLinearMem *info - linear buffer memory
information
------------------------------------------------------------------------------*/
void DWLFreeLinear(const void *instance, struct DWLLinearMem *info) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  assert(dwl_inst != NULL);

  printf("DWLFreeLinear: %8d\n", info->size);
  dwl_inst->linear_total -= info->size;
  dwl_inst->linear_alloc_count--;
  printf("DWLFreeLinear: not freed %8d bytes in %2d buffers @ %p\n",
         dwl_inst->linear_total, dwl_inst->linear_alloc_count, (void *)info->virtual_address);
  osal_aligned_free(info->virtual_address);
  info->size = 0;
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
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  //Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  //u32 *core_reg_base = HwCoreGetBaseAddress(c);
#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, value);
#endif
#ifdef INTERNAL_TEST
  InternalTestDumpWriteSwReg(core_id, offset >> 2, value, core_reg_base);
#endif
  assert(offset <= DEC_X170_REGS * 4);

  UNUSED(dwl_inst);
  dwl_shadow_regs[core_id][offset >> 2] = value;
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

static Core last_dec_core = NULL;


#ifdef SUPPORT_CACHE
static void DWLConfigureCacheShaper(const void *instance, i32 core_id) {

  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  u32 *shadow_regs_base = dwl_shadow_regs[core_id];
#ifdef ASIC_ONL_SIM
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  u32 *core_reg_base = HwCoreGetBaseAddress(c);
#endif
  u32 tile_by_tile = 0;
  u32 no_chroma = 0;
  cache_exception_regs_num = 0;

#ifdef ASIC_ONL_SIM
#ifndef SUPPORT_MULTI_CORE
  if(l2_allocate_buf == 1){
    dpi_l2_update_buf_base(core_reg_base);
    l2_allocate_buf=0;
  }
#endif
#endif

  if (((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_HEVC ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_VP9 ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_H264_H10P ||
      ((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_AVS2)
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.shaper_enable)
#endif
      DWLWriteReg(dwl_inst, core_id, 4*3, (shadow_regs_base[3] | 0x08));
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.shaper_enable)
#endif
    DWLWriteReg(dwl_inst, core_id, 4*265, (shadow_regs_base[265] | 0x80000000));
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.dec_params.cache_version >= 0x5)
#endif
    DWLWriteReg(dwl_inst, core_id, 4*58, (shadow_regs_base[58] | 0x4000));
  tile_by_tile = dwl_inst->tile_by_tile;
  if (!tile_by_tile) {
    DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
      if (!dwl_inst->shaper_enable) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
    }
  } else {
    if ((dwl_inst->tile_id < dwl_inst->num_tiles) && dwl_inst->tile_id)
      DWLDisableCacheChannelALL(WR, core_id);
    if (!dwl_inst->tile_id)
      DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
    if (!dwl_inst->shaper_enable) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
      dwl_inst->tile_id++;
    }
  }
  if (no_chroma || dwl_inst->shaper_enable) {
    DWLWriteReg(dwl_inst, core_id, 4*3, (shadow_regs_base[3] & 0xFFFFFFF7));
    DWLWriteReg(dwl_inst, core_id, 4*265, (shadow_regs_base[265] & 0x7FFFFFFF));
#ifdef ASIC_TRACE_SUPPORT
    if (!tb_cfg.cache_enable)
      cmodel_cache_support = 0;
#endif
  }
  if ((((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_JPEG) &&
      !dwl_inst->ppu_cfg[0].shaper_enabled && !dwl_inst->ppu_cfg[1].shaper_enabled &&
      !dwl_inst->ppu_cfg[2].shaper_enabled && !dwl_inst->ppu_cfg[3].shaper_enabled &&
      !dwl_inst->ppu_cfg[4].shaper_enabled)
    DWLWriteReg(dwl_inst, core_id, 4*265, (shadow_regs_base[265] & 0x7FFFFFFF));
#ifdef ASIC_TRACE_SUPPORT
  cmodel_cache_support = tb_cfg.dec_params.cache_support;
  cmodel_cache_enable = tb_cfg.cache_enable;
#endif
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLEnableHw
    Description     :
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  u32 *core_reg_base = HwCoreGetBaseAddress(c);
  u32 i;
  (void)core_reg_base;
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  cur_core_id = core_id;
  core_reg_base_array[core_id] = core_reg_base;
  if ( cur_pic < max_pics_decode ){
  	start_mc_load = 1;
    while(!mc_load_end){
      usleep(50);
    }
    mc_load_end = 0;
  }
  cur_pic += 1;
#endif

#ifdef SUPPORT_DEC400
  int max_wait_time_dec400 = 10000; /* 10s in ms */
#endif
#endif
#ifdef SUPPORT_CACHE
//  u32 tile_by_tile;
//  u32 no_chroma = 0;
#endif

  /* There are some duplicated variables named "cmodel_xxx",
     they are used in cmodel instead of tb_cfg. */
#ifdef ASIC_TRACE_SUPPORT
  cmodel_first_trace_frame = tb_cfg.tb_params.first_trace_frame;
  cmodel_pipeline_e = tb_cfg.pp_params.pipeline_e;
  cmodel_extra_cu_ctrl_eof = tb_cfg.tb_params.extra_cu_ctrl_eof;
  cmodel_ref_frm_buffer_size = tb_cfg.tb_params.ref_frm_buffer_size;
  cmodel_in_width = tb_cfg.pp_params.in_width;
  cmodel_in_height = tb_cfg.pp_params.in_height;
  cmodel_cache_support = tb_cfg.dec_params.cache_support;
  cmodel_cache_enable = tb_cfg.cache_enable;
  cmodel_unified_reg_fmt = tb_cfg.tb_params.unified_reg_fmt;
  for (i = 0; i < 10; i++)
    shaper_flag[10] = 0;
#endif

  if (!IS_PIPELINE_ENABLED(value)) assert(c == dwl_inst->current_core);
#ifdef SUPPORT_CACHE

#ifdef ASIC_ONL_SIM
#ifndef SUPPORT_MULTI_CORE
  if(l2_allocate_buf == 1){
    dpi_l2_update_buf_base(core_reg_base);
    l2_allocate_buf=0;
  }
#endif
#endif

  /* Cache/Shaper is configured in DWLEnableCmdBuf if vcmd is enabled. */
  if (!dwl_inst->vcmd_enabled)
    DWLConfigureCacheShaper(instance, core_id);
#endif

#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_DEC400
  /*
  do {
    const unsigned int usec_dec400 = 1000; //1 ms polling interval 
    if (dec400_enable[core_id] == 0)
      break;
    usleep(usec_dec400);

    max_wait_time_dec400--;
  } while (max_wait_time_dec400 > 0);
  */
  dpi_DWLDecF1Configure(core_reg_base,instance,core_id);
  //dec400_enable[core_id] = 1;
#endif
#endif

#ifdef SUPPORT_CACHE
#ifdef ASIC_ONL_SIM
	L2_DEC400_configure_done = 1;
#endif
#endif

  DWLWriteReg(dwl_inst, core_id, offset, value);

  PERFORMANCE_STATIC_END(decode_pre_hw);
#ifdef PERFORMANCE_TEST
  ActivityTraceStartDec(&dwl_inst->activity);
#endif

  DWL_DEBUG("%s %d enabled by previous DWLWriteReg\n", "DEC", core_id);

  /* Flush dwl_shadow_regs to core registers. */
  for(i = DEC_X170_REGISTERS; i >= 1; --i) {
    core_reg_base[i] = dwl_shadow_regs[core_id][i];
  }

  if (dwl_inst->client_type != DWL_CLIENT_TYPE_PP) {
    HwCoreDecEnable(c);
  } else {
    /* standalone PP start */
    HwCorePpEnable(c, last_dec_core != NULL ? 0 : 1);
  }

  dwl_inst->core_usage_counts[core_id]++;
}

/*------------------------------------------------------------------------------
    Function name   : DWLDisableHw
    Description     :
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  DWLWriteReg(instance, core_id, offset, value);
  DWL_DEBUG("%s %d disabled by previous DWLWriteReg\n", "DEC", core_id);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  //Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  //u32 *core_reg_base = HwCoreGetBaseAddress(c);
  u32 *core_reg_base = dwl_shadow_regs[core_id];
  u32 val;

#ifdef INTERNAL_TEST
  InternalTestDumpReadSwReg(core_id, offset >> 2, core_reg_base[offset >> 2],
                            core_reg_base);
#endif

  assert(offset <= DEC_X170_REGS * 4);

  val = core_reg_base[offset >> 2];

#ifndef DWL_DISABLE_REG_PRINTS
  DWL_DEBUG("core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, val);
#endif
  UNUSED(dwl_inst);

  return val;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitDecHwReady
    Description     : Wait until decoder hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.
    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR
    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
i32 DWLWaitDecHwReady(const void *instance, i32 core_id, u32 timeout) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
#ifdef SUPPORT_CACHE
  u32 *core_reg_base = HwCoreGetBaseAddress(c);
  u32 asic_status;
#endif
  assert(c == dwl_inst->current_core);
  (void)timeout;

  if (HwCoreWaitDecRdy(c) != 0) {
    return (i32)DWL_HW_WAIT_ERROR;
  }
#ifdef SUPPORT_CACHE
  asic_status = (core_reg_base[1] >> 11) & 0x5FFF;
  if (asic_status & 0x8)
    DWLReleaseL2(dwl_inst, core_id);
#endif
  return (i32)DWL_HW_WAIT_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitPpHwReady
    Description     : Wait until hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.
    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR
    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
i32 DWLWaitPpHwReady(const void *instance, i32 core_id, u32 timeout) {

  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);

  assert(c == dwl_inst->current_core);
  (void)timeout;

  if (HwCoreWaitPpRdy(c) != 0) {
    return (i32)DWL_HW_WAIT_ERROR;
  }

#ifdef ASIC_TRACE_SUPPORT
  /* update swregister_accesses.trc */
  /* TODO pp is currently not used, so put the trace function into comments
     to disable compiler warning.
  TraceWaitPpEnd(); */
#endif

  return (i32)DWL_HW_WAIT_OK;
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
  struct DWLInstance *dec_dwl = (struct DWLInstance *)instance;
  Core c = GetCoreById(dec_dwl->hw_core_array, core_id);
  u32 *core_reg_base = HwCoreGetBaseAddress(c);

  i32 ret, i;
  assert(dec_dwl);
  switch (dec_dwl->client_type) {
  case DWL_CLIENT_TYPE_HEVC_DEC:
  case DWL_CLIENT_TYPE_VP9_DEC:
  case DWL_CLIENT_TYPE_AVS2_DEC:
  case DWL_CLIENT_TYPE_H264_DEC:
  case DWL_CLIENT_TYPE_MPEG4_DEC:
  case DWL_CLIENT_TYPE_JPEG_DEC:
  case DWL_CLIENT_TYPE_VC1_DEC:
  case DWL_CLIENT_TYPE_MPEG2_DEC:
  case DWL_CLIENT_TYPE_AVS_DEC:
  case DWL_CLIENT_TYPE_RV_DEC:
  case DWL_CLIENT_TYPE_VP6_DEC:
  case DWL_CLIENT_TYPE_VP8_DEC:
  case DWL_CLIENT_TYPE_ST_PP:
    ret = DWLWaitDecHwReady(dec_dwl, core_id, timeout);
    break;
  case DWL_CLIENT_TYPE_PP:
    ret = DWLWaitPpHwReady(dec_dwl, core_id, timeout);
    break;
  default:
    assert(0); /* should not happen */
    ret = DWL_HW_WAIT_ERROR;
    break;
  }
#ifdef PERFORMANCE_TEST
  ActivityTraceStopDec(&dec_dwl->activity);
  hw_time_use = dec_dwl->activity.active_time / 100;
#endif
  PERFORMANCE_STATIC_START(decode_post_hw);

  /* Refresh dwl_shadow_regs from hw core registers. */
  for(i = DEC_X170_REGISTERS; i >= 0; --i) {
    dwl_shadow_regs[core_id][i] = core_reg_base[i];
  }

#ifdef _DWL_DEBUG
  {
    u32 irq_stats = DWLReadReg(instance, core_id, 4);

    PrintIrqType(core_id, irq_stats);
  }
#endif

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
  if (DWLTestRandomFail()) {
    return NULL;
  }
  DWL_DEBUG("%8d\n", n);

#ifdef _DWL_PERFORMANCE
  malloc_total_max += n;
#endif /* _DWL_PERFORMANCE */

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
  free(p);
}

/*------------------------------------------------------------------------------
    Function name   : DWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()
    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available
    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *DWLcalloc(u32 n, u32 s) {
  if (DWLTestRandomFail()) {
    return NULL;
  }
  DWL_DEBUG("%8d\n", n * s);
#ifdef _DWL_PERFORMANCE
  malloc_total_max += n * s;
#endif /* _DWL_PERFORMANCE */

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
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
------------------------------------------------------------------------------*/
i32 DWLReserveHw(const void *instance, i32 *core_id, u32 client_type) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  if (client_type == DWL_CLIENT_TYPE_PP) {
    pthread_mutex_lock(&pp_mutex);

    if (last_dec_core == NULL) /* Blocks until core available. */
      dwl_inst->current_core = BorrowHwCore(dwl_inst->hw_core_array);
    else
      /* We rely on the fact that in combined mode the PP is always reserved
       * after the decoder
       */
      dwl_inst->current_core = last_dec_core;
  } else {
    /* Blocks until core available. */
    dwl_inst->current_core = BorrowHwCore(dwl_inst->hw_core_array);
    last_dec_core = dwl_inst->current_core;
  }

  *core_id = HwCoreGetid(dwl_inst->current_core);
  DWL_DEBUG("Reserved %s core %d\n",
            client_type == DWL_CLIENT_TYPE_PP ? "PP" : "DEC",
            *core_id);

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHwPipe
    Description     : Reserve both DEC and PP on same core for pipeline
    Return type     : i32
    Argument        : const void *instance
------------------------------------------------------------------------------*/
i32 DWLReserveHwPipe(const void *instance, i32 *core_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;

  /* only decoder can reserve a DEC+PP hardware for pipelined operation */
  assert(dwl_inst->client_type != DWL_CLIENT_TYPE_PP);

  /* Blocks until core available. */
  dwl_inst->current_core = BorrowHwCore(dwl_inst->hw_core_array);
  last_dec_core = dwl_inst->current_core;

  /* lock PP also */
  pthread_mutex_lock(&pp_mutex);

  dwl_inst->b_reserved_pipe = 1;

  *core_id = HwCoreGetid(dwl_inst->current_core);
  DWL_DEBUG("Reserved DEC+PP core %d\n", *core_id);

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
u32 DWLReleaseHw(const void *instance, i32 core_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  if (dwl_inst->client_type == DWL_CLIENT_TYPE_PP) {
    DWL_DEBUG("Released PP core %d\n", core_id);

    pthread_mutex_unlock(&pp_mutex);

    /* core will be released by decoder */
    if (last_dec_core != NULL) return (-1);
  }

  /* PP reserved by decoder in DWLReserveHwPipe */
  if (dwl_inst->b_reserved_pipe) pthread_mutex_unlock(&pp_mutex);

  dwl_inst->b_reserved_pipe = 0;

  ReturnHwCore(dwl_inst->hw_core_array, c);
  last_dec_core = NULL;
  DWL_DEBUG("Released %s core %d\n",
            dwl_inst->client_type == DWL_CLIENT_TYPE_PP ? "PP" : "DEC",
            core_id);
#ifdef SUPPORT_CACHE
  dwl_inst->tile_id = 0;
  DWLDisableCacheChannelALL(BI, core_id);
  dwl_inst->shaper_enable = 0;
#ifdef ASIC_TRACE_SUPPORT
  stream_buffer_id = 0;
#endif
#endif
  return 0;
}

void DWLReleaseL2(const void *instance, i32 core_id) {
  av_unused struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  assert(dwl_inst != NULL);
#ifdef SUPPORT_CACHE
  DWLDisableCacheChannelALL(RD, core_id);
  dwl_inst->shaper_enable = 1;
#ifdef ASIC_TRACE_SUPPORT
  struct ChannelConf channel_cfg;
  (void) DWLmemset(&channel_cfg,0,sizeof(struct ChannelConf));
  channel_cfg.hw_dec_pic_count = hw_dec_pic_count;
  channel_cfg.stream_buffer_id = stream_buffer_id;
  if (tb_cfg.cache_enable)
    DWLPrintfInfo(&channel_cfg, core_id);
#endif
#endif
} 
/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicCoreCount
    Description     : Return number of ASIC cores, static implementation
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 DWLReadAsicCoreCount(void) {
  return GetCoreCount();
}

i32 DWLTestRandomFail(void) {
#ifdef FAIL_DURING_ALLOC
  if (!failed_alloc_count) {
    srand(time(NULL));
  }
  failed_alloc_count++;

  /* If fail preset to this alloc occurance, failt it */
  if (failed_alloc_count == FAIL_DURING_ALLOC) {
    printf("DWL: Preset allocation fail during alloc %d\n", failed_alloc_count);
    return DWL_ERROR;
  }
  /* If failing point is preset, no randomization */
  if (FAIL_DURING_ALLOC > 0) return DWL_OK;

  if ((rand() % 100) > 90) {
    printf("DWL: Testing a failure in memory allocation number %d\n",
           failed_alloc_count);
    return DWL_ERROR;
  } else {
    return DWL_OK;
  }
#endif
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadPpConfigure
    Description     : Read the pp configure
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : PpUnitIntConfig *ppu_cfg
------------------------------------------------------------------------------*/
void DWLReadPpConfigure(const void *instance, void *ppu_cfg, u16* tiles, u32 tile_enable)
{
    struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
    dwl_inst->tiles = tiles;
    if (dwl_inst->client_type == DWL_CLIENT_TYPE_JPEG_DEC) {
      dwl_inst->pjpeg_coeff_buffer_size = tile_enable;
      dwl_inst->tile_by_tile = 0;
    } else
      dwl_inst->tile_by_tile = tile_enable;
    DWLmemcpy(dwl_inst->ppu_cfg, ppu_cfg, DEC_MAX_PPU_COUNT*sizeof(PpUnitIntConfig));
}

void DWLReadMcRefBuffer(const void *instance, u32 core_id, u64 buf_size, u32 dpb_size) {
    struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
    dwl_inst->cache_cfg[core_id].buf_size = buf_size;
    dwl_inst->cache_cfg[core_id].dpb_size = dpb_size;
}
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_DEC400
/*------------------------------------------------------------------------------
    Function name   : DWLDecF1Configure
    Description     : configure the dec400 F1 before enable the decoding.
    
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/
void dpi_DWLDecF1Configure(u32 *reg_base, const void *instance,i32 core_id) {
  /*
   * when input stream is  yuv/Rgb, Resize pic(PP output) and origin pic should enable dec compression.
   * when input stream is hevc/vp9, Resize pic(PP output) should enable dec compression. 
   * when input stream is h264,  ref pic and resize pic(PP output) should enable dec compression.
  */
  int i = 0;
  unsigned int y_addr = 0;
  unsigned int uv_addr = 0;
  unsigned int y_size = 0;
  unsigned int uv_size = 0;
  unsigned int mode = 0;
  unsigned int mono_chroma = 0;
  unsigned int w_offset = 0;
  unsigned int pic_interlace=0;
  unsigned int frame_mbs_only_flag = 0;
  unsigned int num_tile_column = 0;
  //unsigned int uv_sel = 0;

  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;

  mode = (reg_base[3]>>27)&0x1F;
  printf("start the dec f1 cfg ,mode = %d!!\n", mode);
  
  mono_chroma = (reg_base[7]>>30)&0x01;
  if(mode == DWL_CLIENT_TYPE_H264_DEC) {
    frame_mbs_only_flag = ((reg_base[5] & 0x01) == 0);
    pic_interlace = (reg_base[3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      printf("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return;
    }
  }
  if((mode == DEC_MODE_HEVC) || (mode == DEC_MODE_VP9)) {
    num_tile_column = (reg_base[10]>>17)&0x7F;
    if(num_tile_column > 1)
      return;
  }
  dpi_dec400_apb_op(gcregAHBDECControl, 0x00810000, 1);
  dpi_dec400_apb_op(gcregAHBDECIntrEnbl, 0xFFFFFFFF, 1);
  dpi_dec400_apb_op(gcregAHBDECControlEx, 0x000A0000, 1);//case806 setting
  //dpi_dec400_apb_op(gcregAHBDECControlEx, 0x00020000, 1);
  //dpi_dec400_apb_op(gcregAHBDECIntrEnblEx, 0xFFFFFFFF, 1);
  //dpi_dec400_apb_op(gcregAHBDECControlEx2, 0x0000043f, 1);
  dpi_dec400_apb_op(gcregAHBDECIntrEnblEx2, 0xFFFFFFFF, 1);

  u32 pp_y_addr_reg[DEC_MAX_PPU_COUNT] = {326,348,365,382,453};
  u32 pp_c_addr_reg[DEC_MAX_PPU_COUNT] = {328,350,367,384,455};
  u32 pp_total_buffer_size=0;
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;
  u32 dpi_ppu_base = (u32)(HW_TB_PP_LUMA_BASE);

  u32 y_tbl_size[DEC_MAX_PPU_COUNT];
  u32 y_tbl_base[DEC_MAX_PPU_COUNT];
  u32 c_tbl_size[DEC_MAX_PPU_COUNT];
  u32 c_tbl_base[DEC_MAX_PPU_COUNT];

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[i].enabled == 1){
      reg_addr_y = pp_y_addr_reg[i];
      reg_addr_c = pp_c_addr_reg[i];
      y_addr = reg_base[reg_addr_y];
      uv_addr = reg_base[reg_addr_c];

      if(dwl_inst->ppu_cfg[i].tiled_e == 1){
        y_size = dwl_inst->ppu_cfg[i].ystride * NEXT_MULTIPLE(dwl_inst->ppu_cfg[i].scale.height,4)/4;
        uv_size = dwl_inst->ppu_cfg[i].cstride * NEXT_MULTIPLE(((dwl_inst->ppu_cfg[i].scale.height + 1)/ 2),4)/4;
      }else{
        y_size = dwl_inst->ppu_cfg[i].ystride * dwl_inst->ppu_cfg[i].scale.height;
        uv_size = dwl_inst->ppu_cfg[i].cstride * ((dwl_inst->ppu_cfg[i].scale.height + 1)/ 2);
      }
			
      if(pp_bus_address_start == 0)
        pp_bus_address_start = reg_base[pp_y_addr_reg[i]];
      if(dwl_inst->ppu_cfg[i].pixel_width == 8) {
        dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset, 0x0E020029, 1);
        dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset, 0<<16, 1);
        if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
          dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset + 0x4, 0x10020031, 1);
          dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset + 0x4, 0<<16, 1);
        }
      } else {
          dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset, 0x10020029, 1);
          dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset, 1<<16, 1);
          if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
            dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset + 0x4, 0x04020031, 1);
            dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset + 0x4, 1<<16, 1);
          }
      }
      dpi_dec400_apb_op(gcregAHBDECWriteBufferBase + w_offset, dpi_ppu_base, 1);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferBaseEx + w_offset, 0x00000000, 1);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferEnd + w_offset, dpi_ppu_base + y_size - 1, 1);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferEndEx + w_offset, 0x00000000, 1);
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {	
        dpi_dec400_apb_op(gcregAHBDECWriteBufferBase + w_offset + 0x4,dpi_ppu_base + y_size , 1);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferBaseEx + w_offset + 0x4, 0x00000000, 1);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferEnd + w_offset + 0x4, dpi_ppu_base + y_size + uv_size  - 1, 1);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferEndEx + w_offset + 0x4, 0x00000000, 1);
      }
      w_offset += 0x8;
			
      pp_total_buffer_size += y_size;
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome))
        pp_total_buffer_size += uv_size;

      dpi_ppu_base = dpi_ppu_base + y_size + uv_size;

      pp_enable_channel++;
    }
  }
  w_offset = 0;

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++ ){
    if(dwl_inst->ppu_cfg[i].enabled == 1) {
      y_tbl_size[i] = (((NEXT_MULTIPLE(dwl_inst->ppu_cfg[i].ystride * dwl_inst->ppu_cfg[i].scale.height,ALIGN(dwl_inst->ppu_cfg[i].align))/ALIGN(dwl_inst->ppu_cfg[i].align))*4+7)/8+15)/16*16;
      c_tbl_size[i] = (((NEXT_MULTIPLE(dwl_inst->ppu_cfg[i].ystride * (dwl_inst->ppu_cfg[i].scale.height+1)/2,ALIGN(dwl_inst->ppu_cfg[i].align))/ALIGN(dwl_inst->ppu_cfg[i].align))*4+7)/8+15)/16*16;

      if ( i == 0 ){
        y_tbl_base[i] = dpi_ppu_base;
      } else {
        y_tbl_base[i] = c_tbl_base[i-1]+c_tbl_size[i-1];
      }
      c_tbl_base[i] = y_tbl_base[i] + y_tbl_size[i];
    }
  }

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[i].enabled == 1) {
      dpi_dec400_apb_op(gcregAHBDECWriteCacheBase + w_offset,y_tbl_base[i], 1);
      dpi_dec400_apb_op(gcregAHBDECWriteCacheBaseEx + w_offset, 0x00000000, 1);
      if (!(mono_chroma || dwl_inst->ppu_cfg[i].monochrome)) {
        dpi_dec400_apb_op(gcregAHBDECWriteCacheBase + w_offset + 0x4, c_tbl_base[i], 1);
        dpi_dec400_apb_op(gcregAHBDECWriteCacheBaseEx + w_offset+ 0x4, 0x00000000, 1);
      }  
      w_offset += 0x8;
    }
  }
#if 0
  printf("gcregAHBDECControl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControl));
  printf("gcregAHBDECControlEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControlEx));
  printf("gcregAHBDECControlEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControlEx2));
  printf("gcregAHBDECIntrEnbl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnbl));
  printf("gcregAHBDECIntrEnblEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnblEx));
  printf("gcregAHBDECIntrEnblEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnblEx2));
  printf("luma configure\n");
  printf("gcregAHBDECWriteConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteConfig));
  printf("gcregAHBDECWriteExConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteExConfig));
  printf("gcregAHBDECWriteBufferBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBase));
  printf("gcregAHBDECWriteBufferBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBaseEx));
  printf("gcregAHBDECWriteBufferEnd=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEnd));
  printf("gcregAHBDECWriteBufferEndEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEndEx));
  printf("gcregAHBDECWriteCacheBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBase));
  printf("gcregAHBDECWriteCacheBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBaseEx));
  printf("chroma configure\n");
  printf("gcregAHBDECWriteConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteConfig+0x4));
  printf("gcregAHBDECWriteExConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteExConfig+0x4));
  printf("gcregAHBDECWriteBufferBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBase+0x4));
  printf("gcregAHBDECWriteBufferBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBaseEx+0x4));
  printf("gcregAHBDECWriteBufferEnd+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEnd+0x4));
  printf("gcregAHBDECWriteBufferEndEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEndEx+0x4));
  printf("gcregAHBDECWriteCacheBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBase+0x4));
  printf("gcregAHBDECWriteCacheBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBaseEx+0x4));
#endif
}
#endif // SUPPORT_DEC400
#endif // ASIC_ONL_SIM

/* Reserve one valid command buffer. */
i32 DWLReserveCmdBuf(const void *instance, u32 client_type, u32 width, u32 height, u32 *cmd_buf_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  i32 ret = DWL_ERROR;

  struct exchange_parameter params = {0};
  params.executing_time = width * height;
  params.module_type = 2;   /* VC8000D */
  params.cmdbuf_size = VCMD_BUF_SIZE;

  DWL_DEBUG("%s", "enter\n");

  dwl_inst->vcmd_enabled = 1;
#ifdef SUPPORT_CACHE
  extern u32 vcmd_used;
  vcmd_used = 1;
#endif
  pthread_mutex_lock(&dwl_inst->vcmd_mutex);
  ret = CmodelIoctlReserveCmdbuf(&params);
  if (ret < 0) {
    DWL_DEBUG("%s", "DWLReserveCmdBuf failed\n");
    ret = DWL_ERROR;
  } else {
    int cmdbuf_id = params.cmdbuf_id;
    DWL_DEBUG("reserve cmd buf id %d\n", cmdbuf_id);
    dwl_inst->vcmd[cmdbuf_id].valid = 0;
    dwl_inst->vcmd[cmdbuf_id].client_type = client_type;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf_size = params.cmdbuf_size;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf_used = 0;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmd[cmdbuf_id].status_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_status_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmd[cmdbuf_id].status_bus_addr = dwl_inst->vcmd_mem_params.phy_status_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    *cmd_buf_id = cmdbuf_id;

    ret = DWL_OK;
  }

  pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
  
  return ret;
}

#if 0
/* Append an instruction to command buffer. Instruction payload is in @data. */
i32 DWLAppendInstToCmdBuf(const void *instance, u32 cmd_buf_id,
                           u32 opcode, u32 opdata1, u32 opdata2, u32 opdata3,
                           void *data, u32 size) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  i32 core_id;
  pthread_mutex_lock(&vcmd_mutex);

  pthread_mutex_unlock(&vcmd_mutex);
  return DWL_OK;
}
#endif

/* Reserve one valid command buffer. */
i32 DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  u32 instr_size;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmd_buf_id];
  struct exchange_parameter params;
  i32 ret;
#ifdef SUPPORT_CACHE
  u32 cache_reg_num, shaper_reg_num;
#endif

  DWL_DEBUG("enable cmd buf id %d\n", cmd_buf_id);

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
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.shaper_enable)
#endif
  DWLWriteReg(dwl_inst, core_id, 4*3, (dwl_shadow_regs[core_id][3] | 0x08));
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.shaper_enable)
#endif
  DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] | 0x80000000));
#ifdef ASIC_TRACE_SUPPORT
  if (tb_cfg.dec_params.cache_version >= 0x5)
#endif
    DWLWriteReg(dwl_inst, core_id, 4*58, ((dwl_shadow_regs[core_id][58] & 0xFFFFFF00) | 0x10));
  tile_by_tile = dwl_inst->tile_by_tile;
  if (tile_by_tile) {
    if (dwl_inst->tile_id) {
      if (dwl_inst->tile_id < dwl_inst->num_tiles)
        DWLDisableCacheChannelALL(WR, core_id);
      cache_enable[core_id] = 0;
    }
  }

  if (!tile_by_tile) {
    DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
      if (!dwl_inst->shaper_enable) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
    }
  } else {
    if ((dwl_inst->tile_id < dwl_inst->num_tiles) && dwl_inst->tile_id)
      DWLDisableCacheChannelALL(WR, core_id);
    if (!dwl_inst->tile_id)
      DWLConfigureCacheChannel(instance, core_id, dwl_inst->cache_cfg, dwl_inst->pjpeg_coeff_buffer_size);
    if (!dwl_inst->shaper_enable) {
      u32 pp_enabled = dwl_inst->ppu_cfg[0].enabled |
                       dwl_inst->ppu_cfg[1].enabled |
                       dwl_inst->ppu_cfg[2].enabled |
                       dwl_inst->ppu_cfg[3].enabled |
                       dwl_inst->ppu_cfg[4].enabled;
      DWLConfigureShaperChannel(instance, core_id, dwl_inst->ppu_cfg, pp_enabled, tile_by_tile, dwl_inst->tile_id, &(dwl_inst->num_tiles), dwl_inst->tiles, &no_chroma);
      dwl_inst->tile_id++;
    }
  }
  if (no_chroma || dwl_inst->shaper_enable) {
    DWLWriteReg(dwl_inst, core_id, 4*3, (dwl_shadow_regs[core_id][3] & 0xFFFFFFF7));
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));
#ifdef ASIC_TRACE_SUPPORT
    if (!tb_cfg.cache_enable)
      cmodel_cache_support = 0;
#endif
  }
  if ((((DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F) == DEC_MODE_JPEG) &&
      !dwl_inst->ppu_cfg[0].shaper_enabled && !dwl_inst->ppu_cfg[1].shaper_enabled &&
      !dwl_inst->ppu_cfg[2].shaper_enabled && !dwl_inst->ppu_cfg[3].shaper_enabled &&
      !dwl_inst->ppu_cfg[4].shaper_enabled)
    DWLWriteReg(dwl_inst, core_id, 4*265, (dwl_shadow_regs[core_id][265] & 0x7FFFFFFF));

  DWLRefreshCacheRegs(cache_shadow_regs, &cache_reg_num, shaper_shadow_regs, &shaper_reg_num);

  }
#endif

  /****************************************************************************/
  /* Start to generate VCMD instructions. */
  if (dwl_inst->vcmd_params.vcmd_hw_version_id > VCMD_HW_ID_1_0_C) {
    /* Read VCMD buffer ID (last VCMD registers). */
    CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                          26, 1, /* VCMD command buffer ID register */
                          &instr_size, 0);
    vcmd->cmd_buf_used += instr_size * 4;
  }

#ifdef SUPPORT_CACHE
  /* Append write L2Cache/shaper instructions. */
  if (cache_reg_num) {
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
    CWLCollectWriteRegData(&cache_shadow_regs[1],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                           1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;
  }

  /* Configure shaper instruction */
  if (shaper_reg_num) {
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
  CWLCollectWriteRegData(&dwl_shadow_regs[0][2],
                        (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                        dwl_inst->vcmd_params.submodule_main_addr/4 + 2 , /* register offset in bytes to vcmd base address */
                        510, &instr_size);
  vcmd->cmd_buf_used += instr_size * 4;
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
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr);
  vcmd->cmd_buf_used += instr_size * 4;
  /* Read status (swreg1) register */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 1, 1, /* swreg1 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4);
  vcmd->cmd_buf_used += instr_size * 4;
  /* swreg168/169 */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 168, 2, /* swreg168/169 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 2);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg63 - sw_perf_cycle_count */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 63, 1, /* swreg63 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 4);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg62 - mb pos for mc decoding */
  CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 62, 1, /* swreg62 */
                      &instr_size,
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 5);
  vcmd->cmd_buf_used += instr_size * 4;
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);

#ifdef SUPPORT_CACHE
  {
    u32 core_id = 0;
    DWLDisableCacheChannelALL(BI, core_id);
    dwl_inst->shaper_enable = 0;
    cache_enable[core_id] = 0;

    /* Disable cache */
    cache_shadow_regs[1] = 0;
    cache_shadow_regs[2] = 0;
    CWLCollectWriteRegData(&cache_shadow_regs[1],
                           (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                           dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + CACHE_REG_OFFSET + 1, /* cache_swreg1 */
                            2, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;

    /* Disable shaper to flush it. */
    shaper_shadow_regs[0] = 0;
    CWLCollectWriteRegData(&shaper_shadow_regs[0],
                          (u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                          dwl_inst->vcmd_params.submodule_L2Cache_addr/4 + SHAPER_REG_OFFSET, /* shaper reg0 in 4-byte to vcmd base address */
                          1, &instr_size);
    vcmd->cmd_buf_used += instr_size * 4;

    /* Wait interrupt from shaper */
    CWLCollectStallData((u32 *) (vcmd->cmd_buf + vcmd->cmd_buf_used),
                        &instr_size,
                        (dwl_inst->vcmd_params.vcmd_hw_version_id == VCMD_HW_ID_1_0_C ?
                          VC8000D_L2CACHE_INT_MASK_1_0_C : VC8000D_L2CACHE_INT_MASK));
    vcmd->cmd_buf_used += instr_size * 4;
    ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  }
#endif

  if (dwl_inst->vcmd_params.vcmd_hw_version_id > VCMD_HW_ID_1_0_C) {
    /* Dump all vcmd registers. */
    CWLCollectReadRegData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used),
                          0, 27, /* VCMD registers count */
                          &instr_size, 0);
    vcmd->cmd_buf_used += instr_size * 4;
  }

  /* Jmp */
  CWLCollectJmpData((u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used), &instr_size, cmd_buf_id);
  vcmd->cmd_buf_used += instr_size * 4;


  params.cmdbuf_size = dwl_inst->vcmd[cmd_buf_id].cmd_buf_used;
  params.cmdbuf_id = cmd_buf_id;
  params.client_type = dwl_inst->client_type;
  params.module_type = 2;   /* VC8000D */

#ifdef ASIC_TRACE_SUPPORT
  {
    i32 i;
    cmodel_first_trace_frame = tb_cfg.tb_params.first_trace_frame;
    cmodel_pipeline_e = tb_cfg.pp_params.pipeline_e;
    cmodel_extra_cu_ctrl_eof = tb_cfg.tb_params.extra_cu_ctrl_eof;
    cmodel_ref_frm_buffer_size = tb_cfg.tb_params.ref_frm_buffer_size;
    cmodel_in_width = tb_cfg.pp_params.in_width;
    cmodel_in_height = tb_cfg.pp_params.in_height;
    cmodel_cache_support = tb_cfg.dec_params.cache_support;
    cmodel_cache_enable = tb_cfg.cache_enable;
    cmodel_unified_reg_fmt = tb_cfg.tb_params.unified_reg_fmt;
    for (i = 0; i < 10; i++)
      shaper_flag[10] = 0;
  }
#endif

  ret = CmodelIoctlEnableCmdbuf(&params);
  if (ret < 0) {
    DWL_DEBUG("%s", "CmodelIoctlEnableCmdbuf failed\n");
    pthread_mutex_unlock(&dwl_inst->vcmd_mutex);
    return DWL_ERROR;
  }

  vcmd->core_id = params.core_id;

  pthread_mutex_unlock(&dwl_inst->vcmd_mutex);

  dwl_inst->core_usage_counts[vcmd->core_id]++;

  return DWL_OK;
}

/* Wait cmd buffer ready. Used only in single core decoding. */
i32 DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  u32 * status = NULL;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmd_buf_id];
  i32 ret = 0;

  /* Check invalid parameters */
  if(dwl_inst == NULL)
    return DWL_ERROR;

  DWL_DEBUG("%s", "DWLWaitCmdBufReady\n");
#if 0
  ret = CmodelIoctlWaitCmdbuf(&cmd_buf_id);
#else
  sem_wait(listener_thread_params.sc_dec_rdy_sem+cmd_buf_id);
#endif
  if (ret < 0) {
    DWL_DEBUG("%s", "DWLWaitCmdBufReady failed\n");
    return DWL_HW_WAIT_ERROR;
  } else {
    DWL_DEBUG("%s", "DWLWaitCmdBufReady succeed\n");
    status = (u32 *)(vcmd->status_buf + dwl_inst->vcmd_params.submodule_main_addr);
    status++; // skip swreg0
    DWLWriteReg(instance, 0, 1 * 4, *status++);
    DWLWriteReg(instance, 0, 168 * 4, *status++);
    DWLWriteReg(instance, 0, 169 * 4, *status++);
    DWLWriteReg(instance, 0, 63 * 4, *status++);
  }
  
  return DWL_OK;
}

/* Reserve one valid command buffer. */
i32 DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;
  i32 ret;

  ASSERT (cmd_buf_id < MAX_VCMD_ENTRIES);
  ASSERT(!dwl_inst->vcmd[cmd_buf_id].valid);

  DWL_DEBUG("release cmd buf id %d\n", cmd_buf_id);

  pthread_mutex_lock(&dwl_inst->vcmd_mutex);

  ret = CmodelIoctlReleaseCmdbuf(cmd_buf_id);
  if (ret) {
    DWL_DEBUG("%s", "DWLReleaseCmdBuf failed\n");
    return DWL_ERROR;
  }
  dwl_inst->vcmd[cmd_buf_id].valid = 1;
  dwl_inst->vcmd[cmd_buf_id].cmd_buf_used = 0;

  pthread_mutex_unlock(&dwl_inst->vcmd_mutex);

  return DWL_OK;
}

i32 DWLWaitCmdbufsDone(const void *instance) {
  struct DWLInstance *dwl_inst = (struct DWLInstance *)instance;

  u32 i;
  /* cmdbuf 0 and 1 not used for user */
  for (i = 2; i < MAX_VCMD_ENTRIES; i++) {
    if( dwl_inst->vcmd[i].valid == 0) {
      i = 1;
      sched_yield();
    }
  }
  return 0;
}

u32 DWLVcmdCores(void) {
#ifdef SUPPORT_VCMD
  return 1;
#else
  return 0;
#endif
}

