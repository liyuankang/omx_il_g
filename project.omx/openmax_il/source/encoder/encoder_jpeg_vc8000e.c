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

#include "encoder_jpeg_vc8000e.h"
#include "util.h"
#include "jpegencapi.h"
#include "OSAL.h"
#include <string.h>
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX JPEG"

#if !defined (ENC6280) && !defined (ENC7280) && !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1) && !defined (ENCVC8000E)
#error "SPECIFY AN ENCODER PRODUCT (ENC6280, ENC7280, ENC8270, ENC8290 OR ENCH1) IN COMPILER DEFINES!"
#endif
#define VC8000E_API_VER (1)
#if VC8000E_API_VER == 1
#define API_EWLGetCoreNum(A)       EWLGetCoreNum()
#define API_JpegEncGetBuild(A, B)  JpegEncGetBuild((A))
#define API_JpegEncInit(A, B, C)   JpegEncInit((A), (B))
#else
#define API_EWLGetCoreNum(A)       EWLGetCoreNum(A)
#define API_JpegEncGetBuild(A, B)  JpegEncGetBuild((A), (B))
#define API_JpegEncInit(A, B, C)   JpegEncInit((A), (B), (C))
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

  JpegEncFrameType inputFormat;
  JpegEncPictureRotation rotation;
  JpegEncCodingMode codingMode;

  OMX_U32 exp_of_input_alignment;
} ENCODER_JPEG;


/*------------------------------------------------------------------------------
   get picture size
------------------------------------------------------------------------------*/
static void _vc8000e_getAlignedPicSizebyFormat(JpegEncFrameType type,u32 width, u32 height, u32 alignment,
                                       u64 *luma_Size,u64 *chroma_Size,u64 *picture_Size)
{
    u32 luma_stride=0, chroma_stride = 0;
    u64 lumaSize = 0, chromaSize = 0, pictureSize = 0;

    JpegEncGetAlignedStride(width,type, &luma_stride, &chroma_stride, alignment);
    switch(type)
    {
        case JPEGENC_YUV420_PLANAR:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2*2;
            break;

        case JPEGENC_YUV420_SEMIPLANAR:
        case JPEGENC_YUV420_SEMIPLANAR_VU:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2;
            break;

        case JPEGENC_YUV422_INTERLEAVED_YUYV:
        case JPEGENC_YUV422_INTERLEAVED_UYVY:
        case JPEGENC_RGB565:
        case JPEGENC_BGR565:
        case JPEGENC_RGB555:
        case JPEGENC_BGR555:
        case JPEGENC_RGB444:
        case JPEGENC_BGR444:
        case JPEGENC_RGB888:
        case JPEGENC_BGR888:
        case JPEGENC_RGB101010:
        case JPEGENC_BGR101010:
            lumaSize = luma_stride * height;
            chromaSize = 0;
            break;

        case JPEGENC_YUV420_I010:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2*2;
            break;

        case JPEGENC_YUV420_MS_P010:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2;
            break;

        case JPEGENC_YUV420_8BIT_DAHUA_HEVC:
            lumaSize = luma_stride * height;
            chromaSize = lumaSize/2;
            break;

        case JPEGENC_YUV420_8BIT_DAHUA_H264:
            lumaSize = luma_stride * height * 2* 12/ 8;
            chromaSize = 0;
            break; 

        case JPEGENC_YUV422_888:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height;
            break;

        default:
            chromaSize = lumaSize = 0;
            break;
    }

    pictureSize = lumaSize + chromaSize;
    if (luma_Size != NULL)    *luma_Size = lumaSize;
    if (chroma_Size != NULL)  *chroma_Size = chromaSize;
    if (picture_Size != NULL) *picture_Size = pictureSize;
}

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

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

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

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

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
    u64 height, luma_size = 0, chroma_size = 0;

    this->encIn.frameHeader = this->frameHeader ? 1 : 0;
    height = this->origHeight;

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

        height = tmpSliceHeight;
    }

    _vc8000e_getAlignedPicSizebyFormat(this->inputFormat, this->origWidth, height, 1<<this->exp_of_input_alignment, &luma_size, &chroma_size, NULL);
    this->encIn.busLum = frame->fb_bus_address;
    this->encIn.busCb = this->encIn.busLum + luma_size; // Cb or U
    this->encIn.busCr = this->encIn.busCb + chroma_size/2; // Cr or V
    this->encIn.pLum = (u8 *)frame->fb_bus_data;
    this->encIn.pCb = this->encIn.pLum + luma_size;
    this->encIn.pCr = this->encIn.pCb + chroma_size/2;

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

    JpegEncOut encOut;

    JpegEncRet ret = JpegEncEncode(this->instance, &this->encIn, &encOut);

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

