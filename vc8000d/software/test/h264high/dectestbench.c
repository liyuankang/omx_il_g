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

#include "h264decapi.h"
#include "h264decapi_e.h"
#include "dwl.h"
#include "dwlthread.h"
#include "libav-wrapper.h"
#include "bytestream_parser.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include "h264hwd_container.h"
#include "tb_cfg.h"
#include "tb_tiled.h"
#include "regdrv.h"
#include "deccfg.h"

#include "tb_params.h"
#include "tb_md5.h"
#include "tb_out.h"
#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "command_line_parser.h"
#ifdef _ENABLE_2ND_CHROMA
#include "h264decapi_e.h"
#endif

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/* Debug prints */
#undef DEBUG_PRINT
#if 0
#define DEBUG_PRINT(argv)
#else
#define DEBUG_PRINT(argv) printf argv
#endif

#define NUM_RETRY   100 /* how many retries after HW timeout */
#define MAX_BUFFERS 34
#define ALIGNMENT_MASK 7
#define BUFFER_ALIGN_MASK 0xF

/* Low latency : Frame Mode by default */
#define LOW_LATENCY_FRAME_MODE
#define LOW_LATENCY_PACKET_SIZE 256

u32 NextPacket(u8 ** p_strm);
u32 NextPacketFromFile(u8 ** p_strm);
u32 CropPicture(u8 * p_out_image, u8 * p_in_image,
                u32 frame_width, u32 frame_height, H264CropParams * p_crop_params,
                u32 mono_chrome);
static void printDecodeReturn(i32 retval);
void printH264PicCodingType(u32 *pic_type);
static void SetMissingField2Const( u8 *output_picture,
                            u32 pic_width,
                            u32 pic_height,
                            u32 monochrome,
                            u32 top_field,
                            u32 pic_stride,
                            u32 tiled_output);

u32 fillBuffer(const u8 *stream);
/* Global variables for stream handling */
u8 *stream_stop = NULL;
u32 packetize = 0;
u32 nal_unit_stream = 0;
FILE *foutput[DEC_MAX_OUT_COUNT] = {NULL};
FILE *foutput2[DEC_MAX_OUT_COUNT] = {NULL};
FILE *f_tiled_output = NULL;
FILE *fchroma2 = NULL;

FILE *findex = NULL;

u32 enable_mc;
u32 force_to_single_core = 0;
/* PP only mode. Decoder is enabled only to generate PP input trace file.*/
u32 pp_tile_out = 0;    /* PP tiled output */
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;
/* stream start address */
u8 *byte_strm_start;
u32 trace_used_stream = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 128 bytes alignment */

/* output file writing disable */
u32 disable_output_writing = 0;
u32 retry = 0;

u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 output_format = DEC_X170_OUTPUT_FORMAT;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;

u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 stream_header_corrupt = 0;
u32 hdrs_rdy = 0;
u32 pic_rdy = 0;
struct TBCfg tb_cfg;

u32 long_stream = 0;
FILE *finput;
u32 planar_output = 0;
/*u32 tiled_output = 0;*/

/* flag to enable md5sum output */
u32 md5sum = 0;

u32 out_1010 = 0;
u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 dpb_mode = DEC_DPB_FRAME;
u32 convert_tiled_output = 0;

u32 use_peek_output = 0;
u32 enable_mvc = 0;
u32 mvc_separate_views = 0;
u32 skip_non_reference = 0;
u32 convert_to_frame_dpb = 0;

/* variables for indexing */
u32 save_index = 0;
u32 use_index = 0;
addr_t cur_index = 0;
addr_t next_index = 0;
/* indicate when we save index */
u8 save_flag = 0;

extern u32 test_case_id;

/* thread for low latency feature */
typedef void* task_handle;
typedef void* (*task_func)(void*);

/* variables for low latency feature */
volatile struct strmInfo send_strm_info;
u32 bytes_go_back;
u32 send_strm_len;
u32 pic_decoded;
u32 strm_last_flag;
u32 frame_len;
pthread_mutex_t send_mutex;
sem_t curr_frame_end_sem;
sem_t next_frame_start_sem;
/* func for low latency feature */
void wait_for_task_completion(task_handle task);
task_handle run_task(task_func func, void* param);
void* send_bytestrm_task(void* param);

u32 process_end_flag;
u32 stream_size;
u32 low_latency;
u32 low_latency_sim;
u32 secure;
const char* in_file_name;

u8 *grey_chroma = NULL;
size_t grey_chroma_size = 0;

u8 *pic_big_endian = NULL;
size_t pic_big_endian_size = 0;

u32 pp_enabled = 0;
/* user input arguments */
enum SCALE_MODE scale_mode;

#ifdef H264_EVALUATION
extern u32 g_hw_ver;
extern u32 h264_high_support;
#endif

u32 abort_test;

#ifdef ADS_PERFORMANCE_SIMULATION

volatile u32 tttttt = 0;

void trace_perf() {
  tttttt++;
}

#undef START_SW_PERFORMANCE
#undef END_SW_PERFORMANCE

#define START_SW_PERFORMANCE trace_perf();
#define END_SW_PERFORMANCE trace_perf();

#endif

H264DecInst dec_inst;
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

u32 pic_display_number = 0;

#ifdef TESTBENCH_WATCHDOG
pid_t main_pid;
u32 old_pic_display_number = 0;

void watchdog1(int signal_number) {
  if(pic_display_number == old_pic_display_number) {
    fprintf(stderr, "\n\nTestbench TIMEOUT\n");
    kill(main_pid, SIGTERM);
  } else {
    old_pic_display_number = pic_display_number;
  }
}
#endif

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && (num_buffers < 34)) {
      struct DWLLinearMem mem;
      i32 dwl_ret;
      mem.mem_type = DWL_MEM_TYPE_DPB;
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        enum DecRet rv = H264DecAddBuffer(dec_inst, &mem) ;
        if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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

H264DecInfo dec_info;
char out_file_name[DEC_MAX_OUT_COUNT][256] = {"", "", "", ""};
char out_file_name_tiled[256] = "out_tiled.yuv";
u32 pic_size = 0;
u32 crop_display = 0;
u8 *tmp_image = NULL;
u32 num_errors = 0;
pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;

sem_t buf_release_sem;
H264DecPicture buf_list[100] = {{0}};
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
      H264DecPictureConsumed(dec_inst, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;
      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && (num_buffers < 34)) {
          struct DWLLinearMem mem;
          i32 dwl_ret;
          mem.mem_type = DWL_MEM_TYPE_DPB;
          if (pp_enabled)
            dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          else
            dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          if(dwl_ret == DWL_OK) {
            enum DecRet rv = H264DecAddBuffer(dec_inst, &mem) ;
            if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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
    /* The last buffer has been consumed */
    if(last_pic_flag &&  buf_status[list_pop_index] == 0)
      break;
    usleep(10000);
  }
  return NULL;
}

/* Output thread entry point. */
static void* h264_output_thread(void* arg) {
  H264DecPicture dec_picture;
  pic_display_number = 1;
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
    enum DecRet ret;

    ret = H264DecNextPicture(dec_inst, &dec_picture, 0);

    if(ret == DEC_PIC_RDY) {
     // static u32 pic_count = 0;
     // pic_count++;
     // if (pic_count == 10)
     //   pic_count = 1;
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (!dec_picture.pictures[i].pic_width ||
            !dec_picture.pictures[i].pic_height)
          continue;
        if(!use_peek_output) {
          DEBUG_PRINT(("PIC %d/%d, view %d, type [%s:%s], ",
                       pic_display_number,
                       dec_picture.pic_id,
                       dec_picture.view_id,
                       dec_picture.is_idr_picture[0] ? "IDR" : "NON-IDR",
                       dec_picture.is_idr_picture[1] ? "IDR" : "NON-IDR"));

          /* pic coding type */
          printH264PicCodingType(dec_picture.pic_coding_type);

          if(dec_picture.nbr_of_err_mbs) {
            DEBUG_PRINT((", concealed %d", dec_picture.nbr_of_err_mbs));
          }

          if(dec_picture.interlaced) {
            DEBUG_PRINT((", INTERLACED "));
            if(dec_picture.field_picture) {
              DEBUG_PRINT(("FIELD %s",
                           dec_picture.top_field ? "TOP" : "BOTTOM"));

              SetMissingField2Const((u8*)dec_picture.pictures[i].output_picture,
                                    dec_picture.pictures[i].pic_width,
                                    dec_picture.pictures[i].pic_height,
                                    dec_info.mono_chrome,
                                    !dec_picture.top_field,
                                    dec_picture.pictures[i].pic_stride,
                                    dec_picture.pictures[i].output_format != DEC_OUT_FRM_RASTER_SCAN);
            } else {
              DEBUG_PRINT(("FRAME"));
            }
          }

          DEBUG_PRINT((", Crop: (%d, %d), %dx%d\n",
                       dec_picture.crop_params.crop_left_offset,
                       dec_picture.crop_params.crop_top_offset,
                       dec_picture.crop_params.crop_out_width,
                       dec_picture.crop_params.crop_out_height));

          fflush(stdout);

          num_errors += dec_picture.nbr_of_err_mbs;

          /* Write output picture to file */
          image_data = (u8 *) dec_picture.pictures[i].output_picture;

          if(crop_display) {
            int tmp = CropPicture(tmp_image, image_data,
                                  dec_picture.pictures[i].pic_width,
                                  dec_picture.pictures[i].pic_height,
                                  &dec_picture.crop_params,
                                  dec_info.mono_chrome);
            if(tmp) {
              DEBUG_PRINT(("ERROR in cropping!\n"));
            }
          }
          pp_planar   = 0;
          pp_cr_first = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(dec_picture.pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(dec_picture.pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(dec_picture.pictures[i].output_format) ||
                    dec_picture.pictures[i].output_picture_chroma == NULL)
            pp_mono_chrome = 1;
          if (!dec_picture.nbr_of_err_mbs/*&& pic_count == 2*/)
          WriteOutput(out_file_name[i],
                      crop_display ? tmp_image : image_data,
                      crop_display ? dec_picture.crop_params.crop_out_width : dec_picture.pictures[i].pic_width,
                      crop_display ?
                      dec_picture.crop_params.crop_out_height : dec_picture.pictures[i].pic_height,
                      pic_display_number - 1,
                      pp_mono_chrome,
                      dec_picture.view_id, mvc_separate_views,
                      disable_output_writing,
                      dec_picture.pictures[i].output_format == DEC_OUT_FRM_TILED_4X4,
                      dec_picture.pictures[i].pic_stride,
                      dec_picture.pictures[i].pic_stride_ch, i,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      convert_to_frame_dpb, dpb_mode, md5sum,
                      ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[i]) : &(foutput[i])), 1);

#ifdef _ENABLE_2ND_CHROMA
          WriteOutput2ndChroma(pic_size,
                      pic_display_number - 1,
                      dec_info.mono_chrome || (dec_picture.pictures[i].output_format == DEC_OUT_FRM_YUV400),
                      dec_picture.view_id, mvc_separate_views, i, md5sum,
                      ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[i]) : &(foutput[i])));
#endif

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
    } else if(ret == DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      DEBUG_PRINT(("END-OF-STREAM received in output thread\n"));
      add_buffer_thread_run = 0;
      break;
    }
  }
  return NULL;
}

