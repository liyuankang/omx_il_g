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

#ifndef __COMMAND_LINE_PARSER_H__
#define __COMMAND_LINE_PARSER_H__

#include "basetype.h"
#include "decapicommon.h"
#include "commonconfig.h"
#include "error_simulator.h"
#include "dectypes.h"
#include "tb_defs.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STREAMS 16

enum {
  VP8DEC_DECODER_ALLOC = 0,
  VP8DEC_EXTERNAL_ALLOC = 1,
  VP8DEC_EXTERNAL_ALLOC_ALT = 2
};

enum FileFormat {
  FILEFORMAT_AUTO_DETECT = 0,
  FILEFORMAT_BYTESTREAM,
  FILEFORMAT_IVF,
  FILEFORMAT_WEBM,
  FILEFORMAT_BYTESTREAM_H264,
  FILEFORMAT_AVS2,
  FILEFORMAT_MAX
};

enum StreamReadMode {
  STREAMREADMODE_FRAME = 0,
  STREAMREADMODE_NALUNIT = 1,
  STREAMREADMODE_FULLSTREAM = 2,
  STREAMREADMODE_PACKETIZE = 3
};

enum SinkType {
  SINK_FILE_SEQUENCE = 0,
  SINK_FILE_PICTURE,
  SINK_MD5_SEQUENCE,
  SINK_MD5_PICTURE,
  SINK_SDL,
  SINK_NULL
};

enum TestBenchFormat {
  H264DEC = 0x1,
  MPEG4DEC = 0x2,
  JPEG_DEC = 0x4,
  VC1DEC = 0x8,
  MPEG2DEC = 0x10,
  VP6DEC = 0x20,
  RVDEC = 0x40,
  VP8DEC = 0x80,
  AVSDEC = 0x100,
  G2DEC = 0x200,  /* hevc/vp9/avs2 */
  PPDEC = 0x400,
  AV1DEC = 0x800,
  ALLFORMATS = 0xFFFFFFF
};

struct TestParams {
  char* in_file_name;
  char* out_file_name[2*DEC_MAX_OUT_COUNT];
  u32 num_of_decoded_pics;
  enum DecPictureFormat format;
  enum DecPictureFormat hw_format;
  enum SinkType sink_type;
  u32 pp_enabled;
  u8 display_cropped;
  u8 hw_traces;
  u8 trace_target;
  u8 extra_output_thread;
  u8 disable_display_order;
  u8 only_full_resolution;  /* jpeg full resolution only without thumbnail */
  u8 ri_mc_enable; /* restart interval based on multicore decoding (JPEG) */
  u32 instant_buffer; /* jpeg output buffer provided by user */
  enum FileFormat file_format;
  enum StreamReadMode read_mode;
  struct ErrorSimulationParams error_sim;
  enum DecErrorConcealment concealment_mode;
  enum DecDecoderMode decoder_mode;
  struct DecFixedScaleCfg fscale_cfg;
  PpUnitConfig ppu_cfg[DEC_MAX_PPU_COUNT];
  DelogoConfig delogo_params[2];
  DecPicAlignment align;
  u8 compress_bypass;   /* compressor bypass flag */
  u8 is_ringbuffer;     /* ringbuffer mode by default */
  u32 tile_by_tile;
  u32 pp_standalone;    /* PP in standalone mode */
  /*only used for cache&shaper rtl simulation*/
  u32 shaper_bypass;
  u32 cache_enable;
  u32 shaper_enable;
  u32 out_format_conv;   /* output format conversion enabled in TB (via -T/-S/-P options) */
  u32 mc_enable;
  u32 mvc;              /* MVC for H264. */
  u32 rgb_stan;
  u32 video_range;
  u32 rlc_mode;
  /*for Multi-stream*/
  i32 nstream;
  i32 multimode;  // Multi-stream mode, 0--disable, 1--mult-thread //Next: 2--multi-process
  char *streamcfg[MAX_STREAMS];
  union {
	  int pid[MAX_STREAMS];

	  pthread_t *tid[MAX_STREAMS];

  } multi_stream_id;
};

void PrintUsage(char* executable, enum TestBenchFormat fmt);
void SetupDefaultParams(struct TestParams* params);
int ParseParams(int argc, char* argv[], struct TestParams* params);
int ResolveOverlap(struct TestParams* params);
void ResolvePpParamsOverlap(struct TestParams* params,
                           struct TBPpUnitParams *pp_units_params,
                           u32 standalone);
#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_LINE_PARSER_H__ */
