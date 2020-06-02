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

#ifndef _VSI_VENDOR_EXT_H_
#define _VSI_VENDOR_EXT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Index.h>

/* Second view frame flag:
 * This flag is set when the buffer content contains second view frame from MVC stream
 *  @ingroup buf
 */
#define OMX_BUFFERFLAG_SECOND_VIEW      0x00010000

/* VP8 temporal layer frame flags:
 * One of these flags is set when the buffer contains encoded VP8 temporal layer frame
 *  @ingroup buf
 */
#define OMX_BUFFERFLAG_BASE_LAYER       0x00020000
#define OMX_BUFFERFLAG_FIRST_LAYER      0x00040000
#define OMX_BUFFERFLAG_SECOND_LAYER     0x00080000
#define OMX_BUFFERFLAG_THIRD_LAYER      0x00100000

/* Struct for allocated buffer data. Carried in pInputPortPrivate */
typedef struct ALLOC_PRIVATE {
    OMX_U8* pBufferData;                // Virtual address of the buffer
    OMX_U64 nBusAddress;                // Physical address of the buffer
    OMX_U32 nBufferSize;                // Allocated size
} ALLOC_PRIVATE;

/** Structure for RFC (reference frame compression) table data */
typedef struct RFC_TABLE {
    OMX_U8* pLumaBase;
    OMX_U64 nLumaBusAddress;
    OMX_U8* pChromaBase;
    OMX_U64 nChromaBusAddress;
} RFC_TABLE;

/* Struct for output buffer data. Carried in pOutputPortPrivate */
typedef struct OUTPUT_BUFFER_PRIVATE {
    OMX_U8* pLumaBase;                  // Virtual address of the luminance buffer
    OMX_U64 nLumaBusAddress;            // Physical address of the luminance buffer
    OMX_U32 nLumaSize;                  // Size of the luminance data
    OMX_U8* pChromaBase;                // Virtual address of the chrominance buffer
    OMX_U64 nChromaBusAddress;          // Physical address of the chrominance buffer
    OMX_U32 nChromaSize;                // Size of the chrominance data
    RFC_TABLE sRfcTable;                // RFC table data (G2 only)
    OMX_U32 nBitDepthLuma;              // Luma component valid bit depth
    OMX_U32 nBitDepthChroma;            // Chroma component valid bit depth
    OMX_U32 nFrameWidth;                // Picture width in pixels
    OMX_U32 nFrameHeight;               // Picture height in pixels
    OMX_U32 nStride;                    // Picture stride in bytes
    OMX_U32 nPicId[2];                  // Identifier of the picture in decoding order
                                        // For H264 interlace stream, nPicId[0]/nPicId[1] are used for top/bottom field */
    OMX_BOOL realloc;
    OMX_BOOL singleField;               // Flag to indicate single field in output buffer
} OUTPUT_BUFFER_PRIVATE;

typedef enum OMX_INDEXVSITYPE {
    OMX_IndexVsiStartUnused = OMX_IndexVendorStartUnused + 0x00100000,
    OMX_IndexParamVideoMvcStream,
    OMX_IndexConfigVideoIntraArea,
    OMX_IndexConfigVideoRoiArea,
    OMX_IndexConfigVideoRoiDeltaQp,
    OMX_IndexConfigVideoAdaptiveRoi,
    OMX_IndexConfigVideoVp8TemporalLayers,
    OMX_IndexParamVideoHevc,               /**< reference: OMX_VIDEO_PARAM_HEVCTYPE */
    OMX_IndexParamVideoVp9,                /**< reference: OMX_VIDEO_PARAM_VP9TYPE */
    OMX_IndexParamVideoConfig,
    OMX_IndexParamVideoCodecFormat,        /*which encoding type is selected in  vc8000e combined encoding system in which one of avc, hevc or av1 can be selected*/

    OMX_IndexConfigVideoIPCMArea,
    OMX_IndexParamVideoAvcExt              /* AVC encoding parameters extention, refer to OMX_VIDEO_PARAM_AVCEXTTYPE */
} OMX_INDEXVSITYPE;

