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

#ifndef __PPTESTBENCH_H__
#define __PPTESTBENCH_H__

#include "basetype.h"
#include "ppapi.h"

#include "tb_cfg.h"
#include "defs.h"

enum {
  CFG_UPDATE_OK = 0,
  CFG_UPDATE_FAIL = 1,
  CFG_UPDATE_NEW = 2
};
int pp_startup(char *pp_cfg_file, const void *dec_inst, u32 dec_type,
               const struct TBCfg* tb_cfg);

int pp_update_config(const void *dec_inst, u32 dec_type, const struct TBCfg* tb_cfg);

void pp_write_output(int frame, u32 field_output, u32 top);
void pp_write_output_plain(int frame);

void pp_close(void);

int pp_external_run(char *cfg_file, const struct TBCfg* tb_cfg);

int pp_alloc_buffers(PpParams *pp, u32 ablend_crop);
int pp_api_init(PPInst * pp, const void *dec_inst, u32 dec_type);
int pp_api_cfg(PPInst pp, const void *dec_inst, u32 dec_type,
               PpParams * params, const struct TBCfg* tb_cfg);

void pipeline_disable(void);
int pp_set_rotation();
int pp_set_cropping(void);
void pp_vc1_specific( u32 multires_enable, u32 rangered);
int pp_vc1_non_pipelined_pics(u32 bus_addr, u32 width,
                              u32 height,  u32 range_red,
                              u32 range_map_yenable, u32 range_map_ycoeff,
                              u32 range_map_cenable, u32 range_map_ccoeff,
                              u32 *virtual, u32 dec_output_endian_big,
                              u32 dec_output_tiled);
void pp_set_input_interlaced(u32 is_interlaced);
u32 pp_rotation_used(void);
u32 pp_crop_used(void);
u32 pp_mpeg4_filter_used(void);
u32 pp_deinterlace_used(void);

int pp_change_resolution(u32 width, u32 height, const struct TBCfg * tb_cfg);
void pp_number_of_buffers(u32 amount);

char *pp_get_out_name(void);

#endif /* __PPTESTBENCH_H__ */
