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

#include <string.h>
#include "helix_types.h"
#include "helix_mime_types.h"
#include "rm_property.h"
#include "rm_stream.h"

UINT32 rm_stream_get_number(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulStreamNumber;
    }

    return ulRet;
}

UINT32 rm_stream_get_max_bit_rate(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulMaxBitRate;
    }

    return ulRet;
}

UINT32 rm_stream_get_avg_bit_rate(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulAvgBitRate;
    }

    return ulRet;
}

UINT32 rm_stream_get_max_packet_size(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulMaxPacketSize;
    }

    return ulRet;
}

UINT32 rm_stream_get_avg_packet_size(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulAvgPacketSize;
    }

    return ulRet;
}

UINT32 rm_stream_get_start_time(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulStartTime;
    }

    return ulRet;
}

UINT32 rm_stream_get_preroll(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulPreroll;
    }

    return ulRet;
}

UINT32 rm_stream_get_duration(rm_stream_header* hdr)
{
    UINT32 ulRet = 0;

    if (hdr)
    {
        ulRet = hdr->ulDuration;
    }

    return ulRet;
}

const char* rm_stream_get_name(rm_stream_header* hdr)
{
    const char* pRet = HXNULL;

    if (hdr)
    {
        pRet = (const char*) hdr->pStreamName;
    }

    return pRet;
}

const char* rm_stream_get_mime_type(rm_stream_header* hdr)
{
    const char* pRet = HXNULL;

    if (hdr)
    {
        pRet = (const char*) hdr->pMimeType;
    }

    return pRet;
}

UINT32 rm_stream_get_properties(rm_stream_header* hdr, rm_property** ppProp)
{
    UINT32 ulRet = 0;

    if (hdr && ppProp)
    {
        *ppProp = hdr->pProperty;
        ulRet   = hdr->ulNumProperties;
    }

    return ulRet;
}

HXBOOL rm_stream_is_realaudio(rm_stream_header* hdr)
{
    HXBOOL bRet = FALSE;

    if (hdr)
    {
        bRet = rm_stream_is_realaudio_mimetype((const char*) hdr->pMimeType);
    }

    return bRet;
}

HXBOOL rm_stream_is_realvideo(rm_stream_header* hdr)
{
    HXBOOL bRet = FALSE;

    if (hdr)
    {
        bRet = rm_stream_is_realvideo_mimetype((const char*) hdr->pMimeType);
    }

    return bRet;
}

HXBOOL rm_stream_is_realevent(rm_stream_header* hdr)
{
    HXBOOL bRet = FALSE;

    if (hdr)
    {
        bRet = rm_stream_is_realevent_mimetype((const char*) hdr->pMimeType);
    }

    return bRet;
}

HXBOOL rm_stream_is_realaudio_mimetype(const char* pszStr)
{
    HXBOOL bRet = FALSE;

    if (pszStr)
    {
        if (!strcmp(pszStr, REALAUDIO_MIME_TYPE) ||
            !strcmp(pszStr, REALAUDIO_ENCRYPTED_MIME_TYPE))
        {
            bRet = TRUE;
        }
    }

    return bRet;
}

HXBOOL rm_stream_is_realvideo_mimetype(const char* pszStr)
{
    HXBOOL bRet = FALSE;

    if (pszStr)
    {
        if (!strcmp(pszStr, REALVIDEO_MIME_TYPE) ||
            !strcmp(pszStr, REALVIDEO_ENCRYPTED_MIME_TYPE))
        {
            bRet = TRUE;
        }
    }

    return bRet;
}

HXBOOL rm_stream_is_realevent_mimetype(const char* pszStr)
{
    HXBOOL bRet = FALSE;

    if (pszStr)
    {
        if (!strcmp(pszStr, REALEVENT_MIME_TYPE)              ||
            !strcmp(pszStr, REALEVENT_ENCRYPTED_MIME_TYPE)    ||
            !strcmp(pszStr, REALIMAGEMAP_MIME_TYPE)           ||
            !strcmp(pszStr, REALIMAGEMAP_ENCRYPTED_MIME_TYPE) ||
            !strcmp(pszStr, IMAGEMAP_MIME_TYPE)               ||
            !strcmp(pszStr, IMAGEMAP_ENCRYPTED_MIME_TYPE)     ||
            !strcmp(pszStr, SYNCMM_MIME_TYPE)                 ||
            !strcmp(pszStr, SYNCMM_ENCRYPTED_MIME_TYPE))
        {
            bRet = TRUE;
        }
    }

    return bRet;
}

HXBOOL rm_stream_is_real_mimetype(const char* pszStr)
{
    return rm_stream_is_realaudio_mimetype(pszStr) ||
           rm_stream_is_realvideo_mimetype(pszStr) ||
           rm_stream_is_realevent_mimetype(pszStr);
}

HX_RESULT rm_stream_get_property_int(rm_stream_header* hdr,
                                     const char*       pszStr,
                                     UINT32*           pulVal)
{
    HX_RESULT retVal = HXR_FAIL;

    if (hdr && pszStr && pulVal &&
        hdr->pProperty && hdr->ulNumProperties)
    {
        UINT32 i = 0;
        for (i = 0; i < hdr->ulNumProperties; i++)
        {
            rm_property* pProp = &hdr->pProperty[i];
            if (pProp->ulType == RM_PROPERTY_TYPE_UINT32 &&
                pProp->pName                             &&
                !strcmp(pszStr, (const char*) pProp->pName))
            {
                /* Assign the out parameter */
                *pulVal = (UINT32)((G1_ADDR_T) pProp->pValue);
                /* Clear the return value */
                retVal = HXR_OK;
                break;
            }
        }
    }

    return retVal;
}

HX_RESULT rm_stream_get_property_buf(rm_stream_header* hdr,
                                     const char*       pszStr,
                                     BYTE**            ppBuf,
                                     UINT32*           pulLen)
{
    HX_RESULT retVal = HXR_FAIL;

    if (hdr && pszStr && ppBuf && pulLen &&
        hdr->pProperty && hdr->ulNumProperties)
    {
        UINT32 i = 0;
        for (i = 0; i < hdr->ulNumProperties; i++)
        {
            rm_property* pProp = &hdr->pProperty[i];
            if (pProp->ulType == RM_PROPERTY_TYPE_BUFFER &&
                pProp->pName                             &&
                !strcmp(pszStr, (const char*) pProp->pName))
            {
                /* Assign the out parameters */
                *ppBuf  = pProp->pValue;
                *pulLen = pProp->ulValueLen;
                /* Clear the return value */
                retVal = HXR_OK;
                break;
            }
        }
    }

    return retVal;
}

HX_RESULT rm_stream_get_property_str(rm_stream_header* hdr,
                                     const char*       pszStr,
                                     char**            ppszStr)
{
    HX_RESULT retVal = HXR_FAIL;

    if (hdr && pszStr && ppszStr &&
        hdr->pProperty && hdr->ulNumProperties)
    {
        UINT32 i = 0;
        for (i = 0; i < hdr->ulNumProperties; i++)
        {
            rm_property* pProp = &hdr->pProperty[i];
            if (pProp->ulType == RM_PROPERTY_TYPE_CSTRING &&
                pProp->pName                              &&
                !strcmp(pszStr, (const char*) pProp->pName))
            {
                /* Assign the out parameter */
                *ppszStr = (char*) pProp->pValue;
                /* Clear the return value */
                retVal = HXR_OK;
                break;
            }
        }
    }

    return retVal;
}
