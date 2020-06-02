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

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "encoder.h"
#include "encoder_vc8000e.h"
#include "util.h"
#include "hevcencapi.h"
#include "OSAL.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX CODEC"

#if !defined (ENCVC8000E)
#error "SPECIFY AN ENCODER PRODUCT (ENCVC8000E) IN COMPILER DEFINES!"
#endif

/************************************************************
* ctrlsw API difference management
************************************************************/
#define VC8000E_API_VER (1)
#if VC8000E_API_VER == 1
#define API_EWLGetCoreNum(A)       EWLGetCoreNum()
#define API_VCEncInit(A, B, C)     VCEncInit((A), (B))
#else
#define API_EWLGetCoreNum(A)       EWLGetCoreNum(A)
#define API_VCEncInit(A, B, C)     VCEncInit((A), (B), (C))
#endif

#define MOVING_AVERAGE_FRAMES 120
#define LEAST_MONITOR_FRAME 3
#define MAX_GOP_LEN 300

/* Structure for command line options */
typedef struct VC8000E_OPTIONS
{
    VCEncStreamType streamType;
    OMX_U32 frameRateNum;
    OMX_U32 frameRateDenom;

    OMX_U32 inputRateNumer;
    OMX_U32 outputRateNumer;

    OMX_U32 nBitDepthLuma;
    OMX_U32 nBitDepthChroma;

    OMX_U32 nMaxTLayers;
    OMX_U32 ssim;
    OMX_U32 rdoLevel;
    OMX_U32 exp_of_input_alignment;
    OMX_U32 ctbRcMode;
    OMX_U32 cuInfoVersion;
    OMX_U32 gopSize;
    OMX_U32 gdrDuration;
    OMX_U32 nTcOffset;
    OMX_U32 nBetaOffset;
    OMX_U32 bEnableDeblockOverride;
    OMX_U32 bDeblockOverride;
    OMX_U32 bEnableSAO;
    OMX_U32 bEnableScalingList;
    OMX_U32 bCabacInitFlag;
    OMX_U32 RoiQpDelta_ver;
    OMX_U32 roiMapDeltaQpBlockUnit;
    OMX_U32 roiMapDeltaQpEnable;

    OMX_S32 nTargetBitrate;
    OMX_S32 nQpDefault;
    OMX_S32 nQpMin;
    OMX_S32 nQpMax;
    OMX_S32 nPictureRcEnabled;
    OMX_S32 nHrdEnabled;
    OMX_U32 eRateControl;


    OMX_U32 xOffset;
    OMX_U32 yOffset;
    OMX_U32 formatType;
    OMX_U32 angle;
    OMX_S32 intraPicRate;
    OMX_U32 bStrongIntraSmoothing;

    OMX_S32 interlacedFrame;
    OMX_U32 enableOutputCuInfo;

    OMX_U32 exp_of_ref_alignment;
    OMX_U32 exp_of_ref_ch_alignment;
    OMX_U32 P010RefEnable;
    OMX_U32 picOrderCntType;
    OMX_U32 log2MaxPicOrderCntLsb;
    OMX_U32 log2MaxFrameNum;
    OMX_U32 dumpRegister;
    OMX_U32 rasterscan;
    OMX_U32 parallelCoreNum;
    OMX_U32 lookaheadDepth;
    OMX_U32 bHalfDsInput;

    OMX_S32 ltrInterval;
    OMX_S32 longTermQpDelta;
    OMX_U32 longTermGap;
    OMX_U32 longTermGapOffset;
    OMX_S32 bFrameQpDelta;
    OMX_U32 max_cu_size;


    OMX_U32 bDisableDeblocking;
    OMX_U32 bSeiMessages;
    OMX_U32 nVideoFullRange;
    OMX_S32 sliceSize;

    OMX_STRING gopCfg;
    OMX_U32 gopLowdelay;

    OMX_S32 enableCabac;
    OMX_S32 videoRange;
    OMX_S32 enableRdoQuant;
    OMX_U32 fieldOrder;

    OMX_U32 cirStart;
    OMX_U32 cirInterval;

    OMX_U32 pcm_loop_filter_disabled_flag;
    OMX_U32 ipcmMapEnable;
    OMX_S32 roiQp[8];
    OMX_U32 chromaQpOffset;

    OMX_U32 noiseReductionEnable;
    OMX_U32 noiseLow;
    OMX_U32 noiseFirstFrameSigma;

    OMX_U32 tiles_enabled_flag;
    OMX_U32 num_tile_columns;
    OMX_U32 num_tile_rows;
    OMX_U32 loop_filter_across_tiles_enabled_flag;

    /* HDR10 */
    OMX_U32 hdr10_display_enable;
    OMX_U32 hdr10_dx0;
    OMX_U32 hdr10_dy0;
    OMX_U32 hdr10_dx1;
    OMX_U32 hdr10_dy1;
    OMX_U32 hdr10_dx2;
    OMX_U32 hdr10_dy2;
    OMX_U32 hdr10_wx;
    OMX_U32 hdr10_wy;
    OMX_U32 hdr10_maxluma;
    OMX_U32 hdr10_minluma;
    
    OMX_U32 hdr10_lightlevel_enable;
    OMX_U32 hdr10_maxlight;
    OMX_U32 hdr10_avglight;
    
    OMX_U32 hdr10_color_enable;
    OMX_U32 hdr10_primary;
    OMX_U32 hdr10_transfer;
    OMX_U32 hdr10_matrix;

    OMX_U32 RpsInSliceHeader;

    OMX_S32 blockRCSize;
    OMX_S32 rcQpDeltaRange;

    OMX_S32 rcBaseMBComplexity;
    OMX_S32 picQpDeltaMin;
    OMX_S32 picQpDeltaMax;
    OMX_S32 bitVarRangeI;
    OMX_S32 bitVarRangeP;
    OMX_S32 bitVarRangeB;

    OMX_S32 tolMovingBitRate;
    OMX_S32 tolCtbRcInter;
    OMX_S32 tolCtbRcIntra;

    OMX_S32 ctbRowQpStep;
    OMX_S32 monitorFrames;
    OMX_S32 bitrateWindow;

    OMX_S32 intraQpDelta;
    OMX_S32 fixedIntraQp;
    OMX_S32 vbr;
    OMX_S32 smoothPsnrInGOP;
    OMX_S32 u32StaticSceneIbitPercent;
    
    OMX_S32 picSkip;
    OMX_S32 vui_timing_info_enable;
    OMX_S32 hashtype;

    OMX_S32 firstPic;
    OMX_S32 lastPic;

    OMX_U32 hrdCpbSize;
    //OMX_U32 gdrDuration;

    /* Extension for CL244132*/
    /*MMU*/
    OMX_U32 mmuEnable;

    /*external SRAM*/
    OMX_U32 extSramLumHeightBwd;
    OMX_U32 extSramChrHeightBwd;
    OMX_U32 extSramLumHeightFwd;
    OMX_U32 extSramChrHeightFwd;

    /* AXI alignment */
    OMX_U32 AXIAlignment; // bit[31:28] AXI_burst_align_wr_common
                      // bit[27:24] AXI_burst_align_wr_stream
                      // bit[23:20] AXI_burst_align_wr_chroma_ref
                      // bit[19:16] AXI_burst_align_wr_luma_ref
                      // bit[15:12] AXI_burst_align_rd_common
                      // bit[11: 8] AXI_burst_align_rd_prp
                      // bit[ 7: 4] AXI_burst_align_rd_ch_ref_prefetch
                      // bit[ 3: 0] AXI_burst_align_rd_lu_ref_prefetch

    /* coded chroma_format_idc */
    OMX_U32 codedChromaIdc; // 0: 400, 1: 420, 2: 422

    /* Adaptive Quantization */
    OMX_U32 aq_mode; /* Mode for Adaptive Quantization - 0:none 1:uniform AQ 2:auto variance 3:auto variance with bias to dark scenes. Default 0 */
    float aq_strength; /* Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00 */
    OMX_U32 writeReconToDDR; /*HW write recon to DDR or not if it's pure I-frame encoding*/
    OMX_U32 TxTypeSearchEnable; /*av1 tx type search 1=enable 0=disable, enabled by default*/

    OMX_U32 PsyFactor;
    OMX_U32 meVertSearchRange;
	OMX_U32 layerInRefIdcEnable;

    OMX_S32 crf;             /*CRF constant [0,51]*/

    OMX_U32 preset;         /* 0...4 for HEVC. 0..1 for H264. Trade off performance and compression efficiency */
} VC8000E_OPTIONS;

typedef struct {
    OMX_S32 frame[MOVING_AVERAGE_FRAMES];
    OMX_S32 length;
    OMX_S32 count;
    OMX_S32 pos;
    OMX_S32 frameRateNumer;
    OMX_S32 frameRateDenom;
} ma_s;

typedef struct {
    OMX_S32 gop_frm_num;
    double sum_intra_vs_interskip;
    double sum_skip_vs_interskip;
    double sum_intra_vs_interskipP;
    double sum_intra_vs_interskipB;
    OMX_S32 sum_costP;
    OMX_S32 sum_costB;
    OMX_S32 last_gopsize;
} adapGopCtr;


typedef struct ENCODER_CODEC
{
    ENCODER_PROTOTYPE base;

    VCEncVideoCodecFormat codecFormat;
    VCEncProfile profile;
    VCEncLevel level;
    VCEncTier tier;

    VCEncInst instance;
    VCEncIn encIn;
    VCEncOut encOut;

    OMX_U32 origWidth;
    OMX_U32 origHeight;
    OMX_U32 width;
    OMX_U32 height;

    OMX_U32 nEstTimeInc;
    OMX_U32 bStabilize;
    OMX_U32 nIFrameCounter;
    OMX_U32 nRefFrames;
    OMX_U32 nPFrames;
    OMX_U32 nBFrames;
    OMX_U32 nTotalFrames;

    VCEncGopPicConfig gopPicCfg[MAX_GOP_PIC_CONFIG_NUM];
    VCEncGopPicConfig gopPicCfgPass2[MAX_GOP_PIC_CONFIG_NUM];
    VCEncGopPicSpecialConfig gopPicSpecialCfg[MAX_GOP_SPIC_CONFIG_NUM];
    OMX_U32 maxRefPics;
    OMX_U32 maxTemporalId;

    OMX_S32 nextGopSize;
    VCEncPictureCodingType nextCodingType;

    VC8000E_OPTIONS options;

    OMX_U32 validencodedframenumber;
    ma_s ma;
    adapGopCtr agop;

    OMX_U32 currBitrate;
} ENCODER_CODEC;

#define IS_HEVC(a)  ((a)==VCENC_VIDEO_CODEC_HEVC)
#define IS_H264(a)  ((a)==VCENC_VIDEO_CODEC_H264)
#define IS_AV1(a)   ((a)==VCENC_VIDEO_CODEC_AV1)

#define MAX_LINE_LENGTH_BLOCK (512*8)




