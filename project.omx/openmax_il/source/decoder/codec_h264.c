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
#include "codec_h264.h"
#include "h264decapi.h"
#include "util.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX H264"

#define DISABLE_OUTPUT_REORDER 0
#define USE_DISPLAY_SMOOTHING 0
#define MAX_BUFFERS 34

typedef struct CODEC_H264
{
    CODEC_PROTOTYPE base;
    H264DecInst instance;
    OMX_U32 picId;
    //OMX_BOOL extraEosLoopDone;
    OMX_BOOL update_pp_out;
    OMX_BOOL frame_sent;
    OMX_BOOL pending_flush;
    OMX_U32 out_count;
    OMX_U32 out_index_r;
    OMX_U32 out_index_w;
    OMX_U32 out_num;
    H264DecPicture out_pic[MAX_BUFFERS];
} CODEC_H264;

CODEC_STATE decoder_setframebuffer_h264(CODEC_PROTOTYPE * arg, BUFFER *buff,
                                        OMX_U32 available_buffers);
CODEC_STATE decoder_pictureconsumed_h264(CODEC_PROTOTYPE * arg, BUFFER *buff);
CODEC_STATE decoder_abort_h264(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_abortafter_h264(CODEC_PROTOTYPE * arg);
CODEC_STATE decoder_setnoreorder_h264(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder);
CODEC_STATE decoder_setinfo_h264(CODEC_PROTOTYPE * arg,
                                 OMX_VIDEO_PARAM_CONFIGTYPE *conf, PP_UNIT_CFG *pp_args);
FRAME_BUFFER_INFO decoder_getframebufferinfo_h264(CODEC_PROTOTYPE * arg);

// destroy codec instance
static void decoder_destroy_h264(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *) arg;

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
            H264DecRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// try to consume stream data
static CODEC_STATE decoder_decode_h264(CODEC_PROTOTYPE * arg,
                                       STREAM_BUFFER * buf, OMX_U32 * consumed,
                                       FRAME * frame)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *) arg;

    DBGT_ASSERT(this);
    DBGT_ASSERT(this->instance);
    DBGT_ASSERT(buf);
    DBGT_ASSERT(consumed);

    H264DecInput input;
    H264DecOutput output;

    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    memset(&input, 0, sizeof(H264DecInput));
    memset(&output, 0, sizeof(H264DecOutput));

    input.stream = buf->bus_data;
    input.stream_bus_address = buf->bus_address;
    input.data_len = buf->streamlen;

    input.buffer = buf->buf_data;
    input.buffer_bus_address = buf->buf_address;
    input.buff_len = buf->allocsize;
    
    input.pic_id = buf->picId;
    input.skip_non_reference = 0;
    input.p_user_data = NULL;
    DBGT_PDEBUG("Pic id %d, stream length %d", (int)this->picId, input.data_len);
    *consumed = 0;
    frame->size = 0;
    this->pending_flush = OMX_FALSE;

    enum DecRet ret = H264DecDecode(this->instance, &input, &output);

    switch (ret)
    {
    case DEC_PENDING_FLUSH:
        DBGT_PDEBUG("Pending flush");
        //this->picId++;
        //this->pending_flush = OMX_TRUE;
        stat = CODEC_PENDING_FLUSH;
        break;
    case DEC_PIC_DECODED:
        this->picId++;
        stat = CODEC_HAS_FRAME;
        break;
    case DEC_HDRS_RDY:
        stat = CODEC_HAS_INFO;
        break;
    case DEC_ADVANCED_TOOLS:
        stat = CODEC_NEED_MORE;
        break;
    case DEC_STRM_PROCESSED:
        stat = CODEC_NEED_MORE;
        break;
    case DEC_BUF_EMPTY:
        stat = CODEC_BUFFER_EMPTY;
        break;
    case DEC_NONREF_PIC_SKIPPED:
        DBGT_PDEBUG("Nonreference picture skipped");
        this->picId++;
        output.data_left = 0;
        stat = CODEC_PIC_SKIPPED;
        break;
    case DEC_WAITING_FOR_BUFFER:
    {
        DBGT_PDEBUG("Waiting for frame buffer");
        H264DecBufferInfo info;

        do {
        ret = H264DecGetBufferInfo(this->instance, &info);
        } while (info.buf_to_free.bus_address);
        DBGT_PDEBUG("Buffer size %d, number of buffers %d",
            info.next_buf_size, info.buf_num);
        stat = CODEC_WAITING_FRAME_BUFFER;

        // Reset output relatived parameters
        this->out_index_w = 0;
        this->out_index_r = 0;
        this->out_num = 0;
        memset(this->out_pic, 0, sizeof(H264DecPicture)*MAX_BUFFERS);
    }
        break;
    case DEC_ABORTED:
        DBGT_PDEBUG("Decoding aborted");
        *consumed = input.data_len;
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    case DEC_NO_DECODING_BUFFER:
        stat = CODEC_NO_DECODING_BUFFER;
        break;
    case DEC_PARAM_ERROR:
        stat = CODEC_ERROR_INVALID_ARGUMENT;
        break;
    case DEC_STRM_ERROR:
        stat = CODEC_ERROR_STREAM;
        break;
    case DEC_NOT_INITIALIZED:
        stat = CODEC_ERROR_NOT_INITIALIZED;
        break;
    case DEC_HW_BUS_ERROR:
        stat = CODEC_ERROR_HW_BUS_ERROR;
        break;
    case DEC_HW_TIMEOUT:
        stat = CODEC_ERROR_HW_TIMEOUT;
        break;
    case DEC_SYSTEM_ERROR:
        stat = CODEC_ERROR_SYS;
        break;
    case DEC_HW_RESERVED:
        stat = CODEC_ERROR_HW_RESERVED;
        break;
    case DEC_MEMFAIL:
        stat = CODEC_ERROR_MEMFAIL;
        break;
    case DEC_STREAM_NOT_SUPPORTED:
        stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
        break;
    case DEC_FORMAT_NOT_SUPPORTED:
        stat = CODEC_ERROR_FORMAT_NOT_SUPPORTED;
        break;
    default:
        DBGT_ASSERT(!"Unhandled DecRet");
        break;
    }

    if (stat != CODEC_ERROR_UNSPECIFIED)
    {
        DBGT_PDEBUG("Decoder data left %d", output.data_left);

       // if (stat == CODEC_HAS_INFO)
       //     *consumed = 0;
       // else if (this->pending_flush)
            *consumed = input.data_len - output.data_left;
       // else
       //     *consumed = input.dataLen;
        DBGT_PDEBUG("Consumed %d", (int) *consumed);
    }

    DBGT_EPILOG("");
    return stat;
}

    // get stream info
