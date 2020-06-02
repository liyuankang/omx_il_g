/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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
#include "dwl.h"
#include "regdrv.h"
#include "string.h"
#include "commonVariables.h"
#include "avs2hwd_asic.h"
#include "avs2hwd_api.h"

#define DEC_MODE_AVS2 16
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

#if 0
static const u32 reg_mask[33] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
    0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
    0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
    0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
    0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
    0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF};
#endif

static const u32 ref_ybase_msb[16] = {
    HWIF_REFER0_YBASE_MSB,  HWIF_REFER1_YBASE_MSB,  HWIF_REFER2_YBASE_MSB,
    HWIF_REFER3_YBASE_MSB,  HWIF_REFER4_YBASE_MSB,  HWIF_REFER5_YBASE_MSB,
    HWIF_REFER6_YBASE_MSB,  HWIF_REFER7_YBASE_MSB,  HWIF_REFER8_YBASE_MSB,
    HWIF_REFER9_YBASE_MSB,  HWIF_REFER10_YBASE_MSB, HWIF_REFER11_YBASE_MSB,
    HWIF_REFER12_YBASE_MSB, HWIF_REFER13_YBASE_MSB, HWIF_REFER14_YBASE_MSB,
    HWIF_REFER15_YBASE_MSB};

static const u32 ref_cbase_msb[16] = {
    HWIF_REFER0_CBASE_MSB,  HWIF_REFER1_CBASE_MSB,  HWIF_REFER2_CBASE_MSB,
    HWIF_REFER3_CBASE_MSB,  HWIF_REFER4_CBASE_MSB,  HWIF_REFER5_CBASE_MSB,
    HWIF_REFER6_CBASE_MSB,  HWIF_REFER7_CBASE_MSB,  HWIF_REFER8_CBASE_MSB,
    HWIF_REFER9_CBASE_MSB,  HWIF_REFER10_CBASE_MSB, HWIF_REFER11_CBASE_MSB,
    HWIF_REFER12_CBASE_MSB, HWIF_REFER13_CBASE_MSB, HWIF_REFER14_CBASE_MSB,
    HWIF_REFER15_CBASE_MSB};

static const u32 ref_tybase[16] = {
    HWIF_REFER0_TYBASE_LSB,  HWIF_REFER1_TYBASE_LSB,  HWIF_REFER2_TYBASE_LSB,
    HWIF_REFER3_TYBASE_LSB,  HWIF_REFER4_TYBASE_LSB,  HWIF_REFER5_TYBASE_LSB,
    HWIF_REFER6_TYBASE_LSB,  HWIF_REFER7_TYBASE_LSB,  HWIF_REFER8_TYBASE_LSB,
    HWIF_REFER9_TYBASE_LSB,  HWIF_REFER10_TYBASE_LSB, HWIF_REFER11_TYBASE_LSB,
    HWIF_REFER12_TYBASE_LSB, HWIF_REFER13_TYBASE_LSB, HWIF_REFER14_TYBASE_LSB,
    HWIF_REFER15_TYBASE_LSB};
static const u32 ref_tcbase[16] = {
    HWIF_REFER0_TCBASE_LSB,  HWIF_REFER1_TCBASE_LSB,  HWIF_REFER2_TCBASE_LSB,
    HWIF_REFER3_TCBASE_LSB,  HWIF_REFER4_TCBASE_LSB,  HWIF_REFER5_TCBASE_LSB,
    HWIF_REFER6_TCBASE_LSB,  HWIF_REFER7_TCBASE_LSB,  HWIF_REFER8_TCBASE_LSB,
    HWIF_REFER9_TCBASE_LSB,  HWIF_REFER10_TCBASE_LSB, HWIF_REFER11_TCBASE_LSB,
    HWIF_REFER12_TCBASE_LSB, HWIF_REFER13_TCBASE_LSB, HWIF_REFER14_TCBASE_LSB,
    HWIF_REFER15_TCBASE_LSB};

