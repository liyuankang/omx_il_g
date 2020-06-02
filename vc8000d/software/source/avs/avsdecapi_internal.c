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

#include "basetype.h"
#include "avs_container.h"
#include "avs_cfg.h"
#include "avsdecapi.h"
#include "avs_utils.h"
#include "avsdecapi_internal.h"
#include "regdrv.h"
#include "sw_util.h"

/*------------------------------------------------------------------------------

    5.1 Function name:  AvsAPI_InitDataStructures()

        Purpose:        Initialize Data Structures in DecContainer.

        Input:          DecContainer *dec_cont

        Output:         u32

------------------------------------------------------------------------------*/
void AvsAPI_InitDataStructures(DecContainer * dec_cont) {
  /*
   *  have to be initialized into 1 to enable
   *  decoding stream without VO-headers
   */
  dec_cont->Hdrs.video_format = 5;
  dec_cont->Hdrs.transfer_characteristics = 1;
  dec_cont->Hdrs.matrix_coefficients = 1;
  dec_cont->Hdrs.progressive_sequence = 1;
  dec_cont->Hdrs.picture_structure = 3;
  dec_cont->StrmStorage.field_out_index = 1;
  dec_cont->Hdrs.sample_range = 0;
  dec_cont->ApiStorage.first_headers = 1;
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1 =
                                  INVALID_ANCHOR_PICTURE;
}

/*------------------------------------------------------------------------------

    5.5 Function name:  AvsDecTimeCode();

        Purpose:        Write time data to output

        Input:          DecContainer *dec_cont, AvsDecTime *time_code

        Output:         void

------------------------------------------------------------------------------*/

void AvsDecTimeCode(DecContainer * dec_cont, AvsDecTime * time_code) {


  ASSERT(dec_cont);
  ASSERT(time_code);

  time_code->hours = dec_cont->Hdrs.time_code.hours;
  time_code->minutes = dec_cont->Hdrs.time_code.minutes;
  time_code->seconds = dec_cont->Hdrs.time_code.seconds;
  time_code->pictures = dec_cont->Hdrs.time_code.picture;

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsAllocateBuffers

        Purpose:        Allocate memory

        Input:          DecContainer *dec_cont

        Output:         AVSDEC_MEMFAIL/AVSDEC_OK

------------------------------------------------------------------------------*/
AvsDecRet AvsAllocateBuffers(DecContainer * dec_cont) {

  u32 size_tmp = 0;
  u32 i;
  i32 ret = 0;
  u32 buffers = 0;

  ASSERT(dec_cont->StrmStorage.total_mbs_in_frame != 0);

  /* Calculate minimum amount of buffers */
  buffers = 3;
  dec_cont->StrmStorage.num_buffers = dec_cont->StrmStorage.max_num_buffers;
  if( dec_cont->StrmStorage.num_buffers < buffers )
    dec_cont->StrmStorage.num_buffers = buffers;
  ret = BqueueInit2(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.num_buffers );
  if( ret != HANTRO_OK )
    return AVSDEC_MEMFAIL;

  if (dec_cont->pp_enabled) {
    /* Reference images */
    dec_cont->n_int_buf_size = size_tmp = AvsGetRefFrmSize(dec_cont);
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      ret |= DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                             &dec_cont->StrmStorage.p_pic_buf[i].data);

      AVSDEC_DEBUG(("PicBuffer[%d]: %x, %x\n",
                    i,
                    (u32) dec_cont->StrmStorage.p_pic_buf[i].data.
                    virtual_address,
                    dec_cont->StrmStorage.p_pic_buf[i].data.bus_address));

      if(dec_cont->StrmStorage.p_pic_buf[i].data.bus_address == 0) {
        return (AVSDEC_MEMFAIL);
      }
    }
    /* initialize first picture buffer (work_out is 1 for the first picture)
     * grey, may be used as reference in certain error cases */
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf[1].data.virtual_address,
#ifdef ASIC_TRACE_SUPPORT
                     0,
#else
                     128,
#endif
                     dec_cont->StrmStorage.p_pic_buf[1].data.logical_size);
  }

  ret |= DWLMallocLinear(dec_cont->dwl,
                         NEXT_MULTIPLE(((dec_cont->StrmStorage.total_mbs_in_frame / 2 + 1)&
                             ~0x1) * 4 * sizeof(u32), MAX(16, ALIGN(dec_cont->align))) * 2,
                         &dec_cont->StrmStorage.direct_mvs);

  if(ret) {
    return (AVSDEC_MEMFAIL);
  }

  return AVSDEC_OK;

}

