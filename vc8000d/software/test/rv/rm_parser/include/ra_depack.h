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

#ifndef RA_DEPACK_H
#define RA_DEPACK_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_error.h"
#include "rm_memory.h"
#include "rm_stream.h"
#include "rm_packet.h"
#include "ra_format_info.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/* Encoded audio block definition. */
typedef struct ra_block_struct
{
    BYTE*  pData;
    UINT32 ulDataLen;
    UINT32 ulTimestamp;
    UINT32 ulDataFlags;
} ra_block;

/* Callback functions */
typedef HX_RESULT (*ra_block_avail_func_ptr) (void*     pAvail,
                                              UINT32    ulSubStream,
                                              ra_block* block);

/*
 * ra_depack definition. Opaque to user.
 */
typedef void ra_depack;

/*
 * ra_depack_create
 *
 * Users should use this function if they don't need to
 * customize their memory allocation routines. In this
 * case, the SDK will use malloc() and free() for 
 * memory allocation.
 */
ra_depack* ra_depack_create(void*                   pAvail,
                            ra_block_avail_func_ptr fpAvail,
                            void*                   pUserError,
                            rm_error_func_ptr       fpError);

/*
 * ra_depack_create2
 *
 * Users should use this function if they need to
 * customize the memory allocation routines.
 */
ra_depack* ra_depack_create2(void*                   pAvail,
                             ra_block_avail_func_ptr fpAvail,
                             void*                   pUserError,
                             rm_error_func_ptr       fpError,
                             void*                   pUserMem,
                             rm_malloc_func_ptr      fpMalloc,
                             rm_free_func_ptr        fpFree);

/*
* ra_depack_create2_ex
*
* This is the same as ra_depack_create2_ex(), but it allows
* the user to use custom memory allocation and free functions,
* especially, the user even can use ncnb memory operations
*/
ra_depack* ra_depack_create2_ex(void*                   pAvail,
								ra_block_avail_func_ptr fpAvail,
								void*                   pUserError,
								rm_error_func_ptr       fpError,
								void*                   pUserMem,
								rm_malloc_func_ptr      fpMalloc,
								rm_free_func_ptr        fpFree,
								rm_malloc_func_ptr      fpMalloc_ncnb,
								rm_free_func_ptr        fpFree_ncnb);

/*
 * ra_depack_init
 *
 * The depacketizer will be initialized with a RealAudio stream header.
 */
HX_RESULT ra_depack_init(ra_depack* pDepack, rm_stream_header* pHdr);

/*
 * ra_depack_get_num_substreams
 *
 * This accessor function tells how many audio substreams there are
 * in this audio stream. For single-rate files, this will be 1. For
 * SureStream audio streams, this could be greater than 1.
 */
UINT32 ra_depack_get_num_substreams(ra_depack* pDepack);

/*
 * ra_depack_get_codec_4cc
 *
 * This accessor function returns the 4cc of the codec. This 4cc
 * will be used to determine which codec to use for this substream.
 */
UINT32 ra_depack_get_codec_4cc(ra_depack* pDepack, UINT32 ulSubStream);

/*
 * ra_depack_get_codec_init_info
 *
 * This function fills in the structure which is used to initialize the codec.
 */
HX_RESULT ra_depack_get_codec_init_info(ra_depack*       pDepack,
                                        UINT32           ulSubStream,
                                        ra_format_info** ppInfo);

/*
 * ra_depack_destroy_codec_init_info
 *
 * This function frees the memory associated with the ra_format_info object
 * created by ra_depack_get_codec_init_info().
 */
void ra_depack_destroy_codec_init_info(ra_depack*       pDepack,
                                       ra_format_info** ppInfo);

/*
 * ra_depack_add_packet
 *
 * Put an audio packet into the depacketizer. When enough data is
 * present to deinterleave, then the user will be called back
 * on the rm_block_avail_func_ptr set in ra_depack_create().
 */
HX_RESULT ra_depack_add_packet(ra_depack* pDepack, rm_packet* pPacket);

/*
 * ra_depack_destroy_block
 *
 * This method cleans up a block received via the
 * ra_block_avail_func_ptr callback.
 */
void ra_depack_destroy_block(ra_depack* pDepack, ra_block** ppBlock);

/*
 * ra_depack_seek
 *
 * The user calls this function if the stream is seeked. It
 * should be called before passing any post-seek packets.
 */
HX_RESULT ra_depack_seek(ra_depack* pDepack, UINT32 ulTime);

/*
 * ra_depack_destroy
 *
 * This cleans up all memory allocated by the ra_depack_* calls
 */
void ra_depack_destroy(ra_depack** ppDepack);

/* following added for media engine -caijin @2008.07.01*/
//UINT32 ra_depack_get_fifo_cnt(ra_depack * pDepack);
//
//HX_RESULT ra_depack_attach_block(ra_depack * pDepack, ra_block * pBlock);
//
//ra_block* ra_depack_deattach_block(ra_depack * pDepack);
//
//HX_RESULT ra_depack_destroy_fifo(ra_depack * pDepack);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RA_DEPACK_H */
