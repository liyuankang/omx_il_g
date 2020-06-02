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
--  Description :  Encode picture
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "ewl.h"
#include "encasiccontroller.h"
#include "vp9codeframe.h"
#include "vp9ratecontrol.h"
#include "vp9header.h"
#include "vp9entropy.h"
#include <math.h> /* for pow */


#ifdef INTERNAL_TEST
#include "vp9testid.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Intra 16x16 mode tree penalty values */
static const i32 const intra16ModeTreePenalty[] =
{
  305, 841, 914, 1082, 427
};


/* Intra 4x4 mode tree penalty values */
static const i32 const intra4ModeTreePenalty[] =
{
  280, 622, 832, 1177, 1240, 1341, 1085, 1259, 1357, 1495
};

/* Fitted using octave, floor(2.1532275 + 0.0087831*x + 0.0017989*x^2) */
const i32 weight[] =
{
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4,
  4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6,
  6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9,
  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14,
  14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19,
  19, 19, 20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25,
  25, 26, 26, 26, 27, 27, 28, 28, 29, 29, 29, 30, 30, 31, 31, 32
};

/* experimentally fitted, 24.893*exp(0.02545*qp) */
const i32 vp9SplitPenalty[128] =
{
  24, 25, 26, 26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 36,
  37, 38, 39, 40, 41, 42, 43, 44, 45, 47, 48, 49, 50, 52, 53, 54,
  56, 57, 59, 60, 62, 63, 65, 67, 68, 70, 72, 74, 76, 78, 80, 82,
  84, 86, 88, 91, 93, 95, 98, 100, 103, 106, 108, 111, 114, 117, 120, 123,
  126, 130, 133, 136, 140, 144, 147, 151, 155, 159, 163, 167, 172, 176, 181, 185,
  190, 195, 200, 205, 211, 216, 222, 227, 233, 239, 245, 252, 258, 265, 272, 279,
  286, 293, 301, 309, 317, 325, 333, 342, 351, 360, 369, 379, 388, 398, 409, 419,
  430, 441, 453, 464, 476, 488, 501, 514, 527, 541, 555, 569, 584, 599, 614, 630
};

static const i32 const intraPenalty[128] =
{
  3, 4, 5, 7, 8, 9, 10, 12, 13, 15, 16, 18, 19, 21, 23, 24,
  26, 28, 30, 32, 34, 36, 38, 40, 42, 45, 47, 49, 52, 54, 57, 59,
  62, 65, 68, 70, 73, 76, 79, 82, 86, 89, 92, 96, 99, 103, 106, 110,
  114, 118, 122, 126, 130, 134, 138, 142, 147, 151, 156, 161, 165, 170, 175, 180,
  185, 190, 195, 201, 206, 212, 217, 223, 229, 235, 240, 246, 253, 259, 265, 271,
  278, 284, 291, 298, 304, 311, 318, 325, 333, 340, 347, 354, 362, 369, 377, 385,
  392, 400, 408, 416, 424, 432, 441, 449, 457, 465, 474, 482, 491, 500, 508, 517,
  526, 534, 543, 552, 561, 570, 579, 588, 597, 606, 615, 624, 633, 642, 651, 660
};

/* coefficient for 1P motion estimation dmv penalty */
static const i32 penalty1p[] =
{
  160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
  160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
  160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 168, 177, 186, 194,
  203, 212, 220, 229, 238, 246, 255, 264, 272, 281, 290, 298, 307, 316, 324, 333,
  342, 350, 359, 368, 376, 385, 394, 402, 411, 420, 428, 437, 446, 454, 463, 472,
  480, 489, 498, 506, 515, 524, 532, 541, 550, 558, 567, 576, 584, 593, 602, 610,
  619, 628, 636, 645, 654, 662, 671, 680, 688, 697, 706, 714, 723, 732, 740, 749,
  758, 766, 775, 784, 792, 801, 810, 818, 827, 836, 844, 853, 862, 870, 879, 888
};

/* dmv penalty for 4P/1P me stages, "curve" from libvpx function cal_mvsadcosts, values divided by 256 */
static const i32 dmvPenalty[] =
{
  1, 7, 9, 10, 11, 12, 12, 13, 13, 14, 14, 14, 14, 15, 15, 15,
  15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17,
  17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
  19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20,
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
  20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21
};

static const i32 _lambda[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 41, 42, 43, 44, 45, 46,
  47, 48, 49, 50, 51, 52, 53, 55, 56, 57, 58, 59, 60, 61, 63, 64,
  65, 66, 68, 69, 70, 71, 73, 74, 75, 77, 78, 80, 81, 82, 84, 85,
  87, 88, 90, 91, 93, 94, 96, 97, 99, 101, 102, 104, 105, 107, 109, 111,
  112, 114, 116, 117, 119, 121, 123, 125, 126, 128, 130, 132, 134, 136, 138, 140,
  142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 173
};


static const u32 Vp9IntraPenalty[52] =
{
  24, 24, 24, 26, 27, 30, 32, 35, 39, 43, 48, 53, 58, 64, 71, 78,
  85, 93, 102, 111, 121, 131, 142, 154, 167, 180, 195, 211, 229,
  248, 271, 296, 326, 361, 404, 457, 523, 607, 714, 852, 1034,
  1272, 1588, 2008, 2568, 3318, 4323, 5672, 7486, 9928, 13216,
  17648
};//max*3=16bit

/* Intra Penalty for TU 4x4 vs. 8x8 */
static unsigned short Vp9IntraPenaltyTu4[52] =
{
  7,    7,    8,   10,   11,   12,   14,   15,   17,   20,
  22,   25,   28,   31,   35,   40,   44,   50,   56,   63,
  71,   80,   89,  100,  113,  127,  142,  160,  179,  201,
  226,  254,  285,  320,  359,  403,  452,  508,  570,  640,
  719,  807,  905, 1016, 1141, 1281, 1438, 1614, 1811, 2033,
  2282, 2562
};//max*3=13bit

/* Intra Penalty for TU 8x8 vs. 16x16 */
static unsigned short Vp9IntraPenaltyTu8[52] =
{
  7,    7,    8,   10,   11,   12,   14,   15,   17,   20,
  22,   25,   28,   31,   35,   40,   44,   50,   56,   63,
  71,   80,   89,  100,  113,  127,  142,  160,  179,  201,
  226,  254,  285,  320,  359,  403,  452,  508,  570,  640,
  719,  807,  905, 1016, 1141, 1281, 1438, 1614, 1811, 2033,
  2282, 2562
};//max*3=13bit

/* Intra Penalty for TU 16x16 vs. 32x32 */
static unsigned short Vp9IntraPenaltyTu16[52] =
{
  9,   11,   12,   14,   15,   17,   19,   22,   24,   28,
  31,   35,   39,   44,   49,   56,   62,   70,   79,   88,
  99,  112,  125,  141,  158,  177,  199,  224,  251,  282,
  317,  355,  399,  448,  503,  564,  634,  711,  799,  896,
  1006, 1129, 1268, 1423, 1598, 1793, 2013, 2259, 2536, 2847,
  3196, 3587
};//max*3=14bit

/* Intra Penalty for TU 32x32 vs. 64x64 */
static unsigned short Vp9IntraPenaltyTu32[52] =
{
  9,   11,   12,   14,   15,   17,   19,   22,   24,   28,
  31,   35,   39,   44,   49,   56,   62,   70,   79,   88,
  99,  112,  125,  141,  158,  177,  199,  224,  251,  282,
  317,  355,  399,  448,  503,  564,  634,  711,  799,  896,
  1006, 1129, 1268, 1423, 1598, 1793, 2013, 2259, 2536, 2847,
  3196, 3587
};//max*3=14bit

/* Intra Penalty for Mode a */
static unsigned short Vp9IntraPenaltyModeA[52] =
{
  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  1,    1,    1,    1,    1,    1,    2,    2,    2,    2,
  3,    3,    4,    4,    5,    5,    6,    7,    8,    9,
  10,   11,   13,   15,   16,   19,   21,   23,   26,   30,
  33,   38,   42,   47,   53,   60,   67,   76,   85,   95,
  107,  120
};//max*3=9bit