/*------------------------------------------------------------------------------

   x.x Function name:  AvsDecCheckSupport

       Purpose:        Check picture sizes etc

       Input:          DecContainer *dec_cont

       Output:         AVSDEC_STRMERROR/AVSDEC_OK

------------------------------------------------------------------------------*/

AvsDecRet AvsDecCheckSupport(DecContainer * dec_cont) {

  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if((dec_cont->StrmStorage.frame_height > (hw_feature.avs_max_dec_pic_height >> 4)) ||
      (dec_cont->StrmStorage.frame_height < (dec_cont->min_dec_pic_height >> 4))) {

    AVSDEC_DEBUG(("AvsDecCheckSupport# Height not supported: %d \n",
                  dec_cont->StrmStorage.frame_height << 4));
    return AVSDEC_STREAM_NOT_SUPPORTED;
  }

  if((dec_cont->StrmStorage.frame_width > (hw_feature.avs_max_dec_pic_width >> 4)) ||
      (dec_cont->StrmStorage.frame_width < (dec_cont->min_dec_pic_width >> 4))) {

    AVSDEC_DEBUG(("AvsDecCheckSupport# Width not supported: %d \n",
                  dec_cont->StrmStorage.frame_width << 4));
    return AVSDEC_STREAM_NOT_SUPPORTED;
  }

  if(dec_cont->StrmStorage.total_mbs_in_frame > AVSAPI_DEC_MBS) {
    AVSDEC_DEBUG(("Maximum number of macroblocks exceeded: %d \n",
                  dec_cont->StrmStorage.total_mbs_in_frame));
    return AVSDEC_STREAM_NOT_SUPPORTED;
  }

  return AVSDEC_OK;

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsDecPreparePicReturn

        Purpose:        Prepare return values for PIC returns
                        For use after HW start

        Input:          DecContainer *dec_cont
                        AvsDecOutput *outData    currently used out

        Output:         void

------------------------------------------------------------------------------*/

void AvsDecPreparePicReturn(DecContainer * dec_cont) {

  ASSERT(dec_cont != NULL);

  if(!dec_cont->Hdrs.progressive_sequence) {
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] = 1;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] = 0;
  } else {
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] = 0;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] = 0;
  }

  return;

}

/*------------------------------------------------------------------------------

   x.x Function name:  AvsDecAspectRatio

       Purpose:        Set aspect ratio values for GetInfo

       Input:          DecContainer *dec_cont
                       AvsDecInfo * dec_info    pointer to DecInfo

       Output:         void

------------------------------------------------------------------------------*/