typedef enum OMX_VIDEO_CODINGVSITYPE {
    OMX_VIDEO_CodingVsiStartUnused = OMX_VIDEO_CodingVendorStartUnused + 0x00100000,
    OMX_VIDEO_CodingSORENSON,
    OMX_VIDEO_CodingDIVX,
    OMX_VIDEO_CodingDIVX3,
    OMX_VIDEO_CodingVP6,
    OMX_VIDEO_CodingAVS,
    OMX_VIDEO_CodingHEVC,
    OMX_VIDEO_CodingVP9,

    OMX_VIDEO_CodingAV1
} OMX_VIDEO_CODINGVSITYPE;

typedef enum OMX_COLOR_FORMATVSITYPE {
    OMX_COLOR_FormatVsiStartUnused = OMX_COLOR_FormatVendorStartUnused + 0x00100000,
    OMX_COLOR_FormatYUV411SemiPlanar,
    OMX_COLOR_FormatYUV411PackedSemiPlanar,
    OMX_COLOR_FormatYUV440SemiPlanar,
    OMX_COLOR_FormatYUV440PackedSemiPlanar,
    OMX_COLOR_FormatYUV444SemiPlanar,
    OMX_COLOR_FormatYUV444PackedSemiPlanar,
    OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled,   /* VC8000D tiled format */
    OMX_COLOR_FormatYUV420SemiPlanarP010        /* P010 format */

    , OMX_COLOR_FormatYUV420SemiPlanarVU        /* NV21*/
    , OMX_COLOR_Format16bitBGR555               /* 15-bit RGB 16bpp */
} OMX_COLOR_FORMATVSITYPE;

/** Structure for configuring H.264 MVC mode */
typedef struct OMX_VIDEO_PARAM_MVCSTREAMTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bIsMVCStream;
} OMX_VIDEO_PARAM_MVCSTREAMTYPE;

/** Structure for configuring Intra area for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_INTRAAREATYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
    OMX_U32 nTop;       /* Top mb row inside area [0..heightMbs-1]      */
    OMX_U32 nLeft;      /* Left mb row inside area [0..widthMbs-1]      */
    OMX_U32 nBottom;    /* Bottom mb row inside area [top..heightMbs-1] */
    OMX_U32 nRight;     /* Right mb row inside area [left..widthMbs-1]  */
} OMX_VIDEO_CONFIG_INTRAAREATYPE;

/** Structure for configuring ROI area for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_ROIAREATYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
    OMX_U32 nArea;      /* ROI area number [1..2]                       */
    OMX_U32 nTop;       /* Top mb row inside area [0..heightMbs-1]      */
    OMX_U32 nLeft;      /* Left mb row inside area [0..widthMbs-1]      */
    OMX_U32 nBottom;    /* Bottom mb row inside area [top..heightMbs-1] */
    OMX_U32 nRight;     /* Right mb row inside area [left..widthMbs-1]  */
} OMX_VIDEO_CONFIG_ROIAREATYPE;

/** Structure for configuring IPCM area for VC8000E encoder */
typedef struct OMX_VIDEO_CONFIG_IPCMAREATYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
    OMX_U32 nArea;      /* IPCM area number [1..2]                       */
    OMX_U32 nTop;       /* Top mb row inside area [0..heightMbs-1]      */
    OMX_U32 nLeft;      /* Left mb row inside area [0..widthMbs-1]      */
    OMX_U32 nBottom;    /* Bottom mb row inside area [top..heightMbs-1] */
    OMX_U32 nRight;     /* Right mb row inside area [left..widthMbs-1]  */
} OMX_VIDEO_CONFIG_IPCMAREATYPE;

/** Structure for configuring ROI Delta QP for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_ROIDELTAQPTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nArea;      /* ROI area number [1..2]               */
    OMX_S32 nDeltaQP;   /* QP delta value [-127..0] for VP8     */
                        /*                [-15..0]  for H264    */
} OMX_VIDEO_CONFIG_ROIDELTAQPTYPE;

/** Structure for configuring Adaptive ROI for H1 encoder */
typedef struct OMX_VIDEO_CONFIG_ADAPTIVEROITYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_S32 nAdaptiveROI;       /* QP delta for adaptive ROI [-51..0]       */
    OMX_S32 nAdaptiveROIColor;  /* Color temperature for the Adaptive ROI   */
                                /* -10=2000K, 0=3000K, 10=5000K             */
} OMX_VIDEO_CONFIG_ADAPTIVEROITYPE;

