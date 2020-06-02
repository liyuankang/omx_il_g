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

#include "decapi.h"
#include "decapi_trace.h"
#include "fifo.h"
#include "sw_util.h"
#include "version.h"
#include "dwl.h"
#include "decapicommon.h"
#include "sw_performance.h"
#include "ppu.h"
#ifdef ENABLE_HEVC_SUPPORT
#include "hevcdecapi.h"
#endif /* ENABLE_HEVC_SUPPORT */
#ifdef ENABLE_H264_SUPPORT
#include "h264decapi.h"
#endif /* ENABLE_H264_SUPPORT */
#ifdef ENABLE_VP9_SUPPORT
#include "vp9decapi.h"
#endif /* ENABLE_VP9_SUPPORT */
#ifdef ENABLE_AVS2_SUPPORT
#include "avs2decapi.h"
#endif /* ENABLE_AVS2_SUPPORT */

#include<stdlib.h>
#include <string.h>
#include<stdio.h>
#define MAX_FIFO_CAPACITY (6)

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define DOWN_SCALE_SIZE(w, ds) (((w)/(ds)) & ~0x1)
DECLARE_PERFORMANCE_STATIC(decode_total)
DECLARE_PERFORMANCE_STATIC(decode_pre_hw)
DECLARE_PERFORMANCE_STATIC(decode_post_hw)

enum DecodingState {
  DECODER_WAITING_HEADERS
  , DECODER_WAITING_RESOURCES
  , DECODER_DECODING
  , DECODER_SHUTTING_DOWN
  /* Decoder has been shutdown due to error.
     No more decoding but can output pending pictures */
  , DECODER_ERROR_TO_SHUTTING_DOWN
#ifdef DECAPI_VER2
  , DECODER_TERMINATED
#endif
};

/* thread for low latency feature */
typedef void* task_handle;
typedef void* (*task_func)(void*);

/* variables for low latency feature */
volatile struct strmInfo send_strm_info;
u32 pic_decoded;
u32 frame_len;
u32 send_len;
u32 decoder_release;
u32 process_end_flag;
pthread_mutex_t send_mutex;
sem_t curr_frame_end_sem;
sem_t next_frame_start_sem;
/* func for low latency feature */
void wait_for_task_completion(task_handle task);
task_handle run_task(task_func func, void* param);
void* send_bytestrm_task(void* param);

u32 low_latency;
task_handle task = NULL;
u32 data_consumed = 0;

struct DecOutput {
  u8* strm_curr_pos;
  addr_t strm_curr_bus_address;
  u32 data_left;
  u8* strm_buff;
  addr_t strm_buff_bus_address;
  u32 buff_size;
};

struct Command {
  enum {
    COMMAND_INIT
    , COMMAND_DECODE
    , COMMAND_SETBUFFERS
    , COMMAND_END_OF_STREAM
    , COMMAND_RELEASE
  } id;
  struct {
    struct DecConfig config;
    struct DecInput input;
  } params;
};

struct DecoderWrapper {
  void* inst;
  enum DecRet (*init)(const void** inst, struct DecConfig *config,
                      const void *dwl);
  enum DecRet (*GetInfo)(void* inst, struct DecSequenceInfo* info);
  enum DecRet (*SetInfo)(void* inst, struct DecConfig *info, struct DecSequenceInfo* Info);
  enum DecRet (*Decode)(void* inst, struct DWLLinearMem *input, struct DecOutput* output,
                        u8* stream, u32 strm_len, struct DWL *dwl, u32 pic_id, void *p_user_data);
  enum DecRet (*NextPicture)(void* inst, struct DecPicturePpu* pic,
                             struct DWL *dwl);
  enum DecRet (*PictureConsumed)(void* inst, struct DecPicturePpu *pic,
                                 struct DWL *dwl);
  enum DecRet (*EndOfStream)(void* inst);
  void (*Release)(void* inst);
  enum DecRet (*GetBufferInfo)(void *inst, struct DecBufferInfo *buf_info);
  enum DecRet (*AddBuffer)(void *inst, struct DWLLinearMem *buf);
  enum DecRet (*UseExtraFrmBuffers)(const void *inst, u32 n);
  void (*UpdateStrmInfoCtrl)(void *inst, u32 last_flag, u32 strm_bus_addr);
  void (*UpdateStrm)(struct strmInfo info);
};

typedef struct DecoderInstance_ {
  struct DecoderWrapper dec;
  enum DecodingState state;
  FifoInst input_queue;
  pthread_t decode_thread;
  pthread_t output_thread;
  pthread_mutex_t cs_mutex;
  pthread_mutex_t eos_mutex;
  pthread_mutex_t resource_mutex;
  pthread_cond_t eos_cond;
  pthread_cond_t resource_cond;
  u8 eos_ready;
  u8 resources_acquired;
  struct Command* current_command;
  struct DecOutput buffer_status;
  struct DecClientHandle client;
  struct DecSequenceInfo sequence_info;
  struct DWL dwl;
  const void* dwl_inst;
  u8 pending_eos; /* TODO(vmr): slightly ugly, figure out better way. */
  u32 max_num_of_decoded_pics;
  u32 num_of_decoded_pics;
  struct DecInput prev_input;
  /* TODO(mheikkinen) this is a temporary handler for stream decoded callback
   * until HEVC gets delayed sync implementation */
  void (*stream_decoded)(void* inst);
  u8 picture_in_display;
  struct DecConfig config;
} DecoderInstance;

/* Decode loop and handlers for different states. */
static void* DecodeLoop(void* arg);
static void Initialize(DecoderInstance* inst);
static void WaitForResources(DecoderInstance* inst);
static void Decode(DecoderInstance* inst);
static void EndOfStream(DecoderInstance* inst);
static void Release(DecoderInstance* inst);

/* Output loop. */
static void* OutputLoop(void* arg);

/* Local helpers to manage and protect the state of the decoder. */
static enum DecodingState GetState(DecoderInstance* inst);
static void SetState(DecoderInstance* inst, enum DecodingState state);

/* Hevc codec wrappers. */
#ifdef ENABLE_HEVC_SUPPORT
static enum DecRet HevcInit(const void** inst, struct DecConfig *config,
                            const void *dwl);
static enum DecRet HevcGetInfo(void* inst, struct DecSequenceInfo* info);
static enum DecRet HevcSetInfo(void* inst, struct DecConfig *config, struct DecSequenceInfo* Info);
static enum DecRet HevcDecode(void* inst, struct DWLLinearMem *input, struct DecOutput* output,
                              u8* stream, u32 strm_len, struct DWL *dwl, u32 pic_id, void *p_user_data);
static enum DecRet HevcNextPicture(void* inst, struct DecPicturePpu* pic,
                                   struct DWL *dwl);
static enum DecRet HevcPictureConsumed(void* inst, struct DecPicturePpu *pic,
                                       struct DWL *dwl);
static enum DecRet HevcEndOfStream(void* inst);
static enum DecRet HevcGetBufferInfo(void *inst, struct DecBufferInfo *buf_info);
static enum DecRet HevcAddBuffer(void *inst, struct DWLLinearMem *buf);

static void HevcRelease(void* inst);
static void HevcStreamDecoded(void* inst);
static void HevcUpdateStrmInfoCtrl(void* inst, u32 last_flag, u32 strm_bus_addr);
static void HevcUpdateStrm(struct strmInfo info);
#endif /* ENABLE_HEVC_SUPPORT */

#ifdef ENABLE_H264_SUPPORT
/* H264 codec wrappers. */
static enum DecRet H264Init(const void** inst, struct DecConfig *config,
                            const void *dwl);
static enum DecRet H264GetInfo(void* inst, struct DecSequenceInfo* info);
static enum DecRet H264SetInfo(void* inst, struct DecConfig *config, struct DecSequenceInfo* Info);
static enum DecRet H264Decode(void* inst, struct DWLLinearMem *input, struct DecOutput* output,
                              u8* stream, u32 strm_len, struct DWL *dwl, u32 pic_id, void *p_user_data);
static enum DecRet H264NextPicture(void* inst, struct DecPicturePpu* pic,
                                   struct DWL *dwl);
static enum DecRet H264PictureConsumed(void* inst, struct DecPicturePpu *pic,
                                       struct DWL *dwl);
static enum DecRet H264EndOfStream(void* inst);
static enum DecRet H264GetBufferInfo(void *inst, struct DecBufferInfo *buf_info);
static enum DecRet H264AddBuffer(void *inst, struct DWLLinearMem *buf);
static void H264Release(void* inst);
static void H264StreamDecoded(void* inst);
static void H264UpdateStrmInfoCtrl(void* inst, u32 last_flag, u32 strm_bus_addr);
static void H264UpdateStrm(struct strmInfo info);
#endif /* ENABLE_H264_SUPPORT */

#ifdef ENABLE_VP9_SUPPORT
/* VP9 codec wrappers. */
static enum DecRet Vp9Init(const void** inst, struct DecConfig *config,
                           const void *dwl);
static enum DecRet Vp9GetInfo(void* inst, struct DecSequenceInfo* info);
static enum DecRet Vp9SetInfo(void* inst, struct DecConfig *config, struct DecSequenceInfo* Info);
static enum DecRet Vp9Decode(void* inst, struct DWLLinearMem *input, struct DecOutput* output,
                             u8* stream, u32 strm_len, struct DWL *dwl, u32 pic_id, void *p_user_data);
static enum DecRet Vp9NextPicture(void* inst, struct DecPicturePpu* pic,
                                  struct DWL *dwl);
static enum DecRet Vp9PictureConsumed(void* inst, struct DecPicturePpu *pic,
                                      struct DWL *dwl);
static enum DecRet Vp9EndOfStream(void* inst);
static enum DecRet Vp9GetBufferInfo(void *inst, struct DecBufferInfo *buf_info);
static enum DecRet Vp9AddBuffer(void *inst, struct DWLLinearMem *buf);
static void Vp9Release(void* inst);
static void Vp9StreamDecoded(void* inst);
#endif /* ENABLE_VP9_SUPPORT */

#ifdef ENABLE_AVS2_SUPPORT
/* VP9 codec wrappers. */
static enum DecRet DecAvs2Init(const void** inst, struct DecConfig *config,
                           const void *dwl);
static enum DecRet DecAvs2GetInfo(void* inst, struct DecSequenceInfo* info);
static enum DecRet DecAvs2SetInfo(void* inst, struct DecConfig *config, struct DecSequenceInfo* Info);
static enum DecRet DecAvs2Decode(void* inst, struct DWLLinearMem *input, struct DecOutput* output,
                             u8* stream, u32 strm_len, struct DWL *dwl, u32 pic_id, void *p_user_data);
static enum DecRet DecAvs2NextPicture(void* inst, struct DecPicturePpu* pic,
                                  struct DWL *dwl);
static enum DecRet DecAvs2PictureConsumed(void* inst, struct DecPicturePpu *pic,
                                      struct DWL *dwl);
static enum DecRet DecAvs2EndOfStream(void* inst);
static enum DecRet DecAvs2GetBufferInfo(void *inst, struct DecBufferInfo *buf_info);
static enum DecRet DecAvs2AddBuffer(void *inst, struct DWLLinearMem *buf);
static void DecAvs2Release(void* inst);
static void DecAvs2StreamDecoded(void* inst);
#endif /* ENABLE_VP9_SUPPORT */

struct DecSwHwBuild DecGetBuild(void) {
  struct DecSwHwBuild build;

  (void)DWLmemset(&build, 0, sizeof(build));

  build.sw_build = HANTRO_DEC_SW_BUILD;
  build.hw_build = DWLReadAsicID(DWL_CLIENT_TYPE_HEVC_DEC);

  DWLReadAsicConfig(build.hw_config, DWL_CLIENT_TYPE_HEVC_DEC);

  return build;
}

enum DecRet DecInit(enum DecCodec codec, DecInst* decoder,
                    struct DecConfig* config, struct DecClientHandle* client) {
  if (decoder == NULL || client->Initialized == NULL ||
      client->HeadersDecoded == NULL || client->BufferDecoded == NULL ||
      client->PictureReady == NULL || client->EndOfStream == NULL ||
      client->Released == NULL || client->NotifyError == NULL) {
    return DEC_PARAM_ERROR;
  }

  DecoderInstance* inst = config->dwl.calloc(1, sizeof(DecoderInstance));
  if (inst == NULL) return DEC_MEMFAIL;
  inst->dwl = config->dwl;
  inst->dwl_inst = config->dwl_inst;
  if (config->decoder_mode == DEC_LOW_LATENCY)
    low_latency = 1;
  if (FifoInit(MAX_FIFO_CAPACITY, &inst->input_queue) != FIFO_OK) {
    inst->dwl.free(inst);
    return DEC_MEMFAIL;
  }
  inst->client = *client;
  inst->dwl.pthread_mutex_init(&inst->cs_mutex, NULL);
  inst->dwl.pthread_mutex_init(&inst->resource_mutex, NULL);
  inst->dwl.pthread_cond_init(&inst->resource_cond, NULL);
  inst->dwl.pthread_mutex_init(&inst->eos_mutex, NULL);
  inst->dwl.pthread_cond_init(&inst->eos_cond, NULL);
  inst->eos_ready = 0;
  inst->resources_acquired = 0;
  inst->dwl.pthread_create(&inst->decode_thread, NULL, DecodeLoop, inst);
#ifdef ENABLE_HEVC_SUPPORT
  if(low_latency) {
    send_strm_info.strm_bus_addr = send_strm_info.strm_bus_start_addr = 0;
    send_strm_info.strm_vir_addr = send_strm_info.strm_vir_start_addr = NULL;
    send_strm_info.low_latency = 1;
    send_strm_info.send_len = 0;
    sem_init(&curr_frame_end_sem, 0, 1);
    sem_init(&next_frame_start_sem, 0, 0);
    pthread_mutex_init(&send_mutex, NULL);
    task = run_task(send_bytestrm_task, inst);
  }
#endif
  switch (codec) {
#ifdef ENABLE_HEVC_SUPPORT
  case DEC_HEVC:
    inst->dec.init = HevcInit;
    inst->dec.GetInfo = HevcGetInfo;
    inst->dec.SetInfo = HevcSetInfo;
    inst->dec.Decode = HevcDecode;
    inst->dec.NextPicture = HevcNextPicture;
    inst->dec.PictureConsumed = HevcPictureConsumed;
    inst->dec.EndOfStream = HevcEndOfStream;
    inst->dec.Release = HevcRelease;
    inst->dec.UseExtraFrmBuffers = HevcDecUseExtraFrmBuffers;
    inst->dec.GetBufferInfo = HevcGetBufferInfo;
    inst->dec.AddBuffer = HevcAddBuffer;
    inst->stream_decoded = HevcStreamDecoded;
    inst->dec.UpdateStrmInfoCtrl = HevcUpdateStrmInfoCtrl;
    inst->dec.UpdateStrm = HevcUpdateStrm;
    break;
#endif
#ifdef ENABLE_VP9_SUPPORT
  case DEC_VP9:
    inst->dec.init = Vp9Init;
    inst->dec.GetInfo = Vp9GetInfo;
    inst->dec.SetInfo = Vp9SetInfo;
    inst->dec.Decode = Vp9Decode;
    inst->dec.NextPicture = Vp9NextPicture;
    inst->dec.PictureConsumed = Vp9PictureConsumed;
    inst->dec.EndOfStream = Vp9EndOfStream;
    inst->dec.Release = Vp9Release;
    inst->dec.UseExtraFrmBuffers = Vp9DecUseExtraFrmBuffers;
    inst->stream_decoded = Vp9StreamDecoded;
    inst->dec.GetBufferInfo = Vp9GetBufferInfo;
    inst->dec.AddBuffer = Vp9AddBuffer;
    break;
#endif
#ifdef ENABLE_H264_SUPPORT
  case DEC_H264_H10P:
    inst->dec.init = H264Init;
    inst->dec.GetInfo = H264GetInfo;
    inst->dec.SetInfo = H264SetInfo;
    inst->dec.Decode = H264Decode;
    inst->dec.NextPicture = H264NextPicture;
    inst->dec.PictureConsumed = H264PictureConsumed;
    inst->dec.EndOfStream = H264EndOfStream;
    inst->dec.Release = H264Release;
    inst->dec.UseExtraFrmBuffers = NULL;
    inst->stream_decoded = H264StreamDecoded;
    inst->dec.GetBufferInfo = H264GetBufferInfo;
    inst->dec.AddBuffer = H264AddBuffer;
    inst->dec.UpdateStrmInfoCtrl = H264UpdateStrmInfoCtrl;
    inst->dec.UpdateStrm = H264UpdateStrm;
    break;
#endif
#ifdef ENABLE_AVS2_SUPPORT
  case DEC_AVS2:
    inst->dec.init = DecAvs2Init;
    inst->dec.GetInfo = DecAvs2GetInfo;
    inst->dec.SetInfo = DecAvs2SetInfo;
    inst->dec.Decode = DecAvs2Decode;
    inst->dec.NextPicture = DecAvs2NextPicture;
    inst->dec.PictureConsumed = DecAvs2PictureConsumed;
    inst->dec.EndOfStream = DecAvs2EndOfStream;
    inst->dec.Release = DecAvs2Release;
    inst->dec.UseExtraFrmBuffers = Avs2DecUseExtraFrmBuffers;
    inst->stream_decoded = DecAvs2StreamDecoded;
    inst->dec.GetBufferInfo = DecAvs2GetBufferInfo;
    inst->dec.AddBuffer = DecAvs2AddBuffer;
    inst->dec.UpdateStrmInfoCtrl = NULL;
    inst->dec.UpdateStrm = NULL;
    break;
#endif

  default:
    return DEC_FORMAT_NOT_SUPPORTED;
  }
  SetState(inst, DECODER_WAITING_HEADERS);
  *decoder = inst;
  inst->config = *config;
  struct Command* command = inst->dwl.calloc(1, sizeof(struct Command));
  command->id = COMMAND_INIT;
  command->params.config = *config;
  FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
  return DEC_OK;
}

