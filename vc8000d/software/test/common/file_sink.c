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

#include "sw_util.h"  /* NEXT_MULTIPLE */
#include "decapicommon.h"
#include "file_sink.h"

struct FileSink {
  u8* frame_pic;
  FILE* file[2*DEC_MAX_OUT_COUNT];
  char filename[2*DEC_MAX_OUT_COUNT][FILENAME_MAX];
};

const void* FilesinkOpen(const char** fname) {
  int i;
  struct FileSink* inst = calloc(1, sizeof(struct FileSink));
  if (inst == NULL) return NULL;
#ifdef SUPPORT_DEC400
  for (i = 0; i < 2*DEC_MAX_OUT_COUNT; i++) {
#else
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
#endif
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

void FilesinkClose(const void* inst) {
  struct FileSink* output = (struct FileSink*)inst;
  int i;
#ifdef SUPPORT_DEC400
  for (i = 0; i < 2*DEC_MAX_OUT_COUNT; i++) {
#else
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
#endif
    if (output->file[i] != NULL) {
      /* Close the file and if it is empty, remove it. */
      off_t file_size;
      fseeko(output->file[i], 0, SEEK_END);
      file_size = ftello(output->file[i]);
      fclose(output->file[i]);
      if (file_size == 0) {
        remove(output->filename[i]);
      }
    }
  }
  if (output->frame_pic != NULL) {
    free(output->frame_pic);
  }
  free(output);
}

void FilesinkWritePic(const void* inst, struct DecPicture pic, int index) {
  struct FileSink* output = (struct FileSink*)inst;
#ifndef SUPPORT_DEC400
  u32 in_bitdepth = (pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ? 8 : 10;
#ifdef TB_PP
  u32 w, h, num_of_pixels;
  u32 i;
  u8 pixel_bytes = ((pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ||
                    (IS_PIC_8BIT(pic.picture_info.format)) ||
                    ((pic.sequence_info.bit_depth_luma>>8) == 8 )) ? 1 : 2;
  u8* p = (u8*)pic.luma.virtual_address;
  if (IS_PIC_PACKED_RGB(pic.picture_info.format)) {
    if (IS_PIC_24BIT(pic.picture_info.format))
      in_bitdepth = 24;
    else if (IS_PIC_32BIT(pic.picture_info.format))
      in_bitdepth = 32;
    else if (IS_PIC_48BIT(pic.picture_info.format))
      in_bitdepth = 48;
    for (i = 0; i < pic.sequence_info.pic_height; i++) {
      num_of_pixels = in_bitdepth * pic.sequence_info.pic_width / 8;
      fwrite(p, 1, num_of_pixels, output->file[index]);
      p += pic.sequence_info.pic_stride;
    }
    return;
  }
  if (IS_PIC_PLANAR_RGB(pic.picture_info.format)) {
    if (IS_PIC_8BIT(pic.picture_info.format))
      in_bitdepth = 8;
    else if (IS_PIC_16BIT(pic.picture_info.format))
      in_bitdepth = 16;
    for (i = 0; i < pic.sequence_info.pic_height; i++) {
      fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, output->file[index]);
      p += pic.sequence_info.pic_stride;
      fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, output->file[index]);
      p += pic.sequence_info.pic_stride;
      fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, output->file[index]);
      p += pic.sequence_info.pic_stride;
    }
    fflush(output->file[index]);
    return;
  }
  w = pic.sequence_info.pic_width;
  h = pic.sequence_info.pic_height;
  num_of_pixels = w * h;
  fwrite(p, pixel_bytes, num_of_pixels, output->file[index]);
  p += num_of_pixels;

  if (!pic.chroma.virtual_address) return;

  /* round odd picture dimensions to next multiple of two for chroma */
  if (w & 1) w += 1;
  if (h & 1) h += 1;
  num_of_pixels = w * h;
  fwrite(pic.chroma.virtual_address, pixel_bytes, num_of_pixels / 2, output->file[index]);
#else
  u32 s = pic.sequence_info.pic_stride;
  u32 h = pic.sequence_info.pic_height;
  u32 num_of_pixels = s * h;
  fwrite((u8*)pic.luma.virtual_address, 1, num_of_pixels, output->file[index]);

  if (!pic.chroma.virtual_address) return;

  /* round odd picture dimensions to next multiple of two for chroma */
  s = pic.sequence_info.pic_stride_ch;
  if (h & 1) h += 1;
  num_of_pixels = s * h;
  if (IS_PIC_PLANAR(pic.picture_info.format))
    fwrite((u8*)pic.chroma.virtual_address, 1, num_of_pixels, output->file[index]);
  else
    fwrite((u8*)pic.chroma.virtual_address, 1, num_of_pixels / 2, output->file[index]);
#endif
#else
  u32 s = pic.sequence_info.pic_stride;
  u32 h = pic.sequence_info.pic_height;
  u32 num_of_pixels = s * h;
  fwrite((u8*)pic.luma.virtual_address, 1, num_of_pixels, output->file[index]);
  if(pic.pic_compressed_status == 2)
    fwrite((u8*)pic.luma_table.virtual_address, 1,pic.luma_table.size, output->file[index+DEC_MAX_OUT_COUNT]);

  if (!pic.chroma.virtual_address) return;

  /* round odd picture dimensions to next multiple of two for chroma */
  s = pic.sequence_info.pic_stride_ch;
  if (h & 1) h += 1;
  num_of_pixels = s * h;
  if (IS_PIC_PLANAR(pic.picture_info.format))
    fwrite((u8*)pic.chroma.virtual_address, 1, num_of_pixels, output->file[index]);
  else
    fwrite((u8*)pic.chroma.virtual_address, 1, num_of_pixels / 2, output->file[index]);
  if(pic.pic_compressed_status == 2)/*dec400 table*/ {
    fwrite((u8*)pic.chroma_table.virtual_address, 1,pic.chroma_table.size, output->file[index+DEC_MAX_OUT_COUNT]);
  }
#endif
}

void FilesinkWriteSinglePic(const void* inst, struct DecPicture pic, int index) {
  static int frame_num = 0;
  char name[FILENAME_MAX];
  u32 in_bitdepth = (pic.sequence_info.bit_depth_luma == 8 && pic.sequence_info.bit_depth_chroma == 8) ? 8 : 10;
  FILE* fp = NULL;

  memset(name, 0, sizeof(name));
  frame_num++;
  sprintf(name, "out_%03d_%dx%d.yuv", frame_num, pic.sequence_info.pic_width,
          pic.sequence_info.pic_height);

  fp = fopen(name, "wb");
  if (fp) {
    u32 w = pic.sequence_info.pic_width;
    u32 h = pic.sequence_info.pic_height;
    u32 num_of_pixels = w * h;
    u32 i;
    u8* p = (u8*)pic.luma.virtual_address;

    if (IS_PIC_PACKED_RGB(pic.picture_info.format)) {
      if (IS_PIC_24BIT(pic.picture_info.format))
        in_bitdepth = 24;
      else if (IS_PIC_32BIT(pic.picture_info.format))
        in_bitdepth = 32;
      else if (IS_PIC_48BIT(pic.picture_info.format))
        in_bitdepth = 48;
      for (i = 0; i < pic.sequence_info.pic_height; i++) {
        num_of_pixels = in_bitdepth * pic.sequence_info.pic_width / 8;
        fwrite(p, 1, num_of_pixels, fp);
        p += pic.sequence_info.pic_stride;
      }
      fclose(fp);
      return;
    }
    if (IS_PIC_PLANAR_RGB(pic.picture_info.format)) {
      if (IS_PIC_8BIT(pic.picture_info.format))
        in_bitdepth = 8;
      else if (IS_PIC_16BIT(pic.picture_info.format))
        in_bitdepth = 16;
      for (i = 0; i < pic.sequence_info.pic_height; i++) {
        fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, fp);
        p += pic.sequence_info.pic_stride;
        fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, fp);
        p += pic.sequence_info.pic_stride;
        fwrite(p, 1, pic.sequence_info.pic_width * in_bitdepth / 8, fp);
        p += pic.sequence_info.pic_stride;
      }
      fclose(fp);
      return;
    }


    fwrite(pic.luma.virtual_address, 1, num_of_pixels, fp);

    if (!pic.chroma.virtual_address) return;

    /* round odd picture dimensions to next multiple of two for chroma */
    if (w & 1) w += 1;
    if (h & 1) h += 1;
    num_of_pixels = w * h;
    fwrite(pic.chroma.virtual_address, 1, num_of_pixels / 2, fp);
    fclose(fp);
  }
}
