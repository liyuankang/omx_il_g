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

#include "mp4dechwd_shortvideo.h"
#include "mp4dechwd_utils.h"
#include "mp4dechwd_motiontexture.h"
#include "mp4debug.h"

#define MP4DEC_UNSUPPORTED \
{ \
    dec_cont->StrmStorage.unsupported_features_present = HANTRO_TRUE; \
    return (HANTRO_NOK); \
}

/*------------------------------------------------------------------------------
    2. External identifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Module indentifiers
------------------------------------------------------------------------------*/

u32 StrmDec_DecodeGobHeader(DecContainer * dec_cont);
static u32 StrmDec_DecodeSVHPlusHeader(DecContainer * dec_cont);

/*------------------------------------------------------------------------------

   5.1  Function name: StrmDec_DecodeGobLayer

        Purpose: decode Group Of Blocks (GOB) layer

        Input:
            Pointer to DecContainer structure
                -uses and updates StrmDesc
                -uses and updates SvDesc

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

u32 StrmDec_DecodeGobLayer(DecContainer * dec_cont) {
  u32 tmp;
  u32 mb_number, mb_counter;
  u32 num_mbs;
  u32 status = HANTRO_OK;

  /* gob header */
  if(dec_cont->StrmStorage.gob_resync_flag == HANTRO_TRUE) {
    status = StrmDec_DecodeGobHeader(dec_cont);
    if(status != HANTRO_OK)
      return (status);
    dec_cont->StrmStorage.vp_first_coded_mb =
      dec_cont->StrmStorage.vp_mb_number;
  }

  if(dec_cont->SvDesc.num_mbs_in_gob) {
    num_mbs = dec_cont->SvDesc.num_mbs_in_gob;
  } else {
    num_mbs = dec_cont->VopDesc.vop_width;
  }

  mb_number = dec_cont->StrmStorage.vp_mb_number;

  mb_counter = 0;
  while(1) {
    status = StrmDec_DecodeMb(dec_cont, mb_number);
    if(status != HANTRO_OK)
      return (status);
    if(!MB_IS_STUFFING(mb_number)) {
      mb_number++;
      mb_counter++;
      if(mb_counter == num_mbs) {
        tmp = 9 + (u32)(dec_cont->VopDesc.vop_coding_type == PVOP);
        while(StrmDec_ShowBits(dec_cont, tmp) == 0x1)
          (void) StrmDec_FlushBits(dec_cont, tmp);
        break;
      }
    }
  }

  /* stuffing */
  if(dec_cont->StrmDesc.bit_pos_in_word) {
    /* there is stuffing if next byte aligned bits are resync marker or
     * stream ends at next byte aligned position */
    if((StrmDec_ShowBitsAligned(dec_cont, 17, 1) == SC_RESYNC) ||
        (((dec_cont->StrmDesc.strm_buff_read_bits >> 3) + 1) ==
         dec_cont->StrmDesc.strm_buff_size)) {
      tmp = StrmDec_GetBits(dec_cont,
                            8 - dec_cont->StrmDesc.bit_pos_in_word);

      if(tmp != 0) {
        return (HANTRO_NOK);
      }
    }
  }

  /* last gob of vop -> check if short video end code. Read if yes and
   * check that there is stuffing and either end of stream or short video
   * start */
  if(mb_number == dec_cont->VopDesc.total_mb_in_vop) {
    tmp = StrmDec_ShowBits(dec_cont, 22);
    if(tmp == SC_SV_END) {
      (void) StrmDec_FlushBits(dec_cont, 22);
    }
    /* stuffing */
    if(dec_cont->StrmDesc.bit_pos_in_word) {
      tmp = StrmDec_GetBits(dec_cont,
                            8 - dec_cont->StrmDesc.bit_pos_in_word);
      if(tmp != 0) {
        return (HANTRO_NOK);
      }
    }

    /* there might be extra stuffing byte if next start code is video
     * object sequence start or end code. If this is the case the
     * stuffing is normal mpeg4 stuffing. */
    tmp = StrmDec_ShowBitsAligned(dec_cont, 32, 1);
    if((tmp == SC_VOS_START) || (tmp == SC_VOS_END) ||
        (tmp == 0 && StrmDec_ShowBits(dec_cont, 8) == 0x7F) ) {
      tmp = StrmDec_GetStuffing(dec_cont);
      if(tmp != HANTRO_OK)
        return (tmp);
    }

    /* JanSa: modified to handle extra zeros after VOP */
    tmp = StrmDec_ShowBits(dec_cont,24);
    if ( !tmp ) {
      do {
        if (StrmDec_FlushBits(dec_cont, 8) == END_OF_STREAM)
          break;
        tmp = StrmDec_ShowBits(dec_cont,24);
      } while (!(tmp));
    }

    /* check that there is either end of stream or short video start or
     * at least 23 zeros in the stream */
    tmp = StrmDec_ShowBits(dec_cont, 23);
    if(!IS_END_OF_STREAM(dec_cont) && ((tmp >> 6) != SC_RESYNC) && tmp) {
      return (HANTRO_NOK);
    }
  }

  /* whole video packet decoded and stuffing ok (if stuffed) -> set
   * vp_mb_number in StrmStorage so that this gob layer won't
   * be touched/concealed anymore. Also set VpQP to QP so that concealment
   * will use qp of last decoded macro block. */
  dec_cont->StrmStorage.vp_mb_number = mb_number;
  dec_cont->StrmStorage.vp_qp = dec_cont->StrmStorage.q_p;

  dec_cont->StrmStorage.vp_num_mbs = 0;

  /* store pointer to rlc data buffer for concealment purposes */
  dec_cont->MbSetDesc.p_rlc_data_vp_addr =
    dec_cont->MbSetDesc.p_rlc_data_curr_addr;

  return (status);

}

