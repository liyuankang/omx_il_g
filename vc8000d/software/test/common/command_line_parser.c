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

#include "command_line_parser.h"
#include "dwlthread.h"

#include <stdio.h>
#ifdef _HAVE_PTHREAD_H
#include <stdlib.h>
#endif
#include <string.h>
#include <ctype.h>

extern int optind, opterr, optopt;

void PrintUsage(char* executable, enum TestBenchFormat fmt) {
#ifndef _WIN32
  char helptext[] = {
    #include "./vcdec_help.dat"
  };

  fprintf(stdout, "%s", helptext);
#endif
  printf("Usage: %s [options] <file>\n", executable);
  printf("\t-H Print command line parameters help info. (--help)\n");
/*
  printf("\n\tDecoding options:\n");
  printf(
    "\t-N<n> stop after <n> output pictures. "
    "(--num-pictures <n>)\n");

  printf("\n\tPicture format options:\n");
  printf("\t-C crop the parts not containing visual data in the pictures. ");
  printf("(--crop)\n");
  printf("\tOne of the following:\n");
  printf("\t\t-T 4x4 tiled, uncropped semiplanar YCbCr 4:2:0) (--tiled) ");
  printf("[default]\n");
  printf("\t\t-S use uncropped raster-ordered, semiplanar YCbCr 4:2:0");
  printf(", four_cc 'NV12' (--semiplanar)\n");
  printf("\t\t-P use planar frame format, four_cc 'IYUV'. (--planar)\n");

  printf("\n\tInput options:\n");
  printf("\t(default: auto detect format from file extension");
  printf(", output frame-by-frame)\n");
  printf("\t-i<format> (--input-format), force input file format");
  printf(" interpretation. <format> can be one of the following:\n");
  printf("\t\t bs -- bytestream format\n");
  printf("\t\t ivf -- IVF file format\n");
  printf("\t\t webm -- WebM file format\n");
  printf("\t-p packetize input bitstream.");
  printf("(--packet-by-packet)\n");
  printf("\t-F read full-stream into single buffer. Only with bytestream.");
  printf(" (--full-stream)\n");
  printf("\t-u NALU input bitstream (without start code).");
  printf("(--nalu)\n");

  printf("\n\tOutput options:\n");
  printf("\t-t generate hardware trace files. (--trace-files)\n");
  printf("\t-r trace file format for RTL (extra CU ctrl). (--rtl-trace)\n");
  printf("\t-R Output in decoding order. (--disable-display-order)\n");
  printf("\tOne of the following:\n");
  printf("\t\t-X disable output writing. (--no-write)\n");
  printf(
    "\t\t-O<outfile> write output to <outfile>. "
    "(--output-file <outfile>) (default out.yuv)\n");
  printf("\t\t-Q Output single frames (--single-frames-out)\n");
  printf("\t\t-M write MD5 sum to output instead of yuv. (--md5)\n");
  printf(
    "\t\t-m write MD5 sum for each picture to output instead of yuv. "
    "(--md5-per-pic)\n");
*/
#ifdef SDL_ENABLED
  printf("\t\t-s Use SDL to display the output pictures. Implies also '-P'.");
  printf(" (--sdl)\n");
#endif
/*
  printf("\n\tThreading options\n");
  printf(
    "\t-Z Run output handling in separate thread. "
    "(--separate-output-thread)\n");
  printf("\n\tEnable hardware features (all listed features disabled by ");
  printf("default)\n");
  printf("\t-E<feature> (--enable), enable hw feature where <feature> is ");
  printf("one of the following:\n");
  printf("\t\t rs -- raster scan conversion\n");
  printf("\t\t p010 -- output in P010 format for 10-bit stream\n");
  printf("\t-s[yc]NNN (--out_stride [yc]NNN), Set PP stride for y/c plane, sw check the validation of the value. E.g.,\n");
  printf("\t\t-sy720 -sc360     Set ystride and cstride to 720 and 360.\n");
  printf("\t-d<downscale> (--down_scale), enable down scale feature where <downscale> is ");
  printf("one of the following:\n");
  printf("\t\t <ds_ratio> -- down scale to 1/<ds_ratio> in both directions\n");
  printf("\t\t <ds_ratio_x>:<ds_ratio_y> -- down scale to 1/<ds_ratio_x> in horizontal and 1/<ds_ratio_y> in vertical\n");
  printf("\t\t\t <ds_ratio> should be one of following: 2, 4, 8\n");
  printf("\t-Dwxh  PP output size wxh. E.g.,\n");
  printf("\t\t-D1280x720        PP output size 1280x720\n");
  printf("\t-C[xywh]NNN (--crop [xywh]NNN), Cropping parameters. E.g.,\n");
  printf("\t\t-Cx8 -Cy16        Crop from (8, 16)\n");
  printf("\t\t-Cw720 -Ch480     Crop size  720x480\n");
  printf("\t-Cd Output crop picture by testbench instead of PP.\n");
  printf("\t-f Force output in 8 bits per pixel for HEVC Main 10 profile. (--force-8bits)\n");
  printf("\t--shaper_bypass Enable shaper bypass rtl simulation (external hw IP).\n");
  printf("\t--cache_enable  Enable cache rtl simulation (external hw IP).\n");
  printf("\t--shaper_enable Enable shaper rtl simulation (external hw IP).\n");
  printf("\t--pp-shaper Enable shaper for ppu0.\n");
  printf("\t--delogo(--enable), enable delogo feature,");
  printf("need configure parameters as the following:\n");
  printf("\t\t pos -- the delog pos wxh@(x,y)\n");
  printf("\t\t show -- show the delogo border\n");
  printf("\t\t mode -- select the delogo mode\n");
  printf("\t\t YUV -- set the replace value if use PIXEL_REPLACE mode\n");

  printf("\n\tOther features:\n");
  printf("\t-b Bypass reference frame compression (--compress-bypass)\n");
  printf("\t-n Use non-ringbuffer mode for stream input buffer. Default: ring buffer mode. (--non-ringbuffer)\n");
  printf("\t-A<n> Set stride aligned to n bytes (valid value: 1/8/16/32/64/128/256/512/1024/2048)\n");
  printf("\t--ultra-low-latency Data transmission use low latency mode\n");
  printf("\t--platform-low-latency Enable low latency platform running flag\n");
  printf("\t--secure Enable secure mode flag\n");
  printf("\t--tile-by-tile Enable tile-by-tile decoding\n");
  printf("\t--cr-first PP outputs chroma in CrCb order\n");
  printf("\t--mc enable Enable frame level multi-core decoding\n");
  printf("\t--mvc Enable MVC decoding (H264 only).\n");
  printf("\t--partial Enable partial decoding.\n");
  printf("\t--pp-rgb Enable Yuv2RGB.\n");
  printf("\t--pp-rgb-planar Enable planar Yuv2RGB.\n");
  printf("\t--rgb-fmat set the RGB output format.\n");
  printf("\t--rgb-std set the standard coeff to do Yuv2RGB.\n");
  printf("\t--second-crop enable second crop configure.\n");
*/
  if (fmt & JPEG_DEC) {
    fprintf(stdout, "\t--full-only Force to full resolution decoding only. (JPEG only)\n");
    fprintf(stdout, "\t--ri_mc_enable Enable restart interval based multicore decoding. (JPEG only)\n");
    fprintf(stdout, "\t--instant_buffer Output buffer provided by user. (JPEG only)\n");
  }
  printf("\n");
}