//           Type POC QPoffset  QPfactor  num_ref_pics ref_pics  used_by_cur
OMX_STRING RpsDefault_GOPSize_1[] = {
    "Frame1:  P    1   0        0.578     0      1        -1         1",
    NULL,
};
OMX_STRING RpsDefault_H264_GOPSize_1[] = {
    "Frame1:  P    1   0        0.4     0      1        -1         1",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_2[] = {
    "Frame1:  P    2   0        0.6     0      1        -2         1",
    "Frame2:  B    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_3[] = {
    "Frame1:  P    3   0        0.5     0      1        -3         1   ",
    "Frame2:  B    1   0        0.5     0      2        -1 2       1 1 ",
    "Frame3:  B    2   0        0.68    0      2        -1 1       1 1 ",
    NULL,
};


OMX_STRING RpsDefault_GOPSize_4[] = {
    "Frame1:  P    4   0        0.5      0     1       -4         1 ",
    "Frame2:  B    2   0        0.3536   0     2       -2 2       1 1", 
    "Frame3:  B    1   0        0.5      0     3       -1 1 3     1 1 0", 
    "Frame4:  B    3   0        0.5      0     2       -1 1       1 1 ",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_5[] = {
    "Frame1:  P    5   0        0.442    0     1       -5         1 ",
    "Frame2:  B    2   0        0.3536   0     2       -2 3       1 1", 
    "Frame3:  B    1   0        0.68     0     3       -1 1 4     1 1 0", 
    "Frame4:  B    3   0        0.3536   0     2       -1 2       1 1 ",
    "Frame5:  B    4   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_6[] = {
    "Frame1:  P    6   0        0.442    0     1       -6         1 ",
    "Frame2:  B    3   0        0.3536   0     2       -3 3       1 1", 
    "Frame3:  B    1   0        0.3536   0     3       -1 2 5     1 1 0", 
    "Frame4:  B    2   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame5:  B    4   0        0.3536   0     2       -1 2       1 1 ",
    "Frame6:  B    5   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_7[] = {
    "Frame1:  P    7   0        0.442    0     1       -7         1 ",
    "Frame2:  B    3   0        0.3536   0     2       -3 4       1 1", 
    "Frame3:  B    1   0        0.3536   0     3       -1 2 6     1 1 0", 
    "Frame4:  B    2   0        0.68     0     3       -1 1 5     1 1 0",
    "Frame5:  B    5   0        0.3536   0     2       -2 2       1 1 ",
    "Frame6:  B    4   0        0.68     0     3       -1 1 3     1 1 0",
    "Frame7:  B    6   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

OMX_STRING RpsDefault_GOPSize_8[] = {
    "Frame1:  P    8   0        0.442    0  1           -8        1 ",
    "Frame2:  B    4   0        0.3536   0  2           -4 4      1 1 ",
    "Frame3:  B    2   0        0.3536   0  3           -2 2 6    1 1 0 ",
    "Frame4:  B    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  B    3   0        0.68     0  3           -1 1 5    1 1 0",
    "Frame6:  B    6   0        0.3536   0  2           -2 2      1 1",
    "Frame7:  B    5   0        0.68     0  3           -1 1 3    1 1 0",
    "Frame8:  B    7   0        0.68     0  2           -1 1      1 1",
    NULL,
};

OMX_STRING RpsDefault_Interlace_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8       0   2           -1 -2     0 1",
    NULL,
};

OMX_STRING RpsLowdelayDefault_GOPSize_1[] = {
    "Frame1:  B    1   0        0.65      0     2       -1 -2         1 1",
    NULL,
};

OMX_STRING RpsLowdelayDefault_GOPSize_2[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -3         1 1",
    "Frame2:  B    2   0        0.578     0     2       -1 -2         1 1", 
    NULL,
};

OMX_STRING RpsLowdelayDefault_GOPSize_3[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -4         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1", 
    "Frame3:  B    3   0        0.578     0     2       -1 -3         1 1", 
    NULL,
};

OMX_STRING RpsLowdelayDefault_GOPSize_4[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -5         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1", 
    "Frame3:  B    3   0        0.4624    0     2       -1 -3         1 1", 
    "Frame4:  B    4   0        0.578     0     2       -1 -4         1 1",
    NULL,
};

OMX_STRING RpsPass2_GOPSize_4[] = {
    "Frame1:  B    4   0        0.5      0     2       -4 -8      1 1",
    "Frame2:  B    2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  B    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  B    3   0        0.5      0     3       -1 -3 1    1 0 1",
    NULL,
};

OMX_STRING RpsPass2_GOPSize_8[] = {
    "Frame1:  B    8   0        0.442    0  2           -8 -16    1 1",
    "Frame2:  B    4   0        0.3536   0  2           -4 4      1 1",
    "Frame3:  B    2   0        0.3536   0  3           -2 2 6    1 1 0",
    "Frame4:  B    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  B    3   0        0.68     0  4           -1 -3 1 5 1 0 1 0",
    "Frame6:  B    6   0        0.3536   0  3           -2 -6 2   1 0 1",
    "Frame7:  B    5   0        0.68     0  4           -1 -5 1 3 1 0 1 0",
    "Frame8:  B    7   0        0.68     0  3           -1 -7 1   1 0 1",
    NULL,
};

OMX_STRING RpsPass2_GOPSize_2[] = {
    "Frame1:  B    2   0        0.6     0      2        -2 -4      1 1",
    "Frame2:  B    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

i32 _CheckArea(VCEncPictureArea *area, ENCODER_CODEC *h)
{
  i32 width = (h->width + h->options.max_cu_size - 1) / h->options.max_cu_size;
  i32 height = (h->height + h->options.max_cu_size - 1) / h->options.max_cu_size;

  if ((area->left < (u32)width) && (area->right < (u32)width) &&
      (area->top < (u32)height) && (area->bottom < (u32)height)) return 1;

  return 0;
}

static OMX_STRING nextToken (OMX_STRING str)
{
  OMX_STRING p = (OMX_STRING)strchr((const OMX_STRING)str, ' ');
  if (p)
  {
    while (*p == ' ') p ++;
    if (*p == '\0') p = NULL;
  }
  return p;
}

int ParseGopConfigString (OMX_STRING line, VCEncGopConfig *gopCfg, int frame_idx, int gopSize)
{
  if (!line)
    return -1;

  //format: FrameN Type POC QPoffset QPfactor  num_ref_pics ref_pics  used_by_cur
  int frameN, poc, num_ref_pics, i;
  OMX_S8 type;
  VCEncGopPicConfig *cfg = NULL;
  VCEncGopPicSpecialConfig *scfg = NULL;

  //frame idx
  sscanf ((const OMX_STRING)line, "Frame%d", &frameN);
  if ((frameN != (frame_idx + 1)) && (frameN != 0)) return -1;

  if (frameN > gopSize)
      return 0;

  if (0 == frameN)
  {
      //format: FrameN Type  QPoffset  QPfactor   TemporalId  num_ref_pics   ref_pics  used_by_cur  LTR    Offset   Interval
      scfg = &(gopCfg->pGopPicSpecialCfg[gopCfg->special_size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%c", &type);
      if (type == 'I' || type == 'i')
          scfg->codingType = VCENC_INTRA_FRAME;
      else if (type == 'P' || type == 'p')
          scfg->codingType = VCENC_PREDICTED_FRAME;
      else if (type == 'B' || type == 'b')
          scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      else
          scfg->codingType = FRAME_TYPE_RESERVED;

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(scfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%lf", &(scfg->QpFactor));
      scfg->QpFactor = sqrt(scfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(scfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &num_ref_pics);
      if (num_ref_pics > VCENC_MAX_REF_FRAMES) /* NUMREFPICS_RESERVED -1 */
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }
      scfg->numRefPics = num_ref_pics;

      if ((scfg->codingType == VCENC_INTRA_FRAME) && (0 == num_ref_pics))
          num_ref_pics = 1;
      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp((const OMX_STRING)line, "L", 1) == 0) || (strncmp((const OMX_STRING)line, "l", 1) == 0))
          {
              sscanf((const OMX_STRING)line, "%c%d", &type, &(scfg->refPics[i].ref_pic));
              scfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC( scfg->refPics[i].ref_pic - 1 );
          }
          else
          {
              sscanf((const OMX_STRING)line, "%d", &(scfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf((const OMX_STRING)line, "%u", &(scfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      // LTR
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg->i32Ltr);
      if(VCENC_MAX_LT_REF_FRAMES < scfg->i32Ltr)
          return -1;
      
      // Offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg ->i32Offset );

      // Interval
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg ->i32Interval );

      if (0 != scfg->i32Ltr)
      {
          gopCfg->u32LTR_idx[ gopCfg->ltrcnt ] = LONG_TERM_REF_ID2DELTAPOC(scfg->i32Ltr-1);
          gopCfg->ltrcnt++;
          if (VCENC_MAX_LT_REF_FRAMES < gopCfg->ltrcnt)
              return -1;
      }

      // short_change
      scfg->i32short_change = 0;
      if (0 == scfg->i32Ltr)
      {
          /* not long-term ref */
          scfg->i32short_change = 1;
          for (i = 0; i < num_ref_pics; i++)
          {
              if (IS_LONG_TERM_REF_DELTAPOC(scfg->refPics[i].ref_pic)&& (0!=scfg->refPics[i].used_by_cur))
              {
                  scfg->i32short_change = 0;
                  break;
              }
          }
      }
  }
  else
  {
      //format: FrameN Type  POC  QPoffset    QPfactor   TemporalId  num_ref_pics  ref_pics  used_by_cur
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%c", &type);
      if (type == 'P' || type == 'p')
          cfg->codingType = VCENC_PREDICTED_FRAME;
      else if (type == 'B' || type == 'b')
          cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      else
          return -1;

      //poc
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &poc);
      if (poc < 1 || poc > gopSize) return -1;
      cfg->poc = poc;

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(cfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%lf", &(cfg->QpFactor));
      // sqrt(QpFactor) is used in calculating lambda
      cfg->QpFactor = sqrt(cfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(cfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &num_ref_pics);
      if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES)
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }

      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp((const OMX_STRING)line, "L", 1) == 0) || (strncmp((const OMX_STRING)line, "l", 1) == 0))
          {
              sscanf((const OMX_STRING)line, "%c%d", &type, &(cfg->refPics[i].ref_pic));
              cfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(cfg->refPics[i].ref_pic - 1);
          }
          else
          {
              sscanf((const OMX_STRING)line, "%d", &(cfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf((const OMX_STRING)line, "%u", &(cfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      cfg->numRefPics = num_ref_pics;
  }

  return 0;
}



int ParseGopConfigFile (int gopSize, OMX_STRING fname, VCEncGopConfig *gopCfg)
{
#define MAX_LINE_LENGTH 1024
  int frame_idx = 0, line_idx = 0, addTmp;
  OMX_S8 achParserBuffer[MAX_LINE_LENGTH];
  FILE *fIn = fopen (fname, "r");
  if (fIn == NULL)
  {
    printf("GOP Config: Error, Can Not Open File %s\n", fname );
    return -1;
  }

  while ( 0 == feof(fIn))
  {
    if (feof (fIn)) break;
    line_idx ++;
    achParserBuffer[0] = '\0';
    // Read one line
    OMX_STRING line = (OMX_STRING)fgets ((OMX_STRING) achParserBuffer, MAX_LINE_LENGTH, fIn);
    if (!line) break;
    //handle line end
    OMX_STRING s = strpbrk((const OMX_STRING)line, "#\n");
    if(s) *s = '\0';

    addTmp = 1;
    line = strstr((const OMX_STRING)line, "Frame");
    if (line)
    {
      if( 0 == strncmp((const OMX_STRING)line, "Frame0", 6))
          addTmp = 0;

      if (ParseGopConfigString(line, gopCfg, frame_idx, gopSize) < 0)
      {
          printf("Invalid gop configure!\n");
          return -1;
      }

      frame_idx += addTmp;
    }
  }

  fclose(fIn);
  if (frame_idx != gopSize)
  {
    printf ("GOP Config: Error, Parsing File %s Failed at Line %d\n", fname, line_idx);
    return -1;
  }
  return 0;
}


int ReadGopConfig (OMX_STRING fname, OMX_STRING*config, VCEncGopConfig *gopCfg, int gopSize)
{
  int ret = -1;

  if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
    return -1;

  if(gopSize > MAX_GOP_SIZE)
    return -1;

  gopCfg->gopCfgOffset[gopSize] = gopCfg->size;
  if(fname)
  {
    ret = ParseGopConfigFile (gopSize, fname, gopCfg);
  }
  else if(config)
  {
    int id = 0;
    while (config[id])
    {
      ParseGopConfigString (config[id], gopCfg, id, gopSize);
      id ++;
    }
    ret = 0;
  }
  return ret;
}

static int _InitGopConfigs (OMX_U32 gopSize, ENCODER_CODEC *h, VCEncGopConfig *gopCfg, bool bPass2)
{
  OMX_U32 i, pre_load_num;
  OMX_STRING fname = h->options.gopCfg;
  OMX_STRING *default_configs[8] = {
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_1: (IS_H264(h->codecFormat) ? RpsDefault_H264_GOPSize_1 : RpsDefault_GOPSize_1),
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_2: RpsDefault_GOPSize_2,
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_3: RpsDefault_GOPSize_3,
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_4: RpsDefault_GOPSize_4,
              RpsDefault_GOPSize_5,
              RpsDefault_GOPSize_6,
              RpsDefault_GOPSize_7,
              RpsDefault_GOPSize_8 };

  if (gopSize > MAX_GOP_SIZE)
  {
    printf ("GOP Config: Error, Invalid GOP Size\n");
    return -1;
  }

  if (bPass2)
  {
    default_configs[1] = RpsPass2_GOPSize_2;
    default_configs[3] = RpsPass2_GOPSize_4;
    default_configs[7] = RpsPass2_GOPSize_8;
  }

  //Handle Interlace
  if (h->options.interlacedFrame && gopSize==1)
  {
    default_configs[0] = RpsDefault_Interlace_GOPSize_1;
  }

  // GOP size in rps array for gopSize=N
  // N<=4:      GOP1, ..., GOPN
  // 4<N<=8:   GOP1, GOP2, GOP3, GOP4, GOPN
  // N > 8:       GOP1, GOPN
  // Adaptive:  GOP1, GOP2, GOP3, GOP4, GOP6, GOP8
  if (gopSize > 8)
    pre_load_num = 1;
  else if (gopSize>=4 || gopSize==0)
    pre_load_num = 4;
  else
    pre_load_num = gopSize;

  gopCfg->special_size = 0;
  gopCfg->ltrcnt       = 0;

  for (i = 1; i <= pre_load_num; i ++)
  {    
    if (ReadGopConfig (gopSize==i ? fname : NULL, default_configs[i-1], gopCfg, i))
      return -1;
  }

  if (gopSize == 0)
  {
    //gop6
    if (ReadGopConfig (NULL, default_configs[5], gopCfg, 6))
      return -1;
    //gop8
    if (ReadGopConfig (NULL, default_configs[7], gopCfg, 8))
      return -1;
  }
  else if (gopSize > 4)
  {
    //gopSize
    if (ReadGopConfig (fname, default_configs[gopSize-1], gopCfg, gopSize))
      return -1;
  }

  if ((VSI_DEFAULT_VALUE != h->options.ltrInterval) && (gopCfg->special_size == 0))
  {
      if (gopSize != 1)
      {
          printf("GOP Config: Error, when using --LTR configure option, the gopsize alse should be set to 1!\n");
          return -1;
      }
      gopCfg->pGopPicSpecialCfg[0].poc = 0;
      gopCfg->pGopPicSpecialCfg[0].QpOffset = h->options.longTermQpDelta;
      gopCfg->pGopPicSpecialCfg[0].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].numRefPics = NUMREFPICS_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].i32Ltr = 1;
      gopCfg->pGopPicSpecialCfg[0].i32Offset = 0;
      gopCfg->pGopPicSpecialCfg[0].i32Interval = h->options.ltrInterval;
      gopCfg->pGopPicSpecialCfg[0].i32short_change = 0;
      gopCfg->u32LTR_idx[0]                    = LONG_TERM_REF_ID2DELTAPOC(0);

      gopCfg->pGopPicSpecialCfg[1].poc = 0;
      gopCfg->pGopPicSpecialCfg[1].QpOffset = QPOFFSET_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].numRefPics = 2;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].ref_pic     = -1;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].refPics[1].ref_pic     = LONG_TERM_REF_ID2DELTAPOC(0);
      gopCfg->pGopPicSpecialCfg[1].refPics[1].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].i32Ltr = 0;
      gopCfg->pGopPicSpecialCfg[1].i32Offset = h->options.longTermGapOffset;
      gopCfg->pGopPicSpecialCfg[1].i32Interval = h->options.longTermGap;
      gopCfg->pGopPicSpecialCfg[1].i32short_change = 0;

      gopCfg->special_size = 2;
      gopCfg->ltrcnt = 1;
  }

  if (0)
    for(i = 0; i < (gopSize == 0 ? gopCfg->size : gopCfg->gopCfgOffset[gopSize]); i++)
    {
      // when use long-term, change P to B in default configs (used for last gop)
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_PREDICTED_FRAME)
        cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    }

  //Compatible with old bFrameQpDelta setting
  if (h->options.bFrameQpDelta >= 0 && fname == NULL)
  {
    for (i = 0; i < gopCfg->size; i++)
    {
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_BIDIR_PREDICTED_FRAME)
        cfg->QpOffset = h->options.bFrameQpDelta;
    }
  }

  // lowDelay auto detection
  VCEncGopPicConfig *cfgStart = &(gopCfg->pGopPicCfg[gopCfg->gopCfgOffset[gopSize]]);
  if (gopSize == 1)
  {
    h->options.gopLowdelay = 1;
  }
  else if ((gopSize > 1) && (h->options.gopLowdelay == 0))
  {
     h->options.gopLowdelay = 1;
     for (i = 1; i < gopSize; i++)
     {
        if (cfgStart[i].poc < cfgStart[i-1].poc)
        {
          h->options.gopLowdelay = 0;
          break;
        }
     }
  }


  {
      i32 i32LtrPoc[VCENC_MAX_LT_REF_FRAMES];

      for (i = 0; i < VCENC_MAX_LT_REF_FRAMES; i++)
          i32LtrPoc[i] = -1;
      for (i = 0; i < gopCfg->special_size; i++)
      {
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > VCENC_MAX_LT_REF_FRAMES)
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > 0)
              i32LtrPoc[i] = gopCfg->pGopPicSpecialCfg[i].i32Ltr - 1;
      }

      for (i = 0; i < gopCfg->ltrcnt; i++)
      {
          if ((0 != i32LtrPoc[0]) || (-1 == i32LtrPoc[i]) || ((i>0) && i32LtrPoc[i] != (i32LtrPoc[i - 1] + 1)))
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
      }
  }

  //For lowDelay, Handle the first few frames that miss reference frame
  if (1)
  {
    int nGop;
    int idx = 0;
    int maxErrFrame = 0;
    VCEncGopPicConfig *cfg;

    // Find the max frame number that will miss its reference frame defined in rps    
    while ((idx - maxErrFrame) < (int)gopSize)
    {
      nGop = (idx / gopSize) * gopSize;
      cfg = &(cfgStart[idx % gopSize]);

      for (i = 0; i < cfg->numRefPics; i ++)
      {
        //POC of this reference frame
        int refPoc = cfg->refPics[i].ref_pic + cfg->poc + nGop;
        if (refPoc < 0)
        {
          maxErrFrame = idx + 1;
        }
      }
      idx ++;
    }

    // Try to config a new rps for each "error" frame by modifying its original rps 
    for (idx = 0; idx < maxErrFrame; idx ++)
    {
      int j, iRef, nRefsUsedByCur, nPoc;
      VCEncGopPicConfig *cfgCopy;

      if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
        break;

      // Add to array end
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size]);
      cfgCopy = &(cfgStart[idx % gopSize]);
      memcpy (cfg, cfgCopy, sizeof (VCEncGopPicConfig));
      gopCfg->size ++;

      // Copy reference pictures
      nRefsUsedByCur = iRef = 0;
      nPoc = cfgCopy->poc + ((idx / gopSize) * gopSize);
      for (i = 0; i < cfgCopy->numRefPics; i ++)
      {
        int newRef = 1;
        int used_by_cur = cfgCopy->refPics[i].used_by_cur;
        int ref_pic = cfgCopy->refPics[i].ref_pic;
        // Clip the reference POC
        if ((cfgCopy->refPics[i].ref_pic + nPoc) < 0)
          ref_pic = 0 - (nPoc);

        // Check if already have this reference
        for (j = 0; j < iRef; j ++)
        {
          if (cfg->refPics[j].ref_pic == ref_pic)
          {
             newRef = 0;
             if (used_by_cur)
               cfg->refPics[j].used_by_cur = used_by_cur;
             break;
          }
        }

        // Copy this reference        
        if (newRef)
        {          
          cfg->refPics[iRef].ref_pic = ref_pic;
          cfg->refPics[iRef].used_by_cur = used_by_cur;
          iRef ++;
        }
      }
      cfg->numRefPics = iRef;
      // If only one reference frame, set P type.
      for (i = 0; i < cfg->numRefPics; i ++)
      {
        if (cfg->refPics[i].used_by_cur)
          nRefsUsedByCur ++;
      }
      if (nRefsUsedByCur == 1)
        cfg->codingType = VCENC_PREDICTED_FRAME;
    }
  }

