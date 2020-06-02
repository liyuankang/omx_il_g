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
--                                                                            --
-  Description : Video stabilization standalone control
-
------------------------------------------------------------------------------*/
#include "base_type.h"
#include "vidstabcommon.h"
#include "vidstabinternal.h"
#include "ewl.h"
#include "encswhwregisters.h"
#include "encasiccontroller.h"

#ifdef ASIC_WAVE_TRACE_TRIGGER
extern i32 trigger_point;    /* picture which will be traced */
#endif

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF

#define HSWREG(n)       ((n)*4)


/*------------------------------------------------------------------------------
    Function name   : VSCheckInput
    Description     :
    Return type     : i32
    Argument        : const VideoStbParam * param
------------------------------------------------------------------------------*/
i32 VSCheckInput(const VideoStbParam * param)
{
    /* Input picture minimum dimensions */
    if ((param->inputWidth < 104) || (param->inputHeight < 104))
        return 1;

    /* Stabilized picture minimum  values */
    if ((param->stabilizedWidth < 96) || (param->stabilizedHeight < 96))
        return 1;

    /* Stabilized dimensions multiple of 4 */
    if (((param->stabilizedWidth & 3) != 0) ||
        ((param->stabilizedHeight & 3) != 0))
        return 1;

    /* ensure edge >= 4 pixels */
    if ((param->inputWidth < (param->stabilizedWidth + 8)) ||
        (param->inputHeight < (param->stabilizedHeight + 8)))
        return 1;

    /* stride 8 multiple */
    if ((param->stride < param->inputWidth) || (param->stride & 7) != 0)
        return 1;

    /* input format */
    if (param->format > VIDEOSTB_BGR101010)
    {
        return 1;
    }

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : VSInitAsicCtrl
    Description     :
    Return type     : void
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
void VSInitAsicCtrl(VideoStb * pVidStab, u32 client_type)
{
    RegValues *val = &pVidStab->regval;
    u32 *regMirror = pVidStab->regMirror;
    ASSERT(pVidStab != NULL);

    /* Initialize default values from defined configuration */
    val->asicCfgReg =
        ((ENCH2_AXI_WRITE_ID & 255) << 24) |
        ((ENCH2_AXI_READ_ID  & 255) << 16) |
        ((ENCH2_OUTPUT_SWAP_64 & 1) << 15) |
        ((ENCH2_OUTPUT_SWAP_32 & 1) << 14) |
        ((ENCH2_OUTPUT_SWAP_16 & 1) << 13) |
        ((ENCH2_OUTPUT_SWAP_8  & 1) << 12);

    val->irqDisable = ENCH2_IRQ_DISABLE;
    val->rwStabMode = ASIC_VS_MODE_ALONE;
    val->asicHwId = EncAsicGetAsicHWid(EncAsicGetCoreIdByFormat(client_type));
    EWLmemset(regMirror, 0, sizeof(regMirror));
}

/*------------------------------------------------------------------------------
    Function name   : VSSetupAsicAll
    Description     :
    Return type     : void
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
void VSSetupAsicAll(VideoStb * pVidStab)
{
    const void *ewl = pVidStab->ewl;
    RegValues *val = &pVidStab->regval;
    u32 *regMirror = pVidStab->regMirror;
    i32 i;

    regMirror[1] |= ((val->irqDisable & 1) << 1);    /* IRQ disable flag */
    regMirror[1] &= ~1;                              /* clear interrupt */


  if (val->inputImageFormat < ASIC_INPUT_RGB565||(val->inputImageFormat >= ASIC_INPUT_DAHUA_HEVC_YUV420&&val->inputImageFormat <= ASIC_INPUT_P010_TILE8)) /* YUV input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_YUV | (ENCH2_INPUT_SWAP_16_YUV << 1) | (ENCH2_INPUT_SWAP_32_YUV << 2) | (ENCH2_INPUT_SWAP_64_YUV << 3);
  else if (val->inputImageFormat < ASIC_INPUT_RGB888) /* 16-bit RGB input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_RGB16 | (ENCH2_INPUT_SWAP_16_RGB16 << 1) | (ENCH2_INPUT_SWAP_32_RGB16 << 2) | (ENCH2_INPUT_SWAP_64_RGB16 << 3);
  else if(val->inputImageFormat < ASIC_INPUT_I010)   /* 32-bit RGB input */
    val->asic_pic_swap = ENCH2_INPUT_SWAP_8_RGB32 | (ENCH2_INPUT_SWAP_16_RGB32 << 1) | (ENCH2_INPUT_SWAP_32_RGB32 << 2) | (ENCH2_INPUT_SWAP_64_RGB32 << 3);
  else if(val->inputImageFormat >= ASIC_INPUT_I010&&val->inputImageFormat <= ASIC_INPUT_PACKED_10BIT_Y0L2)
     val->asic_pic_swap = ENCH2_INPUT_SWAP_8_YUV | (ENCH2_INPUT_SWAP_16_YUV << 1) | (ENCH2_INPUT_SWAP_32_YUV << 2) | (ENCH2_INPUT_SWAP_64_YUV << 3);

  //after H2V4, prp swap changed
#if ENCH2_INPUT_SWAP_64_YUV==1
    if (HW_ID_MAJOR_NUMBER(val->asicHwId) >= 4) /* H2V4+ */
      val->asic_pic_swap = ((~val->asic_pic_swap) & 0xf);
#endif
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_PIC_SWAP, val->asic_pic_swap);

    /* Input picture buffers */
    EncAsicSetAddrRegisterValue(regMirror, HWIF_ENC_INPUT_Y_BASE, val->inputLumBase);

    /* Common control register, use INTRA mode */
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_PIC_WIDTH, (val->mbsInRow*2) & 0x3ff);  /* unit: 8 pixels */
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_PIC_WIDTH_MSB,  ((val->mbsInRow*2) >> 10) & 3);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_PIC_WIDTH_MSB2, ((val->mbsInRow*2) >> 12) & 1);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_PIC_HEIGHT, (val->mbsInCol*2) & 0x7ff);  /* unit: 8 pixels */
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_FRAME_CODING_TYPE, 1);  /* Encoded picture type. frameType. 1=I(intra). */

    /* PreP control */
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_LUMOFFSET, val->inputLumaBaseOffset);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_INPUT_LU_STRIDE, val->pixelsOnRow);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_ROWLENGTH, val->pixelsOnRow);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_XFILL, (val->xFill & 3));
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_YFILL, (val->yFill & 7));
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_XFILL_MSB, (val->xFill >> 2) & 3);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_YFILL_MSB, (val->yFill >> 3) & 3);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_INPUT_FORMAT, val->inputImageFormat);
    EncAsicSetAddrRegisterValue(regMirror, HWIF_ENC_STAB_NEXT_LUMA_BASE, val->rwNextLumaBase);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_STAB_MODE, val->rwStabMode);

    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RGBCOEFFA,
                            val->colorConversionCoeffA & mask_16b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RGBCOEFFB,
                            val->colorConversionCoeffB & mask_16b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RGBCOEFFC,
                            val->colorConversionCoeffC & mask_16b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RGBCOEFFE,
                            val->colorConversionCoeffE & mask_16b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RGBCOEFFF,
                            val->colorConversionCoeffF & mask_16b);

    EncAsicSetRegisterValue(regMirror, HWIF_ENC_RMASKMSB, val->rMaskMsb & mask_5b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_GMASKMSB, val->gMaskMsb & mask_5b);
    EncAsicSetRegisterValue(regMirror, HWIF_ENC_BMASKMSB, val->bMaskMsb & mask_5b);