/** Structure for configuring VP8 temporal layers */
typedef struct OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nBaseLayerBitrate;  /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer1Bitrate;     /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer2Bitrate;     /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer3Bitrate;     /* Bits per second [10000..40000000]    */
} OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE;

typedef enum OMX_VIDEO_AVCEXTLEVELTYPE {
    OMX_VIDEO_AVCLevelUnused = OMX_VIDEO_AVCLevelVendorStartUnused + 0x00100000,
    OMX_VIDEO_AVCLevel52,   
    OMX_VIDEO_AVCLevel60,
    OMX_VIDEO_AVCLevel61,
    OMX_VIDEO_AVCLevel62
} OMX_VIDEO_AVCEXTLEVELTYPE;


typedef enum OMX_VIDEO_HEVCPROFILETYPE {
    OMX_VIDEO_HEVCProfileMain     = 0x01,   /**< Main profile */
    OMX_VIDEO_HEVCProfileMain10   = 0x02,   /**< Main10 profile */
    OMX_VIDEO_HEVCProfileMainStillPicture  = 0x04,   /**< Main still picture profile */
    OMX_VIDEO_HEVCProfileKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_HEVCProfileVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCProfileMax      = 0x7FFFFFFF
} OMX_VIDEO_HEVCPROFILETYPE;

typedef enum OMX_VIDEO_HEVCLEVELTYPE {
    OMX_VIDEO_HEVCLevel1   = 0x01,     /**< Level 1   */
    OMX_VIDEO_HEVCLevel2   = 0x02,     /**< Level 2   */
    OMX_VIDEO_HEVCLevel21  = 0x04,     /**< Level 2.1 */
    OMX_VIDEO_HEVCLevel3   = 0x08,     /**< Level 3   */
    OMX_VIDEO_HEVCLevel31  = 0x10,     /**< Level 3.1 */
    OMX_VIDEO_HEVCLevel4   = 0x20,     /**< Level 4   */
    OMX_VIDEO_HEVCLevel41  = 0x40,     /**< Level 4.1 */
    OMX_VIDEO_HEVCLevel5   = 0x80,     /**< Level 5   */
    OMX_VIDEO_HEVCLevel51  = 0x100,    /**< Level 5.1 */
    OMX_VIDEO_HEVCLevel52  = 0x200,    /**< Level 5.2 */
    OMX_VIDEO_HEVCLevel6   = 0x400,    /**< Level 6   */
    OMX_VIDEO_HEVCLevel61  = 0x800,    /**< Level 6.1 */
    OMX_VIDEO_HEVCLevel62  = 0x1000,   /**< Level 6.2 */
    OMX_VIDEO_HEVCLevelKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_HEVCLevelVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCLevelMax = 0x7FFFFFFF
} OMX_VIDEO_HEVCLEVELTYPE;

/**
 * AVC Encoder VSI Extension params
 *
 * STRUCT MEMBERS:
 *  nSize                     : Size of the structure in bytes
 *  nVersion                  : OMX specification version information
 *  nPortIndex                : Port that this structure applies to
 *  nBitDepthLuma             : Luma component valid bit depth.
 *  nBitDepthChroma           : Chroma component valid bit depth.
 *  gopSize                   : GOP Size, [0..8], 0 for adaptive GOP size; 1~7 for fixed GOP size
 *  hrdCpbSize                : HRD Coded Picture Buffer size in bits. Buffer size used by the HRD model.
 *  firstPic                  : First picture of input file to encode.
 *  lastPic                   : Last picture of input file to encode.
 * coded chroma_format_idc
 *  codedChromaIdc            : Specify coded chroma format idc.[1]. 0 -400, 1- 420, 2- 422
 
 * Adaptive Quantization
 *  aq_mode                   : Mode for Adaptive Quantization - 0:none 1:uniform AQ 2:auto variance 3:auto variance with bias to dark scenes. Default 0
 *  aq_strength               : Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00
 *  writeReconToDDR           : HW write recon to DDR or not if it's pure I-frame encoding
 *  TxTypeSearchEnable        : av1 tx type search 1=enable 0=disable, enabled by default

 *  
 *  PsyFactor                 : Specify 0..4 Weight of psycho-visual encoding.
                                    0 = disable psy-rd.
                                    1..4 = encode psy-rd, and set strength of psyFactor, larger favor better subjective quality.

 *  meVertSearchRange         : Specify ME vertical search range.
 *  layerInRefIdcEnable       : Enable/Disable h264 2bit nal_ref_idc

 *  rdoLevel                  : Programable hardware RDO Level [1..3].
 *  crf                       : Specify constant rate factor mode, working with look-ahead turned on.
                              :     [-1..51], -1=disable.
                              :     CRF mode is to keep a certain level of quality based on crf value, working as constant QP with complexity rate control.
                              :     CRF adjusts frame level QP within range of crf constant +-3 based on frame complexity. 
                              :     CRF will disable VBR mode if both enabled.
 * preset                     : Specify preset parameter to trade off performance and compression efficiency. 0...4 for HEVC. 0..1 for H264.
 */
