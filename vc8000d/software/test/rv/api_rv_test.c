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

#include "on2rvdecapi.h"
#include "rv_container.h"

#include "rm_parse.h"
#include "rv_depack.h"
#include "rv_decode.h"

#include "tb_cfg.h"
#include "tb_tiled.h"

#include "regdrv.h"

#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
extern u32 g_hw_ver;
#endif

#ifdef RV_EVALUATION
extern u32 g_hw_ver;
#endif

#include "tb_md5.h"

#include "tb_sw_performance.h"

#define RM2YUV_INITIAL_READ_SIZE  16
#define MAX_BUFFERS 16

#define MB_MULTIPLE(x)  (((x)+15)&~15)

u32 max_coded_width = 0;
u32 max_coded_height = 0;
u32 max_frame_width = 0;
u32 max_frame_height = 0;
u32 enable_frame_picture = 1;
u8 *frame_buffer;
FILE *frame_out = NULL;
char out_file_name[256] = "";
char out_file_name_tiled[256] = "out_tiled.yuv";
u32 disable_output_writing = 0;
u32 planar_output = 0;
u32 tiled_output = 0;
u32 frame_number = 0;
u32 number_of_written_frames = 0;
u32 num_frame_buffers = 0;
u32 skip_non_reference = 0;
u32 md5sum = 0;
FILE *f_tiled_output = NULL;
void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height );
void ppFramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height );

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

#define DEBUG_PRINT(msg)    printf msg
#define ERROR_PRINT(msg)
#ifndef ASSERT
#define ASSERT(s)
#endif /* ASSERT */

struct TBCfg tb_cfg;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 output_format = DEC_X170_OUTPUT_FORMAT;

u32 stream_truncate = 0;
u32 stream_packet_loss = 0;
u32 stream_header_corrupt = 0;
u32 hdrs_rdy = 0;

u32 seed_rnd = 0;
u32 stream_bit_swap = 0;
i32 corrupted_bytes = 0;  /*  */
u32 pic_rdy = 0;
u32 packetize = 0;


static u32 g_iswritefile = 0;
static u32 g_isdisplay = 0;
static u32 g_displaymode = 0;
static u32 g_bfirstdisplay = 0;
static u32 g_rotationmode = 0;
static u32 g_blayerenable = 0;
static u32 g_islog2std = 0;
static u32 g_islog2file = 0;
static DWLFile *g_fp_time_log = NULL;

static RvDecInst g_RVDec_inst = NULL;

typedef struct {
  char         file_path[0x100];
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
  UINT32       ul_start_frame_id;  /* 0-based */
  UINT32       b_dec_end;
  UINT32       ul_total_time;   /* ms */
} rm2yuv_info;

static HX_RESULT rv_frame_available(void* p_avail, UINT32 ul_sub_stream_num, rv_frame* p_frame);
static HX_RESULT rv_decode_stream_decode(u8 *p_strm, On2DecoderInParams *input,
    On2DecoderOutParams *output);
static void rv_decode_frame_flush(rm2yuv_info *p_info, On2DecoderOutParams *output);

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

static codecSegmentInfo slice_info[128];

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

u32 add_extra_flag = 0;

