/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract  :   CuTree process via HW
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include <string.h>
#include "enccommon.h"
#include "ewl.h"
#include "hevcencapi.h"
#include "instance.h"
#include "sw_cu_tree.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
const struct Lowres *invalid_frame = NULL;


/*------------------------------------------------------------------------------

    VCEncCuTreeCodeFrame

------------------------------------------------------------------------------*/
#define SET_CUTREE_INPUT_REG(i) \
  if(i < m_param->nLookaheadFrames) { \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_POC##i,   m_param->lookaheadFrames[i]->poc); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_QP##i,   m_param->lookaheadFrames[i]->qp); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CODINGTYPE##i,   X265_CODING_TYPE(m_param->lookaheadFrames[i]->sliceType)); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ENCODED_FRAME_NUMBER##i,   m_param->lookaheadFrames[i]->gopEncOrder); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_L0_DELTA_POC##i,   m_param->lookaheadFrames[i]->p0); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_L1_DELTA_POC##i,   m_param->lookaheadFrames[i]->p1); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CUDATAIDX##i,   m_param->lookaheadFrames[i]->cuDataIdx); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ROIMAPDELTAIDX##i,   m_param->lookaheadFrames[i]->inRoiMapDeltaBinIdx); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_HIERARCHIAL_BIT_ALLOCATION_GOP_SIZE##i,   m_param->lookaheadFrames[i]->gopSize); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_AGOPSIZE##i,   m_param->lookaheadFrames[i]->aGopSize); \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_PROPAGATECOSTIDX##i,   m_param->lookaheadFrames[i]->propagateCostIdx); \
  }
#define SET_CUTREE_OUTPUT_ADDRREG(i) \
  if(i < m_param->nLookaheadFrames) { \
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ROIMAPDELTAQPADDRIDX##i,   m_param->lookaheadFrames[i]->outRoiMapDeltaQpIdx); \
  }
#define MAKE_CMD(cur, last, middle, frame, frame_p0, frame_p1, cie, coe, qoe, clear_first, clear_last) \
  (((u64)frame->cuDataIdx << 32) | ((u64)(coe?middle:0xf) << 38) | ((u64)frame->sliceType << 42) | ((u64)(!clear_first) << 45) | ((u64)(!clear_last) << 46) \
   | (cie<<28) | (coe<<29) | (qoe<<30) | ((frame->outRoiMapDeltaQpIdx & 0xf)<<24) | (frame->inRoiMapDeltaBinIdx<<18) \
   | ((frame_p1?frame_p1->propagateCostIdx:0x3f)<<12) | ((frame_p0?frame_p0->propagateCostIdx:0x3f)<<6) | frame->propagateCostIdx)
#define GET_CUTREE_OUTPUT_REG(i) \
  if(i < m_param->nLookaheadFrames) { \
    m_param->lookaheadFrames[i]->cost = (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CUTREE_COST##i) + ((u64)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_CUTREE_COST##i##_MSB) << 32) + m_param->unitCount/2)/m_param->unitCount/4; \
  }

/*------------------------------------------------------------------------------
    Function name   : CuTreeAsicFrameStart
    Description     :
    Return type     : void
    Argument        : const void *ewl
    Argument        : regValues_s * val
------------------------------------------------------------------------------*/
#define HSWREG(n)       ((n)*4)
void CuTreeAsicFrameStart(const void *ewl, regValues_s *val)
{
  i32 i;

  //val->asic_pic_swap = ((val->asic_pic_swap));

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WR_ID_E, 0);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_RD_ID_E, 0);

  /* clear all interrupt */
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_SLICE_RDY_STATUS, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_IRQ_LINE_BUFFER, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_TIMEOUT, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_BUFFER_FULL, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_SW_RESET, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_BUS_ERROR_STATUS, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_FRAME_RDY_STATUS, 1);
  EncAsicSetRegisterValue(val->regMirror,     HWIF_ENC_IRQ, 1);

  /* encoder interrupt */
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_IRQ_DIS, val->irqDisable);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_READ_ID, val->asic_axi_readID);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WRITE_ID, val->asic_axi_writeID);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_E, val->asic_clock_gating_encoder);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_H265_E, val->asic_clock_gating_encoder_h265);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_ENCODER_H264_E, val->asic_clock_gating_encoder_h264);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_E, val->asic_clock_gating_inter);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_H265_E, val->asic_clock_gating_inter_h265);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_CLOCK_GATE_INTER_H264_E, val->asic_clock_gating_inter_h264);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MODE, val->codingType);

  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_ROI_MAP_QP_DELTA_MAP_SWAP, (~(ENCH2_ROIMAP_QPDELTA_SWAP_8 | (ENCH2_ROIMAP_QPDELTA_SWAP_16 << 1) | (ENCH2_ROIMAP_QPDELTA_SWAP_32 << 2) | (ENCH2_ROIMAP_QPDELTA_SWAP_64 << 3))) & 0xf);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_MAX_BURST, ENCH2_AXI40_BURST_LENGTH);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_WRITE_OUTSTANDING_NUM, ENCH2_ASIC_AXI_WRITE_OUTSTANDING_NUM);
  EncAsicSetRegisterValue(val->regMirror, HWIF_ENC_AXI_READ_OUTSTANDING_NUM, ENCH2_ASIC_AXI_READ_OUTSTANDING_NUM);

  val->regMirror[HWIF_REG_CFG1] = EWLReadReg(ewl,HWIF_REG_CFG1*4);
  val->regMirror[HWIF_REG_CFG2] = EWLReadReg(ewl,HWIF_REG_CFG2*4);
  val->regMirror[HWIF_REG_CFG3] = EWLReadReg(ewl,HWIF_REG_CFG3*4);
  val->regMirror[HWIF_REG_CFG4] = EWLReadReg(ewl,HWIF_REG_CFG4*4);

  for (i = 1; i < ASIC_SWREG_AMOUNT; i++)
    EWLWriteReg(ewl, HSWREG(i), val->regMirror[i]);

