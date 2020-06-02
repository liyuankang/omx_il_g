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

/*
*************************************************************************************
* File name: global.h
* Function:  global definitions for for AVS decoder.
*
*************************************************************************************
*/

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdio.h>  //!< for FILE
#include "commonStructures.h"
#include "commonVariables.h"
#include "defines.h"

#if FIX_CHROMA_FIELD_MV_BK_DIST
//char bk_img_is_top_field;
#endif

void write_GB_frame(FILE *p_dec);

#if !FIX_MAX_REF
#define MAXREF 4
#define MAXGOP 32
#endif

typedef struct StatBits {
  int curr_frame_bits;
  int prev_frame_bits;
  int emulate_bits;
  int prev_emulate_bits;
  int last_unit_bits;
  int bitrate;
  int total_bitrate[1000];
  int coded_pic_num;
  int time_s;
} StatBits;

typedef struct reference_management {
  int poc;
  int qp_offset;
  int num_of_ref;
  int referd_by_others;
  int ref_pic[MAXREF];
  int predict;
  int deltaRPS;
  int num_to_remove;
  int remove_pic[MAXREF];
} ref_man;

//! Bitstream
typedef struct {
  // AEC Decoding
  int read_len;  //!< actual position in the codebuffer, AEC only
  int code_len;  //!< overall codebuffer length, AEC only
  // UVLC Decoding
  int frame_bitoffset;   //!< actual position in the codebuffer, bit-oriented,
                         // UVLC only
  int bitstream_length;  //!< over codebuffer lnegth, byte oriented, UVLC only
  // ErrorConcealment
  char *streamBuffer;  //!< actual codebuffer for read bytes
} Bitstream;
extern struct StrmData *currStream;

