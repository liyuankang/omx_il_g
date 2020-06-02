/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2002-2016, Audio Video coding Standard Workgroup of China
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of Audio Video coding Standard Workgroup of China
*    nor the names of its contributors maybe used to endorse or promote products
*    derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "global.h"
#include "header.h"
#include "pos_info.h"
// Adaptive frequency weighting quantization
#if FREQUENCY_WEIGHTING_QUANTIZATION
#include "wquant.h"
#endif
#include "basetype.h"
#include "sw_stream.h"
#include "sw_util.h"

#include "avs2_vlc.h"
#include "avs2_container.h"
#include "avs2_decoder.h"
#include "avs2hwd_api.h"
#include "avs2_params.h"

CameraParamters CameraParameter, *camera = &CameraParameter;

extern StatBits *StatBitsPtr;

const int InterlaceSetting[] = {4, 4, 0, 5, 0, 5, 0, 5, 3, 3, 3, 3, 0, 0, 0, 0,
                                2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 4, 0};

#define BYTE_STREAM_ERROR 0xFFFFFFFF

/*
*/

void SequenceHeader(char *buf, int startcodepos, int length) {
  int i, j;
  struct Avs2SeqParam *seq = &avs2dec_cont->storage.sps;

  fprintf(stdout, "Sequence Header\n");
  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 0;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  Avs2ParseSequenceHeader(currStream, &avs2dec_cont->storage.sps);

  input->profile_id = seq->profile_id;
  input->level_id = seq->level_id;
  hd->progressive_sequence = seq->progressive_sequence;
#if INTERLACE_CODING
  hd->is_field_sequence = seq->is_field_sequence;
#endif
#if HALF_PIXEL_COMPENSATION || HALF_PIXEL_CHROMA
  img->is_field_sequence = seq->is_field_sequence;
#endif
  hd->horizontal_size = seq->horizontal_size;
  hd->vertical_size = seq->vertical_size;
  input->chroma_format = seq->chroma_format;
  input->output_bit_depth = seq->output_bit_depth;
  input->sample_bit_depth = seq->sample_bit_depth;
  hd->aspect_ratio_information = seq->aspect_ratio_information;
  hd->frame_rate_code = seq->frame_rate_code;

  hd->bit_rate_lower = seq->bit_rate_lower;
  // CHECKMARKERBIT
  hd->bit_rate_upper = seq->bit_rate_upper;
  hd->low_delay = seq->low_delay;
  hd->temporal_id_exist_flag =
      seq->temporal_id_exist_flag;  // get Extention Flag
  input->g_uiMaxSizeInBit = seq->lcu_size_in_bit;
  hd->weight_quant_enable_flag = seq->weight_quant_enable_flag;
  for (int sizeId = 0; sizeId < 2; sizeId++) {
    int uiWqMSize = min(1 << (sizeId + 2), 8);
    for (int i = 0; i < (uiWqMSize * uiWqMSize); i++) {
      seq_wq_matrix[sizeId][i] = seq->seq_wq_matrix[sizeId][i];
    }
  }
  hd->background_picture_enable = seq->background_picture_enable;
  hd->b_dmh_enabled = seq->b_dmh_enabled;

  hd->b_mhpskip_enabled = seq->b_mhpskip_enabled;
  hd->dhp_enabled = seq->dhp_enabled;
  hd->wsm_enabled = seq->wsm_enabled;
  img->inter_amp_enable = seq->inter_amp_enable;
  input->useNSQT = seq->useNSQT;
  input->useSDIP = seq->useSDIP;
  hd->b_secT_enabled = seq->b_secT_enabled;
  input->sao_enable = seq->sao_enable;
  input->alf_enable = seq->alf_enable;
  hd->b_pmvr_enabled = seq->b_pmvr_enabled;
  hd->gop_size = seq->num_of_rps;
  for (i = 0; i < hd->gop_size; i++) {
    hd->decod_RPS[i].referd_by_others = seq->rps[i].referd_by_others;
    hd->decod_RPS[i].num_of_ref = seq->rps[i].num_of_ref;

    for (j = 0; j < hd->decod_RPS[i].num_of_ref; j++) {
      hd->decod_RPS[i].ref_pic[j] = seq->rps[i].ref_pic[j];
    }
    hd->decod_RPS[i].num_to_remove = seq->rps[i].num_to_remove;
    for (j = 0; j < hd->decod_RPS[i].num_to_remove; j++) {
      hd->decod_RPS[i].remove_pic[j] = seq->rps[i].remove_pic[j];
    }
  }
  hd->picture_reorder_delay = seq->picture_reorder_delay;
  input->crossSliceLoopFilter = seq->cross_slice_loop_filter;

  img->width = hd->horizontal_size;
  img->height = hd->vertical_size;
  img->width_cr = (img->width >> 1);

  if (input->chroma_format == 1) {
    img->height_cr = (img->height >> 1);
  }

  img->PicWidthInMbs = img->width / MIN_CU_SIZE;
  img->PicHeightInMbs = img->height / MIN_CU_SIZE;
  img->PicSizeInMbs = img->PicWidthInMbs * img->PicHeightInMbs;
  img->buf_cycle = input->buf_cycle + 1;
  img->max_mb_nr = (img->width * img->height) / (MIN_CU_SIZE * MIN_CU_SIZE);

  hc->seq_header++;
}

