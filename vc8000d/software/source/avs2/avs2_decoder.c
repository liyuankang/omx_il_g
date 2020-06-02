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

#include "avs2_container.h"
#include "avs2_decoder.h"
//#include "avs2_byte_stream.h"
//#include "avs2_seq_param_set.h"
//#include "avs2_pic_param_set.h"
//#include "avs2_util.h"
#include "avs2_dpb.h"
#include "wquant.h"
//#include "avs2_exp_golomb.h"

u32 Avs2ExtractStream(const u8 *byte_stream, u32 strm_len, const u8 *strm_buf,
                      u32 buf_len, struct StrmData *stream, u32 *read_bytes,
                      u32 *start_code_detected);
u32 Avs2NextStartCode(struct StrmData *stream);

static void Avs2AlignStream(struct StrmData *stream, struct StrmData *strm) {
#if 1
  strm->strm_buff_start = (u8 *)stream->strm_buff_start;
  strm->strm_data_size = stream->strm_data_size;
  strm->strm_buff_read_bits = ((stream->strm_buff_read_bits + 7) / 8) * 8;
  strm->strm_curr_pos = strm->strm_buff_start + strm->strm_buff_read_bits / 8;
  strm->bit_pos_in_word = 0;
#endif
}

/* Initializes the decoder instance. */
void Avs2Init(struct Avs2DecContainer *dec_cont, u32 no_output_reordering) {

  /* Variables */
  struct Avs2Storage *storage = &dec_cont->storage;

  /* Code */

  Avs2HwdInit(&dec_cont->hwdec, dec_cont->dwl);

  Avs2InitStorage(storage);

  storage->no_reordering = no_output_reordering;
  storage->poc_last_display = INIT_POC;
}

/* check if pic needs to be skipped due to random access */
u32 Avs2SkipPicture(struct Avs2Storage *storage) {

#if 0
  if ((nal_unit->nal_unit_type == NAL_CODED_SLICE_RASL_N ||
       nal_unit->nal_unit_type == NAL_CODED_SLICE_RASL_R) &&
      storage->poc->pic_order_cnt < storage->poc_last_display) {
    return HANTRO_TRUE;
  }
  /* CRA and not start of sequence -> all pictures may be output */
  if (storage->poc_last_display != INIT_POC &&
      nal_unit->nal_unit_type == NAL_CODED_SLICE_CRA)
    storage->poc_last_display = -INIT_POC;
  else if (IS_RAP_NAL_UNIT(nal_unit))
    storage->poc_last_display = storage->poc->pic_order_cnt;
#endif

  return HANTRO_FALSE;
}

/**
 * Prepare Picture Decoding processed by HW after a new slice is met.
 */
