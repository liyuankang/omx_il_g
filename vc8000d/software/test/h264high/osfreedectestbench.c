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
#include "dwl.h"
#include "dwlthread.h"

#include <stdio.h>
#include <inttypes.h>
/*#include <stdlib.h>*/
//#define _USE_VSI_STRING_
#ifdef _USE_VSI_STRING_
#include "vsi_string.h"
#else
#include <string.h>
#endif
#include <time.h>
/*#include <signal.h>*/

/* macro for assertion, used only when _ASSERT_USED is defined */
#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"

#include "h264hwd_container.h"
#include "tb_cfg.h"
#include "tb_tiled.h"
#include "regdrv.h"

#include "tb_md5.h"
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

#define NUM_STREAMS  (2)

#define NUM_RETRY   100 /* how many retries after HW timeout */
#define MAX_BUFFERS 34
#define ALIGNMENT_MASK 7
#define BUFFER_ALIGN_MASK 0xF
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

u32 TBWriteOutput(FILE* fout,u8* p_lu, u8 *p_ch,
                  u32 coded_width, u32 coded_height, u32 pic_stride,
                  u32 coded_width_ch, u32 coded_h_ch, u32 pic_stride_ch,
                  u32 md5Sum, u32 planar, u32 mono_chrome, u32 frame_number,
                  u32 pixel_bytes);

void WriteOutput(char *filename, u8 *data, u32 pic_width, u32 pic_height,
                 u32 frame_number, u32 mono_chrome, u32 view, u32 mvc_separate_views,
                 u32 disable_output_writing, u32 tiled_mode, u32 pic_stride,
                 u32 pic_stride_ch, u32 index, u32 planar, u32 cr_first,
                 u32 convert_tiled_output, u32 convert_to_frame_dpb,
                 u32 dpb_mode, u32 md5sum, FILE **fout, u32 pixel_bytes, u32 strm_id);
u32 NextPacket(u8 ** p_strm, u32 strm_id);
u32 NextPacketFromFile(u8 ** p_strm, u32 strm_id);
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

u32 fillBuffer(u8 *stream, u32 strm_id);
/* Global variables for stream handling */
u8 *stream_stop[NUM_STREAMS] = {NULL};
u32 packetize = 0;
u32 nal_unit_stream = 0;
FILE *foutput[NUM_STREAMS][DEC_MAX_OUT_COUNT] = {NULL};
FILE *foutput2[NUM_STREAMS][DEC_MAX_OUT_COUNT] = {NULL};
FILE *f_tiled_output[NUM_STREAMS][DEC_MAX_OUT_COUNT] = {NULL};
FILE *fchroma2[NUM_STREAMS] = {NULL};

FILE *findex[NUM_STREAMS] = {NULL};

/* PP only mode. Decoder is enabled only to generate PP input trace file.*/
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;

/* stream start address */
u8 *byte_strm_start[NUM_STREAMS];
u32 trace_used_stream = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 128 bytes alignment */

/* output file writing disable */
u32 disable_output_writing = 0;
u32 retry = 0;
u32 pending_flush[NUM_STREAMS] = {0, };

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
u32 hdrs_rdy[NUM_STREAMS] = {0, };
u32 pic_rdy[NUM_STREAMS] = {0, };
struct TBCfg tb_cfg;

u32 long_stream = 0;
FILE *finput[NUM_STREAMS];
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
addr_t cur_index[NUM_STREAMS] = {0, };
addr_t next_index[NUM_STREAMS] = {0, };
/* indicate when we save index */
u8 save_flag = 0;

u32 process_end_flag[NUM_STREAMS];
u32 low_latency;
u32 low_latency_sim;
u32 secure;

u8 *grey_chroma[NUM_STREAMS] = {NULL, };

u8 *pic_big_endian[NUM_STREAMS] = {NULL, };
size_t pic_big_endian_size[NUM_STREAMS] = {0, };

u32 pp_enabled = 0;
/* user input arguments */
enum SCALE_MODE scale_mode;

#ifdef H264_EVALUATION
extern u32 g_hw_ver;
extern u32 h264_high_support;
#endif

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

H264DecInst dec_inst[NUM_STREAMS];
const void *dwl_inst[NUM_STREAMS] = {NULL, };
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size[NUM_STREAMS];
u32 num_buffers[NUM_STREAMS];  /* external buffers allocated yet. */
u32 add_buffer_thread_run[NUM_STREAMS] = {0, };
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[NUM_STREAMS][MAX_BUFFERS];

u32 add_extra_flag[NUM_STREAMS] = {0, };

