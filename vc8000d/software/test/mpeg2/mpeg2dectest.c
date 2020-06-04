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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE
#include <fcntl.h>
#endif

#include "mpeg2decapi.h"
#include "dwl.h"
#ifdef USE_EFENCE
#include "efence.h"
#endif

#include "mpeg2hwd_container.h"
#include "regdrv.h"
#include "ppu.h"

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include "tb_cfg.h"
#include "tb_tiled.h"
#include "tb_md5.h"
#include "tb_out.h"
#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "command_line_parser.h"

/*#define DISABLE_WHOLE_STREAM_BUFFERING 0*/
#define DEFAULT -1
#define VOP_START_CODE 0xb6010000

/* Size of stream buffer */
#define STREAMBUFFER_BLOCKSIZE 0xfffff
#define MPEG2_WHOLE_STREAM_SAFETY_LIMIT (10*10*1024)
#define MAX_BUFFERS 16

#define MPEG2_FRAME_BUFFER_SIZE ((1280 * 720 * 3) / 2)  /* 720p frame */
#define MPEG2_NUM_BUFFERS 3 /* number of output buffers for ext alloc */
#define ASSERT(expr) assert(expr)

/* Function prototypes */

void printTimeCode(Mpeg2DecTime * timecode);
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer);
void decRet(Mpeg2DecRet ret);
void decNextPictureRet(Mpeg2DecRet ret);
void printMpeg2Version(void);
i32 AllocatePicBuffers(struct DWLLinearMem * buffer, Mpeg2DecContainer * container);
void printMpeg2PicCodingType(u32 pic_type);

void decsw_performance(void) {
}


/* stream start address */
u8 *byte_strm_start;

/* stream used in SW decode */
u32 trace_used_stream = 0;
u32 previous_used = 0;

/* SW/SW testing, read stream trace file */
FILE *f_stream_trace = NULL;

/* output file */
FILE *fout[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL};
FILE *f_tiled_output = NULL;
static u32 StartCode;

i32 strm_rew = 0;
u32 length = 0;
u32 write_output = 1;
u32 crop_output = 0;
u8 disable_resync = 0;
u8 strm_end = 0;
u8 *stream_stop = NULL;

u32 stream_size = 0;
u32 stop_decoding = 0;
u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 hdrs_rdy = 0;
u32 pic_rdy = 0;
u32 stream_header_corrupt = 0;
struct TBCfg tb_cfg;

/* Give stream to decode as one chunk */
u32 whole_stream_mode = 0;
u32 cumulative_error_mbs = 0;

u32 planar_output = 0;
u32 interlaced_field = 0;

/* flag to enable md5sum output */
u32 md5sum = 0;

u32 use_peek_output = 0;
u32 skip_non_reference = 0;

u32 pic_display_number = 0;
u32 frame_number = 0;

u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 dpb_mode = DEC_DPB_FRAME;
u32 convert_to_frame_dpb = 0;
u32 convert_tiled_output = 0;

FILE *findex = NULL;
u32 save_index = 0;
u32 use_index = 0;
off64_t cur_index = 0;
addr_t next_index = 0;
u32 ds_ratio_x, ds_ratio_y;
u32 pp_tile_out = 0;    /* PP tiled output */
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;
/* user input arguments */
u32 scale_enabled = 0;
u32 scaled_w, scaled_h;
u32 crop_enabled = 0;
u32 crop_x = 0;
u32 crop_y = 0;
u32 crop_w = 0;
u32 crop_h = 0;
enum SCALE_MODE scale_mode;

#ifdef MPEG2_EVALUATION
extern u32 g_hw_ver;
#endif

