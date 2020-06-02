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
#include "jpegdeccontainer.h"
#include "jpegdecapi.h"
#include "jpegdecmarkers.h"
#include "jpegdecutils.h"
#include "jpegdechdrs.h"
#include "jpegdecscan.h"
#include "jpegregdrv.h"
#include "jpegdecinternal.h"
#include "dwl.h"
#include "deccfg.h"
#include "sw_util.h"
#include "regdrv.h"
#include "ppu.h"
#include "delogo.h"
#ifdef JPEGDEC_ASIC_TRACE
#include "jpegasicdbgtrace.h"
#endif /* #ifdef JPEGDEC_TRACE */

#ifdef JPEGDEC_PP_TRACE
#include "ppinternal.h"
#endif /* #ifdef JPEGDEC_PP_TRACE */
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

static const u8 zz_order[64] = {
  0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
  12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

#define JPEGDEC_SLICE_START_VALUE 0
#define JPEGDEC_VLC_LEN_START_REG 16
#define JPEGDEC_VLC_LEN_END_REG 29

#define EMPTY_SCAN_LEN 1024

#ifdef PJPEG_COMPONENT_TRACE
extern u32 pjpeg_component_id;
extern u32 *pjpeg_coeff_base;
extern u32 pjpeg_coeff_size;

#define TRACE_COMPONENT_ID(id) pjpeg_component_id = id
#else
#define TRACE_COMPONENT_ID(id)
#endif
static u32 pjpeg_coeff_size_for_cache = 0;
/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void JpegDecWriteTables(JpegDecContainer * dec_cont);
static void JpegDecWriteTablesNonInterleaved(JpegDecContainer * dec_cont);
static void JpegDecWriteTablesProgressive(JpegDecContainer * dec_cont);
static void JpegDecChromaTableSelectors(JpegDecContainer * dec_cont);
static void JpegDecSetHwStrmParams(JpegDecContainer * dec_cont);
static void JpegDecWriteLenBits(JpegDecContainer * dec_cont);
static void JpegDecWriteLenBitsNonInterleaved(JpegDecContainer * dec_cont);
static void JpegDecWriteLenBitsProgressive(JpegDecContainer * dec_cont);

/*------------------------------------------------------------------------------
    5. Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

        Function name: JpegDecClearStructs

        Functional description:
          handles the initialisation of jpeg decoder data structure

        Inputs:

        Outputs:
          Returns OK when successful, NOK in case unknown message type is
          asked

------------------------------------------------------------------------------*/
void JpegDecClearStructs(JpegDecContainer * dec_cont, u32 mode) {
  u32 i;

  ASSERT(dec_cont);

  /* stream pointers */
  dec_cont->stream.stream_bus = 0;
  dec_cont->stream.p_start_of_stream = NULL;
  dec_cont->stream.p_curr_pos = NULL;
  dec_cont->stream.bit_pos_in_byte = 0;
  dec_cont->stream.stream_length = 0;
  dec_cont->stream.read_bits = 0;
  dec_cont->stream.appn_flag = 0;
  dec_cont->stream.return_sos_marker = 0;

  /* output image pointers and variables */
  dec_cont->image.p_start_of_image = NULL;
  dec_cont->image.p_lum = NULL;
  dec_cont->image.p_cr = NULL;
  dec_cont->image.p_cb = NULL;
  dec_cont->image.header_ready = 0;
  dec_cont->image.image_ready = 0;
  dec_cont->image.ready = 0;
  dec_cont->image.size = 0;
  dec_cont->image.size_luma = 0;
  dec_cont->image.size_chroma = 0;
  for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
    dec_cont->image.columns[i] = 0;
    dec_cont->image.pixels_per_row[i] = 0;
  }

  /* frame info */
  dec_cont->frame.Lf = 0;
  dec_cont->frame.P = 0;
  //dec_cont->frame.Y = 0;
  //dec_cont->frame.X = 0;

  if(!mode) {
    dec_cont->frame.Y = 0;
    dec_cont->frame.X = 0;
    dec_cont->frame.hw_y = 0;
    dec_cont->frame.hw_x = 0;
    dec_cont->frame.hw_ytn = 0;
    dec_cont->frame.hw_xtn = 0;
    dec_cont->frame.full_x = 0;
    dec_cont->frame.full_y = 0;
    dec_cont->stream.thumbnail = 0;
  } else {
    if(dec_cont->stream.thumbnail) {
      dec_cont->frame.hw_y = dec_cont->frame.full_y;
      dec_cont->frame.hw_x = dec_cont->frame.full_x;
      dec_cont->stream.thumbnail = 0;
    }
  }

  dec_cont->frame.Nf = 0; /* Number of components in frame */
  dec_cont->frame.coding_type = 0;
  dec_cont->frame.num_mcu_in_frame = 0;
  dec_cont->frame.num_mcu_in_row = 0;
  dec_cont->frame.mcu_number = 0;
  dec_cont->frame.Ri = 0;
  dec_cont->frame.row = 0;
  dec_cont->frame.col = 0;
  dec_cont->frame.dri_period = 0;
  dec_cont->frame.block = 0;
  dec_cont->frame.c_index = 0;
  dec_cont->frame.buffer_bus = 0;

  if(!mode) {
    dec_cont->frame.p_buffer = NULL;
    dec_cont->frame.p_buffer_cb = NULL;
    dec_cont->frame.p_buffer_cr = NULL;
    for (i = 0; i < MAX_ASIC_CORES; i ++) {
      dec_cont->frame.p_table_base[i].virtual_address = NULL;
      dec_cont->frame.p_table_base[i].bus_address = 0;
    }

    /* asic buffer */
    dec_cont->asic_buff.out_luma_buffer.virtual_address = NULL;
    dec_cont->asic_buff.out_chroma_buffer.virtual_address = NULL;
    dec_cont->asic_buff.out_chroma_buffer2.virtual_address = NULL;
    if(dec_cont->pp_enabled && dec_cont->info.user_alloc_mem) {
      dec_cont->asic_buff.pp_luma_buffer.virtual_address = NULL;
      dec_cont->asic_buff.pp_chroma_buffer.virtual_address = NULL;
      dec_cont->asic_buff.pp_luma_buffer.bus_address = 0;
      dec_cont->asic_buff.pp_chroma_buffer.bus_address = 0;
    }
    dec_cont->asic_buff.out_luma_buffer.bus_address = 0;
    dec_cont->asic_buff.out_chroma_buffer.bus_address = 0;
    dec_cont->asic_buff.out_chroma_buffer2.bus_address = 0;
    dec_cont->asic_buff.out_luma_buffer.size = 0;
    dec_cont->asic_buff.out_chroma_buffer.size = 0;
    dec_cont->asic_buff.out_chroma_buffer2.size = 0;

    /* pp instance */
    dec_cont->pp_status = 0;
    dec_cont->pp_instance = NULL;
    dec_cont->PPRun = NULL;
    dec_cont->PPEndCallback = NULL;
    dec_cont->pp_control.use_pipeline = 0;

    /* resolution */
    dec_cont->min_supported_width = 0;
    dec_cont->min_supported_height = 0;
    dec_cont->max_supported_width = 0;
    dec_cont->max_supported_height = 0;
    dec_cont->max_supported_pixel_amount = 0;
    dec_cont->max_supported_slice_size = 0;

    /* out bus tmp */
    dec_cont->info.out_luma.virtual_address = NULL;
    dec_cont->info.out_chroma.virtual_address = NULL;
    if(dec_cont->pp_enabled && dec_cont->info.user_alloc_mem) {
      dec_cont->info.out_pp_luma.virtual_address = NULL;
      dec_cont->info.out_pp_chroma.virtual_address = NULL;
    }
    dec_cont->info.out_chroma2.virtual_address = NULL;

    /* user allocated addresses */
    dec_cont->info.given_out_luma.virtual_address = NULL;
    dec_cont->info.given_out_chroma.virtual_address = NULL;
    dec_cont->info.given_out_chroma2.virtual_address = NULL;
  }

  /* asic running flag */
  dec_cont->asic_running = 0;

  /* PJPEG flags */
  dec_cont->scan_num = 0;
  dec_cont->last_scan = 0;
  DWLmemset(dec_cont->pjpeg_coeff_bit_map, 0, sizeof(u16)*64*3);

  /* image handling info */
  dec_cont->info.slice_height = 0;
  dec_cont->info.amount_of_qtables = 0;
  dec_cont->info.y_cb_cr_mode = 0;
  dec_cont->info.column = 0;
  dec_cont->info.X = 0;
  dec_cont->info.Y = 0;
  dec_cont->info.mem_size = 0;
  dec_cont->info.SliceCount = 0;
  dec_cont->info.SliceMBCutValue = 0;
  dec_cont->info.pipeline = 0;
  if(!mode)
    dec_cont->info.user_alloc_mem = 0;
  dec_cont->info.slice_start_count = 0;
  dec_cont->info.amount_of_slices = 0;
  dec_cont->info.no_slice_irq_for_user = 0;
  dec_cont->info.SliceReadyForPause = 0;
  dec_cont->info.slice_limit_reached = 0;
  dec_cont->info.slice_mb_set_value = 0;
  dec_cont->info.timeout = (u32) DEC_X170_TIMEOUT_LENGTH;
  dec_cont->info.rlc_mode = 0; /* JPEG always in VLC mode == 0 */
  dec_cont->info.luma_pos = 0;
  dec_cont->info.chroma_pos = 0;
  dec_cont->info.fill_right = 0;
  dec_cont->info.fill_bottom = 0;
  dec_cont->info.stream_end = 0;
  dec_cont->info.stream_end_flag = 0;
  dec_cont->info.input_buffer_empty = 0;
  dec_cont->info.input_streaming = 0;
  dec_cont->info.input_buffer_len = 0;
  dec_cont->info.decoded_stream_len = 0;
  dec_cont->info.init = 0;
  dec_cont->info.init_thumb = 0;
  dec_cont->info.init_buffer_size = 0;

  /* progressive */
  dec_cont->info.non_interleaved = 0;
  dec_cont->info.component_id = 0;
  dec_cont->info.operation_type = 0;
  dec_cont->info.operation_type_thumb = 0;
  dec_cont->info.progressive_scan_ready = 0;
  dec_cont->info.non_interleaved_scan_ready = 0;

  if(!mode) {
    dec_cont->info.p_coeff_base.virtual_address = NULL;
    dec_cont->info.p_coeff_base.bus_address = 0;
    dec_cont->info.y_cb_cr_mode_orig = 0;
    dec_cont->info.get_info_ycb_cr_mode = 0;
    dec_cont->info.get_info_ycb_cr_mode_tn = 0;
  }

  dec_cont->info.allocated = 0;

  for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
    dec_cont->info.components[i] = 0;
    dec_cont->info.pred[i] = 0;
    dec_cont->info.dc_res[i] = 0;
    dec_cont->frame.num_blocks[i] = 0;
    dec_cont->frame.blocks_per_row[i] = 0;
    dec_cont->frame.use_ac_offset[i] = 0;
    dec_cont->frame.component[i].C = 0;
    dec_cont->frame.component[i].H = 0;
    dec_cont->frame.component[i].V = 0;
    dec_cont->frame.component[i].Tq = 0;
  }

  /* scan info */
  dec_cont->scan.Ls = 0;
  dec_cont->scan.Ns = 0;
  dec_cont->scan.Ss = 0;
  dec_cont->scan.Se = 0;
  dec_cont->scan.Ah = 0;
  dec_cont->scan.Al = 0;
  dec_cont->scan.index = 0;
  dec_cont->scan.num_idct_rows = 0;

  for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
    dec_cont->scan.Cs[i] = 0;
    dec_cont->scan.Td[i] = 0;
    dec_cont->scan.Ta[i] = 0;
    dec_cont->scan.pred[i] = 0;
  }

  /* huffman table lengths */
  dec_cont->vlc.default_tables = 0;
  dec_cont->vlc.ac_table0.table_length = 0;
  dec_cont->vlc.ac_table1.table_length = 0;
  dec_cont->vlc.ac_table2.table_length = 0;
  dec_cont->vlc.ac_table3.table_length = 0;

  dec_cont->vlc.dc_table0.table_length = 0;
  dec_cont->vlc.dc_table1.table_length = 0;
  dec_cont->vlc.dc_table2.table_length = 0;
  dec_cont->vlc.dc_table3.table_length = 0;

  /* Restart interval */
  dec_cont->frame.Ri = 0;

  if(dec_cont->vlc.ac_table0.vals) {
    DWLfree(dec_cont->vlc.ac_table0.vals);
  }
  if(dec_cont->vlc.ac_table1.vals) {
    DWLfree(dec_cont->vlc.ac_table1.vals);
  }
  if(dec_cont->vlc.ac_table2.vals) {
    DWLfree(dec_cont->vlc.ac_table2.vals);
  }
  if(dec_cont->vlc.ac_table3.vals) {
    DWLfree(dec_cont->vlc.ac_table3.vals);
  }
  if(dec_cont->vlc.dc_table0.vals) {
    DWLfree(dec_cont->vlc.dc_table0.vals);
  }
  if(dec_cont->vlc.dc_table1.vals) {
    DWLfree(dec_cont->vlc.dc_table1.vals);
  }
  if(dec_cont->vlc.dc_table2.vals) {
    DWLfree(dec_cont->vlc.dc_table2.vals);
  }
  if(dec_cont->vlc.dc_table3.vals) {
    DWLfree(dec_cont->vlc.dc_table3.vals);
  }
  if(dec_cont->frame.p_buffer) {
    DWLfree(dec_cont->frame.p_buffer);
  }

  /* pointer initialisation */
  dec_cont->vlc.ac_table0.vals = NULL;
  dec_cont->vlc.ac_table1.vals = NULL;
  dec_cont->vlc.ac_table2.vals = NULL;
  dec_cont->vlc.ac_table3.vals = NULL;

  dec_cont->vlc.dc_table0.vals = NULL;
  dec_cont->vlc.dc_table1.vals = NULL;
  dec_cont->vlc.dc_table2.vals = NULL;
  dec_cont->vlc.dc_table3.vals = NULL;

  dec_cont->frame.p_buffer = NULL;

  return;
}
/*------------------------------------------------------------------------------

        Function name: JpegDecInitHW

        Functional description:
          Set up HW regs for decode

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          Returns OK when successful, NOK in case unknown message type is
          asked

------------------------------------------------------------------------------*/
JpegDecRet JpegDecInitHW(JpegDecContainer * dec_cont) {
  u32 i;
  addr_t coeff_buffer = 0;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  u32 core_id = 0;
  struct DecHwFeatures hw_feature;
  ASSERT(dec_cont);

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  TRACE_COMPONENT_ID(dec_cont->info.component_id);

  /* Check if first InitHw call */
  if(dec_cont->info.slice_start_count == 0) {
    /* Check if HW resource is available */
    if (!dec_cont->vcmd_used) {
      if(DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_JPEG_DEC) == DWL_ERROR) {
        JPEGDEC_TRACE_INTERNAL(("JpegDecInitHW: ERROR hw resource unavailable"));
        return JPEGDEC_HW_RESERVED;
      }
    } else {
      /* Always store registers in jpeg_regs[0] when VCMD is used.*/
      dec_cont->core_id = 0;
      if (DWLReserveCmdBuf(dec_cont->dwl, DWL_CLIENT_TYPE_JPEG_DEC,
                           dec_cont->info.X, dec_cont->info.Y,
                           &dec_cont->cmd_buf_id) == DWL_ERROR) {
        JPEGDEC_TRACE_INTERNAL(("JpegDecInitHW: ERROR command buffer unavailable"));
        return JPEGDEC_HW_RESERVED;
      }
    }
  }

  core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
  /*************** Set swreg4 data ************/
  if (!hw_feature.pic_size_reg_unified) {
    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_W_EXT,
                   ((((dec_cont->info.X) >> (4)) & 0xE00) >> 9));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_WIDTH,
                   ((dec_cont->info.X) >> (4)) & 0x1FF);

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_H_EXT,
                   ((((dec_cont->info.Y) >> (4)) & 0x700) >> 8));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_HEIGHT_P,
                   ((dec_cont->info.Y) >> (4)) & 0x0FF);
  } else {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_WIDTH_IN_CBS,
                   ((dec_cont->info.X >> 4) << 1));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    if (RI_MC_DISABLED(dec_cont->ri_mc_enabled)) {
      SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_HEIGHT_IN_CBS,
                     (dec_cont->info.Y >> 4) << 1);
    } else {
      SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_HEIGHT_IN_CBS,
                     (MIN(dec_cont->frame.mcu_height *
                          dec_cont->frame.num_mcu_rows_in_interval,
                       (dec_cont->frame.num_mcu_in_frame - dec_cont->frame.Ri *
                        dec_cont->ri_index) / dec_cont->frame.num_mcu_in_row *
                        dec_cont->frame.mcu_height) >> 3));
    }
  }

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set decoding mode: JPEG\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_MODE, JPEG_X170_MODE_JPEG);

  if (dec_cont->hw_feature.pp_version == G1_NATIVE_PP) {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set output write enabled\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_OUT_DIS, 0);
  } else {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set output write disabled\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_OUT_DIS, 1);
  }

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set filtering disabled\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_FILTERING_DIS, 1);

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set amount of QP Table\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_QTABLES,
                 dec_cont->info.amount_of_qtables);

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set input format\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_MODE,
                 dec_cont->info.y_cb_cr_mode_orig);

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: RLC mode enable, JPEG == disable\n"));
  /* In case of JPEG: Always VLC mode used (0) */
  //SetDecRegister(dec_cont->jpeg_regs, HWIF_RLC_MODE_E, dec_cont->info.rlc_mode);

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Width is not multiple of 16\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_FILRIGHT_E,
                 dec_cont->info.fill_right);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_FILDOWN_E,
                 dec_cont->info.fill_bottom);

  /*************** Set swreg15 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set slice/full mode: 0 full; other = slice\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_SLICE_H,
                 dec_cont->info.slice_height);

  /*************** Set swreg52 data ************/
  if(dec_cont->info.operation_type != JPEGDEC_PROGRESSIVE) {
    /* Set JPEG operation mode */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_E, 0);
  } else {
    /* Set JPEG operation mode */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_E, 1);
    /* Only used in PJPEG 411 to indicate pic_width is filled to 32 or not */
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_WIDTH_DIV16,
                   dec_cont->info.wdiv16);
  }

  /* Set spectral selection start coefficient */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_SS, dec_cont->scan.Ss);

  /* Set spectral selection end coefficient */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_SE, dec_cont->scan.Se);

  /* Set the point transform used in the preceding scan */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AH, dec_cont->scan.Ah);

  /* Set the point transform value */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AL, dec_cont->scan.Al);

  /* Set needed progressive parameters */
  if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
    /* Set component ID */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG component ID, only actived in non-interleaved mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_COMPONENT_ID, dec_cont->info.component_id);

    /* Set flag to inidicate the data in the scan is interleaved or non-interleaved. */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG interleave flag\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_INTERLEAVED_E, !dec_cont->info.non_interleaved);

    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set PJPEG last scan flag\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_LAST_SCAN_E, dec_cont->last_scan);

    /* write coeff table base */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set coefficient buffer base address\n"));
    coeff_buffer = dec_cont->info.p_coeff_base.bus_address;
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_COEFF_BUF,
                 coeff_buffer);

    /* write Qtable selector */
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_QTABLE_SEL0,
                    dec_cont->frame.component[0].Tq); 
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_QTABLE_SEL1,
                    dec_cont->frame.component[1].Tq); 
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_QTABLE_SEL2,
                    dec_cont->frame.component[2].Tq); 
