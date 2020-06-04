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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

#include "vp6decapi.h"
#include "tb_cfg.h"
#include "tb_tiled.h"
#include "vp6hwd_container.h"
#include "tb_stream_corrupt.h"
#include "command_line_parser.h"
#include "regdrv.h"
#include "tb_out.h"
#include "md5.h"

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;

/* Size of stream buffer */
#define STREAMBUFFER_BLOCKSIZE 2*2097151
struct DWLLinearMem stream_mem;

u32 asic_trace_enabled; /* control flag for trace generation */
#ifdef ASIC_TRACE_SUPPORT
extern u32 asic_trace_enabled; /* control flag for trace generation */
/* Stuff to enable ref buffer model support */
//#include "../../../system/models/g1hw/ref_bufferd.h"
/* Ref buffer support stuff ends */
#endif

#ifdef VP6_EVALUATION
/* Stuff to enable ref buffer model support */
#include "../../../system/models/g1hw/ref_bufferd.h"
/* Ref buffer support stuff ends */
#endif

#ifdef VP6_EVALUATION
extern u32 g_hw_ver;
#endif

/* for tracing */
void printVp6PicCodingType(u32 pic_type);

#define DEBUG_PRINT(...) printf(__VA_ARGS__)

#define MAX_BUFFERS 16

const char *const short_options = "HO:N:m_mt_pab:EGZ";

addr_t input_stream_bus_address = 0;

u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 output_format = DEC_X170_OUTPUT_FORMAT;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
u32 ErrorConcealment = DEC_EC_PICTURE_FREEZE;

u32 pic_big_endian_size = 0;
u8* pic_big_endian = NULL;

u32 seed_rnd = 0;
u32 stream_bit_swap = 0;
i32 corrupted_bytes = 0;  /*  */
u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 stream_header_corrupt = 0;
u32 hdrs_rdy = 0;
u32 pic_rdy = 0;
u32 num_buffers = 3;
u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 convert_tiled_output = 0;
u32 md5sum = 0;

