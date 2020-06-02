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

#include "OSAL.h"
#include "basecomp.h"
#include "port.h"
#include "util.h"
#include "encoder_version.h"
#include "encoder.h"
#include "encoder_perf.h"

#define DBGT_DECLARE_AUTOVAR
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX "

#if !defined (ENC6280) && !defined (ENC7280) && !defined (ENC8270) && !defined (ENC8290) && !defined (ENCH1) && !defined (ENCH2) && !defined(ENCH2V41) && !defined (ENCVC8000E)
#error "SPECIFY AN ENCODER PRODUCT (ENC6280, ENC7280, ENC8270, ENC8290, ENCH1 , ENCH2 ENCH2V41 or ENCVC8000E) IN COMPILER DEFINES!"
#endif

#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined(ENCH2V41)
#include "encoder_h264.h"
#ifdef ENABLE_JPEG
#include "encoder_jpeg.h"
#endif
#endif

#if defined (ENC6280) || defined (ENC7280)
#include "encoder_mpeg4.h"
#include "encoder_h263.h"
#endif

#ifdef ENCH1
#include "encoder_vp8.h"
#include "encoder_webp.h"
#endif

#ifdef ENCH2
#include "encoder_hevc.h"
#endif

#ifdef ENCVC8000E
#include "encoder_vc8000e.h"
#include "encoder_jpeg_vc8000e.h"
#endif

#ifdef ENCH2V41
#include "encoder_h2v41.h"
#endif


#define PORT_INDEX_INPUT     0
#define PORT_INDEX_OUTPUT    1
#define RETRY_INTERVAL       5
#define TIMEOUT              2000
#define MAX_RETRIES          10000

static OMX_ERRORTYPE async_encoder_set_state(OMX_COMMANDTYPE, OMX_U32, OMX_PTR, OMX_PTR);
static OMX_ERRORTYPE async_encoder_disable_port(OMX_COMMANDTYPE, OMX_U32, OMX_PTR, OMX_PTR);
static OMX_ERRORTYPE async_encoder_enable_port(OMX_COMMANDTYPE, OMX_U32, OMX_PTR, OMX_PTR);
static OMX_ERRORTYPE async_encoder_flush_port(OMX_COMMANDTYPE, OMX_U32, OMX_PTR, OMX_PTR);
static OMX_ERRORTYPE async_encoder_mark_buffer(OMX_COMMANDTYPE, OMX_U32, OMX_PTR, OMX_PTR);

// video domain specific functions
#ifdef OMX_ENCODER_VIDEO_DOMAIN
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
static OMX_ERRORTYPE set_avc_defaults(OMX_ENCODER* pEnc);
#endif
#if defined (ENC6280) || defined (ENC7280)
static OMX_ERRORTYPE set_mpeg4_defaults(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE set_h263_defaults(OMX_ENCODER* pEnc);
#endif
#ifdef ENCH1
static OMX_ERRORTYPE set_vp8_defaults(OMX_ENCODER* pEnc);
#endif
#ifdef ENCH2
static OMX_ERRORTYPE set_hevc_defaults(OMX_ENCODER* pEnc);
#endif
#ifdef ENCVC8000E
static OMX_ERRORTYPE set_vc8000e_hevc_defaults(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE set_vc8000e_avc_defaults(OMX_ENCODER* pEnc);
#endif
#ifdef ENCH2V41
static OMX_ERRORTYPE set_h2v41_hevc_defaults(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE set_h2v41_avc_defaults(OMX_ENCODER* pEnc);
#endif


static OMX_ERRORTYPE set_bitrate_defaults(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE async_video_encoder_encode(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE async_encode_stabilized_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* buff, OMX_U32* retlen, OMX_U32 frameSize);
static OMX_ERRORTYPE async_encode_video_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* buff, OMX_U32* retlen, OMX_U32 frameSize);

#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
static OMX_ERRORTYPE set_avc_intra_period(OMX_ENCODER* pEnc);
#endif
static OMX_ERRORTYPE set_frame_rate(OMX_ENCODER* pEnc);
#endif

// image domain specific functions
#ifdef OMX_ENCODER_IMAGE_DOMAIN
#ifdef ENABLE_JPEG
static OMX_ERRORTYPE set_jpeg_defaults(OMX_ENCODER* pEnc);
#endif
#ifdef ENCH1
static OMX_ERRORTYPE set_webp_defaults(OMX_ENCODER* pEnc);
#endif
static OMX_ERRORTYPE async_image_encoder_encode(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE async_encode_image_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* buff, OMX_U32* retlen, OMX_U32 frameSize);
#endif

static OMX_ERRORTYPE set_preprocessor_defaults(OMX_ENCODER* pEnc);
static OMX_ERRORTYPE calculate_frame_size(OMX_ENCODER* pEnc, OMX_U32* frameSize);
static OMX_ERRORTYPE calculate_new_outputBufferSize(OMX_ENCODER* pEnc);

#define FRAME_BUFF_CAPACITY(fb) (fb)->capacity - (fb)->size
#define FRAME_BUFF_FREE(alloc, fb) \
 OSAL_AllocatorFreeMem((alloc), (fb)->capacity, (fb)->bus_data, (fb)->bus_address)
#define GET_ENCODER(comp) (OMX_ENCODER*)(((OMX_COMPONENTTYPE*)(comp))->pComponentPrivate)

/**
 * static OMX_U32 encoder_thread_main(BASECOMP* base, OMX_PTR arg)
 * Thread function.
 * 1. Initializes signals for command and buffer queue
 * 2. Starts to wait signals for command or buffer queue
 * 3. Processes commands
 * 4. Encodes received frames
 * 5. On exit sets state invalid
 * @param BASECOMP* base - Base component instance
 * @param OMX_PTR arg - Instance of OMX encoder component
 */
static
OMX_U32 encoder_thread_main(BASECOMP* base, OMX_PTR arg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    UNUSED_PARAMETER(base);
    OMX_ENCODER* this = (OMX_ENCODER*)arg;

    OMX_HANDLETYPE handles[2];
    handles[0] = this->base.queue.event;  // event handle for the command queue
    handles[1] = this->inputPort.bufferevent;    // event handle for the normal input port input buffer queue

    OMX_ERRORTYPE err = OMX_ErrorNone;
    OSAL_BOOL timeout = OSAL_FALSE;
    OSAL_BOOL signals[2];

    while (this->run)
    {
        // clear all signal indicators
        signals[0] = OSAL_FALSE;
        signals[1] = OSAL_FALSE;

        // wait for command messages and buffers
        DBGT_PDEBUG("Thread wait");
        err = OSAL_EventWaitMultiple(handles, signals, 2, INFINITE_WAIT, &timeout);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("ASYNC: waiting for events failed: %s", HantroOmx_str_omx_err(err));
            break;
        }
        DBGT_PDEBUG("Thread event received");

        if (signals[0] == OSAL_TRUE)
        {
            CMD cmd;
            while (1)
            {
                OMX_BOOL ok = OMX_TRUE;
                err = HantroOmx_basecomp_try_recv_command(&this->base, &cmd, &ok);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("ASYNC: basecomp_try_recv_command failed: %s", HantroOmx_str_omx_err(err));
                    this->run = OMX_FALSE;
                    break;
                }
                if (ok == OMX_FALSE)
                    break;
                if (cmd.type == CMD_EXIT_LOOP)
                {
                    DBGT_PDEBUG("ASYNC: got CMD_EXIT_LOOP");
                    this->run = OMX_FALSE;
                    break;
                }
                HantroOmx_cmd_dispatch(&cmd, this);
            }
        }
        if (signals[1] == OSAL_TRUE && this->state == OMX_StateExecuting)
        {
#ifdef OMX_ENCODER_VIDEO_DOMAIN
            async_video_encoder_encode(this);
#endif
#ifdef OMX_ENCODER_IMAGE_DOMAIN
            async_image_encoder_encode(this);
#endif
        }
    }
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("ASYNC: error: %s", HantroOmx_str_omx_err(err));
        DBGT_PDEBUG("ASYNC: new state: %s", HantroOmx_str_omx_state(OMX_StateInvalid));
        this->state = OMX_StateInvalid;
        this->app_callbacks.EventHandler(this->self, this->app_data, OMX_EventError,
                                         OMX_ErrorInvalidState, 0, NULL);
    }
    DBGT_EPILOG("");
    return 0;
}

static
OMX_ERRORTYPE encoder_get_version(OMX_IN  OMX_HANDLETYPE   hComponent,
                                  OMX_OUT OMX_STRING       pComponentName,
                                  OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
                                  OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
                                  OMX_OUT OMX_UUIDTYPE*    pComponentUUID)
{
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pComponentName);
    CHECK_PARAM_NON_NULL(pComponentVersion);
    CHECK_PARAM_NON_NULL(pSpecVersion);
    CHECK_PARAM_NON_NULL(pComponentUUID);

    //OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    DBGT_PROLOG("");

#ifdef OMX_ENCODER_VIDEO_DOMAIN
    strncpy(pComponentName, VIDEO_COMPONENT_NAME, OMX_MAX_STRINGNAME_SIZE-1);
#endif
#ifdef OMX_ENCODER_IMAGE_DOMAIN
    strncpy(pComponentName, IMAGE_COMPONENT_NAME, OMX_MAX_STRINGNAME_SIZE-1);
#endif

    pComponentVersion->s.nVersionMajor = COMPONENT_VERSION_MAJOR;
    pComponentVersion->s.nVersionMinor = COMPONENT_VERSION_MINOR;
    pComponentVersion->s.nRevision     = COMPONENT_VERSION_REVISION;
    pComponentVersion->s.nStep         = COMPONENT_VERSION_STEP;

    pSpecVersion->s.nVersionMajor      = 1; // this is the OpenMAX IL version. Has nothing to do with component version.
    pSpecVersion->s.nVersionMinor      = 1;
    pSpecVersion->s.nRevision          = 2;
    pSpecVersion->s.nStep              = 0;

    HantroOmx_generate_uuid(hComponent, pComponentUUID);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * void encoder_dealloc_buffers(OMX_ENCODER* pEnc, PORT* p)
 * Deallocates all buffers that are owned by this component
 */
static
void encoder_dealloc_buffers(OMX_ENCODER* pEnc, PORT* p)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(p);
    OMX_U32 count = HantroOmx_port_buffer_count(p);
    OMX_U32 i;
    for (i=0; i<count; ++i)
    {
        BUFFER* buff = NULL;
        HantroOmx_port_get_allocated_buffer_at(p, &buff, i);
        DBGT_ASSERT(buff);
        if (buff->flags & BUFFER_FLAG_MY_BUFFER)
        {
            DBGT_ASSERT(buff->header == &buff->headerdata);
            DBGT_ASSERT(buff->bus_address);
            DBGT_ASSERT(buff->bus_data);
            OSAL_AllocatorFreeMem(&pEnc->alloc, buff->allocsize, buff->bus_data, buff->bus_address);
        }
    }
    DBGT_EPILOG("");
}


/**
 * OMX_ERRORTYPE encoder_deinit(OMX_HANDLETYPE hComponent)
 * Deinitializes encoder component
 */
