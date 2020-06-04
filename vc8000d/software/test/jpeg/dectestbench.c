/*------------------------------------------------------------------------------
i-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "jpegdecapi.h"
#include "dwl.h"
#include "jpegdeccontainer.h"

#ifdef PP_PIPELINE_ENABLED
#include "ppapi.h"
#include "pptestbench.h"
#endif

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include "deccfg.h"
#include "tb_cfg.h"
#include "regdrv.h"
#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "tb_md5.h"
#include "command_line_parser.h"
#include "tb_out.h"

#ifndef MAX_PATH_
#define MAX_PATH   256  /* maximum lenght of the file path */
#endif
#define DEFAULT -1
#define JPEG_INPUT_BUFFER 0x5120

#define JPEG_INPUT_BUFFER_SIZE  (8192 * 8192)

#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define ALIGN(a) (1 << (a))

#define MAX_BUFFERS 4

#define PERFETCH_EOI_NUM 100
#define RI_MAX_NUM  128

 #ifdef ASIC_ONL_SIM
  u32 dec_done;
//  #else
//  sem_t dec_done;  /* Semaphore which is signalled when decoding is done. */
  #endif

/* SW/SW testing, read stream trace file */
FILE *f_stream_trace = NULL;

/* memory parameters */
static u32 out_pic_size_luma;
static u32 out_pic_size_chroma;
static u32 out_pic_size;
static struct DWLLinearMem output_address_y;
static struct DWLLinearMem output_address_cb_cr;
static u32 frame_ready = 0;
static u32 sliced_output_used = 0;
static u32 mode = 0;
static u32 ThumbDone = 0;
static u32 write_output = 1;
static u32 size_luma = 0;
static u32 size_chroma = 0;
static u32 pp_enabled = 1;
static u32 slice_to_user = 0;
static u32 slice_size = 0;
static u32 non_interleaved = 0;
static i32 full_slice_counter = -1;
static u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
static u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
/* Use independent chroma buffers (currently not supported). */
/* otherwise chroma buffers is allocated in luma buffers. */

static u32 md5sum = 0;

/* stream start address */
u8 *byte_strm_start;

/* user allocated output */
struct DWLLinearMem user_alloc_output_buffer[MAX_BUFFERS] = {{0}};

pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS] = {{0}};

/* progressive parameters */
static u32 scan_counter = 0;
static u32 progressive = 0;
static u32 scan_ready = 0;
static u32 is8170_hw = 0;
static u32 nbr_of_images = 0;
static u32 nbr_of_thumb_images = 0;
static u32 nbr_of_images_total = 0;
static u32 nbr_of_thumb_images_total = 0;
static u32 nbr_of_images_to_out = 0;
static u32 nbr_of_thumb_images_to_out = 0;
static u32 thumb_in_stream = 0;
static u32 next_soi = 0;
static u32 stream_info_check = 0;
static u32 prev_output_width = 0;
static u32 prev_output_height = 0;
static u32 prev_output_format = 0;
static u32 prev_output_width_tn = 0;
static u32 prev_output_height_tn = 0;
static u32 prev_output_format_tn = 0;
static u32 pic_counter = 0;

/* temp string for option parsing */
char tmp_str[512];

/* prototypes */
u32 allocMemory(JpegDecInst dec_inst, JpegDecImageInfo * image_info,
                JpegDecInput * jpeg_in, const void *dwl, u32 buffer_size);
u32 inputMemoryAlloc(JpegDecInst dec_inst, JpegDecImageInfo * image_info,
                     JpegDecInput * jpeg_in, const void *dwl, JpegDecBufferInfo *mem_info);
void calcSize(JpegDecImageInfo * image_info, u32 pic_mode, JpegDecOutput * jpeg_out, u32 i);
void WriteOutputLuma(u8 * data_luma, u32 pic_size_luma, u32 pic_mode);
void WriteOutputChroma(u8 * data_chroma, u32 pic_size_chroma, u32 pic_mode);
void WriteFullOutput(u32 pic_mode);

void handleSlicedOutput(JpegDecImageInfo * image_info, JpegDecInput * jpeg_in,
                        JpegDecOutput * jpeg_out);

void WriteCroppedOutput(JpegDecImageInfo * info, u8 * data_luma, u8 * data_cb,
                        u8 * data_cr);

void WriteProgressiveOutput(u32 size_luma, u32 size_chroma, u32 mode,
                            u8 * data_luma, u8 * data_cb, u8 * data_cr);
void printJpegVersion(void);

void decsw_performance(void) {
}

void *JpegDecMalloc(unsigned int size);
void *JpegDecMemset(void *ptr, int c, unsigned int size);
void JpegDecFree(void *ptr);

void PrintJpegRet(JpegDecRet jpeg_ret);
void PrintGetImageInfo(JpegDecImageInfo * image_info);
u32 FindImageInfoEnd(u8 * stream, u32 stream_length, u32 * p_offset);
u32 FindImageEnd(u8 * stream, u32 stream_length, u32 * p_offset);
u32 FindImageEOI(u8 * stream, u32 stream_length, u32 * p_offset);
u32 FindImageTnEOI(u8 * stream, u32 stream_length, u32 * p_offset, u32 mode, u32 thumb_exist);
static u32 FindImageAllRI(u8 *img_buf, u32 img_len, u32 *ri_array, u32 ri_count);

FILE *fout[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL, NULL};
FILE *fout_table[4] = {NULL, NULL, NULL, NULL};
FILE *fout_tn[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL, NULL};
char out_file_name[DEC_MAX_PPU_COUNT][MAX_PATH] = {"", "", "", "", ""};
char out_file_tn_name[DEC_MAX_PPU_COUNT][MAX_PATH] = {"", "", "", "", ""};
enum SCALE_MODE scale_mode;
u32 scaled_w, scaled_h;
u32 planar_output = 0;
u32 ystride = 0;
u32 cstride = 0;
u32 only_full_resolution = 0;
u32 ds_ratio_x = 1;
u32 ds_ratio_y = 1;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 128 bytes alignment */
u32 output_thread_run = 0;
u32 last_pic_flag = 0;
u32 crop = 0;
u32 mc_enable = 0;
/* restart interval based multicore decoding */
u32 ri_mc_enable = 0;
u32 ri_count = 0;
u32 *ri_array = NULL;
u32 first_open = 1;
pthread_t output_thread;
JpegDecInst jpeg;

u32 add_external_buffer = 0;

#define MAX_STRM_BUFFERS    (MAX_ASIC_CORES + 1)

static struct DWLLinearMem stream_mem_input[MAX_STRM_BUFFERS];
static u32 stream_mem_status[MAX_STRM_BUFFERS];
static u32 frame_mem_status[MAX_BUFFERS];
u32 allocated_buffers = MAX_STRM_BUFFERS;
u32 output_buffers = MAX_BUFFERS;
u32 wr_id;

static sem_t stream_buff_free;
static sem_t frame_buff_free;
#ifdef _HAVE_PTHREAD_H
static pthread_mutex_t strm_buff_stat_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t frm_buff_stat_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static pthread_mutex_t strm_buff_stat_lock = {0, };
static pthread_mutex_t frm_buff_stat_lock = {0, };
#endif //_HAVE_PTHREAD_H

#ifdef ASIC_TRACE_SUPPORT
u32 pic_number;
#endif

struct TBCfg tb_cfg;

#ifdef JPEG_EVALUATION
extern u32 g_hw_ver;
#endif

void PrintUsageJpeg(char *s)
{
  fprintf(stdout, "USAGE:\n%s [-X] [-S] [-P] [-R] [-F] [--full-only] stream.jpg\n", s);
  fprintf(stdout, "\t--help(-h) Print command line parameters help info\n");
  fprintf(stdout, "\t-O<file> write output to <file> (default out.yuv)\n");
  fprintf(stdout, "\t-X to not to write output picture\n");
  fprintf(stdout, "\t-S file.hex stream control trace file\n");
  fprintf(stdout, "\t-a write PP planar output(--pp-planar)\n");
  fprintf(stdout, "\t-U Enable PP tile output\n");
  fprintf(stdout, "\t-R use reference idct (implies cropping)\n");
  fprintf(stdout, "\t-F Force 8170 mode to HW model\n");
  fprintf(stdout, "\t-m Output frame based MD5 checksum, no yuv output.(--md5-per-pic)\n");
  fprintf(stdout, "\t--full-only Force to full resolution decoding only\n");
  fprintf(stdout, "\t-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n");
  fprintf(stdout, "\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g.\n");
  fprintf(stdout, "\t  -d2 -- down scale to 1/2 in both directions\n");
  fprintf(stdout, "\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
  fprintf(stdout, "\t--cr-first PP outputs chroma in CrCb order.\n");
  fprintf(stdout, "\t--mc    enable multi-core decoding.\n");
  fprintf(stdout, "\t--ri-mc enable restart interval based multi-core decoding.\n");
  fprintf(stdout, "\t-C[xywh]NNN Cropping parameters. E.g.,\n");
  fprintf(stdout, "\t  -Cx8 -Cy16        Crop from (8, 16)\n");
  fprintf(stdout, "\t  -Cw720 -Ch480     Crop size  720x480\n");
  fprintf(stdout, "\t-Dwxh  PP output size wxh. E.g.,\n");
  fprintf(stdout, "\t  -D1280x720        PP output size 1280x720\n");
  fprintf(stdout, "\t-s[yc]NNN  Set stride for y/c plane, sw check validation of the value. E.g.,\n");
  fprintf(stdout, "\t  -sy720 -sc360     set ystride and cstride to 720 and 360\n");
  fprintf(stdout, "\t--instant_buffer Output buffer provided by Instant mode.(external).\n");
  fprintf(stdout, "\t--cache_enable Enable cache rtl simulation(external).\n");
  fprintf(stdout, "\t--shaper_enable Enable shaper rtl simulation(external).\n");
  fprintf(stdout, "\t--shaper_bypass Enable shaper bypass rtl simulation(external).\n");
  fprintf(stdout, "\t--ultra-low-latency enable low latency RTL simulation flag.\n");
  fprintf(stdout, "\t--delogo(--enable), enable delogo feature,");
  fprintf(stdout, "need configure parameters as the following:\n");
  fprintf(stdout, "\t\t pos -- the delog pos wxh@(x,y)\n");
  fprintf(stdout, "\t\t show -- show the delogo border\n");
  fprintf(stdout, "\t\t mode -- select the delogo mode\n");
  fprintf(stdout, "\t\t YUV -- set the replace value if use PIXEL_REPLACE mode\n");
  fprintf(stdout, "\t--second-crop enable second crop configure.\n");
}

void StreamBufferConsumed(u8 *stream, void *p_user_data) {
  int idx;
  pthread_mutex_lock(&strm_buff_stat_lock);

  idx = 0;
  do {
    if ((u8*)stream >= (u8*)stream_mem_input[idx].virtual_address &&
        (u8*)stream < (u8*)stream_mem_input[idx].virtual_address + stream_mem_input[idx].size) {
      stream_mem_status[idx] = 0;
      assert(p_user_data == stream_mem_input[idx].virtual_address);
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

static void* jpeg_output_thread(void* arg) {
  JpegDecOutput dec_picture;
  JpegDecImageInfo info_tmp;
  u32 pic_display_number = 1;
  u32 i = 0, j = 0;
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
    JpegDecRet ret;
    u32 first_index = 0;
    u32 first_index_enable = 0;

    ret = JpegDecNextPicture(jpeg, &dec_picture, &info_tmp);

    if(ret == JPEGDEC_FRAME_READY) {
      if(!mode)
        pic_display_number++;
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (!tb_cfg.pp_units_params[i].unit_enabled)
          continue;
        if (!first_index_enable) {
          first_index = i;
          first_index_enable = 1;
        }
        if(dec_picture.pictures[i].output_picture_y.virtual_address != NULL) {
          /* calculate size for output */
          calcSize(&info_tmp, mode, &dec_picture, i);
          fprintf(stdout, "\n\t-JPEG: ++++++++++ FULL RESOLUTION ++++++++++\n");
          fprintf(stdout, "\t-JPEG: Instance %p\n", (void *)jpeg);
          fprintf(stdout, "\t-JPEG: Luma output: %p size: %d\n",
                  (void *)dec_picture.pictures[i].output_picture_y.virtual_address, size_luma);
          fprintf(stdout, "\t-JPEG: Chroma output: %p size: %d\n",
                  (void *)dec_picture.pictures[i].output_picture_cb_cr.virtual_address, size_chroma);
          fprintf(stdout, "\t-JPEG: Luma output bus: 0x%16zx\n",
                  dec_picture.pictures[i].output_picture_y.bus_address);
          fprintf(stdout, "\t-JPEG: Chroma output bus: 0x%16zx\n",
                  dec_picture.pictures[i].output_picture_cb_cr.bus_address);
        }

        if(info_tmp.output_format) {
          switch (info_tmp.output_format) {
          case JPEGDEC_YCbCr400:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr400\n");
            break;
          case JPEGDEC_YCbCr420_SEMIPLANAR:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr420_SEMIPLANAR\n");
            break;
          case JPEGDEC_YCbCr422_SEMIPLANAR:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr422_SEMIPLANAR\n");
            break;
          case JPEGDEC_YCbCr440:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr440\n");
            break;
          case JPEGDEC_YCbCr411_SEMIPLANAR:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n");
            break;
          case JPEGDEC_YCbCr444_SEMIPLANAR:
            fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n");
            break;
          }
        }
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(dec_picture.pictures[i].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(dec_picture.pictures[i].output_format))
          pp_cr_first = 1;
        else if (IS_PIC_MONOCHROME(dec_picture.pictures[i].output_format))
          pp_mono_chrome = 1;
        if (write_output) {
          if(mode) {
            if (!IS_PIC_PACKED_RGB(dec_picture.pictures[i].output_format) &&
                !IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format))
#ifndef SUPPORT_DEC400
                    WriteOutput(out_file_tn_name[i],
                                ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
                                dec_picture.pictures[i].display_width_thumb,
                                dec_picture.pictures[i].display_height_thumb,
                                pic_display_number - 1,
                                pp_mono_chrome, 0, 0, 0,
                                IS_PIC_TILE(dec_picture.pictures[i].output_format),
                                dec_picture.pictures[i].pic_stride,
                                dec_picture.pictures[i].pic_stride_ch, i,
                                pp_planar, pp_cr_first, 0,
                                0, 0, md5sum, &fout[i], 1);
#else
                   WriteOutputdec400(out_file_tn_name[i],
                                ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
                                dec_picture.pictures[i].display_width_thumb,
                                dec_picture.pictures[i].display_height_thumb,
                                pic_display_number - 1,
                                pp_mono_chrome, 0, 0, 0,
                                IS_PIC_TILE(dec_picture.pictures[i].output_format),
                                dec_picture.pictures[i].pic_stride,
                                dec_picture.pictures[i].pic_stride_ch, i,
                                pp_planar, pp_cr_first, 0,
                                0, 0, md5sum, &fout[i], 1, &fout_table[i],
                                (u8 *) dec_picture.pictures[i].luma_table.virtual_address,
                                dec_picture.pictures[i].luma_table.size,
                                (u8 *) dec_picture.pictures[i].chroma_table.virtual_address,
                                dec_picture.pictures[i].chroma_table.size);
#endif
            else
              WriteOutputRGB(out_file_tn_name[i],
                             ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
                             dec_picture.pictures[i].display_width_thumb,
                             dec_picture.pictures[i].display_height_thumb,
                             pic_display_number - 1,
                             pp_mono_chrome, 0, 0, 0,
                             IS_PIC_TILE(dec_picture.pictures[i].output_format),
                             dec_picture.pictures[i].pic_stride,
                             dec_picture.pictures[i].pic_stride_ch, i,
                             IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format), pp_cr_first, 0,
                             0, 0, md5sum, &fout[i], 1);
    }else {
      if (!IS_PIC_PACKED_RGB(dec_picture.pictures[i].output_format) &&
          !IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format))
#ifndef SUPPORT_DEC400
              WriteOutput(out_file_name[i],
                          ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
                          dec_picture.pictures[i].display_width,
                          dec_picture.pictures[i].display_height,
                          pic_display_number - 1,
                          pp_mono_chrome, 0, 0, 0,
                          IS_PIC_TILE(dec_picture.pictures[i].output_format),
                          dec_picture.pictures[i].pic_stride,
                          dec_picture.pictures[i].pic_stride_ch, i,
                          pp_planar, pp_cr_first, 0,
                          0, 0, md5sum, &fout_tn[i], 1);
#else
             WriteOutputdec400(out_file_name[i],
                          ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
                          dec_picture.pictures[i].display_width,
                          dec_picture.pictures[i].display_height,
                          pic_display_number - 1,
                          pp_mono_chrome, 0, 0, 0,
                          IS_PIC_TILE(dec_picture.pictures[i].output_format),
                          dec_picture.pictures[i].pic_stride,
                          dec_picture.pictures[i].pic_stride_ch, i,
                          pp_planar, pp_cr_first, 0,
                          0, 0, md5sum, &fout_tn[i], 1, &fout_table[i],
                          (u8 *) dec_picture.pictures[i].luma_table.virtual_address,
                          dec_picture.pictures[i].luma_table.size,
                          (u8 *) dec_picture.pictures[i].chroma_table.virtual_address,
                          dec_picture.pictures[i].chroma_table.size);
#endif
      else
        WriteOutputRGB(out_file_name[i],
           ((u8 *) dec_picture.pictures[i].output_picture_y.virtual_address),
            dec_picture.pictures[i].display_width,
            dec_picture.pictures[i].display_height,
            pic_display_number - 1,
            pp_mono_chrome, 0, 0, 0,
            IS_PIC_TILE(dec_picture.pictures[i].output_format),
            dec_picture.pictures[i].pic_stride,
            dec_picture.pictures[i].pic_stride_ch, i,
            IS_PIC_PLANAR_RGB(dec_picture.pictures[i].output_format), pp_cr_first, 0,
            0, 0, md5sum, &fout[i], 1);
    }

        if(crop)
          WriteCroppedOutput(&info_tmp,
                             (u8*)dec_picture.pictures[i].output_picture_y.virtual_address,
                             (u8*)dec_picture.pictures[i].output_picture_cb_cr.virtual_address,
                             (u8*)dec_picture.pictures[i].output_picture_cr.virtual_address);
        }

        for (j = 0; j < output_buffers; j++) {
          if(dec_picture.pictures[first_index].output_picture_y.virtual_address ==
               user_alloc_output_buffer[j].virtual_address) {
            pthread_mutex_lock(&frm_buff_stat_lock);
            frame_mem_status[j] = 0;
            pthread_mutex_unlock(&frm_buff_stat_lock);

            sem_post(&frame_buff_free);
#if 0
          DWLFreeLinear(dwl, &(output_stream_mem_luma[i]));
          DWLmemset(&(output_stream_mem_luma[i]), 0, sizeof(struct DWLLinearMem));

          DWLFreeLinear(dwl, &(output_stream_mem_chroma[i]));
          DWLmemset(&(output_stream_mem_chroma[i]), 0, sizeof(struct DWLLinearMem));
#endif
          }
        }
      }
      JpegDecPictureConsumed(jpeg, &dec_picture);
    } else if(ret == JPEGDEC_END_OF_STREAM) {
      last_pic_flag = 1;
      break;
    }
  }
  return NULL;
}

