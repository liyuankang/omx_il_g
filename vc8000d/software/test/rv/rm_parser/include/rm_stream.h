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

#ifndef RM_STREAM_H
#define RM_STREAM_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_property.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/*
 * Stream header definition.
 *
 * Users are strongly encouraged to use the accessor
 * functions below to retrieve information from the 
 * rm_stream_header struct, since the definition
 * may change in the future.
 */
typedef struct rm_stream_header_struct
{
    char*        pMimeType;
    char*        pStreamName;
    UINT32       ulStreamNumber;
    UINT32       ulMaxBitRate;
    UINT32       ulAvgBitRate;
    UINT32       ulMaxPacketSize;
    UINT32       ulAvgPacketSize;
    UINT32       ulDuration;
    UINT32       ulPreroll;
    UINT32       ulStartTime;
    UINT32       ulOpaqueDataLen;
    BYTE*        pOpaqueData;
    UINT32       ulNumProperties;
    rm_property*  pProperty;
} rm_stream_header;

/*
 * These are the accessor functions used to retrieve
 * information from the stream header
 */
UINT32      rm_stream_get_number(rm_stream_header* hdr);
UINT32      rm_stream_get_max_bit_rate(rm_stream_header* hdr);
UINT32      rm_stream_get_avg_bit_rate(rm_stream_header* hdr);
UINT32      rm_stream_get_max_packet_size(rm_stream_header* hdr);
UINT32      rm_stream_get_avg_packet_size(rm_stream_header* hdr);
UINT32      rm_stream_get_start_time(rm_stream_header* hdr);
UINT32      rm_stream_get_preroll(rm_stream_header* hdr);
UINT32      rm_stream_get_duration(rm_stream_header* hdr);
const char* rm_stream_get_name(rm_stream_header* hdr);
const char* rm_stream_get_mime_type(rm_stream_header* hdr);
UINT32      rm_stream_get_properties(rm_stream_header* hdr, rm_property** ppProp);
HXBOOL      rm_stream_is_realaudio(rm_stream_header* hdr);
HXBOOL      rm_stream_is_realvideo(rm_stream_header* hdr);
HXBOOL      rm_stream_is_realevent(rm_stream_header* hdr);
HXBOOL      rm_stream_is_realaudio_mimetype(const char* pszStr);
HXBOOL      rm_stream_is_realvideo_mimetype(const char* pszStr);
HXBOOL      rm_stream_is_realevent_mimetype(const char* pszStr);
HXBOOL      rm_stream_is_real_mimetype(const char* pszStr);
HX_RESULT   rm_stream_get_property_int(rm_stream_header* hdr,
                                       const char*       pszStr,
                                       UINT32*           pulVal);
HX_RESULT   rm_stream_get_property_buf(rm_stream_header* hdr,
                                       const char*       pszStr,
                                       BYTE**            ppBuf,
                                       UINT32*           pulLen);
HX_RESULT   rm_stream_get_property_str(rm_stream_header* hdr,
                                       const char*       pszStr,
                                       char**            ppszStr);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RM_STREAM_H */
