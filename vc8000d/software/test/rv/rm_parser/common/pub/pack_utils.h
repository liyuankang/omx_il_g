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

#ifndef PACK_UTILS_H
#define PACK_UTILS_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"

/* Pack a 32-bit value big-endian */
void rm_pack32(UINT32 ulValue, BYTE** ppBuf, UINT32* pulLen);
/* Pack a 32-bit value little-endian */
void rm_pack32_le(UINT32 ulValue, BYTE** ppBuf, UINT32* pulLen);
/* Pack a 16-bit value big-endian */
void rm_pack16(UINT16 usValue, BYTE** ppBuf, UINT32* pulLen);
/* Pack a 16-bit value little-endian */
void rm_pack16_le(UINT16 usValue, BYTE** ppBuf, UINT32* pulLen);
void rm_pack8(BYTE ucValue, BYTE** ppBuf, UINT32* pulLen);

/* Unpacking utilties */
UINT32    rm_unpack32(BYTE** ppBuf, UINT32* pulLen);
UINT16    rm_unpack16(BYTE** ppBuf, UINT32* pulLen);
/* rm_unpack32() and rm_unpack16() have the side
 * effect of incrementing *ppBuf and decrementing
 * *pulLen. The functions below (rm_unpack32_nse()
 * rm_unpack16_nse()) do not change pBuf and ulLen.
 * That is, they have No Side Effect, hence the suffix
 * _nse.
 */
UINT32    rm_unpack32_nse(BYTE* pBuf, UINT32 ulLen);
UINT16    rm_unpack16_nse(BYTE* pBuf, UINT32 ulLen);
BYTE      rm_unpack8(BYTE** ppBuf, UINT32* pulLen);
HX_RESULT rm_unpack_string(BYTE**             ppBuf,
                           UINT32*            pulLen,
                           UINT32             ulStrLen,
                           char**             ppStr,
                           void*              pUserMem,
                           rm_malloc_func_ptr fpMalloc,
                           rm_free_func_ptr   fpFree);
HX_RESULT rm_unpack_buffer(BYTE**             ppBuf,
                           UINT32*            pulLen,
                           UINT32             ulBufLen,
                           BYTE**             ppUnPackBuf,
                           void*              pUserMem,
                           rm_malloc_func_ptr fpMalloc,
                           rm_free_func_ptr   fpFree);
HX_RESULT rm_unpack_array(BYTE**             ppBuf,
                          UINT32*            pulLen,
                          UINT32             ulNumElem,
                          UINT32             ulElemSize,
                          void**             ppArr,
                          void*              pUserMem,
                          rm_malloc_func_ptr fpMalloc,
                          rm_free_func_ptr   fpFree);

UINT32 rm_unpack32_from_byte_string(BYTE** ppBuf, UINT32* pulLen);

#endif /* #ifndef PACK_UTILS_H */
