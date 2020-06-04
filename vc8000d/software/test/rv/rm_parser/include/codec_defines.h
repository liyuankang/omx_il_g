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

#ifndef CODEC_DEFINES_H
#define CODEC_DEFINES_H

/* Audio codec defines */
#define RA_FORMAT_ID        0x2E7261FD  /* RealAudio Stream */
#define RA_NO_INTERLEAVER   0x496E7430  /* 'Int0' (no interleaver) */
#define RA_INTERLEAVER_SIPR 0x73697072  /* interleaver used for SIPRO codec ("sipr") */
#define RA_INTERLEAVER_GENR 0x67656E72  /* interleaver used for ra8lbr and ra8hbr codecs ("genr") */
#define RA_INTERLEAVER_VBRS 0x76627273  /* Simple VBR interleaver ("vbrs") */
#define RA_INTERLEAVER_VBRF 0x76627266  /* Simple VBR interleaver (with possibly fragmenting) ("vbrf") */

/* Video codec defines */
#define HX_MEDIA_AUDIO    0x4155444FL /* 'AUDO' */
#define HX_MEDIA_VIDEO    0x5649444FL /* 'VIDO' */

#define HX_RVTRVIDEO_ID   0x52565452  /* 'RVTR' (for rv20 codec) */
#define HX_RVTR_RV30_ID   0x52565432  /* 'RVT2' (for rv30 codec) */
#define HX_RV20VIDEO_ID   0x52563230  /* 'RV20' */
#define HX_RV30VIDEO_ID   0x52563330  /* 'RV30' */
#define HX_RV40VIDEO_ID   0x52563430  /* 'RV40' */
#define HX_RVG2VIDEO_ID   0x52564732  /* 'RVG2' (raw TCK format) */
#define HX_RV89COMBO_ID   0x54524F4D  /* 'TROM' (raw TCK format) */

#define RV20_MAJOR_BITSTREAM_VERSION  2

#define RV30_MAJOR_BITSTREAM_VERSION  3
#define RV30_BITSTREAM_VERSION        2
#define RV30_PRODUCT_RELEASE          0
#define RV30_FRONTEND_VERSION         2

#define RV40_MAJOR_BITSTREAM_VERSION  4
#define RV40_BITSTREAM_VERSION        0
#define RV40_PRODUCT_RELEASE          2
#define RV40_FRONTEND_VERSION         0

#define RAW_BITSTREAM_MINOR_VERSION   128

#define RAW_RVG2_MAJOR_VERSION        2
#define RAW_RVG2_BITSTREAM_VERSION    RAW_BITSTREAM_MINOR_VERSION
#define RAW_RVG2_PRODUCT_RELEASE      0
#define RAW_RVG2_FRONTEND_VERSION     0

#define RAW_RV8_MAJOR_VERSION         3
#define RAW_RV8_BITSTREAM_VERSION     RAW_BITSTREAM_MINOR_VERSION
#define RAW_RV8_PRODUCT_RELEASE       0
#define RAW_RV8_FRONTEND_VERSION      0

#define RAW_RV9_MAJOR_VERSION         4
#define RAW_RV9_BITSTREAM_VERSION     RAW_BITSTREAM_MINOR_VERSION
#define RAW_RV9_PRODUCT_RELEASE       0
#define RAW_RV9_FRONTEND_VERSION      0

#define WIDTHBYTES(i) ((unsigned long)((i+31)&(~31))/8)  /* ULONG aligned ! */
#define T_YUV420   10

#endif /* #ifndef CODEC_DEFINES_H */
