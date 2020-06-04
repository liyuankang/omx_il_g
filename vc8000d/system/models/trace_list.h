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

 /* APF output */
 TRACEDESC(APF_FETCH,                   "apf_fetch.trc", 0, 0, 0, 0)
 TRACEDESC(APF_BUS_LOAD,                "apf_bus_load.trc", 0, 0, 0, 0)
 TRACEDESC(APF_REF_BIN,                 "apf_ref.bin", 0, 1, 0, 0)
 TRACEDESC(APF_REF_CTRL_BIN,            "apf_ref_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(APF_PART_CTRL_BIN,           "apf_part_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(APF_TRANSACT_BIN,            "apf_transact.bin", 0, 1, 0, 0)
 TRACEDESC(APF_PARTITIONS,              "apf_partitions.trc", 0, 0, 0, 0)
 TRACEDESC(APF_EC_REF_BIN,              "apf_ec_ref.bin", 0, 1, 0, 0)
 TRACEDESC(APF_EDC_REF_BIN,             "apf_edc_ref_4tile.bin", 0, 1, 0, 0)

 /* EMD */
 TRACEDESC(CABAC_CTX_BIN,               "g2_input_probtabs.bin", 0, 1, 0, 0)
 TRACEDESC(CU_CTRL_EMD0_BIN,           "cu_ctrl_emd0.bin", 0, 1, 0, 0)
 TRACEDESC(CU_CTRL_EMD1_BIN,           "cu_ctrl_emd1.bin", 0, 1, 0, 0)
#if 1
 TRACEDESC(PICTURE_CTRL_DEC_TILED_TRC, "picture_ctrl_dec_tiled.trc", 1, 0, 0, 0)
 TRACEDESC(PICTURE_CTRL_DEC,            "picture_ctrl_dec.trc", 1, 0, 0, 0)
 TRACEDESC(SEQUENCE_CTRL,               "sequence_ctrl.trc", 1, 0, 0, 0)
 TRACEDESC(SIMULATION_CTRL,             "simulation_ctrl.trc", 1, 0, 0, 0)
 TRACEDESC(INPUT_STREAM,                "g2_input_strm.trc", 0, 0, 0, 0)
 TRACEDESC(INPUT_STREAM_BIN,            "g2_input_strm.bin", 0, 1, 0, 0)
 TRACEDESC(STREAM_CONTROL_TRC,          "stream_control.trc", 1, 0, 0, 0)
 TRACEDESC(STREAM_CTRL_BIN,             "g2_stream_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(STREAM_CTRL_OUT_BIN,         "g2_stream_ctrl_out.bin", 0, 1, 0, 0)
 TRACEDESC(TILES_BIN,                   "g2_input_tiles.bin", 0, 1, 0, 0)
 TRACEDESC(OUTPUT_PIC,                  "g2_output_dec.bin", 0, 1, 0, 0)
 TRACEDESC(INPUT_DPB_BIN,               "g2_input_dpb.bin", 0, 1, 0, 0)
 TRACEDESC(STREAM_TXT,                  "stream.txt", 0, 0, 0, 0)

 TRACEDESC(H264_COEFFS_BIN,  "h264_coeffs.bin", 0, 1, 0, 0)
 TRACEDESC(H264_DIFF_MV_BIN,  "h264_diff_mv.bin", 0, 1,  0,0)
 TRACEDESC(H264_DIFF_MV_FIXED_BIN,  "h264_diff_mv_fixed.bin", 0, 1,  0,0)
 TRACEDESC(H264_FAKE_SAO_OUT,  "h264_fakesao2pp_data.bin", 0, 1,  0,0)
 TRACEDESC(H264_FILTER_CTRL_BIN,  "h264_pred2filterd_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(H264_FILTER_CTRL_OUT,  "h264_deb2fakesao_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(H264_FILTER_OUT,  "h264_deb2fakesao_data.bin", 0, 1,  0,0)
 TRACEDESC(H264_IQT_CTRL_OUT,  "h264_iqt2pred_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(H264_IQT_DATA_OUT,  "h264_iqt2pred_data.bin", 0, 1,  0,0)
 TRACEDESC(H264_MB_CTRL_BIN,  "h264_mb_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(H264_MB_CTRL_MVD_BIN,  "h264_mb_ctrl_mvd.bin", 0, 1,  0,0)
 TRACEDESC(H264_MV_CTRL_BIN,  "h264_mv_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_DMVIN_BIN,  "h264_mvd_dmvin.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_DMVINOFFSET_BIN,  "h264_mvd_dmvinoffset.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_DMVOUT_BIN,  "h264_mvd_dmvout.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_DMVOUTOFFSET_BIN,  "h264_mvd_dmvoutoffset.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_POC_BIN,  "h264_mvd_poc.bin", 0, 1,  0,0)
 TRACEDESC(H264_MVD_WT_BIN,  "h264_mvd2pred_wt.bin", 0, 1,  0,0)
 TRACEDESC(H264_RECON_TILE,  "h264_pred2filterd_data.bin", 0, 1,  0,0)
 TRACEDESC(H264_REF_PICID_LIST_BIN,  "h264_ref_picid_list.bin", 0, 1,  0,0)
 TRACEDESC(H264_SCALING_LIST_BIN,  "h264_scaling_list.bin", 0, 1,  0,0)
 TRACEDESC(H264_TU_CTRL_BIN,  "h264_tu_ctrl.bin", 0, 1,  0,0)
 TRACEDESC(LINE_CNT_BIN,  "line_cnt.bin", 0, 1,  0,0)
 TRACEDESC(MB_LOCATION_BIN,  "mb_location.bin", 0, 1,  0,0)
 TRACEDESC(MBCONTROL_HEX,  "mbcontrol.hex", 0, 1,  0,0)
 TRACEDESC(MBCONTROL_TRC,  "mbcontrol.trc", 0, 0,  0,0)
 TRACEDESC(MC_STATUS0_BIN,  "mc_sync_status0.bin", 0, 1,  0,0)
 TRACEDESC(MC_STATUS1_BIN,  "mc_sync_status1.bin", 0, 1,  0,0)
 TRACEDESC(MV_CTRL0_BIN,  "mv_ctrl0.bin", 0, 1,  0,0)
 TRACEDESC(MV_CTRL1_BIN,  "mv_ctrl1.bin", 0, 1,  0,0)
 TRACEDESC(MVSTATISTIC_TRC,  "mvStatistic.trc", 0, 0,  0,0)
 TRACEDESC(PPIN_CTRL_TILED4X4_BIN,  "ppin_ctrl_tiled4x4.bin", 0, 1,  0,0)
 TRACEDESC(PPIN_TILED4X4_BIN,  "ppin_tiled4x4.bin", 0, 1,  0,0)
 TRACEDESC(PPOUT_CTRL_DOWNSCALE_BIN,  "ppout_ctrl_downscale.bin", 0, 1,  0,0)
 TRACEDESC(PPOUT_DOWNSCALE_BIN,  "ppout_downscale.bin", 0, 1,  0,0)
 TRACEDESC(QPSTATISTIC_TRC,  "qpStatistic.trc", 0, 0,  0,0)
 TRACEDESC(SKIPSTATISTIC_TRC,  "skipStatistic.trc", 0, 0,  0,0)
 TRACEDESC(SLICE_SIZES_TRC,  "slice_sizes.trc", 0, 0,  0,0)
 TRACEDESC(STREAM_TRC,  "stream.trc", 1, 0,  0,0)
 TRACEDESC(UNIFIED_CTRL_TILED4X4_BIN,  "unified_ctrl_tiled4x4.bin", 0, 1,  0,0)
 TRACEDESC(UNIFIED_TILED4X4_BIN,  "unified_tiled4x4.bin", 0, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_CTRL_BIN,  "vc8000d_input_ctrl.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_DATA_BIN,  "vc8000d_input_data.bin", 0, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_DC_BIN,  "vc8000d_input_dc.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_INT4X4_BIN,  "vc8000d_input_int4x4.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_MOTVEC_BIN,  "vc8000d_input_motvec.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_SEGMENT_BIN,  "vc8000d_input_segment.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_INPUT_VC1_BITPL_MBCTRL_BIN,  "vc8000d_input_vc1_bitpl_mbctrl.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_OUTPUT_DIR_MV_BIN,  "vc8000d_output_dir_mode_mvs.bin", 1, 1,  0,0)
 TRACEDESC(VC8000D_OUTPUT_SEGMENT_BIN,  "vc8000d_output_segment.bin", 1, 1,  0,0)
 /* Exchange data */
 // SW=>HW, registers by fields
 TRACEDESC(SW_REGS_BIN,                 "sw_registers.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // SW=>HW register dump
 TRACEDESC(VC8000D_REGS_MMAP_BIN,       "vc8000d_regs_mmap.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // BusIf=>IQT
 TRACEDESC(SCALING_LISTS_BIN,           "g2_input_scalelist.bin", 0, 1, 0, HEVC|AVS2)
 TRACEDESC(H264_SCALING_OUT,            "h264_scaling_out.trc", 0, 0, 0, H264)
 // BusIf=>MVD collocated mv from reference picture
 TRACEDESC(DIR_MODE_MVS_IN_BIN,         "dir_mode_mvs_in.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // BusIf=>MVD
 TRACEDESC(DIR_MODE_MVS_BIN,            "g2_output_dir_mode_mvs.bin", 0, 1, 0, 0)  // (g_hw_ver <= 10001)
 TRACEDESC(DIR_MODE_MVS_OUT_OFFSET_BIN, "dir_mode_mvs_out_offset.bin", 0, 1, 0, 0)    // (g_hw_ver > 10001)
 // Busif=>ALF
 TRACEDESC(ALF_TABLES_BIN,              "vc8000d_input_alf_tables.bin", 1, 1, 0, AVS2)

 /* IQT output */
 // IQT=>INTRA
 TRACEDESC(IQT_OUT_BIN,                 "iqt_out.bin", 0, 1, 0, HEVC|VP9|AVS2)
 TRACEDESC(IQT_OUT_YUV,                 "iqt_out.yuv", 0, 0, 0, 0)
 TRACEDESC(H264_TRANSD_1RND,            "transd_1rnd.trc", 0, 0, 0, H264)

 /* BusIf output */
 // BusIf=>EMD
 TRACEDESC(EMD_STREAM_CTRL_BIN,         "emd_stream_ctrl.bin", 0, 1, 0, 0)

 /* EMD output */
 // EMD internal
 TRACEDESC(CABAC_BIN,                   "cabac_bin.trc", 0, 0, 0, HEVC)
 TRACEDESC(CABAC_BIN_BIN,               "cabac_bin.bin", 0, 1, 0, HEVC)
 TRACEDESC(CABAC_CTX,                   "cabac_ctx.trc", 0, 0, 0, HEVC)
 //TRACEDESC(CABAC_CTX_BIN,               "g2_input_probtabs.bin", 1, 1, 0, HEVC)
 TRACEDESC(CTX_CTR_INC_BIN,             "ctxctr_inc.bin", 0, 1, 0, VP9)
 TRACEDESC(SEGMENTID_EMD_IN_BIN,        "emd_segment_input.bin", 0, 1, 0, VP9)
 TRACEDESC(SEGMENTID_EMD_OUT_BIN,       "emd_segment_output.bin", 0, 1, 0, VP9)
 TRACEDESC(SEGMENTID_RASTER_BIN,        "g2_output_segment.bin", 0, 1, 0, VP9)
 TRACEDESC(SEGMENTID_RASTER_IN_BIN,     "g2_input_segment.bin", 0, 1, 0, VP9)
 // EMD=>IQT
 TRACEDESC(TU_CTRL_BIN,                 "tu_ctrl.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // EMD=>IQT
 TRACEDESC(CABAC_COEFFS_BIN,            "cabac_coeffs.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // EMD=>IQT
 TRACEDESC(EMD_RESIDUAL_BIN,            "emd_residual.bin", 0, 1, 0, VP9)
 // EMD=>IQT
 TRACEDESC(IQT_CTRL_BIN,                "iqt_ctrl.bin", 0, 1, 0, H264)
 // EMD=>MVD
 TRACEDESC(CU_CTRL_BIN,                 "cu_ctrl.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // EMD=>MVD
 TRACEDESC(CU_NO_RESI_BACK_FLAG_BIN,    "cu_no_resi_back_flag.bin", 0, 1, 0, VP9)
 // EMD=>MVD
 TRACEDESC(REF_CTRL_BIN,                "ref_list_ctrl.bin", 0, 1, 0, HEVC)
 // EMD=>MVD
 TRACEDESC(DMV_CTRL_BIN,                "dmv_ctrl.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // EMD=>SAO
 TRACEDESC(SAO_CTRL_BIN,                "sao_ctrl.bin", 0, 1, 0, HEVC|AVS2)
 // EMD=>ALF
 TRACEDESC(ALF_CTRL_BIN,                "alf_ctrl.bin", 0, 1, 0, AVS2)
 // EMD=>BusIf
 TRACEDESC(CTXCTR_OUT_BIN,              "g2_output_ctxctr.bin", 0, 1, 0, VP9)
 TRACEDESC(CTXCTR_OUT,                  "ctxctr_out.trc", 0, 0, 0, VP9)

 /* MVD output */
 // MVD=>BusIf output cololcated mv of current picture
 TRACEDESC(CV_CTRL_BIN,                 "cv_ctrl.bin", 0, 1, 0, HEVC)
 TRACEDESC(CV_DATA_BIN,                 "cv_data.bin", 0, 1, 0, HEVC|H264|AVS2)
 TRACEDESC(DMV_ORDERED_BIN,             "g2_output_dmv_ordered.bin", 0, 1, 0, 0)
 // MVD internal
 TRACEDESC(MV_NEIGHBORS,                "mv_neighbors.trc", 0, 0, 0, VP9)
 // MVD=>APF
 TRACEDESC(MV_CTRL_BIN,                 "mv_ctrl.bin", 0, 1, 0, 0)
 // MVD=>INTER
 TRACEDESC(MV_CTRL_TO_PRED_BIN,         "mv_ctrl_to_pred.bin", 0, 1, 0, HEVC|AVS2)
 // MVD=>APF
 TRACEDESC(CU_CTRL_EMD_BIN,             "cu_ctrl_emd.bin", 0, 1, 0, VP9|H264|HEVC)
 // MVD=>Top, output direct mvs.
 TRACEDESC(MVD_OUT_DIR_MVS_BIN,         "mvd_out_dir_mvs.bin", 0, 1, 0, HEVC|AVS2)
 /* INTER output */
 // INTER=>INTRA
 TRACEDESC(INTER_PRED_OUT_BIN,          "inter_pred_out.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 TRACEDESC(INTER_OUT_MV_CTRL_BIN,       "inter_out_mv_ctrl.bin", 0, 1, 0, "HEVC|VP9|H264|AVS2")

 /* INTRA output */
 // INTRA Internal: final pred YUV
 TRACEDESC(PRED_OUT_YUV,                "pred_out.yuv", 0, 0, 0, 0)
 // INTRA=>Deblock
 TRACEDESC(FILTERD_CTRL_BIN,            "filterd_ctrl.bin", 0, 1, 0, 0)
 // INTRA=>Deblock
 TRACEDESC(PRED_OUT_BIN,                "pred_out.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)

 /* Deblock output */
 // Deblock internal
 TRACEDESC(BSD_OUT_BIN,                 "bsd_out.bin", 0, 1, 0, 0)
 TRACEDESC(FILTERD_VER_BIN,             "filterd_ver.bin", 0, 1, 0, 0)
 TRACEDESC(FILTERD_TRC,                 "filterd.trc", 0, 0, 0, VP9)
 // Deblock=>SAO
 TRACEDESC(FILTERD_OUT_BIN,             "filterd_out.bin", 0, 1, 0, HEVC|VP9|AVS2)
 // Deblock->SAO
 TRACEDESC(FILTERD_OUT_BLKCTRL_BIN,     "filterd_out_blkctrl.bin", 0, 1, 0, AVS2)
 // Deblock=>SAO
 TRACEDESC(FILTERD_OUT_CTRL_TRC,        "filterd_out_ctrl.trc", 0, 0, 0, HEVC|VP9|AVS2)

 /* SAO output */
 // SAO=>PP/EC
 TRACEDESC(RECON_OUT_BIN,               "recon_out.bin", 0, 1, 0, HEVC|VP9)
 TRACEDESC(RECON_OUT_CTRL_BIN,          "recon_out_ctrl.bin", 0, 1, 0, HEVC|VP9)
 // SAO=>PP/EC
 TRACEDESC(RECON_OUT_SP_BIN,            "recon_out_sp.bin", 0, 1, 0, 0)
 TRACEDESC(RECON_OUT_SP_CTRL_BIN,       "recon_out_sp_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(PP_IN_BIN,                   "pp_in.bin", 0, 1, 0, 0)
 TRACEDESC(PP_IN_CTRL_BIN,              "pp_in_ctrl.bin", 0, 1, 0, 0)
 // SAO=>ALF
 TRACEDESC(SAO_OUT_CTRL_BIN,            "sao_out_ctrl.bin", 0, 1, 0, AVS2)
 TRACEDESC(SAO_OUT_DATA_BIN,            "sao_out_data.bin", 0, 1, 0, AVS2)

 /* ALF output */
 // ALF=>PP/EC
 TRACEDESC(ALF_OUT_CTRL_BIN,            "alf_out_pp_ctrl.bin", 0, 1, 0, AVS2)
 TRACEDESC(ALF_OUT_DATA_BIN,            "alf_out_pp_data.bin", 0, 1, 0, AVS2)

 /* PP output */
 TRACEDESC(PP_IN_Y_TILE_EDGE_BIN,       "pp_in_y_tile_edge.bin", 0, 1, 0, 0)
 TRACEDESC(PP_IN_C_TILE_EDGE_BIN,       "pp_in_c_tile_edge.bin", 0, 1, 0, 0)
 
 TRACEDESC(G2_OUTPUT_RASTER_BIN,        "g2_output_raster.bin", 0, 1, 0, 0)
 TRACEDESC(G2_RASTER_BIN,               "g2_raster.bin", 0, 1, 0, 0)
 TRACEDESC(PP_OUT_VSI_BIN,              "pp_out_vsi.bin", 0, 1, 0, 0)
 TRACEDESC(PP_OUT_CTRL_VSI_BIN,         "pp_out_vsi_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(PP_OUT_BIN,                  "pp_out.bin", 0, 1, 0, 0)
 TRACEDESC(PP_OUT_CTRL_BIN,             "pp_out_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(G2_DSCALE_BIN,               "g2_dscale.bin", 0, 1, 0, 0)
 TRACEDESC(G2_OUTPUT_DSCALE_BIN,        "g2_output_dscale.bin", 0, 1, 0, 0)

 /* statistic */
 TRACEDESC(SKIP_STA_TRC,                "skip_sta.trc", 0, 0, 0, 0)
 TRACEDESC(QP_STA_TRC,                  "qp_sta.trc", 0, 0, 0, 0)
 TRACEDESC(MV_STA_TRC,                  "mv_sta.trc", 0, 0, 0, 0)
 TRACEDESC(DECODING_TOOLS_TRC,          "decoding_tools.trc", 0, 0, 0, 0)
 //EMD=>?
 TRACEDESC(PIC_CTRL_BIN,                "g2_pic_ctrl.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)       // (g_hw_ver <= 10001)
 TRACEDESC(VC8000D_PIC_CTRL_BIN,        "vc8000d_pic_ctrl.bin", 0, 1, 0, 0)  // (g_hw_ver > 10001)

 /* misc */
 TRACEDESC(EDC_CBSR_CTRL_BIN,           "edc_cbsr_burst_ctrl.bin",0, 1, 0, 0)
 TRACEDESC(EC_OUT_BIN,                  "ec_out.bin", 0, 1, 0, 0)
 TRACEDESC(EC_OUT_CTRL_BIN,             "ec_out_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(EC_TABLE_BIN,                "ec_table.bin", 0, 1, 0, 0)
 TRACEDESC(EC_TABLE_CTRL_BIN,           "ec_table_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(EC_OUT_RTL_BIN,              "ec_out_rtl.bin", 0, 1, 0, 0)
 TRACEDESC(EC_TABLE_RTL_BIN,            "ec_table_rtl.bin", 0, 1, 0, 0)
 TRACEDESC(UPDATE_TABLE_TRANS_BIN,      "update_table_trans.bin", 0, 1, 0, 0)
 TRACEDESC(UPDATE_TABLE_DATA_BIN,       "update_table_data.bin", 0, 1, 0, 0)
 TRACEDESC(MISS_TABLE_TRANS_BIN,        "miss_table_trans.bin", 0, 1, 0, 0)
 TRACEDESC(MISS_TABLE_DATA_BIN,         "miss_table_data.bin", 0, 1, 0, 0)
 TRACEDESC(UPDATE_TABLE_AMOUNT_BIN,     "update_table_amount.bin", 0, 1, 0, 0)
 TRACEDESC(MISS_TABLE_AMOUNT_BIN,       "miss_table_amount.bin", 0, 1, 0, 0)
 TRACEDESC(G2_EC_OUT_BIN,               "g2_ec_out.bin", 0, 1, 0, 0)
 TRACEDESC(G2_EC_TABLE_BIN,             "g2_ec_table_out.bin", 0, 1, 0, 0)
 TRACEDESC(G2_EC_TILED_BIN,             "g2_ec_tiled.bin", 0, 1, 0, 0)
 TRACEDESC(G2_EC_PIC_INFO_TRC,          "g2_ec_pic_info.trc", 0, 1, 0, 0)
 TRACEDESC(G2_TILED_BIN,                "g2_tiled.bin", 0, 1, 0, 0)
 TRACEDESC(LINE_CNT,                    "line_cnt.bin", 0, 1, 0, 0)
 TRACEDESC(H264_IMPLICIT_WEIGHT_BIN,    "h264_weight.bin", 0, 1, 0, 0)
 TRACEDESC(H264_EXPLICIT_WEIGHT_BIN,    "h264_exp_weight.bin", 0, 1, 0, 0)
 TRACEDESC(H264_POC_BIN,                "h264_poc.bin", 0, 1, 0, 0)
 TRACEDESC(H264_DEB_SAO_CTRL_BIN,       "h264_deb_sao_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(H264_PRED_DEB_CTRL_INTER_BIN,"h264_pred_deb_ctrl_inter.bin", 0, 1, 0, 0)
 TRACEDESC(H264_RUN_LEVEL_BIN,          "h264_run_level.bin", 0, 1, 0, 0)
 TRACEDESC(DIR_MODE_MVS_IN_OFFSET_BIN,  "dir_mode_mvs_in_offset.bin", 0, 1, 0, 0)
 
  /* VC8000D extension */
  /* block level: pp standalone input/output from external buffer */
 TRACEDESC(PP_PREFETCH_IN_BIN,       "pp_prefetch_in.bin", 0, 1, 0, 0)
 TRACEDESC(PP_PREFETCH_OUT_BIN,      "pp_prefetch_out.bin", 0, 1, 0, 0)
 TRACEDESC(PP_PREFETCH_OUT_CTRL_BIN, "pp_prefetch_out_ctrl.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_INPUT_TABLES_BIN, "vc8000d_input_tables.bin", 1, 1, 0, 0)
  /* standalone top-level input */
 TRACEDESC(VC8000D_INPUT_PP_BIN,      "vc8000d_input_pp.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_INPUT_PP_CTRL_BIN, "vc8000d_input_pp_ctrl.bin", 1, 1, 0, 0)
  /* pipeline mode */
 // BusIf=>EMD
 TRACEDESC(VC8000D_INPUT_STRM_128BIT_SWAP_BIN,          "vc8000d_input_strm_128bit_swap.bin", 0, 1, 0, AVS2)
 TRACEDESC(VC8000D_INPUT_STRM_BIN,          "vc8000d_input_strm.bin", 1, 1, 0, HEVC|VP9|H264|AVS2)
 TRACEDESC(SWREG_ACCESSES_TRC,              "swreg_accesses.trc", 1, 0, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_DEC_BIN,          "vc8000d_output_dec.bin", 1, 1, 0, 0)
 // BusIf=>EMD
 TRACEDESC(VC8000D_INPUT_STREAM,            "vc8000d_input_strm.trc", 0, 0, 0, HEVC|VP9|H264|AVS2)
 // BusIf=>EMD
 TRACEDESC(VC8000D_STREAM_CTRL_BIN,         "vc8000d_stream_ctrl.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 // BusIf=>EMD
 TRACEDESC(VC8000D_STREAM_CTRL_OUT_BIN,     "vc8000d_stream_ctrl_out.bin", 0, 1, 0, HEVC|VP9|H264|AVS2)
 TRACEDESC(VC8000D_SCALING_LISTS_BIN,       "vc8000d_input_scalelist.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_TILES_BIN,               "vc8000d_input_tiles.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PIC,              "vc8000d_output_dec.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_INPUT_DPB_BIN,           "vc8000d_input_dpb.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_CABAC_CTX_BIN,           "vc8000d_input_probtabs.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_CTXCTR_OUT_BIN,          "vc8000d_output_ctxctr.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_SEGMENTID_RASTER_BIN,    "vc8000d_output_segment.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_SEGMENTID_RASTER_IN_BIN, "vc8000d_input_segment.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_DMV_ORDERED_BIN,         "vc8000d_output_dmv_ordered.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_RASTER_BIN,       "vc8000d_output_raster.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_RASTER_BIN,              "vc8000d_raster.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_DSCALE_BIN,              "vc8000d_dscale.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PP_BIN,           "vc8000d_output_pp0.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PP1_BIN,          "vc8000d_output_pp1.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PP2_BIN,          "vc8000d_output_pp2.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PP3_BIN,          "vc8000d_output_pp3.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_PP4_BIN,          "vc8000d_output_pp4.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_EC_OUT_BIN,              "vc8000d_ec_out.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_OUTPUT_RFC_TBL_BIN,      "vc8000d_output_rfc_tbl.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_EC_TILED_BIN,            "vc8000d_ec_tiled.bin", 1, 1, 0, 0)
 TRACEDESC(VC8000D_EC_PIC_INFO_TRC,         "vc8000d_ec_pic_info.trc", 1, 1, 0, 0)
 TRACEDESC(VC8000D_TILED_BIN,               "vc8000d_tiled.bin", 0, 1, 0, 0)
 TRACEDESC(VC8000D_TRACES_LENGTH,           "vc8000d_traces_len.trc", 1, 0, 0, 0)
 TRACEDESC(VC8000D_PP_LANCZOS_TABLE,       "vc8000d_pp0_lan_table.trc", 1, 1, 0, 0)
 TRACEDESC(VC8000D_PP1_LANCZOS_TABLE,       "vc8000d_pp1_lan_table.trc", 1, 1, 0, 0)
 TRACEDESC(VC8000D_PP2_LANCZOS_TABLE,       "vc8000d_pp2_lan_table.trc", 1, 1, 0, 0)
 TRACEDESC(VC8000D_PP3_LANCZOS_TABLE,       "vc8000d_pp3_lan_table.trc", 1, 1, 0, 0)
 TRACEDESC(VC8000D_PP4_LANCZOS_TABLE,       "vc8000d_pp4_lan_table.trc", 1, 1, 0, 0)
#endif  


