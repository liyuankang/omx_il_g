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

#include "basetype.h"
#include "ivf.h"

#include "vp8decapi.h"
#include "dwl.h"
#include <unistd.h>

#include "vp8hwd_container.h"

#include "vp8filereader.h"
#ifdef USE_EFENCE
#include "efence.h"
#endif

#include "regdrv.h"
#include "tb_cfg.h"
#include "tb_tiled.h"
#include "deccfg.h"

#include "md5.h"
#include "tb_md5.h"

#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "command_line_parser.h"

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vp8filereader.h"
#include "tb_out.h"

struct TBCfg tb_cfg;

u32 number_of_written_frames = 0;
u32 num_frame_buffers = 0;
u32 pp_tile_out = 0;    /* PP tiled output */
u32 pp_planar_out = 0;
/* SW/SW testing, read stream trace file */
FILE * f_stream_trace = NULL;

u32 disable_output_writing = 0;
u32 display_order = 0;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 output_format = DEC_X170_OUTPUT_FORMAT;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
u32 planar_output = 0;
u32 height_crop = 0;
u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 convert_tiled_output = 0;

u32 use_peek_output = 0;
u32 snap_shot = 0;
u32 forced_slice_mode = 0;

u32 stream_packet_loss = 0;
u32 stream_truncate = 0;
u32 stream_header_corrupt = 0;
u32 stream_bit_swap = 0;
u32 hdrs_rdy = 0;
u32 pic_rdy = 0;

u8 *p_frame_pic[4] = {NULL,NULL,NULL,NULL};
u32 include_strides = 0;
i32 extra_strm_buffer_bits = 0; /* For HW bug workaround */

u32 user_mem_alloc = 0;
u32 alternate_alloc_order = 0;
u32 interleaved_user_buffers = 0;
struct DWLLinearMem user_alloc_luma[16];
struct DWLLinearMem user_alloc_chroma[16];
VP8DecPictureBufferProperties pbp = { 0 };
u32 ds_ratio_x, ds_ratio_y;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 cr_first = 0;
u32 pp_enabled = 0;
u32 ystride = 0;
u32 cstride = 0;
/* user input arguments */
u32 scale_enabled = 0;
u32 scaled_w, scaled_h;
u32 crop_enabled = 0;
u32 crop_x = 0;
u32 crop_y = 0;
u32 crop_w = 0;
u32 crop_h = 0;
enum SCALE_MODE scale_mode;

u8 *pic_big_endian = NULL;
size_t pic_big_endian_size = 0;
char out_file_name[DEC_MAX_PPU_COUNT][256] = {"", "", "", ""};
void decsw_performance(void);

void writeSlice(FILE * fp, VP8DecPicture *dec_pic);

void VP8DecTrace(const char *string) {
  printf("%s\n", string);
}

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#define DEBUG_PRINT(str) printf str

#define VP8_MAX_STREAM_SIZE  DEC_X170_MAX_STREAM_VC8000D>>1

#define MAX_BUFFERS 16

#ifdef ADS_PERFORMANCE_SIMULATION
include_strides
volatile u32 tttttt = 0;

void trace_perf() {
  tttttt++;
}

#undef START_SW_PERFORMANCE
#undef END_SW_PERFORMANCE

#define START_SW_PERFORMANCE trace_perf();
#define END_SW_PERFORMANCE trace_perf();

#endif

VP8DecInst dec_inst;
const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 num_buffers;  /* external buffers allocated yet. */
u32 add_buffer_thread_run = 0;
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS];

u32 add_extra_flag = 0;
/* Fixme: this value should be set based on option "-d" when invoking testbench. */

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && (num_buffers < MAX_BUFFERS)) {
      struct DWLLinearMem mem;
      i32 dwl_ret;
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        VP8DecRet rv = VP8DecAddBuffer(dec_inst, &mem);
        if(rv != VP8DEC_OK && rv != VP8DEC_WAITING_FOR_BUFFER) {
          if (pp_enabled)
            DWLFreeLinear(dwl_inst, &mem);
          else
            DWLFreeRefFrm(dwl_inst, &mem);
        } else {
          ext_buffers[num_buffers++] = mem;
        }
      }
    }
    pthread_mutex_unlock(&ext_buffer_contro);
    sched_yield();
  }
  return NULL;
}