static CODEC_STATE decoder_getinfo_h264(CODEC_PROTOTYPE * arg,
                                        STREAM_INFO * pkg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *) arg;
    H264DecInfo decinfo;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(pkg);

    memset(&decinfo, 0, sizeof(H264DecInfo));

    // the headers are ready, get stream information
    enum DecRet ret = H264DecGetInfo(this->instance, &decinfo);

    if (ret == DEC_OK)
    {
        if ((decinfo.pic_width * decinfo.pic_height) > MAX_VIDEO_RESOLUTION)
        {
            DBGT_ERROR("Video stream resolution exceeds the supported video resolution");
            DBGT_EPILOG("");
            return CODEC_ERROR_STREAM_NOT_SUPPORTED;
        }

        pkg->width = decinfo.pic_width;//decinfo.cropParams.cropOutWidth;
        pkg->height = decinfo.pic_height;//decinfo.cropParams.cropOutHeight;
        pkg->sliceheight = decinfo.pic_height;
        pkg->stride = decinfo.pic_width;

        if (decinfo.output_format == DEC_OUT_FRM_TILED_4X4)
            pkg->format = OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled;
        else if ((decinfo.output_format == DEC_OUT_FRM_RASTER_SCAN) && (decinfo.bit_depth > 8))
            pkg->format = OMX_COLOR_FormatYUV420SemiPlanarP010;
        else
            pkg->format = OMX_COLOR_FormatYUV420PackedSemiPlanar;

        pkg->bit_depth = decinfo.bit_depth;
        pkg->crop_available = OMX_FALSE;
        pkg->crop_left = decinfo.crop_params.crop_left_offset;
        pkg->crop_top = decinfo.crop_params.crop_top_offset;
        pkg->crop_width = decinfo.crop_params.crop_out_width;
        pkg->crop_height = decinfo.crop_params.crop_out_height;
        if ((pkg->crop_left != 0) || (pkg->crop_top != 0) ||
            (pkg->crop_width != pkg->width) ||
            (pkg->crop_height != pkg->height))
        {
            pkg->crop_available = OMX_TRUE;
        }
        DBGT_PDEBUG("Crop left %d, top %d, width %d, height %d", (int)pkg->crop_left,
                (int)pkg->crop_top, (int)pkg->crop_width, (int)pkg->crop_height);

        pkg->interlaced = decinfo.interlaced_sequence;
        DBGT_PDEBUG("Interlaced sequence %d", (int)pkg->interlaced);

        // HDR video signal info
        //pkg->colour_desc_available = decinfo.colour_description_present_flag;
        //pkg->video_full_range_flag = decinfo.video_range;
        //pkg->colour_primaries = decinfo.colour_primaries;
        //pkg->transfer_characteristics = decinfo.transfer_characteristics;
        //pkg->matrix_coeffs = decinfo.matrix_coefficients;

        DBGT_EPILOG("");
        return CODEC_OK;
    }
    else if (ret == DEC_PARAM_ERROR)
    {
        DBGT_CRITICAL("DEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    }
    else if (ret == DEC_HDRS_NOT_RDY)
    {
        DBGT_CRITICAL("DEC_HDRS_NOT_RDY");
        DBGT_EPILOG("");
        return CODEC_ERROR_STREAM;
    }
    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

// get decoded frame
static CODEC_STATE decoder_getframe_h264(CODEC_PROTOTYPE * arg, FRAME * frame,
                                         OMX_BOOL eos)
{
    CALLSTACK;
    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *) arg;
    H264DecPicture picture;

    DBGT_ASSERT(this != 0);
    DBGT_ASSERT(this->instance != 0);
    DBGT_ASSERT(frame);

    if (USE_DISPLAY_SMOOTHING && this->frame_sent && !this->pending_flush && !eos)
    {
        DBGT_PDEBUG("Display smoothing: One frame already sent... Return");
        this->frame_sent = OMX_FALSE;
        DBGT_EPILOG("");
        return CODEC_OK;
    }

    /* stream ended but we know there is picture to be outputted. We
     * have to loop one more time without "eos" and on next round
     * NextPicture is called with eos "force last output"
     */

    memset(&picture, 0, sizeof(H264DecPicture));

    u32 endofStream = eos == OMX_TRUE ? 1 : 0;

    /* Flush internal picture buffers */
    if (this->pending_flush)
        endofStream = 1;

    enum DecRet ret = H264DecNextPicture(this->instance, &picture, endofStream);

    this = (CODEC_H264 *) arg;

    if (ret == DEC_PIC_RDY)
    {
        DBGT_PDEBUG("End of stream %d", endofStream);
        DBGT_PDEBUG("Err mbs %d", picture.nbr_of_err_mbs);
        DBGT_PDEBUG("View ID %d", picture.view_id);
        DBGT_PDEBUG("Pic size %dx%d", picture.pictures[0].pic_width,  picture.pictures[0].pic_height);

        frame->fb_bus_address = picture.pictures[0].output_picture_bus_address;
        frame->fb_bus_data = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.pLumaBase = (u8*)picture.pictures[0].output_picture;
        frame->outBufPrivate.nLumaBusAddress = picture.pictures[0].output_picture_bus_address;
        frame->outBufPrivate.pChromaBase = (u8*)picture.pictures[0].output_picture_chroma;
        frame->outBufPrivate.nChromaBusAddress =  picture.pictures[0].output_picture_chroma_bus_address;
        if (picture.pictures[0].output_format == DEC_OUT_FRM_TILED_4X4 ||
            picture.pictures[0].output_format == DEC_OUT_FRM_RFC)
        {
            frame->outBufPrivate.nLumaSize = picture.pictures[0].pic_stride * picture.pictures[0].pic_height / 4;
            if (frame->outBufPrivate.nChromaBusAddress)
                frame->outBufPrivate.nChromaSize = picture.pictures[0].pic_stride_ch * picture.pictures[0].pic_height / 8;
            else
                frame->outBufPrivate.nChromaSize = 0;
        }
        else
        {
            frame->outBufPrivate.nLumaSize = picture.pictures[0].pic_stride * picture.pictures[0].pic_height;
            if (frame->outBufPrivate.nChromaBusAddress)
                frame->outBufPrivate.nChromaSize = picture.pictures[0].pic_stride_ch * picture.pictures[0].pic_height / 2;
            else
                frame->outBufPrivate.nChromaSize = 0;
        }

        frame->outBufPrivate.nBitDepthLuma = picture.bit_depth_luma;
        frame->outBufPrivate.nBitDepthChroma = picture.bit_depth_chroma;
        frame->outBufPrivate.nStride = picture.pictures[0].pic_stride;
        frame->outBufPrivate.nFrameWidth = picture.pictures[0].pic_width;
        frame->outBufPrivate.nFrameHeight = picture.pictures[0].pic_height;
        frame->outBufPrivate.nPicId[0] = picture.decode_id[0];
        frame->outBufPrivate.nPicId[1] = picture.decode_id[1];
        frame->outBufPrivate.singleField = picture.field_picture;

        DBGT_PDEBUG("H264DecNextPicture: outputPictureBusAddress %lu", picture.pictures[0].output_picture_bus_address);
        DBGT_PDEBUG("H264DecNextPicture: nChromaBusAddress %llu", frame->outBufPrivate.nChromaBusAddress);
        DBGT_PDEBUG("pLumaBase %p, pChromaBase %p", frame->outBufPrivate.pLumaBase, frame->outBufPrivate.pChromaBase);
        DBGT_PDEBUG("Luma size %lu", frame->outBufPrivate.nLumaSize);
        DBGT_PDEBUG("Chroma size %lu", frame->outBufPrivate.nChromaSize);
        frame->size = frame->outBufPrivate.nLumaSize + frame->outBufPrivate.nChromaSize;
        frame->MB_err_count = picture.nbr_of_err_mbs;
        frame->viewId = picture.view_id;

        //this->out_pic[this->out_count % MAX_BUFFERS] = picture;
        this->out_pic[this->out_index_w] = picture;
        this->out_count++;
        this->out_index_w++;
        if (this->out_index_w == MAX_BUFFERS) this->out_index_w = 0;
        this->out_num++;

        if (USE_DISPLAY_SMOOTHING && !this->pending_flush && !eos)
        {
            this->frame_sent = OMX_TRUE;
        }
        DBGT_EPILOG("");
        return CODEC_HAS_FRAME;
    }
    else if (ret == DEC_OK)
    {
        DBGT_EPILOG("");
        return CODEC_OK;
    }
    else if (ret == DEC_PARAM_ERROR)
    {
        DBGT_CRITICAL("DEC_PARAM_ERROR");
        DBGT_EPILOG("");
        return CODEC_ERROR_INVALID_ARGUMENT;
    }
    else if (ret == DEC_END_OF_STREAM)
    {
        DBGT_PDEBUG("H264DecNextPicture: End of stream");
        DBGT_EPILOG("");
        return CODEC_END_OF_STREAM;
    }
    else if (ret == DEC_ABORTED)
    {
        DBGT_PDEBUG("H264DecNextPicture: aborted");
        DBGT_EPILOG("");
        return CODEC_ABORTED;
    }
    else if (ret == DEC_FLUSHED)
    {
        DBGT_PDEBUG("H264DecNextPicture: flushed");
        DBGT_EPILOG("");
        return CODEC_FLUSHED;
    }

    DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
    DBGT_EPILOG("");
    return CODEC_ERROR_UNSPECIFIED;
}

static OMX_S32 decoder_scanframe_h264(CODEC_PROTOTYPE * arg, STREAM_BUFFER * buf,
                                  OMX_U32 * first, OMX_U32 * last)
{
    UNUSED_PARAMETER(arg);
    DBGT_PROLOG("");

    // find the first and last start code offsets.
    // returns 1 if start codes are found otherwise -1
    // this doesnt find anything if there's only a single NAL unit in the buffer?

    *first = 0;
    *last = 0;
#ifdef USE_SCANFRAME
    // scan for a NAL
    OMX_S32 i = 0;
    OMX_S32 z = 0;

    CALLSTACK;

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
        if (buf->bus_data[i] == 0 && buf->bus_data[i + 1] == 0
           && buf->bus_data[i + 2] == 1)
        {
            /* Check for leading zeros */
            while(i > 0)
            {
                if (buf->bus_data[i])
                {
                    *last = i + 1;
                    break;
                }
                --i;
            }
            DBGT_EPILOG("");
            return 1;
        }
    }
    DBGT_EPILOG("");
    return -1;
#else
        *first = 0;
        *last = buf->streamlen;
        DBGT_EPILOG("");
        return 1;
#endif
}