#ifdef DECAPI_VER2
enum DecRet DecDestroy(DecInst dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* tmp_cmd;

  /* destroy is allowed only in TERMINATED state. */
  if (GetState(inst) != DECODER_TERMINATED)
    return DEC_PARAM_ERROR;

  /* Abort the current command (it may be long-lasting task). */
  
  /* Before FifoRelease, release the commands queued in fifo firstly. */
  do {
    tmp_cmd = NULL;
    FifoPop(inst->input_queue, (void**)&tmp_cmd,
          FIFO_EXCEPTION_ENABLE);
    if(tmp_cmd)
      inst->dwl.free(tmp_cmd);
  }while(tmp_cmd);

  FifoRelease(inst->input_queue);
  inst->dwl.free(inst);
  return DEC_OK;
}

enum DecRet DecDecode(DecInst dec_inst, struct DecInput* input) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  if (dec_inst == NULL || input == NULL || input->data_len == 0 ||
      input->buffer.virtual_address == NULL || input->buffer.bus_address == 0) {
    return DEC_PARAM_ERROR;
  }

  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN: {
    struct Command* command = inst->dwl.calloc(1, sizeof(struct Command));
    command->id = COMMAND_DECODE;
    inst->dwl.memcpy(&command->params.config, &(inst->config), sizeof(struct DecConfig));
    inst->dwl.memcpy(&command->params.input, input, sizeof(struct DecInput));
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  }
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    /* In this state, no more decoding but will still output pictures. */
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_NOT_INITIALIZED;
  }
}

enum DecRet DecGetPictureBuffersInfo(DecInst dec_inst, struct DecBufferInfo *info) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if (dec_inst == NULL || info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }
  return (inst->dec.GetBufferInfo(inst->dec.inst, info));
}

enum DecRet DecSetPictureBuffers(DecInst dec_inst,
                                 const struct DWLLinearMem* buffers,
                                 u32 num_of_buffers) {
  enum DecRet ret = DEC_OK;
  /* TODO(vmr): Enable checks once we have implementation in place. */
  /* if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0)
  {
      return DEC_PARAM_ERROR;
  } */
  if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }

  /* TODO(vmr): Check the buffers and set them, if they're good. */
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  for (int i = 0; i < num_of_buffers; i++) {
    ret = inst->dec.AddBuffer(inst->dec.inst, (struct DWLLinearMem *)&buffers[i]);

    /* TODO(min): Check return code ... */
  }
  inst->resources_acquired = 1;
  SetState(inst, DECODER_DECODING);
  inst->dwl.pthread_cond_signal(&inst->resource_cond);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
  return ret; //DEC_OK;
}

enum DecRet DecUseExtraFrmBuffers(DecInst dec_inst, u32 n) {
  enum DecRet ret = DEC_OK;

  if (dec_inst == NULL)
    return DEC_PARAM_ERROR;

  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }

  inst->dec.UseExtraFrmBuffers(inst->dec.inst, n);
  return ret;
}

enum DecRet DecPictureConsumed(DecInst dec_inst, struct DecPicturePpu* picture) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    inst->dec.PictureConsumed(inst->dec.inst, picture, &inst->dwl);
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_NOT_INITIALIZED;
  }
  return DEC_OK;
}

enum DecRet DecEndOfStream(DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* command;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    command = inst->dwl.calloc(1, sizeof(struct Command));
    inst->dwl.memset(command, 0, sizeof(struct Command));
    command->id = COMMAND_END_OF_STREAM;
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_INITFAIL;
  }
}

void DecRelease(DecInst dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  /* If we are already terminated, do nothing. */
  if (GetState(inst) == DECODER_TERMINATED) return;
  /* If we are already shutting down, no need to do it more than once. */
  if (GetState(inst) == DECODER_SHUTTING_DOWN) return;
  /* Abort the current command (it may be long-lasting task). */
  if (GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN)
    SetState(inst, DECODER_SHUTTING_DOWN);
  struct Command* command = inst->dwl.calloc(1, sizeof(struct Command));
  inst->dwl.memset(command, 0, sizeof(struct Command));
  command->id = COMMAND_RELEASE;
  FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
  PERFORMANCE_STATIC_REPORT(decode_total)
  PERFORMANCE_STATIC_REPORT(decode_pre_hw);
  PERFORMANCE_STATIC_REPORT(decode_post_hw);
}

#else /* DECAPI_VER2 */

enum DecRet DecDecode(DecInst dec_inst, struct DecInput* input) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  if (dec_inst == NULL || input == NULL || input->data_len == 0 ||
      input->buffer.virtual_address == NULL || input->buffer.bus_address == 0) {
    return DEC_PARAM_ERROR;
  }

  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN: {
    struct Command* command = inst->dwl.calloc(1, sizeof(struct Command));
    command->id = COMMAND_DECODE;
    inst->dwl.memcpy(&command->params.config, &(inst->config), sizeof(struct DecConfig));
    inst->dwl.memcpy(&command->params.input, input, sizeof(struct DecInput));
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  }
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    /* In this state, no more decoding but will still output pictures. */
    return DEC_OK;
  default:
    return DEC_NOT_INITIALIZED;
  }
}

enum DecRet DecGetPictureBuffersInfo(DecInst dec_inst, struct DecBufferInfo *info) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if (dec_inst == NULL || info == NULL) {
    return DEC_PARAM_ERROR;
  }

  return (inst->dec.GetBufferInfo(inst->dec.inst, info));
}

enum DecRet DecSetPictureBuffers(DecInst dec_inst,
                                 const struct DWLLinearMem* buffers,
                                 u32 num_of_buffers) {
  enum DecRet ret = DEC_OK;
  /* TODO(vmr): Enable checks once we have implementation in place. */
  /* if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0)
  {
      return DEC_PARAM_ERROR;
  } */
  if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  /* TODO(vmr): Check the buffers and set them, if they're good. */
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  for (int i = 0; i < num_of_buffers; i++) {
    ret = inst->dec.AddBuffer(inst->dec.inst, (struct DWLLinearMem *)&buffers[i]);

    /* TODO(min): Check return code ... */
  }
  inst->resources_acquired = 1;
  SetState(inst, DECODER_DECODING);
  inst->dwl.pthread_cond_signal(&inst->resource_cond);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
  return ret; //DEC_OK;
}

enum DecRet DecUseExtraFrmBuffers(DecInst dec_inst, u32 n) {
  enum DecRet ret = DEC_OK;

  if (dec_inst == NULL)
    return DEC_PARAM_ERROR;

  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  inst->dec.UseExtraFrmBuffers(inst->dec.inst, n);
  return ret;
}

enum DecRet DecPictureConsumed(DecInst dec_inst, struct DecPicturePpu* picture) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    inst->dec.PictureConsumed(inst->dec.inst, picture, &inst->dwl);
    return DEC_OK;
  default:
    return DEC_NOT_INITIALIZED;
  }
  return DEC_OK;
}

enum DecRet DecEndOfStream(DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* command;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    command = inst->dwl.calloc(1, sizeof(struct Command));
    inst->dwl.memset(command, 0, sizeof(struct Command));
    command->id = COMMAND_END_OF_STREAM;
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  default:
    return DEC_INITFAIL;
  }
}

void DecRelease(DecInst dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  /* If we are already shutting down, no need to do it more than once. */
  if (GetState(inst) == DECODER_SHUTTING_DOWN) return;
  /* Abort the current command (it may be long-lasting task). */
  if (GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN)
    SetState(inst, DECODER_SHUTTING_DOWN);
  struct Command* command = inst->dwl.calloc(1, sizeof(struct Command));
  inst->dwl.memset(command, 0, sizeof(struct Command));
  command->id = COMMAND_RELEASE;
  FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
  PERFORMANCE_STATIC_REPORT(decode_total)
  PERFORMANCE_STATIC_REPORT(decode_pre_hw);
  PERFORMANCE_STATIC_REPORT(decode_post_hw);
}
#endif /* DECAPI_VER2 */

static void NextCommand(DecoderInstance* inst) {
  /* Read next command from stream, if we've finished the previous. */
  if (inst->current_command == NULL) {
    FifoPop(inst->input_queue, (void**)&inst->current_command,
            FIFO_EXCEPTION_DISABLE);
    if (inst->current_command != NULL &&
        inst->current_command->id == COMMAND_DECODE) {
      if(low_latency && !decoder_release) {
        sem_wait(&curr_frame_end_sem);
      }
      inst->buffer_status.strm_curr_pos =
        (u8*)inst->current_command->params.input.stream[0];
      inst->buffer_status.strm_curr_bus_address =
        inst->current_command->params.input.buffer.bus_address;
      inst->buffer_status.data_left =
        inst->current_command->params.input.data_len;
      inst->buffer_status.strm_buff =
        (u8*)inst->current_command->params.input.buffer.virtual_address;
      inst->buffer_status.strm_buff_bus_address =
        inst->current_command->params.input.buffer.bus_address;
      inst->buffer_status.buff_size =
        inst->current_command->params.input.buffer.size;
      if(low_latency) {
        pthread_mutex_lock(&send_mutex);
        send_strm_info.strm_vir_addr = send_strm_info.strm_vir_start_addr = inst->buffer_status.strm_curr_pos;
        send_strm_info.strm_bus_addr = send_strm_info.strm_bus_start_addr = inst->buffer_status.strm_curr_bus_address +
                                       ((addr_t)inst->buffer_status.strm_curr_pos - (addr_t)inst->buffer_status.strm_buff);
        frame_len = inst->buffer_status.data_left;
        send_len = 0;
        send_strm_info.send_len = 0;
        if (inst->max_num_of_decoded_pics > 0 &&
          inst->num_of_decoded_pics >= inst->max_num_of_decoded_pics) {
          process_end_flag = 1;
          pic_decoded = 1;
          decoder_release = 1;
        }
        sem_post(&next_frame_start_sem);
        pthread_mutex_unlock(&send_mutex);
      }
    }
  }
}

static void CommandCompleted(DecoderInstance* inst) {
  if (inst->current_command->id == COMMAND_DECODE &&
      GetState(inst) != DECODER_SHUTTING_DOWN &&
      GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN &&
      !inst->current_command->params.config.mc_cfg.mc_enable) {
    /* When xxxSetInfo() returns error, decoder transits to SHUTTING_DOWN from
       WAITING_HEADERS directly, but there are still some input buffers to be
       consumed. For these buffers, stream_decoded() won't be called. Othewise
       more buffers will be fed in. */
    inst->stream_decoded(inst);
  }
  inst->dwl.free(inst->current_command);
  inst->current_command = NULL;
}

#ifdef DECAPI_VER2
static void* DecodeLoop(void* arg) {
  DecoderInstance* inst = (DecoderInstance*)arg;
  while (1) {
    NextCommand(inst);
    if (inst->current_command == NULL) /* NULL command means to quit. */
      return NULL;

    switch (inst->current_command->id) {
      case COMMAND_INIT:
        Initialize(inst);
        break;
      case COMMAND_DECODE:
        Decode(inst);
        break;
      case COMMAND_END_OF_STREAM:
        EndOfStream(inst);
        break;
      case COMMAND_RELEASE:
        Release(inst);
        return NULL;
      case COMMAND_SETBUFFERS:
      default:
        CommandCompleted(inst);
        break;
    }
  }
  return NULL;
}

static void Initialize(DecoderInstance* inst) {
  enum DecRet rv;

  switch (GetState(inst)) {
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
      rv = inst->dec.init((const void**)&inst->dec.inst,
                       &inst->current_command->params.config, inst->dwl_inst);
      if (rv == DEC_OK)
        inst->client.Initialized(inst->client.client);
      else
        inst->client.NotifyError(inst->client.client, 0, rv);
      inst->max_num_of_decoded_pics =
        inst->current_command->params.config.max_num_pics_to_decode;
      inst->dwl.pthread_create(&inst->output_thread, NULL, OutputLoop, inst);
      break;
    default:
      /* do nothing */
      break;
  }
  CommandCompleted(inst);
}

static void WaitForResources(DecoderInstance* inst) {
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  while (!inst->resources_acquired)
    inst->dwl.pthread_cond_wait(&inst->resource_cond, &inst->resource_mutex);
  SetState(inst, DECODER_DECODING);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
}