/* Intra Penalty for Mode b */
static unsigned short Vp9IntraPenaltyModeB[52] =
{
  0,    0,    0,    0,    0,    0,    1,    1,    1,    1,
  1,    1,    2,    2,    2,    2,    3,    3,    4,    4,
  5,    5,    6,    7,    8,    9,   10,   11,   13,   14,
  16,   18,   21,   23,   26,   29,   33,   37,   42,   47,
  53,   59,   66,   75,   84,   94,  106,  119,  133,  150,
  168,  189
};////max*3=10bit

/* Intra Penalty for Mode c */
static unsigned short Vp9IntraPenaltyModeC[52] =
{
  1,    1,    1,    1,    1,    1,    2,    2,    2,    3,
  3,    3,    4,    4,    5,    6,    6,    7,    8,    9,
  10,   12,   13,   15,   17,   19,   21,   24,   27,   31,
  34,   39,   43,   49,   55,   62,   69,   78,   87,   98,
  110,  124,  139,  156,  175,  197,  221,  248,  278,  312,
  351,  394
};//max*3=11bit





/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void VP9SetNewFrame(vp9Instance_s *inst, i32 penaltySet);
static void SetPenalties(vp9Instance_s *inst, i32 penaltySet);
static void SetSegmentation(vp9Instance_s *inst);
static void SetFilterParameters(vp9Instance_s *inst);
static void UpdateAsicStream(vp9Instance_s *inst);
static void EncSwap32(u32 *buf, u32 sizeBytes);
static void GetStatistics(vp9Instance_s *inst);
static void LamdaQp(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad);


/*------------------------------------------------------------------------------

    VP9SetFrameParams

------------------------------------------------------------------------------*/
void VP9SetFrameParams(vp9Instance_s *inst)
{
  pps *pps = inst->ppss.pps;  /* Active picture parameters */
  sps *sps = &inst->sps;
  i32 qp = inst->rateControl.qpHdr;
  i32 i;

  /* Segment parameters, testId/ROI may override qpSgm and
   * auto filter level setting may override levelSgm. */
  for (i = 0; i < 4; i++)
  {
    pps->qpSgm[i] = qp;
    pps->levelSgm[i] = sps->filterLevel;
  }

  /* Adaptive setting for QP deltas based on frame QP. */
  if (sps->autoQpDelta[0]) sps->qpDelta[0] = 0; /* Luma DC */
  if (sps->autoQpDelta[1]) sps->qpDelta[1] = 0; /* 2nd Luma DC */
  if (sps->autoQpDelta[2]) sps->qpDelta[2] = 0; /* 2nd Luma AC */
  if (sps->autoQpDelta[3]) sps->qpDelta[3] = MIN(0, 27 - qp / 3); /* Chroma DC */
  if (sps->autoQpDelta[4]) sps->qpDelta[4] = 0; /* Chroma AC */
}

/*------------------------------------------------------------------------------

    GetStatistics

        Gather relevant statistics from a coded frame.

------------------------------------------------------------------------------*/
void GetStatistics(vp9Instance_s *inst)
{
  u32 i;
  u32 tmp;
  u32 cntG = 0;
  statPeriod *stat = &inst->statPeriod;
  encOutputMbInfo_s *mbInfo;
  i32 x, y;
  i32 skip = 0, intra = 0;
  asicData_s *asic = &inst->asic;

  /* TODO asserts */

  /* Skip calculation when adaptive golden update not in use. */
  if (!inst->rateControl.goldenPictureRate ||
      !inst->rateControl.adaptiveGoldenUpdate)
    return;

  /* Later stats are useless for a keyframe */
  if (inst->picBuffer.cur_pic->i_frame)
    return;

  /* Loop mvs */
  for (i = 0 ; i < inst->mbPerFrame ; ++i)
  {
    mbInfo = (encOutputMbInfo_s *)EncAsicGetMvOutput(asic, i);

    x = mbInfo->mvX[0];
    y = mbInfo->mvY[0];
    if (mbInfo->nonZeroCnt == 0)
      skip++;
    /* TODO check separate counts for 16x16 and 4x4 */
    if ((mbInfo->mode & MBOUT_9_MODE_MASK) <= 4)
      intra++;

    stat->mvSumX[i] += x;
    stat->mvSumY[i] += y;
  }

  stat->skipCnt += skip;
  stat->intraCnt += intra;
  stat->skipDiv += inst->mbPerFrame;

  /* Golden prediction not used if prev frame is golden */
  if (inst->picBuffer.last_pic &&
      inst->picBuffer.last_pic->grf)
    return;

  for (i = 0; i < inst->mbPerFrame; i++)
  {
    mbInfo = (encOutputMbInfo_s *)EncAsicGetMvOutput(asic, i);
    tmp = (mbInfo->mode & MBOUT_9_REFIDX_MASK) >> 6;
    cntG += (tmp == 1);
  }

  /* Accumulate stats */
  stat->goldenDiv += inst->mbPerFrame;
  stat->goldenCnt += cntG;
}


/*------------------------------------------------------------------------------

    ProcessStatistics

        Process statistics accumulated on period. Calculate approriate boost
        percentage for next period start frame.

        Return 0 if golden frame is shouldn't be updated. 1 otherwise.

------------------------------------------------------------------------------*/
u32 ProcessStatistics(vp9Instance_s *inst, i32 *boost)
{
  i32 usagePct = 10;
  u32 update = 1;
  i32 zero = 0;
  u32 i;
  i32 x, y;
  i32 threshold;
  i32 tmp;
  statPeriod *stat          = &inst->statPeriod;
  vp9RateControl_s *rc    = &inst->rateControl;

  /* Calculate golden frame usage from previous period. If golden usage is
   * over 1% we don't update. */
  if (stat->goldenDiv && rc->goldenRefreshThreshold)
  {
    usagePct = (1000 * stat->goldenCnt + stat->goldenDiv / 2) / (stat->goldenDiv);
    if (usagePct >= rc->goldenRefreshThreshold)
      update = 0; /* TODO needs another criteria in case golden frame
                         * is bad quality */
  }
  /*prevUsagePct = usagePct;*/

  for (i = 0 ; i < inst->mbPerFrame ; ++i)
  {
    x = stat->mvSumX[i] >> 2;
    y = stat->mvSumY[i] >> 2;
    if (!(x | y))
      zero++;
  }

  if (usagePct > 10)
    usagePct = 10;
  if (usagePct == 0)
    usagePct = 1;

  /* Determine total amount of boost based on zero resultant vector MBs in
   * period */
  threshold = inst->mbPerFrame * rc->goldenBoostThreshold / 100;
  if (zero < threshold && threshold)
    *boost = (10 * usagePct * zero * rc->adaptiveGoldenBoost) /
             (10 * threshold);
  else
    *boost = 10 * usagePct * rc->adaptiveGoldenBoost / 10;

  if (stat->skipDiv)
  {
    i32 codedPct = 100 - 100 * stat->skipCnt / stat->skipDiv;

    /* If less than 10% coded mbs, boost a bit. */
    if (codedPct < 20)
      *boost += (10 * (20 - codedPct)) / 5;
  }

  /* Reset counters for next period */
  stat->goldenDiv = 0;
  stat->goldenCnt = 0;
  stat->skipDiv = 0;
  stat->intraCnt = 0;
  stat->skipCnt = 0;
  tmp = inst->mbPerFrame * sizeof(i32);
  EWLmemset(stat->mvSumX, 0, tmp);
  EWLmemset(stat->mvSumY, 0, tmp);

  return update;
}