void AvsDecAspectRatio(DecContainer * dec_cont, AvsDecInfo * dec_info) {

  AVSDEC_DEBUG(("SAR %d\n", dec_cont->Hdrs.aspect_ratio));

  /* If forbidden or reserved */
  if(dec_cont->Hdrs.aspect_ratio == 0 || dec_cont->Hdrs.aspect_ratio > 4) {
    dec_info->display_aspect_ratio = 0;
    return;
  }

  switch (dec_cont->Hdrs.aspect_ratio) {
  case 0x2:  /* 0010 4:3 */
    dec_info->display_aspect_ratio = AVSDEC_4_3;
    break;

  case 0x3:  /* 0011 16:9 */
    dec_info->display_aspect_ratio = AVSDEC_16_9;
    break;

  case 0x4:  /* 0100 2.21:1 */
    dec_info->display_aspect_ratio = AVSDEC_2_21_1;
    break;

  default:   /* Square 0001 1/1 */
    dec_info->display_aspect_ratio = AVSDEC_1_1;
    break;
  }

  /* TODO!  "DAR" */

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsDecBufferPicture

        Purpose:        Handles picture buffering

        Input:


        Output:

------------------------------------------------------------------------------*/
void AvsDecBufferPicture(DecContainer * dec_cont, u32 pic_id, u32 buffer_b,
                         u32 is_inter, AvsDecRet return_value, u32 nbr_err_mbs) {
  i32 i, j;
  u32 pic_type;

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmStorage.out_count <= 16);

  if(buffer_b == 0) {  /* Buffer I or P picture */
    i = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    if(i >= 16)
      i -= 16;
  } else { /* Buffer B picture */
    j = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    i = j - 1;
    if(j >= 16)
      j -= 16;
    if(i < 0)
      i += 16;
    dec_cont->StrmStorage.out_buf[j] = dec_cont->StrmStorage.out_buf[i];
  }
  j = dec_cont->StrmStorage.work_out;

  dec_cont->StrmStorage.out_buf[i] = j;
  dec_cont->StrmStorage.p_pic_buf[j].pic_id = pic_id;
  if(dec_cont->Hdrs.pic_coding_type == IFRAME)
    pic_type = DEC_PIC_TYPE_I;
  else if(dec_cont->Hdrs.pic_coding_type == PFRAME)
    pic_type = DEC_PIC_TYPE_P;
  else
    pic_type = DEC_PIC_TYPE_B;
  dec_cont->StrmStorage.p_pic_buf[j].pic_code_type = pic_type;
  dec_cont->StrmStorage.p_pic_buf[j].ret_val = return_value;
  dec_cont->StrmStorage.p_pic_buf[j].is_inter = is_inter;
  dec_cont->StrmStorage.p_pic_buf[j].pic_type = !is_inter && !buffer_b;
  dec_cont->StrmStorage.p_pic_buf[j].tiled_mode = dec_cont->tiled_reference_enable;

  dec_cont->StrmStorage.p_pic_buf[j].tf = dec_cont->Hdrs.top_field_first;
  dec_cont->StrmStorage.p_pic_buf[j].rff = dec_cont->Hdrs.repeat_first_field;
  if (!buffer_b)
    dec_cont->StrmStorage.p_pic_buf[j].picture_distance =
      dec_cont->Hdrs.picture_distance;

  if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE)
    dec_cont->StrmStorage.p_pic_buf[j].nbr_err_mbs = nbr_err_mbs / 2;
  else
    dec_cont->StrmStorage.p_pic_buf[j].nbr_err_mbs = nbr_err_mbs;

  AvsDecTimeCode(dec_cont, &dec_cont->StrmStorage.p_pic_buf[j].time_code);

  dec_cont->StrmStorage.out_count++;

  dec_cont->fullness = dec_cont->StrmStorage.out_count;

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsFreeBuffers

        Purpose:

        Input:          DecContainer *dec_cont

        Output:

------------------------------------------------------------------------------*/
void AvsFreeBuffers(DecContainer * dec_cont) {
  u32 i;

  BqueueRelease2( &dec_cont->StrmStorage.bq );

  /* Reference images */
  if (dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      if(dec_cont->StrmStorage.p_pic_buf[i].data.virtual_address != NULL) {
        DWLFreeRefFrm(dec_cont->dwl,
                      &dec_cont->StrmStorage.p_pic_buf[i].data);
        dec_cont->StrmStorage.p_pic_buf[i].data.virtual_address = NULL;
        dec_cont->StrmStorage.p_pic_buf[i].data.bus_address = 0;
      }
    }
  }
  if (dec_cont->StrmStorage.direct_mvs.virtual_address != NULL)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.direct_mvs);

  dec_cont->StrmStorage.direct_mvs.virtual_address = NULL;

}
