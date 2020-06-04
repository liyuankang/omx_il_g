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

#ifndef ASIC_H
#define ASIC_H

#include "basetype.h"
#include "trace.h"
#if defined(_WIN64) || defined(_WIN32)
#include "osal_win32.h"
#endif

/*vcmd definition*/
typedef int (*hantrovcmd_isr_norm_callback)(int irq, void *dev_id);
typedef int (*hantrovcmd_isr_abn_callback)(int irq, void *dev_id);
struct vcmd_config
{
  unsigned long vcmd_base_addr;
  u32 vcmd_iosize;
  int vcmd_irq;
  u32 sub_module_type;  /*input vc8000e=0,IM=1,vc8000d=2,jpege=3, jpegd=4*/
  u16 submodule_main_addr; /* in byte*/
  u16 submodule_dec400_addr;/*if submodule addr == 0xffff, this submodule does not exist. in byte*/
  u16 submodule_L2Cache_addr; /* in byte*/
  u16 submodule_MMU_addr; /* in byte*/
};

struct HwVcmdCoreConfig {
  u32 vcmd_hw_version_id;
  struct vcmd_config* vcmd_core_config_ptr;
  int core_id;
  void * dev_id;
  hantrovcmd_isr_norm_callback normal_int_callback;
  hantrovcmd_isr_abn_callback  abnormal_int_callback;
};

/*the following 3 APIs for vcmd.If using vcmd APIs, wrapper APIs should not be used.
The two sets of APIs are mutually exclusive */

CMODEL_DLL_API const void *AsicHwVcmdCoreInit(struct HwVcmdCoreConfig* vcmd_core_config);
CMODEL_DLL_API void AsicHwVcmdCoreRelease(const void * instance);
CMODEL_DLL_API u32 *AsicHwVcmdCoreGetBase(const void * instance);


/* Wrapper */
CMODEL_DLL_API const void *AsicHwCoreInit(void);
CMODEL_DLL_API void AsicHwCoreRelease(void *instance);
CMODEL_DLL_API u32 *AsicHwCoreGetBase(void *instance);
CMODEL_DLL_API void AsicHwCoreRun(void *instance);

/* global variables for tool tracing */
CMODEL_DLL_API extern struct DecodingToolsTrc decoding_tools;
CMODEL_DLL_API extern u32 hw_dec_pic_count;
CMODEL_DLL_API extern u8 *dpb_base_address;
CMODEL_DLL_API extern u8 *real_dpb_base_address;
CMODEL_DLL_API extern u32 alignment;
CMODEL_DLL_API extern u32 test_case_id;
CMODEL_DLL_API extern u32 use_reference_idct;
CMODEL_DLL_API extern u32 use_mpeg2_idct;
CMODEL_DLL_API extern u32 use_jpeg_idct;
CMODEL_DLL_API extern u32 g_hw_build_id;
CMODEL_DLL_API extern u32 g_hw_id;
CMODEL_DLL_API extern u32 g_hw_ver;
CMODEL_DLL_API extern u32 h264_high_support;
CMODEL_DLL_API extern u32 stream_buffer_id;

CMODEL_DLL_API extern u32 cmodel_first_trace_frame;
CMODEL_DLL_API extern u32 cmodel_pipeline_e;
CMODEL_DLL_API extern u32 cmodel_extra_cu_ctrl_eof;
CMODEL_DLL_API extern u32 cmodel_ref_frm_buffer_size;
CMODEL_DLL_API extern u32 cmodel_in_width;
CMODEL_DLL_API extern u32 cmodel_in_height;
CMODEL_DLL_API extern u32 cmodel_cache_support;
CMODEL_DLL_API extern u32 cmodel_cache_enable;
CMODEL_DLL_API extern u32 cmodel_unified_reg_fmt;
CMODEL_DLL_API extern u32 cmodel_ref_buf_alignment;
/*
extern u32 cmodel_vert_down_scale_stripe_disable_support;
extern u32 cmodel_dll_ec_support;
extern u32 cmodel_ablend_crop_support;
extern u32 cmodel_dll_scaling_support;
extern u32 cmodel_bus_width64bit_enable;
extern u32 cmodel_latency;
extern u32 cmodel_non_seq_clk;
extern u32 cmodel_seq_clk;
*/
#endif /* #ifndef ASIC_H */