static const u32 ref_ybase[16] = {
    HWIF_REFER0_YBASE_LSB,  HWIF_REFER1_YBASE_LSB,  HWIF_REFER2_YBASE_LSB,
    HWIF_REFER3_YBASE_LSB,  HWIF_REFER4_YBASE_LSB,  HWIF_REFER5_YBASE_LSB,
    HWIF_REFER6_YBASE_LSB,  HWIF_REFER7_YBASE_LSB,  HWIF_REFER8_YBASE_LSB,
    HWIF_REFER9_YBASE_LSB,  HWIF_REFER10_YBASE_LSB, HWIF_REFER11_YBASE_LSB,
    HWIF_REFER12_YBASE_LSB, HWIF_REFER13_YBASE_LSB, HWIF_REFER14_YBASE_LSB,
    HWIF_REFER15_YBASE_LSB};

static const u32 ref_cbase[16] = {
    HWIF_REFER0_CBASE_LSB,  HWIF_REFER1_CBASE_LSB,  HWIF_REFER2_CBASE_LSB,
    HWIF_REFER3_CBASE_LSB,  HWIF_REFER4_CBASE_LSB,  HWIF_REFER5_CBASE_LSB,
    HWIF_REFER6_CBASE_LSB,  HWIF_REFER7_CBASE_LSB,  HWIF_REFER8_CBASE_LSB,
    HWIF_REFER9_CBASE_LSB,  HWIF_REFER10_CBASE_LSB, HWIF_REFER11_CBASE_LSB,
    HWIF_REFER12_CBASE_LSB, HWIF_REFER13_CBASE_LSB, HWIF_REFER14_CBASE_LSB,
    HWIF_REFER15_CBASE_LSB};

static const u32 ref_tybase_msb[16] = {
    HWIF_REFER0_TYBASE_MSB,  HWIF_REFER1_TYBASE_MSB,  HWIF_REFER2_TYBASE_MSB,
    HWIF_REFER3_TYBASE_MSB,  HWIF_REFER4_TYBASE_MSB,  HWIF_REFER5_TYBASE_MSB,
    HWIF_REFER6_TYBASE_MSB,  HWIF_REFER7_TYBASE_MSB,  HWIF_REFER8_TYBASE_MSB,
    HWIF_REFER9_TYBASE_MSB,  HWIF_REFER10_TYBASE_MSB, HWIF_REFER11_TYBASE_MSB,
    HWIF_REFER12_TYBASE_MSB, HWIF_REFER13_TYBASE_MSB, HWIF_REFER14_TYBASE_MSB,
    HWIF_REFER15_TYBASE_MSB};
static const u32 ref_tcbase_msb[16] = {
    HWIF_REFER0_TCBASE_MSB,  HWIF_REFER1_TCBASE_MSB,  HWIF_REFER2_TCBASE_MSB,
    HWIF_REFER3_TCBASE_MSB,  HWIF_REFER4_TCBASE_MSB,  HWIF_REFER5_TCBASE_MSB,
    HWIF_REFER6_TCBASE_MSB,  HWIF_REFER7_TCBASE_MSB,  HWIF_REFER8_TCBASE_MSB,
    HWIF_REFER9_TCBASE_MSB,  HWIF_REFER10_TCBASE_MSB, HWIF_REFER11_TCBASE_MSB,
    HWIF_REFER12_TCBASE_MSB, HWIF_REFER13_TCBASE_MSB, HWIF_REFER14_TCBASE_MSB,
    HWIF_REFER15_TCBASE_MSB};

#if 0
static const u32 ref_poc_regs_avs2[16] = {
    HWIF_CUR_POC_00, HWIF_CUR_POC_01, HWIF_CUR_POC_02, HWIF_CUR_POC_03,
    HWIF_CUR_POC_04, HWIF_CUR_POC_05, HWIF_CUR_POC_06, HWIF_CUR_POC_07,
    HWIF_CUR_POC_08, HWIF_CUR_POC_09, HWIF_CUR_POC_10, HWIF_CUR_POC_11,
    HWIF_CUR_POC_12, HWIF_CUR_POC_13, HWIF_CUR_POC_14, HWIF_CUR_POC_15};
#endif

static const u32 ref_dbase[16] = {
    HWIF_REFER0_DBASE_LSB,  HWIF_REFER1_DBASE_LSB,  HWIF_REFER2_DBASE_LSB,
    HWIF_REFER3_DBASE_LSB,  HWIF_REFER4_DBASE_LSB,  HWIF_REFER5_DBASE_LSB,
    HWIF_REFER6_DBASE_LSB,  HWIF_REFER7_DBASE_LSB,  HWIF_REFER8_DBASE_LSB,
    HWIF_REFER9_DBASE_LSB,  HWIF_REFER10_DBASE_LSB, HWIF_REFER11_DBASE_LSB,
    HWIF_REFER12_DBASE_LSB, HWIF_REFER13_DBASE_LSB, HWIF_REFER14_DBASE_LSB,
    HWIF_REFER15_DBASE_LSB};