static void AddBuffer(u32 strm_id) {
#ifdef _HAVE_PTHREAD_H
  pthread_mutex_lock(&ext_buffer_contro);
#else
  if(pthread_mutex_lock(&ext_buffer_contro)) return;
#endif
  if(add_extra_flag[strm_id] && (num_buffers[strm_id] < 34)) {
    struct DWLLinearMem mem;
    i32 dwl_ret;
    mem.mem_type = DWL_MEM_TYPE_DPB;
    if (pp_enabled)
      dwl_ret = DWLMallocLinear(dwl_inst[strm_id], buffer_size[strm_id], &mem);
    else
      dwl_ret = DWLMallocRefFrm(dwl_inst[strm_id], buffer_size[strm_id], &mem);
    if(dwl_ret == DWL_OK) {
      enum DecRet rv = H264DecAddBuffer(dec_inst[strm_id], &mem) ;
      if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
        if (pp_enabled)
          DWLFreeLinear(dwl_inst[strm_id], &mem);
        else
          DWLFreeRefFrm(dwl_inst[strm_id], &mem);
      } else {
        ext_buffers[strm_id][num_buffers[strm_id]++] = mem;
      }
    }
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

static void *AddBufferThread(void *arg, u32 strm_id) {
  usleep(100000);
#ifdef _HAVE_PTHREAD_H
  while(add_buffer_thread_run)
#else
  if(add_buffer_thread_run != NULL) /* the address of ‘add_buffer_thread_run’ will always evaluate as ‘true’ */
#endif
  {
    AddBuffer(strm_id);
    sched_yield();
  }
  return NULL;
}

void ReleaseExtBuffers(u32 strm_id) {
  int i;
#ifdef _HAVE_PTHREAD_H
  pthread_mutex_lock(&ext_buffer_contro);
#else
  if(pthread_mutex_lock(&ext_buffer_contro)) return;
#endif
  for(i=0; i<num_buffers[strm_id]; i++) {
    DEBUG_PRINT(("Freeing buffer %p\n", (void *)ext_buffers[strm_id][i].virtual_address));
    if (pp_enabled)
      DWLFreeLinear(dwl_inst[strm_id], &ext_buffers[strm_id][i]);
    else
      DWLFreeRefFrm(dwl_inst[strm_id], &ext_buffers[strm_id][i]);

    DWLmemset(&ext_buffers[strm_id][i], 0, sizeof(ext_buffers[strm_id][i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

H264DecInfo dec_info[NUM_STREAMS];
char out_file_name[NUM_STREAMS][DEC_MAX_OUT_COUNT][256] =
                           {{"", "", "", ""},{"", "", "", ""}};
u32 pic_size[NUM_STREAMS] = {0, };
u32 crop_display = 0;
u8 *tmp_image[NUM_STREAMS] = {NULL, };
u32 num_errors[NUM_STREAMS] = {0, };
pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;

sem_t buf_release_sem;
H264DecPicture buf_list[NUM_STREAMS][100] = {0};
u32 buf_status[NUM_STREAMS][100] = {{0, }, };
u32 list_pop_index[NUM_STREAMS] = {0, };
u32 list_push_index[NUM_STREAMS] = {0, };
u32 last_pic_flag[NUM_STREAMS] = {0, };

/* buf release thread entry point. */
static void* buf_release_thread(void* arg, u32 strm_id) {
  u32 bContinue = pending_flush[strm_id];

  do {
    /* Pop output buffer from buf_list and consume it */
    if(buf_status[strm_id][list_pop_index[strm_id]]) {
      sem_wait(&buf_release_sem);
      H264DecPictureConsumed(dec_inst[strm_id], &buf_list[strm_id][list_pop_index[strm_id]]);
      buf_status[strm_id][list_pop_index[strm_id]] = 0;
      list_pop_index[strm_id]++;
      if(list_pop_index[strm_id] == 100)
        list_pop_index[strm_id] = 0;
      if(allocate_extra_buffers_in_output) {
        AddBuffer(strm_id);
      }
    } else {
      bContinue = 0;
    }
    /* The last buffer has been consumed */
#ifdef _HAVE_PTHREAD_H
    if(last_pic_flag[strm_id] &&  buf_status[strm_id][list_pop_index[strm_id]] == 0)
      break;
#endif //_HAVE_PTHREAD_H
    usleep(10000);
  }
#ifdef _HAVE_PTHREAD_H
  while(1);
#else
  while(bContinue);
#endif
  return (void *)NULL;
}

/* Output thread entry point. */
static void* h264_output_thread(void* arg, u32 strm_id) {
  H264DecPicture dec_picture;
  static u32 pic_display_number = 1;
  u32 i = 0, pp_planar = 0, pp_cr_first = 0, pp_mono_chrome = 0;
  u32 bContinue = pending_flush[strm_id];
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

#ifdef _HAVE_PTHREAD_H
  while(output_thread_run)
#else //_HAVE_PTHREAD_H
  if(output_thread_run)
    do
#endif //_HAVE_PTHREAD_H
  {
    u8 *image_data;
    enum DecRet ret;

    ret = H264DecNextPicture(dec_inst[strm_id], &dec_picture, 0);

#ifndef _HAVE_PTHREAD_H
    bContinue = (ret == DEC_PIC_RDY);
#endif

    if(ret == DEC_PIC_RDY) {
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
                                    dec_info[strm_id].mono_chrome,
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

          num_errors[strm_id] += dec_picture.nbr_of_err_mbs;

          /* Write output picture to file */
          image_data = (u8 *) dec_picture.pictures[i].output_picture;

          if(crop_display) {
            int tmp;

            pic_size[strm_id] = dec_info[strm_id].crop_params.crop_out_width *
                              dec_info[strm_id].crop_params.crop_out_height;
            if(!dec_info[strm_id].mono_chrome)
              pic_size[strm_id] = (3 * pic_size[strm_id]) / 2;
            tmp_image[strm_id] = malloc(pic_size[strm_id]);

            tmp = CropPicture(tmp_image[strm_id], image_data,
                              dec_picture.pictures[i].pic_width,
                              dec_picture.pictures[i].pic_height,
                              &dec_picture.crop_params,
                              dec_info[strm_id].mono_chrome);
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
          WriteOutput(out_file_name[strm_id][i],
                      crop_display ? tmp_image[strm_id] : image_data,
                      crop_display ? dec_picture.crop_params.crop_out_width :
                        dec_picture.pictures[i].pic_width,
                      crop_display ? dec_picture.crop_params.crop_out_height :
                        dec_picture.pictures[i].pic_height,
                      pic_display_number - 1,
                      pp_mono_chrome,
                      dec_picture.view_id, mvc_separate_views,
                      disable_output_writing,
                      dec_picture.pictures[i].output_format == DEC_OUT_FRM_TILED_4X4,
                      dec_picture.pictures[i].pic_stride,
                      dec_picture.pictures[i].pic_stride_ch, i,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      convert_to_frame_dpb, dpb_mode, md5sum,
                      ((dec_picture.view_id && mvc_separate_views) ?
                        &(foutput2[strm_id][i]) : &(foutput[strm_id][i])), 1,
                      strm_id);
        }
      }

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[strm_id][list_push_index[strm_id]] = dec_picture;
      buf_status[strm_id][list_push_index[strm_id]] = 1;

      list_push_index[strm_id]++;
      if(list_push_index[strm_id] == 100)
        list_push_index[strm_id] = 0;

      if(tmp_image[strm_id]) {
        free(tmp_image[strm_id]);
        tmp_image[strm_id] = NULL;
      }
      sem_post(&buf_release_sem);

      pic_display_number++;
    } else if(ret == DEC_END_OF_STREAM) {
      last_pic_flag[strm_id] = 1;
      DEBUG_PRINT(("END-OF-STREAM received in output thread\n"));
      add_buffer_thread_run[strm_id] = 0;
#ifdef _HAVE_PTHREAD_H
      break;
#endif
    }
  }
#ifndef _HAVE_PTHREAD_H
  while(bContinue);
#endif
  return (void *)NULL;
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

void PrintUsageH264(char *s)
{
    DEBUG_PRINT(("Usage: %s [options] file.h264\n", s));
    DEBUG_PRINT(("\t--help(-h) Print command line parameters help info\n"));
    DEBUG_PRINT(("\t-Nn forces decoding to stop after n pictures\n"));
    DEBUG_PRINT(("\t-H2 decode two streams\n"));
    DEBUG_PRINT(("\t-O<file> write output to <file> (default out_wxxxhyyy.yuv)\n"));
    DEBUG_PRINT(("\t-m Output frame based md5 checksum. No YUV output!(--md5-per-pic)\n"));
    DEBUG_PRINT(("\t-X Disable output file writing\n"));
    DEBUG_PRINT(("\t-R disable DPB output reordering\n"));
    DEBUG_PRINT(("\t-W disable packetizing even if stream does not fit to buffer.\n"
                 "\t   Only the first 0x1FFFFF bytes of the stream are decoded.\n"));
    DEBUG_PRINT(("\t-E use tiled reference frame format.\n"));
    DEBUG_PRINT(("\t-G convert tiled output pictures to raster scan\n"));
    DEBUG_PRINT(("\t-L enable support for long streams\n"));
    DEBUG_PRINT(("\t-P write planar output(--planar)\n"));
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
    DEBUG_PRINT(("\t--E1010 Enable output E1010 format.\n"));
    DEBUG_PRINT(("\t--pp-only Enable only to generate PP input trace file for"\
                 "standalone PP verification.\n"));
    DEBUG_PRINT(("\t-U Enable PP tiled output.\n"));
    DEBUG_PRINT(("\t--cache_enable Enable cache rtl simulation(external hw IP).\n"));
    DEBUG_PRINT(("\t--shaper_enable Enable shaper rtl simulation(external hw IP).\n"));
    DEBUG_PRINT(("\t--shaper_bypass Enable shaper bypass rtl simulation(external hw IP).\n"));
}


/*------------------------------------------------------------------------------

    Function name:  main

    Purpose:
        main function of decoder testbench. Provides command line interface
        with file I/O for H.264 decoder. Prints out the usage information
        when executed without arguments.

------------------------------------------------------------------------------*/

int main(int argc, char **argv) {

  u32 i = 0, tmp = 0, strm_id = 0;
  u32 pp_planar = 0, pp_cr_first = 0, pp_mono_chrome = 0;
  u32 max_num_pics = 0;
  u32 num_streams = 1;
  u8 *image_data = NULL;
  long int strm_len[NUM_STREAMS] = {0, 0}, max_strm_len = 0;
  enum DecRet ret;
  H264DecInput dec_input[NUM_STREAMS];
  H264DecOutput dec_output[NUM_STREAMS];
  H264DecPicture dec_picture[NUM_STREAMS];
  struct H264DecConfig dec_cfg = {0};
  struct DWLLinearMem stream_mem[NUM_STREAMS];
  //i32 dwlret = 0;
  u32 prev_width = 0, prev_height = 0;
  u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  H264DecBufferInfo hbuf;
  enum DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_H264_DEC;

  u32 pic_decode_number[NUM_STREAMS] = {0, };
  u32 pic_display_number[NUM_STREAMS] = {0, };
  u32 disable_output_reordering = 0;
  u32 use_display_smoothing = 0;
  u32 rlc_mode = 0;
  u32 mb_error_concealment = 0;
  u32 force_whole_stream = 0;
  const u8 *ptmpstream = NULL;
  u32 stream_will_end[NUM_STREAMS] = {0, };
  u32 eos = 0;
  u32 error_conceal = 0;
  u32 major_version, minor_version, prod_id;
  u32 bContinue = 1;
  u32 nParas = 0;

  FILE *f_tbcfg;
  u32 seed_rnd = 0;
  u32 stream_bit_swap = 0;
  i32 corrupted_bytes[NUM_STREAMS] = {0, };

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
  u32 ret_unused = 0;
  
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


#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */
#if 0
  /* expiry stuff */
  {
    u8 tm_buf[7];
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

  for(strm_id=0; strm_id<NUM_STREAMS; strm_id++) {
    stream_mem[strm_id].virtual_address = NULL;
    stream_mem[strm_id].bus_address = 0;
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

    /* Print API version number */
    dec_api = H264DecGetAPIVersion();
    dec_build = H264DecGetBuild();
    DEBUG_PRINT(("\nX170 H.264 Decoder API v%d.%d - SW build: %d.%d - HW build: %x\n\n",
                 dec_api.major, dec_api.minor, dec_build.sw_build >>16,
                 dec_build.sw_build & 0xFFFF, dec_build.hw_build));
    prod_id = dec_build.hw_build >> 16;
    major_version = (dec_build.hw_build & 0xF000) >> 12;
    minor_version = (dec_build.hw_build & 0x0FF0) >> 4;

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

  /* read command line arguments */
  nParas = (u32) (argc - 1);
  for(i = 1; i < nParas; i++) {
    if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_pics = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-H", 2) == 0) {
      num_streams = (u32) atoi(argv[i] + 2);
      num_streams = (num_streams > NUM_STREAMS) ? NUM_STREAMS: num_streams;
      nParas = (u32) (argc - num_streams);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      strcpy(out_file_name[0][0], argv[i] + 2);
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
    } else if(strcmp(argv[i], "-P") == 0 ||
              strcmp(argv[i], "--planar") == 0) {
      planar_output = 1;
    } else if(strcmp(argv[i], "-a") == 0 ||
              strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = pp_planar_out;
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
    }
#endif
    else if (strncmp(argv[i], "-d", 2) == 0) {
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
        DEBUG_PRINT(("Illegal parameter: %s\n", argv[i]));
        DEBUG_PRINT(("ERROR: Enable down scaler parameter by using: -d[1248][:[1248]]\n"));
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
        DEBUG_PRINT(("Illegal parameter: %s\n", argv[i]));
        DEBUG_PRINT(("ERROR: Enable scaler parameter by using: -D[w]x[h]\n"));
        return 1;
      }
      *px = '\0';
      scale_enabled = 1;
      pp_enabled = 1;
      scaled_w = atoi(argv[i]+2);
      scaled_h = atoi(px+1);
      if (scaled_w == 0 || scaled_h == 0) {
        DEBUG_PRINT(("Illegal scaled width/height: %s/%s\n", argv[i]+2, px+1));
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
        DEBUG_PRINT(("Illegal parameter: %s\n", argv[i]));
        DEBUG_PRINT(("ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,\n"));
        DEBUG_PRINT(("\t -CxXXX -CyYYY     Crop from (XXX, YYY)\n"));
        DEBUG_PRINT(("\t -CwWWW -ChHHH     Crop size  WWWxHHH\n"));
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
    } else if (strcmp(argv[i], "--cr-first") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].cr_first = 1;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "--ec") == 0) {
      error_conceal = 1;
    } else if ((strncmp(argv[i], "-h", 2) == 0) ||
              (strcmp(argv[i], "--help") == 0)){
      PrintUsageH264(argv[0]);
    } else if (strcmp(argv[i], "--pp-only") == 0) {
      pp_enabled = 1;
      params.pp_standalone = 1;
    } else if (strcmp(argv[i], "-U") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].tiled_e = 1;
      pp_enabled = 1;
    } else {
      DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
      return 1;
    }
  }

  /* open input file for reading, file name given by user. If file open
   * fails -> exit */
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    finput[strm_id] = fopen(argv[argc - num_streams + strm_id], "rb");
    if(finput[strm_id] == NULL) {
      DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE\n"));
      return -1;
    }
  }

  /* open index file for saving */
  if(save_index) {
    for(strm_id=0; strm_id<num_streams; strm_id++) {
      char s[32];
      sprintf(s, "stream%d.cfg", strm_id);
      findex[strm_id] = fopen(s, "w");
      if(findex[strm_id] == NULL) {
        DEBUG_PRINT(("UNABLE TO OPEN INDEX FILE\n"));
        return -1;
      }
    }
  }
  /* try open index file */
  else {
    use_index = 1;
    for(strm_id=0; strm_id<num_streams; strm_id++) {
      char s[32];
      sprintf(s, "stream%d.cfg", strm_id);
      findex[strm_id] = fopen(s, "r");
      use_index &= (findex[strm_id] != NULL);
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

  seed_rnd = tb_cfg.tb_params.seed_rnd;
  stream_header_corrupt = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  /* if headers are to be corrupted
   * -> do not wait the picture to finalize before starting stream corruption */
  if(stream_header_corrupt) {
    for(strm_id=0; strm_id<num_streams; strm_id++)
      pic_rdy[strm_id] = 1;
  }
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

  for(strm_id=0; strm_id<num_streams; strm_id++) {
    dwl_inst[strm_id] = DWLInit(&dwl_init);
    if(dwl_inst[strm_id] == NULL) {
      DEBUG_PRINT(("H264DecInit# ERROR: DWL Init failed\n"));
      goto end;
    }
  }
  /* initialize decoder. If unsuccessful -> exit */
  START_SW_PERFORMANCE;
  if( tiled_output )   dec_cfg.dpb_flags |= DEC_REF_FRM_TILED_DEFAULT;
  if( dpb_mode == DEC_DPB_INTERLACED_FIELD )
    dec_cfg.dpb_flags |= DEC_DPB_ALLOW_FIELD_ORDERING;
  dec_cfg.decoder_mode = DEC_NORMAL;
  dec_cfg.no_output_reordering = disable_output_reordering;
  /* just consistent with h264dec*/
  //dec_cfg.error_handling = TBGetDecErrorConcealment( &tb_cfg );
  dec_cfg.error_handling = DEC_EC_FAST_FREEZE;
  dec_cfg.use_display_smoothing = use_display_smoothing;
  dec_cfg.use_video_compressor = 0;
  dec_cfg.use_adaptive_buffers = 1;
  dec_cfg.guard_size = 0;
  dec_cfg.mcinit_cfg.mc_enable = 0;
  dec_cfg.mcinit_cfg.stream_consumed_callback = NULL;
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    ret = H264DecInit(&dec_inst[strm_id], dwl_inst[strm_id], &dec_cfg);
    if(ret != DEC_OK) {
      DEBUG_PRINT(("DECODER INITIALIZATION FAILED\n"));
      goto end;
    }

    /* configure decoder to decode both views of MVC stereo high streams */
    if (enable_mvc)
      H264DecSetMvc(dec_inst[strm_id]);

    /* Set ref buffer test mode */
    //((struct H264DecContainer *) dec_inst[strm_id])->ref_buffer_ctrl.test_function = TBRefbuTestMode;

    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_DEC_LATENCY,
                   latency_comp);
    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_DEC_CLK_GATE_E,
                   clock_gating);
    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_DEC_OUT_ENDIAN,
                   output_picture_endian);
    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_DEC_MAX_BURST,
                   bus_burst_length);
    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_DEC_DATA_DISC_E,
                   data_discard);
    SetDecRegister(((struct H264DecContainer *) dec_inst[strm_id])->h264_regs, HWIF_SERV_MERGE_DIS,
                   service_merge_disable);

    if(rlc_mode) {
      /*Force the decoder into RLC mode */
      ((struct H264DecContainer *) dec_inst[strm_id])->force_rlc_mode = 1;
      ((struct H264DecContainer *) dec_inst[strm_id])->rlc_mode = 1;
      ((struct H264DecContainer *) dec_inst[strm_id])->try_vlc = 0;
    }

#ifdef _ENABLE_2ND_CHROMA
    if (!TBGetDecCh8PixIleavOutput(&tb_cfg)) {
      ((struct H264DecContainer *) dec_inst[strm_id])->storage.enable2nd_chroma = 0;
    } else {
      /* cropping not supported when additional chroma output format used */
      crop_display = 0;
    }
#endif
  }

  END_SW_PERFORMANCE;

  TBInitializeRandom(seed_rnd);

  /* check size of the input file -> length of the stream in bytes */
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    fseek(finput[strm_id], 0L, SEEK_END);
    strm_len[strm_id] = ftell(finput[strm_id]);
    rewind(finput[strm_id]);
    dec_input[strm_id].skip_non_reference = skip_non_reference;


    if(!long_stream) {
      /* If the decoder can not handle the whole stream at once, force packet-by-packet mode */
      if(!force_whole_stream) {
        if(strm_len[strm_id] > max_strm_len) {
          packetize = 1;
        }
      } else {
        if(strm_len[strm_id] > max_strm_len) {
          packetize = 0;
          strm_len[strm_id] = max_strm_len;
        }
      }

      /* sets the stream length to random value */
      if(stream_truncate && !packetize && !nal_unit_stream) {
        DEBUG_PRINT(("strm_len %ld\n", strm_len[strm_id]));
        ret = TBRandomizeU32((u32 *)&strm_len[strm_id]);
        if(ret != 0) {
          DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
          goto end;
        }
        DEBUG_PRINT(("Randomized strm_len %ld\n", strm_len[strm_id]));
      }

      /* NOTE: The DWL should not be used outside decoder SW.
       * here we call it because it is the easiest way to get
       * dynamically allocated linear memory
       * */

      /* allocate memory for stream buffer. if unsuccessful -> exit */
#ifndef ADS_PERFORMANCE_SIMULATION
      stream_mem[strm_id].mem_type = DWL_MEM_TYPE_SLICE;
      if(DWLMallocLinear(dwl_inst[strm_id],
                         strm_len[strm_id], &stream_mem[strm_id]) != DWL_OK) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }
