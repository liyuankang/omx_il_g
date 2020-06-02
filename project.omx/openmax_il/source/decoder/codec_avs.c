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
#include "codec_avs.h"
#include "avsdecapi.h"
#include "util.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX AVS"
#define MAX_BUFFERS 16

/* number of frame buffers decoder should allocate 0=AUTO */
//#define FRAME_BUFFERS 3
#define FRAME_BUFFERS 0

#define SHOW4(p) (p[0]) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); p+=4;
#define ALIGN(offset, align)   ((((unsigned int)offset) + align-1) & ~(align-1))
//#define ALIGN(offset, align)    offset

typedef struct CODEC_AVS
{
    CODEC_PROTOTYPE base;
    AvsDecInst instance;
    OMX_BOOL update_pp_out;
    OMX_U32 picId;
    OMX_U32 out_count;
    OMX_U32 out_index_r;
    OMX_U32 out_index_w;
    OMX_U32 out_num;
    AvsDecPicture out_pic[MAX_BUFFERS];
} CODEC_AVS;

CODEC_STATE decoder_setframebuffer_avs(CODEC_PROTOTYPE * arg, BUFFER *buff,
                                        OMX_U32 available_buffers);
CODEC_STATE decoder_pictureconsumed_avs(CODEC_PROTOTYPE * arg, BUFFER *buff);
FRAME_BUFFER_INFO decoder_getframebufferinfo_avs(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abort_avs(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abortafter_avs(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_setnoreorder_avs(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder);
CODEC_STATE decoder_setinfo_avs(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                PP_UNIT_CFG *pp_args);

// destroy codec instance
static void decoder_destroy_avs(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *) arg;

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
            AvsDecRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}


// try to consume stream data
static CODEC_STATE decoder_decode_avs(CODEC_PROTOTYPE * arg,
                                      STREAM_BUFFER * buf, OMX_U32 * consumed,
                                      FRAME * frame)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *) arg;

    DBGT_ASSERT(this);
    DBGT_ASSERT(this->instance);
    DBGT_ASSERT(buf);
    DBGT_ASSERT(consumed);

    AvsDecInput input;
    AvsDecOutput output;

    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    memset(&input, 0, sizeof(AvsDecInput));
    memset(&output, 0, sizeof(AvsDecOutput));

    input.stream = buf->bus_data;
    input.stream_bus_address = buf->bus_address;
    input.data_len = buf->streamlen;
    input.pic_id = buf->picId;
    input.skip_non_reference = 0;
    DBGT_PDEBUG("Pic id %d, stream length %d ", (int)this->picId, input.data_len);

    frame->size = 0;
    *consumed = 0;

    AvsDecRet ret = AvsDecDecode(this->instance, &input, &output);

    switch (ret)
    {
        case AVSDEC_STRM_PROCESSED:
            //Stream buffer processed, but new frame was not ready.
            stat = CODEC_NEED_MORE;
            break;
        case AVSDEC_BUF_EMPTY:
            stat = CODEC_BUFFER_EMPTY;
            break;
        case AVSDEC_PIC_DECODED:
            //All the data in the stream buffer was processed. Stream buffer must be updated before calling AvsDecDecode again.
            this->picId++;
            stat = CODEC_HAS_FRAME;
            break;
        case AVSDEC_HDRS_RDY:
            //Headers decoded. Stream header information is now readable with the function AvsDecGetInfo.
            stat = CODEC_HAS_INFO;
            break;
        case AVSDEC_NONREF_PIC_SKIPPED:
            DBGT_PDEBUG("Nonreference picture skipped");
            this->picId++;
            output.data_left = 0;
            stat = CODEC_PIC_SKIPPED;
            break;
        case AVSDEC_WAITING_FOR_BUFFER:
            DBGT_PDEBUG("Waiting for frame buffer");
            AvsDecBufferInfo info;
            do {
                ret = AvsDecGetBufferInfo(this->instance, &info);
            } while (info.buf_to_free.bus_address);
            ret = AvsDecGetBufferInfo(this->instance, &info);
            DBGT_PDEBUG("Buffer size %d, number of buffers %d",
                info.next_buf_size, info.buf_num);
            stat = CODEC_WAITING_FRAME_BUFFER;

            // Reset output relatived parameters
            this->out_index_w = 0;
            this->out_index_r = 0;
            this->out_num = 0;
            memset(this->out_pic, 0, sizeof(AvsDecPicture)*MAX_BUFFERS);

            break;
        case AVSDEC_ABORTED:
            DBGT_PDEBUG("Decoding aborted");
            *consumed = input.data_len;
            DBGT_EPILOG("");
            return CODEC_ABORTED;
        case AVSDEC_NO_DECODING_BUFFER:
            stat = CODEC_NO_DECODING_BUFFER;
            break;
        case AVSDEC_PARAM_ERROR:
            //Error in parameter structures. Stream decoding is not started. Possibly a NULL pointer or stream length is 0. Check structures.
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_STRM_ERROR:
            //Error in stream decoding. Can not recover before new headers decoded.
            stat = CODEC_ERROR_STREAM;
            break;
        case AVSDEC_NOT_INITIALIZED:
            //Decoder instance is not initialized. Stream decoding is not started. AvsDecInit must be called before decoding can be started.
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case AVSDEC_HW_BUS_ERROR:
            //A bus error occurred in the hardware operation of the decoder. The validity of each bus address should be checked. The decoding can not continue. The decoder instance has to be released.
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case AVSDEC_HW_TIMEOUT:
            //Error, the wait for a hardware finish has timed out. The current frame is lost. New frame decoding has to be started.
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case AVSDEC_SYSTEM_ERROR:
            //Error, a fatal system error was caught. The decoding can not continue. The decoder instance has to be released.
            stat = CODEC_ERROR_SYS;
            break;
        case AVSDEC_HW_RESERVED:
            //Failed to reserve HW for current decoder instance. The current frame is lost. New frame decoding has to be started.
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case AVSDEC_MEMFAIL:
            //The decoder was not able to allocate memory.
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case AVSDEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        case AVSDEC_FORMAT_NOT_SUPPORTED:
            stat = CODEC_ERROR_FORMAT_NOT_SUPPORTED;
            break;
        default:
            DBGT_ASSERT(!"unhandled AvsDecRet");
            break;
    }

    if (stat != CODEC_ERROR_UNSPECIFIED)
    {
        DBGT_PDEBUG("decoder data left %d", output.data_left);

        *consumed = input.data_len - output.data_left;
        DBGT_PDEBUG("Consumed %d", (int) *consumed);
    }
    DBGT_EPILOG("");
    return stat;
}