typedef struct OMX_VIDEO_PARAM_AVCEXTTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_U32 nBitDepthLuma;
    OMX_U32 nBitDepthChroma;
    OMX_U32 gopSize;
    OMX_U32 hrdCpbSize;
    OMX_S32 firstPic;
    OMX_S32 lastPic;
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
    OMX_U32 aq_strength; /* Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00 */
    OMX_U32 writeReconToDDR; /*HW write recon to DDR or not if it's pure I-frame encoding*/
    OMX_U32 TxTypeSearchEnable; /*av1 tx type search 1=enable 0=disable, enabled by default*/

    /*----*/
    OMX_U32 PsyFactor;
    OMX_U32 meVertSearchRange;
	OMX_U32 layerInRefIdcEnable;

    OMX_U32 rdoLevel;

    OMX_S32 crf;             /*CRF constant [0,51]*/
    OMX_U32 preset;         /* 0...4 for HEVC. 0..1 for H264. Trade off performance and compression efficiency */
} OMX_VIDEO_PARAM_AVCEXTTYPE;

/**
 * HEVC params
 *
 * STRUCT MEMBERS:
 *  nSize                     : Size of the structure in bytes
 *  nVersion                  : OMX specification version information
 *  nPortIndex                : Port that this structure applies to
 *  eProfile                  :
 *  eLevel                    :
 *  nPFrames                  : Number of P frames between each I frame
 *  nBFrames                  : Number of B frames between each I frame


 * Parameters related to Input frame
 *  exp_of_input_alignment    : Alignment value of input frame buffer 0, [4-12]
                                   Base address of input frame buffer and each line are aligned to 2^exp_of_input_alignmeny.
 *  fieldOrder                : Specify interlaced field order, 0=bottom first, 1=top first.


 * Parameters affecting the output stream and encoding tools:
 *  nBitDepthLuma             : Luma component valid bit depth.
 *  nBitDepthChroma           : Chroma component valid bit depth.
 *  byteStream                : Stream type. 0: NAL units; 1: byte stream according to HEVC Standard Annex B.
 *  videoRange                : Specify video signal sample range in encoded stream.
                                    0 - Y range in [16..235]; Cb, Cr in [16..240]; 1 - Y, Cb, Cr range in [0..255].
 *  enableCabac               : CAVLC (0) or CABAC (1)
 *  bCabacInitFlag            : Initialization value for CABAC.
 *  sliceSize                 : slice size in number of CTU rows.  
                                    0 to encode each picture in one slice; Other value [1..height/ctu_size] specify CTU rows to encode each slice.
 *  bEnableSAO:               : Enable/disable arbitrary slice ordering.
 *  nTcOffset                 : Specify deblocking filter tc offset [-6..6]
 *  nBetaOffset               : Specify deblocking filter beta offset[-6..6]
 *  bEnableDeblockOverride    : Enable/disable deblock override between slice.
 *  bDeblockOverride          : Indicate whether deblock override between slices.
 *  num_tile_columns          : HEVC Tile setting: specify num of tile columns.
 *  num_tile_rows             : HEVC Tile setting: specify num of tile rows.
 *  loop_filter_across_tiles_enabled_flag     : HEVC Tile setting: enable/disable loop-filter across filter.
 *  bEnableScalingList        : Use average scaling list (1) or default scaling list (0).
 *  RpsInSliceHeader          : Encode rps in the slice header or not.


 * Parameters affecting GOP pattern, rate control and output stream bitrate
 *  intraPicRate              : Intra-picture rate in frames.
 *  tolMovingBitRate          : Specify percent tolerance over target bitrate of moving bit rate [10..2000].
 *  monitorFrames             : Specify how many frames will be monitored for moving bit rate [10..120]
 *  bitVarRangeI              : Specify percent variations over average bits per frame for I frame. [10..10000] for VC8000E, [10..2000] for H2v4.1.
 *  bitVarRangeP              : Specify percent variations over average bits per frame for P frame. [10..10000] for VC8000E, [10..2000] for H2v4.1.
 *  bitVarRangeB              : Specify percent variations over average bits per frame for B frame. [10..10000] for VC8000E, [10..2000] for H2v4.1.
 *  staticSceneIbitPercent    : Specify I frame bits percent of bitrate in static scene [0..100].
 *  smoothPsnrInGOP           : Enable/Disable Smooth PSNR for frames in one GOP.
 *  ctbRc                     : CTB QP adjustment mode for Rate Control and Subjective Quality. [0..1] for H2, [0..3] for VC8000E.
 *  blockRCSize               : Specify unit size of block Rate control. 0: 64x64, 1: 32x32, 2: 16x16
 *  rcQpDeltaRange            : Specify Max absolute value of CU/MB QP delta relative to frame QP in CTB RC.
 *  rcBaseMBComplexity        : Specify MB complexity base in CTB QP adjustment for Subjective Quality [0..31].
 *  tolCtbRcInter             : Specify tolerance of Ctb Rate Control for INTER frames.
 *  tolCtbRcIntra             : Specify tolerance of Ctb Rate Control for INTRA frames.
 *  ctbRowQpStep              : Specify the maximum accumulated QP adjustment step per CTB Row allowed by Ctb Rate Control
 *  picQpDeltaMin             : Specify minimum value of QP delta relative to previous QP.
 *  picQpDeltaMax             : Specify maximum value of QP delta relative to previous QP.
 *  hrdCpbSize                : HRD Coded Picture Buffer size in bits. Buffer size used by the HRD model.
 *  bitrateWindow             : Specify bit rate window length in frames [1..300].
 *  gopSize                   : GOP Size [0..8]. 0 for adaptive GOP size; 1~7 for fixed GOP size.
 *  ltrInterval               : Specify long term reference interval.
 *  longTermGapOffset         : Specify the first frame after LTR that uses the LTR as reference.
 *  longTermGap               : Specify the POC delta between two consecutive frames that use the same LTR as reference.
 *  longTermQpDelta           : Specify QP delta for frame using LTR.
 *  gopLowdelay               : Use default low delay GOP configuration or not.
 *  picSkip                   : Enable/Disable picture skip rate control. Invalid for B-Frame encoding.
 *  intraQpDelta              : Specify Intra QP delta value [-51..51].
 *  fixedIntraQp              : Specify fixed Intra QP [0..51]. 0: disable.
 *  chromaQpOffset            : Specify chroma QP offset [-12..12]
 *  vbr                       : Enable/Disable Variable Bit Rate Control by qpMin.
 *  gdrDuration               : Specify how many frames it will take to do GDR. 0: disable.


 * Parameters affecting coding
 *  cirStart                  : Specify start for Cyclic Intra Refresh
 *  cirInterval               : Specify interval for Cyclic Intra Refresh
 *  ipcmMapEnable             : Enable/Disable the IPCM Map.
 *  roiQp[8]                  : Specify absolute QP value for ROI[n] CTBs [0..51]
 *  roiMapDeltaQpBlockUnit    : Set the DeltaQp block size for ROI DeltaQp map file [0..3]. 
                                    0-64x64,1-32x32,2-16x16,3-8x8
 *  roiMapDeltaQpEnable       : Enable/disable the QP delta for ROI regions in the frame. 
 *  RoiQpDelta_ver            : ROI Qp Delta map version number [0..3]. Valid only when roiMapInfoBinFile is enabled.


 * Parameters affecting coding and performance
 *  rdoLevel                  : Programable hardware RDO Level [1..3].
 *  enableRdoQuant            : Enable/Disable RDO Quantization.


 * Parameters affecting reporting
 *  ssim                      : Enable (1) or Disable (0) SSIM calculation.
 *  cuInfoVersion             : cu info dump version.
 *  vui_timing_info_enable    : Enable/Disable writing VUI timing info in SPS.
 *  hashtype                  : Specify hash type for frame data hash reporting. 0: disable, 1: crc32, 2: checksum32.
 

 * Parameters affecting stream multi-segment output
 *  streamMultiSegmentMode    : Stream multi-segment mode control[0..2]. 
 *  streamMultiSegmentAmount  : The total amount of segments to control loop-back/SW handshaking/IRQ.


 * Parameters affecting noise reduction
 *  noiseReductionEnable      : Enable/disable noise reduction
 *  noiseLow                  : Specify minimum noise value [1..30]
 *  noiseFirstFrameSigma      : Specify noise estimation for start frames [1..30]

 
 * HDR10 Config               
 *  hdr10_display_enable      : Enable/Disable display color volume SEI message. 
 *  hdr10_dx0                 : Component 0 normalized x chromaticity coordinates
 *  hdr10_dy0                 : Component 0 normalized y chromaticity coordinates
 *  hdr10_dx1                 : Component 1 normalized x chromaticity coordinates
 *  hdr10_dy1                 : Component 1 normalized y chromaticity coordinates
 *  hdr10_dx2                 : Component 2 normalized x chromaticity coordinates
 *  hdr10_dy2                 : Component 2 normalized y chromaticity coordinates
 *  hdr10_wx                  : White point normalized x chromaticity coordinates
 *  hdr10_wy                  : White point normalized y chromaticity coordinates
 *  hdr10_maxluma             : Nominal maximum display luminance
 *  hdr10_minluma             : Nominal minimum display luminance
 *  hdr10_lightlevel_enable   : Enable/Disable light level
 *  hdr10_maxlight            : Specify max content light level
 *  hdr10_avglight            : Specify  picture average light level
 *  hdr10_color_enable        : Enable/Disable color description
 *  hdr10_primary             : Index of chromaticity coordinates in Table E.3 in HEVC spec.
 *  hdr10_transfer            : Specify the reference opto-electronic transfer characteristic function of the source picture in Table E.4 in HEVC spec.
 *  hdr10_matrix              : Specify index of matrix coefficients used in deriving luma and chroma signals 
                                    from the green, blue, and red or Y, Z, and X primaries in Table E.5 in HEVC spec.
 
 *  firstPic                  : Specify the first picture number of input clip to encode.
 *  lastPic                   : Specify the last picture number of input clip to encode.

 *  nRefFrames                : Max number of reference frames to use for inter motion search (1-16) 
                                    Valid for H2 only, obsolete for H2v4.1, VC8000E.
 *  bStrongIntraSmoothing     : Enable/disable HEVC IntraTU32x32 strong intra smoothing filter.
 *  pcm_loop_filter_disabled_flag             : Disable / Enable deblock filter for IPCM. 1-Disable, 0-Enable.

 *Extensions for CL244132
 * Parameters for MMU control
 *  mmuEnable                 : Enable/disable MMU
 *  
 * Parameters for external SRAM
 *  extSramLumHeightBwd       : 0=no external SRAM, 1..16=The number of line count is 4*extSramLumHeightBwd. [hevc:16,h264:12]
 *  extSramChrHeightBwd       : 0=no external SRAM, 1..16=The number of line count is 4*extSramChrHeightBwd. [hevc:8,h264:6]
 *  extSramLumHeightFwd       : 0=no external SRAM, 1..16=The number of line count is 4*extSramLumHeightFwd. [hevc:16,h264:12]
 *  extSramChrHeightFwd       : 0=no external SRAM, 1..16=The number of line count is 4*extSramChrHeightFwd. [hevc:8,h264:6]
 
 * Parameters for AXI alignment
 *  AXIAlignment              : AXI alignment setting (in hexadecimal format).
                              : bit[31:28] AXI_burst_align_wr_common
                              : bit[27:24] AXI_burst_align_wr_stream
                              : bit[23:20] AXI_burst_align_wr_chroma_ref
                              : bit[19:16] AXI_burst_align_wr_luma_ref
                              : bit[15:12] AXI_burst_align_rd_common
                              : bit[11: 8] AXI_burst_align_rd_prp
                              : bit[ 7: 4] AXI_burst_align_rd_ch_ref_prefetch
                              : bit[ 3: 0] AXI_burst_align_rd_lu_ref_prefetch
 
 * coded chroma_format_idc
 *  codedChromaIdc            : Specify coded chroma format idc.[1]. 0 -400, 1- 420, 2- 422
 
 * Adaptive Quantization
 *  aq_mode                   : Mode for Adaptive Quantization - 0:none 1:uniform AQ 2:auto variance 3:auto variance with bias to dark scenes. Default 0
 *  aq_strength               : Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00
 *  writeReconToDDR           : HW write recon to DDR or not if it's pure I-frame encoding
 *  TxTypeSearchEnable        : av1 tx type search 1=enable 0=disable, enabled by default

 *  
 *  PsyFactor                 : Specify 0..4 Weight of psycho-visual encoding.
                                    0 = disable psy-rd.
                                    1..4 = encode psy-rd, and set strength of psyFactor, larger favor better subjective quality.

 *  meVertSearchRange         : Specify ME vertical search range.
 *  layerInRefIdcEnable       : Enable/Disable h264 2bit nal_ref_idc

 *  crf                       : Specify constant rate factor mode, working with look-ahead turned on.
                              :     [-1..51], -1=disable.
                              :     CRF mode is to keep a certain level of quality based on crf value, working as constant QP with complexity rate control.
                              :     CRF adjusts frame level QP within range of crf constant +-3 based on frame complexity. 
                              :     CRF will disable VBR mode if both enabled.
 * preset                     : Specify preset parameter to trade off performance and compression efficiency. 0...4 for HEVC. 0..1 for H264.
 */