#else
      stream_mem[strm_id].virtual_address = malloc(strm_len[strm_id]);
      stream_mem[strm_id].bus_address = (size_t) stream_mem[strm_id].virtual_address;
#endif

      byte_strm_start[strm_id] = (u8 *) stream_mem[strm_id].virtual_address;

      if(byte_strm_start[strm_id] == NULL) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }

      /* read input stream from file to buffer and close input file */
      ret_unused = fread(byte_strm_start[strm_id], 1, strm_len[strm_id], finput[strm_id]);
      /* initialize H264DecDecode() input structure */
      stream_stop[strm_id] = byte_strm_start[strm_id] + strm_len[strm_id];
      dec_input[strm_id].stream = byte_strm_start[strm_id];
      dec_input[strm_id].stream_bus_address = (addr_t) stream_mem[strm_id].bus_address;
      dec_input[strm_id].data_len = strm_len[strm_id];
    } else {
      u32 size = max_strm_len;
#ifndef ADS_PERFORMANCE_SIMULATION
      stream_mem[strm_id].mem_type = DWL_MEM_TYPE_SLICE;
      if(DWLMallocLinear(dwl_inst[strm_id],
                         size, &stream_mem[strm_id]) != DWL_OK) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }
#else
      stream_mem[strm_id].virtual_address = malloc(max_strm_len);
      stream_mem[strm_id].bus_address = (size_t) stream_mem[strm_id].virtual_address;

      if(stream_mem[strm_id].virtual_address == NULL) {
        DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
        goto end;
      }