int main_jpeg_tb(int argc, char *argv[]);

#ifdef ASIC_ONL_SIM
int main_onl(int argc, char* sv_str) {
  char* argv[argc];
  char* delim=" " ;
  int   j_new=0       ;

  char* p=strtok(sv_str,delim);
  argv[j_new]=p;
  while(p!=NULL){
    p=strtok(NULL,delim);
    j_new++;
    argv[j_new]=p;
  }
  return(main_jpeg_tb(argc,argv));
}

#else
int main(int argc, char *argv[]) {
  return(main_jpeg_tb(argc,argv));
}
#endif

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

#ifdef __FREERTOS__
void jpegDec_run_task(void *args);
typedef struct Main_args_
{
  int argc;
  char **argv;
}Main_args;
#define mainPOSIX_DEMO_PRIORITY    ( tskIDLE_PRIORITY + 4 )
#endif/* __FREERTOS__ */

int main_jpeg_tb(int argc, char *argv[]) {
  osal_thread_init();
#ifdef __FREERTOS__
  Main_args args = {argc, argv};
  xTaskCreate(  jpegDec_run_task,
                 "posix",
                 configMINIMAL_STACK_SIZE,
				 &args,
                 mainPOSIX_DEMO_PRIORITY,
                 NULL );
  vTaskStartScheduler();
  return 0;
}
#endif /* __FREERTOS__ */

#ifdef __FREERTOS__
void jpegDec_run_task(void *args)
{
  Main_args *args_ = (Main_args *)args;
  int argc = args_->argc;
  char ** argv = args_->argv;
#endif /* __FREERTOS__ */
  int return_status = 0;

  u32 len = 0;
  u32 stream_total_len = 0;
  u32 stream_seek_len = 0;

  u32 stream_in_file = 0;
#ifdef SLICE_MODE_LARGE_PIC
  u32 mcu_size_divider = 0;
  u32 amount_of_mcus = 0;
  u32 mcu_in_row = 0;
#endif

  i32 i, j = 0;
  u32 tmp = 0;
  u32 input_read_type = 0;
  u32 frame_len_prefetch[PERFETCH_EOI_NUM] = {0};
  u32 frame_len = 0;
  u32 frame_len_index = 0;
  u32 temp_len = 0;
  int ret = 0;
  u32 delogo = 0;
  u32 second_crop = 0;
  u32 pp_planar = 0, pp_cr_first = 0, pp_mono_chrome = 0;

  JpegDecRet jpeg_ret;
  JpegDecBufferInfo hbuf;
  JpegDecRet rv;
  JpegDecImageInfo image_info;
  JpegDecInput jpeg_in;
  JpegDecOutput jpeg_out;
  JpegDecApiVersion dec_ver;
  JpegDecBuild dec_build;
  struct JpegDecConfig dec_cfg = {0};
  struct DWLLinearMem stream_mem;
  const void *dwl;

  pthread_mutex_init(&ext_buffer_contro, NULL);

  u8 *p_image = NULL;

  FILE *f_in = NULL;

  u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
  u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
  u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
  u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
  u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
  u32 low_latency_sim = 0;

#ifdef PP_PIPELINE_ENABLED
  PPApiVersion pp_ver;
  PPBuild pp_build;
#endif

  u32 stream_header_corrupt = 0;
  u32 seed_rnd = 0;
  u32 stream_bit_swap = 0;
  u32 stream_truncate = 0;
  FILE *f_tbcfg;
  u32 image_info_length = 0;
  u32 prev_ret = JPEGDEC_STRM_ERROR;
  u32 major_version, minor_version, prod_id;

  u32 scale_enabled = 0;
  u32 crop_x = 0;
  u32 crop_y = 0;
  u32 crop_w = 0;
  u32 crop_h = 0;
  /* TODO(min): use legacy parsing temporarily. When the common parser is fully
     tested, switch to common parser and remove old one. */
  struct TestParams params;
  struct TestParams new_params;
  struct DWLInitParam dwl_init;

  SetupDefaultParams(&params);
  SetupDefaultParams(&new_params);
#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 8190;   /* default to 8190 mode */
#endif

#ifdef JPEG_EVALUATION_8170
  g_hw_ver = 8170;
#elif JPEG_EVALUATION_8190
  g_hw_ver = 8190;
#elif JPEG_EVALUATION_9170
  g_hw_ver = 9170;
#elif JPEG_EVALUATION_9190
  g_hw_ver = 9190;
#elif JPEG_EVALUATION_G1
  g_hw_ver = 10000;
#endif

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */

  /* expiry stuff */
  {
    char tm_buf[7];
    time_t sys_time;
    struct tm *tm;
    u32 tmp1;

    /* Check expiry date */
    time(&sys_time);
    tm = localtime(&sys_time);
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000 + atoi(tm_buf);
    if(tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1) {
      fprintf(stderr,
              "EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
	  return_status = -1;
      goto return_;
    }
  }

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;
  for(i = 0; i < MAX_STRM_BUFFERS; i++) {
    stream_mem_input[i].virtual_address = NULL;
    stream_mem_input[i].bus_address = 0;
  }

  INIT_SW_PERFORMANCE;

  fprintf(stdout, "\n* * * * * * * * * * * * * * * * \n\n\n"
          "      "
          "X170 JPEG TESTBENCH\n" "\n\n* * * * * * * * * * * * * * * * \n");

  /* reset input */
  jpeg_in.stream_buffer.virtual_address = NULL;
  jpeg_in.stream_buffer.bus_address = 0;
  jpeg_in.stream_length = 0;
  jpeg_in.picture_buffer_y.virtual_address = NULL;
  jpeg_in.picture_buffer_y.bus_address = 0;
  jpeg_in.picture_buffer_cb_cr.virtual_address = NULL;
  jpeg_in.picture_buffer_cb_cr.bus_address = 0;
  jpeg_in.picture_buffer_cr.virtual_address = NULL;
  jpeg_in.picture_buffer_cr.bus_address = 0;

  /* reset output */
  for (i = 0 ; i < DEC_MAX_OUT_COUNT; i++) {
    memset(&jpeg_out.pictures[i], 0, sizeof(jpeg_out.pictures[i]));
  }

  /* reset image_info */
  image_info.display_width = 0;
  image_info.display_height = 0;
  image_info.output_width = 0;
  image_info.output_height = 0;
  image_info.version = 0;
  image_info.units = 0;
  image_info.x_density = 0;
  image_info.y_density = 0;
  image_info.output_format = 0;
  image_info.thumbnail_type = 0;
  image_info.display_width_thumb = 0;
  image_info.display_height_thumb = 0;
  image_info.output_width_thumb = 0;
  image_info.output_height_thumb = 0;
  image_info.output_format_thumb = 0;
  image_info.coding_mode = 0;
  image_info.coding_mode_thumb = 0;

  /* set default */
  jpeg_in.slice_mb_set = 0;
  jpeg_in.buffer_size = 0;

  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if(f_tbcfg == NULL) {
    printf("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n");
    printf("USING DEFAULT CONFIGURATION\n");
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE) {
	  return_status = -1;
      goto return_;
    }
    if(TBCheckCfg(&tb_cfg) != 0) {
	  return_status = -1;
      goto return_;
    }
  }

  /* Print API and build version numbers */
  dec_ver = JpegGetAPIVersion();
  dec_build = JpegDecGetBuild();

  /* Version */
  fprintf(stdout,
          "\nX170 JPEG Decoder API v%d.%d - SW build: %d - HW build: %x\n",
          dec_ver.major, dec_ver.minor, dec_build.sw_build, dec_build.hw_build);

  major_version = (dec_build.hw_build & 0xF000) >> 12;
  minor_version = (dec_build.hw_build & 0x0FF0) >> 4;
  prod_id = dec_build.hw_build >> 16;

  if(argc < 2) {
    PrintUsageJpeg(argv[0]);
    exit(100);
  }

  if(argc == 2) {
    if ((strncmp(argv[1], "-h", 2) == 0) ||
            (strcmp(argv[1], "--help") == 0)){
      PrintUsageJpeg(argv[0]);
	    return_status = 0;
      goto return_;
    }
  }

  /* read cmdl parameters */
  for(i = 1; i < argc - 1; i++) {
    if(strncmp(argv[i], "-X", 2) == 0) {
      params.sink_type = SINK_NULL;
      write_output = 0;
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      tmp = 2;
      while(*(argv[i] + tmp) == '\0') {
        tmp++;
      }
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++) {
        strcpy(out_file_name[j], argv[i] + tmp);
        params.out_file_name[j] = argv[i] + tmp;
      }
      if(tmp != 2)
        i++;
    }else if(strncmp(argv[i], "-S", 2) == 0) {
      f_stream_trace = fopen((argv[i] + 2), "r");
    } else if(strncmp(argv[i], "--pp-planar", 11) == 0 ||
              strncmp(argv[i], "-a", 2) == 0) {
      params.ppu_cfg[0].planar = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      params.format = DEC_OUT_FRM_PLANAR_420;
      params.ppu_cfg[0].rgb_format = DEC_OUT_FRM_RGB888_P;
      //tb_cfg.pp_units_params[0].planar = pp_planar_out;
      pp_enabled = 1;
    } else if(strcmp(argv[i], "--full-only") == 0) {
      params.only_full_resolution = 1;
      only_full_resolution = 1;
      printf("\n\nForce to decode only full resolution image from stream\n\n");
    }
#ifdef ASIC_TRACE_SUPPORT
    else if(strncmp(argv[i], "-R", 2) == 0) {
      use_jpeg_idct = 1;
      crop = 1;
    }
#endif
#ifdef SUPPORT_CACHE
    else if(strcmp(argv[i], "--cache_enable") == 0) {
      tb_cfg.cache_enable = 1;
      params.cache_enable = 1;
    } else if(strcmp(argv[i], "--shaper_enable") == 0) {
      tb_cfg.shaper_enable = 1;
      params.shaper_enable = 1;
    } else if(strcmp(argv[i], "--shaper_bypass") == 0) {
      tb_cfg.shaper_bypass = 1;
      params.shaper_bypass = 1;
    } else if(strcmp(argv[i], "--pp-shaper") == 0) {
      params.ppu_cfg[0].shaper_enabled = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    }
#endif
    else if(strncmp(argv[i], "-m", 2) == 0 ||
            strcmp(argv[i], "--md5-per-pic") == 0) {
      params.sink_type = SINK_MD5_PICTURE;
      md5sum = 1;
    } else if (strncmp(argv[i], "-d", 2) == 0) {
      pp_enabled = 1;
#if 0
      if (!(major_version == 7 && minor_version == 1)) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
		return_status = 1;
        goto return_;
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
        return_status = 1;
        goto return_;
      }
      scale_mode = FIXED_DOWNSCALE;
      scale_enabled = 1;
      params.pp_enabled = 1;
      params.format = DEC_OUT_FRM_RASTER_SCAN;
      params.ppu_cfg[0].scale.set_by_user = 1;
      params.fscale_cfg.fixed_scale_enabled = 1;
      params.fscale_cfg.down_scale_x = ds_ratio_x;
      params.fscale_cfg.down_scale_y = ds_ratio_y;
    } else if (strncmp(argv[i], "-D", 2) == 0) {
      char *px;
      memcpy(tmp_str, argv[i], strlen(argv[i]));
      px = strchr(&tmp_str[2], 'x');
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return_status = 1;
        goto return_;
      } else if (!px) {
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable scaler parameter by using: -D[w]x[h]\n");
        return_status = 1;
        goto return_;
      }
      *px = '\0';
      scale_enabled = 1;
      pp_enabled = 1;
      scaled_w = atoi(tmp_str+2);
      scaled_h = atoi(px+1);
      if (scaled_w == 0 || scaled_h == 0) {
        fprintf(stdout, "Illegal scaled width/height: %sx%s\n", argv[i]+2, px+1);
        return_status = 1;
        goto return_;
      }
      scale_mode = FLEXIBLE_SCALE;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].scale.enabled = 1;
      params.ppu_cfg[0].scale.set_by_user = 1;
      params.ppu_cfg[0].scale.width = scaled_w;
      params.ppu_cfg[0].scale.height = scaled_h;
      params.pp_enabled = 1;
    } else if (strncmp(argv[i], "-C", 2) == 0) {
      pp_enabled = 1;
      if (!(prod_id != 0x6731 || (major_version == 7 && minor_version == 2))) {
        fprintf(stdout, "ERROR: Unsupported parameter %s on HW build [%d:%d]\n",
                argv[i], major_version, minor_version);
        return_status = 1;
        goto return_;
      }
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].crop.enabled = 1;
      params.ppu_cfg[0].crop.set_by_user = 1;
      params.pp_enabled = 1;
      switch (argv[i][2]) {
      case 'x':
        crop_x = atoi(argv[i]+3);
        if (second_crop)
          params.ppu_cfg[0].crop2.x = crop_x;
        else
          params.ppu_cfg[0].crop.x = crop_x;
        break;
      case 'y':
        crop_y = atoi(argv[i]+3);
        if (second_crop)
          params.ppu_cfg[0].crop2.y = crop_y;
        else
          params.ppu_cfg[0].crop.y = crop_y;
        break;
      case 'w':
        crop_w = atoi(argv[i]+3);
        if (second_crop)
          params.ppu_cfg[0].crop2.width = crop_w;
        else
          params.ppu_cfg[0].crop.width = crop_w;
        break;
      case 'h':
        crop_h = atoi(argv[i]+3);
        if (second_crop)
          params.ppu_cfg[0].crop2.height = crop_h;
        else
          params.ppu_cfg[0].crop.height = crop_h;
        break;
      default:
        fprintf(stdout, "Illegal parameter: %s\n", argv[i]);
        fprintf(stdout, "ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,\n");
        fprintf(stdout, "\t -CxXXX -CyYYY     Crop from (XXX, YYY)\n");
        fprintf(stdout, "\t -CwWWW -ChHHH     Crop size  WWWxHHH\n");
		return_status = 1;
        goto return_;
      }
    } else if (strncmp(argv[i], "-s", 2) == 0) {
      pp_enabled = 1;
      switch (argv[i][2]) {
      case 'y':
        ystride = params.ppu_cfg[0].ystride = atoi(argv[i]+3);
        break;
      case 'c':
        cstride = params.ppu_cfg[0].cstride = atoi(argv[i]+3);
        break;
      default:
        fprintf(stdout, "ERROR: Enable out stride parameter by using: -s[yc]NNN. E.g.,\n");
        return_status = -1;
        goto return_;
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
		    return_status = 1;
        goto return_;
      }
      params.align = align;
    } else if (strcmp(argv[i], "--cr-first") == 0) {
      params.ppu_cfg[0].enabled = 1;
      pp_enabled = 1;
      params.pp_enabled = 1;
      params.ppu_cfg[0].cr_first = 1;
    } else if (strcmp(argv[i], "--pp-luma-only") == 0) {
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].monochrome = 1;
      params.format = DEC_OUT_FRM_YUV400;
      pp_enabled = 1;
    } else if (strcmp(argv[i], "--mc") == 0) {
      params.mc_enable = 1;
      mc_enable = 1;
    } else if (strcmp(argv[i], "--ri-mc") == 0) {
      ri_mc_enable = params.ri_mc_enable = 1;
    } else if ((strncmp(argv[i], "-h", 2) == 0) ||
              (strcmp(argv[i], "--help") == 0)){
      PrintUsageJpeg(argv[0]);
    } else if (strcmp(argv[i], "-U") == 0) {
      pp_enabled = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
      params.ppu_cfg[0].tiled_e = 1;
      params.format = DEC_OUT_FRM_TILED_4X4;
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
        params.ppu_cfg[0].rgb_format = PP_OUT_ARGB888;
      else if (strcmp(argv[i]+11, "ABGR888") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_ABGR888;
      else if (strcmp(argv[i]+11, "A2R10G10B10") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_A2R10G10B10;
      else if (strcmp(argv[i]+11, "A2B10G10R10") == 0)
        params.ppu_cfg[0].rgb_format = PP_OUT_A2B10G10R10;
      else {
        fprintf(stdout, "Illegal parameter\n");
		return_status = 1;
        goto return_;
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
 	    return_status = 1;
        goto return_;
      }
    } else if (strncmp(argv[i], "--delogo=", 9) == 0) {
      delogo = atoi(argv[i]+9);
      if (delogo >= 2 || delogo < 0) {
        fprintf(stdout, "ERROR, Invalid index for option \"--delogo\":%s\n", argv[i]);
 	    return_status = 1;
        goto return_;
      }
      params.delogo_params[delogo].enabled = 1;
    } else if (strncmp(argv[i], "--pos=", 6) == 0) {
      if (ParsePosParams(argv[i]+6,&(params.delogo_params[delogo].x),
                                   &(params.delogo_params[delogo].y),
                                   &(params.delogo_params[delogo].w),
                                   &(params.delogo_params[delogo].h))) {
        fprintf(stderr, "ERROR: Illegal parameters: %s\n",argv[i]+7);
        return_status = 1;
        goto return_;
      }
    } else if (strncmp(argv[i], "--show=", 7) == 0) {
      params.delogo_params[delogo].show = atoi(argv[i]+7);
    } else if (strncmp(argv[i], "--mode=",7) == 0) {
      if (strcmp(argv[i]+7, "PIXEL_REPLACE") == 0) {
        params.delogo_params[delogo].mode = PIXEL_REPLACE;
      } else if (strcmp(argv[i]+7, "PIXEL_INTERPOLATION") == 0) {
        params.delogo_params[delogo].mode = PIXEL_INTERPOLATION;
        if (delogo == 1) {
          fprintf(stderr, "ERROR, delogo params 1 not support PIXEL_INTERPOLATION\n");
          return_status = 1;
          goto return_;
        }
      } else {
        fprintf(stderr, "ERROR, no supported mode\n");
        return_status = 1;
        goto return_;
      }
    } else if (strncmp(argv[i], "--YUV=", 6) == 0) {
      if (ParsePixelParams(argv[i]+6, &(params.delogo_params[delogo].Y),
                                      &(params.delogo_params[delogo].U),
                                      &(params.delogo_params[delogo].V))) {
        fprintf(stderr, "ERROR: Illegal parameters: %s\n",argv[i]+7);
        return_status = 1;
        goto return_;
      }
    } else if (strcmp(argv[i], "--second-crop") == 0) {
      second_crop = 1;
      params.ppu_cfg[0].crop2.enabled = 1;
      params.ppu_cfg[0].crop2.x = params.ppu_cfg[0].crop.x;
      params.ppu_cfg[0].crop2.y = params.ppu_cfg[0].crop.y;
      params.ppu_cfg[0].crop2.width = params.ppu_cfg[0].crop.width;
      params.ppu_cfg[0].crop2.height = params.ppu_cfg[0].crop.height;
      params.ppu_cfg[0].crop.x = params.ppu_cfg[0].crop.y =
      params.ppu_cfg[0].crop.width = params.ppu_cfg[0].crop.height = 0;
    } else if (strcmp(argv[i], "--instant_buffer") == 0) {
      params.instant_buffer = 1;
    } else if (strcmp(argv[i], "--ultra-low-latency") == 0) {
      low_latency_sim = 1;
    } else {
      fprintf(stdout, "UNKNOWN PARAMETER: %s\n", argv[i]);
 	  return_status = 1;
      goto return_;
    }
  }

  params.in_file_name = argv[argc - 1];

  /* "-D" option will override "-d" */
  if (params.fscale_cfg.fixed_scale_enabled &&
      params.ppu_cfg[0].scale.enabled) {
    params.fscale_cfg.fixed_scale_enabled = 0;
  }

  if (pp_enabled && !scale_enabled &&
      (!tb_cfg.pp_units_params[0].unit_enabled &&
       !tb_cfg.pp_units_params[1].unit_enabled &&
       !tb_cfg.pp_units_params[2].unit_enabled &&
       !tb_cfg.pp_units_params[3].unit_enabled)) {
    scale_enabled = 1;
    params.fscale_cfg.fixed_scale_enabled = 1;
    params.fscale_cfg.down_scale_x = 1;
    params.fscale_cfg.down_scale_y = 1;
  }

  /* Use common command line parser to parse options. */
  ParseParams(argc, argv, &new_params);
  if (!new_params.pp_enabled) {
    if (!new_params.ppu_cfg[0].enabled)
      new_params.fscale_cfg.fixed_scale_enabled = 1;
    new_params.pp_enabled = 1;
    new_params.ppu_cfg[0].enabled = 1;
    new_params.format = DEC_OUT_FRM_RASTER_SCAN;
    new_params.fscale_cfg.down_scale_x = 1;
    new_params.fscale_cfg.down_scale_y = 1;
    params.pp_enabled = 1;
    params.ppu_cfg[0].enabled = 1;
  }
  ASSERT(pp_enabled == new_params.pp_enabled);
  ASSERT(params.mc_enable == new_params.mc_enable);
  ASSERT(only_full_resolution == new_params.only_full_resolution);
  ASSERT(write_output == (new_params.sink_type != SINK_NULL));
  ASSERT(md5sum == (new_params.sink_type == SINK_MD5_PICTURE));
  ASSERT(ds_ratio_x == new_params.fscale_cfg.down_scale_x);
  ASSERT(ds_ratio_y == new_params.fscale_cfg.down_scale_y);
  ASSERT(scaled_w == new_params.ppu_cfg[0].scale.width);
  ASSERT(scaled_h == new_params.ppu_cfg[0].scale.height);
  ASSERT(ystride == new_params.ppu_cfg[0].ystride);
  ASSERT(cstride == new_params.ppu_cfg[0].cstride);
  ASSERT(ri_mc_enable == new_params.ri_mc_enable);
  if (params.pp_enabled &&
     !params.ppu_cfg[0].tiled_e && !params.ppu_cfg[0].planar &&
     !params.ppu_cfg[0].monochrome)
  params.format = DEC_OUT_FRM_RASTER_SCAN;
  if (memcmp(&params, &new_params, sizeof(new_params))) {
    printf("Different options from common parser\n");
    goto return_;
  }

  /* check if 8170 HW */
  is8170_hw = (dec_build.hw_build >> 16) == 0x8170U ? 1 : 0;

  /*TBPrintCfg(&tb_cfg); */
  jpeg_in.buffer_size = tb_cfg.dec_params.jpeg_input_buffer_size;
  jpeg_in.slice_mb_set = tb_cfg.dec_params.jpeg_mcus_slice;
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);
  printf("Decoder Jpeg Input Buffer Size %d\n", jpeg_in.buffer_size);
  printf("Decoder Slice MB Set %d\n", jpeg_in.slice_mb_set);
  printf("Decoder Clock Gating %d\n", clock_gating);
  printf("Decoder Data Discard %d\n", data_discard);
  printf("Decoder Latency Compensation %d\n", latency_comp);
  printf("Decoder Output Picture Endian %d\n", output_picture_endian);
  printf("Decoder Bus Burst Length %d\n", bus_burst_length);
  printf("Decoder Asic Service Priority %d\n", asic_service_priority);

  seed_rnd = tb_cfg.tb_params.seed_rnd;
  stream_header_corrupt = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  stream_truncate = TBGetTBStreamTruncate(&tb_cfg);
  if(strcmp(tb_cfg.tb_params.stream_bit_swap, "0") != 0) {
    stream_bit_swap = 1;
  } else {
    stream_bit_swap = 0;
  }
  printf("TB Seed Rnd %d\n", seed_rnd);
  printf("TB Stream Truncate %d\n", stream_truncate);
  printf("TB Stream Header Corrupt %d\n", stream_header_corrupt);
  printf("TB Stream Bit Swap %d; odds %s\n", stream_bit_swap,
         tb_cfg.tb_params.stream_bit_swap);

  {
    remove("output.hex");
    remove("registers.hex");
    remove("picture_ctrl.hex");
    remove("picture_ctrl.trc");
    remove("jpeg_tables.hex");
    remove("out.yuv");
    remove("out_chroma.yuv");
    remove("out_tn.yuv");
    remove("out_chroma_tn.yuv");
  }