u32 use_peek_output = 0;
u32 ds_ratio_x, ds_ratio_y;
u32 pp_tile_out = 0;    /* PP tiled output */
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 cr_first = 0;
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
struct TestParams params;
VP6DecInst dec_inst;
const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 external_buf_num;  /* external buffers allocated yet. */
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
    if(add_extra_flag && (external_buf_num < MAX_BUFFERS)) {
      struct DWLLinearMem mem;
      i32 dwl_ret;
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        VP6DecRet rv = VP6DecAddBuffer(dec_inst, &mem);
        if(rv != VP6DEC_OK && rv != VP6DEC_WAITING_FOR_BUFFER) {
          if (pp_enabled)
            DWLFreeLinear(dwl_inst, &mem);
          else
            DWLFreeRefFrm(dwl_inst, &mem);
        } else {
          ext_buffers[external_buf_num++] = mem;
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
  printf("Releasing %d external frame buffers\n", external_buf_num);
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<external_buf_num; i++) {
    printf("Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address);
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

const struct option long_options[] = {
  {"help", 0, NULL, 'H'},
  {"output", 1, NULL, 'O'},
  {"last_pic", 1, NULL, 'N'},
  {"md5-per-pic", 0, NULL, 'm'},
#if 0
  {"md5-total", 0, NULL, 'M'},
#endif
  {"trace", 0, NULL, 't'},
  {"planar", 0, NULL, 'P'},
  {"alpha", 0, NULL, 'A'},
  {"buffers", 1, NULL, 'B'},
  {"tiled-mode", 0, NULL, 'E'},
  {"convert-tiled", 0, NULL, 'G'},
  {"peek", 0, NULL, 'Z'},
  {NULL, 0, NULL, 0}
};

typedef struct {
  const char *input;
  char output[DEC_MAX_PPU_COUNT][256];
  const char *pp_cfg;
  int last_frame;
  int md5;
  int planar;
  int alpha;
  int f,d,s;
} options_s;

struct TBCfg tb_cfg;

pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;
FILE *out_file[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL};
options_s options;

sem_t buf_release_sem;
VP6DecPicture buf_list[100] = {{0}};
u32 buf_status[100] = {0};
u32 list_pop_index = 0;
u32 list_push_index = 0;
u32 last_pic_flag = 0;
unsigned int current_video_frame = 0;

/* buf release thread entry point. */
static void* buf_release_thread(void* arg) {
  while(1) {
    /* Pop output buffer from buf_list and consume it */
    if(buf_status[list_pop_index]) {
      sem_wait(&buf_release_sem);
      VP6DecPictureConsumed(dec_inst, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && (external_buf_num < MAX_BUFFERS)) {
          struct DWLLinearMem mem;
          i32 dwl_ret;
          if (pp_enabled)
            dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          else
            dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          if(dwl_ret == DWL_OK) {
            VP6DecRet rv = VP6DecAddBuffer(dec_inst, &mem);
            if(rv != VP6DEC_OK && rv != VP6DEC_WAITING_FOR_BUFFER) {
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
  return(NULL);
}


/* Output thread entry point. */
static void* vp6_output_thread(void* arg) {
  VP6DecPicture dec_pic;
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
    VP6DecRet ret;
    ret = VP6DecNextPicture(dec_inst, &dec_pic, 0);
    if(ret == VP6DEC_PIC_RDY) {
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
        if (!dec_pic.pictures[i].frame_width ||
            !dec_pic.pictures[i].frame_height)
          continue;
        if(!use_peek_output) {
          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(dec_pic.pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(dec_pic.pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(dec_pic.pictures[i].output_format))
            pp_mono_chrome = 1;
          if (!IS_PIC_PACKED_RGB(dec_pic.pictures[i].output_format) &&
              !IS_PIC_PLANAR_RGB(dec_pic.pictures[i].output_format))
            WriteOutput(options.output[i], (unsigned char *) dec_pic.pictures[i].p_output_frame,
                        dec_pic.pictures[i].coded_width,
                        dec_pic.pictures[i].coded_height,
                        pic_display_number - 1,
                        pp_mono_chrome, 0, 0, 0,
                        IS_PIC_TILE(dec_pic.pictures[i].output_format),
                        dec_pic.pictures[i].pic_stride, dec_pic.pictures[i].pic_stride_ch, i,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        0, DEC_DPB_FRAME, md5sum, &out_file[i], 1);
          else
            WriteOutputRGB(options.output[i], (unsigned char *) dec_pic.pictures[i].p_output_frame,
                           dec_pic.pictures[i].coded_width,
                           dec_pic.pictures[i].coded_height,
                           pic_display_number - 1,
                           pp_mono_chrome, 0, 0, 0,
                           IS_PIC_TILE(dec_pic.pictures[i].output_format),
                           dec_pic.pictures[i].pic_stride, dec_pic.pictures[i].pic_stride_ch, i,
                           IS_PIC_PLANAR_RGB(dec_pic.pictures[i].output_format), pp_cr_first,
                           convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &out_file[i], 1);
        }
      }
      /* pic coding type */
      printVp6PicCodingType(dec_pic.pic_coding_type);
      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = dec_pic;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);

      pic_display_number++;
    }

    else if(ret == VP6DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
  }
  return(NULL);
}


void print_usage(const char *prog) {
  fprintf(stdout, "Usage:\n%s [options] <stream.vp6>\n\n", prog);

  fprintf(stdout,
          "    -h       --help              Print this help.\n");

  fprintf(stdout,
          "    -O[file] --output            Write output to specified file. [out.yuv]\n");

  fprintf(stdout,
          "    -N[n]    --last_frame        Forces decoding to stop after n frames; -1 sets no limit.[-1]\n");
  fprintf(stdout,
          "    -m       --md5-per-pic       Write MD5 checksum for each picture (YUV not written).\n");
#if 0
  fprintf(stdout,
          "    -M       --md5-total         Write MD5 checksum of whole sequence (YUV not written).\n");
#endif
  fprintf(stdout,
          "    -P       --planar            Write planar output\n");
  fprintf(stdout,
          "    -a       --pp-planar         Enable PP planar output.\n");
  fprintf(stdout,
          "    -E       --tiled-mode        Signal decoder to use tiled reference frame format\n");
  fprintf(stdout,
          "    -G       --convert-tiled     Convert tiled mode output pictures into raster scan format\n");
  fprintf(stdout,
          "    --alpha                      Stream contains alpha channelt\n");
  fprintf(stdout,
          "    -Bn                          Use n frame buffers in decoder\n");
  fprintf(stdout,
          "    -Z       --peek              Output pictures using VP6DecPeek() function\n");
  fprintf(stdout,
          "    --AddBuffer                  Add extra external buffer randomly\n");
  fprintf(stdout,
          "    --addbuffer                  Add extra external buffer in ouput thread randomly\n");
  fprintf(stdout,
          "    -An                          Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n");
  fprintf(stdout,
          "    -d[x[:y]]                    Fixed down scale ratio (1/2/4/8). E.g.,\n");
  fprintf(stdout,
          "    \t -d2                       -- down scale to 1/2 in both directions\n");
  fprintf(stdout,
          "    \t -d2:4                     -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
  fprintf(stdout,
          "    --cr-first                   PP outputs chroma in CrCb order.\n");
  fprintf(stdout,
          "    -C[xywh]NNN                  Cropping parameters. E.g.,\n");
  fprintf(stdout,
          "    \t  -Cx8 -Cy16               Crop from (8, 16)\n");
  fprintf(stdout,
          "    \t  -Cw720 -Ch480            Crop size  720x480\n");
  fprintf(stdout,
          "    -Dwxh                        PP output size wxh. E.g.,\n");
  fprintf(stdout,
          "    \t  -D1280x720               PP output size 1280x720\n");
  fprintf(stdout,
          "    --pp-rgb                 Enable Yuv2RGB\n");
  fprintf(stdout,
          "    --pp-rgb-planar          Enable planar Yuv2RGB\n");
  fprintf(stdout,
          "    --rgb-fmat               set the RGB output format\n");
  fprintf(stdout,
          "    --rgb-std                set the standard coeff to do Yuv2RGB\n");
  fprintf(stdout, "\n");
}

int getoptions(int argc, char **argv, options_s * opts) {
  int next_opt;

#ifdef ASIC_TRACE_SUPPORT
  asic_trace_enabled = 0;
#endif /* ASIC_TRACE_SUPPORT */

  do {
    next_opt = getopt_long(argc, argv, short_options, long_options, NULL);
    if(next_opt == -1)
      return 0;

    switch (next_opt) {
    case 'H':
      print_usage(argv[0]);
      exit(0);
    case 'O':
      strcpy(opts->output[0], optarg);
      break;
    case 'N':
      opts->last_frame = atoi(optarg);
      break;
    case 'm':
      opts->md5 = 1;
      break;
#if 0
    case 'M':
      opts->md5 = 2;
      break;
#endif
    case 'P':
      opts->planar = 1;
      break;
    case 'E':
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
      break;
    case 'G':
      convert_tiled_output = 1;
      break;
    case 'B':
      num_buffers = atoi(optarg);
      break;
    case 'A':
      opts->alpha = 1;
      break;
    case 't':
#ifdef ASIC_TRACE_SUPPORT
      asic_trace_enabled = 2;
#else
      fprintf(stdout, "\nWarning! Trace generation not supported!\n");
#endif
      break;

    case 'Z':
      use_peek_output = 1;
      break;

    default:
      fprintf(stderr, "Unknown option\n");
    }
  } while(1);
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#define leRushort( P, V) { \
  register ushort v = (uchar) *P++; \
  (V) = v + ( (ushort) (uchar) *P++ << 8); \
}
#define leRushortF( F, V) { \
  char b[2], *p = b; \
  fread( (void *) b, 1, 2, F); \
  leRushort( p, V); \
}

#define leRulong( P, V) { \
  register ulong v = (uchar) *P++; \
  v += (ulong) (uchar) *P++ << 8; \
  v += (ulong) (uchar) *P++ << 16; \
  (V) = v + ( (ulong) (uchar) *P++ << 24); \
}
#define leRulongF( F, V) { \
  char b[4] = {0,0,0,0}, *p = b; \
    V = 0; \
  ret = fread( (void *) b, 1, 4, F); \
  leRulong( p, V); \
}

#if 0
#  define leWchar( P, V)  { * ( (char *) P)++ = (char) (V);}
#else
#  define leWchar( P, V)  { *P++ = (char) (V);}
#endif

#define leWshort( P, V)  { \
  register short v = (V); \
  leWchar( P, v) \
  leWchar( P, v >> 8) \
}
#define leWshortF( F, V) { \
  char b[2], *p = b; \
  leWshort( p, V); \
  fwrite( (void *) b, 1, 2, F); \
}

#define leWlong( P, V)  { \
  register long v = (V); \
  leWchar( P, v) \
  leWchar( P, v >> 8) \
  leWchar( P, v >> 16) \
  leWchar( P, v >> 24) \
}
#define leWlongF( F, V)  { \
  char b[4], *p = b; \
  leWlong( p, V); \
  fwrite( (void *) b, 1, 4, F); \
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

int readCompressedFrame(FILE * fp) {
  ulong frame_size = 0;
  int ret;

  leRulongF(fp, frame_size);

  if( frame_size >= STREAMBUFFER_BLOCKSIZE ) {
    /* too big a frame */
    return 0;
  }

  ret = fread(stream_mem.virtual_address, 1, frame_size, fp);
  if(!ret) {
    DEBUG_PRINT(("READ COMPRESSED FRAME FAILED!\n"));
  }

  return (int) frame_size;
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

int decode_file(const options_s * opts, u32 ds_x, u32 ds_y) {
  VP6DecInput input;
  VP6DecOutput output;
  VP6DecPicture dec_pic;
  VP6DecRet ret;
  u32 tmp;
  struct VP6DecConfig dec_cfg;
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  VP6DecBufferInfo hbuf;
  VP6DecRet rv;
  int i;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP6_DEC;
  memset(&dec_pic, 0, sizeof(VP6DecPicture));

  FILE *input_file = fopen(opts->input, "rb");

  if(input_file == NULL) {
    perror(opts->input);
    return -1;
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

  dwl_inst = DWLInit(&dwl_init);
  if(dwl_inst == NULL) {
    DEBUG_PRINT(("DWLInit# ERROR: DWL Init failed\n"));
    goto end;
  }
  ret = VP6DecInit(&dec_inst,
                   dwl_inst,
                   ErrorConcealment,
                   num_buffers,
                   tiled_output, 1, 0);
  if (ret != VP6DEC_OK) {
    printf("DECODER INITIALIZATION FAILED\n");
    goto end;
  }

  /* Set ref buffer test mode */
  //((VP6DecContainer_t *) dec_inst)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
                 */
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;

  if(DWLMallocLinear(((VP6DecContainer_t *) dec_inst)->dwl,
                     STREAMBUFFER_BLOCKSIZE, &stream_mem) != DWL_OK) {
    printf(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    exit(-1);
  }

  while(!feof(input_file) &&
        current_video_frame < (unsigned int) opts->last_frame) {
    fflush(stdout);

    input.data_len = readCompressedFrame(input_file);
    input.stream = (u8*)stream_mem.virtual_address;
    input.stream_bus_address = (addr_t)stream_mem.bus_address;

    if(input.data_len == 0)
      break;

    if( opts->alpha ) {
      if( input.data_len < 3 )
        break;
      input.data_len -= 3;
      input.stream += 3;
      input.stream_bus_address += 3;
    }

    if(stream_truncate && pic_rdy && (hdrs_rdy || stream_header_corrupt)) {
      i32 ret;

      ret = TBRandomizeU32(&input.data_len);
      if(ret != 0) {
        DEBUG_PRINT("RANDOM STREAM ERROR FAILED\n");
        return 0;
      }
    }

    /* If enabled, break the stream */
    if(stream_bit_swap) {
      if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
        /* Picture needs to be ready before corrupting next picture */
        if(pic_rdy && corrupted_bytes <= 0) {
          ret =
            TBRandomizeBitSwapInStream((u8 *)input.stream,
                                       input.data_len,
                                       tb_cfg.tb_params.stream_bit_swap);
          if(ret != 0) {
            DEBUG_PRINT("RANDOM STREAM ERROR FAILED\n");
            goto end;
          }

          corrupted_bytes = input.data_len;
        }
      }
    }

    do {
      ret = VP6DecDecode(dec_inst, &input, &output);
      /* printf("VP6DecDecode retruned: %d\n", ret); */

      if(ret == VP6DEC_HDRS_RDY) {
        rv = VP6DecGetBufferInfo(dec_inst, &hbuf);
        printf("VP6DecGetBufferInfo ret %d\n", rv);
        printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
               (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
        VP6DecInfo dec_info;

        TBSetRefbuMemModel( &tb_cfg,
                            ((VP6DecContainer_t *) dec_inst)->vp6_regs,
                            &((VP6DecContainer_t *) dec_inst)->ref_buffer_ctrl );

        hdrs_rdy = 1;

        ret = VP6DecGetInfo(dec_inst, &dec_info);
        if (ret != VP6DEC_OK) {
          DEBUG_PRINT("ERROR in getting stream info!\n");
          goto end;
        }

        fprintf(stdout, "\n"
                "Stream info: vp6_version  = 6.%d\n"
                "             vp6_profile  = %s\n"
                "             coded size  = %d x %d\n"
                "             scaled size = %d x %d\n\n",
                dec_info.vp6_version - 6,
                dec_info.vp6_profile == 0 ? "SIMPLE" : "ADVANCED",
                dec_info.frame_width, dec_info.frame_height,
                dec_info.scaled_width, dec_info.scaled_height);
#if 0
        if (crop_enabled) {
          dec_cfg.crop.enabled = 1;
          dec_cfg.crop.x = crop_x;
          dec_cfg.crop.y = crop_y;
          dec_cfg.crop.width = crop_w;
          dec_cfg.crop.height = crop_h;
        } else {
          dec_cfg.crop.enabled = 0;
          dec_cfg.crop.x = 0;
          dec_cfg.crop.y = 0;
          dec_cfg.crop.width = 0;
          dec_cfg.crop.height = 0;
        }
        dec_cfg.scale.enabled = scale_enabled;
        if (scale_mode == FIXED_DOWNSCALE) {
          dec_cfg.scale.width = dec_info.frame_width / ds_ratio_x;
          dec_cfg.scale.height = dec_info.frame_height / ds_ratio_y;
        } else {
          dec_cfg.scale.width = scaled_w;
          dec_cfg.scale.height = scaled_h;
        }
        dec_cfg.align = align;
        dec_cfg.cr_first = cr_first;
#else
        ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                               !tb_cfg.pp_params.pipeline_e);
        if (params.fscale_cfg.fixed_scale_enabled) {
          if(!params.ppu_cfg[0].crop.set_by_user) {
            params.ppu_cfg[0].crop.width = dec_info.frame_width;
            params.ppu_cfg[0].crop.height = dec_info.frame_height;
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
        dec_cfg.fixed_scale_enabled = params.fscale_cfg.fixed_scale_enabled;
#endif
        tmp = VP6DecSetInfo(dec_inst, &dec_cfg);
        if (tmp != VP6DEC_OK) {
          DEBUG_PRINT(("Invalid pp parameters\n"));
          goto end;
        }

        //prev_width = dec_info.frame_width;
        //prev_height = dec_info.frame_height;
        //min_buffer_num = dec_info.pic_buff_size;
        ret = VP6DEC_HDRS_RDY;  /* restore */
      }
      if (ret == VP6DEC_WAITING_FOR_BUFFER) {
        DEBUG_PRINT(("Waiting for frame buffers\n"));
        struct DWLLinearMem mem;

        rv = VP6DecGetBufferInfo(dec_inst, &hbuf);
        printf("VP6DecGetBufferInfo ret %d\n", rv);
        printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
               (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
        while (rv == VP6DEC_WAITING_FOR_BUFFER) {
          if (hbuf.buf_to_free.virtual_address != NULL) {
            add_extra_flag = 0;
            ReleaseExtBuffers();
            buffer_release_flag = 1;
            external_buf_num = 0;
          }
          rv = VP6DecGetBufferInfo(dec_inst, &hbuf);
        }

        buffer_size = hbuf.next_buf_size;
        if(buffer_release_flag && hbuf.next_buf_size) {
          /* Only add minimum required buffers at first. */
          for(i = 0; i < hbuf.buf_num; i++) {
            if (pp_enabled)
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            else
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            rv = VP6DecAddBuffer(dec_inst, &mem);

            printf("VP6DecAddBuffer ret %d\n", rv);
            if(rv != VP6DEC_OK && rv != VP6DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              ext_buffers[i] = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          external_buf_num = hbuf.buf_num;
          add_extra_flag = 1;
        }
        ret = VP6DEC_HDRS_RDY;
      }
    } while(ret == VP6DEC_HDRS_RDY || ret == VP6DEC_NO_DECODING_BUFFER);

    if(ret == VP6DEC_PIC_DECODED) {
      pic_rdy = 1;

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, vp6_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      if (use_peek_output)
        ret = VP6DecPeek(dec_inst, &dec_pic);

      if (ret == VP6DEC_PIC_RDY && use_peek_output)
      {
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(dec_pic.pictures[0].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(dec_pic.pictures[0].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(dec_pic.pictures[0].output_format))
          pp_mono_chrome = 1;
        if (!IS_PIC_PACKED_RGB(dec_pic.pictures[0].output_format) &&
            !IS_PIC_PLANAR_RGB(dec_pic.pictures[0].output_format))
          WriteOutput(options.output[0], (unsigned char *) dec_pic.pictures[0].p_output_frame,
                      dec_pic.pictures[0].coded_width,
                      dec_pic.pictures[0].coded_height,
                      0, pp_mono_chrome, 0, 0, 0,
                      IS_PIC_TILE(dec_pic.pictures[0].output_format),
                      dec_pic.pictures[0].pic_stride, dec_pic.pictures[0].pic_stride_ch, 0,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      0, DEC_DPB_FRAME, md5sum, &out_file[0], 1);
        else
          WriteOutputRGB(options.output[0], (unsigned char *) dec_pic.pictures[0].p_output_frame,
                      dec_pic.pictures[0].coded_width,
                      dec_pic.pictures[0].coded_height,
                      0, pp_mono_chrome, 0, 0, 0,
                      IS_PIC_TILE(dec_pic.pictures[0].output_format),
                      dec_pic.pictures[0].pic_stride, dec_pic.pictures[0].pic_stride_ch, 0,
                      IS_PIC_PLANAR_RGB(dec_pic.pictures[0].output_format), pp_cr_first,
                      convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &out_file[0], 1);
      }

      current_video_frame++;
#if 0
      if (current_video_frame == 10) {
        ret = VP6DecAbort(dec_inst);
        ret = VP6DecAbortAfter(dec_inst);
        rewind(input_file);
      }
#endif

      fprintf(stdout, "Picture %d,", current_video_frame);

    }

    corrupted_bytes = 0;
  }

end:

  if (output_thread_run)
    VP6DecEndOfStream(dec_inst, 1);

  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  add_buffer_thread_run = 0;

  printf("Pictures decoded: %d\n", current_video_frame);

  if(stream_mem.virtual_address)
    DWLFreeLinear(((VP6DecContainer_t *) dec_inst)->dwl, &stream_mem);
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  VP6DecRelease(dec_inst);
  DWLRelease(dwl_inst);

  fclose(input_file);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (out_file[i])
      fclose(out_file[i]);
  }
  if (pic_big_endian != NULL)
    free(pic_big_endian);

  return 0;
}


int main(int argc, char *argv[]) {
  int i, ret, j;
  FILE *f_tbcfg;
  const char default_out[] = "out.yuv";

  memset(&options, 0, sizeof(options_s));

  SetupDefaultParams(&params);

#ifdef VP6_EVALUATION_9190
  g_hw_ver = 9190;
#elif VP6_EVALUATION_G1
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

  if(argc < 2) {
    print_usage(argv[0]);
    exit(0);
  }

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    strcpy(options.output[i], default_out);
  options.input = argv[argc - 1];
  options.md5 = 0;
  options.last_frame = -1;
  options.planar = 0;
  for (i = 1; i < (u32)(argc-1); i++) {
    if(strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      exit(0);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(options.output[j], argv[i] + 2);
    } else if(strncmp(argv[i], "-N", 2) == 0) {
      options.last_frame = atoi(argv[i]+2);
    } else if(strcmp(argv[i], "-m") == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      options.md5 = 1;
      md5sum = 1;
#if 0
    } else if(strcmp(argv[i], "-M") == 0) {
      options.md5 = 2;
#endif
    } else if(strcmp(argv[i], "-P") == 0 ||
              strcmp(argv[i], "--planar") == 0) {
      options.planar = 1;
    } else if(strncmp(argv[i], "-a", 2) == 0 ||
              strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = pp_planar_out;
      //tb_cfg.pp_units_params[0].planar = pp_planar_out;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "-E") == 0) {
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    } else if(strcmp(argv[i], "-G") == 0) {
      convert_tiled_output = 1;
    } else if(strncmp(argv[i], "-B", 2) == 0) {
      num_buffers = atoi(argv[i]+2);
    } else if(strcmp(argv[i], "--alpha") == 0) {
      options.alpha = 1;
    } else if(strcmp(argv[i], "-t") == 0) {
#ifdef ASIC_TRACE_SUPPORT
      asic_trace_enabled = 2;
#else
      fprintf(stdout, "\nWarning! Trace generation not supported!\n");
#endif
    } else if(strcmp(argv[i], "-Z") == 0) {
      use_peek_output = 1;
    }
    else if(strcmp(argv[i], "--AddBuffer") == 0) {
      use_extra_buffers = 1;
    }
    else if(strcmp(argv[i], "--addbuffer") == 0) {
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
      params.ppu_cfg[0].scale.set_by_user = 1;
      params.ppu_cfg[0].scale.ratio_x = ds_ratio_x;
      params.ppu_cfg[0].scale.ratio_y = ds_ratio_y;
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
      params.ppu_cfg[0].scale.set_by_user = 1;
      params.ppu_cfg[0].scale.ratio_x = 0;
      params.ppu_cfg[0].scale.ratio_y = 0;
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
      pp_enabled = 1;
    } else if (strcmp(argv[i], "--pp-luma-only") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].monochrome = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "-U") == 0) {
      //pp_tile_out = 1;
      //tb_cfg.pp_units_params[0].tiled_e = pp_tile_out;
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
      fprintf(stdout, "UNKNOWN PARAMETER: %s\n", argv[i]);
      return 1;
    }
  }
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
    if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
      tb_cfg.dec_params.cache_support = 1;
    else
      tb_cfg.dec_params.cache_support = 0;
#endif
#endif
  /* check if traces shall be enabled */
#ifdef ASIC_TRACE_SUPPORT
  {
    char trace_string[80];
    FILE *fid = fopen("trace.cfg", "r");
    if (fid) {
      /* all tracing enabled if any of the recognized keywords found */
      while(fscanf(fid, "%s\n", trace_string) != EOF) {
        if (!strcmp(trace_string, "toplevel") ||
            !strcmp(trace_string, "all"))
          asic_trace_enabled = 2;
        else if(!strcmp(trace_string, "fpga") ||
                !strcmp(trace_string, "decoding_tools"))
          if(asic_trace_enabled == 0)
            asic_trace_enabled = 1;
      }
    }
  }
#endif

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

#ifdef ASIC_TRACE_SUPPORT

  /*
      {
          extern refBufferd_t    vp6Refbuffer;
          refBufferd_Reset(&vp6Refbuffer);
      }
      */

#endif /* ASIC_TRACE_SUPPORT */

#ifdef VP6_EVALUATION

  /*
      {
          extern refBufferd_t    vp6Refbuffer;
          refBufferd_Reset(&vp6Refbuffer);
      }
      */

#endif /* VP6_EVALUATION */

  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  output_format = TBGetDecOutputFormat(&tb_cfg);
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  /* ErrorConcealment */
  ErrorConcealment = TBGetDecErrorConcealment(&tb_cfg);

  seed_rnd = tb_cfg.tb_params.seed_rnd;
  stream_header_corrupt = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  /* if headers are to be corrupted
   * -> do not wait the picture to finalize before starting stream corruption */
  if(stream_header_corrupt)
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

  TBInitializeRandom(seed_rnd);

  {
    VP6DecApiVersion dec_api;
    VP6DecBuild dec_build;

    /* Print API version number */
    dec_api = VP6DecGetAPIVersion();
    dec_build = VP6DecGetBuild();
    DEBUG_PRINT
    ("\nG1 VP6 Decoder API v%d.%d - SW build: %d - HW build: %x\n\n",
     dec_api.major, dec_api.minor, dec_build.sw_build, dec_build.hw_build);
  }

  ret = decode_file(&options, ds_ratio_x, ds_ratio_y);

  return ret;
}

/*------------------------------------------------------------------------------

    Function name:            printVp6PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printVp6PicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf(" DEC_PIC_TYPE_I\n");
    break;
  case DEC_PIC_TYPE_P:
    printf(" DEC_PIC_TYPE_P\n");
    break;
  default:
    printf("Other %d\n", pic_type);
    break;
  }
}
