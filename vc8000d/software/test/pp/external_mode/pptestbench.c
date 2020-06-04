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

#include "ppapi.h"
#include "ppinternal.h"
#include "dwl.h"
#include "dwlthread.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "tb_cfg.h"
#include "tb_tiled.h"
#include "tb_out.h"
#include "basetype.h"
#include "regdrv.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif
#include "tb_md5.h"
#include "trace_hooks.h"
#include "command_line_parser.h"

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 128 bytes alignment */

extern u32 dec_pp_in_blk_size;
struct TBCfg tb_cfg;

FILE *finput;

u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;

#ifdef ASIC_TRACE_SUPPORT
extern u32 test_case_id;
#endif

u32 md5sum = 0;
u32 pixel_width = 8;
u32 pixel_width_pp = 8;
u32 pp_enabled = 0;
u32 rgb_enabled = 0;
/* user input arguments */
u32 scale_enabled = 0;
u32 scaled_w, scaled_h;
u32 crop_enabled = 0;
u32 crop_x = 0;
u32 crop_y = 0;
u32 crop_w = 0;
u32 crop_h = 0;
enum SCALE_MODE scale_mode;
u32 pp_tile_out = 0;    /* PP tiled output */
const char* in_file_name;
PPInst pp_inst = NULL;
char out_file_name[DEC_MAX_PPU_COUNT][256] = {"", "", "", ""};
FILE *foutput[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL};
#define DEBUG_PRINT(str) printf str
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define ALIGN(a) (1 << (a))
/*------------------------------------------------------------------------------

    Function name:  PrintUsage()

    Purpose:
        Print C test bench inline help -- command line params
------------------------------------------------------------------------------*/
void PrintUsagePp(char *s)
{
    DEBUG_PRINT(("Usage: %s [options] file.yuv\n", s));
    DEBUG_PRINT(("\t--help(-h) Print command line parameters help info\n"));
    DEBUG_PRINT(("\t-Nn forces decoding to stop after n pictures\n"));
    DEBUG_PRINT(("\t-O<file> write output to <file> (default out_wxxxhyyy.yuv)\n"));
    DEBUG_PRINT(("\t-m Output md5 for each picture. No YUV output.(--md5-per-pic)\n"));
    DEBUG_PRINT(("\t-Sn set input stream stride to n bytes\n"));
    DEBUG_PRINT(("\t-Wn set input stream width to n pixels (MUST)\n"));
    DEBUG_PRINT(("\t-Hn set input stream height to n pixels (MUST)\n"));
    DEBUG_PRINT(("\t--inp010 input YUV is p010 format (10-bit pixel in MSB of 16 bits)\n"));
    DEBUG_PRINT(("\t--inI010 input YUV is I010 format (10-bit pixel in LSB of 16 bits)\n"));
    DEBUG_PRINT(("\t--pix-fmt set the input YUV pixel format(NV12, 420P)\n"));
    DEBUG_PRINT(("\t--outp010 set output to p010 format\n"));
    DEBUG_PRINT(("\t--outI010 set output to I010 format\n"));
    DEBUG_PRINT(("\t--outL010 set output to L010 format\n"));
    DEBUG_PRINT(("\t--size set the file to indicate the input YUV info.\n"));
    DEBUG_PRINT(("\t-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n"));
    DEBUG_PRINT(("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g., \n"));
    DEBUG_PRINT(("\t  -d2 -- down scale to 1/2 in both directions\n"));
    DEBUG_PRINT(("\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n"));
    DEBUG_PRINT(("\t--cr-first PP outputs chroma in CrCb order.\n"));
    DEBUG_PRINT(("\t-T PP outputs in tiled format.\n"));
    DEBUG_PRINT(("\t-P PP outputs in planar format.\n"));
    DEBUG_PRINT(("\t-C[xywh]NNN Cropping parameters. E.g.,\n"));
    DEBUG_PRINT(("\t  -Cx8 -Cy16        Crop from (8, 16)\n"));
    DEBUG_PRINT(("\t  -Cw720 -Ch480     Crop size  720x480\n"));
    DEBUG_PRINT(("\t-Dwxh  PP output size wxh. E.g.,\n"));
    DEBUG_PRINT(("\t  -D1280x720        PP output size 1280x720\n\n"));
    DEBUG_PRINT(("\t--pp-rgb Enable Yuv2RGB.\n"));
    DEBUG_PRINT(("\t--pp-rgb-planar Enable planar Yuv2RGB.\n"));
    DEBUG_PRINT(("\t--rgb-video-range set the input YUV video range.\n"));
    DEBUG_PRINT(("\t--rgb-fmat set the RGB output format.\n"));
    DEBUG_PRINT(("\t--rgb-std set the standard coeff to do Yuv2RGB.\n"));
}