void SetMissingField2Const( u8 *output_picture,
                            u32 pic_width,
                            u32 pic_height,
                            u32 monochrome,
                            u32 top_field,
                            u32 pic_stride,
                            u32 tiled_output) {
  u8 *field_base;
  i32 i;
  /* TODO: Does not support tiled input */
  /* TODO: Fix reference data before enabling this */
  //return;
  u32 size;
  u32 lines_per_stride;
  if (tiled_output) {
    size = pic_stride * pic_height / 4;
    lines_per_stride = 4;
  } else {
    size = pic_width * pic_height;
    lines_per_stride = 1;
  }
  if (dpb_mode != DEC_DPB_FRAME) {
    /* luma */
    field_base = output_picture;

    if (!top_field) {
      /* bottom */
      field_base += size / 2;
    }

    for (i = 0; i < pic_height/2/lines_per_stride; i++) {
      memset(field_base, 128, pic_width*lines_per_stride);
      field_base += pic_stride;
    }

    if (monochrome)
      return;

    /* chroma */
    field_base = output_picture + size;

    if (!top_field) {
      /* bottom */
      field_base += size / 4;
    }

    for (i = 0; i < pic_height/4/lines_per_stride; i++) {
      memset(field_base, 128, pic_width*lines_per_stride);
      field_base += pic_stride;
    }

    return;
  }

  /* luma */
  field_base = output_picture;

  if (!top_field) {
    /* bottom */
    field_base += pic_width;
  }

  for (i = 0; i < pic_height / 2; i++) {
    memset(field_base, 128, pic_width);
    field_base += 2 * pic_width;
  }

  if (monochrome)
    return;

  /* chroma */
  field_base = output_picture + pic_width * pic_height;

  if (!top_field) {
    /* bottom */
    field_base += pic_width;
  }

  for (i = 0; i < (pic_height / 2 + 1) / 2; i++) {
    memset(field_base, 128, pic_width);
    field_base += 2 * pic_width;
  }
}

/* one extra stream buffer so that we can decode ahead,
 * and be ready when core has finished
 */
#define MAX_STRM_BUFFERS    (MAX_ASIC_CORES + 1)

static struct DWLLinearMem stream_mem[MAX_STRM_BUFFERS];
static u32 stream_mem_status[MAX_STRM_BUFFERS];
static u32 allocated_buffers = 0;

static sem_t stream_buff_free;
#ifdef _HAVE_PTHREAD_H
static pthread_mutex_t strm_buff_stat_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static pthread_mutex_t strm_buff_stat_lock = {0, };
#endif //_HAVE_PTHREAD_H

void StreamBufferConsumed(void *stream, void *p_user_data) {
  int idx;
  pthread_mutex_lock(&strm_buff_stat_lock);

  idx = 0;
  do {
    if ((u8*)stream >= (u8*)stream_mem[idx].virtual_address &&
        (u8*)stream < (u8*)stream_mem[idx].virtual_address + stream_mem[idx].size) {
      stream_mem_status[idx] = 0;
      assert(p_user_data == stream_mem[idx].virtual_address);
      break;
    }
    idx++;
  } while(idx < allocated_buffers);

  assert(idx < allocated_buffers);

  pthread_mutex_unlock(&strm_buff_stat_lock);

  sem_post(&stream_buff_free);
}

int GetFreeStreamBuffer() {
  int idx;
  sem_wait(&stream_buff_free);

  pthread_mutex_lock(&strm_buff_stat_lock);

  idx = 0;
  while(stream_mem_status[idx]) {
    idx++;
  }
  assert(idx < allocated_buffers);

  stream_mem_status[idx] = 1;

  pthread_mutex_unlock(&strm_buff_stat_lock);

  return idx;
}


/*------------------------------------------------------------------------------

    Function name:  PrintUsage()

    Purpose:
        Print C test bench inline help -- command line params
------------------------------------------------------------------------------*/
void PrintUsageH264(char *s)
{
    DEBUG_PRINT(("Usage: %s [options] file.h264\n", s));
    DEBUG_PRINT(("\t--help(-h) Print command line parameters help info\n"));
    DEBUG_PRINT(("\t-Nn forces decoding to stop after n pictures\n"));
    DEBUG_PRINT(("\t-O<file> write output to <file> (default out_wxxxhyyy.yuv)\n"));
    DEBUG_PRINT(("\t-m Output frame based md5 checksum. No YUV output!(--md5-per-pic)\n"));
    DEBUG_PRINT(("\t-X Disable output file writing\n"));
    DEBUG_PRINT(("\t-R disable DPB output reordering\n"));
    DEBUG_PRINT(("\t-W disable packetizing even if stream does not fit to buffer.\n"
                 "\t   Only the first 0x1FFFFF bytes of the stream are decoded.\n"));
    DEBUG_PRINT(("\t-E use tiled reference frame format.\n"));
    DEBUG_PRINT(("\t-G convert tiled output pictures to raster scan\n"));
    DEBUG_PRINT(("\t-L enable support for long streams\n"));
    DEBUG_PRINT(("\t-a write PP planar output(--pp-planar)\n"));
    DEBUG_PRINT(("\t-I save index file\n"));
    DEBUG_PRINT(("\t--mvc Enable MVC decoding (use it only with MVC streams)\n"));
    DEBUG_PRINT(("\t-V Write MVC views to separate files\n"));
    DEBUG_PRINT(("\t-Q Skip decoding non-reference pictures.\n"));
    DEBUG_PRINT(("\t-Z output pictures using H264DecPeek() function\n"));
    DEBUG_PRINT(("\t--separate-fields-in-dpb DPB stores interlaced content"\
                 " as fields (default: frames)\n"));
    DEBUG_PRINT(("\t--output-frame-dpb Convert output to frame mode even if"\
                 " field DPB mode used\n"));
    DEBUG_PRINT(("\t--ultra-low-latency enable low latency RTL simulation flag\n"));
    DEBUG_PRINT(("\t--platform-low-latency enable low latency platform running flag\n"));
    DEBUG_PRINT(("\t--secure enable secure mode flag\n"));
    DEBUG_PRINT(("\t-e add extra external buffer randomly\n"));
    DEBUG_PRINT(("\t-k allocate extra external buffer in output thread\n"));
#ifdef ASIC_TRACE_SUPPORT
    DEBUG_PRINT(("\t-F force 8170 mode in 8190 HW model (baseline configuration forced)\n"));
    DEBUG_PRINT(("\t-B force Baseline configuration to 8190 HW model\n"));
#endif
    DEBUG_PRINT(("\t-An Set stride aligned to n bytes (valid value: 1/8/16/32/64/128/256/512/1024/2048)\n"));
    DEBUG_PRINT(("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g., \n"));
    DEBUG_PRINT(("\t  -d2 -- down scale to 1/2 in both directions\n"));
    DEBUG_PRINT(("\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n"));
    DEBUG_PRINT(("\t--ec Decoder continues to decode next slice in picture"\
                 " when slice loss/error detected.\n"));
    DEBUG_PRINT(("\t--cr-first PP outputs chroma in CrCb order.\n"));
    DEBUG_PRINT(("\t-C[xywh]NNN Cropping parameters. E.g.,\n"));
    DEBUG_PRINT(("\t  -Cx8 -Cy16        Crop from (8, 16)\n"));
    DEBUG_PRINT(("\t  -Cw720 -Ch480     Crop size  720x480\n"));
    DEBUG_PRINT(("\t-Dwxh  PP output size wxh. E.g.,\n"));
    DEBUG_PRINT(("\t  -D1280x720        PP output size 1280x720\n\n"));
    DEBUG_PRINT(("\t  --mc              Enable H264 multi-core decoding \n\n"));
    DEBUG_PRINT(("\t--E1010 Enable output E1010 format.\n"));
    DEBUG_PRINT(("\t--pp-only Enable only to generate PP input trace file for"\
                 "standalone PP verification.\n"));
    DEBUG_PRINT(("\t-U Enable PP tiled output.\n"));
    DEBUG_PRINT(("\t-pp-luma-only Enable pp monochrome output.\n"));
    DEBUG_PRINT(("\t--cache_enable Enable cache rtl simulation(external hw IP).\n"));
    DEBUG_PRINT(("\t--shaper_enable Enable shaper rtl simulation(external hw IP).\n"));
    DEBUG_PRINT(("\t--shaper_bypass Enable shaper bypass rtl simulation(external hw IP).\n"));
}

void H264HandleFatalError() {
  DEBUG_PRINT(("Fatal system error\n"));
}

/*------------------------------------------------------------------------------

    Function name:  main

    Purpose:
        main function of decoder testbench. Provides command line interface
        with file I/O for H.264 decoder. Prints out the usage information
        when executed without arguments.

------------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  osal_thread_init();
  u32 i, j, tmp;
  u32 pp_planar, pp_cr_first, pp_mono_chrome;
  u32 k = 0;
  u32 max_num_pics = 0;
  u8 *image_data;
  long int strm_len, max_strm_len;
  enum DecRet ret;
  H264DecInput dec_input;
  H264DecOutput dec_output;
  H264DecPicture dec_picture;
  struct H264DecConfig dec_cfg = {0};
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  H264DecBufferInfo hbuf;
  enum DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_H264_DEC;

  u32 pic_decode_number = 0;
  u32 pic_display_number = 0;
  u32 disable_output_reordering = 0;
  u32 use_display_smoothing = 0;
  u32 rlc_mode = 0;
  u32 mb_error_concealment = 0;
  u32 force_whole_stream = 0;
  const u8 *ptmpstream = NULL;
  u32 stream_will_end = 0;
  u32 ra = 0;
  u32 error_conceal = 0;
  u32 major_version, minor_version, prod_id;

  task_handle task = NULL;
  u32 data_consumed = 0;
  u32 task_has_freed = 0;

  FILE *f_tbcfg;
  u32 seed_rnd = 0;
  u32 stream_bit_swap = 0;
  i32 corrupted_bytes = 0;
  u32 ds_ratio_x, ds_ratio_y;
  /* user input arguments */
  u32 scale_enabled = 0;
  u32 scaled_w = 0, scaled_h = 0;
  u32 crop_x = 0;
  u32 crop_y = 0;
  u32 crop_w = 0;
  u32 crop_h = 0;
  enum SCALE_MODE scale_mode;
  struct TestParams params;

  SetupDefaultParams(&params);
  scale_mode = NON_SCALE;

#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 8190; /* default to 8190 mode */
  h264_high_support = 1;
#endif

#ifdef H264_EVALUATION_8170
  g_hw_ver = 8170;
  h264_high_support = 0;
#elif H264_EVALUATION_8190
  g_hw_ver = 8190;
  h264_high_support = 1;
#elif H264_EVALUATION_9170
  g_hw_ver = 9170;
  h264_high_support = 1;
#elif H264_EVALUATION_9190
  g_hw_ver = 9190;
  h264_high_support = 1;
#elif H264_EVALUATION_G1
  g_hw_ver = 10000;
  h264_high_support = 1;
#endif

#ifdef TESTBENCH_WATCHDOG
  main_pid = getpid();
#endif

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */
#if 1
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
#endif

  for(i = 0; i < MAX_STRM_BUFFERS; i++) {
    stream_mem[i].virtual_address = NULL;
    stream_mem[i].bus_address = 0;
  }

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

#ifdef ASIC_TRACE_SUPPORT
    if (g_hw_ver != 8170)
      g_hw_ver = tb_cfg.dec_params.hw_version;