/*------------------------------------------------------------------------------

   5.2  Function name: StrmDec_DecodeShortVideo

        Purpose: decode VOP with short video header

        Input:
            Pointer to DecContainer structure
                -uses and updates StrmDesc

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

u32 StrmDec_DecodeShortVideo(DecContainer * dec_cont) {
  u32 status = HANTRO_OK;

  status = StrmDec_DecodeShortVideoHeader(dec_cont);
  /* return if status not ok or stream decoder is not ready (first time here
   * and function just read the source format and left the stream as it
   * was) */
  if((status != HANTRO_OK) ||
      (dec_cont->StrmStorage.strm_dec_ready == HANTRO_FALSE)) {
    return (status);
  }

  status = StrmDec_DecodeGobLayer(dec_cont);

  return (status);
}

/*------------------------------------------------------------------------------

   5.3  Function name: StrmDec_DecodeShortVideoHeader

        Purpose: decode short video header

        Input:
            Pointer to DecContainer structure
                -uses and updates StrmDesc
                -uses and updates SvDesc

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

u32 StrmDec_DecodeShortVideoHeader(DecContainer * dec_cont) {
  u32 tmp, status = 0;
  u32 plus_header = 0;

  if(dec_cont->StrmStorage.vp_mb_number) {
    (void) StrmDec_UnFlushBits(dec_cont, 22);
    dec_cont->StrmStorage.p_last_sync =
      dec_cont->StrmDesc.strm_curr_pos;
    return (HANTRO_NOK);
  }
  dec_cont->StrmStorage.valid_vop_header = HANTRO_FALSE;

  dec_cont->Hdrs.low_delay = HANTRO_TRUE;
  /* initialize short video header stuff if first VOP */
  if(dec_cont->StrmStorage.short_video == HANTRO_FALSE) {
    dec_cont->StrmStorage.resync_marker_length = 17;
    dec_cont->StrmStorage.short_video = HANTRO_TRUE;

    dec_cont->VopDesc.vop_rounding_type = 0;
    dec_cont->VopDesc.fcode_fwd = 1;
    dec_cont->VopDesc.intra_dc_vlc_thr = 0;
    dec_cont->VopDesc.vop_coded = 1;

    dec_cont->SvDesc.gob_frame_id = 0;
    dec_cont->SvDesc.temporal_reference = 0;
    dec_cont->SvDesc.tics = 0;

    dec_cont->Hdrs.vop_time_increment_resolution = 30000;
    dec_cont->Hdrs.data_partitioned = HANTRO_FALSE;
    dec_cont->Hdrs.resync_marker_disable = HANTRO_FALSE;

    dec_cont->Hdrs.colour_primaries = 1;
    dec_cont->Hdrs.transfer_characteristics = 1;
    dec_cont->Hdrs.matrix_coefficients = 6;
  }

  if(!dec_cont->SvDesc.source_format) {
    /* vop size not known yet -> read and return.
     * source format is bits 14-16 from current position */
    tmp = StrmDec_ShowBits(dec_cont, 16) & 0x7;

    switch (tmp) {
    case 1:    /* sub-QCIF */
      dec_cont->VopDesc.vop_width = 8;
      dec_cont->VopDesc.vop_height = 6;
      dec_cont->VopDesc.total_mb_in_vop = 48;
      break;

    case 2:    /* QCIF */
      dec_cont->VopDesc.vop_width = 11;
      dec_cont->VopDesc.vop_height = 9;
      dec_cont->VopDesc.total_mb_in_vop = 99;
      break;

    case 3:    /* CIF */
      dec_cont->VopDesc.vop_width = 22;
      dec_cont->VopDesc.vop_height = 18;
      dec_cont->VopDesc.total_mb_in_vop = 396;
      break;

    case 4:    /* 4CIF */
      dec_cont->VopDesc.vop_width = 44;
      dec_cont->VopDesc.vop_height = 36;
      dec_cont->VopDesc.total_mb_in_vop = 1584;
      break;

    case 7:    /* H.263 */
      plus_header = 1;
      /* go to start of plus header */
      status = StrmDec_GetBits(dec_cont, 16);
      if(status == END_OF_STREAM)
        return (HANTRO_NOK);
      status = StrmDec_DecodeSVHPlusHeader(dec_cont);
      if (status != HANTRO_OK || !dec_cont->VopDesc.total_mb_in_vop) {
        dec_cont->VopDesc.vop_width = 0;
        dec_cont->VopDesc.vop_height = 0;
        dec_cont->VopDesc.total_mb_in_vop = 0;
        dec_cont->SvDesc.source_format = 0;
        return(status);
      }
      (void) StrmDec_UnFlushBits(dec_cont, 100);

      break;

    default:
      dec_cont->SvDesc.source_format = 0;
      MP4DEC_UNSUPPORTED;
    }
    dec_cont->SvDesc.source_format = tmp;
    /* return start marker into stream */
    (void) StrmDec_UnFlushBits(dec_cont, 22);
    dec_cont->Hdrs.last_header_type = SC_SV_START;

    if(!plus_header) {
      dec_cont->Hdrs.video_object_layer_width =
        dec_cont->VopDesc.vop_width * 16;
      dec_cont->Hdrs.video_object_layer_height =
        dec_cont->VopDesc.vop_height * 16;
    }
    return (HANTRO_OK);
  }

  /* temporal reference. Note that arithmetics are performed only with
   * eight LSBs */
  tmp = StrmDec_GetBits(dec_cont, 8);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  dec_cont->SvDesc.temporal_reference = tmp;

  /* time increment without etr */
  if (!dec_cont->SvDesc.cpcf) {
    dec_cont->VopDesc.tics_from_prev = (tmp + 256 -
                                            (dec_cont->SvDesc.
                                                tics & 0xFF)) & 0xFF;
    dec_cont->SvDesc.tics += dec_cont->VopDesc.tics_from_prev;
  }

  tmp = StrmDec_GetBits(dec_cont, 1);    /* marker */
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
  if(tmp == 0)
    MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */
  tmp = StrmDec_GetBits(dec_cont, 1);    /* zero */
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
  if(tmp)
    MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */
  dec_cont->SvDesc.split_screen_indicator =
    StrmDec_GetBits(dec_cont, 1);
  if(dec_cont->SvDesc.split_screen_indicator == END_OF_STREAM)
    return (END_OF_STREAM);
  dec_cont->SvDesc.document_camera_indicator =
    StrmDec_GetBits(dec_cont, 1);
  if(dec_cont->SvDesc.document_camera_indicator == END_OF_STREAM)
    return (END_OF_STREAM);
  dec_cont->SvDesc.full_picture_freeze_release =
    StrmDec_GetBits(dec_cont, 1);
  if(dec_cont->SvDesc.full_picture_freeze_release == END_OF_STREAM)
    return (END_OF_STREAM);

  tmp = StrmDec_GetBits(dec_cont, 3);    /* source_format */
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  if(tmp != dec_cont->SvDesc.source_format) {
    MP4DEC_UNSUPPORTED;
  }

  /* If source format is 7, plus header will be decoded which is h.263
   * baseline compatible. Otherwise stream is MPEG-4 shortvideo and it
   * will be decoded without plus header. */
  if(dec_cont->SvDesc.source_format != 7) {
    dec_cont->VopDesc.vop_coding_type =
      StrmDec_GetBits(dec_cont, 1);
    if(dec_cont->VopDesc.vop_coding_type == END_OF_STREAM)
      return (END_OF_STREAM);
    tmp = StrmDec_GetBits(dec_cont, 4);    /* 4 zero */
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
    if(tmp)
#else
    if(tmp && tmp != 8) /* Allow unrestricted mvs although 100% compatibility/
                             * conformity isn't necessarily achieved.. */
#endif /* HANTRO_PEDANTIC_MODE */
      MP4DEC_UNSUPPORTED;

    dec_cont->VopDesc.q_p = StrmDec_GetBits(dec_cont, 5);
    if(dec_cont->VopDesc.q_p == END_OF_STREAM)
      return (END_OF_STREAM);
    if(dec_cont->VopDesc.q_p == 0)
      MP4DEC_UNSUPPORTED;

    dec_cont->StrmStorage.q_p = dec_cont->VopDesc.q_p;

    tmp = StrmDec_GetBits(dec_cont, 1);    /* zero */
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

  } else {
    tmp = StrmDec_DecodeSVHPlusHeader(dec_cont);
    if(tmp != HANTRO_OK)
      return (tmp);
    dec_cont->StrmStorage.q_p = dec_cont->VopDesc.q_p;
  }

  /* time increment with etr */
  if (dec_cont->SvDesc.cpcf) {
    dec_cont->SvDesc.temporal_reference |=
      ((dec_cont->SvDesc.etr & 0x3) << 8);
    tmp = dec_cont->SvDesc.temporal_reference;
    dec_cont->VopDesc.tics_from_prev = (tmp + 1024 -
                                            (dec_cont->SvDesc.
                                                tics & 0x3FF)) & 0x3FF;
    dec_cont->SvDesc.tics += dec_cont->VopDesc.tics_from_prev;
  }

  do {
    tmp = StrmDec_GetBits(dec_cont, 1);    /* pei */
    if(tmp == 1) {
      (void) StrmDec_FlushBits(dec_cont, 8); /* psupp */
    }
  } while(tmp == 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  /* no resync marker in the beginning of first gob */
  dec_cont->StrmStorage.gob_resync_flag = HANTRO_FALSE;

  dec_cont->StrmStorage.vp_mb_number = 0;
  dec_cont->StrmStorage.vp_first_coded_mb = 0;

  /* successful decoding -> set valid vop header */
  dec_cont->StrmStorage.valid_vop_header = HANTRO_TRUE;


  return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

   5.4  Function name: StrmDec_CheckNextGobNumber

        Purpose:

        Input:
            Pointer to DecContainer structure
                -uses StrmDesc
                -uses SvDesc

        Output:
            GOB number
            0 if non-valid gob number found in stream

------------------------------------------------------------------------------*/

u32 StrmDec_CheckNextGobNumber(DecContainer * dec_cont) {
  u32 tmp;

  tmp = StrmDec_ShowBits(dec_cont, 5);
  if(tmp >= dec_cont->VopDesc.vop_height) {
    tmp = 0;
  }
  return (tmp);

}

/*------------------------------------------------------------------------------

   5.3  Function name: StrmDec_DecodeGobHeader

        Purpose: decode Group Of Blocks (GOB) header

        Input:
            Pointer to DecContainer structure

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

u32 StrmDec_DecodeGobHeader(DecContainer * dec_cont) {
  u32 tmp, tmp2;

  tmp = StrmDec_GetBits(dec_cont, 5);    /* gob_number */
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  /* gob_number should be between (0,vop_height) */
  if(!tmp || (tmp >= dec_cont->VopDesc.vop_height))
    MP4DEC_UNSUPPORTED;

  /* store old gob frame id */
  tmp = dec_cont->SvDesc.gob_frame_id;

  tmp2 = StrmDec_GetBits(dec_cont, 2);   /* gob frame id */
  if(tmp2 == END_OF_STREAM)
    return (END_OF_STREAM);

  dec_cont->SvDesc.gob_frame_id = tmp2;

  /* sv vop header lost -> check if gob_frame_id indicates that
   * old source_format etc. are invalid and get away if this is the
   * case */
  if((dec_cont->StrmStorage.valid_vop_header == HANTRO_FALSE) &&
      (dec_cont->SvDesc.gob_frame_id != tmp)) {
    MP4DEC_UNSUPPORTED;
  }

  /* QP */
  tmp = StrmDec_GetBits(dec_cont, 5);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  if(tmp == 0)
    MP4DEC_UNSUPPORTED;

  dec_cont->StrmStorage.q_p = tmp;
  dec_cont->StrmStorage.prev_qp = tmp;
  dec_cont->StrmStorage.vp_qp = dec_cont->StrmStorage.q_p;

  dec_cont->StrmStorage.valid_vop_header = HANTRO_TRUE;

  return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

   5.3  Function name: StrmDec_DecodeSVHPlusHeader

        Purpose: decode short video plus header

        Input:
            Pointer to DecContainer structure
                -uses and updates StrmDesc
                -uses and updates SvDesc

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

static u32 StrmDec_DecodeSVHPlusHeader(DecContainer * dec_cont) {

  u32 tmp;
  u32 ufep = 0;
  u32 custom_width = 0, custom_height = 0;
  u32 custom_source_format = 0;

  MP4DEC_DEBUG(("SVHPLUSHEADERDECODE\n"));

  ufep = StrmDec_GetBits(dec_cont, 3);   /* update full extended ptype */

  if(END_OF_STREAM == ufep)
    return (END_OF_STREAM);
  /* if ufep == 1, opptype will be decoded */
  if(ufep == 1) {
    /* source format */
    tmp = StrmDec_GetBits(dec_cont, 3);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp == 0 || tmp == 7)
      MP4DEC_UNSUPPORTED;

    dec_cont->SvDesc.source_format = tmp;

    switch (dec_cont->SvDesc.source_format) {

    case 1:    /* sub-QCIF */

      dec_cont->VopDesc.vop_width = 8;
      dec_cont->VopDesc.vop_height = 6;
      dec_cont->VopDesc.total_mb_in_vop = 48;
      break;

    case 2:    /* QCIF */
      dec_cont->VopDesc.vop_width = 11;
      dec_cont->VopDesc.vop_height = 9;
      dec_cont->VopDesc.total_mb_in_vop = 99;
      break;

    case 3:    /* CIF */
      dec_cont->VopDesc.vop_width = 22;
      dec_cont->VopDesc.vop_height = 18;
      dec_cont->VopDesc.total_mb_in_vop = 396;
      break;

    case 4:    /* 4CIF */
      dec_cont->VopDesc.vop_width = 44;
      dec_cont->VopDesc.vop_height = 36;
      dec_cont->VopDesc.total_mb_in_vop = 1584;
      break;

    case 6:    /* Custom source format */
      custom_source_format = 1;
      break;
    default:
      /* MPEG4 shortvideo type source format is still 7 */
      dec_cont->SvDesc.source_format = 7;
      MP4DEC_UNSUPPORTED;
    }
    /* MPEG4 shortvideo type source format is still 7 */
    dec_cont->SvDesc.source_format = 7;

    if(!custom_source_format) {
      dec_cont->Hdrs.video_object_layer_width =
        dec_cont->VopDesc.vop_width * 16;
      dec_cont->Hdrs.video_object_layer_height =
        dec_cont->VopDesc.vop_height * 16;
    }

    /* custom picture clock freq. disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    dec_cont->SvDesc.cpcf = tmp;

    /* unrestricted motion vector mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Syntax based arithmetic coding disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Advanced prediction mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Advanced intra coding mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Deblocking filter mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Slice structured mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Reference picture selection disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Independent segment decoding mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Alternative inter vlc disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* Modified quantization mode disab/enab */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
    if(tmp)
      MP4DEC_UNSUPPORTED;

    /* marker */
    tmp = StrmDec_GetBits(dec_cont, 1);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
    if(!tmp)
      MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */

    /*reserved (zero) bits */
    tmp = StrmDec_GetBits(dec_cont, 3);
    if(tmp == END_OF_STREAM)
      return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
    if(tmp)
      MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */
  } else if(ufep > 1) {
    MP4DEC_UNSUPPORTED;
  }
  /* mpptype is decoded anyway regardless of value of ufep */
  /* picture coding type */
  tmp = StrmDec_GetBits(dec_cont, 3);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  if(tmp > 1 || (!tmp && !ufep))
    MP4DEC_UNSUPPORTED;

  dec_cont->VopDesc.vop_coding_type = tmp;

  /* Reference picture resampling disab/enab */
  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  if(tmp)
    MP4DEC_UNSUPPORTED;

  /* Reduced-resolution update mode disab/enab */
  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  if(tmp)
    MP4DEC_UNSUPPORTED;

  /* Rounding type */
  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  dec_cont->VopDesc.vop_rounding_type = tmp;

  /* reserved (zero) bits */
  tmp = StrmDec_GetBits(dec_cont, 2);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
  if(tmp)
    MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */

  /* marker */
  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
  if(!tmp)
    MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */

  /* Plus header decoding continues after plustype */
  /* Picture header location of CPM */
  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
  if(tmp)
    MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */

  if(ufep == 1) {
    u32 epar = 0;
    if(custom_source_format) {
      /* Custom Picture Format */
      /* Pixel aspect ratio */
      tmp = StrmDec_GetBits(dec_cont, 4);
      if(tmp == END_OF_STREAM)
        return (END_OF_STREAM);
      if (tmp == 15) epar = 1; /* extended PAR */
      dec_cont->Hdrs.aspect_ratio_info = tmp;

      /* Picture width indication */
      tmp = StrmDec_GetBits(dec_cont, 9);
      custom_width = (tmp + 1) * 4;

      if(tmp == END_OF_STREAM)
        return (END_OF_STREAM);
      dec_cont->VopDesc.vop_width = (custom_width + 15) / 16;
      dec_cont->Hdrs.video_object_layer_width = custom_width;

      /* marker */
      tmp = StrmDec_GetBits(dec_cont, 1);
      if(tmp == END_OF_STREAM)
        return (END_OF_STREAM);
#ifdef HANTRO_PEDANTIC_MODE
      if(!tmp)
        MP4DEC_UNSUPPORTED;
#endif /* HANTRO_PEDANTIC_MODE */

      /* Picture height indication */
      tmp = StrmDec_GetBits(dec_cont, 9);
      custom_height = tmp * 4;

      if(tmp == END_OF_STREAM)
        return (END_OF_STREAM);
      if(!tmp)
        MP4DEC_UNSUPPORTED;
      dec_cont->VopDesc.vop_height = (custom_height + 15) / 16;
      dec_cont->Hdrs.video_object_layer_height = custom_height;

      dec_cont->VopDesc.total_mb_in_vop =
        dec_cont->VopDesc.vop_width *
        dec_cont->VopDesc.vop_height;
    }

    if (epar) { /* extended PAR */
      /* par width */
      tmp = dec_cont->Hdrs.par_width =
              StrmDec_GetBits(dec_cont, 8);
      /* par height */
      tmp = dec_cont->Hdrs.par_height =
              StrmDec_GetBits(dec_cont, 8);
    }

    /* custom picture clock frequency used, read code */
    if (dec_cont->SvDesc.cpcf)
      tmp = dec_cont->SvDesc.cpcfc =
              StrmDec_GetBits(dec_cont, 8);
  }

  /* extended temporal reference if custom picture clock frequency */
  if (dec_cont->SvDesc.cpcf)
    tmp = dec_cont->SvDesc.etr = StrmDec_GetBits(dec_cont, 2);

  /* QP */
  dec_cont->VopDesc.q_p = StrmDec_GetBits(dec_cont, 5);
  MP4DEC_DEBUG(("dec_cont->VopDesc.q_p %d\n",
                dec_cont->VopDesc.q_p));
  if(dec_cont->VopDesc.q_p == END_OF_STREAM)
    return (END_OF_STREAM);
  if(dec_cont->VopDesc.q_p == 0)
    MP4DEC_UNSUPPORTED;

  /* Number of GOBs in VOP */
  if(custom_height <= 400) {
    dec_cont->SvDesc.num_mbs_in_gob = custom_width / 16;
    if(custom_width & (15)) {
      dec_cont->SvDesc.num_mbs_in_gob++;
    }

    dec_cont->SvDesc.num_gobs_in_vop = custom_height / 16;
    if(custom_height & (15)) {
      dec_cont->SvDesc.num_gobs_in_vop++;
    }
  } else {
    dec_cont->SvDesc.num_mbs_in_gob = custom_width / 16;
    if(custom_width & (15)) {
      dec_cont->SvDesc.num_mbs_in_gob++;
    }
    dec_cont->SvDesc.num_mbs_in_gob *= 2;

    dec_cont->SvDesc.num_gobs_in_vop = custom_height / 32;
    if(custom_height & (31)) {
      dec_cont->SvDesc.num_gobs_in_vop++;
    }
  }
  return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

   Function name: StrmDec_DecodeSorensonSparkHeader

        Purpose:

        Input:
            Pointer to DecContainer structure
                -uses and updates StrmDesc
                -uses and updates SvDesc

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/

u32 StrmDec_DecodeSorensonSparkHeader(DecContainer * dec_cont) {
  u32 tmp = 0;
  u32 source_format;

  dec_cont->StrmStorage.valid_vop_header = HANTRO_FALSE;

  /* initialize short video header stuff if first VOP */
  if(dec_cont->StrmStorage.short_video == HANTRO_FALSE) {
    dec_cont->StrmStorage.resync_marker_length = 17;
    dec_cont->StrmStorage.short_video = HANTRO_TRUE;

    dec_cont->VopDesc.vop_rounding_type = 0;
    dec_cont->VopDesc.fcode_fwd = 1;
    dec_cont->VopDesc.intra_dc_vlc_thr = 0;
    dec_cont->VopDesc.vop_coded = 1;

    dec_cont->SvDesc.gob_frame_id = 0;
    dec_cont->SvDesc.temporal_reference = 0;
    dec_cont->SvDesc.tics = 0;

    dec_cont->Hdrs.vop_time_increment_resolution = 30000;
    dec_cont->Hdrs.data_partitioned = HANTRO_FALSE;
    dec_cont->Hdrs.resync_marker_disable = HANTRO_FALSE;

    dec_cont->Hdrs.colour_primaries = 1;
    dec_cont->Hdrs.transfer_characteristics = 1;
    dec_cont->Hdrs.matrix_coefficients = 6;
  }

  if(!dec_cont->StrmStorage.strm_dec_ready) {
    /* vop size not known yet -> read and return.
     * source format is bits 14-16 from current position */
    source_format = StrmDec_ShowBits(dec_cont, 12) & 0x7;

    switch (source_format) {
    /* TODO: todellisen kuvan koko talteen!! */
    case 0:
      tmp = StrmDec_ShowBitsAligned(dec_cont, 17, 2) & 0xFFFF;
      dec_cont->VopDesc.vop_width = ((tmp>>8)+15)/16;
      dec_cont->Hdrs.video_object_layer_width = (tmp>>8);
      dec_cont->VopDesc.vop_height = ((tmp&0xFF)+15)/16;
      dec_cont->Hdrs.video_object_layer_height = (tmp&0xFF);
      dec_cont->VopDesc.total_mb_in_vop =
        dec_cont->VopDesc.vop_width *
        dec_cont->VopDesc.vop_height;
      break;

    case 1:
      tmp = StrmDec_ShowBitsAligned(dec_cont, 17, 2) & 0xFFFF;
      dec_cont->VopDesc.vop_width = (tmp+15)/16;
      dec_cont->Hdrs.video_object_layer_width = tmp;
      tmp = StrmDec_ShowBitsAligned(dec_cont, 17, 4) & 0xFFFF;
      dec_cont->VopDesc.vop_height = (tmp+15)/16;
      dec_cont->Hdrs.video_object_layer_height = tmp;
      dec_cont->VopDesc.total_mb_in_vop =
        dec_cont->VopDesc.vop_width *
        dec_cont->VopDesc.vop_height;
      break;

    case 2:    /* CIF */
      dec_cont->VopDesc.vop_width = 22;
      dec_cont->VopDesc.vop_height = 18;
      dec_cont->VopDesc.total_mb_in_vop = 396;
      break;

    case 3:    /* QCIF */
      dec_cont->VopDesc.vop_width = 11;
      dec_cont->VopDesc.vop_height = 9;
      dec_cont->VopDesc.total_mb_in_vop = 99;
      break;

    case 4:    /* sub-QCIF */
      dec_cont->VopDesc.vop_width = 8;
      dec_cont->VopDesc.vop_height = 6;
      dec_cont->VopDesc.total_mb_in_vop = 48;
      break;

    case 5:    /* QVGA */
      dec_cont->VopDesc.vop_width = 20;
      dec_cont->VopDesc.vop_height = 15;
      dec_cont->VopDesc.total_mb_in_vop = 300;
      break;

    case 6:    /* QQVGA */
      dec_cont->VopDesc.vop_width = 10;
      dec_cont->VopDesc.vop_height = 8;
      dec_cont->VopDesc.total_mb_in_vop = 80;
      break;

    default:    /* reserved */
      dec_cont->SvDesc.source_format = 0;
      MP4DEC_UNSUPPORTED;
    }
    if (source_format > 1) {
      dec_cont->Hdrs.video_object_layer_width =
        16*dec_cont->VopDesc.vop_width;
      dec_cont->Hdrs.video_object_layer_height =
        16*dec_cont->VopDesc.vop_height;
    }
    dec_cont->SvDesc.source_format = source_format;
    /* return start marker into stream */
    (void) StrmDec_UnFlushBits(dec_cont, 22);
    dec_cont->Hdrs.last_header_type = SC_SV_START;
    return (HANTRO_OK);
  }

  tmp = StrmDec_GetBits(dec_cont, 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  dec_cont->StrmStorage.sorenson_ver = tmp;

  /* temporal reference. Note that arithmetics are performed only with
   * eight LSBs */
  tmp = StrmDec_GetBits(dec_cont, 8);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  dec_cont->SvDesc.temporal_reference = tmp;

  dec_cont->VopDesc.tics_from_prev = (tmp + 256 -
                                          (dec_cont->SvDesc.
                                              tics & 0xFF)) & 0xFF;
  dec_cont->SvDesc.tics += dec_cont->VopDesc.tics_from_prev;

  tmp = StrmDec_GetBits(dec_cont, 3);    /* source_format */
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  if(tmp != dec_cont->SvDesc.source_format)
    MP4DEC_UNSUPPORTED;

  if (dec_cont->SvDesc.source_format == 0 ||
      dec_cont->SvDesc.source_format == 1) {
    tmp = StrmDec_GetBits(dec_cont,
                          8*(dec_cont->SvDesc.source_format+1)); /* width */
    tmp = StrmDec_GetBits(dec_cont,
                          8*(dec_cont->SvDesc.source_format+1)); /* height */
  }

  tmp = StrmDec_GetBits(dec_cont, 2);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);
  if(tmp > 2)
    MP4DEC_UNSUPPORTED;
  if (tmp == 2) { /* disposable */
    dec_cont->StrmStorage.disposable = 1;
    tmp = 1;
  } else
    dec_cont->StrmStorage.disposable = 0;
  dec_cont->VopDesc.vop_coding_type = tmp;

  tmp = StrmDec_GetBits(dec_cont, 1); /* deblocking filter */

  dec_cont->VopDesc.q_p = StrmDec_GetBits(dec_cont, 5);
  if(dec_cont->VopDesc.q_p == END_OF_STREAM)
    return (END_OF_STREAM);
  if(dec_cont->VopDesc.q_p == 0)
    MP4DEC_UNSUPPORTED;

  dec_cont->StrmStorage.q_p = dec_cont->VopDesc.q_p;

  do {
    tmp = StrmDec_GetBits(dec_cont, 1);    /* pei */
    if(tmp == 1) {
      (void) StrmDec_FlushBits(dec_cont, 8); /* psupp */
    }
  } while(tmp == 1);
  if(tmp == END_OF_STREAM)
    return (END_OF_STREAM);

  /* no resync marker in the beginning of first gob */
  dec_cont->StrmStorage.gob_resync_flag = HANTRO_FALSE;

  dec_cont->StrmStorage.vp_mb_number = 0;
  dec_cont->StrmStorage.vp_first_coded_mb = 0;

  /* successful decoding -> set valid vop header */
  dec_cont->StrmStorage.valid_vop_header = HANTRO_TRUE;

  return (HANTRO_OK);

}