#if 0
      //print for debug
      int idx;
      printf ("====== REF PICTURE SETS from %s ======\n",fname ? fname : "VSI_DEFAULT_VALUE");
      for (idx = 0; idx < gopCfg->size; idx ++)
      {
        int i;
        VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[idx]);
        OMX_S8 type = cfg->codingType==VCENC_PREDICTED_FRAME ? 'P' : cfg->codingType == VCENC_INTRA_FRAME ? 'I' : 'B';
        printf (" FRAME%2d:  %c %d %d %f %d", idx, type, cfg->poc, cfg->QpOffset, cfg->QpFactor, cfg->numRefPics);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].ref_pic);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].used_by_cur);
        printf("\n");
      }
      printf ("===========================================\n");
#endif
  return 0;
}

static void vc8000e_init_pic(ENCODER_CODEC *h)
{
//    ma_s *ma = &h->ma;
    adapGopCtr *agop = &h->agop;
    h->validencodedframenumber=0;

    //Adaptive Gop variables
    agop->last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
    agop->gop_frm_num = 0;
    agop->sum_intra_vs_interskip = 0;
    agop->sum_skip_vs_interskip = 0;
    agop->sum_intra_vs_interskipP = 0;
    agop->sum_intra_vs_interskipB = 0;
    agop->sum_costP = 0;
    agop->sum_costB = 0;

#if 0
    ma->pos = ma->count = 0;
    ma->frameRateNumer = cml->outputRateNumer;
    ma->frameRateDenom = cml->outputRateDenom;
    if (cml->outputRateDenom)
        ma->length = MAX(LEAST_MONITOR_FRAME, MIN(cml->monitorFrames,
                MOVING_AVERAGE_FRAMES));
    else
        ma->length = MOVING_AVERAGE_FRAMES;
#endif
}

void VCEncGetAdaptiveGopParams(VCEncInst inst, u32 *lookaheadDepth, u32 *uiPBFrameCost, u32 *uiSkipCu8Num, u32 *uiIntraCu8Num);

i32 AdaptiveGopDecision(ENCODER_CODEC *h)
{
    i32 nextGopSize =-1;
    adapGopCtr * agop = &h->agop;
    VCEncIn *pEncIn = &h->encIn;

    double dIntraVsInterskip = (double)h->encOut.cuStatis.intraCu8Num/(double)((h->width/8) * (h->height/8));
    double dSkipVsInterskip = (double)h->encOut.cuStatis.skipCu8Num/(double)((h->width/8) * (h->height/8));
    
    agop->gop_frm_num++;
    agop->sum_intra_vs_interskip += dIntraVsInterskip;
    agop->sum_skip_vs_interskip += dSkipVsInterskip;
    agop->sum_costP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? h->encOut.cuStatis.PBFrame4NRdCost:0;
    agop->sum_costB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? h->encOut.cuStatis.PBFrame4NRdCost:0;
    agop->sum_intra_vs_interskipP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? dIntraVsInterskip:0;
    agop->sum_intra_vs_interskipB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? dIntraVsInterskip:0; 
    
    if(pEncIn->gopPicIdx == pEncIn->gopSize-1)//last frame of the current gop. decide the gopsize of next gop.
    {
        dIntraVsInterskip = agop->sum_intra_vs_interskip/agop->gop_frm_num;
        dSkipVsInterskip = agop->sum_skip_vs_interskip/agop->gop_frm_num;
        agop->sum_costB = (agop->gop_frm_num>1)?(agop->sum_costB/(agop->gop_frm_num-1)):0xFFFFFFF;
        agop->sum_intra_vs_interskipB = (agop->gop_frm_num>1)?(agop->sum_intra_vs_interskipB/(agop->gop_frm_num-1)):0xFFFFFFF;
        //Enabled adaptive GOP size for large resolution
        if (((h->width * h->height) >= (1280 * 720)) || ((MAX_ADAPTIVE_GOP_SIZE >3)&&((h->width * h->height) >= (416 * 240))))
        {
            if ((((double)agop->sum_costP/(double)agop->sum_costB)<1.1)&&(dSkipVsInterskip >= 0.95))
            {
                agop->last_gopsize = nextGopSize = 1;
            }
            else if (((double)agop->sum_costP/(double)agop->sum_costB)>5)
            {
             nextGopSize = agop->last_gopsize;
            }
            else
            {
                if( ((agop->sum_intra_vs_interskipP > 0.40) && (agop->sum_intra_vs_interskipP < 0.70)&& (agop->sum_intra_vs_interskipB < 0.10)) )
                {
                    agop->last_gopsize++;
                    if(agop->last_gopsize==5 || agop->last_gopsize==7)  
                    {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                }
                else if (dIntraVsInterskip >= 0.30)
                {
                    agop->last_gopsize = nextGopSize = 1; //No B
                }
                else if (dIntraVsInterskip >= 0.20)
                {
                    agop->last_gopsize = nextGopSize = 2; //One B
                }
                else if (dIntraVsInterskip >= 0.10)
                {
                    agop->last_gopsize--;
                    if(agop->last_gopsize == 5 || agop->last_gopsize==7) 
                    {
                        agop->last_gopsize--;
                    }
                    agop->last_gopsize = MAX(agop->last_gopsize, 3);
                    nextGopSize = agop->last_gopsize; //
                }
                else
                {
                    agop->last_gopsize++;
                    if(agop->last_gopsize==5 || agop->last_gopsize==7)  
                    {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                }
            }
        }
        else
        {
            nextGopSize = 3;
        }
        agop->gop_frm_num = 0;
        agop->sum_intra_vs_interskip = 0;
        agop->sum_skip_vs_interskip = 0;
        agop->sum_costP = 0;
        agop->sum_costB = 0;
        agop->sum_intra_vs_interskipP = 0;
        agop->sum_intra_vs_interskipB = 0;

        nextGopSize = MIN(nextGopSize, MAX_ADAPTIVE_GOP_SIZE);
    }

    if(nextGopSize != -1)
        h->nextGopSize = nextGopSize;
    
    return nextGopSize;
}

i32 getNextGopSize(ENCODER_CODEC *h)
{
    VCEncIn *pEncIn = &h->encIn;

    if (h->options.lookaheadDepth)
    {
        i32 updGop = VCEncGetPass1UpdatedGopSize(h->instance);
        if (updGop)
            h->nextGopSize = updGop;
    }
    else if (pEncIn->codingType != VCENC_INTRA_FRAME)
        AdaptiveGopDecision(h);

    return h->nextGopSize;
}

#if 0
/*------------------------------------------------------------------------------
    Add new frame bits for moving average bitrate calculation
------------------------------------------------------------------------------*/
void MaAddFrame(ma_s *ma, i32 frameSizeBits)
{
    ma->frame[ma->pos++] = frameSizeBits;

    if (ma->pos == ma->length)
        ma->pos = 0;

    if (ma->count < ma->length)
        ma->count++;
}

/*------------------------------------------------------------------------------
    Calculate average bitrate of moving window
------------------------------------------------------------------------------*/
i32 Ma(ma_s *ma)
{
    i32 i;
    unsigned long long sum = 0;     /* Using 64-bits to avoid overflow */

    for (i = 0; i < ma->count; i++)
        sum += ma->frame[i];

    if (!ma->frameRateDenom)
        return 0;

    sum = sum / ma->count;

    return sum * (ma->frameRateNumer+ma->frameRateDenom-1) / ma->frameRateDenom;
}
#endif

/*------------------------------------------------------------------------------
Function name : InitPicConfig
Description   : initial pic reference configure
Return type   : void
Argument      : VCEncIn *pEncIn
------------------------------------------------------------------------------*/
void InitPicConfig(ENCODER_CODEC *h)
{
    i32 i, j, k, i32Poc;
    i32 i32MaxpicOrderCntLsb = 1 << 16;
    VCEncIn *pEncIn = &h->encIn;

    pEncIn->gopCurrPicConfig.codingType = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.numRefPics = NUMREFPICS_RESERVED;
    pEncIn->gopCurrPicConfig.poc = -1;
    pEncIn->gopCurrPicConfig.QpFactor = QPFACTOR_RESERVED;
    pEncIn->gopCurrPicConfig.QpOffset = QPOFFSET_RESERVED;
    pEncIn->gopCurrPicConfig.temporalId = 0; //TEMPORALID_RESERVED;
    pEncIn->i8SpecialRpsIdx = -1;
    for (k = 0; k < VCENC_MAX_REF_FRAMES; k++)
    {
        pEncIn->gopCurrPicConfig.refPics[k].ref_pic     = INVALITED_POC;
        pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = 0;
    }

    for (k = 0; k < VCENC_MAX_LT_REF_FRAMES; k++)
        pEncIn->long_term_ref_pic[k] = INVALITED_POC;

    pEncIn->bIsPeriodUsingLTR = HANTRO_FALSE;
    pEncIn->bIsPeriodUpdateLTR = HANTRO_FALSE;

    for (i = 0; i < pEncIn->gopConfig.special_size; i++)
    {
        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Interval <= 0)
            continue;

        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr == 0)
            pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
        else
        {
            pEncIn->bIsPeriodUpdateLTR = HANTRO_TRUE;
            
            for (k = 0; k < (i32)pEncIn->gopConfig.pGopPicSpecialCfg[i].numRefPics; k++)
            {
                i32 i32LTRIdx = pEncIn->gopConfig.pGopPicSpecialCfg[i].refPics[k].ref_pic;
                if ((IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) && ((pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr-1) == LONG_TERM_REF_DELTAPOC2ID(i32LTRIdx)))
                {
                    pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
                }
            }
        }
    }

    memset(pEncIn->bLTR_need_update, 0, sizeof(bool)*VCENC_MAX_LT_REF_FRAMES);
    pEncIn->bIsIDR       = HANTRO_TRUE;

    i32Poc = 0;
    /* check current picture encoded as LTR*/
    pEncIn->u8IdxEncodedAsLTR = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++)
    {
        if (pEncIn->bIsPeriodUsingLTR == HANTRO_FALSE)
            break;

        if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) || (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
            continue;

        i32Poc = i32Poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

        if (i32Poc < 0)
        {
            i32Poc += i32MaxpicOrderCntLsb;
            if (i32Poc >(i32MaxpicOrderCntLsb >> 1))
                i32Poc = -1;
        }

        if ((i32Poc >= 0) && (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
        {
            /* more than one LTR at the same frame position */
            if (0 != pEncIn->u8IdxEncodedAsLTR)
            {
                // reuse the same POC LTR
                pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1] = HANTRO_TRUE;
                continue;
            }

            pEncIn->gopCurrPicConfig.codingType = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) ? pEncIn->gopCurrPicConfig.codingType : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
            pEncIn->gopCurrPicConfig.numRefPics = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED) ? pEncIn->gopCurrPicConfig.numRefPics : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
            pEncIn->gopCurrPicConfig.QpFactor = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) ? pEncIn->gopCurrPicConfig.QpFactor : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
            pEncIn->gopCurrPicConfig.QpOffset = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED) ? pEncIn->gopCurrPicConfig.QpOffset : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
            pEncIn->gopCurrPicConfig.temporalId = (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED) ? pEncIn->gopCurrPicConfig.temporalId : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

            if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics != NUMREFPICS_RESERVED))
            {
                for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++)
                {
                    pEncIn->gopCurrPicConfig.refPics[k].ref_pic = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
                    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
                }
            }

            pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
            pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
        }
    }
    u32 gopSize = h->options.gopSize;
    bool adaptiveGop = (gopSize == 0);

    pEncIn->timeIncrement = 0;
    pEncIn->vui_timing_info_enable = h->options.vui_timing_info_enable;
    pEncIn->hashType = h->options.hashtype;
    pEncIn->poc = 0;
    //default gop size as IPPP
    pEncIn->gopSize =  (adaptiveGop ? (h->options.lookaheadDepth ? 4 : 1) : gopSize);
    pEncIn->last_idr_picture_cnt = pEncIn->picture_cnt = 0;
}

