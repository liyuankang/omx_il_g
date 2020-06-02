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

#include "dwl.h"

#include "global.h"
#include "sw_stream.h"
#include "avs2_vlc.h"
#include "avs2_params.h"
#include "wquant.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))


/**
 * parse sequence header of stream.
 * @return 0 success, -1 fail
 */
int Avs2ParseSequenceHeader(struct StrmData *stream, struct Avs2SeqParam *seq) {
  int i, j, marker_bit;
  seq->cnt=0;
  seq->new_sequence_flag = 1;  // flag
  seq->profile_id = u_v(stream, 8, "profile_id");
  seq->level_id = u_v(stream, 8, "level_id");
  if(seq->level_id==0)
  {
    printf("level_id error = %d, error! \n",seq->level_id);
    return -1;
  }
  seq->progressive_sequence = u_v(stream, 1, "progressive_sequence");
  seq->is_field_sequence = u_v(stream, 1, "field_coded_sequence");
  seq->horizontal_size = u_v(stream, 14, "horizontal_size");
  if(seq->horizontal_size == 0)
  {
    printf("horizontal_size error = %d, error! \n",seq->horizontal_size);
    return -1;
  }
  seq->vertical_size = u_v(stream, 14, "vertical_size");
  if(seq->vertical_size == 0)
  {
    printf("vertical_size error = %d, error! \n",seq->vertical_size);
    return -1;
  }
  seq->chroma_format = u_v(stream, 2, "chroma_format");
  if(seq->chroma_format != 1)
  {
    printf("chroma_format error = %d, error! \n",seq->chroma_format);
    return -1;
  }
  /* 0/2/3 - reserve; 1 - 4:2:0  */
  seq->output_bit_depth = 8;
  seq->sample_bit_depth = 8;
  if (seq->profile_id == BASELINE10_PROFILE) {
    // 10bit profile (0x52)
    seq->output_bit_depth = u_v(stream, 3, "sample_precision");
    seq->output_bit_depth = 6 + (seq->output_bit_depth) * 2;
    seq->sample_bit_depth = u_v(stream, 3, "encoding_precision");
    seq->sample_bit_depth = 6 + (seq->sample_bit_depth) * 2;
  } else {
    // other profile
    u_v(stream, 3, "sample_precision");
  }
  if (seq->profile_id != BASELINE10_PROFILE &&
      seq->profile_id != BASELINE_PROFILE &&
      seq->profile_id != BASELINE_PICTURE_PROFILE) {
    printf("Not support profile %d\n", seq->profile_id);
    return -1;
  }

  seq->aspect_ratio_information = u_v(stream, 4, "aspect_ratio_information");
  /* value SAR/DAR
   *  0  forbit/forbit
   *  1  1.0/-
   *  2  -/4:3
   *  3  -/16:9
   *  4  -/2.21:1
   *  others -/reserv */
  seq->frame_rate_code = u_v(stream, 4, "frame_rate_code");
  seq->bit_rate_lower = u_v(stream, 18, "bit_rate_lower");
  marker_bit = u_v(stream, 1, "marker bit");
  //error concealment
  if(marker_bit != 1)
  {
    printf("marker_bit, error!\n");
    return -1;
  }
  // CHECKMARKERBIT
  seq->bit_rate_upper = u_v(stream, 12, "bit_rate_upper");
  seq->low_delay = u_v(stream, 1, "low_delay");
  marker_bit = u_v(stream, 1, "marker bit");
  //error concealment
  if(marker_bit != 1)
  {
    printf("marker_bit, error!\n");
    return -1;
  }
  // CHECKMARKERBIT
  seq->temporal_id_exist_flag =
      u_v(stream, 1, "temporal_id exist flag");  // get Extention Flag
  u_v(stream, 18, "bbv buffer size");
  seq->lcu_size_in_bit = u_v(stream, 3, "Largest Coding Block Size");
  if(seq->lcu_size_in_bit < 4 || seq->lcu_size_in_bit > 6)
  {
    printf("lcu_size_in_bit error = %d, error! \n",seq->lcu_size_in_bit);
    return -1;
  }
  seq->weight_quant_enable_flag = u_v(stream, 1, "weight_quant_enable");
  if (seq->weight_quant_enable_flag) {
    int x, y, sizeId, uiWqMSize;
    int *Seq_WQM;
    seq->load_seq_weight_quant_data_flag =
        u_v(stream, 1, "load_seq_weight_quant_data_flag");
    for (sizeId = 0; sizeId < 2; sizeId++) {
      uiWqMSize = min(1 << (sizeId + 2), 8);
      if (seq->load_seq_weight_quant_data_flag == 1) {
        for (y = 0; y < uiWqMSize; y++) {
          for (x = 0; x < uiWqMSize; x++) {
            seq->seq_wq_matrix[sizeId][y * uiWqMSize + x] =
                ue_v(stream, "weight_quant_coeff");

            if(seq->seq_wq_matrix[sizeId][y * uiWqMSize + x] < 1 || seq->seq_wq_matrix[sizeId][y * uiWqMSize + x] > 255 )
            {
              printf("seq_wq_matrix = %d, error! \n",seq->seq_wq_matrix[sizeId][y * uiWqMSize + x]);
              return -1;
            }
          }
        }
      } else if (seq->load_seq_weight_quant_data_flag == 0) {
        Seq_WQM = GetDefaultWQM(sizeId);
        for (i = 0; i < (uiWqMSize * uiWqMSize); i++) {
          seq->seq_wq_matrix[sizeId][i] = Seq_WQM[i];

          if(seq->seq_wq_matrix[sizeId][i] < 1 || seq->seq_wq_matrix[sizeId][i] > 255 )
          {
            printf("seq_wq_matrix = %d, error! \n",seq->seq_wq_matrix[sizeId][i]);
            return -1;
          }
        }
      }
    }
  }
  seq->background_picture_enable =
      0x01 ^ u_v(stream, 1, "background_picture_disable");
  seq->b_dmh_enabled = 1;
  seq->b_mhpskip_enabled = u_v(stream, 1, "mhpskip enabled");
  seq->dhp_enabled = u_v(stream, 1, "dhp enabled");
  seq->wsm_enabled = u_v(stream, 1, "wsm enabled");
  seq->inter_amp_enable = u_v(stream, 1, "Asymmetric Motion Partitions");
  seq->useNSQT = u_v(stream, 1, "useNSQT");
  seq->useSDIP = u_v(stream, 1, "useNSIP");
  seq->b_secT_enabled = u_v(stream, 1, "secT enabled");
  seq->sao_enable = u_v(stream, 1, "SAO Enable Flag");
  seq->alf_enable = u_v(stream, 1, "ALF Enable Flag");
  seq->b_pmvr_enabled = u_v(stream, 1, "pmvr enabled");
  u_v(stream, 1, "marker bit");
  seq->num_of_rps = u_v(stream, 6, "num_of_RPS");

  if(seq->num_of_rps > 32 || seq->num_of_rps <= -1)
  {
    printf("num_of_rps=%d, error!\n",seq->num_of_rps);
    return -1;
  }
  for (i = 0; i < seq->num_of_rps; i++) {
    seq->rps[i].referd_by_others = u_v(stream, 1, "refered by others");
    seq->rps[i].num_of_ref = u_v(stream, 3, "num of reference picture");
    if(seq->rps[i].num_of_ref > 7 || seq->rps[i].num_of_ref <= -1)
    {
      printf("num_of_rps=%d, error!\n",seq->num_of_rps);
      return -1;
    }
    for (j = 0; j < seq->rps[i].num_of_ref; j++) {
      seq->rps[i].ref_pic[j] = u_v(stream, 6, "delta COI of ref pic");
    }
    seq->rps[i].num_to_remove = u_v(stream, 3, "num of removed picture");
    for (j = 0; j < seq->rps[i].num_to_remove; j++) {
      seq->rps[i].remove_pic[j] = u_v(stream, 6, "delta COI of removed pic");
    }
    u_v(stream, 1, "marker bit");
  }
  seq->picture_reorder_delay = 0;
  if (seq->low_delay == 0) {
    seq->picture_reorder_delay = u_v(stream, 5, "picture_reorder_delay");
  }
  seq->cross_slice_loop_filter = u_v(stream, 1, "Cross Loop Filter Flag");
  /*bcbr*/
  u_v(stream, 2, "reserved bits");

  /* derived */
  const int log_min_coding_block_size = 3;
  seq->pic_width_in_cbs =
      (seq->horizontal_size + ((1 << log_min_coding_block_size) - 1)) >>
      log_min_coding_block_size;
  if(seq->pic_width_in_cbs == 0)
    return -1;
  seq->pic_height_in_cbs =
      (seq->vertical_size + ((1 << log_min_coding_block_size) - 1)) >>
      log_min_coding_block_size;
  if(seq->pic_height_in_cbs == 0)
    return -1;
  seq->pic_width_in_ctbs =
      (seq->horizontal_size + (1 << seq->lcu_size_in_bit) - 1) >>
      seq->lcu_size_in_bit;
  seq->pic_height_in_ctbs =
      (seq->vertical_size + (1 << seq->lcu_size_in_bit) - 1) >>
      seq->lcu_size_in_bit;
  seq->pic_width = seq->pic_width_in_cbs*8;
  seq->pic_height = seq->pic_height_in_cbs*8;
#if 0
  seq->max_dpb_size = 15;
#else
  if(seq->level_id <= 0x22 )
    seq->max_dpb_size = 15;
  else if(seq->level_id <= 0x4A)
    seq->max_dpb_size = MIN(((13369344)/(seq->pic_width_in_cbs*8*seq->pic_height_in_cbs*8)),16)-1;
  else if(seq->level_id <= 0x5A)
    seq->max_dpb_size = MIN(((56623104)/(seq->pic_width_in_cbs*8*seq->pic_height_in_cbs*8)),16)-1;
  else
    seq->max_dpb_size = MIN(((213909504)/(seq->pic_width_in_cbs*8*seq->pic_height_in_cbs*8)),16)-1;
  //to support multi streams, need to add one buffer
  if(seq->max_dpb_size < 15)
    seq->max_dpb_size++;

    //to fix bitstream error of  Motion_5.6_ZJU_1_2/test.avs
  if(seq->max_dpb_size < 15)
    seq->max_dpb_size+=1;

  if(seq->picture_reorder_delay >=seq->max_dpb_size)
    seq->max_dpb_size =seq->picture_reorder_delay + 1;




#if 1
  if(seq->max_dpb_size > (MAXREF+1))
      seq->max_dpb_size = (MAXREF+1);
#endif
#endif
    seq->max_dpb_size = 16;


  /* util */
  seq->cnt=1;

  return 0;
}