#if 0
    coeff_buffer += (JPEGDEC_COEFF_SIZE) * dec_cont->frame.num_blocks[0];
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_DCCB_BASE,
                 coeff_buffer);
    coeff_buffer += (JPEGDEC_COEFF_SIZE) * dec_cont->frame.num_blocks[1];
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_DCCR_BASE,
                 coeff_buffer);
#endif
  }

  /*************** Set swreg5/swreg6/swreg12/swreg16-swreg27 data ************/

  if(dec_cont->info.operation_type == JPEGDEC_BASELINE) {
    /* write "length amounts" */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write VLC length amounts to register\n"));
    JpegDecWriteLenBits(dec_cont);

    /* Create AC/DC/QP tables for HW */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write AC,DC,QP tables to base\n"));
    JpegDecWriteTables(dec_cont);

  } else if(dec_cont->info.operation_type == JPEGDEC_NONINTERLEAVED) {
    /* write "length amounts" */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write VLC length amounts to register\n"));
    JpegDecWriteLenBitsNonInterleaved(dec_cont);

    /* Create AC/DC/QP tables for HW */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write AC,DC,QP tables to base\n"));
    JpegDecWriteTablesNonInterleaved(dec_cont);
  } else {
    /* write "length amounts" */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write VLC length amounts to register\n"));
    JpegDecWriteLenBitsProgressive(dec_cont);

    /* Create AC/DC/QP tables for HW */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write AC,DC,QP tables to base\n"));
    JpegDecWriteTablesProgressive(dec_cont);
  }

  /* Select which tables the chromas use */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Select chroma AC,DC tables\n"));
  JpegDecChromaTableSelectors(dec_cont);

  /* write table base */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set AC,DC,QP table base address\n"));
  SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_QTABLE_BASE,
               dec_cont->frame.p_table_base[core_id].bus_address);

  /* set up stream position for HW decode */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set stream position for HW\n"));
  JpegDecSetHwStrmParams(dec_cont);

  /* set restart interval */
  if(dec_cont->frame.Ri) {
    SetDecRegister(dec_cont->jpeg_regs, HWIF_SYNC_MARKER_E, 1);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_REST_FREQ,
                   dec_cont->frame.Ri);
  } else
    SetDecRegister(dec_cont->jpeg_regs, HWIF_SYNC_MARKER_E, 0);

  /* Handle PP and output base addresses */

  /* PP depending register writes */
  if(dec_cont->pp_instance != NULL && dec_cont->pp_control.use_pipeline) {
    /*************** Set swreg4 data ************/

    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set output write disabled\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_OUT_DIS, 1);

    /* set output to zero, because of pp */
    /*************** Set swreg13 data ************/
    /* Luminance output */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set LUMA OUTPUT data base address\n"));
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE, (addr_t)0);

    /*************** Set swreg14 data ************/
    /* Chrominance output */
    if(dec_cont->image.size_chroma) {
      /* write output base */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set CHROMA OUTPUT data base address\n"));
      SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_JPG_CH_OUT_BASE, (addr_t)0);
    }

    if(dec_cont->info.slice_start_count == JPEGDEC_SLICE_START_VALUE) {
      /* Enable pp */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Enable pp\n"));
      dec_cont->PPRun(dec_cont->pp_instance, &dec_cont->pp_control);

      dec_cont->pp_control.pp_status = DECPP_RUNNING;
    }

    dec_cont->info.pipeline = 1;
  } else {
    if (dec_cont->hw_feature.pp_version == G1_NATIVE_PP) {
      /*************** Set swreg13 data ************/

      /* Luminance output */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set LUMA OUTPUT data base address\n"));

      if(dec_cont->info.operation_type == JPEGDEC_BASELINE) {
        SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                     dec_cont->asic_buff.out_luma_buffer.bus_address);

        /*************** Set swreg14 data ************/

        /* Chrominance output */
        if(dec_cont->image.size_chroma) {
          /* write output base */
          JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set CHROMA OUTPUT data base address\n"));
          SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_JPG_CH_OUT_BASE,
                       dec_cont->asic_buff.out_chroma_buffer.bus_address);
        }
      } else {
        if(dec_cont->info.component_id == 0) {
          SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                       dec_cont->asic_buff.out_luma_buffer.bus_address);
        } else if(dec_cont->info.component_id == 1) {
          SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                       dec_cont->asic_buff.out_chroma_buffer.bus_address);
        } else {
          SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                       (dec_cont->asic_buff.out_chroma_buffer2.
                        bus_address));
        }
      }

      dec_cont->info.pipeline = 0;
    } else {
      ASSERT(dec_cont->pp_enabled);
      if (dec_cont->pp_enabled /*&& ((asic_id & 0x0000F000) >> 12 >= 7)*/) {
        PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
        PpUnitIntConfig tmp_ppu_cfg[DEC_MAX_PPU_COUNT];
        DWLmemcpy(tmp_ppu_cfg, dec_cont->ppu_cfg, sizeof(dec_cont->ppu_cfg));
        if (RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
          u32 ri_height = dec_cont->frame.num_mcu_rows_in_interval *
                          dec_cont->frame.mcu_height;
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
            u32 full_crop_h = ppu_cfg->crop.height;
            u32 full_scale_h = ppu_cfg->scale.height;
            if (!ppu_cfg->enabled ||
                /* Not in cropping window. */
                dec_cont->ri_index < dec_cont->start_ri_index[i] ||
                dec_cont->ri_index > dec_cont->end_ri_index[i]) {
              ppu_cfg->enabled = 0;
              continue;
            }
            if (dec_cont->ri_index == dec_cont->start_ri_index[i]) {
              /* First restart interval:
               * Update:
               *    crop.y
               *    crop.height
               *    scale.height
               */
              /* Becareful of overflow & underflow, especially for 16/32K jpeg. */
              ppu_cfg->crop.y = ppu_cfg->crop.y % ri_height;
              ppu_cfg->crop.height = ri_height - ppu_cfg->crop.y;
            } else {
              /* Middle/last restart intervals:
               * Update:
               *    crop.y -> 0
               *    crop.height -> restart interval height / real height in ri
               *    scale.height
               *    luma_offset/chroma_offset
               */
              if (dec_cont->ri_index == dec_cont->end_ri_index[i]) {
                ppu_cfg->crop.height = (ppu_cfg->crop.y + ppu_cfg->crop.height) % ri_height;
                if (!ppu_cfg->crop.height)
                  ppu_cfg->crop.height = ri_height;
              } else {
                ppu_cfg->crop.height = ri_height;
              }
              ppu_cfg->luma_offset += ppu_cfg->luma_size / full_crop_h *
                                      (dec_cont->ri_index * ri_height -
                                       ppu_cfg->crop.y);
              ppu_cfg->chroma_offset += ppu_cfg->chroma_size / full_crop_h *
                                        (dec_cont->ri_index * ri_height -
                                         ppu_cfg->crop.y);
              ppu_cfg->crop.y = 0;
            }
            ppu_cfg->scale.height = ppu_cfg->crop.height *
                                    full_scale_h / full_crop_h;
          }
        }
        /* PPSetRegs need to add slice_mb_set_value support in future  */
        DelogoSetRegs(dec_cont->jpeg_regs, &hw_feature, dec_cont->delogo_params);
        if (dec_cont->vcmd_used && dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE &&
          !dec_cont->last_scan && !dec_cont->ppu_saved) {
          /* Disable pp for all the pjpeg interim scans. */
          int i;
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
            dec_cont->ppu_enabled_orig[i] = ppu_cfg[i].enabled;
            ppu_cfg[i].enabled = 0;
          }
          dec_cont->ppu_saved = 1;
        }
        PPSetRegs(dec_cont->jpeg_regs, &hw_feature, dec_cont->ppu_cfg,
                  dec_cont->asic_buff.pp_luma_buffer.bus_address, 0, 0);
        if (RI_MC_ENABLED(dec_cont->ri_mc_enabled)) {
          /* Restore ppu config. */
          DWLmemcpy(dec_cont->ppu_cfg, tmp_ppu_cfg, sizeof(dec_cont->ppu_cfg));
          dec_cont->ri_running_cores++;
        }
      }
      /* Crop when hight is multiple of 8 instead of 16. */
      if (((dec_cont->frame.Y + 7) >> 3) & 1) {
        SetDecRegister(dec_cont->jpeg_regs, HWIF_MB_HEIGHT_OFF, 8);
      } else
        SetDecRegister(dec_cont->jpeg_regs, HWIF_MB_HEIGHT_OFF, 0);
    }
  }
  /* In slice decoding mode, HW is only reserved once, but HW must be reserved for each
   * ri in ri-based multicore decoding. */
  if (!RI_MC_ENABLED(dec_cont->ri_mc_enabled))
    dec_cont->info.slice_start_count = 1;

#ifdef JPEGDEC_ASIC_TRACE
  {
    FILE *fd;

    fd = fopen("picture_ctrl_dec.trc", "at");
    DumpJPEGCtrlReg(dec_cont->jpeg_regs, fd);
    fclose(fd);

    fd = fopen("picture_ctrl_dec.hex", "at");
    HexDumpJPEGCtrlReg(dec_cont->jpeg_regs, fd);
    fclose(fd);

    fd = fopen("jpeg_tables.hex", "at");
    HexDumpJPEGTables(dec_cont->jpeg_regs, dec_cont, fd);
    fclose(fd);

    fd = fopen("registers.hex", "at");
    HexDumpRegs(dec_cont->jpeg_regs, fd);
    fclose(fd);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

#ifdef JPEGDEC_PP_TRACE
  ppRegDump(((PPContainer_t *) dec_cont->pp_instance)->pp_regs);
#endif /* #ifdef JPEGDEC_PP_TRACE */
  dec_cont->asic_running = 1;

  /* Enable jpeg mode and set slice mode */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Enable jpeg\n"));
  DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, NULL, pjpeg_coeff_size_for_cache);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_E, 1);
  
  /* Flush regs to hw register */
  JpegFlushRegs(dec_cont);

  if (!dec_cont->vcmd_used)
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->jpeg_regs[1]);
  else
    DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);

  return JPEGDEC_OK;
}

