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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "md5_sink.h"
#include "md5.h"
#include "sw_util.h"  /* NEXT_MULTIPLE */

struct MD5Sink {
  FILE* file[DEC_MAX_OUT_COUNT];
  char filename[DEC_MAX_OUT_COUNT][FILENAME_MAX];
  struct MD5Context ctx[DEC_MAX_OUT_COUNT];
};

const void* Md5sinkOpen(const char** fname) {
  int i;
  struct MD5Sink* inst = calloc(1, sizeof(struct MD5Sink));
  if (inst == NULL) return NULL;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (fname[i] == NULL)
      continue;
    inst->file[i] = fopen(fname[i], "wb");
    if (inst->file[i] == NULL) {
      free(inst);
      return NULL;
    }
    strcpy(inst->filename[i], fname[i]);
    MD5Init(&inst->ctx[i]);
  }
  return inst;
}

void Md5sinkClose(const void* inst) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  int j;
  for (j = 0; j < DEC_MAX_OUT_COUNT; j++) {
    if (md5sink->file[j] != NULL) {
      u32 file_size;
      unsigned char digest[16];
      MD5Final(digest, &md5sink->ctx[j]);
      for (int i = 0; i < sizeof digest; i++) {
        fprintf(md5sink->file[j], "%02x", digest[i]);
      }
      fprintf(md5sink->file[j], "  %s\n", md5sink->filename[j]);
      fflush(md5sink->file[j]);
      /* Close the file and if it is empty, remove it. */
      fseek(md5sink->file[j], 0, SEEK_END);
      file_size = ftell(md5sink->file[j]);
      fclose(md5sink->file[j]);
      if (file_size == 0) remove(md5sink->filename[j]);
    }
  }
  free(md5sink);
}