#endif

      /* initialize H264DecDecode() input structure */
      dec_input[strm_id].stream = (u8 *) stream_mem[strm_id].virtual_address;
      dec_input[strm_id].stream_bus_address = (addr_t) stream_mem[strm_id].bus_address;
    }

    if(long_stream && !packetize && !nal_unit_stream) {
      if(use_index == 1) {
        u32 amount = 0;
        cur_index[strm_id] = 0;

        /* read index */
        /* off64_t defined differ between LP32 and LP64*/
		ret_unused = fscanf(findex[strm_id], "%zu", &next_index[strm_id]);
		
        {
          /* check if last index -> stream end */
          u8 byte[2];
          ret_unused = fread(&byte, 2, 1, findex[strm_id]);
		  if(feof(findex[strm_id])) {
            DEBUG_PRINT(("STREAM WILL END\n"));
            stream_will_end[strm_id] = 1;
          } else {
            fseek(findex[strm_id], -2, SEEK_CUR);
          }
        }

        amount = next_index[strm_id] - cur_index[strm_id];
        cur_index[strm_id] = next_index[strm_id];

        /* read first block */
        dec_input[strm_id].data_len = fread((u8 *)dec_input[strm_id].stream, 1, amount, finput[strm_id]);
      } else {
        dec_input[strm_id].data_len = fread((u8 *)dec_input[strm_id].stream, 1, max_strm_len, finput[strm_id]);
      }
      /*DEBUG_PRINT(("BUFFER READ\n")); */
      if(feof(finput[strm_id])) {
        DEBUG_PRINT(("STREAM WILL END\n"));
        stream_will_end[strm_id] = 1;
      }
    }

    else {
      if(use_index) {
        if(!nal_unit_stream)
	 	  ret_unused = fscanf(findex[strm_id], "%zu", &cur_index[strm_id]);
      }

      /* get pointer to next packet and the size of packet
       * (for packetize or nal_unit_stream modes) */
      ptmpstream = dec_input[strm_id].stream;
      if((tmp = NextPacket((u8 **) (&dec_input[strm_id].stream), strm_id)) != 0) {
        dec_input[strm_id].data_len = tmp;
        dec_input[strm_id].stream_bus_address += (addr_t) (dec_input[strm_id].stream - ptmpstream);
      }
    }

    pic_decode_number[strm_id] = pic_display_number[strm_id] = 1;
  }

  if(!output_thread_run) {
    output_thread_run = 1;
    sem_init(&buf_release_sem, 0, 0);
    pthread_create(&output_thread, NULL, h264_output_thread, NULL);
    pthread_create(&release_thread, NULL, buf_release_thread, NULL);
  }

  /* If -O option not used, generate default file name */
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    for (i=0; i<DEC_MAX_OUT_COUNT; i++) {
      if(out_file_name[strm_id][0] == 0)
        sprintf(out_file_name[strm_id][i], "out_%d_%d.yuv", strm_id, i);
    }
  }

  /* main decoding loop */
  do {
    for(strm_id=0; strm_id<num_streams; strm_id++) {
      if(dec_input[strm_id].data_len == 0) continue;

      save_flag = 1;
      /*DEBUG_PRINT(("dec_input.data_len %d\n", dec_input.data_len));
       * DEBUG_PRINT(("dec_input.stream %d\n", dec_input.stream)); */

      if(stream_truncate && pic_rdy[strm_id] && (hdrs_rdy[strm_id] || stream_header_corrupt) &&
          (long_stream || (!long_stream && (packetize || nal_unit_stream)))) {
        i32 ret = TBRandomizeU32(&dec_input[strm_id].data_len);
        if(ret != 0) {
          DEBUG_PRINT(("RANDOM STREAM[%d] ERROR FAILED\n", strm_id));
          return 0;
        }
        DEBUG_PRINT(("Randomized stream[%d] size %d\n", strm_id, dec_input[strm_id].data_len));
      }

      /* If enabled, break the stream */
      if(stream_bit_swap) {
        if((hdrs_rdy[strm_id] && !stream_header_corrupt) || stream_header_corrupt) {
          /* Picture needs to be ready before corrupting next picture */
          if(pic_rdy[strm_id] && corrupted_bytes[strm_id] <= 0) {
            i32 ret = TBRandomizeBitSwapInStream((u8 *)dec_input[strm_id].stream,
                                         dec_input[strm_id].data_len,
                                         tb_cfg.tb_params.
                                         stream_bit_swap);
            if(ret != 0) {
              DEBUG_PRINT(("RANDOM STREAM[%d] ERROR FAILED\n", strm_id));
              goto end;
            }

            corrupted_bytes[strm_id] = dec_input[strm_id].data_len;
            DEBUG_PRINT(("stream[%d] corrupted_bytes %d\n", strm_id, corrupted_bytes[strm_id]));
          }
        }
      }

      /* Picture ID is the picture number in decoding order */
      dec_input[strm_id].pic_id = pic_decode_number[strm_id];

      dec_input[strm_id].buffer = (u8 *)((addr_t)dec_input[strm_id].stream &
                         (~BUFFER_ALIGN_MASK));
      dec_input[strm_id].buffer_bus_address = dec_input[strm_id].stream_bus_address &
                                     (~BUFFER_ALIGN_MASK);
      dec_input[strm_id].buff_len = dec_input[strm_id].data_len +
                           (dec_input[strm_id].stream_bus_address & BUFFER_ALIGN_MASK);

#ifndef _HAVE_PTHREAD_H
      AddBufferThread(NULL, strm_id);
      h264_output_thread(NULL, strm_id);
      buf_release_thread(NULL, strm_id);
      pending_flush[strm_id] = 0;
#endif //ndef _HAVE_PTHREAD_H

      /* call API function to perform decoding */
      START_SW_PERFORMANCE;
      ret = H264DecDecode(dec_inst[strm_id], &dec_input[strm_id], &dec_output[strm_id]);
      END_SW_PERFORMANCE;
      printDecodeReturn(ret);

      eos = 0;

      switch (ret) {
      case DEC_STREAM_NOT_SUPPORTED: {
        DEBUG_PRINT(("ERROR: UNSUPPORTED STREAM!\n"));
        goto end;
      }
      case DEC_HDRS_RDY: {
#ifdef DPB_REALLOC_DISABLE
        if(hdrs_rdy[strm_id]) {
          DEBUG_PRINT(("Decoding ended, flush the DPB\n"));
          /* the end of stream is not reached yet */
          H264DecEndOfStream(dec_inst[strm_id], 0);
        }
#endif
        save_flag = 0;
        /* Set a flag to indicate that headers are ready */
        hdrs_rdy[strm_id] = 1;
        DEBUG_PRINT(("sizeof(dpb) = %zu\n", sizeof(dpbStorage_t)));
        DEBUG_PRINT(("offset of dpbs = " "%" PRIXPTR "\n", (u8 *)((struct H264DecContainer *) dec_inst[strm_id])->storage.dpbs-\
			                              (u8 *)&(((struct H264DecContainer *) dec_inst[strm_id])->storage)));
        TBSetRefbuMemModel( &tb_cfg,
                            ((struct H264DecContainer *) dec_inst[strm_id])->h264_regs,
                            &((struct H264DecContainer *) dec_inst[strm_id])->ref_buffer_ctrl );

        /* Stream headers were successfully decoded
         * -> stream information is available for query now */

        START_SW_PERFORMANCE;
        tmp = H264DecGetInfo(dec_inst[strm_id], &dec_info[strm_id]);
        END_SW_PERFORMANCE;
        if(tmp != DEC_OK) {
          DEBUG_PRINT(("ERROR in getting stream info!\n"));
          goto end;
        }

        DEBUG_PRINT(("Width %d Height %d\n",
                     dec_info[strm_id].pic_width, dec_info[strm_id].pic_height));

        DEBUG_PRINT(("Cropping params: (%d, %d) %dx%d\n",
                     dec_info[strm_id].crop_params.crop_left_offset,
                     dec_info[strm_id].crop_params.crop_top_offset,
                     dec_info[strm_id].crop_params.crop_out_width,
                     dec_info[strm_id].crop_params.crop_out_height));

        DEBUG_PRINT(("MonoChrome = %d\n", dec_info[strm_id].mono_chrome));
        DEBUG_PRINT(("Interlaced = %d\n", dec_info[strm_id].interlaced_sequence));
        DEBUG_PRINT(("DPB mode   = %d\n", dec_info[strm_id].dpb_mode));
        DEBUG_PRINT(("Pictures in DPB = %d\n", dec_info[strm_id].pic_buff_size));
        DEBUG_PRINT(("Pictures in Multibuffer PP = %d\n", dec_info[strm_id].multi_buff_pp_size));

        ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                               !tb_cfg.pp_params.pipeline_e);
        if (params.fscale_cfg.fixed_scale_enabled) {
          u32 crop_w = params.ppu_cfg[0].crop.width;
          u32 crop_h = params.ppu_cfg[0].crop.height;
          if (!crop_w) crop_w = dec_info[strm_id].pic_width;
          if (!crop_h) crop_h = dec_info[strm_id].pic_height;
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

        dec_cfg.error_conceal = error_conceal;
        dec_cfg.decoder_mode = low_latency ? DEC_LOW_LATENCY :
                               low_latency_sim ? DEC_LOW_LATENCY_RTL :
                               secure ? DEC_SECURITY : DEC_NORMAL;
        dec_cfg.align = align;
        if (align == DEC_ALIGN_1B)
          dec_cfg.align = DEC_ALIGN_64B;

        tmp = H264DecSetInfo(dec_inst[strm_id], &dec_cfg);
        if (tmp != DEC_OK) {
          DEBUG_PRINT(("Invalid pp parameters\n"));
          goto end;
        }

        if(dec_info[strm_id].output_format == DEC_OUT_FRM_TILED_4X4)
          DEBUG_PRINT(("Output format = DEC_OUT_FRM_TILED_4X4\n"));
        else if(dec_info[strm_id].output_format == DEC_OUT_FRM_MONOCHROME)
          DEBUG_PRINT(("Output format = DEC_OUT_FRM_MONOCHROME\n"));
        else
          DEBUG_PRINT(("Output format = DEC_OUT_FRM_RASTER_SCAN\n"));
        prev_width = dec_info[strm_id].pic_width;
        prev_height = dec_info[strm_id].pic_height;
        min_buffer_num = dec_info[strm_id].pic_buff_size;
        dpb_mode = dec_info[strm_id].dpb_mode;
        /* release the old temp image buffer if exists */
        if(tmp_image[strm_id])
          free(tmp_image[strm_id]);

        /* check if we do need to crop */
        if(dec_info[strm_id].crop_params.crop_left_offset == 0 &&
            dec_info[strm_id].crop_params.crop_top_offset == 0 &&
            dec_info[strm_id].crop_params.crop_out_width ==
              dec_info[strm_id].pic_width &&
            dec_info[strm_id].crop_params.crop_out_height ==
              dec_info[strm_id].pic_height ) {
          crop_display = 0;
        }

        if(crop_display) {
          /* Cropped frame size in planar YUV 4:2:0 */
          pic_size[strm_id] = dec_info[strm_id].crop_params.crop_out_width *
                              dec_info[strm_id].crop_params.crop_out_height;
          if(!dec_info[strm_id].mono_chrome)
            pic_size[strm_id] = (3 * pic_size[strm_id]) / 2;
          tmp_image[strm_id] = malloc(pic_size[strm_id]);
          if(tmp_image[strm_id] == NULL) {
            DEBUG_PRINT(("ERROR in allocating cropped image!\n"));
            goto end;
          }
        } else {
          /* Decoder output frame size in planar YUV 4:2:0 */
          if (scale_enabled && scale_mode == FLEXIBLE_SCALE) {
            pic_size[strm_id] = ((scaled_w + 15) & ~15) * scaled_h;
          } else {
            pic_size[strm_id] = dec_info[strm_id].pic_width * dec_info[strm_id].pic_height;
          }
          if(!dec_info[strm_id].mono_chrome)
            pic_size[strm_id] = (3 * pic_size[strm_id]) / 2;
          if (use_peek_output)
            tmp_image[strm_id] = malloc(pic_size[strm_id]);
        }

        DEBUG_PRINT(("video_range %d, matrix_coefficients %d\n",
                     dec_info[strm_id].video_range, dec_info[strm_id].matrix_coefficients));

        break;
      }
      case DEC_ADVANCED_TOOLS: {
        /* ASO/STREAM ERROR was noticed in the stream. The decoder has to
         * reallocate resources */
        ASSERT(dec_output[strm_id].data_left); /* we should have some data left *//* Used to indicate that picture decoding needs to finalized prior to corrupting next picture */

        /* Used to indicate that picture decoding needs to finalized prior to corrupting next picture
         * pic_rdy = 0; */
#ifdef LOW_LATENCY_FRAME_MODE
      if(low_latency) {
        process_end_flag[strm_id] = 1;
        pic_decoded[strm_id] = 1;
        sem_post(&next_frame_start_sem);
        wait_for_task_completion(task);
        dec_input[strm_id].data_len = 0;
        task_has_freed = 1;
      }
#endif
        break;
      }
      case DEC_PENDING_FLUSH:
        pending_flush[strm_id] = 1;
      case DEC_PIC_DECODED:
        /* case DEC_FREEZED_PIC_RDY: */
        /* Picture is now ready */
        pic_rdy[strm_id] = 1;

        /*lint -esym(644,tmp_image,pic_size) variable initialized at
         * DEC_HDRS_RDY_BUFF_NOT_EMPTY case */

        if (ret == DEC_PIC_DECODED) {
          /* If enough pictures decoded -> force decoding to end
           * by setting that no more stream is available */
          if(pic_decode_number[strm_id] == max_num_pics) {
            process_end_flag[strm_id] = 1;
            dec_input[strm_id].data_len = 0;
          }

          DEBUG_PRINT(("DECODED PICTURE %d\n", pic_decode_number[strm_id]));
          /* Increment decoding number for every decoded picture */
          pic_decode_number[strm_id]++;
        }

        if(!output_thread_run) {
          output_thread_run = 1;
          sem_init(&buf_release_sem, 0, 0);
          pthread_create(&output_thread, NULL, h264_output_thread, NULL);
          pthread_create(&release_thread, NULL, buf_release_thread, NULL);
        }

        if(use_peek_output && ret == DEC_PIC_DECODED &&
            H264DecPeek(dec_inst[strm_id], &dec_picture[strm_id]) == DEC_PIC_RDY) {
          static u32 last_field[NUM_STREAMS] = {0, };

          /* pic coding type */
          printH264PicCodingType(dec_picture[strm_id].pic_coding_type);

          DEBUG_PRINT((", DECPIC %d\n", pic_decode_number[strm_id]));

          /* Write output picture to file. If output consists of separate fields -> store
           * whole frame before writing */
          image_data = (u8 *) dec_picture[strm_id].pictures[0].output_picture;
          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(dec_picture[strm_id].pictures[0].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(dec_picture[strm_id].pictures[0].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(dec_picture[strm_id].pictures[0].output_format) ||
                    dec_picture[strm_id].pictures[0].output_picture_chroma == NULL)
            pp_mono_chrome = 1;

          if (dec_picture[strm_id].field_picture) {
            if (last_field[strm_id] == 0)
              memcpy(tmp_image[strm_id], image_data, pic_size[strm_id]);
            else {
              u32 i;
              u8 *p_in = image_data, *p_out = tmp_image[strm_id];
              if (dec_picture[strm_id].top_field == 0) {
                p_out += dec_picture[strm_id].pictures[0].pic_width;
                p_in += dec_picture[strm_id].pictures[0].pic_width;
              }
              tmp = dec_info[strm_id].mono_chrome ?
                    dec_picture[strm_id].pictures[0].pic_height / 2 :
                    3 * dec_picture[strm_id].pictures[0].pic_height / 4;
              for (i = 0; i < tmp; i++) {
                memcpy(p_out, p_in, dec_picture[strm_id].pictures[0].pic_width);
                p_in += 2*dec_picture[strm_id].pictures[0].pic_width;
                p_out += 2*dec_picture[strm_id].pictures[0].pic_width;
              }
            }
            last_field[strm_id] ^= 1;
          } else {
            if (last_field[strm_id]) {
              WriteOutput(out_file_name[strm_id][0],
                          tmp_image[strm_id],
                          dec_picture[strm_id].pictures[0].pic_width,
                          dec_picture[strm_id].pictures[0].pic_height,
                          pic_decode_number[strm_id] - 2,
                          pp_mono_chrome,
                          dec_picture[strm_id].view_id, mvc_separate_views,
                          disable_output_writing,
                          dec_picture[strm_id].pictures[0].output_format ==
                            DEC_OUT_FRM_TILED_4X4,
                          dec_picture[strm_id].pictures[0].pic_stride,
                          dec_picture[strm_id].pictures[0].pic_stride_ch, 0,
                          pp_planar, pp_cr_first, convert_tiled_output,
                          convert_to_frame_dpb, dpb_mode, md5sum,
                          ((dec_picture[strm_id].view_id && mvc_separate_views) ?
                            &(foutput2[strm_id][0]) : &(foutput[strm_id][0])), 1,
                          strm_id);

#ifdef _ENABLE_2ND_CHROMA
              WriteOutput2ndChroma(pic_size[strm_id],
                        pic_decode_number[strm_id] - 2,
                        pp_mono_chrome,
                        dec_picture[strm_id].view_id,
                        mvc_separate_views, 0, md5sum,
                        ((dec_picture[strm_id].view_id && mvc_separate_views) ?
                        &(foutput2[strm_id][0]) : &(foutput[strm_id][0])));
#endif

            }
            last_field[strm_id] = 0;
            memcpy(tmp_image[strm_id], image_data, pic_size[strm_id]);
          }

          if (!last_field[strm_id]) {
            WriteOutput(out_file_name[strm_id][0],
                        tmp_image[strm_id],
                        dec_picture[strm_id].pictures[0].pic_width,
                        dec_picture[strm_id].pictures[0].pic_height,
                        pic_decode_number[strm_id] - 1,
                        pp_mono_chrome,
                        dec_picture[strm_id].view_id, mvc_separate_views,
                        disable_output_writing,
                        dec_picture[strm_id].pictures[0].output_format ==
                          DEC_OUT_FRM_TILED_4X4,
                        dec_picture[strm_id].pictures[0].pic_stride,
                        dec_picture[strm_id].pictures[0].pic_stride_ch, 0,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        convert_to_frame_dpb, dpb_mode, md5sum,
                        ((dec_picture[strm_id].view_id && mvc_separate_views) ?
                          &(foutput2[strm_id][0]) : &(foutput[strm_id][0])), 1,
                        strm_id);

#ifdef _ENABLE_2ND_CHROMA
            WriteOutput2ndChroma(pic_size[strm_id],
                      pic_decode_number[strm_id] - 1,
                      pp_mono_chrome,
                      dec_picture[strm_id].view_id,
                      mvc_separate_views, 0, md5sum,
                      ((dec_picture[strm_id].view_id && mvc_separate_views) ?
                      &(foutput2[strm_id][0]) : &(foutput[strm_id][0])));
#endif
          }
        }
        if(use_extra_buffers && !add_buffer_thread_run[strm_id]) {
          add_buffer_thread_run[strm_id] = 1;
          pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
        }
        retry = 0;
        break;

      case DEC_STRM_PROCESSED:
      case DEC_BUF_EMPTY:
      case DEC_NONREF_PIC_SKIPPED:
      case DEC_STRM_ERROR:
      case DEC_NO_DECODING_BUFFER: {
        /* Used to indicate that picture decoding needs to finalized prior to corrupting next picture
         * pic_rdy = 0; */

        break;
      }
      case DEC_WAITING_FOR_BUFFER: {
        DEBUG_PRINT(("Waiting for frame buffers\n"));
        struct DWLLinearMem mem;

        rv = H264DecGetBufferInfo(dec_inst[strm_id], &hbuf);
        DEBUG_PRINT(("H264DecGetBufferInfo ret %d\n", rv));
        DEBUG_PRINT(("buf_to_free %p, next_buf_size %d, buf_num %d\n",
                     (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));

        if (hbuf.buf_to_free.virtual_address != NULL) {
          add_extra_flag[strm_id] = 0;
          ReleaseExtBuffers(strm_id);
          buffer_release_flag = 1;
          num_buffers[strm_id] = 0;
        }

        buffer_size[strm_id] = hbuf.next_buf_size;
        if(buffer_release_flag && hbuf.next_buf_size) {
          /* Only add minimum required buffers at first. */
          //extra_buffer_num = hbuf.buf_num - min_buffer_num;
          for(i=0; i<hbuf.buf_num; i++) {
            mem.mem_type = DWL_MEM_TYPE_DPB;
            if (pp_enabled)
              DWLMallocLinear(dwl_inst[strm_id], hbuf.next_buf_size, &mem);
            else
              DWLMallocRefFrm(dwl_inst[strm_id], hbuf.next_buf_size, &mem);
            rv = H264DecAddBuffer(dec_inst[strm_id], &mem);
            DEBUG_PRINT(("H264DecAddBuffer ret %d\n", rv));
            if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst[strm_id], &mem);
              else
                DWLFreeRefFrm(dwl_inst[strm_id], &mem);
            } else {
              ext_buffers[strm_id][i] = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers[strm_id] = hbuf.buf_num;
          add_extra_flag[strm_id] = 1;
        }
        break;
      }
      case DEC_OK:
        /* nothing to do, just call again */
        break;
      case DEC_HW_TIMEOUT:
        DEBUG_PRINT(("Timeout\n"));
        goto end;
      default:
        DEBUG_PRINT(("FATAL ERROR: %d\n", ret));
        goto end;
      }

      /* break out of do-while if max_num_pics reached (data_len set to 0) */
      if(dec_input[strm_id].data_len == 0)
        break;

      if(long_stream && !packetize && !nal_unit_stream) {
        if(stream_will_end[strm_id]) {
          corrupted_bytes[strm_id] -= (dec_input[strm_id].data_len - dec_output[strm_id].data_left);
          dec_input[strm_id].data_len = dec_output[strm_id].data_left;
          dec_input[strm_id].stream = dec_output[strm_id].strm_curr_pos;
          dec_input[strm_id].stream_bus_address = dec_output[strm_id].strm_curr_bus_address;
        } else {
          if(use_index == 1) {
            if(dec_output[strm_id].data_left) {
              dec_input[strm_id].stream_bus_address += (dec_output[strm_id].strm_curr_pos - dec_input[strm_id].stream);
              corrupted_bytes[strm_id] -= (dec_input[strm_id].data_len - dec_output[strm_id].data_left);
              dec_input[strm_id].data_len = dec_output[strm_id].data_left;
              dec_input[strm_id].stream = dec_output[strm_id].strm_curr_pos;
            } else {
              dec_input[strm_id].stream_bus_address = (addr_t) stream_mem[strm_id].bus_address;
              dec_input[strm_id].stream = (u8 *) stream_mem[strm_id].virtual_address;
              dec_input[strm_id].data_len = fillBuffer((u8 *)dec_input[strm_id].stream, strm_id);
            }
          } else {
            if(fseek(finput[strm_id], -(i32)dec_output[strm_id].data_left, SEEK_CUR) == -1) {
              DEBUG_PRINT(("SEEK FAILED\n"));
              dec_input[strm_id].data_len = 0;
            } else {
              /* store file index */
              if(save_index && save_flag) {
                fprintf(findex[strm_id], "%" PRId64"\n", ftello64(finput[strm_id]));
              }

              dec_input[strm_id].data_len =
                fread((u8 *)dec_input[strm_id].stream, 1, max_strm_len, finput[strm_id]);
            }
          }

          if(feof(finput[strm_id])) {
            DEBUG_PRINT(("STREAM WILL END\n"));
            stream_will_end[strm_id] = 1;
          }

          corrupted_bytes[strm_id] = 0;
        }
      }

      else {
        if(dec_output[strm_id].data_left) {
          dec_input[strm_id].stream_bus_address +=
              (dec_output[strm_id].strm_curr_pos - dec_input[strm_id].stream);
          corrupted_bytes[strm_id] -= (dec_input[strm_id].data_len -
                                       dec_output[strm_id].data_left);
          dec_input[strm_id].buff_len -= (dec_output[strm_id].strm_curr_pos -
                                          dec_input[strm_id].stream);
          dec_input[strm_id].data_len = dec_output[strm_id].data_left;
          dec_input[strm_id].stream = dec_output[strm_id].strm_curr_pos;
        } else {
          dec_input[strm_id].stream_bus_address = (addr_t) stream_mem[strm_id].bus_address;
          dec_input[strm_id].stream = (u8 *) stream_mem[strm_id].virtual_address;
          dec_input[strm_id].p_user_data = stream_mem[strm_id].virtual_address;
          /*u32 streamPacketLossTmp = stream_packet_loss;
           *
           * if(!pic_rdy)
           * {
           * stream_packet_loss = 0;
           * } */
          ptmpstream = dec_input[strm_id].stream;

          tmp = ftell(finput[strm_id]);
          dec_input[strm_id].data_len = NextPacket((u8 **) (&dec_input[strm_id].stream), strm_id);
          DEBUG_PRINT(("NextPacket = %d at %d\n", dec_input[strm_id].data_len, tmp));

          if(dec_input[strm_id].data_len == 0)
            process_end_flag[strm_id] = 1;

          dec_input[strm_id].stream_bus_address +=
            (addr_t) (dec_input[strm_id].stream - ptmpstream);

          /*stream_packet_loss = streamPacketLossTmp; */

          corrupted_bytes[strm_id] = 0;
        }
      }
    }

    bContinue = 0;
    for(strm_id=0; strm_id<num_streams; strm_id++) {
      bContinue |= (dec_input[strm_id].data_len > 0);
    }
    /* keep decoding until all data from input stream buffer consumed
     * and all the decoded/queued frames are ready */
  } while(bContinue);// while(dec_input.data_len > 0);

  //end of stream processing
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    /* store last index */
    if(save_index) {
      off64_t pos = ftello64(finput[strm_id]) - dec_output[strm_id].data_left;
      fprintf(findex[strm_id], "%" PRId64"\n", pos);
    }

    if(use_index || save_index) {
      fclose(findex[strm_id]);
    }

    DEBUG_PRINT(("Decoding stream[%d] ended, flush the DPB\n", strm_id));

  }

end:
  for(strm_id=0; strm_id<num_streams; strm_id++) {
    add_buffer_thread_run[strm_id] = 0;

    H264DecEndOfStream(dec_inst[strm_id], 1);

#ifdef _HAVE_PTHREAD_H
    if(output_thread)
      pthread_join(output_thread, NULL);
    if(release_thread)
      pthread_join(release_thread, NULL);
#else //_HAVE_PTHREAD_H
    while(1) {
      if(last_pic_flag[strm_id] &&  buf_status[strm_id][list_pop_index[strm_id]] == 0)
        break;
      h264_output_thread(NULL, strm_id);
      buf_release_thread(NULL, strm_id);
    }
#endif //_HAVE_PTHREAD_H

    byte_strm_start[strm_id] = NULL;
    fflush(stdout);

    if(stream_mem[strm_id].virtual_address != NULL) {
#ifndef ADS_PERFORMANCE_SIMULATION
      if(dec_inst[strm_id])
        DWLFreeLinear(dwl_inst[strm_id], &stream_mem[strm_id]);
#else
      free(stream_mem[strm_id].virtual_address);
#endif
    }

    /* release decoder instance */
    START_SW_PERFORMANCE;
    H264DecRelease(dec_inst[strm_id]);
    END_SW_PERFORMANCE;
    ReleaseExtBuffers(strm_id);
    pthread_mutex_destroy(&ext_buffer_contro);
    DWLRelease(dwl_inst[strm_id]);

    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if(foutput[strm_id][i])
        fclose(foutput[strm_id][i]);
      if(foutput2[strm_id][i])
        fclose(foutput2[strm_id][i]);
      if(f_tiled_output[strm_id][i])
        fclose(f_tiled_output[strm_id][i]);
    }

    if(fchroma2[strm_id])
      fclose(fchroma2[strm_id]);
    if(finput[strm_id])
      fclose(finput[strm_id]);

    /* free allocated buffers */
    if(tmp_image[strm_id] != NULL)
      free(tmp_image[strm_id]);
    if(grey_chroma[strm_id] != NULL)
      free(grey_chroma[strm_id]);
    if(pic_big_endian[strm_id])
      free(pic_big_endian[strm_id]);
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      foutput[strm_id][i] = fopen(out_file_name[strm_id][i], "rb");
      if(NULL == foutput[strm_id][i]) {
        strm_len[strm_id] = 0;
      } else {
        fseek(foutput[strm_id][i], 0L, SEEK_END);
        strm_len[strm_id] = (u32) ftell(foutput[strm_id][i]);
        fclose(foutput[strm_id][i]);
      }
      if (strm_len[strm_id]) {
        DEBUG_PRINT(("Output file: %s\n", out_file_name[strm_id][i]));
        DEBUG_PRINT(("OUTPUT_SIZE %ld\n", strm_len[strm_id]));
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

    if(num_errors[strm_id] || pic_decode_number[strm_id] == 1) {
      DEBUG_PRINT(("Stream[%d] ERRORS FOUND %d %d\n", strm_id, num_errors[strm_id], pic_decode_number[strm_id]));
      /*return 1;*/
//      return 0;
    }
  }
  
  (void)prev_width;
  (void)prev_height;
  (void)min_buffer_num;
  (void)eos;
  (void)ret_unused;
  return 0;
}