static const u32 ref_dbase_msb[16] = {
    HWIF_REFER0_DBASE_MSB,  HWIF_REFER1_DBASE_MSB,  HWIF_REFER2_DBASE_MSB,
    HWIF_REFER3_DBASE_MSB,  HWIF_REFER4_DBASE_MSB,  HWIF_REFER5_DBASE_MSB,
    HWIF_REFER6_DBASE_MSB,  HWIF_REFER7_DBASE_MSB,  HWIF_REFER8_DBASE_MSB,
    HWIF_REFER9_DBASE_MSB,  HWIF_REFER10_DBASE_MSB, HWIF_REFER11_DBASE_MSB,
    HWIF_REFER12_DBASE_MSB, HWIF_REFER13_DBASE_MSB, HWIF_REFER14_DBASE_MSB,
    HWIF_REFER15_DBASE_MSB};

#if 1
#define SET_REGISTER SetDecRegister
#else
#define SET_REGISTER(regs, id, value) \
  {                                   \
    printf("%s = %d\n", #id, value);  \
    SetDecRegister(regs, id, value);  \
  }
#endif

#define TILE_H 4

/* set sequence header realted parameters */
void Avs2SetSequenceRegs(struct Avs2Hwd *hwd, struct Avs2SeqParam *seq) {

  u32 *avs2_regs = hwd->regs;
  struct Avs2ConfigParam *cfg = hwd->cfg;

  // sw_ctrl->progressive_sequence = hd->progressive_sequence;
  // sw_ctrl->field_sequence = img->is_field_sequence;

  /* picture size */
  SET_REGISTER(avs2_regs, HWIF_PIC_INTERLACE_E, seq->is_field_sequence);
  SET_REGISTER(avs2_regs, HWIF_PIC_WIDTH_IN_CBS, seq->pic_width_in_cbs);
  SET_REGISTER(avs2_regs, HWIF_PIC_HEIGHT_IN_CBS, seq->pic_height_in_cbs);
  SET_REGISTER(avs2_regs, HWIF_PIC_WIDTH_4X4, seq->pic_width_in_cbs<<1);
  SET_REGISTER(avs2_regs, HWIF_PIC_HEIGHT_4X4, seq->pic_height_in_cbs<<1);
  SET_REGISTER(avs2_regs, HWIF_PARTIAL_CTB_X,
                 (seq->pic_width != seq->pic_width_in_cbs*8) ? 1 : 0);
  SET_REGISTER(avs2_regs, HWIF_PARTIAL_CTB_Y,
                 (seq->pic_height != seq->pic_height_in_cbs*8) ? 1 : 0);

  /* sample_precision */
  // sw_ctrl->output_bit_depth = input->output_bit_depth;
  SET_REGISTER(avs2_regs, HWIF_BIT_DEPTH_OUT_MINUS8, seq->output_bit_depth - 8);
  /* encoding_precision, luma and chroma use same config for avs2. */
  SET_REGISTER(avs2_regs, HWIF_BIT_DEPTH_Y_MINUS8, seq->sample_bit_depth - 8);
  SET_REGISTER(avs2_regs, HWIF_BIT_DEPTH_C_MINUS8, seq->sample_bit_depth - 8);
  /*stride */
  if (!cfg->use_video_compressor) {
    SET_REGISTER(
        avs2_regs, HWIF_DEC_OUT_Y_STRIDE,
        NEXT_MULTIPLE(TILE_H * seq->pic_width_in_cbs * 8 * seq->sample_bit_depth, ALIGN(hwd->align) * 8) / 8);
    SET_REGISTER(
        avs2_regs, HWIF_DEC_OUT_C_STRIDE,
        NEXT_MULTIPLE(TILE_H * seq->pic_width_in_cbs * 8 * seq->sample_bit_depth, ALIGN(hwd->align) * 8) / 8);
  } else {
    SET_REGISTER(
        avs2_regs, HWIF_DEC_OUT_Y_STRIDE,
        NEXT_MULTIPLE(8 * seq->pic_width_in_cbs * 8 * seq->sample_bit_depth, ALIGN(hwd->align) * 8) >> 6);
    SET_REGISTER(
        avs2_regs, HWIF_DEC_OUT_C_STRIDE,
        NEXT_MULTIPLE(4 * seq->pic_width_in_cbs * 8 * seq->sample_bit_depth, ALIGN(hwd->align) * 8) >> 6);
    /* If the size of CBS row >= 64KB, which means it's possible that the offset
       may overflow in EC table, set the EC output in word alignment. */
    if (RFC_MAY_OVERFLOW(seq->pic_width_in_cbs * 8, seq->sample_bit_depth))
      SET_REGISTER(avs2_regs, HWIF_EC_WORD_ALIGN, 1);
    else
      SET_REGISTER(avs2_regs, HWIF_EC_WORD_ALIGN, 0);
  }
  /* lcu_size */
  SET_REGISTER(avs2_regs, HWIF_MAX_CB_SIZE, seq->lcu_size_in_bit);
  SET_REGISTER(avs2_regs, HWIF_MIN_CB_SIZE, MIN_CU_SIZE_IN_BIT);
  /* weight_quant_enable_flag, redefined in picture header */
  /* !background_picture_disable */
  // sw_ctrl->background_picture_enable =  hd->background_picture_enable;
  /* dmh_enable is always 1, not defined in header now. */
  // sw_ctrl->ena_dmh = hd->b_dmh_enabled;
  SET_REGISTER(avs2_regs, HWIF_DMH_E, seq->b_dmh_enabled);
  /* multi_hypothesis_skip_enable_flag */
  // sw_ctrl->ena_mhpskip = hd->b_mhpskip_enabled;
  SET_REGISTER(avs2_regs, HWIF_MULTI_HYPO_SKIP_E, seq->b_mhpskip_enabled);
  /* dual_hypothesis_prediction_enable_flag */
  // sw_ctrl->ena_dhp = hd->dhp_enabled;
  SET_REGISTER(avs2_regs, HWIF_DUAL_HYPO_PRED_E, seq->dhp_enabled);
  /* weighted_skip_enable_flag */
  // sw_ctrl->ena_wsm = hd->wsm_enabled;
  SET_REGISTER(avs2_regs, HWIF_WEIGHTED_SKIP_E, seq->wsm_enabled);
  /* asymmetric_motion_partitions_enable_flags, FIXME: this flag isn't used. */
  SET_REGISTER(avs2_regs, HWIF_ASYM_PRED_E, seq->inter_amp_enable);
  /* nonsquare_quandtree_transform_enable_flag */
  // sw_ctrl->non_square_quad_tree_enable = input->useNSQT;
  SET_REGISTER(avs2_regs, HWIF_NONSQUARE_TRANSFORM_E, seq->useNSQT);
  /* nonsquare_intra_prediction_enable_flag */
  // sw_ctrl->ena_SDIP = input->useSDIP;
  SET_REGISTER(avs2_regs, HWIF_NONSQUARE_INTRA_PRED_E, seq->useSDIP);
  /* secondary_transform_enable_flag */
  // sw_ctrl->ena_2nd_transform = hd->b_secT_enabled;
  SET_REGISTER(avs2_regs, HWIF_SECOND_TRANSFORM_E, seq->b_secT_enabled);
  /* sample_adaptive_offset_enable_flag */
  SET_REGISTER(avs2_regs, HWIF_SAO_E, seq->sao_enable);
  /* adaptive_leveling_filter_enable_flag => final in picture header. */
  /* pmvr_enable_flag */
  // sw_ctrl->ena_pmvr = hd->b_pmvr_enabled;
  SET_REGISTER(avs2_regs, HWIF_PMVR_E, seq->b_pmvr_enabled);
  /* num_of_rcs => final in picture header. */
  /* cross_slice_loopfilter_enable_flag */
  SET_REGISTER(avs2_regs, HWIF_FILT_SLICE_BORDER, seq->cross_slice_loop_filter);
  /* alf */
  SET_REGISTER(avs2_regs, HWIF_ALF_ENABLE, seq->alf_enable);
  if (seq->alf_enable) {
    u32 lcu_size = 1 << seq->lcu_size_in_bit;
    u32 alf_interval_x =
        ((((seq->pic_width_in_cbs * 8 + lcu_size - 1) / lcu_size) + 1) / 4);
    u32 alf_interval_y =
        ((((seq->pic_height_in_cbs * 8 + lcu_size - 1) / lcu_size) + 1) / 4);
    SET_REGISTER(avs2_regs, HWIF_ALF_INTERVAL_X, alf_interval_x);
    SET_REGISTER(avs2_regs, HWIF_ALF_INTERVAL_Y, alf_interval_y);
  }
  /*bcbr enable */
  seq->bcbr_enable = 0;
  SET_REGISTER(avs2_regs, HWIF_BCBR_ENABLE, seq->bcbr_enable);
  /*bg enable */
  SET_REGISTER(avs2_regs, HWIF_BACKGROUND_E, seq->background_picture_enable);
}

void Avs2SetConfigRegs(struct Avs2Hwd *hwd) {
  struct Avs2ConfigParam *cfg = hwd->cfg;
  u32 *avs2_regs = hwd->regs;

  SET_REGISTER(avs2_regs, HWIF_DEC_OUT_DIS, cfg->disable_out_writing);
  SET_REGISTER(avs2_regs, HWIF_DEC_OUT_EC_BYPASS, !cfg->use_video_compressor);
}

void Avs2SetPictureHeaderRegs(struct Avs2Hwd *hwd, struct Avs2PicParam *pps) {

  u32 *avs2_regs = hwd->regs;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  // sw_ctrl->pic_type = img->type;
  SET_REGISTER(avs2_regs, HWIF_PIC_TYPE, pps->type);
  // sw_ctrl->pic_typeb = img->typeb;
  SET_REGISTER(avs2_regs, HWIF_PIC_TYPEB,
               (pps->typeb == BP_IMG)
                   ? (2)
                   : ((pps->typeb == BACKGROUND_IMG) ? (1) : (0)));
  // sw_ctrl->background_reference_enable = hd->background_reference_enable;
  SET_REGISTER(avs2_regs, HWIF_BACKGROUND_REF_E,
               pps->background_reference_enable);
  /* pic_distance is derived from doi and output_delay */
  // sw_ctrl->pic_distance = img->pic_distance;
  // assert(img->pic_distance==pps->poc);
  SET_REGISTER(avs2_regs, HWIF_PIC_DISTANCE, pps->poc);
  // sw_ctrl->imgtr_next_p = img->imgtr_next_P;
  // SET_REGISTER(avs2_regs, HWIF_POC_NEXT, img->imgtr_next_P);
  /* rcs (rps) setting. */
  // sw_ctrl->rps_refnum = hd->curr_RPS.num_of_ref;
  SET_REGISTER(avs2_regs, HWIF_REF_NUM, pps->rps.num_of_ref);
  /* progressive_frame */
  // REMOVE:sw_ctrl->progressive_frame = hd->progressive_frame; //NOTUSED:
  // REMOVE:sw_ctrl->picture_structure = img->picture_structure; //NOTUSED:
  // sw_ctrl->is_top_field = img->is_top_field;
  SET_REGISTER(avs2_regs, HWIF_PIC_TOPFIELD_E, pps->is_top_field);
  // SET_REGISTER(avs2_regs, HWIF_TOPFIELDFIRST_E, hd->top_field_first);
  // //FIXME: usage?
  /* fixed_picture_qp_flag */
  // DQP<=sw_ctrl->fixed_picture_qp = hd->fixed_picture_qp;
  SET_REGISTER(avs2_regs, HWIF_PIC_FIXED_QUANT, pps->fixed_picture_qp);
  /* picture_qp, or slice_qp */
  SET_REGISTER(avs2_regs, HWIF_INIT_QP, pps->picture_qp);

  /* loop_filter_disable_flag, deblock filter */
  SET_REGISTER(avs2_regs, HWIF_FILTERING_DIS, pps->loop_filter_disable);
  SET_REGISTER(avs2_regs, HWIF_FILT_OFFSET_TC, pps->alpha_c_offset);
  SET_REGISTER(avs2_regs, HWIF_FILT_OFFSET_BETA, pps->beta_offset);
  /* chroma_quant_params */
  // sw_ctrl->cb_qp_offset = hd->chroma_quant_param_delta_u;
  SET_REGISTER(avs2_regs, HWIF_CB_QP_DELTA, pps->chroma_quant_param_delta_u);
  // sw_ctrl->cr_qp_offset = hd->chroma_quant_param_delta_v;
  SET_REGISTER(avs2_regs, HWIF_CR_QP_DELTA, pps->chroma_quant_param_delta_v);
  /* weight quant */
  SET_REGISTER(avs2_regs, HWIF_SCALING_LIST_E,
               pps->pic_weight_quant_enable_flag);
  SET_ADDR_REG(avs2_regs, HWIF_SCALE_LIST_BASE,
               hwd->cmems->wqm_tbl.bus_address);
  /* alf filter */
  SET_REGISTER(avs2_regs, HWIF_ALF_ON_Y, pps->alf_flag[0]);
  SET_REGISTER(avs2_regs, HWIF_ALF_ON_U, pps->alf_flag[1]);
  SET_REGISTER(avs2_regs, HWIF_ALF_ON_V, pps->alf_flag[2]);

  SET_REGISTER(avs2_regs, HWIF_RPS_REFERD_E, pps->rps.referd_by_others);

  SET_ADDR_REG(avs2_regs, HWIF_ALF_TABLE_BASE,
               (addr_t)hwd->cmems->alf_tbl.bus_address);

  SET_REGISTER(avs2_regs, HWIF_TIME_REF, pps->poc);  // signed 10 bit

  /* set other control output */
}

void Avs2SetStreamRegs(struct Avs2Hwd *hwd) {
  u32 *avs2_regs = hwd->regs;
  struct Avs2StreamParam *stream = hwd->stream;
  u32 bit_pos, stream_length;
  addr_t stream_base;
  u32 tmp=0;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if (hwd->cfg->start_code_detected)
      tmp = 1;

  SetDecRegister(avs2_regs, HWIF_START_CODE_E, tmp);

  /* bit offset if base is unaligned */
  bit_pos = ((stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address)))&(DEC_HW_ALIGN_MASK))*8;
  if (hwd->cfg->start_code_detected)
  {
    bit_pos = ((stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address)) - 3)&(DEC_HW_ALIGN_MASK))*8;
  }
  else
  {
    bit_pos = ((stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address)))&(DEC_HW_ALIGN_MASK))*8;
  }
  SET_REGISTER(avs2_regs, HWIF_STRM_START_BIT, bit_pos);

  /* stream */
  if(stream->is_rb) {
    /* ring buffer setting: start - HWIF_STREAM_BASE; len - HWIF_STRM_BUFFER_LEN
     */

    SET_ADDR_REG(avs2_regs, HWIF_STREAM_BASE, stream->ring_buffer.bus_address);

    if(hwd->cfg->start_code_detected)
    {
      stream_length = stream->stream_length - stream->stream_offset + 3 + bit_pos / 8;
    }
    else
    {
      stream_length = stream->stream_length - stream->stream_offset + bit_pos / 8;
    }

    SET_REGISTER(avs2_regs, HWIF_STREAM_LEN, stream_length);

    /* ring buffer setting. */
    SET_REGISTER(avs2_regs, HWIF_STRM_BUFFER_LEN, stream->ring_buffer.size);
    if (hwd->cfg->start_code_detected)
    {
      tmp = (stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address))-3)&( ~DEC_HW_ALIGN_MASK);
    }
    else
    {
      tmp = (stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address)))&( ~DEC_HW_ALIGN_MASK);
    }
    SET_REGISTER(avs2_regs, HWIF_STRM_START_OFFSET, tmp);

  } else {
    stream_base = stream->ring_buffer.bus_address; /* unaligned base */
    stream_base &= (~DEC_HW_ALIGN_MASK);   /* align the base */
    ASSERT((stream_base & 0xF) == 0);
    SET_ADDR_REG(avs2_regs, HWIF_STREAM_BASE, stream_base);

    if(hwd->cfg->start_code_detected)
    {
      stream_length = stream->stream_length - stream->stream_offset + 3 + bit_pos / 8;
    }
    else
    {
      stream_length = stream->stream_length - stream->stream_offset + bit_pos / 8;
    }

    SET_REGISTER(avs2_regs, HWIF_STREAM_LEN, stream_length);

    /* ring buffer setting. */
    SET_REGISTER(avs2_regs, HWIF_STRM_BUFFER_LEN, stream->ring_buffer.size);
    if (hwd->cfg->start_code_detected)
    {
      tmp = (stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address))-3)&( ~DEC_HW_ALIGN_MASK);
    }
    else
    {
      tmp = (stream->stream_offset+((stream->stream_bus_addr-stream->ring_buffer.bus_address)))&( ~DEC_HW_ALIGN_MASK);
    }
    SET_REGISTER(avs2_regs, HWIF_STRM_START_OFFSET, tmp);
  }
}