static void Decode(DecoderInstance* inst) {
  enum DecRet rv = DEC_OK;
  struct DWLLinearMem buffer;
  u8* stream;
  u32 strm_len;

  switch (GetState(inst)) {
    case DECODER_WAITING_RESOURCES:
      /* when WAITING_RESOURCES, wait for resources firstly, then do "real" decoding */
      WaitForResources(inst);
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
      do {
        /* Skip decoding if we've decoded as many pics requested by the user. */
        if (inst->max_num_of_decoded_pics > 0 &&
            inst->num_of_decoded_pics >= inst->max_num_of_decoded_pics) {
          sem_post(&curr_frame_end_sem);
          EndOfStream(inst);
          break;
        }
      
        if(low_latency) {
          while (!send_strm_info.send_len)
            sched_yield();
          buffer.virtual_address = (u32*)inst->buffer_status.strm_buff;
          buffer.bus_address = inst->buffer_status.strm_buff_bus_address;
          buffer.size = inst->buffer_status.buff_size;
          stream = inst->buffer_status.strm_curr_pos;
          pthread_mutex_lock(&send_mutex);
          strm_len = send_strm_info.send_len;
          pthread_mutex_unlock(&send_mutex);
        } else {
          buffer.virtual_address = (u32*)inst->buffer_status.strm_buff;
          buffer.bus_address = inst->buffer_status.strm_buff_bus_address;
          buffer.size = inst->buffer_status.buff_size;
          stream = inst->buffer_status.strm_curr_pos;
          strm_len = inst->buffer_status.data_left;
        }
      
        PERFORMANCE_STATIC_START(decode_total);
        PERFORMANCE_STATIC_START(decode_pre_hw);
        rv = inst->dec.Decode(inst->dec.inst, &buffer, &inst->buffer_status, stream,
                              strm_len, &inst->dwl, inst->num_of_decoded_pics + 1,
                              (void *)inst->client.client);
        PERFORMANCE_STATIC_END(decode_pre_hw);
        PERFORMANCE_STATIC_END(decode_post_hw);
        PERFORMANCE_STATIC_END(decode_total);
        if (GetState(inst) == DECODER_SHUTTING_DOWN) {
          break;
        }
      
        if (rv == DEC_HDRS_RDY) {
          enum DecRet rv_info;
          inst->dec.GetInfo(inst->dec.inst, &inst->sequence_info);
          if (inst->sequence_info.h264_base_mode == 1) {
            inst->current_command->params.config.use_ringbuffer = 0;
          }
          /* Ajust user cropping params based on cropping params from sequence info. */
          if (inst->sequence_info.crop_params.crop_left_offset != 0 ||
              inst->sequence_info.crop_params.crop_top_offset != 0 ||
              (inst->sequence_info.crop_params.crop_out_width != inst->sequence_info.pic_width &&
               inst->sequence_info.crop_params.crop_out_width != 0) ||
              (inst->sequence_info.crop_params.crop_out_height != inst->sequence_info.pic_height &&
               inst->sequence_info.crop_params.crop_out_height != 0)) {
            int i;
            struct DecConfig *config = &inst->current_command->params.config;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
              if (!config->ppu_cfg[i].enabled) continue;
      
              if (!config->ppu_cfg[i].crop.enabled) {
                config->ppu_cfg[i].crop.x = inst->sequence_info.crop_params.crop_left_offset;
                config->ppu_cfg[i].crop.y = inst->sequence_info.crop_params.crop_top_offset;
                config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
                config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
              } else {
                config->ppu_cfg[i].crop.x += inst->sequence_info.crop_params.crop_left_offset;
                config->ppu_cfg[i].crop.y += inst->sequence_info.crop_params.crop_top_offset;
                if(!config->ppu_cfg[i].crop.width)
                  config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
                if(!config->ppu_cfg[i].crop.height)
                  config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
              }
              config->ppu_cfg[i].enabled = 1;
              config->ppu_cfg[i].crop.enabled = 1;
            }
          }
      
          rv_info = inst->dec.SetInfo(inst->dec.inst, &inst->current_command->params.config, &inst->sequence_info);
          if (rv_info == DEC_OK) {
            inst->client.HeadersDecoded(inst->client.client, inst->sequence_info);
          } else {
            if (rv_info == DEC_INFOPARAM_ERROR) {
              SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
            }
            DecRelease(inst);
            inst->client.NotifyError(inst->client.client, 0, rv_info);
            break;
          }
          if (inst->buffer_status.data_left == 0) break;
        }
        else if (rv == DEC_WAITING_FOR_BUFFER) { /* Allocate buffers externally. */
          while (inst->picture_in_display)
          usleep(10);
          inst->dec.GetInfo(inst->dec.inst, &inst->sequence_info);
          SetState(inst, DECODER_WAITING_RESOURCES);
          if (inst->client.ExtBufferReq(inst->client.client)) {
            inst->client.NotifyError(inst->client.client, 0, DEC_MEMFAIL);
            inst->buffer_status.data_left = 0;
            pic_decoded = 1;
          }
          //if (inst->buffer_status.data_left == 0) break;
        }
        else if (rv == DEC_NO_DECODING_BUFFER) {
          /* NO DECODING BUFFER */
          usleep(10);
          continue;
        /* ASO/FMO detected and not supported in multicore mode */
        } else if (rv == DEC_ADVANCED_TOOLS && inst->config.mc_cfg.mc_enable == 1) {
          DecRelease(inst);
          break;
        } else if (rv < 0) { /* Error */
          if (rv == DEC_INFOPARAM_ERROR || rv == DEC_STREAM_NOT_SUPPORTED) {
            SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
            DecRelease(inst);
          }
          inst->client.NotifyError(inst->client.client, 0, rv);
          break; /* Give up on the input buffer. */
        }
        if (rv == DEC_PIC_DECODED) inst->num_of_decoded_pics++;
      } while (inst->buffer_status.data_left > 0 &&
               GetState(inst) != DECODER_SHUTTING_DOWN);
      break;

    case DECODER_SHUTTING_DOWN:
    case DECODER_ERROR_TO_SHUTTING_DOWN:
    default:
      /* do nothing */
      break;
  }
  CommandCompleted(inst);
}

static void EndOfStream(DecoderInstance* inst) {
  switch (GetState(inst)) {
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
    case DECODER_WAITING_RESOURCES:
    case DECODER_ERROR_TO_SHUTTING_DOWN:
      inst->dec.EndOfStream(inst->dec.inst);
      
      inst->dwl.pthread_mutex_lock(&inst->eos_mutex);
      while (!inst->eos_ready)
        inst->dwl.pthread_cond_wait(&inst->eos_cond, &inst->eos_mutex);
      inst->dwl.pthread_mutex_unlock(&inst->eos_mutex);
      inst->pending_eos = 1;
      if (inst->current_command->id == COMMAND_END_OF_STREAM)
        CommandCompleted(inst);
      inst->client.EndOfStream(inst->client.client);
      break;

    case DECODER_SHUTTING_DOWN:
    default:
      if (inst->current_command->id == COMMAND_END_OF_STREAM)
        CommandCompleted(inst);
      break;
  }
}

static void Release(DecoderInstance* inst) {
  struct DecClientHandle client = inst->client;

  if (!inst->pending_eos) {
    inst->dec.EndOfStream(inst->dec.inst); /* In case user didn't. */
  }
#ifdef ENABLE_HEVC_SUPPORT
  if(low_latency) {
    pic_decoded = 1;
    process_end_flag = 1;
    sem_post(&next_frame_start_sem);
    wait_for_task_completion(task);
  }
#endif
  /* Release the resource condition, if decoder is waiting for resources. */
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  inst->dwl.pthread_cond_signal(&inst->resource_cond);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
  inst->dwl.pthread_join(inst->output_thread, NULL);
  inst->dwl.pthread_cond_destroy(&inst->resource_cond);
  inst->dwl.pthread_mutex_destroy(&inst->resource_mutex);
  SetState(inst, DECODER_TERMINATED);
  inst->dwl.pthread_mutex_destroy(&inst->cs_mutex);
  inst->dec.Release(inst->dec.inst);

  CommandCompleted(inst);
  client.Released(client.client);
}


#else /* DECAPI_VER2 */
static void* DecodeLoop(void* arg) {
  DecoderInstance* inst = (DecoderInstance*)arg;
  while (1) {
    NextCommand(inst);
    if (inst->current_command == NULL) /* NULL command means to quit. */
      return NULL;
    switch (GetState(inst)) {
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
      if (inst->current_command->id == COMMAND_INIT) {
        Initialize(inst);
      } else if (inst->current_command->id == COMMAND_DECODE) {
        Decode(inst);
      } else if (inst->current_command->id == COMMAND_END_OF_STREAM) {
        EndOfStream(inst);
      } else if (inst->current_command->id == COMMAND_RELEASE) {
        Release(inst);
        return NULL;
      }
      break;
    case DECODER_SHUTTING_DOWN:
      if (inst->current_command->id == COMMAND_RELEASE) {
        Release(inst);
        return NULL;
      } else {
        CommandCompleted(inst);
      }
      break;
    case DECODER_WAITING_RESOURCES:
      WaitForResources(inst);
      break;
    case DECODER_ERROR_TO_SHUTTING_DOWN:
      if (inst->current_command->id == COMMAND_END_OF_STREAM) {
        EndOfStream(inst);
      } else if (inst->current_command->id == COMMAND_RELEASE) {
        Release(inst);
        return NULL;
      } else {
        /* No more decoding in this state. */
        CommandCompleted(inst);
      }
    default:
      break;
    }
  }
  return NULL;
}

static void Initialize(DecoderInstance* inst) {
  enum DecRet rv =
    inst->dec.init((const void**)&inst->dec.inst,
                   &inst->current_command->params.config, inst->dwl_inst);
  if (rv == DEC_OK)
    inst->client.Initialized(inst->client.client);
  else
    inst->client.NotifyError(inst->client.client, 0, rv);
  inst->max_num_of_decoded_pics =
    inst->current_command->params.config.max_num_pics_to_decode;
  inst->dwl.pthread_create(&inst->output_thread, NULL, OutputLoop, inst);
  CommandCompleted(inst);
}

static void WaitForResources(DecoderInstance* inst) {
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  while (!inst->resources_acquired)
    inst->dwl.pthread_cond_wait(&inst->resource_cond, &inst->resource_mutex);
  SetState(inst, DECODER_DECODING);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
}

static void Decode(DecoderInstance* inst) {
  enum DecRet rv = DEC_OK;
  struct DWLLinearMem buffer;
  u8* stream;
  u32 strm_len;
  do {
    /* Skip decoding if we've decoded as many pics requested by the user. */
    if (inst->max_num_of_decoded_pics > 0 &&
        inst->num_of_decoded_pics >= inst->max_num_of_decoded_pics) {
      sem_post(&curr_frame_end_sem);
      EndOfStream(inst);
      break;
    }

    if(low_latency) {
      while (!send_strm_info.send_len)
        sched_yield();
      buffer.virtual_address = (u32*)inst->buffer_status.strm_buff;
      buffer.bus_address = inst->buffer_status.strm_buff_bus_address;
      buffer.size = inst->buffer_status.buff_size;
      stream = inst->buffer_status.strm_curr_pos;
      pthread_mutex_lock(&send_mutex);
      strm_len = send_strm_info.send_len;
      pthread_mutex_unlock(&send_mutex);
    } else {
      buffer.virtual_address = (u32*)inst->buffer_status.strm_buff;
      buffer.bus_address = inst->buffer_status.strm_buff_bus_address;
      buffer.size = inst->buffer_status.buff_size;
      stream = inst->buffer_status.strm_curr_pos;
      strm_len = inst->buffer_status.data_left;
    }

    PERFORMANCE_STATIC_START(decode_total);
    PERFORMANCE_STATIC_START(decode_pre_hw);
    rv = inst->dec.Decode(inst->dec.inst, &buffer, &inst->buffer_status, stream,
                          strm_len, &inst->dwl, inst->num_of_decoded_pics + 1,
                          (void *)inst->client.client);
    PERFORMANCE_STATIC_END(decode_pre_hw);
    PERFORMANCE_STATIC_END(decode_post_hw);
    PERFORMANCE_STATIC_END(decode_total);
    if (GetState(inst) == DECODER_SHUTTING_DOWN) {
      break;
    }

    if (rv == DEC_HDRS_RDY) {
      enum DecRet rv_info;
      inst->dec.GetInfo(inst->dec.inst, &inst->sequence_info);
      if (inst->sequence_info.h264_base_mode == 1) {
        inst->current_command->params.config.use_ringbuffer = 0;
      }
      /* Ajust user cropping params based on cropping params from sequence info. */
      if (inst->sequence_info.crop_params.crop_left_offset != 0 ||
          inst->sequence_info.crop_params.crop_top_offset != 0 ||
          inst->sequence_info.crop_params.crop_out_width != inst->sequence_info.pic_width ||
          inst->sequence_info.crop_params.crop_out_height != inst->sequence_info.pic_height) {
        int i;
        struct DecConfig *config = &inst->current_command->params.config;
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if (!config->ppu_cfg[i].enabled) continue;

          if (!config->ppu_cfg[i].crop.enabled) {
            config->ppu_cfg[i].crop.x = inst->sequence_info.crop_params.crop_left_offset;
            config->ppu_cfg[i].crop.y = inst->sequence_info.crop_params.crop_top_offset;
            config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
            config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
          } else {
            config->ppu_cfg[i].crop.x += inst->sequence_info.crop_params.crop_left_offset;
            config->ppu_cfg[i].crop.y += inst->sequence_info.crop_params.crop_top_offset;
            if(!config->ppu_cfg[i].crop.width)
              config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
            if(!config->ppu_cfg[i].crop.height)
              config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
          }
          config->ppu_cfg[i].enabled = 1;
          config->ppu_cfg[i].crop.enabled = 1;
        }
      }

      rv_info = inst->dec.SetInfo(inst->dec.inst, &inst->current_command->params.config, &inst->sequence_info);
      if (rv_info == DEC_OK) {
        inst->client.HeadersDecoded(inst->client.client, inst->sequence_info);
      } else {
        if (rv_info == DEC_INFOPARAM_ERROR) {
          SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
        }
        DecRelease(inst);
        inst->client.NotifyError(inst->client.client, 0, rv_info);
        break;
      }
      if (inst->buffer_status.data_left == 0) break;
    }
    else if (rv == DEC_WAITING_FOR_BUFFER) { /* Allocate buffers externally. */
      while (inst->picture_in_display)
	  usleep(10);
      inst->dec.GetInfo(inst->dec.inst, &inst->sequence_info);
      SetState(inst, DECODER_WAITING_RESOURCES);
      if (inst->client.ExtBufferReq(inst->client.client)) {
        inst->client.NotifyError(inst->client.client, 0, DEC_MEMFAIL);
        inst->buffer_status.data_left = 0;
        pic_decoded = 1;
      }
      //if (inst->buffer_status.data_left == 0) break;
    }
    else if (rv == DEC_NO_DECODING_BUFFER) {
      /* NO DECODING BUFFER */
	  usleep(10);
      continue;
    /* ASO/FMO detected and not supported in multicore mode */
    } else if (rv == DEC_ADVANCED_TOOLS && inst->config.mc_cfg.mc_enable == 1) {
      DecRelease(inst);
      break;
    } else if (rv < 0) { /* Error */
      if (rv == DEC_INFOPARAM_ERROR || rv == DEC_STREAM_NOT_SUPPORTED) {
        SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
        DecRelease(inst);
      }
      inst->client.NotifyError(inst->client.client, 0, rv);
      break; /* Give up on the input buffer. */
    }
    if (rv == DEC_PIC_DECODED) inst->num_of_decoded_pics++;
  } while (inst->buffer_status.data_left > 0 &&
           GetState(inst) != DECODER_SHUTTING_DOWN);
  CommandCompleted(inst);
}

static void EndOfStream(DecoderInstance* inst) {
  inst->dec.EndOfStream(inst->dec.inst);

  inst->dwl.pthread_mutex_lock(&inst->eos_mutex);
  while (!inst->eos_ready)
    inst->dwl.pthread_cond_wait(&inst->eos_cond, &inst->eos_mutex);
  inst->dwl.pthread_mutex_unlock(&inst->eos_mutex);
  inst->pending_eos = 1;
  if (inst->current_command->id == COMMAND_END_OF_STREAM)
    CommandCompleted(inst);
  inst->client.EndOfStream(inst->client.client);
}