static u32 Avs2NewSlice(struct Avs2DecContainer *dec_cont, u32 pic_id) {

  struct Avs2Storage *storage = &dec_cont->storage;

  DEBUG_PRINT(("[avs2dec] decode slice header\n"));

  if ((storage->sps.cnt == 0) || (storage->pps.cnt == 0)) {
    /* not valid */
    return AVS2_RDY;
  }

  if (dec_cont->slice_cnt == 0) {
    /* first slice of this picture */
    storage->current_pic_id = pic_id;
    // storage->frame_rate = storage->sps.frame_rate; //avs2GetFrameRate();
    storage->prev_buf_not_finished = HANTRO_TRUE;

    storage->pic_started = HANTRO_FALSE;

    // Adaptive frequency weighting quantization
    if (storage->sps.weight_quant_enable_flag &&
        storage->pps.pic_weight_quant_enable_flag) {
      InitFrameQuantParam(dec_cont);
      FrameUpdateWQMatrix(dec_cont);  // M2148 2007-09
    }
  }

  DEBUG_PRINT(("valid slice TRUE\n"));

#ifdef GET_FREE_BUFFER_NON_BLOCK
  i32 prev_poc = storage->poc[0];
#endif

  /* check if picture shall be skipped (non-decodable picture after
   * random access etc) */
  if (Avs2SkipPicture(storage)) {
    return AVS2_RDY;
  }

  /* move reference setting after abtain the buffer. */
  if (DEC_PARAM_ERROR == Avs2SetRefPics(storage->dpb,
                                        &storage->sps,  // reference list
                                        &storage->pps)) {
    printf("[avs2dec] reference error.\n");
    return AVS2_NONREF_PIC_SKIPPED;
  }

#if 1
  // if (Avs2IsStartOfPicture(storage)) {
  if (dec_cont->slice_cnt == 0) {
    /* try to output even if there's no dpb slot or free buffer. */
    Avs2DpbUpdateOutputList(storage->dpb, &storage->pps);
    /* allocate ond dpb for cuurent image*/
    storage->curr_image->data = Avs2AllocateDpbImage(
        storage->dpb, storage->pps.poc, storage->current_pic_id);

    if (storage->curr_image->data == NULL) {
      if (dec_cont->abort) return AVS2_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else {
        storage->poc[0] = prev_poc;
        return AVS2_NO_FREE_BUFFER;
      }
#endif
    }

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
    {
      i32 bgd = SET_EMPTY_PICTURE_DATA;

      DWLPrivateAreaMemset(storage->curr_image->data->virtual_address, bgd,
                           storage->curr_image->data->size);
    }
#endif

    storage->curr_image->pp_data = storage->dpb->current_out->pp_data;
  }

  storage->valid_slice_in_access_unit = HANTRO_TRUE;
#endif
  return (AVS2_PIC_RDY);
}

/**
 * Continue to decode slices of previous picture.
 */
u32 avs2ContinuePicture(struct Avs2DecContainer *dec_cont, u32 pic_id) {

  u32 ret = 0;
  return ret;
}

/** Decode a stream unit until a slice header. This function calls other modules
 * to perform tasks like
 *   - extract and decode start code unit from the byte stream
 *   - decode parameter sets
 *   - decode slice header and slice data
 *   - conceal errors in the picture
 */