/*------------------------------------------------------------------------------

        Function name: JpegDecInitHWContinue

        Functional description:
          Set up HW regs for decode

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          Returns OK when successful, NOK in case unknown message type is
          asked

------------------------------------------------------------------------------*/
void JpegDecInitHWContinue(JpegDecContainer * dec_cont) {

  ASSERT(dec_cont);
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* update slice counter */
  dec_cont->info.amount_of_slices++;

  if(dec_cont->pp_instance == NULL &&
      dec_cont->info.user_alloc_mem == 1 && dec_cont->info.slice_start_count > 0) {
    /* if user allocated memory ==> new addresses */
    dec_cont->asic_buff.out_luma_buffer.virtual_address =
      dec_cont->info.given_out_luma.virtual_address;
    dec_cont->asic_buff.out_luma_buffer.bus_address =
      dec_cont->info.given_out_luma.bus_address;
    dec_cont->asic_buff.out_chroma_buffer.virtual_address =
      dec_cont->info.given_out_chroma.virtual_address;
    dec_cont->asic_buff.out_chroma_buffer.bus_address =
      dec_cont->info.given_out_chroma.bus_address;
  }

  /* Update only register/values that might have been changed */

  /*************** Set swreg1 data ************/
  /* clear status bit */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_SLICE_INT, 0);

  /*************** Set swreg5 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set stream last buffer bit\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_STREAM_ALL,
                 dec_cont->info.stream_end);

  /*************** Set swreg13 data ************/
  /* PP depending register writes */
  if(dec_cont->pp_instance == NULL) {
    /* Luminance output */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set LUMA OUTPUT data base address\n"));
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                 dec_cont->asic_buff.out_luma_buffer.bus_address);

    /*************** Set swreg14 data ************/

    /* Chrominance output */
    if(dec_cont->image.size_chroma) {
      /* write output base */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set CHROMA OUTPUT data base address\n"));
      SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_JPG_CH_OUT_BASE,
                   dec_cont->asic_buff.out_chroma_buffer.bus_address);
    }

    dec_cont->info.pipeline = 0;
  }

  /*************** Set swreg13 data ************/
  /* PP depending register writes */
  if(dec_cont->pp_instance != NULL && dec_cont->pp_control.use_pipeline == 0) {
    if(dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV420) {
      dec_cont->info.luma_pos = (dec_cont->info.X *
                                 (dec_cont->info.slice_mb_set_value * 16));
      dec_cont->info.chroma_pos = ((dec_cont->info.X) *
                                   (dec_cont->info.slice_mb_set_value * 8));
    } else if(dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV422) {
      dec_cont->info.luma_pos = (dec_cont->info.X *
                                 (dec_cont->info.slice_mb_set_value * 16));
      dec_cont->info.chroma_pos = ((dec_cont->info.X) *
                                   (dec_cont->info.slice_mb_set_value * 16));
    } else if(dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV440) {
      dec_cont->info.luma_pos = (dec_cont->info.X *
                                 (dec_cont->info.slice_mb_set_value * 16));
      dec_cont->info.chroma_pos = ((dec_cont->info.X) *
                                   (dec_cont->info.slice_mb_set_value * 16));
    } else {
      dec_cont->info.luma_pos = (dec_cont->info.X *
                                 (dec_cont->info.slice_mb_set_value * 16));
      dec_cont->info.chroma_pos = 0;
    }

    /* update luma/chroma position */
    dec_cont->info.luma_pos = (dec_cont->info.luma_pos *
                               dec_cont->info.amount_of_slices);
    if(dec_cont->info.chroma_pos) {
      dec_cont->info.chroma_pos = (dec_cont->info.chroma_pos *
                                   dec_cont->info.amount_of_slices);
    }

    /* Luminance output */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set LUMA OUTPUT data base address\n"));
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_DEC_OUT_BASE,
                 dec_cont->asic_buff.out_luma_buffer.bus_address +
                 dec_cont->info.luma_pos);

    /*************** Set swreg14 data ************/

    /* Chrominance output */
    if(dec_cont->image.size_chroma) {
      /* write output base */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set CHROMA OUTPUT data base address\n"));
      SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_JPG_CH_OUT_BASE,
                   dec_cont->asic_buff.out_chroma_buffer.bus_address +
                   dec_cont->info.chroma_pos);
    }

    dec_cont->info.pipeline = 0;
  }

  /*************** Set swreg15 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set slice/full mode: 0 full; other = slice\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_SLICE_H,
                 dec_cont->info.slice_height);

  /* Flush regs to hw register */
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x34,
              dec_cont->jpeg_regs[13]);
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x38,
              dec_cont->jpeg_regs[14]);
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x3C,
              dec_cont->jpeg_regs[15]);
  if(sizeof(addr_t) == 8) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x1EC,
                dec_cont->jpeg_regs[123]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x1F0,
                dec_cont->jpeg_regs[124]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x1F4,
                dec_cont->jpeg_regs[125]);
  }
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x14,
              dec_cont->jpeg_regs[5]);
  DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 0x4,
              dec_cont->jpeg_regs[1]);

#ifdef JPEGDEC_ASIC_TRACE
  {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: REGS BEFORE IRQ CLEAN\n"));
    PrintJPEGReg(dec_cont->jpeg_regs);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

}

/*------------------------------------------------------------------------------

        Function name: JpegDecInitHWInputBuffLoad

        Functional description:
          Set up HW regs for decode after input buffer load

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          Returns OK when successful, NOK in case unknown message type is
          asked

------------------------------------------------------------------------------*/
void JpegDecInitHWInputBuffLoad(JpegDecContainer * dec_cont) {
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  ASSERT(dec_cont);

  /* Update only register/values that might have been changed */
  /*************** Set swreg4 data ************/

  if (!hw_feature.pic_size_reg_unified) {
    /* frame size, round up the number of mbs */

    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_W_EXT,
                   ((((dec_cont->info.X) >> (4)) & 0xE00) >> 9));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_WIDTH,
                   ((dec_cont->info.X) >> (4)) & 0x1FF);

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_H_EXT,
                   ((((dec_cont->info.Y) >> (4)) & 0x700) >> 8));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_HEIGHT_P,
                   ((dec_cont->info.Y) >> (4)) & 0x0FF);
  } else {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_WIDTH_IN_CBS,
                   ((dec_cont->info.X >> 4) << 1));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_HEIGHT_IN_CBS,
                   (dec_cont->info.Y >> 4) << 1);
  }

  /* Only used in PJPEG 411 to indicate pic_width is filled to 32 or not */
  if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE)
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_WIDTH_DIV16,
                   dec_cont->info.wdiv16);

  /*************** Set swreg5 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Set stream start bit\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_BIT,
                 dec_cont->stream.bit_pos_in_byte);

  /*************** Set swreg6 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Set stream length\n"));

  /* check if all stream will processed in this buffer */
  if((dec_cont->info.decoded_stream_len) >= dec_cont->stream.stream_length) {
    dec_cont->info.stream_end = 1;
  }

  SetDecRegister(dec_cont->jpeg_regs, HWIF_STREAM_LEN,
                 dec_cont->info.input_buffer_len);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->info.input_buffer_len);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_OFFSET, 0);


  /*************** Set swreg4 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Set stream last buffer bit\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_STREAM_ALL,
                 dec_cont->info.stream_end);

  /*************** Set swreg12 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Set stream start address\n"));
  SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_RLC_VLC_BASE,
               dec_cont->stream.stream_bus);
  dec_cont->jpeg_hw_start_bus = dec_cont->stream.stream_bus;

  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Stream bus start 0x%08x\n",
                          dec_cont->stream.stream_bus));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Bit position 0x%08x\n",
                          dec_cont->stream.bit_pos_in_byte));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: Stream length 0x%08x\n",
                          dec_cont->stream.stream_length));

  /* Flush regs to hw register */
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x30,
              dec_cont->jpeg_regs[12]);
  if(sizeof(addr_t) == 8) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x1E8,
                dec_cont->jpeg_regs[122]);
  }
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x10,
              dec_cont->jpeg_regs[4]);
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x14,
              dec_cont->jpeg_regs[5]);
  DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x18,
              dec_cont->jpeg_regs[6]);
  DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 0x04,
              dec_cont->jpeg_regs[1]);

#ifdef JPEGDEC_ASIC_TRACE
  {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL BUFFER LOAD: REGS BEFORE IRQ CLEAN\n"));
    PrintJPEGReg(dec_cont->jpeg_regs);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

}

/*------------------------------------------------------------------------------

        Function name: JpegDecInitHWProgressiveContinue

        Functional description:
          Set up HW regs for decode after progressive scan decoded

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          Returns OK when successful, NOK in case unknown message type is
          asked

------------------------------------------------------------------------------*/
void JpegDecInitHWProgressiveContinue(JpegDecContainer * dec_cont) {

  addr_t coeff_buffer = 0;
  u32 asic_id = DWLReadAsicID(DWL_CLIENT_TYPE_JPEG_DEC);
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;
  ASSERT(dec_cont);

  /* Check if first InitHw call */
  if (!dec_cont->vcmd_used) {
    if(dec_cont->info.slice_start_count == 0) {
      /* Check if HW resource is available */
      if(DWLReserveHw(dec_cont->dwl, &dec_cont->core_id, DWL_CLIENT_TYPE_JPEG_DEC) == DWL_ERROR) {
        JPEGDEC_TRACE_INTERNAL(("JpegDecInitHW: ERROR hw resource unavailable"));
        return;
      }
    }
  } else {
    /* When VCMD is enabled, for each hw core will be reserved and released. */
    /* Always store registers in jpeg_regs[0] when VCMD is used.*/
    dec_cont->core_id = 0;
    if (DWLReserveCmdBuf(dec_cont->dwl, DWL_CLIENT_TYPE_JPEG_DEC,
                         dec_cont->info.X, dec_cont->info.Y,
                         &dec_cont->cmd_buf_id) == DWL_ERROR) {
      JPEGDEC_TRACE_INTERNAL(("JpegDecInitHW: ERROR command buffer unavailable"));
      return;
    }
  }

  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(dec_cont->pp_instance == NULL && dec_cont->info.user_alloc_mem == 1) {
    /* if user allocated memory ==> new addresses */
    dec_cont->asic_buff.out_luma_buffer.virtual_address =
      dec_cont->info.given_out_luma.virtual_address;
    dec_cont->asic_buff.out_luma_buffer.bus_address =
      dec_cont->info.given_out_luma.bus_address;
    dec_cont->asic_buff.out_chroma_buffer.virtual_address =
      dec_cont->info.given_out_chroma.virtual_address;
    dec_cont->asic_buff.out_chroma_buffer.bus_address =
      dec_cont->info.given_out_chroma.bus_address;
    dec_cont->asic_buff.out_chroma_buffer2.virtual_address =
      dec_cont->info.given_out_chroma2.virtual_address;
    dec_cont->asic_buff.out_chroma_buffer2.bus_address =
      dec_cont->info.given_out_chroma2.bus_address;
  }

  TRACE_COMPONENT_ID(dec_cont->info.component_id);
  /* Update only register/values that might have been changed */

  if (dec_cont->pp_enabled && !dec_cont->info.user_alloc_mem) {
    if (dec_cont->pp_enabled && ((asic_id & 0x0000F000) >> 12 >= 7)) {
      PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
      addr_t ppu_out_bus_addr = dec_cont->asic_buff.pp_luma_buffer.bus_address;
      if (dec_cont->vcmd_used && dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE &&
          !dec_cont->last_scan && !dec_cont->ppu_saved) {
        /* Disable pp for all the pjpeg internal scans. */
        int i;
        for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
          dec_cont->ppu_enabled_orig[i] = ppu_cfg[i].enabled;
          ppu_cfg[i].enabled = 0;
        }
        dec_cont->ppu_saved = 1;
      }
      /* PPSetRegs need to add slice_mb_set_value support in future  */
      PPSetRegs(dec_cont->jpeg_regs, &hw_feature, ppu_cfg, ppu_out_bus_addr, 0, 0);
      DelogoSetRegs(dec_cont->jpeg_regs, &hw_feature, dec_cont->delogo_params);
    }
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PP_IN_FORMAT_U, 1);
  }
  dec_cont->info.pipeline = 0;

  /* set up stream position for HW decode */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set stream position for HW\n"));

  if (!dec_cont->fake_empty_scan)
    JpegDecSetHwStrmParams(dec_cont);

  /*************** Set swreg5 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set input format\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_MODE,
                 dec_cont->info.y_cb_cr_mode_orig);

  if (!hw_feature.pic_size_reg_unified) {
    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_W_EXT,
                   ((((dec_cont->info.X) >> (4)) & 0xE00) >> 9));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_WIDTH,
                   ((dec_cont->info.X) >> (4)) & 0x1FF);

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height extension\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_H_EXT,
                   ((((dec_cont->info.Y) >> (4)) & 0x700) >> 8));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_MB_HEIGHT_P,
                   ((dec_cont->info.Y) >> (4)) & 0x0FF);
  } else {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame width\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_WIDTH_IN_CBS,
                   ((dec_cont->info.X >> 4) << 1));

    /* frame size, round up the number of mbs */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set Frame height\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PIC_HEIGHT_IN_CBS,
                   (dec_cont->info.Y >> 4) << 1);
  }

  /* Only used in PJPEG 411 to indicate pic_width is filled to 32 or not */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_WIDTH_DIV16,
                 dec_cont->info.wdiv16);

//  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_WDIV8, dec_cont->info.fill_x);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_FILRIGHT_E,
                 dec_cont->info.fill_x || dec_cont->info.fill_right);
//  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_HDIV8, dec_cont->info.fill_y);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_FILDOWN_E,
                 dec_cont->info.fill_y || dec_cont->info.fill_bottom);

  /*************** Set swreg52 data ************/
  /* Set JPEG operation mode */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  if(dec_cont->info.operation_type != JPEGDEC_PROGRESSIVE) {
    /* Set JPEG operation mode */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_E, 0);
  } else {
    /* Set JPEG operation mode */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_E, 1);
  }

  /* Set spectral selection start coefficient */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_SS, dec_cont->scan.Ss);

  /* Set spectral selection end coefficient */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_SE, dec_cont->scan.Se);

  /* Set the point transform used in the preceding scan */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AH, dec_cont->scan.Ah);

  /* Set the point transform value */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG operation mode\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AL, dec_cont->scan.Al);

  /* Set needed progressive parameters */
  if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
    /* Set component ID */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG component ID, only actived in non-interleaved mode\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_COMPONENT_ID, dec_cont->info.component_id);

    /* Set flag to inidicate the data in the scan is interleaved or non-interleaved. */
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set JPEG interleave flag\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_INTERLEAVED_E, !dec_cont->info.non_interleaved);

    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set PJPEG last scan flag\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_LAST_SCAN_E, dec_cont->last_scan);

    JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set coefficient buffer base address\n"));
    coeff_buffer = dec_cont->info.p_coeff_base.bus_address;
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_COEFF_BUF,
                 coeff_buffer);
#if 0
    coeff_buffer += (JPEGDEC_COEFF_SIZE) * dec_cont->frame.num_blocks[0];
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_DCCB_BASE,
                 coeff_buffer);
    coeff_buffer += (JPEGDEC_COEFF_SIZE) * dec_cont->frame.num_blocks[1];
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_PJPEG_DCCR_BASE,
                 coeff_buffer);