//translate OMX profile to VC8000e's profile
static int vc8000e_parse_hevc_profile(OMX_VIDEO_HEVCPROFILETYPE omx_profile, VCEncProfile *vc_profile)
{
    switch(omx_profile)
    {
        case OMX_VIDEO_HEVCProfileMain:
            *vc_profile = VCENC_HEVC_MAIN_PROFILE;
            break;
        case OMX_VIDEO_HEVCProfileMain10:
            *vc_profile = VCENC_HEVC_MAIN_10_PROFILE;
            break;
        case OMX_VIDEO_HEVCProfileMainStillPicture:
            *vc_profile = VCENC_HEVC_MAIN_STILL_PICTURE_PROFILE;
            break;
        default:
            DBGT_CRITICAL("Profile not supported (requested profile: %d)", omx_profile);
            DBGT_EPILOG("");
            return -1;
    }

    return 0;
}

//translate OMX level to VC8000e's level
static int vc8000e_parse_hevc_level(OMX_VIDEO_HEVCLEVELTYPE omx_level, VCEncLevel *vc_level)
{
    switch (omx_level)
    {
        case OMX_VIDEO_HEVCLevel1:
            *vc_level = VCENC_HEVC_LEVEL_1;
            break;
        case OMX_VIDEO_HEVCLevel2:
            *vc_level = VCENC_HEVC_LEVEL_2;
            break;
        case OMX_VIDEO_HEVCLevel21:
            *vc_level = VCENC_HEVC_LEVEL_2_1;
            break;
        case OMX_VIDEO_HEVCLevel3:
            *vc_level = VCENC_HEVC_LEVEL_3;
            break;
        case OMX_VIDEO_HEVCLevel31:
            *vc_level = VCENC_HEVC_LEVEL_3_1;
            break;
        case OMX_VIDEO_HEVCLevel4:
            *vc_level = VCENC_HEVC_LEVEL_4;
            break;
        case OMX_VIDEO_HEVCLevel41:
            *vc_level = VCENC_HEVC_LEVEL_4_1;
            break;
        case OMX_VIDEO_HEVCLevel5:
            *vc_level = VCENC_HEVC_LEVEL_5;
            break;
        case OMX_VIDEO_HEVCLevel51:
            *vc_level = VCENC_HEVC_LEVEL_5_1;
            break;
        case OMX_VIDEO_HEVCLevel52:
            *vc_level = VCENC_HEVC_LEVEL_5_2;
            break;
        case OMX_VIDEO_HEVCLevel6:
            *vc_level = VCENC_HEVC_LEVEL_6;
            break;
        case OMX_VIDEO_HEVCLevel61:
            *vc_level = VCENC_HEVC_LEVEL_6_1;
            break;
        case OMX_VIDEO_HEVCLevel62:
        case OMX_VIDEO_HEVCLevelMax:
            *vc_level = VCENC_HEVC_LEVEL_6_2;
            break;
        default:
            DBGT_CRITICAL("Unsupported encoding level %d", omx_level);
            DBGT_EPILOG("");
            return -1;
    }

    return 0;
}

//translate OMX profile to VC8000e's profile
static int vc8000e_parse_avc_profile(OMX_VIDEO_AVCPROFILETYPE omx_profile, VCEncProfile *vc_profile)
{
    switch(omx_profile)
    {
        case OMX_VIDEO_AVCProfileBaseline:
            *vc_profile = VCENC_H264_BASE_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            *vc_profile = VCENC_H264_MAIN_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            *vc_profile = VCENC_H264_HIGH_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh10:
            *vc_profile = VCENC_H264_HIGH_10_PROFILE;
            break;
       default:
            DBGT_CRITICAL("Profile not supported (requested profile: %d)", omx_profile);
            DBGT_EPILOG("");
            return -1;
    }

    return 0;
}

//translate OMX level to VC8000e's level
static int vc8000e_parse_avc_level(OMX_VIDEO_AVCLEVELTYPE omx_level, VCEncLevel *vc_level)
{
    if(omx_level <= OMX_VIDEO_AVCLevel51)
    {
        switch (omx_level)
        {
            case OMX_VIDEO_AVCLevel1:
                *vc_level = VCENC_H264_LEVEL_1;
                break;
            case OMX_VIDEO_AVCLevel1b:
                *vc_level = VCENC_H264_LEVEL_1_b;
                break;
            case OMX_VIDEO_AVCLevel11:
                *vc_level = VCENC_H264_LEVEL_1_1;
                break;
            case OMX_VIDEO_AVCLevel12:
                *vc_level = VCENC_H264_LEVEL_1_2;
                break;
            case OMX_VIDEO_AVCLevel13:
                *vc_level = VCENC_H264_LEVEL_1_3;
                break;
            case OMX_VIDEO_AVCLevel2:
                *vc_level = VCENC_H264_LEVEL_2;
                break;
            case OMX_VIDEO_AVCLevel21:
                *vc_level = VCENC_H264_LEVEL_2_1;
                break;
            case OMX_VIDEO_AVCLevel22:
                *vc_level = VCENC_H264_LEVEL_2_2;
                break;
            case OMX_VIDEO_AVCLevel3:
                *vc_level = VCENC_H264_LEVEL_3;
                break;
            case OMX_VIDEO_AVCLevel31:
                *vc_level = VCENC_H264_LEVEL_3_1;
                break;
            case OMX_VIDEO_AVCLevel32:
                *vc_level = VCENC_H264_LEVEL_3_2;
                break;
            case OMX_VIDEO_AVCLevel4:
                *vc_level = VCENC_H264_LEVEL_4;
                break;
            case OMX_VIDEO_AVCLevel41:
                *vc_level = VCENC_H264_LEVEL_4_1;
                break;
            case OMX_VIDEO_AVCLevel42:
                *vc_level = VCENC_H264_LEVEL_4_2;
                break;
            case OMX_VIDEO_AVCLevel5:
                *vc_level = VCENC_H264_LEVEL_5;
                break;
            case OMX_VIDEO_AVCLevel51:
                *vc_level = VCENC_H264_LEVEL_5_1;
                break;
            default:
                DBGT_CRITICAL("Unsupported encoding level %d", omx_level);
                DBGT_EPILOG("");
                return -1;
                break;
        }        
    }
    else
    {
        
        switch ((OMX_VIDEO_AVCEXTLEVELTYPE)omx_level)
        {
               
            case OMX_VIDEO_AVCLevel52:
                *vc_level = VCENC_H264_LEVEL_5_2;
                break;
            case OMX_VIDEO_AVCLevel60:
                *vc_level = VCENC_H264_LEVEL_6;
                break;
            case OMX_VIDEO_AVCLevel61:
                *vc_level = VCENC_H264_LEVEL_6_1;
                break;
            case OMX_VIDEO_AVCLevel62:
                *vc_level = VCENC_H264_LEVEL_6_2;
                break;
                
            default:
                DBGT_CRITICAL("Unsupported encoding level %d", omx_level);
                DBGT_EPILOG("");
                return -1;
                break;
        }
    }

    return 0;
}


void Parameter_Preset(VC8000E_OPTIONS *opts)
{
    /************************************************
      Preset: H264    Preset: HEVC      LA=40   rdoLevel    RDOQ
      N/A                 4                          Y           3                Y
      N/A                 3                          Y           3                N
      N/A                 2                          Y           2                N
      1                      1                          Y           1                N
      0                      0                          N           1                N
    *************************************************/
    if(opts->preset >=4)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 3;
      opts->enableRdoQuant = 1;
    }
    else if(opts->preset ==3)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 3;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==2)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 2;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==1)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 1;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==0)
    {
      opts->lookaheadDepth = 0;
      opts->rdoLevel = 1;
      opts->enableRdoQuant = 0;
    }
}