#if TRACE
// syntax trace context
struct syntax_ctx {
  FILE *p_trace;
  int bitcounter;
  char errortext[ET_SIZE];  //!< buffer for error message for exit with error()
};
#define TSYNTAX(fmt, ...)                      \
  {                                            \
    struct syntax_ctx *ctx = trace_get_ctx();  \
    fprintf(ctx->p_trace, fmt, ##__VA_ARGS__); \
    fflush(ctx->p_trace);                      \
  }
#else
#define TSYNTAX(fmt, ...)
#endif

/* ------------------------------------------------------
* dec data
*/
typedef struct {
  // byte **background_frame[3];
  struct DWLLinearMem background_frame[3];
  u8 *background_tbl_y;
  u8 *background_tbl_c;
  int background_reference_enable;

  int background_picture_flag;
  int background_picture_output_flag;
  int background_picture_enable;

  int background_number;

  int bcbr_enable;

  int demulate_enable;
  int currentbitoffset;

  int aspect_ratio_information;
  int frame_rate_code;
  int bit_rate_lower;
  int bit_rate_upper;
  int marker_bit;

  int video_format;
  int color_description;
  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;

  int progressive_sequence;
#if INTERLACE_CODING
  int is_field_sequence;
#endif
  int low_delay;
  int horizontal_size;
  int vertical_size;
  int video_range;

  int display_horizontal_size;
  int display_vertical_size;
  int TD_mode;
  int view_packing_mode;
  int view_reverse;

  int b_pmvr_enabled;
  int dhp_enabled;
  int b_dmh_enabled;
  int b_mhpskip_enabled;
  int wsm_enabled;
  int b_secT_enabled;

  int tmp_time;
  int FrameNum;
  int eos;
  int pre_img_type;
  int pre_img_types;
  // int pre_str_vec;
  int pre_img_tr;
  int pre_img_qp;
  int pre_tmp_time;
  int RefPicExist;  // 20071224
  int BgRefPicExist;
  int dec_ref_num;  // ref order

  /* video edit code */  // M1956 by Grandview 2006.12.12
  int vec_flag;

  /* Copyright_extension() header */
  int copyright_flag;
  int copyright_identifier;
  int original_or_copy;
  long long int copyright_number_1;
  long long int copyright_number_2;
  long long int copyright_number_3;
  /* Camera_parameters_extension */
  int camera_id;
  int height_of_image_device;
  int focal_length;
  int f_number;
  int vertical_angle_of_view;
  int camera_position_x_upper;
  int camera_position_x_lower;
  int camera_position_y_upper;
  int camera_position_y_lower;
  int camera_position_z_upper;
  int camera_position_z_lower;
  int camera_direction_x;
  int camera_direction_y;
  int camera_direction_z;
  int image_plane_vertical_x;
  int image_plane_vertical_y;
  int image_plane_vertical_z;

#if AVS2_HDR_HLS
  /* mastering_display_and_content_metadata_extension() header */
  int display_primaries_x0;
  int display_primaries_y0;
  int display_primaries_x1;
  int display_primaries_y1;
  int display_primaries_x2;
  int display_primaries_y2;
  int white_point_x;
  int white_point_y;
  int max_display_mastering_luminance;
  int min_display_mastering_luminance;
  int maximum_content_light_level;
  int maximum_frame_average_light_level;
#endif

  /* I_pictures_header() */
  int top_field_first;
  int repeat_first_field;
  int progressive_frame;
#if INTERLACE_CODING
  int is_top_field;
#endif
  // int fixed_picture_qp;   //qyu 0927
  int picture_qp;
  int fixed_picture_qp;
  int time_code_flag;
  int time_code;
  int loop_filter_disable;
  int loop_filter_parameter_flag;
  // int alpha_offset;
  // int beta_offset;

  /* Pb_picture_header() */
  int picture_coding_type;

  /*picture_display_extension()*/
  int frame_centre_horizontal_offset[4];
  int frame_centre_vertical_offset[4];

  /* slice_header() */
  int img_width;
  int slice_vertical_position;
  int slice_vertical_position_extension;
  int fixed_slice_qp;
  int slice_qp;
  int slice_horizontal_positon;  // added by mz, 2008.04
  int slice_horizontal_positon_extension;

  int StartCodePosition;
  int background_pred_flag;

  // Reference Manage
  int displaydelay;
  int picture_reorder_delay;
#if M3480_TEMPORAL_SCALABLE
  int temporal_id_exist_flag;
#endif

  int gop_size;
  ref_man decod_RPS[MAXGOP];
  ref_man curr_RPS;
  int last_output;
  int trtmp;
#if M3480_TEMPORAL_SCALABLE
  int cur_layer;
#endif

  // global picture format dependend buffers, mem allocation in ldecod.c
  // ******************

  // byte **imgYRef;                                //!< reference frame find
  // snr
  // byte ** *imgUVRef;
  struct DWLLinearMem imgYRef;  //!< reference frame find snr
  struct DWLLinearMem imgUVRef[2];

// Adaptive frequency weighting quantization
#if FREQUENCY_WEIGHTING_QUANTIZATION
  int weight_quant_enable_flag;
  int load_seq_weight_quant_data_flag;

  int pic_weight_quant_enable_flag;
  int pic_weight_quant_data_index;
  int weighting_quant_param;
  int weighting_quant_model;
  short quant_param_undetail[6];  // M2148 2007-09
  short quant_param_detail[6];    // M2148 2007-09
  int WeightQuantEnable;          // M2148 2007-09
  int mb_adapt_wq_disable;        // M2331 2008-04
  int mb_wq_mode;                 // M2331 2008-04
#if CHROMA_DELTA_QP
  int chroma_quant_param_disable;
  int chroma_quant_param_delta_u;
  int chroma_quant_param_delta_v;
#endif

  int b_pre_dec_intra_img;
  int pre_dec_img_type;
  int CurrentSceneModel;
#endif

  // byte **integerRefY[REF_MAXBUFFER];
  // byte **integerRefUV[REF_MAXBUFFER][2];
  struct DWLLinearMem *integerRefY[REF_MAXBUFFER];
  struct DWLLinearMem *integerRefUV[REF_MAXBUFFER][2];

  // byte **integerRefY_fref[2];
  // //integerRefY_fref[ref_index][height(height/2)][width] ref_index=0 for B
  // frame, ref_index=0,1 for B field
  // byte **integerRefY_bref[2];
  // //integerRefY_bref[ref_index][height(height/2)][width] ref_index=0 for B
  // frame, ref_index=0,1 for B field
  // byte
  // **integerRefUV_fref[2][2];//integerRefUV_fref[ref_index][uv][height/2][width]
  // ref_index=0 for B frame, ref_index=0,1 for B field
  // byte
  // **integerRefUV_bref[2][2];//integerRefUV_bref[ref_index][uv][height/2][width]
  // ref_index=0 for B frame, ref_index=0,1 for B field
  // byte ** *b_ref[2], * **f_ref[2];

  struct DWLLinearMem integerRefY_fref
      [2];  // integerRefY_fref[ref_index][height(height/2)][width] ref_index=0
            // for B frame, ref_index=0,1 for B field
  struct DWLLinearMem integerRefY_bref
      [2];  // integerRefY_bref[ref_index][height(height/2)][width] ref_index=0
            // for B frame, ref_index=0,1 for B field
  struct DWLLinearMem integerRefUV_fref
      [2][2];  // integerRefUV_fref[ref_index][uv][height/2][width] ref_index=0
               // for B frame, ref_index=0,1 for B field
  struct DWLLinearMem integerRefUV_bref
      [2][2];  // integerRefUV_bref[ref_index][uv][height/2][width] ref_index=0
               // for B frame, ref_index=0,1 for B field
  struct DWLLinearMem *b_ref[2];
  struct DWLLinearMem *f_ref[2];
  int curr_IDRcoi;
  int curr_IDRtr;
  int next_IDRtr;
  int next_IDRcoi;
  int end_SeqTr;

  // files
  FILE *p_out;  //<! pointer to output YUV file
  FILE *p_ref;  //<! pointer to input original reference YUV file file
  FILE *p_ref_background;
  FILE *p_out_background;

#if MB_DQP
  int lastQP;
// FILE * testQP;
#endif

  const void *dwl;

} Video_Dec_data;
extern Video_Dec_data *hd;

typedef struct {
  // AC ENGINE PARAMETERS
  // the width of rT1 is 8 bits., the init value is 0xff
  unsigned int rT1;
  // the width of valueT is 9 bits.,
  unsigned int valueT;
  // the bit width of rS1 is  the min integer  that it is greater than or equal
  // to Log(boundS+2), if boundS is 254, the width of rS1 is 8 bits.
  // if boundS = 1, the width of rS1 is 2 bits.
  unsigned char rS1;
  // the bit width of valueS is  the min integer  that it is greater than or
  // equal to Log(boundS+1), if boundS is 254, the width of valueS is 8 bits.
  // if boundS = 1, the width of rS1 is 2 bits.
  unsigned char valueS;
  // cFlag value is 0 or 1, cFlag=0, decode_aec_stuffing_bit or decode_bypass
  // mode,  cFlag=1, decode_desision mode
  // this flag can be removed.
  // unsigned char cFlag;
  unsigned char
      bFlag;  // value is 0 or 1; if valueT<0x100, bFlag=1 else bFlag=0
  unsigned char valueD;  //  is value in R domain 1 is R domain 0 is LG domain

  int last_dquant;

} DecodingEnvironment;
typedef DecodingEnvironment *DecodingEnvironmentPtr;

// added at rm52k version

struct inp_par;
struct Streamd;

struct syntaxelement {
  int type;     //!< type of syntax element for data part.
  int value1;   //!< numerical value of syntax element
  int value2;   //!< for blocked symbols, e.g. run/level
  int len;      //!< length of code
  int inf;      //!< info part of UVLC code
  int context;  //!< AEC context

  int golomb_grad;       //!< Needed if type is a golomb element
  int golomb_maxlevels;  //!< If this is zero, do not use the golomb coding
#if TRACE
#define TRACESTRING_SIZE 100           //!< size of trace string
  char tracestring[TRACESTRING_SIZE];  //!< trace string
#endif

  //! for mapping of UVLC to syntaxElement
  void (*mapping)(int len, int info, int *value1, int *value2);
  u32 (*reading)(struct Streamd *, SyntaxElement *, codingUnit *MB,
                 int uiPosition);
};

#if TRACE
void trace2out(SyntaxElement *se);
#endif

//! struct to characterize the state of the arithmetic coding engine
typedef struct datapartition {
  // Bitstream           *bitstream;
  DecodingEnvironment de_AEC;
  u32 (*readSyntaxElement)(struct Streamd *, SyntaxElement *,
                           codingUnit *currMB, int uiPosition);

} DataPartition;

//! Slice
struct slice {
  int picture_id;
  int qp;
  int picture_type;  //!< picture type
  int start_mb_nr;
  int max_part_nr;  //!< number of different partitions

  // added by lzhang
  DataPartition *partArr;       //!< array of partitions
  SyntaxInfoContexts *syn_ctx;  //!< pointer to struct of context models for use
                                // in AEC
};

struct alfdatapart {
  struct StrmData *bitstream;
  DecodingEnvironment de_AEC;
  SyntaxInfoContexts *syn_ctx;
};

// input parameters from configuration file
struct inp_par {
  char infile[1000];  //<! Telenor AVS input
#if B_BACKGROUND_Fix
  char backgroundref_File[1000];
#endif
  char outfile[1000];  //<! Decoded YUV 4:2:0 output
  char reffile[1000];  //<! Optional YUV 4:2:0 reference file for SNR
                       // measurement

#if ROI_M3264
  char out_datafile[1000];  //<! Location data output...
  int ROI_Coding;
#endif
  int FileFormat;        //<! File format of the Input file, PAR_OF_ANNEXB or
                         // PAR_OF_RTP
  int buf_cycle;         //<! Frame buffer size
  int LFParametersFlag;  //<! Specifies that loop filter parameters are included
                         // in bitstream
  int yuv_structure;     //<! Specifies reference frame's yuv structure
  int check_BBV_flag;    //<! Check BBV buffer (0: disable, 1: enable)
  int ref_pic_order;     //<! ref order
  int output_dec_pic;    //<! output_dec_pic
  int profile_id;
  int level_id;
  int chroma_format;
  int g_uiMaxSizeInBit;
  int alpha_c_offset;
  int beta_offset;
  int useNSQT;
#if MB_DQP
  int useDQP;
#endif
  int useSDIP;
  int sao_enable;
#if M3480_TEMPORAL_SCALABLE
  int temporal_id_exist_flag;
#endif
  int alf_enable;

  int crossSliceLoopFilter;

  int sample_bit_depth;  // sample bit depth
  int output_bit_depth;  // decoded file bit depth (assuming output_bit_depth is
                         // less or equal to sample_bit_depth)

  int MD5Enable;
  int frame_cnt;
  int trace_enable;

#if OUTPUT_INTERLACE_MERGED_PIC
  int output_interlace_merged_picture;
#endif
};

extern struct inp_par *input;

typedef struct {
#if RD170_FIX_BG
  STDOUT_DATA stdoutdata[REF_MAXBUFFER];
#else
  STDOUT_DATA stdoutdata[8];
#endif
  int buffer_num;
} outdata;
//outdata outprint;

// prototypes
void init_conf(int numpar, char **config_str);  // 20071224, command line
void print_help();
void parse_argu(int num_argu, char **input_argv);  // 20071224, command line
void report(SNRParameters *snr);
void find_snr(SNRParameters *snr, FILE *p_ref);
void init();

int decode_one_frame(SNRParameters *snr);

void init_frame();

// Fix by Sunil for RD5.0 test in Linux (2013.11.06)
void write_frame(FILE *p_out, int pos);

void picture_data();

int sign(int a, int b);

int Header();

void DeblockFrame(u16 *imgY, u16 **imgUV);

// dynamic memory allocation
int init_global_buffers();
void free_global_buffers();

void Update_Picture_Buffers();

void frame_postprocessing();

void report_frame(outdata data, int pos);

#define PAYLOAD_TYPE_IDERP 8

struct StrmData *AllocBitstream();
void FreeBitstream();
#if TRACE
void tracebits2(const char *trace_str, int len, int info);
#endif

// int   direct_mv[45][80][4][4][3]; // only to verify result

#define I_PICTURE_START_CODE 0xB3
#define PB_PICTURE_START_CODE 0xB6
#define SLICE_START_CODE_MIN 0x00
#define SLICE_START_CODE_MAX 0x8F
#define USER_DATA_START_CODE 0xB2
#define SEQUENCE_HEADER_CODE 0xB0
#define EXTENSION_START_CODE 0xB5
#define SEQUENCE_END_CODE 0xB1
#define VIDEO_EDIT_CODE 0xB7

#define SEQUENCE_DISPLAY_EXTENSION_ID 2
#define COPYRIGHT_EXTENSION_ID 4
#define CAMERAPARAMETERS_EXTENSION_ID 11
#define PICTURE_DISPLAY_EXTENSION_ID 7
#if M3480_TEMPORAL_SCALABLE
#define TEMPORAL_SCALABLE_EXTENSION_ID 3
#endif

#if ROI_M3264
#if RD1501_FIX_BG
#define LOCATION_DATA_EXTENSION_ID 12
#else
#define LOCATION_DATA_EXTENSION_ID 15
#endif
#endif

#if AVS2_HDR_HLS
#define MASTERING_DISPLAY_AND_CONTENT_METADATA_EXTENSION 10
#endif

#endif
