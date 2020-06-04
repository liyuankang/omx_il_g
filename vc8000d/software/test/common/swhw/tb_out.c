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
#include "tb_out.h"
#include "tb_tiled.h"

/* Debug prints */
#undef DEBUG_PRINT
#if 0
#define DEBUG_PRINT(argv)
#else
#define DEBUG_PRINT(argv) printf argv
#endif
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
enum DecDpbMode {
  DEC_DPB_FRAME = 0,
  DEC_DPB_INTERLACED_FIELD = 1
};

static u8 *grey_chroma = NULL;

u32 TBWriteOutput(FILE* fout,u8* p_lu, u8 *p_ch,
                  u32 coded_width, u32 coded_height, u32 pic_stride,
                  u32 coded_width_ch, u32 coded_h_ch, u32 pic_stride_ch,
                  u32 md5Sum, u32 planar, u32 mono_chrome, u32 frame_number,
                  u32 pixel_bytes) {
#ifndef SUPPORT_DEC400
  u32 i;
#else
  u32 h = coded_height;
  u32 num_of_pixels_uv;
#endif
  if (md5Sum)
    TBWriteFrameMD5SumValidOnly(fout, p_lu, p_ch, coded_width, coded_height,
                                coded_width_ch, coded_h_ch,
                                pic_stride, pic_stride_ch,
                                planar, frame_number);
  else {
#ifndef SUPPORT_DEC400
    for (i = 0; i < coded_height; i++) {
      fwrite(p_lu, pixel_bytes, coded_width, fout);
      p_lu += pic_stride;
    }

    if (!mono_chrome) {
      if (planar) {
        for (i = 0; i < coded_h_ch; i++) {
          fwrite(p_ch, pixel_bytes, coded_width_ch/2, fout);
          p_ch += pic_stride_ch;
        }
        for (i = 0; i < coded_h_ch; i++) {
          fwrite(p_ch, pixel_bytes, coded_width_ch/2, fout);
          p_ch += pic_stride_ch;
        }
      } else {
        for (i = 0; i < coded_h_ch; i++) {
          fwrite(p_ch, pixel_bytes, coded_width_ch, fout);
          p_ch += pic_stride_ch;
        }
      }
    }
#else
    fwrite(p_lu, 1, coded_height*pic_stride, fout);
    if (h & 1) h += 1;
    num_of_pixels_uv = pic_stride_ch * h / 2;
    if (p_ch) {
      fwrite(p_ch, 1, num_of_pixels_uv, fout);
      if (planar)
        fwrite(p_ch+num_of_pixels_uv, 1, num_of_pixels_uv, fout);
    }
#endif
  }
  return 0;
}