u32 Avs2Decode(struct Avs2DecContainer *dec_cont, const u8 *byte_strm,
               u32 strm_len, u32 pic_id, u32 *read_bytes) {

  u32 tmp, type;
  //u32 access_unit_boundary_flag = HANTRO_FALSE;
  struct Avs2SeqParam sps;
  struct Avs2PicParam pps;
  struct StrmData strm;
  const u8 *strm_buf = dec_cont->hw_buffer;
  u32 buf_len = dec_cont->hw_buffer_length;
  struct Avs2Storage *storage = &dec_cont->storage;

  u32 ret = 0;
  i32 retVal=0;

  DEBUG_PRINT(("HevcDecode\n"));
  ASSERT(dec_cont);
  ASSERT(byte_strm);
  ASSERT(strm_buf);
  ASSERT(strm_len);
  ASSERT(buf_len);
  ASSERT(read_bytes);

  (void)DWLmemset(&sps, 0, sizeof(struct Avs2SeqParam));
  (void)DWLmemset(&pps, 0, sizeof(struct Avs2PicParam));

  DEBUG_PRINT(
      ("Valid slice in access unit %d\n", storage->valid_slice_in_access_unit));

  strm.remove_emul3_byte = 0;
  strm.is_rb = dec_cont->use_ringbuffer;

  /* if previous buffer was not finished and same pointer given -> skip NAL
   * unit extraction */
  if (storage->prev_buf_not_finished &&
      byte_strm == storage->prev_buf_pointer) {
    strm = storage->strm[0];
    *read_bytes = storage->prev_bytes_consumed;
  }

  {
    tmp = Avs2ExtractStream(byte_strm, strm_len, strm_buf, buf_len, &strm,
                            read_bytes, &dec_cont->start_code_detected);
    if (tmp != HANTRO_OK) {
      ERROR_PRINT("BYTE_STREAM");
      return (AVS2_ERROR);
    }
    /* store stream */
    storage->strm[0] = strm;
    storage->prev_bytes_consumed = *read_bytes;
    storage->prev_buf_pointer = byte_strm;
  }

  storage->prev_buf_not_finished = HANTRO_FALSE;

  type = SwGetBits(&strm, 8);
  if (tmp != HANTRO_OK) {
    /* the strange data pattern from demux, repeated 00000001 in input bit
     * stream data */
    struct StrmData tmp_strm;
    tmp_strm = strm;
    tmp_strm.bit_pos_in_word = 0;
    if (SwShowBits(&tmp_strm, 24) == 0x000001) {
      *read_bytes = strm_len;
      return AVS2_ERROR;
    }
    ret = AVS2_ERROR;
    goto NEXT_NAL;
  }

  DEBUG_PRINT(("start code type: %d\n", type));
  // strm.strm_buff_read_bits;

  // b0,b2,b5 not need to demulate
  strm.remove_avs_fake_2bits = 1;
  strm.remove_emul3_byte = 1;
  switch (type) {
    case SEQUENCE_HEADER_CODE:
      dec_cont->storage.sps_old= dec_cont->storage.sps;
      strm.remove_avs_fake_2bits = 0;
      retVal=Avs2ParseSequenceHeader(&strm, &dec_cont->storage.sps);
      Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_SEQ, &dec_cont->storage.sps);
      Avs2AlignStream(&strm, &strm);
      if(dec_cont->storage.sps.cnt==0)
        dec_cont->storage.pps.cnt = 0;
      if(dec_cont->storage.sps.cnt)
        dec_cont->storage.sps.new_sps_flag=1;
      ret = AVS2_RDY;
      break;
    case EXTENSION_START_CODE:
      strm.remove_avs_fake_2bits = 0;
      Avs2ParseExtensionData(&strm, &dec_cont->storage.sps,
                             &dec_cont->storage.pps, &dec_cont->storage.ext);
      Avs2AlignStream(&strm, &strm);
      ret = AVS2_RDY;
      break;
    case USER_DATA_START_CODE:
      strm.remove_avs_fake_2bits = 0;
      Avs2ParseUserData(&strm);
      ret = AVS2_RDY;
      break;
    case VIDEO_EDIT_CODE:
      ret = AVS2_RDY;
      break;
    case I_PICTURE_START_CODE:
      retVal=Avs2ParseIntraPictureHeader(&strm, &storage->sps, &storage->pps);
      retVal=Avs2ParseAlfCoeff(&strm, &storage->sps, &storage->pps,
                        &dec_cont->cmems.alf_tbl);
      if(retVal)
      {
        storage->pps.cnt = 0;
      }
      Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PIC, &dec_cont->storage.pps);
      if(dec_cont->storage.pps.cnt)
      {
        dec_cont->storage.picture_broken = HANTRO_FALSE;
        //printf("dec_cont->storage.picture_broken = %d\n",dec_cont->storage.picture_broken);
      }
      dec_cont->slice_cnt = 0; /* clear slice count */
      // Update POC in DPB calc_picture_distance()
      if(dec_cont->storage.sps.cnt &&
        dec_cont->storage.pps.cnt &&
        dec_cont->storage.sps.new_sps_flag&&
      ((dec_cont->storage.sps.horizontal_size!=dec_cont->storage.sps_old.horizontal_size)||
      ((dec_cont->storage.sps.vertical_size!=dec_cont->storage.sps_old.vertical_size))))
      {
        ret = AVS2_HDRS_RDY;

      }
      else
      {
        ret = AVS2_RDY;
      }
      dec_cont->storage.sps.new_sps_flag = 0;

      break;
    case PB_PICTURE_START_CODE:
      retVal = Avs2ParseInterPictureHeader(&strm, &storage->sps, &storage->pps);
      retVal = Avs2ParseAlfCoeff(&strm, &storage->sps, &storage->pps,
                        &dec_cont->cmems.alf_tbl);
      if(retVal)
      {
        storage->pps.cnt = 0;
      }
      Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PIC, &dec_cont->storage.pps);

      dec_cont->slice_cnt = 0; /* clear slice count */
      // Update POC in DPB calc_picture_distance()
      ret = AVS2_RDY;
      break;
    case SEQUENCE_END_CODE:
      storage->poc_last_display = INIT_POC;
      ret = AVS2_RDY;
      break;

    default:
      if (type >= SLICE_START_CODE_MIN && type <= SLICE_START_CODE_MAX) {
        if ((storage->sps.cnt == 0) || (storage->pps.cnt == 0) ||
            (storage->pps.type == B_IMG && dec_cont->B_discard_flag == 1 &&
             !storage->pps.random_access_decodable_flag)) {
          ret = AVS2_RDY;
        } else {
#if 0
          // update
          if ((temp_slice_buf = (char *) calloc(MAX_CODED_FRAME_SIZE ,
                                                sizeof(char))) == NULL) {       // modified 08.16.2007
              no_mem_exit("GetAnnexbNALU: Buf");
          }

          first_slice_length = length;
          first_slice_startpos = startcodepos;
          memcpy(temp_slice_buf, Buf, length);
          free(Buf);
#endif
          /* a valid slice found, feed to HW  */
          ret = Avs2NewSlice(dec_cont, pic_id);
          *read_bytes = 0;

          if (ret == AVS2_PIC_RDY) {
            if (storage->dpb->no_reordering) {
              /* output this picture next */
              struct Avs2DpbStorage *dpb = storage->dpb;
              struct Avs2DpbOutPicture *dpb_out = &dpb->out_buf[dpb->out_index_w];
              const struct Avs2DpbPicture *current_out = dpb->current_out;

              dpb_out->mem_idx = current_out->mem_idx;
              dpb_out->pp_data = current_out->pp_data;
              dpb_out->data = current_out->data;
              dpb_out->pic_id = current_out->pic_id;
              dpb_out->decode_id = current_out->decode_id;
              dpb_out->num_err_mbs = current_out->num_err_mbs;
              dpb_out->type = current_out->type;
              dpb_out->is_tsa_ref = current_out->is_tsa_ref;
              dpb_out->tiled_mode = current_out->tiled_mode;
              dpb_out->is_field_sequence = current_out->is_field_sequence;
              dpb_out->is_top_field = current_out->is_top_field;
              dpb_out->top_field_first = current_out->top_field_first;

              dpb_out->pic_width = dpb->pic_width;
              dpb_out->pic_height = dpb->pic_height;
              dpb_out->crop_params = dpb->crop_params;
              dpb_out->sample_bit_depth = dpb->sample_bit_depth;
              dpb_out->output_bit_depth = dpb->output_bit_depth;

              dpb->num_out++;
              dpb->out_index_w++;
              if (dpb->out_index_w == MAX_DPB_SIZE + 1) dpb->out_index_w = 0;

              MarkTempOutput(dpb->fb_list, current_out->mem_idx);
            }
            /* success */
            return AVS2_PIC_RDY;
          } else if (ret == AVS2_NONREF_PIC_SKIPPED) {
#ifndef AVS2_INPUT_MULTI_FRM
            strm.strm_curr_pos = strm.strm_buff_start + strm.strm_data_size;
            storage->prev_bytes_consumed = strm.strm_data_size;
            *read_bytes = strm_len;
            return ret;
#endif
            goto NEXT_NAL;

          } else if (ret == AVS2_NO_FREE_BUFFER) {
            strm.strm_curr_pos = strm.strm_buff_start;
            storage->prev_bytes_consumed = 0;
            *read_bytes = 0;
            return ret;
          } else {
            goto NEXT_NAL;
          }
        }
      } else {
        printf("Can't find start code");
        // free(Buf);
        return AVS2_RDY;
      }
  }