// set configurations to instance
static int vc8000e_parse_params(ENCODER_CODEC *h, const CODEC_CONFIG* params)
{
    VC8000E_OPTIONS *opts = &h->options;

    h->width = params->common_config.nOutputWidth;
    h->height = params->common_config.nOutputHeight;

    h->origWidth = params->pp_config.origWidth;
    h->origHeight = params->pp_config.origHeight;

    opts->frameRateNum = TIME_RESOLUTION;
//    opts->frameRateDenom = opts->frameRateNum / Q16_FLOAT(params->common_config.nInputFramerate);
    opts->frameRateDenom = opts->frameRateNum / Q16_FLOAT(params->common_config.nOutputFramerate);
    opts->inputRateNumer = Q16_FLOAT(params->common_config.nInputFramerate);

    opts->nBitDepthLuma = params->common_config.nBitDepthLuma;
    opts->nBitDepthChroma = params->common_config.nBitDepthChroma;

    opts->nMaxTLayers = params->common_config.nMaxTLayers;
    opts->nTargetBitrate = params->rate_config.nTargetBitrate;
    opts->nQpDefault = params->rate_config.nQpDefault;

    opts->nQpMin = params->rate_config.nQpMin;
    opts->nQpMax = params->rate_config.nQpMax;
    opts->nPictureRcEnabled = params->rate_config.nPictureRcEnabled;
    opts->nHrdEnabled = params->rate_config.nHrdEnabled;
    opts->eRateControl = params->rate_config.eRateControl;

    opts->xOffset = params->pp_config.xOffset;
    opts->yOffset = params->pp_config.yOffset;
    opts->formatType = params->pp_config.formatType;
    opts->angle = params->pp_config.angle;
    opts->gopCfg = NULL;

    opts->bDisableDeblocking = params->bDisableDeblocking;
    opts->bSeiMessages = params->bSeiMessages;
    opts->nVideoFullRange = params->nVideoFullRange;

    h->bStabilize = 0;//params->pp_config.frameStabilization;
    h->nIFrameCounter = 0;
    h->nEstTimeInc = opts->frameRateDenom;
    memset(h->gopPicCfg, 0, sizeof(h->gopPicCfg));
    memset(h->gopPicSpecialCfg, 0, sizeof(h->gopPicSpecialCfg));


    if(params->codecFormat.nCodecFormat == OMX_VIDEO_CodingHEVC)
    {
        //hevc
        OMX_VIDEO_PARAM_HEVCTYPE *hevc_cfg = &(params->configs.hevc_config);
        if(vc8000e_parse_hevc_profile(hevc_cfg->eProfile, &h->profile))
        {
            return -1;
        }

        if(vc8000e_parse_hevc_level(hevc_cfg->eLevel, &h->level))
        {
            return -1;
        }

        h->codecFormat = VCENC_VIDEO_CODEC_HEVC;
        h->tier = VCENC_HEVC_MAIN_TIER;

        //private options parsing
        h->nPFrames = hevc_cfg->nPFrames;
        h->nBFrames = hevc_cfg->nBFrames;
        h->nRefFrames = hevc_cfg->nRefFrames;

        opts->streamType = (hevc_cfg->byteStream) ? VCENC_BYTE_STREAM : VCENC_NAL_UNIT_STREAM;
        opts->ssim = hevc_cfg->ssim;
        opts->rdoLevel = CLIP3(1, 3, hevc_cfg->rdoLevel) - 1;
        opts->exp_of_input_alignment = hevc_cfg->exp_of_input_alignment;

        opts->ctbRcMode = (hevc_cfg->ctbRc != VSI_DEFAULT_VALUE) ? hevc_cfg->ctbRc : 0;
        opts->cuInfoVersion = hevc_cfg->cuInfoVersion;

        opts->gopSize = MIN(hevc_cfg->gopSize, MAX_GOP_SIZE);
        opts->gdrDuration = hevc_cfg->gdrDuration;
        
        opts->nTcOffset = hevc_cfg->nTcOffset;
        opts->nBetaOffset = hevc_cfg->nBetaOffset;
        opts->bEnableDeblockOverride = hevc_cfg->bEnableDeblockOverride;
        opts->bDeblockOverride = hevc_cfg->bDeblockOverride;
        opts->bEnableSAO = hevc_cfg->bEnableSAO;
        opts->bEnableScalingList = hevc_cfg->bEnableScalingList;
        opts->bCabacInitFlag = hevc_cfg->bCabacInitFlag;
        opts->RoiQpDelta_ver = hevc_cfg->RoiQpDelta_ver;
        opts->roiMapDeltaQpEnable = hevc_cfg->roiMapDeltaQpEnable;
        opts->roiMapDeltaQpBlockUnit = hevc_cfg->roiMapDeltaQpBlockUnit;
        opts->intraPicRate = hevc_cfg->intraPicRate;

        opts->bStrongIntraSmoothing = hevc_cfg->bStrongIntraSmoothing;

        //TBD, need to set value with parameters
        opts->interlacedFrame = 0;
        opts->enableOutputCuInfo = 0;
        opts->exp_of_ref_alignment = 0;
        opts->exp_of_ref_ch_alignment = 0;
        opts->P010RefEnable = 0;
        opts->picOrderCntType = 0;
        opts->log2MaxPicOrderCntLsb = 16;
        opts->log2MaxFrameNum = 12;
        opts->dumpRegister = 0;
        opts->rasterscan = 0;
        opts->parallelCoreNum = 1;
        opts->lookaheadDepth = 0;
        opts->bHalfDsInput = 0;

        opts->ltrInterval = hevc_cfg->ltrInterval;
        opts->longTermQpDelta = hevc_cfg->longTermQpDelta;
        opts->longTermGap = hevc_cfg->longTermGap;
        opts->longTermGapOffset = hevc_cfg->longTermGapOffset;

        opts->bFrameQpDelta = -1;

        opts->sliceSize = hevc_cfg->sliceSize;
        opts->max_cu_size = (h->codecFormat == VCENC_VIDEO_CODEC_HEVC) ? 64 : 16;

        opts->enableCabac = hevc_cfg->enableCabac;
        opts->bCabacInitFlag = hevc_cfg->bCabacInitFlag;

        opts->videoRange = hevc_cfg->videoRange;
        opts->enableRdoQuant = hevc_cfg->enableRdoQuant;
        opts->fieldOrder = hevc_cfg->fieldOrder;

        opts->cirStart = hevc_cfg->cirStart;
        opts->cirInterval = hevc_cfg->cirInterval;

        opts->pcm_loop_filter_disabled_flag = hevc_cfg->pcm_loop_filter_disabled_flag;
        opts->ipcmMapEnable = hevc_cfg->ipcmMapEnable;
        memcpy(opts->roiQp, hevc_cfg->roiQp, sizeof(opts->roiQp));
        opts->chromaQpOffset = hevc_cfg->chromaQpOffset;

        opts->noiseReductionEnable = hevc_cfg->noiseReductionEnable;
        opts->noiseLow = hevc_cfg->noiseLow;
        opts->noiseFirstFrameSigma = hevc_cfg->noiseFirstFrameSigma;

        opts->num_tile_columns = hevc_cfg->num_tile_columns;
        opts->num_tile_rows = hevc_cfg->num_tile_rows;
        opts->loop_filter_across_tiles_enabled_flag = hevc_cfg->loop_filter_across_tiles_enabled_flag;
        opts->tiles_enabled_flag = (opts->num_tile_columns * opts->num_tile_rows) > 1;

        
        /* HDR10 */
        opts->hdr10_display_enable = hevc_cfg->hdr10_display_enable;
        opts->hdr10_dx0 = hevc_cfg->hdr10_dx0;
        opts->hdr10_dy0 = hevc_cfg->hdr10_dy0;
        opts->hdr10_dx1 = hevc_cfg->hdr10_dx1;
        opts->hdr10_dy1 = hevc_cfg->hdr10_dy1;
        opts->hdr10_dx2 = hevc_cfg->hdr10_dx2;
        opts->hdr10_dy2 = hevc_cfg->hdr10_dy2;
        opts->hdr10_wx = hevc_cfg->hdr10_wx;
        opts->hdr10_wy = hevc_cfg->hdr10_wy;
        opts->hdr10_maxluma = hevc_cfg->hdr10_maxluma;
        opts->hdr10_minluma = hevc_cfg->hdr10_minluma;
        opts->hdr10_lightlevel_enable = hevc_cfg->hdr10_lightlevel_enable;
        opts->hdr10_maxlight = hevc_cfg->hdr10_maxlight;
        opts->hdr10_avglight = hevc_cfg->hdr10_avglight;
        opts->hdr10_color_enable = hevc_cfg->hdr10_color_enable;
        opts->hdr10_primary = hevc_cfg->hdr10_primary;
        opts->hdr10_transfer = hevc_cfg->hdr10_transfer;
        opts->hdr10_matrix = hevc_cfg->hdr10_matrix;

        opts->RpsInSliceHeader = hevc_cfg->RpsInSliceHeader;

        opts->blockRCSize = hevc_cfg->blockRCSize;
        opts->rcQpDeltaRange = hevc_cfg->rcQpDeltaRange;
        opts->rcBaseMBComplexity = hevc_cfg->rcBaseMBComplexity;
        opts->picQpDeltaMin = hevc_cfg->picQpDeltaMin;
        opts->picQpDeltaMax = hevc_cfg->picQpDeltaMax;
        opts->bitVarRangeI = hevc_cfg->bitVarRangeI;
        opts->bitVarRangeP = hevc_cfg->bitVarRangeP;
        opts->bitVarRangeB = hevc_cfg->bitVarRangeB;
        opts->tolMovingBitRate = hevc_cfg->tolMovingBitRate;
        opts->tolCtbRcInter = hevc_cfg->tolCtbRcInter;
        opts->tolCtbRcIntra = hevc_cfg->tolCtbRcIntra;
        opts->ctbRowQpStep = hevc_cfg->ctbRowQpStep;

        if(hevc_cfg->monitorFrames != VSI_DEFAULT_VALUE)
        {
            opts->monitorFrames = hevc_cfg->monitorFrames;
        }
        else
        {
            opts->monitorFrames = (h->options.frameRateNum + h->options.frameRateDenom - 1) / h->options.frameRateDenom;
        }

        opts->bitrateWindow = hevc_cfg->bitrateWindow;
        opts->intraQpDelta = hevc_cfg->intraQpDelta;
        opts->fixedIntraQp = hevc_cfg->fixedIntraQp;
        opts->vbr = hevc_cfg->vbr;
        opts->smoothPsnrInGOP = hevc_cfg->smoothPsnrInGOP;
        opts->u32StaticSceneIbitPercent = hevc_cfg->staticSceneIbitPercent;

        opts->picSkip = hevc_cfg->picSkip;
        opts->vui_timing_info_enable = hevc_cfg->vui_timing_info_enable;
        opts->hashtype = hevc_cfg->hashtype;

        opts->firstPic = hevc_cfg->firstPic;
        opts->lastPic = hevc_cfg->lastPic;

        opts->mmuEnable = hevc_cfg->mmuEnable;
        opts->extSramLumHeightBwd = hevc_cfg->extSramLumHeightBwd;
        opts->extSramChrHeightBwd = hevc_cfg->extSramChrHeightBwd;
        opts->extSramLumHeightFwd = hevc_cfg->extSramLumHeightFwd;
        opts->extSramChrHeightFwd = hevc_cfg->extSramChrHeightFwd;
        opts->AXIAlignment = hevc_cfg->AXIAlignment;
        if(hevc_cfg->codedChromaIdc == 0)
            opts->codedChromaIdc = VCENC_CHROMA_IDC_400;
        else if(hevc_cfg->codedChromaIdc == 2)
            opts->codedChromaIdc = VCENC_CHROMA_IDC_422;
        else
            opts->codedChromaIdc = VCENC_CHROMA_IDC_420;
            
        opts->aq_mode = hevc_cfg->aq_mode;
        opts->aq_strength = Q16_FLOAT(hevc_cfg->aq_strength);
        opts->writeReconToDDR = hevc_cfg->writeReconToDDR;
        opts->TxTypeSearchEnable = hevc_cfg->TxTypeSearchEnable;
        opts->PsyFactor = hevc_cfg->PsyFactor;
        opts->meVertSearchRange = hevc_cfg->meVertSearchRange;
        opts->layerInRefIdcEnable = hevc_cfg->layerInRefIdcEnable;
        opts->crf = hevc_cfg->crf;
        opts->preset = hevc_cfg->preset;

        if(opts->preset != VSI_DEFAULT_VALUE)
        {
            Parameter_Preset(opts);
        }
        opts->hrdCpbSize = hevc_cfg->hrdCpbSize;
   }
    else if(params->codecFormat.nCodecFormat == OMX_VIDEO_CodingAVC)
    {
        //h264
        OMX_VIDEO_PARAM_AVCTYPE *avc_cfg = &(params->configs.avc_config);
        const OMX_VIDEO_PARAM_AVCEXTTYPE *avc_ext_cfg = &(params->avc_ext_config);

        if(vc8000e_parse_avc_profile(avc_cfg->eProfile, &h->profile))
        {
            return -1;
        }

        if(vc8000e_parse_avc_level(avc_cfg->eLevel, &h->level))
        {
            return -1;
        }

        h->codecFormat = VCENC_VIDEO_CODEC_H264;
        h->tier = VCENC_HEVC_MAIN_TIER;

        //private options parsing
        h->nPFrames = avc_cfg->nPFrames;
        h->nBFrames = avc_cfg->nBFrames;
        h->nRefFrames = avc_cfg->nRefFrames;

        opts->streamType = VCENC_BYTE_STREAM;
        opts->ssim = 1;
        opts->rdoLevel = CLIP3(1, 3, avc_ext_cfg->rdoLevel) - 1;
        opts->exp_of_input_alignment = 0;

        opts->ctbRcMode = 0;
        opts->cuInfoVersion = -1;

        opts->gopSize = MIN(avc_ext_cfg->gopSize, MAX_GOP_SIZE);
        opts->gdrDuration = 0;
        
        opts->nTcOffset = -2;
        opts->nBetaOffset = 5;
        opts->bEnableDeblockOverride = OMX_FALSE;
        opts->bDeblockOverride = OMX_FALSE;
        opts->bEnableSAO = avc_cfg->bEnableASO;
        opts->bEnableScalingList = OMX_FALSE;
        opts->bCabacInitFlag = OMX_FALSE;
        opts->RoiQpDelta_ver = 1;
        opts->roiMapDeltaQpEnable = 0;
        opts->roiMapDeltaQpBlockUnit = 0;
        opts->intraPicRate = 0;
        opts->intraPicRate = avc_cfg->nPFrames;

        opts->bStrongIntraSmoothing = OMX_FALSE;

        //TBD, need to set value with parameters
        opts->interlacedFrame = 0;
        opts->enableOutputCuInfo = 0;
        opts->exp_of_ref_alignment = 0;
        opts->exp_of_ref_ch_alignment = 0;
        opts->P010RefEnable = 0;
        opts->picOrderCntType = 0;
        opts->log2MaxPicOrderCntLsb = 16;
        opts->log2MaxFrameNum = 12;
        opts->dumpRegister = 0;
        opts->rasterscan = 0;
        opts->parallelCoreNum = 1;
        opts->lookaheadDepth = 0;
        opts->bHalfDsInput = 0;

        opts->ltrInterval = VSI_DEFAULT_VALUE;
        opts->longTermQpDelta = 0;
        opts->longTermGap = 0;
        opts->longTermGapOffset = 0;

        opts->bFrameQpDelta = -1;

        opts->sliceSize = 0;
        opts->max_cu_size = 16;

        opts->enableCabac = avc_cfg->bEntropyCodingCABAC;
        opts->bCabacInitFlag = 0;

        opts->videoRange = 0;
        opts->enableRdoQuant = VSI_DEFAULT_VALUE;
        opts->fieldOrder = 0;

        opts->cirStart = 0;
        opts->cirInterval = 0;

        opts->pcm_loop_filter_disabled_flag = 0;
        opts->ipcmMapEnable = 0;
        {
            int i;
            for(i=0; i<8; i++)
            {
                opts->roiQp[i] = VSI_DEFAULT_VALUE;
            }
        }
        opts->chromaQpOffset = 0;

        opts->noiseReductionEnable = 0;
        opts->noiseLow = 10;
        opts->noiseFirstFrameSigma = 11;

        opts->num_tile_columns = 1;
        opts->num_tile_rows = 1;
        opts->loop_filter_across_tiles_enabled_flag = 1;
        opts->tiles_enabled_flag = (opts->num_tile_columns * opts->num_tile_rows) > 1;

        
        /* HDR10 */
        opts->hdr10_display_enable = 0;
        opts->hdr10_dx0 = 0;
        opts->hdr10_dy0 = 0;
        opts->hdr10_dx1 = 0;
        opts->hdr10_dy1 = 0;
        opts->hdr10_dx2 = 0;
        opts->hdr10_dy2 = 0;
        opts->hdr10_wx = 0;
        opts->hdr10_wy = 0;
        opts->hdr10_maxluma = 0;
        opts->hdr10_minluma = 0;
        opts->hdr10_lightlevel_enable = 0;
        opts->hdr10_maxlight = 0;
        opts->hdr10_avglight = 0;
        opts->hdr10_color_enable = 0;
        opts->hdr10_primary = 9;
        opts->hdr10_transfer = 0;
        opts->hdr10_matrix = 9;

        opts->RpsInSliceHeader = 0;

        opts->blockRCSize = VSI_DEFAULT_VALUE;
        opts->rcQpDeltaRange = VSI_DEFAULT_VALUE;
        opts->rcBaseMBComplexity = 15;
        opts->picQpDeltaMin = VSI_DEFAULT_VALUE;
        opts->picQpDeltaMax = VSI_DEFAULT_VALUE;
        opts->bitVarRangeI = 10000;
        opts->bitVarRangeP = 10000;
        opts->bitVarRangeB = 10000;
        opts->tolMovingBitRate = 2000;
        opts->tolCtbRcInter = VSI_DEFAULT_VALUE;
        opts->tolCtbRcIntra = VSI_DEFAULT_VALUE;
        opts->ctbRowQpStep = VSI_DEFAULT_VALUE;

        opts->monitorFrames = (h->options.frameRateNum + h->options.frameRateDenom - 1) / h->options.frameRateDenom;

        opts->bitrateWindow = VSI_DEFAULT_VALUE;
        opts->intraQpDelta = VSI_DEFAULT_VALUE;
        opts->fixedIntraQp = 0;
        opts->vbr = 0;
        opts->smoothPsnrInGOP = 0;
        opts->u32StaticSceneIbitPercent = 80;

        switch (opts->eRateControl)
        {
            case OMX_Video_ControlRateDisable:
                opts->picSkip = 0;
                break;
            case OMX_Video_ControlRateVariable:
                opts->picSkip = 0;
                break;
            case OMX_Video_ControlRateConstant:
                opts->picSkip = 0;
                break;
            case OMX_Video_ControlRateVariableSkipFrames:
                opts->picSkip = 1;
                break;
            case OMX_Video_ControlRateConstantSkipFrames:
                opts->picSkip = 1;
                break;
            case OMX_Video_ControlRateMax:
                opts->picSkip = 0;
                break;
            default:
                opts->picSkip = 0;
                break;
        }

        opts->vui_timing_info_enable = 1;
        opts->hashtype = 0;

        opts->firstPic = avc_ext_cfg->firstPic;
        opts->lastPic = avc_ext_cfg->lastPic;
        opts->hrdCpbSize = avc_ext_cfg->hrdCpbSize;

        opts->mmuEnable = avc_ext_cfg->mmuEnable;
        opts->extSramLumHeightBwd = avc_ext_cfg->extSramLumHeightBwd;
        opts->extSramChrHeightBwd = avc_ext_cfg->extSramChrHeightBwd;
        opts->extSramLumHeightFwd = avc_ext_cfg->extSramLumHeightFwd;
        opts->extSramChrHeightFwd = avc_ext_cfg->extSramChrHeightFwd;
        opts->AXIAlignment = avc_ext_cfg->AXIAlignment;
        if(avc_ext_cfg->codedChromaIdc == 0)
            opts->codedChromaIdc = VCENC_CHROMA_IDC_400;
        else if(avc_ext_cfg->codedChromaIdc == 2)
            opts->codedChromaIdc = VCENC_CHROMA_IDC_422;
        else
            opts->codedChromaIdc = VCENC_CHROMA_IDC_420;
        opts->aq_mode = avc_ext_cfg->aq_mode;
        opts->aq_strength = Q16_FLOAT(avc_ext_cfg->aq_strength);
        opts->writeReconToDDR = avc_ext_cfg->writeReconToDDR;
        opts->TxTypeSearchEnable = avc_ext_cfg->TxTypeSearchEnable;
        opts->PsyFactor = avc_ext_cfg->PsyFactor;
        opts->meVertSearchRange = avc_ext_cfg->meVertSearchRange;
        opts->layerInRefIdcEnable = avc_ext_cfg->layerInRefIdcEnable;
        opts->crf = avc_ext_cfg->crf;
        opts->preset = avc_ext_cfg->preset;

        if(opts->preset != VSI_DEFAULT_VALUE)
        {
            Parameter_Preset(opts);
        }
    }
    else if(params->codecFormat.nCodecFormat == OMX_VIDEO_CodingAV1)
    {
        //av1
        h->codecFormat = VCENC_VIDEO_CODEC_AV1;
        opts->streamType = VCENC_BYTE_STREAM;
    }

    return 0;
}

