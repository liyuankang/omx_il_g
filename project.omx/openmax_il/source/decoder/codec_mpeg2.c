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
#include "codec_mpeg2.h"
#include "mpeg2decapi.h"
#include "util.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX MPEG2"
#define MAX_BUFFERS 32

/* number of frame buffers decoder should allocate 0=AUTO */
//#define FRAME_BUFFERS 3
#define FRAME_BUFFERS 0

#define ALIGN(offset, align)   ((((unsigned int)offset) + align-1) & ~(align-1))
//#define ALIGN(offset, align)    offset

typedef struct CODEC_MPEG2
{
    CODEC_PROTOTYPE base;
    OMX_BOOL enableDeblock;  // Needs PP pipeline mode
    Mpeg2DecInst instance;
    OMX_BOOL update_pp_out;
    OMX_U32 picId;
    OMX_BOOL extraEosLoopDone;
    OMX_BOOL interlaced;
    OMX_U32 out_count;
    OMX_U32 out_index_r;
    OMX_U32 out_index_w;
    OMX_U32 out_num;
    Mpeg2DecPicture out_pic[MAX_BUFFERS];
} CODEC_MPEG2;

CODEC_STATE decoder_setframebuffer_mpeg2(CODEC_PROTOTYPE * arg, BUFFER *buff,
                                        OMX_U32 available_buffers);
