/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Abstract : HEVC Encoder testbench for linux
--
------------------------------------------------------------------------------*/
#ifndef _HEVCTESTBENCH_H_
#define _HEVCTESTBENCH_H_
#ifdef _WIN32
#include <windows.h>
#endif
#include "base_type.h"

#include "hevcencapi.h"
#include "encinputlinebuffer.h"
#ifndef _WIN32
#include <sys/time.h>
#endif
/*--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define DEFAULT -255
#define MAX_BPS_ADJUST 20
#define MAX_STREAMS 16
#define MAX_SCENE_CHANGE 20
#define MAX_CUTREE_DEPTH 64
#define MAX_DELAY_NUM (MAX_CORE_NUM+MAX_CUTREE_DEPTH)

/* Structure for command line options */
typedef struct commandLine_s
{
  char *input;
  char *output;
  char *recon;
  char *dec400CompTableinput;

  char *test_data_files;
  i32 outputRateNumer;      /* Output frame rate numerator */
  i32 outputRateDenom;      /* Output frame rate denominator */
  i32 inputRateNumer;      /* Input frame rate numerator */
  i32 inputRateDenom;      /* Input frame rate denominator */
  i32 firstPic;
  i32 lastPic;

  i32 width;
  i32 height;
  i32 lumWidthSrc;
  i32 lumHeightSrc;

  i32 inputFormat;
  i32 formatCustomizedType;     /*change general format to customized one*/

  i32 picture_cnt;
  i32 byteStream;
  i32 videoStab;

  i32 max_cu_size;    /* Max coding unit size in pixels */
  i32 min_cu_size;    /* Min coding unit size in pixels */
  i32 max_tr_size;    /* Max transform size in pixels */
  i32 min_tr_size;    /* Min transform size in pixels */
  i32 tr_depth_intra;   /* Max transform hierarchy depth */
  i32 tr_depth_inter;   /* Max transform hierarchy depth */
  VCEncVideoCodecFormat codecFormat;     /* Video Codec Format: HEVC/H264/AV1 */

  i32 min_qp_size;

  i32 enableCabac;      /* [0,1] H.264 entropy coding mode, 0 for CAVLC, 1 for CABAC */
  i32 cabacInitFlag;

  // intra setup
  u32 strong_intra_smoothing_enabled_flag;

  i32 cirStart;
  i32 cirInterval;

  i32 intraAreaEnable;
  i32 intraAreaTop;
  i32 intraAreaLeft;
  i32 intraAreaBottom;
  i32 intraAreaRight;

  i32 pcm_loop_filter_disabled_flag;

  i32 ipcm1AreaTop;
  i32 ipcm1AreaLeft;
  i32 ipcm1AreaBottom;
  i32 ipcm1AreaRight;

  i32 ipcm2AreaTop;
  i32 ipcm2AreaLeft;
  i32 ipcm2AreaBottom;
  i32 ipcm2AreaRight;
  i32 ipcmMapEnable;
  char *ipcmMapFile;

  char *skipMapFile;
  i32 skipMapEnable;
  i32 skipMapBlockUnit;

  i32 roi1AreaEnable;
  i32 roi2AreaEnable;
  i32 roi3AreaEnable;
  i32 roi4AreaEnable;
  i32 roi5AreaEnable;
  i32 roi6AreaEnable;
  i32 roi7AreaEnable;
  i32 roi8AreaEnable;

  i32 roi1AreaTop;
  i32 roi1AreaLeft;
  i32 roi1AreaBottom;
  i32 roi1AreaRight;

  i32 roi2AreaTop;
  i32 roi2AreaLeft;
  i32 roi2AreaBottom;
  i32 roi2AreaRight;

  i32 roi1DeltaQp;
  i32 roi2DeltaQp;
  i32 roi1Qp;
  i32 roi2Qp;

  i32 roi3AreaTop;
  i32 roi3AreaLeft;
  i32 roi3AreaBottom;
  i32 roi3AreaRight;
  i32 roi3DeltaQp;
  i32 roi3Qp;

  i32 roi4AreaTop;
  i32 roi4AreaLeft;
  i32 roi4AreaBottom;
  i32 roi4AreaRight;
  i32 roi4DeltaQp;
  i32 roi4Qp;

  i32 roi5AreaTop;
  i32 roi5AreaLeft;
  i32 roi5AreaBottom;
  i32 roi5AreaRight;
  i32 roi5DeltaQp;
  i32 roi5Qp;

  i32 roi6AreaTop;
  i32 roi6AreaLeft;
  i32 roi6AreaBottom;
  i32 roi6AreaRight;
  i32 roi6DeltaQp;
  i32 roi6Qp;

  i32 roi7AreaTop;
  i32 roi7AreaLeft;
  i32 roi7AreaBottom;
  i32 roi7AreaRight;
  i32 roi7DeltaQp;
  i32 roi7Qp;

  i32 roi8AreaTop;
  i32 roi8AreaLeft;
  i32 roi8AreaBottom;
  i32 roi8AreaRight;
  i32 roi8DeltaQp;
  i32 roi8Qp;

  /* Rate control parameters */
  i32 hrdConformance;
  i32 cpbSize;
  i32 intraPicRate;   /* IDR interval */

  i32 vbr; /* Variable Bit Rate Control by qpMin */
  i32 qpHdr;
  i32 qpMin;
  i32 qpMax;
  i32 qpMinI;
  i32 qpMaxI;
  i32 bitPerSecond;
  i32 crf; /*CRF constant*/

  i32 bitVarRangeI;

  i32 bitVarRangeP;

  i32 bitVarRangeB;
  u32 u32StaticSceneIbitPercent;

  i32 tolMovingBitRate;/*tolerance of max Moving bit rate */
  i32 monitorFrames;/*monitor frame length for moving bit rate*/
  i32 picRc;
  i32 ctbRc;
  i32 blockRCSize;
  u32 rcQpDeltaRange;
  u32 rcBaseMBComplexity;
  i32 picSkip;
  i32 picQpDeltaMin;
  i32 picQpDeltaMax;
  i32 ctbRcRowQpStep;

  float tolCtbRcInter;
  float tolCtbRcIntra;

  i32 bitrateWindow;
  i32 intraQpDelta;
  i32 fixedIntraQp;
  i32 bFrameQpDelta;

  i32 disableDeblocking;

  i32 enableSao;


  i32 tc_Offset;
  i32 beta_Offset;


  i32 chromaQpOffset;

  i32 profile;              /*main profile or main still picture profile*/
  i32 tier;               /*main tier or high tier*/
  i32 level;              /*main profile level*/

  i32 bpsAdjustFrame[MAX_BPS_ADJUST];
  i32 bpsAdjustBitrate[MAX_BPS_ADJUST];
  i32 smoothPsnrInGOP;

  i32 sliceSize;

  i32 testId;

  i32 rotation;
  i32 mirror;
  i32 horOffsetSrc;
  i32 verOffsetSrc;
  i32 colorConversion;
  i32 scaledWidth;
  i32 scaledHeight;
  i32 scaledOutputFormat;

  i32 enableDeblockOverride;
  i32 deblockOverride;

  i32 enableScalingList;

  u32 compressor;

  i32 interlacedFrame;
  i32 fieldOrder;
  i32 videoRange;
  i32 ssim;
  i32 sei;
  char *userData;
  u32 gopSize;
  char *gopCfg;
  u32 gopLowdelay;
  i32 outReconFrame;
  u32 longTermGap;
  u32 longTermGapOffset;
  u32 ltrInterval;
  i32 longTermQpDelta;

  i32 gdrDuration;
  u32 roiMapDeltaQpBlockUnit;
  u32 roiMapDeltaQpEnable;
  char *roiMapDeltaQpFile;
  char *roiMapDeltaQpBinFile;
  char *roiMapInfoBinFile;
  char *RoimapCuCtrlInfoBinFile;
  char *RoimapCuCtrlIndexBinFile;
  u32 RoiCuCtrlVer;
  u32 RoiQpDeltaVer;
  i32 outBufSizeMax;
  i32 multimode;  // Multi-stream mode, 0--disable, 1--mult-thread, 2--multi-process
  char *streamcfg[MAX_STREAMS];
  union {
	  int pid[MAX_STREAMS];
#ifndef _WIN32
	  pthread_t tid[MAX_STREAMS];
#else
	  DWORD tid[MAX_STREAMS];
#endif
  u32 layerInRefIdc;
  };
  struct commandLine_s *streamcml[MAX_STREAMS];
  i32 nstream;

  char **argv;      /* Command line parameter... */
  i32 argc;
  //WIENER_DENOISE
  i32 noiseReductionEnable;
  i32 noiseLow;
  i32 firstFrameSigma;

  i32 bitDepthLuma;
  i32 bitDepthChroma;

  u32 enableOutputCuInfo;

  u32 rdoLevel;
  /* low latency */
  i32 inputLineBufMode;
  i32 inputLineBufDepth;
  i32 amountPerLoopBack;

  u32 hashtype;
  u32 verbose;

  /* for smart */
  i32 smartModeEnable;
  i32 smartH264Qp;
  i32 smartHevcLumQp;
  i32 smartHevcChrQp;
  i32 smartH264LumDcTh;
  i32 smartH264CbDcTh;
  i32 smartH264CrDcTh;
  /* threshold for hevc cu8x8/16x16/32x32 */
  i32 smartHevcLumDcTh[3];
  i32 smartHevcChrDcTh[3];
  i32 smartHevcLumAcNumTh[3];
  i32 smartHevcChrAcNumTh[3];
  /* back ground */
  i32 smartMeanTh[4];
  /* foreground/background threashold: maximum foreground pixels in background block */
  i32 smartPixNumCntTh;

  /* constant chroma control */
  i32 constChromaEn;
  u32 constCb;
  u32 constCr;

  i32 sceneChange[MAX_SCENE_CHANGE];

  /* for tile*/
  i32 tiles_enabled_flag;
  i32 num_tile_columns;
  i32 num_tile_rows;
  i32 loop_filter_across_tiles_enabled_flag;

  /*for skip frame encoding ctr*/
  i32 skip_frame_enabled_flag;
  i32 skip_frame_poc;

  /*stride*/
  u32 exp_of_input_alignment;
  u32 exp_of_ref_alignment;
  u32 exp_of_ref_ch_alignment;

  /* HDR10 */
  u32 hdr10_display_enable;
  u32 hdr10_dx0;
  u32 hdr10_dy0;
  u32 hdr10_dx1;
  u32 hdr10_dy1;
  u32 hdr10_dx2;
  u32 hdr10_dy2;
  u32 hdr10_wx;
  u32 hdr10_wy;
  u32 hdr10_maxluma;
  u32 hdr10_minluma;

  u32 hdr10_lightlevel_enable;
  u32 hdr10_maxlight;
  u32 hdr10_avglight;

  u32 hdr10_color_enable;
  u32 hdr10_primary;
  u32 hdr10_transfer;
  u32 hdr10_matrix;

  u32 RpsInSliceHeader;
  u32 P010RefEnable;
  u32 vui_timing_info_enable;

  u32 picOrderCntType;
  u32 log2MaxPicOrderCntLsb;
  u32 log2MaxFrameNum;

  i16 gmv[2][2];
  char *gmvFileName[2];
  char *halfDsInput;

  u32 parallelCoreNum;
  u32 dumpRegister;
  u32 rasterscan;

  u32 streamBufChain;
  u32 lookaheadDepth;
  u32 streamMultiSegmentMode;
  u32 streamMultiSegmentAmount;
  u32 cuInfoVersion;
  u32 enableRdoQuant;

  u32 extSramLumHeightBwd;
  u32 extSramChrHeightBwd;
  u32 extSramLumHeightFwd;
  u32 extSramChrHeightFwd;

  u32 AXIAlignment;

  u32 mmuEnable;

  u32 ivf;

  u32 MEVertRange;

  /*Overlay*/
  u32 overlayEnables;
  char *olInput[MAX_OVERLAY_NUM];
  u32 olFormat[MAX_OVERLAY_NUM];
  u32 olAlpha[MAX_OVERLAY_NUM];
  u32 olWidth[MAX_OVERLAY_NUM];
  u32 olCropWidth[MAX_OVERLAY_NUM];
  u32 olHeight[MAX_OVERLAY_NUM];
  u32 olCropHeight[MAX_OVERLAY_NUM];
  u32 olXoffset[MAX_OVERLAY_NUM];
  u32 olCropXoffset[MAX_OVERLAY_NUM];
  u32 olYoffset[MAX_OVERLAY_NUM];
  u32 olCropYoffset[MAX_OVERLAY_NUM];
  u32 olYStride[MAX_OVERLAY_NUM];
  u32 olUVStride[MAX_OVERLAY_NUM];
  u32 olBitmapY[MAX_OVERLAY_NUM];
  u32 olBitmapU[MAX_OVERLAY_NUM];
  u32 olBitmapV[MAX_OVERLAY_NUM];

  VCEncChromaIdcType codedChromaIdc;
  u32 PsyFactor;

  u32 aq_mode;
  double aq_strength;

  u32 preset;
  u32 writeReconToDDR;

  u32 TxTypeSearchEnable;
} commandLine_s;