void Avs2SetReferenceRegs(struct Avs2Hwd *hwd) {

  u32 *avs2_regs = hwd->regs;
  struct Avs2PicParam *pps = hwd->pps;
  int i;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* references number */
  u32 num_of_references = pps->rps.num_of_ref;
  if (pps->type == I_IMG && pps->typeb == BACKGROUND_IMG) {  // G/GB frame
    num_of_references = 0;
  }
  SET_REGISTER(avs2_regs, HWIF_REF_FRAMES, num_of_references);

  /* reference buffer */
  for (i = 0; i < MAXREF; i++) {
    SET_ADDR_REG2(avs2_regs, ref_ybase[i], ref_ybase_msb[i],
                  hwd->refs->ref[i].y.bus_address);
    SET_ADDR_REG2(avs2_regs, ref_cbase[i], ref_cbase_msb[i],
                  hwd->refs->ref[i].c.bus_address);

    SET_ADDR_REG2(avs2_regs, ref_tybase[i], ref_tybase_msb[i],
                  hwd->refs->ref[i].y_tbl.bus_address);
    SET_ADDR_REG2(avs2_regs, ref_tcbase[i], ref_tcbase_msb[i],
                  hwd->refs->ref[i].c_tbl.bus_address);
    // col mv
    SET_ADDR_REG2(avs2_regs, ref_dbase[i], ref_dbase_msb[i],
                  hwd->refs->ref[i].mv.bus_address);                  
  }


  // sw_ctrl->imgtr_next_p = img->imgtr_next_P;
  // int imgtr_next_p = pps->type == B_IMG ?
  // hwd->refs->ref[0].imgtr_fwRefDistance : pps->poc;
  // SET_REGISTER(avs2_regs, HWIF_POC_NEXT, img->imgtr_next_P);

  // POI distance for each reference frame, signed 10 bit
  SET_REGISTER(avs2_regs, HWIF_REF_DIST0, hwd->refs->ref[0].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST1, hwd->refs->ref[1].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST2, hwd->refs->ref[2].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST3, hwd->refs->ref[3].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST4, hwd->refs->ref[4].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST5, hwd->refs->ref[5].img_poi);
  SET_REGISTER(avs2_regs, HWIF_REF_DIST6, hwd->refs->ref[6].img_poi);

  // distance of reference frame 0's reference
  SET_REGISTER(avs2_regs, HWIF_REF0_POC0, hwd->refs->ref[0].ref_poc[0]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC1, hwd->refs->ref[0].ref_poc[1]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC2, hwd->refs->ref[0].ref_poc[2]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC3, hwd->refs->ref[0].ref_poc[3]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC4, hwd->refs->ref[0].ref_poc[4]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC5, hwd->refs->ref[0].ref_poc[5]);
  SET_REGISTER(avs2_regs, HWIF_REF0_POC6, hwd->refs->ref[0].ref_poc[6]);
  // SET_REGISTER(avs2_regs, HWIF_REF0_POC7, fref[0]->ref_poc[7]);

  // background reference
  SET_ADDR_REG2(avs2_regs, ref_ybase[MAXREF], ref_ybase_msb[MAXREF],
                hwd->refs->background.y.bus_address);
  SET_ADDR_REG2(avs2_regs, ref_cbase[MAXREF], ref_cbase_msb[MAXREF],
                hwd->refs->background.c.bus_address);

  SET_ADDR_REG2(avs2_regs, ref_tybase[MAXREF], ref_tybase_msb[MAXREF],
                hwd->refs->background.y_tbl.bus_address);
  SET_ADDR_REG2(avs2_regs, ref_tcbase[MAXREF], ref_tcbase_msb[MAXREF],
                hwd->refs->background.c_tbl.bus_address);
  // col mv
  SET_ADDR_REG2(avs2_regs, ref_dbase[MAXREF], ref_dbase_msb[MAXREF],
                hwd->refs->background.mv.bus_address);
  SET_REGISTER(avs2_regs, HWIF_BACKGROUND_IS_TOP, hwd->bk_img_is_top_field);
}

