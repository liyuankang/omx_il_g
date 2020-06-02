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
#include "encoder.h"
#include "encoder_vp8.h"
#include "util.h"
#include "vp8encapi.h"
#include "OSAL.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX VP8"

#if !defined (ENCH1)
#error "SPECIFY AN ENCODER PRODUCT (ENCH1) IN COMPILER DEFINES!"
#endif

#define REFERENCE_FRAME_AMOUNT 1

typedef struct ENCODER_VP8
{
    ENCODER_PROTOTYPE base;

    VP8EncInst instance;
    VP8EncIn encIn;
    OMX_U32 origWidth;
    OMX_U32 origHeight;
    OMX_U32 outputWidth;
    OMX_U32 outputHeight;
    //OMX_U32 nPFrames;
    OMX_U32 nIFrameCounter;
    OMX_U32 nEstTimeInc;
    OMX_U32 nAllowedPictureTypes;
    OMX_U32 nTotalFrames;
    OMX_BOOL bStabilize;
    OMX_U32 nLayers;
    OMX_U32 nCirCounter;
} ENCODER_VP8;

#ifdef H1V6
/* For temporal layers [0..3] the layer ID and reference picture usage
 * (VP8EncRefPictureMode) for [amount of layers][pic idx] */
OMX_U32 temporalLayerId[4][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0 },  /* 1 layer == base layer only */
  {0, 1, 0, 0, 0, 0, 0, 0 },  /* 2 layers, pattern length = 2 */
  {0, 2, 1, 2, 0, 0, 0, 0 },  /* 3 layers, pattern length = 4 */
  {0, 3, 2, 3, 1, 3, 2, 3 }   /* 4 layers, pattern length = 8 */
};

OMX_U32 temporalLayerIpf[4][8] = {
  {3, 0, 0, 0, 0, 0, 0, 0 },
  {3, 1, 0, 0, 0, 0, 0, 0 },
  {3, 1, 1, 0, 0, 0, 0, 0 },
  {3, 1, 1, 0, 1, 0, 0, 0 }
};

OMX_U32 temporalLayerGrf[4][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0 },
  {0, 0, 0, 0, 0, 0, 0, 0 },
  {0, 0, 2, 1, 0, 0, 0, 0 },
  {0, 0, 0, 0, 2, 1, 1, 0 }
};

OMX_U32 temporalLayerArf[4][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0 },
  {0, 0, 0, 0, 0, 0, 0, 0 },
  {0, 0, 0, 0, 0, 0, 0, 0 },
  {0, 0, 2, 1, 0, 0, 2, 1 }
};

OMX_U32 temporalLayerTicScale[4][4] = {
  { 1, 0, 0, 0 },
  { 2, 2, 0, 0 },
  { 4, 4, 2, 0 },
  { 8, 8, 4, 2 }
};
#endif