#endif
  }

  /* write "length amounts" */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write VLC length amounts to register\n"));
  JpegDecWriteLenBitsProgressive(dec_cont);

  /* Create AC/DC/QP tables for HW */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write AC,DC,QP tables to base\n"));
  JpegDecWriteTablesProgressive(dec_cont);

  /* Select which tables the chromas use */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Select chroma AC,DC tables\n"));
  JpegDecChromaTableSelectors(dec_cont);

  /* write table base */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set AC,DC,QP table base address\n"));
  SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_QTABLE_BASE,
               dec_cont->frame.p_table_base[core_id].bus_address);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_QTABLES,
                 dec_cont->info.amount_of_qtables);

  if(dec_cont->info.slice_mb_set_value) {
    /*************** Set swreg15 data ************/
    JPEGDEC_TRACE_INTERNAL(("INTERNAL CONTINUE: Set slice/full mode: 0 full; other = slice\n"));
    SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_SLICE_H,
                   dec_cont->info.slice_height);
  }
#if 1
  /* set restart interval */
  if(dec_cont->frame.Ri) {
    SetDecRegister(dec_cont->jpeg_regs, HWIF_SYNC_MARKER_E, 1);
    /* TODO: Why set HWIF_REFER13_BASE here? Typo? */
#if 0
    SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_REFER13_BASE,
                 dec_cont->frame.Ri);
#else
    SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_REST_FREQ,
                   dec_cont->frame.Ri);
#endif
  } else
    SetDecRegister(dec_cont->jpeg_regs, HWIF_SYNC_MARKER_E, 0);
#endif
  dec_cont->asic_running = 1;

  /* Enable jpeg mode and set slice mode */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Enable jpeg\n"));
  DWLReadPpConfigure(dec_cont->dwl, dec_cont->ppu_cfg, NULL, pjpeg_coeff_size_for_cache);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DEC_E, 1);

  /* Flush regs to hw register */
  JpegFlushRegs(dec_cont);

  if (!dec_cont->vcmd_used)
    DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->jpeg_regs[1]);
  else
    DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmd_buf_id);

#ifdef JPEGDEC_ASIC_TRACE
  {
    JPEGDEC_TRACE_INTERNAL(("PROGRESSIVE CONTINUE: REGS BEFORE IRQ CLEAN\n"));
    PrintJPEGReg(dec_cont->jpeg_regs);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

}

/*------------------------------------------------------------------------------

        Function name: JpegDecSetHwStrmParams

        Functional description:
          set up hw stream start position

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecSetHwStrmParams(JpegDecContainer * dec_cont) {

#define JPG_STR     dec_cont->stream

  addr_t addr_tmp = 0;
  u32 amount_of_stream = 0;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;
  addr_t mask;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  /* calculate and set stream start address to hw */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: read bits %d\n", JPG_STR.read_bits));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: read bytes %d\n", JPG_STR.read_bits / 8));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Stream bus start 0x%08x\n",
                          JPG_STR.stream_bus));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Stream virtual start 0x%08x\n",
                          JPG_STR.p_start_of_stream));

  if (hw_feature.g1_strm_128bit_align)
    mask = 15;
  else
    mask = 7;

  /* calculate and set stream start address to hw */
  addr_tmp = (JPG_STR.stream_bus +
              (addr_t)(JPG_STR.p_curr_pos - JPG_STR.p_start_of_stream)) & (~mask);

  SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_RLC_VLC_BASE, addr_tmp);
  dec_cont->jpeg_hw_start_bus = addr_tmp;

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Stream bus start 0x%08x\n",
                          JPG_STR.stream_bus));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Start Addr 0x%08x\n",
                          GetJpegDecStreamStartAddress(dec_cont->jpeg_regs)));

  /* calculate and set stream start bit to hw */

  /* change current pos to bus address style */
  /* remove three lowest bits and add the difference to bitPosInWord */
  /* used as bit pos in word not as bit pos in byte actually... */
#if 0
  switch ((addr_t) JPG_STR.p_curr_pos & (7)) {
  case 0:
    break;
  case 1:
    JPG_STR.bit_pos_in_byte += 8;
    break;
  case 2:
    JPG_STR.bit_pos_in_byte += 16;
    break;
  case 3:
    JPG_STR.bit_pos_in_byte += 24;
    break;
  case 4:
    JPG_STR.bit_pos_in_byte += 32;
    break;
  case 5:
    JPG_STR.bit_pos_in_byte += 40;
    break;
  case 6:
    JPG_STR.bit_pos_in_byte += 48;
    break;
  case 7:
    JPG_STR.bit_pos_in_byte += 56;
    break;
  default:
    ASSERT(0);
    break;
  }
#endif

  JPG_STR.bit_pos_in_byte += ((addr_t) JPG_STR.p_curr_pos & mask) * 8;
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_BIT,
                 JPG_STR.bit_pos_in_byte);

  /* set up stream length for HW.
   * length = size of original buffer - stream we already decoded in SW */
  JPG_STR.p_curr_pos = (u8 *) ((addr_t) JPG_STR.p_curr_pos & (~mask));

  if(dec_cont->info.input_streaming) {
    amount_of_stream = (dec_cont->info.input_buffer_len -
                        (u32) (JPG_STR.p_curr_pos - JPG_STR.p_start_of_stream));

    SetDecRegister(dec_cont->jpeg_regs, HWIF_STREAM_LEN, amount_of_stream);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_BUFFER_LEN, dec_cont->info.input_buffer_len);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_OFFSET, addr_tmp - JPG_STR.stream_bus);
  } else {
    amount_of_stream = (JPG_STR.stream_length -
                        (u32) (JPG_STR.p_curr_pos - JPG_STR.p_start_of_stream));

    SetDecRegister(dec_cont->jpeg_regs, HWIF_STREAM_LEN, amount_of_stream);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_BUFFER_LEN, amount_of_stream);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_OFFSET, 0);

    /* because no input streaming, frame should be ready during decoding this buffer */
    dec_cont->info.stream_end = 1;
  }

  /*************** Set swreg4 data ************/
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Set stream last buffer bit\n"));
  SetDecRegister(dec_cont->jpeg_regs, HWIF_JPEG_STREAM_ALL,
                 dec_cont->info.stream_end);

  JPEGDEC_TRACE_INTERNAL(("INTERNAL: JPG_STR.stream_length %d\n",
                          JPG_STR.stream_length));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: JPG_STR.p_curr_pos 0x%08x\n",
                          (u32) JPG_STR.p_curr_pos));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: JPG_STR.p_start_of_stream 0x%08x\n",
                          (u32) JPG_STR.p_start_of_stream));
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: JPG_STR.bit_pos_in_byte 0x%08x\n",
                          JPG_STR.bit_pos_in_byte));

  return;

#undef JPG_STR

}

/*------------------------------------------------------------------------------

        Function name: JpegDecAllocateResidual

        Functional description:
          Allocates residual buffer

        Inputs:
          JpegDecContainer *dec_cont  Pointer to DecData structure

        Outputs:
          OK
          JPEGDEC_MEMFAIL

------------------------------------------------------------------------------*/
JpegDecRet JpegDecAllocateResidual(JpegDecContainer * dec_cont) {

  i32 tmp = JPEGDEC_ERROR;
  u32 num_blocks = 0;
  u32 i;
  u32 table_size = 0;

  ASSERT(dec_cont);

  if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
    for(i = 0; i < dec_cont->frame.Nf; i++) {
      num_blocks += dec_cont->frame.num_blocks[i];
    }

    /* allocate coefficient buffer */
    tmp = DWLMallocLinear(dec_cont->dwl, (sizeof(u8) * (JPEGDEC_COEFF_SIZE *
                                          num_blocks)),
                          &(dec_cont->info.p_coeff_base));
    DWLmemset(dec_cont->info.p_coeff_base.virtual_address, 0, num_blocks * JPEGDEC_COEFF_SIZE);
    if(tmp == -1) {
      return (JPEGDEC_MEMFAIL);
    }
#ifdef PJPEG_COMPONENT_TRACE
    pjpeg_coeff_base = dec_cont->info.p_coeff_base.virtual_address;
    pjpeg_coeff_size = num_blocks * JPEGDEC_COEFF_SIZE;
#endif
    pjpeg_coeff_size_for_cache = num_blocks * JPEGDEC_COEFF_SIZE;
    JPEGDEC_TRACE_INTERNAL(("ALLOCATE: COEFF virtual %x bus %x\n",
                            (u32) dec_cont->info.p_coeff_base.virtual_address,
                            dec_cont->info.p_coeff_base.bus_address));

    tmp = DWLMallocLinear(dec_cont->dwl, sizeof(u8) * EMPTY_SCAN_LEN,
                          &dec_cont->info.tmp_strm);
    if(tmp == -1) {
      return (JPEGDEC_MEMFAIL);
    }
  }

  /* QP/VLC memory size */
  if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE)
    table_size = JPEGDEC_PROGRESSIVE_TABLE_SIZE;
  else
    table_size = JPEGDEC_BASELINE_TABLE_SIZE;

  /* allocate VLC/QP table */
  if(dec_cont->frame.p_table_base[0].virtual_address == NULL) {
    for (i = 0; i < dec_cont->n_cores; i++) {
      tmp = DWLMallocLinear(dec_cont->dwl, (sizeof(u8) * table_size),
                             &(dec_cont->frame.p_table_base[i]));
      if(tmp == -1) {
        return (JPEGDEC_MEMFAIL);
      }
    }
  }

  if (!dec_cont->b_mc) {
    JPEGDEC_TRACE_INTERNAL(("ALLOCATE: VLC/QP virtual %x bus %x\n",
                            (u32) dec_cont->frame.p_table_base.virtual_address,
                            dec_cont->frame.p_table_base.bus_address));
  }
  if(dec_cont->pp_instance != NULL) {
    dec_cont->pp_config_query.tiled_mode = 0;
    dec_cont->PPConfigQuery(dec_cont->pp_instance, &dec_cont->pp_config_query);

    dec_cont->pp_control.use_pipeline =
      dec_cont->pp_config_query.pipeline_accepted;

    if(!dec_cont->pp_control.use_pipeline) {
      dec_cont->image.size_luma = (dec_cont->info.X * dec_cont->info.Y);
      if(dec_cont->image.size_chroma) {
        if(dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV420)
          dec_cont->image.size_chroma = (dec_cont->image.size_luma / 2);
        else if(dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV422 ||
                dec_cont->info.y_cb_cr_mode == JPEGDEC_YUV440)
          dec_cont->image.size_chroma = dec_cont->image.size_luma;
      }
    }
  }

  /* if pipelined PP -> decoder's output is not written external memory */
  if(dec_cont->pp_instance == NULL ||
      (dec_cont->pp_instance != NULL && !dec_cont->pp_control.use_pipeline)) {
    if(dec_cont->info.given_out_luma.virtual_address == NULL) {
      /* allocate luminance output */

      if (dec_cont->hw_feature.pp_version == G1_NATIVE_PP) {
        if(dec_cont->asic_buff.out_luma_buffer.virtual_address == NULL) {
          tmp =
            DWLMallocRefFrm(dec_cont->dwl, (dec_cont->image.size_luma),
                            &(dec_cont->asic_buff.out_luma_buffer));
          if(tmp == -1) {
            return (JPEGDEC_MEMFAIL);
          }
        }
      }

      /* luma bus address to output */
      dec_cont->info.out_luma = dec_cont->asic_buff.out_luma_buffer;
      if (dec_cont->pp_enabled) {
        dec_cont->info.out_pp_luma = dec_cont->asic_buff.pp_luma_buffer;
        dec_cont->info.out_pp_chroma = dec_cont->asic_buff.pp_chroma_buffer;
      }
    } else {
      dec_cont->asic_buff.out_luma_buffer.virtual_address =
        dec_cont->info.given_out_luma.virtual_address;
      dec_cont->asic_buff.out_luma_buffer.bus_address =
        dec_cont->info.given_out_luma.bus_address;

      dec_cont->info.out_pp_luma = dec_cont->asic_buff.pp_luma_buffer =
      dec_cont->info.given_out_luma;
      dec_cont->info.out_pp_chroma = dec_cont->asic_buff.pp_chroma_buffer =
        dec_cont->info.given_out_luma; //dec_cont->info.given_out_chroma;
      /* luma bus address to output */
      dec_cont->info.out_luma = dec_cont->asic_buff.out_luma_buffer;

      /* Chroma output buffer address */
      dec_cont->asic_buff.out_chroma_buffer.virtual_address =
        dec_cont->info.given_out_chroma.virtual_address;
      dec_cont->asic_buff.out_chroma_buffer.bus_address =
        dec_cont->info.given_out_chroma.bus_address;

      /* Chroma bus address to output */
      dec_cont->info.out_chroma = dec_cont->asic_buff.out_luma_buffer; //out_chroma_buffer;

      /* when PP is enabled, because user just allocate one set of output buffer,
              output buffer just use as PP output buffer */
      if (dec_cont->pp_enabled) {
        dec_cont->asic_buff.pp_luma_buffer = dec_cont->info.out_luma;
        dec_cont->asic_buff.pp_chroma_buffer = dec_cont->info.out_chroma;

        dec_cont->info.out_pp_luma = dec_cont->asic_buff.pp_luma_buffer;
        dec_cont->info.out_pp_chroma = dec_cont->asic_buff.pp_chroma_buffer;

        /* Just to set chroma offset. */
        CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0);
      }

      /* flag to release */
      dec_cont->info.user_alloc_mem = 1;
    }

    JPEGDEC_TRACE_INTERNAL(("ALLOCATE: Luma virtual %lx bus %lx\n",
                            dec_cont->asic_buff.out_luma_buffer.
                            virtual_address,
                            dec_cont->asic_buff.out_luma_buffer.bus_address));

    /* allocate chrominance output */
    if(dec_cont->image.size_chroma) {
      if(dec_cont->info.given_out_chroma.virtual_address == NULL &&
         dec_cont->hw_feature.pp_version == G1_NATIVE_PP) {
        if(dec_cont->info.operation_type != JPEGDEC_BASELINE) {
          if(dec_cont->asic_buff.out_chroma_buffer.virtual_address == NULL) {
            tmp =
              DWLMallocRefFrm(dec_cont->dwl,
                              (dec_cont->image.size_chroma / 2),
                              &(dec_cont->asic_buff.out_chroma_buffer));
            if(tmp == -1)
              return (JPEGDEC_MEMFAIL);
          }

          if(dec_cont->asic_buff.out_chroma_buffer2.virtual_address == NULL) {
            tmp =
              DWLMallocRefFrm(dec_cont->dwl,
                              (dec_cont->image.size_chroma / 2),
                              &(dec_cont->asic_buff.out_chroma_buffer2));
            if(tmp == -1)
              return (JPEGDEC_MEMFAIL);
          }
        } else {
          if(dec_cont->asic_buff.out_chroma_buffer.virtual_address == NULL) {
            tmp =
              DWLMallocRefFrm(dec_cont->dwl,
                              (dec_cont->image.size_chroma),
                              &(dec_cont->asic_buff.out_chroma_buffer));
            if(tmp == -1)
              return (JPEGDEC_MEMFAIL);
          }

          dec_cont->asic_buff.out_chroma_buffer2.virtual_address = NULL;
          dec_cont->asic_buff.out_chroma_buffer2.bus_address = 0;
        }
      } else {
        dec_cont->asic_buff.out_chroma_buffer.virtual_address =
          dec_cont->info.given_out_chroma.virtual_address;
        dec_cont->asic_buff.out_chroma_buffer.bus_address =
          dec_cont->info.given_out_chroma.bus_address;
        dec_cont->asic_buff.out_chroma_buffer2.virtual_address =
          dec_cont->info.given_out_chroma2.virtual_address;
        dec_cont->asic_buff.out_chroma_buffer2.bus_address =
          dec_cont->info.given_out_chroma2.bus_address;

      }

      /* chroma bus address to output */
      dec_cont->info.out_chroma = dec_cont->asic_buff.out_chroma_buffer;
      dec_cont->info.out_chroma2 = dec_cont->asic_buff.out_chroma_buffer2;

      JPEGDEC_TRACE_INTERNAL(("ALLOCATE: Chroma virtual %lx bus %lx\n",
                              dec_cont->asic_buff.out_chroma_buffer.
                              virtual_address,
                              dec_cont->asic_buff.out_chroma_buffer.
                              bus_address));
    }
  }

