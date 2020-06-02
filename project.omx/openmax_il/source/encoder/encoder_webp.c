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

#include "encoder_webp.h"
#include "util.h"
#include "vp8encapi.h"
#include "OSAL.h"
#include <string.h>
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX WEBP"

#if !defined (ENCH1)
#error "SPECIFY AN ENCODER PRODUCT (ENCH1) IN COMPILER DEFINES!"
#endif

typedef struct ENCODER_WEBP
{
  ENCODER_PROTOTYPE base;

  VP8EncInst instance;
  VP8EncIn encIn;
  OMX_U32 origWidth;
  OMX_U32 origHeight;
  OMX_U32 croppedWidth;
  OMX_U32 croppedHeight;
  //OMX_BOOL frameHeader;
  OMX_BOOL leftoverCrop;
  OMX_BOOL sliceMode;
  OMX_U32 sliceHeight;
  OMX_U32 sliceNumber;
  OMX_COLOR_FORMATTYPE frameType;
} ENCODER_WEBP;


//! Destroy codec instance.
static void encoder_destroy_webp(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_WEBP* this = (ENCODER_WEBP*)arg;

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

// WEBP does not support streaming.
static CODEC_STATE encoder_stream_start_webp(ENCODER_PROTOTYPE* arg,
                                                STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(stream);
    DBGT_EPILOG("");

    return CODEC_OK;
}

static CODEC_STATE encoder_stream_end_webp(ENCODER_PROTOTYPE* arg,
                                                STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(stream);
    DBGT_EPILOG("");

    return CODEC_OK;
}


// Encode a simple raw frame using underlying hantro codec.
// Converts OpenMAX structures to corresponding Hantro codec structures,
// and calls API methods to encode frame.
// Constraints:
// 1. only full frame encoding is supported, i.e., no thumbnails

static CODEC_STATE encoder_encode_webp(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);
    UNUSED_PARAMETER(cfg);

    ENCODER_WEBP* this = (ENCODER_WEBP*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    OMX_U32 i;

#if 0 //Not supported
    if (this->sliceMode)
    {
        OMX_U32 tmpSliceHeight = this->sliceHeight;

        // Last slice may be smaller so calculate slice height.
        if ((this->sliceHeight * this->sliceNumber) > this->origHeight)
        {
            tmpSliceHeight = this->origHeight - ((this->sliceNumber - 1) * this->sliceHeight);
        }

        if (this->leftoverCrop == OMX_TRUE)
        {
            /*  If picture full (Enough slices) start encoding again */
            if ((this->sliceHeight * this->sliceNumber) >= this->origHeight)
            {
                this->leftoverCrop = OMX_FALSE;
                this->sliceNumber = 1;
            }
            else
            {
                stream->streamlen = 0;
                return CODEC_OK;
            }
        }

        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            if ((tmpSliceHeight % 8) != 0)
            {
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }
        else
        {
            if ((tmpSliceHeight % 16) != 0)
            {
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }

        this->encIn.busLuma = frame->fb_bus_address;
        this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth * tmpSliceHeight); // Cb or U
        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth * tmpSliceHeight / 2); // Cr or V
        }
        else
        {
            this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth * tmpSliceHeight / 4); // Cr or V
        }
    }
    else
    {
        this->encIn.busLuma = frame->fb_bus_address;
        this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth * this->origHeight); // Cb or U
        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth * this->origHeight / 2); // Cr or V
        }
        else
        {
            this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth * this->origHeight / 4); // Cr or V
        }
    }