/**
 * @return the poc/tr of the current picture.
 */
static i32 avs2CalculatePOC(struct Avs2SeqParam *sps,
                            struct Avs2PicParam *pps) {

  i32 poc;

  if (sps->low_delay == 0) {
    // poc = tr: POI, picture output index
    // coding_order: DOI
    // picture_reorder_delay:  OutputReoderDelay
    // displaydelay: PictureOutputDelay
    poc = pps->coding_order + pps->displaydelay - sps->picture_reorder_delay;
    //printf("poc=%d,pps->coding_order=%d,pps->displaydelay=%d,sps->picture_reorder_delay=%d\n",poc,pps->coding_order,pps->displaydelay,sps->picture_reorder_delay);
  } else {
    poc = pps->coding_order;
    //printf("poc=%d,pps->coding_order=%d,pps->displaydelay=%d,sps->picture_reorder_delay=%d\n",poc,pps->coding_order,pps->displaydelay,sps->picture_reorder_delay);
  }

  return poc;
}

/**
 * parse intra picture header of stream.
 * @return 0 success, -1 fail
 */
int Avs2ParseIntraPictureHeader(struct StrmData *stream,
                                struct Avs2SeqParam *seq,
                                struct Avs2PicParam *pps) {
  int i;
  pps->type = I_IMG;
  pps->cnt = 0;
  u_v(stream, 32, "bbv_delay");
  pps->time_code_flag = u_v(stream, 1, "time_code_flag");
  if (pps->time_code_flag) {
    pps->time_code = u_v(stream, 24, "time_code");
  } else {
    pps->time_code = 0;
  }
  if (seq->background_picture_enable) {
    pps->background_picture_flag = u_v(stream, 1, "background_picture_flag");
    if (pps->background_picture_flag) {
      pps->typeb = BACKGROUND_IMG;
    } else {
      pps->typeb = 0;
    }
    if (pps->typeb == BACKGROUND_IMG) {
      pps->background_picture_output_flag =
          u_v(stream, 1, "background_picture_output_flag");
    } else {
      pps->background_picture_output_flag = 0;
    }
  } else {
    pps->background_picture_flag = 0;
    pps->background_picture_output_flag = 0;
    pps->typeb = 0;
  }
  {
    pps->displaydelay =0;
    pps->coding_order = u_v(stream, 8, "coding_order");
    if (seq->temporal_id_exist_flag == 1) {
      pps->temporal_id = u_v(stream, TEMPORAL_MAXLEVEL_BIT, "temporal_id");
    }
    if (seq->low_delay == 0 &&
        !(pps->background_picture_flag &&
          !pps->background_picture_output_flag)) {  // cdp
      pps->displaydelay = ue_v(stream, "picture_output_delay");
    }
  }
  {
    int RPS_idx;  // = (img->coding_order-1) % gop_size;
    int predict;
    predict = u_v(stream, 1, "use RCS in SPS");
    if (predict) {
      RPS_idx = u_v(stream, 5, "predict for RCS");
      pps->rps = seq->rps[RPS_idx];
    } else {
      // gop size16
      int j;
      pps->rps.referd_by_others = u_v(stream, 1, "refered by others");
      pps->rps.num_of_ref = u_v(stream, 3, "num of reference picture");
      if(pps->rps.num_of_ref > 7 || pps->rps.num_of_ref <= -1)
      {
        printf("num_of_rps=%d, error!\n",seq->num_of_rps);
        return -1;
      }
      for (j = 0; j < pps->rps.num_of_ref; j++) {
        pps->rps.ref_pic[j] = u_v(stream, 6, "delta COI of ref pic");
      }
      if(pps->rps.num_of_ref>1)
      {
        for(i=0;i<(pps->rps.num_of_ref-1);i++)
        {
          for (j = i+1; j < pps->rps.num_of_ref; j++) {
            if(pps->rps.ref_pic[i] == pps->rps.ref_pic[j])
            {
              printf("reference pic=%d, error!\n",pps->rps.ref_pic[i]);
              return -1;
            }
          }
        }
      }
      pps->rps.num_to_remove = u_v(stream, 3, "num of removed picture");
      for (j = 0; j < pps->rps.num_to_remove; j++) {
        pps->rps.remove_pic[j] = u_v(stream, 6, "delta COI of removed pic");
      }
      if(pps->rps.num_to_remove>1)
      {
        for(i=0;i<(pps->rps.num_to_remove-1);i++)
        {
          for (j = i+1; j < pps->rps.num_to_remove; j++) {
            if(pps->rps.remove_pic[i] == pps->rps.remove_pic[j])
            {
              printf("reference remove pic=%d, error!\n",pps->rps.remove_pic[i]);
              return -1;
            }
          }
        }
      }
      u_v(stream, 1, "marker bit");
    }
  }
  if (seq->low_delay) {
    ue_v(stream, "bbv check times");
  }
  pps->progressive_frame = u_v(stream, 1, "progressive_frame");
  if (!pps->progressive_frame) {
    pps->picture_structure = u_v(stream, 1, "picture_structure");
  } else {
    pps->picture_structure = 1;
  }
  pps->top_field_first = u_v(stream, 1, "top_field_first");
  pps->repeat_first_field = u_v(stream, 1, "repeat_first_field");
  if (seq->is_field_sequence) {
    pps->is_top_field = u_v(stream, 1, "is_top_field");
    u_v(stream, 1, "reserved bit for interlace coding");
  }
  pps->fixed_picture_qp = u_v(stream, 1, "fixed_picture_qp");
  pps->picture_qp = u_v(stream, 7, "picture_qp");
  if(pps->picture_qp < 0 || pps->picture_qp > (63 + 8 * (seq->sample_bit_depth - 8)))
  {
    printf("picture_qp=%d, error!\n",pps->picture_qp);
    return -1;
  }
  pps->loop_filter_disable = u_v(stream, 1, "loop_filter_disable");
  if (!pps->loop_filter_disable) {
    int loop_filter_parameter_flag =
        u_v(stream, 1, "loop_filter_parameter_flag");
    if (loop_filter_parameter_flag) {
      pps->alpha_c_offset = se_v(stream, "alpha_offset");
      pps->beta_offset = se_v(stream, "beta_offset");
    } else {  // 20071009
      pps->alpha_c_offset = 0;
      pps->beta_offset = 0;
    }
    //error concealment
    if(pps->alpha_c_offset < -8 || pps->alpha_c_offset > 8)
    {
      printf("alpha_c_offset=%d, error!\n",pps->alpha_c_offset);
      return -1;
    }
    if(pps->beta_offset < -8 || pps->beta_offset > 8)
    {
      printf("beta_offset=%d, error!\n",pps->beta_offset);
      return -1;
    }
  }
  int chroma_quant_param_disable = u_v(stream, 1, "chroma_quant_param_disable");
  if (!chroma_quant_param_disable) {
    pps->chroma_quant_param_delta_u =
        se_v(stream, "chroma_quant_param_delta_cb");
    pps->chroma_quant_param_delta_v =
        se_v(stream, "chroma_quant_param_delta_cr");
  } else {
    pps->chroma_quant_param_delta_u = 0;
    pps->chroma_quant_param_delta_v = 0;
  }
  //error concealment
  if(pps->chroma_quant_param_delta_u < -16 || pps->chroma_quant_param_delta_u > 16)
  {
    printf("chroma_quant_param_delta_u=%d, error!\n",pps->chroma_quant_param_delta_u);
    return -1;
  }
  if(pps->chroma_quant_param_delta_v < -16 || pps->chroma_quant_param_delta_v > 16)
  {
    printf("chroma_quant_param_delta_v=%d, error!\n",pps->chroma_quant_param_delta_v);
    return -1;
  }
  // Adaptive frequency weighting quantization
  if (seq->weight_quant_enable_flag) {
    pps->pic_weight_quant_enable_flag =
        u_v(stream, 1, "pic_weight_quant_enable");
    if (pps->pic_weight_quant_enable_flag) {
      pps->pic_weight_quant_data_index =
          u_v(stream, 2, "pic_weight_quant_data_index");  // M2331 2008-04
      if (pps->pic_weight_quant_data_index == 1) {
        pps->mb_adapt_wq_disable =
            u_v(stream, 1, "reserved_bits");  // M2331 2008-04
        pps->weighting_quant_param =
            u_v(stream, 2, "weighting_quant_param_index");
        pps->weighting_quant_model = u_v(stream, 2, "weighting_quant_model");
        if (pps->weighting_quant_param == 1) {
          for (i = 0; i < 6; i++) {
            pps->quant_param_undetail[i] = se_v(stream, "quant_param_delta_u") +
                                           wq_param_default[UNDETAILED][i];
            //error concealment
            if(pps->quant_param_undetail[i] < 1 || pps->quant_param_undetail[i] > 255)
            {
              printf("quant_param_undetail=%d, error!\n",pps->quant_param_undetail[i]);
              return -1;
            }
            if((pps->quant_param_undetail[i] -
                    wq_param_default[UNDETAILED][i]) < -128)
            {
              printf("quant_param_undetail=%d, error!\n",pps->quant_param_undetail[i]);
              return -1;
            }
          }
        }
        if (pps->weighting_quant_param == 2) {
          for (i = 0; i < 6; i++) {
            pps->quant_param_detail[i] =
                se_v(stream, "quant_param_delta_d") +
                wq_param_default[DETAILED][i];  // delta2
#if Check_Bitstream
            //error concealment
            if(pps->quant_param_detail[i] < 1 || pps->quant_param_detail[i] > 255)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }
            if((pps->quant_param_detail[i] -
                    wq_param_default[DETAILED][i]) < -128)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }
            if((pps->quant_param_detail[i] -
                    wq_param_default[DETAILED][i]) > 127)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }

#endif
          }
        }
        // M2331 2008-04
      }  // pic_weight_quant_data_index == 1
      else if (pps->pic_weight_quant_data_index == 2) {
        int x, y, sizeId, uiWqMSize;
        for (sizeId = 0; sizeId < 2; sizeId++) {
          int i = 0;
          uiWqMSize = min(1 << (sizeId + 2), 8);
          for (y = 0; y < uiWqMSize; y++) {
            for (x = 0; x < uiWqMSize; x++) {
              pps->wq_matrix[sizeId][i++] = ue_v(stream, "weight_quant_coeff");
#if Check_Bitstream
              //error concealment
              if(pps->wq_matrix[sizeId][i - 1] < 1 || pps->wq_matrix[sizeId][i - 1] > 255)
              {
                printf("wq_matrix=%d, error!\n",pps->wq_matrix[sizeId][i - 1]);
                return -1;
              }
#endif
            }
          }
        }
      }  // pic_weight_quant_data_index == 2
    }
  } else {
    pps->pic_weight_quant_enable_flag = 0;
  }

  pps->cnt = 1;
  pps->poc = avs2CalculatePOC(seq, pps);

  return 0;
}

