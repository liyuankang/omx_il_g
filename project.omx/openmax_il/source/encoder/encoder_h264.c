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
#include "encoder_h264.h"
#include "util.h"
#include "h264encapi.h"
#include "OSAL.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX H264"

#if !defined (ENC6280) && !defined (ENC7280) && !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1)
#error "SPECIFY AN ENCODER PRODUCT (ENC6280, ENC7280, ENC8270, ENC8290 OR ENCH1) IN COMPILER DEFINES!"
#endif

typedef struct ENCODER_H264
{
    ENCODER_PROTOTYPE base;

    H264EncInst instance;
    H264EncIn encIn;
    OMX_U32 origWidth;
    OMX_U32 origHeight;
    OMX_U32 outputWidth;
    OMX_U32 outputHeight;
    OMX_U32 nEstTimeInc;
    OMX_U32 bStabilize;
    OMX_U32 nIFrameCounter;
    OMX_U32 nPFrames;
    OMX_U32 nTotalFrames;
    OMX_U32 nCirCounter;
} ENCODER_H264;

// destroy codec instance
static void encoder_destroy_h264(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_H264* this = (ENCODER_H264*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            H264EncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_h264(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_H264* this = (ENCODER_H264*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;
    this->nTotalFrames = 0;

    H264EncOut encOut;
    H264EncRet ret = H264EncStrmStart(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case H264ENC_OK:
            stream->streamlen = encOut.streamSize;
            stat = CODEC_OK;
            break;
        case H264ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case H264ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case H264ENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_stream_end_h264(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_H264* this = (ENCODER_H264*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;

    H264EncOut encOut;
    H264EncRet ret = H264EncStrmEnd(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case H264ENC_OK:
            stream->streamlen = encOut.streamSize;
            stat = CODEC_OK;
            break;
        case H264ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case H264ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_encode_h264(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);

    ENCODER_H264* this = (ENCODER_H264*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VIDEO_ENCODER_CONFIG* encConf = (VIDEO_ENCODER_CONFIG*) cfg;

    H264EncRet ret;
    H264EncOut encOut;
    H264EncRateCtrl rate_ctrl;
    H264EncCodingCtrl coding_ctrl;

    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth
            * this->origHeight);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth
            * this->origHeight / 4);

    this->encIn.timeIncrement = this->nEstTimeInc;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;
#if !defined (ENC8290) && !defined (ENCH1)
    this->encIn.pNaluSizeBuf = NULL;
    this->encIn.naluSizeBufSize = 0;
#endif
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
        this->encIn.codingType = H264ENC_INTRA_FRAME;
        if (this->nPFrames > 0)
            this->nIFrameCounter++;
     }
    else if (frame->frame_type == OMX_PREDICTED_FRAME)
    {
        if (this->nPFrames > 0)
        {
            if ((this->nIFrameCounter % this->nPFrames) == 0)
            {
                this->encIn.codingType = H264ENC_INTRA_FRAME;
                this->nIFrameCounter = 0;
            }
            else
            {
                this->encIn.codingType = H264ENC_PREDICTED_FRAME;
            }
            this->nIFrameCounter++;
        }
        else
        {
            this->encIn.codingType = H264ENC_INTRA_FRAME;
        }
    }
    else
    {
        DBGT_CRITICAL("Unknown frame type");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSUPPORTED_SETTING;
    }
    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    DBGT_PDEBUG("encIn.busLumaStab 0x%08lx", this->encIn.busLumaStab);
    DBGT_PDEBUG("Frame type %s, IFrame counter (%d/%d)", (this->encIn.codingType == H264ENC_INTRA_FRAME) ? "I":"P",
        (int)this->nIFrameCounter, (int)this->nPFrames);

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncrement = 0;
    }

#ifdef H1V6
    /* This applies for PREDICTED frames only. By default always predict
     * from all available frames and refresh last frame */
    this->encIn.ipf = H264ENC_REFERENCE_AND_REFRESH;
    this->encIn.ltrf = H264ENC_REFERENCE;
#endif

    ret = H264EncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == H264ENC_OK && (rate_ctrl.bitPerSecond != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = H264EncSetRateCtrl(this->instance, &rate_ctrl);
    }

    DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
    DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);

#if defined (ENC8290) || defined (ENCH1)
    ret = H264EncGetCodingCtrl(this->instance, &coding_ctrl);

    if (ret == H264ENC_OK)
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

            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
        }

#ifdef H1V6
        if (coding_ctrl.adaptiveRoi      != encConf->adaptiveRoi.nAdaptiveROI ||
            coding_ctrl.adaptiveRoiColor != encConf->adaptiveRoi.nAdaptiveROIColor)
        {
            coding_ctrl.adaptiveRoi      = encConf->adaptiveRoi.nAdaptiveROI;
            coding_ctrl.adaptiveRoiColor = encConf->adaptiveRoi.nAdaptiveROIColor;
            DBGT_PDEBUG("Adaptive ROI %d color %d", coding_ctrl.adaptiveRoi,
                    coding_ctrl.adaptiveRoiColor);
            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);

        }

        if (coding_ctrl.idrHeader        != encConf->prependSPSPPSToIDRFrames)
        {
            coding_ctrl.idrHeader        = encConf->prependSPSPPSToIDRFrames;
            DBGT_PDEBUG("IdrHeader %d", coding_ctrl.idrHeader);
            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
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

            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
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

            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
        }

        if (encConf->intraRefresh.nCirMBs)
        {
            OMX_U32 macroBlocks = this->outputWidth * this->outputHeight / 256;
            if (encConf->intraRefresh.nCirMBs > macroBlocks)
            {
                DBGT_CRITICAL("H264EncSetCodingCtrl failed! nCirMBs (%d) > Luma macroBlocks (%d)",
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

            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);

            if (this->encIn.codingType == H264ENC_PREDICTED_FRAME)
                this->nCirCounter++;
        }
        else if (!encConf->intraRefresh.nCirMBs && this->nCirCounter)
        {
            coding_ctrl.cirInterval = 0;
            this->nCirCounter = 0;

            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
    }
#endif

    if (ret == H264ENC_OK)
    {
#if !defined (ENC8290) && !defined (ENCH1)
        ret = H264EncStrmEncode(this->instance, &this->encIn, &encOut);
#else
        ret = H264EncStrmEncode(this->instance, &this->encIn, &encOut, NULL, NULL, NULL);
#endif
    }
    else
    {
        DBGT_CRITICAL("H264EncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case H264ENC_FRAME_READY:
            this->nTotalFrames++;
            stream->streamlen = encOut.streamSize;

            if (encOut.codingType == H264ENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (encOut.codingType == H264ENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else if (encOut.codingType == H264ENC_NOTCODED_FRAME)
            {
                DBGT_PDEBUG("encOut: Not coded frame");
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case H264ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case H264ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case H264ENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case H264ENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case H264ENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case H264ENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case H264ENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case H264ENC_HW_RESERVED:
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
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_h264(const H264_CONFIG* params)
{
    DBGT_PROLOG("");
    H264EncConfig cfg;
    H264EncApiVersion apiVer;
    H264EncBuild encBuild;

    memset(&cfg, 0, sizeof(H264EncConfig));

#if defined (ENC8290) || defined (ENCH1)
    if ((params->h264_config.eProfile != OMX_VIDEO_AVCProfileBaseline) &&
        (params->h264_config.eProfile != OMX_VIDEO_AVCProfileMain) &&
        (params->h264_config.eProfile != OMX_VIDEO_AVCProfileHigh))
    {
        DBGT_CRITICAL("Profile not supported (requested profile: %d)", params->h264_config.eProfile);
        DBGT_EPILOG("");
        return NULL;
    }
#else
    if (params->h264_config.eProfile != OMX_VIDEO_AVCProfileBaseline)
    {
        DBGT_CRITICAL("Only baseline profile is supported (requested profile: %d)", params->h264_config.eProfile);
        DBGT_EPILOG("");
        return NULL;
    }
#endif

    switch (params->h264_config.eLevel)
    {
#if defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
        case OMX_VIDEO_AVCLevel1:
            cfg.level = H264ENC_LEVEL_1;
            break;
        case OMX_VIDEO_AVCLevel1b:
            cfg.level = H264ENC_LEVEL_1_b;
            break;
        case OMX_VIDEO_AVCLevel11:
            cfg.level = H264ENC_LEVEL_1_1;
            break;
        case OMX_VIDEO_AVCLevel12:
            cfg.level = H264ENC_LEVEL_1_2;
            break;
        case OMX_VIDEO_AVCLevel13:
            cfg.level = H264ENC_LEVEL_1_3;
            break;
        case OMX_VIDEO_AVCLevel2:
            cfg.level = H264ENC_LEVEL_2;
            break;
        case OMX_VIDEO_AVCLevel21:
            cfg.level = H264ENC_LEVEL_2_1;
            break;
        case OMX_VIDEO_AVCLevel22:
            cfg.level = H264ENC_LEVEL_2_2;
            break;
        case OMX_VIDEO_AVCLevel3:
            cfg.level = H264ENC_LEVEL_3;
            break;
        case OMX_VIDEO_AVCLevel31:
            cfg.level = H264ENC_LEVEL_3_1;
            break;
        case OMX_VIDEO_AVCLevel32:
            cfg.level = H264ENC_LEVEL_3_2;
            break;
        case OMX_VIDEO_AVCLevel4:
#ifdef ENC8270
            cfg.level = H264ENC_LEVEL_4_0;
#else
            cfg.level = H264ENC_LEVEL_4;
#endif
            break;

#if defined (ENC8290) || defined (ENCH1)
        case OMX_VIDEO_AVCLevel41:
            cfg.level = H264ENC_LEVEL_4_1;
            break;
        case OMX_VIDEO_AVCLevel42:
            cfg.level = H264ENC_LEVEL_4_2;
            break;
        case OMX_VIDEO_AVCLevel5:
            cfg.level = H264ENC_LEVEL_5;
            break;
        case OMX_VIDEO_AVCLevel51:
        case OMX_VIDEO_AVCLevelMax:
            cfg.level = H264ENC_LEVEL_5_1;
            break;
#else

        case OMX_VIDEO_AVCLevel41:
        case OMX_VIDEO_AVCLevelMax:
            cfg.level = H264ENC_LEVEL_4_1;
            break;
#endif

#else
        case OMX_VIDEO_AVCLevel1:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_1;
            break;
        case OMX_VIDEO_AVCLevel1b:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_1_b;
            break;
        case OMX_VIDEO_AVCLevel11:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_1_1;
            break;
        case OMX_VIDEO_AVCLevel12:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_1_2;
            break;
        case OMX_VIDEO_AVCLevel13:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_1_3;
            break;
        case OMX_VIDEO_AVCLevel2:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_2;
            break;
        case OMX_VIDEO_AVCLevel21:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_2_1;
            break;
        case OMX_VIDEO_AVCLevel22:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_2_2;
            break;
        case OMX_VIDEO_AVCLevel3:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_3;
            break;
        case OMX_VIDEO_AVCLevel31:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_3_1;
            break;
        case OMX_VIDEO_AVCLevel32:
        case OMX_VIDEO_AVCLevelMax:
            cfg.profileAndLevel = H264ENC_BASELINE_LEVEL_3_2;
            break;
#endif
        default:
            DBGT_CRITICAL("Unsupported encoding level %d", params->h264_config.eLevel);
            DBGT_EPILOG("");
            return NULL;
            break;
    }


    cfg.streamType = H264ENC_BYTE_STREAM;
    cfg.width = params->common_config.nOutputWidth;
    cfg.height = params->common_config.nOutputHeight;
    DBGT_PDEBUG("OutputWidth: %d", (int)cfg.width);
    DBGT_PDEBUG("OutputHeight: %d", (int)cfg.height);
    //cfg.frameRateDenom = 1;
    //cfg.frameRateNum = cfg.frameRateDenom * Q16_FLOAT(params->common_config.nInputFramerate);

    cfg.frameRateNum = TIME_RESOLUTION;
    cfg.frameRateDenom = cfg.frameRateNum / Q16_FLOAT(params->common_config.nInputFramerate);

#if defined (ENC8290) || defined (ENCH1)
    cfg.viewMode = H264ENC_BASE_VIEW_DOUBLE_BUFFER;
#endif

#if defined (H1V5) || defined (H1V6)
    cfg.scaledWidth = 0;
    cfg.scaledHeight = 0;
#endif

#if defined (H1V6) || defined (ENCH1)
    cfg.refFrameAmount = 1;
#endif

#if !defined (ENC8290) && !defined (ENCH1)
    cfg.complexityLevel = H264ENC_COMPLEXITY_1; // Only supported option
#endif

    ENCODER_H264* this = OSAL_Malloc(sizeof(ENCODER_H264));

    this->instance = 0;
    memset(&this->encIn, 0, sizeof(H264EncIn));
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->outputWidth = params->common_config.nOutputWidth;
    this->outputHeight = params->common_config.nOutputHeight;
    this->base.stream_start = encoder_stream_start_h264;
    this->base.stream_end = encoder_stream_end_h264;
    this->base.encode = encoder_encode_h264;
    this->base.destroy = encoder_destroy_h264;

    this->bStabilize = params->pp_config.frameStabilization;
    this->nPFrames = params->nPFrames;
    this->nIFrameCounter = 0;
    this->nEstTimeInc = cfg.frameRateDenom;
    this->nCirCounter = 0;

    DBGT_PDEBUG("Intra period %d", (int)this->nPFrames);

    apiVer = H264EncGetApiVersion();
    encBuild = H264EncGetBuild();

    DBGT_PDEBUG("H.264 Encoder API version %d.%d", apiVer.major,
            apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    H264EncRet ret = H264EncInit(&cfg, &this->instance);

    // Setup coding control
    if (ret == H264ENC_OK)
    {
        H264EncCodingCtrl coding_ctrl;
        ret = H264EncGetCodingCtrl(this->instance, &coding_ctrl);

        if (ret == H264ENC_OK)
        {
            if (params->nSliceHeight > 0)
            {
                // slice height in macroblock rows (each row is 16 pixels)
                coding_ctrl.sliceSize = params->nSliceHeight / 16;
            }

            if (params->h264_config.eProfile == OMX_VIDEO_AVCProfileBaseline) 
            {
                coding_ctrl.enableCabac = 0;
                coding_ctrl.transform8x8Mode = 0;
            } 
            else if  (params->h264_config.eProfile == OMX_VIDEO_AVCProfileMain)
            {
                coding_ctrl.enableCabac = 1;
                coding_ctrl.transform8x8Mode = 0;
            }
            else 
            {
                coding_ctrl.enableCabac = 1;
                coding_ctrl.transform8x8Mode = 2;
            }

            coding_ctrl.disableDeblockingFilter = params->bDisableDeblocking;

            // SEI information
            coding_ctrl.seiMessages = params->bSeiMessages;

            //videoFullRange
            coding_ctrl.videoFullRange = params->nVideoFullRange;
            
            DBGT_PDEBUG("coding_ctrl.sliceSize %d", coding_ctrl.sliceSize);
            DBGT_PDEBUG("coding_ctrl.seiMessages %d", coding_ctrl.seiMessages);
            DBGT_PDEBUG("coding_ctrl.videoFullRange %d", coding_ctrl.videoFullRange);
            DBGT_PDEBUG("coding_ctrl.constrainedIntraPrediction %d", coding_ctrl.constrainedIntraPrediction);
            DBGT_PDEBUG("coding_ctrl.disableDeblockingFilter %d", coding_ctrl.disableDeblockingFilter);
            DBGT_PDEBUG("coding_ctrl.sampleAspectRatioWidth %d", coding_ctrl.sampleAspectRatioWidth);
            DBGT_PDEBUG("coding_ctrl.sampleAspectRatioHeight %d", coding_ctrl.sampleAspectRatioHeight);
#if defined (ENC8290) || defined (ENCH1)
            DBGT_PDEBUG("coding_ctrl.enableCabac %d", coding_ctrl.enableCabac);
            DBGT_PDEBUG("coding_ctrl.cabacInitIdc %d", coding_ctrl.cabacInitIdc);
            DBGT_PDEBUG("coding_ctrl.transform8x8Mode %d", coding_ctrl.transform8x8Mode);
            DBGT_PDEBUG("coding_ctrl.quarterPixelMv %d", coding_ctrl.quarterPixelMv);
            DBGT_PDEBUG("coding_ctrl.cirStart %d", coding_ctrl.cirStart);
            DBGT_PDEBUG("coding_ctrl.cirInterval %d", coding_ctrl.cirInterval);
            DBGT_PDEBUG("coding_ctrl.intraSliceMap1 %d", coding_ctrl.intraSliceMap1);
            DBGT_PDEBUG("coding_ctrl.intraSliceMap2 %d", coding_ctrl.intraSliceMap2);
            DBGT_PDEBUG("coding_ctrl.intraSliceMap3 %d", coding_ctrl.intraSliceMap3);
            DBGT_PDEBUG("coding_ctrl.roi1DeltaQp %d", coding_ctrl.roi1DeltaQp);
            DBGT_PDEBUG("coding_ctrl.roi2DeltaQp %d", coding_ctrl.roi2DeltaQp);
#ifdef H1V6
            DBGT_PDEBUG("coding_ctrl.adaptiveRoi %d", coding_ctrl.adaptiveRoi);
            DBGT_PDEBUG("coding_ctrl.adaptiveRoiColor %d", coding_ctrl.adaptiveRoiColor);
            DBGT_PDEBUG("coding_ctrl.fieldOrder %d", coding_ctrl.fieldOrder);
            DBGT_PDEBUG("coding_ctrl.idrHeader %d", coding_ctrl.idrHeader);
#endif
#endif
            ret = H264EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
        else
        {
            DBGT_CRITICAL("H264EncGetCodingCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("H264EncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    if (ret == H264ENC_OK)
    {
        H264EncRateCtrl rate_ctrl;
        ret = H264EncGetRateCtrl(this->instance, &rate_ctrl);

        if (ret == H264ENC_OK)
        {

            if (params->nPFrames)
            {
                rate_ctrl.gopLen = params->nPFrames;
            }

            // optional. Set to -1 to use default
            if (params->rate_config.nTargetBitrate > 0)
            {
                rate_ctrl.bitPerSecond = params->rate_config.nTargetBitrate;
            }

            // optional. Set to -1 to use default
            if (params->rate_config.nQpDefault > 0)
            {
                rate_ctrl.qpHdr = params->rate_config.nQpDefault;
            }

            // optional. Set to -1 to use default
            if (params->rate_config.nQpMin >= 0)
            {
                rate_ctrl.qpMin = params->rate_config.nQpMin;
                if (rate_ctrl.qpHdr != -1 && rate_ctrl.qpHdr < (OMX_S32)rate_ctrl.qpMin)
                {
                    rate_ctrl.qpHdr = rate_ctrl.qpMin;
                }
            }

            // optional. Set to -1 to use default
            if (params->rate_config.nQpMax > 0)
            {
                rate_ctrl.qpMax = params->rate_config.nQpMax;
                if (rate_ctrl.qpHdr > (OMX_S32)rate_ctrl.qpMax)
                {
                    rate_ctrl.qpHdr = rate_ctrl.qpMax;
                }
            }

            // Set to -1 to use default
            if (params->rate_config.nPictureRcEnabled >= 0)
            {
                rate_ctrl.pictureRc = params->rate_config.nPictureRcEnabled;
            }

            // Set to -1 to use default
            if (params->rate_config.nMbRcEnabled >= 0)
            {
                rate_ctrl.mbRc = params->rate_config.nMbRcEnabled;
            }

            // Set to -1 to use default
            if (params->rate_config.nHrdEnabled >= 0)
            {
                rate_ctrl.hrd = params->rate_config.nHrdEnabled;
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

            DBGT_PDEBUG("rate_ctrl.pictureRc %d", rate_ctrl.pictureRc);
            DBGT_PDEBUG("rate_ctrl.mbRc %d", rate_ctrl.mbRc);
            DBGT_PDEBUG("rate_ctrl.pictureSkip %d", rate_ctrl.pictureSkip);
            DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
            DBGT_PDEBUG("rate_ctrl.qpMin %d", rate_ctrl.qpMin);
            DBGT_PDEBUG("rate_ctrl.qpMax %d", rate_ctrl.qpMax);
            DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);
            DBGT_PDEBUG("rate_ctrl.hrd %d", rate_ctrl.hrd);
            DBGT_PDEBUG("rate_ctrl.hrdCpbSize %d", rate_ctrl.hrdCpbSize);
            DBGT_PDEBUG("rate_ctrl.gopLen %d", rate_ctrl.gopLen);
#if defined (ENC8290) || defined (ENCH1)
            DBGT_PDEBUG("rate_ctrl.intraQpDelta %d", rate_ctrl.intraQpDelta);
            DBGT_PDEBUG("rate_ctrl.fixedIntraQp %d", rate_ctrl.fixedIntraQp);
            DBGT_PDEBUG("rate_ctrl.mbQpAdjustment %d", rate_ctrl.mbQpAdjustment);
#ifdef H1V6
            DBGT_PDEBUG("rate_ctrl.longTermPicRate %d", rate_ctrl.longTermPicRate);
            DBGT_PDEBUG("rate_ctrl.mbQpAutoBoost %d", rate_ctrl.mbQpAutoBoost);
#endif
#endif
            ret = H264EncSetRateCtrl(this->instance, &rate_ctrl);
        }
        else
        {
            DBGT_CRITICAL("H264EncGetRateCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("H264EncSetCodingCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    if (ret == H264ENC_OK)
    {
        H264EncPreProcessingCfg pp_config;
        ret = H264EncGetPreProcessing(this->instance, &pp_config);

        if (ret != H264ENC_OK)
        {
            DBGT_CRITICAL("H264EncGetPreProcessing failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }

        pp_config.origWidth = params->pp_config.origWidth;
        pp_config.origHeight = params->pp_config.origHeight;
        pp_config.xOffset = params->pp_config.xOffset;
        pp_config.yOffset = params->pp_config.yOffset;

        DBGT_PDEBUG("origWidth: %d", (int)pp_config.origWidth);
        DBGT_PDEBUG("origHeight: %d", (int)pp_config.origHeight);
        DBGT_PDEBUG("xOffset: %d", (int)pp_config.xOffset);
        DBGT_PDEBUG("yOffset: %d", (int)pp_config.yOffset);
        DBGT_PDEBUG("Color format: %s", HantroOmx_str_omx_color(params->pp_config.formatType));

        switch (params->pp_config.formatType)
        {
            case OMX_COLOR_FormatYUV420PackedPlanar:
            case OMX_COLOR_FormatYUV420Planar:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_YUV420_PLANAR;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_YUV420_PLANAR;
#endif
                break;
            case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_YUV420_SEMIPLANAR;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_YUV420_SEMIPLANAR;
#endif
                break;
            case OMX_COLOR_FormatYCbYCr:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_YUV422_INTERLEAVED_YUYV;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_YUV422_INTERLEAVED_YUYV;
#endif
                break;
            case OMX_COLOR_FormatCbYCrY:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_YUV422_INTERLEAVED_UYVY;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_YUV422_INTERLEAVED_UYVY;
#endif
                break;
            case OMX_COLOR_Format16bitRGB565:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_RGB565;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_RGB565;
#endif
                break;
            case OMX_COLOR_Format16bitBGR565:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_BGR565;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_BGR565;
#endif
                break;
            case OMX_COLOR_Format16bitARGB4444:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_RGB444;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_RGB444;
#endif
                break;
            case OMX_COLOR_Format16bitARGB1555:
#ifdef ENC6280
                pp_config.yuvType = H264ENC_RGB555;
#endif
#if defined (ENC7280) || defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
                pp_config.inputType = H264ENC_RGB555;
#endif
                break;
#if defined (ENC8290) || defined (ENCH1)
            case OMX_COLOR_Format12bitRGB444:
                pp_config.inputType = H264ENC_RGB444;
                break;
            case OMX_COLOR_Format25bitARGB1888:
            case OMX_COLOR_Format32bitARGB8888:
                pp_config.inputType = H264ENC_RGB888;
                break;
            case OMX_COLOR_Format32bitBGRA8888:
                pp_config.inputType = H264ENC_BGR888;
                break;
#endif
            default:
                DBGT_CRITICAL("Unknown color format");
                ret = H264ENC_INVALID_ARGUMENT;
                break;
        }

        switch (params->pp_config.angle)
        {
            case 0:
                pp_config.rotation = H264ENC_ROTATE_0;
                break;
            case 90:
                pp_config.rotation = H264ENC_ROTATE_90R;
                break;
            case 270:
                pp_config.rotation = H264ENC_ROTATE_90L;
                break;
            default:
                DBGT_CRITICAL("Unsupported rotation angle");
                ret = H264ENC_INVALID_ARGUMENT;
                break;
        }

        pp_config.videoStabilization = params->pp_config.frameStabilization;

        if (ret == H264ENC_OK)
        {
            DBGT_PDEBUG("Video stabilization: %d", (int)pp_config.videoStabilization);
            DBGT_PDEBUG("Rotation: %d", (int)params->pp_config.angle);
            ret = H264EncSetPreProcessing(this->instance, &pp_config);
        }
    }
    else
    {
        DBGT_CRITICAL("H264EncSetRateCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (ret != H264ENC_OK)
    {
        DBGT_CRITICAL("H264EncSetPreProcessing failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_intra_period_h264(ENCODER_PROTOTYPE* arg, OMX_U32 nPFrames)
{
    DBGT_PROLOG("");

    ENCODER_H264* this = (ENCODER_H264*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->nPFrames = nPFrames;
    DBGT_PDEBUG("New Intra period %d", (int)this->nPFrames);

    H264EncRateCtrl rate_ctrl;
    memset(&rate_ctrl,0,sizeof(H264EncRateCtrl));

    H264EncRet ret = H264EncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == H264ENC_OK)
    {
        rate_ctrl.gopLen = nPFrames;

        ret = H264EncSetRateCtrl(this->instance, &rate_ctrl);
    }
    else
    {
        DBGT_CRITICAL("H264EncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case H264ENC_OK:
            stat = CODEC_OK;
            break;
        case H264ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case H264ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case H264ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_h264(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_H264* this = (ENCODER_H264*)arg;

    this->nEstTimeInc = (OMX_U32) (TIME_RESOLUTION / Q16_FLOAT(xFramerate));

    DBGT_PDEBUG("New time increment %d", (int)this->nEstTimeInc);

    DBGT_EPILOG("");
    return CODEC_OK;
}
