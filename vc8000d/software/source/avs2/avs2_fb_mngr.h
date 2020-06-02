/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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

#ifndef AVS2_FB_MNGR_H_
#define AVS2_FB_MNGR_H_

#include "basetype.h"
#include "avs2decapi.h"
#include "dwlthread.h"

#define MAX_FRAME_BUFFER_NUMBER 34
#define FB_NOT_VALID_ID ~0U

#define FB_HW_OUT_FIELD_TOP 0x10U
#define FB_HW_OUT_FIELD_BOT 0x20U
#define FB_HW_OUT_FRAME (FB_HW_OUT_FIELD_TOP | FB_HW_OUT_FIELD_BOT)

#define ABORT_MARKER 2
#define FLUSH_MARKER 3

struct FrameBufferStatus {
  u32 n_ref_count;
  u32 b_used;
  const void *data;
};

struct OutElement {
  u32 mem_idx;
  struct Avs2DecPicture pic;
};

struct FrameBufferList {
  int b_initialized;
  struct FrameBufferStatus fb_stat[MAX_FRAME_BUFFER_NUMBER];
  struct OutElement out_fifo[MAX_FRAME_BUFFER_NUMBER];
  int out_wr_id;
  int out_rd_id;
  int free_buffers;
  int num_out;

  sem_t out_count_sem;
  pthread_mutex_t out_count_mutex;
  pthread_cond_t out_empty_cv;
  pthread_mutex_t ref_count_mutex;
  pthread_cond_t ref_count_cv;
  pthread_cond_t hw_rdy_cv;
  u32 abort;
  u32 flush_all;
};

struct Avs2DpbStorage;

#define AVS2_RENAME_FUNC(func) AVS2##func

#define AllocateAsicBuffers AVS2_RENAME_FUNC(AllocateAsicBuffers)
#define ReleaseAsicBuffers AVS2_RENAME_FUNC(ReleaseAsicBuffers)
#define InitList AVS2_RENAME_FUNC(InitList)
#define ReInitList AVS2_RENAME_FUNC(ReInitList)
#define ReleaseList AVS2_RENAME_FUNC(ReleaseList)
#define AllocateIdUsed AVS2_RENAME_FUNC(AllocateIdUsed)
#define AllocateIdFree AVS2_RENAME_FUNC(AllocateIdFree)
#define ReleaseId AVS2_RENAME_FUNC(ReleaseId)
#define GetDataById AVS2_RENAME_FUNC(GetDataById)
#define GetIdByData AVS2_RENAME_FUNC(GetIdByData)
#define IncrementRefUsage AVS2_RENAME_FUNC(IncrementRefUsage)
#define DecrementRefUsage AVS2_RENAME_FUNC(DecrementRefUsage)
#define MarkHWOutput AVS2_RENAME_FUNC(MarkHWOutput)
#define ClearHWOutput AVS2_RENAME_FUNC(ClearHWOutput)
#define MarkTempOutput AVS2_RENAME_FUNC(MarkTempOutput)
#define FinalizeOutputAll AVS2_RENAME_FUNC(FinalizeOutputAll)
#define ClearOutput AVS2_RENAME_FUNC(ClearOutput)
#define PopFreeBuffer AVS2_RENAME_FUNC(PopFreeBuffer)
#define PushFreeBuffer AVS2_RENAME_FUNC(PushFreeBuffer)
#define GetFreePicBuffer AVS2_RENAME_FUNC(GetFreePicBuffer)
#define GetFreeBufferCount AVS2_RENAME_FUNC(GetFreeBufferCount)
#define SetFreePicBuffer AVS2_RENAME_FUNC(SetFreePicBuffer)
#define IncrementDPBRefCount AVS2_RENAME_FUNC(IncrementDPBRefCount)
#define DecrementDPBRefCount AVS2_RENAME_FUNC(DecrementDPBRefCount)
#define IsBufferReferenced AVS2_RENAME_FUNC(IsBufferReferenced)
#define IsBufferOutput AVS2_RENAME_FUNC(IsBufferOutput)
#define MarkOutputPicCorrupt AVS2_RENAME_FUNC(MarkOutputPicCorrupt)
#define PushOutputPic AVS2_RENAME_FUNC(PushOutputPic)
#define PeekOutputPic AVS2_RENAME_FUNC(PeekOutputPic)
#define PopOutputPic AVS2_RENAME_FUNC(PopOutputPic)
#define RemoveTempOutputAll AVS2_RENAME_FUNC(RemoveTempOutputAll)
#define RemoveOutputAll AVS2_RENAME_FUNC(RemoveOutputAll)
#define IsOutputEmpty AVS2_RENAME_FUNC(IsOutputEmpty)
#define WaitOutputEmpty AVS2_RENAME_FUNC(WaitOutputEmpty)
#define WaitListNotInUse AVS2_RENAME_FUNC(WaitListNotInUse)
#define MarkIdAllocated AVS2_RENAME_FUNC(MarkIdAllocated)
#define MarkIdFree AVS2_RENAME_FUNC(MarkIdFree)
#define SetAbortStatusInList AVS2_RENAME_FUNC(SetAbortStatusInList)
#define ClearAbortStatusInList AVS2_RENAME_FUNC(ClearAbortStatusInList)
#define ResetOutFifoInList AVS2_RENAME_FUNC(ResetOutFifoInList)
//#define Ceil AVS2_RENAME_FUNC(Ceil)
//#define ScalingList AVS2_RENAME_FUNC(ScalingList)
#define RemoveTempOutputId AVS2_RENAME_FUNC(RemoveTempOutputId)
#define MarkListNotInUse AVS2_RENAME_FUNC(MarkListNotInUse)