int Avs2ParseInterPictureHeader(struct StrmData *stream,
                                struct Avs2SeqParam *seq,
                                struct Avs2PicParam *pps) {
  int i;
  pps->cnt = 0;
  u_v(stream, 32, "bbv delay");
  int picture_coding_type = u_v(stream, 2, "picture_coding_type");
  if (seq->background_picture_enable &&
      (picture_coding_type == 1 || picture_coding_type == 3)) {
    if (picture_coding_type == 1) {
      pps->background_pred_flag = u_v(stream, 1, "background_pred_flag");
    } else {
      pps->background_pred_flag = 0;
    }
    if (pps->background_pred_flag == 0) {
      pps->background_reference_enable =
          u_v(stream, 1, "background_reference_enable");
    } else {
      pps->background_reference_enable = 1;
    }
  } else {
    pps->background_pred_flag = 0;
    pps->background_reference_enable = 0;
  }
  if (picture_coding_type == 1) {
    pps->type = P_IMG;
  } else if (picture_coding_type == 3) {
    pps->type = F_IMG;
  } else {
    pps->type = B_IMG;
  }
  if (picture_coding_type == P_IMG && pps->background_pred_flag) {
    pps->typeb = BP_IMG;
  } else {
    pps->typeb = 0;
  }
  {
    pps->displaydelay =0;
    pps->coding_order = u_v(stream, 8, "coding_order");
    if (seq->temporal_id_exist_flag == 1) {
      pps->temporal_id = u_v(stream, TEMPORAL_MAXLEVEL_BIT, "temporal_id");
    }
    if (seq->low_delay == 0) {
      pps->displaydelay = ue_v(stream, "displaydelay");
    }
  }
  {
    int RPS_idx;  // = (img->coding_order-1) % gop_size;
    int predict;
    predict = u_v(stream, 1, "use RPS in SPS");
    if (predict) {
      RPS_idx = u_v(stream, 5, "predict for RPS");
      pps->rps = seq->rps[RPS_idx];
      if((pps->rps.num_of_ref > 7 || pps->rps.num_of_ref <= 0))
      {
        printf("num_of_ref=%d, error!\n",pps->rps.num_of_ref);
        return -1;
      }
    } else {
      // gop size16
      int j;
      pps->rps.referd_by_others = u_v(stream, 1, "refered by others");
      pps->rps.num_of_ref = u_v(stream, 3, "num of reference picture");
      if(pps->rps.num_of_ref > 7 || pps->rps.num_of_ref <= 0)
      {
        printf("num_of_ref=%d, error!\n",pps->rps.num_of_ref);
        return -1;
      }
      for (j = 0; j < pps->rps.num_of_ref; j++) {
        pps->rps.ref_pic[j] = u_v(stream, 6, "delta COI of ref pic");
      }
      if(pps->rps.num_of_ref>1)
      {
        for(i=0;i<(pps->rps.num_of_ref-1);i++)
        {
          for (j = i+1; j < pps->rps.num_of_ref; j++) {
            if(pps->rps.ref_pic[i] == pps->rps.ref_pic[j])
            {
              printf("reference pic=%d, error!\n",pps->rps.ref_pic[i]);
              return -1;
            }
          }
        }
      }
      pps->rps.num_to_remove = u_v(stream, 3, "num of removed picture");
      for (j = 0; j < pps->rps.num_to_remove; j++) {
        pps->rps.remove_pic[j] = u_v(stream, 6, "delta COI of removed pic");
      }
      if(pps->rps.num_to_remove>1)
      {
        for(i=0;i<(pps->rps.num_to_remove-1);i++)
        {
          for (j = i+1; j < pps->rps.num_to_remove; j++) {
            if(pps->rps.remove_pic[i] == pps->rps.remove_pic[j])
            {
              printf("reference remove pic=%d, error!\n",pps->rps.remove_pic[i]);
              return -1;
            }
          }
        }
      }
      u_v(stream, 1, "marker bit");
    }
  }
  if (seq->low_delay) {
    ue_v(stream, "bbv check times");
  }
  pps->progressive_frame = u_v(stream, 1, "progressive_frame");
  if (!pps->progressive_frame) {
    pps->picture_structure = u_v(stream, 1, "picture_structure");
  } else {
    pps->picture_structure = 1;
  }
  pps->top_field_first = u_v(stream, 1, "top_field_first");
  pps->repeat_first_field = u_v(stream, 1, "repeat_first_field");
  if (seq->is_field_sequence) {
    pps->is_top_field = u_v(stream, 1, "is_top_field");
    u_v(stream, 1, "reserved bit for interlace coding");
  }
  pps->fixed_picture_qp = u_v(stream, 1, "fixed_picture_qp");
  pps->picture_qp = u_v(stream, 7, "picture_qp");
  //error concealment
  if(pps->picture_qp < 0 || pps->picture_qp > (63 + 8 * (seq->sample_bit_depth - 8)))
  {
    printf("picture_qp=%d, error!\n",pps->picture_qp);
    return -1;
  }

  if (!(picture_coding_type == 2 && pps->picture_structure == 1)) {
    u_v(stream, 1, "reserved_bit");
  }
  pps->random_access_decodable_flag =
      u_v(stream, 1, "random_access_decodable_flag");
  pps->loop_filter_disable = u_v(stream, 1, "loop_filter_disable");
  if (!pps->loop_filter_disable) {
    int loop_filter_parameter_flag =
        u_v(stream, 1, "loop_filter_parameter_flag");
    if (loop_filter_parameter_flag) {
      pps->alpha_c_offset = se_v(stream, "alpha_offset");
      pps->beta_offset = se_v(stream, "beta_offset");
    } else {
      pps->alpha_c_offset = 0;
      pps->beta_offset = 0;
    }
    //error concealment
    if(pps->alpha_c_offset < -8 || pps->alpha_c_offset > 8)
    {
      printf("alpha_c_offset=%d, error!\n",pps->alpha_c_offset);
      return -1;
    }
    if(pps->beta_offset < -8 || pps->beta_offset > 8)
    {
      printf("beta_offset=%d, error!\n",pps->beta_offset);
      return -1;
    }
                                                              // -8~8
  }
  int chroma_quant_param_disable = u_v(stream, 1, "chroma_quant_param_disable");
  if (!chroma_quant_param_disable) {
    pps->chroma_quant_param_delta_u =
        se_v(stream, "chroma_quant_param_delta_cb");
    pps->chroma_quant_param_delta_v =
        se_v(stream, "chroma_quant_param_delta_cr");
  } else {
    pps->chroma_quant_param_delta_u = 0;
    pps->chroma_quant_param_delta_v = 0;
  }
  //error concealment
  if(pps->chroma_quant_param_delta_u < -16 || pps->chroma_quant_param_delta_u > 16)
  {
    printf("chroma_quant_param_delta_u=%d, error!\n",pps->chroma_quant_param_delta_u);
    return -1;
  }
  if(pps->chroma_quant_param_delta_v < -16 || pps->chroma_quant_param_delta_v > 16)
  {
    printf("chroma_quant_param_delta_v=%d, error!\n",pps->chroma_quant_param_delta_v);
    return -1;
  }

  // Adaptive frequency weighting quantization
  if (seq->weight_quant_enable_flag) {
    pps->pic_weight_quant_enable_flag =
        u_v(stream, 1, "pic_weight_quant_enable");
    if (pps->pic_weight_quant_enable_flag) {
      pps->pic_weight_quant_data_index =
          u_v(stream, 2, "pic_weight_quant_data_index");  // M2331 2008-04
      if (pps->pic_weight_quant_data_index == 1) {
        pps->mb_adapt_wq_disable =
            u_v(stream, 1, "reserved_bits");  // M2331 2008-04
        pps->weighting_quant_param =
            u_v(stream, 2, "weighting_quant_param_index");
        pps->weighting_quant_model = u_v(stream, 2, "weighting_quant_model");
        if (pps->weighting_quant_param == 1) {
          for (i = 0; i < 6; i++) {
            pps->quant_param_undetail[i] = se_v(stream, "quant_param_delta_u") +
                                           wq_param_default[UNDETAILED][i];
            //error concealment
            if(pps->quant_param_undetail[i] < 1 || pps->quant_param_undetail[i] > 255)
            {
              printf("quant_param_undetail=%d, error!\n",pps->quant_param_undetail[i]);
              return -1;
            }
            if((pps->quant_param_undetail[i] -
                    wq_param_default[UNDETAILED][i]) < -128)
            {
              printf("quant_param_undetail=%d, error!\n",pps->quant_param_undetail[i]);
              return -1;
            }
            // FIXME: fail for "Quant_7.3_Hisilicon_0_1/test.avs"
            // assert((pps->quant_param_undetail[i]-
            // wq_param_default[UNDETAILED][i])<=127);

          }
        }
        if (pps->weighting_quant_param == 2) {
          for (i = 0; i < 6; i++) {
            pps->quant_param_detail[i] = se_v(stream, "quant_param_delta_d") +
                                         wq_param_default[DETAILED][i];
            //error concealment
            if(pps->quant_param_detail[i] < 1 || pps->quant_param_detail[i] > 255)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }
            if((pps->quant_param_detail[i] -
                    wq_param_default[DETAILED][i]) < -128)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }
#if 0
            if((pps->quant_param_detail[i] -
                    wq_param_default[DETAILED][i]) > 127)
            {
              printf("quant_param_detail=%d, error!\n",pps->quant_param_detail[i]);
              return -1;
            }
#endif
            // FIXME: fail for "Quant_7.3_Hisilicon_0_1/test.avs"
            // assert((pps->quant_param_detail[i]-wq_param_default[DETAILED][i])<=127);

          }
        }
      }  // pic_weight_quant_data_index == 1
      else if (pps->pic_weight_quant_data_index == 2) {
        int x, y, sizeId, uiWqMSize;
        for (sizeId = 0; sizeId < 2; sizeId++) {
          int i = 0;
          uiWqMSize = min(1 << (sizeId + 2), 8);
          for (y = 0; y < uiWqMSize; y++) {
            for (x = 0; x < uiWqMSize; x++) {
              pps->wq_matrix[sizeId][i++] = ue_v(stream, "weight_quant_coeff");
              //error concealment
              if(pps->wq_matrix[sizeId][i - 1] < 1 || pps->wq_matrix[sizeId][i - 1] > 255)
              {
                printf("wq_matrix=%d, error!\n",pps->wq_matrix[sizeId][i - 1]);
                return -1;
              }
            }
          }
        }
      }
    }
  } else {
    pps->pic_weight_quant_enable_flag = 0;
  }

  pps->cnt = 1;
  pps->poc = avs2CalculatePOC(seq, pps);

  return 0;
}

