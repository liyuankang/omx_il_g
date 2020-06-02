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

#include "codec.h"
#include "encoder_jpeg.h"
#include "util.h"
#include "jpegencapi.h"
#include <OSAL.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

//#define OMX_ENCODER_IMAGE_DOMAIN 1


////////////////////////////////////////////////////////////
// test cases
////////////////////////////////////////////////////////////

//
// create, decode, destroy
//
void test_2004(const char *filename, int width, int height)
{
    printf("Starting test 2004.\n");
    
    unsigned int datalen = 0;

    OMX_U32 sliceHeight = 112;
    
    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = width*height*3/2;
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
    
    JPEG_CONFIG cfg;

    /* Step 1: Configuration for codec init */
    cfg.qLevel = 4;
    cfg.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    cfg.pp_config.angle = 0; // no rotation 
    
    /* Step 2: Configuration of encoder (to mode) */
    cfg.pp_config.origWidth = width;    
    cfg.pp_config.origHeight = height;
    cfg.codingWidth = width;
    cfg.codingHeight = height;
    cfg.pp_config.xOffset = 0;
    cfg.pp_config.yOffset = 0;
    cfg.unitsType = JPEGENC_DOTS_PER_INCH;
    cfg.markerType = JPEGENC_MULTI_MARKER;
    cfg.codingType = JPEGENC_SLICED_FRAME;
    cfg.xDensity = 72;
    cfg.yDensity = 72;
    cfg.bAddHeaders = OMX_TRUE;
    cfg.sliceHeight = sliceHeight;
    
    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_jpeg(&cfg);

    assert(i);
    
    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_2004.jpg", "wb");
    assert(out);

    //s = i->stream_start(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);
    OMX_U32 totalSize = 0;
    
    if ( !in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {       
        // read whole frame
        datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
        
        FRAME slice;
        bzero( &slice, sizeof(FRAME) );
        slice.fb_frameSize = width*sliceHeight*3/2;
        slice.fb_bufferSize = width*sliceHeight*3/2;
        slice.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(slice.fb_bufferSize);
        bzero(slice.fb_bus_data, slice.fb_bufferSize);
        slice.fb_bus_address = (OSAL_BUS_WIDTH) slice.fb_bus_data;      
        
        OMX_BOOL doLoop = OMX_TRUE;     
        
        OMX_U32 origLumSize = width * height;
        OMX_U32 origCbCrSize = width * height / 4;
        
        OMX_U32 tmpSliceHeight = 0;
        
        int e=0;
        while (doLoop)
        {   
            // Last slice may be smalled than other so
            // calculate height of slice. 
            tmpSliceHeight = sliceHeight;
            if ((sliceHeight * (e + 1)) < (OMX_U32)height)
            {
                tmpSliceHeight = sliceHeight;               
            }
            else 
            {
                tmpSliceHeight = height - (e * sliceHeight);
            }
            
            assert((tmpSliceHeight % 16) == 0);
            
            printf("Slice height %d\n", (int)tmpSliceHeight);
            
            OMX_U32 sliceLumSize = width * tmpSliceHeight;
            OMX_U32 sliceCbCrSize = (width * tmpSliceHeight) / 4;           
                    
            // copy luminance
            memcpy( slice.fb_bus_data, 
                    frame.fb_bus_data + (e * (width * sliceHeight)), 
                    sliceLumSize );
            
            // copy Cb after luminance
            memcpy( slice.fb_bus_data + sliceLumSize, 
                    frame.fb_bus_data + origLumSize + (e * ((width * sliceHeight) / 4)), 
                    sliceCbCrSize );
            
            // copy Cr after Cb
            memcpy( slice.fb_bus_data + sliceLumSize + sliceCbCrSize, 
                    frame.fb_bus_data + origLumSize + origCbCrSize + (e * ((width * sliceHeight) / 4)), 
                    sliceCbCrSize );
                        
            // encode slice
            s = i->encode(i, &slice, &stream);
            
            switch (s)
            {
            case CODEC_OK:
                printf("Frame encoded (%d bytes).\n", (int)stream.streamlen);
                doLoop = OMX_FALSE;
                break;
            case CODEC_CODED_SLICE:
                printf("Slice encoded (%d bytes).\n", (int)stream.streamlen);
                doLoop = OMX_TRUE;
                break;
            default:
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
                totalSize += stream.streamlen;
            }
            
            e++;
        }
        
        OMX_OSAL_Free(slice.fb_bus_data);
    }

    //s = i->stream_end(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. Total %d bytes.\n", (int)totalSize);
}

void test_2005(const char *filename, int width, int height)
{
    printf("Starting test 2005.\n");
    
    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = width*height*3/2;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_frameSize);
    bzero(frame.fb_bus_data, frame.fb_frameSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;
    
    JPEG_CONFIG cfg;

    /* Step 1: Configuration for codec init */
    cfg.qLevel = 2;
    cfg.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    cfg.pp_config.angle = 0; // no rotation
    cfg.sliceHeight = 0; // no slices   
    
    /* Step 2: Configuration of encoder (to mode) */
    cfg.pp_config.origWidth = width;    
    cfg.pp_config.origHeight = height;
    cfg.codingWidth = 352;
    cfg.codingHeight = 288;
    cfg.pp_config.xOffset = 160;
    cfg.pp_config.yOffset = 96;
    cfg.unitsType = JPEGENC_DOTS_PER_INCH;
    cfg.markerType = JPEGENC_MULTI_MARKER;
    cfg.codingType = JPEGENC_WHOLE_FRAME;
    cfg.xDensity = 72;
    cfg.yDensity = 72;
    cfg.bAddHeaders = OMX_TRUE; //NOTE check this

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_jpeg(&cfg);

    assert(i);
    
    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_2005.jpg", "wb");
    assert(out);

    //s = i->stream_start(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);

    if ( !in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
        //while (datalen == frame.fb_frameSize)
        //{
            s = i->encode(i, &frame, &stream);
            
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
        //}
    }

    //s = i->stream_end(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

void test_2600(const char *filename, int width, int height)
{
    printf("Starting test 2600.\n");
    
    unsigned int datalen = 0;
    int frames = 0;

    FRAME frame;
    bzero( &frame, sizeof(FRAME));
    frame.fb_frameSize = width*height*3/2;
    frame.fb_bufferSize = width*height*3/2;
    frame.fb_bus_data = (OMX_U8*) OMX_OSAL_Malloc(frame.fb_frameSize);
    bzero(frame.fb_bus_data, frame.fb_frameSize);
    frame.fb_bus_address = (OSAL_BUS_WIDTH) frame.fb_bus_data;

    STREAM_BUFFER stream;
    bzero( &stream, sizeof(STREAM_BUFFER));
    stream.buf_max_size = 1024*1024;
    stream.bus_data = (OMX_U8*) OMX_OSAL_Malloc(stream.buf_max_size);
    bzero(stream.bus_data, stream.buf_max_size);
    stream.bus_address = (OSAL_BUS_WIDTH) stream.bus_data;
    
    CODEC_STATE s = CODEC_ERROR_UNSPECIFIED;
    
    JPEG_CONFIG cfg;

    /* Step 1: Configuration for codec init */
    cfg.qLevel = 5;
    cfg.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
    cfg.pp_config.angle = 90; // no rotation
    cfg.sliceHeight = 0; // no slices   
    
    /* Step 2: Configuration of encoder (to mode) */
    cfg.pp_config.origWidth = width;    
    cfg.pp_config.origHeight = height;
    //cfg.codingWidth = 352;
    //cfg.codingHeight = 288;
    cfg.codingWidth = 288; // rotated
    cfg.codingHeight = 352; // rotated
    cfg.pp_config.xOffset = 0;
    cfg.pp_config.yOffset = 0;
    cfg.unitsType = JPEGENC_DOTS_PER_INCH;
    cfg.markerType = JPEGENC_MULTI_MARKER;
    cfg.codingType = JPEGENC_WHOLE_FRAME;
    cfg.xDensity = 72;
    cfg.yDensity = 72;
    cfg.bAddHeaders = OMX_TRUE; // NOTE check this

    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_jpeg(&cfg);

    assert(i);
    
    FILE* in = fopen(filename, "rb");
    assert(in);
    FILE* out = fopen("test_2600.jpg", "wb");
    assert(out);

    //s = i->stream_start(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);

    if ( !in)
    {
        printf("file %s not found.\n", filename);
    }
    else
    {
        datalen = fread(frame.fb_bus_data, 1, frame.fb_frameSize, in);
        //while (datalen == frame.fb_frameSize)
        //{
            s = i->encode(i, &frame, &stream);
            
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
        //}
    }

    //s = i->stream_end(i, &stream);
    //fwrite(stream.bus_data, stream.streamlen, 1, out);
    
    i->destroy(i);

    OMX_OSAL_Free(stream.bus_data);
    OMX_OSAL_Free(frame.fb_bus_data);

    fclose(in);
    fclose(out);

    printf("Encoding ended. %d frames encoded.\n", frames);
}

int main(int argc, char **argv)
{
    test_2004( "testdata_encoder/yuv/vga/jaa_w640h480.yuv", 640, 480);
    test_2005( "testdata_encoder/yuv/misc/kuuba_maisema_w704h576.yuv", 704, 576);
    test_2600( "testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", 352, 288);

    return 0;
}
