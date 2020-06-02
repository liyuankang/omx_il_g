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
--------------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "hevcencapi.h"
#include "sw_cu_tree.h"
#include "instance.h"
#include "sw_slice.h"

#ifdef TEST_DATA
#include "enctrace.h"
#endif

void setFrameTypeChar (struct Lowres *frame);
VCEncRet TerminateCuTreeThread(struct cuTreeCtr *m_param);
VCEncRet StartCuTreeThread(struct cuTreeCtr *m_param);

u32 iGlobal_aq_mode = 0;
double fGlobal_aq_strength = 1.0;

const uint8_t x265_exp2_lut[64] =
{
    0,  3,  6,  8,  11, 14,  17,  20,  23,  26,  29,  32,  36,  39,  42,  45,
    48,  52,  55,  58,  62,  65,  69,  72,  76,  80,  83,  87,  91,  94,  98,  102,
    106,  110,  114,  118,  122,  126,  130,  135,  139,  143,  147,  152,  156,  161,  165,  170,
    175,  179,  184,  189,  194,  198,  203,  208,  214,  219,  224,  229,  234,  240,  245,  250
};

/* param x should be Q24.8 */
static int exp2fix8(int32_t x)
{
    int i = (-x+12) / 24 + 512;

    if (i < 0) return 0;
    if (i > 1023) return 0xffff;
    return (x265_exp2_lut[i & 63] + 256) << (i >> 6) >> 8;
}

/* Fix point log2 with k bit decimal */
static int log2_fixpoint(uint64_t x, int k)
{
  int r = 0, i;
  for(i = x; i; i >>= 1)
    ++r;
  --r;
  /* normalized to Q1.31 */
  if(r > 31)
    x >>= (r-31);
  else
    x <<= (31-r);
  /* compute decimal bits */
  while(k--) {
    x *= x;
    x >>= 31;
    if(x >> 32) {
      x >>= 1;
      r = ((r<<1)|1);
    } else
      r = (r<<1);
  }
  return r;
}

void statisAheadData (struct cuTreeCtr *m_param, struct Lowres **frames, int numframes, bool bFirst)
{
  i32 i;
  i32 idx = !bFirst;
  i32 gopSize = (numframes > 0 ? frames[1]->gopSize : 0);
  u64 costGop[4] = {0}, costAvg[4] = {0};
   
  /* store the costs */
  for (i = 0; i < 4; i ++)
  {
    m_param->costAvgInt[i] = m_param->costGopInt[i] = 0;
    m_param->FrameTypeNum[i] = m_param->FrameNumGop[i] = 0;
  }
  for (i = idx; i <= numframes; i ++)
  {
    i32 id = frames[i]->predId;
    m_param->FrameTypeNum[id] ++;
    costAvg[id] += frames[i]->cost;
  }
  for (i = 1; i <= MIN(numframes, gopSize); i ++)
  {
    i32 id = frames[i]->predId;
    m_param->FrameNumGop[id] ++;
    costGop[id] += frames[i]->cost;
  }
  for (i = 0; i < 4; i ++)
  {
    if (m_param->FrameTypeNum[i])
      m_param->costAvgInt[i] = (costAvg[i] + m_param->FrameTypeNum[i]/2) / m_param->FrameTypeNum[i];
      
    if (m_param->FrameNumGop[i])
      m_param->costGopInt[i] = (costGop[i] + m_param->FrameNumGop[i]/2) / m_param->FrameNumGop[i];
  }
}

static void dumpInputQpDelta(struct cuTreeCtr *m_param, struct Lowres *frame)
{
  i32 i, j;
  FILE *fp = fopen("roiMapPass1.txt", frame->frameNum ? "a+" : "w");
  i32 n = m_param->inQpDeltaBlkSize / m_param->unitSize;

  if (!n)
  {
    printf ("  block size error!\n");
    return;
  }

  for (j = 0; j < m_param->heightInUnit; j += n)
  {
    for (i = 0; i < m_param->widthInUnit; i += n)
    {
      i8 v = (i8)(frame->qpAqOffset[j*m_param->widthInUnit + i] >> 8);

      /* check if same qpOffset in roi block */
      i32 in, jn;
      i32 jend = MIN(m_param->heightInUnit, j+n);
      i32 iend = MIN(m_param->widthInUnit, i+n);
      for (jn = j; jn < jend; jn ++)
      {
        for (in = i; in < iend; in ++)
        {
          if (v != (i8)(frame->qpAqOffset[jn*m_param->widthInUnit + in] >> 8))
            v = -128; //err marker
        }
      }
      
      fprintf(fp, "%d", v);
      if (i < (m_param->widthInUnit-n))
        fprintf(fp, " ");
    }
    fprintf(fp, "\n");
  }

  fclose(fp);
}

/* input qpOffset: Q8.0
 * output qpAqOffset: Q24.8
 */
void loadInputQpDelta(struct cuTreeCtr* m_param, struct Lowres *frame, i8 *qpOffset)
{
  if (!qpOffset)
    return;

  i32 blkSize = m_param->inQpDeltaBlkSize;
  /* Assume unitSize <= roiBlockSize here */
  if (m_param->unitSize > blkSize)
  {
    printf ("Pass1 Encoding Error: unit size bigger than roi block size\n");
    return;
  }

  u32 dsRatio = m_param->dsRatio;
  blkSize /= dsRatio;

  i32 unitInBlk = ((blkSize >= m_param->unitSize)?(blkSize / m_param->unitSize) : (m_param->unitSize / blkSize));
  i32 i, j;
  i32 stride = (m_param->width+blkSize-1)/blkSize;

  for (j = 0; j < m_param->heightInUnit; j ++)
  {
    for (i = 0; i < m_param->widthInUnit; i ++)
    {
      if (blkSize >= m_param->unitSize)
        frame->qpAqOffset[j*m_param->widthInUnit + i] = (qpOffset[(j/unitInBlk)*stride + (i/unitInBlk)] << 8);
      else
        frame->qpAqOffset[j*m_param->widthInUnit + i] = ((qpOffset[(j*unitInBlk)*stride + (i*unitInBlk)] + \
                                                         qpOffset[(j*unitInBlk)*stride + (i*unitInBlk)+1] + \
                                                         qpOffset[(j*unitInBlk+1)*stride + (i*unitInBlk)] + \
                                                         qpOffset[(j*unitInBlk+1)*stride + (i*unitInBlk)+1])<<6);
    }
  }
}

#define CUTREE_COST_SHIFT 1
void cuTreeFinish(struct cuTreeCtr* m_param, struct Lowres *frame, int averageDuration, int ref0Distance, int p0, int p1, int b)
{
    //from ctr parameter
    int widthInUnit = m_param->widthInUnit;
    int heightInUnit = m_param->heightInUnit;
    int unitCount = m_param->unitCount;
    int m_cuTreeStrength = m_param->m_cuTreeStrength;
    
    int fpsFactor = (int)(CLIP_DURATION_FIX8(averageDuration) * 256 / CLIP_DURATION_FIX8((m_param->fpsDenom<<8) / m_param->fpsNum)); // Q24.8
    int weightdelta = 0;  // Q24.8

    if (ref0Distance && frame->weightedCostDelta[ref0Distance - 1] > 0)
        weightdelta = (256 - frame->weightedCostDelta[ref0Distance - 1]);

    if (m_param->qgSize == 8)
    {
        for (int cuY = 0; cuY < heightInUnit; cuY++)
        {
            for (int cuX = 0; cuX < widthInUnit; cuX++)
            {
                const int cuXY = cuX + cuY * widthInUnit;
                int32_t intracost = ((u64)(frame->intraCost[cuXY]) / 4 * (u64)frame->invQscaleFactor8x8[cuXY] + 128) >> 8;
                if (intracost)
                {
                    int32_t propagateCost = ((u64)(frame->propagateCost[cuXY]) / 4 * (u64)fpsFactor + 128) >> 8;
                    int log2_ratio = X265_LOG2I(intracost + propagateCost) - X265_LOG2I(intracost) + weightdelta;
                    frame->qpCuTreeOffset[cuX * 2 + cuY * widthInUnit * 4] = frame->qpAqOffset[cuX * 2 + cuY * widthInUnit * 4] - ((m_cuTreeStrength * (u64)(log2_ratio)) >> 8);
                    frame->qpCuTreeOffset[cuX * 2 + cuY * widthInUnit * 4 + 1] = frame->qpAqOffset[cuX * 2 + cuY * widthInUnit * 4 + 1] - ((m_cuTreeStrength * (u64)(log2_ratio)) >> 8);
                    frame->qpCuTreeOffset[cuX * 2 + cuY * widthInUnit * 4 + frame->maxBlocksInRowFullRes] = frame->qpAqOffset[cuX * 2 + cuY * widthInUnit * 4 + frame->maxBlocksInRowFullRes] - ((m_cuTreeStrength * (u64)(log2_ratio)) >> 8);
                    frame->qpCuTreeOffset[cuX * 2 + cuY * widthInUnit * 4 + frame->maxBlocksInRowFullRes + 1] = frame->qpAqOffset[cuX * 2 + cuY * widthInUnit * 4 + frame->maxBlocksInRowFullRes + 1] - ((m_cuTreeStrength * (u64)(log2_ratio)) >> 8);
                }
            }
        }        
    }
    else
    {
        for (int cuIndex = 0; cuIndex < unitCount; cuIndex++)
        {
            int32_t intracost = ((u64)frame->intraCost[cuIndex] * (u64)frame->invQscaleFactor[cuIndex] + 128) >> 8;
            if (intracost)
            {
                int32_t propagateCost = ((u64)frame->propagateCost[cuIndex] * (u64)fpsFactor + 128) >> 8;
                int log2_ratio = X265_LOG2I(intracost + propagateCost) - X265_LOG2I(intracost) + weightdelta;
                frame->qpCuTreeOffset[cuIndex] = frame->qpAqOffset[cuIndex] - ((m_cuTreeStrength * (u64)log2_ratio) >> 8);
            }
        }

        {
            u64 newCostAvg = 0;
            int cuIndex;
            for (cuIndex = 0; cuIndex < unitCount; cuIndex++)
            {
               u32 cost = frame->intraCost[cuIndex];
               if (!IS_X265_TYPE_I(frame->sliceType))
                  cost = MIN(cost, frame->lowresCosts[b - p0][p1 - b][cuIndex] & LOWRES_COST_MASK);

               u64 newCost = cost * (u64)exp2fix8(frame->qpCuTreeOffset[cuIndex]); // Q24.8
               newCostAvg += newCost;
            }
            newCostAvg = (newCostAvg + unitCount/2) / unitCount; // Q24.8
            frame->cost = (newCostAvg << CUTREE_COST_SHIFT);

            //union frm cost on 8x8 block unit
            if(m_param->unitSize == 16)
              frame->cost  /=4;

            return;
        }
    }
}

/* Estimate the total amount of influence on future quality that could be had if we
 * were to improve the reference samples used to inter predict any given CU. */
