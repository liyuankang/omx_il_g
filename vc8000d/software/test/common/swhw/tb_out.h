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

#ifndef TB_OUT_H
#define TB_OUT_H

#include "basetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u32 TBWriteOutput(FILE* fout,u8* p_lu, u8 *p_ch,
                  u32 coded_width, u32 coded_height, u32 pic_stride,
                  u32 coded_width_ch, u32 coded_h_ch, u32 pic_stride_ch,
                  u32 md5Sum, u32 planar, u32 mono_chrome, u32 frame_number,
                  u32 pixel_bytes);
u32 TBWriteOutputRGB(FILE* fout,u8* data,
                     u32 coded_width, u32 coded_height, u32 pic_stride,
                     u32 md5Sum, u32 planar, u32 frame_number,
                     u32 pixel_bytes);

void WriteOutput(char *filename, u8 *data, u32 pic_width, u32 pic_height,
                 u32 frame_number, u32 mono_chrome, u32 view, u32 mvc_separate_views,
                 u32 disable_output_writing, u32 tiled_mode, u32 pic_stride,
                 u32 pic_stride_ch, u32 index, u32 planar, u32 cr_first,
                 u32 convert_tiled_output, u32 convert_to_frame_dpb,
                 u32 dpb_mode, u32 md5sum, FILE **fout, u32 pixel_bytes);
void WriteOutputRGB(char *filename, u8 *data, u32 pic_width, u32 pic_height,
		 u32 frame_number, u32 mono_chrome, u32 view, u32 mvc_separate_views,
		 u32 disable_output_writing, u32 tiled_mode, u32 pic_stride,
		 u32 pic_stride_ch, u32 index, u32 planar, u32 cr_first,
		 u32 convert_tiled_output, u32 convert_to_frame_dpb,
		 u32 dpb_mode, u32 md5sum, FILE **fout, u32 pixel_bytes);
void WriteOutputdec400(char *filename, u8 *data, u32 pic_width, u32 pic_height,
                 u32 frame_number, u32 mono_chrome, u32 view, u32 mvc_separate_views,
                 u32 disable_output_writing, u32 tiled_mode, u32 pic_stride,
                 u32 pic_stride_ch, u32 index, u32 planar, u32 cr_first,
                 u32 convert_tiled_output, u32 convert_to_frame_dpb,
                 u32 dpb_mode, u32 md5sum, FILE **fout, u32 pixel_bytes, FILE **fout_table,
                 u8 * y_table_data, u32 y_table_size,u8 * uv_table_data, u32 uv_table_size);
#endif
