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

#include "encoder_jpeg.h"
#include "util.h"
#include "jpegencapi.h"
#include "OSAL.h"
#include <string.h>
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX JPEG"

#if !defined (ENC6280) && !defined (ENC7280) && !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1)
#error "SPECIFY AN ENCODER PRODUCT (ENC6280, ENC7280, ENC8270, ENC8290 OR ENCH1) IN COMPILER DEFINES!"
#endif

typedef struct ENCODER_JPEG
{
  ENCODER_PROTOTYPE base;

  JpegEncInst instance;
  JpegEncIn encIn;
  OMX_U32 origWidth;
  OMX_U32 origHeight;
  OMX_U32 croppedWidth;
  OMX_U32 croppedHeight;
  OMX_BOOL frameHeader;
  OMX_BOOL leftoverCrop;
  OMX_BOOL sliceMode;
  OMX_U32 sliceHeight;
  OMX_U32 sliceNumber;
  OMX_COLOR_FORMATTYPE frameType;
} ENCODER_JPEG;


//! Destroy codec instance.
static void encoder_destroy_jpeg(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;
    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            JpegEncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// JPEG does not support streaming.
static CODEC_STATE encoder_stream_start_jpeg(ENCODER_PROTOTYPE* arg, STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;

    CODEC_STATE stat = CODEC_OK;

    this->encIn.pOutBuf = (u8 *) stream->bus_data;
    if (stream->buf_max_size > 16*1024*1024)
    {
        this->encIn.outBufSize = 16*1024*1024;
    }
    else
    {
        this->encIn.outBufSize = stream->buf_max_size;
    }
    this->encIn.busOutBuf = stream->bus_address;

    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_stream_end_jpeg(ENCODER_PROTOTYPE* arg, STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;

    CODEC_STATE stat = CODEC_OK;

    this->encIn.pOutBuf = (u8 *) stream->bus_data;
    if (stream->buf_max_size > 16*1024*1024)
    {
        this->encIn.outBufSize = 16*1024*1024;
    }
    else
    {
        this->encIn.outBufSize = stream->buf_max_size;
    }
    this->encIn.busOutBuf = stream->bus_address;

    DBGT_EPILOG("");
    return stat;
}


// Encode a simple raw frame using underlying hantro codec.
// Converts OpenMAX structures to corresponding Hantro codec structures,
// and calls API methods to encode frame.
// Constraints:
// 1. only full frame encoding is supported, i.e., no thumbnails
// /param arg Codec instance
// /param frame Frame to encode
// /param stream Output stream
static CODEC_STATE encoder_encode_jpeg(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);
    UNUSED_PARAMETER(cfg);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.frameHeader = 0;
    if (this->frameHeader)
    {
        this->encIn.frameHeader = 1;
    }

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
                DBGT_EPILOG("");
                return CODEC_OK;
            }
        }

        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            if ((tmpSliceHeight % 8) != 0)
            {
                DBGT_CRITICAL("Slice height is not divisible by 8");
                DBGT_EPILOG("");
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }
        else
        {
            if ((tmpSliceHeight % 16) != 0)
            {
                DBGT_CRITICAL("Slice height is not divisible by 16");
                DBGT_EPILOG("");
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }

        this->encIn.busLum = frame->fb_bus_address;
        this->encIn.busCb = frame->fb_bus_address + (this->origWidth * tmpSliceHeight); // Cb or U
        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            this->encIn.busCr = this->encIn.busCb + (this->origWidth * tmpSliceHeight / 2); // Cr or V
        }
        else
        {
            this->encIn.busCr = this->encIn.busCb + (this->origWidth * tmpSliceHeight / 4); // Cr or V
        }
    }
    else
    {
        this->encIn.busLum = frame->fb_bus_address;
        this->encIn.busCb = frame->fb_bus_address + (this->origWidth * this->origHeight); // Cb or U
        if (this->frameType == OMX_COLOR_FormatYUV422Planar)
        {
            this->encIn.busCr = this->encIn.busCb + (this->origWidth * this->origHeight / 2); // Cr or V
        }
        else
        {
            this->encIn.busCr = this->encIn.busCb + (this->origWidth * this->origHeight / 4); // Cr or V
        }
    }

    this->encIn.pOutBuf = (u8 *) stream->bus_data; // output stream buffer
    this->encIn.busOutBuf = stream->bus_address; // output bus address
    
    //this->encIn.outBufSize = stream->buf_max_size; // max size of output buffer
    if (stream->buf_max_size > 16*1024*1024)
    {
        this->encIn.outBufSize = 16*1024*1024;
    }
    else
    {
        this->encIn.outBufSize = stream->buf_max_size;
    }

    JpegEncOut encOut;

    JpegEncRet ret = JpegEncEncode(this->instance, &this->encIn, &encOut, NULL, NULL);

    switch (ret)
    {
        case JPEGENC_RESTART_INTERVAL:
            // return encoded slice
            stream->streamlen = encOut.jfifSize;
            stat = CODEC_CODED_SLICE;
            this->sliceNumber++;
            break;
        case JPEGENC_FRAME_READY:
            stream->streamlen = encOut.jfifSize;
            stat = CODEC_OK;

            if ((this->sliceMode == OMX_TRUE) &&
                (this->sliceHeight * this->sliceNumber) >= this->croppedHeight)
            {
                this->leftoverCrop  = OMX_TRUE;
            }
            break;
        case JPEGENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case JPEGENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case JPEGENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case JPEGENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case JPEGENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case JPEGENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case JPEGENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case JPEGENC_HW_RESERVED:
            stat = CODEC_ERROR_RESERVED;
            break;
        default:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

// Create JPEG codec instance and initialize it.
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_jpeg(const JPEG_CONFIG* params)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(params);

    JpegEncCfg cfg;
    JpegEncRet ret;
    JpegEncApiVersion apiVer;
    JpegEncBuild encBuild;

    memset(&cfg, 0, sizeof(JpegEncCfg));

    cfg.qLevel = params->qLevel; // quantization level.

#if defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
    cfg.qTableLuma = NULL;
    cfg.qTableChroma = NULL;
#endif

    switch (params->pp_config.formatType)
    {
        case OMX_COLOR_FormatYUV420PackedPlanar:
        case OMX_COLOR_FormatYUV420Planar:
            cfg.frameType = JPEGENC_YUV420_PLANAR;
            break;
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            cfg.frameType = JPEGENC_YUV420_SEMIPLANAR;
            break;
#if !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1)
        case OMX_COLOR_FormatYUV422Planar:
            cfg.frameType = JPEGENC_YUV422_PLANAR;
            cfg.codingMode = JPEGENC_422_MODE;
            break;
#endif
        case OMX_COLOR_FormatYCbYCr:
            cfg.frameType = JPEGENC_YUV422_INTERLEAVED_YUYV;
            break;
        case OMX_COLOR_FormatCbYCrY:
            cfg.frameType = JPEGENC_YUV422_INTERLEAVED_UYVY;
            break;
        case OMX_COLOR_Format16bitRGB565:
            cfg.frameType = JPEGENC_RGB565;
            break;
        case OMX_COLOR_Format16bitBGR565:
            cfg.frameType = JPEGENC_BGR565;
            break;
        case OMX_COLOR_Format16bitARGB4444:
            cfg.frameType = JPEGENC_RGB444;
            break;
        case OMX_COLOR_Format16bitARGB1555:
            cfg.frameType = JPEGENC_RGB555;
            break;
#if defined (ENC8290) || defined (ENCH1)
        case OMX_COLOR_Format12bitRGB444:
            cfg.frameType = JPEGENC_RGB444;
            break;
        case OMX_COLOR_Format25bitARGB1888:
        case OMX_COLOR_Format32bitARGB8888:
            cfg.frameType = JPEGENC_RGB888;
            break;
#endif
        default:
            DBGT_CRITICAL("Unknown color format");
            ret = JPEGENC_INVALID_ARGUMENT;
            break;
    }

    switch (params->pp_config.angle)
    {
        case 0:
            cfg.rotation = JPEGENC_ROTATE_0;
            break;
        case 90:
            cfg.rotation = JPEGENC_ROTATE_90R;
            break;
        case 270:
            cfg.rotation = JPEGENC_ROTATE_90L;
            break;
        default:
            DBGT_CRITICAL("Unsupported rotation angle");
            ret = JPEGENC_INVALID_ARGUMENT;
            break;
    }

    if (params->bAddHeaders)
    {
        cfg.unitsType = params->unitsType;
        cfg.xDensity = params->xDensity;
        cfg.yDensity = params->yDensity;
        cfg.markerType = params->markerType;
        cfg.comLength = 0;  // no comment header
        cfg.pCom = 0;       // no header data
    }
    else
    {
        cfg.unitsType = JPEGENC_NO_UNITS;
        cfg.markerType = JPEGENC_SINGLE_MARKER;
        cfg.xDensity = 1;
        cfg.yDensity = 1;
        cfg.comLength = 0;  // no comment header
        cfg.pCom = 0;       // no header data
    }

    DBGT_PDEBUG("unitsType %d", (int)cfg.unitsType);
    DBGT_PDEBUG("markerType %d", (int)cfg.markerType);
    DBGT_PDEBUG("xDensity %d", (int)cfg.xDensity);
    DBGT_PDEBUG("yDensity %d", (int)cfg.yDensity);
    DBGT_PDEBUG("comLength %d", (int)cfg.comLength);
    DBGT_PDEBUG("pCom %p", cfg.pCom);

#if !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1)
    cfg.thumbnail = 0;  // no thumbnails
    cfg.cfgThumb.inputWidth = 0;
    cfg.cfgThumb.inputHeight = 0;
    cfg.cfgThumb.xOffset = 0;
    cfg.cfgThumb.yOffset = 0;
    cfg.cfgThumb.codingWidth = 0;
#endif

    // set encoder mode parameters
    cfg.inputWidth = params->pp_config.origWidth;
    cfg.inputHeight = params->pp_config.origHeight;
    cfg.xOffset = params->pp_config.xOffset;
    cfg.yOffset = params->pp_config.yOffset;

    cfg.codingType = params->codingType;

    ENCODER_JPEG* this = OSAL_Malloc(sizeof(ENCODER_JPEG));

    this->croppedWidth = cfg.codingWidth = params->codingWidth;
    this->croppedHeight = cfg.codingHeight = params->codingHeight;

    // encIn struct init
    this->instance = 0;
    memset(&this->encIn, 0, sizeof(JpegEncIn));
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->frameHeader = params->bAddHeaders;
    this->sliceNumber = 1;
    this->leftoverCrop = 0;
    this->frameType = params->pp_config.formatType;

    // initialize static methods
    this->base.stream_start = encoder_stream_start_jpeg;
    this->base.stream_end = encoder_stream_end_jpeg;
    this->base.encode = encoder_encode_jpeg;
    this->base.destroy = encoder_destroy_jpeg;

    // slice mode configuration
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
            DBGT_CRITICAL("Invalid slice height");
            ret = JPEGENC_INVALID_ARGUMENT;
        }
    }
    else
    {
        this->sliceMode = OMX_FALSE;
        cfg.restartInterval = 0;
        this->sliceHeight = 0;
    }

    apiVer = JpegEncGetApiVersion();
    encBuild = JpegEncGetBuild();
    DBGT_PDEBUG("Jpeg encoder API version %d.%d", apiVer.major, apiVer.minor);
    DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);

    ret = JpegEncInit(&cfg, &this->instance);

    if (ret != JPEGENC_OK)
    {
        DBGT_CRITICAL("JpegEncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    if (ret == JPEGENC_OK)
    {
#if defined (ENC8270) || defined (ENC8290) || defined (ENCH1)
        ret = JpegEncSetPictureSize(this->instance, &cfg);

        if (ret != JPEGENC_OK)
        {
            DBGT_CRITICAL("JpegEncSetPictureSize failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
#else
        ret = JpegEncSetFullResolutionMode(this->instance, &cfg);

        if (ret != JPEGENC_OK)
        {
            DBGT_CRITICAL("JpegEncSetFullResolutionMode failed! (%d)", ret);
            OSAL_Free(this);
            DBGT_EPILOG("");
            return NULL;
        }
#endif
    }
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