Mpeg2DecInst decoder;
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
u32 buffer_release_flag = 1;

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && num_buffers < 16) {
      struct DWLLinearMem mem;
      i32 dwl_ret;
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        Mpeg2DecRet rv = Mpeg2DecAddBuffer(decoder, &mem);
        if(rv != MPEG2DEC_OK && rv != MPEG2DEC_WAITING_FOR_BUFFER) {
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
  printf("Releasing %d external frame buffers\n", num_buffers);
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    printf("Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address);
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

char out_file_name[DEC_MAX_PPU_COUNT][256] = {"", "", "", ""};
char out_file_name_tiled[256] = "out_tiled.yuv";
Mpeg2DecInfo Decinfo;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
pthread_t *output_thread = NULL;
pthread_t *release_thread = NULL;
int output_thread_run = 0;

sem_t buf_release_sem;
Mpeg2DecPicture buf_list[100] = {{0}};
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
      Mpeg2DecPictureConsumed(decoder, &buf_list[list_pop_index]);
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
            dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          else
            dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          if(dwl_ret == DWL_OK) {
            Mpeg2DecRet rv = Mpeg2DecAddBuffer(decoder, &mem);
            if(rv != MPEG2DEC_OK && rv != MPEG2DEC_WAITING_FOR_BUFFER) {
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
    if(last_pic_flag &&  buf_status[list_pop_index] == 0)
      break;
    usleep(5000);  
  }
  return (NULL);
}

/* Output thread entry point. */
static void* mpeg2_output_thread(void* arg) {
  Mpeg2DecPicture DecPic;
  u32 pic_display_number = 1;
  u32 i, pp_planar, pp_cr_first, pp_mono_chrome;
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
    u8 *image_data;
    Mpeg2DecRet ret;
    ret = Mpeg2DecNextPicture(decoder, &DecPic, 0);
    if(ret == MPEG2DEC_PIC_RDY) {
      if (DecPic.interlaced && DecPic.field_picture &&
          DecPic.output_other_field) {
          continue; // do not send one field frame twice
      }

      for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
        if (!DecPic.pictures[i].frame_width ||
            !DecPic.pictures[i].frame_height)
          continue;
        if(!use_peek_output) {
          /* print result */
          decNextPictureRet(ret);
          /* printf info */
          printf("PIC %d, %s", DecPic.pic_id,
                 DecPic.key_picture ? "key picture,    " :
                 "non key picture,");

          if(DecPic.field_picture) {
            /* pic coding type */
            if(DecPic.top_field)
              printMpeg2PicCodingType(DecPic.pic_coding_type[0]);
            else
              printMpeg2PicCodingType(DecPic.pic_coding_type[1]);
            printf(" %s ", DecPic.top_field ?
                   "top field.   " : "bottom field.");
          } else {
            printMpeg2PicCodingType(DecPic.pic_coding_type[0]);
            printf(" frame picture. ");
          }

          printTimeCode(&(DecPic.time_code));
          if(DecPic.number_of_err_mbs) {
            printf(", %d/%d error mbs\n",
                   DecPic.number_of_err_mbs,
                   (DecPic.pictures[i].frame_width >> 4) *
                   (DecPic.pictures[i].frame_height >> 4));
            cumulative_error_mbs += DecPic.number_of_err_mbs;
          }

          /* Write output picture to file */
          image_data = (u8 *) DecPic.pictures[i].output_picture;

          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(DecPic.pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(DecPic.pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(DecPic.pictures[i].output_format))
            pp_mono_chrome = 1;

          printf("DecPic.first_field %d\n", DecPic.first_field);

          if (!IS_PIC_PACKED_RGB(DecPic.pictures[i].output_format) &&
              !IS_PIC_PLANAR_RGB(DecPic.pictures[i].output_format))
            WriteOutput(out_file_name[i], image_data,
                        DecPic.pictures[i].coded_width,
                        DecPic.pictures[i].coded_height,
                        pic_display_number - 1,
                        pp_mono_chrome, 0, 0, !write_output,
                        IS_PIC_TILE(DecPic.pictures[i].output_format),
                        DecPic.pictures[i].pic_stride, DecPic.pictures[i].pic_stride_ch, i,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        convert_to_frame_dpb, dpb_mode, md5sum, &fout[i], 1);
          else
            WriteOutputRGB(out_file_name[i], image_data,
                           DecPic.pictures[i].coded_width,
                           DecPic.pictures[i].coded_height,
                           pic_display_number - 1,
                           pp_mono_chrome, 0, 0, !write_output,
                           IS_PIC_TILE(DecPic.pictures[i].output_format),
                           DecPic.pictures[i].pic_stride, DecPic.pictures[i].pic_stride_ch, i,
                           IS_PIC_PLANAR_RGB(DecPic.pictures[i].output_format), pp_cr_first,
                           convert_tiled_output, convert_to_frame_dpb, dpb_mode, md5sum, &fout[i], 1);
        }
      }

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = DecPic;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);
      pic_display_number++;
    }

    else if(ret == MPEG2DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
//        usleep(50000);
  }
  return (NULL);
}

int main(int argc, char **argv) {
  FILE *f_tbcfg;
  u8 *p_strm_data = 0;
  u32 max_num_frames;   /* todo! */
  u32 num_frame_buffers = 0;
  u8 *image_data;

  u32 i, j, stream_len = 0;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;
  u32 vp_num = 0;
  u32 allocate_buffers = 0;    /* Allocate buffers in test bench */
  u32 cr_first = 0;
  struct Mpeg2DecConfig dec_cfg;
  u32 tmp = 0;

  /*
   * Decoder API structures
   */
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  Mpeg2DecBufferInfo hbuf;
  Mpeg2DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  Mpeg2DecRet ret;
  Mpeg2DecRet info_ret;
  Mpeg2DecInput DecIn;
  Mpeg2DecOutput DecOut;
  Mpeg2DecPicture DecPic = {0};
  struct DWLLinearMem stream_mem;
  u32 pic_id = 0;

  FILE *f_in = NULL;
  struct DWLLinearMem pic_buffer[MPEG2_NUM_BUFFERS] = { {0} };

  u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
  u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
  u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
  u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
  u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
  u32 output_format = DEC_X170_OUTPUT_FORMAT;
  u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;

  u32 seed_rnd = 0;
  u32 stream_bit_swap = 0;
  i32 corrupted_bytes = 0;
  enum DecErrorHandling error_handling;

  struct TestParams params;

  SetupDefaultParams(&params);

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

  INIT_SW_PERFORMANCE;

#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 8190; /* default to 8190 mode */
#endif

#ifdef MPEG2_EVALUATION_8170
  g_hw_ver = 8170;
#elif MPEG2_EVALUATION_8190
  g_hw_ver = 8190;
#elif MPEG2_EVALUATION_9170
  g_hw_ver = 9170;
#elif MPEG2_EVALUATION_9190
  g_hw_ver = 9190;
#elif MPEG2_EVALUATION_G1
  g_hw_ver = 10000;
#endif

  if(argc < 2) {

    printf("\n8170 MPEG-2 Decoder Testbench\n\n");
    printf("USAGE:\n%s [options] stream.mpeg2\n", argv[0]);
    printf("\t-Ooutfile write output to \"outfile\" (default out.yuv)\n");
    printf("\t-Nn to decode only first n frames of the stream\n");
    printf("\t-X to not to write output picture\n");
    printf("\t-m Output md5 for each picture(--md5-per-pic)\n");
    printf("\t-P write planar output.(--planar)\n");
    printf("\t-a Enable PP planar output.(--pp-planar)\n");
    printf("\t-Bn to use n frame buffers in decoder\n");
    printf("\t-E use tiled reference frame format.\n");
    printf("\t-G convert tiled output pictures to raster scan\n");
    printf("\t-Sfile.hex stream control trace file\n");
#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
    printf("\t-R use reference decoder IDCT (sw/sw integration only)\n");
#endif
    printf("\t-W whole stream mode - give stream to decoder in one chunk\n");
    printf("\t-I save index file\n");
    printf
    ("\t-T write tiled output (out_tiled.yuv) by converting raster scan output\n");
    printf("\t-Y Write output as Interlaced Fields (instead of Frames).\n");
    printf
    ("\t-C crop output picture to real picture dimensions (only planar)\n");
    printf("\t-Q Skip decoding non-reference pictures.\n");
    printf("\t-Z output pictures using Mpeg2DecPeek() function\n");
    printf("\t--separate-fields-in-dpb DPB stores interlaced content"\
           " as fields (default: frames)\n");
    printf("\t--output-frame-dpb Convert output to frame mode even if"\
           " field DPB mode used\n");
    printf("\t-e add extra external buffer randomly\n");
    printf("\t-k add extra external buffer in ouput thread\n");
    printf("\t-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n");
    printf("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g.,\n");
    printf("\t  -d2 -- down scale to 1/2 in both directions\n");
    printf("\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
    printf("\t--cr-first PP outputs chroma in CrCb order.\n");
    printf("\t-C[xywh]NNN Cropping parameters. E.g.,\n");
    printf("\t  -Cx8 -Cy16        Crop from (8, 16)\n");
    printf("\t  -Cw720 -Ch480     Crop size  720x480\n");
    printf("\t-Dwxh  PP output size wxh. E.g.,\n");
    printf("\t  -D1280x720        PP output size 1280x720\n");
    printf("\t--pp-rgb Enable Yuv2RGB.\n");
    printf("\t--pp-rgb-planar Enable planar Yuv2RGB.\n");
    printf("\t--rgb-fmat set the RGB output format.\n");
    printf("\t--rgb-std set the standard coeff to do Yuv2RGB.\n\n");
    printMpeg2Version();
    exit(100);
  }

  remove("pp_out.yuv");
  max_num_frames = 0;
  for(i = 1; i < argc - 1; i++) {
    if(strncmp(argv[i], "-O", 2) == 0) {
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(out_file_name[j], argv[i] + 2);
    } else if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_frames = atoi(argv[i] + 2);
    } else if (strncmp(argv[i], "-E", 2) == 0)
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    else if (strncmp(argv[i], "-G", 2) == 0)
      convert_tiled_output = 1;
    else if(strncmp(argv[i], "-B", 2) == 0) {
      num_frame_buffers = atoi(argv[i] + 2);
      if(num_frame_buffers > 16)
        num_frame_buffers = 16;
    } else if(strncmp(argv[i], "-X", 2) == 0) {
      write_output = 0;
    } else if(strncmp(argv[i], "-S", 2) == 0) {
      f_stream_trace = fopen((argv[i] + 2), "r");
    } else if(strncmp(argv[i], "-P", 2) == 0 ||
              strcmp(argv[i], "--planar") == 0) {
      planar_output = 1;
    } else if(strncmp(argv[i], "-a", 2) == 0 ||
              strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      //tb_cfg.pp_units_params[0].planar = pp_planar_out;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = 1;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "-Q") == 0) {
      skip_non_reference = 1;
    }
#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
    else if(strncmp(argv[i], "-R", 2) == 0) {
      use_mpeg2_idct = 1;
    }
#endif
    else if(strncmp(argv[i], "-W", 2) == 0) {
      whole_stream_mode = 1;
    } else if(strcmp(argv[i], "-Y") == 0) {
      interlaced_field = 1;
    } else if(strcmp(argv[i], "-C") == 0) {
      crop_output = 1;
    } else if(strcmp(argv[i], "-Z") == 0) {
      use_peek_output = 1;
    } else if(strcmp(argv[i], "-m") == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
    } else if(strcmp(argv[i], "-I") == 0) {
      save_index = 1;
    } else if(strcmp(argv[i], "--separate-fields-in-dpb") == 0) {
      dpb_mode = DEC_DPB_INTERLACED_FIELD;
    } else if(strcmp(argv[i], "--output-frame-dpb") == 0) {
      convert_to_frame_dpb = 1;
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
      params.ppu_cfg[0].crop.set_by_user = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].crop.enabled = 1;
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
      printf("UNKNOWN PARAMETER: %s\n", argv[i]);
      return 1;
    }
  }

  printMpeg2Version();
  /* open data file */
  f_in = fopen(argv[argc - 1], "rb");
  if(f_in == NULL) {
    printf("Unable to open input file %s\n", argv[argc - 1]);
    exit(100);
  }

#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
  if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
    tb_cfg.dec_params.cache_support = 1;
  else
    tb_cfg.dec_params.cache_support = 0;
#endif
#endif
  /* open index file for saving */
  if(save_index) {
    findex = fopen("stream.cfg", "w");
    if(findex == NULL) {
      printf("UNABLE TO OPEN INDEX FILE\n");
      return -1;
    }
  } else {
    findex = fopen("stream.cfg", "r");
    if(findex != NULL) {
      use_index = 1;
    }
  }

  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if(f_tbcfg == NULL) {
    printf("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n");
    printf("USING DEFAULT CONFIGURATION\n");
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if(TBCheckCfg(&tb_cfg) != 0)
      return -1;
  }
  /*TBPrintCfg(&tb_cfg); */
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  output_format = TBGetDecOutputFormat(&tb_cfg);
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);
  /*#if MD5SUM
      output_picture_endian = DEC_X170_LITTLE_ENDIAN;
      printf("Decoder Output Picture Endian forced to %d\n",
             output_picture_endian);
  #endif*/
  printf("Decoder Clock Gating %d\n", clock_gating);
  printf("Decoder Data Discard %d\n", data_discard);
  printf("Decoder Latency Compensation %d\n", latency_comp);
  printf("Decoder Output Picture Endian %d\n", output_picture_endian);
  printf("Decoder Bus Burst Length %d\n", bus_burst_length);
  printf("Decoder Asic Service Priority %d\n", asic_service_priority);
  printf("Decoder Output Format %d\n", output_format);

  seed_rnd = tb_cfg.tb_params.seed_rnd;
  stream_header_corrupt = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  /* if headers are to be corrupted
    -> do not wait the picture to finalize before starting stream corruption */
  if (stream_header_corrupt)
    pic_rdy = 1;
  stream_truncate = TBGetTBStreamTruncate(&tb_cfg);
  if(strcmp(tb_cfg.tb_params.stream_bit_swap, "0") != 0) {
    stream_bit_swap = 1;
  } else {
    stream_bit_swap = 0;
  }
  if(strcmp(tb_cfg.tb_params.stream_packet_loss, "0") != 0) {
    stream_packet_loss = 1;
  } else {
    stream_packet_loss = 0;
  }
  disable_resync = TBGetTBPacketByPacket(&tb_cfg);
  printf("TB Slice by slice %d\n", disable_resync);
  printf("TB Seed Rnd %d\n", seed_rnd);
  printf("TB Stream Truncate %d\n", stream_truncate);
  printf("TB Stream Header Corrupt %d\n", stream_header_corrupt);
  printf("TB Stream Bit Swap %d; odds %s\n",
         stream_bit_swap, tb_cfg.tb_params.stream_bit_swap);
  printf("TB Stream Packet Loss %d; odds %s\n",
         stream_packet_loss, tb_cfg.tb_params.stream_packet_loss);

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;

  length = STREAMBUFFER_BLOCKSIZE;

  rewind(f_in);

  TBInitializeRandom(seed_rnd);

  /* check size of the input file -> length of the stream in bytes */
  fseek(f_in, 0L, SEEK_END);
  stream_size = (u32) ftell(f_in);
  rewind(f_in);

  /* sets the stream length to random value */
  if(stream_truncate && !disable_resync) {
    printf("stream_size %d\n", stream_size);
    ret = TBRandomizeU32(&stream_size);
    if(ret != 0) {
      printf("RANDOM STREAM ERROR FAILED\n");
      return -1;
    }
    printf("Randomized stream_size %d\n", stream_size);
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if(!tmp) {
    printf("UNABLE TO OPEN TRACE FILES(S)\n");
  }
#endif

  dwl_init.client_type = DWL_CLIENT_TYPE_MPEG2_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto end2;
  }

  switch (TBGetDecErrorConcealment(&tb_cfg)) {
  case 1:
    error_handling = DEC_EC_VIDEO_FREEZE;
    break;
  case 2:
    error_handling = DEC_EC_PARTIAL_FREEZE;
    break;
  case 3:
    error_handling = DEC_EC_PARTIAL_IGNORE;
    break;
  case 0:
  default:
    error_handling = DEC_EC_PICTURE_FREEZE;
    break;
  }

  /* Initialize the decoder */
  START_SW_PERFORMANCE;
  decsw_performance();
  ret = Mpeg2DecInit(&decoder,
                     dwl_inst,
                     error_handling,
                     num_frame_buffers,
                     tiled_output |
                     (dpb_mode == DEC_DPB_INTERLACED_FIELD ? DEC_DPB_ALLOW_FIELD_ORDERING : 0), 1, 0);

  END_SW_PERFORMANCE;
  decsw_performance();

  if(ret != MPEG2DEC_OK) {
    printf("Could not initialize decoder\n");
    goto end2;
  }

  /* Set ref buffer test mode */
  //((Mpeg2DecContainer *) decoder)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  if(DWLMallocLinear(((Mpeg2DecContainer *) decoder)->dwl,
                     STREAMBUFFER_BLOCKSIZE, &stream_mem) != DWL_OK) {
    printf(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end2;
  }

  /* allocate output buffers if necessary */
  if(allocate_buffers) {
    if(AllocatePicBuffers(pic_buffer, (Mpeg2DecContainer *) decoder))
      goto end2;
  }
  byte_strm_start = (u8 *) stream_mem.virtual_address;

  DecIn.skip_non_reference = skip_non_reference;
  DecIn.stream = byte_strm_start;
  DecIn.stream_bus_address = stream_mem.bus_address;

  if(byte_strm_start == NULL) {
    printf(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end2;
  }

  stream_stop = byte_strm_start + length;
  /* NOTE: The registers should not be used outside decoder SW for other
   * than compile time setting test purposes */
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
                 */
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((Mpeg2DecContainer *) decoder)->mpeg2_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  decsw_performance();
  info_ret = Mpeg2DecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(info_ret) {
    decRet(info_ret);
  }

  p_strm_data = (u8 *) DecIn.stream;

  /* Read sequence headers */
  stream_len = readDecodeUnit(f_in, p_strm_data);

  i = StartCode;
  /* decrease 4 because previous function call
   * read the first sequence start code */

  stream_len -= 4;
  DecIn.data_len = stream_len;
  DecOut.data_left = 0;

  printf("Start decoding\n");
  do {
    if (ret != MPEG2DEC_NO_DECODING_BUFFER)
      printf("DecIn.data_len %d\n", DecIn.data_len);
    DecIn.pic_id = pic_id;
    if(ret != MPEG2DEC_STRM_PROCESSED &&
        ret != MPEG2DEC_BUF_EMPTY &&
        ret != MPEG2DEC_NO_DECODING_BUFFER &&
        ret != MPEG2DEC_NONREF_PIC_SKIPPED )
      printf("\nStarting to decode picture ID %d\n", pic_id);

    /* If enabled, break the stream */
    if(stream_bit_swap) {
      if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
        /* Picture needs to be ready before corrupting next picture */
        if(pic_rdy && corrupted_bytes <= 0) {
          ret = TBRandomizeBitSwapInStream(DecIn.stream,
                                           DecIn.data_len,
                                           tb_cfg.tb_params.
                                           stream_bit_swap);
          if(ret != 0) {
            printf("RANDOM STREAM ERROR FAILED\n");
            goto end2;
          }

          corrupted_bytes = DecIn.data_len;
          printf("corrupted_bytes %d\n", corrupted_bytes);
        }
      }
    }

    assert(DecOut.data_left == DecIn.data_len || !DecOut.data_left);

    START_SW_PERFORMANCE;
    decsw_performance();
    ret = Mpeg2DecDecode(decoder, &DecIn, &DecOut);
    END_SW_PERFORMANCE;
    decsw_performance();

    decRet(ret);

    /*
     * Choose what to do now based on the decoder return value
     */

    switch (ret) {

    case MPEG2DEC_HDRS_RDY:

      /* Set a flag to indicate that headers are ready */
      hdrs_rdy = 1;
      TBSetRefbuMemModel( &tb_cfg,
                          ((Mpeg2DecContainer *) decoder)->mpeg2_regs,
                          &((Mpeg2DecContainer *) decoder)->ref_buffer_ctrl );

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      info_ret = Mpeg2DecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(info_ret) {
        decRet(info_ret);
      }

      if (Decinfo.interlaced_sequence)
        printf("INTERLACED SEQUENCE\n");
      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);

      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if(!params.ppu_cfg[i].enabled) continue;

        if(!params.ppu_cfg[i].crop.set_by_user) {
          params.ppu_cfg[i].crop.width = Decinfo.coded_width;
          params.ppu_cfg[i].crop.height = Decinfo.coded_height;
          params.ppu_cfg[i].crop.enabled = 1;
        }
      }

      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = Decinfo.coded_width;
          params.ppu_cfg[0].crop.height = Decinfo.coded_height;
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
      dec_cfg.cr_first = cr_first;
      tmp = Mpeg2DecSetInfo(decoder, &dec_cfg);
      if (tmp != MPEG2DEC_OK)
        goto end2;
      //prev_width = Decinfo.frame_width;
      //prev_height = Decinfo.frame_height;
      //min_buffer_num = Decinfo.pic_buff_size;
      //dpb_mode = Decinfo.dpb_mode;

      /* If -O option not used, generate default file name */
      if(out_file_name[0] == 0) {
        if(planar_output && crop_output)
          sprintf(out_file_name[0], "out_%dx%d.yuv",
                  Decinfo.coded_width, Decinfo.coded_height);
        else
          sprintf(out_file_name[0], "out_%dx%d.yuv",
                  Decinfo.frame_width, Decinfo.frame_height);
      }

      if(!frame_number) {
        /*disable_resync = 0; */
        if(Decinfo.stream_format == MPEG2)
          printf("MPEG-2 stream\n");
        else
          printf("MPEG-1 stream\n");

        printf("Profile and level %d\n",
               Decinfo.profile_and_level_indication);
        switch (Decinfo.display_aspect_ratio) {
        case MPEG2DEC_1_1:
          printf("Display Aspect ratio 1:1\n");
          break;
        case MPEG2DEC_4_3:
          printf("Display Aspect ratio 4:3\n");
          break;
        case MPEG2DEC_16_9:
          printf("Display Aspect ratio 16:9\n");
          break;
        case MPEG2DEC_2_21_1:
          printf("Display Aspect ratio 2.21:1\n");
          break;
        }
        printf("Output format %s\n",
               Decinfo.output_format == MPEG2DEC_SEMIPLANAR_YUV420
               ? "MPEG2DEC_SEMIPLANAR_YUV420" :
               "MPEG2DEC_TILED_YUV420");
      }

      if (ret != MPEG2DEC_NO_DECODING_BUFFER)
        printf("DecOut.data_left %d \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.data_len - DecOut.data_left);
        DecIn.data_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {
        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          /* stream ended */
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;

        corrupted_bytes = 0;

      }

      break;

    case MPEG2DEC_WAITING_FOR_BUFFER:
      rv = Mpeg2DecGetBufferInfo(decoder, &hbuf);
      printf("MREG2DecGetBufferInfo ret %d\n", rv);
      printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
      if (hbuf.buf_to_free.virtual_address != NULL) {
        add_extra_flag = 0;
        ReleaseExtBuffers();
        buffer_release_flag = 1;
        num_buffers = 0;
      }
      if(buffer_release_flag && hbuf.next_buf_size) {
        /* Only add minimum required buffers at first. */
        //extra_buffer_num = hbuf.buf_num - min_buffer_num;
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;
        for(i=0; i<hbuf.buf_num; i++) {
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = Mpeg2DecAddBuffer(decoder, &mem);
          printf("Mpeg2DecAddBuffer ret %d\n", rv);
          if(rv != MPEG2DEC_OK && rv != MPEG2DEC_WAITING_FOR_BUFFER) {
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
      break;

    case MPEG2DEC_PIC_DECODED:
      /* Picture is ready */
      pic_rdy = 1;
      pic_id++;
#if 0
      if (pic_id == 10) {
        info_ret = Mpeg2DecAbort(decoder);
        info_ret = Mpeg2DecAbortAfter(decoder);
        pic_id = 0;
        rewind(f_in);

        /* Read sequence headers */
        stream_len = readDecodeUnit(f_in, p_strm_data);

        i = StartCode;
        /* decrease 4 because previous function call
         * read the first sequence start code */

        stream_len -= 4;
        DecIn.data_len = stream_len;
        DecIn.stream = byte_strm_start;
        DecIn.stream_bus_address = stream_mem.bus_address;
        DecOut.data_left = 0;
        break;
      }
#endif

      /* Read what kind of stream is coming */
      info_ret = Mpeg2DecGetInfo(decoder, &Decinfo);
      if(info_ret) {
        decRet(info_ret);
      }
      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
	    output_thread = (pthread_t *)calloc(1, sizeof(pthread_t));
	    release_thread = (pthread_t *)calloc(1, sizeof(pthread_t));
		assert(NULL != output_thread);
		assert(NULL != release_thread);
        pthread_create(output_thread, NULL, mpeg2_output_thread, NULL);
        pthread_create(release_thread, NULL, buf_release_thread, NULL);
      }

      if (use_peek_output &&
          Mpeg2DecPeek(decoder, &DecPic) == MPEG2DEC_PIC_RDY) {
        pic_display_number++;
        printf("DECPIC %d, %s", DecPic.pic_id,
               DecPic.key_picture ? "key picture,    " :
               "non key picture,");

        /* pic coding type */
        printMpeg2PicCodingType(DecPic.pic_coding_type[0]);

        image_data = (u8 *) DecPic.pictures[0].output_picture;

        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(DecPic.pictures[0].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(DecPic.pictures[0].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(DecPic.pictures[0].output_format))
          pp_mono_chrome = 1;

        if (!IS_PIC_PACKED_RGB(DecPic.pictures[0].output_format) &&
                      !IS_PIC_PLANAR_RGB(DecPic.pictures[0].output_format))
          WriteOutput(out_file_name[0], image_data,
                      DecPic.pictures[0].coded_width,
                      DecPic.pictures[0].coded_height,
                      pic_display_number - 1,
                      pp_mono_chrome,
                      0, 0, !write_output,
                      IS_PIC_TILE(DecPic.pictures[0].output_format),
                      DecPic.pictures[0].pic_stride, DecPic.pictures[0].pic_stride_ch, 0,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      convert_to_frame_dpb, dpb_mode, md5sum, &fout[0], 1);
        else
          WriteOutputRGB(out_file_name[0], image_data,
                         DecPic.pictures[0].coded_width,
                         DecPic.pictures[0].coded_height,
                         pic_display_number - 1,
                         pp_mono_chrome,
                         0, 0, !write_output,
                         IS_PIC_TILE(DecPic.pictures[0].output_format),
                         DecPic.pictures[0].pic_stride, DecPic.pictures[0].pic_stride_ch, 0,
                         IS_PIC_PLANAR_RGB(DecPic.pictures[0].output_format), pp_cr_first,
                         convert_tiled_output, convert_to_frame_dpb, dpb_mode, md5sum, &fout[0], 1);
      }


      frame_number++;
      vp_num = 0;

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      if (ret != MPEG2DEC_NO_DECODING_BUFFER)
        printf("DecOut.data_left %d \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.data_len - DecOut.data_left);
        DecIn.data_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;

        corrupted_bytes = 0;

      }

      if(max_num_frames && (frame_number >= max_num_frames)) {
        printf("\n\nMax num of pictures reached\n\n");
        DecIn.data_len = 0;
        goto end2;
      }

      break;

    case MPEG2DEC_STRM_PROCESSED:
    case MPEG2DEC_BUF_EMPTY:
    case MPEG2DEC_NONREF_PIC_SKIPPED:
      fprintf(stdout,
              "TB: Frame Number: %u, pic: %d\n", vp_num++, frame_number);
    /* Used to indicate that picture decoding needs to
     * finalized prior to corrupting next picture */
#ifdef GET_FREE_BUFFER_NON_BLOCK
    case MPEG2DEC_NO_DECODING_BUFFER:
    /* Just for simulation: if no buffer, sleep 0.5 second and try decoding again. */
#endif

    /* fallthrough */

    case MPEG2DEC_OK:

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      info_ret = Mpeg2DecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;
      decsw_performance();

      if(info_ret) {
        decRet(info_ret);
      }

      /*
       **  Write output picture to the file
       */

      /*
       *    Read next decode unit. Because readDecodeUnit
       *   reads VOP start code in previous
       *   function call, Insert this start code
       *   in to first word
       *   of stream buffer, and increase
       *   stream buffer pointer by 4 in
       *   the function call.
       */
      if (ret != MPEG2DEC_NO_DECODING_BUFFER)
        printf("DecOut.data_left %d \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.data_len - DecOut.data_left);
        DecIn.data_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;

        corrupted_bytes = 0;

      }

      break;

    case MPEG2DEC_PARAM_ERROR:
      printf("INCORRECT STREAM PARAMS\n");
      goto end2;
      break;

    case MPEG2DEC_STRM_ERROR:
      printf("STREAM ERROR\n");

      printf("DecOut.data_left %d \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.data_len - DecOut.data_left);
        DecIn.data_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;

      }
      break;

    default:
      decRet(ret);
      goto end2;
    }
    /*
     * While there is stream
     */

  } while(DecIn.data_len > 0);
end2:

  /* Output buffered images also... */
  START_SW_PERFORMANCE;
  decsw_performance();
  add_buffer_thread_run = 0;

  if(output_thread_run)
    Mpeg2DecEndOfStream(decoder, 1);

  if(output_thread) {
    pthread_join(*output_thread, NULL);
	free(output_thread);
    output_thread = NULL;
  }
  if(release_thread) {
    pthread_join(*release_thread, NULL);
	free(release_thread);
    release_thread = NULL;
  }
  END_SW_PERFORMANCE;
  decsw_performance();

  START_SW_PERFORMANCE;
  decsw_performance();
  Mpeg2DecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;
  decsw_performance();

  /*
   * Release the decoder
   */
  if(stream_mem.virtual_address)
    DWLFreeLinear(((Mpeg2DecContainer *) decoder)->dwl, &stream_mem);

  START_SW_PERFORMANCE;
  decsw_performance();
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  Mpeg2DecRelease(decoder);
  DWLRelease(dwl_inst);
  END_SW_PERFORMANCE;
  decsw_performance();

  if(Decinfo.frame_width < 1921)
    printf("\nWidth %d Height %d\n", Decinfo.frame_width,
           Decinfo.frame_height);
  if(cumulative_error_mbs) {
    printf("Cumulative errors: %d/%d macroblocks, ",
           cumulative_error_mbs,
           (Decinfo.frame_width >> 4) * (Decinfo.frame_height >> 4) *
           frame_number);
  }
  printf("decoded %d pictures\n", frame_number);

  if(f_in)
    fclose(f_in);

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(fout[i])
      fclose(fout[i]);
  }

  if(f_tiled_output)
    fclose(f_tiled_output);

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if(save_index || use_index) {
    fclose(findex);
  }

  /* Calculate the output size and print it  */
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    fout[i] = fopen(out_file_name[i], "rb");
    if(NULL == fout[i]) {
      stream_len = 0;
    } else {
      fseek(fout[i], 0L, SEEK_END);
      stream_len = (u32) ftell(fout[i]);
      fclose(fout[i]);
    }
  }

  printf("output size %d\n", stream_len);

  FINALIZE_SW_PERFORMANCE;

  if(cumulative_error_mbs || !frame_number) {
    printf("ERRORS FOUND\n");
    return (1);
  } else
    return (0);

}

