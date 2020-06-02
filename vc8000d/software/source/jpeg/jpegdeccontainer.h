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

#ifndef JPEGDECCONT_H
#define JPEGDECCONT_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "jpegdecapi.h"
#include "dwl.h"
#include "deccfg.h"
#include "decppif.h"
#include "vpufeature.h"
#include "decapicommon.h"
#include "ppu.h"
#include "input_queue.h"
/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/
#ifdef _ASSERT_USED
#include <assert.h>
#endif

/* macro for assertion, used only if compiler flag _ASSERT_USED is defined */
#ifdef _ASSERT_USED
#define ASSERT(expr) assert(expr)
#else
#define ASSERT(expr)
#endif

#define MIN_NUMBER_OF_COMPONENTS 1
#define MAX_NUMBER_OF_COMPONENTS 3

#define JPEGDEC_X170_MIN_BUFFER 5120
#define JPEGDEC_X170_MAX_BUFFER (1<<30) /* 16776960 */
#define JPEGDEC_MAX_SLICE_SIZE 4096
#define JPEGDEC_TABLE_SIZE 544
#define JPEGDEC_MIN_WIDTH 48
#define JPEGDEC_MIN_HEIGHT 48
#define JPEGDEC_MAX_SLICE_SIZE_8190 8100
#define JPEGDEC_MAX_SLICE_SIZE_WEBP (1<<30)
#define JPEGDEC_MAX_WIDTH_TN 256
#define JPEGDEC_MAX_HEIGHT_TN 256
#define JPEGDEC_YUV400 0
#define JPEGDEC_YUV420 2
#define JPEGDEC_YUV422 3
#define JPEGDEC_YUV444 4
#define JPEGDEC_YUV440 5
#define JPEGDEC_YUV411 6
#define JPEGDEC_BASELINE_TABLE_SIZE 544
#define JPEGDEC_PROGRESSIVE_TABLE_SIZE 888//896//576
#define JPEGDEC_QP_BASE 32
#define JPEGDEC_AC1_BASE 48
#define JPEGDEC_AC2_BASE 88
#define JPEGDEC_DC1_BASE 129
#define JPEGDEC_DC2_BASE 132
#define JPEGDEC_DC3_BASE 135

#define PJPEGDEC_AC1_BASE 48
#define PJPEGDEC_AC2_BASE 88
#define PJPEGDEC_AC3_BASE 129
#define PJPEGDEC_AC4_BASE 169
#define PJPEGDEC_DC1_BASE 210
#define PJPEGDEC_DC2_BASE 213
#define PJPEGDEC_DC3_BASE 216
#define PJPEGDEC_DC4_BASE 219

/* progressive */
#define JPEGDEC_COEFF_SIZE 96

#define MAX_FRAME_NUMBER 32
#define FB_HW_ONGOING 0x30U

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

typedef struct {
  u32 C;  /* Component id */
  u32 H;  /* Horizontal sampling factor */
  u32 V;  /* Vertical sampling factor */
  u32 Tq; /* Quantization table destination selector */
} Components;

typedef struct {
  u8 *p_start_of_stream;
  u8 *p_curr_pos;
  addr_t stream_bus;
  u32 bit_pos_in_byte;
  u32 stream_length;
  u32 read_bits;
  u32 appn_flag;
  u32 thumbnail;
  u32 return_sos_marker;
} StreamStorage;

typedef struct {
  u8 *p_start_of_image;
  u8 *p_lum;
  u8 *p_cr;
  u8 *p_cb;
  u32 image_ready;
  u32 header_ready;
  u32 size;
  u32 size_luma;
  u32 size_chroma;
  u32 ready;
  u32 columns[MAX_NUMBER_OF_COMPONENTS];
  u32 pixels_per_row[MAX_NUMBER_OF_COMPONENTS];
} ImageData;

typedef struct {
  u32 Lf;
  u32 P;
  u32 Y;
  u32 hw_y;
  u32 X;
  u32 hw_x;
  u32 hw_ytn;
  u32 hw_xtn;
  u32 full_x;
  u32 full_y;
  u32 Nf; /* Number of components in frame */
  u32 coding_type;
  u32 num_mcu_in_frame;
  u32 num_mcu_in_row;
  u32 mcu_number;
  u32 next_rst_number;
  u32 Ri;
  u32 dri_period;
  u32 block;
  u32 row;
  u32 col;
  u32 c_index;
  u32 *p_buffer;
  addr_t buffer_bus;
  i32 *p_buffer_cb;
  i32 *p_buffer_cr;
  struct DWLLinearMem p_table_base[MAX_ASIC_CORES];
  u32 num_blocks[MAX_NUMBER_OF_COMPONENTS];
  u32 blocks_per_row[MAX_NUMBER_OF_COMPONENTS];
  u32 use_ac_offset[MAX_NUMBER_OF_COMPONENTS];
  Components component[MAX_NUMBER_OF_COMPONENTS];
  u32 mcu_height; /* mcu height in pixels */
  u32 num_mcu_rows_in_interval;
  u32 num_mcu_rows_in_frame;
  u32 intervals;  /* total count of restart intervals */
  u8 **ri_array;  /* pointers to the beginnings of each ri. */
} FrameInfo;