u32 TBWriteOutputRGB(FILE* fout,u8* data,
                     u32 coded_width, u32 coded_height, u32 pic_stride,
                     u32 md5Sum, u32 planar, u32 frame_number,
                     u32 pixel_bytes) {
  u32 num_of_pixels = 0;
  u32 i;

  if (md5Sum)
    TBWriteFrameMD5SumValidOnlyRGB(fout, data, coded_width, coded_height,
                                   pic_stride, planar, frame_number, pixel_bytes);
  else {
    if (!planar) {
      for (i = 0; i < coded_height; i++) {
        num_of_pixels = 3 * coded_width;
        fwrite(data, pixel_bytes, num_of_pixels, fout);
        data += pic_stride;
      }
    } else {
      for (i = 0; i < coded_height; i++) {
        fwrite(data, pixel_bytes, coded_width, fout);
        data += pic_stride;
        fwrite(data, pixel_bytes, coded_width, fout);
        data += pic_stride;
        fwrite(data, pixel_bytes, coded_width, fout);
        data += pic_stride;
      }
    }
  }
  return 0;
}
/*------------------------------------------------------------------------------

 Function name:  WriteOutput

  Purpose:
  Write picture pointed by data to file. Size of the
  picture in pixels is indicated by pic_size.

  When pixel_bytes = 2(for 16 bits pixel), do not support data conversion,
  e.g., TBTiledToRaster, TBTiledToRaster, TBFieldDpbToFrameDpb

------------------------------------------------------------------------------*/
void WriteOutput(char *filename, u8 * data,
                 u32 pic_width, u32 pic_height, u32 frame_number, u32 mono_chrome,
                 u32 view, u32 mvc_separate_views, u32 disable_output_writing,
                 u32 tiled_mode, u32 pic_stride, u32 pic_stride_ch, u32 index,
                 u32 planar, u32 cr_first, u32 convert_tiled_output,
                 u32 convert_to_frame_dpb, u32 dpb_mode, u32 md5sum, FILE **fout,
                 u32 pixel_bytes) {
  u32 e_height_luma = pic_height;
  char* fn;
  u8 *raster_scan = NULL;
  u8 *out_data = NULL;
  char alt_file_name[256];

  u32 pp_ch_w = pic_width;
  u32 pp_ch_h = pic_height / 2;
  u32 out_w = pic_width;
  u32 out_h = pic_height;

  if(disable_output_writing) {
    return;
  }

  if (tiled_mode) {
    /* For case of partial tiles ... */
    pp_ch_w = (pic_width + 3) & ~0x3;
    pp_ch_h = (pic_height/2 + 3) & ~0x3;
    pic_width = (pic_width + 3) & ~0x3;
    pic_height = (pic_height + 3) & ~0x3;
  }

  /* foutput is global file pointer */
  if(*fout == NULL) {
    if(filename[0] == 0) {
      if (planar) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, cr_first ? "yv12" : "iyuv", index);
      } else if (tiled_mode && !convert_tiled_output) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                pic_width, pic_height, "tiled4x4", index);
      } else if (cr_first) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "nv21", index);
      } else {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "nv12", index);
      }
    } else {
      strcpy(alt_file_name, filename);
      if (strlen(alt_file_name) >= 4)
        sprintf(alt_file_name + strlen(alt_file_name)-4, "_%d.yuv", index);
    }

    if (view && mvc_separate_views) {
      sprintf(alt_file_name+strlen(alt_file_name)-4, "_%d.yuv", view);
      fn = alt_file_name;
    } else {
      fn = alt_file_name;
    }

    /* open output file for writing, can be disabled with define.
     * If file open fails -> exit */
    if(strcmp(filename, "none") != 0) {
      *fout = fopen(fn, "wb");
      if(*fout == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN OUTPUT FILE\n"));
        return;
      }
    }
  memcpy(filename, alt_file_name, 256);
  }

  if (tiled_mode) {
    if (convert_tiled_output) {
      /* Tile -> raster */
      u32 eff_height = out_h;
      raster_scan = (u8*)malloc(pic_width*eff_height + pp_ch_w*pp_ch_h);
      if(!raster_scan) {
        fprintf(stderr, "error allocating memory for tiled"
                    "-->raster conversion!\n");
        if(out_data)
          free(out_data);
        return;
      }

      TBTiledToRaster( tiled_mode, convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                       data, raster_scan, pic_width,
                       eff_height, pic_stride);
      if(!mono_chrome)
        TBTiledToRaster( tiled_mode, convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                         data+pic_stride*pic_height/4,
                         raster_scan+pic_width*eff_height, pic_width, pp_ch_h,
                         pic_stride_ch );

      data = raster_scan;
      pic_stride = pic_width;
      pic_stride_ch = pic_width;
      pp_ch_w = out_w;
      pp_ch_h = out_h/2;
    } else {
      /* Tile output directly */
      e_height_luma = pic_height;
      pic_width = pic_width * 4;
      pic_height = pic_height / 4;
      pp_ch_w *= 4;
      pp_ch_h /= 4;
      e_height_luma = e_height_luma / 4;
      out_w = pic_width;
      out_h = pic_height;
    }
  } else if (convert_to_frame_dpb && (dpb_mode != DEC_DPB_FRAME)) {
    u32 eff_height = pic_height;
    raster_scan = (u8*)malloc(pic_width*eff_height*3/2);
    if(!raster_scan) {
      fprintf(stderr, "error allocating memory for tiled"
                  "-->raster conversion!\n");
      if(out_data)
        free(out_data);
      return;
    }

    TBFieldDpbToFrameDpb( convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                          data, raster_scan, mono_chrome, pic_width, eff_height );

    data = raster_scan;
    pp_ch_w = out_w;
    pp_ch_h = out_h;
  }

  if (mono_chrome) {
    grey_chroma = NULL;
  }

  TBWriteOutput(*fout, data,
                mono_chrome ? grey_chroma : data + pic_stride * e_height_luma,
                out_w, out_h, pic_stride,
                pp_ch_w, pp_ch_h,
                pic_stride_ch,
                md5sum, planar,
                mono_chrome, frame_number, pixel_bytes);

  if(raster_scan)
    free(raster_scan);
  if(out_data)
    free(out_data);
  return;
}

