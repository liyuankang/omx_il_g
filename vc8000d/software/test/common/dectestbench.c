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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwlthread.h"
#include "fifo.h"
#include "regdrv.h"
#include "basetype.h"
#include "decapi.h"
#include "dwl.h"
#include "bytestream_parser.h"
#include "command_line_parser.h"
#include "error_simulator.h"
#include "file_sink.h"
#include "md5_sink.h"
#include "null_sink.h"
#ifdef SDL_ENABLED
#include "sdl_sink.h"
#endif
#include "tb_cfg.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#endif
#include "trace_hooks.h"
#include "vpxfilereader.h"
#include "yuvfilters.h"
#include "libav-wrapper.h"
#include "error.h"

#define NUM_OF_STREAM_BUFFERS (MAX_ASIC_CORES + 1)
#define DEFAULT_STREAM_BUFFER_SIZE (1024 * 1024)

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define DEBUG_PRINT(str) printf str

#define MAX_BUFFERS 40

#define NUM_REF_BUFFERS 10

#define MAXARGS 128

/* Generic yuv writing interface. */
typedef const void* YuvsinkOpenFunc(const char** fname);
typedef void YuvsinkWritePictureFunc(const void* inst, struct DecPicture pic, int index);
typedef void YuvsinkCloseFunc(const void* inst);
typedef struct YuvSink_ {
  const void* inst;
  YuvsinkOpenFunc* open;
  YuvsinkWritePictureFunc* WritePicture;
  YuvsinkCloseFunc* close;
} YuvSink;

