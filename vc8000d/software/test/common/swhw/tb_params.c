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

#ifdef _USE_VSI_STRING_
#include "vsi_string.h"
#else
#include <string.h>
#endif
#include <stdlib.h>
#include "tb_cfg.h"
#include "deccfg.h"
#include "ppcfg.h"
#include "dwl.h"
#include "refbuffer.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#endif
#include "regdrv.h"

#ifdef _ASSERT_USED
#include <assert.h>
#define ASSERT(expr) assert(expr)
#else
#ifdef _USE_VSI_STRING_
#define ASSERT(expr)
#else
#define ASSERT(expr)                                                        \
  if (!(expr)) {                                                            \
    printf("assert failed, file: %s line: %d :: %s.\n", __FILE__, __LINE__, \
           #expr);                                                          \
    abort();                                                                \
  }
#endif
#endif

/*------------------------------------------------------------------------------
    Blocks
------------------------------------------------------------------------------*/
#define BLOCK_TB_PARAMS ("TbParams")
#define BLOCK_DEC_PARAMS ("DecParams")
#define BLOCK_PP_PARAMS ("PpParams")
#define BLOCK_PP_UNIT_PARAMS ("PpUnitParams")

/*------------------------------------------------------------------------------
    Keys
------------------------------------------------------------------------------*/
#define KEY_PACKET_BY_PACKET      ("PacketByPacket")
#define KEY_NAL_UNIT_STREAM       ("NalUnitStream")
#define KEY_SEED_RND              ("SeedRnd")
#define KEY_STREAM_BIT_SWAP       ("StreamBitSwap")
#define KEY_STREAM_BIT_LOSS       ("StreamBitLoss")
#define KEY_STREAM_PACKET_LOSS    ("StreamPacketLoss")
#define KEY_STREAM_HEADER_CORRUPT ("StreamHeaderCorrupt")
#define KEY_STREAM_TRUNCATE       ("StreamTruncate")
#define KEY_SLICE_UD_IN_PACKET    ("SliceUdInPacket")
#define KEY_FIRST_TRACE_FRAME     ("FirstTraceFrame")
#define KEY_EXTRA_CU_CTRL_EOF     ("ExtraCuCtrlEof")

#define KEY_REFBU_ENABLED         ("refbuEnable")
#define KEY_REFBU_DIS_INTERLACED  ("refbuDisableInterlaced")
#define KEY_REFBU_DIS_DOUBLE      ("refbuDisableDouble")
#define KEY_REFBU_DIS_EVAL_MODE   ("refbuDisableEvalMode")
#define KEY_REFBU_DIS_CHECKPOINT  ("refbuDisableCheckpoint")
#define KEY_REFBU_DIS_OFFSET      ("refbuDisableOffset")
#define KEY_REFBU_DIS_TOPBOT      ("refbuDisableTopBotSum")
#define KEY_REFBU_DATA_EXCESS     ("refbuAdjustValue")

#define KEY_REFBU_TEST_OFFS       ("refbuTestOffsetEnable")
#define KEY_REFBU_TEST_OFFS_MIN   ("refbuTestOffsetMin")
#define KEY_REFBU_TEST_OFFS_MAX   ("refbuTestOffsetMax")
#define KEY_REFBU_TEST_OFFS_START ("refbuTestOffsetStart")
#define KEY_REFBU_TEST_OFFS_INCR  ("refbuTestOffsetIncr")

#define KEY_APF_THRESHOLD_DIS     ("apfDisableThreshold")
#define KEY_APF_THRESHOLD_VAL     ("apfThresholdValue")
#define KEY_APF_DISABLE           ("apfDisable")

#define KEY_BUS_WIDTH             ("BusWidth")
#define KEY_MEM_LATENCY           ("MemLatencyClks")
#define KEY_MEM_NONSEQ            ("MemNonSeqClks")
#define KEY_MEM_SEQ               ("MemSeqClks")
#define KEY_STRM_SWAP             ("strmSwap")
#define KEY_PIC_SWAP              ("picSwap")
#define KEY_DIRMV_SWAP            ("dirmvSwap")
#define KEY_TAB0_SWAP             ("tab0Swap")
#define KEY_TAB1_SWAP             ("tab1Swap")
#define KEY_TAB2_SWAP             ("tab2Swap")
#define KEY_TAB3_SWAP             ("tab3Swap")
#define KEY_RSCAN_SWAP            ("rscanSwap")
#define KEY_COMPTBL_SWAP          ("compTabSwap")
#define KEY_MAX_BURST             ("maxBurst")

#define KEY_SUPPORT_MPEG2         ("SupportMpeg2")
#define KEY_SUPPORT_VC1           ("SupportVc1")
#define KEY_SUPPORT_JPEG          ("SupportJpeg")
#define KEY_SUPPORT_MPEG4         ("SupportMpeg4")
#define KEY_SUPPORT_H264          ("SupportH264")
#define KEY_SUPPORT_VP6           ("SupportVp6")
#define KEY_SUPPORT_VP7           ("SupportVp7")
#define KEY_SUPPORT_VP8           ("SupportVp8")
#define KEY_SUPPORT_PJPEG         ("SupportPjpeg")
#define KEY_SUPPORT_SORENSON      ("SupportSorenson")
#define KEY_SUPPORT_AVS           ("SupportAvs")
#define KEY_SUPPORT_RV            ("SupportRv")
#define KEY_SUPPORT_MVC           ("SupportMvc")
#define KEY_SUPPORT_WEBP          ("SupportWebP")
#define KEY_SUPPORT_EC            ("SupportEc")
#define KEY_SUPPORT_STRIDE        ("SupportStride")
#define KEY_SUPPORT_CUSTOM_MPEG4  ("SupportCustomMpeg4")
#define KEY_SUPPORT_JPEGE         ("SupportJpegE")
#define KEY_SUPPORT_HEVC_MAIN10   ("SupportHevcMain10")
#define KEY_SUPPORT_VP9_PROFILE2_10 ("SupportVp9Profile2")
#define KEY_SUPPORT_RFC           ("SupportRFC")
#define KEY_SUPPORT_DOWNSCALING   ("SupportDownscaling")
#define KEY_SUPPORT_RINGBUFFER    ("SupportRingBuffer")
#define KEY_SUPPORT_NON_COMPLIANT ("SupportNonCompliant")
#define KEY_SUPPORT_PP_OUT_ENDIAN ("SupportPpOutEndianess")
#define KEY_SUPPORT_STRIPE_DIS    ("SupportStripeRemoval")
#define KEY_MAX_DEC_PIC_WIDTH     ("MaxDecPicWidth")
#define KEY_MAX_DEC_PIC_HEIGHT    ("MaxDecPicHeight")

#define KEY_SUPPORT_MRB_PREFETCH  ("SupportMRBPrefetch")
#define KEY_SUPPORT_64BIT_ADDR    ("Support64BitAddr")
#define KEY_SUPPORT_FORMAT_P010   ("SupportOutputFormatP010")
#define KEY_SUPPORT_FORMAT_CUSTOMER1    ("SupportOutputFormatCustomer1")

#define KEY_SUPPORT_PPD           ("SupportPpd")
#define KEY_SUPPORT_DITHER        ("SupportDithering")
#define KEY_SUPPORT_TILED         ("SupportTiled")
#define KEY_SUPPORT_TILED_REF     ("SupportTiledReference")
#define KEY_SUPPORT_FIELD_DPB     ("SupportFieldDPB")
#define KEY_SUPPORT_PIX_ACC_OUT   ("SupportPixelAccurOut")
#define KEY_SUPPORT_SCALING       ("SupportScaling")
#define KEY_SUPPORT_DEINT         ("SupportDeinterlacing")
#define KEY_SUPPORT_ABLEND        ("SupportAlphaBlending")
#define KEY_SUPPORT_ABLEND_CROP   ("SupportAblendCrop")
#define KEY_FAST_HOR_D_SCALE_DIS  ("FastHorizontalDownscaleDisable")
#define KEY_FAST_VER_D_SCALE_DIS  ("FastVerticalDownscaleDisable")
#define KEY_D_SCALE_STRIPES_DIS   ("VerticalDownscaleStripesDisable")
#define KEY_MAX_PP_OUT_PIC_WIDTH  ("MaxPpOutPicWidth")
#define KEY_HW_VERSION            ("HwVersion")
#define KEY_CACHE_VERSION         ("CacheVersion")
#define KEY_HW_BUILD              ("HwBuild")
#define KEY_HW_BUILD_ID           ("HwBuildId")
#define KEY_FILTER_ENABLED        ("FilterEnabled")
#define KEY_SUPPORT_MUTI_CORE     ("SupportMutiCore")

#define KEY_DWL_PAGE_SIZE         ("DwlMemPageSize")
#define KEY_DWL_REF_FRM_BUFFER    ("DwlRefFrmBufferSize")

#define KEY_UNIFIED_REG_FMT       ("UnifiedRegFmt")


#define KEY_OUTPUT_PICTURE_ENDIAN ("OutputPictureEndian")
#define KEY_BUS_BURST_LENGTH      ("BusBurstLength")
#define KEY_ASIC_SERVICE_PRIORITY ("AsicServicePriority")
#define KEY_OUTPUT_FORMAT         ("OutputFormat")
#define KEY_LATENCY_COMPENSATION  ("LatencyCompensation")
#define KEY_CLOCK_GATING          ("ClkGateDecoder")
#define KEY_CLOCK_GATING_RUNTIME  ("ClkGateDecoderIdle")
#define KEY_DATA_DISCARD          ("DataDiscard")
#define KEY_MEMORY_ALLOCATION     ("MemoryAllocation")
#define KEY_RLC_MODE_FORCED       ("RlcModeForced")
#define KEY_ERROR_CONCEALMENT     ("ErrorConcealment")
#define KEY_JPEG_MCUS_SLICE            ("JpegMcusSlice")
#define KEY_JPEG_INPUT_BUFFER_SIZE     ("JpegInputBufferSize")

#define KEY_INPUT_PICTURE_ENDIAN  ("InputPictureEndian")
#define KEY_WORD_SWAP             ("WordSwap")
#define KEY_WORD_SWAP_16          ("WordSwap16")
#define KEY_FORCE_MPEG4_IDCT      ("ForceMpeg4Idct")

#define KEY_MULTI_BUFFER          ("MultiBuffer")

#define KEY_CH_8PIX_ILEAV         ("Ch8PixIleavOutput")

#define KEY_SERV_MERGE_DISABLE    ("ServiceMergeDisable")

#define KEY_DOUBLE_REF_BUFFER     ("refDoubleBufferEnable")

#define KEY_TIMEOUT_CYCLES        ("timeoutOverrideLimit")

#define KEY_AXI_ID_R              ("axiIdRd")
#define KEY_AXI_ID_RE             ("axiIdRdUniqueE")
#define KEY_AXI_ID_W              ("axiIdWr")
#define KEY_AXI_ID_WE             ("axiIdWrUniqueE")

/* PP common params */
#define KEY_IN_WIDTH              ("InWidth")
#define KEY_IN_HEIGHT             ("InHeight")
#define KEY_IN_STRIDE             ("InStride")
#define KEY_PIPELINE_ENABLED      ("PipelineEnabled")
#define KEY_TILED_ENABLED         ("TiledOutEnabled")
#define KEY_PRE_FETCH_HEIGHT      ("PreFetchHeight")

/* PP unit params */
#define KEY_PPU_INDEX             ("UnitIndex")
#define KEY_PPU_TILED_ENABLED     ("TiledOutEnabled")
#define KEY_PPU_CR_FIRST          ("CrFirst")
#define KEY_PPU_OUT_MONOCHROME    ("OutMonoChrome")
#define KEY_PPU_CROPX             ("CropX")
#define KEY_PPU_CROPY             ("CropY")
#define KEY_PPU_CROPWIDTH         ("CropWidth")
#define KEY_PPU_CROPHEIGHT        ("CropHeight")
#define KEY_PPU_CROP2X            ("Crop2X")
#define KEY_PPU_CROP2Y            ("Crop2Y")
#define KEY_PPU_CROP2WIDTH        ("Crop2Width")
#define KEY_PPU_CROP2HEIGHT       ("Crop2Height")
#define KEY_PPU_SCALEWIDTH        ("ScaleWidth")
#define KEY_PPU_SCALEHEIGHT       ("ScaleHeight")
#define KEY_PPU_ENABLED           ("UnitEnabled")
#define KEY_PPU_SHAPERENABLE      ("ShaperEnable")
#define KEY_PPU_OUT_P010          ("OutP010")
#define KEY_PPU_OUT_I010          ("OutI010")
#define KEY_PPU_OUT_L010          ("OutL010")
#define KEY_PPU_OUT_1010          ("Out1010")
#define KEY_PPU_OUT_CUT8BIT       ("OutCut8Bits")
#define KEY_PPU_PLANNAR           ("Planar")
#define KEY_PPU_OUT_YSTRIDE       ("OutYStride")
#define KEY_PPU_OUT_CSTRIDE       ("OutCStride")

#ifdef ASIC_TRACE_SUPPORT
/* RTL block check enable params */
#define BLOCK_RTLCHK_PARAMS       ("RtlchkParams")
#define KEY_CABAC_COEFFS          ("cabac_coeffs")
#define KEY_EMD_TU_CTRL           ("emd_tu_ctrl")
#define KEY_EMD_CU_CTRL           ("emd_cu_ctrl")
#define KEY_DMV_CTRL              ("dmv_ctrl")
#define KEY_ALF_CTRL              ("alf_ctrl")
#define KEY_IQT_TU_CTRL           ("iqt_tu_ctrl")
#define KEY_IQT_OUT               ("iqt_out")
#define KEY_MVD_CU_CTRL           ("mvd_cu_ctrl")
#define KEY_MV_CTRL               ("mv_ctrl")
#define KEY_MVD_OUT_DIR_MVS       ("mvd_out_dir_mvs")
#define KEY_PFT_CU_CTRL           ("pft_cu_ctrl")
#define KEY_MV_CTRL_TO_PRED       ("mv_ctrl_to_pred")
#define KEY_APF_TRANSACT          ("apf_transact")
#define KEY_APF_PART_CTRL         ("apf_part_ctrl")
#define KEY_INTER_OUT_MV_CTRL     ("inter_out_mv_ctrl")
#define KEY_INTER_PRED_OUT        ("inter_pred_out")
#define KEY_INTRA_CU_CTRL         ("intra_cu_ctrl")
#define KEY_FILTERD_CTRL          ("filterd_ctrl")
#define KEY_PRED_OUT              ("pred_out")
#define KEY_FILTERD_OUT           ("filterd_out")
#define KEY_FILTERD_OUT_BLKCTRL   ("filterd_out_blkctrl")
#define KEY_SAO_OUT_CTRL          ("sao_out_ctrl")
#define KEY_SAO_OUT_DATA          ("sao_out_data")
#define KEY_ALF_OUT_PP_CTRL       ("alf_out_pp_ctrl")
#define KEY_ALF_OUT_PP_DATA       ("alf_out_pp_data")
#define KEY_EDC_CBSR_BURST_CTRL   ("edc_cbsr_burst_ctrl")
#define KEY_MISS_TABLE_TRANS      ("miss_table_trans")
#define KEY_UPDATE_TABLE_TRANS    ("update_table_trans")
 #endif

#define BLOCK_CMDBUF_PARAMS ("CmdbufParams")
#define KEY_CMD_EN ("cmd_en")

#define BLOCK_AXI_PARAMS ("AxiParams")
#define KEY_RD_AXI_ID ("rd_axi_id")
#define KEY_AXI_RD_ID_E ("axi_rd_id_e")

/*------------------------------------------------------------------------------
    Implement reading interer parameter
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_INTEGER(b, k, tgt)    \
  if (!strcmp(block, b) && !strcmp(key, k)) { \
    char* endptr;                             \
    tgt = strtol(value, &endptr, 10);         \
    if (*endptr) return TB_CFG_INVALID_VALUE; \
  }
/*------------------------------------------------------------------------------
    Implement reading string parameter
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_STRING(b, k, tgt)     \
  if (!strcmp(block, b) && !strcmp(key, k)) { \
    strncpy(tgt, value, sizeof(tgt));         \
  }

/*------------------------------------------------------------------------------
    Implement reading string parameter
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_HEX_INTEGER(b, k, tgt)    \
  if (!strcmp(block, b) && !strcmp(key, k)) { \
    char* endptr;                             \
    tgt = strtol(value, &endptr, 16);         \
    if (*endptr) return TB_CFG_INVALID_VALUE; \
  }

/*------------------------------------------------------------------------------
    Implement reading code parameter; Code parsing is handled by supplied
    function fn.
------------------------------------------------------------------------------*/
#define INVALID_CODE (0xFFFFFFFF)
#define IMPLEMENT_PARAM_CODE(b, k, tgt, fn)                            \
  if (block && key && !strcmp(block, b) && !strcmp(key, k)) {          \
    if ((tgt = fn(value)) == INVALID_CODE) return TB_CFG_INVALID_CODE; \
  }
/*------------------------------------------------------------------------------
    Implement structure allocation upon parsing a specific block.
------------------------------------------------------------------------------*/
#define IMPLEMENT_ALLOC_BLOCK(b, tgt, type) \
  if (key && !strcmp(key, b)) {             \
    register type** t = (type**)&tgt;       \
    if (!*t) {                              \
      *t = (type*)malloc(sizeof(type));     \
      ASSERT(*t);                           \
      memset(*t, 0, sizeof(type));          \
    } else                                  \
      return CFG_DUPLICATE_BLOCK;           \
  }

u32 ParseRefbuTestMode(char* value) {
  if (!strcmp(value, "NONE")) return 0;
  if (!strcmp(value, "OFFSET")) return 1;

  return INVALID_CODE;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ReadParam

        Functional description:
          Read parameter callback function.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
enum TBCfgCallbackResult TBReadParam(char* block, char* key, char* value,
                                     enum TBCfgCallbackParam state,
                                     void* cb_param) {
  struct TBCfg* tb_cfg = (struct TBCfg*)cb_param;

  ASSERT(key);
  ASSERT(tb_cfg);

  switch (state) {
  case TB_CFG_CALLBACK_BLK_START:
    /*printf("CFG_CALLBACK_BLK_START\n");
      printf("block == %s\n", block);
      printf("key == %s\n", key);
      printf("value == %s\n\n", value);*/
    /*IMPLEMENT_ALLOC_BLOCK( BLOCK_TD_PARAMS, tb_cfg->tb_params, TbParams );
      IMPLEMENT_ALLOC_BLOCK( BLOCK_DEC_PARAMS, tb_cfg->dec_params, DecParams );
      IMPLEMENT_ALLOC_BLOCK( BLOCK_PP_PARAMS, tb_cfg->pp_params, PpParams );*/
    break;
  case TB_CFG_CALLBACK_VALUE:
    /*printf("CFG_CALLBACK_VALUE\n");
      printf("block == %s\n", block);
      printf("key == %s\n", key);
      printf("value == %s\n\n", value);*/
    /* TbParams */
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_PACKET_BY_PACKET,
                           tb_cfg->tb_params.packet_by_packet);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_NAL_UNIT_STREAM,
                           tb_cfg->tb_params.nal_unit_stream);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_SEED_RND,
                            tb_cfg->tb_params.seed_rnd);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_STREAM_BIT_SWAP,
                           tb_cfg->tb_params.stream_bit_swap);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_STREAM_BIT_LOSS,
                           tb_cfg->tb_params.stream_bit_loss);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_STREAM_PACKET_LOSS,
                           tb_cfg->tb_params.stream_packet_loss);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_STREAM_HEADER_CORRUPT,
                           tb_cfg->tb_params.stream_header_corrupt);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_STREAM_TRUNCATE,
                           tb_cfg->tb_params.stream_truncate);
    IMPLEMENT_PARAM_STRING(BLOCK_TB_PARAMS, KEY_SLICE_UD_IN_PACKET,
                           tb_cfg->tb_params.slice_ud_in_packet);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_FIRST_TRACE_FRAME,
                            tb_cfg->tb_params.first_trace_frame);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_EXTRA_CU_CTRL_EOF,
                            tb_cfg->tb_params.extra_cu_ctrl_eof);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_DWL_PAGE_SIZE,
                            tb_cfg->tb_params.memory_page_size);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_DWL_REF_FRM_BUFFER,
                            tb_cfg->tb_params.ref_frm_buffer_size);
    IMPLEMENT_PARAM_INTEGER(BLOCK_TB_PARAMS, KEY_UNIFIED_REG_FMT,
                            tb_cfg->tb_params.unified_reg_fmt);

    /* DecParams */
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_OUTPUT_PICTURE_ENDIAN,
                           tb_cfg->dec_params.output_picture_endian);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_BUS_BURST_LENGTH,
                            tb_cfg->dec_params.bus_burst_length);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_ASIC_SERVICE_PRIORITY,
                            tb_cfg->dec_params.asic_service_priority);
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_OUTPUT_FORMAT,
                           tb_cfg->dec_params.output_format);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_LATENCY_COMPENSATION,
                            tb_cfg->dec_params.latency_compensation);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_CLOCK_GATING,
                            tb_cfg->dec_params.clk_gate_decoder);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_CLOCK_GATING_RUNTIME,
                            tb_cfg->dec_params.clk_gate_decoder_idle);
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_DATA_DISCARD,
                           tb_cfg->dec_params.data_discard);
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_MEMORY_ALLOCATION,
                           tb_cfg->dec_params.memory_allocation);
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_RLC_MODE_FORCED,
                           tb_cfg->dec_params.rlc_mode_forced);
    IMPLEMENT_PARAM_STRING(BLOCK_DEC_PARAMS, KEY_ERROR_CONCEALMENT,
                           tb_cfg->dec_params.error_concealment);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_JPEG_MCUS_SLICE,
                            tb_cfg->dec_params.jpeg_mcus_slice);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_JPEG_INPUT_BUFFER_SIZE,
                            tb_cfg->dec_params.jpeg_input_buffer_size);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_INTERLACED,
                            tb_cfg->dec_params.refbu_disable_interlaced);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_DOUBLE,
                            tb_cfg->dec_params.refbu_disable_double);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_EVAL_MODE,
                            tb_cfg->dec_params.refbu_disable_eval_mode);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_CHECKPOINT,
                            tb_cfg->dec_params.refbu_disable_checkpoint);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_OFFSET,
                            tb_cfg->dec_params.refbu_disable_offset);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DIS_TOPBOT,
                            tb_cfg->dec_params.refbu_disable_top_bot_sum);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_DATA_EXCESS,
                            tb_cfg->dec_params.refbu_data_excess_max_pct);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_ENABLED,
                            tb_cfg->dec_params.refbu_enable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_TILED_REF,
                            tb_cfg->dec_params.tiled_ref_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_FIELD_DPB,
                            tb_cfg->dec_params.field_dpb_support);

    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_APF_THRESHOLD_DIS,
                            tb_cfg->dec_params.apf_threshold_disable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_APF_THRESHOLD_VAL,
                            tb_cfg->dec_params.apf_threshold_value);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_APF_DISABLE,
                            tb_cfg->dec_params.apf_disable);

    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_TEST_OFFS,
                            tb_cfg->dec_params.ref_buffer_test_mode_offset_enable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_TEST_OFFS_MIN,
                            tb_cfg->dec_params.ref_buffer_test_mode_offset_min);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_TEST_OFFS_MAX,
                            tb_cfg->dec_params.ref_buffer_test_mode_offset_max);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_TEST_OFFS_START,
                            tb_cfg->dec_params.ref_buffer_test_mode_offset_start);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_REFBU_TEST_OFFS_INCR,
                            tb_cfg->dec_params.ref_buffer_test_mode_offset_incr);

    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_MPEG2,
                            tb_cfg->dec_params.mpeg2_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_VC1,
                            tb_cfg->dec_params.vc1_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_JPEG,
                            tb_cfg->dec_params.jpeg_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_MPEG4,
                            tb_cfg->dec_params.mpeg4_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_CUSTOM_MPEG4,
                            tb_cfg->dec_params.custom_mpeg4_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_H264,
                            tb_cfg->dec_params.h264_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_VP6,
                            tb_cfg->dec_params.vp6_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_VP7,
                            tb_cfg->dec_params.vp7_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_VP8,
                            tb_cfg->dec_params.vp8_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_PJPEG,
                            tb_cfg->dec_params.prog_jpeg_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_SORENSON,
                            tb_cfg->dec_params.sorenson_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_AVS,
                            tb_cfg->dec_params.avs_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_RV,
                            tb_cfg->dec_params.rv_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_MVC,
                            tb_cfg->dec_params.mvc_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_WEBP,
                            tb_cfg->dec_params.webp_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_EC,
                            tb_cfg->dec_params.ec_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_STRIDE,
                            tb_cfg->dec_params.stride_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_JPEGE,
                            tb_cfg->dec_params.jpeg_esupport);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_HEVC_MAIN10,
                            tb_cfg->dec_params.hevc_main10_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_VP9_PROFILE2_10,
                            tb_cfg->dec_params.vp9_profile2_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_RFC,
                            tb_cfg->dec_params.rfc_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_DOWNSCALING,
                            tb_cfg->dec_params.ds_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_RINGBUFFER,
                            tb_cfg->dec_params.ring_buffer_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_64BIT_ADDR,
                            tb_cfg->dec_params.addr64_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_FORMAT_P010,
                            tb_cfg->dec_params.format_p010_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_FORMAT_CUSTOMER1,
                            tb_cfg->dec_params.format_customer1_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_MRB_PREFETCH,
                            tb_cfg->dec_params.mrb_prefetch);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MAX_DEC_PIC_WIDTH,
                            tb_cfg->dec_params.max_dec_pic_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MAX_DEC_PIC_HEIGHT,
                            tb_cfg->dec_params.max_dec_pic_height);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_NON_COMPLIANT,
                            tb_cfg->dec_params.support_non_compliant);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_HW_VERSION,
                            tb_cfg->dec_params.hw_version);
    IMPLEMENT_PARAM_HEX_INTEGER(BLOCK_DEC_PARAMS, KEY_CACHE_VERSION,
                            tb_cfg->dec_params.cache_version);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_HW_BUILD,
                            tb_cfg->dec_params.hw_build);
    IMPLEMENT_PARAM_HEX_INTEGER(BLOCK_DEC_PARAMS, KEY_HW_BUILD_ID,
                            tb_cfg->dec_params.hw_build_id);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_BUS_WIDTH,
                            tb_cfg->dec_params.bus_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MEM_LATENCY,
                            tb_cfg->dec_params.latency);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MEM_NONSEQ,
                            tb_cfg->dec_params.non_seq_clk);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MEM_SEQ,
                            tb_cfg->dec_params.seq_clk);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_FORCE_MPEG4_IDCT,
                            tb_cfg->dec_params.force_mpeg4_idct);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_CH_8PIX_ILEAV,
                            tb_cfg->dec_params.ch8_pix_ileav_output);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SERV_MERGE_DISABLE,
                            tb_cfg->dec_params.service_merge_disable);

    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_STRM_SWAP,
                            tb_cfg->dec_params.strm_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_PIC_SWAP,
                            tb_cfg->dec_params.pic_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_DIRMV_SWAP,
                            tb_cfg->dec_params.dirmv_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_TAB0_SWAP,
                            tb_cfg->dec_params.tab0_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_TAB1_SWAP,
                            tb_cfg->dec_params.tab1_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_TAB2_SWAP,
                            tb_cfg->dec_params.tab2_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_TAB3_SWAP,
                            tb_cfg->dec_params.tab3_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_RSCAN_SWAP,
                            tb_cfg->dec_params.rscan_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_COMPTBL_SWAP,
                            tb_cfg->dec_params.comp_tab_swap);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_MAX_BURST,
                            tb_cfg->dec_params.max_burst);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_DOUBLE_REF_BUFFER,
                            tb_cfg->dec_params.ref_double_buffer_enable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_TIMEOUT_CYCLES,
                            tb_cfg->dec_params.timeout_cycles);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_AXI_ID_R,
                            tb_cfg->dec_params.axi_id_rd);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_AXI_ID_RE,
                            tb_cfg->dec_params.axi_id_rd_unique_enable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_AXI_ID_W,
                            tb_cfg->dec_params.axi_id_wr);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_AXI_ID_WE,
                            tb_cfg->dec_params.axi_id_wr_unique_enable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_DEC_PARAMS, KEY_SUPPORT_MUTI_CORE,
                            tb_cfg->dec_params.muti_core_support);
    /* PpParams */
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_OUTPUT_PICTURE_ENDIAN,
                           tb_cfg->pp_params.output_picture_endian);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_INPUT_PICTURE_ENDIAN,
                           tb_cfg->pp_params.input_picture_endian);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_WORD_SWAP,
                           tb_cfg->pp_params.word_swap);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_WORD_SWAP_16,
                           tb_cfg->pp_params.word_swap16);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_BUS_BURST_LENGTH,
                            tb_cfg->pp_params.bus_burst_length);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_CLOCK_GATING,
                           tb_cfg->pp_params.clock_gating);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_DATA_DISCARD,
                           tb_cfg->pp_params.data_discard);
    IMPLEMENT_PARAM_STRING(BLOCK_PP_PARAMS, KEY_MULTI_BUFFER,
                           tb_cfg->pp_params.multi_buffer);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_PPD,
                            tb_cfg->pp_params.ppd_exists);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_DITHER,
                            tb_cfg->pp_params.dithering_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_SCALING,
                            tb_cfg->pp_params.scaling_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_DEINT,
                            tb_cfg->pp_params.deinterlacing_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_ABLEND,
                            tb_cfg->pp_params.alpha_blending_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_ABLEND_CROP,
                            tb_cfg->pp_params.ablend_crop_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_PP_OUT_ENDIAN,
                            tb_cfg->pp_params.pp_out_endian_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_TILED,
                            tb_cfg->pp_params.tiled_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_STRIPE_DIS,
                            tb_cfg->pp_params.vert_down_scale_stripe_disable_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_MAX_PP_OUT_PIC_WIDTH,
                            tb_cfg->pp_params.max_pp_out_pic_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_TILED_REF,
                            tb_cfg->pp_params.tiled_ref_support);

    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_FAST_HOR_D_SCALE_DIS,
                            tb_cfg->pp_params.fast_hor_down_scale_disable);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_FAST_VER_D_SCALE_DIS,
                            tb_cfg->pp_params.fast_ver_down_scale_disable);
    /* IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS,
     *                          KEY_D_SCALE_STRIPES_DIS,
     *                          tb_cfg->pp_params.ver_downscale_stripes_disable );*/
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_SUPPORT_PIX_ACC_OUT,
                            tb_cfg->pp_params.pix_acc_out_support);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_FILTER_ENABLED,
                            tb_cfg->pp_params.filter_enabled);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_PIPELINE_ENABLED,
                            tb_cfg->pp_params.pipeline_e);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_TILED_ENABLED,
                            tb_cfg->pp_params.tiled_e);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_IN_WIDTH, tb_cfg->pp_params.in_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_IN_HEIGHT, tb_cfg->pp_params.in_height);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_IN_STRIDE, tb_cfg->pp_params.in_stride);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_PARAMS, KEY_PRE_FETCH_HEIGHT,
                            tb_cfg->pp_params.pre_fetch_height);

    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_INDEX, tb_cfg->ppu_index);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_TILED_ENABLED,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].tiled_e);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CR_FIRST,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].cr_first);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_MONOCHROME,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].monochrome);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROPX,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop_x);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROPY,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop_y);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROPWIDTH,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROPHEIGHT,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop_height);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROP2X,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop2_x);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROP2Y,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop2_y);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROP2WIDTH,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop2_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_CROP2HEIGHT,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].crop2_height);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_SCALEWIDTH,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].scale_width);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_SCALEHEIGHT,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].scale_height);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_ENABLED,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].unit_enabled);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_SHAPERENABLE,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].shaper_enabled);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_P010,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].out_p010);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_I010,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].out_I010);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_L010,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].out_L010);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_1010,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].out_1010);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_CUT8BIT,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].out_cut_8bits);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_PLANNAR,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].planar);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_YSTRIDE,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].ystride);
    IMPLEMENT_PARAM_INTEGER(BLOCK_PP_UNIT_PARAMS, KEY_PPU_OUT_CSTRIDE,
                            tb_cfg->pp_units_params[tb_cfg->ppu_index].cstride);