void WriteOutputRGB(char *filename, u8 * data,
         u32 pic_width, u32 pic_height, u32 frame_number, u32 mono_chrome,
         u32 view, u32 mvc_separate_views, u32 disable_output_writing,
         u32 tiled_mode, u32 pic_stride, u32 pic_stride_ch, u32 index,
         u32 planar, u32 cr_first, u32 convert_tiled_output,
         u32 convert_to_frame_dpb, u32 dpb_mode, u32 md5sum, FILE **fout,
         u32 pixel_bytes) {
  char* fn;
  char alt_file_name[256];

  u32 out_w = pic_width;
  u32 out_h = pic_height;

  if(disable_output_writing) {
    return;
  }
  /* foutput is global file pointer */
  if(*fout == NULL) {
    if(filename[0] == 0) {
      if (planar) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "rgb", index);
      } else {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "rgb", index);
      }
    } else {
      strcpy(alt_file_name, filename);
      if (strlen(alt_file_name) >= 4)
        sprintf(alt_file_name + strlen(alt_file_name)-4, "_%d.yuv", index);
    }
    fn = alt_file_name;
    /* open output file for writing, can be disabled with define.
     *      * If file open fails -> exit */
    if(strcmp(filename, "none") != 0) {
      *fout = fopen(fn, "wb");
      if(*fout == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN OUTPUT FILE\n"));
        return;
      }
    }
    memcpy(filename, alt_file_name, 256);
  }
  TBWriteOutputRGB(*fout, data, pic_width, pic_height, pic_stride,
                   md5sum, planar, frame_number, pixel_bytes);
  return;
} 