#endif
  }

  INIT_SW_PERFORMANCE;

  {
    H264DecApiVersion dec_api;
    H264DecBuild dec_build;
    u32 n_cores;

    /* Print API version number */
    dec_api = H264DecGetAPIVersion();
    dec_build = H264DecGetBuild();
    n_cores = H264DecMCGetCoreCount();
    DEBUG_PRINT(("\nX170 H.264 Decoder API v%d.%d - SW build: %d.%d - HW build: %x, %d cores\n\n",
                 dec_api.major, dec_api.minor, dec_build.sw_build >>16,
                 dec_build.sw_build & 0xFFFF, dec_build.hw_build, n_cores));
    prod_id = dec_build.hw_build >> 16;
    major_version = (dec_build.hw_build & 0xF000) >> 12;
    minor_version = (dec_build.hw_build & 0x0FF0) >> 4;

    /* number of stream buffers to allocate */
    allocated_buffers = n_cores + 1;

    /* for VC8000D, the max suporrted stream length is 32bit, consider SW memory allocation capability,
           the limitation is set to 2^30 Bytes length */
    if (prod_id == 0x6731)
      max_strm_len = DEC_X170_MAX_STREAM;
    else
      max_strm_len = DEC_X170_MAX_STREAM_VC8000D;
  }

  /* Check that enough command line arguments given, if not -> print usage
   * information out */
  if(argc < 2) {
    PrintUsageH264(argv[0]);
    return 0;
  }

  if(argc == 2) {
    if ((strncmp(argv[1], "-H", 2) == 0) ||
            (strcmp(argv[1], "--help") == 0)){
      PrintUsageH264(argv[0]);
      return 0;
    }
  }

  /* read command line arguments */
  for(i = 1; i < (u32) (argc - 1); i++) {
    if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_pics = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      if (k < DEC_MAX_OUT_COUNT) {
        for(j = k; j < DEC_MAX_OUT_COUNT; j++) {
          strcpy(out_file_name[j], argv[i] + 2);
        }
        k++;
      }
    } else if(strcmp(argv[i], "-X") == 0) {
      disable_output_writing = 1;
    } else if (strncmp(argv[i], "-E", 2) == 0)
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    else if (strncmp(argv[i], "-G", 2) == 0)
      convert_tiled_output = 1;
    else if(strcmp(argv[i], "-R") == 0) {
      disable_output_reordering = 1;
    } else if(strcmp(argv[i], "-W") == 0) {
      force_whole_stream = 1;
    } else if(strcmp(argv[i], "-L") == 0) {
      long_stream = 1;
#if 0
    } else if(strcmp(argv[i], "-P") == 0 ||
              strcmp(argv[i], "--planar") == 0) {
      planar_output = 1;
#endif
    } else if(strcmp(argv[i], "-a") == 0 ||
              strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = pp_planar_out;
      //tb_cfg.pp_units_params[0].planar = pp_planar_out;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "-I") == 0) {
      save_index = 1;
    } else if(strcmp(argv[i], "--mvc") == 0) {
      enable_mvc = 1;
    } else if(strcmp(argv[i], "-V") == 0) {
      mvc_separate_views = 1;
    } else if(strcmp(argv[i], "-Q") == 0) {
      skip_non_reference = 1;
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
#ifdef ASIC_TRACE_SUPPORT
    else if(strcmp(argv[i], "-F") == 0) {
      g_hw_ver = 8170;
      h264_high_support = 0;
      printf("\n\nForce 8170 mode to HW model!!!\n\n");
    } else if(strcmp(argv[i], "-B") == 0) {
      h264_high_support = 0;
      printf("\n\nForce Baseline configuration to 8190 HW model!!!\n\n");
    }
#endif
    else if(strcmp(argv[i], "-Z") == 0) {
      use_peek_output = 1;
    } else if(strcmp(argv[i], "-m") == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
    } else if(strcmp(argv[i], "--ultra-low-latency") == 0) {
      low_latency_sim = 1;
    } else if(strcmp(argv[i], "--platform-low-latency") == 0) {
      low_latency = 1;
    } else if(strcmp(argv[i], "--secure") == 0) {
      secure = 1;
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
      scale_enabled = 1;
      pp_enabled = 1;
#if 0
      if (!(major_version == 7 && minor_version == 1)) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return 1;
      }
#endif
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
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return 1;
      } else if (!px) {
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
      pp_enabled = 1;
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return 1;
      }
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
    } else if(strcmp(argv[i], "--E1010") == 0) {
      pp_enabled = 1;
      out_1010 = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].out_1010 = out_1010;
      //tb_cfg.pp_units_params[0].out_1010 = out_1010;
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
      //cr_first = 1;
      //tb_cfg.pp_units_params[0].cr_first = cr_first;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].cr_first = 1;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "--pp-luma-only") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].monochrome = 1;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "--ec") == 0) {
      error_conceal = 1;
    } else if(strcmp(argv[i], "--mc") == 0) {
      enable_mc = 1;
#ifndef SUPPORT_MULTI_CORE
      DEBUG_PRINT(("Parameter \"--mc\" available only when macro SUPPORT_MULTI_CORE is defined.\n"));
      return 1;
#endif
    } else if ((strncmp(argv[i], "-h", 2) == 0) ||
              (strcmp(argv[i], "--help") == 0)){
      PrintUsageH264(argv[0]);
    } else if (strcmp(argv[i], "--pp-only") == 0) {
      pp_enabled = 1;
      params.pp_standalone = 1;
    } else if (strcmp(argv[i], "-U") == 0) {
      //pp_tile_out = 1;
      //tb_cfg.pp_units_params[0].tiled_e = pp_tile_out;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].tiled_e = 1;
      pp_enabled = 1;
    } else {
      DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
      return 1;
    }
  }
#if 0
   if (pp_enabled && !scale_enabled) {
     scale_enabled = 1;
     ds_ratio_x = 1;
     ds_ratio_y = 1;
     scale_mode = FIXED_DOWNSCALE;
   }
#endif
   if (params.pp_standalone)
    tb_cfg.pp_params.pipeline_e = 0;
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
    if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
      tb_cfg.dec_params.cache_support = 1;
    else
      tb_cfg.dec_params.cache_support = 0;
#endif
#endif
  /* open input file for reading, file name given by user. If file open
   * fails -> exit */
  in_file_name = argv[argc - 1];
  finput = fopen(argv[argc - 1], "rb");
  if(finput == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE\n"));
    return -1;
  }

  if ((enable_mc && !enable_mvc) || low_latency) {
    libav_init();

    if(libav_open(in_file_name) < 0)
      return -1;
  }

  /* open index file for saving */
  if(save_index) {
    findex = fopen("stream.cfg", "w");
    if(findex == NULL) {
      DEBUG_PRINT(("UNABLE TO OPEN INDEX FILE\n"));
      return -1;
    }
  }
  /* try open index file */
  else {
    findex = fopen("stream.cfg", "r");
    if(findex != NULL) {
      use_index = 1;
    }

  }

  /*TBPrintCfg(&tb_cfg); */
  mb_error_concealment = 0; /* TBGetDecErrorConcealment(&tb_cfg); */
  rlc_mode = TBGetDecRlcModeForced(&tb_cfg);
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  output_format = TBGetDecOutputFormat(&tb_cfg);
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);
  //pp_only = TBGetPpPipelineEnabled(&tb_cfg) == 0;
  /*#if MD5SUM
      output_picture_endian = DEC_X170_LITTLE_ENDIAN;
      printf("Decoder Output Picture Endian forced to %d\n",
             output_picture_endian);
  #endif*/
  DEBUG_PRINT(("Decoder Macro Block Error Concealment %d\n",
               mb_error_concealment));
  DEBUG_PRINT(("Decoder RLC %d\n", rlc_mode));
  DEBUG_PRINT(("Decoder Clock Gating %d\n", clock_gating));
  DEBUG_PRINT(("Decoder Data Discard %d\n", data_discard));
  DEBUG_PRINT(("Decoder Latency Compensation %d\n", latency_comp));
  DEBUG_PRINT(("Decoder Output Picture Endian %d\n", output_picture_endian));
  DEBUG_PRINT(("Decoder Bus Burst Length %d\n", bus_burst_length));
  DEBUG_PRINT(("Decoder Asic Service Priority %d\n", asic_service_priority));
  DEBUG_PRINT(("Decoder Output Format %d\n", output_format));
  DEBUG_PRINT(("Decoder PP %s mode\n",
              TBGetPpPipelineEnabled(&tb_cfg) ? "Pipelined" : "Standalone"));

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

  packetize = TBGetTBPacketByPacket(&tb_cfg);
  nal_unit_stream = TBGetTBNalUnitStream(&tb_cfg);

  if (enable_mc) {
    long_stream = 1;
    packetize = 1;
    nal_unit_stream = 0;
  }

  DEBUG_PRINT(("TB Packet by Packet  %d\n", packetize));
  DEBUG_PRINT(("TB Nal Unit Stream %d\n", nal_unit_stream));
  DEBUG_PRINT(("TB Seed Rnd %d\n", seed_rnd));
  DEBUG_PRINT(("TB Stream Truncate %d\n", stream_truncate));
  DEBUG_PRINT(("TB Stream Header Corrupt %d\n", stream_header_corrupt));
  DEBUG_PRINT(("TB Stream Bit Swap %d; odds %s\n", stream_bit_swap,
               tb_cfg.tb_params.stream_bit_swap));
  DEBUG_PRINT(("TB Stream Packet Loss %d; odds %s\n", stream_packet_loss,
               tb_cfg.tb_params.stream_packet_loss));

  {
    remove("regdump.txt");
    remove("mbcontrol.hex");
    remove("intra4x4_modes.hex");
    remove("motion_vectors.hex");
    remove("rlc.hex");
    remove("picture_ctrl_dec.trc");
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  /* open tracefiles */
  tmp = OpenAsicTraceFiles();
  if(!tmp) {
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
  }
  if(nal_unit_stream)
    decoding_tools.stream_mode.nal_unit_strm = 1;
  else
    decoding_tools.stream_mode.byte_strm = 1;
#endif

  dwl_inst = DWLInit(&dwl_init);
  if(dwl_inst == NULL) {
    DEBUG_PRINT(("DWLInit# ERROR: DWL Init failed\n"));
    goto end;
  }
  /* initialize decoder. If unsuccessful -> exit */
  START_SW_PERFORMANCE;
  {
    dec_cfg.dpb_flags = 0;
    if( tiled_output )   dec_cfg.dpb_flags |= DEC_REF_FRM_TILED_DEFAULT;
    if( dpb_mode == DEC_DPB_INTERLACED_FIELD )
      dec_cfg.dpb_flags |= DEC_DPB_ALLOW_FIELD_ORDERING;

    if (enable_mc) {
      dec_cfg.mcinit_cfg.mc_enable = 1;
      dec_cfg.mcinit_cfg.stream_consumed_callback = StreamBufferConsumed;
    }
    dec_cfg.decoder_mode = DEC_NORMAL;
    if (low_latency_sim)
      dec_cfg.decoder_mode = DEC_LOW_LATENCY_RTL;
    if (low_latency)
      dec_cfg.decoder_mode = DEC_LOW_LATENCY;
    if (secure)
      dec_cfg.decoder_mode = DEC_SECURITY;

    /* TODO: set error_handling based on TBGetDecErrorConcealment() */
    // TBGetDecErrorConcealment(&tb_cfg)
    dec_cfg.error_handling = DEC_EC_FAST_FREEZE;
    dec_cfg.no_output_reordering = disable_output_reordering;
    dec_cfg.use_display_smoothing = use_display_smoothing;
    dec_cfg.use_video_compressor = 0;
    dec_cfg.use_adaptive_buffers = 1;
    dec_cfg.guard_size = 0;
    ret =
      H264DecInit(&dec_inst,
                  dwl_inst,
                  &dec_cfg);
  }
  END_SW_PERFORMANCE;
  if(ret != DEC_OK) {
    DEBUG_PRINT(("DECODER INITIALIZATION FAILED\n"));
    goto end;
  }

  if (enable_mc)
    sem_init(&stream_buff_free, 0, allocated_buffers);
  else
    allocated_buffers = 1;

  /* configure decoder to decode both views of MVC stereo high streams */
  if (enable_mvc)
    H264DecSetMvc(dec_inst);

  /* Set ref buffer test mode */
  //((struct H264DecContainer *) dec_inst)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((struct H264DecContainer *) dec_inst)->h264_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  if(rlc_mode) {
    /*Force the decoder into RLC mode */
    ((struct H264DecContainer *) dec_inst)->force_rlc_mode = 1;
    ((struct H264DecContainer *) dec_inst)->rlc_mode = 1;
    ((struct H264DecContainer *) dec_inst)->try_vlc = 0;
  }

