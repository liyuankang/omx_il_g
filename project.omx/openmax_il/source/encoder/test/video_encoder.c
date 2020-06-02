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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OSAL.h>
#include <util.h>

#define BUFFER_COUNT 10
#define VIDEO_FORMAT_H264  1
#define VIDEO_FORMAT_MPEG4 2
#define VIDEO_FORMAT_H263  3
#define VIDEO_FORMAT_VP8   4

#define YUV420PackedPlanar 1
#define YUV420PackedSemiPlanar 2
#define YCbYCr 3


#define INIT_OMX_TYPE(f) \
    memset(&f, 0, sizeof(f)); \
  (f).nSize = sizeof(f);      \
  (f).nVersion.s.nVersionMajor = 1; \
  (f).nVersion.s.nVersionMinor = 1

/* Intermediate Video File Format */
#define IVF_HDR_BYTES       32
#define IVF_FRM_BYTES       12

static int output_size;
static unsigned long long frame_counter;
static int video_format;
static int width;
static int height;
static int frameRateDenom;
static int frameRateNum;
static int bitrate;
static int outputBufferNum;

OMX_ERRORTYPE HantroHwEncOmx_hantro_encoder_video_constructor(OMX_COMPONENTTYPE*, OMX_STRING);

typedef struct HEADERLIST
{
   OMX_BUFFERHEADERTYPE* hdrs[BUFFER_COUNT];
   OMX_U32 readpos;
   OMX_U32 writepos;
} HEADERLIST;

OMX_HANDLETYPE queue_mutex;
OMX_BOOL       EOS;
OMX_BOOL       PORT_SET;

HEADERLIST src_queue;
HEADERLIST dst_queue;

HEADERLIST input_queue;
HEADERLIST output_queue;
HEADERLIST input_ret_queue;
HEADERLIST output_ret_queue;

FILE* out;

void write_ivf_file_header(FILE *fpOut);
void write_ivf_frame_header(OMX_BUFFERHEADERTYPE* buffer, FILE *fpOut);

void init_list(HEADERLIST* list)
{
    memset(list, 0, sizeof(HEADERLIST));
}

void push_header(HEADERLIST* list, OMX_BUFFERHEADERTYPE* header)
{
    assert(list->writepos < BUFFER_COUNT);
    list->hdrs[list->writepos++] = header;
}

void get_header(HEADERLIST* list, OMX_BUFFERHEADERTYPE** header)
{
    if (list->readpos == list->writepos)
    {
        *header = NULL;
        return;
    }
    *header = list->hdrs[list->readpos++];
}


void copy_list(HEADERLIST* dst, HEADERLIST* src)
{
    memcpy(dst, src, sizeof(HEADERLIST));
}

OMX_ERRORTYPE fill_buffer_done(OMX_HANDLETYPE comp, OMX_PTR appdata, OMX_BUFFERHEADERTYPE* buffer)
{
    if (EOS == OMX_TRUE)
        return OMX_ErrorNone; // this should be a buffered returned on transition from executing to idle

    /*printf("Fill buffer done\n"
           "\tnFilledLen: %u nOffset: %u\n",
           (unsigned)buffer->nFilledLen,
           (unsigned)buffer->nOffset);*/

    assert(out);
    if(video_format == VIDEO_FORMAT_VP8 && buffer->nFilledLen > 0)
    {
        //printf("Write ivf frame header\n");
        write_ivf_frame_header(buffer, out);
        output_size += IVF_FRM_BYTES;
    }
        
    size_t ret = fwrite(buffer->pBuffer, 1, buffer->nFilledLen, out);
    //printf("\tWrote %u bytes\n", ret);
    if ( ret > 0 )
        frame_counter++;

    output_size += buffer->nFilledLen;
    if (buffer->nFlags & OMX_BUFFERFLAG_EOS)
    {
        printf("Encoded %u frames.\n", frame_counter );
        printf("Encoded file is %u bytes", output_size);
        printf("\tOMX_BUFFERFLAG_EOS\n");
        return OMX_ErrorNone;
    }

    buffer->nFilledLen = 0;
    buffer->nOffset    = 0;
    buffer->nFlags = 0;
    ((OMX_COMPONENTTYPE*)comp)->FillThisBuffer(comp, buffer);

    /*
    OSAL_MutexLock(queue_mutex);
    {
        if (buffer->nFlags & OMX_BUFFERFLAG_EOS)
            EOS = OMX_TRUE;
        if (output_ret_queue.writepos >= BUFFER_COUNT)
            printf("No space in return queue\n");
        else
            push_header(&output_ret_queue, buffer);

        assert(out);
        size_t ret = fwrite(buffer->pBuffer, 1, buffer->nFilledLen, out);
        printf("\tWrote %u bytes\n", ret);
    }
    OSAL_MutexUnlock(queue_mutex);
    */
    printf("Encoded %u frames.\n", frame_counter );
    return OMX_ErrorNone;
}