typedef struct {
  u32 Ls;
  u32 Ns;
  u32 Cs[MAX_NUMBER_OF_COMPONENTS];   /* Scan component selector */
  u32 Td[MAX_NUMBER_OF_COMPONENTS];   /* Selects table for DC */
  u32 Ta[MAX_NUMBER_OF_COMPONENTS];   /* Selects table for AC */
  u32 Ss;
  u32 Se;
  u32 Ah;
  u32 Al;
  u32 index;
  i32 num_idct_rows;
  i32 pred[MAX_NUMBER_OF_COMPONENTS];
} ScanInfo;

typedef struct {
  u32 slice_height;
  u32 amount_of_qtables;
  u32 y_cb_cr_mode;
  u32 column;
  u32 X;
  u32 Y;
  u32 mem_size;
  u32 SliceCount;
  u32 SliceReadyForPause;
  u32 SliceMBCutValue;
  u32 pipeline;
  u32 user_alloc_mem;
  u32 slice_mb_set_value;
  u32 timeout;
  u32 rlc_mode;
  u32 luma_pos;
  u32 chroma_pos;
  u32 slice_start_count;
  u32 amount_of_slices;
  u32 no_slice_irq_for_user;
  u32 slice_limit_reached;
  u32 input_buffer_empty;
  u32 fill_right;
  u32 fill_bottom;
  u32 stream_end;
  u32 stream_end_flag;
  u32 input_buffer_len;
  u32 input_streaming;
  u32 decoded_stream_len;
  u32 init;
  u32 init_thumb;
  u32 init_buffer_size;
  i32 dc_res[MAX_NUMBER_OF_COMPONENTS];
  struct DWLLinearMem out_luma;
  struct DWLLinearMem out_chroma;
  struct DWLLinearMem out_chroma2;
  struct DWLLinearMem given_out_luma;
  struct DWLLinearMem given_out_chroma;
  struct DWLLinearMem given_out_chroma2;
  struct DWLLinearMem out_pp_luma;
  struct DWLLinearMem out_pp_chroma;
  i32 pred[MAX_NUMBER_OF_COMPONENTS];
  /* progressive parameters */
  u32 non_interleaved;
  u32 component_id;
  u32 operation_type;
  u32 operation_type_thumb;
  u32 progressive_scan_ready;
  u32 non_interleaved_scan_ready;
  u32 allocated;
  u32 y_cb_cr_mode_orig;
  u32 get_info_ycb_cr_mode;
  u32 get_info_ycb_cr_mode_tn;
  u32 components[MAX_NUMBER_OF_COMPONENTS];
  struct DWLLinearMem p_coeff_base;

  u32 fill_x;
  u32 fill_y;
  u32 wdiv16;
  struct DWLLinearMem tmp_strm;
} DecInfo;

typedef struct {

  struct DWLLinearMem out_luma_buffer;
  struct DWLLinearMem out_chroma_buffer;
  struct DWLLinearMem out_chroma_buffer2;
  struct DWLLinearMem pp_luma_buffer;   /* luma/chroma both in this buffer */
  struct DWLLinearMem pp_chroma_buffer; /* not allocated, just form pp_luma_buffer */
} JpegAsicBuffers;

typedef struct {
  u32 bits[16];
  u32 *vals;
  u32 table_length;
  u32 start;
  u32 last;
} VlcTable;

typedef struct {
  u32 Lh;
  u32 default_tables;
  VlcTable ac_table0;
  VlcTable ac_table1;
  VlcTable ac_table2;
  VlcTable ac_table3;
  VlcTable dc_table0;
  VlcTable dc_table1;
  VlcTable dc_table2;
  VlcTable dc_table3;
  VlcTable *table;
} HuffmanTables;

typedef struct {
  u32 Lq; /* Quantization table definition length */
  u32 table0[64];
  u32 table1[64];
  u32 table2[64];
  u32 table3[64];
  u32 *table;
} QuantTables;

typedef struct {
  u32 core_id;  /* Also used as cmd buffer id when VCMD is enabled. */
  u32 out_id;
  const u8 *stream;
  const void *p_user_data;
} JpegHwRdyCallbackArg;

typedef struct OutElement_ {
  u32 mem_idx;
  JpegDecOutput pic;
  JpegDecImageInfo info;
} OutElement;

typedef struct FrameBufferList_ {
  u32  fb_stat[MAX_FRAME_NUMBER];
  OutElement out_fifo[MAX_FRAME_NUMBER];
  int wr_id;
  int rd_id;
  int num_out;
  u32 b_initialized;
  u32 end_of_stream;

  sem_t out_count_sem;
  pthread_mutex_t out_count_mutex;
  pthread_mutex_t ref_count_mutex;
  pthread_cond_t hw_rdy_cv;
  u32 abort;
} FrameBufferList;