/*------------------------------------------------------------------------------
        readDecodeUnit
        Description : search pic start code and read one decode unit at a time
------------------------------------------------------------------------------*/
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer) {

  u32 idx = 0, VopStart = 0;
  u8 temp = 0;
  u8 next_packet = 0;
  int ret = 0;

  StartCode = 0;

  if(stop_decoding) {
    printf("Truncated stream size reached -> stop decoding\n");
    return 0;
  }

  /* If enabled, loose the packets (skip this packet first though) */
  if(stream_packet_loss && disable_resync) {
    //u32 ret = 0;
	
    ret = TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, &next_packet);
    if(ret != 0) {
      printf("RANDOM STREAM ERROR FAILED\n");
      return 0;
    }
  }

  if(use_index) {
    u32 amount = 0;

    /* get next index */
    ret = fscanf(findex, "%zu", &next_index);
    amount = next_index - cur_index;

    /* read data */
    idx = fread(frame_buffer, 1, amount, fp);

    VopStart = 1;
    StartCode = ((frame_buffer[idx - 1] << 24) |
                 (frame_buffer[idx - 2] << 16) |
                 (frame_buffer[idx - 3] << 8) |
                 frame_buffer[idx - 4]);

    /* end of stream */
    if(next_index == stream_size) {
      strm_end = 1;
      idx += 4;
    }

    cur_index = next_index;
  } else {
    while(!VopStart) {

      ret = fread(&temp, sizeof(u8), 1, fp);

      if(feof(fp)) {

        fprintf(stdout, "TB: End of stream noticed in readDecodeUnit\n");
        strm_end = 1;
        idx += 4;
        break;
      }
      /* Reading the whole stream at once must be limited to buffer size */
      if((idx > (length - MPEG2_WHOLE_STREAM_SAFETY_LIMIT)) &&
          whole_stream_mode) {

        whole_stream_mode = 0;

      }

      frame_buffer[idx] = temp;

      if(idx >= (frame_buffer == byte_strm_start ? 4 : 3)) {
        if(!whole_stream_mode) {
          if(disable_resync) {
            /*-----------------------------------
                Slice by slice
            -----------------------------------*/
            if((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (frame_buffer[idx - 1] == 0x01)) {
              if(frame_buffer[idx] > 0x00) {
                if(frame_buffer[idx] <= 0xAF) {
                  VopStart = 1;
                  StartCode = ((frame_buffer[idx] << 24) |
                               (frame_buffer[idx - 1] << 16) |
                               (frame_buffer[idx - 2] << 8) |
                               frame_buffer[idx - 3]);
                  /*printf("SLICE FOUND\n");*/
                }
              } else if(frame_buffer[idx] == 0x00) {
                VopStart = 1;
                StartCode = ((frame_buffer[idx] << 24) |
                             (frame_buffer[idx - 1] << 16) |
                             (frame_buffer[idx - 2] << 8) |
                             frame_buffer[idx - 3]);
                /* MPEG2 start code found */
              }
            }
          } else {
            /*-----------------------------------
                MPEG2 Start code
            -----------------------------------*/
            if(((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (((frame_buffer[idx - 1] == 0x01) &&
                  (frame_buffer[idx] == 0x00))))) {
              VopStart = 1;
              StartCode = ((frame_buffer[idx] << 24) |
                           (frame_buffer[idx - 1] << 16) |
                           (frame_buffer[idx - 2] << 8) |
                           frame_buffer[idx - 3]);
              /* MPEG2 start code found */
            }
          }
        }
      }
      if(idx >= length) {
        fprintf(stdout, "idx = %d,lenght = %d \n", idx, length);
        fprintf(stdout, "TB: Out Of Stream Buffer\n");
        break;
      }
      if(idx > strm_rew + 128) {
        idx -= strm_rew;
      }
      idx++;
      /* stop reading if truncated stream size is reached */
      if(stream_truncate && !disable_resync) {
        if(previous_used + idx >= stream_size) {
          printf("Stream truncated at %d bytes\n", previous_used + idx);
          stop_decoding = 1;   /* next call return 0 size -> exit decoding main loop */
          break;
        }
      }
    }
  }

  if(save_index && !use_index) {
    fprintf(findex, "%" PRId64"\n", ftello64(fp));
  }

  trace_used_stream = previous_used;
  previous_used += idx;

  /* If we skip this packet */
  if(pic_rdy && next_packet && ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt)) {
    /* Get the next packet */
    printf("Packet Loss\n");
    return readDecodeUnit(fp, frame_buffer);
  } else {
    /*printf("READ DECODE UNIT %d\n", idx); */
    printf("No Packet Loss\n");
    if (disable_resync && pic_rdy && stream_truncate
        && ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt)) {
      i32 ret;
      printf("Original packet size %d\n", idx);
      ret = TBRandomizeU32(&idx);
      if(ret != 0) {
        printf("RANDOM STREAM ERROR FAILED\n");
        stop_decoding = 1;   /* next call return 0 size -> exit decoding main loop */
        return 0;
      }
      printf("Randomized packet size %d\n", idx);
    }
    return (idx);
  }
}

