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

#define BUFFER_COUNT 4 
#define IMAGE_FORMAT_JPEG  1
#define IMAGE_FORMAT_WEBP  2


#define YUV420PackedPlanar 1
#define YUV420PackedSemiPlanar 2
#define YCbYCr 3

#define OMX_DECODER_IMAGE_DOMAIN 1


#define INIT_OMX_TYPE(f) \
    memset(&f, 0, sizeof(f)); \
  (f).nSize = sizeof(f);      \
  (f).nVersion.s.nVersionMajor = 1; \
  (f).nVersion.s.nVersionMinor = 1

#define RIFF_HEADER_SIZE 20
#define WEBP_METADATA_SIZE 12
  
static int writelaskuri;
static int framelaskuri;
static int imageSize;
static int image_format;
static int width;
static int height;

OMX_ERRORTYPE HantroHwEncOmx_hantro_encoder_image_constructor(OMX_COMPONENTTYPE*, OMX_STRING);

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

void write_webp_file_header(FILE *fpOut);

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

    printf("Fill buffer done\n"
           "\tnFilledLen: %u nOffset: %u\n", 
           (unsigned)buffer->nFilledLen,
           (unsigned)buffer->nOffset);

    assert(out);
    size_t ret = fwrite(buffer->pBuffer, 1, buffer->nFilledLen, out);
    printf("\tWrote %u bytes\n", ret);
    if ( ret > 0 )
        framelaskuri++;
        
    writelaskuri += buffer->nFilledLen;

    if (buffer->nFilledLen > 0)
        imageSize =  buffer->nFilledLen;

    if (buffer->nFlags & OMX_BUFFERFLAG_EOS)
    {
        printf("Encoded %u frames.\n", framelaskuri ); 
        printf("Encoded file is %u bytes", writelaskuri); 
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
    printf("Encoded %u frames.\n", framelaskuri );
    return OMX_ErrorNone;
}