/*------------------------------------------------------------------------------

 Function name:  WriteOutput

  Purpose:
  Write picture pointed by data to file. Size of the
  picture in pixels is indicated by pic_size.

  When pixel_bytes = 2(for 16 bits pixel), do not support data conversion,
  e.g., TBTiledToRaster, TBTiledToRaster, TBFieldDpbToFrameDpb

------------------------------------------------------------------------------*/
void WriteOutputdec400(char *filename, u8 * data,
                       u32 pic_width, u32 pic_height, u32 frame_number, u32 mono_chrome,
                       u32 view, u32 mvc_separate_views, u32 disable_output_writing,
                       u32 tiled_mode, u32 pic_stride, u32 pic_stride_ch, u32 index,
                       u32 planar, u32 cr_first, u32 convert_tiled_output,
                       u32 convert_to_frame_dpb, u32 dpb_mode, u32 md5sum, FILE **fout,
                       u32 pixel_bytes, FILE **fout_table, u8 * y_table_data, u32 y_table_size,u8 * uv_table_data, u32 uv_table_size) {
  u32 e_height_luma = pic_height;
  char* fn;
  char *fn_table;
  u8 *raster_scan = NULL;
  u8 *out_data = NULL;
  char alt_file_name[256];
  char alt_file_name_table[256];

  u32 pp_ch_w = pic_width;
  u32 pp_ch_h = pic_height / 2;
  u32 out_w = pic_width;
  u32 out_h = pic_height;

  if(disable_output_writing) {
    return;
  }
  if (tiled_mode) {
    /* For case of partial tiles ... */
    pp_ch_w = (pic_width + 3) & ~0x3;
    pp_ch_h = (pic_height/2 + 3) & ~0x3;
    pic_width = (pic_width + 3) & ~0x3;
    pic_height = (pic_height + 3) & ~0x3;
  }

  /* foutput is global file pointer */
  if(*fout == NULL) {
    if(filename[0] == 0) {
      if (planar) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "yv12", index);
      } else if (tiled_mode && !convert_tiled_output) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                pic_width, pic_height, "tiled4x4", index);
      } else if (cr_first) {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "nv21", index);
      } else {
        sprintf(alt_file_name, "out_%dx%d_%s_%d.yuv",
                out_w, out_h, "nv12", index);
      }
    } else {
      strcpy(alt_file_name, filename);
      if (strlen(alt_file_name) >= 4)
        sprintf(alt_file_name + strlen(alt_file_name)-4, "_pp_%d.yuv", index);
    }

    if (view && mvc_separate_views) {
      sprintf(alt_file_name+strlen(alt_file_name)-4, "_%d.yuv", view);
      fn = alt_file_name;
    } else {
      fn = alt_file_name;
    }
    strcpy(alt_file_name_table, alt_file_name);
    sprintf(alt_file_name_table + strlen(alt_file_name_table)-4, "_dec400_table.bin");
    fn_table= alt_file_name_table;
    /* open output file for writing, can be disabled with define.
     * If file open fails -> exit */
    if(strcmp(filename, "none") != 0) {
      *fout = fopen(fn, "wb");
      if(*fout == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN OUTPUT FILE\n"));
        return;
      }
      *fout_table= fopen(fn_table, "wb");
      if(*fout_table == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN OUTPUT TABLE FILE\n"));
        return;
      }
    }
  memcpy(filename, alt_file_name, 256);
  }
  if(y_table_data != NULL)
    fwrite(y_table_data, 1,y_table_size , *fout_table);
  if(uv_table_data != NULL)
    fwrite(uv_table_data, 1,uv_table_size , *fout_table);
  if (tiled_mode) {
    if (convert_tiled_output) {
      /* Tile -> raster */
      u32 eff_height = out_h;
      raster_scan = (u8*)malloc(pic_width*eff_height + pp_ch_w*pp_ch_h);
      if(!raster_scan) {
        fprintf(stderr, "error allocating memory for tiled"
                    "-->raster conversion!\n");
        if(out_data)
          free(out_data);
        return;
      }
      TBTiledToRaster( tiled_mode, convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                       data, raster_scan, pic_width,
                       eff_height, pic_stride);
      if(!mono_chrome)
        TBTiledToRaster( tiled_mode, convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                         data+pic_stride*pic_height/4,
                         raster_scan+pic_width*eff_height, pic_width, pp_ch_h,
                         pic_stride_ch );

      data = raster_scan;
      pic_stride = pic_width;
      pic_stride_ch = pic_width;
      pp_ch_w = out_w;
      pp_ch_h = out_h/2;
    } else {
      /* Tile output directly */
      e_height_luma = pic_height;
      pic_width = pic_width * 4;
      pic_height = pic_height / 4;
      pp_ch_w *= 4;
      pp_ch_h /= 4;
      e_height_luma = e_height_luma / 4;
      out_w = pic_width;
      out_h = pic_height;
    }
  } else if (convert_to_frame_dpb && (dpb_mode != DEC_DPB_FRAME)) {
    u32 eff_height = pic_height;
    raster_scan = (u8*)malloc(pic_width*eff_height*3/2);
    if(!raster_scan) {
      fprintf(stderr, "error allocating memory for tiled"
                  "-->raster conversion!\n");
      if(out_data)
        free(out_data);
      return;
    }
    TBFieldDpbToFrameDpb( convert_to_frame_dpb ? dpb_mode : DEC_DPB_FRAME,
                          data, raster_scan, mono_chrome, pic_width, eff_height );

    data = raster_scan;
    pp_ch_w = out_w;
    pp_ch_h = out_h;
  }

  if (mono_chrome) {
    grey_chroma = NULL;
  }
  TBWriteOutput(*fout, data,
                mono_chrome ? grey_chroma : data + pic_stride * e_height_luma,
                out_w, out_h, pic_stride,
                pp_ch_w, pp_ch_h,
                pic_stride_ch,
                md5sum, planar,
                mono_chrome, frame_number, pixel_bytes);

  if(raster_scan)
    free(raster_scan);
  if(out_data)
    free(out_data);
  return;
}