typedef struct {
  u32 streamPos;
  u32 multislice_encoding;
  u32 output_byte_stream;
  FILE *outStreamFile;
} SliceCtl_s;

typedef struct {
  u32 streamRDCounter;
  u32 streamMultiSegEn;
  u8 *streamBase;
  u32 segmentSize;
  u32 segmentAmount;
  FILE *outStreamFile;
  u8 startCodeDone;
  i32 output_byte_stream;
} SegmentCtl_s;

struct test_bench
{
  char *input;
  char *halfDsInput;
  char *output;
  char *test_data_files;
  char *dec400TableFile;
  FILE *in;
  FILE *inDS;
  FILE *out;
  FILE *fmv;
  FILE *dec400Table;
  i32 width;
  i32 height;
  i32 outputRateNumer;      /* Output frame rate numerator */
  i32 outputRateDenom;      /* Output frame rate denominator */
  i32 inputRateNumer;      /* Input frame rate numerator */
  i32 inputRateDenom;      /* Input frame rate denominator */
  i32 firstPic;
  i32 lastPic;
  i32 picture_enc_cnt;
  i32 idr_interval;
  i32 byteStream;
  u8 *lum;
  u8 *cb;
  u8 *cr;
  u8 *lumDS;
  u8 *cbDS;
  u8 *crDS;
  u32 src_img_size_ds;
  i32 interlacedFrame;
  u32 validencodedframenumber;
  u32 input_alignment;
  u32 ref_alignment;
  u32 ref_ch_alignment;
  i32 formatCustomizedType;
  u32 lumaSize;
  u32 chromaSize;
  u32 transformedSize;
  u32 extSramLumBwdSize;
  u32 extSramLumFwdSize;
  u32 extSramChrBwdSize;
  u32 extSramChrFwdSize;
  u32 dec400LumaTableSize;
  u32 dec400ChrTableSize;