#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
  if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
    tb_cfg.dec_params.cache_support = 1;
  else
    tb_cfg.dec_params.cache_support = 0;
#endif
#endif
#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if(!tmp) {
    fprintf(stdout, "Unable to open trace file(s)\n");
  }
#endif

  #ifdef ASIC_ONL_SIM
       dec_done = 0;
//  #else
//     sem_init(dec_done, 0, 0);
  #endif

  dwl_init.client_type = DWL_CLIENT_TYPE_JPEG_DEC;
  /* Initialize Wrapper */
  dwl = DWLInit(&dwl_init);
  if(dwl == NULL) {
    goto end;
  }

  /* after thumnails done ==> decode full images */
start_full_decode:

  /******** PHASE 1 ********/
  fprintf(stdout, "\nPhase 1: INIT JPEG DECODER\n");
  add_external_buffer = 0;
  /* Jpeg initialization */
  START_SW_PERFORMANCE;
  decsw_performance();
  JpegDecMCConfig mc_init_cfg;
  enum DecDecoderMode decoder_mode = DEC_NORMAL;
  if (low_latency_sim)
    decoder_mode = DEC_LOW_LATENCY_RTL;

  mc_init_cfg.mc_enable = params.mc_enable;
  mc_init_cfg.stream_consumed_callback = StreamBufferConsumed;
  jpeg_ret = JpegDecInit(&jpeg, dwl, decoder_mode, &mc_init_cfg);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(jpeg_ret != JPEGDEC_OK) {
    /* Handle here the error situation */
    PrintJpegRet(jpeg_ret);
    goto end;
  }

#ifdef PP_PIPELINE_ENABLED
  /* Initialize the post processer. If unsuccessful -> exit */
  if(pp_startup
      (params.in_file_name, jpeg, PP_PIPELINED_DEC_TYPE_JPEG, &tb_cfg) != 0) {
    fprintf(stdout, "PP INITIALIZATION FAILED\n");
    goto end;
  }

  if(pp_update_config
      (jpeg, PP_PIPELINED_DEC_TYPE_JPEG, &tb_cfg) == CFG_UPDATE_FAIL)

  {
    fprintf(stdout, "PP CONFIG LOAD FAILED\n");
    goto end;
  }
