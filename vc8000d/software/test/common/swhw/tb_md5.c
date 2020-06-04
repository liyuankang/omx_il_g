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

#include "tb_md5.h"
#include "md5.h"
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
/* Calculates MD5 check sum for the provided frame and writes the result to
 * provided file. */
u32 TBWriteFrameMD5Sum(FILE* f_out, u8* yuv, u32 yuv_size, u32 frame_number) {
  unsigned char digest[16];
  struct MD5Context ctx;
  int i = 0;
  MD5Init(&ctx);
  MD5Update(&ctx, yuv, yuv_size);
  MD5Final(digest, &ctx);
  /*    fprintf(f_out, "FRAME %d: ", frame_number);*/
  for (i = 0; i < sizeof digest; i++) {
    fprintf(f_out, "%02X", digest[i]);
  }
  fprintf(f_out, "\n");
  fflush(f_out);
  return 0;
}
#if 1
u32 TBWriteFrameMD5SumValidOnly2(FILE* f_out, u8* p_lu, u8 *p_ch,
                                            u32 coded_width, u32 coded_height,
                                            u32 pic_stride, u32 pic_stride_ch, u32 pic_height,
                                            u32 planar, u32 frame_number) {
  unsigned char digest[16];
  struct MD5Context ctx;
  int i = 0;
  MD5Init(&ctx);
  u8 *p_yuv = p_lu;
  if (p_yuv) {
    for (i = 0; i < coded_height; i++) {
      MD5Update(&ctx, p_yuv, coded_width);
      p_yuv += pic_stride;
    }
  }
  p_yuv = p_ch;
  if (p_yuv) {
    if (!planar) {
      for (i = 0; i < coded_height / 2; i++) {
        MD5Update(&ctx, p_yuv, coded_width);
        p_yuv += pic_stride;
      }
    } else {
      for (i = 0; i < coded_height / 2; i++) {
        MD5Update(&ctx, p_yuv, coded_width/2);
        p_yuv += pic_stride_ch;
      }
      for (i = 0; i < coded_height / 2; i++) {
        MD5Update(&ctx, p_yuv, coded_width/2);
        p_yuv += pic_stride_ch;
      }
    }
  }
  MD5Final(digest, &ctx);
  /*    fprintf(f_out, "FRAME %d: ", frame_number);*/
  for (i = 0; i < sizeof digest; i++) {
    fprintf(f_out,"%02X", digest[i]);
  }
  fprintf(f_out,"\n");
  fflush(f_out);
  return 0;
}
#endif
u32 TBWriteFrameMD5SumValidOnly(FILE* f_out, u8* p_lu, u8 *p_ch,
                                            u32 coded_width, u32 coded_height,
                                            u32 coded_width_ch, u32 coded_height_ch,
                                            u32 pic_stride, u32 pic_stride_ch,
                                            u32 planar, u32 frame_number) {
  unsigned char digest[16];
  struct MD5Context ctx;
  int i = 0;
  MD5Init(&ctx);
  u8 *p_yuv = p_lu;
  if (p_yuv) {
    for (i = 0; i < coded_height; i++) {
      MD5Update(&ctx, p_yuv, coded_width);
      p_yuv += pic_stride;
    }
  }
  p_yuv = p_ch;
  if (p_yuv) {
    if (!planar) {
      for (i = 0; i < coded_height_ch; i++) {
        MD5Update(&ctx, p_yuv, coded_width_ch);
        p_yuv += pic_stride;
      }
    } else {
      for (i = 0; i < coded_height_ch; i++) {
        MD5Update(&ctx, p_yuv, coded_width_ch/2);
        p_yuv += pic_stride_ch;
      }
      for (i = 0; i < coded_height_ch; i++) {
        MD5Update(&ctx, p_yuv, coded_width_ch/2);
        p_yuv += pic_stride_ch;
      }
    }
  }
  MD5Final(digest, &ctx);
  /*    fprintf(f_out, "FRAME %d: ", frame_number);*/
  for (i = 0; i < sizeof digest; i++) {
    fprintf(f_out,"%02X", digest[i]);
  }
  fprintf(f_out,"\n");
  fflush(f_out);
  return 0;
}

u32 TBWriteFrameMD5SumValidOnlyRGB(FILE* fout, u8* data, u32 coded_width, u32 coded_height,
                                   u32 pic_stride, u32 planar, u32 frame_number, u32 pixel_bytes) {
  unsigned char digest[16];
  struct MD5Context ctx;
  int i = 0;
  u32 num_of_pixels = 0;
  MD5Init(&ctx);
  if (!planar) {
    for (i = 0; i < coded_height; i++) {
      num_of_pixels = 3 * coded_width;
      MD5Update(&ctx, data, num_of_pixels * pixel_bytes);
      data += pic_stride;
    }
    MD5Final(digest,&ctx);
    for(i = 0; i< sizeof(digest); i++)
      fprintf(fout, "%02X",digest[i]);
    fprintf(fout, "\n");
    fflush(fout);
    return 0;
  } else {
    for (i = 0; i < coded_height; i++) {
      MD5Update(&ctx, data, coded_width * pixel_bytes);
      data += pic_stride;
      MD5Update(&ctx, data, coded_width * pixel_bytes);
      data += pic_stride;
      MD5Update(&ctx, data, coded_width * pixel_bytes);
      data += pic_stride;
    }
    MD5Final(digest,&ctx);
    for(i = 0; i< sizeof(digest); i++)
      fprintf(fout, "%02X",digest[i]);
    fprintf(fout, "\n");
    fflush(fout);
    return 0;
  }
}