void SetupDefaultParams(struct TestParams* params) {
  int i = 0;
  memset(params, 0, sizeof(struct TestParams));
  params->read_mode = STREAMREADMODE_FRAME;
  params->fscale_cfg.down_scale_x = 1;
  params->fscale_cfg.down_scale_y = 1;
  params->is_ringbuffer = 1;
  params->display_cropped = 0;
  params->tile_by_tile = 0;
  params->mc_enable = 0;
  params->video_range = 1;
  params->rgb_stan = BT601;
  memset(params->ppu_cfg, 0, sizeof(params->ppu_cfg));
  memset(params->delogo_params, 0, sizeof(params->delogo_params));
  
  /*for Multi-stream*/
  params->multimode = 0;
  params->nstream = 0;
  for(i = 0; i < MAX_STREAMS; i++)
    params->streamcfg[i] = NULL;
}

static int ParsePosParams(char *optarg, u32 *x, u32 *y, u32 *w, u32 *h) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != 'x') return 1;
  p++; *w = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != '@') return 1;
  p++; *h = atoi(q); q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *x= atoi(q); q = p;
  if (*x <= 0) return 1;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  p++; *y= atoi(q); q = p;
  if (*y <= 0) return 1;
  if (*p) return 1;
  return 0;
}

static int ParsePixelParams(char *optarg, u32 *Y, u32 *U, u32 *V) {
  char *p = optarg;
  char *q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *Y = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *U = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  p++; *V = atoi(q); q = p;
  return 0;
}

