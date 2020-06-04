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

#include "helix_types.h"
#include "rm_property.h"

const char* rm_property_get_name(rm_property* prop)
{
    const char* pRet = HXNULL;

    if (prop)
    {
        pRet = (const char*) prop->pName;
    }

    return pRet;
}

UINT32 rm_property_get_type(rm_property* prop)
{
    UINT32 ulRet = 0;

    if (prop)
    {
        ulRet = prop->ulType;
    }

    return ulRet;
}

UINT32 rm_property_get_value_uint32(rm_property* prop)
{
    UINT32 ulRet = 0;

    if (prop)
    {
        ulRet = (UINT32)((G1_ADDR_T) prop->pValue);
    }

    return ulRet;
}

const char* rm_property_get_value_cstring(rm_property* prop)
{
    const char* pRet = HXNULL;

    if (prop)
    {
        pRet = (const char*) prop->pValue;
    }

    return pRet;
}

UINT32 rm_property_get_value_buffer_length(rm_property* prop)
{
    UINT32 ulRet = 0;

    if (prop)
    {
        ulRet = prop->ulValueLen;
    }

    return ulRet;
}

BYTE* rm_property_get_value_buffer(rm_property* prop)
{
    BYTE* pRet = HXNULL;

    if (prop)
    {
        pRet = prop->pValue;
    }

    return pRet;
}