struct Client {
  Demuxer demuxer; /* Demuxer instance. */
  DecInst decoder; /* Decoder instance. */
  YuvSink yuvsink; /* Yuvsink instance. */
  const void* dwl; /* OS wrapper layer instance and function pointers. */
  #ifdef ASIC_ONL_SIM
  u32 dec_done;
  #else
  sem_t dec_done;  /* Semaphore which is signalled when decoding is done. */
  #endif
  struct DecInput buffers[NUM_OF_STREAM_BUFFERS]; /* Stream buffers. */
  u32 num_of_output_pics; /* Counter for number of pictures decoded. */
  FifoInst pic_fifo;      /* Picture FIFO to parallelize output. */
  pthread_t parallel_output_thread; /* Parallel output thread handle. */
  struct TestParams test_params;    /* Parameters for the testing. */
  u8 eos; /* Flag telling whether client has encountered end of stream. */
  u32 cycle_count; /* Sum of average cycles/mb counts */
  u8 dec_initialized; /* Flag indicating whether decoder has been initialized successfully. */
  u8 new_hdr;         /* Flag indicating whether a new header is coming. */
  struct DWLLinearMem ext_buffers[MAX_BUFFERS];
  u32 max_buffers;
  u32 num_pics_to_display;  /* Picture numbers to be displayed. */
#ifdef ASIC_TRACE_SUPPORT
  struct DWLLinearMem ext_frm_buffers[MAX_BUFFERS];
  u32 max_frm_buffers;
#endif
  struct Client * multi_client[MAX_STREAMS];
  i32 argc;
  char **argv;      /* Command line parameter... */
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef __FREERTOS__
static pthread_t tid_task;
typedef struct Main_args_
{
  int argc;
  char **argv;
}Main_args;
static void* main_task(void *args);
#endif

/* Client actions */
static void DispatchBufferForDecoding(struct Client* client,
                                      struct DecInput* buffer);
static void DispatchEndOfStream(struct Client* client);
static void PostProcessPicture(struct Client* client,
                               struct DecPicturePpu* picture);

/* Callbacks from decoder. */
static void InitializedCb(ClientInst inst);
static void HeadersDecodedCb(ClientInst inst,
                             struct DecSequenceInfo sequence_info);
static u32 BufferRequestCb(ClientInst inst);
static void BufferDecodedCb(ClientInst inst, struct DecInput* buffer);
static void PictureReadyCb(ClientInst inst, struct DecPicturePpu picture);
static void EndOfStreamCb(ClientInst inst);
static void ReleasedCb(ClientInst inst);
static void NotifyErrorCb(ClientInst inst, u32 pic_id, enum DecRet rv);

/* Output thread functionality. */
static void* ParallelOutput(void* arg); /* Output loop. */

/* Helper functionality. */
static const void* CreateDemuxer(struct Client* client);
static void ReleaseDemuxer(struct Client* client);
static const void* CreateSink(struct Client* client);
static void ReleaseSink(struct Client* client);

static u8 ErrorSimulationNeeded(struct Client* client);
static i32 GetStreamBufferCount(struct Client* client);

static void StreamBufferConsumedMC(void *stream, void *p_user_data);

/* Pointers to the DWL functionality. */
struct DWL dwl = {DWLReserveHw,          /* ReserveHw */
         DWLReserveHwPipe,      /* ReserveHwPipe */
         DWLReleaseHw,          /* ReleaseHw */
         DWLMallocLinear,       /* MallocLinear */
         DWLFreeLinear,         /* FreeLinear */
         DWLWriteReg,           /* WriteReg */
         DWLReadReg,            /* ReadReg */
         DWLEnableHw,           /* EnableHw */
         DWLDisableHw,          /* DisableHw */
         DWLWaitHwReady,        /* WaitHwReady */
         DWLSetIRQCallback,     /* SetIRQCallback */
         malloc,                /* malloc */
         free,                  /* free */
         calloc,                /* calloc */
         memcpy,                /* memcpy */
         memset,                /* memset */
         pthread_create,        /* pthread_create */
         pthread_exit,          /* pthread_exit */
         pthread_join,          /* pthread_join */
         pthread_mutex_init,    /* pthread_mutex_init */
         pthread_mutex_destroy, /* pthread_mutex_destroy */
         pthread_mutex_lock,    /* pthread_mutex_lock */
         pthread_mutex_unlock,  /* pthread_mutex_unlock */
         pthread_cond_init,     /* pthread_cond_init */
         pthread_cond_destroy,  /* pthread_cond_destroy */
         pthread_cond_wait,     /* pthread_co/ux/VideoIP-IPD/cn8067/LOGS/VC8000D_LOGS/g2_mcpp_407/sum.lognd_wait */
         pthread_cond_signal,   /* pthread_cond_signal */
         printf,                /* printf */
};

static char out_file_name_buffer[2*DEC_MAX_OUT_COUNT][NAME_MAX];

/* (Unfortunate) Hack globals to carry around data for model. */
static i32 exist_f_tbcfg = 0;
struct TBCfg tb_cfg;
static void *thread_main(void *args);
static int run_instance(struct Client  *client);
static void setClientByTBCfg(struct Client* client);
static void OpenTestHooks(const struct Client* client);
static void CloseTestHooks(struct Client* client);
static int HwconfigOverride(DecInst dec_inst, struct TBCfg* tbcfg);
static int parse_stream_cfg(const char *streamcfg, struct Client *client);
#ifdef ASIC_ONL_SIM
u32 max_pics_decode;

int main_onl(int argc, char* sv_str) {
  char* argv[argc];
  char* delim=" " ;
  int   j=0       ;

  char* p=strtok(sv_str,delim);
  argv[j]=p;
  while(p!=NULL){
    p=strtok(NULL,delim);
    j++;
    argv[j]=p;
  }

  //for(int i=0;i<argc;i++){
  //  printf("\n converted argv[%d] is: %s",i,argv[i]);
  //}

#else
int main(int argc, char* argv[]) {
#endif
  osal_thread_init();
#ifdef __FREERTOS__
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  Main_args args = { argc, argv };
  pthread_create(&tid_task, &attr, &main_task, &args);
  vTaskStartScheduler();
  while(1); //should not arrive here if start scheduler successfully  
  return 0;
}
#endif //__FREERTOS__
#ifdef __FREERTOS__
void* main_task(void * args)
{
  Main_args *args_ = (Main_args *)args;
  int argc = args_->argc;
  char **argv = args_->argv;
#endif //__FREERTOS__

  struct Client client, *multi_client = NULL;
  int ret = 0, i = 0;
  
  memset(&client, 0, sizeof(struct Client));

  printf("\nHantro G2 decoder command line interface\n\n");
  SetupDefaultParams(&client.test_params);

  if (argc < 2) {
    PrintUsage(argv[0], G2DEC);
    ret = 0;
    goto error;
  }
  struct DecSwHwBuild build = DecGetBuild();
  printf("Hardware build: 0x%x, Software build: 0x%x\n", build.hw_build,
         build.sw_build);
  printf("VP9 %s\n", build.hw_config[0].vp9_support ? "enabled" : "disabled");
  printf("HEVC %s\n", build.hw_config[0].hevc_support ? "enabled" : "disabled");
  printf("PP %s\n", build.hw_config[0].pp_support ? "enabled" : "disabled");
  if (!build.hw_config[0].max_dec_pic_width ||
      !build.hw_config[0].max_dec_pic_height) {
    printf("Maximum supported picture width, height unspecified");
    printf(" (hw config reports %d, %d)\n\n", build.hw_config[0].max_dec_pic_width,
           build.hw_config[0].max_dec_pic_height);
  } else
    printf("Maximum supported picture width %d, height %d\n\n",
           build.hw_config[0].max_dec_pic_width,
           build.hw_config[0].max_dec_pic_height);
  
  if ((ret = ParseParams(argc, argv, &client.test_params))) {
    if (ret == 1) {
      printf("Failed to parse params.\n\n");
      PrintUsage(argv[0], G2DEC);
      ret = 1;
      goto error;
    }
    else {
      ret = 0;
      goto error;
    }
  }
  
#ifdef ASIC_ONL_SIM
  max_pics_decode = client->test_params.num_of_decoded_pics;
#endif

  OpenTestHooks(&client);

  if(client.test_params.nstream > 0) {
    if(client.test_params.multimode == 1) {
      /* multi thread */
      for(i = 0; i < client.test_params.nstream; i++) {
        multi_client = client.multi_client[i] = (struct Client *)calloc(sizeof(struct Client), 0);
        parse_stream_cfg(client.test_params.streamcfg[i], multi_client);
        
        pthread_attr_t attr;
        pthread_t *tid= (pthread_t*)calloc(sizeof(pthread_t), 0);
        pthread_attr_init(&attr);
        pthread_create(tid, &attr, &thread_main, multi_client);
        client.test_params.multi_stream_id.tid[i] = tid;
      }
    } else if(client.test_params.multimode == 2) {
      /* multi process */
      //Next to do
    } else if(client.test_params.multimode == 0) {
        printf("multi-stream disabled, ignore extra stream configs\n");
    } else {
        printf("Invalid multi stream mode\n");
        exit(-1);
    }
  }
  client.argc = argc;
  client.argv = argv;
  ret = run_instance(&client);
  if(client.test_params.multimode == 1)
  {
    for(i = 0; i < client.test_params.nstream; i++)
    {
      if(client.test_params.multi_stream_id.tid[i] != NULL)
        pthread_join(*client.test_params.multi_stream_id.tid[i], NULL);
    }
  }

  else if (client.test_params.multimode == 2)
  {
    //Next to do
  }
  if(client.test_params.multimode != 0)
  {
    for(i = 0; i < client.test_params.nstream; i++)
    {
      free(client.multi_client[i]);
      client.multi_client[i] = NULL;
      //multi_client = NULL;
      free(client.test_params.multi_stream_id.tid[i]);
      client.test_params.multi_stream_id.tid[i] = NULL;
    }
  }
error:
#ifdef __FREERTOS__
  vTaskEndScheduler(); // need to open in simulator, and need to close on real env
  return (void *)NULL;
#else
  return ret;
#endif
}
void *thread_main(void *args)
{
#if 0 //need to open when dwl is OK for multi stream
  run_instance((struct Client *)args);
  pthread_exit(NULL);
#endif
  return NULL;
}
int run_instance(struct Client * client)
{
  struct DecClientHandle client_if = {
    client,         InitializedCb, HeadersDecodedCb, BufferDecodedCb,
    BufferRequestCb,
    PictureReadyCb, EndOfStreamCb, ReleasedCb,       NotifyErrorCb,
  };

  client->max_buffers = 0;
  DWLmemset(client->ext_buffers, 0, sizeof(client->ext_buffers));
#ifdef ASIC_TRACE_SUPPORT
  client->max_frm_buffers = 0;
  DWLmemset(client->ext_frm_buffers, 0, sizeof(client->ext_frm_buffers));
#endif
  struct DWLInitParam dwl_params = {DWL_CLIENT_TYPE_HEVC_DEC};

//  OpenTestHooks(client);
  setClientByTBCfg(client);

  struct DecSwHwBuild build = DecGetBuild();
  
  client->demuxer.inst = CreateDemuxer(/*&*/client);
  if (client->demuxer.inst == NULL) {
    printf("Failed to open demuxer (file missing or of wrong type?)\n");
    return -1;
  }

  if (client->test_params.extra_output_thread)
    /* Create the fifo to enable parallel output processing */
    FifoInit(2, &client->pic_fifo);

#ifdef ASIC_ONL_SIM
  client->dec_done = 0;
#else
  sem_init(&client->dec_done, 0, 0);
#endif

  enum DecCodec codec = DEC_HEVC;
  switch (client->demuxer.GetVideoFormat(client->demuxer.inst)) {
  case BITSTREAM_HEVC:
    codec = DEC_HEVC;
    dwl_params.client_type = DWL_CLIENT_TYPE_HEVC_DEC;
    break;
  case BITSTREAM_H264:
    codec = DEC_H264_H10P;
    dwl_params.client_type = DWL_CLIENT_TYPE_H264_DEC;
    break;
  case BITSTREAM_VP9:
    codec = DEC_VP9;
    dwl_params.client_type = DWL_CLIENT_TYPE_VP9_DEC;
    if (client->test_params.read_mode == STREAMREADMODE_FULLSTREAM) {
      fprintf(stderr, "Full-stream (-F) is not supported in VP9.\n");
      return -1;;
    }
    if (client->test_params.disable_display_order) {
      fprintf(stderr, "Disable display reorder (-R) is not supported in VP9.\n");
      return -1;
    }
    break;
  case BITSTREAM_VP7:
    fprintf(stderr, "Please check whether you are decoding a webm stream without WEBM_ENABLED.\n");
    break;
  case BITSTREAM_AVS2:
    codec = DEC_AVS2;
    dwl_params.client_type = DWL_CLIENT_TYPE_AVS2_DEC;
    break;
  default:
    return -1;
  }

  /* Create struct DWL. */
  client->dwl = DWLInit(&dwl_params);

  struct DecConfig config = {0};
  config.disable_picture_reordering = client->test_params.disable_display_order;
  config.rlc_mode = client->test_params.rlc_mode;
  config.concealment_mode = client->test_params.concealment_mode;
  config.align = client->test_params.align;
  config.decoder_mode = client->test_params.decoder_mode;
  config.tile_by_tile = client->test_params.tile_by_tile;
  config.fscale_cfg = client->test_params.fscale_cfg;
  memcpy(config.ppu_cfg, client->test_params.ppu_cfg, sizeof(config.ppu_cfg));
  memcpy(config.delogo_params, client->test_params.delogo_params, sizeof(config.delogo_params));
  switch (client->test_params.hw_format) {
  case DEC_OUT_FRM_TILED_4X4:
    config.output_format = DEC_OUT_FRM_TILED_4X4;
    break;
  case DEC_OUT_FRM_RASTER_SCAN: /* fallthrough */
  case DEC_OUT_FRM_PLANAR_420:
    if (!build.hw_config[0].pp_support) {
      fprintf(stderr, "Cannot do raster output; No PP support.\n");
      return -1;
    }
    config.output_format = DEC_OUT_FRM_RASTER_SCAN;
    break;
  default:
    return -1;
  }
  printf("Configuring hardware to output: %s\n",
         config.output_format == DEC_OUT_FRM_RASTER_SCAN
         ? "Semiplanar YCbCr 4:2:0 (four_cc 'NV12')"
         : "4x4 tiled YCbCr 4:2:0");
  config.dwl = dwl;
  config.dwl_inst = client->dwl;
  config.max_num_pics_to_decode = client->test_params.num_of_decoded_pics;
  config.use_video_compressor = client->test_params.compress_bypass ? 0 : 1;
  config.use_ringbuffer = client->test_params.is_ringbuffer;
  config.mvc = client->test_params.mvc;
  if (client->test_params.mc_enable) {
    config.mc_cfg.mc_enable = 1;
    config.mc_cfg.stream_consumed_callback = StreamBufferConsumedMC;
  } else {
    config.mc_cfg.mc_enable = 0;
    config.mc_cfg.stream_consumed_callback = NULL;
  }

  /* Initialize the decoder. */
  if (DecInit(codec, &client->decoder, &config, &client_if) != DEC_OK) {
    return -1;
  }

  /* The rest is driven by the callbacks and this thread just has to Wait
  * until decoder has finished it's job. */
#ifdef ASIC_ONL_SIM
  while (!client->dec_done) {
    usleep(100);
  }
#else
  sem_wait(&client->dec_done);
#endif
  ReleaseDemuxer(client);
  if (client->pic_fifo != NULL) FifoRelease(client->pic_fifo);
  if (client->dwl != NULL) DWLRelease(client->dwl);

#ifdef DECAPI_VER2
  DecDestroy(client->decoder);
#endif
  CloseTestHooks(client);

  return 0;
}

static void DispatchBufferForDecoding(struct Client* client,
                                      struct DecInput* buffer) {
  enum DecRet rv;
  i32 size = buffer->buffer.size;
  //memset(buffer->buffer.virtual_address, 0, size);
  i32 len = client->demuxer.ReadPacket(
              client->demuxer.inst,
              (u8*)buffer->buffer.virtual_address,
              buffer->stream, &size,
              client->test_params.is_ringbuffer);
  if (len <= 0) {/* EOS or error. */
    /* If reading was due to insufficient buffer size, try to realloc with
       sufficient size. */
    if (size > buffer->buffer.size) {
      i32 i;
      DEBUG_PRINT(("Trying to reallocate buffer to fit next buffer.\n"));
      for (i = 0; i < GetStreamBufferCount(client); i++) {
        if (client->buffers[i].buffer.virtual_address ==
            buffer->buffer.virtual_address) {
          DWLFreeLinear(client->dwl, &client->buffers[i].buffer);
          client->buffers[i].buffer.mem_type = DWL_MEM_TYPE_DPB;
          if (DWLMallocLinear(client->dwl, size, &client->buffers[i].buffer)) {
            fprintf(stderr, "No memory available for the stream buffer\n");
            DispatchEndOfStream(client);
            return;
          }
          /* Initialize stream to buffer start */
          client->buffers[i].stream[0] = (u8*)client->buffers[i].buffer.virtual_address;
          client->buffers[i].stream[1] = (u8*)client->buffers[i].buffer.virtual_address;
          DispatchBufferForDecoding(client, &client->buffers[i]);
          return;
        }
      }
    } else {
      DispatchEndOfStream(client);
      return;
    }
  }
  buffer->data_len = len;
  if (client->eos) return; /* Don't dispatch new buffers if EOS already done. */

  /* Decode the contents of the input stream buffer. */
  switch (rv = DecDecode(client->decoder, buffer)) {
  case DEC_OK:
    /* Everything is good, keep on going. */
    break;
  default:
    fprintf(stderr, "UNKNOWN ERROR!\n");
    DispatchEndOfStream(client);
    break;
  }
}

static void DispatchEndOfStream(struct Client* client) {
  if (!client->eos) {
    client->eos = 1;
    DecEndOfStream(client->decoder);
  }
}

static void InitializedCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  /* Internal testing feature: Override HW configuration parameters */
  HwconfigOverride(client->decoder, &tb_cfg);
  client->dec_initialized = 1;

  if (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_H264) {
    libav_init();
    if(libav_open(client->test_params.in_file_name) < 0) {
      DecRelease(client->decoder);
      return;
    }
  }

  /* Start the output handling thread, if needed. */
  if (client->test_params.extra_output_thread)
    pthread_create(&client->parallel_output_thread, NULL, ParallelOutput,
                   client);
  for (int i = 0; i < GetStreamBufferCount(client); i++) {
    client->buffers[i].buffer.mem_type = DWL_MEM_TYPE_CPU;
    if (DWLMallocLinear(client->dwl, DEFAULT_STREAM_BUFFER_SIZE,
                        &client->buffers[i].buffer)) {
      fprintf(stderr, "No memory available for the stream buffer\n");
      DecRelease(client->decoder);
      return;
    }
    /* Initialize stream to buffer start */
    client->buffers[i].stream[0] = (u8*)client->buffers[i].buffer.virtual_address;
    client->buffers[i].stream[1] = (u8*)client->buffers[i].buffer.virtual_address;
    /* Dispatch the first buffers for decoding. When decoder finished
     * decoding each buffer it will be refilled within the callback. */
    DispatchBufferForDecoding(client, &client->buffers[i]);
  }
}

static void HeadersDecodedCb(ClientInst inst, struct DecSequenceInfo info) {
  struct Client* client = (struct Client*)inst;
  int i, index;
#ifdef SUPPORT_DEC400
  int j;
#endif
  char local[256];
  char alt_file_name[256];
  DEBUG_PRINT(
    ("Headers: Width %d Height %d\n", info.pic_width, info.pic_height));
  DEBUG_PRINT(
    ("Headers: Cropping params: (%d, %d) %dx%d\n",
     info.crop_params.crop_left_offset, info.crop_params.crop_top_offset,
     info.crop_params.crop_out_width, info.crop_params.crop_out_height));
  DEBUG_PRINT(("Headers: MonoChrome = %d\n", info.is_mono_chrome));
  DEBUG_PRINT(("Headers: Pictures in DPB = %d\n", info.num_of_ref_frames));
  DEBUG_PRINT(("Headers: video_range %d\n", info.video_range));
  DEBUG_PRINT(("Headers: matrix_coefficients %d\n", info.matrix_coefficients));

  if (info.h264_base_mode == 1)
    client->test_params.is_ringbuffer = 0;

  /* Ajust user cropping params based on cropping params from sequence info. */
  if (info.crop_params.crop_left_offset != 0 ||
      info.crop_params.crop_top_offset != 0 ||
      info.crop_params.crop_out_width != info.pic_width ||
      info.crop_params.crop_out_height != info.pic_height) {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!client->test_params.ppu_cfg[i].enabled &&
          !client->test_params.fscale_cfg.fixed_scale_enabled)
        continue;

      if (!client->test_params.ppu_cfg[i].crop.enabled) {
        client->test_params.ppu_cfg[i].crop.x = info.crop_params.crop_left_offset;
        client->test_params.ppu_cfg[i].crop.y = info.crop_params.crop_top_offset;
        client->test_params.ppu_cfg[i].crop.width = (info.crop_params.crop_out_width +1) & ~0x1;
        client->test_params.ppu_cfg[i].crop.height = (info.crop_params.crop_out_height +1) & ~0x1;
      } else {
        client->test_params.ppu_cfg[i].crop.x += info.crop_params.crop_left_offset;
        client->test_params.ppu_cfg[i].crop.y += info.crop_params.crop_top_offset;
        if(!client->test_params.ppu_cfg[i].crop.width)
          client->test_params.ppu_cfg[i].crop.width = (info.crop_params.crop_out_width+1) & ~0x1;
        if(!client->test_params.ppu_cfg[i].crop.height)
          client->test_params.ppu_cfg[i].crop.height = (info.crop_params.crop_out_height+1) & ~0x1;
      }
      client->test_params.ppu_cfg[i].enabled = 1;
      client->test_params.ppu_cfg[i].crop.enabled = 1;
      client->test_params.pp_enabled = 1;
    }
  }

  if (client->yuvsink.inst == NULL) {
    if (client->test_params.out_file_name[0] == NULL) {
      u32 w = info.pic_width;
      u32 h = info.pic_height;
      char* fourcc[] = {"tiled4x4", "tiled8x4", "nv12", "iyuv", "nv21", "rgb"};
      if (!client->test_params.pp_enabled) {
        client->test_params.out_file_name[0] = out_file_name_buffer[0];
        sprintf(client->test_params.out_file_name[0], "out_%dx%d_%s.yuv", w, h,
                fourcc[client->test_params.format]);
      } else {
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if (i == 0 && client->test_params.fscale_cfg.fixed_scale_enabled) {
            u32 dscale_shift[9] = {0, 0, 1, 0, 2, 0, 0, 0, 3};
            u32 dscale_x_shift = dscale_shift[client->test_params.fscale_cfg.down_scale_x];
            u32 dscale_y_shift = dscale_shift[client->test_params.fscale_cfg.down_scale_y];
            w = client->test_params.ppu_cfg[i].crop.width ?
              client->test_params.ppu_cfg[i].crop.width : info.pic_width;
            h = client->test_params.ppu_cfg[i].crop.height ?
              client->test_params.ppu_cfg[i].crop.height : info.pic_height;

            client->test_params.out_file_name[i] = out_file_name_buffer[i];
            w = ((w / 2) >> dscale_x_shift) << 1;
            if (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_AVS2)
              h = ((h / 2) >> dscale_y_shift) << 1;
            else
              h = ((h / 2) >> dscale_y_shift) << 1 << info.is_interlaced;
            sprintf(client->test_params.out_file_name[i], "out_%dx%d_%s_%d.yuv", w, h,
              fourcc[client->test_params.format], i);
          } else if (client->test_params.ppu_cfg[i].enabled) {
            client->test_params.out_file_name[i] = out_file_name_buffer[i];
            w = client->test_params.ppu_cfg[i].crop2.enabled ?
                client->test_params.ppu_cfg[i].crop2.width :
                (client->test_params.ppu_cfg[i].scale.width ?
                client->test_params.ppu_cfg[i].scale.width :
                  (client->test_params.ppu_cfg[i].crop.width ?
                   client->test_params.ppu_cfg[i].crop.width : info.pic_width));
            h = client->test_params.ppu_cfg[i].crop2.enabled ?
                client->test_params.ppu_cfg[i].crop2.height :
                (client->test_params.ppu_cfg[i].scale.height ?
                client->test_params.ppu_cfg[i].scale.height :
                  (client->test_params.ppu_cfg[i].crop.height ?
                   client->test_params.ppu_cfg[i].crop.height : info.pic_height));

            client->test_params.out_file_name[i] = out_file_name_buffer[i];
            if (client->test_params.out_format_conv)
              index = client->test_params.format;
            else if (client->test_params.ppu_cfg[i].planar)
              index = 3;
            else if (client->test_params.ppu_cfg[i].tiled_e)
              index = 0;
            else if (client->test_params.ppu_cfg[i].cr_first)
              index = 4;
            else if (client->test_params.ppu_cfg[i].rgb)
              index = 5;
            else
              index = 2;
            sprintf(client->test_params.out_file_name[i], "out_%dx%d_%s_%d.yuv", w, h,
                    fourcc[index], i);
          }
        }
#ifdef SUPPORT_DEC400
        for (j = DEC_MAX_OUT_COUNT,i=DEC_MAX_OUT_COUNT; j < DEC_MAX_OUT_COUNT*2; i++, j++) {
          if(client->test_params.out_file_name[j-DEC_MAX_OUT_COUNT] != NULL) {
            client->test_params.out_file_name[i] = out_file_name_buffer[i];
            strcpy(alt_file_name, client->test_params.out_file_name[j-DEC_MAX_OUT_COUNT]);
            sprintf(alt_file_name+strlen(alt_file_name)-4, "_dec400_table.bin");
            strcpy(client->test_params.out_file_name[i],alt_file_name);
          }
        }
#endif
      }
    } else {
      if (client->test_params.pp_enabled) {
        strcpy(local, client->test_params.out_file_name[0]);
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          client->test_params.out_file_name[i] = out_file_name_buffer[i];
          strcpy(alt_file_name, local);
          sprintf(alt_file_name+strlen(alt_file_name)-4, "_%d.yuv", i);
          strcpy(client->test_params.out_file_name[i],alt_file_name);
        }
#ifdef SUPPORT_DEC400
        for (j = DEC_MAX_OUT_COUNT,i=DEC_MAX_OUT_COUNT; j < DEC_MAX_OUT_COUNT*2; i++, j++) {
          client->test_params.out_file_name[i] = out_file_name_buffer[i];
          strcpy(alt_file_name, local);
          sprintf(alt_file_name+strlen(alt_file_name)-4, "_pp_%d_dec400_table.bin", i-DEC_MAX_OUT_COUNT);
          strcpy(client->test_params.out_file_name[i],alt_file_name);
        }
#endif
      }
    }
    if ((client->yuvsink.inst = CreateSink(client)) == NULL) {
      fprintf(stderr, "Failed to create YUV sink\n");
      DispatchEndOfStream(client);
    }
  }
  client->demuxer.HeadersDecoded(client->demuxer.inst);
  client->new_hdr = 1;