static CODEC_STATE decoder_endofstream_h264(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    enum DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = H264DecEndOfStream(this->instance, 1);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        case DEC_ABORTED:
            stat = CODEC_ABORTED;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

// create codec instance and initialize it
CODEC_PROTOTYPE *HantroHwDecOmx_decoder_create_h264(const void *DWLInstance,
                                        OMX_BOOL mvc_stream,
                                        OMX_VIDEO_PARAM_CONFIGTYPE *conf)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = OSAL_Malloc(sizeof(CODEC_H264));
    H264DecApiVersion decApi;
    H264DecBuild decBuild;

    memset(this, 0, sizeof(CODEC_H264));

    this->base.destroy = decoder_destroy_h264;
    this->base.decode = decoder_decode_h264;
    this->base.getinfo = decoder_getinfo_h264;
    this->base.getframe = decoder_getframe_h264;
    this->base.scanframe = decoder_scanframe_h264;
    this->base.endofstream = decoder_endofstream_h264;
    this->base.pictureconsumed = decoder_pictureconsumed_h264;
    this->base.setframebuffer = decoder_setframebuffer_h264;
    this->base.getframebufferinfo = decoder_getframebufferinfo_h264;
    this->base.abort = decoder_abort_h264;
    this->base.abortafter = decoder_abortafter_h264;
    this->base.setnoreorder = decoder_setnoreorder_h264;
    this->base.setinfo = decoder_setinfo_h264;
    this->instance = 0;
    this->picId = 0;
    this->frame_sent = OMX_FALSE;

    /* Print API version number */
    decApi = H264DecGetAPIVersion();
    decBuild = H264DecGetBuild();
    DBGT_PDEBUG("X170 H.264 Decoder API v%d.%d - SW build: %d.%d - HW build: %x",
            decApi.major, decApi.minor, decBuild.sw_build >> 16,
            decBuild.sw_build & 0xFFFF, decBuild.hw_build);
    UNUSED_PARAMETER(decApi);
    UNUSED_PARAMETER(decBuild);

    struct H264DecConfig dec_cfg;
    /* TODO: (yc) multicore should be supported in next version */
    dec_cfg.mcinit_cfg.mc_enable = 0;//conf->mc_cfg.mc_enable;
    dec_cfg.mcinit_cfg.stream_consumed_callback = NULL; //conf->mc_cfg.stream_consumed_callback;
    dec_cfg.use_ringbuffer = conf->bEnableRingBuffer;
    dec_cfg.decoder_mode = conf->eDecMode;
    dec_cfg.dpb_flags = DEC_REF_FRM_TILED_DEFAULT;
    dec_cfg.no_output_reordering = conf->bDisableReordering;
    dec_cfg.error_handling = DEC_EC_FAST_FREEZE; //conf->concealment_mode;
    dec_cfg.use_video_compressor = conf->bEnableRFC;
    dec_cfg.use_display_smoothing = 0;
    dec_cfg.use_adaptive_buffers = conf->bEnableAdaptiveBuffers;
    dec_cfg.guard_size = conf->nGuardSize;

    enum DecRet ret = H264DecInit(&this->instance,
                        DWLInstance,
                        &dec_cfg);

    if (ret == DEC_OK && mvc_stream)
        ret = H264DecSetMvc(this->instance);

    if (ret != DEC_OK)
    {
        decoder_destroy_h264((CODEC_PROTOTYPE *) this);
        DBGT_CRITICAL("H264DecInit error");
        DBGT_EPILOG("");
        return NULL;
    }

    DBGT_EPILOG("");
    return (CODEC_PROTOTYPE *) this;
}