u32 InitList(struct FrameBufferList *fb_list);
u32 ReInitList(struct FrameBufferList *fb_list);
void Avs2ReleaseList(struct FrameBufferList *fb_list);

u32 AllocateIdUsed(struct FrameBufferList *fb_list, const void *data);
u32 AllocateIdFree(struct FrameBufferList *fb_list, const void *data);
void ReleaseId(struct FrameBufferList *fb_list, u32 id);
void *GetDataById(struct FrameBufferList *fb_list, u32 id);
u32 GetIdByData(struct FrameBufferList *fb_list, const void *data);

void IncrementRefUsage(struct FrameBufferList *fb_list, u32 id);
void DecrementRefUsage(struct FrameBufferList *fb_list, u32 id);

void IncrementDPBRefCount(struct Avs2DpbStorage *dpb);
void DecrementDPBRefCount(struct Avs2DpbStorage *dpb);

void MarkHWOutput(struct FrameBufferList *fb_list, u32 id, u32 type);
void ClearHWOutput(struct FrameBufferList *fb_list, u32 id, u32 type);

void MarkTempOutput(struct FrameBufferList *fb_list, u32 id);
void ClearOutput(struct FrameBufferList *fb_list, u32 id);

void FinalizeOutputAll(struct FrameBufferList *fb_list);
void RemoveTempOutputAll(struct FrameBufferList *fb_list,
                         struct Avs2DpbStorage *dpb);

u32 GetFreePicBuffer(struct FrameBufferList *fb_list, u32 old_id);
void SetFreePicBuffer(struct FrameBufferList *fb_list, u32 id);
u32 GetFreeBufferCount(struct FrameBufferList *fb_list);

void PushOutputPic(struct FrameBufferList *fb_list,
                   const struct Avs2DecPicture *pic, u32 id);
u32 PeekOutputPic(struct FrameBufferList *fb_list, struct Avs2DecPicture *pic);
u32 PopOutputPic(struct FrameBufferList *fb_list, u32 id);

void MarkOutputPicCorrupt(struct FrameBufferList *fb_list, u32 id, u32 errors);

u32 IsBufferReferenced(struct FrameBufferList *fb_list, u32 id);
void RemoveTempOutputId(struct FrameBufferList *fb_list, u32 id);
u32 IsOutputEmpty(struct FrameBufferList *fb_list);
void WaitOutputEmpty(struct FrameBufferList *fb_list);
void MarkListNotInUse(struct FrameBufferList *fb_list);
void WaitListNotInUse(struct FrameBufferList *fb_list);
void SetAbortStatusInList(struct FrameBufferList *fb_list);
void ClearAbortStatusInList(struct FrameBufferList *fb_list);
void RemoveOutputAll(struct FrameBufferList *fb_list,
                     struct Avs2DpbStorage *dpb);
void ResetOutFifoInList(struct FrameBufferList *fb_list);
void MarkIdAllocated(struct FrameBufferList *fb_list, u32 id);
void MarkIdFree(struct FrameBufferList *fb_list, u32 id);
u32 IsBufferOutput(struct FrameBufferList *fb_list, u32 id);

#endif /*  HEVC_FB_MNGR_H_ */