u32 TBWriteOutput(FILE* fout,u8* p_lu, u8 *p_ch,
                  u32 coded_width, u32 coded_height, u32 pic_stride,
                  u32 coded_width_ch, u32 coded_h_ch, u32 pic_stride_ch,
                  u32 md5Sum, u32 planar, u32 mono_chrome, u32 frame_number,
                  u32 pixel_bytes) {
  u32 i;
  if (md5Sum)
    TBWriteFrameMD5SumValidOnly(fout, p_lu, p_ch, coded_width, coded_height,
                                coded_width_ch, coded_h_ch,
                                pic_stride, pic_stride_ch,
                                planar, frame_number);
  else {
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
  }
  return 0;
}

/*------------------------------------------------------------------------------

    Function name:  WriteOutput

    Purpose:
        Write picture pointed by data to file. Size of the
        picture in pixels is indicated by pic_size.

------------------------------------------------------------------------------*/
void WriteOutput(char *filename, u8 * data,
                 u32 pic_width, u32 pic_height, u32 frame_number, u32 mono_chrome,
                 u32 view, u32 mvc_separate_views, u32 disable_output_writing,
                 u32 tiled_mode, u32 pic_stride, u32 pic_stride_ch, u32 index,
                 u32 planar, u32 cr_first, u32 convert_tiled_output,
                 u32 convert_to_frame_dpb, u32 dpb_mode, u32 md5sum, FILE **fout,
                 u32 pixel_bytes, u32 strm_id) {
  u32 e_height_luma = pic_height;
  char* fn = NULL;
  u8 *raster_scan = NULL;
  u8 *out_data = NULL;
  char alt_file_name[256];
  u32 pic_size = 0;

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
    grey_chroma[strm_id] = NULL;
  }

  TBWriteOutput(*fout, data,
                mono_chrome ? grey_chroma[strm_id] : data + pic_stride * e_height_luma,
                out_w, out_h, pic_stride,
                pp_ch_w, pp_ch_h,
                pic_stride_ch,
                md5sum, planar,
                mono_chrome, frame_number, pixel_bytes);

  if(raster_scan)
    free(raster_scan);
  if(out_data)
    free(out_data);
  (void)pic_size;
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