static void Avs2AlfBuildCoeffs(int *coeffmulti, u8 *table) {
  int sum, i, coeffPred;
  sum = 0;
  int coeffs[ALF_MAX_NUM_COEF];

  for (i = 0; i < ALF_MAX_NUM_COEF - 1; i++) {
    sum += (2 * coeffmulti[i]);
    coeffs[i] = coeffmulti[i];
  }
  coeffPred = (1 << ALF_NUM_BIT_SHIFT) - sum;
  coeffs[ALF_MAX_NUM_COEF - 1] = coeffPred + coeffmulti[ALF_MAX_NUM_COEF - 1];

  for(i=0; i<8; i++) {
    table[i] = (u8)coeffs[i];
  }
  table[8] = (u8)(coeffs[ALF_MAX_NUM_COEF - 1]&0xff);
  table[9] = (u8)(coeffs[ALF_MAX_NUM_COEF - 1]>>8);
}

static void Avs2AlfBuildTable(struct Avs2AlfParams *tbl, int *filterPattern,
                              int alf_y_filters) {
  int i;
  if (alf_y_filters > 1) {
    short table[16];
    memset(table, 0, NO_VAR_BINS * sizeof(short));
    for (i = 1; i < NO_VAR_BINS; ++i) {
      if (filterPattern[i]) {
        table[i] = table[i - 1] + 1;
      } else {
        table[i] = table[i - 1];
      }
    }
    for(i=0; i<NO_VAR_BINS/2; i++) {
      tbl->alf_table[i] = (table[2*i]&0xf) | (table[2*i+1]<<4);
    }
  } else {
    memset(tbl->alf_table, 0, NO_VAR_BINS/2);
  }
}

