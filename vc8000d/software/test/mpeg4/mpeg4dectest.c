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
#include <unistd.h>
#include <inttypes.h>
#ifndef MP4DEC_EXTERNAL_ALLOC_DISABLE
#include <fcntl.h>
#include <sys/mman.h>
#endif

#include "dwl.h"
#include "mp4decapi.h"
#ifdef USE_EFENCE
#include "efence.h"
#endif

#include "mp4dechwd_container.h"
#include "regdrv.h"

#include "tb_cfg.h"
#include "tb_tiled.h"
#include "tb_params.h"
#include "tb_md5.h"
#include "tb_out.h"
#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "command_line_parser.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif
#define DEFAULT -1
#define VOP_START_CODE 0xb6010000
#define MAX_BUFFERS 16

/* Size of stream buffer */
#define STREAMBUFFER_BLOCKSIZE 2*2097151
#define MP4_WHOLE_STREAM_SAFETY_LIMIT (10*10*1024)

/* local function prototypes */

void printTimeCode(MP4DecTime * timecode);
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer, void *dec_inst);
void decRet(MP4DecRet ret);
void printMP4Version(void);
void GetUserData(MP4DecInst dec_inst,
                 MP4DecInput DecIn, MP4DecUserDataType type);

void printMpeg4PicCodingType(u32 pic_type);

void decsw_performance(void);
u32 MP4GetResyncLength(MP4DecInst dec_inst, u8 * p_strm);

/* stream start address */
u8 *byte_strm_start;

/* stream used in SW decode */
u32 trace_used_stream = 0;
u32 previous_used = 0;
u32 pp_tile_out = 0;
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;
/* SW/SW testing, read stream trace file */
FILE *f_stream_trace = NULL;

/* output file */
FILE *fout[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL};
FILE *f_tiled_output = NULL;
static u32 StartCode;

i32 strm_rew = 0;
u32 length = 0;
u32 write_output = 1;
u8 disable_h263 = 0;
u32 crop_output = 0;
u8 disable_resync = 1;
u8 strm_end = 0;
u8 *stream_stop = NULL;
u32 strm_offset = 4;

u32 stream_size = 0;
u32 stop_decoding = 0;
u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 pic_rdy = 0;
u32 hdrs_rdy = 0;
u32 stream_header_corrupt = 0;
struct TBCfg tb_cfg;

/* Give stream to decode as one chunk */
u32 whole_stream_mode = 0;
u32 cumulative_error_mbs = 0;

u32 use_peek_output = 0;
u32 skip_non_reference = 0;

u32 planar_output = 0;
u32 second_field[4] = {1, 1, 1, 1};         /* for field pictures, flag that
                              * whole frame ready after this field */
u32 custom_width = 0;
u32 custom_height = 0;
u32 custom_dimensions = 0; /* Implicates that raw bitstream does not carry
                           * frame dimensions */
u32 no_start_codes = 0;     /* If raw bitstream doesn't carry startcodes */

u32 md5sum = 0;

MP4DecStrmFmt strm_fmt = MP4DEC_MPEG4;
u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 convert_tiled_output = 0;
u32 dpb_mode = DEC_DPB_FRAME;
u32 convert_to_frame_dpb = 0;

FILE *findex = NULL;
u32 save_index = 0;
u32 use_index = 0;
off64_t cur_index = 0;
addr_t next_index = 0;
u32 ds_ratio_x, ds_ratio_y;
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

#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
extern u32 use_reference_idct;
#endif

#ifdef ASIC_TRACE_SUPPORT
extern u32 g_hw_ver;
#endif

#ifdef MPEG4_EVALUATION
extern u32 g_hw_ver;
#endif