/*------------------------------------------------------------------------------
        printTimeCode
        Description : Print out time code
------------------------------------------------------------------------------*/

void printTimeCode(Mpeg2DecTime * timecode) {

  fprintf(stdout, "hours %u, "
          "minutes %u, "
          "seconds %u, "
          "time_pictures %u \n",
          timecode->hours,
          timecode->minutes, timecode->seconds, timecode->pictures);
}

/*------------------------------------------------------------------------------
        decRet
        Description : Print out Decoder return values
------------------------------------------------------------------------------*/

void decRet(Mpeg2DecRet ret) {
  static Mpeg2DecRet prev_ret = MPEG2DEC_NO_DECODING_BUFFER;
  if (ret == prev_ret) return;
  prev_ret = ret;
  printf("Decode result: ");

  switch (ret) {
  case MPEG2DEC_OK:
    printf("MPEG2DEC_OK\n");
    break;
  case MPEG2DEC_STRM_PROCESSED:
    printf("MPEG2DEC_STRM_PROCESSED\n");
    break;
  case MPEG2DEC_BUF_EMPTY:
    printf("MPEG2DEC_BUF_EMPTY\n");
    break;
  case MPEG2DEC_NO_DECODING_BUFFER:
    printf("MPEG2DEC_NO_DECODING_BUFFER\n");
    break;
  case MPEG2DEC_NONREF_PIC_SKIPPED:
    printf("MPEG2DEC_NONREF_PIC_SKIPPED\n");
    break;
  case MPEG2DEC_PIC_RDY:
    printf("MPEG2DEC_PIC_RDY\n");
    break;
  case MPEG2DEC_HDRS_RDY:
    printf("MPEG2DEC_HDRS_RDY\n");
    break;
  case MPEG2DEC_PIC_DECODED:
    printf("MPEG2DEC_PIC_DECODED\n");
    break;
  case MPEG2DEC_PARAM_ERROR:
    printf("MPEG2DEC_PARAM_ERROR\n");
    break;
  case MPEG2DEC_STRM_ERROR:
    printf("MPEG2DEC_STRM_ERROR\n");
    break;
  case MPEG2DEC_NOT_INITIALIZED:
    printf("MPEG2DEC_NOT_INITIALIZED\n");
    break;
  case MPEG2DEC_MEMFAIL:
    printf("MPEG2DEC_MEMFAIL\n");
    break;
  case MPEG2DEC_DWL_ERROR:
    printf("MPEG2DEC_DWL_ERROR\n");
    break;
  case MPEG2DEC_HW_BUS_ERROR:
    printf("MPEG2DEC_HW_BUS_ERROR\n");
    break;
  case MPEG2DEC_SYSTEM_ERROR:
    printf("MPEG2DEC_SYSTEM_ERROR\n");
    break;
  case MPEG2DEC_HW_TIMEOUT:
    printf("MPEG2DEC_HW_TIMEOUT\n");
    break;
  case MPEG2DEC_HDRS_NOT_RDY:
    printf("MPEG2DEC_HDRS_NOT_RDY\n");
    break;
  case MPEG2DEC_STREAM_NOT_SUPPORTED:
    printf("MPEG2DEC_STREAM_NOT_SUPPORTED\n");
    break;
  default:
    printf("Other %d\n", ret);
    break;
  }
}