NEXT_NAL :

#if 0
  if (Avs2NextStartCode(&strm) == HANTRO_OK) {
    if (strm.strm_curr_pos < byte_strm)
      *read_bytes = (u32)(strm.strm_curr_pos - byte_strm + strm.strm_buff_size);
    else
      *read_bytes = (u32)(strm.strm_curr_pos - byte_strm);
    storage->prev_bytes_consumed = *read_bytes;
  }
  else
#endif
{
  /*END_OF_STREAM*/
  storage->prev_bytes_consumed = *read_bytes = strm_len;
}
#if 0
  if (storage->sei_param.bumping_flag) {
    if (nal_unit.nal_unit_type == NAL_FILLER_DATA)
      storage->sei_param.stream_len += *read_bytes;
  }
#endif

  return ret;
}

/* Shutdown a decoder instance. Function frees all the memories allocated
 * for the decoder instance. */
void Avs2Shutdown(struct Avs2Storage *storage) {

#if 0
  u32 i;

  ASSERT(storage);

  for (i = 0; i < MAX_NUM_SEQ_PARAM_SETS; i++) {
    if (storage->sps[i]) {
      FREE(storage->sps[i]);
    }
  }

  for (i = 0; i < MAX_NUM_PIC_PARAM_SETS; i++) {
    if (storage->pps[i]) {
      FREE(storage->pps[i]);
    }
  }
#endif
}