#ifdef TRACE_REGS
  EncTraceRegs(ewl, 0, 0);
#endif
  /* Register with enable bit is written last */
  val->regMirror[ASIC_REG_INDEX_STATUS] |= ASIC_STATUS_ENABLE;

  EWLEnableHW(ewl, HSWREG(ASIC_REG_INDEX_STATUS), val->regMirror[ASIC_REG_INDEX_STATUS]);
}

static void collectGopCmds(struct cuTreeCtr *m_param, struct Lowres **frames, i32 cur, i32 last, i32 depth, bool *p_clear_first, bool *p_clear_last, bool *p_clear_middle, bool coe, int outGopSize)
{
  bool clear_first = *p_clear_first;
  bool clear_last = *p_clear_last;
  i32 bframes = last - cur - 1;
  i32 i = last - 1;
  int middle = (bframes + 1) / 2 + cur;
  bool cie;
  bool qoe;
  struct Lowres fake_frame;
  struct Lowres *p_fake_frame = &fake_frame;
  p_fake_frame->propagateCostIdx = m_param->nLookaheadFrames;
  if(cur < 0) return;
  if(depth == 0)
    m_param->maxHieDepth = ((frames[last]->gopSize == 8) && (frames[last]->aGopSize == 4)) ? 3 : DEFAULT_MAX_HIE_DEPTH;
  cie = (depth < m_param->maxHieDepth && bframes > 1);
  if (bframes > 1)
  {
      bool clear_middle = true;
      bool tmp;
      collectGopCmds(m_param, frames, middle, last, depth+1, &clear_middle, &clear_last, &tmp, coe, outGopSize);
      collectGopCmds(m_param, frames, cur, middle, depth+1, &clear_first, &clear_middle, &tmp, coe, outGopSize);
      if(clear_middle)
        cie = false;
  }

  if (bframes >= 1)
  {
    qoe = coe && (depth == 0);
    if(frames[middle]->p0 == (middle-cur) && frames[middle]->p1 == (last-middle) && depth+1 <= m_param->maxHieDepth) {
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(cur, last, middle, frames[middle], frames[cur], frames[last], cie, true, qoe, clear_first, clear_last);
      clear_first = clear_last = false;
    } else {
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(middle, middle, middle, frames[middle], p_fake_frame, p_fake_frame, cie, true, false, true, true);
    }
    ASSERT(m_param->num_cmds <= 56);
  }
  *p_clear_middle = !cie;
  if (depth == 0) {
    cie = !clear_last;
    qoe = coe || m_param->nLookaheadFrames <= outGopSize+1;
    if(frames[last]->p0 == (last-cur))
    {
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(cur, last, last, frames[last], frames[cur], invalid_frame, cie, (cur >= outGopSize || m_param->nLookaheadFrames <= outGopSize+1), qoe, clear_first, clear_last);
      clear_first = false;
    }
    else
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(last, last, last, frames[last], p_fake_frame, p_fake_frame, cie, true, false, true, true);
    clear_last = false;
  }
  *p_clear_first = clear_first;
  *p_clear_last = clear_last;
}
void processGopConvert_4to8_asic (struct cuTreeCtr *m_param, struct Lowres **frames);
static void collectCmds(struct cuTreeCtr* m_param, struct Lowres **frames, int numframes, bool bFirst)
{
    int idx = !bFirst;
    int lastnonb, curnonb = 1;
    int bframes = 0;
    int i = numframes;
    m_param->num_cmds = 0;
    bool bAgop_4to8 = (m_param->nLookaheadFrames > 8 && (frames[4]->gopEncOrder == 0) && (frames[4]->gopSize == 4) && (frames[4]->aGopSize == 8) &&
        (frames[8]->gopEncOrder == 0) && (frames[8]->gopSize == 4) && (frames[8]->aGopSize == 8));
    int outGopSize = (m_param->nLookaheadFrames > 1 ? (bAgop_4to8 ? 8 : frames[1]->gopSize) : 1);

    if(!(bFirst && bAgop_4to8))
      processGopConvert_4to8_asic(m_param, frames);

    while (i > 0 && IS_X265_TYPE_B(frames[i]->sliceType))
        i--;

    lastnonb = i;

    bool clear_last = true;
    bool clear_first = true;
    bool clear_middle = true;
    /* est one gop */
    while (i-- > 0)
    {
        curnonb = i;
        while (IS_X265_TYPE_B(frames[curnonb]->sliceType) && curnonb > 0)
            curnonb--;
  
        bframes = lastnonb - curnonb - 1;
        clear_first = true;
        collectGopCmds(m_param, frames, curnonb, lastnonb, 0, &clear_first, &clear_last, &clear_middle, false, outGopSize);
        if(curnonb == outGopSize)
          m_param->commands[m_param->num_cmds++] = MAKE_CMD(curnonb, curnonb, curnonb, frames[curnonb], invalid_frame, invalid_frame, (!clear_first), true, true, false, false);
        lastnonb = i = curnonb;
        clear_last = clear_first;
    }
    if(bFirst)
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(0, 0, 0, frames[0], invalid_frame, invalid_frame, (!clear_first), true, true, false, false);
    if(bFirst && bAgop_4to8)
    {
      clear_first = clear_last = true;
      processGopConvert_4to8_asic(m_param, frames);
      collectGopCmds(m_param, frames, 0, 8, 0, &clear_first, &clear_last, &clear_middle, false, outGopSize);
    }
    if(outGopSize > 1)
      m_param->commands[m_param->num_cmds++] = MAKE_CMD(outGopSize/2, outGopSize/2, outGopSize/2, frames[outGopSize/2], invalid_frame, invalid_frame, (!clear_middle), true, true, false, false);
    ASSERT(m_param->num_cmds > 0);
}