// destroy codec instance
static void encoder_destroy_vp8(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_VP8* this = (ENCODER_VP8*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            VP8EncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_vp8(ENCODER_PROTOTYPE* arg,
                                              STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(stream);
    DBGT_EPILOG("");

    return CODEC_OK;
}

static CODEC_STATE encoder_stream_end_vp8(ENCODER_PROTOTYPE* arg,
                                            STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(stream);
    DBGT_EPILOG("");

    return CODEC_OK;
}

static CODEC_STATE encoder_encode_vp8(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);

    ENCODER_VP8* this = (ENCODER_VP8*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VIDEO_ENCODER_CONFIG* encConf = (VIDEO_ENCODER_CONFIG*) cfg;

    VP8EncOut encOut;
    VP8EncRet ret;
    VP8EncRateCtrl rate_ctrl;
    VP8EncCodingCtrl coding_ctrl;
    OMX_U32 i;

    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth
            * this->origHeight);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth
            * this->origHeight / 4);

    this->encIn.timeIncrement = this->nEstTimeInc;
    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;

    DBGT_PDEBUG("Input timeInc: %u outBufSize: %u",
        this->encIn.timeIncrement, this->encIn.outBufSize);

    // The bus address of the luminance component buffer of the picture to be stabilized.
    // Used only when video stabilization is enabled.
#ifdef USE_STAB_TEMP_INPUTBUFFER
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
    }

    if (frame->frame_type == OMX_INTRA_FRAME)
    {
        this->encIn.codingType = VP8ENC_INTRA_FRAME;
        this->nIFrameCounter = 0;
    }
    else if (frame->frame_type == OMX_PREDICTED_FRAME)
    {
        this->encIn.codingType = VP8ENC_PREDICTED_FRAME;
    }
    else
    {
        DBGT_CRITICAL("Unknown frame type");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSUPPORTED_SETTING;
    }

    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    DBGT_PDEBUG("encIn.busLumaStab 0x%08lx", this->encIn.busLumaStab);
    DBGT_PDEBUG("Frame type %s", (this->encIn.codingType == VP8ENC_INTRA_FRAME) ? "I":"P");

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncrement = 0;
    }

    // Set previous frame reference picture mode
    if (encConf->vp8Ref.bPreviousFrameRefresh && encConf->vp8Ref.bUsePreviousFrame)
        this->encIn.ipf = VP8ENC_REFERENCE_AND_REFRESH;
    else if (encConf->vp8Ref.bUsePreviousFrame)
        this->encIn.ipf = VP8ENC_REFERENCE;
    else if (encConf->vp8Ref.bPreviousFrameRefresh)
        this->encIn.ipf = VP8ENC_REFRESH;
    else
        this->encIn.ipf = VP8ENC_NO_REFERENCE_NO_REFRESH;

    // Set Golden frame reference picture mode
    if (encConf->vp8Ref.bGoldenFrameRefresh && encConf->vp8Ref.bUseGoldenFrame)
        this->encIn.grf = VP8ENC_REFERENCE_AND_REFRESH;
    else if (encConf->vp8Ref.bUseGoldenFrame)
        this->encIn.grf = VP8ENC_REFERENCE;
    else if (encConf->vp8Ref.bGoldenFrameRefresh)
        this->encIn.grf = VP8ENC_REFRESH;
    else
        this->encIn.grf = VP8ENC_NO_REFERENCE_NO_REFRESH;

    // Set Alternate frame reference picture mode
    if (encConf->vp8Ref.bAlternateFrameRefresh && encConf->vp8Ref.bUseAlternateFrame)
        this->encIn.arf = VP8ENC_REFERENCE_AND_REFRESH;
    else if (encConf->vp8Ref.bUseAlternateFrame)
        this->encIn.arf = VP8ENC_REFERENCE;
    else if (encConf->vp8Ref.bAlternateFrameRefresh)
        this->encIn.arf = VP8ENC_REFRESH;
    else
        this->encIn.arf = VP8ENC_NO_REFERENCE_NO_REFRESH;

    ret = VP8EncGetRateCtrl(this->instance, &rate_ctrl);