  char **argv;      /* Command line parameter... */
  i32 argc;
  /* Moved from global space */
  FILE *yuvFile;
  FILE *roiMapFile;
  FILE *roiMapBinFile;
  FILE *ipcmMapFile;
  FILE *skipMapFile;
  FILE *roiMapInfoBinFile;
  FILE *RoimapCuCtrlInfoBinFile;
  FILE *RoimapCuCtrlIndexBinFile;

  /* SW/HW shared memories for input/output buffers */
  EWLLinearMem_t * pictureMem;
  EWLLinearMem_t * pictureDSMem;
  EWLLinearMem_t * outbufMem[MAX_STRM_BUF_NUM];
  EWLLinearMem_t * Av1PreoutbufMem[MAX_STRM_BUF_NUM];
  EWLLinearMem_t * roiMapDeltaQpMem;
  EWLLinearMem_t * transformMem;
  EWLLinearMem_t * RoimapCuCtrlInfoMem;
  EWLLinearMem_t * RoimapCuCtrlIndexMem;
  EWLLinearMem_t * overlayInputMem[MAX_OVERLAY_NUM];

  EWLLinearMem_t pictureMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t pictureDSMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t outbufMemFactory[MAX_CORE_NUM][MAX_STRM_BUF_NUM]; /* [coreIdx][bufIdx] */
  EWLLinearMem_t Av1PreoutbufMemFactory[MAX_CORE_NUM][MAX_STRM_BUF_NUM]; /* [coreIdx][bufIdx] */
  EWLLinearMem_t roiMapDeltaQpMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t transformMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t RoimapCuCtrlInfoMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t RoimapCuCtrlIndexMemFactory[MAX_DELAY_NUM];
  EWLLinearMem_t extSRAMMemFactory[MAX_CORE_NUM];
  EWLLinearMem_t Dec400CmpTableMem;
  EWLLinearMem_t overlayInputMemFactory[MAX_DELAY_NUM][MAX_OVERLAY_NUM];