OMX_ERRORTYPE empty_buffer_done(OMX_HANDLETYPE comp, OMX_PTR appdata, OMX_BUFFERHEADERTYPE* buffer)
{
    //printf("Empty buffer done\n");
    OMX_VIDEO_VP8REFERENCEFRAMETYPE vp8Ref;
    OMX_CONFIG_INTRAREFRESHVOPTYPE intraRefresh;
    OMX_COMPONENTTYPE* pEnc = ((OMX_COMPONENTTYPE*)comp);
    OMX_ERRORTYPE err;

    INIT_OMX_TYPE(vp8Ref);
    INIT_OMX_TYPE(intraRefresh);

    buffer->nOffset = 0;
    buffer->nFilledLen = 0;

    OSAL_MutexLock(queue_mutex);
    {
        if (input_ret_queue.writepos >= BUFFER_COUNT)
            printf("No space in return queue\n");
        else
            push_header(&input_ret_queue, buffer);

        if(video_format == VIDEO_FORMAT_VP8)
        {
            vp8Ref.bPreviousFrameRefresh = OMX_TRUE;
            vp8Ref.bGoldenFrameRefresh = OMX_FALSE;
            vp8Ref.bAlternateFrameRefresh = OMX_FALSE;
            vp8Ref.bUsePreviousFrame = OMX_TRUE;
            vp8Ref.bUseGoldenFrame = OMX_TRUE;
            vp8Ref.bUseAlternateFrame = OMX_TRUE;
            vp8Ref.nPortIndex = 1;

            /*if(outputBufferNum%30 == 2)
                vp8Ref.bUseGoldenFrame = OMX_FALSE;

            if(outputBufferNum%30 == 4)
                vp8Ref.bUseAlternateFrame = OMX_FALSE;*/

            err = pEnc->SetConfig(pEnc, OMX_IndexConfigVideoVp8ReferenceFrame, &vp8Ref);
            assert( err == OMX_ErrorNone );

            if(outputBufferNum%150 == 0)
            {
                printf("Intra refresh true, buffer %d\n", outputBufferNum);
                intraRefresh.IntraRefreshVOP = OMX_TRUE;
                intraRefresh.nPortIndex = 1;
                err = pEnc->SetConfig(pEnc, OMX_IndexConfigVideoIntraVOPRefresh, &intraRefresh);
                assert( err == OMX_ErrorNone );
            }
            else if (outputBufferNum%150 == 1)
            {
                printf("Intra refresh false, buffer %d\n", outputBufferNum);
                intraRefresh.IntraRefreshVOP = OMX_FALSE;
                intraRefresh.nPortIndex = 1;
                err = pEnc->SetConfig(pEnc, OMX_IndexConfigVideoIntraVOPRefresh, &intraRefresh);
                assert( err == OMX_ErrorNone );
            }
        }
    }
    OSAL_MutexUnlock(queue_mutex);

    outputBufferNum++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE comp_event(OMX_HANDLETYPE comp, OMX_PTR appdata, OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2, OMX_PTR eventdata)
{
    assert(comp);
    printf("Got component event: event:%s data1:%u data2:%u\n",
        HantroOmx_str_omx_event(event),
        (unsigned)data1,
        (unsigned)data2);
    switch (event)
    {
        case OMX_EventPortSettingsChanged:
            {
                printf("\tPORT_SET: True\n");
                PORT_SET = OMX_TRUE;
            }
            break;

        case OMX_EventCmdComplete:
            {
                OMX_STATETYPE state;
                ((OMX_COMPONENTTYPE*)comp)->GetState(comp, &state);
                printf("\tState: %s\n", HantroOmx_str_omx_state(state));
            }
            break;
        case OMX_EventBufferFlag:
            {
                printf("\tEOS: True\n");
                EOS = OMX_TRUE;
            }
            break;
        default: break;

    }
    return OMX_ErrorNone;
}

OMX_COMPONENTTYPE* create_video_encoder( )
{
    OMX_ERRORTYPE err;
    OMX_COMPONENTTYPE* comp = (OMX_COMPONENTTYPE*)malloc(sizeof(OMX_COMPONENTTYPE));
    if (!comp)
    {
        err = OMX_ErrorInsufficientResources;
        goto FAIL;
    }
    memset(comp, 0, sizeof(OMX_COMPONENTTYPE));
    err = HantroHwEncOmx_hantro_encoder_video_constructor(comp, "OMX.hantro.H1.video.encoder");
    if (err != OMX_ErrorNone)
        goto FAIL;

    OMX_CALLBACKTYPE cb;
    cb.EventHandler    = comp_event;
    cb.EmptyBufferDone = empty_buffer_done;
    cb.FillBufferDone  = fill_buffer_done;

    err = comp->SetCallbacks(comp, &cb, NULL);
    if (err != OMX_ErrorNone)
        goto FAIL;

    return comp;
 FAIL:
    if (comp)
        free(comp);
    printf("error: %s\n", HantroOmx_str_omx_err(err));
    return NULL;
}

OMX_ERRORTYPE setup_component(OMX_COMPONENTTYPE* comp, int inputFormat, int outputFormat)
{
    OMX_ERRORTYPE err;

    // get the expected buffer sizes
    OMX_PARAM_PORTDEFINITIONTYPE inputPort;
    OMX_PARAM_PORTDEFINITIONTYPE outputPort;
    INIT_OMX_TYPE(inputPort);
    INIT_OMX_TYPE(outputPort);
    inputPort.nPortIndex = 0; // input port
    outputPort.nPortIndex = 1; // output port

    err = comp->GetParameter(comp, OMX_IndexParamPortDefinition, &inputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;

    inputPort.nBufferCountActual = BUFFER_COUNT;
    //176h144f30
    //352h288f25
    inputPort.format.video.nFrameWidth = width;
    inputPort.format.video.nStride = width;
    inputPort.format.video.nFrameHeight = height;
    inputPort.format.video.xFramerate = FLOAT_Q16(30);
    switch (inputFormat)
    {
        case YUV420PackedPlanar:
            inputPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
            break;

        case YUV420PackedSemiPlanar:
            inputPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
            break;

        case YCbYCr:
            inputPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
            break;

        default:
            inputPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
            break;

    }
     err = comp->SetParameter(comp, OMX_IndexParamPortDefinition, &inputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;

    if (err != OMX_ErrorNone)
        goto FAIL;

    err = comp->GetParameter(comp, OMX_IndexParamPortDefinition, &outputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;
    outputPort.nBufferCountActual = BUFFER_COUNT;// + 2; //start and end frames
    switch ( outputFormat )
    {
        case VIDEO_FORMAT_H264:
            outputPort.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
            break;
        case VIDEO_FORMAT_MPEG4:
            outputPort.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
            break;
        case VIDEO_FORMAT_H263:
            outputPort.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
            break;
        case VIDEO_FORMAT_VP8:
            outputPort.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
            break;
        default:
            outputPort.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
            break;
    }
    outputPort.format.video.nFrameWidth = width;
    outputPort.format.video.nFrameHeight = height;
    outputPort.format.video.nSliceHeight = 0;
    outputPort.format.video.nBitrate = bitrate;

    frameRateDenom = 1;
    frameRateNum = 30;

    //dst.format.video.bFlagErrorConcealment = OMX_TRUE;
    err = comp->SetParameter(comp, OMX_IndexParamPortDefinition, &outputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;

    // Get the correct output buffer size
    err = comp->GetParameter(comp, OMX_IndexParamPortDefinition, &outputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;

    assert(inputPort.nBufferSize != 0);
    assert(outputPort.nBufferSize != 0);

    OMX_U32 buffersize = outputPort.format.video.nFrameWidth * outputPort.format.video.nFrameHeight * 3/ 2;


    // create the buffers
    err = comp->SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (err != OMX_ErrorNone)
        goto FAIL;

    int i=0;

    for (i=0; i<BUFFER_COUNT; ++i)
    {
        OMX_BUFFERHEADERTYPE* header = NULL;
        err = comp->AllocateBuffer(comp, &header, 0, NULL, buffersize);
        if (err != OMX_ErrorNone)
            goto FAIL;
        assert(header);
        push_header(&src_queue, header);
        push_header(&input_queue, header);
    }

    for (i=0; i<BUFFER_COUNT; ++i)
    {
        OMX_BUFFERHEADERTYPE* header = NULL;
        err = comp->AllocateBuffer(comp, &header, 1, NULL, outputPort.nBufferSize );
        if (err != OMX_ErrorNone)
            goto FAIL;
        assert(header);
        push_header(&dst_queue, header);
        push_header(&output_queue, header);
    }

    // should have transition to idle state now

    OMX_STATETYPE state = OMX_StateLoaded;
    while (state != OMX_StateIdle && state != OMX_StateInvalid)
    {
        err = comp->GetState(comp, &state);
        if (err != OMX_ErrorNone)
            goto FAIL;
    }
    if (state == OMX_StateInvalid)
        goto FAIL;

    err = comp->SendCommand(comp, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (err != OMX_ErrorNone)
        goto FAIL;

    while (state != OMX_StateExecuting && state != OMX_StateInvalid)
    {
        err = comp->GetState(comp, &state);
        if (err != OMX_ErrorNone)
            goto FAIL;
    }
    if (state == OMX_StateInvalid)
        goto FAIL;

    return OMX_ErrorNone;
 FAIL:
    // todo: should deallocate buffers and stuff
    printf("error: %s\n", HantroOmx_str_omx_err(err));
    return err;
}


void destroy_video_encoder(OMX_COMPONENTTYPE* comp)
{
    OMX_STATETYPE state = OMX_StateExecuting;
    comp->GetState(comp, &state);
    if (state == OMX_StateExecuting)
    {
        comp->SendCommand(comp, OMX_CommandStateSet, OMX_StateIdle, NULL);
        while (state != OMX_StateIdle)
            comp->GetState(comp, &state);
    }
    if (state == OMX_StateIdle)
        comp->SendCommand(comp, OMX_CommandStateSet, OMX_StateLoaded, NULL);

    int i;
    for (i=0; i<BUFFER_COUNT; ++i)
    {
        OMX_BUFFERHEADERTYPE* src_hdr = NULL;
        OMX_BUFFERHEADERTYPE* dst_hdr = NULL;
        get_header(&src_queue, &src_hdr);
        get_header(&dst_queue, &dst_hdr);
        comp->FreeBuffer(comp, 0, src_hdr);
        comp->FreeBuffer(comp, 1, dst_hdr);
    }

    while (state != OMX_StateLoaded)
        comp->GetState(comp, &state);

    comp->ComponentDeInit(comp);
    free(comp);
}



void encode_clean_video(const char* input_file, const char* output_file, int encoding, int colorformat, 
                        int input_width, int input_height, int target_bitrate)
{
    init_list(&src_queue);
    init_list(&dst_queue);
    init_list(&input_queue);
    init_list(&output_queue);
    init_list(&input_ret_queue);
    init_list(&output_ret_queue);

    //int frame_counter = 0;

    printf("video file: %s\nyuv file: %s\n", input_file, output_file);

    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pEnc = NULL;
    FILE* inputFile = NULL;
    FILE* outputFile   = NULL;

    char buff[256];
    memset(buff, 0, sizeof(buff));

    inputFile = fopen(input_file, "rb");
    if (inputFile==NULL)
    {
        printf("failed to open input file\n");
        goto FAIL;
    }

    outputFile = fopen(output_file, "wb");
    if (outputFile==NULL)
    {
        printf("failed to open output output file\n");
        goto FAIL;
    }
    out = outputFile;
    
    width = input_width;
    height = input_height;
    bitrate = target_bitrate;

    if(video_format == VIDEO_FORMAT_VP8)
    {
        //printf("Write ivf file header\n");
        write_ivf_file_header(out);
        output_size += IVF_HDR_BYTES;
    }

    pEnc = create_video_encoder();
    if (!pEnc)
        goto FAIL;

    if (setup_component(pEnc, colorformat, encoding) != OMX_ErrorNone)
        goto FAIL;

    OMX_BUFFERHEADERTYPE* outputBuffer = NULL;
    do
    {
        get_header(&output_queue, &outputBuffer);
        if (outputBuffer)
        {
            outputBuffer->nOutputPortIndex = 1;
            err = pEnc->FillThisBuffer(pEnc, outputBuffer);
            assert(err == OMX_ErrorNone);
        }
    }
    while (outputBuffer);


    OMX_BOOL eof = OMX_FALSE;
    while (eof == OMX_FALSE)
    {
        OMX_BUFFERHEADERTYPE* inputBuffer  = NULL;
        get_header(&input_queue, &inputBuffer);
        if (inputBuffer==NULL)
        {
            OSAL_MutexLock(queue_mutex);
            copy_list(&input_queue, &input_ret_queue);
            init_list(&input_ret_queue);
            OSAL_MutexUnlock(queue_mutex);
            get_header(&input_queue, &inputBuffer);
            if (inputBuffer==NULL)
            {
                //printf("output buffer not available\n");
                usleep(1000*1000);
                continue;
                //goto FAIL;
            }
        }
        assert(inputBuffer);
        inputBuffer->nInputPortIndex   = 0;

        OMX_U32 buffersize = width * height * 3/  2;

        //size_t ret = fread(inputBuffer->pBuffer, 1, inputBuffer->nAllocLen, inputFile);
        size_t ret = fread(inputBuffer->pBuffer, 1, buffersize, inputFile);
        //printf("inputBuffer->nAllocLen = %d\n", (int)inputBuffer->nAllocLen);
        //printf("read %d\n", ret);

        inputBuffer->nOffset    = 0;
        inputBuffer->nFilledLen = ret;
        if ( feof(inputFile) ) // || ret == 0 )
        {
            printf("feof = true\n");
            eof = OMX_TRUE;
        }
        if ( eof )
            inputBuffer->nFlags = inputBuffer->nFlags | OMX_BUFFERFLAG_EOS;

        err = pEnc->EmptyThisBuffer(pEnc, inputBuffer);
        assert( err == OMX_ErrorNone );
        usleep(0);
    }

    // get stream end event
    EOS = OMX_FALSE;
    while (EOS == OMX_FALSE)
    {
        usleep(1000);
    }

    if(video_format == VIDEO_FORMAT_VP8)
    {
        //printf("Write ivf file header\n");
        write_ivf_file_header(out);
    }
    
    printf("encoding done\n");
 FAIL:
    if (inputFile) fclose(inputFile);
    if (outputFile)   fclose(outputFile);
    if (pEnc)   destroy_video_encoder(pEnc);
}

#define CMP_BUFF_SIZE 10*1024

void compare_output(const char* temp, const char* reference)
{
    printf("comparing files\n");
    printf("temporary file:%s\nreference file:%s\n", temp, reference);

    char* buff_tmp = NULL;
    char* buff_ref = NULL;
    FILE* file_tmp = fopen(temp, "rb");
    FILE* file_ref = fopen(reference, "rb");
    if (!file_tmp)
    {
        printf("failed to open temp file\n");
        goto FAIL;
    }
    if (!file_ref)
    {
        printf("failed to open reference file\n");
        goto FAIL;
    }
    fseek(file_tmp, 0, SEEK_END);
    fseek(file_ref, 0, SEEK_END);
    int tmp_size = ftell(file_tmp);
    int ref_size = ftell(file_ref);
    int min_size = tmp_size;
    if (tmp_size != ref_size)
    {
        min_size = tmp_size < ref_size ? tmp_size : ref_size;
        printf("file sizes do not match: temp: %d reference: %d bytes\n", tmp_size, ref_size);
        printf("comparing first %d bytes\n", min_size);
    }
    fseek(file_tmp, 0, SEEK_SET);
    fseek(file_ref, 0, SEEK_SET);

    buff_tmp = (char*)malloc(CMP_BUFF_SIZE);
    buff_ref = (char*)malloc(CMP_BUFF_SIZE);

    int pos = 0;
    int min = 0;
    int rem = 0;
    while (pos < min_size)
    {
        rem = min_size - pos;
        min = rem < CMP_BUFF_SIZE ? rem : CMP_BUFF_SIZE;
        fread(buff_tmp, min, 1, file_tmp);
        fread(buff_ref, min, 1, file_ref);

        if (memcmp(buff_tmp, buff_ref, min))
        {
            printf("mismatch found\n");

        }
        pos += min;
    }
    printf("file cmp done\n");
 FAIL:
    if (file_tmp) fclose(file_tmp);
    if (file_ref) fclose(file_ref);
    if (buff_tmp) free(buff_tmp);
    if (buff_ref) free(buff_ref);
}

void write_ivf_file_header(FILE *fpOut)
{
    OMX_U8 data[IVF_HDR_BYTES] = {0};

    /* File header signature */
    data[0] = 'D';
    data[1] = 'K';
    data[2] = 'I';
    data[3] = 'F';

    /* File format version and file header size */
    data[6] = 32;

    /* Video data FourCC */
    data[8] = 'V';
    data[9] = 'P';
    data[10] = '8';
    data[11] = '0';

    /* Video Image width and height */
    data[12] = width & 0xff;
    data[13] = (width >> 8) & 0xff;
    data[14] = height & 0xff;
    data[15] = (height >> 8) & 0xff;

    /* Frame rate rate */
    data[16] = frameRateNum & 0xff;
    data[17] = (frameRateNum >> 8) & 0xff;
    data[18] = (frameRateNum >> 16) & 0xff;
    data[19] = (frameRateNum >> 24) & 0xff;

    /* Frame rate scale */
    data[20] = frameRateDenom & 0xff;
    data[21] = (frameRateDenom >> 8) & 0xff;
    data[22] = (frameRateDenom >> 16) & 0xff;
    data[23] = (frameRateDenom >> 24) & 0xff;

    /* Video length in frames */
    data[24] = frame_counter & 0xff;
    data[25] = (frame_counter >> 8) & 0xff;
    data[26] = (frame_counter >> 16) & 0xff;
    data[27] = (frame_counter >> 24) & 0xff;

    /* The Ivf "File Header" is in the beginning of the file */
    rewind(fpOut);
    fwrite(data, 1, IVF_HDR_BYTES, fpOut);
}

void write_ivf_frame_header(OMX_BUFFERHEADERTYPE* buffer, FILE *fpOut)
{
    int byteCnt = 0;
    OMX_U8 data[IVF_FRM_BYTES];

    /* Frame size (4 bytes) */
    byteCnt = buffer->nFilledLen;
    data[0] =  byteCnt        & 0xff;
    data[1] = (byteCnt >> 8)  & 0xff;
    data[2] = (byteCnt >> 16) & 0xff;
    data[3] = (byteCnt >> 24) & 0xff;

    /* Timestamp (8 bytes) */
    data[4]  =  (frame_counter)        & 0xff;
    data[5]  = ((frame_counter) >> 8)  & 0xff;
    data[6]  = ((frame_counter) >> 16) & 0xff;
    data[7]  = ((frame_counter) >> 24) & 0xff;
    data[8]  = ((frame_counter) >> 32) & 0xff;
    data[9]  = ((frame_counter) >> 40) & 0xff;
    data[10] = ((frame_counter) >> 48) & 0xff;
    data[11] = ((frame_counter) >> 56) & 0xff;

    fwrite(data, 1, IVF_FRM_BYTES, fpOut);
}

int main(int argc, const char* argv[])
{
    printf("OMX component test video encoder\n");
    printf(__DATE__", "__TIME__"\n");

    OMX_ERRORTYPE err = OSAL_MutexCreate(&queue_mutex);
    if (err != OMX_ErrorNone)
    {
        printf("mutex create error: %s\n", HantroOmx_str_omx_err(err));
        return 1;
    }
    output_size = 0;
    outputBufferNum = 0;

    int i = 0;

    if ( argc == 2 )
    {
        //printf( argv );    wtf?
        i = atoi(argv[1]);
    }

    switch ( i )
    {
        case 1:   //H264 cases:
            video_format = VIDEO_FORMAT_H264;
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", "packedplanar.h264", VIDEO_FORMAT_H264, YUV420PackedPlanar);
            encode_clean_video("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.h264", VIDEO_FORMAT_H264, YUV420PackedPlanar, 352, 288, 128000);
            //encode_clean_video("testdata_encoder/yuv/cif/out_w640h480.yuv", "temppi.h264", VIDEO_FORMAT_H264, YUV420PackedPlanar);
            //encode_clean_video("testdata_encoder/yuv/cif/out_w640h480.yuv", "temppi.h264", VIDEO_FORMAT_H264, YCbYCr);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuvsp", "packedsemiplanar.h264", VIDEO_FORMAT_H264, YUV420PackedSemiPlanar);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuyv422", "YCbYCr.h264", VIDEO_FORMAT_H264, YCbYCr);
            //compare_output("temp.h264", "testdata_encoder/case_1900/stream.h264");
            break;
        case 2: //MPEG4 cases:
            video_format = VIDEO_FORMAT_MPEG4;
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", "packedplanar.mpeg4", VIDEO_FORMAT_MPEG4, YUV420PackedPlanar);
            encode_clean_video("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.mp4", VIDEO_FORMAT_MPEG4, YUV420PackedPlanar, 352, 288, 128000);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuvsp", "packedsemiplanar.mpeg4", VIDEO_FORMAT_MPEG4, YUV420PackedSemiPlanar);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuyv422", "YCbYCr.mpeg4", VIDEO_FORMAT_MPEG4, YCbYCr);
            break;
        case 3: //H263 cases:
            video_format = VIDEO_FORMAT_H263;
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuv", "packedplanar.h263", VIDEO_FORMAT_H263, YUV420PackedPlanar);
            encode_clean_video("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.h263", VIDEO_FORMAT_H263, YUV420PackedPlanar, 352, 288, 128000);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuvsp", "packedsemiplanar.h263", VIDEO_FORMAT_H263, YUV420PackedSemiPlanar);
            //encode_clean_video("testdata_encoder/yuv/synthetic/kuuba_random_w352h288.yuyv422", "YCbYCr.h263", VIDEO_FORMAT_H263, YCbYCr);
            break;
        case 4: //VP8 cases:
            video_format = VIDEO_FORMAT_VP8;
            //encode_clean_video("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.vp8", VIDEO_FORMAT_VP8, YUV420PackedPlanar, 352, 288, 128000);
            encode_clean_video("testdata_encoder/yuv/vga/tanya_640x480.yuv", "tanyapackedplanar.vp8", VIDEO_FORMAT_VP8, YUV420PackedPlanar, 640, 480, 1024000);
            break;
        default:
            break;
    }
    OSAL_MutexDestroy(queue_mutex);

    return 0;
}
