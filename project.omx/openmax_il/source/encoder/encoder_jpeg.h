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

#ifndef ENCODER_JPEG_H_
#define ENCODER_JPEG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "codec.h"
#include "OMX_Video.h"
#include "jpegencapi.h"
    
typedef struct JPEG_CONFIG {
    // ----------config-----------

    OMX_U32 qLevel;     // The quantization level. [0..9]
    OMX_U32 sliceHeight;    

    // ---------- APP0 header related config -----------
    OMX_BOOL bAddHeaders;
    JpegEncAppUnitsType unitsType;      // Specifies unit type of xDensity and yDensity in APP0 header 
    JpegEncTableMarkerType markerType;  // Specifies the type of the DQT and DHT headers.
    JpegEncCodingType codingType;   
    OMX_U32 xDensity;                   // Horizontal pixel density to APP0 header.
    OMX_U32 yDensity;                   // Vertical pixel density to APP0 header.
    
    //OMX_U32 comLength;                // length of comment header data
    //OMX_U8    *pCom;                  // Pointer to comment header data of size comLength.
    //OMX_U32 thumbnail;                // Indicates if thumbnail is added to stream.
    
    // ---------- encoder options -----------
    OMX_U32 codingWidth;
    OMX_U32 codingHeight;
    OMX_U32 yuvFormat; // output picture YUV format
    PRE_PROCESSOR_CONFIG pp_config;
    

} JPEG_CONFIG;

// create codec instance
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_jpeg(const JPEG_CONFIG* config);

#ifdef __cplusplus
}
#endif

#endif /*CODEC_JPEG_H_*/
