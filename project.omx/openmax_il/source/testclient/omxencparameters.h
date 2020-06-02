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

#ifndef OMXENCPARAMETERS_H_
#define OMXENCPARAMETERS_H_

#include "omxtestcommon.h"

/* Defines rectangular macroblock area in encoder picture */
typedef struct PICTURE_AREA
{
    OMX_BOOL enable;
    OMX_U32 top;
    OMX_U32 left;
    OMX_U32 bottom;
    OMX_U32 right;
} PICTURE_AREA;

/*
    struct for transferring parameter definitions
    between functions
 */
typedef struct OMXENCODER_PARAMETERS
{
    OMX_STRING infile;
    OMX_STRING outfile;
    OMX_STRING varfile;

    OMX_U32 firstvop;
    OMX_U32 lastvop;

    OMX_U32 buffer_count;
    OMX_U32 buffer_size;

    OMX_BOOL image_output;
    OMX_BOOL sliced;

    OMX_VIDEO_CODINGTYPE output_compression;

    int rotation;
    OMX_BOOL stabilization;

    OMX_BOOL framed_output;
    OMX_BOOL sliced_output;

    OMX_BOOL cropping;
    OMX_U32 ctop, cleft, cwidth, cheight;

    PICTURE_AREA intraArea;
    PICTURE_AREA ipcm1Area;
    PICTURE_AREA ipcm2Area;
    PICTURE_AREA roi1Area;
    PICTURE_AREA roi2Area;
    OMX_S32      roi1DeltaQP;
    OMX_S32      roi2DeltaQP;
    OMX_S32      adaptiveRoi;
    OMX_S32      adaptiveRoiColor;

    /* VP8 temporal layers */
    OMX_U32      baseLayerBitrate;
    OMX_U32      layer1Bitrate;
    OMX_U32      layer2Bitrate;
    OMX_U32      layer3Bitrate;

    OMX_U32      cir;
    OMX_STRING   cRole;
} OMXENCODER_PARAMETERS;

#ifdef __CPLUSPLUS
extern "C"
{
#endif                       /* __CPLUSPLUS */

/* common */

    void print_usage(OMX_STRING swname);

    OMX_ERRORTYPE process_encoder_parameters(int argc, char **args,
                                             OMXENCODER_PARAMETERS * params);

    OMX_ERRORTYPE process_encoder_input_parameters(int argc, char **args,
                                                   OMX_PARAM_PORTDEFINITIONTYPE
                                                   * params);

    OMX_ERRORTYPE process_encoder_output_parameters(int argc, char **args,
                                                    OMX_PARAM_PORTDEFINITIONTYPE
                                                    * params);

    OMX_ERRORTYPE process_parameters_quantization(int argc, char **args,
                                                  OMX_VIDEO_PARAM_QUANTIZATIONTYPE
                                                  * quantization);

    OMX_ERRORTYPE process_parameters_bitrate(int argc, char **args,
                                             OMX_VIDEO_PARAM_BITRATETYPE *
                                             bitrate);

/* MPEG4 */

    void print_mpeg4_usage();

    OMX_ERRORTYPE process_mpeg4_parameters(int argc, char **args,
                                           OMX_VIDEO_PARAM_MPEG4TYPE *
                                           mpeg4parameters);

/* AVC */

    void print_avc_usage();

    OMX_ERRORTYPE process_avc_parameters_deblocking(int argc, char **args,
                                                    OMX_PARAM_DEBLOCKINGTYPE *
                                                    deblocking);

    OMX_ERRORTYPE process_avc_parameters(int argc, char **args,
                                         OMX_VIDEO_PARAM_AVCTYPE * parameters);

    OMX_ERRORTYPE process_parameters_avc_extension(int argc, char **args,
                                                  OMX_VIDEO_PARAM_AVCEXTTYPE *extensions);

/* H263 */

    void print_h263_usage();

    OMX_ERRORTYPE process_h263_parameters(int argc, char **args,
                                          OMX_VIDEO_PARAM_H263TYPE *
                                          parameters);
/* VP8 */

    void print_vp8_usage();

    OMX_ERRORTYPE process_vp8_parameters(int argc, char **args,
                                          OMX_VIDEO_PARAM_VP8TYPE * parameters,
                                          OMXCLIENT * appdata);
    void print_webp_usage();

/* HEVC */

    void print_hevc_usage();

    OMX_ERRORTYPE process_hevc_parameters(int argc, char **args,
                                          OMX_VIDEO_PARAM_HEVCTYPE * parameters);
/* JPEG */

    void print_jpeg_usage();

    OMX_ERRORTYPE process_encoder_image_input_parameters(int argc, char **args,
                                                        OMX_PARAM_PORTDEFINITIONTYPE
                                                        * params);

    OMX_ERRORTYPE process_encoder_image_output_parameters(int argc, char **args,
                                                         OMX_PARAM_PORTDEFINITIONTYPE
                                                         * params);

    OMX_ERRORTYPE process_parameters_image_qfactor(int argc, char **args,
                                                   OMX_IMAGE_PARAM_QFACTORTYPE *
                                                   quantization);

#ifdef __CPLUSPLUS
}
#endif                       /* __CPLUSPLUS */

#endif                       /*OMXENCPARAMETERS_H_ */