void Avs2SetOutputRegs(struct Avs2Hwd *hwd) {
  u32 *avs2_regs = hwd->regs;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_AVS2_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* output */
  // sw_ctrl->out_y_stride = GetDecRegister(reg_base, HWIF_DEC_OUT_Y_STRIDE);
  // sw_ctrl->out_c_stride = GetDecRegister(reg_base, HWIF_DEC_OUT_C_STRIDE);
  /* raster output */
  // sw_ctrl->raster_y = (u8 *)GET_ADDR_REG(reg_base, HWIF_DEC_RSY_BASE);
  // sw_ctrl->raster_c = (u8 *)GET_ADDR_REG(reg_base, HWIF_DEC_RSC_BASE);

  /* recon */
  SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_YBASE, hwd->recon->y.bus_address);
  SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_CBASE, hwd->recon->c.bus_address);
  SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_TYBASE, hwd->recon->y_tbl.bus_address);
  SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_TCBASE, hwd->recon->c_tbl.bus_address);

  /* not output col-mv for G/GB frame or not reference frame. */
  if ((hwd->pps->type == I_IMG && hwd->pps->typeb == BACKGROUND_IMG)
     || (hwd->pps->type != I_IMG && hwd->pps->rps.referd_by_others == 0) ) {
    SetDecRegister(avs2_regs, HWIF_WRITE_MVS_E, 0);
    SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_DBASE, hwd->recon->mv.bus_address);//for multi core status
  } else {
    SetDecRegister(avs2_regs, HWIF_WRITE_MVS_E, 1);
    SET_ADDR_REG(avs2_regs, HWIF_DEC_OUT_DBASE, hwd->recon->mv.bus_address);
  }
}