CODEC_STATE decoder_pictureconsumed_mpeg2(CODEC_PROTOTYPE * arg, BUFFER *buff);
FRAME_BUFFER_INFO decoder_getframebufferinfo_mpeg2(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abort_mpeg2(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abortafter_mpeg2(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_setnoreorder_mpeg2(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder);
CODEC_STATE decoder_setinfo_mpeg2(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                  PP_UNIT_CFG *pp_args);
// destroy codec instance
static void decoder_destroy_mpeg2(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *) arg;

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
            Mpeg2DecRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// try to consume stream data
static CODEC_STATE decoder_decode_mpeg2(CODEC_PROTOTYPE * arg,
                                        STREAM_BUFFER * buf, OMX_U32 * consumed,
                                        FRAME * frame)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *) arg;

    DBGT_ASSERT(this);
    DBGT_ASSERT(this->instance);
    DBGT_ASSERT(buf);
    DBGT_ASSERT(consumed);

    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    Mpeg2DecInput input;

    Mpeg2DecOutput output;

    memset(&input, 0, sizeof(Mpeg2DecInput));
    memset(&output, 0, sizeof(Mpeg2DecOutput));

    input.stream = buf->bus_data;
    input.stream_bus_address = buf->bus_address;
    input.data_len = buf->streamlen;
    frame->size = 0;
    input.pic_id = buf->picId;
    input.skip_non_reference = 0;
    DBGT_PDEBUG("Pic id %d, stream length %d ", (int)this->picId, input.data_len);

    Mpeg2DecRet ret = Mpeg2DecDecode(this->instance, &input, &output);

    switch (ret)
    {
    case MPEG2DEC_STRM_PROCESSED:
        stat = CODEC_NEED_MORE;
        break;
    case MPEG2DEC_BUF_EMPTY:
        stat = CODEC_BUFFER_EMPTY;
        break;
    case MPEG2DEC_PIC_DECODED:
        this->picId++;
        stat = CODEC_HAS_FRAME;
        break;
    case MPEG2DEC_HDRS_RDY:
        stat = CODEC_HAS_INFO;
        break;
    case MPEG2DEC_NONREF_PIC_SKIPPED:
        DBGT_PDEBUG("Nonreference picture skipped");
        this->picId++;
        output.data_left = 0;
        stat = CODEC_PIC_SKIPPED;
        break;
    case MPEG2DEC_WAITING_FOR_BUFFER:
    {
        DBGT_PDEBUG("Waiting for frame buffer");
        Mpeg2DecBufferInfo info;
        do {
            ret = Mpeg2DecGetBufferInfo(this->instance, &info);
        } while (info.buf_to_free.bus_address);
        DBGT_PDEBUG("Buffer size %d, number of buffers %d",
            info.next_buf_size, info.buf_num);
        stat = CODEC_WAITING_FRAME_BUFFER;

        // Reset output relatived parameters
        this->out_index_w = 0;
        this->out_index_r = 0;
        this->out_num = 0;
        memset(this->out_pic, 0, sizeof(Mpeg2DecPicture)*MAX_BUFFERS);
    }
        break;
    case MPEG2DEC_ABORTED:
        DBGT_PDEBUG("Decoding aborted");
        *consumed = input.data_len;
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    case MPEG2DEC_NO_DECODING_BUFFER:
        stat = CODEC_NO_DECODING_BUFFER;
        break;
    case MPEG2DEC_PARAM_ERROR:
        stat = CODEC_ERROR_INVALID_ARGUMENT;
        break;
    case MPEG2DEC_STRM_ERROR:
        stat = CODEC_ERROR_STREAM;
        break;
    case MPEG2DEC_NOT_INITIALIZED:
        stat = CODEC_ERROR_NOT_INITIALIZED;
        break;
    case MPEG2DEC_HW_BUS_ERROR:
        stat = CODEC_ERROR_HW_BUS_ERROR;
        break;
    case MPEG2DEC_HW_TIMEOUT:
        stat = CODEC_ERROR_HW_TIMEOUT;
        break;
    case MPEG2DEC_SYSTEM_ERROR:
        stat = CODEC_ERROR_SYS;
        break;
    case MPEG2DEC_HW_RESERVED:
        stat = CODEC_ERROR_HW_RESERVED;
        break;
    case MPEG2DEC_MEMFAIL:
        stat = CODEC_ERROR_MEMFAIL;
        break;
    case MPEG2DEC_STREAM_NOT_SUPPORTED:
        stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
        break;
    case MPEG2DEC_FORMAT_NOT_SUPPORTED:
        stat = CODEC_ERROR_FORMAT_NOT_SUPPORTED;
        break;
    default:
        DBGT_ASSERT(!"unhandled Mpeg2DecRet");
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
static CODEC_STATE decoder_getinfo_mpeg2(CODEC_PROTOTYPE * arg,
                                         STREAM_INFO * pkg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(pkg);

    Mpeg2DecInfo info;

    memset(&info, 0, sizeof(Mpeg2DecInfo));
    Mpeg2DecRet ret = Mpeg2DecGetInfo(this->instance, &info);

    switch (ret)
    {
    case MPEG2DEC_OK:
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
        pkg->framesize = pkg->width * pkg->height * 3 / 2;
        pkg->interlaced = info.interlaced_sequence;
        this->interlaced = info.interlaced_sequence;

        if (info.output_format == MPEG2DEC_TILED_YUV420)
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

        // HDR video signal info
        pkg->video_full_range_flag = 0;
        //pkg->colour_desc_available = info.colour_description_present_flag;
        //pkg->colour_primaries = info.colour_primaries;
        //pkg->transfer_characteristics = info.transfer_characteristics;
        //pkg->matrix_coeffs = info.matrix_coefficients;

        DBGT_EPILOG("");
        return CODEC_OK;
    case MPEG2DEC_PARAM_ERROR:
        DBGT_CRITICAL("MPEG2DEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    case MPEG2DEC_HDRS_NOT_RDY:
        DBGT_CRITICAL("MPEG2DEC_HDRS_NOT_RDY");
        DBGT_EPILOG("");
        return CODEC_ERROR_STREAM;
    default:
        DBGT_ASSERT(!"unhandled Mpeg2DecRet");
        break;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

// get decoded frame
static CODEC_STATE decoder_getframe_mpeg2(CODEC_PROTOTYPE * arg, FRAME * frame,
                                          OMX_BOOL eos)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(frame);
    /* stream ended but we know there is picture to be outputted. We
     * have to loop one more time without "eos" and on next round
     * NextPicture is called with eos "force last output"
     */

    /*if (eos && this->extraEosLoopDone == OMX_FALSE && this->interlaced)
    {
        this->extraEosLoopDone = OMX_TRUE;
        eos = OMX_FALSE;
    }*/

    Mpeg2DecPicture picture;

    memset(&picture, 0, sizeof(Mpeg2DecPicture));
    Mpeg2DecRet ret = Mpeg2DecNextPicture(this->instance, &picture, eos);

    switch (ret)
    {
    case MPEG2DEC_PIC_RDY:

        DBGT_PDEBUG("end of stream %d", eos);
        DBGT_PDEBUG("err mbs %d", picture.number_of_err_mbs);
        DBGT_PDEBUG("Pic size %dx%d", picture.pictures[0].frame_width, picture.pictures[0].frame_height);
        if (picture.interlaced && picture.field_picture && picture.output_other_field)
        {
            DBGT_EPILOG("");
            return CODEC_HAS_FRAME; // do not send one field frame twice
        }
        frame->fb_bus_address = picture.pictures[0].output_picture_bus_address;
        frame->fb_bus_data = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.pLumaBase = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.nLumaBusAddress = picture.pictures[0].output_picture_bus_address;

        if (picture.pictures[0].output_format == DEC_OUT_FRM_TILED_4X4)
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
        frame->outBufPrivate.singleField = picture.single_field;
        DBGT_PDEBUG("Mpeg2DecNextPicture: outputPictureBusAddress %lu", picture.pictures[0].output_picture_bus_address);
        DBGT_PDEBUG("Mpeg2DecNextPicture: nChromaBusAddress %llu", frame->outBufPrivate.nChromaBusAddress);
        DBGT_PDEBUG("pLumaBase %p, pChromaBase %p", frame->outBufPrivate.pLumaBase, frame->outBufPrivate.pChromaBase);
        DBGT_PDEBUG("Luma size %lu", frame->outBufPrivate.nLumaSize);
        DBGT_PDEBUG("Chroma size %lu", frame->outBufPrivate.nChromaSize);
        frame->size = frame->outBufPrivate.nLumaSize + frame->outBufPrivate.nChromaSize;
        frame->MB_err_count = picture.number_of_err_mbs;

        //this->out_pic[this->out_count % MAX_BUFFERS] = picture;
        this->out_pic[this->out_index_w] = picture;
        this->out_count++;
        this->out_index_w++;
        if (this->out_index_w == MAX_BUFFERS) this->out_index_w = 0;
        this->out_num++;

        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    case MPEG2DEC_OK:
        DBGT_EPILOG("");
        return CODEC_OK;
    case MPEG2DEC_END_OF_STREAM:
        DBGT_PDEBUG("Mpeg2DecNextPicture: End of stream");
        DBGT_EPILOG("");
        return CODEC_END_OF_STREAM;
    case MPEG2DEC_ABORTED:
        DBGT_PDEBUG("Mpeg2DecNextPicture: aborted");
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    case MPEG2DEC_FLUSHED:
        DBGT_PDEBUG("Mpeg2DecNextPicture: flushed");
        DBGT_EPILOG("");
        return CODEC_FLUSHED;
    default:
        break;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

/* Find first and last startcode of stream. places masked by "first" and "last" */

static OMX_S32 decoder_scanframe_mpeg2(CODEC_PROTOTYPE * arg, STREAM_BUFFER * buf,
                                   OMX_U32 * first, OMX_U32 * last)
{
    UNUSED_PARAMETER(arg);
    CALLSTACK;

#ifdef USE_SCANFRAME
    *first = 0;
    *last = 0;

    // scan for start code
    OMX_S32 i = 0;

    OMX_S32 z = 0;

    for(; i < (OMX_S32)buf->streamlen; ++i)
    {
        if (!buf->bus_data[i])
            ++z;
        /* match to frame or sequence level start code */
        else if ((buf->bus_data[i] == 0x01) && (z >= 2) &&
                ((buf->bus_data[i + 1] > 0xAF) || (buf->bus_data[i + 1] == 0)))
        {
            *first = i - z;
            break;
        }
        else
            z = 0;
    }

    for(i = buf->streamlen - 3; i >= 0; --i)
    {
        // frame start code
        if ((buf->bus_data[i] == 0x00) && (buf->bus_data[i + 1] == 0x00) &&
           (buf->bus_data[i + 2] == 0x01) && (buf->bus_data[i + 3] == 0x00))
        {
            *last = i;
            return 1;
        }
    }
    return -1;
#else
    *first = 0;
    *last = buf->streamlen;
    return 1;
#endif
}


static CODEC_STATE decoder_endofstream_mpeg2(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = Mpeg2DecEndOfStream(this->instance, 1);

    switch (ret)
    {
        case MPEG2DEC_OK:
            stat = CODEC_OK;
            break;
        case MPEG2DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MPEG2DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MPEG2DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MPEG2DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MPEG2DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MPEG2DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MPEG2DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MPEG2DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MPEG2DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        case MPEG2DEC_ABORTED:
            stat = CODEC_ABORTED;
            break;
        default:
            DBGT_PDEBUG("Mpeg2DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled Mpeg2DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

// create codec instance and initialize it
CODEC_PROTOTYPE *HantroHwDecOmx_decoder_create_mpeg2(const void *DWLInstance,
                                            OMX_VIDEO_PARAM_CONFIGTYPE *conf)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = OSAL_Malloc(sizeof(CODEC_MPEG2));
    Mpeg2DecApiVersion decApi;
    Mpeg2DecBuild decBuild;

    memset(this, 0, sizeof(CODEC_MPEG2));

    this->base.destroy = decoder_destroy_mpeg2;
    this->base.decode = decoder_decode_mpeg2;
    this->base.getinfo = decoder_getinfo_mpeg2;
    this->base.getframe = decoder_getframe_mpeg2;
    this->base.scanframe = decoder_scanframe_mpeg2;
    this->base.endofstream = decoder_endofstream_mpeg2;
    this->base.pictureconsumed = decoder_pictureconsumed_mpeg2;
    this->base.setframebuffer = decoder_setframebuffer_mpeg2;
    this->base.getframebufferinfo = decoder_getframebufferinfo_mpeg2;
    this->base.abort = decoder_abort_mpeg2;
    this->base.abortafter = decoder_abortafter_mpeg2;
    this->base.setnoreorder = decoder_setnoreorder_mpeg2;
    this->base.setinfo = decoder_setinfo_mpeg2;
    this->instance = 0;
    this->update_pp_out = OMX_FALSE;
    this->picId = 0;
    this->extraEosLoopDone = OMX_FALSE;

    /* Print API version number */
    decApi = Mpeg2DecGetAPIVersion();
    decBuild = Mpeg2DecGetBuild();
    DBGT_PDEBUG("X170 Mpeg2 Decoder API v%d.%d - SW build: %d.%d - HW build: %x",
            decApi.major, decApi.minor, decBuild.sw_build >> 16,
            decBuild.sw_build & 0xFFFF, decBuild.hw_build);
    UNUSED_PARAMETER(decApi);
    UNUSED_PARAMETER(decBuild);

    enum DecDpbFlags dpbFlags = DEC_REF_FRM_RASTER_SCAN;

    if (conf->bEnableTiled)
        dpbFlags = DEC_REF_FRM_TILED_DEFAULT;

    DBGT_PDEBUG("dpbFlags 0x%x", dpbFlags);

    Mpeg2DecRet ret = Mpeg2DecInit(&this->instance,
                        DWLInstance,
                        ERROR_HANDLING,
                        FRAME_BUFFERS, dpbFlags,
                        conf->bEnableAdaptiveBuffers,
                        conf->nGuardSize);

    if (ret != MPEG2DEC_OK)
    {
        decoder_destroy_mpeg2((CODEC_PROTOTYPE *) this);
        DBGT_CRITICAL("Mpeg2DecInit error");
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (CODEC_PROTOTYPE *) this;
}

FRAME_BUFFER_INFO decoder_getframebufferinfo_mpeg2(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;
    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecBufferInfo info;
    FRAME_BUFFER_INFO bufInfo;
    memset(&bufInfo, 0, sizeof(FRAME_BUFFER_INFO));

    Mpeg2DecGetBufferInfo(this->instance, &info);

    bufInfo.bufferSize = info.next_buf_size;
    bufInfo.numberOfBuffers = info.buf_num;
    DBGT_PDEBUG("bufferSize %d, numberOfBuffers %d", (int)bufInfo.bufferSize, (int)bufInfo.numberOfBuffers);

    DBGT_EPILOG("");
    return bufInfo;
}

CODEC_STATE decoder_setframebuffer_mpeg2(CODEC_PROTOTYPE * arg, BUFFER *buff, OMX_U32 available_buffers)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(available_buffers);
    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct DWLLinearMem mem;
    Mpeg2DecBufferInfo info;
    Mpeg2DecRet ret;

    memset(&info, 0, sizeof(Mpeg2DecBufferInfo));

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

    ret = Mpeg2DecAddBuffer(this->instance, &mem);
    DBGT_PDEBUG("Mpeg2DecAddBuffer ret (%d)", ret);

    switch (ret)
    {
        case MPEG2DEC_OK:
            stat = CODEC_OK;
            break;
        case MPEG2DEC_WAITING_FOR_BUFFER:
            stat = CODEC_NEED_MORE;
            break;
        case MPEG2DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MPEG2DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"Unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_pictureconsumed_mpeg2(CODEC_PROTOTYPE * arg, BUFFER *buff)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecPicture pic;
    OMX_U32 i, j;

    DBGT_PDEBUG("Consumed: bus_address %lu", buff->bus_address);

    for (i=0; i<MAX_BUFFERS; i++)
    {
        if (buff->bus_address == this->out_pic[i].pictures[0].output_picture_bus_address)
        {
            DBGT_PDEBUG("Found out_pic[%d]: bus_address %lu", (int)i, buff->bus_address);
            pic = this->out_pic[i];
            Mpeg2DecRet ret = Mpeg2DecPictureConsumed(this->instance, &pic);
            DBGT_PDEBUG("Mpeg2DecPictureConsumed ret (%d)", ret);
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


CODEC_STATE decoder_abort_mpeg2(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = Mpeg2DecAbort(this->instance);

    switch (ret)
    {
        case MPEG2DEC_OK:
            stat = CODEC_OK;
            break;
        case MPEG2DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MPEG2DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MPEG2DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MPEG2DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MPEG2DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MPEG2DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MPEG2DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MPEG2DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MPEG2DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("Mpeg2DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled Mpeg2DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_abortafter_mpeg2(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = Mpeg2DecAbortAfter(this->instance);

    switch (ret)
    {
        case MPEG2DEC_OK:
            stat = CODEC_OK;
            break;
        case MPEG2DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MPEG2DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MPEG2DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MPEG2DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MPEG2DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MPEG2DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MPEG2DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MPEG2DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MPEG2DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("Mpeg2DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled Mpeg2DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_setnoreorder_mpeg2(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(no_reorder);
    DBGT_EPILOG("");
    return CODEC_OK;
}

CODEC_STATE decoder_setinfo_mpeg2(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                  PP_UNIT_CFG *pp_args)
{
    CALLSTACK;

    DBGT_PROLOG("");
    CODEC_MPEG2 *this = (CODEC_MPEG2 *)arg;
    Mpeg2DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct Mpeg2DecConfig dec_cfg;
    memset(&dec_cfg, 0, sizeof(struct Mpeg2DecConfig));
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

    ret = Mpeg2DecSetInfo(this->instance, &dec_cfg);

    switch (ret)
    {
        case MPEG2DEC_OK:
            stat = CODEC_OK;
            break;
        case MPEG2DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case MPEG2DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case MPEG2DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case MPEG2DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case MPEG2DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case MPEG2DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case MPEG2DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case MPEG2DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case MPEG2DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("Mpeg2DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled Mpeg2DecRet");
            break;
    }

    DBGT_EPILOG("");
    UNUSED_PARAMETER(conf);
    return stat;
}