// init vc8000e gop configs
static int vc8000e_init_gopConfigs(ENCODER_CODEC *h)
{
    h->encIn.gopConfig.pGopPicCfg = h->gopPicCfg;

    h->encIn.gopConfig.pGopPicSpecialCfg = h->gopPicSpecialCfg;

    
    h->encIn.gopConfig.idr_interval = h->options.intraPicRate;
    h->encIn.gopConfig.gdrDuration = h->options.gdrDuration;
    h->encIn.gopConfig.firstPic = h->options.firstPic;
    h->encIn.gopConfig.lastPic = h->options.lastPic;
    h->encIn.gopConfig.outputRateNumer = TIME_RESOLUTION; //tb.outputRateNumer;      /* Output frame rate numerator */
    h->encIn.gopConfig.outputRateDenom = h->options.frameRateDenom; //tb.outputRateDenom;      /* Output frame rate denominator */
    h->encIn.gopConfig.inputRateNumer = h->options.inputRateNumer;      /* Input frame rate numerator */
    h->encIn.gopConfig.inputRateDenom = 1; //tb.inputRateDenom;      /* Input frame rate denominator */
    h->encIn.gopConfig.interlacedFrame = h->options.interlacedFrame;

    if (_InitGopConfigs(h->options.gopSize, h, &h->encIn.gopConfig, HANTRO_FALSE) != 0)
    {
        return -1;
    }

    if(h->options.lookaheadDepth) {
        memset (h->gopPicCfgPass2, 0, sizeof(h->gopPicCfgPass2));
        h->encIn.gopConfig.pGopPicCfg = h->gopPicCfgPass2;
        h->encIn.gopConfig.size = 0;
        memset(h->gopPicSpecialCfg, 0, sizeof(h->gopPicSpecialCfg));
        h->encIn.gopConfig.pGopPicSpecialCfg = h->gopPicSpecialCfg;
        if (_InitGopConfigs (h->options.gopSize, h, &h->encIn.gopConfig, HANTRO_TRUE) != 0)
        {
          return -1;
        }
        h->encIn.gopConfig.pGopPicCfgPass1 = h->gopPicCfg;
        h->encIn.gopConfig.pGopPicCfg = h->encIn.gopConfig.pGopPicCfgPass2 = h->gopPicCfgPass2;
    }

    h->encIn.gopConfig.gopLowdelay = h->options.gopLowdelay;

    if (h->options.intraPicRate != 1)
    {
        u32 maxRefPics = 0;
        i32 maxTemporalId = 0;
        int idx;
        for (idx = 0; idx < h->encIn.gopConfig.size; idx ++)
        {
            VCEncGopPicConfig *gop_cfg = &(h->encIn.gopConfig.pGopPicCfg[idx]);
            if (gop_cfg->codingType != VCENC_INTRA_FRAME)
            {
                if (maxRefPics < gop_cfg->numRefPics)
                    maxRefPics = gop_cfg->numRefPics;

                if (maxTemporalId < gop_cfg->temporalId)
                    maxTemporalId = gop_cfg->temporalId;
            }
            //TBD! Is it a potential bug for maxTemporalID???
        }

        h->maxRefPics = maxRefPics;
        h->maxTemporalId = maxTemporalId;
    }
    return 0;
}