OMX_ERRORTYPE empty_buffer_done(OMX_HANDLETYPE comp, OMX_PTR appdata, OMX_BUFFERHEADERTYPE* buffer)
{
    printf("Empty buffer done\n");
    
    buffer->nOffset = 0;
    buffer->nFilledLen = 0;
    
    OSAL_MutexLock(queue_mutex);
    {
        if (input_ret_queue.writepos >= BUFFER_COUNT)
            printf("No space in return queue\n");
        else
            push_header(&input_ret_queue, buffer);
    }
    OSAL_MutexUnlock(queue_mutex);
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

OMX_COMPONENTTYPE* create_image_encoder( )
{
    OMX_ERRORTYPE err;
    OMX_COMPONENTTYPE* comp = (OMX_COMPONENTTYPE*)malloc(sizeof(OMX_COMPONENTTYPE));
    if (!comp)
    {
        err = OMX_ErrorInsufficientResources;
        goto FAIL;
    }
    memset(comp, 0, sizeof(OMX_COMPONENTTYPE));
    err = HantroHwEncOmx_hantro_encoder_image_constructor(comp, "OMX.hantro.H1.image.encoder");
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

OMX_ERRORTYPE setup_component(OMX_COMPONENTTYPE* comp, int inputFormat, int outputFormat )
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
    inputPort.format.image.nFrameWidth = width;
    inputPort.format.image.nStride = width;
    inputPort.format.image.nFrameHeight = height;  
    inputPort.format.image.nSliceHeight = 0;
    switch (inputFormat)
    {   
        case YUV420PackedPlanar:        
            inputPort.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
            break;
            
        case YUV420PackedSemiPlanar:        
            inputPort.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
            break;
            
        case YCbYCr:
            //inputPort.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
            break;
                    
        default:
            inputPort.format.image.eColorFormat = OMX_COLOR_FormatYCbYCr;
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
        case IMAGE_FORMAT_JPEG:
            outputPort.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            break;
        case IMAGE_FORMAT_WEBP:
            outputPort.format.image.eCompressionFormat = OMX_IMAGE_CodingWEBP;
            break;
    }    
    outputPort.format.image.nFrameWidth = width;
    outputPort.format.image.nFrameHeight = height;
    printf("Output resolution %dx%d\n", outputPort.format.image.nFrameWidth, outputPort.format.image.nFrameHeight);
    
    //dst.format.image.bFlagErrorConcealment = OMX_TRUE;
    err = comp->SetParameter(comp, OMX_IndexParamPortDefinition, &outputPort);
    if (err != OMX_ErrorNone)
        goto FAIL;

    assert(inputPort.nBufferSize != 0);
    assert(outputPort.nBufferSize != 0);

    //OMX_U32 buffersize = 176*144*3 /2;
    OMX_U32 buffersize = width * height * 3 / 2;
    

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


void destroy_image_encoder(OMX_COMPONENTTYPE* comp)
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



void encode_clean_image(const char* input_file, const char* output_file, int encoding, int colorformat, int image_width, int image_height)
{
    init_list(&src_queue);
    init_list(&dst_queue);
    init_list(&input_queue);
    init_list(&output_queue);
    init_list(&input_ret_queue);
    init_list(&output_ret_queue);
    
    //int framelaskuri = 0;
    
    printf("image file: %s\nyuv file: %s\n", input_file, output_file);

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
    width = image_width;
    height = image_height;
    
    if(image_format == IMAGE_FORMAT_WEBP)
    {
        //printf("Write WebP file header\n");
        write_webp_file_header(out);
    }

    pEnc = create_image_encoder();
    if (!pEnc)
        goto FAIL;

    if (setup_component(pEnc, colorformat, encoding ) != OMX_ErrorNone)
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
        
        //OMX_U32 buffersize = 176*144*3/2;
        OMX_U32 buffersize = width * height * 3 / 2;
        
        //size_t ret = fread(inputBuffer->pBuffer, 1, inputBuffer->nAllocLen, inputFile);
        size_t ret = fread(inputBuffer->pBuffer, 1, buffersize, inputFile);
        printf("inputBuffer->nAllocLen = %d\n", (int)inputBuffer->nAllocLen);
        printf("read %d\n", ret);
        
        inputBuffer->nOffset    = 0;
        inputBuffer->nFilledLen = ret;
        //if ( feof(inputFile) ) // || ret == 0 )
        //{
            printf("feof = true\n");
            eof = OMX_TRUE;         
        //}
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

    if(image_format == IMAGE_FORMAT_WEBP)
    {
        write_webp_file_header(out);
    }

    printf("encoding done\n");
 FAIL:
    if (inputFile) fclose(inputFile);
    if (outputFile)   fclose(outputFile);
    if (pEnc)   destroy_image_encoder(pEnc);
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

void write_webp_file_header(FILE *fpOut)
{
    OMX_U8 data[RIFF_HEADER_SIZE] = {0};
    OMX_U32 totalSize = imageSize + WEBP_METADATA_SIZE; //

    /* RIFF tag */
    data[0] = 'R';
    data[1] = 'I';
    data[2] = 'F';
    data[3] = 'F';

    /* Size of image & meta data */
    data[4] = totalSize & 0xff;
    data[5] = (totalSize >> 8) & 0xff;
    data[6] = (totalSize >> 16) & 0xff;
    data[7] = (totalSize >> 24) & 0xff;

    /* Form type signature */
    data[8] = 'W';
    data[9] = 'E';
    data[10] = 'B';
    data[11] = 'P';

    /* Video format tag */
    data[12] = 'V';
    data[13] = 'P';
    data[14] = '8';
    data[15] = ' ';

    /* Size of image data */
    data[16] = imageSize & 0xff;
    data[17] = (imageSize >> 8) & 0xff;
    data[18] = (imageSize >> 16) & 0xff;
    data[19] = (imageSize >> 24) & 0xff;

    /* The WebP "File Header" is in the beginning of the file */
    rewind(fpOut);
    fwrite(data, 1, RIFF_HEADER_SIZE, fpOut);
}

int main(int argc, const char* argv[])
{
    printf("OMX component test image encoder\n");
    printf(__DATE__", "__TIME__"\n");
    
    OMX_ERRORTYPE err = OSAL_MutexCreate(&queue_mutex);
    if (err != OMX_ErrorNone)
    {
        printf("mutex create error: %s\n", HantroOmx_str_omx_err(err));
        return 1;
    }
    writelaskuri = 0;
    imageSize = 0;
    
    int i = 0;
    
    if ( argc == 2 )
    {
        //printf( argv );    
        i = atoi(argv[1]);
    }
    
    switch ( i )
    {
        case 1:   //jpeg cases:
            image_format = IMAGE_FORMAT_JPEG;
            encode_clean_image("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.jpg", IMAGE_FORMAT_JPEG, YUV420PackedPlanar, 352, 288);
            break;
        case 2:   //webp cases:
            image_format = IMAGE_FORMAT_WEBP;
            encode_clean_image("testdata_encoder/yuv/cif/bus_stop_25fps_w352h288.yuv", "buspackedplanar.webp", IMAGE_FORMAT_WEBP, YUV420PackedPlanar, 352, 288);
            break;
        case 3:   //webp cases:
            image_format = IMAGE_FORMAT_WEBP;
            encode_clean_image("testdata_encoder/yuv/vga/tanya_640x480.yuv", "tanya.webp", IMAGE_FORMAT_WEBP, YUV420PackedPlanar, 640, 480);
            break;
        default:
            break;
    }
    OSAL_MutexDestroy(queue_mutex);

    return 0;
}
