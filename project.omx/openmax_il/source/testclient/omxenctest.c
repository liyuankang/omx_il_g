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

#include <OMX_Core.h>
#include <OMX_Types.h>

/* std includes */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* common */
#include "util.h"

/* test client */
#include "omxtestcommon.h"
#include "omxencparameters.h"

#include "encoder_version.h"
#include "vsi_vendor_ext.h"
#include "android_ext.h"

OMXENCODER_PARAMETERS parameters;

OMX_VIDEO_PARAM_MPEG4TYPE mpeg4_parameters;
OMX_VIDEO_PARAM_AVCTYPE avc_parameters;
OMX_VIDEO_PARAM_H263TYPE h263_parameters;
OMX_VIDEO_PARAM_VP8TYPE vp8_parameters;
OMX_VIDEO_PARAM_HEVCTYPE hevc_parameters;

static int arg_count;

static char **arguments;

/* forward declarations */

OMX_U32 omxclient_next_vop(OMX_U32 inputRateNumer, OMX_U32 inputRateDenom,
                           OMX_U32 outputRateNumer, OMX_U32 outputRateDenom,
                           OMX_U32 frameCnt, OMX_U32 firstVop);

/*
    Macro that is used specifically to check parameter values
    in parameter checking loop.
 */
#define OMXENCODER_CHECK_NEXT_VALUE(i, args, argc, msg) \
    if(++(i) == argc || (*((args)[i])) == '-')  { \
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, msg); \
        return OMX_ErrorBadParameter; \
    }

/*
    omx_encoder_port_initialize
*/
OMX_ERRORTYPE omx_encoder_port_initialize(OMXCLIENT * appdata,
                                          OMX_PARAM_PORTDEFINITIONTYPE * p)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    switch (p->eDir)
    {
        /* CASE IN: initialize input port */
    case OMX_DirInput:
        OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                       "Using port at index %i as input port\n", p->nPortIndex);

        p->nBufferCountActual = parameters.buffer_count;
        error = process_encoder_input_parameters(arg_count, arguments, p);
        break;

        /* CASE OUT: initialize output port */
    case OMX_DirOutput:

        OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                       "Using port at index %i as output port\n",
                       p->nPortIndex);

        p->nBufferCountActual = parameters.buffer_count;
        error = process_encoder_output_parameters(arg_count, arguments, p);
        appdata->domain = OMX_PortDomainVideo;

        if(error != OMX_ErrorNone)
            break;

        parameters.output_compression = p->format.video.eCompressionFormat;
        switch ((OMX_U32)parameters.output_compression)
        {
        case OMX_VIDEO_CodingAVC:
            omxclient_struct_init(&avc_parameters, OMX_VIDEO_PARAM_AVCTYPE);
            avc_parameters.nPortIndex = 1;

            error =
                process_avc_parameters(arg_count, arguments, &avc_parameters);
            break;

        case OMX_VIDEO_CodingMPEG4:
            omxclient_struct_init(&mpeg4_parameters, OMX_VIDEO_PARAM_MPEG4TYPE);
            mpeg4_parameters.nPortIndex = 1;

            error =
                process_mpeg4_parameters(arg_count, arguments,
                                         &mpeg4_parameters);
            break;

        case OMX_VIDEO_CodingH263:
            omxclient_struct_init(&h263_parameters, OMX_VIDEO_PARAM_H263TYPE);
            h263_parameters.nPortIndex = 1;

            error =
                process_h263_parameters(arg_count, arguments, &h263_parameters);
            break;

        case OMX_VIDEO_CodingVP8:
            omxclient_struct_init(&vp8_parameters, OMX_VIDEO_PARAM_VP8TYPE);
            vp8_parameters.nPortIndex = 1;

            error =
                process_vp8_parameters(arg_count, arguments, &vp8_parameters, appdata);
            break;

        case OMX_VIDEO_CodingHEVC:
            omxclient_struct_init(&hevc_parameters, OMX_VIDEO_PARAM_HEVCTYPE);
            hevc_parameters.nPortIndex = 1;

            error =
                process_hevc_parameters(arg_count, arguments, &hevc_parameters);
            break;
        default:
            return OMX_ErrorBadParameter;
        }

        break;

    case OMX_DirMax:
    default:
        break;
    }

    return error;
}