void Avs2SetModeRegs(struct Avs2Hwd *hwd) {
  u32 *avs2_regs = hwd->regs;

  /* should check feature firstly */
  avs2_regs[0] = 0x8001 << 16;  // 0x6732<<16;

  /* set as AVS2 mode */
  SET_REGISTER(avs2_regs, HWIF_DEC_MODE, DEC_MODE_AVS2);
}

void Avs2SetFeaturesRegs(struct Avs2Hwd *hwd) {
  u32 *avs2_regs = hwd->regs;
  /* config */
  assert(hwd->sps->chroma_format == 1);
  SET_REGISTER(avs2_regs, HWIF_BLACKWHITE_E,
               hwd->sps->chroma_format == 1 ? 0 : 1);
}

void Avs2SetPpRegs(struct Avs2Hwd *hwd) {
  u32 *avs2_regs = hwd->regs;
  /* config */
  SET_REGISTER(avs2_regs, HWIF_PP_OUT_E_U, hwd->pp_enabled);
  SET_REGISTER(avs2_regs, HWIF_TOPFIELDFIRST_E, hwd->ppout->top_field_first);
  PPSetRegs(avs2_regs, hwd->ppout->hw_feature, hwd->ppout->ppu_cfg,
            hwd->ppout->pp_buffer->bus_address, 0, hwd->ppout->bottom_flag);
}
void Avs2SetRegs(struct Avs2Hwd *hwd) {

  /* set work mode as AVS2 decoder */
  Avs2SetModeRegs(hwd);
  /* configure */
  Avs2SetConfigRegs(hwd);
  /* set features */
  Avs2SetFeaturesRegs(hwd);
  /* sequence features */
  Avs2SetSequenceRegs(hwd, hwd->sps);
  /* picture features */
  Avs2SetPictureHeaderRegs(hwd, hwd->pps);
  /* set stream buffer */
  Avs2SetStreamRegs(hwd);
  /* set buffer pointers */
  Avs2SetReferenceRegs(hwd);
  /* set recon and output */
  Avs2SetOutputRegs(hwd);
  /* set pp parameters */
  if (hwd->pp_enabled) Avs2SetPpRegs(hwd);
}