  EWLLinearMem_t scaledPictureMem;
  EWLLinearMem_t pictureStabMem;
  float sumsquareoferror;
  float averagesquareoferror;
  i32 maxerrorovertarget;
  i32 maxerrorundertarget;
  long numbersquareoferror;

  u32 gopSize;
  i32 nextGopSize;
  VCEncIn encIn;

  inputLineBufferCfg inputCtbLineBuf;

  FILE *gmvFile[2];

  u32 parallelCoreNum;
  SliceCtl_s sliceCtlFactory[MAX_DELAY_NUM];
  SliceCtl_s *sliceCtl;
  SliceCtl_s *sliceCtlOut;

  i32 streamBufNum;
  u32 frame_delay;
  u32 buffer_cnt;
  SegmentCtl_s streamSegCtl;

  struct timeval timeFrameStart;
  struct timeval timeFrameEnd;

	//Overlay
	FILE *overlayFile[MAX_OVERLAY_NUM];
	u32 olFrameNume[MAX_OVERLAY_NUM];
	u8 olEnable[MAX_OVERLAY_NUM];
};

#define MOVING_AVERAGE_FRAMES    120

typedef struct {
    i32 frame[MOVING_AVERAGE_FRAMES];
    i32 length;
    i32 count;
    i32 pos;
    i32 frameRateNumer;
    i32 frameRateDenom;
} ma_s;

typedef struct {
    int gop_frm_num;
    double sum_intra_vs_interskip;
    double sum_skip_vs_interskip;
    double sum_intra_vs_interskipP;
    double sum_intra_vs_interskipB;
    int sum_costP;
    int sum_costB;
    int last_gopsize;
} adapGopCtr;

#endif