#ifdef _ENABLE_2ND_CHROMA
  if (!TBGetDecCh8PixIleavOutput()) {
    ((struct H264DecContainer *) dec_inst)->storage.enable2nd_chroma = 0;
  } else {
    /* cropping not supported when additional chroma output format used */
    crop_display = 0;
  }
#endif

  TBInitializeRandom(seed_rnd);

  /* check size of the input file -> length of the stream in bytes */
  fseek(finput, 0L, SEEK_END);
  strm_len = ftell(finput);
  stream_size = strm_len;
  rewind(finput);

  dec_input.skip_non_reference = skip_non_reference;

  if(!long_stream) {
    /* If the decoder can not handle the whole stream at once, force packet-by-packet mode */
    if(!force_whole_stream) {
      if(strm_len > max_strm_len) {
        packetize = 1;
      }
    } else {
      if(strm_len > max_strm_len) {
        packetize = 0;
        strm_len = max_strm_len;
      }
    }

    /* sets the stream length to random value */
    if(stream_truncate && !packetize && !nal_unit_stream) {
      DEBUG_PRINT(("strm_len %ld\n", strm_len));
      ret = TBRandomizeU32((u32 *)&strm_len);
      if(ret != 0) {
        DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
        goto end;
      }
      DEBUG_PRINT(("Randomized strm_len %ld\n", strm_len));
    }

    /* NOTE: The DWL should not be used outside decoder SW.
     * here we call it because it is the easiest way to get
     * dynamically allocated linear memory
     * */

    /* allocate memory for stream buffer. if unsuccessful -> exit */
#ifndef ADS_PERFORMANCE_SIMULATION
    stream_mem[0].mem_type = DWL_MEM_TYPE_SLICE;
    if(DWLMallocLinear(dwl_inst,
                       strm_len + 1, &stream_mem[0]) != DWL_OK) {
      DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
      goto end;
    }
#else
    stream_mem[0].virtual_address = malloc(strm_len);
    stream_mem[0].bus_address = (size_t) stream_mem[0].virtual_address;
#endif

    byte_strm_start = (u8 *) stream_mem[0].virtual_address;

    if(byte_strm_start == NULL) {
      DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
      goto end;
    }

    /* read input stream from file to buffer and close input file */
    if(low_latency) {
      send_strm_len = strm_len;
      send_strm_info.strm_bus_addr = send_strm_info.strm_bus_start_addr = stream_mem[0].bus_address;
      send_strm_info.strm_vir_addr = send_strm_info.strm_vir_start_addr = (u8*)stream_mem[0].virtual_address;
      send_strm_info.low_latency = 1;
      sem_init(&curr_frame_end_sem, 0, 0);
      sem_init(&next_frame_start_sem, 0, 0);
      pthread_mutex_init(&send_mutex, NULL);
      task = run_task(send_bytestrm_task, NULL);
    } else {
      ra = fread(byte_strm_start, 1, strm_len, finput);
      (void) ra;
    }

    /* initialize H264DecDecode() input structure */
    stream_stop = byte_strm_start + strm_len;
    dec_input.stream = byte_strm_start;
    dec_input.stream_bus_address = (addr_t) stream_mem[0].bus_address;
    dec_input.data_len = strm_len;
    dec_input.buff_len = strm_len;
  } else {
    u32 size = max_strm_len;
    if (enable_mc) size = 4096*1165;
#ifndef ADS_PERFORMANCE_SIMULATION
    for(i = 0; i < allocated_buffers; i++) {
      stream_mem[i].mem_type = DWL_MEM_TYPE_SLICE;
      if(DWLMallocLinear(dwl_inst,
                        size, stream_mem + i) != DWL_OK) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }
    }
#else
    for(i = 0; i < allocated_buffers; i++) {
      stream_mem[i].virtual_address = malloc(size);
      stream_mem[i].bus_address = (size_t) stream_mem.virtual_address;

      if(stream_mem[i].virtual_address == NULL) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }
    }