OMX_ERRORTYPE omx_encoder_image_port_initialize(OMXCLIENT * appdata,
                                               OMX_PARAM_PORTDEFINITIONTYPE * p)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    switch (p->eDir)
    {
        /* CASE IN: initialize input port */
    case OMX_DirInput:
        OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                       "Using port at index %i as input port\n", p->nPortIndex);

        p->nBufferCountActual = parameters.buffer_count;
        error = process_encoder_image_input_parameters(arg_count, arguments, p);
        break;

        /* CASE OUT: initialize output port */
    case OMX_DirOutput:

        OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                       "Using port at index %i as output port\n",
                       p->nPortIndex);

        p->nBufferCountActual = parameters.buffer_count;
        error = process_encoder_image_output_parameters(arg_count, arguments, p);
        appdata->coding_type = p->format.image.eCompressionFormat;
        appdata->domain = OMX_PortDomainImage;
        break;

    case OMX_DirMax:
    default:
        break;
    }
    return error;
}

OMX_ERRORTYPE initialize_avc_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_PARAM_DEBLOCKINGTYPE deblocking;
    OMX_VIDEO_PARAM_AVCTYPE parameters;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;
    OMX_VIDEO_PARAM_AVCEXTTYPE extensions;

    /* set common AVC parameters */
    omxclient_struct_init(&parameters, OMX_VIDEO_PARAM_AVCTYPE);
    client->coding_type = OMX_VIDEO_CodingAVC;
    parameters.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoAvc,
                               &parameters), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_avc_parameters(argc, args, &parameters),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoAvc,
                               &parameters), omxError);

    /* set bitrate */
    omxclient_struct_init(&bitrate, OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_bitrate(argc, args, &bitrate),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);

    /* set deblocking */
    omxclient_struct_init(&deblocking, OMX_PARAM_DEBLOCKINGTYPE);
    deblocking.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamCommonDeblocking, &deblocking),
                              omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_avc_parameters_deblocking
                              (argc, args, &deblocking), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamCommonDeblocking, &deblocking),
                              omxError);

    /* set quantization */
    omxclient_struct_init(&quantization, OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
    quantization.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_quantization
                              (argc, args, &quantization), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);


    /* set extension encoding parameters */
    omxclient_struct_init(&extensions, OMX_VIDEO_PARAM_AVCEXTTYPE);
    extensions.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoAvcExt, &extensions),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_avc_extension
                              (argc, args, &extensions), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoAvcExt, &extensions),
                              omxError);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE initialize_mpeg4_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;
    OMX_VIDEO_PARAM_MPEG4TYPE parameters;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;

    /* set bitrate */
    omxclient_struct_init(&parameters, OMX_VIDEO_PARAM_MPEG4TYPE);
    client->coding_type = OMX_VIDEO_CodingMPEG4;
    parameters.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoMpeg4,
                               &parameters), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_mpeg4_parameters(argc, args, &parameters),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoMpeg4,
                               &parameters), omxError);

    /* set quantization */
    omxclient_struct_init(&quantization, OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
    quantization.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_quantization
                              (argc, args, &quantization), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    /* set bitrate */
    omxclient_struct_init(&bitrate, OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_bitrate(argc, args, &bitrate),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE initialize_h263_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;
    OMX_VIDEO_PARAM_H263TYPE parameters;
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;

    /* set parameters */
    omxclient_struct_init(&parameters, OMX_VIDEO_PARAM_H263TYPE);
    client->coding_type = OMX_VIDEO_CodingH263;
    parameters.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoH263,
                               &parameters), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_h263_parameters(argc, args, &parameters),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoH263,
                               &parameters), omxError);

    /* set bitrate */
    omxclient_struct_init(&bitrate, OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_bitrate(argc, args, &bitrate),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);

    /* set quantization */
    omxclient_struct_init(&quantization, OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
    quantization.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_quantization
                              (argc, args, &quantization), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE initialize_vp8_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;
    OMX_VIDEO_PARAM_VP8TYPE vp8_parameters;
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;

    /* set parameters */
    omxclient_struct_init(&vp8_parameters, OMX_VIDEO_PARAM_VP8TYPE);
    client->coding_type = OMX_VIDEO_CodingVP8;
    vp8_parameters.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoVp8,
                               &vp8_parameters), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_vp8_parameters(argc, args, &vp8_parameters, client),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoVp8,
                               &vp8_parameters), omxError);

    /* set bitrate */
    omxclient_struct_init(&bitrate, OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_bitrate(argc, args, &bitrate),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);

    /* set quantization */
    omxclient_struct_init(&quantization, OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
    quantization.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_quantization
                              (argc, args, &quantization), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE initialize_hevc_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;
    OMX_VIDEO_PARAM_HEVCTYPE hevc_parameters;
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_PARAM_DEBLOCKINGTYPE deblocking;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;

    /* set parameters */
    omxclient_struct_init(&hevc_parameters, OMX_VIDEO_PARAM_HEVCTYPE);
    client->coding_type = OMX_VIDEO_CodingHEVC;
    hevc_parameters.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoHevc,
                               &hevc_parameters), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_hevc_parameters(argc, args, &hevc_parameters),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoHevc,
                               &hevc_parameters), omxError);

    /* set bitrate */
    omxclient_struct_init(&bitrate, OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_bitrate(argc, args, &bitrate),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamVideoBitrate,
                               &bitrate), omxError);

    /* set deblocking */
    omxclient_struct_init(&deblocking, OMX_PARAM_DEBLOCKINGTYPE);
    deblocking.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamCommonDeblocking, &deblocking),
                              omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_avc_parameters_deblocking
                              (argc, args, &deblocking), omxError);

    deblocking.nPortIndex = 1;
    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamCommonDeblocking, &deblocking),
                              omxError);

    /* set quantization */
    omxclient_struct_init(&quantization, OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
    quantization.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    OMXCLIENT_RETURN_ON_ERROR(process_parameters_quantization
                              (argc, args, &quantization), omxError);

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component,
                               OMX_IndexParamVideoQuantization, &quantization),
                              omxError);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE initialize_image_output(OMXCLIENT * client, int argc, char **args)
{
    OMX_ERRORTYPE omxError;

    OMX_IMAGE_PARAM_QFACTORTYPE qfactor;

    printf("%s\n", __FUNCTION__);

    /* set parameters */
    omxclient_struct_init(&qfactor, OMX_IMAGE_PARAM_QFACTORTYPE);
    qfactor.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_GetParameter
                              (client->component, OMX_IndexParamQFactor,
                               &qfactor), omxError);
    OMXCLIENT_RETURN_ON_ERROR(process_parameters_image_qfactor
                              (argc, args, &qfactor), omxError);

    printf("QValue: %d\n", (int) qfactor.nQFactor);
    qfactor.nPortIndex = 1;

    OMXCLIENT_RETURN_ON_ERROR(OMX_SetParameter
                              (client->component, OMX_IndexParamQFactor,
                               &qfactor), omxError);

    return OMX_ErrorNone;
}