typedef struct {
  u32 jpeg_regs[TOTAL_X170_REGISTERS];
  u32 asic_running;
  StreamStorage stream;
  FrameInfo frame;
  ImageData image;
  ScanInfo scan;
  DecInfo info;
  HuffmanTables vlc;
  QuantTables quant;
  u32 tmp_data[64];
  u32 is8190;
  u32 cr_first;
  u32 fuse_burned;
  u32 min_supported_width;
  u32 min_supported_height;
  u32 max_supported_width;
  u32 max_supported_height;
  u32 max_supported_pixel_amount;
  u32 max_supported_slice_size;
  u32 extensions_supported;
  JpegAsicBuffers asic_buff;
  DecPpInterface pp_control;
  DecPpQuery pp_config_query;   /* Decoder asks pp info about setup, info stored here */
  u32 pp_status;
  const void *dwl;    /* DWL instance */
  i32 core_id;
  u32 pp_enabled;     /* set to 1 to enable pp */
  DecPicAlignment align;
  u32 dscale_shift_x;
  u32 dscale_shift_y;
  PpUnitIntConfig ppu_cfg[DEC_MAX_PPU_COUNT];
  u32 ppu_saved;
  u32 ppu_enabled_orig[DEC_MAX_PPU_COUNT];
  DelogoConfig delogo_params[2];
//  u32 hw_major_ver;          /* HW major version */
//  u32 hw_minor_ver;          /* HW minor version */
  struct DecHwFeatures hw_feature;

  struct DWLLinearMem ext_buffers[16];

  u32 pre_buf_size;  /* size of the requested previous external buffer, used for MJPEG dynamic size */
  u32 next_buf_size;  /* size of the requested external buffer */
  u32 buf_num;        /* number of buffers (with size of next_buf_size) requested to be allocated externally */
  u32 ext_buffer_num;
  u32 buffer_index;
  struct DWLLinearMem *buf_to_free;
  u32 tot_buffers;
  u32 n_ext_buf_size;        // size of external buffers added
  u32 buffer_queue_index;
  u32 realloc_buffer;

  InputQueue pp_buffer_queue;

  u32 b_mc;
  u32 n_cores;
  u32 n_cores_available;
  FrameBufferList fb_list;
  JpegHwRdyCallbackArg hw_rdy_callback_arg[MAX_MC_CB_ENTRIES];
  JpegDecImageInfo p_image_info;
  u32 dec_image_type;
  struct {
    JpegDecMCStreamConsumed *fn;
    const u8 *p_strm_buff; /* stream buffer passed in callback */
    const void *p_user_data; /* user data to be passed in callback */
  } stream_consumed_callback;

  const void *pp_instance;
  void (*PPRun) (const void *, const DecPpInterface *);
  void (*PPEndCallback) (const void *);
  void (*PPConfigQuery) (const void *, DecPpQuery *);

  /*
   * Whether it supports restart interval based multicore decoding.
   * If it's supported in sw, when DRI marker is detected and restart interval
   * satisfies following condition, then SW will parse all the restart interval
   * form input bitstream.
   *  a) each interval covers one or multiple of complete MCU rows, and
   *  b) pp out won't use pixels that cross interval boundary
   **/
  u32 ri_running_cores;   /* cores number running for ri decoding */
  sem_t sem_ri_mc_done;   /* ri mc decoding done */
  u32 ri_index;           /* ri to be decoded */
  u32 first_ri_index;     /* first restart interval of cropping window. */
  u32 last_ri_index;      /* last restart interval of cropping window */
  /* start/end ri interval for each PP units */
  u32 start_ri_index[DEC_MAX_PPU_COUNT];
  u32 end_ri_index[DEC_MAX_PPU_COUNT];
  u32 allow_sw_ri_parse;  /* Allow sw parsing intervals when it's not present? */
  /* multicore to decode one picture based on restart interval. */
  enum {
    RI_MC_UNDETERMINED,
    RI_MC_ENA,
    RI_MC_DISABLED_BY_USER,   /* user disabled sw parsing */
    RI_MC_DISABLED_BY_RI,     /* cond a) not satisfied */
    RI_MC_DISABLED_BY_PP,     /* cond b) not satisfied */
  } ri_mc_enabled;
  u32 abort;

  u32 scan_num;
  u32 last_scan;
  u16 pjpeg_coeff_bit_map[3][64];
  u32 jpeg_hw_start_bus;

  u32 vcmd_used;  /* decoder connects to VCMD engine to start HW */
  u32 cmd_buf_id; /* command buffer id used for current picture */
  u32 fake_empty_scan;

  u32 low_latency;
} JpegDecContainer;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

#define RI_MC_DISABLED(ri) ((ri) == RI_MC_DISABLED_BY_USER || \
                            (ri) == RI_MC_DISABLED_BY_RI || \
                            (ri) == RI_MC_DISABLED_BY_PP)
#define RI_MC_ENABLED(ri) ((ri) == RI_MC_ENA)

#endif /* #endif JPEGDECDATA_H */
