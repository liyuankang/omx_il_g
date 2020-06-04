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

#include <stdlib.h>
#include <string.h>

#include "rvdecapi.h"
#include <unistd.h>

#include "rv_container.h"
#include "rm_parse.h"
#include "rv_depack.h"
#include "rv_decode.h"

#include "hantro_rv_test.h"

#include "tb_cfg.h"
#include "tb_tiled.h"

#include "regdrv.h"
#include "command_line_parser.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif
#include "tb_md5.h"

#include "tb_sw_performance.h"
#include "tb_stream_corrupt.h"
#include "tb_out.h"

#define RM2YUV_INITIAL_READ_SIZE    16
#define MAX_BUFFERS 16

#define MB_MULTIPLE(x)  (((x)+15)&~15)

u32 max_coded_width = 0;
u32 max_coded_height = 0;
u32 max_frame_width = 0;
u32 max_frame_height = 0;
u32 enable_frame_picture = 1;
u32 pp_tile_out = 0;    /* PP tiled output */
u32 pp_planar_out = 0;
u32 ystride = 0;
u32 cstride = 0;
u8 *frame_buffer;
FILE *frame_out = NULL;
char out_file_name[DEC_MAX_PPU_COUNT][256] = {"", "", "", ""};
char out_file_name_tiled[256] = "out_tiled.yuv";
u32 disable_output_writing = 0;
u32 planar_output = 0;
u32 frame_number = 0;
u32 number_of_written_frames = 0;
FILE *f_tiled_output = NULL;
u32 num_frame_buffers = 0;
u32 tiled_output = DEC_REF_FRM_RASTER_SCAN;
u32 convert_tiled_output = 0;
struct TestParams params;

void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height );

u32 rv_input = 0;

u32 use_peek_output = 0;
u32 skip_non_reference = 0;

void decsw_performance(void) {
}


#ifdef RV_TIME_TRACE
#include "../timer/timer.h"
#define TIMER_MAX_COUNTER   0xFFFFFFFF
#define TIMER_FACTOR_MSECOND 1000
u32 ul_start_time_DEC, ul_stop_time_DEC;    /* HW decode time (us) */
u32 ul_start_time_LCD, ul_stop_time_LCD;    /* display time (us) */
u32 ul_start_time_Full, ul_stop_time_Full;  /* total time (us), including parser(sw), driver(sw) & decoder(hw) */
double f_max_time_DEC, f_time_DEC, f_total_time_DEC;
double f_max_time_LCD, f_time_LCD, f_total_time_LCD;
double f_max_time_Full, f_time_Full, f_total_time_Full;
char str_time_info[0x100];
#endif

#ifdef RV_DISPLAY
#include "../../video/v830video_type.h"
#include "../../video/highapi/hbridge.h"
#include "../../video/driver/dipp.h"
#include "../../video/panel/panel.h"

static TSize g_srcimg;
static TSize g_dstimg;
static u32 g_bufmode = LBUF_TWOBUF;
static u32 g_delay_time = 0;
#endif

#ifdef RV_ERROR_SIM
#include "../../tools/random.h"
static u32 g_randomerror = 0;
#endif

#define DWLFile FILE

#define DWL_SEEK_CUR    SEEK_CUR
#define DWL_SEEK_END    SEEK_END
#define DWL_SEEK_SET    SEEK_SET

#define DWLfopen(filename,mode)                 fopen(filename, mode)
#define DWLfread(buf,size,_size_t,filehandle)   fread(buf,size,_size_t,filehandle)
#define DWLfwrite(buf,size,_size_t,filehandle)  fwrite(buf,size,_size_t,filehandle)
#define DWLftell(filehandle)                    ftell(filehandle)
#define DWLfseek(filehandle,offset,whence)      fseek(filehandle,offset,whence)
#define DWLfrewind(filehandle)                  rewind(filehandle)
#define DWLfclose(filehandle)                   fclose(filehandle)
#define DWLfeof(filehandle)                     feof(filehandle)

#undef DEBUG_PRINT
#define DEBUG_PRINT(argv) printf argv
#define ERROR_PRINT(msg) printf msg
#undef ASSERT
#define ASSERT(s)

struct TBCfg tb_cfg;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 output_format = DEC_X170_OUTPUT_FORMAT;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;

u32 pic_big_endian_size = 0;
u8* pic_big_endian = NULL;

u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 stream_header_corrupt = 0;
u32 hdrs_rdy = 0;

u32 seed_rnd = 0;
u32 stream_bit_swap = 0;
i32 corrupted_bytes = 0;
u32 pic_rdy = 0;
u32 packetize = 0;
u32 md5sum = 0;

static u32 g_iswritefile = 0;
static u32 g_isdisplay = 0;
static u32 g_displaymode = 0;
static u32 g_bfirstdisplay = 0;
static u32 g_rotationmode = 0;
static u32 g_blayerenable = 0;
#ifdef RV_TIME_TRACE
static u32 g_islog2std = 0;
static u32 g_islog2file = 0;
#endif
static DWLFile *g_fp_time_log = NULL;

static RvDecInst g_RVDec_inst = NULL;

typedef struct {
  char         file_path[0x100];
  char         out_file_name[DEC_MAX_PPU_COUNT][256];
  DWLFile*     fp_out;
  rv_depack*   p_depack;
  rv_decode*   p_decode;
  BYTE*        p_out_frame;
  UINT32       ul_out_frame_size;
  UINT32       ul_width;
  UINT32       ul_height;
  UINT32       ul_num_input_frames;
  UINT32       ul_num_output_frames;
  UINT32       ul_max_num_dec_frames;
  UINT32       ul_start_frame_id;    /* 0-based */
  UINT32       b_dec_end;
  UINT32       ul_total_time;       /* ms */
} rm2yuv_info;

static HX_RESULT rv_frame_available(void* p_avail, UINT32 ul_sub_stream_num, rv_frame* p_frame);
static HX_RESULT rv_decode_stream_decode(rv_decode* p_front_end, RvDecPicture* output);
static void rv_decode_frame_flush(rm2yuv_info *p_info, RvDecPicture *output);

static UINT32 rm_io_read(void* p_user_read, BYTE* p_buf, UINT32 ul_bytes_to_read);
static void rm_io_seek(void* p_user_read, UINT32 ulOffset, UINT32 ul_origin);
static void* rm_memory_malloc(void* p_user_mem, UINT32 ul_size);
static void rm_memory_free(void* p_user_mem, void* ptr);
static void* rm_memory_create_ncnb(void* p_user_mem, UINT32 ul_size);
static void rm_memory_destroy_ncnb(void* p_user_mem, void* ptr);

static void parse_path(const char *path, char *dir, char *file);
static void rm2yuv_error(void* p_error, HX_RESULT result, const char* psz_msg);
static void printDecodeReturn(i32 retval);
static void printRvVersion(void);
#ifdef RV_DISPLAY
static void displayOnScreen(RvDecPicture *p_dec_picture);
#endif
static void RV_Time_Init(i32 islog2std, i32 islog2file);
static void RV_Time_Full_Reset(void);
static void RV_Time_Full_Pause(void);
static void RV_Time_Dec_Reset(void);
static void RV_Time_Dec_Pause(u32 picid, u32 pictype);
static void printRvPicCodingType(u32 pic_type);

static u32 max_num_pics = 0;
static u32 pics_decoded = 0;
u32 ds_ratio_x, ds_ratio_y;
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

const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 num_buffers;  /* external buffers allocated yet. */
u32 add_buffer_thread_run = 0;
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS];
struct DWLLinearMem rpr_ext_buffers;
FILE *fout[DEC_MAX_PPU_COUNT] = {NULL, NULL, NULL, NULL};

u32 add_extra_flag = 0;
/* Fixme: this value should be set based on option "-d" when invoking testbench. */
u32 buffer_release_flag = 1;

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
        RvDecRet rv = RvDecAddBuffer(g_RVDec_inst, &mem);
        if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
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

  if(rpr_ext_buffers.virtual_address) {
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &rpr_ext_buffers);
    else
      DWLFreeRefFrm(dwl_inst, &rpr_ext_buffers);
    DWLmemset(&rpr_ext_buffers, 0, sizeof(rpr_ext_buffers));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;
u32 is_rm_file = 0;
rm2yuv_info info;

sem_t buf_release_sem;
RvDecPicture buf_list[100] = {{0}};
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
      RvDecPictureConsumed(g_RVDec_inst, &buf_list[list_pop_index]);
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
            RvDecRet rv = RvDecAddBuffer(g_RVDec_inst, &mem);
            if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
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
static void* rv_output_thread(void* arg) {
  RvDecPicture output[1];
  u32 pic_display_number = 0;
  u32 i;
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
    RvDecRet ret;
    u32 pp_planar, pp_cr_first, pp_mono_chrome;
    ret = RvDecNextPicture(g_RVDec_inst, output, 0);

    if(ret == RVDEC_PIC_RDY) {
      if(!is_rm_file) {
        pic_display_number++;

        for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
          if (!output->pictures[i].frame_width ||
              !output->pictures[i].frame_height)
            continue;
          pp_planar      = 0;
          pp_cr_first    = 0;
          pp_mono_chrome = 0;
          if(IS_PIC_PLANAR(output->pictures[i].output_format))
            pp_planar = 1;
          else if(IS_PIC_NV21(output->pictures[i].output_format))
            pp_cr_first = 1;
          else if(IS_PIC_MONOCHROME(output->pictures[i].output_format))
            pp_mono_chrome = 1;

          if (!IS_PIC_PACKED_RGB(output->pictures[i].output_format) &&
              !IS_PIC_PLANAR_RGB(output->pictures[i].output_format))
            WriteOutput(out_file_name[i], (u8*)output->pictures[i].output_picture,
                        output->pictures[i].coded_width,
                        output->pictures[i].coded_height,
                        pic_display_number - 1,
                        pp_mono_chrome, 0, 0, 0,
                        IS_PIC_TILE(output->pictures[i].output_format),
                        output->pictures[i].pic_stride, output->pictures[i].pic_stride_ch, i,
                        pp_planar, pp_cr_first, convert_tiled_output,
                        0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
          else
            WriteOutputRGB(out_file_name[i], (u8*)output->pictures[i].output_picture,
                           output->pictures[i].coded_width,
                           output->pictures[i].coded_height,
                           pic_display_number - 1,
                           pp_mono_chrome, 0, 0, 0,
                           IS_PIC_TILE(output->pictures[i].output_format),
                           output->pictures[i].pic_stride, output->pictures[i].pic_stride_ch, i,
                           IS_PIC_PLANAR_RGB(output->pictures[i].output_format), pp_cr_first,
                           convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
        }
      } else
        rv_decode_frame_flush(&info, output);

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = output[0];
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);

      pic_display_number++;
    }

    else if(ret == RVDEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
  }
  return (NULL);
}



/*-----------------------------------------------------------------------------------------------------
function:
    demux input rm file, decode rv stream threrein, and display on LCD or write to file if necessary

input:
    filename:       the file path and name to be display
    startframeid:   the beginning frame of the whole input stream to be decoded
    maxnumber:      the maximal number of frames to be decoded
    iswritefile:    write the decoded frame to local file or not, 0: no, 1: yes
    isdisplay:      send the decoded data to display device or not, 0: no, 1: yes
    displaymode:    display mode on LCD, 0: auto size, 1: full screen
    rotationmode:   rotation mode on LCD, 0: normal, 1: counter-clockwise, 2: clockwise
    islog2std:      output time log to console
    islog2file:     output time log to file
    errconceal:     error conceal mode (reserved)

return:
    NULL
------------------------------------------------------------------------------------------------------*/
u32 NextPacket(u8 ** p_strm, u8* stream_stop, u32 is_rv8) {
  u32 index;
  u32 max_index;
  static u32 prev_index = 0;
  static u8 *stream = NULL;
  u8 *p;

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

  /* Search stream for next start code prefix */
  /*lint -e(716) while(1) used consciously */
  p = stream + 1;
  while(p < stream_stop) {
    /* RV9 */
    if (is_rv8) {
      if (p[0] == 0   && p[1] == 0   && p[2] == 1  &&
          !(p[3]&170) && !(p[4]&170) && !(p[5]&170) && (p[6]&170) == 2)
        break;
    } else {
      if (p[0] == 85  && p[1] == 85  && p[2] == 85  && p[3] == 85 &&
          !(p[4]&170) && !(p[5]&170) && !(p[6]&170) && (p[7]&170) == 2)
        break;
    }
    p++;
  }

  index = p - stream;
  /* Store pointer to the beginning of the packet */
  *p_strm = stream;
  prev_index = index;

  return (index);
}