#endif

  /* NOTE: The registers should not be used outside decoder SW for other
   * than compile time setting test purposes */
  SetDecRegister(((JpegDecContainer *) jpeg)->jpeg_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((JpegDecContainer *) jpeg)->jpeg_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  SetDecRegister(((JpegDecContainer *) jpeg)->jpeg_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((JpegDecContainer *) jpeg)->jpeg_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);

  SetDecRegister(((JpegDecContainer *) jpeg)->jpeg_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  sem_init(&stream_buff_free, 0, allocated_buffers);
  sem_init(&frame_buff_free, 0, output_buffers);

  if (params.mc_enable) {
    for(i = 0; i < allocated_buffers; i++) {
      if(DWLMallocLinear(dwl,
                         JPEG_INPUT_BUFFER_SIZE, stream_mem_input + i) != DWL_OK) {
        printf("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
        goto end;
      }
    }
  }

  fprintf(stdout, "PHASE 1: INIT JPEG DECODER successful\n");

  /******** PHASE 2 ********/
  fprintf(stdout, "\nPhase 2: OPEN/READ FILE \n");

reallocate_input_buffer:

  /* Reading input file */
  f_in = fopen(params.in_file_name, "rb");
  if(f_in == NULL) {
    fprintf(stdout, "Unable to open input file\n");
    exit(-1);
  }

  /* file i/o pointer to full */
  fseek(f_in, 0L, SEEK_END);
  len = ftell(f_in);
  rewind(f_in);

  if(!stream_info_check) {
    fprintf(stdout, "\nPhase 2: CHECK THE CONTENT OF STREAM BEFORE ACTIONS\n");

    /* NOTE: The DWL should not be used outside decoder SW
     * here we call it because it is the easiest way to get
     * dynamically allocated linear memory
     * */

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    if(DWLMallocLinear(dwl, len, &stream_mem) != DWL_OK) {
      fprintf(stdout, "UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
      goto end;
    }

    fprintf(stdout, "\t-Input: Allocated buffer: virt: 0x%p bus: 0x%16zx\n",
            (void *)stream_mem.virtual_address, stream_mem.bus_address);

    /* memset input */
    (void) DWLmemset(stream_mem.virtual_address, 0, len);

    byte_strm_start = (u8 *) stream_mem.virtual_address;
    if(byte_strm_start == NULL) {
      fprintf(stdout, "UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
      goto end;
    }

    /* read input stream from file to buffer and close input file */
    ret = fread(byte_strm_start, sizeof(u8), len, f_in);
    (void) ret;

    fclose(f_in);

    jpeg_ret = FindImageEnd(byte_strm_start, len, &image_info_length);

    if(jpeg_ret != 0) {
      printf("EOI missing from end of file!\n");
    }

    if(stream_mem.virtual_address != NULL)
      DWLFreeLinear(dwl, &stream_mem);

    /* set already done */
    stream_info_check = 1;

    fprintf(stdout, "PHASE 2: CHECK THE CONTENT OF STREAM BEFORE ACTIONS successful\n\n");
  }

  /* Reading input file */
  f_in = fopen(params.in_file_name, "rb");
  if(f_in == NULL) {
    fprintf(stdout, "Unable to open input file\n");
    exit(-1);
  }

  /* file i/o pointer to full */
  fseek(f_in, 0L, SEEK_END);
  len = ftell(f_in);
  rewind(f_in);

  TBInitializeRandom(seed_rnd);

  /* sets the stream length to random value */
  if(stream_truncate) {
    u32 ret = 0;

    printf("\tStream length %d\n", len);
    ret = TBRandomizeU32(&len);
    if(ret != 0) {
      printf("RANDOM STREAM ERROR FAILED\n");
      goto end;
    }
    printf("\tRandomized stream length %d\n", len);
  }

  /* Handle input buffer load */
  if(jpeg_in.buffer_size) {
    if(len > jpeg_in.buffer_size) {
      stream_total_len = len;
      len = jpeg_in.buffer_size;
    } else {
      stream_total_len = len;
      len = stream_total_len;
      jpeg_in.buffer_size = 0;
    }
  } else {
    jpeg_in.buffer_size = 0;
    stream_total_len = len;
  }

  if(prev_ret != JPEGDEC_FRAME_READY && !stream_in_file)
    stream_in_file = stream_total_len;

  /* NOTE: The DWL should not be used outside decoder SW
   * here we call it because it is the easiest way to get
   * dynamically allocated linear memory
   * */

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  if(DWLMallocLinear(dwl, len, &stream_mem) != DWL_OK) {
    fprintf(stdout, "UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    goto end;
  }

  fprintf(stdout, "\t-Input: Allocated buffer: virt: %p bus: 0x%16zx\n",
          (void *)stream_mem.virtual_address, stream_mem.bus_address);

  /* memset input */
  (void) DWLmemset(stream_mem.virtual_address, 0, len);

  byte_strm_start = (u8 *) stream_mem.virtual_address;
  if(byte_strm_start == NULL) {
    fprintf(stdout, "UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    goto end;
  }

  /* file i/o pointer to full */
  if (!params.mc_enable) {
    if(prev_ret == JPEGDEC_FRAME_READY)
      fseek(f_in, stream_seek_len, SEEK_SET);
  }

  /* read input stream from file to buffer and close input file */
  ret = fread(byte_strm_start, sizeof(u8), len, f_in);
  (void) ret;

  fclose(f_in);

  if (params.mc_enable) {
    u32 i = 0;
    u32 tmp = 0;
    int id;

    frame_len_index = 0;
    while (i < PERFETCH_EOI_NUM) {
      jpeg_ret = FindImageEOI(byte_strm_start + tmp, len - tmp, &frame_len_prefetch[i]);
      if(jpeg_ret != 0) {
        break;
      }
      tmp += frame_len_prefetch[i];
      i++;
    }

    if(jpeg_ret != 0 && i == 0) {
      printf("NO MORE IMAGES!\n");
      goto end;
    }

    id = GetFreeStreamBuffer();
    if (stream_mem_input[id].size < frame_len_prefetch[frame_len_index]) {
      /* Default input buffer size is not enough, reallocate it. */
      DWLFreeLinear(dwl, stream_mem_input + id);
      DWLMallocLinear(dwl,
                      frame_len_prefetch[frame_len_index],
                      stream_mem_input + id);
    }
    jpeg_in.stream_buffer.virtual_address = (u32 *) stream_mem_input[id].virtual_address;
    jpeg_in.stream_buffer.bus_address = (addr_t) stream_mem_input[id].bus_address;

    /* stream processed callback param */
    jpeg_in.p_user_data = stream_mem_input[id].virtual_address;
    jpeg_in.stream_length = frame_len = frame_len_prefetch[frame_len_index++];
    DWLmemcpy(jpeg_in.stream_buffer.virtual_address, byte_strm_start, frame_len);
    temp_len += frame_len;  //after 1st frame buffer set, set temp_len
  } else {
    /* initialize JpegDecDecode input structure */
    jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
    jpeg_in.stream_buffer.bus_address = stream_mem.bus_address +
                                        ((addr_t)byte_strm_start - (addr_t)stream_mem.virtual_address);
    if(!pic_counter)
      jpeg_in.stream_length = stream_total_len;
    else
      jpeg_in.stream_length = stream_in_file;
  }

  if(write_output)
    fprintf(stdout, "\t-File: Write output: YES: %d\n", write_output);
  else
    fprintf(stdout, "\t-File: Write output: NO: %d\n", write_output);
  fprintf(stdout, "\t-File: MbRows/slice: %d\n", jpeg_in.slice_mb_set);
  fprintf(stdout, "\t-File: Buffer size: %d\n", jpeg_in.buffer_size);
  fprintf(stdout, "\t-File: Stream size: %d\n", jpeg_in.stream_length);

  if(params.instant_buffer)
    fprintf(stdout, "\t-File: Output allocated by Instant Buffer mode: %d\n",
            params.instant_buffer);
  else
    fprintf(stdout, "\t-File: Output allocated by Pre-added Buffer mode: %d\n",
            params.instant_buffer);

  fprintf(stdout, "\nPhase 2: OPEN/READ FILE successful\n");

  /* jump here is frames still left */
decode:

  /******** PHASE 3 ********/
  fprintf(stdout, "\nPhase 3: GET IMAGE INFO\n");
  if (!params.mc_enable) {
    jpeg_ret = FindImageInfoEnd(byte_strm_start, len, &image_info_length);
    printf("\timage_info_length %d\n", image_info_length);
    /* If image info is not found, do not corrupt the header */
    if(jpeg_ret != 0) {
      if(stream_header_corrupt) {
        u32 ret = 0;

        ret =
          TBRandomizeBitSwapInStream(byte_strm_start, image_info_length,
                                     tb_cfg.tb_params.stream_bit_swap);
        if(ret != 0) {
          printf("RANDOM STREAM ERROR FAILED\n");
          goto end;
        }
      }
    }
  } else {
    jpeg_ret = FindImageInfoEnd((u8*)jpeg_in.stream_buffer.virtual_address, frame_len, &image_info_length);
    printf("\timage_info_length %d\n", image_info_length);
    /* If image info is not found, do not corrupt the header */
    if(jpeg_ret != 0) {
      if(stream_header_corrupt) {
        u32 ret = 0;

        ret =
          TBRandomizeBitSwapInStream((u8*)jpeg_in.stream_buffer.virtual_address, image_info_length,
                                     tb_cfg.tb_params.stream_bit_swap);
        if(ret != 0) {
          printf("RANDOM STREAM ERROR FAILED\n");
          goto end;
        }
      }
    }
  }

  /* Get image information of the JFIF and decode JFIF header */
  START_SW_PERFORMANCE;
  decsw_performance();
  jpeg_ret = JpegDecGetImageInfo(jpeg, &jpeg_in, &image_info);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(jpeg_ret != JPEGDEC_OK) {
    /* Handle here the error situation */
    PrintJpegRet(jpeg_ret);
    if(JPEGDEC_INCREASE_INPUT_BUFFER == jpeg_ret) {
      DWLFreeLinear(dwl, &stream_mem);
      jpeg_in.buffer_size += 256;
      goto reallocate_input_buffer;
    } else {
      /* printf JpegDecGetImageInfo() info */
      fprintf(stdout, "\n\t--------------------------------------\n");
      fprintf(stdout, "\tNote! IMAGE INFO WAS CHANGED!!!\n");
      fprintf(stdout, "\t--------------------------------------\n\n");
      PrintGetImageInfo(&image_info);
      fprintf(stdout, "\t--------------------------------------\n");

      /* check if MJPEG stream and Thumb decoding ==> continue to FULL */
      if(mode) {
        if( nbr_of_thumb_images &&
            image_info.output_width_thumb == prev_output_width_tn &&
            image_info.output_height_thumb == prev_output_height_tn &&
            image_info.output_format_thumb == prev_output_format_tn) {
          fprintf(stdout, "\n\t--------------------------------------\n");
          fprintf(stdout, "\tNote! THUMB INFO NOT CHANGED ==> DECODE!!!\n");
          fprintf(stdout, "\t--------------------------------------\n\n");
        } else {
          ThumbDone = 1;
          nbr_of_thumb_images = 0;
          pic_counter = 0;
          stream_seek_len = 0;
          stream_in_file = 0;
          goto end;
        }
      } else {
        /* if MJPEG and only THUMB changed ==> continue */
        if( image_info.output_width == prev_output_width &&
            image_info.output_height == prev_output_height &&
            image_info.output_format == prev_output_format) {
          fprintf(stdout, "\n\t--------------------------------------\n");
          fprintf(stdout, "\tNote! FULL IMAGE INFO NOT CHANGED ==> DECODE!!!\n");
          fprintf(stdout, "\t--------------------------------------\n\n");
        } else {
          nbr_of_images = 0;
          pic_counter = 0;
          stream_seek_len = 0;
          stream_in_file = 0;
          goto end;
        }
      }
    }
  }

  /* update the alignment setting in "image_info" data structure
   and output picture width */;
  dec_cfg.align = align;
  if (align == DEC_ALIGN_1B)
    dec_cfg.align = DEC_ALIGN_64B;

  image_info.align = dec_cfg.align;
  image_info.output_width = NEXT_MULTIPLE(image_info.output_width, ALIGN(image_info.align));
  image_info.output_width_thumb = NEXT_MULTIPLE(image_info.output_width_thumb, ALIGN(image_info.align));

  /* save for MJPEG check */
  /* full */
  prev_output_width = image_info.output_width;
  prev_output_height = image_info.output_height;
  prev_output_format = image_info.output_format;
  /* thumbnail */
  prev_output_width_tn = image_info.output_width_thumb;
  prev_output_height_tn = image_info.output_height_thumb;
  prev_output_format_tn = image_info.output_format_thumb;

  /* printf JpegDecGetImageInfo() info */
  PrintGetImageInfo(&image_info);
#if 0
  /* update output_width/height for output buffer allocation if scaled is enabled */
  if ( scale_enabled ) {
    image_info.output_width = NEXT_MULTIPLE(dec_cfg.ppu_config[0].scale.width, ALIGN(image_info.align));
    image_info.output_width_thumb = NEXT_MULTIPLE(dec_cfg.ppu_config[0].scale.width, ALIGN(image_info.align));
    image_info.output_height = dec_cfg.ppu_config[0].scale.height;
    image_info.output_height_thumb = dec_cfg.ppu_config[0].scale.height;
  }
#endif

  /*  ******************** THUMBNAIL **************************** */
  /* Select if Thumbnail or full resolution image will be decoded */
  if(image_info.thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    /* if all thumbnails processed (MJPEG) */
    if(!ThumbDone)
      jpeg_in.dec_image_type = JPEGDEC_THUMBNAIL;
    else
      jpeg_in.dec_image_type = JPEGDEC_IMAGE;

    thumb_in_stream = 1;
  } else if(image_info.thumbnail_type == JPEGDEC_NO_THUMBNAIL)
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;
  else if(image_info.thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT)
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;

  /* check if forced to decode only full resolution images
      ==> discard thumbnail */
  if(only_full_resolution) {
    /* decode only full resolution image */
    fprintf(stdout,
            "\n\tNote! FORCED BY USER TO DECODE ONLY FULL RESOLUTION IMAGE\n");
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;
  }

  fprintf(stdout, "PHASE 3: GET IMAGE INFO successful\n");

  /* TB SPECIFIC == LOOP IF THUMBNAIL IN JFIF */
  /* Decode JFIF */
  if(jpeg_in.dec_image_type == JPEGDEC_THUMBNAIL)
    mode = 1; /* TODO KIMA */
  else
    mode = 0;

  /* Set PP info after image info and decoding mode is determined. */
#if 0
  if (tb_cfg.ppu_index == -1) {
    if (pp_enabled) {
      u32 display_width = NEXT_MULTIPLE(NEXT_MULTIPLE(image_info.display_width, 2) / ds_ratio_x, 2);
      u32 display_height = NEXT_MULTIPLE(NEXT_MULTIPLE(image_info.display_height, 2) / ds_ratio_y, 2);
      u32 display_width_thumb = NEXT_MULTIPLE(NEXT_MULTIPLE(image_info.display_width_thumb, 2) / ds_ratio_x, 2);
      u32 display_height_thumb = NEXT_MULTIPLE(NEXT_MULTIPLE(image_info.display_height_thumb, 2) / ds_ratio_y, 2);

      /* No PpUnitParams in tb.cfg, use command line options instead */
      if (crop_enabled) {
        tb_cfg.pp_units_params[0].crop_x = crop_x;
        tb_cfg.pp_units_params[0].crop_y = crop_y;
        tb_cfg.pp_units_params[0].crop_width = crop_w;
        tb_cfg.pp_units_params[0].crop_height = crop_h;
      } else {
        tb_cfg.pp_units_params[0].crop_x = 0;
        tb_cfg.pp_units_params[0].crop_y = 0;
        tb_cfg.pp_units_params[0].crop_width = 0;
        tb_cfg.pp_units_params[0].crop_height = 0;
      }
      if (scale_mode == FIXED_DOWNSCALE) {
        if (!mode) {
          tb_cfg.pp_units_params[0].scale_width = display_width;
          tb_cfg.pp_units_params[0].scale_height = display_height;
          if (!crop_enabled) {
            tb_cfg.pp_units_params[0].crop_width = display_width;
            tb_cfg.pp_units_params[0].crop_height = display_height;
          }
        } else {
          tb_cfg.pp_units_params[0].scale_width = display_width_thumb;
          tb_cfg.pp_units_params[0].scale_height = display_height_thumb;
          if (!crop_enabled) {
            tb_cfg.pp_units_params[0].crop_width = display_width_thumb;
            tb_cfg.pp_units_params[0].crop_height = display_height_thumb;
          }
        }
      } else {
        tb_cfg.pp_units_params[0].scale_width = scaled_w;
        tb_cfg.pp_units_params[0].scale_height = scaled_h;
        if (!crop_enabled) {
          if (!mode) {
            tb_cfg.pp_units_params[0].crop_width = display_width;
            tb_cfg.pp_units_params[0].crop_height = display_height;
          } else {
            tb_cfg.pp_units_params[0].crop_width = display_width_thumb;
            tb_cfg.pp_units_params[0].crop_height = display_height_thumb;
          }
        }
      }
    }
    tb_cfg.pp_units_params[0].cr_first = cr_first;
    tb_cfg.pp_units_params[0].tiled_e = pp_tile_out;
    tb_cfg.pp_units_params[0].ystride = ystride;
    tb_cfg.pp_units_params[0].cstride = cstride;
    tb_cfg.pp_units_params[0].unit_enabled = pp_enabled;
    tb_cfg.pp_units_params[1].unit_enabled = 0;
    tb_cfg.pp_units_params[2].unit_enabled = 0;
    tb_cfg.pp_units_params[3].unit_enabled = 0;
  }
  for (i = 0; i < 4; i++) {
    dec_cfg.ppu_config[i].enabled = tb_cfg.pp_units_params[i].unit_enabled;
    dec_cfg.ppu_config[i].tiled_e = tb_cfg.pp_units_params[i].tiled_e;
    dec_cfg.ppu_config[i].cr_first = tb_cfg.pp_units_params[i].cr_first;
    dec_cfg.ppu_config[i].monochrome = tb_cfg.pp_units_params[i].monochrome;
    dec_cfg.ppu_config[i].crop.enabled = 1;
    dec_cfg.ppu_config[i].crop.x = tb_cfg.pp_units_params[i].crop_x;
    dec_cfg.ppu_config[i].crop.y = tb_cfg.pp_units_params[i].crop_y;
    dec_cfg.ppu_config[i].crop.width = tb_cfg.pp_units_params[i].crop_width;
    dec_cfg.ppu_config[i].crop.height = tb_cfg.pp_units_params[i].crop_height;
    dec_cfg.ppu_config[i].scale.enabled = 1;
    dec_cfg.ppu_config[i].scale.width = tb_cfg.pp_units_params[i].scale_width;
    dec_cfg.ppu_config[i].scale.height = tb_cfg.pp_units_params[i].scale_height;
    dec_cfg.ppu_config[i].planar = tb_cfg.pp_units_params[i].planar;
    dec_cfg.ppu_config[i].out_p010 = tb_cfg.pp_units_params[i].out_p010;
    dec_cfg.ppu_config[i].out_1010 = 0;
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

    u32 display_width = (image_info.display_width + 1) & ~0x1;
    u32 display_height = (image_info.display_height + 1) & ~0x1;
    u32 display_width_thumb = (image_info.display_width_thumb + 1) & ~0x1;
    u32 display_height_thumb = (image_info.display_height_thumb + 1) & ~0x1;

    if (!params.ppu_cfg[0].crop.set_by_user) {
      params.ppu_cfg[0].crop.width = mode ? display_width_thumb: display_width;
      params.ppu_cfg[0].crop.height = mode ? display_height_thumb: display_height;
      params.ppu_cfg[0].crop.enabled = 1;
    }
    u32 crop_w = params.ppu_cfg[0].crop.width;
    u32 crop_h = params.ppu_cfg[0].crop.height;

    params.ppu_cfg[0].scale.width = NEXT_MULTIPLE(crop_w / params.fscale_cfg.down_scale_x - 1, 2);
    params.ppu_cfg[0].scale.height = NEXT_MULTIPLE(crop_h / params.fscale_cfg.down_scale_y - 1, 2);
    params.ppu_cfg[0].scale.enabled = 1;
    params.ppu_cfg[0].enabled = 1;
    params.ppu_cfg[0].align = params.align;  //set align for pp0
    params.pp_enabled = 1;
    image_info.output_width = NEXT_MULTIPLE(params.ppu_cfg[0].scale.width, ALIGN(image_info.align));
    image_info.output_width_thumb = NEXT_MULTIPLE(params.ppu_cfg[0].scale.width, ALIGN(image_info.align));
    image_info.output_height = params.ppu_cfg[0].scale.height;
    image_info.output_height_thumb = params.ppu_cfg[0].scale.height;
    tb_cfg.pp_units_params[0].unit_enabled = 1;
  if (params.ppu_cfg[0].enabled && !params.ppu_cfg[1].enabled &&
      !params.ppu_cfg[2].enabled && !params.ppu_cfg[3].enabled && !params.ppu_cfg[4].enabled)
      params.ppu_cfg[0].shaper_enabled = 1;
  }
  pp_enabled = params.pp_enabled;
  memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
  memcpy(dec_cfg.delogo_params, params.delogo_params, sizeof(params.delogo_params));
  if (params.ppu_cfg[0].enabled == 1) {
    if (params.ppu_cfg[0].scale.width) {
      image_info.output_width = NEXT_MULTIPLE(params.ppu_cfg[0].scale.width, ALIGN(image_info.align));
      image_info.output_width_thumb = NEXT_MULTIPLE(params.ppu_cfg[0].scale.width, ALIGN(image_info.align));
    }
    if (params.ppu_cfg[0].scale.height) {
      image_info.output_height = params.ppu_cfg[0].scale.height;
      image_info.output_height_thumb = params.ppu_cfg[0].scale.height;
    }
    tb_cfg.pp_units_params[0].unit_enabled = 1;
  }
  /* TBD, there is some conflict between multi PP output and MC, temporarily disable this combination */
  if ( params.mc_enable && (params.pp_enabled == 1) && !params.ppu_cfg[0].enabled) {
    printf("\n Parameter error: MC and multi-PP output. Only PP0 output is allowed in MC! \n");
  }
#endif
  dec_cfg.dec_image_type = jpeg_in.dec_image_type;
  tmp = JpegDecSetInfo(jpeg, &dec_cfg);
  if (tmp != JPEGDEC_OK)
    goto end;
#ifdef ASIC_TRACE_SUPPORT
  /* Handle incorrect slice size for HW testing */
  if(jpeg_in.slice_mb_set > (image_info.output_height >> 4)) {
    jpeg_in.slice_mb_set = (image_info.output_height >> 4);
    printf("FIXED Decoder Slice MB Set %d\n", jpeg_in.slice_mb_set);
  }
#endif

  /* no slice mode supported in progressive || non-interleaved ==> force to full mode */
  if((jpeg_in.dec_image_type == JPEGDEC_THUMBNAIL &&
      image_info.coding_mode_thumb == JPEGDEC_PROGRESSIVE) ||
      (jpeg_in.dec_image_type == JPEGDEC_IMAGE &&
       image_info.coding_mode == JPEGDEC_PROGRESSIVE))
    jpeg_in.slice_mb_set = 0;

  /******** PHASE 4 ********/
  /* Image mode to decode */
  if(mode)
    fprintf(stdout, "\nPhase 4: DECODE FRAME: THUMBNAIL\n");
  else
    fprintf(stdout, "\nPhase 4: DECODE FRAME: FULL RESOLUTION\n");

  /* if input (only full, not tn) > 4096 MCU      */
  /* ==> force to slice mode                                      */
  if(mode == 0 && !params.mc_enable) {
#ifdef SLICE_MODE_LARGE_PIC
#ifdef ASIC_TRACE_SUPPORT
    /* calculate MCU's */
    if(image_info.output_format == JPEGDEC_YCbCr400 ||
        image_info.output_format == JPEGDEC_YCbCr444_SEMIPLANAR) {
      amount_of_mcus =
        ((image_info.output_width * image_info.output_height) / 64);
      mcu_in_row = (image_info.output_width / 8);
    } else if(image_info.output_format == JPEGDEC_YCbCr420_SEMIPLANAR) {
      /* 265 is the amount of luma samples in MB for 4:2:0 */
      amount_of_mcus =
        ((image_info.output_width * image_info.output_height) / 256);
      mcu_in_row = (image_info.output_width / 16);
    } else if(image_info.output_format == JPEGDEC_YCbCr422_SEMIPLANAR) {
      /* 128 is the amount of luma samples in MB for 4:2:2 */
      amount_of_mcus =
        ((image_info.output_width * image_info.output_height) / 128);
      mcu_in_row = (image_info.output_width / 16);
    } else if(image_info.output_format == JPEGDEC_YCbCr440) {
      /* 128 is the amount of luma samples in MB for 4:4:0 */
      amount_of_mcus =
        ((image_info.output_width * image_info.output_height) / 128);
      mcu_in_row = (image_info.output_width / 8);
    } else if(image_info.output_format == JPEGDEC_YCbCr411_SEMIPLANAR) {
      amount_of_mcus =
        ((image_info.output_width * image_info.output_height) / 256);
      mcu_in_row = (image_info.output_width / 32);
    }

    /* set mcu_size_divider for slice size count */
    if(image_info.output_format == JPEGDEC_YCbCr400 ||
        image_info.output_format == JPEGDEC_YCbCr440 ||
        image_info.output_format == JPEGDEC_YCbCr444_SEMIPLANAR)
      mcu_size_divider = 2;
    else
      mcu_size_divider = 1;

    if(is8170_hw) {
      /* over max MCU ==> force to slice mode */
      if((jpeg_in.slice_mb_set == 0) &&
          (amount_of_mcus > JPEGDEC_MAX_SLICE_SIZE)) {
        do {
          jpeg_in.slice_mb_set++;
        } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                 (mcu_in_row / mcu_size_divider)) <
                JPEGDEC_MAX_SLICE_SIZE);
        printf("Force to slice mode ==> Decoder Slice MB Set %d\n",
               jpeg_in.slice_mb_set);
      }
    } else {
      /* 8190 and over 16M ==> force to slice mode */
      if((jpeg_in.slice_mb_set == 0) &&
          ((image_info.output_width * image_info.output_height) >
           (((JpegDecContainer *) jpeg)->hw_feature.img_max_dec_width *
             ((JpegDecContainer *) jpeg)->hw_feature.img_max_dec_height))) {
        do {
          jpeg_in.slice_mb_set++;
        } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                 (mcu_in_row / mcu_size_divider)) <
                JPEGDEC_MAX_SLICE_SIZE_8190);
        printf
        ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",
         jpeg_in.slice_mb_set);
      }
    }
#else
    if(is8170_hw) {
      /* over max MCU ==> force to slice mode */
      if((jpeg_in.slice_mb_set == 0) &&
          (amount_of_mcus > JPEGDEC_MAX_SLICE_SIZE)) {
        do {
          jpeg_in.slice_mb_set++;
        } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                 (mcu_in_row / mcu_size_divider)) <
                JPEGDEC_MAX_SLICE_SIZE);
        printf("Force to slice mode ==> Decoder Slice MB Set %d\n",
               jpeg_in.slice_mb_set);
      }
    } else {
      /* 8190 and over 16M ==> force to slice mode */
      if((jpeg_in.slice_mb_set == 0) &&
          ((image_info.output_width * image_info.output_height) >
           (((JpegDecContainer *) jpeg)->hw_feature.img_max_dec_width *
             ((JpegDecContainer *) jpeg)->hw_feature.img_max_dec_height))) {
        do {
          jpeg_in.slice_mb_set++;
        } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                 (mcu_in_row / mcu_size_divider)) <
                JPEGDEC_MAX_SLICE_SIZE_8190);
        printf
        ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",
         jpeg_in.slice_mb_set);
      }
    }
#endif
#endif
  }