u32 NextPacket(u8 ** p_strm, u32 strm_id) {
  u32 index;
  u32 max_index;
  u32 zero_count;
  u8 byte;
  static u32 prev_index[NUM_STREAMS] = {0, };
  static u8 *stream[NUM_STREAMS] = {NULL, };
  u8 next_packet = 0;

  /* Next packet is read from file is long stream support is enabled */
  if(long_stream) {
    return NextPacketFromFile(p_strm, strm_id);
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

  if(stream[strm_id] == NULL)
    stream[strm_id] = *p_strm;
  else
    stream[strm_id] += prev_index[strm_id];

  max_index = (u32) (stream_stop[strm_id] - stream[strm_id]);

  if(stream[strm_id] > stream_stop[strm_id])
    return (0);

  if(max_index == 0)
    return (0);

  /* leading zeros of first NAL unit */
  do {
    byte = stream[strm_id][index++];
  } while(byte != 1 && index < max_index);

  /* invalid start code prefix */
  if(index == max_index || index < 3) {
    DEBUG_PRINT(("INVALID BYTE STREAM\n"));
    return 0;
  }

  /* nal_unit_stream is without start code prefix */
  if(nal_unit_stream) {
    stream[strm_id] += index;
    max_index -= index;
    index = 0;
  }

  zero_count = 0;

  /* Search stream for next start code prefix */
  /*lint -e(716) while(1) used consciously */
  while(1) {
    byte = stream[strm_id][index++];
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
  *p_strm = stream[strm_id];
  prev_index[strm_id] = index;

  /* If we skip this packet */
  if(pic_rdy[strm_id] && next_packet &&
      ((hdrs_rdy[strm_id] && !stream_header_corrupt) || stream_header_corrupt)) {
    /* Get the next packet */
    DEBUG_PRINT(("Packet Loss\n"));
    return NextPacket(p_strm, strm_id);
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
u32 NextPacketFromFile(u8 ** p_strm, u32 strm_id) {

  u32 index = 0;
  u32 zero_count = 0;
  u8 byte = 0;
  u8 next_packet = 0;
  i32 ret = 0;
  u8 first_read = 1;
  fpos_t strm_pos;
  static u8 *stream[NUM_STREAMS] = {NULL, NULL};

  /* store the buffer start address for later use */
  if(stream[strm_id] == NULL)
    stream[strm_id] = *p_strm;
  else
    *p_strm = stream[strm_id];

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

  if(fgetpos(finput[strm_id], &strm_pos)) {
    DEBUG_PRINT(("FILE POSITION GET ERROR\n"));
    return 0;
  }

  if(use_index == 0) {
    /* test end of stream */
    ret = fread(&byte, 1, 1, finput[strm_id]);
    if(ferror(finput[strm_id])) {
      DEBUG_PRINT(("STREAM READ ERROR\n"));
      return 0;
    }
    if(feof(finput[strm_id])) {
      DEBUG_PRINT(("END OF STREAM\n"));
      return 0;
    }

    /* leading zeros of first NAL unit */
    do {
      index++;
      /* the byte is already read to test the end of stream */
      if(!first_read) {
        ret = fread(&byte, 1, 1, finput[strm_id]);
        if(ferror(finput[strm_id])) {
          DEBUG_PRINT(("STREAM READ ERROR\n"));
          return 0;
        }
      } else {
        first_read = 0;
      }
    } while(byte != 1 && !feof(finput[strm_id]));

    /* invalid start code prefix */
    if(feof(finput[strm_id]) || index < 3) {
      DEBUG_PRINT(("INVALID BYTE STREAM\n"));
      return 0;
    }

    /* nal_unit_stream is without start code prefix */
    if(nal_unit_stream) {
      if(fgetpos(finput[strm_id], &strm_pos)) {
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
      ret = fread(&byte, 1, 1, finput[strm_id]);
      if(ferror(finput[strm_id])) {
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

      if(feof(finput[strm_id])) {
        --index;
        break;
      }
    }

    /* Store pointer to the beginning of the packet */
    if(fsetpos(finput[strm_id], &strm_pos)) {
      DEBUG_PRINT(("FILE POSITION SET ERROR\n"));
      return 0;
    }

    if(save_index) {
      ret = fprintf(*findex, "%" PRIu64 "\n", strm_pos);
      if(nal_unit_stream) {
        /* store amount */
        ret = fprintf(findex[strm_id], "%u\n", index);
      }
    }

    /* Read the rewind stream */
    ret = fread(*p_strm, 1, index, finput[strm_id]);
    
	if(feof(finput[strm_id])) {
      DEBUG_PRINT(("TRYING TO READ STREAM BEYOND STREAM END\n"));
      return 0;
    }
    if(ferror(finput[strm_id])) {
      DEBUG_PRINT(("FILE ERROR\n"));
      return 0;
    }
  } else {
    u32 amount = 0;
    u32 f_pos = 0;

    if(nal_unit_stream)
	  ret = fscanf(findex[strm_id], "%zu", &cur_index[strm_id]);

    /* check position */
    f_pos = ftell(finput[strm_id]);
    if(f_pos != cur_index[strm_id]) {
      fseeko64(finput[strm_id], cur_index[strm_id] - f_pos, SEEK_CUR);
    }

    if(nal_unit_stream) {
      ret = fscanf(findex[strm_id], "%u", &amount);
    } 
	else {
	  ret = fscanf(findex[strm_id], "%zu", &next_index[strm_id]);
      amount = next_index[strm_id] - cur_index[strm_id];
      cur_index[strm_id] = next_index[strm_id];
    }

    ret = fread(*p_strm, 1, amount, finput[strm_id]);
    index = amount;
  }
  /* If we skip this packet */
  if(pic_rdy[strm_id] && next_packet &&
      ((hdrs_rdy[strm_id] && !stream_header_corrupt) || stream_header_corrupt)) {
    /* Get the next packet */
    DEBUG_PRINT(("Packet Loss\n"));
    return NextPacket(p_strm, strm_id);
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
    (void)ret;
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
    DEBUG_PRINT(("[I:"));
    break;
  case DEC_PIC_TYPE_P:
    DEBUG_PRINT(("[P:"));
    break;
  case DEC_PIC_TYPE_B:
    DEBUG_PRINT(("[B:"));
    break;
  default:
    DEBUG_PRINT(("[Other %d:", pic_type[0]));
    break;
  }

  switch (pic_type[1]) {
  case DEC_PIC_TYPE_I:
    DEBUG_PRINT(("I]"));
    break;
  case DEC_PIC_TYPE_P:
    DEBUG_PRINT(("P]"));
    break;
  case DEC_PIC_TYPE_B:
    DEBUG_PRINT(("B]"));
    break;
  default:
    DEBUG_PRINT(("Other %d]", pic_type[1]));
    break;
  }
}

u32 fillBuffer(u8 *stream, u32 strm_id) {
  u32 amount = 0;
  u32 data_len = 0;
  u32 ret_unused = 0;
  
  if(cur_index[strm_id] != ftell(finput[strm_id])) {
    fseeko64(finput[strm_id], cur_index[strm_id], SEEK_SET);
  }

  /* read next index */
  ret_unused = fscanf(findex[strm_id], "%zu", &next_index[strm_id]);

  amount = next_index[strm_id] - cur_index[strm_id];
  cur_index[strm_id] = next_index[strm_id];

  /* read data */
  data_len = fread(stream, 1, amount, finput[strm_id]);

  (void)ret_unused;
  return data_len;
}