static void Release(DecoderInstance* inst) {
  struct Command* tmp_cmd;
  if (!inst->pending_eos) {
    inst->dec.EndOfStream(inst->dec.inst); /* In case user didn't. */
  }
#ifdef ENABLE_HEVC_SUPPORT
  if(low_latency) {
    pic_decoded = 1;
    process_end_flag = 1;
    sem_post(&next_frame_start_sem);
    wait_for_task_completion(task);
  }
#endif
  /* Release the resource condition, if decoder is waiting for resources. */
  inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
  inst->dwl.pthread_cond_signal(&inst->resource_cond);
  inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
  inst->dwl.pthread_join(inst->output_thread, NULL);
  inst->dwl.pthread_cond_destroy(&inst->resource_cond);
  inst->dwl.pthread_mutex_destroy(&inst->resource_mutex);
  inst->dwl.pthread_mutex_destroy(&inst->cs_mutex);
  inst->dec.Release(inst->dec.inst);

  /* Before FifoRelease, release the commands queued in fifo firstly. */
  do {
    tmp_cmd = NULL;
    FifoPop(inst->input_queue, (void**)&tmp_cmd,
          FIFO_EXCEPTION_ENABLE);
    if(tmp_cmd)
      inst->dwl.free(tmp_cmd);
  }while(tmp_cmd);

  FifoRelease(inst->input_queue);
  struct DecClientHandle client = inst->client;
  CommandCompleted(inst);
  inst->dwl.free(inst);
  client.Released(client.client);
}


#endif /* DECAPI_VER2 */

static void* OutputLoop(void* arg) {
  DecoderInstance* inst = (DecoderInstance*)arg;
  struct DecPicturePpu pic;
  enum DecRet rv;
  inst->dwl.memset(&pic, 0, sizeof(pic));
  while (1) {
    switch (GetState(inst)) {
    case DECODER_WAITING_RESOURCES:
      inst->dwl.pthread_mutex_lock(&inst->resource_mutex);
      while (!inst->resources_acquired &&
             GetState(inst) != DECODER_SHUTTING_DOWN)
        inst->dwl.pthread_cond_wait(&inst->resource_cond,
                                    &inst->resource_mutex);
      inst->dwl.pthread_mutex_unlock(&inst->resource_mutex);
      break;
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
    /* flush pending output buffer in DECODER_ERROR_TO_SHUTTING_DOWN state */
    case DECODER_ERROR_TO_SHUTTING_DOWN:
      while ((rv = inst->dec.NextPicture(inst->dec.inst, &pic, &inst->dwl)) ==
             DEC_PIC_RDY) {
        inst->picture_in_display = 1;
        inst->client.PictureReady(inst->client.client, pic);
        inst->picture_in_display = 0;
      }
      if (rv == DEC_END_OF_STREAM) {
        inst->dwl.pthread_mutex_lock(&inst->eos_mutex);
        inst->eos_ready = 1;
        inst->pending_eos = 0;
        inst->dwl.pthread_cond_signal(&inst->eos_cond);
        inst->dwl.pthread_mutex_unlock(&inst->eos_mutex);

        inst->dwl.pthread_exit(0);
        return NULL;
      }
      break;
    case DECODER_SHUTTING_DOWN:
      return NULL;
    default:
      break;
    }
	usleep(10);
  }
  return NULL;
}

static enum DecodingState GetState(DecoderInstance* inst) {
  inst->dwl.pthread_mutex_lock(&inst->cs_mutex);
  enum DecodingState state = inst->state;
  inst->dwl.pthread_mutex_unlock(&inst->cs_mutex);
  return state;
}

static void SetState(DecoderInstance* inst, enum DecodingState state) {
  const char* states[] = {
    "DECODER_WAITING_HEADERS", "DECODER_WAITING_RESOURCES",
    "DECODER_DECODING",        "DECODER_SHUTTING_DOWN",
    "DECODER_ERROR_TO_SHUTTING_DOWN"
#ifdef DECAPI_VER2
    , "DECODER_TERMINATED"
#endif
  };
  inst->dwl.pthread_mutex_lock(&inst->cs_mutex);
  inst->dwl.printf("Decoder state change: %s => %s\n", states[inst->state],
                   states[state]);
  inst->state = state;
  inst->dwl.pthread_mutex_unlock(&inst->cs_mutex);
}

#ifdef ENABLE_HEVC_SUPPORT
task_handle run_task(task_func func, void* param) {
  int ret;
  pthread_attr_t attr;
  struct sched_param par;
  pthread_t* thread_handle = malloc(sizeof(pthread_t));
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  ASSERT(ret == 0);
  ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
  par.sched_priority = 60;
  ret = pthread_attr_setschedparam(&attr, &par);
  //ret = pthread_create(thread_handle, &attr, func, param);
  ret = pthread_create(thread_handle, NULL, func, param);
  ASSERT(ret == 0);

  if(ret != 0) {
    free(thread_handle);
    thread_handle = NULL;
  }
  (void)attr;
  (void)par;
  return thread_handle;
}

void wait_for_task_completion(task_handle task) {
  int ret;
  ret = pthread_join(*((pthread_t*)task), NULL);
  ASSERT(ret == 0);
  (void) ret;
  free(task);
}

void SendBytesToDDR(u8* p, u32 n, u8* fp) {
  memcpy(p, fp, n);
}

void* send_bytestrm_task(void* param) {
  DecoderInstance* inst = (DecoderInstance*)param;

  u32 packet_size = LOW_LATENCY_PACKET_SIZE;
  u32 bytes = 0;
  u8 *fp = NULL;
  while (!inst->dec.inst)
    sched_yield();
  sem_wait(&next_frame_start_sem);

  while (!process_end_flag) {
   fp = inst->buffer_status.strm_curr_pos;
    while (frame_len > send_len || pic_decoded == 0) {
      if (frame_len - send_len > packet_size) {
        bytes = packet_size;
        if (fp + bytes > inst->buffer_status.strm_buff + inst->buffer_status.buff_size) {
          SendBytesToDDR(send_strm_info.strm_vir_addr, inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp, fp);
          bytes = packet_size - (inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp);
          fp = inst->buffer_status.strm_buff;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_vir_addr = inst->buffer_status.strm_buff;
          send_strm_info.strm_bus_addr = inst->buffer_status.strm_buff_bus_address;
          pthread_mutex_unlock(&send_mutex);
          SendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += packet_size;
          send_strm_info.last_flag = 0;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += packet_size;
          /* update the global structure in control sw layer */
          inst->dec.UpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        } else {
          SendBytesToDDR(send_strm_info.strm_vir_addr, packet_size, fp);
          fp += packet_size;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += packet_size;
          send_len += packet_size;
          send_strm_info.last_flag = 0;
          send_strm_info.strm_vir_addr += packet_size;
          send_strm_info.send_len += packet_size;
          /* update the global structure in control sw layer */
          inst->dec.UpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        }
      } else if(frame_len - send_len > 0) {
        bytes = frame_len - send_len;
        if (fp + bytes > inst->buffer_status.strm_buff + inst->buffer_status.buff_size) {
          SendBytesToDDR(send_strm_info.strm_vir_addr, inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp, fp);
          bytes = bytes - (inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp);
          fp = inst->buffer_status.strm_buff;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_vir_addr = inst->buffer_status.strm_buff;
          send_strm_info.strm_bus_addr = inst->buffer_status.strm_buff_bus_address;
          pthread_mutex_unlock(&send_mutex);
          SendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += frame_len - send_len;
          send_strm_info.last_flag = 1;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += frame_len - send_len;
          /* update the global structure in control sw layer */
          inst->dec.UpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        } else {
          SendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += bytes;
          send_strm_info.last_flag = 1;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += bytes;
          /* update the global structure in control sw layer */
          inst->dec.UpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        }
      }	
	 usleep(1000); /* delay 1 ms */
      /* update length register if hw is ready */
     inst->dec.UpdateStrmInfoCtrl(inst->dec.inst, send_strm_info.last_flag, send_strm_info.strm_bus_addr);
    }

    if(pic_decoded == 1) {
      /* reset flag & update variable */
      pic_decoded = 0;
      send_strm_info.last_flag = 0;
      send_strm_info.send_len = 0;
      /* do sync with main thread after one frame decoded */
      sem_post(&curr_frame_end_sem);
      sem_wait(&next_frame_start_sem);
    }
  }
  return NULL;
}

static enum DecRet HevcInit(const void** inst, struct DecConfig *config,
                            const void *dwl) {
  struct HevcDecConfig dec_cfg;
  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  dec_cfg.output_format = config->output_format;
  dec_cfg.decoder_mode = config->decoder_mode;
  dec_cfg.tile_by_tile = config->tile_by_tile;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 1;
  dec_cfg.mcinit_cfg.mc_enable = config->mc_cfg.mc_enable;
  dec_cfg.mcinit_cfg.stream_consumed_callback = config->mc_cfg.stream_consumed_callback;

  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;
  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
  return HevcDecInit(inst, dwl, &dec_cfg);
}

static enum DecRet HevcGetInfo(void* inst, struct DecSequenceInfo* info) {
  struct HevcDecInfo hevc_info = { 0 };
  enum DecRet rv = HevcDecGetInfo(inst, &hevc_info);
  info->pic_width = hevc_info.pic_width;
  info->pic_height = hevc_info.pic_height;
  info->sar_width = hevc_info.sar_width;
  info->sar_height = hevc_info.sar_height;
  info->crop_params.crop_left_offset = hevc_info.crop_params.crop_left_offset;
  info->crop_params.crop_out_width = hevc_info.crop_params.crop_out_width;
  info->crop_params.crop_top_offset = hevc_info.crop_params.crop_top_offset;
  info->crop_params.crop_out_height = hevc_info.crop_params.crop_out_height;
  info->video_range = hevc_info.video_range;
  info->matrix_coefficients = hevc_info.matrix_coefficients;
  info->is_mono_chrome = hevc_info.mono_chrome;
  info->is_interlaced = hevc_info.interlaced_sequence;
  info->num_of_ref_frames = hevc_info.pic_buff_size;
  return rv;
}

static enum DecRet HevcSetInfo(void* inst, struct DecConfig* config, struct DecSequenceInfo* info) {
  struct HevcDecConfig dec_cfg;
  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  dec_cfg.output_format = config->output_format;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 1;
  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;
#if 0
  if (config->crop_cfg.crop_enabled) {
    dec_cfg.crop.enabled = 1;
    dec_cfg.crop.x = config->crop_cfg.crop_x;
    dec_cfg.crop.y = config->crop_cfg.crop_y;
    dec_cfg.crop.width = config->crop_cfg.crop_w;
    dec_cfg.crop.height = config->crop_cfg.crop_h;
  } else {
    dec_cfg.crop.enabled = 0;
    dec_cfg.crop.x = 0;
    dec_cfg.crop.y = 0;
    dec_cfg.crop.width = 0;
    dec_cfg.crop.height = 0;
  }
  dec_cfg.scale.enabled = config->scale_cfg.scale_enabled;
  if (config->scale_cfg.scale_mode == FIXED_DOWNSCALE) {
    dec_cfg.scale.width = Info->pic_width / config->scale_cfg.down_scale_x;
    dec_cfg.scale.height = Info->pic_height / config->scale_cfg.down_scale_y;
    dec_cfg.scale.width = ((dec_cfg.scale.width >> 1) << 1);
    dec_cfg.scale.height = ((dec_cfg.scale.height >> 1) << 1);
  } else {
    dec_cfg.scale.width = config->scale_cfg.scaled_w;
    dec_cfg.scale.height = config->scale_cfg.scaled_h;
  }
#else
  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
  memcpy(dec_cfg.delogo_params, config->delogo_params,sizeof(config->delogo_params));
  if (config->fscale_cfg.fixed_scale_enabled) {
    /* Convert fixed ratio scale to ppu_cfg[0] */
    dec_cfg.ppu_cfg[0].enabled = 1;
    if (!config->ppu_cfg[0].crop.enabled) {
      dec_cfg.ppu_cfg[0].crop.enabled = 1;
      dec_cfg.ppu_cfg[0].crop.x = info->crop_params.crop_left_offset;
      dec_cfg.ppu_cfg[0].crop.y = info->crop_params.crop_top_offset;
      dec_cfg.ppu_cfg[0].crop.width = info->crop_params.crop_out_width;
      dec_cfg.ppu_cfg[0].crop.height = info->crop_params.crop_out_height;
    }
    if (!config->ppu_cfg[0].scale.enabled) {
      dec_cfg.ppu_cfg[0].scale.enabled = 1;
      dec_cfg.ppu_cfg[0].scale.width = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.width,
                                                       config->fscale_cfg.down_scale_x);
      dec_cfg.ppu_cfg[0].scale.height = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.height,
                                                        config->fscale_cfg.down_scale_y);
    }
  }
#endif
  dec_cfg.fixed_scale_enabled = config->fscale_cfg.fixed_scale_enabled;
  dec_cfg.align = config->align;
  /* TODO(min): assume 1-byte aligned is only applied for pp output */
  if (dec_cfg.align == DEC_ALIGN_1B)
    dec_cfg.align = DEC_ALIGN_64B;
  return HevcDecSetInfo(inst, &dec_cfg);
}

static enum DecRet HevcDecode(void* inst, struct DWLLinearMem* input, struct DecOutput* output,
                              u8* stream, u32 strm_len, struct DWL* dwl, u32 pic_id, void *p_user_data) {
  enum DecRet rv;
  struct HevcDecInput hevc_input;
  struct HevcDecOutput hevc_output;
  dwl->memset(&hevc_input, 0, sizeof(hevc_input));
  dwl->memset(&hevc_output, 0, sizeof(hevc_output));
  hevc_input.stream = (u8*)stream;
  hevc_input.stream_bus_address = input->bus_address + ((addr_t)stream - (addr_t)input->virtual_address);
  hevc_input.data_len = strm_len;
  hevc_input.buffer = (u8 *)input->virtual_address;
  hevc_input.buffer_bus_address = input->bus_address;
  hevc_input.buff_len = input->size;
  hevc_input.p_user_data = p_user_data;
  hevc_input.pic_id = pic_id;
  /* TODO(vmr): hevc must not acquire the resources automatically after
   *            successful header decoding. */
  rv = HevcDecDecode(inst, &hevc_input, &hevc_output);
  output->strm_curr_pos = hevc_output.strm_curr_pos;
  output->strm_curr_bus_address = hevc_output.strm_curr_bus_address;
  output->data_left = hevc_output.data_left;
  if (low_latency) {
    if (hevc_output.strm_curr_pos < stream) {
      data_consumed += (u32)(hevc_output.strm_curr_pos + input->size - (u8*)stream);
    } else {
      data_consumed += (u32)(hevc_output.strm_curr_pos - stream);
    }
    output->data_left = frame_len - data_consumed;
    if (data_consumed >= frame_len) {
      output->data_left = 0;
      data_consumed = 0;
      pic_decoded = 1;
    }
  }
  return rv;
}