/* Stuff for parsing/reading RealVideo streams */
typedef struct {
  FILE *fid;
  u32 w, h;
  u32 len;
  u32 is_rv8;
  u32 num_frame_sizes;
  u32 frame_sizes[18];
  u32 num_segments;
  RvDecSliceInfo slice_info[128];
} rvParser_t;

u32 RvParserInit(rvParser_t *parser, char *file) {
  u32 tmp;
  char buff[100];
  int ret;

  parser->fid = fopen(file, "rb");
  if (parser->fid == NULL) {
    return (HXR_FAIL);
  }

  ret = fread(buff, 1, 4, parser->fid);
  (void) ret;

  /* length */
  tmp = (buff[0] << 24) |
        (buff[1] << 16) |
        (buff[2] <<  8) |
        (buff[3] <<  0);

  tmp = fread(buff, 1, tmp-4, parser->fid);
  if (tmp <= 0)
    return(HXR_FAIL);

  /* 32b len
   * 32b 'VIDO'
   * 32b 'RV30'/'RV40'
   * 16b width
   * 16b height
   * 80b stuff */

  if (strncmp(buff+4, "RV30", 4) == 0)
    parser->is_rv8 = 1;
  else
    parser->is_rv8 = 0;

  parser->w = (buff[8] << 8) | buff[9];
  parser->h = (buff[10] << 8) | buff[11];

  if (parser->is_rv8) {
    u32 i;
    char *p = buff + 22; /* header size 26 - four byte length field */
    parser->num_frame_sizes = 1 + (p[1] & 0x7);
    p += 8;
    parser->frame_sizes[0] = parser->w;
    parser->frame_sizes[1] = parser->h;
    for (i = 1; i < parser->num_frame_sizes; i++) {
      parser->frame_sizes[2*i + 0] = (*p++) << 2;
      parser->frame_sizes[2*i + 1] = (*p++) << 2;
    }
  }
  return (HXR_OK);
}

void RvParserRelease(rvParser_t *parser) {
  if (parser->fid)
    fclose(parser->fid);

  parser->fid = NULL;
}

u32 RvParserReadFrame(rvParser_t *parser, u8 *p) {
  u32 i, tmp;
  u8 buff[256];

  tmp = fread(buff, 1, 20, parser->fid);
  if (tmp < 20)
    return (HXR_FAIL);
  parser->len = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
  parser->num_segments =
    (buff[16] << 24) | (buff[17] << 16) | (buff[18] << 8) | buff[19];

  for (i = 0; i < parser->num_segments; i++) {
    tmp = fread(buff, 1, 8, parser->fid);
    parser->slice_info[i].is_valid =
      (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
    parser->slice_info[i].offset =
      (buff[4] << 24) | (buff[5] << 16) | (buff[6] << 8) | buff[7];
  }

  tmp = fread(p, 1, parser->len, parser->fid);

  if (tmp > 0)
    return HXR_OK;
  else
    return HXR_FAIL;
}

void rv_mode(const char *filename,
             char *filename_pp_cfg,
             int startframeid,
             int maxnumber,
             int iswritefile,
             int isdisplay,
             int displaymode,
             int rotationmode,
             int islog2std,
             int islog2file,
             int blayerenable,
             int errconceal,
             int randomerror) {
  UINT32              i              = 0;
  char                rv_outfilename[256];
  UINT8               size[9] = {0,1,1,2,2,3,3,3,3};
  rvParser_t*         parser         = HXNULL;

  INIT_SW_PERFORMANCE;

  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  RvDecBufferInfo hbuf;
  RvDecRet rv;
  struct DWLInitParam dwl_init;
  RvDecRet rv_ret;
  RvDecInput dec_input;
  RvDecOutput dec_output;
  RvDecInfo dec_info;
  u32 stream_len;
  struct RvDecConfig dec_cfg;
  struct DWLLinearMem stream_mem;
  DWLFile *fp_out = HXNULL;
  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  if (!OpenAsicTraceFiles())
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
#endif

  /* Initialize all static global variables */
  g_RVDec_inst = NULL;
  g_iswritefile = iswritefile;
  g_isdisplay = isdisplay;
  g_displaymode = displaymode;
  g_rotationmode = rotationmode;
  g_bfirstdisplay = 0;
  g_blayerenable = blayerenable;

#ifdef RV_ERROR_SIM
  g_randomerror = randomerror;
#endif

  islog2std = islog2std;
  islog2file = islog2file;

  RV_Time_Init(islog2std, islog2file);

  parser = (rvParser_t *)malloc(sizeof(rvParser_t));
  memset(parser, 0, sizeof(rvParser_t));

  RvParserInit(parser, (char *)filename);

  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto cleanup;
  }
  START_SW_PERFORMANCE;
  decsw_performance();
  rv_ret = RvDecInit(&g_RVDec_inst,
                     dwl_inst,
                     TBGetDecErrorConcealment( &tb_cfg ),
                     size[parser->num_frame_sizes], parser->frame_sizes, !parser->is_rv8,
                     parser->w, parser->h,
                     num_frame_buffers, tiled_output, 1, 0);
  END_SW_PERFORMANCE;
  decsw_performance();
  if (rv_ret != RVDEC_OK) {
    ERROR_PRINT(("RVDecInit fail! \n"));
    goto cleanup;
  }

  /* Set ref buffer test mode */
  //((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl.test_function = TBRefbuTestMode;

  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
  */
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_SERV_MERGE_DIS,
                 service_merge_disable);

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  decsw_performance();
  rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(rv_ret) {
    printDecodeReturn(rv_ret);
  }

  TBInitializeRandom(seed_rnd);

  /* memory for stream buffer */
  stream_len = 10000000;
  if (DWLMallocLinear(dwl_inst,
                      stream_len, &stream_mem) != DWL_OK) {
    DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    return;
  }

  dec_input.skip_non_reference = skip_non_reference;
  dec_input.pic_id = 0;
  dec_input.timestamp = 0;

  while (dec_input.data_len == dec_output.data_left ||
    RvParserReadFrame(parser, (u8*)stream_mem.virtual_address) == HXR_OK) {
    dec_input.data_len = parser->len;
    dec_input.stream = (u8*)stream_mem.virtual_address;
    dec_input.stream_bus_address = stream_mem.bus_address;

    dec_input.slice_info_num = parser->num_segments;
    dec_input.slice_info = parser->slice_info;

    START_SW_PERFORMANCE;
    decsw_performance();
    rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
    END_SW_PERFORMANCE;
    decsw_performance();

    printDecodeReturn(rv_ret);

    if (rv_ret == RVDEC_HDRS_RDY) {
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );

      rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
      printf("*********************** RvDecGetInfo()/RAW ******************\n");
      printf("dec_info.frame_width %d\n",(dec_info.frame_width << 4));
      printf("dec_info.frame_height %d\n",(dec_info.frame_height << 4));
      printf("dec_info.coded_width %d\n",dec_info.coded_width);
      printf("dec_info.coded_height %d\n",dec_info.coded_height);
      if(dec_info.output_format == RVDEC_SEMIPLANAR_YUV420)
        printf("dec_info.output_format RVDEC_SEMIPLANAR_YUV420\n");
      else
        printf("dec_info.output_format RVDEC_TILED_YUV420\n");
      printf("******************* RAW MODE ********************************\n");
      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);

      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.coded_width;
          params.ppu_cfg[0].crop.height = dec_info.coded_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }

        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;
        params.ppu_cfg[0].scale.width = (crop_w /
                                         params.fscale_cfg.down_scale_x) & ~0x1;
        params.ppu_cfg[0].scale.height = (crop_h /
                                         params.fscale_cfg.down_scale_y) & ~0x1;
        params.ppu_cfg[0].scale.enabled = 1;
        params.ppu_cfg[0].scale.ratio_x |= 0;
        params.ppu_cfg[0].scale.ratio_y |= 0;
        params.ppu_cfg[0].enabled = 1;
        params.pp_enabled = 1;
      }
      pp_enabled = params.pp_enabled;
      memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
      dec_cfg.align = align;
      dec_cfg.cr_first = cr_first;
      u32 tmp = RvDecSetInfo(g_RVDec_inst, &dec_cfg);
      if (tmp != RVDEC_OK)
        goto cleanup;
#if 0
      if(dec_info.pic_buff_size != min_buffer_num ||
          (dec_info.frame_width * dec_info.frame_height > prev_width * prev_height)) {
        /* Reset buffers added and stop adding extra buffers when a new header comes. */
        if (pp_enabled)
          res_changed = 1;
        else {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
      }
#endif
      //prev_width = dec_info.frame_width;
      //prev_height = dec_info.frame_height;
      //min_buffer_num = dec_info.pic_buff_size;
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
      END_SW_PERFORMANCE;
      decsw_performance();
      if (rv_ret == RVDEC_WAITING_FOR_BUFFER) {
        rv = RvDecGetBufferInfo(g_RVDec_inst, &hbuf);

        printf("RvDecGetBufferInfo ret %d\n", rv);
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
          buffer_size = hbuf.next_buf_size;
          struct DWLLinearMem mem;

          for(i = 0; i < hbuf.buf_num; i++) {
            if (pp_enabled)
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            else
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            rv = RvDecAddBuffer(g_RVDec_inst, &mem);
            printf("RvDecAddBuffer ret %d\n", rv);

            if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              if(i < (hbuf.buf_num - 1))
                ext_buffers[i] = mem;
              else
                rpr_ext_buffers = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num - 1;
          add_extra_flag = 1;
        }

        START_SW_PERFORMANCE;
        decsw_performance();
        rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
        END_SW_PERFORMANCE;
        decsw_performance();
      }
    }

    switch(rv_ret) {
    case RVDEC_OK:
    case RVDEC_PIC_DECODED:
      printf("Pic %d decoded\n", pics_decoded);
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = RvDecAbort(g_rvdec_inst);
        rv_ret = RvDecAbortAfter(g_rvdec_inst);
      }
#endif

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      break;

    case RVDEC_HDRS_RDY:
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(rv_ret) {
        printDecodeReturn(rv_ret);
      }

      printf("*********************** RvDecGetInfo()/RAW ******************\n");
      printf("dec_info.frame_width %d\n",(dec_info.frame_width << 4));
      printf("dec_info.frame_height %d\n",(dec_info.frame_height << 4));
      printf("dec_info.coded_width %d\n",dec_info.coded_width);
      printf("dec_info.coded_height %d\n",dec_info.coded_height);
      if(dec_info.output_format == RVDEC_SEMIPLANAR_YUV420)
        printf("dec_info.output_format RVDEC_SEMIPLANAR_YUV420\n");
      else
        printf("dec_info.output_format RVDEC_TILED_YUV420\n");
      printf("******************* RAW MODE ********************************\n");

      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);
      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.coded_width;
          params.ppu_cfg[0].crop.height = dec_info.coded_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }

        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;

        params.ppu_cfg[0].scale.width = (crop_w /
                                         params.fscale_cfg.down_scale_x) & ~0x1;
        params.ppu_cfg[0].scale.height = (crop_h /
                                         params.fscale_cfg.down_scale_y) & ~0x1;
        params.ppu_cfg[0].scale.enabled = 1;
        params.ppu_cfg[0].scale.ratio_x |= 0;
        params.ppu_cfg[0].scale.ratio_y |= 0;
        params.ppu_cfg[0].enabled = 1;
        params.pp_enabled = 1;
      }
      pp_enabled = params.pp_enabled;
      memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
      dec_cfg.align = align;
      dec_cfg.cr_first = cr_first;
      u32 tmp = RvDecSetInfo(g_RVDec_inst, &dec_cfg);
      if (tmp != RVDEC_OK)
        goto cleanup;