void ReleaseExtBuffers() {
  int i;
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    DEBUG_PRINT(("Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address));
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

FILE *fout[4] = {NULL, NULL, NULL, NULL};
u32 md5sum = 0; /* flag to enable md5sum output */
u32 slice_mode = 0;
pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;

sem_t buf_release_sem;
VP8DecPicture buf_list[100];
u32 buf_status[100] = {0};
u32 list_pop_index = 0;
u32 list_push_index = 0;
u32 last_pic_flag = 0;

/* buf release thread entry point. */
static void* buf_release_thread(void* arg) {
  while(1) {
    /* Pop output buffer from buf_list and consume it */
    if(buf_status[list_pop_index]) {
      sem_wait(&buf_release_sem);
      VP8DecPictureConsumed(dec_inst, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && num_buffers < 16) {
          struct DWLLinearMem mem;
          i32 dwl_ret;
          if (pp_enabled)
            dwl_ret  = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          else
            dwl_ret  = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          if(dwl_ret == DWL_OK) {
            VP8DecRet rv = VP8DecAddBuffer(dec_inst, &mem);
            if(rv != VP8DEC_OK && rv != VP8DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              ext_buffers[num_buffers++] = mem;
            }
          }
        }
        pthread_mutex_unlock(&ext_buffer_contro);
      }
    }
    if(last_pic_flag && buf_status[list_pop_index] == 0)
      break;
    usleep(10000);
  }
  return (NULL);
}

/* Output thread entry point. */
static void* vp8_output_thread(void* arg) {
  VP8DecPicture dec_picture;
  u32 pic_display_number = 1;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;

#ifdef TESTBENCH_WATCHDOG
  /* fpga watchdog: 30 sec timer for frozen decoder */
  {
    struct itimerval t = {{30,0}, {30,0}};
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = watchdog1;
    sa.sa_flags |= SA_RESTART;  /* restart of system calls */
    sigaction(SIGALRM, &sa, NULL);

    setitimer(ITIMER_REAL, &t, NULL);
  }
#endif
  while(output_thread_run) {
    VP8DecRet ret;
    u32 i;
    ret = VP8DecNextPicture(dec_inst, &dec_picture, 0);
    if(ret == VP8DEC_PIC_RDY) {
      for (i = 0; i < 4; i++) {
        if (!dec_picture.pictures[i].frame_width ||
            !dec_picture.pictures[i].frame_height)
          continue;
        if(!use_peek_output) {
          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(dec_picture.pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(dec_picture.pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(dec_picture.pictures[i].output_format))
            pp_mono_chrome = 1;
          if (dec_picture.num_slice_rows) {
            writeSlice(fout[0], &dec_picture);
          } else {
            DEBUG_PRINT(("WRITING PICTURE %d\n", pic_display_number));
            if (!IS_PIC_PACKED_RGB(dec_picture.pictures[i].output_format) &&
                !IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format))
              WriteOutput(out_file_name[i], (unsigned char *) dec_picture.pictures[i].p_output_frame,
                          dec_picture.pictures[i].coded_width,
                          dec_picture.pictures[i].coded_height,
                          pic_display_number,
                          pp_mono_chrome, 0, 0, 0,
                          IS_PIC_TILE(dec_picture.pictures[i].output_format),
                          dec_picture.pictures[i].pic_stride, dec_picture.pictures[i].pic_stride_ch, i,
                          pp_planar, pp_cr_first, convert_tiled_output,
                          0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
            else
              WriteOutputRGB(out_file_name[i], (unsigned char *) dec_picture.pictures[i].p_output_frame,
                             dec_picture.pictures[i].coded_width,
                             dec_picture.pictures[i].coded_height,
                             pic_display_number,
                             pp_mono_chrome, 0, 0, 0,
                             IS_PIC_TILE(dec_picture.pictures[i].output_format),
                             dec_picture.pictures[i].pic_stride, dec_picture.pictures[i].pic_stride_ch, i,
                             IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format), pp_cr_first,
                             convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
          }
          if(dec_picture.num_slice_rows)
            slice_mode = 1;
        }
      }

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = dec_picture;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);

      pic_display_number++;
    }

    else if(ret == VP8DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
    usleep(10000);
  }
  return (NULL);
}

/*------------------------------------------------------------------------------
    Local functions
------------------------------------------------------------------------------*/
static u32 FfReadFrame( reader_inst reader, const u8 *buffer,
                        u32 max_buffer_size, u32 *frame_size, u32 pedantic );

/*------------------------------------------------------------------------------

    main

        Main function

------------------------------------------------------------------------------*/
int main(int argc, char**argv) {
  reader_inst reader;
  u32 i, j, tmp;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;
  u32 size;
  u32 more_frames = 0;
  u32 pedantic_reading = 1;
  u32 max_num_pics = 0;
  u32 pic_decode_number = 0;
  struct DWLLinearMem stream_mem;
  u32 stream_len = 10*1024; /* initial input stream buffer size: 10KB */
  DWLHwConfig hw_config;
  VP8DecRet ret;
  VP8DecFormat dec_format = 0;
  VP8DecInput dec_input;
  VP8DecOutput dec_output;
  VP8DecPicture dec_picture;
  VP8DecInfo info;
  struct VP8DecConfig dec_cfg;
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  VP8DecBufferInfo hbuf;
  VP8DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP8_DEC;
  u32 webp_loop = 0;
  FILE *f_tbcfg;
  u32 seed_rnd = 0;
  struct TestParams params;

  SetupDefaultParams(&params);

#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 10000; /* default to g1 mode */
#endif

#ifdef VP8_EVALUATION_G1
  g_hw_ver = 10000;
#endif

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */

  /* expiry stuff */
  {
    char tm_buf[7];
    time_t sys_time;
    struct tm * tm;
    u32 tmp1;

    /* Check expiry date */
    time(&sys_time);
    tm = localtime(&sys_time);
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000+atoi(tm_buf);
    if (tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1 ) {
      fprintf(stderr,
              "EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
      return -1;
    }
  }

  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;
  dec_picture.num_slice_rows = 0;

  if (argc < 2) {
    printf("Usage: %s [options] file.ivf\n", argv[0]);
    printf("\t-Nn forces decoding to stop after n pictures\n");
    printf("\t-Ooutfile write output to \"outfile\" (default out.yuv)\n");
    printf("\t-m Output md5 for each picture. No YUV output!(--md5-per-pic)\n");
    printf("\t-C display cropped image\n");
    printf("\t-a Enable PP planar output.(--pp-planar)\n");
    printf("\t-E use tiled reference frame format.\n");
    printf("\t-G convert tiled output pictures to raster scan\n");
    printf("\t-Bn to use n frame buffers in decoder\n");
    printf("\t-Z output pictures using VP8DecPeek() function\n");
    printf("\t-S Set decode format to webp\n");
    printf("\t-X user allocates picture buffers\n");
    printf("\t-Xa same as above but alternate allocation order\n");
    printf("\t-xn Add n bytes of extra space after "\
           "stream buffer for decoder\n");
    printf("\t--cache_enable Enable cache rtl simulation(external hw IP)\n");
    printf("\t--shaper_enable Enable shaper rtl simulation(external hw IP)\n");
    printf("\t--shaper_bypass Enable shaper bypass rtl simulation(external hw IP)\n");
    printf("\t-e add extra external buffer randomly\n");
    printf("\t-k add extra external buffer in ouput thread\n");
    printf("\t-An Set stride aligned to n bytes (valid value: 1/8/16/32/64/128/256/512/1024/2048)\n");
    printf("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g.,\n");
    printf("\t  -d2 -- down scale to 1/2 in both directions\n");
    printf("\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
    printf("\t--cr-first PP outputs chroma in CrCb order.\n");
    printf("\t-C[xywh]NNN Cropping parameters. E.g.,\n");
    printf("\t  -Cx8 -Cy16        Crop from (8, 16)\n");
    printf("\t  -Cw720 -Ch480     Crop size  720x480\n");
    printf("\t-Dwxh  PP output size wxh. E.g.,\n");
    printf("\t  -D1280x720        PP output size 1280x720\n");
    printf("\t-s[yc]NNN Set stride for y/c plane directly. \n");
    printf("\t   sw check the validation of the value. E.g.,\n");
    printf("\t  -sy720 -sc360     set y, c stride to 720, 360\n");
    printf("\t--pp-rgb Enable Yuv2RGB.\n");
    printf("\t--pp-rgb-planar Enable planar Yuv2RGB.\n");
    printf("\t--rgb-fmat set the RGB output format.\n");
    printf("\t--rgb-std set the standard coeff to do Yuv2RGB.\n\n");

    return 0;
  }

  /* read command line arguments */
  for (i = 1; i < (u32)(argc-1); i++) {
    if (strncmp(argv[i], "-N", 2) == 0)
      max_num_pics = (u32)atoi(argv[i] + 2);
#if 0
    else if (strncmp(argv[i], "-P", 2) == 0 ||
             strcmp(argv[i], "--planar") == 0)
      planar_output = 1;
#endif
    else if (strncmp(argv[i], "-a", 2) == 0 ||
               strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = 1;
      pp_enabled = 1;
    } else if (strncmp(argv[i], "-E", 2) == 0)
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    else if (strncmp(argv[i], "-G", 2) == 0)
      convert_tiled_output = 1;
#if 0
    else if (strncmp(argv[i], "-C", 2) == 0)
      height_crop = 1;
#endif
    else if (strncmp(argv[i], "-O", 2) == 0)
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(out_file_name[j], argv[i] + 2);
    else if(strncmp(argv[i], "-B", 2) == 0) {
      num_frame_buffers = atoi(argv[i] + 2);
      if(num_frame_buffers > 16)
        num_frame_buffers = 16;
    } else if (strncmp(argv[i], "-Z", 2) == 0)
      use_peek_output = 1;
    else if (strncmp(argv[i], "-S", 2) == 0)
      snap_shot = 1;
    else if (strncmp(argv[i], "-x", 2) == 0) {
      extra_strm_buffer_bits = atoi(argv[i] + 2);
    } else if (strncmp(argv[i], "-X", 2) == 0) {
      user_mem_alloc = 1;
      if( strncmp(argv[i], "-Xa", 3) == 0)
        alternate_alloc_order = 1;
#if 0
    } else if (strncmp(argv[i], "-I", 2) == 0)
      interleaved_user_buffers = 1;
    else if (strncmp(argv[i], "-R", 2) == 0)
      include_strides = 1;
#endif
    } else if (strncmp(argv[i], "-m", 2) == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
    }
    else if(strcmp(argv[i], "-e") == 0) {
      use_extra_buffers = 1;
    }
    else if(strcmp(argv[i], "-k") == 0) {
      use_extra_buffers = 0;
      allocate_extra_buffers_in_output = 1;
    }
#ifdef SUPPORT_CACHE
    else if(strcmp(argv[i], "--cache_enable") == 0) {
      tb_cfg.cache_enable = 1;
    } else if(strcmp(argv[i], "--shaper_enable") == 0) {
      tb_cfg.shaper_enable = 1;
    } else if(strcmp(argv[i], "--shaper_bypass") == 0) {
      tb_cfg.shaper_bypass = 1;
    } else if(strcmp(argv[i], "--pp-shaper") == 0) {
      params.ppu_cfg[0].shaper_enabled = 1;
    }
#endif
    else if (strncmp(argv[i], "-d", 2) == 0) {
      pp_enabled = 1;
      scale_enabled = 1;
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
      scale_mode = FIXED_DOWNSCALE;
      params.fscale_cfg.fixed_scale_enabled = 1;
      params.fscale_cfg.down_scale_x = ds_ratio_x;
      params.fscale_cfg.down_scale_y = ds_ratio_y;
    } else if (strncmp(argv[i], "-D", 2) == 0) {
      char *px = strchr(argv[i]+2, 'x');
      if (!px) {
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable scaler parameter by using: -D[w]x[h]\n");
        return 1;
      }
      *px = '\0';
      scale_enabled = 1;
      pp_enabled = 1;
      scaled_w = atoi(argv[i]+2);
      scaled_h = atoi(px+1);
      if (scaled_w == 0 || scaled_h == 0) {
        fprintf(stdout, "Illegal scaled width/height: %sx%s\n", argv[i]+2, px+1);
        return 1;
      }
      scale_mode = FLEXIBLE_SCALE;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].scale.enabled = 1;
      params.ppu_cfg[0].scale.width = scaled_w;
      params.ppu_cfg[0].scale.height = scaled_h;
    } else if (strncmp(argv[i], "-C", 2) == 0) {
      crop_enabled = 1;
      pp_enabled = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].crop.enabled = 1;
      params.ppu_cfg[0].crop.set_by_user = 1;
      switch (argv[i][2]) {
      case 'x':
        crop_x = atoi(argv[i]+3);
        params.ppu_cfg[0].crop.x = crop_x;
        break;
      case 'y':
        crop_y = atoi(argv[i]+3);
        params.ppu_cfg[0].crop.y = crop_y;
        break;
      case 'w':
        crop_w = atoi(argv[i]+3);
        params.ppu_cfg[0].crop.width = crop_w;
        break;
      case 'h':
        crop_h = atoi(argv[i]+3);
        params.ppu_cfg[0].crop.height = crop_h;
        break;
      default:
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,\n");
        fprintf(stdout, "\t -CxXXX -CyYYY     Crop from (XXX, YYY)\n");
        fprintf(stdout, "\t -CwWWW -ChHHH     Crop size  WWWxHHH\n");
        return 1;
      }
    } else if (strncmp(argv[i], "-s", 2) == 0) {
      pp_enabled = 1;
      switch (argv[i][2]) {
      case 'y':
        ystride = atoi(argv[i]+3);
        break;
      case 'c':
        cstride = atoi(argv[i]+3);
        break;
      default:
        fprintf(stdout, "ERROR: Enable out stride parameter by using: -s[yc]NNN. E.g.,\n");
        return 1;
      }
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
      cr_first = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].cr_first = 1;
     } else if (strcmp(argv[i], "--pp-luma-only") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].monochrome = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "-U") == 0) {
      pp_tile_out = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].tiled_e = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "--pp-rgb") == 0) {
      pp_enabled = 1;
      params.ppu_cfg[0].rgb = 1;
      params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    } else if (strcmp(argv[i], "--pp-rgb-planar") == 0) {
      pp_enabled = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      params.ppu_cfg[0].rgb_planar = 1;
      params.pp_enabled = 1;
    } else if (strncmp(argv[i], "--rgb-fmat=", 11) == 0) {
      if (strcmp(argv[i]+11, "RGB888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888;
      else if (strcmp(argv[i]+11, "BGR888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_BGR888;
      else if (strcmp(argv[i]+11, "R16G16B16") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_R16G16B16;
      else if (strcmp(argv[i]+11, "B16G16R16") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_B16G16R16;
      else if (strcmp(argv[i]+11, "ARGB888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_ARGB888;
      else if (strcmp(argv[i]+11, "ABGR888") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_ABGR888;
      else if (strcmp(argv[i]+11, "A2R10G10B10") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_A2R10G10B10;
      else if (strcmp(argv[i]+11, "A2B10G10R10") == 0)
        params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_A2B10G10R10;
      else {
        fprintf(stdout, "Illegal parameter\n");
        return 1;
      }
    } else if (strncmp(argv[i], "--rgb-std=", 10) == 0) {
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
      else {
        fprintf(stdout, "Illegal parameter\n");
        return 1;
      }
    } else {
      DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
      return 1;
    }
  }

  reader = rdr_open(argv[argc-1]);
  if (reader == NULL) {
    fprintf(stderr, "Unable to open input file\n");
    exit(100);
  }

  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if (f_tbcfg == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN TEST BENCH CONFIGURATION FILE: \"tb.cfg\"\n"));
    DEBUG_PRINT(("USING DEFAULT CONFIGURATION\n"));
  } else {
    fclose(f_tbcfg);
    if (TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if (TBCheckCfg(&tb_cfg) != 0)
      return -1;
  }
  user_mem_alloc |= TBGetDecMemoryAllocation(&tb_cfg);
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  output_format = TBGetDecOutputFormat(&tb_cfg);
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  DEBUG_PRINT(("Decoder Clock Gating %d\n", clock_gating));
  DEBUG_PRINT(("Decoder Data Discard %d\n", data_discard));
  DEBUG_PRINT(("Decoder Latency Compensation %d\n", latency_comp));
  DEBUG_PRINT(("Decoder Output Picture Endian %d\n", output_picture_endian));
  DEBUG_PRINT(("Decoder Bus Burst Length %d\n", bus_burst_length));
  DEBUG_PRINT(("Decoder Asic Service Priority %d\n", asic_service_priority));
  DEBUG_PRINT(("Decoder Output Format %d\n", output_format));
  seed_rnd = tb_cfg.tb_params.seed_rnd;
  stream_header_corrupt = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  /* if headers are to be corrupted
    -> do not wait the picture to readeralize before starting stream corruption */
  if (stream_header_corrupt)
    pic_rdy = 1;
  stream_truncate = TBGetTBStreamTruncate(&tb_cfg);
  if (strcmp(tb_cfg.tb_params.stream_bit_swap, "0") != 0) {
    stream_bit_swap = 1;
  } else {
    stream_bit_swap = 0;
  }
  if (strcmp(tb_cfg.tb_params.stream_packet_loss, "0") != 0) {
    stream_packet_loss = 1;
  } else {
    stream_packet_loss = 0;
  }
  DEBUG_PRINT(("TB Seed Rnd %d\n", seed_rnd));
  DEBUG_PRINT(("TB Stream Truncate %d\n", stream_truncate));
  DEBUG_PRINT(("TB Stream Header Corrupt %d\n", stream_header_corrupt));
  DEBUG_PRINT(("TB Stream Bit Swap %d; odds %s\n",
               stream_bit_swap, tb_cfg.tb_params.stream_bit_swap));
  DEBUG_PRINT(("TB Stream Packet Loss %d; odds %s\n",
               stream_packet_loss, tb_cfg.tb_params.stream_packet_loss));
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
    if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
      tb_cfg.dec_params.cache_support = 1;
    else
      tb_cfg.dec_params.cache_support = 0;
#endif
#endif
  INIT_SW_PERFORMANCE;

  {
    VP8DecApiVersion dec_api;
    VP8DecBuild dec_build;

    /* Print API version number */
    dec_api = VP8DecGetAPIVersion();
    dec_build = VP8DecGetBuild();
    DWLReadAsicConfig(&hw_config, DWL_CLIENT_TYPE_VP8_DEC);
    DEBUG_PRINT((
                  "\n8170 VP8 Decoder API v%d.%d - SW build: %d - HW build: %x\n",
                  dec_api.major, dec_api.minor, dec_build.sw_build, dec_build.hw_build));
    DEBUG_PRINT((
                  "HW Supports video decoding up to %d pixels,\n",
                  hw_config.max_dec_pic_width));

    DEBUG_PRINT((
                  "Supported codecs: %s%s\n",
                  hw_config.vp7_support ? "VP-7 " : "",
                  hw_config.vp8_support ? "VP-8" : ""));

    if(hw_config.pp_support)
      DEBUG_PRINT((
                    "Maximum Post-processor output size %d pixels\n\n",
                    hw_config.max_pp_out_pic_width));
    else
      DEBUG_PRINT(("Post-Processor not supported\n\n"));
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if (!tmp) {
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
  }
#endif
  /* check format */
  switch (rdr_identify_format(reader)) {
  case G1_BITSTREAM_VP7:
    dec_format = VP8DEC_VP7;
    break;
  case G1_BITSTREAM_VP8:
    dec_format = VP8DEC_VP8;
    break;
  case G1_BITSTREAM_WEBP:
    dec_format = VP8DEC_WEBP;
    pedantic_reading = 0;  /* With WebP we rely on non-pedantic reading
                                      mode for correct operation. */
    break;
  }
  if (snap_shot)
    dec_format = VP8DEC_WEBP;
  if (dec_format == VP8DEC_WEBP)
    snap_shot = 1;

  dwl_inst = DWLInit(&dwl_init);
  if(dwl_inst == NULL) {
    DEBUG_PRINT(("DWLInit# ERROR: DWL Init failed\n"));
    goto end;
  }
  /* initialize decoder. If unsuccessful -> exit */
  decsw_performance();
  START_SW_PERFORMANCE;
  ret = VP8DecInit(&dec_inst,
                   dwl_inst,
                   dec_format, TBGetDecErrorConcealment( &tb_cfg ),
                   num_frame_buffers, tiled_output, 1, 0);
  END_SW_PERFORMANCE;
  decsw_performance();

  if (ret != VP8DEC_OK) {
    fprintf(stderr,"DECODER INITIALIZATION FAILED\n");
    goto end;
  }

  /* Set ref buffer test mode */
  //((VP8DecContainer_t *) dec_inst)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  TBSetRefbuMemModel( &tb_cfg,
                      ((VP8DecContainer_t *) dec_inst)->vp8_regs,
                      &((VP8DecContainer_t *) dec_inst)->ref_buffer_ctrl );

  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((VP8DecContainer_t *) dec_inst)->vp8_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  TBInitializeRandom(seed_rnd);

  /* Allocate stream memory */
  if(DWLMallocLinear(dwl_inst,
                     stream_len,
                     &stream_mem) != DWL_OK ) {
    fprintf(stderr,"UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    return -1;
  }

  DWLmemset(&dec_input, 0, sizeof(dec_input));
  DWLmemset(&dec_output, 0, sizeof(dec_output));

  dec_input.slice_height = tb_cfg.dec_params.jpeg_mcus_slice;

  /* No slice mode for VP8 video */
  /*    if(!snap_shot)
          dec_input.slice_height = 0;*/

  /* Start decode loop */
  do {
    /* read next frame from file format */
    if (!webp_loop && !dec_output.data_left) {
      tmp = FfReadFrame( reader, (u8*)stream_mem.virtual_address,
                         stream_mem.size, &size, pedantic_reading );
      /* ugly hack for large webp streams. drop reserved stream buffer
       * and reallocate new with correct stream size. */
      if (tmp == HANTRO_NOK &&
          size < VP8_MAX_STREAM_SIZE &&
          size + extra_strm_buffer_bits > stream_mem.size) {
        DWLFreeLinear(dwl_inst, &stream_mem);
        stream_len = size + extra_strm_buffer_bits;
        /* Allocate MORE stream memory */
        if (DWLMallocLinear(dwl_inst,
                           stream_len,
                           &stream_mem) != DWL_OK ) {
          fprintf(stderr,"UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
          return -1;
        }
        /* try again */
        tmp = FfReadFrame( reader, (u8*)stream_mem.virtual_address,
                           stream_mem.size, &size, pedantic_reading );

      }
      more_frames = (tmp==HANTRO_OK) ? HANTRO_TRUE : HANTRO_FALSE;
    }
    if( (more_frames && size != (u32)-1) || dec_output.data_left) {
      /* Decode */
      if (!webp_loop && !dec_output.data_left) {
        dec_input.stream = (const u8*)stream_mem.virtual_address;
        dec_input.data_len = size + extra_strm_buffer_bits;
        dec_input.stream_bus_address = (addr_t)stream_mem.bus_address;
      }

      decsw_performance();
      START_SW_PERFORMANCE;
      do {
        ret = VP8DecDecode(dec_inst, &dec_input, &dec_output);
      } while(dec_input.data_len == 0 && ret == VP8DEC_NO_DECODING_BUFFER)
      END_SW_PERFORMANCE;
      decsw_performance();
      if (ret == VP8DEC_HDRS_RDY) {
        rv = VP8DecGetBufferInfo(dec_inst, &hbuf);
        DEBUG_PRINT(("VP8DecGetBufferInfo ret %d\n", rv));
        DEBUG_PRINT(("buf_to_free %p, next_buf_size %d, buf_num %d\n",
                     (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
#ifdef SLICE_MODE_LARGE_PIC
        i32 mcu_in_row;
        i32 mcu_size_divider = 1;
#endif
        hdrs_rdy = 1;
        ret = VP8DecGetInfo(dec_inst, &info);
#ifdef SLICE_MODE_LARGE_PIC
        mcu_in_row = (info.frame_width / 16);
#endif
        ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                               !tb_cfg.pp_params.pipeline_e);
        if (dec_format == VP8DEC_WEBP && !params.pp_enabled && !user_mem_alloc) {
          params.ppu_cfg[0].scale.enabled = 1;
          params.fscale_cfg.fixed_scale_enabled = 1;
        }
        if (params.fscale_cfg.fixed_scale_enabled) {
          if(!params.ppu_cfg[0].crop.set_by_user) {
            params.ppu_cfg[0].crop.width = ((info.coded_width + 1) >> 1) << 1;
            params.ppu_cfg[0].crop.height = ((info.coded_height + 1) >> 1) << 1;
            params.ppu_cfg[0].crop.enabled = 1;
          }

          u32 crop_w = params.ppu_cfg[0].crop.width;
          u32 crop_h = params.ppu_cfg[0].crop.height;
          params.ppu_cfg[0].scale.width = (crop_w /
                                           params.fscale_cfg.down_scale_x) & ~0x1;
          params.ppu_cfg[0].scale.height = (crop_h /
                                           params.fscale_cfg.down_scale_y) & ~0x1;
          params.ppu_cfg[0].scale.enabled = 1;
          params.ppu_cfg[0].enabled = 1;
          params.pp_enabled = 1;
        }
        pp_enabled = params.pp_enabled;
        memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));

        dec_cfg.align = align;
        if (align == DEC_ALIGN_1B)
          dec_cfg.align = DEC_ALIGN_64B;
        dec_cfg.cr_first = cr_first;
        dec_cfg.fixed_scale_enabled = params.fscale_cfg.fixed_scale_enabled;
        tmp = VP8DecSetInfo(dec_inst, &dec_cfg);
        if (tmp != VP8DEC_OK)
          goto end;
#ifdef ASIC_TRACE_SUPPORT
        /* Handle incorrect slice size for HW testing */
        if(dec_input.slice_height > (info.frame_height >> 4)) {
          dec_input.slice_height = (info.frame_height >> 4);
          printf("FIXED Decoder Slice MB Set %d\n", dec_input.slice_height);
        }
#endif /* ASIC_TRACE_SUPPORT */
#ifdef SLICE_MODE_LARGE_PIC
#ifdef ASIC_TRACE_SUPPORT
        /* 8190 and over 16M ==> force to slice mode */
        if((dec_input.slice_height == 0) &&
            (snap_shot) &&
            ((info.frame_width * info.frame_height) >
             VP8DEC_MAX_PIXEL_AMOUNT)) {
          do {
            dec_input.slice_height++;
          } while(((dec_input.slice_height * (mcu_in_row / mcu_size_divider)) +
                   (mcu_in_row / mcu_size_divider)) <
                  VP8DEC_MAX_SLICE_SIZE);
          printf
          ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",
           dec_input.slice_height);
          forced_slice_mode = 1;
        }
#else
        /* 8190 and over 16M ==> force to slice mode */
        if((dec_input.slice_height == 0) &&
            (snap_shot) &&
            ((info.frame_width * info.frame_height) >
             VP8DEC_MAX_PIXEL_AMOUNT)) {
          do {
            dec_input.slice_height++;
          } while(((dec_input.slice_height * (mcu_in_row / mcu_size_divider)) +
                   (mcu_in_row / mcu_size_divider)) <
                  VP8DEC_MAX_SLICE_SIZE);
          printf
          ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",
           dec_input.slice_height);
          forced_slice_mode = 1;
        }
#endif /* ASIC_TRACE_SUPPORT */
#endif
        DEBUG_PRINT(("\nStream info:\n"));
        DEBUG_PRINT(("VP Version %d, Profile %d\n", info.vp_version, info.vp_profile));
        DEBUG_PRINT(("Frame size %dx%d\n", info.frame_width, info.frame_height));
        DEBUG_PRINT(("Coded size %dx%d\n", info.coded_width, info.coded_height));
        DEBUG_PRINT(("Scaled size %dx%d\n", info.scaled_width, info.scaled_height));
        DEBUG_PRINT(("Output format %s\n\n", info.output_format == VP8DEC_SEMIPLANAR_YUV420
                     ? "VP8DEC_SEMIPLANAR_YUV420" : "VP8DEC_TILED_YUV420"));

        if (user_mem_alloc && dec_format == VP8DEC_WEBP) {
          u32 size_luma;
          u32 size_chroma;
          u32 slice_height = 0;
          u32 width_y, width_c;

          width_y = info.frame_width;
          width_c = info.frame_width;

          for( i = 0 ; i < 16 ; ++i ) {
            if(user_alloc_luma[i].virtual_address)
              DWLFreeRefFrm(dwl_inst, &user_alloc_luma[i]);
            if(user_alloc_chroma[i].virtual_address)
              DWLFreeRefFrm(dwl_inst, &user_alloc_chroma[i]);
          }

          DEBUG_PRINT(("User allocated memory,width=%d,height=%d\n",
                       info.frame_width, info.frame_height));

          slice_height = ((VP8DecContainer_t *) dec_inst)->slice_height;
          if(forced_slice_mode)
            slice_height = dec_input.slice_height;
          size_luma = slice_height ?
                      (slice_height + 1) * 16 * width_y :
                      info.frame_height * width_y;

          size_chroma = slice_height ?
                        (slice_height + 1) * 8 * width_c :
                        info.frame_height * width_c / 2;


          if (DWLMallocRefFrm(dwl_inst,
                              size_luma, &user_alloc_luma[0]) != DWL_OK) {
            fprintf(stderr,"UNABLE TO ALLOCATE PICTURE MEMORY\n");
            return -1;
          }
          if (DWLMallocRefFrm(dwl_inst,
                              size_chroma, &user_alloc_chroma[0]) != DWL_OK) {
            fprintf(stderr,"UNABLE TO ALLOCATE PICTURE MEMORY\n");
            return -1;
          }

          dec_input.p_pic_buffer_y = user_alloc_luma[0].virtual_address;
          dec_input.pic_buffer_bus_address_y = user_alloc_luma[0].bus_address;
          dec_input.p_pic_buffer_c = user_alloc_chroma[0].virtual_address;
          dec_input.pic_buffer_bus_address_c = user_alloc_chroma[0].bus_address;
        }
        //prev_width = info.frame_width;
        //prev_height = info.frame_height;
        //min_buffer_num = info.pic_buff_size;
        decsw_performance();
        START_SW_PERFORMANCE;

        ret = VP8DecDecode(dec_inst, &dec_input, &dec_output);
        END_SW_PERFORMANCE;
        decsw_performance();
        TBSetRefbuMemModel( &tb_cfg,
                            ((VP8DecContainer_t *) dec_inst)->vp8_regs,
                            &((VP8DecContainer_t *) dec_inst)->ref_buffer_ctrl );

      }
      if (ret == VP8DEC_WAITING_FOR_BUFFER) {
        DEBUG_PRINT(("Waiting for frame buffers\n"));
        struct DWLLinearMem mem;

        rv = VP8DecGetBufferInfo(dec_inst, &hbuf);
        DEBUG_PRINT(("VP8DecGetBufferInfo ret %d\n", rv));
        DEBUG_PRINT(("buf_to_free %p, next_buf_size %d, buf_num %d\n",
                     (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
        while (rv == VP8DEC_WAITING_FOR_BUFFER) {
          if (hbuf.buf_to_free.virtual_address != NULL) {
            add_extra_flag = 0;
            ReleaseExtBuffers();
            buffer_release_flag = 1;
            num_buffers = 0;
          }
          rv = VP8DecGetBufferInfo(dec_inst, &hbuf);
        }

        buffer_size = hbuf.next_buf_size;
        if(buffer_release_flag && hbuf.next_buf_size) {
          /* Only add minimum required buffers at first. */
          //extra_buffer_num = hbuf.buf_num - min_buffer_num;
          for(i=0; i<hbuf.buf_num; i++) {
            if (pp_enabled)
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            else
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            rv = VP8DecAddBuffer(dec_inst, &mem);
            DEBUG_PRINT(("VP8DecAddBuffer ret %d\n", rv));
            if(rv != VP8DEC_OK && rv != VP8DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              ext_buffers[i] = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num;
          add_extra_flag = 1;
        }
        ret = VP8DecDecode(dec_inst, &dec_input, &dec_output);
        END_SW_PERFORMANCE;
        decsw_performance();
        TBSetRefbuMemModel( &tb_cfg,
                            ((VP8DecContainer_t *) dec_inst)->vp8_regs,
                            &((VP8DecContainer_t *) dec_inst)->ref_buffer_ctrl );
      }
      if (ret == VP8DEC_STRM_PROCESSED) {
        ret = VP8DecDecode(dec_inst, &dec_input, &dec_output);
        END_SW_PERFORMANCE;
        decsw_performance();
        TBSetRefbuMemModel( &tb_cfg,
                            ((VP8DecContainer_t *) dec_inst)->vp8_regs,
                            &((VP8DecContainer_t *) dec_inst)->ref_buffer_ctrl );
      }
      else if (ret != VP8DEC_PIC_DECODED &&
               ret != VP8DEC_SLICE_RDY) {
        continue;
      }

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, vp8_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      webp_loop = (ret == VP8DEC_SLICE_RDY);

      pic_rdy = 1;

      decsw_performance();
      START_SW_PERFORMANCE;
      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      if (use_peek_output &&
          VP8DecPeek(dec_inst, &dec_picture) == VP8DEC_PIC_RDY) {
        END_SW_PERFORMANCE;
        decsw_performance();
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(dec_picture.pictures[0].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(dec_picture.pictures[0].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(dec_picture.pictures[0].output_format))
          pp_mono_chrome = 1;
        if (!IS_PIC_PACKED_RGB(dec_picture.pictures[0].output_format) &&
            !IS_PIC_PLANAR_RGB(dec_picture.pictures[0].output_format))
          WriteOutput(out_file_name[0], (unsigned char *) dec_picture.pictures[i].p_output_frame,
                          dec_picture.pictures[0].coded_width,
                          dec_picture.pictures[0].coded_height,
                          0, pp_mono_chrome, 0, 0, 0,
                          IS_PIC_TILE(dec_picture.pictures[0].output_format),
                          dec_picture.pictures[0].pic_stride, dec_picture.pictures[0].pic_stride_ch, 0,
                          pp_planar, pp_cr_first, convert_tiled_output,
                          0, DEC_DPB_FRAME, md5sum, &fout[0], 1);
        else
          WriteOutputRGB(out_file_name[0], (unsigned char *) dec_picture.pictures[i].p_output_frame,
                          dec_picture.pictures[0].coded_width,
                          dec_picture.pictures[0].coded_height,
                          0, pp_mono_chrome, 0, 0, 0,
                          IS_PIC_TILE(dec_picture.pictures[0].output_format),
                          dec_picture.pictures[0].pic_stride, dec_picture.pictures[0].pic_stride_ch, 0,
                          IS_PIC_PLANAR_RGB(dec_picture.pictures[0].output_format), pp_cr_first,
                          convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &fout[0], 1);

        decsw_performance();
        START_SW_PERFORMANCE;

        END_SW_PERFORMANCE;
        decsw_performance();

      }
      END_SW_PERFORMANCE;
      decsw_performance();
    }
    if (ret != VP8DEC_SLICE_RDY)
      pic_decode_number++;
#if 0
    if (pic_decode_number == 10) {
      VP8DecRet tmp_ret = VP8DecAbort(dec_inst);
      tmp_ret = VP8DecAbortAfter(dec_inst);
      if(reader)
        rdr_close(reader);
      reader = rdr_open(argv[argc-1]);
    }
#endif
  } while( more_frames && (pic_decode_number != max_num_pics || !max_num_pics) );

  decsw_performance();
  START_SW_PERFORMANCE;

end:
  add_buffer_thread_run = 0;

  VP8DecEndOfStream(dec_inst, 1);

  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);


  if(stream_mem.virtual_address)
    DWLFreeLinear(dwl_inst, &stream_mem);

  if (user_mem_alloc) {
    for( i = 0 ; i < 16 ; ++i ) {
      if(user_alloc_luma[i].virtual_address)
        DWLFreeRefFrm(dwl_inst, &user_alloc_luma[i]);
      if(user_alloc_chroma[i].virtual_address)
        DWLFreeRefFrm(dwl_inst, &user_alloc_chroma[i]);
    }
  }

  decsw_performance();
  START_SW_PERFORMANCE;
  VP8DecRelease(dec_inst);
  END_SW_PERFORMANCE;
  decsw_performance();
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  DWLRelease(dwl_inst);

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if(reader)
    rdr_close(reader);
  for (i = 0; i < 4; i++) {
    if(fout[i]) {
      fclose(fout[i]);
    }
  }

  for (i = 0; i < 4; i++) {
    if(p_frame_pic[i])
      free(p_frame_pic[i]);
  }

  if (pic_big_endian)
    free(pic_big_endian);

  if(md5sum) {
    /* slice mode output is semiplanar yuv, convert to md5sum */
    if (slice_mode)
    {
      char cmd[256];
      sprintf(cmd, "md5sum %s | tr 'a-z' 'A-Z' | sed 's/ .*//' > .md5sum.txt", out_file_name[0]);
      ret = system(cmd);
      sprintf(cmd, "mv .md5sum.txt %s", out_file_name[0]);
      ret = system(cmd);
    }
  }

  return 0;
}

/*------------------------------------------------------------------------------

    Function name: decsw_performance

    Functional description: breakpoint for performance

    Inputs:  void

    Outputs:
chromaBufOff
    Returns: void

------------------------------------------------------------------------------*/
void decsw_performance(void) {
}


void writeSlice(FILE * fp, VP8DecPicture *dec_pic) {

  int luma_size = include_strides ? dec_pic->pictures[0].luma_stride * dec_pic->pictures[0].frame_height :
                  dec_pic->pictures[0].frame_width * dec_pic->pictures[0].frame_height;
//  int slice_size = include_strides ? dec_pic->pictures[0].luma_stride * dec_pic->num_slice_rows :
//                   dec_pic->pictures[0].frame_width * dec_pic->num_slice_rows;
  int chroma_size = include_strides ? dec_pic->pictures[0].chroma_stride * dec_pic->pictures[0].frame_height / 2:
                    dec_pic->pictures[0].frame_width * dec_pic->pictures[0].frame_height / 2;
  static u8 *tmp_ch = NULL;
  static u32 row_count = 0;
  u32 slice_rows;
  u32 luma_stride, chroma_stride;
  u32 i, j;
  u8 *ptr = (u8*)dec_pic->pictures[0].p_output_frame;

  if (tmp_ch == NULL) {
    tmp_ch = (u8*)malloc(chroma_size);
  }

  slice_rows = dec_pic->num_slice_rows;

  DEBUG_PRINT(("WRITING SLICE, rows %d\n",dec_pic->num_slice_rows));

  luma_stride = dec_pic->pictures[0].luma_stride;
  chroma_stride = dec_pic->pictures[0].chroma_stride;

  if(!height_crop) {
    /*fwrite(dec_pic->p_output_frame, 1, slice_size, fp);*/
    for ( i = 0 ; i < dec_pic->num_slice_rows ; ++i ) {
      fwrite(ptr, 1, include_strides ? luma_stride : dec_pic->pictures[0].coded_width, fp );
      ptr += luma_stride;
    }
  } else {
    if(row_count + slice_rows > dec_pic->pictures[0].coded_height )
      slice_rows -=
        (dec_pic->pictures[0].frame_height - dec_pic->pictures[0].coded_height);
    for ( i = 0 ; i < slice_rows ; ++i ) {
      fwrite(ptr, 1, include_strides ? luma_stride : dec_pic->pictures[0].coded_width, fp );
      ptr += luma_stride;
    }
  }

  for( i = 0 ; i < dec_pic->num_slice_rows/2 ; ++i ) {
    memcpy(tmp_ch + ((i+(row_count/2)) * (include_strides ? chroma_stride : dec_pic->pictures[0].frame_width)),
           dec_pic->pictures[0].p_output_frame_c + (i*chroma_stride)/4,
           include_strides ? chroma_stride : dec_pic->pictures[0].frame_width );
  }
  /*    memcpy(tmp_ch + row_count/2 * dec_pic->frame_width, dec_pic->p_output_frame_c,
          slice_size/2);*/

  row_count += dec_pic->num_slice_rows;

  if (row_count == dec_pic->pictures[0].frame_height) {
    if(!height_crop) {
      if (!planar_output) {
        fwrite(tmp_ch, luma_size/2, 1, fp);
        /*
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->num_slice_rows/2 ; ++i )
        {
            fwrite(ptr, 1, dec_pic->coded_width, fp );
            ptr += chroma_stride;
        } */
      } else {
        u32 i, tmp;
        tmp = chroma_size / 2;
        for(i = 0; i < tmp; i++)
          fwrite(tmp_ch + i * 2, 1, 1, fp);
        for(i = 0; i < tmp; i++)
          fwrite(tmp_ch + 1 + i * 2, 1, 1, fp);
      }
    } else {
      if(!planar_output) {
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->pictures[0].coded_height/2 ; ++i ) {
          fwrite(ptr, 1, dec_pic->pictures[0].coded_width, fp );
          ptr += dec_pic->pictures[0].frame_width;
        }
      } else {
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->pictures[0].coded_height/2 ; ++i ) {
          for( j = 0 ; j < dec_pic->pictures[0].coded_width/2 ; ++j )
            fwrite(ptr + 2*j, 1, 1, fp );
          ptr += dec_pic->pictures[0].frame_width;
        }
        ptr = tmp_ch+1;
        for ( i = 0 ; i < dec_pic->pictures[0].coded_height/2 ; ++i ) {
          for( j = 0 ; j < dec_pic->pictures[0].coded_width/2 ; ++j )
            fwrite(ptr + 2*j, 1, 1, fp );
          ptr += dec_pic->pictures[0].frame_width;
        }
      }
    }
  }

}

u32 FfReadFrame( reader_inst reader, const u8 *buffer, u32 max_buffer_size,
                 u32 *frame_size, u32 pedantic ) {
  u32 tmp;
  u32 ret = rdr_read_frame(reader, buffer, max_buffer_size, frame_size,
                           pedantic);
  if (ret != HANTRO_OK)
    return ret;

  /* stream corruption, packet loss etc */
  if (stream_packet_loss) {
    ret =  TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, (u8 *)&tmp);
    if (ret == 0 && tmp)
      return FfReadFrame(reader, buffer, max_buffer_size, frame_size,
                         pedantic);
  }

  if (stream_truncate && pic_rdy && (hdrs_rdy || stream_header_corrupt)) {
    DEBUG_PRINT(("strm_len %d\n", *frame_size));
    ret = TBRandomizeU32(frame_size);
    DEBUG_PRINT(("Randomized strm_len %d\n", *frame_size));
  }
  if (stream_bit_swap) {
    if (stream_header_corrupt || hdrs_rdy) {
      if (pic_rdy) {
        ret = TBRandomizeBitSwapInStream((u8 *)buffer,
                                         *frame_size, tb_cfg.tb_params.stream_bit_swap);
      }
    }
  }
  return HANTRO_OK;
}
