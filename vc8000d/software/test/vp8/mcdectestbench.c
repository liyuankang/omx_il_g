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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "basetype.h"
#include "dwl.h"
#include "dwlthread.h"
#include "fifo.h"
#include "regdrv.h"
#include "tb_cfg.h"
#include "testparams.h"
#include "vp8bufferalloc.h"
#include "vp8decapi.h"
#include "vp8filereader.h"
#include "vp8hwd_container.h"
#include "vp8writeoutput.h"

#define DEFAULT_FIFO_CAPACITY   (MAX_ASIC_CORES+1)
#define DEFAULT_OUTPUT_FILENAME "out.yuv"
#define VP8_MAX_STREAM_SIZE     DEC_X170_MAX_STREAM_VC8000D>>4
#define MAX_BUFFERS 16
/* Multicore decoding functionality */
/* States to control the test bench */
typedef enum test_state_ {
  STATE_DOWN = 0,      /* Nothing running, no resources allocated. */
  STATE_INITIALIZING,  /* Allocating resources in input thread. */
  STATE_STREAMING,     /* Input reading&decoding, output handling pictures. */
  STATE_END_OF_STREAM, /* Input waiting, outputs still pending. */
  STATE_TEARING_DOWN,  /* Input waiting, in middle of tear-down process. */
  STATE_OUTPUT_DOWN    /* Output thread has been torn down. */
} test_state;

/* Generic function declarations. */
typedef i32 read_stream_func(VP8DecInput*);   /* Input reading. */
typedef i32 write_pic_func(VP8DecPicture);    /* Output writing. */

typedef struct shared_data_ {
  FifoInst input_fifo_;    /* Handle to the fifo instance. */
  VP8DecInst dec_inst_;     /* Decoder instance. */
  void *dwl;                /* DWL instance. */
  pthread_t output_thread_; /* Handle to the output thread. */
  pthread_t release_thread_;
  test_state state_;        /* State of the test bench */
  u8 stream_buffer_refresh; /* Flag to tell whether to refresh strmbuffer. */
  VP8DecInput dec_input_;   /* Decoder input. */
  i32 output_status_;       /* Status value for output thread */
  write_pic_func* write_pic_; /* Function pointer to output writing. */
} shared_data;

typedef struct decoder_cb_data_ {
  FifoInst input_fifo_;      /* Handle to the fifo instance. */
  struct DWLLinearMem stream_mem_; /* Information about the linear mem. */
} decoder_cb_data;

typedef i32 buffer_alloc_func(shared_data*);          /* User buffer allocation */
typedef void buffer_free_func(shared_data*);          /* User buffer free */

typedef struct decoder_utils_ {
  read_stream_func* read_stream;
  write_pic_func* write_pic;
  buffer_alloc_func* alloc_buffers;
  buffer_free_func* free_buffers;
} decoder_utils;

/* Input thread functionality. */
static int vp8_multicore_decode(decoder_utils* decoder_utils, test_params *params); /* Input loop. */
static int input_on_initializing(shared_data* shared_data, test_params *params);
static int input_on_streaming(shared_data* shared_data,
                              decoder_utils* decoder_utils);
static int input_on_end_of_stream(shared_data* shared_data);
static int input_on_output_down(shared_data* shared_data,
                                decoder_utils* decoder_utils);
static void vp8_stream_consumed_cb(u8* consumed_stream_buffer,
                                   void* user_data); /* Callback from dec. */

/* Output thread functionality. */
static void* vp8_output_thread(void* arg); /* Output loop. */

/* I/O functionality required by tests. */
i32 alloc_user_buffers(shared_data* shared_data); /* Allocate user buffers */
void free_user_buffers(shared_data* shared_data);
i32 alloc_user_null(shared_data* shared_data);
void free_user_null(shared_data* shared_data);
i32 read_vp8_stream(VP8DecInput* input); /* Input reading. */
i32 write_pic_null(VP8DecPicture pic); /* Output writing. */
i32 write_pic(VP8DecPicture pic); /* Output writing. */

/* Parameter helpers. */
static void print_header(void);
static void print_usage(char* executable);
static void setup_default_params(test_params* params);
static void params_resolve_overlapping(struct TBCfg* tbcfg, test_params* params);
static int parse_params(int argc, char* argv[], test_params* params);
static int hwconfig_override(VP8DecInst dec_inst,  struct TBCfg* tbcfg);