/* Get next output picture in display order. */
const struct Avs2DpbOutPicture *Avs2NextOutputPicture(
    struct Avs2Storage *storage) {

  const struct Avs2DpbOutPicture *out;

  ASSERT(storage);

  out = Avs2DpbOutputPicture(storage->dpb);

  return out;
}

/* Returns picture width in samples. */
u32 Avs2PicWidth(struct Avs2Storage *storage) {

  ASSERT(storage);

  if (storage->sps.cnt) {
#if 0
    if (storage->raster_enabled)
      return ((storage->active_sps->pic_width + 15) & ~15);
    else
#endif
    return (storage->sps.pic_width_in_cbs * 8);
  } else
    return (0);
}

/* Returns picture height in samples. */
u32 Avs2PicHeight(struct Avs2Storage *storage) {

  ASSERT(storage);

  if (storage->sps.cnt)
    return (storage->sps.pic_height_in_cbs * 8);
  else
    return (0);
}

/* Returns true if chroma format is 4:0:0. */
u32 Avs2IsMonoChrome(struct Avs2Storage *storage) {

  ASSERT(storage);

  return (0);
}

/* Flushes the decoded picture buffer, see dpb.c for details. */
void Avs2FlushBuffer(struct Avs2Storage *storage) {

  ASSERT(storage);

  Avs2FlushDpb(storage->dpb);
}

/* Get value of video_full_range_flag received in the VUI data. */
u32 Avs2VideoRange(struct Avs2Storage *storage) {
#if 0
  const struct SeqParamSet *sps;

  ASSERT(storage);
  sps = storage->active_sps;

  if (sps && sps->vui_parameters_present_flag &&
      sps->vui_parameters.video_signal_type_present_flag &&
      sps->vui_parameters.video_full_range_flag)
    return (1);
  else /* default value of video_full_range_flag is 0 */
    return (0);
#endif
  // FIXME:
  return (0);
}

/* Get value of matrix_coefficients received in the VUI data. */
u32 Avs2MatrixCoefficients(struct Avs2Storage *storage) {
#if 0
  const struct SeqParamSet *sps;

  ASSERT(storage);
  sps = storage->active_sps;

  if (sps && sps->vui_parameters_present_flag &&
      sps->vui_parameters.video_signal_type_present_flag &&
      sps->vui_parameters.colour_description_present_flag)
    return (sps->vui_parameters.matrix_coefficients);
  else /* default unspecified */
    return (2);
#else
  // FIXME:
  return 0;
#endif
}