/*------------------------------------------------------------------------------

    VP9CodeFrameMultiPass

        Encode frame in multiple passes. Functionality is divided into two;
        first we run all but the last pass using probability data from the
        previous frame. We perform some trial-and-errors here in order to
        optimize coding parameters. Reconstructed picture is not written nor
        stored at this point. Then we encode the final pass with the best
        parameters.

------------------------------------------------------------------------------*/
vp9EncodeFrame_e VP9CodeFrameMultiPass(vp9Instance_s *inst)
{
  asicData_s *asic = &inst->asic;
  vp9EncodeFrame_e ret;
  u32 status = ASIC_STATUS_ERROR;
  regValues_s *regs = &inst->asic.regs;
  u32 pass;
  regValues_s regsTmp;
  entropy entropyTmp;
  u32 numExtraPass = inst->maxNumPasses - 1;

  /* Get copies of IF on top of which we perform the last pass */
  //regs->recWriteDisable = 1;
  regsTmp = *regs;
  entropyTmp = *inst->entropy;
  inst->passNbr = 0;

  /* Run all but final pass */
  for (pass = 0 ; pass < numExtraPass ; ++pass)
  {
    /* Initialize probability tables for frame header. */
    InitEntropy(inst);

    /* TODO finalize a list of stuff to try
       -QP throttling based on MSE
       -golden/previous motion search
       -loopfilter level selection
       -prediction filter selection
    */

    SetSegmentation(inst);

    SetFilterParameters(inst);

    VP9SetNewFrame(inst, 0);

    /* Write final probability tables for ASIC. */
    WriteEntropyTables(inst, true);

    EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs);

    {
      /* Encode one frame */
      i32 ewl_ret;

      ewl_ret = EWLWaitHwRdy(asic->ewl, NULL,regs->sliceNum,&status);

      if (ewl_ret != EWL_OK)
      {
        status = ASIC_STATUS_ERROR;

        if (ewl_ret == EWL_ERROR)
        {
          /* IRQ error => Stop and release HW */
          ret = VP9ENCODE_SYSTEM_ERROR;
        }
        else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
        {
          /* IRQ Timeout => Stop and release HW */
          ret = VP9ENCODE_TIMEOUT;
        }

        EncAsicStop(asic->ewl);
        /* Release HW so that it can be used by other codecs */
        EWLReleaseHw(asic->ewl);

      }
      else
      {
        /* Check ASIC status bits and possibly release HW */
        status = EncAsicCheckStatus_V2(asic,status);

        switch (status)
        {
          case ASIC_STATUS_ERROR:
            UpdateAsicStream(inst);  /* Output the stream for debugging */
            ret = VP9ENCODE_HW_ERROR;
            break;
          case ASIC_STATUS_HW_TIMEOUT:
            UpdateAsicStream(inst);  /* Output the stream for debugging */
            ret = VP9ENCODE_TIMEOUT;
            break;
          case ASIC_STATUS_SLICE_READY: /* Slice ready not possible for VP9 */
            ret = VP9ENCODE_HW_ERROR;
            break;
          case ASIC_STATUS_BUFF_FULL:
            /* One of the buffers overflowed, API checks control buffer */
            inst->buffer[1].size = 0;
            ret = VP9ENCODE_OK;
            break;
          case ASIC_STATUS_HW_RESET:
            ret = VP9ENCODE_HW_RESET;
            break;
          case ASIC_STATUS_FRAME_READY:
            ret = VP9ENCODE_OK;
            break;
          default:
            /* should never get here */
            ASSERT(0);
            ret = VP9ENCODE_HW_ERROR;
        }
      }

      if (ret != VP9ENCODE_OK)
        return ret; /* Bail out */
    }

    inst->passNbr++;
    /* Rollback any registry and IF changes */
    *regs = regsTmp;
    *inst->entropy = entropyTmp;
#if 0
    intra16RecReadEnable = 1; /* Following passes use better approx */
#endif

  }

  /* Run final pass */
  //regs->recWriteDisable = 0;

  /* Initialize probability tables for frame header. */
  InitEntropy(inst);

  SetSegmentation(inst);

  SetFilterParameters(inst);

  /* Write frame headers, also updates segmentation probs. */
  VP9FrameHeader(inst);
  /* SW workaround for carry, rewrite headers in case of carry possibility. */
  {
    u32 carryMask = ((1 << (32 - inst->buffer[1].bitsLeft)) - 1) & 0xFFFFFF00;
    if ((inst->buffer[1].bottom & carryMask) == carryMask)
    {
      entropy *entropy = inst->entropy;
      /* Don't update probabilities for this frame. */
      EWLmemcpy(entropy->coeffProb, entropy->oldCoeffProb, sizeof(entropy->coeffProb));
      EWLmemcpy(entropy->mvProb, entropy->oldMvProb, sizeof(entropy->mvProb));
      VP9SetBuffer(&inst->buffer[1], inst->buffer[1].pData, inst->buffer[1].size);
      VP9FrameHeader(inst);
    }
  }

  VP9FrameHeaderFinish(inst);

  VP9SetNewFrame(inst, 0); /* set to 1 when we got correct intra recon pxls */

  /* Write final probability tables for ASIC. */
  WriteEntropyTables(inst, true);

  EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs);

  {
    /* Encode one frame */
    i32 ewl_ret;

    ewl_ret = EWLWaitHwRdy(asic->ewl, NULL,regs->sliceNum,&status);

    if (ewl_ret != EWL_OK)
    {
      status = ASIC_STATUS_ERROR;

      if (ewl_ret == EWL_ERROR)
      {
        /* IRQ error => Stop and release HW */
        ret = VP9ENCODE_SYSTEM_ERROR;
      }
      else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
      {
        /* IRQ Timeout => Stop and release HW */
        ret = VP9ENCODE_TIMEOUT;
      }

      EncAsicStop(asic->ewl);
      /* Release HW so that it can be used by other codecs */
      EWLReleaseHw(asic->ewl);

    }
    else
    {
      /* Check ASIC status bits and possibly release HW */
      status = EncAsicCheckStatus_V2(asic,status);

      switch (status)
      {
        case ASIC_STATUS_ERROR:
          UpdateAsicStream(inst);  /* Output the stream for debugging */
          ret = VP9ENCODE_HW_ERROR;
          break;
        case ASIC_STATUS_HW_TIMEOUT:
          UpdateAsicStream(inst);  /* Output the stream for debugging */
          ret = VP9ENCODE_TIMEOUT;
          break;
        case ASIC_STATUS_SLICE_READY: /* Slice ready not possible for VP9 */
          ret = VP9ENCODE_HW_ERROR;
          break;
        case ASIC_STATUS_BUFF_FULL:
          /* One of the buffers overflowed, API checks control buffer */
          inst->buffer[1].size = 0;
          ret = VP9ENCODE_OK;
          break;
        case ASIC_STATUS_HW_RESET:
          ret = VP9ENCODE_HW_RESET;
          break;
        case ASIC_STATUS_FRAME_READY:
          UpdateAsicStream(inst);
          ret = VP9ENCODE_OK;
          break;
        default:
          /* should never get here */
          ASSERT(0);
          ret = VP9ENCODE_HW_ERROR;
      }
    }
  }

  VP9FrameTag(inst);
  /* Check that there's space in the end of control partition to write
   * the size of second residual partition. */
  if (VP9BufferGap(&inst->buffer[1], 4) == ENCHW_OK)
    VP9DataPartitionSizes(inst);
  if (status == ASIC_STATUS_FRAME_READY) GetStatistics(inst);
  return ret;
}