#ifdef H1V6
    if (encConf->temporalLayer.nBaseLayerBitrate)
    {
        this->nLayers = 1;
        if (encConf->temporalLayer.nLayer1Bitrate)
            this->nLayers++;
        if (encConf->temporalLayer.nLayer2Bitrate)
            this->nLayers++;
        if (encConf->temporalLayer.nLayer3Bitrate)
            this->nLayers++;
    }
    else
        this->nLayers = 0;
    DBGT_PDEBUG("Temporal layers %d", (int)this->nLayers);

    if (this->nLayers &&
       ((rate_ctrl.layerBitPerSecond[0] != encConf->temporalLayer.nBaseLayerBitrate) ||
       (rate_ctrl.layerBitPerSecond[1] != encConf->temporalLayer.nLayer1Bitrate) ||
       (rate_ctrl.layerBitPerSecond[2] != encConf->temporalLayer.nLayer2Bitrate) ||
       (rate_ctrl.layerBitPerSecond[3] != encConf->temporalLayer.nLayer3Bitrate)))
    {
        /* Disable golden&altref boost&update */
        rate_ctrl.adaptiveGoldenBoost   = 0;
        rate_ctrl.adaptiveGoldenUpdate  = 0;
        rate_ctrl.goldenPictureBoost    = 0;
        rate_ctrl.goldenPictureRate     = 0;
        rate_ctrl.altrefPictureRate     = 0;

        rate_ctrl.layerBitPerSecond[0] = encConf->temporalLayer.nBaseLayerBitrate;
        rate_ctrl.layerBitPerSecond[1] = encConf->temporalLayer.nLayer1Bitrate;
        rate_ctrl.layerBitPerSecond[2] = encConf->temporalLayer.nLayer2Bitrate;
        rate_ctrl.layerBitPerSecond[3] = encConf->temporalLayer.nLayer3Bitrate;

        /* Set framerates for individual layers, fixed pattern */
        rate_ctrl.layerFrameRateDenom[0] =
            this->nEstTimeInc * temporalLayerTicScale[this->nLayers-1][0];
        rate_ctrl.layerFrameRateDenom[1] =
            this->nEstTimeInc * temporalLayerTicScale[this->nLayers-1][1];
        rate_ctrl.layerFrameRateDenom[2] =
            this->nEstTimeInc * temporalLayerTicScale[this->nLayers-1][2];
        rate_ctrl.layerFrameRateDenom[3] =
            this->nEstTimeInc * temporalLayerTicScale[this->nLayers-1][3];

        DBGT_PDEBUG("Layer rate control: base = %d bps, 1st = %d bps, "
            "2nd = %d bps, 3rd = %d bps",
            rate_ctrl.layerBitPerSecond[0], rate_ctrl.layerBitPerSecond[1],
            rate_ctrl.layerBitPerSecond[2], rate_ctrl.layerBitPerSecond[3]);
        DBGT_PDEBUG("Tics: base = %d, 1st = %d, 2nd = %d, 3rd = %d",
            rate_ctrl.layerFrameRateDenom[0], rate_ctrl.layerFrameRateDenom[1],
            rate_ctrl.layerFrameRateDenom[2], rate_ctrl.layerFrameRateDenom[3]);

        ret = VP8EncSetRateCtrl(this->instance, &rate_ctrl);
    }
#endif

    if (ret == VP8ENC_OK && (rate_ctrl.bitPerSecond != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = VP8EncSetRateCtrl(this->instance, &rate_ctrl);
    }


    ret = VP8EncGetCodingCtrl(this->instance, &coding_ctrl);

    if (ret == VP8ENC_OK)
    {
        if (coding_ctrl.intraArea.top     != encConf->intraArea.nTop ||
            coding_ctrl.intraArea.left    != encConf->intraArea.nLeft ||
            coding_ctrl.intraArea.bottom  != encConf->intraArea.nBottom ||
            coding_ctrl.intraArea.right   != encConf->intraArea.nRight)
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

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);
        }

#ifdef H1V6
        coding_ctrl.adaptiveRoi      = encConf->adaptiveRoi.nAdaptiveROI;
        coding_ctrl.adaptiveRoiColor = encConf->adaptiveRoi.nAdaptiveROIColor;
        DBGT_PDEBUG("Adaptive ROI %d color %d", coding_ctrl.adaptiveRoi,
                coding_ctrl.adaptiveRoiColor);
#endif

        if (coding_ctrl.roi1Area.top     != encConf->roiArea[0].nTop ||
            coding_ctrl.roi1Area.left    != encConf->roiArea[0].nLeft ||
            coding_ctrl.roi1Area.bottom  != encConf->roiArea[0].nBottom ||
            coding_ctrl.roi1Area.right   != encConf->roiArea[0].nRight ||
            coding_ctrl.roi1DeltaQp      != encConf->roiDeltaQP[0].nDeltaQP)
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

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);
        }

        if (coding_ctrl.roi2Area.top     != encConf->roiArea[1].nTop ||
            coding_ctrl.roi2Area.left    != encConf->roiArea[1].nLeft ||
            coding_ctrl.roi2Area.bottom  != encConf->roiArea[1].nBottom ||
            coding_ctrl.roi2Area.right   != encConf->roiArea[1].nRight ||
            coding_ctrl.roi2DeltaQp      != encConf->roiDeltaQP[1].nDeltaQP)
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

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);
        }

        if (encConf->intraRefresh.nCirMBs)
        {
            OMX_U32 macroBlocks = this->outputWidth * this->outputHeight / 256;
            if (encConf->intraRefresh.nCirMBs > macroBlocks)
            {
                DBGT_CRITICAL("VP8EncSetCodingCtrl failed! nCirMBs (%d) > Luma macroBlocks (%d)",
                    (int)encConf->intraRefresh.nCirMBs, (int)macroBlocks);
                DBGT_EPILOG("");
                return CODEC_ERROR_INVALID_ARGUMENT;
            }

            coding_ctrl.cirInterval =
                ((((double) macroBlocks) / ((double) encConf->intraRefresh.nCirMBs)) + 0.5);
            coding_ctrl.cirStart = this->nCirCounter % coding_ctrl.cirInterval;

            DBGT_PDEBUG("nCirMBs %d cirInterval %d cirStart %d",
                (int)encConf->intraRefresh.nCirMBs, (int)coding_ctrl.cirInterval,
                    (int)coding_ctrl.cirStart);

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);

            if (this->encIn.codingType == VP8ENC_PREDICTED_FRAME)
                this->nCirCounter++;
        }
        else if (!encConf->intraRefresh.nCirMBs && this->nCirCounter)
        {
            coding_ctrl.cirInterval = 0;
            this->nCirCounter = 0;

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
    }