//  if (info.num_of_ref_frames < NUM_REF_BUFFERS)
//    DecUseExtraFrmBuffers(client->decoder, NUM_REF_BUFFERS - info.num_of_ref_frames);

}

static u32 BufferRequestCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;

  struct DWLLinearMem mem;
  struct DecBufferInfo info;
  i32 i;

  if (client->new_hdr &&
      ((client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_HEVC)||
      (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_AVS2))) {
    while (client->num_pics_to_display) sched_yield();
    /* Release all external buffers for HEVC.
       For H264, buffers to be freed is returned in DecGetPictureBuffersInfo(),
       then freed one by one. */
    for (i = 0; i < client->max_buffers; i++) {
      DWLFreeLinear(client->dwl, &client->ext_buffers[i]);
      DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
    }
#ifdef ASIC_TRACE_SUPPORT
    for (i = 0; i < client->max_frm_buffers; i++) {
      DWLFreeRefFrm(client->dwl, &client->ext_frm_buffers[i]);
      DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
    }
    client->max_frm_buffers = 0;
#endif
    client->max_buffers = 0;
    client->new_hdr = 0;
  }
  while (1) {
    if (DecGetPictureBuffersInfo(client->decoder, &info) != DEC_WAITING_FOR_BUFFER)
      break;
    if (info.buf_to_free.virtual_address != NULL) {
#ifndef ASIC_TRACE_SUPPORT
      DWLFreeLinear(client->dwl, &info.buf_to_free);

      for (i = 0; i < client->max_buffers; i++) {
        if (client->ext_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
          DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
          break;
        }
      }
      ASSERT(i < client->max_buffers);
      if (i == client->max_buffers - 1) client->max_buffers--;
#else
      if (!client->test_params.pp_enabled) {
        DWLFreeRefFrm(client->dwl, &info.buf_to_free);

        for (i = 0; i < client->max_frm_buffers; i++) {
          if (client->ext_frm_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
            DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
            break;
          }
        }
        ASSERT(i < client->max_frm_buffers);
        if (i == client->max_frm_buffers - 1) client->max_frm_buffers--;
      } else {
        DWLFreeLinear(client->dwl, &info.buf_to_free);

        for (i = 0; i < client->max_buffers; i++) {
          if (client->ext_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
            DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
            break;
          }
        }
        ASSERT(i < client->max_buffers);
        if (i == client->max_buffers - 1) client->max_buffers--;
      }
#endif
    }

    if (info.next_buf_size != 0) {
#ifndef ASIC_TRACE_SUPPORT
      mem.mem_type = DWL_MEM_TYPE_DPB;
      if (DWLMallocLinear(client->dwl, info.next_buf_size, &mem) != DWL_OK)
        return -1;

      for (i = 0; i < client->max_buffers; i++) {
        if (client->ext_buffers[i].virtual_address == NULL) {
          break;
        }
      }
      if (i == client->max_buffers) client->max_buffers++;
      ASSERT(i < MAX_BUFFERS);
      client->ext_buffers[i] = mem;
#else
      if (!client->test_params.pp_enabled) {
        mem.mem_type = DWL_MEM_TYPE_DPB;
        if (DWLMallocRefFrm(client->dwl, info.next_buf_size, &mem) != DWL_OK)
          return -1;

        for (i = 0; i < client->max_frm_buffers; i++) {
          if (client->ext_frm_buffers[i].virtual_address == NULL) {
            break;
          }
        }
        if (i == client->max_frm_buffers) client->max_frm_buffers++;
        ASSERT(i < MAX_BUFFERS);
        client->ext_frm_buffers[i] = mem;
      } else {
        mem.mem_type = DWL_MEM_TYPE_DPB;
        if (DWLMallocLinear(client->dwl, info.next_buf_size, &mem) != DWL_OK)
          return -1;

        for (i = 0; i < client->max_buffers; i++) {
          if (client->ext_buffers[i].virtual_address == NULL) {
            break;
          }
        }
        if (i == client->max_buffers) client->max_buffers++;
        ASSERT(i < MAX_BUFFERS);
        client->ext_buffers[i] = mem;
      }
#endif
      if (DecSetPictureBuffers(client->decoder, &mem, 1) != DEC_WAITING_FOR_BUFFER)
        break;
    }
  }
  return 0;
}