void video_edit_code_data(char *buf, int startcodepos, int length) {
  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 1;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  hd->vec_flag = 1;
}

void I_Picture_Header(char *buf, int startcodepos, int length) {
  int i;
  struct Avs2PicParam *pps = &avs2dec_cont->storage.pps;

  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 1;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  Avs2ParseIntraPictureHeader(currStream, &avs2dec_cont->storage.sps, pps);
  Avs2ParseAlfCoeff(currStream, &avs2dec_cont->storage.sps, pps,
                    &avs2dec_cont->cmems.alf_tbl);

  hd->time_code_flag = pps->time_code_flag;

  hd->time_code = pps->time_code;
  hd->background_picture_flag = pps->background_picture_flag;
  img->typeb = pps->typeb;
  hd->background_picture_output_flag = pps->background_picture_output_flag;

  {
    img->coding_order = pps->coding_order;
    hd->cur_layer = pps->temporal_id;
    hd->displaydelay = pps->displaydelay;
  }
  {
    // gop size16
    int j;
    hd->curr_RPS.referd_by_others = pps->rps.referd_by_others;
    hd->curr_RPS.num_of_ref = pps->rps.num_of_ref;
    for (j = 0; j < hd->curr_RPS.num_of_ref; j++) {
      hd->curr_RPS.ref_pic[j] = pps->rps.ref_pic[j];
    }
    hd->curr_RPS.num_to_remove = pps->rps.num_to_remove;
    for (j = 0; j < hd->curr_RPS.num_to_remove; j++) {
      hd->curr_RPS.remove_pic[j] = pps->rps.remove_pic[j];
    }
  }

  hd->progressive_frame = pps->progressive_frame;
  img->picture_structure = pps->picture_structure;

  hd->top_field_first = pps->top_field_first;
  hd->repeat_first_field = pps->repeat_first_field;
  hd->is_top_field = pps->is_top_field;
  img->is_top_field = hd->is_top_field;
  hd->fixed_picture_qp = pps->fixed_picture_qp;
  hd->picture_qp = pps->picture_qp;
  hd->loop_filter_disable = pps->loop_filter_disable;

  input->alpha_c_offset = pps->alpha_c_offset;
  input->beta_offset = pps->beta_offset;
  // hd->chroma_quant_param_disable = u_v(currStream,1,
  // "chroma_quant_param_disable");
  hd->chroma_quant_param_delta_u = pps->chroma_quant_param_delta_u;
  hd->chroma_quant_param_delta_v = pps->chroma_quant_param_delta_v;
// Adaptive frequency weighting quantization
#if FREQUENCY_WEIGHTING_QUANTIZATION

  if (hd->weight_quant_enable_flag) {
    hd->pic_weight_quant_enable_flag = pps->pic_weight_quant_enable_flag;
    if (hd->pic_weight_quant_enable_flag) {
      hd->pic_weight_quant_data_index = pps->pic_weight_quant_data_index;
      if (hd->pic_weight_quant_data_index == 1) {
        hd->mb_adapt_wq_disable = pps->mb_adapt_wq_disable;
        hd->weighting_quant_param = pps->weighting_quant_param;

        hd->weighting_quant_model = pps->weighting_quant_model;
        hd->CurrentSceneModel = hd->weighting_quant_model;

        // M2331 2008-04
        if (hd->weighting_quant_param == 1) {
          for (i = 0; i < 6; i++) {
            hd->quant_param_undetail[i] = pps->quant_param_undetail[i];
          }
        }
        if (hd->weighting_quant_param == 2) {
          for (i = 0; i < 6; i++) {
            hd->quant_param_detail[i] = pps->quant_param_detail[i];
          }
        }
        // M2331 2008-04

      }  // pic_weight_quant_data_index == 1
      else if (hd->pic_weight_quant_data_index == 2) {
        int x, y, sizeId, uiWqMSize;
        for (sizeId = 0; sizeId < 2; sizeId++) {
          int i = 0;
          uiWqMSize = min(1 << (sizeId + 2), 8);
          for (y = 0; y < uiWqMSize; y++) {
            for (x = 0; x < uiWqMSize; x++) {
              pic_user_wq_matrix[sizeId][i] = pps->wq_matrix[sizeId][i];
              i++;
            }
          }
        }
      }  // pic_weight_quant_data_index == 2
    }
  }

#endif

  img->qp = pps->picture_qp;

  img->type = pps->type;

  /* alf */
  img->pic_alf_on[0] = pps->alf_flag[0];
  img->pic_alf_on[1] = pps->alf_flag[1];
  img->pic_alf_on[2] = pps->alf_flag[2];
}
#if 0
void showInterHeader()
{
    static FILE *fp=NULL;
    int i;
    if (fp==NULL) {
      fp = fopen("traceInterHeader.txt", "wt");
    }
    //hd->picture_coding_type                = pps->picture_coding_type;
    fprintf(fp, "hd->background_pred_flag=%d\n", hd->background_pred_flag);
    fprintf(fp, "hd->background_reference_enable=%d\n",  hd->background_reference_enable);
    
    fprintf(fp, "img->type = %d\n", img->type);
    fprintf(fp, "img->typeb = %d\n", img->typeb);

    {
        fprintf(fp, "img->coding_order = %d\n", img->coding_order);
        fprintf(fp, "hd->cur_layer = %d\n", hd->cur_layer);
        fprintf(fp, "hd->displaydelay = %d\n", hd->displaydelay);
    }
    {
        int j;
        fprintf(fp, "hd->curr_RPS.referd_by_others = %d\n", hd->curr_RPS.referd_by_others);
        fprintf(fp, "hd->curr_RPS.num_of_ref = %d\n", hd->curr_RPS.num_of_ref);
        for (j = 0; j < hd->curr_RPS.num_of_ref; j++) {
            fprintf(fp, "hd->curr_RPS.ref_pic[j] = %d\n", hd->curr_RPS.ref_pic[j]);
        }
        fprintf(fp, "hd->curr_RPS.num_to_remove = %d\n", hd->curr_RPS.num_to_remove);
        for (j = 0; j < hd->curr_RPS.num_to_remove; j++) {
            fprintf(fp, "hd->curr_RPS.remove_pic[j] = %d\n", hd->curr_RPS.remove_pic[j]);
        }
    }
    fprintf(fp, "hd->progressive_frame       = %d\n", hd->progressive_frame);
    fprintf(fp, "img->picture_structure = %d\n", img->picture_structure);
    fprintf(fp, "hd->top_field_first         = %d\n", hd->top_field_first);
    fprintf(fp, "hd->repeat_first_field      = %d\n", hd->repeat_first_field);
    fprintf(fp, "hd->is_top_field        = %d\n", hd->is_top_field);
    fprintf(fp, "img->is_top_field = %d\n", img->is_top_field);

    fprintf(fp, "hd->fixed_picture_qp        = %d\n", hd->fixed_picture_qp);
    fprintf(fp, "hd->picture_qp              = %d\n", hd->picture_qp);
    fprintf(fp, "img->random_access_decodable_flag = %d\n", img->random_access_decodable_flag);
    //u_v ( currStream,3, "reserved bits" );

    /*  skip_mode_flag      = u_v(1,"skip_mode_flag");*/  //qyu0822 delete skip_mode_flag
    fprintf(fp, "hd->loop_filter_disable       = %d\n", hd->loop_filter_disable);
    //hd->loop_filter_parameter_flag = pps->loop_filter_parameter_flag);
    fprintf(fp, "input->alpha_c_offset = %d\n", input->alpha_c_offset);
    fprintf(fp, "input->beta_offset  = %d\n", input->beta_offset);
    //hd->chroma_quant_param_disable     = u_v(currStream,1, "chroma_quant_param_disable");
    fprintf(fp, "hd->chroma_quant_param_delta_u = %d\n", hd->chroma_quant_param_delta_u);
    fprintf(fp, "hd->chroma_quant_param_delta_v = %d\n", hd->chroma_quant_param_delta_v);
    // Adaptive frequency weighting quantization
    if (hd->weight_quant_enable_flag) {
        fprintf(fp, "hd->pic_weight_quant_enable_flag = %d\n", hd->pic_weight_quant_enable_flag);
        if (hd->pic_weight_quant_enable_flag) {
            fprintf(fp, "hd->pic_weight_quant_data_index = %d\n", hd->pic_weight_quant_data_index); //M2331 2008-04
            if (hd->pic_weight_quant_data_index == 1) {
                fprintf(fp, "hd->mb_adapt_wq_disable = %d\n", hd->mb_adapt_wq_disable); //M2331 2008-04
#if CHROMA_DELTA_QP
#endif
                fprintf(fp, "hd->weighting_quant_param = %d\n", hd->weighting_quant_param);

                fprintf(fp, "hd->weighting_quant_model = %d\n", hd->weighting_quant_model);
                fprintf(fp, "hd->CurrentSceneModel = %d\n", hd->CurrentSceneModel);

                //M2331 2008-04
                if (hd->weighting_quant_param == 1) {
                    for (i = 0; i < 6; i++) {
                        fprintf(fp, "hd->quant_param_undetail[i] = %d\n", hd->quant_param_undetail[i]);
                    }
                }
                if (hd->weighting_quant_param == 2) {
                    for (i = 0; i < 6; i++) {
                        fprintf(fp, "hd->quant_param_detail[i] = %d\n", hd->quant_param_detail[i]);
                    }
                }
                //M2331 2008-04
            } //pic_weight_quant_data_index == 1
            else if (hd->pic_weight_quant_data_index == 2) {
                int x, y, sizeId, uiWqMSize;
                for (sizeId = 0; sizeId < 2; sizeId++) {
                    int i = 0;
                    uiWqMSize = min(1 << (sizeId + 2), 8);
                    for (y = 0; y < uiWqMSize; y++) {
                        for (x = 0; x < uiWqMSize; x++) {
                            fprintf(fp, "pic_user_wq_matrix[sizeId][i] = %d\n", pic_user_wq_matrix[sizeId][i]);
                            i++;
                        }
                    }
                }
            }//pic_weight_quant_data_index == 2
        }
    }

    fprintf(fp, "img->qp                = %d\n", img->qp);
}
#else
#define showInterHeader(...)
#endif