static enum DecRet HevcNextPicture(void* inst, struct DecPicturePpu* pic,
                                   struct DWL* dwl) {
  enum DecRet rv;
  u32 stride, stride_ch, i;
#ifdef SUPPORT_DEC400
  u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address=0;
  u32 tile_status_address_offset = 0;
#endif
  struct HevcDecPicture hpic = {{0},0};
  rv = HevcDecNextPicture(inst, &hpic);
  dwl->memset(pic, 0, sizeof(struct DecPicturePpu));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!hpic.pp_enabled) {
      stride = hpic.pictures[0].pic_stride;
      stride_ch = hpic.pictures[0].pic_stride_ch;
    } else {
      stride = hpic.pictures[i].pic_stride;
      stride_ch = hpic.pictures[i].pic_stride_ch;
    }
    pic->pictures[i].luma.virtual_address = (u32*)hpic.pictures[i].output_picture;
    pic->pictures[i].luma.bus_address = hpic.pictures[i].output_picture_bus_address;
#if 0
  if (hpic.output_format == DEC_OUT_FRM_RASTER_SCAN)
    hpic.pic_width = NEXT_MULTIPLE(hpic.pic_width, 16);
#endif
    if (IS_PIC_TILE(hpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * (NEXT_MULTIPLE(hpic.pictures[i].pic_height, 4) / 4);
      pic->pictures[i].chroma.size = stride_ch * (NEXT_MULTIPLE(hpic.pictures[i].pic_height / 2, 4) / 4);
    } else if (IS_PIC_PLANAR(hpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height;
    } else if (hpic.pictures[i].output_format == DEC_OUT_FRM_RFC) {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height / 4;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height / 8;
    } else {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height / 2;
    }
    /* TODO temporal solution to set chroma base here */
    pic->pictures[i].chroma.virtual_address = (u32*)hpic.pictures[i].output_picture_chroma;
    pic->pictures[i].chroma.bus_address = hpic.pictures[i].output_picture_chroma_bus_address;
#ifdef SUPPORT_DEC400
    if (pic->pictures[i].luma.size) {
      if(tile_status_bus_address == 0) {
        tile_status_virtual_address = pic->pictures[i].luma.virtual_address;
        tile_status_bus_address = pic->pictures[i].luma.bus_address;
      }
      tile_status_address_offset += pic->pictures[i].luma.size;
      if (pic->pictures[i].chroma.virtual_address)
        tile_status_address_offset += pic->pictures[i].chroma.size;
    }
#endif
    /* TODO(vmr): find out for real also if it is B frame */
    pic->pictures[i].picture_info.pic_coding_type =
      hpic.is_idr_picture ? DEC_PIC_TYPE_I : DEC_PIC_TYPE_P;
    pic->pictures[i].picture_info.format = hpic.pictures[i].output_format;
    pic->pictures[i].picture_info.pic_id = hpic.pic_id;
    pic->pictures[i].picture_info.decode_id = hpic.decode_id;
    pic->pictures[i].picture_info.cycles_per_mb = hpic.cycles_per_mb;
    pic->pictures[i].sequence_info.pic_width = hpic.pictures[i].pic_width;
    pic->pictures[i].sequence_info.pic_height = hpic.pictures[i].pic_height;
    pic->pictures[i].sequence_info.crop_params.crop_left_offset =
      hpic.crop_params.crop_left_offset;
    pic->pictures[i].sequence_info.crop_params.crop_out_width =
      hpic.crop_params.crop_out_width;
    pic->pictures[i].sequence_info.crop_params.crop_top_offset =
      hpic.crop_params.crop_top_offset;
    pic->pictures[i].sequence_info.crop_params.crop_out_height =
      hpic.crop_params.crop_out_height;
    pic->pictures[i].sequence_info.sar_width = hpic.dec_info.sar_width;
    pic->pictures[i].sequence_info.sar_height = hpic.dec_info.sar_height;
    pic->pictures[i].sequence_info.video_range = hpic.dec_info.video_range;
    pic->pictures[i].sequence_info.matrix_coefficients =
      hpic.dec_info.matrix_coefficients;
    pic->pictures[i].sequence_info.is_mono_chrome = 0;//hpic.dec_info.mono_chrome;
    pic->pictures[i].sequence_info.is_interlaced = hpic.dec_info.interlaced_sequence;
    pic->pictures[i].sequence_info.num_of_ref_frames =
      hpic.dec_info.pic_buff_size;
    pic->pictures[i].sequence_info.bit_depth_luma = hpic.bit_depth_luma;
    pic->pictures[i].sequence_info.bit_depth_chroma = hpic.bit_depth_chroma;
    pic->pictures[i].sequence_info.pic_stride = hpic.pictures[i].pic_stride;
    pic->pictures[i].sequence_info.pic_stride_ch = hpic.pictures[i].pic_stride_ch;
    pic->pictures[i].pic_width = hpic.pictures[i].pic_width;
    pic->pictures[i].pic_height = hpic.pictures[i].pic_height;
    pic->pictures[i].pic_stride = hpic.pictures[i].pic_stride;
    pic->pictures[i].pic_stride_ch = hpic.pictures[i].pic_stride_ch;
    pic->pictures[i].pp_enabled = hpic.pp_enabled;
  }
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (pic->pictures[i].luma.size && (hpic.num_tile_columns < 2)) {
#ifdef SUPPORT_DEC400
      pic->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
      pic->pictures[i].luma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_Y_TABLE_OFFSET(i);
      pic->pictures[i].luma_table.size = NEXT_MULTIPLE((pic->pictures[i].luma.size / 256 * 4 + 7) / 8, 16);
      if (pic->pictures[i].chroma.virtual_address) {
        pic->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
        pic->pictures[i].chroma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_UV_TABLE_OFFSET(i);
        pic->pictures[i].chroma_table.size = NEXT_MULTIPLE((pic->pictures[i].chroma.size / 256 * 4 + 7) / 8, 16);
      }
      pic->pictures[i].pic_compressed_status = 2;
#else
      pic->pictures[i].pic_compressed_status = 0;
#endif
    } else {
      pic->pictures[i].pic_compressed_status = 0;
    }
  }
  return rv;
}

static enum DecRet HevcPictureConsumed(void* inst, struct DecPicturePpu* pic,
                                       struct DWL* dwl) {
  struct HevcDecPicture hpic;
  u32 i;
  dwl->memset(&hpic, 0, sizeof(struct HevcDecPicture));
  /* TODO update chroma luma/chroma base */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    hpic.pictures[i].output_picture = pic->pictures[i].luma.virtual_address;
    hpic.pictures[i].output_picture_bus_address = pic->pictures[i].luma.bus_address;
  }
  hpic.is_idr_picture = pic->pictures[0].picture_info.pic_coding_type == DEC_PIC_TYPE_I;
  return HevcDecPictureConsumed(inst, &hpic);
}

static enum DecRet HevcEndOfStream(void* inst) {
  return HevcDecEndOfStream(inst);
}

static enum DecRet HevcGetBufferInfo(void *inst, struct DecBufferInfo *buf_info) {
  struct HevcDecBufferInfo hbuf;
  enum DecRet rv;

  rv = HevcDecGetBufferInfo(inst, &hbuf);
  buf_info->buf_to_free = hbuf.buf_to_free;
  buf_info->next_buf_size = hbuf.next_buf_size;
  buf_info->buf_num = hbuf.buf_num;
  return rv;
}

static enum DecRet HevcAddBuffer(void *inst, struct DWLLinearMem *buf) {
  return HevcDecAddBuffer(inst, buf);
}

static void HevcRelease(void* inst) {
  HevcDecRelease(inst);
}

static void HevcStreamDecoded(void* dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  inst->client.BufferDecoded(inst->client.client,
                             &inst->current_command->params.input);
}


static void HevcUpdateStrmInfoCtrl(void* inst, u32 last_flag, u32 strm_bus_addr) {
  HevcDecUpdateStrmInfoCtrl(inst, last_flag, strm_bus_addr);
}

static void HevcUpdateStrm(struct strmInfo info) {
  HevcDecUpdateStrm(info);
}

#endif /* ENABLE_HEVC_SUPPORT */

#ifdef ENABLE_H264_SUPPORT
#if 0
task_handle run_task(task_func func, void* param) {
  int ret;
  pthread_attr_t attr;
  struct sched_param par;
  pthread_t* thread_handle = malloc(sizeof(pthread_t));
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  ASSERT(ret == 0);
  ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
  par.sched_priority = 60;
  ret = pthread_attr_setschedparam(&attr, &par);
  //ret = pthread_create(thread_handle, &attr, func, param);
  ret = pthread_create(thread_handle, NULL, func, param);
  ASSERT(ret == 0);

  if(ret != 0) {
    free(thread_handle);
    thread_handle = NULL;
  }

  return thread_handle;
}

void wait_for_task_completion(task_handle task) {
  int ret = pthread_join(*((pthread_t*)task), NULL);
  ASSERT(ret == 0);
  free(task);
}

void HevcSendBytesToDDR(u8* p, u32 n, u8* fp) {
  memcpy(p, fp, n);
}

void* send_bytestrm_task(void* param) {
  DecoderInstance* inst = (DecoderInstance*)param;

  u32 packet_size = LOW_LATENCY_PACKET_SIZE;
  u32 bytes = 0;
  u8 *fp = NULL;
  while (!inst->dec.inst)
    sched_yield();
  sem_wait(&next_frame_start_sem);

  while (!process_end_flag) {
   fp = inst->buffer_status.strm_curr_pos;
    while (frame_len > send_len || pic_decoded == 0) {
      if (frame_len - send_len > packet_size) {
        bytes = packet_size;
        if (fp + bytes > inst->buffer_status.strm_buff + inst->buffer_status.buff_size) {
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp, fp);
          bytes = packet_size - (inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp);
          fp = inst->buffer_status.strm_buff;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_vir_addr = inst->buffer_status.strm_buff;
          send_strm_info.strm_bus_addr = inst->buffer_status.strm_buff_bus_address;
          pthread_mutex_unlock(&send_mutex);
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += packet_size;
          send_strm_info.last_flag = 0;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += packet_size;
          /* update the global structure in control sw layer */
          HevcDecUpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        } else {
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, packet_size, fp);
          fp += packet_size;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += packet_size;
          send_len += packet_size;
          send_strm_info.last_flag = 0;
          send_strm_info.strm_vir_addr += packet_size;
          send_strm_info.send_len += packet_size;
          /* update the global structure in control sw layer */
          HevcDecUpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        }
      } else if(frame_len - send_len > 0) {
        bytes = frame_len - send_len;
        if (fp + bytes > inst->buffer_status.strm_buff + inst->buffer_status.buff_size) {
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp, fp);
          bytes = bytes - (inst->buffer_status.strm_buff + inst->buffer_status.buff_size - fp);
          fp = inst->buffer_status.strm_buff;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_vir_addr = inst->buffer_status.strm_buff;
          send_strm_info.strm_bus_addr = inst->buffer_status.strm_buff_bus_address;
          pthread_mutex_unlock(&send_mutex);
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += frame_len - send_len;
          send_strm_info.last_flag = 1;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += frame_len - send_len;
          /* update the global structure in control sw layer */
          HevcDecUpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        } else {
          HevcSendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
          fp += bytes;
          pthread_mutex_lock(&send_mutex);
          send_strm_info.strm_bus_addr += bytes;
          send_len += bytes;
          send_strm_info.last_flag = 1;
          send_strm_info.strm_vir_addr += bytes;
          send_strm_info.send_len += bytes;
          /* update the global structure in control sw layer */
          HevcDecUpdateStrm(send_strm_info);
          pthread_mutex_unlock(&send_mutex);
        }
      }
      usleep(1000); /* delay 1 ms */
      /* update length register if hw is ready */
      HevcDecUpdateStrmInfoCtrl(inst->dec.inst, send_strm_info.last_flag, send_strm_info.strm_bus_addr);
    }

    if(pic_decoded == 1) {
      /* reset flag & update variable */
      pic_decoded = 0;
      send_strm_info.last_flag = 0;
      send_strm_info.send_len = 0;
      /* do sync with main thread after one frame decoded */
      sem_post(&curr_frame_end_sem);
      sem_wait(&next_frame_start_sem);
    }
  }
  return NULL;
}
#endif

static enum DecRet H264Init(const void** inst, struct DecConfig* config,
                            const void *dwl) {
  struct H264DecConfig dec_cfg;
  enum DecRet ret;
  dec_cfg.mcinit_cfg.mc_enable = config->mc_cfg.mc_enable;
  dec_cfg.mcinit_cfg.stream_consumed_callback = config->mc_cfg.stream_consumed_callback;

  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  dec_cfg.error_handling = DEC_EC_FAST_FREEZE; //config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  //dec_cfg.output_format = config->output_format;
  dec_cfg.decoder_mode = config->decoder_mode;
  dec_cfg.rlc_mode = config->rlc_mode;
  dec_cfg.dpb_flags = DEC_REF_FRM_TILED_DEFAULT;
  dec_cfg.use_display_smoothing = 0;
  //dec_cfg.tile_by_tile = config->tile_by_tile;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 1;
#if 0
  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;
#endif
  memcpy(dec_cfg.ppu_config, config->ppu_cfg, sizeof(config->ppu_cfg));
  ret = H264DecInit(inst, dwl, &dec_cfg);
  if (ret != DEC_OK) return ret;

  if (config->mvc) ret = H264DecSetMvc(*inst);
  return ret;
}

static enum DecRet H264GetInfo(void* inst, struct DecSequenceInfo* info) {
  H264DecInfo h264_info;
  enum DecRet rv = (enum DecRet)H264DecGetInfo(inst, &h264_info);
  info->pic_width = h264_info.pic_width;
  info->pic_height = h264_info.pic_height;
  info->sar_width = h264_info.sar_width;
  info->sar_height = h264_info.sar_height;
  info->crop_params.crop_left_offset = h264_info.crop_params.crop_left_offset;
  info->crop_params.crop_out_width = h264_info.crop_params.crop_out_width;
  info->crop_params.crop_top_offset = h264_info.crop_params.crop_top_offset;
  info->crop_params.crop_out_height = h264_info.crop_params.crop_out_height;
  info->video_range = h264_info.video_range;
  info->matrix_coefficients = h264_info.matrix_coefficients;
  info->is_mono_chrome = h264_info.mono_chrome;
  info->is_interlaced = h264_info.interlaced_sequence;
  info->num_of_ref_frames = h264_info.pic_buff_size;
  info->h264_base_mode = h264_info.base_mode;
  return rv;
}

static enum DecRet H264SetInfo(void* inst, struct DecConfig* config, struct DecSequenceInfo* info) {
  struct H264DecConfig dec_cfg;
  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  //dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  //dec_cfg.output_format = config->output_format;
  dec_cfg.decoder_mode = config->decoder_mode;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 1;
  memcpy(dec_cfg.ppu_config, config->ppu_cfg, sizeof(config->ppu_cfg));
  memcpy(dec_cfg.delogo_params, config->delogo_params,sizeof(config->delogo_params));
  if (config->fscale_cfg.fixed_scale_enabled) {
    /* Convert fixed ratio scale to ppu_config[0] */
    dec_cfg.ppu_config[0].enabled = 1;
    if (!config->ppu_cfg[0].crop.enabled) {
      dec_cfg.ppu_config[0].crop.enabled = 1;
      dec_cfg.ppu_config[0].crop.x = info->crop_params.crop_left_offset;
      dec_cfg.ppu_config[0].crop.y = info->crop_params.crop_top_offset;
      dec_cfg.ppu_config[0].crop.width = info->crop_params.crop_out_width;
      dec_cfg.ppu_config[0].crop.height = info->crop_params.crop_out_height;
    }
    if (!config->ppu_cfg[0].scale.enabled) {
      dec_cfg.ppu_config[0].scale.enabled = 1;
      dec_cfg.ppu_config[0].scale.width = DOWN_SCALE_SIZE(dec_cfg.ppu_config[0].crop.width,
                                                          config->fscale_cfg.down_scale_x);
      dec_cfg.ppu_config[0].scale.height = DOWN_SCALE_SIZE(dec_cfg.ppu_config[0].crop.height,
                                                           config->fscale_cfg.down_scale_y);
    }
  }
  dec_cfg.fixed_scale_enabled = config->fscale_cfg.fixed_scale_enabled;
  dec_cfg.align = config->align;
  dec_cfg.error_conceal = 0;
  /* TODO(min): assume 1-byte aligned is only applied for pp output */
  if (dec_cfg.align == DEC_ALIGN_1B)
    dec_cfg.align = DEC_ALIGN_64B;
  return (enum DecRet)H264DecSetInfo(inst, &dec_cfg);
}