int Avs2ParseAlfCoeffComp(struct StrmData *stream, struct Avs2PicParam *pps,
                           struct Avs2AlfParams *tbl, int comp) {
  int pos;
  int f = 0, val, symbol, pre_symbole;
  const int numCoeff = (int)ALF_MAX_NUM_COEF;
  int filterPattern[NO_VAR_BINS];
  int coeffmulti[ALF_MAX_NUM_COEF];
  if (comp == 0) {
    pps->alf_y_filters = ue_v(stream, "ALF filter number");
    //error concealment
    if(pps->alf_y_filters < 0 || pps->alf_y_filters > 15)
    {
      printf("alf_y_filters=%d, error!\n",pps->alf_y_filters);
      return -1;
    }
    pps->alf_y_filters += 1;
    memset(filterPattern, 0, NO_VAR_BINS * sizeof(int));
    pre_symbole = 0;
    symbol = 0;
    for (f = 0; f < pps->alf_y_filters; f++) {
      if (f > 0) {
        if (pps->alf_y_filters != 16) {
          symbol = ue_v(stream, "Region distance");
        } else {
          symbol = 1;
        }
        if((symbol + pre_symbole) < 0 || (symbol + pre_symbole) > 15)
        {
          printf("symbol + pre_symbole=%d, error!\n",symbol + pre_symbole);
          return -1;
        }
        filterPattern[symbol + pre_symbole] = 1;
        pre_symbole = symbol + pre_symbole;
      }
      for (pos = 0; pos < numCoeff; pos++) {
        val = se_v(stream, "Luma ALF coefficients");
        //error concealment
        if(pos<=7)
        {
          if(val < -64 || val > 63)
          {
            printf("val=%d, error!\n",val);
            return -1;
          }
        }
        if(pos == 8)
        {
          if(val < -1088 || val > 1071)
          {
            printf("val=%d, error!\n",val);
            return -1;
          }
        }
        coeffmulti[pos] = val;
      }
      Avs2AlfBuildCoeffs(coeffmulti,   tbl->alf_coeff_y[f]);
    }
    Avs2AlfBuildTable(tbl, filterPattern, pps->alf_y_filters);
  } else {
    for (pos = 0; pos < numCoeff; pos++) {
      val = se_v(stream, "Chroma ALF coefficients");
      //error concealment
      if(pos<=7)
      {
        if(val < -64 || val > 63)
        {
          printf("val=%d, error!\n",val);
          return -1;
        }
      }
      if(pos == 8)
      {
        if(val < -1088 || val > 1071)
        {
          printf("val=%d, error!\n",val);
          return -1;
        }
      }
      coeffmulti[pos] = val;
    }
    if (comp == ALF_Cb) {
      Avs2AlfBuildCoeffs(coeffmulti, tbl->alf_coeff_u);
    } else {
      Avs2AlfBuildCoeffs(coeffmulti, tbl->alf_coeff_v);
    }
  }
  return 0;
}