void Md5sinkWritePic(const void* inst, struct DecPicture pic, int index) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
#ifdef TB_PP
  u32 w = pic.sequence_info.pic_width;
  u32 h = pic.sequence_info.pic_height;
  u8 pixel_bytes = ((pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ||
                    IS_PIC_8BIT(pic.picture_info.format)) ? 1 : 2;
  u32 num_of_pixels = w * h;
  if (IS_PIC_PACKED_RGB(pic.picture_info.format)) {
    if (IS_PIC_24BIT(pic.picture_info.format))
      pixel_bytes = 3;
    else if (IS_PIC_32BIT(pic.picture_info.format))
      pixel_bytes = 4;
    else if (IS_PIC_48BIT(pic.picture_info.format))
      pixel_bytes = 6;
    num_of_pixels = w * h;
    MD5Update(&md5sink->ctx[index], pic.luma.virtual_address, num_of_pixels * pixel_bytes); 
    return;
  }
  if (IS_PIC_PLANAR_RGB(pic.picture_info.format)) {
    num_of_pixels = w * h;
    u8 *p = (u8*)pic.luma.virtual_address;
    MD5Update(&md5sink->ctx[index], p, num_of_pixels * pixel_bytes);
    p += NEXT_MULTIPLE(num_of_pixels, 128);
    MD5Update(&md5sink->ctx[index], p, num_of_pixels * pixel_bytes);
    p += NEXT_MULTIPLE(num_of_pixels, 128);
    MD5Update(&md5sink->ctx[index], p, num_of_pixels * pixel_bytes); 
    return;
  }
  MD5Update(&md5sink->ctx[index], pic.luma.virtual_address, num_of_pixels * pixel_bytes);

  if (!pic.chroma.virtual_address) return;

  /* round odd picture dimensions to next multiple of two for chroma */
  if (w & 1) w += 1;
  if (h & 1) h += 1;
  num_of_pixels = w * h;
  MD5Update(&md5sink->ctx[index], pic.chroma.virtual_address, num_of_pixels / 2 * pixel_bytes);
#else
  u8 pixel_width = ((pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ||
                    IS_PIC_8BIT(pic.picture_info.format)) ? 8 :
                   (IS_PIC_16BIT(pic.picture_info.format)) ? 16 : 10;
  u8* p = (u8*)pic.luma.virtual_address;
  u32 s = pic.sequence_info.pic_stride;
  u32 w = pic.sequence_info.pic_width * pixel_width / 8;
  u32 h = pic.sequence_info.pic_height;
  u32 extra_bits = pic.sequence_info.pic_width * pixel_width & 7;
#if 0
  if (pic.picture_info.pixel_format == DEC_OUT_PIXEL_CUSTOMER1) {
    /* In customized format, fill 0 of padding bytes/bits in last 128-bit burst. */
    u32 padding_bytes, padding_bits, bursts, fill_bits;
    u8 *last_burst;
    bursts = pic.sequence_info.pic_width * pixel_width / 128;
    fill_bits = NEXT_MULTIPLE(pic.sequence_info.pic_width * pixel_width, 128)
                - pic.sequence_info.pic_width * pixel_width;
    padding_bytes = fill_bits / 8;
    padding_bits = fill_bits & 7;
    for (u32 i = 0; i < h; i++) {
      last_burst = p + bursts * 16;
      memset(last_burst, 0, padding_bytes);
      if (padding_bits) {
        u8 b = last_burst[padding_bytes];
        b &= ~((1 << padding_bits) - 1);
        last_burst[padding_bytes] = b;
      }
      p += s;
    }

    if (!pic.chroma.virtual_address) {   
      p = (u8*)pic.chroma.virtual_address;
      for (u32 i = 0; i < h / 2; i++) {
        last_burst = p + bursts * 16;
        memset(last_burst, 0, padding_bytes);
        if (padding_bits) {
          u8 b = last_burst[padding_bytes];
          b &= ~((1 << padding_bits) - 1);
          last_burst[padding_bytes] = b;
        }
        p += s;
      }
    }

    w = NEXT_MULTIPLE(pic.sequence_info.pic_width * pixel_width, 128) / 8;
    extra_bits = 0;
  }
#endif
  p = (u8*)pic.luma.virtual_address;
  for (u32 i = 0; i < h; i++) {
    MD5Update(&md5sink->ctx[index], p, w);
    if (extra_bits) {
      u8 last_byte = p[w];
      last_byte &= (1 << extra_bits) - 1;
      MD5Update(&md5sink->ctx[index], &last_byte, 1);
    }
    p += s;
  }

  if (!pic.chroma.virtual_address) return;

  /* round odd picture dimensions to next multiple of two for chroma */
  p = (u8*)pic.chroma.virtual_address;
  for (u32 i = 0; i < h / 2; i++) {
    MD5Update(&md5sink->ctx[index], p, w);
    if (extra_bits) {
      u8 last_byte = p[w];
      last_byte &= (1 << extra_bits) - 1;
      MD5Update(&md5sink->ctx[index], &last_byte, 1);
    }
    p += s;
  }
#endif
}

const void* md5perpicsink_open(const char** fname) {
  struct MD5Sink* inst = calloc(1, sizeof(struct MD5Sink));
  int i;
  if (inst == NULL) return NULL;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (fname[i] == NULL)
      continue;
    inst->file[i] = fopen(fname[i], "wb");
    if (inst->file[i] == NULL) {
      free(inst);
      return NULL;
    }
    strcpy(inst->filename[i], fname[i]);
  }
  return inst;
}

void md5perpicsink_close(const void* inst) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  int i;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (md5sink->file[i] != NULL) {
      /* Close the file and if it is empty, remove it. */
      u32 file_size;
      fseek(md5sink->file[i], 0, SEEK_END);
      file_size = ftell(md5sink->file[i]);
      fclose(md5sink->file[i]);
      if (file_size == 0) remove(md5sink->filename[i]);
    }
  }
  free(md5sink);
}