static void estimateCUPropagateCost(int32_t* dst, const uint32_t* propagateIn, const int32_t* intraCosts, const uint32_t* interCosts,
                                    const int32_t* invQscales, const int* fpsFactor, int len)
{
    int fps = (*fpsFactor+128) / 256;  // range[0.01, 1.00], Q24.8
    for (int i = 0; i < len; i++)
    {
        int intraCost = intraCosts[i];
        int interCost = MIN(intraCosts[i], (int32_t)interCosts[i] & LOWRES_COST_MASK);
        uint64_t propagateIntra = (u64)intraCost * (u64)invQscales[i]; // Q16 x Q8.8 = Q24.8
        uint32_t propagateAmount = propagateIn[i] + ((propagateIntra * (u64)fps + 128) >> 8); // Q16.0 + Q24.8 x Q0.x = Q25.0
        int32_t propagateNum = (intraCost - interCost); // Q32 - Q32 = Q33.0
#if 0
        // algorithm that output match to asm
        float intraRcp = (float)1.0f / intraCost;   // VC can't mapping this into RCPPS
        float intraRcpError1 = (float)intraCost * (float)intraRcp;
        intraRcpError1 *= (float)intraRcp;
        float intraRcpError2 = intraRcp + intraRcp;
        float propagateDenom = intraRcpError2 - intraRcpError1;
        dst[i] = (int)(propagateAmount * propagateNum * (double)propagateDenom + 0.5);
#else
        int32_t propagateDenom = intraCost;             // Q32
        dst[i] = (uint32_t)(((u64)propagateAmount * (u64)propagateNum * 2 + propagateDenom) / (2*propagateDenom));
#endif
        }
}