int Avs2ParseAlfCoeff(struct StrmData *stream, struct Avs2SeqParam *seq,
                       struct Avs2PicParam *pps, struct DWLLinearMem *tbl_mem) {
  int compIdx;
  int retVal;
  struct Avs2AlfParams *tbl = (struct Avs2AlfParams *)tbl_mem->virtual_address;
  if (seq->alf_enable) {
    pps->alf_flag[0] = u_v(stream, 1, "alf_pic_flag_Y");
    pps->alf_flag[1] = u_v(stream, 1, "alf_pic_flag_Cb");
    pps->alf_flag[2] = u_v(stream, 1, "alf_pic_flag_Cr");
    if (pps->alf_flag[0] || pps->alf_flag[1] || pps->alf_flag[2]) {
      for (compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++) {
        if (pps->alf_flag[compIdx]) {
          retVal=Avs2ParseAlfCoeffComp(stream, pps, tbl, compIdx);
          if(retVal)
          {
            return retVal;
          }
        }
      }
    }
  }
  return 0;
}

void Avs2ParseSequenceDisplayExtension(struct StrmData *stream,
                                       struct Avs2SeqDisaplay *seq_ext) {
  int marker_bit;
  seq_ext->video_format = u_v(stream, 3, "video format ID");
  seq_ext->video_range = u_v(stream, 1, "video range");
  seq_ext->color_description = u_v(stream, 1, "color description");

  if (seq_ext->color_description) {
    seq_ext->color_primaries = u_v(stream, 8, "color primaries");
    seq_ext->transfer_characteristics =
        u_v(stream, 8, "transfer characteristics");
    seq_ext->matrix_coefficients = u_v(stream, 8, "matrix coefficients");
  }

  seq_ext->display_horizontal_size =
      u_v(stream, 14, "display_horizontaol_size");
  marker_bit = u_v(stream, 1, "marker bit");
  seq_ext->display_vertical_size = u_v(stream, 14, "display_vertical_size");
  seq_ext->TD_mode = u_v(stream, 1, "3D_mode");

  if (seq_ext->TD_mode) {

    seq_ext->view_packing_mode = u_v(stream, 8, "view_packing_mode");
    seq_ext->view_reverse = u_v(stream, 1, "view_reverse");
  }
  (void)(marker_bit);
}