/*------------------------------------------------------------------------------

    Function name:  main

    Purpose:
        main function of decoder testbench. Provides command line interface
        with file I/O for H.264 decoder. Prints out the usage information
        when executed without arguments.

------------------------------------------------------------------------------*/

int main(int argc, char **argv) {

  u32 i, j, m, n;
  u32 pp_width, pp_height, pp_stride, pp_stride_2;
  u32 pp_planar, pp_cr_first, pp_mono_chrome, pp_rgb_out;
  long int strm_len;
  PPResult ret;
  PPConfig dec_cfg = {0};
  PPDecPicture dec_picture;
  const void *dwl = NULL;
  struct DWLInitParam dwl_init = { DWL_CLIENT_TYPE_ST_PP };
  struct DecHwFeatures hw_feature;
  u32 hw_build_id;

  u32 pic_num = 0;
  u32 decode_pic_num = 0;
  u32 max_num_pics = 0;
  u32 in_stride = 0;
  u32 in_height = 0;
  u32 in_width = 0;
  u32 in_p010 = 0;
  u32 in_I010 = 0;
  u32 input_pixel_planar = 0;
  u32 pp_buff_size = 0, buff_size;
  u32 ds_ratio_x, ds_ratio_y;

  FILE *f_tbcfg = NULL;

  FILE *in_ctrl = NULL;
  u32 ctrl_flag = 0;
  u32 len;
  u32 * pic_ctrl = NULL;
  char pic_name[256] = {0};
  struct DWLLinearMem pp_in_buffer = {0};
  struct DWLLinearMem pp_out_buffer = {0};
  struct TestParams params;
  u32 tmp;
  (void)tmp;
  int ra;

  SetupDefaultParams(&params);
  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if(f_tbcfg == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n"));
    DEBUG_PRINT(("USING DEFAULT CONFIGURATION\n"));
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if(TBCheckCfg(&tb_cfg) != 0)
      return -1;
  }

  if(argc < 2) {
    PrintUsagePp(argv[0]);
    return 0;
  }

  if (argc == 2) {
    if ((strncmp(argv[1], "-h", 2) == 0) ||
        (strcmp(argv[1], "--help") == 0)){
      PrintUsagePp(argv[0]);
      return 0;
    }
  }

  /* read command line arguments */
  for(i = 1; i < (u32) (argc - 1); i++) {
    if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_pics = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-S", 2) == 0) {
      in_stride = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-H", 2) == 0) {
      in_height = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-W", 2) == 0) {
      in_width = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(out_file_name[j], argv[i] + 2);
    } else if(strcmp(argv[i], "--inp010") == 0) {
      in_p010 = 1;
    } else if(strcmp(argv[i], "--inI010") == 0) {
      in_p010 = 1;
      in_I010 = 1;
    } else if(strcmp(argv[i], "--outp010") == 0) {
      params.ppu_cfg[0].out_p010 = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    } else if(strcmp(argv[i], "--outI010") == 0) {
      params.ppu_cfg[0].out_I010 = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    } else if(strcmp(argv[i], "--outL010") == 0) {
      params.ppu_cfg[0].out_L010 = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    } else if ((strncmp(argv[i], "--pix-fmt", 9) == 0)) {
      if (strcmp(argv[i]+10, "NV12") == 0)
        input_pixel_planar = 0;
      else if (strcmp(argv[i]+10, "420P") == 0)
        input_pixel_planar = 1;
      else {
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        return 1;
      }
    } else if(strncmp(argv[i], "--sizes", 7) == 0) {
      char *px = strchr(argv[i]+7, '=');
      strcpy(pic_name, px+1);
    } else if(strncmp(argv[i], "-m", 2) == 0) {
      md5sum = 1;
    } else if(strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
#ifdef SUPPORT_CACHE
    } else if(strcmp(argv[i], "--cache_enable") == 0) {
      tb_cfg.cache_enable = 1;
    } else if(strcmp(argv[i], "--shaper_enable") == 0) {
      tb_cfg.shaper_enable = 1;
    } else if(strcmp(argv[i], "--shaper_bypass") == 0) {
      tb_cfg.shaper_bypass = 1;
    } else if(strcmp(argv[i], "--pp-shaper") == 0) {
      params.ppu_cfg[0].shaper_enabled = 1;
#endif
    } else if (strncmp(argv[i], "-d", 2) == 0) {
      scale_enabled = 1;
      pp_enabled = 1;
      if (strlen(argv[i]) == 3 &&
          (argv[i][2] == '1' || argv[i][2] == '2' || argv[i][2] == '4' || argv[i][2] == '8')) {
        ds_ratio_x = ds_ratio_y = argv[i][2] - '0';
      } else if (strlen(argv[i]) == 5 &&
                 ((argv[i][2] == '1' || argv[i][2] == '2' || argv[i][2] == '4' || argv[i][2] == '8') &&
                  (argv[i][4] == '1' || argv[i][4] == '2' || argv[i][4] == '4' || argv[i][4] == '8') &&
                  argv[i][3] == ':')) {
        ds_ratio_x = argv[i][2] - '0';
        ds_ratio_y = argv[i][4] - '0';
      } else {
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable down scaler parameter by using: -d[1248][:[1248]]\n");
        return 1;
      }
      params.fscale_cfg.down_scale_x = ds_ratio_x;
      params.fscale_cfg.down_scale_y = ds_ratio_y;
      params.fscale_cfg.fixed_scale_enabled = 1;
      params.pp_enabled = 1;
    } else if (strncmp(argv[i], "-D", 2) == 0) {
      char *px = strchr(argv[i]+2, 'x');
#if 0
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return 1;
      } else if (!px) {
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable scaler parameter by using: -D[w]x[h]\n");
        return 1;
      }
#endif
      *px = '\0';
      scale_enabled = 1;
      pp_enabled = 1;
      scaled_w = atoi(argv[i]+2);
      scaled_h = atoi(px+1);
      if (scaled_w == 0 || scaled_h == 0) {
        fprintf(stdout, "Illegal scaled width/height: %s,%s\n", argv[i]+2, px+1);
        return 1;
      }
      params.ppu_cfg[0].scale.width = scaled_w;
      params.ppu_cfg[0].scale.height = scaled_h;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].scale.enabled = 1;
      params.pp_enabled = 1;
    } else if (strncmp(argv[i], "-C", 2) == 0) {
      crop_enabled = 1;
      pp_enabled = 1;
#if 0
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return 1;
      }