VCEncRet VCEncCuTreeStart(struct cuTreeCtr *m_param)
{
  int i;
  VCEncRet ret;
  regValues_s *regs = &m_param->asic.regs;
  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
  i32 roiAbsQpSupport = ((struct vcenc_instance *)enc)->asic.regs.asicCfg.roiAbsQpSupport;
  i32 roiMapVersion = ((struct vcenc_instance *)enc)->asic.regs.asicCfg.roiMapVersion;
  i32 cuInforVersion = ((struct vcenc_instance *)enc)->asic.regs.cuInfoVersion;
  ptr_t busAddress = 0;
  u8 *memory;
  struct Lowres **frames = m_param->lookaheadFrames;

  EWLmemset(regs->regMirror, 0, sizeof(regs->regMirror));
  
  for(i = 0; i < MIN(9, m_param->nLookaheadFrames); i++) {
    if(m_param->lookaheadFrames[i]->outRoiMapDeltaQpIdx != INVALID_INDEX)
      continue;
    memory = GetRoiMapBufferFromBufferPool(m_param, &busAddress);
    m_param->lookaheadFrames[i]->outRoiMapDeltaQpIdx = (busAddress - m_param->outRoiMapDeltaQp_Base) / m_param->outRoiMapDeltaQp_frame_size;
    memset(memory, 0, m_param->outRoiMapDeltaQp_frame_size);
  }
  int totalDuration = 0;
  for (int j = 0; j < m_param->nLookaheadFrames; j++) {
    totalDuration += m_param->fpsDenom*256 / m_param->fpsNum;
    m_param->lookaheadFrames[j]->propagateCostIdx = j;
  }
  int averageDuration = totalDuration / (m_param->nLookaheadFrames); // Q24.8
  int fpsFactor = (int)(CLIP_DURATION_FIX8(averageDuration) * 256 / CLIP_DURATION_FIX8((m_param->fpsDenom<<8) / m_param->fpsNum)); // Q24.8

  /* set new frame encoding parameters */
  EncAsicSetRegisterValue(regs->regMirror, HWIF_TIMEOUT_OVERRIDE_E, ENCH2_ASIC_TIMEOUT_OVERRIDE_ENABLE);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_TIMEOUT_CYCLES, ENCH2_ASIC_TIMEOUT_CYCLES & 0x7fffff);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_TIMEOUT_CYCLES_MSB, ENCH2_ASIC_TIMEOUT_CYCLES >> 23);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CODECH264,   IS_H264(m_param->codecFormat));
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_HEIGHT,   m_param->height*m_param->dsRatio);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_WIDTH,   m_param->width*m_param->dsRatio);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ROIMAPENABLE,   m_param->roiMapEnable);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_HWLOOKAHEADDEPTH,   m_param->nLookaheadFrames);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_INLOOPDSRATIO,   m_param->dsRatio-1);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_BPASS1ADAPTIVEGOP,   m_param->bUpdateGop);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CUINFORVERSION,   cuInforVersion);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ROIMAPVERSION,   roiMapVersion);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_ROIABSQPSUPPORT,   roiAbsQpSupport);
  u32 cuTreeStrength = (((5 * (10000 - (int)(m_param->qCompress*10000+0.5))) << 8) + 5000) / 10000;  // Q24.8
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_STRENGTH, cuTreeStrength);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_FPSFACTOR, fpsFactor);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_INQPDELTABLKSIZE, m_param->inQpDeltaBlkSize/m_param->dsRatio==64?2:(m_param->inQpDeltaBlkSize/m_param->dsRatio==32?1:0));
  m_param->outRoiMapDeltaQpBlockUnit = IS_AV1(m_param->codecFormat)?0:1;
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_OUTPUT_BLOCKUNITSIZE, m_param->outRoiMapDeltaQpBlockUnit);
  EncAsicSetAddrRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CUDATA_BASE,   m_param->cuData_Base);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CUDATA_FRAME_SIZE,   m_param->cuData_frame_size);
  EncAsicSetAddrRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_INROIMAPDELTABIN_BASE,   m_param->inRoiMapDeltaBin_Base);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_INROIMAPDELTABIN_FRAME_SIZE,   m_param->inRoiMapDeltaBin_frame_size);
  EncAsicSetAddrRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_OUTROIMAPDELTAQP_BASE,   m_param->outRoiMapDeltaQp_Base);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_OUTROIMAPDELTAQP_FRAME_SIZE,   m_param->outRoiMapDeltaQp_frame_size);
  EncAsicSetAddrRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_PROPAGATECOST_BASE,   m_param->propagateCost_Base);
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_PROPAGATECOST_FRAME_SIZE,   m_param->propagateCost_frame_size);
  SET_CUTREE_INPUT_REG(0);
  SET_CUTREE_INPUT_REG(1);
  SET_CUTREE_INPUT_REG(2);
  SET_CUTREE_INPUT_REG(3);
  SET_CUTREE_INPUT_REG(4);
  SET_CUTREE_INPUT_REG(5);
  SET_CUTREE_INPUT_REG(6);
  SET_CUTREE_INPUT_REG(7);
  SET_CUTREE_INPUT_REG(8);
  SET_CUTREE_INPUT_REG(9);
  SET_CUTREE_INPUT_REG(10);
  SET_CUTREE_INPUT_REG(11);
  SET_CUTREE_INPUT_REG(12);
  SET_CUTREE_INPUT_REG(13);
  SET_CUTREE_INPUT_REG(14);
  SET_CUTREE_INPUT_REG(15);
  SET_CUTREE_INPUT_REG(16);
  SET_CUTREE_INPUT_REG(17);
  SET_CUTREE_INPUT_REG(18);
  SET_CUTREE_INPUT_REG(19);
  SET_CUTREE_INPUT_REG(20);
  SET_CUTREE_INPUT_REG(21);
  SET_CUTREE_INPUT_REG(22);
  SET_CUTREE_INPUT_REG(23);
  SET_CUTREE_INPUT_REG(24);
  SET_CUTREE_INPUT_REG(25);
  SET_CUTREE_INPUT_REG(26);
  SET_CUTREE_INPUT_REG(27);
  SET_CUTREE_INPUT_REG(28);
  SET_CUTREE_INPUT_REG(29);
  SET_CUTREE_INPUT_REG(30);
  SET_CUTREE_INPUT_REG(31);
  SET_CUTREE_INPUT_REG(32);
  SET_CUTREE_INPUT_REG(33);
  SET_CUTREE_INPUT_REG(34);
  SET_CUTREE_INPUT_REG(35);
  SET_CUTREE_INPUT_REG(36);
  SET_CUTREE_INPUT_REG(37);
  SET_CUTREE_INPUT_REG(38);
  SET_CUTREE_INPUT_REG(39);
  SET_CUTREE_INPUT_REG(40);
  SET_CUTREE_INPUT_REG(41);
  SET_CUTREE_INPUT_REG(42);
  SET_CUTREE_INPUT_REG(43);
  SET_CUTREE_INPUT_REG(44);
  SET_CUTREE_INPUT_REG(45);
  SET_CUTREE_INPUT_REG(46);
  SET_CUTREE_INPUT_REG(47);
  SET_CUTREE_OUTPUT_ADDRREG(0);
  SET_CUTREE_OUTPUT_ADDRREG(1);
  SET_CUTREE_OUTPUT_ADDRREG(2);
  SET_CUTREE_OUTPUT_ADDRREG(3);
  SET_CUTREE_OUTPUT_ADDRREG(4);
  SET_CUTREE_OUTPUT_ADDRREG(5);
  SET_CUTREE_OUTPUT_ADDRREG(6);
  SET_CUTREE_OUTPUT_ADDRREG(7);
  SET_CUTREE_OUTPUT_ADDRREG(8);
  collectCmds(m_param, m_param->lookaheadFrames, m_param->nLookaheadFrames-1, IS_X265_TYPE_I(frames[0]->sliceType));
  for(i = 0; i < m_param->num_cmds; i++) {
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CMD0+2*i, (u32)m_param->commands[i]);
    EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_CMD0+2*i+1, (u32)(m_param->commands[i]>>32));
  }
  EncAsicSetRegisterValue(regs->regMirror, HWIF_ENC_CUTREE_NUM_CMDS,   m_param->num_cmds);

  /* Try to reserve the HW resource */
  u32 reserve_core_info = 0;

  u32 core_info = 0; /*mode[1bit](1:all 0:specified)+amount[3bit](the needing amount -1)+reserved+core_mapping[8bit]*/
  //u32 valid_num =0;
  //EWLHwConfig_t cfg;

  //for (i=0; i< EWLGetCoreNum(); i++)
  //{
  // cfg = EncAsicGetAsicConfig(i);
  // if (cfg.cuTreeSupport == 1)
  // {
    //core_info |= 1<<i;
  //  valid_num++;
  // }
  // else
  //  continue;
  //}
  //if (valid_num == 0) /*none of cores is supported*/
  //  return VCENC_INVALID_ARGUMENT;

  core_info |= 1<< CORE_INFO_MODE_OFFSET; //now just support 1 core,so mode is all.

  core_info |= 0<< CORE_INFO_AMOUNT_OFFSET;//now just support 1 core

  reserve_core_info = core_info;

  if ((EWLReserveHw(m_param->asic.ewl,&reserve_core_info) == EWL_ERROR) || (EWLCheckCutreeValid(m_param->asic.ewl) == EWL_ERROR))
  {
    return VCENC_HW_RESERVED;
  }

  /* start hw encoding */
  CuTreeAsicFrameStart(m_param->asic.ewl, &m_param->asic.regs);

  return VCENC_OK;
}