#ifdef JPEGDEC_RESET_OUTPUT
  {
    (void) DWLmemset(dec_cont->asic_buff.out_luma_buffer.virtual_address,
                     128, dec_cont->image.size_luma);
    if (dec_cont->pp_enabled && !dec_cont->info.user_alloc_mem) {
      (void) DWLmemset(dec_cont->asic_buff.pp_luma_buffer.virtual_address,
                       128, ext_buffer_size);
    }
    if(dec_cont->image.size_chroma) {
      if(dec_cont->info.operation_type != JPEGDEC_BASELINE) {
        (void) DWLmemset(dec_cont->asic_buff.out_chroma_buffer.
                         virtual_address, 128,
                         dec_cont->image.size_chroma / 2);
        (void) DWLmemset(dec_cont->asic_buff.out_chroma_buffer2.
                         virtual_address, 128,
                         dec_cont->image.size_chroma / 2);
      } else {
        (void) DWLmemset(dec_cont->asic_buff.out_chroma_buffer.
                         virtual_address, 128,
                         dec_cont->image.size_chroma);
        }
      }
    }
    for (i = 0; i < dec_cont->n_cores; i++) {
      (void) DWLmemset(dec_cont->frame.p_table_base[i].virtual_address, 0,
                       (sizeof(u8) * table_size));
    }
    if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE) {
      (void) DWLmemset(dec_cont->info.p_coeff_base.virtual_address, 0,
                       (sizeof(u8) * JPEGDEC_COEFF_SIZE * num_blocks));
    }
  }
#endif /* #ifdef JPEGDEC_RESET_OUTPUT */

  return JPEGDEC_OK;

}

/*------------------------------------------------------------------------------

        Function name: JpegDecSliceSizeCalculation

        Functional description:
          Calculates slice size

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
void JpegDecSliceSizeCalculation(JpegDecContainer * dec_cont) {

  if(((dec_cont->info.SliceCount +
       1) * (dec_cont->info.slice_mb_set_value * 16)) > dec_cont->info.Y) {
    dec_cont->info.slice_height = ((dec_cont->info.Y / 16) -
                                   (dec_cont->info.SliceCount *
                                    dec_cont->info.slice_height));
  } else {
    /* TODO! other sampling formats also than YUV420 */
    if(dec_cont->info.operation_type == JPEGDEC_PROGRESSIVE &&
        dec_cont->info.component_id != 0)
      dec_cont->info.slice_height = dec_cont->info.slice_mb_set_value / 2;
    else
      dec_cont->info.slice_height = dec_cont->info.slice_mb_set_value;
  }
}

