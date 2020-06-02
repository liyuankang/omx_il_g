/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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

#ifndef AVS2_DECODER_H
#define AVS2_DECODER_H

#include "basetype.h"
#include "sw_debug.h"
#include "avs2_storage.h"
#include "avs2_container.h"
#include "avs2_dpb.h"

enum {
  CMEM_STREAM = 0,
  CMEM_TABLE,
  CMEM_REFS,
  CMEM_MV,
  CMEM_TYPE_MAX,
};

enum {
  AVS2_RDY,
  AVS2_PIC_RDY,
  AVS2_HDRS_RDY,
  AVS2_BUFFER_NOT_READY,
  AVS2_ABORTED,

#ifdef GET_FREE_BUFFER_NON_BLOCK
  AVS2_NO_FREE_BUFFER, /* GetFreePicBuffer() returns no free buffer. */
#endif
  AVS2_ERROR,
  AVS2_PARAM_SET_ERROR,
  AVS2_NEW_ACCESS_UNIT,
  AVS2_NONREF_PIC_SKIPPED
};

/* asic interface */
struct Avs2DecAsic {
  i32 chroma_qp_index_offset;
  i32 chroma_qp_index_offset2;
  u32 disable_out_writing;
  u32 asic_id;
};

void Avs2Init(struct Avs2DecContainer *dec_cont, u32 no_output_reordering);
u32 Avs2Decode(struct Avs2DecContainer *dec_cont, const u8 *byte_strm, u32 len,
               u32 pic_id, u32 *read_bytes);
void Avs2Shutdown(struct Avs2Storage *storage);

const struct Avs2DpbOutPicture *Avs2NextOutputPicture(
    struct Avs2Storage *storage);

u32 Avs2PicWidth(struct Avs2Storage *storage);
u32 Avs2PicHeight(struct Avs2Storage *storage);
u32 Avs2VideoRange(struct Avs2Storage *storage);
u32 Avs2MatrixCoefficients(struct Avs2Storage *storage);
u32 Avs2IsMonoChrome(struct Avs2Storage *storage);
void Avs2CroppingParams(struct Avs2Storage *storage, u32 *cropping_flag,
                        u32 *left, u32 *width, u32 *top, u32 *height);

u32 Avs2CheckValidParamSets(struct Avs2Storage *storage);

void Avs2FlushBuffer(struct Avs2Storage *storage);

u32 Avs2AspectRatioIdc(const struct Avs2Storage *storage);
void Avs2GetSarSize(const struct Avs2Storage *storage, u32 *sar_width,
                    u32 *sar_height);

u32 Avs2SampleBitDepth(struct Avs2Storage *storage);
u32 Avs2OutputBitDepth(struct Avs2Storage *storage);
u32 Avs2GetMinDpbSize(struct Avs2Storage *storage);
void Avs2CroppingParams(struct Avs2Storage *storage, u32 *cropping_flag,
                        u32 *left_offset, u32 *width, u32 *top_offset,
                        u32 *height);

#endif /* #ifdef AVS2_DECODER_H */
