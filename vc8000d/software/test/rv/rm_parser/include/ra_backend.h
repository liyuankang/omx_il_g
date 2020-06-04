/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#ifndef RA_BACKEND_H__
#define RA_BACKEND_H__

/* Unified RealAudio decoder backend interface */

#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "ra_format_info.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/* ra decoder backend interface */

typedef HX_RESULT (*ra_decode_init_func_ptr)(void*              pInitParams,
                                             UINT32             ulInitParamsSize,
                                             ra_format_info*    pStreamInfo,
                                             void**             pDecode,
                                             void*              pUserMem,
                                             rm_malloc_func_ptr fpMalloc,
                                             rm_free_func_ptr   fpFree);
typedef HX_RESULT (*ra_decode_reset_func_ptr)(void*   pDecode,
                                              UINT16* pSamplesOut,
                                              UINT32  ulNumSamplesAvail,
                                              UINT32* pNumSamplesOut);
typedef HX_RESULT (*ra_decode_conceal_func_ptr)(void*  pDecode,
                                                UINT32 ulNumSamples);
typedef HX_RESULT (*ra_decode_decode_func_ptr)(void*       pDecode,
                                               UINT8*      pData,
                                               UINT32      ulNumBytes,
                                               UINT32*     pNumBytesConsumed,
                                               UINT16*     pSamplesOut,
                                               UINT32      ulNumSamplesAvail,
                                               UINT32*     pNumSamplesOut,
                                               UINT32      ulFlags);
//frames per block -caijin@2008.07.23
typedef HX_RESULT (*ra_decode_getfpb_func_ptr)(void*   pDecode,
											   UINT32* pNumSamples);
typedef HX_RESULT (*ra_decode_getmaxsize_func_ptr)(void*   pDecode,
                                                   UINT32* pNumSamples);
typedef HX_RESULT (*ra_decode_getchannels_func_ptr)(void*   pDecode,
                                                    UINT32* pNumChannels);
typedef HX_RESULT (*ra_decode_getchannelmask_func_ptr)(void*   pDecode,
                                                       UINT32* pChannelMask);
typedef HX_RESULT (*ra_decode_getrate_func_ptr)(void*   pDecode,
                                                UINT32* pSampleRate);
typedef HX_RESULT (*ra_decode_getdelay_func_ptr)(void*   pDecode,
                                                 UINT32* pNumSamples);
typedef HX_RESULT (*ra_decode_close_func_ptr)(void*            pDecode,
                                              void*            pUserMem,
                                              rm_free_func_ptr fpFree);

#ifdef __cplusplus
}
#endif

#endif /* RA_BACKEND_H__ */
