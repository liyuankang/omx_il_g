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

#ifndef ENCODER_H
#define ENCODER_H

/******************************************************
  1. Includes
******************************************************/
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "OMX_Core.h"
#include "OSAL.h"
#include "basecomp.h"
#include "port.h"
#include "util.h"
#include "vsi_vendor_ext.h"


#include "codec.h"

/******************************************************
 2. Module defines
******************************************************/
#define OMX_MAX_GOP_SIZE (8)
#define MAX_IPCM_AREA   (2)
#if defined (ENCVC8000E)
#define MAX_ROI_AREA    (8)
#define MAX_ROI_DELTAQP (8)
#else
#define MAX_ROI_AREA    (2)
#define MAX_ROI_DELTAQP (2)
#endif

/******************************************************
 3. Data types
******************************************************/

typedef struct FRAME_BUFFER
{
    OSAL_BUS_WIDTH bus_address;
    OMX_U8*        bus_data;
    OMX_U32        capacity;   // buffer size
    OMX_U32        size;       // how many bytes is in the buffer currently
    OMX_U32        offset;
}FRAME_BUFFER;

//#ifdef OMX_ENCODER_VIDEO_DOMAIN
typedef struct VIDEO_ENCODER_CONFIG
{
    OMX_VIDEO_PARAM_AVCTYPE             avc;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD     avcIdr; //Only nIDRPeriod is affecting for codec configuration.
                                                //When nPFrames is changed it's stored here and also in
                                                //OMX_VIDEO_PARAM_AVCTYPE structure above.
    OMX_PARAM_DEBLOCKINGTYPE            deblocking;
    OMX_VIDEO_PARAM_H263TYPE            h263;
    OMX_VIDEO_PARAM_MPEG4TYPE           mpeg4;
    OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE ec;
    OMX_VIDEO_PARAM_BITRATETYPE         bitrate; //Only eControlRate is affecting for codec configuration!
                                                 //When target bitrate is changed it's stored here and
                                                 //also in output port configuration. PortChanged event
                                                 //must be created when target rate is changed.
    OMX_CONFIG_FRAMESTABTYPE            stab;
    OMX_VIDEO_PARAM_QUANTIZATIONTYPE    videoQuantization;
    OMX_CONFIG_ROTATIONTYPE             rotation;
    OMX_CONFIG_RECTTYPE                 crop;
    OMX_CONFIG_INTRAREFRESHVOPTYPE      intraRefreshVop;
#ifdef ENCH1
    OMX_VIDEO_PARAM_VP8TYPE             vp8;
    OMX_VIDEO_VP8REFERENCEFRAMETYPE     vp8Ref;
    OMX_VIDEO_CONFIG_ADAPTIVEROITYPE    adaptiveRoi;
    OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE temporalLayer;
    OMX_BOOL                            prependSPSPPSToIDRFrames;
#endif
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2) || defined (ENCVC8000E) || defined (ENCH2V41)
    OMX_VIDEO_CONFIG_INTRAAREATYPE      intraArea;
    OMX_VIDEO_CONFIG_ROIAREATYPE        roiArea[MAX_ROI_AREA];
    OMX_VIDEO_CONFIG_ROIDELTAQPTYPE     roiDeltaQP[MAX_ROI_DELTAQP];
    OMX_VIDEO_CONFIG_IPCMAREATYPE       ipcmArea[MAX_IPCM_AREA];

    OMX_VIDEO_CONFIG_ROIAREATYPE        roi1Area;
    OMX_VIDEO_CONFIG_ROIAREATYPE        roi2Area;
    OMX_VIDEO_CONFIG_ROIDELTAQPTYPE     roi1DeltaQP;
    OMX_VIDEO_CONFIG_ROIDELTAQPTYPE     roi2DeltaQP;
    OMX_VIDEO_PARAM_INTRAREFRESHTYPE    intraRefresh;
#endif
#ifdef ENCH2
    OMX_VIDEO_PARAM_HEVCTYPE             hevc;
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
    OMX_VIDEO_PARAM_CODECFORMAT          codecFormat;
    OMX_VIDEO_PARAM_HEVCTYPE             hevc;
    OMX_VIDEO_PARAM_AVCEXTTYPE           avcExt;
#endif

}VIDEO_ENCODER_CONFIG;
//#endif //OMX_ENCODER_VIDEO_DOMAIN

//#ifdef OMX_ENCODER_IMAGE_DOMAIN
typedef struct IMAGE_ENCODER_CONFIG
{
    OMX_IMAGE_PARAM_QFACTORTYPE         imageQuantization;
    OMX_CONFIG_ROTATIONTYPE             rotation;
    OMX_CONFIG_RECTTYPE                 crop;
}IMAGE_ENCODER_CONFIG;
//#endif

typedef struct OMX_ENCODER
{
    BASECOMP                        base;
    volatile OMX_STATETYPE          state;
    volatile OMX_STATETYPE          statetrans;
    volatile OMX_BOOL               run;
    OMX_CALLBACKTYPE                app_callbacks;
    OMX_U32                         priority_group;
    OMX_U32                         priority_id;
    OMX_PTR                         app_data;
    PORT                            inputPort;
    PORT                            outputPort;
    OMX_STRING                      name;
    FRAME_BUFFER                    frame_in;
    FRAME_BUFFER                    frame_out;
    OMX_PORT_PARAM_TYPE             ports;
    OMX_HANDLETYPE                  self;
    OMX_HANDLETYPE                  statemutex;  // mutex to protect state changes
    OSAL_ALLOCATOR                  alloc;
    ENCODER_PROTOTYPE*              codec;
    OMX_BOOL                        streamStarted;
    OMX_U32                         frameSize;
    OMX_U32                         frameCounter;
    OMX_U8                          role[128];
    OMX_MARKTYPE                    marks[10];
    OMX_U32                         mark_read_pos;
    OMX_U32                         mark_write_pos;
#ifdef OMX_ENCODER_VIDEO_DOMAIN
    VIDEO_ENCODER_CONFIG            encConfig;
    OMX_U32                         busLumaStab;
    OMX_U8*                         busDataStab;
#endif //OMX_ENCODER_VIDEO_DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN
    IMAGE_ENCODER_CONFIG            encConfig;
    OMX_BOOL                        sliceMode;
    OMX_U32                         sliceNum;
    OMX_U32                         numOfSlices;
#ifdef CONFORMANCE
    OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE quant_table;
#endif
#endif //OMX_ENCODER_IMAGE_DOMAIN
    OMX_U64                         curr_frame_start_time;
    OMX_U64                         prev_frame_start_time;
    OMX_U64                         total_time;
    OMX_U64                         min_time;
    OMX_U64                         max_time;
    OMX_BOOL                        perfStarted;

    //re-ordering management
    OMX_U32                         in_frame_id;
    OMX_U32                         out_frame_id;
    OMX_U32                         enc_curr_pic_id;
    OMX_U32                         enc_next_pic_id;

    OMX_BOOL                        input_eos;
} OMX_ENCODER;

/******************************************************
 4. Function prototypes
******************************************************/
OMX_ERRORTYPE HantroHwEncOmx_encoder_init(OMX_HANDLETYPE hComponent);

#endif //~ENCODER_H