VCEncRet VCEncCuTreeWait(struct cuTreeCtr * m_param)
{
    VCEncRet ret;
    asicData_s *asic = &m_param->asic;
    u32 status = ASIC_STATUS_ERROR;

    do {
        /* Encode one frame */
        i32 ewl_ret;

        /* Wait for IRQ */
        ewl_ret = EWLWaitHwRdy(asic->ewl, NULL, 0, &status);
        if(ewl_ret != EWL_OK)
        {
            status = ASIC_STATUS_ERROR;

            if(ewl_ret == EWL_ERROR)
            {
                /* IRQ error => Stop and release HW */
                ret = VCENC_SYSTEM_ERROR;
            }
            else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
            {
                /* IRQ Timeout => Stop and release HW */
                ret = VCENC_HW_TIMEOUT;
            }

            EncAsicStop(asic->ewl);
            /* Release HW so that it can be used by other codecs */
            EWLReleaseHw(asic->ewl);

        }
        else
        {
            
            status = EncAsicCheckStatus_V2(asic, status);

            switch (status)
            {
            case ASIC_STATUS_ERROR:
                EWLReleaseHw(asic->ewl);
                ret = VCENC_ERROR;
                break;
            case ASIC_STATUS_HW_RESET:
                EWLReleaseHw(asic->ewl);
                ret = VCENC_HW_RESET;
                break;
            case ASIC_STATUS_HW_TIMEOUT:
                EWLReleaseHw(asic->ewl);
                ret = VCENC_HW_TIMEOUT;
                break;
            case ASIC_STATUS_FRAME_READY:
                if (m_param->nLookaheadFrames > 8 && ((m_param->lookaheadFrames[4]->gopEncOrder == 0) && (m_param->lookaheadFrames[4]->gopSize == 4) && (m_param->lookaheadFrames[4]->aGopSize == 8) &&
                      (m_param->lookaheadFrames[8]->gopEncOrder == 0) && (m_param->lookaheadFrames[8]->gopSize == 4) && (m_param->lookaheadFrames[8]->aGopSize == 8)))
                  m_param->rem_frames = m_param->nLookaheadFrames-8;
                else
                  m_param->rem_frames = m_param->nLookaheadFrames-(m_param->nLookaheadFrames > 1 ? m_param->lookaheadFrames[1]->gopSize : 0);
                GET_CUTREE_OUTPUT_REG(0);
                GET_CUTREE_OUTPUT_REG(1);
                GET_CUTREE_OUTPUT_REG(2);
                GET_CUTREE_OUTPUT_REG(3);
                GET_CUTREE_OUTPUT_REG(4);
                GET_CUTREE_OUTPUT_REG(5);
                GET_CUTREE_OUTPUT_REG(6);
                GET_CUTREE_OUTPUT_REG(7);
                GET_CUTREE_OUTPUT_REG(8);
                GET_CUTREE_OUTPUT_REG(9);
                GET_CUTREE_OUTPUT_REG(10);
                GET_CUTREE_OUTPUT_REG(11);
                GET_CUTREE_OUTPUT_REG(12);
                GET_CUTREE_OUTPUT_REG(13);
                GET_CUTREE_OUTPUT_REG(14);
                GET_CUTREE_OUTPUT_REG(15);
                GET_CUTREE_OUTPUT_REG(16);
                GET_CUTREE_OUTPUT_REG(17);
                GET_CUTREE_OUTPUT_REG(18);
                GET_CUTREE_OUTPUT_REG(19);
                GET_CUTREE_OUTPUT_REG(20);
                GET_CUTREE_OUTPUT_REG(21);
                GET_CUTREE_OUTPUT_REG(22);
                GET_CUTREE_OUTPUT_REG(23);
                GET_CUTREE_OUTPUT_REG(24);
                GET_CUTREE_OUTPUT_REG(25);
                GET_CUTREE_OUTPUT_REG(26);
                GET_CUTREE_OUTPUT_REG(27);
                GET_CUTREE_OUTPUT_REG(28);
                GET_CUTREE_OUTPUT_REG(29);
                GET_CUTREE_OUTPUT_REG(30);
                GET_CUTREE_OUTPUT_REG(31);
                GET_CUTREE_OUTPUT_REG(32);
                GET_CUTREE_OUTPUT_REG(33);
                GET_CUTREE_OUTPUT_REG(34);
                GET_CUTREE_OUTPUT_REG(35);
                GET_CUTREE_OUTPUT_REG(36);
                GET_CUTREE_OUTPUT_REG(37);
                GET_CUTREE_OUTPUT_REG(38);
                GET_CUTREE_OUTPUT_REG(39);
                GET_CUTREE_OUTPUT_REG(40);
                GET_CUTREE_OUTPUT_REG(41);
                GET_CUTREE_OUTPUT_REG(42);
                GET_CUTREE_OUTPUT_REG(43);
                GET_CUTREE_OUTPUT_REG(44);
                GET_CUTREE_OUTPUT_REG(45);
                GET_CUTREE_OUTPUT_REG(46);
                GET_CUTREE_OUTPUT_REG(47);
                ret = VCENC_OK;
                EWLReleaseHw(asic->ewl);
                break;
            default:
                /* should never get here */
                ASSERT(0);
                ret = VCENC_ERROR;
            }
        }
    }while (status == ASIC_STATUS_LINE_BUFFER_DONE || status == ASIC_STATUS_SEGMENT_READY);

    return ret;
}