static enum DecRet H264Decode(void* inst, struct DWLLinearMem* input, struct DecOutput* output,
                              u8* stream, u32 strm_len, struct DWL* dwl, u32 pic_id, void *p_user_data) {
  enum DecRet rv;
  H264DecInput H264_input;
  H264DecOutput H264_output;
  dwl->memset(&H264_input, 0, sizeof(H264_input));
  dwl->memset(&H264_output, 0, sizeof(H264_output));
  H264_input.stream = (u8*)stream;
  H264_input.stream_bus_address = input->bus_address + ((addr_t)stream - (addr_t)input->virtual_address);
  H264_input.data_len = strm_len;
  H264_input.buffer = (u8 *)input->virtual_address;
  H264_input.buffer_bus_address = input->bus_address;
  H264_input.buff_len = input->size;
  H264_input.pic_id = pic_id;
  H264_input.p_user_data = p_user_data;
  /* TODO(vmr): H264 must not acquire the resources automatically after
   *            successful header decoding. */
  rv = H264DecDecode(inst, &H264_input, &H264_output);
  output->strm_curr_pos = H264_output.strm_curr_pos;
  output->strm_curr_bus_address = H264_output.strm_curr_bus_address;
  output->data_left = H264_output.data_left;
  if (low_latency) {
    if (H264_output.strm_curr_pos < stream) {
      data_consumed += (u32)(H264_output.strm_curr_pos + input->size - (u8*)stream);
    } else {
      data_consumed += (u32)(H264_output.strm_curr_pos - stream);
    }
    output->data_left = frame_len - data_consumed;
    if (data_consumed >= frame_len || (data_consumed && (rv == DEC_PIC_DECODED))) {
      output->data_left = 0;
      data_consumed = 0;
      pic_decoded = 1;
    }
  }
  return rv;
}

static enum DecRet H264NextPicture(void* inst, struct DecPicturePpu* pic,
                                   struct DWL* dwl) {
  enum DecRet rv;
  u32 stride, stride_ch, i;
#ifdef SUPPORT_DEC400
  u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address=0;
  u32 tile_status_address_offset = 0;
#endif
  H264DecPicture hpic;
  rv = H264DecNextPicture(inst, &hpic, 0);
  dwl->memset(pic, 0, sizeof(struct DecPicturePpu));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    stride = hpic.pictures[i].pic_stride;
    stride_ch = hpic.pictures[i].pic_stride_ch;
    pic->pictures[i].luma.virtual_address = (u32*)hpic.pictures[i].output_picture;
    pic->pictures[i].luma.bus_address = hpic.pictures[i].output_picture_bus_address;
#if 0
  if (hpic.output_format == DEC_OUT_FRM_RASTER_SCAN)
    hpic.pic_width = NEXT_MULTIPLE(hpic.pic_width, 16);
#endif
    if (IS_PIC_TILE(hpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * (NEXT_MULTIPLE(hpic.pictures[i].pic_height, 4) / 4);
      pic->pictures[i].chroma.size = stride_ch * (NEXT_MULTIPLE(hpic.pictures[i].pic_height / 2, 4) / 4);
    } else if (IS_PIC_PLANAR(hpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height;
    } else if (hpic.pictures[i].output_format == DEC_OUT_FRM_RFC) {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height / 4;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height / 8;
    } else {
      pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
      pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height / 2;
    }
    /* TODO temporal solution to set chroma base here */
    pic->pictures[i].chroma.virtual_address = (u32*)hpic.pictures[i].output_picture_chroma;
    pic->pictures[i].chroma.bus_address = hpic.pictures[i].output_picture_chroma_bus_address;
#ifdef SUPPORT_DEC400
    if (pic->pictures[i].luma.size) {
      if(tile_status_bus_address == 0) {
        tile_status_virtual_address = pic->pictures[i].luma.virtual_address;
        tile_status_bus_address = pic->pictures[i].luma.bus_address;
      }
      tile_status_address_offset += pic->pictures[i].luma.size;
      if (pic->pictures[i].chroma.virtual_address)
        tile_status_address_offset += pic->pictures[i].chroma.size;
    }
#endif
    /* TODO(vmr): find out for real also if it is B frame */
    pic->pictures[i].picture_info.pic_coding_type = hpic.pic_coding_type[0];
    pic->pictures[i].picture_info.format = hpic.pictures[i].output_format;
    pic->pictures[i].picture_info.pic_id = hpic.pic_id;
    pic->pictures[i].picture_info.decode_id = hpic.decode_id[0];
    pic->pictures[i].picture_info.cycles_per_mb = hpic.cycles_per_mb;
    pic->pictures[i].sequence_info.pic_width = hpic.pictures[i].pic_width;
    pic->pictures[i].sequence_info.pic_height = hpic.pictures[i].pic_height;
    pic->pictures[i].sequence_info.crop_params.crop_left_offset =
      hpic.crop_params.crop_left_offset;
    pic->pictures[i].sequence_info.crop_params.crop_out_width =
      hpic.crop_params.crop_out_width;
    pic->pictures[i].sequence_info.crop_params.crop_top_offset =
      hpic.crop_params.crop_top_offset;
    pic->pictures[i].sequence_info.crop_params.crop_out_height =
      hpic.crop_params.crop_out_height;
    pic->pictures[i].sequence_info.sar_width = hpic.sar_width;
    pic->pictures[i].sequence_info.sar_height = hpic.sar_height;
    pic->pictures[i].sequence_info.video_range = 0; //hpic.video_range;
    pic->pictures[i].sequence_info.matrix_coefficients =
      0; //hpic.matrix_coefficients;
    pic->pictures[i].sequence_info.is_mono_chrome =
      0; //hpic.mono_chrome;
    pic->pictures[i].sequence_info.is_interlaced = hpic.interlaced;
    pic->pictures[i].sequence_info.num_of_ref_frames =
      0; //hpic.dec_info.pic_buff_size;
    pic->pictures[i].sequence_info.bit_depth_luma = hpic.bit_depth_luma;
    pic->pictures[i].sequence_info.bit_depth_chroma = hpic.bit_depth_chroma;
    pic->pictures[i].sequence_info.pic_stride = hpic.pictures[i].pic_stride;
    pic->pictures[i].sequence_info.pic_stride_ch = hpic.pictures[i].pic_stride_ch;
    pic->pictures[i].pic_width = hpic.pictures[i].pic_width;
    pic->pictures[i].pic_height = hpic.pictures[i].pic_height;
    pic->pictures[i].pic_stride = hpic.pictures[i].pic_stride;
    pic->pictures[i].pic_stride_ch = hpic.pictures[i].pic_stride_ch;
    pic->pictures[i].pp_enabled = 0; //hpic.pp_enabled;
  }
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (pic->pictures[i].luma.size) {
#ifdef SUPPORT_DEC400
      pic->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
      pic->pictures[i].luma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_Y_TABLE_OFFSET(i);
      pic->pictures[i].luma_table.size = NEXT_MULTIPLE((pic->pictures[i].luma.size / 256 * 4 + 7) / 8, 16);
      if (pic->pictures[i].chroma.virtual_address) {
        pic->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
        pic->pictures[i].chroma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_UV_TABLE_OFFSET(i);
        pic->pictures[i].chroma_table.size = NEXT_MULTIPLE((pic->pictures[i].chroma.size / 256 * 4 + 7) / 8, 16);
      }
      pic->pictures[i].pic_compressed_status = 2;
#else
      pic->pictures[i].pic_compressed_status = 0;
#endif
    }
  }
  return rv;
}

static enum DecRet H264PictureConsumed(void* inst, struct DecPicturePpu* pic,
                                       struct DWL* dwl) {
  H264DecPicture hpic;
  u32 i;
  dwl->memset(&hpic, 0, sizeof(H264DecPicture));
  /* TODO update chroma luma/chroma base */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    hpic.pictures[i].output_picture = pic->pictures[i].luma.virtual_address;
    hpic.pictures[i].output_picture_bus_address = pic->pictures[i].luma.bus_address;
  }
  hpic.is_idr_picture[0] = pic->pictures[0].picture_info.pic_coding_type == DEC_PIC_TYPE_I;
  return H264DecPictureConsumed(inst, &hpic);
}

static enum DecRet H264EndOfStream(void* inst) {
  return H264DecEndOfStream(inst, 1);
}

static enum DecRet H264GetBufferInfo(void *inst, struct DecBufferInfo *buf_info) {
  H264DecBufferInfo hbuf;
  enum DecRet rv;

  rv = H264DecGetBufferInfo(inst, &hbuf);
  buf_info->buf_to_free = hbuf.buf_to_free;
  buf_info->next_buf_size = hbuf.next_buf_size;
  buf_info->buf_num = hbuf.buf_num;
  return rv;
}

static enum DecRet H264AddBuffer(void *inst, struct DWLLinearMem *buf) {
  return H264DecAddBuffer(inst, buf);
}

static void H264Release(void* inst) {
  H264DecRelease(inst);
}

static void H264StreamDecoded(void* dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  inst->client.BufferDecoded(inst->client.client,
                             &inst->current_command->params.input);
}

static void H264UpdateStrmInfoCtrl(void* inst, u32 last_flag, u32 strm_bus_addr) {
  H264DecUpdateStrmInfoCtrl(inst, last_flag, strm_bus_addr);
}

static void H264UpdateStrm(struct strmInfo info) {
  H264DecUpdateStrm(info);
}
#endif /* ENABLE_H264_SUPPORT */

#ifdef ENABLE_VP9_SUPPORT
static enum DecRet Vp9Init(const void** inst, struct DecConfig* config,
                           const void *dwl) {
  enum DecPictureFormat format = config->output_format;

  struct Vp9DecConfig dec_cfg;
  dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.num_frame_buffers = 9;
  dec_cfg.dpb_flags = 4;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  dec_cfg.output_format = format;
  dec_cfg.tile_by_tile = config->tile_by_tile;
  dec_cfg.mcinit_cfg.mc_enable = config->mc_cfg.mc_enable;
  dec_cfg.mcinit_cfg.stream_consumed_callback = config->mc_cfg.stream_consumed_callback;

  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;
  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
  return Vp9DecInit(inst, dwl, &dec_cfg);
}

static enum DecRet Vp9GetInfo(void* inst, struct DecSequenceInfo* info) {
  struct Vp9DecInfo vp9_info = {0};
  enum DecRet rv = Vp9DecGetInfo(inst, &vp9_info);
  info->pic_width = vp9_info.frame_width;
  info->pic_height = vp9_info.frame_height;
  info->sar_width = 1;
  info->sar_height = 1;
  info->crop_params.crop_left_offset = 0;
  info->crop_params.crop_out_width = vp9_info.coded_width;
  info->crop_params.crop_top_offset = 0;
  info->crop_params.crop_out_height = vp9_info.coded_height;
  /* TODO(vmr): Consider adding scaled_width & scaled_height. */
  /* TODO(vmr): output_format? */
  info->num_of_ref_frames = vp9_info.pic_buff_size;
  info->video_range = DEC_VIDEO_RANGE_NORMAL;
  info->matrix_coefficients = 0;
  info->is_mono_chrome = 0;
  info->is_interlaced = 0;
  info->bit_depth_luma = info->bit_depth_chroma = vp9_info.bit_depth;
  return rv;
}

static enum DecRet Vp9SetInfo(void* inst, struct DecConfig* config, struct DecSequenceInfo* info) {
  struct Vp9DecConfig dec_cfg;
  dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.num_frame_buffers = 9;
  dec_cfg.dpb_flags = 4;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;

  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
  memcpy(dec_cfg.delogo_params, config->delogo_params,sizeof(config->delogo_params));
  if (config->fscale_cfg.fixed_scale_enabled) {
    /* Convert fixed ratio scale to ppu_cfg[0] */
    dec_cfg.ppu_cfg[0].enabled = 1;
    if (!config->ppu_cfg[0].crop.enabled) {
      dec_cfg.ppu_cfg[0].crop.enabled = 1;
      dec_cfg.ppu_cfg[0].crop.x = info->crop_params.crop_left_offset;
      dec_cfg.ppu_cfg[0].crop.y = info->crop_params.crop_top_offset;
      dec_cfg.ppu_cfg[0].crop.width = (info->crop_params.crop_out_width + 1) & (~0x1);
      dec_cfg.ppu_cfg[0].crop.height = (info->crop_params.crop_out_height + 1) & (~0x1);
    }
    if (!config->ppu_cfg[0].scale.enabled) {
      dec_cfg.ppu_cfg[0].scale.enabled = 1;
      dec_cfg.ppu_cfg[0].scale.width = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.width,
                                                       config->fscale_cfg.down_scale_x);
      dec_cfg.ppu_cfg[0].scale.height = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.height,
                                                        config->fscale_cfg.down_scale_y);
      dec_cfg.ppu_cfg[0].scale.set_by_user = config->fscale_cfg.fixed_scale_enabled;
      dec_cfg.ppu_cfg[0].scale.ratio_x = config->fscale_cfg.down_scale_x;
      dec_cfg.ppu_cfg[0].scale.ratio_y = config->fscale_cfg.down_scale_y;
    }
  }
  dec_cfg.output_format = config->output_format;
  dec_cfg.align = config->align;
  dec_cfg.fixed_scale_enabled = config->fscale_cfg.fixed_scale_enabled;
  /* TODO(min): assume 1-byte aligned is only applied for pp output */
  if (dec_cfg.align == DEC_ALIGN_1B)
    dec_cfg.align = DEC_ALIGN_64B;
  return Vp9DecSetInfo(inst, &dec_cfg);
}

/* function copied from the libvpx */
static void ParseSuperframeIndex(const u8* data, size_t data_sz,
                                 const u8* buf, size_t buf_sz,
                                 u32 sizes[8], i32* count) {
  u8 marker;
  u8* buf_end = (u8*)buf + buf_sz;
  if ((data + data_sz - 1) < buf_end)
    marker = DWLPrivateAreaReadByte(data + data_sz - 1);
  else
    marker = DWLPrivateAreaReadByte(data + (i32)data_sz - 1 - (i32)buf_sz);
  //marker = data[data_sz - 1];
  *count = 0;

  if ((marker & 0xe0) == 0xc0) {
    const u32 frames = (marker & 0x7) + 1;
    const u32 mag = ((marker >> 3) & 0x3) + 1;
    const u32 index_sz = 2 + mag * frames;
    u8 index_value;
    u32 turn_around = 0;
    if((data + data_sz - index_sz) < buf_end)
      index_value = DWLPrivateAreaReadByte(data + data_sz - index_sz);
    else {
      index_value = DWLPrivateAreaReadByte(data + data_sz - index_sz - buf_sz);
      turn_around = 1;
    }

    if (data_sz >= index_sz && index_value == marker) {
      /* found a valid superframe index */
      u32 i, j;
      const u8* x = data + data_sz - index_sz + 1;
      if(turn_around)
        x = data + data_sz - index_sz + 1 - buf_sz;

      for (i = 0; i < frames; i++) {
        u32 this_sz = 0;

        for (j = 0; j < mag; j++) {
          if (x == buf + buf_sz)
            x = buf;
          this_sz |= DWLPrivateAreaReadByte(x) << (j * 8);
          x++;
        }
        sizes[i] = this_sz;
      }

      *count = frames;
    }
  }
}