allocate_buffer:
  rv = JpegDecGetBufferInfo(jpeg, &hbuf);
  if(rv != JPEGDEC_WAITING_FOR_BUFFER && rv != JPEGDEC_OK) {
    /* Handle here the error situation */
    PrintJpegRet(rv);
    goto error;
  }
  printf("JpegDecGetBufferInfo ret %d\n", rv);
  printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
         (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);

  /* if Instant Buffer mode & !mc*/
  if((params.instant_buffer && !params.mc_enable && !add_external_buffer) ||
     (rv == JPEGDEC_WAITING_FOR_BUFFER && hbuf.buf_to_free.virtual_address != NULL && !params.mc_enable)) {
    fprintf(stdout, "\n\t-JPEG: USER ALLOCATED MEMORY\n");
    if (rv == JPEGDEC_WAITING_FOR_BUFFER && hbuf.buf_to_free.virtual_address != NULL) {
      if(user_alloc_output_buffer[0].virtual_address != NULL)
        DWLFreeRefFrm(dwl, &user_alloc_output_buffer[0]);
    }
    jpeg_ret = allocMemory(jpeg, &image_info, &jpeg_in, dwl, hbuf.next_buf_size);
    if(jpeg_ret != JPEGDEC_OK) {
      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      goto end;
    }
    add_external_buffer = 1;
    fprintf(stdout, "\t-JPEG: USER ALLOCATED MEMORY successful\n\n");
  } else if(!params.instant_buffer && !params.mc_enable) {
    struct DWLLinearMem mem;

    if (hbuf.buf_to_free.virtual_address != NULL) {
      if(ext_buffers[0].virtual_address != NULL) {
        ASSERT(ext_buffers[0].virtual_address == hbuf.buf_to_free.virtual_address);
        DWLFreeLinear(dwl, &ext_buffers[0]);
      }
      add_external_buffer = 0;  //reset the flag to reallocate buffer
    }

    if(hbuf.next_buf_size && !add_external_buffer) {
      /* Only add minimum required buffers at first. */
      for(i = 0; i < hbuf.buf_num; i++) {
        DWLMallocLinear(dwl, hbuf.next_buf_size, &mem);
        memset(mem.virtual_address, 0, mem.size);
        rv = JpegDecAddBuffer(jpeg, &mem);

        printf("JpegDecAddBuffer ret %d\n", rv);
        if(rv != JPEGDEC_OK) {
          DWLFreeLinear(dwl, &mem);
        } else {
          ext_buffers[i] = mem;
        }
      }
      /* Extra buffers are allowed when minimum required buffers have been added.*/
      add_external_buffer = 1;
    }
  }

  if(params.mc_enable) {
    if (params.instant_buffer) {
      fprintf(stdout, "\n\t-JPEG: USER ALLOCATED MEMORY\n");
      jpeg_ret = inputMemoryAlloc(jpeg, &image_info, &jpeg_in, dwl, &hbuf);
      if(jpeg_ret != JPEGDEC_OK) {
        /* Handle here the error situation */
        PrintJpegRet(jpeg_ret);
        goto end;
      }
      fprintf(stdout, "\t-JPEG: USER ALLOCATED MEMORY successful\n\n");
    } else {
      struct DWLLinearMem mem;

      pthread_mutex_lock(&ext_buffer_contro);

      if (hbuf.buf_to_free.virtual_address != NULL) {
        for (i = 0; i < output_buffers; i++) {
          if(ext_buffers[i].virtual_address == hbuf.buf_to_free.virtual_address) {
            DWLFreeLinear(dwl, &ext_buffers[i]);
            break;
          }
        }
        ASSERT(i < output_buffers);
        DWLMallocLinear(dwl, hbuf.next_buf_size, &mem);
        memset(mem.virtual_address, 0, mem.size);
        rv = JpegDecAddBuffer(jpeg, &mem);

        printf("JpegDecAddBuffer ret %d\n", rv);
        if(rv != JPEGDEC_OK) {
          DWLFreeLinear(dwl, &mem);
        } else {
          ext_buffers[i] = mem;
        }
      }

      if(hbuf.next_buf_size && !add_external_buffer) {
        for(i = 0; i < output_buffers; i++) {
          DWLMallocLinear(dwl, hbuf.next_buf_size, &mem);
          memset(mem.virtual_address, 0, mem.size);
          rv = JpegDecAddBuffer(jpeg, &mem);

          printf("JpegDecAddBuffer ret %d\n", rv);
          if(rv != JPEGDEC_OK) {
            DWLFreeLinear(dwl, &mem);
          } else {
            ext_buffers[i] = mem;
          }
        }
        add_external_buffer = 1;
      }
      pthread_mutex_unlock(&ext_buffer_contro);
    }
  }
  /*  Now corrupt only data beyong image info */
  if(stream_bit_swap) {
    if (!params.mc_enable) {
      jpeg_ret =
        TBRandomizeBitSwapInStream(byte_strm_start + image_info_length,
                                   len - image_info_length,
                                   tb_cfg.tb_params.stream_bit_swap);
      if(jpeg_ret != 0) {
        printf("RANDOM STREAM ERROR FAILED\n");
        goto end;
      }
    } else {
      jpeg_ret =
        TBRandomizeBitSwapInStream(((u8*)(jpeg_in.stream_buffer.virtual_address)) + image_info_length,
                                   frame_len - image_info_length,
                                   tb_cfg.tb_params.stream_bit_swap);
      if(jpeg_ret != 0) {
        printf("RANDOM STREAM ERROR FAILED\n");
        goto end;
      }
    }
  }

  if (ri_mc_enable) {
    ri_array = (u32 *)DWLmalloc(RI_MAX_NUM * sizeof(u32));
    ri_count = FindImageAllRI((u8 *)jpeg_in.stream_buffer.virtual_address,
                              jpeg_in.stream_length,
                              ri_array, RI_MAX_NUM);
    if (ri_count > RI_MAX_NUM) {
      /* If buffer not enough, reallocate and re-parse. */
      DWLfree(ri_array);
      ri_array = (u32 *)DWLmalloc(ri_count * sizeof(u32));
      ri_count = FindImageAllRI((u8 *)jpeg_in.stream_buffer.virtual_address,
                                jpeg_in.stream_length,
                                ri_array, ri_count);
    }
  }
  jpeg_in.ri_count = ri_count;
  jpeg_in.ri_array = ri_array;

  /* decode */
  do {
    START_SW_PERFORMANCE;
    decsw_performance();
    jpeg_ret = JpegDecDecode(jpeg, &jpeg_in, &jpeg_out);
    END_SW_PERFORMANCE;
    decsw_performance();

    /* release curr stream buffer */
    if (params.mc_enable && jpeg_ret != JPEGDEC_FRAME_READY) {
      StreamBufferConsumed((u8 *)jpeg_in.stream_buffer.virtual_address,
              jpeg_in.stream_buffer.virtual_address);
    }

    if(jpeg_ret == JPEGDEC_FRAME_READY) {
      u32 output_width, output_height, display_width, display_height;
      fprintf(stdout, "\t-JPEG: JPEGDEC_FRAME_READY\n");
      if(!output_thread_run) {
        output_thread_run = 1;
        pthread_create(&output_thread, NULL, jpeg_output_thread, NULL);
      }

      if (!mode) {
        output_width = jpeg_out.pictures[0].output_width;
        output_height = jpeg_out.pictures[0].output_height;
        display_width = jpeg_out.pictures[0].display_width;
        display_height = jpeg_out.pictures[0].display_height;
      }
      else {
        output_width = jpeg_out.pictures[0].output_width_thumb;
        output_height = jpeg_out.pictures[0].output_height_thumb;
        display_width = jpeg_out.pictures[0].display_width_thumb;
        display_height = jpeg_out.pictures[0].display_height_thumb;
      }
      fprintf(stdout, "\t-JPEG: The decoded image information\n");
      fprintf(stdout, "\t-JPEG: image output width(stride): %d\n", output_width);
      fprintf(stdout, "\t-JPEG: image output height:        %d\n", output_height);
      fprintf(stdout, "\t-JPEG: image decoded(HW) width: %d\n", NEXT_MULTIPLE(display_width, 16));
      fprintf(stdout, "\t-JPEG: image decoded(HW) height: %d\n", NEXT_MULTIPLE(display_height, 8));
      fprintf(stdout, "\t-JPEG: image DISPLAY width:  %d\n", display_width);
      fprintf(stdout, "\t-JPEG: image DISPLAY height: %d\n", display_height);

      /* check if progressive ==> planar output */
      if((image_info.coding_mode == JPEGDEC_PROGRESSIVE && mode == 0) ||
          (image_info.coding_mode_thumb == JPEGDEC_PROGRESSIVE &&
           mode == 1)) {
        progressive = 1;
      }

      if((image_info.coding_mode == JPEGDEC_NONINTERLEAVED && mode == 0)
          || (image_info.coding_mode_thumb == JPEGDEC_NONINTERLEAVED &&
              mode == 1))
        non_interleaved = 1;
      else
        non_interleaved = 0;

      if(jpeg_in.slice_mb_set && full_slice_counter == -1)
        sliced_output_used = 1;

      /* info to handleSlicedOutput */
      frame_ready = 1;
      if(!mode)
        nbr_of_images_to_out++;

      /* for input buffering */
      prev_ret = JPEGDEC_FRAME_READY;
#ifdef ASIC_TRACE_SUPPORT
      pic_number++;
#endif
    } else if(jpeg_ret == JPEGDEC_SCAN_PROCESSED) {
      /* TODO! Progressive scan ready... */
      fprintf(stdout, "\t-JPEG: JPEGDEC_SCAN_PROCESSED\n");
      if(!output_thread_run) {
        output_thread_run = 1;
        pthread_create(&output_thread, NULL, jpeg_output_thread, NULL);
      }

      /* progressive ==> planar output */
      if(image_info.coding_mode == JPEGDEC_PROGRESSIVE)
        progressive = 1;

      /* info to handleSlicedOutput */
      printf("SCAN %d READY\n", scan_counter);

      if(image_info.coding_mode == JPEGDEC_PROGRESSIVE) {
        /* calculate size for output */
        calcSize(&image_info, mode, &jpeg_out, 0);

        printf("size_luma %d and size_chroma %d\n", size_luma,
               size_chroma);
#if 0
        WriteProgressiveOutput(size_luma, size_chroma, mode,
                               (u8*)jpeg_out.pictures[0].output_picture_y.
                               virtual_address,
                               (u8*)jpeg_out.pictures[0].output_picture_cb_cr.
                               virtual_address,
                               (u8*)jpeg_out.pictures[0].output_picture_cr.
                               virtual_address);
#endif

        scan_counter++;
      }

      /* update/reset */
      progressive = 0;
      scan_ready = 0;

    } else if(jpeg_ret == JPEGDEC_SLICE_READY) {
      fprintf(stdout, "\t-JPEG: JPEGDEC_SLICE_READY\n");

      sliced_output_used = 1;

      /* calculate/write output of slice
       * and update output budder in case of
       * user allocated memory */
      if(jpeg_out.pictures[0].output_picture_y.virtual_address != NULL)
        handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);

      scan_counter++;
    } else if(jpeg_ret == JPEGDEC_STRM_PROCESSED) {
      fprintf(stdout,
              "\t-JPEG: JPEGDEC_STRM_PROCESSED ==> Load input buffer\n");

      /* update seek value */
      stream_in_file -= len;
      stream_seek_len += len;

      if(stream_in_file < 0) {
        fprintf(stdout, "\t\t==> Unable to load input buffer\n");
        fprintf(stdout,
                "\t\t\t==> TRUNCATED INPUT ==> JPEGDEC_STRM_ERROR\n");
        jpeg_ret = JPEGDEC_STRM_ERROR;
        goto strm_error;
      }

      if(stream_in_file < len) {
        len = stream_in_file;
      }

      /* update the buffer size in case last buffer
         doesn't have the same amount of data as defined */
      if(len < jpeg_in.buffer_size) {
        jpeg_in.buffer_size = len;
      }

      /* Reading input file */
      f_in = fopen(params.in_file_name, "rb");
      if(f_in == NULL) {
        fprintf(stdout, "Unable to open input file\n");
        exit(-1);
      }

      /* file i/o pointer to full */
      fseek(f_in, stream_seek_len, SEEK_SET);
      /* read input stream from file to buffer and close input file */
      ret = fread(byte_strm_start, sizeof(u8), len, f_in);
      (void) ret;
      fclose(f_in);

      /* update */
      jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address;

      if(stream_bit_swap) {
        jpeg_ret =
          TBRandomizeBitSwapInStream(byte_strm_start, len,
                                     tb_cfg.tb_params.
                                     stream_bit_swap);
        if(jpeg_ret != 0) {
          printf("RANDOM STREAM ERROR FAILED\n");
          goto end;
        }
      }
    } else if(jpeg_ret == JPEGDEC_NO_DECODING_BUFFER) {
      PrintJpegRet(jpeg_ret);
      usleep(10000);
    } else if(jpeg_ret == JPEGDEC_WAITING_FOR_BUFFER) {
      PrintJpegRet(jpeg_ret);
      goto allocate_buffer;
    } else if(jpeg_ret == JPEGDEC_STRM_ERROR) {
strm_error:

      if(jpeg_in.slice_mb_set && full_slice_counter == -1)
        sliced_output_used = 1;

      if(!output_thread_run && !params.instant_buffer) {
        output_thread_run = 1;
        pthread_create(&output_thread, NULL, jpeg_output_thread, NULL);
      }

      /* calculate/write output of slice
       * and update output budder in case of
       * user allocated memory */
      if(sliced_output_used &&
          jpeg_out.pictures[0].output_picture_y.virtual_address != NULL)
        handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);

      /* info to handleSlicedOutput */
      frame_ready = 1;
      sliced_output_used = 0;

      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      if(mode == 1)
        break;
      else
        goto error;
    } else {
      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      if(mode == 1)
        break;
      else
        goto error;;
    }
  } while(jpeg_ret != JPEGDEC_FRAME_READY);