/*------------------------------------------------------------------------------

    VP9CodeFrame

------------------------------------------------------------------------------*/
vp9EncodeFrame_e VP9CodeFrame(vp9Instance_s *inst)
{
  asicData_s *asic = &inst->asic;
  vp9EncodeFrame_e ret;
  u32 status = ASIC_STATUS_ERROR;

  inst->passNbr = 0;
  /*
  intra16RecReadEnable = 0;
  intra16RecWriteEnable = 0;
  pIntra16RecPix = tmpBuffer;
  pMSE = mse;*/

  /* Initialize probability tables for frame header. */
  InitEntropy(inst);

  SetSegmentation(inst);

  SetFilterParameters(inst);

  /* Write frame headers, also updates segmentation probs. */
  VP9FrameHeader(inst);
  /* SW workaround for carry, rewrite headers in case of carry possibility. */
  {
    u32 carryMask = ((1 << (32 - inst->buffer[1].bitsLeft)) - 1) & 0xFFFFFF00;
    if ((inst->buffer[1].bottom & carryMask) == carryMask)
    {
      entropy *entropy = inst->entropy;
      /* Don't update probabilities for this frame. */
      EWLmemcpy(entropy->coeffProb, entropy->oldCoeffProb, sizeof(entropy->coeffProb));
      EWLmemcpy(entropy->mvProb, entropy->oldMvProb, sizeof(entropy->mvProb));
      VP9SetBuffer(&inst->buffer[1], inst->buffer[1].pData, inst->buffer[1].size);
      VP9FrameHeader(inst);
    }
  }

  VP9FrameHeaderFinish(inst);

  VP9SetNewFrame(inst, 0);

  /* Write final probability tables for ASIC. */
  WriteEntropyTables(inst, false);

#ifdef INTERNAL_TEST
  /* Configure the ASIC penalty values according to the test vector */
  Vp9ConfigureTestPenalties(inst);
#endif

  EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs);

  {
    /* Encode one frame */
    i32 ewl_ret;

    ewl_ret = EWLWaitHwRdy(asic->ewl, NULL,asic->regs.sliceNum,&status);

    if (ewl_ret != EWL_OK)
    {
      status = ASIC_STATUS_ERROR;

      if (ewl_ret == EWL_ERROR)
      {
        /* IRQ error => Stop and release HW */
        ret = VP9ENCODE_SYSTEM_ERROR;
      }
      else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
      {
        /* IRQ Timeout => Stop and release HW */
        ret = VP9ENCODE_TIMEOUT;
      }

      EncAsicStop(asic->ewl);
      /* Release HW so that it can be used by other codecs */
      EWLReleaseHw(asic->ewl);

    }
    else
    {
      /* Check ASIC status bits and possibly release HW */
      status = EncAsicCheckStatus_V2(asic,status);

      switch (status)
      {
        case ASIC_STATUS_ERROR:
          UpdateAsicStream(inst);  /* Output the stream for debugging */
          ret = VP9ENCODE_HW_ERROR;
          break;
        case ASIC_STATUS_HW_TIMEOUT:
          UpdateAsicStream(inst);  /* Output the stream for debugging */
          ret = VP9ENCODE_TIMEOUT;
          break;
        case ASIC_STATUS_SLICE_READY: /* Slice ready not possible for VP9 */
          ret = VP9ENCODE_HW_ERROR;
          break;
        case ASIC_STATUS_BUFF_FULL:
          /* One of the buffers overflowed, API checks control buffer */
          inst->buffer[1].size = 0;
          ret = VP9ENCODE_OK;
          break;
        case ASIC_STATUS_HW_RESET:
          ret = VP9ENCODE_HW_RESET;
          break;
        case ASIC_STATUS_FRAME_READY:
          UpdateAsicStream(inst);
          ret = VP9ENCODE_OK;
          break;
        default:
          /* should never get here */
          ASSERT(0);
          ret = VP9ENCODE_HW_ERROR;
      }
    }
  }

  VP9FrameTag(inst);
  /* Check that there's space in the end of control partition to write
   * the size of second residual partition. */
  if (VP9BufferGap(&inst->buffer[1], 4) == ENCHW_OK)
    VP9DataPartitionSizes(inst);

  if (status == ASIC_STATUS_FRAME_READY) GetStatistics(inst);

  return ret;
}

/*------------------------------------------------------------------------------

    Set encoding parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
void VP9SetNewFrame(vp9Instance_s *inst, i32 penaltySet)
{
  regValues_s *regs = &inst->asic.regs;
  sps *sps = &inst->sps;
  i32 i;
  i32 maxSgm = SGM_CNT;

  if (!inst->ppss.pps->segmentEnabled) maxSgm = 1;

  //regs->frameNum = inst->frameCnt;

  /* We tell HW the size of DCT partition buffers, they all are equal size.
   * There is no overflow check for control partition on ASIC, but since
   * the stream buffers are in one linear memory the overflow of control
   * partition will only corrupt the first DCT partition and SW will
   * notice this and discard the frame. */
  //    regs->outputStrmSize /= 8;  /* 64-bit addresses */
  //    regs->outputStrmSize &= (~0x07);    /* 8 multiple size */

  /* Since frame tag is 10 bytes the stream base is not 64-bit aligned.
   * Now the frame headers have been written so we must align the base for
   * HW and set the header remainder properly. */
  regs->outputStrmBase += inst->buffer[1].byteCnt;

  /* bit offset in the last 64-bit word */
  //regs->firstFreeBit = (regs->outputStrmBase & 0x07) * 8;

  /* 64-bit aligned HW base */
  //regs->outputStrmBase = regs->outputStrmBase & (~0x07);
#if 0
  /* header remainder is byte aligned, max 7 bytes = 56 bits */
  if (regs->firstFreeBit != 0)
  {
    /* 64-bit aligned stream pointer */
    u8 *pTmp = (u8 *)((size_t)(inst->buffer[1].data) & (u32)(~0x07));
    u32 val;

    /* Clear remaining bits */
    for (val = 6; val >= regs->firstFreeBit / 8; val--)
      pTmp[val] = 0;

    val = pTmp[0] << 24;
    val |= pTmp[1] << 16;
    val |= pTmp[2] << 8;
    val |= pTmp[3];

    regs->strmStartMSB = val;  /* 32 bits to MSB */

    if (regs->firstFreeBit > 32)
    {
      val = pTmp[4] << 24;
      val |= pTmp[5] << 16;
      val |= pTmp[6] << 8;

      regs->strmStartLSB = val;
    }
    else
      regs->strmStartLSB = 0;
  }
  else
  {
    regs->strmStartMSB = regs->strmStartLSB = 0;
  }
#endif
  /* VP8 stream profile: bicubic/bilinear */
  regs->ipolFilterMode = sps->profile;



  /* Quarter pixel MV mode */
  if (sps->quarterPixelMv == 0)
    regs->disableQuarterPixelMv = 1;
  else if (sps->quarterPixelMv == 1)
  {
    /* Adaptive setting. When resolution larger than 1080p = 8160 macroblocks
     * there is not enough time to do 1/4 pixel ME */
    if (inst->mbPerFrame > 8160)
      regs->disableQuarterPixelMv = 1;
    else
      regs->disableQuarterPixelMv = 0;
  }
  else
    regs->disableQuarterPixelMv = 0;

  /* Cabac enable bit signals ASIC to read probability tables */
  //regs->enableCabac = 1;


  regs->picWidth = sps->picWidthInMbs;
  regs->picHeight = sps->picHeightInMbs;

  /* Split MV mode */
  if (sps->splitMv == 0)
    regs->splitMvMode = 0;
  else if (sps->splitMv == 1)
  {
    /* Adaptive setting. When resolution larger than 720p = 3600 macroblocks
     * there is no benefit from using split MVs */
    if (inst->mbPerFrame > 3600)
      regs->splitMvMode = 0;
    else
      regs->splitMvMode = 1;
  }
  else
    regs->splitMvMode = 1;