int ParseParams(int argc, char* argv[], struct TestParams* params) {
  i32 c = 0;
  i32 option_index = 0;
  u8 flag_S, flag_T, flag_P, flag_s;
  u8 flag_b = 0;
  u32 second_crop = 0;
  u32 a = 0;
  u32 delogo = 0;
  char *px = NULL;
  flag_S = flag_T = flag_P = flag_s = 0;
  static struct option long_options[] = {
    {"crop", required_argument, 0, 'C'},
    {"no-write", no_argument, 0, 'X'},
    {"ultra-low-latency", no_argument, 0, 'l'},
    {"platform-low-latency", no_argument, 0, 'L'},
    {"secure", no_argument, 0, 'y'},
    {"partial", no_argument, 0, 'K'},
    {"tile-by-tile", no_argument, 0, 'B'},
    {"cr-first", no_argument, 0, 'c'},
    {"semiplanar", no_argument, 0, 'S'},
    {"planar", no_argument, 0, 'P'},
    {"md5", no_argument, 0, 'M'},
    {"md5-per-pic", no_argument, 0, 'm'},
    {"mvc", no_argument, 0, 'J'},
    {"num-pictures", required_argument, 0, 'N'},
    {"output-file", required_argument, 0, 'O'},
    {"trace-files", no_argument, 0, 't'},
    {"rtl-trace", no_argument, 0, 'r'},
    {"separate-output-thread", no_argument, 0, 'Z'},
    {"single-frames-out", no_argument, 0, 'Q'},
    {"full-stream", no_argument, 0, 'F'},
    {"packet-by-packet", no_argument, 0, 'p'},
    {"nalu", no_argument, 0, 'u'},
    {"enable", required_argument, 0, 'E'},
    {"tiled", no_argument, 0, 'T'},
    {"disable-display-order", no_argument, 0, 'R'},
    {"sdl", no_argument, 0, 'j'},
    {"input-format", required_argument, 0, 'i'},
    {"down-scale", required_argument, 0, 'd'},
    {"flexible-scale", required_argument, 0, 'D'},
    {"stride", required_argument, 0, 'A'},
    {"force-8bits", no_argument, 0, 'f'},
    {"compress-bypass", no_argument, 0, 'b'},
    {"non-ringbuffer", no_argument, 0, 'n'},
    {"prefetch-onepic", no_argument, 0, 'g'},
    {"shaper_bypass", no_argument, 0, 'w'},
    {"cache_enable", no_argument, 0, 'V'},
    {"shaper_enable", no_argument, 0, 'v'},
    {"pp-shaper", no_argument, 0, '2'},
    {"pp-tiled-out", no_argument, 0, 'U'},
    {"pp-planar", no_argument, 0, 'a'},
    {"pp-luma-only", no_argument, 0, 'W'},
    {"pp-rgb", no_argument, 0, 'G'},
    {"pp-rgb-planar", no_argument, 0, 'h'},
    {"rgb-fmat", required_argument, 0, 'I'},
    {"rgb-std", required_argument, 0, 'o'},
    {"out_stride", required_argument, 0, 's'},
    {"delogo", required_argument, 0, 0},
    {"pos", required_argument, 0, 0},
    {"show", required_argument, 0, 0},
    {"mode", required_argument, 0, 0},
    {"YUV", required_argument, 0, 0},
    {"second-crop", no_argument, 0, 1},
    {"mc", no_argument, 0, 'e'},
    {"full-only", no_argument, 0, 0},
    {"ri_mc_enable", no_argument, 0, 0},
    {"instant_buffer", no_argument, 0, 0},
    /* multi stream configs */
    {"multimode", required_argument, 0, 0}, // Multi-stream mode, 0--disable, 1--mult-thread, 2--multi-process(next to do)
    {"streamcfg", required_argument, 0, 0}, // extra stream config.
    {"help", no_argument, 0, 'H'},
    {0, 0, 0, 0}
  };

  /* read command line arguments */
  optind = 1; // to prevent segmentation fault happers after the second and later call getopt_long.
  while ((c = getopt_long(argc, argv,
                          "C:E:Fi:MmN:A:O:s:PpSTtrXlLBcZQRjd:D:fbngWwVvaYUGhHuI:o:k:0:12",
                          long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'C':
      switch (optarg[0]) {
      case 'x':
        if (second_crop)
          params->ppu_cfg[0].crop2.x = atoi(optarg + 1);
        else
          params->ppu_cfg[0].crop.x = atoi(optarg + 1);
        break;
      case 'y':
        if (second_crop)
          params->ppu_cfg[0].crop2.y = atoi(optarg + 1);
        else
          params->ppu_cfg[0].crop.y = atoi(optarg + 1);
        break;
      case 'w':
        if (second_crop)
          params->ppu_cfg[0].crop2.width = atoi(optarg + 1);
        else  
          params->ppu_cfg[0].crop.width = atoi(optarg + 1);
        break;
      case 'h':
        if (second_crop)
          params->ppu_cfg[0].crop2.height = atoi(optarg + 1);
        else
          params->ppu_cfg[0].crop.height = atoi(optarg + 1);
        break;
      case 'd':
        params->display_cropped = 1;
        break;
      default:
        fprintf(stderr, "ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,\n");
        fprintf(stderr, "\t -CxXXX -CyYYY     Crop from (XXX, YYY)\n");
        fprintf(stderr, "\t -CwWWW -ChHHH     Crop size  WWWxHHH\n");
        return 1;
      }
      if (optarg[0]!='d') {
        params->ppu_cfg[0].enabled = 1;
        params->ppu_cfg[0].crop.enabled = 1;
        params->ppu_cfg[0].crop.set_by_user = 1;
        params->pp_enabled = 1;
      }
      break;
    case 'X':
      params->sink_type = SINK_NULL;
      break;
    case 'w':
      params->shaper_bypass = 1;
      break;
    case 'V':
      params->cache_enable = 1;
      break;
    case 'v':
      params->shaper_enable = 1;
      break;
    case '2':
      params->ppu_cfg[0].shaper_enabled = 1;
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'L':
      params->decoder_mode = DEC_LOW_LATENCY;
      break;
    case 'l':
      params->decoder_mode = DEC_LOW_LATENCY_RTL;
      break;
    case 'y':
      params->decoder_mode = DEC_SECURITY;
      break;
    case 'K':
      params->decoder_mode = DEC_PARTIAL_DECODING;
      break;
    case 'B':
      params->tile_by_tile = 1;
      break;
    case 'c':
      //params->cr_first = 1;
      params->ppu_cfg[0].enabled = 1;
      params->ppu_cfg[0].cr_first = 1;
      params->pp_enabled = 1;
      break;
    case 'S':
      if (flag_T || flag_P) {
        fprintf(stderr,
                "ERROR: options -T -P and -S are mutually "
                "exclusive!\n");
        return 1;
      }
      if (flag_s) {
        fprintf(stderr, "ERROR: SDL sink supports only -P!\n");
        return 1;
      }
      flag_S = 1;
      params->format = DEC_OUT_FRM_RASTER_SCAN;
      break;
    case 't':
      params->hw_traces = 1;
      /* TODO(vmr): Check SW support for traces. */
      break;
    case 'r':
      params->trace_target = 1;
      break;
    case 'M':
      params->sink_type = SINK_MD5_SEQUENCE;
      break;
    case 'm':
      params->sink_type = SINK_MD5_PICTURE;
      break;
    case 'N':
      params->num_of_decoded_pics = atoi(optarg);
      break;
    case 'O':
      for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
        params->out_file_name[i] = optarg;
      }
      break;
    case 'p':
      params->read_mode = STREAMREADMODE_PACKETIZE;
      break;
    case 'u':
      params->read_mode = STREAMREADMODE_NALUNIT;
      break;
    case 'P':
#ifdef TB_PP
      if (flag_T || flag_S) {
        fprintf(stderr,
                "ERROR: options -T -P and -S are mutually "
                "exclusive!\n");
        return 1;
      }
      flag_P = 1;
      params->format = DEC_OUT_FRM_PLANAR_420;
#else
      fprintf(stderr,
              "ERROR: -P is not supported when TB_PP is NOT defined.\n");
      return 1;
#endif
      break;
    case 'Z':
      params->extra_output_thread = 1;
      break;
    case 'Q':
      params->sink_type = SINK_FILE_PICTURE;
      break;
    case 'R':
      params->disable_display_order = 1;
      break;
    case 'F':
      params->read_mode = STREAMREADMODE_FULLSTREAM;
      break;
    case 'E':
      if (strcmp(optarg, "rs") == 0) {
        params->hw_format = DEC_OUT_FRM_RASTER_SCAN;
        params->ppu_cfg[0].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "p010") == 0) {
        params->ppu_cfg[0].out_p010 = 1;
        params->ppu_cfg[0].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "I010") == 0) {
        params->ppu_cfg[0].out_I010 = 1;
        params->ppu_cfg[0].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "L010") == 0) {
        params->ppu_cfg[0].out_L010 = 1;
        params->ppu_cfg[0].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "1010") == 0) {
        params->ppu_cfg[0].enabled = 1;
        params->ppu_cfg[0].out_1010 = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "pbe") == 0) {
        params->ppu_cfg[0].enabled = 1;
        params->pp_enabled = 1;
      }
      break;
    case 'T':
      if (flag_P || flag_S) {
        fprintf(stderr,
                "ERROR: options -T -P and -S are mutually "
                "exclusive!\n");
        return 1;
      }
      if (flag_s) {
        fprintf(stderr, "ERROR: SDL sink supports only -P!\n");
        return 1;
      }
      flag_T = 1;
      params->format = DEC_OUT_FRM_TILED_4X4;
      break;