#if 0
      if(dec_info.pic_buff_size != min_buffer_num ||
          (dec_info.frame_width * dec_info.frame_height > prev_width * prev_height)) {
        /* Reset buffers added and stop adding extra buffers when a new header comes. */
        if (pp_enabled)
          res_changed = 1;
        else {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
      }
#endif
      //prev_width = dec_info.frame_width;
      //prev_height = dec_info.frame_height;
      //min_buffer_num = dec_info.pic_buff_size;
      break;
    case RVDEC_WAITING_FOR_BUFFER:
      rv = RvDecGetBufferInfo(g_RVDec_inst, &hbuf);
      printf("RvDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;

        for(i = 0; i < hbuf.buf_num; i++) {
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = RvDecAddBuffer(g_RVDec_inst, &mem);
          printf("RvDecAddBuffer ret %d\n", rv);

          if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i < (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num - 1;
        add_extra_flag = 1;
      }
      break;

    case RVDEC_NONREF_PIC_SKIPPED:
    case RVDEC_NO_DECODING_BUFFER:
      break;

    default:
      /*
      * RVDEC_NOT_INITIALIZED
      * RVDEC_PARAM_ERROR
      * RVDEC_STRM_ERROR
      * RVDEC_STRM_PROCESSED
      * RVDEC_SYSTEM_ERROR
      * RVDEC_MEMFAIL
      * RVDEC_HW_TIMEOUT
      * RVDEC_HW_RESERVED
      * RVDEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("Fatal Error, Decode end!\n"));
      break;
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  }

  if(output_thread_run)
    RvDecEndOfStream(g_RVDec_inst, 1);

  DWLFreeLinear(dwl_inst, &stream_mem);

  if (pic_big_endian != NULL)
    free(pic_big_endian);

  TIME_PRINT(("\n"));

cleanup:
  add_buffer_thread_run = 0;

  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  RvParserRelease(parser);

  /* Close the output file */
  if (fp_out) {
    DWLfclose(fp_out);
  }

  if(f_tiled_output) {
    DWLfclose(f_tiled_output);
    f_tiled_output = NULL;
  }

  /* Release the decoder instance */
  if (g_RVDec_inst) {
    START_SW_PERFORMANCE;
    decsw_performance();
    ReleaseExtBuffers();
    pthread_mutex_destroy(&ext_buffer_contro);
    RvDecRelease(g_RVDec_inst);
    DWLRelease(dwl_inst);
    END_SW_PERFORMANCE;
    decsw_performance();
  }

  FINALIZE_SW_PERFORMANCE;

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

}

void raw_mode(DWLFile* fp_in,
              char *filename_pp_cfg,
              int startframeid,
              int maxnumber,
              int iswritefile,
              int isdisplay,
              int displaymode,
              int rotationmode,
              int islog2std,
              int islog2file,
              int blayerenable,
              int errconceal,
              int randomerror) {
  UINT32              i              = 0;
  char                rv_outfilename[256];
  char                tmp_stream[4];
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;

  INIT_SW_PERFORMANCE;

  RvDecBufferInfo hbuf;
  RvDecRet rv;
  struct DWLInitParam dwl_init;
  RvDecRet rv_ret;
  RvDecOutput dec_output;
  RvDecInput dec_input;
  RvDecInfo dec_info;

  struct DWLLinearMem stream_mem;
  struct RvDecConfig dec_cfg;
  u32 stream_len;
  u8 *stream_stop;
  u8 *ptmpstream;
  u32 tmp;
  u32 is_rv8;
  int ret;

  DWLFile *fp_out = HXNULL;
  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

  DEBUG_PRINT(("Start decode ...\n"));

  ret = DWLfread(tmp_stream, sizeof(u8), 4, fp_in);
  (void) ret;
  DWLfseek(fp_in, 0, DWL_SEEK_SET);

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    return;
  }
  /* RV9 raw stream */
  if (tmp_stream[0] == 0 && tmp_stream[1] == 0 &&
      tmp_stream[2] == 1) {
    START_SW_PERFORMANCE;
    decsw_performance();
    rv_ret = RvDecInit(&g_RVDec_inst,
                       dwl_inst,
                       TBGetDecErrorConcealment( &tb_cfg ), 0, 0, 0, 0, 0,
                       num_frame_buffers, tiled_output, 1 , 0);
    END_SW_PERFORMANCE;
    decsw_performance();
    is_rv8 = 1;
  } else {
    START_SW_PERFORMANCE;
    decsw_performance();
    rv_ret = RvDecInit(&g_RVDec_inst,
                       dwl_inst,
                       TBGetDecErrorConcealment( &tb_cfg ), 0, 0, 1, 0, 0,
                       num_frame_buffers, tiled_output, 1, 0);
    END_SW_PERFORMANCE;
    decsw_performance();
    is_rv8 = 0;
  }

  if (rv_ret != RVDEC_OK) {
    ERROR_PRINT(("RVDecInit fail! \n"));
    return;
  }

  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
                 */
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);

  TBInitializeRandom(seed_rnd);

  stream_len = 10000000;
  if(DWLMallocLinear(dwl_inst,
                     stream_len, &stream_mem) != DWL_OK) {
    DEBUG_PRINT(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    return;
  }

  stream_len = DWLfread(stream_mem.virtual_address, sizeof(u8), stream_len, fp_in);
  stream_stop = (u8*)stream_mem.virtual_address+stream_len;

  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = (u8*)stream_mem.virtual_address;
  dec_input.stream_bus_address = stream_mem.bus_address;
  dec_input.pic_id = 0;
  dec_input.timestamp = 0;

  ptmpstream = dec_input.stream;
  tmp = NextPacket((u8**)(&dec_input.stream),stream_stop, is_rv8);
  dec_input.data_len = tmp;
  dec_input.stream_bus_address += (u32)(dec_input.stream-ptmpstream);

  dec_input.slice_info_num = 0;
  dec_input.slice_info = NULL;

  /* Now keep getting packets until we receive an error */
  do {
    START_SW_PERFORMANCE;
    decsw_performance();
    rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
    END_SW_PERFORMANCE;
    decsw_performance();
    if (rv_ret == RVDEC_HDRS_RDY) {
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );
      rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
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
#endif
      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);

      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.coded_width;
          params.ppu_cfg[0].crop.height = dec_info.coded_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }

        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;

        params.ppu_cfg[0].scale.width = (crop_w /
                                         params.fscale_cfg.down_scale_x) & ~0x1;
        params.ppu_cfg[0].scale.height = (crop_h /
                                         params.fscale_cfg.down_scale_y) & ~0x1;
        params.ppu_cfg[0].scale.enabled = 1;
        params.ppu_cfg[0].scale.ratio_x |= 0;
        params.ppu_cfg[0].scale.ratio_y |= 0;
        params.ppu_cfg[0].enabled = 1;
        params.pp_enabled = 1;
      }
      pp_enabled = params.pp_enabled;
      memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
      dec_cfg.align = align;
      dec_cfg.cr_first = cr_first;
      tmp = RvDecSetInfo(g_RVDec_inst, &dec_cfg);
      if (tmp != RVDEC_OK)
        return;
#if 0
      if(dec_info.pic_buff_size != min_buffer_num ||
          (dec_info.frame_width * dec_info.frame_height > prev_width * prev_height)) {
        /* Reset buffers added and stop adding extra buffers when a new header comes. */
        if (pp_enabled)
          res_changed = 1;
        else {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
      }
#endif
      //prev_width = dec_info.frame_width;
      //prev_height = dec_info.frame_height;
      //min_buffer_num = dec_info.pic_buff_size;
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
      END_SW_PERFORMANCE;
      decsw_performance();
      if (rv_ret == RVDEC_WAITING_FOR_BUFFER) {
        rv = RvDecGetBufferInfo(g_RVDec_inst, &hbuf);
        printf("RvDecGetBufferInfo ret %d\n", rv);
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
          buffer_size = hbuf.next_buf_size;
          struct DWLLinearMem mem;

          for(i = 0; i < hbuf.buf_num; i++) {
            if (pp_enabled)
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            else
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            rv = RvDecAddBuffer(g_RVDec_inst, &mem);
            printf("RvDecAddBuffer ret %d\n", rv);

            if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              if(i < (hbuf.buf_num - 1))
                ext_buffers[i] = mem;
              else
                rpr_ext_buffers = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num - 1;
          add_extra_flag = 1;
        }

        START_SW_PERFORMANCE;
        decsw_performance();
        rv_ret = RvDecDecode(g_RVDec_inst, &dec_input, &dec_output);
        END_SW_PERFORMANCE;
        decsw_performance();
      }
    }

    switch(rv_ret) {
    case RVDEC_OK:
    case RVDEC_PIC_DECODED:
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = RvDecAbort(g_rvdec_inst);
        rv_ret = RvDecAbortAfter(g_rvdec_inst);
      }
#endif


      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      break;

    case RVDEC_HDRS_RDY:
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(rv_ret) {
        printDecodeReturn(rv_ret);
      }

      printf("*********************** RvDecGetInfo()/RAW ******************\n");
      printf("dec_info.frame_width %d\n",(dec_info.frame_width << 4));
      printf("dec_info.frame_height %d\n",(dec_info.frame_height << 4));
      printf("dec_info.coded_width %d\n",dec_info.coded_width);
      printf("dec_info.coded_height %d\n",dec_info.coded_height);
      if(dec_info.output_format == RVDEC_SEMIPLANAR_YUV420)
        printf("dec_info.output_format RVDEC_SEMIPLANAR_YUV420\n");
      else
        printf("dec_info.output_format RVDEC_TILED_YUV420\n");
      printf("******************* RAW MODE ********************************\n");

      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);

      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.coded_width;
          params.ppu_cfg[0].crop.height = dec_info.coded_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }

        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;

        params.ppu_cfg[0].scale.width = (crop_w /
                                         params.fscale_cfg.down_scale_x) & ~0x1;
        params.ppu_cfg[0].scale.height = (crop_h /
                                         params.fscale_cfg.down_scale_y) & ~0x1;
        params.ppu_cfg[0].scale.enabled = 1;
        params.ppu_cfg[0].scale.ratio_x |= 0;
        params.ppu_cfg[0].scale.ratio_y |= 0;
        params.ppu_cfg[0].enabled = 1;
        params.pp_enabled = 1;
      }
      pp_enabled = params.pp_enabled;
      memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
      dec_cfg.align = align;
      dec_cfg.cr_first = cr_first;
      u32 tmp = RvDecSetInfo(g_RVDec_inst, &dec_cfg);
      if (tmp != RVDEC_OK)
        return;