#if 0
  /* Quantization tables for each segment */
  for (i = 0; i < maxSgm; i++)
  {
    i32 qp[6];
    qp[0] = MIN(MAX(inst->ppss.pps->qpSgm[i] + inst->sps.qpDelta[0], 0), 127);
    qp[1] = inst->ppss.pps->qpSgm[i];
    qp[2] = MIN(MAX(inst->ppss.pps->qpSgm[i] + inst->sps.qpDelta[1], 0), 127);
    qp[3] = MIN(MAX(inst->ppss.pps->qpSgm[i] + inst->sps.qpDelta[2], 0), 127);
    qp[4] = MIN(MAX(inst->ppss.pps->qpSgm[i] + inst->sps.qpDelta[3], 0), 127);
    qp[5] = MIN(MAX(inst->ppss.pps->qpSgm[i] + inst->sps.qpDelta[4], 0), 127);
    regs->qpY1QuantDc[i] = inst->qpY1[qp[0]].quant[0];
    regs->qpY1QuantAc[i] = inst->qpY1[qp[1]].quant[1];
    regs->qpY2QuantDc[i] = inst->qpY2[qp[2]].quant[0];
    regs->qpY2QuantAc[i] = inst->qpY2[qp[3]].quant[1];
    regs->qpChQuantDc[i] = inst->qpCh[qp[4]].quant[0];
    regs->qpChQuantAc[i] = inst->qpCh[qp[5]].quant[1];
    regs->qpY1ZbinDc[i] = inst->qpY1[qp[0]].zbin[0];
    regs->qpY1ZbinAc[i] = inst->qpY1[qp[1]].zbin[1];
    regs->qpY2ZbinDc[i] = inst->qpY2[qp[2]].zbin[0];
    regs->qpY2ZbinAc[i] = inst->qpY2[qp[3]].zbin[1];
    regs->qpChZbinDc[i] = inst->qpCh[qp[4]].zbin[0];
    regs->qpChZbinAc[i] = inst->qpCh[qp[5]].zbin[1];
    regs->qpY1RoundDc[i] = inst->qpY1[qp[0]].round[0];
    regs->qpY1RoundAc[i] = inst->qpY1[qp[1]].round[1];
    regs->qpY2RoundDc[i] = inst->qpY2[qp[2]].round[0];
    regs->qpY2RoundAc[i] = inst->qpY2[qp[3]].round[1];
    regs->qpChRoundDc[i] = inst->qpCh[qp[4]].round[0];
    regs->qpChRoundAc[i] = inst->qpCh[qp[5]].round[1];
    regs->qpY1DequantDc[i] = inst->qpY1[qp[0]].dequant[0];
    regs->qpY1DequantAc[i] = inst->qpY1[qp[1]].dequant[1];
    regs->qpY2DequantDc[i] = inst->qpY2[qp[2]].dequant[0];
    regs->qpY2DequantAc[i] = inst->qpY2[qp[3]].dequant[1];
    regs->qpChDequantDc[i] = inst->qpCh[qp[4]].dequant[0];
    regs->qpChDequantAc[i] = inst->qpCh[qp[5]].dequant[1];

    regs->filterLevel[i] = inst->ppss.pps->levelSgm[i];
  }
#endif
  regs->boolEncValue = inst->buffer[1].bottom;
  regs->boolEncValueBits = 24 - inst->buffer[1].bitsLeft;
  regs->boolEncRange = inst->buffer[1].range;

  //    regs->cpTarget = NULL;

  /* Select frame type */
  if (inst->picBuffer.cur_pic->i_frame)
    regs->frameCodingType = ASIC_INTRA;
  else
    regs->frameCodingType = ASIC_INTER;

  /* HW base address for partition sizes */
  regs->sizeTblBase = inst->asic.sizeTbl.busAddress;

  /* HW Base must be 64-bit aligned */
  ASSERT(regs->sizeTblBase % 8 == 0);

  regs->dctPartitions          = sps->dctPartitions;
  regs->filterDisable          = sps->filterType;
  regs->filterSharpness        = sps->filterSharpness;
  regs->segmentEnable          = inst->ppss.pps->segmentEnabled;
  regs->segmentMapUpdate       = inst->ppss.pps->sgm.mapModified;

  /* For next frame the segmentation map is not needed unless it is modified. */
  inst->ppss.pps->sgm.mapModified = false;

  for (i = 0; i < 4; i++)
  {
    regs->lfRefDelta[i] = sps->refDelta[i];
    regs->lfModeDelta[i] = sps->modeDelta[i];
  }

  //if (regs->asicHwId < ASIC_ID_CLOUDBERRY)
  //     SetPenalties(inst, penaltySet);

  /* average variance of previous frame for activity masking */
  if (inst->qualityMetric == VP9ENC_SSIM && regs->avgVar)
  {
    /*regs->avgVar = regs->avgVar/inst->mbPerFrame;*/
    regs->avgVar = 300;
    regs->invAvgVar = 16384 / MAX(1, regs->avgVar);
  }
  else
    regs->avgVar = 0; /* implicit disable of activity masking */


  regs->intraPenaltyPic4x4  = Vp9IntraPenaltyTu4[regs->qp * 2 / 5];
  regs->intraPenaltyPic8x8  = Vp9IntraPenaltyTu8[regs->qp * 2 / 5];
  regs->intraPenaltyPic16x16 = Vp9IntraPenaltyTu16[regs->qp * 2 / 5];
  regs->intraPenaltyPic32x32 = Vp9IntraPenaltyTu32[regs->qp * 2 / 5];

  regs->intraMPMPenaltyPic1 = Vp9IntraPenaltyModeA[regs->qp * 2 / 5];
  regs->intraMPMPenaltyPic2 = Vp9IntraPenaltyModeB[regs->qp * 2 / 5];
  regs->intraMPMPenaltyPic3 = Vp9IntraPenaltyModeC[regs->qp * 2 / 5];

  u32 roi1_qp = CLIP3(0, 51, ((int)regs->qp - (int)regs->roi1DeltaQp));
  regs->intraPenaltyRoi14x4 = Vp9IntraPenaltyTu4[roi1_qp * 2 / 5];
  regs->intraPenaltyRoi18x8 = Vp9IntraPenaltyTu8[roi1_qp * 2 / 5];
  regs->intraPenaltyRoi116x16 = Vp9IntraPenaltyTu16[roi1_qp * 2 / 5];
  regs->intraPenaltyRoi132x32 = Vp9IntraPenaltyTu32[roi1_qp * 2 / 5];

  regs->intraMPMPenaltyRoi11 = Vp9IntraPenaltyModeA[roi1_qp * 2 / 5];
  regs->intraMPMPenaltyRoi12 = Vp9IntraPenaltyModeB[roi1_qp * 2 / 5];
  regs->intraMPMPenaltyRoi13 = Vp9IntraPenaltyModeC[roi1_qp * 2 / 5];

  u32 roi2_qp = CLIP3(0, 51, ((int)regs->qp - (int)regs->roi2DeltaQp));
  regs->intraPenaltyRoi24x4 = Vp9IntraPenaltyTu4[roi2_qp * 2 / 5];
  regs->intraPenaltyRoi28x8 = Vp9IntraPenaltyTu8[roi2_qp * 2 / 5];
  regs->intraPenaltyRoi216x16 = Vp9IntraPenaltyTu16[roi2_qp * 2 / 5];
  regs->intraPenaltyRoi232x32 = Vp9IntraPenaltyTu32[roi2_qp * 2 / 5];

  regs->intraMPMPenaltyRoi21 = Vp9IntraPenaltyModeA[roi2_qp * 2 / 5]; //need to change
  regs->intraMPMPenaltyRoi22 = Vp9IntraPenaltyModeB[roi2_qp * 2 / 5]; //need to change
  regs->intraMPMPenaltyRoi23 = Vp9IntraPenaltyModeC[roi2_qp * 2 / 5]; //need to change

  /*inter prediction parameters*/
  LamdaQp(regs->qp, &regs->lamda_motion_sse, &regs->lambda_motionSAD);
  LamdaQp(regs->qp - regs->roi1DeltaQp, &regs->lamda_motion_sse_roi1, &regs->lambda_motionSAD_ROI1);
  LamdaQp(regs->qp - regs->roi2DeltaQp, &regs->lamda_motion_sse_roi2, &regs->lambda_motionSAD_ROI2);
  regs->skip_chroma_dc_threadhold = 2;

  /*tuning on intra cu cost bias*/
  regs->bits_est_bias_intra_cu_8 = 22; //25;
  regs->bits_est_bias_intra_cu_16 = 40; //48;
  regs->bits_est_bias_intra_cu_32 = 86; //108;
  regs->bits_est_bias_intra_cu_64 = 38 * 8; //48*8;

  regs->bits_est_1n_cu_penalty = 5;
  regs->bits_est_tu_split_penalty = 3;
  regs->inter_skip_bias = 124;

  /*small resolution, smaller CU/TU prefered*/
  if (sps->picWidthInMbs < 832 && sps->picHeightInMbs < 480)
  {
    regs->bits_est_1n_cu_penalty = 0;
    regs->bits_est_tu_split_penalty = 2;
  }

  EWLmemset(inst->asic.probCount.virtualAddress, 0,
            inst->asic.probCount.size);