u32 buffer_release_flag = 1;
u32 res_changed = 0;

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && num_buffers < MAX_BUFFERS) {
      struct DWLLinearMem mem;
      if(DWLMallocLinear(dwl_inst, buffer_size, &mem) == DWL_OK) {
        On2RvDecRet rv = On2RvDecAddBuffer(&mem, g_RVDec_inst);
        if(rv != ON2RVDEC_OK && rv != ON2RVDEC_WAITING_BUFFER) {
          DWLFreeLinear(dwl_inst, &mem);
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
    printf("Freeing buffer %p\n", ext_buffers[i].virtual_address);
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
u32 is_rm_file = 1;
rm2yuv_info info;

sem_t buf_release_sem;
On2DecoderOutParams buf_list[100] = {0};
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
      On2RvDecPictureConsumed(&buf_list[list_pop_index], g_RVDec_inst);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && num_buffers < MAX_BUFFERS) {
          struct DWLLinearMem mem;
          if(DWLMallocLinear(dwl_inst, buffer_size, &mem) == DWL_OK) {
            On2RvDecRet rv = On2RvDecAddBuffer(&mem, g_RVDec_inst);
            if(rv != ON2RVDEC_OK && rv != ON2RVDEC_WAITING_BUFFER) {
              DWLFreeLinear(dwl_inst, &mem);
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
}


/* Output thread entry point. */
static void* rv_output_thread(void* arg) {
  RvDecPicture output[1];
  On2DecoderOutParams dec_output;
  u32 pic_display_number = 1;
  u32 pic_size = 0;
  DWLFile *fp_out = HXNULL;
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
    RvDecRet ret;

    ret = On2RvDecNextPicture(&dec_output, g_RVDec_inst);

    if(ret == ON2RVDEC_OK) {
      rv_decode_frame_flush(&info, &dec_output);

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = dec_output;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);

      pic_display_number++;
    }

    else if(ret == ON2RVDEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
  }
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
  UINT32              ul_frames_per_sec = 0;
  rm_parser*          p_parser        = HXNULL;
  rm_stream_header*   p_hdr           = HXNULL;
  rm_packet*          p_packet        = HXNULL;
  rv_depack*          p_depack        = HXNULL;
  rv_decode*          p_decode        = HXNULL;
  rv_format_info*     p_info          = HXNULL;
  BYTE                uc_buf[RM2YUV_INITIAL_READ_SIZE];
  char                rv_outfilename[256];

  On2RvDecRet rv_ret;
  On2DecoderInit rv_init;
  On2RvMsgSetDecoderRprSizes msg;
  u32 tmp_buf[16];

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

  islog2std = islog2std;
  islog2file = islog2file;

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
    goto cleanup;
  }

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
  * Get the frames per second. This value is in 32-bit
  * fixed point, where the upper 16 bits is the integer
  * part of the fps, and the lower 16 bits is the fractional
  * part. We're going to truncate to integer, so all
  * we need is the upper 16 bits.
  */
  ul_frames_per_sec = p_info->ufFramesPerSecond >> 16;

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

  max_coded_width = p_decode->ulLargestPels;
  max_coded_height = p_decode->ulLargestLines;
  max_frame_width = MB_MULTIPLE( max_coded_width );
  max_frame_height = MB_MULTIPLE( max_coded_height );

  rv_init.outtype = 0;
  rv_init.pels = p_decode->ulLargestPels;
  rv_init.lines = p_decode->ulLargestLines;
  rv_init.n_pad_width = rv_init.n_pad_height = rv_init.pad_to_32 = 0;
  rv_init.ul_invariants = 0;
  rv_init.packetization = 0;
  rv_init.ul_stream_version = p_decode->bIsRV8 ? 0x30200000 : 0x40000000;

  /* initialize decoder */
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto cleanup;
  }
  rv_ret = On2RvDecInit(&rv_init,
                        dwl_inst,
                        &g_RVDec_inst);
  if (rv_ret != ON2RVDEC_OK) {
    ERROR_PRINT(("RVDecInit fail! \n"));
    goto cleanup;
  }

  /* override picture buffer amount */
  if(num_frame_buffers) {
    rv_ret = On2RvDecSetNbrOfBuffers( num_frame_buffers, g_RVDec_inst );
    if (rv_ret != ON2RVDEC_OK) {
      ERROR_PRINT(("Number of buffers failed!\n"));
      goto cleanup;
    }
  }


  /* set possible picture sizes for RV8 using
   * ON2RV_MSG_ID_Set_RVDecoder_RPR_Sizes custom message */
  if (p_decode->bIsRV8) {
    u32 i;
    msg.message_id = ON2RV_MSG_ID_Set_RVDecoder_RPR_Sizes;
    msg.num_sizes = p_decode->ulNumImageSizes;
    msg.sizes = tmp_buf;
    for (i = 0; i < 16; i++)
      msg.sizes[i] = p_decode->pDimensions[i];
    rv_ret = On2RvDecCustomMessage(&msg, g_RVDec_inst);
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

  /* Fill in the width and height */
  info.ul_width  = (p_decode->ulLargestPels+15)&0xFFFFFFF0;
  info.ul_height = (p_decode->ulLargestLines+15)&0xFFFFFFF0;

  /* Parse the output file path */
  if(g_iswritefile) {
    char flnm[0x100] = "";
    parse_path(filename, info.file_path, NULL);
  }

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

#ifdef ASIC_TRACE_SUPPORT
    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
#endif
  }

  /* last picture */
  {
    On2DecoderOutParams dec_output;
    On2DecoderInParams dec_input;
    dec_input.flags = ON2RV_DECODE_LAST_FRAME;
    dec_input.num_data_segments = 0;
    ret_val = rv_decode_stream_decode(tmp_buf, &dec_input, &dec_output);
  }

  /* Display results */
  DEBUG_PRINT(("Video stream in '%s' decoding complete: %lu input frames, %lu output frames\n",
               (const char*) filename, pics_decoded, number_of_written_frames));

  /* If the error was just a normal "out-of-packets" error,
  then clean it up */
  if (ret_val == HXR_NO_DATA) {
    ret_val = HXR_OK;
  }

cleanup:

  if(output_thread)
    pthread_join(output_thread, NULL);
  if(release_thread)
    pthread_join(release_thread, NULL);

  add_buffer_thread_run = 0;
  /* Destroy the codec init info */
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
    ReleaseExtBuffers();
    pthread_mutex_destroy(&ext_buffer_contro);
    On2RvDecFree(g_RVDec_inst);
    DWLRelease(dwl_inst);
  }

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
  On2DecoderOutParams dec_output;
  On2DecoderInParams dec_input;
  u32 i;

  DWLmemset(&dec_output, 0, sizeof(dec_output));
  DWLmemset(&dec_input, 0, sizeof(dec_input));

  if (p_avail && p_frame) {
    /* Get the info pointer */
    rm2yuv_info* p_info = (rm2yuv_info*) p_avail;
    if (p_info->p_depack && p_info->p_decode) {
      /* Increment the number of input frames */
      dec_input.data_length = p_frame->ulDataLen;
      dec_input.num_data_segments = p_frame->ulNumSegments;
      dec_input.p_data_segments = slice_info;
      DWLmemset(dec_input.p_data_segments, 0, sizeof(slice_info));
      for (i = 0; i < dec_input.num_data_segments; i++) {
        slice_info[i].b_is_valid = p_frame->pSegment[i].bIsValid;
        slice_info[i].ul_segment_offset = p_frame->pSegment[i].ulOffset;
      }
      dec_input.stream_bus_addr = p_frame->ulDataBusAddress;
      dec_input.timestamp = p_frame->ulTimestamp;
      dec_input.skip_non_reference = skip_non_reference;

      /* Decode frames until there aren't any more */
      do {
        dec_input.flags = (dec_output.notes & ON2RV_DECODE_MORE_FRAMES) ?
                          ON2RV_DECODE_MORE_FRAMES : 0;
        ret_val = rv_decode_stream_decode(p_frame->pData,
                                          &dec_input, &dec_output);

        if (HX_SUCCEEDED(ret_val)) {
        } else {
          p_info->b_dec_end = 1;
        }
      } while (HX_SUCCEEDED(ret_val) &&
               (dec_output.notes & ON2RV_DECODE_MORE_FRAMES));

      rv_depack_destroy_frame(p_info->p_depack, &p_frame);
      pics_decoded++;
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
HX_RESULT rv_decode_stream_decode(u8 *p_strm, On2DecoderInParams *input,
                                  On2DecoderOutParams *output) {
  HX_RESULT ret_val = HXR_FAIL;
  On2RvDecRet rv_ret;
  u32 more_data = 1;
  UINT32 i = 0;
  u32 packet_loss = 0;
  u32 ret = 0;
  struct RvDecConfig dec_cfg;
  u32 prev_width = 0, prev_height = 0;
  u32 min_buffer_num = 0;
  On2DecoderInfo dec_info;
  On2DecoderBufferInfo hbuf;
  On2RvDecRet rv;

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

  if (stream_bit_swap) {
    if((hdrs_rdy && !stream_header_corrupt) || stream_header_corrupt) {
      /* Picture needs to be ready before corrupting next picture */
      if(pic_rdy && corrupted_bytes <= 0) {
        ret = TBRandomizeBitSwapInStream(p_strm,
                                         input->data_length, tb_cfg.tb_params.stream_bit_swap);
        if(ret != 0) {
          printf("RANDOM STREAM ERROR FAILED\n");
          return 0;
        }

        corrupted_bytes = input->data_length;
        printf("corrupted_bytes %d\n", corrupted_bytes);
      }
    }
  }

  do {
    more_data = 0;

    rv_ret = On2RvDecDecode(p_strm, NULL, input, output, g_RVDec_inst);

    printDecodeReturn(rv_ret);

    switch(rv_ret) {
    case ON2RVDEC_HDRS_RDY:
      more_data = 1;
      rv = On2RvDecGetInfo(&dec_info, g_RVDec_inst);
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
      On2RvDecSetInfo(&dec_cfg, g_RVDec_inst);

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

      prev_width = dec_info.frame_width;
      prev_height = dec_info.frame_height;
      min_buffer_num = dec_info.pic_buff_size;
      break;

    case ON2RVDEC_WAITING_BUFFER:
      more_data = 1;
      rv = On2RvDecGetBufferInfo(&hbuf, g_RVDec_inst);

      printf("On2RvDecGetBufferInfo ret %d\n", rv);
      printf("buf_to_free %p, next_buf_size %d, buf_num %d\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);

      if (hbuf.buf_to_free.virtual_address != NULL && res_changed) {
        add_extra_flag = 0;
        ReleaseExtBuffers();
        buffer_release_flag = 1;
        num_buffers = 0;
        res_changed = 0;
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
          rv = On2RvDecAddBuffer(&mem, g_RVDec_inst);
          printf("On2RvDecAddBuffer ret %d\n", rv);
          if(rv != ON2RVDEC_OK && rv != ON2RVDEC_WAITING_BUFFER) {
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
    case ON2RVDEC_OK:

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
      if (output->num_frames) {
        frame_number++;
      }

      ret_val = HXR_OK;
      break;
    default:
      DEBUG_PRINT(("Fatal Error, Decode end!\n"));
      ret_val = HXR_FAIL;
      break;
    }
  } while(more_data);

  return ret_val;
}

/*------------------------------------------------------------------------------
function
  rv_decode_frame_flush()

purpose
  write the decoded picture to a local file or display on screen, if neccesary
------------------------------------------------------------------------------*/
void rv_decode_frame_flush(rm2yuv_info *p_info, On2DecoderOutParams *output) {
  UINT32 ul_width, ul_height, ul_out_frame_size;
  u8 *out_data = NULL;
  if (output == NULL)
    return;
   if(pp_enabled) {
     out_data =(u8*)malloc(output->width*output->height*3/2);
     max_coded_width = output->width;
     max_coded_height = output->height;
     max_frame_width = MB_MULTIPLE( max_coded_width );
     max_frame_height = MB_MULTIPLE( max_coded_height );
     ppFramePicture( output->p_out_frame,
                   output->width, output->height,
                   output->frame_width,
                   output->frame_height,
                   out_data, max_coded_width, max_coded_height );
   } else
     out_data = output->p_out_frame;

  /* Is there a valid output frame? */
  if (output->num_frames && !(output->notes & ON2RV_DECODE_DONT_DRAW)) {
    /* Get the dimensions and size of output frame */
    ul_width = output->width;
    ul_height = output->height;
    ul_out_frame_size = ul_width * ul_height * 3 / 2;

    /* Check to see if dimensions have changed */
    if (ul_width != p_info->ul_width || ul_height != p_info->ul_height ) {
      DEBUG_PRINT(("Warning: YUV output dimensions changed from "
                   "%lux%lu to %lux%lu at frame #: %lu (in decoding order)\n",
                   p_info->ul_width, p_info->ul_height, ul_width, ul_height, 0));
      /* Don't bother resizing to display dimensions */
      p_info->ul_width = ul_width;
      p_info->ul_height = ul_height;
      p_info->ul_out_frame_size = ul_out_frame_size;
      /* clear the flag of first display */
      g_bfirstdisplay = 0;
    }

    if(g_iswritefile) {
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

        if(f_tiled_output == NULL && tiled_output) {
          /* open output file for writing, can be disabled with define.
           * If file open fails -> exit */
          if(strcmp(out_file_name_tiled, "none") != 0) {
            f_tiled_output = fopen(out_file_name_tiled, "wb");
            if(f_tiled_output == NULL) {
              printf("UNABLE TO OPEN TILED OUTPUT FILE\n");
              if(out_data && pp_enabled)
                free(out_data);
              return;
            }
          }
        }
      }
      if (p_info->fp_out) {
#ifndef MD5SUM
        if( enable_frame_picture ) {
          u32 tmp = max_coded_width * max_coded_height, i;
          FramePicture( out_data,
                        p_info->ul_width, p_info->ul_height,
                        MB_MULTIPLE(p_info->ul_width),
                        MB_MULTIPLE(p_info->ul_height),
                        frame_buffer, max_coded_width, max_coded_height );
          DWLfwrite(frame_buffer, 1, 3*tmp/2, frame_out);
        }
#endif

#ifdef MD5SUM
        {
          u32 tmp = max_coded_width * max_coded_height, i;
          FramePicture( out_data,
                        p_info->ul_width, p_info->ul_height,
                        MB_MULTIPLE(p_info->ul_width),
                        MB_MULTIPLE(p_info->ul_height),
                        frame_buffer, max_coded_width, max_coded_height );

          /* TODO: frame counter */
          if(planar_output) {
            TBWriteFrameMD5Sum(p_info->fp_out, frame_buffer,
                               max_coded_width*
                               max_coded_height*3/2,
                               0);
          } else {
            TBWriteFrameMD5Sum(p_info->fp_out, out_data,
                               MB_MULTIPLE(p_info->ul_width)*
                               MB_MULTIPLE(p_info->ul_height)*3/2,
                               0);
          }
        }


        /* tiled */
        if (tiled_output) {
          /* round up to next multiple of 16 */
          u32 tmp_width = MB_MULTIPLE(p_info->ul_width);
          u32 tmp_height = MB_MULTIPLE(p_info->ul_height);

          TbWriteTiledOutput(f_tiled_output, out_data, tmp_width >> 4, tmp_height >> 4,
                             number_of_written_frames, 1 /* write md5sum */, 1 /* semi-planar data */ );
        }
#else
        if( !planar_output ) {
          DWLfwrite(out_data, 1,
                    MB_MULTIPLE(p_info->ul_width)*
                    MB_MULTIPLE(p_info->ul_height)*3/2,
                    p_info->fp_out);
        } else {
          u32 tmp = MB_MULTIPLE(p_info->ul_width) *
                    MB_MULTIPLE(p_info->ul_height), i;
          DWLfwrite(out_data, 1, tmp, p_info->fp_out);
          for(i = 0; i < tmp / 4; i++)
            DWLfwrite(out_data + tmp + i * 2, 1, 1, p_info->fp_out);
          for(i = 0; i < tmp / 4; i++)
            DWLfwrite(out_data + tmp + 1 + i * 2, 1, 1, p_info->fp_out);
        }

        /* tiled */
        if (tiled_output) {
          /* round up to next multiple of 16 */
          u32 tmp_width = MB_MULTIPLE(p_info->ul_width);
          u32 tmp_height = MB_MULTIPLE(p_info->ul_height);

          TbWriteTiledOutput(f_tiled_output, out_data, tmp_width >> 4, tmp_height >> 4,
                             number_of_written_frames, 0 /* not write md5sum */, 1 /* semi-planar data */ );
        }

#endif
        number_of_written_frames++;
      }
    }

  }
  if(out_data && pp_enabled)
    free(out_data);
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
  ERROR_PRINT(("rm2yuv_error p_error=0x%08x result=0x%08x msg=%s\n", p_error, result, psz_msg));
}

/*------------------------------------------------------------------------------
function
  printDecodeReturn

purpose
  Print out decoder return value
------------------------------------------------------------------------------*/
static void printDecodeReturn(i32 retval) {
  DEBUG_PRINT(("TB: RvDecDecode returned: "));
  switch(retval) {
  case ON2RVDEC_OK:
    DEBUG_PRINT(("ON2RVDEC_OK\n"));
    break;
  case ON2RVDEC_INVALID_PARAMETER:
    DEBUG_PRINT(("ON2RVDEC_INVALID_PARAMETER\n"));
    break;
  case ON2RVDEC_OUTOFMEMORY:
    DEBUG_PRINT(("ON2RVDEC_OUTOFMEMORY\n"));
    break;
  case ON2RVDEC_NOTIMPL:
    DEBUG_PRINT(("ON2RVDEC_NOTIMPL\n"));
    break;
  case ON2RVDEC_POINTER:
    DEBUG_PRINT(("ON2RVDEC_POINTER\n"));
    break;
  case ON2RVDEC_FAIL:
    DEBUG_PRINT(("ON2RVDEC_FAIL\n"));
    break;
  default:
    DEBUG_PRINT(("Other %d\n", retval));
    break;
  }
}

/*------------------------------------------------------------------------------
function
  parse_path

purpose
  parse filename and path

parameter
  path(I):  full path of input file
  dir(O):   directory path
  file(O):  file name
-----------------------------------------------------------------------------*/
void parse_path(const char *path, char *dir, char *file) {
  int len = 0;
  char *ptr = NULL;

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
  RvDecContainer *dec_cont = (RvDecContainer *)g_RVDec_inst;
  const void *dwl = NULL;

  p_user_mem = p_user_mem;

  ASSERT(dec_cont != NULL && dec_cont->dwl != NULL);

  p_buf = (struct DWLLinearMem *)DWLmalloc(sizeof(struct DWLLinearMem));
  if (p_buf == NULL)
    return NULL;

  dwl = dec_cont->dwl;
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
  RvDecContainer *dec_cont = (RvDecContainer *)g_RVDec_inst;
  const void *dwl = NULL;

  p_user_mem = p_user_mem;

  ASSERT(dec_cont != NULL && dec_cont->dwl != NULL);

  if (ptr == NULL)
    return;

  p_buf = *(struct DWLLinearMem **)ptr;

  if (p_buf == NULL)
    return;

  dwl = dec_cont->dwl;

  DWLFreeLinear(dwl, p_buf);
  DWLfree(p_buf);
  *(struct DWLLinearMem **)ptr = NULL;
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
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  int i;
  FILE *f_tbcfg;

  INIT_SW_PERFORMANCE;

  /* set test bench configuration */
#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 9170;
#endif

#ifdef RV_EVALUATION_9170
  g_hw_ver = 9170;
#elif RV_EVALUATION_9190
  g_hw_ver = 9190;
#endif

  if (argc < 2) {
    printf("\nReal Decoder Testbench\n");
    printf("Usage: %s [options] stream.rm\n", argv[0]);
    printf("\t-Nn forces decoding to stop after n pictures\n");
    printf("\t-Ooutfile write output to \"outfile\" (default out_wxxxhyyy.yuv)\n");
    printf("\t-X disable output file writing\n");
    printf("\t-P write planar output\n");
    printf("\t-T write tiled output by converting raster scan output\n");
    printf("\t-Bn to use n frame buffers in decoder\n");
    printf("\t-Q skip decoding non-reference pictures\n");
    printf("\t--md5 Output frame based md5 checksum\n");
    printf("\t-e add extra external buffer randomly\n");
    printf("\t-a add extra external buffer in ouput thread\n");
    printf("-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n");
    printf("-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g., ");
    printf("\t -d2 -- down scale to 1/2 in both directions\n");
    printf("\t -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
    printf("--cr-first PP outputs chroma in CrCb order.\n");
    printf("-C[xywh]NNN Cropping parameters. E.g.,\n");
    printf("\t  -Cx8 -Cy16        Crop from (8, 16)\n");
    printf("\t  -Cw720 -Ch480     Crop size  720x480\n");
    printf("-Dwxh  PP output size wxh. E.g.,\n");
    printf("\t  -D1280x720        PP output size 1280x720\n\n");
    printRvVersion();
    return -1;
  }

  /* read command line arguments */
  for(i = 1; i < (u32) (argc - 1); i++) {
    if(strncmp(argv[i], "-N", 2) == 0) {
      max_num_pics = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "-O", 2) == 0) {
      strcpy(out_file_name, argv[i] + 2);
    } else if(strcmp(argv[i], "-X") == 0) {
      disable_output_writing = 1;
    } else if(strcmp(argv[i], "-P") == 0) {
      planar_output = 1;
    } else if(strcmp(argv[i], "--md5") == 0) {
      md5sum = 1;
    } else if(strncmp(argv[i], "-B", 2) == 0) {
      num_frame_buffers = atoi(argv[i] + 2);
      if(num_frame_buffers > 16)
        num_frame_buffers = 16;
    } else if(strcmp(argv[i], "-Q") == 0) {
      skip_non_reference = 1;
    }
    else if(strcmp(argv[i], "-e") == 0) {
      use_extra_buffers = 1;
    }
    else if(strcmp(argv[i], "-a") == 0) {
      use_extra_buffers = 0;
      allocate_extra_buffers_in_output = 1;
    }
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
        fprintf(stdout, "Illegal scaled width/height: %s\n", argv[i]+2, px+1);
        return 1;
      }
      scale_mode = FLEXIBLE_SCALE;
    } else if (strncmp(argv[i], "-C", 2) == 0) {
      crop_enabled = 1;
      pp_enabled = 1;
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
      cr_first = 1;
    } else {
      DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
    }
  }

  printRvVersion();

  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if(f_tbcfg == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n"));
    DEBUG_PRINT(("USING DEFAULT CONFIGURATION\n"));
    tb_cfg.dec_params.hw_version = 9170;
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

  /*TBPrintCfg(&tb_cfg); */
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  output_format = TBGetDecOutputFormat(&tb_cfg);
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

void ppFramePicture( u8 *p_in, i32 in_width, i32 in_height,
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
  out_height /= 2;
  in_height /= 2;

  /* Chroma */
  /*p_out += 2 * out_width * ( out_height - in_height );*/
  p_out_cb = p_out;
  p_out_cr = p_out + out_width*out_height;
  for ( y = 0 ; y < in_height ; ++y ) {
    for( x = 0 ; x < in_width; ++x ) {
      *p_out++ = *p_in++;
    }
    p_in += ( in_frame_width - in_width );
    p_out += ( out_width - in_width );
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