#if 0
      if(dec_info.pic_buff_size != min_buffer_num ||
          (dec_info.frame_width * dec_info.frame_height > prev_width * prev_height)) {
        /* Reset buffers added and stop adding extra buffers when a new header comes. */
        if (pp_enabled)
          res_changed = 1;
        else {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
      }
#endif
      //prev_width = dec_info.frame_width;
      //prev_height = dec_info.frame_height;
      //min_buffer_num = dec_info.pic_buff_size;
      break;
    case RVDEC_WAITING_FOR_BUFFER:
      rv = RvDecGetBufferInfo(g_RVDec_inst, &hbuf);
      printf("RvDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;
        for(i = 0; i < hbuf.buf_num; i++) {
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = RvDecAddBuffer(g_RVDec_inst, &mem);
          printf("RvDecAddBuffer ret %d\n", rv);

          if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i < (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num - 1;
        add_extra_flag = 1;
      }
      break;

    case RVDEC_NONREF_PIC_SKIPPED:
    case RVDEC_NO_DECODING_BUFFER:
      break;

    default:
      /*
      * RVDEC_NOT_INITIALIZED
      * RVDEC_PARAM_ERROR
      * RVDEC_STRM_ERROR
      * RVDEC_STRM_PROCESSED
      * RVDEC_SYSTEM_ERROR
      * RVDEC_MEMFAIL
      * RVDEC_HW_TIMEOUT
      * RVDEC_HW_RESERVED
      * RVDEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("Fatal Error, Decode end!\n"));
      break;
    }

    if (rv_ret != RVDEC_NO_DECODING_BUFFER) {
      ptmpstream = dec_input.stream;
      tmp = NextPacket((u8**)(&dec_input.stream),stream_stop, is_rv8);
      dec_input.data_len = tmp;
      dec_input.stream_bus_address += (u32)(dec_input.stream-ptmpstream);
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  } while(dec_input.data_len > 0);

  if(output_thread_run)
    RvDecEndOfStream(g_RVDec_inst, 1);
  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  DWLFreeLinear(dwl_inst, &stream_mem);

  if(fp_out)
    DWLfclose(fp_out);
  if(f_tiled_output)
    DWLfclose(f_tiled_output);

  if (pic_big_endian != NULL)
    free(pic_big_endian);

  FINALIZE_SW_PERFORMANCE;
}

void rv_display(const char *filename,
                char *filename_pp_cfg,
                int startframeid,
                int maxnumber,
                int iswritefile,
                int isdisplay,
                int displaymode,
                int rotationmode,
                int islog2std,
                int islog2file,
                int blayerenable,
                int errconceal,
                int randomerror) {
  HX_RESULT           ret_val         = HXR_OK;
  DWLFile*            fp_in           = HXNULL;
  INT32               l_bytes_read     = 0;
  UINT32              ul_num_streams   = 0;
  UINT32              i              = 0;
  UINT16              us_stream_num    = 0;
  UINT32              ul_out_frame_size = 0;
  rm_parser*          p_parser        = HXNULL;
  rm_stream_header*   p_hdr           = HXNULL;
  rm_packet*          p_packet        = HXNULL;
  rv_depack*          p_depack        = HXNULL;
  rv_decode*          p_decode        = HXNULL;
  rv_format_info*     p_info          = HXNULL;
  BYTE                uc_buf[RM2YUV_INITIAL_READ_SIZE];
  char                rv_outfilename[256];

#ifdef RV_TIME_TRACE
  UINT32              ul_start_time = TIMER_GET_NOW_TIME();
  UINT32              ul_stop_time = 0;
#endif

  INIT_SW_PERFORMANCE;

  RvDecRet rv_ret;
  /*RVDecInstInit RVInstInit;*/
  RvDecInfo dec_info;

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  if (!OpenAsicTraceFiles())
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
#endif

  /* Initialize all static global variables */
  g_RVDec_inst = NULL;
  g_iswritefile = iswritefile;
  g_isdisplay = isdisplay;
  g_displaymode = displaymode;
  g_rotationmode = rotationmode;
  g_bfirstdisplay = 0;
  g_blayerenable = blayerenable;

#ifdef RV_ERROR_SIM
  g_randomerror = randomerror;
#endif

  islog2std = islog2std;
  islog2file = islog2file;

  RV_Time_Init(islog2std, islog2file);

  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

  /* NULL out the info */
  DWLmemset(info.file_path, 0, sizeof(info.file_path));
  info.fp_out             = HXNULL;
  info.p_depack           = HXNULL;
  info.p_decode           = HXNULL;
  info.p_out_frame         = HXNULL;
  info.ul_width           = 0;
  info.ul_height          = 0;
  info.ul_out_frame_size    = 0;
  info.ul_num_input_frames  = 0;
  info.ul_num_output_frames = 0;
  info.ul_max_num_dec_frames = maxnumber;
  info.ul_start_frame_id    = startframeid;
  info.b_dec_end           = 0;
  info.ul_total_time       = 0;

  /* Open the input file */
#ifdef WIN32
  fp_in = DWLfopen((const char*) filename, "rb");
#else
  fp_in = DWLfopen((const char*) filename, "r");
#endif

  DEBUG_PRINT(("filename=%s\n",filename));
  if (!fp_in) {
    ERROR_PRINT(("Could not open %s for reading.\n", filename));
    goto cleanup;
  }

  /* Read the first few bytes of the file */
  l_bytes_read = (INT32) DWLfread((void*) uc_buf, 1, RM2YUV_INITIAL_READ_SIZE, fp_in);
  if (l_bytes_read != RM2YUV_INITIAL_READ_SIZE) {
    ERROR_PRINT(("Could not read %d bytes at the beginning of %s\n",
                 RM2YUV_INITIAL_READ_SIZE, filename));
    goto cleanup;
  }
  /* Seek back to the beginning */
  DWLfseek(fp_in, 0, DWL_SEEK_SET);

  /* Make sure this is an .rm file */
  if (!rm_parser_is_rm_file(uc_buf, RM2YUV_INITIAL_READ_SIZE)) {
    ERROR_PRINT(("%s is not an .rm file.\n", filename));
    raw_mode(fp_in,
             filename_pp_cfg,
             startframeid,
             maxnumber,
             iswritefile,
             isdisplay,
             displaymode,
             rotationmode,
             islog2std,
             islog2file,
             blayerenable,
             errconceal,
             randomerror);
    goto cleanup;
  }

  is_rm_file = 1;
  /* Create the parser struct */
  p_parser = rm_parser_create2(HXNULL,
                               rm2yuv_error,
                               HXNULL,
                               rm_memory_malloc,
                               rm_memory_free);
  if (!p_parser) {
    goto cleanup;
  }

  /* Set the file into the parser */
  ret_val = rm_parser_init_io(p_parser,
                              (void *)fp_in,
                              (rm_read_func_ptr)rm_io_read,
                              (rm_seek_func_ptr)rm_io_seek);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Read all the headers at the beginning of the .rm file */
  ret_val = rm_parser_read_headers(p_parser);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the number of streams */
  ul_num_streams = rm_parser_get_num_streams(p_parser);
  if (ul_num_streams == 0) {
    ERROR_PRINT(("Error: rm_parser_get_num_streams() returns 0\n"));
    goto cleanup;
  }

  /* Now loop through the stream headers and find the video */
  for (i = 0; i < ul_num_streams && ret_val == HXR_OK; i++) {
    ret_val = rm_parser_get_stream_header(p_parser, i, &p_hdr);
    if (ret_val == HXR_OK) {
      if (rm_stream_is_realvideo(p_hdr)) {
        us_stream_num = (UINT16) i;
        break;
      } else {
        /* Destroy the stream header */
        rm_parser_destroy_stream_header(p_parser, &p_hdr);
      }
    }
  }

  /* Do we have a RealVideo stream in this .rm file? */
  if (!p_hdr) {
    ERROR_PRINT(("There is no RealVideo stream in this file.\n"));
    goto cleanup;
  }

  /* Create the RealVideo depacketizer */
  p_depack = rv_depack_create2_ex((void*) &info,
                                  rv_frame_available,
                                  HXNULL,
                                  rm2yuv_error,
                                  HXNULL,
                                  rm_memory_malloc,
                                  rm_memory_free,
                                  rm_memory_create_ncnb,
                                  rm_memory_destroy_ncnb);
  if (!p_depack) {
    goto cleanup;
  }

  /* Assign the rv_depack pointer to the info struct */
  info.p_depack = p_depack;

  /* Initialize the RV depacketizer with the RealVideo stream header */
  ret_val = rv_depack_init(p_depack, p_hdr);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the bitstream header information, create rv_infor_format struct and init it */
  ret_val = rv_depack_get_codec_init_info(p_depack, &p_info);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /*
  * Print out the width and height so the user
  * can input this into their YUV-viewing program.
  */
  //DEBUG_PRINT(("Video in %s has dimensions %lu x %lu pixels (width x height)\n",
  //  (const char*) filename, info.ul_width, info.ul_height));

  /*
  * Get the frames per second. This value is in 32-bit
  * fixed point, where the upper 16 bits is the integer
  * part of the fps, and the lower 16 bits is the fractional
  * part. We're going to truncate to integer, so all
  * we need is the upper 16 bits.
  */

  /* Create an rv_decode object */
  p_decode = rv_decode_create2(HXNULL,
                               rm2yuv_error,
                               HXNULL,
                               rm_memory_malloc,
                               rm_memory_free);
  if (!p_decode) {
    goto cleanup;
  }

  /* Assign the decode object into the rm2yuv_info struct */
  info.p_decode = p_decode;

  /* Init the rv_decode object */
  ret_val = rv_decode_init(p_decode, p_info);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the size of an output frame */
  ul_out_frame_size = rv_decode_max_output_size(p_decode);
  if (!ul_out_frame_size) {
    goto cleanup;
  }
  info.ul_out_frame_size = ul_out_frame_size;

  DEBUG_PRINT(("Start decode ...\n"));

  /* Init RV decode instance */
  /*RVInstInit.isrv8 = p_decode->bIsRV8;*/
  /*RVInstInit.pctszSize = p_decode->ulPctszSize;*/
  /*RVInstInit.pSizes = p_decode->pDimensions;*/
  /*RVInstInit.maxwidth = p_decode->ulLargestPels;*/
  /*RVInstInit.maxheight = p_decode->ulLargestLines;*/

  max_coded_width = p_decode->ulLargestPels;
  max_coded_height = p_decode->ulLargestLines;
  max_frame_width = MB_MULTIPLE( max_coded_width );
  max_frame_height = MB_MULTIPLE( max_coded_height );

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto cleanup;
  }
  START_SW_PERFORMANCE;
  decsw_performance();
  rv_ret = RvDecInit(&g_RVDec_inst,
                     dwl_inst,
                     /*&RVInstInit*/TBGetDecErrorConcealment( &tb_cfg ),
                     p_decode->ulPctszSize, (u32*)p_decode->pDimensions, !p_decode->bIsRV8,
                     p_decode->ulLargestPels, p_decode->ulLargestLines,
                     num_frame_buffers, tiled_output, 1 , 0);
  END_SW_PERFORMANCE;
  decsw_performance();
  if (rv_ret != RVDEC_OK) {
    ERROR_PRINT(("RVDecInit fail! \n"));
    goto cleanup;
  }

  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  /*
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_TILED_E,
                 output_format);
                 */
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((RvDecContainer *) g_RVDec_inst)->rv_regs, HWIF_DEC_DATA_DISC_E,
                 data_discard);

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  decsw_performance();
  rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
  END_SW_PERFORMANCE;
  decsw_performance();
  if(rv_ret) {
    printDecodeReturn(rv_ret);
  }

  TBInitializeRandom(seed_rnd);

  /* Fill in the width and height */
  info.ul_width  = (p_decode->ulLargestPels+15)&0xFFFFFFF0;
  info.ul_height = (p_decode->ulLargestLines+15)&0xFFFFFFF0;

  /* Parse the output file path */
  if(g_iswritefile) {
    parse_path(filename, info.file_path, NULL);
  }

#ifdef RV_DISPLAY
  if (g_displaymode) {
    Panel_GetSize(&g_dstimg);
  } else {
    g_dstimg.cx = 0;
    g_dstimg.cy = 0;
  }
#endif

  RV_Time_Full_Reset();

  /* Now keep getting packets until we receive an error */
  while (ret_val == HXR_OK && !info.b_dec_end) {
    /* Get the next packet */
    ret_val = rm_parser_get_packet(p_parser, &p_packet);
    if (ret_val == HXR_OK) {
      /* Is this a video packet? */
      if (p_packet->usStream == us_stream_num) {
        /*
        * Put the packet into the depacketizer. When frames
        * are available, we will get a callback to
        * rv_frame_available().
        */
        ret_val = rv_depack_add_packet(p_depack, p_packet);
      }
      /* Destroy the packet */
      rm_parser_destroy_packet(p_parser, &p_packet);
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  }

  TIME_PRINT(("\n"));

  /* Output the last picture in decoded picture memory pool */
  START_SW_PERFORMANCE;
  decsw_performance();

  if(output_thread_run)
    RvDecEndOfStream(g_RVDec_inst, 1);

  END_SW_PERFORMANCE;
  decsw_performance();

  /* Display results */
  DEBUG_PRINT(("Video stream in '%s' decoding complete: %u input frames, %u output frames\n",
               (const char*) filename, info.ul_num_input_frames, info.ul_num_output_frames));

#ifdef RV_TIME_TRACE
  ul_stop_time = TIMER_GET_NOW_TIME();
  if (ul_stop_time <= ul_start_time)
    info.ul_total_time = (TIMER_MAX_COUNTER-ul_start_time+ul_stop_time)/TIMER_FACTOR_MSECOND;
  else
    info.ul_total_time = (ul_stop_time-ul_start_time)/TIMER_FACTOR_MSECOND;

  TIME_PRINT(("Video stream in '%s' decoding complete:\n", (const char*) filename));
  TIME_PRINT(("    width is %d, height is %d\n",info.ul_width, info.ul_height));
  TIME_PRINT(("    %lu input frames, %lu output frames\n",info.ul_num_input_frames, info.ul_num_output_frames));
  TIME_PRINT(("    total time is %lu ms, avarage time is %lu ms, fps is %6.2f\n",
              info.ul_total_time, info.ul_total_time/info.ul_num_output_frames, 1000.0/(info.ul_total_time/info.ul_num_output_frames)));
#endif

  /* If the error was just a normal "out-of-packets" error,
  then clean it up */
  if (ret_val == HXR_NO_DATA) {
    ret_val = HXR_OK;
  }

cleanup:

  /* Destroy the codec init info */
  add_buffer_thread_run = 0;

  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  if (p_info) {
    rv_depack_destroy_codec_init_info(p_depack, &p_info);
  }
  /* Destroy the depacketizer */
  if (p_depack) {
    rv_depack_destroy(&p_depack);
  }
  /* If we have a packet, destroy it */
  if (p_packet) {
    rm_parser_destroy_packet(p_parser, &p_packet);
  }
  /* Destroy the stream header */
  if (p_hdr) {
    rm_parser_destroy_stream_header(p_parser, &p_hdr);
  }
  /* Destroy the rv_decode object */
  if (p_decode) {
    rv_decode_destroy(p_decode);
    p_decode = HXNULL;
  }
  /* Destroy the parser */
  if (p_parser) {
    rm_parser_destroy(&p_parser);
  }
  /* Close the input file */
  if (fp_in) {
    DWLfclose(fp_in);
    fp_in = HXNULL;
  }
  /* Close the output file */
  if (info.fp_out) {
    DWLfclose(info.fp_out);
    info.fp_out = HXNULL;
  }

  if(f_tiled_output) {
    DWLfclose(f_tiled_output);
    f_tiled_output = NULL;
  }

  /* Close the time log file */
  if (g_fp_time_log) {
    DWLfclose(g_fp_time_log);
    g_fp_time_log = HXNULL;
  }
  /* Release the decoder instance */
  if (g_RVDec_inst) {
    START_SW_PERFORMANCE;
    decsw_performance();
    ReleaseExtBuffers();
    pthread_mutex_destroy(&ext_buffer_contro);
    RvDecRelease(g_RVDec_inst);
    DWLRelease(dwl_inst);
    END_SW_PERFORMANCE;
    decsw_performance();
  }

  FINALIZE_SW_PERFORMANCE;

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif
}

/*------------------------------------------------------------------------------
function
    rv_frame_available()

purpose
    If enough data have been extracted from rm packte(s) for an entire frame,
    this function will be called to decode the available stream data.
------------------------------------------------------------------------------*/

HX_RESULT rv_frame_available(void* p_avail, UINT32 ul_sub_stream_num, rv_frame* p_frame) {
  HX_RESULT ret_val = HXR_FAIL;
  RvDecPicture dec_output;

  if (p_avail && p_frame) {
    /* Get the info pointer */
    rm2yuv_info* p_info = (rm2yuv_info*) p_avail;
    if (p_info->p_depack && p_info->p_decode) {
      /* Put the frame into rv_decode */
      ret_val = rv_decode_stream_input(p_info->p_decode, p_frame);
      if (HX_SUCCEEDED(ret_val)) {
        /* Increment the number of input frames */
        p_info->ul_num_input_frames++;

        /* Decode frames until there aren't any more */
        do {
          /* skip all B-Frames before Frame g_startframeid */
          if (p_info->p_decode->pInputFrame->usSequenceNum < p_info->ul_start_frame_id
              && p_info->p_decode->pInputFrame->ucCodType == 3/*RVDEC_TRUEBPIC*/) {
            ret_val = HXR_OK;
            break;
          }
          ret_val = rv_decode_stream_decode(p_info->p_decode, &dec_output);
          RV_Time_Full_Pause();
          RV_Time_Full_Reset();

          if (HX_SUCCEEDED(ret_val)) {
          } else {
            p_info->b_dec_end = 1;
          }
        } while (HX_SUCCEEDED(ret_val) && rv_decode_more_frames(p_info->p_decode));
      }

      rv_depack_destroy_frame(p_info->p_depack, &p_frame);
    }
  }

  return ret_val;
}


/*------------------------------------------------------------------------------
function
    rv_decode_stream_decode()

purpose
    Calls the decoder backend (HW) to decode issued frame
    and output a decoded frame if any possible.

return
    Returns zero on success, negative result indicates failure.

------------------------------------------------------------------------------*/
HX_RESULT rv_decode_stream_decode(rv_decode *p_front_end, RvDecPicture *output) {
  HX_RESULT ret_val = HXR_FAIL;
  RvDecRet rv_ret;
  RvDecInput dec_input;
  RvDecInfo dec_info;
  RvDecOutput dec_output;
  RvDecContainer *dec_cont = (RvDecContainer *)g_RVDec_inst;
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  RvDecBufferInfo hbuf;
  RvDecRet rv;
  u32 more_data = 1;
  UINT32 size = 0;
  UINT32 i = 0;
  u8 packet_loss = 0;
  RvDecSliceInfo slice_info[128];
  u32 ret = 0;
  struct RvDecConfig dec_cfg;

  INIT_SW_PERFORMANCE;

  /* If enabled, loose the packets (skip this packet first though) */
  if(stream_packet_loss) {

    ret =
      TBRandomizePacketLoss(tb_cfg.tb_params.stream_packet_loss, &packet_loss);
    if(ret != 0) {
      DEBUG_PRINT(("RANDOM STREAM ERROR FAILED\n"));
      return 0;
    }
  }

  if( packet_loss )
    return HXR_OK;

#ifdef RV_ERROR_SIM
  DWLFile *fp = NULL;
  char flnm[0x100] = "";

  if (g_randomerror) {
    CreateError((char *)(p_front_end->pInputFrame->pData + 100), (int)(p_front_end->pInputParams.dataLength), 20);
    sprintf(flnm, "stream_%d.bin", p_front_end->pInputFrame->usSequenceNum);
    fp = DWLfopen(flnm, "w");
    if(fp) {
      DWLfwrite(p_front_end->pInputFrame->pData, 1, p_front_end->pInputParams.dataLength, fp);
      DWLfclose(fp);
    }
  }
#endif

  dec_input.skip_non_reference = skip_non_reference;
  dec_input.stream = p_front_end->pInputFrame->pData;
  dec_input.stream_bus_address = p_front_end->pInputFrame->ulDataBusAddress;
  dec_input.pic_id = p_front_end->pInputFrame->usSequenceNum;
  dec_input.timestamp = p_front_end->pInputFrame->ulTimestamp;
  dec_input.data_len = p_front_end->pInputParams.dataLength;
  dec_input.slice_info_num = p_front_end->pInputParams.numDataSegments+1;
  /* Configure slice_info, which will be accessed by HW */
  size = sizeof(slice_info);
  dec_input.slice_info = slice_info;
  DWLmemset(dec_input.slice_info, 0, size);
  for (i = 0; i < dec_input.slice_info_num; i++) {
    slice_info[i].offset = p_front_end->pInputParams.pDataSegments[i].ulOffset;
    slice_info[i].is_valid = p_front_end->pInputParams.pDataSegments[i].bIsValid;
  }

  if (stream_bit_swap) {
    if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
      /* Picture needs to be ready before corrupting next picture */
      if(pic_rdy && corrupted_bytes <= 0) {
        ret = TBRandomizeBitSwapInStream(dec_input.stream,
                                         dec_input.data_len, tb_cfg.tb_params.stream_bit_swap);
        if(ret != 0) {
          printf("RANDOM STREAM ERROR FAILED\n");
          return 0;
        }

        corrupted_bytes = dec_input.data_len;
        printf("corrupted_bytes %d\n", corrupted_bytes);
      }
    }
  }

  do {
    more_data = 0;

    //printf("\n%d\n",dec_input.timestamp);
    //printf("\n%d\n",dec_input.pic_id);

    RV_Time_Dec_Reset();
    START_SW_PERFORMANCE;
    decsw_performance();
    rv_ret = RvDecDecode(dec_cont, &dec_input, &dec_output);
    END_SW_PERFORMANCE;
    decsw_performance();
    RV_Time_Dec_Pause(0,0);

    printDecodeReturn(rv_ret);
    p_front_end->pOutputParams.notes &= ~RV_DECODE_MORE_FRAMES;

    switch(rv_ret) {
    case RVDEC_HDRS_RDY:
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );
      more_data = 1;

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = RvDecGetInfo(g_RVDec_inst, &dec_info);
      END_SW_PERFORMANCE;
      decsw_performance();
      if(rv_ret) {
        printDecodeReturn(rv_ret);
      }
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
#endif
      ResolvePpParamsOverlap(&params, tb_cfg.pp_units_params,
                             !tb_cfg.pp_params.pipeline_e);

      if (params.fscale_cfg.fixed_scale_enabled) {
        if(!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.coded_width;
          params.ppu_cfg[0].crop.height = dec_info.coded_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }

        u32 crop_w = params.ppu_cfg[0].crop.width;
        u32 crop_h = params.ppu_cfg[0].crop.height;
        params.ppu_cfg[0].scale.width = (crop_w /
                                         params.fscale_cfg.down_scale_x) & ~0x1;
        params.ppu_cfg[0].scale.height = (crop_h /
                                         params.fscale_cfg.down_scale_y) & ~0x1;
        params.ppu_cfg[0].scale.enabled = 1;
        params.ppu_cfg[0].scale.ratio_x |= 0;
        params.ppu_cfg[0].scale.ratio_y |= 0;
        params.ppu_cfg[0].enabled = 1;
        params.pp_enabled = 1;
      }
      pp_enabled = params.pp_enabled;
      memcpy(dec_cfg.ppu_config, params.ppu_cfg, sizeof(params.ppu_cfg));
      dec_cfg.align = align;
      dec_cfg.cr_first = cr_first;
      u32 tmp = RvDecSetInfo(g_RVDec_inst, &dec_cfg);
      if (tmp != RVDEC_OK)
        return ret_val;
      printf("*********************** RvDecGetInfo() **********************\n");
      printf("dec_info.frame_width %d\n",(dec_info.frame_width << 4));
      printf("dec_info.frame_height %d\n",(dec_info.frame_height << 4));
      printf("dec_info.coded_width %d\n",dec_info.coded_width);
      printf("dec_info.coded_height %d\n",dec_info.coded_height);
      if(dec_info.output_format == RVDEC_SEMIPLANAR_YUV420)
        printf("dec_info.output_format RVDEC_SEMIPLANAR_YUV420\n");
      else
        printf("dec_info.output_format RVDEC_TILED_YUV420\n");
      printf("*************************************************************\n");
#if 0
      if(dec_info.pic_buff_size != min_buffer_num ||
          (dec_info.frame_width * dec_info.frame_height > prev_width * prev_height)) {
        /* Reset buffers added and stop adding extra buffers when a new header comes. */
        if (pp_enabled)
          res_changed = 1;
        else {
          add_extra_flag = 0;
          ReleaseExtBuffers();
          buffer_release_flag = 1;
          num_buffers = 0;
        }
      }
#endif
      //prev_width = dec_info.frame_width;
      //prev_height = dec_info.frame_height;
      //min_buffer_num = dec_info.pic_buff_size;
      hdrs_rdy = 1;
      break;
    case RVDEC_WAITING_FOR_BUFFER:
      more_data = 1;
      rv = RvDecGetBufferInfo(g_RVDec_inst, &hbuf);
      printf("RvDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;

        for(i = 0; i < hbuf.buf_num; i++) {
          if (pp_enabled)
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          else
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          rv = RvDecAddBuffer(g_RVDec_inst, &mem);
          printf("RvDecAddBuffer ret %d\n", rv);

          if(rv != RVDEC_OK && rv != RVDEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i < (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num - 1;
        add_extra_flag = 1;
      }
      break;

    case RVDEC_STRM_PROCESSED:
      //if (dec_input.data_len == dec_output.data_left)
      //  more_data = 1;
    case RVDEC_BUF_EMPTY:
    case RVDEC_NONREF_PIC_SKIPPED:
    case RVDEC_NO_DECODING_BUFFER:
      /* In case picture size changes we need to reinitialize memory model */
      TBSetRefbuMemModel( &tb_cfg,
                          ((RvDecContainer *) g_RVDec_inst)->rv_regs,
                          &((RvDecContainer *) g_RVDec_inst)->ref_buffer_ctrl );
      START_SW_PERFORMANCE;
      decsw_performance();
      rv_ret = ((RvDecContainer *) g_RVDec_inst)->output_stat;
      output = &((RvDecContainer *) g_RVDec_inst)->out_pic;
      END_SW_PERFORMANCE;
      decsw_performance();
      if (rv_ret == RVDEC_PIC_RDY) {
        p_front_end->pOutputParams.width = output->pictures[0].coded_width;
        p_front_end->pOutputParams.height = output->pictures[0].coded_height;
        p_front_end->pOutputParams.notes &= ~RV_DECODE_DONT_DRAW;
        p_front_end->pOutputParams.notes |= RV_DECODE_MORE_FRAMES;
        ret_val = HXR_OK;

        /* update frame number */
        frame_number++;
      } else if (rv_ret == RVDEC_OK) {
        p_front_end->pOutputParams.notes |= RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;
      } else {
        ret_val = HXR_FAIL;
      }

      break;

    case RVDEC_OK:
    case RVDEC_PIC_DECODED:
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = RvDecAbort(g_rvdec_inst);
        rv_ret = RvDecAbortAfter(g_rvdec_inst);
      }
#endif

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      if (use_peek_output) {
        rv_ret = RvDecPeek(g_RVDec_inst, output);
      } else {
        START_SW_PERFORMANCE;
        decsw_performance();
        rv_ret = ((RvDecContainer *) g_RVDec_inst)->output_stat;
        output = &((RvDecContainer *) g_RVDec_inst)->out_pic;
        END_SW_PERFORMANCE;
        decsw_performance();
      }

      if (rv_ret == RVDEC_PIC_RDY) {
        p_front_end->pOutputParams.width = output->pictures[0].coded_width;
        p_front_end->pOutputParams.height = output->pictures[0].coded_height;
        p_front_end->pOutputParams.notes &= ~RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;

        /* pic coding type */
        printRvPicCodingType(output->pic_coding_type);

        /* update frame number */
        frame_number++;
      } else if (rv_ret == RVDEC_OK) {
        p_front_end->pOutputParams.notes |= RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;
      } else {
        DEBUG_PRINT(("Error, Decode end!\n"));
        ret_val = HXR_FAIL;
      }

      pic_rdy = 1;
      corrupted_bytes = 0;
      break;
    default:
      /*
      * RVDEC_NOT_INITIALIZED
      * RVDEC_PARAM_ERROR
      * RVDEC_STRM_ERROR
      * RVDEC_STRM_PROCESSED
      * RVDEC_SYSTEM_ERROR
      * RVDEC_MEMFAIL
      * RVDEC_HW_TIMEOUT
      * RVDEC_HW_RESERVED
      * RVDEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("Fatal Error, Decode end!\n"));
      ret_val = HXR_FAIL;
      break;
    }
  } while(more_data);

  FINALIZE_SW_PERFORMANCE;
  return ret_val;
}

/*------------------------------------------------------------------------------
function
    rv_decode_frame_flush()

purpose
    update rm2yuv_info
    write the decoded picture to a local file or display on screen, if neccesary
------------------------------------------------------------------------------*/
void rv_decode_frame_flush(rm2yuv_info *p_info, RvDecPicture *output) {
  UINT32 ul_width, ul_height, ul_out_frame_size;
  u32 i, pp_planar, pp_cr_first, pp_mono_chrome;
  if (p_info == NULL || output == NULL)
    return;

  /* Is there a valid output frame? */
  {
    /* Increment the number of output frames */
    p_info->ul_num_output_frames++;
    /* Get the dimensions and size of output frame */
    ul_width = output->pictures[0].coded_width;
    ul_height = output->pictures[0].coded_height;
    ul_out_frame_size = (ul_width * ul_height * 12) >> 3;
    /* Check to see if dimensions have changed */
    if (ul_width != p_info->ul_width || ul_height != p_info->ul_height ) {
      DEBUG_PRINT(("Warning: YUV output dimensions changed from "
                   "%dx%d to %dx%d at frame #: %d (in decoding order)\n",
                   p_info->ul_width, p_info->ul_height, ul_width, ul_height, output->pic_id));
      /* Don't bother resizing to display dimensions */
      p_info->ul_width = ul_width;
      p_info->ul_height = ul_height;
      p_info->ul_out_frame_size = ul_out_frame_size;
      /* close the opened output file handle if necessary */
      /*
      if (p_info->fp_out)
      {
          DWLfclose(p_info->fp_out);
          p_info->fp_out = HXNULL;
      }
      */
      /* clear the flag of first display */
      g_bfirstdisplay = 0;
    }

    if(g_iswritefile) {
#if 0
      if (p_info->fp_out == HXNULL) {
        char flnm[0x100] = "";
        if(enable_frame_picture && !frame_out) {
          sprintf(flnm, "frame.yuv",
                  /*p_info->file_path,*/ max_coded_width, max_coded_height);
          frame_out = DWLfopen(flnm, "w");
          frame_buffer = (u8*)malloc(max_frame_width*max_frame_height*3/2);
        }
        if( *out_file_name == '\0' ) {
          sprintf(out_file_name, "out_%dx%d.yuv",
                  /*p_info->file_path,*/
                  MB_MULTIPLE(p_info->ul_width),
                  MB_MULTIPLE(p_info->ul_height));
        }
        p_info->fp_out = DWLfopen(out_file_name, "w");
#endif
#if 0
        if(f_tiled_output == NULL && tiled_output) {
          /* open output file for writing, can be disabled with define.
           * If file open fails -> exit */
          if(strcmp(out_file_name_tiled, "none") != 0) {
            f_tiled_output = fopen(out_file_name_tiled, "wb");
            if(f_tiled_output == NULL) {
              printf("UNABLE TO OPEN TILED OUTPUT FILE\n");
              return;
            }
          }
        }

#endif
#if 0
      }
      if (pp_enabled) {
        u32 pic_width = output->coded_width;  /* valid bytes */
        u32 pic_stride = output->frame_width;
        u32 md5sum = 0;
        u32 i;
#ifdef MD5SUM
        md5sum = 1;
#endif
        if (md5sum) {
          TBWriteFrameMD5SumValidOnly2(p_info->fp_out, data,
                                      data + pic_stride * output->frame_height,
                                      pic_width, output->coded_height,
                                      pic_stride, pic_stride, output->frame_height, 0,
                                      p_info->ul_num_output_frames);
        } else {
          for (i = 0; i < output->coded_height; i++) {
            fwrite(data, 1, pic_width, p_info->fp_out);
            data += pic_stride;
          }
          data += (output->frame_height - output->coded_height) * pic_stride;
          for (i = 0; i < output->coded_height / 2; i++) {
            fwrite(data, 1, pic_width, p_info->fp_out);
            data += pic_stride;
          }
          fflush(p_info->fp_out);
        }
        if(out_data)
          free(out_data);
        if(raster_scan)
          free(raster_scan);
        return;
      }
      if (p_info->fp_out) {
#ifndef ASIC_TRACE_SUPPORT
        u8* buffer = data;
        if(output_picture_endian == DEC_X170_BIG_ENDIAN) {
          u32 frame_size = MB_MULTIPLE(p_info->ul_width)*MB_MULTIPLE(p_info->ul_height)*3/2;
          if(pic_big_endian_size != frame_size) {
            if(pic_big_endian != NULL)
              free(pic_big_endian);

            pic_big_endian = (u8 *) malloc(frame_size);
            if(pic_big_endian == NULL) {
              DEBUG_PRINT(("MALLOC FAILED @ %s %d", __FILE__, __LINE__));
              if(raster_scan)
                free(raster_scan);
              return;
            }

            pic_big_endian_size = frame_size;
          }

          memcpy(pic_big_endian, buffer, frame_size);
          TbChangeEndianess(pic_big_endian, frame_size);
          data = pic_big_endian;
        }
#endif
#ifndef MD5SUM
        if( enable_frame_picture ) {
          u32 tmp = max_coded_width * max_coded_height, i;
          FramePicture( data,
                        p_info->ul_width, p_info->ul_height,
                        MB_MULTIPLE(p_info->ul_width),
                        MB_MULTIPLE(p_info->ul_height),
                        frame_buffer, max_coded_width, max_coded_height );

          DWLfwrite(frame_buffer, 1, 3*tmp/2, frame_out);
          /*
          for(i = 0; i < tmp / 4; i++)
              DWLfwrite(frame_buffer + tmp + i * 2, 1, 1, frame_out);
          for(i = 0; i < tmp / 4; i++)
              DWLfwrite(frame_buffer + tmp + 1 + i * 2, 1, 1, frame_out);
              */
        }
#endif

#ifdef MD5SUM
        {
          u32 tmp = max_coded_width * max_coded_height, i;
          FramePicture( data,
                        p_info->ul_width, p_info->ul_height,
                        MB_MULTIPLE(p_info->ul_width),
                        MB_MULTIPLE(p_info->ul_height),
                        frame_buffer, max_coded_width, max_coded_height );

          /* TODO: frame counter */
          if(planar_output) {
            TBWriteFrameMD5Sum(p_info->fp_out, frame_buffer,
                               max_coded_width*
                               max_coded_height*3/2,
                               p_info->ul_num_output_frames);
          } else {
            TBWriteFrameMD5Sum(p_info->fp_out, data,
                               MB_MULTIPLE(p_info->ul_width)*
                               MB_MULTIPLE(p_info->ul_height)*3/2,
                               p_info->ul_num_output_frames);
          }
        }


#if 0
        /* tiled */
        if (tiled_output) {
          /* round up to next multiple of 16 */
          u32 tmp_width = MB_MULTIPLE(p_info->ul_width);
          u32 tmp_height = MB_MULTIPLE(p_info->ul_height);

          TbWriteTiledOutput(f_tiled_output, output->output_picture, tmp_width >> 4, tmp_height >> 4,
                             number_of_written_frames, 1 /* write md5sum */, 1 /* semi-planar data */ );
        }
#endif

#else
        if( !planar_output ) {
          DWLfwrite(data, 1,
                    MB_MULTIPLE(p_info->ul_width)*
                    MB_MULTIPLE(p_info->ul_height)*3/2,
                    p_info->fp_out);
        } else {
          u32 tmp = MB_MULTIPLE(p_info->ul_width) *
                    MB_MULTIPLE(p_info->ul_height), i;
          DWLfwrite(data, 1, tmp, p_info->fp_out);
          for(i = 0; i < tmp / 4; i++)
            DWLfwrite(data + tmp + i * 2, 1, 1, p_info->fp_out);
          for(i = 0; i < tmp / 4; i++)
            DWLfwrite(data + tmp + 1 + i * 2, 1, 1, p_info->fp_out);
        }

#if 0
        /* tiled */
        if (tiled_output) {
          /* round up to next multiple of 16 */
          u32 tmp_width = MB_MULTIPLE(p_info->ul_width);
          u32 tmp_height = MB_MULTIPLE(p_info->ul_height);

          TbWriteTiledOutput(f_tiled_output, output->output_picture, tmp_width >> 4, tmp_height >> 4,
                             number_of_written_frames, 0 /* not write md5sum */, 1 /* semi-planar data */ );
        }
#endif

#endif
        number_of_written_frames++;

#ifndef ASIC_TRACE_SUPPORT
        data = buffer;
#endif
      }
#endif
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
        if (!output->pictures[i].frame_width ||
            !output->pictures[i].frame_height)
          continue;
        pp_planar      = 0;
        pp_cr_first    = 0;
        pp_mono_chrome = 0;
        if(IS_PIC_PLANAR(output->pictures[i].output_format))
          pp_planar = 1;
        else if(IS_PIC_NV21(output->pictures[i].output_format))
          pp_cr_first = 1;
        else if(IS_PIC_MONOCHROME(output->pictures[i].output_format))
          pp_mono_chrome = 1;
        if (!IS_PIC_PACKED_RGB(output->pictures[i].output_format) &&
            !IS_PIC_PLANAR_RGB(output->pictures[i].output_format))
          WriteOutput(out_file_name[i], (u8*)output->pictures[i].output_picture,
                      output->pictures[i].coded_width,
                      output->pictures[i].coded_height,
                      p_info->ul_num_output_frames - 1,
                      pp_mono_chrome, 0, 0, 0,
                      IS_PIC_TILE(output->pictures[i].output_format),
                      output->pictures[i].pic_stride, output->pictures[i].pic_stride_ch, i,
                      pp_planar, pp_cr_first, convert_tiled_output,
                      0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
        else
          WriteOutputRGB(out_file_name[i], (u8*)output->pictures[i].output_picture,
                         output->pictures[i].coded_width,
                         output->pictures[i].coded_height,
                         p_info->ul_num_output_frames - 1,
                         pp_mono_chrome, 0, 0, 0,
                         IS_PIC_TILE(output->pictures[i].output_format),
                         output->pictures[i].pic_stride, output->pictures[i].pic_stride_ch, i,
                         IS_PIC_PLANAR_RGB(output->pictures[i].output_format), pp_cr_first,
                         convert_tiled_output, 0, DEC_DPB_FRAME, md5sum, &fout[i], 1);
    }
  }
#ifdef RV_DISPLAY
    g_srcimg.cx = output->pictures[0].frame_width;  // 16-pixel up rounding
    g_srcimg.cy = output->pictures[0].frame_height; // 16-pixel up rounding
    if(g_isdisplay) {
      displayOnScreen(output);
    }
#endif

    DEBUG_PRINT(("Get frame %d.\n", p_info->ul_num_output_frames));
    if (p_info->ul_max_num_dec_frames && (p_info->ul_num_output_frames >= p_info->ul_max_num_dec_frames)) {
      p_info->b_dec_end = 1;
    }
#if 0
    if(out_data)
      free(out_data);
    if(raster_scan)
      free(raster_scan);
#endif
  }
}

/*------------------------------------------------------------------------------
function
    rm2yuv_error

purpose
    print error message
------------------------------------------------------------------------------*/
void rm2yuv_error(void* p_error, HX_RESULT result, const char* psz_msg) {
  p_error = p_error;
  result = result;
  psz_msg = psz_msg;
  ERROR_PRINT(("rm2yuv_error p_error=%p result=0x%08x msg=%s\n", p_error, result, psz_msg));
}

/*------------------------------------------------------------------------------
function
    printDecodeReturn

purpose
    Print out decoder return value
------------------------------------------------------------------------------*/
static void printDecodeReturn(i32 retval) {
  static i32 prev_ret = -0xDEAD;
  if (prev_ret == retval) return;
  prev_ret = retval;
  DEBUG_PRINT(("RvDecDecode returned: "));
  switch(retval) {
  case RVDEC_OK:
    DEBUG_PRINT(("RVDEC_OK\n"));
    break;
  case RVDEC_PIC_DECODED:
    DEBUG_PRINT(("RVDEC_PIC_DECODED\n"));
    break;
  case RVDEC_HDRS_RDY:
    DEBUG_PRINT(("RVDEC_HDRS_RDY\n"));
    break;
  case RVDEC_NONREF_PIC_SKIPPED:
    DEBUG_PRINT(("RVDEC_NONREF_PIC_SKIPPED\n"));
    break;
  case RVDEC_STRM_PROCESSED:
    DEBUG_PRINT(("RVDEC_STRM_PROCESSED\n"));
    break;
  case RVDEC_NO_DECODING_BUFFER:
    DEBUG_PRINT(("RVDEC_NO_DECODING_BUFFER\n"));
    break;
  case RVDEC_BUF_EMPTY:
    DEBUG_PRINT(("RVDEC_BUF_EMPTY\n"));
    break;
  case RVDEC_STRM_ERROR:
    DEBUG_PRINT(("RVDEC_STRM_ERROR\n"));
    break;
  case RVDEC_HW_TIMEOUT:
    DEBUG_PRINT(("RVDEC_HW_TIMEOUT\n"));
    break;
  case RVDEC_HW_BUS_ERROR:
    DEBUG_PRINT(("RVDEC_HW_BUS_ERROR\n"));
    break;
  case RVDEC_HW_RESERVED:
    DEBUG_PRINT(("RVDEC_HW_RESERVED\n"));
    break;
  case RVDEC_PARAM_ERROR:
    DEBUG_PRINT(("RVDEC_PARAM_ERROR\n"));
    break;
  case RVDEC_NOT_INITIALIZED:
    DEBUG_PRINT(("RVDEC_NOT_INITIALIZED\n"));
    break;
  case RVDEC_SYSTEM_ERROR:
    DEBUG_PRINT(("RVDEC_SYSTEM_ERROR\n"));
    break;
  default:
    DEBUG_PRINT(("Other %d\n", retval));
    break;
  }
}

/*------------------------------------------------------------------------------

    Function name:            printRvPicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
static void printRvPicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf("DEC_PIC_TYPE_I, ");
    break;
  case DEC_PIC_TYPE_P:
    printf("DEC_PIC_TYPE_P, ");
    break;
  case DEC_PIC_TYPE_B:
    printf("DEC_PIC_TYPE_B, ");
    break;
  case DEC_PIC_TYPE_FI:
    printf("DEC_PIC_TYPE_FI, ");
    break;
  default:
    printf("Other %d\n", pic_type);
    break;
  }
}

/*------------------------------------------------------------------------------
function
    parse_path

purpose
    parse filename and path

parameter
    path(I):    full path of input file
    dir(O):     directory path
    file(O):    file name
-----------------------------------------------------------------------------*/
void parse_path(const char *path, char *dir, char *file) {
  int len = 0;
  const char *ptr = NULL;

  if (path == NULL)
    return;

  len = strlen(path);
  if (len == 0)
    return;

  ptr = path + len - 1;    // point to the last char
  while (ptr != path) {
    if ((*ptr == '\\') || (*ptr == '/'))
      break;

    ptr--;
  }

  if (file != NULL) {
    if (ptr != path)
      strcpy(file, ptr + 1);
    else
      strcpy(file, ptr);
  }

  if ((ptr != path) && (dir != NULL)) {
    len = ptr - path + 1;    // with the "/" or "\"
    DWLmemcpy(dir, path, len);
    dir[len] = '\0';
  }
}

/*------------------------------------------------------------------------------
function
    rm_io_read

purpose
    io read interface for rm parse/depack
-----------------------------------------------------------------------------*/
UINT32 rm_io_read(void* p_user_read, BYTE* p_buf, UINT32 ul_bytes_to_read) {
  UINT32 ul_ret = 0;

  if (p_user_read && p_buf && ul_bytes_to_read) {
    /* The void* is a DWLFile* */
    DWLFile *fp = (DWLFile *) p_user_read;
    /* Read the number of bytes requested */
    ul_ret = (UINT32) DWLfread(p_buf, 1, ul_bytes_to_read, fp);
  }

  return ul_ret;
}

/*------------------------------------------------------------------------------
function
    rm_io_seek

purpose
    io seek interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_io_seek(void* p_user_read, UINT32 ulOffset, UINT32 ul_origin) {
  if (p_user_read) {
    /* The void* is a DWLFile* */
    DWLFile *fp = (DWLFile *) p_user_read;
    /* Do the seek */
    DWLfseek(fp, ulOffset, ul_origin);
  }
}

/*------------------------------------------------------------------------------
function
    rm_memory_malloc

purpose
    memory (sw only) allocation interface for rm parse/depack
-----------------------------------------------------------------------------*/
void* rm_memory_malloc(void* p_user_mem, UINT32 ul_size) {
  p_user_mem = p_user_mem;

  return (void*)DWLmalloc(ul_size);
}

/*------------------------------------------------------------------------------
function
    rm_memory_free

purpose
    memory (sw only) free interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_memory_free(void* p_user_mem, void* ptr) {
  p_user_mem = p_user_mem;

  DWLfree(ptr);
}

/*------------------------------------------------------------------------------
function
    rm_memory_create_ncnb

purpose
    memory (sw/hw share) allocation interface for rm parse/depack
-----------------------------------------------------------------------------*/
void* rm_memory_create_ncnb(void* p_user_mem, UINT32 ul_size) {
  struct DWLLinearMem *p_buf = NULL;
  const void *dwl = dwl_inst;

  p_user_mem = p_user_mem;

  ASSERT(dwl != NULL);

  p_buf = (struct DWLLinearMem *)DWLmalloc(sizeof(struct DWLLinearMem));
  if (p_buf == NULL)
    return NULL;

  if (DWLMallocLinear(dwl, ul_size, p_buf)) {
    DWLfree(p_buf);
    return NULL;
  } else {
    return p_buf;
  }
}

/*------------------------------------------------------------------------------
function
    rm_memory_destroy_ncnb

purpose
    memory (sw/hw share) release interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_memory_destroy_ncnb(void* p_user_mem, void* ptr) {
  struct DWLLinearMem *p_buf = NULL;
  const void *dwl = dwl_inst;

  p_user_mem = p_user_mem;

  ASSERT(dwl != NULL);

  if (ptr == NULL)
    return;

  p_buf = *(struct DWLLinearMem **)ptr;

  if (p_buf == NULL)
    return;

  DWLFreeLinear(dwl, p_buf);
  DWLfree(p_buf);
  *(struct DWLLinearMem **)ptr = NULL;
}

/*------------------------------------------------------------------------------
Function name:  RvDecTrace()

Functional description:
    This implementation appends trace messages to file named 'dec_api.trc'.

Argument:
    string - trace message, a null terminated string
------------------------------------------------------------------------------*/

void RvDecTrace(const char *str) {
  DWLFile *fp = NULL;

  fp = DWLfopen("dec_api.trc", "a");
  if(!fp)
    return;

  DWLfwrite(str, 1, strlen(str), fp);
  DWLfwrite("\n", 1, 1, fp);

  DWLfclose(fp);
}

#ifdef RV_DISPLAY
void displayOnScreen(RvDecPicture *p_dec_picture) {
  p_dec_picture = p_dec_picture;

#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  f_time_LCD = 0.0;
  ul_start_time_LCD = TIMER_GET_NOW_TIME();
#endif

  VIM_DISPLAY((g_srcimg, g_dstimg, (char *)p_dec_picture->pData, &g_bfirstdisplay,
               g_delay_time, g_rotationmode, g_blayerenable));

#ifdef RV_TIME_TRACE
  ul_stop_time_LCD = TIMER_GET_NOW_TIME();
  if (ul_stop_time_LCD <= ul_start_time_LCD)
    f_time_LCD += (double)((TIMER_MAX_COUNTER-ul_start_time_LCD+ul_stop_time_LCD)/TIMER_FACTOR_MSECOND);
  else
    f_time_LCD += (double)((ul_stop_time_LCD-ul_start_time_LCD)/TIMER_FACTOR_MSECOND);

  if(f_time_LCD > f_max_time_LCD)
    f_max_time_LCD = f_time_LCD;

  f_total_time_LCD += f_time_LCD;

  if (g_islog2std) {
    TIME_PRINT((" | \tDisplay:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", p_dec_picture->pic_id, 0, f_time_LCD));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, " | \tDisplay:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", p_dec_picture->pic_id, 0, f_time_LCD);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }

  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}
#endif

void RV_Time_Init(i32 islog2std, i32 islog2file) {
#ifdef RV_TIME_TRACE
  g_islog2std = islog2std;
  g_islog2file = islog2file;

  if (g_islog2file) {
#ifdef WIN32
    g_fp_time_log = DWLfopen("rvtime.log", "wb");
#else
    g_fp_time_log = DWLfopen("rvtime.log", "w");
#endif
  }

  f_max_time_DEC = 0.0;
  f_max_time_LCD = 0.0;
  f_max_time_Full = 0.0;
  f_total_time_DEC = 0.0;
  f_total_time_LCD = 0.0;
  f_total_time_Full = 0.0;
#endif
}

void RV_Time_Full_Reset(void) {
#ifdef RV_TIME_TRACE
  f_time_Full = 0.0;
  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}

void RV_Time_Full_Pause(void) {
#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  if(f_time_Full > f_max_time_Full)
    f_max_time_Full = f_time_Full;

  f_total_time_Full += f_time_Full;

  if (g_islog2std) {
    TIME_PRINT((" | \tFull_time= %-6.0f ms", f_time_Full));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, " | \tFull_time= %-6.0f ms", f_time_Full);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }
#endif
}