/*------------------------------------------------------------------------------

        Function name: JpegDecWriteTables

        Functional description:
          Writes q/ac/dc tables to the HW format as specified in HW regs

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteTables(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  u32 i, j = 0;
  u32 shifter = 32;
  u32 table_word = 0;
  u32 table_value = 0;
  u8 table_tmp[64] = { 0 };
  u32 *p_table_base = NULL;
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;

  ASSERT(dec_cont);
  ASSERT(dec_cont->frame.p_table_base[core_id].virtual_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].bus_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].size);

  p_table_base = dec_cont->frame.p_table_base[core_id].virtual_address;

  /* QP tables for all components */
  for(j = 0; j < dec_cont->info.amount_of_qtables; j++) {
    if((JPG_FRM.component[j].Tq) == 0) {
      for(i = 0; i < 64; i++) {
        table_tmp[zz_order[i]] = (u8) JPG_QTB.table0[i];
      }

      /* update shifter */
      shifter = 32;

      for(i = 0; i < 64; i++) {
        shifter -= 8;

        if(shifter == 24)
          table_word = (table_tmp[i] << shifter);
        else
          table_word |= (table_tmp[i] << shifter);

        if(shifter == 0) {
          *(p_table_base) = table_word;
          p_table_base++;
          shifter = 32;
        }
      }
    } else {
      for(i = 0; i < 64; i++) {
        table_tmp[zz_order[i]] = (u8) JPG_QTB.table1[i];
      }

      /* update shifter */
      shifter = 32;

      for(i = 0; i < 64; i++) {
        shifter -= 8;

        if(shifter == 24)
          table_word = (table_tmp[i] << shifter);
        else
          table_word |= (table_tmp[i] << shifter);

        if(shifter == 0) {
          *(p_table_base) = table_word;
          p_table_base++;
          shifter = 32;
        }
      }
    }
  }

  /* update shifter */
  shifter = 32;

  if(dec_cont->info.y_cb_cr_mode != JPEGDEC_YUV400) {
    /* this trick is done because hw always wants luma table as ac hw table 1 */
    if(JPG_SCN.Ta[0] == 0) {
      /* Write AC Table 1 (as specified in HW regs)
       * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
      if(JPG_VLC.ac_table0.vals) {
        for(i = 0; i < 162; i++) {
          if(i < JPG_VLC.ac_table0.table_length) {
            table_value = (u8) JPG_VLC.ac_table0.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 162; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
      /* Write AC Table 2 */
      JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC2 (not-luma)\n"));
      if(JPG_VLC.ac_table1.vals) {
        for(i = 0; i < 162; i++) {
          if(i < JPG_VLC.ac_table1.table_length) {
            table_value = (u8) JPG_VLC.ac_table1.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 162; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
    } else {
      /* Write AC Table 1 (as specified in HW regs)
       * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */

      if(JPG_VLC.ac_table1.vals) {
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
        for(i = 0; i < 162; i++) {
          if(i < JPG_VLC.ac_table1.table_length) {
            table_value = (u8) JPG_VLC.ac_table1.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 162; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

      /* Write AC Table 2 */

      if(JPG_VLC.ac_table0.vals) {
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: write_tables: AC2 (not-luma)\n"));
        for(i = 0; i < 162; i++) {
          if(i < JPG_VLC.ac_table0.table_length) {
            table_value = (u8) JPG_VLC.ac_table0.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 162; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
    }

    /* this trick is done because hw always wants luma table as dc hw table 1 */
    if(JPG_SCN.Td[0] == 0) {
      if(JPG_VLC.dc_table0.vals) {
        for(i = 0; i < 12; i++) {
          if(i < JPG_VLC.dc_table0.table_length) {
            table_value = (u8) JPG_VLC.dc_table0.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 12; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

      if(JPG_VLC.dc_table1.vals) {
        for(i = 0; i < 12; i++) {
          if(i < JPG_VLC.dc_table1.table_length) {
            table_value = (u8) JPG_VLC.dc_table1.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 12; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

    } else {
      if(JPG_VLC.dc_table1.vals) {
        for(i = 0; i < 12; i++) {
          if(i < JPG_VLC.dc_table1.table_length) {
            table_value = (u8) JPG_VLC.dc_table1.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 12; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

      if(JPG_VLC.dc_table0.vals) {
        for(i = 0; i < 12; i++) {
          if(i < JPG_VLC.dc_table0.table_length) {
            table_value = (u8) JPG_VLC.dc_table0.vals[i];
          } else {
            table_value = 0;
          }

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        for(i = 0; i < 12; i++) {
          table_word = 0;

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
    }
  } else { /* YUV400 */
    if(!dec_cont->info.non_interleaved_scan_ready) {
      /* this trick is done because hw always wants luma table as ac hw table 1 */
      if(JPG_SCN.Ta[0] == 0) {
        /* Write AC Table 1 (as specified in HW regs)
         * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
        if(JPG_VLC.ac_table0.vals) {
          for(i = 0; i < 162; i++) {
            if(i < JPG_VLC.ac_table0.table_length) {
              table_value = (u8) JPG_VLC.ac_table0.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 162; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        /* Write AC Table 2 */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write zero table (YUV400): \n"));
        for(i = 0; i < 162; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        /* Write AC Table 1 (as specified in HW regs)
         * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */

        if(JPG_VLC.ac_table1.vals) {
          JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
          for(i = 0; i < 162; i++) {
            if(i < JPG_VLC.ac_table1.table_length) {
              table_value = (u8) JPG_VLC.ac_table1.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 162; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        /* Write AC Table 2 */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: write_tables: padding zero (YUV400)\n"));
        for(i = 0; i < 162; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

      /* this trick is done because hw always wants luma table as dc hw table 1 */
      if(JPG_SCN.Td[0] == 0) {
        if(JPG_VLC.dc_table0.vals) {
          for(i = 0; i < 12; i++) {
            if(i < JPG_VLC.dc_table0.table_length) {
              table_value = (u8) JPG_VLC.dc_table0.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 12; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        for(i = 0; i < 12; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        if(JPG_VLC.dc_table1.vals) {
          for(i = 0; i < 12; i++) {
            if(i < JPG_VLC.dc_table1.table_length) {
              table_value = (u8) JPG_VLC.dc_table1.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 12; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        for(i = 0; i < 12; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
    } else {
      /* this trick is done because hw always wants luma table as ac hw table 1 */
      if(JPG_SCN.Ta[dec_cont->info.component_id] == 0) {
        /* Write AC Table 1 (as specified in HW regs)
         * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
        if(JPG_VLC.ac_table0.vals) {
          for(i = 0; i < 162; i++) {
            if(i < JPG_VLC.ac_table0.table_length) {
              table_value = (u8) JPG_VLC.ac_table0.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 162; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        /* Write AC Table 2 */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write zero table (YUV400): \n"));
        for(i = 0; i < 162; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        /* Write AC Table 1 (as specified in HW regs)
         * NOTE: Not the same as actable[1] (as specified in JPEG Spec) */

        if(JPG_VLC.ac_table1.vals) {
          JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write tables: AC1 (luma)\n"));
          for(i = 0; i < 162; i++) {
            if(i < JPG_VLC.ac_table1.table_length) {
              table_value = (u8) JPG_VLC.ac_table1.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 162; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        /* Write AC Table 2 */
        JPEGDEC_TRACE_INTERNAL(("INTERNAL: write_tables: padding zero (YUV400)\n"));
        for(i = 0; i < 162; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }

      /* this trick is done because hw always wants luma table as dc hw table 1 */
      if(JPG_SCN.Td[dec_cont->info.component_id] == 0) {
        if(JPG_VLC.dc_table0.vals) {
          for(i = 0; i < 12; i++) {
            if(i < JPG_VLC.dc_table0.table_length) {
              table_value = (u8) JPG_VLC.dc_table0.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 12; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        for(i = 0; i < 12; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      } else {
        if(JPG_VLC.dc_table1.vals) {
          for(i = 0; i < 12; i++) {
            if(i < JPG_VLC.dc_table1.table_length) {
              table_value = (u8) JPG_VLC.dc_table1.vals[i];
            } else {
              table_value = 0;
            }

            if(shifter == 32)
              table_word = (table_value << (shifter - 8));
            else
              table_word |= (table_value << (shifter - 8));

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        } else {
          for(i = 0; i < 12; i++) {
            table_word = 0;

            shifter -= 8;

            if(shifter == 0) {
              *(p_table_base) = table_word;
              p_table_base++;
              shifter = 32;
            }
          }
        }

        for(i = 0; i < 12; i++) {
          table_value = 0;

          if(shifter == 32)
            table_word = (table_value << (shifter - 8));
          else
            table_word |= (table_value << (shifter - 8));

          shifter -= 8;

          if(shifter == 0) {
            *(p_table_base) = table_word;
            p_table_base++;
            shifter = 32;
          }
        }
      }
    }

  }

  for(i = 0; i < 4; i++) {
    table_value = 0;

    if(shifter == 32)
      table_word = (table_value << (shifter - 8));
    else
      table_word |= (table_value << (shifter - 8));

    shifter -= 8;

    if(shifter == 0) {
      *(p_table_base) = table_word;
      p_table_base++;
      shifter = 32;
    }
  }

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM

}

/*------------------------------------------------------------------------------
        Function name: JpegDecWriteTablesNonInterleaved

        Functional description:
          Writes q/ac/dc tables to the HW format as specified in HW regs

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteTablesNonInterleaved(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  u32 i = 0, j = 0;
  u32 table_word = 0;
  u8 table_tmp[64] = { 0 };
  u8 *p_tmp;
  u32 *p_table_base = NULL;
  u32 first = 0, count = 0;
  u32 len = 0, num_words = 0;
  u32 *vals;
  u32 *p_table;
  u32 qp_table_base = 0;
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;

  ASSERT(dec_cont);
  ASSERT(dec_cont->frame.p_table_base[core_id].virtual_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].bus_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].size);
  ASSERT(dec_cont->info.non_interleaved);

  /* Reset the table memory */
  (void) DWLmemset(dec_cont->frame.p_table_base[core_id].virtual_address, 0,
                   (sizeof(u8) * JPEGDEC_BASELINE_TABLE_SIZE));

  p_table_base = dec_cont->frame.p_table_base[core_id].virtual_address;

  first = dec_cont->info.component_id;
  count = 1;

  /* QP tables for all components */
  for(j = first; j < first + count; j++) {
    if((JPG_FRM.component[j].Tq) == 0)
      p_table = JPG_QTB.table0;
    else
      p_table = JPG_QTB.table1;

    for(i = 0; i < 64; i++) {
      table_tmp[zz_order[i]] = (u8) p_table[i];
    }

    p_tmp = table_tmp;
    for(i = 0; i < 16; i++) {
      table_word = (p_tmp[0] << 24) | (p_tmp[1] << 16) |
                   (p_tmp[2] << 8) | (p_tmp[3] << 0);;

      *p_table_base++ = table_word;
      p_tmp += 4;
    }
  }

  /* AC table */
  for(i = first; i < first + count; i++) {
    num_words = 162;
    switch (JPG_SCN.Ta[i]) {
    case 0:
      vals = JPG_VLC.ac_table0.vals;
      len = JPG_VLC.ac_table0.table_length;
      break;
    case 1:
      vals = JPG_VLC.ac_table1.vals;
      len = JPG_VLC.ac_table1.table_length;
      break;
    case 2:
      vals = JPG_VLC.ac_table2.vals;
      len = JPG_VLC.ac_table2.table_length;
      break;
    default:
      vals = JPG_VLC.ac_table3.vals;
      len = JPG_VLC.ac_table3.table_length;
      break;
    }

    /* set pointer */
    if(count == 3)
      qp_table_base = 0;
    else
      qp_table_base = JPEGDEC_QP_BASE;

    p_table_base =
      &dec_cont->frame.p_table_base[core_id].virtual_address[JPEGDEC_AC1_BASE -
          qp_table_base];

    for(j = 0; j < num_words; j++) {
      table_word <<= 8;
      if(j < len)
        table_word |= vals[j];

      if((j & 0x3) == 0x3)
        *p_table_base++ = table_word;
    }

    /* fill to border */
    num_words = 164;
    len = 164;
    for(j = 162; j < num_words; j++) {
      table_word <<= 8;
      if(j < len)
        table_word |= 0;

      if((j & 0x3) == 0x3)
        *p_table_base++ = table_word;
    }
  }

  /* DC table */
  for(i = first; i < first + count; i++) {
    num_words = 12;
    switch (JPG_SCN.Td[i]) {
    case 0:
      vals = JPG_VLC.dc_table0.vals;
      len = JPG_VLC.dc_table0.table_length;
      break;
    case 1:
      vals = JPG_VLC.dc_table1.vals;
      len = JPG_VLC.dc_table1.table_length;
      break;
    case 2:
      vals = JPG_VLC.dc_table2.vals;
      len = JPG_VLC.dc_table2.table_length;
      break;
    default:
      vals = JPG_VLC.dc_table3.vals;
      len = JPG_VLC.dc_table3.table_length;
      break;
    }

    /* set pointer */
    if(count == 3)
      qp_table_base = 0;
    else
      qp_table_base = JPEGDEC_QP_BASE;

    p_table_base =
      &dec_cont->frame.p_table_base[core_id].virtual_address[JPEGDEC_DC1_BASE -
          qp_table_base];

    for(j = 0; j < num_words; j++) {
      table_word <<= 8;
      if(j < len)
        table_word |= vals[j];

      if((j & 0x3) == 0x3)
        *p_table_base++ = table_word;
    }
  }

  *p_table_base = 0;

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM


}

/*------------------------------------------------------------------------------

        Function name: JpegDecWriteTablesProgressive

        Functional description:
          Writes q/ac/dc tables to the HW format as specified in HW regs

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteTablesProgressive(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  u32 i = 0, j = 0;
  u32 table_word = 0;
  u8 table_tmp[64] = { 0 };
  u8 *p_tmp;
  u32 *p_table_base = NULL;
  u32 first = 0, count = 0;
  u32 len = 0, num_words = 0;
  u32 *vals;
  u32 *p_table;
  u32 dc_table = 0;
  u32 qp_table_base = 0;
  u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
  //u32 shifter = 0;

  ASSERT(dec_cont);
  ASSERT(dec_cont->frame.p_table_base[core_id].virtual_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].bus_address);
  ASSERT(dec_cont->frame.p_table_base[core_id].size);

  /* Reset the table memory */
  (void) DWLmemset(dec_cont->frame.p_table_base[core_id].virtual_address, 0,
                   (sizeof(u8) * JPEGDEC_PROGRESSIVE_TABLE_SIZE));

  p_table_base = dec_cont->frame.p_table_base[core_id].virtual_address;

  /* Prepare all QP tables for PJPEG */
  if(dec_cont->info.y_cb_cr_mode_orig == JPEGDEC_YUV400) {
    first = dec_cont->info.component_id;
    count = 1;
  } else {
    first = 0;
    count = 3;
  }

  /* QP tables for all components */
  for(j = first; j < first + count; j++) {
    if((JPG_FRM.component[j].Tq) == 0)
      p_table = JPG_QTB.table0;
    else
      p_table = JPG_QTB.table1;

    for(i = 0; i < 64; i++) {
      table_tmp[zz_order[i]] = (u8) p_table[i];
    }

#if 0
    /* update shifter */
    shifter = 0;

    for(i = 0; i < 64; i++) {
      shifter += 8;
      if(shifter == 8)
        table_word = (table_tmp[i] << (shifter-8));
      else
        table_word |= (table_tmp[i] << (shifter-8));

      if(shifter == 32) {
        *p_table_base++ = table_word;
        shifter = 0;
      }
    }
#else

    p_tmp = table_tmp;
    for(i = 0; i < 16; i++) {
      table_word = (p_tmp[0] << 24) | (p_tmp[1] << 16) |
                   (p_tmp[2] << 8) | (p_tmp[3] << 0);
      *p_table_base++ = table_word;
      p_tmp += 4;
    }
#endif
  }

  if (dec_cont->info.non_interleaved)
  {
      first = dec_cont->info.component_id;
      count = 1;
  }
  else
  {
      first = 0;
      count = 3;
  }

  /* if later stage DC ==> no need for table */
  if(dec_cont->scan.Ah != 0 && dec_cont->scan.Ss == 0)
    return;

  /* set pointer */
  if(count == 3 || dec_cont->info.y_cb_cr_mode_orig != JPEGDEC_YUV400)
    qp_table_base = 0;
  else
    qp_table_base = JPEGDEC_QP_BASE;

  for(i = first; i < first + count; i++) {
    if(dec_cont->scan.Ss == 0) { /* DC */
      dc_table = 1;
      num_words = 12;
      switch (JPG_SCN.Td[i]) {
      case 0:
        vals = JPG_VLC.dc_table0.vals;
        len = JPG_VLC.dc_table0.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_DC1_BASE - qp_table_base];
        break;
      case 1:
        vals = JPG_VLC.dc_table1.vals;
        len = JPG_VLC.dc_table1.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_DC2_BASE - qp_table_base];
        break;
      case 2:
        vals = JPG_VLC.dc_table2.vals;
        len = JPG_VLC.dc_table2.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_DC3_BASE - qp_table_base];
        break;
      default:
        vals = JPG_VLC.dc_table3.vals;
        len = JPG_VLC.dc_table3.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_DC4_BASE - qp_table_base];
        break;
      }
    } else {
      num_words = 162;
      switch (JPG_SCN.Ta[i]) {
      case 0:
        vals = JPG_VLC.ac_table0.vals;
        len = JPG_VLC.ac_table0.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_AC1_BASE - qp_table_base];
        break;
      case 1:
        vals = JPG_VLC.ac_table1.vals;
        len = JPG_VLC.ac_table1.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_AC2_BASE - qp_table_base];
        break;
      case 2:
        vals = JPG_VLC.ac_table2.vals;
        len = JPG_VLC.ac_table2.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_AC3_BASE - qp_table_base];
        break;
      default:
        vals = JPG_VLC.ac_table3.vals;
        len = JPG_VLC.ac_table3.table_length;
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[PJPEGDEC_AC4_BASE - qp_table_base];
        break;
      }
    }

#if 0
    /* set pointer */
    if(count == 3 || dec_cont->info.y_cb_cr_mode_orig != JPEGDEC_YUV400)
      qp_table_base = 0;
    else
      qp_table_base = JPEGDEC_QP_BASE;

    if(dc_table) {
      /* interleaved || non-interleaved */
      if(count == 3) {
        if(i == 0)
          p_table_base =
            &dec_cont->frame.p_table_base[core_id].
            virtual_address[JPEGDEC_DC1_BASE - qp_table_base];
        else if(i == 1)
          p_table_base =
            &dec_cont->frame.p_table_base[core_id].
            virtual_address[JPEGDEC_DC2_BASE - qp_table_base];
        else
          p_table_base =
            &dec_cont->frame.p_table_base[core_id].
            virtual_address[JPEGDEC_DC3_BASE - qp_table_base];
      } else {
        p_table_base =
          &dec_cont->frame.p_table_base[core_id].
          virtual_address[JPEGDEC_DC1_BASE - qp_table_base];
      }
    } else {
      p_table_base =
        &dec_cont->frame.p_table_base[core_id].virtual_address[JPEGDEC_AC1_BASE -
            qp_table_base];
    }
#endif

    //shifter = 0;
    for(j = 0; j < num_words; j++) {
#if 1
      table_word <<= 8;
      if(j < len)
        table_word |= vals[j];

      if((j & 0x3) == 0x3)
        *p_table_base++ = table_word;
#else
      shifter += 8;
      if(shifter == 8)
        table_word = (vals[j] << (shifter - 8));
      else
        table_word |= (vals[j] << (shifter - 8));

      if(shifter == 32) {
        *p_table_base++ = table_word;
        shifter = 0;
      }
#endif
    }
    //shifter = 0;
    /* fill to border */
    if(i == 0 && dc_table == 0) {
      num_words = 164;
      len = 164;
      for(j = 162; j < num_words; j++) {
#if 1
        table_word <<= 8;
        if(j < len)
          table_word |= 0;

        if((j & 0x3) == 0x3)
          *p_table_base++ = table_word;
#else
        shifter += 8;
        if(shifter == 8)
          table_word = (vals[j] << (shifter - 8));
        else
          table_word |= (vals[j] << (shifter - 8));

        if(shifter == 32) {
          *p_table_base++ = table_word;
          shifter = 0;
        }
#endif
      }
    }

    /* reset */
    dc_table = 0;
  }

  *p_table_base = 0;

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM


}

/*------------------------------------------------------------------------------

        Function name: JpegDecChromaTableSelectors

        Functional description:
          select what tables chromas use

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecChromaTableSelectors(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_FRM     dec_cont->frame

  /* this trick is done because hw always wants luma table as ac hw table 1 */
  if(JPG_SCN.Ta[0] == 0) {
    SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_AC_VLCTABLE, JPG_SCN.Ta[2]);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_AC_VLCTABLE, JPG_SCN.Ta[1]);
  } else {
    if(JPG_SCN.Ta[0] == JPG_SCN.Ta[1])
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_AC_VLCTABLE, 0);
    else
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_AC_VLCTABLE, 1);

    if(JPG_SCN.Ta[0] == JPG_SCN.Ta[2])
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_AC_VLCTABLE, 0);
    else
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_AC_VLCTABLE, 1);
  }

  /* Third DC table selectors */
  if(dec_cont->info.operation_type != JPEGDEC_PROGRESSIVE) {
    if(JPG_SCN.Td[0] == 0) {
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_DC_VLCTABLE,
                     JPG_SCN.Td[2]);
      SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_DC_VLCTABLE,
                     JPG_SCN.Td[1]);
    } else {
      if(JPG_SCN.Td[0] == JPG_SCN.Td[1])
        SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_DC_VLCTABLE, 0);
      else
        SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_DC_VLCTABLE, 1);

      if(JPG_SCN.Td[0] == JPG_SCN.Td[2])
        SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_DC_VLCTABLE, 0);
      else
        SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_DC_VLCTABLE, 1);
    }

    SetDecRegister(dec_cont->jpeg_regs, HWIF_CR_DC_VLCTABLE3, 0);
    SetDecRegister(dec_cont->jpeg_regs, HWIF_CB_DC_VLCTABLE3, 0);
  } else {
    /* if non-interleaved ==> decoding mode YUV400, uses table zero (0) */
    if(dec_cont->info.non_interleaved) {
      SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL0, JPG_SCN.Td[dec_cont->info.component_id]);
      SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AC_SEL, JPG_SCN.Ta[dec_cont->info.component_id]);
    } else {
      /* if later stage DC ==> no need for table */
      if(dec_cont->scan.Ah != 0 && dec_cont->scan.Ss == 0) {
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL0, 0);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL1, 0);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL2, 0);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_AC_SEL, 0);

      } else {
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL0, JPG_SCN.Td[0]);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL1, JPG_SCN.Td[1]);
        SetDecRegister(dec_cont->jpeg_regs, HWIF_PJPEG_DC_SEL2, JPG_SCN.Td[2]);
      }
    }
  }

  return;

#undef JPG_SCN
#undef JPG_FRM

}

/*------------------------------------------------------------------------------

        Function name: JpegDecWriteLenBits

        Functional description:
          tell hw how many vlc words of different lengths we have

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteLenBits(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  VlcTable *p_table1 = NULL;
  VlcTable *p_table2 = NULL;

  /* first select the table we'll use */

  /* this trick is done because hw always wants luma table as ac hw table 1 */
  if(JPG_SCN.Ta[0] == 0) {

    p_table1 = &(JPG_VLC.ac_table0);
    p_table2 = &(JPG_VLC.ac_table1);

  } else {

    p_table1 = &(JPG_VLC.ac_table1);
    p_table2 = &(JPG_VLC.ac_table0);
  }

  ASSERT(p_table1);
  ASSERT(p_table2);

  /* write AC table 1 (luma) */

  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE1_CNT, p_table1->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE2_CNT, p_table1->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE3_CNT, p_table1->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE4_CNT, p_table1->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE5_CNT, p_table1->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE6_CNT, p_table1->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE7_CNT, p_table1->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE8_CNT, p_table1->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE9_CNT, p_table1->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE10_CNT, p_table1->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE11_CNT, p_table1->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE12_CNT, p_table1->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE13_CNT, p_table1->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE14_CNT, p_table1->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE15_CNT, p_table1->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE16_CNT, p_table1->bits[15]);

  /* table AC2 (the not-luma table) */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE1_CNT, p_table2->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE2_CNT, p_table2->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE3_CNT, p_table2->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE4_CNT, p_table2->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE5_CNT, p_table2->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE6_CNT, p_table2->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE7_CNT, p_table2->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE8_CNT, p_table2->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE9_CNT, p_table2->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE10_CNT, p_table2->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE11_CNT, p_table2->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE12_CNT, p_table2->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE13_CNT, p_table2->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE14_CNT, p_table2->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE15_CNT, p_table2->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC2_CODE16_CNT, p_table2->bits[15]);

  if(JPG_SCN.Td[0] == 0) {

    p_table1 = &(JPG_VLC.dc_table0);
    p_table2 = &(JPG_VLC.dc_table1);

  } else {

    p_table1 = &(JPG_VLC.dc_table1);
    p_table2 = &(JPG_VLC.dc_table0);
  }

  ASSERT(p_table1);
  ASSERT(p_table2);

  /* write DC table 1 (luma) */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE1_CNT, p_table1->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE2_CNT, p_table1->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE3_CNT, p_table1->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE4_CNT, p_table1->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE5_CNT, p_table1->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE6_CNT, p_table1->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE7_CNT, p_table1->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE8_CNT, p_table1->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE9_CNT, p_table1->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE10_CNT, p_table1->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE11_CNT, p_table1->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE12_CNT, p_table1->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE13_CNT, p_table1->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE14_CNT, p_table1->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE15_CNT, p_table1->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE16_CNT, p_table1->bits[15]);

  /* table DC2 (the not-luma table) */

  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE1_CNT, p_table2->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE2_CNT, p_table2->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE3_CNT, p_table2->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE4_CNT, p_table2->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE5_CNT, p_table2->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE6_CNT, p_table2->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE7_CNT, p_table2->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE8_CNT, p_table2->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE9_CNT, p_table2->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE10_CNT, p_table2->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE11_CNT, p_table2->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE12_CNT, p_table2->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE13_CNT, p_table2->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE14_CNT, p_table2->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE15_CNT, p_table2->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC2_CODE16_CNT, p_table2->bits[15]);

  return;

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM

}

/*------------------------------------------------------------------------------

        Function name: JpegDecWriteLenBitsNonInterleaved

        Functional description:
          tell hw how many vlc words of different lengths we have

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteLenBitsNonInterleaved(JpegDecContainer * dec_cont) {

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  VlcTable *p_table1 = NULL;
  VlcTable *p_table2 = NULL;
#ifdef NDEBUG
  UNUSED(p_table2);
#endif

  /* first select the table we'll use */

  /* this trick is done because hw always wants luma table as ac hw table 1 */
  if(JPG_SCN.Ta[dec_cont->info.component_id] == 0) {

    p_table1 = &(JPG_VLC.ac_table0);
    p_table2 = &(JPG_VLC.ac_table1);

  } else {

    p_table1 = &(JPG_VLC.ac_table1);
    p_table2 = &(JPG_VLC.ac_table0);
  }

  ASSERT(p_table1);
  ASSERT(p_table2);

  /* write AC table 1 (luma) */

  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE1_CNT, p_table1->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE2_CNT, p_table1->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE3_CNT, p_table1->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE4_CNT, p_table1->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE5_CNT, p_table1->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE6_CNT, p_table1->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE7_CNT, p_table1->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE8_CNT, p_table1->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE9_CNT, p_table1->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE10_CNT, p_table1->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE11_CNT, p_table1->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE12_CNT, p_table1->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE13_CNT, p_table1->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE14_CNT, p_table1->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE15_CNT, p_table1->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE16_CNT, p_table1->bits[15]);

  if(JPG_SCN.Td[dec_cont->info.component_id] == 0) {

    p_table1 = &(JPG_VLC.dc_table0);
    p_table2 = &(JPG_VLC.dc_table1);

  } else {

    p_table1 = &(JPG_VLC.dc_table1);
    p_table2 = &(JPG_VLC.dc_table0);
  }

  ASSERT(p_table1);
  ASSERT(p_table2);

  /* write DC table 1 (luma) */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE1_CNT, p_table1->bits[0]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE2_CNT, p_table1->bits[1]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE3_CNT, p_table1->bits[2]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE4_CNT, p_table1->bits[3]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE5_CNT, p_table1->bits[4]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE6_CNT, p_table1->bits[5]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE7_CNT, p_table1->bits[6]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE8_CNT, p_table1->bits[7]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE9_CNT, p_table1->bits[8]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE10_CNT, p_table1->bits[9]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE11_CNT, p_table1->bits[10]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE12_CNT, p_table1->bits[11]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE13_CNT, p_table1->bits[12]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE14_CNT, p_table1->bits[13]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE15_CNT, p_table1->bits[14]);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_DC1_CODE16_CNT, p_table1->bits[15]);

  return;

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM

}
static const u32 dc_code_cnt[4][16] = {
{
HWIF_DC1_CODE1_CNT, HWIF_DC1_CODE2_CNT, HWIF_DC1_CODE3_CNT, HWIF_DC1_CODE4_CNT,
HWIF_DC1_CODE5_CNT, HWIF_DC1_CODE6_CNT, HWIF_DC1_CODE7_CNT, HWIF_DC1_CODE8_CNT,
HWIF_DC1_CODE9_CNT, HWIF_DC1_CODE10_CNT, HWIF_DC1_CODE11_CNT, HWIF_DC1_CODE12_CNT,
HWIF_DC1_CODE13_CNT, HWIF_DC1_CODE14_CNT, HWIF_DC1_CODE15_CNT, HWIF_DC1_CODE16_CNT},
{
HWIF_DC2_CODE1_CNT, HWIF_DC2_CODE2_CNT, HWIF_DC2_CODE3_CNT, HWIF_DC2_CODE4_CNT,
HWIF_DC2_CODE5_CNT, HWIF_DC2_CODE6_CNT, HWIF_DC2_CODE7_CNT, HWIF_DC2_CODE8_CNT,
HWIF_DC2_CODE9_CNT, HWIF_DC2_CODE10_CNT, HWIF_DC2_CODE11_CNT, HWIF_DC2_CODE12_CNT,
HWIF_DC2_CODE13_CNT, HWIF_DC2_CODE14_CNT, HWIF_DC2_CODE15_CNT, HWIF_DC2_CODE16_CNT
},
{
HWIF_DC3_CODE1_CNT, HWIF_DC3_CODE2_CNT, HWIF_DC3_CODE3_CNT, HWIF_DC3_CODE4_CNT,
HWIF_DC3_CODE5_CNT, HWIF_DC3_CODE6_CNT, HWIF_DC3_CODE7_CNT, HWIF_DC3_CODE8_CNT,
HWIF_DC3_CODE9_CNT, HWIF_DC3_CODE10_CNT, HWIF_DC3_CODE11_CNT, HWIF_DC3_CODE12_CNT,
HWIF_DC3_CODE13_CNT, HWIF_DC3_CODE14_CNT, HWIF_DC3_CODE15_CNT, HWIF_DC3_CODE16_CNT
},
{
HWIF_DC4_CODE1_CNT, HWIF_DC4_CODE2_CNT, HWIF_DC4_CODE3_CNT, HWIF_DC4_CODE4_CNT,
HWIF_DC4_CODE5_CNT, HWIF_DC4_CODE6_CNT, HWIF_DC4_CODE7_CNT, HWIF_DC4_CODE8_CNT,
HWIF_DC4_CODE9_CNT, HWIF_DC4_CODE10_CNT, HWIF_DC4_CODE11_CNT, HWIF_DC4_CODE12_CNT,
HWIF_DC4_CODE13_CNT, HWIF_DC4_CODE14_CNT, HWIF_DC4_CODE15_CNT, HWIF_DC4_CODE16_CNT
}
};

static  const u32 ac_code_cnt[4][16] = {
{
HWIF_AC1_CODE1_CNT, HWIF_AC1_CODE2_CNT, HWIF_AC1_CODE3_CNT, HWIF_AC1_CODE4_CNT,
HWIF_AC1_CODE5_CNT, HWIF_AC1_CODE6_CNT, HWIF_AC1_CODE7_CNT, HWIF_AC1_CODE8_CNT,
HWIF_AC1_CODE9_CNT, HWIF_AC1_CODE10_CNT, HWIF_AC1_CODE11_CNT, HWIF_AC1_CODE12_CNT,
HWIF_AC1_CODE13_CNT, HWIF_AC1_CODE14_CNT, HWIF_AC1_CODE15_CNT, HWIF_AC1_CODE16_CNT},
{
HWIF_AC2_CODE1_CNT, HWIF_AC2_CODE2_CNT, HWIF_AC2_CODE3_CNT, HWIF_AC2_CODE4_CNT,
HWIF_AC2_CODE5_CNT, HWIF_AC2_CODE6_CNT, HWIF_AC2_CODE7_CNT, HWIF_AC2_CODE8_CNT,
HWIF_AC2_CODE9_CNT, HWIF_AC2_CODE10_CNT, HWIF_AC2_CODE11_CNT, HWIF_AC2_CODE12_CNT,
HWIF_AC2_CODE13_CNT, HWIF_AC2_CODE14_CNT, HWIF_AC2_CODE15_CNT, HWIF_AC2_CODE16_CNT
},
{
HWIF_AC3_CODE1_CNT, HWIF_AC3_CODE2_CNT, HWIF_AC3_CODE3_CNT, HWIF_AC3_CODE4_CNT,
HWIF_AC3_CODE5_CNT, HWIF_AC3_CODE6_CNT, HWIF_AC3_CODE7_CNT, HWIF_AC3_CODE8_CNT,
HWIF_AC3_CODE9_CNT, HWIF_AC3_CODE10_CNT, HWIF_AC3_CODE11_CNT, HWIF_AC3_CODE12_CNT,
HWIF_AC3_CODE13_CNT, HWIF_AC3_CODE14_CNT, HWIF_AC3_CODE15_CNT, HWIF_AC3_CODE16_CNT
},
{
HWIF_AC4_CODE1_CNT, HWIF_AC4_CODE2_CNT, HWIF_AC4_CODE3_CNT, HWIF_AC4_CODE4_CNT,
HWIF_AC4_CODE5_CNT, HWIF_AC4_CODE6_CNT, HWIF_AC4_CODE7_CNT, HWIF_AC4_CODE8_CNT,
HWIF_AC4_CODE9_CNT, HWIF_AC4_CODE10_CNT, HWIF_AC4_CODE11_CNT, HWIF_AC4_CODE12_CNT,
HWIF_AC4_CODE13_CNT, HWIF_AC4_CODE14_CNT, HWIF_AC4_CODE15_CNT, HWIF_AC4_CODE16_CNT
}
};



/*------------------------------------------------------------------------------

        Function name: JpegDecWriteLenBitsProgressive

        Functional description:
          tell hw how many vlc words of different lengths we have

        Inputs:
          JpegDecContainer *dec_cont

        Outputs:
          void

------------------------------------------------------------------------------*/
static void JpegDecWriteLenBitsProgressive(JpegDecContainer * dec_cont) {

  u32 i;

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  VlcTable *p_table1 = NULL;
  VlcTable *p_table2 = NULL;
  VlcTable *p_table3 = NULL;

  /* reset swregs that contains vlc length information: swregs [16-28] */
  for(i = JPEGDEC_VLC_LEN_START_REG; i < JPEGDEC_VLC_LEN_END_REG; i++)
    dec_cont->jpeg_regs[i] = 0;

  /* check if interleaved scan ==> only one table needed */
  if(dec_cont->info.non_interleaved) {
    /* check if AC or DC coefficient scan */
    if(dec_cont->scan.Ss == 0) { /* DC */
      /* check component ID */
      if(dec_cont->info.component_id == 0) {
        if(JPG_SCN.Td[0] == 0)
          p_table1 = &(JPG_VLC.dc_table0);
        else if(JPG_SCN.Td[0] == 1)
          p_table1 = &(JPG_VLC.dc_table1);
        else if(JPG_SCN.Td[0] == 2)
          p_table1 = &(JPG_VLC.dc_table2);
        else
          p_table1 = &(JPG_VLC.dc_table3);
      } else if(dec_cont->info.component_id == 1) {
        if(JPG_SCN.Td[1] == 0)
          p_table1 = &(JPG_VLC.dc_table0);
        else if(JPG_SCN.Td[1] == 1)
          p_table1 = &(JPG_VLC.dc_table1);
        else if(JPG_SCN.Td[1] == 2)
          p_table1 = &(JPG_VLC.dc_table2);
        else
          p_table1 = &(JPG_VLC.dc_table3);
      } else {
        if(JPG_SCN.Td[2] == 0)
          p_table1 = &(JPG_VLC.dc_table0);
        else if(JPG_SCN.Td[2] == 1)
          p_table1 = &(JPG_VLC.dc_table1);
        else if(JPG_SCN.Td[2] == 2)
          p_table1 = &(JPG_VLC.dc_table2);
        else
          p_table1 = &(JPG_VLC.dc_table3);
      }

      ASSERT(p_table1);

      /* if later stage DC ==> no need for table */
      if(dec_cont->scan.Ah == 0) {
        /* write DC table 1 */
        for (i = 0; i < 16; i++) {
          SetDecRegister(dec_cont->jpeg_regs,
		dc_code_cnt[JPG_SCN.Td[dec_cont->info.component_id]][i],
		p_table1->bits[i]);
	}
      } else {
        /* write zero table */
        for (i = 0; i < 16; i++) {
          SetDecRegister(dec_cont->jpeg_regs,
		dc_code_cnt[JPG_SCN.Td[dec_cont->info.component_id]][i],
		0);
	}
      }

    } else { /* AC */
      /* check component ID */
      if(dec_cont->info.component_id == 0) {
        if(JPG_SCN.Ta[0] == 0)
          p_table1 = &(JPG_VLC.ac_table0);
        else if(JPG_SCN.Ta[0] == 1)
          p_table1 = &(JPG_VLC.ac_table1);
        else if(JPG_SCN.Ta[0] == 2)
          p_table1 = &(JPG_VLC.ac_table2);
        else
          p_table1 = &(JPG_VLC.ac_table3);
      } else if(dec_cont->info.component_id == 1) {
        if(JPG_SCN.Ta[1] == 0)
          p_table1 = &(JPG_VLC.ac_table0);
        else if(JPG_SCN.Ta[1] == 1)
          p_table1 = &(JPG_VLC.ac_table1);
        else if(JPG_SCN.Ta[1] == 2)
          p_table1 = &(JPG_VLC.ac_table2);
        else
          p_table1 = &(JPG_VLC.ac_table3);
      } else {
        if(JPG_SCN.Ta[2] == 0)
          p_table1 = &(JPG_VLC.ac_table0);
        else if(JPG_SCN.Ta[2] == 1)
          p_table1 = &(JPG_VLC.ac_table1);
        else if(JPG_SCN.Ta[2] == 2)
          p_table1 = &(JPG_VLC.ac_table2);
        else
          p_table1 = &(JPG_VLC.ac_table3);
      }

      ASSERT(p_table1);

      /* write AC table 1 */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
		ac_code_cnt[JPG_SCN.Ta[dec_cont->info.component_id]][i],
		p_table1->bits[i]);
      }
    }
  } else { /* interleaved */
    /* first select the table we'll use */
    /* this trick is done because hw always wants luma table as ac hw table 1 */

    if(JPG_SCN.Td[0] == 0)
      p_table1 = &(JPG_VLC.dc_table0);
    else if(JPG_SCN.Td[0] == 1)
      p_table1 = &(JPG_VLC.dc_table1);
    else if(JPG_SCN.Td[0] == 2)
      p_table1 = &(JPG_VLC.dc_table2);
    else
      p_table1 = &(JPG_VLC.dc_table3);

    if(JPG_SCN.Td[1] == 0)
      p_table2 = &(JPG_VLC.dc_table0);
    else if(JPG_SCN.Td[1] == 1)
      p_table2 = &(JPG_VLC.dc_table1);
    else if(JPG_SCN.Td[1] == 2)
      p_table2 = &(JPG_VLC.dc_table2);
    else
      p_table2 = &(JPG_VLC.dc_table3);

    if(JPG_SCN.Td[2] == 0)
      p_table3 = &(JPG_VLC.dc_table0);
    else if(JPG_SCN.Td[2] == 1)
      p_table3 = &(JPG_VLC.dc_table1);
    else if(JPG_SCN.Td[2] == 2)
      p_table3 = &(JPG_VLC.dc_table2);
    else
      p_table3 = &(JPG_VLC.dc_table3);

    ASSERT(p_table1);
    ASSERT(p_table2);
    ASSERT(p_table3);

    /* if later stage DC ==> no need for table */
    if(dec_cont->scan.Ah == 0) {
      /* write DC table 1 (luma) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[0]][i], p_table1->bits[i]);
      }

      /* table DC2 (Cb) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[1]][i], p_table2->bits[i]);
      }

      /* table DC3 (Cr) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[2]][i], p_table3->bits[i]);
      }

    } else {
      /* write DC table 1 (luma) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[0]][i], 0);
      }

      /* table DC2 (Cb) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[1]][i], 0);
      }

      /* table DC3 (Cr) */
      for (i = 0; i < 16; i++) {
        SetDecRegister(dec_cont->jpeg_regs,
                dc_code_cnt[JPG_SCN.Td[2]][i], 0);
      }

    }
  }

  return;

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM

}

/*------------------------------------------------------------------------------

        Function name: JpegDecNextScanHdrs

        Functional description:
          Decodes next headers in case of non-interleaved stream

        Inputs:
          JpegDecContainer *pDecData      Pointer to JpegDecContainer structure

        Outputs:
          OK/NOK

------------------------------------------------------------------------------*/
JpegDecRet JpegDecNextScanHdrs(JpegDecContainer * dec_cont) {

  u32 i;
  u32 current_byte = 0;
  u32 current_bytes = 0;
  JpegDecRet ret_code;

#define JPG_SCN     dec_cont->scan
#define JPG_VLC     dec_cont->vlc
#define JPG_QTB     dec_cont->quant
#define JPG_FRM     dec_cont->frame

  ret_code = JPEGDEC_OK;

  /* reset for new headers */
  dec_cont->image.header_ready = 0;

  /* find markers and go ! */
  do {
    /* Look for marker prefix byte from stream */
    if(JpegDecGetByte(&(dec_cont->stream)) == 0xFF) {
      current_byte = JpegDecGetByte(&(dec_cont->stream));

      /* switch to certain header decoding */
      switch (current_byte) {
      case 0x00:
      case SOF0:
      case SOF2:
        break;
      /* Start of Scan */
      case SOS:
        /* reset image ready */
        dec_cont->image.image_ready = 0;
        ret_code = JpegDecDecodeScan(dec_cont);
        dec_cont->image.header_ready = 1;
        if(ret_code != JPEGDEC_OK) {
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Stream error"));
            return (ret_code);
          } else {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# JpegDecDecodeScan err\n"));
            return (ret_code);
          }
        }

        if(dec_cont->stream.bit_pos_in_byte) {
          /* delete stuffing bits */
          current_byte = (8 - dec_cont->stream.bit_pos_in_byte);
          if(JpegDecFlushBits
              (&(dec_cont->stream),
               8 - dec_cont->stream.bit_pos_in_byte) == STRM_ERROR) {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Stream error"));
            return (JPEGDEC_STRM_ERROR);
          }
        }
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# Stuffing bits deleted\n"));
        break;
      /* Start of Huffman tables */
      case DHT:
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# JpegDecDecodeHuffmanTables dec"));
        ret_code = JpegDecDecodeHuffmanTables(dec_cont);
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# JpegDecDecodeHuffmanTables stops"));
        if(ret_code != JPEGDEC_OK) {
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Stream error"));
            return (ret_code);
          } else {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: JpegDecDecodeHuffmanTables err"));
            return (ret_code);
          }
        }
        break;
      /* start of Quantisation Tables */
      case DQT:
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# JpegDecDecodeQuantTables dec"));
        ret_code = JpegDecDecodeQuantTables(dec_cont);
        if(ret_code != JPEGDEC_OK) {
          if(ret_code == JPEGDEC_STRM_ERROR) {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Stream error"));
            return (ret_code);
          } else {
            JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: JpegDecDecodeQuantTables err"));
            return (ret_code);
          }
        }
        break;
      /* Start of Image */
      case SOI:
        /* no actions needed, continue */
        break;
      /* End of Image */
      case EOI:
        if(dec_cont->image.image_ready) {
          JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# EOI: OK\n"));
          return (JPEGDEC_FRAME_READY);
        } else {
          JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: EOI: NOK\n"));
          return (JPEGDEC_ERROR);
        }
      /* Define Restart Interval */
      case DRI:
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# DRI"));
        current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
        if(current_bytes == STRM_ERROR) {
          JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Read bits "));
          return (JPEGDEC_STRM_ERROR);
        }
        dec_cont->frame.Ri = JpegDecGet2Bytes(&(dec_cont->stream));
        break;
      /* Restart with modulo 8 count m */
      case RST0:
      case RST1:
      case RST2:
      case RST3:
      case RST4:
      case RST5:
      case RST6:
      case RST7:
        /* initialisation of DC predictors to zero value !!! */
        for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++) {
          dec_cont->scan.pred[i] = 0;
        }
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# DC predictors init"));
        break;
      /* unsupported features */
      case DNL:
      case SOF1:
      case SOF3:
      case SOF5:
      case SOF6:
      case SOF7:
      case SOF9:
      case SOF10:
      case SOF11:
      case SOF13:
      case SOF14:
      case SOF15:
      case DAC:
      case DHP:
      case TEM:
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Unsupported Features"));
        return (JPEGDEC_UNSUPPORTED);
      /* application data & comments */
      case APP0:
      case APP1:
      case APP2:
      case APP3:
      case APP4:
      case APP5:
      case APP6:
      case APP7:
      case APP8:
      case APP9:
      case APP10:
      case APP11:
      case APP12:
      case APP13:
      case APP14:
      case APP15:
      case COM:
        JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# COM"));
        current_bytes = JpegDecGet2Bytes(&(dec_cont->stream));
        if(current_bytes == STRM_ERROR) {
          JPEGDEC_TRACE_INTERNAL(("JpegDecNextScanHdrs# ERROR: Read bits "));
          return (JPEGDEC_STRM_ERROR);
        }
        /* jump over not supported header */
        if(current_bytes != 0) {
          dec_cont->stream.read_bits += ((current_bytes * 8) - 16);
          dec_cont->stream.p_curr_pos +=
            (((current_bytes * 8) - 16) / 8);
        }
        break;
      default:
        break;
      }
    } else {
      if(current_byte == 0xFFFFFFFF) {
        break;
      }
    }

    if(dec_cont->image.header_ready)
      break;
  } while((dec_cont->stream.read_bits >> 3) <= dec_cont->stream.stream_length);

  return (JPEGDEC_OK);

#undef JPG_SCN
#undef JPG_VLC
#undef JPG_QTB
#undef JPG_FRM

}

/*------------------------------------------------------------------------------
    Function name   : JpegRefreshRegs
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void JpegRefreshRegs(JpegDecContainer * dec_cont) {
  i32 i;
  u32 offset = 0x0;

  u32 *pp_regs = dec_cont->jpeg_regs;

  for(i = DEC_X170_REGISTERS; i > 0; i--) {
    *pp_regs++ = DWLReadReg(dec_cont->dwl, dec_cont->core_id, offset);
    offset += 4;
  }
}

/*------------------------------------------------------------------------------
    Function name   : JpegFlushRegs
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void JpegFlushRegs(JpegDecContainer * dec_cont) {
  i32 i;
  u32 offset = 0;
  u32 *pp_regs = dec_cont->jpeg_regs;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

#ifdef JPEGDEC_ASIC_TRACE
  {
    JPEGDEC_TRACE_INTERNAL(("INTERNAL: REGS BEFORE HW ENABLE\n"));
    PrintJPEGReg(dec_cont->jpeg_regs);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

#ifdef JPEGDEC_INTEGRATOR
  DWLWriteReg(dec_cont->dwl, 0, 0x00000000);
#endif /* #ifdef JPEGDEC_INTEGRATOR */

  /* skip id register */
  //pp_regs++; offset = 4;

  for(i = DEC_X170_REGISTERS; i > 0; i--) {
    DWLWriteReg(dec_cont->dwl,dec_cont->core_id, offset, *pp_regs);
    //*pp_regs = 0;
    pp_regs++;
    offset += 4;
  }
#if 0
  if(sizeof(void *) == 8) {
    if (hw_feature.addr64_support) {
      offset = TOTAL_X170_ORIGIN_REGS * 0x04;
      pp_regs = dec_cont->jpeg_regs + TOTAL_X170_ORIGIN_REGS;
      for(i = DEC_X170_EXPAND_REGS; i > 0; --i) {
        DWLWriteReg(dec_cont->dwl,dec_cont->core_id, offset, *pp_regs);
        //*pp_regs = 0;
        pp_regs++;
        offset += 4;
      }
    } else {
      pp_regs = dec_cont->jpeg_regs + TOTAL_X170_ORIGIN_REGS;
      for(i = DEC_X170_EXPAND_REGS; i > 0; --i) {
        ASSERT(*pp_regs == 0);
        pp_regs++;
      }
    }
  } else {
    if (hw_feature.addr64_support) {
      offset = TOTAL_X170_ORIGIN_REGS * 0x04;
      for(i = DEC_X170_EXPAND_REGS; i > 0; --i) {
        DWLWriteReg(dec_cont->dwl,dec_cont->core_id, offset, 0);
        offset += 4;
      }
    }
  }

  offset = PP_START_UNIFIED_REGS * 0x04;
  pp_regs =  dec_cont->jpeg_regs + PP_START_UNIFIED_REGS;
  for(i = PP_UNIFIED_REGS; i > 0; --i) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, offset, *pp_regs);
    pp_regs++;
    offset += 4;
  }