#ifdef ASIC_ONL_SIM
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_CABAC_COEFFS, tb_cfg->rtlchk_params.cabac_coeffs);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_EMD_TU_CTRL , tb_cfg->rtlchk_params.emd_tu_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_EMD_CU_CTRL , tb_cfg->rtlchk_params.emd_cu_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_DMV_CTRL    , tb_cfg->rtlchk_params.dmv_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_ALF_CTRL    , tb_cfg->rtlchk_params.alf_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_IQT_TU_CTRL                , tb_cfg->rtlchk_params.iqt_tu_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_IQT_OUT                    , tb_cfg->rtlchk_params.iqt_out);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_MVD_CU_CTRL                , tb_cfg->rtlchk_params.mvd_cu_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_MV_CTRL                    , tb_cfg->rtlchk_params.mv_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_MVD_OUT_DIR_MVS  , tb_cfg->rtlchk_params.mvd_out_dir_mvs);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_PFT_CU_CTRL      , tb_cfg->rtlchk_params.pft_cu_ctrl      );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_MV_CTRL_TO_PRED  , tb_cfg->rtlchk_params.mv_ctrl_to_pred  );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_APF_TRANSACT     , tb_cfg->rtlchk_params.apf_transact     );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_APF_PART_CTRL    , tb_cfg->rtlchk_params.apf_part_ctrl    );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_INTER_OUT_MV_CTRL, tb_cfg->rtlchk_params.inter_out_mv_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_INTER_PRED_OUT   , tb_cfg->rtlchk_params.inter_pred_out   );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_INTRA_CU_CTRL      , tb_cfg->rtlchk_params.intra_cu_ctrl      );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_FILTERD_CTRL       , tb_cfg->rtlchk_params.filterd_ctrl       );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_PRED_OUT           , tb_cfg->rtlchk_params.pred_out           );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_FILTERD_OUT        , tb_cfg->rtlchk_params.filterd_out        );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_FILTERD_OUT_BLKCTRL, tb_cfg->rtlchk_params.filterd_out_blkctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_SAO_OUT_CTRL       , tb_cfg->rtlchk_params.sao_out_ctrl       );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_SAO_OUT_DATA       , tb_cfg->rtlchk_params.sao_out_data       );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_ALF_OUT_PP_CTRL    , tb_cfg->rtlchk_params.alf_out_pp_ctrl    );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_ALF_OUT_PP_DATA    , tb_cfg->rtlchk_params.alf_out_pp_data    );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_EDC_CBSR_BURST_CTRL, tb_cfg->rtlchk_params.edc_cbsr_burst_ctrl);
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_MISS_TABLE_TRANS   , tb_cfg->rtlchk_params.miss_table_trans   );
    IMPLEMENT_PARAM_INTEGER(BLOCK_RTLCHK_PARAMS, KEY_UPDATE_TABLE_TRANS , tb_cfg->rtlchk_params.update_table_trans );