/*------------------------------------------------------------------------------
        decNextPictureRet
        Description : Print out NextPicture return values
------------------------------------------------------------------------------*/
void decNextPictureRet(Mpeg2DecRet ret) {
  printf("next picture returns: ");

  decRet(ret);
}

/*------------------------------------------------------------------------------

    Function name:            printMpeg2PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printMpeg2PicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf(" DEC_PIC_TYPE_I,");
    break;
  case DEC_PIC_TYPE_P:
    printf(" DEC_PIC_TYPE_P,");
    break;
  case DEC_PIC_TYPE_B:
    printf(" DEC_PIC_TYPE_B,");
    break;
  case DEC_PIC_TYPE_D:
    printf(" DEC_PIC_TYPE_D,");
    break;
  default:
    printf("Other %d\n", pic_type);
    break;
  }
}


/*------------------------------------------------------------------------------
        printMpeg2Version
        Description : Print out decoder version info
------------------------------------------------------------------------------*/

void printMpeg2Version(void) {

  Mpeg2DecApiVersion dec_version;
  Mpeg2DecBuild dec_build;

  /*
   * Get decoder version info
   */

  dec_version = Mpeg2DecGetAPIVersion();
  printf("\nApi version:  %d.%d, ", dec_version.major, dec_version.minor);

  dec_build = Mpeg2DecGetBuild();
  printf("sw build nbr: %d, hw build nbr: %x\n\n",
         dec_build.sw_build, dec_build.hw_build);

}