void Avs2ParsePictureDisplayExtension(struct StrmData *stream,
                                      struct Avs2SeqParam *sps,
                                      struct Avs2PicParam *pps,
                                      struct Avs2PicDisplay *pic_ext) {
  int NumberOfFrameCentreOffsets = 1;
  int marker_bit;
  int i;

  if (sps->progressive_sequence == 1) {
    if (pps->repeat_first_field == 1) {
      if (pps->top_field_first == 1) {
        NumberOfFrameCentreOffsets = 3;
      } else {
        NumberOfFrameCentreOffsets = 2;
      }
    } else {
      NumberOfFrameCentreOffsets = 1;
    }
  } else {
    if (pps->picture_structure == 1) {
      if (pps->repeat_first_field == 1) {
        NumberOfFrameCentreOffsets = 3;
      } else {
        NumberOfFrameCentreOffsets = 2;
      }
    }
  }

  for (i = 0; i < NumberOfFrameCentreOffsets; i++) {
    pic_ext->frame_centre_horizontal_offset[i] =
        u_v(stream, 16, "frame_centre_horizontal_offset");
    marker_bit = u_v(stream, 1, "marker_bit");
    pic_ext->frame_centre_vertical_offset[i] =
        u_v(stream, 16, "frame_centre_vertical_offset");
    marker_bit = u_v(stream, 1, "marker_bit");
  }
  (void)(marker_bit);
}

void Avs2ParseUserData(struct StrmData *stream) {
  int i;
  int user_data;
  u8 *data = (u8 *)stream->strm_buff_start;
  int start = stream->strm_buff_read_bits / 8;
  int end = stream->strm_data_size;

  for (i = start; i < end; i++) {
    if ((i < end - 3) && (data[i] == 0) && (data[i + 1] == 0) &&
        (data[i + 2] == 1))
      break;
    user_data = u_v(stream, 8, "user data");
  }
  (void)(user_data);
}