#if defined(ASIC_WAVE_TRACE_TRIGGER)
  {
    u32 index;

    for (index = 0; index < inst->numRefBuffsLum; index++)
    {
      if (inst->asic.regs.internalImageLumBaseW ==
          inst->asic.internalImageLuma[index].busAddress)
      {
        EWLmemset(inst->asic.internalImageLuma[index].virtualAddress, 0,
                  inst->asic.internalImageLuma[index].size);
        break;
      }
    }
  }
#endif

}
#if 0
/*------------------------------------------------------------------------------
      VP9InitPenalties for penalties that are set in the beginning of stream
------------------------------------------------------------------------------*/
void VP9InitPenalties(vp9Instance_s *inst)
{
  regValues_s *regs = &inst->asic.regs;
  i32 i;

  /* 4P/1P DMV penalty table */
  for (i = 0; i < ASIC_PENALTY_TABLE_SIZE; i++)
  {
    regs->dmvPenalty[i] = dmvPenalty[i];
  }
}
#endif
#if 0
/*------------------------------------------------------------------------------
    SetPenalties for penalties in the beginning of each frame
------------------------------------------------------------------------------*/
void SetPenalties(vp9Instance_s *inst, i32 penaltySet)
{
  i32 s, i;
  regValues_s *regs = &inst->asic.regs;
  i32 qp = inst->rateControl.qpHdr;
  i32 c = _lambda[qp];
  i32 maxSgm = SGM_CNT;

  if (!inst->ppss.pps->segmentEnabled) maxSgm = 1;

  /* Set penalty values for every segment based on segment QP */
  for (s = 0; s < maxSgm; s++)
  {
    qp = inst->ppss.pps->qpSgm[s];
    c = _lambda[qp];

    /* Intra 4x4 mode */
    for (i = 0; i < 10; i++)
      regs->pen[s][ASIC_PENALTY_I4MODE0 + i] = intra4ModeTreePenalty[i] * c / 2 >> 8;

    /* Intra 16x16 mode */
    for (i = 0; i < 4; i++)
      regs->pen[s][ASIC_PENALTY_I16MODE0 + i] = intra16ModeTreePenalty[i] * c / 2 >> 8;

    /* If favor has not been set earlier by testId use default */
    regs->pen[s][ASIC_PENALTY_I16FAVOR] = 4 * intra16ModeTreePenalty[4] * c / 2 >> 8;

    if (penaltySet)
      regs->pen[s][ASIC_PENALTY_INTER_FAVOR] = intraPenalty[qp] / 4; /* TODO */
    else
    {
      switch (inst->qualityMetric)
      {
        case VP9ENC_SSIM:
          regs->pen[s][ASIC_PENALTY_INTER_FAVOR] = intraPenalty[qp] / 2;
          break;
        case VP9ENC_PSNR:
          regs->pen[s][ASIC_PENALTY_INTER_FAVOR] = intraPenalty[qp];
          break;
        default:
          ASSERT(0);
      }
    }

    regs->pen[s][ASIC_PENALTY_DMV_4P] = (65 + 4) / 8;
    regs->pen[s][ASIC_PENALTY_DMV_1P] = (penalty1p[qp] + 4) / 8;
    regs->pen[s][ASIC_PENALTY_DMV_QP] = weight[qp];
    regs->pen[s][ASIC_PENALTY_SKIP] = 0;
    regs->pen[s][ASIC_PENALTY_GOLDEN] = MAX(0, 5 * qp / 4 - 10);
    regs->pen[s][ASIC_PENALTY_SPLIT16x8] = MIN(1023, vp9SplitPenalty[qp] / 2);
    regs->pen[s][ASIC_PENALTY_SPLIT8x8] = MIN(1023, (2 * vp9SplitPenalty[qp] + 40) / 4);
    regs->pen[s][ASIC_PENALTY_SPLIT4x4] = MIN(511, (8 * vp9SplitPenalty[qp] + 500) / 16);
    regs->pen[s][ASIC_PENALTY_SPLIT_ZERO] = MIN(255, vp9SplitPenalty[qp] / 10);

    regs->pen[s][ASIC_PENALTY_SPLIT8x4] = 0x3FF; /* No 8x4 MVs in VP9 */
    regs->pen[s][ASIC_PENALTY_I4_PREV_MODE_FAVOR] = 0;

    /* Params needed in intra/inter mode selection */
    regs->pen[s][ASIC_PENALTY_DMV_COST_CONST] = c * 128 / weight[qp];

    SetModeCosts(inst, c, s);
  }

  /* Quarter-pixel DMV penalty table */
  for (i = 0; i < ASIC_PENALTY_TABLE_SIZE; i++)
  {
    i32 y, x;

    y = CostMv(i * 2, inst->entropy->mvProb[0]); /* mv y */
    x = CostMv(i * 2, inst->entropy->mvProb[1]); /* mv x */
    regs->dmvQpelPenalty[i] = MIN(255, (y + x + 1) / 2 * 8 >> 8);
  }

}
#endif
/*------------------------------------------------------------------------------
    SetSegmentMap
------------------------------------------------------------------------------*/
void SetSegmentMap(vp9Instance_s *inst, i32 reorder)
{
  regValues_s *regs = &inst->asic.regs;
  pps *pps = inst->ppss.pps;  /* Active picture parameters */
  i32 cnt[SGM_CNT] = {0};
  i32 *ids;
  u32 x, y, mb, mask, id;
  u32 *map = inst->asic.segmentMap.virtualAddress;

  if (!reorder)
  {
    /* Estimate the amount of MBs in each segment.*/
    cnt[SGM_BG] = inst->mbPerFrame;
    cnt[SGM_AROI] = inst->preProcess.roiMbCount[2];
    if (regs->roi1DeltaQp) cnt[SGM_ROI1] = (regs->roi1Bottom - regs->roi1Top + 1) *
                                             (regs->roi1Right - regs->roi1Left + 1);
    if (regs->roi2DeltaQp) cnt[SGM_ROI2] = (regs->roi2Bottom - regs->roi2Top + 1) *
                                             (regs->roi2Right - regs->roi2Left + 1);
  }
  else
  {
    /* Exact segment ID counts are known so use those to reorder. */
    cnt[SGM_BG] = pps->sgm.idCnt[pps->sgmQpMapping[SGM_BG]];
    cnt[SGM_AROI] = pps->sgm.idCnt[pps->sgmQpMapping[SGM_AROI]];
    cnt[SGM_ROI1] = pps->sgm.idCnt[pps->sgmQpMapping[SGM_ROI1]];
    cnt[SGM_ROI2] = pps->sgm.idCnt[pps->sgmQpMapping[SGM_ROI2]];
  }

  EWLmemset(pps->sgm.idCnt, 0, sizeof(pps->sgm.idCnt));
  EWLmemset(pps->sgmQpMapping, 0, sizeof(pps->sgmQpMapping));

  /* Order segments so that segment with highest MB count has lowest ID. */
  ids = pps->sgmQpMapping;
  (cnt[SGM_BG] < cnt[SGM_ROI1]) ? ids[SGM_BG]++ : ids[SGM_ROI1]++;
  (cnt[SGM_BG] < cnt[SGM_ROI2]) ? ids[SGM_BG]++ : ids[SGM_ROI2]++;
  (cnt[SGM_BG] < cnt[SGM_AROI]) ? ids[SGM_BG]++ : ids[SGM_AROI]++;
  (cnt[SGM_AROI] < cnt[SGM_ROI1]) ? ids[SGM_AROI]++ : ids[SGM_ROI1]++;
  (cnt[SGM_AROI] < cnt[SGM_ROI2]) ? ids[SGM_AROI]++ : ids[SGM_ROI2]++;
  (cnt[SGM_ROI1] < cnt[SGM_ROI2]) ? ids[SGM_ROI1]++ : ids[SGM_ROI2]++;

  /* Shortcut for segmentation disabled. */
  if (cnt[SGM_AROI] + cnt[SGM_ROI1] + cnt[SGM_ROI2] == 0)
  {
    EWLmemset(inst->asic.segmentMap.virtualAddress, 0,
              inst->asic.segmentMap.size);
    pps->sgm.idCnt[0] = cnt[SGM_BG];
    return;
  }

  /* Create the segment map and count macroblocks in each segment. */
  for (y = 0, mb = 0, mask = 0; y < inst->mbPerCol; y++)
  {
    for (x = 0; x < inst->mbPerRow; x++, mb++)
    {
      id = pps->sgmQpMapping[SGM_BG];
      if (inst->preProcess.roiSegmentMap[2][mb])
        id = pps->sgmQpMapping[SGM_AROI];
      if ((x >= regs->roi1Left) && (x <= regs->roi1Right) &&
          (y >= regs->roi1Top)  && (y <= regs->roi1Bottom))
        id = pps->sgmQpMapping[SGM_ROI1];
      if ((x >= regs->roi2Left) && (x <= regs->roi2Right) &&
          (y >= regs->roi2Top)  && (y <= regs->roi2Bottom))
        id = pps->sgmQpMapping[SGM_ROI2];

      pps->sgm.idCnt[id]++;

      mask |= id << (28 - 4 * (mb % 8));
      if ((mb % 8) == 7)
      {
        *map++ = mask;
        mask = 0;
      }
    }
  }
  *map++ = mask;
}