static void BufferDecodedCb(ClientInst inst, struct DecInput* buffer) {
  struct Client* client = (struct Client*)inst;
  if (!client->eos) {
    DispatchBufferForDecoding(client, buffer);
  }
}

static void PictureReadyCb(ClientInst inst, struct DecPicturePpu picture) {
  static char* pic_types[] = {"        IDR", "Non-IDR (P)", "Non-IDR (B)"};
  static char* avs2_types[] = {" I", " P", " B", " F", " S", " G", "GB"};
  struct Client* client = (struct Client*)inst;
  client->num_of_output_pics++;
  client->num_pics_to_display++;
  if (BITSTREAM_AVS2 == client->demuxer.GetVideoFormat(client->demuxer.inst)) {
    DEBUG_PRINT(("PIC %2d/%2d, type %s,", client->num_of_output_pics,
                 picture.pictures[0].picture_info.pic_id,
                 avs2_types[picture.pictures[0].picture_info.pic_coding_type]));
  } else {
    DEBUG_PRINT(("PIC %2d/%2d, type %s,", client->num_of_output_pics,
                 picture.pictures[0].picture_info.pic_id,
                 pic_types[picture.pictures[0].picture_info.pic_coding_type]));
  }
  if (picture.pictures[0].picture_info.cycles_per_mb) {
    client->cycle_count += picture.pictures[0].picture_info.cycles_per_mb;
    DEBUG_PRINT((" %4d cycles / mb,", picture.pictures[0].picture_info.cycles_per_mb));
  }
  DEBUG_PRINT((" %d x %d, Crop: (%d, %d), %d x %d %s\n",
               picture.pictures[0].sequence_info.pic_width,
               picture.pictures[0].sequence_info.pic_height,
               picture.pictures[0].sequence_info.crop_params.crop_left_offset,
               picture.pictures[0].sequence_info.crop_params.crop_top_offset,
               picture.pictures[0].sequence_info.crop_params.crop_out_width,
               picture.pictures[0].sequence_info.crop_params.crop_out_height,
               picture.pictures[0].picture_info.is_corrupted ? "CORRUPT" : ""));
  if (client->test_params.extra_output_thread) {
    struct DecPicturePpu* copy = malloc(sizeof(struct DecPicturePpu));
    *copy = picture;
    FifoPush(client->pic_fifo, copy, FIFO_EXCEPTION_DISABLE);
  } else {
    PostProcessPicture(client, &picture);
  }
  client->num_pics_to_display--;
}