/*------------------------------------------------------------------------------

    Function name: allocatePicBuffers

    Functional description: Allocates frame buffers

    Inputs:     DWLLinearMem * buffer       pointers stored here

    Outputs:    NONE

    Returns:    nonzero if err

------------------------------------------------------------------------------*/

i32 AllocatePicBuffers(struct DWLLinearMem * buffer, Mpeg2DecContainer * container) {

  u32 offset = (MPEG2_FRAME_BUFFER_SIZE + 0xFFF) & ~(0xFFF);
  u32 i = 0;

#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE

  if(DWLMallocRefFrm(((Mpeg2DecContainer *) container)->dwl,
                     offset * MPEG2_NUM_BUFFERS,
                     (struct DWLLinearMem *) buffer) != DWL_OK) {
    printf(("UNABLE TO ALLOCATE OUTPUT BUFFER MEMORY\n"));
    return 1;
  }

  buffer[1].virtual_address = buffer[0].virtual_address + offset / 4;
  buffer[1].bus_address = buffer[0].bus_address + offset;

  buffer[2].virtual_address = buffer[1].virtual_address + offset / 4;
  buffer[2].bus_address = buffer[1].bus_address + offset;

  for(i = 0; i < MPEG2_NUM_BUFFERS; i++) {
    printf("buff %d vir %p bus %zx\n", i,
           (void *)buffer[i].virtual_address, buffer[i].bus_address);
  }

#endif
  return 0;
}