typedef struct OMX_VIDEO_PARAM_HEVCTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_HEVCPROFILETYPE eProfile;
    OMX_VIDEO_HEVCLEVELTYPE eLevel;
    OMX_U32 nPFrames;
    OMX_U32 nBFrames;

    OMX_U32 exp_of_input_alignment;
    OMX_U32 fieldOrder;

    OMX_U32 nBitDepthLuma;
    OMX_U32 nBitDepthChroma;
    OMX_U32 byteStream;
    OMX_S32 videoRange;
    OMX_S32 enableCabac;
    OMX_BOOL bCabacInitFlag;
    OMX_S32 sliceSize;
    OMX_BOOL bEnableSAO;
    OMX_S32 nTcOffset;
    OMX_S32 nBetaOffset;
    OMX_BOOL bEnableDeblockOverride;
    OMX_BOOL bDeblockOverride;
    OMX_U32 num_tile_columns;
    OMX_U32 num_tile_rows;
    OMX_U32 loop_filter_across_tiles_enabled_flag;
    OMX_BOOL bEnableScalingList;
    OMX_U32 RpsInSliceHeader;


    OMX_U32 intraPicRate;
    OMX_S32 tolMovingBitRate;
    OMX_S32 monitorFrames;
    OMX_S32 bitVarRangeI;
    OMX_S32 bitVarRangeP;
    OMX_S32 bitVarRangeB;
    OMX_S32 staticSceneIbitPercent;
    OMX_S32 smoothPsnrInGOP;
    OMX_S32 ctbRc;
    OMX_S32 blockRCSize;
    OMX_S32 rcQpDeltaRange;
    OMX_S32 rcBaseMBComplexity;
    OMX_S32 tolCtbRcInter;
    OMX_S32 tolCtbRcIntra;
    OMX_S32 ctbRowQpStep;
    OMX_S32 picQpDeltaMin;
    OMX_S32 picQpDeltaMax;
	OMX_U32 hrdCpbSize;
    OMX_S32 bitrateWindow;
    OMX_U32 gopSize;
    OMX_S32 ltrInterval;
    OMX_S32 longTermGapOffset;
    OMX_S32 longTermGap;
    OMX_S32 longTermQpDelta;
    OMX_U32 gopLowdelay;
    OMX_S32 picSkip;
    OMX_S32 intraQpDelta;
    OMX_S32 fixedIntraQp;
    OMX_U32 chromaQpOffset;
    OMX_S32 vbr;
    OMX_U32 gdrDuration;


    OMX_U32 cirStart;
    OMX_U32 cirInterval;
    OMX_U32 ipcmMapEnable;
    OMX_S32 roiQp[8];
    OMX_U32 roiMapDeltaQpBlockUnit;
    OMX_U32 roiMapDeltaQpEnable;
    OMX_U32 RoiQpDelta_ver;


    OMX_U32 rdoLevel;
    OMX_S32 enableRdoQuant;


    OMX_U32 ssim;
    OMX_S32 cuInfoVersion;
    OMX_S32 vui_timing_info_enable;
    OMX_S32 hashtype;


    OMX_U32 streamMultiSegmentMode;
    OMX_U32 streamMultiSegmentAmount;


    OMX_U32 noiseReductionEnable;
    OMX_U32 noiseLow;
    OMX_U32 noiseFirstFrameSigma;


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


    OMX_S32 firstPic;
    OMX_S32 lastPic;
    OMX_U32 nRefFrames;
    OMX_BOOL bStrongIntraSmoothing;
    OMX_U32 pcm_loop_filter_disabled_flag;

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
    OMX_U32 aq_strength; /* Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00 */
    OMX_U32 writeReconToDDR; /*HW write recon to DDR or not if it's pure I-frame encoding*/
    OMX_U32 TxTypeSearchEnable; /*av1 tx type search 1=enable 0=disable, enabled by default*/

    /*----*/
    OMX_U32 PsyFactor;
    OMX_U32 meVertSearchRange;
	OMX_U32 layerInRefIdcEnable;

    OMX_S32 crf;             /*CRF constant [0,51]*/
    OMX_U32 preset;         /* 0...4 for HEVC. 0..1 for H264. Trade off performance and compression efficiency */
} OMX_VIDEO_PARAM_HEVCTYPE;