void RV_Time_Dec_Reset(void) {
#ifdef RV_TIME_TRACE
  f_time_DEC = 0.0;
  ul_start_time_DEC = TIMER_GET_NOW_TIME();
#endif
}

void RV_Time_Dec_Pause(u32 picid, u32 pictype) {
#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  ul_stop_time_DEC = TIMER_GET_NOW_TIME();
  if (ul_stop_time_DEC <= ul_start_time_DEC)
    f_time_DEC += (double)((TIMER_MAX_COUNTER-ul_start_time_DEC+ul_stop_time_DEC)/TIMER_FACTOR_MSECOND);
  else
    f_time_DEC += (double)((ul_stop_time_DEC-ul_start_time_DEC)/TIMER_FACTOR_MSECOND);

  if(f_time_DEC > f_max_time_DEC)
    f_max_time_DEC = f_time_DEC;

  f_total_time_DEC += f_time_DEC;

  if (g_islog2std) {
    TIME_PRINT(("\nDecode:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", picid, pictype, f_time_DEC));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, "\nDecode:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", picid, pictype, f_time_DEC);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }

  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}

int main(int argc, char *argv[]) {
  int startframeid = 0;
  int maxnumber = 1000000000;
  int iswritefile = 1;
  int isdisplay = 0;
  int displaymode = 0;
  int rotationmode = 0;
  int islog2std = 1;
  int islog2file = 0;
  int blayerenable = 0;
  int errconceal = 0;
  int randomerror = 0;
  int i, j;
  FILE *f_tbcfg;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);

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

  SetupDefaultParams(&params);
  /* set test bench configuration */