// destroy codec instance
static void encoder_destroy_codec(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            VCEncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_codec(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    vc8000e_init_pic(this);
    InitPicConfig(this);

    //TBD: only 1 stream output supported
    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;
    this->encIn.vui_timing_info_enable = 1;
    this->nTotalFrames = 0;

    //this->encIn.gopConfig.pGopPicCfg = this->gopPicCfg;

    VCEncRet ret = VCEncStrmStart(this->instance, &this->encIn, &this->encOut);

    switch (ret)
    {
        case VCENC_OK:
            //TBD alen: need to consider NAL UNIT stream type.
            stream->streamlen = this->encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_ERROR:
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_stream_end_codec(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;

    //TBD alen: VCEncFlush should be called before VCEncStrmEnd
    VCEncRet ret = VCEncStrmEnd(this->instance, &this->encIn, &this->encOut);

    switch (ret)
    {
        case VCENC_OK:
            //TBD alen: need to consider NAL-UNIT stream type
            stream->streamlen = this->encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_encode_codec(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VIDEO_ENCODER_CONFIG* encConf = (VIDEO_ENCODER_CONFIG*) cfg;

    VCEncRet ret;
    VCEncRateCtrl rate_ctrl;
    VCEncCodingCtrl coding_ctrl;
    bool adaptiveGop = (this->options.gopSize == 0);

    //TBD: support 8-bit YUV 420 only, other formats??
    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth * this->origHeight);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth * this->origHeight / 4);

    this->encIn.timeIncrement = this->nEstTimeInc;

    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;
    
    /*this->encIn.gopSize = 1; // H2v1 supports only size 1 */
    this->encIn.gopConfig.pGopPicCfg = this->gopPicCfg;

    DBGT_PDEBUG("Input timeInc: %u outBufSize: %u, virtual address: %p, bus address: 0x%08lx",
        this->encIn.timeIncrement, this->encIn.outBufSize,
        this->encIn.pOutBuf, this->encIn.busOutBuf);

    // The bus address of the luminance component buffer of the picture to be stabilized.
    // Used only when video stabilization is enabled.
/*#ifdef USE_STAB_TEMP_INPUTBUFFER
    if (this->bStabilize && (frame->fb_bufferSize == (2 * frame->fb_frameSize))) {
        this->encIn.busLumaStab = frame->fb_bus_address + frame->fb_frameSize;
    }
#else
    if (this->bStabilize) {
        DBGT_ASSERT(frame->bus_lumaStab != 0);
        this->encIn.busLumaStab = frame->bus_lumaStab;
    }
#endif
    else
    {
        this->encIn.busLumaStab = 0;
    }*/

#if 0
    if (frame->frame_type == OMX_INTRA_FRAME)
    {
        this->encIn.codingType = VCENC_INTRA_FRAME;
        this->encIn.poc = 0;
    }
    else if (frame->frame_type == OMX_PREDICTED_FRAME)
    {
        if (this->nPFrames > 0)
        {
            if ((this->nIFrameCounter % (this->nPFrames+1)) == 0)
            {
                this->encIn.codingType = VCENC_INTRA_FRAME;
                this->nIFrameCounter = 0;
                this->encIn.poc = 0;
            }
            else
            {
                this->encIn.codingType = VCENC_PREDICTED_FRAME;
            }
            this->nIFrameCounter++;

        }
        else
        {
            this->encIn.codingType = VCENC_INTRA_FRAME;
            this->encIn.poc = 0;
        }
    }
    else
    {
        DBGT_CRITICAL("Unknown frame type");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSUPPORTED_SETTING;
    }

#else
    this->encIn.codingType = (this->encIn.poc == 0) ? VCENC_INTRA_FRAME : this->nextCodingType;
#endif

    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    //DBGT_PDEBUG("encIn.busLumaStab 0x%08x", (unsigned int)this->encIn.busLumaStab);
    DBGT_PDEBUG("Frame type %s, POC %d, IFrame counter (%d/%d)", (this->encIn.codingType == VCENC_INTRA_FRAME) ? "I":"P",
        (int)this->encIn.poc, (int)this->nIFrameCounter, (int)this->nPFrames+1);

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncrement = 0;
    }
    else
    {
        this->encIn.timeIncrement = this->options.frameRateDenom;
    }

    ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK && (this->currBitrate != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
        this->currBitrate = frame->bitrate;
    }

    DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
    DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);

    ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);

    if (ret == VCENC_OK)
    {
        int new_coding_ctrl = 0;
        if (coding_ctrl.intraArea.enable  != encConf->intraArea.bEnable ||
           ((coding_ctrl.intraArea.top     != encConf->intraArea.nTop ||
            coding_ctrl.intraArea.left    != encConf->intraArea.nLeft ||
            coding_ctrl.intraArea.bottom  != encConf->intraArea.nBottom ||
            coding_ctrl.intraArea.right   != encConf->intraArea.nRight) &&
            encConf->intraArea.bEnable))
        {
            coding_ctrl.intraArea.enable  = encConf->intraArea.bEnable;
            coding_ctrl.intraArea.top     = encConf->intraArea.nTop;
            coding_ctrl.intraArea.left    = encConf->intraArea.nLeft;
            coding_ctrl.intraArea.bottom  = encConf->intraArea.nBottom;
            coding_ctrl.intraArea.right   = encConf->intraArea.nRight;
            DBGT_PDEBUG("Intra area enable %d (%d, %d, %d, %d)",
                coding_ctrl.intraArea.enable, coding_ctrl.intraArea.top,
                coding_ctrl.intraArea.left, coding_ctrl.intraArea.bottom,
                coding_ctrl.intraArea.right);

            new_coding_ctrl = 1;
        }

        if (coding_ctrl.roi1Area.enable  != encConf->roiArea[0].bEnable ||
           ((coding_ctrl.roi1Area.top     != encConf->roiArea[0].nTop ||
            coding_ctrl.roi1Area.left    != encConf->roiArea[0].nLeft ||
            coding_ctrl.roi1Area.bottom  != encConf->roiArea[0].nBottom ||
            coding_ctrl.roi1Area.right   != encConf->roiArea[0].nRight ||
            coding_ctrl.roi1DeltaQp      != encConf->roiDeltaQP[0].nDeltaQP) &&
            encConf->roiArea[0].bEnable))
        {
            coding_ctrl.roi1Area.enable  = encConf->roiArea[0].bEnable;
            coding_ctrl.roi1Area.top     = encConf->roiArea[0].nTop;
            coding_ctrl.roi1Area.left    = encConf->roiArea[0].nLeft;
            coding_ctrl.roi1Area.bottom  = encConf->roiArea[0].nBottom;
            coding_ctrl.roi1Area.right   = encConf->roiArea[0].nRight;
            coding_ctrl.roi1DeltaQp      = encConf->roiDeltaQP[0].nDeltaQP;
            DBGT_PDEBUG("ROI area 1 enable %d (%d, %d, %d, %d)",
                coding_ctrl.roi1Area.enable, coding_ctrl.roi1Area.top,
                coding_ctrl.roi1Area.left, coding_ctrl.roi1Area.bottom,
                coding_ctrl.roi1Area.right);
            DBGT_PDEBUG("ROI 1 delta QP %d", coding_ctrl.roi1DeltaQp);

            new_coding_ctrl = 1;
        }

        if (coding_ctrl.roi2Area.enable  != encConf->roiArea[1].bEnable || 
           ((coding_ctrl.roi2Area.top     != encConf->roiArea[1].nTop ||
            coding_ctrl.roi2Area.left    != encConf->roiArea[1].nLeft ||
            coding_ctrl.roi2Area.bottom  != encConf->roiArea[1].nBottom ||
            coding_ctrl.roi2Area.right   != encConf->roiArea[1].nRight ||
            coding_ctrl.roi2DeltaQp      != encConf->roiDeltaQP[1].nDeltaQP) &&
            encConf->roiArea[1].bEnable))
        {
            coding_ctrl.roi2Area.enable  = encConf->roiArea[1].bEnable;
            coding_ctrl.roi2Area.top     = encConf->roiArea[1].nTop;
            coding_ctrl.roi2Area.left    = encConf->roiArea[1].nLeft;
            coding_ctrl.roi2Area.bottom  = encConf->roiArea[1].nBottom;
            coding_ctrl.roi2Area.right   = encConf->roiArea[1].nRight;
            coding_ctrl.roi2DeltaQp      = encConf->roiDeltaQP[1].nDeltaQP;
            DBGT_PDEBUG("ROI area 2 enable %d (%d, %d, %d, %d)",
                coding_ctrl.roi2Area.enable, coding_ctrl.roi2Area.top,
                coding_ctrl.roi2Area.left, coding_ctrl.roi2Area.bottom,
                coding_ctrl.roi2Area.right);
            DBGT_PDEBUG("ROI 2 delta QP %d", coding_ctrl.roi2DeltaQp);

            new_coding_ctrl = 1;
        }
        if(new_coding_ctrl)
            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
    }
    //TBD: ??
//    this->encIn.bSkipFrame = 0;

    ret = VCEncStrmEncode(this->instance, &this->encIn, &this->encOut, NULL, NULL);
    switch (ret)
    {
        case VCENC_FRAME_READY:
            this->nTotalFrames++;
//            this->encIn.poc++;
            if (this->encOut.streamSize == 0)
            {
              this->encIn.picture_cnt++;
			  stream->next_input_frame_id = this->encIn.picture_cnt;
              stat = CODEC_OK;
              break;
            }
            stream->streamlen = this->encOut.streamSize;
            if (adaptiveGop)
            {
                getNextGopSize(this);
            }
            else if(this->options.lookaheadDepth) // for sync only, not update gopSize
            {
                VCEncGetPass1UpdatedGopSize(this->instance);
            }

            this->nextCodingType = VCEncFindNextPic(this->instance, &this->encIn, this->nextGopSize, this->encIn.gopConfig.gopCfgOffset, false);
            stream->next_input_frame_id = this->encIn.picture_cnt;
            if (this->encOut.codingType == VCENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (this->encOut.codingType == VCENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_BIDIR_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_BIDIR_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_NOTCODED_FRAME)
            {
                DBGT_PDEBUG("encOut: Not coded frame");
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case VCENC_FRAME_ENQUEUE:
            this->nTotalFrames++;
            if (adaptiveGop && this->options.lookaheadDepth)
            {
                getNextGopSize(this);
            }
            else if(this->options.lookaheadDepth) // for sync only, not update gopSize
            {
                VCEncGetPass1UpdatedGopSize(this->instance);
            }
            this->nextCodingType = VCEncFindNextPic(this->instance, &this->encIn, this->nextGopSize, this->encIn.gopConfig.gopCfgOffset, false);
            stream->next_input_frame_id = this->encIn.picture_cnt;
            this->encIn.timeIncrement = this->options.frameRateDenom;
            stat = CODEC_ENQUEUE;
            break;

        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            this->encIn.picture_cnt++;
            break;
        case VCENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case VCENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case VCENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case VCENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case VCENC_HW_RESERVED:
            stat = CODEC_ERROR_RESERVED;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

// create codec instance and initialize it
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_codec(const CODEC_CONFIG* params)
{
    DBGT_PROLOG("");
    VCEncConfig cfg;
    //VCEncApiVersion apiVer;
    //VCEncBuild encBuild;
    u32 i;
    VCEncRet ret = VCENC_OK;
    ENCODER_CODEC* this = OSAL_Malloc(sizeof(ENCODER_CODEC));

    memset(this, 0, sizeof(ENCODER_CODEC));
    if(0 != vc8000e_parse_params(this, params))
    {
        DBGT_CRITICAL("Parameters Error!");
        DBGT_EPILOG("");
        return NULL;
    }

    if(0 != vc8000e_init_gopConfigs(this))
    {
        return NULL;
    }

    this->nIFrameCounter = 0;

    memset(&cfg, 0, sizeof(cfg));
    cfg.codecFormat = this->codecFormat;
    cfg.profile = this->profile;
    cfg.level = this->level;
    cfg.streamType = this->options.streamType;

    cfg.tier = this->tier;
//    cfg.refFrameAmount = (this->options.intraPicRate == 1) ? 0 : this->nRefFrames;
//    cfg.maxTLayers = this->options.nMaxTLayers;
    cfg.refFrameAmount = (this->options.intraPicRate == 1) ? 0 : (this->maxRefPics + this->options.interlacedFrame + this->encIn.gopConfig.ltrcnt);
    cfg.maxTLayers = (this->options.intraPicRate == 1) ? 1 : this->maxTemporalId + 1;

    cfg.strongIntraSmoothing = this->options.bStrongIntraSmoothing;

    cfg.width = this->width;
    cfg.height = this->height;
    
    cfg.frameRateNum = this->options.frameRateNum;
    cfg.frameRateDenom = this->options.frameRateDenom;
    cfg.bitDepthLuma = this->options.nBitDepthLuma;
    cfg.bitDepthChroma = this->options.nBitDepthChroma;

    cfg.enableSsim = this->options.ssim;
    cfg.rdoLevel = this->options.rdoLevel;
    cfg.exp_of_input_alignment = this->options.exp_of_input_alignment;
    cfg.ctbRcMode = this->options.ctbRcMode;

    cfg.bPass1AdaptiveGop = (this->options.gopSize == 0);
    cfg.cuInfoVersion = this->options.cuInfoVersion;

    //======== TBD beginning ============================
    //need to extend this parameter if necessary.
    cfg.compressor = 0;
    cfg.interlacedFrame = this->options.interlacedFrame;
    cfg.enableOutputCuInfo = this->options.enableOutputCuInfo;
    cfg.verbose = 0;
    cfg.exp_of_ref_alignment = this->options.exp_of_ref_alignment;
    cfg.exp_of_ref_ch_alignment = this->options.exp_of_ref_ch_alignment;
    cfg.exteralReconAlloc = 0;
    cfg.P010RefEnable = this->options.P010RefEnable;
    cfg.picOrderCntType = this->options.picOrderCntType;
    cfg.log2MaxPicOrderCntLsb = this->options.log2MaxPicOrderCntLsb;
    cfg.log2MaxFrameNum = this->options.log2MaxFrameNum;
    cfg.dumpRegister = this->options.dumpRegister;
    cfg.rasterscan = this->options.rasterscan;
    cfg.parallelCoreNum = this->options.parallelCoreNum;
    cfg.pass = this->options.lookaheadDepth ? 2 : 0;
    cfg.extDSRatio = (this->options.lookaheadDepth && this->options.bHalfDsInput) ? 1 : 0;
    cfg.lookaheadDepth = this->options.lookaheadDepth;
    //======== ending ============================

    // extension config for new version
    cfg.mmuEnable = this->options.mmuEnable;
    cfg.extSramLumHeightBwd = this->options.extSramLumHeightBwd;
    cfg.extSramChrHeightBwd = this->options.extSramChrHeightBwd;
    cfg.extSramLumHeightBwd = this->options.extSramLumHeightBwd;
    cfg.extSramLumHeightFwd = this->options.extSramLumHeightFwd;
    cfg.extSramChrHeightFwd = this->options.extSramChrHeightFwd;
    cfg.AXIAlignment = this->options.AXIAlignment;
    cfg.codedChromaIdc = this->options.codedChromaIdc;
    cfg.aq_mode = this->options.aq_mode;
    cfg.aq_strength = (double)this->options.aq_strength;
    cfg.writeReconToDDR = this->options.writeReconToDDR;
    cfg.TxTypeSearchEnable = this->options.TxTypeSearchEnable;
	//============================================

    VCEncApiVersion apiVer;
    VCEncBuild encBuild;

    DBGT_PDEBUG("Intra period %d", (int)this->nPFrames + 1);

    apiVer = VCEncGetApiVersion();
    
    DBGT_PDEBUG("VC Encoder API version %d.%d", apiVer.major, apiVer.minor);
            
    for (i = 0; i< API_EWLGetCoreNum(NULL); i++)
    {
        encBuild = VCEncGetBuild(i);
        DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
             encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
             encBuild.hwBuild, encBuild.swBuild / 1000000,
             (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
    }
    this->instance = NULL;
    ret = API_VCEncInit(&cfg, &this->instance, NULL);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    this->base.stream_start = encoder_stream_start_codec;
    this->base.stream_end = encoder_stream_end_codec;
    this->base.encode = encoder_encode_codec;
    this->base.destroy = encoder_destroy_codec;

    // Setup coding control
    VCEncCodingCtrl coding_ctrl;
    ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);
    if(ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetCodingCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (this->options.sliceSize != VSI_DEFAULT_VALUE)
      coding_ctrl.sliceSize = this->options.sliceSize;
    if (this->options.enableCabac != VSI_DEFAULT_VALUE)
      coding_ctrl.enableCabac = this->options.enableCabac;
    coding_ctrl.cabacInitFlag = this->options.bCabacInitFlag ? 1 : 0;
    coding_ctrl.videoFullRange = 0;
    if (this->options.videoRange != VSI_DEFAULT_VALUE)
      coding_ctrl.videoFullRange = this->options.videoRange;
    if (this->options.enableRdoQuant != VSI_DEFAULT_VALUE)
      coding_ctrl.enableRdoQuant = this->options.enableRdoQuant;

    coding_ctrl.videoFullRange = this->options.nVideoFullRange;
    coding_ctrl.enableScalingList = this->options.bEnableScalingList;
//    coding_ctrl.cabacInitFlag = this->options.bCabacInitFlag;

    coding_ctrl.disableDeblockingFilter = this->options.bDisableDeblocking;
    coding_ctrl.tc_Offset = this->options.nTcOffset;
    coding_ctrl.beta_Offset = this->options.nBetaOffset;
    coding_ctrl.enableSao = this->options.bEnableSAO;
    coding_ctrl.enableDeblockOverride = this->options.bEnableDeblockOverride;
    coding_ctrl.deblockOverride = this->options.bDeblockOverride;

    coding_ctrl.seiMessages = this->options.bSeiMessages;
    coding_ctrl.gdrDuration = this->options.gdrDuration;
    coding_ctrl.fieldOrder = this->options.fieldOrder;

    coding_ctrl.cirStart = this->options.cirStart;
    coding_ctrl.cirInterval = this->options.cirInterval;

    if(coding_ctrl.gdrDuration == 0)
    {
      coding_ctrl.intraArea.top = params->intraArea.nTop;
      coding_ctrl.intraArea.left = params->intraArea.nLeft;
      coding_ctrl.intraArea.bottom = params->intraArea.nBottom;
      coding_ctrl.intraArea.right = params->intraArea.nRight;
      coding_ctrl.intraArea.enable = params->intraArea.bEnable;
      coding_ctrl.intraArea.enable = _CheckArea(&coding_ctrl.intraArea, this);
    }
    else
    {
      //intraArea will be used by GDR, customer can not use intraArea when GDR is enabled. 
      coding_ctrl.intraArea.enable = 0;
    }

    coding_ctrl.pcm_loop_filter_disabled_flag = this->options.pcm_loop_filter_disabled_flag;

    if (params->ipcmArea[0].bEnable)
    {
        coding_ctrl.ipcm1Area.top     = params->ipcmArea[0].nTop;
        coding_ctrl.ipcm1Area.left    = params->ipcmArea[0].nLeft;
        coding_ctrl.ipcm1Area.bottom  = params->ipcmArea[0].nBottom;
        coding_ctrl.ipcm1Area.right   = params->ipcmArea[0].nRight;
        coding_ctrl.ipcm1Area.enable = _CheckArea(&coding_ctrl.ipcm1Area, this);

        DBGT_PDEBUG("IPCM area 1 enabled (%d, %d, %d, %d)",
            coding_ctrl.ipcm1Area.top, coding_ctrl.ipcm1Area.left,
            coding_ctrl.ipcm1Area.bottom, coding_ctrl.ipcm1Area.right);
    }

    if (params->ipcmArea[1].bEnable)
    {
        coding_ctrl.ipcm2Area.top     = params->ipcmArea[1].nTop;
        coding_ctrl.ipcm2Area.left    = params->ipcmArea[1].nLeft;
        coding_ctrl.ipcm2Area.bottom  = params->ipcmArea[1].nBottom;
        coding_ctrl.ipcm2Area.right   = params->ipcmArea[1].nRight;
        coding_ctrl.ipcm2Area.enable = _CheckArea(&coding_ctrl.ipcm2Area, this);

        DBGT_PDEBUG("IPCM area 1 enabled (%d, %d, %d, %d)",
            coding_ctrl.ipcm2Area.top, coding_ctrl.ipcm2Area.left,
            coding_ctrl.ipcm2Area.bottom, coding_ctrl.ipcm2Area.right);
    }
    coding_ctrl.ipcmMapEnable = this->options.ipcmMapEnable;
    coding_ctrl.pcm_enabled_flag = (coding_ctrl.ipcm1Area.enable || coding_ctrl.ipcm2Area.enable || coding_ctrl.ipcmMapEnable);

    if(coding_ctrl.gdrDuration == 0)
    {
        coding_ctrl.roi1Area.top = params->roiArea[0].nTop;
        coding_ctrl.roi1Area.left = params->roiArea[0].nLeft;
        coding_ctrl.roi1Area.bottom = params->roiArea[0].nBottom;
        coding_ctrl.roi1Area.right = params->roiArea[0].nRight;
        if (_CheckArea(&coding_ctrl.roi1Area, this) && (params->roiDeltaQP[0].nDeltaQP /* TBD alen || (cml->roi1Qp >= 0)*/))
          coding_ctrl.roi1Area.enable = 1;
        else
          coding_ctrl.roi1Area.enable = 0;
    }
    else
    {
        coding_ctrl.roi1Area.enable = 0;
    }

    VCEncPictureArea *roi_ptr[MAX_ROI_AREA-1] = {&coding_ctrl.roi2Area, 
                                                 &coding_ctrl.roi3Area,
                                                 &coding_ctrl.roi4Area,
                                                 &coding_ctrl.roi5Area,
                                                 &coding_ctrl.roi6Area,
                                                 &coding_ctrl.roi7Area,
                                                 &coding_ctrl.roi8Area,
                                                };
    for(i=0; i<MAX_ROI_AREA-1; i++)
    {
        roi_ptr[i]->top = params->roiArea[i+1].nTop;
        roi_ptr[i]->left = params->roiArea[i+1].nLeft;
        roi_ptr[i]->bottom = params->roiArea[i+1].nBottom;
        roi_ptr[i]->right = params->roiArea[i+1].nRight;
        if (_CheckArea(roi_ptr[i], this) && (params->roiDeltaQP[i+1].nDeltaQP /* TBD alen || (cml->roi1Qp >= 0)*/))
          roi_ptr[i]->enable = 1;
        else
          roi_ptr[i]->enable = 0;
    }

    i32 *roi_deltaQP_ptr[MAX_ROI_DELTAQP] = {&coding_ctrl.roi1DeltaQp,
                                             &coding_ctrl.roi2DeltaQp,
                                             &coding_ctrl.roi3DeltaQp,
                                             &coding_ctrl.roi4DeltaQp,
                                             &coding_ctrl.roi5DeltaQp,
                                             &coding_ctrl.roi6DeltaQp,
                                             &coding_ctrl.roi7DeltaQp,
                                             &coding_ctrl.roi8DeltaQp,
                                            };
    for(i=0; i<MAX_ROI_DELTAQP; i++)
    {
        *roi_deltaQP_ptr[i] = params->roiDeltaQP[i].nDeltaQP;
    }

    i32 *roi_QP_ptr[MAX_ROI_AREA] =         {&coding_ctrl.roi1Qp,
                                             &coding_ctrl.roi2Qp,
                                             &coding_ctrl.roi3Qp,
                                             &coding_ctrl.roi4Qp,
                                             &coding_ctrl.roi5Qp,
                                             &coding_ctrl.roi6Qp,
                                             &coding_ctrl.roi7Qp,
                                             &coding_ctrl.roi8Qp,
                                            };
    for(i=0; i<MAX_ROI_DELTAQP; i++)
    {
        *roi_QP_ptr[i] = this->options.roiQp[i];
    }

    coding_ctrl.roiMapDeltaQpEnable = this->options.roiMapDeltaQpEnable;
    coding_ctrl.roiMapDeltaQpBlockUnit = this->options.roiMapDeltaQpBlockUnit;

    //TBD alen: need to consider how to pass file handler.
    coding_ctrl.RoimapCuCtrl_index_enable = 0;
    coding_ctrl.RoimapCuCtrl_enable       = 0;
    coding_ctrl.roiMapDeltaQpBinEnable    = 0;
    coding_ctrl.RoimapCuCtrl_ver          = 0;
    coding_ctrl.RoiQpDelta_ver            = this->options.RoiQpDelta_ver; // only valid when roiMapInfoBinFile is enabled;

    coding_ctrl.chroma_qp_offset = this->options.chromaQpOffset;

    //TBD alen: need to consider how to pass callback function context.
    /* low latency */
    coding_ctrl.inputLineBufEn = 0;
    coding_ctrl.inputLineBufLoopBackEn = 0;
    coding_ctrl.inputLineBufDepth = 0;
    coding_ctrl.amountPerLoopBack = 0;
    coding_ctrl.inputLineBufHwModeEn = 0;
    coding_ctrl.inputLineBufCbFunc = NULL; //TBD alen: VCEncInputLineBufDone;
    coding_ctrl.inputLineBufCbData = NULL;
    /*stream multi-segment*/
    coding_ctrl.streamMultiSegmentMode = 0;
    coding_ctrl.streamMultiSegmentAmount = 0;
    coding_ctrl.streamMultiSegCbFunc = NULL; //TBD alen: EncStreamSegmentReady;
    coding_ctrl.streamMultiSegCbData = NULL;



    coding_ctrl.noiseReductionEnable = this->options.noiseReductionEnable;
    coding_ctrl.noiseLow = CLIP3(1, 30, this->options.noiseLow);
    coding_ctrl.firstFrameSigma = CLIP3(1, 30, this->options.noiseFirstFrameSigma);

    //TBD alen: need to consider how to parse smart config
    coding_ctrl.smartModeEnable = 0;


    /* tile */
    coding_ctrl.tiles_enabled_flag = this->options.tiles_enabled_flag && !IS_H264(this->codecFormat);
    coding_ctrl.num_tile_columns = this->options.num_tile_columns;
    coding_ctrl.num_tile_rows = this->options.num_tile_rows;
    coding_ctrl.loop_filter_across_tiles_enabled_flag = this->options.loop_filter_across_tiles_enabled_flag;


    /* HDR 10 */
    coding_ctrl.Hdr10Display.hdr10_display_enable = this->options.hdr10_display_enable;
    if (this->options.hdr10_display_enable)
    {
    	coding_ctrl.Hdr10Display.hdr10_dx0 = this->options.hdr10_dx0;
    	coding_ctrl.Hdr10Display.hdr10_dy0 = this->options.hdr10_dy0;
    	coding_ctrl.Hdr10Display.hdr10_dx1 = this->options.hdr10_dx1;
    	coding_ctrl.Hdr10Display.hdr10_dy1 = this->options.hdr10_dy1;
    	coding_ctrl.Hdr10Display.hdr10_dx2 = this->options.hdr10_dx2;
    	coding_ctrl.Hdr10Display.hdr10_dy2 = this->options.hdr10_dy2;
    	coding_ctrl.Hdr10Display.hdr10_wx  = this->options.hdr10_wx;
    	coding_ctrl.Hdr10Display.hdr10_wy  = this->options.hdr10_wy;
    	coding_ctrl.Hdr10Display.hdr10_maxluma = this->options.hdr10_maxluma;
    	coding_ctrl.Hdr10Display.hdr10_minluma = this->options.hdr10_minluma;
    }
    
    coding_ctrl.Hdr10LightLevel.hdr10_lightlevel_enable = this->options.hdr10_lightlevel_enable;
    if (this->options.hdr10_lightlevel_enable)
    {
    	coding_ctrl.Hdr10LightLevel.hdr10_maxlight = this->options.hdr10_maxlight;
    	coding_ctrl.Hdr10LightLevel.hdr10_avglight = this->options.hdr10_avglight;
    }
    
    coding_ctrl.Hdr10Color.hdr10_color_enable = this->options.hdr10_color_enable;
    if (this->options.hdr10_color_enable)
    {
    	coding_ctrl.Hdr10Color.hdr10_matrix   = this->options.hdr10_matrix;
    	coding_ctrl.Hdr10Color.hdr10_primary  = this->options.hdr10_primary;
    
    	if (this->options.hdr10_transfer == 1)
    		coding_ctrl.Hdr10Color.hdr10_transfer = VCENC_HDR10_ST2084;
    	else if (this->options.hdr10_transfer == 2)
    		coding_ctrl.Hdr10Color.hdr10_transfer = VCENC_HDR10_STDB67;
    	else
    		coding_ctrl.Hdr10Color.hdr10_transfer = VCENC_HDR10_BT2020;
    }

    coding_ctrl.RpsInSliceHeader = this->options.RpsInSliceHeader;

    coding_ctrl.PsyFactor = this->options.PsyFactor;
    coding_ctrl.meVertSearchRange = this->options.meVertSearchRange;
    coding_ctrl.layerInRefIdcEnable = this->options.layerInRefIdcEnable;

    ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
    if(ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetCodingCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    VCEncRateCtrl rate_ctrl;
    ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // optional. Set to -1 to use default
    if (this->options.nQpDefault > 0)
    {
        rate_ctrl.qpHdr = this->options.nQpDefault;
    }
    else
    {
        rate_ctrl.qpHdr = -1;
    }

    // optional. Set to -1 to use default
    if (this->options.nQpMin >= 0)
    {
        rate_ctrl.qpMinI = rate_ctrl.qpMinPB = this->options.nQpMin;

        if (rate_ctrl.qpHdr != -1 && rate_ctrl.qpHdr < (OMX_S32)rate_ctrl.qpMinI)
        {
            rate_ctrl.qpHdr = rate_ctrl.qpMinI;
        }
    }

    // optional. Set to -1 to use default
    if (this->options.nQpMax > 0)
    {
        rate_ctrl.qpMaxI = rate_ctrl.qpMaxPB = this->options.nQpMax;

        if (rate_ctrl.qpHdr > (OMX_S32)rate_ctrl.qpMaxI)
        {
            rate_ctrl.qpHdr = rate_ctrl.qpMaxI;
        }
    }

    // Set to -1 to use default
    if (this->options.nPictureRcEnabled >= 0)
    {
        rate_ctrl.pictureRc = this->options.nPictureRcEnabled;
    }

    rate_ctrl.ctbRc = this->options.ctbRcMode;
    rate_ctrl.blockRCSize = (this->options.blockRCSize == VSI_DEFAULT_VALUE) ? 0 : this->options.blockRCSize;
    rate_ctrl.rcQpDeltaRange = (this->options.rcQpDeltaRange == VSI_DEFAULT_VALUE) ? 10 : this->options.rcQpDeltaRange;
    rate_ctrl.rcBaseMBComplexity = (this->options.rcBaseMBComplexity == VSI_DEFAULT_VALUE) ? 15 : this->options.rcBaseMBComplexity;
    if(this->options.picQpDeltaMin != VSI_DEFAULT_VALUE)
        rate_ctrl.picQpDeltaMin = this->options.picQpDeltaMin;
    if(this->options.picQpDeltaMax != VSI_DEFAULT_VALUE)
        rate_ctrl.picQpDeltaMax = this->options.picQpDeltaMax;
    if(this->options.bitVarRangeI != VSI_DEFAULT_VALUE)
        rate_ctrl.bitVarRangeI = this->options.bitVarRangeI;
    if(this->options.bitVarRangeP != VSI_DEFAULT_VALUE)
        rate_ctrl.bitVarRangeP = this->options.bitVarRangeP;
    if(this->options.bitVarRangeB != VSI_DEFAULT_VALUE)
        rate_ctrl.bitVarRangeB = this->options.bitVarRangeB;

    if(this->options.tolCtbRcInter != VSI_DEFAULT_VALUE)
        rate_ctrl.tolCtbRcInter = Q16_FLOAT(this->options.tolCtbRcInter);
    if(this->options.tolCtbRcIntra != VSI_DEFAULT_VALUE)
        rate_ctrl.tolCtbRcIntra = this->options.tolCtbRcIntra;
    if(this->options.ctbRowQpStep != VSI_DEFAULT_VALUE)
        rate_ctrl.ctbRcRowQpStep = this->options.ctbRowQpStep;

    rate_ctrl.longTermQpDelta = this->options.longTermQpDelta;
    rate_ctrl.tolMovingBitRate = this->options.tolMovingBitRate;

    rate_ctrl.monitorFrames = this->options.monitorFrames;
    if(rate_ctrl.monitorFrames>MOVING_AVERAGE_FRAMES)
    {
        rate_ctrl.monitorFrames=MOVING_AVERAGE_FRAMES;
    }
    else if(rate_ctrl.monitorFrames < 10)
    {
        rate_ctrl.monitorFrames = (this->options.frameRateNum > this->options.frameRateDenom)? 10 : LEAST_MONITOR_FRAME;
    }

    // optional. Set to -1 to use default
    if (this->options.nTargetBitrate >= 0)
    {
        rate_ctrl.bitPerSecond = this->options.nTargetBitrate;
    }
    this->currBitrate = rate_ctrl.bitPerSecond;

    // Set to -1 to use default
    if (this->options.nHrdEnabled >= 0)
    {
        rate_ctrl.hrd = this->options.nHrdEnabled;
    }

#if 0
    //if (params->hevc_config.nPFrames)
    {
        rate_ctrl.bitrateWindow = this->nPFrames + 1;
    }
#endif
    if (this->options.intraPicRate != 0)
    {
        rate_ctrl.bitrateWindow = MIN(this->options.intraPicRate, MAX_GOP_LEN);
    }

    if(this->options.bitrateWindow != VSI_DEFAULT_VALUE)
    {
        rate_ctrl.bitrateWindow = this->options.bitrateWindow;
    }

    if(this->options.intraQpDelta != VSI_DEFAULT_VALUE)
    {
        rate_ctrl.intraQpDelta = this->options.intraQpDelta;
    }

    if(this->options.vbr != VSI_DEFAULT_VALUE)
    {
        rate_ctrl.vbr = this->options.vbr;
    }

    rate_ctrl.fixedIntraQp = this->options.fixedIntraQp;
    rate_ctrl.smoothPsnrInGOP = this->options.smoothPsnrInGOP;
    rate_ctrl.u32StaticSceneIbitPercent = this->options.u32StaticSceneIbitPercent;
    rate_ctrl.hrdCpbSize = this->options.hrdCpbSize;

    rate_ctrl.pictureSkip = this->options.picSkip;

    rate_ctrl.crf = this->options.crf;

    ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetRateCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    VCEncPreProcessingCfg pp_config;
    ret = VCEncGetPreProcessing(this->instance, &pp_config);

    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetPreProcessing failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    pp_config.origWidth = params->pp_config.origWidth;
    pp_config.origHeight = params->pp_config.origHeight;
    pp_config.xOffset = params->pp_config.xOffset;
    pp_config.yOffset = params->pp_config.yOffset;

    pp_config.scaledOutput = 0;

    switch (params->pp_config.formatType)
    {
        case OMX_COLOR_FormatYUV420PackedPlanar:
        case OMX_COLOR_FormatYUV420Planar:
            pp_config.inputType = VCENC_YUV420_PLANAR;
            break;
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            pp_config.inputType = VCENC_YUV420_SEMIPLANAR;
            break;
        case OMX_COLOR_FormatYCbYCr:
            pp_config.inputType = VCENC_YUV422_INTERLEAVED_YUYV;
            break;
        case OMX_COLOR_FormatCbYCrY:
            pp_config.inputType = VCENC_YUV422_INTERLEAVED_UYVY;
            break;
        case OMX_COLOR_Format16bitRGB565:
            pp_config.inputType = VCENC_RGB565;
            break;
        case OMX_COLOR_Format16bitBGR565:
            pp_config.inputType = VCENC_BGR565;
            break;
        case OMX_COLOR_Format16bitARGB4444:
            pp_config.inputType = VCENC_RGB444;
            break;
        case OMX_COLOR_Format16bitARGB1555:
            pp_config.inputType = VCENC_RGB555;
            break;
        case OMX_COLOR_Format12bitRGB444:
            pp_config.inputType = VCENC_RGB444;
        case OMX_COLOR_Format25bitARGB1888:
        case OMX_COLOR_Format32bitARGB8888:
            pp_config.inputType = VCENC_RGB888;
            break;

        default:
            DBGT_CRITICAL("Unknown color format");
            ret = VCENC_INVALID_ARGUMENT;
            break;
    }

    switch (params->pp_config.angle)
    {
        case 0:
            pp_config.rotation = VCENC_ROTATE_0;
            break;
        case 90:
            pp_config.rotation = VCENC_ROTATE_90R;
            break;
        case 270:
            pp_config.rotation = VCENC_ROTATE_90L;
            break;
        default:
            DBGT_CRITICAL("Unsupported rotation angle");
            ret = VCENC_INVALID_ARGUMENT;
            break;
    }

    //pp_config.videoStabilization = params->pp_config.frameStabilization;

    if (ret == VCENC_OK)
    {
        //DBGT_PDEBUG("Video stabilization: %d", (int)pp_config.videoStabilization);
        DBGT_PDEBUG("Rotation: %d", (int)params->pp_config.angle);
        ret = VCEncSetPreProcessing(this->instance, &pp_config);
    }

    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetPreProcessing failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

#ifdef TEST_DATA
{
    extern i32 Enc_test_data_init(i32 parallelCoreNum);
    Enc_test_data_init(cfg.parallelCoreNum);
}
#endif

    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_intra_period_codec(ENCODER_PROTOTYPE* arg, OMX_U32 nPFrames, OMX_U32 nBFrames)
{
    DBGT_PROLOG("");

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->nPFrames = nPFrames;
	this->nBFrames = nBFrames;
    DBGT_PDEBUG("New Intra period %d", (int)this->nPFrames+this->nBFrames+1);

    VCEncRateCtrl rate_ctrl;
    memset(&rate_ctrl, 0, sizeof(VCEncRateCtrl));

    VCEncRet ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK)
    {
        rate_ctrl.bitrateWindow = nPFrames+nBFrames+1;

        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    }
    else
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case VCENC_OK:
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_codec(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;

    this->nEstTimeInc = (OMX_U32) (TIME_RESOLUTION / Q16_FLOAT(xFramerate));

    DBGT_PDEBUG("New time increment %d", (int)this->nEstTimeInc);

    DBGT_EPILOG("");
    return CODEC_OK;
}