typedef struct OMX_VIDEO_PARAM_CODECFORMAT {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_CODINGTYPE nCodecFormat;
} OMX_VIDEO_PARAM_CODECFORMAT;


/** VP9 profiles */
typedef enum OMX_VIDEO_VP9PROFILETYPE {
    OMX_VIDEO_VP9Profile0 = 0x01, /* 8-bit 4:2:0 */
    OMX_VIDEO_VP9Profile1 = 0x02, /* 8-bit 4:2:2, 4:4:4, alpha channel */
    OMX_VIDEO_VP9Profile2 = 0x04, /* 10-bit/12-bit 4:2:0, YouTube Premium Content Profile */
    OMX_VIDEO_VP9Profile3 = 0x08, /* 10-bit/12-bit 4:2:2, 4:4:4, alpha channel */
    OMX_VIDEO_VP9ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9ProfileKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VP9ProfileVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP9ProfileMax = 0x7FFFFFFF
} OMX_VIDEO_VP9PROFILETYPE;

/** VP9 Param */
typedef struct OMX_VIDEO_PARAM_VP9TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_VP9PROFILETYPE eProfile;
    OMX_U32 nBitDepthLuma;
    OMX_U32 nBitDepthChroma;
} OMX_VIDEO_PARAM_VP9TYPE;