#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 10000; // G1
#endif

  if (argc < 2) {
    printf("\nReal Decoder Testbench\n");
    printf("Usage: %s [options] stream.rm\n", argv[0]);
    printf("\t-Nn forces decoding to stop after n pictures\n");
    printf("\t-Ooutfile write output to \"outfile\" (default out_wxxxhyyy.yuv)\n");
    printf("\t-X disable output file writing\n");
    printf("\t-P write planar output(--planar)\n");
    printf("\t-a Enable PP planar output(--pp-planar)\n");
    printf("\t-E use tiled reference frame format.\n");
    printf("\t-G convert tiled output pictures to raster scan\n");
    printf("\t-Bn to use n frame buffers in decoder\n");
    printf("\t-Q skip decoding non-reference pictures\n");
    printf("\t-Z output pictures using RvDecPeek() function\n");
    printf("\t-V input file in RealVideo format\n");
    printf("\t-m Output md5 for each picture.(--md5-per-pic)\n");
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
    printRvVersion();
    return -1;
  }

  /* read command line arguments */
  for(i = 1; i < (u32) (argc - 1); i++) {
    if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_pics = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      for (j = 0; j < DEC_MAX_PPU_COUNT; j++)
        strcpy(out_file_name[j], argv[i] + 2);
    } else if(strcmp(argv[i], "-X") == 0) {
      disable_output_writing = 1;
    } else if(strcmp(argv[i], "-P") == 0 ||
              strcmp(argv[i], "--planar") == 0) {
      planar_output = 1;
    } else if(strcmp(argv[i], "-a") == 0 ||
              strcmp(argv[i], "--pp-planar") == 0) {
      pp_planar_out = 1;
      //tb_cfg.pp_units_params[0].planar = pp_planar_out;
      params.ppu_cfg[0].enabled = 1;
      params.ppu_cfg[0].planar = 1;
      pp_enabled = 1;
    } else if (strncmp(argv[i], "-E", 2) == 0)
      tiled_output = DEC_REF_FRM_TILED_DEFAULT;
    else if (strncmp(argv[i], "-G", 2) == 0)
      convert_tiled_output = 1;
    else if(strncmp(argv[i], "-B", 2) == 0) {
      num_frame_buffers = atoi(argv[i] + 2);
      if(num_frame_buffers > 16)
        num_frame_buffers = 16;
    } else if(strcmp(argv[i], "-V") == 0) {
      rv_input = 1;
    } else if(strcmp(argv[i], "-Z") == 0) {
      use_peek_output = 1;
    } else if(strcmp(argv[i], "-m") == 0 ||
              strcmp(argv[i], "--md5-per-pic") == 0) {
      md5sum = 1;
    } else if(strcmp(argv[i], "-Q") == 0) {
      skip_non_reference = 1;
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

  printRvVersion();

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
    g_hw_ver = tb_cfg.dec_params.hw_version;
#endif
  }