static int _vc8000e_jpeg_parse_input_format(OMX_COLOR_FORMATTYPE omx_format, ENCODER_JPEG *h)
{
    switch (omx_format)
    {
        case OMX_COLOR_FormatYUV420PackedPlanar:
        case OMX_COLOR_FormatYUV420Planar:
            h->inputFormat = JPEGENC_YUV420_PLANAR;
            break;
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            h->inputFormat = JPEGENC_YUV420_SEMIPLANAR;
            break;
#if !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1) && !defined (ENCVC8000E)
        case OMX_COLOR_FormatYUV422Planar:
            h->inputFormat = JPEGENC_YUV422_PLANAR;
            h->codingMode = JPEGENC_422_MODE;
            break;
#endif
        case OMX_COLOR_FormatYCbYCr:
            h->inputFormat = JPEGENC_YUV422_INTERLEAVED_YUYV;
            break;
        case OMX_COLOR_FormatCbYCrY:
            h->inputFormat = JPEGENC_YUV422_INTERLEAVED_UYVY;
            break;
        case OMX_COLOR_Format16bitRGB565:
            h->inputFormat = JPEGENC_RGB565;
            break;
        case OMX_COLOR_Format16bitBGR565:
            h->inputFormat = JPEGENC_BGR565;
            break;
        case OMX_COLOR_Format16bitARGB4444:
            h->inputFormat = JPEGENC_RGB444;
            break;
        case OMX_COLOR_Format16bitARGB1555:
            h->inputFormat = JPEGENC_RGB555;
            break;
#if defined (ENC8290) || defined (ENCH1) || defined (ENCVC8000E)
        case OMX_COLOR_Format12bitRGB444:
            h->inputFormat = JPEGENC_RGB444;
            break;
        case OMX_COLOR_Format25bitARGB1888:
        case OMX_COLOR_Format32bitARGB8888:
            h->inputFormat = JPEGENC_RGB888;
            break;
#endif
        default:
            DBGT_CRITICAL("Unknown color format");
            return -1;
    }

    return 0;
}

static int _vc8000e_jpeg_parse_params(ENCODER_JPEG *h, const JPEG_CONFIG* params)
{
    h->croppedWidth = params->codingWidth;
    h->croppedHeight = params->codingHeight;
    h->exp_of_input_alignment = 0;

    if(_vc8000e_jpeg_parse_input_format(params->pp_config.formatType, h))
    {
        return -1;
    }

    switch (params->pp_config.angle)
    {
        case 0:
            h->rotation = JPEGENC_ROTATE_0;
            break;
        case 90:
            h->rotation = JPEGENC_ROTATE_90R;
            break;
#ifdef ENCVC8000E
        case 180:
            h->rotation = JPEGENC_ROTATE_180;
            break;
#endif
        case 270:
            h->rotation = JPEGENC_ROTATE_90L;
            break;
        default:
            DBGT_CRITICAL("Unsupported rotation angle");
            return -1;
    }

    return 0;
}

