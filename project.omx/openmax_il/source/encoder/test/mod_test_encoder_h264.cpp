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
#include <assert.h>
#include "encoder_h264.h"
#include "codec.h"
#include "h264encapi.h"
#include "util.h"
#include <OSAL.h>

////////////////////////////////////////////////////////////
// test cases
////////////////////////////////////////////////////////////

void test_1153(const char *filename, int width, int height, int lastVop)
{

    printf("Starting test 1153.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    config.common_config.nOutputWidth = 112;
    config.common_config.nOutputHeight = 96;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 10;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
    config.rate_config.nPictureRcEnabled = 0;
    config.rate_config.nMbRcEnabled = 0;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 3;
    config.pp_config.yOffset = 1;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1153.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            printf("Encoded frame %d\n", frames);

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_1160(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1160.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    //config.common_config.nOutputWidth = 112;
    //config.common_config.nOutputHeight = 96;
    config.common_config.nOutputWidth = 96;
    config.common_config.nOutputHeight = 112;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 10;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
    config.rate_config.nPictureRcEnabled = 0;
    config.rate_config.nMbRcEnabled = 0;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 2;
    config.pp_config.yOffset = 1;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 90;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1160.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            printf("Encoded frame %d\n", frames);

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_1188(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1188.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    //config.common_config.nOutputWidth = 112;
    config.common_config.nOutputWidth = 96;
    //config.common_config.nOutputHeight = 96;
    config.common_config.nOutputHeight = 112;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 10;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
    config.rate_config.nPictureRcEnabled = 0;
    config.rate_config.nMbRcEnabled = 0;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 6;
    config.pp_config.yOffset = 3;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedSemiPlanar;
    config.pp_config.angle = 90;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1188.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            printf("Encoded frame %d\n", frames);

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_1205(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1205.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = (width * height) * 2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    config.common_config.nOutputWidth = 112;
    //config.common_config.nOutputWidth = 96;
    config.common_config.nOutputHeight = 96;
    //config.common_config.nOutputHeight = 112;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 10;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
    config.rate_config.nPictureRcEnabled = 0;
    config.rate_config.nMbRcEnabled = 0;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 7;
    config.pp_config.yOffset = 3;
    config.pp_config.formatType = OMX_COLOR_FormatYCbYCr;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1205.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            printf("Encoded frame %d\n", frames);

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}


void test_1764(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1764.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize * 2;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    config.common_config.nOutputWidth = 320;
    config.common_config.nOutputHeight = 256;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 35;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
    config.rate_config.nPictureRcEnabled = 0;
    config.rate_config.nMbRcEnabled = 0;
    config.rate_config.nHrdEnabled = 0;

    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 16;
    config.pp_config.yOffset = 16;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_TRUE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1764.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);

    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize || datalen == frame.fb_bufferSize))
        {
            s = i->encode(i, &frame, &stream);
            
            if ((frames % 25) == 0)
            {
                printf("Encoded frame %d\n", frames);
            }

            switch (s)
            {
            case CODEC_CODED_INTRA:
                //printf("I");
                break;
            case CODEC_CODED_PREDICTED:
                //printf("P");
                break;
            case CODEC_OK:
                //printf("_");
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }
            
            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);
                
                // move data to beginning of buffer
                memcpy(frame.fb_bus_data, (frame.fb_bus_data + frame.fb_frameSize + 1), frame.fb_frameSize);
                
                datalen = fread(frame.fb_bus_data + frame.fb_frameSize, 1, frame.fb_frameSize, in);
                frames++;

                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    printf("\n");
    
    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_1900(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1900.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    config.common_config.nOutputWidth = 176;
    config.common_config.nOutputHeight = 144;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 35;
    config.rate_config.nQpMin = 0;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 293333;
    config.rate_config.nPictureRcEnabled = 1;
    config.rate_config.nMbRcEnabled = 1;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 0;
    config.pp_config.yOffset = 0;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 0;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1900.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_1925(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 1925.\n");

    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H264_CONFIG config;

    config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
    config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

    config.common_config.nOutputWidth = 352;
    config.common_config.nOutputHeight = 288;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.nSliceHeight = 0; // no slices
    config.bDisableDeblocking = OMX_FALSE;
    config.bSeiMessages = OMX_FALSE; // not supported???

    //config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
    config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
    config.rate_config.nQpDefault = 35;
    config.rate_config.nQpMin = 10;
    config.rate_config.nQpMax = 51;
    config.rate_config.nTargetBitrate = 384000;
    config.rate_config.nPictureRcEnabled = 1;
    config.rate_config.nMbRcEnabled = 1;
    config.rate_config.nHrdEnabled = 0;
    
    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 0;
    config.pp_config.yOffset = 0;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_FALSE;

    config.nPFrames = 25;
    //config.bNalEnabled = OMX_FALSE;
    //config.nNalSize = 0;

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_1925.h264", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    frame.frame_type = PREDICTED_FRAME;
    if (!in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        while ((frames <= lastVop) && (datalen == frame.fb_frameSize))
        {
            s = i->encode(i, &frame, &stream);
            frames++;

            if ((frames % 25) == 0)
            {
                printf("Encoded frame %d\n", frames);
            }

            switch (s)
            {
            case CODEC_CODED_INTRA:
                printf("I frame encoded at %d\n", frames);
                break;
            case CODEC_CODED_PREDICTED:
                break;
            case CODEC_OK:
                break;
            case CODEC_ERROR_HW_TIMEOUT:
                printf("HW timeout error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_BUS_ERROR:
                printf("HW bus error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_HW_RESET:
                printf("HW reset error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_SYSTEM:
                printf("System error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_UNSPECIFIED:
                printf("Unspecified error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_RESERVED:
                printf("Reserved error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_ARGUMENT:
                printf("Invalid argument error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_BUFFER_OVERFLOW:
                printf("Buffer overflow error at %d: %d\n", frames, s);
                break;
            case CODEC_ERROR_INVALID_STATE:
                printf("Invalid state error at %d: %d\n", frames, s);
                break;
            default:
                printf("Error at %d: %d\n", frames, s);
                break;
            }

            if (s < 0)
            {
                datalen = 0;
            }
            else
            {
                fwrite(stream.bus_data, stream.streamlen, 1, out);              
                datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
                /*
                if (frames % 25 == 0)
                {
                    frame.frame_type = INTRA_FRAME;
                }
                else
                {
                    frame.frame_type = PREDICTED_FRAME;
                }
                */
            }       

        }
    }

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}


int main(int argc, char **argv)
{
    test_1153( "testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", 352, 288, 1);
    test_1160( "testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", 352, 288, 1);
    test_1188( "testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuvsp", 352, 288, 1);
    test_1205( "testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuyv422", 352, 288, 1);
    test_1764( "testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", 352, 288, 404);
    test_1900( "testdata_encoder/yuv/qcif/stockholm_30fps_w176h144.yuv", 176, 144, 30);
    test_1925( "testdata_encoder/yuv/cif/torielamaa_25fps_cif.yuv", 352, 288, 640);
    
    return 0;
}
