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

#include "encoder_mpeg4.h"
#include "util.h"
#include "mp4encapi.h"
#include "OSAL.h"
#include <string.h>
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX MPEG4"

#if !defined (ENC6280) && !defined (ENC7280)
#error "SPECIFY AN ENCODER PRODUCT (ENC6280 or ENC7280) IN COMPILER DEFINES!"
#endif

typedef struct ENCODER_MPEG4
{
  ENCODER_PROTOTYPE base;

  MP4EncInst instance;
  MP4EncIn encIn;
  OMX_U32 origWidth;
  OMX_U32 origHeight;
  OMX_U32 nPFrames;
  OMX_U32 nFrameCounter;
  OMX_U32 bGovEnabled;
  OMX_U32 nTickIncrement;
  OMX_U32 nAllowedPictureTypes;
  OMX_U32 nTotalFrames;
  OMX_BOOL bStabilize;
} ENCODER_MPEG4;

// destroy codec instance
static void encoder_destroy_mpeg4(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_MPEG4* this = (ENCODER_MPEG4*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            MP4EncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_mpeg4(ENCODER_PROTOTYPE* arg,
                                              STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);
    ENCODER_MPEG4* this = (ENCODER_MPEG4*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufBusAddress = stream->bus_address;
    this->encIn.outBufSize = stream->buf_max_size;
    this->nTotalFrames = 0;

    MP4EncOut encOut;
    MP4EncRet ret = MP4EncStrmStart(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case ENC_OK:
            stream->streamlen = encOut.strmSize;
            stat = CODEC_OK;
            break;
        case ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case ENC_OUTPUT_BUFFER_OVERFLOW:
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

static CODEC_STATE encoder_stream_end_mpeg4(ENCODER_PROTOTYPE* arg,
                                            STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);
    ENCODER_MPEG4* this = (ENCODER_MPEG4*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufSize = stream->buf_max_size;
    this->encIn.outBufBusAddress = stream->bus_address;

    MP4EncOut encOut;
    MP4EncRet ret = MP4EncStrmEnd(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case ENC_OK:
            stream->streamlen = encOut.strmSize;
            stat = CODEC_OK;
            break;
        case ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INVALID_STATUS:
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

// NOTE This needs to support changing coding control settings during runtime.
// NOTE Otherwise GOV headers can't be inserted into stream.
static CODEC_STATE encoder_encode_mpeg4(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);
    UNUSED_PARAMETER(cfg);

    ENCODER_MPEG4* this = (ENCODER_MPEG4*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    MP4EncOut encOut;
    MP4EncOut encOutGov;

    MP4EncRet ret;

    encOutGov.strmSize = 0;

    // Planar YCbCr 4:2:0
    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU =
      frame->fb_bus_address +
      (this->origWidth * this->origHeight);
    this->encIn.busChromaV =
      this->encIn.busChromaU +
      (this->origWidth * this->origHeight / 4);
    // Semiplanar YCbCr 4:2:0
    // Interleaved YCbCr 4:2:2

    // Optimal size for output buffer size is the size of the VBV buffer (as specified by standard)
    this->encIn.pOutBuf = (u32 *) stream->bus_data;
    this->encIn.outBufBusAddress = stream->bus_address;
    this->encIn.outBufSize = stream->buf_max_size;

    OMX_BOOL bInsIFrame;
    bInsIFrame = OMX_FALSE;

    // GOV header insertion
    if (this->nPFrames > 0)
    {
        this->nFrameCounter++;
        if ((this->nFrameCounter % this->nPFrames) == 0)
        {
            if (this->bGovEnabled)
            {
                MP4EncCodingCtrl coding_ctrl;
                ret = MP4EncGetCodingCtrl(this->instance, &coding_ctrl);

                if (ret == ENC_OK)
                {
                    coding_ctrl.insGOV = 1;
                    ret = MP4EncSetCodingCtrl(this->instance, &coding_ctrl);
                    if (ret == ENC_OK)
                    {
                        ret = MP4EncStrmEncode(this->instance, &this->encIn, &encOutGov);
                        if (ret == ENC_GOV_READY)
                        {
                            this->encIn.pOutBuf += encOutGov.strmSize;
                            this->encIn.outBufBusAddress += encOutGov.strmSize;
                            this->encIn.outBufBusAddress &= ~7;

                        }
                    }
                    coding_ctrl.insGOV = 0;
                }
            }
            else
            {
                // GOV not enabled, but insert I frame anyway.
                bInsIFrame = OMX_TRUE;
            }
        }
    }

    if (frame->frame_type == OMX_INTRA_FRAME || bInsIFrame)
    {
        this->encIn.vopType = INTRA_VOP;
    }
    else if (frame->frame_type == OMX_PREDICTED_FRAME)
    {
        this->encIn.vopType = PREDICTED_VOP;
    }
    else
    {
        DBGT_CRITICAL("Unknown frame type");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSUPPORTED_SETTING;
    }

    // The time stamp of the frame relative to the last encoded frame. This is given in
    // ticks. A zero value is usually used for the very first frame.
    // Valid value range: [0, 127*time resolution]
    this->encIn.timeIncr = this->nTickIncrement;

    // Pointer to a buffer where the individual video package’s sizes will be returned. Set
    // to NULL if the information is not needed. This buffer has to have sizeof(u32)
    // bytes for each macroblock of the encoded picture. A zero value is written after the
    // last relevant VP size (when buffer big enough). Maximum size of 14404 bytes for
    // HD 720p resolution (when sizeof(u32) = 4).
    this->encIn.pVpBuf = NULL;

    // This is the size in bytes of the video package size buffer described above.
    this->encIn.vpBufSize = 0;

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

    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    DBGT_PDEBUG("encIn.busLumaStab 0x%08lx", this->encIn.busLumaStab);
    DBGT_PDEBUG("Frame type %s", (this->encIn.vopType == INTRA_VOP) ? "I":"P");

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncr = 0;
    }

    MP4EncRateCtrl rate_ctrl;
    ret = MP4EncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == ENC_OK && (rate_ctrl.bitPerSecond != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = MP4EncSetRateCtrl(this->instance, &rate_ctrl);
    }

    if (ret == ENC_OK)
    {
        ret = MP4EncStrmEncode(this->instance, &this->encIn, &encOut);
    }
    else
    {
        DBGT_CRITICAL("MP4EncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case ENC_VOP_READY:
            this->nTotalFrames++;
            stream->streamlen = encOut.strmSize + encOutGov.strmSize;

            if (encOut.vopType == INTRA_VOP)
            {
                stat = CODEC_CODED_INTRA;
                this->nFrameCounter = 0;
            }
            else if (encOut.vopType == PREDICTED_VOP)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case ENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case ENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case ENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case ENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case ENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case ENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case ENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case ENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case ENC_HW_RESERVED:
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
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_mpeg4(const MPEG4_CONFIG* params)
{
    DBGT_PROLOG("");
    MP4EncCfg cfg;
    MP4EncApiVersion apiVer;
    MP4EncBuild encBuild;

    cfg.strmType = MPEG4_PLAIN_STRM;

    if (params->mp4_config.bSVH)
    {
        cfg.strmType = MPEG4_SVH_STRM;
    }
    else
    {
        if (params->mp4_config.bReversibleVLC)
        {
            cfg.strmType = MPEG4_VP_DP_RVLC_STRM;
        }
        else if (params->error_ctrl_config.bEnableDataPartitioning)
        {
            cfg.strmType = MPEG4_VP_DP_STRM;
        }
        else if (params->error_ctrl_config.bEnableResync)
        {
            cfg.strmType = MPEG4_VP_STRM;
        }
    }

    // Find out correct profile and level
    switch (params->mp4_config.eProfile)
    {
        case OMX_VIDEO_MPEG4ProfileSimple:
        {
            switch (params->mp4_config.eLevel)
            {
                case OMX_VIDEO_MPEG4Level0:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_0;
                    break;
                case OMX_VIDEO_MPEG4Level0b:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_0B;
                    break;
                case OMX_VIDEO_MPEG4Level1:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_1;
                    break;
                case OMX_VIDEO_MPEG4Level2:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_2;
                    break;
                case OMX_VIDEO_MPEG4Level3:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_3;
                    break;
                case OMX_VIDEO_MPEG4Level4a:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_4A;
                    break;
                case OMX_VIDEO_MPEG4Level5:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_5;
                case OMX_VIDEO_MPEG4LevelMax:
                    cfg.profileAndLevel = MPEG4_SIMPLE_PROFILE_LEVEL_6;
                    break;
                default:
                    DBGT_CRITICAL("Simple profile: Unsupported encoding level 0x%x",
                        params->mp4_config.eLevel);
                    DBGT_EPILOG("");
                    return NULL;
                    break;
            }
        }
        break;
        case OMX_VIDEO_MPEG4ProfileAdvancedSimple:
        {
            switch (params->mp4_config.eLevel)
            {
                case OMX_VIDEO_MPEG4Level3:
                    cfg.profileAndLevel = MPEG4_ADV_SIMPLE_PROFILE_LEVEL_3;
                    break;
                case OMX_VIDEO_MPEG4Level4:
                    cfg.profileAndLevel = MPEG4_ADV_SIMPLE_PROFILE_LEVEL_4;
                    break;
                case OMX_VIDEO_MPEG4Level5:
                case OMX_VIDEO_MPEG4LevelMax:
                    cfg.profileAndLevel = MPEG4_ADV_SIMPLE_PROFILE_LEVEL_5;
                    break;
                default:
                    DBGT_CRITICAL("Advanced simple profile: Unsupported encoding level 0x%x",
                        params->mp4_config.eLevel);
                    DBGT_EPILOG("");
                    return NULL;
                    break;
            }
        }
        break;
        case OMX_VIDEO_MPEG4ProfileMain:
        case OMX_VIDEO_MPEG4ProfileMax:
        {
            switch (params->mp4_config.eLevel)
            {
                case OMX_VIDEO_MPEG4Level4:
                case OMX_VIDEO_MPEG4LevelMax:
                    cfg.profileAndLevel = MPEG4_MAIN_PROFILE_LEVEL_4;
                    break;
                default:
                    DBGT_CRITICAL("Main profile: Unsupported encoding level 0x%x",
                        params->mp4_config.eLevel);
                    DBGT_EPILOG("");
                    return NULL;
                    break;
            }
        }
        break;
        default:
            DBGT_CRITICAL("Unsupported encoding profile %d", params->mp4_config.eProfile);
            DBGT_EPILOG("");
            return NULL;
    }

    cfg.width = params->common_config.nOutputWidth;
    cfg.height = params->common_config.nOutputHeight;

    // The numerator part of the input frame rate. The frame rate is defined by the
    // frmRateNum/frmRateDenom ratio. This value is also used as the time resolution or
    // the number of subunits (ticks) within a second.
    // Valid value range: [1, 65535]
    cfg.frmRateNum = params->mp4_config.nTimeIncRes;

    // The denominator part of the input frame rate. This value has to be equal or less
    // than the numerator part frmRateNum.
    // Valid value range: [1, 65535]
    cfg.frmRateDenom = cfg.frmRateNum / Q16_FLOAT(params->common_config.nInputFramerate);

    ENCODER_MPEG4* this = OSAL_Malloc(sizeof(ENCODER_MPEG4));

    this->instance = 0;
    memset(&this->encIn, 0, sizeof(MP4EncIn));
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->base.stream_start = encoder_stream_start_mpeg4;
    this->base.stream_end = encoder_stream_end_mpeg4;
    this->base.encode = encoder_encode_mpeg4;
    this->base.destroy = encoder_destroy_mpeg4;

    // calculate per frame tick increment
    this->nTickIncrement = cfg.frmRateDenom;

    apiVer = MP4EncGetApiVersion();
    encBuild = MP4EncGetBuild();
    DBGT_PDEBUG("Mpeg4 encoder API version %d.%d", apiVer.major, apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    MP4EncRet ret = MP4EncInit(&cfg, &this->instance);

    // Setup coding control
    if (ret == ENC_OK)
    {
        MP4EncCodingCtrl coding_ctrl;
        ret = MP4EncGetCodingCtrl(this->instance, &coding_ctrl);

        if (ret == ENC_OK)
        {
            // Header extension codes enable/disable
            // Not supported, use defaults.
            if (params->error_ctrl_config.bEnableHEC)
            {
                coding_ctrl.insHEC = 1;
            }
            else if (params->mp4_config.nHeaderExtension > 0)
            {
                coding_ctrl.insHEC = 1;
            }
            else
            {
                coding_ctrl.insHEC = 0;
            }

            // GOV header insertion enable/disable
            if ((params->mp4_config.bGov) && (params->mp4_config.nPFrames > 0))
            {
                coding_ctrl.insGOV = 0;
                this->bGovEnabled = OMX_TRUE;
            }
            else
            {
                coding_ctrl.insGOV = 0;
                this->bGovEnabled = OMX_FALSE;
            }
            this->nFrameCounter = 0;

            if (params->mp4_config.nPFrames > 0)
            {
                this->nPFrames = params->mp4_config.nPFrames;
            }

            // Video package (VP) size
            if (params->error_ctrl_config.nResynchMarkerSpacing > 0)
            {
                coding_ctrl.vpSize = params->error_ctrl_config.nResynchMarkerSpacing;
            }
            else if (params->mp4_config.nMaxPacketSize > 0)
            {
                coding_ctrl.vpSize = params->mp4_config.nMaxPacketSize;
            }
            // else use default value

            ret = MP4EncSetCodingCtrl(this->instance, &coding_ctrl);
        }
        else
        {
            DBGT_CRITICAL("MP4EncGetCodingCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("MP4EncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    if (ret == ENC_OK)
    {
        MP4EncRateCtrl rate_ctrl;
        ret = MP4EncGetRateCtrl(this->instance, &rate_ctrl);
        if (ret == ENC_OK)
        {
            rate_ctrl.gopLen = params->mp4_config.nPFrames;
            // Optional. Set to -1 to use default.
            if (params->rate_config.nPictureRcEnabled >= 0)
            {
                rate_ctrl.vopRc = params->rate_config.nPictureRcEnabled;
            }

            // Optional. Set to -1 to use default.
            if (params->rate_config.nMbRcEnabled >= 0)
            {
                rate_ctrl.mbRc = params->rate_config.nMbRcEnabled;
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

            // Optional. Set to -1 to use default.
            if ((OMX_S32)params->rate_config.nVbvEnabled >= 0)
            {
                rate_ctrl.vbv = params->rate_config.nVbvEnabled;
            }

            switch (params->rate_config.eRateControl)
            {
                case OMX_Video_ControlRateDisable:
                    rate_ctrl.vopSkip = 0;
                    break;
                case OMX_Video_ControlRateVariable:
                    rate_ctrl.vopSkip = 0;
                    break;
                case OMX_Video_ControlRateConstant:
                    rate_ctrl.vopSkip = 0;
                    break;
                case OMX_Video_ControlRateVariableSkipFrames:
                    rate_ctrl.vopSkip = 1;
                    break;
                case OMX_Video_ControlRateConstantSkipFrames:
                    rate_ctrl.vopSkip = 1;
                    break;
                case OMX_Video_ControlRateMax:
                    rate_ctrl.vopSkip = 0;
                    break;
                default:
                    break;
            }
            ret = MP4EncSetRateCtrl(this->instance, &rate_ctrl);
        }
        else
        {
            DBGT_CRITICAL("MP4EncGetRateCtrl failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
    }
    else
    {
        DBGT_CRITICAL("MP4EncSetCodingCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    if (ret == ENC_OK)
    {
        MP4EncPreProcessingCfg pp_config;

        ret = MP4EncGetPreProcessing(this->instance, &pp_config);

        if (ret != ENC_OK)
        {
            DBGT_CRITICAL("MP4EncGetPreProcessing failed! (%d)", ret);
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
#ifdef ENC6280
                pp_config.yuvType = ENC_YUV420_PLANAR;
#endif
#ifdef ENC7280
                pp_config.inputType = ENC_YUV420_PLANAR;
#endif
                break;
            case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
#ifdef ENC6280
            	pp_config.yuvType = ENC_YUV420_SEMIPLANAR;
#endif
#ifdef ENC7280
            	pp_config.inputType = ENC_YUV420_SEMIPLANAR;
#endif
                break;
            case OMX_COLOR_FormatYCbYCr:
#ifdef ENC6280
            	pp_config.yuvType = ENC_YUV422_INTERLEAVED_YUYV;
#endif
#ifdef ENC7280
            	pp_config.inputType = ENC_YUV422_INTERLEAVED_YUYV;
#endif
                break;
            case OMX_COLOR_FormatCbYCrY:
#ifdef ENC6280
            	pp_config.yuvType = ENC_YUV422_INTERLEAVED_UYVY;
#endif
#ifdef ENC7280
            	pp_config.inputType = ENC_YUV422_INTERLEAVED_UYVY;
#endif
                break;
            case OMX_COLOR_Format16bitRGB565:
#ifdef ENC6280
                pp_config.yuvType = ENC_RGB565;
#endif
#ifdef ENC7280
                pp_config.inputType = ENC_RGB565;
#endif
                break;
            case OMX_COLOR_Format16bitBGR565:
#ifdef ENC6280
                pp_config.yuvType = ENC_BGR565;
#endif
#ifdef ENC7280
                pp_config.inputType = ENC_BGR565;
#endif
                break;
            case OMX_COLOR_Format16bitARGB4444:
#ifdef ENC6280
                pp_config.yuvType = ENC_RGB444;
#endif
#ifdef ENC7280
                pp_config.inputType = ENC_RGB444;
#endif
                break;
            case OMX_COLOR_Format16bitARGB1555:
#ifdef ENC6280
                pp_config.yuvType = ENC_RGB555;
#endif
#ifdef ENC7280
                pp_config.inputType = ENC_RGB555;
#endif
                break;
            default:
                DBGT_CRITICAL("Unknown color format");
                ret = ENC_INVALID_ARGUMENT;
                break;
        }

        switch (params->pp_config.angle)
        {
            case 0:
                pp_config.rotation = ENC_ROTATE_0;
                break;
            case 90:
                pp_config.rotation = ENC_ROTATE_90R;
                break;
            case 270:
                pp_config.rotation = ENC_ROTATE_90L;
                break;
            default:
                DBGT_CRITICAL("Unsupported rotation angle");
                ret = ENC_INVALID_ARGUMENT;
                break;
        }

        // Enables or disables the video stabilization function. Set to a non-zero value will
        // enable the stabilization. The input image’s dimensions (origWidth, origHeight)
        // have to be at least 8 pixels bigger than the final encoded image’s. Also when
        // enabled the cropping offset (xOffset, yOffset) values are ignored.
        this->bStabilize = params->pp_config.frameStabilization;
        pp_config.videoStabilization = params->pp_config.frameStabilization;

        if (ret == ENC_OK)
        {
            ret = MP4EncSetPreProcessing(this->instance, &pp_config);
        }
    }
    else
    {
        DBGT_CRITICAL("MP4EncSetRateCtrl failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (ret != ENC_OK)
    {
        DBGT_CRITICAL("MP4EncSetPreProcessing failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_mpeg4(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_MPEG4* this = (ENCODER_MPEG4*)arg;

    this->nTickIncrement = (OMX_U32) (TIME_RESOLUTION_MPEG4 / Q16_FLOAT(xFramerate));

    DBGT_EPILOG("");
    return CODEC_OK;
}