/*
    main
 */
int main(int argc, char **args)
{
    OMXCLIENT client;
    OMX_ERRORTYPE omxError;
    OMX_U32 *variance_array;
    OMX_U32 variance_array_len;

    arg_count = argc;
    arguments = args;

    memset(&parameters, 0, sizeof(OMXENCODER_PARAMETERS));

    parameters.buffer_size = 0;
    parameters.buffer_count = 8+1;

    parameters.stabilization = OMX_FALSE;

    omxError = process_encoder_parameters(argc, args, &parameters);
    if(omxError != OMX_ErrorNone)
    {
        print_usage(args[0]);
        return omxError;
    }

    variance_array_len = 0;
    variance_array = 0;

    if(parameters.varfile &&
       (omxError =
        omxclient_read_variance_file(&variance_array, &variance_array_len,
                                     parameters.varfile)) != OMX_ErrorNone)
    {
        return omxError;
    }

    omxError = OMX_Init();
    if(omxError == OMX_ErrorNone)
    {
        if(parameters.image_output)
        {
            omxError =
                omxclient_component_create(&client,
                                           IMAGE_COMPONENT_NAME,
                                           parameters.cRole /*"image_encoder.jpeg"*/,
                                           parameters.buffer_count);
        }
        else
        {
            omxError =
                omxclient_component_create(&client,
                                           VIDEO_COMPONENT_NAME,
                                           parameters.cRole /*"video_encoder.avc"*/,
                                           parameters.buffer_count);
        }

        client.store_frames = parameters.framed_output;
        client.store_buffers = parameters.sliced_output;
        client.output_name = parameters.outfile;

        if(omxError == OMX_ErrorNone)
        {
            omxError = omxclient_check_component_version(client.component);
        }
        else
        {
            OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                           "Component creation failed: '%s'\n",
                           OMX_OSAL_TraceErrorStr(omxError));
        }

        if(omxError == OMX_ErrorNone)
        {
            if(parameters.image_output == OMX_FALSE)
            {
                omxError =
                    omxclient_component_initialize(&client,
                                                   &omx_encoder_port_initialize);

                switch ((OMX_U32)parameters.output_compression)
                {

                case OMX_VIDEO_CodingAVC:
                    omxError = initialize_avc_output(&client, argc, args);
                    break;

                case OMX_VIDEO_CodingMPEG4:
                    omxError = initialize_mpeg4_output(&client, argc, args);
                    break;

                case OMX_VIDEO_CodingH263:
                    omxError = initialize_h263_output(&client, argc, args);
                    break;

                case OMX_VIDEO_CodingVP8:
                    omxError = initialize_vp8_output(&client, argc, args);
                    break;

                case OMX_VIDEO_CodingHEVC:
                    omxError = initialize_hevc_output(&client, argc, args);
                    break;

                default:
                    {
                        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                       "Format is unsupported\n");
                        return OMX_ErrorNone;
                    }
                }
            }
            else
            {
                omxError =
                    omxclient_component_initialize_image(&client,
                                                        &omx_encoder_image_port_initialize);
                if(omxError == OMX_ErrorNone)
                {
                    initialize_image_output(&client, argc, args);
                }
            }

            /* set rotation */
            if(omxError == OMX_ErrorNone && parameters.rotation != 0)
            {

                OMX_CONFIG_ROTATIONTYPE rotation;

                omxclient_struct_init(&rotation, OMX_CONFIG_ROTATIONTYPE);

                rotation.nPortIndex = 0;
                rotation.nRotation = parameters.rotation;

                if((omxError =
                    OMX_SetConfig(client.component, OMX_IndexConfigCommonRotate,
                                  &rotation)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Rotation could not be set: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));
                    return omxError;
                }
            }

            /* set stabilization */
            if(omxError == OMX_ErrorNone && !parameters.image_output &&
               parameters.stabilization)
            {

                OMX_CONFIG_FRAMESTABTYPE stab;

                omxclient_struct_init(&stab, OMX_CONFIG_FRAMESTABTYPE);

                stab.nPortIndex = 0;
                stab.bStab = parameters.stabilization;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigCommonFrameStabilisation,
                                  &stab)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Stabilization could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));
                    return omxError;
                }
            }

            /* set cropping */
            if(omxError == OMX_ErrorNone && parameters.cropping)
            {

                OMX_CONFIG_RECTTYPE rect;

                omxclient_struct_init(&rect, OMX_CONFIG_RECTTYPE);

                rect.nPortIndex = 0;
                rect.nLeft      = parameters.cleft;
                rect.nTop       = parameters.ctop;
                rect.nHeight    = parameters.cheight;
                rect.nWidth     = parameters.cwidth;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigCommonInputCrop,
                                  &rect)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Cropping could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }

            /* set intra area */
            if(omxError == OMX_ErrorNone && parameters.intraArea.enable)
            {

                OMX_VIDEO_CONFIG_INTRAAREATYPE area;

                omxclient_struct_init(&area, OMX_VIDEO_CONFIG_INTRAAREATYPE);

                area.nPortIndex = 1;
                area.bEnable    = parameters.intraArea.enable;
                area.nLeft      = parameters.intraArea.left;
                area.nTop       = parameters.intraArea.top;
                area.nBottom    = parameters.intraArea.bottom;
                area.nRight     = parameters.intraArea.right;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoIntraArea,
                                  &area)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Intra area could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }

            /* set IPCM 1 area */
            if(omxError == OMX_ErrorNone && parameters.ipcm1Area.enable)
            {

                OMX_VIDEO_CONFIG_IPCMAREATYPE ipcm;
                omxclient_struct_init(&ipcm, OMX_VIDEO_CONFIG_IPCMAREATYPE);

                ipcm.nPortIndex = 1;
                ipcm.nArea      = 1;
                ipcm.bEnable    = parameters.ipcm1Area.enable;
                ipcm.nLeft      = parameters.ipcm1Area.left;
                ipcm.nTop       = parameters.ipcm1Area.top;
                ipcm.nBottom    = parameters.ipcm1Area.bottom;
                ipcm.nRight     = parameters.ipcm1Area.right;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoIPCMArea,
                                  &ipcm)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "IPCM area 1 could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }

            }

            /* set IPCM 2 area */
            if(omxError == OMX_ErrorNone && parameters.ipcm2Area.enable)
            {

                OMX_VIDEO_CONFIG_IPCMAREATYPE ipcm;
                omxclient_struct_init(&ipcm, OMX_VIDEO_CONFIG_IPCMAREATYPE);

                ipcm.nPortIndex = 1;
                ipcm.nArea      = 2;
                ipcm.bEnable    = parameters.ipcm2Area.enable;
                ipcm.nLeft      = parameters.ipcm2Area.left;
                ipcm.nTop       = parameters.ipcm2Area.top;
                ipcm.nBottom    = parameters.ipcm2Area.bottom;
                ipcm.nRight     = parameters.ipcm2Area.right;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoIPCMArea,
                                  &ipcm)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "IPCM area 2 could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }

            }

            /* set ROI 1 area */
            if(omxError == OMX_ErrorNone && parameters.roi1Area.enable)
            {

                OMX_VIDEO_CONFIG_ROIAREATYPE roi;
                omxclient_struct_init(&roi, OMX_VIDEO_CONFIG_ROIAREATYPE);

                roi.nPortIndex = 1;
                roi.nArea      = 1;
                roi.bEnable    = parameters.roi1Area.enable;
                roi.nLeft      = parameters.roi1Area.left;
                roi.nTop       = parameters.roi1Area.top;
                roi.nBottom    = parameters.roi1Area.bottom;
                roi.nRight     = parameters.roi1Area.right;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoRoiArea,
                                  &roi)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "ROI area 1 could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }

                OMX_VIDEO_CONFIG_ROIDELTAQPTYPE deltaQp;
                omxclient_struct_init(&deltaQp, OMX_VIDEO_CONFIG_ROIDELTAQPTYPE);

                deltaQp.nPortIndex = 1;
                deltaQp.nArea      = 1;
                deltaQp.nDeltaQP   = parameters.roi1DeltaQP;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoRoiDeltaQp,
                                  &deltaQp)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "ROI 1 delta QP could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }

           /* set ROI 2 area */
            if(omxError == OMX_ErrorNone && parameters.roi2Area.enable)
            {

                OMX_VIDEO_CONFIG_ROIAREATYPE roi;
                omxclient_struct_init(&roi, OMX_VIDEO_CONFIG_ROIAREATYPE);

                roi.nPortIndex = 1;
                roi.nArea      = 2;
                roi.bEnable    = parameters.roi2Area.enable;
                roi.nLeft      = parameters.roi2Area.left;
                roi.nTop       = parameters.roi2Area.top;
                roi.nBottom    = parameters.roi2Area.bottom;
                roi.nRight     = parameters.roi2Area.right;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoRoiArea,
                                  &roi)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "ROI area 2 could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }

                OMX_VIDEO_CONFIG_ROIDELTAQPTYPE deltaQp;
                omxclient_struct_init(&deltaQp, OMX_VIDEO_CONFIG_ROIDELTAQPTYPE);

                deltaQp.nPortIndex = 1;
                deltaQp.nArea      = 2;
                deltaQp.nDeltaQP   = parameters.roi2DeltaQP;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoRoiDeltaQp,
                                  &deltaQp)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "ROI 2 delta QP could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }

            /* set adaptive ROI */
