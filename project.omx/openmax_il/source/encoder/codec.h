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

#ifndef HANTRO_ENCODER_H
#define HANTRO_ENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OSAL.h>

#define VSI_DEFAULT_VALUE (-255)

#ifdef ENC7280
#define TIME_RESOLUTION         30000   // time resolution for h264 [1..65535]
#define TIME_RESOLUTION_MPEG4   30      // [1..65535]
#else
#define TIME_RESOLUTION         90000   // time resolution for h264 & vp8 [1..1048575]
#endif

#define DEFAULT_INPUT_BUFFER_SIZE   460800
#define DEFAULT_OUTPUT_BUFFER_SIZE  230400

// hantro encoder interface

/**
 * Structure for encoded stream
 */
typedef struct STREAM_BUFFER
{
    OMX_U8* bus_data;           // set by common
    OSAL_BUS_WIDTH bus_address; // set by common
    OMX_U32 buf_max_size;       // set by common
    OMX_U32 streamlen;          // set by codec

    OMX_U32 *pOutBuf[9];        // vp8 partition pointers
    OSAL_BUS_WIDTH busAddress[9];     // set by common
    OMX_U32 streamSize[9];      // vp8 partition sizes
    OMX_U32 layerId;            // vp8 temporal layer ID

    //for re-ordering management
    OMX_U32 next_input_frame_id; // init as 0xFFFFFFFF by common, then set by codec to specify next input frame id.
} STREAM_BUFFER;

typedef enum FRAME_TYPE
{
    OMX_INTRA_FRAME,
    OMX_PREDICTED_FRAME
} FRAME_TYPE;

#if defined (ENC8290) || defined (ENCH1)
/* Defines rectangular macroblock area in encoder picture */
typedef struct PICTURE_AREA
{
    OMX_BOOL enable;
    OMX_U32 top;
    OMX_U32 left;
    OMX_U32 bottom;
    OMX_U32 right;
} PICTURE_AREA;
#endif

/**
 * Structure for raw input frame
 */
typedef struct FRAME
{
    OMX_U8* fb_bus_data;
    OSAL_BUS_WIDTH fb_bus_address;
    OMX_U32 fb_frameSize;
    OMX_U32 fb_bufferSize;
    FRAME_TYPE frame_type;
    OMX_U32 bitrate;

    // bus address of the picture to be stabilized
    OSAL_BUS_WIDTH bus_lumaStab;
} FRAME;

typedef enum CODEC_STATE
{
    CODEC_OK,
    CODEC_CODED_INTRA,
    CODEC_CODED_PREDICTED,
    CODEC_CODED_BIDIR_PREDICTED,
    CODEC_CODED_SLICE,
    CODEC_ENQUEUE,
    CODEC_ERROR_HW_TIMEOUT = -1,
    CODEC_ERROR_HW_BUS_ERROR = -2,
    CODEC_ERROR_HW_RESET = -3,
    CODEC_ERROR_SYSTEM = -4,
    CODEC_ERROR_UNSPECIFIED = -5,
    CODEC_ERROR_RESERVED = -6,
    CODEC_ERROR_INVALID_ARGUMENT = -7,
    CODEC_ERROR_BUFFER_OVERFLOW = -8,
    CODEC_ERROR_INVALID_STATE = -9,
    CODEC_ERROR_UNSUPPORTED_SETTING = -10
} CODEC_STATE;

typedef struct ENCODER_PROTOTYPE ENCODER_PROTOTYPE;

// internal ENCODER interface, which wraps up Hantro API
struct ENCODER_PROTOTYPE
{
    void (*destroy)(ENCODER_PROTOTYPE*);
    CODEC_STATE (*stream_start)(ENCODER_PROTOTYPE*, STREAM_BUFFER*);
    CODEC_STATE (*stream_end)(ENCODER_PROTOTYPE*, STREAM_BUFFER*);
    CODEC_STATE (*encode)(ENCODER_PROTOTYPE*, FRAME*, STREAM_BUFFER*, void *cfg);
};