#ifdef H1V6
    /* When temporal layers enabled, force references&updates for layers. */
    if (this->nLayers)
    {
        OMX_U32 layerPatternPics = 1 << (this->nLayers-1);
        OMX_U32 picIdx = this->nIFrameCounter % layerPatternPics;
        this->encIn.ipf = temporalLayerIpf[this->nLayers-1][picIdx];
        this->encIn.grf = temporalLayerGrf[this->nLayers-1][picIdx];
        this->encIn.arf = temporalLayerArf[this->nLayers-1][picIdx];
        this->encIn.layerId = temporalLayerId[this->nLayers-1][picIdx];
    }
#endif

    if (ret == VP8ENC_OK)
    {
        ret = VP8EncStrmEncode(this->instance, &this->encIn, &encOut);
    }
    else
    {
        DBGT_CRITICAL("VP8EncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    if (encOut.codingType != VP8ENC_NOTCODED_FRAME)
        this->nIFrameCounter++;

    switch (ret)
    {
        case VP8ENC_FRAME_READY:
            this->nTotalFrames++;
            stream->streamlen = encOut.frameSize;
#ifdef H1V6
            stream->layerId = this->encIn.layerId;
#endif
            // set each partition pointers and sizes
            for(i = 0; i < 9; i++)
            {
                stream->pOutBuf[i] = (OMX_U32 *)encOut.pOutBuf[i];
                stream->streamSize[i] = encOut.streamSize[i];
            }

            if (encOut.codingType == VP8ENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (encOut.codingType == VP8ENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case VP8ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VP8ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VP8ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VP8ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VP8ENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case VP8ENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case VP8ENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case VP8ENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case VP8ENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case VP8ENC_HW_RESERVED:
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
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_vp8(const VP8_CONFIG* params)
{
    DBGT_PROLOG("");
    VP8EncConfig cfg;
    VP8EncApiVersion apiVer;
    VP8EncBuild encBuild;

    memset(&cfg, 0, sizeof(VP8EncConfig));

    cfg.width = params->common_config.nOutputWidth;
    cfg.height = params->common_config.nOutputHeight;
    //cfg.frameRateDenom = 1;
    //cfg.frameRateNum = cfg.frameRateDenom * Q16_FLOAT(params->common_config.nInputFramerate);
    cfg.frameRateNum = TIME_RESOLUTION;
    cfg.frameRateDenom = cfg.frameRateNum / Q16_FLOAT(params->common_config.nInputFramerate);
    cfg.refFrameAmount = REFERENCE_FRAME_AMOUNT;

    ENCODER_VP8* this = OSAL_Malloc(sizeof(ENCODER_VP8));

    this->instance = 0;
    memset(&this->encIn, 0, sizeof(VP8EncIn));
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->outputWidth = params->common_config.nOutputWidth;
    this->outputHeight = params->common_config.nOutputHeight;
    this->base.stream_start = encoder_stream_start_vp8;
    this->base.stream_end = encoder_stream_end_vp8;
    this->base.encode = encoder_encode_vp8;
    this->base.destroy = encoder_destroy_vp8;

    this->bStabilize = params->pp_config.frameStabilization;
    this->nIFrameCounter = 0;
    this->nEstTimeInc = cfg.frameRateDenom;
    this->nTotalFrames = 0;
    this->nCirCounter = 0;

    apiVer = VP8EncGetApiVersion();
    encBuild = VP8EncGetBuild();

    DBGT_PDEBUG("VP8 Encoder API version %d.%d", apiVer.major,
            apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    VP8EncRet ret = VP8EncInit(&cfg, &this->instance);

    // Setup coding control
    if (ret == VP8ENC_OK)
    {
        VP8EncCodingCtrl coding_ctrl;
        memset(&coding_ctrl, 0, sizeof(VP8EncCodingCtrl));

        ret = VP8EncGetCodingCtrl(this->instance, &coding_ctrl);

        if (ret == VP8ENC_OK)
        {
            coding_ctrl.filterLevel = VP8ENC_FILTER_LEVEL_AUTO;
            coding_ctrl.filterSharpness = VP8ENC_FILTER_SHARPNESS_AUTO;
            coding_ctrl.filterType = 0;

            switch (params->vp8_config.eLevel)
            {
                case OMX_VIDEO_VP8Level_Version0:
                    coding_ctrl.interpolationFilter = 0;
                    break;
                case OMX_VIDEO_VP8Level_Version1:
                    coding_ctrl.interpolationFilter = 1;
                    coding_ctrl.filterType = 1;
                    break;
                case OMX_VIDEO_VP8Level_Version2:
                    coding_ctrl.interpolationFilter = 1;
                    coding_ctrl.filterLevel = 0;
                    break;
                case OMX_VIDEO_VP8Level_Version3:
                    coding_ctrl.interpolationFilter = 2;
                    coding_ctrl.filterLevel = 0;
                    break;
                default:
                    DBGT_CRITICAL("Invalid VP8 eLevel");
                    coding_ctrl.interpolationFilter = 0;
                    break;
            }

            coding_ctrl.dctPartitions = params->vp8_config.nDCTPartitions;
            coding_ctrl.errorResilient = params->vp8_config.bErrorResilientMode;
            coding_ctrl.quarterPixelMv = 1;

            ret = VP8EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
        else
        {
            DBGT_CRITICAL("VP8EncGetCodingCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("VP8EncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    if (ret == VP8ENC_OK)
    {
        VP8EncRateCtrl rate_ctrl;
        memset(&rate_ctrl, 0, sizeof(VP8EncRateCtrl));

        ret = VP8EncGetRateCtrl(this->instance, &rate_ctrl);

        if (ret == VP8ENC_OK)
        {
            // Optional. Set to -1 to use default.
            if (params->rate_config.nPictureRcEnabled >= 0)
            {
                rate_ctrl.pictureRc = params->rate_config.nPictureRcEnabled;
            }

            // Optional settings. Set to -1 to use default.
            if (params->rate_config.nQpDefault >= 0)
            {
                rate_ctrl.qpHdr = params->rate_config.nQpDefault;
            }

            // Optional settings. Set to -1 to use default.
            if (params->rate_config.nQpMin >= 0)
            {
                rate_ctrl.qpMin = params->rate_config.nQpMin;
                if (rate_ctrl.qpHdr != -1 && rate_ctrl.qpHdr < (OMX_S32)rate_ctrl.qpMin)
                {
                    rate_ctrl.qpHdr = rate_ctrl.qpMin;
                }
            }

            // Optional settings. Set to -1 to use default.
            if (params->rate_config.nQpMax > 0)
            {
                rate_ctrl.qpMax = params->rate_config.nQpMax;
                if (rate_ctrl.qpHdr > (OMX_S32)rate_ctrl.qpMax)
                {
                    rate_ctrl.qpHdr = rate_ctrl.qpMax;
                }
            }

            // Optional. Set to -1 to use default.
            if (params->rate_config.nTargetBitrate >= 0)
            {
                rate_ctrl.bitPerSecond = params->rate_config.nTargetBitrate;
            }

            switch (params->rate_config.eRateControl)
            {
                case OMX_Video_ControlRateDisable:
                    rate_ctrl.pictureSkip = 0;
                    break;
                case OMX_Video_ControlRateVariable:
                    rate_ctrl.pictureSkip = 0;
                    break;
                case OMX_Video_ControlRateConstant:
                    rate_ctrl.pictureSkip = 0;
                    break;
                case OMX_Video_ControlRateVariableSkipFrames:
                    rate_ctrl.pictureSkip = 1;
                    break;
                case OMX_Video_ControlRateConstantSkipFrames:
                    rate_ctrl.pictureSkip = 1;
                    break;
                case OMX_Video_ControlRateMax:
                    rate_ctrl.pictureSkip = 0;
                    break;
                default:
                    break;
            }

            ret = VP8EncSetRateCtrl(this->instance, &rate_ctrl);
        }
        else
        {
            DBGT_CRITICAL("VP8EncGetRateCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("VP8EncSetCodingCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    if (ret == VP8ENC_OK)
    {
        VP8EncPreProcessingCfg pp_config;
        memset(&pp_config, 0, sizeof(VP8EncPreProcessingCfg));

        ret = VP8EncGetPreProcessing(this->instance, &pp_config);

        if (ret != VP8ENC_OK)
        {
            DBGT_CRITICAL("VP8EncGetPreProcessing failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }

        // input image size
        pp_config.origWidth = params->pp_config.origWidth;
        pp_config.origHeight = params->pp_config.origHeight;

        // cropping offset
        pp_config.xOffset = params->pp_config.xOffset;
        pp_config.yOffset = params->pp_config.yOffset;

        switch (params->pp_config.formatType)
        {
            case OMX_COLOR_FormatYUV420PackedPlanar:
            case OMX_COLOR_FormatYUV420Planar:
                pp_config.inputType = VP8ENC_YUV420_PLANAR;
                break;
            case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
                pp_config.inputType = VP8ENC_YUV420_SEMIPLANAR;
                break;
            case OMX_COLOR_FormatYCbYCr:
                pp_config.inputType = VP8ENC_YUV422_INTERLEAVED_YUYV;
                break;
            case OMX_COLOR_FormatCbYCrY:
                pp_config.inputType = VP8ENC_YUV422_INTERLEAVED_UYVY;
                break;
            case OMX_COLOR_Format16bitRGB565:
                pp_config.inputType = VP8ENC_RGB565;
                break;
            case OMX_COLOR_Format16bitBGR565:
                pp_config.inputType = VP8ENC_BGR565;
                break;
            case OMX_COLOR_Format12bitRGB444:
                pp_config.inputType = VP8ENC_RGB444;
                break;
            case OMX_COLOR_Format16bitARGB4444:
                pp_config.inputType = VP8ENC_RGB444;
                break;
            case OMX_COLOR_Format16bitARGB1555:
                pp_config.inputType = VP8ENC_RGB555;
                break;
            case OMX_COLOR_Format25bitARGB1888:
            case OMX_COLOR_Format32bitARGB8888:
                pp_config.inputType = VP8ENC_RGB888;
                break;
            case OMX_COLOR_Format32bitBGRA8888:
                pp_config.inputType = VP8ENC_BGR888;
                break;
            default:
                DBGT_CRITICAL("Unknown color format");
                ret = VP8ENC_INVALID_ARGUMENT;
                break;
        }

        switch (params->pp_config.angle)
        {
            case 0:
                pp_config.rotation = VP8ENC_ROTATE_0;
                break;
            case 90:
                pp_config.rotation = VP8ENC_ROTATE_90R;
                break;
            case 270:
                pp_config.rotation = VP8ENC_ROTATE_90L;
                break;
            default:
                DBGT_CRITICAL("Unsupported rotation angle");
                ret = VP8ENC_INVALID_ARGUMENT;
                break;
        }

        // Enables or disables the video stabilization function. Set to a non-zero value will
        // enable the stabilization. The input image dimensions (origWidth, origHeight)
        // have to be at least 8 pixels bigger than the final encoded image dimensions. Also when
        // enabled the cropping offset (xOffset, yOffset) values are ignored.
        this->bStabilize = params->pp_config.frameStabilization;
        pp_config.videoStabilization = params->pp_config.frameStabilization;

        if (ret == VP8ENC_OK)
        {
            ret = VP8EncSetPreProcessing(this->instance, &pp_config);
        }
    }
    else
    {
        DBGT_CRITICAL("VP8EncSetRateCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (ret != VP8ENC_OK)
    {
        DBGT_CRITICAL("VP8EncSetPreProcessing failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_vp8(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_VP8* this = (ENCODER_VP8*)arg;

    this->nEstTimeInc = (OMX_U32) (TIME_RESOLUTION / Q16_FLOAT(xFramerate));

    DBGT_EPILOG("");
    return CODEC_OK;
}