error:

  /* release curr frame buffer */
  if (params.mc_enable && jpeg_ret != JPEGDEC_FRAME_READY) {
    for (j = 0; j < output_buffers; j++) {
      if(jpeg_in.picture_buffer_y.virtual_address ==
          user_alloc_output_buffer[j].virtual_address) {
        pthread_mutex_lock(&frm_buff_stat_lock);
        frame_mem_status[j] = 0;
        pthread_mutex_unlock(&frm_buff_stat_lock);
        sem_post(&frame_buff_free);
        break;
      }
    }
  }

  /* calculate/write output of slice */
  if(sliced_output_used && jpeg_out.pictures[0].output_picture_y.virtual_address != NULL) {
    handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);
    sliced_output_used = 0;
  }

  if(jpeg_out.pictures[0].output_picture_y.virtual_address != NULL && !params.mc_enable) {
    /* calculate size for output */
    calcSize(&image_info, mode, &jpeg_out, 0);

    /* Thumbnail || full resolution */
    if(!mode)
      fprintf(stdout, "\n\t-JPEG: ++++++++++ FULL RESOLUTION ++++++++++\n");
    else
      fprintf(stdout, "\t-JPEG: ++++++++++ THUMBNAIL ++++++++++\n");
    fprintf(stdout, "\t-JPEG: Instance %p\n", (void *) jpeg);
    fprintf(stdout, "\t-JPEG: Luma output: %p size: %d\n",
            (void *)jpeg_out.pictures[0].output_picture_y.virtual_address, size_luma);
    fprintf(stdout, "\t-JPEG: Chroma output: %p size: %d\n",
            (void *)jpeg_out.pictures[0].output_picture_cb_cr.virtual_address, size_chroma);
    fprintf(stdout, "\t-JPEG: Luma output bus: 0x%16zx\n",
            jpeg_out.pictures[0].output_picture_y.bus_address);
    fprintf(stdout, "\t-JPEG: Chroma output bus: 0x%16zx\n",
            jpeg_out.pictures[0].output_picture_cb_cr.bus_address);
  }

  fprintf(stdout, "PHASE 4: DECODE FRAME successful\n");

  /* if output write not disabled by TB */
  if(write_output && params.instant_buffer && !params.mc_enable) {
    /******** PHASE 5 ********/
    fprintf(stdout, "\nPhase 5: WRITE OUTPUT\n");

#ifndef PP_PIPELINE_ENABLED
    if(image_info.output_format) {
      switch (image_info.output_format) {
      case JPEGDEC_YCbCr400:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr400\n");
        break;
      case JPEGDEC_YCbCr420_SEMIPLANAR:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr420_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr422_SEMIPLANAR:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr422_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr440:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr440\n");
        break;
      case JPEGDEC_YCbCr411_SEMIPLANAR:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr444_SEMIPLANAR:
        fprintf(stdout, "\t-JPEG: DECODER OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n");
        break;
      }
    }

    if(image_info.coding_mode == JPEGDEC_PROGRESSIVE)
      progressive = 1;

    /* write output */
    if(jpeg_in.slice_mb_set) {
      if(image_info.output_format != JPEGDEC_YCbCr400)
        WriteFullOutput(mode);
    } else {
      if(image_info.coding_mode != JPEGDEC_PROGRESSIVE) {
        for (j = 0; j < DEC_MAX_OUT_COUNT; j++) {
          if (!params.ppu_cfg[j].enabled)
            continue;
          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(jpeg_out.pictures[j].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(jpeg_out.pictures[j].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(jpeg_out.pictures[j].output_format))
            pp_mono_chrome = 1;
          if (!jpeg_in.dec_image_type) {
#ifndef SUPPORT_DEC400
            WriteOutput(out_file_name[j],
                       ((u8 *) jpeg_out.pictures[j].output_picture_y.virtual_address),
                       jpeg_out.pictures[j].display_width,
                       jpeg_out.pictures[j].display_height,
                       0, pp_mono_chrome, 0, 0, 0,
                       IS_PIC_TILE(jpeg_out.pictures[j].output_format),
                       jpeg_out.pictures[j].pic_stride, jpeg_out.pictures[j].pic_stride_ch, j,
                       pp_planar, pp_cr_first, 0,
                       0, 0, md5sum, &fout[j], 1);
#else
            WriteOutputdec400(out_file_name[j],
                       ((u8 *) jpeg_out.pictures[j].output_picture_y.virtual_address),
                       jpeg_out.pictures[j].display_width,
                       jpeg_out.pictures[j].display_height,
                       0, pp_mono_chrome, 0, 0, 0,
                       IS_PIC_TILE(jpeg_out.pictures[j].output_format),
                       jpeg_out.pictures[j].pic_stride, jpeg_out.pictures[j].pic_stride_ch, j,
                       pp_planar, pp_cr_first, 0,
                       0, 0, md5sum, &fout[j], 1, &fout_table[i],
                       (u8 *) jpeg_out.pictures[j].luma_table.virtual_address,
                        jpeg_out.pictures[j].luma_table.size,
                        (u8 *) jpeg_out.pictures[j].chroma_table.virtual_address,
                        jpeg_out.pictures[j].chroma_table.size);
#endif
          } else {
#ifndef SUPPORT_DEC400
            WriteOutput(out_file_tn_name[j],
                      ((u8 *) jpeg_out.pictures[j].output_picture_y.virtual_address),
                      jpeg_out.pictures[j].display_width_thumb,
                      jpeg_out.pictures[j].display_height_thumb,
                      0, pp_mono_chrome, 0, 0, 0,
                      IS_PIC_TILE(jpeg_out.pictures[j].output_format),
                      jpeg_out.pictures[j].pic_stride, jpeg_out.pictures[j].pic_stride_ch, j,
                      pp_planar, pp_cr_first, 0,
                      0, 0, md5sum, &fout_tn[j], 1);
#else
           WriteOutputdec400(out_file_tn_name[j],
                      ((u8 *) jpeg_out.pictures[j].output_picture_y.virtual_address),
                      jpeg_out.pictures[j].display_width_thumb,
                      jpeg_out.pictures[j].display_height_thumb,
                      0, pp_mono_chrome, 0, 0, 0,
                      IS_PIC_TILE(jpeg_out.pictures[j].output_format),
                      jpeg_out.pictures[j].pic_stride, jpeg_out.pictures[j].pic_stride_ch, j,
                      pp_planar, pp_cr_first, 0,
                      0, 0, md5sum, &fout_tn[j], 1, &fout_table[i],
                       (u8 *) jpeg_out.pictures[j].luma_table.virtual_address,
                        jpeg_out.pictures[j].luma_table.size,
                        (u8 *) jpeg_out.pictures[j].chroma_table.virtual_address,
                        jpeg_out.pictures[j].chroma_table.size);
#endif

          }
        }
      } else {
        /* calculate size for output */
        calcSize(&image_info, mode, &jpeg_out, 0);

        printf("size_luma %d and size_chroma %d\n", size_luma, size_chroma);

        WriteProgressiveOutput(size_luma, size_chroma, mode,
                               (u8*)jpeg_out.pictures[0].output_picture_y.virtual_address,
                               (u8*)jpeg_out.pictures[0].output_picture_cb_cr.
                               virtual_address,
                               (u8*)jpeg_out.pictures[0].output_picture_cr.virtual_address);
      }

    }

    if(crop)
      WriteCroppedOutput(&image_info,
                         (u8*)jpeg_out.pictures[0].output_picture_y.virtual_address,
                         (u8*)jpeg_out.pictures[0].output_picture_cb_cr.virtual_address,
                         (u8*)jpeg_out.pictures[0].output_picture_cr.virtual_address);

    progressive = 0;
#else
    /* PP test bench will do the operations only if enabled */
    /*pp_set_rotation(); */

    fprintf(stdout, "\t-JPEG: PP_OUTPUT_WRITE\n");
    pp_write_output(0, 0, 0);
    pp_check_combined_status();
#endif
    fprintf(stdout, "PHASE 5: WRITE OUTPUT successful from user buffer\n");
  } else {
    fprintf(stdout, "\nPhase 5: WRITE OUTPUT DISABLED from user buffer\n");
  }

  /* more images to decode? */
  if(nbr_of_images || (nbr_of_thumb_images && !only_full_resolution)) {
    pic_counter++;

    if(mode) {
      nbr_of_thumb_images--;
      nbr_of_thumb_images_to_out++;
      if(nbr_of_thumb_images == 0 ||
         (params.mc_enable && (nbr_of_thumb_images+1)%nbr_of_images==0)) {
        /* set */
        ThumbDone = 1;
        pic_counter = 0;
        stream_seek_len = 0;
        stream_in_file = 0;
        goto end;
      }
    } else {
      nbr_of_images--;
      if(nbr_of_images == 0) {
        /* set */
        pic_counter = 0;
        stream_seek_len = 0;
        stream_in_file = 0;
        goto end;
      }
    }

    /* if input buffered load */
    if(jpeg_in.buffer_size) {
      u32 counter = 0;
      /* seek until next pic start */
      do {
        /* Seek next EOI */
        jpeg_ret = FindImageTnEOI(byte_strm_start, len, &image_info_length, mode, thumb_in_stream);

        /* check result */
        if(jpeg_ret == JPEGDEC_OK && next_soi)
          break;
        else {
          jpeg_ret = -1;
          counter++;
        }

        /* update seek value */
        stream_in_file -= len;
        stream_seek_len += len;

        if(stream_in_file <= 0) {
          fprintf(stdout, "\t\t==> Unable to load input buffer\n");
          fprintf(stdout,
                  "\t\t\t==> TRUNCATED INPUT ==> JPEGDEC_STRM_ERROR\n");
          jpeg_ret = JPEGDEC_STRM_ERROR;
          goto end;
        }

        if(stream_in_file < len)
          len = stream_in_file;

        /* Reading input file */
        f_in = fopen(params.in_file_name, "rb");
        if(f_in == NULL) {
          fprintf(stdout, "Unable to open input file\n");
          exit(-1);
        }

        /* file i/o pointer to full */
        fseek(f_in, stream_seek_len, SEEK_SET);
        /* read input stream from file to buffer and close input file */
        ret = fread(byte_strm_start, sizeof(u8), len, f_in);
        (void) ret;
        fclose(f_in);
      } while(jpeg_ret != 0);
    } else {
      /* Find next image */
      if (!params.mc_enable)
        jpeg_ret = FindImageTnEOI(byte_strm_start, len, &image_info_length, mode, thumb_in_stream);
    }
    /* If image info is not found */
    if(jpeg_ret != 0 && !params.mc_enable) {
      printf("NO MORE IMAGES!\n");
      goto end;
    }

    /* update seek value */
    if (!params.mc_enable) {
      stream_in_file -= image_info_length;
      stream_seek_len += image_info_length;

      if(stream_in_file <= 0) {
        fprintf(stdout, "\t\t==> Unable to load input buffer\n");
        fprintf(stdout,
                "\t\t\t==> TRUNCATED INPUT ==> JPEGDEC_STRM_ERROR\n");
        jpeg_ret = JPEGDEC_STRM_ERROR;
        goto strm_error;
      }

      if(stream_in_file < len) {
        len = stream_in_file;
      }

      /* update the buffer size in case last buffer
         doesn't have the same amount of data as defined */
      if(len < jpeg_in.buffer_size) {
        jpeg_in.buffer_size = len;
      }

#if 0
      /* Reading input file */
      f_in = fopen(argv[argc - 1], "rb");
      if(f_in == NULL) {
        fprintf(stdout, "Unable to open input file\n");
        exit(-1);
      }

      /* file i/o pointer to full */
      fseek(f_in, stream_seek_len, SEEK_SET);
      /* read input stream from file to buffer and close input file */
      ret = fread(byte_strm_start, sizeof(u8), len, f_in);
      (void) ret;
      fclose(f_in);

      /* update */
      jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address;
      jpeg_in.stream_length = stream_in_file;
#else
      /* Don't need to read a new picture from file again and again.
         Just use the buffer with whole stream in it. */
      byte_strm_start += image_info_length;

      /* update */
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address + (addr_t)byte_strm_start -
                                   (addr_t)stream_mem.virtual_address;
      jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
      jpeg_in.stream_length = stream_in_file;
#endif

      /* loop back to start */
    } else {
      if (frame_len_index >= PERFETCH_EOI_NUM) {
        jpeg_ret = FindImageEOI(byte_strm_start + temp_len, len - temp_len, &frame_len);
        if(jpeg_ret != 0) {
          printf("NO MORE IMAGES!\n");
          goto end;
        }
      } else {
        frame_len = frame_len_prefetch[frame_len_index++];
      }
      {
        int id = GetFreeStreamBuffer();

        jpeg_in.stream_buffer.virtual_address = (u32 *) stream_mem_input[id].virtual_address;
        jpeg_in.stream_buffer.bus_address = (addr_t) stream_mem_input[id].bus_address;

        /* stream processed callback param */
        jpeg_in.p_user_data = stream_mem_input[id].virtual_address;
      }
      jpeg_in.stream_length = frame_len;
      DWLmemcpy(jpeg_in.stream_buffer.virtual_address, byte_strm_start + temp_len, frame_len);
      temp_len += frame_len;
    }
    goto decode;
  }

end:

  /******** PHASE 6 ********/
  fprintf(stdout, "\nPhase 6: RELEASE JPEG DECODER\n");

  /* reset output write option */
  progressive = 0;
  JpegDecEndOfStream(jpeg);
#ifdef PP_PIPELINE_ENABLED
  pp_close();
#endif

  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    output_thread_run = 0;
  }
  /* have to release stream buffers before releasing decoder as we need DWL */
  sem_destroy(&stream_buff_free);
  sem_destroy(&frame_buff_free);
  for(i = 0; i < allocated_buffers; i++) {
    if(stream_mem_input[i].virtual_address != NULL) {
      DWLFreeLinear(dwl, &stream_mem_input[i]);
    }
  }

  if (ri_array)
    DWLfree(ri_array);

  if(stream_mem.virtual_address != NULL)
    DWLFreeLinear(dwl, &stream_mem);

  if (!params.mc_enable) {
    if (params.instant_buffer) {
      if(user_alloc_output_buffer[0].virtual_address != NULL)
        DWLFreeRefFrm(dwl, &user_alloc_output_buffer[0]);
    } else {
      if(ext_buffers[0].virtual_address != NULL)
          DWLFreeLinear(dwl, &ext_buffers[0]);
    }
  } else {
    if(params.instant_buffer) {
      for (i = 0; i < output_buffers; i++) {
        if(user_alloc_output_buffer[i].virtual_address != NULL)
          DWLFreeLinear(dwl, &user_alloc_output_buffer[i]);
        user_alloc_output_buffer[i].virtual_address = NULL;
      }
    } else {
      pthread_mutex_lock(&ext_buffer_contro);
      for(i=0; i < output_buffers; i++) {
        if(ext_buffers[i].virtual_address != NULL)
          DWLFreeLinear(dwl, &ext_buffers[i]);
        DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
      }
      pthread_mutex_unlock(&ext_buffer_contro);
    }
  }
  /* release decoder instance */
  START_SW_PERFORMANCE;
  decsw_performance();
  JpegDecRelease(jpeg);
  END_SW_PERFORMANCE;
  decsw_performance();

  #ifdef ASIC_ONL_SIM
       dec_done = 1;
//  #else
//     sem_post(dec_done);
  #endif

  fprintf(stdout, "PHASE 6: RELEASE JPEG DECODER successful\n\n");

  /* check if (thumbnail + full) ==> decode all full images */
  if(ThumbDone && nbr_of_images) {
    prev_ret = JPEGDEC_STRM_ERROR;
    goto start_full_decode;
  }

  if(input_read_type) {
    if(f_in) {
      fclose(f_in);
    }
  }

  for (i =0; i < DEC_MAX_PPU_COUNT; i++) {
    if(fout[i])
      fclose(fout[i]);
    if(fout_tn[i])
      fclose(fout_tn[i]);
  }
  if( f_stream_trace)
    fclose(f_stream_trace);

  #ifdef ASIC_ONL_SIM
  while(!dec_done) {
    usleep(100);
  }
//  #else
//  sem_wait(dec_done);
  #endif

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  /* Leave properly */
  JpegDecFree(p_image);

  pthread_mutex_destroy(&ext_buffer_contro);

  DWLRelease(dwl);

  FINALIZE_SW_PERFORMANCE;

  /* amounf of decoded frames */
  if(nbr_of_images_to_out) {
    fprintf(stdout, "Information of decoded pictures:\n");
    fprintf(stdout, "\tPictures decoded: \t%d of total %d\n", nbr_of_images_to_out, nbr_of_images_total);
    if(nbr_of_thumb_images_to_out)
      fprintf(stdout, "\tThumbnails decoded: \t%d of total %d\n", nbr_of_thumb_images_to_out, nbr_of_thumb_images_total);

    if( (nbr_of_images_to_out != nbr_of_images_total) ||
        (nbr_of_thumb_images_to_out != nbr_of_thumb_images_total)) {
      fprintf(stdout, "\n\t-NOTE! \tCheck decoding log for the reason of \n");
      fprintf(stdout, "\t\tnot decoded, failed or unsupported pictures!\n");
      /* only full resolution */
      if(only_full_resolution)
        fprintf(stdout,"\n\t-NOTE! Forced by user to decode only full resolution image!\n");
    }

    fprintf(stdout, "\n");
  }

  fprintf(stdout, "TB: ...released\n");

  return_status = 0;
return_:
#ifdef __FREERTOS__
  (void)return_status;
  /* so should be commented if in real environment Embedded OS FreeRTOS */
  vTaskEndScheduler();
#else
  return return_status;
#endif
}

/*------------------------------------------------------------------------------

    Function name:  WriteOutputLuma

    Purpose:
        Write picture pointed by data to file. Size of the
        picture in pixels is indicated by picSize.

------------------------------------------------------------------------------*/
void WriteOutputLuma(u8 * data_luma, u32 pic_size_luma, u32 pic_mode) {
  u32 i;
  FILE *foutput = NULL;
  u8 *p_yuv_out = NULL;

  /* foutput is global file pointer */
  if(foutput == NULL) {
    if(pic_mode == 0) {
      foutput = fopen("out.yuv", "ab");
    } else {
      foutput = fopen("out_tn.yuv", "ab");
    }

    if(foutput == NULL) {
      fprintf(stdout, "UNABLE TO OPEN OUTPUT FILE\n");
      return;
    }
  }

  if(foutput && data_luma) {
    if(1) {
      fprintf(stdout, "\t-JPEG: Luminance\n");
      /* write decoder output to file */
      p_yuv_out = data_luma;
      for(i = 0; i < (pic_size_luma >> 2); i++) {
#ifndef ASIC_TRACE_SUPPORT
        if(DEC_X170_BIG_ENDIAN == output_picture_endian) {
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput);
        } else {
#endif
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput);
#ifndef ASIC_TRACE_SUPPORT
        }
#endif
      }
    }
  }

  fclose(foutput);
}

/*------------------------------------------------------------------------------

    Function name:  WriteOutput

    Purpose:
        Write picture pointed by data to file. Size of the
        picture in pixels is indicated by picSize.

------------------------------------------------------------------------------*/
void WriteOutputChroma(u8 * data_chroma, u32 pic_size_chroma, u32 pic_mode) {
  u32 i;
  FILE *foutput_chroma = NULL;
  u8 *p_yuv_out = NULL;

  /* file pointer */
  if(foutput_chroma == NULL) {
    if(pic_mode == 0) {
      if(!progressive) {
        if(full_slice_counter == 0)
          foutput_chroma = fopen("out_chroma.yuv", "wb");
        else
          foutput_chroma = fopen("out_chroma.yuv", "ab");
      } else {
        if(!sliced_output_used) {
          foutput_chroma = fopen("out_chroma.yuv", "wb");
        } else {
          if(scan_counter == 0 || full_slice_counter == 0)
            foutput_chroma = fopen("out_chroma.yuv", "wb");
          else
            foutput_chroma = fopen("out_chroma.yuv", "ab");
        }
      }
    } else {
      if(!progressive) {
        if(full_slice_counter == 0)
          foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
        else
          foutput_chroma = fopen("out_chroma_tn.yuv", "ab");
      } else {
        if(!sliced_output_used) {
          foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
        } else {
          if(scan_counter == 0 || full_slice_counter == 0)
            foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
          else
            foutput_chroma = fopen("out_chroma_tn.yuv", "ab");
        }
      }
    }

    if(foutput_chroma == NULL) {
      fprintf(stdout, "UNABLE TO OPEN OUTPUT FILE\n");
      return;
    }
  }

  if(foutput_chroma && data_chroma) {
    fprintf(stdout, "\t-JPEG: Chrominance\n");
    /* write decoder output to file */
    p_yuv_out = data_chroma;

    if(!progressive) {
      for(i = 0; i < (pic_size_chroma >> 2); i++) {
#ifndef ASIC_TRACE_SUPPORT
        if(DEC_X170_BIG_ENDIAN == output_picture_endian) {
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput_chroma);
        } else {
#endif
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput_chroma);
#ifndef ASIC_TRACE_SUPPORT
        }
#endif
      }
    } else {
      printf("PROGRESSIVE PLANAR OUTPUT CHROMA\n");
      for(i = 0; i < pic_size_chroma; i++)
        fwrite(p_yuv_out + (1 * i), sizeof(u8), 1, foutput_chroma);
    }
  }
  fclose(foutput_chroma);
}

/*------------------------------------------------------------------------------

    Function name:  WriteFullOutput

    Purpose:
        Write picture pointed by data to file.

------------------------------------------------------------------------------*/
void WriteFullOutput(u32 pic_mode) {
  u32 i;
  FILE *foutput = NULL;
  u8 *p_yuv_out_chroma = NULL;
  FILE *f_input_chroma = NULL;
  u32 length = 0;
  u32 chroma_len = 0;
  int ret;
  char filename[256];

  fprintf(stdout, "\t-JPEG: WriteFullOutput\n");

  /* if semi-planar output */
  if(!planar_output) {
    /* Reading chroma file */
    if(pic_mode == 0)
      ret = system("cat out_chroma.yuv >> out.yuv");
    else
      ret = system("cat out_chroma_tn.yuv >> out_tn.yuv");
  } else {
    /* Reading chroma file */
    if(pic_mode == 0)
      f_input_chroma = fopen("out_chroma.yuv", "rb");
    else
      f_input_chroma = fopen("out_chroma_tn.yuv", "rb");

    if(f_input_chroma == NULL) {
      fprintf(stdout, "Unable to open chroma output tmp file\n");
      exit(-1);
    }

    /* file i/o pointer to full */
    fseek(f_input_chroma, 0L, SEEK_END);
    length = ftell(f_input_chroma);
    rewind(f_input_chroma);

    /* check length */
    chroma_len = length;

    p_yuv_out_chroma = JpegDecMalloc(sizeof(u8) * (chroma_len));

    /* read output stream from file to buffer and close input file */
    ret = fread(p_yuv_out_chroma, sizeof(u8), chroma_len, f_input_chroma);
    (void) ret;

    fclose(f_input_chroma);

    /* foutput is global file pointer */
    if(pic_mode == 0)
      foutput = fopen(out_file_name[0], "ab");
    else {
      strcpy(filename, out_file_name[0]);
      sprintf(filename+strlen(filename)-4, "_tn.yuv");
      foutput = fopen(filename, "ab");
    }
    if(foutput == NULL) {
      fprintf(stdout, "UNABLE TO OPEN OUTPUT FILE\n");
      return;
    }

    if(foutput && p_yuv_out_chroma) {
      fprintf(stdout, "\t-JPEG: Chrominance\n");
      if(!progressive) {
        if(!planar_output) {
          /* write decoder output to file */
          for(i = 0; i < (chroma_len >> 2); i++) {
            fwrite(p_yuv_out_chroma + (4 * i) + 0, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 1, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 2, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 3, sizeof(u8), 1, foutput);
          }
        } else {
          for(i = 0; i < chroma_len / 2; i++)
            fwrite(p_yuv_out_chroma + 2 * i, sizeof(u8), 1, foutput);
          for(i = 0; i < chroma_len / 2; i++)
            fwrite(p_yuv_out_chroma + 2 * i + 1, sizeof(u8), 1, foutput);
        }
      } else {
        if(!planar_output) {
          /* write decoder output to file */
          for(i = 0; i < (chroma_len >> 2); i++) {
            fwrite(p_yuv_out_chroma + (4 * i) + 0, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 1, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 2, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 3, sizeof(u8), 1, foutput);
          }
        } else {
          printf("PROGRESSIVE FULL CHROMA %d\n", chroma_len);
          for(i = 0; i < chroma_len; i++)
            fwrite(p_yuv_out_chroma + i, sizeof(u8), 1, foutput);
        }
      }
    }
    fclose(foutput);

    /* Leave properly */
    JpegDecFree(p_yuv_out_chroma);
  }
}