#endif

    /* initialize H264DecDecode() input structure */
    int id;
    if (enable_mc && !force_to_single_core)
      id = GetFreeStreamBuffer();
    else
      id = 0;

    dec_input.stream = (u8 *) stream_mem[id].virtual_address;
    dec_input.stream_bus_address = (addr_t) stream_mem[id].bus_address;
    dec_input.buff_len = size;
    /* stream processed callback param */
    dec_input.p_user_data = stream_mem[id].virtual_address;
  }

  if(long_stream && !packetize && !nal_unit_stream) {
    if(use_index == 1) {
      u32 amount = 0;
      cur_index = 0;

      /* read index */
      /* addr_t defined same between LP32 and LP64*/
	  ra = fscanf(findex, "%zu", &next_index);
	  (void) ra;
      {
        /* check if last index -> stream end */
        u8 byte[2];
        ra = fread(&byte, 2, 1, findex);
        (void) ra;
        if(feof(findex)) {
          DEBUG_PRINT(("STREAM WILL END\n"));
          stream_will_end = 1;
        } else {
          fseek(findex, -2, SEEK_CUR);
        }
      }

      amount = next_index - cur_index;
      cur_index = next_index;

      /* read first block */
      dec_input.data_len = fread((u8 *)dec_input.stream, 1, amount, finput);
    } else {
      dec_input.data_len =
        fread((u8 *)dec_input.stream, 1, max_strm_len, finput);
    }
    /*DEBUG_PRINT(("BUFFER READ\n")); */
    if(feof(finput)) {
      DEBUG_PRINT(("STREAM WILL END\n"));
      stream_will_end = 1;
    }
  }

  else {
    if(use_index) {
      if(!nal_unit_stream) {
		ra = fscanf(findex, "%zu", &cur_index);
		(void) ra;
      }
    }

    /* get pointer to next packet and the size of packet
     * (for packetize or nal_unit_stream modes) */
    ptmpstream = dec_input.stream;
    if(low_latency) {
      while (send_strm_info.strm_vir_addr <= ptmpstream)
        sched_yield();
      dec_input.data_len = send_strm_info.strm_vir_addr - ptmpstream;
      printf("input.data_len = %d\n", dec_input.data_len);
    } else if((tmp = NextPacket((u8 **) (&dec_input.stream))) != 0) {
      dec_input.data_len = tmp;
      dec_input.stream_bus_address += (addr_t) (dec_input.stream - ptmpstream);
    }
  }

  pic_decode_number = pic_display_number = 1;
  if(!output_thread_run) {
    output_thread_run = 1;
    sem_init(&buf_release_sem, 0, 0);
    pthread_create(&output_thread, NULL, h264_output_thread, NULL);
    pthread_create(&release_thread, NULL, buf_release_thread, NULL);
  }

  /* main decoding loop */
  do {
    save_flag = 1;
    /*DEBUG_PRINT(("dec_input.data_len %d\n", dec_input.data_len));
     * DEBUG_PRINT(("dec_input.stream %d\n", dec_input.stream)); */

    if(stream_truncate && pic_rdy && (hdrs_rdy || stream_header_corrupt) &&
        (long_stream || (!long_stream && (packetize || nal_unit_stream)))) {
      i32 ret;

      ret = TBRandomizeU32(&dec_input.data_len);
      if(ret != 0) {
        DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
        return 0;
      }
      DEBUG_PRINT(("Randomized stream size %d\n", dec_input.data_len));
    }

    /* If enabled, break the stream */
    if(stream_bit_swap) {
      if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
        /* Picture needs to be ready before corrupting next picture */
        if(pic_rdy && corrupted_bytes <= 0) {
          ret =
            TBRandomizeBitSwapInStream((u8 *)dec_input.stream,
                                       dec_input.data_len,
                                       tb_cfg.tb_params.
                                       stream_bit_swap);
          if(ret != 0) {
            DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
            goto end;
          }

          corrupted_bytes = dec_input.data_len;
          DEBUG_PRINT(("corrupted_bytes %d\n", corrupted_bytes));
        }
      }
    }

    /* Picture ID is the picture number in decoding order */
    dec_input.pic_id = pic_decode_number;

    /* test decAbort function */
    if (abort_test && pic_decode_number > 3) {
      abort_test = 0;
      H264DecAbort(dec_inst);
    }

    dec_input.buffer = (u8 *)((addr_t)dec_input.stream & (~BUFFER_ALIGN_MASK));
    dec_input.buffer_bus_address = dec_input.stream_bus_address & (~BUFFER_ALIGN_MASK);
    dec_input.buff_len = dec_input.data_len + (dec_input.stream_bus_address & BUFFER_ALIGN_MASK);

    /* call API function to perform decoding */
    START_SW_PERFORMANCE;
    ret = H264DecDecode(dec_inst, &dec_input, &dec_output);
    END_SW_PERFORMANCE;
    printDecodeReturn(ret);

    switch (ret) {
    case DEC_STREAM_NOT_SUPPORTED: {
      DEBUG_PRINT(("ERROR: UNSUPPORTED STREAM!\n"));
      goto end;
    }
    case DEC_HDRS_RDY: {
#ifdef DPB_REALLOC_DISABLE
      if(hdrs_rdy) {
        DEBUG_PRINT(("Decoding ended, flush the DPB\n"));
        /* the end of stream is not reached yet */
        H264DecEndOfStream(dec_inst, 0);
      }
#endif
      save_flag = 0;
      /* Set a flag to indicate that headers are ready */
      hdrs_rdy = 1;
      TBSetRefbuMemModel( &tb_cfg,
                          ((struct H264DecContainer *) dec_inst)->h264_regs,
                          &((struct H264DecContainer *) dec_inst)->ref_buffer_ctrl );

      /* Stream headers were successfully decoded
       * -> stream information is available for query now */

      START_SW_PERFORMANCE;
      tmp = H264DecGetInfo(dec_inst, &dec_info);
      END_SW_PERFORMANCE;
      if(tmp != DEC_OK) {
        DEBUG_PRINT(("ERROR in getting stream info!\n"));
        goto end;
      }

      DEBUG_PRINT(("Width %d Height %d\n",
                   dec_info.pic_width, dec_info.pic_height));

      DEBUG_PRINT(("Cropping params: (%d, %d) %dx%d\n",
                   dec_info.crop_params.crop_left_offset,
                   dec_info.crop_params.crop_top_offset,
                   dec_info.crop_params.crop_out_width,
                   dec_info.crop_params.crop_out_height));

      DEBUG_PRINT(("MonoChrome = %d\n", dec_info.mono_chrome));
      DEBUG_PRINT(("Interlaced = %d\n", dec_info.interlaced_sequence));
      DEBUG_PRINT(("DPB mode   = %d\n", dec_info.dpb_mode));
      DEBUG_PRINT(("Pictures in DPB = %d\n", dec_info.pic_buff_size));
      DEBUG_PRINT(("Pictures in Multibuffer PP = %d\n", dec_info.multi_buff_pp_size));

      if (dec_info.interlaced_sequence)
        force_to_single_core = 1;
#if 0
      if (!tb_cfg.pp_units_params[0].unit_enabled) {
        if (pp_enabled) {
          /* Command line options overrides PPU0 if it's not enabed in tb.cfg */
          if (crop_enabled) {
            tb_cfg.pp_units_params[0].crop_x = crop_x;
            tb_cfg.pp_units_params[0].crop_y = crop_y;
            tb_cfg.pp_units_params[0].crop_width = crop_w;
            tb_cfg.pp_units_params[0].crop_height = crop_h;
          } else {
            tb_cfg.pp_units_params[0].crop_x = 0;
            tb_cfg.pp_units_params[0].crop_y = 0;
            tb_cfg.pp_units_params[0].crop_width = dec_info.pic_width;
            tb_cfg.pp_units_params[0].crop_height = dec_info.pic_height;
          }
          if (scale_mode == FIXED_DOWNSCALE) {
            tb_cfg.pp_units_params[0].scale_width =
              tb_cfg.pp_units_params[0].crop_width / ds_ratio_x;
            tb_cfg.pp_units_params[0].scale_height =
              tb_cfg.pp_units_params[0].crop_height / ds_ratio_y;
          } else {
            tb_cfg.pp_units_params[0].scale_width = scaled_w;
            tb_cfg.pp_units_params[0].scale_height = scaled_h;
          }
          tb_cfg.pp_units_params[0].cr_first = cr_first;
          tb_cfg.pp_units_params[0].tiled_e = pp_tile_out;
          tb_cfg.pp_units_params[0].planar = pp_planar_out;
          tb_cfg.pp_units_params[0].ystride = ystride;
          tb_cfg.pp_units_params[0].cstride = cstride;
          tb_cfg.pp_units_params[0].out_1010 = out_1010;
        }
        tb_cfg.pp_units_params[0].unit_enabled = pp_enabled;
      }

      for (i = 0; i < 4; i++) {
        dec_cfg.ppu_config[i].enabled = tb_cfg.pp_units_params[i].unit_enabled;
        dec_cfg.ppu_config[i].cr_first = tb_cfg.pp_units_params[i].cr_first;
        dec_cfg.ppu_config[i].tiled_e = tb_cfg.pp_units_params[i].tiled_e;
        dec_cfg.ppu_config[i].crop.enabled = 1;
        dec_cfg.ppu_config[i].crop.x = tb_cfg.pp_units_params[i].crop_x;
        dec_cfg.ppu_config[i].crop.y = tb_cfg.pp_units_params[i].crop_y;
        dec_cfg.ppu_config[i].crop.width = tb_cfg.pp_units_params[i].crop_width;
        dec_cfg.ppu_config[i].crop.height = tb_cfg.pp_units_params[i].crop_height;
        dec_cfg.ppu_config[i].scale.enabled = 1;
        dec_cfg.ppu_config[i].scale.width = tb_cfg.pp_units_params[i].scale_width;
        dec_cfg.ppu_config[i].scale.height = tb_cfg.pp_units_params[i].scale_height;
        dec_cfg.ppu_config[i].shaper_enabled = tb_cfg.pp_units_params[i].shaper_enabled;
        dec_cfg.ppu_config[i].monochrome = tb_cfg.pp_units_params[i].monochrome;
        dec_cfg.ppu_config[i].planar = tb_cfg.pp_units_params[i].planar;
        dec_cfg.ppu_config[i].out_p010 = tb_cfg.pp_units_params[i].out_p010;
        dec_cfg.ppu_config[i].out_1010 = tb_cfg.pp_units_params[i].out_1010;
        dec_cfg.ppu_config[i].out_cut_8bits = tb_cfg.pp_units_params[i].out_cut_8bits;
        dec_cfg.ppu_config[i].ystride = tb_cfg.pp_units_params[i].ystride;
        dec_cfg.ppu_config[i].cstride = tb_cfg.pp_units_params[i].cstride;
        dec_cfg.ppu_config[i].align = align;
        if (dec_cfg.ppu_config[i].enabled)
          pp_enabled = 1;
      }
#else
      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);
      if (params.fscale_cfg.fixed_scale_enabled) {
        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;
        if (!crop_w) crop_w = dec_info.pic_width;
        if (!crop_h) crop_h = dec_info.pic_height;
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
#endif
      dec_cfg.error_conceal = error_conceal;
      dec_cfg.decoder_mode = low_latency ? DEC_LOW_LATENCY :
                             low_latency_sim ? DEC_LOW_LATENCY_RTL :
                             secure ? DEC_SECURITY : DEC_NORMAL;
      dec_cfg.align = align;
      /* sync align set with H264DecInit() */
      if (align == DEC_ALIGN_1B)
        dec_cfg.align = DEC_ALIGN_128B;

      tmp = H264DecSetInfo(dec_inst, &dec_cfg);
      if (tmp != DEC_OK) {
        DEBUG_PRINT(("Invalid pp parameters\n"));
        goto end;
      }
      /* To get right info, so that tmp_image size is right*/
      START_SW_PERFORMANCE;
      tmp = H264DecGetInfo(dec_inst, &dec_info);
      END_SW_PERFORMANCE;

      if(dec_info.output_format == DEC_OUT_FRM_TILED_4X4)
        DEBUG_PRINT(("Output format = DEC_OUT_FRM_TILED_4X4\n"));
      else if(dec_info.output_format == DEC_OUT_FRM_MONOCHROME)
        DEBUG_PRINT(("Output format = DEC_OUT_FRM_MONOCHROME\n"));
      else
        DEBUG_PRINT(("Output format = DEC_OUT_FRM_RASTER_SCAN\n"));
      //prev_width = dec_info.pic_width;
      //prev_height = dec_info.pic_height;
      //min_buffer_num = dec_info.pic_buff_size;
      dpb_mode = dec_info.dpb_mode;

      /* release the old temp image buffer if exists */
      if(tmp_image)
        free(tmp_image);

      /* check if we do need to crop */
      if(dec_info.crop_params.crop_left_offset == 0 &&
          dec_info.crop_params.crop_top_offset == 0 &&
          dec_info.crop_params.crop_out_width == dec_info.pic_width &&
          dec_info.crop_params.crop_out_height == dec_info.pic_height ) {
        crop_display = 0;
      }

      if(crop_display) {
        /* Cropped frame size in planar YUV 4:2:0 */
        pic_size = dec_info.crop_params.crop_out_width *
                   dec_info.crop_params.crop_out_height;
        if(!dec_info.mono_chrome)
          pic_size = (3 * pic_size) / 2;
        tmp_image = malloc(pic_size);
        if(tmp_image == NULL) {
          DEBUG_PRINT(("ERROR in allocating cropped image!\n"));
          goto end;
        }
      } else {
        if (!dec_info.pp_enabled) {
          pic_size = (NEXT_MULTIPLE(dec_info.pic_width *
                                   dec_info.bit_depth * 4, ALIGN(dec_cfg.align) * 8) / 8) * 
                     dec_info.pic_height / 4;
        /* Decoder output frame size in planar YUV 4:2:0 */
        } else if (scale_enabled && scale_mode == FLEXIBLE_SCALE) {
          pic_size = NEXT_MULTIPLE(scaled_w, ALIGN(dec_cfg.align)) * scaled_h;
          //pic_size = ((scaled_w + 15) & ~15) * scaled_h;
        } else {
          pic_size = NEXT_MULTIPLE(dec_info.pic_width, ALIGN(dec_cfg.align)) *
                     dec_info.pic_height;
        }
        if(!dec_info.mono_chrome)
          pic_size = (3 * pic_size) / 2;
        if (use_peek_output)
          tmp_image = malloc(pic_size);
      }

      DEBUG_PRINT(("video_range %d, matrix_coefficients %d\n",
                   dec_info.video_range, dec_info.matrix_coefficients));

      /* If -O option not used, generate default file name */
      /*
      if(out_file_name[0] == 0)
        sprintf(out_file_name, "out.yuv");
      */

      break;
    }
    case DEC_ADVANCED_TOOLS: {

      if (enable_mc) {
        /* ASO/FMO detected and not supported in multicore mode */
        DEBUG_PRINT(("ASO/FMO detected in multicore, decoding will stop\n"));
        goto end;
      }
      /* ASO/STREAM ERROR was noticed in the stream. The decoder has to
       * reallocate resources */
      assert(dec_output.data_left); /* we should have some data left *//* Used to indicate that picture decoding needs to finalized prior to corrupting next picture */

      /* Used to indicate that picture decoding needs to finalized prior to corrupting next picture
       * pic_rdy = 0; */
#ifdef LOW_LATENCY_FRAME_MODE
      if(low_latency) {
        process_end_flag = 1;
        pic_decoded = 1;
        sem_post(&next_frame_start_sem);
        wait_for_task_completion(task);
        dec_input.data_len = 0;
        task_has_freed = 1;
      }
#endif
      break;
    }
    case DEC_PENDING_FLUSH:
    case DEC_PIC_DECODED:
      /* case DEC_FREEZED_PIC_RDY: */
      /* Picture is now ready */
      pic_rdy = 1;

      /*lint -esym(644,tmp_image,pic_size) variable initialized at
       * DEC_HDRS_RDY_BUFF_NOT_EMPTY case */

      if (ret == DEC_PIC_DECODED) {
        /* If enough pictures decoded -> force decoding to end
         * by setting that no more stream is available */
        if(pic_decode_number == max_num_pics) {
          process_end_flag = 1;
          dec_input.data_len = 0;
        }

        printf("DECODED PICTURE %d\n", pic_decode_number);
        if (pic_decode_number==298)
        printf("AAAA\n");
        /* Increment decoding number for every decoded picture */
        pic_decode_number++;
      }

      if(!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, h264_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_peek_output && ret == DEC_PIC_DECODED &&
          H264DecPeek(dec_inst, &dec_picture) == DEC_PIC_RDY) {
        static u32 last_field = 0;

        /* pic coding type */
        printH264PicCodingType(dec_picture.pic_coding_type);

        DEBUG_PRINT((", DECPIC %d\n", pic_decode_number));

        /* Write output picture to file. If output consists of separate fields -> store
         * whole frame before writing */
        image_data = (u8 *) dec_picture.pictures[0].output_picture;
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(dec_picture.pictures[0].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(dec_picture.pictures[0].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(dec_picture.pictures[0].output_format) ||
                  dec_picture.pictures[0].output_picture_chroma == NULL)
          pp_mono_chrome = 1;

        if (dec_picture.field_picture) {
          if (last_field == 0)
            memcpy(tmp_image, image_data, pic_size);
          else {
            u32 i;
            u8 *p_in = image_data, *p_out = tmp_image;
            if (dec_picture.top_field == 0) {
              p_out += dec_picture.pictures[0].pic_width;
              p_in += dec_picture.pictures[0].pic_width;
            }
            tmp = dec_info.mono_chrome ?
                  dec_picture.pictures[0].pic_height / 2 :
                  3 * dec_picture.pictures[0].pic_height / 4;
            for (i = 0; i < tmp; i++) {
              memcpy(p_out, p_in, dec_picture.pictures[0].pic_width);
              p_in += 2*dec_picture.pictures[0].pic_width;
              p_out += 2*dec_picture.pictures[0].pic_width;
            }
          }
          last_field ^= 1;
        } else {
          if (last_field) {
            WriteOutput(out_file_name[0],
                        tmp_image,
                        dec_picture.pictures[0].pic_width,
                        dec_picture.pictures[0].pic_height,
                        pic_decode_number - 2,
                        pp_mono_chrome,
                        dec_picture.view_id, mvc_separate_views,
                        disable_output_writing,
                        dec_picture.pictures[0].output_format == DEC_OUT_FRM_TILED_4X4,
                        dec_picture.pictures[0].pic_stride,
                        dec_picture.pictures[0].pic_stride_ch, 0,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        convert_to_frame_dpb, dpb_mode, md5sum,
                        ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[0]) : &(foutput[0])), 1);

#ifdef _ENABLE_2ND_CHROMA
            WriteOutput2ndChroma(pic_size,
                        pic_decode_number - 2,
                        dec_info.mono_chrome || (dec_picture.pictures[0].output_format == DEC_OUT_FRM_YUV400),
                        dec_picture.view_id, mvc_separate_views, 0, md5sum,
                        ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[0]) : &(foutput[0])));
#endif
          }
          last_field = 0;
          memcpy(tmp_image, image_data, pic_size);
        }

        if (!last_field) {
          WriteOutput(out_file_name[0],
                      tmp_image,
                      dec_picture.pictures[0].pic_width,
                      dec_picture.pictures[0].pic_height,
                      pic_decode_number - 1,
                      pp_mono_chrome,
                      dec_picture.view_id, mvc_separate_views,
                      disable_output_writing,
                      dec_picture.pictures[0].output_format == DEC_OUT_FRM_TILED_4X4,
                      dec_picture.pictures[0].pic_stride,
                      dec_picture.pictures[0].pic_stride_ch, 0,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      convert_to_frame_dpb, dpb_mode, md5sum,
                      ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[0]) : &(foutput[0])), 1);

