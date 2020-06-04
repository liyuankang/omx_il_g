/*------------------------------------------------------------------------------
--       Copyright (c) 2019, VeriSilicon Inc. All rights reserved             --
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

#ifndef __TRACE_FILES_H__
#define __TRACE_FILES_H__

#ifndef NON_PTHREAD_H
#define _HAVE_PTHREAD_H
#endif

#include <stdio.h>
#ifdef _HAVE_PTHREAD_H
#include <stdlib.h>
#endif
#define TRACEDESC(INDEX,NAME,LEVEL,BIN,CNT,FORMAT) INDEX,
enum {
  #include "trace_list.h"
  /* don't change below */
  NUM_TRACES
};
#undef TRACEDESC

struct TraceFile {
  FILE *fid;
  char name[64];
  u32 top_level;
  u32 bin;
  u32 bytes;    /* Bytes written to this trace file. */
};

struct ReconOutCtrlTrc {
  u16 width;
  u8 height;
  u8 y_offset;
  u8 type;
  u8 tile_right_edge;
  u8 tile_bottom_edge;
  u8 pic_right_edge;
  u8 picture_ready;
  u8 _fill0;
};

struct ExtendedReconOutCtrlTrc {
  struct ReconOutCtrlTrc base;
  u16 x_coord_4x4;
  u16 y_coord_4x4;
  u8 tile_left_edge;
  u8 _fill0;
};

extern struct TraceFile trace_files[];

#endif

