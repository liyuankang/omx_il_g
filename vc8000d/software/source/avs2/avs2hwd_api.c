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

#include "sw_debug.h"
#include "avs2hwd_api.h"
#include "avs2hwd_asic.h"
#include "commonconfig.h"
#include <assert.h>

#if 0
static HwdRet Avs2HwdCheckParams(struct Avs2Hwd *hwd) {
  const u32 full_params = (1 << ATTRIB_CNT) - 1;
  if (hwd->flags != full_params)
    return HWD_FAIL;
  else
    return HWD_OK;
}
#endif

/* Set/Get Paramters */
static HwdRet Avs2HwdSetSeqParam(struct Avs2Hwd *hwd,
                                 struct Avs2SeqParam *param) {
  // TODO: check parameter range.
  hwd->sps = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetPicParam(struct Avs2Hwd *hwd,
                                 struct Avs2PicParam *pps) {
  // TODO: check parameter range.
  hwd->pps = pps;

  if (pps->type == I_IMG && pps->typeb == BACKGROUND_IMG) {  // G/GB frame
    hwd->bk_img_is_top_field = pps->is_top_field;
  }
  return HWD_OK;
}

static HwdRet Avs2HwdSetConfig(struct Avs2Hwd *hwd,
                               struct Avs2ConfigParam *param) {
  // TODO: check parameter range.
  hwd->cfg = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetRecon(struct Avs2Hwd *hwd,
                              struct Avs2ReconParam *param) {
  // TODO: check parameter range.
  hwd->recon = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetReference(struct Avs2Hwd *hwd,
                                  struct Avs2RefsParam *refs) {
#if 0
  u32 i, j;
  u32 list0[16] = {0};
  u32 list1[16] = {0};

  /* list 0, short term before + short term after + long term */
  for (i = 0, j = 0; i < dpb->num_poc_st_curr; i++)
    list0[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list0[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list0[j++] = list0[i++];

  /* list 1, short term after + short term before + long term */
  /* after */
  for (i = dpb->num_poc_st_curr_before, j = 0; i < dpb->num_poc_st_curr; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  /* before */
  for (i = 0; i < dpb->num_poc_st_curr_before; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list1[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list1[j++] = list1[i++];

  /* TODO: size? */
  for (i = 0; i < MAX_DPB_SIZE; i++) {
    SetDecRegister(hwd->regs, ref_pic_list0[i], list0[i]);
    SetDecRegister(hwd->regs, ref_pic_list1[i], list1[i]);
  }
#endif
  hwd->refs = refs;

  return HWD_OK;
}

/* Initializes the decoder instance. */
HwdRet Avs2HwdInit(struct Avs2Hwd *hwd, const void *dwl) {

  /* Variables */
//  HwdRet ret;

  /* Code */

  assert(hwd);
  hwd->dwl = (struct DWL *)dwl;
  if (hwd->dwl == NULL) return HWD_FAIL;

  pthread_mutex_init(&hwd->mutex, NULL);

  Avs2SetModeRegs(hwd);

  Avs2HwdSetParams(hwd, ATTRIB_SETUP, NULL);

  return HWD_OK;
}

HwdRet Avs2HwdAllocInternals(struct Avs2Hwd *hwd,
                             struct Avs2AsicBuffers *cmems) {
  i32 dwl_ret;

  /* internal SW/HW buffers */
  hwd->cmems = cmems;

  dwl_ret =
      DWLMallocLinear(hwd->dwl, sizeof(struct Avs2AlfParams), &cmems->alf_tbl);
  if (dwl_ret != DWL_OK) {
    DEBUG_PRINT("[dwl] cannot allocate alf table.\n");
    return HWD_FAIL;
  }
  memset((void*)cmems->alf_tbl.virtual_address, 0, sizeof(struct Avs2AlfParams));

  dwl_ret =
      DWLMallocLinear(hwd->dwl, sizeof(struct Avs2WQMatrix), &cmems->wqm_tbl);
  if (dwl_ret != DWL_OK) {
    DEBUG_PRINT("[dwl] cannot allocate wqm table.\n");
    return HWD_FAIL;
  }

  return HWD_OK;
}

/* Set PP configure Paramters */
static HwdRet Avs2HwdSetPp(struct Avs2Hwd *hwd, struct Avs2PpoutParam *param) {
  hwd->ppout = param;
  return HWD_OK;
}

HwdRet Avs2HwdSetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data) {
  HwdRet ret = HWD_OK;

  pthread_mutex_lock(&hwd->mutex);
  switch (attribute) {
    case ATTRIB_SETUP:
      SetCommonConfigRegs(hwd->regs);
      break;
    case ATTRIB_CFG:
      ret = Avs2HwdSetConfig(hwd, data);
      break;
    case ATTRIB_SEQ:
      ret = Avs2HwdSetSeqParam(hwd, data);
      break;
    case ATTRIB_PIC:
      ret = Avs2HwdSetPicParam(hwd, data);
      break;
    case ATTRIB_RECON:
      ret = Avs2HwdSetRecon(hwd, data);
      break;
    case ATTRIB_PP:
      ret = Avs2HwdSetPp(hwd, data);
      break;
    case ATTRIB_REFS:
      ret = Avs2HwdSetReference(hwd, data);
      break;
    case ATTRIB_STREAM:
      hwd->stream = data;
      break;
    default:
      ret = HWD_FAIL;
      break;
  }

  if (ret == HWD_OK) {
    hwd->flags |= (1 << attribute);
  }
  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdGetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data) {
  switch (attribute) {
    case ATTRIB_STREAM:
    // return Avs2HwdGetStreamParam(hwd, data);
    default:
      break;
  }
  return HWD_FAIL;
}

HwdRet Avs2HwdGetBufferSpec(struct Avs2Hwd *hwd,
                            struct Avs2BufferSpec *buffer_spec) {
  if (hwd->flags & (1 << ATTRIB_SEQ)) {
    DEBUG_PRINT("[avs2hwd] Cannot Get buffer spec before sps is got.\n");
    return HWD_FAIL;
  }

  // FIXME:
  return HWD_FAIL;
}

HwdRet Avs2HwdUpdateStream(struct Avs2Hwd *hwd, u32 asic_status) {
  struct Avs2StreamParam *stream = hwd->stream;
  HwdRet ret = HWD_OK;

  addr_t last_read_address;
  u32 bytes_processed;
  const addr_t start_address = stream->stream_bus_addr & (~DEC_HW_ALIGN_MASK);
  const u32 offset_bytes = stream->stream_bus_addr & DEC_HW_ALIGN_MASK;

  last_read_address = GET_ADDR_REG(hwd->regs, HWIF_STREAM_BASE);

  if (asic_status == DEC_HW_IRQ_RDY &&
      last_read_address == stream->stream_bus_addr) {
    last_read_address = start_address + stream->stream_length;  // FIXME:
  }

  if (last_read_address <= start_address)
    bytes_processed =
        stream->ring_buffer.size - (u32)(start_address - last_read_address);
  else
    bytes_processed = (u32)(last_read_address - start_address);

  DEBUG_PRINT(
      ("HW updated stream position: %08x\n"
       "           processed bytes: %8d\n"
       "     of which offset bytes: %8d\n",
       last_read_address, bytes_processed, offset_bytes));

  /* from start of the buffer add what HW has decoded */
  /* end - start smaller or equal than maximum */
  // FIXME:
  if ((bytes_processed - offset_bytes) > stream->stream_length) {

    if ((asic_status & DEC_HW_IRQ_RDY) || (asic_status & DEC_HW_IRQ_BUFFER)) {
      DEBUG_PRINT(("New stream position out of range!\n"));
      // ASSERT(0);
      stream->stream += stream->stream_length;
      stream->stream_bus_addr += stream->stream_length;
      stream->stream_length = 0; /* no bytes left */

      /* Though asic_status returns DEC_HW_IRQ_BUFFER, the stream consumed is
       * abnormal,
       * so we consider it's an errorous stream and release HW. */
      if (asic_status & DEC_HW_IRQ_BUFFER) {
        u32 irq;
        irq = DWLReleaseHw(hwd->dwl, hwd->core_id);
        if (irq == DWL_CACHE_FATAL_RECOVERY) hwd->status = HWD_SYSTEM_ERROR;
        if (irq == DWL_CACHE_FATAL_UNRECOVERY) hwd->status = HWD_SYSTEM_FETAL;
        ret = HWD_FAIL;
      }
      return ret;
    }

    /* consider all buffer processed */
    stream->stream += stream->stream_length;
    stream->stream_bus_addr += stream->stream_length;
    stream->stream_length = 0; /* no bytes left */
  } else {
    stream->stream_length -= (bytes_processed - offset_bytes);
    stream->stream += (bytes_processed - offset_bytes);
    stream->stream_bus_addr += (bytes_processed - offset_bytes);
  }

  /* if turnaround */
  if (stream->stream >
      ((u8 *)stream->ring_buffer.virtual_address + stream->ring_buffer.size)) {
    stream->stream -= stream->ring_buffer.size;
    stream->stream_bus_addr -= stream->ring_buffer.size;
  }

  return ret;
}

HwdRet Avs2HwdRun(struct Avs2Hwd *hwd) {
  HwdRet ret = HWD_OK;

  pthread_mutex_lock(&hwd->mutex);

#if 0  // FIXME: !!!!
  if (HWD_OK!=Avs2HwdCheckParams(hwd)) {
    ret = HWD_WAIT;
    goto out;
  }
#endif

  Avs2SetRegs(hwd);
#if 1  // FIXME: enable when code move to c-model.
  if (DWL_OK != DWLReserveHw(hwd->dwl, &hwd->core_id, DWL_CLIENT_TYPE_AVS2_DEC)) {
    hwd->status = X170_DEC_HW_RESERVED;
    ret = HWD_FAIL;
    goto out;
  }
  if (hwd->pp_enabled)
    DWLReadPpConfigure(hwd->dwl, hwd->ppout->ppu_cfg, NULL, 0);
  FlushDecRegisters(hwd->dwl, hwd->core_id, hwd->regs);

  // FIXME:
  // DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, p);
  SetDecRegister(hwd->regs, HWIF_DEC_E, 1);
  DWLEnableHw(hwd->dwl, hwd->core_id, 4 * 1, hwd->regs[1]);
#endif
  hwd->status = 0;

out:
  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}


HwdRet Avs2HwdSync(struct Avs2Hwd *hwd, i32 timeout) {
  HwdRet ret = HWD_OK;
  i32 val;
  u32 irq;
  pthread_mutex_lock(&hwd->mutex);

  val = DWLWaitHwReady(hwd->dwl, hwd->core_id, timeout);

  if (val != DWL_HW_WAIT_OK) {
    ERROR_PRINT("DWLWaitHwReady");
    DEBUG_PRINT(("DWLWaitHwReady returned: %d\n", ret));
    // FIXME:

    /* Reset HW */
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0);

    DWLDisableHw(hwd->dwl, hwd->core_id, 4 * 1, hwd->regs[1]);

    irq = DWLReleaseHw(hwd->dwl, hwd->core_id);
    if (irq == DWL_CACHE_FATAL_RECOVERY) {
      hwd->status = X170_DEC_SYSTEM_ERROR;
    } else if (irq == DWL_CACHE_FATAL_UNRECOVERY) {
      hwd->status = X170_DEC_FATAL_SYSTEM_ERROR;
    } else {
      hwd->status =
          (val == DWL_HW_WAIT_ERROR) ? X170_DEC_SYSTEM_ERROR : X170_DEC_TIMEOUT;
    }
    ret = HWD_FAIL;
  } else {

    RefreshDecRegisters(hwd->dwl, hwd->core_id, hwd->regs);
    hwd->status = GetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT);

    SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0); /* just in case */

    if (!(hwd->status & DEC_HW_IRQ_BUFFER)) {
      /* HW done, release it! */
      hwd->asic_running = 0;

      irq = DWLReleaseHw(hwd->dwl, hwd->core_id);
      if (irq == DWL_CACHE_FATAL_RECOVERY) hwd->status = X170_DEC_SYSTEM_ERROR;
      if (irq == DWL_CACHE_FATAL_UNRECOVERY)
        hwd->status = X170_DEC_FATAL_SYSTEM_ERROR;
      ret = HWD_OK;
    }

    /* update stream offset */
    ret = Avs2HwdUpdateStream(hwd, hwd->status);
  }

  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdDone(struct Avs2Hwd *hwd, i32 ID) {
  HwdRet ret = HWD_OK;
  pthread_mutex_lock(&hwd->mutex);

  // FIXME:

  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdRelease(struct Avs2Hwd *hwd) {
  // TODO: check slot status, wait when they are busy
  // const void *dwl = hwd->dwl;

  // Free resources
  if (hwd->cmems) {
    DWLFreeLinear(hwd->dwl, &hwd->cmems->alf_tbl);
    DWLFreeLinear(hwd->dwl, &hwd->cmems->wqm_tbl);
  }

  // DWLRelease() should be called at APP.

  return HWD_OK;
}

u32 Avs2HwdCheckStatus(struct Avs2Hwd *hwd, i32 ID) {
  u32 tmp = DWLReadRegFromHw(hwd->dwl, ID, 4 * 1);
  if (tmp & 0x01)
    return 1;
  else
    return 0;
}

HwdRet Avs2HwdStopHw(struct Avs2Hwd *hwd, i32 ID) {
  SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0);
  SetDecRegister(hwd->regs, HWIF_DEC_E, 0);
  DWLDisableHw(hwd->dwl, ID, 4 * 1, hwd->regs[1]);
  DWLReleaseHw(hwd->dwl, ID); /* release HW lock */

  return HWD_OK;
}