static void EndOfStreamCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  client->eos = 1;

  if (client->test_params.extra_output_thread) {
    /* We're done, wait for the output to Finish it's job. */
    FifoPush(client->pic_fifo, NULL, FIFO_EXCEPTION_DISABLE);
    pthread_join(client->parallel_output_thread, NULL);
  }
  DecRelease(client->decoder);
}

static void ReleasedCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  for (int i = 0; i < NUM_OF_STREAM_BUFFERS; i++) {
    if (client->buffers[i].buffer.virtual_address) {
      DWLFreeLinear(client->dwl, &client->buffers[i].buffer);
    }
  }
  if(client->cycle_count && client->num_of_output_pics)
    DEBUG_PRINT(("\nAverage cycles/MB: %4d\n", client->cycle_count/client->num_of_output_pics));
  ReleaseSink(client);

  for (i32 i = 0; i < client->max_buffers; i++) {
    if (client->ext_buffers[i].virtual_address != NULL) {
      DWLFreeLinear(client->dwl, &client->ext_buffers[i]);
      DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
    }
  }
  client->max_buffers = 0;
#ifdef ASIC_TRACE_SUPPORT
  for (i32 i = 0; i < client->max_frm_buffers; i++) {
    if (client->ext_frm_buffers[i].virtual_address != NULL) {
      DWLFreeRefFrm(client->dwl, &client->ext_frm_buffers[i]);
      DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
    }
  }
  client->max_frm_buffers = 0;
#endif

  if (client->test_params.mc_enable &&
     (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_H264)) {
    libav_release();
  }

  #ifdef ASIC_ONL_SIM
  client->dec_done = 1;
  #else
  sem_post(&client->dec_done);
  #endif
}

static void NotifyErrorCb(ClientInst inst, u32 pic_id, enum DecRet rv) {
  struct Client* client = (struct Client*)inst;

  if (!client->dec_initialized) {
    fprintf(stderr, "Decoder initialize error: %d\n", rv);
    DecRelease(client->decoder);
  } else {
    fprintf(stderr, "Decoding error on pic_id %d: %d\n", pic_id, rv);
    if (rv == DEC_MEMFAIL ||
        rv == DEC_PARAM_ERROR ||
        rv == DEC_INFOPARAM_ERROR ||
        rv == DEC_SYSTEM_ERROR ||
        rv == DEC_FATAL_SYSTEM_ERROR) {
      if (rv == DEC_SYSTEM_ERROR) {
        /* TODO: Process fatal system error here, such as reset HW. */
      }
      /* There's serious decoding error, so we'll consider it as end of stream to
         get the pending pictures out of the decoder. */
      DispatchEndOfStream(client);
    }
  }
}

