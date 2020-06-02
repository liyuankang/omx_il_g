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
#include "encoder_h263.h"
#include "codec.h"
#include "mp4encapi.h"
#include "util.h"
#include <OSAL.h>

////////////////////////////////////////////////////////////
// test cases
////////////////////////////////////////////////////////////

void test_541(const char *filename, int width, int height, int lastVop)
{
    printf("Starting test 541.\n");

    unsigned int datalen = 0;
    int frames = 0;

    // input buffer needs to be able to accomodate 2 frames
    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = frame.fb_frameSize;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_bufferSize);
    bzero(frame.fb_bus_data, frame.fb_bufferSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    // output stream
    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;

    H263_CONFIG config;

    config.nVideoRange = 1;
    
    config.h263_config.eProfile = OMX_VIDEO_H263ProfileBaseline;
    config.h263_config.eLevel = OMX_VIDEO_H263Level50;
    config.h263_config.nPFrames = 0; // I frame interval
    config.h263_config.bPLUSPTYPEAllowed = OMX_FALSE;
    //config.h263_config.nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeB;

    // the following used to set stream type if bSvh or bReversibleVLC are
    // not enabled.
    config.error_ctrl_config.bEnableDataPartitioning = OMX_FALSE;
    config.error_ctrl_config.bEnableResync = OMX_FALSE;
    config.error_ctrl_config.nResynchMarkerSpacing = 0;
    
    config.common_config.nOutputWidth = 176;
    config.common_config.nOutputHeight = 144;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.rate_config.eRateControl = OMX_Video_ControlRateVariable;
    config.rate_config.nPictureRcEnabled = 1;
    config.rate_config.nMbRcEnabled = 1; 
    config.rate_config.nQpDefault = 15; 
    config.rate_config.nQpMin = 1; 
    config.rate_config.nQpMax = 31; 
    config.rate_config.nVbvEnabled = 0;
    config.rate_config.nTargetBitrate = 512000;

    config.pp_config.origWidth = width;
    config.pp_config.origHeight = height;
    config.pp_config.xOffset = 0;
    config.pp_config.yOffset = 0;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    config.pp_config.angle = 0;
    config.pp_config.frameStabilization = OMX_FALSE;    

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h263(&config);
    
    assert(i);

    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_541.mpeg4", "wb");
    assert(out);

    s = i->stream_start(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);

    frame.frame_type = PREDICTED_FRAME;
    if ( !in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        // need to read two frames at first
        datalen = fread(frame.fb_bus_data, 1, frame.fb_bufferSize, in);
        
        while (frames <= lastVop && (datalen == frame.fb_frameSize || datalen == frame.fb_bufferSize))
        {
            s = i->encode(i, &frame, &stream);
            
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
                frames++;
            }
        }       
    }
    printf("\nDone\n");

    s = i->stream_end(i, &stream);
    fwrite(stream.bus_data, stream.streamlen, 1, out);
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("encoding ended. %d frames encoded.\n", frames);
}


int main(int argc, char **argv)
{
    test_541( "testdata_encoder/yuv/qcif/metro2_25fps_qcif.yuv", 176, 144, 30);
    return 0;
}