void estimateCUPropagate(struct cuTreeCtr* m_param, struct Lowres **frames, int averageDuration, int p0, int p1, int b, int referenced)
{
    //from ctr parameter
    int widthInUnit = m_param->widthInUnit;
    int heightInUnit = m_param->heightInUnit;
    int32_t *m_scratch= m_param->m_scratch;

    if (IS_X265_TYPE_I(frames[b]->sliceType))
      p0 = p1 = b;

    //err check
    if ((frames[b]->p0 != (b-p0)) || (frames[b]->p1 != (p1-b)))
      return;

    if (referenced && (frames[b]->sliceType == X265_TYPE_B))
    {
      frames[b]->sliceType = X265_TYPE_BREF;
      setFrameTypeChar (frames[b]);
      frames[b]->predId = getFramePredId(frames[b]->sliceType);
    }

    if (frames[b]->hieDepth > m_param->maxHieDepth)
      return;
    
    uint32_t *refCosts[2] = { frames[p0]->propagateCost, frames[p1]->propagateCost };
    //int32_t distScaleFactor = (((b - p0) << 8) + ((p1 - p0) >> 1)) / (p1 - p0);
    int32_t bipredWeight = 32;//m_param->bEnableWeightedBiPred ? 64 - (distScaleFactor >> 2) : 32;
    int32_t bipredWeights[2] = { bipredWeight, 64 - bipredWeight };
    int listDist[2] = { b - p0, p1 - b };

    memset(m_scratch, 0, widthInUnit * sizeof(int32_t));

    uint32_t *propagateCost = frames[b]->propagateCost;

    int fpsFactor = CLIP_DURATION_FIX8(m_param->fpsDenom*256 / m_param->fpsNum)*256 / CLIP_DURATION_FIX8(averageDuration); // Q24.8

    /* For non-referred frames the source costs are always zero, so just memset one row and re-use it. */
    if (!referenced)
        memset(frames[b]->propagateCost, 0, widthInUnit * sizeof(uint32_t));

    int32_t strideInCU = widthInUnit;
    
    for (uint16_t blocky = 0; blocky < heightInUnit; blocky++)
    {
        int cuIndex = blocky * strideInCU;
        if (m_param->qgSize == 8)
            estimateCUPropagateCost(m_scratch, propagateCost,
                       frames[b]->intraCost + cuIndex, frames[b]->lowresCosts[b - p0][p1 - b] + cuIndex,
                       frames[b]->invQscaleFactor8x8 + cuIndex, &fpsFactor, widthInUnit);
        else
            estimateCUPropagateCost(m_scratch, propagateCost,
                       frames[b]->intraCost + cuIndex, frames[b]->lowresCosts[b - p0][p1 - b] + cuIndex,
                       frames[b]->invQscaleFactor + cuIndex, &fpsFactor, widthInUnit);

        if (referenced)
            propagateCost += widthInUnit;

        for (uint16_t blockx = 0; blockx < widthInUnit; blockx++, cuIndex++)
        {
            int32_t propagate_amount = m_scratch[blockx];
            /* Don't propagate for an intra block. */
            if (propagate_amount > 0)
            {
                /* Access width-2 bitfield. */
                int32_t lists_used = frames[b]->lowresCosts[b - p0][p1 - b][cuIndex] >> LOWRES_COST_SHIFT;
                /* Follow the MVs to the previous frame(s). */
                for (uint16_t list = 0; list < 2; list++)
                {
                    if ((lists_used >> list) & 1)
                    {
#define CLIP_ADD(s, x) (s) = X265_MIN((s) + (x), 0xffffffffU)
                        int32_t listamount = propagate_amount;
                        /* Apply bipred weighting. */
                        if (lists_used == 3)
                            listamount = ((u64)listamount * (u64)bipredWeights[list] + 32) >> 6;

                        struct MV *mvs = frames[b]->lowresMvs[list][listDist[list]];

                        /* Early termination for simple case of mv0. */
                        if (!mvs[cuIndex].x && !mvs[cuIndex].y)
                        {
                            CLIP_ADD(refCosts[list][cuIndex], listamount);
                            continue;
                        }
                        
                        int32_t log2_mv_one = (m_param->unitSize == 8 ? 5 : 6);
                        int32_t x = mvs[cuIndex].x;
                        int32_t y = mvs[cuIndex].y;
                        int32_t cux = (x >> log2_mv_one) + blockx;
                        int32_t cuy = (y >> log2_mv_one) + blocky;
                        int32_t idx0 = cux + cuy * strideInCU;
                        int32_t idx1 = idx0 + 1;
                        int32_t idx2 = idx0 + strideInCU;
                        int32_t idx3 = idx0 + strideInCU + 1;
                        int32_t mv_one = (m_param->unitSize<<2);
                        x &= mv_one-1;
                        y &= mv_one-1;
                        int32_t idx0weight = (mv_one - y) * (mv_one - x);
                        int32_t idx1weight = (mv_one - y) * x;
                        int32_t idx2weight = y * (mv_one - x);
                        int32_t idx3weight = y * x;

                        /* We could just clip the MVs, but pixels that lie outside the frame probably shouldn't
                         * be counted. */
                        if (cux < widthInUnit - 1 && cuy < heightInUnit - 1 && cux >= 0 && cuy >= 0)
                        {
                            CLIP_ADD(refCosts[list][idx0], ((u64)listamount * (u64)idx0weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            CLIP_ADD(refCosts[list][idx1], ((u64)listamount * (u64)idx1weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            CLIP_ADD(refCosts[list][idx2], ((u64)listamount * (u64)idx2weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            CLIP_ADD(refCosts[list][idx3], ((u64)listamount * (u64)idx3weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                        }
                        else /* Check offsets individually */
                        {
                            if (cux < widthInUnit && cuy < heightInUnit && cux >= 0 && cuy >= 0)
                                CLIP_ADD(refCosts[list][idx0], ((u64)listamount * (u64)idx0weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            if (cux + 1 < widthInUnit && cuy < heightInUnit && cux + 1 >= 0 && cuy >= 0)
                                CLIP_ADD(refCosts[list][idx1], ((u64)listamount * (u64)idx1weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            if (cux < widthInUnit && cuy + 1 < heightInUnit && cux >= 0 && cuy + 1 >= 0)
                                CLIP_ADD(refCosts[list][idx2], ((u64)listamount * (u64)idx2weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                            if (cux + 1 < widthInUnit && cuy + 1 < heightInUnit && cux + 1 >= 0 && cuy + 1 >= 0)
                                CLIP_ADD(refCosts[list][idx3], ((u64)listamount * (u64)idx3weight + (1<<(2*log2_mv_one-1))) >> (2*log2_mv_one));
                        }
                    }
                }
            }
        }
    }

    if (m_param->vbvBufferSize && m_param->lookaheadDepth && referenced)
        cuTreeFinish(m_param, frames[b], averageDuration, b == p1 ? b - p0 : 0, p0, p1, b);
}

void hierachyEstPropagate(struct cuTreeCtr* m_param, struct Lowres **frames, int averageDuration, i32 cur, i32 last, i32 depth)
{
  i32 bframes = last - cur - 1;
  i32 i = last - 1;
  if (bframes > 1)
  {
      int middle = (bframes + 1) / 2 + cur;
      memset(frames[middle]->propagateCost, 0, m_param->unitCount * sizeof(uint32_t));
      hierachyEstPropagate(m_param, frames, averageDuration, middle, last, depth + 1);
      hierachyEstPropagate(m_param, frames, averageDuration, cur, middle, depth + 1);

      frames[middle]->hieDepth = depth;
      estimateCUPropagate(m_param, frames, averageDuration, cur, last, middle, 1);
  }
  else if (bframes == 1)
  {
      frames[i]->hieDepth = depth;
      estimateCUPropagate(m_param, frames, averageDuration, cur, last, i, 0);
  }
}

void estHierachyGop(struct cuTreeCtr* m_param, struct Lowres **frames, int averageDuration, i32 cur, i32 last)
{
  i32 depth = 0;
  m_param->maxHieDepth = ((frames[last]->gopSize == 8) && (frames[last]->aGopSize == 4)) ? 3 : DEFAULT_MAX_HIE_DEPTH;
  memset(frames[cur]->propagateCost, 0, m_param->unitCount * sizeof(uint32_t));
  hierachyEstPropagate(m_param, frames, averageDuration, cur, last, depth + 1);
  frames[last]->hieDepth = depth;
  estimateCUPropagate(m_param, frames, averageDuration, cur, last, last, 1);
}

void estPyramidGop(struct cuTreeCtr* m_param, struct Lowres **frames, int averageDuration, i32 curnonb, i32 lastnonb)
{
    int bframes = lastnonb - curnonb - 1;
    int unitCount = m_param->unitCount;
    int i = lastnonb - 1;
    memset(frames[curnonb]->propagateCost, 0, unitCount * sizeof(uint32_t));
    if (m_param->bBPyramid && bframes > 1)
    {
        int middle = (bframes + 1) / 2 + curnonb;
        memset(frames[middle]->propagateCost, 0, unitCount * sizeof(uint32_t));
        while (i > curnonb)
        {
            int p0 = i > middle ? middle : curnonb;
            int p1 = i < middle ? middle : lastnonb;
            if (i != middle)
            {
                estimateCUPropagate(m_param, frames, averageDuration, p0, p1, i, 0);
            }
            i--;
        }
  
        estimateCUPropagate(m_param, frames, averageDuration, curnonb, lastnonb, middle, 1);
    }
    else
    {
        while (i > curnonb)
        {
            estimateCUPropagate(m_param, frames, averageDuration, curnonb, lastnonb, i, 0);
            i--;
        }
    }
    estimateCUPropagate(m_param, frames, averageDuration, curnonb, lastnonb, lastnonb, 1);
}

void estGopPropagate(struct cuTreeCtr* m_param, struct Lowres **frames, int averageDuration, i32 cur, i32 last)
{
  if (m_param->bBHierachy)
    estHierachyGop(m_param, frames, averageDuration, cur, last);
  else
    estPyramidGop(m_param, frames, averageDuration, cur, last);
}

void cuTree(struct cuTreeCtr* m_param, struct Lowres **frames, int numframes, bool bFirst)
{    
    //from ctr parameter
    int unitCount = m_param->unitCount;

    int idx = !bFirst;
    int lastnonb, curnonb = 1;
    int bframes = 0;

    int totalDuration = 0;
    for (int j = 0; j <= numframes; j++)
        totalDuration += m_param->fpsDenom*256 / m_param->fpsNum;

    int averageDuration = totalDuration / (numframes + 1); // Q24.8

    int i = numframes;

    while (i > 0 && IS_X265_TYPE_B(frames[i]->sliceType))
        i--;

    lastnonb = i;

    /* Lookaheadless MB-tree is not a theoretically distinct case; the same extrapolation could
     * be applied to the end of a lookahead buffer of any size.  However, it's most needed when
     * lookahead=0, so that's what's currently implemented. */
    if (!m_param->lookaheadDepth)
    {
        if (bFirst)
        {
            memset(frames[0]->propagateCost, 0, unitCount * sizeof(uint32_t));
            if (m_param->qgSize == 8)
                memcpy(frames[0]->qpCuTreeOffset, frames[0]->qpAqOffset, unitCount * 4 * sizeof(int32_t));
            else
                memcpy(frames[0]->qpCuTreeOffset, frames[0]->qpAqOffset, unitCount * sizeof(int32_t));
            return;
        }
        memset(frames[0]->propagateCost, 0, unitCount * sizeof(uint32_t));
    }
    else
    {
        if (lastnonb < idx)
            return;
        memset(frames[lastnonb]->propagateCost, 0, unitCount * sizeof(uint32_t));
    }

    /* est one gop */
    while (i-- > idx)
    {
        curnonb = i;
        while (IS_X265_TYPE_B(frames[curnonb]->sliceType) && curnonb > 0)
            curnonb--;
  
        if(curnonb < idx) {
            bframes = lastnonb - curnonb - 1;
            break;
        }
  
        bframes = lastnonb - curnonb - 1;
        estGopPropagate(m_param, frames, averageDuration, curnonb, lastnonb);
        lastnonb = i = curnonb;
    }

    /* finish one gop */
    for (i = MAX(0, lastnonb - bframes); i <= lastnonb; i ++)
    {
      if ((frames[i]->sliceType != X265_TYPE_B) && (frames[i]->hieDepth < m_param->maxHieDepth))
        cuTreeFinish(m_param, frames[i], averageDuration, (i == lastnonb) ? lastnonb : 0, i - frames[i]->p0, i + frames[i]->p1, i);
      else
        memset (frames[i]->qpCuTreeOffset, 0, sizeof(int32_t)*m_param->unitCount);
    }

    statisAheadData (m_param, frames, numframes, bFirst);
}

/* collect input frame info from pass 1 instance (in lookahead thread) */
static void initFrameFromEncInst(struct Lowres *cur_frame, struct cuTreeCtr *m_param,
                        struct vcenc_instance *pEncInst, VCEncIn *pEncIn, VCEncOut *pEncOut)
{
  int i, j;
  asicData_s *asic = &pEncInst->asic;

  cur_frame->poc = pEncInst->poc;
  cur_frame->frameNum = m_param->frameNum++;
  cur_frame->qp = (pEncInst->rateControl.qpHdr >> QP_FRACTIONAL_BITS);

  cur_frame->cost = 0.0;
  cur_frame->gopEncOrder = pEncInst->rateControl.encoded_frame_number;
  if (pEncOut->codingType == VCENC_PREDICTED_FRAME)
  {
    cur_frame->sliceType = X265_TYPE_P;
  }
  else if (pEncOut->codingType == VCENC_INTRA_FRAME)
  {
    cur_frame->sliceType = X265_TYPE_I;
  }
  else if (pEncOut->codingType == VCENC_BIDIR_PREDICTED_FRAME)
  {
    cur_frame->sliceType = cur_frame->gopEncOrder ? X265_TYPE_B : X265_TYPE_BLDY;
  }  
  setFrameTypeChar (cur_frame);
  cur_frame->predId = getFramePredId(cur_frame->sliceType);
  cur_frame->gopSize = (cur_frame->sliceType == X265_TYPE_I ? 1 : pEncInst->rateControl.hierarchial_bit_allocation_GOP_size);
  cur_frame->gopEnd = pEncInst->rateControl.encoded_frame_number == (cur_frame->gopSize-1);
  m_param->dsRatio = pEncInst->preProcess.inLoopDSRatio + 1;
  if(pEncInst->extDSRatio)
    m_param->dsRatio = pEncInst->extDSRatio + 1;

  if(cur_frame->sliceType != X265_TYPE_IDR && cur_frame->sliceType != X265_TYPE_I)
    cur_frame->p0 = abs(asic->regs.l0_delta_poc[0]);
  if(IS_X265_TYPE_B(cur_frame->sliceType))
    cur_frame->p1 = abs(asic->regs.l1_delta_poc[0]);

  if(m_param->bHWMultiPassSupport)
  {
    cur_frame->motionScore[0][0] = pEncInst->asic.regs.motionScore[0][0];
    cur_frame->motionScore[0][1] = pEncInst->asic.regs.motionScore[0][1];
    cur_frame->motionScore[1][0] = pEncInst->asic.regs.motionScore[1][0];
    cur_frame->motionScore[1][1] = pEncInst->asic.regs.motionScore[1][1];
    cur_frame->outRoiMapDeltaQpIdx = INVALID_INDEX;
    cur_frame->cuDataIdx = ((ptr_t)pEncOut->cuOutData.cuData-((ptr_t)pEncInst->asic.cuInfoMem[0].virtualAddress + pEncInst->asic.cuInfoTableSize))/m_param->cuData_frame_size;
    cur_frame->inRoiMapDeltaBinIdx = INVALID_INDEX;
    m_param->roiMapEnable = pEncInst->roiMapEnable;
    if (m_param->roiMapEnable && pEncIn->pRoiMapDelta) {
      if(m_param->inRoiMapDeltaBin_Base == 0) {
        m_param->inRoiMapDeltaBin_Base = pEncIn->roiMapDeltaQpAddr;
        m_param->inRoiMapDeltaBin_frame_size = pEncIn->roiMapDeltaSize;
      }
      cur_frame->inRoiMapDeltaBinIdx = ((ptr_t)pEncIn->roiMapDeltaQpAddr - m_param->inRoiMapDeltaBin_Base)/m_param->inRoiMapDeltaBin_frame_size;
    }
  }
}
/* collect input cu info and allocate internal resources (in cutree thread) */
static void initFrame(struct Lowres *cur_frame, struct cuTreeCtr *m_param, const VCEncIn *pEncIn)
{

  cur_frame->propagateCost = malloc(m_param->unitCount * sizeof(uint64_t));
  cur_frame->qpCuTreeOffset = calloc(1, m_param->unitCount * (m_param->qgSize == 8 ? 4 : 1) * sizeof(int32_t));
  cur_frame->qpAqOffset = calloc(1, m_param->unitCount * (m_param->qgSize == 8 ? 4 : 1) * sizeof(int32_t));
  cur_frame->intraCost = malloc(m_param->unitCount * sizeof(int32_t));
  cur_frame->invQscaleFactor = malloc(m_param->unitCount * sizeof(int32_t));

  memset(cur_frame->weightedCostDelta, 0, sizeof(int32_t) * (X265_BFRAME_MAX + 2));

  cur_frame->lowresCosts[cur_frame->p0][cur_frame->p1] = calloc(1, m_param->unitCount * sizeof(uint32_t));
  cur_frame->lowresMvs[0][cur_frame->p0] = malloc(m_param->unitCount * sizeof(struct MV));
  cur_frame->lowresMvs[1][cur_frame->p1] = malloc(m_param->unitCount * sizeof(struct MV));

  if (m_param->roiMapEnable && pEncIn->pRoiMapDelta)
    loadInputQpDelta(m_param, cur_frame, pEncIn->pRoiMapDelta);
}

static void releaseFrame(struct Lowres *cur_frame)
{
  int i, j;
  
  if(cur_frame->propagateCost) {
    free(cur_frame->propagateCost);
    cur_frame->propagateCost = NULL;
  }
  if(cur_frame->qpCuTreeOffset) {
    free(cur_frame->qpCuTreeOffset);
    cur_frame->qpCuTreeOffset = NULL;
  }
  if(cur_frame->qpAqOffset) {
    free(cur_frame->qpAqOffset);
    cur_frame->qpAqOffset = NULL;
  }
  if(cur_frame->intraCost) {
    free(cur_frame->intraCost);
    cur_frame->intraCost = NULL;
  }
  if(cur_frame->invQscaleFactor) {
    free(cur_frame->invQscaleFactor);
    cur_frame->invQscaleFactor = NULL;
  }
  for(i = 0; i <= X265_BFRAME_MAX+1; i++)
    for(j = 0; j <= X265_BFRAME_MAX+1; j++) {
      if (cur_frame->lowresCosts[i][j])
      {
        free(cur_frame->lowresCosts[i][j]);
        cur_frame->lowresCosts[i][j] = NULL;
      }
    }
  for(i = 0; i <= 1; i++)
    for(j = 0; j <= X265_BFRAME_MAX+1; j++) {
      if (cur_frame->lowresMvs[i][j])
      {
        free(cur_frame->lowresMvs[i][j]);
        cur_frame->lowresMvs[i][j] = NULL;
      }
    }
  if (cur_frame->job) {
    free(cur_frame->job);
    cur_frame->job = NULL;
  }
  if (cur_frame)
  {
    free(cur_frame);
    cur_frame = NULL;
  }
}
void remove_one_frame(struct cuTreeCtr *m_param)
{
  //remove one frame from queue
  struct Lowres *out_frame = *m_param->lookaheadFrames;
  releaseFrame(out_frame);

  *m_param->lookaheadFrames = NULL;
  ++m_param->lookaheadFrames;
  --m_param->nLookaheadFrames;

  if(m_param->lookaheadFrames - m_param->lookaheadFramesBase >= m_param->nLookaheadFrames) {
    memcpy(m_param->lookaheadFramesBase, m_param->lookaheadFrames, m_param->nLookaheadFrames*sizeof(struct Lowres *));
    m_param->lookaheadFrames = m_param->lookaheadFramesBase;
  }
}

void setFrameTypeChar (struct Lowres *frame)
{
  char type = 0;
  switch (frame->sliceType)
  {
    case X265_TYPE_I: type = 'I'; break;
    case X265_TYPE_P: type = 'P'; break;
    case X265_TYPE_B: type = 'b'; break;
    case X265_TYPE_BREF: type = 'B'; break;
    case X265_TYPE_BLDY: type = 'L'; break;
    default: break;
  }
  frame->typeChar = type;
}

i32 getFramePredId (i32 type)
{
  i32 id = 0;
  switch (type)
  {
    case X265_TYPE_I:
    case 'I':
        id = I_SLICE; break;

    case X265_TYPE_P:
    case X265_TYPE_BLDY:
    case 'P':
    case 'L':
        id = P_SLICE; break;

    case X265_TYPE_B:
    case 'b':
        id = B_SLICE; break;

    case X265_TYPE_BREF:
    case 'B':
        id = 3; break;

    default: break;
  }
  return id;
}

static void writeQpValue2Memory(char qpDelta, u8* memory,u16 column,u16 row,u16 blockunit,u16 width, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  u32 blks_per_ctb = ctb_size/8;
  u32 blks_per_unit = 1<<(3-blockunit);
  u32 ctb_row_number    = row*blks_per_unit    / blks_per_ctb;
  u32 ctb_column_number = column*blks_per_unit / blks_per_ctb;
  u32 ctb_row_stride = ctb_per_row * blks_per_ctb * blks_per_ctb;  
  u32 xoffset = (column*blks_per_unit) % blks_per_ctb;
  u32 yoffset = (row*blks_per_unit) % blks_per_ctb;
  u32 stride = blks_per_ctb;
  u32 columns, rows, r, c;

  rows = columns = blks_per_unit;
  if(blks_per_ctb < blks_per_unit)
  {
    rows = MIN(rows, ctb_per_column*blks_per_ctb - row*blks_per_unit);
    columns = MIN(columns, ctb_per_row*blks_per_ctb - column*blks_per_unit);
    rows /= blks_per_ctb;
    columns *= blks_per_ctb;
    stride = ctb_row_stride;
  }

  // ctb addr --> blk addr
  memory += ctb_row_number*ctb_row_stride + ctb_column_number*(blks_per_ctb*blks_per_ctb);
  memory += yoffset * stride + xoffset;
  for(r=0;r<rows;r++)
  {
    u8 *dst = memory + r*stride;
    for(c=0;c<columns;c++)
      *dst++ = qpDelta;
  }
}


static void writeQpDeltaData2Memory(char qpDelta,u8* memory,u16 column,u16 row,u16 blockunit,u16 width, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column)
{
  u8 twoBlockDataCombined;
  int r,c;
  u32 blks_per_ctb = ctb_size/8;
  u32 blks_per_unit = (1<<(3-blockunit));
  u32 ctb_row_number=row*blks_per_unit/blks_per_ctb;
  u32 ctb_row_stride=ctb_per_row*(blks_per_ctb*blks_per_ctb)/2;
  u32 ctb_row_offset=(blks_per_ctb < blks_per_unit ? 0 : row-(row/(blks_per_ctb/blks_per_unit))*(blks_per_ctb/blks_per_unit))*blks_per_unit;
  u32 internal_ctb_stride=blks_per_ctb/2;
  u32 ctb_column_number;
  u8* rowMemoryStartPtr=memory+ctb_row_number*ctb_row_stride;
  u8* ctbMemoryStartPtr, *curMemoryStartPtr;
  u32 columns,rows;
  u32 xoffset;

  {
    ctb_column_number=column*blks_per_unit/blks_per_ctb;
    ctbMemoryStartPtr=rowMemoryStartPtr+ctb_column_number*(blks_per_ctb*blks_per_ctb)/2;
    switch(blockunit)
    {
      case 0:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=8;
        columns=4;
        xoffset=0;
        break;
      case 1:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=4;
        columns=2;
        xoffset=column%((ctb_size+31)/32);
        xoffset=xoffset<<1;
        break;
      case 2:
        twoBlockDataCombined=(-qpDelta&0X0F)|(((-qpDelta&0X0F))<<4);
        rows=2;
        columns=1;
        xoffset=column%((ctb_size+15)/16);
        break;
      case 3:
        xoffset=column>>1;
        xoffset=xoffset%((ctb_size+15)/16);
        curMemoryStartPtr=ctbMemoryStartPtr+ctb_row_offset*internal_ctb_stride + xoffset;
        twoBlockDataCombined=*curMemoryStartPtr;
        if(column%2)
        {
          twoBlockDataCombined=(twoBlockDataCombined&0x0f)|(((-qpDelta&0X0F))<<4);
        }
        else
        {
          twoBlockDataCombined=(twoBlockDataCombined&0xf0)|(-qpDelta&0X0F);
        }
        rows=1;
        columns=1;
        break;
      default:
        rows=0;
        twoBlockDataCombined=0;
        columns=0;
        xoffset=0;
        break;
    }
    u32 stride = internal_ctb_stride;
    if(blks_per_ctb < blks_per_unit) {
      rows = MIN(rows, ctb_per_column*blks_per_ctb-row*blks_per_unit);
      columns = MIN(columns, (ctb_per_row*blks_per_ctb-column*blks_per_unit)/2);
      rows /= blks_per_ctb;
      columns *= blks_per_ctb;
      stride = ctb_row_stride;
    }
    for(r=0;r<rows;r++)
    {
      curMemoryStartPtr=ctbMemoryStartPtr+(ctb_row_offset+r)*stride + xoffset;
      for(c=0;c<columns;c++)
      {
        *curMemoryStartPtr++=twoBlockDataCombined;
      }
    }
  }
}

static void writeQpDeltaRowData2Memory(char* qpDeltaRowStartAddr,u8* memory,u16 width,u16 rowNumber,u16 blockunit, u16 ctb_size, u32 ctb_per_row, u32 ctb_per_column, i32 roiMapVersion)
{
  i32 i = 0;
  while(i<width)
  {
    if (roiMapVersion >= 1)
      writeQpValue2Memory(*qpDeltaRowStartAddr,memory,i,rowNumber,blockunit,width, ctb_size, ctb_per_row, ctb_per_column);
    else
      writeQpDeltaData2Memory(*qpDeltaRowStartAddr,memory,i,rowNumber,blockunit,width, ctb_size, ctb_per_row, ctb_per_column);

    qpDeltaRowStartAddr++;
    i++;
  }
}
/* cutree output one frame */
static void write_cutree_file(struct cuTreeCtr *m_param, struct Lowres *frame)
{
  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
  VCEncLookaheadJob *out = frame->job;
  i32 roiAbsQpSupport = ((struct vcenc_instance *)enc)->asic.regs.asicCfg.roiAbsQpSupport;
  i32 roiMapVersion = ((struct vcenc_instance *)enc)->asic.regs.asicCfg.roiMapVersion;
  char rowbuffer[1024];
  i32 line_idx = 0, row_num = 0, i, j, k;
  i32 qpdelta_num=0;
  i32 qpdelta;
  u16 block_unit = 32;
  i16 block_unit0 = m_param->unitSize*m_param->dsRatio;
  i32 enc_w = enc->width*m_param->dsRatio;
  i32 enc_h = enc->height*m_param->dsRatio;
  u16 width = (((enc_w+enc->max_cu_size-1)& (~(enc->max_cu_size - 1)))+block_unit-1)/block_unit;
  u32 width0 = (enc_w+block_unit0-1)/block_unit0;
  u32 height0 = (enc_h+block_unit0-1)/block_unit0;
  u32 ctb_per_row = (enc_w+enc->max_cu_size-1)/enc->max_cu_size;
  u32 ctb_per_column = (enc_h+enc->max_cu_size-1)/enc->max_cu_size;
  i16 avg_unit = block_unit/block_unit0;
  ptr_t busAddress = 0;
  u8 *memory = GetRoiMapBufferFromBufferPool(m_param, &busAddress);
  u8 pictype;
  char* rowbufferptr;
  i32 poc, idx, qp;
  float cost, avgCost[4], gopCost[4];
  i32 frameNum[4], gopFrameNum[4];
  char frame_type;
  short qpline[4][1024];
  i32 gopSize;

  /* output frame info */
  out->encIn.roiMapDeltaQpAddr = busAddress;
  out->frame.poc      = frame->poc;
  out->frame.frameNum = frame->frameNum;
  out->frame.typeChar = frame->typeChar;
  out->frame.qp       = frame->qp;
  out->frame.cost     = frame->cost/256.0;
  out->frame.gopSize  = frame->gopSize;
  for(i = 0; i < 4; i++) {
    out->frame.costGop[i] = m_param->costGopInt[i]/256.0;
    out->frame.FrameNumGop[i] = m_param->FrameNumGop[i];
    out->frame.costAvg[i] = m_param->costAvgInt[i]/256.0;
    out->frame.FrameTypeNum[i] = m_param->FrameTypeNum[i];
  }

  /* fill roiMapDeltaQp buffer */
  if(frame->typeChar == 'b') {
    i32 block_size=((enc_w+m_param->max_cu_size-1)& (~(m_param->max_cu_size - 1)))*((enc_h+m_param->max_cu_size-1)& (~(m_param->max_cu_size - 1)))/(8*8*2);
    if (enc->asic.regs.asicCfg.roiMapVersion >= 1)
      block_size *= 2;
    memset((void *)memory, 0, block_size);
  } else {
    line_idx = 0;
    while (line_idx < ((height0+avg_unit-1) & ~(avg_unit-1)))
    {
      if(line_idx < height0) {
        for(int i = 0; i < width0; i++)
          qpline[line_idx & (avg_unit-1)][i] = (u16)(frame->qpCuTreeOffset[m_param->widthInUnit*line_idx+i]);
        for(i = width0; i < ((width0+(avg_unit-1))&~(avg_unit-1)); i++)
          qpline[line_idx & (avg_unit-1)][i] = qpline[line_idx & (avg_unit-1)][i-1];
      } else {
        memcpy(qpline[line_idx & (avg_unit-1)], qpline[(line_idx-1) & (avg_unit-1)], ((width0+(avg_unit-1))&~(avg_unit-1))*sizeof(short));
      }
      line_idx++;
      if(line_idx & (avg_unit-1))
        continue;
      rowbufferptr=rowbuffer;
      i=0;
      while(i<width0)
      {
        // read data from file
        qpdelta = 0;
        for(j = 0; j < avg_unit; j++)
          for(k = i; k < i+avg_unit; k++) {
            qpdelta += (i32)qpline[j][k];
          }
        qpdelta /= (avg_unit*avg_unit);
        qpdelta = ((qpdelta+128) >> 8);
        qpdelta = CLIP3(-31,32, qpdelta);
        if (roiAbsQpSupport)
        {
          qpdelta = ((-qpdelta)&0x3f) << 1;
        }
        else
        {
        }

        // get qpdelta
        *(rowbufferptr++)=(char)qpdelta;
        i+=avg_unit;
        qpdelta_num++;
      }
      writeQpDeltaRowData2Memory(rowbuffer, memory, width, row_num, 
          1, enc->max_cu_size, ctb_per_row, ctb_per_column,
          roiMapVersion);
      row_num++;
    }
  }

  out->status = VCENC_FRAME_READY;
  LookaheadEnqueueOutput(&enc->lookahead, out);
  frame->job = NULL;
}

static void write_gop_cutree(struct cuTreeCtr *m_param, struct Lowres **frame, i32 size)
{
  i32 i, j;

  for (i = 0; i < size; i ++)
  {
    for (j = 0; j < size; j ++)
    {
      if (frame[j]->gopEncOrder == i)
        break;
    }
    write_cutree_file(m_param, frame[j]);
    //dumpInputQpDelta(m_param, frame[i]);
  }
}

static void bframesDecision (struct cuTreeCtr *m_param)
{   
  if (m_param->nLookaheadFrames <= 8)
    return;

  struct Lowres **frames = m_param->lookaheadFrames + m_param->nLookaheadFrames - 1 - 8;
  i32 i;
  u32 th4 = AGOP_MOTION_TH * 4;
  u32 th8 = AGOP_MOTION_TH * 8;
  i32 aGopSize = 0;

  /* convert gop4 to gop8  */
  if (IS_X265_TYPE_P(frames[4]->sliceType) && (frames[4]->gopSize == 4) && (frames[4]->aGopSize == 0) &&
      IS_X265_TYPE_P(frames[8]->sliceType) && (frames[8]->gopSize == 4) && (frames[8]->aGopSize == 0))
  {
    if ((frames[4]->motionScore[0][0] <= th4) && (frames[4]->motionScore[0][1] <= th4) &&
        (frames[8]->motionScore[0][0] <= th4) && (frames[8]->motionScore[0][1] <= th4))
    {
      aGopSize = 8;
    }
  }
  /* convert gop8 to gop4  */
  else if (IS_X265_TYPE_P(frames[8]->sliceType) && (frames[8]->gopSize == 8))
  {
    if ((frames[8]->motionScore[0][0] > th8) || (frames[8]->motionScore[0][1] > th8))
    {
      aGopSize = 4;
    }
  }

  if (aGopSize)
  {
    m_param->latestGopSize = aGopSize;  
    for (i = 1; i <= 8; i ++)
      frames[i]->aGopSize = aGopSize;
  }
}

void processGopConvert_4to8 (struct cuTreeCtr *m_param, struct Lowres **frames)
{
  int i;
  if (m_param->nLookaheadFrames <= 8)
    return;

  /* convert 2xgop4 to gop8 */
  if ((frames[4]->gopEncOrder == 0) && (frames[4]->gopSize == 4) && (frames[4]->aGopSize == 8) &&
      (frames[8]->gopEncOrder == 0) && (frames[8]->gopSize == 4) && (frames[8]->aGopSize == 8))
  { 
    for (i = 1; i <= 8; i ++)
      frames[i]->gopSize = 8;
    
    frames[4]->sliceType = X265_TYPE_BREF;
    setFrameTypeChar (frames[4]);
    frames[4]->predId = getFramePredId(frames[4]->sliceType);
  
    frames[8]->gopEncOrder = 0;
    frames[4]->gopEncOrder = 1;
    frames[2]->gopEncOrder = 2;
    frames[1]->gopEncOrder = 3;
    frames[3]->gopEncOrder = 4;
    frames[6]->gopEncOrder = 5;
    frames[5]->gopEncOrder = 6;
    frames[7]->gopEncOrder = 7;

    cuTree(m_param, frames, 8, HANTRO_TRUE);

    for (i = 1; i <= 8; i ++)
      frames[i]->aGopSize = 0;
  }
}
void processGopConvert_8to4 (struct cuTreeCtr *m_param, struct Lowres **frames)
{
  i32 i;
  if (m_param->nLookaheadFrames <= 8)
    return;
    
  /* convert gop8 to 2xgop4 */
  if ((frames[8]->gopEncOrder == 0) && (frames[8]->gopSize == 8) && (frames[8]->aGopSize == 4))
  {
    for (i = 1; i <= 8; i ++)
      frames[i]->gopSize = 4;
              
    frames[4]->sliceType = X265_TYPE_P;
    setFrameTypeChar (frames[4]);
    frames[4]->predId = getFramePredId(frames[4]->sliceType);
  
    frames[4]->gopEncOrder = 0;
    frames[2]->gopEncOrder = 1;
    frames[1]->gopEncOrder = 2;
    frames[3]->gopEncOrder = 3;
    frames[8]->gopEncOrder = 0;
    frames[6]->gopEncOrder = 1;
    frames[5]->gopEncOrder = 2;
    frames[7]->gopEncOrder = 3;
  
    statisAheadData (m_param, frames, m_param->nLookaheadFrames-1, HANTRO_FALSE);

    write_gop_cutree(m_param, m_param->lookaheadFrames + 1, 4);
    for (i = 0; i < 4; i ++)
      remove_one_frame(m_param);

    for (i = 1; i <= 8; i ++)
      frames[i]->aGopSize = 0;
    
    for (i = 0; i < m_param->nLookaheadFrames; i++)
      frames[i] = m_param->lookaheadFrames[i];
  }
}

static void process_one_frame(struct cuTreeCtr *m_param)
{
  struct Lowres *frames[X265_LOOKAHEAD_MAX + X265_BFRAME_MAX + 4];
  struct Lowres *out_frame = *m_param->lookaheadFrames;
  bool bIntra = (out_frame->sliceType == X265_TYPE_I);
  struct Lowres *middle_frame, *last_nonb;
  int i;

  if(m_param->bHWMultiPassSupport) {
    VCEncCuTreeProcessOneFrame(m_param);
    return;
  }
  for (i = 0; i < m_param->nLookaheadFrames; i++)
    frames[i] = m_param->lookaheadFrames[i];

  /* process the I frame at the head of queue */
  if(IS_X265_TYPE_I(out_frame->sliceType))
  {
    cuTree(m_param, frames, m_param->nLookaheadFrames-1, HANTRO_TRUE);
    write_gop_cutree(m_param, &out_frame, 1);
  }

  /* convert between gop4 and gop8 */
  processGopConvert_4to8 (m_param, frames);

  /* process Pbbb... */
  cuTree(m_param, frames, m_param->nLookaheadFrames-1, HANTRO_FALSE);

  /* convert between gop4 and gop8 */
  processGopConvert_8to4 (m_param, frames);

  /* write out a gop */
  last_nonb = middle_frame = NULL;
  for(i = 1; i < m_param->nLookaheadFrames; i++)
  {
    if(!IS_X265_TYPE_B(m_param->lookaheadFrames[i]->sliceType))
    {
      last_nonb = m_param->lookaheadFrames[i];
      break;
    }
  }
  if (last_nonb)
  {
    i32 gopSize = i;

    //TODO Qunying: IDR and I should be different
    if (!IS_X265_TYPE_I(last_nonb->sliceType))
      write_gop_cutree(m_param, m_param->lookaheadFrames + 1, gopSize);
    
    for (i = 0; i < gopSize; i ++)
      remove_one_frame(m_param);
  }
  return;
}

//Init cuTree
VCEncRet cuTreeInit(struct cuTreeCtr* m_param, VCEncInst inst, const VCEncConfig *config)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    m_param->pEncInst = inst;
    i32 i;

    //alg default parameter
    m_param->vbvBufferSize = 0;
    m_param->bEnableWeightedBiPred =0;
    m_param->bBPyramid = 1;
    m_param->bBHierachy = 1;
    /*
       x264: 
                veryslow, lookahead=60
       x265: 
                veryslow, lookahead=40
                placebo,  lookahead=60
       */
    m_param->lookaheadDepth = config->lookaheadDepth;
    m_param->qgSize =32;
    m_param->qCompress = 0.6;
    m_param->m_cuTreeStrength = (int)(5.0 * (1.0 - m_param->qCompress) * 256.0 + 0.5);

    //from encoder
    m_param->unitSize = 16;
    m_param->widthInUnit = (vcenc_instance->width+m_param->unitSize-1)/m_param->unitSize;
    m_param->heightInUnit =  (vcenc_instance->height+m_param->unitSize-1)/m_param->unitSize;
    m_param->unitCount = (m_param->widthInUnit) * (m_param->heightInUnit);
    m_param->fpsNum =vcenc_instance->rateControl.outRateNum;
    m_param->fpsDenom=vcenc_instance->rateControl.outRateDenom;
    m_param->width = vcenc_instance->width;
    m_param->height = vcenc_instance->height;
    m_param->max_cu_size = vcenc_instance->max_cu_size;
    m_param->roiMapEnable = vcenc_instance->roiMapEnable;
    m_param->codecFormat = vcenc_instance->codecFormat;
    iGlobal_aq_mode = m_param->aq_mode = config->aq_mode;
    fGlobal_aq_strength = m_param->aq_strength = config->aq_strength;

    //temp buffer for cutree propagate
    m_param->m_scratch = malloc(sizeof(int64_t) * m_param->widthInUnit); 

    m_param->nLookaheadFrames = 0;
    m_param->lookaheadFrames = m_param->lookaheadFramesBase;
    m_param->frameNum = 0;
    for (i32 i = 0; i < 4; i ++)
    {
      m_param->FrameTypeNum[i] = 0;
      m_param->costAvgInt[i] = 0;      
      m_param->FrameNumGop[i] = 0;
      m_param->costGopInt[i] = 0;
    }
    m_param->bUpdateGop = config->bPass1AdaptiveGop;
    m_param->latestGopSize = 0;
    m_param->maxHieDepth = DEFAULT_MAX_HIE_DEPTH;
    m_param->bHWMultiPassSupport = vcenc_instance->asic.regs.asicCfg.bMultiPassSupport;
    {
      //allocate delta qp map memory.
      // 4 bits per block.
      int block_size=((vcenc_instance->width+vcenc_instance->max_cu_size-1)& (~(vcenc_instance->max_cu_size - 1)))*((vcenc_instance->height+vcenc_instance->max_cu_size-1)& (~(vcenc_instance->max_cu_size - 1)))/(8*8*2);
      // 8 bits per block if ipcm map/absolute roi qp is supported
      if (vcenc_instance->asic.regs.asicCfg.roiMapVersion >= 1)
        block_size *= 2;
      i32 in_loop_ds_ratio = vcenc_instance->asic.regs.asicCfg.inLoopDSRatio + 1;
      if(vcenc_instance->extDSRatio)
        in_loop_ds_ratio = vcenc_instance->extDSRatio + 1;
      block_size *= in_loop_ds_ratio * in_loop_ds_ratio;
      block_size = ((block_size+63)&(~63));
      if (EWLMallocLinear(vcenc_instance->asic.ewl, block_size*CUTREE_BUFFER_NUM + ROIMAP_PREFETCH_EXT_SIZE,0,&m_param->roiMapDeltaQpMemFactory[0]) != EWL_OK)
      {
        for(i=0; i< CUTREE_BUFFER_NUM; i++)
        {
          m_param->roiMapDeltaQpMemFactory[i].virtualAddress = NULL;
        }

        cuTreeRelease(m_param);
        return VCENC_EWL_MEMORY_ERROR;
      }
      else
      {
        i32 total_size = m_param->roiMapDeltaQpMemFactory[0].size;
        memset(m_param->roiMapDeltaQpMemFactory[0].virtualAddress, 0, block_size*CUTREE_BUFFER_NUM);
        for(i=0; i< CUTREE_BUFFER_NUM; i++)
        {
          m_param->roiMapDeltaQpMemFactory[i].virtualAddress = (u32*)((ptr_t)m_param->roiMapDeltaQpMemFactory[0].virtualAddress + i*block_size);
          m_param->roiMapDeltaQpMemFactory[i].busAddress = m_param->roiMapDeltaQpMemFactory[0].busAddress + i*block_size;
          m_param->roiMapDeltaQpMemFactory[i].size = (i < CUTREE_BUFFER_NUM-1 ? block_size : total_size-(CUTREE_BUFFER_NUM-1)*block_size);
          m_param->roiMapRefCnt[i] = 0;
        }
      }
    }
    m_param->bFlush = HANTRO_FALSE;
    if(m_param->bHWMultiPassSupport)
    {
      VCEncRet ret = VCEncCuTreeInit(m_param);
      if(ret != VCENC_OK)
      {
          cuTreeRelease(m_param);
          return ret;
      }
    }
    queue_init(&m_param->jobs);
    queue_init(&m_param->agop);
    m_param->job_cnt = 0;
    m_param->output_cnt = 0;
    m_param->total_frames = 0;
    m_param->terminated = HANTRO_FALSE;
    StartCuTreeThread(m_param);

    return VCENC_OK;
}
u8 *GetRoiMapBufferFromBufferPool(struct cuTreeCtr *m_param, ptr_t *busAddr)
{
  int i;
  u8 *ret = NULL;

  pthread_mutex_lock(&m_param->roibuf_mutex);
  while(ret == NULL) {
    for(i=0; i< CUTREE_BUFFER_NUM; i++)
      if (m_param->roiMapRefCnt[i] == 0) {
        m_param->roiMapRefCnt[i]++;
        ret = (u8*)m_param->roiMapDeltaQpMemFactory[i].virtualAddress;
        *busAddr = m_param->roiMapDeltaQpMemFactory[i].busAddress;
        break;
      }
    if(ret == NULL)
      pthread_cond_wait(&m_param->roibuf_cond, &m_param->roibuf_mutex);
  }
  pthread_mutex_unlock(&m_param->roibuf_mutex);
  return ret;
}
void PutRoiMapBufferToBufferPool(struct cuTreeCtr *m_param, u8 *addr)
{
  int i;
  pthread_mutex_lock(&m_param->roibuf_mutex);
  for(i=0; i< CUTREE_BUFFER_NUM; i++)
    if ((u8*)m_param->roiMapDeltaQpMemFactory[i].busAddress== addr) {
      m_param->roiMapRefCnt[i]--;
      break;
    }
  pthread_mutex_unlock(&m_param->roibuf_mutex);
  pthread_cond_signal(&m_param->roibuf_cond);
}

//Release cuTree resource
void cuTreeRelease(struct cuTreeCtr* m_param)
{
    TerminateCuTreeThread(m_param);
    while (m_param->nLookaheadFrames)
      remove_one_frame(m_param);

    if(m_param->roiMapDeltaQpMemFactory[0].virtualAddress)
    {        
        struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)m_param->pEncInst;
        EWLFreeLinear(vcenc_instance->asic.ewl, &m_param->roiMapDeltaQpMemFactory[0]);
    }
    
    if(m_param->bHWMultiPassSupport)
      VCEncCuTreeRelease(m_param);

    if(m_param->m_scratch) 
      free(m_param->m_scratch);
    
    m_param->m_scratch = NULL;
}

void calMotionScore (struct Lowres *frame, VCEncCuInfo *cuInfo, i32 cnt, i32 end)
{
  const i32 maxScore = (AGOP_MOTION_TH * 3) >> 1;
  if (cuInfo)
  {
    if (cuInfo->cuMode == 0)
    {
      i32 dir = 1 + cuInfo->interPredIdc;
      if (dir & 1)
      {      
        frame->motionScore[0][0] += ABS(cuInfo->mv[0].mvX)*cnt;
        frame->motionScore[0][1] += ABS(cuInfo->mv[0].mvY)*cnt;
        frame->motionNum[0] += cnt;
      }
      if (dir & 2)
      {
        frame->motionScore[1][0] += ABS(cuInfo->mv[1].mvX)*cnt;
        frame->motionScore[1][1] += ABS(cuInfo->mv[1].mvY)*cnt;
        frame->motionNum[1] += cnt;
      }
    }
    else
    {
      frame->motionScore[0][0] += maxScore * frame->p0*cnt;
      frame->motionScore[0][1] += maxScore * frame->p0*cnt;
      frame->motionNum[0] += cnt;
    
      frame->motionScore[1][0] += maxScore * frame->p1*cnt;
      frame->motionScore[1][1] += maxScore * frame->p1*cnt;
      frame->motionNum[1] += cnt; 
    }
  }

  if (end)
  {
    if (frame->motionNum[0])
    {
      frame->motionScore[0][0] /= frame->motionNum[0];
      frame->motionScore[0][1] /= frame->motionNum[0];
    }
    else
      frame->motionScore[0][0] = frame->motionScore[0][1] = maxScore * frame->p0;

    if (frame->motionNum[1])
    {
      frame->motionScore[1][0] /= frame->motionNum[1];
      frame->motionScore[1][1] /= frame->motionNum[1];
    }    
    else
      frame->motionScore[1][0] = frame->motionScore[1][1] = maxScore * frame->p1;
  }
}

static void updateAgopSize(struct cuTreeCtr *m_param)
{
    pthread_mutex_lock(&m_param->agop_mutex);
    struct agop_res *res = malloc(sizeof(struct agop_res));
    res->agop_size = m_param->latestGopSize;
    queue_put(&m_param->agop, (struct node *)res);
    pthread_mutex_unlock(&m_param->agop_mutex);
    pthread_cond_signal(&m_param->agop_cond);
}

/* Collect frame info for cutree in lookahead thread */
VCEncRet cuTreeAddFrame(VCEncInst inst, VCEncLookaheadJob *job)
{
  struct vcenc_instance *pEncInst = (struct vcenc_instance *) inst;
  VCEncIn *pEncIn = &job->encIn;
  VCEncOut *pEncOut = &job->encOut;
  struct cuTreeCtr *m_param = &pEncInst->cuTreeCtl;
  struct Lowres *cur_frame = (struct Lowres *)calloc(1, sizeof(struct Lowres));
  static i32 cnt = 0;
  i32 frame_cnt;
  initFrameFromEncInst(cur_frame, m_param, pEncInst, pEncIn, pEncOut);
  cur_frame->job = job;

  pthread_mutex_lock(&m_param->cutree_mutex);
  queue_put(&m_param->jobs, (struct node *)cur_frame);
  m_param->job_cnt++;
  m_param->total_frames++;
  pthread_mutex_unlock(&m_param->cutree_mutex);
  pthread_cond_signal(&m_param->cutree_cond);
  if(m_param->bFlush || m_param->total_frames >= CUTREE_BUFFER_CNT(m_param->lookaheadDepth)) {
    return VCENC_FRAME_READY;
  }
  else
    return VCENC_FRAME_ENQUEUE;
}

/* Handle input frame in cutree thread */
void cuTreeHandleInputFrame(struct Lowres *cur_frame, struct cuTreeCtr *m_param)
{
  VCEncLookaheadJob *job = cur_frame->job;
  VCEncIn *pEncIn = &job->encIn;
  VCEncOut *pEncOut = &job->encOut;
  i32 width = m_param->width;
  i32 height = m_param->height;
  i32 ctuPerRow = (width+m_param->max_cu_size-1)/m_param->max_cu_size;
  i32 ctuPerCol = (height+m_param->max_cu_size-1)/m_param->max_cu_size;
  VCEncCuInfo cuInfo;
  u32 *ctuTable = pEncOut->cuOutData.ctuOffset;
  i32 iCtuX, iCtuY, iCu, iPu = 0, iX, iY, iBlk;
  i32 iCtu = 0;
  i32 nblks;
  int widthInUnit = m_param->widthInUnit;
  i32 p0 = 0;
  i32 p1 = 0;
  int i;
  u64  totalCost = 0;

  if (!(m_param->bUpdateGop && cur_frame->gopEnd))
  {
    updateAgopSize(m_param);
  }

  //alloc/init cur_frame instance
  if(!m_param->bHWMultiPassSupport) {
    initFrame(cur_frame, m_param, pEncIn);
    p0 = cur_frame->p0;
    p1 = cur_frame->p1;
  }
  
  //insert to lookahead queue
  i = m_param->nLookaheadFrames++;
  if(cur_frame->sliceType != X265_TYPE_IDR && cur_frame->sliceType != X265_TYPE_I)
    while(i > 0 && m_param->lookaheadFrames[i-1]->poc > cur_frame->poc) {
      m_param->lookaheadFrames[i] = m_param->lookaheadFrames[i-1];
      --i;
    }
  m_param->lookaheadFrames[i] = cur_frame;

  if(!m_param->bHWMultiPassSupport) {
  //fill into 8x8 blocks from CU infor dump 
  for (iCtuY = 0; iCtuY < ctuPerCol; iCtuY ++)
  {
    for (iCtuX = 0; iCtuX < ctuPerRow; iCtuX ++)
    {
      i32 nCu = ctuTable[iCtu];
      if (iCtu)
        nCu -= ctuTable[iCtu - 1]; 

      /*
          HEVC: HW generate ctuIdx table
                     sw reads num of CUs from the table
          H264:   HW not write anything to ctuIdx table(C-Model write 1 CU per CTU),
                      HW is not same as C-Model, ctr sw need fill nCu itself, not dependent on 
                      ctuIdx
      */
      if(IS_H264(m_param->codecFormat))
          nCu =1;
      
      for (iCu = 0; iCu < nCu; iCu ++)
      {
        VCEncGetCuInfo(m_param->pEncInst, &pEncOut->cuOutData, &cuInfo, iCtu, iCu);
        if(cuInfo.cuSize < m_param->unitSize) {
          ASSERT(cuInfo.cuSize == 8 && m_param->unitSize == 16);
          /* Group 8x8 CU to 16x16 unit */
          iX = cuInfo.cuLocationX;
          iY = cuInfo.cuLocationY;
          iBlk = ((iCtuX*m_param->max_cu_size+iX)/m_param->unitSize) + ((iCtuY*m_param->max_cu_size+iY)/m_param->unitSize)*widthInUnit;
          ASSERT(iBlk < m_param->unitCount);
          i32 intra_cost = 0, intra_cost_other = 0, inter_cost = 0, inter_cost_other = 0;
          i32 mv_fwd_x = 0, mv_fwd_y = 0, mv_bwd_x = 0, mv_bwd_y = 0, n_mv_fwd = 0, n_mv_bwd = 0, intra_num = 0;
          i32 n8x8 = 0;
          for(i = 0; i < 4 && iCu+i < nCu; i++) {
            VCEncGetCuInfo(m_param->pEncInst, &pEncOut->cuOutData, &cuInfo, iCtu, iCu+i);
            if (cuInfo.cuLocationX >= iX + 16 || cuInfo.cuLocationY >= iY + 16)
              break;
            ++n8x8;
            if(!m_param->bHWMultiPassSupport)
              calMotionScore (cur_frame, &cuInfo, cuInfo.cuSize*cuInfo.cuSize/(8*8), 0);
            if(cuInfo.cuMode) {
              ++intra_num;
              intra_cost += cuInfo.costIntraSatd; 
              intra_cost_other += cuInfo.costInterSatd; 
            } else {
              inter_cost += cuInfo.costInterSatd; 
              inter_cost_other += cuInfo.costIntraSatd; 
              if((1+cuInfo.interPredIdc) & 1) {
                mv_fwd_x += cuInfo.mv[0].mvX;
                mv_fwd_y += cuInfo.mv[0].mvY;
                ++n_mv_fwd;
              }
              if((1+cuInfo.interPredIdc) & 2) {
                mv_bwd_x += cuInfo.mv[1].mvX;
                mv_bwd_y += cuInfo.mv[1].mvY;
                ++n_mv_bwd;
              }
            }
          }
          iCu += n8x8-1;
          i32 cu_mode = (intra_num > n8x8-intra_num);
          /* scale cost & mv based on cu_mode, from cu8x8 with same cu_mode */
          if(cu_mode) {
            intra_cost = (intra_cost * 4)/intra_num;
            inter_cost = (intra_cost_other * 4)/intra_num;
            n_mv_fwd = n_mv_bwd = 0;
            mv_fwd_x = mv_fwd_y = mv_bwd_x = mv_bwd_y = 0;
          } else {
            intra_cost = (inter_cost_other * 4)/(n8x8-intra_num);
            inter_cost = (inter_cost * 4)/(n8x8-intra_num);
            if(n_mv_fwd) {
              mv_fwd_x /= n_mv_fwd;
              mv_fwd_y /= n_mv_fwd;
            }
            if(n_mv_bwd) {
              mv_bwd_x /= n_mv_bwd;
              mv_bwd_y /= n_mv_bwd;
            }
          }
          cur_frame->invQscaleFactor[iBlk] = exp2fix8(cur_frame->qpAqOffset[iBlk]);
          cur_frame->intraCost[iBlk] = ((intra_cost+((1<<CUTREE_COST_SHIFT)>>1)) >> CUTREE_COST_SHIFT);
          cur_frame->lowresCosts[p0][p1][iBlk] = ((inter_cost+((1<<CUTREE_COST_SHIFT)>>1)) >> CUTREE_COST_SHIFT);
          cur_frame->lowresCosts[p0][p1][iBlk] |= ((cu_mode==1 ? 0 : ((n_mv_fwd>0)|((n_mv_bwd>0)<<1))) << LOWRES_COST_SHIFT);
          cur_frame->lowresMvs[0][p0][iBlk].x = mv_fwd_x;
          cur_frame->lowresMvs[0][p0][iBlk].y = mv_fwd_y;
          cur_frame->lowresMvs[1][p1][iBlk].x = mv_bwd_x;
          cur_frame->lowresMvs[1][p1][iBlk].y = mv_bwd_y;

          u32 blockCost = (IS_X265_TYPE_I(cur_frame->sliceType) ? intra_cost : MIN(intra_cost, inter_cost));
          totalCost += (blockCost * cur_frame->invQscaleFactor[iBlk] + 128) >> 8;            
          continue;
        }
        nblks = cuInfo.cuSize/m_param->unitSize;
        if(!m_param->bHWMultiPassSupport)
          calMotionScore (cur_frame, &cuInfo, cuInfo.cuSize*cuInfo.cuSize/(8*8), 0);
        for(iX = cuInfo.cuLocationX; iX < cuInfo.cuLocationX+cuInfo.cuSize; iX+=m_param->unitSize)
          for(iY = cuInfo.cuLocationY; iY < cuInfo.cuLocationY+cuInfo.cuSize; iY+=m_param->unitSize) {
            iBlk = ((iCtuX*m_param->max_cu_size+iX)/m_param->unitSize) + ((iCtuY*m_param->max_cu_size+iY)/m_param->unitSize)*widthInUnit;
            ASSERT(iBlk < m_param->unitCount);
            cur_frame->invQscaleFactor[iBlk] = exp2fix8(cur_frame->qpAqOffset[iBlk]);
            cur_frame->intraCost[iBlk] = (cuInfo.costIntraSatd/(nblks*nblks) + ((1<<CUTREE_COST_SHIFT)>>1)) >> CUTREE_COST_SHIFT;
            cur_frame->lowresCosts[p0][p1][iBlk] = (cuInfo.costInterSatd/(nblks*nblks) + ((1<<CUTREE_COST_SHIFT)>>1)) >> CUTREE_COST_SHIFT;
            cur_frame->lowresCosts[p0][p1][iBlk] |= ((cuInfo.cuMode==1 ? 0 : (1+cuInfo.interPredIdc)) << LOWRES_COST_SHIFT);
            cur_frame->lowresMvs[0][p0][iBlk].x = cuInfo.mv[0].mvX;
            cur_frame->lowresMvs[0][p0][iBlk].y = cuInfo.mv[0].mvY;
            cur_frame->lowresMvs[1][p1][iBlk].x = cuInfo.mv[1].mvX;
            cur_frame->lowresMvs[1][p1][iBlk].y = cuInfo.mv[1].mvY;

            u32 blockCost = (IS_X265_TYPE_I(cur_frame->sliceType) ? cuInfo.costIntraSatd : MIN(cuInfo.costIntraSatd, cuInfo.costInterSatd));
            blockCost /= nblks*nblks;
            totalCost += (blockCost * cur_frame->invQscaleFactor[iBlk] + 128) >> 8;            
          }
      }
      iCtu ++;
    }
  }
  pthread_mutex_lock(&m_param->cuinfobuf_mutex);
  ASSERT(m_param->cuInfoToRead > 0);
  m_param->cuInfoToRead--;
  pthread_mutex_unlock(&m_param->cuinfobuf_mutex);
  pthread_cond_signal(&m_param->cuinfobuf_cond);

    /* motion score for adaptive GOP */
    if(!m_param->bHWMultiPassSupport)
      calMotionScore (cur_frame, NULL, 1, 1);
    cur_frame->cost = (((totalCost << 8) + m_param->unitCount/2)) / m_param->unitCount; // Q24.8

    //union frm cost on 8x8 block unit
    if(m_param->unitSize == 16)
      cur_frame->cost /=4;
  }
    
    /* adaptive GOP size */
    if (m_param->bUpdateGop && cur_frame->gopEnd)
    {
      bframesDecision(m_param);
      updateAgopSize(m_param);
    }

    //one frame analysis
    while(((m_param->nLookaheadFrames >= m_param->lookaheadDepth) && cur_frame->gopEnd))
      process_one_frame(m_param);
}

void cuTreeFlush(struct cuTreeCtr *m_param)
{
  m_param->bFlush = HANTRO_TRUE;
  pthread_cond_signal(&m_param->cutree_cond);
}

/* 2 pass AGOP result, sync with pass 1 encoding */
i32 getPass1UpdatedGopSize(VCEncInst inst)
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

/********************* 
 * Lookahead thread  * 
 *********************/

/* prepare pass 1 job */
bool AddPictureToLookahead(VCEncLookahead *lookahead, const VCEncIn *pEncIn, VCEncOut *pEncOut)
{
  static i32 cnt = 0;
  if(lookahead->status != VCENC_OK)
    return HANTRO_FALSE;
  VCEncLookaheadJob *job = (VCEncLookaheadJob *)malloc(sizeof(VCEncLookaheadJob));
  memcpy(&job->encIn, pEncIn, sizeof(VCEncIn));
  memcpy(&job->encOut, pEncOut, sizeof(VCEncOut));
  job->encIn.gopConfig.pGopPicCfg = pEncIn->gopConfig.pGopPicCfgPass1;
  OutputBuffer *pBuf = (OutputBuffer*)malloc(sizeof(OutputBuffer));
  pBuf->pOutBuf   =job->encIn.pOutBuf[0];
  pBuf->busOutBuf =job->encIn.busOutBuf[0];
  pBuf->outBufSize=job->encIn.outBufSize[0];
  queue_put(&lookahead->outbuf, (struct node *)pBuf);

  pthread_mutex_lock(&lookahead->job_mutex);
  queue_put(&lookahead->jobs, (struct node *)job);
  pthread_mutex_unlock(&lookahead->job_mutex);
  pthread_cond_signal(&lookahead->job_cond);

  return HANTRO_TRUE;
}
/* Get cutree analyse result */
VCEncLookaheadJob *GetLookaheadOutput(VCEncLookahead *pLookahead, bool bFlush)
{
  VCEncLookahead *lookahead = &((struct vcenc_instance *)(pLookahead->priv_inst))->lookahead;

  pthread_mutex_lock(&lookahead->output_mutex);
  VCEncLookaheadJob *output = (VCEncLookaheadJob *)queue_get(&lookahead->output);
  if(bFlush) {
    pLookahead->bFlush = HANTRO_TRUE;
    pthread_cond_signal(&pLookahead->job_cond);
  }
  while(output == NULL && !((struct vcenc_instance *)(pLookahead->priv_inst))->cuTreeCtl.terminated) {
    pthread_cond_wait(&lookahead->output_cond, &lookahead->output_mutex);
    output = (VCEncLookaheadJob *)queue_get(&lookahead->output);
  }
  if(output == NULL) {
    pthread_mutex_unlock(&lookahead->output_mutex);
    return NULL;
  }
  pthread_mutex_unlock(&lookahead->output_mutex);
  if(output->status == VCENC_FRAME_READY) {
    /* collect gop info */
    i32 lastPoc = output->encIn.poc;
    i32 lastGopPicIdx = output->encIn.gopPicIdx;
    VCEncPictureCodingType lastCodingType = output->encIn.codingType;
    if(pLookahead->lastPoc != -1) {
      output->encIn.poc = pLookahead->lastPoc;
      output->encIn.gopPicIdx = pLookahead->lastGopPicIdx;
      output->encIn.codingType = pLookahead->lastCodingType;
    }
    pLookahead->lastPoc = lastPoc;
    pLookahead->lastGopPicIdx = lastGopPicIdx;
    pLookahead->lastCodingType = lastCodingType;
    /* output buffer reorder */
    OutputBuffer *pBuf = (OutputBuffer *)queue_get(&pLookahead->outbuf);
    output->encIn.pOutBuf[0]   =pBuf->pOutBuf;
    output->encIn.busOutBuf[0] =pBuf->busOutBuf;
    output->encIn.outBufSize[0]=pBuf->outBufSize;
    free(pBuf);
    /* gopConfig selection */
    output->encIn.gopConfig.pGopPicCfg = output->encIn.gopConfig.pGopPicCfgPass2;
  } else if(output->status != VCENC_FRAME_ENQUEUE) {
    OutputBuffer *pBuf = (OutputBuffer *)queue_get(&pLookahead->outbuf);
    if(pBuf) free(pBuf);
  }

  return output;
}
/* Release roi map buffer after pass 2 encoding */
void ReleaseLookaheadPicture(VCEncLookahead *lookahead, VCEncLookaheadJob *output)
{
  if(output) {
    PutRoiMapBufferToBufferPool(&((struct vcenc_instance*)(lookahead->priv_inst))->cuTreeCtl, (u8*)output->encIn.roiMapDeltaQpAddr);
    free(output);
  }
}

/* Get input for pass1 encoding */
static VCEncLookaheadJob *DequeueLookaheadJob(VCEncLookahead *lookahead)
{
  static i32 cnt = 0;
  pthread_mutex_lock(&lookahead->job_mutex);
  VCEncLookaheadJob *job = (VCEncLookaheadJob *)queue_get(&lookahead->jobs);
  while(job == NULL && !lookahead->bFlush) {
    pthread_cond_wait(&lookahead->job_cond, &lookahead->job_mutex);
    job = (VCEncLookaheadJob *)queue_get(&lookahead->jobs);
    if(job == NULL && lookahead->bFlush)
      return NULL;
  }
  pthread_mutex_unlock(&lookahead->job_mutex);
  return job;
}
/* Enqueue cutree analyse result for pass 2 use */
bool LookaheadEnqueueOutput(VCEncLookahead *lookahead, VCEncLookaheadJob *output)
{
  pthread_mutex_lock(&lookahead->output_mutex);
  if(output->status != VCENC_FRAME_READY && output->status != VCENC_FRAME_ENQUEUE) {
    struct node *p;
    while((p = queue_get(&lookahead->output)) != NULL)
      free(p);
    while((p = queue_get(&lookahead->outbuf)) != NULL)
      free(p);
  }
  queue_put(&lookahead->output, (struct node *)output);
  pthread_mutex_unlock(&lookahead->output_mutex);
  pthread_cond_signal(&lookahead->output_cond);

  return HANTRO_TRUE;
}
/* lookahead thread performs pass 1 encoding, passing result to cutree thread */
void *LookaheadThread(void *arg)
{
  VCEncRet ret = VCENC_OK;
  VCEncLookaheadJob *job = NULL;
  VCEncLookahead *lookahead = (VCEncLookahead *)arg;
  VCEncLookahead *lookahead2 = &((struct vcenc_instance *)(lookahead->priv_inst))->lookahead;
  VCEncLookaheadJob output;
  i32 i;
  i32 ndelay = CUTREE_BUFFER_CNT(((struct vcenc_instance *)(lookahead->priv_inst))->cuTreeCtl.lookaheadDepth)-1;
  for(i = 0; i < ndelay; i++) {
    VCEncLookaheadJob *out = (VCEncLookaheadJob*)malloc(sizeof(VCEncLookaheadJob));
    out->next = NULL;
    out->status = VCENC_FRAME_ENQUEUE;
    LookaheadEnqueueOutput(lookahead2,out);
  }
  while((job=DequeueLookaheadJob(lookahead))!=NULL) {
    job->status = VCEncStrmEncode(lookahead->priv_inst, &job->encIn, &job->encOut, NULL, NULL);
    if(job->status == VCENC_FRAME_READY) {
      ASSERT(job->encOut.codingType != VCENC_NOTCODED_FRAME);
      ret = cuTreeAddFrame(lookahead->priv_inst, job);
    } else {
      lookahead->status = lookahead2->status = job->status;
      LookaheadEnqueueOutput(lookahead2,job);
      break;
    }
  }
  cuTreeFlush(&((struct vcenc_instance*)lookahead->priv_inst)->cuTreeCtl);
  return NULL;
}

/* cutree analyse thread. */
void *cuTreeThread(void *arg)
{
  struct cuTreeCtr *m_param = (struct cuTreeCtr *)arg;
  struct Lowres *cur_frame = NULL;
  static i32 cnt = 0;

  while(!m_param->bFlush || m_param->job_cnt > 0) {
    pthread_mutex_lock(&m_param->cutree_mutex);
    while (!m_param->bFlush && m_param->job_cnt == 0) {
      pthread_cond_wait(&m_param->cutree_cond, &m_param->cutree_mutex);
    }
    if(m_param->job_cnt > 0) {
      cur_frame = (struct Lowres *)queue_get(&m_param->jobs);
      m_param->job_cnt--;
      pthread_mutex_unlock(&m_param->cutree_mutex);
      cuTreeHandleInputFrame(cur_frame, m_param);
    } else
      pthread_mutex_unlock(&m_param->cutree_mutex);
  }
  // Flush remaining frames
  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
  if(enc->lookahead.status == VCENC_OK) {
    while(m_param->nLookaheadFrames > 1)
      process_one_frame(m_param);
    if(m_param->nLookaheadFrames == 1 && m_param->lookaheadFrames[0]->sliceType == I_SLICE)
      process_one_frame(m_param);
  } else {
    pthread_cond_signal(&m_param->agop_cond);
  }
  while(m_param->nLookaheadFrames > 0)
    remove_one_frame(m_param);
  m_param->terminated = HANTRO_TRUE;
  return NULL;
}

/* Start lookahead (pass1) thread. */
VCEncRet StartLookaheadThread(VCEncLookahead *lookahead)
{
  VCEncLookahead *lookahead2 = &((struct vcenc_instance *)(lookahead->priv_inst))->lookahead;
#ifndef _WIN32
  pthread_attr_t attr;
  pthread_t tid_lookahead;
  pthread_mutexattr_t mutexattr;
  pthread_condattr_t condattr;
#else
  DWORD threadID;
  HANDLE tid_lookahead = 0;
#endif
  queue_init(&lookahead->jobs);
  queue_init(&lookahead2->output);
  queue_init(&lookahead->outbuf);
  lookahead->lastPoc = -1;
  lookahead->last_idr_picture_cnt = lookahead->picture_cnt = 0;
#ifndef _WIN32
  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&lookahead->job_mutex, &mutexattr);
  pthread_mutex_init(&lookahead2->output_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);
  pthread_condattr_init(&condattr);
  pthread_cond_init(&lookahead->job_cond, &condattr);
  pthread_cond_init(&lookahead2->output_cond, &condattr);
  pthread_condattr_destroy(&condattr);
  pthread_attr_init(&attr);
  pthread_create(&tid_lookahead, &attr, &LookaheadThread, lookahead);
  pthread_attr_destroy(&attr);
#else
  tid_lookahead = CreateThread(NULL, 0, (THREAD_ROUTINE)LookaheadThread, (void*)lookahead, 0, &threadID);
#endif // !_WIN32
  lookahead->tid_lookahead = tid_lookahead;
  lookahead->bFlush = HANTRO_FALSE;
  lookahead->status = lookahead2->status = VCENC_OK;
  return tid_lookahead ? VCENC_OK : VCENC_ERROR;
}
/* Start cutree analyse thread. */
VCEncRet StartCuTreeThread(struct cuTreeCtr *m_param)
{
#ifndef _WIN32
  pthread_attr_t attr;
  pthread_t tid_cutree;
  pthread_mutexattr_t mutexattr;
  pthread_condattr_t condattr;
#else
  DWORD threadID;
  HANDLE tid_cutree = 0;
#endif
#ifndef _WIN32
  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&m_param->cutree_mutex, &mutexattr);
  pthread_mutex_init(&m_param->roibuf_mutex, &mutexattr);
  pthread_mutex_init(&m_param->cuinfobuf_mutex, &mutexattr);
  pthread_mutex_init(&m_param->agop_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);
  pthread_condattr_init(&condattr);
  pthread_cond_init(&m_param->cutree_cond, &condattr);
  pthread_cond_init(&m_param->roibuf_cond, &condattr);
  pthread_cond_init(&m_param->cuinfobuf_cond, &condattr);
  m_param->cuInfoToRead = 0;
  pthread_cond_init(&m_param->agop_cond, &condattr);
  pthread_condattr_destroy(&condattr);
  pthread_attr_init(&attr);
  pthread_create(&tid_cutree, &attr, &cuTreeThread, m_param);
  pthread_attr_destroy(&attr);
#else
  tid_cutree = CreateThread(NULL, 0, (THREAD_ROUTINE)cuTreeThread, (void*)m_param, 0, &threadID);
#endif // !_WIN32
  m_param->tid_cutree = tid_cutree;
  return tid_cutree ? VCENC_OK : VCENC_ERROR;
}

/* Terminate lookahead (pass1) thread. */
VCEncRet TerminateLookaheadThread(VCEncLookahead *lookahead)
{
  // destroy lookahead thread
  if(!lookahead->tid_lookahead) return VCENC_OK;
  VCEncLookahead *lookahead2 = &((struct vcenc_instance *)(lookahead->priv_inst))->lookahead;
#ifndef _WIN32
  pthread_join(lookahead->tid_lookahead,NULL);
  pthread_mutex_destroy(&lookahead->job_mutex);
  pthread_mutex_destroy(&lookahead2->output_mutex);
  pthread_cond_destroy(&lookahead->job_cond);
  pthread_cond_destroy(&lookahead2->output_cond);
#else
  WaitForSingleObject(lookahead->tid_lookahead, INFINITE);
  CloseHandle(tid_lookahead);
#endif
  lookahead->tid_lookahead = 0;
  lookahead->bFlush = true;
  VCEncLookaheadJob *job = NULL;
  while((job = (VCEncLookaheadJob *)queue_get(&lookahead->jobs))!=NULL)
    free(job);
  struct node *p;
  while((p = queue_get(&lookahead->outbuf)) != NULL)
    free(p);
  while((job = (VCEncLookaheadJob *)queue_get(&lookahead->output)) != NULL)
    ReleaseLookaheadPicture(lookahead, job);
  return VCENC_OK;
}
/* Terminate cutree analyse thread. */
VCEncRet TerminateCuTreeThread(struct cuTreeCtr *m_param)
{
  if(!m_param->tid_cutree) return VCENC_OK;
  cuTreeFlush(m_param);
#ifndef _WIN32
  pthread_join(m_param->tid_cutree,NULL);
  pthread_mutex_destroy(&m_param->cutree_mutex);
  pthread_cond_destroy(&m_param->cutree_cond);
  pthread_mutex_destroy(&m_param->roibuf_mutex);
  pthread_cond_destroy(&m_param->roibuf_cond);
  pthread_mutex_destroy(&m_param->cuinfobuf_mutex);
  pthread_cond_destroy(&m_param->cuinfobuf_cond);
#else
  WaitForSingleObject(m_param->tid_cutree, INFINITE);
  CloseHandle(tid_cutree);
#endif
  m_param->tid_cutree = 0;
  return VCENC_OK;
}
VCEncRet waitCuInfoBufPass1(struct vcenc_instance *vcenc_instance)
{
  struct cuTreeCtr *m_param = &vcenc_instance->cuTreeCtl;
  pthread_mutex_lock(&m_param->cuinfobuf_mutex);

  while(m_param->cuInfoToRead == vcenc_instance->numCuInfoBuf)
    pthread_cond_wait(&m_param->cuinfobuf_cond, &m_param->cuinfobuf_mutex);
  m_param->cuInfoToRead++;
  pthread_mutex_unlock(&m_param->cuinfobuf_mutex);
  return VCENC_OK;
}