MP4DecInst decoder;
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

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && num_buffers < MAX_BUFFERS) {
      struct DWLLinearMem mem;
      i32 dwl_ret;
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        MP4DecRet rv = MP4DecAddBuffer(decoder, &mem);
        if(rv != MP4DEC_OK && rv != MP4DEC_WAITING_FOR_BUFFER) {
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
  u32 i;
  printf("Releasing %d external frame buffers\n", num_buffers);
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    printf("Freeing buffer %p\n", (void*)ext_buffers[i].virtual_address);
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
MP4DecInfo Decinfo;
u32 end_of_stream = 0;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;

sem_t buf_release_sem;
MP4DecPicture buf_list[100];
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
      MP4DecPictureConsumed(decoder, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;

      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && num_buffers < MAX_BUFFERS) {
          struct DWLLinearMem mem;
          i32 dwl_ret;
          if (pp_enabled)
            dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          else
            dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          if(dwl_ret == DWL_OK) {
            MP4DecRet rv = MP4DecAddBuffer(decoder, &mem);
            if(rv != MP4DEC_OK && rv != MP4DEC_WAITING_FOR_BUFFER) {
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
    usleep(10000);
  }
  return (NULL);
}


/* Output thread entry point. */
static void* mpeg4_output_thread(void* arg) {
  MP4DecPicture DecPicture;
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
    MP4DecRet ret;
    ret = MP4DecNextPicture(decoder, &DecPicture, 0);
    if(ret == MP4DEC_PIC_RDY) {
      if (DecPicture.interlaced && DecPicture.field_picture &&
          DecPicture.top_field) {
        continue; // do not send one field frame twice
      }

      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (!DecPicture.pictures[i].frame_width ||
            !DecPicture.pictures[i].frame_height)
          continue;
        if(!use_peek_output) {
          decRet(ret);

          printf("PIC %d, %s", DecPicture.pic_id,
                 DecPicture.
                 key_picture ? "key picture,    " : "non key picture,");

          /* pic coding type */
          printMpeg4PicCodingType(DecPicture.pic_coding_type);

          if(DecPicture.field_picture)
            printf(" %s ",
                   DecPicture.top_field ? "top field.   " : "bottom field.");
          else
            printf(" frame picture. ");

          printTimeCode(&(DecPicture.time_code));
          if(DecPicture.nbr_of_err_mbs) {
            printf(", %d/%d error mbs\n",
                   DecPicture.nbr_of_err_mbs,
                   (DecPicture.pictures[0].frame_width >> 4) * (DecPicture.pictures[0].frame_height >> 4));
            cumulative_error_mbs += DecPicture.nbr_of_err_mbs;
          } else {
            printf("\n");

          }
          /* Write output picture to file */
          image_data = (u8 *) DecPicture.pictures[i].output_picture;

          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(DecPicture.pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(DecPicture.pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(DecPicture.pictures[i].output_format))
            pp_mono_chrome = 1;
          if (!IS_PIC_PACKED_RGB(DecPicture.pictures[i].output_format) &&
              !IS_PIC_PLANAR_RGB(DecPicture.pictures[i].output_format))
            WriteOutput(out_file_name[i], image_data,
                        DecPicture.pictures[i].coded_width,
                        DecPicture.pictures[i].coded_height,
                        pic_display_number - 1,
                        pp_mono_chrome, 0, 0, !write_output,
                        IS_PIC_TILE(DecPicture.pictures[i].output_format),
                        DecPicture.pictures[i].pic_stride, DecPicture.pictures[i].pic_stride_ch, i,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        convert_to_frame_dpb, dpb_mode, md5sum, &fout[i], 1);
          else
            WriteOutputRGB(out_file_name[i], image_data,
                           DecPicture.pictures[i].coded_width,
                           DecPicture.pictures[i].coded_height,
                           pic_display_number - 1,
                           pp_mono_chrome, 0, 0, !write_output,
                           IS_PIC_TILE(DecPicture.pictures[i].output_format),
                           DecPicture.pictures[i].pic_stride, DecPicture.pictures[i].pic_stride_ch, i,
                           IS_PIC_PLANAR_RGB(DecPicture.pictures[i].output_format), pp_cr_first,
                           convert_tiled_output, convert_to_frame_dpb, dpb_mode, md5sum, &fout[i], 1);
        }
      }

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = DecPicture;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);
      pic_display_number++;
    }

    else if(ret == MP4DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
  }
  return (NULL);
}

int main(int argc, char **argv) {
  FILE *f_tbcfg;
  u8 *p_strm_data = 0;
  u32 max_num_vops;

  u32 i, j, tmp = 0, stream_len = 0;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;
  u32 vop_number = 0, vp_num = 0;
  u32 rlc_mode = 0;
  u32 num_frame_buffers = 0;
  u32 cr_first = 0;
  struct MP4DecConfig dec_cfg;
  /*
   * Decoder API structures
   */
  //u32 prev_width = 0, prev_height = 0;
  u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  MP4DecBufferInfo hbuf;
  MP4DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  MP4DecRet ret;
  MP4DecRet info_ret;
  MP4DecInput DecIn;
  MP4DecOutput DecOut;
  MP4DecPicture DecPicture;
  struct DWLLinearMem stream_mem;
  u32 pic_id = 0;
  u8 *image_data;

  FILE *f_in = NULL;

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

#ifdef MPEG4_EVALUATION_8170
  g_hw_ver = 8170;
#elif MPEG4_EVALUATION_8190
  g_hw_ver = 8190;
#elif MPEG4_EVALUATION_9170
  g_hw_ver = 9170;
#elif MPEG4_EVALUATION_9190
  g_hw_ver = 9190;
#elif MPEG4_EVALUATION_G1
  g_hw_ver = 10000;
#endif

  if(argc < 2) {

    printf("\nx170 MPEG-4 Decoder Testbench\n\n");
    printf("USAGE:\n%s [options] stream.mpeg4\n", argv[0]);
    printf("\t-Ooutfile write output to \"outfile\" (default out.yuv)\n");
    printf("\t-Nn to decode only first n vops of the stream\n");
    printf("\t-X to not to write output picture\n");
    printf("\t-Bn to use n frame buffers in decoder\n");
    printf("\t-Sfile.hex stream control trace file\n");
#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
    printf("\t-R use reference decoder IDCT (sw/sw integration only)\n");
#endif
    printf("\t-W whole stream mode - give stream to decoder in one chunk\n");
    printf("\t-P write planar output(--planar)\n");
    printf("\t-a Enable PP planar output(--pp-planar)\n");
    printf("\t-I save index file\n");
    printf("\t-E use tiled reference frame format.\n");
    printf("\t-G convert tiled output pictures to raster scan\n");
    printf("\t-F decode Sorenson Spark stream\n");
    printf
    ("\t-C crop output picture to real picture dimensions (only planar)\n");
    printf("\t-J decode DivX4 or DivX5 stream\n");
    printf("\t-H<width>x<height> decode DivX3 stream of resolution width x height\n");
    printf("\t-Q Skip decoding non-reference pictures.\n");
    printf("\t-m Output md5 for each picture(--md5-per-pic)\n");
    printf("\t-Z output pictures using MP4DecPeek() function\n");
    printf("\t--separate-fields-in-dpb DPB stores interlaced content"\
           " as fields (default: frames)\n");
    printf("\t--output-frame-dpb Convert output to frame mode even if"\
           " field DPB mode used\n");
    printf("\t-e add extra external buffer randomly\n");
    printf("\t-k allocate extra external buffer in output thread\n");
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
    printMP4Version();
    exit(100);
  }

  max_num_vops = 0;
  for(i = 1; i < (u32)argc - 1; i++) {
    if(strncmp(argv[i], "-O", 2) == 0) {
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(out_file_name[j], argv[i] + 2);
    } else if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_vops = atoi(argv[i] + 2);
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
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = 1;
      pp_enabled = 1;
    } else if(strncmp(argv[i], "-B", 2) == 0) {
      num_frame_buffers = atoi(argv[i] + 2);
      if(num_frame_buffers > 16)
        num_frame_buffers = 16;
    }
#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
    else if(strncmp(argv[i], "-R", 2) == 0) {
      use_reference_idct = 1;
    }
#endif
    else if(strncmp(argv[i], "-W", 2) == 0) {
      whole_stream_mode = 1;
    } else if (strncmp(argv[i], "-E", 2) == 0)
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    else if (strncmp(argv[i], "-G", 2) == 0)
      convert_tiled_output = 1;
    else if(strcmp(argv[i], "-F") == 0) {
      strm_fmt = MP4DEC_SORENSON;
    } else if(strcmp(argv[i], "-J") == 0) {
      strm_fmt = MP4DEC_CUSTOM_1;
    } else if(strcmp(argv[i], "-Z") == 0) {
      use_peek_output = 1;
    } else if(strcmp(argv[i], "-Q") == 0) {
      skip_non_reference = 1;
    } else if(strcmp(argv[i], "-m") == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
    } else if(strncmp(argv[i], "-H", 2) == 0) {
      custom_dimensions = 1;
      no_start_codes = 1;
      tmp = sscanf(argv[i]+2, "%dx%d", (int*)&custom_width, (int*)&custom_height );
      if( tmp != 2 ) {
        printf("MALFORMED WIDTHxHEIGHT: %s\n", argv[i]+2);
        return 1;
      }
      strm_offset = 0;
#if 0
      /* If -O option not used, generate default file name */
      if(out_file_name[0] == 0) {
        if(planar_output && crop_output)
          sprintf(out_file_name, "out_%dx%d.yuv",
                  custom_width, custom_height);
        else
          sprintf(out_file_name, "out_%dx%d.yuv",
                  frame_width, frame_height );
      }
#endif
#if 0
    } else if(strcmp(argv[i], "-C") == 0) {
      crop_output = 1;
#endif
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
        fprintf(stdout, "Illegal scaled width/height: %s:%s\n", argv[i]+2, px+1);
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

  printMP4Version();
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

  rlc_mode = TBGetDecRlcModeForced(&tb_cfg);
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
  printf("Decoder RLC %d\n", rlc_mode);
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
  disable_resync = !TBGetTBPacketByPacket(&tb_cfg);
  printf("TB Packet by Packet  %d\n", !disable_resync);
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
  if(stream_truncate && disable_resync) {
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
#endif
  if(!tmp) {
    printf("UNABLE TO OPEN TRACE FILES(S)\n");
  }

  dwl_init.client_type = DWL_CLIENT_TYPE_MPEG4_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto end;
  }
  /*
   * Initialize the decoder
   */
  START_SW_PERFORMANCE;
  decsw_performance();
  ret = MP4DecInit(&decoder,
                   dwl_inst,
                   strm_fmt,
                   TBGetDecErrorConcealment( &tb_cfg ),
                   num_frame_buffers,
                   tiled_output |
                   (dpb_mode == DEC_DPB_INTERLACED_FIELD ? DEC_DPB_ALLOW_FIELD_ORDERING : 0), 1, 0);
  END_SW_PERFORMANCE;
  decsw_performance();

  if(ret != MP4DEC_OK) {
    decoder = NULL;
    printf("Could not initialize decoder\n");
    goto end;
  }

  /* Set ref buffer test mode */
  //((DecContainer *) decoder)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  /* Check if we have to supply decoder with custom frame dimensions */
  if(custom_dimensions) {
    START_SW_PERFORMANCE;
    decsw_performance();
    MP4DecSetCustomInfo( decoder, custom_width, custom_height );
    END_SW_PERFORMANCE;
    decsw_performance();
    TBSetRefbuMemModel( &tb_cfg,
                        ((DecContainer *) decoder)->mp4_regs,
                        &((DecContainer *) decoder)->ref_buffer_ctrl );
  }

  DecIn.enable_deblock = TBGetPpFilterEnabled(&tb_cfg);
#ifdef PP_PIPELINE_ENABLED
  /* Initialize the post processer. If unsuccessful -> exit */
  if(pp_startup
      (argv[argc - 1], decoder, PP_PIPELINED_DEC_TYPE_MPEG4, &tb_cfg) != 0) {
    fprintf(stdout, "PP INITIALIZATION FAILED\n");
    goto end;
  }
  if(pp_update_config
      (decoder, PP_PIPELINED_DEC_TYPE_MPEG4, &tb_cfg) == CFG_UPDATE_FAIL)

  {
    fprintf(stdout, "PP CONFIG LOAD FAILED\n");
    goto end;
  }
#endif

  if(DWLMallocLinear(((DecContainer *) decoder)->dwl,
                     STREAMBUFFER_BLOCKSIZE, &stream_mem) != DWL_OK) {
    printf(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end;
  }

  byte_strm_start = (u8 *) stream_mem.virtual_address;

  DecIn.stream = byte_strm_start;
  DecIn.stream_bus_address =  stream_mem.bus_address;


  stream_stop = byte_strm_start + length;
  /* NOTE: The registers should not be used outside decoder SW for other
   * than compile time setting test purposes */
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
                 */
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((DecContainer *) decoder)->mp4_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  if(rlc_mode) {
    printf("RLC mode forced\n");
    /*Force the decoder into RLC mode */
    ((DecContainer *) decoder)->rlc_mode = 1;
  }

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  decsw_performance();
  info_ret = MP4DecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(info_ret) {
    decRet(info_ret);
  }
  if (custom_dimensions) {
    ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                           !tb_cfg.pp_params.pipeline_e);
    if (params.fscale_cfg.fixed_scale_enabled) {
      u32 crop_w = params.ppu_cfg[0].crop.width;
      u32 crop_h = params.ppu_cfg[0].crop.height;
      if (!crop_w) crop_w = Decinfo.frame_width;
      if (!crop_h) crop_h = Decinfo.frame_height;
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
    tmp = MP4DecSetInfo(decoder, &dec_cfg);
    if (tmp != MP4DEC_OK) {
      goto end;
    }
  }
  /*
      fout = fopen(out_file_name, "wb");
      if(fout == NULL)
      {

          printf("Could not open output file\n");
          goto end2;
      }*/
#ifdef ASIC_TRACE_SUPPORT
  if(1 == info_ret)
    decoding_tools.short_video = 1;
#endif

  p_strm_data = (u8 *) DecIn.stream;
  DecIn.skip_non_reference = skip_non_reference;

  /* Read vop headers */

  do {
    stream_len = readDecodeUnit(f_in, p_strm_data, decoder);
  } while (!feof(f_in) && stream_len && stream_len <= 4);

  i = StartCode;
  /* decrease 4 because previous function call
   * read the first VOP start code */

  if( !no_start_codes )
    stream_len -= 4;
  DecIn.data_len = stream_len;
  DecOut.data_left = 0;
  printf("Start decoding\n");
  do {
    /*printf("DecIn.data_len %d\n", DecIn.data_len);*/
    DecIn.pic_id = pic_id;
    if(ret != MP4DEC_STRM_PROCESSED &&
        ret != MP4DEC_BUF_EMPTY &&
        ret != MP4DEC_NO_DECODING_BUFFER &&
        ret != MP4DEC_NONREF_PIC_SKIPPED)
      printf("Starting to decode picture ID %d\n", pic_id);

    if(rlc_mode) {
      printf("RLC mode forced \n");
      /*Force the decoder into RLC mode */
      ((DecContainer *) decoder)->rlc_mode = 1;
    }

    /* If enabled, break the stream */
    if(stream_bit_swap) {
      if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
        /* Picture needs to be ready before corrupting next picture */
        if(pic_rdy && corrupted_bytes <= 0) {
          ret = TBRandomizeBitSwapInStream((u8*)DecIn.stream,
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
    ret = MP4DecDecode(decoder, &DecIn, &DecOut);
    END_SW_PERFORMANCE;
    decsw_performance();

    decRet(ret);

    /*
     * Choose what to do now based on the decoder return value
     */

    switch (ret) {
    case MP4DEC_HDRS_RDY:
    case MP4DEC_DP_HDRS_RDY:

      /* Set a flag to indicate that headers are ready */
      rv = MP4DecGetBufferInfo(decoder, &hbuf);
      printf("Mpeg4DecGetBufferInfo ret %d\n", rv);
      printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
      hdrs_rdy = 1;
      TBSetRefbuMemModel( &tb_cfg, ((DecContainer *) decoder)->mp4_regs,
                          &((DecContainer *) decoder)->ref_buffer_ctrl );

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      info_ret = MP4DecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(info_ret) {
        decRet(info_ret);
      }

      dpb_mode = Decinfo.dpb_mode;

      if (Decinfo.interlaced_sequence)
        printf("INTERLACED SEQUENCE\n");
#if 0
      /* If -O option not used, generate default file name */
      if(out_file_name[0] == 0) {
        if(planar_output && crop_output)
          sprintf(out_file_name, "out_%dx%d.yuv",
                  Decinfo.coded_width, Decinfo.coded_height);
        else
          sprintf(out_file_name, "out_x%dx%d.yuv",
                  Decinfo.frame_width, Decinfo.frame_height);
      }
#endif

      if(!vop_number) {
        /*disable_resync = 1; */
        disable_h263 = !Decinfo.stream_format;
        if(Decinfo.stream_format)
          printf("%s stream\n",
                 Decinfo.stream_format ==
                 1 ? "MPEG-4 short video" : "h.263");
        else
          printf("MPEG-4 stream\n");

        printf("Profile and level %d\n",
               Decinfo.profile_and_level_indication);
        printf("Pixel Aspect ratio %d : %d\n",
               Decinfo.par_width, Decinfo.par_height);
        printf("Output format %s\n",
               Decinfo.output_format == MP4DEC_SEMIPLANAR_YUV420
               ? "MP4DEC_SEMIPLANAR_YUV420" : "MP4DEC_TILED_YUV420");
      } else {
        MP4DecEndOfStream(decoder, 0);
      }
      /* get user data if present.
       * Valid for only current stream buffer */
      if(Decinfo.user_data_voslen != 0)
        GetUserData(decoder, DecIn, MP4DEC_USER_DATA_VOS);
      if(Decinfo.user_data_visolen != 0)
        GetUserData(decoder, DecIn, MP4DEC_USER_DATA_VISO);
      if(Decinfo.user_data_vollen != 0)
        GetUserData(decoder, DecIn, MP4DEC_USER_DATA_VOL);

      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);
      if (params.fscale_cfg.fixed_scale_enabled &&
          !params.ppu_cfg[0].scale.enabled) {
        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;
        if (!crop_w) crop_w = Decinfo.frame_width;
        if (!crop_h) crop_h = Decinfo.frame_height;
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
      dec_cfg.fixed_scale_enabled = params.fscale_cfg.fixed_scale_enabled;
      tmp = MP4DecSetInfo(decoder, &dec_cfg);
      if (tmp != MP4DEC_OK) {
        goto end;
      }
      //prev_width = Decinfo.frame_width;
      //prev_height = Decinfo.frame_height;
      /*printf("DecOut.data_left %d \n", DecOut.data_left);*/
      if(DecOut.data_left && !no_start_codes) {
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
          stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          if(!feof(f_in) && stream_len && stream_len <= 4 &&
              !no_start_codes ) {
            for ( i = 0 ; i < 4 ; ++i )
              p_strm_data[i] = p_strm_data[4+i];
            stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          }
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;
        DecOut.data_left = 0;

        corrupted_bytes = 0;

      }
      break;
    case MP4DEC_WAITING_FOR_BUFFER:
      rv = MP4DecGetBufferInfo(decoder, &hbuf);
      printf("MREG4DecGetBufferInfo ret %d\n", rv);
      printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
      while (rv == MP4DEC_WAITING_FOR_BUFFER) {
        if (hbuf.buf_to_free.virtual_address != NULL) {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
        rv = MP4DecGetBufferInfo(decoder, &hbuf);
      }
      min_buffer_num = hbuf.buf_num;
      if(hbuf.next_buf_size && buffer_release_flag) {
        /* Only add minimum required buffers at first. */
        //extra_buffer_num = hbuf.buf_num - min_buffer_num;
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;
        for(i=0; i<min_buffer_num; i++) {
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = MP4DecAddBuffer(decoder, &mem);
          printf("MP4DecAddBuffer ret %d\n", rv);
          if(rv != MP4DEC_OK && rv != MP4DEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            ext_buffers[i] = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = min_buffer_num;
        add_extra_flag = 1;
      }
      break;
    case MP4DEC_PIC_DECODED:
      /* Picture is now ready */
      pic_rdy = 1;
      /* Read what kind of stream is coming */
      pic_id++;
#if 0
      if (pic_id == 10) {
        info_ret = MP4DecAbort(decoder);
        info_ret = MP4DecAbortAfter(decoder);
        pic_id = 0;
        rewind(f_in);
        do {
          stream_len = readDecodeUnit(f_in, p_strm_data, decoder);
        } while (!feof(f_in) && stream_len && stream_len <= 4);

        i = StartCode;
        /* decrease 4 because previous function call
        * read the first VOP start code */

        if( !no_start_codes )
        stream_len -= 4;
        DecIn.data_len = stream_len;
        DecOut.data_left = 0;
        break;
      }
#endif

      START_SW_PERFORMANCE;
      decsw_performance();
      info_ret = MP4DecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(info_ret) {
        decRet(info_ret);
      }

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, mpeg4_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if (use_peek_output &&
          MP4DecPeek(decoder, &DecPicture) == MP4DEC_PIC_RDY) {

        printf("next picture returns:");
        decRet(ret);

        printf("PIC %d, %s", DecPicture.pic_id,
               DecPicture.
               key_picture ? "key picture,    " : "non key picture,");

        /* pic coding type */
        printMpeg4PicCodingType(DecPicture.pic_coding_type);

        if(DecPicture.field_picture)
          printf(" %s ",
                 DecPicture.top_field ? "top field.   " : "bottom field.");
        else
          printf(" frame picture. ");

        printTimeCode(&(DecPicture.time_code));
        if(DecPicture.nbr_of_err_mbs) {
          printf(", %d/%d error mbs\n",
                 DecPicture.nbr_of_err_mbs,
                 (DecPicture.pictures[0].frame_width >> 4) * (DecPicture.pictures[0].frame_height >> 4));
          cumulative_error_mbs += DecPicture.nbr_of_err_mbs;
        } else {
          printf("\n");

        }

        /* Write output picture to file */
        image_data = (u8 *) DecPicture.pictures[0].output_picture;
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(DecPicture.pictures[0].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(DecPicture.pictures[0].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(DecPicture.pictures[0].output_format))
          pp_mono_chrome = 1;
        if (!IS_PIC_PACKED_RGB(DecPicture.pictures[0].output_format) &&
            !IS_PIC_PLANAR_RGB(DecPicture.pictures[0].output_format))
          WriteOutput(out_file_name[0], image_data,
                      DecPicture.pictures[0].coded_width,
                      DecPicture.pictures[0].coded_height,
                      vop_number,
                      pp_mono_chrome, 0, 0, !write_output,
                      IS_PIC_TILE(DecPicture.pictures[0].output_format),
                      DecPicture.pictures[0].pic_stride, DecPicture.pictures[0].pic_stride_ch, i,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      convert_to_frame_dpb, dpb_mode, md5sum, &fout[0], 1);
        else
          WriteOutputRGB(out_file_name[0], image_data,
                         DecPicture.pictures[0].coded_width,
                         DecPicture.pictures[0].coded_height,
                         vop_number,
                         pp_mono_chrome, 0, 0, !write_output,
                         IS_PIC_TILE(DecPicture.pictures[0].output_format),
                         DecPicture.pictures[0].pic_stride, DecPicture.pictures[0].pic_stride_ch, i,
                         IS_PIC_PLANAR_RGB(DecPicture.pictures[0].output_format), pp_cr_first,
                         convert_tiled_output, convert_to_frame_dpb, dpb_mode, md5sum, &fout[0], 1);
      }

      vop_number++;
      vp_num = 0;
      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      /*printf("DecOut.data_left %d \n", DecOut.data_left);*/
      if(DecOut.data_left && !no_start_codes) {
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

          printf("READ NEXT DECODE UNIT\n");

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          if(!feof(f_in) && stream_len && stream_len <= 4 &&
              !no_start_codes) {
            for ( i = 0 ; i < 4 ; ++i )
              p_strm_data[i] = p_strm_data[4+i];
            stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          }
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;
        DecOut.data_left = 0;

        corrupted_bytes = 0;

      }

      if(max_num_vops && (vop_number >= max_num_vops)) {
        printf("\n\nMax num of pictures reached\n\n");
        DecIn.data_len = 0;
        goto end2;
      }

      break;

    case MP4DEC_STRM_PROCESSED:
    case MP4DEC_BUF_EMPTY:
    case MP4DEC_NONREF_PIC_SKIPPED:
      fprintf(stdout,
              "TB: Video packet Number: %u, vop: %d\n", vp_num++,
              vop_number);
    /* Used to indicate that picture decoding needs to
     * finalized prior to corrupting next picture */

    /* fallthrough */

#ifdef GET_FREE_BUFFER_NON_BLOCK
    case MP4DEC_NO_DECODING_BUFFER:
    /* Just for simulation: if no buffer, sleep 0.5 second and try decoding again. */
#endif

    case MP4DEC_OK:

      if((ret == MP4DEC_OK) && (StartCode == VOP_START_CODE)) {
        fprintf(stdout, "\nTb: ...::::!! The decoder missed"
                "a VOP startcode !!::::...\n\n");
      }

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      info_ret = MP4DecGetInfo(decoder, &Decinfo);
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

      /*printf("DecOut.data_left %d \n", DecOut.data_left);*/
      if(DecOut.data_left && (!no_start_codes || ret == MP4DEC_NO_DECODING_BUFFER)) {
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
          stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          if(!feof(f_in) && stream_len && stream_len <= 4 &&
              !no_start_codes) {
            for ( i = 0 ; i < 4 ; ++i )
              p_strm_data[i] = p_strm_data[4+i];
            stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          }
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;
        DecOut.data_left = 0;

        corrupted_bytes = 0;

      }

      break;

    case MP4DEC_VOS_END:
      printf("Video object seq end\n");
      /*DecIn.data_len = 0;*/
      /*printf("DecOut.data_left %d \n", DecOut.data_left);*/
      {

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
          stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          if(!feof(f_in) && stream_len && stream_len <= 4 &&
              !no_start_codes) {
            for ( i = 0 ; i < 4 ; ++i )
              p_strm_data[i] = p_strm_data[4+i];
            stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
          }                    /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.data_len = stream_len;
        DecOut.data_left = 0;

        corrupted_bytes = 0;
      }

      break;

    case MP4DEC_PARAM_ERROR:
      printf("INCORRECT STREAM PARAMS\n");
      goto end2;
      break;

    case MP4DEC_STRM_ERROR:
      /* Used to indicate that picture decoding needs to
       * finalized prior to corrupting next picture
      pic_rdy = 0;*/
      printf("STREAM ERROR\n");
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
        stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
        if(!feof(f_in) && stream_len && stream_len <= 4 &&
            !no_start_codes) {
          for ( i = 0 ; i < 4 ; ++i )
            p_strm_data[i] = p_strm_data[4+i];
          stream_len = readDecodeUnit(f_in, p_strm_data + strm_offset, decoder);
        }
        /*stream_packet_loss = streamPacketLossTmp;*/
      }
      DecIn.data_len = stream_len;
      DecOut.data_left = 0;
      /*goto end2; */
      break;

    default:
      goto end2;
    }
    /*
     * While there is stream
     */

  } while(DecIn.data_len > 0);
end2:

  if(output_thread_run)
    MP4DecEndOfStream(decoder, 1);
  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  START_SW_PERFORMANCE;
  decsw_performance();
  MP4DecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;
  decsw_performance();

end:
  /*
   * Release the decoder
   */
  add_buffer_thread_run = 0;
  if(stream_mem.virtual_address)
    DWLFreeLinear(((DecContainer *) decoder)->dwl, &stream_mem);

  START_SW_PERFORMANCE;
  decsw_performance();
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  if (decoder != NULL) MP4DecRelease(decoder);
  DWLRelease(dwl_inst);
  END_SW_PERFORMANCE;
  decsw_performance();

  if(Decinfo.frame_width < 1921)
    printf("\nWidth %d Height %d (Cropped %dx%d)\n", Decinfo.frame_width,
           Decinfo.frame_height, Decinfo.coded_width, Decinfo.coded_height);

  if(cumulative_error_mbs) {
    printf("Cumulative errors: %d/%d macroblocks, ",
           cumulative_error_mbs,
           (Decinfo.frame_width >> 4) * (Decinfo.frame_height >> 4) *
           vop_number);
  }
  printf("decoded %d pictures\n", vop_number);

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
    if (stream_len) {
      printf( "Output file: %s\n", out_file_name[i]);
      printf( "OUTPUT_SIZE %d\n", stream_len);
    }
  }
  FINALIZE_SW_PERFORMANCE;

  if(cumulative_error_mbs || !vop_number) {
    printf("ERRORS FOUND\n");
    return (1);
  } else
    return (0);
}


/*------------------------------------------------------------------------------
        readDecodeUnitNoSc
        Description : search VOP start code  or video
        packet start code and read one decode unit at a time
------------------------------------------------------------------------------*/
static u32 readDecodeUnitNoSc(FILE * fp, u8 * frame_buffer, void *dec_inst) {
  u8  size_tmp[4];
  u32 size, tmp;
  static off64_t offset = 0;
  int ret;

  if(use_index) {
    ret = fscanf(findex, "%zu", &next_index);
    (void) ret;

	if(feof(findex)) {
      strm_end = 1;
      return 0;
    }

    if(ftello64(fp) != next_index) {
      fseek(fp, next_index, SEEK_SET);
    }

    ret = fread(&size_tmp, sizeof(u8), 4, fp);
    (void) ret;
  } else {
    /* skip "00dc" from frame beginning (may signal video chunk start code).
     * also skip "0000" in case stream contains zero-size packets */
    for(;;) {
      fseeko64( fp, offset, SEEK_SET );
      if (fread( &size_tmp, sizeof(u8), 4, fp ) != 4)
        break;
      if( ( size_tmp[0] == '0' &&
            size_tmp[1] == '0' &&
            size_tmp[2] == 'd' &&
            size_tmp[3] == 'c' ) ||
          ( size_tmp[0] == 0x0 &&
            size_tmp[1] == 0x0 &&
            size_tmp[2] == 0x0 &&
            size_tmp[3] == 0x0 ) ) {
        offset += 4;
        continue;
      }
      break;
    }
  }

  size = (size_tmp[0]) +
         (size_tmp[1] << 8) +
         (size_tmp[2] << 16) +
         (size_tmp[3] << 24);

  if( size == (u32)-1 ) {
    strm_end = 1;
    return 0;
  }

  if(save_index && !use_index) {
    fprintf(findex, "%" PRId64"\n", offset);
  }

  tmp = fread( frame_buffer, sizeof(u8), size, fp );
  if( size != tmp ) {
    strm_end = 1;
    return 0;
  }

  offset += size + 4;
  return size;

}

/*------------------------------------------------------------------------------
        readDecodeUnit
        Description : search VOP start code  or video
        packet start code and read one decode unit at a time
------------------------------------------------------------------------------*/
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer, void *dec_inst) {

  u32 idx = 0, VopStart = 0;
  u8 temp;
  u8 next_packet = 0;
  static u32 resync_marker_length = 0;
  int ret;

  if( no_start_codes ) {
    return readDecodeUnitNoSc(fp, frame_buffer, dec_inst);
  }

  StartCode = 0;

  if(stop_decoding) {
    printf("Truncated stream size reached -> stop decoding\n");
    return 0;
  }

  /* If enabled, lose the packets (skip this packet first though) */
  if(stream_packet_loss && !disable_resync) {
    u32 ret = 0;

    ret =
      TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, &next_packet);
    if(ret != 0) {
      printf("RANDOM STREAM ERROR FAILED\n");
      return 0;
    }
  }

  if(use_index) {
    u32 amount = 0;

    /* read index */
	ret = fscanf(findex, "%zu", &next_index);
	(void) ret;

    amount = next_index - cur_index;

    idx = fread(frame_buffer, 1, amount, fp);

    /* start code */
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
      (void) ret;

      if(feof(fp)) {

        fprintf(stdout, "TB: End of stream noticed in readDecodeUnit\n");
        strm_end = 1;
        idx += 4;
        break;
      }
      /* Reading the whole stream at once must be limited to buffer size */
      if((idx > (length - MP4_WHOLE_STREAM_SAFETY_LIMIT)) && whole_stream_mode) {

        whole_stream_mode = 0;

      }

      frame_buffer[idx] = temp;

      if(idx >= 3) {

        if(!whole_stream_mode) {

          /*-----------------------------------
              H263 Start code

          -----------------------------------*/
          if((strm_fmt == MP4DEC_SORENSON) && (idx >= 7)) {
            if((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                ((frame_buffer[idx - 1] & 0xF8) == 0x80)) {
              VopStart = 1;
              StartCode = ((frame_buffer[idx] << 24) |
                           (frame_buffer[idx - 1] << 16) |
                           (frame_buffer[idx - 2] << 8) |
                           frame_buffer[idx - 3]);
            }
          } else if(!disable_h263 && (idx >= 7)) {
            if((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                ((frame_buffer[idx - 1] & 0xFC) == 0x80)) {
              VopStart = 1;
              StartCode = ((frame_buffer[idx] << 24) |
                           (frame_buffer[idx - 1] << 16) |
                           (frame_buffer[idx - 2] << 8) |
                           frame_buffer[idx - 3]);
            }
          }
          /*-----------------------------------
              MPEG4 Start code
          -----------------------------------*/
          if(((frame_buffer[idx - 3] == 0x00) &&
              (frame_buffer[idx - 2] == 0x00) &&
              (((frame_buffer[idx - 1] == 0x01) &&
                (frame_buffer[idx] == 0xB6))))) {
            VopStart = 1;
            StartCode = ((frame_buffer[idx] << 24) |
                         (frame_buffer[idx - 1] << 16) |
                         (frame_buffer[idx - 2] << 8) |
                         frame_buffer[idx - 3]);
            /* if MPEG4 start code found,
             * no need to look for H263 start code */
            disable_h263 = 1;
            resync_marker_length = 0;

          }

          /*-----------------------------------
              resync_marker
          -----------------------------------*/

          else if(disable_h263 && !disable_resync) {
            if((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (frame_buffer[idx - 1] > 0x01)) {

              if(resync_marker_length == 0) {
                resync_marker_length = MP4GetResyncLength(dec_inst,
                                       frame_buffer);
              }
              if((frame_buffer[idx - 1] >> (24 - resync_marker_length))
                  == 0x1) {
                VopStart = 1;
                StartCode = ((frame_buffer[idx] << 24) |
                             (frame_buffer[idx - 1] << 16) |
                             (frame_buffer[idx - 2] << 8) |
                             frame_buffer[idx - 3]);
              }
            }
          }
          /*-----------------------------------
              VOS end code
          -----------------------------------*/
          if(idx >= 7) {
            if(((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (((frame_buffer[idx - 1] == 0x01) &&
                  (frame_buffer[idx] == 0xB1))))) {

              /* stream end located */
              VopStart = 1;
              StartCode = ((frame_buffer[idx] << 24) |
                           (frame_buffer[idx - 1] << 16) |
                           (frame_buffer[idx - 2] << 8) |
                           frame_buffer[idx - 3]);
            }
          }

        }
      }
      if(idx >= length) {
        fprintf(stdout, "idx = %d,lenght = %d \n", idx, length);
        fprintf(stdout, "TB: Out Of Stream Buffer\n");
        break;
      }
      if(idx > (u32)strm_rew + 128) {
        idx -= strm_rew;
      }
      idx++;
      /* stop reading if truncated stream size is reached */
      if(stream_truncate && disable_resync) {
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
    return readDecodeUnit(fp, frame_buffer, dec_inst);
  } else {
    /*printf("No Packet Loss\n");*/
    if (!disable_resync && pic_rdy && stream_truncate && ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt)) {
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

void printTimeCode(MP4DecTime * timecode) {

  fprintf(stdout, "hours %u, "
          "minutes %u, "
          "seconds %u, "
          "time_incr %u, "
          "time_res %u",
          timecode->hours,
          timecode->minutes,
          timecode->seconds, timecode->time_incr, timecode->time_res);
}

/*------------------------------------------------------------------------------
        decRet
        Description : Print out Decoder return values
------------------------------------------------------------------------------*/

void decRet(MP4DecRet ret) {

  static MP4DecRet prev_retval = 0xFFFFFF;

  if (MP4DEC_NO_DECODING_BUFFER != ret)
    printf("Decode result: ");

  switch (ret) {
  case MP4DEC_OK:
    printf("MP4DEC_OK\n");
    break;
  case MP4DEC_STRM_PROCESSED:
    printf("MP4DEC_STRM_PROCESSED\n");
    break;
#ifdef GET_FREE_BUFFER_NON_BLOCK
  case MP4DEC_NO_DECODING_BUFFER:
    if (prev_retval == MP4DEC_NO_DECODING_BUFFER) break;

    printf("Decode result: MP4DEC_NO_DECODING_BUFFER\n");
    break;
#endif
  case MP4DEC_BUF_EMPTY:
    printf("MP4DEC_BUF_EMPTY\n");
    break;
  case MP4DEC_NONREF_PIC_SKIPPED:
    printf("MP4DEC_NONREF_PIC_SKIPPED\n");
    break;
  case MP4DEC_PIC_RDY:
    printf("MP4DEC_PIC_RDY\n");
    break;
  case MP4DEC_HDRS_RDY:
    printf("MP4DEC_HDRS_RDY\n");
    break;
  case MP4DEC_DP_HDRS_RDY:
    printf("MP4DEC_DP_HDRS_RDY\n");
    break;
  case MP4DEC_PARAM_ERROR:
    printf("MP4DEC_PARAM_ERROR\n");
    break;
  case MP4DEC_STRM_ERROR:
    printf("MP4DEC_STRM_ERROR\n");
    break;
  case MP4DEC_NOT_INITIALIZED:
    printf("MP4DEC_NOT_INITIALIZED\n");
    break;
  case MP4DEC_MEMFAIL:
    printf("MP4DEC_MEMFAIL\n");
    break;
  case MP4DEC_DWL_ERROR:
    printf("MP4DEC_DWL_ERROR\n");
    break;
  case MP4DEC_HW_BUS_ERROR:
    printf("MP4DEC_HW_BUS_ERROR\n");
    break;
  case MP4DEC_SYSTEM_ERROR:
    printf("MP4DEC_SYSTEM_ERROR\n");
    break;
  case MP4DEC_HW_TIMEOUT:
    printf("MP4DEC_HW_TIMEOUT\n");
    break;
  case MP4DEC_HDRS_NOT_RDY:
    printf("MP4DEC_HDRS_NOT_RDY\n");
    break;
  case MP4DEC_PIC_DECODED:
    printf("MP4DEC_PIC_DECODED\n");
    break;
  case MP4DEC_FORMAT_NOT_SUPPORTED:
    printf("MP4DEC_FORMAT_NOT_SUPPORTED\n");
    break;
  case MP4DEC_STRM_NOT_SUPPORTED:
    printf("MP4DEC_STRM_NOT_SUPPORTED\n");
    break;
  default:
    printf("Other %d\n", ret);
    break;
  }
  prev_retval = ret;
}

/*------------------------------------------------------------------------------

    Function name:            printMpeg4PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printMpeg4PicCodingType(u32 pic_type) {
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
  default:
    printf("Other %d\n", pic_type);
    break;
  }
}

/*------------------------------------------------------------------------------

    Function name: printMP4Version

    Functional description: Print version info

    Inputs:

    Outputs:    NONE

    Returns:    NONE

------------------------------------------------------------------------------*/
void printMP4Version(void) {

  MP4DecApiVersion dec_version;
  MP4DecBuild dec_build;

  /*
   * Get decoder version info
   */

  dec_version = MP4DecGetAPIVersion();
  printf("\nApi version:  %d.%d, ", dec_version.major, dec_version.minor);

  dec_build = MP4DecGetBuild();
  printf("sw build nbr: %d, hw build nbr: %x\n\n",
         dec_build.sw_build, dec_build.hw_build);

}

/*------------------------------------------------------------------------------

    Function name: GetUserData

    Functional description: This function is used to get user data from the
                            decoder.

    Inputs:     MP4DecInst dec_inst       decoder instance
                MP4DecUserDataType type   user data type to read

    Outputs:    NONE

    Returns:    NONE

------------------------------------------------------------------------------*/
void GetUserData(MP4DecInst dec_inst,
                 MP4DecInput DecIn, MP4DecUserDataType type) {
  u32 tmp;
  MP4DecUserConf user_data_config;
  MP4DecInfo dec_info;
  u8 *data = NULL;
  u32 size = 0;

  /* get info from the decoder */
  tmp = MP4DecGetInfo(dec_inst, &dec_info);
  if(tmp != 0) {
    printf(("ERROR, exiting...\n"));
  }
  switch (type) {
  case MP4DEC_USER_DATA_VOS:
    size = dec_info.user_data_voslen;
    data = (u8 *) calloc(size + 1, sizeof(u8));
    user_data_config.p_user_data_vos = data;
    user_data_config.user_data_vosmax_len = size;
    break;
  case MP4DEC_USER_DATA_VISO:
    size = dec_info.user_data_visolen;
    data = (u8 *) calloc(size + 1, sizeof(u8));
    user_data_config.p_user_data_viso = data;
    user_data_config.user_data_visomax_len = size;
    break;
  case MP4DEC_USER_DATA_VOL:
    size = dec_info.user_data_vollen;
    data = (u8 *) calloc(size + 1, sizeof(u8));
    user_data_config.p_user_data_vol = data;
    user_data_config.user_data_volmax_len = size;
    break;
  case MP4DEC_USER_DATA_GOV:
    size = dec_info.user_data_govlen;
    data = (u8 *) calloc(size + 1, sizeof(u8));
    user_data_config.p_user_data_gov = data;
    user_data_config.user_data_govmax_len = size;

    printf("VOS user data size: %d\n", size);
    break;
  default:
    break;
  }
  user_data_config.user_data_type = type;

  /* get user data */
  tmp = MP4DecGetUserData(dec_inst, &DecIn, &user_data_config);
  if(tmp != 0) {
    printf("ERROR, exiting...\n");
  }

  /* print user data */
  if(type == MP4DEC_USER_DATA_VOS)
    printf("VOS user data: %s\n", data);
  else if(type == MP4DEC_USER_DATA_VISO)
    printf("VISO user data: %s\n", data);
  else if(type == MP4DEC_USER_DATA_VOL)
    printf("VOL user data: %s\n", data);
  else if(type == MP4DEC_USER_DATA_GOV) {
    printf("\nGov user data: %s\n", data);
    fflush(stdout);
  }
  /* free allocated memory */
  if(data)
    free(data);
}

/*------------------------------------------------------------------------------

    Function name: MP4DecTrace

    Functional description: API trace implementation

    Inputs: string

    Outputs:

    Returns: void

------------------------------------------------------------------------------*/
void MP4DecTrace(const char *string) {
  printf("%s", string);
}

/*------------------------------------------------------------------------------

    Function name: decsw_performance

    Functional description: breakpoint for performance

    Inputs:  void

    Outputs:

    Returns: void

------------------------------------------------------------------------------*/
void decsw_performance(void) {
}