static void PostProcessPicture(struct Client* client,
                               struct DecPicturePpu* picture) {
  struct DecPicture in = picture->pictures[0];
  u32 i;
  av_unused u32 uncompressed_tile4x4 = 0; /* uncompressed reference tile4x4 or pp */
  av_unused u32 pp_ch_h, pp_ch_w;
  static u32 pic_count = 0;
  pic_count++;
  if (pic_count == 10)
    pic_count = 1;

  for (i = 0; i < (client->test_params.pp_enabled ? DEC_MAX_OUT_COUNT : 1); i++) {
    if (picture->pictures[i].luma.virtual_address == NULL)
      continue;

    /* Current output is available */
    uncompressed_tile4x4 = 0;
    in = picture->pictures[i];
    pp_ch_w = in.pic_width;
    pp_ch_h = in.pic_height/2;
    if (client->test_params.sink_type == SINK_NULL /*|| pic_count != 2*/) goto PIC_CONSUMED;

    if (IS_PIC_PACKED_RGB(picture->pictures[i].picture_info.format) ||
        IS_PIC_PLANAR_RGB(picture->pictures[i].picture_info.format)) {
      client->yuvsink.WritePicture(client->yuvsink.inst, in, i);
      continue;
    }
    /* PP output may have partial tile4x4 at edge. */
    if (picture->pictures[i].picture_info.format == DEC_OUT_FRM_TILED_4X4) {
      pp_ch_w = (in.pic_width + 3) & ~0x3;
      pp_ch_h = (in.pic_height/2 + 3) & ~0x3;
      in.pic_width = (in.pic_width + 3) & ~0x3;
      in.pic_height = (in.pic_height + 3) & ~0x3;

      in.sequence_info.pic_width = in.pic_width;
      in.sequence_info.pic_height = in.pic_height;
      in.sequence_info.pic_stride = in.pic_stride;
      in.sequence_info.pic_stride_ch = in.pic_stride_ch;
    }

    /* Compressed tiled data should be output without being converted to 16 bits.
       It's treated as a special picture to output. */
    if (picture->pictures[i].picture_info.format == DEC_OUT_FRM_RFC) {
      u32 bit_depth = ((picture->pictures[i].sequence_info.bit_depth_luma&0xff) == 8 &&
                       (picture->pictures[i].sequence_info.bit_depth_chroma&0xff) == 8) ? 8 : 10;
      in.sequence_info.pic_width *= 4 * bit_depth / 8;
      in.sequence_info.pic_height /= 4;
      in.sequence_info.bit_depth_luma = 8;
      in.sequence_info.bit_depth_chroma = 8;
    } else if (picture->pictures[i].picture_info.format == DEC_OUT_FRM_TILED_4X4 ||
               picture->pictures[i].picture_info.format == DEC_OUT_FRM_YUV400TILE ||
               picture->pictures[i].picture_info.format == DEC_OUT_FRM_YUV400TILE_P010 ||
               picture->pictures[i].picture_info.format == DEC_OUT_FRM_YUV400TILE_1010) {
      /* Set pic_width as a tile row tricky for YuvfilterConvertPixel16Bits(),
         for pic_stride is based on a tile row. */
      in.sequence_info.pic_width *= 4;
      in.sequence_info.pic_height /= 4;
      uncompressed_tile4x4 = 1;
    } else if (picture->pictures[i].picture_info.format == DEC_OUT_FRM_YUV420SP_1010) {
      /* 4 bytes per 3 pixels */
      /* Simply treat 101010 formats as YUV420 8-bit. */
      in.sequence_info.pic_width = (in.sequence_info.pic_width + 2) / 3 * 4;
      in.sequence_info.bit_depth_luma = 8;
      in.sequence_info.bit_depth_chroma = 8;
      in.picture_info.format = DEC_OUT_FRM_YUV420SP;
    }

#ifndef SUPPORT_DEC400
#ifdef TB_PP
    u32 pic_ch_size, pic_size;
    struct DecPicture copy = in;
    pic_size = in.sequence_info.pic_width *
               in.sequence_info.pic_height * 2;  // one pixel in 16 bits
    if (uncompressed_tile4x4)
      pic_ch_size = pp_ch_w * pp_ch_h * 2;
    else
      pic_ch_size = pic_size / 2;

    copy.luma.virtual_address = malloc(pic_size);
    if (in.chroma.virtual_address)
      copy.chroma.virtual_address = malloc(pic_ch_size);

    YuvfilterConvertPixel16Bits(&in, &copy);

    /* Recover pic_width. */
    if (uncompressed_tile4x4) {
      in.sequence_info.pic_width /= 4;
      in.sequence_info.pic_height *= 4;
      copy.sequence_info.pic_width /= 4;
      copy.sequence_info.pic_height *= 4;
    }

    if (client->test_params.out_format_conv) {
      if (uncompressed_tile4x4) {
        /* Tile -> other formats conversion allowed only when compression is not enabled.*/
        if (client->test_params.display_cropped &&
            (client->test_params.format == DEC_OUT_FRM_TILED_4X4)) {
          YuvfilterTiledcrop(&copy);
        } else if (client->test_params.format == DEC_OUT_FRM_RASTER_SCAN) {
          YuvfilterTiled2Semiplanar(&copy);
          if (client->test_params.display_cropped) YuvfilterSemiplanarcrop(&copy);
        } else if (client->test_params.format == DEC_OUT_FRM_PLANAR_420) {
          if (client->test_params.display_cropped) {
            YuvfilterTiled2Semiplanar(&copy);
            YuvfilterSemiplanarcrop(&copy);
            YuvfilterSemiplanar2Planar(&copy);
          } else {
            YuvfilterTiled2Planar(&copy);
          }
        }
      } else if (IS_PIC_SEMIPLANAR(in.picture_info.format)) {
        if (client->test_params.display_cropped) YuvfilterSemiplanarcrop(&copy);
        if (client->test_params.format == DEC_OUT_FRM_PLANAR_420)
          YuvfilterSemiplanar2Planar(&copy);
      }
    }
    YuvfilterPrepareOutput(&copy);
    client->yuvsink.WritePicture(client->yuvsink.inst, copy, i);

    free(copy.luma.virtual_address);
    free(copy.chroma.virtual_address);
#else
    client->yuvsink.WritePicture(client->yuvsink.inst, in, i);
#endif
#else
    client->yuvsink.WritePicture(client->yuvsink.inst, in, i);
#endif
  }

PIC_CONSUMED:
  DecPictureConsumed(client->decoder, picture);
}

static void* ParallelOutput(void* arg) {
  struct Client* client = arg;
  struct DecPicturePpu* pic = NULL;
  do {
    FifoPop(client->pic_fifo, (void**)&pic, FIFO_EXCEPTION_DISABLE);
    if (pic == NULL) {
      DEBUG_PRINT(("END-OF-STREAM received in output thread\n"));
      return NULL;
    }
    PostProcessPicture(client, pic);
  } while (1);
  return NULL;
}