static
OMX_ERRORTYPE encoder_deinit(OMX_HANDLETYPE hComponent)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);

    if (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate == NULL)
    {
        // play nice and handle destroying a component that was never created nicely...
        DBGT_PDEBUG("API: pComponentPrivate == NULL");
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    DBGT_PDEBUG("API: waiting for thread to finish");
    DBGT_PDEBUG("API: current state: %s",
                HantroOmx_str_omx_state(pEnc->state));

    if (pEnc->base.thread)
    {
        // bring down the component thread, and join it
        pEnc->run = OMX_FALSE;
        CMD c;
        INIT_EXIT_CMD(c);
        HantroOmx_basecomp_send_command(&pEnc->base, &c);
        OSAL_ThreadSleep(RETRY_INTERVAL);
        HantroOmx_basecomp_destroy(&pEnc->base);
    }

    DBGT_ASSERT(HantroOmx_port_is_allocated(&pEnc->inputPort) == OMX_TRUE);
    DBGT_ASSERT(HantroOmx_port_is_allocated(&pEnc->outputPort) == OMX_TRUE);

    if (pEnc->state != OMX_StateLoaded)
    {
        // if there's stuff in the input/output port buffer queues
        // simply ignore it

        // deallocate allocated buffers (if any)
        // this could have catastrophic consequences if someone somewhere is
        // is still holding a pointer to these buffers... (what could be done here, except leak?)

        encoder_dealloc_buffers(pEnc, &pEnc->inputPort);
        encoder_dealloc_buffers(pEnc, &pEnc->outputPort);
        DBGT_PDEBUG("API: delloc buffers done");

        if (pEnc->codec)
            pEnc->codec->destroy(pEnc->codec);
        if (pEnc->frame_in.bus_address)
            FRAME_BUFF_FREE(&pEnc->alloc, &pEnc->frame_in);
        if (pEnc->frame_out.bus_address)
            FRAME_BUFF_FREE(&pEnc->alloc, &pEnc->frame_out);

        DBGT_PDEBUG("API: dealloc frame buffers done");
    }
    else
    {
        // ports should not have any queued buffers at this point anymore.
        DBGT_ASSERT(HantroOmx_port_buffer_queue_count(&pEnc->inputPort) == 0);
        DBGT_ASSERT(HantroOmx_port_buffer_queue_count(&pEnc->outputPort) == 0);

        // ports should not have any buffers allocated at this point anymore
        DBGT_ASSERT(HantroOmx_port_has_buffers(&pEnc->inputPort) == OMX_FALSE);
        DBGT_ASSERT(HantroOmx_port_has_buffers(&pEnc->outputPort) == OMX_FALSE);

        // temporary frame buffers should not exist anymore
        DBGT_ASSERT(pEnc->frame_in.bus_data == NULL);
        DBGT_ASSERT(pEnc->frame_out.bus_data == NULL);
    }
    HantroOmx_port_destroy(&pEnc->inputPort);
    HantroOmx_port_destroy(&pEnc->outputPort);

    perf_show(pEnc);

    if (pEnc->statemutex)
        OSAL_MutexDestroy(pEnc->statemutex);

    OSAL_AllocatorDestroy(&pEnc->alloc);
    OSAL_Free(pEnc);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE encoder_send_command(OMX_IN OMX_HANDLETYPE  hComponent,
                                   OMX_IN OMX_COMMANDTYPE Cmd,
                                   OMX_IN OMX_U32         nParam1,
                                   OMX_IN OMX_PTR         pCmdData)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    CHECK_STATE_INVALID(pEnc->state);

    DBGT_PDEBUG("API: Cmd:%s nParam1:%u pCmdData:%p",
                HantroOmx_str_omx_cmd(Cmd), (unsigned)nParam1, pCmdData);

    CMD c;
    OMX_ERRORTYPE err = OMX_ErrorNotImplemented;

    switch (Cmd)
    {
        case OMX_CommandStateSet:
            if (pEnc->statetrans != pEnc->state)
            {
                // transition is already pending
                DBGT_CRITICAL("Transition is already pending (dec->statetrans != dec->state)");
                DBGT_EPILOG("");
                return OMX_ErrorIncorrectStateTransition;
            }
            DBGT_PDEBUG("API: next state:%s", HantroOmx_str_omx_state((OMX_STATETYPE)nParam1));
            INIT_SEND_CMD(c, Cmd, nParam1, NULL, async_encoder_set_state);
            pEnc->statetrans = (OMX_STATETYPE)nParam1;
            err = HantroOmx_basecomp_send_command(&pEnc->base, &c);
            break;

        case OMX_CommandFlush:
            if (nParam1 > PORT_INDEX_OUTPUT && nParam1 != OMX_ALL)
            {
                DBGT_CRITICAL("API: bad port index:%u", (unsigned)nParam1);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
            INIT_SEND_CMD(c, Cmd, nParam1, NULL, async_encoder_flush_port);
            err = HantroOmx_basecomp_send_command(&pEnc->base, &c);
            break;

        case OMX_CommandPortDisable:
            if (nParam1 > PORT_INDEX_OUTPUT && nParam1 != OMX_ALL)
            {
                DBGT_CRITICAL("API: bad port index:%u", (unsigned)nParam1);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
            if (nParam1 == PORT_INDEX_INPUT || nParam1 == OMX_ALL)
                pEnc->inputPort.def.bEnabled = OMX_FALSE;
            if (nParam1 == PORT_INDEX_OUTPUT || nParam1 == OMX_ALL)
                pEnc->outputPort.def.bEnabled = OMX_FALSE;


            INIT_SEND_CMD(c, Cmd, nParam1, NULL, async_encoder_disable_port);
            err = HantroOmx_basecomp_send_command(&pEnc->base, &c);
            break;

        case OMX_CommandPortEnable:
            if (nParam1 > PORT_INDEX_OUTPUT && nParam1 != OMX_ALL)
            {
                DBGT_CRITICAL("API: bad port index:%u", (unsigned)nParam1);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
            if (nParam1 == PORT_INDEX_INPUT || nParam1 == OMX_ALL)
                pEnc->inputPort.def.bEnabled = OMX_TRUE;
            if (nParam1 == PORT_INDEX_OUTPUT || nParam1 == OMX_ALL)
                pEnc->outputPort.def.bEnabled = OMX_TRUE;


            INIT_SEND_CMD(c, Cmd, nParam1, NULL, async_encoder_enable_port);
            err = HantroOmx_basecomp_send_command(&pEnc->base, &c);
            break;

        case OMX_CommandMarkBuffer:
            if ((nParam1 != PORT_INDEX_INPUT) && (nParam1 != PORT_INDEX_OUTPUT))
            {
                DBGT_CRITICAL("API: bad port index:%u", (unsigned)nParam1);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
            CHECK_PARAM_NON_NULL(pCmdData);
            OMX_MARKTYPE* mark = (OMX_MARKTYPE*)OSAL_Malloc(sizeof(OMX_MARKTYPE));
            if (!mark)
            {
                DBGT_CRITICAL("API: cannot marshall mark (OMX_ErrorInsufficientResources)");
                DBGT_EPILOG("");
                return OMX_ErrorInsufficientResources;
            }
            memcpy(mark, pCmdData, sizeof(OMX_MARKTYPE));
            INIT_SEND_CMD(c, Cmd, nParam1, mark, async_encoder_mark_buffer);
            err = HantroOmx_basecomp_send_command(&pEnc->base, &c);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("HantroOmx_basecomp_send_command failed");
                OSAL_Free(mark);
            }
            break;

        default:
            DBGT_CRITICAL("API: bad command:%u", (unsigned)Cmd);
            err = OMX_ErrorBadParameter;
            break;
    }
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("API: error: %s", HantroOmx_str_omx_err(err));
    }
    DBGT_EPILOG("");
    return err;
}

static
OMX_ERRORTYPE encoder_set_callbacks(OMX_IN OMX_HANDLETYPE    hComponent,
                                    OMX_IN OMX_CALLBACKTYPE* pCallbacks,
                                    OMX_IN OMX_PTR           pAppData)

{
    DBGT_PROLOG("");

    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pCallbacks);
    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    DBGT_PDEBUG("API: pCallbacks:%p pAppData:%p", pCallbacks, pAppData);

    pEnc->app_callbacks = *pCallbacks;
    pEnc->app_data   = pAppData;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE encoder_get_state(OMX_IN  OMX_HANDLETYPE hComponent,
                                OMX_OUT OMX_STATETYPE* pState)
{
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pState);
    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    OSAL_MutexLock(pEnc->statemutex);
    *pState = pEnc->state;
    OSAL_MutexUnlock(pEnc->statemutex);
    return OMX_ErrorNone;
}


/**
 * static PORT* encoder_map_index_to_port(OMX_ENCODER* pEnc, OMX_U32 portIndex)
 * Static function.
 * @param OMX_ENCODER - OMX encoder instance
 * @param OMX_U32 - Index indicating required port
 * @return PORT - input or output port depending on given index
 */
static
PORT* encoder_map_index_to_port(OMX_ENCODER* pEnc, OMX_U32 portIndex)
{
    switch (portIndex)
    {
        DBGT_PDEBUG("ASYNC: portIndex - %d", (int)portIndex);
        case PORT_INDEX_INPUT:  return &pEnc->inputPort;
        case PORT_INDEX_OUTPUT: return &pEnc->outputPort;
    }
    return NULL;
}

/**
 * static OMX_ERRORTYPE encoder_verify_buffer_allocation(OMX_ENCODER* pEnc, PORT* p, OMX_U32 buffSize)
 * @param OMX_ENCODER pEnc - OMX Encoder instance
 * @param PORT* p - Instance of port holding buffer
 * @param OMX_U32 buffSize -
 */
static
OMX_ERRORTYPE encoder_verify_buffer_allocation(OMX_ENCODER* pEnc, PORT* p, OMX_U32 buffSize)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(pEnc);
    DBGT_ASSERT(p);

    OMX_ERRORTYPE err = OMX_ErrorIncorrectStateOperation;

    // buffers can only be allocated when the component is in one of the following states:
    // 1. OMX_StateLoaded and has already sent a request for state transition to OMX_StateIdle
    // 2. OMX_StateWaitForResources, the resources are available and the component
    //    is ready to go to the OMX_StateIdle state
    // 3. OMX_StateExecuting, OMX_StatePause or OMX_StateIdle and the port is disabled.
    if (p->def.bPopulated)
    {
        DBGT_WARNING("API: port is already populated");
        DBGT_EPILOG("");
        return err;
    }
    if (buffSize < p->def.nBufferSize)
    {
        DBGT_ERROR("API: buffer is too small required:%u given:%u",
                    (unsigned) p->def.nBufferSize, (unsigned) buffSize);
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }
    // 3.2.2.15
    switch (pEnc->state)
    {
        case OMX_StateLoaded:
            if (pEnc->statetrans != OMX_StateIdle)
            {
                DBGT_ERROR("API: not in transition to idle");
                DBGT_EPILOG("");
                return err;
            }
            break;
        case OMX_StateWaitForResources:
            DBGT_WARNING("OMX_StateWaitForResources not implemented");
            DBGT_EPILOG("");
            return OMX_ErrorNotImplemented;

            //
            // These cases are in disagreement with the OMX_AllocateBuffer definition
            // in the specification in chapter 3.2.2.15. (And that chapter in turn seems to be
            // conflicting with chapter 3.2.2.6.)
            //
            // The bottom line is that the conformance tester sets the component to these states
            // and then wants to disable and enable ports and then allocate buffers. For example
            // Conformance tester PortCommunicationTest sets the component into executing state,
            // then disables all ports (-1), frees all buffers on those ports and finally tries
            // to enable all ports (-1), and then allocate buffers.
            // The specification says that if component is in executing state (or pause or idle)
            // the port must be disabled when allocating a buffer, but in order to pass the test
            // we must violate that requirement.
            //
            // A common guideline seems to be that when the tester and specification are in disagreement
            // the tester wins.
            //
        case OMX_StateIdle:
        case OMX_StateExecuting:
            break;
        default:
            if (p->def.bEnabled)
            {
                DBGT_CRITICAL("API: port is not disabled");
                DBGT_EPILOG("");
                return err;
            }
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRORTYPE encoder_use_buffer(OMX_IN    OMX_HANDLETYPE hComponent,
                                    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
                                    OMX_IN    OMX_U32 nPortIndex,
                                    OMX_IN    OMX_PTR pAppPrivate,
                                    OMX_IN    OMX_U32 nSizeBytes,
                                    OMX_IN    OMX_U8* pBuffer)
 * Client gives allocated buffer for OMX Component
 * Component must allocate its own temporary buffer to support correct memory mapping.
 * Call can be made when:
 * Component State is Loaded and it has requested to transition for Idle state  ||
 * Component state is WaitingForResources and it is ready to go Idle state ||
 * Port is disabled and Component State is Executing or Loaded or Idle
 *
 */
static
OMX_ERRORTYPE encoder_use_buffer(OMX_IN    OMX_HANDLETYPE hComponent,
                                 OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
                                 OMX_IN    OMX_U32 nPortIndex,
                                 OMX_IN    OMX_PTR pAppPrivate,
                                 OMX_IN    OMX_U32 nSizeBytes,
                                 OMX_IN    OMX_U8* pBuffer)
{
    DBGT_PROLOG("");

    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(ppBuffer);
    CHECK_PARAM_NON_NULL(pBuffer);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    CHECK_STATE_INVALID(pEnc->state);

    DBGT_PDEBUG("API: nPortIndex:%u pAppPrivate:%p nSizeBytes:%u pBuffer:%p",
                (unsigned)nPortIndex, pAppPrivate, (unsigned)nSizeBytes, pBuffer);

    PORT* port = encoder_map_index_to_port(pEnc, nPortIndex);
    if (port == NULL)
    {
        DBGT_CRITICAL("API: bad port index:%u", (unsigned) nPortIndex);
        DBGT_EPILOG("");
        return OMX_ErrorBadPortIndex;
    }
    OMX_ERRORTYPE err = encoder_verify_buffer_allocation(pEnc, port, nSizeBytes);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("encoder_verify_buffer_allocation (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }

    DBGT_PDEBUG("API: port index:%u", (unsigned) nPortIndex);
    DBGT_PDEBUG("API: buffer size:%u", (unsigned) nSizeBytes);

    BUFFER* buff = NULL;
    HantroOmx_port_allocate_next_buffer(port, &buff);
    if (buff == NULL)
    {
        DBGT_CRITICAL("API: HantroOmx_port_allocate_next_buffer: no more buffers");
        DBGT_EPILOG("");
        return OMX_ErrorInsufficientResources;
    }

    // save the pointer here into the header. The data in this buffer
    // needs to be copied in to the DMA accessible frame buffer before encoding.

    INIT_OMX_VERSION_PARAM(*buff->header);
    buff->flags               &= ~BUFFER_FLAG_MY_BUFFER;
    buff->header->pBuffer     = pBuffer;
    buff->header->pAppPrivate = pAppPrivate;
    buff->header->nAllocLen   = nSizeBytes;
    buff->bus_address         = 0;
    buff->allocsize           = nSizeBytes;

    if (HantroOmx_port_buffer_count(port) == port->def.nBufferCountActual)
    {
        DBGT_PDEBUG("API: port is populated");
        port->def.bPopulated = OMX_TRUE;
    }
    if (nPortIndex == PORT_INDEX_INPUT)
    {
        buff->header->nInputPortIndex = nPortIndex;
        buff->header->nOutputPortIndex = 0;
    }
    else
    {
        buff->header->nInputPortIndex = 0;
        buff->header->nOutputPortIndex = nPortIndex;
    }

    *ppBuffer = buff->header;
    DBGT_PDEBUG("API: pBufferHeader:%p", *ppBuffer);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRORTYPE encoder_allocate_buffer(OMX_IN    OMX_HANDLETYPE hComponent,
                                      OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
                                      OMX_IN    OMX_U32 nPortIndex,
                                      OMX_IN    OMX_PTR pAppPrivate,
                                      OMX_IN    OMX_U32 nSizeBytes)
 * @param OMX_BUFFERHEADERTYPE** ppBuffer - Buffer header information for buffer that will be allocated
 * @param OMX_U32 nPortIndex - Indes of port, 1 input port, 2 output port
 * @param OMX_U32 nSizeBytes - Size of buffer that will be allocated
 * OMX Component allocates new buffer for encoding.
 */
static
OMX_ERRORTYPE encoder_allocate_buffer(OMX_IN    OMX_HANDLETYPE hComponent,
                                      OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
                                      OMX_IN    OMX_U32 nPortIndex,
                                      OMX_IN    OMX_PTR pAppPrivate,
                                      OMX_IN    OMX_U32 nSizeBytes)
{
    DBGT_PROLOG("");

    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(ppBuffer);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    CHECK_STATE_INVALID(pEnc->state);

    DBGT_PDEBUG("API: nPortIndex:%u pAppPrivate:%p nSizeBytes:%u",
        (unsigned)nPortIndex, pAppPrivate, (unsigned)nSizeBytes);

    PORT* port = encoder_map_index_to_port(pEnc, nPortIndex);
    if (port == NULL)
    {
        DBGT_CRITICAL("API: bad port index:%u", (unsigned) nPortIndex);
        DBGT_EPILOG("");
        return OMX_ErrorBadPortIndex;
    }

    OMX_ERRORTYPE err = encoder_verify_buffer_allocation(pEnc, port, nSizeBytes);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("encoder_verify_buffer_allocation (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }

    DBGT_PDEBUG("API: port index:%u", (unsigned) nPortIndex);
    DBGT_PDEBUG("API: buffer size:%u", (unsigned) nSizeBytes);

    // about locking here.
    // Only in case of tunneling is the component thread accessing the port's
    // buffer's directly. However this function should not be called at that time.
    // note: perhaps we could lock/somehow make sure that a misbehaving client thread
    // wont get a change to mess everything up?

    OMX_U8*         bus_data    = NULL;
    OSAL_BUS_WIDTH  bus_address = 0;
    OMX_U32         allocsize   = nSizeBytes;

    // the conformance tester assumes that the allocated buffers equal nSizeBytes in size.
    // however when running on HW this might not be a valid assumption because the memory allocator
    // allocates memory always in fixed size chunks.
    err = OSAL_AllocatorAllocMem(&pEnc->alloc, &allocsize, &bus_data, &bus_address);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("API: memory allocation failed: %s", HantroOmx_str_omx_err(err));
        DBGT_EPILOG("");
        return err;
    }

    DBGT_ASSERT(allocsize >= nSizeBytes);
    DBGT_PDEBUG("API: allocated buffer size:%u @physical addr: 0x%08lx @logical addr: %p",
                (unsigned) allocsize, bus_address, bus_data);

    BUFFER* buff = NULL;
    HantroOmx_port_allocate_next_buffer(port, &buff);
    if (!buff)
    {
        DBGT_CRITICAL("API: no more buffers");
        OSAL_AllocatorFreeMem(&pEnc->alloc, nSizeBytes, bus_data, bus_address);
        DBGT_EPILOG("");
        return OMX_ErrorInsufficientResources;
    }

    INIT_OMX_VERSION_PARAM(*buff->header);
    buff->flags              |= BUFFER_FLAG_MY_BUFFER;
    buff->bus_data            = bus_data;
    buff->bus_address         = bus_address;
    buff->allocsize           = allocsize;
    buff->header->pBuffer     = bus_data;
    buff->header->pAppPrivate = pAppPrivate;
    buff->header->nAllocLen   = nSizeBytes; // the conformance tester assumes that the allocated buffers equal nSizeBytes in size.
    if (nPortIndex == PORT_INDEX_INPUT)
    {
        buff->header->nInputPortIndex  = nPortIndex;
        buff->header->nOutputPortIndex = 0;
    }
    else
    {
        buff->header->nInputPortIndex  = 0;
        buff->header->nOutputPortIndex = nPortIndex;
    }
    if (HantroOmx_port_buffer_count(port) == port->def.nBufferCountActual)
    {
        HantroOmx_port_lock_buffers(port);
        port->def.bPopulated = OMX_TRUE;
        HantroOmx_port_unlock_buffers(port);
        DBGT_PDEBUG("API: port is populated");
    }
    *ppBuffer = buff->header;
    DBGT_PDEBUG("API: data (virtual address):%p pBufferHeader:%p", bus_data, *ppBuffer);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRORTYPE encoder_free_buffer(OMX_IN OMX_HANDLETYPE hComponent,
                                  OMX_IN OMX_U32 nPortIndex,
                                  OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
 * @param OMX_U32 nPortIndex - Indes of port, 1 input port, 2 output port
 * @param OMX_BUFFERHEADERTYPE pBufferHeader -  Buffer to be freed.
 */
static
OMX_ERRORTYPE encoder_free_buffer(OMX_IN OMX_HANDLETYPE hComponent,
                                  OMX_IN OMX_U32 nPortIndex,
                                  OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pBufferHeader);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    DBGT_PDEBUG("API: nPortIndex:%u pBufferHeader:%p", (unsigned)nPortIndex, pBufferHeader);

    // 3.2.2.16
    // The call should be performed under the following conditions:
    // o  While the component is in the OMX_StateIdle state and the IL client has already
    //    sent a request for the state transition to OMX_StateLoaded (e.g., during the
    //    stopping of the component)
    // o  On a disabled port when the component is in the OMX_StateExecuting, the
    //    OMX_StatePause, or the OMX_StateIdle state.

    // the above is "should" only, in other words meaning that free buffer CAN be performed
    // at anytime. However the conformance tester expects that the component generates
    // an OMX_ErrorPortUnpopulated event, even though in the specification this is described as "may generate OMX_ErrorPortUnpopulated".

    PORT* port = encoder_map_index_to_port(pEnc, nPortIndex);
    if (port == NULL)
    {
        DBGT_CRITICAL("API: bad port index:%u", (unsigned) nPortIndex);
        DBGT_EPILOG("");
        return OMX_ErrorBadPortIndex;
    }
    OMX_BOOL violation = OMX_FALSE;

    if (port->def.bEnabled)
    {
        if (pEnc->state == OMX_StateIdle && pEnc->statetrans != OMX_StateLoaded)
        {
            violation = OMX_TRUE;
        }
    }

    BUFFER* buff = HantroOmx_port_find_buffer(port, pBufferHeader);
    if (!buff)
    {
        DBGT_CRITICAL("API: HantroOmx_port_find_buffer: no such buffer");
        //HantroOmx_port_unlock_buffers(port);
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }

    if (!(buff->flags & BUFFER_FLAG_IN_USE))
    {
        DBGT_CRITICAL("API: HantroOmx_port_find_buffer: buffer is not allocated");
        //HantroOmx_port_unlock_buffers(port);
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }

    if (buff->flags & BUFFER_FLAG_MY_BUFFER)
    {
        DBGT_ASSERT(buff->bus_address && buff->bus_data);
        DBGT_ASSERT(buff->bus_data == buff->header->pBuffer);
        DBGT_ASSERT(buff->allocsize);
        OSAL_AllocatorFreeMem(&pEnc->alloc, buff->allocsize, buff->bus_data, buff->bus_address);
    }

    HantroOmx_port_release_buffer(port, buff);

    if (HantroOmx_port_buffer_count(port) < port->def.nBufferCountActual)
        port->def.bPopulated = OMX_FALSE;

    // this is a hack because of the conformance tester.
    if (port->def.bPopulated == OMX_FALSE && violation == OMX_TRUE)
    {
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorPortUnpopulated, 0, NULL);
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#define CHECK_PORT_STATE(encoder, port)                          \
  if ((encoder)->state != OMX_StateLoaded && (port)->bEnabled)   \
  {                                                              \
      DBGT_ERROR("API: CHECK_PORT_STATE, port is not disabled"); \
      return OMX_ErrorIncorrectStateOperation;                   \
  }

#define CHECK_PORT_OUTPUT(param)                              \
  if ((param)->nPortIndex != PORT_INDEX_OUTPUT)               \
  {                                                           \
      DBGT_ERROR("CHECK_PORT_OUTPUT, OMX_ErrorBadPortIndex"); \
      return OMX_ErrorBadPortIndex;                           \
  }

#define CHECK_PORT_INPUT(param)                              \
  if ((param)->nPortIndex != PORT_INDEX_INPUT)               \
  {                                                          \
      DBGT_ERROR("CHECK_PORT_INPUT, OMX_ErrorBadPortIndex"); \
      return OMX_ErrorBadPortIndex;                          \
  }

#ifdef OMX_ENCODER_VIDEO_DOMAIN
static
OMX_ERRORTYPE encoder_set_parameter(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE  nIndex,
                                    OMX_IN OMX_PTR        pParam)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorUndefined;

    CHECK_STATE_INVALID(pEnc->state);
    CHECK_STATE_IDLE(pEnc->state);

    if (pEnc->state != OMX_StateLoaded && pEnc->state != OMX_StateWaitForResources)
    {
        DBGT_CRITICAL("API: unsupported state: %s", HantroOmx_str_omx_state(pEnc->state));
        return OMX_ErrorIncorrectStateOperation;
    }

    switch ((OMX_U32)nIndex)
    {
        case OMX_IndexParamAudioInit: //Fall through
        case OMX_IndexParamOtherInit:
        case OMX_IndexParamImageInit:
            DBGT_CRITICAL("API: Unsupported index");
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
        case OMX_IndexParamVideoInit:
            DBGT_EPILOG("");
            return OMX_ErrorNone;
            break;

        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* port  = NULL;
                OMX_PARAM_PORTDEFINITIONTYPE* param = (OMX_PARAM_PORTDEFINITIONTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        port = &pEnc->inputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        if (param->format.video.eCompressionFormat != OMX_VIDEO_CodingUnused)
                        {
                            DBGT_CRITICAL("API: Only uncompressed video accepted for input");
                            DBGT_EPILOG("");
                            return OMX_ErrorUnsupportedSetting;
                        }

                        if (param->nBufferCountActual >= port->nBufferCountMin)
                            port->nBufferCountActual        = param->nBufferCountActual;

                        port->format.video.xFramerate       = param->format.video.xFramerate;
                        port->format.video.nFrameWidth      = param->format.video.nFrameWidth;
                        port->format.video.nFrameHeight     = param->format.video.nFrameHeight;
                        port->format.video.nStride          = param->format.video.nStride;
                        port->format.video.eColorFormat     = param->format.video.eColorFormat;
                        //port->nBufferSize                   = param->nBufferSize; // According to OMX IL spec, port buffer size is read only
#ifndef USE_DEFAULT_INPUT_BUFFER_SIZE
                        err =  calculate_frame_size(pEnc, &port->nBufferSize);
#endif
                        if (err != OMX_ErrorNone)
                        {
                            DBGT_CRITICAL("Error calculating frame size (err=%x)", err);
                            return err;
                        }
                        DBGT_PDEBUG("OMX_IndexParamPortDefinition: port->nBufferSize = %d, param->nBufferSize = %d",
                                    (int)port->nBufferSize, (int)param->nBufferSize);
                        break;

                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);

                        if (param->nBufferCountActual >= port->nBufferCountMin)
                            port->nBufferCountActual              = param->nBufferCountActual;

                        port->format.video.nBitrate           = param->format.video.nBitrate;
                        port->format.video.xFramerate         = param->format.video.xFramerate;
                        port->format.video.nFrameWidth        = param->format.video.nFrameWidth;
                        port->format.video.nFrameHeight       = param->format.video.nFrameHeight;
                        port->format.video.eCompressionFormat = param->format.video.eCompressionFormat;
                        switch ((OMX_U32)port->format.video.eCompressionFormat)
                        {
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)

                            case OMX_VIDEO_CodingAVC:
                                set_avc_defaults(pEnc);
                                break;
#endif
#if defined (ENC6280) || defined (ENC7280)
                            case OMX_VIDEO_CodingH263:
                                set_h263_defaults(pEnc);
                                break;
                            case OMX_VIDEO_CodingMPEG4:
                                set_mpeg4_defaults(pEnc);
                                break;
#endif
#ifdef ENCH1
                            case OMX_VIDEO_CodingVP8:
                                set_vp8_defaults(pEnc);
                                break;
#endif
#ifdef ENCH2
                            case OMX_VIDEO_CodingHEVC:
                                set_hevc_defaults(pEnc);
                                break;
#endif
#ifdef ENCVC8000E
                            case OMX_VIDEO_CodingHEVC:
                                set_vc8000e_hevc_defaults(pEnc);
                                break;
                            case OMX_VIDEO_CodingAVC:
                                set_vc8000e_avc_defaults(pEnc);
                                break;
#endif
#ifdef ENCH2V41
                            case OMX_VIDEO_CodingHEVC:
                                set_h2v41_hevc_defaults(pEnc);
                                break;
                            case OMX_VIDEO_CodingAVC:
                                set_h2v41_avc_defaults(pEnc);
                                break;
#endif

                            default:
                                DBGT_CRITICAL("API: unsupported video compression format:%u",
                                             (unsigned)port->format.video.eCompressionFormat);
                                DBGT_EPILOG("");
                                return OMX_ErrorUnsupportedIndex;
                        }
#ifndef USE_DEFAULT_OUTPUT_BUFFER_SIZE
                        calculate_new_outputBufferSize(pEnc);
#endif
                        //if width or height is changed for input port it's affecting also
                        //to preprocessor crop
                        OMX_CONFIG_RECTTYPE* crop  = 0;
                        crop = &pEnc->encConfig.crop;
                        crop->nWidth = param->format.video.nStride;
                        crop->nHeight = param->format.video.nFrameHeight;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoPortFormat:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* port    = NULL;
                OMX_VIDEO_PARAM_PORTFORMATTYPE* param = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    DBGT_PDEBUG("param->nIndex - %u ", (int)param->nIndex);
                    case PORT_INDEX_INPUT:
                        port = &pEnc->inputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        port->format.video.eCompressionFormat = param->eCompressionFormat;
                        port->format.video.eColorFormat       = param->eColorFormat;
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        port->format.video.eCompressionFormat = param->eCompressionFormat;
                        port->format.video.eColorFormat       = param->eColorFormat;
                        switch ((OMX_U32)port->format.video.eCompressionFormat)
                        {
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
                            case OMX_VIDEO_CodingAVC:
                                set_avc_defaults(pEnc);
                                break;
#endif
#if defined (ENC6280) || defined (ENC7280)
                            case OMX_VIDEO_CodingH263:
                                set_h263_defaults(pEnc);
                                break;
                            case OMX_VIDEO_CodingMPEG4:
                                set_mpeg4_defaults(pEnc);
                                break;
#endif
#ifdef ENCH1
                            case OMX_VIDEO_CodingVP8:
                                set_vp8_defaults(pEnc);
                                break;
#endif
#ifdef ENCH2
                            case OMX_VIDEO_CodingHEVC:
                                set_hevc_defaults(pEnc);
                                break;
#endif
#ifdef ENCVC8000E
                            case OMX_VIDEO_CodingHEVC:
                                set_vc8000e_hevc_defaults(pEnc);
                                break;
                            
                            case OMX_VIDEO_CodingAVC:
                                set_vc8000e_avc_defaults(pEnc);
                                break;
#endif
#ifdef ENCH2V41
                            case OMX_VIDEO_CodingHEVC:
                                set_h2v41_hevc_defaults(pEnc);
                                break;
														
                            case OMX_VIDEO_CodingAVC:
                                set_h2v41_avc_defaults(pEnc);
                                break;
#endif


                            default:
                                DBGT_CRITICAL("API: Unsupported video compression format:%u",
                                             (unsigned)port->format.video.eCompressionFormat);
                                DBGT_EPILOG("");
                                return OMX_ErrorUnsupportedIndex;
                        }
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
            }
            break;
#if !defined (ENCVC8000E) && !defined (ENCH2V41)
        case OMX_IndexParamVideoAvc:
            {
                OMX_VIDEO_PARAM_AVCTYPE* config  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_VIDEO_PARAM_AVCTYPE* param = (OMX_VIDEO_PARAM_AVCTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No H264 config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        config = &pEnc->encConfig.avc;
                        config->eLevel = param->eLevel;
                        config->eProfile = param->eProfile;
                        config->nPFrames = param->nPFrames;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
#endif

#if defined (ENC6280) || defined (ENC7280)
        case OMX_IndexParamVideoH263:
            {
                OMX_VIDEO_PARAM_H263TYPE* config  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_VIDEO_PARAM_H263TYPE* param = (OMX_VIDEO_PARAM_H263TYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No H263 config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        config = &pEnc->encConfig.h263;
                        config->eProfile = param->eProfile;
                        config->eLevel = param->eLevel;
                        config->nPFrames = param->nPFrames;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
        case OMX_IndexParamVideoMpeg4:
            {
                OMX_VIDEO_PARAM_MPEG4TYPE* config  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_VIDEO_PARAM_MPEG4TYPE* param = (OMX_VIDEO_PARAM_MPEG4TYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No mpeg4 config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        config = &pEnc->encConfig.mpeg4;
                        config->eProfile = param->eProfile;
                        config->eLevel = param->eLevel;
                        config->bSVH = param->bSVH;
                        config->bGov = param->bGov;
                        config->nPFrames = param->nPFrames;
                        config->nMaxPacketSize = param->nMaxPacketSize;
                        config->nTimeIncRes = param->nTimeIncRes;
                        config->nHeaderExtension = param->nHeaderExtension;
                        config->bReversibleVLC = param->bReversibleVLC;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
#endif /* ifdef ENC6280 || ENC7280 */
#ifdef ENCH1
        case OMX_IndexParamVideoVp8:
            {
                OMX_VIDEO_PARAM_VP8TYPE* config  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_VIDEO_PARAM_VP8TYPE* param = (OMX_VIDEO_PARAM_VP8TYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No VP8 config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        config = &pEnc->encConfig.vp8;
                        config->eLevel = param->eLevel;
                        config->eProfile = param->eProfile;
                        config->nDCTPartitions = param->nDCTPartitions;
                        config->bErrorResilientMode = param->bErrorResilientMode;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
#endif
#ifdef ENCH2
        case OMX_IndexParamVideoHevc:
            {
                OMX_VIDEO_PARAM_HEVCTYPE* config  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_VIDEO_PARAM_HEVCTYPE* param = (OMX_VIDEO_PARAM_HEVCTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No HEVC config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        config = &pEnc->encConfig.hevc;
                        /*config->eLevel = param->eLevel;
                        config->eProfile = param->eProfile;
                        config->nBitDepthLuma = param->nBitDepthLuma;
                        config->nBitDepthChroma = param->nBitDepthChroma;
                        config->nPFrames = param->nPFrames;*/
                        memcpy(config, param, param->nSize);
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
#endif
#ifdef ENCVC8000E
        case OMX_IndexParamVideoHevc:
           {
               OMX_VIDEO_PARAM_HEVCTYPE* config  = 0;
               OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
               OMX_VIDEO_PARAM_HEVCTYPE* param = (OMX_VIDEO_PARAM_HEVCTYPE*)pParam;
               switch (param->nPortIndex)
               {
                   case PORT_INDEX_INPUT:
                       DBGT_PDEBUG("No HEVC config for input");
                       break;
                   case PORT_INDEX_OUTPUT:
                       port = &pEnc->outputPort.def;
                       CHECK_PORT_STATE(pEnc, port);
                       config = &pEnc->encConfig.hevc;
                       /*config->eLevel = param->eLevel;
                       config->eProfile = param->eProfile;
                       config->nBitDepthLuma = param->nBitDepthLuma;
                       config->nBitDepthChroma = param->nBitDepthChroma;
                       config->nPFrames = param->nPFrames;*/
                       memcpy(config, param, param->nSize);
                       break;
                   default:
                       DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                       DBGT_EPILOG("");
                       return OMX_ErrorBadPortIndex;
                       break;
               }
           }
           break;
       case OMX_IndexParamVideoAvc:
           {
              OMX_VIDEO_PARAM_AVCTYPE* config  = 0;
              OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
              OMX_VIDEO_PARAM_AVCTYPE* param = (OMX_VIDEO_PARAM_AVCTYPE*)pParam;
              switch (param->nPortIndex)
              {
                  case PORT_INDEX_INPUT:
                      DBGT_PDEBUG("No H264 config for input");
                      break;
                  case PORT_INDEX_OUTPUT:
                      port = &pEnc->outputPort.def;
                      CHECK_PORT_STATE(pEnc, port);
                      config = &pEnc->encConfig.avc;
                      memcpy(config, param, param->nSize);
                      break;
                  default:
                      DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                      DBGT_EPILOG("");
                      return OMX_ErrorBadPortIndex;
                      break;
              }
           }
           break;
       case OMX_IndexParamVideoAvcExt:
           {
               OMX_VIDEO_PARAM_AVCEXTTYPE* config  = 0;
               OMX_VIDEO_PARAM_AVCEXTTYPE* param = (OMX_VIDEO_PARAM_AVCEXTTYPE*)pParam;
               OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
               switch (param->nPortIndex)
               {
                   case PORT_INDEX_INPUT:
                       DBGT_PDEBUG("No H264 extension config for input");
                       break;
                   case PORT_INDEX_OUTPUT:
                       port = &pEnc->outputPort.def;
                       CHECK_PORT_STATE(pEnc, port);
                       config = &pEnc->encConfig.avcExt;
                       memcpy(config, param, param->nSize);
                       break;
                   default:
                       DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                       DBGT_EPILOG("");
                       return OMX_ErrorBadPortIndex;
                       //break;
              }
          }
          break;
#endif

#ifdef ENCH2V41
       case OMX_IndexParamVideoHevc:
           {
              OMX_VIDEO_PARAM_HEVCTYPE* config	= 0;
              OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
              OMX_VIDEO_PARAM_HEVCTYPE* param = (OMX_VIDEO_PARAM_HEVCTYPE*)pParam;
              switch (param->nPortIndex)
              {
                  case PORT_INDEX_INPUT:
                      DBGT_PDEBUG("No HEVC config for input");
                      break;
                  case PORT_INDEX_OUTPUT:
                      port = &pEnc->outputPort.def;
                      CHECK_PORT_STATE(pEnc, port);
                      config = &pEnc->encConfig.hevc;
                      /*config->eLevel = param->eLevel;
                      config->eProfile = param->eProfile;
                      config->nBitDepthLuma = param->nBitDepthLuma;
                      config->nBitDepthChroma = param->nBitDepthChroma;
                      config->nPFrames = param->nPFrames;*/
                      memcpy(config, param, param->nSize);
                      break;
                  default:
                      DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                      DBGT_EPILOG("");
                      return OMX_ErrorBadPortIndex;
                      //break;
              }
           }
           break;
       case OMX_IndexParamVideoAvc:
           {
               OMX_VIDEO_PARAM_AVCTYPE* config  = 0;
               OMX_VIDEO_PARAM_AVCTYPE* param = (OMX_VIDEO_PARAM_AVCTYPE*)pParam;
               OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
               switch (param->nPortIndex)
               {
                   case PORT_INDEX_INPUT:
                       DBGT_PDEBUG("No H264 config for input");
                       break;
                   case PORT_INDEX_OUTPUT:
                       port = &pEnc->outputPort.def;
                       CHECK_PORT_STATE(pEnc, port);
                       config = &pEnc->encConfig.avc;
                       memcpy(config, param, param->nSize);
                       break;
                   default:
                       DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                       DBGT_EPILOG("");
                       return OMX_ErrorBadPortIndex;
                       //break;
              }
          }
          break;
       case OMX_IndexParamVideoAvcExt:
           {
               OMX_VIDEO_PARAM_AVCEXTTYPE* config  = 0;
               OMX_VIDEO_PARAM_AVCEXTTYPE* param = (OMX_VIDEO_PARAM_AVCEXTTYPE*)pParam;
               OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
               switch (param->nPortIndex)
               {
                   case PORT_INDEX_INPUT:
                       DBGT_PDEBUG("No H264 extension config for input");
                       break;
                   case PORT_INDEX_OUTPUT:
                       port = &pEnc->outputPort.def;
                       CHECK_PORT_STATE(pEnc, port);
                       config = &pEnc->encConfig.avcExt;
                       memcpy(config, param, param->nSize);
                       break;
                   default:
                       DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                       DBGT_EPILOG("");
                       return OMX_ErrorBadPortIndex;
                       //break;
              }
          }
          break;
#endif

       case OMX_IndexParamCompBufferSupplier:
            {
                OMX_PARAM_BUFFERSUPPLIERTYPE* param = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pParam;
                PORT* port = encoder_map_index_to_port(pEnc, param->nPortIndex);
                if (!port)
                {
                    DBGT_CRITICAL("OMX_IndexParamCompBufferSupplier, NULL port");
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
                CHECK_PORT_STATE(pEnc, &port->def);

                port->tunnel.eSupplier = param->eBufferSupplier;

                DBGT_PDEBUG("API: new buffer supplier value:%s port:%d",
                            HantroOmx_str_omx_supplier(param->eBufferSupplier),
                            (int)param->nPortIndex);

                if (port->tunnelcomp && port->def.eDir == OMX_DirInput)
                {
                    DBGT_PDEBUG("API: propagating value to tunneled component: %p port: %d",
                                port->tunnelcomp, (int)port->tunnelport);
                    OMX_ERRORTYPE err;
                    OMX_PARAM_BUFFERSUPPLIERTYPE foo;
                    memset(&foo, 0, sizeof(foo));
                    INIT_OMX_VERSION_PARAM(foo);
                    foo.nPortIndex      = port->tunnelport;
                    foo.eBufferSupplier = port->tunnel.eSupplier;
                    err = ((OMX_COMPONENTTYPE*)port->tunnelcomp)->
                            SetParameter(port->tunnelcomp, OMX_IndexParamCompBufferSupplier, &foo);
                    if (err != OMX_ErrorNone)
                    {
                        DBGT_CRITICAL("API: tunneled component refused buffer supplier config:%s",
                                HantroOmx_str_omx_err(err));
                        DBGT_EPILOG("");
                        return err;
                    }
                }
            }
            break;

        case OMX_IndexParamCommonDeblocking:
            {
                OMX_PARAM_DEBLOCKINGTYPE* deblocking  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* port = 0;
                OMX_PARAM_DEBLOCKINGTYPE* param = (OMX_PARAM_DEBLOCKINGTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        DBGT_PDEBUG("No deblocking config for input");
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        CHECK_PORT_STATE(pEnc, port);
                        deblocking = &pEnc->encConfig.deblocking;
                        deblocking->bDeblocking = param->bDeblocking;
                        break;
                    default:
                        DBGT_CRITICAL("API: Bad port index:%u", (unsigned)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                        break;
                }
            }
            break;
        case OMX_IndexParamVideoBitrate:
            {
                OMX_VIDEO_PARAM_BITRATETYPE* paramBitrate;
                paramBitrate = (OMX_VIDEO_PARAM_BITRATETYPE *)pParam;
                OMX_VIDEO_PARAM_BITRATETYPE* portBitrate;
                portBitrate = &pEnc->encConfig.bitrate;
                if (paramBitrate->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    portBitrate->eControlRate = paramBitrate->eControlRate;
                    if (paramBitrate->nTargetBitrate != pEnc->outputPort.def.format.video.nBitrate)
                    {
                        //Port configuration is used when codec is initialized
                        pEnc->outputPort.def.format.video.nBitrate = paramBitrate->nTargetBitrate;
                        //Store target bitrate to OMX_VIDEO_PARAM_BITRATETYPE also
                        //so we can return correct target bitrate easily when
                        // client makes get_param(OMX_IndexParamVideoBitrate) call
                        portBitrate->nTargetBitrate = paramBitrate->nTargetBitrate;
#if 0
                        pEnc->app_callbacks.EventHandler(
                            pEnc->self,
                            pEnc->app_data,
                        OMX_EventPortSettingsChanged,
                        PORT_INDEX_OUTPUT,
                        0,
                        NULL);
#endif
                    }
                }
                else if (paramBitrate->nPortIndex == PORT_INDEX_INPUT)
                {
                    DBGT_EPILOG("");
                    return OMX_ErrorNone;
                }
                else
                {
                    DBGT_CRITICAL("API: Bad port index:%u", (unsigned)paramBitrate->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoErrorCorrection:
            {
                OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE* paramEc;
                paramEc = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE*) pParam;
                OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE* portEc;
                portEc = &pEnc->encConfig.ec;

                if (paramEc->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    portEc->bEnableHEC = paramEc->bEnableHEC;
                    portEc->bEnableResync = paramEc->bEnableResync;
                    portEc->nResynchMarkerSpacing = paramEc->nResynchMarkerSpacing;
                    portEc->bEnableDataPartitioning = paramEc->bEnableDataPartitioning;
                    portEc->bEnableRVLC = paramEc->bEnableRVLC;
                }
                else if (paramEc->nPortIndex == PORT_INDEX_INPUT)
                {
                    DBGT_EPILOG("");
                    return OMX_ErrorNone;
                }
                else
                {
                    DBGT_CRITICAL("API: Bad port index:%u", (unsigned)paramEc->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
                break;
            }
            break;
        case OMX_IndexParamVideoQuantization:
            {
                DBGT_PDEBUG("API: got VideoQuantization param");
                OMX_VIDEO_PARAM_QUANTIZATIONTYPE* paramQ;
                paramQ = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE*) pParam;
                OMX_VIDEO_PARAM_QUANTIZATIONTYPE* portQ;
                portQ = &pEnc->encConfig.videoQuantization;

                if (paramQ->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    portQ->nQpI = paramQ->nQpI;
                    portQ->nQpP = paramQ->nQpP;
					portQ->nQpB = paramQ->nQpB;
                    DBGT_PDEBUG("API:  QpI value: %d", (int)portQ->nQpI);
                    DBGT_PDEBUG("API:  QpP value: %d", (int)portQ->nQpP);
					DBGT_PDEBUG("API:  QpB value: %d", (int)portQ->nQpB);
                }
                else if (paramQ->nPortIndex == PORT_INDEX_INPUT)
                {
                    DBGT_EPILOG("");
                    return OMX_ErrorNone;
                }
                else
                {
                    DBGT_CRITICAL("API: Bad port index:%u", (unsigned)paramQ->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamStandardComponentRole:
            {
                OMX_PARAM_COMPONENTROLETYPE* param = (OMX_PARAM_COMPONENTROLETYPE*)pParam;
                strcpy((char*)pEnc->role, (const char*)param->cRole);
#if 1 //ifdef CONFORMANCE
                if (strcmp((char*)pEnc->role, "video_encoder.avc") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
#if defined (ENC6280) || defined (ENC7280)
                else if (strcmp((char*)pEnc->role, "video_encoder.mpeg4") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
                else if (strcmp((char*)pEnc->role, "video_encoder.h263") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
#endif
#ifdef ENCH1
                else if (strcmp((char*)pEnc->role, "video_encoder.vp8") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
#endif
#ifdef ENCH2
                else if (strcmp((char*)pEnc->role, "video_encoder.hevc") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
                else if (strcmp((char*)pEnc->role, "video_encoder.hevc") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
                else if (strcmp((char*)pEnc->role, "video_encoder.avc") == 0)
                    pEnc->outputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
#endif

#endif // CONFORMANCE
            }
            break;
        case OMX_IndexParamPriorityMgmt:
            {
                CHECK_STATE_NOT_LOADED(pEnc->state);
                OMX_PRIORITYMGMTTYPE* param = (OMX_PRIORITYMGMTTYPE*)pParam;
                pEnc->priority_group = param->nGroupPriority;
                pEnc->priority_id    = param->nGroupID;
            }
            break;
        case OMX_IndexParamVideoProfileLevelCurrent:
            {
                OMX_VIDEO_PARAM_PROFILELEVELTYPE* param = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pParam;
                //CHECK_PORT_INPUT(param); // should this be check port output?
                switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
                {
                    case OMX_VIDEO_CodingAVC:
                        pEnc->encConfig.avc.eProfile = param->eProfile;
                        pEnc->encConfig.avc.eLevel = param->eLevel;
                        break;
#if defined (ENC6280) || defined (ENC7280)
                    case OMX_VIDEO_CodingH263:
                        pEnc->encConfig.h263.eProfile = param->eProfile;
                        pEnc->encConfig.h263.eLevel = param->eLevel;
                        break;
                    case OMX_VIDEO_CodingMPEG4:
                        pEnc->encConfig.mpeg4.eProfile = param->eProfile;
                        pEnc->encConfig.mpeg4.eLevel = param->eLevel;
                        break;
#endif
#ifdef ENCH1
                    case OMX_VIDEO_CodingVP8:
                        pEnc->encConfig.vp8.eProfile = param->eProfile;
                        pEnc->encConfig.vp8.eLevel = param->eLevel;
                        break;
#endif
#if defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
                    case OMX_VIDEO_CodingHEVC:
                        pEnc->encConfig.hevc.eProfile = param->eProfile;
                        pEnc->encConfig.hevc.eLevel = param->eLevel;
                        break;
#endif

                    default:
                        DBGT_EPILOG("");
                        return OMX_ErrorNoMore;
                }
            }
            break;
#if defined (ENC8290) || defined (ENCH1)
        case OMX_IndexParamVideoIntraRefresh:
            {
                OMX_VIDEO_PARAM_INTRAREFRESHTYPE* param;
                param = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE*)pParam;
                if (param->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    if (param->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic)
                    {
                        pEnc->encConfig.intraRefresh.nCirMBs = param->nCirMBs;
                        DBGT_PDEBUG("API: intraRefresh nCirMBs: %u",
                            (unsigned int)pEnc->encConfig.intraRefresh.nCirMBs);
                    }
                    else
                    {
                        DBGT_CRITICAL("Unsupported intra refresh mode (%u)", (unsigned int)param->eRefreshMode);
                        DBGT_EPILOG("");
                        return OMX_ErrorUnsupportedSetting;
                    }
                }
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)param->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
#endif

#ifdef ANDROID
        case (OMX_INDEXTYPE)OMX_google_android_index_prependSPSPPSToIDRFrames:
            {
                struct PrependSPSPPSToIDRFramesParams *param = (struct PrependSPSPPSToIDRFramesParams *) pParam;
#ifdef ENCH1
                pEnc->encConfig.prependSPSPPSToIDRFrames = param->bEnable;
#else
                pEnc->encConfig.prependSPSPPSToIDRFrames = param->bEnable = OMX_FALSE;
                return OMX_ErrorUnsupportedSetting;
#endif
                DBGT_PDEBUG("API: pEnc->encConfig.prependSPSPPSToIDRFrames %d", pEnc->encConfig.prependSPSPPSToIDRFrames);
            }
            break;
#endif
        default:
            DBGT_CRITICAL("API: unsupported settings index");
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE encoder_get_parameter(OMX_IN    OMX_HANDLETYPE hComponent,
                                    OMX_IN    OMX_INDEXTYPE  nIndex,
                                    OMX_INOUT OMX_PTR        pParam)
{
    DBGT_PROLOG("");

    static OMX_S32 output_formats[][2] = {
#if !defined (ENCH2)
        { OMX_VIDEO_CodingAVC,    OMX_COLOR_FormatUnused }
#else
        { OMX_VIDEO_CodingHEVC,    OMX_COLOR_FormatUnused }
#endif
#if defined (ENC6280) || defined (ENC7280)
        ,{ OMX_VIDEO_CodingH263,   OMX_COLOR_FormatUnused },
        { OMX_VIDEO_CodingMPEG4,  OMX_COLOR_FormatUnused }
#endif
#ifdef ENCH1
        ,{ OMX_VIDEO_CodingVP8,    OMX_COLOR_FormatUnused }
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
        ,{ OMX_VIDEO_CodingHEVC,    OMX_COLOR_FormatUnused }
#endif

     };

    static OMX_S32 input_formats[][2] = {
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420Planar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420PackedPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420SemiPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420PackedSemiPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYCbYCr },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatCbYCrY },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitRGB565 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitBGR565 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format12bitRGB444 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitARGB4444 },
#if defined (ENC6280) || defined (ENC7280)
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV422Planar },
#else
        /*{ OMX_VIDEO_CodingUnused, OMX_COLOR_Format24bitRGB888 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format24bitBGR888 },*/
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format25bitARGB1888 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format32bitARGB8888 },
#endif
#ifdef ENCVC8000E
        /*list here*/
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420SemiPlanarP010 },
#endif
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420SemiPlanarVU },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitBGR555        },

        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitARGB1555 }
    };

#ifdef ENC8270
    static OMX_S32 avc_profiles[][2] = {
        { OMX_VIDEO_AVCProfileBaseline,     OMX_VIDEO_AVCLevel41 },
        { OMX_VIDEO_AVCProfileMain,         OMX_VIDEO_AVCLevel41 },
        { OMX_VIDEO_AVCProfileHigh,         OMX_VIDEO_AVCLevel41 }
    };
#endif

#if defined (ENC8290) || defined (ENCH1)
    static OMX_S32 avc_profiles[][2] = {
        { OMX_VIDEO_AVCProfileBaseline,     OMX_VIDEO_AVCLevel51 },
        { OMX_VIDEO_AVCProfileMain,         OMX_VIDEO_AVCLevel51 },
        { OMX_VIDEO_AVCProfileHigh,         OMX_VIDEO_AVCLevel51 }
    };
#endif

#if defined (ENCH2V41)
    static OMX_S32 avc_profiles[][2] = {
        { OMX_VIDEO_AVCProfileBaseline,     OMX_VIDEO_AVCLevel52 },
        { OMX_VIDEO_AVCProfileMain,         OMX_VIDEO_AVCLevel52 },
        { OMX_VIDEO_AVCProfileHigh,         OMX_VIDEO_AVCLevel52 }
    };
#endif
	


#if defined (ENCVC8000E)
    static OMX_S32 avc_profiles[][2] = {
        { OMX_VIDEO_AVCProfileBaseline,     OMX_VIDEO_AVCLevel62 },
        { OMX_VIDEO_AVCProfileMain,         OMX_VIDEO_AVCLevel62 },
        { OMX_VIDEO_AVCProfileHigh,         OMX_VIDEO_AVCLevel62 },
        { OMX_VIDEO_AVCProfileHigh10,       OMX_VIDEO_AVCLevel62 }
    };
#endif


#ifdef ENCH1
    static OMX_S32 vp8_profiles[][2] = {
        { OMX_VIDEO_VP8ProfileMain,         OMX_VIDEO_VP8Level_Version0 }
    };
#endif

#ifdef ENCH2
    static OMX_S32 hevc_profiles[][2] = {
        { OMX_VIDEO_HEVCProfileMain,        OMX_VIDEO_HEVCLevel5 }
    };
#endif

#ifdef ENCH2V41
    static OMX_S32 hevc_profiles[][2] = {
        { OMX_VIDEO_HEVCProfileMain,                OMX_VIDEO_HEVCLevel62 },
        { OMX_VIDEO_HEVCProfileMainStillPicture,    OMX_VIDEO_HEVCLevel62 }
    };
#endif

#ifdef ENCVC8000E
    static OMX_S32 hevc_profiles[][2] = {
        { OMX_VIDEO_HEVCProfileMain,                OMX_VIDEO_HEVCLevel62 },
        { OMX_VIDEO_HEVCProfileMain10,              OMX_VIDEO_HEVCLevel62 },
        { OMX_VIDEO_HEVCProfileMainStillPicture,    OMX_VIDEO_HEVCLevel62 }
    };
#endif


#if defined (ENC6280) || defined (ENC7280)
    static OMX_S32 avc_profiles[][2] = {
        { OMX_VIDEO_AVCProfileBaseline,     OMX_VIDEO_AVCLevel32 }
    };

    static OMX_S32 H263_profiles[][2] = {
        { OMX_VIDEO_H263ProfileBaseline,    OMX_VIDEO_H263Level70 }
    };

    static OMX_S32 mpeg4_profiles[][2] = {
        { OMX_VIDEO_MPEG4ProfileSimple,     OMX_VIDEO_MPEG4Level5 },
        { OMX_VIDEO_MPEG4ProfileMain,       OMX_VIDEO_MPEG4Level4 },
        { OMX_VIDEO_MPEG4ProfileAdvancedSimple,    OMX_VIDEO_MPEG4Level5 }
    };
#endif
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    OSAL_MutexLock(pEnc->statemutex);
    CHECK_STATE_INVALID(pEnc->state);
    OSAL_MutexUnlock(pEnc->statemutex);

    DBGT_PDEBUG("API: Getting index: %s", HantroOmx_str_omx_index(nIndex));

    switch ((OMX_U32)nIndex)
    {
        case OMX_IndexParamAudioInit:
        case OMX_IndexParamOtherInit:
        case OMX_IndexParamImageInit:
            {
                OMX_PORT_PARAM_TYPE* pPortDomain = 0;
                pPortDomain = (OMX_PORT_PARAM_TYPE*) pParam;
                pPortDomain->nPorts = 0;
                pPortDomain->nStartPortNumber = 0;
            }
            break;
        case OMX_IndexParamVideoInit:
            {
                OMX_PORT_PARAM_TYPE* pPortDomain = 0;
                pPortDomain = (OMX_PORT_PARAM_TYPE*) pParam;
                memcpy(pPortDomain, &pEnc->ports, pPortDomain->nSize);
            }
            break;
        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* param = (OMX_PARAM_PORTDEFINITIONTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        port = &pEnc->inputPort.def;
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        break;
                    default:
                        DBGT_CRITICAL("OMX_IndexParamPortDefinition, wrong index (%u)",
                                        (unsigned int)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
                DBGT_ASSERT(param->nSize);
                memcpy(param, port, param->nSize);
            }
            break;
        case OMX_IndexParamVideoPortFormat:
            {
                OMX_VIDEO_PARAM_PORTFORMATTYPE* param = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        if (param->nIndex >= sizeof(input_formats)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->xFramerate = 0;
                        param->eCompressionFormat = input_formats[param->nIndex][0];
                        param->eColorFormat       = input_formats[param->nIndex][1];
                        break;
                    case PORT_INDEX_OUTPUT:
                        if (param->nIndex >= sizeof(output_formats)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->xFramerate = 0;
                        param->eCompressionFormat = output_formats[param->nIndex][0];
                        param->eColorFormat       = output_formats[param->nIndex][1];
                        break;
                    default:
                        DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoAvc:
            {
                OMX_VIDEO_PARAM_AVCTYPE* avc;
                avc = (OMX_VIDEO_PARAM_AVCTYPE *)pParam;
                if (avc->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(avc, &pEnc->encConfig.avc, avc->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)avc->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
#if defined (ENC6280) || defined (ENC7280)
        case OMX_IndexParamVideoMpeg4:
            {
                OMX_VIDEO_PARAM_MPEG4TYPE* mpeg4;
                mpeg4 = (OMX_VIDEO_PARAM_MPEG4TYPE *)pParam;
                if (mpeg4->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(mpeg4, &pEnc->encConfig.mpeg4, mpeg4->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)mpeg4->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoH263:
            {
                OMX_VIDEO_PARAM_H263TYPE* h263;
                h263 = (OMX_VIDEO_PARAM_H263TYPE *)pParam;
                if (h263->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(h263, &pEnc->encConfig.h263, h263->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)h263->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
#else
        case OMX_IndexParamVideoH263:
        case OMX_IndexParamVideoMpeg4:
            break;
#endif
#ifdef ENCH1
        case OMX_IndexParamVideoVp8:
            {
                OMX_VIDEO_PARAM_VP8TYPE* vp8;
                vp8 = (OMX_VIDEO_PARAM_VP8TYPE *)pParam;
                if (vp8->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(vp8, &pEnc->encConfig.vp8, vp8->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)vp8->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
#endif
#if defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
        case OMX_IndexParamVideoHevc:
            {
                OMX_VIDEO_PARAM_HEVCTYPE* hevc;
                hevc = (OMX_VIDEO_PARAM_HEVCTYPE *)pParam;
                if (hevc->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(hevc, &pEnc->encConfig.hevc, hevc->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)hevc->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoAvcExt:
        {
            OMX_VIDEO_PARAM_AVCEXTTYPE* avc_ext;
            avc_ext = (OMX_VIDEO_PARAM_AVCEXTTYPE *)pParam;
            if (avc_ext->nPortIndex == PORT_INDEX_OUTPUT)
                memcpy(avc_ext, &pEnc->encConfig.avcExt, avc_ext->nSize);
            else
            {
                DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)avc_ext->nPortIndex);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
        }
        break;
#endif

        case OMX_IndexParamVideoErrorCorrection:
            {
                OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE* ec;
                ec = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE*) pParam;
                if (ec->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(ec, &pEnc->encConfig.ec, ec->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)ec->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;

        case OMX_IndexParamVideoProfileLevelCurrent:
            {
                OMX_VIDEO_PARAM_PROFILELEVELTYPE* param = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pParam;
                CHECK_PORT_OUTPUT(param);
                switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
                {
#if !defined (ENCH2)
                    case OMX_VIDEO_CodingAVC:
                        param->eProfile = pEnc->encConfig.avc.eProfile;
                        param->eLevel   = pEnc->encConfig.avc.eLevel;
                        break;
#else
                    case OMX_VIDEO_CodingHEVC:
                        param->eProfile = pEnc->encConfig.hevc.eProfile;
                        param->eLevel   = pEnc->encConfig.hevc.eLevel;
                        break;
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
                    case OMX_VIDEO_CodingHEVC:
                        param->eProfile = pEnc->encConfig.hevc.eProfile;
                        param->eLevel   = pEnc->encConfig.hevc.eLevel;
                        break;

#endif

#if defined (ENC6280) || defined (ENC7280)
                    case OMX_VIDEO_CodingH263:
                        param->eProfile = pEnc->encConfig.h263.eProfile;
                        param->eLevel   = pEnc->encConfig.h263.eLevel;
                        break;
                    case OMX_VIDEO_CodingMPEG4:
                        param->eProfile = pEnc->encConfig.mpeg4.eProfile;
                        param->eLevel   = pEnc->encConfig.mpeg4.eLevel;
                        break;
#endif
#ifdef ENCH1
                    case OMX_VIDEO_CodingVP8:
                        param->eProfile = pEnc->encConfig.vp8.eProfile;
                        param->eLevel   = pEnc->encConfig.vp8.eLevel;
                        break;
#endif
                    default:
                        DBGT_ERROR("Unsupported compression format (%x)",
                                    pEnc->outputPort.def.format.video.eCompressionFormat);
                        DBGT_EPILOG("");
                        return OMX_ErrorNoMore;
                }
            }
            break;

        case OMX_IndexParamVideoProfileLevelQuerySupported:
            {
                OMX_VIDEO_PARAM_PROFILELEVELTYPE* param = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pParam;
                //CHECK_PORT_INPUT(param); // conformance tester uses output port for query
                switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
                {
#if !defined (ENCH2)
                    case OMX_VIDEO_CodingAVC:
                        if (param->nProfileIndex >= sizeof(avc_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = avc_profiles[param->nProfileIndex][0];
                        param->eLevel   = avc_profiles[param->nProfileIndex][1];
                        break;
#else
                    case OMX_VIDEO_CodingHEVC:
                        if (param->nProfileIndex >= sizeof(hevc_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = hevc_profiles[param->nProfileIndex][0];
                        param->eLevel   = hevc_profiles[param->nProfileIndex][1];
                        break;
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
                    case OMX_VIDEO_CodingHEVC:
                        if (param->nProfileIndex >= sizeof(hevc_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = hevc_profiles[param->nProfileIndex][0];
                        param->eLevel   = hevc_profiles[param->nProfileIndex][1];
                        break;

#endif
#if defined (ENC6280) || defined (ENC7280)
                    case OMX_VIDEO_CodingH263:
                        if (param->nProfileIndex >= sizeof(H263_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = H263_profiles[param->nProfileIndex][0];
                        param->eLevel   = H263_profiles[param->nProfileIndex][1];
                    break;
                    case OMX_VIDEO_CodingMPEG4:
                        if (param->nProfileIndex >= sizeof(mpeg4_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = mpeg4_profiles[param->nProfileIndex][0];
                        param->eLevel   = mpeg4_profiles[param->nProfileIndex][1];
                    break;
#endif
#ifdef ENCH1
                    case OMX_VIDEO_CodingVP8:
                        if (param->nProfileIndex >= sizeof(vp8_profiles)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nProfileIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        param->eProfile = vp8_profiles[param->nProfileIndex][0];
                        param->eLevel   = vp8_profiles[param->nProfileIndex][1];
                        break;
#endif
                    default:
                        DBGT_ERROR("Unsupported compression format (%x)",
                                    pEnc->outputPort.def.format.video.eCompressionFormat);
                        DBGT_EPILOG("");
                        return OMX_ErrorNoMore;
                }
            }
            break;

        case OMX_IndexParamCommonDeblocking:
            {
                OMX_PARAM_DEBLOCKINGTYPE* deb;
                deb = (OMX_PARAM_DEBLOCKINGTYPE *)pParam;
                if (deb->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(deb, &pEnc->encConfig.deblocking, deb->nSize) ;
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)deb->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;

        case OMX_IndexParamVideoQuantization:
            {
                OMX_VIDEO_PARAM_QUANTIZATIONTYPE* qp;
                qp = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE*)pParam;
                if (qp->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(qp, &pEnc->encConfig.videoQuantization, qp->nSize);
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)qp->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;

        case OMX_IndexParamVideoBitrate:
            {
                OMX_VIDEO_PARAM_BITRATETYPE* bitrate;
                bitrate = (OMX_VIDEO_PARAM_BITRATETYPE *)pParam;
                if (bitrate->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(bitrate, &pEnc->encConfig.bitrate, bitrate->nSize) ;
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)bitrate->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamCompBufferSupplier:
            {
                OMX_PARAM_BUFFERSUPPLIERTYPE* param = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pParam;
                PORT* port = encoder_map_index_to_port(pEnc, param->nPortIndex);
                if (!port)
                {
                    DBGT_CRITICAL("NULL port, OMX_ErrorBadPortIndex");
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
                param->eBufferSupplier = port->tunnel.eSupplier;
            }
            break;

        case OMX_IndexParamStandardComponentRole:
            {
                OMX_PARAM_COMPONENTROLETYPE* param = (OMX_PARAM_COMPONENTROLETYPE*)pParam;
                strcpy((char*)param->cRole, (const char*)pEnc->role);
            }
            break;

        case OMX_IndexParamPriorityMgmt:
            {
                OMX_PRIORITYMGMTTYPE* param = (OMX_PRIORITYMGMTTYPE*)pParam;
                param->nGroupPriority = pEnc->priority_group;
                param->nGroupID       = pEnc->priority_id;
            }
            break;
#if defined (ENC8290) || defined (ENCH1)
        case OMX_IndexParamVideoIntraRefresh:
            {
                OMX_VIDEO_PARAM_INTRAREFRESHTYPE* param;
                param = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE*)pParam;
                if (param->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    memcpy(param, &pEnc->encConfig.intraRefresh, param->nSize);
                }
                else
                {
                    DBGT_CRITICAL("OMX_ErrorBadPortIndex (%u)", (unsigned int)param->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
#endif

        default:
            DBGT_CRITICAL("API: unsupported settings index: %d", nIndex);
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#endif //OMX_ENCODER_VIDEO_DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN
static
OMX_ERRORTYPE encoder_set_parameter(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE  nIndex,
                                    OMX_IN OMX_PTR        pParam)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorHardware;

    CHECK_STATE_INVALID(pEnc->state);
    CHECK_STATE_IDLE(pEnc->state);

    if (pEnc->state != OMX_StateLoaded && pEnc->state != OMX_StateWaitForResources)
    {
        DBGT_CRITICAL("API: unsupported state: %s", HantroOmx_str_omx_state(pEnc->state));
        return OMX_ErrorIncorrectStateOperation;
    }

    DBGT_PDEBUG("API: Setting index:%s", HantroOmx_str_omx_index(nIndex));

    switch (nIndex)
    {
        case OMX_IndexParamAudioInit: //Fall through
        case OMX_IndexParamOtherInit:
        case OMX_IndexParamVideoInit:
            DBGT_CRITICAL("API: Unsupported index");
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
        case OMX_IndexParamImageInit:
            DBGT_EPILOG("");
            return OMX_ErrorNone;
            break;

        case OMX_IndexParamImagePortFormat:
        {
            OMX_PARAM_PORTDEFINITIONTYPE* port    = NULL;
            OMX_IMAGE_PARAM_PORTFORMATTYPE* param = (OMX_IMAGE_PARAM_PORTFORMATTYPE*)pParam;
            switch (param->nPortIndex)
            {
                case PORT_INDEX_INPUT:
                    port = &pEnc->inputPort.def;
                    CHECK_PORT_STATE(pEnc, port);
                    port->format.image.eColorFormat       = param->eColorFormat;
                    port->format.image.eCompressionFormat = param->eCompressionFormat;
                    break;
                case PORT_INDEX_OUTPUT:
                    port = &pEnc->outputPort.def;
                    CHECK_PORT_STATE(pEnc, port);
                    port->format.image.eColorFormat       = param->eColorFormat;
                    port->format.image.eCompressionFormat = param->eCompressionFormat;
                    switch ((OMX_U32)port->format.image.eCompressionFormat)
                    {
#ifdef ENABLE_JPEG
                        case OMX_IMAGE_CodingJPEG:
                            set_jpeg_defaults(pEnc);
                            break;
#endif
#ifdef ENCH1
                        case OMX_IMAGE_CodingWEBP:
                            set_webp_defaults(pEnc);
                            break;
#endif
                        default:
                            DBGT_CRITICAL("API: image compression format");
                            DBGT_EPILOG("");
                            return OMX_ErrorUnsupportedIndex;
                    }
                    break;
                default:
                    DBGT_CRITICAL("API: no such image port:%u", (unsigned)param->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE* port  = NULL;
            OMX_PARAM_PORTDEFINITIONTYPE* param = (OMX_PARAM_PORTDEFINITIONTYPE*)pParam;
            if (param->eDomain != OMX_PortDomainImage)
            {
                DBGT_CRITICAL("API: unsupported port domain");
                DBGT_EPILOG("");
                return OMX_ErrorUnsupportedSetting;
            }
            switch (param->nPortIndex)
            {
                case PORT_INDEX_INPUT:
                    port = &pEnc->inputPort.def;
                    CHECK_PORT_STATE(pEnc, port);
                    if (param->format.image.eCompressionFormat != OMX_IMAGE_CodingUnused)
                    {
                        DBGT_CRITICAL("API: Only uncompressed image accepted for input");
                        DBGT_EPILOG("");
                        return OMX_ErrorUnsupportedSetting;
                    }
                    switch (param->format.image.eColorFormat)
                    {
                        case OMX_COLOR_FormatYUV420Planar:
                        case OMX_COLOR_FormatYUV420PackedPlanar:
                        case OMX_COLOR_FormatYUV420SemiPlanar:
                        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
                        case OMX_COLOR_FormatYCbYCr: // YCbCr 4:2:2 interleaved (YUYV)
                        case OMX_COLOR_FormatCbYCrY:
                        case OMX_COLOR_Format16bitRGB565:
                        case OMX_COLOR_Format16bitBGR565:
                        case OMX_COLOR_Format12bitRGB444:
                        case OMX_COLOR_Format16bitARGB4444:
                        case OMX_COLOR_Format16bitARGB1555:
#if defined (ENC6280) || defined (ENC7280)
                        case OMX_COLOR_FormatYUV422Planar:
#else
                        /*case OMX_COLOR_Format24bitRGB888:
                        case OMX_COLOR_Format24bitBGR888:*/
                        case OMX_COLOR_Format25bitARGB1888:
                        case OMX_COLOR_Format32bitARGB8888:
#endif
                            break;
                        default:
                            DBGT_CRITICAL("API: unsupported image color format");
                            DBGT_EPILOG("");
                            return OMX_ErrorUnsupportedSetting;
                    }
                    if (param->nBufferCountActual >= port->nBufferCountMin)
                        port->nBufferCountActual        = param->nBufferCountActual;

                    port->format.image.nStride          = param->format.image.nStride;
                    port->format.image.nSliceHeight     = param->format.image.nSliceHeight;
                    port->format.image.eColorFormat     = param->format.image.eColorFormat;
                    port->format.image.nFrameWidth      = param->format.image.nFrameWidth;
                    port->format.image.nFrameHeight     = param->format.image.nFrameHeight;
                    //port->nBufferSize                   = param->nBufferSize; // According to OMX IL spec, port buffer size is read only
#ifndef USE_DEFAULT_INPUT_BUFFER_SIZE
                    err =  calculate_frame_size(pEnc, &port->nBufferSize);
#endif
                    if (err != OMX_ErrorNone)
                    {
                        DBGT_CRITICAL("Error calculating frame size (err=%x)", err);
                        return err;
                    }
                    DBGT_PDEBUG("OMX_IndexParamPortDefinition: port->nBufferSize = %d, param->nBufferSize = %d",
                                (int)port->nBufferSize, (int)param->nBufferSize);
                    break;

                case PORT_INDEX_OUTPUT:
                    port = &pEnc->outputPort.def;
                    CHECK_PORT_STATE(pEnc, port);
                    DBGT_ASSERT(pEnc->outputPort.def.format.image.eCompressionFormat == OMX_IMAGE_CodingJPEG ||
                            pEnc->outputPort.def.format.image.eCompressionFormat == (OMX_U32)OMX_IMAGE_CodingWEBP);
                    if (param->nBufferCountActual >= port->nBufferCountMin)
                        port->nBufferCountActual          = param->nBufferCountActual;

                    port->format.image.nFrameWidth        = param->format.image.nFrameWidth;
                    port->format.image.nFrameHeight       = param->format.image.nFrameHeight;
                    port->format.image.nSliceHeight       = param->format.image.nSliceHeight;
                    port->format.image.eCompressionFormat = param->format.image.eCompressionFormat;
                    switch ((OMX_U32)port->format.image.eCompressionFormat)
                    {
#ifdef ENABLE_JPEG
                        case OMX_IMAGE_CodingJPEG:
                            set_jpeg_defaults(pEnc);
                            break;
#endif
#ifdef ENCH1
                        case OMX_IMAGE_CodingWEBP:
                            set_webp_defaults(pEnc);
                            break;
#endif
                        default:
                            DBGT_CRITICAL("API: image compression format");
                            DBGT_EPILOG("");
                            return OMX_ErrorUnsupportedIndex;
                    }
#ifndef USE_DEFAULT_OUTPUT_BUFFER_SIZE
                    calculate_new_outputBufferSize(pEnc);
#endif
                    //if width or height is changed for input port it's affecting also
                    //to preprocessor crop
                    OMX_CONFIG_RECTTYPE* crop  = 0;
                    crop = &pEnc->encConfig.crop;
                    crop->nWidth = param->format.video.nStride;
                    crop->nHeight = param->format.video.nFrameHeight;
                    break;

                default:
                    DBGT_CRITICAL("API: Bad port index: %u", (unsigned) param->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
            }
        }
        break;
        case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE* paramQf;
            OMX_IMAGE_PARAM_QFACTORTYPE* portQf;
            paramQf = (OMX_IMAGE_PARAM_QFACTORTYPE*)pParam;
            portQf = &pEnc->encConfig.imageQuantization;

            if (paramQf->nPortIndex == PORT_INDEX_OUTPUT)
            {
                portQf->nQFactor = paramQf->nQFactor;
                DBGT_PDEBUG("API: QFactor: %d", (int)portQf->nQFactor);
            }
            else
            {
                DBGT_CRITICAL("API: Bad port index: %u", (unsigned) paramQf->nPortIndex);
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamCompBufferSupplier:
        {
            OMX_PARAM_BUFFERSUPPLIERTYPE* param = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pParam;
            PORT* port = encoder_map_index_to_port(pEnc, param->nPortIndex);
            if (!port)
            {
                DBGT_CRITICAL("encoder_map_index_to_port (NULL port)");
                DBGT_EPILOG("");
                return OMX_ErrorBadPortIndex;
            }
            CHECK_PORT_STATE(pEnc, &port->def);

            port->tunnel.eSupplier = param->eBufferSupplier;

            DBGT_PDEBUG("API: new buffer supplier value:%s port:%d",
                        HantroOmx_str_omx_supplier(param->eBufferSupplier),
                        (int)param->nPortIndex);

            if (port->tunnelcomp && port->def.eDir == OMX_DirInput)
            {
                DBGT_PDEBUG("API: propagating value to tunneled component: %p port: %d",
                            port->tunnelcomp, (int)port->tunnelport);
                OMX_ERRORTYPE err;
                OMX_PARAM_BUFFERSUPPLIERTYPE foo;
                memset(&foo, 0, sizeof(foo));
                INIT_OMX_VERSION_PARAM(foo);
                foo.nPortIndex      = port->tunnelport;
                foo.eBufferSupplier = port->tunnel.eSupplier;
                err = ((OMX_COMPONENTTYPE*)port->tunnelcomp)->
                        SetParameter(port->tunnelcomp, OMX_IndexParamCompBufferSupplier, &foo);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("API: tunneled component refused buffer supplier config:%s",
                            HantroOmx_str_omx_err(err));
                    DBGT_EPILOG("");
                    return err;
                }
            }
        }
        break;

        case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE* param = (OMX_PARAM_COMPONENTROLETYPE*)pParam;
            strcpy((char*)pEnc->role, (const char*)param->cRole);
#ifdef CONFORMANCE
                if (strcmp((char*)pEnc->role, "image_encoder.jpeg") == 0)
                    pEnc->outputPort.def.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
#ifdef ENCH1
                else if (strcmp((char*)pEnc->role, "image_encoder.webp") == 0)
                    pEnc->outputPort.def.format.image.eCompressionFormat = OMX_IMAGE_CodingWEBP;
#endif
#endif // CONFORMANCE
        }
        break;
        case OMX_IndexParamPriorityMgmt:
        {
            CHECK_STATE_NOT_LOADED(pEnc->state);
            OMX_PRIORITYMGMTTYPE* param = (OMX_PRIORITYMGMTTYPE*)pParam;
            pEnc->priority_group = param->nGroupPriority;
            pEnc->priority_id    = param->nGroupID;
        }
        break;
#ifdef CONFORMANCE
        case OMX_IndexParamQuantizationTable:
        {
            OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *param =
                (OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *) pParam;
            memcpy(&pEnc->quant_table, param, param->nSize);
        }
        break;
#endif
        default:
            DBGT_CRITICAL("API: unsupported settings index (%x)", (unsigned int)nIndex);
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE encoder_get_parameter(OMX_IN    OMX_HANDLETYPE hComponent,
                                    OMX_IN    OMX_INDEXTYPE  nIndex,
                                    OMX_INOUT OMX_PTR        pParam)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    OSAL_MutexLock(pEnc->statemutex);
    CHECK_STATE_INVALID(pEnc->state);
    OSAL_MutexUnlock(pEnc->statemutex);

    DBGT_PDEBUG("API: Getting index:%s", HantroOmx_str_omx_index(nIndex));

static OMX_S32 output_formats[][2] = {
#ifdef ENABLE_JPEG
        { OMX_IMAGE_CodingJPEG,    OMX_COLOR_FormatUnused },
#endif
#ifdef ENCH1
        { OMX_IMAGE_CodingWEBP,    OMX_COLOR_FormatUnused },
#endif
     };

static OMX_S32 input_formats[][2] = {
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420Planar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420PackedPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420SemiPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420PackedSemiPlanar },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYCbYCr },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatCbYCrY },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitRGB565 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitBGR565 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitARGB4444 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format12bitRGB444 },
#if defined (ENC6280) || defined (ENC7280)
        { OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV422Planar },
#else
        /*{ OMX_VIDEO_CodingUnused, OMX_COLOR_Format24bitRGB888 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format24bitBGR888 },*/
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format25bitARGB1888 },
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format32bitARGB8888 },
#endif
        { OMX_VIDEO_CodingUnused, OMX_COLOR_Format16bitARGB1555 }
    };

    switch (nIndex)
    {
        case OMX_IndexParamAudioInit:
        case OMX_IndexParamOtherInit:
        case OMX_IndexParamVideoInit:
            {
                OMX_PORT_PARAM_TYPE* pPortDomain;
                pPortDomain = (OMX_PORT_PARAM_TYPE*) pParam;
                pPortDomain->nPorts = 0;
                pPortDomain->nStartPortNumber = 0;
            }
            break;
        case OMX_IndexParamImageInit:
            {
                OMX_PORT_PARAM_TYPE* pPortDomain;
                pPortDomain = (OMX_PORT_PARAM_TYPE*) pParam;
                memcpy(pPortDomain, &pEnc->ports, pPortDomain->nSize);
            }
            break;

        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                OMX_PARAM_PORTDEFINITIONTYPE* param = (OMX_PARAM_PORTDEFINITIONTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        port = &pEnc->inputPort.def;
                        break;
                    case PORT_INDEX_OUTPUT:
                        port = &pEnc->outputPort.def;
                        break;
                    default:
                        DBGT_CRITICAL("Bad port index (%x)", (unsigned int)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
                DBGT_ASSERT(param->nSize);
                memcpy(param, port, param->nSize);
            }
            break;

        case OMX_IndexParamImagePortFormat:
            {
                OMX_IMAGE_PARAM_PORTFORMATTYPE* param = (OMX_IMAGE_PARAM_PORTFORMATTYPE*)pParam;
                switch (param->nPortIndex)
                {
                    case PORT_INDEX_INPUT:
                        if (param->nIndex >= sizeof(input_formats)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        //param->xFramerate = 0;
                        param->eCompressionFormat = input_formats[param->nIndex][0];
                        param->eColorFormat       = input_formats[param->nIndex][1];
                        break;
                    case PORT_INDEX_OUTPUT:
                        if (param->nIndex >= sizeof(output_formats)/(sizeof(int)*2))
                        {
                            DBGT_ERROR("OMX_ErrorNoMore (%u)", (unsigned int)param->nIndex);
                            DBGT_EPILOG("");
                            return OMX_ErrorNoMore;
                        }
                        //param->xFramerate = 0;
                        param->eCompressionFormat = output_formats[param->nIndex][0];
                        param->eColorFormat       = output_formats[param->nIndex][1];
                        break;
                    default:
                        DBGT_CRITICAL("Bad port index (%x)", (unsigned int)param->nPortIndex);
                        DBGT_EPILOG("");
                        return OMX_ErrorBadPortIndex;
                }
            }
            break;

        case OMX_IndexParamQFactor:
            {
                OMX_IMAGE_PARAM_QFACTORTYPE* qf;
                qf = (OMX_IMAGE_PARAM_QFACTORTYPE *)pParam;
                if (qf->nPortIndex == PORT_INDEX_OUTPUT)
                    memcpy(qf, &pEnc->encConfig.imageQuantization, qf->nSize);
                else
                {
                    DBGT_CRITICAL("Bad port index (%x)", (unsigned int)qf->nPortIndex);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
            }
            break;
        // these are specified for standard JPEG encoder
#ifdef CONFORMANCE
        case OMX_IndexParamQuantizationTable:
        {
            OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *param =
                (OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *) pParam;
            memcpy(param, &pEnc->quant_table, param->nSize);
        }
        break;
#endif
        case OMX_IndexParamCompBufferSupplier:
            {
                OMX_PARAM_BUFFERSUPPLIERTYPE* param = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pParam;
                PORT* port = encoder_map_index_to_port(pEnc, param->nPortIndex);
                if (!port)
                {
                    DBGT_CRITICAL("encoder_map_index_to_port NULL port");
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
                param->eBufferSupplier = port->tunnel.eSupplier;
            }
            break;

        case OMX_IndexParamStandardComponentRole:
            {
                OMX_PARAM_COMPONENTROLETYPE* param = (OMX_PARAM_COMPONENTROLETYPE*)pParam;
                strcpy((char*)param->cRole, (const char*)pEnc->role);
            }
            break;
        case OMX_IndexParamPriorityMgmt:
            {
                OMX_PRIORITYMGMTTYPE* param = (OMX_PRIORITYMGMTTYPE*)pParam;
                param->nGroupPriority = pEnc->priority_group;
                param->nGroupID       = pEnc->priority_id;
            }
            break;
        default:
            DBGT_CRITICAL("API: Unsupported index (%x)", nIndex);
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif //#ifdef OMX_ENCODER_IMAGE_DOMAIN

static
OMX_ERRORTYPE encoder_set_config(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE  nIndex,
                                 OMX_IN OMX_PTR        pParam)

{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    CHECK_STATE_INVALID(pEnc->state);

    DBGT_PDEBUG("API: config index:%s", HantroOmx_str_omx_index(nIndex));

    /*if (pEnc->state != OMX_StateLoaded && pEnc->state != OMX_StateIdle && pEnc->state != OMX_StateExecuting)
    {
        // by an agreement with the client. Simplify parameter setting by allowing
        // parameters to be set only in the loaded/idle state.
        // OMX specification does not know about such constraint, but an implementation is allowed to do this.
        DBGT_CRITICAL("API: unsupported state: %s", HantroOmx_str_omx_state(pEnc->state));
        return OMX_ErrorUnsupportedSetting;
    }*/

    switch ((OMX_U32)nIndex)
    {
        case OMX_IndexConfigCommonRotate:
            {
                OMX_CONFIG_ROTATIONTYPE* param = (OMX_CONFIG_ROTATIONTYPE*)pParam;
                CHECK_PORT_INPUT(param);
                memcpy(&pEnc->encConfig.rotation, param, param->nSize);
            }
            break;
        case OMX_IndexConfigCommonInputCrop:
            {
                OMX_CONFIG_RECTTYPE* param = (OMX_CONFIG_RECTTYPE*)pParam;
                CHECK_PORT_INPUT(param);

#if /*defined (ENCVC8000E) ||*/ defined (ENCH2V41)
                param->nLeft = (param->nLeft + 1) & (~1);
                param->nTop = (param->nTop + 1) & (~1);
#endif
                memcpy(&pEnc->encConfig.crop, param, param->nSize);
                //if crop is changed it's affecting to output port
                //height and width.
                DBGT_PDEBUG("SetConfig Crop, nLeft : %u", (unsigned)param->nLeft);
                DBGT_PDEBUG("SetConfig Crop, nTop : %u", (unsigned)param->nTop);

                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                port = &pEnc->outputPort.def;
#ifdef OMX_ENCODER_VIDEO_DOMAIN
                port->format.video.nFrameHeight = param->nHeight;
                port->format.video.nFrameWidth = param->nWidth;
                DBGT_PDEBUG("SetConfig Crop, port->format.video.nFrameHeight : %u",
                            (unsigned)port->format.video.nFrameHeight);
                DBGT_PDEBUG("SetConfig Crop, port->format.video.nFrameWidth : %u",
                            (unsigned)port->format.video.nFrameWidth);
#endif
#ifdef OMX_ENCODER_IMAGE_DOMAIN
                port->format.image.nFrameHeight = param->nHeight;
                port->format.image.nFrameWidth = param->nWidth;
                DBGT_PDEBUG("SetConfig Crop, port->format.image.nFrameHeight : %u",
                            (unsigned)port->format.image.nFrameHeight);
                DBGT_PDEBUG("SetConfig Crop, port->format.image.nFrameWidth : %u",
                            (unsigned)port->format.image.nFrameWidth);
#endif
#ifndef USE_DEFAULT_OUTPUT_BUFFER_SIZE
                calculate_new_outputBufferSize(pEnc);
#endif
                pEnc->app_callbacks.EventHandler(
                    pEnc->self,
                    pEnc->app_data,
                    OMX_EventPortSettingsChanged,
                    PORT_INDEX_OUTPUT,
                    0,
                    NULL);
            }
            break;
#ifdef OMX_ENCODER_VIDEO_DOMAIN
        case OMX_IndexConfigCommonFrameStabilisation:
            {
                OMX_CONFIG_FRAMESTABTYPE* param = (OMX_CONFIG_FRAMESTABTYPE*) pParam;
                if (param->nPortIndex == PORT_INDEX_OUTPUT)
                {
                    DBGT_CRITICAL("API: wrong port index");
                    DBGT_EPILOG("");
                    return OMX_ErrorBadPortIndex;
                }
                OMX_CONFIG_FRAMESTABTYPE* stab = 0;
                stab = &pEnc->encConfig.stab;
#if defined(ENCVC8000E) || defined (ENCH2V41)
                //The test-bech of VC8000E doesn't have stab config.
                stab->bStab = OMX_FALSE;
#else
                stab->bStab = param->bStab;
#endif
            }
            break;
        case OMX_IndexConfigVideoBitrate:
            {
                OMX_VIDEO_CONFIG_BITRATETYPE* param = (OMX_VIDEO_CONFIG_BITRATETYPE*) pParam;
                DBGT_PDEBUG("SetConfig Bitrate: %u", (unsigned int)param->nEncodeBitrate);

                OMX_VIDEO_PARAM_BITRATETYPE* encBitrate = 0;
                encBitrate = &pEnc->encConfig.bitrate;
                encBitrate->nTargetBitrate = param->nEncodeBitrate;
                //Store target bitrate to output port and to encoder config bitrate
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                port = &pEnc->outputPort.def;
                port->format.video.nBitrate = param->nEncodeBitrate;
#if 0
                //Send port settings changed event to client
                pEnc->app_callbacks.EventHandler(
                    pEnc->self,
                    pEnc->app_data,
                    OMX_EventPortSettingsChanged,
                    PORT_INDEX_OUTPUT,
                    0,
                    NULL);
#endif
            }
            break;

        case OMX_IndexConfigVideoFramerate:
            {
                OMX_CONFIG_FRAMERATETYPE* param = (OMX_CONFIG_FRAMERATETYPE*) pParam;
                DBGT_PDEBUG("SetConfig Framerate: %.2f", Q16_FLOAT(param->xEncodeFramerate));
                //Store target bitrate to output port and to encoder config bitrate
                OMX_PARAM_PORTDEFINITIONTYPE* port  = 0;
                port = &pEnc->outputPort.def;
                port->format.video.xFramerate = param->xEncodeFramerate;
                set_frame_rate(pEnc);
#if 0
                //Send port settings changed event to client
                pEnc->app_callbacks.EventHandler(
                    pEnc->self,
                    pEnc->app_data,
                    OMX_EventPortSettingsChanged,
                    PORT_INDEX_OUTPUT,
                    0,
                    NULL);
#endif
            }
            break;
#ifdef ENCH1
        case OMX_IndexConfigVideoVp8ReferenceFrame:
            {
                OMX_VIDEO_VP8REFERENCEFRAMETYPE* param = (OMX_VIDEO_VP8REFERENCEFRAMETYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.vp8Ref, param, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoAdaptiveRoi:
            {
                OMX_VIDEO_CONFIG_ADAPTIVEROITYPE* param = (OMX_VIDEO_CONFIG_ADAPTIVEROITYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.adaptiveRoi, param, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoVp8TemporalLayers:
            {
                OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE* param = (OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.temporalLayer, param, param->nSize);
            }
            break;
#endif /* ENCH1 */
        case OMX_IndexConfigVideoIntraVOPRefresh:
            {
                OMX_CONFIG_INTRAREFRESHVOPTYPE* param = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.intraRefreshVop, param, param->nSize);
            }
            break;
#if !defined (ENCH2) && !defined (ENCVC8000E) && !defined (ENCH2V41)
        case OMX_IndexConfigVideoAVCIntraPeriod:
            {
                OMX_VIDEO_CONFIG_AVCINTRAPERIOD* param = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.avcIdr, param, param->nSize);
                pEnc->encConfig.avc.nPFrames = param->nPFrames;
                set_avc_intra_period(pEnc);
            }
            break;
#endif
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
        case OMX_IndexConfigVideoIntraArea:
            {
                OMX_VIDEO_CONFIG_INTRAAREATYPE* param = (OMX_VIDEO_CONFIG_INTRAAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(&pEnc->encConfig.intraArea, param, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoIPCMArea:
            {
                OMX_VIDEO_CONFIG_IPCMAREATYPE* param = (OMX_VIDEO_CONFIG_IPCMAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_IPCM_AREA)
                    memcpy(&pEnc->encConfig.ipcmArea[idx], param, param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown IPCM area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        case OMX_IndexConfigVideoRoiArea:
            {
                OMX_VIDEO_CONFIG_ROIAREATYPE* param = (OMX_VIDEO_CONFIG_ROIAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_ROI_AREA)
                    memcpy(&pEnc->encConfig.roiArea[idx], param, param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown ROI area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        case OMX_IndexConfigVideoRoiDeltaQp:
            {
                OMX_VIDEO_CONFIG_ROIDELTAQPTYPE* param = (OMX_VIDEO_CONFIG_ROIDELTAQPTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_ROI_DELTAQP)
                    memcpy(&pEnc->encConfig.roiDeltaQP[idx], param, param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown ROI area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
#endif
#endif /* OMX_ENCODER_VIDEO_DOMAIN */
        default:
        DBGT_CRITICAL("Bad index (%x)", nIndex);
        DBGT_EPILOG("");
        return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE encoder_get_config(OMX_IN    OMX_HANDLETYPE hComponent,
                                 OMX_IN    OMX_INDEXTYPE  nIndex,
                                 OMX_INOUT OMX_PTR        pParam)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pParam);

    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    CHECK_STATE_INVALID(pEnc->state);
    DBGT_PDEBUG("API: Get config index: %s", HantroOmx_str_omx_index(nIndex));

    switch ((OMX_U32)nIndex)
    {
        case OMX_IndexConfigCommonRotate:
            {
                OMX_CONFIG_ROTATIONTYPE* param = (OMX_CONFIG_ROTATIONTYPE*) pParam;
                CHECK_PORT_INPUT(param);
                memcpy(param, &pEnc->encConfig.rotation, param->nSize);
            }
            break;
        case OMX_IndexConfigCommonInputCrop:
            {
                OMX_CONFIG_RECTTYPE* param = (OMX_CONFIG_RECTTYPE*) pParam;
                CHECK_PORT_INPUT(param);
                memcpy(param, &pEnc->encConfig.crop, param->nSize);
            }
            break;

#ifdef OMX_ENCODER_VIDEO_DOMAIN
        case OMX_IndexConfigVideoFramerate:
            {
                OMX_CONFIG_FRAMERATETYPE* param = (OMX_CONFIG_FRAMERATETYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                param->xEncodeFramerate = pEnc->outputPort.def.format.video.xFramerate;
            }
            break;
        case OMX_IndexConfigVideoBitrate:
            {
                OMX_VIDEO_CONFIG_BITRATETYPE* param = (OMX_VIDEO_CONFIG_BITRATETYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                param->nEncodeBitrate = pEnc->outputPort.def.format.video.nBitrate;
            }
            break;
        case OMX_IndexConfigCommonFrameStabilisation:
            {
                OMX_CONFIG_FRAMESTABTYPE* param = (OMX_CONFIG_FRAMESTABTYPE*) pParam;
                CHECK_PORT_INPUT(param);
                memcpy(param, &pEnc->encConfig.stab, param->nSize);
            }
            break;
#ifdef ENCH1
        case OMX_IndexConfigVideoVp8ReferenceFrame:
            {
                OMX_VIDEO_VP8REFERENCEFRAMETYPE* param = (OMX_VIDEO_VP8REFERENCEFRAMETYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.vp8Ref, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoAdaptiveRoi:
            {
                OMX_VIDEO_CONFIG_ADAPTIVEROITYPE* param = (OMX_VIDEO_CONFIG_ADAPTIVEROITYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.adaptiveRoi, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoVp8TemporalLayers:
            {
                OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE* param = (OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.temporalLayer, param->nSize);
            }
            break;
#endif /* ENCH1 */
        case OMX_IndexConfigVideoIntraVOPRefresh:
            {
                OMX_CONFIG_INTRAREFRESHVOPTYPE* param = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.intraRefreshVop, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoAVCIntraPeriod:
            {
                OMX_VIDEO_CONFIG_AVCINTRAPERIOD* param = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.avcIdr, param->nSize);
            }
            break;
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
        case OMX_IndexConfigVideoIntraArea:
            {
                OMX_VIDEO_CONFIG_INTRAAREATYPE* param = (OMX_VIDEO_CONFIG_INTRAAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                memcpy(param, &pEnc->encConfig.intraArea, param->nSize);
            }
            break;
        case OMX_IndexConfigVideoIPCMArea:
            {
                OMX_VIDEO_CONFIG_IPCMAREATYPE* param = (OMX_VIDEO_CONFIG_IPCMAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_ROI_AREA)
                    memcpy(param, &pEnc->encConfig.ipcmArea[idx], param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown ROI area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        case OMX_IndexConfigVideoRoiArea:
            {
                OMX_VIDEO_CONFIG_ROIAREATYPE* param = (OMX_VIDEO_CONFIG_ROIAREATYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_ROI_AREA)
                    memcpy(param, &pEnc->encConfig.roiArea[idx], param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown ROI area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        case OMX_IndexConfigVideoRoiDeltaQp:
            {
                OMX_VIDEO_CONFIG_ROIDELTAQPTYPE* param = (OMX_VIDEO_CONFIG_ROIDELTAQPTYPE*) pParam;
                CHECK_PORT_OUTPUT(param);
                OMX_U32 idx = param->nArea - 1;
                if (idx < MAX_ROI_DELTAQP)
                    memcpy(param, &pEnc->encConfig.roiDeltaQP[idx], param->nSize);
                else
                {
                    DBGT_CRITICAL("Unknown ROI area: %d", (int)param->nArea);
                    DBGT_EPILOG("");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
#endif
#endif /* OMX_ENCODER_VIDEO_DOMAIN */

        default:
        DBGT_CRITICAL("Bad index (%x)", nIndex);
        DBGT_EPILOG("");
        return OMX_ErrorUnsupportedIndex;
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}



/**
 * static OMX_ERRORTYPE encoder_push_buffer(OMX_HANDLETYPE hComponent,
                                  OMX_BUFFERHEADERTYPE* pBufferHeader,
                                  OMX_U32 portindex)
 * @param OMX_BUFFERHEADERTYPE* pBufferHeader - Buffer to be
 */
static
OMX_ERRORTYPE encoder_push_buffer(OMX_HANDLETYPE hComponent,
                                  OMX_BUFFERHEADERTYPE* pBufferHeader,
                                  OMX_U32 portindex)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pBufferHeader);

    OMX_ENCODER* pEnc  = GET_ENCODER(hComponent);

    CHECK_STATE_INVALID(pEnc->state);
    DBGT_PDEBUG("API: header:%p port index:%u", pBufferHeader, (unsigned)portindex);

    if (pEnc->state != OMX_StateExecuting && pEnc->state != OMX_StatePause)
    {
        DBGT_CRITICAL("API: incorrect encoder state: %s", HantroOmx_str_omx_state(pEnc->state));
        DBGT_EPILOG("");
        return OMX_ErrorIncorrectStateOperation;
    }

    PORT* port = encoder_map_index_to_port(pEnc, portindex);
    if (!port)
    {
        DBGT_CRITICAL("API: no such port %u",(unsigned)portindex);
        DBGT_EPILOG("");
        return OMX_ErrorBadPortIndex;
    }

    // In case of a tunneled port the client will request to disable a port on the buffer supplier,
    // and then on the non-supplier. The non-supplier needs to be able to return the supplied
    // buffer to our queue. So in this case this function will be invoked on a disabled port.
    // Then on the other hand the conformance tester (PortCommunicationTest) tests to see that
    // when a port is disabled it returns an appropriate error when a buffer is sent to it.
    //
    // In IOP/PortDisableEnable test tester disables all of our ports. Then destroys the TTC
    // and creates a new TTC. The new TTC is told to allocate buffers for our output port.
    // The the tester tells the TTC to transit to Executing state, at which point it is trying to
    // initiate buffer trafficing by calling our FillThisBuffer. However at this point the
    // port is still disabled.
    //
    if (!HantroOmx_port_is_tunneled(port))
    {
        if (!port->def.bEnabled)
        {
            DBGT_CRITICAL("API: port is disabled");
            DBGT_EPILOG("");
            return OMX_ErrorIncorrectStateOperation;
        }
    }

    // Lock the port's buffer queue here.
    OMX_ERRORTYPE err = HantroOmx_port_lock_buffers(port);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("API: failed to lock port: %s",
                    HantroOmx_str_omx_err(err));
        DBGT_EPILOG("");
        return err;
    }

    BUFFER* buff = HantroOmx_port_find_buffer(port, pBufferHeader);
    if (!buff)
    {
        HantroOmx_port_unlock_buffers(port);
        DBGT_CRITICAL("API: no such buffer");
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }

    //re-ordering management
    if(portindex == PORT_INDEX_INPUT)
    {
        buff->frame_id = pEnc->in_frame_id++;
    }

    err = HantroOmx_port_push_buffer(port, buff);
    if (err != OMX_ErrorNone)
        DBGT_CRITICAL("API: failed to queue buffer: %s", HantroOmx_str_omx_err(err));

    if (portindex == PORT_INDEX_INPUT && pBufferHeader->nFlags & OMX_BUFFERFLAG_EOS)
    {
        pEnc->input_eos = OMX_TRUE;
    }

    // remember to unlock the queue too
    HantroOmx_port_unlock_buffers(port);
    DBGT_EPILOG("");
    return err;
}

static
OMX_ERRORTYPE encoder_fill_this_buffer(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    // this function gives us a databuffer encoded data is stored
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pBufferHeader);

    // OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    //DBGT_PDEBUG("API: nOutputPortIndex %u", (unsigned) pBufferHeader->nOutputPortIndex);
    //DBGT_PDEBUG("API: nInputPortIndex %u", (unsigned)pBufferHeader->nInputPortIndex);
    DBGT_PDEBUG("API: pBufferHeader %p", pBufferHeader);

    if (pBufferHeader->nSize != sizeof(OMX_BUFFERHEADERTYPE))
    {
        DBGT_CRITICAL("API: buffer header size mismatch");
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }
    DBGT_EPILOG("");
    return encoder_push_buffer(hComponent, pBufferHeader, pBufferHeader->nOutputPortIndex);
}

static
OMX_ERRORTYPE encoder_empty_this_buffer(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    CHECK_PARAM_NON_NULL(pBufferHeader);

    // OMX_ENCODER* pEnc = GET_ENCODER(hComponent);

    if (pBufferHeader->nFilledLen > pBufferHeader->nAllocLen)
    {
        DBGT_CRITICAL("API: incorrect nFilledLen value: %u nAllocLen: %u",
            (unsigned)pBufferHeader->nFilledLen,
            (unsigned)pBufferHeader->nAllocLen);
            DBGT_EPILOG("");
            return OMX_ErrorBadParameter;
    }

    if (pBufferHeader->nSize != sizeof(OMX_BUFFERHEADERTYPE))
    {
        DBGT_CRITICAL("API: buffer header size mismatch");
        DBGT_EPILOG("");
        return OMX_ErrorBadParameter;
    }

    DBGT_PDEBUG("API: pBufferHeader %p, nFilledLen:%u, nFlags:%x", pBufferHeader,
        (unsigned)pBufferHeader->nFilledLen, (unsigned)pBufferHeader->nFlags);
    DBGT_EPILOG("");
    return encoder_push_buffer(hComponent, pBufferHeader, pBufferHeader->nInputPortIndex);
}

static
OMX_ERRORTYPE encoder_component_tunnel_request(OMX_IN OMX_HANDLETYPE hComponent,
                                               OMX_IN OMX_U32        nPort,
                                               OMX_IN OMX_HANDLETYPE hTunneledComp,
                                               OMX_IN OMX_U32        nTunneledPort,
                                               OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    DBGT_PROLOG("");
    CHECK_PARAM_NON_NULL(hComponent);
    OMX_ENCODER* pEnc = GET_ENCODER(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    PORT* port = encoder_map_index_to_port(pEnc, nPort);
    if (port == NULL)
    {
        DBGT_CRITICAL("API: bad port index:%d", (int) nPort);
        DBGT_EPILOG("");
        return OMX_ErrorBadPortIndex;
    }
    if (pEnc->state != OMX_StateLoaded && port->def.bEnabled)
    {
        DBGT_CRITICAL("API: port is not disabled");
        DBGT_EPILOG("");
        return OMX_ErrorIncorrectStateOperation;
    }

    DBGT_PDEBUG("API: setting up tunnel on port: %d", (int) nPort);
    DBGT_PDEBUG("API: tunnel component:%p tunnel port:%d",
                hTunneledComp, (int) nTunneledPort);

    if (hTunneledComp == NULL)
    {
        HantroOmx_port_setup_tunnel(port, NULL, 0, OMX_BufferSupplyUnspecified);
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

#ifndef OMX_ENCODER_TUNNELING_SUPPORT
    DBGT_CRITICAL("API: ERROR Tunneling unsupported");
    return OMX_ErrorTunnelingUnsupported;
#endif

    CHECK_PARAM_NON_NULL(pTunnelSetup);
    if (port->def.eDir == OMX_DirOutput)
    {
        // 3.3.11
        // the component that provides the output port of the tunneling has to do the following:
        // 1. Indicate its supplier preference in pTunnelSetup.
        // 2. Set the OMX_PORTTUNNELFLAG_READONLY flag to indicate that buffers
        //    from this output port are read-only and that the buffers cannot be shared
        //    through components or modified.

        // do not overwrite if something has been specified with SetParameter
        if (port->tunnel.eSupplier == OMX_BufferSupplyUnspecified)
            port->tunnel.eSupplier = OMX_BufferSupplyOutput;

        // if the component that provides the input port
        // wants to override the buffer supplier setting it will call our SetParameter
        // to override the setting put here.
        pTunnelSetup->eSupplier    = port->tunnel.eSupplier;
        pTunnelSetup->nTunnelFlags = OMX_PORTTUNNELFLAG_READONLY;
        HantroOmx_port_setup_tunnel(port, hTunneledComp, nTunneledPort, port->tunnel.eSupplier);
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }
    else
    {
        // the input port is responsible for checking that the
        // ports are compatible
        // so get the portdefinition from the output port
        OMX_PARAM_PORTDEFINITIONTYPE other;
        memset(&other, 0, sizeof(other));
        INIT_OMX_VERSION_PARAM(other);
        other.nPortIndex = nTunneledPort;
        err = ((OMX_COMPONENTTYPE*)hTunneledComp)->GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &other);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("GetParameter failed (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        // next do the port compatibility checking
        if (port->def.eDomain != other.eDomain)
        {
            DBGT_CRITICAL("API: ports are not compatible (incompatible domains)");
            DBGT_EPILOG("");
        }
        if (port == &pEnc->inputPort)
        {
#ifdef OMX_ENCODER_VIDEO_DOMAIN
            switch (other.format.video.eCompressionFormat)
            {
                case OMX_VIDEO_CodingUnused:
                    switch ((int)other.format.video.eColorFormat)
                    {
						case OMX_COLOR_FormatYUV420Planar:			 break;
						case OMX_COLOR_FormatYUV420PackedPlanar:	 break;
						case OMX_COLOR_FormatYUV420SemiPlanar:		 break;
						case OMX_COLOR_FormatYUV420PackedSemiPlanar: break;
						case OMX_COLOR_FormatYCbYCr:				 break;
						case OMX_COLOR_FormatCbYCrY:				 break;
						case OMX_COLOR_Format16bitRGB565:			 break;
						case OMX_COLOR_Format16bitBGR565:			 break;
						case OMX_COLOR_Format16bitARGB4444: 		 break;
						case OMX_COLOR_Format12bitRGB444:			 break;
#if defined (ENC6280) || defined (ENC7280)
						case OMX_COLOR_FormatYUV422Planar:			 break;
#else
						case OMX_COLOR_Format25bitARGB1888: 		 break;
						case OMX_COLOR_Format32bitARGB8888: 		 break;
#endif
#ifdef ENCVC8000E
						case OMX_COLOR_FormatYUV420SemiPlanarP010:   break;
#endif
						case OMX_COLOR_FormatYUV420SemiPlanarVU:     break;
						case OMX_COLOR_Format16bitBGR555:            break;
						case OMX_COLOR_Format16bitARGB1555: 		 break;
                        default:
                            DBGT_CRITICAL("API: ports are not compatible (incompatible color format)");
                            DBGT_EPILOG("");
                            return OMX_ErrorPortsNotCompatible;
                    }
                    break;
                default:
                    DBGT_CRITICAL("API: ports are not compatible (incompatible video coding)");
                    DBGT_EPILOG("");
                    return OMX_ErrorPortsNotCompatible;
            }
#endif //OMX_ENCODER_VIDEO_DOMAIN

#ifdef OMX_ENCODER_IMAGE_DOMAIN
            switch (other.format.image.eCompressionFormat)
            {
                case OMX_IMAGE_CodingUnused:
                    switch  (other.format.image.eColorFormat)
                    {
                    	case OMX_COLOR_FormatYUV420Planar:           break;
                        case OMX_COLOR_FormatYUV420PackedPlanar:     break;
						case OMX_COLOR_FormatYUV420SemiPlanar:       break;
                        case OMX_COLOR_FormatYUV420PackedSemiPlanar: break;
                        case OMX_COLOR_FormatYCbYCr:                 break;
						case OMX_COLOR_FormatCbYCrY:                 break;
						case OMX_COLOR_Format16bitRGB565:            break;
						case OMX_COLOR_Format16bitBGR565:            break;
						case OMX_COLOR_Format16bitARGB4444:          break;
						case OMX_COLOR_Format12bitRGB444:            break;
#if defined (ENC6280) || defined (ENC7280)
						case OMX_COLOR_FormatYUV422Planar:           break;
#else
						case OMX_COLOR_Format25bitARGB1888:          break;
						case OMX_COLOR_Format32bitARGB8888:          break;
#endif
						case OMX_COLOR_Format16bitARGB1555:          break;
                        default:
                            DBGT_CRITICAL("API: ports are not compatible (incompatible color format)");
                            DBGT_EPILOG("");
                            return OMX_ErrorPortsNotCompatible;
                    }
                    break;
                default:
                    DBGT_CRITICAL("ASYNC: ports are not compatible (incompatible image coding)");
                    DBGT_EPILOG("");
                    return OMX_ErrorPortsNotCompatible;
            }
#endif //OMX_ENCODER_IMAGE_DOMAIN
        }
        if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified)
            pTunnelSetup->eSupplier = OMX_BufferSupplyInput;

        // need to send back the result of the buffer supply negotiation
        // to the component providing the output port.
        OMX_PARAM_BUFFERSUPPLIERTYPE param;
        memset(&param, 0, sizeof(param));
        INIT_OMX_VERSION_PARAM(param);

        param.eBufferSupplier = pTunnelSetup->eSupplier;
        param.nPortIndex      = nTunneledPort;
        err = ((OMX_COMPONENTTYPE*)hTunneledComp)->SetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &param);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("SetParameter failed (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        // save tunneling crap somewhere
        HantroOmx_port_setup_tunnel(port, hTunneledComp, nTunneledPort, pTunnelSetup->eSupplier);
        DBGT_PDEBUG("API: tunnel supplier: %s", HantroOmx_str_omx_supplier(pTunnelSetup->eSupplier));
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#ifdef ANDROID
OMX_ERRORTYPE encoder_get_extension_index(OMX_HANDLETYPE hComponent, OMX_STRING cParameterName, OMX_INDEXTYPE* pIndexType)
{
    UNUSED_PARAMETER(hComponent);

    DBGT_PROLOG("");
    DBGT_PDEBUG("API: GetIndexExtension %s", cParameterName);

    if (strncmp(cParameterName, "OMX.google.android.index.prependSPSPPSToIDRFrames", 49) == 0)
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_google_android_index_prependSPSPPSToIDRFrames;

        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

    DBGT_EPILOG("");
    return OMX_ErrorUnsupportedIndex;
}
#else
OMX_ERRORTYPE encoder_get_extension_index(OMX_HANDLETYPE hComponent, OMX_STRING cParameterName, OMX_INDEXTYPE* pIndexType)
{
    UNUSED_PARAMETER(hComponent);
    UNUSED_PARAMETER(cParameterName);
    UNUSED_PARAMETER(pIndexType);

    DBGT_ERROR("API: extensions not supported");
    return OMX_ErrorNotImplemented;
}
#endif

OMX_ERRORTYPE encoder_useegl_image(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, void* pBuffer)
{
    UNUSED_PARAMETER(hComponent);
    UNUSED_PARAMETER(ppBufferHdr);
    UNUSED_PARAMETER(nPortIndex);
    UNUSED_PARAMETER(pAppPrivate);
    UNUSED_PARAMETER(pBuffer);

    DBGT_ERROR("API: Use egl image not implemented");
    return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE encoder_enum_roles(OMX_HANDLETYPE hComponent, OMX_U8 *cRole, OMX_U32 nIndex)
{
    UNUSED_PARAMETER(hComponent);
    UNUSED_PARAMETER(cRole);
    UNUSED_PARAMETER(nIndex);

    DBGT_ERROR("API: enum roles not implemented");
    return OMX_ErrorNotImplemented;
}


/**
 * OMX_ERRORTYPE encoder_init(OMX_HANDLETYPE hComponent)
 * Initializes encoder component
 * constructor
 */
OMX_ERRORTYPE HantroHwEncOmx_encoder_init(OMX_HANDLETYPE hComponent)
{
#ifdef ENC7280
    DBGT_TRACE_INIT(7280enc);
#endif
#ifdef ENC8270
    DBGT_TRACE_INIT(8270enc);
#endif
#ifdef ENC8290
    DBGT_TRACE_INIT(8290enc);
#endif
#ifdef ENCH1
    DBGT_TRACE_INIT(h1enc);
#endif
#ifdef ENCH2
    DBGT_TRACE_INIT(h2enc);
#endif
#ifdef ENCVC8000E
    DBGT_TRACE_INIT(vc8000e);
#endif
#ifdef ENCH2V41
    DBGT_TRACE_INIT(h2v41);
#endif


    CHECK_PARAM_NON_NULL(hComponent);

    OMX_ERRORTYPE err;
    OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
    int i;

    // work around an issue in the conformance tester.
    // In the ResourceExhaustion test the conformance tester tries to create
    // components more than the system can handle. However if it is the constructor
    // that fails instead of the Idle state transition,
    // (the component is never actually created but only the handle IS created)
    // the tester passes the component to the core for cleaning up which then just ends up calling the DeInit function
    // on an object that hasn't ever even been constructed.
    // To work around this problem we set the DeInit function pointer here and then in the DeInit function
    // check if the pComponentPrivate is set to a non NULL value. (i.e. has the object really been created). Lame.
    //
    pComp->ComponentDeInit = encoder_deinit;

    OMX_ENCODER* pEnc = (OMX_ENCODER*) OSAL_Malloc(sizeof(OMX_ENCODER));
    if (pEnc == 0)
    {
        DBGT_CRITICAL("OSAL_Malloc failed");
        return OMX_ErrorInsufficientResources;
    }

    DBGT_PDEBUG("Component version: %d.%d.%d.%d",
             COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR,
             COMPONENT_VERSION_REVISION, COMPONENT_VERSION_STEP);

    memset(pEnc, 0, sizeof(OMX_ENCODER));

    pEnc->enc_next_pic_id = 0xFFFFFFFF;
    pEnc->in_frame_id = 0;
    pEnc->out_frame_id = 0;

    err = OSAL_MutexCreate(&pEnc->statemutex);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("OSAL_MutexCreate failed (err=%x)", err);
        goto INIT_FAILURE;
    }

    err = HantroOmx_port_init(&pEnc->inputPort, 9, 9, 16, DEFAULT_INPUT_BUFFER_SIZE);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_init (in) failed (err=%x)", err);
        goto INIT_FAILURE;
    }

    err = HantroOmx_port_init(&pEnc->outputPort, 4, 4, 5, DEFAULT_OUTPUT_BUFFER_SIZE);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_init (out) failed (err=%x)", err);
        goto INIT_FAILURE;
    }

    err = OSAL_AllocatorInit(&pEnc->alloc);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("OSAL_AllocatorInit failed (err=%x)", err);
        goto INIT_FAILURE;
    }

    INIT_OMX_VERSION_PARAM(pEnc->ports);
    INIT_OMX_VERSION_PARAM(pEnc->inputPort.def);
    INIT_OMX_VERSION_PARAM(pEnc->outputPort.def);

    pEnc->ports.nPorts                  = 2;
    pEnc->ports.nStartPortNumber        = 0;
    pEnc->state                         = OMX_StateLoaded;
    pEnc->statetrans                    = OMX_StateLoaded;
    pEnc->run                           = OMX_TRUE;
    pEnc->self                          = pComp;
    pEnc->streamStarted                 = OMX_FALSE;
    pEnc->perfStarted                   = OMX_FALSE;
    pEnc->min_time                      = 0;
    pEnc->max_time                      = 0;

#ifdef OMX_ENCODER_VIDEO_DOMAIN
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.avc);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.avcIdr);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.deblocking);
#if defined (ENC6280) || defined (ENC7280)
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.h263);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.mpeg4);
#endif
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.ec);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.bitrate);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.stab);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.videoQuantization);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.rotation);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.crop);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.intraRefreshVop);
#ifdef ENCH1
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.vp8);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.vp8Ref);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.adaptiveRoi);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.temporalLayer);
#endif
#if defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.hevc);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.avcExt);
#endif

#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.intraArea);
    for(i=0; i<MAX_ROI_AREA; i++)
    {
        INIT_OMX_VERSION_PARAM(pEnc->encConfig.roiArea[i]);
    }
    for(i=0; i<MAX_ROI_DELTAQP; i++)
    {
        INIT_OMX_VERSION_PARAM(pEnc->encConfig.roiDeltaQP[i]);
    }
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.intraRefresh);
#endif

    //Input port
    pEnc->inputPort.def.nPortIndex          = PORT_INDEX_INPUT;
    pEnc->inputPort.def.eDir                = OMX_DirInput;
    pEnc->inputPort.def.bEnabled            = OMX_TRUE;
    pEnc->inputPort.def.bPopulated          = OMX_FALSE;
    pEnc->inputPort.def.eDomain             = OMX_PortDomainVideo;
    pEnc->inputPort.def.format.video.xFramerate         = FLOAT_Q16(30);
    pEnc->inputPort.def.format.video.nFrameWidth        = 176;
    pEnc->inputPort.def.format.video.nFrameHeight       = 144;
    pEnc->inputPort.def.format.video.nStride            = 176;
    pEnc->inputPort.def.format.video.nSliceHeight       = 144;
    pEnc->inputPort.def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pEnc->inputPort.def.format.video.eColorFormat       = OMX_COLOR_FormatYUV420PackedPlanar;

    //Output port (Use H264 as an default)
    strcpy((char*)pEnc->role, "video_encoder.avc");
    pEnc->outputPort.def.nPortIndex                         = PORT_INDEX_OUTPUT;
    pEnc->outputPort.def.eDir                               = OMX_DirOutput;
    pEnc->outputPort.def.bEnabled                           = OMX_TRUE;
    pEnc->outputPort.def.bPopulated                         = OMX_FALSE;
    pEnc->outputPort.def.eDomain                            = OMX_PortDomainVideo;
    pEnc->outputPort.def.format.video.nBitrate              = 128000;
    pEnc->outputPort.def.format.video.xFramerate            = FLOAT_Q16(30);
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
    pEnc->outputPort.def.format.video.eCompressionFormat    = OMX_VIDEO_CodingAVC;
#else
    pEnc->outputPort.def.format.video.eCompressionFormat    = OMX_VIDEO_CodingHEVC;
#endif
    pEnc->outputPort.def.format.video.eColorFormat          = OMX_COLOR_FormatUnused;
    pEnc->outputPort.def.format.video.nFrameWidth           = 176;
    pEnc->outputPort.def.format.video.nFrameHeight          = 144;

#ifdef CONFORMANCE
    pEnc->inputPort.def.format.video.eColorFormat       = OMX_COLOR_FormatYUV420Planar;
    pEnc->outputPort.def.format.video.xFramerate        = 15 << 16;
    pEnc->outputPort.def.format.video.nBitrate          = 64000;
#endif

#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
    pEnc->encConfig.intraArea.nTop      = -1;
    pEnc->encConfig.intraArea.nLeft     = -1;
    pEnc->encConfig.intraArea.nBottom   = -1;
    pEnc->encConfig.intraArea.nRight    = -1;

    for(i=0; i<MAX_ROI_AREA; i++)
    {
        pEnc->encConfig.roiArea[i].nTop       = 0;
        pEnc->encConfig.roiArea[i].nLeft      = 0;
        pEnc->encConfig.roiArea[i].nBottom    = 0;
        pEnc->encConfig.roiArea[i].nRight     = 0;
    }
#endif
#endif

#ifdef OMX_ENCODER_IMAGE_DOMAIN
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.imageQuantization);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.rotation);
    INIT_OMX_VERSION_PARAM(pEnc->encConfig.crop);

    pEnc->inputPort.def.nPortIndex          = PORT_INDEX_INPUT;
    pEnc->inputPort.def.eDir                = OMX_DirInput;
    pEnc->inputPort.def.bEnabled            = OMX_TRUE;
    pEnc->inputPort.def.bPopulated          = OMX_FALSE;
    pEnc->inputPort.def.eDomain             = OMX_PortDomainImage;
    pEnc->inputPort.def.format.image.nFrameWidth        = 96;
    pEnc->inputPort.def.format.image.nFrameHeight       = 96;
    pEnc->inputPort.def.format.image.nStride            = 96;
    pEnc->inputPort.def.format.image.nSliceHeight       = 16;
    pEnc->inputPort.def.format.image.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pEnc->inputPort.def.format.image.eColorFormat       = OMX_COLOR_FormatYUV420PackedPlanar;
    pEnc->inputPort.def.format.image.bFlagErrorConcealment  = OMX_FALSE;
    pEnc->inputPort.def.format.image.pNativeWindow          = 0x0;

    //Output port (Use Jpeg as an default)
#ifdef ENABLE_JPEG
    strcpy((char*)pEnc->role, "image_encoder.jpeg");
    pEnc->outputPort.def.format.image.eCompressionFormat    = OMX_IMAGE_CodingJPEG;
#else
    strcpy((char*)pEnc->role, "image_encoder.webp");
    pEnc->outputPort.def.format.image.eCompressionFormat    = OMX_IMAGE_CodingWEBP;
#endif
    pEnc->outputPort.def.nPortIndex                         = PORT_INDEX_OUTPUT;
    pEnc->outputPort.def.eDir                               = OMX_DirOutput;
    pEnc->outputPort.def.bEnabled                           = OMX_TRUE;
    pEnc->outputPort.def.bPopulated                         = OMX_FALSE;
    pEnc->outputPort.def.eDomain                            = OMX_PortDomainImage;
    pEnc->outputPort.def.format.image.pNativeRender         = 0;
    pEnc->outputPort.def.format.image.nFrameWidth           = 96;
    pEnc->outputPort.def.format.image.nFrameHeight          = 96;
    pEnc->outputPort.def.format.image.nStride               = 96;
    pEnc->outputPort.def.format.image.nSliceHeight          = 16; //No slices
    pEnc->outputPort.def.format.image.bFlagErrorConcealment = OMX_FALSE;
    pEnc->outputPort.def.format.image.eColorFormat          = OMX_COLOR_FormatUnused;
    pEnc->outputPort.def.format.image.pNativeWindow         = 0x0;

    pEnc->numOfSlices                   = 0;
    pEnc->sliceNum                      = 1;
#ifdef CONFORMANCE
    pEnc->inputPort.def.format.image.nFrameWidth        = 640;
    pEnc->inputPort.def.format.image.nFrameHeight       = 480;
    pEnc->inputPort.def.format.image.nStride            = 640;
    pEnc->inputPort.def.format.image.eColorFormat       = OMX_COLOR_FormatYUV420Planar;
    pEnc->outputPort.def.format.image.nFrameWidth           = 640;
    pEnc->outputPort.def.format.image.nFrameHeight          = 480;
    pEnc->outputPort.def.format.image.nStride               = 640;
    pEnc->outputPort.def.format.image.nSliceHeight          = 0;
#endif

#endif

    //Set up function pointers for component interface functions
    pComp->GetComponentVersion   = encoder_get_version;
    pComp->SendCommand           = encoder_send_command;
    pComp->GetParameter          = encoder_get_parameter;
    pComp->SetParameter          = encoder_set_parameter;
    pComp->SetCallbacks          = encoder_set_callbacks;
    pComp->GetConfig             = encoder_get_config;
    pComp->SetConfig             = encoder_set_config;
    pComp->GetExtensionIndex     = encoder_get_extension_index;
    pComp->GetState              = encoder_get_state;
    pComp->ComponentTunnelRequest= encoder_component_tunnel_request;
    pComp->UseBuffer             = encoder_use_buffer;
    pComp->AllocateBuffer        = encoder_allocate_buffer;
    pComp->FreeBuffer            = encoder_free_buffer;
    pComp->EmptyThisBuffer       = encoder_empty_this_buffer;
    pComp->FillThisBuffer        = encoder_fill_this_buffer;
    pComp->UseEGLImage           = encoder_useegl_image;
    pComp->ComponentRoleEnum     = encoder_enum_roles;
    // save the "this" pointer into component struct for later use
    pComp->pComponentPrivate     = pEnc;

    pEnc->frameCounter = 0 ;

#ifdef OMX_ENCODER_VIDEO_DOMAIN
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
    err = set_avc_defaults(pEnc);
#else
#ifdef ENCH2
    err = set_hevc_defaults(pEnc);
#endif

#ifdef ENCVC8000E
    err = set_vc8000e_hevc_defaults(pEnc);
#endif

#ifdef ENCH2V41
    err = set_h2v41_hevc_defaults(pEnc);
#endif

#endif
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;

    err = set_preprocessor_defaults(pEnc);
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;

    err = set_bitrate_defaults(pEnc);
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;
#endif //~OMX_ENCODER_VIDEO DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN
#ifdef ENABLE_JPEG
    err = set_jpeg_defaults(pEnc);
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;
#else
    err = set_webp_defaults(pEnc);
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;
#endif
    err = set_preprocessor_defaults(pEnc);
    if (err != OMX_ErrorNone)
        goto INIT_FAILURE;
#endif //~OMX_ENCODER_IMAGE DOMAIN

    err = HantroOmx_basecomp_init(&pEnc->base, encoder_thread_main, pEnc);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("basecomp_init failed");
        goto INIT_FAILURE;
    }

    return OMX_ErrorNone;

 INIT_FAILURE:
    DBGT_ASSERT(pEnc);
    DBGT_CRITICAL("%s %s", "init failure", HantroOmx_str_omx_err(err));

    if (pEnc->statemutex)
        OSAL_MutexDestroy(pEnc->statemutex);

#ifndef CONFORMANCE // conformance tester calls deinit
    if (HantroOmx_port_is_allocated(&pEnc->inputPort))
        HantroOmx_port_destroy(&pEnc->inputPort);
    if (HantroOmx_port_is_allocated(&pEnc->outputPort))
        HantroOmx_port_destroy(&pEnc->outputPort);
    if (OSAL_AllocatorIsReady(&pEnc->alloc))
        OSAL_AllocatorDestroy(&pEnc->alloc);
    OSAL_Free(pEnc);
#endif
    return err;
}

static
OMX_ERRORTYPE supply_tunneled_port(OMX_ENCODER* pEnc, PORT* port)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(port->tunnelcomp);

    DBGT_PDEBUG("ASYNC: supplying buffers for: %p (%d)", port->tunnelcomp, (int)port->tunnelport);

    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE param;
    memset(&param, 0, sizeof(param));
    INIT_OMX_VERSION_PARAM(param);
    param.nPortIndex = port->tunnelport;
    // get the port definition, cause we need the number of buffers
    // that we need to allocate for this port
    err = ((OMX_COMPONENTTYPE*)port->tunnelcomp)->GetParameter(port->tunnelcomp, OMX_IndexParamPortDefinition, &param);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("GetParameter failed (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }

    // this operation should be fine without locking.
    // there's no access to the supply queue through the public API,
    // so the component thread is the only thread doing thea access here.

    // 2.1.7.2
    // 2. Allocate buffers according to the maximum of its own requirements and the
    //    requirements of the tunneled port.

    OMX_U32 count = param.nBufferCountActual > port->def.nBufferCountActual ? param.nBufferCountActual : port->def.nBufferCountActual;
    OMX_U32 size  = param.nBufferSize        > port->def.nBufferSize        ? param.nBufferSize        : port->def.nBufferSize;
    DBGT_PDEBUG("ASYNC: allocating %d buffers", (int)count);

    OMX_U32 i=0;
    for (i=0; i<count; ++i)
    {
        OMX_U8*        bus_data    = NULL;
        OSAL_BUS_WIDTH bus_address = 0;
        OMX_U32        allocsize   = size;
        // allocate the memory chunk for the buffer
        err = OSAL_AllocatorAllocMem(&pEnc->alloc, &allocsize, &bus_data, &bus_address);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("ASYNC: failed to supply buffer (%d bytes)", (int)param.nBufferSize);
            goto FAIL;
        }

        // allocate the BUFFER object
        BUFFER* buff = NULL;
        HantroOmx_port_allocate_next_buffer(port, &buff);
        if (buff == NULL)
        {
            DBGT_CRITICAL("ASYNC: failed to supply buffer object");
            OSAL_AllocatorFreeMem(&pEnc->alloc, allocsize, bus_data, bus_address);
            goto FAIL;
        }
        buff->flags               |= BUFFER_FLAG_MY_BUFFER;
        buff->bus_data            = bus_data;
        buff->bus_address         = bus_address;
        buff->allocsize           = allocsize;
        buff->header->pBuffer     = bus_data;
        buff->header->pAppPrivate = NULL;
        buff->header->nAllocLen   = size;
        // the header will remain empty because the
        // tunneled port allocates it.
        buff->header = NULL;
        err = ((OMX_COMPONENTTYPE*)port->tunnelcomp)->UseBuffer(
            port->tunnelcomp,
            &buff->header,
            port->tunnelport,
            NULL,
            allocsize,
            bus_data);

        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("ASYNC: use buffer call failed on tunneled component:%s",
                            HantroOmx_str_omx_err(err));
            goto FAIL;
        }
        // the tunneling component is responsible for allocating the
        // buffer header and filling in the buffer information.
        DBGT_ASSERT(buff->header);
        DBGT_ASSERT(buff->header != &buff->headerdata);
        DBGT_ASSERT(buff->header->nSize);
        DBGT_ASSERT(buff->header->nVersion.nVersion);
        DBGT_ASSERT(buff->header->nAllocLen);

        DBGT_PDEBUG("ASYNC: supplied buffer data virtual address:%p size:%d header:%p",
                    bus_data, (int)allocsize, buff->header);
    }
    DBGT_ASSERT(HantroOmx_port_buffer_count(port) >= port->def.nBufferCountActual);
    DBGT_PDEBUG("ASYNC: port is populated");
    port->def.bPopulated = OMX_TRUE;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
 FAIL:
    DBGT_PDEBUG("ASYNC: freeing already supplied buffers");
    // must free any buffers we have allocated
    count = HantroOmx_port_buffer_count(port);
    for (i=0; i<count; ++i)
    {
        BUFFER* buff = NULL;
        HantroOmx_port_get_allocated_buffer_at(port, &buff, i);
        DBGT_ASSERT(buff);
        DBGT_ASSERT(buff->bus_data);
        DBGT_ASSERT(buff->bus_address);

        if (buff->header)
            ((OMX_COMPONENTTYPE*)port->tunnelcomp)->FreeBuffer(port->tunnelcomp, port->tunnelport, buff->header);

        OSAL_AllocatorFreeMem(&pEnc->alloc, buff->allocsize, buff->bus_data, buff->bus_address);
    }
    HantroOmx_port_release_all_allocated(port);
    DBGT_EPILOG("");
    return err;
}

static
OMX_ERRORTYPE unsupply_tunneled_port(OMX_ENCODER* pEnc, PORT* port)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(port->tunnelcomp);

    DBGT_PDEBUG("ASYNC: removing buffers from: %p (%d)", port->tunnelcomp, (int)port->tunnelport);

    // tell the non-supplier to release them buffers
    OMX_U32 count = HantroOmx_port_buffer_count(port);
    OMX_U32 i;
    for (i=0; i<count; ++i)
    {
        BUFFER* buff = NULL;
        HantroOmx_port_get_allocated_buffer_at(port, &buff, i);
        DBGT_ASSERT(buff);
        DBGT_ASSERT(buff->bus_data);
        DBGT_ASSERT(buff->bus_address);
        DBGT_ASSERT(buff->header != &buff->headerdata);
        ((OMX_COMPONENTTYPE*)port->tunnelcomp)->FreeBuffer(port->tunnelcomp, port->tunnelport, buff->header);
        OSAL_AllocatorFreeMem(&pEnc->alloc, buff->allocsize, buff->bus_data, buff->bus_address);
    }
    HantroOmx_port_release_all_allocated(port);
    port->def.bPopulated = OMX_FALSE;

    // since we've allocated the buffers, and they have been
    // destroyed empty the port's buffer queue
    OMX_BOOL loop = OMX_TRUE;
    while (loop)
    {
        loop = HantroOmx_port_pop_buffer(port);
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


#ifdef OMX_ENCODER_VIDEO_DOMAIN
/**
 * OMX_U32 calculate_frame_size(OMX_ENCODER* pEnc)
 */
OMX_ERRORTYPE calculate_frame_size(OMX_ENCODER* pEnc, OMX_U32* frameSize)
{
    DBGT_PROLOG("");
    switch ((int)pEnc->inputPort.def.format.video.eColorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420PackedPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanarVU:
            *frameSize    =
            pEnc->inputPort.def.format.video.nFrameHeight *
            pEnc->inputPort.def.format.video.nStride * 3/2;
            break;
        case OMX_COLOR_Format16bitARGB4444:
        case OMX_COLOR_Format16bitARGB1555:
        case OMX_COLOR_Format12bitRGB444:
#if defined (ENC6280) || defined (ENC7280)
        case OMX_COLOR_FormatYUV422Planar:
#endif
        case OMX_COLOR_Format16bitRGB565:
        case OMX_COLOR_Format16bitBGR565:
        case OMX_COLOR_FormatCbYCrY:
        case OMX_COLOR_FormatYCbYCr: // YCbCr 4:2:2 interleaved (YUYV)
        case OMX_COLOR_Format16bitBGR555:
            *frameSize    =
            pEnc->inputPort.def.format.video.nFrameHeight *
            pEnc->inputPort.def.format.video.nStride * 2;
            break;
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2)|| defined (ENCVC8000E) || defined (ENCH2V41)
        /*case OMX_COLOR_Format24bitRGB888:
        case OMX_COLOR_Format24bitBGR888:*/
        case OMX_COLOR_Format25bitARGB1888:
        case OMX_COLOR_Format32bitARGB8888:
            *frameSize    =
            pEnc->inputPort.def.format.video.nFrameHeight *
            pEnc->inputPort.def.format.video.nStride * 4;
            break;
#endif
        default:
            DBGT_CRITICAL("ASYNC: unsupported color format");
            return OMX_ErrorUnsupportedSetting;
            break;
    }
    DBGT_PDEBUG("Frame (%dx%d) size: %d", (int)pEnc->inputPort.def.format.video.nStride,
                 (int)pEnc->inputPort.def.format.video.nFrameHeight, (int)*frameSize);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)

/**
 * static OMX_ERRORTYPE set_avc_defaults(OMX_ENCODER* pEnc);
 */
OMX_ERRORTYPE set_avc_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_AVCTYPE* config  =  0;
    config = &pEnc->encConfig.avc;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_AVCProfileBaseline;
    config->eLevel = OMX_VIDEO_AVCLevel3;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
    config->nRefFrames = 1;
    config->nAllowedPictureTypes &= OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    config->nPFrames = 150;
    config->nSliceHeaderSpacing = 0 ;
    config->nBFrames = 0;
    config->bUseHadamard = OMX_FALSE;
    config->nRefIdx10ActiveMinus1 = 0;
    config->nRefIdx11ActiveMinus1 = 0;
    config->bEnableUEP = OMX_FALSE;
    config->bEnableFMO = OMX_FALSE;
    config->bEnableASO = OMX_FALSE;
    config->bEnableRS = OMX_FALSE;
    config->bFrameMBsOnly = OMX_FALSE;
    config->bMBAFF = OMX_FALSE;
    config->bEntropyCodingCABAC = OMX_FALSE;
    config->bWeightedPPrediction = OMX_FALSE;
    config->nWeightedBipredicitonMode = OMX_FALSE;
    config->bconstIpred = OMX_FALSE;
    config->bDirect8x8Inference = OMX_FALSE;
    config->bDirectSpatialTemporal = OMX_FALSE;
    config->nCabacInitIdc = OMX_FALSE;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
#ifdef CONFORMANCE
    config->eLevel = OMX_VIDEO_AVCLevel1;
    config->bUseHadamard = OMX_TRUE;
#endif

    OMX_VIDEO_CONFIG_AVCINTRAPERIOD* avcIdr;
    avcIdr = &pEnc->encConfig.avcIdr;
    avcIdr->nPFrames = 150;
    avcIdr->nIDRPeriod = 150;

    OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
    deb = &pEnc->encConfig.deblocking;
    deb->nPortIndex = PORT_INDEX_OUTPUT;
    deb->bDeblocking = OMX_FALSE;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = 36;
    quantization->nQpP = 36;
    quantization->nQpB = 0; //Not used

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif

#ifdef ENCH2
OMX_ERRORTYPE set_hevc_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_HEVCTYPE* config  =  0;
    config = &pEnc->encConfig.hevc;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_HEVCProfileMain;
    config->eLevel = OMX_VIDEO_HEVCLevel5;
    config->nPFrames = 149;
    config->nBFrames = 0;
    config->nRefFrames = 1;
    config->nBitDepthLuma = 8;
    config->nBitDepthChroma = 8;
    config->bStrongIntraSmoothing = OMX_FALSE;
    config->nTcOffset = -2;
    config->nBetaOffset = 5;
    config->bEnableDeblockOverride = OMX_FALSE;
    config->bDeblockOverride = OMX_FALSE;
    config->bEnableSAO = OMX_TRUE;
    config->bEnableScalingList = OMX_FALSE;
    config->bCabacInitFlag = OMX_FALSE;

    OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
    deb = &pEnc->encConfig.deblocking;
    deb->nPortIndex = PORT_INDEX_OUTPUT;
    deb->bDeblocking = OMX_TRUE;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = 36;
    quantization->nQpP = 36;
    quantization->nQpB = 0; //Not used

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif

#ifdef ENCVC8000E
OMX_ERRORTYPE set_vc8000e_hevc_defaults(OMX_ENCODER* pEnc)
{
	DBGT_PROLOG("");
	int i;

	OMX_VIDEO_PARAM_HEVCTYPE* config  =  0;
	config = &pEnc->encConfig.hevc;
	config->nPortIndex = PORT_INDEX_OUTPUT;

	/* set default value for hevc parameters */
	config->eProfile = OMX_VIDEO_HEVCProfileMain;
	config->eLevel = OMX_VIDEO_HEVCLevel5;
	config->nPFrames = 149;
	config->nBFrames = 0;
	config->nRefFrames = 1;
	config->nBitDepthLuma = 8;
	config->nBitDepthChroma = 8;
	config->bStrongIntraSmoothing = OMX_FALSE;
	config->nTcOffset = -2;
	config->nBetaOffset = 5;
	config->bEnableDeblockOverride = OMX_FALSE;
	config->bDeblockOverride = OMX_FALSE;
	config->bEnableSAO = OMX_TRUE;

	config->ssim = 1;
	config->rdoLevel = 3;
	config->exp_of_input_alignment = 0;
#ifdef RECON_REF_1KB_BURST_RW
	config->exp_of_input_alignment = 10;
#endif
	config->ctbRc = VSI_DEFAULT_VALUE;
	config->gopSize = 0; //default of test-bench is 0, set to 1 for JLQ because it needs GOP size=1 only;
	config->intraPicRate = 0;
	config->cuInfoVersion = -1;
	config->gdrDuration = 0;
	config->roiMapDeltaQpBlockUnit = 0;
	config->roiMapDeltaQpEnable = 0;
	config->RoiQpDelta_ver = 1;
	config->byteStream = 1;
	config->sliceSize = 0;
	config->gopLowdelay = 0;
	config->enableCabac = 1;
	config->bCabacInitFlag = OMX_FALSE;
	config->videoRange = 0;
	config->enableRdoQuant = VSI_DEFAULT_VALUE;
	config->fieldOrder = 0;
	config->cirStart = 0;
	config->cirInterval = 0;
	config->pcm_loop_filter_disabled_flag = 0;
	config->ipcmMapEnable = 0;
	for(i=0; i<8; i++)
	{
		config->roiQp[i] = VSI_DEFAULT_VALUE;
	}
	config->bEnableScalingList = OMX_FALSE;
	config->chromaQpOffset = 0;
	config->noiseReductionEnable = 0;
	config->noiseLow = 10;
	config->noiseFirstFrameSigma = 11;

	config->num_tile_columns = 1;
	config->num_tile_rows = 1;
	config->loop_filter_across_tiles_enabled_flag = 1;

	/* HDR10 */
	config->hdr10_display_enable = 0;
	config->hdr10_dx0 = 0;
	config->hdr10_dy0 = 0;
	config->hdr10_dx1 = 0;
	config->hdr10_dy1 = 0;
	config->hdr10_dx2 = 0;
	config->hdr10_dy2 = 0;
	config->hdr10_wx  = 0;
	config->hdr10_wy  = 0;
	config->hdr10_maxluma = 0;
	config->hdr10_minluma = 0;

	config->hdr10_lightlevel_enable = 0;
	config->hdr10_maxlight			= 0;
	config->hdr10_avglight			= 0;

	config->hdr10_color_enable = 0;
	config->hdr10_primary  = 9;
	config->hdr10_transfer = 0; 
	config->hdr10_matrix   = 9;

	config->blockRCSize = VSI_DEFAULT_VALUE;
	config->rcQpDeltaRange = VSI_DEFAULT_VALUE;
	config->rcBaseMBComplexity = VSI_DEFAULT_VALUE;
	config->picQpDeltaMin = VSI_DEFAULT_VALUE;
	config->picQpDeltaMax = VSI_DEFAULT_VALUE;

	config->bitVarRangeI = 10000;
	config->bitVarRangeP = 10000;
	config->bitVarRangeB = 10000;

	config->tolMovingBitRate = 2000;

	config->tolCtbRcInter = VSI_DEFAULT_VALUE;
	config->tolCtbRcIntra = VSI_DEFAULT_VALUE;
	config->ctbRowQpStep = VSI_DEFAULT_VALUE;

	config->ltrInterval = VSI_DEFAULT_VALUE;
	config->longTermQpDelta = 0;
	config->longTermGap = 0;
	config->longTermGapOffset = 0;
	config->monitorFrames = VSI_DEFAULT_VALUE;
	config->bitrateWindow = VSI_DEFAULT_VALUE;
	config->intraQpDelta = VSI_DEFAULT_VALUE;
	config->hrdCpbSize = 1000000;
	config->vbr = 0;
	config->fixedIntraQp = 0;
	config->smoothPsnrInGOP = 0;
	config->staticSceneIbitPercent = 80;
	config->picSkip = 0;

	config->vui_timing_info_enable = 1;
	config->hashtype = 0;
	config->RpsInSliceHeader = 0;

    /*extension config for new API*/
    config->mmuEnable = 0;
    config->extSramLumHeightBwd = 0;
    config->extSramChrHeightBwd = 0;
    config->extSramLumHeightFwd = 0;
    config->extSramChrHeightFwd = 0;
    config->AXIAlignment = 0;
    config->codedChromaIdc = 1;
    config->aq_mode = 0;
    config->aq_strength = FLOAT_Q16(1.0);
    config->writeReconToDDR = 1;
    config->TxTypeSearchEnable = 0;
    config->PsyFactor = 0;
    config->meVertSearchRange = 0;
	config->layerInRefIdcEnable = 0;
    config->crf = -1;
	config->preset = VSI_DEFAULT_VALUE;

	OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
	deb = &pEnc->encConfig.deblocking;
	deb->nPortIndex = PORT_INDEX_OUTPUT;
	deb->bDeblocking = OMX_TRUE;

	OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
	quantization = &pEnc->encConfig.videoQuantization;
	quantization->nPortIndex = PORT_INDEX_OUTPUT;
	quantization->nQpI = 36;
	quantization->nQpP = 36;
	quantization->nQpB = 0; //Not used

	OMX_VIDEO_PARAM_CODECFORMAT* codecFormat = 0;
	codecFormat = &pEnc->encConfig.codecFormat;
	codecFormat->nPortIndex = PORT_INDEX_OUTPUT;
	codecFormat->nCodecFormat = OMX_VIDEO_CodingHEVC;

	DBGT_EPILOG("");
	return OMX_ErrorNone;
}

OMX_ERRORTYPE set_vc8000e_avc_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_AVCTYPE* config  =  0;
    config = &pEnc->encConfig.avc;

    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_AVCProfileHigh;
    config->eLevel = OMX_VIDEO_AVCLevel51;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

    config->nRefFrames = 1;

    config->nAllowedPictureTypes &= OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    config->nPFrames = 150;
    config->nSliceHeaderSpacing = 0 ;
    config->nBFrames = 0;
    config->bUseHadamard = OMX_FALSE;
    config->nRefIdx10ActiveMinus1 = 0;
    config->nRefIdx11ActiveMinus1 = 0;
    config->bEnableUEP = OMX_FALSE;
    config->bEnableFMO = OMX_FALSE;
    config->bEnableASO = OMX_TRUE; //OMX_FALSE;
    config->bEnableRS = OMX_FALSE;
    config->bFrameMBsOnly = OMX_FALSE;
    config->bMBAFF = OMX_FALSE;
    config->bEntropyCodingCABAC = OMX_TRUE; //OMX_FALSE;
    config->bWeightedPPrediction = OMX_FALSE;
    config->nWeightedBipredicitonMode = OMX_FALSE;
    config->bconstIpred = OMX_FALSE;
    config->bDirect8x8Inference = OMX_FALSE;
    config->bDirectSpatialTemporal = OMX_FALSE;
    config->nCabacInitIdc = OMX_FALSE;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
#ifdef CONFORMANCE
    config->eLevel = OMX_VIDEO_AVCLevel1;
    config->bUseHadamard = OMX_TRUE;
#endif

    OMX_VIDEO_CONFIG_AVCINTRAPERIOD* avcIdr;
    avcIdr = &pEnc->encConfig.avcIdr;
    avcIdr->nPFrames = 150;
    avcIdr->nIDRPeriod = 150;

    OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
    deb = &pEnc->encConfig.deblocking;
    deb->nPortIndex = PORT_INDEX_OUTPUT;
    deb->bDeblocking = OMX_TRUE; //OMX_FALSE;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = -1; //36;
    quantization->nQpP = 36;
    quantization->nQpB = 0; //Not used

    OMX_VIDEO_PARAM_CODECFORMAT* codecFormat = 0;
    codecFormat = &pEnc->encConfig.codecFormat;
    codecFormat->nPortIndex = PORT_INDEX_OUTPUT;
    codecFormat->nCodecFormat = OMX_VIDEO_CodingAVC;

    /*extension config for new API*/

    /*avc extension */
    OMX_VIDEO_PARAM_AVCEXTTYPE *cfg_ext;
    cfg_ext = &pEnc->encConfig.avcExt;
    cfg_ext->nPortIndex = PORT_INDEX_OUTPUT;
	cfg_ext->nBitDepthLuma = 8;
	cfg_ext->nBitDepthChroma = 8;
    cfg_ext->gopSize = 0;
	cfg_ext->hrdCpbSize = 1000000;
    cfg_ext->firstPic = 0;
    cfg_ext->lastPic = 0;
    cfg_ext->mmuEnable = 0;
    cfg_ext->extSramLumHeightBwd = 0;
    cfg_ext->extSramChrHeightBwd = 0;
    cfg_ext->extSramLumHeightFwd = 0;
    cfg_ext->extSramChrHeightFwd = 0;
    cfg_ext->AXIAlignment = 0;
    cfg_ext->codedChromaIdc = 1;
    cfg_ext->aq_mode = 0;
    cfg_ext->aq_strength = FLOAT_Q16(1.0);
    cfg_ext->writeReconToDDR = 1;
    cfg_ext->TxTypeSearchEnable = 0;
    cfg_ext->PsyFactor = 0;
    cfg_ext->meVertSearchRange = 0;
	cfg_ext->layerInRefIdcEnable = 0;
    cfg_ext->rdoLevel = 3;
    cfg_ext->crf = -1;
	cfg_ext->preset = VSI_DEFAULT_VALUE;

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


#endif

#ifdef ENCH2V41
OMX_ERRORTYPE set_h2v41_hevc_defaults(OMX_ENCODER* pEnc)
{
	int i;
	DBGT_PROLOG("");

	OMX_VIDEO_PARAM_HEVCTYPE* config  =  0;
	config = &pEnc->encConfig.hevc;

	config->nPortIndex = PORT_INDEX_OUTPUT;

	/* set default value for hevc parameters */
	config->eProfile = OMX_VIDEO_HEVCProfileMain;
	config->eLevel = OMX_VIDEO_HEVCLevel5;
	config->nPFrames = 0;
	config->nBFrames = 0;
	config->nRefFrames = 1;
	config->nBitDepthLuma = 8;
	config->nBitDepthChroma = 8;
	config->bStrongIntraSmoothing = OMX_FALSE;
	config->nTcOffset = -2;
	config->nBetaOffset = 5;
	config->bEnableDeblockOverride = OMX_FALSE;
	config->bDeblockOverride = OMX_FALSE;
	config->bEnableSAO = OMX_TRUE;

	config->ssim = 1;
	config->rdoLevel = 2;
	config->exp_of_input_alignment = 0;
	config->ctbRc = VSI_DEFAULT_VALUE;
	config->gopSize = 0; //default of test-bench is 0, set to 1 for JLQ because it needs GOP size=1 only;
	config->intraPicRate = 0;
	config->cuInfoVersion = -1;
	config->gdrDuration = 0;
	config->roiMapDeltaQpBlockUnit = 0;
	config->roiMapDeltaQpEnable = 0;
	config->RoiQpDelta_ver = 1;
	config->byteStream = 1;
	config->sliceSize = 0;
	config->gopLowdelay = 0;
	config->enableCabac = 1;
	config->bCabacInitFlag = OMX_FALSE;
	config->videoRange = 0;
	config->enableRdoQuant = VSI_DEFAULT_VALUE;
	config->fieldOrder = 0;
	config->cirStart = 0;
	config->cirInterval = 0;
	config->pcm_loop_filter_disabled_flag = 0;
	config->ipcmMapEnable = 0;
	for(i=0; i<8; i++)
	{
		config->roiQp[i] = VSI_DEFAULT_VALUE;
	}
	config->bEnableScalingList = OMX_FALSE;
	config->chromaQpOffset = 0;
	config->noiseReductionEnable = 0;
	config->noiseLow = 10;
	config->noiseFirstFrameSigma = 11;

	config->num_tile_columns = 1;
	config->num_tile_rows = 1;
	config->loop_filter_across_tiles_enabled_flag = 1;

	config->blockRCSize = VSI_DEFAULT_VALUE;
	config->picQpDeltaMin = VSI_DEFAULT_VALUE;
	config->picQpDeltaMax = VSI_DEFAULT_VALUE;

	config->bitVarRangeI = 10000;
	config->bitVarRangeP = 10000;
	config->bitVarRangeB = 10000;

	config->bitVarRangeI = 2000;
	config->bitVarRangeP = 2000;
	config->bitVarRangeB = 2000;

	config->tolMovingBitRate = 2000;

	config->ltrInterval = VSI_DEFAULT_VALUE;
	config->longTermQpDelta = 0;
	config->longTermGap = 0;
	config->longTermGapOffset = 0;
	config->monitorFrames = VSI_DEFAULT_VALUE;
	config->intraQpDelta = VSI_DEFAULT_VALUE;
	config->fixedIntraQp = 0;
	config->picSkip = 0;

	/* set default value for deblocking parameters */
	OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
	deb = &pEnc->encConfig.deblocking;
	deb->nPortIndex = PORT_INDEX_OUTPUT;
	deb->bDeblocking = OMX_TRUE;

	/* set default value for quant parameters */
	OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
	quantization = &pEnc->encConfig.videoQuantization;
	quantization->nPortIndex = PORT_INDEX_OUTPUT;
	quantization->nQpI = 36;
	quantization->nQpP = 36;
	quantization->nQpB = 0; //Not used

	/* set default value for format parameters */
	OMX_VIDEO_PARAM_CODECFORMAT* codecFormat = 0;
	codecFormat = &pEnc->encConfig.codecFormat;
	codecFormat->nPortIndex = PORT_INDEX_OUTPUT;
	codecFormat->nCodecFormat = OMX_VIDEO_CodingHEVC;

	DBGT_EPILOG("");
	return OMX_ErrorNone;
}

OMX_ERRORTYPE set_h2v41_avc_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_AVCTYPE* config  =  0;
    config = &pEnc->encConfig.avc;

    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_AVCProfileHigh;
    config->eLevel = OMX_VIDEO_AVCLevel51;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

    config->nRefFrames = 1;

    config->nAllowedPictureTypes &= OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    config->nPFrames = 150;
    config->nSliceHeaderSpacing = 0 ;
    config->nBFrames = 0;
    config->bUseHadamard = OMX_FALSE;
    config->nRefIdx10ActiveMinus1 = 0;
    config->nRefIdx11ActiveMinus1 = 0;
    config->bEnableUEP = OMX_FALSE;
    config->bEnableFMO = OMX_FALSE;
    config->bEnableASO = OMX_TRUE; //OMX_FALSE;
    config->bEnableRS = OMX_FALSE;
    config->bFrameMBsOnly = OMX_FALSE;
    config->bMBAFF = OMX_FALSE;
    config->bEntropyCodingCABAC = OMX_TRUE; //OMX_FALSE;
    config->bWeightedPPrediction = OMX_FALSE;
    config->nWeightedBipredicitonMode = OMX_FALSE;
    config->bconstIpred = OMX_FALSE;
    config->bDirect8x8Inference = OMX_FALSE;
    config->bDirectSpatialTemporal = OMX_FALSE;
    config->nCabacInitIdc = OMX_FALSE;
    config->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
#ifdef CONFORMANCE
    config->eLevel = OMX_VIDEO_AVCLevel1;
    config->bUseHadamard = OMX_TRUE;
#endif

    OMX_VIDEO_CONFIG_AVCINTRAPERIOD* avcIdr;
    avcIdr = &pEnc->encConfig.avcIdr;
    avcIdr->nPFrames = 150;
    avcIdr->nIDRPeriod = 150;

    OMX_PARAM_DEBLOCKINGTYPE* deb = 0;
    deb = &pEnc->encConfig.deblocking;
    deb->nPortIndex = PORT_INDEX_OUTPUT;
    deb->bDeblocking = OMX_TRUE; //OMX_FALSE;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = -1; //36;
    quantization->nQpP = 36;
    quantization->nQpB = 0; //Not used

    OMX_VIDEO_PARAM_CODECFORMAT* codecFormat = 0;
    codecFormat = &pEnc->encConfig.codecFormat;
    codecFormat->nPortIndex = PORT_INDEX_OUTPUT;
    codecFormat->nCodecFormat = OMX_VIDEO_CodingAVC;

    /*avc extension */
    OMX_VIDEO_PARAM_AVCEXTTYPE *cfg_ext;
    cfg_ext = &pEnc->encConfig.avcExt;
    cfg_ext->nPortIndex = PORT_INDEX_OUTPUT;
	cfg_ext->nBitDepthLuma = 8;
	cfg_ext->nBitDepthChroma = 8;
    cfg_ext->gopSize = 0;
	cfg_ext->hrdCpbSize = 0;
    cfg_ext->firstPic = 0;
    cfg_ext->lastPic = 0;

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


#endif



#if defined (ENC6280) || defined (ENC7280)
/**
 * OMX_ERRORTYPE set_mpeg4_defaults(OMX_ENCODER* pEnc)
 */
OMX_ERRORTYPE set_mpeg4_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_MPEG4TYPE* config = 0;
    config = &pEnc->encConfig.mpeg4;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    config->eLevel = OMX_VIDEO_MPEG4Level5;
    config->nAllowedPictureTypes &= OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    config->bSVH = OMX_FALSE;
    config->bGov = OMX_FALSE;
    config->nTimeIncRes = TIME_RESOLUTION_MPEG4;
    config->nPFrames = 0;
    config->nMaxPacketSize = 0;
    config->nHeaderExtension = 0;
    config->bReversibleVLC = OMX_FALSE;
    config->bACPred = 0;
    config->nSliceHeaderSpacing = 0;
    config->nBFrames = 0;
    config->nIDCVLCThreshold = 0;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = 20;
    quantization->nQpP = 20;
    quantization->nQpB = 0; //Not used

    OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE* ec = 0;
    ec = &pEnc->encConfig.ec;
    ec->nPortIndex = PORT_INDEX_OUTPUT;
    ec->bEnableHEC = OMX_FALSE;
    ec->bEnableResync = OMX_FALSE;
    ec->nResynchMarkerSpacing = 0;
    ec->bEnableDataPartitioning = OMX_FALSE;
    ec->bEnableRVLC = OMX_FALSE;

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE set_h263_defaults(OMX_ENCODER* pEnc);
 */
OMX_ERRORTYPE set_h263_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_H263TYPE* config = 0;
    config = &pEnc->encConfig.h263;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_H263ProfileBaseline;
    config->eLevel = OMX_VIDEO_H263Level70;
    config->bPLUSPTYPEAllowed = OMX_FALSE;
    config->nAllowedPictureTypes &= OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    config->nPFrames = 0;

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE* quantization  = 0;
    quantization = &pEnc->encConfig.videoQuantization;
    quantization->nPortIndex = PORT_INDEX_OUTPUT;
    quantization->nQpI = 20;
    quantization->nQpP = 20;
    quantization->nQpB = 0; //Not used

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif /*ndef ENC8270 & ENC8290*/

#ifdef ENCH1
OMX_ERRORTYPE set_vp8_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    OMX_VIDEO_PARAM_VP8TYPE* config  =  0;
    config = &pEnc->encConfig.vp8;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eProfile = OMX_VIDEO_VP8ProfileMain;
    config->eLevel = OMX_VIDEO_VP8Level_Version0;
    config->nDCTPartitions = 0;
    config->bErrorResilientMode = OMX_FALSE;

    OMX_VIDEO_VP8REFERENCEFRAMETYPE* vp8Ref = 0;
    vp8Ref = &pEnc->encConfig.vp8Ref;
    vp8Ref->bPreviousFrameRefresh = OMX_TRUE;
    vp8Ref->bGoldenFrameRefresh = OMX_FALSE;
    vp8Ref->bAlternateFrameRefresh = OMX_FALSE;
    vp8Ref->bUsePreviousFrame = OMX_TRUE;
    vp8Ref->bUseGoldenFrame = OMX_FALSE;
    vp8Ref->bUseAlternateFrame = OMX_FALSE;

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif // ENCH1
/**
 * static OMX_ERRORTYPE set_bitrate_defaults(OMX_ENCODER* pEnc);
 */
static OMX_ERRORTYPE set_bitrate_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    OMX_VIDEO_PARAM_BITRATETYPE* config = 0;
    config = &pEnc->encConfig.bitrate;
    config->nPortIndex = PORT_INDEX_OUTPUT;
    config->eControlRate = OMX_Video_ControlRateVariable;
    config->nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
#ifdef CONFORMANCE
    config->eControlRate = OMX_Video_ControlRateConstant;
#endif
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#endif // ~OMX_ENCODER_VIDEO_DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN

/**
 * OMX_U32 calculate_frame_size(OMX_ENCODER* pEnc)
 */
OMX_ERRORTYPE calculate_frame_size(OMX_ENCODER* pEnc, OMX_U32* frameSize)
{
    DBGT_PROLOG("");
    DBGT_PDEBUG("ASYNC: pEnc->sliceNum - %u", (unsigned)pEnc->sliceNum);
    DBGT_PDEBUG("ASYNC: pEnc->numOfSlices - %u", (unsigned)pEnc->numOfSlices);

    switch (pEnc->inputPort.def.format.image.eColorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420PackedPlanar:
            if (!pEnc->sliceMode)
            {
                *frameSize    =
                pEnc->inputPort.def.format.image.nFrameHeight *
                pEnc->inputPort.def.format.image.nStride * 3/2;
            }
            else
            {
                if (!(pEnc->sliceNum == pEnc->numOfSlices))  //Not an last slice use sliceheight
                {
                    *frameSize    =
                    pEnc->inputPort.def.format.image.nSliceHeight *
                    pEnc->inputPort.def.format.image.nStride * 3/2;
                }
                else
                {

                    DBGT_PDEBUG("ASYNC: Encoding last slice...");
                    *frameSize = (pEnc->outputPort.def.format.image.nFrameHeight -
                                  (pEnc->numOfSlices -1) * pEnc->inputPort.def.format.image.nSliceHeight) *
                                  pEnc->inputPort.def.format.image.nStride * 3/2;
                }
            }
            break;
#if defined (ENC6280) || defined (ENC7280)
        case OMX_COLOR_FormatYUV422Planar:
#endif
        case OMX_COLOR_Format16bitARGB4444:
        case OMX_COLOR_Format16bitARGB1555:
        case OMX_COLOR_Format12bitRGB444:
        case OMX_COLOR_Format16bitRGB565:
        case OMX_COLOR_Format16bitBGR565:
        case OMX_COLOR_FormatCbYCrY:
        case OMX_COLOR_FormatYCbYCr: // YCbCr 4:2:2 interleaved (YUYV)
            if (!pEnc->sliceMode)
            {
                *frameSize    =
                pEnc->inputPort.def.format.image.nFrameHeight *
                pEnc->inputPort.def.format.image.nStride * 2;
            }
            else
            {
                if (!(pEnc->sliceNum == pEnc->numOfSlices))  //Not an last slice use sliceheight
                {
                    *frameSize    =
                    pEnc->inputPort.def.format.image.nSliceHeight *
                    pEnc->inputPort.def.format.image.nStride * 2;
                }
                else
                {
                    DBGT_PDEBUG("ASYNC: Encoding last slice...");
                    *frameSize = (pEnc->outputPort.def.format.image.nFrameHeight -
                                  (pEnc->numOfSlices -1) * pEnc->inputPort.def.format.image.nSliceHeight) *
                                  pEnc->inputPort.def.format.image.nStride * 2;
                }
            }
            break;
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2)||defined (ENCVC8000E) || defined (ENCH2V41)
        /*case OMX_COLOR_Format24bitRGB888:
        case OMX_COLOR_Format24bitBGR888:*/
        case OMX_COLOR_Format25bitARGB1888:
        case OMX_COLOR_Format32bitARGB8888:
            if (!pEnc->sliceMode)
            {
                *frameSize    =
                pEnc->inputPort.def.format.image.nFrameHeight *
                pEnc->inputPort.def.format.image.nStride * 4;
            }
            else
            {
                if (!(pEnc->sliceNum == pEnc->numOfSlices))  //Not an last slice use sliceheight
                {
                    *frameSize    =
                    pEnc->inputPort.def.format.image.nSliceHeight *
                    pEnc->inputPort.def.format.image.nStride * 4;
                }
                else
                {
                    DBGT_PDEBUG("ASYNC: Encoding last slice...");
                    *frameSize = (pEnc->outputPort.def.format.image.nFrameHeight -
                                  (pEnc->numOfSlices -1) * pEnc->inputPort.def.format.image.nSliceHeight) *
                                  pEnc->inputPort.def.format.image.nStride * 4;
                }
            }
            break;
#endif
        default:
            DBGT_CRITICAL("ASYNC: unsupported format");
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedSetting;
            break;
    }
    DBGT_PDEBUG("Frame (%dx%d) size: %d", (int)pEnc->inputPort.def.format.image.nStride,
                 (int)pEnc->outputPort.def.format.image.nFrameHeight, (int)*frameSize);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE set_jpeg_defaults(OMX_ENCODER* pEnc);
 */
#ifdef ENABLE_JPEG
OMX_ERRORTYPE set_jpeg_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    OMX_IMAGE_PARAM_QFACTORTYPE* config = 0;
    config = &pEnc->encConfig.imageQuantization;
#ifdef ENCVC8000E
    config->nQFactor = 1;
#else
    config->nQFactor = 8;
#endif
    pEnc->sliceMode = OMX_FALSE;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif
#ifdef ENCH1
/**
 * static OMX_ERRORTYPE set_webp_defaults(OMX_ENCODER* pEnc);
 */
OMX_ERRORTYPE set_webp_defaults(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    // OMX_PARAM_PORTDEFINITIONTYPE* portDef = 0;
    // portDef = &pEnc->inputPort.def;
    
    OMX_IMAGE_PARAM_QFACTORTYPE* config = 0;
    config = &pEnc->encConfig.imageQuantization;
    config->nQFactor = 8;

    pEnc->sliceMode = OMX_FALSE;
    //portDef->format.image.nSliceHeight = 0;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif //ENCH1
#endif //#ifdef OMX_ENCODER_IMAGE_DOMAIN

/**
 * static OMX_ERRORTYPE set_preprocessor_defaults(OMX_ENCODER* pEnc);
 */
 OMX_ERRORTYPE set_preprocessor_defaults(OMX_ENCODER* pEnc)
 {
    DBGT_PROLOG("");

    OMX_CONFIG_ROTATIONTYPE* rotation = 0;
    rotation = &pEnc->encConfig.rotation;
    rotation->nPortIndex = PORT_INDEX_INPUT;
    rotation->nRotation = 0;

    OMX_CONFIG_RECTTYPE* crop = 0;
    crop = &pEnc->encConfig.crop;
    crop->nPortIndex = PORT_INDEX_INPUT;
    crop->nLeft = 0;
    crop->nTop = 0;
    crop->nWidth = pEnc->outputPort.def.format.video.nFrameWidth;
    crop->nHeight = pEnc->outputPort.def.format.video.nFrameHeight;

    DBGT_EPILOG("");
    return OMX_ErrorNone;
 }

 /**
  * static OMX_ERRORTYPE calculate_new_outputBufferSize(OMX_ENCODER* pEnc)
  */
 static OMX_ERRORTYPE calculate_new_outputBufferSize(OMX_ENCODER* pEnc)
 {
    DBGT_PROLOG("");

    OMX_U32 newBufferSize = 0;
    OMX_PARAM_PORTDEFINITIONTYPE* portDef = 0;
    portDef = &pEnc->outputPort.def;

#ifdef OMX_ENCODER_IMAGE_DOMAIN
    OMX_U32 frameWidth = portDef->format.image.nFrameWidth;
    OMX_U32 frameHeight;
    if (portDef->format.image.nSliceHeight > 0)
    {
        frameHeight = portDef->format.image.nSliceHeight;
    }
    else
    {
        frameHeight = portDef->format.image.nFrameHeight;
    }
#endif //#ifdef OMX_ENCODER_IMAGE_DOMAIN
#ifdef OMX_ENCODER_VIDEO_DOMAIN
    OMX_U32 frameWidth = portDef->format.video.nFrameWidth;
    OMX_U32 frameHeight = portDef->format.video.nFrameHeight;
#endif // ~OMX_ENCODER_VIDEO_DOMAIN

    DBGT_PDEBUG("API: framewidth - %u", (unsigned)frameWidth);
    DBGT_PDEBUG("API: frameHeight - %u", (unsigned)frameHeight);

    if ((((frameWidth + 15)/16) * ((frameHeight + 15)/16)) > 1620)
        newBufferSize = (frameWidth*frameHeight*3/4);
    else
        newBufferSize = (frameWidth*frameHeight*3/2);

// 7280 encoder requires bigger output buffer
#if defined (ENC7280) || defined (ENCVC8000E) || defined (ENCH2V41)
        newBufferSize *= 2;
#endif

#ifdef OMX_ENCODER_IMAGE_DOMAIN
    if (newBufferSize > 16*1024*1024)
    {
        newBufferSize = 16*1024*1024;
    }
#endif // OMX_ENCODER_IMAGE_DOMAIN

    portDef->nBufferSize = newBufferSize;

    //Inform client about new output port buffer size
#ifndef ANDROID // Stagefright doesn't like dynamic port config here
    pEnc->app_callbacks.EventHandler(
                    pEnc->self,
                    pEnc->app_data,
                    OMX_EventPortSettingsChanged,
                    PORT_INDEX_OUTPUT,
                    0,
                    NULL);
#endif
    DBGT_PDEBUG("API: calculated new buffer size - %u", (unsigned)newBufferSize);
    DBGT_PDEBUG("API: portDef->nBufferSize - %u", (unsigned)portDef->nBufferSize);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
 }


/**
 * static OMX_ERRORTYPE transition_to_idle_from_loaded(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_idle_from_loaded(OMX_ENCODER* pEnc)
{

    DBGT_PROLOG("");

    // the state transition cannot complete untill all
    // enabled ports are populated and the component has acquired
    // all of its static resources.
    DBGT_ASSERT(pEnc->state      == OMX_StateLoaded);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateIdle);
    DBGT_ASSERT(pEnc->codec == NULL);

    OMX_ERRORTYPE err = OMX_ErrorHardware;
    //ENCODER_PROTOTYPE*  codec = NULL;
    //FRAME_BUFFER        in;
    //FRAME_BUFFER        out;
    //memset(&in,  0, sizeof(FRAME_BUFFER));
    //memset(&out, 0, sizeof(FRAME_BUFFER));

    DBGT_PDEBUG("ASYNC: input port 0 is tunneled with: %p port: %d supplier:%s",
                pEnc->inputPort.tunnelcomp, (int)pEnc->inputPort.tunnelport,
                HantroOmx_str_omx_supplier(pEnc->inputPort.tunnel.eSupplier));

    DBGT_PDEBUG("ASYNC: output port 1 is tunneled with: %p port: %d supplier:%s",
                pEnc->outputPort.tunnelcomp, (int)pEnc->outputPort.tunnelport,
                HantroOmx_str_omx_supplier(pEnc->outputPort.tunnel.eSupplier));

    if (pEnc->inputPort.def.format.video.nStride & 0x0f)
    {
        err = OMX_ErrorUnsupportedSetting;
        goto FAIL;
    }

    if (HantroOmx_port_is_supplier(&pEnc->inputPort))
        if (supply_tunneled_port(pEnc, &pEnc->inputPort) != OMX_ErrorNone) {
            DBGT_CRITICAL("supply_tunneled_port (in) failed");
            goto FAIL;
        }

    if (HantroOmx_port_is_supplier(&pEnc->outputPort))
        if (supply_tunneled_port(pEnc, &pEnc->outputPort) != OMX_ErrorNone) {
            DBGT_CRITICAL("supply_tunneled_port (out) failed");
            goto FAIL;
        }

    DBGT_PDEBUG("ASYNC: waiting for buffers now!");

    while (!HantroOmx_port_is_ready(&pEnc->inputPort) ||
           !HantroOmx_port_is_ready(&pEnc->outputPort))
    {
        OSAL_ThreadSleep(RETRY_INTERVAL);
    }
    DBGT_PDEBUG("ASYNC: got all buffers");

#ifdef OMX_ENCODER_TEST_CODEC
    DBGT_PDEBUG("TEST Codec");
    ENCODER_PROTOTYPE* test_codec_create();
    pEnc->codec = test_codec_create();
#else
#ifdef OMX_ENCODER_VIDEO_DOMAIN
    switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
    {
#if !defined (ENCH2)&& !defined (ENCVC8000E) && !defined (ENCH2V41)
        case OMX_VIDEO_CodingAVC:
            {
            DBGT_PDEBUG("ASYNC: Creating H264 codec");
            H264_CONFIG config;

            config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
            config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
            config.nSliceHeight = 0;
            config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
            config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
            config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
            }
            else
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
            }
            config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
            if (config.pp_config.frameStabilization &&
                 (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
                 pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
            {
                //Stabilization with default value (16 pixels)
                DBGT_PDEBUG("ASYNC: Using default stabilization values");
                config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
                config.common_config.nOutputWidth -= 16;
                config.common_config.nOutputHeight -= 16;
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else if (config.pp_config.frameStabilization)
            {
                //Stabilization with user defined values
                //No other crop
                DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else
            {
                DBGT_PDEBUG("ASYNC: No stabilization");
                config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
                config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
            }
            config.h264_config.eProfile = pEnc->encConfig.avc.eProfile;
            config.h264_config.eLevel = pEnc->encConfig.avc.eLevel;
            config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
            config.nPFrames = pEnc->encConfig.avc.nPFrames;
            config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
            config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
            config.rate_config.nQpMin = 10;
            config.rate_config.nQpMax = 51;

            config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
            switch (config.rate_config.eRateControl)
            {
                case OMX_Video_ControlRateVariable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariable");
                }
                break;
                case OMX_Video_ControlRateDisable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateDisable");
                }
                break;
                case OMX_Video_ControlRateConstant:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstant");
                }
                break;
                case OMX_Video_ControlRateConstantSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstantSkipFrames");
                }
                break;
                case  OMX_Video_ControlRateVariableSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariableSkipFrames");
                }
                break;
                default:
                {
                    DBGT_PDEBUG("No rate control defined... using disabled value");
                    config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
            }
            config.bSeiMessages = OMX_FALSE;

            pEnc->codec = HantroHwEncOmx_encoder_create_h264(&config);
        }
        break;
#endif /* ifndef ENCH2 */

#if defined (ENC6280) || defined (ENC7280)
    case OMX_VIDEO_CodingMPEG4:
    {
        DBGT_PDEBUG("ASYNC: Creating MPEG4 codec");

        MPEG4_CONFIG config;

        config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
        DBGT_PDEBUG("ASYNC: config.pp_config.origWidth - %u", (unsigned) config.pp_config.origWidth);
        config.pp_config.origHeight =  pEnc->inputPort.def.format.video.nFrameHeight;
        DBGT_PDEBUG("ASYNC: config.pp_config.origHeight - %u", (unsigned)config.pp_config.origHeight);
        config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
        config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
        DBGT_PDEBUG("ASYNC: config.common_config.nInputFramerate - %u", (unsigned) config.common_config.nInputFramerate);
        config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
        DBGT_PDEBUG("ASYNC: config.pp_config.formatType - %u", (unsigned)config.pp_config.formatType);
        config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
        DBGT_PDEBUG("ASYNC: config.pp_config.frameStabilization - %u", (unsigned) config.pp_config.frameStabilization);
        config.pp_config.angle = pEnc->encConfig.rotation.nRotation;

        if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
        }
        else
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
        }
        config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
        if (config.pp_config.frameStabilization &&
                (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
                pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
        {
            //Stabilization with default value (16 pixels)
            DBGT_PDEBUG("ASYNC: Using default stabilization values");
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            config.common_config.nOutputWidth -= 16;
            config.common_config.nOutputHeight -= 16;
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else if (config.pp_config.frameStabilization)
        {
            //Stabilization with user defined values
            //No other crop
            DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else
        {
            DBGT_PDEBUG("ASYNC: No stabilization");
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
        }


        config.mp4_config.eProfile = pEnc->encConfig.mpeg4.eProfile;
        DBGT_PDEBUG("ASYNC: config.mp4_config.eProfile  - %u", (unsigned) config.mp4_config.eProfile);
        config.mp4_config.eLevel = pEnc->encConfig.mpeg4.eLevel;
        DBGT_PDEBUG("ASYNC: config.mp4_config.eLevel  - %u", (unsigned) config.mp4_config.eLevel);
        config.mp4_config.bSVH = pEnc->encConfig.mpeg4.bSVH;
        DBGT_PDEBUG("ASYNC: config.mp4_config.bSVH  - %u", (unsigned) config.mp4_config.bSVH);
        config.mp4_config.bReversibleVLC = pEnc->encConfig.mpeg4.bReversibleVLC;
        DBGT_PDEBUG("ASYNC: config.mp4_config.bReversibleVLC  - %u", (unsigned) config.mp4_config.bReversibleVLC);
        config.mp4_config.bGov = pEnc->encConfig.mpeg4.bGov;
        DBGT_PDEBUG("ASYNC: config.mp4_config.bGov  - %u", (unsigned) config.mp4_config.bGov);
        config.mp4_config.nPFrames = pEnc->encConfig.mpeg4.nPFrames;
        DBGT_PDEBUG("ASYNC: config.mp4_config.nPFrames  - %u", (unsigned) config.mp4_config.nPFrames);
        config.mp4_config.nTimeIncRes = pEnc->encConfig.mpeg4.nTimeIncRes;
        DBGT_PDEBUG("ASYNC: config.mp4_config.nTimeIncRes  - %u", (unsigned) config.mp4_config.nTimeIncRes);
        config.mp4_config.nHeaderExtension = pEnc->encConfig.mpeg4.nHeaderExtension;
        DBGT_PDEBUG("ASYNC: config.mp4_config.nHeaderExtension  - %u", (unsigned) config.mp4_config.nHeaderExtension);
        config.mp4_config.nMaxPacketSize = pEnc->encConfig.mpeg4.nMaxPacketSize;
        DBGT_PDEBUG("ASYNC: config.mp4_config.nMaxPacketSize  - %u", (unsigned) config.mp4_config.nMaxPacketSize);
        config.error_ctrl_config.bEnableDataPartitioning = pEnc->encConfig.ec.bEnableDataPartitioning;
        DBGT_PDEBUG("ASYNC: config.common_config.nOutputHeight  - %u", (unsigned) config.common_config.nOutputHeight);
        config.error_ctrl_config.bEnableResync = pEnc->encConfig.ec.bEnableResync;
        DBGT_PDEBUG("ASYNC: config.error_ctrl_config.bEnableResync - %u", (unsigned) config.error_ctrl_config.bEnableResync);
        config.error_ctrl_config.nResynchMarkerSpacing = pEnc->encConfig.ec.bEnableDataPartitioning;
        DBGT_PDEBUG("ASYNC: config.error_ctrl_config.nResynchMarkerSpacing  - %u", (unsigned) config.error_ctrl_config.nResynchMarkerSpacing);
        config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
        DBGT_PDEBUG("ASYNC: config.rate_config.nTargetBitrate  - %u", (unsigned) config.rate_config.nTargetBitrate);
        config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
        DBGT_PDEBUG("ASYNC: config.rate_config.nQpDefault  - %u", (unsigned) config.rate_config.nQpDefault);
        //config.rate_config.nQpMin = 4;
        config.rate_config.nQpMin = 1;
        DBGT_PDEBUG("ASYNC: config.rate_config.nQpMin  - %u", (unsigned) config.rate_config.nQpMin);
        config.rate_config.nQpMax = 31;
        DBGT_PDEBUG("ASYNC: config.rate_config.nQpMax  - %u", (unsigned) config.rate_config.nQpMax);
        config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;

        switch (config.rate_config.eRateControl)
        {
            case OMX_Video_ControlRateVariable:
            {
                config.rate_config.nVbvEnabled = 0;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
                DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariable");
            }
            break;
            case OMX_Video_ControlRateDisable:
            {
                config.rate_config.nVbvEnabled = 0;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
                DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateDisable");
            }
            break;
            case OMX_Video_ControlRateConstant:
            {
                config.rate_config.nVbvEnabled = 1; // will in turn enable mbRC in EncRateControl
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;
                DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstant");
            }
            break;
            case OMX_Video_ControlRateConstantSkipFrames:
            {
                config.rate_config.nVbvEnabled = 1;
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;
                DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstantSkipFrames");
            }
            break;
            case  OMX_Video_ControlRateVariableSkipFrames:
            {
                config.rate_config.nVbvEnabled = 0;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
                DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariableSkipFrames");
            }
            break;
            default:
            {
                DBGT_PDEBUG("No rate control defined... using disabled value");
                config.rate_config.nVbvEnabled = 0;
                config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
        }
        DBGT_PDEBUG("ASYNC: config.rate_config.eRateControl - %u", (unsigned) config.rate_config.eRateControl);
        DBGT_PDEBUG("ASYNC: config.rate_config.nMbRcEnabled - %u", (unsigned) config.rate_config.nMbRcEnabled);
        DBGT_PDEBUG("ASYNC: config.rate_config.nHrdEnabled - %u", (unsigned) config.rate_config.nHrdEnabled);
        DBGT_PDEBUG("ASYNC: config.rate_config.nPictureRcEnabled - %u", (unsigned) config.rate_config.nPictureRcEnabled);

        pEnc->codec = HantroHwEncOmx_encoder_create_mpeg4(&config);
    }
    break;
    case OMX_VIDEO_CodingH263:
    {
        DBGT_PDEBUG("ASYNC: Creating H263 codec");

        H263_CONFIG config;

        //Values from input port
        config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
        config.pp_config.origHeight =  pEnc->inputPort.def.format.video.nFrameHeight;
        config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
        config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
        config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;

        config.pp_config.angle = pEnc->encConfig.rotation.nRotation;

        if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
        }
        else
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
        }
        config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
        if (config.pp_config.frameStabilization &&
                (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
                pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
        {
            //Stabilization with default value (16 pixels)
            DBGT_PDEBUG("ASYNC: Using default stabilization values");
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            config.common_config.nOutputWidth -= 16;
            config.common_config.nOutputHeight -= 16;
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else if (config.pp_config.frameStabilization)
        {
            //Stabilization with user defined values
            //No other crop!
            DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else
        {
            DBGT_PDEBUG("ASYNC: No stabilization");
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
        }

        //H263 specific values
        config.h263_config.eProfile =  pEnc->encConfig.h263.eProfile;
        config.h263_config.eLevel = pEnc->encConfig.h263.eLevel;
        config.h263_config.nPFrames = pEnc->encConfig.h263.nPFrames; // I frame interval
        config.h263_config.bPLUSPTYPEAllowed = OMX_FALSE;

        //Rate Control
        config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
        config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
        config.rate_config.nQpMin = 1;
        config.rate_config.nQpMax = 31;
        config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
        switch (config.rate_config.eRateControl)
        {
            case OMX_Video_ControlRateVariable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case OMX_Video_ControlRateDisable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
            case OMX_Video_ControlRateConstant:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;

            }
            break;
            case OMX_Video_ControlRateConstantSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case  OMX_Video_ControlRateVariableSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            default:
            {
                DBGT_PDEBUG("No rate control defined... using disabled value");
                config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
        }

        config.rate_config.nVbvEnabled = 0;
        pEnc->codec = HantroHwEncOmx_encoder_create_h263(&config);
        }
        break;
#endif /* def ENC6280 || ENC7280 */

#ifdef ENCH1
    case OMX_VIDEO_CodingVP8:
    {
        DBGT_PDEBUG("ASYNC: Creating VP8 codec");
        VP8_CONFIG config;

        config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
        config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
        //config.nSliceHeight = 0;
        config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
        config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
        config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
        config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
        if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
        }
        else
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
        }
        config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
        if (config.pp_config.frameStabilization &&
             (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
             pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
        {
            //Stabilization with default value (16 pixels)
            DBGT_PDEBUG("ASYNC: Using default stabilization values");
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            config.common_config.nOutputWidth -= 16;
            config.common_config.nOutputHeight -= 16;
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else if (config.pp_config.frameStabilization)
        {
            //Stabilization with user defined values
            //No other crop
            DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else
        {
            DBGT_PDEBUG("ASYNC: No stabilization");
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
        }
        config.vp8_config.eProfile = pEnc->encConfig.vp8.eProfile;
        config.vp8_config.eLevel = pEnc->encConfig.vp8.eLevel;
        config.vp8_config.nDCTPartitions = pEnc->encConfig.vp8.nDCTPartitions;
        config.vp8_config.bErrorResilientMode = pEnc->encConfig.vp8.bErrorResilientMode;
        config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
        config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
        config.rate_config.nQpMin = 0;
        config.rate_config.nQpMax = 127;

        config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
        switch (config.rate_config.eRateControl)
        {
            case OMX_Video_ControlRateVariable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case OMX_Video_ControlRateDisable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
            case OMX_Video_ControlRateConstant:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;

            }
            break;
            case OMX_Video_ControlRateConstantSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case  OMX_Video_ControlRateVariableSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            default:
            {
                DBGT_PDEBUG("No rate control defined... using disabled value");
                config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
        }

        pEnc->codec = HantroHwEncOmx_encoder_create_vp8(&config);
    }
    break;
#endif //ENCH1

#ifdef ENCH2
    case OMX_VIDEO_CodingHEVC:
    {
        DBGT_PDEBUG("ASYNC: Creating HEVC codec");
        HEVC_CONFIG config;

        config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
        config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
        config.nSliceHeight = 0;
        config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
        config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
        config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
        config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
        config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
        config.pp_config.yOffset = pEnc->encConfig.crop.nTop;

        if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
        }
        else
        {
            config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
            config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
        }
        //config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
        if (pEnc->encConfig.stab.bStab)
        {
            DBGT_CRITICAL("ASYNC: Video stabilization not supported");
            err = OMX_ErrorUnsupportedSetting;
            break;
        }
       /* if (config.pp_config.frameStabilization &&
             (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
             pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
        {
            //Stabilization with default value (16 pixels)
            DBGT_PDEBUG("ASYNC: Using default stabilization values");
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            config.common_config.nOutputWidth -= 16;
            config.common_config.nOutputHeight -= 16;
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else if (config.pp_config.frameStabilization)
        {
            //Stabilization with user defined values
            //No other crop
            DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
            config.pp_config.xOffset = 0;
            config.pp_config.yOffset = 0;
        }
        else
        {
            DBGT_PDEBUG("ASYNC: No stabilization");
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
        }*/
        /*config.hevc_config.eProfile = pEnc->encConfig.hevc.eProfile;
        config.hevc_config.eLevel = pEnc->encConfig.hevc.eLevel;
        config.hevc_config.nBitDepthLuma = pEnc->encConfig.hevc.nBitDepthLuma;
        config.hevc_config.nBitDepthChroma = pEnc->encConfig.hevc.nBitDepthChroma;
        config.hevc_config.nPFrames = pEnc->encConfig.hevc.nPFrames;*/
        memcpy(&config.hevc_config, &pEnc->encConfig.hevc, pEnc->encConfig.hevc.nSize);

        config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
        config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
        config.rate_config.nQpMin = 0;
        config.rate_config.nQpMax = 51;
        config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
        config.bSeiMessages = OMX_FALSE;

        config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
        switch (config.rate_config.eRateControl)
        {
            case OMX_Video_ControlRateVariable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case OMX_Video_ControlRateDisable:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
            case OMX_Video_ControlRateConstant:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;

            }
            break;
            case OMX_Video_ControlRateConstantSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 1;
                config.rate_config.nHrdEnabled = 1;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            case  OMX_Video_ControlRateVariableSkipFrames:
            {
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 1;
            }
            break;
            default:
            {
                DBGT_PDEBUG("No rate control defined... using disabled value");
                config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                config.rate_config.nMbRcEnabled = 0;
                config.rate_config.nHrdEnabled = 0;
                config.rate_config.nPictureRcEnabled = 0;
            }
            break;
        }

        memcpy(config.roiArea, pEnc->encConfig.roiArea, sizeof(pEnc->encConfig.roiArea));
        memcpy(config.roiDeltaQP, pEnc->encConfig.roiDeltaQP, sizeof(pEnc->encConfig.roiDeltaQP));

        pEnc->codec = HantroHwEncOmx_encoder_create_hevc(&config);
    }
    break;
#endif //ENCH2
#ifdef ENCVC8000E
        case OMX_VIDEO_CodingHEVC:
        {
            DBGT_PDEBUG("ASYNC: Creating HEVC codec");
            CODEC_CONFIG config;

            config.codecFormat = pEnc->encConfig.codecFormat;

            //set pp configs
            config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
            config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
            config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            //config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
            if (pEnc->encConfig.stab.bStab)
            {
                DBGT_CRITICAL("ASYNC: Video stabilization not supported");
                err = OMX_ErrorUnsupportedSetting;
                break;
            }

            //set basic configs
            config.nSliceHeight = 0;
            config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
            config.bSeiMessages = OMX_FALSE;
            config.intraArea = pEnc->encConfig.intraArea;
            memcpy(config.ipcmArea, pEnc->encConfig.ipcmArea, sizeof(pEnc->encConfig.ipcmArea));
            memcpy(&config.roiArea, &pEnc->encConfig.roiArea, sizeof(pEnc->encConfig.roiArea));
            memcpy(config.roiDeltaQP, pEnc->encConfig.roiDeltaQP, sizeof(pEnc->encConfig.roiDeltaQP));

            //set common configs
            config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
            config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
            config.common_config.nBitDepthLuma = pEnc->encConfig.hevc.nBitDepthLuma;
            config.common_config.nBitDepthChroma = pEnc->encConfig.hevc.nBitDepthChroma;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
            }
            else
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
            }

            //set rate configs
            config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
            config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
            config.rate_config.nQpMin = 0;
            config.rate_config.nQpMax = 51;
    
            config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
            switch (config.rate_config.eRateControl)
            {
                case OMX_Video_ControlRateVariable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                case OMX_Video_ControlRateDisable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
                case OMX_Video_ControlRateConstant:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
    
                }
                break;
                case OMX_Video_ControlRateConstantSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                case  OMX_Video_ControlRateVariableSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                default:
                {
                    DBGT_PDEBUG("No rate control defined... using disabled value");
                    config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
            }

            //set hevc configs
            memcpy(&config.configs.hevc_config, &pEnc->encConfig.hevc, pEnc->encConfig.hevc.nSize);
            pEnc->codec = HantroHwEncOmx_encoder_create_codec(&config);
        }
        break;
        case OMX_VIDEO_CodingAVC:
            {
            DBGT_PDEBUG("ASYNC: Creating H264 codec");
            CODEC_CONFIG config;

            config.codecFormat = pEnc->encConfig.codecFormat;

            //set pp configs
            config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
            config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
            config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;;
            config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;

            //set basic configs
            config.nSliceHeight = 0;
            config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
            config.bSeiMessages = OMX_FALSE;
            config.nPFrames = pEnc->encConfig.avc.nPFrames;
            config.nBFrames = pEnc->encConfig.avc.nBFrames;
            //Reference to HEVC, to be confirmed
            config.intraArea = pEnc->encConfig.intraArea;
            memcpy(config.ipcmArea, pEnc->encConfig.ipcmArea, sizeof(pEnc->encConfig.ipcmArea));
            memcpy(&config.roiArea, &pEnc->encConfig.roiArea, sizeof(pEnc->encConfig.roiArea));
            memcpy(config.roiDeltaQP, pEnc->encConfig.roiDeltaQP, sizeof(pEnc->encConfig.roiDeltaQP));

            //set common configs
            config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
            config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
            config.common_config.nBitDepthLuma = pEnc->encConfig.avcExt.nBitDepthLuma;
            config.common_config.nBitDepthChroma = pEnc->encConfig.avcExt.nBitDepthChroma;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
            }
            else
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
            }

            //set pp configs part 2
            if (config.pp_config.frameStabilization &&
                 (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
                 pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
            {
                //Stabilization with default value (16 pixels)
                DBGT_PDEBUG("ASYNC: Using default stabilization values");
                config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
                config.common_config.nOutputWidth -= 16;
                config.common_config.nOutputHeight -= 16;
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else if (config.pp_config.frameStabilization)
            {
                //Stabilization with user defined values
                //No other crop
                DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else
            {
                DBGT_PDEBUG("ASYNC: No stabilization");
                config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
                config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
            }

            //set rate configs
            config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
            config.rate_config.nQpDefault = (OMX_S32)pEnc->encConfig.videoQuantization.nQpI;
            config.rate_config.nQpMin = 0;
            config.rate_config.nQpMax = 51;
            config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
            switch (config.rate_config.eRateControl)
            {
                case OMX_Video_ControlRateVariable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariable");
                }
                break;
                case OMX_Video_ControlRateDisable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateDisable");
                }
                break;
                case OMX_Video_ControlRateConstant:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstant");
                }
                break;
                case OMX_Video_ControlRateConstantSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstantSkipFrames");
                }
                break;
                case  OMX_Video_ControlRateVariableSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariableSkipFrames");
                }
                break;
                default:
                {
                    DBGT_PDEBUG("No rate control defined... using disabled value");
                    config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
            }

            memcpy(&config.configs.avc_config, &pEnc->encConfig.avc, pEnc->encConfig.avc.nSize);
            //set avc extension parameters
            memcpy(&config.avc_ext_config, &pEnc->encConfig.avcExt, pEnc->encConfig.avcExt.nSize);

            pEnc->codec = HantroHwEncOmx_encoder_create_codec(&config);
        }
        break;        
#endif //ENCVC8000E

#ifdef ENCH2V41
        case OMX_VIDEO_CodingHEVC:
        {
            DBGT_PDEBUG("ASYNC: Creating HEVC codec");
            CODEC_CONFIG config;

            config.codecFormat = pEnc->encConfig.codecFormat;

            //set pp configs
            config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
            config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
            config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
            config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
            config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            //config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;
            if (pEnc->encConfig.stab.bStab)
            {
                DBGT_CRITICAL("ASYNC: Video stabilization not supported");
                err = OMX_ErrorUnsupportedSetting;
                break;
            }

            //set basic configs
            config.nSliceHeight = 0;
            config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
            config.bSeiMessages = OMX_FALSE;
            config.intraArea = pEnc->encConfig.intraArea;
            memcpy(config.ipcmArea, pEnc->encConfig.ipcmArea, sizeof(pEnc->encConfig.ipcmArea));
            memcpy(&config.roiArea, &pEnc->encConfig.roiArea, sizeof(pEnc->encConfig.roiArea));
            memcpy(config.roiDeltaQP, pEnc->encConfig.roiDeltaQP, sizeof(pEnc->encConfig.roiDeltaQP));

            //set common configs
            config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
            config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
            config.common_config.nBitDepthLuma = pEnc->encConfig.hevc.nBitDepthLuma;
            config.common_config.nBitDepthChroma = pEnc->encConfig.hevc.nBitDepthChroma;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
            }
            else
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
            }

            //set rate configs
            config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
            config.rate_config.nQpDefault = pEnc->encConfig.videoQuantization.nQpI;
            config.rate_config.nQpMin = 0;
            config.rate_config.nQpMax = 51;
    
            config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
            switch (config.rate_config.eRateControl)
            {
                case OMX_Video_ControlRateVariable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                case OMX_Video_ControlRateDisable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
                case OMX_Video_ControlRateConstant:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
    
                }
                break;
                case OMX_Video_ControlRateConstantSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                case  OMX_Video_ControlRateVariableSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                }
                break;
                default:
                {
                    DBGT_PDEBUG("No rate control defined... using disabled value");
                    config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
            }

            //set hevc configs
            memcpy(&config.configs.hevc_config, &pEnc->encConfig.hevc, pEnc->encConfig.hevc.nSize);
            pEnc->codec = HantroHwEncOmx_encoder_create_codec(&config);
        }
        break;
        case OMX_VIDEO_CodingAVC:
        {
            DBGT_PDEBUG("ASYNC: Creating H264 codec");
            CODEC_CONFIG config;

            config.codecFormat = pEnc->encConfig.codecFormat;

            //set pp configs
            config.pp_config.formatType = pEnc->inputPort.def.format.video.eColorFormat;
            config.pp_config.origWidth = pEnc->inputPort.def.format.video.nStride;
            config.pp_config.origHeight = pEnc->inputPort.def.format.video.nFrameHeight;
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;;
            config.pp_config.frameStabilization = pEnc->encConfig.stab.bStab;

            //set basic configs
            config.nSliceHeight = 0;
            config.bDisableDeblocking = !pEnc->encConfig.deblocking.bDeblocking;
            config.bSeiMessages = OMX_FALSE;
            config.nPFrames = pEnc->encConfig.avc.nPFrames;
            config.nBFrames = pEnc->encConfig.avc.nBFrames;
            //Reference to HEVC, to be confirmed
            config.intraArea = pEnc->encConfig.intraArea;
            memcpy(config.ipcmArea, pEnc->encConfig.ipcmArea, sizeof(pEnc->encConfig.ipcmArea));
            memcpy(&config.roiArea, &pEnc->encConfig.roiArea, sizeof(pEnc->encConfig.roiArea));
            memcpy(config.roiDeltaQP, pEnc->encConfig.roiDeltaQP, sizeof(pEnc->encConfig.roiDeltaQP));

            //set common configs
            config.common_config.nInputFramerate = pEnc->inputPort.def.format.video.xFramerate;
            config.common_config.nOutputFramerate = pEnc->outputPort.def.format.video.xFramerate;
            config.common_config.nBitDepthLuma = pEnc->encConfig.avcExt.nBitDepthLuma;
            config.common_config.nBitDepthChroma = pEnc->encConfig.avcExt.nBitDepthChroma;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameHeight;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameWidth;
            }
            else
            {
                config.common_config.nOutputWidth = pEnc->outputPort.def.format.video.nFrameWidth;
                config.common_config.nOutputHeight = pEnc->outputPort.def.format.video.nFrameHeight;
            }

            //set pp configs part 2
            if (config.pp_config.frameStabilization &&
                 (OMX_S32)pEnc->outputPort.def.format.video.nFrameWidth == pEnc->inputPort.def.format.video.nStride &&
                 pEnc->outputPort.def.format.video.nFrameHeight == pEnc->inputPort.def.format.video.nFrameHeight)
            {
                //Stabilization with default value (16 pixels)
                DBGT_PDEBUG("ASYNC: Using default stabilization values");
                config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
                config.common_config.nOutputWidth -= 16;
                config.common_config.nOutputHeight -= 16;
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else if (config.pp_config.frameStabilization)
            {
                //Stabilization with user defined values
                //No other crop
                DBGT_PDEBUG("ASYNC: Using user defined stabilization values");
                config.pp_config.xOffset = 0;
                config.pp_config.yOffset = 0;
            }
            else
            {
                DBGT_PDEBUG("ASYNC: No stabilization");
                config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
                config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
            }

            //set rate configs
            config.rate_config.nTargetBitrate = pEnc->outputPort.def.format.video.nBitrate;
            config.rate_config.nQpDefault = (OMX_S32)pEnc->encConfig.videoQuantization.nQpI;
            config.rate_config.nQpMin = 0; //10;
            config.rate_config.nQpMax = 51;
            config.rate_config.eRateControl = pEnc->encConfig.bitrate.eControlRate;
            switch (config.rate_config.eRateControl)
            {
                case OMX_Video_ControlRateVariable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariable");
                }
                break;
                case OMX_Video_ControlRateDisable:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateDisable");
                }
                break;
                case OMX_Video_ControlRateConstant:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstant");
                }
                break;
                case OMX_Video_ControlRateConstantSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 1;
                    config.rate_config.nHrdEnabled = 1;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateConstantSkipFrames");
                }
                break;
                case  OMX_Video_ControlRateVariableSkipFrames:
                {
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 1;
                    DBGT_PDEBUG("ASYNC: eRateControl: OMX_VIDEO_ControlRateVariableSkipFrames");
                }
                break;
                default:
                {
                    DBGT_PDEBUG("No rate control defined... using disabled value");
                    config.rate_config.eRateControl = OMX_Video_ControlRateDisable;
                    config.rate_config.nMbRcEnabled = 0;
                    config.rate_config.nHrdEnabled = 0;
                    config.rate_config.nPictureRcEnabled = 0;
                }
                break;
            }

            memcpy(&config.configs.avc_config, &pEnc->encConfig.avc, pEnc->encConfig.avc.nSize);

            //set avc extension parameters
            memcpy(&config.avc_ext_config, &pEnc->encConfig.avcExt, pEnc->encConfig.avcExt.nSize);

            pEnc->codec = HantroHwEncOmx_encoder_create_codec(&config);
        }
        break;        
#endif //ENCH2V41


    default:
        DBGT_CRITICAL("ASYNC: unsupported input format. No such video codec");
        err = OMX_ErrorUnsupportedSetting;
        break;
    }
#endif //~OMX_ENCODER_VIDEO_DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN
    switch ((OMX_U32)pEnc->outputPort.def.format.image.eCompressionFormat)
    {
        DBGT_PDEBUG("IMAGE Domain");
#ifdef ENABLE_JPEG
        case OMX_IMAGE_CodingJPEG:
        {
            DBGT_PDEBUG("Creating JPEG encoder");

            JPEG_CONFIG config;

            //Values from input port
            config.pp_config.origWidth = pEnc->inputPort.def.format.image.nStride;
            config.pp_config.origHeight =  pEnc->inputPort.def.format.image.nFrameHeight;
            config.pp_config.formatType = pEnc->inputPort.def.format.image.eColorFormat;
            config.sliceHeight = pEnc->inputPort.def.format.image.nSliceHeight;
            //Pre processor values
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {

                config.pp_config.xOffset = pEnc->encConfig.crop.nTop;
                config.pp_config.yOffset = pEnc->encConfig.crop.nLeft;
                config.codingWidth = pEnc->outputPort.def.format.image.nFrameHeight;
                config.codingHeight = pEnc->outputPort.def.format.image.nFrameWidth;
            }
            else
            {
                config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
                config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
                config.codingWidth = pEnc->outputPort.def.format.image.nFrameWidth;
                config.codingHeight = pEnc->outputPort.def.format.image.nFrameHeight;
            }

            config.qLevel = pEnc->encConfig.imageQuantization.nQFactor;

            DBGT_PDEBUG("ASYNC: config.pp_config.origWidth - %u", (unsigned) config.pp_config.origWidth);
            DBGT_PDEBUG("ASYNC: config.pp_config.origHeight - %u", (unsigned) config.pp_config.origHeight);
            DBGT_PDEBUG("ASYNC: config.pp_config.formatType - %u", (unsigned) config.pp_config.formatType);
            DBGT_PDEBUG("ASYNC: config.sliceHeight - %u", (unsigned) config.sliceHeight);
            DBGT_PDEBUG("ASYNC: config.pp_config.angle - %u", (unsigned) config.pp_config.angle);
            DBGT_PDEBUG("ASYNC: config.pp_config.xOffset - %u", (unsigned) config.pp_config.xOffset);
            DBGT_PDEBUG("ASYNC: config.pp_config.yOffset - %u", (unsigned) config.pp_config.yOffset);
            DBGT_PDEBUG("ASYNC: config.codingWidth - %u", (unsigned) config.codingWidth);
            DBGT_PDEBUG("ASYNC: config.codingHeight - %u", (unsigned) config.codingHeight);
            DBGT_PDEBUG("ASYNC: config.qLevel - %u", (unsigned) config.qLevel);

            if (config.sliceHeight == pEnc->inputPort.def.format.image.nFrameHeight ||
                config.sliceHeight == 0)
            {
                DBGT_PDEBUG("config.codingType = JPEGENC_WHOLE_FRAME");
                config.codingType = JPEGENC_WHOLE_FRAME;
            }
            else
            {
                DBGT_PDEBUG("config.codingType = JPEGENC_SLICED_FRAME;");
                config.codingType = JPEGENC_SLICED_FRAME;
                pEnc->sliceMode = OMX_TRUE;
                if (config.codingHeight % config.sliceHeight == 0)
                {
                    DBGT_PDEBUG("config.codingHeight config.sliceHeight > 0 - TRUE");
                    pEnc->numOfSlices = config.codingHeight / config.sliceHeight;
                }
                else
                {
                    DBGT_PDEBUG("config.codingHeight config.sliceHeight > 0 - FALSE");
                    pEnc->numOfSlices = config.codingHeight / config.sliceHeight + 1;
                }
            }

            //set default configurations
#ifdef ENCVC8000E
            config.unitsType = JPEGENC_DOTS_PER_INCH;
            config.markerType = JPEGENC_MULTI_MARKER;
            config.xDensity = 72;
            config.yDensity = 72;
            config.bAddHeaders = OMX_TRUE;
            config.stringCommentMarker = "This is Hantro's test COM data header.";
#else
            config.unitsType = JPEGENC_DOTS_PER_INCH;
            config.markerType = JPEGENC_MULTI_MARKER;
            config.xDensity = 72;
            config.yDensity = 72;
            config.bAddHeaders = OMX_TRUE;
#endif
            pEnc->codec = HantroHwEncOmx_encoder_create_jpeg(&config);
        }
            break;
#endif // ENABLE_JPEG
#ifdef ENCH1
        case OMX_IMAGE_CodingWEBP:
        {
            DBGT_PDEBUG("Creating WEBP encoder");

            WEBP_CONFIG config;

            //Values from input port
            config.pp_config.origWidth = pEnc->inputPort.def.format.image.nStride;
            DBGT_PDEBUG("ASYNC: config.pp_config.origWidth - %u", (unsigned) config.pp_config.origWidth);
            config.pp_config.origHeight =  pEnc->inputPort.def.format.image.nFrameHeight;
            DBGT_PDEBUG("ASYNC: config.pp_config.origHeight - %u", (unsigned) config.pp_config.origHeight);
            config.pp_config.formatType = pEnc->inputPort.def.format.image.eColorFormat;
            DBGT_PDEBUG("ASYNC: config.pp_config.formatType - %u", (unsigned) config.pp_config.formatType);
            config.sliceHeight = pEnc->inputPort.def.format.image.nSliceHeight;
            DBGT_PDEBUG("ASYNC: config.sliceHeight - %u", (unsigned) config.sliceHeight);

            //Pre processor values
            config.pp_config.angle = pEnc->encConfig.rotation.nRotation;
            DBGT_PDEBUG("ASYNC: config.pp_config.angle - %u", (unsigned) config.pp_config.angle);
            if (pEnc->encConfig.rotation.nRotation == 90 || pEnc->encConfig.rotation.nRotation == 270)
            {

                config.pp_config.xOffset = pEnc->encConfig.crop.nTop;
                DBGT_PDEBUG("ASYNC: config.pp_config.xOffset - %u", (unsigned) config.pp_config.xOffset);
                config.pp_config.yOffset = pEnc->encConfig.crop.nLeft;
                DBGT_PDEBUG("ASYNC: config.pp_config.yOffset - %u", (unsigned) config.pp_config.yOffset);
                config.codingWidth = pEnc->outputPort.def.format.image.nFrameHeight;
                DBGT_PDEBUG("ASYNC: config.codingWidth - %u", (unsigned) config.codingWidth);
                config.codingHeight = pEnc->outputPort.def.format.image.nFrameWidth;
                DBGT_PDEBUG("ASYNC: config.codingHeight - %u", (unsigned) config.codingHeight);
            }
            else
            {
                config.pp_config.xOffset = pEnc->encConfig.crop.nLeft;
                DBGT_PDEBUG("ASYNC: config.pp_config.xOffset - %u", (unsigned) config.pp_config.xOffset);
                config.pp_config.yOffset = pEnc->encConfig.crop.nTop;
                DBGT_PDEBUG("ASYNC: config.pp_config.yOffset - %u", (unsigned) config.pp_config.yOffset);
                config.codingWidth = pEnc->outputPort.def.format.image.nFrameWidth;
                DBGT_PDEBUG("ASYNC: config.codingWidth - %u", (unsigned) config.codingWidth);
                config.codingHeight = pEnc->outputPort.def.format.image.nFrameHeight;
                DBGT_PDEBUG("ASYNC: config.codingHeight - %u", (unsigned) config.codingHeight);
            }

            config.qLevel = pEnc->encConfig.imageQuantization.nQFactor;
            DBGT_PDEBUG("ASYNC: config.qLevel - %u", (unsigned) config.qLevel);
            /* not supported
                if (config.sliceHeight == pEnc->inputPort.def.format.image.nFrameHeight ||
                config.sliceHeight == 0)
            {
                TRACE_PRINT(pEnc->log, "config.codingType = WEBPENC_WHOLE_FRAME\n");
                config.codingType = WEBPENC_WHOLE_FRAME;
            }
            else
            {
                TRACE_PRINT(pEnc->log, "config.codingType = WEBPENC_SLICED_FRAME;\n");
                config.codingType = WEBPENC_SLICED_FRAME;
                pEnc->sliceMode = OMX_TRUE;
                if (config.codingHeight % config.sliceHeight == 0)
                {
                    TRACE_PRINT(pEnc->log, "config.codingHeight config.sliceHeight > 0 - TRUE \n");
                    pEnc->numOfSlices = config.codingHeight / config.sliceHeight;
                }
                else
                {
                    TRACE_PRINT(pEnc->log, "config.codingHeight config.sliceHeight > 0 - FALSE \n");
                    pEnc->numOfSlices = config.codingHeight / config.sliceHeight + 1;
                }
            }*/

            pEnc->codec = HantroHwEncOmx_encoder_create_webp(&config);
        }
            break;
#endif //ENCH1
        default:
            DBGT_CRITICAL("ASYNC: unsupported input format. No such image codec");
            err = OMX_ErrorUnsupportedSetting;
            break;
    }
#endif //~OMX_ENCODER_IMAGE_DOMAIN
#endif //~OMX_ENCODER_TEST_CODEC

    if (!pEnc->codec)
    {
        DBGT_CRITICAL("ASYNC: Codec creating error");
        goto FAIL;
    }

    DBGT_ASSERT(pEnc->codec->destroy);
    DBGT_ASSERT(pEnc->codec->stream_start);
    DBGT_ASSERT(pEnc->codec->stream_end);
    DBGT_ASSERT(pEnc->codec->encode);

    OMX_U32 input_buffer_size  = pEnc->inputPort.def.nBufferSize;
    OMX_U32 output_buffer_size = pEnc->outputPort.def.nBufferSize;

    // create the temporary frame buffers. These are needed when
    // we are either using a buffer that was allocated by the client
    // or when the data in the buffers contains partial encoding units
    err = OSAL_AllocatorAllocMem(&pEnc->alloc, &input_buffer_size, &pEnc->frame_in.bus_data, &pEnc->frame_in.bus_address);
    if (err != OMX_ErrorNone)
    {
      DBGT_CRITICAL("OSAL_AllocatorAllocMem (frame in) failed (size=%d)", (int)input_buffer_size);
        goto FAIL;
    }
    DBGT_PDEBUG("API: allocated frame in buffer size:%u @physical addr: 0x%08lx @logical addr: %p",
                (unsigned) input_buffer_size, pEnc->frame_in.bus_address, pEnc->frame_in.bus_data);

    //NOTE: Specify internal outputbuffer size here according to compression format
    err = OSAL_AllocatorAllocMem(&pEnc->alloc, &output_buffer_size, &pEnc->frame_out.bus_data, &pEnc->frame_out.bus_address);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("OSAL_AllocatorAllocMem (frame out) failed (size=%d)", (int)output_buffer_size);
        goto FAIL;
    }
    DBGT_PDEBUG("API: allocated frame out size:%u @physical addr: 0x%08lx @logical addr: %p",
                (unsigned) output_buffer_size, pEnc->frame_out.bus_address, pEnc->frame_out.bus_data);

    pEnc->frame_in.capacity  = input_buffer_size;
    pEnc->frame_out.capacity = output_buffer_size;
    //pEnc->codec     = codec;
    //pEnc->frame_in  = in;
    //pEnc->frame_out = out;

    DBGT_EPILOG("");
    return OMX_ErrorNone;

 FAIL:
    DBGT_CRITICAL("ASYNC: error: %s", HantroOmx_str_omx_err(err));
    if (pEnc->frame_out.bus_address)
    {
        OSAL_AllocatorFreeMem(&pEnc->alloc, output_buffer_size, pEnc->frame_out.bus_data, pEnc->frame_out.bus_address);
        memset(&pEnc->frame_out, 0, sizeof(FRAME_BUFFER));
    }
    if (pEnc->frame_in.bus_address)
    {
        OSAL_AllocatorFreeMem(&pEnc->alloc, input_buffer_size, pEnc->frame_in.bus_data, pEnc->frame_in.bus_address);
        memset(&pEnc->frame_in, 0, sizeof(FRAME_BUFFER));
    }
    if (pEnc->codec)
    {
        pEnc->codec->destroy(pEnc->codec);
        pEnc->codec = NULL;
    }
    DBGT_EPILOG("");
    return err;
}


/**
 * static OMX_ERRORTYPE async_encoder_return_buffers(OMX_ENCODER* pEnc, PORT* p)
 */
static
OMX_ERRORTYPE async_encoder_return_buffers(OMX_ENCODER* pEnc, PORT* p)
{
    DBGT_PROLOG("");

    if (HantroOmx_port_is_supplier(p))
    {
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

    DBGT_PDEBUG("ASYNC: returning allocated buffers on port %d to %p %d",
                (int)p->def.nPortIndex, p->tunnelcomp, (int)p->tunnelport);

    // return the queued buffers to the suppliers
    // Danger. The port's locked and there are callbacks into the unknown.

    OMX_ERRORTYPE err = HantroOmx_port_lock_buffers(p);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_lock_buffers failed (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }

    OMX_U32 i;
    OMX_U32 count = HantroOmx_port_buffer_queue_count(p);
    for (i=0; i<count; ++i)
    {
        BUFFER* buff = 0;
        HantroOmx_port_get_buffer_at(p, &buff, i);
        DBGT_ASSERT(buff);
        if (HantroOmx_port_is_tunneled(p))
        {
            // tunneled but someone else is supplying. i.e. we have allocated the header.
            DBGT_ASSERT(buff->header == &buff->headerdata);
            // return the buffer to the supplier
            if (p->def.eDir == OMX_DirInput)
                ((OMX_COMPONENTTYPE*)p->tunnelcomp)->FillThisBuffer(p->tunnelcomp, buff->header);
            if (p->def.eDir == OMX_DirOutput)
                ((OMX_COMPONENTTYPE*)p->tunnelcomp)->EmptyThisBuffer(p->tunnelcomp, buff->header);
        }
        else
        {
            // return the buffer to the client
            if (p->def.eDir == OMX_DirInput)
                pEnc->app_callbacks.EmptyBufferDone(pEnc->self, pEnc->app_data, buff->header);
            if (p->def.eDir == OMX_DirOutput)
                pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, buff->header);
        }
    }
    HantroOmx_port_buffer_queue_clear(p);
    HantroOmx_port_unlock_buffers(p);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_idle_from_paused(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_idle_from_paused(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(pEnc->state == OMX_StatePause);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateIdle);

    async_encoder_return_buffers(pEnc, &pEnc->inputPort);
    async_encoder_return_buffers(pEnc, &pEnc->outputPort);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE wait_supplied_buffers(OMX_ENCODER* pEnc, PORT* port)
{
    DBGT_PROLOG("");
    UNUSED_PARAMETER(pEnc);

    if (!HantroOmx_port_is_supplier(port))
    {
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

    // buffer supplier component cannot transition
    // to idle untill all the supplied buffers have been returned
    // by the buffer user via a call to our FillThisBuffer
    OMX_U32 loop = 1;
    while (loop)
    {
        HantroOmx_port_lock_buffers(port);
        OMX_U32 queued = HantroOmx_port_buffer_queue_count(port);

        if (HantroOmx_port_buffer_count(port) == queued)
            loop = 0;

        DBGT_PDEBUG("ASYNC: port %d has %d buffers out of %d supplied",
                    (int)port->def.nPortIndex, (int)queued,
                    (int)HantroOmx_port_buffer_count(port));
        HantroOmx_port_unlock_buffers(port);
        if (loop)
            OSAL_ThreadSleep(RETRY_INTERVAL);
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_idle_from_executing(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_idle_from_executing(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(pEnc->state      == OMX_StateExecuting);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateIdle);

    async_encoder_return_buffers(pEnc, &pEnc->inputPort);
    async_encoder_return_buffers(pEnc, &pEnc->outputPort);

    wait_supplied_buffers(pEnc, &pEnc->inputPort);
    wait_supplied_buffers(pEnc, &pEnc->outputPort);

    //Check do we have to call stream_end for HW encoder
    if (pEnc->streamStarted)
    {
        pEnc->streamStarted = OMX_FALSE;
    }

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_loaded_from_idle(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_loaded_from_idle(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    // the state transition cannot complete untill all
    // enabled ports have their buffers freed

    DBGT_ASSERT(pEnc->state      == OMX_StateIdle);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateLoaded);

    if (HantroOmx_port_is_supplier(&pEnc->inputPort))
        unsupply_tunneled_port(pEnc, &pEnc->inputPort);
    if (HantroOmx_port_is_supplier(&pEnc->outputPort))
        unsupply_tunneled_port(pEnc, &pEnc->outputPort);

    // should be okay without locking
    DBGT_ASSERT(HantroOmx_port_buffer_queue_count(&pEnc->inputPort)==0);
    DBGT_ASSERT(HantroOmx_port_buffer_queue_count(&pEnc->outputPort)==0);

    DBGT_PDEBUG("ASYNC: waiting for ports to be non-populated");
    while (HantroOmx_port_has_buffers(&pEnc->inputPort)  ||
           HantroOmx_port_has_buffers(&pEnc->outputPort))
    {
        OSAL_ThreadSleep(RETRY_INTERVAL);
    }

    DBGT_ASSERT(pEnc->codec);
    DBGT_PDEBUG("ASYNC: destroying codec");
    pEnc->codec->destroy(pEnc->codec);

    DBGT_PDEBUG("ASYNC: freeing internal frame buffers");
    if (pEnc->frame_in.bus_address)
        FRAME_BUFF_FREE(&pEnc->alloc, &pEnc->frame_in);
    if (pEnc->frame_out.bus_address)
        FRAME_BUFF_FREE(&pEnc->alloc, &pEnc->frame_out);

    pEnc->codec    = NULL;
    memset(&pEnc->frame_in,  0, sizeof(FRAME_BUFFER));
    memset(&pEnc->frame_out, 0, sizeof(FRAME_BUFFER));

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_loaded_from_waitForResources(pEnc)
 */
static
OMX_ERRORTYPE transition_to_loaded_from_waitForResources(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    // the state transition cannot complete untill all
    // enabled ports have their buffers freed

    DBGT_ASSERT(pEnc->state      == OMX_StateWaitForResources);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateLoaded);

    //NOTE: External HWRM should be used here
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_executing_from_paused(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_executing_from_paused(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc->state == OMX_StatePause);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateExecuting);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE transition_to_executing_from_idle(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_executing_from_idle(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc->state == OMX_StateIdle);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateExecuting);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE startup_tunnel(OMX_ENCODER* pEnc, PORT* port)
{
    DBGT_PROLOG("");
    OMX_U32 buffers   = 0;
    OMX_U32 i         = 0;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (HantroOmx_port_is_enabled(port) && HantroOmx_port_is_supplier(port))
    {
        if (port == &pEnc->outputPort)
        {
            // queue the buffers into own queue since the
            // output port is the supplier the tunneling component
            // cannot fill the buffer queue.
            buffers = HantroOmx_port_buffer_count(port);
            for (i=0; i<buffers; ++i)
            {
                BUFFER* buff = NULL;
                HantroOmx_port_get_allocated_buffer_at(port, &buff, i);
                HantroOmx_port_push_buffer(port, buff);
            }
        }
        else
        {
            buffers = HantroOmx_port_buffer_count(port);
            for (; i<buffers; ++i)
            {
                BUFFER* buff = NULL;
                HantroOmx_port_get_allocated_buffer_at(port, &buff, i);
                DBGT_ASSERT(buff);
                DBGT_ASSERT(buff->header != &buff->headerdata);
                err = ((OMX_COMPONENTTYPE*)port->tunnelcomp)->FillThisBuffer(port->tunnelcomp, buff->header);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("ASYNC: tunneling component failed to fill buffer: %s", HantroOmx_str_omx_err(err));
                    DBGT_EPILOG("");
                    return err;
                }
            }
        }
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


/**
 * OMX_ERRORTYPE transition_to_waitForResources_from_loaded(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE transition_to_waitForResources_from_loaded(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc->state == OMX_StateLoaded);
    DBGT_ASSERT(pEnc->statetrans == OMX_StateWaitForResources);
    //NOTE: External HWRM should be used here
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static OMX_ERRORTYPE async_encoder_set_state(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                             OMX_PTR pCmdData, OMX_PTR arg)
 */
static
OMX_ERRORTYPE async_encoder_set_state(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                             OMX_PTR pCmdData, OMX_PTR arg)
{
    UNUSED_PARAMETER(Cmd);
    UNUSED_PARAMETER(pCmdData);
    DBGT_PROLOG("");

    OMX_ERRORTYPE err = OMX_ErrorIncorrectStateTransition;
    OMX_ENCODER*  pEnc = (OMX_ENCODER*)arg;
    OMX_STATETYPE nextstate = (OMX_STATETYPE)nParam1;

    if (nextstate == pEnc->state)
    {
        DBGT_PDEBUG("ASYNC: same state:%s", HantroOmx_str_omx_state(pEnc->state));
        pEnc->app_callbacks.EventHandler(pEnc->self,
                pEnc->app_data, OMX_EventError, OMX_ErrorSameState, 0, NULL);
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }
    switch (nextstate)
    {
        case OMX_StateIdle:
            switch (pEnc->state)
            {
                case OMX_StateLoaded:
                    err = transition_to_idle_from_loaded(pEnc);
                    break;
                case OMX_StatePause:
                    err = transition_to_idle_from_paused(pEnc);
                    break;
                case OMX_StateExecuting:
                    err = transition_to_idle_from_executing(pEnc);
                    break;
                default:
                    break;
            }
            break;
        case OMX_StateLoaded:
            if (pEnc->state == OMX_StateIdle)
                err = transition_to_loaded_from_idle(pEnc);
            if (pEnc->state == OMX_StateWaitForResources)
                err = transition_to_loaded_from_waitForResources(pEnc);
            break;
        case OMX_StateExecuting:
            if (pEnc->state == OMX_StatePause)                   /// OMX_StatePause to OMX_StateExecuting
                err = transition_to_executing_from_paused(pEnc);
            if (pEnc->state == OMX_StateIdle)
                err = transition_to_executing_from_idle(pEnc);   /// OMX_StateIdle to OMX_StateExecuting
            break;
        case OMX_StatePause:
            {
                if (pEnc->state == OMX_StateIdle)                /// OMX_StateIdle to OMX_StatePause
                    err = OMX_ErrorNone;
                if (pEnc->state == OMX_StateExecuting)           /// OMX_StateExecuting to OMX_StatePause
                    err = OMX_ErrorNone;
            }
            break;
        case OMX_StateInvalid:
            DBGT_CRITICAL("Invalid state");
            err = OMX_ErrorInvalidState;
            break;
        case OMX_StateWaitForResources:
            if (pEnc->state == OMX_StateLoaded)
                    err = transition_to_waitForResources_from_loaded(pEnc);
            break;
        default:
            DBGT_ASSERT(!"Incorrect state");
            break;
    }

    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("ASYNC: set state error:%s", HantroOmx_str_omx_err(err));
        if (err != OMX_ErrorIncorrectStateTransition)
        {
            pEnc->state = OMX_StateInvalid;
            pEnc->run   = OMX_FALSE;
        }
        else
        {
            // clear state transition flag
            pEnc->statetrans = pEnc->state;
        }
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, err, 0, NULL);
    }
    else
    {
        // transition complete
        DBGT_PDEBUG("ASYNC: set state complete:%s", HantroOmx_str_omx_state(nextstate));
        OSAL_MutexLock(pEnc->statemutex);
        pEnc->statetrans = nextstate;
        pEnc->state      = nextstate;
        OSAL_MutexUnlock(pEnc->statemutex);
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandStateSet, pEnc->state, NULL);

        if (pEnc->state == OMX_StateExecuting)
        {
            err = startup_tunnel(pEnc, &pEnc->inputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("ASYNC: failed to startup buffer processing: %s", HantroOmx_str_omx_err(err));
                pEnc->state = OMX_StateInvalid;
                pEnc->run   = OMX_FALSE;
            }
            err = startup_tunnel(pEnc, &pEnc->outputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("ASYNC: failed to startup buffer processing: %s", HantroOmx_str_omx_err(err));
                pEnc->state = OMX_StateInvalid;
                pEnc->run   = OMX_FALSE;
            }
        }
    }
    DBGT_EPILOG("");
    return err;
}

/**
 * static OMX_ERRORTYPE async_encoder_wait_port_buffer_dealloc(OMX_ENCODER* pEnc, PORT* p)
 */
static
OMX_ERRORTYPE async_encoder_wait_port_buffer_dealloc(OMX_ENCODER* pEnc, PORT* p)
{
    UNUSED_PARAMETER(pEnc);
    DBGT_PROLOG("");

    while (HantroOmx_port_has_buffers(p))
    {
        OSAL_ThreadSleep(RETRY_INTERVAL);
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


/**
 * OMX_ERRORTYPE async_encoder_disable_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                            OMX_PTR pCmdData, OMX_PTR arg)
 * Port cannot be populated when it is disabled
 */
static
OMX_ERRORTYPE async_encoder_disable_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                         OMX_PTR pCmdData, OMX_PTR arg)
{
    UNUSED_PARAMETER(Cmd);
#ifndef ENABLE_DBGT_TRACE
    UNUSED_PARAMETER(pCmdData);
#endif
    DBGT_PROLOG("");

    OMX_ENCODER*  pEnc = (OMX_ENCODER*)arg;
    OMX_U32 portIndex = nParam1;

    // return the queue'ed buffers to the suppliers
    // and wait untill all the buffers have been free'ed
    DBGT_PDEBUG("ASYNC: nParam1:%u pCmdData:%p", (unsigned)nParam1, pCmdData);

    if (portIndex == OMX_ALL)
    {
        DBGT_PDEBUG("ASYNC: disabling all ports");
        async_encoder_return_buffers(pEnc, &pEnc->inputPort);
        async_encoder_return_buffers(pEnc, &pEnc->outputPort);

        wait_supplied_buffers(pEnc, &pEnc->inputPort);
        wait_supplied_buffers(pEnc, &pEnc->outputPort);

        if (HantroOmx_port_is_supplier(&pEnc->inputPort))
            unsupply_tunneled_port(pEnc, &pEnc->inputPort);
        if (HantroOmx_port_is_supplier(&pEnc->outputPort))
            unsupply_tunneled_port(pEnc, &pEnc->outputPort);

        async_encoder_wait_port_buffer_dealloc(pEnc, &pEnc->inputPort);
        async_encoder_wait_port_buffer_dealloc(pEnc, &pEnc->outputPort);

        OMX_U32 i = 0;
        for (i=0; i<2; ++i)
        {
            pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandPortDisable, i, NULL);
        }
        DBGT_PDEBUG("ASYNC: ports disabled");
    }
    else
    {
        PORT* ports[] = {&pEnc->inputPort, &pEnc->outputPort};
        OMX_U32 i = 0;
        for (i=0; i<sizeof(ports)/sizeof(PORT*); ++i)
        {
            if (portIndex == i)
            {
                DBGT_PDEBUG("ASYNC: disabling port: %d", (int)i);
                async_encoder_return_buffers(pEnc, ports[i]);
                wait_supplied_buffers(pEnc, ports[i]);
                if (HantroOmx_port_is_supplier(ports[i]))
                    unsupply_tunneled_port(pEnc, ports[i]);

                async_encoder_wait_port_buffer_dealloc(pEnc, ports[i]);
                break;
            }
        }
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandPortDisable, portIndex, NULL);
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}


/**
 * OMX_ERRORTYPE async_encoder_enable_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                            OMX_PTR pCmdData, OMX_PTR arg)
 */
static
OMX_ERRORTYPE async_encoder_enable_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1,
                                        OMX_PTR pCmdData, OMX_PTR arg)
{
    UNUSED_PARAMETER(Cmd);
#ifndef ENABLE_DBGT_TRACE
    UNUSED_PARAMETER(pCmdData);
#endif
    DBGT_PROLOG("");

    OMX_ENCODER* pEnc  = (OMX_ENCODER*)arg;
    OMX_U32 portIndex = nParam1;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    DBGT_PDEBUG("ASYNC: nParam1:%u pCmdData:%p", (unsigned)nParam1, pCmdData);

    if (portIndex == PORT_INDEX_INPUT || portIndex == OMX_ALL)
    {
        if (pEnc->state != OMX_StateLoaded)
        {
            if (HantroOmx_port_is_supplier(&pEnc->inputPort))
            {
                err = supply_tunneled_port(pEnc, &pEnc->inputPort);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("supply_tunneled_port (in) (err=%x)", err);
                    DBGT_EPILOG("");
                    return err;
                }
            }
            while (!HantroOmx_port_is_ready(&pEnc->inputPort))
                OSAL_ThreadSleep(RETRY_INTERVAL);
        }
        if (pEnc->state == OMX_StateExecuting)
            startup_tunnel(pEnc, &pEnc->inputPort);

        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandPortEnable, PORT_INDEX_INPUT, NULL);
        DBGT_PDEBUG("ASYNC: input port enabled");

    }
    if (portIndex == PORT_INDEX_OUTPUT || portIndex == OMX_ALL)
    {
        if (pEnc->state != OMX_StateLoaded)
        {
            if (HantroOmx_port_is_supplier(&pEnc->outputPort))
            {
                err = supply_tunneled_port(pEnc, &pEnc->outputPort);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("supply_tunneled_port (out) (err=%x)", err);
                    DBGT_EPILOG("");
                    return err;
                }
            }
            while (!HantroOmx_port_is_ready(&pEnc->outputPort))
                OSAL_ThreadSleep(RETRY_INTERVAL);
        }
        if (pEnc->state == OMX_StateExecuting)
            startup_tunnel(pEnc, &pEnc->outputPort);

        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandPortEnable, PORT_INDEX_OUTPUT, NULL);
        DBGT_PDEBUG("ASYNC: output port enabled");
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * static
OMX_ERRORTYPE async_encoder_flush_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData, OMX_PTR arg)
 */
static
OMX_ERRORTYPE async_encoder_flush_port(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData, OMX_PTR arg)
{
    UNUSED_PARAMETER(Cmd);
#ifndef ENABLE_DBGT_TRACE
    UNUSED_PARAMETER(pCmdData);
#endif
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    OMX_ENCODER* pEnc  = (OMX_ENCODER*)arg;
    OMX_U32 portindex = nParam1;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    DBGT_PDEBUG("ASYNC: nParam1:%u pCmdData:%p", (unsigned)nParam1, pCmdData);

    if (portindex == OMX_ALL)
    {
        DBGT_PDEBUG("ASYNC: flushing all ports");
        err = async_encoder_return_buffers(pEnc, &pEnc->inputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_encoder_return_buffers (in) failed (err=%x)", err);
            goto FAIL;
        }
        err = async_encoder_return_buffers(pEnc, &pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_encoder_return_buffers (out) failed (err=%x)", err);
            goto FAIL;
        }
        OMX_U32 i = 0;
        for (i=0; i<2; ++i)
        {
            pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandFlush, i, NULL);
        }
        DBGT_PDEBUG("ASYNC: all buffers flushed");
    }
    else
    {
        if (portindex == PORT_INDEX_INPUT)
        {
            err = async_encoder_return_buffers(pEnc, &pEnc->inputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("async_encoder_return_buffers (in) failed (err=%x)", err);
                goto FAIL;
            }

            pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandFlush, portindex, NULL);
            DBGT_PDEBUG("ASYNC: input port flushed");
        }
        else if (portindex == PORT_INDEX_OUTPUT)
        {
            err = async_encoder_return_buffers(pEnc, &pEnc->outputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("async_encoder_return_buffers (out) failed (err=%x)", err);
                goto FAIL;
            }

            pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventCmdComplete, OMX_CommandFlush, portindex, NULL);
            DBGT_PDEBUG("ASYNC: output port flushed");
        }
    }
    DBGT_EPILOG("");
    return OMX_ErrorNone;
 FAIL:
    DBGT_ASSERT(err != OMX_ErrorNone);
    DBGT_CRITICAL("ASYNC: error while flushing port: %s", HantroOmx_str_omx_err(err));
    pEnc->state = OMX_StateInvalid;
    pEnc->run   = OMX_FALSE;
    pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, err, 0, NULL);
    DBGT_EPILOG("");
    return err;
}


static
OMX_ERRORTYPE async_encoder_mark_buffer(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData, OMX_PTR arg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(pCmdData);
    UNUSED_PARAMETER(Cmd);
    UNUSED_PARAMETER(nParam1);
    OMX_MARKTYPE* mark = (OMX_MARKTYPE*)pCmdData;
    OMX_ENCODER* dec   = (OMX_ENCODER*)arg;

    if (dec->mark_write_pos < sizeof(dec->marks)/sizeof(OMX_MARKTYPE))
    {
        dec->marks[dec->mark_write_pos] = *mark;
        DBGT_PDEBUG("ASYNC: set mark in index: %d", (int)dec->mark_write_pos);
        dec->mark_write_pos++;
    }
    else
    {
        DBGT_ERROR("ASYNC: no space for mark");
        dec->app_callbacks.EventHandler(dec->self, dec->app_data, OMX_EventError, OMX_ErrorUndefined, 0, NULL);
    }
    OSAL_Free(mark);
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRORTYPE grow_frame_buffer(OMX_ENCODER* pEnc, FRAME_BUFFER* fb, OMX_U32 capacity)
 */
static
OMX_ERRORTYPE grow_frame_buffer(OMX_ENCODER* pEnc, FRAME_BUFFER* fb, OMX_U32 capacity)
{
    DBGT_PROLOG("");
    DBGT_PDEBUG("ASYNC: fb capacity: %u new capacity:%u",
                (unsigned)fb->capacity, (unsigned)capacity);

    FRAME_BUFFER new;

    memset(&new, 0, sizeof(FRAME_BUFFER));
    OMX_ERRORTYPE err = OSAL_AllocatorAllocMem(&pEnc->alloc, &capacity, &new.bus_data, &new.bus_address);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("ASYNC: frame buffer reallocation failed: %s", HantroOmx_str_omx_err(err));
        DBGT_EPILOG("");
        return err;
    }
    DBGT_PDEBUG("API: allocated grow frame buffer size:%u @physical addr: 0x%08lx @logical addr: %p",
                (unsigned) capacity, new.bus_address, new.bus_data);

    memcpy(new.bus_data, fb->bus_data, fb->size);
    new.capacity = capacity;
    new.size     = fb->size;
    OSAL_AllocatorFreeMem(&pEnc->alloc, fb->capacity, fb->bus_data, fb->bus_address);
    *fb = new;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRRORTYPE async_start_stream(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE async_start_stream(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    BUFFER*         outputBuffer        = NULL;
    STREAM_BUFFER   outputStream;
    OMX_ERRORTYPE   err                 = OMX_ErrorNone;
#ifdef USE_TEMP_OUTPUTBUFFER
    FRAME_BUFFER*   tempBuffer          = &pEnc->frame_out;
#endif

    err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_lock_buffers (out) (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }
    HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
    HantroOmx_port_unlock_buffers(&pEnc->outputPort);

    OMX_S32 retry = MAX_RETRIES; // needed for stagefright when video is paused (no transition to pause)
    while (outputBuffer == NULL && retry > 0 && pEnc->state != OMX_StatePause && pEnc->statetrans
                != OMX_StatePause && pEnc->statetrans != OMX_StateIdle)
    {
        OSAL_ThreadSleep(RETRY_INTERVAL);

        err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("HantroOmx_port_lock_buffers(out) (err=%x)", err);
            return err;
        }

        HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);
        retry--;
    }

    if (outputBuffer == NULL)
    {
        DBGT_CRITICAL("ASYNC: there's no output buffer!");
        DBGT_EPILOG("");
        return OMX_ErrorOverflow;
    }

    memset(&outputStream, 0, sizeof(STREAM_BUFFER));
#ifdef USE_TEMP_OUTPUTBUFFER
    DBGT_PDEBUG("ASYNC: Using temp output buffer");
    outputStream.buf_max_size = tempBuffer->capacity;
    outputStream.bus_data = tempBuffer->bus_data;
    outputStream.bus_address = tempBuffer->bus_address;
#else
    DBGT_PDEBUG("ASYNC: outputBuffer->bus_data %p outputBuffer->header->pBuffer %p",
                outputBuffer->bus_data, outputBuffer->header->pBuffer);
    outputStream.buf_max_size = outputBuffer->header->nAllocLen;
    outputStream.bus_data = outputBuffer->bus_data;
    outputStream.bus_address = outputBuffer->bus_address;
#endif

    CODEC_STATE codecState = CODEC_ERROR_UNSPECIFIED;
    codecState = pEnc->codec->stream_start(pEnc->codec, &outputStream);
    if (codecState != CODEC_OK)
    {
        DBGT_CRITICAL("ASYNC: Stream start failed (codecState=%d)", codecState);
        DBGT_EPILOG("");
        return OMX_ErrorHardware;
    }

#ifdef USE_TEMP_OUTPUTBUFFER
    memcpy(outputBuffer->header->pBuffer, outputStream.bus_data, outputStream.streamlen);
#endif
    outputBuffer->header->nOffset    = 0;
    outputBuffer->header->nFilledLen = outputStream.streamlen;
    outputBuffer->header->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;

    pEnc->streamStarted = OMX_TRUE;

    if (HantroOmx_port_is_tunneled(&pEnc->outputPort))
    {
        ((OMX_COMPONENTTYPE*)pEnc->outputPort.tunnelcomp)->EmptyThisBuffer(pEnc->outputPort.tunnelcomp, outputBuffer->header);
    }
    else
    {
        pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, outputBuffer->header);
    }

#ifdef USE_TEMP_OUTPUTBUFFER
    tempBuffer->offset = 0;
    tempBuffer->size = 0 ;
#endif

    HantroOmx_port_lock_buffers(&pEnc->outputPort);
    HantroOmx_port_pop_buffer(&pEnc->outputPort);
    HantroOmx_port_unlock_buffers(&pEnc->outputPort);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
 }

 /**
 * OMX_ERRORTYPE async_end_stream(OMX_ENCODER* pEnc)
 */
static
OMX_ERRORTYPE async_end_stream(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    BUFFER*         outputBuffer        = NULL;
    STREAM_BUFFER   outputStream;
    OMX_ERRORTYPE   err                 = OMX_ErrorNone;
#ifdef USE_TEMP_OUTPUTBUFFER
    FRAME_BUFFER*   tempBuffer          = &pEnc->frame_out;
#endif

    err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_lock_buffers (out) (err=%x)", err);
        DBGT_EPILOG("");
        return err;
    }
    HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
    HantroOmx_port_unlock_buffers(&pEnc->outputPort);

    OMX_S32 retry = MAX_RETRIES; // needed for stagefright when video is paused (no transition to pause)
    while (outputBuffer == NULL && retry > 0 && pEnc->state != OMX_StatePause && pEnc->statetrans
                != OMX_StatePause && pEnc->statetrans != OMX_StateIdle)
    {
        OSAL_ThreadSleep(RETRY_INTERVAL);

        err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("HantroOmx_port_lock_buffers(out) (err=%x)", err);
            return err;
        }

        HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);
        retry--;
    }

    if (outputBuffer == NULL)
    {
        DBGT_CRITICAL("ASYNC: there's no output buffer!");
        DBGT_EPILOG("");
        return OMX_ErrorOverflow;
    }

    /* DBGT_ASSERT(outputBuffer->bus_data != NULL);
    DBGT_ASSERT(outputBuffer->bus_address != 0);
    DBGT_ASSERT(outputBuffer->header->pBuffer == outputBuffer->bus_data); */

    memset(&outputStream, 0, sizeof(STREAM_BUFFER));
#ifdef USE_TEMP_OUTPUTBUFFER
    DBGT_PDEBUG("ASYNC: Using temp output buffer");
    outputStream.buf_max_size = tempBuffer->capacity;
    outputStream.bus_data = tempBuffer->bus_data;
    outputStream.bus_address = tempBuffer->bus_address;
#else
    DBGT_PDEBUG("ASYNC: outputBuffer->bus_data %p outputBuffer->header->pBuffer %p",
                outputBuffer->bus_data, outputBuffer->header->pBuffer);
    outputStream.buf_max_size = outputBuffer->header->nAllocLen;
    outputStream.bus_data = outputBuffer->bus_data;
    outputStream.bus_address = outputBuffer->bus_address;
#endif

    CODEC_STATE codecState = CODEC_ERROR_UNSPECIFIED;
    codecState = pEnc->codec->stream_end(pEnc->codec, &outputStream);
    if (codecState != CODEC_OK)
    {
        DBGT_CRITICAL("ASYNC: Stream end failed (codecState=%d)", codecState);
        DBGT_EPILOG("");
        return OMX_ErrorHardware;
    }

#ifdef USE_TEMP_OUTPUTBUFFER
    memcpy(outputBuffer->header->pBuffer, outputStream.bus_data, outputStream.streamlen);
#endif
    outputBuffer->header->nOffset    = 0;
    outputBuffer->header->nFilledLen = outputStream.streamlen;
    outputBuffer->header->nFlags |= OMX_BUFFERFLAG_EOS;

    pEnc->streamStarted = OMX_FALSE;

    if (HantroOmx_port_is_tunneled(&pEnc->outputPort))
    {
        ((OMX_COMPONENTTYPE*)pEnc->outputPort.tunnelcomp)->EmptyThisBuffer(pEnc->outputPort.tunnelcomp, outputBuffer->header);
    }
    else
    {
        pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, outputBuffer->header);
    }

#ifdef USE_TEMP_OUTPUTBUFFER
    tempBuffer->offset = 0;
    tempBuffer->size = 0 ;
#endif
    HantroOmx_port_lock_buffers(&pEnc->outputPort);
    HantroOmx_port_pop_buffer(&pEnc->outputPort);
    HantroOmx_port_unlock_buffers(&pEnc->outputPort);

    DBGT_EPILOG("");

    pEnc->in_frame_id = 0;
    pEnc->out_frame_id = 0;

    return OMX_ErrorNone;
 }


#ifdef OMX_ENCODER_VIDEO_DOMAIN
/**
 * OMX_ERRORTYPE async_video_encoder_encode(OMX_ENCODER* pEnc)
 * 1. Get next input buffer from buffer queue
 * 1.1 if stream is started call codec start stream
 * 2. Check that we have complete frame
 * 3. Make copy of inputbuffer if buffer is not allocated by us
 * 4. encode
 * 5 when EOS flag is received call stream end
 */
static
OMX_ERRORTYPE async_video_encoder_encode(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(pEnc->codec);
    BUFFER* inputBuffer         = NULL;
    OMX_ERRORTYPE err           = OMX_ErrorNone;
    OMX_U32 frameSize           = 0;

    err =  calculate_frame_size(pEnc, &frameSize);

    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("Error calculating frame size (err=%x)", err);
        goto INVALID_STATE;
    }

    perf_start(pEnc);

    if (pEnc->streamStarted == OMX_FALSE)
    {
        err = async_start_stream(pEnc);
        if (err != OMX_ErrorNone)
        {
            if (err == OMX_ErrorOverflow)
            {
                DBGT_CRITICAL("ASYNC: firing OMX_EventErrorOverflow");
                pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorOverflow, 0, NULL);
                DBGT_EPILOG("");
                return OMX_ErrorNone;
            }
            else
            {
                DBGT_CRITICAL("async_start_stream (err=%x)", err);
                goto INVALID_STATE;
            }
        }
    }

    err = HantroOmx_port_lock_buffers(&pEnc->inputPort);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_lock_buffers (err=%x)", err);
        goto INVALID_STATE;
    }

    //for re-ordering management
    if(pEnc->enc_next_pic_id == 0xFFFFFFFF)
        HantroOmx_port_get_buffer(&pEnc->inputPort, &inputBuffer);
    else
        HantroOmx_port_get_buffer_by_frame_id(&pEnc->inputPort, &inputBuffer, pEnc->enc_next_pic_id);

    HantroOmx_port_unlock_buffers(&pEnc->inputPort);

    //For conformance tester
    if (!inputBuffer)
    {
        if (pEnc->input_eos == OMX_FALSE)
        {
            DBGT_EPILOG("No input buffer");
            return OMX_ErrorNone;
        }

        //do eos processing when input is eos and can't find target input frame.
        DBGT_PDEBUG("ASYNC: encoding EOS processing");
        err = async_end_stream(pEnc);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_end_stream (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        async_encoder_return_buffers(pEnc, &pEnc->inputPort);
        async_encoder_return_buffers(pEnc, &pEnc->outputPort);
        
        wait_supplied_buffers(pEnc, &pEnc->inputPort);
        wait_supplied_buffers(pEnc, &pEnc->outputPort);
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventBufferFlag, PORT_INDEX_OUTPUT, OMX_BUFFERFLAG_EOS, NULL);

		DBGT_EPILOG("");
        return OMX_ErrorNone;
    }
    pEnc->enc_curr_pic_id = pEnc->enc_next_pic_id;

    FRAME_BUFFER*   tempBuffer = &pEnc->frame_in;

    // if there is previous data in the frame buffer left over from a previous call to encode
    // or if the buffer has possibly misaligned offset or if the buffer is allocated by the client
    // invoke the decoding through the temporary frame buffer
#ifdef USE_STAB_TEMP_INPUTBUFFER
    if (inputBuffer->header->nOffset != 0 ||
         tempBuffer->size != 0 ||
         !(inputBuffer->flags & BUFFER_FLAG_MY_BUFFER) ||
         pEnc->encConfig.stab.bStab)
#else
    if (inputBuffer->header->nOffset != 0 ||
         tempBuffer->size != 0 ||
         !(inputBuffer->flags & BUFFER_FLAG_MY_BUFFER))
#endif
    {
        DBGT_PDEBUG("ASYNC: Using temp input buffer");
        DBGT_PDEBUG("inputBuffer->header->nOffset %d", (int)inputBuffer->header->nOffset);
        DBGT_PDEBUG("tempBuffer->size %d", (int)tempBuffer->size);
        DBGT_PDEBUG("Input buffer flag 'My buffer': %s", (inputBuffer->flags & BUFFER_FLAG_MY_BUFFER) ?
                    "YES" : "NO");
        DBGT_PDEBUG("pEnc->encConfig.stab.bStab %d", pEnc->encConfig.stab.bStab);

        OMX_U32 capacity  = tempBuffer->capacity;
        OMX_U32 needed    = inputBuffer->header->nFilledLen;

        DBGT_PDEBUG("inputBuffer->header->pBuffer %p", inputBuffer->header->pBuffer);
        DBGT_PDEBUG("capacity %d needed %d tempBuffer->size %d", (int)capacity, (int)needed, (int)tempBuffer->size);

        while (1)
        {
            OMX_U32 available = capacity - tempBuffer->size;

            if (available >= needed)
                break;
            capacity *= 2;
        }

        if (capacity >  tempBuffer->capacity)
        {
            err = grow_frame_buffer(pEnc, tempBuffer, capacity);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                goto INVALID_STATE;
            }
        }

        DBGT_ASSERT(tempBuffer->capacity - tempBuffer->size >= inputBuffer->header->nFilledLen);

        OMX_U32 len = inputBuffer->header->nFilledLen;
        OMX_U8* src = inputBuffer->header->pBuffer + inputBuffer->header->nOffset;
        OMX_U8* dst = tempBuffer->bus_data + tempBuffer->size; // append to the buffer

        memcpy(dst, src, len);
        tempBuffer->size += len;

        OMX_U32 retlen = 0;

        if (pEnc->encConfig.stab.bStab)
        {
            if (tempBuffer->size >= frameSize * 2)
            {
                err = async_encode_stabilized_data(pEnc, tempBuffer->bus_data, tempBuffer->bus_address,
                        tempBuffer->size, inputBuffer, &retlen, frameSize);
                if (err != OMX_ErrorNone)
                {
                    if (err == OMX_ErrorOverflow)
                    {
                        DBGT_CRITICAL("ASYNC: firing OMX_EventErrorOverflow");
                        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorOverflow, 0, NULL);
                    }
                    else if (err == OMX_ErrorBadParameter)
                    {
                        DBGT_CRITICAL("ASYNC: firing OMX_ErrorBadParameter");
                        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorBadParameter, 0, NULL);
                    }
                    else
                        goto INVALID_STATE;
                }
                tempBuffer->size = retlen;
            }
        }
        else if (tempBuffer->size >= frameSize)
        {
           DBGT_PDEBUG("tempBuffer->bus_data = %p, tempBuffer->bus_address = 0x%lx, tempBuffer->size = %d, frameSize = %d",
                    tempBuffer->bus_data, tempBuffer->bus_address, (int)tempBuffer->size, (int)frameSize);

            err = async_encode_video_data(pEnc, tempBuffer->bus_data, tempBuffer->bus_address, tempBuffer->size, inputBuffer, &retlen, frameSize);
            if (err != OMX_ErrorNone)
            {
                if (err == OMX_ErrorOverflow)
                {
                    DBGT_CRITICAL("ASYNC: firing OMX_EventErrorOverflow");
                    pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorOverflow, 0, NULL);
                }
                else if (err == OMX_ErrorBadParameter)
                {
                    DBGT_CRITICAL("ASYNC: firing OMX_ErrorBadParameter");
                    pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorBadParameter, 0, NULL);
                }
                else
                    goto INVALID_STATE;
            }
            tempBuffer->size = retlen;
        }
    }
    else
    {
        DBGT_ASSERT(inputBuffer->header->nOffset == 0);
        DBGT_ASSERT(inputBuffer->bus_data == inputBuffer->header->pBuffer);
        DBGT_ASSERT(inputBuffer->flags & BUFFER_FLAG_MY_BUFFER);
        DBGT_PDEBUG("pEnc->encConfig.stab.bStab %d", pEnc->encConfig.stab.bStab);

        if (pEnc->encConfig.stab.bStab)
        {
            DBGT_PDEBUG("inputBuffer->header->pBuffer %p", inputBuffer->header->pBuffer);
            // Get next input buffer for stabilization
            if (!(inputBuffer->header->nFlags & OMX_BUFFERFLAG_EOS))
            {
                BUFFER* stabBuffer = NULL;

                err = HantroOmx_port_lock_buffers(&pEnc->inputPort);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("HantroOmx_port_lock_buffers (err=%x)", err);
                    //goto INVALID_STATE;
                }

                HantroOmx_port_get_buffer_at(&pEnc->inputPort, &stabBuffer, 1);
                HantroOmx_port_unlock_buffers(&pEnc->inputPort);
                if (!stabBuffer)
                {
                    DBGT_EPILOG("No video stabilization buffer");
					DBGT_EPILOG("");
                    return OMX_ErrorNone;
                }

                pEnc->busLumaStab = stabBuffer->bus_address;
                pEnc->busDataStab = stabBuffer->bus_data;
                DBGT_PDEBUG("stabBuffer->header->pBuffer %p", stabBuffer->header->pBuffer);
                DBGT_PDEBUG("pEnc->busLumaStab 0x%08lx", pEnc->busLumaStab);
            }
        }

        OMX_U8* bus_data = inputBuffer->bus_data;
        OSAL_BUS_WIDTH bus_address = inputBuffer->bus_address;
        OMX_U32 filledlen = inputBuffer->header->nFilledLen;
        OMX_U32 retlen    = 0;

        if (filledlen >= frameSize)
        {
            err = async_encode_video_data(pEnc, bus_data, bus_address, filledlen, inputBuffer, &retlen, frameSize);
        }

        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_encode_video_data (err=%x)", err);
            goto INVALID_STATE;
        }

        if (retlen > 0)
        {
            OMX_U8* dst = tempBuffer->bus_data + tempBuffer->size;
            OMX_U8* src = bus_data;
            OMX_U32 capacity = tempBuffer->capacity;
            OMX_U32 needed   = retlen;
            while (1)
            {
                OMX_U32 available = capacity - tempBuffer->size;
                if (available > needed)
                    break;
                capacity *= 2;
            }
            if (capacity != tempBuffer->capacity)
            {
                err = grow_frame_buffer(pEnc, tempBuffer, capacity);
                if (err != OMX_ErrorNone)
                {
                    if (err == OMX_ErrorOverflow)
                    {
                        DBGT_CRITICAL("ASYNC: firing OMX_EventErrorOverflow");
                        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorOverflow, 0, NULL);
                    }
                    else
                        goto INVALID_STATE;
                }
            }

            memcpy(dst, src, retlen);
            tempBuffer->size += retlen;
        }
    }

    perf_stop(pEnc);


    if (inputBuffer->header->nFlags & OMX_BUFFERFLAG_EOS)
    {
    	if((pEnc->enc_next_pic_id != 0xFFFFFFFF) && (pEnc->enc_next_pic_id >= inputBuffer->frame_id))
    	{
            DBGT_PDEBUG("ASYNC: got buffer EOS flag");
            err = async_end_stream(pEnc);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("async_end_stream (err=%x)", err);
                DBGT_EPILOG("");
                return err;
            }
			async_encoder_return_buffers(pEnc, &pEnc->inputPort);
			async_encoder_return_buffers(pEnc, &pEnc->outputPort);
			
			wait_supplied_buffers(pEnc, &pEnc->inputPort);
			wait_supplied_buffers(pEnc, &pEnc->outputPort);
            pEnc->app_callbacks.EventHandler(pEnc->self, 
                                             pEnc->app_data, 
                                             OMX_EventBufferFlag, 
                                             PORT_INDEX_OUTPUT, 
                                             inputBuffer->header->nFlags, 
                                             NULL);
    	}

		DBGT_EPILOG("");
        return OMX_ErrorNone;
    }

    if (inputBuffer->header->hMarkTargetComponent == pEnc->self)
    {
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventMark, 0, 0, inputBuffer->header->pMarkData);
        inputBuffer->header->hMarkTargetComponent = NULL;
        inputBuffer->header->pMarkData            = NULL;
    }

    HantroOmx_port_lock_buffers(&pEnc->inputPort);
    //for re-ordering management
    if(pEnc->enc_curr_pic_id == 0xFFFFFFFF)
	{
		HantroOmx_port_unlock_buffers(&pEnc->inputPort);
		if (HantroOmx_port_is_tunneled(&pEnc->inputPort))
		{
			((OMX_COMPONENTTYPE*)pEnc->inputPort.tunnelcomp)->FillThisBuffer(pEnc->inputPort.tunnelcomp, inputBuffer->header);
		}
		else
		{
			pEnc->app_callbacks.EmptyBufferDone(pEnc->self, pEnc->app_data, inputBuffer->header);
		}
		HantroOmx_port_lock_buffers(&pEnc->inputPort);
        HantroOmx_port_pop_buffer(&pEnc->inputPort);
	}
    else
    {
    	OMX_U32 pic_id;
		BUFFER *buff;
		pic_id = pEnc->enc_curr_pic_id;
		if(pEnc->enc_next_pic_id != 0xFFFFFFFF)
		{
			pic_id = pEnc->enc_next_pic_id;
		}
		if(pic_id > OMX_MAX_GOP_SIZE)
		{
			pic_id -= OMX_MAX_GOP_SIZE;
			do
			{
				buff = HantroOmx_port_pop_buffer_by_frame_id(&pEnc->inputPort, pic_id);
				if(buff == NULL)
				{
					break;
				}
				HantroOmx_port_unlock_buffers(&pEnc->inputPort);
				if (HantroOmx_port_is_tunneled(&pEnc->inputPort))
				{
					((OMX_COMPONENTTYPE*)pEnc->inputPort.tunnelcomp)->FillThisBuffer(pEnc->inputPort.tunnelcomp, buff->header);
				}
				else
				{
					pEnc->app_callbacks.EmptyBufferDone(pEnc->self, pEnc->app_data, buff->header);
				}
				HantroOmx_port_lock_buffers(&pEnc->inputPort);
			}while(--pic_id);
		}
    }
    HantroOmx_port_unlock_buffers(&pEnc->inputPort);

    DBGT_EPILOG("");
    return OMX_ErrorNone;

 INVALID_STATE:
    DBGT_ASSERT(err != OMX_ErrorNone);

    DBGT_CRITICAL("ASYNC: error while processing buffers: %s", HantroOmx_str_omx_err(err));

    pEnc->state = OMX_StateInvalid;
    pEnc->run   = OMX_FALSE;
    pEnc->app_callbacks.EventHandler(
        pEnc->self,
        pEnc->app_data,
        OMX_EventError,
        err,
        0,
        NULL);
    DBGT_EPILOG("");
    return err;
}

/**
 * static OMX_ERRORTYPE async_encode_stabilized_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* buff, OMX_U32* retlen, OMX_U32 frameSize)
 */
static
OMX_ERRORTYPE async_encode_stabilized_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* inputBuffer, OMX_U32* retlen, OMX_U32 frameSize)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc);
    DBGT_ASSERT(retlen);

    FRAME           frame;          //Frame for codec
    STREAM_BUFFER   stream;         //Encoder stream from codec
    BUFFER*         outputBuffer    = NULL;         //Output buffer from client
    OMX_ERRORTYPE   err             = OMX_ErrorNone;
    OMX_U32         i, dataSize;
#ifdef USE_TEMP_OUTPUTBUFFER
    FRAME_BUFFER*   tempBuffer      = &pEnc->frame_out;
#endif

    while (datalen >= frameSize * 2)
    {
        err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("HantroOmx_port_lock_buffers (out) (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);

        OMX_S32 retry = MAX_RETRIES;
        while (outputBuffer == NULL && retry > 0 && pEnc->state != OMX_StatePause && pEnc->statetrans
                    != OMX_StatePause && pEnc->statetrans != OMX_StateIdle)
        {
            OSAL_ThreadSleep(RETRY_INTERVAL);

            err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("HantroOmx_port_lock_buffers(out) (err=%x)", err);
                return err;
            }

            HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
            HantroOmx_port_unlock_buffers(&pEnc->outputPort);
            retry--;
        }

        if (outputBuffer == NULL)
        {
            DBGT_CRITICAL("ASYNC: there's no output buffer!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }

#ifdef USE_TEMP_OUTPUTBUFFER
        if (tempBuffer->capacity < outputBuffer->header->nAllocLen)
        {
            err = grow_frame_buffer(pEnc, tempBuffer, outputBuffer->header->nAllocLen);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                DBGT_EPILOG("");
                return err;
            }
        }
#endif
        frame.fb_bus_data    = bus_data;
        frame.fb_bus_address = bus_address;
        frame.fb_frameSize = frameSize;
        frame.fb_bufferSize = 2 * frame.fb_frameSize;
        frame.bitrate = pEnc->outputPort.def.format.video.nBitrate;

        switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
        {
            case OMX_VIDEO_CodingAVC:
                {
                    //DBGT_PDEBUG("ASYNC: pEnc->encConfig.avc.nPFrames: %d", (unsigned) pEnc->encConfig.avc.nPFrames);
                    if (pEnc->encConfig.avc.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
            case OMX_VIDEO_CodingMPEG4:
                {
                    //DBGT_PDEBUG("ASYNC: pEnc->encConfig.mpeg4.nPFrames: %d", (unsigned) pEnc->encConfig.mpeg4.nPFrames);
                    if (pEnc->encConfig.mpeg4.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
            case OMX_VIDEO_CodingH263:
                {
                    frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#ifdef ENCH1
            case OMX_VIDEO_CodingVP8:
                {
                    frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#endif
#if defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
            case OMX_VIDEO_CodingHEVC:
                {
                    if (pEnc->encConfig.hevc.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#endif

            default:
                DBGT_ERROR("Illegal compression format!");
            break;
        }

        if (pEnc->encConfig.intraRefreshVop.IntraRefreshVOP == OMX_TRUE)
        {
            DBGT_PDEBUG("ASYNC: Intra frame refresh");
            frame.frame_type = OMX_INTRA_FRAME;
            pEnc->encConfig.intraRefreshVop.IntraRefreshVOP = OMX_FALSE;
        }

        memset(&stream, 0, sizeof(STREAM_BUFFER));
#ifdef USE_TEMP_OUTPUTBUFFER
        DBGT_PDEBUG("ASYNC: Using temp output buffer");
        stream.buf_max_size = tempBuffer->capacity;
        stream.bus_data = tempBuffer->bus_data;
        stream.bus_address = tempBuffer->bus_address;
#else
        DBGT_PDEBUG("ASYNC: outputBuffer->bus_data %p outputBuffer->header->pBuffer %p",
                    outputBuffer->bus_data, outputBuffer->header->pBuffer);
        stream.buf_max_size = outputBuffer->header->nAllocLen;
        stream.bus_data = outputBuffer->bus_data;
        stream.bus_address = outputBuffer->bus_address;
#endif
        CODEC_STATE codecState = CODEC_ERROR_UNSPECIFIED;

        codecState = pEnc->codec->encode(pEnc->codec, &frame, &stream, &pEnc->encConfig);

        switch (codecState)
        {
        case CODEC_OK:
            break;
        case CODEC_CODED_INTRA:
            break;
        case CODEC_CODED_PREDICTED:
            break;
        case CODEC_CODED_SLICE:
            break;
        case CODEC_ERROR_HW_TIMEOUT:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_TIMEOUT");
            DBGT_EPILOG("");
            return OMX_ErrorTimeout;
        case CODEC_ERROR_HW_BUS_ERROR:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_BUS_ERROR");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_HW_RESET:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_RESET");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_SYSTEM:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_SYSTEM");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_RESERVED:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_RESERVED");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_INVALID_ARGUMENT:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_INVALID_ARGUMENT");
            DBGT_EPILOG("");
            return OMX_ErrorBadParameter;
        case CODEC_ERROR_BUFFER_OVERFLOW:
            DBGT_CRITICAL("Output buffer size is too small");
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_BUFFER_OVERFLOW");
            DBGT_EPILOG("");
            return OMX_ErrorBadParameter;
        default:
            DBGT_CRITICAL("pEnc->codec->encode() returned undefined error: %d", codecState);
            DBGT_EPILOG("");
            return OMX_ErrorUndefined;
        }

        //Move frame start bit to the beginning of the frame buffer
        OMX_U32 rem = datalen - frame.fb_frameSize;

        memmove(bus_data, bus_data + frame.fb_frameSize, rem);
        datalen -=  frame.fb_frameSize;

        DBGT_PDEBUG("ASYNC: output buffer size: %d", (int)outputBuffer->header->nAllocLen);
        if (stream.streamlen > outputBuffer->header->nAllocLen)
        {
            DBGT_CRITICAL("ASYNC: output buffer is too small!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }

        if ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat == OMX_VIDEO_CodingVP8)
        {
            dataSize = 0;
            // copy data from each partition to the output buffer
            for(i = 0; i < 9; i++)
            {
                DBGT_PDEBUG("Partition %lu, size %lu", i, stream.streamSize[i]);
                if ((stream.streamSize[i] <= outputBuffer->header->nAllocLen) &&
                    (dataSize <= outputBuffer->header->nAllocLen))
                {
                    memmove(outputBuffer->header->pBuffer + dataSize, stream.pOutBuf[i], stream.streamSize[i]);
                }
                else
                {
                    DBGT_CRITICAL("ASYNC: output buffer is too small!");
                    DBGT_EPILOG("");
                    return OMX_ErrorOverflow;
                }

                dataSize += stream.streamSize[i];
            }
        }
#ifdef USE_TEMP_OUTPUTBUFFER
        else
            memcpy(outputBuffer->header->pBuffer, stream.bus_data, stream.streamlen);
#endif

        outputBuffer->header->nOffset    = 0;
        outputBuffer->header->nFilledLen = stream.streamlen;
        outputBuffer->header->nTimeStamp = inputBuffer->header->nTimeStamp;
        outputBuffer->header->nFlags     = inputBuffer->header->nFlags & ~OMX_BUFFERFLAG_EOS;

        // mark intra frame as sync frame
        if (codecState == CODEC_CODED_INTRA)
        {
            DBGT_PDEBUG("ASYNC: Sync frame");
            outputBuffer->header->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }

        DBGT_PDEBUG("ASYNC: output buffer OK");

        OMX_S32 markcount = pEnc->mark_write_pos - pEnc->mark_read_pos;
        if (markcount)
        {
            DBGT_PDEBUG("ASYNC: got %d marks pending", (int)markcount);
            outputBuffer->header->hMarkTargetComponent = pEnc->marks[pEnc->mark_read_pos].hMarkTargetComponent;
            outputBuffer->header->pMarkData            = pEnc->marks[pEnc->mark_read_pos].pMarkData;
            pEnc->mark_read_pos++;
            if (--markcount == 0)
            {
                pEnc->mark_read_pos  = 0;
                pEnc->mark_write_pos = 0;
                DBGT_PDEBUG("ASYNC: mark buffer empty");
            }
        }
        else
        {
            outputBuffer->header->hMarkTargetComponent = inputBuffer->header->hMarkTargetComponent;
            outputBuffer->header->pMarkData            = inputBuffer->header->pMarkData;
        }

        HantroOmx_port_lock_buffers(&pEnc->outputPort);
        HantroOmx_port_pop_buffer(&pEnc->outputPort);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);

        if (HantroOmx_port_is_tunneled(&pEnc->outputPort))
        {
            ((OMX_COMPONENTTYPE*)pEnc->outputPort.tunnelcomp)->EmptyThisBuffer(pEnc->outputPort.tunnelcomp, outputBuffer->header);
        }
        else
        {
            pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, outputBuffer->header);
        }
    }

    *retlen = datalen;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

/**
 * OMX_ERRORTYPE async_encode_video_data(OMX_ENCODER* pEnc,
 *                                  OMX_U8* bus_data,
 *                                  OSAL_BUS_WIDTH bus_address,
 *                                  OMX_U32 datalen, BUFFER* buff,
                                    OMX_U32* retlen)
 */
static
OMX_ERRORTYPE async_encode_video_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* inputBuffer, OMX_U32* retlen, OMX_U32 frameSize)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc);
    DBGT_ASSERT(retlen);

    FRAME           frame;       //Frame for codec
    STREAM_BUFFER   stream;      //Encoder stream from codec
    BUFFER*         outputBuffer        = NULL;         //Output buffer from client
    OMX_ERRORTYPE   err                 = OMX_ErrorNone;
    OMX_U32         i, dataSize;
#ifdef USE_TEMP_OUTPUTBUFFER
    FRAME_BUFFER*   tempBuffer          = &pEnc->frame_out;
#endif

    while (datalen >= frameSize)
    {
        err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("HantroOmx_port_lock_buffers (out) (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);

        OMX_S32 retry = MAX_RETRIES;
        while (outputBuffer == NULL && retry > 0 && pEnc->state != OMX_StatePause && pEnc->statetrans
                    != OMX_StatePause && pEnc->statetrans != OMX_StateIdle)
        {
            OSAL_ThreadSleep(RETRY_INTERVAL);

            err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("HantroOmx_port_lock_buffers(out) (err=%x)", err);
                return err;
            }

            HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
            HantroOmx_port_unlock_buffers(&pEnc->outputPort);
            retry--;
        }

        if (outputBuffer == NULL)
        {
            DBGT_CRITICAL("ASYNC: there's no output buffer!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }
#ifdef USE_TEMP_OUTPUTBUFFER
        if (tempBuffer->capacity < outputBuffer->header->nAllocLen)
        {
            err = grow_frame_buffer(pEnc, tempBuffer, outputBuffer->header->nAllocLen);

            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                DBGT_EPILOG("");
                return err;
            }
        }
#endif
        frame.fb_bus_data       = bus_data; // + offset
        frame.fb_bus_address    = bus_address; // + offset
        frame.fb_frameSize      = frameSize;
        frame.fb_bufferSize     = frame.fb_frameSize;
        frame.bitrate           = pEnc->outputPort.def.format.video.nBitrate;
        frame.bus_lumaStab      = pEnc->busLumaStab;
        DBGT_PDEBUG("pEnc->busLumaStab 0x%08lx", pEnc->busLumaStab);

        switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
        {
            case OMX_VIDEO_CodingAVC:
                {
                    //DBGT_PDEBUG("ASYNC: pEnc->encConfig.avc.nPFrames: %u", (unsigned)pEnc->encConfig.avc.nPFrames);
                    if (pEnc->encConfig.avc.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
            case OMX_VIDEO_CodingMPEG4:
                {
                    //DBGT_PDEBUG("ASYNC: pEnc->encConfig.mpeg4.nPFrames: %u", (unsigned) pEnc->encConfig.mpeg4.nPFrames);
                    if (pEnc->encConfig.mpeg4.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
            case OMX_VIDEO_CodingH263:
                {
                    frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#ifdef ENCH1
            case OMX_VIDEO_CodingVP8:
                {
                    frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#endif
#if defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
            case OMX_VIDEO_CodingHEVC:
                {
                    if (pEnc->encConfig.hevc.nPFrames == 0)
                        frame.frame_type = OMX_INTRA_FRAME;
                    else
                        frame.frame_type = OMX_PREDICTED_FRAME;
                }
                break;
#endif

            default:
                DBGT_ERROR("Illegal compression format!");
            break;
        }

        if (pEnc->encConfig.intraRefreshVop.IntraRefreshVOP == OMX_TRUE)
        {
            DBGT_PDEBUG("ASYNC: Intra frame refresh");
            frame.frame_type = OMX_INTRA_FRAME;
            pEnc->encConfig.intraRefreshVop.IntraRefreshVOP = OMX_FALSE;
        }

        memset(&stream, 0, sizeof(STREAM_BUFFER));
        stream.next_input_frame_id = 0xFFFFFFFF;
#ifdef USE_TEMP_OUTPUTBUFFER
        DBGT_PDEBUG("ASYNC: Using temp output buffer");
        stream.buf_max_size = tempBuffer->capacity;
        stream.bus_data = tempBuffer->bus_data;
        stream.bus_address = tempBuffer->bus_address;
#else
        DBGT_PDEBUG("ASYNC: outputBuffer->bus_data %p outputBuffer->header->pBuffer %p",
                    outputBuffer->bus_data, outputBuffer->header->pBuffer);
        stream.buf_max_size = outputBuffer->header->nAllocLen;
        stream.bus_data = outputBuffer->bus_data;
        stream.bus_address = outputBuffer->bus_address;
#endif
        CODEC_STATE codecState = CODEC_ERROR_UNSPECIFIED;

        codecState = pEnc->codec->encode(pEnc->codec, &frame, &stream, &pEnc->encConfig);

        switch (codecState)
        {
        case CODEC_OK:
            break;
        case CODEC_CODED_INTRA:
            break;
        case CODEC_CODED_PREDICTED:
            break;
		case CODEC_CODED_BIDIR_PREDICTED:
			break;
        case CODEC_CODED_SLICE:
            break;
        case CODEC_ENQUEUE:
            break;
        case CODEC_ERROR_HW_TIMEOUT:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_TIMEOUT");
            DBGT_EPILOG("");
            return OMX_ErrorTimeout;
        case CODEC_ERROR_HW_BUS_ERROR:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_BUS_ERROR");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_HW_RESET:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_HW_RESET");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_SYSTEM:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_SYSTEM");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_RESERVED:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_RESERVED");
            DBGT_EPILOG("");
            return OMX_ErrorHardware;
        case CODEC_ERROR_INVALID_ARGUMENT:
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_INVALID_ARGUMENT");
            DBGT_EPILOG("");
            return OMX_ErrorBadParameter;
        case CODEC_ERROR_BUFFER_OVERFLOW:
            DBGT_CRITICAL("Output buffer size is too small");
            DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_BUFFER_OVERFLOW");
            DBGT_EPILOG("");
            return OMX_ErrorBadParameter;
        default:
            DBGT_CRITICAL("pEnc->codec->encode() returned undefined error: %d", codecState);
            DBGT_EPILOG("");
            return OMX_ErrorUndefined;
        }

        pEnc->enc_next_pic_id = stream.next_input_frame_id;

        OMX_U32 rem = datalen - frame.fb_frameSize;

        memmove(bus_data, bus_data + frame.fb_frameSize, rem);
        datalen -=  frame.fb_frameSize;

        DBGT_PDEBUG("ASYNC: output buffer size: %d", (int)outputBuffer->header->nAllocLen);
        if (stream.streamlen > outputBuffer->header->nAllocLen)
        {
            DBGT_CRITICAL("ASYNC: output buffer is too small!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }

        if ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat == OMX_VIDEO_CodingVP8)
        {
            dataSize = 0;
            // copy data from each partition to the output buffer
            for(i = 0; i < 9; i++)
            {
                DBGT_PDEBUG("Partition %lu, size %lu", i, stream.streamSize[i]);
                if (stream.streamSize[i] <= outputBuffer->header->nAllocLen &&
                    dataSize <= outputBuffer->header->nAllocLen)
                {
                    memmove(outputBuffer->header->pBuffer + dataSize, stream.pOutBuf[i], stream.streamSize[i]);
                }
                else
                {
                    DBGT_CRITICAL("ASYNC: output buffer is too small!");
                    DBGT_EPILOG("");
                    return OMX_ErrorOverflow;
                }

                dataSize += stream.streamSize[i];
            }
        }
#ifdef USE_TEMP_OUTPUTBUFFER
        else
            memcpy(outputBuffer->header->pBuffer, stream.bus_data, stream.streamlen);
#endif

        outputBuffer->header->nOffset    = 0;
        outputBuffer->header->nFilledLen = stream.streamlen;
        outputBuffer->header->nTimeStamp = inputBuffer->header->nTimeStamp;
        outputBuffer->header->nFlags     = inputBuffer->header->nFlags & ~OMX_BUFFERFLAG_EOS;

        // mark intra frame as sync frame
        if (codecState == CODEC_CODED_INTRA)
        {
            DBGT_PDEBUG("ASYNC: Sync frame");
            outputBuffer->header->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }

#ifdef ENCH1
        // mark vp8 temporal layer
        if (((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat == OMX_VIDEO_CodingVP8) &&
            pEnc->encConfig.temporalLayer.nBaseLayerBitrate)
        {
            switch (stream.layerId)
            {
            case 0:
                outputBuffer->header->nFlags |= OMX_BUFFERFLAG_BASE_LAYER;
                break;
            case 1:
                outputBuffer->header->nFlags |= OMX_BUFFERFLAG_FIRST_LAYER;
                break;
            case 2:
                outputBuffer->header->nFlags |= OMX_BUFFERFLAG_SECOND_LAYER;
                break;
            case 3:
                outputBuffer->header->nFlags |= OMX_BUFFERFLAG_THIRD_LAYER;
                break;
            default:
                DBGT_CRITICAL("Unknown VP8 temporal layer ID: %d", (int)stream.layerId);
                DBGT_EPILOG("");
                return OMX_ErrorBadParameter;
            }
        }
#endif
        DBGT_PDEBUG("ASYNC: output buffer OK");

        OMX_S32 markcount = pEnc->mark_write_pos - pEnc->mark_read_pos;
        if (markcount)
        {
            DBGT_PDEBUG("ASYNC: got %d marks pending", (int)markcount);
            outputBuffer->header->hMarkTargetComponent = pEnc->marks[pEnc->mark_read_pos].hMarkTargetComponent;
            outputBuffer->header->pMarkData            = pEnc->marks[pEnc->mark_read_pos].pMarkData;
            pEnc->mark_read_pos++;
            if (--markcount == 0)
            {
                pEnc->mark_read_pos  = 0;
                pEnc->mark_write_pos = 0;
                DBGT_PDEBUG("ASYNC: mark buffer empty");
            }
        }
        else
        {
            outputBuffer->header->hMarkTargetComponent = inputBuffer->header->hMarkTargetComponent;
            outputBuffer->header->pMarkData            = inputBuffer->header->pMarkData;
        }

        HantroOmx_port_lock_buffers(&pEnc->outputPort);
        HantroOmx_port_pop_buffer(&pEnc->outputPort);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);

        if (HantroOmx_port_is_tunneled(&pEnc->outputPort))
        {
            ((OMX_COMPONENTTYPE*)pEnc->outputPort.tunnelcomp)->EmptyThisBuffer(pEnc->outputPort.tunnelcomp, outputBuffer->header);
        }
        else
        {
            pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, outputBuffer->header);
        }
    }
    *retlen = datalen;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#if !defined (ENCH2) && !defined (ENCVC8000E) && !defined (ENCH2V41)
static OMX_ERRORTYPE set_avc_intra_period(OMX_ENCODER* pEnc)
{
    DBGT_ASSERT(pEnc);
    DBGT_PROLOG("");

    HantroHwEncOmx_encoder_intra_period_h264(pEnc->codec, pEnc->encConfig.avcIdr.nPFrames);

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}
#endif

static OMX_ERRORTYPE set_frame_rate(OMX_ENCODER* pEnc)
{
    DBGT_ASSERT(pEnc);
    DBGT_PROLOG("");

    switch ((OMX_U32)pEnc->outputPort.def.format.video.eCompressionFormat)
    {
#if !defined (ENCH2) && !defined(ENCVC8000E) && !defined (ENCH2V41)
        case OMX_VIDEO_CodingAVC:
            HantroHwEncOmx_encoder_frame_rate_h264(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif
#if defined (ENC6280) || defined (ENC7280)
        case OMX_VIDEO_CodingH263:
            HantroHwEncOmx_encoder_frame_rate_h263(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
        case OMX_VIDEO_CodingMPEG4:
            HantroHwEncOmx_encoder_frame_rate_mpeg4(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif
#ifdef ENCH1
        case OMX_VIDEO_CodingVP8:
            HantroHwEncOmx_encoder_frame_rate_vp8(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif
#ifdef ENCH2
        case OMX_VIDEO_CodingHEVC:
            HantroHwEncOmx_encoder_frame_rate_hevc(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif
#ifdef ENCVC8000E
        case OMX_VIDEO_CodingAVC:
            HantroHwEncOmx_encoder_frame_rate_codec(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
        case OMX_VIDEO_CodingHEVC:
            HantroHwEncOmx_encoder_frame_rate_codec(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif

#ifdef ENCH2V41
        case OMX_VIDEO_CodingAVC:
            HantroHwEncOmx_encoder_frame_rate_h2v41codec(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
        case OMX_VIDEO_CodingHEVC:
            HantroHwEncOmx_encoder_frame_rate_h2v41codec(pEnc->codec,
                        pEnc->outputPort.def.format.video.xFramerate);
            break;
#endif

        default:
            DBGT_CRITICAL("Unsupported index");
            DBGT_EPILOG("");
            return OMX_ErrorUnsupportedIndex;
    }

    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#endif //~OMX_ENCODER_VIDEO_DOMAIN

#ifdef OMX_ENCODER_IMAGE_DOMAIN
/**
 * OMX_ERRORTYPE async_image_encoder_encode(OMX_ENCODER* pEnc)
 * 1. Get next input buffer from buffer queue
 * 1.1 if stream is started call codec start stream
 * 2. Check that we have complete frame
 * 3. Make copy of inputbuffer if buffer is not allocated by us
 * 4. encode
 * 5 when EOS flag is received call stream end
 */
static
OMX_ERRORTYPE async_image_encoder_encode(OMX_ENCODER* pEnc)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(pEnc->codec);
    BUFFER* inputBuffer         = NULL;
    OMX_ERRORTYPE err           = OMX_ErrorNone;
    OMX_U32 frameSize           = 0;

    err = calculate_frame_size(pEnc, &frameSize);

    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("Error calculating frame size (err=%x)", err);
        goto INVALID_STATE;
    }

    perf_start(pEnc);

    if (pEnc->streamStarted == OMX_FALSE)
    {
        err = async_start_stream(pEnc);
        if (err != OMX_ErrorNone)
        {
            if (err == OMX_ErrorOverflow)
            {
                DBGT_CRITICAL("ASYNC: firing OMX_EventErrorOverflow");
                pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventError, OMX_ErrorOverflow, 0, NULL);
                DBGT_EPILOG("");
                return OMX_ErrorNone;
            }
            else
            {
                DBGT_CRITICAL("async_start_stream (err=%x)", err);
                goto INVALID_STATE;
            }
        }
    }

    err = HantroOmx_port_lock_buffers(&pEnc->inputPort);
    if (err != OMX_ErrorNone)
    {
        DBGT_CRITICAL("HantroOmx_port_lock_buffers (in)");
        goto INVALID_STATE;
    }
    HantroOmx_port_get_buffer(&pEnc->inputPort, &inputBuffer);
    HantroOmx_port_unlock_buffers(&pEnc->inputPort);

      //For conformance tester
    if (!inputBuffer)
    {
        DBGT_EPILOG("");
        return OMX_ErrorNone;
    }
    FRAME_BUFFER*   tempBuffer = &pEnc->frame_in;

    // if there is previous data in the frame buffer left over from a previous call to encode
    // or if the buffer has possibly misaligned offset or if the buffer is allocated by the client
    // invoke the decoding through the temporary frame buffer
    if (inputBuffer->header->nOffset != 0 || tempBuffer->size != 0 ||
        !(inputBuffer->flags & BUFFER_FLAG_MY_BUFFER))
    {
        DBGT_PDEBUG("ASYNC: Using temp input buffer");
        DBGT_PDEBUG("inputBuffer->header->nOffset %d", (int)inputBuffer->header->nOffset);
        DBGT_PDEBUG("tempBuffer->size %d", (int)tempBuffer->size);
        DBGT_PDEBUG("Input buffer flag 'My buffer': %s", (inputBuffer->flags & BUFFER_FLAG_MY_BUFFER) ?
                    "YES" : "NO");

        OMX_U32 capacity  = tempBuffer->capacity;
        OMX_U32 needed    = inputBuffer->header->nFilledLen;
        while (1)
        {
            OMX_U32 available = capacity - tempBuffer->size;

            if (available >= needed)
                break;
            capacity *= 2;
        }
        if (capacity >  tempBuffer->capacity)
        {
            err = grow_frame_buffer(pEnc, tempBuffer, capacity);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                goto INVALID_STATE;
            }
        }

        DBGT_ASSERT(tempBuffer->capacity - tempBuffer->size >= inputBuffer->header->nFilledLen);

        OMX_U32 len = inputBuffer->header->nFilledLen;
        OMX_U8* src = inputBuffer->header->pBuffer + inputBuffer->header->nOffset;
        OMX_U8* dst = tempBuffer->bus_data + tempBuffer->size; // append to the buffer

        memcpy(dst, src, len);
        tempBuffer->size += len;

        OMX_U32 retlen = 0;
        if (tempBuffer->size >= frameSize)
        {
            err = async_encode_image_data(pEnc, tempBuffer->bus_data, tempBuffer->bus_address, tempBuffer->size, inputBuffer, &retlen, frameSize);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("async_encode_image_data (err=%x)", err);
                goto INVALID_STATE;
            }
            tempBuffer->size = retlen;
        }
    }
    else
    {
        DBGT_ASSERT(inputBuffer->header->nOffset == 0);
        DBGT_ASSERT(inputBuffer->bus_data == inputBuffer->header->pBuffer);
        DBGT_ASSERT(inputBuffer->flags & BUFFER_FLAG_MY_BUFFER);

        OMX_U8* bus_data = inputBuffer->bus_data;
        OSAL_BUS_WIDTH bus_address = inputBuffer->bus_address;
        OMX_U32 filledlen = inputBuffer->header->nFilledLen;
        OMX_U32 retlen    = 0;

        // encode if we have frames
        //or slices in slice mode
        if (filledlen >= frameSize)
        {
            err = async_encode_image_data(pEnc, bus_data, bus_address, filledlen, inputBuffer, &retlen, frameSize);
        }
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_encode_image_data (err=%x)", err);
            goto INVALID_STATE;
        }

        // Check do we have encoded something and is leftover bits
        // Store leftovers to internal temp buffer
        if (retlen > 0)
        {
            OMX_U8* dst = tempBuffer->bus_data + tempBuffer->size;
            OMX_U8* src = bus_data;
            OMX_U32 capacity = tempBuffer->capacity;
            OMX_U32 needed   = retlen;
            while (1)
            {
                OMX_U32 available = capacity - tempBuffer->size;
                if (available > needed)
                    break;
                capacity *= 2;
            }
            if (capacity != tempBuffer->capacity)
            {
                err = grow_frame_buffer(pEnc, tempBuffer, capacity);
                if (err != OMX_ErrorNone)
                {
                    DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                    goto INVALID_STATE;
                }
            }

            memcpy(dst, src, retlen);
            tempBuffer->size += retlen;
        }
    }

    perf_stop(pEnc);

    if (inputBuffer->header->nFlags & OMX_BUFFERFLAG_EOS)
    {
        DBGT_PDEBUG("ASYNC: got buffer EOS flag");
        err = async_end_stream(pEnc);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("async_end_stream (err=%x)", err);
            DBGT_EPILOG("");
            return err;
        }

        pEnc->app_callbacks.EventHandler(
            pEnc->self,
            pEnc->app_data,
            OMX_EventBufferFlag,
            PORT_INDEX_OUTPUT,
            inputBuffer->header->nFlags,
            NULL);
    }

    if (inputBuffer->header->hMarkTargetComponent == pEnc->self)
    {
        pEnc->app_callbacks.EventHandler(pEnc->self, pEnc->app_data, OMX_EventMark, 0, 0, inputBuffer->header->pMarkData);
        inputBuffer->header->hMarkTargetComponent = NULL;
        inputBuffer->header->pMarkData            = NULL;
    }

    if (HantroOmx_port_is_tunneled(&pEnc->inputPort))
    {
        ((OMX_COMPONENTTYPE*)pEnc->inputPort.tunnelcomp)->FillThisBuffer(pEnc->inputPort.tunnelcomp, inputBuffer->header);
    }
    else
    {
        pEnc->app_callbacks.EmptyBufferDone(pEnc->self, pEnc->app_data, inputBuffer->header);
    }

    HantroOmx_port_lock_buffers(&pEnc->inputPort);
    HantroOmx_port_pop_buffer(&pEnc->inputPort);
    HantroOmx_port_unlock_buffers(&pEnc->inputPort);
    DBGT_EPILOG("");
    return OMX_ErrorNone;

 INVALID_STATE:
    DBGT_ASSERT(err != OMX_ErrorNone);
    DBGT_CRITICAL("ASYNC: error while processing buffers: %s", HantroOmx_str_omx_err(err));

    pEnc->state = OMX_StateInvalid;
    pEnc->run   = OMX_FALSE;
    pEnc->app_callbacks.EventHandler(
        pEnc->self,
        pEnc->app_data,
        OMX_EventError,
        err,
        0,
        NULL);
    DBGT_EPILOG("");
    return err;
}

/**
 * OMX_ERRORTYPE async_encode_image_data(OMX_ENCODER* pEnc,
 *                                  OMX_U8* bus_data,
 *                                  OSAL_BUS_WIDTH bus_address,
 *                                  OMX_U32 datalen, BUFFER* buff,
                                    OMX_U32* retlen)
 */
static
OMX_ERRORTYPE async_encode_image_data(OMX_ENCODER* pEnc, OMX_U8* bus_data, OSAL_BUS_WIDTH bus_address,
                                OMX_U32 datalen, BUFFER* inputBuffer, OMX_U32* retlen, OMX_U32 frameSize)
{
    DBGT_PROLOG("");
    DBGT_ASSERT(pEnc);
    DBGT_ASSERT(retlen);
    DBGT_PDEBUG("ASYNC: datalen %u", (unsigned)datalen);

    FRAME           frame;       //Frame for codec
    STREAM_BUFFER   stream;      //Encoder stream from codec
    BUFFER*         outputBuffer        = NULL;         //Output buffer from client
    OMX_ERRORTYPE   err                 = OMX_ErrorNone;
    OMX_U32         i, dataSize;
#ifdef USE_TEMP_OUTPUTBUFFER
    FRAME_BUFFER*   tempBuffer          = &pEnc->frame_out;
#endif

    while (datalen >= frameSize)
    {

        err = HantroOmx_port_lock_buffers(&pEnc->outputPort);
        if (err != OMX_ErrorNone)
        {
            DBGT_CRITICAL("HantroOmx_port_lock_buffers (out)");
            DBGT_EPILOG("");
            return err;
        }

        HantroOmx_port_get_buffer(&pEnc->outputPort, &outputBuffer);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);
        if (outputBuffer == NULL)
        {
            DBGT_CRITICAL("ASYNC: there's no output buffer!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }
#ifdef USE_TEMP_OUTPUTBUFFER
        if (tempBuffer->capacity < outputBuffer->header->nAllocLen)
        {
            err = grow_frame_buffer(pEnc, tempBuffer, outputBuffer->header->nAllocLen);
            if (err != OMX_ErrorNone)
            {
                DBGT_CRITICAL("grow_frame_buffer (err=%x)", err);
                DBGT_EPILOG("");
                return err;
            }
        }
#endif

        frame.fb_bus_data    = bus_data; // + offset
        frame.fb_bus_address = bus_address; // + offset
        frame.fb_frameSize = frameSize;
        frame.fb_bufferSize = frame.fb_frameSize;

        memset(&stream, 0, sizeof(STREAM_BUFFER));
#ifdef USE_TEMP_OUTPUTBUFFER
        DBGT_PDEBUG("ASYNC: Using temp output buffer");
        stream.buf_max_size = tempBuffer->capacity;
        stream.bus_data = tempBuffer->bus_data;
        stream.bus_address = tempBuffer->bus_address;
#else
        DBGT_PDEBUG("ASYNC: outputBuffer->bus_data %p outputBuffer->header->pBuffer %p",
                    outputBuffer->bus_data, outputBuffer->header->pBuffer);
        stream.buf_max_size = outputBuffer->header->nAllocLen;
        stream.bus_data = outputBuffer->bus_data;
        stream.bus_address = outputBuffer->bus_address;
#endif
        CODEC_STATE codecState = CODEC_ERROR_UNSPECIFIED;

        codecState = pEnc->codec->encode(pEnc->codec,&frame, &stream, &pEnc->encConfig);

        if (codecState < 0)
        {
            if (codecState == CODEC_ERROR_BUFFER_OVERFLOW)
            {
                DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_BUFFER_OVERFLOW!");
                DBGT_EPILOG("");
                return OMX_ErrorOverflow;
            }
            else if (codecState == CODEC_ERROR_INVALID_ARGUMENT)
            {
                DBGT_CRITICAL("pEnc->codec->encode() returned CODEC_ERROR_INVALID_ARGUMENT!");
                DBGT_EPILOG("");
                return OMX_ErrorBadParameter;
            }
            else
            {
                DBGT_CRITICAL("pEnc->codec->encode() returned undefined error!");
                DBGT_EPILOG("");
                return OMX_ErrorUndefined;
            }
        }

        OMX_U32 rem = datalen - frame.fb_frameSize;

        DBGT_PDEBUG("ASYNC: stream.streamlen - %u", (unsigned)stream.streamlen);
        DBGT_PDEBUG("ASYNC: rem - %u", (unsigned)rem);

        memmove(bus_data, bus_data + frame.fb_frameSize, rem);
        datalen -=  frame.fb_frameSize;
        DBGT_PDEBUG("ASYNC: bytes left in temp buffer - %u", (unsigned)datalen);
        DBGT_PDEBUG("ASYNC: output buffer size: %d", (int)outputBuffer->header->nAllocLen);

        if (stream.streamlen > outputBuffer->header->nAllocLen)
        {
            // note: create overflow event, continue?
            DBGT_CRITICAL("ASYNC: output buffer is too small!");
            DBGT_EPILOG("");
            return OMX_ErrorOverflow;
        }
        if (pEnc->outputPort.def.format.image.eCompressionFormat == (OMX_U32)OMX_IMAGE_CodingWEBP)
        {
            dataSize = 0;
            // copy data from each partition to the output buffer
            for(i = 0; i < 9; i++)
            {
                DBGT_PDEBUG("Partition %lu, size %lu", i, stream.streamSize[i]);
                if (stream.streamSize[i] <= outputBuffer->header->nAllocLen &&
                    dataSize <= outputBuffer->header->nAllocLen)
                {
                    memmove(outputBuffer->header->pBuffer + dataSize, stream.pOutBuf[i], stream.streamSize[i]);
                }
                else
                {
                    DBGT_CRITICAL("ASYNC: output buffer is too small!");
                    DBGT_EPILOG("");
                    return OMX_ErrorOverflow;
                }

                dataSize += stream.streamSize[i];
            }
        }
#ifdef USE_TEMP_OUTPUTBUFFER
        else
            memcpy(outputBuffer->header->pBuffer, stream.bus_data, stream.streamlen);
#endif
        outputBuffer->header->nOffset    = 0;
        outputBuffer->header->nFilledLen = stream.streamlen;
        outputBuffer->header->nTimeStamp = inputBuffer->header->nTimeStamp;
        outputBuffer->header->nFlags     = inputBuffer->header->nFlags & ~OMX_BUFFERFLAG_EOS;

        DBGT_PDEBUG("ASYNC: output buffer OK");

        OMX_S32 markcount = pEnc->mark_write_pos - pEnc->mark_read_pos;
        if (markcount)
        {
            DBGT_PDEBUG("ASYNC: got %d marks pending", (int)markcount);
            outputBuffer->header->hMarkTargetComponent = pEnc->marks[pEnc->mark_read_pos].hMarkTargetComponent;
            outputBuffer->header->pMarkData            = pEnc->marks[pEnc->mark_read_pos].pMarkData;
            pEnc->mark_read_pos++;
            if (--markcount == 0)
            {
                pEnc->mark_read_pos  = 0;
                pEnc->mark_write_pos = 0;
                DBGT_PDEBUG("ASYNC: mark buffer empty");
            }
        }
        else
        {
            outputBuffer->header->hMarkTargetComponent = inputBuffer->header->hMarkTargetComponent;
            outputBuffer->header->pMarkData            = inputBuffer->header->pMarkData;
        }

        if (pEnc-> sliceMode)
        {
            DBGT_PDEBUG("ASYNC: pEnc->sliceCounter %u", (unsigned)pEnc->sliceNum);
            DBGT_PDEBUG("ASYNC: pEnc->numOfSlices %u", (unsigned)pEnc->numOfSlices);
            pEnc->sliceNum++;
            if (pEnc->sliceNum > pEnc->numOfSlices)
            {
                pEnc->frameCounter++;
                DBGT_PDEBUG("OMX_BUFFERFLAG_ENDOFFRAME");

                outputBuffer->header->nFlags &= OMX_BUFFERFLAG_ENDOFFRAME;
                pEnc->sliceNum = 1;
            }
        }
        else
        {
            DBGT_PDEBUG("OMX_BUFFERFLAG_ENDOFFRAME");
            outputBuffer->header->nFlags &= OMX_BUFFERFLAG_ENDOFFRAME;
        }

        HantroOmx_port_lock_buffers(&pEnc->outputPort);
        HantroOmx_port_pop_buffer(&pEnc->outputPort);
        HantroOmx_port_unlock_buffers(&pEnc->outputPort);

        if (HantroOmx_port_is_tunneled(&pEnc->outputPort))
        {
            ((OMX_COMPONENTTYPE*)pEnc->outputPort.tunnelcomp)->EmptyThisBuffer(pEnc->outputPort.tunnelcomp, outputBuffer->header);
        }
        else
        {
            pEnc->app_callbacks.FillBufferDone(pEnc->self, pEnc->app_data, outputBuffer->header);
        }
    }
    *retlen = datalen;
    DBGT_EPILOG("");
    return OMX_ErrorNone;
}

#endif //~OMX_ENCODER_IMAGE_DOMAIN