static enum DecRet Vp9Decode(void* inst, struct DWLLinearMem* input, struct DecOutput* output,
                             u8* stream, u32 strm_len, struct DWL* dwl, u32 pic_id, void *p_user_data) {
  enum DecRet rv;
  struct Vp9DecInput vp9_input;
  struct Vp9DecOutput vp9_output;
  dwl->memset(&vp9_input, 0, sizeof(vp9_input));
  dwl->memset(&vp9_output, 0, sizeof(vp9_output));
  vp9_input.stream = (u8*)stream;
  vp9_input.stream_bus_address = input->bus_address + ((addr_t)stream - (addr_t)input->virtual_address);
  vp9_input.data_len = strm_len;
  vp9_input.buffer = (u8*)input->virtual_address;
  vp9_input.buffer_bus_address = input->bus_address;
  vp9_input.buff_len = input->size;
  vp9_input.p_user_data = p_user_data;
  vp9_input.pic_id = pic_id;
  static u32 sizes[8];
  static i32 frames_this_pts, frame_count = 0;
  u32 data_sz = vp9_input.data_len;
  u32 data_len = vp9_input.data_len;
  u32 consumed_sz = 0;
  const u8* data_start = vp9_input.stream;
  const u8* buf_end = vp9_input.buffer + vp9_input.buff_len;
  //const u8* data_end = data_start + data_sz;

  /* TODO(vmr): vp9 must not acquire the resources automatically after
   *            successful header decoding. */

  /* TODO: Is this correct place to handle superframe indexes? */
  ParseSuperframeIndex(vp9_input.stream, data_sz,
                       vp9_input.buffer, input->size,
                       sizes, &frames_this_pts);

  do {
    /* Skip over the superframe index, if present */
    if (data_sz && (DWLPrivateAreaReadByte(data_start) & 0xe0) == 0xc0) {
      const u8 marker = DWLPrivateAreaReadByte(data_start);
      const u32 frames = (marker & 0x7) + 1;
      const u32 mag = ((marker >> 3) & 0x3) + 1;
      const u32 index_sz = 2 + mag * frames;
      u8 index_value;
      if(data_start + index_sz - 1 < buf_end)
        index_value = DWLPrivateAreaReadByte(data_start + index_sz - 1);
      else
        index_value = DWLPrivateAreaReadByte(data_start + (i32)index_sz - 1 - (i32)vp9_input.buff_len);

      if (data_sz >= index_sz && index_value == marker) {
        data_start += index_sz;
        if (data_start >= buf_end)
          data_start -= vp9_input.buff_len;
        consumed_sz += index_sz;
        data_sz -= index_sz;
        if (consumed_sz < data_len)
          continue;
        else {
          frames_this_pts = 0;
          frame_count = 0;
          break;
        }
      }
    }

    /* Use the correct size for this frame, if an index is present. */
    if (frames_this_pts) {
      u32 this_sz = sizes[frame_count];

      if (data_sz < this_sz) {
        /* printf("Invalid frame size in index\n"); */
        return DEC_STRM_ERROR;
      }

      data_sz = this_sz;
      // frame_count++;
    }

    /* For superframe, although there are several pictures, stream is consumed only once */
    if (frames_this_pts > 1 && frame_count < frames_this_pts - 1)
      vp9_input.p_user_data = NULL;
    else
      vp9_input.p_user_data = p_user_data;

    vp9_input.stream_bus_address =
      input->bus_address + (addr_t)data_start - (addr_t)input->virtual_address;
    vp9_input.stream = (u8*)data_start;
    vp9_input.data_len = data_sz;
    // Once "DEC_NO_DECODING_BUFFER" is returned, continue decoding the same stream.
    do {
      rv = Vp9DecDecode(inst, &vp9_input, &vp9_output);
      if (rv == DEC_NO_DECODING_BUFFER) {
	    usleep(10);
      }
    } while (rv == DEC_NO_DECODING_BUFFER);
    /* Headers decoded or error occurred */
    if (rv == DEC_HDRS_RDY || rv != DEC_PIC_DECODED) break;
    else if (frames_this_pts) frame_count++;

    data_start += data_sz;
    consumed_sz += data_sz;
    if (data_start >= buf_end)
      data_start -= vp9_input.buff_len;

    /* Account for suboptimal termination by the encoder. */
    while (consumed_sz < data_len && *data_start == 0) {
      data_start++;
      consumed_sz++;
      if (data_start >= buf_end)
        data_start -= vp9_input.buff_len;
    }

    data_sz = data_len - consumed_sz;

  } while ((consumed_sz < data_len) && (frame_count <= frames_this_pts));

  /* TODO(vmr): output is complete garbage on on VP9. Fix in the code decoding
   *            the frame headers. */
  /*output->strm_curr_pos = vp9_output.strm_curr_pos;
    output->strm_curr_bus_address = vp9_output.strm_curr_bus_address;
    output->data_left = vp9_output.data_left;*/
  switch (rv) {/* Workaround */
  case DEC_HDRS_RDY:
    output->strm_curr_pos = (u8*)vp9_input.stream;
    output->strm_curr_bus_address = vp9_input.stream_bus_address;
    output->data_left = vp9_input.data_len;
    break;
  case DEC_WAITING_FOR_BUFFER:
    output->strm_curr_pos = (u8*)vp9_input.stream;
    output->strm_curr_bus_address = vp9_input.stream_bus_address;
    output->data_left = data_len - consumed_sz;
    break;
  default:
    if ((vp9_input.stream + vp9_input.data_len) >= buf_end) {
      output->strm_curr_pos = (u8*)(vp9_input.stream + vp9_input.data_len
                                    - vp9_input.buff_len);
      output->strm_curr_bus_address = vp9_input.stream_bus_address + vp9_input.data_len
                                      - vp9_input.buff_len;
    } else {
      output->strm_curr_pos = (u8*)(vp9_input.stream + vp9_input.data_len);
      output->strm_curr_bus_address = vp9_input.stream_bus_address + vp9_input.data_len;
    }
    output->data_left = 0;
    break;
  }
  return rv;
}

static enum DecRet Vp9NextPicture(void* inst, struct DecPicturePpu* pic,
                                  struct DWL* dwl) {
  enum DecRet rv;
  u32 stride, stride_ch, i;
#ifdef SUPPORT_DEC400
  u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address=0;
  u32 tile_status_address_offset = 0;
#endif
  struct Vp9DecPicture vpic = {0};
  rv = Vp9DecNextPicture(inst, &vpic);
  dwl->memset(pic, 0, sizeof(struct DecPicture));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!vpic.pp_enabled) {
      stride = vpic.pictures[0].pic_stride;
      stride_ch = vpic.pictures[0].pic_stride_ch;
    } else {
      stride = vpic.pictures[i].pic_stride;
      stride_ch = vpic.pictures[i].pic_stride_ch;
    }
    pic->pictures[i].luma.virtual_address = (u32*)vpic.pictures[i].output_luma_base;
    pic->pictures[i].luma.bus_address = vpic.pictures[i].output_luma_bus_address;
    if (IS_PIC_TILE(vpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * (NEXT_MULTIPLE(vpic.pictures[i].frame_height, 4) / 4);
      pic->pictures[i].chroma.size = stride_ch * (NEXT_MULTIPLE(vpic.pictures[i].frame_height / 2, 4) / 4);
    } else if (IS_PIC_PLANAR(vpic.pictures[i].output_format)) {
      pic->pictures[i].luma.size = stride * vpic.pictures[i].frame_height;
      pic->pictures[i].chroma.size = stride_ch * vpic.pictures[i].frame_height;
    } else if (vpic.pictures[i].output_format == DEC_OUT_FRM_RFC) {
      pic->pictures[i].luma.size = stride * vpic.pictures[i].frame_height / 4;
      pic->pictures[i].chroma.size = stride_ch * vpic.pictures[i].frame_height / 8;
   } else {
      pic->pictures[i].luma.size = stride * vpic.pictures[i].frame_height;
      pic->pictures[i].chroma.size = stride_ch * vpic.pictures[i].frame_height / 2;
   }
    pic->pictures[i].chroma.virtual_address = (u32*)vpic.pictures[i].output_chroma_base;
    pic->pictures[i].chroma.bus_address = vpic.pictures[i].output_chroma_bus_address;
    pic->pictures[i].pic_width = vpic.pictures[i].frame_width;
    pic->pictures[i].pic_height = vpic.pictures[i].frame_height;
    pic->pictures[i].pic_stride = vpic.pictures[i].pic_stride;
    pic->pictures[i].pic_stride_ch = vpic.pictures[i].pic_stride_ch;
#ifdef SUPPORT_DEC400
    if (pic->pictures[i].luma.size) {
      if(tile_status_bus_address == 0) {
        tile_status_virtual_address = pic->pictures[i].luma.virtual_address;
        tile_status_bus_address = pic->pictures[i].luma.bus_address;
      }
      tile_status_address_offset += pic->pictures[i].luma.size;
      if (pic->pictures[i].chroma.virtual_address)
        tile_status_address_offset += pic->pictures[i].chroma.size;
    }
#endif
    /* TODO(vmr): find out for real also if it is B frame */
    pic->pictures[i].picture_info.pic_coding_type =
      vpic.is_intra_frame ? DEC_PIC_TYPE_I : DEC_PIC_TYPE_P;
    pic->pictures[i].sequence_info.pic_width = vpic.pictures[i].frame_width;
    pic->pictures[i].sequence_info.pic_height = vpic.pictures[i].frame_height;
    pic->pictures[i].sequence_info.sar_width = 1;
    pic->pictures[i].sequence_info.sar_height = 1;
    pic->pictures[i].sequence_info.crop_params.crop_left_offset = 0;
    pic->pictures[i].sequence_info.crop_params.crop_out_width = vpic.coded_width;
    pic->pictures[i].sequence_info.crop_params.crop_top_offset = 0;
    pic->pictures[i].sequence_info.crop_params.crop_out_height = vpic.coded_height;
    pic->pictures[i].sequence_info.video_range = DEC_VIDEO_RANGE_NORMAL;
    pic->pictures[i].sequence_info.matrix_coefficients = 0;
    pic->pictures[i].sequence_info.is_mono_chrome = 0;
    pic->pictures[i].sequence_info.is_interlaced = 0;
    pic->pictures[i].sequence_info.num_of_ref_frames = pic->pictures[i].sequence_info.num_of_ref_frames;
    pic->pictures[i].picture_info.format = vpic.pictures[i].output_format;
    pic->pictures[i].picture_info.pic_id = vpic.pic_id;
    pic->pictures[i].picture_info.decode_id = vpic.decode_id;
    pic->pictures[i].picture_info.cycles_per_mb = vpic.cycles_per_mb;
    pic->pictures[i].sequence_info.bit_depth_luma = vpic.bit_depth_luma;
    pic->pictures[i].sequence_info.bit_depth_chroma = vpic.bit_depth_chroma;
    pic->pictures[i].sequence_info.pic_stride = vpic.pictures[i].pic_stride;
    pic->pictures[i].sequence_info.pic_stride_ch = vpic.pictures[i].pic_stride_ch;
    pic->pictures[i].pp_enabled = vpic.pp_enabled;
  }
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (pic->pictures[i].luma.size && (vpic.num_tile_columns < 2)) {
#ifdef SUPPORT_DEC400
      pic->pictures[i].luma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_Y_TABLE_OFFSET(i));
      pic->pictures[i].luma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_Y_TABLE_OFFSET(i);
      pic->pictures[i].luma_table.size = NEXT_MULTIPLE((pic->pictures[i].luma.size / 256 * 4 + 7) / 8, 16);
      if (pic->pictures[i].chroma.virtual_address) {
        pic->pictures[i].chroma_table.virtual_address = (u32*)((addr_t)tile_status_virtual_address + tile_status_address_offset + DEC400_PPn_UV_TABLE_OFFSET(i));
        pic->pictures[i].chroma_table.bus_address= tile_status_bus_address + tile_status_address_offset+ DEC400_PPn_UV_TABLE_OFFSET(i);
        pic->pictures[i].chroma_table.size = NEXT_MULTIPLE((pic->pictures[i].chroma.size / 256 * 4 + 7) / 8, 16);
      }
      pic->pictures[i].pic_compressed_status = 2;
#else
      pic->pictures[i].pic_compressed_status = 0;
#endif
    } else {
      pic->pictures[i].pic_compressed_status = 0;
    }
  }
  return rv;
}

static enum DecRet Vp9PictureConsumed(void* inst, struct DecPicturePpu* pic,
                                      struct DWL* dwl) {
  struct Vp9DecPicture vpic;
  u32 i;
  dwl->memset(&vpic, 0, sizeof(struct Vp9DecPicture));
  /* TODO chroma base needed? */
  for (i = 0; i < DEC_MAX_OUT_COUNT;i++) {
    vpic.pictures[i].output_luma_base = pic->pictures[i].luma.virtual_address;
    vpic.pictures[i].output_luma_bus_address = pic->pictures[i].luma.bus_address;
  }
  vpic.pp_enabled = pic->pictures[0].pp_enabled;
  vpic.is_intra_frame = pic->pictures[0].picture_info.pic_coding_type == DEC_PIC_TYPE_I;
  return Vp9DecPictureConsumed(inst, &vpic);
}

static enum DecRet Vp9EndOfStream(void* inst) {
  return Vp9DecEndOfStream(inst);
}

static enum DecRet Vp9GetBufferInfo(void *inst, struct DecBufferInfo *buf_info) {
  struct Vp9DecBufferInfo vbuf;
  enum DecRet rv;

  rv = Vp9DecGetBufferInfo(inst, &vbuf);
  buf_info->buf_to_free = vbuf.buf_to_free;
  buf_info->next_buf_size = vbuf.next_buf_size;
  buf_info->buf_num = vbuf.buf_num;
  return rv;
}

static enum DecRet Vp9AddBuffer(void *inst, struct DWLLinearMem *buf) {
  return Vp9DecAddBuffer(inst, buf);
}

static void Vp9Release(void* inst) {
  Vp9DecRelease(inst);
}