#endif

    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->origWidth
            * this->origHeight);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->origWidth
            * this->origHeight / 4);
    this->encIn.timeIncrement = 1;
    this->encIn.codingType = VP8ENC_INTRA_FRAME;

    this->encIn.pOutBuf = (u32 *) stream->bus_data; // output stream buffer
    this->encIn.busOutBuf = stream->bus_address; // output bus address
    this->encIn.outBufSize = stream->buf_max_size;

    VP8EncOut encOut;

    VP8EncRet ret = VP8EncStrmEncode(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
#if 0 //Not supported
        case VP8ENC_RESTART_INTERVAL:
            // return encoded slice
            stream->streamlen = encOut.jfifSize;
            stat = CODEC_CODED_SLICE;
            this->sliceNumber++;
            break;
#endif
        case VP8ENC_FRAME_READY:
#if 0 //Not supported
            if ((this->sliceMode == OMX_TRUE)
            && (this->sliceHeight * this->sliceNumber) >= this->croppedHeight)
            {
                this->leftoverCrop  = OMX_TRUE;
            }
#endif
            stream->streamlen = encOut.frameSize;
            //DBGT_PDEBUG("Frame size %d", stream->streamlen);

            // set each partition pointers and sizes
            for(i = 0; i < 9; i++)
            {
                stream->pOutBuf[i] = (OMX_U32 *)encOut.pOutBuf[i];
                stream->streamSize[i] = encOut.streamSize[i];
            }
            stat = CODEC_OK;
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

// Create WEBP codec instance and initialize it.
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_webp(const WEBP_CONFIG* params)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(params);

    VP8EncConfig cfg;
    VP8EncRet ret;
    VP8EncApiVersion apiVer;
    VP8EncBuild encBuild;

    i32 qValues[10] = {120,108,96,84,72,60,48,24,12,0}; //VP8 Quantization levels

    memset(&cfg, 0, sizeof(VP8EncConfig));
    cfg.width = params->codingWidth;
    cfg.height = params->codingHeight;
    cfg.frameRateDenom = 1;
    cfg.frameRateNum = 1;
    cfg.refFrameAmount = 1;

    ENCODER_WEBP* this = OSAL_Malloc(sizeof(ENCODER_WEBP));

    this->croppedWidth = params->codingWidth;
    this->croppedHeight = params->codingHeight;

    // encIn struct init
    this->instance = 0;
    memset(&this->encIn, 0, sizeof(VP8EncIn));
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->sliceNumber = 1;
    this->leftoverCrop = 0;
    this->frameType = params->pp_config.formatType;

    // initialize static methods
    this->base.stream_start = encoder_stream_start_webp;
    this->base.stream_end = encoder_stream_end_webp;
    this->base.encode = encoder_encode_webp;
    this->base.destroy = encoder_destroy_webp;

    // slice mode configuration
#if 0 //Not supported
    if (params->codingType > 0)
    {
        if (params->sliceHeight > 0)
        {
            this->sliceMode = OMX_TRUE;
            if (this->frameType == OMX_COLOR_FormatYUV422Planar)
            {
                cfg.restartInterval = params->sliceHeight / 8;
            }
            else
            {
                cfg.restartInterval = params->sliceHeight / 16;
            }
            this->sliceHeight = params->sliceHeight;
        }
        else
        {
            ret = VP8ENC_INVALID_ARGUMENT;
        }
    }
    else
    {
        this->sliceMode = OMX_FALSE;
        cfg.restartInterval = 0;
        this->sliceHeight = 0;
    }
#endif

    this->sliceMode = OMX_FALSE;
    this->sliceHeight = 0;

    apiVer = VP8EncGetApiVersion();
    encBuild = VP8EncGetBuild();

    DBGT_PDEBUG("WebP Encoder API version %d.%d", apiVer.major,
            apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    ret = VP8EncInit(&cfg, &this->instance);

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
            coding_ctrl.interpolationFilter = 0;
            coding_ctrl.dctPartitions = 0;
            coding_ctrl.errorResilient = 0;
            coding_ctrl.quarterPixelMv = 0;

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
            if (params->qLevel == -1)
                rate_ctrl.qpHdr = -1;
            else
                rate_ctrl.qpHdr = qValues[params->qLevel];

            //rate_ctrl.pictureRc = 1;

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

        pp_config.videoStabilization = 0;

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
            case OMX_COLOR_Format16bitARGB4444:
            case OMX_COLOR_Format12bitRGB444:
                pp_config.inputType = VP8ENC_RGB444;
                break;
            case OMX_COLOR_Format16bitARGB1555:
                pp_config.inputType = VP8ENC_RGB555;
                break;
            case OMX_COLOR_Format25bitARGB1888:
            case OMX_COLOR_Format32bitARGB8888:
                pp_config.inputType = VP8ENC_RGB888;
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