#ifdef ASIC_WAVE_TRACE_TRIGGER
    if(val->vop_count++ == trigger_point)
    {
        /* logic analyzer triggered by writing to the ID reg */
        EWLWriteReg(ewl, 0x00, ~0);
    }
#endif

    regMirror[HWIF_REG_CFG1] = EWLReadReg(ewl, HWIF_REG_CFG1*4);
    regMirror[HWIF_REG_CFG2] = EWLReadReg(ewl, HWIF_REG_CFG2*4);
    regMirror[HWIF_REG_CFG3] = EWLReadReg(ewl, HWIF_REG_CFG3*4);
    regMirror[HWIF_REG_CFG4] = EWLReadReg(ewl, HWIF_REG_CFG4*4);

    for (i = 1; i < ASIC_SWREG_AMOUNT; i++)
    {
        if (i != 367)  /* Write all regs except standalone stab enable */
        {
            EWLWriteReg(ewl, HSWREG(i), regMirror[i]);
        }
    }

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 0, 0);
#endif

    /* Register with enable bit is written last */
    EWLWriteReg(ewl, HSWREG(367), regMirror[367]);
    regMirror[ASIC_REG_INDEX_STATUS] |= ASIC_STATUS_ENABLE;

    EWLEnableHW(ewl, HSWREG(ASIC_REG_INDEX_STATUS), regMirror[ASIC_REG_INDEX_STATUS]);
}