#ifdef SDL_ENABLED
    case 'j':
      if (flag_T || flag_S) {
        fprintf(stderr, "ERROR: SDL sink supports only -P!\n");
        return 1;
      }
      params->sink_type = SINK_SDL;
      params->format = DEC_OUT_FRM_PLANAR_420;
      flag_s = 1;
      break;
#endif /* SDL_ENABLED */
    case 'i':
      if (strcmp(optarg, "bs") == 0) {
        params->file_format = FILEFORMAT_BYTESTREAM;
      } else if (strcmp(optarg, "ivf") == 0) {
        params->file_format = FILEFORMAT_IVF;
      } else if (strcmp(optarg, "webm") == 0) {
        params->file_format = FILEFORMAT_WEBM;
      } else {
        fprintf(stderr, "Unsupported file format\n");
        return 1;
      }
      break;
    case ':':
      fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      return 1;
    case 'd':
      if (strlen(optarg) == 1 && (optarg[0] == '1' ||
                                  optarg[0] == '2' ||
                                  optarg[0] == '4' ||
                                  optarg[0] == '8'))
        params->fscale_cfg.down_scale_x = params->fscale_cfg.down_scale_y = optarg[0] - '0';
      else if (strlen(optarg) == 3 &&
               (optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '4' || optarg[0] == '8') &&
               (optarg[2] == '1' || optarg[2] == '2' || optarg[2] == '4' || optarg[2] == '8') &&
               optarg[1] == ':') {
        params->fscale_cfg.down_scale_x = optarg[0] - '0';
        params->fscale_cfg.down_scale_y = optarg[2] - '0';
      } else {
        fprintf(stderr, "ERROR: Enable down scaler parameter by using: -d[1248][:[1248]]\n");
        return 1;
      }
      fprintf(stderr, "Down scaler enabled: 1/%d, 1/%d\n",
              params->fscale_cfg.down_scale_x, params->fscale_cfg.down_scale_y);
      params->fscale_cfg.fixed_scale_enabled = 1;
      params->ppu_cfg[0].scale.set_by_user = 1;
      params->pp_enabled = 1;
      break;
    case 'D':
      px = strchr(optarg, 'x');
      if (!px) {
        fprintf(stdout, "Illegal parameter\n");
        fprintf(stdout, "ERROR: Enable scaler parameter by using: -D[w]x[h]\n");
        return 1;
      }
      *px = '\0';
      params->ppu_cfg[0].scale.width = atoi(optarg);
      params->ppu_cfg[0].scale.height = atoi(px+1);
      if (params->ppu_cfg[0].scale.width == 0 || params->ppu_cfg[0].scale.height == 0) {
        fprintf(stdout, "Illegal scaled width/height: %d,%d\n",
                params->ppu_cfg[0].scale.width,
                params->ppu_cfg[0].scale.height);
        return 1;
      }
      params->ppu_cfg[0].enabled = 1;
      params->ppu_cfg[0].scale.enabled = 1;
      params->ppu_cfg[0].scale.set_by_user = 1;
      params->pp_enabled = 1;
      break;
    case 'A':
      a = atoi(optarg);