static const void* CreateDemuxer(struct Client* client) {
  Demuxer demuxer;
  enum FileFormat ff = client->test_params.file_format;
  if (ff == FILEFORMAT_AUTO_DETECT) {
    if (strstr(client->test_params.in_file_name, ".ivf") ||
        strstr(client->test_params.in_file_name, ".vp9"))
      ff = FILEFORMAT_IVF;
    else if (strstr(client->test_params.in_file_name, ".webm"))
      ff = FILEFORMAT_WEBM;
    else if ((strstr(client->test_params.in_file_name, ".avs")) ||
              (strstr(client->test_params.in_file_name, ".avs2")))
      ff = FILEFORMAT_AVS2;
    else if (strstr(client->test_params.in_file_name, ".hevc"))
      ff = FILEFORMAT_BYTESTREAM;
    else if (strstr(client->test_params.in_file_name, ".h264") ||
             strstr(client->test_params.in_file_name, ".264"))
      ff = FILEFORMAT_BYTESTREAM_H264;
  }
  if (ff == FILEFORMAT_IVF || ff == FILEFORMAT_WEBM) {
    demuxer.open = VpxRdrOpen;
    demuxer.GetVideoFormat = VpxRdrIdentifyFormat;
    demuxer.HeadersDecoded = VpxRdrHeadersDecoded;
    demuxer.ReadPacket = VpxRdrReadFrame;
    demuxer.close = VpxRdrClose;
  } else if (ff == FILEFORMAT_AVS2) {
    demuxer.open = ByteStreamParserOpen;
    demuxer.GetVideoFormat = ByteStreamParserIdentifyFormatAvs2;
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
    demuxer.ReadPacket = ByteStreamParserReadFrameAvs2;
    demuxer.close = ByteStreamParserClose;
  } else if (ff == FILEFORMAT_BYTESTREAM) {
    demuxer.open = ByteStreamParserOpen;
    demuxer.GetVideoFormat = ByteStreamParserIdentifyFormat;
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
    demuxer.ReadPacket = ByteStreamParserReadFrame;
    demuxer.close = ByteStreamParserClose;
  } else if (ff == FILEFORMAT_BYTESTREAM_H264) {
    demuxer.open = ByteStreamParserOpen;
    demuxer.GetVideoFormat = ByteStreamParserIdentifyFormatH264;
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
#ifdef USE_LIBAV
    demuxer.ReadPacket = ByteStreamParserLibAVReadFrameH264;
#else
    demuxer.ReadPacket = ByteStreamParserReadFrameH264;
#endif
    demuxer.close = ByteStreamParserClose;
  } else {
    /* TODO(vmr): In addition to file suffix, consider also looking into
     *            shebang of the files. */
    return NULL;
  }

  demuxer.inst = demuxer.open(client->test_params.in_file_name,
                              client->test_params.read_mode);
  /* If needed, instantiate error simulator to process data from demuxer. */
  if (ErrorSimulationNeeded(client))
    demuxer.inst =
      ErrorSimulatorInject(&demuxer, client->test_params.error_sim);
  client->demuxer = demuxer;
  return demuxer.inst;
}

static void ReleaseDemuxer(struct Client* client) {
  client->demuxer.close(client->demuxer.inst);
}

static const void* CreateSink(struct Client* client) {
  YuvSink yuvsink;
  switch (client->test_params.sink_type) {
  case SINK_FILE_SEQUENCE:
    yuvsink.open = FilesinkOpen;
    yuvsink.WritePicture = FilesinkWritePic;
    yuvsink.close = FilesinkClose;
    break;
  case SINK_FILE_PICTURE:
    yuvsink.open = NullsinkOpen;
    yuvsink.WritePicture = FilesinkWriteSinglePic;
    yuvsink.close = NullsinkClose;
    break;
  case SINK_MD5_SEQUENCE:
    yuvsink.open = Md5sinkOpen;
    yuvsink.WritePicture = Md5sinkWritePic;
    yuvsink.close = Md5sinkClose;
    break;
  case SINK_MD5_PICTURE:
    yuvsink.open = md5perpicsink_open;
    yuvsink.WritePicture = md5perpicsink_write_pic;
    yuvsink.close = md5perpicsink_close;
    break;
#ifdef SDL_ENABLED
  case SINK_SDL:
    yuvsink.open = SdlSinkOpen;
    yuvsink.WritePicture = SdlSinkWrite;
    yuvsink.close = SdlSinkClose;
    break;
#endif
  case SINK_NULL:
    yuvsink.open = NullsinkOpen;
    yuvsink.WritePicture = NullsinkWrite;
    yuvsink.close = NullsinkClose;
    break;
  default:
    assert(0);
  }
  yuvsink.inst = yuvsink.open((const char **)client->test_params.out_file_name);
  client->yuvsink = yuvsink;
  return client->yuvsink.inst;
}

static void ReleaseSink(struct Client* client) {
  if (client->yuvsink.inst) {
    client->yuvsink.close(client->yuvsink.inst);
  }
}

static u8 ErrorSimulationNeeded(struct Client* client) {
  return client->test_params.error_sim.corrupt_headers ||
         client->test_params.error_sim.truncate_stream ||
         client->test_params.error_sim.swap_bits_in_stream ||
         client->test_params.error_sim.lose_packets;
}

static i32 GetStreamBufferCount(struct Client* client) {
  return client->test_params.read_mode == STREAMREADMODE_FULLSTREAM
         ? 1
         : NUM_OF_STREAM_BUFFERS;
}

/* These global values are found from commonconfig.c */
extern u32 dec_stream_swap;
extern u32 dec_pic_swap;
extern u32 dec_dirmv_swap;
extern u32 dec_tab0_swap;
extern u32 dec_tab1_swap;
extern u32 dec_tab2_swap;
extern u32 dec_tab3_swap;
extern u32 dec_rscan_swap;
extern u32 dec_comp_tab_swap;
extern u32 dec_burst_length;
extern u32 dec_bus_width;
extern u32 dec_apf_treshold;
extern u32 dec_apf_disable;
extern u32 dec_clock_gating;
extern u32 dec_clock_gating_runtime;
extern u32 dec_ref_double_buffer;
extern u32 dec_timeout_cycles;
extern u32 dec_axi_id_rd;
extern u32 dec_axi_id_rd_unique_enable;
extern u32 dec_axi_id_wr;
extern u32 dec_axi_id_wr_unique_enable;
extern u32 dec_pp_in_blk_size;