FRAME_BUFFER_INFO decoder_getframebufferinfo_h264(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;
    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    H264DecBufferInfo info;
    FRAME_BUFFER_INFO bufInfo;
    memset(&bufInfo, 0, sizeof(FRAME_BUFFER_INFO));

    H264DecGetBufferInfo(this->instance, &info);

    bufInfo.bufferSize = info.next_buf_size;
    bufInfo.numberOfBuffers = info.buf_num;
    bufInfo.fb_bus_address = info.buf_to_free.bus_address;
    DBGT_PDEBUG("bufferSize %d, numberOfBuffers %d", (int)bufInfo.bufferSize, (int)bufInfo.numberOfBuffers);

    DBGT_EPILOG("");
    return bufInfo;
}

CODEC_STATE decoder_setframebuffer_h264(CODEC_PROTOTYPE * arg, BUFFER *buff, OMX_U32 available_buffers)
{
    CALLSTACK;

    DBGT_PROLOG("");
    UNUSED_PARAMETER(available_buffers);
    CODEC_H264 *this = (CODEC_H264 *)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    struct DWLLinearMem mem;
    H264DecBufferInfo info;
    enum DecRet ret;

    memset(&info, 0, sizeof(H264DecBufferInfo));

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
    //mem.size = NEXT_MULTIPLE(buff->allocsize, page_size);   /* physical size (rounded to page multiple) */
    //mem.logical_size = buff->allocsize;                     /* requested size in bytes */
    DBGT_PDEBUG("virtual_address %p, bus_address %lu, size %d",
    mem.virtual_address, mem.bus_address, mem.size);

    ret = H264DecAddBuffer(this->instance, &mem);
    DBGT_PDEBUG("H264DecAddBuffer ret (%d)", ret);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_WAITING_FOR_BUFFER:
            stat = CODEC_NEED_MORE;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_MEMFAIL:
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

CODEC_STATE decoder_pictureconsumed_h264(CODEC_PROTOTYPE * arg, BUFFER *buff)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    H264DecPicture pic;
    OMX_U32 i, j;

    DBGT_PDEBUG("Consumed: bus_address %lu", buff->bus_address);

    for (i=0; i<MAX_BUFFERS; i++)
    {
        if (buff->bus_address == this->out_pic[i].pictures[0].output_picture_bus_address)
        {
            DBGT_PDEBUG("Found out_pic[%d]: bus_address %lu", (int)i, buff->bus_address);
            pic = this->out_pic[i];
            enum DecRet ret = H264DecPictureConsumed(this->instance, &pic);
            DBGT_PDEBUG("H264DecPictureConsumed ret (%d)", ret);
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


CODEC_STATE decoder_abort_h264(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    enum DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = H264DecAbort(this->instance);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_abortafter_h264(CODEC_PROTOTYPE * arg)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    enum DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = H264DecAbortAfter(this->instance);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}


CODEC_STATE decoder_setnoreorder_h264(CODEC_PROTOTYPE * arg, OMX_BOOL no_reorder)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    enum DecRet ret;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    ret = H264DecSetNoReorder(this->instance, no_reorder);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE decoder_setinfo_h264(CODEC_PROTOTYPE * arg, OMX_VIDEO_PARAM_CONFIGTYPE *conf,
                                 PP_UNIT_CFG *pp_args)
{
    CALLSTACK;

    DBGT_PROLOG("");

    CODEC_H264 *this = (CODEC_H264 *)arg;
    enum DecRet ret;
    struct H264DecConfig dec_cfg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    memset(&dec_cfg, 0, sizeof(struct H264DecConfig));
    dec_cfg.decoder_mode = DEC_NORMAL;//conf->eDecMode;
    dec_cfg.align = DEC_ALIGN_16B;
    dec_cfg.use_adaptive_buffers = conf->bEnableAdaptiveBuffers;
    dec_cfg.guard_size = conf->nGuardSize;
    if (pp_args->enabled)
    {
        dec_cfg.ppu_config[0].enabled = pp_args->enabled;
        dec_cfg.ppu_config[0].crop.enabled = pp_args->crop.enabled;
        dec_cfg.ppu_config[0].crop.x = pp_args->crop.left;
        dec_cfg.ppu_config[0].crop.y = pp_args->crop.top;
        dec_cfg.ppu_config[0].crop.width = pp_args->crop.width;
        dec_cfg.ppu_config[0].crop.height = pp_args->crop.height;
        dec_cfg.ppu_config[0].out_p010 = pp_args->out_p010;
    }

    ret = H264DecSetInfo(this->instance, &dec_cfg);

    switch (ret)
    {
        case DEC_OK:
            stat = CODEC_OK;
            break;
        case DEC_PARAM_ERROR:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case DEC_STRM_ERROR:
            stat = CODEC_ERROR_STREAM;
            break;
        case DEC_NOT_INITIALIZED:
            stat = CODEC_ERROR_NOT_INITIALIZED;
            break;
        case DEC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case DEC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case DEC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYS;
            break;
        case DEC_HW_RESERVED:
            stat = CODEC_ERROR_HW_RESERVED;
            break;
        case DEC_MEMFAIL:
            stat = CODEC_ERROR_MEMFAIL;
            break;
        case DEC_STREAM_NOT_SUPPORTED:
            stat = CODEC_ERROR_STREAM_NOT_SUPPORTED;
            break;
        default:
            DBGT_PDEBUG("DecRet (%d)", ret);
            DBGT_ASSERT(!"unhandled DecRet");
            break;
    }

    DBGT_EPILOG("");
    return stat;
}
