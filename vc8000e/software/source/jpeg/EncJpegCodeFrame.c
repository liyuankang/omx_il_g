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
--  Abstract  :    JPEG Code Frame control
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "ewl.h"

#include "EncJpegCodeFrame.h"
#include "encdec400.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void NextHeader(stream_s * stream, jpegData_s * jpeg);
static void EndRi(stream_s * stream, jpegData_s * jpeg);
static void jpegSetNewFrame(jpegInstance_s * inst);

/*------------------------------------------------------------------------------

    EncJpegCodeFrame

------------------------------------------------------------------------------*/
jpegEncodeFrame_e EncJpegCodeFrameRun(jpegInstance_s * inst)
{
    jpegEncodeFrame_e ret;

    /* set output stream start point in case of whole encoding mode
     * and no rst used */
    if(inst->stream.byteCnt == 0)
    {
        inst->jpeg.streamStartAddress = inst->stream.stream;
    }

    /* set new frame encoding parameters */
    jpegSetNewFrame(inst);

    /* start hw encoding */
    EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs, 0);

    return JPEGENCODE_OK;
}

jpegEncodeFrame_e EncJpegCodeFrameWait(jpegInstance_s * inst)
{
    jpegEncodeFrame_e ret;
    asicData_s *asic = &inst->asic;
    u32 status = ASIC_STATUS_ERROR;

    do {
        /* Encode one frame */
        i32 ewl_ret;

#ifndef JPEG_SIMULATION
        /* Wait for IRQ */
        ewl_ret = EWLWaitHwRdy(asic->ewl, NULL, 0, &status);
        if (inst->dec400Enable == 1)
          VCEncDisableDec400(asic->ewl);
#else
        return JPEGENCODE_OK;
#endif
        if(ewl_ret != EWL_OK)
        {
            status = ASIC_STATUS_ERROR;

            if(ewl_ret == EWL_ERROR)
            {
                /* IRQ error => Stop and release HW */
                ret = JPEGENCODE_SYSTEM_ERROR;
            }
            else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
            {
                /* IRQ Timeout => Stop and release HW */
                ret = JPEGENCODE_TIMEOUT;
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
                ret = JPEGENCODE_HW_ERROR;
                break;
            case ASIC_STATUS_BUFF_FULL:
                EWLReleaseHw(asic->ewl);
                ret = JPEGENCODE_OK;
                inst->stream.overflow = ENCHW_YES;
                break;
            case ASIC_STATUS_HW_RESET:
                EWLReleaseHw(asic->ewl);
                ret = JPEGENCODE_HW_RESET;
                break;
            case ASIC_STATUS_HW_TIMEOUT:
                EWLReleaseHw(asic->ewl);
                ret = JPEGENCODE_TIMEOUT;
                break;
            case ASIC_STATUS_FRAME_READY:
                inst->stream.byteCnt -= (asic->regs.firstFreeBit/8);    /* last not full 64-bit counted in HW data */
                inst->stream.byteCnt += asic->regs.outputStrmSize[0];
                ret = JPEGENCODE_OK;
                EWLReleaseHw(asic->ewl);
                break;
            case ASIC_STATUS_LINE_BUFFER_DONE:
                ret = JPEGENCODE_OK;
                /* SW handshaking: Software will clear the line buffer interrupt and then update the 
                           *   line buffer write pointer, when the next line buffer is ready. The encoder will
                           *   continue to run when detected the write pointer is updated.  */
                if (!inst->inputLineBuf.inputLineBufHwModeEn)
                {
                    if (inst->inputLineBuf.cbFunc)
                        inst->inputLineBuf.cbFunc(inst->inputLineBuf.cbData);
                }
                break;
            case ASIC_STATUS_SEGMENT_READY:
                ret = JPEGENCODE_OK;
                          
                while(inst->streamMultiSegment.streamMultiSegmentMode != 0 &&
                      inst->streamMultiSegment.rdCnt < EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STRM_SEGMENT_WR_PTR))
                {
                  if (inst->streamMultiSegment.cbFunc)
                    inst->streamMultiSegment.cbFunc(inst->streamMultiSegment.cbData);
                  /*note: must make sure the data of one segment is read by app then rd counter can increase*/
                  inst->streamMultiSegment.rdCnt++;
                }
                EncAsicWriteRegisterValue(asic->ewl, asic->regs.regMirror, HWIF_ENC_STRM_SEGMENT_RD_PTR,inst->streamMultiSegment.rdCnt);
                break;
            default:
                /* should never get here */
                ASSERT(0);
                ret = JPEGENCODE_HW_ERROR;
            }
        }
    }while (status == ASIC_STATUS_LINE_BUFFER_DONE || status == ASIC_STATUS_SEGMENT_READY);

    /* Handle EOI */
    if(ret == JPEGENCODE_OK)
    {
        /* update mcu count */
        if(inst->jpeg.codingType == ENC_PARTIAL_FRAME)
        {
            if (inst->jpeg.losslessEn)
            {
                u32  rstMbs = (inst->jpeg.width + 15) / 16 * inst->jpeg.rstMbRows;

                if((inst->jpeg.mbNum + rstMbs) <
                   ((u32) inst->jpeg.mbPerFrame))
                {
                    inst->jpeg.mbNum += rstMbs;
                    inst->jpeg.row += inst->jpeg.sliceRows;
                }
                else
                {
                    inst->jpeg.mbNum += (inst->jpeg.mbPerFrame - inst->jpeg.mbNum);
                }
            }
            else
            {
                u32  rstMbs = (inst->jpeg.width + 15) / 16 * inst->jpeg.rstMbRows;

                if((inst->jpeg.mbNum + rstMbs) <
                   ((u32) inst->jpeg.mbPerFrame))
                {
                    inst->jpeg.mbNum += rstMbs;
                    inst->jpeg.row += inst->jpeg.sliceRows;
                }
                else
                {
                    inst->jpeg.mbNum += (inst->jpeg.mbPerFrame - inst->jpeg.mbNum);
                }
            }
        }
        else
        {
            inst->jpeg.mbNum += inst->jpeg.mbPerFrame;
        }

        EndRi(&inst->stream, &inst->jpeg);
    }

    return ret;
}