void md5perpicsink_write_pic(const void* inst, struct DecPicture pic, int index) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  struct MD5Context ctx;
#ifdef TB_PP
  u32 w = pic.sequence_info.pic_width;
  u32 h = pic.sequence_info.pic_height;
  u32 stride = pic.sequence_info.pic_stride;
  u32 num_of_pixels = w * h;
  unsigned char digest[16];
  u8 pixel_bytes = ((pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ||
                    IS_PIC_8BIT(pic.picture_info.format)) ? 1 : 2;
  u32 i = 0;
  u8* p = (u8*)pic.luma.virtual_address;

  MD5Init(&ctx);
  if (IS_PIC_PACKED_RGB(pic.picture_info.format)) {
    num_of_pixels = 3 * w;
    for (i = 0; i < h; i++) {
      MD5Update(&ctx, p, num_of_pixels * pixel_bytes);
      p += stride;
    }
    MD5Final(digest, &ctx);
    for (int i = 0; i < sizeof(digest); i++) {
      fprintf(md5sink->file[index], "%02X", digest[i]);
    }
    fprintf(md5sink->file[index], "\n");
    fflush(md5sink->file[index]);
    return;
  }
  if (IS_PIC_PLANAR_RGB(pic.picture_info.format)) {
    num_of_pixels = w;
    for (i = 0; i < pic.sequence_info.pic_height; i++) {
      MD5Update(&ctx, p, num_of_pixels * pixel_bytes);
      p += stride;
      MD5Update(&ctx, p, num_of_pixels * pixel_bytes);
      p += stride;
      MD5Update(&ctx, p, num_of_pixels * pixel_bytes);
      p += stride;
    }

    MD5Final(digest, &ctx);
    for (int i = 0; i < sizeof(digest); i++) {
      fprintf(md5sink->file[index], "%02X", digest[i]);
    }
    fprintf(md5sink->file[index], "\n");
    fflush(md5sink->file[index]);
    return;
  }
  MD5Update(&ctx, pic.luma.virtual_address, num_of_pixels * pixel_bytes);

  if (pic.chroma.virtual_address) {
    /* round odd picture dimensions to next multiple of two for chroma */
    if (w & 1) w += 1;
    if (h & 1) h += 1;
    num_of_pixels = w * h;
    MD5Update(&ctx, pic.chroma.virtual_address, num_of_pixels / 2 * pixel_bytes);
  }
  MD5Final(digest, &ctx);
#else
  u8 pixel_width = ((pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ||
                    IS_PIC_8BIT(pic.picture_info.format)) ? 8 :
                   (IS_PIC_16BIT(pic.picture_info.format)) ? 16 : 10;
  u8* p = (u8*)pic.luma.virtual_address;
  u32 s = pic.sequence_info.pic_stride;
  u32 w = pic.sequence_info.pic_width * pixel_width / 8;
  u32 h = pic.sequence_info.pic_height;
  u32 extra_bits = pic.sequence_info.pic_width * pixel_width & 7;
  unsigned char digest[16];
#if 0
  if (pic.picture_info.pixel_format == DEC_OUT_PIXEL_CUSTOMER1) {
    /* In customized format, fill 0 of padding bytes/bits in last 128-bit burst. */
    u32 padding_bytes, padding_bits, bursts, fill_bits;
    u8 *last_burst;
    bursts = pic.sequence_info.pic_width * pixel_width / 128;
    fill_bits = NEXT_MULTIPLE(pic.sequence_info.pic_width * pixel_width, 128)
                - pic.sequence_info.pic_width * pixel_width;
    padding_bytes = fill_bits / 8;
    padding_bits = fill_bits & 7;
    for (u32 i = 0; i < h; i++) {
      last_burst = p + bursts * 16;
      memset(last_burst, 0, padding_bytes);
      if (padding_bits) {
        u8 b = last_burst[padding_bytes];
        b &= ~((1 << padding_bits) - 1);
        last_burst[padding_bytes] = b;
      }
      p += s;
    }
    if (pic.chroma.virtual_address) {
      p = (u8*)pic.chroma.virtual_address;
      for (u32 i = 0; i < h / 2; i++) {
        last_burst = p + bursts * 16;
        memset(last_burst, 0, padding_bytes);
        if (padding_bits) {
          u8 b = last_burst[padding_bytes];
          b &= ~((1 << padding_bits) - 1);
          last_burst[padding_bytes] = b;
        }
        p += s;
      }
    }

    w = NEXT_MULTIPLE(pic.sequence_info.pic_width * pixel_width, 128) / 8;
    extra_bits = 0;
  }
#endif
  MD5Init(&ctx);

  p = (u8*)pic.luma.virtual_address;
  for (u32 i = 0; i < h; i++) {
    MD5Update(&ctx, p, w);
    if (extra_bits) {
      u8 last_byte = p[w];
      last_byte &= (1 << extra_bits) - 1;
      MD5Update(&ctx, &last_byte, 1);
    }
    p += s;
  }
  /* round odd picture dimensions to next multiple of two for chroma */
  if (pic.chroma.virtual_address) {
    p = (u8*)pic.chroma.virtual_address;
    for (u32 i = 0; i < h / 2; i++) {
      MD5Update(&ctx, p, w);
      if (extra_bits) {
        u8 last_byte = p[w];
        last_byte &= (1 << extra_bits) - 1;
        MD5Update(&ctx, &last_byte, 1);
      }
      p += s;
    }
  }
  MD5Final(digest, &ctx);
#endif
  for (int i = 0; i < sizeof(digest); i++) {
    fprintf(md5sink->file[index], "%02X", digest[i]);
  }
  fprintf(md5sink->file[index], "\n");
  fflush(md5sink->file[index]);
}