#ifdef _ENABLE_2ND_CHROMA
          WriteOutput2ndChroma(pic_size,
                      pic_decode_number - 1,
                      dec_info.mono_chrome || (dec_picture.pictures[0].output_format == DEC_OUT_FRM_YUV400),
                      dec_picture.view_id, mvc_separate_views, 0, md5sum,
                      ((dec_picture.view_id && mvc_separate_views) ? &(foutput2[0]) : &(foutput[0])));
#endif

        }
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      retry = 0;
      break;

    case DEC_STRM_PROCESSED:
    case DEC_BUF_EMPTY:
    case DEC_NONREF_PIC_SKIPPED:
    case DEC_NO_DECODING_BUFFER:
    case DEC_STRM_ERROR: {
      /* Used to indicate that picture decoding needs to finalized prior to corrupting next picture
       * pic_rdy = 0; */

      break;
    }

    case DEC_WAITING_FOR_BUFFER: {
      DEBUG_PRINT(("Waiting for frame buffers\n"));
      struct DWLLinearMem mem;

      rv = H264DecGetBufferInfo(dec_inst, &hbuf);
      DEBUG_PRINT(("H264DecGetBufferInfo ret %d\n", rv));
      DEBUG_PRINT(("buf_to_free %p, next_buf_size %d, buf_num %d\n",
                   (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));

      if (hbuf.buf_to_free.virtual_address != NULL) {
        while (H264DecGetBufferInfo(dec_inst, &hbuf) == DEC_WAITING_FOR_BUFFER) {
          if (hbuf.buf_to_free.virtual_address == NULL)
            break;
        }
        add_extra_flag = 0;
        ReleaseExtBuffers();
        buffer_release_flag = 1;
        num_buffers = 0;
      }

      buffer_size = hbuf.next_buf_size;
      if(buffer_release_flag && hbuf.next_buf_size) {
        /* Only add minimum required buffers at first. */
        //extra_buffer_num = hbuf.buf_num - min_buffer_num;
        for(i=0; i<hbuf.buf_num; i++) {
          mem.mem_type = DWL_MEM_TYPE_DPB;
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = H264DecAddBuffer(dec_inst, &mem);
          DEBUG_PRINT(("H264DecAddBuffer ret %d\n", rv));
          if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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
    }
    case DEC_OK:
      /* nothing to do, just call again */
      break;
    case DEC_HW_TIMEOUT:
      DEBUG_PRINT(("Timeout\n"));
      goto end;
    case DEC_ABORTED:
      DEBUG_PRINT(("H264 decoder is aborted: %d\n", ret));
      H264DecAbortAfter(dec_inst);
      break;
    case DEC_SYSTEM_ERROR:
      H264HandleFatalError();
      goto end;
    default:
      DEBUG_PRINT(("FATAL ERROR: %d\n", ret));
      goto end;
    }

    /* break out of do-while if max_num_pics reached (data_len set to 0) */
    if(dec_input.data_len == 0)
      break;

    if(long_stream && !packetize && !nal_unit_stream) {
      if(stream_will_end) {
        corrupted_bytes -= (dec_input.data_len - dec_output.data_left);
        dec_input.data_len = dec_output.data_left;
        dec_input.stream = dec_output.strm_curr_pos;
        dec_input.stream_bus_address = dec_output.strm_curr_bus_address;
      } else {
        if(use_index == 1) {
          if(dec_output.data_left) {
            dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
            corrupted_bytes -= (dec_input.data_len - dec_output.data_left);
            dec_input.data_len = dec_output.data_left;
            dec_input.stream = dec_output.strm_curr_pos;
          } else {
            dec_input.stream_bus_address = (addr_t) stream_mem[0].bus_address;
            dec_input.stream = (u8 *) stream_mem[0].virtual_address;
            dec_input.data_len = fillBuffer(dec_input.stream);
          }
        } else {
          if(fseek(finput, -(i32)dec_output.data_left, SEEK_CUR) == -1) {
            DEBUG_PRINT(("SEEK FAILED\n"));
            dec_input.data_len = 0;
          } else {
            /* store file index */
            if(save_index && save_flag) {
              fprintf(findex, "%" PRId64"\n", ftello64(finput));
            }

            dec_input.data_len =
              fread((u8 *)dec_input.stream, 1, max_strm_len, finput);
          }
        }

        if(feof(finput)) {
          DEBUG_PRINT(("STREAM WILL END\n"));
          stream_will_end = 1;
        }

        corrupted_bytes = 0;
      }
    }

    else {
      if (low_latency) {
        dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
        data_consumed += (u32)(dec_output.strm_curr_pos - dec_input.stream);
        dec_input.buff_len -= (u32)(dec_output.strm_curr_pos - dec_input.stream);
        corrupted_bytes -= (dec_output.strm_curr_pos - dec_input.stream);
        dec_input.data_len = dec_output.data_left;
        dec_input.stream = dec_output.strm_curr_pos;
        dec_input.buff_len -= data_consumed;
#ifndef LOW_LATENCY_FRAME_MODE
        if (ret == DEC_PIC_DECODED) {
          pic_decoded = 1;
          sem_wait(&curr_frame_end_sem);
          bytes_go_back = send_strm_info.strm_vir_addr - dec_input.stream;
          dec_input.stream_bus_address = (addr_t) stream_mem[0].bus_address;
          dec_input.stream = (u8 *) stream_mem[0].virtual_address;
          send_strm_info.strm_vir_addr = (u8 *) stream_mem[0].virtual_address;
          send_strm_info.strm_bus_addr = (addr_t) stream_mem[0].bus_address;
          send_strm_len -= (dec_output.strm_curr_pos - dec_input.stream);
          sem_post(&next_frame_start_sem);
        }
#else
        if (/*ret == DEC_PIC_DECODED &&*/ data_consumed >= frame_len) {
          data_consumed = 0;
          pic_decoded = 1;
          sem_wait(&curr_frame_end_sem);
          dec_input.stream_bus_address = (addr_t) stream_mem[0].bus_address;
          dec_input.stream = (u8 *) stream_mem[0].virtual_address;
          send_strm_info.strm_vir_addr = (u8 *) stream_mem[0].virtual_address;
          send_strm_info.strm_bus_addr = (addr_t) stream_mem[0].bus_address;
          dec_input.buff_len = stream_mem[0].logical_size;
          sem_post(&next_frame_start_sem);
        }
#endif
        while(1) {
          pthread_mutex_lock(&send_mutex);
          if(send_strm_info.strm_vir_addr > dec_input.stream || strm_last_flag == 1) {
            pthread_mutex_unlock(&send_mutex);
            break;
          }
          pthread_mutex_unlock(&send_mutex);
        }

        if(send_strm_info.strm_vir_addr > dec_input.stream)
          dec_input.data_len = send_strm_info.strm_vir_addr - dec_input.stream;
        else
          dec_input.data_len = 0;

        printf("Input.data_len = %d\n", dec_input.data_len);
      } else if(dec_output.data_left) {
        dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
        corrupted_bytes -= (dec_input.data_len - dec_output.data_left);
        dec_input.buff_len -= (dec_output.strm_curr_pos - dec_input.stream);
        dec_input.data_len = dec_output.data_left;
        dec_input.stream = dec_output.strm_curr_pos;
      } else {
        int id;
        if (enable_mc && !force_to_single_core)
          id = GetFreeStreamBuffer();
        else
          id = 0;

        dec_input.stream_bus_address = (addr_t) stream_mem[id].bus_address;
        dec_input.stream = (u8 *) stream_mem[id].virtual_address;
        /* stream processed callback param */
        dec_input.p_user_data = stream_mem[id].virtual_address;
        /*u32 streamPacketLossTmp = stream_packet_loss;
         *
         * if(!pic_rdy)
         * {
         * stream_packet_loss = 0;
         * } */
        ptmpstream = dec_input.stream;


        dec_input.data_len = NextPacket((u8 **) (&dec_input.stream));
        if(long_stream)
          tmp = ftell(finput);
        else
          tmp = dec_input.stream - byte_strm_start;
        printf("NextPacket = %d at %d\n", dec_input.data_len, tmp);

        if(dec_input.data_len == 0)
          process_end_flag = 1;

        dec_input.stream_bus_address +=
          (addr_t) (dec_input.stream - ptmpstream);

        /*stream_packet_loss = streamPacketLossTmp; */

        corrupted_bytes = 0;
      }
    }

    /* keep decoding until all data from input stream buffer consumed
     * and all the decoded/queued frames are ready */
  } while(dec_input.data_len > 0);

  /* store last index */
  if(save_index) {
    off64_t pos = ftello64(finput) - dec_output.data_left;
    fprintf(findex, "%" PRId64"\n", pos);
  }

  if(use_index || save_index) {
    fclose(findex);
  }

  DEBUG_PRINT(("Decoding ended, flush the DPB\n"));

end:

  add_buffer_thread_run = 0;

  H264DecEndOfStream(dec_inst, 1);

#ifdef _HAVE_PTHREAD_H
  if (output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
    output_thread_run = 0;
  }
#else //_HAVE_PTHREAD_H
  while(1)
  {
    if(last_pic_flag &&  buf_status[list_pop_index] == 0)
      break;
    h264_output_thread(NULL);
    buf_release_thread(NULL);
  }
#endif //_HAVE_PTHREAD_H

  if(low_latency && !task_has_freed) {
    process_end_flag = 1;
    pic_decoded = 1;
    sem_post(&next_frame_start_sem);
    wait_for_task_completion(task);
    task_has_freed = 1;
  }

  byte_strm_start = NULL;

  sem_destroy(&stream_buff_free);

  fflush(stdout);

  /* have to release stream buffers before releasing decoder as we need DWL */
  for(i = 0; i < allocated_buffers; i++) {
    if(stream_mem[i].virtual_address != NULL) {
#ifndef ADS_PERFORMANCE_SIMULATION
      if(dec_inst)
        DWLFreeLinear(dwl_inst, &stream_mem[i]);
#else
      free(stream_mem[i].virtual_address);
#endif
    }
  }

  /* release decoder instance */
  START_SW_PERFORMANCE;
  H264DecRelease(dec_inst);
  END_SW_PERFORMANCE;
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  DWLRelease(dwl_inst);

   if (enable_mc || low_latency)
     libav_release();
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if(foutput[i])
      fclose(foutput[i]);
    if(foutput2[i])
      fclose(foutput2[i]);
  }
  if(fchroma2)
    fclose(fchroma2);
  if(f_tiled_output)
    fclose(f_tiled_output);
  if(finput)
    fclose(finput);

  /* free allocated buffers */
  if(tmp_image != NULL)
    free(tmp_image);
  if(grey_chroma != NULL)
    free(grey_chroma);
  if(pic_big_endian)
    free(pic_big_endian);

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    foutput[i] = fopen(out_file_name[i], "rb");
    if(NULL == foutput[i]) {
      strm_len = 0;
    } else {
      fseek(foutput[i], 0L, SEEK_END);
      strm_len = (u32) ftell(foutput[i]);
      fclose(foutput[i]);
    }

    if (strm_len) {
      DEBUG_PRINT(("Output file: %s\n", out_file_name[i]));
      DEBUG_PRINT(("OUTPUT_SIZE %ld\n", strm_len));
    }
  }

  FINALIZE_SW_PERFORMANCE;

  DEBUG_PRINT(("DECODING DONE\n"));

#ifdef ASIC_TRACE_SUPPORT
  /* close trace files */
  CloseAsicTraceFiles();
#endif

  if(retry > NUM_RETRY) {
    return -1;
  }

  if(num_errors || pic_decode_number == 1) {
    DEBUG_PRINT(("ERRORS FOUND %d %d\n", num_errors, pic_decode_number));
    /*return 1;*/
    return 0;
  }

  return 0;
}