// common configuration structure for encoder pre-processor
typedef struct PRE_PROCESSOR_CONFIG
{
    OMX_U32 origWidth;
    OMX_U32 origHeight;

    // picture cropping
    OMX_U32 xOffset;
    OMX_U32 yOffset;

    // color format conversion
    OMX_COLOR_FORMATTYPE formatType;

    // picture rotation
    OMX_S32 angle;

    // H264 frame stabilization
    OMX_BOOL frameStabilization;

} PRE_PROCESSOR_CONFIG;

typedef struct RATE_CONTROL_CONFIG
{
    // If different than zero it will enable the VOP/picture based rate control. The QP will be
    // changed between VOPs/pictures. Enabled by default.
    //OMX_U32 vopRcEnabled;
    // Value range is [0, 1]
    OMX_S32 nPictureRcEnabled;

    // If different than zero it will enable the macroblock based rate control. The QP will
    // be adjusting also inside a VOP. Enabled by default.
    //OMX_U32 mbRcEnabled;
    // Value range is [0, 1]
    OMX_S32 nMbRcEnabled;

    // If different than zero then frame skipping is enabled, i.e. the rate control is allowed
    // to skip frames if needed. When VBV is enabled, the rate control may have to skip
    // frames despite of this value. Disabled by default.
    //OMX_BOOL bVopSkipEnabled;
    // Value range is [0, 1]
    //OMX_U32 nSkippingEnabled;

    // Valid value range: [1, 31]
    // The default quantization parameter used by the encoder. If the rate control is
    // enabled then this value is used just at the beginning of the encoding process.
    // When the rate control is disabled then this QP value is used all the time. Default
    // value is 10.
    OMX_S32 nQpDefault;

    // The minimum QP that can be used by the RC in the stream. Default value is 1.
    // Valid value range: [1, 31]
    OMX_S32 nQpMin;

    // The maximum QP that can be used by the RC in the stream. Default value is 31.
    // Valid value range: [1, 31]
    OMX_S32 nQpMax;

    // The target bit rate as bits per second (bps) when the rate control is enabled. The
    // rate control is considered enabled when any of the VOP or MB based rate control is
    // enabled. Default value is the minimum between the maximum bitrate allowed for
    // the selected profile&level and 4mbps.
    OMX_S32 nTargetBitrate;

    //OMX_U32 eControlRate;

    // If different than zero it will enable the VBV model. Value 1 represents the video
    // buffer size defined by the standard. Otherwise this value represents the video
    // buffer size in units of 16384 bits. Enabled by default with size set by standard.
    // NOTE! This should always be ON since turning it OFF may not be 100% standard compatible.
    // (See Hantro MPEG4 API User Manual)
    OMX_U32 nVbvEnabled;

    OMX_VIDEO_CONTROLRATETYPE eRateControl;

    // Hypothetical Reference Decoder model, [0,1]
    // restricts the instantaneous bitrate and
    // total bit amount of every coded picture.
    // Enabling HRD will cause tight constrains
    // on the operation of the rate control.
    // NOTE! If HRD is OFF output stream may not be
    // 100% standard compatible. (see hantro API user guide)
    // NOTE! If HDR is used, bitrate can't be changed after stream
    // has been created.
    // u32 hrd;
    OMX_S32 nHrdEnabled;

    // size in bits of Coded Picture Buffer used in HRD
    // NOTE: check if OMX supports this?
    // NOTE! Do not change unless you really know what you are doing!
    // u32 hrdCpbSize;
    //OMX_U32 hrdCobSize;
} RATE_CONTROL_CONFIG;

typedef struct ENCODER_COMMON_CONFIG
{
    OMX_U32 nOutputWidth;
    OMX_U32 nOutputHeight;

    // frameRateNum
    // frameRateDenom
    OMX_U32 nInputFramerate;    // framerate in Q16 format
    OMX_U32 nOutputFramerate;    // framerate in Q16 format

    OMX_U32 nBitDepthLuma;
    OMX_U32 nBitDepthChroma;

    OMX_U32 nMaxTLayers;

} ENCODER_COMMON_CONFIG;

#ifdef __cplusplus
}
#endif
#endif // HANTRO_ENCODER_H
