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
#include "OSAL.h"
#include "codec_mpeg4.h"
#include "mp4decapi.h"
#include "util.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX MPEG4"
#define MAX_BUFFERS 32

/* number of frame buffers decoder should allocate 0=AUTO */
//#define FRAME_BUFFERS 3
#define FRAME_BUFFERS 0

//#define ALIGN(offset, align)    offset
#define ALIGN(offset, align)   ((((unsigned int)offset) + align-1) & ~(align-1))

typedef enum MPEG4_CODEC_STATE
{
    MPEG4_DECODE,
    MPEG4_PARSE_METADATA
} MPEG4_CODEC_STATE;

typedef struct CODEC_MPEG4
{
    CODEC_PROTOTYPE base;
    OMX_U32 framesize;
    OMX_BOOL enableDeblock;  // Needs PP pipeline mode
    MP4DecInst instance;
    OMX_BOOL update_pp_out;
    OMX_U32 picId;
    OMX_BOOL extraEosLoopDone;
    MPEG4_FORMAT format;
    MPEG4_CODEC_STATE state;
    OMX_S32 custom1Width;
    OMX_S32 custom1Height;
    OMX_BOOL dispatchOnlyFrame;
    OMX_BOOL interlaced;
    OMX_U32 out_count;
    OMX_U32 out_index_r;
    OMX_U32 out_index_w;
    OMX_U32 out_num;
    MP4DecPicture out_pic[MAX_BUFFERS];
} CODEC_MPEG4;

CODEC_STATE decoder_setframebuffer_mpeg4(CODEC_PROTOTYPE * arg, BUFFER *buff,
                                        OMX_U32 available_buffers);