static void Vp9StreamDecoded(void* dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  if (inst->prev_input.data_len) {
    inst->client.BufferDecoded(inst->client.client, &inst->prev_input);
  }
  inst->prev_input.data_len = inst->current_command->params.input.data_len;
  inst->prev_input.buffer.virtual_address =
    inst->current_command->params.input.buffer.virtual_address;
  inst->prev_input.buffer.bus_address =
    inst->current_command->params.input.buffer.bus_address;
  inst->prev_input.buffer.size =
    inst->current_command->params.input.buffer.size;
  inst->prev_input.buffer.logical_size =
    inst->current_command->params.input.buffer.logical_size;
  inst->prev_input.stream[0] = (u8*)inst->current_command->params.input.stream[0];
  inst->prev_input.stream[1] = (u8*)inst->current_command->params.input.stream[1];

}
#endif /* ENABLE_VP9_SUPPORT */
#if 0
void StreamBufferConsumedMC(void *stream, void *dec_inst) {
  int i;
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  for (i = 0; i < GetStreamBufferCount(inst->client); i++) {
    if (inst->client.buffers[i].buffer.virtual_address == stream)
      break;
  }

  inst->client.BufferDecoded(inst->client.client,
                           &inst->client.buffers[i].buffer);
}
#endif

#ifdef ENABLE_AVS2_SUPPORT
static enum DecRet DecAvs2Init(const void** inst, struct DecConfig* config,
                            const void *dwl) {
  struct Avs2DecConfig dec_cfg;
  //H264DecMCConfig mcinit_cfg;
  //mcinit_cfg.mc_enable = config->mc_cfg.mc_enable;
  //mcinit_cfg.stream_consumed_callback = config->mc_cfg.stream_consumed_callback;

  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  //dec_cfg.error_handling = DEC_EC_FAST_FREEZE; //config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  dec_cfg.output_format = config->output_format;
  dec_cfg.decoder_mode = config->decoder_mode;
  dec_cfg.align = config->align;
  //dec_cfg.dpb_flags = DEC_REF_FRM_TILED_DEFAULT;
  //dec_cfg.tile_by_tile = config->tile_by_tile;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 0;
#if 0
  if (config->use_8bits_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUT_8BIT;
  else if (config->use_p010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_P010;
  else if (config->use_1010_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_1010;
  else if (config->use_bige_output)
    dec_cfg.pixel_format = DEC_OUT_PIXEL_CUSTOMER1;
  else
    dec_cfg.pixel_format = DEC_OUT_PIXEL_DEFAULT;
#endif
  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
  return (enum DecRet)Avs2DecInit(inst, dwl, &dec_cfg);
}

static enum DecRet DecAvs2GetInfo(void* inst, struct DecSequenceInfo* info) {
  struct Avs2DecInfo avs2_info;
  enum DecRet rv = (enum DecRet)Avs2DecGetInfo(inst, &avs2_info);
  info->pic_width = avs2_info.pic_width;
  info->pic_height = avs2_info.pic_height;
  info->sar_width = avs2_info.sar_width;
  info->sar_height = avs2_info.sar_height;
  //info->crop_params.crop_left_offset = avs2_info.crop_params.crop_left_offset;
  //info->crop_params.crop_out_width = avs2_info.crop_params.crop_out_width;
  //info->crop_params.crop_top_offset = avs2_info.crop_params.crop_top_offset;
  //info->crop_params.crop_out_height = avs2_info.crop_params.crop_out_height;
  info->video_range = avs2_info.video_range;
  //info->matrix_coefficients = avs2_info.matrix_coefficients;
  info->is_mono_chrome = avs2_info.mono_chrome;
  info->is_interlaced = avs2_info.interlaced_sequence;
  info->num_of_ref_frames = avs2_info.pic_buff_size;
  return rv;
}

static enum DecRet DecAvs2SetInfo(void* inst, struct DecConfig* config, struct DecSequenceInfo* Info) {
  struct Avs2DecConfig dec_cfg;
  dec_cfg.no_output_reordering = config->disable_picture_reordering;
  //dec_cfg.use_video_freeze_concealment = config->concealment_mode;
  dec_cfg.use_video_compressor = config->use_video_compressor;
  dec_cfg.use_ringbuffer = config->use_ringbuffer;
  dec_cfg.output_format = config->output_format;
  dec_cfg.decoder_mode = config->decoder_mode;
  dec_cfg.guard_size = 0;
  dec_cfg.use_adaptive_buffers = 0;
  memcpy(dec_cfg.ppu_cfg, config->ppu_cfg, sizeof(config->ppu_cfg));
#if 0
  if (config->fscale_cfg.fixed_scale_enabled) {
    /* Convert fixed ratio scale to ppu_config[0] */
    dec_cfg.ppu_config[0].enabled = 1;
    if (!config->ppu_cfg[0].crop.enabled) {
      dec_cfg.ppu_config[0].crop.enabled = 1;
      dec_cfg.ppu_config[0].crop.x = 0;
      dec_cfg.ppu_config[0].crop.y = 0;
      dec_cfg.ppu_config[0].crop.width = Info->pic_width;
      dec_cfg.ppu_config[0].crop.height = Info->pic_height;
    }
    if (!config->ppu_cfg[0].scale.enabled) {
      dec_cfg.ppu_config[0].scale.enabled = 1;
      dec_cfg.ppu_config[0].scale.width = dec_cfg.ppu_config[0].crop.width / config->fscale_cfg.down_scale_x;
      dec_cfg.ppu_config[0].scale.height = dec_cfg.ppu_config[0].crop.height / config->fscale_cfg.down_scale_y;
    }
  }
#endif
  if (config->fscale_cfg.fixed_scale_enabled) {
    /* Convert fixed ratio scale to ppu_cfg[0] */
    dec_cfg.ppu_cfg[0].enabled = 1;
    if (!config->ppu_cfg[0].crop.enabled) {
      dec_cfg.ppu_cfg[0].crop.enabled = 1;
      dec_cfg.ppu_cfg[0].crop.x = 0;
      dec_cfg.ppu_cfg[0].crop.y = 0;
      dec_cfg.ppu_cfg[0].crop.width = Info->pic_width;
      dec_cfg.ppu_cfg[0].crop.height = Info->pic_height;
    }
    if (!config->ppu_cfg[0].scale.enabled) {
      dec_cfg.ppu_cfg[0].scale.enabled = 1;
      dec_cfg.ppu_cfg[0].scale.width = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.width ,
                                       config->fscale_cfg.down_scale_x);
      dec_cfg.ppu_cfg[0].scale.height = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[0].crop.height,
                                        config->fscale_cfg.down_scale_y);
    }
  }
  dec_cfg.align = config->align;
  //dec_cfg.error_conceal = 0;
  /* TODO(min): assume 1-byte aligned is only applied for pp output */
  if (dec_cfg.align == DEC_ALIGN_1B)
    dec_cfg.align = DEC_ALIGN_64B;
  return (enum DecRet)Avs2DecSetInfo(inst, &dec_cfg);
}

static enum DecRet DecAvs2Decode(void* inst, struct DWLLinearMem* input, struct DecOutput* output,
                              u8* stream, u32 strm_len, struct DWL* dwl, u32 pic_id, void *p_user_data) {
  enum DecRet rv;
  struct Avs2DecInput Avs2_input;
  struct Avs2DecOutput Avs2_output;
  dwl->memset(&Avs2_input, 0, sizeof(Avs2_input));
  dwl->memset(&Avs2_output, 0, sizeof(Avs2_output));
  Avs2_input.stream = (u8*)stream;
  Avs2_input.stream_bus_address = input->bus_address + ((addr_t)stream - (addr_t)input->virtual_address);
  Avs2_input.data_len = strm_len;
  Avs2_input.buffer = (u8 *)input->virtual_address;
  Avs2_input.buffer_bus_address = input->bus_address;
  Avs2_input.buff_len = input->size;
  Avs2_input.pic_id = pic_id;
  //Avs2_input.p_user_data = p_user_data;
  /* TODO(vmr): H264 must not acquire the resources automatically after
   *            successful header decoding. */
  rv = Avs2DecDecode(inst, &Avs2_input, &Avs2_output);
  output->strm_curr_pos = Avs2_output.strm_curr_pos;
  output->strm_curr_bus_address = Avs2_output.strm_curr_bus_address;
  output->data_left = Avs2_output.data_left;
  if (low_latency) {
    if (Avs2_output.strm_curr_pos < stream) {
      data_consumed += (u32)(Avs2_output.strm_curr_pos + input->size - (u8*)stream);
    } else {
      data_consumed += (u32)(Avs2_output.strm_curr_pos - stream);
    }
    output->data_left = frame_len - data_consumed;
    if (data_consumed >= frame_len) {
      output->data_left = 0;
      data_consumed = 0;
      pic_decoded = 1;
    }
  }
  return rv;
}

static enum DecRet DecAvs2NextPicture(void* inst, struct DecPicturePpu* pic,
                                   struct DWL* dwl) {
  enum DecRet rv;
  u32 stride, stride_ch, i;
  struct Avs2DecPicture hpic;
  rv = Avs2DecNextPicture(inst, &hpic);
  dwl->memset(pic, 0, sizeof(struct DecPicturePpu));
  if (rv==DEC_PIC_RDY) {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      stride = hpic.pictures[i].pic_stride;
      stride_ch = hpic.pictures[i].pic_stride_ch;
      pic->pictures[i].luma.virtual_address = (u32*)hpic.pictures[i].output_picture;
      pic->pictures[i].luma.bus_address = hpic.pictures[i].output_picture_bus_address;
#if 0
    if (hpic.output_format == DEC_OUT_FRM_RASTER_SCAN)
      hpic.pic_width = NEXT_MULTIPLE(hpic.pic_width, 16);
#endif
      if (IS_PIC_TILE(hpic.pictures[i].output_format)) {
        pic->pictures[i].luma.size = stride * (NEXT_MULTIPLE(hpic.pictures[i].pic_height, 4) / 4);
        pic->pictures[i].chroma.size = stride_ch * (NEXT_MULTIPLE(hpic.pictures[i].pic_height / 2, 4) / 4);
      } else if (IS_PIC_PLANAR(hpic.pictures[i].output_format)) {
        pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
        pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height;
      } else {
        pic->pictures[i].luma.size = stride * hpic.pictures[i].pic_height;
        pic->pictures[i].chroma.size = stride_ch * hpic.pictures[i].pic_height / 2;
      }
      /* TODO temporal solution to set chroma base here */
      pic->pictures[i].chroma.virtual_address = (u32*)hpic.pictures[i].output_picture_chroma;
      pic->pictures[i].chroma.bus_address = hpic.pictures[i].output_picture_chroma_bus_address;
      /* TODO(vmr): find out for real also if it is B frame */
      pic->pictures[i].picture_info.pic_coding_type = (int)hpic.type; //FIXME
      pic->pictures[i].picture_info.format = hpic.pictures[i].output_format;
      //pic->pictures[i].picture_info.pixel_format = DEC_OUT_PIXEL_DEFAULT; //hpic.pictures[i].pixel_format;
      pic->pictures[i].picture_info.pic_id = hpic.pic_id;
      pic->pictures[i].picture_info.decode_id = hpic.decode_id; //[0];
      pic->pictures[i].picture_info.cycles_per_mb = hpic.cycles_per_mb;
      pic->pictures[i].sequence_info.pic_width = hpic.pictures[i].pic_width;
      pic->pictures[i].sequence_info.pic_height = hpic.pictures[i].pic_height;
      if (!hpic.pp_enabled) {
        pic->pictures[i].sequence_info.crop_params.crop_left_offset =
          hpic.crop_params.crop_left_offset;
        pic->pictures[i].sequence_info.crop_params.crop_out_width =
          hpic.crop_params.crop_out_width;
        pic->pictures[i].sequence_info.crop_params.crop_top_offset =
          hpic.crop_params.crop_top_offset;
        pic->pictures[i].sequence_info.crop_params.crop_out_height =
          hpic.crop_params.crop_out_height;
      } else {
        pic->pictures[i].sequence_info.crop_params.crop_left_offset = 0;
        pic->pictures[i].sequence_info.crop_params.crop_out_width =
          hpic.pictures[i].pic_width;
        pic->pictures[i].sequence_info.crop_params.crop_top_offset = 0;
        pic->pictures[i].sequence_info.crop_params.crop_out_height =
          hpic.pictures[i].pic_height;
      }
      //pic->pictures[i].sequence_info.sar_width = hpic.sar_width;
      //pic->pictures[i].sequence_info.sar_height = hpic.sar_height;
      pic->pictures[i].sequence_info.video_range = 0; //hpic.video_range;
      pic->pictures[i].sequence_info.matrix_coefficients =
        0; //hpic.matrix_coefficients;
      pic->pictures[i].sequence_info.is_mono_chrome =
        0; //hpic.mono_chrome;
      //pic->pictures[i].sequence_info.is_interlaced = hpic.interlaced;
      pic->pictures[i].sequence_info.num_of_ref_frames =
        0; //hpic.dec_info.pic_buff_size;
      if (hpic.sample_bit_depth!=hpic.output_bit_depth) {
        if (hpic.pp_enabled) {
          pic->pictures[i].sequence_info.bit_depth_luma =
          pic->pictures[i].sequence_info.bit_depth_chroma = 8;
        } else {
          pic->pictures[i].sequence_info.bit_depth_luma = hpic.sample_bit_depth | (hpic.output_bit_depth<<8);
          pic->pictures[i].sequence_info.bit_depth_chroma = hpic.sample_bit_depth | (hpic.output_bit_depth<<8);
        }
      } else {
        pic->pictures[i].sequence_info.bit_depth_luma = hpic.sample_bit_depth;
        pic->pictures[i].sequence_info.bit_depth_chroma = hpic.sample_bit_depth;
      }
      pic->pictures[i].sequence_info.pic_stride = hpic.pictures[i].pic_stride;
      pic->pictures[i].sequence_info.pic_stride_ch = hpic.pictures[i].pic_stride_ch;
      pic->pictures[i].pic_width = hpic.pictures[i].pic_width;
      pic->pictures[i].pic_height = hpic.pictures[i].pic_height;
      pic->pictures[i].pic_stride = hpic.pictures[i].pic_stride;
      pic->pictures[i].pic_stride_ch = hpic.pictures[i].pic_stride_ch;
      pic->pictures[i].pp_enabled = 0; //hpic.pp_enabled;
    }
  }
  return rv;
}

static enum DecRet DecAvs2PictureConsumed(void* inst, struct DecPicturePpu* pic,
                                       struct DWL* dwl) {
  struct Avs2DecPicture hpic;
  u32 i;
  dwl->memset(&hpic, 0, sizeof(hpic));
  /* TODO update chroma luma/chroma base */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    hpic.pictures[i].output_picture = pic->pictures[i].luma.virtual_address;
    hpic.pictures[i].output_picture_bus_address = pic->pictures[i].luma.bus_address;
  }
  hpic.type = pic->pictures[0].picture_info.pic_coding_type;
  return Avs2DecPictureConsumed(inst, &hpic);
}

static enum DecRet DecAvs2EndOfStream(void* inst) {
  return Avs2DecEndOfStream(inst);
}

static enum DecRet DecAvs2GetBufferInfo(void *inst, struct DecBufferInfo *buf_info) {
  struct Avs2DecBufferInfo hbuf;
  enum DecRet rv;

  rv = Avs2DecGetBufferInfo(inst, &hbuf);
  buf_info->buf_to_free = hbuf.buf_to_free;
  buf_info->next_buf_size = hbuf.next_buf_size;
  buf_info->buf_num = hbuf.buf_num;
  return rv;
}

static enum DecRet DecAvs2AddBuffer(void *inst, struct DWLLinearMem *buf) {
  return Avs2DecAddBuffer(inst, buf);
}

static void DecAvs2Release(void* inst) {
  Avs2DecRelease(inst);
}

static void DecAvs2StreamDecoded(void* dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  inst->client.BufferDecoded(inst->client.client,
                             &inst->current_command->params.input);
}
#endif /* ENABLE_AVS2_SUPPORT */