/*------------------------------------------------------------------------------
    Function name   : CheckAsicStatus
    Description     :
    Return type     : i32
    Argument        : u32 status
------------------------------------------------------------------------------*/
i32 CheckAsicStatus(u32 status)
{
    i32 ret;

    if(status & ASIC_STATUS_HW_RESET)
    {
        ret = VIDEOSTB_HW_RESET;
    }
    else if(status & ASIC_STATUS_FRAME_READY)
    {
        ret = VIDEOSTB_OK;
    }
    else
    {
        ret = VIDEOSTB_HW_BUS_ERROR;
    }

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : VSWaitAsicReady
    Description     :
    Return type     : i32
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
i32 VSWaitAsicReady(VideoStb * pVidStab)
{
    const void *ewl = pVidStab->ewl;
    u32 *regMirror = pVidStab->regMirror;
    i32 ret;
    u32 status = ASIC_STATUS_ERROR;

    /* Wait for IRQ */
    ret = EWLWaitHwRdy(ewl, NULL, 0, &status);

    if(ret != EWL_OK)
    {
        if(ret == EWL_ERROR)
        {
            /* IRQ error => Stop and release HW */
            ret = VIDEOSTB_SYSTEM_ERROR;
        }
        else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
        {
            /* IRQ Timeout => Stop and release HW */
            ret = VIDEOSTB_HW_TIMEOUT;
        }

        EncAsicStop(ewl);       /* make sure ASIC is OFF */
    }
    else
    {
        i32 i;

        regMirror[1] = EWLReadReg(ewl, HSWREG(1));  /* IRQ status */
        for (i = 367; i <= 377; i++)
        {
            regMirror[i] = EWLReadReg(ewl, HSWREG(i));  /* VS results */
        }

        ret = EncAsicGetStatus(ewl);
    }

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 1, 0);
#endif

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : VSSetCropping
    Description     :
    Return type     : void
    Argument        : VideoStb * pVidStab
    Argument        : u32 currentPictBus
    Argument        : u32 nextPictBus
------------------------------------------------------------------------------*/
void VSSetCropping(VideoStb * pVidStab, ptr_t currentPictBus, ptr_t nextPictBus)
{
    u32 byteOffsetCurrent;
    u32 width, height;
    RegValues *regs;

    ASSERT(pVidStab != NULL && currentPictBus != 0 && nextPictBus != 0);

    regs = &pVidStab->regval;

    regs->inputLumBase = currentPictBus;
    regs->rwNextLumaBase = nextPictBus;

    /* RGB to YCbCr conversion coefficients for RGB input; non-full range, Y=0.2989R+0.5866G+0.1145B */
    if (pVidStab->yuvFormat >= ASIC_INPUT_RGB565) {
        regs->colorConversionCoeffA = 19589;
        regs->colorConversionCoeffB = 38443;
        regs->colorConversionCoeffC = 7504;
        regs->colorConversionCoeffE = 37008;
        regs->colorConversionCoeffF = 46740;
    }

    /* Setup masks to separate R, G and B from RGB */
    switch ((i32)pVidStab->yuvFormat)
    {
        case 5: /* RGB565 */
            regs->rMaskMsb = 15;
            regs->gMaskMsb = 10;
            regs->bMaskMsb = 4;
            break;
        case 6: /* BGR565 */
            regs->bMaskMsb = 15;
            regs->gMaskMsb = 10;
            regs->rMaskMsb = 4;
            break;
        case 7: /* RGB555 */
            regs->rMaskMsb = 14;
            regs->gMaskMsb = 9;
            regs->bMaskMsb = 4;
            break;
        case 8: /* BGR555 */
            regs->bMaskMsb = 14;
            regs->gMaskMsb = 9;
            regs->rMaskMsb = 4;
            break;
        case 9: /* RGB444 */
            regs->rMaskMsb = 11;
            regs->gMaskMsb = 7;
            regs->bMaskMsb = 3;
            break;
        case 10: /* BGR444 */
            regs->bMaskMsb = 11;
            regs->gMaskMsb = 7;
            regs->rMaskMsb = 3;
            break;
        case 11: /* RGB888 */
            regs->rMaskMsb = 23;
            regs->gMaskMsb = 15;
            regs->bMaskMsb = 7;
            break;
        case 12: /* BGR888 */
            regs->bMaskMsb = 23;
            regs->gMaskMsb = 15;
            regs->rMaskMsb = 7;
            break;
        case 13: /* RGB101010 */
            regs->rMaskMsb = 29;
            regs->gMaskMsb = 19;
            regs->bMaskMsb = 9;
            break;
        case 14: /* BGR101010 */
            regs->bMaskMsb = 29;
            regs->gMaskMsb = 19;
            regs->rMaskMsb = 9;
            break;
        default:
            /* No masks needed for YUV format */
            regs->rMaskMsb = regs->gMaskMsb = regs->bMaskMsb = 0;
    }

    if (pVidStab->yuvFormat <= 1)
        regs->inputImageFormat = pVidStab->yuvFormat;       /* YUV */
    else if (pVidStab->yuvFormat <= 4)
        regs->inputImageFormat = pVidStab->yuvFormat-1;       /* YUV */
    else if (pVidStab->yuvFormat <= 6)
        regs->inputImageFormat = ASIC_INPUT_RGB565;         /* 16-bit RGB */
    else if (pVidStab->yuvFormat <= 8)
        regs->inputImageFormat = ASIC_INPUT_RGB555;         /* 15-bit RGB */
    else if (pVidStab->yuvFormat <= 10)
        regs->inputImageFormat = ASIC_INPUT_RGB444;         /* 12-bit RGB */
    else if (pVidStab->yuvFormat <= 12)
        regs->inputImageFormat = ASIC_INPUT_RGB888;         /* 24-bit RGB */
    else
        regs->inputImageFormat = ASIC_INPUT_RGB101010;      /* 30-bit RGB */

    regs->pixelsOnRow = pVidStab->stride;

    /* cropping */

    /* Current image position */
    printf("VSSetCropping: stabOffsetX = %d, stabOffsetY = %d, stride = %d\n",
           pVidStab->data.stabOffsetX, pVidStab->data.stabOffsetY, pVidStab->stride);

    byteOffsetCurrent = pVidStab->data.stabOffsetY;
    byteOffsetCurrent *= pVidStab->stride;
    byteOffsetCurrent += pVidStab->data.stabOffsetX;

    printf("VSSetCropping: byteOffsetCurrent = %d\n", byteOffsetCurrent);

    if (pVidStab->yuvFormat >=VIDEOSTB_YUV422_INTERLEAVED_YUYV && pVidStab->yuvFormat <= VIDEOSTB_BGR444)    /* YUV 422 / RGB 16bpp */
    {
        byteOffsetCurrent *= 2;
    }
    else if (pVidStab->yuvFormat > VIDEOSTB_BGR444)    /* RGB 32bpp */
    {
        byteOffsetCurrent *= 4;
    }

    regs->inputLumBase += (byteOffsetCurrent & (~15));  /* crop current picture based on stabilization result */
    regs->inputLumaBaseOffset = (byteOffsetCurrent & 15);

    /* next picture's offset same as above */
    regs->rwNextLumaBase += (byteOffsetCurrent & (~15));

    /* source image setup, size and fill */
    width = pVidStab->data.stabilizedWidth;
    height = pVidStab->data.stabilizedHeight;

    /* Set stabilized picture dimensions */
    regs->mbsInRow = (width + 15) / 16;
    regs->mbsInCol = (height + 15) / 16;

    /* Set the overfill values */
    if (width & 0x0F)
        regs->xFill = (16 - (width & 0x0F)) / 2;
    else
        regs->xFill = 0;

    if (height & 0x0F)
        regs->yFill = 16 - (height & 0x0F);
    else
        regs->yFill = 0;

    return;
}

