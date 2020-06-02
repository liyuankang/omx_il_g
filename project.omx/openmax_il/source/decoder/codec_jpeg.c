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
#include "codec_jpeg.h"
#include "jpegdecapi.h"
#include "util.h"
#include "dbgtrace.h"
#include "vsi_vendor_ext.h"
#include "test/queue.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX JPEG"
#define MAX_BUFFERS 32

#define MCU_SIZE 16
#define MCU_MAX_COUNT (8100)

#define MAX_RETRY_TIMES 2

typedef enum JPEG_DEC_STATE
{
    JPEG_PARSE_HEADERS,
    JPEG_WAITING_FRAME_BUFFER,
    JPEG_DECODE,
    JPEG_END_OF_STREAM
} JPEG_DEC_STATE;

typedef struct CODEC_JPEG
{
    CODEC_PROTOTYPE base;
    OSAL_ALLOCATOR alloc;
    JpegDecInst instance;
    JpegDecImageInfo info;
    JpegDecInput input;
    JPEG_DEC_STATE state;
    OMX_U32 sliceWidth;
    OMX_U32 sliceHeight;
    OMX_U32 scanLineSize;
    OMX_U32 scanLinesLeft;

    struct buffer
    {
        OSAL_BUS_WIDTH bus_address;
        OMX_U8 *bus_data;
        OMX_U32 capacity;
        OMX_U32 size;
    } Y, CbCr;

    OMX_BOOL mjpeg;
    OMX_BOOL forcedSlice;
    OMX_BOOL ppInfoSet;
    QUEUE_CLASS queue;
    FRAME frame;
    OMX_U32 retry;
    OMX_U32 out_count;
    OMX_U32 out_index_r;
    OMX_U32 out_index_w;
    OMX_U32 out_num;
    JpegDecOutput out_pic[MAX_BUFFERS];
} CODEC_JPEG;

