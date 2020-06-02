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

#ifndef HANTRO_ENCODER_VERSION_H
#define HANTRO_ENCODER_VERSION_H

#define COMPONENT_VERSION_MAJOR    2
#define COMPONENT_VERSION_MINOR    0
#define COMPONENT_VERSION_REVISION 2
#define COMPONENT_VERSION_STEP     0

/* Roles currently disabled. Standard roles requires support for YUV420Planar
 * format which is not implemeted. */
//#define VIDEO_COMPONENT_ROLES 0
//#define IMAGE_COMPONENT_ROLES 0

#ifdef ENC7280
#define VIDEO_COMPONENT_NAME "OMX.hantro.7280.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.7280.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - H264 Encoder */
#define COMPONENT_NAME_H264  "OMX.hantro.7280.video.encoder.avc"
#define COMPONENT_ROLE_H264  "video_encoder.avc"
/** Role 2 - MPEG4 Encoder */
#define COMPONENT_NAME_MPEG4 "OMX.hantro.7280.video.encoder.mpeg4"
#define COMPONENT_ROLE_MPEG4 "video_encoder.mpeg4"
/** Role 3 - H263 Encoder */
#define COMPONENT_NAME_H263  "OMX.hantro.7280.video.encoder.h263"
#define COMPONENT_ROLE_H263  "video_encoder.h263"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - JPEG Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.7280.image.encoder.jpeg"
#define COMPONENT_ROLE_JPEG  "image_encoder.jpeg"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif // ENC7280

#ifdef ENC8270
#define VIDEO_COMPONENT_NAME "OMX.hantro.8270.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.8270.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - H264 Encoder */
#define COMPONENT_NAME_H264  "OMX.hantro.8270.video.encoder.avc"
#define COMPONENT_ROLE_H264  "video_encoder.avc"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - JPEG Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.8270.image.encoder.jpeg"
#define COMPONENT_ROLE_JPEG  "image_encoder.jpeg"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENC8270

#ifdef ENC8290
#define VIDEO_COMPONENT_NAME "OMX.hantro.8290.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.8290.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - H264 Encoder */
#define COMPONENT_NAME_H264  "OMX.hantro.8290.video.encoder.avc"
#define COMPONENT_ROLE_H264  "video_encoder.avc"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - JPEG Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.8290.image.encoder.jpeg"
#define COMPONENT_ROLE_JPEG  "image_encoder.jpeg"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENC8290

#ifdef ENCH1
#define VIDEO_COMPONENT_NAME "OMX.hantro.H1.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.H1.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - H264 Encoder */
#define COMPONENT_NAME_H264  "OMX.hantro.H1.video.encoder.avc"
#define COMPONENT_ROLE_H264  "video_encoder.avc"
/** Role 2 - VP8 Encoder */
#define COMPONENT_NAME_VP8  "OMX.hantro.H1.video.encoder.vp8"
#define COMPONENT_ROLE_VP8  "video_encoder.vp8"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - JPEG Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.H1.image.encoder.jpeg"
#define COMPONENT_ROLE_JPEG  "image_encoder.jpeg"
/** Role 2 - WEBP Encoder */
#define COMPONENT_NAME_WEBP  "OMX.hantro.H1.image.encoder.webp"
#define COMPONENT_ROLE_WEBP  "image_encoder.webp"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENCH1

#ifdef ENCH2
#define VIDEO_COMPONENT_NAME "OMX.hantro.H2.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.H2.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - HEVC Encoder */
#define COMPONENT_NAME_HEVC  "OMX.hantro.H2.video.encoder.hevc"
#define COMPONENT_ROLE_HEVC  "video_encoder.hevc"
/** Role 2 - VP9 Encoder */
//#define COMPONENT_NAME_VP9  "OMX.hantro.H2.video.encoder.vp9"
//#define COMPONENT_ROLE_VP9  "video_encoder.vp9"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - HECV Still Image Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.H2.image.encoder.hevc"
#define COMPONENT_ROLE_JPEG  "image_encoder.hevc"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENCH2

#ifdef ENCVC8000E
#define VIDEO_COMPONENT_NAME "OMX.hantro.H2.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.H2.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - HEVC Encoder */
#define COMPONENT_NAME_HEVC  "OMX.hantro.H2.video.encoder.hevc"
#define COMPONENT_ROLE_HEVC  "video_encoder.hevc"
#define COMPONENT_NAME_AVC   "OMX.hantro.H2.video.encoder.avc"
#define COMPONENT_ROLE_AVC   "video_encoder.avc"

/** Role 2 - VP9 Encoder */
//#define COMPONENT_NAME_VP9  "OMX.hantro.H2.video.encoder.vp9"
//#define COMPONENT_ROLE_VP9  "video_encoder.vp9"
#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - HECV Still Image Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.H2.image.encoder.hevc"
#define COMPONENT_ROLE_JPEG  "image_encoder.hevc"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENCVC8000E

#ifdef ENCH2V41
#define VIDEO_COMPONENT_NAME "OMX.hantro.H2.video.encoder"
#define IMAGE_COMPONENT_NAME "OMX.hantro.H2.image.encoder"
//VIDEO DOMAIN ROLES
#ifdef OMX_ENCODER_VIDEO_DOMAIN
/** Role 1 - HEVC Encoder */
#define COMPONENT_NAME_HEVC  "OMX.hantro.H2.video.encoder.hevc"
#define COMPONENT_ROLE_HEVC  "video_encoder.hevc"
#define COMPONENT_NAME_AVC   "OMX.hantro.H2.video.encoder.avc"
#define COMPONENT_ROLE_AVC   "video_encoder.avc"

#endif //~OMX_ENCODER_VIDEO_DOMAIN
//IMAGE DOMAIN ROLES
#ifdef OMX_ENCODER_IMAGE_DOMAIN
/** Role 1 - HECV Still Image Encoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.H2.image.encoder.hevc"
#define COMPONENT_ROLE_JPEG  "image_encoder.hevc"
#endif //OMX_ENCODER_IMAGE_DOMAIN
#endif //ENCVC8000E


#endif // HANTRO_ENCODER_VERSION_H