/* Get cropping parameters of the active SPS. */
void Avs2CroppingParams(struct Avs2Storage *storage, u32 *cropping_flag,
                        u32 *left_offset, u32 *width, u32 *top_offset,
                        u32 *height) {

  const struct Avs2SeqParam *sps;

  ASSERT(storage);
  sps = &storage->sps;

  *left_offset = 0;
  *width = sps->horizontal_size;
  *top_offset = 0;
  *height = sps->vertical_size;

  if ((sps->horizontal_size != sps->pic_width_in_cbs * 8) ||
      (sps->vertical_size != sps->pic_height_in_cbs * 8)) {
    *cropping_flag = 1;
  } else {
    *cropping_flag = 0;
  }
}

/* Returns luma bit depth. */
u32 Avs2SampleBitDepth(struct Avs2Storage *storage) {

  ASSERT(storage);

  if (storage->sps.cnt)
    return (storage->sps.sample_bit_depth);
  else
    return (0);
}

/* Returns chroma bit depth. */
u32 Avs2OutputBitDepth(struct Avs2Storage *storage) {

  ASSERT(storage);

  if (storage->sps.cnt)
    return (storage->sps.output_bit_depth);
  else
    return (0);
}

/* Return the minimun dpb buffer size according to sequence header */
u32 Avs2GetMinDpbSize(struct Avs2Storage *storage) {
  // FIXME:
  return storage->sps.max_dpb_size;
}

u32 Avs2ExtractStream(const u8 *byte_stream, u32 strm_len, const u8 *strm_buf,
                      u32 buf_len, struct StrmData *stream, u32 *read_bytes,
                      u32 *start_code_detected) {

  /* Variables */

  /* Code */

  ASSERT(byte_stream);
  ASSERT(strm_len);
  // ASSERT(strm_len < BYTE_STREAM_ERROR);
  ASSERT(stream);

  /* from strm to buf end */
  stream->strm_buff_start = strm_buf;
  stream->strm_curr_pos = byte_stream;
  stream->bit_pos_in_word = 0;
  stream->strm_buff_read_bits = 0;
  stream->strm_buff_size = buf_len;
  stream->strm_data_size = strm_len;

  stream->remove_emul3_byte = 1;
  stream->remove_avs_fake_2bits = 0;

  /* byte stream format if starts with 0x000001 or 0x000000. Force using
   * byte stream format if start codes found earlier. */
  if (*start_code_detected || SwShowBits(stream, 3) <= 0x01) {
    *start_code_detected = 1;
    DEBUG_PRINT(("BYTE STREAM detected\n"));

    /* search for NAL unit start point, i.e. point after first start code
     * prefix in the stream */
    while (SwShowBits(stream, 24) != 0x01) {
      if (SwFlushBits(stream, 8) == END_OF_STREAM) {
        *read_bytes = strm_len;
        stream->remove_emul3_byte = 0;
        return HANTRO_NOK;
      }
    }
    if (SwFlushBits(stream, 24) == END_OF_STREAM) {
      *read_bytes = strm_len;
      stream->remove_emul3_byte = 0;
      return HANTRO_NOK;
    }
  }

  /* return number of bytes "consumed" */
  stream->remove_emul3_byte = 0;
  *read_bytes = stream->strm_buff_read_bits / 8;
  return (HANTRO_OK);
}

/* Searches next start code in the stream buffer. */
u32 Avs2NextStartCode(struct StrmData *stream) {

  u32 tmp;

  if (stream->bit_pos_in_word) SwGetBits(stream, 8 - stream->bit_pos_in_word);

  stream->remove_emul3_byte = 0;

  while (1) {
    tmp = SwShowBits(stream, 32);
    if (tmp <= 0x01 || (tmp >> 8) == 0x01) {
      stream->remove_emul3_byte = 0;
      return HANTRO_OK;
    }

    if (SwFlushBits(stream, 8) == END_OF_STREAM) {
      stream->remove_emul3_byte = 0;
      return END_OF_STREAM;
    }
  }

  return HANTRO_OK;
}