/*
*************************************************************************
* Function:pb picture header
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void PB_Picture_Header(char *buf, int startcodepos, int length) {
  int i;
  struct Avs2PicParam *pps = &avs2dec_cont->storage.pps;

  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 1;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  Avs2ParseInterPictureHeader(currStream, &avs2dec_cont->storage.sps, pps);
  Avs2ParseAlfCoeff(currStream, &avs2dec_cont->storage.sps, pps,
                    &avs2dec_cont->cmems.alf_tbl);

  // hd->picture_coding_type                = pps->picture_coding_type;
  hd->background_pred_flag = pps->background_pred_flag;
  hd->background_reference_enable = pps->background_reference_enable;

  img->type = pps->type;
  img->typeb = pps->typeb;

  {
    img->coding_order = pps->coding_order;
    hd->cur_layer = pps->temporal_id;
    hd->displaydelay = pps->displaydelay;
  }
  {
    int j;
    hd->curr_RPS.referd_by_others = pps->rps.referd_by_others;
    hd->curr_RPS.num_of_ref = pps->rps.num_of_ref;
    for (j = 0; j < hd->curr_RPS.num_of_ref; j++) {
      hd->curr_RPS.ref_pic[j] = pps->rps.ref_pic[j];
    }
    hd->curr_RPS.num_to_remove = pps->rps.num_to_remove;
    for (j = 0; j < hd->curr_RPS.num_to_remove; j++) {
      hd->curr_RPS.remove_pic[j] = pps->rps.remove_pic[j];
    }
  }
  hd->progressive_frame = pps->progressive_frame;
  img->picture_structure = pps->picture_structure;
  hd->top_field_first = pps->top_field_first;
  hd->repeat_first_field = pps->repeat_first_field;
  hd->is_top_field = pps->is_top_field;
  img->is_top_field = hd->is_top_field;

  hd->fixed_picture_qp = pps->fixed_picture_qp;
  hd->picture_qp = pps->picture_qp;
  img->random_access_decodable_flag = pps->random_access_decodable_flag;
  // u_v ( currStream,3, "reserved bits" );

  /*  skip_mode_flag      = u_v(1,"skip_mode_flag");*/  // qyu0822 delete
                                                        // skip_mode_flag
  hd->loop_filter_disable = pps->loop_filter_disable;
  // hd->loop_filter_parameter_flag = pps->loop_filter_parameter_flag;
  input->alpha_c_offset = pps->alpha_c_offset;
  input->beta_offset = pps->beta_offset;
  // hd->chroma_quant_param_disable     = u_v(currStream,1,
  // "chroma_quant_param_disable");
  hd->chroma_quant_param_delta_u = pps->chroma_quant_param_delta_u;
  hd->chroma_quant_param_delta_v = pps->chroma_quant_param_delta_v;
  // Adaptive frequency weighting quantization
  if (hd->weight_quant_enable_flag) {
    hd->pic_weight_quant_enable_flag = pps->pic_weight_quant_enable_flag;
    if (hd->pic_weight_quant_enable_flag) {
      hd->pic_weight_quant_data_index =
          pps->pic_weight_quant_data_index;  // M2331 2008-04
      if (hd->pic_weight_quant_data_index == 1) {
        hd->mb_adapt_wq_disable = pps->mb_adapt_wq_disable;  // M2331 2008-04
#if CHROMA_DELTA_QP
#endif
        hd->weighting_quant_param = pps->weighting_quant_param;

        hd->weighting_quant_model = pps->weighting_quant_model;
        hd->CurrentSceneModel = hd->weighting_quant_model;

        // M2331 2008-04
        if (hd->weighting_quant_param == 1) {
          for (i = 0; i < 6; i++) {
            hd->quant_param_undetail[i] = pps->quant_param_undetail[i];
          }
        }
        if (hd->weighting_quant_param == 2) {
          for (i = 0; i < 6; i++) {
            hd->quant_param_detail[i] = pps->quant_param_detail[i];
          }
        }
        // M2331 2008-04
      }  // pic_weight_quant_data_index == 1
      else if (hd->pic_weight_quant_data_index == 2) {
        int x, y, sizeId, uiWqMSize;
        for (sizeId = 0; sizeId < 2; sizeId++) {
          int i = 0;
          uiWqMSize = min(1 << (sizeId + 2), 8);
          for (y = 0; y < uiWqMSize; y++) {
            for (x = 0; x < uiWqMSize; x++) {
              pic_user_wq_matrix[sizeId][i] = pps->wq_matrix[sizeId][i];
              i++;
            }
          }
        }
      }  // pic_weight_quant_data_index == 2
    }
  }

  img->qp = pps->picture_qp;

  /* alf */
  img->pic_alf_on[0] = pps->alf_flag[0];
  img->pic_alf_on[1] = pps->alf_flag[1];
  img->pic_alf_on[2] = pps->alf_flag[2];

  showInterHeader();
}
/*
*************************************************************************
* Function:decode extension and user data
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void extension_data(char *buf, int startcodepos, int length) {
  int ext_ID;

  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 0;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  ext_ID = u_v(currStream, 4, "extension ID");

  switch (ext_ID) {
    case SEQUENCE_DISPLAY_EXTENSION_ID:
      sequence_display_extension();
      break;
    case COPYRIGHT_EXTENSION_ID:
      copyright_extension();
      break;
    case PICTURE_DISPLAY_EXTENSION_ID:
      picture_display_extension();
      break;  // rm52k_r1
    case CAMERAPARAMETERS_EXTENSION_ID:
      cameraparameters_extension();
      break;
#if M3480_TEMPORAL_SCALABLE
    case TEMPORAL_SCALABLE_EXTENSION_ID:
      scalable_extension();
      break;
#endif
#if ROI_M3264
    case LOCATION_DATA_EXTENSION_ID:
      input->ROI_Coding = 1;
      OpenPosFile(input->out_datafile);
      roi_parameters_extension();
      break;
#endif
#if AVS2_HDR_HLS
    case MASTERING_DISPLAY_AND_CONTENT_METADATA_EXTENSION:
      break;
#endif
    default:
      printf("reserved extension start code ID %d\n", ext_ID);
      break;
  }
}
/*
*************************************************************************
* Function: decode sequence display extension
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void sequence_display_extension() {

  hd->video_format = u_v(currStream, 3, "video format ID");
  hd->video_range = u_v(currStream, 1, "video range");
  hd->color_description = u_v(currStream, 1, "color description");

  if (hd->color_description) {
    hd->color_primaries = u_v(currStream, 8, "color primaries");
    hd->transfer_characteristics =
        u_v(currStream, 8, "transfer characteristics");
    hd->matrix_coefficients = u_v(currStream, 8, "matrix coefficients");
  }

  hd->display_horizontal_size = u_v(currStream, 14, "display_horizontaol_size");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->display_vertical_size = u_v(currStream, 14, "display_vertical_size");
  hd->TD_mode = u_v(currStream, 1, "3D_mode");

  if (hd->TD_mode) {

    hd->view_packing_mode = u_v(currStream, 8, "view_packing_mode");
    hd->view_reverse = u_v(currStream, 1, "view_reverse");
  }
}

/*
*************************************************************************
* Function: decode picture display extension
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void picture_display_extension() {
#if RD170_FIX_BG
  int NumberOfFrameCentreOffsets = 1;
#else
  int NumberOfFrameCentreOffsets;
#endif
  int i;

  if (hd->progressive_sequence == 1) {
    if (hd->repeat_first_field == 1) {
      if (hd->top_field_first == 1) {
        NumberOfFrameCentreOffsets = 3;
      } else {
        NumberOfFrameCentreOffsets = 2;
      }
    } else {
      NumberOfFrameCentreOffsets = 1;
    }
  } else {
    if (img->picture_structure == 1) {
      if (hd->repeat_first_field == 1) {
        NumberOfFrameCentreOffsets = 3;
      } else {
        NumberOfFrameCentreOffsets = 2;
      }
    }
  }

  for (i = 0; i < NumberOfFrameCentreOffsets; i++) {
    hd->frame_centre_horizontal_offset[i] =
        u_v(currStream, 16, "frame_centre_horizontal_offset");
    hd->marker_bit = u_v(currStream, 1, "marker_bit");
    hd->frame_centre_vertical_offset[i] =
        u_v(currStream, 16, "frame_centre_vertical_offset");
    hd->marker_bit = u_v(currStream, 1, "marker_bit");
  }
}

/*
*************************************************************************
* Function:user data //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void user_data(char *buf, int startcodepos, int length) {
  int i;
  int user_data;

  memcpy(currStream->strm_buff_start, buf, length);
  currStream->strm_buff_size = currStream->strm_data_size = length;
  currStream->strm_buff_read_bits = (startcodepos + 1) * 8;
  currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
  currStream->strm_curr_pos = currStream->strm_buff_start + startcodepos + 1;
  currStream->remove_emul3_byte = 1;
  currStream->remove_avs_fake_2bits = 0;
  currStream->emul_byte_count = 0;
  currStream->is_rb = 0;

  for (i = 0; i < length - 4; i++) {
    user_data = u_v(currStream, 8, "user data");
  }
}

/*
*************************************************************************
* Function:Copyright extension //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void copyright_extension() {
  int reserved_data;
  int marker_bit;
  long long int copyright_number;
  hd->copyright_flag = u_v(currStream, 1, "copyright_flag");
  hd->copyright_identifier = u_v(currStream, 8, "copyright_identifier");
  hd->original_or_copy = u_v(currStream, 1, "original_or_copy");

  /* reserved */
  reserved_data = u_v(currStream, 7, "reserved_data");
  marker_bit = u_v(currStream, 1, "marker_bit");
  hd->copyright_number_1 = u_v(currStream, 20, "copyright_number_1");
  marker_bit = u_v(currStream, 1, "marker_bit");
  hd->copyright_number_2 = u_v(currStream, 22, "copyright_number_2");
  marker_bit = u_v(currStream, 1, "marker_bit");
  hd->copyright_number_3 = u_v(currStream, 22, "copyright_number_3");

  copyright_number = (hd->copyright_number_1 << 44) +
                     (hd->copyright_number_2 << 22) + hd->copyright_number_3;
}