/* Helpers to protect the state. */
static test_state get_state(shared_data* data);
static void set_state(shared_data* data, test_state state);

/* Input reader and output writer instance. */
reader_inst g_reader_inst;
output_inst g_output_inst;
useralloc_inst g_useralloc_inst;

/* Hack globals to carry around data for model. */
struct TBCfg tb_cfg;
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
u32 pp_tile_out = 0;    /* PP tiled output */
const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 buffer_size;
u32 num_buffers;  /* external buffers allocated yet. */
u32 add_buffer_thread_run = 0;
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS];

u32 add_extra_flag = 0;
/* Fixme: this value should be set based on option "-d" when invoking testbench. */
u32 res_changed = 0;

void ReleaseExtBuffers() {
  int i;
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    //DEBUG_PRINT(("Freeing buffer %p\n", ext_buffers[i].virtual_address));
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

FILE *fout;
u32 md5sum = 0; /* flag to enable md5sum output */
u32 slice_mode = 0;
int output_thread_run = 0;

sem_t buf_release_sem;
VP8DecPicture buf_list[100];
u32 buf_status[100] = {0};
u32 list_pop_index = 0;
u32 list_push_index = 0;
u32 last_pic_flag = 0;

static void* buf_release_thread(void* arg) {
  shared_data* shared_data = arg;
  while(1) {
    /* Pop output buffer from buf_list and consume it */
    if(buf_status[list_pop_index]) {
      sem_wait(&buf_release_sem);
      VP8DecMCPictureConsumed(shared_data->dec_inst_, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

    }
    if(last_pic_flag && buf_status[list_pop_index] == 0)
      break;
    usleep(10000);
  }
}

/* Output thread entry point. */
static void* vp8_output_thread(void* arg) {
  VP8DecPicture dec_picture;
  u32 pic_display_number = 1;
  u32 pic_size = 0;
  shared_data* shared_data = arg;
  while (1) {
    switch (get_state(shared_data)) {
    case STATE_STREAMING:
    case STATE_END_OF_STREAM:
      /* Function call blocks until picture is actually ready. */
      do {
        u8 *image_data;
        VP8DecRet ret;
        u32 tmp;
        ret = VP8DecMCNextPicture(shared_data->dec_inst_, &dec_picture);
        switch (ret) {
        case VP8DEC_OK:
        case VP8DEC_PIC_RDY:
          shared_data->output_status_ = shared_data->write_pic_(dec_picture);
          if (shared_data->output_status_) {
            fprintf(stderr, "ERROR: output writing failed.");
            set_state(shared_data, STATE_END_OF_STREAM);
          }
          /* Push output buffer into buf_list and wait to be consumed */
          buf_list[list_push_index] = dec_picture;
          buf_status[list_push_index] = 1;
          list_push_index++;
          if(list_push_index == 100)
            list_push_index = 0;

          sem_post(&buf_release_sem);

          pic_display_number++;
          usleep(10000);
          break;
        case VP8DEC_END_OF_STREAM:
          set_state(shared_data, STATE_TEARING_DOWN);
          last_pic_flag = 1;
          output_thread_run = 0;
          break;
        default:
          /* If this happens, we're already screwed. */
          fprintf(stderr,
                  "ERROR: unhandled condition for NextPicture.");
          set_state(shared_data, STATE_OUTPUT_DOWN);
          output_thread_run = 0;
          shared_data->output_status_ = -1;
          break;
        }
      } while (output_thread_run);
      break;

      case STATE_TEARING_DOWN:
        set_state(shared_data, STATE_OUTPUT_DOWN);
        pthread_exit(NULL);
        break;

      case STATE_INITIALIZING:
      case STATE_OUTPUT_DOWN:
      case STATE_DOWN:
      default:
        fprintf(stderr, "Output thread active in invalid state.");
        assert(0);
        pthread_exit(NULL);
        break;
    }
  }
}

/* buf release thread entry point. */
#define DEBUG_PRINT(str) printf str

int main(int argc, char* argv[]) {
  test_params params;
  u32 tmp;
  decoder_utils dec_utils;
  FILE * f_tbcfg;

  print_header();
  setup_default_params(&params);
  if (argc < 2) {
    print_usage(argv[0]);
    return 0;
  }
  if (parse_params(argc, argv, &params)) {
    return 1;
  }

  TBSetDefaultCfg(&tb_cfg); /* Set up the struct TBCfg hack. */
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

  params_resolve_overlapping(&tb_cfg, &params);

  /* Create the reader and output. */
  g_reader_inst = rdr_open(params.in_file_name_);
  if (g_reader_inst == NULL)
    return 1;
  g_output_inst = output_open(params.out_file_name_, &params);
  if (g_output_inst == NULL)
    return 1;

  /* Create user buffer allocator if necessary */
  if (params.user_allocated_buffers_) {
    g_useralloc_inst = useralloc_open(&params);
    if (g_useralloc_inst == NULL)
      return 1;
  }

#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if (!tmp) {
    DEBUG_PRINT(("UNABLE TO OPEN TRACE FILE(S)\n"));
  }
#endif

  /* Setup decoder utility functions */
  dec_utils.read_stream = read_vp8_stream;
  dec_utils.write_pic = write_pic;
  if (params.user_allocated_buffers_) {
    dec_utils.alloc_buffers = alloc_user_buffers;
    dec_utils.free_buffers = free_user_buffers;
  } else {
    dec_utils.alloc_buffers = alloc_user_null;
    dec_utils.free_buffers = free_user_null;
  }
  /* Run the input loop. */
  vp8_multicore_decode(&dec_utils, &params);

  /* Close the reader and output. */
  rdr_close(g_reader_inst);
  output_close(g_output_inst);

  if (params.user_allocated_buffers_) {
    useralloc_close(g_useralloc_inst);
  }
  return 0;
}

static int vp8_multicore_decode(decoder_utils* decoder_utils, test_params *params) {
  shared_data shared_data;
  set_state(&shared_data, STATE_INITIALIZING);
  shared_data.write_pic_ = decoder_utils->write_pic;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  /* Start the actual decoding loop. */
  while (1) {
    switch (get_state(&shared_data)) {
    case STATE_INITIALIZING:
      if(input_on_initializing(&shared_data, params)) {
        /* If init fails do not continue. */
        return 0;
      }
      break;
    case STATE_STREAMING:
      input_on_streaming(&shared_data, decoder_utils);
      break;
    case STATE_END_OF_STREAM:
      input_on_end_of_stream(&shared_data);
      break;
    case STATE_OUTPUT_DOWN:
      input_on_output_down(&shared_data, decoder_utils);
      return 0;
    case STATE_TEARING_DOWN:
    case STATE_DOWN:
    default:
      fprintf(stderr, "Input thread in invalid state.");
      assert(0);
      return -1;
    }
  }
  fprintf(stderr, "ERROR: Something went horribly wrong with input thread.");
  assert(0);
  return -1;
}

static void vp8_stream_consumed_cb(u8* consumed_stream_buffer,
                                   void* user_data) {
  /* This function could be called in signal handler context. */
  assert(user_data);
  decoder_cb_data* callback_data = (decoder_cb_data*)user_data;
  FifoObject stream_buffer = callback_data;
  /* Recycle the input buffer to the input buffer queue. */
  FifoPush(callback_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
}

i32 alloc_user_buffers(shared_data* shared_data) {
  return useralloc_alloc(g_useralloc_inst, shared_data->dec_inst_, shared_data->dwl);
}
void free_user_buffers(shared_data* shared_data) {
  useralloc_free(g_useralloc_inst, shared_data->dec_inst_, shared_data->dwl);

}

i32 alloc_user_null(shared_data* shared_data) {
  return 0;
}

void free_user_null(shared_data* shared_data) {
  return;
}

i32 read_vp8_stream(VP8DecInput* input) {
  return rdr_read_frame(g_reader_inst, input->stream, VP8_MAX_STREAM_SIZE,
                        &input->data_len, 1);
}

i32 write_pic(VP8DecPicture pic) {
  DEBUG_PRINT(("WRITING PICTURE %d\n", pic.pic_id));

  return output_write_pic(g_output_inst,
                          (unsigned char *) pic.pictures[0].p_output_frame,
                          (unsigned char *) pic.pictures[0].p_output_frame_c,
                          pic.pictures[0].frame_width, pic.pictures[0].frame_height,
                          pic.pictures[0].coded_width, pic.pictures[0].coded_height,
                          0,
                          pic.pictures[0].output_format,
                          pic.pictures[0].luma_stride,
                          pic.pictures[0].chroma_stride,
                          pic.pic_id, pic.pictures[0].pic_stride, pp_enabled);

}

i32 write_pic_null(VP8DecPicture pic) {
  return 0;
}

static int input_on_initializing(shared_data* shared_data, test_params *params) {
  i32 i;
  i32 num_of_stream_bufs = DEFAULT_FIFO_CAPACITY;
  VP8DecFormat dec_format;
  VP8DecMCConfig config;
  VP8DecRet rv;
  enum DecDpbFlags flags = DEC_REF_FRM_RASTER_SCAN;
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP8_DEC;

  shared_data->dwl = DWLInit(&dwl_init);
  if(shared_data->dwl == NULL) {
    return -1;
  }
  dwl_inst = shared_data->dwl;
  /* Allocate initial stream buffers and put them to fifo queue */
  if (FifoInit(DEFAULT_FIFO_CAPACITY, &shared_data->input_fifo_))
    return -1;

  if(rdr_identify_format(g_reader_inst) != G1_BITSTREAM_VP8) {
    return -1;
  }
  if (params->tiled_)
    flags |= DEC_REF_FRM_TILED_DEFAULT;
  config.dpb_flags = flags;
  config.stream_consumed_callback = vp8_stream_consumed_cb;
  /* Initialize the decoder in multicore mode. */
  if (VP8DecMCInit(&shared_data->dec_inst_, shared_data->dwl, &config) != VP8DEC_OK) {
    FifoRelease(shared_data->input_fifo_);
    set_state(shared_data, STATE_TEARING_DOWN);
    return -1;
  }

  /* Create the stream buffers and push them into the FIFO. */
  for (i = 0; i < num_of_stream_bufs; i++) {
    FifoObject stream_buffer;
    struct DWLLinearMem stream_mem;
    stream_buffer = malloc(sizeof(decoder_cb_data));
    stream_buffer->input_fifo_ = shared_data->input_fifo_;
    if (DWLMallocLinear(
          ((VP8DecContainer_t *)shared_data->dec_inst_)->dwl,
          VP8_MAX_STREAM_SIZE,
          &stream_buffer->stream_mem_) != DWL_OK) {
      fprintf(stderr,"UNABLE TO ALLOCATE STREAM BUFFER MEM\n");
      set_state(shared_data, STATE_TEARING_DOWN);
      return -1;
    }
    FifoPush(shared_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
  }

  /* Internal testing feature: Override HW configuration parameters */
  hwconfig_override(shared_data->dec_inst_, &tb_cfg);

  /* Start the output handling thread. */
  shared_data->stream_buffer_refresh = 1;
  set_state(shared_data, STATE_STREAMING);
  pthread_create(&shared_data->output_thread_, NULL, vp8_output_thread, shared_data);
  pthread_create(&shared_data->release_thread_, NULL, buf_release_thread, shared_data);
  return 0;
}

static int input_on_streaming(shared_data* shared_data,
                              decoder_utils* decoder_utils) {
  FifoObject stream_buffer = NULL;
  VP8DecRet rv;
  VP8DecRet ret;
  VP8DecOutput output;
  VP8DecInfo info;
  u32 i;
  u32 prev_width = 0, prev_height = 0;
  u32 min_buffer_num = 0;
  u32 buffer_release_flag = 1;
  VP8DecBufferInfo hbuf;
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct VP8DecConfig dec_cfg;
  struct DWLLinearMem mem;
  /* If needed, refresh the input stream buffer. */
  if (shared_data->stream_buffer_refresh) {
    /* Fifo pop blocks until available or queue released. */
    FifoPop(shared_data->input_fifo_, &stream_buffer, FIFO_EXCEPTION_DISABLE);
    /* Read data to stream buffer. */
    memset(&shared_data->dec_input_, 0, sizeof(shared_data->dec_input_));
    shared_data->dec_input_.stream =
      (u8*)stream_buffer->stream_mem_.virtual_address;
    shared_data->dec_input_.stream_bus_address =
      stream_buffer->stream_mem_.bus_address;
    /* Trick to recycle buffer and fifo to the callback. */
    shared_data->dec_input_.p_user_data = stream_buffer;
    switch (decoder_utils->read_stream(&shared_data->dec_input_)) {
    case 0:
      shared_data->stream_buffer_refresh = 0;
      break;
    /* TODO(vmr): Remember to check eos and set it. */
    default:
      /* Return the unused buffer. */
      FifoPush(shared_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
      set_state(shared_data, STATE_END_OF_STREAM);
      return 0;
    }
  }
  /* Decode the contents of the input stream buffer. */
#if 0
  switch (rv = VP8DecMCDecode(shared_data->dec_inst_, &shared_data->dec_input_,
                              &output)) {
#else
  do {
    rv = VP8DecMCDecode(shared_data->dec_inst_, &shared_data->dec_input_,&output);
  } while(shared_data->dec_input_.data_len == 0 && rv == VP8DEC_NO_DECODING_BUFFER);
  switch(rv) {
#endif
  case VP8DEC_HDRS_RDY:
    if(decoder_utils->alloc_buffers(shared_data)) {
      fprintf(stderr, "Error in custom buffer allocation\n");
      /* Return the stream buffer if allocation fails. */
      if(stream_buffer != NULL) {
        FifoPush(shared_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
      }
      set_state(shared_data, STATE_END_OF_STREAM);
    }
    ret = VP8DecGetInfo(shared_data->dec_inst_, &info);
    if (tb_cfg.ppu_index == -1) {
      if (pp_enabled) {
        /* No PpUnitParams in tb.cfg, use command line options instead */
        if (crop_enabled) {
          tb_cfg.pp_units_params[0].crop_x = crop_x;
          tb_cfg.pp_units_params[0].crop_y = crop_y;
          tb_cfg.pp_units_params[0].crop_width = crop_w;
          tb_cfg.pp_units_params[0].crop_height = crop_h;
        } else {
          tb_cfg.pp_units_params[0].crop_x = 0;
          tb_cfg.pp_units_params[0].crop_y = 0;
          tb_cfg.pp_units_params[0].crop_width = info.frame_width;
          tb_cfg.pp_units_params[0].crop_height = info.frame_height;
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
      }
      tb_cfg.pp_units_params[0].unit_enabled = pp_enabled;
      tb_cfg.pp_units_params[1].unit_enabled = 0;
      tb_cfg.pp_units_params[2].unit_enabled = 0;
      tb_cfg.pp_units_params[3].unit_enabled = 0;
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
      dec_cfg.ppu_config[i].out_p010 = tb_cfg.pp_units_params[i].out_p010;
      dec_cfg.ppu_config[i].out_cut_8bits = tb_cfg.pp_units_params[i].out_cut_8bits;
      dec_cfg.ppu_config[i].align = align;
      if (dec_cfg.ppu_config[i].enabled)
        pp_enabled = 1;
    }
    dec_cfg.align = align;
    dec_cfg.cr_first = cr_first;
    VP8DecSetInfo(shared_data->dec_inst_, &dec_cfg);
    if((info.pic_buff_size != min_buffer_num) ||
       (info.frame_width * info.frame_height != prev_width * prev_height)) {
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
    prev_width = info.frame_width;
    prev_height = info.frame_height;
    min_buffer_num = info.pic_buff_size;
    break;
  case VP8DEC_WAITING_FOR_BUFFER:
    ret = VP8DecGetBufferInfo(shared_data->dec_inst_, &hbuf);
    //DEBUG_PRINT(("VP8DecGetBufferInfo ret %d\n", ret));
    //DEBUG_PRINT(("buf_to_free %p, next_buf_size %d, buf_num %d\n",
      //         (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
    while (ret == VP8DEC_WAITING_FOR_BUFFER) {
      if (hbuf.buf_to_free.virtual_address != NULL && res_changed) {
        add_extra_flag = 0;
        ReleaseExtBuffers();
        buffer_release_flag = 1;
        num_buffers = 0;
        res_changed = 0;
      }
      ret = VP8DecGetBufferInfo(shared_data->dec_inst_, &hbuf);
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
        ret = VP8DecAddBuffer(shared_data->dec_inst_, &mem);
        //DEBUG_PRINT(("VP8DecAddBuffer ret %d\n", ret));
        if(ret != VP8DEC_OK && ret != VP8DEC_WAITING_FOR_BUFFER) {
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
    if (!output_thread_run) {
      output_thread_run = 1;
      sem_init(&buf_release_sem, 0, 0);
    }
    break;
  case VP8DEC_STRM_PROCESSED:
    break;
  case VP8DEC_PIC_DECODED:
  case VP8DEC_SLICE_RDY:
  case VP8DEC_OK:
    /* Everything is good, keep on going. */
    shared_data->stream_buffer_refresh = 1;
    break;
  case VP8DEC_STREAM_NOT_SUPPORTED:
    /* Probably encountered faulty bitstream. */
    fprintf(stderr, "Missing headers or unsupported stream(?)\n");
    set_state(shared_data, STATE_END_OF_STREAM);
    break;
  case VP8DEC_MEMFAIL:
    /* Might be low on memory or something worse, anyway we
     * need to stop */
    fprintf(stderr,
            "VP8DecDecode VP8DEC_MEMFAIL. Low on memory?\n");
    set_state(shared_data, STATE_END_OF_STREAM);
    break;
  case VP8DEC_PARAM_ERROR:
  case VP8DEC_NOT_INITIALIZED:
    /* these are errors when the decoder cannot possibly call
     * stream consumed callback and therefore we push the buffer
     * into the empty queue here. */
    FifoPush(shared_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
  case VP8DEC_INITFAIL:
  case VP8DEC_HW_RESERVED:
  case VP8DEC_HW_TIMEOUT:
  case VP8DEC_HW_BUS_ERROR:
  case VP8DEC_SYSTEM_ERROR:
  case VP8DEC_DWL_ERROR:
  case VP8DEC_STRM_ERROR:
    /* This is bad stuff and something in the system is
     * fundamentally flawed. Can't do much so just stop. */
    fprintf(stderr, "VP8DecDecode fatal failure %i\n", rv);
    set_state(shared_data, STATE_END_OF_STREAM);
    break;
  case VP8DEC_EVALUATION_LIMIT_EXCEEDED:
    fprintf(stderr, "Decoder evaluation limit reached\n");
    /* Return the stream buffer. */
    set_state(shared_data, STATE_END_OF_STREAM);
    break;
  case VP8DEC_PIC_RDY:
  default:
    fprintf(stderr, "UNKNOWN ERROR!\n");
    /* Return the stream buffer. */
    FifoPush(shared_data->input_fifo_, stream_buffer, FIFO_EXCEPTION_DISABLE);
    set_state(shared_data, STATE_END_OF_STREAM);
    break;
  }
  return 0;
}

static int input_on_end_of_stream(shared_data* shared_data) {
  if(VP8DecMCEndOfStream(shared_data->dec_inst_) != VP8DEC_OK) {
    fprintf(stderr, "VP8DecMCEndOfStream returned unexpected failure\n");
  }

  /* We're done, wait for the output to finish it's job. */
  pthread_join(shared_data->output_thread_, NULL);
  pthread_join(shared_data->release_thread_, NULL);
  add_buffer_thread_run = 0;
  if (shared_data->output_status_) {
    fprintf(stderr, "Output thread returned fail status (%u)\n",
            shared_data->output_status_);
  }
  return 0;
}

static int input_on_output_down(shared_data* shared_data,
                                decoder_utils* decoder_utils) {
  i32 i;
  for (i = 0; i < DEFAULT_FIFO_CAPACITY; i++) {
    FifoObject stream_buffer;
    FifoPop(shared_data->input_fifo_, &stream_buffer, FIFO_EXCEPTION_DISABLE);
    DWLFreeLinear(((VP8DecContainer_t *)shared_data->dec_inst_)->dwl,
                  &stream_buffer->stream_mem_);
    free(stream_buffer);
  }

  decoder_utils->free_buffers(shared_data);

  /* Release the allocated resources. */
  FifoRelease(shared_data->input_fifo_);
  set_state(shared_data, STATE_DOWN);
  VP8DecRelease(shared_data->dec_inst_);
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  DWLRelease(dwl_inst);
  return 0;
}

static void print_header(void) {
  VP8DecApiVersion version = VP8DecGetAPIVersion();
  VP8DecBuild build = VP8DecGetBuild();
  printf("Multicore VP8 decoder\n");
  printf("API version %i.%i, ", version.major, version.minor);
  printf("SW build %u, ", build.sw_build);
  printf("HW build %u\n", build.hw_build);
}

static void print_usage(char* executable) {
  printf("Usage: %s [options] <file>\n", executable);
  printf("\t-a (or -Xa) user allocates picture buffers, alternate allocation order. (--user-allocated-buffers-alt)\n");
  printf("\t-cn Chrominance stride. (--chroma-stride)\n");
  printf("\t-C display cropped image. (--display-cropped)\n");
  printf("\t-W Disable output writing (--disable-write)\n");
  printf("\t-E use tiled reference frame format. (--tiled-output)\n");
  printf("\t-F Enable frame picture writing, filled black. (--frame-write)\n");
  printf("\t-I use interleaved frame buffers (requires stride mode and "\
         "user allocated buffers(--interleaved-buffers\n");
  printf("\t-ln Luminance stride. (--luma-stride)\n");
  printf("\t-Nn forces decoding to stop after n pictures. (--decode-n-pictures)\n");
  printf("\t-Ooutfile write output to \"outfile\" (default out.yuv). (--output-file <outfile>)\n");
  printf("\t-M write output as MD5 sum. (--md5)\n");
  printf("\t-X user allocates picture buffers. (--user-allocated-buffers)\n");
  printf("\t-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n");
  printf("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g., ");
  printf("\t -d2 -- down scale to 1/2 in both directions\n");
  printf("\t -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n");
  printf("\t--cr-first PP outputs chroma in CrCb order.\n");
  printf("\t-C[xywh]NNN Cropping parameters. E.g.,\n");
  printf("\t  -Cx8 -Cy16        Crop from (8, 16)\n");
  printf("\t  -Cw720 -Ch480     Crop size  720x480\n");
  printf("\t-Dwxh  PP output size wxh. E.g.,\n");
  printf("\t  -D1280x720        PP output size 1280x720\n");
}

static void setup_default_params(test_params* params) {
  memset(params, 0, sizeof(test_params));
  params->out_file_name_ = DEFAULT_OUTPUT_FILENAME;
}

static void params_resolve_overlapping(struct TBCfg* tbcfg, test_params* params) {
  /* Override decoder allocation with tbcfg value */
  if (TBGetDecMemoryAllocation(&tb_cfg) &&
      params->user_allocated_buffers_ == VP8DEC_DECODER_ALLOC) {
    params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC;
  }

  if (params->luma_stride_ || params->chroma_stride_) {
    if (params->user_allocated_buffers_ == VP8DEC_DECODER_ALLOC) {
      params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC;
    }

    if (params->md5_) {
      params->frame_picture_ = 1;
    }
  }
  /* MD5 sum unsupported for external mem allocation */
  if (params->interleaved_buffers_ ||
      params->user_allocated_buffers_ != VP8DEC_DECODER_ALLOC) {
    params->md5_ = 0;
  }
}

static int parse_params(int argc, char* argv[], test_params* params) {
  i32 c;
  u32 i;
  i32 option_index = 0;
  static struct option long_options[] = {
    {"user-allocated-buffers-alt", no_argument, 0, 'a'},
    {"chroma-stride", required_argument, 0, 'c'},
    {"display-cropped", no_argument, 0, 'C'},
    {"disable-write", no_argument, 0, 'D'},
    {"tiled-ouput", no_argument, 0, 'E'},
    {"frame-write", no_argument, 0, 'F'},
    {"interleaved-buffers", no_argument, 0, 'I'},
    {"luma-stride", required_argument, 0, 'l'},
    {"md5",  no_argument, 0, 'M'},
    {"decode-n-pictures",  required_argument, 0, 'N'},
    {"output-file",  required_argument, 0, 'O'},
    {"user-allocated-buffers", no_argument, 0, 'X'},
    {0, 0, 0, 0}
  };

  /* read command line arguments */
#if 0
  while ((c = getopt_long(argc, argv, "ac:CDEFIl:MN:O:X", long_options, &option_index)) != -1) {
    switch (c) {

    case 'a':
      params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC_ALT;
      break;
    case 'c':
      params->chroma_stride_ = atoi(optarg);
      break;
    case 'C':
      params->display_cropped_ = 1;
      break;
    case 'W':
      params->disable_write_ = 1;
      break;
    case 'E':
      params->tiled_ = 1;
      break;
    case 'F':
      params->frame_picture_ = 1;
      break;
    case 'I':
      params->interleaved_buffers_ = 1;
      break;
    case 'l':
      params->luma_stride_ = atoi(optarg);
      break;
    case 'M':
      params->md5_ = 1;
      break;
    case 'N':
      params->num_of_decoded_pics_ = atoi(optarg);
      break;
    case 'O':
      params->out_file_name_ = optarg;
      break;
    case 'X':
      params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC;
      break;
    case ':':
      if (optopt == 'O' || optopt == 'N' || optopt == 'l' || optopt == 'c')
        fprintf(stderr, "Option -%c requires an argument.\n",
                optopt);
      return 1;
    case '?':
      if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr,
                "Unknown option character `\\x%x'.\n",
                optopt);
      return 1;
    default:
      break;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "Invalid or no input file specified\n");
    return 1;
  }
  params->in_file_name_ = argv[optind];
  return 0;
#else
  for (i = 1; i < (u32)(argc-1); i++) {
    if (strncmp(argv[i], "-a", 2) == 0)
      params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC_ALT;
    else if (strncmp(argv[i], "-c", 2) == 0)
      params->chroma_stride_ = (u32)atoi(argv[i] + 2);
#if 0
    else if (strncmp(argv[i], "-C", 2) == 0)
      params->display_cropped_ = 1;
#endif
    else if (strncmp(argv[i], "-W", 2) == 0)
      params->disable_write_ = 1;
    else if (strncmp(argv[i], "-E", 2) == 0)
      params->tiled_ = 1;
    else if (strncmp(argv[i], "-F", 2) == 0)
      params->frame_picture_ = 1;
    else if (strncmp(argv[i], "-I", 2) == 0)
      params->interleaved_buffers_ = 1;
    else if (strncmp(argv[i], "-l", 2) == 0)
      params->luma_stride_ = (u32)atoi(argv[i] + 2);
    else if (strncmp(argv[i], "-M", 2) == 0)
      params->md5_ = 1;
    else if (strncmp(argv[i], "-N", 2) == 0)
      params->num_of_decoded_pics_ = (u32)atoi(argv[i] + 2);
    else if (strncmp(argv[i], "-O", 2) == 0)
      strcpy(params->out_file_name_, argv[i] + 2);
    else if (strncmp(argv[i], "-X", 2) == 0)
      params->user_allocated_buffers_ = VP8DEC_EXTERNAL_ALLOC;
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
    } else if (strcmp(argv[i], "-U") == 0) {
      pp_tile_out = 1;
      tb_cfg.pp_units_params[0].tiled_e = pp_tile_out;
      pp_enabled = 1;
    } else {
      //DEBUG_PRINT(("UNKNOWN PARAMETER: %s\n", argv[i]));
      return 1;
    }
  }
  while ((c = getopt_long(argc, argv, "ac:CDEFIl:MN:O:X", long_options, &option_index)) != -1);
  if (optind >= argc) {
    fprintf(stderr, "Invalid or no input file specified\n");
    return 1;
  }
  params->in_file_name_ = argv[optind];
  return 0;
#endif
}

static pthread_mutex_t cs_mutex = PTHREAD_MUTEX_INITIALIZER;

static test_state get_state(shared_data* data) {
  pthread_mutex_lock(&cs_mutex);
  test_state state = data->state_;
  pthread_mutex_unlock(&cs_mutex);
  return state;
}

static void set_state(shared_data* data, test_state state) {
  pthread_mutex_lock(&cs_mutex);
  data->state_ = state;
  pthread_mutex_unlock(&cs_mutex);
}

static int hwconfig_override(VP8DecInst dec_inst,  struct TBCfg* tbcfg) {
  u32 clock_gating = TBGetDecClockGating(&tb_cfg);
  u32 data_discard = TBGetDecDataDiscard(&tb_cfg);
  u32 latency_comp = tb_cfg.dec_params.latency_compensation;
  u32 output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  u32 bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  u32 asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  u32 output_format = TBGetDecOutputFormat(&tb_cfg);
  u32 service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  DEBUG_PRINT(("Decoder Clock Gating %d\n", clock_gating));
  DEBUG_PRINT(("Decoder Data Discard %d\n", data_discard));
  DEBUG_PRINT(("Decoder Latency Compensation %d\n", latency_comp));
  DEBUG_PRINT(("Decoder Output Picture Endian %d\n", output_picture_endian));
  DEBUG_PRINT(("Decoder Bus Burst Length %d\n", bus_burst_length));
  DEBUG_PRINT(("Decoder Asic Service Priority %d\n", asic_service_priority));
  DEBUG_PRINT(("Decoder Output Format %d\n", output_format));

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
}