void setFrameTypeChar (struct Lowres *frame);
static void markBRef(struct cuTreeCtr *m_param, struct Lowres **frames, i32 cur, i32 last, i32 depth)
{
  i32 bframes = last - cur - 1;
  i32 i = last - 1;
  if(cur < 0) return;
  if (bframes > 1)
  {
      int middle = (bframes + 1) / 2 + cur;
      markBRef(m_param, frames, middle, last, depth+1);
      markBRef(m_param, frames, cur, middle, depth+1);

      frames[middle]->sliceType = X265_TYPE_BREF;
      setFrameTypeChar (frames[middle]);
      frames[middle]->predId = getFramePredId(frames[middle]->sliceType);
  }
}
void statisAheadData (struct cuTreeCtr *m_param, struct Lowres **frames, int numframes, bool bFirst);
void remove_one_frame(struct cuTreeCtr *m_param);
static void write_asic_gop_cutree(struct cuTreeCtr *m_param, struct Lowres **frame, i32 size, i32 base);
void processGopConvert_4to8_asic (struct cuTreeCtr *m_param, struct Lowres **frames)
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

    for (i = 1; i <= 8; i ++)
      frames[i]->aGopSize = 0;
  }
}
bool processGopConvert_8to4_asic (struct cuTreeCtr *m_param, struct Lowres **frames)
{
  i32 i;
  if (m_param->nLookaheadFrames <= 8)
    return false;
    
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
    write_asic_gop_cutree(m_param, m_param->lookaheadFrames + 1, 4, 1);

    for (i = 1; i <= 8; i ++)
      frames[i]->aGopSize = 0;

    for (i = 0; i < 4; i ++)
      remove_one_frame(m_param);
    m_param->out_cnt += 4;
    m_param->pop_cnt += 4;
    
    return true;
  }
  return false;
}
static void write_asic_cutree_file(struct cuTreeCtr *m_param, struct Lowres *frame, int i)
{
  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
  VCEncLookaheadJob *out = frame->job;
  ptr_t busAddress = m_param->outRoiMapDeltaQp_Base + m_param->outRoiMapDeltaQp_frame_size * frame->outRoiMapDeltaQpIdx;

  /* output frame info */
  out->encIn.roiMapDeltaQpAddr = busAddress;
  out->frame.poc      = frame->poc;
  out->frame.frameNum = frame->frameNum;
  out->frame.typeChar = frame->typeChar;
  out->frame.qp       = frame->qp;
  out->frame.cost     = (int)(m_param->lookaheadFrames[i]->cost/256.0);
  out->frame.gopSize  = frame->gopSize;
  for(i = 0; i < 4; i++) {
    out->frame.costGop[i] = m_param->costGopInt[i]/256.0;
    out->frame.FrameNumGop[i] = m_param->FrameNumGop[i];
    out->frame.costAvg[i] = m_param->costAvgInt[i]/256.0;
    out->frame.FrameTypeNum[i] = m_param->FrameTypeNum[i];
  }
  out->status = VCENC_FRAME_READY;
  LookaheadEnqueueOutput(&enc->lookahead, out);
  frame->job = NULL;
}