/*------------------------------------------------------------------------------
    SetSegmentation
------------------------------------------------------------------------------*/
void SetSegmentation(vp9Instance_s *inst)
{
  regValues_s *regs = &inst->asic.regs;
  u32 *map = inst->asic.segmentMap.virtualAddress;
  ppss *ppss = &inst->ppss;
  pps *pps = inst->ppss.pps;  /* Active picture parameters */
  i32 qp = inst->rateControl.qpHdr;
  i32 qpMin = inst->rateControl.qpMin;
  i32 qpMax = inst->rateControl.qpMax;
  u32 mapSize = (inst->mbPerFrame + 15) / 16 * 8; /* Bytes, 64-bit multiple */
  u32 mb, count = 0;
  i32 mask, x, id, reorder = 0;

  /* Set the segmentation parameters according to ROI settings.
   * This will override any earlier segmentation settings. */

  /* Intra frame resets segment map so we must encode ROIs again. */
  if (inst->picBuffer.cur_pic->i_frame &&
      (regs->roi1DeltaQp || regs->roi2DeltaQp))
    regs->roiUpdate = 1;

  /* Disable adaptive ROI segment map updates when high QP. */
  if (inst->preProcess.adaptiveRoi && (qp >= 124))
    inst->preProcess.roiUpdate = 0;

  /* ROI or adaptive ROI has changed => encode new segment map into stream. */
  if (regs->roiUpdate || inst->preProcess.roiUpdate)
  {
    pps->segmentEnabled = 1;
    pps->sgm.mapModified = 1;
    regs->roiUpdate = 0;            /* Write map only when it changes. */
    inst->preProcess.roiCoded = 1;  /* AROI is coded into stream */

    /* Sort ROI IDs based on MB counts and create segment map. */
    SetSegmentMap(inst, 0);

    /* Check if some ROI is masked by others and possibly reorder IDs. */
    for (id = 0; id < 4; id++)
    {
      if ((count < inst->mbPerFrame) && !pps->sgm.idCnt[id]) reorder = 1;
      else count += pps->sgm.idCnt[id];
    }
    if (reorder) SetSegmentMap(inst, 1);

    EncSwap32((u32 *)inst->asic.segmentMap.virtualAddress, mapSize);
  }
  else if (pps->segmentEnabled && pps->sgm.mapModified)
  {
    EWLmemset(pps->sgm.idCnt, 0, sizeof(pps->sgm.idCnt));

    /* For testIds, use the map to calculate ID counts */
    for (mb = 0, mask = 0; mb < mapSize / 4; mb++)
    {
      mask = map[mb]; /* Each 32 bits in map stores ID of 8 MBs */
      for (x = 0; x < 8; x++)
      {
        if (mb * 8 + x < inst->mbPerFrame)
        {
          id = (mask >> (28 - 4 * x)) & 0xF;
          pps->sgm.idCnt[id]++;
        }
      }
    }
    EncSwap32((u32 *)inst->asic.segmentMap.virtualAddress, mapSize);
  }

  /* Set ROI QPs for correct segments. */
  if (regs->roi1DeltaQp)
    pps->qpSgm[pps->sgmQpMapping[SGM_ROI1]] = CLIP3(qp - regs->roi1DeltaQp, qpMin, qpMax);

  if (regs->roi2DeltaQp)
    pps->qpSgm[pps->sgmQpMapping[SGM_ROI2]] = CLIP3(qp - regs->roi2DeltaQp, qpMin, qpMax);

  if (!inst->picBuffer.cur_pic->i_frame && inst->preProcess.adaptiveRoi)
  {
    i32 deltaQ = inst->preProcess.adaptiveRoi;

    /* When qp close to max limit deltaQ. Note that deltaQ is negative. */
    deltaQ = MAX(deltaQ, 2 * (qp - 127));
    /* When qp close to min limit deltaQ. */
    deltaQ = MAX(deltaQ, MIN(0, 10 - qp));

    pps->qpSgm[pps->sgmQpMapping[SGM_AROI]] = CLIP3(qp + deltaQ, qpMin, qpMax);
  }

  /* Final check to disable segmentation when only one segment is used or
     encoding intra frame without new segment map. */
  if ((pps->sgm.idCnt[0] == (i32)inst->mbPerFrame) ||
      (inst->picBuffer.cur_pic->i_frame && !pps->sgm.mapModified))
    pps->segmentEnabled = 0;

  /* If current frame is key frame or segmentation is not enabled old
   * segmentation data is not valid anymore, set out of range data to
   * inform Segmentation(). */
  if (inst->picBuffer.cur_pic->i_frame || !pps->segmentEnabled)
  {
    EWLmemset(ppss->qpSgm, 0xff, sizeof(ppss->qpSgm));
    EWLmemset(ppss->levelSgm, 0xff, sizeof(ppss->levelSgm));
    ppss->prevPps = NULL;
  }
  else
    ppss->prevPps = ppss->pps;

#ifdef TRACE_SEGMENTS
  EncTraceSegments((u32 *)inst->asic.segmentMap.virtualAddress, mapSize,
                   pps->segmentEnabled, pps->sgm.mapModified, pps->sgm.idCnt, pps->qpSgm);
#endif
}