#ifdef ENCH1
            if(omxError == OMX_ErrorNone && parameters.adaptiveRoi)
            {
                OMX_VIDEO_CONFIG_ADAPTIVEROITYPE adaptiveRoi;
                omxclient_struct_init(&adaptiveRoi, OMX_VIDEO_CONFIG_ADAPTIVEROITYPE);

                adaptiveRoi.nPortIndex          = 1;
                adaptiveRoi.nAdaptiveROI        = parameters.adaptiveRoi;
                adaptiveRoi.nAdaptiveROIColor   = parameters.adaptiveRoiColor;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoAdaptiveRoi,
                                  &adaptiveRoi)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Adaptive ROI could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }
#endif

            /* set VP8 temporal layers */
            if(omxError == OMX_ErrorNone && parameters.baseLayerBitrate)
            {
                OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE layers;
                omxclient_struct_init(&layers, OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE);

                layers.nPortIndex           = 1;
                layers.nBaseLayerBitrate    = parameters.baseLayerBitrate;
                layers.nLayer1Bitrate       = parameters.layer1Bitrate;
                layers.nLayer2Bitrate       = parameters.layer2Bitrate;
                layers.nLayer3Bitrate       = parameters.layer3Bitrate;

                if((omxError =
                    OMX_SetConfig(client.component,
                                  OMX_IndexConfigVideoVp8TemporalLayers,
                                  &layers)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "VP8 temporal layers could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }

                client.vp8Layers = 1;
                if(parameters.layer1Bitrate)
                    client.vp8Layers++;
                if(parameters.layer2Bitrate)
                    client.vp8Layers++;
                if(parameters.layer3Bitrate)
                    client.vp8Layers++;
            }

            /* set cyclic intra refresh */
            if(omxError == OMX_ErrorNone && parameters.cir)
            {
                OMX_VIDEO_PARAM_INTRAREFRESHTYPE intraRefresh;
                omxclient_struct_init(&intraRefresh, OMX_VIDEO_PARAM_INTRAREFRESHTYPE);

                intraRefresh.nPortIndex     = 1;
                intraRefresh.eRefreshMode   = OMX_VIDEO_IntraRefreshCyclic;
                intraRefresh.nCirMBs        = parameters.cir;

                if((omxError =
                    OMX_SetParameter(client.component,
                                  OMX_IndexParamVideoIntraRefresh,
                                  &intraRefresh)) != OMX_ErrorNone)
                {

                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Intra refresh could not be enabled: %s\n",
                                   OMX_OSAL_TraceErrorStr(omxError));

                    return omxError;
                }
            }

#ifdef ANDROIDTEST
            OMX_INDEXTYPE index;

            omxError = OMX_GetExtensionIndex(client.component, "OMX.google.android.index.prependSPSPPSToIDRFrames", &index);

            if (omxError == OMX_ErrorNone) {
                struct PrependSPSPPSToIDRFramesParams params;
                params.bEnable = OMX_TRUE;

                omxError = OMX_SetParameter(client.component, index, &params);
            }
#endif
            if(omxError == OMX_ErrorNone)
            {
             /*   omxError = omxclient_initialize_buffers_fixed(&client,
                                                               parameters.
                                                               buffer_size,
                                                               parameters.
                                                               buffer_count);*/

                omxError = omxclient_initialize_buffers(&client);

                if(omxError != OMX_ErrorNone)
                {
                    /* raw free, because state was not changed to idle */
                    omxclient_component_free_buffers(&client);
                }

            }

            if(omxError == OMX_ErrorNone)
            {
                if(parameters.image_output)
                {
                    /* execute conversion as sliced */
                    omxError =
                        omxclient_execute_yuv_sliced(&client,
                                                        parameters.infile,
                                                        parameters.outfile,
                                                        parameters.firstvop,
                                                        parameters.lastvop);
                }
                else
                {
                    omxError =
                        omxclient_execute_yuv_range(&client,
                                                    parameters.infile,
                                                    variance_array,
                                                    variance_array_len,
                                                    parameters.outfile,
                                                    parameters.firstvop,
                                                    parameters.lastvop);
                }

                if(omxError != OMX_ErrorNone)
                {
                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                                   "Video processing failed: '%s'\n",
                                   OMX_OSAL_TraceErrorStr(omxError));
                }
            }
            else
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Component video initialization failed: '%s'\n",
                               OMX_OSAL_TraceErrorStr(omxError));
            }

            /* destroy the component since it was succesfully created */
            omxError = omxclient_component_destroy(&client);
            if(omxError != OMX_ErrorNone)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Component destroy failed: '%s'\n",
                               OMX_OSAL_TraceErrorStr(omxError));
            }
        }

        OMX_Deinit();
    }
    else
    {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                       "OMX initialization failed: '%s'\n",
                       OMX_OSAL_TraceErrorStr(omxError));

        /* FAIL: OMX initialization failed, reason */
    }

    return omxError;
}