#ifdef ASIC_TRACE_SUPPORT
#ifdef SUPPORT_CACHE
    if (tb_cfg.cache_enable || tb_cfg.shaper_enable)
      tb_cfg.dec_params.cache_support = 1;
    else
      tb_cfg.dec_params.cache_support = 0;
#endif
#endif
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
  DEBUG_PRINT(("TB Packet by Packet  %d\n", packetize));
  DEBUG_PRINT(("TB Seed Rnd %d\n", seed_rnd));
  DEBUG_PRINT(("TB Stream Truncate %d\n", stream_truncate));
  DEBUG_PRINT(("TB Stream Header Corrupt %d\n", stream_header_corrupt));
  DEBUG_PRINT(("TB Stream Bit Swap %d; odds %s\n", stream_bit_swap,
               tb_cfg.tb_params.stream_bit_swap));
  DEBUG_PRINT(("TB Stream Packet Loss %d; odds %s\n", stream_packet_loss,
               tb_cfg.tb_params.stream_packet_loss));

  if (rv_input) {
    rv_mode(argv[argc-1],
            NULL,
            startframeid,
            maxnumber,
            iswritefile,
            isdisplay,
            displaymode,
            rotationmode,
            islog2std,
            islog2file,
            blayerenable,
            errconceal,
            randomerror);
    if(frame_out) {
      fclose(frame_out);
      free(frame_buffer);
    }
    return 0;
  }
  rv_display(argv[argc-1],
             NULL,
             startframeid,
             maxnumber,
             iswritefile,
             isdisplay,
             displaymode,
             rotationmode,
             islog2std,
             islog2file,
             blayerenable,
             errconceal,
             randomerror);

  if(frame_out) {
    fclose(frame_out);
    free(frame_buffer);
  }

  return 0;
}