#endif
      switch (argv[i][2]) {
      case 'x':
        crop_x = atoi(argv[i]+3);
        break;
      case 'y':
        crop_y = atoi(argv[i]+3);
        break;
      case 'w':
        crop_w = atoi(argv[i]+3);
        break;
      case 'h':
        crop_h = atoi(argv[i]+3);
        break;
      default:
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,\n");
        fprintf(stdout, "\t -CxXXX -CyYYY     Crop from (XXX, YYY)\n");
        fprintf(stdout, "\t -CwWWW -ChHHH     Crop size  WWWxHHH\n");
        return 1;
      }
      params.ppu_cfg[0].crop.x = crop_x;
      params.ppu_cfg[0].crop.y = crop_y;
      params.ppu_cfg[0].crop.width = crop_w;
      params.ppu_cfg[0].crop.height = crop_h;
      params.ppu_cfg[0].crop.enabled = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    } else if (strncmp(argv[i], "-A", 2) == 0) {
      u32 a = atoi(argv[i] + 2);
#define CASE_ALIGNMENT(NBYTE) case NBYTE: align = DEC_ALIGN_##NBYTE##B; break;
      switch (a) {
      CASE_ALIGNMENT(1);
      CASE_ALIGNMENT(8);
      CASE_ALIGNMENT(16);
      CASE_ALIGNMENT(32);
      CASE_ALIGNMENT(64);
      CASE_ALIGNMENT(128);
      CASE_ALIGNMENT(256);
      CASE_ALIGNMENT(512);
      CASE_ALIGNMENT(1024);
      CASE_ALIGNMENT(2048);
      default:
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: valid alignment value: 1/8/16/32/64/128/256/512/1024/2048 bytes\n");
        return 1;
      }
      params.align = align;
    } else if (strcmp(argv[i], "--cr-first") == 0) {
      params.ppu_cfg[0].cr_first = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "--pp-luma-only") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].monochrome = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
    } else if ((strncmp(argv[i], "-H", 2) == 0) ||
              (strcmp(argv[i], "--help") == 0)){
      PrintUsage(argv[0], PPDEC);
    } else if (strcmp(argv[i], "-T") == 0) {
      pp_tile_out = 1;
      params.ppu_cfg[0].tiled_e = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "-P") == 0) {
      params.ppu_cfg[0].planar = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
    } else if ((strcmp(argv[i], "--pp-rgb") == 0)) {
      params.ppu_cfg[0].rgb = 1;
      params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
      rgb_enabled = 1;
    } else if ((strcmp(argv[i], "--pp-rgb-planar") == 0)) {
      params.ppu_cfg[0].rgb_planar = 1;
      params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      pp_enabled = 1;
      rgb_enabled = 1;
    } else if ((strncmp(argv[i], "--rgb-video-range", 17) == 0)) {
      if (strcmp(argv[i]+18, "1") == 0)
        params.video_range = 1;
      else if (strcmp(argv[i]+18, "0") == 0)
        params.video_range = 0;
      else
        fprintf(stdout, "Illegal parameter\n");
    } else if ((strncmp(argv[i], "--rgb-fmat", 10) == 0)) {
      if (strcmp(argv[i]+11, "RGB888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      else if (strcmp(argv[i]+11, "BGR888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_BGR888;
      else if (strcmp(argv[i]+11, "R16G16B16") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_R16G16B16;
      else if (strcmp(argv[i]+11, "B16G16R16") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_B16G16R16;
      else if (strcmp(argv[i]+11, "ARGB888") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_ARGB888;
      else if (strcmp(argv[i]+11, "ABGR888") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_ABGR888;
      else if (strcmp(argv[i]+11, "A2R10G10B10") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_A2R10G10B10;
      else if (strcmp(argv[i]+11, "A2B10G10R10") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_A2B10G10R10;
      else
        fprintf(stdout, "Illegal parameter %s\n", argv[i]);
    } else if ((strncmp(argv[i], "--rgb-std", 9) == 0)) {
      if (strcmp(argv[i]+10, "BT601") == 0)
        params.rgb_stan = BT601;
      else if (strcmp(argv[i]+10, "BT601_L") == 0)
        params.rgb_stan = BT601_L;
      else if (strcmp(argv[i]+10, "BT709") == 0)
        params.rgb_stan = BT709;
      else if (strcmp(argv[i]+10, "BT709_L") == 0)
        params.rgb_stan = BT709_L;
      else if (strcmp(argv[i]+10, "BT2020") == 0)
        params.rgb_stan = BT2020;
      else if (strcmp(argv[i]+10, "BT2020_L") == 0)
        params.rgb_stan = BT2020_L;
      else
        fprintf(stdout, "Illegal parameter %s\n", argv[i]);
    } else {
      DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
      return 1;
    }
  }

  /* open input file for reading, file name given by user. If file open
   * fails -> exit */
  in_file_name = argv[argc - 1];
  finput = fopen(argv[argc - 1], "rb");
  if(finput == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE\n"));
    return -1;
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
  tb_cfg.pp_params.pipeline_e = 0; /* Disable pipeline mode for pp standalone tb. */
#endif

#ifdef ASIC_TRACE_SUPPORT
  /* open tracefiles */
  tmp = OpenTraceFiles();
  if(!tmp) {
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
  }

  /* determine test case id from input file name (if contains "case_") */
  {
    char* pc, *pe;
    char in[256] = {0};
    strncpy(in, in_file_name, strnlen(in_file_name, 256));
    pc = strstr(in, "case_");
    if (pc != NULL) {
      pc += 5;
      pe = strstr(pc, "/");
      if (pe == NULL) pe = strstr(pc, ".");
      if (pe != NULL) {
        *pe = '\0';
        test_case_id = atoi(pc);
      }
    }
  }

#ifdef SUPPORT_CACHE
  if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
    tb_cfg.dec_params.cache_support = 1;
  else
    tb_cfg.dec_params.cache_support = 0;
#endif
#endif
  if (tb_cfg.pp_params.pre_fetch_height == 16)
    dec_pp_in_blk_size = 0;
  else if (tb_cfg.pp_params.pre_fetch_height == 64)
    dec_pp_in_blk_size = 1;
  in_ctrl = fopen(pic_name, "r");
  if (in_ctrl == NULL) {
    ctrl_flag = 0;
    if (!in_width || !in_height) {
      fprintf(stdout, "ERROR: Input width/height must be specified by options "
                      "-W/-H.\n");
      return 1;
    }
    if (!in_stride)
      in_stride = in_width << in_p010;
  }
  else {
    ctrl_flag = 1;
    fseek(in_ctrl, 0L, SEEK_END);
    len = ftell(in_ctrl);
    rewind(in_ctrl);
    pic_num = len / 16;
    pic_ctrl = (u32*)malloc(len);
    if (pic_ctrl == NULL)
      return 1;
    ra = fread((u8*)(pic_ctrl), 1, len, in_ctrl);
    (void) ra;
  }

  /* check size of the input file -> length of the stream in bytes */
  if (!ctrl_flag) {
    fseek(finput, 0L, SEEK_END);
    strm_len = ftell(finput);
    pic_num = (strm_len / (in_stride * in_height * 3 / 2));
    rewind(finput);
  } else {
    in_p010 = pic_ctrl[0];
    in_width = pic_ctrl[1];
    in_stride = pic_ctrl[2];
    in_height = pic_ctrl[3];
  }
  if (in_p010)
      pixel_width = 10;
#ifdef ASIC_TRACE_SUPPORT
  tb_cfg.pp_params.in_width = in_width;
  tb_cfg.pp_params.in_height = in_height;
#endif

  ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                         !tb_cfg.pp_params.pipeline_e);
  if (params.fscale_cfg.fixed_scale_enabled) {
    u32 crop_w = params.ppu_cfg[0].crop.width;
    u32 crop_h = params.ppu_cfg[0].crop.height;
    if (!crop_w) crop_w = in_width;
    if (!crop_h) crop_h = in_height;
    params.ppu_cfg[0].scale.width = (crop_w / params.fscale_cfg.down_scale_x) & ~0x1;
    params.ppu_cfg[0].scale.height = (crop_h /
                                      params.fscale_cfg.down_scale_y) & ~0x1;
    params.ppu_cfg[0].scale.enabled = 1;
    params.ppu_cfg[0].enabled = 1;
    params.pp_enabled = 1;
  }
  pp_enabled = params.pp_enabled;
  memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (pixel_width > 8) {
      if (dec_cfg.ppu_config[i].tiled_e)
        dec_cfg.ppu_config[i].out_cut_8bits = 0;
      else if (!dec_cfg.ppu_config[i].out_p010 && ! dec_cfg.ppu_config[i].out_I010 && !dec_cfg.ppu_config[i].out_L010 &&
               !dec_cfg.ppu_config[i].rgb && !dec_cfg.ppu_config[i].rgb_planar)
        dec_cfg.ppu_config[i].out_cut_8bits = 1;
    } else  {
      dec_cfg.ppu_config[i].out_cut_8bits = 0;
      dec_cfg.ppu_config[i].out_p010 = 0;
      dec_cfg.ppu_config[i].out_1010 = 0;
      dec_cfg.ppu_config[i].out_I010 = 0;
      dec_cfg.ppu_config[i].out_L010 = 0;
    }
    if (dec_cfg.ppu_config[i].video_range) {
      if (pixel_width == 8) {
        dec_cfg.ppu_config[i].range_max = 255;
        dec_cfg.ppu_config[i].range_min = 0;
      } else {
        dec_cfg.ppu_config[i].range_max = 1023;
        dec_cfg.ppu_config[i].range_min = 0;
      }
    } else {
      if (pixel_width == 8) {
        dec_cfg.ppu_config[i].range_max = 235;
        dec_cfg.ppu_config[i].range_min = 16;
      } else {
        dec_cfg.ppu_config[i].range_max = 940;
        dec_cfg.ppu_config[i].range_min = 64;
      }
    }
    if (dec_cfg.ppu_config[i].monochrome) {
      dec_cfg.ppu_config[i].rgb = 0;
      dec_cfg.ppu_config[i].rgb_planar = 0;
    }
    if (!dec_cfg.ppu_config[i].crop.width || !dec_cfg.ppu_config[i].crop.height) {
      dec_cfg.ppu_config[i].crop.x = dec_cfg.ppu_config[i].crop.y = 0;
      dec_cfg.ppu_config[i].crop.width = in_width;
      dec_cfg.ppu_config[i].crop.height = in_height;
    }
    if (!dec_cfg.ppu_config[i].scale.width || !dec_cfg.ppu_config[i].scale.height) {
      dec_cfg.ppu_config[i].scale.width = dec_cfg.ppu_config[i].crop.width;
      dec_cfg.ppu_config[i].scale.height = dec_cfg.ppu_config[i].crop.height;
    }
  }
  if (!pp_enabled) {
    DEBUG_PRINT(("PP is not enabled. Finish.\n"));
    goto end;
  }

  /* Initialize decoder. If unsuccessful -> exit */
  dwl = DWLInit(&dwl_init);
  ret = PPInit(&pp_inst, dwl);
  if(ret != PP_OK) {
    DEBUG_PRINT(("PP INITIALIZATION FAILED\n"));
    goto end;
  }
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_ST_PP);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
  if (!dec_cfg.ppu_config[0].shaper_enabled && !dec_cfg.ppu_config[1].shaper_enabled &&
      !dec_cfg.ppu_config[2].shaper_enabled && !dec_cfg.ppu_config[3].shaper_enabled) {
    tb_cfg.shaper_enable = 0;
    tb_cfg.dec_params.cache_support = 0;
  }
#endif
#endif
  dec_cfg.in_format = in_p010;
  dec_cfg.in_stride = in_stride;
  dec_cfg.in_height = in_height;
  dec_cfg.in_width = in_width;
  buff_size = in_stride * in_height * 3 / 2;
  if(DWLMallocLinear(dwl, buff_size, &pp_in_buffer) != DWL_OK) {
    DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end;
  }
  u32 ext_buffer_size = 0;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!dec_cfg.ppu_config[i].enabled) continue;
    pixel_width_pp = (dec_cfg.ppu_config[i].out_cut_8bits ||
                      pixel_width == 8) ? 8 :
                      ((dec_cfg.ppu_config[i].out_p010 || dec_cfg.ppu_config[i].out_I010 || dec_cfg.ppu_config[i].out_L010 ||
			dec_cfg.ppu_config[i].rgb || dec_cfg.ppu_config[i].rgb_planar ||
                      (dec_cfg.ppu_config[i].tiled_e && pixel_width > 8)) ? 16 : pixel_width);
    if (dec_cfg.ppu_config[i].tiled_e) {
      pp_width = NEXT_MULTIPLE(dec_cfg.ppu_config[i].scale.width, 4);
      pp_height = NEXT_MULTIPLE(dec_cfg.ppu_config[i].scale.height, 4) / 4;
      pp_stride = NEXT_MULTIPLE(4 * pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
      pp_buff_size = pp_stride * pp_height;
      /* chroma */
      if (!dec_cfg.ppu_config[i].monochrome) {
        pp_height = NEXT_MULTIPLE(dec_cfg.ppu_config[i].scale.height/2, 4) / 4;
        pp_buff_size += pp_stride * pp_height;
      }
    } else {
      pp_width = dec_cfg.ppu_config[i].scale.width * (dec_cfg.ppu_config[i].rgb ? 3 : 1);
      pp_height = dec_cfg.ppu_config[i].scale.height;
      if (dec_cfg.ppu_config[i].rgb) {
        if (hw_feature.rgb_line_stride_support)
          pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        else
          pp_stride = pp_width * pixel_width_pp / 8;
        pp_buff_size = NEXT_MULTIPLE(pp_stride * pp_height, HABANA_OFFSET);
      } else if (dec_cfg.ppu_config[i].rgb_planar) {
        if (hw_feature.rgb_line_stride_support)
          pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        else
          pp_stride = pp_width * pixel_width_pp / 8;
        pp_buff_size = 3 * NEXT_MULTIPLE(pp_stride * pp_height, HABANA_OFFSET);
      } else {
        pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        pp_stride_2 = NEXT_MULTIPLE(pp_width / 2 * pixel_width_pp, ALIGN(align) * 8) / 8;
        pp_buff_size = pp_stride * pp_height;
      }
      if (!dec_cfg.ppu_config[i].monochrome && !dec_cfg.ppu_config[i].rgb && !dec_cfg.ppu_config[i].rgb_planar) {
        if (!dec_cfg.ppu_config[i].planar)
          pp_buff_size += pp_stride * pp_height / 2;
        else
          pp_buff_size += pp_stride_2 * pp_height;
      }
    }
    ext_buffer_size += NEXT_MULTIPLE(pp_buff_size, 16);
  }

  pp_buff_size = ext_buffer_size;
  if(DWLMallocLinear(dwl, pp_buff_size, &pp_out_buffer) != DWL_OK) {
    DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end;
  }
  dec_cfg.pp_in_buffer = pp_in_buffer;
  dec_cfg.pp_out_buffer = pp_out_buffer;
  ret = PPSetInfo(pp_inst, &dec_cfg);
  if (ret != PP_OK) {
    DEBUG_PRINT(("Invalid pp parameters\n"));
    goto end;
  }

  /* main decoding loop */
  i = 0;
  do {
    if ((i != 0) && (ctrl_flag)) {
      in_p010 = pic_ctrl[4 * i];
      in_width = pic_ctrl[4 * i + 1];
      in_stride = pic_ctrl[4 * i + 2];
      in_height = pic_ctrl[4 * i + 3];
#ifdef ASIC_TRACE_SUPPORT
      tb_cfg.pp_params.in_width = in_width;
      tb_cfg.pp_params.in_height = in_height;
#endif
      if (in_stride * in_height * 3 / 2 != buff_size) {
        buff_size = in_stride * in_height * 3 / 2;
        DWLFreeLinear(dwl, &pp_in_buffer);
        if(DWLMallocLinear(dwl, buff_size, &pp_in_buffer) != DWL_OK) {
          DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
          goto end;
        }
        dec_cfg.in_format = in_p010;
        dec_cfg.in_stride = in_stride;
        dec_cfg.in_height = in_height;
        dec_cfg.in_width = in_width;
        dec_cfg.pp_in_buffer = pp_in_buffer;
        ret = PPSetInfo(pp_inst, &dec_cfg);
        if (ret != PP_OK) {
          DEBUG_PRINT(("Invalid pp parameters\n"));
          goto end;
        }
      }
    }
    /* Picture ID is the picture number in decoding order */
    decode_pic_num++;
    ra = fread((u8*)(pp_in_buffer.virtual_address), 1, buff_size, finput);
    (void) ra;
    if (in_I010) {
      u16* base = (u16*)pp_in_buffer.virtual_address;
      for (m = 0; m < in_height * 3 / 2; m++) {
        for (n = 0; n < in_width; n++) {
          *base = *base << 6;
          base++;
        }
      }
    }
    if (input_pixel_planar) {
      u8 *data_ch = (u8*)malloc(buff_size/3);
      u8 *data_in_ch = (u8*)(pp_in_buffer.virtual_address)+2*buff_size/3;
      for (j = 0; j < buff_size/6;j++) {
        data_ch[2*j] = data_in_ch[j];
        data_ch[2*j+1] = data_in_ch[j+buff_size/6];
      }
      for (j = 0; j < buff_size/3;j++)
        data_in_ch[j] = data_ch[j];
      free(data_ch);
    }
    /* call API function to perform decoding */
    ret = PPDecode(pp_inst);
    ret = PPNextPicture(pp_inst, &dec_picture);
    for (j = 0; j < DEC_MAX_PPU_COUNT; j++) {
      if (!dec_picture.pictures[j].pic_width ||
         !dec_picture.pictures[j].pic_height)
        continue;

      u32 pixel_bytes = 1;
      if(in_p010 == 1 && dec_cfg.ppu_config[j].out_cut_8bits != 1)
        pixel_bytes = 2;

      pp_planar      = 0;
      pp_cr_first    = 0;
      pp_mono_chrome = 0;
      pp_rgb_out     = 0;
      if (IS_PIC_PACKED_RGB(dec_picture.pictures[j].output_format) ||
          IS_PIC_PLANAR_RGB(dec_picture.pictures[j].output_format))
        pp_rgb_out = 1;
      if(IS_PIC_PLANAR(dec_picture.pictures[j].output_format))
        pp_planar = 1;
      else if(IS_PIC_NV21(dec_picture.pictures[j].output_format))
        pp_cr_first = 1;
      else if(IS_PIC_MONOCHROME(dec_picture.pictures[j].output_format))
        pp_mono_chrome = 1;
      /* Write output picture to file */
      if (pp_rgb_out) {
        WriteOutputRGB(out_file_name[j], (u8 *) dec_picture.pictures[j].output_picture,
                    dec_picture.pictures[j].pic_width,
                    dec_picture.pictures[j].pic_height,
                    decode_pic_num - 1,
                    pp_mono_chrome, 0, 0, 0,
                    IS_PIC_TILE(dec_picture.pictures[j].output_format),
                    dec_picture.pictures[j].pic_stride, dec_picture.pictures[j].pic_stride_ch, j,
                    pp_planar, pp_cr_first, 0, 0, DEC_DPB_FRAME, md5sum, &foutput[j],
                    pixel_bytes);
      } else {
        WriteOutput(out_file_name[j], (u8 *) dec_picture.pictures[j].output_picture,
                    dec_picture.pictures[j].pic_width,
                    dec_picture.pictures[j].pic_height,
                    decode_pic_num - 1,
                    pp_mono_chrome, 0, 0, 0,
                    IS_PIC_TILE(dec_picture.pictures[j].output_format),
                    dec_picture.pictures[j].pic_stride, dec_picture.pictures[j].pic_stride_ch, j,
                    pp_planar, pp_cr_first, 0, 0, DEC_DPB_FRAME, md5sum, &foutput[j],
                    pixel_bytes);
      }
    }
    if (decode_pic_num == max_num_pics)
      decode_pic_num = pic_num;
    i++;
   } while(decode_pic_num < pic_num);


end:
#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif
  /* release decoder instance */
  if (pp_in_buffer.virtual_address)
    DWLFreeLinear(dwl, &pp_in_buffer);
  if (pp_out_buffer.virtual_address)
    DWLFreeLinear(dwl, &pp_out_buffer);
  if (pp_inst)
    PPRelease(pp_inst);
  if (dwl)
    DWLRelease(dwl);
  if (pic_ctrl)
    free(pic_ctrl);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(foutput[i])
      fclose(foutput[i]);
  }
  return 0;
}