static void setClientByTBCfg(struct Client* client)
{
  /* Override PPU1-3 parameters with tb.cfg */
  ResolvePpParamsOverlap(&client->test_params, &tb_cfg.pp_units_params[0],
                         !tb_cfg.pp_params.pipeline_e);
  
  client->test_params.rlc_mode = TBGetDecRlcModeForced(&tb_cfg);

   /* Read the error simulation parameters. */
  struct ErrorSimulationParams* error_params = &client->test_params.error_sim;
  error_params->seed = tb_cfg.tb_params.seed_rnd;
  error_params->truncate_stream = TBGetTBStreamTruncate(&tb_cfg);
  error_params->corrupt_headers = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  if (strcmp(tb_cfg.tb_params.stream_bit_swap, "0") != 0) {
    error_params->swap_bits_in_stream = 1;
    memcpy(error_params->swap_bit_odds, tb_cfg.tb_params.stream_bit_swap,
           sizeof(tb_cfg.tb_params.stream_bit_swap));
  }
  if (strcmp(tb_cfg.tb_params.stream_packet_loss, "0") != 0) {
    error_params->lose_packets = 1;
    memcpy(error_params->packet_loss_odds, tb_cfg.tb_params.stream_packet_loss,
           sizeof(tb_cfg.tb_params.stream_packet_loss));
  }
  DEBUG_PRINT(("TB Seed Rnd %d\n", error_params->seed));
  DEBUG_PRINT(("TB Stream Truncate %d\n", error_params->truncate_stream));
  DEBUG_PRINT(("TB Stream Header Corrupt %d\n", error_params->corrupt_headers));
  DEBUG_PRINT(("TB Stream Bit Swap %d; odds %s\n",
               error_params->swap_bits_in_stream, error_params->swap_bit_odds));
  DEBUG_PRINT(("TB Stream Packet Loss %d; odds %s\n",
               error_params->lose_packets, error_params->packet_loss_odds));
  if(exist_f_tbcfg == 1) {
    client->test_params.concealment_mode = TBGetDecErrorConcealment(&tb_cfg);
  }
}
static void OpenTestHooks(const struct Client* client) {
  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  FILE* f_tbcfg = fopen("tb.cfg", "r");
  if (f_tbcfg == NULL) {
    DEBUG_PRINT(("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n"));
    DEBUG_PRINT(("USING DEFAULT CONFIGURATION\n"));
  } else {
    exist_f_tbcfg = 1;
    fclose(f_tbcfg);
    if (TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE) return;
    if (TBCheckCfg(&tb_cfg) != 0) return;
  }
  if (client->test_params.cache_enable || client->test_params.shaper_enable) {
    tb_cfg.dec_params.cache_support = 1;
  } else {
    tb_cfg.dec_params.cache_support = 0;
  }
  tb_cfg.shaper_bypass = client->test_params.shaper_bypass;
  tb_cfg.cache_enable = client->test_params.cache_enable;
  tb_cfg.shaper_enable = client->test_params.shaper_enable;

#if 0
  /* Override PPU1-3 parameters with tb.cfg */
  ResolvePpParamsOverlap(&client->test_params, &tb_cfg.pp_units_params[0],
                         !tb_cfg.pp_params.pipeline_e);
#endif

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#if 0
  client->test_params.rlc_mode = TBGetDecRlcModeForced(&tb_cfg);
#endif

  if (client->test_params.trace_target) tb_cfg.tb_params.extra_cu_ctrl_eof = 1;

  if (client->test_params.hw_traces) {
#ifdef ASIC_TRACE_SUPPORT
    if (!OpenTraceFiles())
      DEBUG_PRINT(
        ("UNABLE TO OPEN TRACE FILE(S) Do you have a trace.cfg "
         "file?\n"));
#else
    DEBUG_PRINT(
        ("UNABLE TO OPEN TRACE FILE(S) "
         "Do you enable ASIC_TRACE_SUPPORT when building?\n"));
#endif
  }

  if (f_tbcfg != NULL) {
    dec_stream_swap = tb_cfg.dec_params.strm_swap;
    dec_pic_swap = tb_cfg.dec_params.pic_swap;
    dec_dirmv_swap = tb_cfg.dec_params.dirmv_swap;
    dec_tab0_swap = tb_cfg.dec_params.tab0_swap;
    dec_tab1_swap = tb_cfg.dec_params.tab1_swap;
    dec_tab2_swap = tb_cfg.dec_params.tab2_swap;
    dec_tab3_swap = tb_cfg.dec_params.tab3_swap;
    dec_rscan_swap = tb_cfg.dec_params.rscan_swap;
    dec_comp_tab_swap = tb_cfg.dec_params.comp_tab_swap;
    dec_burst_length = tb_cfg.dec_params.max_burst;
    dec_bus_width = TBGetDecBusWidth(&tb_cfg);
    dec_apf_treshold = tb_cfg.dec_params.apf_threshold_value;
    dec_apf_disable = tb_cfg.dec_params.apf_disable;
    dec_clock_gating = TBGetDecClockGating(&tb_cfg);
    dec_clock_gating_runtime = TBGetDecClockGatingRuntime(&tb_cfg);
    dec_ref_double_buffer = tb_cfg.dec_params.ref_double_buffer_enable;
    dec_timeout_cycles = tb_cfg.dec_params.timeout_cycles;
    dec_axi_id_rd = tb_cfg.dec_params.axi_id_rd;
    dec_axi_id_rd_unique_enable = tb_cfg.dec_params.axi_id_rd_unique_enable;
    dec_axi_id_wr = tb_cfg.dec_params.axi_id_wr;
    dec_axi_id_wr_unique_enable = tb_cfg.dec_params.axi_id_wr_unique_enable;
    if (tb_cfg.pp_params.pre_fetch_height == 16)
      dec_pp_in_blk_size = 0;
    else if (tb_cfg.pp_params.pre_fetch_height == 64)
      dec_pp_in_blk_size = 1;
#if 0 //set it if exist_f_tbcfg == 1
    client->test_params.concealment_mode = TBGetDecErrorConcealment(&tb_cfg);
#endif
  }

#ifdef ASIC_TRACE_SUPPORT
  /* determine test case id from input file name (if contains "case_") */
  {
    char* pc, *pe;
    char in[256] = {0};
    strncpy(in, client->test_params.in_file_name,
            strnlen(client->test_params.in_file_name, 256));
    pc = strstr(in, "case_");
    if (pc != NULL) {
      pc += 5;
      pe = strstr(pc, "/");
      if (pe == NULL) pe = strstr(pc, ".");
      if (pe != NULL) {
        *pe = '\0';
        test_case_id = atoi(pc);
      }
    }
  }
#endif
}

static void CloseTestHooks(struct Client* client) {
  if (client->test_params.hw_traces) {
#ifdef ASIC_TRACE_SUPPORT
    CloseTraceFiles();
#endif
  }
}

static int HwconfigOverride(DecInst dec_inst, struct TBCfg* tbcfg) {
  u32 data_discard = TBGetDecDataDiscard(&tb_cfg);
  u32 latency_comp = tb_cfg.dec_params.latency_compensation;
  u32 output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  u32 bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  u32 asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  u32 output_format = TBGetDecOutputFormat(&tb_cfg);
  u32 service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  DEBUG_PRINT(("struct TBCfg: Decoder Data Discard %d\n", data_discard));
  DEBUG_PRINT(("struct TBCfg: Decoder Latency Compensation %d\n", latency_comp));
  DEBUG_PRINT(
    ("struct TBCfg: Decoder Output Picture Endian %d\n", output_picture_endian));
  DEBUG_PRINT(("struct TBCfg: Decoder Bus Burst Length %d\n", bus_burst_length));
  DEBUG_PRINT(
    ("struct TBCfg: Decoder Asic Service Priority %d\n", asic_service_priority));
  DEBUG_PRINT(("struct TBCfg: Decoder Output Format %d\n", output_format));
  DEBUG_PRINT(
    ("struct TBCfg: Decoder Service Merge Disable %d\n", service_merge_disable));

  /* TODO(vmr): Enable these, remove what's not needed, add what's needed.
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_LATENCY,
                 latency_comp);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_CLK_GATE_E,
                 clock_gating);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_OUT_TILED_E,
                 output_format);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_OUT_ENDIAN,
                 output_picture_endian);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_MAX_BURST,
                 bus_burst_length);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_DEC_DATA_DISC_E,
                 data_discard);
  SetDecRegister(((struct HevcDecContainer *) dec_inst)->hevc_regs,
                 HWIF_SERV_MERGE_DIS,
                 service_merge_disable);
  */

  return 0;
}

void StreamBufferConsumedMC(void *stream, void *p_user_data) {
  int i;
  int found = 0;

  if (p_user_data == NULL)
    return;

  pthread_mutex_lock(&mutex);
  struct Client* client = (struct Client*)p_user_data;
  for (i = 0; i < GetStreamBufferCount(client); i++) {
    if ((u8 *)stream >= (u8 *)client->buffers[i].buffer.virtual_address &&
        (u8 *)stream < (u8 *)client->buffers[i].buffer.virtual_address + client->buffers[i].buffer.size) {
      found = 1;
      break;
    }
  }

  if (found == 0)
    printf("Stream buffer not found.\n");

  BufferDecodedCb(client, &client->buffers[i]);
  pthread_mutex_unlock(&mutex);
}

static int parse_stream_cfg(const char *streamcfg, struct Client *client)
{
  i32 ret, i;
  char *p;
  FILE *fp = fopen(streamcfg, "r");
  ;
  if(fp == NULL)
    return NOK;
  if((ret = fseek(fp, 0, SEEK_END)) < 0)
    return NOK;
  i32 fsize = ftell(fp);
  if(fsize < 0)
    return NOK;
  if((ret = fseek(fp, 0, SEEK_SET)) < 0)
    return NOK;
  client->argv = (char **)malloc(MAXARGS*sizeof(char *));
  client->argv[0] = (char *)malloc(fsize);
  ret = fread(client->argv[0], 1, fsize, fp);
  if(ret < 0)
    return ret;
  fclose(fp);

  p = client->argv[0];
  for(i = 1; i < MAXARGS; i++) {
    while(*p && *p <= 32)
      ++p;
    if(!*p) break;
    client->argv[i] = p;
    while(*p > 32)
      ++p;
    *p = 0; ++p;
  }
  client->argc = i;
  if (ParseParams(client->argc, client->argv, &client->test_params))
  {
    Error(2, ERR, "Input parameter error");
    return NOK;
  }
  return OK;
}
