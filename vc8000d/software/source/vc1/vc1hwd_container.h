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

#ifndef VC1HWD_CONTAINER_H
#define VC1HWD_CONTAINER_H

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/

#include "vc1hwd_storage.h"
#include "deccfg.h"
#include "decppif.h"
#include "refbuffer.h"
#include "input_queue.h"
#include "ppu.h"
#include "fifo.h"

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/* String length for tracing */
#define VC1DEC_TRACE_STR_LEN 100

/*------------------------------------------------------------------------------
    Data types
------------------------------------------------------------------------------*/

typedef struct {
  enum {
    VC1DEC_UNINITIALIZED,
    VC1DEC_INITIALIZED,
    VC1DEC_HEADERSDECODED,
    VC1DEC_STREAMDECODING,
    VC1DEC_STREAM_END,
  } dec_stat;

  swStrmStorage_t storage;
#ifdef VC1DEC_TRACE
  char str[VC1DEC_TRACE_STR_LEN];
#endif
  u32 pic_number;
  u32 asic_running;
  u32 vc1_regs[TOTAL_X170_REGISTERS];

  u32 max_width_hw;

  u32 ref_buf_support;
  u32 tiled_mode_support;
  u32 tiled_reference_enable;
  u32 allow_dpb_field_ordering;
  u32 dpb_mode;
  struct refBuffer ref_buffer_ctrl;
  struct DWLLinearMem bit_plane_ctrl;
  struct DWLLinearMem direct_mvs;
  const void *dwl;    /* DWL instance */
  i32 core_id;
  u32 next_buf_size;  /* size of the requested external buffer */
  u32 buf_num;        /* number of buffers (with size of next_buf_size) requested to be allocated externally */
  struct DWLLinearMem *buf_to_free;

  u32 realloc_ext_buf;   /* flag to reallocate external buffer */
  u32 realloc_int_buf;   /* flag to reallocate internal buffer */
  u32 buffer_index;
  u32 ext_min_buffer_num;
  u32 ext_buffer_num;
  u32 n_ext_buf_size;        // size of external buffers added
  u32 n_int_buf_size;        // size of internal reference buffers(should alway be 0 if PP is disabled)
  u32 use_adaptive_buffers;
  u32 n_guard_size;
  u32 fullness;
  FifoInst fifo_display;
  u32 fifo_index;
  u32 abort;

  u32 dec_state;
  u32 dec_flag;
  pthread_mutex_t protect_mutex;
  u32 same_pic_header;
  u32 pp_enabled;     /* set to 1 to enable pp */
  u32 dscale_shift_x;
  u32 dscale_shift_y;

  PpUnitIntConfig ppu_cfg[DEC_MAX_PPU_COUNT];
  struct DWLLinearMem ext_buffers[16];
  u32 tiled_stride_enable;
  DecPicAlignment align;  /* buffer alignment for both reference/pp output */
  u32 cr_first;
  u32 max_strm_len;

  InputQueue pp_buffer_queue;
  u32 prev_pp_width;
  u32 prev_pp_height;
} decContainer_t;

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/

#endif /* #ifndef VC1HWD_CONTAINER_H */