// Create JPEG codec instance and initialize it.
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_jpeg(const JPEG_CONFIG* params)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(params);

    u32 i;
    JpegEncCfg cfg;
    JpegEncRet ret;
    JpegEncApiVersion apiVer;
    JpegEncBuild encBuild;
    ENCODER_JPEG* this = OSAL_Malloc(sizeof(ENCODER_JPEG));

    memset(this, 0, sizeof(ENCODER_JPEG));
    this->codingMode = JPEGENC_420_MODE;
    if(_vc8000e_jpeg_parse_params(this, params))
    {
        DBGT_CRITICAL("Create jpeg encoder failed -- parameters not supported!");
        OSAL_Free((OSAL_PTR *)this);
        DBGT_EPILOG("");
        return NULL;
    }

    memset(&cfg, 0, sizeof(JpegEncCfg));

    //set default values -- for configs which no params passed  from OMX
    cfg.losslessEn = 0;
    cfg.predictMode = 0;
    cfg.ptransValue = 0;
    cfg.mirror = 0;
    cfg.colorConversion.type = 0;       /*JPEGENC_RGBTOYUV_BT601*/
    cfg.inputLineBufEn = 0;
    cfg.inputLineBufLoopBackEn = 0;
    cfg.inputLineBufDepth = 1;
    cfg.amountPerLoopBack = 0;
    cfg.inputLineBufHwModeEn = 0;
    cfg.inputLineBufCbFunc = NULL;
    cfg.inputLineBufCbData = NULL;
    cfg.streamMultiSegmentMode = 0;
    cfg.streamMultiSegmentAmount = 4;
    cfg.streamMultiSegCbFunc = NULL;
    cfg.streamMultiSegCbData = NULL;
    /* constant chroma control */
    cfg.constChromaEn = 0;
    cfg.constCb = 0x80;
    cfg.constCr = 0x80;
    /* jpeg rc*/
    cfg.targetBitPerSecond = 0;
    cfg.frameRateNum = 1;
    cfg.frameRateDenom = 1;
    cfg.qpmin = 0; 
    cfg.qpmax = 51; 
    cfg.fixedQP = -1; 
    cfg.rcMode   = 1;
    cfg.picQpDeltaMax = 3;
    cfg.picQpDeltaMin = -2;
    cfg.comLength = 0;

    cfg.exp_of_input_alignment = this->exp_of_input_alignment;
    cfg.qLevel = params->qLevel; // quantization level.
    cfg.frameType = this->inputFormat;
    cfg.rotation = this->rotation;
    cfg.codingMode = this->codingMode;

#if defined (ENC8270) || defined (ENC8290) || defined (ENCH1) || defined (ENCVC8000E)
    cfg.qTableLuma = NULL;
    cfg.qTableChroma = NULL;
#endif

    if (params->bAddHeaders)
    {
        cfg.unitsType = params->unitsType;
        cfg.xDensity = params->xDensity;
        cfg.yDensity = params->yDensity;
        cfg.markerType = params->markerType;
        cfg.comLength = strlen(params->stringCommentMarker);
        cfg.pCom = params->stringCommentMarker;
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

#if !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1) && !defined (ENCVC8000E)
    cfg.thumbnail = 0;  // no thumbnails
    cfg.cfgThumb.inputWidth = 0;
    cfg.cfgThumb.inputHeight = 0;
    cfg.cfgThumb.xOffset = 0;
    cfg.cfgThumb.yOffset = 0;
    cfg.cfgThumb.codingWidth = 0;
#endif

    // set encoder mode parameters
#ifdef ENCVC8000E
    cfg.inputWidth = (params->pp_config.origWidth + 15) & (~15);
#else
    cfg.inputWidth = params->pp_config.origWidth;
#endif
    cfg.inputHeight = params->pp_config.origHeight;
    cfg.xOffset = params->pp_config.xOffset;
    cfg.yOffset = params->pp_config.yOffset;

    cfg.codingType = params->codingType;

    cfg.codingWidth = params->codingWidth;
    cfg.codingHeight = params->codingHeight;

    // encIn struct init
    this->instance = 0;
    this->origWidth = params->pp_config.origWidth;
    this->origHeight = params->pp_config.origHeight;
    this->frameHeader = params->bAddHeaders;
    this->sliceNumber = 1;
    this->leftoverCrop = OMX_FALSE;
    this->frameType = params->pp_config.formatType;

    // initialize static methods
    this->base.stream_start = encoder_stream_start_jpeg;
    this->base.stream_end = encoder_stream_end_jpeg;
    this->base.encode = encoder_encode_jpeg;
    this->base.destroy = encoder_destroy_jpeg;

    // slice mode configuration
    if (params->codingType != JPEGENC_WHOLE_FRAME)
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
    DBGT_PDEBUG("Jpeg encoder API version %d.%d", apiVer.major, apiVer.minor);

    for (i = 0; i< API_EWLGetCoreNum(NULL); i++)
    {
        encBuild = API_JpegEncGetBuild(i, NULL);
        DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
             encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
             encBuild.hwBuild, encBuild.swBuild / 1000000,
             (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
    }

    ret = API_JpegEncInit(&cfg, &this->instance, NULL);

    if (ret != JPEGENC_OK)
    {
        DBGT_CRITICAL("JpegEncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

#if defined (ENC8270) || defined (ENC8290) || defined (ENCH1) || defined (ENCVC8000E)
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
    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

