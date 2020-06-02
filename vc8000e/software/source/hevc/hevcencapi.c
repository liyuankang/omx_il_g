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
--                 The entire notice above must be reproduced                                                  --
--                  on all copies and should not be removed.                                                     --
--                                                                                                                                --
--------------------------------------------------------------------------------
--
--  Abstract : VC Encoder API
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> /* for pow */
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "hevcencapi.h"

#include "base_type.h"
#include "error.h"
#include "instance.h"
#include "sw_parameter_set.h"
#include "rate_control_picture.h"
#include "sw_slice.h"
#include "tools.h"
#include "enccommon.h"
#include "hevcApiVersion.h"
#include "enccfg.h"
#include "hevcenccache.h"
#include "encdec400.h"

#include "sw_put_bits.h"
#include "av1_prob_init.h"

#ifdef INTERNAL_TEST
#include "sw_test_id.h"
#endif

#ifdef TEST_DATA
#include "enctrace.h"
#endif

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
#define VCENC_SW_BUILD ((VCENC_BUILD_MAJOR * 1000000) + \
  (VCENC_BUILD_MINOR * 1000) + VCENC_BUILD_REVISION)

#define PRM_SET_BUFF_SIZE 1024    /* Parameter sets max buffer size */
#define VCENC_MAX_BITRATE             (800000*1000)    /* Level 6.2 high tier limit */

/* HW ID check. VCEncInit() will fail if HW doesn't match. */



#define VCENC_MIN_ENC_WIDTH       64 //132     /* 144 - 12 pixels overfill */
#define VCENC_MAX_ENC_WIDTH       8192
#define VCENC_MIN_ENC_HEIGHT      64 //96
#define VCENC_MAX_ENC_HEIGHT      8192
#define VCENC_MAX_ENC_HEIGHT_EXT  8640
#define VCENC_HEVC_MAX_LEVEL      VCENC_HEVC_LEVEL_6
#define VCENC_H264_MAX_LEVEL      VCENC_H264_LEVEL_6_2

#define VCENC_DEFAULT_QP          26

#define VCENC_MAX_PP_INPUT_WIDTH      8192

#define VCENC_MAX_USER_DATA_SIZE      2048

#define VCENC_BUS_ADDRESS_VALID(bus_address)  (((bus_address) != 0) /*&& \
                                              ((bus_address & 0x0F) == 0)*/)

#define VCENC_BUS_CH_ADDRESS_VALID(bus_address)  (((bus_address) != 0) /*&& \
                                              ((bus_address & 0x07) == 0)*/)

#define FIX_POINT_LAMBDA

typedef enum
{
  RCROIMODE_RC_ROIMAP_ROIAREA_DIABLE = 0,
  RCROIMODE_ROIAREA_ENABLE = 1,
  RCROIMODE_ROIMAP_ENABLE = 2,
  RCROIMODE_ROIMAP_ROIAREA_ENABLE = 3,
  RCROIMODE_RC_ENABLE = 4,
  RCROIMODE_RC_ROIAREA_ENABLE=5,
  RCROIMODE_RC_ROIMAP_ENABLE=6,
  RCROIMODE_RC_ROIMAP_ROIAREA_ENABLE=7

} RCROIMODEENABLE;

/* Tracing macro */
#ifdef HEVCENC_TRACE
#define APITRACE(str) VCEncTrace(str)
#define APITRACEERR(str) VCEncTrace(str)
#define APITRACEPARAM_X(str, val) \
  { char tmpstr[255]; sprintf(tmpstr, "  %s: %p", str, (void *)val); VCEncTrace(tmpstr); }
#define APITRACEPARAM(str, val) \
  { char tmpstr[255]; sprintf(tmpstr, "  %s: %d", str, (int)val); VCEncTrace(tmpstr); }
#else
#define APITRACE(str)
#define APITRACEERR(str) { printf(str); printf("\n"); }
#define APITRACEPARAM_X(str, val)
#define APITRACEPARAM(str, val)
#endif
#define APIPRINT(v, ...) { if(v)printf(__VA_ARGS__); }

const u64 VCEncMaxSBPS[33] =
{
  552960, 3686400, 7372800, 16588800, 33177600, 66846720, 133693440, 267386880,
  534773760, 1069547520, 1069547520, 2139095040, 4278190080
  /* H.264 Level Limits */
  , 1485*256, 1485*256, 3000*256, 6000*256, 11880*256, 11880*256, 19800*256, 20250*256
  , 40500*256, 108000*256, 216000*256, 245760*256, 245760*256
  , 522240*256, 589824*256, 983040*256, 2073600*256
  , 4177920*256, 8355840*256, 16711680ULL*256
};
const u32 VCEncMaxCPBS[33] =
{
  350000, 1500000, 3000000, 6000000, 10000000, 12000000, 20000000,
  25000000, 40000000, 60000000, 60000000, 120000000, 240000000
  /* H.264 Level Limits */
  , 175000, 350000, 500000, 1000000, 2000000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 25000000, 62500000
  , 62500000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};
const u32 VCEncMaxCPBSHighTier[33] =
{
  350000, 1500000, 3000000, 6000000, 10000000, 30000000, 50000000,
  100000000, 160000000, 240000000, 240000000, 480000000, 800000000
  /* H.264 Level Limits */
  , 175000, 350000, 500000, 1000000, 2000000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 25000000, 62500000
  , 62500000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};

const u32 VCEncMaxBR[33] =
{
  350000, 1500000, 3000000, 6000000, 10000000, 12000000, 20000000,
  25000000, 40000000, 60000000, 60000000, 120000000, 240000000
  /* H.264 Level Limits */
  , 64000, 128000, 192000, 384000, 768000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 20000000, 50000000
  , 50000000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};

const u32 VCEncMaxBRHighTier[33] =
{
  350000, 1500000, 3000000, 6000000, 10000000, 30000000, 50000000,
  100000000, 160000000, 240000000, 240000000, 480000000, 800000000
  /* H.264 Level Limits */
  , 64000, 128000, 192000, 384000, 768000, 2000000, 4000000, 4000000
  , 10000000, 14000000, 20000000, 20000000, 50000000
  , 50000000, 135000000, 240000000, 240000000
  , 240000000, 480000000, 800000000
};

const u32 VCEncMaxFS[33] = { 36864, 122880, 245760, 552960, 983040, 2228224, 2228224, 8912896,
                            8912896, 8912896, 35651584, 35651584, 35651584
                            /* H.264 Level Limits */
                            , 99*256, 99*256, 396*256, 396*256, 396*256, 396*256, 792*256, 1620*256
                            , 1620*256, 3600*256, 5120*256, 8192*256, 8192*256
                            , 8704*256, 22080*256, 36864*256, 36864*256
                            , 139264*256, 139264*256, 139264*256
                          };

static const u32 VCEncIntraPenalty[52] =
{
  24, 24, 24, 26, 27, 30, 32, 35, 39, 43, 48, 53, 58, 64, 71, 78,
  85, 93, 102, 111, 121, 131, 142, 154, 167, 180, 195, 211, 229,
  248, 271, 296, 326, 361, 404, 457, 523, 607, 714, 852, 1034,
  1272, 1588, 2008, 2568, 3318, 4323, 5672, 7486, 9928, 13216,
  17648
};//max*3=16bit

static const u32 lambdaSadDepth0Tbl[] =
{
  0x00000100, 0x0000011f, 0x00000143, 0x0000016a, 0x00000196, 0x000001c8,
  0x00000200, 0x0000023f, 0x00000285, 0x000002d4, 0x0000032d, 0x00000390,
  0x00000400, 0x0000047d, 0x0000050a, 0x000005a8, 0x00000659, 0x00000721,
  0x00000800, 0x000008fb, 0x00000a14, 0x00000b50, 0x00000cb3, 0x00000e41,
  0x00001000, 0x000011f6, 0x00001429, 0x000016a1, 0x00001966, 0x00001c82,
  0x00002000, 0x000023eb, 0x00002851, 0x00002d41, 0x000032cc, 0x00003904,
  0x00004000, 0x000047d6, 0x000050a3, 0x00005a82, 0x00006598, 0x00007209,
  0x00008000, 0x00008fad, 0x0000a145, 0x0000b505, 0x0000cb30, 0x0000e412,
  0x00010000, 0x00011f5a, 0x0001428a, 0x00016a0a
};

static const u32 lambdaSseDepth0Tbl[] =
{
  0x00000040, 0x00000051, 0x00000066, 0x00000080, 0x000000a1, 0x000000cb,
  0x00000100, 0x00000143, 0x00000196, 0x00000200, 0x00000285, 0x0000032d,
  0x00000400, 0x0000050a, 0x00000659, 0x00000800, 0x00000a14, 0x00000cb3,
  0x00001000, 0x00001429, 0x00001966, 0x00002000, 0x00002851, 0x000032cc,
  0x00004000, 0x000050a3, 0x00006598, 0x00008000, 0x0000a145, 0x0000cb30,
  0x00010000, 0x0001428a, 0x00019660, 0x00020000, 0x00028514, 0x00032cc0,
  0x00040000, 0x00050a29, 0x00065980, 0x00080000, 0x000a1451, 0x000cb2ff,
  0x00100000, 0x001428a3, 0x001965ff, 0x00200000, 0x00285146, 0x0032cbfd,
  0x00400000, 0x0050a28c, 0x006597fb, 0x00800000
};

static const u32 lambdaSadDepth1Tbl[] =
{
  0x0000016a, 0x00000196, 0x000001c8, 0x00000200, 0x0000023f, 0x00000285,
  0x000002d4, 0x0000032d, 0x00000390, 0x00000400, 0x0000047d, 0x0000050a,
  0x000005a8, 0x00000659, 0x00000721, 0x00000800, 0x000008fb, 0x00000a14,
  0x00000b50, 0x00000cb3, 0x00000e41, 0x00001000, 0x000011f6, 0x00001429,
  0x000016a1, 0x00001a6f, 0x00001ecb, 0x000023c7, 0x0000297a, 0x00002ffd,
  0x0000376d, 0x00003feb, 0x0000499c, 0x000054aa, 0x00006145, 0x00006fa2,
  0x00008000, 0x00008fad, 0x0000a145, 0x0000b505, 0x0000cb30, 0x0000e412,
  0x00010000, 0x00011f5a, 0x0001428a, 0x00016a0a, 0x00019660, 0x0001c824,
  0x00020000, 0x00023eb3, 0x00028514, 0x0002d414
};

static const u32 lambdaSseDepth1Tbl[] =
{
  0x00000080, 0x000000a1, 0x000000cb, 0x00000100, 0x00000143, 0x00000196,
  0x00000200, 0x00000285, 0x0000032d, 0x00000400, 0x0000050a, 0x00000659,
  0x00000800, 0x00000a14, 0x00000cb3, 0x00001000, 0x00001429, 0x00001966,
  0x00002000, 0x00002851, 0x000032cc, 0x00004000, 0x000050a3, 0x00006598,
  0x00008000, 0x0000aeb6, 0x0000ed0d, 0x00014000, 0x0001ae0e, 0x00023fb3,
  0x00030000, 0x0003fd60, 0x00054a95, 0x00070000, 0x00093d4b, 0x000c2b8a,
  0x00100000, 0x001428a3, 0x001965ff, 0x00200000, 0x00285146, 0x0032cbfd,
  0x00400000, 0x0050a28c, 0x006597fb, 0x00800000, 0x00a14518, 0x00cb2ff5,
  0x01000000, 0x01428a30, 0x01965fea, 0x02000000
};

static const float sqrtPow2QpDiv3[] = {
  /* for computing lambda_sad
   * for(int qp = 0; qp <= 63; qp++)
   *   sqrtPow2QpDiv3[i] = sqrt(pow(2.0, (qp-12)/3.0));
   */
  0.250000000, 0.280615512, 0.314980262, 0.353553391, 0.396850263, 0.445449359,
  0.500000000, 0.561231024, 0.629960525, 0.707106781, 0.793700526, 0.890898718,
  1.000000000, 1.122462048, 1.259921050, 1.414213562, 1.587401052, 1.781797436,
  2.000000000, 2.244924097, 2.519842100, 2.828427125, 3.174802104, 3.563594873,
  4.000000000, 4.489848193, 5.039684200, 5.656854249, 6.349604208, 7.127189745,
  8.000000000, 8.979696386, 10.079368399, 11.313708499, 12.699208416, 14.254379490,
  16.000000000, 17.959392773, 20.158736798, 22.627416998, 25.398416831, 28.508758980,
  32.000000000, 35.918785546, 40.317473597, 45.254833996, 50.796833663, 57.017517961,
  64.000000000, 71.837571092, 80.634947193, 90.509667992, 101.593667326, 114.035035922,
  128.000000000, 143.675142184, 161.269894387, 181.019335984, 203.187334652, 228.070071844,
  256.000000000, 287.350284367, 322.539788773, 362.038671968
};
static const float sqrtQpDiv6[] = {
  /* for computing lambda_sad
   * for(int qp = 0; qp <= 63; qp++)
   *   sqrtQpDiv6[i] = sqrt(CLIP3(2.0, 4.0, (qp-12) / 6.0));
   */
  1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562,
  1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562,
  1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.414213562, 1.471960144, 1.527525232, 1.581138830, 1.632993162, 1.683250823,
  1.732050808, 1.779513042, 1.825741858, 1.870828693, 1.914854216, 1.957890021, 2.000000000, 2.000000000, 2.000000000, 2.000000000,
  2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000,
  2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000, 2.000000000,
  2.000000000, 2.000000000, 2.000000000, 2.000000000
};

static u32 av1_level_idc[] = { (0 << 2) | 0, (0 << 2) | 1, (0 << 2) | 2, (0 << 2) | 3,
                               (1 << 2) | 0, (1 << 2) | 1, (1 << 2) | 2, (1 << 2) | 3,
                               (2 << 2) | 0, (2 << 2) | 1, (2 << 2) | 2, (2 << 2) | 3,
                               (3 << 2) | 0, (3 << 2) | 1, (3 << 2) | 2, (3 << 2) | 3,
                               (4 << 2) | 0, (4 << 2) | 1, (4 << 2) | 2, (4 << 2) | 3,
                               (5 << 2) | 0, (5 << 2) | 1, (5 << 2) | 2, (5 << 2) | 3};

bool multipass_enabled = HANTRO_FALSE;

u32 PsyFactor = 0;

#ifdef CDEF_BLOCK_STRENGTH_ENABLE
// [pass][luma/chroma][QP][strength]
u32 cdef_stats[2][2][64];

// [pass][luma_strength][chroma_strength]
u32 cdef_union[2][64][64];

// [pass][index]
u8 cdef_y_strenth[2][8];
u8 cdef_uv_strenth[2][8];
#endif

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static i32 out_of_memory(struct vcenc_buffer *n, u32 size);
static i32 get_buffer(struct buffer *, struct vcenc_buffer *, i32 size, i32 reset);
static i32 init_buffer(struct vcenc_buffer *, struct vcenc_instance *);
static i32 set_parameter(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn, struct vps *v, struct sps *s, struct pps *p);
static bool_e VCEncCheckCfg(const VCEncConfig *pEncCfg);
static bool_e SetParameter(struct vcenc_instance *inst, const VCEncConfig   *pEncCfg);
static i32 vcenc_set_ref_pic_set(struct vcenc_instance *vcenc_instance);
static i32 vcenc_ref_pic_sets(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn);
static i32 VCEncGetAllowedWidth(i32 width, VCEncPictureType inputType);
static void IntraLamdaQpFixPoint(int qp, u32 *lamda_sad, enum slice_type type, int poc, u32 qpFactorSad, bool depth0);
static void InterLamdaQpFixPoint(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad, enum slice_type type, u32 qpFactorSad, u32 qpFactorSse, bool depth0, u32 asicId);
static void IntraLamdaQp(int qp, u32 *lamda_sad, enum slice_type type, int poc, float dQPFactor, bool depth0);
static void InterLamdaQp(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad, enum slice_type type, float dQPFactor, bool depth0, u32 asicId);
static void FillIntraFactor(regValues_s *regs);
static void LamdaTableQp(regValues_s *regs, int qp, enum slice_type type, int poc, double dQPFactor, bool depth0, bool ctbRc);
static void VCEncSetQuantParameters(asicData_s *asic, double qp_factor, enum slice_type type, bool is_depth0, true_e enable_ctu_rc);
static void VCEncSetNewFrame(VCEncInst inst);

/* DeNoise Filter */
static void VCEncHEVCDnfInit(struct vcenc_instance *vcenc_instance);
static void VCEncHEVCDnfSetParameters(struct vcenc_instance *vcenc_instance, const VCEncCodingCtrl *pCodeParams);
static void VCEncHEVCDnfGetParameters(struct vcenc_instance *vcenc_instance, VCEncCodingCtrl *pCodeParams);
static void VCEncHEVCDnfPrepare(struct vcenc_instance *vcenc_instance);
static void VCEncHEVCDnfUpdate(struct vcenc_instance *vcenc_instance);

/* RPS in slice header*/
static void VCEncGenSliceHeaderRps(struct vcenc_instance *vcenc_instance, VCEncPictureCodingType codingType, VCEncGopPicConfig *cfg);
static void VCEncGenPicRefConfig(struct container *c, VCEncGopPicConfig *cfg, struct sw_picture *pCurPic, i32 curPicPoc);

/*  av1 */
static void VCEncInitAV1(const VCEncConfig *config, VCEncInst inst);
static void VCEncUpdateRefPicInfo(VCEncInst inst, const VCEncIn *pEncIn, VCEncPictureCodingType codingType);
static void av1_update_reference_frames(VCEncInst inst, const VCEncIn *pEncIn);
static VCEncRet VCEncGenAV1Config(VCEncInst inst, const VCEncIn *pEncIn, struct sw_picture *pic, VCEncPictureCodingType codingType);
static void ref_idx_markingAv1(VCEncInst inst, struct container *c, struct sw_picture *pic, i32 curPoc);
void VCEncFindPicToDisplay(VCEncInst inst, const VCEncIn *pEncIn);
void preserve_last3_av1(VCEncInst inst, struct container *c, const VCEncIn *pEncIn);
extern i32 get_ref_frame_map_idx(struct vcenc_instance *vcenc_instance, const i32 ref_frame);

/*------------------------------------------------------------------------------

    Function name : VCEncGetApiVersion
    Description   : Return the API version info

    Return type   : VCEncApiVersion
    Argument      : void
------------------------------------------------------------------------------*/
VCEncApiVersion VCEncGetApiVersion(void)
{
  VCEncApiVersion ver;

  ver.major = VCENC_MAJOR_VERSION;
  ver.minor = VCENC_MINOR_VERSION;

  APITRACE("VCEncGetApiVersion# OK");
  return ver;
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetBuild
    Description   : Return the SW and HW build information

    Return type   : VCEncBuild
    Argument      : void
------------------------------------------------------------------------------*/
VCEncBuild VCEncGetBuild(u32 core_id)
{
  VCEncBuild ver;

  ver.swBuild = VCENC_SW_BUILD;
  ver.hwBuild = EncAsicGetAsicHWid(core_id);

  APITRACE("VCEncGetBuild# OK");

  return (ver);
}

/*------------------------------------------------------------------------------
  VCEncInit initializes the encoder and creates new encoder instance.
------------------------------------------------------------------------------*/
VCEncRet VCEncInit(const VCEncConfig *config, VCEncInst *instAddr)
{
  struct instance *instance;
  const void *ewl = NULL;
  EWLInitParam_t param;
  struct vcenc_instance *vcenc_inst = NULL;
  struct container *c;
  u32 client_type;
  asicMemAlloc_s allocCfg;
  i32 parallelCoreNum = config->parallelCoreNum;

  regValues_s *regs;
  preProcess_s *pPP;
  struct vps *v;
  struct sps *s;
  struct pps *p;
  VCEncRet ret;
  int i;
  APITRACE("VCEncInit#");

  APITRACEPARAM("streamType", config->streamType);
  APITRACEPARAM("profile", config->profile);
  APITRACEPARAM("level", config->level);
  APITRACEPARAM("width", config->width);
  APITRACEPARAM("height", config->height);
  APITRACEPARAM("frameRateNum", config->frameRateNum);
  APITRACEPARAM("frameRateDenom", config->frameRateDenom);
  APITRACEPARAM("refFrameAmount", config->refFrameAmount);
  APITRACEPARAM("strongIntraSmoothing", config->strongIntraSmoothing);
  APITRACEPARAM("compressor", config->compressor);
  APITRACEPARAM("interlacedFrame", config->interlacedFrame);
  APITRACEPARAM("bitDepthLuma", config->bitDepthLuma);
  APITRACEPARAM("bitDepthChroma", config->bitDepthChroma);
  APITRACEPARAM("outputCuInfo", config->enableOutputCuInfo);
  APITRACEPARAM("SSIM", config->enableSsim);
  APITRACEPARAM("ctbRcMode", config->ctbRcMode);

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  /* Check for illegal inputs */
  if (config == NULL || instAddr == NULL)
  {
    APITRACEERR("VCEncInit: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for correct HW */
  client_type = IS_H264(config->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
  if (((HW_ID_PRODUCT(EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type)))) != HW_ID_PRODUCT_H2) && ((HW_ID_PRODUCT(EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type)))) != HW_ID_PRODUCT_VC8000E))
  {
    APITRACEERR("VCEncInit: ERROR Invalid HW ID");
    return VCENC_ERROR;
  }

  /* Check that configuration is valid */
  if (VCEncCheckCfg(config) == ENCHW_NOK)
  {
    APITRACEERR("VCEncInit: ERROR Invalid configuration");
    return VCENC_INVALID_ARGUMENT;
  }

  if (parallelCoreNum > 1 && config->width * config->height < 256*256) {
    APITRACE("VCEncInit: Use single core for small resolution");
    parallelCoreNum = 1;
  }

  param.clientType = IS_H264(config->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
  param.mmuEnable = config->mmuEnable;

  /* Init EWL */
  if ((ewl = EWLInit(&param)) == NULL)
  {
    APITRACEERR("VCEncInit: EWL Initialization failed");
    return VCENC_EWL_ERROR;
  }


  *instAddr = NULL;
  if (!(instance = (struct instance *)EWLcalloc(1, sizeof(struct instance))))
  {
    ret = VCENC_MEMORY_ERROR;
    goto error;
  }

  instance->instance = (struct vcenc_instance *)instance;
  *instAddr = (VCEncInst)instance;
  ASSERT(instance == (struct instance *)&instance->vcenc_instance);
  vcenc_inst = (struct vcenc_instance *)instance;
  if (!(c = get_container(vcenc_inst)))
  {
    ret = VCENC_MEMORY_ERROR;
    goto error;
  }

  /* Create parameter sets */
  v = (struct vps *)create_parameter_set(VPS_NUT);
  s = (struct sps *)create_parameter_set(SPS_NUT);
  p = (struct pps *)create_parameter_set(PPS_NUT);

  /* Replace old parameter set with new ones */
  remove_parameter_set(c, VPS_NUT, 0);
  remove_parameter_set(c, SPS_NUT, 0);
  remove_parameter_set(c, PPS_NUT, 0);

  queue_put(&c->parameter_set, (struct node *)v);
  queue_put(&c->parameter_set, (struct node *)s);
  queue_put(&c->parameter_set, (struct node *)p);
  vcenc_inst->sps = s;
  vcenc_inst->vps = v;
  vcenc_inst->interlaced = config->interlacedFrame;

  vcenc_inst->insertNewPPS = 0;
  vcenc_inst->maxPPSId = 0;

  #ifdef RECON_REF_1KB_BURST_RW
  /*
   1KB_BURST_RW feature enabled, aligment is fixed not changable
   User must set the config correct, API internal not trick to change it
  */
  ASSERT( (config->exp_of_input_alignment == 10) && (config->exp_of_ref_alignment == 10) &&
                  (config->exp_of_ref_ch_alignment==10) && (config->compressor ==2));
  #endif

  /* Initialize ASIC */

  vcenc_inst->asic.ewl = ewl;
  (void) EncAsicControllerInit(&vcenc_inst->asic,param.clientType);

  EWLHwConfig_t asic_cfg = EncAsicGetAsicConfig(0);
  u32 lookaheadDepth = config->lookaheadDepth;
  u32 pass = config->pass;
  if(lookaheadDepth > 0 && (asic_cfg.cuInforVersion < 1)) {
    // lookahead needs bFrame support & cuInfo version 1
    lookaheadDepth = 0;
    pass = 0;
  }

  vcenc_inst->width = config->width;
  vcenc_inst->height = config->height / (vcenc_inst->interlaced + 1);


  vcenc_inst->input_alignment = 1<<config->exp_of_input_alignment;
  vcenc_inst->ref_alignment = 1<<config->exp_of_ref_alignment;
  vcenc_inst->ref_ch_alignment = 1<<config->exp_of_ref_ch_alignment;

  vcenc_inst->dumpRegister = config->dumpRegister;
  vcenc_inst->rasterscan = config->rasterscan;

  vcenc_inst->writeReconToDDR = config->writeReconToDDR;

  /* Allocate internal SW/HW shared memories */
  memset(&allocCfg, 0, sizeof (asicMemAlloc_s));
  allocCfg.width = config->width;
  allocCfg.height = config->height / (vcenc_inst->interlaced + 1);
  allocCfg.encodingType = (IS_H264(config->codecFormat)? ASIC_H264 : (IS_HEVC(config->codecFormat)? ASIC_HEVC: IS_VP9(config->codecFormat)?ASIC_VP9:ASIC_AV1));
  allocCfg.numRefBuffsLum = allocCfg.numRefBuffsChr = config->refFrameAmount + parallelCoreNum - 1;
  allocCfg.compressor = config->compressor;
  allocCfg.outputCuInfo = config->enableOutputCuInfo;
  allocCfg.bitDepthLuma = config->bitDepthLuma;
  allocCfg.bitDepthChroma = config->bitDepthChroma;
  allocCfg.input_alignment = vcenc_inst->input_alignment;
  allocCfg.ref_alignment = vcenc_inst->ref_alignment;
  allocCfg.ref_ch_alignment = vcenc_inst->ref_ch_alignment;
  allocCfg.exteralReconAlloc = config->exteralReconAlloc;
  allocCfg.maxTemporalLayers = config->maxTLayers;
  allocCfg.ctbRcMode = config->ctbRcMode;
  allocCfg.numCuInfoBuf = vcenc_inst->numCuInfoBuf = (pass == 1 && asic_cfg.bMultiPassSupport ? lookaheadDepth+X265_BFRAME_MAX : 2) + parallelCoreNum;
  vcenc_inst->cuInfoBufIdx = 0;
  vcenc_inst->asic.regs.P010RefEnable = config->P010RefEnable;
  if (config->P010RefEnable) {
    if(config->bitDepthLuma > 8 && (config->compressor & 1) == 0)
      allocCfg.bitDepthLuma = 16;
    if(config->bitDepthChroma > 8 && (config->compressor & 2) == 0)
      allocCfg.bitDepthChroma = 16;
  }
  // TODO,internal memory size alloc according to codedChromaIdc
  allocCfg.codedChromaIdc = (vcenc_inst->asic.regs.asicCfg.MonoChromeSupport == EWL_HW_CONFIG_NOT_SUPPORTED) ? VCENC_CHROMA_IDC_420 : config->codedChromaIdc;
  if (EncAsicMemAlloc_V2(&vcenc_inst->asic, &allocCfg) != ENCHW_OK)
  {
    ret = VCENC_EWL_MEMORY_ERROR;
    goto error;
  }

  /* Set parameters depending on user config */
  if (SetParameter(vcenc_inst, config) != ENCHW_OK)
  {
    ret = VCENC_INVALID_ARGUMENT;
    goto error;
  }
  vcenc_inst->rateControl.codingType = ASIC_HEVC;
  vcenc_inst->rateControl.monitorFrames = MAX(LEAST_MONITOR_FRAME, config->frameRateNum / config->frameRateDenom);
  vcenc_inst->rateControl.picRc = ENCHW_NO;
  vcenc_inst->rateControl.picSkip = ENCHW_NO;
  vcenc_inst->rateControl.qpHdr = -1<<QP_FRACTIONAL_BITS;
  vcenc_inst->rateControl.ctbRcQpDeltaReverse = 0;
  vcenc_inst->rateControl.qpMin = vcenc_inst->rateControl.qpMinI = vcenc_inst->rateControl.qpMinPB = 0<<QP_FRACTIONAL_BITS;
  vcenc_inst->rateControl.qpMax = vcenc_inst->rateControl.qpMaxI = vcenc_inst->rateControl.qpMaxPB = 51<<QP_FRACTIONAL_BITS;
  vcenc_inst->rateControl.hrd = ENCHW_NO;
  vcenc_inst->rateControl.virtualBuffer.bitRate = 1000000;
  vcenc_inst->rateControl.virtualBuffer.bufferSize = 1000000;
  vcenc_inst->rateControl.bitrateWindow = 150;
  vcenc_inst->rateControl.vbr = ENCHW_NO;
  vcenc_inst->rateControl.crf = -1;
  vcenc_inst->rateControl.pbOffset = 6.0 * 0.378512;
  vcenc_inst->rateControl.ipOffset = 6.0 * 0.485427;

  vcenc_inst->rateControl.intraQpDelta = INTRA_QPDELTA_DEFAULT << QP_FRACTIONAL_BITS;
  vcenc_inst->rateControl.fixedIntraQp = 0 << QP_FRACTIONAL_BITS;
  vcenc_inst->rateControl.tolMovingBitRate = 2000;
  vcenc_inst->rateControl.tolCtbRcInter = 0.0;
  vcenc_inst->rateControl.tolCtbRcIntra = -1.0;
  vcenc_inst->rateControl.longTermQpDelta = 0;
  vcenc_inst->rateControl.f_tolMovingBitRate = 2000.0;
  vcenc_inst->rateControl.smooth_psnr_in_gop = 0;

  vcenc_inst->rateControl.maxPicSizeI = MIN( (((i64)rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum))*(1 + 20)),I32_MAX);
  vcenc_inst->rateControl.maxPicSizeP = MIN( (((i64)rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum))*(1 + 20)),I32_MAX);
  vcenc_inst->rateControl.maxPicSizeB = MIN( (((i64)rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum))*(1 + 20)),I32_MAX);

  vcenc_inst->rateControl.minPicSizeI = rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum)/(1+20) ;
  vcenc_inst->rateControl.minPicSizeP = rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum)/(1+20) ;
  vcenc_inst->rateControl.minPicSizeB = rcCalculate(vcenc_inst->rateControl.virtualBuffer.bitRate, vcenc_inst->rateControl.outRateDenom, vcenc_inst->rateControl.outRateNum)/(1+20) ;
  vcenc_inst->blockRCSize = 0;
  vcenc_inst->rateControl.rcQpDeltaRange = 10;
  vcenc_inst->rateControl.rcBaseMBComplexity = 15;
  vcenc_inst->rateControl.picQpDeltaMin = -2;
  vcenc_inst->rateControl.picQpDeltaMax = 3;
  for (i = 0; i < 3; i ++)
  {
    vcenc_inst->rateControl.ctbRateCtrl.models[i].ctbMemPreAddr        = vcenc_inst->asic.ctbRcMem[i].busAddress;
    vcenc_inst->rateControl.ctbRateCtrl.models[i].ctbMemPreVirtualAddr = vcenc_inst->asic.ctbRcMem[i].virtualAddress;
  }
  vcenc_inst->rateControl.ctbRateCtrl.ctbMemCurAddr        = vcenc_inst->asic.ctbRcMem[3].busAddress;
  vcenc_inst->rateControl.ctbRateCtrl.ctbMemCurVirtualAddr = vcenc_inst->asic.ctbRcMem[3].virtualAddress;

  // ctbRc for rate purpose
  if (IS_CTBRC_FOR_BITRATE(allocCfg.ctbRcMode))
  {
    i32 rowQpStep = IS_H264(vcenc_inst->codecFormat)  ? 4 : 16;
    vcenc_inst->rateControl.ctbRateCtrl.qpStep =
        ((rowQpStep << CTB_RC_QP_STEP_FIXP) + vcenc_inst->rateControl.ctbCols/2) / vcenc_inst->rateControl.ctbCols;
  }
  vcenc_inst->rateControl.pass = vcenc_inst->pass = pass;
  multipass_enabled = (pass > 0);

  if (VCEncInitRc(&vcenc_inst->rateControl, 1) != ENCHW_OK)
  {
    ret = VCENC_INVALID_ARGUMENT;
    goto error;
  }

  vcenc_inst->rateControl.sei.enabled = ENCHW_NO;
  vcenc_inst->sps->vui.videoFullRange = ENCHW_NO;
  vcenc_inst->disableDeblocking = 0;
  vcenc_inst->tc_Offset = -2;
  vcenc_inst->beta_Offset = 5;
  vcenc_inst->enableSao = 1;
  vcenc_inst->enableScalingList = 0;
  vcenc_inst->sarWidth = 0;
  vcenc_inst->sarHeight = 0;

  vcenc_inst->max_cu_size = 64;   /* Max coding unit size in pixels */
  vcenc_inst->min_cu_size = 8;   /* Min coding unit size in pixels */
  vcenc_inst->max_tr_size = 16;   /* Max transform size in pixels */ /*Default max 16, assume no TU32 support*/
  vcenc_inst->min_tr_size = 4;   /* Min transform size in pixels */

  VCEncHEVCDnfInit(vcenc_inst);

  vcenc_inst->tr_depth_intra = 2;   /* Max transform hierarchy depth */
  vcenc_inst->tr_depth_inter = (vcenc_inst->max_cu_size == 64) ? 4 : 3;   /* Max transform hierarchy depth */
  vcenc_inst->codecFormat = config->codecFormat;
  if(IS_H264(vcenc_inst->codecFormat) ) {
    vcenc_inst->max_cu_size = 16;   /* Max coding unit size in pixels */
    vcenc_inst->min_cu_size = 8;   /* Min coding unit size in pixels */
    vcenc_inst->max_tr_size = 16;   /* Max transform size in pixels */
    vcenc_inst->min_tr_size = 4;   /* Min transform size in pixels */
    vcenc_inst->tr_depth_intra = 1;   /* Max transform hierarchy depth */
    vcenc_inst->tr_depth_inter = 2;   /* Max transform hierarchy depth */
  }

  vcenc_inst->fieldOrder = 0;
  vcenc_inst->chromaQpOffset = (IS_AV1(vcenc_inst->codecFormat) ? 1 : 0);
  vcenc_inst->rateControl.sei.insertRecoveryPointMessage = 0;
  vcenc_inst->rateControl.sei.recoveryFrameCnt = 0;
  vcenc_inst->min_qp_size = vcenc_inst->max_cu_size;

  /* low latency */
  vcenc_inst->inputLineBuf.inputLineBufDepth = 1;

  /* smart */
  vcenc_inst->smartModeEnable = 0;
  vcenc_inst->smartH264LumDcTh = 5;
  vcenc_inst->smartH264CbDcTh = 1;
  vcenc_inst->smartH264CrDcTh = 1;
  vcenc_inst->smartHevcLumDcTh[0] = 2;
  vcenc_inst->smartHevcLumDcTh[1] = 2;
  vcenc_inst->smartHevcLumDcTh[2] = 2;
  vcenc_inst->smartHevcChrDcTh[0] = 2;
  vcenc_inst->smartHevcChrDcTh[1] = 2;
  vcenc_inst->smartHevcChrDcTh[2] = 2;
  vcenc_inst->smartHevcLumAcNumTh[0] = 12;
  vcenc_inst->smartHevcLumAcNumTh[1] = 51;
  vcenc_inst->smartHevcLumAcNumTh[2] = 204;
  vcenc_inst->smartHevcChrAcNumTh[0] = 3;
  vcenc_inst->smartHevcChrAcNumTh[1] = 12;
  vcenc_inst->smartHevcChrAcNumTh[2] = 51;
  vcenc_inst->smartH264Qp = 30;
  vcenc_inst->smartHevcLumQp = 30;
  vcenc_inst->smartHevcChrQp = 30;
  vcenc_inst->smartMeanTh[0] = 5;
  vcenc_inst->smartMeanTh[1] = 5;
  vcenc_inst->smartMeanTh[2] = 5;
  vcenc_inst->smartMeanTh[3] = 5;
  vcenc_inst->smartPixNumCntTh = 0;

  vcenc_inst->verbose = config->verbose;

  vcenc_inst->tiles_enabled_flag = 0;
  vcenc_inst->num_tile_columns = 1;
  vcenc_inst->num_tile_rows = 1;
  vcenc_inst->loop_filter_across_tiles_enabled_flag =1;

  regs = &vcenc_inst->asic.regs;

  regs->LatencyOfDdrRAW = 0;
  regs->cabac_init_flag = 0;
  regs->entropy_coding_mode_flag = 1;
  regs->cirStart = 0;
  regs->cirInterval = 0;
  regs->intraAreaTop = regs->intraAreaLeft =
                         regs->intraAreaBottom = regs->intraAreaRight = INVALID_POS;

  regs->ipcm1AreaTop = regs->ipcm1AreaLeft =
                         regs->ipcm1AreaBottom = regs->ipcm1AreaRight = INVALID_POS;
  regs->ipcm2AreaTop = regs->ipcm2AreaLeft =
                         regs->ipcm2AreaBottom = regs->ipcm2AreaRight = INVALID_POS;

  regs->roi1Top = regs->roi1Left =
                    regs->roi1Bottom = regs->roi1Right = INVALID_POS;
  vcenc_inst->roi1Enable = 0;

  regs->roi2Top = regs->roi2Left =
                    regs->roi2Bottom = regs->roi2Right = INVALID_POS;
  vcenc_inst->roi2Enable = 0;

  regs->roi3Top = regs->roi3Left =
                    regs->roi3Bottom = regs->roi3Right = INVALID_POS;
  vcenc_inst->roi3Enable = 0;

  regs->roi4Top = regs->roi4Left =
                    regs->roi4Bottom = regs->roi4Right = INVALID_POS;
  vcenc_inst->roi4Enable = 0;

  regs->roi5Top = regs->roi5Left =
                    regs->roi5Bottom = regs->roi5Right = INVALID_POS;
  vcenc_inst->roi5Enable = 0;

  regs->roi6Top = regs->roi6Left =
                    regs->roi6Bottom = regs->roi6Right = INVALID_POS;
  vcenc_inst->roi6Enable = 0;

  regs->roi7Top = regs->roi7Left =
                    regs->roi7Bottom = regs->roi7Right = INVALID_POS;
  vcenc_inst->roi7Enable = 0;


  regs->roi8Top = regs->roi8Left =
                    regs->roi8Bottom = regs->roi8Right = INVALID_POS;
  vcenc_inst->roi8Enable = 0;

  regs->roi1DeltaQp = 0;
  regs->roi2DeltaQp = 0;
  regs->roi3DeltaQp = 0;
  regs->roi4DeltaQp = 0;
  regs->roi5DeltaQp = 0;
  regs->roi6DeltaQp = 0;
  regs->roi7DeltaQp = 0;
  regs->roi8DeltaQp = 0;
  regs->roi1Qp = -1;
  regs->roi2Qp = -1;
  regs->roi3Qp = -1;
  regs->roi4Qp = -1;
  regs->roi5Qp = -1;
  regs->roi6Qp = -1;
  regs->roi7Qp = -1;
  regs->roi8Qp = -1;
  regs->ssim = config->enableSsim;

  /* External line buffer settings */
  regs->sram_linecnt_lum_fwd = config->extSramLumHeightFwd;
  regs->sram_linecnt_lum_bwd = config->extSramLumHeightBwd;
  regs->sram_linecnt_chr_fwd = config->extSramChrHeightFwd;
  regs->sram_linecnt_chr_bwd = config->extSramChrHeightBwd;

  /* AXI alignment */
  regs->AXI_burst_align_wr_common = (config->AXIAlignment >> 28) & 0xf;
  regs->AXI_burst_align_wr_stream = (config->AXIAlignment >> 24) & 0xf;
  regs->AXI_burst_align_wr_chroma_ref = (config->AXIAlignment >> 20) & 0xf;
  regs->AXI_burst_align_wr_luma_ref = (config->AXIAlignment >> 16) & 0xf;
  regs->AXI_burst_align_rd_common = (config->AXIAlignment >> 12) & 0xf;
  regs->AXI_burst_align_rd_prp = (config->AXIAlignment >> 8) & 0xf;
  regs->AXI_burst_align_rd_ch_ref_prefetch = (config->AXIAlignment >> 4) & 0xf;
  regs->AXI_burst_align_rd_lu_ref_prefetch = (config->AXIAlignment) & 0xf;

  vcenc_inst->pcm_enabled_flag = 0;
  vcenc_inst->pcm_loop_filter_disabled_flag = 0;

  vcenc_inst->roiMapEnable = 0;
  vcenc_inst->RoimapCuCtrl_index_enable = 0;
  vcenc_inst->RoimapCuCtrl_enable       = 0;
  vcenc_inst->RoimapCuCtrl_ver = 0;
  vcenc_inst->RoiQpDelta_ver = 0;
  vcenc_inst->ctbRCEnable = 0;
  regs->rcRoiEnable = 0x00;
  regs->diffCuQpDeltaDepth = 0;
  /* Status == INIT   Initialization succesful */
  vcenc_inst->encStatus = VCENCSTAT_INIT;
  vcenc_inst->created_pic_num = 0;

  pPP = &(vcenc_inst->preProcess);
  pPP->constChromaEn = 0;
  pPP->constCb = pPP->constCr = 0x80 << (config->bitDepthChroma-8);
  /* HDR10 */
  vcenc_inst->Hdr10Display.hdr10_display_enable = 0;
  vcenc_inst->Hdr10Display.hdr10_dx0 = 0;
  vcenc_inst->Hdr10Display.hdr10_dy0 = 0;
  vcenc_inst->Hdr10Display.hdr10_dx1 = 0;
  vcenc_inst->Hdr10Display.hdr10_dy1 = 0;
  vcenc_inst->Hdr10Display.hdr10_dx2 = 0;
  vcenc_inst->Hdr10Display.hdr10_dy2 = 0;
  vcenc_inst->Hdr10Display.hdr10_wx = 0;
  vcenc_inst->Hdr10Display.hdr10_wy = 0;
  vcenc_inst->Hdr10Display.hdr10_maxluma = 0;
  vcenc_inst->Hdr10Display.hdr10_minluma = 0;

  vcenc_inst->Hdr10Color.hdr10_color_enable = 0;
  vcenc_inst->Hdr10Color.hdr10_primary = 9;
  vcenc_inst->Hdr10Color.hdr10_transfer = 0;
  vcenc_inst->Hdr10Color.hdr10_matrix = 9;

  vcenc_inst->Hdr10LightLevel.hdr10_lightlevel_enable = 0;
  vcenc_inst->Hdr10LightLevel.hdr10_maxlight = 0;
  vcenc_inst->Hdr10LightLevel.hdr10_avglight = 0;

  vcenc_inst->RpsInSliceHeader = 0;

  /* Multi-core */
  vcenc_inst->parallelCoreNum = parallelCoreNum;
  vcenc_inst->reservedCore =0;

  /* Multi-pass */
  vcenc_inst->bSkipCabacEnable = 0;
  vcenc_inst->bMotionScoreEnable = 0;
  vcenc_inst->extDSRatio = config->extDSRatio;
  if(vcenc_inst->pass == 1) {
    ret = cuTreeInit(&vcenc_inst->cuTreeCtl, vcenc_inst, config);

    if (ret != VCENC_OK) {
      APITRACE("VCEncInit: cuTreeInit failed");
      goto error;
    }

    //skip CABAC engine for better througput in first pass
    vcenc_inst->bSkipCabacEnable = regs->asicCfg.bMultiPassSupport;
    //enable motion score computing for agop decision in first pass
    vcenc_inst->bMotionScoreEnable = regs->asicCfg.bMultiPassSupport;
  }
  /* enable RDOQ by default if HW supported */
  regs->bRDOQEnable = IS_H264(config->codecFormat) ? regs->asicCfg.RDOQSupportH264 : regs->asicCfg.RDOQSupportHEVC;
  regs->bRDOQEnable = regs->bRDOQEnable && (vcenc_inst->pass != 1);

  vcenc_inst->lookaheadDepth = lookaheadDepth;
  if(vcenc_inst->pass==2) {
    VCEncConfig new_cfg;
    u32 in_loop_ds_ratio;

    memcpy(&new_cfg, config, sizeof(VCEncConfig));

    /*config pass 1 encoder cfg*/
    new_cfg.pass = 1;
    new_cfg.lookaheadDepth = lookaheadDepth;
    new_cfg.enableOutputCuInfo = 1;

    /*pass-1 cutree downscaler*/
    in_loop_ds_ratio = regs->asicCfg.inLoopDSRatio + 1;
    if(vcenc_inst->extDSRatio)
      in_loop_ds_ratio = vcenc_inst->extDSRatio + 1;
    new_cfg.width /= in_loop_ds_ratio;
    new_cfg.height /= in_loop_ds_ratio;
    new_cfg.width = ( new_cfg.width >> 1)<< 1;
    new_cfg.height = ( new_cfg.height >> 1)<< 1;

    new_cfg.exteralReconAlloc = 0;//this should alloc internarlly when in the first pass.

    ret = VCEncInit(&new_cfg, &vcenc_inst->lookahead.priv_inst);
    if (ret != VCENC_OK) {
      vcenc_inst->lookahead.priv_inst = NULL;
      APITRACE("VCEncInit: LookaheadInit failed");
      goto error;
    }
  }

  if(IS_AV1(config->codecFormat))
      VCEncInitAV1(config, vcenc_inst);

  if(IS_VP9(config->codecFormat))
    VCEncInitVP9(config, vcenc_inst);

  vcenc_inst->inst = vcenc_inst;
  APITRACE("VCEncInit: OK");
  return VCENC_OK;

error:

  APITRACEERR("VCEncInit: ERROR Initialization failed");

  //release all resource(instance/ewl/memory/ewl_mem) if vcenc_inst created but error happen
  if(vcenc_inst != NULL)
  {
      vcenc_inst->inst = vcenc_inst;
      VCEncRelease( vcenc_inst);
      vcenc_inst = NULL;
      return ret;
  }
  //release ewl resource if vcenc_inst not created yet
  if (instance != NULL)
    EWLfree(instance);
  if (ewl != NULL)
    (void) EWLRelease(ewl);

  return ret;

}
/*------------------------------------------------------------------------------

    VCEncShutdown

    Function frees the encoder instance.

    Input   inst    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void VCEncShutdown(VCEncInst inst)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  const void *ewl;

  ASSERT(inst);

  ewl = pEncInst->asic.ewl;

  EncAsicMemFree_V2(&pEncInst->asic);


  EWLfree(pEncInst);

  (void) EWLRelease(ewl);
}

/*------------------------------------------------------------------------------

    Function name : VCEncRelease
    Description   : Releases encoder instance and all associated resource

    Return type   : VCEncRet
    Argument      : inst - the instance to be released
------------------------------------------------------------------------------*/
VCEncRet VCEncRelease(VCEncInst inst)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  struct container *c;
  VCEncRet ret = VCENC_OK;

  APITRACE("VCEncRelease#");

  /* Check for illegal inputs */
  if (pEncInst == NULL)
  {
    APITRACEERR("VCEncRelease: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != inst)
  {
    APITRACEERR("VCEncRelease: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  if(pEncInst->pass == 1) {
    cuTreeRelease(&pEncInst->cuTreeCtl);

    //release pass-1 internal stream buffer
   if(pEncInst->lookahead.internal_mem.mem.virtualAddress)
     EWLFreeLinear(pEncInst->asic.ewl,  &(pEncInst->lookahead.internal_mem.mem));
  }

  if(pEncInst->pass==2 && pEncInst->lookahead.priv_inst) {
    VCEncRet ret = TerminateLookaheadThread(&pEncInst->lookahead);
    if (ret != VCENC_OK) {
      APITRACE("VCEncStrmEnd: TerminateLookaheadThread failed");
      return ret;
    }
    ret = VCEncRelease(pEncInst->lookahead.priv_inst);
    if (ret != VCENC_OK) {
      APITRACE("VCEncRelease: LookaheadRelease failed");
      return ret;
    }
  }

  if (!(c = get_container(pEncInst))) return VCENC_ERROR;

  sw_free_pictures(c);  /* Call this before free_parameter_sets() */
  free_parameter_sets(c);

  VCEncShutdown((struct instance *)pEncInst);
  APITRACE("VCEncRelease: OK");
  return VCENC_OK;
}
/*------------------------------------------------------------------------------

    VCEncGetPerformance

    Function frees the encoder instance.

    Input   inst    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
u32 VCEncGetPerformance(VCEncInst inst)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  const void *ewl;
  u32 performanceData;
  ASSERT(inst);
  APITRACE("VCEncGetPerformance#");
  /* Check for illegal inputs */
  if (pEncInst == NULL)
  {
    APITRACEERR("VCEncGetPerformance: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != inst)
  {
    APITRACEERR("VCEncGetPerformance: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  ewl = pEncInst->asic.ewl;
  performanceData = EncAsicGetPerformance(ewl);
  APITRACE("VCEncGetPerformance:OK");
  return performanceData;
}

/*------------------------------------------------------------------------------
  get_container
------------------------------------------------------------------------------*/
struct container *get_container(struct vcenc_instance *vcenc_instance)
{
  struct instance *instance = (struct instance *)vcenc_instance;

  if (!instance || (instance->instance != vcenc_instance)) return NULL;

  return &instance->container;
}


/*------------------------------------------------------------------------------
  VCEncSetCodingCtrl
------------------------------------------------------------------------------*/
VCEncRet VCEncSetCodingCtrl(VCEncInst instAddr, const VCEncCodingCtrl *pCodeParams)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) instAddr;
  regValues_s *regs = &pEncInst->asic.regs;
  u32 i;
  u32 client_type;
  EWLHwConfig_t cfg;
  APITRACE("VCEncSetCodingCtrl#");
  APITRACEPARAM("sliceSize", pCodeParams->sliceSize);
  APITRACEPARAM("seiMessages", pCodeParams->seiMessages);
  APITRACEPARAM("videoFullRange", pCodeParams->videoFullRange);
  APITRACEPARAM("disableDeblockingFilter", pCodeParams->disableDeblockingFilter);
  APITRACEPARAM("tc_Offset", pCodeParams->tc_Offset);
  APITRACEPARAM("beta_Offset", pCodeParams->beta_Offset);
  APITRACEPARAM("enableDeblockOverride", pCodeParams->enableDeblockOverride);
  APITRACEPARAM("deblockOverride", pCodeParams->deblockOverride);
  APITRACEPARAM("enableSao", pCodeParams->enableSao);
  APITRACEPARAM("enableScalingList", pCodeParams->enableScalingList);
  APITRACEPARAM("sampleAspectRatioWidth", pCodeParams->sampleAspectRatioWidth);
  APITRACEPARAM("sampleAspectRatioHeight", pCodeParams->sampleAspectRatioHeight);
  APITRACEPARAM("enableCabac", pCodeParams->enableCabac);
  APITRACEPARAM("cabacInitFlag", pCodeParams->cabacInitFlag);
  APITRACEPARAM("cirStart", pCodeParams->cirStart);
  APITRACEPARAM("cirInterval", pCodeParams->cirInterval);
  APITRACEPARAM("intraArea.enable", pCodeParams->intraArea.enable);
  APITRACEPARAM("intraArea.top", pCodeParams->intraArea.top);
  APITRACEPARAM("intraArea.bottom", pCodeParams->intraArea.bottom);
  APITRACEPARAM("intraArea.left", pCodeParams->intraArea.left);
  APITRACEPARAM("intraArea.right", pCodeParams->intraArea.right);
  APITRACEPARAM("ipcm1Area.enable", pCodeParams->ipcm1Area.enable);
  APITRACEPARAM("ipcm1Area.top", pCodeParams->ipcm1Area.top);
  APITRACEPARAM("ipcm1Area.bottom", pCodeParams->ipcm1Area.bottom);
  APITRACEPARAM("ipcm1Area.left", pCodeParams->ipcm1Area.left);
  APITRACEPARAM("ipcm1Area.right", pCodeParams->ipcm1Area.right);
  APITRACEPARAM("ipcm2Area.enable", pCodeParams->ipcm2Area.enable);
  APITRACEPARAM("ipcm2Area.top", pCodeParams->ipcm2Area.top);
  APITRACEPARAM("ipcm2Area.bottom", pCodeParams->ipcm2Area.bottom);
  APITRACEPARAM("ipcm2Area.left", pCodeParams->ipcm2Area.left);
  APITRACEPARAM("ipcm2Area.right", pCodeParams->ipcm2Area.right);
  APITRACEPARAM("roi1Area.enable", pCodeParams->roi1Area.enable);
  APITRACEPARAM("roi1Area.top", pCodeParams->roi1Area.top);
  APITRACEPARAM("roi1Area.bottom", pCodeParams->roi1Area.bottom);
  APITRACEPARAM("roi1Area.left", pCodeParams->roi1Area.left);
  APITRACEPARAM("roi1Area.right", pCodeParams->roi1Area.right);
  APITRACEPARAM("roi2Area.enable", pCodeParams->roi2Area.enable);
  APITRACEPARAM("roi2Area.top", pCodeParams->roi2Area.top);
  APITRACEPARAM("roi2Area.bottom", pCodeParams->roi2Area.bottom);
  APITRACEPARAM("roi2Area.left", pCodeParams->roi2Area.left);
  APITRACEPARAM("roi2Area.right", pCodeParams->roi2Area.right);
  APITRACEPARAM("roi3Area.enable", pCodeParams->roi3Area.enable);
  APITRACEPARAM("roi3Area.top", pCodeParams->roi3Area.top);
  APITRACEPARAM("roi3Area.bottom", pCodeParams->roi3Area.bottom);
  APITRACEPARAM("roi3Area.left", pCodeParams->roi3Area.left);
  APITRACEPARAM("roi3Area.right", pCodeParams->roi3Area.right);
  APITRACEPARAM("roi4Area.enable", pCodeParams->roi4Area.enable);
  APITRACEPARAM("roi4Area.top", pCodeParams->roi4Area.top);
  APITRACEPARAM("roi4Area.bottom", pCodeParams->roi4Area.bottom);
  APITRACEPARAM("roi4Area.left", pCodeParams->roi4Area.left);
  APITRACEPARAM("roi4Area.right", pCodeParams->roi4Area.right);
  APITRACEPARAM("roi5Area.enable", pCodeParams->roi5Area.enable);
  APITRACEPARAM("roi5Area.top", pCodeParams->roi5Area.top);
  APITRACEPARAM("roi5Area.bottom", pCodeParams->roi5Area.bottom);
  APITRACEPARAM("roi5Area.left", pCodeParams->roi5Area.left);
  APITRACEPARAM("roi5Area.right", pCodeParams->roi5Area.right);
  APITRACEPARAM("roi6Area.enable", pCodeParams->roi6Area.enable);
  APITRACEPARAM("roi6Area.top", pCodeParams->roi6Area.top);
  APITRACEPARAM("roi6Area.bottom", pCodeParams->roi6Area.bottom);
  APITRACEPARAM("roi6Area.left", pCodeParams->roi6Area.left);
  APITRACEPARAM("roi6Area.right", pCodeParams->roi6Area.right);
  APITRACEPARAM("roi7Area.enable", pCodeParams->roi7Area.enable);
  APITRACEPARAM("roi7Area.top", pCodeParams->roi7Area.top);
  APITRACEPARAM("roi7Area.bottom", pCodeParams->roi7Area.bottom);
  APITRACEPARAM("roi7Area.left", pCodeParams->roi7Area.left);
  APITRACEPARAM("roi7Area.right", pCodeParams->roi7Area.right);
  APITRACEPARAM("roi8Area.enable", pCodeParams->roi8Area.enable);
  APITRACEPARAM("roi8Area.top", pCodeParams->roi8Area.top);
  APITRACEPARAM("roi8Area.bottom", pCodeParams->roi8Area.bottom);
  APITRACEPARAM("roi8Area.left", pCodeParams->roi8Area.left);
  APITRACEPARAM("roi8Area.right", pCodeParams->roi8Area.right);
  APITRACEPARAM("roi1DeltaQp", pCodeParams->roi1DeltaQp);
  APITRACEPARAM("roi2DeltaQp", pCodeParams->roi2DeltaQp);
  APITRACEPARAM("roi3DeltaQp", pCodeParams->roi3DeltaQp);
  APITRACEPARAM("roi4DeltaQp", pCodeParams->roi4DeltaQp);
  APITRACEPARAM("roi5DeltaQp", pCodeParams->roi5DeltaQp);
  APITRACEPARAM("roi6DeltaQp", pCodeParams->roi6DeltaQp);
  APITRACEPARAM("roi7DeltaQp", pCodeParams->roi7DeltaQp);
  APITRACEPARAM("roi8DeltaQp", pCodeParams->roi8DeltaQp);
  APITRACEPARAM("roi1Qp", pCodeParams->roi1Qp);
  APITRACEPARAM("roi2Qp", pCodeParams->roi2Qp);
  APITRACEPARAM("roi3Qp", pCodeParams->roi3Qp);
  APITRACEPARAM("roi4Qp", pCodeParams->roi4Qp);
  APITRACEPARAM("roi5Qp", pCodeParams->roi5Qp);
  APITRACEPARAM("roi6Qp", pCodeParams->roi6Qp);
  APITRACEPARAM("roi7Qp", pCodeParams->roi7Qp);
  APITRACEPARAM("roi8Qp", pCodeParams->roi8Qp);
  APITRACEPARAM("chroma_qp_offset", pCodeParams->chroma_qp_offset);
  APITRACEPARAM("fieldOrder", pCodeParams->fieldOrder);
  APITRACEPARAM("gdrDuration", pCodeParams->gdrDuration);
  APITRACEPARAM("roiMapDeltaQpEnable", pCodeParams->roiMapDeltaQpEnable);
  APITRACEPARAM("roiMapDeltaQpBlockUnit", pCodeParams->roiMapDeltaQpBlockUnit);
  APITRACEPARAM("RoimapCuCtrl_index_enable", pCodeParams->RoimapCuCtrl_index_enable);
  APITRACEPARAM("RoimapCuCtrl_enable", pCodeParams->RoimapCuCtrl_enable);
  APITRACEPARAM("roiMapDeltaQpBinEnable", pCodeParams->roiMapDeltaQpBinEnable);
  APITRACEPARAM("RoimapCuCtrl_ver", pCodeParams->RoimapCuCtrl_ver);
  APITRACEPARAM("RoiQpDelta_ver", pCodeParams->RoiQpDelta_ver);
  APITRACEPARAM("pcm_enabled_flag", pCodeParams->pcm_enabled_flag);
  APITRACEPARAM("pcm_loop_filter_disabled_flag", pCodeParams->pcm_loop_filter_disabled_flag);
  APITRACEPARAM("ipcmMapEnable", pCodeParams->ipcmMapEnable);
  APITRACEPARAM("tiles_enabled_flag", pCodeParams->tiles_enabled_flag);
  APITRACEPARAM("num_tile_columns", pCodeParams->num_tile_columns);
  APITRACEPARAM("num_tile_rows", pCodeParams->num_tile_rows);
  APITRACEPARAM("loop_filter_across_tiles_enabled_flag", pCodeParams->loop_filter_across_tiles_enabled_flag);
  APITRACEPARAM("skipMapEnable", pCodeParams->skipMapEnable);
  APITRACEPARAM("enableRdoQuant", pCodeParams->enableRdoQuant);

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pCodeParams == NULL))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  client_type = IS_H264(pEncInst->codecFormat)  ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;

  i32 core_id = -1;

  for (i=0;i<EWLGetCoreNum();i++)
  {
    cfg = EncAsicGetAsicConfig(i);
    if (client_type == EWL_CLIENT_TYPE_H264_ENC && cfg.h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if (client_type == EWL_CLIENT_TYPE_HEVC_ENC && cfg.hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if (cfg.deNoiseEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
        pCodeParams->noiseReductionEnable == 1)
    {
      APITRACEERR("VCEncSetCodingCtrl: noise reduction core not supported");
      continue;
    }

    if (pCodeParams->streamMultiSegmentMode != 0 && cfg.streamMultiSegment == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      APITRACEERR("VCEncSetCodingCtrl: stream multi-segment core not supported");
      continue;
    }

    core_id = i;

    if (pCodeParams->noiseReductionEnable == 1)
      pEncInst->featureToSupport.deNoiseEnabled = 1;
    else
      pEncInst->featureToSupport.deNoiseEnabled = 0;

    break;
  }

  if (core_id < 0)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR unsupported argument by HW");
    return VCENC_INVALID_ARGUMENT;
  }

    //change max_tr_size ifHW cfg support TU32
   for (i=0;i<EWLGetCoreNum();i++)
   {
     cfg = EncAsicGetAsicConfig(i);
     if (client_type == EWL_CLIENT_TYPE_H264_ENC && cfg.h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
       continue;

     if (client_type == EWL_CLIENT_TYPE_HEVC_ENC && cfg.hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
       continue;

     if(cfg.tu32Enable)
     {
       pEncInst->max_tr_size = 32;
       pEncInst->tr_depth_inter = (cfg.tu32Enable && cfg.dynamicMaxTuSize && pEncInst->rdoLevel == 0 ? 3 : 2);
     }
   }

  u32 core_info = 0; /*mode[1bit](1:all 0:specified)+amount[3bit](the needing amount -1)+reserved+core_mapping[8bit]*/
  u32 valid_num =0;

  for (i=0; i< EWLGetCoreNum(); i++)
  {
   cfg = EncAsicGetAsicConfig(i);
   if ((client_type == EWL_CLIENT_TYPE_H264_ENC&&cfg.h264Enabled == 1)||
       (client_type == EWL_CLIENT_TYPE_HEVC_ENC&&cfg.hevcEnabled == 1))
   {
    //core_info |= 1<<i;
    valid_num++;
   }
   else
    continue;
  }
  if (valid_num == 0) /*none of cores is supported*/
    return VCENC_INVALID_ARGUMENT;

  core_info |= 1<< CORE_INFO_MODE_OFFSET; //now just support 1 core,so mode is all.

  core_info |= 0<< CORE_INFO_AMOUNT_OFFSET;//now just support 1 core

  pEncInst->reserve_core_info = core_info;
  printf("VC8000E reserve core info:%x\n",pEncInst->reserve_core_info);

  if ((HW_ID_MAJOR_NUMBER(regs->asicHwId) < 2 /* H2V1 */)&&(pCodeParams->roiMapDeltaQpEnable == 1))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR ROI MAP not supported");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pEncInst->asic.regs.asicCfg.roiMapVersion < 3) && pCodeParams->RoimapCuCtrl_index_enable )
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR ROI MAP cu ctrl index not supported");
    return VCENC_INVALID_ARGUMENT;
  }
  if ((pEncInst->asic.regs.asicCfg.roiMapVersion < 3) && pCodeParams->RoimapCuCtrl_enable )
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR ROI MAP cu ctrl not supported");
    return VCENC_INVALID_ARGUMENT;
  }
  if(pCodeParams->RoimapCuCtrl_enable && ((pCodeParams->RoimapCuCtrl_ver < 3) || (pCodeParams->RoimapCuCtrl_ver > 7)))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoiCuCtrlVer");
    return VCENC_INVALID_ARGUMENT;
  }
  if((pCodeParams->RoimapCuCtrl_enable == 0) && pCodeParams->RoimapCuCtrl_ver != 0)
  {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoimapCuCtrl_ver");
      return VCENC_INVALID_ARGUMENT;
  }
  if (((pEncInst->asic.regs.asicCfg.roiMapVersion == 3) &&(pCodeParams->roiMapDeltaQpEnable || pCodeParams->ipcmMapEnable || pCodeParams->skipMapEnable)&& (pCodeParams->RoiQpDelta_ver == 0)) || (pCodeParams->RoiQpDelta_ver > 3))
  {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoiQpDelta_ver");
      return VCENC_INVALID_ARGUMENT;
  }

  if ((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roiMapEnable != pCodeParams->roiMapDeltaQpEnable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roiMapDeltaQpEnable");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check for invalid values */
  if (pCodeParams->sliceSize > (u32)pEncInst->ctbPerCol)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sliceSize");
    return VCENC_INVALID_ARGUMENT;
  }
  if ((IS_AV1(pEncInst->codecFormat)) &&(pCodeParams->sliceSize != 0))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sliceSize for AV1, it should be 0");
    return VCENC_INVALID_ARGUMENT;
  }
  if ((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->gdrEnabled != (pCodeParams->gdrDuration > 0)))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config gdrDuration");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->cirStart > (u32)pEncInst->ctbPerFrame ||
      pCodeParams->cirInterval > (u32)pEncInst->ctbPerFrame)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid CIR value");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->intraArea.enable)
  {
    if (!(pCodeParams->intraArea.top <= pCodeParams->intraArea.bottom &&
          pCodeParams->intraArea.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->intraArea.left <= pCodeParams->intraArea.right &&
          pCodeParams->intraArea.right < (u32)pEncInst->ctbPerRow) || (pCodeParams->gdrDuration > 0))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid intraArea");
      return VCENC_INVALID_ARGUMENT;
    }
  }
  if (IS_AV1(pEncInst->codecFormat) && (pCodeParams->ipcm1Area.enable || pCodeParams->ipcm2Area.enable || pCodeParams->ipcmMapEnable)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR IPCM not supported for AV1");
      return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->ipcm1Area.enable)
  {
    if (!(pCodeParams->ipcm1Area.top <= pCodeParams->ipcm1Area.bottom &&
          pCodeParams->ipcm1Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->ipcm1Area.left <= pCodeParams->ipcm1Area.right &&
          pCodeParams->ipcm1Area.right < (u32)pEncInst->ctbPerRow) || (pCodeParams->gdrDuration > 0))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm1Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }
  if (pCodeParams->ipcm2Area.enable)
  {
    if (!(pCodeParams->ipcm2Area.top <= pCodeParams->ipcm2Area.bottom &&
          pCodeParams->ipcm2Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->ipcm2Area.left <= pCodeParams->ipcm2Area.right &&
          pCodeParams->ipcm2Area.right < (u32)pEncInst->ctbPerRow) || (pCodeParams->gdrDuration > 0))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm2Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }
  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi1Enable != pCodeParams->roi1Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi1Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi1Area.enable)
  {
    if (!(pCodeParams->roi1Area.top <= pCodeParams->roi1Area.bottom &&
          pCodeParams->roi1Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi1Area.left <= pCodeParams->roi1Area.right &&
          pCodeParams->roi1Area.right < (u32)pEncInst->ctbPerRow) || (pCodeParams->gdrDuration > 0))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi1Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi2Enable != pCodeParams->roi2Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi2Area");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pCodeParams->roi2Area.enable)
  {
    if (!(pCodeParams->roi2Area.top <= pCodeParams->roi2Area.bottom &&
          pCodeParams->roi2Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi2Area.left <= pCodeParams->roi2Area.right &&
          pCodeParams->roi2Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi2Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((regs->asicCfg.ROI8Support == 0 ) && (pCodeParams->roi3Area.enable || pCodeParams->roi4Area.enable || pCodeParams->roi5Area.enable
                                         || pCodeParams->roi6Area.enable || pCodeParams->roi7Area.enable || pCodeParams->roi8Area.enable))
  {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roiArea");
      return VCENC_INVALID_ARGUMENT;
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi3Enable != pCodeParams->roi3Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi3Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi3Area.enable)
  {
    if (!(pCodeParams->roi3Area.top <= pCodeParams->roi3Area.bottom &&
          pCodeParams->roi3Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi3Area.left <= pCodeParams->roi3Area.right &&
          pCodeParams->roi3Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi3Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi4Enable != pCodeParams->roi4Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi4Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi4Area.enable)
  {
    if (!(pCodeParams->roi4Area.top <= pCodeParams->roi4Area.bottom &&
          pCodeParams->roi4Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi4Area.left <= pCodeParams->roi4Area.right &&
          pCodeParams->roi4Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi4Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi5Enable != pCodeParams->roi5Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi5Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi5Area.enable)
  {
    if (!(pCodeParams->roi5Area.top <= pCodeParams->roi5Area.bottom &&
          pCodeParams->roi5Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi5Area.left <= pCodeParams->roi5Area.right &&
          pCodeParams->roi5Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi5Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi6Enable != pCodeParams->roi6Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi6Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi6Area.enable)
  {
    if (!(pCodeParams->roi6Area.top <= pCodeParams->roi6Area.bottom &&
          pCodeParams->roi6Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi6Area.left <= pCodeParams->roi6Area.right &&
          pCodeParams->roi6Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi6Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi7Enable != pCodeParams->roi7Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi7Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi7Area.enable)
  {
    if (!(pCodeParams->roi7Area.top <= pCodeParams->roi7Area.bottom &&
          pCodeParams->roi7Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi7Area.left <= pCodeParams->roi7Area.right &&
          pCodeParams->roi7Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi7Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if((pEncInst->encStatus >= VCENCSTAT_START_STREAM)&&(pEncInst->roi8Enable != pCodeParams->roi8Area.enable))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi8Area");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->roi8Area.enable)
  {
    if (!(pCodeParams->roi8Area.top <= pCodeParams->roi8Area.bottom &&
          pCodeParams->roi8Area.bottom < (u32)pEncInst->ctbPerCol &&
          pCodeParams->roi8Area.left <= pCodeParams->roi8Area.right &&
          pCodeParams->roi8Area.right < (u32)pEncInst->ctbPerRow))
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi8Area");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if (regs->asicCfg.roiMapVersion != 1 && regs->asicCfg.roiMapVersion != 3 && pCodeParams->ipcmMapEnable)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR IPCM MAP not supported");
    return VCENC_INVALID_ARGUMENT;
  }

  if (regs->asicCfg.roiMapVersion != 2 && regs->asicCfg.roiMapVersion != 3 && pCodeParams->skipMapEnable)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR SKIP MAP not supported");
    return VCENC_INVALID_ARGUMENT;
  }

  if (regs->asicCfg.roiAbsQpSupport)
  {
    if (pCodeParams->roi1DeltaQp < -51 ||
        pCodeParams->roi1DeltaQp > 51 ||
        pCodeParams->roi2DeltaQp < -51 ||
        pCodeParams->roi2DeltaQp > 51 ||
        pCodeParams->roi3DeltaQp < -51 ||
        pCodeParams->roi3DeltaQp > 51 ||
        pCodeParams->roi4DeltaQp < -51 ||
        pCodeParams->roi4DeltaQp > 51 ||
        pCodeParams->roi5DeltaQp < -51 ||
        pCodeParams->roi5DeltaQp > 51 ||
        pCodeParams->roi6DeltaQp < -51 ||
        pCodeParams->roi6DeltaQp > 51 ||
        pCodeParams->roi7DeltaQp < -51 ||
        pCodeParams->roi7DeltaQp > 51 ||
        pCodeParams->roi8DeltaQp < -51 ||
        pCodeParams->roi8DeltaQp > 51 ||
        pCodeParams->roi1Qp > 51 ||
        pCodeParams->roi2Qp > 51 ||
        pCodeParams->roi3Qp > 51 ||
        pCodeParams->roi4Qp > 51 ||
        pCodeParams->roi5Qp > 51 ||
        pCodeParams->roi6Qp > 51 ||
        pCodeParams->roi7Qp > 51 ||
        pCodeParams->roi8Qp > 51)
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ROI delta QP");
      return VCENC_INVALID_ARGUMENT;
    }
  }
  else
  {
    if (pCodeParams->roi1DeltaQp < -30 ||
        pCodeParams->roi1DeltaQp > 0 ||
        pCodeParams->roi2DeltaQp < -30 ||
        pCodeParams->roi2DeltaQp > 0 /*||
       pCodeParams->adaptiveRoi < -51 ||
       pCodeParams->adaptiveRoi > 0*/)
    {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ROI delta QP");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  /* low latency : Input Line buffer check */
  if(pCodeParams->inputLineBufEn)
  {
     /* check zero depth */
     if ((pCodeParams->inputLineBufDepth == 0 || pCodeParams->amountPerLoopBack == 0) &&
         (pCodeParams->inputLineBufLoopBackEn || pCodeParams->inputLineBufHwModeEn))
     {
          APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid input buffer depth");
          return ENCHW_NOK;
     }
  }

  if(pCodeParams->streamMultiSegmentMode > 2 ||
    ((pCodeParams->streamMultiSegmentMode >0) &&  (pCodeParams->streamMultiSegmentAmount <= 1 || pCodeParams->streamMultiSegmentAmount > 16)))
  {
     APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid stream multi-segment config");
     return ENCHW_NOK;
  }

  if(pCodeParams->streamMultiSegmentMode != 0 && pCodeParams->sliceSize != 0)
  {
     APITRACEERR("VCEncSetCodingCtrl: ERROR multi-segment not support slice encoding");
     return ENCHW_NOK;
  }

  if ((pCodeParams->Hdr10Display.hdr10_display_enable == ENCHW_YES) || (pCodeParams->Hdr10Color.hdr10_color_enable == ENCHW_YES) || (pCodeParams->Hdr10LightLevel.hdr10_lightlevel_enable == ENCHW_YES))
  {
	  // parameters check
	  if (!IS_HEVC(pEncInst->codecFormat) )
	  {
		  APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid Encoder Type, It should be HEVC!\n");
		  return ENCHW_NOK;
	  }

	  if (pEncInst->profile != VCENC_HEVC_MAIN_10_PROFILE)
	  {
		  APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid profile, It should be HEVC_MAIN_10_PROFILE!\n");
		  return ENCHW_NOK;
	  }

	  if (pCodeParams->Hdr10Color.hdr10_color_enable == ENCHW_YES)
	  {
		  if ((pCodeParams->Hdr10Color.hdr10_matrix != 9) || (pCodeParams->Hdr10Color.hdr10_primary != 9))
		  {
			  APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid HDR10 colour transfer VUI parameter\n");
			  return ENCHW_NOK;
		  }
	  }
  }

  /* Programable ME vertical search range */
  if (pCodeParams->meVertSearchRange)
  {
    if(!regs->asicCfg.meVertRangeProgramable)
    {
      APITRACEERR("VCEncSetCodingCtrl: Programable ME vertical search range not supported");
      return VCENC_INVALID_ARGUMENT;
    }
    else if ((IS_H264(pEncInst->codecFormat) && (pCodeParams->meVertSearchRange!=24) && (pCodeParams->meVertSearchRange!=48)) ||
           ((!IS_H264(pEncInst->codecFormat)) && (pCodeParams->meVertSearchRange!=40) && (pCodeParams->meVertSearchRange!=64)))
    {
      APITRACEERR("VCEncSetCodingCtrl: Invalid ME vertical search range. Should be 24 or 48 for H264; 40 or 64 for HEVC/AV1.");
      return VCENC_INVALID_ARGUMENT;
    }
  }
  regs->meAssignedVertRange = pCodeParams->meVertSearchRange >> 3;

  pEncInst->Hdr10Display.hdr10_display_enable = pCodeParams->Hdr10Display.hdr10_display_enable;
  pEncInst->Hdr10Display.hdr10_dx0 = pCodeParams->Hdr10Display.hdr10_dx0;
  pEncInst->Hdr10Display.hdr10_dy0 = pCodeParams->Hdr10Display.hdr10_dy0;
  pEncInst->Hdr10Display.hdr10_dx1 = pCodeParams->Hdr10Display.hdr10_dx1;
  pEncInst->Hdr10Display.hdr10_dy1 = pCodeParams->Hdr10Display.hdr10_dy1;
  pEncInst->Hdr10Display.hdr10_dx2 = pCodeParams->Hdr10Display.hdr10_dx2;
  pEncInst->Hdr10Display.hdr10_dy2 = pCodeParams->Hdr10Display.hdr10_dy2;
  pEncInst->Hdr10Display.hdr10_wx  = pCodeParams->Hdr10Display.hdr10_wx;
  pEncInst->Hdr10Display.hdr10_wy  = pCodeParams->Hdr10Display.hdr10_wy;
  pEncInst->Hdr10Display.hdr10_maxluma = pCodeParams->Hdr10Display.hdr10_maxluma;
  pEncInst->Hdr10Display.hdr10_minluma = pCodeParams->Hdr10Display.hdr10_minluma;

  pEncInst->Hdr10Color.hdr10_color_enable = pCodeParams->Hdr10Color.hdr10_color_enable;
  pEncInst->Hdr10Color.hdr10_matrix   = pCodeParams->Hdr10Color.hdr10_matrix;
  pEncInst->Hdr10Color.hdr10_primary  = pCodeParams->Hdr10Color.hdr10_primary;
  pEncInst->Hdr10Color.hdr10_transfer = pCodeParams->Hdr10Color.hdr10_transfer;

  pEncInst->Hdr10LightLevel.hdr10_lightlevel_enable = pCodeParams->Hdr10LightLevel.hdr10_lightlevel_enable;
  pEncInst->Hdr10LightLevel.hdr10_maxlight = pCodeParams->Hdr10LightLevel.hdr10_maxlight;
  pEncInst->Hdr10LightLevel.hdr10_avglight = pCodeParams->Hdr10LightLevel.hdr10_avglight;

  pEncInst->RpsInSliceHeader = IS_H264(pEncInst->codecFormat)  ? 0: pCodeParams->RpsInSliceHeader;

  /* Check status, only slice size, CIR & ROI allowed to change after start */
  if (pEncInst->encStatus != VCENCSTAT_INIT)
  {
    goto set_slice_size;
  }


  if (pCodeParams->enableSao > 1 || pCodeParams->enableSao > 1 || pCodeParams->roiMapDeltaQpEnable > 1 ||
      pCodeParams->disableDeblockingFilter > 1 ||
      pCodeParams->seiMessages > 1 || pCodeParams->videoFullRange > 1)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid enable/disable");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pCodeParams->sampleAspectRatioWidth > 65535 ||
      pCodeParams->sampleAspectRatioHeight > 65535)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sampleAspectRatio");
    return VCENC_INVALID_ARGUMENT;
  }

  if(IS_H264(pEncInst->codecFormat) ) {
    pEncInst->max_cu_size = 16;   /* Max coding unit size in pixels */
    pEncInst->min_cu_size = 8;   /* Min coding unit size in pixels */
    pEncInst->max_tr_size = 16;   /* Max transform size in pixels */
    pEncInst->min_tr_size = 4;   /* Min transform size in pixels */
    pEncInst->tr_depth_intra = 1;   /* Max transform hierarchy depth */
    pEncInst->tr_depth_inter = 2;   /* Max transform hierarchy depth */
	pEncInst->layerInRefIdc = pCodeParams->layerInRefIdcEnable;
  }

  if (pCodeParams->cabacInitFlag > 1)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid cabacInitIdc");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pCodeParams->enableCabac > 2)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid enableCabac");
    return VCENC_INVALID_ARGUMENT;
  }

  /* check limitation for H.264 baseline profile */
  if (IS_H264(pEncInst->codecFormat)  && pEncInst->profile == 66 && pCodeParams->enableCabac > 0)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid entropy coding mode for baseline profile");
    return ENCHW_NOK;
  }

  if ((IS_H264(pEncInst->codecFormat)  && pCodeParams->roiMapDeltaQpBlockUnit >= 3) ||
      (IS_AV1(pEncInst->codecFormat)  && pCodeParams->roiMapDeltaQpBlockUnit >= 1) ) //AV1 qpDelta in CTU level
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roiMapDeltaQpBlockUnit");
    return VCENC_INVALID_ARGUMENT;
  }

  if(IS_AV1(pEncInst->codecFormat)  && pCodeParams->tiles_enabled_flag)
  {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid, AV1 tiles not up yet, disable it");
  }

  regs->bRDOQEnable = pCodeParams->enableRdoQuant;
  if (regs->bRDOQEnable)
  {
    i32 rdoqSupport = IS_H264(pEncInst->codecFormat) ? regs->asicCfg.RDOQSupportH264 : regs->asicCfg.RDOQSupportHEVC;
    if (rdoqSupport == 0)
    {
      regs->bRDOQEnable = 0;
      APITRACEERR("VCEncSetCodingCtrl: RDO Quant is not supported by HW and forced to be disabled");
    }
    else if (pEncInst->pass == 1)
    {
      regs->bRDOQEnable = 0;
      APITRACEERR("VCEncSetCodingCtrl: RDO Quant is not supported by PASS1 encoding and forced to be disabled");
    }
  }

  pEncInst->chromaQpOffset = pCodeParams->chroma_qp_offset;
  pEncInst->fieldOrder = pCodeParams->fieldOrder ? 1 : 0;
  pEncInst->disableDeblocking = pCodeParams->disableDeblockingFilter;
  pEncInst->enableDeblockOverride = pCodeParams->enableDeblockOverride;
  if (pEncInst->enableDeblockOverride)
  {
    regs->slice_deblocking_filter_override_flag = pCodeParams->deblockOverride;
  }
  else
  {
    regs->slice_deblocking_filter_override_flag = 0;
  }
  if(IS_H264(pEncInst->codecFormat) ) {
    /* always enable deblocking override for H.264 */
    pEncInst->enableDeblockOverride = 1;
    regs->slice_deblocking_filter_override_flag = 1;
  }
  pEncInst->tc_Offset = pCodeParams->tc_Offset;
  pEncInst->beta_Offset = pCodeParams->beta_Offset;
  pEncInst->enableSao = pCodeParams->enableSao;

  VCEncHEVCDnfSetParameters(pEncInst, pCodeParams);

  regs->cabac_init_flag = pCodeParams->cabacInitFlag;
  regs->entropy_coding_mode_flag = (pCodeParams->enableCabac ? 1 : 0);
  pEncInst->sarWidth = pCodeParams->sampleAspectRatioWidth;
  pEncInst->sarHeight = pCodeParams->sampleAspectRatioHeight;


  pEncInst->videoFullRange = pCodeParams->videoFullRange;

  /* SEI messages are written in the beginning of each frame */
  if (pCodeParams->seiMessages)
    pEncInst->rateControl.sei.enabled = ENCHW_YES;
  else
    pEncInst->rateControl.sei.enabled = ENCHW_NO;


set_slice_size:

  regs->sliceSize = pCodeParams->sliceSize;
  regs->sliceNum = (regs->sliceSize == 0) ?  1 : ((pEncInst->ctbPerCol + (regs->sliceSize - 1)) / regs->sliceSize);

  //Limit MAX_SLICE_NUM
  if(regs->sliceNum > MAX_SLICE_NUM)
  {
    do
    {
      regs->sliceSize++;
      regs->sliceNum = (regs->sliceSize == 0) ?  1 : ((pEncInst->ctbPerCol + (regs->sliceSize - 1)) / regs->sliceSize);
    }while(regs->sliceNum > MAX_SLICE_NUM);
  }

  pEncInst->enableScalingList = pCodeParams->enableScalingList;

  if (pCodeParams->roiMapDeltaQpEnable || pCodeParams->roiMapDeltaQpBinEnable || pCodeParams->RoimapCuCtrl_enable )
  {
    pEncInst->min_qp_size = MIN((1 << (6 - pCodeParams->roiMapDeltaQpBlockUnit)), pEncInst->max_cu_size);
  }
  else
    pEncInst->min_qp_size = pEncInst->max_cu_size;


  /* Set CIR, intra forcing and ROI parameters */
  regs->cirStart = pCodeParams->cirStart;
  regs->cirInterval = pCodeParams->cirInterval;

  if (pCodeParams->intraArea.enable)
  {
    regs->intraAreaTop = pCodeParams->intraArea.top;
    regs->intraAreaLeft = pCodeParams->intraArea.left;
    regs->intraAreaBottom = pCodeParams->intraArea.bottom;
    regs->intraAreaRight = pCodeParams->intraArea.right;
  }
  else
  {
    regs->intraAreaTop = regs->intraAreaLeft = regs->intraAreaBottom =
                           regs->intraAreaRight = INVALID_POS;
  }

  pEncInst->pcm_enabled_flag = pCodeParams->pcm_enabled_flag || (pCodeParams->RoimapCuCtrl_ver > 3);
  regs->ipcmFilterDisable = pEncInst->pcm_loop_filter_disabled_flag = pCodeParams->pcm_loop_filter_disabled_flag;

  if (pCodeParams->ipcm1Area.enable)
  {
    regs->ipcm1AreaTop = pCodeParams->ipcm1Area.top;
    regs->ipcm1AreaLeft = pCodeParams->ipcm1Area.left;
    regs->ipcm1AreaBottom = pCodeParams->ipcm1Area.bottom;
    regs->ipcm1AreaRight = pCodeParams->ipcm1Area.right;
  }
  else
  {
    regs->ipcm1AreaTop = regs->ipcm1AreaLeft = regs->ipcm1AreaBottom =
                           regs->ipcm1AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm2Area.enable)
  {
    regs->ipcm2AreaTop = pCodeParams->ipcm2Area.top;
    regs->ipcm2AreaLeft = pCodeParams->ipcm2Area.left;
    regs->ipcm2AreaBottom = pCodeParams->ipcm2Area.bottom;
    regs->ipcm2AreaRight = pCodeParams->ipcm2Area.right;
  }
  else
  {
      regs->ipcm2AreaTop = regs->ipcm2AreaLeft = regs->ipcm2AreaBottom =
                             regs->ipcm2AreaRight = INVALID_POS;
  }
  regs->ipcmMapEnable = pCodeParams->ipcmMapEnable;

  if (pCodeParams->roi1Area.enable && pEncInst->pass != 1)
  {
    regs->roi1Top = pCodeParams->roi1Area.top;
    regs->roi1Left = pCodeParams->roi1Area.left;
    regs->roi1Bottom = pCodeParams->roi1Area.bottom;
    regs->roi1Right = pCodeParams->roi1Area.right;
    pEncInst->roi1Enable = 1;
  }
  else
  {
    regs->roi1Top = regs->roi1Left = regs->roi1Bottom =
                                       regs->roi1Right = INVALID_POS;
    pEncInst->roi1Enable = 0;
  }
  if (pCodeParams->roi2Area.enable && pEncInst->pass != 1)
  {
    regs->roi2Top = pCodeParams->roi2Area.top;
    regs->roi2Left = pCodeParams->roi2Area.left;
    regs->roi2Bottom = pCodeParams->roi2Area.bottom;
    regs->roi2Right = pCodeParams->roi2Area.right;
    pEncInst->roi2Enable = 1;
  }
  else
  {
    regs->roi2Top = regs->roi2Left = regs->roi2Bottom =
                                       regs->roi2Right = INVALID_POS;
    pEncInst->roi2Enable = 0;
  }
  if (pCodeParams->roi3Area.enable && pEncInst->pass != 1)
  {
    regs->roi3Top = pCodeParams->roi3Area.top;
    regs->roi3Left = pCodeParams->roi3Area.left;
    regs->roi3Bottom = pCodeParams->roi3Area.bottom;
    regs->roi3Right = pCodeParams->roi3Area.right;
    pEncInst->roi3Enable = 1;
  }
  else
  {
    regs->roi3Top = regs->roi3Left = regs->roi3Bottom =
                                       regs->roi3Right = INVALID_POS;
    pEncInst->roi3Enable = 0;
  }
  if (pCodeParams->roi4Area.enable && pEncInst->pass != 1)
  {
    regs->roi4Top = pCodeParams->roi4Area.top;
    regs->roi4Left = pCodeParams->roi4Area.left;
    regs->roi4Bottom = pCodeParams->roi4Area.bottom;
    regs->roi4Right = pCodeParams->roi4Area.right;
    pEncInst->roi4Enable = 1;
  }
  else
  {
    regs->roi4Top = regs->roi4Left = regs->roi4Bottom =
                                       regs->roi4Right = INVALID_POS;
    pEncInst->roi4Enable = 0;
  }
  if (pCodeParams->roi5Area.enable && pEncInst->pass != 1)
  {
    regs->roi5Top = pCodeParams->roi5Area.top;
    regs->roi5Left = pCodeParams->roi5Area.left;
    regs->roi5Bottom = pCodeParams->roi5Area.bottom;
    regs->roi5Right = pCodeParams->roi5Area.right;
    pEncInst->roi5Enable = 1;
  }
  else
  {
    regs->roi5Top = regs->roi5Left = regs->roi5Bottom =
                                       regs->roi5Right = INVALID_POS;
    pEncInst->roi5Enable = 0;
  }
  if (pCodeParams->roi6Area.enable && pEncInst->pass != 1)
  {
    regs->roi6Top = pCodeParams->roi6Area.top;
    regs->roi6Left = pCodeParams->roi6Area.left;
    regs->roi6Bottom = pCodeParams->roi6Area.bottom;
    regs->roi6Right = pCodeParams->roi6Area.right;
    pEncInst->roi6Enable = 1;
  }
  else
  {
    regs->roi6Top = regs->roi6Left = regs->roi6Bottom =
                                       regs->roi6Right = INVALID_POS;
    pEncInst->roi6Enable = 0;
  }
  if (pCodeParams->roi7Area.enable && pEncInst->pass != 1)
  {
    regs->roi7Top = pCodeParams->roi7Area.top;
    regs->roi7Left = pCodeParams->roi7Area.left;
    regs->roi7Bottom = pCodeParams->roi7Area.bottom;
    regs->roi7Right = pCodeParams->roi7Area.right;
    pEncInst->roi7Enable = 1;
  }
  else
  {
    regs->roi7Top = regs->roi7Left = regs->roi7Bottom =
                                       regs->roi7Right = INVALID_POS;
    pEncInst->roi7Enable = 0;
  }
  if (pCodeParams->roi8Area.enable && pEncInst->pass != 1)
  {
    regs->roi8Top = pCodeParams->roi8Area.top;
    regs->roi8Left = pCodeParams->roi8Area.left;
    regs->roi8Bottom = pCodeParams->roi8Area.bottom;
    regs->roi8Right = pCodeParams->roi8Area.right;
    pEncInst->roi8Enable = 1;
  }
  else
  {
    regs->roi8Top = regs->roi8Left = regs->roi8Bottom =
                                       regs->roi8Right = INVALID_POS;
    pEncInst->roi8Enable = 0;
  }

  regs->roi1DeltaQp = -pCodeParams->roi1DeltaQp;
  regs->roi2DeltaQp = -pCodeParams->roi2DeltaQp;
  regs->roi1Qp = pCodeParams->roi1Qp;
  regs->roi2Qp = pCodeParams->roi2Qp;
  regs->roi3DeltaQp = -pCodeParams->roi3DeltaQp;
  regs->roi4DeltaQp = -pCodeParams->roi4DeltaQp;
  regs->roi5DeltaQp = -pCodeParams->roi5DeltaQp;
  regs->roi6DeltaQp = -pCodeParams->roi6DeltaQp;
  regs->roi7DeltaQp = -pCodeParams->roi7DeltaQp;
  regs->roi8DeltaQp = -pCodeParams->roi8DeltaQp;
  regs->roi3Qp = pCodeParams->roi3Qp;
  regs->roi4Qp = pCodeParams->roi4Qp;
  regs->roi5Qp = pCodeParams->roi5Qp;
  regs->roi6Qp = pCodeParams->roi6Qp;
  regs->roi7Qp = pCodeParams->roi7Qp;
  regs->roi8Qp = pCodeParams->roi8Qp;

  regs->roiUpdate   = 1;    /* ROI has changed from previous frame. */

  pEncInst->roiMapEnable = 0;
  if(pEncInst->asic.regs.asicCfg.roiMapVersion == 3)
  {
      pEncInst->RoimapCuCtrl_index_enable = pCodeParams->RoimapCuCtrl_index_enable;
      pEncInst->RoimapCuCtrl_enable       = pCodeParams->RoimapCuCtrl_enable;

      pEncInst->RoimapCuCtrl_ver          = pCodeParams->RoimapCuCtrl_ver;
      pEncInst->RoiQpDelta_ver            = pCodeParams->RoiQpDelta_ver;
  }
#ifndef CTBRC_STRENGTH
  if ((pCodeParams->roiMapDeltaQpEnable == 1) || pCodeParams->roiMapDeltaQpBinEnable)
  {
    pEncInst->roiMapEnable = 1;
    regs->rcRoiEnable = RCROIMODE_ROIMAP_ENABLE;
  }
  else if (pCodeParams->roi1Area.enable || pCodeParams->roi2Area.enable)
    regs->rcRoiEnable = RCROIMODE_ROIAREA_ENABLE;
  else
    regs->rcRoiEnable = RCROIMODE_RC_ROIMAP_ROIAREA_DIABLE;
#else
    /*rcRoiEnable : bit2: rc control bit, bit1: roi map control bit, bit 0: roi area control bit*/
    if(((pCodeParams->roiMapDeltaQpEnable == 1) || pCodeParams->roiMapDeltaQpBinEnable || pCodeParams->RoimapCuCtrl_enable) &&( pCodeParams->roi1Area.enable ==0 && pCodeParams->roi2Area.enable ==0 && pCodeParams->roi3Area.enable ==0&& pCodeParams->roi4Area.enable ==0
                                                 && pCodeParams->roi5Area.enable ==0&& pCodeParams->roi6Area.enable ==0&& pCodeParams->roi7Area.enable ==0&& pCodeParams->roi8Area.enable ==0))
    {
      pEncInst->roiMapEnable = 1;
      regs->rcRoiEnable = 0x02;
    }
    else if((pCodeParams->roiMapDeltaQpEnable == 0) && (pCodeParams->roiMapDeltaQpBinEnable == 0)&& (pCodeParams->RoimapCuCtrl_enable == 0) && ( pCodeParams->roi1Area.enable !=0 || pCodeParams->roi2Area.enable !=0 || pCodeParams->roi3Area.enable !=0 || pCodeParams->roi4Area.enable !=0
                                                       || pCodeParams->roi5Area.enable !=0 || pCodeParams->roi6Area.enable !=0 || pCodeParams->roi7Area.enable !=0 || pCodeParams->roi8Area.enable !=0))
    {
      regs->rcRoiEnable = 0x01;
    }
    else if(((pCodeParams->roiMapDeltaQpEnable != 0) || pCodeParams->roiMapDeltaQpBinEnable || (pCodeParams->RoimapCuCtrl_enable != 0)) && ( pCodeParams->roi1Area.enable !=0 || pCodeParams->roi2Area.enable !=0 || pCodeParams->roi3Area.enable !=0 || pCodeParams->roi4Area.enable !=0
                                                       || pCodeParams->roi5Area.enable !=0 || pCodeParams->roi6Area.enable !=0 || pCodeParams->roi7Area.enable !=0 || pCodeParams->roi8Area.enable !=0))
    {
      pEncInst->roiMapEnable = 1;
      regs->rcRoiEnable = 0x03;
    }
    else
      regs->rcRoiEnable = 0x00;
#endif

  if (pEncInst->pass == 1)
    regs->rcRoiEnable = 0x00;

  /* SKIP map */
  regs->skipMapEnable = pCodeParams->skipMapEnable ? 1 : 0;

  pEncInst->rateControl.sei.insertRecoveryPointMessage = ENCHW_NO;
  pEncInst->rateControl.sei.recoveryFrameCnt = pCodeParams->gdrDuration;
  if (pEncInst->encStatus < VCENCSTAT_START_FRAME)
  {
    pEncInst->gdrFirstIntraFrame = (1 + pEncInst->interlaced);
  }
  pEncInst->gdrEnabled = (pCodeParams->gdrDuration > 0);
  bool gdrInit = pEncInst->gdrEnabled && (pEncInst->gdrDuration != (i32)pCodeParams->gdrDuration);
  pEncInst->gdrDuration = pCodeParams->gdrDuration;

  if (gdrInit )
  {
    pEncInst->gdrStart = 0;
    pEncInst->gdrCount = 0;

    pEncInst->gdrAverageMBRows = (pEncInst->ctbPerCol - 1) / pEncInst->gdrDuration;
    pEncInst->gdrMBLeft = pEncInst->ctbPerCol - 1 - pEncInst->gdrAverageMBRows * pEncInst->gdrDuration;

    if (pEncInst->gdrAverageMBRows == 0)
    {
      pEncInst->rateControl.sei.recoveryFrameCnt = pEncInst->gdrMBLeft;
      pEncInst->gdrDuration = pEncInst->gdrMBLeft;
    }
  }

  /* low latency */
  pEncInst->inputLineBuf.inputLineBufEn = pCodeParams->inputLineBufEn;
  pEncInst->inputLineBuf.inputLineBufLoopBackEn = pCodeParams->inputLineBufLoopBackEn;
  pEncInst->inputLineBuf.inputLineBufDepth = pCodeParams->inputLineBufDepth;
  pEncInst->inputLineBuf.amountPerLoopBack = pCodeParams->amountPerLoopBack;
  pEncInst->inputLineBuf.inputLineBufHwModeEn = pCodeParams->inputLineBufHwModeEn;
  pEncInst->inputLineBuf.cbFunc = pCodeParams->inputLineBufCbFunc;
  pEncInst->inputLineBuf.cbData = pCodeParams->inputLineBufCbData;

  /*stream multi-segment*/
  pEncInst->streamMultiSegment.streamMultiSegmentMode = pCodeParams->streamMultiSegmentMode;
  pEncInst->streamMultiSegment.streamMultiSegmentAmount = pCodeParams->streamMultiSegmentAmount;
  pEncInst->streamMultiSegment.cbFunc = pCodeParams->streamMultiSegCbFunc;
  pEncInst->streamMultiSegment.cbData = pCodeParams->streamMultiSegCbData;

  /* smart */
  pEncInst->smartModeEnable = pCodeParams->smartModeEnable;
  pEncInst->smartH264LumDcTh = pCodeParams->smartH264LumDcTh;
  pEncInst->smartH264CbDcTh = pCodeParams->smartH264CbDcTh;
  pEncInst->smartH264CrDcTh = pCodeParams->smartH264CrDcTh;
  for(i = 0; i < 3; i++) {
    pEncInst->smartHevcLumDcTh[i] = pCodeParams->smartHevcLumDcTh[i];
    pEncInst->smartHevcChrDcTh[i] = pCodeParams->smartHevcChrDcTh[i];
    pEncInst->smartHevcLumAcNumTh[i] = pCodeParams->smartHevcLumAcNumTh[i];
    pEncInst->smartHevcChrAcNumTh[i] = pCodeParams->smartHevcChrAcNumTh[i];
  }
  pEncInst->smartH264Qp = pCodeParams->smartH264Qp;
  pEncInst->smartHevcLumQp = pCodeParams->smartHevcLumQp;
  pEncInst->smartHevcChrQp = pCodeParams->smartHevcChrQp;
  for(i = 0; i < 4; i++)
    pEncInst->smartMeanTh[i] = pCodeParams->smartMeanTh[i];
  pEncInst->smartPixNumCntTh = pCodeParams->smartPixNumCntTh;

  /* tile*/
  pEncInst->tiles_enabled_flag = pCodeParams->tiles_enabled_flag && (!IS_AV1(pEncInst->codecFormat));
  pEncInst->num_tile_columns = pCodeParams->num_tile_columns;
  pEncInst->num_tile_rows = pCodeParams->num_tile_rows;
  pEncInst->loop_filter_across_tiles_enabled_flag = pCodeParams->loop_filter_across_tiles_enabled_flag;

  if(pEncInst->asic.regs.asicCfg.cuInforVersion>= 3)
    PsyFactor = pCodeParams->PsyFactor;

  /* multipass */
  pEncInst->cuTreeCtl.inQpDeltaBlkSize = 64 >> pCodeParams->roiMapDeltaQpBlockUnit;
  if(pEncInst->pass==2) {
    pEncInst->RoiQpDelta_ver            = 1;
    VCEncCodingCtrl newCodeParams;
    memcpy(&newCodeParams, pCodeParams, sizeof(VCEncCodingCtrl));
    newCodeParams.roiMapDeltaQpEnable == 0;
    newCodeParams.chroma_qp_offset = 0;
    newCodeParams.RoimapCuCtrl_enable = HANTRO_FALSE;
    newCodeParams.RoimapCuCtrl_index_enable = HANTRO_FALSE;
    if(pEncInst->asic.regs.asicCfg.roiMapVersion == 3)
      newCodeParams.RoiQpDelta_ver            = 1;
    VCEncRet ret = VCEncSetCodingCtrl(pEncInst->lookahead.priv_inst, &newCodeParams);
    if (ret != VCENC_OK) {
      APITRACE("VCEncSetCodingCtrl: LookaheadSetCodingCtrl failed");
      return ret;
    }
  }

  APITRACE("VCEncSetCodingCtrl: OK");
  return VCENC_OK;
}

/*-----------------------------------------------------------------------------
check reference lists modification
-------------------------------------------------------------------------------*/
i32 check_ref_lists_modification(const VCEncIn *pEncIn)
{
  int i;
  bool lowdelayB = HANTRO_FALSE;
  VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(pEncIn->gopConfig));

  for (i = 0; i < gopCfg->size; i ++)
  {
    VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
    if (cfg->codingType == VCENC_BIDIR_PREDICTED_FRAME)
    {
      u32 iRefPic;
      lowdelayB = HANTRO_TRUE;
      for (iRefPic = 0; iRefPic < cfg->numRefPics; iRefPic ++)
      {
        if (cfg->refPics[iRefPic].used_by_cur && cfg->refPics[iRefPic].ref_pic > 0)
          lowdelayB = HANTRO_FALSE;
      }
      if (lowdelayB)
        break;
    }
  }
  return (lowdelayB || pEncIn->bIsPeriodUpdateLTR ) ? 1 : 0;
}


int16_t av1_ac_quant_obu_Q3(int qindex, int delta, int bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, 255);
  switch (bit_depth) {
    case 8:  return ac_qlookup_Q3_obu[q_clamped];
    case 10: return ac_qlookup_10_Q3_obu[q_clamped];
    case 12: return ac_qlookup_12_Q3_obu[q_clamped];
    default:
      ASSERT(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
}

void av1_pick_filter_level(struct vcenc_instance *vcenc_instance, VCENC_AV1_FRAME_TYPE frame_type)
{
    int q_index = quantizer_to_qindex[(vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS)];
    int bit_depth = 8 + vcenc_instance->sps->bit_depth_luma_minus8;
    const int q = av1_ac_quant_obu_Q3(q_index, 0, bit_depth);
    // Keyframes: filt_guess = q * 0.06699 - 1.60817
    // Other frames: filt_guess = q * 0.02295 + 2.48225
    int filt_guess,filt_uv_guess;

#if 0
    switch (bit_depth) {
      case 8:
        filt_guess = (frame_type == VCENC_AV1_KEY_FRAME)
                         ? ROUND_POWER_OF_TWO(q * 17563 - 421574, 18)
                         : ROUND_POWER_OF_TWO(q * 6017 + 650707, 18);
        break;
      case 10:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 4060632, 20);
        break;
      case 12:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 16242526, 22);
        break;
      default:
        ASSERT(0 &&
               "bit_depth should be AOM_BITS_8, AOM_BITS_10 "
               "or AOM_BITS_12");
        return;
    }
    if (bit_depth != 8 && frame_type == VCENC_AV1_KEY_FRAME)
      filt_guess -= 4;

    filt_uv_guess = filt_guess;
    filt_guess    = filt_uv_guess+3;
#endif

    filt_guess = ROUND_POWER_OF_TWO(q * 100, 10) - 2; //currently best
    filt_uv_guess = filt_guess - 3;
    //filt_guess = qp_to_lf[(vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS)];
    //filt_uv_guess = filt_guess - 3;

    i32 minLevel = vcenc_instance->av1_inst.delta_lf_present_flag ? 1 : 0;
    vcenc_instance->av1_inst.filter_level[0] = clamp(filt_guess, minLevel, 63);
    vcenc_instance->av1_inst.filter_level[1] = clamp(filt_guess, minLevel, 63);
    vcenc_instance->av1_inst.filter_level_u  = clamp(filt_uv_guess, minLevel, 63);
    vcenc_instance->av1_inst.filter_level_v  = clamp(filt_uv_guess, minLevel, 63);
    vcenc_instance->av1_inst.sharpness_level = 3;
}


void av1_pick_cdef_para(struct vcenc_instance *vcenc_instance, VCENC_AV1_FRAME_TYPE frame_type)
{
    static int cdef_y_strenth_conv0[8] = { 8, 8, 8, 12, 12, 34, 39, 42 };
    static int cdef_uv_strenth_conv0[8] = { 8, 8, 8, 12, 12, 20, 24, 40 };

    static int cdef_y_strenth_conv[8] = { 1, 8, 12, 13, 15, 21, 21, 21 };
    static int cdef_uv_strenth_conv[8] = { 1, 8, 12, 12, 16, 20, 20, 20 };

    static int cdef_y_strenth_conv2[8] = { 4, 4, 4, 8, 12,   21, 21, 21 };
    static int cdef_uv_strenth_conv2[8] = { 4, 4, 8, 12, 12,   20, 20, 20 };

    int q_index = quantizer_to_qindex[(vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS)];
    int cdef_strengths = cdef_y_strenth_conv[q_index>>5];
    int cdef_uv_strengths = cdef_uv_strenth_conv[q_index>>5];
	u32 cdef_strength_index;

    if(vcenc_instance->width <1024 )
    {
      cdef_strengths    = cdef_y_strenth_conv0[q_index>>5];
      cdef_uv_strengths = cdef_uv_strenth_conv0[q_index>>5];
    }
	else if(vcenc_instance->pass == 2){
	  	cdef_strengths    = cdef_y_strenth_conv2[q_index>>5];
        cdef_uv_strengths = cdef_uv_strenth_conv2[q_index>>5];
	}

    int cdef_damping = 3 + (q_index >> 6);

    if (cdef_damping>6) cdef_damping= 6;

    vcenc_instance->av1_inst.cdef_damping          = cdef_damping;
    vcenc_instance->av1_inst.cdef_bits             = 0;
    vcenc_instance->av1_inst.cdef_strengths[0]     = cdef_strengths;
    vcenc_instance->av1_inst.cdef_uv_strengths[0]  = cdef_uv_strengths;
    vcenc_instance->sps->enable_cdef               = vcenc_instance->sps->sao_enabled_flag;

#ifdef CDEF_BLOCK_STRENGTH_ENABLE
	if(vcenc_instance->pass == 1){
        vcenc_instance->av1_inst.cdef_bits      = 0;

		for(int i = 0; i < (1<<vcenc_instance->av1_inst.cdef_bits); i++){
	        cdef_y_strenth[vcenc_instance->pass-1][i]      = cdef_strengths;
            cdef_uv_strenth[vcenc_instance->pass-1][i]     = cdef_uv_strengths;
		    vcenc_instance->av1_inst.cdef_strengths[i]     = cdef_strengths;
            vcenc_instance->av1_inst.cdef_uv_strengths[i]  = cdef_uv_strengths;
	    }
	}
	else{
		i32 pass = 1;
		vcenc_instance->av1_inst.cdef_bits      = 3;

        typedef struct av1_cdef_tbl
        {
            u8 qidx;
            u8 yS;
            u8 uvS;
        } av1_cdef_tbl_t;
    
        static const av1_cdef_tbl_t cdef_tbl[16] = {
            {5,   0,   0},
            {9,   0,   1},
            {33,  1,   1},
            {43,  1,   4},
            {56,  5,   4},
            {93,  5,   8},
            {107, 13,  8},
            {136, 17,  8},
            {150, 26,  12},
            {163, 38,  12},
            {175, 46,  16},
            {200, 55,  28},
            {212, 59,  33},
            {224, 63,  41},
            {237, 63,  46},
            {248, 63,  51}
        };
        int sIdx = 0;
        int qIdxDiff = ABS(q_index - cdef_tbl[0].qidx);
        for (i32 i = 1; i < 16; i ++)
        {
            i32 diff = ABS(q_index - cdef_tbl[i].qidx);
            if (diff < qIdxDiff)
            {
                qIdxDiff = diff;
                sIdx = i;
            }
        }
        cdef_strengths    = cdef_tbl[sIdx].yS;
        cdef_uv_strengths = cdef_tbl[sIdx].uvS;
        i32 nS = 1 << vcenc_instance->av1_inst.cdef_bits;
        sIdx = CLIP3(0, 16 - nS, sIdx - (nS/2));
        for (i32 i = 0; i < nS; i ++)
        {
            cdef_y_strenth[pass][i]  = cdef_tbl[sIdx + i].yS;
            cdef_uv_strenth[pass][i] = cdef_tbl[sIdx + i].uvS;
			vcenc_instance->av1_inst.cdef_strengths[i]     = cdef_tbl[sIdx + i].yS;
            vcenc_instance->av1_inst.cdef_uv_strengths[i]  = cdef_tbl[sIdx + i].uvS;
        }
    }
#endif
}

/*------------------------------------------------------------------------------
  set_parameter
------------------------------------------------------------------------------*/
i32 set_parameter(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn,
                  struct vps *v, struct sps *s, struct pps *p)
{
  struct container *c;
  struct vcenc_buffer source;
  i32 tmp;
  i32 i;

  if (!(c = get_container(vcenc_instance))) goto out;
  if (!v || !s || !p) goto out;

  /* Initialize stream buffers */
  if (init_buffer(&source, vcenc_instance)) goto out;
  if (get_buffer(&v->ps.b, &source, PRM_SET_BUFF_SIZE, HANTRO_TRUE)) goto out;
#ifdef TEST_DATA
  Enc_open_stream_trace(&v->ps.b);
#endif

  if (get_buffer(&s->ps.b, &source, PRM_SET_BUFF_SIZE, HANTRO_TRUE)) goto out;
#ifdef TEST_DATA
  Enc_open_stream_trace(&s->ps.b);
#endif

  if (get_buffer(&p->ps.b, &source, PRM_SET_BUFF_SIZE, HANTRO_TRUE)) goto out;
#ifdef TEST_DATA
  Enc_open_stream_trace(&p->ps.b);
#endif


  /* Coding unit sizes */
  if (log2i(vcenc_instance->min_cu_size, &tmp)) goto out;
  if (check_range(tmp, 3, 3)) goto out;
  s->log2_min_cb_size = tmp;

  if (log2i(vcenc_instance->max_cu_size, &tmp)) goto out;
  i32 log2_ctu_size = (IS_H264(vcenc_instance->codecFormat)  ? 4 : 6);
  if (check_range(tmp, log2_ctu_size, log2_ctu_size)) goto out;
  s->log2_diff_cb_size = tmp - s->log2_min_cb_size;
  p->log2_ctb_size = s->log2_min_cb_size + s->log2_diff_cb_size;
  p->ctb_size = 1 << p->log2_ctb_size;
  ASSERT(p->ctb_size == vcenc_instance->max_cu_size);

  /* Transform size */
  if (log2i(vcenc_instance->min_tr_size, &tmp)) goto out;
  if (check_range(tmp, 2, 2)) goto out;
  s->log2_min_tr_size = tmp;

  if (log2i(vcenc_instance->max_tr_size, &tmp)) goto out;
  if (check_range(tmp, s->log2_min_tr_size, MIN(p->log2_ctb_size, 5)))
    goto out;
  s->log2_diff_tr_size = tmp - s->log2_min_tr_size;
  p->log2_max_tr_size = s->log2_min_tr_size + s->log2_diff_tr_size;
  ASSERT(1 << p->log2_max_tr_size == vcenc_instance->max_tr_size);

  /* Max transform hierarchy depth intra/inter */
  tmp = p->log2_ctb_size - s->log2_min_tr_size;
  if (check_range(vcenc_instance->tr_depth_intra, 0, tmp)) goto out;
  s->max_tr_hierarchy_depth_intra = vcenc_instance->tr_depth_intra;

  if (check_range(vcenc_instance->tr_depth_inter, 0, tmp)) goto out;
  s->max_tr_hierarchy_depth_inter = vcenc_instance->tr_depth_inter;

  s->scaling_list_enable_flag = vcenc_instance->enableScalingList;

  /* Parameter set id */
  if (check_range(vcenc_instance->vps_id, 0, 15)) goto out;
  v->ps.id = vcenc_instance->vps_id;

  if (check_range(vcenc_instance->sps_id, 0, 15)) goto out;
  s->ps.id = vcenc_instance->sps_id;
  s->vps_id = v->ps.id;

  if (check_range(vcenc_instance->pps_id, 0, 63)) goto out;
  p->ps.id = vcenc_instance->pps_id;
  p->sps_id = s->ps.id;

  /* TODO image cropping parameters */
  if (!(vcenc_instance->width > 0)) goto out;
  if (!(vcenc_instance->height > 0)) goto out;
  s->width = vcenc_instance->width;
  s->height = vcenc_instance->height;

  /* strong_intra_smoothing_enabled_flag */
  //if (check_range(s->strong_intra_smoothing_enabled_flag, 0, 1)) goto out;
  s->strong_intra_smoothing_enabled_flag = vcenc_instance->strong_intra_smoothing_enabled_flag;
  ASSERT((s->strong_intra_smoothing_enabled_flag == 0) || (s->strong_intra_smoothing_enabled_flag == 1));

  /* Default quantization parameter */
  if (check_range(vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS, 0, 51)) goto out;
  p->init_qp = vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS;

  /* Picture level rate control TODO... */
  if (check_range(vcenc_instance->rateControl.picRc, 0, 1)) goto out;

  p->cu_qp_delta_enabled_flag = ((vcenc_instance->asic.regs.rcRoiEnable != 0) || vcenc_instance->gdrDuration != 0) ? 1 : 0;
#ifdef INTERNAL_TEST
  p->cu_qp_delta_enabled_flag = (vcenc_instance->testId == TID_ROI) ? 1 : p->cu_qp_delta_enabled_flag;
#endif

  if (p->cu_qp_delta_enabled_flag == 0 && vcenc_instance->rateControl.ctbRc)
  {
    p->cu_qp_delta_enabled_flag = 1;
  }

  vcenc_instance->av1_inst.delta_q_present_flag = p->cu_qp_delta_enabled_flag;

  vcenc_instance->av1_inst.delta_lf_present_flag = 0;
#ifdef DELTA_LF_ENABLE
  /* Currently set to be the same as delta q flag
     Todo: separated option */
  if(p->cu_qp_delta_enabled_flag && (vcenc_instance->disableDeblocking==0))
  {
    vcenc_instance->av1_inst.delta_lf_present_flag = 1;
    vcenc_instance->av1_inst.delta_lf_res = DEFAULT_DELTA_LF_RES;
    vcenc_instance->av1_inst.delta_lf_multi = DEFAULT_DELTA_LF_MULTI;
  }
#endif

  if (log2i(vcenc_instance->min_qp_size, &tmp)) goto out;
#if 1
  /* Get the qp size from command line. */
  p->diff_cu_qp_delta_depth = p->log2_ctb_size - tmp;
#else
  /* Make qp size the same as ctu now. FIXME: when qp size is different. */
  p->diff_cu_qp_delta_depth = 0;
#endif

  if (check_range(tmp, s->log2_min_cb_size, p->log2_ctb_size)) goto out;

  //tile
  p->tiles_enabled_flag = vcenc_instance->tiles_enabled_flag;
  p->num_tile_columns = vcenc_instance->num_tile_columns;
  p->num_tile_rows = vcenc_instance->num_tile_rows;
  p->loop_filter_across_tiles_enabled_flag = vcenc_instance->loop_filter_across_tiles_enabled_flag;

  /* Init the rest. TODO: tiles are not supported yet. How to get
   * slice/tiles from test_bench.c ? */
  if (init_parameter_set(s, p)) goto out;
  p->deblocking_filter_disabled_flag = vcenc_instance->disableDeblocking;
  p->tc_offset = vcenc_instance->tc_Offset * 2;
  p->beta_offset = vcenc_instance->beta_Offset * 2;
  p->deblocking_filter_override_enabled_flag = vcenc_instance->enableDeblockOverride;


  p->cb_qp_offset = vcenc_instance->chromaQpOffset;
  p->cr_qp_offset = vcenc_instance->chromaQpOffset;

  s->sao_enabled_flag = vcenc_instance->enableSao;

  s->frameCropLeftOffset = vcenc_instance->preProcess.frameCropLeftOffset;
  s->frameCropRightOffset = vcenc_instance->preProcess.frameCropRightOffset;
  s->frameCropTopOffset = vcenc_instance->preProcess.frameCropTopOffset;
  s->frameCropBottomOffset = vcenc_instance->preProcess.frameCropBottomOffset;

  v->streamMode = vcenc_instance->asic.regs.streamMode;
  s->streamMode = vcenc_instance->asic.regs.streamMode;
  p->streamMode = vcenc_instance->asic.regs.streamMode;

  v->general_level_idc = vcenc_instance->level;
  s->general_level_idc = vcenc_instance->level;

  v->general_profile_idc = vcenc_instance->profile;
  s->general_profile_idc = vcenc_instance->profile;

  if (IS_AV1(vcenc_instance->codecFormat)){
      v->general_level_idc = 4;
      s->general_level_idc = 4;
      v->general_profile_idc = VCENC_AV1_MAIN_PROFILE;
      s->general_profile_idc = VCENC_AV1_MAIN_PROFILE;
  }

  /*TODO: VP9 read profile from user*/
  if(IS_VP9(vcenc_instance->codecFormat)){
    v->general_profile_idc = VCENC_VP9_MAIN_PROFILE;
    s->general_profile_idc = VCENC_VP9_MAIN_PROFILE;
  }
  s->chroma_format_idc = vcenc_instance->asic.regs.codedChromaIdc;

  v->general_tier_flag = vcenc_instance->tier;
  s->general_tier_flag = vcenc_instance->tier;

  s->pcm_enabled_flag = vcenc_instance->pcm_enabled_flag;
  s->pcm_sample_bit_depth_luma_minus1 = vcenc_instance->sps->bit_depth_luma_minus8+7;
  s->pcm_sample_bit_depth_chroma_minus1 = vcenc_instance->sps->bit_depth_chroma_minus8+7;
  s->pcm_loop_filter_disabled_flag = vcenc_instance->pcm_loop_filter_disabled_flag;

  /* Set H264 sps*/
  if (IS_H264(vcenc_instance->codecFormat))
  {
    /* Internal images, next macroblock boundary */
    i32 width = 16 * ((s->width + 15) / 16);
    i32 height = 16 * ((s->height + 15) / 16);
    i32 mbPerFrame = (width/16) * (height/16);
    if (vcenc_instance->interlaced)
    {
      s->frameMbsOnly = ENCHW_NO;
      /* Map unit 32-pixels high for fields */
      height = 32 * ((s->height + 31) / 32);
    }
    s->picWidthInMbsMinus1 = width / 16 - 1;
    s->picHeightInMapUnitsMinus1 = height / (16*(1+vcenc_instance->interlaced)) - 1;

    /* Set cropping parameters if required */
    if( s->width%16 || s->height%16 ||
        (vcenc_instance->interlaced && s->height%32))
    {
      s->frameCropping = ENCHW_YES;
    }

    /* Level 1b is indicated with levelIdc == 11 (later) and constraintSet3 */
    if(vcenc_instance->level == VCENC_H264_LEVEL_1_b)
    {
      s->constraintSet3 = ENCHW_YES;
    }

    /* Level 1b is indicated with levelIdc == 11 (constraintSet3) */
    if(vcenc_instance->level == VCENC_H264_LEVEL_1_b)
    {
        s->general_level_idc = 11;
    }

    if (IS_AV1(vcenc_instance->codecFormat)){
        ASSERT(s->general_level_idc < 24);
        s->general_level_idc = av1_level_idc[s->general_level_idc];
        ASSERT(s->general_level_idc <= 7);
    }

    /* Interlaced => main profile */
    if (vcenc_instance->interlaced >= 1 && s->general_profile_idc == 66)
      s->general_profile_idc = 77;

    p->transform8x8Mode = (s->general_profile_idc >= 100 ? ENCHW_YES : ENCHW_NO);
    p->entropy_coding_mode_flag = vcenc_instance->asic.regs.entropy_coding_mode_flag;

    /* always CABAC => main profile */
    if (s->general_profile_idc == 66) {
      ASSERT(p->entropy_coding_mode_flag == 0);
    }

    /*  always 8x8 transform enabled => high profile */
    ASSERT(p->transform8x8Mode == ENCHW_NO || s->general_profile_idc >= 100);

    /* set numRefFrame*/
    s->numRefFrames = s->max_dec_pic_buffering[0] - 1;
    vcenc_instance->frameNum = 0;
    vcenc_instance->idrPicId = 0;
    vcenc_instance->h264_mmo_nops = 0;
  }

  vcenc_instance->asic.regs.outputBitWidthLuma = s->bit_depth_luma_minus8;
  vcenc_instance->asic.regs.outputBitWidthChroma = s->bit_depth_chroma_minus8;

  p->lists_modification_present_flag = check_ref_lists_modification(pEncIn);
  if(pEncIn->gopConfig.ltrcnt > 0)
    s->long_term_ref_pics_present_flag = 1;
  return VCENC_OK;

out:
  /*
    The memory instances are allocated in VCEncInit()
    should be released in VCEncRelease()
    */
  //free_parameter_set((struct ps *)v);
  //free_parameter_set((struct ps *)s);
  //free_parameter_set((struct ps *)p);

  return VCENC_ERROR;
}


VCEncRet VCEncGetCodingCtrl(VCEncInst inst,
                                VCEncCodingCtrl *pCodeParams)
{
  struct vcenc_instance  *pEncInst = (struct vcenc_instance *) inst;
  regValues_s *regs = &pEncInst->asic.regs;
  i32 i;
  APITRACE("VCEncGetCodingCtrl#");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pCodeParams == NULL))
  {
    APITRACEERR("VCEncGetCodingCtrl: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst)
  {
    APITRACEERR("VCEncGetCodingCtrl: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  pCodeParams->seiMessages =
    (pEncInst->rateControl.sei.enabled == ENCHW_YES) ? 1 : 0;

  pCodeParams->disableDeblockingFilter = pEncInst->disableDeblocking;
  pCodeParams->fieldOrder = pEncInst->fieldOrder;

  pCodeParams->tc_Offset = pEncInst->tc_Offset;
  pCodeParams->beta_Offset = pEncInst->beta_Offset;
  pCodeParams->enableSao = pEncInst->enableSao;
  pCodeParams->enableScalingList = pEncInst->enableScalingList;
  pCodeParams->videoFullRange =
    (pEncInst->sps->vui.videoFullRange == ENCHW_YES) ? 1 : 0;
  pCodeParams->sampleAspectRatioWidth = pEncInst->sarWidth;
  pCodeParams->sampleAspectRatioHeight = pEncInst->sarHeight;
  pCodeParams->sliceSize = regs->sliceSize;

  pCodeParams->cabacInitFlag = regs->cabac_init_flag;
  pCodeParams->enableCabac = regs->entropy_coding_mode_flag;

  pCodeParams->cirStart = regs->cirStart;
  pCodeParams->cirInterval = regs->cirInterval;
  pCodeParams->intraArea.top    = regs->intraAreaTop;
  pCodeParams->intraArea.left   = regs->intraAreaLeft;
  pCodeParams->intraArea.bottom = regs->intraAreaBottom;
  pCodeParams->intraArea.right  = regs->intraAreaRight;
  if ((pCodeParams->intraArea.top <= pCodeParams->intraArea.bottom &&
       pCodeParams->intraArea.bottom < (u32)pEncInst->ctbPerCol &&
       pCodeParams->intraArea.left <= pCodeParams->intraArea.right &&
       pCodeParams->intraArea.right < (u32)pEncInst->ctbPerRow))
  {
    pCodeParams->intraArea.enable = 1;
  }
  else
  {
    pCodeParams->intraArea.enable = 0;
  }

  pCodeParams->ipcm1Area.top    = regs->ipcm1AreaTop;
  pCodeParams->ipcm1Area.left   = regs->ipcm1AreaLeft;
  pCodeParams->ipcm1Area.bottom = regs->ipcm1AreaBottom;
  pCodeParams->ipcm1Area.right  = regs->ipcm1AreaRight;
  if ((pCodeParams->ipcm1Area.top <= pCodeParams->ipcm1Area.bottom &&
       pCodeParams->ipcm1Area.bottom < (u32)pEncInst->ctbPerCol &&
       pCodeParams->ipcm1Area.left <= pCodeParams->ipcm1Area.right &&
       pCodeParams->ipcm1Area.right < (u32)pEncInst->ctbPerRow))
  {
    pCodeParams->ipcm1Area.enable = 1;
  }
  else
  {
    pCodeParams->ipcm1Area.enable = 0;
  }

  pCodeParams->ipcm2Area.top    = regs->ipcm2AreaTop;
  pCodeParams->ipcm2Area.left   = regs->ipcm2AreaLeft;
  pCodeParams->ipcm2Area.bottom = regs->ipcm2AreaBottom;
  pCodeParams->ipcm2Area.right  = regs->ipcm2AreaRight;
  if ((pCodeParams->ipcm2Area.top <= pCodeParams->ipcm2Area.bottom &&
       pCodeParams->ipcm2Area.bottom < (u32)pEncInst->ctbPerCol &&
       pCodeParams->ipcm2Area.left <= pCodeParams->ipcm2Area.right &&
       pCodeParams->ipcm2Area.right < (u32)pEncInst->ctbPerRow))
  {
    pCodeParams->ipcm2Area.enable = 1;
  }
  else
  {
    pCodeParams->ipcm2Area.enable = 0;
  }

  pCodeParams->roi1Area.top    = regs->roi1Top;
  pCodeParams->roi1Area.left   = regs->roi1Left;
  pCodeParams->roi1Area.bottom = regs->roi1Bottom;
  pCodeParams->roi1Area.right  = regs->roi1Right;
  if (pCodeParams->roi1Area.top <= pCodeParams->roi1Area.bottom &&
      pCodeParams->roi1Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi1Area.left <= pCodeParams->roi1Area.right &&
      pCodeParams->roi1Area.right < (u32)pEncInst->ctbPerRow)
  {
    pCodeParams->roi1Area.enable = 1;
  }
  else
  {
    pCodeParams->roi1Area.enable = 0;
  }

  pCodeParams->roi2Area.top    = regs->roi2Top;
  pCodeParams->roi2Area.left   = regs->roi2Left;
  pCodeParams->roi2Area.bottom = regs->roi2Bottom;
  pCodeParams->roi2Area.right  = regs->roi2Right;
  if ((pCodeParams->roi2Area.top <= pCodeParams->roi2Area.bottom &&
       pCodeParams->roi2Area.bottom < (u32)pEncInst->ctbPerCol &&
       pCodeParams->roi2Area.left <= pCodeParams->roi2Area.right &&
       pCodeParams->roi2Area.right < (u32)pEncInst->ctbPerRow))
  {
    pCodeParams->roi2Area.enable = 1;
  }
  else
  {
    pCodeParams->roi2Area.enable = 0;
  }

  pCodeParams->roi1DeltaQp = -regs->roi1DeltaQp;
  pCodeParams->roi2DeltaQp = -regs->roi2DeltaQp;
  pCodeParams->roi1Qp = regs->roi1Qp;
  pCodeParams->roi2Qp = regs->roi2Qp;

  pCodeParams->roi3Area.top = regs->roi3Top;
  pCodeParams->roi3Area.left = regs->roi3Left;
  pCodeParams->roi3Area.bottom = regs->roi3Bottom;
  pCodeParams->roi3Area.right = regs->roi3Right;
  if ((pCodeParams->roi3Area.top <= pCodeParams->roi3Area.bottom &&
      pCodeParams->roi3Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi3Area.left <= pCodeParams->roi3Area.right &&
      pCodeParams->roi3Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi3Area.enable = 1;
  }
  else
  {
      pCodeParams->roi3Area.enable = 0;
  }

  pCodeParams->roi4Area.top = regs->roi4Top;
  pCodeParams->roi4Area.left = regs->roi4Left;
  pCodeParams->roi4Area.bottom = regs->roi4Bottom;
  pCodeParams->roi4Area.right = regs->roi4Right;
  if ((pCodeParams->roi4Area.top <= pCodeParams->roi4Area.bottom &&
      pCodeParams->roi4Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi4Area.left <= pCodeParams->roi4Area.right &&
      pCodeParams->roi4Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi4Area.enable = 1;
  }
  else
  {
      pCodeParams->roi4Area.enable = 0;
  }

  pCodeParams->roi5Area.top = regs->roi5Top;
  pCodeParams->roi5Area.left = regs->roi5Left;
  pCodeParams->roi5Area.bottom = regs->roi5Bottom;
  pCodeParams->roi5Area.right = regs->roi5Right;
  if ((pCodeParams->roi5Area.top <= pCodeParams->roi5Area.bottom &&
      pCodeParams->roi5Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi5Area.left <= pCodeParams->roi5Area.right &&
      pCodeParams->roi5Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi5Area.enable = 1;
  }
  else
  {
      pCodeParams->roi5Area.enable = 0;
  }

  pCodeParams->roi6Area.top = regs->roi6Top;
  pCodeParams->roi6Area.left = regs->roi6Left;
  pCodeParams->roi6Area.bottom = regs->roi6Bottom;
  pCodeParams->roi6Area.right = regs->roi6Right;
  if ((pCodeParams->roi6Area.top <= pCodeParams->roi6Area.bottom &&
      pCodeParams->roi6Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi6Area.left <= pCodeParams->roi6Area.right &&
      pCodeParams->roi6Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi6Area.enable = 1;
  }
  else
  {
      pCodeParams->roi6Area.enable = 0;
  }

  pCodeParams->roi7Area.top = regs->roi7Top;
  pCodeParams->roi7Area.left = regs->roi7Left;
  pCodeParams->roi7Area.bottom = regs->roi7Bottom;
  pCodeParams->roi7Area.right = regs->roi7Right;
  if ((pCodeParams->roi7Area.top <= pCodeParams->roi7Area.bottom &&
      pCodeParams->roi7Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi7Area.left <= pCodeParams->roi7Area.right &&
      pCodeParams->roi7Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi7Area.enable = 1;
  }
  else
  {
      pCodeParams->roi7Area.enable = 0;
  }

  pCodeParams->roi8Area.top = regs->roi8Top;
  pCodeParams->roi8Area.left = regs->roi8Left;
  pCodeParams->roi8Area.bottom = regs->roi8Bottom;
  pCodeParams->roi8Area.right = regs->roi8Right;
  if ((pCodeParams->roi8Area.top <= pCodeParams->roi8Area.bottom &&
      pCodeParams->roi8Area.bottom < (u32)pEncInst->ctbPerCol &&
      pCodeParams->roi8Area.left <= pCodeParams->roi8Area.right &&
      pCodeParams->roi8Area.right < (u32)pEncInst->ctbPerRow))
  {
      pCodeParams->roi8Area.enable = 1;
  }
  else
  {
      pCodeParams->roi8Area.enable = 0;
  }

  pCodeParams->roi3DeltaQp = -regs->roi3DeltaQp;
  pCodeParams->roi4DeltaQp = -regs->roi4DeltaQp;
  pCodeParams->roi5DeltaQp = -regs->roi5DeltaQp;
  pCodeParams->roi6DeltaQp = -regs->roi6DeltaQp;
  pCodeParams->roi7DeltaQp = -regs->roi7DeltaQp;
  pCodeParams->roi8DeltaQp = -regs->roi8DeltaQp;
  pCodeParams->roi3Qp = regs->roi3Qp;
  pCodeParams->roi4Qp = regs->roi4Qp;
  pCodeParams->roi5Qp = regs->roi5Qp;
  pCodeParams->roi6Qp = regs->roi6Qp;
  pCodeParams->roi7Qp = regs->roi7Qp;
  pCodeParams->roi8Qp = regs->roi8Qp;

  pCodeParams->pcm_enabled_flag = pEncInst->pcm_enabled_flag;
  pCodeParams->pcm_loop_filter_disabled_flag = pEncInst->pcm_loop_filter_disabled_flag;


  pCodeParams->enableDeblockOverride = pEncInst->enableDeblockOverride;

  pCodeParams->deblockOverride = regs->slice_deblocking_filter_override_flag;

  pCodeParams->chroma_qp_offset = pEncInst->chromaQpOffset;

  pCodeParams->roiMapDeltaQpEnable = pEncInst->roiMapEnable;
  pCodeParams->roiMapDeltaQpBlockUnit = regs->diffCuQpDeltaDepth;
  pCodeParams->ipcmMapEnable = regs->ipcmMapEnable;
  pCodeParams->RoimapCuCtrl_index_enable = pEncInst->RoimapCuCtrl_index_enable;
  pCodeParams->RoimapCuCtrl_enable       = pEncInst->RoimapCuCtrl_enable;
  pCodeParams->RoimapCuCtrl_ver          = pEncInst->RoimapCuCtrl_ver;
  pCodeParams->RoiQpDelta_ver            = pEncInst->RoiQpDelta_ver;

  /* SKIP map */
  pCodeParams->skipMapEnable = regs->skipMapEnable;

  pCodeParams->gdrDuration = pEncInst->gdrEnabled ? pEncInst->gdrDuration : 0;

  VCEncHEVCDnfGetParameters(pEncInst, pCodeParams);

  /* low latency */
  pCodeParams->inputLineBufEn = pEncInst->inputLineBuf.inputLineBufEn;
  pCodeParams->inputLineBufLoopBackEn = pEncInst->inputLineBuf.inputLineBufLoopBackEn;
  pCodeParams->inputLineBufDepth = pEncInst->inputLineBuf.inputLineBufDepth;
  pCodeParams->amountPerLoopBack = pEncInst->inputLineBuf.amountPerLoopBack;
  pCodeParams->inputLineBufHwModeEn = pEncInst->inputLineBuf.inputLineBufHwModeEn;
  pCodeParams->inputLineBufCbFunc = pEncInst->inputLineBuf.cbFunc;
  pCodeParams->inputLineBufCbData = pEncInst->inputLineBuf.cbData;

  /*stream multi-segment*/
  pCodeParams->streamMultiSegmentMode = pEncInst->streamMultiSegment.streamMultiSegmentMode;
  pCodeParams->streamMultiSegmentAmount = pEncInst->streamMultiSegment.streamMultiSegmentAmount;
  pCodeParams->streamMultiSegCbFunc = pEncInst->streamMultiSegment.cbFunc;
  pCodeParams->streamMultiSegCbData = pEncInst->streamMultiSegment.cbData;

  /* smart */
  pCodeParams->smartModeEnable = pEncInst->smartModeEnable;
  pCodeParams->smartH264LumDcTh = pEncInst->smartH264LumDcTh;
  pCodeParams->smartH264CbDcTh = pEncInst->smartH264CbDcTh;
  pCodeParams->smartH264CrDcTh = pEncInst->smartH264CrDcTh;
  for(i = 0; i < 3; i++) {
    pCodeParams->smartHevcLumDcTh[i] = pEncInst->smartHevcLumDcTh[i];
    pCodeParams->smartHevcChrDcTh[i] = pEncInst->smartHevcChrDcTh[i];
    pCodeParams->smartHevcLumAcNumTh[i] = pEncInst->smartHevcLumAcNumTh[i];
    pCodeParams->smartHevcChrAcNumTh[i] = pEncInst->smartHevcChrAcNumTh[i];
  }
  pCodeParams->smartH264Qp = pEncInst->smartH264Qp;
  pCodeParams->smartHevcLumQp = pEncInst->smartHevcLumQp;
  pCodeParams->smartHevcChrQp = pEncInst->smartHevcChrQp;
  for(i = 0; i < 4; i++)
    pCodeParams->smartMeanTh[i] = pEncInst->smartMeanTh[i];
  pCodeParams->smartPixNumCntTh = pEncInst->smartPixNumCntTh;

  pCodeParams->tiles_enabled_flag = pEncInst->tiles_enabled_flag;
  pCodeParams->num_tile_columns = pEncInst->num_tile_columns;
  pCodeParams->num_tile_rows = pEncInst->num_tile_rows;
  pCodeParams->loop_filter_across_tiles_enabled_flag = pEncInst->loop_filter_across_tiles_enabled_flag;

  pCodeParams->Hdr10Display.hdr10_display_enable = pEncInst->Hdr10Display.hdr10_display_enable;
  pCodeParams->Hdr10Display.hdr10_dx0 = pEncInst->Hdr10Display.hdr10_dx0;
  pCodeParams->Hdr10Display.hdr10_dy0 = pEncInst->Hdr10Display.hdr10_dy0;
  pCodeParams->Hdr10Display.hdr10_dx1 = pEncInst->Hdr10Display.hdr10_dx1;
  pCodeParams->Hdr10Display.hdr10_dy1 = pEncInst->Hdr10Display.hdr10_dy1;
  pCodeParams->Hdr10Display.hdr10_dx2 = pEncInst->Hdr10Display.hdr10_dx2;
  pCodeParams->Hdr10Display.hdr10_dy2 = pEncInst->Hdr10Display.hdr10_dy2;
  pCodeParams->Hdr10Display.hdr10_wx  = pEncInst->Hdr10Display.hdr10_wx;
  pCodeParams->Hdr10Display.hdr10_wy  = pEncInst->Hdr10Display.hdr10_wy;
  pCodeParams->Hdr10Display.hdr10_maxluma = pEncInst->Hdr10Display.hdr10_maxluma;
  pCodeParams->Hdr10Display.hdr10_minluma = pEncInst->Hdr10Display.hdr10_minluma;
  pCodeParams->Hdr10Color.hdr10_color_enable = pEncInst->Hdr10Color.hdr10_color_enable;
  pCodeParams->Hdr10Color.hdr10_matrix   = pEncInst->Hdr10Color.hdr10_matrix;
  pCodeParams->Hdr10Color.hdr10_primary  = pEncInst->Hdr10Color.hdr10_primary;
  pCodeParams->Hdr10Color.hdr10_transfer = pEncInst->Hdr10Color.hdr10_transfer;


  pCodeParams->Hdr10LightLevel.hdr10_lightlevel_enable=pEncInst->Hdr10LightLevel.hdr10_lightlevel_enable;
  pCodeParams->Hdr10LightLevel.hdr10_maxlight=pEncInst->Hdr10LightLevel.hdr10_maxlight;
  pCodeParams->Hdr10LightLevel.hdr10_avglight=pEncInst->Hdr10LightLevel.hdr10_avglight;

  pCodeParams->RpsInSliceHeader = pEncInst->RpsInSliceHeader;

  pCodeParams->enableRdoQuant = regs->bRDOQEnable;

  /* Assigned ME vertical search range */
  pCodeParams->meVertSearchRange = regs->meAssignedVertRange << 3;

  APITRACE("VCEncGetCodingCtrl: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VCEncSetRateCtrl
    Description   : Sets rate control parameters

    Return type   : VCEncRet
    Argument      : inst - the instance in use
                    pRateCtrl - user provided parameters
------------------------------------------------------------------------------*/
VCEncRet VCEncSetRateCtrl(VCEncInst inst, const VCEncRateCtrl *pRateCtrl)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  vcencRateControl_s *rc;
  u32 i, tmp;
  i32 prevBitrate;
  regValues_s *regs = &vcenc_instance->asic.regs;
  i32 bitrateWindow;
  float h264_high10_factor = 1;
  if(vcenc_instance->profile == 100)
    h264_high10_factor = 1.25;
  else if(vcenc_instance->profile == 110)
    h264_high10_factor = 3.0;

  APITRACE("VCEncSetRateCtrl#");
  APITRACEPARAM("pictureRc", pRateCtrl->pictureRc);
  APITRACEPARAM("pictureSkip", pRateCtrl->pictureSkip);
  APITRACEPARAM("qpHdr", pRateCtrl->qpHdr);
  APITRACEPARAM("qpMinPB", pRateCtrl->qpMinPB);
  APITRACEPARAM("qpMaxPB", pRateCtrl->qpMaxPB);
  APITRACEPARAM("qpMinI", pRateCtrl->qpMinI);
  APITRACEPARAM("qpMaxI", pRateCtrl->qpMaxI);
  APITRACEPARAM("bitPerSecond", pRateCtrl->bitPerSecond);
  APITRACEPARAM("hrd", pRateCtrl->hrd);
  APITRACEPARAM("hrdCpbSize", pRateCtrl->hrdCpbSize);
  APITRACEPARAM("bitrateWindow", pRateCtrl->bitrateWindow);
  APITRACEPARAM("intraQpDelta", pRateCtrl->intraQpDelta);
  APITRACEPARAM("fixedIntraQp", pRateCtrl->fixedIntraQp);
  APITRACEPARAM("bitVarRangeI", pRateCtrl->bitVarRangeI);
  APITRACEPARAM("bitVarRangeP", pRateCtrl->bitVarRangeP);
  APITRACEPARAM("bitVarRangeB", pRateCtrl->bitVarRangeB);
  APITRACEPARAM("staticSceneIbitPercent", pRateCtrl->u32StaticSceneIbitPercent);
  APITRACEPARAM("ctbRc", pRateCtrl->ctbRc);
  APITRACEPARAM("blockRCSize", pRateCtrl->blockRCSize);
  APITRACEPARAM("rcQpDeltaRange", pRateCtrl->rcQpDeltaRange);
  APITRACEPARAM("rcBaseMBComplexity", pRateCtrl->rcBaseMBComplexity);
  APITRACEPARAM("picQpDeltaMin", pRateCtrl->picQpDeltaMin);
  APITRACEPARAM("picQpDeltaMax", pRateCtrl->picQpDeltaMax);
  APITRACEPARAM("vbr", pRateCtrl->vbr);
  APITRACEPARAM("ctbRcQpDeltaReverse", pRateCtrl->ctbRcQpDeltaReverse);

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pRateCtrl == NULL))
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  rc = &vcenc_instance->rateControl;
  if (pRateCtrl->ctbRcQpDeltaReverse > 1)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR ctbRcQpDeltaReverse out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  rc->ctbRcQpDeltaReverse = pRateCtrl->ctbRcQpDeltaReverse;


  /* after stream was started with HRD ON,
   * it is not allowed to change RC params */
  if (vcenc_instance->encStatus == VCENCSTAT_START_FRAME && rc->hrd == ENCHW_YES)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Stream started with HRD ON. Not allowed to change any parameters");
    return VCENC_INVALID_STATUS;
  }

  if ((vcenc_instance->encStatus >= VCENCSTAT_START_STREAM)&&(vcenc_instance->ctbRCEnable != pRateCtrl->ctbRc))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid encoding status to config ctbRc");
    return VCENC_INVALID_ARGUMENT;
  }
  if ((HW_ID_MAJOR_NUMBER(regs->asicHwId) < 2 /* H2V1 */) && pRateCtrl->ctbRc)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR CTB RC not supported");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check for invalid input values */
  if (pRateCtrl->pictureRc > 1 ||
      pRateCtrl->pictureSkip > 1 || pRateCtrl->hrd > 1)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid enable/disable value");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->qpHdr > 51 ||
      pRateCtrl->qpMinPB > 51 ||
      pRateCtrl->qpMaxPB > 51 ||
      pRateCtrl->qpMaxPB < pRateCtrl->qpMinPB ||
      pRateCtrl->qpMinI > 51 ||
      pRateCtrl->qpMaxI > 51 ||
      pRateCtrl->qpMaxI < pRateCtrl->qpMinI)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid QP");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((u32)(pRateCtrl->intraQpDelta + 51) > 102)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR intraQpDelta out of range");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->fixedIntraQp > 51)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR fixedIntraQp out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pRateCtrl->bitrateWindow < 1 || pRateCtrl->bitrateWindow > 300)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid GOP length");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pRateCtrl->monitorFrames < LEAST_MONITOR_FRAME || pRateCtrl->monitorFrames > 120)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid monitorFrames");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->blockRCSize > 2 || pRateCtrl->ctbRc > 3 ||
     (IS_AV1(vcenc_instance->codecFormat) && pRateCtrl->blockRCSize > 0)) //AV1 qpDelta in CTU level
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid blockRCSize");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Bitrate affects only when rate control is enabled */
  if ((pRateCtrl->pictureRc ||
       pRateCtrl->pictureSkip || pRateCtrl->hrd) &&
      (((pRateCtrl->bitPerSecond < 10000) &&(rc->outRateNum>rc->outRateDenom)) ||
      ((((pRateCtrl->bitPerSecond *rc->outRateDenom)/ rc->outRateNum)< 10000) && (rc->outRateNum<rc->outRateDenom)) ||
       pRateCtrl->bitPerSecond > VCENC_MAX_BITRATE))
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid bitPerSecond");
    return VCENC_INVALID_ARGUMENT;
  }

  /* HRD and VBR are conflict */
  if (pRateCtrl->hrd && pRateCtrl->vbr)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR HRD and VBR can not be enabled at the same time");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pRateCtrl->picQpDeltaMin > -1) || (pRateCtrl->picQpDeltaMin < -10) ||
      (pRateCtrl->picQpDeltaMax <  1) || (pRateCtrl->picQpDeltaMax >  10))
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR picQpRange out of range. Min:Max should be in [-1,-10]:[1,10]");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->ctbRcRowQpStep < 0)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR ctbRowQpStep out of range");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->ctbRc >= 2 && pRateCtrl->crf >= 0)
  {
    APITRACEERR("VCEncSetRateCtrl: ERROR crf is set with ctbRc>=2");
    return VCENC_INVALID_ARGUMENT;
  }
  {
    u32 cpbSize = pRateCtrl->hrdCpbSize;
    u32 bps = pRateCtrl->bitPerSecond;
    u32 level = vcenc_instance->levelIdx;

    /* Limit maximum bitrate based on resolution and frame rate */
    /* Saturates really high settings */
    /* bits per unpacked frame */
    tmp = (vcenc_instance->sps->bit_depth_chroma_minus8/2 + vcenc_instance->sps->bit_depth_luma_minus8 + 12)
            * vcenc_instance->ctbPerFrame * vcenc_instance->max_cu_size * vcenc_instance->max_cu_size;
    rc->i32MaxPicSize = tmp;
    /* bits per second */
    tmp = MIN((((i64)rcCalculate(tmp, rc->outRateNum, rc->outRateDenom))* 5 / 3),I32_MAX);
    if (bps > (u32)tmp)
      bps = (u32)tmp;

    if (cpbSize == 0)
      cpbSize = (vcenc_instance->tier==VCENC_HEVC_HIGH_TIER ? VCEncMaxCPBSHighTier[level] : VCEncMaxCPBS[level] * h264_high10_factor);
    else if (cpbSize == (u32)(-1))
      cpbSize = bps;

    /* Limit minimum CPB size based on average bits per frame */
    tmp = rcCalculate(bps, rc->outRateDenom, rc->outRateNum);
    cpbSize = MAX(cpbSize, tmp);

    /* cpbSize must be rounded so it is exactly the size written in stream */
    i = 0;
    tmp = cpbSize;
    while (4095 < (tmp >> (4 + i++)));

    cpbSize = (tmp >> (4 + i)) << (4 + i);

    /* if HRD is ON we have to obay all its limits */
    if (pRateCtrl->hrd != 0)
    {
      if (cpbSize > (vcenc_instance->tier==VCENC_HEVC_HIGH_TIER ? VCEncMaxCPBSHighTier[level] : VCEncMaxCPBS[level] * h264_high10_factor))
      {
        APITRACEERR("VCEncSetRateCtrl: ERROR. HRD is ON. hrdCpbSize higher than maximum allowed for stream level");
        return VCENC_INVALID_ARGUMENT;
      }

      if (bps > (vcenc_instance->tier==VCENC_HEVC_HIGH_TIER ? VCEncMaxBRHighTier[level]:VCEncMaxBR[level] * h264_high10_factor))
      {
        APITRACEERR("VCEncSetRateCtrl: ERROR. HRD is ON. bitPerSecond higher than maximum allowed for stream level");
        return VCENC_INVALID_ARGUMENT;
      }
    }

    rc->virtualBuffer.bufferSize = cpbSize;

    /* Set the parameters to rate control */
    if (pRateCtrl->pictureRc != 0)
      rc->picRc = ENCHW_YES;
    else
      rc->picRc = ENCHW_NO;

    /* CTB_RC */
    vcenc_instance->rateControl.rcQpDeltaRange = pRateCtrl->rcQpDeltaRange;
    vcenc_instance->rateControl.rcBaseMBComplexity = pRateCtrl->rcBaseMBComplexity;
    vcenc_instance->rateControl.picQpDeltaMin = pRateCtrl->picQpDeltaMin;
    vcenc_instance->rateControl.picQpDeltaMax = pRateCtrl->picQpDeltaMax;
    if (pRateCtrl->ctbRc)
    {
      u32 maxCtbRcQpDelta = (regs->asicCfg.ctbRcVersion > 1) ? 51 : 15;
      rc->ctbRc = pRateCtrl->ctbRc;

      /* If CTB QP adjustment for Rate Control not supported, just disable it, not return error. */
      if (IS_CTBRC_FOR_BITRATE(rc->ctbRc) &&
          ((regs->asicCfg.ctbRcVersion == 0) || (vcenc_instance->pass == 1)))
      {
        CLR_CTBRC_FOR_BITRATE(rc->ctbRc);
        if (regs->asicCfg.ctbRcVersion == 0)
        {
          APITRACEERR("VCEncSetRateCtrl: ERROR CTB QP adjustment for Rate Control not supported, Disabled it");
        }
      }

      if (vcenc_instance->rateControl.rcQpDeltaRange > maxCtbRcQpDelta)
      {
        vcenc_instance->rateControl.rcQpDeltaRange = maxCtbRcQpDelta;
        APITRACEERR("VCEncSetRateCtrl: rcQpDeltaRange too big, Clipped it into valid range");
      }

      vcenc_instance->ctbRCEnable = rc->ctbRc;
      rc->picRc = ENCHW_YES;
      vcenc_instance->blockRCSize = pRateCtrl->blockRCSize;
      if(vcenc_instance->min_qp_size > (64 >> vcenc_instance->blockRCSize))
         vcenc_instance->min_qp_size = (64 >> vcenc_instance->blockRCSize);
    }
    else
    {
      rc->ctbRc = 0;
      vcenc_instance->ctbRCEnable = 0;
    }

    if (pRateCtrl->pictureSkip != 0)
      rc->picSkip = ENCHW_YES;
    else
      rc->picSkip = ENCHW_NO;

    if (pRateCtrl->hrd != 0)
    {
      rc->hrd = ENCHW_YES;
      rc->picRc = ENCHW_YES;
    }
    else
      rc->hrd = ENCHW_NO;

    rc->vbr = pRateCtrl->vbr ? ENCHW_YES : ENCHW_NO;
    rc->qpHdr = pRateCtrl->qpHdr << QP_FRACTIONAL_BITS;
    rc->qpMinPB = pRateCtrl->qpMinPB << QP_FRACTIONAL_BITS;
    rc->qpMaxPB = pRateCtrl->qpMaxPB << QP_FRACTIONAL_BITS;
    rc->qpMinI = pRateCtrl->qpMinI << QP_FRACTIONAL_BITS;
    rc->qpMaxI = pRateCtrl->qpMaxI << QP_FRACTIONAL_BITS;
    prevBitrate = rc->virtualBuffer.bitRate;
    rc->virtualBuffer.bitRate = bps;
    bitrateWindow = rc->bitrateWindow;
    rc->bitrateWindow = pRateCtrl->bitrateWindow;
    rc->maxPicSizeI = MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100)* (100 + pRateCtrl->bitVarRangeI)),rc->i32MaxPicSize);
    rc->maxPicSizeP = MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100)* (100 + pRateCtrl->bitVarRangeP)),rc->i32MaxPicSize);
    rc->maxPicSizeB = MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100)* (100 + pRateCtrl->bitVarRangeB)),rc->i32MaxPicSize);

    rc->minPicSizeI = (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100/ (100 + pRateCtrl->bitVarRangeI));
    rc->minPicSizeP = (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100/ (100 + pRateCtrl->bitVarRangeP));
    rc->minPicSizeB = (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100/ (100 + pRateCtrl->bitVarRangeB));

    rc->tolMovingBitRate = pRateCtrl->tolMovingBitRate;
    rc->f_tolMovingBitRate = (float)rc->tolMovingBitRate;
    rc->monitorFrames = pRateCtrl->monitorFrames;
    rc->u32StaticSceneIbitPercent = pRateCtrl->u32StaticSceneIbitPercent;
    rc->tolCtbRcInter = pRateCtrl->tolCtbRcInter;
    rc->tolCtbRcIntra = pRateCtrl->tolCtbRcIntra;
    rc->ctbRateCtrl.qpStep = ((pRateCtrl->ctbRcRowQpStep << CTB_RC_QP_STEP_FIXP) + rc->ctbCols/2) / rc->ctbCols;
  }

  rc->intraQpDelta = pRateCtrl->intraQpDelta << QP_FRACTIONAL_BITS;
  rc->fixedIntraQp = pRateCtrl->fixedIntraQp << QP_FRACTIONAL_BITS;
  rc->frameQpDelta = 0 << QP_FRACTIONAL_BITS ;
  rc->smooth_psnr_in_gop = pRateCtrl->smoothPsnrInGOP;
  rc->longTermQpDelta = pRateCtrl->longTermQpDelta << QP_FRACTIONAL_BITS;

  /*CRF parameters: pb and ip offset is set to default value of x265.
    Todo: set it available to user*/
  rc->crf = pRateCtrl->crf;
  //rc->pbOffset = 6.0 * X265_LOG2(1.3f);
  //rc->ipOffset = 6.0 * X265_LOG2(1.4f);
  rc->pbOffset = 6.0 * 0.378512;
  rc->ipOffset = 6.0 * 0.485427;

  /* New parameters checked already so ignore return value.
  * Reset RC bit counters when changing bitrate. */
  (void) VCEncInitRc(rc, (vcenc_instance->encStatus == VCENCSTAT_INIT) ||
                    (rc->virtualBuffer.bitRate != prevBitrate)||
                    (bitrateWindow != rc->bitrateWindow));
  if(vcenc_instance->pass==2) {
    VCEncRet ret = VCEncSetRateCtrl(vcenc_instance->lookahead.priv_inst, pRateCtrl);
    if (ret != VCENC_OK) {
      APITRACE("VCEncSetRateCtrl: LookaheadSetRateCtrl failed");
      return ret;
    }
  }
  APITRACE("VCEncSetRateCtrl: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VCEncGetRateCtrl
    Description   : Return current rate control parameters

    Return type   : VCEncRet
    Argument      : inst - the instance in use
                    pRateCtrl - place where parameters are returned
------------------------------------------------------------------------------*/
VCEncRet VCEncGetRateCtrl(VCEncInst inst, VCEncRateCtrl *pRateCtrl)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  vcencRateControl_s *rc;

  APITRACE("VCEncGetRateCtrl#");

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pRateCtrl == NULL))
  {
    APITRACEERR("VCEncGetRateCtrl: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncGetRateCtrl: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  /* Get the values */
  rc = &vcenc_instance->rateControl;

  pRateCtrl->pictureRc = rc->picRc == ENCHW_NO ? 0 : 1;
  /* CTB RC */
  pRateCtrl->ctbRc = rc->ctbRc;
  pRateCtrl->pictureSkip = rc->picSkip == ENCHW_NO ? 0 : 1;
  pRateCtrl->qpHdr = rc->qpHdr >> QP_FRACTIONAL_BITS;
  pRateCtrl->qpMinPB = rc->qpMinPB >> QP_FRACTIONAL_BITS;
  pRateCtrl->qpMaxPB = rc->qpMaxPB >> QP_FRACTIONAL_BITS;
  pRateCtrl->qpMinI = rc->qpMinI >> QP_FRACTIONAL_BITS;
  pRateCtrl->qpMaxI = rc->qpMaxI >> QP_FRACTIONAL_BITS;
  pRateCtrl->bitPerSecond = rc->virtualBuffer.bitRate;
  if (rc->virtualBuffer.bitPerPic)
  {
    pRateCtrl->bitVarRangeI = (i32)(((i64)rc->maxPicSizeI) * 100/ rc->virtualBuffer.bitPerPic - 100);
    pRateCtrl->bitVarRangeP = (i32)(((i64)rc->maxPicSizeP) * 100/ rc->virtualBuffer.bitPerPic - 100);
    pRateCtrl->bitVarRangeB = (i32)(((i64)rc->maxPicSizeB) * 100/ rc->virtualBuffer.bitPerPic - 100);
  }
  else
  {
    pRateCtrl->bitVarRangeI = 10000;
    pRateCtrl->bitVarRangeP = 10000;
    pRateCtrl->bitVarRangeB = 10000;
  }
  pRateCtrl->hrd = rc->hrd == ENCHW_NO ? 0 : 1;
  pRateCtrl->bitrateWindow = rc->bitrateWindow;

  pRateCtrl->hrdCpbSize = (u32) rc->virtualBuffer.bufferSize;
  pRateCtrl->intraQpDelta = rc->intraQpDelta >> QP_FRACTIONAL_BITS;
  pRateCtrl->fixedIntraQp = rc->fixedIntraQp >> QP_FRACTIONAL_BITS;
  pRateCtrl->monitorFrames = rc->monitorFrames;
  pRateCtrl->u32StaticSceneIbitPercent = rc->u32StaticSceneIbitPercent;
  pRateCtrl->tolMovingBitRate = rc->tolMovingBitRate;
  pRateCtrl->tolCtbRcInter = rc->tolCtbRcInter;
  pRateCtrl->tolCtbRcIntra = rc->tolCtbRcIntra;
  pRateCtrl->ctbRcRowQpStep = rc->ctbRateCtrl.qpStep ?
    ((rc->ctbRateCtrl.qpStep*rc->ctbCols + (1<<(CTB_RC_QP_STEP_FIXP-1))) >> CTB_RC_QP_STEP_FIXP) : 0;
  pRateCtrl->longTermQpDelta = rc->longTermQpDelta >> QP_FRACTIONAL_BITS;

  pRateCtrl->blockRCSize = vcenc_instance->blockRCSize ;
  pRateCtrl->rcQpDeltaRange = vcenc_instance->rateControl.rcQpDeltaRange;
  pRateCtrl->rcBaseMBComplexity= vcenc_instance->rateControl.rcBaseMBComplexity;
  pRateCtrl->picQpDeltaMin = vcenc_instance->rateControl.picQpDeltaMin;
  pRateCtrl->picQpDeltaMax = vcenc_instance->rateControl.picQpDeltaMax;
  pRateCtrl->vbr = rc->vbr == ENCHW_NO ? 0 : 1;
  pRateCtrl->ctbRcQpDeltaReverse = rc->ctbRcQpDeltaReverse;

  APITRACE("VCEncGetRateCtrl: OK");
  return VCENC_OK;
}


/*------------------------------------------------------------------------------
    Function name   : VSCheckSize
    Description     :
    Return type     : i32
    Argument        : u32 inputWidth
    Argument        : u32 inputHeight
    Argument        : u32 stabilizedWidth
    Argument        : u32 stabilizedHeight
------------------------------------------------------------------------------*/
i32 VSCheckSize(u32 inputWidth, u32 inputHeight, u32 stabilizedWidth,
                      u32 stabilizedHeight)
{
    /* Input picture minimum dimensions */
    if((inputWidth < 104) || (inputHeight < 104))
        return 1;

    /* Stabilized picture minimum  values */
    if((stabilizedWidth < 96) || (stabilizedHeight < 96))
        return 1;

    /* Stabilized dimensions multiple of 4 */
    if(((stabilizedWidth & 3) != 0) || ((stabilizedHeight & 3) != 0))
        return 1;

    /* Edge >= 4 pixels, not checked because stabilization can be
     * used without cropping for scene change detection
    if((inputWidth < (stabilizedWidth + 8)) ||
       (inputHeight < (stabilizedHeight + 8)))
        return 1; */

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : osd_overlap
    Description     : check osd input overlap
    Return type     : VCEncRet
    Argument        : pPreProcCfg - user provided parameters
    Argument        : i - osd channel to check
------------------------------------------------------------------------------*/
VCEncRet osd_overlap(const VCEncPreProcessingCfg *pPreProcCfg, u8 id, VCEncVideoCodecFormat format)
{
  int i,tmpx, tmpy;
  int blockW = 64;
  int blockH = (format == VCENC_VIDEO_CODEC_H264)?16:64;
  VCEncOverlayArea area0 = pPreProcCfg->overlayArea[id];
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    if(!pPreProcCfg->overlayArea[i].enable || i == id) continue;

    VCEncOverlayArea area1 = pPreProcCfg->overlayArea[i];

    if(!((area0.xoffset+area0.cropWidth) <= area1.xoffset || (area0.yoffset + area0.cropHeight) <= area1.yoffset ||
       area0.xoffset >= (area1.xoffset + area1.cropWidth) || area0.yoffset >= (area1.yoffset + area1.cropHeight)))
    {
      return VCENC_ERROR;
    }

    /* Check not share CTB: avoid loop all ctb */
    if((area0.xoffset+area0.cropWidth) <= area1.xoffset && (area0.yoffset+area0.cropHeight) <= area1.yoffset)
    {
      tmpx = ((area0.xoffset+area0.cropWidth -1)/blockW) * blockW;
      tmpy = ((area0.yoffset+area0.cropHeight -1)/blockH) * blockH;

      if(tmpx + blockW  > area1.xoffset && tmpy + blockH  > area1.yoffset)
      {
        return VCENC_ERROR;
      }
    }
    else if((area0.xoffset+area0.cropWidth) <= area1.xoffset && area0.yoffset >= (area1.yoffset + area1.cropHeight))
    {
      tmpx = ((area0.xoffset+area0.cropWidth -1)/blockW) * blockW;
      tmpy = ((area1.yoffset + area1.cropHeight - 1)/blockH) * blockH;
      if(tmpx + blockW  > area1.xoffset && tmpy + blockH > area0.yoffset)
      {
        return VCENC_ERROR;
      }
    }
    else if(area0.xoffset >= (area1.xoffset + area1.cropWidth) && (area0.yoffset+area0.cropHeight) <= area1.yoffset)
    {
      tmpx = ((area1.xoffset+area1.cropWidth -1)/blockW) * blockW;
      tmpy = ((area0.yoffset + area0.cropHeight -1)/blockH) * blockH;
      if(tmpx + blockW > area0.xoffset && tmpy + blockH > area1.yoffset)
      {
        return VCENC_ERROR;
      }
    }
    else if(area0.xoffset >= (area1.xoffset + area1.cropWidth) && area0.yoffset >= (area1.yoffset + area1.cropHeight))
    {
      tmpx = ((area1.xoffset+area1.cropWidth - 1)/blockW) * blockW;
      tmpy = ((area1.yoffset + area1.cropHeight - 1)/blockH) * blockH;
      if(tmpx + blockW > area0.xoffset && tmpy + blockH > area0.yoffset)
      {
        return VCENC_ERROR;
      }
    }
    else if((area0.xoffset+area0.cropWidth) <= area1.xoffset)
    {
      tmpx = ((area0.xoffset+area0.cropWidth - 1)/blockW) * blockW;
      if(tmpx + blockW > area1.xoffset)
        return VCENC_ERROR;
    }
    else if((area0.yoffset+area0.cropHeight) <= area1.yoffset)
    {
      tmpy = ((area0.yoffset+area0.cropHeight - 1)/blockH) * blockH;
      if(tmpy + blockH > area1.yoffset)
        return VCENC_ERROR;
    }
    else if(area0.xoffset >= (area1.xoffset + area1.cropWidth))
    {
      tmpx = ((area1.xoffset+area1.cropWidth - 1)/blockW) * blockW;
      if(tmpx + blockW > area0.xoffset)
        return VCENC_ERROR;
    }
    else if(area0.yoffset >= (area1.yoffset + area1.cropHeight))
    {
      tmpy = ((area1.yoffset+area1.cropHeight - 1)/blockH) * blockH;
      if(tmpy + blockH > area0.yoffset)
        return VCENC_ERROR;
    }
  }

  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncSetPreProcessing
    Description     : Sets the preprocessing parameters
    Return type     : VCEncRet
    Argument        : inst - encoder instance in use
    Argument        : pPreProcCfg - user provided parameters
------------------------------------------------------------------------------*/
VCEncRet VCEncSetPreProcessing(VCEncInst inst, const VCEncPreProcessingCfg *pPreProcCfg)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s pp_tmp;
  asicData_s *asic = &vcenc_instance->asic;
  u32 client_type;

  APITRACE("VCEncSetPreProcessing#");
  APITRACEPARAM("origWidth", pPreProcCfg->origWidth);
  APITRACEPARAM("origHeight", pPreProcCfg->origHeight);
  APITRACEPARAM("xOffset", pPreProcCfg->xOffset);
  APITRACEPARAM("yOffset", pPreProcCfg->yOffset);
  APITRACEPARAM("inputType", pPreProcCfg->inputType);
  APITRACEPARAM("rotation", pPreProcCfg->rotation);
  APITRACEPARAM("mirror", pPreProcCfg->mirror);
  APITRACEPARAM("colorConversion.type", pPreProcCfg->colorConversion.type);
  APITRACEPARAM("colorConversion.coeffA", pPreProcCfg->colorConversion.coeffA);
  APITRACEPARAM("colorConversion.coeffB", pPreProcCfg->colorConversion.coeffB);
  APITRACEPARAM("colorConversion.coeffC", pPreProcCfg->colorConversion.coeffC);
  APITRACEPARAM("colorConversion.coeffE", pPreProcCfg->colorConversion.coeffE);
  APITRACEPARAM("colorConversion.coeffF", pPreProcCfg->colorConversion.coeffF);
  APITRACEPARAM("scaledWidth", pPreProcCfg->scaledWidth);
  APITRACEPARAM("scaledHeight", pPreProcCfg->scaledHeight);
  APITRACEPARAM("scaledOutput", pPreProcCfg->scaledOutput);
  APITRACEPARAM_X("virtualAddressScaledBuff", pPreProcCfg->virtualAddressScaledBuff);
  APITRACEPARAM("busAddressScaledBuff", pPreProcCfg->busAddressScaledBuff);
  APITRACEPARAM("sizeScaledBuff", pPreProcCfg->sizeScaledBuff);
  APITRACEPARAM("constChromaEn", pPreProcCfg->constChromaEn);
  APITRACEPARAM("constCb", pPreProcCfg->constCb);
  APITRACEPARAM("constCr", pPreProcCfg->constCr);

  /* Check for illegal inputs */
  if ((inst == NULL) || (pPreProcCfg == NULL))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  /* check HW limitations */
  u32 i = 0;
  i32 core_id = -1;

  for (i=0; i<EWLGetCoreNum(); i++)
  {
    client_type = IS_H264(vcenc_instance->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
    EWLHwConfig_t cfg = EncAsicGetAsicConfig(i);

    if (client_type == EWL_CLIENT_TYPE_H264_ENC && cfg.h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if (client_type == EWL_CLIENT_TYPE_HEVC_ENC && cfg.hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if (cfg.rgbEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
        pPreProcCfg->inputType >= VCENC_RGB565 &&
        pPreProcCfg->inputType <= VCENC_BGR101010)
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR RGB input core not supported");
      continue;
    }
    if (cfg.scalingEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
        pPreProcCfg->scaledOutput != 0)
    {
      APITRACEERR("VCEncSetPreProcessing: WARNING Scaling core not supported, disabling output");
      continue;
    }
    if (cfg.cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
        (pPreProcCfg->colorConversion.type >= VCENC_RGBTOYUV_BT601_FULL_RANGE) &&
        (pPreProcCfg->colorConversion.type <=VCENC_RGBTOYUV_BT709_FULL_RANGE))
    {
      APITRACEERR("VCEncSetPreProcessing: WARNING color conversion extend not supported, set to BT601_l.mat");
      continue;
    }

    if ((pPreProcCfg->colorConversion.type == VCENC_RGBTOYUV_USER_DEFINED) &&
        (cfg.cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED) &&
        ((pPreProcCfg->colorConversion.coeffG != pPreProcCfg->colorConversion.coeffE) ||
         (pPreProcCfg->colorConversion.coeffH != pPreProcCfg->colorConversion.coeffF) ||
         (pPreProcCfg->colorConversion.LumaOffset != 0)))
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR color conversion extend not supported, invalid coefficient");
      continue;
    }

    if (cfg.scaled420Support == EWL_HW_CONFIG_NOT_SUPPORTED && pPreProcCfg->scaledOutput != 0 &&
        pPreProcCfg->scaledOutputFormat == 1)
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR Scaling to 420SP core not supported");
      continue;
    }

    if (cfg.inLoopDSRatio == EWL_HW_CONFIG_ENABLED && vcenc_instance->pass == 1 &&
        pPreProcCfg->inputType >= VCENC_YUV420_10BIT_PACKED_Y0L2 &&
        pPreProcCfg->inputType <= VCENC_YUV420_VU_10BIT_TILE_96_2)
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR in-loop down-scaler not supported by this format");
      continue;
    }

    if (cfg.scaled420Support == EWL_HW_CONFIG_ENABLED && pPreProcCfg->scaledOutput != 0 &&
        (!((((double)vcenc_instance->width / pPreProcCfg->scaledWidth) >= 1) && (((double)vcenc_instance->width / pPreProcCfg->scaledWidth) <= 32) &&
         (((double)vcenc_instance->height / pPreProcCfg->scaledHeight) >= 1) && (((double)vcenc_instance->height / pPreProcCfg->scaledHeight) <= 32)))
        )
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR Scaling ratio not supported");
      continue;
    }

    core_id = i;
    if (pPreProcCfg->inputType >= VCENC_RGB565 && pPreProcCfg->inputType <= VCENC_BGR101010)
      vcenc_instance->featureToSupport.rgbEnabled = 1;
    else
      vcenc_instance->featureToSupport.rgbEnabled = 0;

    if (pPreProcCfg->scaledOutput != 0)
      vcenc_instance->featureToSupport.scalingEnabled = 1;
    else
      vcenc_instance->featureToSupport.scalingEnabled = 0;
    break;
  }

  if(core_id < 0)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR RGB input or Scaling or videoRange Color convert not supported");
    return VCENC_INVALID_ARGUMENT;
  }

  if (IS_AV1(vcenc_instance->codecFormat) && ((vcenc_instance->preProcess.lumWidth & 7) || (vcenc_instance->preProcess.lumHeight & 7)) && pPreProcCfg->rotation != VCENC_ROTATE_0)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR rotation support only 8-pixel aligned width/height in AV1");
    return ENCHW_NOK;
  }

  if (pPreProcCfg->origWidth > VCENC_MAX_PP_INPUT_WIDTH)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Input image width is too big");
    return VCENC_INVALID_ARGUMENT;
  }
#if 1
  /* Scaled image width limits, multiple of 4 (YUYV) and smaller than input */
  if ((pPreProcCfg->scaledWidth > (u32)vcenc_instance->width) ||
      ((pPreProcCfg->scaledWidth & 0x1) != 0) ||
      (pPreProcCfg->scaledWidth > DOWN_SCALING_MAX_WIDTH))
  {
    APITRACEERR("VCEncSetPreProcessing: Invalid scaledWidth");
    return ENCHW_NOK;
  }

  if (((i32)pPreProcCfg->scaledHeight > vcenc_instance->height) ||
      (pPreProcCfg->scaledHeight & 0x1) != 0)
  {
    APITRACEERR("VCEncSetPreProcessing: Invalid scaledHeight");
    return ENCHW_NOK;
  }
#endif
  if (pPreProcCfg->inputType >= VCENC_FORMAT_MAX)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid YUV type");
    return VCENC_INVALID_ARGUMENT;
  }
  if ((pPreProcCfg->inputType == VCENC_YUV420_PLANAR_10BIT_I010
  ||pPreProcCfg->inputType == VCENC_YUV420_PLANAR_10BIT_P010
  ||pPreProcCfg->inputType == VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR
  ||pPreProcCfg->inputType == VCENC_YUV420_10BIT_PACKED_Y0L2
  ||pPreProcCfg->inputType == VCENC_YUV420_PLANAR_10BIT_P010_FB
  ||pPreProcCfg->inputType == VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC
  ||pPreProcCfg->inputType == VCENC_YUV420_PLANAR_8BIT_DAHUA_H264)&&(HW_ID_MAJOR_NUMBER(asic->regs.asicHwId)<4))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid YUV type, HW not support I010 format.");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pPreProcCfg->rotation > VCENC_ROTATE_180R)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid rotation");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pPreProcCfg->mirror > VCENC_MIRROR_YES)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid mirror");
    return VCENC_INVALID_ARGUMENT;
  }

  if ( (vcenc_instance->inputLineBuf.inputLineBufEn) && ((pPreProcCfg->rotation!=0) || (pPreProcCfg->mirror!=0)
                                                     || (pPreProcCfg->videoStabilization!=0)) )
  {
    APITRACE("VCEncSetPreProcessing: ERROR Cannot do rotation, stabilization, and mirror in low latency mode");
    return VCENC_INVALID_ARGUMENT;
  }

#if 0
  if (((pPreProcCfg->xOffset & 1) != 0) || ((pPreProcCfg->yOffset & 1) != 0))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset");
    return VCENC_INVALID_ARGUMENT;
  }
#endif

  if (pPreProcCfg->inputType == VCENC_YUV420_SEMIPLANAR_101010 && (pPreProcCfg->xOffset % 6) != 0)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType == VCENC_YUV420_8BIT_TILE_64_4 || pPreProcCfg->inputType == VCENC_YUV420_UV_8BIT_TILE_64_4)&& (((pPreProcCfg->xOffset % 64) != 0) || ((pPreProcCfg->yOffset % 4) != 0)))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pPreProcCfg->inputType == VCENC_YUV420_10BIT_TILE_32_4 && (((pPreProcCfg->xOffset % 32) != 0) || ((pPreProcCfg->yOffset % 4) != 0)))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType == VCENC_YUV420_10BIT_TILE_48_4 || pPreProcCfg->inputType == VCENC_YUV420_VU_10BIT_TILE_48_4)&& (((pPreProcCfg->xOffset % 48) != 0) || ((pPreProcCfg->yOffset % 4) != 0)))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType == VCENC_YUV420_8BIT_TILE_128_2 || pPreProcCfg->inputType == VCENC_YUV420_UV_8BIT_TILE_128_2)&& ((pPreProcCfg->xOffset % 128) != 0))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType == VCENC_YUV420_10BIT_TILE_96_2 || pPreProcCfg->inputType == VCENC_YUV420_VU_10BIT_TILE_96_2)&& ((pPreProcCfg->xOffset % 96) != 0))
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid offset for this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType >= VCENC_YUV420_SEMIPLANAR_8BIT_FB &&
       pPreProcCfg->inputType <= VCENC_YUV420_VU_10BIT_TILE_96_2)
      && pPreProcCfg->rotation!=0)
  {
    APITRACEERR("VCEncSetPreProcessing: rotation not supported by this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((pPreProcCfg->inputType > VCENC_BGR101010)
      && pPreProcCfg->videoStabilization!=0)
  {
    APITRACEERR("VCEncSetPreProcessing: stabilization not supported by this format");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pPreProcCfg->constChromaEn)
  {
    u32 maxCh = (1 << (8 + vcenc_instance->sps->bit_depth_chroma_minus8)) - 1;
    if ((pPreProcCfg->constCb > maxCh) || (pPreProcCfg->constCr > maxCh))
    {
      APITRACEERR("VCEncSetPreProcessing: ERROR Invalid constant chroma pixel value");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if ((IS_HEVC(vcenc_instance->codecFormat)) &&((vcenc_instance->Hdr10Color.hdr10_color_enable == ENCHW_YES) || (vcenc_instance->Hdr10Display.hdr10_display_enable == ENCHW_YES) || (vcenc_instance->Hdr10LightLevel.hdr10_lightlevel_enable == ENCHW_YES)))
  {
	  if ((pPreProcCfg->inputType >= VCENC_RGB565) && (pPreProcCfg->inputType <= VCENC_BGR101010))
	  {
		  if (pPreProcCfg->colorConversion.type != VCENC_RGBTOYUV_BT2020)
		  {
			  APITRACEERR("VCEncSetPreProcessing: Invalid color conversion in HDR10\n");
			  return ENCHW_NOK;
		  }
	  }

	  if ((vcenc_instance->sps->bit_depth_luma_minus8 != 2) || (vcenc_instance->sps->bit_depth_chroma_minus8 != 2))
	  {
		  APITRACEERR("VCEncSetPreProcessing: Invalid bit depth for main10 profile in HDR10\n");
		  return ENCHW_NOK;
	  }
  }

  /*Overlay Area check*/
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    if(pPreProcCfg->overlayArea[i].enable && vcenc_instance->pass != 1)
    {
      if(
       pPreProcCfg->overlayArea[i].xoffset + pPreProcCfg->overlayArea[i].cropWidth > vcenc_instance->width - pPreProcCfg->xOffset ||
       pPreProcCfg->overlayArea[i].yoffset + pPreProcCfg->overlayArea[i].cropHeight > vcenc_instance->height - pPreProcCfg->yOffset)
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid overlay area\n");
        return VCENC_INVALID_ARGUMENT;
      }

      if(pPreProcCfg->overlayArea[i].cropWidth == 0 || pPreProcCfg->overlayArea[i].cropHeight == 0)
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid overlay size\n");
        return VCENC_INVALID_ARGUMENT;
      }

      if(pPreProcCfg->overlayArea[i].cropXoffset + pPreProcCfg->overlayArea[i].cropWidth > pPreProcCfg->overlayArea[i].width ||
        pPreProcCfg->overlayArea[i].cropYoffset + pPreProcCfg->overlayArea[i].cropHeight > pPreProcCfg->overlayArea[i].height)
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid overlay cropping size\n");
        return VCENC_INVALID_ARGUMENT;
      }

      if(pPreProcCfg->overlayArea[i].format == 2 && (pPreProcCfg->overlayArea[i].cropWidth % 8 != 0))
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid overlay cropping width for bitmap \n");
        return VCENC_INVALID_ARGUMENT;
      }

      if(osd_overlap(pPreProcCfg, i, vcenc_instance->codecFormat))
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR overlay area overlapping \n");
        return VCENC_INVALID_ARGUMENT;
      }
    }
  }

  pp_tmp = vcenc_instance->preProcess;

  if (pPreProcCfg->videoStabilization == 0)
  {
    pp_tmp.horOffsetSrc = pPreProcCfg->xOffset;
    pp_tmp.verOffsetSrc = pPreProcCfg->yOffset;
  }
  else
  {
    pp_tmp.horOffsetSrc = 0;
    pp_tmp.verOffsetSrc = 0;
  }

  pp_tmp.subsample_x = (vcenc_instance->asic.regs.codedChromaIdc == VCENC_CHROMA_IDC_420) ? 1 : 0;
  pp_tmp.subsample_y = (vcenc_instance->asic.regs.codedChromaIdc == VCENC_CHROMA_IDC_420) ? 1 : 0;

  /* Encoded frame resolution as set in Init() */
  //pp_tmp = pEncInst->preProcess;    //here is a bug? if enabling this line.
  EWLHwConfig_t cfg = EncAsicGetAsicConfig(0);

  pp_tmp.lumWidthSrc  = pPreProcCfg->origWidth;
  pp_tmp.lumHeightSrc = pPreProcCfg->origHeight;
  pp_tmp.rotation     = pPreProcCfg->rotation;
  pp_tmp.mirror       = pPreProcCfg->mirror;
  pp_tmp.inputFormat  = pPreProcCfg->inputType;
  pp_tmp.videoStab    = (pPreProcCfg->videoStabilization != 0) ? 1 : 0;
  pp_tmp.scaledWidth  = pPreProcCfg->scaledWidth;
  pp_tmp.scaledHeight = pPreProcCfg->scaledHeight;
  asic->scaledImage.busAddress = pPreProcCfg->busAddressScaledBuff;
  asic->scaledImage.virtualAddress = pPreProcCfg->virtualAddressScaledBuff;
  asic->scaledImage.size = pPreProcCfg->sizeScaledBuff;
  asic->regs.scaledLumBase = asic->scaledImage.busAddress;
  pp_tmp.input_alignment = pPreProcCfg->input_alignment;
  /* external DS takes precedence over inLoopDS, allowing user to switch if both are supported */
  pp_tmp.inLoopDSRatio = ((vcenc_instance->pass == 1 && !vcenc_instance->extDSRatio)? cfg.inLoopDSRatio : 0);

  pp_tmp.scaledOutputFormat = pPreProcCfg->scaledOutputFormat;
  pp_tmp.scaledOutput = (pPreProcCfg->scaledOutput) ? 1 : 0;
  if (pPreProcCfg->scaledWidth * pPreProcCfg->scaledHeight == 0 || vcenc_instance->pass == 1)
    pp_tmp.scaledOutput = 0;
  /* Check for invalid values */
  if (EncPreProcessCheck(&pp_tmp) != ENCHW_OK)
  {
    APITRACEERR("VCEncSetPreProcessing: ERROR Invalid cropping values");
    return VCENC_INVALID_ARGUMENT;
  }
#if 1
  pp_tmp.frameCropping = ENCHW_NO;
  pp_tmp.frameCropLeftOffset = 0;
  pp_tmp.frameCropRightOffset = 0;
  pp_tmp.frameCropTopOffset = 0;
  pp_tmp.frameCropBottomOffset = 0;
  /* Set cropping parameters if required */
  u32 alignment = (IS_H264(vcenc_instance->codecFormat) ? 16 : 8);
  if (vcenc_instance->preProcess.lumWidth % alignment || vcenc_instance->preProcess.lumHeight % alignment)
  {
    u32 fillRight = (vcenc_instance->preProcess.lumWidth + alignment-1) / alignment * alignment -
                    vcenc_instance->preProcess.lumWidth;
    u32 fillBottom = (vcenc_instance->preProcess.lumHeight + alignment-1) / alignment * alignment -
                     vcenc_instance->preProcess.lumHeight;


    pp_tmp.frameCropping = ENCHW_YES;

    if (pPreProcCfg->rotation == VCENC_ROTATE_0)     /* No rotation */
    {
      if (pp_tmp.mirror)
        pp_tmp.frameCropLeftOffset = fillRight >> pp_tmp.subsample_x;
      else
        pp_tmp.frameCropRightOffset = fillRight >> pp_tmp.subsample_x;

      pp_tmp.frameCropBottomOffset = fillBottom >> pp_tmp.subsample_y;
    }
    else if (pPreProcCfg->rotation == VCENC_ROTATE_90R)        /* Rotate right */
    {

      pp_tmp.frameCropLeftOffset = fillRight >> pp_tmp.subsample_x;

      if (pp_tmp.mirror)
        pp_tmp.frameCropTopOffset = fillBottom >> pp_tmp.subsample_y;
      else
        pp_tmp.frameCropBottomOffset = fillBottom >> pp_tmp.subsample_y;
    }
    else if (pPreProcCfg->rotation == VCENC_ROTATE_90L)        /* Rotate left */
    {
      pp_tmp.frameCropRightOffset = fillRight >> pp_tmp.subsample_x;

      if (pp_tmp.mirror)
        pp_tmp.frameCropBottomOffset = fillBottom >> pp_tmp.subsample_y;
      else
        pp_tmp.frameCropTopOffset = fillBottom >> pp_tmp.subsample_y;
    }
    else if (pPreProcCfg->rotation == VCENC_ROTATE_180R)        /* Rotate 180 degree left */
    {
      if (pp_tmp.mirror)
        pp_tmp.frameCropRightOffset = fillRight >> pp_tmp.subsample_x;
      else
        pp_tmp.frameCropLeftOffset = fillRight >> pp_tmp.subsample_x;

      pp_tmp.frameCropTopOffset = fillBottom >> pp_tmp.subsample_y;
    }
  }
#endif

  if (pp_tmp.videoStab != 0)
  {
    u32 width = pp_tmp.lumWidth;
    u32 height = pp_tmp.lumHeight;
    u32 heightSrc = pp_tmp.lumHeightSrc;

    if (pp_tmp.rotation)
    {
      u32 tmp;

      tmp = width;
      width = height;
      height = tmp;
    }

    if (VSCheckSize(pp_tmp.lumWidthSrc, heightSrc, width, height) != 0)
    {
      APITRACE("VCEncSetPreProcessing: ERROR Invalid size for stabilization");
      return VCENC_INVALID_ARGUMENT;
    }

#ifdef VIDEOSTAB_ENABLED
    VSAlgInit(&vcenc_instance->vsSwData, pp_tmp.lumWidthSrc, heightSrc, width, height);

    VSAlgGetResult(&vcenc_instance->vsSwData, &pp_tmp.horOffsetSrc, &pp_tmp.verOffsetSrc);
#endif
  }

  pp_tmp.colorConversionType = pPreProcCfg->colorConversion.type;
  pp_tmp.colorConversionCoeffA = pPreProcCfg->colorConversion.coeffA;
  pp_tmp.colorConversionCoeffB = pPreProcCfg->colorConversion.coeffB;
  pp_tmp.colorConversionCoeffC = pPreProcCfg->colorConversion.coeffC;
  pp_tmp.colorConversionCoeffE = pPreProcCfg->colorConversion.coeffE;
  pp_tmp.colorConversionCoeffF = pPreProcCfg->colorConversion.coeffF;
  pp_tmp.colorConversionCoeffG = pPreProcCfg->colorConversion.coeffG;
  pp_tmp.colorConversionCoeffH = pPreProcCfg->colorConversion.coeffH;
  pp_tmp.colorConversionLumaOffset = pPreProcCfg->colorConversion.LumaOffset;
  EncSetColorConversion(&pp_tmp, &vcenc_instance->asic);

  /* constant chroma control */
  pp_tmp.constChromaEn = pPreProcCfg->constChromaEn;
  pp_tmp.constCb = pPreProcCfg->constCb;
  pp_tmp.constCr = pPreProcCfg->constCr;

  /* overlay control*/
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    pp_tmp.overlayEnable[i] = pPreProcCfg->overlayArea[i].enable;
    pp_tmp.overlayAlpha[i] = pPreProcCfg->overlayArea[i].alpha;
    pp_tmp.overlayFormat[i] = pPreProcCfg->overlayArea[i].format;
    pp_tmp.overlayHeight[i] = pPreProcCfg->overlayArea[i].height;
    pp_tmp.overlayCropHeight[i] = pPreProcCfg->overlayArea[i].cropHeight;
    pp_tmp.overlayWidth[i] = pPreProcCfg->overlayArea[i].width;
    pp_tmp.overlayCropWidth[i] = pPreProcCfg->overlayArea[i].cropWidth;
    pp_tmp.overlayXoffset[i] = pPreProcCfg->overlayArea[i].xoffset;
    pp_tmp.overlayCropXoffset[i] = pPreProcCfg->overlayArea[i].cropXoffset;
    pp_tmp.overlayYoffset[i] = pPreProcCfg->overlayArea[i].yoffset;
    pp_tmp.overlayCropYoffset[i] = pPreProcCfg->overlayArea[i].cropYoffset;
    pp_tmp.overlayYStride[i] = pPreProcCfg->overlayArea[i].Ystride;
    pp_tmp.overlayUVStride[i] = pPreProcCfg->overlayArea[i].UVstride;
    pp_tmp.overlayBitmapY[i] = pPreProcCfg->overlayArea[i].bitmapY;
    pp_tmp.overlayBitmapU[i] = pPreProcCfg->overlayArea[i].bitmapU;
    pp_tmp.overlayBitmapV[i] = pPreProcCfg->overlayArea[i].bitmapV;
    //HEVC handles PRP per frame, no need to do slice
    pp_tmp.overlaySliceHeight[i] = pPreProcCfg->overlayArea[i].cropHeight;
    pp_tmp.overlayVerOffset[i] = pPreProcCfg->overlayArea[i].cropYoffset;
  }


  vcenc_instance->preProcess = pp_tmp;
  if(vcenc_instance->pass==2) {
    VCEncPreProcessingCfg new_cfg;
    u32 ds_ratio = 1;

    memcpy(&new_cfg, pPreProcCfg, sizeof(VCEncPreProcessingCfg));

    /*pass-1 cutree downscaler*/
    if(vcenc_instance->extDSRatio) {
      ds_ratio = vcenc_instance->extDSRatio + 1;
      new_cfg.origWidth  /= ds_ratio;
      new_cfg.origHeight /= ds_ratio;
      new_cfg.origWidth  = (new_cfg.origWidth  >> 1) << 1;
      new_cfg.origHeight = (new_cfg.origHeight >> 1) << 1;
      new_cfg.xOffset /= ds_ratio;
      new_cfg.yOffset /= ds_ratio;
    }

    VCEncRet ret = VCEncSetPreProcessing(vcenc_instance->lookahead.priv_inst, &new_cfg);
    if (ret != VCENC_OK) {
      APITRACE("VCEncSetPreProcessing: LookaheadSetPreProcessing failed");
      return ret;
    }
  }
  APITRACE("VCEncSetPreProcessing: OK");

  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncGetPreProcessing
    Description     : Returns current preprocessing parameters
    Return type     : VCEncRet
    Argument        : inst - encoder instance
    Argument        : pPreProcCfg - place where the parameters are returned
------------------------------------------------------------------------------*/
VCEncRet VCEncGetPreProcessing(VCEncInst inst,
                                   VCEncPreProcessingCfg *pPreProcCfg)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s *pPP;
  asicData_s *asic = &vcenc_instance->asic;
  APITRACE("VCEncGetPreProcessing#");
  /* Check for illegal inputs */
  if ((inst == NULL) || (pPreProcCfg == NULL))
  {
    APITRACEERR("VCEncGetPreProcessing: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncGetPreProcessing: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  pPP = &vcenc_instance->preProcess;

  pPreProcCfg->origHeight = pPP->lumHeightSrc;
  pPreProcCfg->origWidth = pPP->lumWidthSrc;
  pPreProcCfg->xOffset = pPP->horOffsetSrc;
  pPreProcCfg->yOffset = pPP->verOffsetSrc;

  pPreProcCfg->rotation = (VCEncPictureRotation) pPP->rotation;
  pPreProcCfg->mirror = (VCEncPictureMirror) pPP->mirror;
  pPreProcCfg->inputType = (VCEncPictureType) pPP->inputFormat;

  pPreProcCfg->busAddressScaledBuff = asic->scaledImage.busAddress;
  pPreProcCfg->virtualAddressScaledBuff = asic->scaledImage.virtualAddress;
  pPreProcCfg->sizeScaledBuff = asic->scaledImage.size;

  pPreProcCfg->scaledOutput       = pPP->scaledOutput;
  pPreProcCfg->scaledWidth       = pPP->scaledWidth;
  pPreProcCfg->scaledHeight       = pPP->scaledHeight;
  pPreProcCfg->input_alignment = pPP->input_alignment;
  pPreProcCfg->inLoopDSRatio = pPP->inLoopDSRatio;
  pPreProcCfg->videoStabilization = pPP->videoStab;
  pPreProcCfg->scaledOutputFormat = pPP->scaledOutputFormat;

  pPreProcCfg->colorConversion.type =
    (VCEncColorConversionType) pPP->colorConversionType;
  pPreProcCfg->colorConversion.coeffA = pPP->colorConversionCoeffA;
  pPreProcCfg->colorConversion.coeffB = pPP->colorConversionCoeffB;
  pPreProcCfg->colorConversion.coeffC = pPP->colorConversionCoeffC;
  pPreProcCfg->colorConversion.coeffE = pPP->colorConversionCoeffE;
  pPreProcCfg->colorConversion.coeffF = pPP->colorConversionCoeffF;
  pPreProcCfg->colorConversion.coeffG = pPP->colorConversionCoeffG;
  pPreProcCfg->colorConversion.coeffH = pPP->colorConversionCoeffH;
  pPreProcCfg->colorConversion.LumaOffset = pPP->colorConversionLumaOffset;

  /* constant chroma control */
  pPreProcCfg->constChromaEn = pPP->constChromaEn;
  pPreProcCfg->constCb = pPP->constCb;
  pPreProcCfg->constCr = pPP->constCr;

  APITRACE("VCEncGetPreProcessing: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncCreateNewPPS
    Description     : create new PPS
    Return type     : VCEncRet
    Argument        : inst - encoder instance in use
    Argument        : pPPSCfg - user provided parameters
    Argument        : newPPSId - new PPS id for user
------------------------------------------------------------------------------*/
VCEncRet VCEncCreateNewPPS(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32* newPPSId)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s pp_tmp;
  asicData_s *asic = &vcenc_instance->asic;
  struct pps * p;
  struct pps * p0;
  i32 ppsId;

  struct container *c;


  APITRACE("VCEncCreateNewPPS#");
  APITRACEPARAM("chroma_qp_offset", pPPSCfg->chroma_qp_offset);
  APITRACEPARAM("tc_Offset", pPPSCfg->tc_Offset);
  APITRACEPARAM("beta_Offset", pPPSCfg->beta_Offset);

  /* Check for illegal inputs */
  if ((inst == NULL) || (pPPSCfg == NULL))
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }


  if (pPPSCfg->chroma_qp_offset > 12 ||
      pPPSCfg->chroma_qp_offset < -12)
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR chroma_qp_offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pPPSCfg->tc_Offset > 6 || pPPSCfg->tc_Offset < -6)
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR tc_Offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pPPSCfg->beta_Offset > 6 || pPPSCfg->beta_Offset < -6)
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR beta_Offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  c = get_container(vcenc_instance);

  p0 = (struct pps *)get_parameter_set(c, PPS_NUT, 0);
  /*get ppsId*/
  ppsId = 0;
  while(1)
  {
    p = (struct pps *)get_parameter_set(c, PPS_NUT, ppsId);
    if(p == NULL)
    {
      break;
    }
    ppsId++;
  }
  *newPPSId = ppsId;

  if(ppsId > 63)
  {
    APITRACEERR("VCEncCreateNewPPS: ERROR PPS id is greater than 63");
    return VCENC_INVALID_ARGUMENT;
  }
  p = (struct pps *)create_parameter_set(PPS_NUT);
  {
    struct ps pstemp = p->ps;
    *p = *p0;
    p->ps = pstemp;
  }
  p->cb_qp_offset = p->cr_qp_offset = pPPSCfg->chroma_qp_offset;

  p->tc_offset = pPPSCfg->tc_Offset*2;

  p->beta_offset = pPPSCfg->beta_Offset*2;

  p->ps.id = ppsId;

  queue_put(&c->parameter_set, (struct node *)p);

  vcenc_instance->insertNewPPS = 1;
  vcenc_instance->insertNewPPSId = ppsId;

  vcenc_instance->maxPPSId++;

  /*create new PPS with ppsId*/
  APITRACE("VCEncCreateNewPPS: OK");

  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncModifyOldPPS
    Description     : modify old PPS
    Return type     : VCEncRet
    Argument        : inst - encoder instance in use
    Argument        : pPPSCfg - user provided parameters
    Argument        : newPPSId - old PPS id provided by user
------------------------------------------------------------------------------*/
VCEncRet VCEncModifyOldPPS(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32 ppsId)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s pp_tmp;
  asicData_s *asic = &vcenc_instance->asic;
  struct pps * p;
  struct container *c;

  APITRACE("VCEncModifyOldPPS#");
  APITRACEPARAM("chroma_qp_offset", pPPSCfg->chroma_qp_offset);
  APITRACEPARAM("tc_Offset", pPPSCfg->tc_Offset);
  APITRACEPARAM("beta_Offset", pPPSCfg->beta_Offset);

  /* Check for illegal inputs */
  if ((inst == NULL) || (pPPSCfg == NULL))
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }


  if (pPPSCfg->chroma_qp_offset > 12 ||
      pPPSCfg->chroma_qp_offset < -12)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR chroma_qp_offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pPPSCfg->tc_Offset > 6 || pPPSCfg->tc_Offset < -6)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR tc_Offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }
  if (pPPSCfg->beta_Offset > 6 || pPPSCfg->beta_Offset < -6)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR beta_Offset out of range");
    return VCENC_INVALID_ARGUMENT;
  }

  if(vcenc_instance->maxPPSId < ppsId || ppsId < 0)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }

  c = get_container(vcenc_instance);

  p = (struct pps *)get_parameter_set(c, PPS_NUT, ppsId);

  if(p == NULL)
  {
    APITRACEERR("VCEncModifyOldPPS: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }


  p->cb_qp_offset = p->cr_qp_offset = pPPSCfg->chroma_qp_offset;

  p->tc_offset = pPPSCfg->tc_Offset*2;

  p->beta_offset = pPPSCfg->beta_Offset*2;


  vcenc_instance->insertNewPPS = 1;
  vcenc_instance->insertNewPPSId = ppsId;


  /*create new PPS with ppsId*/
  APITRACE("VCEncModifyOldPPS: OK");

  return VCENC_OK;
}


/*------------------------------------------------------------------------------
    Function name   : VCEncGetPPSData
    Description     : get PPS parameters for user
    Return type     : VCEncRet
    Argument        : inst - encoder instance
    Argument        : pPPSCfg - place where the parameters are returned
    Argument        : ppsId - PPS id provided by user
------------------------------------------------------------------------------*/
VCEncRet VCEncGetPPSData(VCEncInst inst,  VCEncPPSCfg *pPPSCfg, i32 ppsId)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s *pPP;
  struct pps * p;
  struct pps * p0;
  struct container *c;

  APITRACE("VCEncGetPPSData#");
  /* Check for illegal inputs */
  if ((inst == NULL) || (pPPSCfg == NULL))
  {
    APITRACEERR("VCEncGetPPSData: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncGetPPSData: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  if(vcenc_instance->maxPPSId < ppsId || ppsId < 0)
  {
    APITRACEERR("VCEncGetPPSData: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }

  c = get_container(vcenc_instance);

  p = (struct pps *)get_parameter_set(c, PPS_NUT, ppsId);

  if(p == NULL)
  {
    APITRACEERR("VCEncGetPPSData: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }

  pPPSCfg->chroma_qp_offset = p->cb_qp_offset;

  pPPSCfg->tc_Offset = p->tc_offset/2;

  pPPSCfg->beta_Offset = p->beta_offset/2;


  APITRACE("VCEncGetPPSData: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncActiveAnotherPPS
    Description     : active another PPS
    Return type     : VCEncRet
    Argument        : inst - encoder instance in use
    Argument        : ppsId - PPS id provided by user
------------------------------------------------------------------------------*/
VCEncRet VCEncActiveAnotherPPS(VCEncInst inst, i32 ppsId)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s pp_tmp;
  asicData_s *asic = &vcenc_instance->asic;
  struct pps * p;

  struct container *c;


  APITRACE("VCEncActiveAnotherPPS#");
  APITRACEPARAM("ppsId", ppsId);

  /* Check for illegal inputs */
  if ((inst == NULL))
  {
    APITRACEERR("VCEncActiveAnotherPPS: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncActiveAnotherPPS: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  if(vcenc_instance->maxPPSId < ppsId || ppsId < 0)
  {
    APITRACEERR("VCEncActiveAnotherPPS: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }

  c = get_container(vcenc_instance);

  p = (struct pps *)get_parameter_set(c, PPS_NUT, ppsId);

  if(p == NULL)
  {
    APITRACEERR("VCEncActiveAnotherPPS: ERROR Invalid ppsId");
    return VCENC_INVALID_ARGUMENT;
  }

  vcenc_instance->pps_id = ppsId;

  /*create new PPS with ppsId*/
  APITRACE("VCEncActiveAnotherPPS: OK");

  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VCEncGetActivePPSId
    Description     : get PPS parameters for user
    Return type     : VCEncRet
    Argument        : inst - encoder instance
    Argument        : pPPSCfg - place where the parameters are returned
    Argument        : ppsId - PPS id provided by user
------------------------------------------------------------------------------*/
VCEncRet VCEncGetActivePPSId(VCEncInst inst,  i32* ppsId)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  preProcess_s *pPP;
  struct pps * p;
  struct container *c;

  APITRACE("VCEncGetPPSData#");
  /* Check for illegal inputs */
  if ((inst == NULL) || (ppsId == NULL))
  {
    APITRACEERR("VCEncGetActivePPSId: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncGetActivePPSId: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  *ppsId = vcenc_instance->pps_id;

  APITRACE("VCEncGetActivePPSId: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncSetSeiUserData
    Description   : Sets user data SEI messages
    Return type   : VCEncRet
    Argument      : inst - the instance in use
                    pUserData - pointer to userData, this is used by the
                                encoder so it must not be released before
                                disabling user data
                    userDataSize - size of userData, minimum size 16,
                                   maximum size VCENC_MAX_USER_DATA_SIZE
                                   not valid size disables userData sei messages
------------------------------------------------------------------------------*/
VCEncRet VCEncSetSeiUserData(VCEncInst inst, const u8 *pUserData,
                                 u32 userDataSize)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

  APITRACE("VCEncSetSeiUserData#");
  APITRACEPARAM("userDataSize", userDataSize);

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (userDataSize != 0 && pUserData == NULL))
  {
    APITRACEERR("VCEncSetSeiUserData: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncSetSeiUserData: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  /* Disable user data */
  if ((userDataSize < 16) || (userDataSize > VCENC_MAX_USER_DATA_SIZE))
  {
    vcenc_instance->rateControl.sei.userDataEnabled = ENCHW_NO;
    vcenc_instance->rateControl.sei.pUserData = NULL;
    vcenc_instance->rateControl.sei.userDataSize = 0;
  }
  else
  {
    vcenc_instance->rateControl.sei.userDataEnabled = ENCHW_YES;
    vcenc_instance->rateControl.sei.pUserData = pUserData;
    vcenc_instance->rateControl.sei.userDataSize = userDataSize;
  }

  APITRACE("VCEncSetSeiUserData: OK");
  return VCENC_OK;

}

/*------------------------------------------------------------------------------
    Function name : VCEncAddNaluSize
    Description   : Adds the size of a NAL unit into NAL size output buffer.

    Return type   : void
    Argument      : pEncOut - encoder output structure
    Argument      : naluSizeBytes - size of the NALU in bytes
------------------------------------------------------------------------------*/
void VCEncAddNaluSize(VCEncOut *pEncOut, u32 naluSizeBytes)
{
  if (pEncOut->pNaluSizeBuf != NULL)
  {
    pEncOut->pNaluSizeBuf[pEncOut->numNalus++] = naluSizeBytes;
    pEncOut->pNaluSizeBuf[pEncOut->numNalus] = 0;
  }
}


/*------------------------------------------------------------------------------
  parameter_set

  RPS:
  poc[n*2  ] = delta_poc
  poc[n*2+1] = used_by_curr_pic
  poc[n*2  ] = 0, termination
  For example:
  { 0, 0},    No reference pictures
  {-1, 1},    Only one previous pictures
  {-1, 1, -2, 1},   Two previous pictures
  {-1, 1, -2, 0},   Two previous ictures, 2'nd not used
  {-1, 1, -2, 1, 1, 1}, Two previous and one future picture
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name : VCEncStrmStart
    Description   : Starts a new stream
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VCEncRet VCEncStrmStart(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut)
{

  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  struct container *c;
  struct vps *v;
  struct sps *s;
  struct pps *p;
  i32 i, ret = VCENC_ERROR;
  u32 client_type;

  APITRACE("VCEncStrmStart#");
  APITRACEPARAM_X("busLuma", pEncIn->busLuma);
  APITRACEPARAM_X("busChromaU", pEncIn->busChromaU);
  APITRACEPARAM_X("busChromaV", pEncIn->busChromaV);
  APITRACEPARAM("timeIncrement", pEncIn->timeIncrement);
  APITRACEPARAM_X("pOutBuf", pEncIn->pOutBuf[0]);
  APITRACEPARAM_X("busOutBuf", pEncIn->busOutBuf[0]);
  APITRACEPARAM("outBufSize", pEncIn->outBufSize[0]);
  APITRACEPARAM("codingType", pEncIn->codingType);
  APITRACEPARAM("poc", pEncIn->poc);
  APITRACEPARAM("gopSize", pEncIn->gopSize);
  APITRACEPARAM("gopPicIdx", pEncIn->gopPicIdx);
  APITRACEPARAM_X("roiMapDeltaQpAddr", pEncIn->roiMapDeltaQpAddr);

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pEncIn == NULL) || (pEncOut == NULL))
  {
    APITRACEERR("VCEncStrmStart: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncStrmStart: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }


  /* Check status */
  if ((vcenc_instance->encStatus != VCENCSTAT_INIT) &&
      (vcenc_instance->encStatus != VCENCSTAT_START_FRAME))
  {
    APITRACEERR("VCEncStrmStart: ERROR Invalid status");
    return VCENC_INVALID_STATUS;
  }

  /* Check output stream buffers */
  if (pEncIn->pOutBuf[0] == NULL)
  {
    APITRACEERR("VCEncStrmStart: ERROR Invalid output stream buffer");
    return VCENC_INVALID_ARGUMENT;
  }

  if (vcenc_instance->streamMultiSegment.streamMultiSegmentMode == 0 && (pEncIn->outBufSize[0] < VCENC_STREAM_MIN_BUF0_SIZE))
  {
    APITRACEERR("VCEncStrmStart: ERROR Too small output stream buffer");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check for invalid input values */
  client_type = IS_H264(vcenc_instance->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
  if ((HW_ID_MAJOR_NUMBER(EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type))) <= 1) && (pEncIn->gopConfig.size > 1))
  {
    APITRACEERR("VCEncStrmStart: ERROR Invalid input. gopConfig");
    return VCENC_INVALID_ARGUMENT;
  }
  if (!(c = get_container(vcenc_instance))) return VCENC_ERROR;

  vcenc_instance->vps_id = 0;
  vcenc_instance->sps_id = 0;
  vcenc_instance->pps_id = 0;


  vcenc_instance->av1_inst.POCtobeDisplayAV1 = 0;
  vcenc_instance->av1_inst.MaxPocDisplayAV1  = pEncIn->gopConfig.idr_interval ? pEncIn->gopConfig.idr_interval : (1 << DEFAULT_EXPLICIT_ORDER_HINT_BITS);
  vcenc_instance->av1_inst.MaxPocDisplayAV1  = MIN(vcenc_instance->av1_inst.MaxPocDisplayAV1, (1 << DEFAULT_EXPLICIT_ORDER_HINT_BITS));

  if(vcenc_instance->pass == 1) {
    ret = EWLMallocLinear(vcenc_instance->asic.ewl, pEncIn->outBufSize[0],0, &(vcenc_instance->lookahead.internal_mem.mem));
    if(ret != EWL_OK)
        return  VCENC_ERROR;
    vcenc_instance->stream.stream = (u8*)vcenc_instance->lookahead.internal_mem.mem.virtualAddress;
    vcenc_instance->lookahead.internal_mem.pOutBuf = vcenc_instance->lookahead.internal_mem.mem.virtualAddress;
    vcenc_instance->stream.stream_bus = vcenc_instance->lookahead.internal_mem.busOutBuf = vcenc_instance->lookahead.internal_mem.mem.busAddress;
    vcenc_instance->stream.size = vcenc_instance->lookahead.internal_mem.outBufSize = vcenc_instance->lookahead.internal_mem.mem.size;
  } else {
    vcenc_instance->stream.stream = (u8 *)pEncIn->pOutBuf[0];
    vcenc_instance->stream.stream_bus = pEncIn->busOutBuf[0];
    vcenc_instance->stream.size = pEncIn->outBufSize[0];
	vcenc_instance->stream.stream_av1pre_bus = pEncIn->busAv1PrecarryOutBuf[0];
  }

  pEncOut->pNaluSizeBuf = (u32 *)vcenc_instance->asic.sizeTbl[0].virtualAddress;

  hash_init(&vcenc_instance->hashctx, pEncIn->hashType);

  vcenc_instance->temp_size = 1024 * 10;
  vcenc_instance->temp_buffer = vcenc_instance->stream.stream + vcenc_instance->stream.size - vcenc_instance->temp_size;

  pEncOut->streamSize = 0;
  pEncOut->sliceHeaderSize = 0;
  pEncOut->numNalus = 0;
  pEncOut->maxSliceStreamSize = 0;

  /* set RPS */
  if (vcenc_ref_pic_sets(vcenc_instance, pEncIn))
  {
    Error(2, ERR, "vcenc_ref_pic_sets() fails");
    return NOK;
  }

  v = (struct vps *)get_parameter_set(c, VPS_NUT, 0);
  s = (struct sps *)get_parameter_set(c, SPS_NUT, 0);
  p = (struct pps *)get_parameter_set(c, PPS_NUT, 0);

  /* Initialize parameter sets */
  if (set_parameter(vcenc_instance, pEncIn, v, s, p)) goto out;


  /* init VUI */

  VCEncSpsSetVuiAspectRatio(s,
                           vcenc_instance->sarWidth,
                           vcenc_instance->sarHeight);
  VCEncSpsSetVuiVideoInfo(s,
                         vcenc_instance->videoFullRange);
  if(pEncIn->vui_timing_info_enable)
  {
    const rcVirtualBuffer_s *vb = &vcenc_instance->rateControl.virtualBuffer;

    VCEncSpsSetVuiTimigInfo(s,
                           vb->timeScale, vb->unitsInTic);
  } else {
    VCEncSpsSetVuiTimigInfo(s, 0, 0);
  }

  if((vcenc_instance->Hdr10Color.hdr10_color_enable == ENCHW_YES) || (vcenc_instance->videoFullRange != 0))
	  VCEncSpsSetVuiSignalType(s, 1, 5, vcenc_instance->videoFullRange, vcenc_instance->Hdr10Color.hdr10_color_enable, vcenc_instance->Hdr10Color.hdr10_primary,
		  vcenc_instance->Hdr10Color.hdr10_transfer, vcenc_instance->Hdr10Color.hdr10_matrix);

  /* update VUI */
  if (vcenc_instance->rateControl.sei.enabled == ENCHW_YES)
  {
    VCEncSpsSetVuiPictStructPresentFlag(s, 1);
  }

  VCEncSpsSetVui_field_seq_flag(s, vcenc_instance->preProcess.interlacedFrame);
  if (vcenc_instance->rateControl.hrd == ENCHW_YES)
  {
    VCEncSpsSetVuiHrd(s, 1);

    VCEncSpsSetVuiHrdBitRate(s,
                            vcenc_instance->rateControl.virtualBuffer.bitRate);

    VCEncSpsSetVuiHrdCpbSize(s,
                            vcenc_instance->rateControl.virtualBuffer.bufferSize);
  }
  /* Init SEI */
  HevcInitSei(&vcenc_instance->rateControl.sei, s->streamMode == ASIC_VCENC_BYTE_STREAM,
              vcenc_instance->rateControl.hrd, vcenc_instance->rateControl.outRateNum, vcenc_instance->rateControl.outRateDenom);

  /* Create parameter set nal units */
  if (IS_HEVC(vcenc_instance->codecFormat))
  {
    v->ps.b.stream = vcenc_instance->stream.stream;
    video_parameter_set(v, inst);
    APITRACEPARAM("vps size=%d\n", *v->ps.b.cnt);
    pEncOut->streamSize += *v->ps.b.cnt;
    VCEncAddNaluSize(pEncOut, *v->ps.b.cnt);
    hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, *v->ps.b.cnt);
    vcenc_instance->stream.stream += *v->ps.b.cnt;
  }

  //H264 SVCT enable: insert H264Scalability SEI
  if (IS_H264(vcenc_instance->codecFormat) && (s->max_num_sub_layers > 1))
  {
        //Code SVCT SEI
        v->ps.b.stream = vcenc_instance->stream.stream;
        struct buffer *b = &v->ps.b;

        H264NalUnitHdr(b, 0, H264_SEI, (v->streamMode==VCENC_BYTE_STREAM) ? ENCHW_YES : ENCHW_NO);
        H264ScalabilityInfoSei(b,  s, s->max_num_sub_layers-1, vcenc_instance->rateControl.outRateNum*256/vcenc_instance->rateControl.outRateDenom);
        rbsp_trailing_bits(b);

        APITRACEPARAM("H264Scalability SEI size=%d\n", *v->ps.b.cnt);
        pEncOut->streamSize += *v->ps.b.cnt;
        VCEncAddNaluSize(pEncOut, *v->ps.b.cnt);
        hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, *v->ps.b.cnt);
        vcenc_instance->stream.stream += *v->ps.b.cnt;
  }

  if (!IS_HEVC(vcenc_instance->codecFormat))
      cnt_ref_pic_set(c, s);

  if (!IS_AV1(vcenc_instance->codecFormat)){
    s->ps.b.stream = vcenc_instance->stream.stream;
    sequence_parameter_set(c, s, inst);
    APITRACEPARAM("sps size=%d\n", *s->ps.b.cnt);
    pEncOut->streamSize += *s->ps.b.cnt;
    VCEncAddNaluSize(pEncOut, *s->ps.b.cnt);
    hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, *s->ps.b.cnt);
    vcenc_instance->stream.stream += *s->ps.b.cnt;

    HevcUpdateSeiMasteringDisplayColour(&vcenc_instance->rateControl.sei, &vcenc_instance->Hdr10Display);
    if ((vcenc_instance->rateControl.sei.hdr10_display_enable == ENCHW_YES) && IS_HEVC(vcenc_instance->codecFormat))
    {
	  u32 u32SeiCnt = 0;
	  vcenc_instance->stream.cnt = &u32SeiCnt;

	  HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, vcenc_instance->rateControl.sei.byteStream);
	  HevcMasteringDisplayColourSei(&vcenc_instance->stream, &vcenc_instance->rateControl.sei);
	  APITRACEPARAM("sei size=%d\n", u32SeiCnt);
	  pEncOut->streamSize += u32SeiCnt;
	  VCEncAddNaluSize(pEncOut, u32SeiCnt);
	  hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, u32SeiCnt);
    }

    HevcUpdateSeiContentLightLevelInfo(&vcenc_instance->rateControl.sei, &vcenc_instance->Hdr10LightLevel);
    if ((vcenc_instance->rateControl.sei.hdr10_lightlevel_enable == ENCHW_YES) && IS_HEVC(vcenc_instance->codecFormat))
    {
	  u32 u32SeiCnt = 0;
	  vcenc_instance->stream.cnt = &u32SeiCnt;

	  HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, vcenc_instance->rateControl.sei.byteStream);
	  HevcContentLightLevelSei(&vcenc_instance->stream, &vcenc_instance->rateControl.sei);
	  APITRACEPARAM("sei size=%d\n", u32SeiCnt);
	  pEncOut->streamSize += u32SeiCnt;
	  VCEncAddNaluSize(pEncOut, u32SeiCnt);
	  hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, u32SeiCnt);
    }

    p->ps.b.stream = vcenc_instance->stream.stream;
    picture_parameter_set(p, inst);
    //APITRACEPARAM("pps size", *p->ps.b.cnt);
    APITRACEPARAM("pps size=%d\n", *p->ps.b.cnt);
    pEncOut->streamSize += *p->ps.b.cnt;
    VCEncAddNaluSize(pEncOut, *p->ps.b.cnt);
    hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, *p->ps.b.cnt);
  }

  /* Status == START_STREAM   Stream started */
  vcenc_instance->encStatus = VCENCSTAT_START_STREAM;

  if (vcenc_instance->rateControl.hrd == ENCHW_YES)
  {
    /* Update HRD Parameters to RC if needed */
    u32 bitrate = VCEncSpsGetVuiHrdBitRate(s);
    u32 cpbsize = VCEncSpsGetVuiHrdCpbSize(s);

    if ((vcenc_instance->rateControl.virtualBuffer.bitRate != (i32)bitrate) ||
        (vcenc_instance->rateControl.virtualBuffer.bufferSize != (i32)cpbsize))
    {
      vcenc_instance->rateControl.virtualBuffer.bitRate = bitrate;
      vcenc_instance->rateControl.virtualBuffer.bufferSize = cpbsize;
      (void) VCEncInitRc(&vcenc_instance->rateControl, 1);
    }
  }
  if(vcenc_instance->pass==2) {
    VCEncOut encOut;
    VCEncIn encIn;
    memcpy(&encIn, pEncIn, sizeof(VCEncIn));
    encIn.gopConfig.pGopPicCfg = pEncIn->gopConfig.pGopPicCfgPass1;
    VCEncRet ret = VCEncStrmStart(vcenc_instance->lookahead.priv_inst, &encIn, &encOut);
    if (ret != VCENC_OK) {
      APITRACE("VCEncStrmStart: LookaheadStrmStart failed");
      goto out;
    }
    ret = StartLookaheadThread(&vcenc_instance->lookahead);
    if (ret != VCENC_OK) {
      APITRACE("VCEncStrmStart: StartLookaheadThread failed");
      goto out;
    }

    queue_init(&vcenc_instance->extraParaQ);//for extra parameters.
  }

  APITRACE("VCEncStrmStart: OK");
  ret = VCENC_OK;

out:
#ifdef TEST_DATA
  Enc_close_stream_trace();
#endif

  return ret;

}

/*------------------------------------------------------------------------------
  vcenc_ref_pic_sets
------------------------------------------------------------------------------*/
i32 vcenc_replace_rps(struct vcenc_instance *vcenc_instance, VCEncGopPicConfig *cfg, i32 rps_id)
{
  struct vcenc_buffer *vcenc_buffer;
  i32 *poc;

  /* Initialize tb->instance->buffer so that we can cast it to vcenc_buffer
   * by calling hevc_set_parameter(), see api.c TODO...*/
  vcenc_instance->rps_id = -1;
  vcenc_set_ref_pic_set(vcenc_instance);
  vcenc_buffer = (struct vcenc_buffer *)vcenc_instance->temp_buffer;
  poc = (i32 *)vcenc_buffer->buffer;

  u32 iRefPic, idx = 0;
  for (iRefPic = 0; iRefPic < cfg->numRefPics; iRefPic ++)
  {
      poc[idx++] = cfg->refPics[iRefPic].ref_pic;
      poc[idx++] = cfg->refPics[iRefPic].used_by_cur;
  }
  poc[idx] = 0; //end
  vcenc_instance->rps_id = rps_id;
  if (vcenc_set_ref_pic_set(vcenc_instance))
    return NOK;

  return OK;
}

struct sw_picture *get_ref_picture(struct vcenc_instance *vcenc_instance, VCEncGopPicConfig *cfg,
                                      i32 idx, bool bRecovery, bool *error)
{
  struct node *n;
  struct sw_picture *p, *ref = NULL;
  struct container *c = get_container(vcenc_instance);
  i32 curPoc = vcenc_instance->poc;
  i32 deltaPoc = cfg->refPics[idx].ref_pic;
  i32 poc = curPoc + deltaPoc;
  i32 iDelta, i;
  *error = HANTRO_TRUE;
  bRecovery = bRecovery && cfg->refPics[idx].used_by_cur;

  if (poc < 0) return NULL;

  for (n = c->picture.tail; n; n = n->next)
  {
    p = (struct sw_picture *)n;
    if (p->reference)
    {
      iDelta = p->poc - curPoc;
      if (p->poc == poc)
      {
        ref = p;
        *error = HANTRO_FALSE;
        break;
      }

      if (bRecovery && (p->encOrderInGop == 0) &&((deltaPoc * iDelta) > 0))
      {
        bool bCand = 1;
        for (i = 0; i < (i32)cfg->numRefPics; i ++)
        {
          if ((p->poc == (cfg->refPics[i].ref_pic + curPoc)) && cfg->refPics[i].used_by_cur)
          {
            bCand = 0;
            break;
          }
        }

        if (!bCand)
          continue;

        if (!ref)
          ref = p;
        else
        {
          i32 lastDelta = ref->poc - curPoc;
          if (ABS(iDelta) < ABS(lastDelta))
            ref = p;
        }
      }
    }
  }
  return ref;
}

static i32 check_multipass_references (struct vcenc_instance *vcenc_instance, struct sps *s,
                                               VCEncGopPicConfig *pConfig, bool bRecovery)
{
  struct sw_picture *p;
  struct container *c = get_container(vcenc_instance);
  i32 i, j;
  bool error = HANTRO_FALSE;
  VCEncGopPicConfig tmpConfig = *pConfig;
  VCEncGopPicConfig *cfg = &tmpConfig;

  for (i = 0; i < (i32)cfg->numRefPics; i ++)
  {
    bool iErr;
    i32 refPoc = vcenc_instance->poc + cfg->refPics[i].ref_pic;
    p = get_ref_picture(vcenc_instance, cfg, i, bRecovery, &iErr);

    if (iErr)
      error = HANTRO_TRUE;
    else
      continue;

    bool remove = HANTRO_FALSE;
    if (!p)
      remove = HANTRO_TRUE;
    else if (p->poc != refPoc)
    {
      for (j = 0; j < (i32)cfg->numRefPics; j ++)
      {
        cfg->refPics[i].ref_pic = p->poc - vcenc_instance->poc;
        if((j != i) && (cfg->refPics[j].ref_pic == cfg->refPics[i].ref_pic))
        {
          if (cfg->refPics[i].used_by_cur)
            cfg->refPics[j].used_by_cur = 1;
          remove = HANTRO_TRUE;
          break;
        }
      }
    }

    if (remove)
    {
      for (j = i; j < (i32)cfg->numRefPics - 1; j ++)
      {
        cfg->refPics[j] = cfg->refPics[j + 1];
      }
      cfg->numRefPics --;
      i --;
    }
  }

  vcenc_instance->RpsInSliceHeader = HANTRO_FALSE;
  if (error)
  {
    if (cfg->numRefPics == 0)
    {
      vcenc_instance->rps_id = s->num_short_term_ref_pic_sets;
    }
    else
    {
      i32 rps_id = s->num_short_term_ref_pic_sets - 1;
      vcenc_replace_rps(vcenc_instance, cfg, rps_id);
      vcenc_instance->rps_id = rps_id;
      if (!IS_H264(vcenc_instance->codecFormat))
        vcenc_instance->RpsInSliceHeader = HANTRO_TRUE;
    }
  }

  return cfg->numRefPics;
}

static VCEncPictureCodingType check_references(struct vcenc_instance *vcenc_instance,
                                                    const VCEncIn *pEncIn,
                                                    struct sps *s,
                                                    VCEncGopPicConfig *cfg,
                                                    VCEncPictureCodingType codingType)
{
  u32 i;
  if (vcenc_instance->pass)
  {
    i32 numRef = check_multipass_references(vcenc_instance, s, cfg, pEncIn->gopPicIdx == 0);
    if (numRef == 0)
      codingType = VCENC_INTRA_FRAME;
    else if (numRef == 1)
      codingType = VCENC_PREDICTED_FRAME;
  }
  else
  {
    for (i = 0; i < cfg->numRefPics; i ++)
    {
      if (cfg->refPics[i].used_by_cur && (vcenc_instance->poc + cfg->refPics[i].ref_pic) < 0)
      {
        vcenc_instance->rps_id = s->num_short_term_ref_pic_sets - 1; //IPPPPPP
        codingType = VCENC_PREDICTED_FRAME;
      }
    }
  }
  return codingType;
}

/*------------------------------------------------------------------------------
select RPS according to coding type and interlace option
--------------------------------------------------------------------------------*/
static VCEncPictureCodingType get_rps_id(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn,
                                           struct sps *s, u8 *rpsModify)
{
  VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(pEncIn->gopConfig));
  VCEncPictureCodingType codingType = pEncIn->codingType;
  i32 bak = vcenc_instance->RpsInSliceHeader;

  if ((vcenc_instance->gdrEnabled == 1) && (vcenc_instance->encStatus == VCENCSTAT_START_FRAME) && (vcenc_instance->gdrFirstIntraFrame == 0))
  {

    if (pEncIn->codingType == VCENC_INTRA_FRAME)
    {
      vcenc_instance->gdrStart++ ;
      codingType = VCENC_PREDICTED_FRAME;
    }
  }
  //Intra
  if (codingType == VCENC_INTRA_FRAME && vcenc_instance->poc == 0)
  {
    vcenc_instance->rps_id = s->num_short_term_ref_pic_sets;
  }
  //else if ((codingType == VCENC_INTRA_FRAME) && ((vcenc_instance->u32IsPeriodUsingLTR != 0 && vcenc_instance->u8IdxEncodedAsLTR != 0)
  //    ||  vcenc_instance->poc > 0))
  else if(pEncIn->i8SpecialRpsIdx >= 0)
  {
    vcenc_instance->rps_id = s->num_short_term_ref_pic_sets - 1 - pEncIn->gopConfig.special_size + pEncIn->i8SpecialRpsIdx;
  }
  else
  {
    vcenc_instance->rps_id = gopCfg->id;
    // check reference validness
    VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[vcenc_instance->rps_id]);
    codingType = check_references (vcenc_instance, pEncIn, s, cfg, codingType);
  }

  *rpsModify = vcenc_instance->RpsInSliceHeader;
  vcenc_instance->RpsInSliceHeader = bak;

  return codingType;
}

/*------------------------------------------------------------------------------
calculate the val of H264 nal_ref_idc
--------------------------------------------------------------------------------*/

static u32 h264_refIdc_val(const VCEncIn *pEncIn)
{
	u32 nalRefIdc_2bit = 0;
	u8 idx = pEncIn->gopConfig.id;
	if(pEncIn->codingType == VCENC_INTRA_FRAME)
	{
		nalRefIdc_2bit = 3;
	}
	else if (pEncIn->gopConfig.special_size != 0 && (pEncIn->poc >= 0) && (pEncIn->poc % pEncIn->gopConfig.pGopPicSpecialCfg[1].i32Interval == 0))
	{
		nalRefIdc_2bit = pEncIn->gopConfig.pGopPicSpecialCfg[1].temporalId;
	}
	else if (pEncIn->gopConfig.size != 0)
	{
		nalRefIdc_2bit = pEncIn->gopConfig.pGopPicCfg[idx].temporalId;
	}
	return nalRefIdc_2bit;
}


/*------------------------------------------------------------------------------
check if delta_poc is in rps
(0 for unused, 1 for short term ref, 2+LongTermFrameIdx for long term ref)
--------------------------------------------------------------------------------*/
static int h264_ref_in_use(int delta_poc, int poc, struct rps *r, const i32 *long_term_ref_pic_poc)
{
  struct ref_pic *p;
  int i;
  for (i = 0; i < r->num_lt_pics; i++)
  {
    int id = r->ref_pic_lt[i].delta_poc;
    if(id >= 0 && (poc == long_term_ref_pic_poc[id]) && (INVALITED_POC != long_term_ref_pic_poc[id]))
      return 2+id;
  }

  for (i = 0; i < r->num_negative_pics; i++)
  {
    p = &r->ref_pic_s0[i];
    if(delta_poc == p->delta_poc)
      return 1;
  }

  for (i = 0; i < r->num_positive_pics; i++)
  {
    p = &r->ref_pic_s1[i];
    if(delta_poc == p->delta_poc)
      return 1;
  }
  return 0;
}
/*------------------------------------------------------------------------------
collect unused ref frames for H.264 MMO
--------------------------------------------------------------------------------*/
bool h264_mmo_collect(struct vcenc_instance *vcenc_instance, struct sw_picture *pic, const VCEncIn *pEncIn)
{
  struct container *c = get_container(vcenc_instance);
  if(!c) return HANTRO_FALSE;
  const VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(pEncIn->gopConfig));

  /* get the next coded pic's info */
  int poc2 = vcenc_instance->poc + gopCfg->delta_poc_to_next;
  int id = gopCfg->id_next;
  if (id == 255)
      id = pic->sps->num_short_term_ref_pic_sets - 1 - ((pEncIn->gopConfig.special_size == 0)? 1: pEncIn->gopConfig.special_size);
  //if ( -1 != pEncIn->i8SpecialRpsIdx_next)
  //    id = pic->sps->num_short_term_ref_pic_sets - 1 - pEncIn->gopConfig.special_size + pEncIn->i8SpecialRpsIdx_next;
  struct rps *r2 = (struct rps *)get_parameter_set(c, RPS, id);
  if (!r2) return HANTRO_FALSE;

  struct rps *r = pic->rps;
  int i,j;

  if (pic->sliceInst->type == I_SLICE) {	

	pic->nalRefIdc = 1;
	
	if (1 == vcenc_instance->layerInRefIdc)
	  pic->nalRefIdc_2bit = h264_refIdc_val(pEncIn);
	
    pic->markCurrentLongTerm = (pEncIn->u8IdxEncodedAsLTR > 0 ? 1 : 0);
    pic->curLongTermIdx = (pic->markCurrentLongTerm ? (pEncIn->u8IdxEncodedAsLTR-1) : -1);

    if (vcenc_instance->poc == 0)
    {
        vcenc_instance->h264_mmo_nops = 0;
        return HANTRO_TRUE;
    }
  }

  /* marking the pic in the ref list */
  for (i = 0; i < r->before_cnt + r->after_cnt + r->lt_current_cnt; i++) {
    struct sw_picture *ref = pic->rpl[0][i];
    int inuse = h264_ref_in_use(ref->poc - poc2, ref->poc, r2, pEncIn->long_term_ref_pic); /* whether used as ref of next pic */
    if(inuse == 0) {
      vcenc_instance->h264_mmo_unref[vcenc_instance->h264_mmo_nops] = ref->frameNum;
      bool is_long = (h264_ref_in_use(ref->poc-pic->poc, ref->poc, r, pEncIn->long_term_ref_pic) >= 2);
      vcenc_instance->h264_mmo_long_term_flag[vcenc_instance->h264_mmo_nops] = is_long;
      vcenc_instance->h264_mmo_ltIdx[vcenc_instance->h264_mmo_nops++] = -1;  // unref short/long
    }
    else if(inuse > 1 && h264_ref_in_use(ref->poc-pic->poc, ref->poc, r, pEncIn->long_term_ref_pic) == 1 && (0 == pEncIn->bIsPeriodUsingLTR)) {
      vcenc_instance->h264_mmo_unref[vcenc_instance->h264_mmo_nops] = ref->frameNum;
      vcenc_instance->h264_mmo_long_term_flag[vcenc_instance->h264_mmo_nops] = 0;
      vcenc_instance->h264_mmo_ltIdx[vcenc_instance->h264_mmo_nops++] = inuse-2;  // str-> ltr
      ref->curLongTermIdx = inuse-2;
    }
  }

  for (i = 0; i < r->lt_follow_cnt; i++) {
    struct sw_picture *ref = get_picture(c, r->lt_follow[i]);

    int inuse = h264_ref_in_use(ref->poc - poc2, ref->poc, r2, pEncIn->long_term_ref_pic);
    if(inuse == 0) {
      vcenc_instance->h264_mmo_unref[vcenc_instance->h264_mmo_nops] = ref->frameNum;
      vcenc_instance->h264_mmo_long_term_flag[vcenc_instance->h264_mmo_nops] = 1;
      vcenc_instance->h264_mmo_ltIdx[vcenc_instance->h264_mmo_nops++] = -1;  // unref long
    }
    else if(inuse > 1 && h264_ref_in_use(ref->poc-pic->poc, ref->poc, r, pEncIn->long_term_ref_pic) == 1 && (0 == pEncIn->bIsPeriodUsingLTR)) {
      vcenc_instance->h264_mmo_unref[vcenc_instance->h264_mmo_nops] = ref->frameNum;
      vcenc_instance->h264_mmo_long_term_flag[vcenc_instance->h264_mmo_nops] = 0;
      vcenc_instance->h264_mmo_ltIdx[vcenc_instance->h264_mmo_nops++] = inuse-2;  // short -> long
      ref->curLongTermIdx = inuse-2;
    }
  }

  /* marking cur coded pic */
  i32 long_term_ref_pic_poc_2[VCENC_MAX_LT_REF_FRAMES];
  for (i = 0; i < VCENC_MAX_LT_REF_FRAMES; i++)
      long_term_ref_pic_poc_2[i] = INVALITED_POC;
  for(i = 0; i < pEncIn->gopConfig.ltrcnt; i++) {
    if(HANTRO_TRUE == pEncIn->bLTR_need_update[i])
      long_term_ref_pic_poc_2[i] = pic->poc;
    else
      long_term_ref_pic_poc_2[i] = pEncIn->long_term_ref_pic[i];
  }
  pic->nalRefIdc = h264_ref_in_use(pic->poc - poc2, pic->poc, r2, long_term_ref_pic_poc_2);
  pic->markCurrentLongTerm = 0;
  pic->curLongTermIdx = -1;
  if(pic->nalRefIdc >= 2) {
    pic->markCurrentLongTerm = 1;
    pic->curLongTermIdx = pic->nalRefIdc - 2;
    pic->nalRefIdc = 1;
  }
  if((0 != pEncIn->bIsPeriodUsingLTR) && (0!= pEncIn->u8IdxEncodedAsLTR)) {
    pic->markCurrentLongTerm = 1;
    pic->curLongTermIdx = pEncIn->u8IdxEncodedAsLTR-1;
    pic->nalRefIdc = 1;
  }
  if(1 == vcenc_instance->layerInRefIdc)
  {
	if(1 == pic->nalRefIdc)
	  pic->nalRefIdc_2bit = h264_refIdc_val(pEncIn);
	else
	  pic->nalRefIdc_2bit = 0;
  }
  return HANTRO_TRUE;
}

/*------------------------------------------------------------------------------
mark unused ref frames in lX_used_by_next_pic for H.264 MMO
--------------------------------------------------------------------------------*/
void h264_mmo_mark_unref(regValues_s *regs, int frame_num, int ltrflag, int ltIdx, int cnt[2], struct sw_picture *pic)
{
  int i;

  // search frame_num in current reference list
  for (i = 0; i < pic->sliceInst->active_l0_cnt; i++)
  {
    if(frame_num == pic->rpl[0][i]->frameNum) {
      regs->l0_used_by_next_pic[i] = 0;
      regs->l0_long_term_flag[i] = ltrflag;
      regs->l0_referenceLongTermIdx[i] = ltIdx;
      return;
    }
  }

  if (pic->sliceInst->type == B_SLICE)
  {
    for (i = 0; i < pic->sliceInst->active_l1_cnt; i++)
    {
      if(frame_num == pic->rpl[1][i]->frameNum) {
        regs->l1_used_by_next_pic[i] = 0;
        regs->l1_long_term_flag[i] = ltrflag;
        regs->l1_referenceLongTermIdx[i] = ltIdx;
        return;
      }
    }
  }

  // insert in free slot
  ASSERT(cnt[0] + cnt[1] < 4);
  if(cnt[0] < 2) {
    i = cnt[0]++;
    regs->l0_delta_framenum[i] = pic->frameNum - frame_num;
    regs->l0_used_by_next_pic[i] = 0;
    regs->l0_long_term_flag[i] = ltrflag;
    regs->l0_referenceLongTermIdx[i] = ltIdx;
  } else {
    i = cnt[1]++;
    regs->l1_delta_framenum[i] = pic->frameNum - frame_num;
    regs->l1_used_by_next_pic[i] = 0;
    regs->l1_long_term_flag[i] = ltrflag;
    regs->l1_referenceLongTermIdx[i] = ltIdx;
  }
}

#ifdef CTBRC_STRENGTH

static i32 float2fixpoint(float data)
{
    i32 i = 0;
    i32 result = 0;
    float pow2=2.0;
    /*0.16 format*/
    float base = 0.5;
    for(i= 0; i<16 ;i++)
    {
        result <<= 1;
        if(data >= base)
        {
           result |= 1;
           data -= base;

        }

        pow2 *= 2;
        base = 1.0/pow2;

    }
    return result;

}
static i32 float2fixpoint8(float data)
{
    i32 i = 0;
    i32 result = 0;
    float pow2=2.0;
    /*0.16 format*/
    float base = 0.5;
    for(i= 0; i<8 ;i++)
    {
        result <<= 1;
        if(data >= base)
        {
           result |= 1;
           data -= base;

        }

        pow2 *= 2;
        base = 1.0/pow2;

    }
    return result;

}
#endif

static VCEncRet getCtbRcParams (struct vcenc_instance *inst, enum slice_type type)
{
  if (inst == NULL)
    return VCENC_ERROR;

  ctbRateControl_s *pCtbRateCtrl = &(inst->rateControl.ctbRateCtrl);
  asicData_s *asic = &(inst->asic);

  if ((!asic->regs.asicCfg.ctbRcVersion) || (!(asic->regs.rcRoiEnable&0x08)))
    return VCENC_ERROR;

  pCtbRateCtrl->models[type].x0 = asic->regs.ctbRcModelParam0 =
    EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM0);

  pCtbRateCtrl->models[type].x1 = asic->regs.ctbRcModelParam1 =
    EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM1);

  pCtbRateCtrl->models[type].preFrameMad = asic->regs.prevPicLumMad =
    EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_PREV_PIC_LUM_MAD);

  pCtbRateCtrl->qpSumForRc = asic->regs.ctbQpSumForRc =
    EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_QP_SUM_FOR_RC) |
   (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_QP_SUM_FOR_RC_MSB) << 24);

  pCtbRateCtrl->models[type].started = 1;

  return VCENC_OK;
}

static VCEncRet VCEncGetSSIM (struct vcenc_instance *inst, VCEncOut *pEncOut)
{
  if ((inst == NULL) || (pEncOut == NULL))
    return VCENC_ERROR;

  pEncOut->ssim[0] = pEncOut->ssim[1] = pEncOut->ssim[2] = 0.0;

  asicData_s *asic = &inst->asic;
  if ((!asic->regs.asicCfg.ssimSupport) || (!asic->regs.ssim))
    return VCENC_ERROR;

  const i32 shift_y  = (inst->sps->bit_depth_luma_minus8==0)   ? SSIM_FIX_POINT_FOR_8BIT : SSIM_FIX_POINT_FOR_10BIT;
  const i32 shift_uv = (inst->sps->bit_depth_chroma_minus8==0) ? SSIM_FIX_POINT_FOR_8BIT : SSIM_FIX_POINT_FOR_10BIT;
  i64 ssim_numerator_y = (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_NUMERATOR_MSB);
  i64 ssim_numerator_u = (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_U_NUMERATOR_MSB);
  i64 ssim_numerator_v = (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_V_NUMERATOR_MSB);
  u32 ssim_denominator_y  = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_DENOMINATOR);
  u32 ssim_denominator_uv = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_UV_DENOMINATOR);

  ssim_numerator_y = ssim_numerator_y << 32;
  ssim_numerator_u = ssim_numerator_u << 32;
  ssim_numerator_v = ssim_numerator_v << 32;
  ssim_numerator_y |= EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_NUMERATOR_LSB);
  ssim_numerator_u |= EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_U_NUMERATOR_LSB);
  ssim_numerator_v |= EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_V_NUMERATOR_LSB);

  if (ssim_denominator_y)
    pEncOut->ssim[0] = ssim_numerator_y * 1.0 / (1 << shift_y)  / ssim_denominator_y;

  if (ssim_denominator_uv)
  {
    pEncOut->ssim[1] = ssim_numerator_u * 1.0 / (1 << shift_uv) / ssim_denominator_uv;
    pEncOut->ssim[2] = ssim_numerator_v * 1.0 / (1 << shift_uv) / ssim_denominator_uv;
  }

#if 0
  double ssim = pEncOut->ssim[0] * 0.8 + 0.1 * (pEncOut->ssim[1] + pEncOut->ssim[2]);
  printf("    SSIM %.4f SSIM Y %.4f U %.4f V %.4f\n", ssim, pEncOut->ssim[0], pEncOut->ssim[1], pEncOut->ssim[2]);
#endif
  return VCENC_OK;
}

static void onSliceReady (struct vcenc_instance *inst, VCEncSliceReady *slice_callback)
{
  if ((!inst) || (!slice_callback))
    return;

  asicData_s *asic = &inst->asic;
  i32 sliceInc = slice_callback->slicesReady - slice_callback->slicesReadyPrev;
  if (sliceInc > 0)
  {
    slice_callback->nalUnitInfoNum += sliceInc;
    if (asic->regs.prefixnal_svc_ext)
      slice_callback->nalUnitInfoNum += sliceInc;
  }

  if (inst->sliceReadyCbFunc &&
      (slice_callback->slicesReadyPrev < slice_callback->slicesReady) &&
      (inst->rateControl.hrd == ENCHW_NO))
  {
    inst->sliceReadyCbFunc(slice_callback);
  }

  slice_callback->slicesReadyPrev = slice_callback->slicesReady;
  slice_callback->nalUnitInfoNumPrev = slice_callback->nalUnitInfoNum;
}

VCEncRet VCEncStrmWaitReady(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut, struct sw_picture *pic, VCEncSliceReady *slice_callback, struct container *c)
{
   struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
   asicData_s *asic = &vcenc_instance->asic;
   struct node *n;
   struct sps *s = pic->sps;

  i32 ret = VCENC_ERROR;
  u32 status = ASIC_STATUS_ERROR;
  int  sliceNum;
  i32  i;

  do
  {
    /* Encode one frame */
    i32 ewl_ret;

    /* Wait for IRQ for every slice or for complete frame */
    if ((asic->regs.sliceNum > 1) && vcenc_instance->sliceReadyCbFunc)
      ewl_ret = EWLWaitHwRdy(asic->ewl, &slice_callback->slicesReady, asic->regs.sliceNum, &status);
    else
      ewl_ret = EWLWaitHwRdy(asic->ewl, NULL, asic->regs.sliceNum, &status);

    if (pEncIn->dec400Enable == 1)
      VCEncDisableDec400(asic->ewl);

    if (ewl_ret != EWL_OK)
    {
      status = ASIC_STATUS_ERROR;

      if (ewl_ret == EWL_ERROR)
      {
        /* IRQ error => Stop and release HW */
        ret = VCENC_SYSTEM_ERROR;
      }
      else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
      {
        /* IRQ Timeout => Stop and release HW */
        ret = VCENC_HW_TIMEOUT;
      }
      APITRACEERR("VCEncStrmEncode: ERROR Fatal system error ewl_ret != EWL_OK.");
      EncAsicStop(asic->ewl);
      /* Release HW so that it can be used by other codecs */
      EWLReleaseHw(asic->ewl);
    }
    else
    {
      /* Check ASIC status bits and possibly release HW */
      status = EncAsicCheckStatus_V2(asic, status);

      switch (status)
      {
        case ASIC_STATUS_ERROR:
          APITRACEERR("VCEncStrmEncode: ERROR HW bus access error");
          EWLReleaseHw(asic->ewl);
          ret = VCENC_ERROR;
          break;
        case ASIC_STATUS_FUSE_ERROR:
          APITRACEERR("VCEncStrmEncode: ERROR HW fuse error");
          EWLReleaseHw(asic->ewl);
          ret = VCENC_ERROR;
          break;
        case ASIC_STATUS_HW_TIMEOUT:
          APITRACEERR("VCEncStrmEncode: ERROR HW/IRQ timeout");
          EWLReleaseHw(asic->ewl);
          ret = VCENC_HW_TIMEOUT;
          break;
        case ASIC_STATUS_SLICE_READY:
        case ASIC_STATUS_LINE_BUFFER_DONE: /* low latency */
        case (ASIC_STATUS_LINE_BUFFER_DONE|ASIC_STATUS_SLICE_READY):
        case ASIC_STATUS_SEGMENT_READY:
          if (status&ASIC_STATUS_LINE_BUFFER_DONE)
          {
            APITRACE("VCEncStrmEncode: IRQ Line Buffer INT");
            /* clear the interrupt */
            if (!vcenc_instance->inputLineBuf.inputLineBufHwModeEn)
            { /* SW handshaking: Software will clear the line buffer interrupt and then update the
               *   line buffer write pointer, when the next line buffer is ready. The encoder will
               *   continue to run when detected the write pointer is updated.  */
              if (vcenc_instance->inputLineBuf.cbFunc)
                  vcenc_instance->inputLineBuf.cbFunc(vcenc_instance->inputLineBuf.cbData);
            }
          }
          if (status&ASIC_STATUS_SLICE_READY)
          {
            APITRACE("VCEncStrmEncode: IRQ Slice Ready");
            /* Issue callback to application telling how many slices
             * are available. */
            onSliceReady (vcenc_instance, slice_callback);
          }

          if (status&ASIC_STATUS_SEGMENT_READY)
          {
            APITRACE("VCEncStrmEncode: IRQ stream segment INT");
            while(vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0 &&
                  vcenc_instance->streamMultiSegment.rdCnt < EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STRM_SEGMENT_WR_PTR))
            {
              if (vcenc_instance->streamMultiSegment.cbFunc)
                vcenc_instance->streamMultiSegment.cbFunc(vcenc_instance->streamMultiSegment.cbData);
              /*note: must make sure the data of one segment is read by app then rd counter can increase*/
              vcenc_instance->streamMultiSegment.rdCnt++;
            }
            EncAsicWriteRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STRM_SEGMENT_RD_PTR,vcenc_instance->streamMultiSegment.rdCnt);
          }

          ret = VCENC_OK;
          break;
        case ASIC_STATUS_BUFF_FULL:
          APITRACEERR("VCEncStrmEncode: ERROR buffer full.");
          EWLReleaseHw(asic->ewl);
          vcenc_instance->output_buffer_over_flow = 1;
          //inst->stream.overflow = ENCHW_YES;
          ret = VCENC_OK;
          break;
        case ASIC_STATUS_HW_RESET:
          APITRACEERR("VCEncStrmEncode: ERROR HW abnormal");
          EWLReleaseHw(asic->ewl);
          ret = VCENC_HW_RESET;
          break;
        case ASIC_STATUS_FRAME_READY:
          APITRACE("VCEncStrmEncode: IRQ Frame Ready");
          /* clear all interrupt */
          if (asic->regs.sliceReadyInterrupt)
          {
            //this is used for hardware interrupt.
            slice_callback->slicesReady = asic->regs.sliceNum;
            onSliceReady (vcenc_instance, slice_callback);
          }
          sliceNum = 0;
          for (n = pic->slice.tail; n; n = n->next)
          {
            u32 sliceBytes = ((asic->sizeTbl[(vcenc_instance->jobCnt+1)%vcenc_instance->parallelCoreNum].virtualAddress))[pEncOut->numNalus];
            pEncOut->numNalus++;
            if (asic->regs.prefixnal_svc_ext)
            {
              sliceBytes += ((asic->sizeTbl[(vcenc_instance->jobCnt+1)%vcenc_instance->parallelCoreNum].virtualAddress))[pEncOut->numNalus];
              pEncOut->numNalus++;
            }

            APIPRINT(vcenc_instance->verbose, "POC %3d slice %d size=%d\n", vcenc_instance->poc, sliceNum, sliceBytes);
            sliceNum++;
            pEncOut->maxSliceStreamSize = MAX(pEncOut->maxSliceStreamSize, sliceBytes);
#if 0
            /* Stream header remainder ie. last not full 64-bit address
             * is counted in HW data. */
            const u32 hw_offset = inst->stream.byteCnt & (0x07U);

            inst->stream.byteCnt +=
              asic->regs.outputStrmSize - hw_offset;
            inst->stream.stream +=
              asic->regs.outputStrmSize - hw_offset;
#endif
          }
          u32 hashval = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_HASH_VAL);
          u32 hashoffset = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_HASH_OFFSET);
          hash_reset(&vcenc_instance->hashctx, hashval, hashoffset);
          hashval = hash_finalize(&vcenc_instance->hashctx);
          if(vcenc_instance->hashctx.hash_type == 1)
            printf("POC %3d crc32 %08x\n", vcenc_instance->poc, hashval);
          else if(vcenc_instance->hashctx.hash_type == 2)
            printf("POC %3d checksum %08x\n", vcenc_instance->poc, hashval);
          hash_init(&vcenc_instance->hashctx, vcenc_instance->hashctx.hash_type);
          vcenc_instance->streamMultiSegment.rdCnt = 0;

          ((asic->sizeTbl[(vcenc_instance->jobCnt+1)%vcenc_instance->parallelCoreNum].virtualAddress))[pEncOut->numNalus] = 0;
          asic->regs.outputStrmSize[0] =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
          vcenc_instance->stream.byteCnt += asic->regs.outputStrmSize[0];
          pEncOut->sliceHeaderSize =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SLICE_HEADER_SIZE);
          asic->regs.frameCodingType =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_FRAME_CODING_TYPE);
#ifdef CTBRC_STRENGTH
          asic->regs.sumOfQP =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_QP_SUM) |
           (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_QP_SUM_MSB) << 26);
          asic->regs.sumOfQPNumber =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_QP_NUM) |
           (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_QP_NUM_MSB) << 20);
          asic->regs.picComplexity =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_PIC_COMPLEXITY) |
           (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_PIC_COMPLEXITY_MSB) << 23);
#else
          asic->regs.averageQP =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_AVERAGEQP);
          asic->regs.nonZeroCount =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_NONZEROCOUNT);
#endif

          //for adaptive GOP
          vcenc_instance->rateControl.intraCu8Num= asic->regs.intraCu8Num =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_INTRACU8NUM) |
           (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_INTRACU8NUM_MSB) << 20);

          vcenc_instance->rateControl.skipCu8Num= asic->regs.skipCu8Num =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SKIPCU8NUM) |
           (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SKIPCU8NUM_MSB) << 20);

          vcenc_instance->rateControl.PBFrame4NRdCost = asic->regs.PBFrame4NRdCost =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_PBFRAME4NRDCOST);

          // video stabilization
          asic->regs.stabMotionMin = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MINIMUM);
          asic->regs.stabMotionSum = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MOTION_SUM);
          asic->regs.stabGmvX    = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_GMVX);
          asic->regs.stabGmvY    = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_GMVY);
          asic->regs.stabMatrix1 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX1);
          asic->regs.stabMatrix2 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX2);
          asic->regs.stabMatrix3 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX3);
          asic->regs.stabMatrix4 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX4);
          asic->regs.stabMatrix5 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX5);
          asic->regs.stabMatrix6 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX6);
          asic->regs.stabMatrix7 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX7);
          asic->regs.stabMatrix8 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX8);
          asic->regs.stabMatrix9 = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STAB_MATRIX9);

          //CTB_RC
          asic->regs.ctbBitsMin =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTBBITSMIN);
          asic->regs.ctbBitsMax =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CTBBITSMAX);
          asic->regs.totalLcuBits =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_TOTALLCUBITS);
          vcenc_instance->iThreshSigmaCalcd = asic->regs.nrThreshSigmaCalced =
                                               EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_THRESH_SIGMA_CALCED);
          vcenc_instance->iSigmaCalcd = asic->regs.nrFrameSigmaCalced =
                                         EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_FRAME_SIGMA_CALCED);


          asic->regs.SSEDivide256 =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_SSE_DIV_256);
          {

          /* calculate PSNR_Y, HW calculate SSE, SW calculate log10f
              In HW, 8 bottom lines in every 64 height unit is skipped to save line buffer
              SW log10f operation will waste too much CPU cycles, we suggest not use it, comment it out by default
              */
          //pEncOut->psnr_y = (asic->regs.SSEDivide256 == 0)? 999999.0 :
          //                  10.0 * log10f((float)((256 << asic->regs.outputBitWidthLuma) - 1) * ((256 << asic->regs.outputBitWidthLuma) - 1) * asic->regs.picWidth * (asic->regs.picHeight - ((asic->regs.picHeight>>6)<<3))/ (float)(asic->regs.SSEDivide256 * ((256 << asic->regs.outputBitWidthLuma) << asic->regs.outputBitWidthLuma)));
          }
          asic->regs.lumSSEDivide256 =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_LUM_SSE_DIV_256);
          asic->regs.cbSSEDivide64 =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CB_SSE_DIV_64);
          asic->regs.crSSEDivide64 =
            EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CR_SSE_DIV_64);
          /* calculate PSNR_Y, HW calculate SSE, SW calculate log10f
              SW log10f operation will waste too much CPU cycles, we suggest not use it, disable by default
           */
#ifdef DJI
          pEncOut->psnr_y_predeb = (asic->regs.lumSSEDivide256 == 0)? 999999.0 :
                            10.0 * log10f((float)((256 << asic->regs.outputBitWidthLuma) - 1) * ((256 << asic->regs.outputBitWidthLuma) - 1) * asic->regs.picWidth * asic->regs.picHeight / (float)(asic->regs.lumSSEDivide256 * ((256 << asic->regs.outputBitWidthLuma) << asic->regs.outputBitWidthLuma)));
          pEncOut->psnr_cb_predeb = (asic->regs.cbSSEDivide64 == 0)? 999999.0 :
                            10.0 * log10f((float)((256 << asic->regs.outputBitWidthChroma) - 1) * ((256 << asic->regs.outputBitWidthChroma) - 1) * (asic->regs.picWidth/2) * (asic->regs.picHeight/2) / (float)(asic->regs.cbSSEDivide64 * ((64 << asic->regs.outputBitWidthChroma) << asic->regs.outputBitWidthChroma)));
          pEncOut->psnr_cr_predeb = (asic->regs.crSSEDivide64 == 0)? 999999.0 :
                            10.0 * log10f((float)((256 << asic->regs.outputBitWidthChroma) - 1) * ((256 << asic->regs.outputBitWidthChroma) - 1) * (asic->regs.picWidth/2) * (asic->regs.picHeight/2) / (float)(asic->regs.crSSEDivide64 * ((64 << asic->regs.outputBitWidthChroma) << asic->regs.outputBitWidthChroma)));
#endif
          /* motion score for 2-pass Agop */
          if (asic->regs.asicCfg.bMultiPassSupport) {
            asic->regs.motionScore[0][0] = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_MOTION_SCORE_L0_0);;
            asic->regs.motionScore[0][1] = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_MOTION_SCORE_L0_1);;
            asic->regs.motionScore[1][0] = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_MOTION_SCORE_L1_0);;
            asic->regs.motionScore[1][1] = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_MOTION_SCORE_L1_1);;
          }

          vcenc_instance->rateControl.hierarchial_sse[vcenc_instance->rateControl.hierarchial_bit_allocation_GOP_size-1][vcenc_instance->rateControl.gopPoc] = asic->regs.SSEDivide256;

          if (asic->regs.asicCfg.ssimSupport)
            VCEncGetSSIM(vcenc_instance, pEncOut);

          if ((asic->regs.asicCfg.ctbRcVersion > 0) && IS_CTBRC_FOR_BITRATE(vcenc_instance->rateControl.ctbRc))
            getCtbRcParams(vcenc_instance, pic->sliceInst->type);

          /* should before Release */
          if (vcenc_instance->pass!=1)
            EWLTraceProfile(asic->ewl);

          EWLReleaseHw(asic->ewl);
          if(s->long_term_ref_pics_present_flag) {
            if( 0 != pEncIn->u8IdxEncodedAsLTR) {
                if(IS_H264(vcenc_instance->codecFormat)) {
                  // H.264 ltr can not be reused after replaced
                  struct sw_picture *p_old;
                  i32 old_poc = pEncIn->long_term_ref_pic[pEncIn->u8IdxEncodedAsLTR-1];
                  if (old_poc >= 0 && (p_old = get_picture(c, old_poc))) {
                    p_old->h264_no_ref = HANTRO_TRUE;
                  }
                }
              }
#if 0  /* Not needed (mmco=4, set max long-term index) */
            if(codingType == VCENC_INTRA_FRAME && pic->poc == 0 && vcenc_instance->rateControl.ltrCnt > 0)
              asic->regs.max_long_term_frame_idx_plus1 = vcenc_instance->rateControl.ltrCnt;
            else
#endif
              asic->regs.max_long_term_frame_idx_plus1 = 0;
          }
          ret = VCENC_OK;
          break;

        default:
          APITRACEERR("VCEncStrmEncode: ERROR Fatal system error");
          /* should never get here */
          ASSERT(0);
          ret = VCENC_ERROR;
      }
    }
  }
  while (status & (ASIC_STATUS_SLICE_READY|ASIC_STATUS_LINE_BUFFER_DONE|ASIC_STATUS_SEGMENT_READY));

  return ret;
}

#define SLICE_TYPE_P            0
#define SLICE_TYPE_B            1
#define SLICE_TYPE_I            2
#define SLICE_TYPE_SP           3
#define SLICE_TYPE_SI           4

void useExtPara(const VCEncExtParaIn *pEncExtParaIn, asicData_s *asic,i32 h264Codec )
{
  i32 i,j;
  regValues_s *regs=&asic->regs;


  regs->poc = pEncExtParaIn->recon.poc;
  regs->frameNum = pEncExtParaIn->recon.frame_num;

  //                = pEncExtParaIn->recon.flags;
  regs->reconLumBase = pEncExtParaIn->recon.busReconLuma;
  regs->reconCbBase  = pEncExtParaIn->recon.busReconChromaUV;

  regs->reconL4nBase = pEncExtParaIn->recon.reconLuma_4n;

  regs->recon_luma_compress_tbl_base = pEncExtParaIn->recon.compressTblReconLuma;
  regs->recon_chroma_compress_tbl_base = pEncExtParaIn->recon.compressTblReconChroma;

  regs->colctbs_store_base = pEncExtParaIn->recon.colBufferH264Recon;



  //cu info output
  regs->cuInfoTableBase = pEncExtParaIn->recon.cuInfoMemRecon;
  regs->cuInfoDataBase = pEncExtParaIn->recon.cuInfoMemRecon +  asic->cuInfoTableSize;;


  if(h264Codec)
  {
    //regs->frameNum = pEncExtParaIn->params.h264Para.frame_num;
    if(regs->frameCodingType == 1)//I
    {
      if(pEncExtParaIn->params.h264Para.idr_pic_flag)
      {
        asic->regs.nal_unit_type = H264_IDR;
      }
      else
        asic->regs.nal_unit_type = H264_NONIDR;
    }


    regs->idrPicId = pEncExtParaIn->params.h264Para.idr_pic_id&1;//(pEncExtParaIn->params.h264Para.idr_pic_flag)? (pEncExtParaIn->params.h264Para.idr_pic_id):0;

    regs->nalRefIdc = pEncExtParaIn->params.h264Para.reference_pic_flag;
    //  regs->transform8x8Enable
    //  regs->entropy_coding_mode_flag
    regs->colctbs_load_base = 0;
    if (pEncExtParaIn->params.h264Para.slice_type == SLICE_TYPE_I ||
        pEncExtParaIn->params.h264Para.slice_type == SLICE_TYPE_SI)
    {
        if((pEncExtParaIn->params.h264Para.idr_pic_flag))
        {
            //not need curLongTermIdx
            regs->currentLongTermIdx = 0;
            regs->markCurrentLongTerm = !!(pEncExtParaIn->recon.flags&LONG_TERM_REFERENCE);
        }
        else
        {
            //curLongTermIdx
            regs->markCurrentLongTerm = !!(pEncExtParaIn->recon.flags&LONG_TERM_REFERENCE);
            regs->currentLongTermIdx = pEncExtParaIn->recon.frame_idx;
        }
    }



    regs->l0_delta_framenum[0] = 0;
    regs->l0_delta_framenum[1] = 0;
    regs->l1_delta_framenum[0] = 0;
    regs->l1_delta_framenum[1] = 0;

    regs->l0_used_by_curr_pic[0] = 0;
    regs->l0_used_by_curr_pic[1] = 0;
    regs->l1_used_by_curr_pic[0] = 0;
    regs->l1_used_by_curr_pic[1] = 0;
    regs->l0_long_term_flag[0] = 0;
    regs->l0_long_term_flag[1] = 0;
    regs->l1_long_term_flag[0] = 0;
    regs->l1_long_term_flag[1] = 0;
    regs->num_long_term_pics = 0;

    if((pEncExtParaIn->params.h264Para.slice_type == SLICE_TYPE_P)||(pEncExtParaIn->params.h264Para.slice_type == SLICE_TYPE_B))
    {
      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l0_active_minus1+1); i ++)
      {
        regs->l0_delta_framenum[i] = regs->frameNum - pEncExtParaIn->reflist0[i].frame_num;
        regs->l0_referenceLongTermIdx[i] = pEncExtParaIn->reflist0[i].frame_idx;
      }
      //regs->l0_referenceLongTermIdx[i] = (pic->rpl[0][i]->long_term_flag ? pic->rpl[0][i]->curLongTermIdx : 0);
      //libva not support mmco so
      if(pEncExtParaIn->recon.flags&LONG_TERM_REFERENCE)
      {
        //curLongTermIdx
        regs->markCurrentLongTerm = 1;
      }
      else
      {
        regs->markCurrentLongTerm = 0;
      }

      regs->currentLongTermIdx = pEncExtParaIn->recon.frame_idx;

      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l0_active_minus1+1); i ++)
        regs->l0_used_by_curr_pic[i] = 1;

      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l0_active_minus1+1); i++)
      {
        regs->pRefPic_recon_l0[0][i] = pEncExtParaIn->reflist0[i].busReconLuma;
        regs->pRefPic_recon_l0[1][i] = pEncExtParaIn->reflist0[i].busReconChromaUV;
        regs->pRefPic_recon_l0_4n[i] =  pEncExtParaIn->reflist0[i].reconLuma_4n;

        regs->l0_delta_poc[i] = pEncExtParaIn->reflist0[i].poc - pEncExtParaIn->recon.poc;
        regs->l0_long_term_flag[i] = !!(pEncExtParaIn->reflist0[i].flags&LONG_TERM_REFERENCE);
        regs->num_long_term_pics += regs->l0_long_term_flag[i];

        //compress
        regs->ref_l0_luma_compressed[i] = regs->recon_luma_compress;
        regs->ref_l0_luma_compress_tbl_base[i] = pEncExtParaIn->reflist0[i].compressTblReconLuma;

        regs->ref_l0_chroma_compressed[i] = regs->recon_chroma_compress;
        regs->ref_l0_chroma_compress_tbl_base[i] = pEncExtParaIn->reflist0[i].compressTblReconChroma;
      }

    }

    if (pEncExtParaIn->params.h264Para.slice_type == SLICE_TYPE_B)
    {
      regs->colctbs_load_base = pEncExtParaIn->reflist1[0].colBufferH264Recon;


      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l1_active_minus1+1); i ++)
      {
        regs->l1_delta_framenum[i] = regs->frameNum - pEncExtParaIn->reflist1[i].frame_num;
        regs->l1_referenceLongTermIdx[i] = pEncExtParaIn->reflist1[i].frame_idx;
      }
      //libva not support mmco so
      if(pEncExtParaIn->recon.flags&LONG_TERM_REFERENCE)
      {
        //curLongTermIdx
        regs->markCurrentLongTerm = 1;
      }
      else
        regs->markCurrentLongTerm = 0;

      regs->currentLongTermIdx = pEncExtParaIn->recon.frame_idx;

      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l1_active_minus1+1); i++)
        regs->l1_used_by_curr_pic[i] = 1;

      for (i = 0; i < (pEncExtParaIn->params.h264Para.num_ref_idx_l1_active_minus1+1); i++)
      {
        regs->pRefPic_recon_l1[0][i] = pEncExtParaIn->reflist1[i].busReconLuma;
        regs->pRefPic_recon_l1[1][i] = pEncExtParaIn->reflist1[i].busReconChromaUV;
        regs->pRefPic_recon_l1_4n[i] =  pEncExtParaIn->reflist1[i].reconLuma_4n;

        regs->l1_delta_poc[i] = pEncExtParaIn->reflist1[i].poc - pEncExtParaIn->recon.poc;
        regs->l1_long_term_flag[i] = !!(pEncExtParaIn->reflist1[i].flags&LONG_TERM_REFERENCE);
        regs->num_long_term_pics += regs->l1_long_term_flag[i];

        //compress
        regs->ref_l1_luma_compressed[i] = regs->recon_luma_compress;
        regs->ref_l1_luma_compress_tbl_base[i] = pEncExtParaIn->reflist1[i].compressTblReconLuma;

        regs->ref_l1_chroma_compressed[i] = regs->recon_chroma_compress;
        regs->ref_l1_chroma_compress_tbl_base[i] = pEncExtParaIn->reflist1[i].compressTblReconChroma;
      }
    }

    /* H.264 MMO  libva not support this feature*/
    regs->l0_used_by_next_pic[0] = 1;
    regs->l0_used_by_next_pic[1] = 1;
    regs->l1_used_by_next_pic[0] = 1;
    regs->l1_used_by_next_pic[1] = 1;
  }
  else
  {
    //hevc
    //regs->frameNum = pEncExtParaIn->params.hevcPara.frame_num;
    if(regs->frameCodingType == 1)//I
    {
      if(pEncExtParaIn->params.hevcPara.idr_pic_flag)
      {
        asic->regs.nal_unit_type = IDR_W_RADL;
      }
      else
        asic->regs.nal_unit_type = TRAIL_R;
    }

    //can del this row, this register is only used for h264
    regs->idrPicId = pEncExtParaIn->params.hevcPara.idr_pic_id&1;//s(regs->frameCodingType == 1)? (pEncExtParaIn->params.hevcPara.idr_pic_id & 1):0;

    //can del this row, this register is only used for h264
    regs->nalRefIdc = pEncExtParaIn->params.hevcPara.reference_pic_flag;
    //  regs->transform8x8Enable
    //  regs->entropy_coding_mode_flag
    regs->colctbs_load_base = 0;
    //
    if (pEncExtParaIn->params.hevcPara.slice_type == SLICE_TYPE_I ||
        pEncExtParaIn->params.hevcPara.slice_type == SLICE_TYPE_SI)
    {
        //can del this row, this register is only used for h264
        regs->markCurrentLongTerm = !!(pEncExtParaIn->recon.flags&LONG_TERM_REFERENCE);
    }



    //not used for hevc
    regs->l0_delta_framenum[0] = 0;
    regs->l0_delta_framenum[1] = 0;
    regs->l1_delta_framenum[0] = 0;
    regs->l1_delta_framenum[1] = 0;

    regs->l0_used_by_curr_pic[0] = 0;
    regs->l0_used_by_curr_pic[1] = 0;
    regs->l1_used_by_curr_pic[0] = 0;
    regs->l1_used_by_curr_pic[1] = 0;
    regs->l0_long_term_flag[0] = 0;
    regs->l0_long_term_flag[1] = 0;
    regs->l1_long_term_flag[0] = 0;
    regs->l1_long_term_flag[1] = 0;
    regs->num_long_term_pics = 0;

    if((pEncExtParaIn->params.hevcPara.slice_type == SLICE_TYPE_P)||(pEncExtParaIn->params.hevcPara.slice_type == SLICE_TYPE_B))
    {
      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l0_active_minus1+1); i ++)
      {
        regs->l0_delta_framenum[i] = regs->frameNum - pEncExtParaIn->reflist0[i].frame_num;
        regs->l0_referenceLongTermIdx[i] = pEncExtParaIn->reflist0[i].frame_idx;
      }
      //libva not support mmco so
      regs->markCurrentLongTerm = 0;

      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l0_active_minus1+1); i ++)
        regs->l0_used_by_curr_pic[i] = 1;

      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l0_active_minus1+1); i++)
      {
        regs->pRefPic_recon_l0[0][i] = pEncExtParaIn->reflist0[i].busReconLuma;
        regs->pRefPic_recon_l0[1][i] = pEncExtParaIn->reflist0[i].busReconChromaUV;
        regs->pRefPic_recon_l0_4n[i] =  pEncExtParaIn->reflist0[i].reconLuma_4n;

        regs->l0_delta_poc[i] = pEncExtParaIn->reflist0[i].poc - pEncExtParaIn->recon.poc;
        regs->l0_long_term_flag[i] = !!(pEncExtParaIn->reflist0[i].flags&LONG_TERM_REFERENCE);
        regs->num_long_term_pics += regs->l0_long_term_flag[i];

        //compress
        regs->ref_l0_luma_compressed[i] = regs->recon_luma_compress;
        regs->ref_l0_luma_compress_tbl_base[i] = pEncExtParaIn->reflist0[i].compressTblReconLuma;

        regs->ref_l0_chroma_compressed[i] = regs->recon_chroma_compress;
        regs->ref_l0_chroma_compress_tbl_base[i] = pEncExtParaIn->reflist0[i].compressTblReconChroma;
      }

    }

    if (pEncExtParaIn->params.hevcPara.slice_type == SLICE_TYPE_B)
    {
      regs->colctbs_load_base = pEncExtParaIn->reflist1[0].colBufferH264Recon;


      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l1_active_minus1+1); i ++)
      {
        regs->l1_delta_framenum[i] = regs->frameNum - pEncExtParaIn->reflist1[i].frame_num;
        regs->l1_referenceLongTermIdx[i] = pEncExtParaIn->reflist1[i].frame_idx;
      }
      //libva not support mmco so
      regs->markCurrentLongTerm = 0;

      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l1_active_minus1+1); i++)
        regs->l1_used_by_curr_pic[i] = 1;

      for (i = 0; i < (pEncExtParaIn->params.hevcPara.num_ref_idx_l1_active_minus1+1); i++)
      {
        regs->pRefPic_recon_l1[0][i] = pEncExtParaIn->reflist1[i].busReconLuma;
        regs->pRefPic_recon_l1[1][i] = pEncExtParaIn->reflist1[i].busReconChromaUV;
        regs->pRefPic_recon_l1_4n[i] =  pEncExtParaIn->reflist1[i].reconLuma_4n;

        regs->l1_delta_poc[i] = pEncExtParaIn->reflist1[i].poc - pEncExtParaIn->recon.poc;
        regs->l1_long_term_flag[i] = !!(pEncExtParaIn->reflist1[i].flags&LONG_TERM_REFERENCE);
        regs->num_long_term_pics += regs->l1_long_term_flag[i];

        //compress
        regs->ref_l1_luma_compressed[i] = regs->recon_luma_compress;
        regs->ref_l1_luma_compress_tbl_base[i] = pEncExtParaIn->reflist1[i].compressTblReconLuma;

        regs->ref_l1_chroma_compressed[i] = regs->recon_chroma_compress;
        regs->ref_l1_chroma_compress_tbl_base[i] = pEncExtParaIn->reflist1[i].compressTblReconChroma;
      }
    }
#if 0
    //used for long term ref?
    {
      for (i = (pEncExtParaIn->params.hevcPara.num_ref_idx_l0_active_minus1+1), j = 0; i < 2 && j < r->lt_follow_cnt; i++, j++) {
        asic->regs.l0_delta_poc[i] = r->lt_follow[j] - pic->poc;
        regs->l0_delta_poc[i] = pEncExtParaIn->reflist0[i].poc - pEncExtParaIn->recon.poc;
        asic->regs.l0_long_term_flag[i] = 1;
      }
      for (i = (pEncExtParaIn->params.hevcPara.num_ref_idx_l1_active_minus1+1); i < 2 && j < r->lt_follow_cnt; i++, j++) {
        asic->regs.l1_delta_poc[i] = r->lt_follow[j] - pic->poc;
        asic->regs.l1_long_term_flag[i] = 1;
      }
    }
#endif
    /* H.264 MMO  libva not support this feature*/
    regs->l0_used_by_next_pic[0] = 1;
    regs->l0_used_by_next_pic[1] = 1;
    regs->l1_used_by_next_pic[0] = 1;
    regs->l1_used_by_next_pic[1] = 1;

    if(regs->num_long_term_pics)
        regs->long_term_ref_pics_present_flag = 1;



    regs->rps_neg_pic_num = pEncExtParaIn->params.hevcPara.rps_neg_pic_num;
	regs->rps_pos_pic_num = pEncExtParaIn->params.hevcPara.rps_pos_pic_num;
	ASSERT(regs->rps_neg_pic_num + regs->rps_pos_pic_num <= VCENC_MAX_REF_FRAMES);
	for ( i = 0; i < (i32)(regs->rps_neg_pic_num+regs->rps_pos_pic_num); i++)
	{
		regs->rps_delta_poc[i] = pEncExtParaIn->params.hevcPara.rps_delta_poc[i];
		regs->rps_used_by_cur[i] = pEncExtParaIn->params.hevcPara.rps_used_by_cur[i];
	}
	for (i=regs->rps_neg_pic_num+regs->rps_pos_pic_num; i < VCENC_MAX_REF_FRAMES; i++)
	{
		regs->rps_delta_poc[i] = 0;
		regs->rps_used_by_cur[i] = 0;
	}
  }
}
/*------------------------------------------------------------------------------

    Function name : VCEncStrmEncode
    Description   : Encodes a new picture
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/


VCEncRet VCEncStrmEncode(VCEncInst inst, const VCEncIn *pEncIn,
                             VCEncOut *pEncOut,
                             VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                             void *pAppData)
{
  VCEncRet ret = VCEncStrmEncodeExt(inst,pEncIn,NULL,pEncOut,sliceReadyCbFunc,pAppData,0);
  return ret;
}
/*------------------------------------------------------------------------------

    Function name : VCEncStrmEncode
    Description   : Encodes a new picture
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/


VCEncRet VCEncStrmEncodeExt(VCEncInst inst, const VCEncIn *pEncIn,
                                const VCEncExtParaIn *pEncExtParaIn,
                               VCEncOut *pEncOut,
                               VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                               void *pAppData,i32 useExtFlag)

{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  asicData_s *asic = &vcenc_instance->asic;
  VCEncGopPicConfig sSliceRpsCfg;
  struct container *c;
  struct vcenc_buffer source;
  struct vps *v;
  struct sps *s;
  struct pps *p;
  struct rps *r;
  struct sw_picture *pic;
  struct slice *slice;
  i32 bSkipFrame = 0;
  i32 tmp, i, j;
  i32 ret = VCENC_ERROR;
  VCEncSliceReady slice_callback;
  VCEncPictureCodingType codingType;
  i32 top_pos, bottom_pos;
  u32 client_type;
  i32 core_id = -1;
  const void *ewl = NULL;
  i32 i32NalTmp;
  u8 rpsInSliceHeader = vcenc_instance->RpsInSliceHeader;
  VCDec400data dec400_data;

  APITRACE("VCEncStrmEncode#");
  APITRACEPARAM_X("busLuma", pEncIn->busLuma);
  APITRACEPARAM_X("busChromaU", pEncIn->busChromaU);
  APITRACEPARAM_X("busChromaV", pEncIn->busChromaV);
  APITRACEPARAM("timeIncrement", pEncIn->timeIncrement);
  APITRACEPARAM_X("pOutBuf", pEncIn->pOutBuf[0]);
  APITRACEPARAM_X("busOutBuf", pEncIn->busOutBuf[0]);
  APITRACEPARAM("outBufSize", pEncIn->outBufSize[0]);
  if (vcenc_instance->asic.regs.asicCfg.streamBufferChain)
  {
    APITRACEPARAM_X("pOutBuf1", pEncIn->pOutBuf[1]);
    APITRACEPARAM_X("busOutBuf1", pEncIn->busOutBuf[1]);
    APITRACEPARAM("outBufSize1", pEncIn->outBufSize[1]);
  }
  APITRACEPARAM("codingType", pEncIn->codingType);
  APITRACEPARAM("poc", pEncIn->poc);
  APITRACEPARAM("gopSize", pEncIn->gopSize);
  APITRACEPARAM("gopPicIdx", pEncIn->gopPicIdx);
  APITRACEPARAM_X("roiMapDeltaQpAddr", pEncIn->roiMapDeltaQpAddr);


  VCEncLookaheadJob *outframe = NULL;
  if(vcenc_instance->pass==2) {

    if(pEncExtParaIn != NULL){//for extra parameters.
      VCEncExtParaIn *pExtParaInBuf = (VCEncExtParaIn*)malloc(sizeof(VCEncExtParaIn));
      memcpy(pExtParaInBuf, pEncExtParaIn, sizeof(VCEncExtParaIn));
      queue_put(&vcenc_instance->extraParaQ, (struct node *)pExtParaInBuf);
    }

    if(pEncIn != NULL)
      AddPictureToLookahead(&vcenc_instance->lookahead, pEncIn, pEncOut);
    outframe = GetLookaheadOutput(&vcenc_instance->lookahead, pEncIn==NULL);
    if (outframe == NULL)
      return VCENC_OK;
    if (outframe->status != VCENC_FRAME_READY) {
      VCEncRet status = outframe->status;
      free(outframe);
      return status;
    }
    pEncIn = &outframe->encIn;
    //pEncOut = &outframe->encOut;
    i32 nextGopSize = outframe->frame.gopSize;
    if(outframe->frame.frameNum != 0)
      outframe->encIn.gopSize = vcenc_instance->lookahead.lastGopSize;
    outframe->encIn.picture_cnt = vcenc_instance->lookahead.picture_cnt;
    outframe->encIn.last_idr_picture_cnt = vcenc_instance->lookahead.last_idr_picture_cnt;
    outframe->encIn.codingType = (outframe->frame.frameNum == 0) ? VCENC_INTRA_FRAME : VCEncFindNextPic(inst, &outframe->encIn, nextGopSize, pEncIn->gopConfig.gopCfgOffset, false);
    vcenc_instance->lookahead.picture_cnt = outframe->encIn.picture_cnt;
    vcenc_instance->lookahead.last_idr_picture_cnt = outframe->encIn.last_idr_picture_cnt;
    if(vcenc_instance->extDSRatio) {
      /* switch to full resolution yuv for 2nd pass */
      outframe->encIn.busLuma = outframe->encIn.busLumaOrig;
      outframe->encIn.busChromaU = outframe->encIn.busChromaUOrig;
      outframe->encIn.busChromaV = outframe->encIn.busChromaVOrig;
    }
    vcenc_instance->cuTreeCtl.costCur = outframe->frame.cost;
    vcenc_instance->cuTreeCtl.curTypeChar = outframe->frame.typeChar;
    vcenc_instance->cuTreeCtl.gopSize = outframe->frame.gopSize;
    vcenc_instance->lookahead.lastGopPicIdx = outframe->encIn.gopPicIdx;
    vcenc_instance->lookahead.lastGopSize = outframe->encIn.gopSize;
    for (i = 0; i < 4; i ++)
    {
      vcenc_instance->cuTreeCtl.costAvg[i] = outframe->frame.costAvg[i];
      vcenc_instance->cuTreeCtl.FrameTypeNum[i] = outframe->frame.FrameTypeNum[i];
      vcenc_instance->cuTreeCtl.costGop[i] = outframe->frame.costGop[i];
      vcenc_instance->cuTreeCtl.FrameNumGop[i] = outframe->frame.FrameNumGop[i];
    }

  }

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pEncIn == NULL) || (pEncOut == NULL))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }
  /* Check status, INIT and ERROR not allowed */
  if ((vcenc_instance->encStatus != VCENCSTAT_START_STREAM) &&
      (vcenc_instance->encStatus != VCENCSTAT_START_FRAME))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid status");
    return VCENC_INVALID_STATUS;
  }

  /* Check for invalid input values */
  client_type = IS_H264(vcenc_instance->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
  if ((pEncIn->gopSize > 1) &&
       (HW_ID_MAJOR_NUMBER(EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type))) <= 1))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid gopSize");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pEncIn->codingType > VCENC_NOTCODED_FRAME)
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid coding type");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check output stream buffers */
  if ((pEncIn->busOutBuf[0] == 0) || (pEncIn->pOutBuf[0] == NULL))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid output stream buffer");
    return VCENC_INVALID_ARGUMENT;
  }

  if ((vcenc_instance->streamMultiSegment.streamMultiSegmentMode == 0) && (pEncIn->outBufSize[0] < VCENC_STREAM_MIN_BUF0_SIZE))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Too small output stream buffer");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pEncIn->busOutBuf[1] || pEncIn->pOutBuf[1] || pEncIn->outBufSize[1])
  {
    if (!asic->regs.asicCfg.streamBufferChain)
    {
      APITRACEERR("VCEncStrmEncode: ERROR Two stream buffer not supported");
      return VCENC_INVALID_ARGUMENT;
    }
    else if ((pEncIn->busOutBuf[1] == 0) || (pEncIn->pOutBuf[1] == NULL))
    {
      APITRACEERR("VCEncStrmEncode: ERROR Invalid output stream buffer1");
      return VCENC_INVALID_ARGUMENT;
    }
    else if (vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0)
    {
      APITRACEERR("VCEncStrmEncode:two output buffer not support multi-segment");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if (vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0 && vcenc_instance->parallelCoreNum > 1)
  {
    APITRACEERR("VCEncStrmEncode: multi-segment not support multi-core");
    return VCENC_INVALID_ARGUMENT;
  }

  /* check GDR */
  if ((vcenc_instance->gdrEnabled) && (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME))
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR gdr not support B frame");
    return VCENC_INVALID_ARGUMENT;
  }
  /* check limitation for H.264 baseline profile */
  if (IS_H264(vcenc_instance->codecFormat) && vcenc_instance->profile == 66 && pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)
  {
    APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid frame type for baseline profile");
    return ENCHW_NOK;
  }

  switch (vcenc_instance->preProcess.inputFormat)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_PLANAR_10BIT_I010:
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC:
      if (!VCENC_BUS_CH_ADDRESS_VALID(pEncIn->busChromaV))
      {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busChromaV");
        return VCENC_INVALID_ARGUMENT;
      }
      /* fall through */
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV420_PLANAR_10BIT_P010:
    case VCENC_YUV420_SEMIPLANAR_8BIT_FB:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB:
    case VCENC_YUV420_PLANAR_10BIT_P010_FB:
    case VCENC_YUV420_SEMIPLANAR_101010:
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
    case VCENC_YUV420_10BIT_TILE_32_4:
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
    case VCENC_YUV420_8BIT_TILE_8_8:
    case VCENC_YUV420_10BIT_TILE_8_8:
      if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busChromaU))
      {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busChromaU");
        return VCENC_INVALID_ARGUMENT;
      }
      /* fall through */
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_H264:
      if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busLuma))
      {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busLuma");
        return VCENC_INVALID_ARGUMENT;
      }
      break;
    default:
      APITRACEERR("VCEncStrmEncode: ERROR Invalid input format");
      return VCENC_INVALID_ARGUMENT;
  }

  if (vcenc_instance->preProcess.videoStab)
  {
    if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busLumaStab))
    {
      APITRACE("VCEncStrmEncodeExt: ERROR Invalid input busLumaStab");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  /*stride feature only support YUV420SP and YUV422*/
  if ((vcenc_instance->input_alignment > 1) &&
      ((vcenc_instance->preProcess.inputFormat == VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR)||
       (vcenc_instance->preProcess.inputFormat == VCENC_YUV420_10BIT_PACKED_Y0L2)||
       (vcenc_instance->preProcess.inputFormat == VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC)||
       (vcenc_instance->preProcess.inputFormat == VCENC_YUV420_PLANAR_8BIT_DAHUA_H264)))
    {
      APITRACEERR("VCEncStrmEncode: WARNING alignment doesn't support input format");
    }

  if (((vcenc_instance->preProcess.inputFormat == VCENC_YUV420_SEMIPLANAR_8BIT_FB)||
      (vcenc_instance->preProcess.inputFormat == VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB)||
      (vcenc_instance->preProcess.inputFormat == VCENC_YUV420_PLANAR_10BIT_P010_FB))&&
      ((vcenc_instance->preProcess.lumHeightSrc & 0x3)!=0))
  {
      APITRACEERR("VCEncStrmEncode: the source height not supported by the format");
      return VCENC_INVALID_ARGUMENT;
  }

  if (pEncIn->bSkipFrame)
  {
    if (vcenc_instance->asic.regs.asicCfg.roiMapVersion >= 2)
        bSkipFrame = pEncIn->bSkipFrame;
    else
    {
      APITRACEERR("VCEncStrmEncode: FRAME_SKIP not supported by HW, force to disable it");
    }
  }

#ifdef INTERNAL_TEST
  /* Configure the encoder instance according to the test vector */
  HevcConfigureTestBeforeFrame(vcenc_instance);
#endif

  for (i=0;i < (i32)EWLGetCoreNum(); i++)
  {
    EWLHwConfig_t cfg = EncAsicGetAsicConfig(i);

    if (client_type == EWL_CLIENT_TYPE_H264_ENC && cfg.h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if (client_type == EWL_CLIENT_TYPE_HEVC_ENC && cfg.hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
      continue;

    if ((cfg.bFrameEnabled == EWL_HW_CONFIG_NOT_SUPPORTED) && ((pEncIn->codingType) == VCENC_BIDIR_PREDICTED_FRAME))
    {
      APITRACEERR("VCEncStrmEncode: Invalid coding format, B frame not supported by core");
      continue;
    }

    if ((cfg.IframeOnly == EWL_HW_CONFIG_ENABLED) && ((pEncIn->codingType) != VCENC_INTRA_FRAME))
    {
      APITRACEERR("VCEncStrmEncode: Invalid coding format, only I frame supported by core");
      continue;
    }

    if (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)
      vcenc_instance->featureToSupport.bFrameEnabled = 1;

    if (client_type == EWL_CLIENT_TYPE_H264_ENC)
    {
      vcenc_instance->featureToSupport.h264Enabled = 1;
      vcenc_instance->featureToSupport.hevcEnabled = 0;
    }
    else
    {
      vcenc_instance->featureToSupport.h264Enabled = 0;
      vcenc_instance->featureToSupport.hevcEnabled = 1;
    }

    core_id = i;
    break;
  }

  if (core_id < 0)
  {
    APITRACEERR("VCEncStrmEncode: Invalid coding format not supported by HW");
    return ENCHW_NOK;
  }

  memset(&sSliceRpsCfg, 0,sizeof(VCEncGopPicConfig));

  asic->regs.roiMapDeltaQpAddr = pEncIn->roiMapDeltaQpAddr;
  asic->regs.RoimapCuCtrlAddr  = pEncIn->RoimapCuCtrlAddr;
  asic->regs.RoimapCuCtrlIndexAddr = pEncIn->RoimapCuCtrlIndexAddr;
  asic->regs.RoimapCuCtrl_enable           = vcenc_instance->RoimapCuCtrl_enable;
  asic->regs.RoimapCuCtrl_index_enable     = vcenc_instance->RoimapCuCtrl_index_enable;
  asic->regs.RoimapCuCtrl_ver              = vcenc_instance->RoimapCuCtrl_ver;
  asic->regs.RoiQpDelta_ver                = vcenc_instance->RoiQpDelta_ver;

  asic->regs.sram_base_lum_fwd = pEncIn->extSRAMLumFwdBase;
  asic->regs.sram_base_lum_bwd = pEncIn->extSRAMLumBwdBase;
  asic->regs.sram_base_chr_fwd = pEncIn->extSRAMChrFwdBase;
  asic->regs.sram_base_chr_bwd = pEncIn->extSRAMChrBwdBase;

  if(vcenc_instance->pass == 1) {
    vcenc_instance->stream.stream = (u8*)vcenc_instance->lookahead.internal_mem.pOutBuf;
    vcenc_instance->stream.stream_bus = vcenc_instance->lookahead.internal_mem.busOutBuf;
    vcenc_instance->stream.size = vcenc_instance->lookahead.internal_mem.outBufSize;
  } else {
    vcenc_instance->stream.stream = (u8 *)pEncIn->pOutBuf[0];
    vcenc_instance->stream.stream_bus = pEncIn->busOutBuf[0];
    vcenc_instance->stream.size = pEncIn->outBufSize[0];
  }
  vcenc_instance->stream.cnt = &vcenc_instance->stream.byteCnt;

  pEncOut->pNaluSizeBuf = (u32 *)vcenc_instance->asic.sizeTbl[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].virtualAddress;

  pEncOut->streamSize = 0;
  pEncOut->numNalus = 0;
  pEncOut->maxSliceStreamSize = 0;
  pEncOut->codingType = VCENC_NOTCODED_FRAME;
  i32NalTmp = 0;

  vcenc_instance->sliceReadyCbFunc = sliceReadyCbFunc;
  vcenc_instance->pAppData[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum] = pAppData;
  i32 coreIdx = vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum;
  for (i = 0; i < MAX_STRM_BUF_NUM; i ++)
  {
    vcenc_instance->streamBufs[coreIdx].buf[i] = (u8 *)pEncIn->pOutBuf[i];
    vcenc_instance->streamBufs[coreIdx].bufLen[i] = pEncIn->outBufSize[i];
  }

  /* low latency */
  vcenc_instance->inputLineBuf.wrCnt = pEncIn->lineBufWrCnt;

  /* Clear the NAL unit size table */
  if (pEncOut->pNaluSizeBuf != NULL)
    pEncOut->pNaluSizeBuf[0] = 0;

  vcenc_instance->stream.byteCnt = 0;
  pEncOut->sliceHeaderSize = 0;

  asic->regs.outputStrmBase[0] = (ptr_t)vcenc_instance->stream.stream_bus;
  asic->regs.outputStrmSize[0] = vcenc_instance->stream.size;
  asic->regs.Av1PreoutputStrmBase[0] = (ptr_t)vcenc_instance->stream.stream_bus;
  if (asic->regs.asicCfg.streamBufferChain)
  {
    asic->regs.outputStrmBase[1] = pEncIn->busOutBuf[1];
    asic->regs.outputStrmSize[1] = pEncIn->outBufSize[1];

	asic->regs.Av1PreoutputStrmBase[1] = pEncIn->busAv1PrecarryOutBuf[1];
  }

  if (!(c = get_container(vcenc_instance))) return VCENC_ERROR;
  if (!IS_AV1(vcenc_instance->codecFormat)){
    if((pEncIn->resendVPS) && (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_HEVC))
    {
      struct vps * v;

      v = (struct vps *)get_parameter_set(c, VPS_NUT, vcenc_instance->vps_id);
      v->ps.b = vcenc_instance->stream;
      tmp = vcenc_instance->stream.byteCnt;
      video_parameter_set(v, inst);

      pEncOut->streamSize += vcenc_instance->stream.byteCnt-tmp;
      hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, vcenc_instance->stream.byteCnt-tmp);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);

      tmp = vcenc_instance->stream.byteCnt;
      vcenc_instance->stream = v->ps.b;
      vcenc_instance->stream.byteCnt = tmp;
    }

    if(pEncIn->sendAUD)
    {

      tmp = vcenc_instance->stream.byteCnt;
      if(IS_H264(vcenc_instance->codecFormat))
      {
        H264AccessUnitDelimiter(&vcenc_instance->stream,vcenc_instance->asic.regs.streamMode,pEncIn->codingType);
      }
      else
      {
        HEVCAccessUnitDelimiter(&vcenc_instance->stream,vcenc_instance->asic.regs.streamMode,pEncIn->codingType);
      }
      pEncOut->streamSize += vcenc_instance->stream.byteCnt-tmp;
      hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, vcenc_instance->stream.byteCnt-tmp);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
    }

    if(pEncIn->resendSPS)
    {
      struct sps * s;

      s = (struct sps *)get_parameter_set(c, SPS_NUT, vcenc_instance->sps_id);
      s->ps.b = vcenc_instance->stream;
      tmp = vcenc_instance->stream.byteCnt;
      sequence_parameter_set(c, s, inst);

      pEncOut->streamSize += vcenc_instance->stream.byteCnt-tmp;
      hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, vcenc_instance->stream.byteCnt-tmp);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);

      tmp = vcenc_instance->stream.byteCnt;
      vcenc_instance->stream = s->ps.b;
      vcenc_instance->stream.byteCnt = tmp;

    }

    if(pEncIn->resendPPS)
    {
      struct pps * p;
      for(i=0;i<=vcenc_instance->maxPPSId;i++)
      {
        p = (struct pps *)get_parameter_set(c, PPS_NUT, i);
        p->ps.b = vcenc_instance->stream;
        tmp = vcenc_instance->stream.byteCnt;
        //printf("byteCnt=%d\n",vcenc_instance->stream.byteCnt);
        // p->ps.b.stream = vcenc_instance->stream.stream;
        picture_parameter_set(p, inst);

        //printf("pps inserted size=%d\n", vcenc_instance->stream.byteCnt-tmp);
        pEncOut->streamSize += vcenc_instance->stream.byteCnt-tmp;
        hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, vcenc_instance->stream.byteCnt-tmp);
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);

        //printf("pps size=%d\n", *p->ps.b.cnt);
        //pEncOut->streamSize += *p->ps.b.cnt;
        //VCEncAddNaluSize(pEncOut, *p->ps.b.cnt);
        tmp = vcenc_instance->stream.byteCnt;
        vcenc_instance->stream = p->ps.b;
        vcenc_instance->stream.byteCnt = tmp;
      }

    }

    if(vcenc_instance->insertNewPPS)
    {
      struct pps * p;

      p = (struct pps *)get_parameter_set(c, PPS_NUT, vcenc_instance->insertNewPPSId);
      p->ps.b = vcenc_instance->stream;
      tmp = vcenc_instance->stream.byteCnt;
      //printf("byteCnt=%d\n",vcenc_instance->stream.byteCnt);
      // p->ps.b.stream = vcenc_instance->stream.stream;
      picture_parameter_set(p, inst);

      //printf("pps inserted size=%d\n", vcenc_instance->stream.byteCnt-tmp);
      pEncOut->streamSize += vcenc_instance->stream.byteCnt-tmp;
      hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, vcenc_instance->stream.byteCnt-tmp);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);

      //printf("pps size=%d\n", *p->ps.b.cnt);
      //pEncOut->streamSize += *p->ps.b.cnt;
      //VCEncAddNaluSize(pEncOut, *p->ps.b.cnt);
      tmp = vcenc_instance->stream.byteCnt;
      vcenc_instance->stream = p->ps.b;
      vcenc_instance->stream.byteCnt = tmp;

      //printf("byteCnt=%d\n",vcenc_instance->stream.byteCnt);
      vcenc_instance->insertNewPPS = 0;
    }
  }


  vcenc_instance->poc = pEncIn->poc;

  if(IS_AV1(vcenc_instance->codecFormat)){
      vcenc_instance->av1_inst.is_preserve_last_as_gld = ENCHW_NO;
      if(pEncIn->gopConfig.pGopPicCfg[pEncIn->gopConfig.gopCfgOffset[pEncIn->gopSize]].numRefPics > 1){
          for(i32 i=0; i < pEncIn->gopConfig.pGopPicCfg[pEncIn->gopConfig.gopCfgOffset[pEncIn->gopSize]].numRefPics; i++){
              i32 ref_poc = pEncIn->gopConfig.pGopPicCfg[pEncIn->gopConfig.gopCfgOffset[pEncIn->gopSize]].refPics[i].ref_pic;
              if((!IS_LONG_TERM_REF_DELTAPOC(ref_poc)) && (ABS(ref_poc) > pEncIn->gopSize) && (pEncIn->gopConfig.pGopPicCfg[pEncIn->gopPicIdx+pEncIn->gopConfig.gopCfgOffset[pEncIn->gopSize]].poc == 1)){
                  vcenc_instance->av1_inst.is_preserve_last_as_gld = ENCHW_YES;
                  break;
              }
          }
      }
  }

  pEncOut->busScaledLuma = vcenc_instance->asic.scaledImage.busAddress;
  pEncOut->scaledPicture = (u8 *)vcenc_instance->asic.scaledImage.virtualAddress;
  /* Get parameter sets */
  v = (struct vps *)get_parameter_set(c, VPS_NUT, 0);
  s = (struct sps *)get_parameter_set(c, SPS_NUT, 0);
  p = (struct pps *)get_parameter_set(c, PPS_NUT, vcenc_instance->pps_id);

  bool use_ltr_cur = pEncOut->boolCurIsLongTermRef = (pEncIn->u8IdxEncodedAsLTR != 0);
  codingType = get_rps_id(vcenc_instance, pEncIn, s, &rpsInSliceHeader);
  r = (struct rps *)get_parameter_set(c, RPS,     vcenc_instance->rps_id);

  if (!v || !s || !p || !r) return VCENC_ERROR;


  /* Initialize before/after/follow poc list */
  if ((codingType == VCENC_INTRA_FRAME)&&(vcenc_instance->poc == 0)) /* IDR */
      j = 0;
  else
  {
      for (i = 0, j = 0; i < VCENC_MAX_LT_REF_FRAMES; i++)
      {
          if (pEncIn->long_term_ref_pic[i] != INVALITED_POC)
              r->long_term_ref_pic_poc[j++] = pEncIn->long_term_ref_pic[i];
      }
  }
  for (i = j; i < VCENC_MAX_LT_REF_FRAMES; i++)
      r->long_term_ref_pic_poc[i] = -1;

  init_poc_list(r, vcenc_instance->poc, use_ltr_cur, codingType, c, IS_H264(vcenc_instance->codecFormat), pEncIn->bLTR_used_by_cur);
  if(r->before_cnt + r->after_cnt + r->lt_current_cnt == 1)
    codingType = VCENC_PREDICTED_FRAME;

  /* mark unused picture, so that you can reuse those. TODO: IDR flush */
  if (ref_pic_marking(c, r))
  {
    Error(2, ERR, "RPS error: reference picture(s) not available");
    return VCENC_ERROR;
  }
  /* Get free picture (if any) and remove it from picture store */
  if (!(pic = get_picture(c, -1)))
  {
    if (!(pic = create_picture_ctrlsw(vcenc_instance, v, s, p, asic->regs.sliceSize, vcenc_instance->preProcess.lumWidthSrc, vcenc_instance->preProcess.lumHeightSrc))) return VCENC_ERROR;
  }
  create_slices_ctrlsw(pic, p, asic->regs.sliceSize);
  queue_remove(&c->picture, (struct node *)pic);

  pic->encOrderInGop = pEncIn->gopPicIdx;
  pic->h264_no_ref = HANTRO_FALSE;
  if(pic->pps != p)
    pic->pps = p;

  //store input addr
  pic->input.lum = pEncIn->busLuma;
  pic->input.cb = pEncIn->busChromaU;
  pic->input.cr = pEncIn->busChromaV;

  if (pic->picture_memeory_init == 0)
  {
    pic->recon.lum = asic->internalreconLuma[pic->picture_memeory_id].busAddress;
    pic->recon.cb = asic->internalreconChroma[pic->picture_memeory_id].busAddress;
    pic->recon.cr = pic->recon.cb + asic->regs.recon_chroma_half_size;
    pic->recon_4n_base = asic->internalreconLuma_4n[pic->picture_memeory_id].busAddress;
    pic->framectx_base = asic->internalAv1FrameContext[pic->picture_memeory_id].busAddress;
    pic->mc_sync_addr = asic->internalreconLuma[pic->picture_memeory_id].busAddress + asic->internalreconLuma[pic->picture_memeory_id].size - 64;
    pic->mc_sync_ptr = (ptr_t)asic->internalreconLuma[pic->picture_memeory_id].virtualAddress + asic->internalreconLuma[pic->picture_memeory_id].size - 64;

    // for compress
    {
      u32 lumaTblSize = asic->regs.recon_luma_compress ?
                        (((pic->sps->width + 63) / 64) * ((pic->sps->height + 63) / 64) * 8) : 0;
      lumaTblSize = ((lumaTblSize + 15) >> 4) << 4;
      pic->recon_compress.lumaTblBase = asic->compressTbl[pic->picture_memeory_id].busAddress;
      pic->recon_compress.chromaTblBase = (pic->recon_compress.lumaTblBase + lumaTblSize);
    }

    // for H.264 collocated mb
    pic->colctbs_store = (struct h264_mb_col *)asic->colBuffer[pic->picture_memeory_id].virtualAddress;
    pic->colctbs_store_base = asic->colBuffer[pic->picture_memeory_id].busAddress;

    pic->picture_memeory_init = 1;
  }

  //cu info output
  pic->cuInfoTableBase = asic->cuInfoMem[vcenc_instance->cuInfoBufIdx].busAddress;
  pic->cuInfoDataBase = pic->cuInfoTableBase + asic->cuInfoTableSize;

  if(vcenc_instance->pass == 1)
    waitCuInfoBufPass1(vcenc_instance);
  if (asic->regs.enableOutputCuInfo)
  {
    u8 *cuInfoMem = (u8 *)asic->cuInfoMem[vcenc_instance->cuInfoBufIdx].virtualAddress;
    pEncOut->cuOutData.ctuOffset = (u32 *)cuInfoMem;
    pEncOut->cuOutData.cuData = cuInfoMem + asic->cuInfoTableSize;
  }
  else
  {
    pEncOut->cuOutData.ctuOffset = NULL;
    pEncOut->cuOutData.cuData = NULL;
  }
  vcenc_instance->cuInfoBufIdx++;
  if(vcenc_instance->cuInfoBufIdx == vcenc_instance->numCuInfoBuf)
    vcenc_instance->cuInfoBufIdx = 0;

  /* Activate reference paremater sets and reference picture set,
   * TODO: if parameter sets if id differs... */
  pic->rps = r;
  ASSERT(pic->vps == v);
  ASSERT(pic->sps == s);
  ASSERT(pic->pps == p);
  pic->poc     = vcenc_instance->poc;

  //0=INTER. 1=INTRA(IDR). 2=MVC-INTER. 3=MVC-INTER(ref mod).
  if ((vcenc_instance->gdrEnabled == 1) && (vcenc_instance->encStatus == VCENCSTAT_START_FRAME) && (vcenc_instance->gdrFirstIntraFrame == 0))
  {
    asic->regs.intraAreaTop = asic->regs.intraAreaLeft = asic->regs.intraAreaBottom =
                                asic->regs.intraAreaRight = INVALID_POS;
    asic->regs.roi1Top = asic->regs.roi1Left = asic->regs.roi1Bottom =
                           asic->regs.roi1Right = INVALID_POS;
    //asic->regs.roi1DeltaQp = 0;
    asic->regs.roi1Qp = -1;
    if (pEncIn->codingType == VCENC_INTRA_FRAME)
    {
      //vcenc_instance->gdrStart++ ;
      codingType = VCENC_PREDICTED_FRAME;
    }
    if (vcenc_instance->gdrStart)
    {
      if (vcenc_instance->gdrCount == 0)
        vcenc_instance->rateControl.sei.insertRecoveryPointMessage = ENCHW_YES;
      else
        vcenc_instance->rateControl.sei.insertRecoveryPointMessage = ENCHW_NO;
      top_pos = (vcenc_instance->gdrCount / (1 + vcenc_instance->interlaced)) * vcenc_instance->gdrAverageMBRows;
      bottom_pos = 0;
      if (vcenc_instance->gdrMBLeft)
      {
        if ((vcenc_instance->gdrCount / (1 + (i32)vcenc_instance->interlaced)) < vcenc_instance->gdrMBLeft)
        {
          top_pos += (vcenc_instance->gdrCount / (1 + (i32)vcenc_instance->interlaced));
          bottom_pos += 1;
        }
        else
        {
          top_pos += vcenc_instance->gdrMBLeft;
        }
      }
      bottom_pos += top_pos + vcenc_instance->gdrAverageMBRows;
      if (bottom_pos > ((i32)vcenc_instance->ctbPerCol - 1))
      {
        bottom_pos = vcenc_instance->ctbPerCol - 1;
      }

      asic->regs.intraAreaTop = top_pos;
      asic->regs.intraAreaLeft = 0;
      asic->regs.intraAreaBottom = bottom_pos;
      asic->regs.intraAreaRight = vcenc_instance->ctbPerRow - 1;

      //to make video quality in intra area is close to inter area.
      asic->regs.roi1Top = top_pos;
      asic->regs.roi1Left = 0;
      asic->regs.roi1Bottom = bottom_pos;
      asic->regs.roi1Right = vcenc_instance->ctbPerRow - 1;

      /*
          roi1DeltaQp from user setting, or if user not provide roi1DeltaQp, use default roi1DeltaQp=3
      */
      if(!asic->regs.roi1DeltaQp)
        asic->regs.roi1DeltaQp = 3;

      asic->regs.rcRoiEnable = 0x01;

    }

    asic->regs.roiUpdate   = 1;    /* ROI has changed from previous frame. */

  }

  if (codingType == VCENC_INTRA_FRAME)
  {
    asic->regs.frameCodingType = 1;
    asic->regs.nal_unit_type = (IS_H264(vcenc_instance->codecFormat) ? H264_IDR : IDR_W_RADL);
    if (vcenc_instance->poc > 0)
      asic->regs.nal_unit_type = (IS_H264(vcenc_instance->codecFormat) ? H264_NONIDR : TRAIL_R);
    pic->sliceInst->type = I_SLICE;
    pEncOut->codingType = VCENC_INTRA_FRAME;
  }
  else if (codingType == VCENC_PREDICTED_FRAME)
  {
    asic->regs.frameCodingType = 0;
    asic->regs.nal_unit_type = (IS_H264(vcenc_instance->codecFormat) ? H264_NONIDR : TRAIL_R);
    pic->sliceInst->type = P_SLICE;
    pEncOut->codingType = VCENC_PREDICTED_FRAME;
  }
  else if (codingType == VCENC_BIDIR_PREDICTED_FRAME)
  {
    asic->regs.frameCodingType = 2;
    asic->regs.nal_unit_type = (IS_H264(vcenc_instance->codecFormat) ? H264_NONIDR : TRAIL_R);
    pic->sliceInst->type = B_SLICE;
    pEncOut->codingType = VCENC_BIDIR_PREDICTED_FRAME;
  }

  asic->regs.hashtype = vcenc_instance->hashctx.hash_type;
  hash_getstate(&vcenc_instance->hashctx, &asic->regs.hashval, &asic->regs.hashoffset);

  /* Generate reference picture list of current picture using active
   * reference picture set */
  reference_picture_list(c, pic);

  vcenc_instance->sHwRps.u1short_term_ref_pic_set_sps_flag = rpsInSliceHeader ? 0 : 1;
  vcenc_instance->asic.regs.short_term_ref_pic_set_sps_flag = vcenc_instance->sHwRps.u1short_term_ref_pic_set_sps_flag;
  if(rpsInSliceHeader)
  {
    VCEncGenPicRefConfig(c, &sSliceRpsCfg, pic, vcenc_instance->poc);
    VCEncGenSliceHeaderRps(vcenc_instance, codingType, &sSliceRpsCfg);
  }

  pic->colctbs = NULL;
  pic->colctbs_load_base = 0;
  if(IS_H264(vcenc_instance->codecFormat) && codingType == VCENC_BIDIR_PREDICTED_FRAME) {
    pic->colctbs = pic->rpl[1][0]->colctbs_store;
    pic->colctbs_load_base = pic->rpl[1][0]->colctbs_store_base;
  }
  if (pic->sliceInst->type != I_SLICE ||((vcenc_instance->poc > 0)&&(!IS_H264(vcenc_instance->codecFormat))))// check only hevc?
  {
    int ref_cnt = r->before_cnt + r->after_cnt + r->lt_current_cnt;
    //H2V2 limit:
    //List lengh is no more than 2
    //number of active ref is no more than 1
    ASSERT(ref_cnt <= 2);
    ASSERT(pic->sliceInst->active_l0_cnt <= 1);
    ASSERT(pic->sliceInst->active_l1_cnt <= 1);
    ASSERT(ref_cnt + r->lt_follow_cnt <= VCENC_MAX_REF_FRAMES);

    asic->regs.l0_used_by_curr_pic[0] = 0;
    asic->regs.l0_used_by_curr_pic[1] = 0;
    asic->regs.l1_used_by_curr_pic[0] = 0;
    asic->regs.l1_used_by_curr_pic[1] = 0;
    asic->regs.l0_long_term_flag[0] = 0;
    asic->regs.l0_long_term_flag[1] = 0;
    asic->regs.l1_long_term_flag[0] = 0;
    asic->regs.l1_long_term_flag[1] = 0;
    asic->regs.num_long_term_pics = 0;

    for (i = 0; i < pic->sliceInst->active_l0_cnt; i ++)
      asic->regs.l0_used_by_curr_pic[i] = 1;

    for (i = 0; i < pic->sliceInst->active_l0_cnt; i++)
    {
      asic->regs.pRefPic_recon_l0[0][i] = pic->rpl[0][i]->recon.lum;
      asic->regs.pRefPic_recon_l0[1][i] = pic->rpl[0][i]->recon.cb;
      asic->regs.pRefPic_recon_l0[2][i] = pic->rpl[0][i]->recon.cr;
      asic->regs.pRefPic_recon_l0_4n[i] =  pic->rpl[0][i]->recon_4n_base;

      asic->regs.l0_delta_poc[i] = pic->rpl[0][i]->poc - pic->poc;
      asic->regs.l0_long_term_flag[i] = pic->rpl[0][i]->long_term_flag;
      asic->regs.num_long_term_pics += asic->regs.l0_long_term_flag[i];

      //compress
      asic->regs.ref_l0_luma_compressed[i] = pic->rpl[0][i]->recon_compress.lumaCompressed;
      asic->regs.ref_l0_luma_compress_tbl_base[i] = pic->rpl[0][i]->recon_compress.lumaTblBase;

      asic->regs.ref_l0_chroma_compressed[i] = pic->rpl[0][i]->recon_compress.chromaCompressed;
      asic->regs.ref_l0_chroma_compress_tbl_base[i] = pic->rpl[0][i]->recon_compress.chromaTblBase;
    }

    if (pic->sliceInst->type == B_SLICE)
    {
      for (i = 0; i < pic->sliceInst->active_l1_cnt; i ++)
        asic->regs.l1_used_by_curr_pic[i] = 1;

      for (i = 0; i < pic->sliceInst->active_l1_cnt; i++)
      {
        asic->regs.pRefPic_recon_l1[0][i] = pic->rpl[1][i]->recon.lum;
        asic->regs.pRefPic_recon_l1[1][i] = pic->rpl[1][i]->recon.cb;
        asic->regs.pRefPic_recon_l1[2][i] = pic->rpl[1][i]->recon.cr;
        asic->regs.pRefPic_recon_l1_4n[i] = pic->rpl[1][i]->recon_4n_base;

        asic->regs.l1_delta_poc[i] = pic->rpl[1][i]->poc - pic->poc;
        asic->regs.l1_long_term_flag[i] = pic->rpl[1][i]->long_term_flag;
        asic->regs.num_long_term_pics += asic->regs.l1_long_term_flag[i];

        //compress
        asic->regs.ref_l1_luma_compressed[i] = pic->rpl[1][i]->recon_compress.lumaCompressed;
        asic->regs.ref_l1_luma_compress_tbl_base[i] = pic->rpl[1][i]->recon_compress.lumaTblBase;

        asic->regs.ref_l1_chroma_compressed[i] = pic->rpl[1][i]->recon_compress.chromaCompressed;
        asic->regs.ref_l1_chroma_compress_tbl_base[i] = pic->rpl[1][i]->recon_compress.chromaTblBase;
      }
    }
    if(!IS_H264(vcenc_instance->codecFormat)) {
      for (i = pic->sliceInst->active_l0_cnt, j = 0; i < 2 && j < r->lt_follow_cnt; i++, j++) {
        asic->regs.l0_delta_poc[i] = r->lt_follow[j] - pic->poc;
        asic->regs.l0_long_term_flag[i] = 1;
      }
      for (i = pic->sliceInst->active_l1_cnt; i < 2 && j < r->lt_follow_cnt; i++, j++) {
        asic->regs.l1_delta_poc[i] = r->lt_follow[j] - pic->poc;
        asic->regs.l1_long_term_flag[i] = 1;
      }
    }
    asic->regs.num_long_term_pics += r->lt_follow_cnt;
  }
  if (IS_H264(vcenc_instance->codecFormat)) {
    asic->regs.colctbs_load_base = (ptr_t)pic->colctbs_load_base;
    asic->regs.colctbs_store_base = (ptr_t)pic->colctbs_store_base;
  }

  asic->regs.recon_luma_compress_tbl_base = pic->recon_compress.lumaTblBase;
  asic->regs.recon_chroma_compress_tbl_base = pic->recon_compress.chromaTblBase;

  asic->regs.active_l0_cnt = pic->sliceInst->active_l0_cnt;
  asic->regs.active_l1_cnt = pic->sliceInst->active_l1_cnt;
  asic->regs.active_override_flag = pic->sliceInst->active_override_flag;

  asic->regs.lists_modification_present_flag = pic->pps->lists_modification_present_flag;
  asic->regs.ref_pic_list_modification_flag_l0 = pic->sliceInst->ref_pic_list_modification_flag_l0;
  asic->regs.list_entry_l0[0] = pic->sliceInst->list_entry_l0[0];
  asic->regs.list_entry_l0[1] = pic->sliceInst->list_entry_l0[1];
  asic->regs.ref_pic_list_modification_flag_l1 = pic->sliceInst->ref_pic_list_modification_flag_l1;
  asic->regs.list_entry_l1[0] = pic->sliceInst->list_entry_l1[0];
  asic->regs.list_entry_l1[1] = pic->sliceInst->list_entry_l1[1];

  asic->regs.log2_max_pic_order_cnt_lsb = pic->sps->log2_max_pic_order_cnt_lsb;
  asic->regs.log2_max_frame_num = pic->sps->log2MaxFrameNumMinus4+4;
  asic->regs.pic_order_cnt_type = pic->sps->picOrderCntType;

  //cu info output
  asic->regs.cuInfoTableBase = pic->cuInfoTableBase;
  asic->regs.cuInfoDataBase = pic->cuInfoDataBase;

  //register dump
  asic->dumpRegister = vcenc_instance->dumpRegister;

  //write recon to ddr
  asic->regs.writeReconToDDR = vcenc_instance->writeReconToDDR;

  vcenc_instance->rateControl.hierarchial_bit_allocation_GOP_size = pEncIn->gopSize;
  /* Rate control */
  if (pic->sliceInst->type == I_SLICE)
  {
    vcenc_instance->rateControl.gopPoc = 0;
    vcenc_instance->rateControl.encoded_frame_number = 0;
  }
  else
  {
    vcenc_instance->rateControl.frameQpDelta = pEncIn->gopCurrPicConfig.QpOffset << QP_FRACTIONAL_BITS;
    vcenc_instance->rateControl.gopPoc       = pEncIn->gopCurrPicConfig.poc;
    vcenc_instance->rateControl.encoded_frame_number = pEncIn->gopPicIdx;
    if (vcenc_instance->rateControl.gopPoc > 0)
      vcenc_instance->rateControl.gopPoc -= 1;

    if (pEncIn->gopSize > 8)
    {
      vcenc_instance->rateControl.hierarchial_bit_allocation_GOP_size = 1;
      vcenc_instance->rateControl.gopPoc = 0;
      vcenc_instance->rateControl.encoded_frame_number = 0;
    }
  }

  //CTB_RC
  vcenc_instance->rateControl.ctbRcBitsMin = asic->regs.ctbBitsMin;
  vcenc_instance->rateControl.ctbRcBitsMax = asic->regs.ctbBitsMax;
  vcenc_instance->rateControl.ctbRctotalLcuBit = asic->regs.totalLcuBits;

  if (codingType == VCENC_INTRA_FRAME)
  {
    vcenc_instance->rateControl.qpMin = vcenc_instance->rateControl.qpMinI;
    vcenc_instance->rateControl.qpMax = vcenc_instance->rateControl.qpMaxI;
  }
  else
  {
    vcenc_instance->rateControl.qpMin = vcenc_instance->rateControl.qpMinPB;
    vcenc_instance->rateControl.qpMax = vcenc_instance->rateControl.qpMaxPB;
  }
  vcenc_instance->rateControl.inputSceneChange = pEncIn->sceneChange;
#ifdef CTBRC_STRENGTH
  asic->regs.ctbRcQpDeltaReverse = vcenc_instance->rateControl.ctbRcQpDeltaReverse;
  asic->regs.qpMin = vcenc_instance->rateControl.qpMin >> QP_FRACTIONAL_BITS;
  asic->regs.qpMax = vcenc_instance->rateControl.qpMax >> QP_FRACTIONAL_BITS;
  asic->regs.rcQpDeltaRange = vcenc_instance->rateControl.rcQpDeltaRange;
  asic->regs.rcBaseMBComplexity = vcenc_instance->rateControl.rcBaseMBComplexity;
#endif

  if (vcenc_instance->pass == 2)
  {
    i32 i;
    i32 nBlk = (vcenc_instance->width/8) * (vcenc_instance->height/8);
    i32 costScale = 4;
    for (i = 0; i < 4; i ++)
    {
      vcenc_instance->rateControl.pass1CurCost = vcenc_instance->cuTreeCtl.costCur * nBlk / costScale;
      vcenc_instance->rateControl.pass1AvgCost[i] = vcenc_instance->cuTreeCtl.costAvg[i] * nBlk / costScale;
      vcenc_instance->rateControl.pass1FrameNum[i]  = vcenc_instance->cuTreeCtl.FrameTypeNum[i];

      vcenc_instance->rateControl.pass1GopCost[i] = vcenc_instance->cuTreeCtl.costGop[i] * nBlk / costScale;
      vcenc_instance->rateControl.pass1GopFrameNum[i]  = vcenc_instance->cuTreeCtl.FrameNumGop[i];
    }
    vcenc_instance->rateControl.predId = getFramePredId(vcenc_instance->cuTreeCtl.curTypeChar);
  }

  asic->regs.bSkipCabacEnable = vcenc_instance->bSkipCabacEnable;
  asic->regs.inLoopDSRatio = vcenc_instance->preProcess.inLoopDSRatio;
  asic->regs.bMotionScoreEnable = vcenc_instance->bMotionScoreEnable;

  VCEncBeforePicRc(&vcenc_instance->rateControl, pEncIn->timeIncrement,
                  pic->sliceInst->type, use_ltr_cur, pic);

  asic->regs.bitsRatio    = vcenc_instance->rateControl.bitsRatio;

  asic->regs.ctbRcThrdMin = vcenc_instance->rateControl.ctbRcThrdMin;
  asic->regs.ctbRcThrdMax = vcenc_instance->rateControl.ctbRcThrdMax;
  asic->regs.rcRoiEnable |= ((vcenc_instance->rateControl.ctbRc & 3) << 2);

#if 0
  printf("/***********sw***********/\nvcenc_instance->rateControl.ctbRc=%d,asic->regs.ctbRcBitMemAddrCur=0x%x,asic->regs.ctbRcBitMemAddrPre=0x%x\n", vcenc_instance->rateControl.ctbRc, asic->regs.ctbRcBitMemAddrCur, asic->regs.ctbRcBitMemAddrPre);
  printf("asic->regs.bitsRatio=%d,asic->regs.ctbRcThrdMin=%d,asic->regs.ctbRcThrdMax=%d,asic->regs.rcRoiEnable=%d\n", asic->regs.bitsRatio, asic->regs.ctbRcThrdMin, asic->regs.ctbRcThrdMax, asic->regs.rcRoiEnable);
  printf("asic->regs.ctbBitsMin=%d,asic->regs.ctbBitsMax=%d,asic->regs.totalLcuBits=%d\n", asic->regs.ctbBitsMin, asic->regs.ctbBitsMax, asic->regs.totalLcuBits);
#endif

  vcenc_instance->strmHeaderLen = 0;

  /* time stamp updated */
  HevcUpdateSeiTS(&vcenc_instance->rateControl.sei, pEncIn->timeIncrement);
  /* Rate control may choose to skip the frame */
  if (vcenc_instance->rateControl.frameCoded == ENCHW_NO)
  {
    APITRACE("VCEncStrmEncode: OK, frame skipped");

    //pSlice->frameNum = pSlice->prevFrameNum;    /* restore frame_num */
    pEncOut->codingType = VCENC_NOTCODED_FRAME;
    queue_put(&c->picture, (struct node *)pic);

    return VCENC_FRAME_READY;
  }

  i32 POCtobeDisplayAV1Orig = vcenc_instance->av1_inst.POCtobeDisplayAV1;
  if(IS_AV1(vcenc_instance->codecFormat)){
    if (codingType == VCENC_INTRA_FRAME && (vcenc_instance->poc == 0)) {
        vcenc_instance->frameNum = 0;
    }
    else
        vcenc_instance->frameNum++;

    i32NalTmp = vcenc_instance->strmHeaderLen;
    if ((vcenc_instance->av1_inst.is_av1_TmpId == ENCHW_YES) && ( ENCHW_NO == vcenc_instance->av1_inst.show_existing_frame)){
      VCEncFindPicToDisplay(vcenc_instance, pEncIn);
      AV1RefreshPic(vcenc_instance, pEncIn, &vcenc_instance->strmHeaderLen);
      i32 nalsize = vcenc_instance->strmHeaderLen - i32NalTmp;
      if(nalsize > 0){
          VCEncAddNaluSize(pEncOut, nalsize);
          i32NalTmp = vcenc_instance->strmHeaderLen;
      }
    }

    if( VCENC_OK != VCEncGenAV1Config(vcenc_instance, pEncIn, pic, codingType))
        return VCENC_ERROR;
    preserve_last3_av1(vcenc_instance, c, pEncIn);
    ref_idx_markingAv1(vcenc_instance, c, pic, pEncIn->poc);

	if (IS_VP9(vcenc_instance->codecFormat)){
		//Generate vp9 config
		VCEncGenVP9Config(vcenc_instance, pEncIn, pic, codingType);
		//Reset Entropy
		if(pEncIn->bIsIDR || vcenc_instance->vp9_inst.error_resilient_mode || vcenc_instance->vp9_inst.reset_frame_context == 3)
			VCEncVP9ResetEntropy(vcenc_instance);
		//Vp9 frame header
		//Frame header
		VCEncFrameHeaderVP9(vcenc_instance, pEncIn, &vcenc_instance->strmHeaderLen, codingType);
	}else{
        u32 u32NalLen = 0;
		// sequence and frame header
    	VCEncStreamHeaderAV1(vcenc_instance, pEncIn, &vcenc_instance->strmHeaderLen, codingType, &u32NalLen);
        if(0 != u32NalLen){
            VCEncAddNaluSize(pEncOut, u32NalLen);
            i32NalTmp = vcenc_instance->strmHeaderLen;
        }
    }
    asic->regs.outputStrmBase[0] += (ptr_t)vcenc_instance->strmHeaderLen;
    asic->regs.outputStrmSize[0] -= vcenc_instance->strmHeaderLen;
}

#ifdef TEST_DATA
  EncTraceReferences(c, pic, vcenc_instance->pass);
#endif

  if (IS_H264(vcenc_instance->codecFormat)) {
    if (!h264_mmo_collect(vcenc_instance, pic, pEncIn))
      return VCENC_ERROR;


    /* use default sliding window mode for P-only ltr gap pattern for compatibility */
    if (codingType == VCENC_INTRA_FRAME && (vcenc_instance->poc == 0)) {
      vcenc_instance->frameNum = 0;
      ++vcenc_instance->idrPicId;
    }
    else if(pic->nalRefIdc)
      vcenc_instance->frameNum++;
    pic->frameNum = vcenc_instance->frameNum;

    if (pic->sliceInst->type != I_SLICE)
    {
      for (i = 0; i < pic->sliceInst->active_l0_cnt; i++)
      {
        asic->regs.l0_delta_framenum[i] = pic->frameNum - pic->rpl[0][i]->frameNum;
        asic->regs.l0_referenceLongTermIdx[i] = (pic->rpl[0][i]->long_term_flag ? pic->rpl[0][i]->curLongTermIdx : 0);
      }

      if (pic->sliceInst->type == B_SLICE)
      {
        for (i = 0; i < pic->sliceInst->active_l1_cnt; i++)
        {
          asic->regs.l1_delta_framenum[i] = pic->frameNum - pic->rpl[1][i]->frameNum;
          asic->regs.l1_referenceLongTermIdx[i] = (pic->rpl[1][i]->long_term_flag ? pic->rpl[1][i]->curLongTermIdx : 0);
        }
      }
    }
  } else {
    pic->markCurrentLongTerm = (pEncIn->u8IdxEncodedAsLTR != 0);
  }

  asic->regs.targetPicSize = vcenc_instance->rateControl.targetPicSize;

  if (asic->regs.asicCfg.ctbRcVersion && IS_CTBRC_FOR_BITRATE(vcenc_instance->rateControl.ctbRc))
  {
    // New ctb rc testing
    vcencRateControl_s *rc = &(vcenc_instance->rateControl);
    float f_tolCtbRc = (codingType == VCENC_INTRA_FRAME) ? rc->tolCtbRcIntra : rc->tolCtbRcInter;
    const i32 tolScale = 1<<TOL_CTB_RC_FIX_POINT;
    i32 tolCtbRc = (i32)(f_tolCtbRc * tolScale);
    enum slice_type idxType = pic->sliceInst->type;

    if (tolCtbRc >= 0)
    {
      i32 baseSize = rc->virtualBuffer.bitPerPic;
      if ((codingType != VCENC_INTRA_FRAME) && (rc->targetPicSize < baseSize))
        baseSize = rc->targetPicSize;

      i32 minDeltaSize = rcCalculate(baseSize, tolCtbRc, (tolScale + tolCtbRc));
      i32 maxDeltaSize = rcCalculate(baseSize, tolCtbRc, tolScale);

      asic->regs.minPicSize = MAX(0, (rc->targetPicSize - minDeltaSize));
      asic->regs.maxPicSize = rc->targetPicSize + maxDeltaSize;
    }
    else
    {
      asic->regs.minPicSize = asic->regs.maxPicSize = 0;
    }

    asic->regs.ctbRcRowFactor = rc->ctbRateCtrl.rowFactor;
    asic->regs.ctbRcQpStep = rc->ctbRateCtrl.qpStep;
    asic->regs.ctbRcMemAddrCur = rc->ctbRateCtrl.ctbMemCurAddr;
    asic->regs.ctbRcModelParamMin = rc->ctbRateCtrl.models[idxType].xMin;
    if(rc->ctbRateCtrl.models[idxType].started == 0)
    {
      if ((idxType == B_SLICE) && rc->ctbRateCtrl.models[P_SLICE].started)
        idxType = P_SLICE;
      else if (rc->ctbRateCtrl.models[I_SLICE].started)
        idxType = I_SLICE;
    }
    asic->regs.ctbRcModelParam0  = rc->ctbRateCtrl.models[idxType].x0;
    asic->regs.ctbRcModelParam1  = rc->ctbRateCtrl.models[idxType].x1;
    asic->regs.prevPicLumMad     = rc->ctbRateCtrl.models[idxType].preFrameMad;
    asic->regs.ctbRcMemAddrPre   = rc->ctbRateCtrl.models[idxType].ctbMemPreAddr;
    asic->regs.ctbRcPrevMadValid = rc->ctbRateCtrl.models[idxType].started ? 1 : 0;
    asic->regs.ctbRcDelay = IS_H264(vcenc_instance->codecFormat) ? 5 : 2;
    if (pic->sliceInst->type != I_SLICE)
    {
      i32 bpBlk = rc->targetPicSize / (rc->picArea >> 6);
      i32 bpTh = 16;
      if (bpBlk >= bpTh)
        asic->regs.ctbRcDelay = IS_H264(vcenc_instance->codecFormat)? 7 : 3;
    }
  }

  asic->regs.inputLumBase = pic->input.lum;
  asic->regs.inputCbBase = pic->input.cb;
  asic->regs.inputCrBase = pic->input.cr;

  /* setup stabilization */
  if (vcenc_instance->preProcess.videoStab)
  {
    asic->regs.stabNextLumaBase = pEncIn->busLumaStab;
  }

  asic->regs.minCbSize = pic->sps->log2_min_cb_size;
  asic->regs.maxCbSize = pic->pps->log2_ctb_size;
  asic->regs.minTrbSize = pic->sps->log2_min_tr_size;
  asic->regs.maxTrbSize = pic->pps->log2_max_tr_size;

  asic->regs.picWidth = pic->sps->width_min_cbs;
  asic->regs.picHeight = pic->sps->height_min_cbs;

  asic->regs.pps_deblocking_filter_override_enabled_flag = pic->pps->deblocking_filter_override_enabled_flag;

  /* Enable slice ready interrupts if defined by config and slices in use */
  asic->regs.sliceReadyInterrupt =
    ENCH2_SLICE_READY_INTERRUPT & ((asic->regs.sliceNum > 1));
  asic->regs.picInitQp = pic->pps->init_qp;
#ifndef CTBRC_STRENGTH
  asic->regs.qp = vcenc_instance->rateControl.qpHdr;
#else
    //printf("float qp=%f\n",vcenc_instance->rateControl.qpHdr);
    //printf("float qp=%f\n",inst->rateControl.qpHdr);
    asic->regs.qp = (vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS);
    asic->regs.qpfrac = (vcenc_instance->rateControl.qpHdr - (asic->regs.qp << QP_FRACTIONAL_BITS)) << (16 - QP_FRACTIONAL_BITS);
    //printf("float qp=%f, int qp=%d,qpfrac=0x%x\n",vcenc_instance->rateControl.qpHdr,asic->regs.qp, asic->regs.qpfrac);
    //#define BLOCK_SIZE_MIN_RC  32 , 2=16x16,1= 32x32, 0=64x64
    asic->regs.rcBlockSize = vcenc_instance->blockRCSize;
    {
        i32 qpDeltaGain;
        float strength = 1.6f;
        float qpDeltaGainFloat;

        qpDeltaGainFloat = vcenc_instance->rateControl.complexity;
        if(qpDeltaGainFloat < 14.0)
        {
            qpDeltaGainFloat = 14.0;
        }
        qpDeltaGainFloat = qpDeltaGainFloat*qpDeltaGainFloat*strength;
        qpDeltaGain = (i32)(qpDeltaGainFloat);

        asic->regs.qpDeltaMBGain = qpDeltaGain ;
    }
#endif
  if(asic->regs.asicCfg.tu32Enable && asic->regs.asicCfg.dynamicMaxTuSize && vcenc_instance->rdoLevel == 0 && asic->regs.qp <= 30 && !IS_AV1(vcenc_instance->codecFormat))
    asic->regs.current_max_tu_size_decrease = 1;
  else
    asic->regs.current_max_tu_size_decrease = 0;

  asic->regs.qpMax = vcenc_instance->rateControl.qpMax >> QP_FRACTIONAL_BITS;
  asic->regs.qpMin = vcenc_instance->rateControl.qpMin >> QP_FRACTIONAL_BITS;

  pic->pps->qp = asic->regs.qp;
  pic->pps->qpMin = asic->regs.qpMin;
  pic->pps->qpMax = asic->regs.qpMax;

  asic->regs.diffCuQpDeltaDepth = pic->pps->diff_cu_qp_delta_depth;

  asic->regs.cbQpOffset = pic->pps->cb_qp_offset;
  asic->regs.saoEnable = pic->sps->sao_enabled_flag;
  asic->regs.maxTransHierarchyDepthInter = pic->sps->max_tr_hierarchy_depth_inter;
  asic->regs.maxTransHierarchyDepthIntra = pic->sps->max_tr_hierarchy_depth_intra;

  asic->regs.cuQpDeltaEnabled = pic->pps->cu_qp_delta_enabled_flag && ((!IS_AV1(vcenc_instance->codecFormat)) || (quantizer_to_qindex[vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS] > 0));
  asic->regs.log2ParellelMergeLevel = pic->pps->log2_parallel_merge_level;
  if(rpsInSliceHeader || IS_H264(vcenc_instance->codecFormat))
  {
    asic->regs.numShortTermRefPicSets = 0;
  }
  else
  {
    asic->regs.numShortTermRefPicSets = pic->sps->num_short_term_ref_pic_sets;
  }
  asic->regs.long_term_ref_pics_present_flag = pic->sps->long_term_ref_pics_present_flag;

  asic->regs.reconLumBase = pic->recon.lum;
  asic->regs.reconCbBase = pic->recon.cb;
  asic->regs.reconCrBase = pic->recon.cr;

  asic->regs.reconL4nBase = pic->recon_4n_base;

  /* Map the reset of instance parameters */
  asic->regs.poc = pic->poc;
  asic->regs.frameNum = pic->frameNum;
  asic->regs.idrPicId = (codingType == VCENC_INTRA_FRAME ? (vcenc_instance->idrPicId & 1) : 0);
  asic->regs.nalRefIdc = pic->nalRefIdc;
  asic->regs.nalRefIdc_2bit = pic->nalRefIdc_2bit;
  asic->regs.markCurrentLongTerm = pic->markCurrentLongTerm;
  asic->regs.currentLongTermIdx = pic->curLongTermIdx;
  asic->regs.transform8x8Enable = pic->pps->transform8x8Mode;
  asic->regs.entropy_coding_mode_flag = pic->pps->entropy_coding_mode_flag;
  if(rpsInSliceHeader || IS_H264(vcenc_instance->codecFormat))
  {
    asic->regs.rpsId = 0;
  }
  else
  {
    asic->regs.rpsId = pic->rps->ps.id;
  }
  asic->regs.numNegativePics = pic->rps->num_negative_pics;
  asic->regs.numPositivePics = pic->rps->num_positive_pics;
  /* H.264 MMO */
  asic->regs.l0_used_by_next_pic[0] = 1;
  asic->regs.l0_used_by_next_pic[1] = 1;
  asic->regs.l1_used_by_next_pic[0] = 1;
  asic->regs.l1_used_by_next_pic[1] = 1;
  if(IS_H264(vcenc_instance->codecFormat) && pic->nalRefIdc && vcenc_instance->h264_mmo_nops > 0) {
    int cnt[2] = {pic->sliceInst->active_l0_cnt, pic->sliceInst->active_l1_cnt};
#if 1 // passing all unreferenced frame to hw
    for(i = 0; i < vcenc_instance->h264_mmo_nops; i++)
      h264_mmo_mark_unref(&asic->regs, vcenc_instance->h264_mmo_unref[i], vcenc_instance->h264_mmo_long_term_flag[i], vcenc_instance->h264_mmo_ltIdx[i], cnt, pic);
    vcenc_instance->h264_mmo_nops = 0;
#else // passing at most 1 unreferenced frame to hw
    h264_mmo_mark_unref(&asic->regs, vcenc_instance->h264_mmo_unref[--vcenc_instance->h264_mmo_nops], cnt, pic);
#endif
  }

  asic->regs.pps_id = pic->pps->ps.id;

  asic->regs.filterDisable = vcenc_instance->disableDeblocking;
  asic->regs.tc_Offset = vcenc_instance->tc_Offset * 2;
  asic->regs.beta_Offset = vcenc_instance->beta_Offset * 2;

  /* strong_intra_smoothing_enabled_flag */
  asic->regs.strong_intra_smoothing_enabled_flag = pic->sps->strong_intra_smoothing_enabled_flag;

  /* constrained_intra_pred_flag */
  asic->regs.constrained_intra_pred_flag = pic->pps->constrained_intra_pred_flag;

  /* skip frame flag */
  asic->regs.skip_frame_enabled = bSkipFrame;

  /*  update rdoLevel from instance per-frame:
    *  if skip_frame_enabled, config rdoLevel=0
    *  otherwise restore instance's rdoLevel setting
    */
  asic->regs.rdoLevel = vcenc_instance->rdoLevel;
  if(asic->regs.skip_frame_enabled)
   asic->regs.rdoLevel =0;

  /* set rdoLevel=0 when HW not support progRdo */
  if(!asic->regs.asicCfg.progRdoEnable)
    asic->regs.rdoLevel =0;

  /* HW base address for NAL unit sizes is affected by start offset
   * and SW created NALUs. */
  asic->regs.sizeTblBase = asic->sizeTbl[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum].busAddress;
  asic->regs.compress_coeff_scan_base = asic->compress_coeff_SACN[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum].busAddress;
  /*clip qp*/
#ifdef CTBRC_STRENGTH
  asic->regs.offsetSliceQp = 0;
  if( asic->regs.qp >= 35)
  {
  	asic->regs.offsetSliceQp = 35 - asic->regs.qp;
  }
  if(asic->regs.qp <= 15)
  {
  	asic->regs.offsetSliceQp = 15 - asic->regs.qp;
  }

  if((vcenc_instance->asic.regs.rcRoiEnable&0x0c) == 0)
  {
    if(vcenc_instance->asic.regs.rcRoiEnable &0x3)
    {
      if (asic->regs.asicCfg.roiAbsQpSupport)
      {
        i32 minDelta = (i32)asic->regs.qp - 51;

        asic->regs.roi1DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi1DeltaQp);
        asic->regs.roi2DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi2DeltaQp);

        if (asic->regs.roi1Qp >= 0)
          asic->regs.roi1Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi1Qp);

        if (asic->regs.roi2Qp >= 0)
          asic->regs.roi2Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi2Qp);

        if (asic->regs.asicCfg.ROI8Support)
        {
            asic->regs.roi3DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi3DeltaQp);
            asic->regs.roi4DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi4DeltaQp);
            asic->regs.roi5DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi5DeltaQp);
            asic->regs.roi6DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi6DeltaQp);
            asic->regs.roi7DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi7DeltaQp);
            asic->regs.roi8DeltaQp = CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi8DeltaQp);

            if (asic->regs.roi3Qp >= 0)
                asic->regs.roi3Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi3Qp);

            if (asic->regs.roi4Qp >= 0)
                asic->regs.roi4Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi4Qp);

            if (asic->regs.roi5Qp >= 0)
                asic->regs.roi5Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi5Qp);

            if (asic->regs.roi6Qp >= 0)
                asic->regs.roi6Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi6Qp);

            if (asic->regs.roi7Qp >= 0)
                asic->regs.roi7Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi7Qp);

            if (asic->regs.roi8Qp >= 0)
                asic->regs.roi8Qp = CLIP3((i32)asic->regs.qpMin, (i32)asic->regs.qpMax, asic->regs.roi8Qp);
        }
      }
      else
      {
        asic->regs.roi1DeltaQp = CLIP3(0 ,15 - asic->regs.offsetSliceQp,asic->regs.roi1DeltaQp);
        asic->regs.roi2DeltaQp = CLIP3(0 ,15 - asic->regs.offsetSliceQp,asic->regs.roi2DeltaQp);
      }

      if(((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi1DeltaQp)
      {
        asic->regs.roi1DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
      }

      if(((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi2DeltaQp)
      {
        asic->regs.roi2DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
      }

      if (asic->regs.asicCfg.ROI8Support)
      {
          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi3DeltaQp)
          {
              asic->regs.roi3DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }

          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi4DeltaQp)
          {
              asic->regs.roi4DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }

          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi5DeltaQp)
          {
              asic->regs.roi5DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }

          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi6DeltaQp)
          {
              asic->regs.roi6DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }

          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi7DeltaQp)
          {
              asic->regs.roi7DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }

          if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) < asic->regs.roi8DeltaQp)
          {
              asic->regs.roi8DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
          }
      }
    }
  }
#endif

  /* HW Base must be 64-bit aligned */
  ASSERT(asic->regs.sizeTblBase % 8 == 0);

  /*inter prediction parameters*/
  double dQPFactor = 0.387298335; // sqrt(0.15);
  bool depth0 = HANTRO_TRUE;

  asic->regs.nuh_temporal_id = 0;

  VCEncExtParaIn* pExtParaQ = NULL;
  if(vcenc_instance->pass == 2)
  {
      pExtParaQ = (VCEncExtParaIn*)queue_get(&vcenc_instance->extraParaQ);//this queue will be empty after flush.
  }

  if (pic->sliceInst->type != I_SLICE)
  {
    dQPFactor = pEncIn->gopCurrPicConfig.QpFactor;
    depth0 = (pEncIn->gopCurrPicConfig.poc % pEncIn->gopSize) ? HANTRO_FALSE : HANTRO_TRUE;
    asic->regs.nuh_temporal_id = pEncIn->gopCurrPicConfig.temporalId;
    if(useExtFlag && vcenc_instance->pass == 0)
      asic->regs.nuh_temporal_id = pEncExtParaIn->recon.temporalId;
    else if(useExtFlag && vcenc_instance->pass == 2)
      asic->regs.nuh_temporal_id = pExtParaQ->recon.temporalId;
    //HEVC support temporal svc
    if((asic->regs.nuh_temporal_id) && (!IS_H264(vcenc_instance->codecFormat)))
      asic->regs.nal_unit_type = TSA_R;
  }

  //flag for HW to code PrefixNAL
  asic->regs.prefixnal_svc_ext = (IS_H264(vcenc_instance->codecFormat) && (s->max_num_sub_layers > 1));

  VCEncSetQuantParameters(asic, dQPFactor, pic->sliceInst->type, depth0, vcenc_instance->rateControl.ctbRc);

  asic->regs.skip_chroma_dc_threadhold = 2;

  /*tuning on intra cu cost bias*/
  asic->regs.bits_est_bias_intra_cu_8 = 22; //25;
  asic->regs.bits_est_bias_intra_cu_16 = 40; //48;
  asic->regs.bits_est_bias_intra_cu_32 = 86; //108;
  asic->regs.bits_est_bias_intra_cu_64 = 38 * 8; //48*8;

  asic->regs.bits_est_1n_cu_penalty = 5;
  asic->regs.bits_est_tu_split_penalty = 3;
  asic->regs.inter_skip_bias = 124;

  /*small resolution, smaller CU/TU prefered*/
  if (pic->sps->width <= 832 && pic->sps->height <= 480)
  {
    asic->regs.bits_est_1n_cu_penalty = 0;
    asic->regs.bits_est_tu_split_penalty = 2;
  }

  if (IS_H264(vcenc_instance->codecFormat))
    asic->regs.bits_est_1n_cu_penalty = 15;

  // scaling list enable
  asic->regs.scaling_list_enabled_flag = pic->sps->scaling_list_enable_flag;

  /*stream multi-segment*/
  asic->regs.streamMultiSegEn = vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0;
  asic->regs.streamMultiSegSWSyncEn = vcenc_instance->streamMultiSegment.streamMultiSegmentMode == 2;
  asic->regs.streamMultiSegRD = 0;
  asic->regs.streamMultiSegIRQEn = 0;
  vcenc_instance->streamMultiSegment.rdCnt = 0;
  if (asic->regs.streamMultiSegEn)
  {
    //make sure the stream size is equal to N*segment size
    asic->regs.streamMultiSegSize = asic->regs.outputStrmSize[0]/vcenc_instance->streamMultiSegment.streamMultiSegmentAmount;
    asic->regs.streamMultiSegSize = ((asic->regs.streamMultiSegSize + 16 - 1) & (~(16 - 1)));//segment size must be aligned to 16byte
    asic->regs.outputStrmSize[0] = asic->regs.streamMultiSegSize*vcenc_instance->streamMultiSegment.streamMultiSegmentAmount;
    asic->regs.streamMultiSegIRQEn = 1;
    printf("segment size = %d\n",asic->regs.streamMultiSegSize);
  }

  /* smart */
  asic->regs.smartModeEnable = vcenc_instance->smartModeEnable;
  asic->regs.smartH264LumDcTh = vcenc_instance->smartH264LumDcTh;
  asic->regs.smartH264CbDcTh = vcenc_instance->smartH264CbDcTh;
  asic->regs.smartH264CrDcTh = vcenc_instance->smartH264CrDcTh;
  for(i = 0; i < 3; i++) {
    asic->regs.smartHevcLumDcTh[i] = vcenc_instance->smartHevcLumDcTh[i];
    asic->regs.smartHevcChrDcTh[i] = vcenc_instance->smartHevcChrDcTh[i];
    asic->regs.smartHevcLumAcNumTh[i] = vcenc_instance->smartHevcLumAcNumTh[i];
    asic->regs.smartHevcChrAcNumTh[i] = vcenc_instance->smartHevcChrAcNumTh[i];
  }
  asic->regs.smartH264Qp = vcenc_instance->smartH264Qp;
  asic->regs.smartHevcLumQp = vcenc_instance->smartHevcLumQp;
  asic->regs.smartHevcChrQp = vcenc_instance->smartHevcChrQp;
  for(i = 0; i < 4; i++)
    asic->regs.smartMeanTh[i] = vcenc_instance->smartMeanTh[i];
  asic->regs.smartPixNumCntTh = vcenc_instance->smartPixNumCntTh;

  /* Tile */
  asic->regs.tiles_enabled_flag = vcenc_instance->tiles_enabled_flag;
  asic->regs.num_tile_columns = vcenc_instance->num_tile_columns;
  asic->regs.num_tile_rows = vcenc_instance->num_tile_rows;
  asic->regs.loop_filter_across_tiles_enabled_flag = vcenc_instance->loop_filter_across_tiles_enabled_flag;

  /* Global MV */
  asic->regs.gmv[0][0] = asic->regs.gmv[0][1] =
  asic->regs.gmv[1][0] = asic->regs.gmv[1][1] = 0;
  if (pic->sliceInst->type != I_SLICE)
  {
    asic->regs.gmv[0][0] = pEncIn->gmv[0][0];
    asic->regs.gmv[0][1] = pEncIn->gmv[0][1];
  }
  if (pic->sliceInst->type == B_SLICE)
  {
    asic->regs.gmv[1][0] = pEncIn->gmv[1][0];
    asic->regs.gmv[1][1] = pEncIn->gmv[1][1];
  }
  /* Check GMV */
  if (asic->regs.asicCfg.gmvSupport)
  {
    i16 maxX, maxY;
    getGMVRange (&maxX, &maxY, 0, IS_H264(vcenc_instance->codecFormat), pic->sliceInst->type == B_SLICE);

    if ((asic->regs.gmv[0][0] > maxX) || (asic->regs.gmv[0][0] < -maxX) ||
        (asic->regs.gmv[0][1] > maxY) || (asic->regs.gmv[0][1] < -maxY) ||
        (asic->regs.gmv[1][0] > maxX) || (asic->regs.gmv[1][0] < -maxX) ||
        (asic->regs.gmv[1][1] > maxY) || (asic->regs.gmv[1][1] < -maxY))
    {
      asic->regs.gmv[0][0] = CLIP3(-maxX,maxX,asic->regs.gmv[0][0]);
      asic->regs.gmv[0][1] = CLIP3(-maxY,maxY,asic->regs.gmv[0][1]);
      asic->regs.gmv[1][0] = CLIP3(-maxX,maxX,asic->regs.gmv[1][0]);
      asic->regs.gmv[1][1] = CLIP3(-maxY,maxY,asic->regs.gmv[1][1]);
      APITRACEERR("VCEncStrmEncode: Global MV out of valid range");
      APIPRINT(1, "VCEncStrmEncode: Clip Global MV to valid range: (%d, %d) for list0 and (%d, %d) for list1.\n",
        asic->regs.gmv[0][0],asic->regs.gmv[0][1],asic->regs.gmv[1][0],asic->regs.gmv[1][1]);
    }

    if (asic->regs.gmv[0][0] || asic->regs.gmv[0][1] || asic->regs.gmv[1][0] || asic->regs.gmv[1][1])
    {
      if ((pic->sps->width < 320) || ((pic->sps->width * pic->sps->height) < (320 * 256)))
      {
        asic->regs.gmv[0][0] = asic->regs.gmv[0][1] = asic->regs.gmv[1][0] = asic->regs.gmv[1][1] = 0;
        APITRACEERR("VCEncStrmEncode: Video size is too small to support Global MV, reset Global MV zero");
      }
    }
  }

  /* multi-core sync */
  asic->regs.mc_sync_enable = (vcenc_instance->parallelCoreNum > 1);
  if(asic->regs.mc_sync_enable) {
    asic->regs.mc_sync_l0_addr = (codingType == VCENC_INTRA_FRAME ? 0 : pic->rpl[0][0]->mc_sync_addr);
    asic->regs.mc_sync_l1_addr = (codingType != VCENC_BIDIR_PREDICTED_FRAME ? 0 : pic->rpl[1][0]->mc_sync_addr);
    asic->regs.mc_sync_rec_addr = pic->mc_sync_addr;
    asic->regs.mc_ref_ready_threshold = 0;
    asic->regs.mc_ddr_polling_interval = (IS_H264(vcenc_instance->codecFormat) ? 32 : 128)*pic->pps->ctb_per_row;
    /* clear sync address */
    *(int *)pic->mc_sync_ptr = 0;
  }

  //set/clear picture compress flag
  pic->recon_compress.lumaCompressed = asic->regs.recon_luma_compress ? 1 : 0;
  pic->recon_compress.chromaCompressed = asic->regs.recon_chroma_compress ? 1 : 0;

  VCEncHEVCDnfPrepare(vcenc_instance);

#ifdef INTERNAL_TEST
  /* Configure the ASIC penalties according to the test vector */
  HevcConfigureTestBeforeStart(vcenc_instance);
#endif

  vcenc_instance->preProcess.bottomField = (vcenc_instance->poc % 2) == vcenc_instance->fieldOrder;
  HevcUpdateSeiPS(&vcenc_instance->rateControl.sei, vcenc_instance->preProcess.interlacedFrame, vcenc_instance->preProcess.bottomField);

  /* overlay regs */
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    //Offsets are set for after cropping, so we don't need to adjust them
    asic->regs.overlayYAddr[i] = pEncIn->overlayInputYAddr[i];
    asic->regs.overlayUAddr[i] = pEncIn->overlayInputUAddr[i];
    asic->regs.overlayVAddr[i] = pEncIn->overlayInputVAddr[i];
    //asic->regs.overlayEnable[i] = vcenc_instance->preProcess.overlayEnable[i];
    asic->regs.overlayEnable[i] = (vcenc_instance->pass == 1)?0 : pEncIn->overlayEnable[i];
    asic->regs.overlayFormat[i] = vcenc_instance->preProcess.overlayFormat[i];
    asic->regs.overlayAlpha[i] = vcenc_instance->preProcess.overlayAlpha[i];
    asic->regs.overlayXoffset[i] = vcenc_instance->preProcess.overlayXoffset[i];
    asic->regs.overlayYoffset[i] = vcenc_instance->preProcess.overlayYoffset[i];
    asic->regs.overlayWidth[i] = vcenc_instance->preProcess.overlayWidth[i];
    asic->regs.overlayHeight[i] = vcenc_instance->preProcess.overlayHeight[i];
    asic->regs.overlayYStride[i] = vcenc_instance->preProcess.overlayYStride[i];
    asic->regs.overlayUVStride[i] = vcenc_instance->preProcess.overlayUVStride[i];
    asic->regs.overlayBitmapY[i] = vcenc_instance->preProcess.overlayBitmapY[i];
    asic->regs.overlayBitmapU[i] = vcenc_instance->preProcess.overlayBitmapU[i];
    asic->regs.overlayBitmapV[i] = vcenc_instance->preProcess.overlayBitmapV[i];
  }

  EncPreProcess(asic, &vcenc_instance->preProcess);

  /* SEI message */
  if (IS_HEVC(vcenc_instance->codecFormat))
  {
    sei_s *sei = &vcenc_instance->rateControl.sei;

    if (sei->enabled == ENCHW_YES || sei->userDataEnabled == ENCHW_YES || sei->insertRecoveryPointMessage == ENCHW_YES)
    {
      if (sei->activated_sps == 0)
      {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, sei->byteStream);
        HevcActiveParameterSetsSei(&vcenc_instance->stream, sei, &s->vui);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf(" activated_sps sei size=%d\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
        sei->activated_sps = 1;
      }
      if (sei->enabled == ENCHW_YES)
      {
        if ((pic->sliceInst->type == I_SLICE) && (sei->hrd == ENCHW_YES))
        {
          tmp = vcenc_instance->stream.byteCnt;
          HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, sei->byteStream);
          HevcBufferingSei(&vcenc_instance->stream, sei, &s->vui);
          rbsp_trailing_bits(&vcenc_instance->stream);

          sei->nalUnitSize = vcenc_instance->stream.byteCnt;
          printf("BufferingSei sei size=%d\n", vcenc_instance->stream.byteCnt);
          //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
          VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
        }
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, sei->byteStream);
        HevcPicTimingSei(&vcenc_instance->stream, sei, &s->vui);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("PicTiming sei size=%d\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
      }
      if (sei->userDataEnabled == ENCHW_YES)
      {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, sei->byteStream);
        HevcUserDataUnregSei(&vcenc_instance->stream, sei);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("UserDataUnreg sei size=%d\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
      }
      if (sei->insertRecoveryPointMessage == ENCHW_YES)
      {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, sei->byteStream);
        HevcRecoveryPointSei(&vcenc_instance->stream, sei);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("RecoveryPoint sei size=%d\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
      }
    }
  }
  else if (IS_H264(vcenc_instance->codecFormat))
  {
    sei_s *sei = &vcenc_instance->rateControl.sei;
    if(sei->enabled == ENCHW_YES || sei->userDataEnabled == ENCHW_YES || sei->insertRecoveryPointMessage == ENCHW_YES)
    {
      tmp = vcenc_instance->stream.byteCnt;
      H264NalUnitHdr(&vcenc_instance->stream, 0, H264_SEI, sei->byteStream);
      if(sei->enabled == ENCHW_YES)
      {
          if((pic->sliceInst->type == I_SLICE) && (sei->hrd == ENCHW_YES))
          {
          	H264BufferingSei(&vcenc_instance->stream, sei);
          	printf("H264BufferingSei, ");
          }
          H264PicTimingSei(&vcenc_instance->stream, sei);
          printf("PicTiming, ");
      }
      if(sei->userDataEnabled == ENCHW_YES)
      {
      	H264UserDataUnregSei(&vcenc_instance->stream, sei);
      	printf("UserDataUnreg, ");
      }
      if (sei->insertRecoveryPointMessage == ENCHW_YES)
      {
      	H264RecoveryPointSei(&vcenc_instance->stream, sei);
      	printf("RecoveryPoint, ");
      }
      rbsp_trailing_bits(&vcenc_instance->stream);
      sei->nalUnitSize = vcenc_instance->stream.byteCnt;
      printf("sei total size=%d\n", vcenc_instance->stream.byteCnt);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp);
    }
  }
  vcenc_instance->numNalus[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum] = pEncOut->numNalus;

  /* Reset callback struct */
  slice_callback.slicesReadyPrev = 0;
  slice_callback.slicesReady = 0;
  slice_callback.sliceSizes = (u32 *) vcenc_instance->asic.sizeTbl[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum].virtualAddress;
  //slice_callback.sliceSizes += vcenc_instance->numNalus;
  slice_callback.nalUnitInfoNum = vcenc_instance->numNalus[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  slice_callback.nalUnitInfoNumPrev = 0;
  slice_callback.streamBufs = vcenc_instance->streamBufs[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  slice_callback.pAppData = vcenc_instance->pAppData[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];

  VCEncSetNewFrame(vcenc_instance);
  vcenc_instance->output_buffer_over_flow = 0;

  if(EnableCache(vcenc_instance,asic,pic)  != VCENC_OK)
  {
      printf("Encoder enable cache failed!!\n");
      return VCENC_ERROR;
  }

  //printf("reserve hw core_info =%x\n",vcenc_instance->reserve_core_info);

  /* Try to reserve the HW resource */
  if (EWLReserveHw(vcenc_instance->asic.ewl,&vcenc_instance->reserve_core_info) == EWL_ERROR)
  {
    APITRACEERR("VCEncStrmEncode: ERROR HW unavailable");
    return VCENC_HW_RESERVED;
  }

  if(useExtFlag)
  {
    if(vcenc_instance->pass == 2 && pExtParaQ != NULL)
        useExtPara(pExtParaQ, asic, IS_H264(vcenc_instance->codecFormat));
    else if(vcenc_instance->pass == 0)
        useExtPara(pEncExtParaIn, asic, IS_H264(vcenc_instance->codecFormat));
  }

  if ((!IS_H264(vcenc_instance->codecFormat)) || pic->nalRefIdc)
    pic->reference = HANTRO_TRUE;

  dec400_data.format = vcenc_instance->preProcess.inputFormat;
  dec400_data.lumWidthSrc = vcenc_instance->preProcess.lumWidthSrc;
  dec400_data.lumHeightSrc = vcenc_instance->preProcess.lumHeightSrc;
  dec400_data.input_alignment = vcenc_instance->preProcess.input_alignment;
  dec400_data.dec400LumTableBase = pEncIn->dec400LumTableBase;
  dec400_data.dec400CbTableBase = pEncIn->dec400CbTableBase;
  dec400_data.dec400CrTableBase = pEncIn->dec400CrTableBase;

  if (pEncIn->dec400Enable == 1 && VCEncEnableDec400(asic,&dec400_data) == VCENC_INVALID_ARGUMENT)
  {
    APITRACEERR("VCEncStrmEncode: DEC400 doesn't exist or format not supported");
    EWLReleaseHw(asic->ewl);
    return VCENC_INVALID_ARGUMENT;
  }

  EncAsicFrameStart(asic->ewl, &asic->regs, asic->dumpRegister);

  sw_ref_cnt_increase(pic);

  // picture instance switch for multi-core encoding
  if(vcenc_instance->parallelCoreNum > 1) {
    memcpy(&vcenc_instance->streams[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum], &vcenc_instance->stream, sizeof(struct buffer));
    vcenc_instance->pic[vcenc_instance->jobCnt % vcenc_instance->parallelCoreNum] = pic;
    if(vcenc_instance->reservedCore >= vcenc_instance->parallelCoreNum-1)
    {
      queue_put(&c->picture, (struct node *)pic);
      pic = vcenc_instance->pic[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
      queue_remove(&c->picture, (struct node *)pic);
      memcpy(&vcenc_instance->stream, &vcenc_instance->streams[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum], sizeof(struct buffer));
    }
  }
  vcenc_instance->reservedCore++;
  vcenc_instance->reservedCore = MIN(vcenc_instance->reservedCore, vcenc_instance->parallelCoreNum);
  pEncOut->numNalus = vcenc_instance->numNalus[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];

  if(vcenc_instance->reservedCore == vcenc_instance->parallelCoreNum)
    ret = VCEncStrmWaitReady(inst, pEncIn, pEncOut,  pic,  &slice_callback, c);
  else
  {
    ret =VCENC_FRAME_ENQUEUE;
    vcenc_instance->frameCnt++;  // update frame count after one frame is ready.
    vcenc_instance->jobCnt++;  // update job count after one frame is ready.
    queue_put(&c->picture, (struct node *)pic);
    return ret;
  }

  DisableCache(vcenc_instance);

  vcenc_instance->jobCnt++;  // update job count after one frame is ready.
  pEncOut->pNaluSizeBuf = (u32 *)vcenc_instance->asic.sizeTbl[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].virtualAddress;

  if (ret != VCENC_OK)
    goto out;

  if (vcenc_instance->output_buffer_over_flow == 1)
  {
    vcenc_instance->stream.byteCnt = 0;
    pEncOut->streamSize = vcenc_instance->stream.byteCnt;
    pEncOut->numNalus = 0;
    pEncOut->pNaluSizeBuf[0] = 0;
    vcenc_instance->encStatus = VCENCSTAT_START_FRAME;
    ret = VCENC_OUTPUT_BUFFER_OVERFLOW;
    /* revert frameNum update on output buffer overflow */
    if (IS_H264(vcenc_instance->codecFormat) && pic->nalRefIdc)
      vcenc_instance->frameNum--;
    APITRACEERR("VCEncStrmEncode: ERROR Output buffer too small");
    goto out;
  }

  i32 stat;
  {
#ifdef CTBRC_STRENGTH
    vcenc_instance->rateControl.rcPicComplexity = asic->regs.picComplexity;
    vcenc_instance->rateControl.complexity = (float)vcenc_instance->rateControl.rcPicComplexity * (vcenc_instance->rateControl.reciprocalOfNumBlk8);
#endif

    const enum slice_type _slctypes[3] = {P_SLICE, I_SLICE, B_SLICE};
    vcenc_instance->rateControl.sliceTypeCur = _slctypes[asic->regs.frameCodingType];
    stat = VCEncAfterPicRc(&vcenc_instance->rateControl, 0, vcenc_instance->stream.byteCnt, asic->regs.sumOfQP,asic->regs.sumOfQPNumber);

    /* After HRD overflow discard the coded frame and go back old time,
        * just like not coded frame. But if only one reference frame
        * buffer is in use we can't discard the frame unless the next frame
        * is coded as intra. */
    if (stat == VCENCRC_OVERFLOW)
    {
      vcenc_instance->stream.byteCnt = 0;

      pEncOut->numNalus = 0;
      pEncOut->pNaluSizeBuf[0] = 0;

      /* revert frameNum update on HRD overflow */
      if (IS_H264(vcenc_instance->codecFormat) && pic->nalRefIdc)
        vcenc_instance->frameNum--;
      //queue_put(&c->picture, (struct node *)pic);
      APITRACE("VCEncStrmEncode: OK, Frame discarded (HRD overflow)");
      //return VCENC_FRAME_READY;
    }
    else
      vcenc_instance->rateControl.sei.activated_sps = 1;
  }
#ifdef INTERNAL_TEST
  if(vcenc_instance->testId == TID_RFD) {
    HevcRFDTest(vcenc_instance, pic);
  }
#endif
  vcenc_instance->frameCnt++;  // update frame count after one frame is ready.

  if (vcenc_instance->gdrEnabled == 1 && stat != VCENCRC_OVERFLOW)
  {
    if (vcenc_instance->gdrFirstIntraFrame != 0)
    {
      vcenc_instance->gdrFirstIntraFrame--;
    }
    if (vcenc_instance->gdrStart)
      vcenc_instance->gdrCount++;

    if (vcenc_instance->gdrCount == (vcenc_instance->gdrDuration *(1 + (i32)vcenc_instance->interlaced)))
    {
      vcenc_instance->gdrStart--;
      vcenc_instance->gdrCount = 0;
      asic->regs.rcRoiEnable = 0x00;
    }
  }

  pEncOut->nextSceneChanged = 0;
  
#ifdef VIDEOSTAB_ENABLED
  /* Finalize video stabilization */
  if (vcenc_instance->preProcess.videoStab)
  {
    u32 no_motion;

    VSReadStabData(vcenc_instance->asic.regs.regMirror, &vcenc_instance->vsHwData);

    no_motion = VSAlgStabilize(&vcenc_instance->vsSwData, &vcenc_instance->vsHwData);
    if (no_motion)
    {
      VSAlgReset(&vcenc_instance->vsSwData);
    }

    /* update offset after stabilization */
    VSAlgGetResult(&vcenc_instance->vsSwData, &vcenc_instance->preProcess.horOffsetSrc,
                   &vcenc_instance->preProcess.verOffsetSrc);

    /* output scene change result */
    pEncOut->nextSceneChanged = vcenc_instance->vsSwData.sceneChange;
  }
#endif

  VCEncHEVCDnfUpdate(vcenc_instance);

  vcenc_instance->encStatus = VCENCSTAT_START_FRAME;
  ret = VCENC_FRAME_READY;

  APITRACE("VCEncStrmEncode: OK");

out:
  sw_ref_cnt_decrease(pic);
  vcenc_instance->reservedCore--;
  if(vcenc_instance->pass==2) {
    ReleaseLookaheadPicture(&vcenc_instance->lookahead, outframe);
    if(ret != VCENC_FRAME_READY) {
      VCEncLookahead *lookahead = &vcenc_instance->lookahead;
      VCEncLookahead *lookahead2 = &((struct vcenc_instance *)(lookahead->priv_inst))->lookahead;
      lookahead->status = lookahead2->status = ret;
      lookahead->bFlush = true;
    }
  }

  /* Put picture back to store */
  queue_put(&c->picture, (struct node *)pic);
  pEncOut->streamSize = vcenc_instance->stream.byteCnt;
  if(IS_AV1(vcenc_instance->codecFormat)){
    if(pEncOut->streamSize > 0 || vcenc_instance->pass == 1) { // No stream output in 1st pass
      pEncOut->streamSize += vcenc_instance->strmHeaderLen;
      pEncOut->numNalus--;
      VCEncAddNaluSize(pEncOut, pEncOut->streamSize - i32NalTmp);

      if(pEncIn->bIsIDR){
        for (i32 i = 0; i < SW_REF_FRAMES-1; i++)
          vcenc_instance->av1_inst.remapped_ref_idx[i] = i;

        vcenc_instance->av1_inst.is_valid_idx[SW_LAST_FRAME - SW_LAST_FRAME] = ENCHW_YES;
        for(i32 i = SW_LAST2_FRAME; i < SW_REF_FRAMES; i++)
          vcenc_instance->av1_inst.is_valid_idx[i - SW_LAST_FRAME] = ENCHW_NO;

        if(pEncIn->u8IdxEncodedAsLTR)
          vcenc_instance->av1_inst.is_valid_idx[SW_GOLDEN_FRAME - SW_LAST_FRAME] = ENCHW_YES;
      }
      else
        av1_update_reference_frames(vcenc_instance, pEncIn);

      vcenc_instance->av1_inst.is_av1_TmpId = ENCHW_YES;
      VCEncUpdateRefPicInfo(vcenc_instance, pEncIn, codingType);
    } else {
      /* frame skipped, reset POCtobeDisplayAV1 */
      vcenc_instance->av1_inst.POCtobeDisplayAV1 = POCtobeDisplayAV1Orig;
      vcenc_instance->frameNum--;
    }
  }

  /* output the statistics data of cus. */
  pEncOut->cuStatis.intraCu8Num = vcenc_instance->asic.regs.intraCu8Num;
  pEncOut->cuStatis.skipCu8Num = vcenc_instance->asic.regs.skipCu8Num;
  pEncOut->cuStatis.PBFrame4NRdCost = vcenc_instance->asic.regs.PBFrame4NRdCost;

  return ret;
}

/*------------------------------------------------------------------------------

    Function name : VCEncMultiCoreFlush
    Description   : Flush remaining frames at the end of multi-core encoding.
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/


VCEncRet VCEncMultiCoreFlush(VCEncInst inst, const VCEncIn *pEncIn,
                             VCEncOut *pEncOut,
                             VCEncSliceReadyCallBackFunc sliceReadyCbFunc)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  asicData_s *asic = &vcenc_instance->asic;
  struct container *c;
  struct sw_picture *pic;
  i32 ret = VCENC_ERROR;
  VCEncSliceReady slice_callback;
  i32 i;

  APITRACE("VCEncStrmEncode#");

  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pEncIn == NULL) || (pEncOut == NULL))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }
  /* Check status, INIT and ERROR not allowed */
  if ((vcenc_instance->encStatus != VCENCSTAT_START_STREAM) &&
      (vcenc_instance->encStatus != VCENCSTAT_START_FRAME))
  {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid status");
    return VCENC_INVALID_STATUS;
  }

  if(vcenc_instance->reservedCore == 0)
    return VCENC_OK;

  if (!(c = get_container(vcenc_instance))) return VCENC_ERROR;

  /* Reset callback struct */
  vcenc_instance->stream.byteCnt = 0;
  slice_callback.slicesReadyPrev = 0;
  slice_callback.slicesReady = 0;
  slice_callback.sliceSizes = (u32 *) vcenc_instance->asic.sizeTbl[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum].virtualAddress;
  //slice_callback.sliceSizes += vcenc_instance->numNalus;
  slice_callback.nalUnitInfoNum = vcenc_instance->numNalus[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  slice_callback.nalUnitInfoNumPrev = 0;
  slice_callback.streamBufs = vcenc_instance->streamBufs[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  slice_callback.pAppData = vcenc_instance->pAppData[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];

  // picture instance switch for multi-core encoding
  pic = vcenc_instance->pic[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  queue_remove(&c->picture, (struct node *)pic);
  pEncOut->numNalus = vcenc_instance->numNalus[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum];
  memcpy(&vcenc_instance->stream, &vcenc_instance->streams[(vcenc_instance->jobCnt+1) % vcenc_instance->parallelCoreNum], sizeof(struct buffer));

  ret = VCEncStrmWaitReady(inst, pEncIn, pEncOut,  pic,  &slice_callback, c);

  DisableCache(vcenc_instance);

  if (ret != VCENC_OK)
    goto out;

  //set/clear picture compress flag
  pic->recon_compress.lumaCompressed = asic->regs.recon_luma_compress ? 1 : 0;
  pic->recon_compress.chromaCompressed = asic->regs.recon_chroma_compress ? 1 : 0;

  if (vcenc_instance->output_buffer_over_flow == 1)
  {
    vcenc_instance->stream.byteCnt = 0;
    pEncOut->streamSize = vcenc_instance->stream.byteCnt;
    pEncOut->numNalus = 0;
    pEncOut->pNaluSizeBuf[0] = 0;
    vcenc_instance->encStatus = VCENCSTAT_START_FRAME;
    ret = VCENC_OUTPUT_BUFFER_OVERFLOW;
    /* revert frameNum update on output buffer overflow */
    if (IS_H264(vcenc_instance->codecFormat) && pic->nalRefIdc)
      vcenc_instance->frameNum--;
    APITRACEERR("VCEncStrmEncode: ERROR Output buffer too small");
    goto out;
  }

  i32 stat;
  {
#ifdef CTBRC_STRENGTH
    vcenc_instance->rateControl.rcPicComplexity = asic->regs.picComplexity;
    vcenc_instance->rateControl.complexity = (float)vcenc_instance->rateControl.rcPicComplexity * (vcenc_instance->rateControl.reciprocalOfNumBlk8);
#endif

    const enum slice_type _slctypes[3] = {P_SLICE, I_SLICE, B_SLICE};
    vcenc_instance->rateControl.sliceTypeCur = _slctypes[asic->regs.frameCodingType];
    stat = VCEncAfterPicRc(&vcenc_instance->rateControl, 0, vcenc_instance->stream.byteCnt, asic->regs.sumOfQP,asic->regs.sumOfQPNumber);

    /* After HRD overflow discard the coded frame and go back old time,
        * just like not coded frame. But if only one reference frame
        * buffer is in use we can't discard the frame unless the next frame
        * is coded as intra. */
    if (stat == VCENCRC_OVERFLOW)
    {
      vcenc_instance->stream.byteCnt = 0;

      pEncOut->numNalus = 0;
      pEncOut->pNaluSizeBuf[0] = 0;

      /* revert frameNum update on HRD overflow */
      if (IS_H264(vcenc_instance->codecFormat) && pic->nalRefIdc)
        vcenc_instance->frameNum--;
      //queue_put(&c->picture, (struct node *)pic);
      APITRACE("VCEncStrmEncode: OK, Frame discarded (HRD overflow)");
      //return VCENC_FRAME_READY;
    }
    else
      vcenc_instance->rateControl.sei.activated_sps = 1;
  }
#ifdef INTERNAL_TEST
  if(vcenc_instance->testId == TID_RFD) {
    HevcRFDTest(vcenc_instance, pic);
  }
#endif
  vcenc_instance->jobCnt++;  // update frame count after one frame is ready.
  pEncOut->pNaluSizeBuf = (u32 *)vcenc_instance->asic.sizeTbl[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].virtualAddress;

  if (vcenc_instance->gdrEnabled == 1 && stat != VCENCRC_OVERFLOW)
  {
    if (vcenc_instance->gdrFirstIntraFrame != 0)
    {
      vcenc_instance->gdrFirstIntraFrame--;
    }
    if (vcenc_instance->gdrStart)
      vcenc_instance->gdrCount++;

    if (vcenc_instance->gdrCount == (vcenc_instance->gdrDuration *(1 + (i32)vcenc_instance->interlaced)))
    {
      vcenc_instance->gdrStart--;
      vcenc_instance->gdrCount = 0;
      asic->regs.rcRoiEnable = 0x00;
    }
  }

  VCEncHEVCDnfUpdate(vcenc_instance);

  vcenc_instance->encStatus = VCENCSTAT_START_FRAME;
  ret = VCENC_FRAME_READY;

  APITRACE("VCEncStrmEncode: OK");

out:
  sw_ref_cnt_decrease(pic);
  vcenc_instance->reservedCore--;

  /* Put picture back to store */
  queue_put(&c->picture, (struct node *)pic);
  pEncOut->streamSize = vcenc_instance->stream.byteCnt;
  return ret;
}
/*------------------------------------------------------------------------------

    Function name : VCEncFlush
    Description   : Flush remaining frames at the end of multi-core/lookahead encoding.
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VCEncRet VCEncFlush(VCEncInst inst, const VCEncIn *pEncIn,
                             VCEncOut *pEncOut,
                             VCEncSliceReadyCallBackFunc sliceReadyCbFunc)
{
  VCEncRet ret = VCENC_OK;
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  if(vcenc_instance->pass == 2) {
    // Lookahead flush
    ret = VCEncStrmEncodeExt(inst,NULL,NULL,pEncOut,NULL,NULL,0);
    if(ret != VCENC_OK)
      return ret;
  }
  // Multicore flush
  if(vcenc_instance->reservedCore > 0)
    return VCEncMultiCoreFlush(inst, pEncIn, pEncOut, sliceReadyCbFunc);

  return VCENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VCEncStrmEnd
    Description   : Ends a stream
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VCEncRet VCEncStrmEnd(VCEncInst inst, const VCEncIn *pEncIn,
                          VCEncOut *pEncOut)
{

  /* Status == INIT   Stream ended, next stream can be started */
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  APITRACE("VCEncStrmEnd#");
  APITRACEPARAM_X("busLuma", pEncIn->busLuma);
  APITRACEPARAM_X("busChromaU", pEncIn->busChromaU);
  APITRACEPARAM_X("busChromaV", pEncIn->busChromaV);
  APITRACEPARAM("timeIncrement", pEncIn->timeIncrement);
  APITRACEPARAM_X("pOutBuf", pEncIn->pOutBuf[0]);
  APITRACEPARAM_X("busOutBuf", pEncIn->busOutBuf[0]);
  APITRACEPARAM("outBufSize", pEncIn->outBufSize[0]);
  APITRACEPARAM("codingType", pEncIn->codingType);
  APITRACEPARAM("poc", pEncIn->poc);
  APITRACEPARAM("gopSize", pEncIn->gopSize);
  APITRACEPARAM("gopPicIdx", pEncIn->gopPicIdx);
  APITRACEPARAM_X("roiMapDeltaQpAddr", pEncIn->roiMapDeltaQpAddr);


  /* Check for illegal inputs */
  if ((vcenc_instance == NULL) || (pEncIn == NULL) || (pEncOut == NULL))
  {
    APITRACEERR("VCEncStrmEnd: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance)
  {
    APITRACEERR("VCEncStrmEnd: ERROR Invalid instance");
    return VCENC_INSTANCE_ERROR;
  }

  /* Check status, this also makes sure that the instance is valid */
  if ((vcenc_instance->encStatus != VCENCSTAT_START_FRAME) &&
      (vcenc_instance->encStatus != VCENCSTAT_START_STREAM))
  {
    APITRACEERR("VCEncStrmEnd: ERROR Invalid status");
    return VCENC_INVALID_STATUS;
  }

  if(vcenc_instance->pass == 1) {
    vcenc_instance->stream.stream = (u8*)vcenc_instance->lookahead.internal_mem.pOutBuf;
    vcenc_instance->stream.stream_bus = vcenc_instance->lookahead.internal_mem.busOutBuf;
    vcenc_instance->stream.size = vcenc_instance->lookahead.internal_mem.outBufSize;
  } else {
    vcenc_instance->stream.stream = (u8 *)pEncIn->pOutBuf[0];
    vcenc_instance->stream.stream_bus = pEncIn->busOutBuf[0];
    vcenc_instance->stream.size = pEncIn->outBufSize[0];
  }
  vcenc_instance->stream.cnt = &vcenc_instance->stream.byteCnt;

  pEncOut->streamSize = 0;
  vcenc_instance->stream.byteCnt = 0;

  /* Set pointer to the beginning of NAL unit size buffer */
  pEncOut->pNaluSizeBuf = (u32 *) vcenc_instance->asic.sizeTbl[0].virtualAddress;
  pEncOut->numNalus = 0;

  /* Clear the NAL unit size table */
  if (pEncOut->pNaluSizeBuf != NULL)
    pEncOut->pNaluSizeBuf[0] = 0;

  /* Write end-of-stream code */
  if (IS_H264(vcenc_instance->codecFormat))
    H264EndOfSequence(&vcenc_instance->stream, vcenc_instance->asic.regs.streamMode);
  else if(IS_HEVC(vcenc_instance->codecFormat))
    HEVCEndOfSequence(&vcenc_instance->stream, vcenc_instance->asic.regs.streamMode);
  else if(IS_AV1(vcenc_instance->codecFormat))
    AV1EndOfSequence(vcenc_instance, pEncIn, pEncOut, &vcenc_instance->stream.byteCnt);
  pEncOut->streamSize = vcenc_instance->stream.byteCnt;
  if(!IS_AV1(vcenc_instance->codecFormat)){
      pEncOut->numNalus = 1;
      pEncOut->pNaluSizeBuf[0] = vcenc_instance->stream.byteCnt;
      pEncOut->pNaluSizeBuf[1] = 0;
  }

  if(vcenc_instance->pass == 1) {
    cuTreeRelease(&vcenc_instance->cuTreeCtl);

    //release pass-1 internal stream buffer
   EWLFreeLinear(vcenc_instance->asic.ewl,  &(vcenc_instance->lookahead.internal_mem.mem));
  }

  /* Status == INIT   Stream ended, next stream can be started */
  vcenc_instance->encStatus = VCENCSTAT_INIT;
  if(vcenc_instance->pass==2) {
    VCEncRet ret = TerminateLookaheadThread(&vcenc_instance->lookahead);
    if (ret != VCENC_OK) {
      APITRACE("VCEncStrmEnd: TerminateLookaheadThread failed");
      return ret;
    }
    VCEncOut encOut;
    VCEncIn encIn;
    memcpy(&encIn, pEncIn, sizeof(VCEncIn));
    encIn.gopConfig.pGopPicCfg = pEncIn->gopConfig.pGopPicCfgPass1;
    ret = VCEncStrmEnd(vcenc_instance->lookahead.priv_inst, &encIn, &encOut);
    if (ret != VCENC_OK) {
      APITRACE("VCEncStrmEnd: LookaheadStrmEnd failed");
      return ret;
    }
  }

  APITRACE("VCEncStrmEnd: OK");
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetBitsPerPixel
    Description   : Returns the amount of bits per pixel for given format.
    Return type   : u32
------------------------------------------------------------------------------*/
u32 VCEncGetBitsPerPixel(VCEncPictureType type)
{
  switch (type)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
      return 12;
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
      return 16;
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
      return 32;
    case VCENC_YUV420_PLANAR_10BIT_I010:
    case VCENC_YUV420_PLANAR_10BIT_P010:
      return 24;
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      return 15;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
      return 16;
    default:
      return 0;
  }
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetAlignedStride
    Description   : Returns the stride in byte by given aligment and format.
    Return type   : u32
------------------------------------------------------------------------------*/
u32 VCEncGetAlignedStride(int width, i32 input_format, u32 *luma_stride, u32 *chroma_stride,u32 input_alignment)
{
  return EncGetAlignedByteStride(width, input_format, luma_stride, chroma_stride, input_alignment);
}

/*------------------------------------------------------------------------------
    Function name : VCEncSetTestId
    Description   : Sets the encoder configuration according to a test vector
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : testId - test vector ID
------------------------------------------------------------------------------*/
VCEncRet VCEncSetTestId(VCEncInst inst, u32 testId)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  (void) pEncInst;
  (void) testId;

  APITRACE("VCEncSetTestId#");

#ifdef INTERNAL_TEST
  pEncInst->testId = testId;

  APITRACE("VCEncSetTestId# OK");
  return VCENC_OK;
#else
  /* Software compiled without testing support, return error always */
  APITRACEERR("VCEncSetTestId# ERROR, testing disabled at compile time");
  return VCENC_ERROR;
#endif
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetCuInfo
    Description      : Get the encoding information of a CU in a CTU
    Return type     : VCEncRet
    Argument        : inst - encoder instance
    Argument        : pEncCuOutData - structure containning ctu table and cu information stream output by HW
    Argument        : pEncCuInfo - structure returns the parsed cu information
    Argument        : ctuNum - ctu number within picture
    Argument        : cuNum - cu number within ctu
------------------------------------------------------------------------------*/
VCEncRet VCEncGetCuInfo(VCEncInst inst, VCEncCuOutData *pEncCuOutData,
                                       VCEncCuInfo *pEncCuInfo, u32 ctuNum, u32 cuNum)
{
    struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
    u32 nCuInCtu;
    u32 *ctuTable;
    u8 *cuStream;
    bits_buffer_s b;
    i32 version, infoSize;
    i32 i;

    APITRACE("VCEncGetCuInfo#");

    /* Check for illegal inputs */
    if (!pEncInst || !pEncCuOutData || !pEncCuInfo) {
    	APITRACEERR("VCEncGetCuInfo: ERROR Null argument");
    	return VCENC_INVALID_ARGUMENT;
    }

    if ((i32)ctuNum >= pEncInst->ctbPerFrame) {
    	APITRACEERR("VCEncGetCuInfo: ERROR Invalid ctu number");
    	return VCENC_INVALID_ARGUMENT;
    }

    version = pEncInst->asic.regs.asicCfg.cuInforVersion;
    infoSize = version ? CU_INFO_OUTPUT_SIZE_V1 : CU_INFO_OUTPUT_SIZE;
    ctuTable = pEncCuOutData->ctuOffset;
    cuStream = pEncCuOutData->cuData;
    if (!ctuTable || !cuStream) {
		APITRACEERR("VCEncGetCuInfo: ERROR Null argument");
		return VCENC_INVALID_ARGUMENT;
	}

    nCuInCtu = ctuTable[ctuNum];
    if (ctuNum)
    {
      nCuInCtu -= ctuTable[ctuNum-1];
      cuStream += ctuTable[ctuNum-1] * infoSize;
    }
    if(IS_H264(pEncInst->codecFormat)) {
      nCuInCtu = 1;
      cuStream = pEncCuOutData->cuData + ctuNum * infoSize;
    }
    if (cuNum >= nCuInCtu) {
		APITRACEERR("VCEncGetCuInfo: ERROR Invalid cu number");
		return VCENC_INVALID_ARGUMENT;
	}
    cuStream += cuNum * infoSize;
    b.cache = 0;
    b.bit_cnt = 0;
    b.stream = cuStream;
    b.accu_bits = 0;

    pEncCuInfo->cuLocationY = (IS_H264(pEncInst->codecFormat) ? 0 : get_value(&b, CU_INFO_LOC_Y_BITS, HANTRO_FALSE) * 8);
    pEncCuInfo->cuLocationX = (IS_H264(pEncInst->codecFormat) ? 0 : get_value(&b, CU_INFO_LOC_X_BITS, HANTRO_FALSE) * 8);
    pEncCuInfo->cuSize = (IS_H264(pEncInst->codecFormat) ? 16 : (1 << (get_value(&b, CU_INFO_SIZE_BITS, HANTRO_FALSE) + 3)));
    pEncCuInfo->cuMode = get_value(&b, CU_INFO_MODE_BITS, HANTRO_FALSE);
    pEncCuInfo->cost = get_value(&b, CU_INFO_COST_BITS, HANTRO_FALSE);
    if (pEncCuInfo->cuMode == 0)
    {
      pEncCuInfo->interPredIdc = get_value(&b, CU_INFO_INTER_PRED_IDC_BITS, HANTRO_FALSE);
      for (i = 0; i < 2; i ++)
      {
        pEncCuInfo->mv[i].refIdx = get_value(&b, CU_INFO_MV_REF_IDX_BITS, HANTRO_FALSE);
        pEncCuInfo->mv[i].mvX = get_value(&b, CU_INFO_MV_X_BITS, HANTRO_TRUE);
        pEncCuInfo->mv[i].mvY = get_value(&b, CU_INFO_MV_Y_BITS, HANTRO_TRUE);
      }
    }
    else
    {
      pEncCuInfo->intraPartMode = get_value(&b, IS_H264(pEncInst->codecFormat) ? CU_INFO_INTRA_PART_BITS_H264 : CU_INFO_INTRA_PART_BITS, HANTRO_FALSE);
      for (i = 0; i < (IS_H264(pEncInst->codecFormat) ? 16 : 4); i ++)
        pEncCuInfo->intraPredMode[i] = get_value(&b, IS_H264(pEncInst->codecFormat) ? CU_INFO_INTRA_PRED_MODE_BITS_H264 : CU_INFO_INTRA_PRED_MODE_BITS, HANTRO_FALSE);
      if(pEncCuInfo->intraPredMode[0] == (IS_H264(pEncInst->codecFormat) ? 15 : 63))
        pEncCuInfo->cuMode = 2;
    }

    if (version >= 1)
    {
      get_align(&b, CU_INFO_OUTPUT_SIZE);
      pEncCuInfo->mean = get_value(&b, CU_INFO_MEAN_BITS, HANTRO_FALSE);
      pEncCuInfo->variance = get_value(&b, CU_INFO_VAR_BITS, HANTRO_FALSE);
      pEncCuInfo->qp = get_value(&b, CU_INFO_QP_BITS, HANTRO_FALSE);
      pEncCuInfo->costOfOtherMode = get_value(&b, CU_INFO_COST_BITS, HANTRO_FALSE);
      pEncCuInfo->costIntraSatd = get_value(&b, CU_INFO_COST_BITS, HANTRO_FALSE);
      pEncCuInfo->costInterSatd = get_value(&b, CU_INFO_COST_BITS, HANTRO_FALSE);
    }

    return VCENC_OK;
}

/*------------------------------------------------------------------------------
  out_of_memory
------------------------------------------------------------------------------*/
i32 out_of_memory(struct vcenc_buffer *n, u32 size)
{
  return (size > n->cnt);
}

/*------------------------------------------------------------------------------
  init_buffer
------------------------------------------------------------------------------*/
i32 init_buffer(struct vcenc_buffer *vcenc_buffer,
                struct vcenc_instance *vcenc_instance)
{
  if (!vcenc_instance->temp_buffer) return NOK;
  vcenc_buffer->next   = NULL;
  vcenc_buffer->buffer = vcenc_instance->temp_buffer;
  vcenc_buffer->cnt    = vcenc_instance->temp_size;
  vcenc_buffer->busaddr = vcenc_instance->temp_bufferBusAddress;
  return OK;
}

/*------------------------------------------------------------------------------
  get_buffer
------------------------------------------------------------------------------*/
i32 get_buffer(struct buffer *buffer, struct vcenc_buffer *source, i32 size,
               i32 reset)
{
  struct vcenc_buffer *vcenc_buffer;

  if (size < 0) return NOK;
  memset(buffer, 0, sizeof(struct buffer));

  /* New vcenc_buffer node */
  if (out_of_memory(source, sizeof(struct vcenc_buffer))) return NOK;
  vcenc_buffer = (struct vcenc_buffer *)source->buffer;
  if (reset) memset(vcenc_buffer, 0, sizeof(struct vcenc_buffer));
  source->buffer += sizeof(struct vcenc_buffer);
  source->busaddr += sizeof(struct vcenc_buffer);
  source->cnt    -= sizeof(struct vcenc_buffer);

  /* Connect previous vcenc_buffer node */
  if (source->next) source->next->next = vcenc_buffer;
  source->next = vcenc_buffer;

  /* Map internal buffer and user space vcenc_buffer, NOTE that my chunk
   * size is sizeof(struct vcenc_buffer) */
  size = (size / sizeof(struct vcenc_buffer)) * sizeof(struct vcenc_buffer);
  if (out_of_memory(source, size)) return NOK;
  vcenc_buffer->buffer = source->buffer;
  vcenc_buffer->busaddr = source->busaddr;
  buffer->stream      = source->buffer;
  buffer->stream_bus  = source->busaddr;
  buffer->size        = size;
  buffer->cnt         = &vcenc_buffer->cnt;
  source->buffer     += size;
  source->busaddr    += size;
  source->cnt        -= size;

  return OK;
}

/*------------------------------------------------------------------------------
  vcenc_set_ref_pic_set
------------------------------------------------------------------------------*/
i32 vcenc_set_ref_pic_set(struct vcenc_instance *vcenc_instance)
{
  struct container *c;
  struct vcenc_buffer source;
  struct rps *r;

  if (!(c = get_container(vcenc_instance))) return NOK;

  /* Create new reference picture set */
  if (!(r = (struct rps *)create_parameter_set(RPS))) return NOK;

  /* We will read vcenc_instance->buffer */
  if (init_buffer(&source, vcenc_instance)) goto out;
  if (get_buffer(&r->ps.b, &source, PRM_SET_BUFF_SIZE, HANTRO_FALSE)) goto out;

  /* Initialize parameter set id */
  r->ps.id = vcenc_instance->rps_id;
  r->sps_id = vcenc_instance->sps_id;

  if (set_reference_pic_set(r)) goto out;

  /* Replace old reference picture set with new one */
  remove_parameter_set(c, RPS, vcenc_instance->rps_id);
  queue_put(&c->parameter_set, (struct node *)r);

  return OK;

out:
  free_parameter_set((struct ps *)r);
  return NOK;
}


/*------------------------------------------------------------------------------
  vcenc_ref_pic_sets
------------------------------------------------------------------------------*/
i32 vcenc_ref_pic_sets(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn)
{
  struct vcenc_buffer *vcenc_buffer;
  const VCEncGopConfig *gopCfg;
  i32 *poc;
  i32 rps_id = 0;

  /* Initialize tb->instance->buffer so that we can cast it to vcenc_buffer
   * by calling hevc_set_parameter(), see api.c TODO...*/
  vcenc_instance->rps_id = -1;
  vcenc_set_ref_pic_set(vcenc_instance);
  vcenc_buffer = (struct vcenc_buffer *)vcenc_instance->temp_buffer;
  poc = (i32 *)vcenc_buffer->buffer;

  gopCfg = &pEncIn->gopConfig;

  //RPS based on user GOP configuration
  int i, j;
  rps_id = 0;

  for (i = 0; i < gopCfg->size; i ++)
  {
    VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
    u32 iRefPic, idx = 0;
    for (iRefPic = 0; iRefPic < cfg->numRefPics; iRefPic ++)
    {
        if (IS_LONG_TERM_REF_DELTAPOC(cfg->refPics[iRefPic].ref_pic))
            continue;
        poc[idx++] = cfg->refPics[iRefPic].ref_pic;
        poc[idx++] = cfg->refPics[iRefPic].used_by_cur;
    }
    for (j = 0; j <gopCfg->ltrcnt; j++)
    {
        poc[idx++] = gopCfg->u32LTR_idx[j];
        poc[idx++] = 0;
    }
    poc[idx] = 0; //end
    vcenc_instance->rps_id = rps_id;
    if (vcenc_set_ref_pic_set(vcenc_instance))
    {
      Error(2, ERR, "vcenc_set_ref_pic_set() fails");
      return NOK;
    }
    rps_id++;
  }

  if (0 != gopCfg->special_size) {//
      for (i = 0; i < gopCfg->special_size; i++)
      {
          VCEncGopPicSpecialConfig *cfg = &(gopCfg->pGopPicSpecialCfg[i]);
          u32 iRefPic, idx = 0;
          if (((i32)cfg->numRefPics) != NUMREFPICS_RESERVED)
          {
              for (iRefPic = 0; iRefPic < cfg->numRefPics; iRefPic++)
              {
                  if (IS_LONG_TERM_REF_DELTAPOC(cfg->refPics[iRefPic].ref_pic))
                      continue;
                  poc[idx++] = cfg->refPics[iRefPic].ref_pic;
                  poc[idx++] = cfg->refPics[iRefPic].used_by_cur;
              }
          }

          // add LTR idx to default GOP setting auto loaded
          for (j = 0; j < gopCfg->ltrcnt; j++)
          {
              poc[idx++] = gopCfg->u32LTR_idx[j];
              poc[idx++] = 0;
          }
          poc[idx] = 0; //end
          vcenc_instance->rps_id = rps_id;
          if (vcenc_set_ref_pic_set(vcenc_instance))
          {
              Error(2, ERR, "vcenc_set_ref_pic_set() fails");
              return NOK;
          }
          rps_id++;
      }
  }

  //add some special RPS configuration
  //P frame for sequence starting
  poc[0] = -1;
  poc[1] = 1;
  for (i = 1; i <gopCfg->ltrcnt; i++)
  {
      poc[2 * i] = gopCfg->u32LTR_idx[i-1];
      poc[2 * i + 1] = 0;
  }
  poc[2 * i] = 0;

  vcenc_instance->rps_id = rps_id;
  if (vcenc_set_ref_pic_set(vcenc_instance))
  {
    Error(2, ERR, "vcenc_set_ref_pic_set() fails");
    return NOK;
  }
  rps_id++;
  //Intra
  for (i  = 0; i <gopCfg->ltrcnt; i++)
  {
      poc[2*i]     = gopCfg->u32LTR_idx[i];
      poc[2 * i+1] = 0;
  }
  poc[2 * i] = 0;

  vcenc_instance->rps_id = rps_id;
  if (vcenc_set_ref_pic_set(vcenc_instance))
  {
    Error(2, ERR, "vcenc_set_ref_pic_set() fails");
    return NOK;
  }
  return OK;
}

/*------------------------------------------------------------------------------
  vcenc_get_ref_pic_set
------------------------------------------------------------------------------*/
i32 vcenc_get_ref_pic_set(struct vcenc_instance *vcenc_instance)
{
  struct container *c;
  struct vcenc_buffer source;
  struct ps *p;

  if (!(c = get_container(vcenc_instance))) return NOK;
  if (!(p = get_parameter_set(c, RPS, vcenc_instance->rps_id))) return NOK;
  if (init_buffer(&source, vcenc_instance)) return NOK;
  if (get_buffer(&p->b, &source, PRM_SET_BUFF_SIZE, HANTRO_TRUE)) return NOK;
  if (get_reference_pic_set((struct rps *)p)) return NOK;

  return OK;
}

u32 VCEncGetRoiMapVersion(u32 core_id)
{
  EWLHwConfig_t cfg = EWLReadAsicConfig(core_id);

  return cfg.roiMapVersion;
}

/*------------------------------------------------------------------------------

    VCEncCheckCfg

    Function checks that the configuration is valid.

    Input   pEncCfg Pointer to configuration structure.

    Return  ENCHW_OK      The configuration is valid.
            ENCHW_NOK     Some of the parameters in configuration are not valid.

------------------------------------------------------------------------------*/
bool_e VCEncCheckCfg(const VCEncConfig *pEncCfg)
{
  i32 height = pEncCfg->height;
  u32 client_type;

  ASSERT(pEncCfg);

  if ((pEncCfg->streamType != VCENC_BYTE_STREAM) &&
      (pEncCfg->streamType != VCENC_NAL_UNIT_STREAM))
  {
    APITRACEERR("VCEncCheckCfg: Invalid stream type");
    return ENCHW_NOK;
  }

  if (IS_AV1(pEncCfg->codecFormat)) {
    if (pEncCfg->streamType != VCENC_BYTE_STREAM)
    {
      APITRACEERR("VCEncCheckCfg: Invalid stream type, need byte stream when AV1");
      return ENCHW_NOK;
    }

    if (pEncCfg->width > 4096)
    {
      APITRACEERR("VCEncCheckCfg: Invalid width, need 4096 or smaller when AV1");
      return ENCHW_NOK;
    }

    if (pEncCfg->width*pEncCfg->height > 4096*2304)
    {
      APITRACEERR("VCEncCheckCfg: Invalid area, need 4096*2304 or below when AV1");
      return ENCHW_NOK;
    }
  }

  /* Encoded image width limits, multiple of 2 */
  if (pEncCfg->width < VCENC_MIN_ENC_WIDTH ||
      pEncCfg->width > VCENC_MAX_ENC_WIDTH || (pEncCfg->width & 0x1) != 0)
  {
    APITRACEERR("VCEncCheckCfg: Invalid width");
    return ENCHW_NOK;
  }

  /* Encoded image height limits, multiple of 2 */
  if (height < VCENC_MIN_ENC_HEIGHT ||
      height > VCENC_MAX_ENC_HEIGHT_EXT || (height & 0x1) != 0)
  {
    APITRACEERR("VCEncCheckCfg: Invalid height");
    return ENCHW_NOK;
  }

  /* Check frame rate */
  if (pEncCfg->frameRateNum < 1 || pEncCfg->frameRateNum > ((1 << 20) - 1))
  {
    APITRACEERR("VCEncCheckCfg: Invalid frameRateNum");
    return ENCHW_NOK;
  }

  if (pEncCfg->frameRateDenom < 1)
  {
    APITRACEERR("VCEncCheckCfg: Invalid frameRateDenom");
    return ENCHW_NOK;
  }

  /* check profile */
  {
    client_type = IS_H264(pEncCfg->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
    if ((IS_HEVC(pEncCfg->codecFormat) && ((pEncCfg->profile > VCENC_HEVC_MAINREXT) ||
        ((i32)pEncCfg->profile < VCENC_HEVC_MAIN_PROFILE) ||
        (((HW_ID_MAJOR_NUMBER(EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type)))) < 4) && (pEncCfg->profile == VCENC_HEVC_MAIN_10_PROFILE)))) ||
        (IS_H264(pEncCfg->codecFormat) && (pEncCfg->profile > VCENC_H264_HIGH_10_PROFILE || pEncCfg->profile < VCENC_H264_BASE_PROFILE)))
    {
      APITRACEERR("VCEncCheckCfg: Invalid profile");
      return ENCHW_NOK;
    }
  }

  /* check bit depth for main8 and main still profile */
  if (pEncCfg->profile < (IS_H264(pEncCfg->codecFormat) ? VCENC_H264_HIGH_10_PROFILE : VCENC_HEVC_MAIN_10_PROFILE)
      && (pEncCfg->bitDepthLuma != 8 || pEncCfg->bitDepthChroma != 8) && !IS_AV1(pEncCfg->codecFormat))
  {
    APITRACEERR("VCEncCheckCfg: Invalid bit depth for the profile");
    return ENCHW_NOK;
  }

  if (pEncCfg->codedChromaIdc > VCENC_CHROMA_IDC_420)
  {
    APITRACEERR("VCEncCheckCfg: Invalid codedChromaIdc, it's should be 0 ~ 1");
    return ENCHW_NOK;
  }

  /* check bit depth for main10 profile */
  if ((pEncCfg->profile == VCENC_HEVC_MAIN_10_PROFILE || pEncCfg->profile == VCENC_H264_HIGH_10_PROFILE)
      && (pEncCfg->bitDepthLuma < 8  || pEncCfg->bitDepthLuma > 10
          || pEncCfg->bitDepthChroma < 8 || pEncCfg->bitDepthChroma > 10))
  {
    APITRACEERR("VCEncCheckCfg: Invalid bit depth for main10 profile");
    return ENCHW_NOK;
  }


  /* check level */
  if ((IS_HEVC(pEncCfg->codecFormat) && ((pEncCfg->level > VCENC_HEVC_MAX_LEVEL) ||
      (pEncCfg->level < VCENC_HEVC_LEVEL_1))) ||
      (IS_H264(pEncCfg->codecFormat) && ((pEncCfg->level > VCENC_H264_MAX_LEVEL) ||
      (pEncCfg->level < VCENC_H264_LEVEL_1))))
  {
    APITRACEERR("VCEncCheckCfg: Invalid level");
    return ENCHW_NOK;
  }

  if (((i32)pEncCfg->tier > (i32)VCENC_HEVC_HIGH_TIER) || ((i32)pEncCfg->tier < (i32)VCENC_HEVC_MAIN_TIER))
  {
    APITRACEERR("VCEncCheckCfg: Invalid tier");
    return ENCHW_NOK;
  }

  if ((pEncCfg->tier == VCENC_HEVC_HIGH_TIER) && (IS_H264(pEncCfg->codecFormat) || pEncCfg->level < VCENC_HEVC_LEVEL_4))
  {
    APITRACEERR("VCEncCheckCfg: Invalid codec/level for chosen tier");
    return ENCHW_NOK;
  }

  if (pEncCfg->refFrameAmount > VCENC_MAX_REF_FRAMES)
  {
    APITRACEERR("VCEncCheckCfg: Invalid refFrameAmount");
    return ENCHW_NOK;
  }

  if ((pEncCfg->exp_of_input_alignment<4&&pEncCfg->exp_of_input_alignment>0)||
      (pEncCfg->exp_of_ref_alignment<4&&pEncCfg->exp_of_ref_alignment>0)||
      (pEncCfg->exp_of_ref_ch_alignment<4&&pEncCfg->exp_of_ref_ch_alignment>0))
  {
    APITRACEERR("VCEncCheckCfg: Invalid alignment value");
    return ENCHW_NOK;
  }

  if (IS_H264(pEncCfg->codecFormat) && (pEncCfg->picOrderCntType != 2 && (pEncCfg->picOrderCntType != 0)))
  {
    APITRACEERR("VCEncCheckCfg: H264 POCCntType support 0 or 2");
    return ENCHW_NOK;
  }

  /* check HW limitations */
  u32 i = 0;
  i32 core_id = -1;
  i32 core_num = EWLGetCoreNum();
  for (i = 0; i < core_num; i ++)
  {
    EWLHwConfig_t cfg = EncAsicGetAsicConfig(i);
    /* is HEVC encoding supported */
    if (IS_HEVC(pEncCfg->codecFormat) && cfg.hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      APITRACEERR("VCEncCheckCfg: Invalid format, hevc not supported by HW coding core");
      continue;
    }

    /* is H264 encoding supported */
    if (IS_H264(pEncCfg->codecFormat) && cfg.h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      APITRACEERR("VCEncCheckCfg: Invalid format, h264 not supported by HW coding core");
      continue;
    }

    /* max width supported */
    if ((IS_H264(pEncCfg->codecFormat) ? cfg.maxEncodedWidthH264 : cfg.maxEncodedWidthHEVC) < pEncCfg->width)
    {
      APITRACEERR("VCEncCheckCfg: Invalid width, not supported by HW coding core");
      continue;
    }

    /* P010 Ref format */
    if (pEncCfg->P010RefEnable && cfg.P010RefSupport == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      APITRACEERR("VCEncCheckCfg: Invalid format, P010Ref not supported by HW coding core");
      continue;
    }

    /*extend video coding height */
    if ((height > VCENC_MAX_ENC_HEIGHT) && (cfg.videoHeightExt == EWL_HW_CONFIG_NOT_SUPPORTED))
    {
      APITRACEERR("VCEncCheckCfg: Invalid height, height extension not supported by HW coding core");
      continue;
    }

    /*disable recon write to DDR*/
    if (pEncCfg->writeReconToDDR == 0 && cfg.disableRecWtSupport == EWL_HW_CONFIG_NOT_SUPPORTED)
    {
      APITRACEERR("VCEncCheckCfg: disable recon write to DDR not supported by HW coding core");
      continue;
    }

    core_id = i;
    break;
  }

  if (core_id < 0)
  {
    APITRACEERR("VCEncCheckCfg: Invalid coding configuration for HW");
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}
static i32 getlevelIdx(i32 level)
{
  switch (level)
  {
    case VCENC_HEVC_LEVEL_1:
      return 0;
    case VCENC_HEVC_LEVEL_2:
      return 1;
    case VCENC_HEVC_LEVEL_2_1:
      return 2;
    case VCENC_HEVC_LEVEL_3:
      return 3;
    case VCENC_HEVC_LEVEL_3_1:
      return 4;
    case VCENC_HEVC_LEVEL_4:
      return 5;
    case VCENC_HEVC_LEVEL_4_1:
      return 6;
    case VCENC_HEVC_LEVEL_5:
      return 7;
    case VCENC_HEVC_LEVEL_5_1:
      return 8;
    case VCENC_HEVC_LEVEL_5_2:
      return 9;
    case VCENC_HEVC_LEVEL_6:
      return 10;
    case VCENC_HEVC_LEVEL_6_1:
      return 11;
    case VCENC_HEVC_LEVEL_6_2:
      return 12;
    default:
      ASSERT(0);
  }
  return 0;
}
static i32 getlevelIdxH264(i32 level)
{
  switch (level)
  {
    case VCENC_H264_LEVEL_1:
      return 13;
    case VCENC_H264_LEVEL_1_b:
      return 14;
    case VCENC_H264_LEVEL_1_1:
      return 15;
    case VCENC_H264_LEVEL_1_2:
      return 16;
    case VCENC_H264_LEVEL_1_3:
      return 17;
    case VCENC_H264_LEVEL_2:
      return 18;
    case VCENC_H264_LEVEL_2_1:
      return 19;
    case VCENC_H264_LEVEL_2_2:
      return 20;
    case VCENC_H264_LEVEL_3:
      return 21;
    case VCENC_H264_LEVEL_3_1:
      return 22;
    case VCENC_H264_LEVEL_3_2:
      return 23;
    case VCENC_H264_LEVEL_4:
      return 24;
    case VCENC_H264_LEVEL_4_1:
      return 25;
    case VCENC_H264_LEVEL_4_2:
      return 26;
    case VCENC_H264_LEVEL_5:
      return 27;
    case VCENC_H264_LEVEL_5_1:
      return 28;
    case VCENC_H264_LEVEL_5_2:
      return 29;
    case VCENC_H264_LEVEL_6:
      return 30;
    case VCENC_H264_LEVEL_6_1:
      return 31;
    case VCENC_H264_LEVEL_6_2:
      return 32;
    default:
      ASSERT(0);
  }
  return 0;
}

/*------------------------------------------------------------------------------

    SetParameter

    Set all parameters in instance to valid values depending on user config.

------------------------------------------------------------------------------*/
bool_e SetParameter(struct vcenc_instance *inst, const VCEncConfig   *pEncCfg)
{
  i32 width, height, bps;
  EWLHwConfig_t cfg;
  int ctb_size;
  float h264_high10_factor = 1;
  if(pEncCfg->profile == VCENC_H264_HIGH_PROFILE)
    h264_high10_factor = 1.25;
  else if(pEncCfg->profile == VCENC_H264_HIGH_10_PROFILE)
    h264_high10_factor = 3.0;
  true_e bCorrectProfile;

  ASSERT(inst);

  //inst->width = pEncCfg->width;
  //inst->height = pEncCfg->height;

  inst->codecFormat = pEncCfg->codecFormat;
  ctb_size = (IS_H264(inst->codecFormat) ? 16 : 64);
  /* Internal images, next macroblock boundary */
  width = ctb_size * ((inst->width + ctb_size - 1) / ctb_size);
  height = ctb_size * ((inst->height + ctb_size - 1) / ctb_size);

  /* stream type */
  if (pEncCfg->streamType == VCENC_BYTE_STREAM)
  {
    inst->asic.regs.streamMode = ASIC_VCENC_BYTE_STREAM;
    //        inst->rateControl.sei.byteStream = ENCHW_YES;
  }
  else
  {
    inst->asic.regs.streamMode = ASIC_VCENC_NAL_UNIT;
    //        inst->rateControl.sei.byteStream = ENCHW_NO;
  }

  /* ctb */
  inst->ctbPerRow = width / ctb_size;
  inst->ctbPerCol = height / ctb_size;
  inst->ctbPerFrame = inst->ctbPerRow * inst->ctbPerCol;


  /* Disable intra and ROI areas by default */
  inst->asic.regs.intraAreaTop = inst->asic.regs.intraAreaBottom = inst->ctbPerCol;
  inst->asic.regs.intraAreaLeft = inst->asic.regs.intraAreaRight = inst->ctbPerRow;
  inst->asic.regs.roi1Top = inst->asic.regs.roi1Bottom = inst->ctbPerCol;
  inst->asic.regs.roi1Left = inst->asic.regs.roi1Right = inst->ctbPerRow;
  inst->asic.regs.roi2Top = inst->asic.regs.roi2Bottom = inst->ctbPerCol;
  inst->asic.regs.roi2Left = inst->asic.regs.roi2Right = inst->ctbPerRow;

  /* Disable IPCM areas by default */
  inst->asic.regs.ipcm1AreaTop= inst->asic.regs.ipcm1AreaBottom= inst->ctbPerCol;
  inst->asic.regs.ipcm1AreaLeft = inst->asic.regs.ipcm1AreaRight = inst->ctbPerRow;
  inst->asic.regs.ipcm2AreaTop= inst->asic.regs.ipcm2AreaBottom= inst->ctbPerCol;
  inst->asic.regs.ipcm2AreaLeft = inst->asic.regs.ipcm2AreaRight = inst->ctbPerRow;

  /* Slice num */
  inst->asic.regs.sliceSize = 0;
  inst->asic.regs.sliceNum = 1;

  /* compressor */
  inst->asic.regs.recon_luma_compress = (pEncCfg->compressor & 1) ? 1 : 0;
   inst->asic.regs.recon_chroma_compress = ((pEncCfg->compressor & 2)&&(!(( VCENC_CHROMA_IDC_400 == pEncCfg->codedChromaIdc) && (inst->asic.regs.asicCfg.MonoChromeSupport == EWL_HW_CONFIG_ENABLED)))) ? 1 : 0;

  /* CU Info output*/
  inst->asic.regs.enableOutputCuInfo = pEncCfg->enableOutputCuInfo;
  if(inst->asic.regs.asicCfg.cuInforVersion==2 || inst->asic.regs.asicCfg.cuInforVersion==3)
  {
    if(pEncCfg->cuInfoVersion != -1)
      inst->asic.regs.cuInfoVersion = pEncCfg->cuInfoVersion;
    else
      inst->asic.regs.cuInfoVersion = (inst->asic.regs.asicCfg.bMultiPassSupport && pEncCfg->pass == 1 ? inst->asic.regs.asicCfg.cuInforVersion : 1);
  }
  else
    inst->asic.regs.cuInfoVersion = inst->asic.regs.asicCfg.cuInforVersion;

  /* Sequence parameter set */
  inst->level = pEncCfg->level;
  inst->levelIdx = (IS_H264(pEncCfg->codecFormat) ? getlevelIdxH264(inst->level) : getlevelIdx(inst->level));
  inst->tier = pEncCfg->tier;

  /* RDO effort level */
  inst->rdoLevel = inst->asic.regs.rdoLevel = pEncCfg->rdoLevel;

  inst->asic.regs.codedChromaIdc = (inst->asic.regs.asicCfg.MonoChromeSupport == EWL_HW_CONFIG_NOT_SUPPORTED) ? VCENC_CHROMA_IDC_420: pEncCfg->codedChromaIdc;
  bCorrectProfile   = ( VCENC_CHROMA_IDC_400 == pEncCfg->codedChromaIdc) && (inst->asic.regs.asicCfg.MonoChromeSupport == EWL_HW_CONFIG_ENABLED);
  switch (pEncCfg->profile)
  {
    case 0:
      //main profile
      inst->profile = bCorrectProfile ? 4:1;
      break;
    case 1:
      //main still picture profile.
      inst->profile = 3;
      break;
    case 2:
      //main 10 profile.
      inst->profile = bCorrectProfile ? 4:2;
      break;
    case 3:
      //main ext profile.
      inst->profile = 4;
      break;
    case 9:
      //H.264 baseline profile
      inst->profile = bCorrectProfile ? 100:66;
      break;
    case 10:
      //H.264 main profile
      inst->profile = bCorrectProfile ? 100:77;
      break;
    case 11:
      //H.264 high profile
      inst->profile = 100;
      break;
    case 12:
      //H.264 high 10 profile
      inst->profile = 110;
      break;
    default:
      break;
  }
  /* enforce maximum frame size in level */
  if ((u32)(inst->width * inst->height) > VCEncMaxFS[inst->levelIdx])
  {
    printf("WARNING: MaxFS violates level limit\n");
  }

  /* enforce Max luma sample rate in level */
  {
    u32 sample_rate =
      (pEncCfg->frameRateNum * inst->width * inst->height) /
      pEncCfg->frameRateDenom;

    if (sample_rate > VCEncMaxSBPS[inst->levelIdx])
    {
      printf("WARNING: MaxSBPS violates level limit\n");
    }
  }
  /* intra */
  inst->constrained_intra_pred_flag = 0;
  inst->strong_intra_smoothing_enabled_flag = pEncCfg->strongIntraSmoothing;


  inst->vps->max_dec_pic_buffering[0] = pEncCfg->refFrameAmount + 1;
  inst->sps->max_dec_pic_buffering[0] = pEncCfg->refFrameAmount + 1;


  inst->sps->bit_depth_luma_minus8 = pEncCfg->bitDepthLuma - 8;
  inst->sps->bit_depth_chroma_minus8 = pEncCfg->bitDepthChroma - 8;

  inst->vps->max_num_sub_layers = pEncCfg->maxTLayers;
  inst->vps->temporal_id_nesting_flag =1;
  for(int i = 0; i < inst->vps->max_num_sub_layers; i++)
  {
    inst->vps->max_dec_pic_buffering[i] =inst->vps->max_dec_pic_buffering[0];
  }

  inst->sps->max_num_sub_layers = pEncCfg->maxTLayers;
  inst->sps->temporal_id_nesting_flag =1;
  for(int i = 0; i < inst->sps->max_num_sub_layers; i++)
  {
    inst->sps->max_dec_pic_buffering[i] =inst->sps->max_dec_pic_buffering[0];
  }

  inst->sps->picOrderCntType = pEncCfg->picOrderCntType;
  inst->sps->log2_max_pic_order_cnt_lsb  = pEncCfg->log2MaxPicOrderCntLsb;
  inst->sps->log2MaxFrameNumMinus4 = (pEncCfg->log2MaxFrameNum - 4);
  inst->sps->log2MaxpicOrderCntLsbMinus4 = (pEncCfg->log2MaxPicOrderCntLsb - 4);

  /* Rate control setup */

  /* Maximum bitrate for the specified level */
  bps = (inst->tier==VCENC_HEVC_HIGH_TIER ? VCEncMaxBRHighTier[inst->levelIdx]:VCEncMaxBR[inst->levelIdx] * h264_high10_factor);

  {
    vcencRateControl_s *rc = &inst->rateControl;

    rc->outRateDenom = pEncCfg->frameRateDenom;
    rc->outRateNum = pEncCfg->frameRateNum;
    rc->picArea = ((inst->width + 7) & (~7)) * ((inst->height + 7) & (~7));
    rc->ctbPerPic = inst->ctbPerFrame;
    rc->reciprocalOfNumBlk8 = 1.0/(rc->ctbPerPic*(ctb_size/8)*(ctb_size/8));
    rc->ctbRows = inst->ctbPerCol;
    rc->ctbCols = inst->ctbPerRow;
    rc->ctbSize = ctb_size;

    {
      rcVirtualBuffer_s *vb = &rc->virtualBuffer;

      vb->bitRate = bps;
      vb->unitsInTic = pEncCfg->frameRateDenom;
      vb->timeScale = pEncCfg->frameRateNum * (inst->interlaced + 1);
      vb->bufferSize = (pEncCfg->tier==VCENC_HEVC_HIGH_TIER ? VCEncMaxCPBSHighTier[inst->levelIdx] : VCEncMaxCPBS[inst->levelIdx] * h264_high10_factor);
    }

    rc->hrd = ENCHW_NO;
    rc->picRc = ENCHW_NO;
    rc->ctbRc = 0;
    rc->picSkip = ENCHW_NO;
    rc->vbr = ENCHW_NO;

    rc->qpHdr = VCENC_DEFAULT_QP << QP_FRACTIONAL_BITS;
    rc->qpMin = 0 << QP_FRACTIONAL_BITS;
    rc->qpMax = 51 << QP_FRACTIONAL_BITS;

    rc->frameCoded = ENCHW_YES;
    rc->sliceTypeCur = I_SLICE;
    rc->sliceTypePrev = P_SLICE;
    rc->bitrateWindow = 150;

    /* Default initial value for intra QP delta */
    rc->intraQpDelta = INTRA_QPDELTA_DEFAULT << QP_FRACTIONAL_BITS;
    rc->fixedIntraQp = 0 << QP_FRACTIONAL_BITS;
    rc->frameQpDelta = 0 << QP_FRACTIONAL_BITS;
    rc->gopPoc       = 0;
  }

  /* no SEI by default */
  inst->rateControl.sei.enabled = ENCHW_NO;

  /* Pre processing */
  inst->preProcess.lumWidth = pEncCfg->width;
  inst->preProcess.lumWidthSrc =
    VCEncGetAllowedWidth(pEncCfg->width, VCENC_YUV420_PLANAR);

  inst->preProcess.lumHeight = pEncCfg->height / ((inst->interlaced + 1));
  inst->preProcess.lumHeightSrc = pEncCfg->height;

  inst->preProcess.horOffsetSrc = 0;
  inst->preProcess.verOffsetSrc = 0;

  inst->preProcess.rotation = ROTATE_0;
  inst->preProcess.mirror = VCENC_MIRROR_NO;
  inst->preProcess.inputFormat = VCENC_YUV420_PLANAR;
  inst->preProcess.videoStab = 0;

  inst->preProcess.scaledWidth    = 0;
  inst->preProcess.scaledHeight   = 0;
  inst->preProcess.scaledOutput   = 0;
  inst->preProcess.interlacedFrame = inst->interlaced;
  inst->asic.scaledImage.busAddress = 0;
  inst->asic.scaledImage.virtualAddress = NULL;
  inst->asic.scaledImage.size = 0;
  inst->preProcess.adaptiveRoi = 0;
  inst->preProcess.adaptiveRoiColor  = 0;
  inst->preProcess.adaptiveRoiMotion = -5;

  inst->preProcess.input_alignment = inst->input_alignment;
  inst->preProcess.colorConversionType = 0;

  EncSetColorConversion(&inst->preProcess, &inst->asic);

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    Round the width to the next multiple of 8 or 16 depending on YUV type.

------------------------------------------------------------------------------*/
i32 VCEncGetAllowedWidth(i32 width, VCEncPictureType inputType)
{
  if (inputType == VCENC_YUV420_PLANAR)
  {
    /* Width must be multiple of 16 to make
     * chrominance row 64-bit aligned */
    return ((width + 15) / 16) * 16;
  }
  else
  {
    /* VCENC_YUV420_SEMIPLANAR */
    /* VCENC_YUV422_INTERLEAVED_YUYV */
    /* VCENC_YUV422_INTERLEAVED_UYVY */
    return ((width + 7) / 8) * 8;
  }
}

void IntraLamdaQp(int qp, u32 *lamda_sad, enum slice_type type, int poc, float dQPFactor, bool depth0)
{
  float recalQP = (float)qp;
  float lambda;

  // pre-compute lambda and QP values for all possible QP candidates
  {
    // compute lambda value
    //Int    NumberBFrames = 0;
    //double dLambda_scale = 1.0 - MAX( 0.0, MIN( 0.5, 0.05*(double)NumberBFrames ));
    //double qp_temp = (double) recalQP + bitdepth_luma_qp_scale - SHIFT_QP;
    // Case #1: I or P-slices (key-frame)
    //double dQPFactor = type == B_SLICE ? 0.68 : 0.8;//0.442;//0.576;//0.8;
    //lambda = dQPFactor * pow(2.0, qp_temp / 3.0);
    //if (type == B_SLICE)
    //lambda *= CLIP3(2.0, 4.0, (qp - 12) / 6.0); //* 0.8/0.576;

    //clip
    //    if (((unsigned int)lambda) > ((1<<14)-1))
    //      lambda = (double)((1<<14)-1);

    Int    SHIFT_QP = 12;
    Int    g_bitDepth = 8;
    Int    bitdepth_luma_qp_scale = 6 * (g_bitDepth - 8);
    Int qp_temp = qp + bitdepth_luma_qp_scale;


    lambda = (float)dQPFactor * sqrtPow2QpDiv3[qp_temp];
    if (!depth0)
      lambda *= sqrtQpDiv6[qp];
  }

  // store lambda
  //lambda = sqrt(lambda);

  ASSERT(lambda < 255);

  *lamda_sad = (u32)(lambda * (1 << IMS_LAMBDA_SAD_SHIFT) + 0.5f);
}

void InterLamdaQp(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad, enum slice_type type, float dQPFactor, bool depth0, u32 asicId)
{
  float recalQP = (float)qp;
  float lambda;

  // pre-compute lambda and QP values for all possible QP candidates
  {
    // compute lambda value
    //Int    NumberBFrames = 0;
    //double dLambda_scale = 1.0 - MAX( 0.0, MIN( 0.5, 0.05*(double)NumberBFrames ));
    //double qp_temp = (double) recalQP + bitdepth_luma_qp_scale - SHIFT_QP;
    // Case #1: I or P-slices (key-frame)
    //double dQPFactor = type == B_SLICE ? 0.68 : 0.8;//0.442;//0.576;//0.8;
    //lambda = dQPFactor * pow(2.0, qp_temp / 3.0);
    //if (type == B_SLICE)
    //lambda *= CLIP3(2.0, 4.0, (qp - 12) / 6.0); //* 0.8/0.576;

    Int    SHIFT_QP = 12;
    Int    g_bitDepth = 8;
    Int    bitdepth_luma_qp_scale = 6 * (g_bitDepth - 8);
    Int qp_temp = qp + bitdepth_luma_qp_scale;


    lambda = (float)dQPFactor * sqrtPow2QpDiv3[qp_temp];
    if (!depth0)
      lambda *= sqrtQpDiv6[qp];
  }

  u32 lambdaSSEShift = 0;
  u32 lambdaSADShift = 0;
  if ((HW_ID_MAJOR_NUMBER(asicId)) >= 5) /* H2V5 rev01 */
  {
    lambdaSSEShift = MOTION_LAMBDA_SSE_SHIFT;
    lambdaSADShift = MOTION_LAMBDA_SAD_SHIFT;
  }

  //keep accurate for small QP
  u32 lambdaSSE = (u32)(lambda*lambda * (1 << lambdaSSEShift) + 0.5f);
  u32 lambdaSAD = (u32)(lambda * (1 << lambdaSADShift) + 0.5f);

  //clip
  u32 maxLambdaSSE = (1 << (15+lambdaSSEShift)) - 1;
  u32 maxLambdaSAD = (1 << (8+lambdaSADShift)) - 1;
  lambdaSSE = MIN(lambdaSSE, maxLambdaSSE);
  lambdaSAD = MIN(lambdaSAD, maxLambdaSAD);

  // store lambda
  *lamda_sse = lambdaSSE;
  *lamda_sad = lambdaSAD;
}

void IntraLamdaQpFixPoint(int qp, u32 *lamda_sad, enum slice_type type, int poc, u32 qpFactorSad, bool depth0)
{
  i32 shiftSad = LAMBDA_FIX_POINT + QPFACTOR_FIX_POINT - IMS_LAMBDA_SAD_SHIFT;
  u32 roundSad = 1 << (shiftSad-1);
  u32 maxLambdaSAD = (1 << (8 + IMS_LAMBDA_SAD_SHIFT)) - 1;
  const u32 *lambdaSadTbl = (!depth0) ? lambdaSadDepth1Tbl : lambdaSadDepth0Tbl;

  u64 lambdaSad = ((u64)qpFactorSad * lambdaSadTbl[qp] + roundSad) >> shiftSad;
  *lamda_sad = MIN(lambdaSad, maxLambdaSAD);
}

void InterLamdaQpFixPoint(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad, enum slice_type type, u32 qpFactorSad, u32 qpFactorSse, bool depth0, u32 asicId)
{
  i32 shiftSad = LAMBDA_FIX_POINT + QPFACTOR_FIX_POINT - MOTION_LAMBDA_SAD_SHIFT;
  i32 shiftSse = LAMBDA_FIX_POINT + QPFACTOR_FIX_POINT - MOTION_LAMBDA_SSE_SHIFT;
  u32 roundSad = 1 << (shiftSad-1);
  u32 roundSse = 1 << (shiftSse-1);
  u32 maxLambdaSAD = (1 << (8 + MOTION_LAMBDA_SAD_SHIFT)) - 1;
  u32 maxLambdaSSE = (1 << (15+ MOTION_LAMBDA_SSE_SHIFT)) - 1;
  const u32 *lambdaSadTbl = (!depth0) ? lambdaSadDepth1Tbl : lambdaSadDepth0Tbl;
  const u32 *lambdaSseTbl = (!depth0) ? lambdaSseDepth1Tbl : lambdaSseDepth0Tbl;

  u64 lambdaSad = ((u64)qpFactorSad * lambdaSadTbl[qp] + roundSad) >> shiftSad;
  u64 lambdaSse = ((u64)qpFactorSse * lambdaSseTbl[qp] + roundSse) >> shiftSse;
  lambdaSse = MIN(lambdaSse, maxLambdaSSE);
  lambdaSad = MIN(lambdaSad, maxLambdaSAD);
  if ((HW_ID_MAJOR_NUMBER(asicId)) < 5) /* H2V5 rev01 */
  {
    lambdaSse >>= MOTION_LAMBDA_SSE_SHIFT;
    lambdaSad >>= MOTION_LAMBDA_SAD_SHIFT;
  }

  // store lambda
  *lamda_sse = lambdaSse;
  *lamda_sad = lambdaSad;
}

//Set LamdaTable for all qp
void LamdaTableQp(regValues_s *regs, int qp, enum slice_type type, int poc, double dQPFactor, bool depth0, bool ctbRc)
{
  int qpoffset, lamdaIdx;
#ifdef FIX_POINT_LAMBDA
  u32 qpFactorSad = (u32)((dQPFactor * (1<<QPFACTOR_FIX_POINT)) + 0.5f);
  u32 qpFactorSse = (u32)((dQPFactor * dQPFactor * (1<<QPFACTOR_FIX_POINT)) + 0.5f);
#endif
#ifndef CTBRC_STRENGTH

  //setup LamdaTable dependent on ctbRc enable
  if (ctbRc)
  {

    for (qpoffset = -2, lamdaIdx = 14; qpoffset <= +2; qpoffset++, lamdaIdx++)
    {
#ifdef FIX_POINT_LAMBDA
      IntraLamdaQpFixPoint(qp - qpoffset, regs->lambda_satd_ims + (lamdaIdx & 0xf), type, poc, qpFactorSad, depth0);
      InterLamdaQpFixPoint(qp - qpoffset, regs->lambda_sse_me + (lamdaIdx & 0xf),  regs->lambda_satd_me + (lamdaIdx & 0xf), type, qpFactorSad, qpFactorSse, depth0, regs->asicHwId);
#else
      IntraLamdaQp(qp - qpoffset, regs->lambda_satd_ims + (lamdaIdx & 0xf), type, poc, dQPFactor, depth0);
      InterLamdaQp(qp - qpoffset, regs->lambda_sse_me + (lamdaIdx & 0xf),  regs->lambda_satd_me + (lamdaIdx & 0xf), type, dQPFactor, depth0, regs->asicHwId);
#endif
    }

  }
  else
  {
    //for ROI-window, ROI-Map
    for (qpoffset = 0, lamdaIdx = 0; qpoffset <= 15; qpoffset++, lamdaIdx++)
    {
#ifdef FIX_POINT_LAMBDA
      IntraLamdaQpFixPoint(qp - qpoffset, regs->lambda_satd_ims + lamdaIdx, type, poc, qpFactorSad, depth0);
      InterLamdaQpFixPoint(qp - qpoffset, regs->lambda_sse_me + lamdaIdx,   regs->lambda_satd_me + lamdaIdx, type, qpFactorSad, qpFactorSse, depth0, regs->asicHwId);
#else
      IntraLamdaQp(qp - qpoffset, regs->lambda_satd_ims + lamdaIdx, type, poc, dQPFactor, depth0);
      InterLamdaQp(qp - qpoffset, regs->lambda_sse_me + lamdaIdx,   regs->lambda_satd_me + lamdaIdx, type, dQPFactor, depth0, regs->asicHwId);
#endif
    }
  }
#else

	regs->offsetSliceQp = 0;
	if( qp >= 35)
	{
		regs->offsetSliceQp = 35 - qp;
	}
	if(qp <= 15)
	{
		regs->offsetSliceQp = 15 - qp;
	}

  for (qpoffset = -16, lamdaIdx = 16 /* - regs->offsetSliceQp*/; qpoffset <= +15; qpoffset++, lamdaIdx++)
  {
#ifdef FIX_POINT_LAMBDA
    IntraLamdaQpFixPoint(CLIP3(0,51,qp - qpoffset + regs->offsetSliceQp), regs->lambda_satd_ims + (lamdaIdx & 0x1f), type, poc, qpFactorSad, depth0);
    InterLamdaQpFixPoint(CLIP3(0,51,qp - qpoffset + regs->offsetSliceQp), regs->lambda_sse_me + (lamdaIdx & 0x1f),  regs->lambda_satd_me + (lamdaIdx & 0x1f), type, qpFactorSad, qpFactorSse, depth0, regs->asicHwId);
#else
    IntraLamdaQp(CLIP3(0,51,qp - qpoffset + regs->offsetSliceQp), regs->lambda_satd_ims + (lamdaIdx & 0x1f), type, poc, dQPFactor, depth0);
    InterLamdaQp(CLIP3(0,51,qp - qpoffset + regs->offsetSliceQp), regs->lambda_sse_me + (lamdaIdx & 0x1f),  regs->lambda_satd_me + (lamdaIdx & 0x1f), type, dQPFactor, depth0, regs->asicHwId);
#endif
  }

  if (regs->asicCfg.roiAbsQpSupport)
  {
    regs->qpFactorSad = (u32)((dQPFactor * (1<<QPFACTOR_FIX_POINT)) + 0.5);
    regs->qpFactorSse = (u32)((dQPFactor * dQPFactor * (1<<QPFACTOR_FIX_POINT)) + 0.5);
    regs->lambdaDepth = !depth0;
  }
#endif
}

void FillIntraFactor(regValues_s *regs)
{
#if 0
  const double weight = 0.57 / 0.8;
  const double size_coeff[4] =
  {
    30.0,
    30.0,
    42.0,
    42.0
  };
  const u32 mode_coeff[3] =
  {
    0x6276 + 1 * 0x8000,
    0x6276 + 2 * 0x8000,
    0x6276 + 5 * 0x8000,
  };

  regs->intra_size_factor[0] = double_to_u6q4(sqrt(weight) * (1 / 0.8) * size_coeff[0]);
  regs->intra_size_factor[1] = double_to_u6q4(sqrt(weight) * (1 / 0.8) * size_coeff[1]);
  regs->intra_size_factor[2] = double_to_u6q4(sqrt(weight) * (1 / 0.8) * size_coeff[2]);
  regs->intra_size_factor[3] = double_to_u6q4(sqrt(weight) * (1 / 0.8) * size_coeff[3]);

  regs->intra_mode_factor[0] = double_to_u6q4(sqrt(weight) * (double) mode_coeff[0] / 32768.0);
  regs->intra_mode_factor[1] = double_to_u6q4(sqrt(weight) * (double) mode_coeff[1] / 32768.0);
  regs->intra_mode_factor[2] = double_to_u6q4(sqrt(weight) * (double) mode_coeff[2] / 32768.0);
#else
  regs->intra_size_factor[0] = 506;
  regs->intra_size_factor[1] = 506;
  regs->intra_size_factor[2] = 709;
  regs->intra_size_factor[3] = 709;

  if(regs->codingType == ASIC_H264) {
    regs->intra_mode_factor[0] = 24;
    regs->intra_mode_factor[1] = 12;
    regs->intra_mode_factor[2] = 48;
  } else {
    regs->intra_mode_factor[0] = 24;
    regs->intra_mode_factor[1] = 37;
    regs->intra_mode_factor[2] = 78;
  }
#endif

  return;
}

/*------------------------------------------------------------------------------

    Set QP related parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
static void VCEncSetQuantParameters(asicData_s *asic, double qp_factor,
                                      enum slice_type type, bool is_depth0,
                                      true_e enable_ctu_rc)
{
  /* Intra Penalty for TU 4x4 vs. 8x8 */
  static unsigned short VCEncIntraPenaltyTu4[52] =
  {
    7,    7,    8,   10,   11,   12,   14,   15,   17,   20,
    22,   25,   28,   31,   35,   40,   44,   50,   56,   63,
    71,   80,   89,  100,  113,  127,  142,  160,  179,  201,
    226,  254,  285,  320,  359,  403,  452,  508,  570,  640,
    719,  807,  905, 1016, 1141, 1281, 1438, 1614, 1811, 2033,
    2282, 2562
  };//max*3=13bit

  /* Intra Penalty for TU 8x8 vs. 16x16 */
  static unsigned short VCEncIntraPenaltyTu8[52] =
  {
    7,    7,    8,   10,   11,   12,   14,   15,   17,   20,
    22,   25,   28,   31,   35,   40,   44,   50,   56,   63,
    71,   80,   89,  100,  113,  127,  142,  160,  179,  201,
    226,  254,  285,  320,  359,  403,  452,  508,  570,  640,
    719,  807,  905, 1016, 1141, 1281, 1438, 1614, 1811, 2033,
    2282, 2562
  };//max*3=13bit

  /* Intra Penalty for TU 16x16 vs. 32x32 */
  static unsigned short VCEncIntraPenaltyTu16[52] =
  {
    9,   11,   12,   14,   15,   17,   19,   22,   24,   28,
    31,   35,   39,   44,   49,   56,   62,   70,   79,   88,
    99,  112,  125,  141,  158,  177,  199,  224,  251,  282,
    317,  355,  399,  448,  503,  564,  634,  711,  799,  896,
    1006, 1129, 1268, 1423, 1598, 1793, 2013, 2259, 2536, 2847,
    3196, 3587
  };//max*3=14bit

  /* Intra Penalty for TU 32x32 vs. 64x64 */
  static unsigned short VCEncIntraPenaltyTu32[52] =
  {
    9,   11,   12,   14,   15,   17,   19,   22,   24,   28,
    31,   35,   39,   44,   49,   56,   62,   70,   79,   88,
    99,  112,  125,  141,  158,  177,  199,  224,  251,  282,
    317,  355,  399,  448,  503,  564,  634,  711,  799,  896,
    1006, 1129, 1268, 1423, 1598, 1793, 2013, 2259, 2536, 2847,
    3196, 3587
  };//max*3=14bit

  /* Intra Penalty for Mode a */
  static unsigned short VCEncIntraPenaltyModeA[52] =
  {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,
    3,    3,    4,    4,    5,    5,    6,    7,    8,    9,
    10,   11,   13,   15,   16,   19,   21,   23,   26,   30,
    33,   38,   42,   47,   53,   60,   67,   76,   85,   95,
    107,  120
  };//max*3=9bit

  /* Intra Penalty for Mode b */
  static unsigned short VCEncIntraPenaltyModeB[52] =
  {
    0,    0,    0,    0,    0,    0,    1,    1,    1,    1,
    1,    1,    2,    2,    2,    2,    3,    3,    4,    4,
    5,    5,    6,    7,    8,    9,   10,   11,   13,   14,
    16,   18,   21,   23,   26,   29,   33,   37,   42,   47,
    53,   59,   66,   75,   84,   94,  106,  119,  133,  150,
    168,  189
  };////max*3=10bit

  /* Intra Penalty for Mode c */
  static unsigned short VCEncIntraPenaltyModeC[52] =
  {
    1,    1,    1,    1,    1,    1,    2,    2,    2,    3,
    3,    3,    4,    4,    5,    6,    6,    7,    8,    9,
    10,   12,   13,   15,   17,   19,   21,   24,   27,   31,
    34,   39,   43,   49,   55,   62,   69,   78,   87,   98,
    110,  124,  139,  156,  175,  197,  221,  248,  278,  312,
    351,  394
  };//max*3=11bit

  switch (HW_ID_MAJOR_NUMBER(asic->regs.asicHwId))
  {
    case 1: /* H2V1 rev01 */
      asic->regs.intraPenaltyPic4x4  = VCEncIntraPenaltyTu4[asic->regs.qp];
      asic->regs.intraPenaltyPic8x8  = VCEncIntraPenaltyTu8[asic->regs.qp];
      asic->regs.intraPenaltyPic16x16 = VCEncIntraPenaltyTu16[asic->regs.qp];
      asic->regs.intraPenaltyPic32x32 = VCEncIntraPenaltyTu32[asic->regs.qp];

      asic->regs.intraMPMPenaltyPic1 = VCEncIntraPenaltyModeA[asic->regs.qp];
      asic->regs.intraMPMPenaltyPic2 = VCEncIntraPenaltyModeB[asic->regs.qp];
      asic->regs.intraMPMPenaltyPic3 = VCEncIntraPenaltyModeC[asic->regs.qp];

      u32 roi1_qp = CLIP3(0, 51, ((int)asic->regs.qp - (int)asic->regs.roi1DeltaQp));
      asic->regs.intraPenaltyRoi14x4 = VCEncIntraPenaltyTu4[roi1_qp];
      asic->regs.intraPenaltyRoi18x8 = VCEncIntraPenaltyTu8[roi1_qp];
      asic->regs.intraPenaltyRoi116x16 = VCEncIntraPenaltyTu16[roi1_qp];
      asic->regs.intraPenaltyRoi132x32 = VCEncIntraPenaltyTu32[roi1_qp];

      asic->regs.intraMPMPenaltyRoi11 = VCEncIntraPenaltyModeA[roi1_qp];
      asic->regs.intraMPMPenaltyRoi12 = VCEncIntraPenaltyModeB[roi1_qp];
      asic->regs.intraMPMPenaltyRoi13 = VCEncIntraPenaltyModeC[roi1_qp];

      u32 roi2_qp = CLIP3(0, 51, ((int)asic->regs.qp - (int)asic->regs.roi2DeltaQp));
      asic->regs.intraPenaltyRoi24x4 = VCEncIntraPenaltyTu4[roi2_qp];
      asic->regs.intraPenaltyRoi28x8 = VCEncIntraPenaltyTu8[roi2_qp];
      asic->regs.intraPenaltyRoi216x16 = VCEncIntraPenaltyTu16[roi2_qp];
      asic->regs.intraPenaltyRoi232x32 = VCEncIntraPenaltyTu32[roi2_qp];

      asic->regs.intraMPMPenaltyRoi21 = VCEncIntraPenaltyModeA[roi2_qp]; //need to change
      asic->regs.intraMPMPenaltyRoi22 = VCEncIntraPenaltyModeB[roi2_qp]; //need to change
      asic->regs.intraMPMPenaltyRoi23 = VCEncIntraPenaltyModeC[roi2_qp];  //need to change

      /*inter prediction parameters*/
#ifndef FIX_POINT_LAMBDA
      InterLamdaQp(asic->regs.qp, &asic->regs.lamda_motion_sse, &asic->regs.lambda_motionSAD,
                   type, qp_factor, is_depth0, asic->regs.asicHwId);
      InterLamdaQp(asic->regs.qp - asic->regs.roi1DeltaQp, &asic->regs.lamda_motion_sse_roi1,
                   &asic->regs.lambda_motionSAD_ROI1, type,
                   qp_factor, is_depth0, asic->regs.asicHwId);
      InterLamdaQp(asic->regs.qp - asic->regs.roi2DeltaQp, &asic->regs.lamda_motion_sse_roi2,
                   &asic->regs.lambda_motionSAD_ROI2, type,
                   qp_factor, is_depth0, asic->regs.asicHwId);
#else
      u32 qpFactorSad = (u32)((qp_factor * (1<<QPFACTOR_FIX_POINT)) + 0.5f);
      u32 qpFactorSse = (u32)((qp_factor * qp_factor * (1<<QPFACTOR_FIX_POINT)) + 0.5f);
      InterLamdaQpFixPoint(asic->regs.qp, &asic->regs.lamda_motion_sse, &asic->regs.lambda_motionSAD,
                   type, qpFactorSad, qpFactorSse, is_depth0, asic->regs.asicHwId);
      InterLamdaQpFixPoint(asic->regs.qp - asic->regs.roi1DeltaQp, &asic->regs.lamda_motion_sse_roi1,
                   &asic->regs.lambda_motionSAD_ROI1, type,
                   qpFactorSad, qpFactorSse, is_depth0, asic->regs.asicHwId);
      InterLamdaQpFixPoint(asic->regs.qp - asic->regs.roi2DeltaQp, &asic->regs.lamda_motion_sse_roi2,
                   &asic->regs.lambda_motionSAD_ROI2, type,
                   qpFactorSad, qpFactorSse, is_depth0, asic->regs.asicHwId);
#endif

      asic->regs.lamda_SAO_luma = asic->regs.lamda_motion_sse;
      asic->regs.lamda_SAO_chroma = 0.75 * asic->regs.lamda_motion_sse;

      break;
    case 2: /* H2V2 rev01 */
    case 3: /* H2V3 rev01 */
      LamdaTableQp(&asic->regs, asic->regs.qp, type, asic->regs.poc,
                   qp_factor, is_depth0, enable_ctu_rc);
      FillIntraFactor(&asic->regs);
      asic->regs.lamda_SAO_luma = asic->regs.lambda_sse_me[0];
      asic->regs.lamda_SAO_chroma = 0.75 * asic->regs.lambda_sse_me[0];
      break;
    case 4: /* H2V4 rev01 */
      LamdaTableQp(&asic->regs, asic->regs.qp, type, asic->regs.poc,
                   qp_factor, is_depth0, enable_ctu_rc);
      FillIntraFactor(&asic->regs);
      asic->regs.lamda_SAO_luma = asic->regs.lambda_sse_me[0];
      asic->regs.lamda_SAO_chroma = 0.75 * asic->regs.lambda_sse_me[0];
      break;
    case 5: /* H2V5 rev01 */
    case 6: /* H2V6 rev01 */
    case 7: /* H2V7 rev01 */
    case 0x60: /* VC8000E 6.0 */
    case 0x61: /* VC8000E 6.1 */
    case 0x62:
    case 0x81:
    case 0x82:
    default:
      LamdaTableQp(&asic->regs, asic->regs.qp, type, asic->regs.poc,
                   qp_factor, is_depth0, enable_ctu_rc);
      FillIntraFactor(&asic->regs);
      asic->regs.lamda_SAO_luma = asic->regs.lambda_sse_me[0];
      asic->regs.lamda_SAO_chroma = 0.75 * asic->regs.lambda_sse_me[0];
      // sao lambda registers are not expanded
      asic->regs.lamda_SAO_luma >>= MOTION_LAMBDA_SSE_SHIFT;
      asic->regs.lamda_SAO_chroma >>= MOTION_LAMBDA_SSE_SHIFT;
      break;
  }

  {
    u32 maxSaoLambda = (1<<14)-1;
    asic->regs.lamda_SAO_luma = MIN(asic->regs.lamda_SAO_luma, maxSaoLambda);
    asic->regs.lamda_SAO_chroma = MIN(asic->regs.lamda_SAO_chroma, maxSaoLambda);
  }
}

/*------------------------------------------------------------------------------

    Set encoding parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
void VCEncSetNewFrame(VCEncInst inst)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  asicData_s *asic = &vcenc_instance->asic;
  regValues_s *regs = &vcenc_instance->asic.regs;
  i32 qpHdr, qpMin, qpMax;
  i32 qp[4];
  i32 s, i, aroiDeltaQ = 0, tmp;

  regs->outputStrmSize[0] -= vcenc_instance->stream.byteCnt;
  /* stream base address */
  regs->outputStrmBase[0] += vcenc_instance->stream.byteCnt;

  /* Enable slice ready interrupts if defined by config and slices in use */
  regs->sliceReadyInterrupt =
    ENCH2_SLICE_READY_INTERRUPT & (regs->sliceNum > 1);

  /* HW base address for NAL unit sizes is affected by start offset
   * and SW created NALUs. */
  regs->sizeTblBase = asic->sizeTbl[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].busAddress + vcenc_instance->numNalus[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum] * 4;


  /* HW Base must be 64-bit aligned */
  //ASSERT(regs->sizeTblBase%8 == 0);

  /* low latency: configure related register.*/
  regs->lineBufferEn = vcenc_instance->inputLineBuf.inputLineBufEn;
  regs->lineBufferHwHandShake = vcenc_instance->inputLineBuf.inputLineBufHwModeEn;
  regs->lineBufferLoopBackEn = vcenc_instance->inputLineBuf.inputLineBufLoopBackEn;
  regs->lineBufferDepth = vcenc_instance->inputLineBuf.inputLineBufDepth;
  regs->amountPerLoopBack = vcenc_instance->inputLineBuf.amountPerLoopBack;
  regs->mbWrPtr = vcenc_instance->inputLineBuf.wrCnt;
  regs->mbRdPtr = 0;
  regs->lineBufferInterruptEn = ENCH2_INPUT_BUFFER_INTERRUPT &
                                regs->lineBufferEn &
                                (regs->lineBufferHwHandShake == 0) &
                                (regs->lineBufferDepth > 0);
}

/* DeNoise Filter */
static void VCEncHEVCDnfInit(struct vcenc_instance *vcenc_instance)
{
  vcenc_instance->uiNoiseReductionEnable = 0;

#if USE_TOP_CTRL_DENOISE
  vcenc_instance->iNoiseL = 10 << FIX_POINT_BIT_WIDTH;
  vcenc_instance->iSigmaCur = vcenc_instance->iFirstFrameSigma = 11 << FIX_POINT_BIT_WIDTH;
  //printf("init seq noiseL = %d, SigCur = %d, first = %d\n\n\n\n\n",pEncInst->iNoiseL, regs->nrSigmaCur,  pCodeParams->firstFrameSigma);
#endif
}

static void VCEncHEVCDnfSetParameters(struct vcenc_instance *inst, const VCEncCodingCtrl *pCodeParams)
{
  regValues_s *regs = &inst->asic.regs;

  //wiener denoise paramter set
  regs->noiseReductionEnable = inst->uiNoiseReductionEnable = pCodeParams->noiseReductionEnable;//0: disable noise reduction; 1: enable noise reduction
  regs->noiseLow = pCodeParams->noiseLow;//0: use default value; valid value range: [1, 30]

#if USE_TOP_CTRL_DENOISE
  inst->iNoiseL = pCodeParams->noiseLow << FIX_POINT_BIT_WIDTH;
  regs->nrSigmaCur = inst->iSigmaCur = inst->iFirstFrameSigma = pCodeParams->firstFrameSigma << FIX_POINT_BIT_WIDTH;
  //printf("init seq noiseL = %d, SigCur = %d, first = %d\n\n\n\n\n",pEncInst->iNoiseL, regs->nrSigmaCur,  pCodeParams->firstFrameSigma);
#endif
}

static void VCEncHEVCDnfGetParameters(struct vcenc_instance *inst, VCEncCodingCtrl *pCodeParams)
{
  pCodeParams->noiseReductionEnable = inst->uiNoiseReductionEnable;
#if USE_TOP_CTRL_DENOISE
  pCodeParams->noiseLow =  inst->iNoiseL >> FIX_POINT_BIT_WIDTH;
  pCodeParams->firstFrameSigma = inst->iFirstFrameSigma >> FIX_POINT_BIT_WIDTH;
#endif
}

// run before HW run, set register's value according to coding settings
static void VCEncHEVCDnfPrepare(struct vcenc_instance *vcenc_instance)
{
  asicData_s *asic = &vcenc_instance->asic;
#if USE_TOP_CTRL_DENOISE
  asic->regs.nrSigmaCur = vcenc_instance->iSigmaCur;
  asic->regs.nrThreshSigmaCur = vcenc_instance->iThreshSigmaCur;
  asic->regs.nrSliceQPPrev = vcenc_instance->iSliceQPPrev;
#endif
}

// run after HW finish one frame, update collected parameters
static void VCEncHEVCDnfUpdate(struct vcenc_instance *vcenc_instance)
{
#if USE_TOP_CTRL_DENOISE
  const int KK=102;
  unsigned int j = 0;
  int CurSigmaTmp = 0;
  unsigned int SigmaSmoothFactor[5] = {1024, 512, 341, 256, 205};
  int dSumFrmNoiseSigma = 0;
  int QpSlc = vcenc_instance->asic.regs.qp;
  int FrameCodingType = vcenc_instance->asic.regs.frameCodingType;
  unsigned int uiFrmNum = vcenc_instance->uiFrmNum++;

  // check pre-conditions
  if (vcenc_instance->uiNoiseReductionEnable == 0 || vcenc_instance->stream.byteCnt == 0)
    return;

  if (uiFrmNum == 0)
    vcenc_instance->FrmNoiseSigmaSmooth[0] = vcenc_instance->iFirstFrameSigma; //(pvcenc_instance->asic.regs.firstFrameSigma<< FIX_POINT_BIT_WIDTH);//pvcenc_instance->iFirstFrmSigma;//(double)((51-QpSlc-5)/2);
  int iFrmSigmaRcv = vcenc_instance->iSigmaCalcd;
  //int iThSigmaRcv = vcenc_instance->iThreshSigmaCalcd;
  int iThSigmaRcv = (FrameCodingType != 1) ? vcenc_instance->iThreshSigmaCalcd : vcenc_instance->iThreshSigmaPrev;
  //printf("iFrmSigRcv = %d\n\n\n", iFrmSigmaRcv);
  if (iFrmSigmaRcv == 0xFFFF)
  {
    iFrmSigmaRcv = vcenc_instance->iFirstFrameSigma;
    //printf("initial to %d\n", iFrmSigmaRcv);
  }
  iFrmSigmaRcv = (iFrmSigmaRcv * KK) >> 7;
  if (iFrmSigmaRcv < vcenc_instance->iNoiseL) iFrmSigmaRcv = 0;
  vcenc_instance->FrmNoiseSigmaSmooth[(uiFrmNum+1)%SIGMA_SMOOTH_NUM] = iFrmSigmaRcv;
  for (j = 0; j < (MIN(SIGMA_SMOOTH_NUM - 1, uiFrmNum + 1) + 1); j++)
  {
    dSumFrmNoiseSigma += vcenc_instance->FrmNoiseSigmaSmooth[j];
    //printf("FrmNoiseSig %d = %d\n", j, pvcenc_instance->FrmNoiseSigmaSmooth[j]);
  }
  CurSigmaTmp = (dSumFrmNoiseSigma * SigmaSmoothFactor[MIN(SIGMA_SMOOTH_NUM-1, uiFrmNum+1)]) >> 10;
  vcenc_instance->iSigmaCur = CLIP3(0, (SIGMA_MAX << FIX_POINT_BIT_WIDTH), CurSigmaTmp);
  vcenc_instance->iThreshSigmaCur = vcenc_instance->iThreshSigmaPrev = iThSigmaRcv;
  vcenc_instance->iSliceQPPrev = QpSlc;
  //printf("TOP::uiFrmNum = %d, FrmSig=%d, Th=%d, QP=%d, QP2=%d\n", uiFrmNum,pvcenc_instance->asic.regs.nrSigmaCur, iThSigmaRcv, QpSlc, QpSlc2 );
#endif
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetEncodedMbLines
    Description   : Get how many MB lines has been encoded by encoder.
    Return type   : int
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 VCEncGetEncodedMbLines(VCEncInst inst)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  u32 lines;

  APITRACE("VCEncGetEncodedMbLines#");

  /* Check for illegal inputs */
  if (!vcenc_instance) {
    APITRACE("VCEncGetEncodedMbLines: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  if (!vcenc_instance->inputLineBuf.inputLineBufEn) {
    APITRACE("VCEncGetEncodedMbLines: ERROR Invalid mode for input control");
    return VCENC_INVALID_ARGUMENT;
  }
  lines = EncAsicGetRegisterValue(vcenc_instance->asic.ewl, vcenc_instance->asic.regs.regMirror, HWIF_CTB_ROW_RD_PTR);

  return lines;
}

/*------------------------------------------------------------------------------
    Function name : VCEncSetInputMBLines
    Description   : Set the input buffer lines available of current picture.
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : mbNum - macroblock number
------------------------------------------------------------------------------*/
VCEncRet VCEncSetInputMBLines(VCEncInst inst, u32 lines)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

  APITRACE("VCEncSetInputMBLines#");

  /* Check for illegal inputs */
  if (!vcenc_instance) {
    APITRACE("VCEncSetInputMBLines: ERROR Null argument");
    return VCENC_NULL_ARGUMENT;
  }

  if (!vcenc_instance->inputLineBuf.inputLineBufEn) {
    APITRACE("VCEncSetInputMBLines: ERROR Invalid mode for input control");
    return VCENC_INVALID_ARGUMENT;
  }
  EncAsicWriteRegisterValue(vcenc_instance->asic.ewl, vcenc_instance->asic.regs.regMirror, HWIF_CTB_ROW_WR_PTR, lines);
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
Function name : VCEncGenSliceHeaderRps
Description   : cal RPS parameter in the slice header.
Return type   : void
Argument      : VCEncPictureCodingType codingType
Argument      : VCEncGopPicConfig *cfg
------------------------------------------------------------------------------*/
void VCEncGenSliceHeaderRps(struct vcenc_instance *vcenc_instance, VCEncPictureCodingType codingType, VCEncGopPicConfig *cfg)
{
	regValues_s *regs = &vcenc_instance->asic.regs;
	HWRPS_CONFIG *pHwRps = &vcenc_instance->sHwRps;
	i32 i, j, tmp, i32TempPoc;
	i32 i32NegDeltaPoc[VCENC_MAX_REF_FRAMES], i32PosDeltaPoc[VCENC_MAX_REF_FRAMES];
	i32 i32NegDeltaPocUsed[VCENC_MAX_REF_FRAMES], i32PosDeltaPocUsed[VCENC_MAX_REF_FRAMES];
	u32 used_by_cur;

	ASSERT(cfg != NULL);
	ASSERT(codingType < VCENC_NOTCODED_FRAME);

	for (i = 0; i < VCENC_MAX_REF_FRAMES; i++)
	{
		pHwRps->u20DeltaPocS0[i] = 0;
		pHwRps->u20DeltaPocS1[i] = 0;

		i32NegDeltaPoc[i] = 0;
		i32PosDeltaPoc[i] = 0;
		i32NegDeltaPocUsed[i] = 0;
		i32PosDeltaPocUsed[i] = 0;
	}

	pHwRps->u3NegPicNum = 0;
	pHwRps->u3PosPicNum = 0;

	for (i = 0; i < (i32)cfg->numRefPics; i++)
	{
		if (cfg->refPics[i].ref_pic < 0)
		{
			i32NegDeltaPoc[pHwRps->u3NegPicNum]	   = cfg->refPics[i].ref_pic;
			i32NegDeltaPocUsed[pHwRps->u3NegPicNum] = cfg->refPics[i].used_by_cur;

			pHwRps->u3NegPicNum++;
		}
		else
		{
			i32PosDeltaPoc[pHwRps->u3PosPicNum]	   = cfg->refPics[i].ref_pic;
			i32PosDeltaPocUsed[pHwRps->u3PosPicNum] = cfg->refPics[i].used_by_cur;

			pHwRps->u3PosPicNum++;
		}
	}

	for (i = 0; i < pHwRps->u3NegPicNum; i++)
	{
		i32TempPoc = i32NegDeltaPoc[i];
		for (j = i; j < (i32)pHwRps->u3NegPicNum; j++)
		{
			if (i32NegDeltaPoc[j] > i32TempPoc)
			{
				i32NegDeltaPoc[i] = i32NegDeltaPoc[j];
				i32NegDeltaPoc[j] = i32TempPoc;
				i32TempPoc = i32NegDeltaPoc[i];

				used_by_cur = i32NegDeltaPocUsed[i];
				i32NegDeltaPocUsed[i] = i32NegDeltaPocUsed[j];
				i32NegDeltaPocUsed[j] = used_by_cur;
			}
		}
	}

	for (i = 0; i < pHwRps->u3PosPicNum; i++)
	{
		i32TempPoc = i32PosDeltaPoc[i];
		for (j = i; j < (i32)pHwRps->u3PosPicNum; j++)
		{
			if (i32PosDeltaPoc[j] < i32TempPoc)
			{
				i32PosDeltaPoc[i] = i32PosDeltaPoc[j];
				i32PosDeltaPoc[j] = i32TempPoc;
				i32TempPoc = i32PosDeltaPoc[i];

				used_by_cur = i32PosDeltaPocUsed[i];
				i32PosDeltaPocUsed[i] = i32PosDeltaPocUsed[j];
				i32PosDeltaPocUsed[j] = used_by_cur;
			}
		}
	}

	tmp = 0;
	for ( i = 0; i < pHwRps->u3NegPicNum; i++)
	{
		ASSERT((-i32NegDeltaPoc[i] - 1 + tmp) < 0x100000);
		pHwRps->u1DeltaPocS0Used[i] = i32NegDeltaPocUsed[i] & 0x01;
		pHwRps->u20DeltaPocS0[i] = -i32NegDeltaPoc[i] - 1 + tmp;
		tmp = i32NegDeltaPoc[i];
	}
	tmp = 0;
	for ( i = 0; i < pHwRps->u3PosPicNum; i++)
	{
		ASSERT((i32PosDeltaPoc[i] - 1 - tmp) < 0x100000);
		pHwRps->u1DeltaPocS1Used[i] = i32PosDeltaPocUsed[i] & 0x01;
		pHwRps->u20DeltaPocS1[i] = i32PosDeltaPoc[i] - 1 - tmp;
		tmp = i32PosDeltaPoc[i];
	}

	regs->short_term_ref_pic_set_sps_flag = pHwRps->u1short_term_ref_pic_set_sps_flag;
	regs->rps_neg_pic_num = pHwRps->u3NegPicNum;
	regs->rps_pos_pic_num = pHwRps->u3PosPicNum;
	ASSERT(pHwRps->u3NegPicNum + pHwRps->u3PosPicNum <= VCENC_MAX_REF_FRAMES);
	for ( i = 0, j = 0; i < pHwRps->u3NegPicNum; i++, j++)
	{
		regs->rps_delta_poc[j] = pHwRps->u20DeltaPocS0[i];
		regs->rps_used_by_cur[j] = pHwRps->u1DeltaPocS0Used[i];
	}
	for ( i = 0; i < pHwRps->u3PosPicNum; i++, j++)
	{
		regs->rps_delta_poc[j] = pHwRps->u20DeltaPocS1[i];
		regs->rps_used_by_cur[j] = pHwRps->u1DeltaPocS1Used[i];
	}
	for (; j < VCENC_MAX_REF_FRAMES; j++)
	{
		regs->rps_delta_poc[j] = 0;
		regs->rps_used_by_cur[j] = 0;
	}

}

/*------------------------------------------------------------------------------
Function name : VCEncGenPicRefConfig
Description   : cal the ref pic of current pic.
Return type   : void
Argument      : struct container *c
Argument      : VCEncGopPicConfig *cfg
Argument      : struct sw_picture *pCurPic
Argument      : i32 curPicPoc
------------------------------------------------------------------------------*/
void VCEncGenPicRefConfig(struct container *c, VCEncGopPicConfig *cfg, struct sw_picture *pCurPic, i32 curPicPoc)
{
	struct sw_picture *p;
	struct node *n;
	i32 i, j;
	i32 i32PicPoc[VCENC_MAX_REF_FRAMES];

	ASSERT(c != NULL);
	ASSERT(cfg != NULL);
	ASSERT(pCurPic != NULL);
	ASSERT(curPicPoc >= 0);

	for (n = c->picture.tail; n; n = n->next)
	{
		p = (struct sw_picture *)n;
		if ((p->long_term_flag == HANTRO_FALSE) && (p->reference == HANTRO_TRUE) && (p->poc > -1))
		{
			i32PicPoc[cfg->numRefPics] = p->poc;  /* looking short_term ref */
			cfg->numRefPics++;
		}
	}

	for (i = 0; i < (i32)cfg->numRefPics; i++)
	{
		cfg->refPics[i].used_by_cur = 0;

		for (j = 0; j < pCurPic->sliceInst->active_l0_cnt; j++)
		{
			ASSERT(pCurPic->rpl[0][j]->long_term_flag == HANTRO_FALSE );
			if (pCurPic->rpl[0][j]->poc == i32PicPoc[i])
			{
				cfg->refPics[i].used_by_cur = 1;
				break;
			}
		}

		if (j == pCurPic->sliceInst->active_l0_cnt)
		{
			for (j = 0; j < pCurPic->sliceInst->active_l1_cnt; j++)
			{
				ASSERT(pCurPic->rpl[1][j]->long_term_flag == HANTRO_FALSE);
				if (pCurPic->rpl[1][j]->poc == i32PicPoc[i])
				{
					cfg->refPics[i].used_by_cur = 1;
					break;
				}
			}
		}
	}

	for (i = 0; i < (i32)cfg->numRefPics; i++)
	{
		cfg->refPics[i].ref_pic = i32PicPoc[i] - curPicPoc;
	}
}

/*------------------------------------------------------------------------------

    API tracing
        TRacing as defined by the API.
    Params:
        msg - null terminated tracing message
------------------------------------------------------------------------------*/
void VCEncTrace(const char *msg)
{
    static FILE *fp = NULL;

    if(fp == NULL)
        fp = fopen("api.trc", "wt");

    if(fp)
        fprintf(fp, "%s\n", msg);
}

/*------------------------------------------------------------------------------
    Function name : VCEncTraceProfile
    Description   : Tracing PSNR profile result
    Return type   : void
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
void VCEncTraceProfile(VCEncInst inst)
{
	ASSERT(inst != NULL);
	struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
	EWLTraceProfile(vcenc_instance->asic.ewl);
}


/*------------------------------------------------------------------------------
  next_picture calculates next input picture depending input and output
  frame rates.
------------------------------------------------------------------------------*/
static u64 next_picture(VCEncGopConfig *cfg, int picture_cnt)
{
  u64 numer, denom;

  numer = (u64)cfg->inputRateNumer * (u64)cfg->outputRateDenom;
  denom = (u64)cfg->inputRateDenom * (u64)cfg->outputRateNumer;

  return numer * (picture_cnt / (1 << cfg->interlacedFrame)) / denom;
}

/*------------------------------------------------------------------------------
Function name : GenNextPicConfig
Description   : generate the pic reference configure befor one picture encoded
Return type   : void
Argument      : VCEncIn *pEncIn
Argument      : u8 *gopCfgOffset
Argument      : i32 codecH264
Argument      : i32 i32LastPicPoc
------------------------------------------------------------------------------*/
static void GenNextPicConfig(VCEncIn *pEncIn, const u8 *gopCfgOffset, i32 codecH264, i32 i32LastPicPoc)
{
    i32 i, j, k, i32Poc, i32LTRIdx, numRefPics, numRefPics_org=0;
    u8 u8CfgStart, u8IsLTR_ref, u8IsUpdated;
    i32 i32MaxpicOrderCntLsb = 1 << 16;

    ASSERT(pEncIn != NULL);
    ASSERT(gopCfgOffset != NULL);

    u8CfgStart = gopCfgOffset[pEncIn->gopSize];
    memcpy(&pEncIn->gopCurrPicConfig, &(pEncIn->gopConfig.pGopPicCfg[u8CfgStart + pEncIn->gopPicIdx]), sizeof(VCEncGopPicConfig));

    pEncIn->i8SpecialRpsIdx = -1;
    pEncIn->i8SpecialRpsIdx_next = -1;
    memset(pEncIn->bLTR_used_by_cur, 0, sizeof(bool)*VCENC_MAX_LT_REF_FRAMES);
    if (0 == pEncIn->gopConfig.special_size)
        return;

    /* update ltr */
    i32 i32RefIdx;
    for (i32RefIdx = 0; i32RefIdx < pEncIn->gopConfig.ltrcnt; i32RefIdx++)
    {
        if (HANTRO_TRUE == pEncIn->bLTR_need_update[i32RefIdx])
            pEncIn->long_term_ref_pic[i32RefIdx] = i32LastPicPoc;
    }

    memset(pEncIn->bLTR_need_update, 0, sizeof(bool)*pEncIn->gopConfig.ltrcnt);
    if (0 != pEncIn->bIsIDR)
    {
        i32Poc = 0;
        pEncIn->gopCurrPicConfig.temporalId = 0;
        for (i = 0; i < pEncIn->gopConfig.ltrcnt; i++)
            pEncIn->long_term_ref_pic[i]  = INVALITED_POC;
    }
    else
        i32Poc = pEncIn->poc;
    /* check the current picture encoded as LTR*/
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
            if (i32Poc > (i32MaxpicOrderCntLsb >> 1))
                i32Poc = -1;
        }

        if ((i32Poc >= 0)&& (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
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
                    pEncIn->gopCurrPicConfig.refPics[k].ref_pic     = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
                    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
                }
            }

            pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
            pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
        }
    }

    if (0 != pEncIn->bIsIDR)
        return;

    u8IsUpdated = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++)
    {
        if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0)
            continue;

        /* no changed */
        if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) && ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED)
            && (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) && (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
            && (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED))
            continue;

        /* only consider LTR ref config */
        if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED))
        {
            /* reserved for later */
            pEncIn->i8SpecialRpsIdx = -1;
            u8IsUpdated                 = 1;
            continue;
        }

        if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == VCENC_INTRA_FRAME)&&(pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == 0))
            numRefPics = 1;
        else
            numRefPics = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;

        numRefPics_org = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
        for (k = 0; k < numRefPics; k++)
        {
            u8IsLTR_ref = IS_LONG_TERM_REF_DELTAPOC(pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic);
            if ((u8IsLTR_ref == HANTRO_FALSE) && (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
                continue;
            i32LTRIdx = (u8IsLTR_ref == HANTRO_TRUE)?LONG_TERM_REF_DELTAPOC2ID(pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic):0;

            if ((pEncIn->long_term_ref_pic[i32LTRIdx] == INVALITED_POC)&&(u8IsLTR_ref != HANTRO_FALSE) && (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
                continue;

            if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change != 0)
                i32Poc = pEncIn->poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;
            else
                i32Poc = pEncIn->poc - pEncIn->long_term_ref_pic[i32LTRIdx] - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

            if (i32Poc < 0)
            {
                i32Poc += i32MaxpicOrderCntLsb;
                if (i32Poc > (i32MaxpicOrderCntLsb >> 1))
                   i32Poc = -1;
            }

            if ((i32Poc >= 0) && (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
            {
                pEncIn->gopCurrPicConfig.codingType = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) ? pEncIn->gopCurrPicConfig.codingType : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
                pEncIn->gopCurrPicConfig.numRefPics = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED) ? pEncIn->gopCurrPicConfig.numRefPics : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
                pEncIn->gopCurrPicConfig.QpFactor = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) ? pEncIn->gopCurrPicConfig.QpFactor : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
                pEncIn->gopCurrPicConfig.QpOffset = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED) ? pEncIn->gopCurrPicConfig.QpOffset : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
                pEncIn->gopCurrPicConfig.temporalId = (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED) ? pEncIn->gopCurrPicConfig.temporalId : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

                if ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics != NUMREFPICS_RESERVED)
                {
                    for (i = 0; i < (i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics; i++)
                    {
                        pEncIn->gopCurrPicConfig.refPics[i].ref_pic = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[i].ref_pic;
                        pEncIn->gopCurrPicConfig.refPics[i].used_by_cur = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[i].used_by_cur;
                    }
                }

                pEncIn->i8SpecialRpsIdx = j;

                u8IsUpdated = 1;
            }

            if (0 != u8IsUpdated)
                break;
        }

        if(0 != u8IsUpdated)
            break;
    }

    if (codecH264)
    {
        u8IsUpdated = 0;
        for (j = 0; j < pEncIn->gopConfig.special_size; j++)
        {
            if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0)
                continue;

            /* no changed */
            if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) && ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED)
                && (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) && (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
                && (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED))
                continue;

            /* only consider LTR ref config */
            if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED))
            {
                /* reserved for later */
                pEncIn->i8SpecialRpsIdx_next = -1;
                u8IsUpdated = 1;
                continue;
            }

            if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == VCENC_INTRA_FRAME) && (pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == 0))
                numRefPics = 1;
            else
                numRefPics = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
            for (k = 0; k < numRefPics; k++)
            {
                u8IsLTR_ref = IS_LONG_TERM_REF_DELTAPOC(pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic);
                if ((u8IsLTR_ref == HANTRO_FALSE) && (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
                    continue;
                i32LTRIdx = (u8IsLTR_ref == HANTRO_TRUE) ? LONG_TERM_REF_DELTAPOC2ID(pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic) : 0;

                if ((pEncIn->long_term_ref_pic[i32LTRIdx] == INVALITED_POC) && (u8IsLTR_ref != HANTRO_FALSE) && (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
                    continue;

                if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change != 0)
                    i32Poc = pEncIn->poc + pEncIn->gopConfig.delta_poc_to_next - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;
                else
                    i32Poc = pEncIn->poc + pEncIn->gopConfig.delta_poc_to_next - pEncIn->long_term_ref_pic[i32LTRIdx] - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

                if (i32Poc < 0)
                {
                    i32Poc += i32MaxpicOrderCntLsb;
                    if (i32Poc >(i32MaxpicOrderCntLsb >> 1))
                        i32Poc = -1;
                }

                if ((i32Poc >= 0) && (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
                {
                    pEncIn->i8SpecialRpsIdx_next = j;
                    u8IsUpdated = 1;
                }

                if (0 != u8IsUpdated)
                    break;
            }

            if (0 != u8IsUpdated)
                break;
        }
    }

    if (pEncIn->gopCurrPicConfig.codingType == VCENC_INTRA_FRAME)
        pEncIn->gopCurrPicConfig.numRefPics = numRefPics_org;


    j = 0;
    for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++)
    {
        if (0 != pEncIn->gopCurrPicConfig.refPics[k].used_by_cur)
            j++;

        u8IsLTR_ref = IS_LONG_TERM_REF_DELTAPOC(pEncIn->gopCurrPicConfig.refPics[k].ref_pic);
        if (u8IsLTR_ref == HANTRO_FALSE)
            continue;
        i32LTRIdx = LONG_TERM_REF_DELTAPOC2ID(pEncIn->gopCurrPicConfig.refPics[k].ref_pic);

        pEncIn->bLTR_used_by_cur[i32LTRIdx] = pEncIn->gopCurrPicConfig.refPics[k].used_by_cur? HANTRO_TRUE:HANTRO_FALSE;
    }

    if (j > 1)
        pEncIn->gopCurrPicConfig.codingType = VCENC_BIDIR_PREDICTED_FRAME;
}

/*------------------------------------------------------------------------------
    Function name : VCEncFindNextPic
    Description   : get gopSize/rps for next frame
    Return type   : void
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
VCEncPictureCodingType VCEncFindNextPic (VCEncInst inst, VCEncIn *encIn, i32 nextGopSize, const u8 *gopCfgOffset, bool forceIDR)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  VCEncPictureCodingType nextCodingType;
  int idx, offset, cur_poc, delta_poc_to_next;
  bool bIsCodingTypeChanged;
  int next_gop_size = nextGopSize;
  VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(encIn->gopConfig));
  i32 *p_picture_cnt = &encIn->picture_cnt;
  i32 last_idr_picture_cnt = encIn->last_idr_picture_cnt;
  int picture_cnt_tmp = *p_picture_cnt;
  i32 i32LastPicPoc;

  //get current poc within GOP
  if (encIn->codingType == VCENC_INTRA_FRAME && (encIn->poc == 0))
  {
    // last is an IDR
    cur_poc = 0;
    encIn->gopPicIdx = 0;
  }
  else
  {
    //Update current idx and poc within a GOP
    idx = encIn->gopPicIdx + gopCfgOffset[encIn->gopSize];
    cur_poc = gopCfg->pGopPicCfg[idx].poc;
    encIn->gopPicIdx = (encIn->gopPicIdx + 1) % encIn->gopSize;
    if (encIn->gopPicIdx == 0)
      cur_poc -= encIn->gopSize;
  }

  //a GOP end, to start next GOP
  if (encIn->gopPicIdx == 0)
    offset = gopCfgOffset[next_gop_size];
  else
    offset = gopCfgOffset[encIn->gopSize];

  //get next poc within GOP, and delta_poc
  idx = encIn->gopPicIdx + offset;
  delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;
  //next picture cnt
  *p_picture_cnt = picture_cnt_tmp + delta_poc_to_next;

  //Handle Tail (seqence end or cut by an I frame)
  {
    //just finished a GOP and will jump to a P frame
    if (encIn->gopPicIdx == 0 && delta_poc_to_next > 1)
    {
       int gop_end_pic = *p_picture_cnt;
       int gop_shorten = 0, gop_shorten_idr = 0, gop_shorten_tail = 0;

       //cut  by an IDR
       if ((gopCfg->idr_interval) && ((gop_end_pic - last_idr_picture_cnt) >= gopCfg->idr_interval) && !gopCfg->gdrDuration)
          gop_shorten_idr = 1 + ((gop_end_pic - last_idr_picture_cnt) - gopCfg->idr_interval);

       //handle sequence tail
       while (((next_picture(gopCfg, gop_end_pic --) + gopCfg->firstPic) > gopCfg->lastPic) &&
               (gop_shorten_tail < next_gop_size - 1))
         gop_shorten_tail ++;

       gop_shorten = gop_shorten_idr > gop_shorten_tail ? gop_shorten_idr : gop_shorten_tail;

       if (gop_shorten >= next_gop_size)
       {
         //for gopsize = 1
         *p_picture_cnt = picture_cnt_tmp + 1 - cur_poc;
       }
       else if (gop_shorten > 0)
       {
         //reduce gop size
         const int max_reduced_gop_size = gopCfg->gopLowdelay ? 1 : 4;
         next_gop_size -= gop_shorten;
         if (next_gop_size > max_reduced_gop_size)
           next_gop_size = max_reduced_gop_size;

         idx = gopCfgOffset[next_gop_size];
         delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;
         *p_picture_cnt = picture_cnt_tmp + delta_poc_to_next;
       }
       encIn->gopSize = next_gop_size;
    }

    i32LastPicPoc = encIn->poc;
    encIn->poc += *p_picture_cnt - picture_cnt_tmp;
    bIsCodingTypeChanged = HANTRO_FALSE;
    //next coding type
    bool forceIntra = (gopCfg->idr_interval && ((*p_picture_cnt - last_idr_picture_cnt) >= gopCfg->idr_interval)) || forceIDR;
    if (forceIntra)
    {
      nextCodingType = VCENC_INTRA_FRAME;
      encIn->bIsIDR = HANTRO_TRUE;
      bIsCodingTypeChanged = HANTRO_TRUE;
    }
    else
    {
      encIn->bIsIDR = HANTRO_FALSE;
      idx = encIn->gopPicIdx + gopCfgOffset[encIn->gopSize];
      nextCodingType = gopCfg->pGopPicCfg[idx].codingType;
    }
  }
  gopCfg->id = encIn->gopPicIdx + gopCfgOffset[encIn->gopSize];
  {
    // guess next rps needed for H.264 DPB management (MMO), assume gopSize unchanged.
    // gopSize change only occurs on adaptive GOP or tail GOP (lowdelay = 0).
    // then the next RPS is 1st of default RPS of some gopSize, which only includes the P frame of last GOP
    i32 next_poc = gopCfg->pGopPicCfg[gopCfg->id].poc;
    i32 gopPicIdx = (encIn->gopPicIdx + 1) % encIn->gopSize;
    i32 gopSize = encIn->gopSize;
    if (gopPicIdx == 0)
    {
      next_poc -= gopSize;
    }
    gopCfg->id_next = gopPicIdx + gopCfgOffset[gopSize];
    gopCfg->delta_poc_to_next = gopCfg->pGopPicCfg[gopCfg->id_next].poc - next_poc;

    if ((gopPicIdx == 0) && (gopCfg->delta_poc_to_next > 1) && (gopCfg->idr_interval && ((encIn->poc + gopCfg->delta_poc_to_next) >= gopCfg->idr_interval)))
    {
        i32 i32gopsize;
        i32gopsize = gopCfg->idr_interval - encIn->poc - 2;

        if (i32gopsize > 0)
        {
            int max_reduced_gop_size = gopCfg->gopLowdelay ? 1 : 4;
            if (i32gopsize > max_reduced_gop_size)
                i32gopsize = max_reduced_gop_size;

            idx = gopCfgOffset[i32gopsize];
            delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;

            gopCfg->id_next = gopPicIdx + gopCfgOffset[i32gopsize];
            gopCfg->delta_poc_to_next = gopCfg->pGopPicCfg[gopCfg->id_next].poc - next_poc;
        }
    }

    if(gopCfg->gdrDuration==0 && gopCfg->idr_interval  && (encIn->poc+gopCfg->delta_poc_to_next) % gopCfg->idr_interval == 0)
      gopCfg->id_next = -1;
  }

  if (vcenc_instance->lookaheadDepth == 0)
  {
    //Handle the first few frames for lowdelay
    if ((nextCodingType != VCENC_INTRA_FRAME))
    {
      int i;
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[gopCfg->id]);
      for (i = 0; i < cfg->numRefPics; i ++)
      {
        if ((encIn->poc + cfg->refPics[i].ref_pic) < 0)
        {
          int curCfgEnd = gopCfgOffset[encIn->gopSize] + encIn->gopSize;
          int cfgOffset = encIn->poc - 1;
          if ((curCfgEnd + cfgOffset) > gopCfg->size)
              cfgOffset = 0;

          gopCfg->id = curCfgEnd + cfgOffset;
          nextCodingType = gopCfg->pGopPicCfg[gopCfg->id].codingType;
          bIsCodingTypeChanged = HANTRO_TRUE;
          break;
        }
      }
    }
  }

  GenNextPicConfig(encIn, gopCfg->gopCfgOffset, IS_H264(vcenc_instance->codecFormat), i32LastPicPoc);
  if (bIsCodingTypeChanged == HANTRO_FALSE)
      nextCodingType = encIn->gopCurrPicConfig.codingType;
  if (nextCodingType == VCENC_INTRA_FRAME && ((encIn->poc == 0)|| (encIn->bIsIDR)))
  {
    // refresh IDR POC for !GDR
    if(!encIn->gopConfig.gdrDuration) encIn->poc = 0;
    encIn->last_idr_picture_cnt = encIn->picture_cnt;
  }

  encIn->codingType = (encIn->poc == 0) ? VCENC_INTRA_FRAME : nextCodingType;

  return nextCodingType;
}

static i32 tile_log2(i32 blk_size, i32 target) {
    i32 k;
    for (k = 0; (blk_size << k) < target; k++) {
    }
    return k;
}

/*------------------------------------------------------------------------------
  preserve_last3_av1 // 8762
------------------------------------------------------------------------------*/
void preserve_last3_av1(VCEncInst inst, struct container *c, const VCEncIn *pEncIn)
{
  struct sw_picture *p;
  struct node *n;
  i32 poc[8], poc_num, i , j;
  true_e bMatched;
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  VCENC_FrameUpdateType updateType = pEncIn->bIsIDR ? KF_UPDATE : vcenc_instance->av1_inst.upDateRefFrame[pEncIn->gopSize -1][pEncIn->gopPicIdx];

  vcenc_instance->av1_inst.is_preserve_last3_as_gld = ENCHW_NO;
  if(updateType != LF_UPDATE)
    return;

  /* pick up unreference poc */
  poc_num = 0;
  for (n = c->picture.tail; n; n = n->next)
  {
      p = (struct sw_picture *)n;
      if((p) && (p->reference == HANTRO_TRUE)){
          poc[poc_num] = p->poc;
          poc_num ++;
      }
  }

  /* secondly check if only in the LAST3 */
  for(j =0; j < poc_num; j++){
      if( poc[j] != vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST3_FRAME-SW_LAST_FRAME]].poc)
        continue;

      bMatched = ENCHW_NO;
      for (i = SW_LAST_FRAME; i < SW_REF_FRAMES; i++)
      {
          if((i != SW_LAST3_FRAME) && (poc[j] == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc)){
              bMatched = ENCHW_YES;
              break;
          }
      }
      if(bMatched == ENCHW_YES)
        continue;

      vcenc_instance->av1_inst.is_preserve_last3_as_gld = ENCHW_YES;
      break;
  }

  vcenc_instance->av1_inst.preserve_last3_as = COMMON_NO_UPDATE;
  if(vcenc_instance->av1_inst.is_preserve_last3_as_gld == ENCHW_YES){
      for( i = BRF_UPDATE; i >= GF_UPDATE; i--)
      {
          bMatched = ENCHW_NO;
          for(j = 0; j < pEncIn->gopSize; j++){
              if(i == vcenc_instance->av1_inst.upDateRefFrame[pEncIn->gopSize -1][j]){
                  bMatched = ENCHW_YES;
                  break;
              }
          }

          if(bMatched == ENCHW_NO){
              vcenc_instance->av1_inst.preserve_last3_as = i;
              break;
          }
      }
  }
}

// Define the reference buffers that will be updated post encode.
void av1_configure_buffer_updates(VCEncInst inst, VCEncIn *pEncIn) {
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    VCENC_FrameUpdateType updateType = pEncIn->bIsIDR ? KF_UPDATE : vcenc_instance->av1_inst.upDateRefFrame[pEncIn->gopSize -1][pEncIn->gopPicIdx];

    vcenc_instance->av1_inst.is_bwd_last_frame  = ENCHW_NO;
    vcenc_instance->av1_inst.is_arf_last_frame  = ENCHW_NO;
    vcenc_instance->av1_inst.is_arf2_last_frame = ENCHW_NO;
    vcenc_instance->av1_inst.refresh_last_frame     = 0;
    vcenc_instance->av1_inst.refresh_golden_frame   = 0;
    vcenc_instance->av1_inst.refresh_bwd_ref_frame  = 0;
    vcenc_instance->av1_inst.refresh_alt2_ref_frame = 0;
    vcenc_instance->av1_inst.refresh_alt_ref_frame  = 0;
    vcenc_instance->av1_inst.refresh_ext_ref_frame  = 0;

    if(pEncIn->u8IdxEncodedAsLTR)
        vcenc_instance->av1_inst.refresh_golden_frame   = 1;

    switch (updateType) {
    case KF_UPDATE:
        vcenc_instance->av1_inst.refresh_last_frame     = 1;
        vcenc_instance->av1_inst.refresh_golden_frame   = 1;
        vcenc_instance->av1_inst.refresh_bwd_ref_frame  = 1;
        vcenc_instance->av1_inst.refresh_alt2_ref_frame = 1;
        vcenc_instance->av1_inst.refresh_alt_ref_frame  = 1;
        break;

    case LF_UPDATE:
        vcenc_instance->av1_inst.refresh_last_frame     = 1;
        break;

    case ARF_UPDATE:
        vcenc_instance->av1_inst.refresh_alt_ref_frame  = 1;
        break;

    case ARF2_UPDATE:
        vcenc_instance->av1_inst.refresh_alt2_ref_frame = 1;
        break;

    case BRF_UPDATE:
        vcenc_instance->av1_inst.refresh_bwd_ref_frame  = 1;
        break;

    case COMMON_NO_UPDATE:
        break;

    case BWD_TO_LAST_UPDATE:
        vcenc_instance->av1_inst.refresh_last_frame     = 1;
        vcenc_instance->av1_inst.is_bwd_last_frame      = ENCHW_YES;
        break;

    case ARF_TO_LAST_UPDATE:
        vcenc_instance->av1_inst.refresh_last_frame     = 1;
        vcenc_instance->av1_inst.is_arf_last_frame      = ENCHW_YES;
        break;

    case ARF2_TO_LAST_UPDATE:
        vcenc_instance->av1_inst.refresh_last_frame     = 1;
        vcenc_instance->av1_inst.is_arf2_last_frame     = ENCHW_YES;
        break;

    default: ASSERT(0); break;
    }
}

void VCEncUpdateRefPicInfo(VCEncInst inst, const VCEncIn *pEncIn, VCEncPictureCodingType codingType) {
    i32 i;
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    i32 pocDisplay = vcenc_instance->poc % vcenc_instance->av1_inst.MaxPocDisplayAV1;
    for (i = 0; i < SW_REF_FRAMES; i++)
    {
        if (((vcenc_instance->av1_inst.refresh_frame_flags >> i) & 0x01) == 0)
            continue;

        vcenc_instance->av1_inst.ref_frame_map[i].poc             = pocDisplay;
        vcenc_instance->av1_inst.ref_frame_map[i].frame_num       = vcenc_instance->frameNum;
        vcenc_instance->av1_inst.ref_frame_map[i].codingType      = codingType;
        vcenc_instance->av1_inst.ref_frame_map[i].showable_frame  = vcenc_instance->av1_inst.showable_frame;
        vcenc_instance->av1_inst.ref_frame_map[i].ref_count       = 0;
        vcenc_instance->av1_inst.ref_frame_map[i].temporalId      = pEncIn->gopCurrPicConfig.temporalId;
    }
}

void VCEncFindPicToDisplay(VCEncInst inst, const VCEncIn *pEncIn) {
    i32 i;
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    vcenc_instance->av1_inst.show_existing_frame   = ENCHW_NO;

    for (i = 0; i < SW_REF_FRAMES; i++)
    {
        if (vcenc_instance->av1_inst.ref_frame_map[i].showable_frame == ENCHW_NO)
            continue;
        if (vcenc_instance->av1_inst.ref_frame_map[i].poc == vcenc_instance->av1_inst.POCtobeDisplayAV1)
        {
            vcenc_instance->av1_inst.show_existing_frame   = ENCHW_YES;
            vcenc_instance->av1_inst.frame_to_show_map_idx = i;
            vcenc_instance->av1_inst.ref_frame_map[i].showable_frame = ENCHW_NO;

            vcenc_instance->av1_inst.POCtobeDisplayAV1             = (vcenc_instance->av1_inst.POCtobeDisplayAV1 + 1) % vcenc_instance->av1_inst.MaxPocDisplayAV1;
            if(pEncIn->bIsIDR)
                vcenc_instance->av1_inst.POCtobeDisplayAV1 = 0;
            return;
        }
    }
}

i8 VCEncLookforRefAV1(VCEncInst inst, i32 refPoc)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    for(i32 i = 0; i < 7; i++)
    {
        if((vcenc_instance->av1_inst.is_valid_idx[i] == ENCHW_YES) &&(refPoc == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i]].poc))
            return (i + SW_LAST_FRAME);
    }

    for(i32 i = 0; i < 7; i++)
    {
        if((refPoc == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i]].poc))
            return (i + SW_LAST_FRAME);
    }
    return SW_NONE_FRAME;
}


/*------------------------------------------------------------------------------
  remarking av1 ref map idx.
------------------------------------------------------------------------------*/
void ref_idx_markingAv1(VCEncInst inst, struct container *c, struct sw_picture *pic, i32 curPoc)
{
  struct sw_picture *p;
  struct node *n;
  i32 i, ref, poc, map_idx;
  true_e bMatched;
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  i8 ref_idx[2] = {vcenc_instance->av1_inst.list0_ref_frame, vcenc_instance->av1_inst.list1_ref_frame};

  /* First mark not used idx to not used as referene */
  for(i = SW_LAST_FRAME; i < SW_REF_FRAMES; i++){
      bMatched = ENCHW_NO;
      map_idx = vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME];
      for (n = c->picture.tail; n; n = n->next)
      {
          p = (struct sw_picture *)n;
          if((p) && (p->reference == HANTRO_TRUE)){
              if(p->poc == vcenc_instance->av1_inst.ref_frame_map[map_idx].poc){
                  bMatched = ENCHW_YES;
                  break;
              }
          }
      }

      if(bMatched == ENCHW_NO){
          vcenc_instance->av1_inst.ref_frame_map[map_idx].poc = curPoc;
          vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] = ENCHW_NO;
      }
  }

  /* secondly mark repeatly idx to not used as referene */
  for(ref = 0; ref < 2; ref++){
      if(ref_idx[ref] > SW_INTRA_FRAME){
          poc = vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[ref_idx[ref]-SW_LAST_FRAME]].poc;
          for (i = SW_LAST_FRAME; i < SW_GOLDEN_FRAME; i++)
          {
              if((i != ref_idx[ref]) && (poc == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc)){
                vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] = ENCHW_NO;
                vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc = curPoc;
              }
          }
          for (i = SW_GOLDEN_FRAME; i < SW_REF_FRAMES; i++)
          {
              if((i != ref_idx[ref]) && (vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] == ENCHW_NO)&& (poc == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc))
                vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc = curPoc;
          }
      }
  }

  /* thirdly pickup idx to valid */
  for(ref = 0; ref < 2; ref++){
      if(ref_idx[ref] > SW_INTRA_FRAME){
          poc = vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[ref_idx[ref]-SW_LAST_FRAME]].poc;
          for (i = SW_LAST_FRAME; i < SW_REF_FRAMES; i++)
          {
              if((i == ref_idx[ref]) && (poc == vcenc_instance->av1_inst.ref_frame_map[vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME]].poc)&&(ENCHW_NO == vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME]))
                vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] = ENCHW_YES;
          }
      }
  }

  for(i = SW_LAST_FRAME; i < SW_REF_FRAMES; i++){
      if(vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] == ENCHW_NO){
          map_idx = vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME];
          vcenc_instance->av1_inst.ref_frame_map[map_idx].poc = curPoc;
      }
  }

  if((vcenc_instance->sps->enable_order_hint == ENCHW_YES) && (vcenc_instance->av1_inst.reference_mode == REFERENCE_MODE_SELECT)){
      map_idx = vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST3_FRAME-SW_LAST_FRAME];
      for(i = SW_LAST_FRAME; i < SW_REF_FRAMES; i++){
          if(vcenc_instance->av1_inst.is_valid_idx[i-SW_LAST_FRAME] == ENCHW_NO){
              map_idx = vcenc_instance->av1_inst.remapped_ref_idx[i-SW_LAST_FRAME];
              break;
          }
      }
      if((vcenc_instance->av1_inst.list1_ref_frame != SW_BWDREF_FRAME) && (vcenc_instance->av1_inst.is_valid_idx[SW_BWDREF_FRAME-SW_LAST_FRAME] == ENCHW_YES))
          vcenc_instance->av1_inst.encoded_ref_idx[SW_BWDREF_FRAME-SW_LAST_FRAME] = map_idx;
      if((vcenc_instance->av1_inst.list1_ref_frame != SW_ALTREF2_FRAME) && (vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF2_FRAME-SW_LAST_FRAME] == ENCHW_YES))
          vcenc_instance->av1_inst.encoded_ref_idx[SW_ALTREF2_FRAME-SW_LAST_FRAME] = map_idx;
      if((vcenc_instance->av1_inst.list1_ref_frame != SW_ALTREF_FRAME) && (vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF_FRAME-SW_LAST_FRAME] == ENCHW_YES))
          vcenc_instance->av1_inst.encoded_ref_idx[SW_ALTREF_FRAME-SW_LAST_FRAME] = map_idx;
  }

  vcenc_instance->av1_inst.ref_frame_map[7].poc = curPoc;
}

i32 VCEncAV1PocIdx(const i32 *pPocList, i32 gopSize, i32 looked_poc){
    for(i32 i = 0; i < gopSize; i++){
        if(pPocList[i] == looked_poc)
            return i;
    }
    return 0;
}

void VCEncInitAV1RefIdx(const VCEncIn *pEncIn, VCEncInst inst){
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

    if(vcenc_instance->av1_inst.is_RefIdx_inited == ENCHW_YES)
        return;

    true_e useing_ref[MAX_GOP_SIZE];
    true_e poc_skip[MAX_GOP_SIZE];
    i32 gopSize, descendNum, index, curr_id, ref_poc;
    i32 poc[MAX_GOP_SIZE];

    for(i32 gopSize=1; gopSize <= MAX_GOP_SIZE ; gopSize++){
        for(index = 0; index < gopSize; index++){
            curr_id           = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
            useing_ref[index] = ENCHW_NO;
            poc_skip[index]   = ENCHW_NO;
            poc[index]        = pEncIn->gopConfig.pGopPicCfg[curr_id].poc;
        }

        descendNum = 0;
        for(index = 1; index < gopSize; index++){
            if(poc[index] < poc[index-1])
                descendNum = index;
            else
                break;
        }

        for(index = descendNum; index < (gopSize-1); index++){
            if((poc[index]+1) != poc[index+1])
                poc_skip[index]   = ENCHW_YES;
        }

        if(poc[gopSize-1] < poc[0])
            poc_skip[gopSize-1] = ENCHW_YES;

        for(index = 0; index < gopSize; index++){
            curr_id = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
            for(i32 i = 0; i < pEncIn->gopConfig.pGopPicCfg[curr_id].numRefPics; i++)
            {
                ref_poc = pEncIn->gopConfig.pGopPicCfg[curr_id].refPics[i].ref_pic;
                if(!IS_LONG_TERM_REF_DELTAPOC(ref_poc)){
                    ref_poc += poc[index];
                    if(ref_poc == 0)
                        ref_poc = gopSize;
                    if((ref_poc > 0) && (ref_poc <= gopSize))
                        useing_ref[VCEncAV1PocIdx(poc, gopSize, ref_poc)] = ENCHW_YES;
                }
            }
        }

        //
        if(descendNum == 3){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = ARF_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][1] = ARF2_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][2] = BRF_UPDATE;
        }
        else if(descendNum == 2){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = ARF_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][1] = BRF_UPDATE;
        }
        if(descendNum == 1){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = BRF_UPDATE;
        }

        for(index = descendNum; index < gopSize; index++){
            if(useing_ref[index]){
                if(poc_skip[index])
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = BRF_UPDATE;
                else
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = LF_UPDATE;
            }
            else{
                if(poc_skip[index]){
                    curr_id = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
                    ref_poc = poc[index]+1;
                    if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][VCEncAV1PocIdx(poc, gopSize, ref_poc)] == ARF_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = ARF_TO_LAST_UPDATE;
                    else if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][VCEncAV1PocIdx(poc, gopSize, ref_poc)] == ARF2_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = ARF2_TO_LAST_UPDATE;
                    else if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][VCEncAV1PocIdx(poc, gopSize, ref_poc)] == BRF_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = BWD_TO_LAST_UPDATE;
                    else
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = COMMON_NO_UPDATE;
                }
                else
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = COMMON_NO_UPDATE;
            }
        }
    }

    vcenc_instance->av1_inst.is_RefIdx_inited = ENCHW_YES;
}

VCEncRet VCEncGenAV1Config(VCEncInst inst, const VCEncIn *pEncIn, struct sw_picture *pic, VCEncPictureCodingType codingType)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    true_e bQpDeltaZero;
    asicData_s *asic = &vcenc_instance->asic;
    i32 orgList[2] = {0,1};

    VCENC_AV1_FRAME_TYPE frame_type;
    if (pEncIn->codingType == VCENC_INTRA_FRAME)
      frame_type = pEncIn->bIsIDR ? VCENC_AV1_KEY_FRAME : VCENC_AV1_INTRA_ONLY_FRAME;
    else
      frame_type = VCENC_AV1_INTER_FRAME;

    if(vcenc_instance->disableDeblocking == 0)
    {
      av1_pick_filter_level(vcenc_instance,frame_type);
    }
    else
    {
      vcenc_instance->av1_inst.filter_level[0] = 0;
      vcenc_instance->av1_inst.filter_level[1] = 0;
      vcenc_instance->av1_inst.filter_level_u = 0;
      vcenc_instance->av1_inst.filter_level_v = 0;
    }

    if(vcenc_instance->enableSao)
    {
      av1_pick_cdef_para(vcenc_instance,frame_type);
    }

    if(pEncIn->bIsIDR){
        //if((pEncIn->gopConfig.idr_interval == 0) || ((pEncIn->bIsPeriodUsingLTR == HANTRO_TRUE) && (pEncIn->gopConfig.idr_interval > ( (1<< DEFAULT_EXPLICIT_ORDER_HINT_BITS)>>1)))){
        if((pEncIn->gopConfig.idr_interval == 0) || (pEncIn->bIsPeriodUsingLTR == HANTRO_TRUE) || (pEncIn->gopConfig.idr_interval > ( (1<< DEFAULT_EXPLICIT_ORDER_HINT_BITS)>>1))){
            vcenc_instance->sps->frame_id_numbers_present_flag = ENCHW_YES;
            vcenc_instance->sps->enable_order_hint             = ENCHW_NO;

            vcenc_instance->av1_inst.MaxPocDisplayAV1  = pEncIn->gopConfig.idr_interval ? pEncIn->gopConfig.idr_interval : (1 << FRAME_ID_LENGTH);
        }else{
            vcenc_instance->sps->frame_id_numbers_present_flag = ENCHW_NO;
            vcenc_instance->sps->enable_order_hint             = ENCHW_YES;

            vcenc_instance->av1_inst.MaxPocDisplayAV1  = pEncIn->gopConfig.idr_interval ? pEncIn->gopConfig.idr_interval : (1 << DEFAULT_EXPLICIT_ORDER_HINT_BITS);
            vcenc_instance->av1_inst.MaxPocDisplayAV1  = MIN(vcenc_instance->av1_inst.MaxPocDisplayAV1, (1 << DEFAULT_EXPLICIT_ORDER_HINT_BITS));
        }
    }

    if((vcenc_instance->sps->max_num_sub_layers > 1) &&(vcenc_instance->sps->frame_id_numbers_present_flag == ENCHW_YES)){
        APITRACEERR("VCEncGenAV1Config: ERROR Invalid configure for AV1 SVC");
        return VCENC_ERROR;
    }

    VCEncFindPicToDisplay(vcenc_instance, pEncIn);
    if(pEncIn->bIsIDR)
      vcenc_instance->av1_inst.POCtobeDisplayAV1 = 0;
    i32 pocDisplay = pEncIn->poc % vcenc_instance->av1_inst.MaxPocDisplayAV1;
    if (pocDisplay == vcenc_instance->av1_inst.POCtobeDisplayAV1)
    {
        vcenc_instance->av1_inst.show_frame          = ENCHW_YES;
        vcenc_instance->av1_inst.showable_frame      = ENCHW_NO;
        vcenc_instance->av1_inst.POCtobeDisplayAV1   = (vcenc_instance->av1_inst.POCtobeDisplayAV1 + 1)%vcenc_instance->av1_inst.MaxPocDisplayAV1;
    }
    else
    {
        vcenc_instance->av1_inst.show_frame          = ENCHW_NO;
        vcenc_instance->av1_inst.showable_frame      = ENCHW_YES;
    }

    vcenc_instance->av1_inst.order_hint            = pEncIn->poc % (1 << vcenc_instance->sps->order_hint_bits);

    VCEncInitAV1RefIdx(pEncIn, inst);

    // ref
    av1_configure_buffer_updates(vcenc_instance, (VCEncIn *)pEncIn);

    vcenc_instance->av1_inst.last_frame_idx         = 0;
    vcenc_instance->av1_inst.gold_frame_idx         = 0;

    for(i32 i = 0; i < SW_REF_FRAMES; i++)
        vcenc_instance->av1_inst.encoded_ref_idx[i] = vcenc_instance->av1_inst.remapped_ref_idx[i];
    vcenc_instance->av1_inst.list0_ref_frame = SW_NONE_FRAME;
    vcenc_instance->av1_inst.list1_ref_frame = SW_NONE_FRAME;
    if(codingType == VCENC_PREDICTED_FRAME){
        vcenc_instance->av1_inst.reference_mode = SINGLE_REFERENCE;
        vcenc_instance->av1_inst.list0_ref_frame      = pic->rpl[0][0]->long_term_flag ? SW_GOLDEN_FRAME : VCEncLookforRefAV1(vcenc_instance, pic->rpl[0][0]->poc);
    }
    else if (codingType == VCENC_BIDIR_PREDICTED_FRAME){
        vcenc_instance->av1_inst.reference_mode = REFERENCE_MODE_SELECT;
        vcenc_instance->av1_inst.list0_ref_frame      = pic->rpl[0][0]->long_term_flag ? SW_GOLDEN_FRAME:VCEncLookforRefAV1(vcenc_instance, pic->rpl[0][0]->poc);
        vcenc_instance->av1_inst.list1_ref_frame      = pic->rpl[1][0]->long_term_flag ? SW_GOLDEN_FRAME:VCEncLookforRefAV1(vcenc_instance, pic->rpl[1][0]->poc);

        if(vcenc_instance->av1_inst.list1_ref_frame < vcenc_instance->av1_inst.list0_ref_frame){
            i8 tmp = vcenc_instance->av1_inst.list1_ref_frame;
            vcenc_instance->av1_inst.list1_ref_frame = vcenc_instance->av1_inst.list0_ref_frame;
            vcenc_instance->av1_inst.list0_ref_frame = tmp;

            orgList[0] = 1;
            orgList[1] = 0;

            for (i32 i = 0; i < pic->sliceInst->active_l0_cnt; i++)
            {
                asic->regs.pRefPic_recon_l0[0][i] = pic->rpl[1][i]->recon.lum;
                asic->regs.pRefPic_recon_l0[1][i] = pic->rpl[1][i]->recon.cb;
                asic->regs.pRefPic_recon_l0[2][i] = pic->rpl[1][i]->recon.cr;
                asic->regs.pRefPic_recon_l0_4n[i] =  pic->rpl[1][i]->recon_4n_base;
                asic->regs.l0_delta_poc[i] = pic->rpl[1][i]->poc - pic->poc;
                asic->regs.l0_long_term_flag[i] = pic->rpl[1][i]->long_term_flag;

                //compress
                asic->regs.ref_l0_luma_compressed[i] = pic->rpl[1][i]->recon_compress.lumaCompressed;
                asic->regs.ref_l0_luma_compress_tbl_base[i] = pic->rpl[1][i]->recon_compress.lumaTblBase;
                asic->regs.ref_l0_chroma_compressed[i] = pic->rpl[1][i]->recon_compress.chromaCompressed;
                asic->regs.ref_l0_chroma_compress_tbl_base[i] = pic->rpl[1][i]->recon_compress.chromaTblBase;
            }

            for (i32 i = 0; i < pic->sliceInst->active_l1_cnt; i++)
            {
                asic->regs.pRefPic_recon_l1[0][i] = pic->rpl[0][i]->recon.lum;
                asic->regs.pRefPic_recon_l1[1][i] = pic->rpl[0][i]->recon.cb;
                asic->regs.pRefPic_recon_l1[2][i] = pic->rpl[0][i]->recon.cr;
                asic->regs.pRefPic_recon_l1_4n[i] = pic->rpl[0][i]->recon_4n_base;
                asic->regs.l1_delta_poc[i] = pic->rpl[0][i]->poc - pic->poc;
                asic->regs.l1_long_term_flag[i] = pic->rpl[0][i]->long_term_flag;

                //compress
                asic->regs.ref_l1_luma_compressed[i] = pic->rpl[0][i]->recon_compress.lumaCompressed;
                asic->regs.ref_l1_luma_compress_tbl_base[i] = pic->rpl[0][i]->recon_compress.lumaTblBase;
                asic->regs.ref_l1_chroma_compressed[i] = pic->rpl[0][i]->recon_compress.chromaCompressed;
                asic->regs.ref_l1_chroma_compress_tbl_base[i] = pic->rpl[0][i]->recon_compress.chromaTblBase;
            }
        }
    }

    vcenc_instance->av1_inst.primary_ref_frame     = PRIMARY_REF_NONE;
    asic->regs.frameCtx_base = pic->framectx_base;
    if((codingType != VCENC_INTRA_FRAME) && (vcenc_instance->av1_inst.error_resilient_mode == ENCHW_NO) && (vcenc_instance->av1_inst.disable_cdf_update == ENCHW_NO) && (vcenc_instance->av1_inst.disable_frame_end_update_cdf == ENCHW_NO)){
        i32 list0_idx = (vcenc_instance->av1_inst.list0_ref_frame > SW_INTRA_FRAME) ? vcenc_instance->av1_inst.remapped_ref_idx[vcenc_instance->av1_inst.list0_ref_frame -SW_LAST_FRAME] : SW_NONE_FRAME;
        i32 list1_idx = (vcenc_instance->av1_inst.list1_ref_frame > SW_INTRA_FRAME) ? vcenc_instance->av1_inst.remapped_ref_idx[vcenc_instance->av1_inst.list1_ref_frame -SW_LAST_FRAME] : SW_NONE_FRAME;
        i32 list = 0;
        vcenc_instance->av1_inst.primary_ref_frame     = (vcenc_instance->frameNum != 0) ? (vcenc_instance->av1_inst.list0_ref_frame-SW_LAST_FRAME) : PRIMARY_REF_NONE;
        if((list1_idx > SW_NONE_FRAME) &&(((vcenc_instance->av1_inst.ref_frame_map[list0_idx].codingType == VCENC_INTRA_FRAME) && (vcenc_instance->av1_inst.ref_frame_map[list1_idx].codingType != VCENC_INTRA_FRAME)) ||  ((vcenc_instance->av1_inst.ref_frame_map[list1_idx].codingType == codingType) && (vcenc_instance->av1_inst.ref_frame_map[list0_idx].codingType != codingType)))){
            vcenc_instance->av1_inst.primary_ref_frame     = vcenc_instance->av1_inst.list1_ref_frame-SW_LAST_FRAME;
            list = 1;
        }
        else if(list0_idx > SW_NONE_FRAME)
            vcenc_instance->av1_inst.primary_ref_frame     = vcenc_instance->av1_inst.list0_ref_frame-SW_LAST_FRAME;

        if( PRIMARY_REF_NONE != vcenc_instance->av1_inst.primary_ref_frame){
            memcpy((u8 *)pic->framectx_base, (u8 *)pic->rpl[orgList[list]][0]->framectx_base, FRAME_CONTEXT_LENGTH);
        }
    }
    asic->regs.sw_primary_ref_frame = vcenc_instance->av1_inst.primary_ref_frame;
    if(vcenc_instance->av1_inst.primary_ref_frame == PRIMARY_REF_NONE)
        av1_prob_init((vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS), (FRAME_CONTEXT *)pic->framectx_base);

    vcenc_instance->av1_inst.is_skip_mode_allowed = (codingType == VCENC_BIDIR_PREDICTED_FRAME) && vcenc_instance->sps->enable_order_hint;
    vcenc_instance->av1_inst.skip_mode_flag       = vcenc_instance->av1_inst.is_skip_mode_allowed? ENCHW_YES : ENCHW_NO;
    vcenc_instance->av1_inst.LumaDcQpOffset   = 0;
    vcenc_instance->av1_inst.ChromaDcQpOffset = 0;

    bQpDeltaZero = (vcenc_instance->av1_inst.LumaDcQpOffset == 0) && (vcenc_instance->av1_inst.ChromaDcQpOffset == 0) && (vcenc_instance->chromaQpOffset == 0)
                   && (quantizer_to_qindex[vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS] == 0);

    vcenc_instance->av1_inst.all_lossless   = bQpDeltaZero ? ENCHW_YES : ENCHW_NO;
    vcenc_instance->av1_inst.coded_lossless = bQpDeltaZero ? ENCHW_YES : ENCHW_NO;
    vcenc_instance->av1_inst.tx_mode        = bQpDeltaZero ? SW_ONLY_4X4 : SW_TX_MODE_SELECT;

    asic->regs.sw_av1_allow_intrabc  = vcenc_instance->av1_inst.allow_intrabc;
    asic->regs.sw_av1_coded_lossless = vcenc_instance->av1_inst.coded_lossless;
    asic->regs.sw_av1_delta_q_res    = vcenc_instance->av1_inst.delta_q_res;
    asic->regs.sw_av1_tx_mode        = vcenc_instance->av1_inst.tx_mode;
    asic->regs.sw_av1_reduced_tx_set_used        = vcenc_instance->av1_inst.reduced_tx_set_used;
    asic->regs.sw_av1_allow_high_precision_mv    = vcenc_instance->av1_inst.allow_high_precision_mv;
    asic->regs.sw_av1_enable_interintra_compound = vcenc_instance->sps->enable_interintra_compound;
    asic->regs.sw_av1_enable_dual_filter         = vcenc_instance->sps->enable_dual_filter;
    asic->regs.sw_av1_enable_filter_intra        = vcenc_instance->sps->enable_filter_intra;
    asic->regs.sw_av1_cur_frame_force_integer_mv = vcenc_instance->av1_inst.force_integer_mv;
    asic->regs.sw_av1_switchable_motion_mode     = vcenc_instance->av1_inst.switchable_motion_mode;
    asic->regs.sw_av1_interp_filter              = vcenc_instance->av1_inst.interp_filter;
    asic->regs.sw_av1_seg_enable                 = vcenc_instance->av1_inst.seg_enable;
    asic->regs.sw_av1_allow_update_cdf           = !vcenc_instance->av1_inst.disable_cdf_update;
    asic->regs.sw_av1_skip_mode_flag  = vcenc_instance->av1_inst.skip_mode_flag;
    asic->regs.sw_av1_reference_mode  = vcenc_instance->av1_inst.reference_mode;
    asic->regs.sw_av1_list0_ref_frame = vcenc_instance->av1_inst.list0_ref_frame & 0xf; // 4 bits signed
    asic->regs.sw_av1_list1_ref_frame = vcenc_instance->av1_inst.list1_ref_frame & 0xf; // 4 bits signed
    asic->regs.sw_av1_db_filter_lvl[0] = vcenc_instance->av1_inst.filter_level[0];
    asic->regs.sw_av1_db_filter_lvl[1] = vcenc_instance->av1_inst.filter_level[1];
    asic->regs.sw_av1_db_filter_lvl_u  = vcenc_instance->av1_inst.filter_level_u;
    asic->regs.sw_av1_db_filter_lvl_v  = vcenc_instance->av1_inst.filter_level_v;
    asic->regs.sw_av1_sharpness_lvl    = vcenc_instance->av1_inst.sharpness_level;
    asic->regs.sw_cdef_damping         = vcenc_instance->av1_inst.cdef_damping;
    asic->regs.sw_cdef_bits            = vcenc_instance->av1_inst.cdef_bits;
    asic->regs.sw_cdef_strengths       = vcenc_instance->av1_inst.cdef_strengths[0];
    asic->regs.sw_cdef_uv_strengths    = vcenc_instance->av1_inst.cdef_uv_strengths[0];
    asic->regs.sw_av1_enable_order_hint= vcenc_instance->sps->enable_order_hint;
    asic->regs.av1_bTxTypeSearch       = vcenc_instance->av1_inst.bTxTypeSearch;

    return VCENC_OK;
}

void VCEncInitAV1(const VCEncConfig *config, VCEncInst inst)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

    vcenc_instance->av1_inst.is_av1_TmpId = ENCHW_NO;
    vcenc_instance->av1_inst.error_resilient_mode = ENCHW_NO; // ENCHW_YES; //
    vcenc_instance->av1_inst.disable_cdf_update = ENCHW_NO;
    vcenc_instance->av1_inst.allow_screen_content_tools = ENCHW_NO;  // intrabc
    vcenc_instance->av1_inst.force_integer_mv = ENCHW_NO;

    vcenc_instance->av1_inst.allow_high_precision_mv = ENCHW_NO;
    vcenc_instance->av1_inst.disable_frame_end_update_cdf = ENCHW_NO; // ENCHW_YES; //
    vcenc_instance->av1_inst.allow_warped_motion = ENCHW_NO;
    vcenc_instance->av1_inst.render_and_frame_size_different = ENCHW_NO;
    vcenc_instance->av1_inst.frame_size_override_flag = ENCHW_NO;
    vcenc_instance->av1_inst.allow_intrabc = ENCHW_NO;  // intrabc
    vcenc_instance->av1_inst.large_scale_tile = ENCHW_NO;
    vcenc_instance->av1_inst.using_qmatrix = ENCHW_NO;
    vcenc_instance->av1_inst.segmentation_enable = ENCHW_NO;
    vcenc_instance->av1_inst.delta_lf_present_flag = ENCHW_NO;
    vcenc_instance->av1_inst.delta_lf_multi = ENCHW_NO;
    vcenc_instance->av1_inst.reduced_tx_set_used = ENCHW_NO;
    vcenc_instance->av1_inst.uniform_tile_spacing_flag = ENCHW_YES;
    vcenc_instance->av1_inst.equal_picture_interval = ENCHW_YES;
    vcenc_instance->av1_inst.decoder_model_info_present_flag = ENCHW_NO;
    vcenc_instance->av1_inst.cur_frame_force_integer_mv = ENCHW_NO;
    vcenc_instance->av1_inst.switchable_motion_mode = ENCHW_NO;
    vcenc_instance->av1_inst.allow_ref_frame_mvs = ENCHW_NO;
    vcenc_instance->av1_inst.seg_enable          = ENCHW_NO;

    if(vcenc_instance->width * vcenc_instance->height < 1024*768)
        vcenc_instance->av1_inst.interp_filter = AV1_EIGHTTAP_SHARP;
    else
        vcenc_instance->av1_inst.interp_filter = AV1_EIGHTTAP;

#ifdef AV1_INTERP_FILTER_SWITCH_EN
    if (vcenc_instance->width * vcenc_instance->height > 360000)
       vcenc_instance->av1_inst.interp_filter = AV1_SWITCHABLE;

#ifdef AV1_INTERP_FILTER_DUAL_EN
    vcenc_instance->sps->enable_dual_filter = vcenc_instance->av1_inst.interp_filter == AV1_SWITCHABLE;
#endif
#endif

    vcenc_instance->av1_inst.is_RefIdx_inited = ENCHW_NO;
    vcenc_instance->av1_inst.frame_context_idx = 0;

    vcenc_instance->av1_inst.superres_upscaled_width = vcenc_instance->width;
    vcenc_instance->av1_inst.delta_q_res = 1;// DEFAULT_DELTA_Q_RES;

    // n_shift is the multiplier for lf_deltas
    // the multiplier is 1 for when filter_lvl is between 0 and 31;
    // 2 when filter_lvl is between 32 and 63
    vcenc_instance->av1_inst.delta_lf_res = DEFAULT_DELTA_LF_RES;

    for (i32 i = 0; i < SW_REF_FRAMES; i++)
    {
        vcenc_instance->av1_inst.is_ref_globle[i]                = ENCHW_NO;
        vcenc_instance->av1_inst.ref_frame_map[i].showable_frame = ENCHW_NO;
        vcenc_instance->av1_inst.fb_of_context_type[i]           = 0;
        // vcenc_instance->remapped_ref_idx[REF_FRAMES];
    }
    for (i32 i = 0; i < (SW_REF_FRAMES-1); i++)
        vcenc_instance->av1_inst.remapped_ref_idx[i] = i;

    vcenc_instance->av1_inst.mode_ref_delta_enabled = 0;
    vcenc_instance->av1_inst.mode_ref_delta_update = 0;

    vcenc_instance->av1_inst.mi_cols = ALIGN_POWER_OF_TWO(vcenc_instance->width, 3) >> MI_SIZE_LOG2;
    vcenc_instance->av1_inst.mi_rows = ALIGN_POWER_OF_TWO(vcenc_instance->height, 3) >> MI_SIZE_LOG2;
    vcenc_instance->av1_inst.tile_cols = 1; // log2 number of tile columns
    vcenc_instance->av1_inst.tile_rows = 1; // log2 number of tile rows
    i32 mi_cols = ALIGN_POWER_OF_TWO(vcenc_instance->av1_inst.mi_cols, vcenc_instance->sps->mib_size_log2);
    i32 mi_rows = ALIGN_POWER_OF_TWO(vcenc_instance->av1_inst.mi_rows, vcenc_instance->sps->mib_size_log2);
    i32 sb_cols = mi_cols >> vcenc_instance->sps->mib_size_log2;
    i32 sb_rows = mi_rows >> vcenc_instance->sps->mib_size_log2;

    i32 sb_size_log2 = vcenc_instance->sps->mib_size_log2 + MI_SIZE_LOG2;
    vcenc_instance->av1_inst.max_tile_width_sb = MAX_TILE_WIDTH >> sb_size_log2;
    i32 max_tile_area_sb = MAX_TILE_AREA >> (2 * sb_size_log2);

    vcenc_instance->av1_inst.min_log2_tile_cols = tile_log2(vcenc_instance->av1_inst.max_tile_width_sb, sb_cols);
    vcenc_instance->av1_inst.max_log2_tile_cols = tile_log2(1, MIN(sb_cols, SW_MAX_TILE_COLS));
    vcenc_instance->av1_inst.max_log2_tile_rows = tile_log2(1, MIN(sb_rows, SW_MAX_TILE_ROWS));
    vcenc_instance->av1_inst.min_log2_tiles = tile_log2(max_tile_area_sb, sb_cols * sb_rows);
    vcenc_instance->av1_inst.min_log2_tiles = MAX(vcenc_instance->av1_inst.min_log2_tiles, vcenc_instance->av1_inst.min_log2_tile_cols);

    log2i(vcenc_instance->av1_inst.tile_cols, (i32*)&vcenc_instance->av1_inst.log2_tile_cols);
    vcenc_instance->av1_inst.log2_tile_cols = MAX(vcenc_instance->av1_inst.log2_tile_cols, vcenc_instance->av1_inst.min_log2_tile_cols);
    vcenc_instance->av1_inst.log2_tile_cols = MIN(vcenc_instance->av1_inst.log2_tile_cols, vcenc_instance->av1_inst.max_log2_tile_cols);

    vcenc_instance->av1_inst.min_log2_tile_rows = MAX(vcenc_instance->av1_inst.min_log2_tiles - vcenc_instance->av1_inst.log2_tile_cols, 0);
    vcenc_instance->av1_inst.max_tile_height_sb = sb_rows >> vcenc_instance->av1_inst.min_log2_tile_rows;

    log2i(vcenc_instance->av1_inst.tile_rows, (i32*)&vcenc_instance->av1_inst.log2_tile_rows);
    vcenc_instance->av1_inst.log2_tile_rows = MAX(vcenc_instance->av1_inst.log2_tile_rows, vcenc_instance->av1_inst.min_log2_tile_rows);
    vcenc_instance->av1_inst.log2_tile_rows = MIN(vcenc_instance->av1_inst.log2_tile_rows, vcenc_instance->av1_inst.max_log2_tile_rows);

    vcenc_instance->av1_inst.filter_level[0] = 0;
    vcenc_instance->av1_inst.filter_level[1] = 0;
    vcenc_instance->av1_inst.filter_level_u  = 0;
    vcenc_instance->av1_inst.filter_level_v  = 0;
    vcenc_instance->av1_inst.sharpness_level = 0;

    vcenc_instance->av1_inst.cdef_damping       = 0;
    vcenc_instance->av1_inst.cdef_bits          = 0;
	for(i32 i = 0; i < AV1_CDEF_STRENGTH_NUM; i++){
        vcenc_instance->av1_inst.cdef_strengths[i]     = 0;
        vcenc_instance->av1_inst.cdef_uv_strengths[i]  = 0;
	}
    vcenc_instance->sps->enable_cdef            = vcenc_instance->sps->sao_enabled_flag;

    // [gopsize][gopIndex]
    vcenc_instance->av1_inst.upDateRefFrame[0][0] = LF_UPDATE;          // gopsize = 1, 0

    vcenc_instance->av1_inst.upDateRefFrame[1][0] = BRF_UPDATE;         // gopsize = 2 , 2
    vcenc_instance->av1_inst.upDateRefFrame[1][1] = BWD_TO_LAST_UPDATE; // gopsize = 2 , 1

    vcenc_instance->av1_inst.upDateRefFrame[2][0] = BRF_UPDATE;         // gopsize = 3 , 3
    vcenc_instance->av1_inst.upDateRefFrame[2][1] = LF_UPDATE;          // gopsize = 3 , 1
    vcenc_instance->av1_inst.upDateRefFrame[2][2] = BWD_TO_LAST_UPDATE; // gopsize = 3 , 2

    vcenc_instance->av1_inst.upDateRefFrame[3][0] = ARF_UPDATE;         // gopsize = 4 , 4
    vcenc_instance->av1_inst.upDateRefFrame[3][1] = BRF_UPDATE;         // gopsize = 4 , 2
    vcenc_instance->av1_inst.upDateRefFrame[3][2] = BWD_TO_LAST_UPDATE; // gopsize = 4 , 1
    vcenc_instance->av1_inst.upDateRefFrame[3][3] = ARF_TO_LAST_UPDATE; // gopsize = 4 , 3

    vcenc_instance->av1_inst.upDateRefFrame[4][0] = ARF_UPDATE;         // gopsize = 5 , 5
    vcenc_instance->av1_inst.upDateRefFrame[4][1] = BRF_UPDATE;         // gopsize = 5 , 2
    vcenc_instance->av1_inst.upDateRefFrame[4][2] = BWD_TO_LAST_UPDATE; // gopsize = 5 , 1
    vcenc_instance->av1_inst.upDateRefFrame[4][3] = LF_UPDATE;          // gopsize = 5 , 3
    vcenc_instance->av1_inst.upDateRefFrame[4][4] = ARF_TO_LAST_UPDATE; // gopsize = 5 , 4

    vcenc_instance->av1_inst.upDateRefFrame[5][0] = ARF_UPDATE;         // gopsize = 6 , 6
    vcenc_instance->av1_inst.upDateRefFrame[5][1] = BRF_UPDATE;         // gopsize = 6 , 3
    vcenc_instance->av1_inst.upDateRefFrame[5][2] = LF_UPDATE;          // gopsize = 6 , 1
    vcenc_instance->av1_inst.upDateRefFrame[5][3] = BWD_TO_LAST_UPDATE; // gopsize = 6 , 2
    vcenc_instance->av1_inst.upDateRefFrame[5][4] = LF_UPDATE;          // gopsize = 6 , 4
    vcenc_instance->av1_inst.upDateRefFrame[5][5] = ARF_TO_LAST_UPDATE; // gopsize = 6 , 5

    vcenc_instance->av1_inst.upDateRefFrame[6][0] = ARF_UPDATE;         // gopsize = 7 , 7
    vcenc_instance->av1_inst.upDateRefFrame[6][1] = BRF_UPDATE;         // gopsize = 7 , 3
    vcenc_instance->av1_inst.upDateRefFrame[6][2] = LF_UPDATE;          // gopsize = 7 , 1
    vcenc_instance->av1_inst.upDateRefFrame[6][3] = BWD_TO_LAST_UPDATE; // gopsize = 7 , 2
    vcenc_instance->av1_inst.upDateRefFrame[6][4] = BRF_UPDATE;         // gopsize = 7 , 5
    vcenc_instance->av1_inst.upDateRefFrame[6][5] = BWD_TO_LAST_UPDATE; // gopsize = 7 , 4
    vcenc_instance->av1_inst.upDateRefFrame[6][6] = ARF_TO_LAST_UPDATE; // gopsize = 7 , 6

    vcenc_instance->av1_inst.upDateRefFrame[7][0] = ARF_UPDATE;         // gopsize = 8 , 8
    vcenc_instance->av1_inst.upDateRefFrame[7][1] = ARF2_UPDATE;        // gopsize = 8 , 4
    vcenc_instance->av1_inst.upDateRefFrame[7][2] = BRF_UPDATE;         // gopsize = 8 , 2
    vcenc_instance->av1_inst.upDateRefFrame[7][3] = BWD_TO_LAST_UPDATE; // gopsize = 8 , 1
    vcenc_instance->av1_inst.upDateRefFrame[7][4] = ARF2_TO_LAST_UPDATE;// gopsize = 8 , 3
    vcenc_instance->av1_inst.upDateRefFrame[7][5] = BRF_UPDATE;         // gopsize = 8 , 6
    vcenc_instance->av1_inst.upDateRefFrame[7][6] = BWD_TO_LAST_UPDATE; // gopsize = 8 , 5
    vcenc_instance->av1_inst.upDateRefFrame[7][7] = ARF_TO_LAST_UPDATE; // gopsize = 8 , 7

    vcenc_instance->av1_inst.bTxTypeSearch = config->TxTypeSearchEnable; // enable tx type search by default
    if(vcenc_instance->av1_inst.bTxTypeSearch == ENCHW_NO)
      vcenc_instance->av1_inst.reduced_tx_set_used = ENCHW_YES;
}

// This function is used to shift the virtual indices of last reference frames as follows:
// LAST_FRAME -> LAST2_FRAME -> LAST3_FRAME when the LAST_FRAME is updated.
static INLINE void shift_last_ref_frames(VCEncInst inst) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  for (int ref_frame = SW_LAST3_FRAME; ref_frame > SW_LAST_FRAME; --ref_frame) {
    const int ref_idx = ref_frame - SW_LAST_FRAME;
    vcenc_instance->av1_inst.remapped_ref_idx[ref_idx] = vcenc_instance->av1_inst.remapped_ref_idx[ref_idx - 1];

#if 0
    if (!cpi->rc.is_src_frame_alt_ref) {
      memcpy(cpi->interp_filter_selected[ref_frame],
             cpi->interp_filter_selected[ref_frame - 1],
             sizeof(cpi->interp_filter_selected[ref_frame - 1]));
    }
#endif
  }
  vcenc_instance->av1_inst.is_valid_idx[SW_LAST3_FRAME - SW_LAST_FRAME] = vcenc_instance->av1_inst.is_valid_idx[SW_LAST2_FRAME - SW_LAST_FRAME];
  vcenc_instance->av1_inst.is_valid_idx[SW_LAST2_FRAME - SW_LAST_FRAME] = vcenc_instance->av1_inst.is_valid_idx[SW_LAST_FRAME - SW_LAST_FRAME];
}

// Don't allow a show_existing_frame to coincide with an error resilient
// frame. An exception can be made for a forward keyframe since it has no
// previous dependencies.
static INLINE int encode_show_existing_frame(VCEncInst inst, const VCEncIn *pEncIn) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

  return vcenc_instance->av1_inst.show_existing_frame && (!vcenc_instance->av1_inst.error_resilient_mode || pEncIn->bIsIDR);
}

void av1_update_reference_frames(VCEncInst inst, const VCEncIn *pEncIn) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;

  { /* For non key/golden frames */
    // === ALTREF_FRAME ===
    if (vcenc_instance->av1_inst.refresh_alt_ref_frame) {
        vcenc_instance->av1_inst.refresh_alt_ref_frame = 0;
        vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF_FRAME - SW_LAST_FRAME] = ENCHW_YES;
    }

    // === GOLDEN_FRAME ===
    if (vcenc_instance->av1_inst.refresh_golden_frame) {
        vcenc_instance->av1_inst.is_valid_idx[SW_GOLDEN_FRAME - SW_LAST_FRAME] = ENCHW_YES;
    }

    // === BWDREF_FRAME ===
    if (vcenc_instance->av1_inst.refresh_bwd_ref_frame) {
      vcenc_instance->av1_inst.is_valid_idx[SW_BWDREF_FRAME - SW_LAST_FRAME] = ENCHW_YES;
    }

    if (vcenc_instance->av1_inst.refresh_ext_ref_frame) {
      vcenc_instance->av1_inst.refresh_ext_ref_frame = 0;
    }

    // === ALTREF2_FRAME ===
    if (vcenc_instance->av1_inst.refresh_alt2_ref_frame) {
      vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF2_FRAME - SW_LAST_FRAME] = ENCHW_YES;
      vcenc_instance->av1_inst.refresh_alt2_ref_frame = 0;
    }
  }

  if (vcenc_instance->av1_inst.refresh_last_frame) {
    // If the new structure is used, we will always have overlay frames coupled
    // with bwdref frames. Therefore, we won't have to perform this update
    // in advance (we do this update when the overlay frame shows up).
    if(vcenc_instance->av1_inst.is_preserve_last_as_gld){
        i32 golden_remapped_idx = get_ref_frame_map_idx(vcenc_instance, SW_GOLDEN_FRAME);
        vcenc_instance->av1_inst.is_valid_idx[SW_GOLDEN_FRAME - SW_LAST_FRAME] = ENCHW_YES;
        vcenc_instance->av1_inst.remapped_ref_idx[SW_GOLDEN_FRAME - SW_LAST_FRAME]   = get_ref_frame_map_idx(vcenc_instance, SW_LAST_FRAME);
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST_FRAME - SW_LAST_FRAME]     = golden_remapped_idx;
    }
    else if((vcenc_instance->av1_inst.is_preserve_last3_as_gld) && (COMMON_NO_UPDATE != vcenc_instance->av1_inst.preserve_last3_as)){
        i32 remapped_ref_idx;
        if( BRF_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_BWDREF_FRAME;
        else if( ARF2_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_ALTREF2_FRAME;
        else if( ARF_UPDATE == vcenc_instance->av1_inst.preserve_last3_as)
            remapped_ref_idx = SW_ALTREF_FRAME;
        else
            remapped_ref_idx = SW_GOLDEN_FRAME;

        i32 golden_remapped_idx = get_ref_frame_map_idx(vcenc_instance, remapped_ref_idx);
        vcenc_instance->av1_inst.is_valid_idx[remapped_ref_idx - SW_LAST_FRAME] = ENCHW_YES;
        vcenc_instance->av1_inst.remapped_ref_idx[remapped_ref_idx - SW_LAST_FRAME]   = get_ref_frame_map_idx(vcenc_instance, SW_LAST3_FRAME);
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST3_FRAME - SW_LAST_FRAME]     = golden_remapped_idx;
    }

    i32 last3_remapped_idx = get_ref_frame_map_idx(vcenc_instance, SW_LAST3_FRAME);
    shift_last_ref_frames(vcenc_instance);
    if (vcenc_instance->av1_inst.is_bwd_last_frame) {
        vcenc_instance->av1_inst.is_valid_idx[SW_BWDREF_FRAME - SW_LAST_FRAME] = ENCHW_NO;
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST_FRAME - SW_LAST_FRAME]   = get_ref_frame_map_idx(vcenc_instance, SW_BWDREF_FRAME);
        vcenc_instance->av1_inst.remapped_ref_idx[SW_BWDREF_FRAME - SW_LAST_FRAME] = last3_remapped_idx;
    }
    else if (vcenc_instance->av1_inst.is_arf_last_frame) {
        vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF_FRAME - SW_LAST_FRAME] = ENCHW_NO;
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST_FRAME - SW_LAST_FRAME]   = get_ref_frame_map_idx(vcenc_instance, SW_ALTREF_FRAME);
        vcenc_instance->av1_inst.remapped_ref_idx[SW_ALTREF_FRAME - SW_LAST_FRAME] = last3_remapped_idx;
    }
    else if (vcenc_instance->av1_inst.is_arf2_last_frame) {
        vcenc_instance->av1_inst.is_valid_idx[SW_ALTREF2_FRAME - SW_LAST_FRAME] = ENCHW_NO;
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST_FRAME - SW_LAST_FRAME]   = get_ref_frame_map_idx(vcenc_instance, SW_ALTREF2_FRAME);
        vcenc_instance->av1_inst.remapped_ref_idx[SW_ALTREF2_FRAME - SW_LAST_FRAME] = last3_remapped_idx;
    }
    else{
        vcenc_instance->av1_inst.remapped_ref_idx[SW_LAST_FRAME - SW_LAST_FRAME] = last3_remapped_idx;
        ASSERT(!encode_show_existing_frame(vcenc_instance, pEncIn));
    }
    vcenc_instance->av1_inst.is_valid_idx[SW_LAST_FRAME - SW_LAST_FRAME] = ENCHW_YES;

    vcenc_instance->av1_inst.refresh_last_frame = 0;
  }

  if(vcenc_instance->av1_inst.refresh_frame_flags == 0xFF){
    for(i32 i = SW_LAST2_FRAME; i < SW_REF_FRAMES; i++)
        vcenc_instance->av1_inst.is_valid_idx[i - SW_LAST_FRAME] = ENCHW_NO;

    if(pEncIn->u8IdxEncodedAsLTR)
        vcenc_instance->av1_inst.is_valid_idx[SW_GOLDEN_FRAME - SW_LAST_FRAME] = ENCHW_YES;
  }
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetEwl
    Description   : Get the ewl instance.
    Return type   : ewl instance pointer
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
const void *VCEncGetEwl(VCEncInst inst)
{
    const void * ewl;
    if(inst == NULL)
    {
      APITRACEERR("VCEncGetEwl: ERROR Null argument");
      ASSERT(0);
    }
    ewl = ((struct vcenc_instance *)inst)->asic.ewl;
    if(ewl == NULL)
    {
        APITRACEERR("VCEncGetEwl: EWL instance get failed.");
        ASSERT(0);
    }
    return ewl;
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetAsicConfig
    Description   : Get HW configuration.
    Return type   : EWLHwConfig_t
    Argument      : codecFormat - codec format
------------------------------------------------------------------------------*/
EWLHwConfig_t VCEncGetAsicConfig(VCEncVideoCodecFormat codecFormat)
{
  u32 clientType = EWL_CLIENT_TYPE_JPEG_ENC;
  if(IS_H264(codecFormat))
    clientType = EWL_CLIENT_TYPE_H264_ENC;
  else if(IS_HEVC(codecFormat) || IS_AV1(codecFormat))
    clientType = EWL_CLIENT_TYPE_HEVC_ENC;
  else{
    ASSERT(0 && "Unsupported codecFormat");
  }
  return EncAsicGetAsicConfig(EncAsicGetCoreIdByFormat(clientType));
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetPass1UpdatedGopSize
    Description   : Get pass1 updated GopSize.
    Return type   : pass1 updated GopSize.
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
i32 VCEncGetPass1UpdatedGopSize(VCEncInst inst)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  if(pEncInst->pass == 2)
    pEncInst = (struct vcenc_instance *)pEncInst->lookahead.priv_inst;
  struct cuTreeCtr *m_param = &pEncInst->cuTreeCtl;

  pthread_mutex_lock(&m_param->agop_mutex);
  while(m_param->agop.head == NULL) {
    if(pEncInst->lookahead.status != VCENC_OK)
      return 1;
    pthread_cond_wait(&m_param->agop_cond, &m_param->agop_mutex);
  }
  struct agop_res *res = (struct agop_res *)queue_get(&m_param->agop);
  pthread_mutex_unlock(&m_param->agop_mutex);
  i32 size = res->agop_size;
  free(res);

  return size;
}