CODEC_STATE decoder_setinfo_jpeg(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                 PP_UNIT_CFG *pp_args);
/* NV12 by default*/
#if 0
// ratio for calculating chroma offset for slice data
static OMX_U32 decoder_offset_ratio(OMX_U32 sampling)
{
    DBGT_PROLOG("");
    OMX_U32 ratio = 1;

    switch (sampling)
    {
    case JPEGDEC_YCbCr400:
        ratio = 1;
        break;
    case JPEGDEC_YCbCr420_SEMIPLANAR:
    case JPEGDEC_YCbCr411_SEMIPLANAR:
        ratio = 2;
        break;
    case JPEGDEC_YCbCr422_SEMIPLANAR:
        ratio = 1;
        break;
    case JPEGDEC_YCbCr444_SEMIPLANAR:
        ratio = 1;
        break;

    default:
        ratio = 1;
    }

    DBGT_EPILOG("");
    return ratio;
}
#endif
// destroy codec instance
static void decoder_destroy_jpeg(CODEC_PROTOTYPE * arg)
{
    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *) arg;

    if (this)
    {
        this->base.decode = 0;
        this->base.getframe = 0;
        this->base.getinfo = 0;
        this->base.destroy = 0;
        this->base.scanframe = 0;
        this->base.abort = 0;
        this->base.abortafter = 0;
        this->base.setnoreorder = 0;
        this->base.setinfo = 0;
        if (this->instance)
        {
            JpegDecRelease(this->instance);
            this->instance = 0;
        }
        if (this->Y.bus_address)
            OSAL_AllocatorFreeMem(&this->alloc, this->Y.capacity,
                                  this->Y.bus_data, this->Y.bus_address);
        if (this->CbCr.bus_address)
            OSAL_AllocatorFreeMem(&this->alloc, this->CbCr.capacity,
                                  this->CbCr.bus_data, this->CbCr.bus_address);

        //queue_clear(&this->queue);
        //OSAL_AllocatorDestroy(&this->alloc);
        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// try to consume stream data
static CODEC_STATE decoder_decode_jpeg(CODEC_PROTOTYPE * arg,
                                       STREAM_BUFFER * buf, OMX_U32 * consumed,
                                       FRAME * frame)
{
    DBGT_PROLOG("");
    CODEC_JPEG *this = (CODEC_JPEG *) arg;

    DBGT_ASSERT(arg);
    DBGT_ASSERT(buf);
    DBGT_ASSERT(consumed);

    OMX_U32 old_width;
    OMX_U32 old_height;

    this->input.stream_buffer.virtual_address = (u32 *) buf->bus_data;
    this->input.stream_buffer.bus_address = buf->bus_address;
    this->input.stream_length = buf->streamlen;
    this->input.buffer_size = 0; // note: use this if streamlen > 2097144
    this->input.dec_image_type = JPEGDEC_IMAGE;   // Only fullres supported

    DBGT_PDEBUG("streamLength %d", this->input.stream_length);
    if (this->state == JPEG_PARSE_HEADERS)
    {
        old_width = this->info.display_width;
        old_height = this->info.display_height;

        DBGT_PDEBUG("JpegDecGetImageInfo");
        JpegDecRet ret =
            JpegDecGetImageInfo(this->instance, &this->input, &this->info);
        *consumed = 0;
        switch (ret)
        {
        case JPEGDEC_OK:
            if ((this->info.output_width * this->info.output_height) > MAX_IMAGE_RESOLUTION)
            {
                DBGT_ERROR("Image resolution exceeds the supported resolution");
                DBGT_EPILOG("");
                return CODEC_ERROR_STREAM_NOT_SUPPORTED;
            }
            if (this->info.display_width != old_width || this->info.display_height != old_height)
            {
                this->state = JPEG_WAITING_FRAME_BUFFER;
                return CODEC_HAS_INFO;
            }
            else
            {
                this->scanLinesLeft = this->info.output_height;
                this->state = JPEG_DECODE;
                return CODEC_NEED_MORE;
            }

        case JPEGDEC_PARAM_ERROR:
            return CODEC_ERROR_INVALID_ARGUMENT;
        case JPEGDEC_ERROR:
        case JPEGDEC_STRM_ERROR:
            return CODEC_ERROR_STREAM;
        case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
        case JPEGDEC_INVALID_STREAM_LENGTH:
        case JPEGDEC_INCREASE_INPUT_BUFFER:
            return CODEC_ERROR_INVALID_ARGUMENT;
        case JPEGDEC_UNSUPPORTED:
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        default:
            DBGT_ASSERT(!"unhandled JpegDecRet");
            break;
        }
    }
    else if (this->state == JPEG_WAITING_FRAME_BUFFER)
    {
        this->state = JPEG_DECODE;
        return CODEC_WAITING_FRAME_BUFFER;
    }
    else
    {
        // decode
        JpegDecOutput output;
        //OMX_U32 sliceOffset = 0;
        //OMX_U32 offset = this->sliceWidth;
        //BUFFER buff;
        //int found = 0;

        memset(&output, 0, sizeof(JpegDecOutput));

#if 0
        offset *= this->info.output_height;
        if (!this->frame.fb_bus_address)
        {
          queue_pop_noblock (&this->queue, &buff, &found);
          if(!found)
            return CODEC_NO_DECODING_BUFFER;
          this->frame.fb_bus_data = buff.bus_data;
          this->frame.fb_bus_address = buff.bus_address;
        }

        // Offset created by the slices decoded so far
        sliceOffset =
            this->sliceWidth * (this->info.output_height -
                                this->scanLinesLeft);
        DBGT_PDEBUG("Slice offset %lu", sliceOffset);

        DBGT_PDEBUG("Using external output buffer. Bus address: %lu", this->frame.fb_bus_address);
        OMX_U32 ratio = decoder_offset_ratio(this->info.output_format);
        this->input.picture_buffer_y.virtual_address =
            (u32 *) this->frame.fb_bus_data + sliceOffset;
        this->input.picture_buffer_y.bus_address =
            this->frame.fb_bus_address + sliceOffset;

        this->input.picture_buffer_cb_cr.virtual_address =
            (u32 *) (this->frame.fb_bus_data + offset + sliceOffset / ratio);
        this->input.picture_buffer_cb_cr.bus_address =
            this->frame.fb_bus_address + offset + sliceOffset / ratio;
#endif
        JpegDecRet ret = JpegDecDecode(this->instance, &this->input, &output);

        switch (ret)
        {
        case JPEGDEC_FRAME_READY:
            this->retry = 0;
#ifdef ENABLE_CODEC_MJPEG
            if (this->mjpeg)
            {
                // next frame starts with headers
                this->state = JPEG_PARSE_HEADERS;
            }
#endif
            *consumed = buf->streamlen;
            DBGT_EPILOG("");
            return CODEC_HAS_FRAME;
#if 0
        case JPEGDEC_SLICE_READY:
            {
                if (!this->forcedSlice)
                    this->frame.size = this->scanLineSize * this->sliceHeight;
                this->scanLinesLeft -= this->sliceHeight;

                OMX_U32 luma = this->info.output_width * this->sliceHeight;

                OMX_U32 chroma = 0;

                switch (this->info.output_format)
                {
                case JPEGDEC_YCbCr420_SEMIPLANAR:
                    chroma = (luma * 3 / 2) - luma;
                    break;
                case JPEGDEC_YCbCr422_SEMIPLANAR:
                    chroma = luma;
                    break;
                }
            }
            DBGT_EPILOG("");
            return CODEC_NEED_MORE;
#endif
        case JPEGDEC_WAITING_FOR_BUFFER:
            return CODEC_WAITING_FRAME_BUFFER;
        case JPEGDEC_ABORTED:
            DBGT_PDEBUG("Decoding aborted");
            *consumed = buf->streamlen;
            this->state = JPEG_PARSE_HEADERS;
            DBGT_EPILOG("");
            return CODEC_ABORTED;
        case JPEGDEC_STRM_PROCESSED:
            return CODEC_NEED_MORE;
        case JPEGDEC_HW_RESERVED:
            return CODEC_ERROR_HW_TIMEOUT;
        case JPEGDEC_PARAM_ERROR:
        case JPEGDEC_INVALID_STREAM_LENGTH:
        case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
            return CODEC_ERROR_INVALID_ARGUMENT;
        case JPEGDEC_UNSUPPORTED:
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        case JPEGDEC_SYSTEM_ERROR:
            return CODEC_ERROR_SYS;
        case JPEGDEC_HW_BUS_ERROR:
            return CODEC_ERROR_HW_BUS_ERROR;
        case JPEGDEC_ERROR:
        case JPEGDEC_STRM_ERROR:
            DBGT_ERROR("JPEGDEC_STRM_ERROR");
            *consumed = buf->streamlen;
            this->state = JPEG_PARSE_HEADERS;
            DBGT_EPILOG("");
            if (ret == JPEGDEC_STRM_ERROR)  // output erroneous frame
                return CODEC_HAS_FRAME;
            else
                return CODEC_ERROR_STREAM;
        default:
            DBGT_ASSERT(!"unhandled JpegDecRet");
            break;
        }
    }
    UNUSED_PARAMETER(frame);
    return CODEC_ERROR_UNSPECIFIED;
}

// get stream info
static CODEC_STATE decoder_getinfo_jpeg(CODEC_PROTOTYPE * arg,
                                        STREAM_INFO * pkg)
{
    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(pkg);

    this->sliceWidth = this->info.output_width;
    this->scanLinesLeft = this->info.output_height;

#if 0
    // Calculate slice size
    if (this->info.output_width / MCU_SIZE * this->info.output_height / MCU_SIZE
       > MCU_MAX_COUNT)
    {
        this->input.slice_mb_set =
            MCU_MAX_COUNT / (this->info.output_width / MCU_SIZE);
        this->sliceHeight = this->input.slice_mb_set * MCU_SIZE;
        this->forcedSlice = OMX_TRUE;
    }
    else
    {
        this->input.slice_mb_set = 0;
        this->sliceHeight = this->info.output_height;
    }
#endif

    switch (this->info.output_format)
    {
    case JPEGDEC_YCbCr400:
        this->input.slice_mb_set /= 2;
        this->sliceHeight = this->input.slice_mb_set * MCU_SIZE;
        //pkg->format = OMX_COLOR_FormatL8;
        this->scanLineSize = this->info.output_width;
        break;
    case JPEGDEC_YCbCr420_SEMIPLANAR:
        //pkg->format = OMX_COLOR_FormatYUV420PackedSemiPlanar;
        this->scanLineSize = (this->info.output_width * 3) / 2;
        break;
    case JPEGDEC_YCbCr422_SEMIPLANAR:
        //pkg->format = OMX_COLOR_FormatYUV422PackedSemiPlanar;
        this->scanLineSize = this->info.output_width * 2;
        break;
    case JPEGDEC_YCbCr440:
        //pkg->format = OMX_COLOR_FormatYUV440PackedSemiPlanar;
        this->scanLineSize = this->info.output_width * 2;
        break;
    case JPEGDEC_YCbCr411_SEMIPLANAR:
        //pkg->format = OMX_COLOR_FormatYUV411PackedSemiPlanar;
        this->scanLineSize = (this->info.output_width * 3) / 2;
        break;
    case JPEGDEC_YCbCr444_SEMIPLANAR:
        //pkg->format = OMX_COLOR_FormatYUV444PackedSemiPlanar;
        this->scanLineSize = this->info.output_width * 3;
        break;
    default:
        DBGT_ASSERT(!"Unknown output format");
        break;
    }

    pkg->format = OMX_COLOR_FormatYUV420PackedSemiPlanar;
    pkg->width = this->info.output_width;
    pkg->height = this->info.output_height;
    pkg->stride = this->info.output_width;
    pkg->sliceheight = this->sliceHeight;

    pkg->crop_available = OMX_FALSE;
    if ((this->info.output_width != this->info.display_width) ||
        (this->info.output_height != this->info.display_height))
    {
        pkg->crop_left = 0;
        pkg->crop_top = 0;
        pkg->crop_width = this->info.display_width;
        pkg->crop_height = this->info.display_height;
        pkg->crop_available = OMX_TRUE;
        DBGT_PDEBUG("Crop left %d, top %d, width %d, height %d", (int)pkg->crop_left,
                (int)pkg->crop_top, (int)pkg->crop_width, (int)pkg->crop_height);
    }

    //this->state = JPEG_DECODE;
    DBGT_EPILOG("");
    return CODEC_OK;
}

// get decoded frame
static CODEC_STATE decoder_getframe_jpeg(CODEC_PROTOTYPE * arg, FRAME * frame,
                                         OMX_BOOL eos)
{
    CALLSTACK;
    UNUSED_PARAMETER(eos);
    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *) arg;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(frame);

    JpegDecOutput picture;
    JpegDecImageInfo info;

    memset(&picture, 0, sizeof(JpegDecOutput));
    JpegDecRet ret = JpegDecNextPicture(this->instance, &picture, &info);

    switch (ret)
    {
    case JPEGDEC_FRAME_READY:
        DBGT_PDEBUG("Pic size %dx%d", picture.pictures[0].display_width, picture.pictures[0].display_height);
        frame->fb_bus_address = picture.pictures[0].output_picture_y.bus_address;
        frame->fb_bus_data = (u8*)picture.pictures[0].output_picture_y.virtual_address;
        frame->outBufPrivate.pLumaBase = (u8*)picture.pictures[0].output_picture_y.virtual_address;
        frame->outBufPrivate.nLumaBusAddress = picture.pictures[0].output_picture_y.bus_address;
        frame->outBufPrivate.nLumaSize = picture.pictures[0].pic_stride * picture.pictures[0].output_height;
        frame->outBufPrivate.nChromaSize = picture.pictures[0].pic_stride_ch * picture.pictures[0].output_height / 2;
        frame->outBufPrivate.pChromaBase = (u8*)picture.pictures[0].output_picture_cb_cr.virtual_address;
        frame->outBufPrivate.nChromaBusAddress = picture.pictures[0].output_picture_cb_cr.bus_address;
        frame->outBufPrivate.nStride = picture.pictures[0].pic_stride;
        frame->outBufPrivate.nFrameWidth = picture.pictures[0].display_width;
        frame->outBufPrivate.nFrameHeight = picture.pictures[0].display_height;
        DBGT_PDEBUG("JpegDecNextPicture: outputPictureBusAddress %llu",  frame->outBufPrivate.nLumaBusAddress);
        DBGT_PDEBUG("JpegDecNextPicture: nChromaBusAddress %llu", frame->outBufPrivate.nChromaBusAddress);
        DBGT_PDEBUG("pLumaBase %p, pChromaBase %p", frame->outBufPrivate.pLumaBase, frame->outBufPrivate.pChromaBase);
        DBGT_PDEBUG("Luma size %lu", frame->outBufPrivate.nLumaSize);
        DBGT_PDEBUG("Chroma size %lu", frame->outBufPrivate.nChromaSize);
        frame->size = frame->outBufPrivate.nLumaSize + frame->outBufPrivate.nChromaSize;

        //this->out_pic[this->out_count % MAX_BUFFERS] = picture;
        this->out_pic[this->out_index_w] = picture;
        this->out_count++;
        this->out_index_w++;
        if (this->out_index_w == MAX_BUFFERS) this->out_index_w = 0;
        this->out_num++;

        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    case JPEGDEC_PARAM_ERROR:
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    case JPEGDEC_OK:
        DBGT_EPILOG("");
        return CODEC_OK;
    case JPEGDEC_END_OF_STREAM:
        DBGT_PDEBUG("JpegDecNextPicture: End of stream");
        DBGT_EPILOG("");
        return CODEC_END_OF_STREAM;
    default:
        break;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
#if 0
    if (this->frame.size)
    {
        memcpy(frame, &this->frame, sizeof(FRAME));
        frame->outBufPrivate.nFrameWidth = this->info.display_width;
        frame->outBufPrivate.nFrameHeight = this->info.display_height;
        this->frame.fb_bus_address = 0;
        this->frame.size = 0;
        //As for jpeg, set state to end_of_stream once one picture is outputted.
        if (!this->mjpeg)
            this->state = JPEG_END_OF_STREAM;

        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    }
    else if (this->state == JPEG_END_OF_STREAM)
    {
      return CODEC_END_OF_STREAM;
    }

    DBGT_EPILOG("");
    return CODEC_OK;
#endif
}


static CODEC_STATE decoder_abort_jpeg(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    JpegDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = JpegDecAbort(this->instance);

    switch (ret)
    {
        case JPEGDEC_OK:
            stat = CODEC_OK;
            break;
        case JPEGDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case JPEGDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case JPEGDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case JPEGDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case JPEGDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        default:
            DBGT_PDEBUG("JpegDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled JpegDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}


static CODEC_STATE decoder_abortafter_jpeg(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    JpegDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = JpegDecAbortAfter(this->instance);

    switch (ret)
    {
        case JPEGDEC_OK:
            stat = CODEC_OK;
            break;
        case JPEGDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case JPEGDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case JPEGDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case JPEGDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case JPEGDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        default:
            DBGT_PDEBUG("JpegDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled JpegDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE decoder_setnoreorder_jpeg(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder)
{
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(no_reorder);

    return CODEC_ERROR_UNSPECIFIED;
}


static OMX_S32 decoder_scanframe_jpeg(CODEC_PROTOTYPE * arg, STREAM_BUFFER * buf,
                                  OMX_U32 * first, OMX_U32 * last)
{
#ifdef ENABLE_CODEC_MJPEG
    CODEC_JPEG *this = (CODEC_JPEG *) arg;
#ifdef USE_SCANFRAME
    if (this->mjpeg)
    {
        OMX_U32 i;
        *first = 0;
        *last = 0;

        for(i = 0; i < buf->streamlen; ++i)
        {
            if (0xFF == buf->bus_data[i])
            {
                if (((i + 1) < buf->streamlen) && 0xD9 == buf->bus_data[i + 1])
                {
                    *last = i+2; /* add 16 bits to get the new start code */
                    return 1;
                }
            }
        }
    }
    else
        return -1;
#else
    if (this->mjpeg)
    {
        *first = 0;
        *last = buf->streamlen;
        return 1;
    }
    else
        return -1;
#endif
#else
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(buf);
    UNUSED_PARAMETER(first);
    UNUSED_PARAMETER(last);
#endif /* ENABLE_CODEC_MJPEG */
    return -1;
}

static CODEC_STATE decoder_endofstream_jpeg(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    JpegDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = JpegDecEndOfStream(this->instance);

    switch (ret)
    {
        case JPEGDEC_OK:
            stat = CODEC_OK;
            break;
        case JPEGDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        default:
            DBGT_PDEBUG("JpegDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled JpegDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

FRAME_BUFFER_INFO decoder_getframebufferinfo_jpeg(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;
    DBGT_PROLOG("");
    FRAME_BUFFER_INFO bufInfo;
    memset(&bufInfo, 0, sizeof(FRAME_BUFFER_INFO));

    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    JpegDecBufferInfo info;
    JpegDecGetBufferInfo(this->instance, &info);
    bufInfo.bufferSize = info.next_buf_size;
    bufInfo.numberOfBuffers = info.buf_num;
    DBGT_PDEBUG("bufferSize %d, numberOfBuffers %d", (int)bufInfo.bufferSize, (int)bufInfo.numberOfBuffers);

    DBGT_EPILOG("");
    return bufInfo;
}

CODEC_STATE decoder_setframebuffer_jpeg(CODEC_PROTOTYPE * arg, BUFFER *buff, OMX_U32 available_buffers)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(available_buffers);
    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct DWLLinearMem mem;
    JpegDecRet ret;

    mem.virtual_address = (u32*)buff->bus_data;
    mem.bus_address = buff->bus_address;
    mem.size = buff->allocsize;
    DBGT_PDEBUG("virtual_address %p, bus_address %lu, size %d",
    mem.virtual_address, mem.bus_address, mem.size);

    ret = JpegDecAddBuffer(this->instance, &mem);
    DBGT_PDEBUG("JpegDecAddBuffer ret (%d)", ret);

    switch (ret)
    {
        case JPEGDEC_OK:
            stat = CODEC_OK;
            break;
        case JPEGDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"Unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_pictureconsumed_jpeg(CODEC_PROTOTYPE * arg, BUFFER *buff)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_JPEG *this = (CODEC_JPEG *)arg;
    JpegDecOutput pic;
    OMX_U32 i, j;

    DBGT_PDEBUG("Consumed: bus_address %lu", buff->bus_address);

    for (i=0; i<MAX_BUFFERS; i++)
    {
        if (buff->bus_address == this->out_pic[i].pictures[0].output_picture_y.bus_address)
        {
            DBGT_PDEBUG("Found out_pic[%d]: bus_address %lu", (int)i, buff->bus_address);
            pic = this->out_pic[i];
            JpegDecRet ret = JpegDecPictureConsumed(this->instance, &pic);
            DBGT_PDEBUG("JpegDecPictureConsumed ret (%d)", ret);
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

// create codec instance and initialize it
CODEC_PROTOTYPE *HantroHwDecOmx_decoder_create_jpeg(const void *DWLInstance, OMX_BOOL motion_jpeg)
{
    DBGT_PROLOG("");

    CODEC_JPEG *this = OSAL_Malloc(sizeof(CODEC_JPEG));
    JpegDecApiVersion decApi;
    JpegDecBuild decBuild;

    memset(this, 0, sizeof(CODEC_JPEG));

    this->base.destroy = decoder_destroy_jpeg;
    this->base.decode = decoder_decode_jpeg;
    this->base.getinfo = decoder_getinfo_jpeg;
    this->base.getframe = decoder_getframe_jpeg;
    this->base.scanframe = decoder_scanframe_jpeg;
    this->base.endofstream = decoder_endofstream_jpeg;
    this->base.pictureconsumed = decoder_pictureconsumed_jpeg;
    this->base.setframebuffer = decoder_setframebuffer_jpeg;
    this->base.getframebufferinfo = decoder_getframebufferinfo_jpeg;
    this->base.abort = decoder_abort_jpeg;
    this->base.abortafter = decoder_abortafter_jpeg;
    this->base.setnoreorder = decoder_setnoreorder_jpeg;
    this->base.setinfo = decoder_setinfo_jpeg;
    this->instance = 0;
    this->state = JPEG_PARSE_HEADERS;

    /* Print API version number */
    decApi = JpegGetAPIVersion();
    decBuild = JpegDecGetBuild();
    DBGT_PDEBUG("X170 Jpeg Decoder API v%d.%d - SW build: %d.%d - HW build: %x",
            decApi.major, decApi.minor, decBuild.sw_build >> 16,
            decBuild.sw_build & 0xFFFF, decBuild.hw_build);
    UNUSED_PARAMETER(decApi);
    UNUSED_PARAMETER(decBuild);

    JpegDecMCConfig mc_init_cfg = {0, 0};

    JpegDecRet ret = JpegDecInit(&this->instance, DWLInstance, DEC_NORMAL, &mc_init_cfg);

#ifdef ENABLE_CODEC_MJPEG
    this->mjpeg = motion_jpeg;
#else
    UNUSED_PARAMETER(motion_jpeg);
#endif
    this->forcedSlice = OMX_FALSE;
    this->ppInfoSet = OMX_FALSE;

    if (ret != JPEGDEC_OK)
    {
        OSAL_Free(this);
        DBGT_CRITICAL("JpegDecInit error");
        DBGT_EPILOG("");
        return NULL;
    }
    if (OSAL_AllocatorInit(&this->alloc) != OMX_ErrorNone)
    {
        JpegDecRelease(this->instance);
        OSAL_Free(this);
        DBGT_CRITICAL("JpegDecInit error");
        DBGT_EPILOG("");
        return NULL;
    }

    queue_init(&this->queue, sizeof(BUFFER));
    DBGT_EPILOG("");
    return (CODEC_PROTOTYPE *) this;
}

CODEC_STATE decoder_setinfo_jpeg(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                 PP_UNIT_CFG *pp_args)
{
    CALLSTACK;

    DBGT_PROLOG("");
    CODEC_JPEG *this = (CODEC_JPEG *) arg;
    JpegDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct JpegDecConfig dec_cfg;
    memset(&dec_cfg, 0, sizeof(struct JpegDecConfig));
    dec_cfg.dec_image_type = JPEGDEC_IMAGE;    // Only fullres supported
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

    ret = JpegDecSetInfo(this->instance, &dec_cfg);
    switch (ret)
    {
    case JPEGDEC_OK:
        stat = CODEC_OK;
        break;
    case JPEGDEC_PARAM_ERROR:
        stat = CODEC_ERROR_INVALID_ARGUMENT;
        break;
    case JPEGDEC_ERROR:
    case JPEGDEC_STRM_ERROR:
        stat = CODEC_ERROR_STREAM;
        break;
    case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
    case JPEGDEC_INVALID_STREAM_LENGTH:
    case JPEGDEC_INCREASE_INPUT_BUFFER:
        stat = CODEC_ERROR_INVALID_ARGUMENT;
        break;
    case JPEGDEC_UNSUPPORTED:
        stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
        break;
    default:
        DBGT_ASSERT(!"unhandled JpegDecRet");
        break;
        }

    DBGT_EPILOG("");
    UNUSED_PARAMETER(conf);
    return stat;
}