/*------------------------------------------------------------------------------

    Function name:  WriteOutputLittleEndian

     Purpose:
     Write picture pointed by data to file. Size of the
     picture in pixels is indicated by pic_size. The data
     is converted to little endian.

------------------------------------------------------------------------------*/
void WriteOutputLittleEndian(u8 * data, u32 pic_size) {
  u32 chunks = 0;
  u32 i = 0;
  u32 word = 0;

  chunks = pic_size / 4;
  for(i = 0; i < chunks; ++i) {
    word = data[0];
    word <<= 8;
    word |= data[1];
    word <<= 8;
    word |= data[2];
    word <<= 8;
    word |= data[3];
    fwrite(&word, 4, 1, fout[0]);
    data += 4;
  }

  if(pic_size % 4 == 0) {
    return;
  } else if(pic_size % 4 == 1) {
    word = data[0];
    word <<= 24;
    fwrite(&word, 1, 1, fout[0]);
  } else if(pic_size % 4 == 2) {
    word = data[0];
    word <<= 8;
    word |= data[1];
    word <<= 16;
    fwrite(&word, 2, 1, fout[0]);
  } else if(pic_size % 4 == 3) {
    word = data[0];
    word <<= 8;
    word |= data[1];
    word <<= 8;
    word |= data[2];
    word <<= 8;
    fwrite(&word, 3, 1, fout[0]);
  }
}

/*------------------------------------------------------------------------------

    Function name:  Mpeg2DecTrace

    Purpose:
        Example implementation of Mpeg2DecTrace function. Prototype of this
        function is given in mpeg2decapi.h. This implementation appends
        trace messages to file named 'dec_api.trc'.

------------------------------------------------------------------------------*/
void Mpeg2DecTrace(const char *string) {
  FILE *fp;

  fp = fopen("dec_api.trc", "at");

  if(!fp)
    return;

  fwrite(string, 1, strlen(string), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);
}
