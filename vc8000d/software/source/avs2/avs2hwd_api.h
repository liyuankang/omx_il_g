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

#ifndef AVS2HWD_DECODER_H_
#define AVS2HWD_DECODER_H_

#include "basetype.h"
#include "deccfg.h"
#include "global.h"
#include "ppu.h"
#include "avs2_params.h"


/* defines */

/* enumerated return values of the functions */
typedef enum {
  HWD_OK,
  HWD_WAIT,
  HWD_BUSY,
  HWD_FAIL,
  HWD_SYSTEM_ERROR,
  HWD_SYSTEM_FETAL,
  HWD_MAX
} HwdRet;

typedef enum {
  ATTRIB_SETUP,
  ATTRIB_CFG,
  ATTRIB_SEQ,
  ATTRIB_PIC,
  ATTRIB_CNT,
  ATTRIB_STREAM,
  ATTRIB_REFS,
  ATTRIB_RECON,
  ATTRIB_PP,
  ATTRIB_MAX
} ATTRIBUTE;

struct Avs2WQMatrix {
  short matrix[4 * 64];
};

/* asic interface */
struct Avs2AsicBuffers {
  /* internal */
  struct DWLLinearMem alf_tbl;
  struct DWLLinearMem wqm_tbl;

  /* external */
  struct DWLLinearMem *stream;
  struct DWLLinearMem *out_buffer;
  struct DWLLinearMem *out_pp_buffer;

  /* params */
  u32 fake_tbly_size;
  u32 fake_tblc_size;
};

/* structure derived from SPS for buffer requirement spec */
struct Avs2BufferSpec {
  struct ReferenceInfo {
    /* tile reference */
    u32 picy_size;
    u32 pic_size;

    /* dmv */
    u32 dmv_size;

    /* compressor */
    u32 tbly_size;
    u32 tblc_size;
    u32 tbl_size;

    /* total = recon+dmv+compress_table */
    u32 buff_size;

    /* dpb number */
    int max_dpb_size; /* derived according to B.2~14 according to profile/level
                        */
  } ref;

  struct OutputInfo {
    /* raster */
    u32 rs_buff_size;
  } out;
};

struct Reference {
  struct DWLLinearMem y;
  struct DWLLinearMem c;
  /* for compression table */
  struct DWLLinearMem y_tbl;
  struct DWLLinearMem c_tbl;
  /* for mv */
  struct DWLLinearMem mv;
  /* poc */
  int img_poi;
  int ref_poc[MAXREF];
};

struct Avs2RefsParam {
  struct Reference ref[MAXREF];
  struct Reference background;
};

struct Avs2ConfigParam {
  u32 use_video_compressor;
  int disable_out_writing;
  u32 start_code_detected;
};

struct Avs2StreamParam {
  int is_rb; /* alwayse 0 */
  u8 *stream;
  addr_t stream_bus_addr;
  u32 stream_length;
  u32 stream_offset;
  /* ring buffer */
  struct DWLLinearMem ring_buffer;
  /* status */
  u32 pos_updated;
};

#define Avs2ReconParam Reference

struct Avs2PpoutParam {
  int type; /* 0: nv12; 1: i420 */
  int is_tile;
  union {
    struct {
      struct DWLLinearMem y;
      struct DWLLinearMem c;
    } nv12;
    struct {
      struct DWLLinearMem y;
      struct DWLLinearMem u;
      struct DWLLinearMem v;
    } i420;
  } pic;
  struct DWLLinearMem *pp_buffer;
  PpUnitIntConfig *ppu_cfg;
  struct DecHwFeatures *hw_feature;
  u32 bottom_flag;
  u32 top_field_first;
};

/* storage data structure, holds all data of a decoder instance */
struct Avs2Hwd {
  /* dwl handel */
  struct DWL *dwl;

  pthread_mutex_t mutex;

  /* id for this job */
  int job_id;
  int core_id;
  /* status */
  u32 status;
  u32 asic_running;
  /* parameter flags, bitwise defined. 1 means this param has been set. */
  u32 flags;
  u32 pp_enabled;
  DecPicAlignment align; /* buffer alignment for both reference/pp output */
  /* parameter set. */
  struct Avs2SeqParam *sps;
  struct Avs2PicParam *pps;
  struct Avs2RefsParam *refs;
  struct Avs2ReconParam *recon;
  struct Avs2PpoutParam *ppout;
  struct Avs2StreamParam *stream;
  /* hw config */
  struct Avs2ConfigParam *cfg;

  /* buffers */
  struct Avs2AsicBuffers *cmems;

  /* registers */
  u32 regs[DEC_X170_REGISTERS];

  /* derived */
  u32 bk_img_is_top_field;


#if 0
  u32 pic_width_in_cbs;
  u32 pic_height_in_cbs;
  u32 pic_width_in_ctbs;
  u32 pic_height_in_ctbs;
  u32 pic_size_in_ctbs;
  u32 pic_size;
  double frame_rate;

  u32 pic_started;

  /* flag to indicate if current access unit contains any valid slices */
  u32 valid_slice_in_access_unit;

  /* pic_id given by application */
  u32 current_pic_id;

  /* flag to store no_output_reordering flag set by the application */
  u32 no_reordering;

  /* pointer to DPB */
  //struct DpbStorage dpb[2];

  /* access unit boundary checking related data */
  //struct AubCheck aub[1];

  /* current processed image */
  //struct Image curr_image[1];

  /* slice header, second structure used as a temporary storage while
   * decoding slice header, first one stores last successfully decoded
   * slice header */
  //struct SliceHeader slice_header[2];

  /* fields to store old stream buffer pointers, needed when only part of
   * a stream buffer is processed by HevcDecode function */
  u32 prev_buf_not_finished;
  const u8 *prev_buf_pointer;
  u32 prev_bytes_consumed;
  //struct StrmData strm[1];

  u32 checked_aub;        /* signal that AUB was checked already */
  u32 prev_idr_pic_ready; /* for FFWD workaround */

  u32 intra_freeze;
  u32 picture_broken;

  i32 poc_last_display;

  u32 dmv_mem_size;
  u32 n_extra_frm_buffers;
  //const struct DpbOutPicture *pending_out_pic;

  u32 no_rasl_output;

  u32 raster_enabled;
  //RasterBufferMgr raster_buffer_mgr;
  u32 pp_enabled;
  u32 down_scale_x_shift;
  u32 down_scale_y_shift;

  u32 use_p010_output;
  u32 use_8bits_output;
  u32 use_video_compressor;
#ifdef USE_FAST_EC
  u32 fast_freeze;
#endif
#endif
};

HwdRet Avs2HwdInit(struct Avs2Hwd *hwd, const void *dwl);
HwdRet Avs2HwdSetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data);
HwdRet Avs2HwdGetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data);
HwdRet Avs2HwdRun(struct Avs2Hwd *hwd);
HwdRet Avs2HwdSync(struct Avs2Hwd *hwd, i32 timeout);
HwdRet Avs2HwdFree(struct Avs2Hwd *hwd);
HwdRet Avs2HwdRelease(struct Avs2Hwd *hwd);

/* utitlity */
HwdRet Avs2HwdAllocInternals(struct Avs2Hwd *hwd,
                             struct Avs2AsicBuffers *cmems);
u32 Avs2HwdCheckStatus(struct Avs2Hwd *hwd, i32 ID);
HwdRet Avs2HwdStopHw(struct Avs2Hwd *hwd, i32 ID);

#endif /* #ifdef AVS2HWD_DECODER_H_ */
