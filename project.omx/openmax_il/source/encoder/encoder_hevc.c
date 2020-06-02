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
#include <stdio.h>
#include "encoder.h"
#include "encoder_hevc.h"
#include "util.h"
#include "hevcencapi.h"
#include "OSAL.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX HEVC"

#if !defined (ENCH2)
#error "SPECIFY AN ENCODER PRODUCT (ENCH2) IN COMPILER DEFINES!"
#endif

typedef struct ENCODER_HEVC
{
    ENCODER_PROTOTYPE base;

    VCEncInst instance;
    VCEncIn encIn;
    VCEncGopConfig gopConfig;
    OMX_U32 origWidth;
    OMX_U32 origHeight;
    OMX_U32 nEstTimeInc;
    OMX_U32 bStabilize;
    OMX_U32 nIFrameCounter;
    OMX_U32 nPFrames;
    OMX_U32 nTotalFrames;
} ENCODER_HEVC;

VCEncGopPicConfig gopConfig1[] = {{1, 0, 0.8, VCENC_PREDICTED_FRAME, 1, {{-1,1}, {0,0}, {0,0}, {0,0}}}};

// destroy codec instance
static void encoder_destroy_hevc(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            VCEncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_hevc(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;
    this->nTotalFrames = 0;

    this->encIn.gopConfig = this->gopConfig;

    VCEncOut encOut;
    VCEncRet ret = VCEncStrmStart(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case VCENC_OK:
            stream->streamlen = encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_ERROR:
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
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

static CODEC_STATE encoder_stream_end_hevc(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;

    VCEncOut encOut;
    VCEncRet ret = VCEncStrmEnd(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case VCENC_OK:
            stream->streamlen = encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
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

static CODEC_STATE encoder_encode_hevc(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);

    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VIDEO_ENCODER_CONFIG* encConf = (VIDEO_ENCODER_CONFIG*) cfg;

    VCEncRet ret;
    VCEncOut encOut;
    VCEncRateCtrl rate_ctrl;
    VCEncCodingCtrl coding_ctrl;

    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth
            * this->origHeight);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth
            * this->origHeight / 4);

    this->encIn.timeIncrement = this->nEstTimeInc;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.busOutBuf = stream->bus_address;

    this->encIn.gopSize = 1; // H2v1 supports only size 1
    this->encIn.gopConfig = this->gopConfig;

    DBGT_PDEBUG("Input timeInc: %u outBufSize: %u, virtual address: %p, bus address: 0x%08lx",
        this->encIn.timeIncrement, this->encIn.outBufSize,
        this->encIn.pOutBuf, this->encIn.busOutBuf);

    // The bus address of the luminance component buffer of the picture to be stabilized.
    // Used only when video stabilization is enabled.
/*#ifdef USE_STAB_TEMP_INPUTBUFFER
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
    }*/

    if (frame->frame_type == OMX_INTRA_FRAME)
    {
        this->encIn.codingType = VCENC_INTRA_FRAME;
        this->encIn.poc = 0;
    }
    else if (frame->frame_type == OMX_PREDICTED_FRAME)
    {
        if (this->nPFrames > 0)
        {
            if ((this->nIFrameCounter % (this->nPFrames+1)) == 0)
            {
                this->encIn.codingType = VCENC_INTRA_FRAME;
                this->nIFrameCounter = 0;
                this->encIn.poc = 0;
            }
            else
            {
                this->encIn.codingType = VCENC_PREDICTED_FRAME;
            }
            this->nIFrameCounter++;

        }
        else
        {
            this->encIn.codingType = VCENC_INTRA_FRAME;
            this->encIn.poc = 0;
        }
    }
    else
    {
        DBGT_CRITICAL("Unknown frame type");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSUPPORTED_SETTING;
    }
    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    //DBGT_PDEBUG("encIn.busLumaStab 0x%08x", (unsigned int)this->encIn.busLumaStab);
    DBGT_PDEBUG("Frame type %s, POC %d, IFrame counter (%d/%d)", (this->encIn.codingType == VCENC_INTRA_FRAME) ? "I":"P",
        (int)this->encIn.poc, (int)this->nIFrameCounter, (int)this->nPFrames+1);

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncrement = 0;
    }

    ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK && (rate_ctrl.bitPerSecond != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    }

    DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
    DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);

    ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);

    if (ret == VCENC_OK)
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

            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
        }

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

            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
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

            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
        }
    }

    if (ret == VCENC_OK)
    {
        ret = VCEncStrmEncode(this->instance, &this->encIn, &encOut, NULL, NULL);
    }
    else
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case VCENC_FRAME_READY:
            this->nTotalFrames++;
            this->encIn.poc++;
            stream->streamlen = encOut.streamSize;

            if (encOut.codingType == VCENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (encOut.codingType == VCENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else if (encOut.codingType == VCENC_NOTCODED_FRAME)
            {
                DBGT_PDEBUG("encOut: Not coded frame");
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case VCENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case VCENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case VCENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case VCENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case VCENC_HW_RESERVED:
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
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_hevc(const HEVC_CONFIG* params)
{
    DBGT_PROLOG("");
    VCEncConfig cfg;
    VCEncApiVersion apiVer;
    VCEncBuild encBuild;
    VCEncRet ret = VCENC_OK;

    memset(&cfg, 0, sizeof(VCEncConfig));

    if (params->hevc_config.eProfile != OMX_VIDEO_HEVCProfileMain)
    {
        DBGT_CRITICAL("Profile not supported (requested profile: %d)", params->hevc_config.eProfile);
        DBGT_EPILOG("");
        return NULL;
    }

    switch (params->hevc_config.eLevel)
    {
        case OMX_VIDEO_HEVCLevel1:
            cfg.level = VCENC_HEVC_LEVEL_1;
            break;
        case OMX_VIDEO_HEVCLevel2:
            cfg.level = VCENC_HEVC_LEVEL_2;
            break;
        case OMX_VIDEO_HEVCLevel21:
            cfg.level = VCENC_HEVC_LEVEL_2_1;
            break;
        case OMX_VIDEO_HEVCLevel3:
            cfg.level = VCENC_HEVC_LEVEL_3;
            break;
        case OMX_VIDEO_HEVCLevel31:
            cfg.level = VCENC_HEVC_LEVEL_3_1;
            break;
        case OMX_VIDEO_HEVCLevel4:
            cfg.level = VCENC_HEVC_LEVEL_4;
            break;
        case OMX_VIDEO_HEVCLevel41:
            cfg.level = VCENC_HEVC_LEVEL_4_1;
            break;
        case OMX_VIDEO_HEVCLevel5:
        case OMX_VIDEO_HEVCLevelMax:
            cfg.level = VCENC_HEVC_LEVEL_5;
            break;
        default:
            DBGT_CRITICAL("Unsupported encoding level %d", params->hevc_config.eLevel);
            DBGT_EPILOG("");
            return NULL;
            break;
    }


    cfg.streamType = VCENC_BYTE_STREAM;
    cfg.width = params->common_config.nOutputWidth;
    cfg.height = params->common_config.nOutputHeight;
    cfg.strongIntraSmoothing = params->hevc_config.bStrongIntraSmoothing;
    cfg.frameRateNum = TIME_RESOLUTION;
    cfg.frameRateDenom = cfg.frameRateNum / Q16_FLOAT(params->common_config.nInputFramerate);
    cfg.bitDepthLuma = params->hevc_config.nBitDepthLuma;
    cfg.bitDepthChroma = params->hevc_config.nBitDepthChroma;
    cfg.refFrameAmount = params->hevc_config.nRefFrames;

    if(!params->hevc_config.nPFrames)
        cfg.refFrameAmount = 0;
       
    DBGT_PDEBUG("streamType %d", cfg.streamType);
    DBGT_PDEBUG("profile %d", cfg.profile);
    DBGT_PDEBUG("Hevc level: %d", (int)cfg.level);
    DBGT_PDEBUG("OutputWidth: %d", (int)cfg.width);
    DBGT_PDEBUG("OutputHeight: %d", (int)cfg.height);
    DBGT_PDEBUG("frameRateNum %d / frameRateDenom %d", cfg.frameRateNum, cfg.frameRateDenom);
    DBGT_PDEBUG("strongIntraSmoothing %d", cfg.strongIntraSmoothing);
    DBGT_PDEBUG("refFrameAmount %d", cfg.refFrameAmount);
    DBGT_PDEBUG("sliceSize %d", cfg.sliceSize);
    DBGT_PDEBUG("compressor %d", cfg.compressor);
    DBGT_PDEBUG("interlacedFrame %d", cfg.interlacedFrame);
    DBGT_PDEBUG("bitDepthLuma %d", cfg.bitDepthLuma);
    DBGT_PDEBUG("bitDepthChroma %d", cfg.bitDepthChroma);

    ENCODER_HEVC* this = OSAL_Malloc(sizeof(ENCODER_HEVC));

    this->instance = 0;
    memset(&this->encIn, 0, sizeof(VCEncIn));
    this->base.stream_start = encoder_stream_start_hevc;
    this->base.stream_end = encoder_stream_end_hevc;
    this->base.encode = encoder_encode_hevc;
    this->base.destroy = encoder_destroy_hevc;

    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->bStabilize = 0;//params->pp_config.frameStabilization;
    this->nPFrames = params->hevc_config.nPFrames;
    this->nIFrameCounter = 0;
    this->nEstTimeInc = cfg.frameRateDenom;

    this->gopConfig.pGopPicCfg = gopConfig1;
    this->gopConfig.size = 1;
    this->gopConfig.id = 0;

    DBGT_PDEBUG("Intra period %d", (int)this->nPFrames + 1);

    apiVer = VCEncGetApiVersion();
    encBuild = VCEncGetBuild();

    DBGT_PDEBUG("VC Encoder API version %d.%d", apiVer.major,
            apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    ret = VCEncInit(&cfg, &this->instance);

    // Setup coding control
    if (ret == VCENC_OK)
    {
        VCEncCodingCtrl coding_ctrl;
        ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);

        if (ret == VCENC_OK)
        {
            if (params->nSliceHeight > 0)
            {
                // slice height in CTU rows
                coding_ctrl.sliceSize = params->nSliceHeight / 16;
            }

            coding_ctrl.disableDeblockingFilter = params->bDisableDeblocking;
            coding_ctrl.seiMessages = params->bSeiMessages;
            coding_ctrl.videoFullRange = params->nVideoFullRange;
            coding_ctrl.tc_Offset = params->hevc_config.nTcOffset;
            coding_ctrl.beta_Offset = params->hevc_config.nBetaOffset;
            coding_ctrl.enableDeblockOverride = params->hevc_config.bEnableDeblockOverride;
            coding_ctrl.deblockOverride = params->hevc_config.bDeblockOverride;
            coding_ctrl.enableSao = params->hevc_config.bEnableSAO;
            coding_ctrl.enableScalingList = params->hevc_config.bEnableScalingList;
            coding_ctrl.cabacInitFlag = params->hevc_config.bCabacInitFlag;

            if (params->roiArea[0].bEnable)
            {
                coding_ctrl.roi1Area.enable  = params->roiArea[0].bEnable;
                coding_ctrl.roi1Area.top     = params->roiArea[0].nTop;
                coding_ctrl.roi1Area.left    = params->roiArea[0].nLeft;
                coding_ctrl.roi1Area.bottom  = params->roiArea[0].nBottom;
                coding_ctrl.roi1Area.right   = params->roiArea[0].nRight;
                coding_ctrl.roi1DeltaQp      = params->roiDeltaQP[0].nDeltaQP;
                DBGT_PDEBUG("ROI area 1 enabled (%d, %d, %d, %d)",
                    coding_ctrl.roi1Area.top, coding_ctrl.roi1Area.left,
                    coding_ctrl.roi1Area.bottom, coding_ctrl.roi1Area.right);
                DBGT_PDEBUG("ROI 1 delta QP %d", coding_ctrl.roi1DeltaQp);
            }

            if (params->roiArea[1].bEnable)
            {
                coding_ctrl.roi2Area.enable  = params->roiArea[1].bEnable;
                coding_ctrl.roi2Area.top     = params->roiArea[1].nTop;
                coding_ctrl.roi2Area.left    = params->roiArea[1].nLeft;
                coding_ctrl.roi2Area.bottom  = params->roiArea[1].nBottom;
                coding_ctrl.roi2Area.right   = params->roiArea[1].nRight;
                coding_ctrl.roi2DeltaQp      = params->roiDeltaQP[1].nDeltaQP;
                DBGT_PDEBUG("ROI area 2 enabled (%d, %d, %d, %d)",
                    coding_ctrl.roi2Area.top, coding_ctrl.roi2Area.left,
                    coding_ctrl.roi2Area.bottom, coding_ctrl.roi2Area.right);
                DBGT_PDEBUG("ROI 2 delta QP %d", coding_ctrl.roi2DeltaQp);
            }

            DBGT_PDEBUG("coding_ctrl.sliceSize %d", coding_ctrl.sliceSize);
            DBGT_PDEBUG("coding_ctrl.seiMessages %d", coding_ctrl.seiMessages);
            DBGT_PDEBUG("coding_ctrl.videoFullRange %d", coding_ctrl.videoFullRange);
            DBGT_PDEBUG("coding_ctrl.disableDeblockingFilter %d", coding_ctrl.disableDeblockingFilter);
            DBGT_PDEBUG("coding_ctrl.tc_Offset %d", coding_ctrl.tc_Offset);
            DBGT_PDEBUG("coding_ctrl.beta_Offset %d", coding_ctrl.beta_Offset);
            DBGT_PDEBUG("coding_ctrl.enableDeblockOverride %d", coding_ctrl.enableDeblockOverride);
            DBGT_PDEBUG("coding_ctrl.deblockOverride %d", coding_ctrl.deblockOverride);
            DBGT_PDEBUG("coding_ctrl.enableSao %d", coding_ctrl.enableSao);
            DBGT_PDEBUG("coding_ctrl.enableScalingList %d", coding_ctrl.enableScalingList);
            DBGT_PDEBUG("coding_ctrl.sampleAspectRatioWidth %d", coding_ctrl.sampleAspectRatioWidth);
            DBGT_PDEBUG("coding_ctrl.sampleAspectRatioHeight %d", coding_ctrl.sampleAspectRatioHeight);
            DBGT_PDEBUG("coding_ctrl.cabacInitFlag %d", coding_ctrl.cabacInitFlag);
            DBGT_PDEBUG("coding_ctrl.cirStart %d", coding_ctrl.cirStart);
            DBGT_PDEBUG("coding_ctrl.cirInterval %d", coding_ctrl.cirInterval);
            DBGT_PDEBUG("coding_ctrl.intraArea.enable %d", coding_ctrl.intraArea.enable);
            DBGT_PDEBUG("coding_ctrl.roi1Area.enable %d", coding_ctrl.roi1Area.enable);
            DBGT_PDEBUG("coding_ctrl.roi2Area.enable %d", coding_ctrl.roi2Area.enable);
            DBGT_PDEBUG("coding_ctrl.roi1DeltaQp %d", coding_ctrl.roi1DeltaQp);
            DBGT_PDEBUG("coding_ctrl.roi2DeltaQp %d", coding_ctrl.roi2DeltaQp);
            DBGT_PDEBUG("coding_ctrl.fieldOrder %d", coding_ctrl.fieldOrder);
            DBGT_PDEBUG("coding_ctrl.chroma_qp_offset %d", coding_ctrl.chroma_qp_offset);
            DBGT_PDEBUG("coding_ctrl.roiMapDeltaQpEnable %d", coding_ctrl.roiMapDeltaQpEnable);
            DBGT_PDEBUG("coding_ctrl.roiMapDeltaQpBlockUnit %d", coding_ctrl.roiMapDeltaQpBlockUnit);
            DBGT_PDEBUG("coding_ctrl.noiseReductionEnable %d", coding_ctrl.noiseReductionEnable);
            DBGT_PDEBUG("coding_ctrl.noiseLow %d", coding_ctrl.noiseLow);
            DBGT_PDEBUG("coding_ctrl.firstFrameSigma %d", coding_ctrl.firstFrameSigma);
            DBGT_PDEBUG("coding_ctrl.gdrDuration %d", coding_ctrl.gdrDuration);

            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
        }
        else
        {
            DBGT_CRITICAL("VCEncGetCodingCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("VCEncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    if (ret == VCENC_OK)
    {
        VCEncRateCtrl rate_ctrl;
        ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

        if (ret == VCENC_OK)
        {

            //if (params->hevc_config.nPFrames)
            {
                rate_ctrl.bitrateWindow = params->hevc_config.nPFrames + 1;
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

            rate_ctrl.monitorFrames = cfg.frameRateNum / cfg.frameRateDenom;

            DBGT_PDEBUG("rate_ctrl.pictureRc %d", rate_ctrl.pictureRc);
            DBGT_PDEBUG("rate_ctrl.ctbRc %d", rate_ctrl.ctbRc);
            DBGT_PDEBUG("rate_ctrl.pictureSkip %d", rate_ctrl.pictureSkip);
            DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
            DBGT_PDEBUG("rate_ctrl.qpMin %d", rate_ctrl.qpMin);
            DBGT_PDEBUG("rate_ctrl.qpMax %d", rate_ctrl.qpMax);
            DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);
            DBGT_PDEBUG("rate_ctrl.hrd %d", rate_ctrl.hrd);
            DBGT_PDEBUG("rate_ctrl.hrdCpbSize %d", rate_ctrl.hrdCpbSize);
            DBGT_PDEBUG("rate_ctrl.gopLen %d", rate_ctrl.gopLen);
            DBGT_PDEBUG("rate_ctrl.intraQpDelta %d", rate_ctrl.intraQpDelta);
            DBGT_PDEBUG("rate_ctrl.fixedIntraQp %d", rate_ctrl.fixedIntraQp);
            DBGT_PDEBUG("rate_ctrl.bitVarRangeI %d", rate_ctrl.bitVarRangeI);
            DBGT_PDEBUG("rate_ctrl.bitVarRangeP %d", rate_ctrl.bitVarRangeP);
            DBGT_PDEBUG("rate_ctrl.bitVarRangeB %d", rate_ctrl.bitVarRangeB);
            DBGT_PDEBUG("rate_ctrl.tolMovingBitRate %d", rate_ctrl.tolMovingBitRate);
            DBGT_PDEBUG("rate_ctrl.monitorFrames %d", rate_ctrl.monitorFrames);

            rate_ctrl.hrdCpbSize = 0;

            ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
        }
        else
        {
            DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("VCEncSetCodingCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    if (ret == VCENC_OK)
    {
        VCEncPreProcessingCfg pp_config;
        ret = VCEncGetPreProcessing(this->instance, &pp_config);

        if (ret != VCENC_OK)
        {
            DBGT_CRITICAL("VCEncGetPreProcessing failed! (%d)", ret);
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
        DBGT_PDEBUG("inputType: %d", (int)pp_config.inputType);
        DBGT_PDEBUG("rotation: %d", (int)pp_config.rotation);
        //DBGT_PDEBUG("colorConversion: %d", (int)pp_config.colorConversion);
        DBGT_PDEBUG("scaledWidth: %d", (int)pp_config.scaledWidth);
        DBGT_PDEBUG("scaledHeight: %d", (int)pp_config.scaledHeight);
        DBGT_PDEBUG("scaledOutput: %d", (int)pp_config.scaledOutput);
        DBGT_PDEBUG("virtualAddressScaledBuff: %p", pp_config.virtualAddressScaledBuff);
        DBGT_PDEBUG("busAddressScaledBuff: 0x%08lx", pp_config.busAddressScaledBuff);
        DBGT_PDEBUG("sizeScaledBuff: %d", (int)pp_config.sizeScaledBuff);
        DBGT_PDEBUG("Color format: %s", HantroOmx_str_omx_color(params->pp_config.formatType));
        //pp_config.scaledWidth = 0;
        //pp_config.scaledHeight = 0;
        pp_config.scaledOutput = 0;

        switch (params->pp_config.formatType)
        {
            case OMX_COLOR_FormatYUV420PackedPlanar:
            case OMX_COLOR_FormatYUV420Planar:
                pp_config.inputType = VCENC_YUV420_PLANAR;
                break;
            case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
                pp_config.inputType = VCENC_YUV420_SEMIPLANAR;
                break;
            case OMX_COLOR_FormatYCbYCr:
                pp_config.inputType = VCENC_YUV422_INTERLEAVED_YUYV;
                break;
            case OMX_COLOR_FormatCbYCrY:
                pp_config.inputType = VCENC_YUV422_INTERLEAVED_UYVY;
                break;
            case OMX_COLOR_Format16bitRGB565:
                pp_config.inputType = VCENC_RGB565;
                break;
            case OMX_COLOR_Format16bitBGR565:
                pp_config.inputType = VCENC_BGR565;
                break;
            case OMX_COLOR_Format16bitARGB4444:
                pp_config.inputType = VCENC_RGB444;
                break;
            case OMX_COLOR_Format16bitARGB1555:
                pp_config.inputType = VCENC_RGB555;
                break;
            case OMX_COLOR_Format12bitRGB444:
                pp_config.inputType = VCENC_RGB444;
            case OMX_COLOR_Format25bitARGB1888:
            case OMX_COLOR_Format32bitARGB8888:
                pp_config.inputType = VCENC_RGB888;
                break;

            default:
                DBGT_CRITICAL("Unknown color format");
                ret = VCENC_INVALID_ARGUMENT;
                break;
        }

        switch (params->pp_config.angle)
        {
            case 0:
                pp_config.rotation = VCENC_ROTATE_0;
                break;
            case 90:
                pp_config.rotation = VCENC_ROTATE_90R;
                break;
            case 270:
                pp_config.rotation = VCENC_ROTATE_90L;
                break;
            default:
                DBGT_CRITICAL("Unsupported rotation angle");
                ret = VCENC_INVALID_ARGUMENT;
                break;
        }

        //pp_config.videoStabilization = params->pp_config.frameStabilization;

        if (ret == VCENC_OK)
        {
            //DBGT_PDEBUG("Video stabilization: %d", (int)pp_config.videoStabilization);
            DBGT_PDEBUG("Rotation: %d", (int)params->pp_config.angle);
            ret = VCEncSetPreProcessing(this->instance, &pp_config);
        }
    }
    else
    {
        DBGT_CRITICAL("VCEncSetRateCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetPreProcessing failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_intra_period_hevc(ENCODER_PROTOTYPE* arg, OMX_U32 nPFrames)
{
    DBGT_PROLOG("");

    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->nPFrames = nPFrames;
    DBGT_PDEBUG("New Intra period %d", (int)this->nPFrames+1);

    VCEncRateCtrl rate_ctrl;
    memset(&rate_ctrl, 0, sizeof(VCEncRateCtrl));

    VCEncRet ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK)
    {
        rate_ctrl.bitrateWindow = nPFrames+1;

        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    }
    else
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case VCENC_OK:
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
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

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_hevc(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_HEVC* this = (ENCODER_HEVC*)arg;

    this->nEstTimeInc = (OMX_U32) (TIME_RESOLUTION / Q16_FLOAT(xFramerate));

    DBGT_PDEBUG("New time increment %d", (int)this->nEstTimeInc);

    DBGT_EPILOG("");
    return CODEC_OK;
}