/** G2 Decoder pixel formats */
typedef enum OMX_VIDEO_G2PIXELFORMAT {
    OMX_VIDEO_G2PixelFormat_Default     = 0x0,
    OMX_VIDEO_G2PixelFormat_8bit        = 0x01, /* 10 bit data is clamped to 8 bit per pixel */
    OMX_VIDEO_G2PixelFormat_P010        = 0x02, /* MS P010 format */
    OMX_VIDEO_G2PixelFormat_Custom1     = 0x03
} OMX_VIDEO_G2PIXELFORMAT;

typedef enum OMX_VIDEO_DECODER_MODE {
  OMX_VIDEO_DEC_NORMAL = 0,
  OMX_VIDEO_DEC_LOW_LATENCY = 1,
  //OMX_VIDEO_DEC_LOW_LATENCY_RTL = 2,
  OMX_VIDEO_DEC_SECURITY = 3
} OMX_VIDEO_DECODER_MODE;

/** Structure for configuring VC8000D decoder */
typedef struct OMX_VIDEO_PARAM_CONFIGTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnableTiled;          /* Store reference pictures in tiled format */
    OMX_U32 nGuardSize;
    OMX_BOOL bEnableAdaptiveBuffers;
    OMX_VIDEO_G2PIXELFORMAT ePixelFormat;
    OMX_BOOL bEnableRFC;
    OMX_BOOL bEnableRingBuffer;
    OMX_BOOL bDisableReordering;
    OMX_VIDEO_DECODER_MODE eDecMode;
} OMX_VIDEO_PARAM_CONFIGTYPE;
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _VSI_VENDOR_EXT_H_
