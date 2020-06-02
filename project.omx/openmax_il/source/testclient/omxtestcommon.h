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

#ifndef OMXTESTCOMMON_
#define OMXTESTCOMMON_

#include <stdio.h>
#include "vsi_vendor_ext.h"

/**
 * This is not the place and way to define the versions, but unfortunately
 * those weren't available in OMX headers...
 */

/* definitions */
/*#define OMX_VERSION_MINOR		0x01
#define OMX_VERSION_MAJOR		0x01
#define OMX_VERSION_REVISION	0x01
#define OMX_VERSION_STEP		0x00*/

/**
 *
 */
typedef struct HEADERLIST
{
    OMX_BUFFERHEADERTYPE **hdrs;

    OMX_U32 readpos;
    OMX_U32 writepos;
    OMX_U32 capacity;

} HEADERLIST;

/**
 *
 */
typedef struct OMXCLIENT
{
    OMX_HANDLETYPE component;
    OMX_HANDLETYPE queue_mutex;
    OMX_HANDLETYPE state_event;

    HEADERLIST input_queue;
    HEADERLIST output_queue;

    OMX_BOOL EOS;

    OMX_STRING output_name;

    FILE *input;
    FILE *output;

    OMX_BOOL store_frames;
    OMX_BOOL store_buffers;

    FILE *frame_output;
    OMX_U64 frame_count;
    OMX_U32 output_buffer_count;
    OMX_U32 output_size;
    OMX_PORTDOMAINTYPE domain;
    OMX_VIDEO_CODINGTYPE coding_type;
    OMX_U32 vp8IntraPicRate;
    OMX_U32 vp8Layers;
} OMXCLIENT;

#define OMXCLIENT_PTR(P) ((OMXCLIENT*)P)

/* define event timeout if not defined */
#ifndef OMXCLIENT_EVENT_TIMEOUT
#define OMXCLIENT_EVENT_TIMEOUT 5000    /* ms */
#endif

/**
 * Common struct initialization
 */
#define omxclient_struct_init(_p_, _name_) \
	do { \
	memset((_p_), 0, sizeof(_name_)); \
	(_p_)->nSize = sizeof(_name_); \
	(_p_)->nVersion.s.nVersionMajor = OMX_VERSION_MINOR; \
	(_p_)->nVersion.s.nVersionMinor = OMX_VERSION_MAJOR; \
	(_p_)->nVersion.s.nRevision = OMX_VERSION_REVISION; \
	(_p_)->nVersion.s.nStep  = OMX_VERSION_STEP; \
	} while(0)

#define OMXCLIENT_RETURN_ON_ERROR(f, e) \
	(e) = (f); \
	if((e) != OMX_ErrorNone) \
    	return (e);

#ifdef __CPLUSPLUS
extern "C"
{
#endif                       /* __CPLUSPLUS */

/* function prototypes */
    OMX_ERRORTYPE omxclient_wait_state(OMXCLIENT * client, OMX_STATETYPE state);

    OMX_ERRORTYPE omxclient_change_state_and_wait(OMXCLIENT * client,
                                                  OMX_STATETYPE state);

    OMX_ERRORTYPE omxclient_component_create(OMXCLIENT * ppComp,
                                             OMX_STRING cComponentName,
                                             OMX_STRING cRole,
                                             OMX_U32 buffer_count);

    OMX_ERRORTYPE omxclient_component_destroy(OMXCLIENT * appdata);

    OMX_ERRORTYPE omxclient_component_initialize(OMXCLIENT * appdata,
                                                 OMX_ERRORTYPE
                                                 (*port_configurator_fn)
                                                 (OMXCLIENT * appdata,
                                                  OMX_PARAM_PORTDEFINITIONTYPE *
                                                  port));

    OMX_ERRORTYPE omxclient_component_initialize_image(OMXCLIENT * appdata,
                                                      OMX_ERRORTYPE
                                                      (*port_configurator_fn)
                                                      (OMXCLIENT * appdata,
                                                       OMX_PARAM_PORTDEFINITIONTYPE
                                                       * port));

    OMX_ERRORTYPE omxclient_initialize_buffers(OMXCLIENT * client);

    OMX_ERRORTYPE omxclient_initialize_buffers_fixed(OMXCLIENT * client,
                                                      OMX_U32 size,
                                                      OMX_U32 count);

    OMX_ERRORTYPE omxclient_component_free_buffers(OMXCLIENT * client);

    OMX_ERRORTYPE omxclient_check_component_version(OMX_IN OMX_HANDLETYPE
                                                    hComponent);

    OMX_ERRORTYPE omxclient_execute(OMXCLIENT * appdata,
                                    OMX_STRING input_filename,
                                    OMX_U32 * variance, OMX_U32 varlen,
                                    OMX_STRING output_filename);

    OMX_ERRORTYPE omxclient_execute_yuv_range(OMXCLIENT * appdata,
                                              OMX_STRING input_filename,
                                              OMX_U32 * variance,
                                              OMX_U32 varlen,
                                              OMX_STRING output_filename,
                                              OMX_U32 firstVop,
                                              OMX_U32 lastVop);

    OMX_ERRORTYPE omxclient_execute_yuv_sliced(OMXCLIENT * appdata,
                                               OMX_STRING input_filename,
                                               OMX_STRING output_filename,
                                               OMX_U32 firstVop,
                                               OMX_U32 lastVop);

    OMX_ERRORTYPE omxclient_execute_rcv(OMXCLIENT * appdata,
                                        OMX_STRING input_filename,
                                        OMX_STRING output_filename);

    OMX_ERRORTYPE omxclient_read_variance_file(OMX_U32 ** variance_array,
                                               OMX_U32 * array_len,
                                               OMX_STRING filename);

/* ---------------- TRACE-H -------------------- */

#define OMX_OSAL_TRACE_DEBUG	0x0004
#define OMX_OSAL_TRACE_INFO		0x0008
#define OMX_OSAL_TRACE_ERROR	0x0010
#define OMX_OSAL_TRACE_BUFFER	0x0020
#define OMX_OSAL_TRACE_WARNING	0x0040

    OMX_ERRORTYPE OMX_OSAL_Trace(OMX_IN OMX_U32 nTraceFlags,
                                 OMX_IN char *format, ...);

    OMX_STRING OMX_OSAL_TraceErrorStr(OMX_IN OMX_ERRORTYPE omxError);

/* ---------------- TRACE - END -------------------- */

#ifdef __CPLUSPLUS
}
#endif                       /* __CPLUSPLUS */

#endif                       /*OMXTESTCOMMON_ */