void Avs2ParseCopyrightExtension(struct StrmData *stream,
                                 struct Avs2Copyright *copyright) {
  int reserved_data;
  int marker_bit;
  long long int copyright_number;
  copyright->copyright_flag = u_v(stream, 1, "copyright_flag");
  copyright->copyright_identifier = u_v(stream, 8, "copyright_identifier");
  copyright->original_or_copy = u_v(stream, 1, "original_or_copy");

  /* reserved */
  reserved_data = u_v(stream, 7, "reserved_data");
  marker_bit = u_v(stream, 1, "marker_bit");
  copyright->copyright_number_1 = u_v(stream, 20, "copyright_number_1");
  marker_bit = u_v(stream, 1, "marker_bit");
  copyright->copyright_number_2 = u_v(stream, 22, "copyright_number_2");
  marker_bit = u_v(stream, 1, "marker_bit");
  copyright->copyright_number_3 = u_v(stream, 22, "copyright_number_3");

  copyright_number = (copyright->copyright_number_1 << 44) +
                     (copyright->copyright_number_2 << 22) +
                     (copyright->copyright_number_3);
  (void)(copyright_number);
  (void)(reserved_data);
  (void)(marker_bit);
}

void Avs2ParseCameraParametersExtension(struct StrmData *stream,
                                        struct Avs2CameraParams *cam) {
  int marker_bit;
  u_v(stream, 1, "reserved");
  cam->camera_id = u_v(stream, 7, "camera id");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->height_of_image_device = u_v(stream, 22, "height_of_image_device");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->focal_length = u_v(stream, 22, "focal_length");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->f_number = u_v(stream, 22, "f_number");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->vertical_angle_of_view = u_v(stream, 22, "vertical_angle_of_view");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_position_x_upper = u_v(stream, 16, "camera_position_x_upper");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->camera_position_x_lower = u_v(stream, 16, "camera_position_x_lower");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_position_y_upper = u_v(stream, 16, "camera_position_y_upper");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->camera_position_y_lower = u_v(stream, 16, "camera_position_y_lower");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_position_z_upper = u_v(stream, 16, "camera_position_z_upper");
  marker_bit = u_v(stream, 1, "marker bit");
  cam->camera_position_z_lower = u_v(stream, 16, "camera_position_z_lower");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_direction_x = u_v(stream, 22, "camera_direction_x");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_direction_y = u_v(stream, 22, "camera_direction_y");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->camera_direction_z = u_v(stream, 22, "camera_direction_z");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->image_plane_vertical_x = u_v(stream, 22, "image_plane_vertical_x");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->image_plane_vertical_y = u_v(stream, 22, "image_plane_vertical_y");
  marker_bit = u_v(stream, 1, "marker bit");

  cam->image_plane_vertical_z = u_v(stream, 22, "image_plane_vertical_z");
  marker_bit = u_v(stream, 1, "marker bit");

  u_v(stream, 16, "reserved data");
  (void)(marker_bit);
}

void Avs2ParseScalableExtension(struct StrmData *stream) {
  int i;
  int max_temporal_level = u_v(stream, 3, "max temporal level");
  for (i = 0; i < max_temporal_level; i++) {
    u_v(stream, 4, "fps code per temporal level");
    u_v(stream, 18, "bit rate lower");
    u_v(stream, 1, "marker bit");
    u_v(stream, 12, "bit rate upper");
  }
}

void Avs2ParseMasteringDisplayAndContentMetadataExtension(
    struct StrmData *stream, struct Avs2DispAndContent *dc) {
  dc->display_primaries_x0 = u_v(stream, 16, "display_primaries_x0");
  u_v(stream, 1, "marker bit");
  dc->display_primaries_y0 = u_v(stream, 16, "display_primaries_y0");
  u_v(stream, 1, "marker bit");
  dc->display_primaries_x1 = u_v(stream, 16, "display_primaries_x1");
  u_v(stream, 1, "marker bit");
  dc->display_primaries_y1 = u_v(stream, 16, "display_primaries_y1");
  u_v(stream, 1, "marker bit");
  dc->display_primaries_x2 = u_v(stream, 16, "display_primaries_x2");
  u_v(stream, 1, "marker bit");
  dc->display_primaries_y2 = u_v(stream, 16, "display_primaries_y2");
  u_v(stream, 1, "marker bit");

  dc->white_point_x = u_v(stream, 16, "white_point_x");
  u_v(stream, 1, "marker bit");
  dc->white_point_y = u_v(stream, 16, "white_point_y");
  u_v(stream, 1, "marker bit");

  dc->max_display_mastering_luminance =
      u_v(stream, 16, "max_display_mastering_luminance");
  u_v(stream, 1, "marker bit");
  dc->min_display_mastering_luminance =
      u_v(stream, 16, "min_display_mastering_luminance");
  u_v(stream, 1, "marker bit");

  dc->maximum_content_light_level =
      u_v(stream, 16, "maximum_content_light_level");
  u_v(stream, 1, "marker bit");
  dc->maximum_frame_average_light_level =
      u_v(stream, 16, "maximum_frame_average_light_level");
  u_v(stream, 1, "marker bit");
}

void Avs2ParseExtensionData(struct StrmData *stream, struct Avs2SeqParam *sps,
                            struct Avs2PicParam *pps, struct Avs2ExtData *ext) {
  int ext_ID;

  ext_ID = u_v(stream, 4, "extension ID");

  switch (ext_ID) {
    case SEQUENCE_DISPLAY_EXTENSION_ID:
      Avs2ParseSequenceDisplayExtension(stream, &ext->display);
      break;
    case COPYRIGHT_EXTENSION_ID:
      Avs2ParseCopyrightExtension(stream, &ext->copyright);
      break;
    case PICTURE_DISPLAY_EXTENSION_ID:
      Avs2ParsePictureDisplayExtension(stream, sps, pps, &ext->pic);
      break;
    case CAMERAPARAMETERS_EXTENSION_ID:
      Avs2ParseCameraParametersExtension(stream, &ext->cam);
      break;
    case TEMPORAL_SCALABLE_EXTENSION_ID:
      Avs2ParseScalableExtension(stream);
      break;
    case LOCATION_DATA_EXTENSION_ID:
#if 0
        input->ROI_Coding = 1;
        OpenPosFile(input->out_datafile);
        roi_parameters_extension();
#endif
      break;
    case MASTERING_DISPLAY_AND_CONTENT_METADATA_EXTENSION:
      break;
    default:
      printf("reserved extension start code ID %d\n", ext_ID);
      break;
  }
}
