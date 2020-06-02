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

#ifndef ENCODER_H264_H
#define ENCODER_H264_H

#ifdef __cplusplus
extern  "C" {
#endif

#include "codec.h"
#include "OMX_Video.h"

typedef struct H264_CONFIG
{
    // ----------config-----------
    OMX_VIDEO_PARAM_AVCTYPE h264_config;
    
    // ---------codingctrl-----------
    // sliceSize * 16
    // Only slice height can be changed once stream has been started.
    OMX_U32 nSliceHeight;
    
    // I frame period.
    OMX_U32 nPFrames;
    
    // seiMessages
    OMX_BOOL bSeiMessages;

    // videoFullRange
    OMX_U32 nVideoFullRange;

    // constrainedIntraPrediction
    OMX_BOOL bConstrainedIntraPrediction;
    
    // disableDeblockingFilter
    OMX_BOOL bDisableDeblocking;
    
    ENCODER_COMMON_CONFIG common_config;
    RATE_CONTROL_CONFIG rate_config;
    PRE_PROCESSOR_CONFIG pp_config;
} H264_CONFIG;

// create codec instance
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_h264(const H264_CONFIG* config);
// change intra frame period
CODEC_STATE HantroHwEncOmx_encoder_intra_period_h264(ENCODER_PROTOTYPE* arg, OMX_U32 nPFrames);
// change encoding frame rate
CODEC_STATE HantroHwEncOmx_encoder_frame_rate_h264(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate);
#ifdef __cplusplus
}
#endif
#endif // ENCODER_H264_H