/*------------------------------------------------------------------------------

    Function name:  handleSlicedOutput

    Purpose:
        Calculates size for slice and writes sliced output

------------------------------------------------------------------------------*/
void
handleSlicedOutput(JpegDecImageInfo * image_info,
                   JpegDecInput * jpeg_in, JpegDecOutput * jpeg_out) {
  u32 pp_planar, pp_cr_first, pp_mono_chrome;

  /* for output name */
  full_slice_counter++;

  /******** PHASE X ********/
  if(jpeg_in->slice_mb_set)
    fprintf(stdout, "\nPhase SLICE: HANDLE SLICE %d\n", full_slice_counter);

  /* save start pointers for whole output */
  if(full_slice_counter == 0) {
    /* virtual address */
    output_address_y.virtual_address =
      jpeg_out->pictures[0].output_picture_y.virtual_address;
    output_address_cb_cr.virtual_address =
      jpeg_out->pictures[0].output_picture_cb_cr.virtual_address;

    /* bus address */
    output_address_y.bus_address = jpeg_out->pictures[0].output_picture_y.bus_address;
    output_address_cb_cr.bus_address = jpeg_out->pictures[0].output_picture_cb_cr.bus_address;
  }

  /* if output write not disabled by TB */
  if(write_output) {
    /******** PHASE 5 ********/
    fprintf(stdout, "\nPhase 5: WRITE OUTPUT\n");

    if(image_info->output_format) {
      if(!frame_ready) {
        slice_size = jpeg_in->slice_mb_set * 16;
      } else {
        if(mode == 0)
          slice_size =
            (image_info->output_height -
             ((full_slice_counter) * (slice_size)));
        else
          slice_size =
            (image_info->output_height_thumb -
             ((full_slice_counter) * (slice_size)));
      }
    }

    /* slice interrupt from decoder */
    slice_to_user = 1;

    /* calculate size for output */
    calcSize(image_info, mode, jpeg_out, 0);

    /* test printf */
    fprintf(stdout, "\t-JPEG: ++++++++++ SLICE INFORMATION ++++++++++\n");
    fprintf(stdout, "\t-JPEG: Luma output: %p size: %d\n",
            (void *)jpeg_out->pictures[0].output_picture_y.virtual_address, size_luma);
    fprintf(stdout, "\t-JPEG: Chroma output: %p size: %d\n",
            (void *)jpeg_out->pictures[0].output_picture_cb_cr.virtual_address, size_chroma);
    fprintf(stdout, "\t-JPEG: Luma output bus: %p\n",
            (void *) jpeg_out->pictures[0].output_picture_y.bus_address);
    fprintf(stdout, "\t-JPEG: Chroma output bus: %p\n",
            (void *) jpeg_out->pictures[0].output_picture_cb_cr.bus_address);

    pp_planar      = 0;
    pp_cr_first    = 0;
    pp_mono_chrome = 0;
    if(IS_PIC_PLANAR(jpeg_out->pictures[0].output_format))
      pp_planar = 1;
    else if(IS_PIC_NV21(jpeg_out->pictures[0].output_format))
      pp_cr_first = 1;
    else if(IS_PIC_MONOCHROME(jpeg_out->pictures[0].output_format))
      pp_mono_chrome = 1;
    /* write slice output */
    if (!jpeg_in->dec_image_type) {
      WriteOutput(out_file_name[0],
                   ((u8 *) jpeg_out->pictures[0].output_picture_y.virtual_address),
                   jpeg_out->pictures[0].display_width,
                   jpeg_out->pictures[0].display_height,
                   0, pp_mono_chrome, 0, 0, 0,
                   IS_PIC_TILE(jpeg_out->pictures[0].output_format),
                   jpeg_out->pictures[0].pic_stride, jpeg_out->pictures[0].pic_stride_ch, 0,
                   pp_planar, pp_cr_first, 0,
                   0, 0, md5sum, &fout[0], 1);
    } else {
      WriteOutput(out_file_tn_name[0],
                   ((u8 *) jpeg_out->pictures[0].output_picture_y.virtual_address),
                   jpeg_out->pictures[0].display_width,
                   jpeg_out->pictures[0].display_height,
                   0, pp_mono_chrome, 0, 0, 0,
                   IS_PIC_TILE(jpeg_out->pictures[0].output_format),
                   jpeg_out->pictures[0].pic_stride, jpeg_out->pictures[0].pic_stride_ch, 0,
                   pp_planar, pp_cr_first, 0,
                   0, 0, md5sum, &fout_tn[0], 1);
    }
    /* write luma to final output file */
    WriteOutputLuma(((u8 *) jpeg_out->pictures[0].output_picture_y.virtual_address),
                    size_luma, mode);

    if(image_info->output_format != JPEGDEC_YCbCr400) {
      /* write chroam to tmp file */
      WriteOutputChroma(((u8 *) jpeg_out->pictures[0].output_picture_cb_cr.
                         virtual_address), size_chroma, mode);
    }

    fprintf(stdout, "PHASE 5: WRITE OUTPUT successful\n");
  } else {
    fprintf(stdout, "\nPhase 5: WRITE OUTPUT DISABLED\n");
  }

  if(frame_ready) {
    /* give start pointers for whole output write */

    /* virtual address */
    jpeg_out->pictures[0].output_picture_y.virtual_address =
      output_address_y.virtual_address;
    jpeg_out->pictures[0].output_picture_cb_cr.virtual_address =
      output_address_cb_cr.virtual_address;

    /* bus address */
    jpeg_out->pictures[0].output_picture_y.bus_address = output_address_y.bus_address;
    jpeg_out->pictures[0].output_picture_cb_cr.bus_address = output_address_cb_cr.bus_address;
  }

  if(frame_ready) {
    frame_ready = 0;
    slice_to_user = 0;

    /******** PHASE X ********/
    if(jpeg_in->slice_mb_set)
      fprintf(stdout, "\nPhase SLICE: HANDLE SLICE %d successful\n",
              full_slice_counter);

    full_slice_counter = -1;
  } else {
    /******** PHASE X ********/
    if(jpeg_in->slice_mb_set)
      fprintf(stdout, "\nPhase SLICE: HANDLE SLICE %d successful\n",
              full_slice_counter);
  }

}

/*------------------------------------------------------------------------------

    Function name:  calcSize

    Purpose:
        Calculate size

------------------------------------------------------------------------------*/
void calcSize(JpegDecImageInfo * image_info, u32 pic_mode, JpegDecOutput * jpeg_out, u32 i) {

  u32 format;

  size_luma = 0;
  size_chroma = 0;

  format = pic_mode == 0 ?
           image_info->output_format : image_info->output_format_thumb;

  /* if slice interrupt not given to user */
  if(!slice_to_user || scan_ready) {
    if(pic_mode == 0) {  /* full */
      size_luma = (((image_info->output_width / ds_ratio_x) + 15) & ~0xF) *
                  (image_info->output_height / ds_ratio_y);
    } else { /* thumbnail */
      size_luma =
        (((image_info->output_width_thumb / ds_ratio_x) + 15) & ~0xF)
         * (image_info->output_height_thumb / ds_ratio_y);
    }
  } else {
    if(pic_mode == 0) {  /* full */
      size_luma = (((image_info->output_width / ds_ratio_x) + 15) & ~0xF) *
                   (slice_size / ds_ratio_y);
    } else { /* thumbnail */
      size_luma = (((image_info->output_width_thumb / ds_ratio_x) + 15) & ~0xF) *
                   (slice_size / ds_ratio_y);
    }
  }

  if(format != JPEGDEC_YCbCr400) {
    if(format == JPEGDEC_YCbCr420_SEMIPLANAR ||
       format == JPEGDEC_YCbCr411_SEMIPLANAR) {
      size_chroma = (size_luma / 2);
    } else if(format == JPEGDEC_YCbCr444_SEMIPLANAR) {
      size_chroma = size_luma * 2;
    } else {
      size_chroma = size_luma;
    }
  }
  if(pp_enabled) {
    if(pic_mode == 0) {
      size_luma = jpeg_out->pictures[i].output_width * jpeg_out->pictures[i].output_height;
    } else {
      size_luma = jpeg_out->pictures[i].output_width_thumb * jpeg_out->pictures[i].output_height_thumb;
    }
    size_chroma = (size_luma / 2);
  }
}

/*------------------------------------------------------------------------------

    Function name:  allocMemory

    Purpose:
        Allocates user specific memory for output.

------------------------------------------------------------------------------*/
u32
allocMemory(JpegDecInst dec_inst, JpegDecImageInfo * image_info,
            JpegDecInput * jpeg_in, const void *dwl, u32 buffer_size) {

  out_pic_size_luma = 0;
  out_pic_size_chroma = 0;
  jpeg_in->picture_buffer_y.virtual_address = NULL;
  jpeg_in->picture_buffer_y.bus_address = 0;
  jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cb_cr.bus_address = 0;
  jpeg_in->picture_buffer_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cr.bus_address = 0;

#ifdef PP_PIPELINE_ENABLED
  /* check if rotation used */
  rotation = pp_rotation_used();

  if(rotation)
    fprintf(stdout,
            "\t-JPEG: IN CASE ROTATION ==> USER NEEDS TO ALLOCATE FULL OUTPUT MEMORY\n");
#endif

#if 1
  {
    fprintf(stdout, "\t\t-JPEG: USER OUTPUT MEMORY ALLOCATION\n");

    jpeg_in->picture_buffer_y.virtual_address = NULL;
    jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
    jpeg_in->picture_buffer_cr.virtual_address = NULL;

    /**** memory area ****/

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    user_alloc_output_buffer[0].virtual_address = NULL;
    user_alloc_output_buffer[0].bus_address = 0;

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    if(DWLMallocRefFrm
        (dwl, buffer_size,
         &user_alloc_output_buffer[0]) != DWL_OK) {
      fprintf(stdout, "UNABLE TO ALLOCATE USER LUMA OUTPUT MEMORY\n");
      return JPEGDEC_MEMFAIL;
    }

    /* Luma Bus */
    jpeg_in->picture_buffer_y.virtual_address = user_alloc_output_buffer[0].virtual_address;
    jpeg_in->picture_buffer_y.bus_address = user_alloc_output_buffer[0].bus_address;
    jpeg_in->picture_buffer_y.logical_size = user_alloc_output_buffer[0].logical_size;
    jpeg_in->picture_buffer_y.mem_type = user_alloc_output_buffer[0].mem_type;
    jpeg_in->picture_buffer_y.size = user_alloc_output_buffer[0].size;

    /* memset output to gray */
    (void) DWLmemset(jpeg_in->picture_buffer_y.virtual_address, 128,
                     out_pic_size);

  }
#endif /* #ifdef LINUX */

#if 0
  {
    fprintf(stdout, "\t\t-JPEG: MALLOC\n");

    /* allocate luma */
    jpeg_in->picture_buffer_y.virtual_address =
      (u32 *) JpegDecMalloc(sizeof(u8) * out_pic_size_luma);

    JpegDecMemset(jpeg_in->picture_buffer_y.virtual_address, 128,
                  out_pic_size_luma);

    /* allocate chroma */
    if(out_pic_size_chroma) {
      jpeg_in->picture_buffer_cb_cr.virtual_address =
        (u32 *) JpegDecMalloc(sizeof(u8) * out_pic_size_chroma);

      JpegDecMemset(jpeg_in->picture_buffer_cb_cr.virtual_address, 128,
                    out_pic_size_chroma);
    }
  }
#endif /* #ifndef LINUX */

  /*fprintf(stdout, "\t\t-JPEG: Allocate: Luma virtual %zx bus %zx size %d\n",
          (addr_t)jpeg_in->picture_buffer_y.virtual_address,
          jpeg_in->picture_buffer_y.bus_address, out_pic_size_luma);

  if(separate_chroma == 0) {
    fprintf(stdout,
            "\t\t-JPEG: Allocate: Chroma virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cb_cr.virtual_address,
            jpeg_in->picture_buffer_cb_cr.bus_address, out_pic_size_chroma);
  } else {
    fprintf(stdout,
            "\t\t-JPEG: Allocate: Cb virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cb_cr.virtual_address,
            jpeg_in->picture_buffer_cb_cr.bus_address,
            (out_pic_size_chroma / 2));

    fprintf(stdout,
            "\t\t-JPEG: Allocate: Cr virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cr.virtual_address,
            jpeg_in->picture_buffer_cr.bus_address, (out_pic_size_chroma / 2));
  }*/

  return JPEGDEC_OK;
}


u32 inputMemoryAlloc(JpegDecInst dec_inst, JpegDecImageInfo * image_info,
                     JpegDecInput * jpeg_in, const void *dwl, JpegDecBufferInfo *mem_info) {
  u32 i, idx;
  out_pic_size_luma = 0;
  out_pic_size_chroma = 0;
  jpeg_in->picture_buffer_y.virtual_address = NULL;
  jpeg_in->picture_buffer_y.bus_address = 0;
  jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cb_cr.bus_address = 0;
  jpeg_in->picture_buffer_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cr.bus_address = 0;

  if (user_alloc_output_buffer[0].virtual_address == NULL) {
    for (i = 0; i < output_buffers; i++) {
      if(DWLMallocLinear(dwl, mem_info->next_buf_size,
                         &user_alloc_output_buffer[i]) != 0)
        return (JPEGDEC_MEMFAIL);
    }
  }

  sem_wait(&frame_buff_free);
  pthread_mutex_lock(&frm_buff_stat_lock);
  idx = 0;
  while(frame_mem_status[idx]) {
    idx++;
  }
  assert(idx < output_buffers);
  frame_mem_status[idx] = 1;

  /* reallocate buffer if needed. */
  if (mem_info->buf_to_free.virtual_address != NULL) {
    DWLFreeLinear(dwl, &user_alloc_output_buffer[idx]);
    if(DWLMallocLinear(dwl, mem_info->next_buf_size, &user_alloc_output_buffer[idx]) != 0)
      return (JPEGDEC_MEMFAIL);
  }
  pthread_mutex_unlock(&frm_buff_stat_lock);

  /* Luma Bus */
  jpeg_in->picture_buffer_y.virtual_address = user_alloc_output_buffer[idx].virtual_address;
  jpeg_in->picture_buffer_y.bus_address = user_alloc_output_buffer[idx].bus_address;
  jpeg_in->picture_buffer_y.logical_size = user_alloc_output_buffer[idx].logical_size;
  jpeg_in->picture_buffer_y.size = user_alloc_output_buffer[idx].size;
  jpeg_in->picture_buffer_y.mem_type = user_alloc_output_buffer[idx].mem_type;
  /* memset output to gray */
  (void) DWLmemset(jpeg_in->picture_buffer_y.virtual_address, 128,
                   out_pic_size_luma + out_pic_size_chroma);

#if 0
  if (user_alloc_chroma[0].size == 0) {
    for (i = 0; i < output_buffers; i++) {
      if(DWLMallocLinear(dwl,
                         out_pic_size_chroma, &user_alloc_chroma[i]) != 0) {
        free (&user_alloc_luma[i]);
        return (JPEGDEC_MEMFAIL);
      }
    }
  }

  /* Chroma Bus */
  jpeg_in->picture_buffer_cb_cr.virtual_address =
    user_alloc_chroma[idx].virtual_address;
  jpeg_in->picture_buffer_cb_cr.bus_address =
    user_alloc_chroma[idx].bus_address;

  /* memset output to gray */
  (void) DWLmemset(jpeg_in->picture_buffer_cb_cr.virtual_address, 128,
                   out_pic_size_chroma);
  /* Chroma Bus */
  jpeg_in->picture_buffer_cb_cr.virtual_address =
    user_alloc_luma[idx].virtual_address + out_pic_size_luma / 4;
  jpeg_in->picture_buffer_cb_cr.bus_address =
    user_alloc_chroma[idx].bus_address + out_pic_size_luma;
#endif
  return JPEGDEC_OK;
}
/*-----------------------------------------------------------------------------

Print JPEG api return value

-----------------------------------------------------------------------------*/
void PrintJpegRet(JpegDecRet jpeg_ret) {
  static JpegDecRet prev_retval = 0xFFFFFF;

  switch (jpeg_ret) {
  case JPEGDEC_FRAME_READY:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_FRAME_READY\n");
    break;
  case JPEGDEC_OK:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_OK\n");
    break;
  case JPEGDEC_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_ERROR\n");
    break;
  case JPEGDEC_DWL_HW_TIMEOUT:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_HW_TIMEOUT\n");
    break;
  case JPEGDEC_UNSUPPORTED:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_UNSUPPORTED\n");
    break;
  case JPEGDEC_PARAM_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_PARAM_ERROR\n");
    break;
  case JPEGDEC_MEMFAIL:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_MEMFAIL\n");
    break;
  case JPEGDEC_INITFAIL:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_INITFAIL\n");
    break;
  case JPEGDEC_HW_BUS_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_HW_BUS_ERROR\n");
    break;
  case JPEGDEC_SYSTEM_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_SYSTEM_ERROR\n");
    break;
  case JPEGDEC_DWL_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_DWL_ERROR\n");
    break;
  case JPEGDEC_INVALID_STREAM_LENGTH:
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_INVALID_STREAM_LENGTH\n");
    break;
  case JPEGDEC_STRM_ERROR:
    fprintf(stdout, "TB: jpeg API returned : JPEGDEC_STRM_ERROR\n");
    break;
  case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_INVALID_INPUT_BUFFER_SIZE\n");
    break;
  case JPEGDEC_INCREASE_INPUT_BUFFER:
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_INCREASE_INPUT_BUFFER\n");
    break;
  case JPEGDEC_SLICE_MODE_UNSUPPORTED:
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_SLICE_MODE_UNSUPPORTED\n");
    break;
  case JPEGDEC_NO_DECODING_BUFFER:
    if (prev_retval == JPEGDEC_NO_DECODING_BUFFER) break;
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_NO_DECODING_BUFFER\n");
    break;
  case JPEGDEC_WAITING_FOR_BUFFER:
    fprintf(stdout,
            "TB: jpeg API returned : JPEGDEC_WAITING_FOR_BUFFER\n");
    break;
  default:
    fprintf(stdout, "TB: jpeg API returned unknown status\n");
    break;
  }
  prev_retval = jpeg_ret;
}