/*------------------------------------------------------------------------------
    SetFilterParameters
------------------------------------------------------------------------------*/
void SetFilterParameters(vp9Instance_s *inst)
{
  sps *sps = &inst->sps;
  pps *pps = inst->ppss.pps;  /* Active picture parameters */
  u32 qp = inst->rateControl.qpHdr;
  i32 tmp;
  u32 i;
  u32 iframe = inst->picBuffer.cur_pic->i_frame;
  const i32 const interLevel[128] =
  {
    8,  8,  8,  9,  9,  9,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 11, 11, 11, 11, 11,
    11, 11, 12, 12, 12, 12, 12, 12, 13, 13,
    13, 13, 13, 14, 14, 14, 14, 15, 15, 15,
    15, 16, 16, 16, 16, 17, 17, 17, 18, 18,
    18, 19, 19, 20, 20, 20, 21, 21, 22, 22,
    23, 23, 24, 24, 25, 25, 26, 26, 27, 28,
    28, 29, 30, 30, 31, 32, 33, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
    46, 48, 49, 50, 51, 53, 54, 56, 57, 59,
    60, 62, 63, 63, 63, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63
  };


  /* auto level */
  if (sps->autoFilterLevel)
  {
    if (iframe)
    {
      tmp = (qp * 64) / 128 + 8;
      sps->filterLevel = CLIP3(tmp, 0, 63);
      pps->levelSgm[0] = CLIP3((pps->qpSgm[0] * 64) / 128 + 8, 0, 63);
      pps->levelSgm[1] = CLIP3((pps->qpSgm[1] * 64) / 128 + 8, 0, 63);
      pps->levelSgm[2] = CLIP3((pps->qpSgm[2] * 64) / 128 + 8, 0, 63);
      pps->levelSgm[3] = CLIP3((pps->qpSgm[3] * 64) / 128 + 8, 0, 63);
    }
    else
    {
      sps->filterLevel = interLevel[qp];
      pps->levelSgm[0] = interLevel[pps->qpSgm[0]];
      pps->levelSgm[1] = interLevel[pps->qpSgm[1]];
      pps->levelSgm[2] = interLevel[pps->qpSgm[2]];
      pps->levelSgm[3] = interLevel[pps->qpSgm[3]];
    }
  }
  /* auto sharpness */
  if (sps->autoFilterSharpness)
  {
    sps->filterSharpness = 0;
  }

  if (!sps->filterDeltaEnable) return;

  if (sps->filterDeltaEnable == 2)
  {
    /* Special meaning, test ID set filter delta values */
    sps->filterDeltaEnable = true;
    return;
  }

  /* force deltas to 0 if filterLevel == 0 (assumed to mean that filtering
   * is completely disabled) */
  if (sps->filterLevel == 0)
  {
    sps->refDelta[0] =  0;      /* Intra frame */
    sps->refDelta[1] =  0;      /* Last frame */
    sps->refDelta[2] =  0;      /* Golden frame */
    sps->refDelta[3] =  0;      /* Altref frame */
    sps->modeDelta[0] = 0;      /* BPRED */
    sps->modeDelta[1] = 0;      /* Zero */
    sps->modeDelta[2] = 0;      /* New mv */
    sps->modeDelta[3] = 0;      /* Split mv */
    return;
  }

  if (!inst->picBuffer.cur_pic->ipf && !inst->picBuffer.cur_pic->grf &&
      !inst->picBuffer.cur_pic->arf)
  {
    /* Frame is droppable, ie. doesn't update ipf, grf nor arf so don't
     * update the filter level deltas. */
    EWLmemcpy(sps->refDelta, sps->oldRefDelta, sizeof(sps->refDelta));
    EWLmemcpy(sps->modeDelta, sps->oldModeDelta, sizeof(sps->modeDelta));
    return;
  }

  /* Adjustment based on reference frame */
#if 0
  sps->refDelta[0] =  4;      /* Intra frame */
  sps->refDelta[1] =  0;      /* Last frame */
  sps->refDelta[2] = -2;      /* Golden frame */
  sps->refDelta[3] = -2;      /* Altref frame */
#endif
  sps->refDelta[0] =  4;      /* Intra frame */
  sps->refDelta[1] = interLevel[inst->rateControl.qpHdrPrev2] - sps->filterLevel;
  sps->refDelta[2] = interLevel[inst->rateControl.qpHdrGolden] - sps->filterLevel;
  sps->refDelta[3] = -2;      /* Altref frame */

  /* Adjustment based on mb mode */
  sps->modeDelta[0] =  5;     /* BPRED */
  sps->modeDelta[1] = -2;     /* Zero */
  sps->modeDelta[2] =  2;     /* New mv */
  sps->modeDelta[3] =  6;     /* Split mv */

  /* ABS(delta) is 6bits, see FilterLevelDelta() */
  for (i = 0; i < 4; i++)
  {
    sps->refDelta[i] = CLIP3(sps->refDelta[i], -0x3f, 0x3f);
    sps->modeDelta[i] = CLIP3(sps->modeDelta[i], -0x3f, 0x3f);
  }

}

/*------------------------------------------------------------------------------
    UpdateAsicStream
------------------------------------------------------------------------------*/
void UpdateAsicStream(vp9Instance_s *inst)
{
  /* Update the stream pointers with the stream created by the ASIC. */

  /* Stream header remainder ie. last not full 64-bit address
   * of stream headers is also counted in HW data so we
   * have to take care that it is not counted twice. */
  const u32 hw_offset = 0;//inst->asic.regs.firstFreeBit/8;
  u32 *partitionSizes = (u32 *)inst->asic.sizeTbl.virtualAddress;
  i32 i;

#ifdef TRACE_HWOUTPUT_PIC
  /* Dump out the memories written by HW for debugging */
  if (inst->frameCnt == TRACE_HWOUTPUT_PIC)
  {
    EncDumpMem((u32 *)inst->asic.sizeTbl.virtualAddress,
               inst->asic.sizeTblSize, "strm_size");
    EncDumpMem((u32 *)(inst->buffer[1].data - hw_offset),
               inst->buffer[1].size, "strm_ctrl");
    EncDumpMem((u32 *)inst->buffer[2].pData,
               inst->buffer[2].size, "strm_resi_1");
    EncDumpMem((u32 *)inst->buffer[3].pData,
               inst->buffer[3].size, "strm_resi_2");
    EncDumpMem((u32 *)inst->buffer[4].pData,
               inst->buffer[4].size, "strm_resi_3");
    EncDumpMem((u32 *)inst->buffer[5].pData,
               inst->buffer[5].size, "strm_resi_4");
  }
#endif

  /* Control partition */
  inst->buffer[1].byteCnt += partitionSizes[0] - hw_offset;
  inst->buffer[1].data    += partitionSizes[0] - hw_offset;

  /* DCT partitions, completely written by HW */
  for (i = 2; i < inst->sps.partitionCnt; i++)
  {
    inst->buffer[i].byteCnt  = partitionSizes[i - 1];
    inst->buffer[i].data    += partitionSizes[i - 1];
  }

}
/*------------------------------------------------------------------------------
  EncSwap32
------------------------------------------------------------------------------*/
void EncSwap32(u32 *buf, u32 sizeBytes)
{
  u32 i = 0;
  u32 words = sizeBytes / 4;

  ASSERT((sizeBytes % 8) == 0);

  while (i < words)
  {
#if(ENCH1_OUTPUT_SWAP_32 == 1)    /* need this for 64-bit HW */
    u32 val  = buf[i];
    u32 val2 = buf[i + 1];

    buf[i]   = val2;
    buf[i + 1] = val;
#endif

    i += 2;
  }

}

void LamdaQp(int qp, unsigned int *lamda_sse, unsigned int *lamda_sad)
{
  double recalQP = (double)qp;
  double lambda;

  // pre-compute lambda and QP values for all possible QP candidates
  {
    // compute lambda value
    //Int    NumberBFrames = 0;
    Int    SHIFT_QP = 12;
    //double dLambda_scale = 1.0 - MAX( 0.0, MIN( 0.5, 0.05*(double)NumberBFrames ));
    Int    g_bitDepth = 8;
    Int    bitdepth_luma_qp_scale = 6 * (g_bitDepth - 8);
    double qp_temp = (double) recalQP + bitdepth_luma_qp_scale - SHIFT_QP;

    // Case #1: I or P-slices (key-frame)
    double dQPFactor = 0.8;
    lambda = dQPFactor * pow(2.0, qp_temp / 3.0);
  }

  // store lambda
  double m_dLambda           = lambda;
  double m_sqrtLambda        = sqrt(m_dLambda);
  UInt m_uiLambdaMotionSAD = ((UInt)floor(65536.0 * m_sqrtLambda)) >> 16;

  *lamda_sse = (unsigned int)lambda;
  *lamda_sad = m_uiLambdaMotionSAD;
}