CODEC_STATE decoder_pictureconsumed_mpeg4(CODEC_PROTOTYPE * arg, BUFFER *buff);
FRAME_BUFFER_INFO decoder_getframebufferinfo_mpeg4(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abort_mpeg4(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abortafter_mpeg4(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_setnoreorder_mpeg4(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder);
CODEC_STATE decoder_setinfo_mpeg4(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                  PP_UNIT_CFG *pp_args);
// destroy codec instance
static void decoder_destroy_mpeg4(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *) arg;

    if (this)
    {
        this->base.decode = 0;
        this->base.getframe = 0;
        this->base.getinfo = 0;
        this->base.destroy = 0;
        this->base.scanframe = 0;
        this->base.endofstream = 0;
        this->base.pictureconsumed = 0;
        this->base.setframebuffer = 0;
        this->base.getframebufferinfo = 0;
        this->base.abort = 0;
        this->base.abortafter = 0;
        this->base.setnoreorder = 0;
        this->base.setinfo = 0;
        if (this->instance)
        {
            MP4DecRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// try to consume stream data
static CODEC_STATE decoder_decode_mpeg4(CODEC_PROTOTYPE * arg,
                                        STREAM_BUFFER * buf, OMX_U32 * consumed,
                                        FRAME * frame)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *) arg;

    DBGT_ASSERT(this);
    DBGT_ASSERT(this->instance);
    DBGT_ASSERT(buf);
    DBGT_ASSERT(consumed);

    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    if (this->state == MPEG4_PARSE_METADATA)
    {
        DBGT_PDEBUG("PARSE_METADATA");
        this->state = MPEG4_DECODE;
        this->custom1Width =
            (buf->bus_data[0]) | (buf->bus_data[1] << 8) |
            (buf->bus_data[2] << 16) | (buf->bus_data[3] << 24);
        this->custom1Height =
            (buf->bus_data[4]) | (buf->bus_data[5] << 8) |
            (buf->bus_data[6] << 16) | (buf->bus_data[7] << 24);

        MP4DecSetCustomInfo(this->instance, this->custom1Width, this->custom1Height);
        //*consumed = buf->streamlen;
        *consumed = 8;
        frame->size = 0;
        return CODEC_HAS_INFO;
    }

    MP4DecInput input;
    MP4DecOutput output;

    memset(&input, 0, sizeof(MP4DecInput));
    memset(&output, 0, sizeof(MP4DecOutput));

    input.stream = buf->bus_data;
    input.stream_bus_address = buf->bus_address;
    input.data_len = buf->streamlen;
    input.enable_deblock = this->enableDeblock;
    frame->size = 0;
    input.pic_id = buf->picId;
    input.skip_non_reference = 0;
    DBGT_PDEBUG("MP4DecDecode\n input.pStream = %p\n input.streamBusAddress = 0x%08lx",
                input.stream, input.stream_bus_address);

    MP4DecRet ret = MP4DecDecode(this->instance, &input, &output);

    DBGT_PDEBUG("Decoding ID %lu\nMP4DecDecode returned: %d", this->picId, ret);
    DBGT_PDEBUG("dataLen = %d\n dataLeft = %d", input.data_len, output.data_left);

    switch (ret)
    {
    case MP4DEC_PIC_DECODED:
        this->picId++;
        stat = CODEC_HAS_FRAME;
        break;
    case MP4DEC_STRM_NOT_SUPPORTED:
        stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
        break;
    case MP4DEC_STRM_PROCESSED:
        stat = CODEC_NEED_MORE;
        break;
    case MP4DEC_BUF_EMPTY:
        stat = CODEC_BUFFER_EMPTY;
        break;
    case MP4DEC_VOS_END:   //NOTE: ??
        stat = CODEC_HAS_FRAME;
        this->dispatchOnlyFrame = OMX_TRUE;
        break;
    case MP4DEC_HDRS_RDY:
    case MP4DEC_DP_HDRS_RDY:
        DBGT_PDEBUG("MP4DecDecode: HDRS_RDY");
        stat = CODEC_HAS_INFO;
        break;
    case MP4DEC_WAITING_FOR_BUFFER:
    {
        DBGT_PDEBUG("Waiting for frame buffer");
        MP4DecBufferInfo info;
        do {
            ret = MP4DecGetBufferInfo(this->instance, &info);
        } while (info.buf_to_free.bus_address);
        DBGT_PDEBUG("Buffer size %d, number of buffers %d",
            info.next_buf_size, info.buf_num);
        stat = CODEC_WAITING_FRAME_BUFFER;

        // Reset output relatived parameters
        this->out_index_w = 0;
        this->out_index_r = 0;
        this->out_num = 0;
        memset(this->out_pic, 0, sizeof(MP4DecPicture)*MAX_BUFFERS);
    }
        break;
    case MP4DEC_ABORTED:
        DBGT_PDEBUG("Decoding aborted");
        *consumed = input.data_len;
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    case MP4DEC_NO_DECODING_BUFFER:
        stat = CODEC_NO_DECODING_BUFFER;
        break;
    case MP4DEC_NONREF_PIC_SKIPPED:
        DBGT_PDEBUG("Nonreference picture skipped");
        this->picId++;
        output.data_left = 0;
        stat = CODEC_PIC_SKIPPED;
        break;
    case MP4DEC_PARAM_ERROR:
        stat = CODEC_ERROR_INVALID_ARGUMENT;
        break;
    case MP4DEC_STRM_ERROR:
        stat = CODEC_ERROR_STREAM;
        break;
    case MP4DEC_NOT_INITIALIZED:
        stat = CODEC_ERROR_NOT_INITIALIZED;
        break;
    case MP4DEC_HW_BUS_ERROR:
        stat = CODEC_ERROR_HW_BUS_ERROR;
        break;
    case MP4DEC_HW_TIMEOUT:
        stat = CODEC_ERROR_HW_TIMEOUT;
        break;
    case MP4DEC_SYSTEM_ERROR:
        stat = CODEC_ERROR_SYS;
        break;
    case MP4DEC_HW_RESERVED:
        stat = CODEC_ERROR_HW_RESERVED;
        break;
    case MP4DEC_FORMAT_NOT_SUPPORTED:
        stat = CODEC_ERROR_FORMAT_NOT_SUPPORTED;
        break;
    case MP4DEC_MEMFAIL:
        stat = CODEC_ERROR_MEMFAIL;
        break;
    default:
        DBGT_ASSERT(!"unhandled MP4DecRet");
        break;
    }

    if (stat != CODEC_ERROR_UNSPECIFIED)
    {
        *consumed = input.data_len - output.data_left;
    }
    DBGT_EPILOG("");
    return stat;
}

    // get stream info
static CODEC_STATE decoder_getinfo_mpeg4(CODEC_PROTOTYPE * arg,
                                         STREAM_INFO * pkg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(pkg);

    if (this->format == MPEG4FORMAT_CUSTOM_1_3)
    {
        if ((this->custom1Width * this->custom1Height) > MAX_VIDEO_RESOLUTION)
        {
            DBGT_ERROR("Video stream resolution exceeds the supported video resolution");
            DBGT_EPILOG("");
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        }
        pkg->width = ALIGN(this->custom1Width, 16);
        pkg->height = ALIGN(this->custom1Height, 16);
        pkg->stride = ALIGN(this->custom1Width, 16);
        pkg->sliceheight = ALIGN(this->custom1Height, 16);
        pkg->framesize = pkg->width * pkg->height * 3 / 2;
    }
    else
    {
        MP4DecInfo info;

        memset(&info, 0, sizeof(MP4DecInfo));
        MP4DecRet ret = MP4DecGetInfo(this->instance, &info);

        switch (ret)
        {
        case MP4DEC_OK:
            break;
        case MP4DEC_PARAM_ERROR:
            DBGT_CRITICAL("MP4DEC_PARAM_ERROR");
            DBGT_EPILOG("");
            return CODEC_ERROR_INVALID_ARGUMENT;
        case MP4DEC_HDRS_NOT_RDY:
            DBGT_CRITICAL("MP4DEC_HDRS_NOT_RDY");
            DBGT_EPILOG("");
            return CODEC_ERROR_STREAM;
        default:
            DBGT_ASSERT(!"unhandled MP4DecRet");
            return CODEC_ERROR_UNSPECIFIED;
        }
        if ((info.frame_width * info.frame_height) > MAX_VIDEO_RESOLUTION)
        {
            DBGT_ERROR("Video stream resolution exceeds the supported video resolution");
            DBGT_EPILOG("");
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        }
        pkg->width = ALIGN(info.frame_width, 16);
        pkg->height = ALIGN(info.frame_height, 16);
        pkg->stride = ALIGN(info.frame_width, 16);
        pkg->sliceheight = ALIGN(info.frame_height, 16);
        pkg->interlaced = info.interlaced_sequence;
        this->interlaced = info.interlaced_sequence;
        pkg->framesize = pkg->width * pkg->height * 3 / 2;

        if (info.output_format == MP4DEC_TILED_YUV420)
            pkg->format = OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled;
        else
            pkg->format = OMX_COLOR_FormatYUV420PackedSemiPlanar;

        pkg->crop_available = OMX_FALSE;
        if ((info.frame_width != info.coded_width) ||
            (info.frame_height != info.coded_height))
        {
            pkg->crop_left = 0;
            pkg->crop_top = 0;
            pkg->crop_width = info.coded_width;
            pkg->crop_height = info.coded_height;
            pkg->crop_available = OMX_TRUE;
            DBGT_PDEBUG("Crop left %d, top %d, width %d, height %d", (int)pkg->crop_left,
                    (int)pkg->crop_top, (int)pkg->crop_width, (int)pkg->crop_height);
        }
    }


    MP4DecBufferInfo bufInfo;

    MP4DecGetBufferInfo(this->instance, &bufInfo);

    pkg->frame_buffers = bufInfo.buf_num;
    DBGT_PDEBUG("Frame size %lu", pkg->framesize);

    this->framesize = pkg->framesize;
    DBGT_EPILOG("");
    return CODEC_OK;
}

// get decoded frame
static CODEC_STATE decoder_getframe_mpeg4(CODEC_PROTOTYPE * arg, FRAME * frame,
                                          OMX_BOOL eos)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(frame);
    /* stream ended but we know there is picture to be outputted. We
     * have to loop one more time without "eos" and on next round
     * NextPicture is called with eos "force last output"
     */

    if (eos && (this->extraEosLoopDone == OMX_FALSE) && this->interlaced)
    {
        this->extraEosLoopDone = OMX_TRUE;
        eos = OMX_FALSE;
    }

    MP4DecPicture picture;

    memset(&picture, 0, sizeof(MP4DecPicture));

    if (this->dispatchOnlyFrame == OMX_TRUE)
    {
        eos = OMX_TRUE;
    }

    MP4DecRet ret = MP4DecNextPicture(this->instance, &picture, eos);

    switch (ret)
    {
    case MP4DEC_PIC_RDY:
        DBGT_PDEBUG("end of stream %d", eos);
        DBGT_PDEBUG("err mbs %d", picture.nbr_of_err_mbs);
        if (picture.interlaced && picture.field_picture && picture.top_field)
        {
            DBGT_EPILOG("");
            return CODEC_HAS_FRAME; // do not send first field frame
        }
        frame->fb_bus_address = picture.pictures[0].output_picture_bus_address;
        frame->fb_bus_data = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.pLumaBase = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.nLumaBusAddress = picture.pictures[0].output_picture_bus_address;
        if (picture.pictures[0].output_format == DEC_OUT_FRM_TILED_4X4 ||
            picture.pictures[0].output_format == DEC_OUT_FRM_RFC)
        {
            frame->outBufPrivate.nLumaSize = picture.pictures[0].pic_stride * picture.pictures[0].frame_height / 4;
            frame->outBufPrivate.nChromaSize = picture.pictures[0].pic_stride_ch * picture.pictures[0].frame_height / 8;
        }
        else
        {
            frame->outBufPrivate.nLumaSize = picture.pictures[0].pic_stride * picture.pictures[0].frame_height;
            frame->outBufPrivate.nChromaSize = picture.pictures[0].pic_stride_ch * picture.pictures[0].frame_height / 2;
        }
        frame->outBufPrivate.nStride = picture.pictures[0].pic_stride;
        frame->outBufPrivate.nFrameWidth = picture.pictures[0].frame_width;
        frame->outBufPrivate.nFrameHeight = picture.pictures[0].frame_height;
        frame->outBufPrivate.pChromaBase = frame->outBufPrivate.pLumaBase + frame->outBufPrivate.nLumaSize;
        frame->outBufPrivate.nChromaBusAddress = frame->outBufPrivate.nLumaBusAddress + frame->outBufPrivate.nLumaSize;
        frame->outBufPrivate.nPicId[0] = frame->outBufPrivate.nPicId[1] = picture.decode_id;
        DBGT_PDEBUG("MP4DecNextPicture: outputPictureBusAddress %lu", picture.pictures[0].output_picture_bus_address);
        DBGT_PDEBUG("MP4DecNextPicture: nChromaBusAddress %llu", frame->outBufPrivate.nChromaBusAddress);
        DBGT_PDEBUG("pLumaBase %p, pChromaBase %p", frame->outBufPrivate.pLumaBase, frame->outBufPrivate.pChromaBase);
        DBGT_PDEBUG("Luma size %lu", frame->outBufPrivate.nLumaSize);
        DBGT_PDEBUG("Chroma size %lu", frame->outBufPrivate.nChromaSize);
        frame->size = frame->outBufPrivate.nLumaSize + frame->outBufPrivate.nChromaSize;
        frame->MB_err_count = picture.nbr_of_err_mbs;

        //this->out_pic[this->out_count % MAX_BUFFERS] = picture;
        this->out_pic[this->out_index_w] = picture;
        this->out_count++;
        this->out_index_w++;
        if (this->out_index_w == MAX_BUFFERS) this->out_index_w = 0;
        this->out_num++;

        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    case MP4DEC_OK:
        DBGT_EPILOG("");
        return CODEC_OK;
    case MP4DEC_END_OF_STREAM:
        DBGT_PDEBUG("MP4DecNextPicture: End of stream");
        DBGT_EPILOG("");
        return CODEC_END_OF_STREAM;
    case MP4DEC_ABORTED:
        DBGT_PDEBUG("MP4DecNextPicture: aborted");
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    case MP4DEC_FLUSHED:
        DBGT_PDEBUG("MP4DecNextPicture: flushed");
        DBGT_EPILOG("");
        return CODEC_FLUSHED;
    case MP4DEC_PARAM_ERROR:
        DBGT_CRITICAL("MP4DEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    case MP4DEC_NOT_INITIALIZED:
        DBGT_CRITICAL("MP4DEC_NOT_INITIALIZED");
        DBGT_EPILOG("");
        return CODEC_ERROR_SYS;
    default:
        break;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

static OMX_S32 decoder_scanframe_mpeg4(CODEC_PROTOTYPE * arg, STREAM_BUFFER * buf,
                                   OMX_U32 * first, OMX_U32 * last)
{
    CALLSTACK;

    CODEC_MPEG4 *this = (CODEC_MPEG4 *) arg;

    DBGT_ASSERT(this);
#ifdef USE_SCANFRAME
    if (this->format == MPEG4FORMAT_MPEG4)
    {
        *first = 0;
        *last = 0;
        // scan for start code
        OMX_S32 i = 0;

        OMX_S32 z = 0;

        for(; i < (OMX_S32)buf->streamlen; ++i)
        {
            if (!buf->bus_data[i])
                ++z;
            else if (buf->bus_data[i] == 0x01 && z >= 2)
            {
                *first = i - z;
                break;
            }
            else
                z = 0;
        }

        for(i = buf->streamlen - 3; i >= 0; --i)
        {
            if ((buf->bus_data[i] == 0x00) &&
               (buf->bus_data[i + 1] == 0x00) && (buf->bus_data[i + 2] == 0x01))
            {
                *last = i;
                return 1;
            }
        }
        return -1;
    }
    else
    {
        *first = 0;
        *last = buf->streamlen;
        return 1;
    }
#else
    *first = 0;
    *last = buf->streamlen;
    return 1;
#endif
}


static CODEC_STATE decoder_endofstream_mpeg4(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = MP4DecEndOfStream(this->instance, 1);

    switch (ret)
    {
        case MP4DEC_OK:
            stat = CODEC_OK;
            break;
        case MP4DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MP4DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MP4DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MP4DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MP4DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MP4DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MP4DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MP4DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MP4DEC_STRM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        case MP4DEC_ABORTED:
            stat = CODEC_ABORTED;
            break;
        default:
            DBGT_PDEBUG("MP4DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled MP4DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

// create codec instance and initialize it
CODEC_PROTOTYPE *HantroHwDecOmx_decoder_create_mpeg4(const void *DWLInstance,
                                            OMX_BOOL enable_deblocking,
                                            MPEG4_FORMAT format,
                                            OMX_VIDEO_PARAM_CONFIGTYPE *conf)
{
    CALLSTACK;

    DBGT_PROLOG("");

    MP4DecStrmFmt strmFormat;

    CODEC_MPEG4 *this = OSAL_Malloc(sizeof(CODEC_MPEG4));
    MP4DecApiVersion decApi;
    MP4DecBuild decBuild;

    memset(this, 0, sizeof(CODEC_MPEG4));

    this->base.destroy = decoder_destroy_mpeg4;
    this->base.decode = decoder_decode_mpeg4;
    this->base.getinfo = decoder_getinfo_mpeg4;
    this->base.getframe = decoder_getframe_mpeg4;
    this->base.scanframe = decoder_scanframe_mpeg4;
    this->base.endofstream = decoder_endofstream_mpeg4;
    this->base.pictureconsumed = decoder_pictureconsumed_mpeg4;
    this->base.setframebuffer = decoder_setframebuffer_mpeg4;
    this->base.getframebufferinfo = decoder_getframebufferinfo_mpeg4;
    this->base.abort = decoder_abort_mpeg4;
    this->base.abortafter = decoder_abortafter_mpeg4;
    this->base.setnoreorder = decoder_setnoreorder_mpeg4;
    this->base.setinfo = decoder_setinfo_mpeg4;
    this->instance = 0;
    this->update_pp_out = OMX_FALSE;
    this->picId = 0;
    this->extraEosLoopDone = OMX_FALSE;
    this->format = format;
    this->custom1Width = 0;
    this->custom1Height = 0;
    this->state = MPEG4_DECODE;
    this->dispatchOnlyFrame = OMX_FALSE;

    if (enable_deblocking)
    {
        this->enableDeblock = OMX_TRUE;
    }

    switch (format)
    {
    case MPEG4FORMAT_MPEG4:
    case MPEG4FORMAT_H263:
        strmFormat = MP4DEC_MPEG4;
        break;
    case MPEG4FORMAT_SORENSON:
        strmFormat = MP4DEC_SORENSON;
        break;
    case MPEG4FORMAT_CUSTOM_1:
        strmFormat = MP4DEC_CUSTOM_1;
        break;
    case MPEG4FORMAT_CUSTOM_1_3:
        strmFormat = MP4DEC_CUSTOM_1;
        this->state = MPEG4_PARSE_METADATA;
        break;
    default:
        strmFormat = MP4DEC_MPEG4;
        break;
    }

    /* Print API version number */
    decApi = MP4DecGetAPIVersion();
    decBuild = MP4DecGetBuild();
    DBGT_PDEBUG("X170 Mpeg4 Decoder API v%d.%d - SW build: %d.%d - HW build: %x",
            decApi.major, decApi.minor, decBuild.sw_build >> 16,
            decBuild.sw_build & 0xFFFF, decBuild.hw_build);
    UNUSED_PARAMETER(decApi);
    UNUSED_PARAMETER(decBuild);

    enum DecDpbFlags dpbFlags = DEC_REF_FRM_RASTER_SCAN;

    if (conf->bEnableTiled)
        dpbFlags = DEC_REF_FRM_TILED_DEFAULT;

    DBGT_PDEBUG("dpbFlags 0x%x", dpbFlags);

    MP4DecRet ret = MP4DecInit(&this->instance,
                        DWLInstance,
                        strmFormat,
                        ERROR_HANDLING, FRAME_BUFFERS,
                        dpbFlags,
                        conf->bEnableAdaptiveBuffers,
                        conf->nGuardSize);

    if (ret != MP4DEC_OK)
    {
        decoder_destroy_mpeg4((CODEC_PROTOTYPE *) this);
        DBGT_CRITICAL("MP4DecInit error");
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (CODEC_PROTOTYPE *) this;
}

FRAME_BUFFER_INFO decoder_getframebufferinfo_mpeg4(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;
    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecBufferInfo info;
    FRAME_BUFFER_INFO bufInfo;
    memset(&bufInfo, 0, sizeof(FRAME_BUFFER_INFO));

    MP4DecGetBufferInfo(this->instance, &info);

    bufInfo.bufferSize = info.next_buf_size;
    bufInfo.numberOfBuffers = info.buf_num;
    DBGT_PDEBUG("bufferSize %d, numberOfBuffers %d", (int)bufInfo.bufferSize, (int)bufInfo.numberOfBuffers);

    DBGT_EPILOG("");
    return bufInfo;
}

CODEC_STATE decoder_setframebuffer_mpeg4(CODEC_PROTOTYPE * arg, BUFFER *buff, OMX_U32 available_buffers)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(available_buffers);
    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct DWLLinearMem mem;
    MP4DecBufferInfo info;
    MP4DecRet ret;

    memset(&info, 0, sizeof(MP4DecBufferInfo));

    if (info.next_buf_size > buff->allocsize)
    {
        DBGT_CRITICAL("Buffer size error. Required size %d > allocated size %d",
            (int)info.next_buf_size, (int)buff->allocsize);
        DBGT_EPILOG("");
        return CODEC_ERROR_BUFFER_SIZE;
    }

    mem.virtual_address = (u32*)buff->bus_data;
    mem.bus_address = buff->bus_address;
    mem.size = buff->allocsize;
    DBGT_PDEBUG("virtual_address %p, bus_address %lu, size %d",
    mem.virtual_address, mem.bus_address, mem.size);

    ret = MP4DecAddBuffer(this->instance, &mem);
    DBGT_PDEBUG("MP4DecAddBuffer ret (%d)", ret);

    switch (ret)
    {
        case MP4DEC_OK:
            stat = CODEC_OK;
            break;
        case MP4DEC_WAITING_FOR_BUFFER:
            stat = CODEC_NEED_MORE;
            break;
        case MP4DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MP4DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        default:
            DBGT_PDEBUG("MP4DecAddBuffer (%d)", ret);
            DBGT_ASSERT(!"Unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_pictureconsumed_mpeg4(CODEC_PROTOTYPE * arg, BUFFER *buff)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecPicture pic;
    OMX_U32 i, j;

    DBGT_PDEBUG("Consumed: bus_address %lu", buff->bus_address);

    for (i=0; i<MAX_BUFFERS; i++)
    {
        if (buff->bus_address == this->out_pic[i].pictures[0].output_picture_bus_address)
        {
            DBGT_PDEBUG("Found out_pic[%d]: bus_address %lu", (int)i, buff->bus_address);
            pic = this->out_pic[i];
            MP4DecRet ret = MP4DecPictureConsumed(this->instance, &pic);
            DBGT_PDEBUG("MP4DecPictureConsumed ret (%d)", ret);
            UNUSED_PARAMETER(ret);
            memset(&this->out_pic[i], 0, sizeof(this->out_pic[i]));
            break;
        }
    }

    // This condition may be happened in CommandFlush/PortReconfig/StateChange seq.
    // Show the warning message in verbose mode.
    if (i >= MAX_BUFFERS)
    {
        DBGT_PDEBUG("Output picture not found");
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    j = (i + MAX_BUFFERS - this->out_index_r) % MAX_BUFFERS;
    while (j > 0)
    {
        if (i == 0)
        {
            this->out_pic[0] = this->out_pic[MAX_BUFFERS - 1];
            i = MAX_BUFFERS;
        }
        else
            this->out_pic[i] = this->out_pic[i - 1];
        i--;
        j--;
    }
    memset(&this->out_pic[this->out_index_r], 0, sizeof(this->out_pic[0]));
    this->out_index_r++;
    if (this->out_index_r == MAX_BUFFERS)
        this->out_index_r = 0;
    this->out_num--;
    DBGT_EPILOG("");
    return CODEC_OK;
}


CODEC_STATE decoder_abort_mpeg4(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = MP4DecAbort(this->instance);

    switch (ret)
    {
        case MP4DEC_OK:
            stat = CODEC_OK;
            break;
        case MP4DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MP4DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MP4DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MP4DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MP4DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MP4DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MP4DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MP4DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MP4DEC_STRM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("MP4DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled MP4DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}


CODEC_STATE decoder_abortafter_mpeg4(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = MP4DecAbortAfter(this->instance);

    switch (ret)
    {
        case MP4DEC_OK:
            stat = CODEC_OK;
            break;
        case MP4DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MP4DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MP4DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MP4DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MP4DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MP4DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MP4DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MP4DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MP4DEC_STRM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("MP4DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled MP4DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_setnoreorder_mpeg4(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(no_reorder);
    DBGT_EPILOG("");
    return CODEC_OK;
}

CODEC_STATE decoder_setinfo_mpeg4(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                  PP_UNIT_CFG *pp_args)
{
    CALLSTACK;

    DBGT_PROLOG("");
    CODEC_MPEG4 *this = (CODEC_MPEG4 *)arg;
    MP4DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct MP4DecConfig dec_cfg;

    memset(&dec_cfg, 0, sizeof(struct MP4DecConfig));
    dec_cfg.align = DEC_ALIGN_16B;
    if (pp_args->enabled)
    {
        dec_cfg.ppu_config[0].enabled = pp_args->enabled;
        dec_cfg.ppu_config[0].crop.enabled = pp_args->crop.enabled;
        dec_cfg.ppu_config[0].crop.x = pp_args->crop.left;
        dec_cfg.ppu_config[0].crop.y = pp_args->crop.top;
        dec_cfg.ppu_config[0].crop.width = pp_args->crop.width;
        dec_cfg.ppu_config[0].crop.height = pp_args->crop.height;
    }

    ret = MP4DecSetInfo(this->instance, &dec_cfg);

    switch (ret)
    {
        case MP4DEC_OK:
            stat = CODEC_OK;
            break;
        case MP4DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MP4DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MP4DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MP4DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MP4DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MP4DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MP4DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MP4DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MP4DEC_STRM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("MP4DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled MP4DecRet");
            break;
    }

    DBGT_EPILOG("");
    UNUSED_PARAMETER(conf);
    return stat;
}