/*-----------------------------------------------------------------------------

Print JpegDecGetImageInfo values

-----------------------------------------------------------------------------*/
void PrintGetImageInfo(JpegDecImageInfo * image_info) {
  assert(image_info);

  /* Select if Thumbnail or full resolution image will be decoded */
  if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    /* decode thumbnail */
    fprintf(stdout, "\t-JPEG THUMBNAIL IN STREAM\n");
    fprintf(stdout, "\t-JPEG THUMBNAIL INFO\n");
    fprintf(stdout, "\t\t-JPEG thumbnail display resolution(W x H): %d x %d\n",
            image_info->display_width_thumb, image_info->display_height_thumb);
    fprintf(stdout, "\t\t-JPEG thumbnail HW decoded RESOLUTION(W x H): %d x %d\n",
            NEXT_MULTIPLE(image_info->display_width_thumb, 16),
            NEXT_MULTIPLE(image_info->display_height_thumb, 8));
    fprintf(stdout, "\t\t-JPEG thumbnail OUTPUT SIZE(Stride x H): %d x %d\n",
            image_info->output_width_thumb, image_info->output_height_thumb);

    /* stream type */
    switch (image_info->coding_mode_thumb) {
    case JPEGDEC_BASELINE:
      fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_BASELINE\n");
      break;
    case JPEGDEC_PROGRESSIVE:
      fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_PROGRESSIVE\n");
      break;
    case JPEGDEC_NONINTERLEAVED:
      fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_NONINTERLEAVED\n");
      break;
    }

    if(image_info->output_format_thumb) {
      switch (image_info->output_format_thumb) {
      case JPEGDEC_YCbCr400:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr400\n");
        break;
      case JPEGDEC_YCbCr420_SEMIPLANAR:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr420_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr422_SEMIPLANAR:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr422_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr440:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr440\n");
        break;
      case JPEGDEC_YCbCr411_SEMIPLANAR:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n");
        break;
      case JPEGDEC_YCbCr444_SEMIPLANAR:
        fprintf(stdout,
                "\t\t-JPEG: THUMBNAIL OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n");
        break;
      }
    }
  } else if(image_info->thumbnail_type == JPEGDEC_NO_THUMBNAIL) {
    /* decode full image */
    fprintf(stdout,
            "\t-NO THUMBNAIL IN STREAM ==> Decode full resolution image\n");
  } else if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT) {
    /* decode full image */
    fprintf(stdout,
            "\tNot SUPPORTED THUMBNAIL IN STREAM ==> Decode full resolution image\n");
  }

  fprintf(stdout, "\t-JPEG FULL RESOLUTION INFO\n");
  fprintf(stdout, "\t\t-JPEG display resolution(W x H): %d x %d\n",
          image_info->display_width, image_info->display_height);
  fprintf(stdout, "\t\t-JPEG HW decoded RESOLUTION(W x H): %d x %d\n",
          NEXT_MULTIPLE(image_info->display_width, 16),
          NEXT_MULTIPLE(image_info->display_height, 8));
  fprintf(stdout, "\t\t-JPEG OUTPUT SIZE(Stride x H): %d x %d\n",
          image_info->output_width, image_info->output_height);
  if(image_info->output_format) {
    switch (image_info->output_format) {
    case JPEGDEC_YCbCr400:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr400\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_0_0 = 1;
#endif
      break;
    case JPEGDEC_YCbCr420_SEMIPLANAR:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr420_SEMIPLANAR\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_2_0 = 1;
#endif
      break;
    case JPEGDEC_YCbCr422_SEMIPLANAR:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr422_SEMIPLANAR\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_2_2 = 1;
#endif
      break;
    case JPEGDEC_YCbCr440:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr440\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_4_0 = 1;
#endif
      break;
    case JPEGDEC_YCbCr411_SEMIPLANAR:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_1_1 = 1;
#endif
      break;
    case JPEGDEC_YCbCr444_SEMIPLANAR:
      fprintf(stdout,
              "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_4_4 = 1;
#endif
      break;
    }
  }

  /* stream type */
  switch (image_info->coding_mode) {
  case JPEGDEC_BASELINE:
    fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_BASELINE\n");
    break;
  case JPEGDEC_PROGRESSIVE:
    fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_PROGRESSIVE\n");
#ifdef ASIC_TRACE_SUPPORT
    decoding_tools.progressive = 1;
#endif
    break;
  case JPEGDEC_NONINTERLEAVED:
    fprintf(stdout, "\t\t-JPEG: STREAM TYPE: JPEGDEC_NONINTERLEAVED\n");
    break;
  }

  if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    fprintf(stdout, "\t-JPEG ThumbnailType: JPEG\n");
#ifdef ASIC_TRACE_SUPPORT
    decoding_tools.thumbnail = 1;
#endif
  } else if(image_info->thumbnail_type == JPEGDEC_NO_THUMBNAIL)
    fprintf(stdout, "\t-JPEG ThumbnailType: NO THUMBNAIL\n");
  else if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT)
    fprintf(stdout, "\t-JPEG ThumbnailType: NOT SUPPORTED THUMBNAIL\n");
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecMalloc

------------------------------------------------------------------------------*/
void *JpegDecMalloc(unsigned int size) {
  void *mem_ptr = (char *) malloc(size);

  return mem_ptr;
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecMemset

------------------------------------------------------------------------------*/
void *JpegDecMemset(void *ptr, int c, unsigned int size) {
  void *rv = NULL;

  if(ptr != NULL) {
    rv = memset(ptr, c, size);
  }
  return rv;
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecFree

------------------------------------------------------------------------------*/
void JpegDecFree(void *ptr) {
  if(ptr != NULL) {
    free(ptr);
  }
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecTrace

    Purpose:
        Example implementation of JpegDecTrace function. Prototype of this
        function is given in jpegdecapi.h. This implementation appends
        trace messages to file named 'dec_api.trc'.

------------------------------------------------------------------------------*/
void JpegDecTrace(const char *string) {
  FILE *fp;

  /* in MC mode, the operation to this file cause sgement fault. TBD */
  if (mc_enable)
    return;

  fp = fopen("dec_api.trc", "at");

  if(!fp)
    return;

  fwrite(string, 1, strlen(string), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageInfoEnd

    Purpose:
        Finds 0xFFC4 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageInfoEnd(u8 * stream, u32 stream_length, u32 * p_offset) {
  u32 i;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == stream[i]) {
      if(((i + 1) < stream_length) && 0xC4 == stream[i + 1]) {
        *p_offset = i;
        return 0;
      }
    }
  }
  return -1;
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageEnd

    Purpose:
        Finds 0xFFD9 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageEnd(u8 * stream, u32 stream_length, u32 * p_offset) {
  u32 i,j;
  u32 jpeg_thumb_in_stream = 0;
  u32 tmp, tmp1, tmp_total = 0;
  u32 last_marker = 0;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == stream[i]) {
      /* if 0xFFE1 to 0xFFFD ==> skip  */
      if( (((i + 1) < stream_length) && 0xE1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xED == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEE == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEF == stream[i + 1]) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1]) */) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = stream[i];
        tmp1 = stream[i+1];
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;
        continue;
      }

      /* if 0xFFC2 to 0xFFCB ==> skip  */
      if( (((i + 1) < stream_length) && 0xC1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xCA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xCB == stream[i + 1]) ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = stream[i];
        tmp1 = stream[i+1];
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;

        /* look for EOI */
        for(j = i; j < stream_length; ++j) {
          if(0xFF == stream[j]) {
            /* EOI */
            if(((j + 1) < stream_length) && 0xD9 == stream[j + 1]) {
              /* check length vs. data */
              if((j + 2) >= (stream_length)) {
                nbr_of_images_total = nbr_of_images;
                nbr_of_thumb_images_total = nbr_of_thumb_images;
                return (0);
              }
              /* update */
              i = j;
              /* stil data left ==> break this loop to continue */
              break;
            }
          }
        }
      }

      /* check if thumbnails in stream */
      if(((i + 1) < stream_length) && 0xE0 == stream[i + 1]) {
        if( ((i + 9) < stream_length) &&
            0x4A == stream[i + 4] &&
            0x46 == stream[i + 5] &&
            0x58 == stream[i + 6] &&
            0x58 == stream[i + 7] &&
            0x00 == stream[i + 8] &&
            0x10 == stream[i + 9]) {
          jpeg_thumb_in_stream = 1;
        }
        last_marker = 1;
      }

      /* EOI */
      if(((i + 1) < stream_length) && 0xD9 == stream[i + 1]) {
        *p_offset = i;
        /* update amount of thumbnail or full resolution image */
        if(jpeg_thumb_in_stream) {
          nbr_of_thumb_images++;
          jpeg_thumb_in_stream = 0;
        } else
          nbr_of_images++;

        last_marker = 2;
      }
    }
  }

  /* update total amount of pictures */
  nbr_of_images_total = nbr_of_images;
  nbr_of_thumb_images_total = nbr_of_thumb_images;

  /* continue until amount of frames counted */
  if(last_marker == 2)
    return 0;
  else
    return -1;
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageEOI

    Purpose:
        Finds 0xFFD9 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageEOI(u8 * stream, u32 stream_length, u32 * p_offset) {
  u32 i,j;
  u32 jpeg_thumb_in_stream = 0;
  u32 tmp, tmp1, tmp_total = 0;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == stream[i]) {
      /* if 0xFFE1 to 0xFFFD ==> skip  */

      if( (((i + 1) < stream_length) && 0xE1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xE9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xED == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEE == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xEF == stream[i + 1]) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = stream[i];
        tmp1 = stream[i+1];
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;
        continue;
      }

      /* if 0xFFC2 to 0xFFCB ==> skip  */
      if( (((i + 1) < stream_length) && 0xC1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xC9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xCA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xCB == stream[i + 1]) ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = stream[i];
        tmp1 = stream[i+1];
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;

        /* look for EOI */
        for(j = i; j < stream_length; ++j) {
          if(0xFF == stream[j]) {
            /* EOI */
            if(((j + 1) < stream_length) && 0xD9 == stream[j + 1]) {
              /* check length vs. data */
              if((j + 2) >= (stream_length)) {
                *p_offset = j+2;
                return (0);
              }
              /* update */
              i = j;
              /* stil data left ==> continue */
              continue;
            }
          }
        }
      }

      /* check if thumbnails in stream */
      if(((i + 1) < stream_length) && 0xE0 == stream[i + 1]) {
        if( ((i + 9) < stream_length) &&
            0x4A == stream[i + 4] &&
            0x46 == stream[i + 5] &&
            0x58 == stream[i + 6] &&
            0x58 == stream[i + 7] &&
            0x00 == stream[i + 8] &&
            0x10 == stream[i + 9]) {
          jpeg_thumb_in_stream = 1;
        }
      }

      /* EOI */
      if(((i + 1) < stream_length) && 0xD9 == stream[i + 1]) {
        *p_offset = i + 2;
        /* update amount of thumbnail or full resolution image */
        if(jpeg_thumb_in_stream) {
          jpeg_thumb_in_stream = 0;
        } else
          return 0;

      }
    }
  }

  return -1;
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageTnEOI

    Purpose:
        Finds 0xFFD9 and next 0xFFE0 (containing THUMB) from the stream and
        p_offset includes number of bytes to this marker. In case of an error
        returns != 0 (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageTnEOI(u8 * stream, u32 stream_length, u32 * p_offset, u32 mode, u32 thumb_exist) {
  u32 i,j,k;
  u32 h = 0;

  /* reset */
  next_soi = 0;
  *p_offset = 0;

  for(i = 0; i < stream_length; ++i) {
    if(0xFF == stream[i]) {
      if(((i + 1) < stream_length) && 0xD9 == stream[i + 1]) {
        if(thumb_exist) {
          for(j = (i+2); j < stream_length; ++j) {
            if(0xFF == stream[j]) {
              /* seek for next thumbnail in stream */
              if(((j + 1) < stream_length) && 0xE0 == stream[j + 1]) {
                if( ((j + 9) < stream_length) &&
                    0x4A == stream[j + 4] &&
                    0x46 == stream[j + 5] &&
                    0x58 == stream[j + 6] &&
                    0x58 == stream[j + 7] &&
                    0x00 == stream[j + 8] &&
                    0x10 == stream[j + 9]) {
                  next_soi = 1;
                  *p_offset = h;
                  return 0;
                }
              } else if(((j + 1) < stream_length) && 0xD9 == stream[j + 1]) {
                k = j+2;
                /* return if FULL */
                if(!mode) {
                  *p_offset = k;
                  return 0;
                }
              } else if(((j + 1) < stream_length) && 0xD8 == stream[j + 1]) {
                if(j) {
                  h = j;
                }
              }
            }
          }

          /* if no THUMB, but found next SOI ==> return */
          if(h) {
            *p_offset = h;
            next_soi = 1;
            return 0;
          } else
            return -1;
        } else {
          next_soi = 1;
          *p_offset = i+2;
          return 0;
        }
      }
      /* if 0xFFE1 to 0xFFFD ==> skip  */
      else if( (((i + 1) < stream_length) && 0xE1 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE2 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE3 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE4 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE5 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE6 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE7 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE8 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xE9 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xEA == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xEB == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xEC == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xED == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xEE == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xEF == stream[i + 1]) /*||
               (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
        u32 tmp, tmp1, tmp_total = 0;

        /* increase counter */
        i += 2;
        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = stream[i];
        tmp1 = stream[i+1];
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;
        continue;
      }
    }
  }
  return -1;
}

static u32 FindImageAllRI(u8 *img_buf, u32 img_len, u32 *ri_array, u32 ri_count) {
  u8 *p = img_buf;
  u32 rst_markers = 0;
  u32 last_rst = 0, i;
  ri_array[0] = 0;
  while (p < img_buf + img_len) {
    if (p[0] == 0xFF && p[1] >= 0xD0 && p[1] <= 0xD7) {
      if (!rst_markers) {
        rst_markers++;
      } else {
        /* missing restart intervals */
        u32 missing_rst_count = (p[1] - 0xD7 + 8 - last_rst - 1) % 8;
        for (i = 0; i < missing_rst_count; i++) {
          if (++rst_markers < ri_count)
            ri_array[rst_markers] = 0;
        }
        rst_markers++;
      }
      if (rst_markers < ri_count) {
        ri_array[rst_markers]  = p + 2 - img_buf;
      }
      last_rst = p[1] - 0xD7;
      printf("%d RST%d @ offset %d: %02X%02X\n", rst_markers,  p[1] - 0xD0,
            (u32)(p - img_buf), p[0], p[1]);
    }
    p++;
  }
  rst_markers++;
  return (rst_markers);
}


void WriteCroppedOutput(JpegDecImageInfo * info, u8 * data_luma, u8 * data_cb,
                        u8 * data_cr) {
  u32 i, j;
  FILE *foutput = NULL;
  u8 *p_yuv_out = NULL;
  u32 luma_w, luma_h, chroma_w, chroma_h, chroma_output_width;

  fprintf(stdout, "TB: WriteCroppedOut, display_w %d, display_h %d\n",
          info->display_width, info->display_height);

  foutput = fopen("cropped.yuv", "wb");
  if(foutput == NULL) {
    fprintf(stdout, "UNABLE TO OPEN OUTPUT FILE\n");
    return;
  }

  if(info->output_format == JPEGDEC_YCbCr420_SEMIPLANAR) {
    luma_w = (info->display_width + 1) & ~0x1;
    luma_h = (info->display_height + 1) & ~0x1;
    chroma_w = luma_w / 2;
    chroma_h = luma_h / 2;
    chroma_output_width = info->output_width / 2;
  } else if(info->output_format == JPEGDEC_YCbCr422_SEMIPLANAR) {
    luma_w = (info->display_width + 1) & ~0x1;
    luma_h = info->display_height;
    chroma_w = luma_w / 2;
    chroma_h = luma_h;
    chroma_output_width = info->output_width / 2;
  } else if(info->output_format == JPEGDEC_YCbCr440) {
    luma_w = info->display_width;
    luma_h = (info->display_height + 1) & ~0x1;
    chroma_w = luma_w;
    chroma_h = luma_h / 2;
    chroma_output_width = info->output_width;
  } else if(info->output_format == JPEGDEC_YCbCr411_SEMIPLANAR) {
    luma_w = (info->display_width + 3) & ~0x3;
    luma_h = info->display_height;
    chroma_w = luma_w / 4;
    chroma_h = luma_h;
    chroma_output_width = info->output_width / 4;
  } else if(info->output_format == JPEGDEC_YCbCr444_SEMIPLANAR) {
    luma_w = info->display_width;
    luma_h = info->display_height;
    chroma_w = luma_w;
    chroma_h = luma_h;
    chroma_output_width = info->output_width;
  } else {
    luma_w = info->display_width;
    luma_h = info->display_height;
    chroma_w = 0;
    chroma_h = 0;
    chroma_output_width = 0;

  }

  /* write decoder output to file */
  p_yuv_out = data_luma;
  for(i = 0; i < luma_h; i++) {
    fwrite(p_yuv_out, sizeof(u8), luma_w, foutput);
    p_yuv_out += info->output_width;
  }

  p_yuv_out += (info->output_height - luma_h) * info->output_width;

  /* baseline -> output in semiplanar format */
  if(info->coding_mode != JPEGDEC_PROGRESSIVE) {
    for(i = 0; i < chroma_h; i++)
      for(j = 0; j < chroma_w; j++)
        fwrite(p_yuv_out + i * chroma_output_width * 2 + j * 2,
               sizeof(u8), 1, foutput);
    for(i = 0; i < chroma_h; i++)
      for(j = 0; j < chroma_w; j++)
        fwrite(p_yuv_out + i * chroma_output_width * 2 + j * 2 + 1,
               sizeof(u8), 1, foutput);
  } else {
    p_yuv_out = data_cb;
    for(i = 0; i < chroma_h; i++) {
      fwrite(p_yuv_out, sizeof(u8), chroma_w, foutput);
      p_yuv_out += chroma_output_width;
    }
    /*p_yuv_out += (chroma_output_height-chroma_h)*chroma_output_width; */
    p_yuv_out = data_cr;
    for(i = 0; i < chroma_h; i++) {
      fwrite(p_yuv_out, sizeof(u8), chroma_w, foutput);
      p_yuv_out += chroma_output_width;
    }
  }

  fclose(foutput);
}

void WriteProgressiveOutput(u32 size_luma, u32 size_chroma, u32 mode,
                            u8 * data_luma, u8 * data_cb, u8 * data_cr) {
  FILE *foutput = NULL;

  fprintf(stdout, "TB: WriteProgressiveOutput\n");

  if (strlen(out_file_name[0]) >= 4)
    sprintf(out_file_name[0] + strlen(out_file_name[0])-4, "_0.yuv");

  foutput = fopen(out_file_name[0], "ab");
  if(foutput == NULL) {
    fprintf(stdout, "UNABLE TO OPEN OUTPUT FILE\n");
    return;
  }

  /* write decoder output to file */
  fwrite(data_luma, sizeof(u8), size_luma + size_chroma, foutput);
  //fwrite(data_cb, sizeof(u8), size_chroma / 2, foutput);
  //fwrite(data_cr, sizeof(u8), size_chroma / 2, foutput);

  fclose(foutput);
}
/*------------------------------------------------------------------------------

    Function name: printJpegVersion

    Functional description: Print version info

    Inputs:

    Outputs:    NONE

    Returns:    NONE

------------------------------------------------------------------------------*/
void printJpegVersion(void) {

  JpegDecApiVersion dec_ver;
  JpegDecBuild dec_build;

  /*
   * Get decoder version info
   */

  dec_ver = JpegGetAPIVersion();
  printf("\nApi version:  %d.%d, ", dec_ver.major, dec_ver.minor);

  dec_build = JpegDecGetBuild();
  printf("sw build nbr: %d, hw build nbr: %x\n\n",
         dec_build.sw_build, dec_build.hw_build);

}