/*------------------------------------------------------------------------------

    Function name: WriteOutput2ndChroma

    Purpose:
        Write 2ndChroma data to file. Size of the
        picture in pixels is indicated by pic_size.

------------------------------------------------------------------------------*/
void
WriteOutput2ndChroma(u32 pic_size, u32 frame_number,
                 u32 mono_chrome, u32 view, u32 mvc_separate_views,
                 u32 index, u32 md5sum, FILE **fout) {

  if(*fout == NULL) {

    if (TBGetDecCh8PixIleavOutput() && !mono_chrome) {
      fchroma2 = fopen("out_ch8pix.yuv", "wb");
      if(fchroma2 == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN OUTPUT FILE\n"));
        return;
      }
    }
  }

  if (TBGetDecCh8PixIleavOutput() && !mono_chrome) {
    u8 *p_ch;
    addr_t tmp;
    enum DecRet ret;

    ret = H264DecNextChPicture(dec_inst, (u32 **)&p_ch, &tmp);
    ASSERT(ret == DEC_PIC_RDY);
    (void) ret;
#ifndef ASIC_TRACE_SUPPORT
    if(output_picture_endian == DEC_X170_BIG_ENDIAN) {
      if(pic_big_endian_size != pic_size/3) {
        if(pic_big_endian != NULL)
          free(pic_big_endian);

        pic_big_endian = (u8 *) malloc(pic_size/3);
        if(pic_big_endian == NULL) {
          DEBUG_PRINT(("MALLOC FAILED @ %s %d", __FILE__, __LINE__));
          return;
        }

        pic_big_endian_size = pic_size/3;
      }

      memcpy(pic_big_endian, p_ch, pic_size/3);
      TbChangeEndianess(pic_big_endian, pic_size/3);
      p_ch = pic_big_endian;
    }
#endif
    if (md5sum)
      TBWriteFrameMD5Sum(fchroma2, p_ch, pic_size/3, frame_number);
    else
      fwrite(p_ch, 1, pic_size/3, fchroma2);
  }
  return;
}



/*------------------------------------------------------------------------------

    Function name: NextPacket

    Purpose:
        Get the pointer to start of next packet in input stream. Uses
        global variables 'packetize' and 'nal_unit_stream' to determine the
        decoder input stream mode and 'stream_stop' to determine the end
        of stream. There are three possible stream modes:
            default - the whole stream at once
            packetize - a single NAL-unit with start code prefix
            nal_unit_stream - a single NAL-unit without start code prefix

        p_strm stores pointer to the start of previous decoder input and is
        replaced with pointer to the start of the next decoder input.

        Returns the packet size in bytes

------------------------------------------------------------------------------*/

u32 NextPacket(u8 ** p_strm) {
  u32 index;
  u32 max_index;
  u32 zero_count;
  u8 byte;
  static u32 prev_index = 0;
  static u8 *stream = NULL;
  u8 next_packet = 0;

  /* Next packet is read from file is long stream support is enabled */
  if(long_stream) {
    /* libav does not support MVC */
    if (enable_mc && enable_mvc) {
      return NextNALFromFile(finput, *p_strm, stream_mem->size);
    } else if (enable_mc || low_latency)
      return libav_read_frame((char *)*p_strm, stream_mem->size);
    else
      return NextPacketFromFile(p_strm);
  }

  /* For default stream mode all the stream is in first packet */
  if(!packetize && !nal_unit_stream)
    return 0;

  /* If enabled, loose the packets (skip this packet first though) */
  if(stream_packet_loss) {
    u32 ret = 0;

    ret =
      TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, &next_packet);
    if(ret != 0) {
      DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
      return 0;
    }
  }

  index = 0;

  if(stream == NULL)
    stream = *p_strm;
  else
    stream += prev_index;

  max_index = (u32) (stream_stop - stream);

  if(stream > stream_stop)
    return (0);

  if(max_index == 0)
    return (0);

  /* leading zeros of first NAL unit */
  do {
    byte = stream[index++];
  } while(byte != 1 && index < max_index);

  /* invalid start code prefix */
  if(index == max_index || index < 3) {
    DEBUG_PRINT(("INVALID BYTE STREAM\n"));
    return 0;
  }

  /* nal_unit_stream is without start code prefix */
  if(nal_unit_stream) {
    stream += index;
    max_index -= index;
    index = 0;
  }

  zero_count = 0;

  /* Search stream for next start code prefix */
  /*lint -e(716) while(1) used consciously */
  while(1) {
    byte = stream[index++];
    if(!byte)
      zero_count++;

    if((byte == 0x01) && (zero_count >= 2)) {
      /* Start code prefix has two zeros
       * Third zero is assumed to be leading zero of next packet
       * Fourth and more zeros are assumed to be trailing zeros of this
       * packet */
      if(zero_count > 3) {
        index -= 4;
        zero_count -= 3;
      } else {
        index -= zero_count + 1;
        zero_count = 0;
      }
      break;
    } else if(byte)
      zero_count = 0;

    if(index == max_index) {
      break;
    }

  }

  /* Store pointer to the beginning of the packet */
  *p_strm = stream;
  prev_index = index;

  /* If we skip this packet */
  if(pic_rdy && next_packet &&
      ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt)) {
    /* Get the next packet */
    DEBUG_PRINT(("Packet Loss\n"));
    return NextPacket(p_strm);
  } else {
    /* nal_unit_stream is without trailing zeros */
    if(nal_unit_stream)
      index -= zero_count;
    /*DEBUG_PRINT(("No Packet Loss\n")); */
    /*if (pic_rdy && stream_truncate && ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt))
     * {
     * i32 ret;
     * DEBUG_PRINT(("Original packet size %d\n", index));
     * ret = TBRandomizeU32(&index);
     * if(ret != 0)
     * {
     * DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
     * return 0;
     * }
     * DEBUG_PRINT(("Randomized packet size %d\n", index));
     * } */
    return (index);
  }
}

/*------------------------------------------------------------------------------

    Function name: NextPacketFromFile

    Purpose:
        Get the pointer to start of next NAL-unit in input stream. Uses global
        variables 'finput' to read the stream and 'packetize' to determine the
        decoder input stream mode.

        nal_unit_stream a single NAL-unit without start code prefix. If not
        enabled, a single NAL-unit with start code prefix

        p_strm stores pointer to the start of previous decoder input and is
        replaced with pointer to the start of the next decoder input.

        Returns the packet size in bytes

------------------------------------------------------------------------------*/
u32 NextPacketFromFile(u8 ** p_strm) {

  u32 index = 0;
  u32 zero_count;
  u8 byte;
  u8 next_packet = 0;
  i32 ret = 0;
  u8 first_read = 1;
  fpos_t strm_pos;
  static u8 *stream = NULL;

  /* store the buffer start address for later use */
  if(stream == NULL)
    stream = *p_strm;
  else
    *p_strm = stream;

  /* If enabled, loose the packets (skip this packet first though) */
  if(stream_packet_loss) {
    u32 ret = 0;

    ret =
      TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, &next_packet);
    if(ret != 0) {
      DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
      return 0;
    }
  }

  if(fgetpos(finput, &strm_pos)) {
    DEBUG_PRINT(("FILE POSITION GET ERROR\n"));
    return 0;
  }

  if(use_index == 0) {
    /* test end of stream */
    ret = fread(&byte, 1, 1, finput);
    (void) ret;
    if(ferror(finput)) {
      DEBUG_PRINT(("STREAM READ ERROR\n"));
      return 0;
    }
    if(feof(finput)) {
      DEBUG_PRINT(("END OF STREAM\n"));
      return 0;
    }

    /* leading zeros of first NAL unit */
    do {
      index++;
      /* the byte is already read to test the end of stream */
      if(!first_read) {
        ret = fread(&byte, 1, 1, finput);
        (void) ret;
        if(ferror(finput)) {
          DEBUG_PRINT(("STREAM READ ERROR\n"));
          return 0;
        }
      } else {
        first_read = 0;
      }
    } while(byte != 1 && !feof(finput));

    /* invalid start code prefix */
    if(feof(finput) || index < 3) {
      DEBUG_PRINT(("INVALID BYTE STREAM\n"));
      return 0;
    }

    /* nal_unit_stream is without start code prefix */
    if(nal_unit_stream) {
      if(fgetpos(finput, &strm_pos)) {
        DEBUG_PRINT(("FILE POSITION GET ERROR\n"));
        return 0;
      }
      index = 0;
    }

    zero_count = 0;

    /* Search stream for next start code prefix */
    /*lint -e(716) while(1) used consciously */
    while(1) {
      /*byte = stream[index++]; */
      index++;
      ret = fread(&byte, 1, 1, finput);
      (void) ret;
      if(ferror(finput)) {
        DEBUG_PRINT(("FILE ERROR\n"));
        return 0;
      }
      if(!byte)
        zero_count++;

      if((byte == 0x01) && (zero_count >= 2)) {
        /* Start code prefix has two zeros
         * Third zero is assumed to be leading zero of next packet
         * Fourth and more zeros are assumed to be trailing zeros of this
         * packet */
        if(zero_count > 3) {
          index -= 4;
          zero_count -= 3;
        } else {
          index -= zero_count + 1;
          zero_count = 0;
        }
        break;
      } else if(byte)
        zero_count = 0;

      if(feof(finput)) {
        --index;
        break;
      }
    }

    /* Store pointer to the beginning of the packet */
    if(fsetpos(finput, &strm_pos)) {
      DEBUG_PRINT(("FILE POSITION SET ERROR\n"));
      return 0;
    }

    if(save_index) {
	  fprintf(findex, "%" PRId64"\n", OSAL_STRM_POS);
      if(nal_unit_stream) {
        /* store amount */
        fprintf(findex, "%u\n", index);
      }
    }

    /* Read the rewind stream */
    ret = fread(*p_strm, 1, index, finput);
    (void) ret;
    if(feof(finput)) {
      DEBUG_PRINT(("TRYING TO READ STREAM BEYOND STREAM END\n"));
      return 0;
    }
    if(ferror(finput)) {
      DEBUG_PRINT(("FILE ERROR\n"));
      return 0;
    }
  } else {
    u32 amount = 0;
    u32 f_pos = 0;

    if(nal_unit_stream) {
	    ret = fscanf(findex, "%zu", &cur_index);
		(void) ret;
    }

    /* check position */
    f_pos = ftell(finput);
    if(f_pos != cur_index) {
      fseeko64(finput, cur_index - f_pos, SEEK_CUR);
    }

    if(nal_unit_stream) {
      ret = fscanf(findex, "%u", &amount);
      (void) ret;
    } 
	else {
	  ret = fscanf(findex, "%zu", &next_index);
	  (void) ret;
      amount = next_index - cur_index;
      cur_index = next_index;
    }

    ret = fread(*p_strm, 1, amount, finput);
    (void) ret;
    index = amount;
  }
  /* If we skip this packet */
  if(pic_rdy && next_packet &&
      ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt)) {
    /* Get the next packet */
    DEBUG_PRINT(("Packet Loss\n"));
    return NextPacket(p_strm);
  } else {
    /*DEBUG_PRINT(("No Packet Loss\n")); */
    /*if (pic_rdy && stream_truncate && ((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt))
     * {
     * i32 ret;
     * DEBUG_PRINT(("Original packet size %d\n", index));
     * ret = TBRandomizeU32(&index);
     * if(ret != 0)
     * {
     * DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
     * return 0;
     * }
     * DEBUG_PRINT(("Randomized packet size %d\n", index));
     * } */
    return (index);
  }
}