// get stream info
static CODEC_STATE decoder_getinfo_avs(CODEC_PROTOTYPE * arg, STREAM_INFO * pkg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *) arg;
    AvsDecInfo info;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(pkg);

    memset(&info, 0, sizeof(AvsDecInfo));

    AvsDecRet ret = AvsDecGetInfo(this->instance, &info);
    if (ret == AVSDEC_HDRS_NOT_RDY)
    {
        DBGT_CRITICAL("AVSDEC_HDRS_NOT_RDY");
        DBGT_EPILOG("");
        return CODEC_ERROR_STREAM;
    }
    else if (ret == AVSDEC_PARAM_ERROR)
    {
        DBGT_CRITICAL("AVSDEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    }
    else if (ret == AVSDEC_OK)
    {
        if ((info.frame_width * info.frame_height) > MAX_VIDEO_RESOLUTION)
        {
            DBGT_ERROR("Video stream resolution exceeds the supported video resolution");
            DBGT_EPILOG("");
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        }
        pkg->width = info.frame_width;
        pkg->height = info.frame_height;
        pkg->stride = info.frame_width;
        pkg->sliceheight = info.frame_height;
        pkg->interlaced = info.interlaced_sequence;
        pkg->framesize = pkg->width * pkg->height * 3 / 2;

        if (info.output_format == AVSDEC_TILED_YUV420)
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
        DBGT_EPILOG("");
        return CODEC_OK;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

// get decoded frame
static CODEC_STATE decoder_getframe_avs(CODEC_PROTOTYPE * arg, FRAME * frame,
                                        OMX_BOOL eos)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecPicture picture;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(frame);
    /* stream ended but we know there is picture to be outputted. We
     * have to loop one more time without "eos" and on next round
     * NextPicture is called with eos "force last output"
     */
    memset(&picture, 0, sizeof(AvsDecPicture));

    u32 endofStream = eos == OMX_TRUE ? 1 : 0;

    AvsDecRet ret = AvsDecNextPicture(this->instance, &picture, endofStream);

    if (ret == AVSDEC_PIC_RDY)
    {
        DBGT_PDEBUG("end of stream %d", endofStream);
        DBGT_PDEBUG("err mbs %d", picture.number_of_err_mbs);
        DBGT_PDEBUG("Frame size %dx%d", picture.pictures[0].frame_width, picture.pictures[0].frame_height);
        DBGT_PDEBUG("Coded size %dx%d", picture.pictures[0].coded_width, picture.pictures[0].coded_height);

        if (picture.interlaced && picture.field_picture && picture.first_field)
            return CODEC_HAS_FRAME; // do not send first field frame
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
        DBGT_PDEBUG("AvsDecNextPicture: outputPictureBusAddress %lu", picture.pictures[0].output_picture_bus_address);
        DBGT_PDEBUG("AvsDecNextPicture: nChromaBusAddress %llu", frame->outBufPrivate.nChromaBusAddress);
        DBGT_PDEBUG("pLumaBase %p, pChromaBase %p", frame->outBufPrivate.pLumaBase, frame->outBufPrivate.pChromaBase);
        DBGT_PDEBUG("Luma size %lu", frame->outBufPrivate.nLumaSize);
        DBGT_PDEBUG("Chroma size %lu", frame->outBufPrivate.nChromaSize);
        frame->size = frame->outBufPrivate.nLumaSize + frame->outBufPrivate.nChromaSize;
        this->out_pic[this->out_index_w] = picture;
        this->out_count++;
        this->out_index_w++;
        if (this->out_index_w == MAX_BUFFERS) this->out_index_w = 0;
        this->out_num++;

        frame->MB_err_count = picture.number_of_err_mbs;
        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    }
    else if (ret == AVSDEC_OK)
    {
        DBGT_EPILOG("");
        return CODEC_OK;
    }
    else if (ret == AVSDEC_PARAM_ERROR)
    {
        DBGT_CRITICAL("AVSDEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    }
    else if (ret == AVSDEC_NOT_INITIALIZED)
    {
        DBGT_CRITICAL("AVSDEC_NOT_INITIALIZED");
        DBGT_EPILOG("");
        return CODEC_ERROR_SYS;
    }
    else if (ret == AVSDEC_END_OF_STREAM)
    {
        DBGT_PDEBUG("AvsDecNextPicture: End of stream");
        DBGT_EPILOG("");
        return CODEC_END_OF_STREAM;
    }
    else if (ret == AVSDEC_ABORTED)
    {
        DBGT_PDEBUG("AvsDecNextPicture: aborted");
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    }
    else if (ret == AVSDEC_FLUSHED)
    {
        DBGT_PDEBUG("AvsDecNextPicture: flushed");
        DBGT_EPILOG("");
        return CODEC_FLUSHED;
    }
    DBGT_CRITICAL("AVS CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

static OMX_S32 decoder_scanframe_avs(CODEC_PROTOTYPE * arg, STREAM_BUFFER * buf,
                                 OMX_U32 * first, OMX_U32 * last)
{
    CALLSTACK;
    CODEC_AVS *this = (CODEC_AVS *) arg;

    DBGT_ASSERT(this);

    *first = 0;
    *last = buf->streamlen;
    return 1;
}

static CODEC_STATE decoder_endofstream_avs(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = AvsDecEndOfStream(this->instance, 1);

    switch (ret)
    {
        case AVSDEC_OK:
            stat = CODEC_OK;
            break;
        case AVSDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case AVSDEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case AVSDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case AVSDEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case AVSDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case AVSDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case AVSDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case AVSDEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        case AVSDEC_ABORTED:
            stat = CODEC_ABORTED;
            break;
        default:
            DBGT_PDEBUG("AvsDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled AvsDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

// create codec instance and initialize it
CODEC_PROTOTYPE *HantroHwDecOmx_decoder_create_avs(const void *DWLInstance,
                                        OMX_VIDEO_PARAM_CONFIGTYPE *conf)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = OSAL_Malloc(sizeof(CODEC_AVS));
    AvsDecApiVersion decApi;
    AvsDecBuild decBuild;

    memset(this, 0, sizeof(CODEC_AVS));

    this->base.destroy = decoder_destroy_avs;
    this->base.decode = decoder_decode_avs;
    this->base.getinfo = decoder_getinfo_avs;
    this->base.getframe = decoder_getframe_avs;
    this->base.scanframe = decoder_scanframe_avs;
    this->base.endofstream = decoder_endofstream_avs;
    this->base.pictureconsumed = decoder_pictureconsumed_avs;
    this->base.setframebuffer = decoder_setframebuffer_avs;
    this->base.getframebufferinfo = decoder_getframebufferinfo_avs;
    this->base.abort = decoder_abort_avs;
    this->base.abortafter = decoder_abortafter_avs;
    this->base.setnoreorder = decoder_setnoreorder_avs;
    this->base.setinfo = decoder_setinfo_avs;
    this->instance = 0;
    this->picId = 0;

    /* Print API version number */
    decApi = AvsDecGetAPIVersion();
    decBuild = AvsDecGetBuild();
    DBGT_PDEBUG("X170 AVS Decoder API v%d.%d - SW build: %d.%d - HW build: %x",
            decApi.major, decApi.minor, decBuild.sw_build >> 16,
            decBuild.sw_build & 0xFFFF, decBuild.hw_build);
    UNUSED_PARAMETER(decApi);
    UNUSED_PARAMETER(decBuild);

    enum DecDpbFlags dpbFlags = DEC_REF_FRM_RASTER_SCAN;

    if (conf->bEnableTiled)
        dpbFlags = DEC_REF_FRM_TILED_DEFAULT;

    DBGT_PDEBUG("dpbFlags 0x%x", dpbFlags);

    AvsDecRet ret = AvsDecInit(&this->instance,
                        DWLInstance,
                        ERROR_HANDLING,
                        FRAME_BUFFERS, dpbFlags,
                        conf->bEnableAdaptiveBuffers,
                        conf->nGuardSize);

    if (ret != AVSDEC_OK)
    {
        decoder_destroy_avs((CODEC_PROTOTYPE *) this);
        DBGT_CRITICAL("AvsDecInit error");
        DBGT_EPILOG("");
        return NULL;
    }
    DBGT_EPILOG("");
    return (CODEC_PROTOTYPE *) this;
}

FRAME_BUFFER_INFO decoder_getframebufferinfo_avs(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;
    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecBufferInfo info;
    FRAME_BUFFER_INFO bufInfo;
    memset(&bufInfo, 0, sizeof(FRAME_BUFFER_INFO));

    AvsDecGetBufferInfo(this->instance, &info);

    bufInfo.bufferSize = info.next_buf_size;
    bufInfo.numberOfBuffers = info.buf_num;
    DBGT_PDEBUG("bufferSize %d, numberOfBuffers %d", (int)bufInfo.bufferSize, (int)bufInfo.numberOfBuffers);

    DBGT_EPILOG("");
    return bufInfo;
}

CODEC_STATE decoder_setframebuffer_avs(CODEC_PROTOTYPE * arg, BUFFER *buff, OMX_U32 available_buffers)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(available_buffers);
    CODEC_AVS *this = (CODEC_AVS *)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct DWLLinearMem mem;
    AvsDecBufferInfo info;
    AvsDecRet ret;

    memset(&info, 0, sizeof(AvsDecBufferInfo));

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

    ret = AvsDecAddBuffer(this->instance, &mem);
    DBGT_PDEBUG("AvsDecAddBuffer ret (%d)", ret);

    switch (ret)
    {
        case AVSDEC_OK:
            stat = CODEC_OK;
            break;
        case AVSDEC_WAITING_FOR_BUFFER:
            stat = CODEC_NEED_MORE;
            break;
        case AVSDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_MEMFAIL:
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

CODEC_STATE decoder_pictureconsumed_avs(CODEC_PROTOTYPE * arg, BUFFER *buff)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecPicture pic;
    OMX_U32 i, j;

    DBGT_PDEBUG("Consumed: bus_address %lu", buff->bus_address);

    for (i=0; i<MAX_BUFFERS; i++)
    {
        if (buff->bus_address == this->out_pic[i].pictures[0].output_picture_bus_address)
        {
            DBGT_PDEBUG("Found out_pic[%d]: bus_address %lu", (int)i, buff->bus_address);
            pic = this->out_pic[i];
            AvsDecRet ret = AvsDecPictureConsumed(this->instance, &pic);
            DBGT_PDEBUG("AvsDecPictureConsumed ret (%d)", ret);
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


CODEC_STATE decoder_abort_avs(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = AvsDecAbort(this->instance);

    switch (ret)
    {
        case AVSDEC_OK:
            stat = CODEC_OK;
            break;
        case AVSDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case AVSDEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case AVSDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case AVSDEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case AVSDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case AVSDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case AVSDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case AVSDEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("AvsDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled AvsDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}


CODEC_STATE decoder_abortafter_avs(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = AvsDecAbortAfter(this->instance);

    switch (ret)
    {
        case AVSDEC_OK:
            stat = CODEC_OK;
            break;
        case AVSDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case AVSDEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case AVSDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case AVSDEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case AVSDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case AVSDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case AVSDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case AVSDEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("AvsDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled AvsDecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_setnoreorder_avs(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(no_reorder);
    DBGT_EPILOG("");
    return CODEC_OK;
}

CODEC_STATE decoder_setinfo_avs(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                PP_UNIT_CFG *pp_args)
{
    CALLSTACK;

    DBGT_PROLOG("");
    CODEC_AVS *this = (CODEC_AVS *)arg;
    AvsDecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct AvsDecConfig dec_cfg;
    memset(&dec_cfg, 0, sizeof(struct AvsDecConfig));
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

    ret = AvsDecSetInfo(this->instance, &dec_cfg);

    switch (ret)
    {
        case AVSDEC_OK:
            stat = CODEC_OK;
            break;
        case AVSDEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case AVSDEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case AVSDEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case AVSDEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case AVSDEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case AVSDEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case AVSDEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case AVSDEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case AVSDEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("AvsDecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled AvsDecRet");
            break;
    }

    DBGT_EPILOG("");
    UNUSED_PARAMETER(conf);
    return stat;
}