/*
*************************************************************************
* Function:camera parameters extension //sw
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void cameraparameters_extension() {

  u_v(currStream, 1, "reserved");
  hd->camera_id = u_v(currStream, 7, "camera id");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->height_of_image_device = u_v(currStream, 22, "height_of_image_device");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->focal_length = u_v(currStream, 22, "focal_length");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->f_number = u_v(currStream, 22, "f_number");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->vertical_angle_of_view = u_v(currStream, 22, "vertical_angle_of_view");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_position_x_upper = u_v(currStream, 16, "camera_position_x_upper");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->camera_position_x_lower = u_v(currStream, 16, "camera_position_x_lower");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_position_y_upper = u_v(currStream, 16, "camera_position_y_upper");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->camera_position_y_lower = u_v(currStream, 16, "camera_position_y_lower");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_position_z_upper = u_v(currStream, 16, "camera_position_z_upper");
  hd->marker_bit = u_v(currStream, 1, "marker bit");
  hd->camera_position_z_lower = u_v(currStream, 16, "camera_position_z_lower");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_direction_x = u_v(currStream, 22, "camera_direction_x");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_direction_y = u_v(currStream, 22, "camera_direction_y");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->camera_direction_z = u_v(currStream, 22, "camera_direction_z");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->image_plane_vertical_x = u_v(currStream, 22, "image_plane_vertical_x");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->image_plane_vertical_y = u_v(currStream, 22, "image_plane_vertical_y");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  hd->image_plane_vertical_z = u_v(currStream, 22, "image_plane_vertical_z");
  hd->marker_bit = u_v(currStream, 1, "marker bit");

  u_v(currStream, 16, "reserved data");
}

#if M3480_TEMPORAL_SCALABLE
/*
*************************************************************************
* Function:temporal scalable extension data
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void scalable_extension() {
  int i;
  int max_temporal_level = u_v(currStream, 3, "max temporal level");
  for (i = 0; i < max_temporal_level; i++) {
    u_v(currStream, 4, "fps code per temporal level");
    u_v(currStream, 18, "bit rate lower");
    u_v(currStream, 1, "marker bit");
    u_v(currStream, 12, "bit rate upper");
  }
}
#endif

/*
*************************************************************************
* Function:mastering display and content metadata extension data
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void mastering_display_and_content_metadata_extension() {
  hd->display_primaries_x0 = u_v(currStream, 16, "display_primaries_x0");
  u_v(currStream, 1, "marker bit");
  hd->display_primaries_y0 = u_v(currStream, 16, "display_primaries_y0");
  u_v(currStream, 1, "marker bit");
  hd->display_primaries_x1 = u_v(currStream, 16, "display_primaries_x1");
  u_v(currStream, 1, "marker bit");
  hd->display_primaries_y1 = u_v(currStream, 16, "display_primaries_y1");
  u_v(currStream, 1, "marker bit");
  hd->display_primaries_x2 = u_v(currStream, 16, "display_primaries_x2");
  u_v(currStream, 1, "marker bit");
  hd->display_primaries_y2 = u_v(currStream, 16, "display_primaries_y2");
  u_v(currStream, 1, "marker bit");

  hd->white_point_x = u_v(currStream, 16, "white_point_x");
  u_v(currStream, 1, "marker bit");
  hd->white_point_y = u_v(currStream, 16, "white_point_y");
  u_v(currStream, 1, "marker bit");

  hd->max_display_mastering_luminance =
      u_v(currStream, 16, "max_display_mastering_luminance");
  u_v(currStream, 1, "marker bit");
  hd->min_display_mastering_luminance =
      u_v(currStream, 16, "min_display_mastering_luminance");
  u_v(currStream, 1, "marker bit");

  hd->maximum_content_light_level =
      u_v(currStream, 16, "maximum_content_light_level");
  u_v(currStream, 1, "marker bit");
  hd->maximum_frame_average_light_level =
      u_v(currStream, 16, "maximum_frame_average_light_level");
  u_v(currStream, 1, "marker bit");
}
/*
*************************************************************************
* Function:To calculate the poc values
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void calc_picture_distance() {
  // for POC mode 0:
  unsigned int MaxPicDistanceLsb = (1 << 8);

  if (img->coding_order < img->PrevPicDistanceLsb)  // 9.2.2(d), new DOI loop
  {
    int i, j;

    hc->total_frames++;
    for (i = 0; i < REF_MAXBUFFER; i++) {
      if (fref[i]->img_poi >=
          0) {  // all reference POI = POI - 256,  DOI = DOI -256
        fref[i]->img_poi -= 256;
        fref[i]->img_coi -= 256;
      }
#if RD170_FIX_BG
      for (j = 0; j < MAXREF; j++) {
#else
      for (j = 0; j < 4; j++) {
#endif
        fref[i]->ref_poc[j] -= 256;  // all reference frame's POI = POI - 256
      }
    }
    for (i = 0; i < outprint.buffer_num;
         i++) {  // all outprint buffer POI = POI- 256
      outprint.stdoutdata[i].framenum -= 256;
      outprint.stdoutdata[i].tr -= 256;
    }

    hd->last_output -= 256;
    hd->curr_IDRtr -= 256;  // IDR DOI
    hd->curr_IDRcoi -= 256;  // IDR POI
    hd->next_IDRtr -= 256;
    hd->next_IDRcoi -= 256;
  }
  if (hd->low_delay == 0) {
    // tr: POI, picture  output index
    // coding_order: DOI
    // picture_reorder_delay:  OutputReoderDelay
    // displaydelay: PictureOutputDelay
    img->tr = img->coding_order + hd->displaydelay - hd->picture_reorder_delay;
  } else {
    img->tr = img->coding_order;
  }

#if REMOVE_UNUSED
  img->pic_distance = img->tr;
#else
  img->pic_distance = img->tr % 256;
#endif
}

// Lou
int sign(int a, int b) {
  int x;

  x = abs(a);

  if (b > 0) {
    return (x);
  } else {
    return (-x);
  }
}
