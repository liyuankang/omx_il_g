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

//#include <memory.h>
#include "string.h"
#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "pack_utils.h"
#include "stream_hdr_structs.h"
#include "stream_hdr_utils.h"

HX_RESULT rm_unpack_rule_map(BYTE**             ppBuf,
                             UINT32*            pulLen,
                             rm_malloc_func_ptr fpMalloc,
                             rm_free_func_ptr   fpFree,
                             void*              pUserMem,
                             rm_rule_map*       pMap)
{
    HX_RESULT retVal = HXR_FAIL;

    if (ppBuf && pulLen && fpMalloc && fpFree && pMap &&
        *ppBuf && *pulLen >= 2)
    {
        /* Initialize local variables */
        UINT32 ulSize = 0;
        UINT32 i      = 0;
        /* Clean up any existing rule to flag map */
        rm_cleanup_rule_map(fpFree, pUserMem, pMap);
        /* Unpack the number of rules */
        pMap->ulNumRules = rm_unpack16(ppBuf, pulLen);
        if (pMap->ulNumRules && *pulLen >= pMap->ulNumRules * 2)
        {
            /* Allocate the map array */
            ulSize = pMap->ulNumRules * sizeof(UINT32);
            pMap->pulMap = (UINT32*) fpMalloc(pUserMem, ulSize);
            if (pMap->pulMap)
            {
                /* Zero out the memory */
                memset(pMap->pulMap, 0, ulSize);
                /* Unpack each of the flags */
                for (i = 0; i < pMap->ulNumRules; i++)
                {
                    pMap->pulMap[i] = rm_unpack16(ppBuf, pulLen);
                }
                /* Clear the return value */
                retVal = HXR_OK;
            }
        }
        else
        {
            /* No rules - not an error */
            retVal = HXR_OK;
        }
    }

    return retVal;
}

void rm_cleanup_rule_map(rm_free_func_ptr fpFree,
                         void*            pUserMem,
                         rm_rule_map*     pMap)
{
    if (fpFree && pMap && pMap->pulMap)
    {
        fpFree(pUserMem, pMap->pulMap);
        pMap->pulMap     = HXNULL;
        pMap->ulNumRules = 0;
    }
}

HX_RESULT rm_unpack_multistream_hdr(BYTE**              ppBuf,
                                    UINT32*             pulLen,
                                    rm_malloc_func_ptr  fpMalloc,
                                    rm_free_func_ptr    fpFree,
                                    void*               pUserMem,
                                    rm_multistream_hdr* hdr)
{
    HX_RESULT retVal = HXR_FAIL;

    if (ppBuf && pulLen && fpMalloc && fpFree && hdr &&
        *ppBuf && *pulLen >= 4)
    {
        /* Unpack the multistream members */
        hdr->ulID = rm_unpack32(ppBuf, pulLen);
        /* Unpack the rule to substream map */
        retVal = rm_unpack_rule_map(ppBuf, pulLen,
                                    fpMalloc, fpFree, pUserMem,
                                    &hdr->rule2SubStream);
        if (retVal == HXR_OK)
        {
            if (*pulLen >= 2)
            {
                /* Unpack the number of substreams */
                hdr->ulNumSubStreams = rm_unpack16(ppBuf, pulLen);
            }
            else
            {
                retVal = HXR_FAIL;
            }
        }
    }

    return retVal;
}

void rm_cleanup_multistream_hdr(rm_free_func_ptr    fpFree,
                                void*               pUserMem,
                                rm_multistream_hdr* hdr)
{
    if (hdr)
    {
        rm_cleanup_rule_map(fpFree, pUserMem, &hdr->rule2SubStream);
    }
}