static void write_asic_gop_cutree(struct cuTreeCtr *m_param, struct Lowres **frame, i32 size, i32 base)
{
  i32 i, j;

  markBRef(m_param, frame-1, 0, size, 0);
  for (i = 0; i < size; i ++)
  {
    for (j = 0; j < size; j ++)
    {
      if (frame[j]->gopEncOrder == i)
        break;
    }
    write_asic_cutree_file(m_param, frame[j], base+j);
    m_param->qpOutIdx[m_param->out_cnt+i] = frame[j]->outRoiMapDeltaQpIdx;
  }
}

VCEncRet VCEncCuTreeInit(struct cuTreeCtr *m_param)
{
  VCEncRet ret = VCENC_OK;
  const void *ewl = NULL;
  EWLInitParam_t param;
  asicMemAlloc_s allocCfg;
  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;

  param.clientType = EWL_CLIENT_TYPE_CUTREE;
  /* Init EWL */
  if ((ewl = EWLInit(&param)) == NULL)
  {
    return VCENC_EWL_ERROR;
  }

  m_param->asic.ewl = ewl;
  (void) EncAsicControllerInit(&m_param->asic,param.clientType);

  /* Allocate internal SW/HW shared memories */
  memset(&allocCfg, 0, sizeof (asicMemAlloc_s));
  allocCfg.width = m_param->width; 
  allocCfg.height = m_param->height;
  allocCfg.encodingType = ASIC_CUTREE;
  if (EncAsicMemAlloc_V2(&m_param->asic, &allocCfg) != ENCHW_OK)
  {
    ret = VCENC_EWL_MEMORY_ERROR;
    goto error;
  }
  m_param->cuData_Base = enc->asic.cuInfoMem[0].busAddress + enc->asic.cuInfoTableSize;
  m_param->cuData_frame_size = enc->asic.cuInfoMem[1].busAddress - enc->asic.cuInfoMem[0].busAddress;
  m_param->outRoiMapDeltaQp_Base = m_param->roiMapDeltaQpMemFactory[0].busAddress;
  m_param->outRoiMapDeltaQp_frame_size = m_param->roiMapDeltaQpMemFactory[1].busAddress - m_param->roiMapDeltaQpMemFactory[0].busAddress;
  m_param->inRoiMapDeltaBin_Base = 0;
  m_param->inRoiMapDeltaBin_frame_size = 0;
  {
    //allocate propagate cost memory.
    i32 num_buf = m_param->lookaheadDepth+X265_BFRAME_MAX;
    int block_size= m_param->unitCount * sizeof(uint32_t);
    int i;
    block_size = ((block_size+63)&(~63));
    if (EWLMallocLinear(m_param->asic.ewl, block_size*num_buf,0,&m_param->propagateCostMemFactory[0]) != EWL_OK)
    {
      for(i=0; i< num_buf; i++)
      {
        m_param->propagateCostMemFactory[i].virtualAddress = NULL;
      }

      ret = VCENC_EWL_MEMORY_ERROR;
      goto error;      
    }
    else
    {
      i32 total_size = m_param->propagateCostMemFactory[0].size;
      memset(m_param->propagateCostMemFactory[0].virtualAddress, 0, block_size*num_buf);
      for(i=0; i< num_buf; i++)
      {
        m_param->propagateCostMemFactory[i].virtualAddress = (u32*)((ptr_t)m_param->propagateCostMemFactory[0].virtualAddress + i*block_size);
        m_param->propagateCostMemFactory[i].busAddress = m_param->propagateCostMemFactory[0].busAddress + i*block_size;
        m_param->propagateCostMemFactory[i].size = (i < CUTREE_BUFFER_NUM-1 ? block_size : total_size-(CUTREE_BUFFER_NUM-1)*block_size);
        m_param->propagateCostRefCnt[i] = 0;
      }
    }
    m_param->propagateCost_Base = m_param->propagateCostMemFactory[0].busAddress;
    m_param->propagateCost_frame_size = m_param->propagateCostMemFactory[1].busAddress - m_param->propagateCostMemFactory[0].busAddress;
  }

  return VCENC_OK;

error:
  if (ewl != NULL)
  {
    (void) EWLRelease(ewl);
    m_param->asic.ewl = NULL;
  }

  return ret;

}