/*------------------------------------------------------------------------------

    Write the header data (frame header or restart marker) to the stream. 
    
------------------------------------------------------------------------------*/
void NextHeader(stream_s * stream, jpegData_s * jpeg)
{
    if(jpeg->mbNum == 0)
    {
        (void) EncJpegHdr(stream, jpeg);
    }
}

/*------------------------------------------------------------------------------

    Write the end of current coding unit (RI / FRAME) into stream.
    
------------------------------------------------------------------------------*/
void EndRi(stream_s * stream, jpegData_s * jpeg)
{
    /* not needed anymore, ASIC generates EOI marker */
}

/*------------------------------------------------------------------------------

    Set encoding parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
void jpegSetNewFrame(jpegInstance_s * inst)
{
    regValues_s *regs = &inst->asic.regs;
    u8 offsetTo8BytesAlignment = 0;
    ptr_t strmBaseTmp = regs->outputStrmBase[0];
  
    /* Write next header if needed */
    NextHeader(&inst->stream, &inst->jpeg);

    offsetTo8BytesAlignment = (u8)((regs->outputStrmBase[0] + inst->stream.byteCnt) & 0x07);

    /* calculate output start point for hw */
    regs->outputStrmSize[0] -= (inst->stream.byteCnt - offsetTo8BytesAlignment);
    inst->invalidBytesInBuf0Tail = regs->outputStrmSize[0] & 0x07;
    regs->outputStrmSize[0] &= (~0x07);    /* 8 multiple size */

    /* 64-bit aligned stream base address */
    regs->outputStrmBase[0] = ((regs->outputStrmBase[0] + inst->stream.byteCnt) & (~0x07));
    /* bit offset in the last 64-bit word */
    regs->firstFreeBit = (offsetTo8BytesAlignment) * 8;
    regs->jpegHeaderLength = (u32)(regs->outputStrmBase[0] - strmBaseTmp);
    hash(&inst->jpeg.hashctx, inst->jpeg.streamStartAddress, (inst->stream.byteCnt - offsetTo8BytesAlignment));
    regs->hashtype = inst->jpeg.hashctx.hash_type;
    hash_getstate(&inst->jpeg.hashctx, &regs->hashval, &regs->hashoffset);

    /* header remainder is byte aligned, max 7 bytes = 56 bits */
    if(regs->firstFreeBit != 0)
    {
        /* 64-bit aligned stream pointer */
        u8 *pTmp = (u8 *) ((size_t) (inst->stream.stream) & (~0x07));
        u32 val;

        /* Clear remaining bits */
        for (val = 6; val >= regs->firstFreeBit/8; val--)
            pTmp[val] = 0;

        val = pTmp[0] << 24;
        val |= pTmp[1] << 16;
        val |= pTmp[2] << 8;
        val |= pTmp[3];

        regs->strmStartMSB = val;  /* 32 bits to MSB */

        if(regs->firstFreeBit > 32)
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

    /* low latency: configure related register.*/
    regs->lineBufferEn = inst->inputLineBuf.inputLineBufEn;
    regs->lineBufferHwHandShake = inst->inputLineBuf.inputLineBufHwModeEn;
    regs->lineBufferLoopBackEn = inst->inputLineBuf.inputLineBufLoopBackEn;
    regs->lineBufferDepth = inst->inputLineBuf.inputLineBufDepth;
    regs->amountPerLoopBack = inst->inputLineBuf.amountPerLoopBack;
    regs->mbWrPtr = inst->inputLineBuf.wrCnt;
    regs->mbRdPtr = 0;
    regs->lineBufferInterruptEn = ENCH2_INPUT_BUFFER_INTERRUPT &
                                  regs->lineBufferEn &
                                  (regs->lineBufferHwHandShake == 0) &
                                  (regs->lineBufferDepth > 0);
}