#define CASE_ALIGNMENT(NBYTE) case NBYTE: params->align = DEC_ALIGN_##NBYTE##B; break;
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
        fprintf(stdout, "Illegal parameter: A%s\n", optarg);
        fprintf(stdout, "ERROR: valid alignment value: 1/8/16/32/64/128/256/512/1024/2048 bytes\n");
        return 1;
      }
      break;
    case 's':
      switch (optarg[0]) {
      case 'y':
        params->ppu_cfg[0].ystride = atoi(optarg + 1);
        break;
      case 'c':
        params->ppu_cfg[0].cstride = atoi(optarg + 1);
        break;
      default:
        fprintf(stderr, "ERROR: Enable out stride parameter by using: -s[yc]NNN. E.g.,\n");
        return 1;
      }
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'b':
      flag_b = 1;
      params->compress_bypass = 1;
      break;
    case 'n':
      params->is_ringbuffer = 0;
      break;
    case 'g':
      fprintf(stderr, "Option -g is obsoleted and ignored. Please DON'T use it anymore!\n");
      break;
#ifndef SDL_ENABLED
    case 'j':
#endif /* SDL_ENABLED */
    case 'f':
      params->ppu_cfg[0].out_cut_8bits = 1;
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'U':
      params->ppu_cfg[0].tiled_e = 1;
      params->format = DEC_OUT_FRM_TILED_4X4;
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'W':
      params->ppu_cfg[0].monochrome = 1;
      params->format = DEC_OUT_FRM_YUV400;
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'G':
      params->ppu_cfg[0].rgb = 1;
      params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;  /* RGB888 as default */
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'h':
      params->ppu_cfg[0].rgb_planar = 1;
      params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;  /* RGB888 as default */
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'a':
      params->ppu_cfg[0].planar = 1;
      params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888_P;
      params->ppu_cfg[0].enabled = 1;
      params->pp_enabled = 1;
      params->format = DEC_OUT_FRM_PLANAR_420;
      break;
    case 'e':
      params->mc_enable = 1;
      break;
    case 'I':
      if (strcmp(optarg, "RGB888") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      else if (strcmp(optarg, "BGR888") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_BGR888;
      else if (strcmp(optarg, "R16G16B16") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_R16G16B16;
      else if (strcmp(optarg, "B16G16R16") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_B16G16R16;
      else if (strcmp(optarg, "ARGB888") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_ARGB888;
      else if (strcmp(optarg, "ABGR888") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_ABGR888;
      else if (strcmp(optarg, "A2R10G10B10") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_A2R10G10B10;
      else if (strcmp(optarg, "A2B10G10R10") == 0)
        params->ppu_cfg[0].rgb_format = DEC_OUT_FRM_A2B10G10R10;
      else
        fprintf(stdout, "Illegal RGB format: %s\n", optarg);

      break;
    case 'o':
      if (strcmp(optarg, "BT601") == 0)
        params->rgb_stan = BT601;
      else if (strcmp(optarg, "BT601_L") == 0)
        params->rgb_stan = BT601_L;
      else if (strcmp(optarg, "BT709") == 0)
        params->rgb_stan = BT709;
      else if (strcmp(optarg, "BT709_L") == 0)
        params->rgb_stan = BT709_L;
      else if (strcmp(optarg, "BT2020") == 0)
        params->rgb_stan = BT2020;
      else if (strcmp(optarg, "BT2020_L") == 0)
        params->rgb_stan = BT2020_L;
      else
        fprintf(stdout, "Illegal parameter\n");
      break;
    case 'H':
      PrintUsage(argv[0], ALLFORMATS);
      /* if there is only printing HELP info argument, Yes, return special value */
      if (argc == 2)
        return 2;

      break;
    case 'J':
      params->mvc = 1;
      break;
    case 0:
      if (strcmp(long_options[option_index].name, "delogo") == 0) {
        delogo = atoi(optarg);
        if (delogo >= 2/*|| delogo < 0 */) {
          fprintf(stderr, "ERROR, Invalid index for option \"--delogo\":%s\n", optarg);
          return 1;
        }
        params->delogo_params[delogo].enabled = 1;
      } else if (strcmp(long_options[option_index].name, "pos") == 0) {
        if (ParsePosParams(optarg, &(params->delogo_params[delogo].x),
                                   &(params->delogo_params[delogo].y),
                                   &(params->delogo_params[delogo].w),
                                   &(params->delogo_params[delogo].h))) {
          fprintf(stderr, "ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "show") == 0) {
        params->delogo_params[delogo].show = atoi(optarg);
      } else if (strcmp(long_options[option_index].name, "mode") == 0) {
        if (strcmp(optarg, "PIXEL_REPLACE") == 0) {
          params->delogo_params[delogo].mode = PIXEL_REPLACE;
        } else if (strcmp(optarg, "PIXEL_INTERPOLATION") == 0) {
          params->delogo_params[delogo].mode = PIXEL_INTERPOLATION;
          if (delogo == 1) {
            fprintf(stderr, "ERROR, delogo params 1 not support PIXEL_INTERPOLATION\n");
            return 1;
          }
        } else {
          fprintf(stderr, "ERROR, no supported mode\n");
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "YUV") == 0) {
        if (ParsePixelParams(optarg, &(params->delogo_params[delogo].Y),
                                     &(params->delogo_params[delogo].U),
                                     &(params->delogo_params[delogo].V))) {
          fprintf(stderr, "ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "full-only") == 0) {
        params->only_full_resolution = 1;
      } else if (strcmp(long_options[option_index].name, "ri_mc_enable") == 0) {
        params->ri_mc_enable = 1;
      } else if (strcmp(long_options[option_index].name, "instant_buffer") == 0) {
        params->instant_buffer = 1;
      } else if(strcmp(long_options[option_index].name, "multimode") == 0) {
        params->multimode = atoi(optarg); 
      } else if(strcmp(long_options[option_index].name, "streamcfg") == 0) {
        params->streamcfg[params->nstream++] = optarg;
      } else {
       fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      }
      break;
    case 1:
      second_crop = 1;
      params->ppu_cfg[0].crop2.enabled = 1;
      params->ppu_cfg[0].crop2.x = params->ppu_cfg[0].crop.x;
      params->ppu_cfg[0].crop2.y = params->ppu_cfg[0].crop.y;
      params->ppu_cfg[0].crop2.width = params->ppu_cfg[0].crop.width;
      params->ppu_cfg[0].crop2.height = params->ppu_cfg[0].crop.height;
      params->ppu_cfg[0].crop.x = params->ppu_cfg[0].crop.y =
      params->ppu_cfg[0].crop.width = params->ppu_cfg[0].crop.height = 0;
      break;
    case '?':
      if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      break;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "Invalid or no input file specified\n");
    return 1;
  }

  if (params->ppu_cfg[0].scale.enabled &&
      params->fscale_cfg.fixed_scale_enabled) {
    /* Scaling option -D will override -d */
    params->fscale_cfg.fixed_scale_enabled = 0;
  }

  if (params->pp_enabled && !params->ppu_cfg[0].scale.enabled) {
    if (!params->fscale_cfg.fixed_scale_enabled) {
      params->fscale_cfg.fixed_scale_enabled = 1;
      params->fscale_cfg.down_scale_x = 1;
      params->fscale_cfg.down_scale_y = 1;
    }
  }

  if (flag_T || flag_P || flag_S)
    params->out_format_conv = 1;

#if 0
  if (flag_d && params->display_cropped) {
    fprintf(stderr,
            "ERROR: options -C and -d are mutually "
            "exclusive!\n");
    return 1;
  }
#endif
  if ((params->ppu_cfg[0].out_p010 &&
      (params->ppu_cfg[0].out_cut_8bits || params->ppu_cfg[0].out_be)) ||
      (params->ppu_cfg[0].out_cut_8bits && params->ppu_cfg[0].out_be)) {
    fprintf(stderr,
            "ERROR: options -f or -Epbe and -Ep010 are mutually "
            "exclusive!\n");
    return 1;
  }
  if (params->ppu_cfg[0].out_1010 && (params->ppu_cfg[0].out_p010 ||
      params->ppu_cfg[0].out_cut_8bits || params->ppu_cfg[0].out_be)) {
    fprintf(stderr,
            "ERROR: options -E1010 -f or -Epbe and -Ep010 are mutually "
            "exclusive!\n");
    return 1;
  }
  if (params->ppu_cfg[0].out_I010 && (params->ppu_cfg[0].out_1010 ||
      params->ppu_cfg[0].out_p010 || params->ppu_cfg[0].out_L010 ||
      params->ppu_cfg[0].out_cut_8bits || params->ppu_cfg[0].out_be)) {
    fprintf(stderr,
            "ERROR: options -EI010 -E1010 -EL010 -f or -Epbe and -Ep010 are mutually "
            "exclusive!\n");
    return 1;
  }
  if (params->ppu_cfg[0].out_L010 && (params->ppu_cfg[0].out_1010 ||
      params->ppu_cfg[0].out_p010 || params->ppu_cfg[0].out_I010 ||
      params->ppu_cfg[0].out_cut_8bits || params->ppu_cfg[0].out_be)) {
    fprintf(stderr,
            "ERROR: options -EL010 -EI010 -E1010 -f or -Epbe and -Ep010 are mutually "
            "exclusive!\n");
    return 1;
  }
  if ((params->ppu_cfg[0].rgb || params->ppu_cfg[0].rgb_planar) && (params->ppu_cfg[0].planar || params->ppu_cfg[0].tiled_e)) {
    fprintf(stderr,
            "ERROR: options --pp_rgb or --pp_rgb_planar and --pp-planar or --pp-tiled-out are mutually "
            "exclusive!\n");
    return 1;
  }
  if (!flag_T && !flag_P && params->pp_enabled &&
      !params->ppu_cfg[0].tiled_e && !params->ppu_cfg[0].planar &&
      !params->ppu_cfg[0].monochrome)
    params->format = DEC_OUT_FRM_RASTER_SCAN;

  params->in_file_name = argv[optind];
  if (ResolveOverlap(params)) return 1;

  (void)flag_b;
  return 0;
}

int ResolveOverlap(struct TestParams* params) {
  if (params->mc_enable && params->read_mode != STREAMREADMODE_FRAME) {
    fprintf(stderr,
            "Overriding read_mode to FRAME mode "
            "when multicore decoding (--mc) is enabled.\n");
    params->read_mode = STREAMREADMODE_FRAME;
  }

  if (params->format == DEC_OUT_FRM_TILED_4X4 &&
      params->hw_format == DEC_OUT_FRM_RASTER_SCAN) {
    fprintf(stderr,
            "Overriding hw_format to tiled 4x4 as the requested "
            "output is tiled 4x4. (i.e. '-Ers' or '-Ep010' option ignored)\n");
    params->hw_format = DEC_OUT_FRM_TILED_4X4;
  }

  if (params->format != DEC_OUT_FRM_TILED_4X4 &&
      params->hw_format == DEC_OUT_FRM_TILED_4X4 &&
      !params->compress_bypass && !params->pp_enabled) {
    fprintf(stderr,
            "Disable conversion from compressed tiled 4x4 to Semi_Planar/Planar "
            "when hw_format is tiled 4x4 and compression is enabled.\n\n");
    params->format = DEC_OUT_FRM_TILED_4X4;
  }

  return 0;
}

void ResolvePpParamsOverlap(struct TestParams* params,
                           struct TBPpUnitParams *pp_units_params,
                           u32 standalone) {
  /* Override PPU1-3 parameters with tb.cfg */
  u32 i, pp_enabled;

  /* Either fixed ration scaling, or fixed size scaling. */
  if (params->ppu_cfg[0].enabled &&
      !params->ppu_cfg[0].scale.enabled &&
      !params->fscale_cfg.fixed_scale_enabled) {
    params->fscale_cfg.fixed_scale_enabled = 1;
    params->fscale_cfg.down_scale_x = 1;
    params->fscale_cfg.down_scale_y = 1;
  }

  if (params->ppu_cfg[0].rgb || params->ppu_cfg[0].rgb_planar) {
    params->ppu_cfg[0].video_range = params->video_range;
//    params->ppu_cfg[0].rgb_format = params->rgb_format;
    params->ppu_cfg[0].rgb_stan = params->rgb_stan;
  }
  for (i = 1; i < DEC_MAX_OUT_COUNT; i++) {
    params->ppu_cfg[i].enabled = pp_units_params[i].unit_enabled;
    params->ppu_cfg[i].cr_first = pp_units_params[i].cr_first;
    params->ppu_cfg[i].tiled_e = pp_units_params[i].tiled_e;
    params->ppu_cfg[i].crop.enabled = pp_units_params[i].unit_enabled;
    params->ppu_cfg[i].crop.set_by_user |= pp_units_params[i].crop_width ? 1: 0;
    params->ppu_cfg[i].crop.x = pp_units_params[i].crop_x;
    params->ppu_cfg[i].crop.y = pp_units_params[i].crop_y;
    params->ppu_cfg[i].crop.width = pp_units_params[i].crop_width;
    params->ppu_cfg[i].crop.height = pp_units_params[i].crop_height;
    params->ppu_cfg[i].crop2.x = pp_units_params[i].crop2_x;
    params->ppu_cfg[i].crop2.y = pp_units_params[i].crop2_y;
    params->ppu_cfg[i].crop2.width = pp_units_params[i].crop2_width;
    params->ppu_cfg[i].crop2.height = pp_units_params[i].crop2_height;
    if (params->ppu_cfg[i].crop2.width && params->ppu_cfg[i].crop2.height) {
      params->ppu_cfg[i].crop.x = params->ppu_cfg[i].crop.y =
      params->ppu_cfg[i].crop.width = params->ppu_cfg[i].crop.height = 0;
      params->ppu_cfg[i].crop2.enabled = 1;
    }
    params->ppu_cfg[i].scale.enabled = pp_units_params[i].unit_enabled;
    params->ppu_cfg[i].scale.set_by_user |= pp_units_params[i].scale_width ? 1: 0;
    params->ppu_cfg[i].scale.width = pp_units_params[i].scale_width;
    params->ppu_cfg[i].scale.height = pp_units_params[i].scale_height;
    params->ppu_cfg[i].shaper_enabled = pp_units_params[i].shaper_enabled;
    params->ppu_cfg[i].monochrome = pp_units_params[i].monochrome;
    params->ppu_cfg[i].planar = pp_units_params[i].planar;
    params->ppu_cfg[i].out_p010 = pp_units_params[i].out_p010;
    params->ppu_cfg[i].out_1010 = pp_units_params[i].out_1010;
    params->ppu_cfg[i].out_I010 = pp_units_params[i].out_I010;
    params->ppu_cfg[i].out_L010 = pp_units_params[i].out_L010;
    params->ppu_cfg[i].out_cut_8bits = pp_units_params[i].out_cut_8bits;
    params->ppu_cfg[i].align = params->align;
    params->ppu_cfg[i].ystride = pp_units_params[i].ystride;
    params->ppu_cfg[i].cstride = pp_units_params[i].cstride;
    params->ppu_cfg[i].rgb = pp_units_params[i].rgb;
    params->ppu_cfg[i].rgb_planar = pp_units_params[i].rgb_planar;
    params->ppu_cfg[i].video_range = params->video_range;
    params->ppu_cfg[i].rgb_stan = params->rgb_stan;
  }
  if (!(params->ppu_cfg[0].enabled || params->ppu_cfg[1].enabled ||
       params->ppu_cfg[2].enabled || params->ppu_cfg[3].enabled) &&
       params->decoder_mode == DEC_PARTIAL_DECODING) {
       params->ppu_cfg[0].enabled = 1;
       params->pp_enabled = 1;
   }
   if (params->ppu_cfg[0].enabled) {
     /* PPU0 */
     params->ppu_cfg[0].align = params->align;
   }
#if 0
  if (params->ppu_cfg[0].enabled) {
    /* PPU0 */
    params->ppu_cfg[0].align = params->align;
    params->ppu_cfg[0].enabled |= pp_units_params[0].unit_enabled;
    params->ppu_cfg[0].cr_first |= pp_units_params[0].cr_first;
    if (params->hw_format != DEC_OUT_FRM_RASTER_SCAN)
      params->ppu_cfg[0].tiled_e |= pp_units_params[0].tiled_e;
    params->ppu_cfg[0].planar |= pp_units_params[0].planar;
    params->ppu_cfg[0].rgb |= pp_units_params[0].rgb;
    params->ppu_cfg[0].out_p010 |= pp_units_params[0].out_p010;
    params->ppu_cfg[0].out_I010 |= pp_units_params[0].out_I010;
    params->ppu_cfg[0].out_L010 |= pp_units_params[0].out_L010;
    params->ppu_cfg[0].out_1010 |= pp_units_params[0].out_1010;
    params->ppu_cfg[0].out_cut_8bits |= pp_units_params[0].out_cut_8bits;
    if (!params->ppu_cfg[0].crop.enabled && pp_units_params[0].unit_enabled) {
      params->ppu_cfg[0].crop.set_by_user = 1;
      params->ppu_cfg[0].crop.x = pp_units_params[0].crop_x;
      params->ppu_cfg[0].crop.y = pp_units_params[0].crop_y;
      params->ppu_cfg[0].crop.width = pp_units_params[0].crop_width;
      params->ppu_cfg[0].crop.height = pp_units_params[0].crop_height;
    }
    if (params->ppu_cfg[0].crop.width || params->ppu_cfg[0].crop.height)
      params->ppu_cfg[0].crop.enabled = 1;
    if (!params->ppu_cfg[0].scale.enabled && pp_units_params[0].unit_enabled) {
      params->ppu_cfg[0].scale.width = pp_units_params[0].scale_width;
      params->ppu_cfg[0].scale.height = pp_units_params[0].scale_height;
      params->ppu_cfg[0].scale.set_by_user = 1;
      params->ppu_cfg[0].scale.ratio_x = 0;
      params->ppu_cfg[0].scale.ratio_y = 0;
    }
#if 0
    /* when '-d' enabled, ignore scaling in tb.cfg. Set the scaling w/h in
       function CheckPpUnitConfig() after we get stream pic info. */
    if (params->fscale_cfg.fixed_scale_enabled == 1) {
      params->ppu_cfg[0].scale.width = 0;
      params->ppu_cfg[0].scale.height = 0;
      params->ppu_cfg[0].scale.set_by_user = 1;
      params->ppu_cfg[0].scale.ratio_x = params->fscale_cfg.down_scale_x;
      params->ppu_cfg[0].scale.ratio_y = params->fscale_cfg.down_scale_y;
    }
#endif
    if (params->ppu_cfg[0].scale.width || params->ppu_cfg[0].scale.height)
      params->ppu_cfg[0].scale.enabled = 1;
    params->ppu_cfg[0].shaper_enabled |= pp_units_params[0].shaper_enabled;
    params->ppu_cfg[0].monochrome |= pp_units_params[0].monochrome;
    params->ppu_cfg[0].align = params->align;
    if (!params->ppu_cfg[0].ystride)
      params->ppu_cfg[0].ystride = pp_units_params[0].ystride;
    if (!params->ppu_cfg[0].cstride)
      params->ppu_cfg[0].cstride = pp_units_params[0].cstride;
  }
#endif
  pp_enabled = pp_units_params[0].unit_enabled ||
               pp_units_params[1].unit_enabled ||
               pp_units_params[2].unit_enabled ||
               pp_units_params[3].unit_enabled ||
               pp_units_params[4].unit_enabled;
  if (pp_enabled)
    params->pp_enabled = 1;
  if (standalone) { /* pp standalone mode */
    params->pp_standalone = 1;
    if (!params->pp_enabled &&
        !params->fscale_cfg.fixed_scale_enabled) {
      /* No pp enabled explicitly, then enable fixed ratio pp (1:1) */
      params->fscale_cfg.down_scale_x = 1;
      params->fscale_cfg.down_scale_y = 1;
      params->fscale_cfg.fixed_scale_enabled = 1;
      params->pp_enabled = 1;
    }
  }
  /* display_cropped is for uncompressed tile4x4 reference output only. */
  if (params->pp_enabled)
    params->display_cropped = 0;
}