void trace_sw_cutree_ctrl_flow(int size, int out_cnt, int pop_cnt, int *qpoutidx);
VCEncRet VCEncCuTreeProcessOneFrame(struct cuTreeCtr *m_param)
{
  VCEncRet ret;
  int i;

  ret = VCEncCuTreeStart(m_param);
  if (ret != VCENC_OK)
    return ret;
  ret = VCEncCuTreeWait(m_param);
  if (ret != VCENC_OK)
    return ret;
  struct Lowres **frames = m_param->lookaheadFrames;
  struct Lowres *out_frame = *m_param->lookaheadFrames;
  i32 idx = 1;
  i32 size = m_param->nLookaheadFrames;
  m_param->out_cnt = m_param->pop_cnt = 0;
  i = 0;
  while(i+1 < m_param->nLookaheadFrames) {
    markBRef(m_param, frames, i, i+frames[i+1]->gopSize, 0);
    i += frames[i+1]->gopSize;
  }
  if(IS_X265_TYPE_I(out_frame->sliceType))
  {
    statisAheadData (m_param, m_param->lookaheadFrames, m_param->nLookaheadFrames-1, HANTRO_TRUE);
    write_asic_gop_cutree(m_param, &out_frame, 1, 0);
    m_param->out_cnt++;
  }
  if(processGopConvert_8to4_asic(m_param, frames))
    idx += 4;
  if(m_param->nLookaheadFrames > 1) {
    int gopSize = (m_param->lookaheadFrames[1]->poc == 0 ? 1 : m_param->lookaheadFrames[1]->gopSize);
    if (!IS_X265_TYPE_I(m_param->lookaheadFrames[1]->sliceType)) {
      statisAheadData (m_param, m_param->lookaheadFrames, m_param->nLookaheadFrames-1, HANTRO_FALSE);
      write_asic_gop_cutree(m_param, m_param->lookaheadFrames + 1, gopSize, 1);
      m_param->out_cnt += gopSize;
    }
    for (i = 0; i < gopSize; i ++)
      remove_one_frame(m_param);
    m_param->pop_cnt += gopSize;
  }
  pthread_mutex_lock(&m_param->cuinfobuf_mutex);
  ASSERT(m_param->cuInfoToRead >= m_param->out_cnt);
  m_param->cuInfoToRead-=m_param->out_cnt;
  pthread_mutex_unlock(&m_param->cuinfobuf_mutex);
  pthread_cond_signal(&m_param->cuinfobuf_cond);
#ifdef TEST_DATA
  trace_sw_cutree_ctrl_flow(size, m_param->out_cnt, m_param->pop_cnt, m_param->qpOutIdx);
#endif
  return VCENC_OK;
}

VCEncRet VCEncCuTreeRelease(struct cuTreeCtr *pEncInst)
{
  const void *ewl;

  ASSERT(pEncInst);
  ewl = pEncInst->asic.ewl;
  if(ewl == NULL)
    return VCENC_OK;
  if(pEncInst->propagateCostMemFactory[0].virtualAddress != NULL)
  {
    EWLFreeLinear(pEncInst->asic.ewl, &pEncInst->propagateCostMemFactory[0]);
    pEncInst->propagateCostMemFactory[0].virtualAddress = NULL;
  }
  if(ewl)
  {
    EncAsicMemFree_V2(&pEncInst->asic);
    (void) EWLRelease(ewl);
  }
  pEncInst->asic.ewl = NULL;
  return VCENC_OK;
}