/*------------------------------------------------------------------------------

    Function name: CropPicture

    Purpose:
        Perform cropping for picture. Input picture p_in_image with dimensions
        frame_width x frame_height is cropped with p_crop_params and the resulting
        picture is stored in p_out_image.

------------------------------------------------------------------------------*/
u32 CropPicture(u8 * p_out_image, u8 * p_in_image,
                u32 frame_width, u32 frame_height, H264CropParams * p_crop_params,
                u32 mono_chrome) {

  u32 i, j;
  u32 out_width, out_height;
  u8 *p_out, *p_in;

  if(p_out_image == NULL || p_in_image == NULL || p_crop_params == NULL ||
      !frame_width || !frame_height) {
    return (1);
  }

  if(((p_crop_params->crop_left_offset + p_crop_params->crop_out_width) >
      frame_width) ||
      ((p_crop_params->crop_top_offset + p_crop_params->crop_out_height) >
       frame_height)) {
    return (1);
  }

  out_width = p_crop_params->crop_out_width;
  out_height = p_crop_params->crop_out_height;

  /* Calculate starting pointer for luma */
  p_in = p_in_image + p_crop_params->crop_top_offset * frame_width +
         p_crop_params->crop_left_offset;
  p_out = p_out_image;

  /* Copy luma pixel values */
  for(i = out_height; i; i--) {
    for(j = out_width; j; j--) {
      *p_out++ = *p_in++;
    }
    p_in += frame_width - out_width;
  }

#if 0   /* planar */
  out_width >>= 1;
  out_height >>= 1;

  /* Calculate starting pointer for cb */
  p_in = p_in_image + frame_width * frame_height +
         p_crop_params->crop_top_offset * frame_width / 4 +
         p_crop_params->crop_left_offset / 2;

  /* Copy cb pixel values */
  for(i = out_height; i; i--) {
    for(j = out_width; j; j--) {
      *p_out++ = *p_in++;
    }
    p_in += frame_width / 2 - out_width;
  }

  /* Calculate starting pointer for cr */
  p_in = p_in_image + 5 * frame_width * frame_height / 4 +
         p_crop_params->crop_top_offset * frame_width / 4 +
         p_crop_params->crop_left_offset / 2;

  /* Copy cr pixel values */
  for(i = out_height; i; i--) {
    for(j = out_width; j; j--) {
      *p_out++ = *p_in++;
    }
    p_in += frame_width / 2 - out_width;
  }
#else /* semiplanar */

  if(mono_chrome)
    return 0;

  out_height >>= 1;

  /* Calculate starting pointer for chroma */
  p_in = p_in_image + frame_width * frame_height +
         (p_crop_params->crop_top_offset * frame_width / 2 +
          p_crop_params->crop_left_offset);

  /* Copy chroma pixel values */
  for(i = out_height; i; i--) {
    for(j = out_width; j; j -= 2) {
      *p_out++ = *p_in++;
      *p_out++ = *p_in++;
    }
    p_in += (frame_width - out_width);
  }

#endif

  return (0);
}

/*------------------------------------------------------------------------------

    Function name:  H264DecTrace

    Purpose:
        Example implementation of H264DecTrace function. Prototype of this
        function is given in H264DecApi.h. This implementation appends
        trace messages to file named 'dec_api.trc'.

------------------------------------------------------------------------------*/
void H264DecTrace(const char *string) {
  FILE *fp;

#if 0
  fp = fopen("dec_api.trc", "at");
#else
  fp = stderr;
#endif

  if(!fp)
    return;

  fprintf(fp, "%s", string);

  if(fp != stderr)
    fclose(fp);
}

/*------------------------------------------------------------------------------

    Function name:  bsdDecodeReturn

    Purpose: Print out decoder return value

------------------------------------------------------------------------------*/
static void printDecodeReturn(i32 retval) {

  static i32 prev_retval = 0xFFFFFF;
  if (prev_retval != retval ||
      (prev_retval != DEC_NO_DECODING_BUFFER &&
       prev_retval != DEC_PENDING_FLUSH))
    DEBUG_PRINT(("TB: H264DecDecode returned: "));
  switch (retval) {

  case DEC_OK:
    DEBUG_PRINT(("DEC_OK\n"));
    break;
  case DEC_NONREF_PIC_SKIPPED:
    DEBUG_PRINT(("DEC_NONREF_PIC_SKIPPED\n"));
    break;
  case DEC_STRM_PROCESSED:
    DEBUG_PRINT(("DEC_STRM_PROCESSED\n"));
    break;
  case DEC_BUF_EMPTY:
    DEBUG_PRINT(("DEC_BUF_EMPTY\n"));
    break;
  case DEC_NO_DECODING_BUFFER:
    /* There may be too much DEC_NO_DECODING_BUFFER.
       Only print for the 1st time. */
    if (prev_retval != DEC_NO_DECODING_BUFFER)
      DEBUG_PRINT(("DEC_NO_DECODING_BUFFER\n"));
    break;
  case DEC_PIC_RDY:
    DEBUG_PRINT(("DEC_PIC_RDY\n"));
    break;
  case DEC_PIC_DECODED:
    DEBUG_PRINT(("DEC_PIC_DECODED\n"));
    break;
  case DEC_ADVANCED_TOOLS:
    DEBUG_PRINT(("DEC_ADVANCED_TOOLS\n"));
    break;
  case DEC_HDRS_RDY:
    DEBUG_PRINT(("DEC_HDRS_RDY\n"));
    break;
  case DEC_STREAM_NOT_SUPPORTED:
    DEBUG_PRINT(("DEC_STREAM_NOT_SUPPORTED\n"));
    break;
  case DEC_DWL_ERROR:
    DEBUG_PRINT(("DEC_DWL_ERROR\n"));
    break;
  case DEC_STRM_ERROR:
    DEBUG_PRINT(("DEC_STRM_ERROR\n"));
    break;
  case DEC_HW_TIMEOUT:
    DEBUG_PRINT(("DEC_HW_TIMEOUT\n"));
    break;
  case DEC_PENDING_FLUSH:
    if (prev_retval != DEC_PENDING_FLUSH)
      DEBUG_PRINT(("DEC_PENDING_FLUSH\n"));
    break;
  default:
    DEBUG_PRINT(("Other %d\n", retval));
    break;
  }
  prev_retval = retval;
}

/*------------------------------------------------------------------------------

    Function name:            printH264PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printH264PicCodingType(u32 *pic_type) {
  DEBUG_PRINT(("Coding type "));
  switch (pic_type[0]) {
  case DEC_PIC_TYPE_I:
    printf("[I:");
    break;
  case DEC_PIC_TYPE_P:
    printf("[P:");
    break;
  case DEC_PIC_TYPE_B:
    printf("[B:");
    break;
  default:
    printf("[Other %d:", pic_type[0]);
    break;
  }

  switch (pic_type[1]) {
  case DEC_PIC_TYPE_I:
    printf("I]");
    break;
  case DEC_PIC_TYPE_P:
    printf("P]");
    break;
  case DEC_PIC_TYPE_B:
    printf("B]");
    break;
  default:
    printf("Other %d]", pic_type[1]);
    break;
  }
}

u32 fillBuffer(const u8 *stream) {
  u32 amount = 0;
  u32 data_len = 0;
  int ret;
  if(cur_index != ftell(finput)) {
    fseeko64(finput, cur_index, SEEK_SET);
  }

  /* read next index */
  ret = fscanf(findex, "%zu", &next_index);
  (void) ret;
  amount = next_index - cur_index;
  cur_index = next_index;

  /* read data */
  data_len = fread((u8 *)stream, 1, amount, finput);

  return data_len;
}

task_handle run_task(task_func func, void* param) {
  int ret;
  pthread_attr_t attr;
  struct sched_param par;
  pthread_t* thread_handle = malloc(sizeof(pthread_t));
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  assert(ret == 0);
  ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
  par.sched_priority = 60;
  ret = pthread_attr_setschedparam(&attr, &par);
  ret = pthread_create(thread_handle, &attr, func, param);
  assert(ret == 0);

  if(ret != 0) {
    free(thread_handle);
    thread_handle = NULL;
  }

  return thread_handle;
}

void wait_for_task_completion(task_handle task) {
  int ret;
  ret = pthread_join(*((pthread_t*)task), NULL);
  assert(ret == 0);
  (void) ret;
  free(task);
}

void H264SendBytesToDDR(u8* p, u32 n, FILE* fp) {
  i32 ret;
  ret = fread(p, 1, n, fp);
  (void) ret;
}

void* send_bytestrm_task(void* param) {
  u32 strm_len;
  FILE* fp  = fopen(in_file_name, "rb");

  u32 packet_size = LOW_LATENCY_PACKET_SIZE;
  u32 send_len = 0;
  u32 bytes = 0;

  char* tmp_space = (char *)malloc(send_strm_len);
  strm_len = send_strm_len;

  while (!process_end_flag) {
#ifndef LOW_LATENCY_FRAME_MODE
    if (send_strm_len - send_len > packet_size) {
      bytes = packet_size;
      H264SendBytesToDDR(send_strm_info.strm_vir_addr, packet_size, fp);
      pthread_mutex_lock(&send_mutex);
      send_strm_info.strm_bus_addr += packet_size;
      send_len += packet_size;
      send_strm_info.last_flag = strm_last_flag = 0;
      send_strm_info.strm_vir_addr += packet_size;
      /* update the global structure in control sw layer */
      H264UpdateVar(send_strm_info);
      pthread_mutex_unlock(&send_mutex);
    } else {
      bytes = send_strm_len - send_len;
      H264SendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
      pthread_mutex_lock(&send_mutex);
      send_strm_info.strm_bus_addr += bytes;
      send_len += bytes;
      send_strm_info.last_flag = strm_last_flag = 1;
      send_strm_info.strm_vir_addr += bytes;
      /* update the global structure in control sw layer */
      H264UpdateVar(send_strm_info);
      pthread_mutex_unlock(&send_mutex);
    }
    usleep(1000); /* delay 1 ms */
    /* update length register if hw is ready */
    H264UpdateStrmInfoCtrl(dec_inst, send_strm_info.last_flag, send_strm_info.strm_bus_addr);

    if (pic_decoded == 1) {
      pic_decoded = 0;
      send_strm_info.last_flag = strm_last_flag = 0;
      sem_post(&currFrameEndSem);
      sem_wait(&nextFrameStartSem);
      send_len = 0;
      /* set the pointor back to the start position of next frame */
      fseek(fp, -bytesGoBack, SEEK_CUR);
    }

#else

    frame_len = libav_read_frame(tmp_space, strm_len);
    while (frame_len > send_len || pic_decoded == 0) {
      if (frame_len - send_len > packet_size) {
        bytes = packet_size;
        H264SendBytesToDDR(send_strm_info.strm_vir_addr, packet_size, fp);
        pthread_mutex_lock(&send_mutex);
        send_strm_info.strm_bus_addr += packet_size;
        send_len += packet_size;
        send_strm_info.last_flag = 0;
        send_strm_info.strm_vir_addr += packet_size;
        /* update the global structure in control sw layer */
        H264DecUpdateStrm(send_strm_info);
        pthread_mutex_unlock(&send_mutex);
      } else if(frame_len - send_len > 0) {
        bytes = frame_len - send_len;
        H264SendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
        pthread_mutex_lock(&send_mutex);
        send_strm_info.strm_bus_addr += bytes;
        send_len += bytes;
        send_strm_info.last_flag = 1;
        send_strm_info.strm_vir_addr += bytes;
        /* update the global structure in control sw layer */
        H264DecUpdateStrm(send_strm_info);
        pthread_mutex_unlock(&send_mutex);
      }
      usleep(1000); /* delay 1 ms */
      /* update length register if hw is ready */
      H264DecUpdateStrmInfoCtrl(dec_inst, send_strm_info.last_flag, send_strm_info.strm_bus_addr);
    }

    if(pic_decoded == 1) {
      /* reset flag & update variable */
      pic_decoded = 0;
      send_strm_info.last_flag = 0;
      send_strm_len -= frame_len;
      if(send_strm_len == 0)
        strm_last_flag = 1;
      /* do sync with main thread after one frame decoded */
      sem_post(&curr_frame_end_sem);
      sem_wait(&next_frame_start_sem);
      send_len = 0;
    }
#endif
  }
  free(tmp_space);
  fclose(fp);
  return NULL;
}
