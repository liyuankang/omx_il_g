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
-  Description : Standalone stabilization internal stuff
-
------------------------------------------------------------------------------*/
#ifndef __VIDSTBINTERNAL_H__
#define __VIDSTBINTERNAL_H__

#include "base_type.h"
#include "vidstabcommon.h"
#include "vidstbapi.h"
#include "ewl.h"

#define ASIC_STATUS_ENABLE              0x001

#define ASIC_VS_MODE_OFF                0x00
#define ASIC_VS_MODE_ALONE              0x01
#define ASIC_VS_MODE_ENCODER            0x02

#define ASIC_INPUT_YUV420PLANAR         0x00
#define ASIC_INPUT_YUV420SEMIPLANAR     0x01
#define ASIC_INPUT_YUYV422INTERLEAVED   0x02
#define ASIC_INPUT_UYVY422INTERLEAVED   0x03
#define ASIC_INPUT_RGB565               0x04
#define ASIC_INPUT_RGB555               0x05
#define ASIC_INPUT_RGB444               0x06
#define ASIC_INPUT_RGB888               0x07
#define ASIC_INPUT_RGB101010            0x08

typedef struct RegValues_
{
    u32 irqDisable;
    u32 mbsInCol;
    u32 mbsInRow;
    u32 pixelsOnRow;
    u32 xFill;
    u32 yFill;
    u32 inputImageFormat;
    ptr_t inputLumBase;
    u32 inputLumaBaseOffset;
    ptr_t rwNextLumaBase;
    u32 rwStabMode;

    u32 rMaskMsb;
    u32 gMaskMsb;
    u32 bMaskMsb;
    u32 colorConversionCoeffA;
    u32 colorConversionCoeffB;
    u32 colorConversionCoeffC;
    u32 colorConversionCoeffE;
    u32 colorConversionCoeffF;

    u32 asic_pic_swap;

    HWStabData hwStabData;

    u32 asicCfgReg;

    u32 asicHwId;

#ifdef ASIC_WAVE_TRACE_TRIGGER
    u32 vop_count;
#endif
} RegValues;

typedef u32 SwStbMotionType;

typedef struct VideoStb_
{
    const void *ewl;
    u32 reserve_core_info;  /* for multi-core */
    u32 regMirror[479];
    const void *checksum;
    SwStbData data;
    RegValues regval;
    u32 stride;
    VideoStbInputFormat yuvFormat;
} VideoStb;

void VSSetCropping(VideoStb * pVidStab, ptr_t currentPictBus, ptr_t nextPictBus);

void VSInitAsicCtrl(VideoStb * pVidStab, u32 client_type);
i32 VSCheckInput(const VideoStbParam * param);
void VSSetupAsicAll(VideoStb * pVidStab);
i32 VSWaitAsicReady(VideoStb * pVidStab);

#endif /* __VIDSTBINTERNAL_H__ */