/*------------------------------------------------------------------------------

    FramePicture

        Create frame of max-coded-width*max-coded-height around output image.
        Useful for system model verification, this way we can directly compare
        our output to the DecRef_* output generated by the reference decoder..

------------------------------------------------------------------------------*/
void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height ) {

  /* Variables */

  i32 x, y;
  u8 *p_out_cb, *p_out_cr;

  /* Code */

  memset( p_out, 128, out_width*out_height*3/2 );

  /* Luma */
  for ( y = 0 ; y < in_height ; ++y ) {
    for( x = 0 ; x < in_width; ++x )
      *p_out++ = *p_in++;
    p_in += ( in_frame_width - in_width );
    p_out += ( out_width - in_width );
  }

  p_in += in_frame_width * ( in_frame_height - in_height );
  p_out += out_width * ( out_height - in_height );

  in_frame_height /= 2;
  in_frame_width /= 2;
  out_height /= 2;
  out_width /= 2;
  in_height /= 2;
  in_width /= 2;

  /* Chroma */
  /*p_out += 2 * out_width * ( out_height - in_height );*/
  p_out_cb = p_out;
  p_out_cr = p_out + out_width*out_height;
  for ( y = 0 ; y < in_height ; ++y ) {
    for( x = 0 ; x < in_width; ++x ) {
      *p_out_cb++ = *p_in++;
      *p_out_cr++ = *p_in++;
    }
    p_in += 2 * ( in_frame_width - in_width );
    p_out_cb += ( out_width - in_width );
    p_out_cr += ( out_width - in_width );
  }
}

/*------------------------------------------------------------------------------
        printRvVersion
        Description : Print out decoder version info
------------------------------------------------------------------------------*/

void printRvVersion(void) {
  RvDecApiVersion dec_version;
  RvDecBuild dec_build;

  /*
   * Get decoder version info
   */

  dec_version = RvDecGetAPIVersion();
  printf("\nApi version:  %d.%d, ", dec_version.major, dec_version.minor);

  dec_build = RvDecGetBuild();
  printf("sw build nbr: %d, hw build nbr: %x\n\n",
         dec_build.sw_build, dec_build.hw_build);
}