#endif
}

static u32 NumBits(u32 value) {

  u32 num_bits = 0;

  while(value) {
    value >>= 1;
    num_bits++;
  }

  if(!num_bits) {
    num_bits = 1;
  }

  return (num_bits);

}

void JpegDecInitHWEmptyScan(JpegDecContainer * dec_cont, u32 component_id) {

  u32 i;
  i32 n;
  addr_t coeff_buffer = 0;
  u32 num_blocks;
  u32 num_max;
  u8 *p_strm;
  u32 bits;
  u32 bit_pos;
  u32 *p_table_base = NULL;
  u32 hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_JPEG_DEC);
  struct DecHwFeatures hw_feature;
  u32 core_id = 0;
  ASSERT(dec_cont);

  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  num_blocks = dec_cont->frame.hw_x * dec_cont->frame.hw_y / 64;
  coeff_buffer = dec_cont->info.p_coeff_base.bus_address;
  if(component_id) {
    coeff_buffer += JPEGDEC_COEFF_SIZE * num_blocks;
    if(dec_cont->info.y_cb_cr_mode_orig == JPEGDEC_YUV420) {
      dec_cont->info.X /= 2;
      if(dec_cont->info.X & 0xF) {
        dec_cont->info.X += 8;
        dec_cont->info.fill_x = 1;
      }
      dec_cont->info.Y /= 2;
      if(dec_cont->info.Y & 0xF) {
        dec_cont->info.Y += 8;
        dec_cont->info.fill_y = 1;
      }
      num_blocks /= 4;
    } else if(dec_cont->info.y_cb_cr_mode_orig == JPEGDEC_YUV422) {
      dec_cont->info.X /= 2;
      if(dec_cont->info.X & 0xF) {
        dec_cont->info.X += 8;
        dec_cont->info.fill_x = 1;
      }
      num_blocks /= 2;
    } else if(dec_cont->info.y_cb_cr_mode_orig == JPEGDEC_YUV440) {
      dec_cont->info.Y /= 2;
      if(dec_cont->info.Y & 0xF) {
        dec_cont->info.Y += 8;
        dec_cont->info.fill_y = 1;
      }
      num_blocks /= 2;
    }
    if(component_id > 1)
      coeff_buffer += JPEGDEC_COEFF_SIZE * num_blocks;
  }

  p_strm = (u8*)dec_cont->info.tmp_strm.virtual_address;
  num_max = 0;
  while(num_blocks > 32767) {
    num_blocks -= 32767;
    num_max++;
  }

  n = NumBits(num_blocks);

  /* do we still have correct quantization tables ?? */
  JPEGDEC_TRACE_INTERNAL(("INTERNAL: Write AC,DC,QP tables to base\n"));
  //JpegDecWriteTablesProgressive(dec_cont);

  /* two vlc codes, both with length 1 (can be done?), 0 for largest eob, 1
   * for last eob (EOBn) */
  /* write "length amounts" */
  SetDecRegister(dec_cont->jpeg_regs, HWIF_AC1_CODE1_CNT, 2);

  /* codeword values 0xE0 (for EOB run of 32767 blocks) and 0xn0 */
  p_table_base = dec_cont->frame.p_table_base[core_id].virtual_address;
  p_table_base += 48;   /* start of vlc tables */
  *p_table_base = (0xE0 << 24) | ((n - 1) << 20);

  /* write num_max ext eobs of length 32767 followed by last ext eob */
  bit_pos = 0;
  for(i = 0; i < num_max; i++) {
    bits = 0x3FFF << 17;
    *p_strm = (bit_pos ? *p_strm : 0) | bits >> (24 + bit_pos);
    p_strm++;
    bits <<= 8 - bit_pos;
    *p_strm = bits >> 24;
    if(bit_pos >= 1) {
      p_strm++;
      bits <<= 8;
      *p_strm = bits >> 24;
    }
    bit_pos = (bit_pos + 15) & 0x7;
  }

  if(num_blocks) {
    /* codeword to be written:
     * '1' to indicate EOBn followed by number of blocks - 2^(n-1) */
    bits = num_blocks << (32 - n);
    *p_strm = (bit_pos ? *p_strm : 0) | bits >> (24 + bit_pos);
    p_strm++;
    bits <<= 8 - bit_pos;
    n -= 8 - bit_pos;
    while(n > 0) {
      *p_strm++ = bits >> 24;
      bits <<= 8;
      n -= 8;
    }
  }

  SET_ADDR_REG(dec_cont->jpeg_regs, HWIF_RLC_VLC_BASE,
               dec_cont->info.tmp_strm.bus_address);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STRM_START_BIT, 0);
  SetDecRegister(dec_cont->jpeg_regs, HWIF_STREAM_LEN, EMPTY_SCAN_LEN);

  dec_cont->scan.Ss = 1;
  dec_cont->scan.Se = 1;
  dec_cont->scan.Ah = 0;
  dec_cont->scan.Al = 0;

  dec_cont->fake_empty_scan = 1;

#ifdef JPEGDEC_ASIC_TRACE
  {
    JPEGDEC_TRACE_INTERNAL(("PROGRESSIVE CONTINUE: REGS BEFORE IRQ CLEAN\n"));
    PrintJPEGReg(dec_cont->jpeg_regs);
  }
#endif /* #ifdef JPEGDEC_ASIC_TRACE */

}