/*BLOCK_CMDBUF_PARAMS*/
    IMPLEMENT_PARAM_INTEGER(BLOCK_CMDBUF_PARAMS, KEY_CMD_EN , tb_cfg->cmdbuf_params.cmd_en);

/*BLOCK_AXI_PARAMS*/
    IMPLEMENT_PARAM_INTEGER(BLOCK_AXI_PARAMS, KEY_RD_AXI_ID , tb_cfg->axi_params.rd_axi_id);    
    IMPLEMENT_PARAM_INTEGER(BLOCK_AXI_PARAMS, KEY_AXI_RD_ID_E , tb_cfg->axi_params.axi_rd_id_e);

#endif

    break;
  }
  return TB_CFG_OK;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBPrintCfg

        Functional description:
          Prints the cofiguration to stdout.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void TBPrintCfg(const struct TBCfg* tb_cfg) {
  /* TbParams */
  printf("tb_cfg->tb_params.packet_by_packet: %s\n",
         tb_cfg->tb_params.packet_by_packet);
  printf("tb_cfg->tb_params.nal_unit_stream: %s\n",
         tb_cfg->tb_params.nal_unit_stream);
  printf("tb_cfg->tb_params.seed_rnd: %d\n", tb_cfg->tb_params.seed_rnd);
  printf("tb_cfg->tb_params.stream_bit_swap: %s\n",
         tb_cfg->tb_params.stream_bit_swap);
  printf("tb_cfg->tb_params.stream_bit_loss: %s\n",
         tb_cfg->tb_params.stream_bit_loss);
  printf("tb_cfg->tb_params.stream_packet_loss: %s\n",
         tb_cfg->tb_params.stream_packet_loss);
  printf("tb_cfg->tb_params.stream_header_corrupt: %s\n",
         tb_cfg->tb_params.stream_header_corrupt);
  printf("tb_cfg->tb_params.stream_truncate: %s\n",
         tb_cfg->tb_params.stream_truncate);
  printf("tb_cfg->tb_params.slice_ud_in_packet: %s\n",
         tb_cfg->tb_params.slice_ud_in_packet);
  printf("tb_cfg->tb_params.first_trace_frame: %d\n",
         tb_cfg->tb_params.first_trace_frame);
  printf("tb_cfg->tb_params.extra_cu_ctrl_eof: %d\n",
         tb_cfg->tb_params.extra_cu_ctrl_eof);

  /* DecParams */
  printf("tb_cfg->dec_params.output_picture_endian: %s\n",
         tb_cfg->dec_params.output_picture_endian);
  printf("tb_cfg->dec_params.bus_burst_length: %d\n",
         tb_cfg->dec_params.bus_burst_length);
  printf("tb_cfg->dec_params.asic_service_priority: %d\n",
         tb_cfg->dec_params.asic_service_priority);
  printf("tb_cfg->dec_params.output_format: %s\n",
         tb_cfg->dec_params.output_format);
  printf("tb_cfg->dec_params.latency_compensation: %d\n",
         tb_cfg->dec_params.latency_compensation);
  printf("tb_cfg->dec_params.clk_gate_decoder: %d\n",
         tb_cfg->dec_params.clk_gate_decoder);
  printf("tb_cfg->dec_params.clk_gate_decoder_idle: %d\n",
         tb_cfg->dec_params.clk_gate_decoder_idle);
  printf("tb_cfg->dec_params.data_discard: %s\n",
         tb_cfg->dec_params.data_discard);
  printf("tb_cfg->dec_params.memory_allocation: %s\n",
         tb_cfg->dec_params.memory_allocation);
  printf("tb_cfg->dec_params.rlc_mode_forced: %s\n",
         tb_cfg->dec_params.rlc_mode_forced);
  printf("tb_cfg->dec_params.error_concealment: %s\n",
         tb_cfg->dec_params.error_concealment);
  printf("tb_cfg->dec_params.jpeg_mcus_slice: %d\n",
         tb_cfg->dec_params.jpeg_mcus_slice);
  printf("tb_cfg->dec_params.jpeg_input_buffer_size: %d\n",
         tb_cfg->dec_params.jpeg_input_buffer_size);

  /* PpParams */
  printf("tb_cfg->pp_params.output_picture_endian: %s\n",
         tb_cfg->pp_params.output_picture_endian);
  printf("tb_cfg->pp_params.input_picture_endian: %s\n",
         tb_cfg->pp_params.input_picture_endian);
  printf("tb_cfg->pp_params.word_swap: %s\n", tb_cfg->pp_params.word_swap);
  printf("tb_cfg->pp_params.word_swap16: %s\n", tb_cfg->pp_params.word_swap16);
  printf("tb_cfg->pp_params.multi_buffer: %s\n",
         tb_cfg->pp_params.multi_buffer);
  printf("tb_cfg->pp_params.bus_burst_length: %d\n",
         tb_cfg->pp_params.bus_burst_length);
  printf("tb_cfg->pp_params.clock_gating: %s\n",
         tb_cfg->pp_params.clock_gating);
  printf("tb_cfg->pp_params.data_discard: %s\n",
         tb_cfg->pp_params.data_discard);
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBSetDefaultCfg

        Functional description:
          Sets the default configuration.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void TBSetDefaultCfg(struct TBCfg* tb_cfg) {
  /* TbParams */
  strcpy(tb_cfg->tb_params.packet_by_packet, "DISABLED");
  strcpy(tb_cfg->tb_params.nal_unit_stream, "DISABLED");
  tb_cfg->tb_params.seed_rnd = 1;
  strcpy(tb_cfg->tb_params.stream_bit_swap, "0");
  strcpy(tb_cfg->tb_params.stream_bit_loss, "0");
  strcpy(tb_cfg->tb_params.stream_packet_loss, "0");
  strcpy(tb_cfg->tb_params.stream_header_corrupt, "DISABLED");
  strcpy(tb_cfg->tb_params.stream_truncate, "DISABLED");
  strcpy(tb_cfg->tb_params.slice_ud_in_packet, "DISABLED");
  tb_cfg->tb_params.memory_page_size = 1;
  tb_cfg->tb_params.ref_frm_buffer_size = -1;
  tb_cfg->tb_params.first_trace_frame = 0;
  tb_cfg->tb_params.extra_cu_ctrl_eof = 0;
  tb_cfg->tb_params.unified_reg_fmt = 0;
  tb_cfg->dec_params.force_mpeg4_idct = 0;
  tb_cfg->dec_params.ref_buffer_test_mode_offset_enable = 0;
  tb_cfg->dec_params.ref_buffer_test_mode_offset_min = -256;
  tb_cfg->dec_params.ref_buffer_test_mode_offset_start = -256;
  tb_cfg->dec_params.ref_buffer_test_mode_offset_max = 255;
  tb_cfg->dec_params.ref_buffer_test_mode_offset_incr = 16;
  tb_cfg->dec_params.apf_threshold_disable = 1;
  tb_cfg->dec_params.apf_threshold_value = -1;
  tb_cfg->dec_params.apf_disable = 0;
  tb_cfg->dec_params.field_dpb_support = 0;
  tb_cfg->dec_params.service_merge_disable = 0;

  /* Enable new features by default */
  tb_cfg->dec_params.hevc_main10_support = 1;
  tb_cfg->dec_params.vp9_profile2_support = 1;
  tb_cfg->dec_params.ds_support = 1;
  tb_cfg->dec_params.rfc_support = 1;
  tb_cfg->dec_params.ring_buffer_support = 1;
  tb_cfg->dec_params.addr64_support = 1;
  tb_cfg->dec_params.service_merge_disable = 0;
  tb_cfg->dec_params.mrb_prefetch = 1;

  /* Output pixel format by default */
  tb_cfg->dec_params.format_p010_support = 1;
  tb_cfg->dec_params.format_customer1_support = 1;

  /* DecParams */
#if (DEC_X170_OUTPUT_PICTURE_ENDIAN == DEC_X170_BIG_ENDIAN)
  strcpy(tb_cfg->dec_params.output_picture_endian, "BIG_ENDIAN");
#else
  strcpy(tb_cfg->dec_params.output_picture_endian, "LITTLE_ENDIAN");
#endif

  tb_cfg->dec_params.bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
  tb_cfg->dec_params.asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;

#if (DEC_X170_OUTPUT_FORMAT == DEC_X170_OUTPUT_FORMAT_RASTER_SCAN)
  strcpy(tb_cfg->dec_params.output_format, "RASTER_SCAN");
#else
  strcpy(tb_cfg->dec_params.output_format, "TILED");
#endif

  tb_cfg->dec_params.latency_compensation = DEC_X170_LATENCY_COMPENSATION;

  tb_cfg->dec_params.clk_gate_decoder = DEC_X170_INTERNAL_CLOCK_GATING;
  tb_cfg->dec_params.clk_gate_decoder_idle =
    DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME;

#if (DEC_X170_INTERNAL_CLOCK_GATING == 0)
  strcpy(tb_cfg->dec_params.clock_gating, "DISABLED");
#else
  strcpy(tb_cfg->dec_params.clock_gating, "ENABLED");
#endif

#if (DEC_X170_DATA_DISCARD_ENABLE == 0)
  strcpy(tb_cfg->dec_params.data_discard, "DISABLED");
#else
  strcpy(tb_cfg->dec_params.data_discard, "ENABLED");
#endif

  strcpy(tb_cfg->dec_params.memory_allocation, "INTERNAL");
  strcpy(tb_cfg->dec_params.rlc_mode_forced, "DISABLED");
  strcpy(tb_cfg->dec_params.error_concealment, "PICTURE_FREEZE");
  tb_cfg->dec_params.stride_support = 0;
  tb_cfg->dec_params.jpeg_mcus_slice = 0;
  tb_cfg->dec_params.jpeg_input_buffer_size = 0;
  tb_cfg->dec_params.ch8_pix_ileav_output = 0;

  tb_cfg->dec_params.tiled_ref_support =
     tb_cfg->pp_params.tiled_ref_support = 0;

  tb_cfg->dec_params.refbu_enable = 0;
  tb_cfg->dec_params.refbu_disable_interlaced = 1;
  tb_cfg->dec_params.refbu_disable_double = 1;
  tb_cfg->dec_params.refbu_disable_eval_mode = 1;
  tb_cfg->dec_params.refbu_disable_checkpoint = 1;
  tb_cfg->dec_params.refbu_disable_offset = 1;
  tb_cfg->dec_params.refbu_disable_top_bot_sum = 1;
#ifdef DEC_X170_REFBU_ADJUST_VALUE
  tb_cfg->dec_params.refbu_data_excess_max_pct = DEC_X170_REFBU_ADJUST_VALUE;
#else
  tb_cfg->dec_params.refbu_data_excess_max_pct = 130;
#endif

  tb_cfg->dec_params.mpeg2_support = 1;
  tb_cfg->dec_params.vc1_support = 3; /* Adv profile */
  tb_cfg->dec_params.jpeg_support = 1;
  tb_cfg->dec_params.mpeg4_support = 2; /* ASP */
  tb_cfg->dec_params.h264_support = 3; /* High */
  tb_cfg->dec_params.vp6_support = 1;
  tb_cfg->dec_params.vp7_support = 1;
  tb_cfg->dec_params.vp8_support = 1;
  tb_cfg->dec_params.prog_jpeg_support = 1;
  tb_cfg->dec_params.sorenson_support = 1;
  tb_cfg->dec_params.custom_mpeg4_support = 1; /* custom feature 1 */
  tb_cfg->dec_params.avs_support = 2; /* AVS Plus */
  tb_cfg->dec_params.rv_support = 1;
  tb_cfg->dec_params.mvc_support = 1;
  tb_cfg->dec_params.webp_support = 1;
  tb_cfg->dec_params.ec_support = 0;
  tb_cfg->dec_params.jpeg_esupport = 0;
  tb_cfg->dec_params.support_non_compliant = 1;
  tb_cfg->dec_params.max_dec_pic_width = 4096;
  tb_cfg->dec_params.max_dec_pic_height = 2304;
  tb_cfg->dec_params.hw_version = 18001;
  tb_cfg->dec_params.cache_version = 0x0;
  tb_cfg->dec_params.hw_build = 7000;
  tb_cfg->dec_params.hw_build_id = DEC_HW_BUILD_ID;

  tb_cfg->dec_params.bus_width = DEC_X170_BUS_WIDTH;
  tb_cfg->dec_params.latency = DEC_X170_REFBU_LATENCY;
  tb_cfg->dec_params.non_seq_clk = DEC_X170_REFBU_NONSEQ;
  tb_cfg->dec_params.seq_clk = DEC_X170_REFBU_SEQ;

  tb_cfg->dec_params.strm_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.pic_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.dirmv_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.tab0_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.tab1_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.tab2_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.tab3_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.rscan_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.comp_tab_swap = HANTRODEC_STREAM_SWAP;
  tb_cfg->dec_params.max_burst = HANTRODEC_MAX_BURST;
  tb_cfg->dec_params.ref_double_buffer_enable =
    HANTRODEC_INTERNAL_DOUBLE_REF_BUFFER;
  tb_cfg->dec_params.timeout_cycles = HANTRODEC_TIMEOUT_OVERRIDE;
#ifdef DEC_X170_REFBU_SEQ
  tb_cfg->dec_params.apf_threshold_value = DEC_X170_REFBU_NONSEQ / DEC_X170_REFBU_SEQ;
#else
  tb_cfg->dec_params.apf_threshold_value = DEC_X170_REFBU_NONSEQ;
#endif
  tb_cfg->dec_params.apf_disable = DEC_X170_APF_DISABLE;
  tb_cfg->dec_params.clk_gate_decoder = DEC_X170_INTERNAL_CLOCK_GATING;
  tb_cfg->dec_params.clk_gate_decoder_idle = DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME;
  tb_cfg->dec_params.axi_id_rd = DEC_X170_AXI_ID_R;
  tb_cfg->dec_params.axi_id_rd_unique_enable = DEC_X170_AXI_ID_R_E;
  tb_cfg->dec_params.axi_id_wr = DEC_X170_AXI_ID_W;
  tb_cfg->dec_params.axi_id_wr_unique_enable = DEC_X170_AXI_ID_W_E;

  /* PpParams */
  strcpy(tb_cfg->pp_params.output_picture_endian, "PP_CFG");
  strcpy(tb_cfg->pp_params.input_picture_endian, "PP_CFG");
  strcpy(tb_cfg->pp_params.word_swap, "PP_CFG");
  strcpy(tb_cfg->pp_params.word_swap16, "PP_CFG");
  tb_cfg->pp_params.bus_burst_length = PP_X170_BUS_BURST_LENGTH;

  strcpy(tb_cfg->pp_params.multi_buffer, "DISABLED");

#if (PP_X170_INTERNAL_CLOCK_GATING == 0)
  strcpy(tb_cfg->pp_params.clock_gating, "DISABLED");
#else
  strcpy(tb_cfg->pp_params.clock_gating, "ENABLED");
#endif

#if (DEC_X170_DATA_DISCARD_ENABLE == 0)
  strcpy(tb_cfg->pp_params.data_discard, "DISABLED");
#else
  strcpy(tb_cfg->pp_params.data_discard, "ENABLED");
#endif

  tb_cfg->pp_params.ppd_exists = 1;
  tb_cfg->pp_params.dithering_support = 1;
  tb_cfg->pp_params.scaling_support = 1; /* Lo/Hi performance? */
  tb_cfg->pp_params.deinterlacing_support = 1;
  tb_cfg->pp_params.alpha_blending_support = 1;
  tb_cfg->pp_params.ablend_crop_support = 0;
  tb_cfg->pp_params.pp_out_endian_support = 1;
  tb_cfg->pp_params.tiled_support = 1;
  tb_cfg->pp_params.max_pp_out_pic_width = 4096;

  tb_cfg->pp_params.fast_hor_down_scale_disable = 0;
  tb_cfg->pp_params.fast_ver_down_scale_disable = 0;
  /*    tb_cfg->pp_params.ver_downscale_stripes_disable = 0;*/

  tb_cfg->pp_params.filter_enabled = 0; /* MPEG4 deblocking filter disabled as default. */

  tb_cfg->pp_params.pix_acc_out_support = 1;
  tb_cfg->pp_params.vert_down_scale_stripe_disable_support = 0;
  tb_cfg->pp_params.pipeline_e = 1;
  tb_cfg->pp_params.pre_fetch_height = 16;
  tb_cfg->ppu_index = -1;
  memset(tb_cfg->pp_units_params, 0, sizeof(tb_cfg->pp_units_params));

  if (tb_cfg->dec_params.hw_build == 7020 || tb_cfg->dec_params.hw_version == 18001) {
    tb_cfg->dec_params.tiled_ref_support = 1;
    tb_cfg->dec_params.field_dpb_support = 1;
  }
}

u32 TBCheckCfg(struct TBCfg* tb_cfg) {
  /* TbParams */
  /*if (tb_cfg->tb_params.max_pics)
  {
  }*/

  if (strcmp(tb_cfg->tb_params.packet_by_packet, "ENABLED") &&
      strcmp(tb_cfg->tb_params.packet_by_packet, "DISABLED")) {
    printf("Error in TbParams.PacketByPacket: %s\n",
           tb_cfg->tb_params.packet_by_packet);
    return 1;
  }

  if (strcmp(tb_cfg->tb_params.nal_unit_stream, "ENABLED") &&
      strcmp(tb_cfg->tb_params.nal_unit_stream, "DISABLED")) {
    printf("Error in TbParams.NalUnitStream: %s\n",
           tb_cfg->tb_params.nal_unit_stream);
    return 1;
  }

  /*if (strcmp(tb_cfg->tb_params.stream_bit_swap, "0") == 0 &&
          strcmp(tb_cfg->tb_params.stream_header_corrupt, "ENABLED") == 0)
  {
      printf("Stream header corrupt requires enabled stream bit swap (see test
  bench configuration)\n");
      return 1;
  }*/

  /* HEVC/VP9 doesn't support packet loss currrently, for they doesn't support packet-by-packet mode.*/
  if (strcmp(tb_cfg->tb_params.stream_packet_loss, "0") &&
      strcmp(tb_cfg->tb_params.packet_by_packet, "DISABLED") == 0) {
    printf("Stream packet loss requires enabled packet by packet mode (see test bench configuration)\n");
    return 1;
  }


  if (strcmp(tb_cfg->tb_params.stream_header_corrupt, "ENABLED") &&
      strcmp(tb_cfg->tb_params.stream_header_corrupt, "DISABLED")) {
    printf("Error in TbParams.StreamHeaderCorrupt: %s\n",
           tb_cfg->tb_params.stream_header_corrupt);
    return 1;
  }

  if (strcmp(tb_cfg->tb_params.stream_truncate, "ENABLED") &&
      strcmp(tb_cfg->tb_params.stream_truncate, "DISABLED")) {
    printf("Error in TbParams.StreamTruncate: %s\n",
           tb_cfg->tb_params.stream_truncate);
    return 1;
  }

  if (strcmp(tb_cfg->tb_params.slice_ud_in_packet, "ENABLED") &&
      strcmp(tb_cfg->tb_params.slice_ud_in_packet, "DISABLED")) {
    printf("Error in TbParams.stream_truncate: %s\n",
           tb_cfg->tb_params.slice_ud_in_packet);
    return 1;
  }

  /* DecParams */
  if (strcmp(tb_cfg->dec_params.output_picture_endian, "LITTLE_ENDIAN") &&
      strcmp(tb_cfg->dec_params.output_picture_endian, "BIG_ENDIAN")) {
    printf("Error in DecParams.OutputPictureEndian: %s\n",
           tb_cfg->dec_params.output_picture_endian);
    return 1;
  }

  if (tb_cfg->dec_params.bus_burst_length > 31) {
    printf("Error in DecParams.BusBurstLength: %d\n",
           tb_cfg->dec_params.bus_burst_length);
    return 1;
  }

  if (tb_cfg->dec_params.asic_service_priority > 4) {
    printf("Error in DecParams.AsicServicePriority: %d\n",
           tb_cfg->dec_params.asic_service_priority);
    return 1;
  }

  if (strcmp(tb_cfg->dec_params.output_format, "RASTER_SCAN") &&
      strcmp(tb_cfg->dec_params.output_format, "TILED")) {
    printf("Error in DecParams.OutputFormat: %s\n",
           tb_cfg->dec_params.output_format);
    return 1;
  }

  if (tb_cfg->dec_params.latency_compensation > 63/* ||
      tb_cfg->dec_params.latency_compensation < 0*/) {
    printf("Error in DecParams.LatencyCompensation: %d\n",
           tb_cfg->dec_params.latency_compensation);
    return 1;
  }

  if (strcmp(tb_cfg->dec_params.clock_gating, "ENABLED") &&
      strcmp(tb_cfg->dec_params.clock_gating, "DISABLED")) {
    printf("Error in DecParams.ClockGating: %s\n", tb_cfg->dec_params.clock_gating);
    return 1;
  }

  if (tb_cfg->dec_params.clk_gate_decoder > 1) {
    printf("Error in DecParams.clk_gate_decoder: %d\n",
           tb_cfg->dec_params.clk_gate_decoder);
    return 1;
  }
  if (tb_cfg->dec_params.clk_gate_decoder_idle > 1) {
    printf("Error in DecParams.clk_gate_decoder_idle: %d\n",
           tb_cfg->dec_params.clk_gate_decoder_idle);
    return 1;
  }
  if (tb_cfg->dec_params.clk_gate_decoder_idle &&
      !tb_cfg->dec_params.clk_gate_decoder) {
    printf("Error in DecParams.clk_gate_decoder_idle: %d\n",
           tb_cfg->dec_params.clk_gate_decoder_idle);
    return 1;
  }

  if (strcmp(tb_cfg->dec_params.data_discard, "ENABLED") &&
      strcmp(tb_cfg->dec_params.data_discard, "DISABLED")) {
    printf("Error in DecParams.DataDiscard: %s\n",
           tb_cfg->dec_params.data_discard);
    return 1;
  }

  if (strcmp(tb_cfg->dec_params.memory_allocation, "INTERNAL") &&
      strcmp(tb_cfg->dec_params.memory_allocation, "EXTERNAL")) {
    printf("Error in DecParams.MemoryAllocation: %s\n",
           tb_cfg->dec_params.memory_allocation);
    return 1;
  }

  if (strcmp(tb_cfg->dec_params.rlc_mode_forced, "DISABLED") &&
      strcmp(tb_cfg->dec_params.rlc_mode_forced, "ENABLED")) {
    printf("Error in DecParams.RlcModeForced: %s\n",
           tb_cfg->dec_params.rlc_mode_forced);
    return 1;
  }

  /*if (strcmp(tb_cfg->dec_params.rlc_mode_forced, "ENABLED") == 0 &&
          strcmp(tb_cfg->dec_params.error_concealment, "PICTURE_FREEZE") == 0)
  {
    printf("MACRO_BLOCK DecParams.ErrorConcealment must be enabled if RLC
  coding\n");
    return 1;
  }*/

  /*
    if (strcmp(tb_cfg->dec_params.rlc_mode_forced, "ENABLED") == 0 &&
            (strcmp(tb_cfg->tb_params.packet_by_packet, "ENABLED") == 0 ||
          strcmp(tb_cfg->tb_params.nal_unit_stream, "ENABLED") == 0))
    {
        printf("TbParams.PacketByPacket and TbParams.NalUnitStream must not be enabled if RLC coding\n");
      return 1;
    }
    */ /* why is that above? */

  if (strcmp(tb_cfg->tb_params.nal_unit_stream, "ENABLED") == 0 &&
      strcmp(tb_cfg->tb_params.packet_by_packet, "DISABLED") == 0) {
    printf(
      "TbParams.PacketByPacket must be enabled if NAL unit stream is used\n");
    return 1;
  }

  if (strcmp(tb_cfg->tb_params.slice_ud_in_packet, "ENABLED") == 0 &&
      strcmp(tb_cfg->tb_params.packet_by_packet, "DISABLED") == 0) {
    printf(
      "TbParams.PacketByPacket must be enabled if slice user data is "
      "included in packet\n");
    return 1;
  }

  /*if (strcmp(tb_cfg->dec_params.error_concealment, "MACRO_BLOCK") &&
      strcmp(tb_cfg->dec_params.error_concealment, "PICTURE_FREEZE"))
  {
      printf("Error in DecParams.ErrorConcealment: %s\n",
  tb_cfg->dec_params.error_concealment);
          return 1;
  }*/

  /*if (tb_cfg->dec_params.mcus_slice)
  {
  }*/

  if (tb_cfg->dec_params.jpeg_input_buffer_size != 0 &&
      ((tb_cfg->dec_params.jpeg_input_buffer_size > 0 &&
        tb_cfg->dec_params.jpeg_input_buffer_size < 5120) ||
       tb_cfg->dec_params.jpeg_input_buffer_size > 16776960 ||
       tb_cfg->dec_params.jpeg_input_buffer_size % 256 != 0)) {
    printf("Error in DecParams.input_buffer_size: %d\n",
           tb_cfg->dec_params.jpeg_input_buffer_size);
    return 1;
  }

  /* PpParams */
  if (strcmp(tb_cfg->pp_params.output_picture_endian, "LITTLE_ENDIAN") &&
      strcmp(tb_cfg->pp_params.output_picture_endian, "BIG_ENDIAN") &&
      strcmp(tb_cfg->pp_params.output_picture_endian, "PP_CFG")) {
    printf("Error in PpParams.OutputPictureEndian: %s\n",
           tb_cfg->pp_params.output_picture_endian);
    return 1;
  }

  if (strcmp(tb_cfg->pp_params.input_picture_endian, "LITTLE_ENDIAN") &&
      strcmp(tb_cfg->pp_params.input_picture_endian, "BIG_ENDIAN") &&
      strcmp(tb_cfg->pp_params.input_picture_endian, "PP_CFG")) {
    printf("Error in PpParams.InputPictureEndian: %s\n",
           tb_cfg->pp_params.input_picture_endian);
    return 1;
  }

  if (strcmp(tb_cfg->pp_params.word_swap, "ENABLED") &&
      strcmp(tb_cfg->pp_params.word_swap, "DISABLED") &&
      strcmp(tb_cfg->pp_params.word_swap, "PP_CFG")) {
    printf("Error in PpParams.WordSwap: %s\n", tb_cfg->pp_params.word_swap);
    return 1;
  }

  if (strcmp(tb_cfg->pp_params.word_swap16, "ENABLED") &&
      strcmp(tb_cfg->pp_params.word_swap16, "DISABLED") &&
      strcmp(tb_cfg->pp_params.word_swap16, "PP_CFG")) {
    printf("Error in PpParams.WordSwap16: %s\n", tb_cfg->pp_params.word_swap16);
    return 1;
  }

  if (tb_cfg->pp_params.bus_burst_length > 31) {
    printf("Error in PpParams.BusBurstLength: %d\n",
           tb_cfg->pp_params.bus_burst_length);
    return 1;
  }

  if (strcmp(tb_cfg->pp_params.clock_gating, "ENABLED") &&
      strcmp(tb_cfg->pp_params.clock_gating, "DISABLED")) {
    printf("Error in PpParams.ClockGating: %s\n",
           tb_cfg->pp_params.clock_gating);
    return 1;
  }

  if (strcmp(tb_cfg->pp_params.data_discard, "ENABLED") &&
      strcmp(tb_cfg->pp_params.data_discard, "DISABLED")) {
    printf("Error in PpParams.DataDiscard: %s\n",
           tb_cfg->pp_params.data_discard);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPDataDiscard

        Functional description:
          Gets the integer values of PP data disgard.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPDataDiscard(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.data_discard, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->pp_params.data_discard, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPClockGating

        Functional description:
          Gets the integer values of PP clock gating.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPClockGating(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.clock_gating, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->pp_params.clock_gating, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPWordSwap

        Functional description:
          Gets the integer values of PP word swap.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPWordSwap(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.word_swap, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->pp_params.word_swap, "DISABLED") == 0) {
    return 0;
  } else if (strcmp(tb_cfg->pp_params.word_swap, "PP_CFG") == 0) {
    return 2;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPWordSwap

        Functional description:
          Gets the integer values of PP word swap.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPWordSwap16(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.word_swap16, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->pp_params.word_swap16, "DISABLED") == 0) {
    return 0;
  } else if (strcmp(tb_cfg->pp_params.word_swap16, "PP_CFG") == 0) {
    return 2;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPInputPictureEndian

        Functional description:
          Gets the integer values of PP input picture endian.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPInputPictureEndian(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.input_picture_endian, "BIG_ENDIAN") == 0) {
    return PP_X170_PICTURE_BIG_ENDIAN;
  } else if (strcmp(tb_cfg->pp_params.input_picture_endian, "LITTLE_ENDIAN") ==
             0) {
    return PP_X170_PICTURE_LITTLE_ENDIAN;
  } else if (strcmp(tb_cfg->pp_params.input_picture_endian, "PP_CFG") == 0) {
    return 2;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetPPOutputPictureEndian

        Functional description:
          Gets the integer values of PP out picture endian.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetPPOutputPictureEndian(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.output_picture_endian, "BIG_ENDIAN") == 0) {
    return PP_X170_PICTURE_BIG_ENDIAN;
  } else if (strcmp(tb_cfg->pp_params.output_picture_endian, "LITTLE_ENDIAN") ==
             0) {
    return PP_X170_PICTURE_LITTLE_ENDIAN;
  } else if (strcmp(tb_cfg->pp_params.output_picture_endian, "PP_CFG") == 0) {
    return 2;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecErrorConcealment

        Functional description:
          Gets the integer values of decoder error concealment.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecErrorConcealment(const struct TBCfg* tb_cfg) {
  /*
  if (strcmp(tb_cfg->dec_params.error_concealment, "MACRO_BLOCK") == 0)
  {
      return 1;
  }
  else*/
  if (strcmp(tb_cfg->dec_params.error_concealment, "PICTURE_FREEZE") == 0)
    return 0;
  else if (strcmp(tb_cfg->dec_params.error_concealment, "INTRA_FREEZE") == 0)
    return 1;
  else if (strcmp(tb_cfg->dec_params.error_concealment, "PARTIAL_FREEZE") == 0)
    return 2;
  else if (strcmp(tb_cfg->dec_params.error_concealment, "PARTIAL_IGNORE") == 0)
    return 3;
  else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecRlcModeForced

        Functional description:
          Gets the integer values of decoder rlc mode forced.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecRlcModeForced(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.rlc_mode_forced, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->dec_params.rlc_mode_forced, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecMemoryAllocation

        Functional description:
          Gets the integer values of decoder memory allocation.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecMemoryAllocation(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.memory_allocation, "INTERNAL") == 0) {
    return 0;
  } else if (strcmp(tb_cfg->dec_params.memory_allocation, "EXTERNAL") == 0) {
    return 1;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecDataDiscard

        Functional description:
          Gets the integer values of decoder data disgard.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecDataDiscard(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.data_discard, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->dec_params.data_discard, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecClockGating

        Functional description:
          Gets the integer values of decoder clock gating.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecClockGating(const struct TBCfg* tb_cfg) {
  return tb_cfg->dec_params.clk_gate_decoder ? 1 : 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecClockGatingRuntime

        Functional description:
          Gets the integer values of decoder runtime clock gating.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecClockGatingRuntime(const struct TBCfg* tb_cfg) {
  return tb_cfg->dec_params.clk_gate_decoder_idle ? 1 : 0;
}
/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecOutputFormat

        Functional description:
          Gets the integer values of decoder output format.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecOutputFormat(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.output_format, "RASTER_SCAN") == 0) {
    return DEC_X170_OUTPUT_FORMAT_RASTER_SCAN;
  } else if (strcmp(tb_cfg->dec_params.output_format, "TILED") == 0) {
    return DEC_X170_OUTPUT_FORMAT_TILED;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecOutputPictureEndian

        Functional description:
          Gets the integer values of decoder output format.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecOutputPictureEndian(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.output_picture_endian, "BIG_ENDIAN") == 0) {
    return DEC_X170_BIG_ENDIAN;
  } else if (strcmp(tb_cfg->dec_params.output_picture_endian,
                    "LITTLE_ENDIAN") == 0) {
    return DEC_X170_LITTLE_ENDIAN;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBPacketByPacket

        Functional description:
          Gets the integer values of TB packet by packet.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBPacketByPacket(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->tb_params.packet_by_packet, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->tb_params.packet_by_packet, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBNalUnitStream

        Functional description:
          Gets the integer values of TB NALU unit stream.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBNalUnitStream(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->tb_params.nal_unit_stream, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->tb_params.nal_unit_stream, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBStreamHeaderCorrupt

        Functional description:
          Gets the integer values of TB header corrupt.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBStreamHeaderCorrupt(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->tb_params.stream_header_corrupt, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->tb_params.stream_header_corrupt, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBStreamTruncate

        Functional description:
          Gets the integer values of TB stream truncate.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBStreamTruncate(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->tb_params.stream_truncate, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->tb_params.stream_truncate, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBStreamTruncate

        Functional description:
          Gets the integer values of TB stream truncate.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBSliceUdInPacket(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->tb_params.slice_ud_in_packet, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->tb_params.slice_ud_in_packet, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBmultiBuffer

        Functional description:


        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBMultiBuffer(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->pp_params.multi_buffer, "ENABLED") == 0) {
    return 1;
  } else if (strcmp(tb_cfg->pp_params.multi_buffer, "DISABLED") == 0) {
    return 0;
  } else {
    ASSERT(0);
    return -1;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecRefbuEvalMode

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecRefbuEvalMode(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecRefbuCheckpoint

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecRefbuCheckpoint(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecRefbuOffset

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecRefbuOffset(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecRefbuEnabled

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecRefbuEnabled(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDec64BitEnable

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDec64BitEnable(void) {
  return (DEC_X170_REFBU_WIDTH == 64);
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecBusWidth

        Functional description:
          Gets the integer values of TB disable reference buffer eval mode

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecBusWidth(const struct TBCfg* tb_cfg) {
  return tb_cfg->dec_params.bus_width;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetDecSupportNonCompliant

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecSupportNonCompliant(void) {
  return 1;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetHwConfig

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void TBGetHwConfig(const struct DecHwFeatures* hw_feature, DWLHwConfig* hw_cfg) {

  u32 tmp;

  hw_cfg->hevc_main10_support = hw_feature->hevc_main10_support;
  hw_cfg->vp9_10bit_support = hw_feature->vp9_profile2_support;
  hw_cfg->ds_support = hw_feature->dscale_support[0] |
                       hw_feature->dscale_support[1] |
                       hw_feature->dscale_support[2] |
                       hw_feature->dscale_support[3];
  hw_cfg->rfc_support = hw_feature->rfc_support;
  hw_cfg->ring_buffer_support = hw_feature->ring_buffer_support;
  hw_cfg->addr64_support = hw_feature->addr64_support;
  hw_cfg->mrb_prefetch = hw_feature->mrb_prefetch;
  hw_cfg->fmt_p010_support = hw_feature->fmt_p010_support;
  hw_cfg->fmt_customer1_support = hw_feature->fmt_customer1_support;

  hw_cfg->max_dec_pic_width = 0;
  hw_cfg->max_dec_pic_height = 0;
  hw_cfg->max_pp_out_pic_width = hw_feature->max_pp_out_pic_width[0];

  hw_cfg->h264_support = hw_feature->h264_support;
  hw_cfg->jpeg_support = hw_feature->jpeg_support;
  hw_cfg->mpeg4_support = hw_feature->mpeg4_support;
  hw_cfg->mpeg2_support = hw_feature->mpeg2_support;
  hw_cfg->vc1_support = hw_feature->vc1_support;
  hw_cfg->vp6_support = hw_feature->vp6_support;
  hw_cfg->vp7_support = hw_feature->vp7_support;
  hw_cfg->vp8_support = hw_feature->vp8_support;

  hw_cfg->custom_mpeg4_support = hw_feature->custom_mpeg4_support;
  hw_cfg->pp_support = hw_feature->pp_support;
  tmp = 0;
  if( hw_feature->dither_support )  tmp |= PP_DITHERING;
  if (hw_feature->dscale_support[0] || hw_feature->uscale_support[0]) {
    u32 scaling_bits;
    scaling_bits = 0x3;
    scaling_bits <<= 26;
    tmp |= scaling_bits; /* PP_SCALING */
  }

  hw_cfg->pp_config = tmp;
  hw_cfg->sorenson_spark_support = hw_feature->sorenson_spark_support;
  hw_cfg->ref_buf_support = hw_feature->ref_buf_support;
  hw_cfg->tiled_mode_support = hw_feature->tiled_mode_support;
  hw_cfg->field_dpb_support = hw_feature->field_dpb_support;
  hw_cfg->stride_support = hw_feature->dec_stride_support;

#ifdef DEC_X170_APF_DISABLE
  if (DEC_X170_APF_DISABLE) {
    hw_cfg->tiled_mode_support = 0;
  }
#endif /* DEC_X170_APF_DISABLE */

  hw_cfg->avs_support = hw_feature->avs_support;

  if (hw_feature->avs_support == 2)
    hw_cfg->avs_plus_support = 1;
  else
    hw_cfg->avs_plus_support = 0;

  hw_cfg->rv_support = hw_feature->rv_support;
  hw_cfg->jpeg_esupport = hw_feature->jpeg_esupport;
  hw_cfg->mvc_support = hw_feature->mvc_support;
  hw_cfg->webp_support = hw_feature->webp_support;
  hw_cfg->ec_support = hw_feature->ec_support;

  hw_cfg->double_buffer_support = 0;
}

#if 0
void TBRefbuTestMode( void *refbu, u32 *reg_base,
                      u32 is_intra_frame, u32 mode ) {
  extern struct TBCfg tb_cfg;
  struct refBuffer *p_refbu = (struct refBuffer *)refbu;

  if(tb_cfg.dec_params.ref_buffer_test_mode_offset_enable &&
      !is_intra_frame &&
      p_refbu->pic_width_in_mbs >= 16 ) {
    static u32 offset_test_step = 0;
    i32 offset_y;
    i32 max_yoffset;
    u32 range;
    i32 pic_height;
    i32 mv_limit;

    range = tb_cfg.dec_params.ref_buffer_test_mode_offset_max -
            tb_cfg.dec_params.ref_buffer_test_mode_offset_min + 1;

    offset_y = ( offset_test_step * tb_cfg.dec_params.ref_buffer_test_mode_offset_incr +
                 tb_cfg.dec_params.ref_buffer_test_mode_offset_start -
                 tb_cfg.dec_params.ref_buffer_test_mode_offset_min
               ) % range;
    offset_y += tb_cfg.dec_params.ref_buffer_test_mode_offset_min;
    pic_height = GetDecRegister( reg_base, HWIF_PIC_MB_HEIGHT_P );
    mv_limit = 48;
    if( mode == REFBU_FIELD )
      pic_height /= 2;
    else if( mode == REFBU_MBAFF )
      mv_limit = 64;

    max_yoffset = pic_height * 16 - mv_limit;
    if( offset_y > max_yoffset )
      offset_y = max_yoffset;
    if( offset_y < -max_yoffset )
      offset_y = -max_yoffset;

    SetDecRegister(reg_base, HWIF_REFBU_Y_OFFSET, offset_y );
    SetDecRegister(reg_base, HWIF_REFBU_E, 1 );
    SetDecRegister(reg_base, HWIF_REFBU_THR, 0 );
    SetDecRegister(reg_base, HWIF_REFBU_PICID, 1 );
    /* Eval mode left as-is */
    /*SetDecRegister(reg_base, HWIF_REFBU_EVAL_E, 0 ); */

    /* Disable double buffering */
    if(p_refbu->double_support) {
      SetDecRegister(reg_base, HWIF_REFBU2_BUF_E, 0 );
    }

    offset_test_step++;
  }
}
#endif

/*------------------------------------------------------------------------------
   <++>.<++>  Function: TBSetRefbuMemModel

        Functional description:
          Override reference buffer memory model parameters

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void TBSetRefbuMemModel( const struct TBCfg* tb_cfg, u32 *reg_base, void *refbu ) {
  struct refBuffer *p_refbu = (struct refBuffer *)refbu;
  p_refbu->bus_width_in_bits = 32 + 32*tb_cfg->dec_params.bus_width64bit_enable;
  p_refbu->curr_mem_model.latency = tb_cfg->dec_params.latency;
  p_refbu->curr_mem_model.nonseq = tb_cfg->dec_params.non_seq_clk;
  p_refbu->curr_mem_model.seq = tb_cfg->dec_params.seq_clk;
  p_refbu->data_excess_max_pct = tb_cfg->dec_params.refbu_data_excess_max_pct;
  p_refbu->mb_weight =
    p_refbu->dec_mode_mb_weights[tb_cfg->dec_params.bus_width64bit_enable];
  if( p_refbu->mem_access_stats_flag == 0 ) {
    if( tb_cfg->dec_params.bus_width64bit_enable && ! (DEC_X170_REFBU_WIDTH == 64) ) {
      p_refbu->mem_access_stats.seq >>= 1;
    } else if( !tb_cfg->dec_params.bus_width64bit_enable && (DEC_X170_REFBU_WIDTH == 64) ) {
      p_refbu->mem_access_stats.seq <<= 1;
    }
    p_refbu->mem_access_stats_flag = 1;
  }

  if( tb_cfg->dec_params.apf_threshold_value >= 0 )
    SetDecRegister( reg_base, HWIF_APF_THRESHOLD, tb_cfg->dec_params.apf_threshold_value);
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetIntraFreezeEnable

        Functional description:
          Override reference buffer memory model parameters

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecIntraFreezeEnable(const struct TBCfg* tb_cfg) {
  if (strcmp(tb_cfg->dec_params.error_concealment, "INTRA_FREEZE") == 0) {
    return 1;
  }
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetIntraFreezeEnable

        Functional description:
          Override reference buffer memory model parameters

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecDoubleBufferSupported(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetIntraFreezeEnable

        Functional description:
          Override reference buffer memory model parameters

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecTopBotSumSupported(void) {
  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBFirstTraceFrame

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetTBFirstTraceFrame(const struct TBCfg* tb_cfg) {
  return tb_cfg->tb_params.first_trace_frame;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: TBGetTBFirstTraceFrame

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 TBGetDecForceMpeg4Idct(void) {
  return 0;
}

u32 TBGetDecCh8PixIleavOutput(void) {
  return 0;
}

u32 TBGetDecApfThresholdEnabled(void) {
  return 0;
}

u32 TBGetDecServiceMergeDisable(const struct TBCfg* tb_cfg) {
  return tb_cfg->dec_params.service_merge_disable;
}

u32 TBGetPpFilterEnabled( const struct TBCfg* tb_cfg ) {
  return tb_cfg->pp_params.filter_enabled;
}

u32 TBGetPpPipelineEnabled( const struct TBCfg* tb_cfg ) {
  return tb_cfg->pp_params.pipeline_e;
}

