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

#ifndef RV_DEPACK_H
#define RV_DEPACK_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_error.h"
#include "rm_memory.h"
#include "rm_stream.h"
#include "rm_packet.h"
#include "rv_format_info.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/* Callback functions */
typedef HX_RESULT (*rv_frame_avail_func_ptr) (void* pAvail, UINT32 ulSubStreamNum, rv_frame* frame);

/*
 * rv_depack definition. Opaque to user.
 */
typedef void rv_depack;

/*
 * rv_depack_create
 */
rv_depack* rv_depack_create(void*                   pAvail,
                            rv_frame_avail_func_ptr fpAvail,
                            void*                   pUserError,
                            rm_error_func_ptr       fpError);

/*
 * rv_depack_create2
 *
 * This is the same as rv_depack_create(), but it allows
 * the user to use custom memory allocation and free functions.
 */
rv_depack* rv_depack_create2(void*                   pAvail,
                             rv_frame_avail_func_ptr fpAvail,
                             void*                   pUserError,
                             rm_error_func_ptr       fpError,
                             void*                   pUserMem,
                             rm_malloc_func_ptr      fpMalloc,
                             rm_free_func_ptr        fpFree);

/*
 * rv_depack_init
 *
 * This is initialized with a RealVideo rm_stream_header struct.
 */
HX_RESULT rv_depack_init(rv_depack* pDepack, rm_stream_header* header);

/*
 * rv_depack_get_num_substreams
 *
 * This accessor function tells how many video substreams there are
 * in this video stream. For single-rate video streams, this will be 1. For
 * SureStream video streams, this could be greater than 1.
 */
UINT32 rv_depack_get_num_substreams(rv_depack* pDepack);

/*
 * rv_depack_get_codec_4cc
 *
 * This accessor function returns the 4cc of the codec. This 4cc
 * will be used to determine which codec to use. RealVideo always
 * uses the same codec for all substreams.
 */
UINT32 rv_depack_get_codec_4cc(rv_depack* pDepack);

/*
 * rv_depack_get_codec_init_info
 *
 * This function fills in the structure which is used to initialize the codec.
 */
HX_RESULT rv_depack_get_codec_init_info(rv_depack* pDepack, rv_format_info** ppInfo);

/*
 * rv_depack_destroy_codec_init_info
 *
 * This function frees the memory associated with the rv_format_info object
 * created by rv_depack_get_codec_init_info().
 */
void rv_depack_destroy_codec_init_info(rv_depack* pDepack, rv_format_info** ppInfo);

/*
 * rv_depack_add_packet
 *
 * Put a video packet into the depacketizer. When enough data is
 * present to depacketize, then the user will be called back
 * on the rv_frame_avail_func_ptr set in rv_depack_create().
 */
HX_RESULT rv_depack_add_packet(rv_depack* pDepack, rm_packet* packet);

/*
 * rv_depack_destroy_frame
 *
 * This cleans up a frame that was returned via
 * the rv_frame_avail_func_ptr.
 */
void rv_depack_destroy_frame(rv_depack* pDepack, rv_frame** ppFrame);

/*
 * rv_depack_seek
 *
 * The user calls this function if the stream is seeked. It
 * should be called before passing any post-seek packets. After calling
 * rv_depack_seek(), no more pre-seek packets should be
 * passed into rv_depack_add_packet().
 */
HX_RESULT rv_depack_seek(rv_depack* pDepack, UINT32 ulTime);

/*
 * rv_depack_destroy
 *
 * This cleans up all memory allocated by the rv_depack_* calls
 */
void rv_depack_destroy(rv_depack** ppDepack);

/*
* rv_depack_create2_ex
*
* This is the same as rv_depack_create2(), but it allows
* the user to use custom memory allocation and free functions,
* especially, the user even can use ncnb memory operations
*/
rv_depack* rv_depack_create2_ex(void*                   pAvail,
								rv_frame_avail_func_ptr fpAvail,
								void*                   pUserError,
								rm_error_func_ptr       fpError,
								void*                   pUserMem,
								rm_malloc_func_ptr      fpMalloc,
								rm_free_func_ptr        fpFree,
								rm_malloc_func_ptr      fpMalloc_ncnb,
								rm_free_func_ptr        fpFree_ncnb);

/*
* rv_depack_add_packet_ex
*
* Put a video packet into the depacketizer.
* user can assign a specical depack mode:
* callback or send-to-fifo ?
* packet-by-packet or frame-by-frame ?
*/
HX_RESULT rv_depack_add_packet_ex(rv_depack * pDepack, rm_packet * packet, UINT32 ulDepackMode);

/* following added for media engine -caijin @2008.06.01*/
//UINT32 rv_depack_get_fifo_cnt(rv_depack * pDepack);
HX_RESULT rv_depack_attach_frame(rv_depack * pDepack, rv_frame * pFrame);
//rv_frame* rv_depack_deattach_frame(rv_depack * pDepack);
//HX_RESULT rv_depack_destroy_fifo(rv_depack * pDepack);

UINT32 rv_depack_next_keyframe_sn(rv_depack* pDepack,UINT32 ulTimeNow);

UINT8 rv_depack_get_pictype(rv_depack *pDepack, rv_frame *pFrame);

HXBOOL rv_depack_is_keyframe(rv_depack *pDepack, rv_frame *pFrame);

void rv_depack_destroy_frame_ex(rv_depack* pDepack, rv_frame** ppFrame);

/* The last frame is hold by pCurFrame */
HX_RESULT rv_depack_get_last_frame(rv_depack *pDepack, rv_frame **ppFrame);

/* The read frame is hold by pReadyFrame */
HX_RESULT rv_depack_get_ready_frame(rv_depack *pDepack, rv_frame **ppFrame);

HXBOOL rv_depack_is_frame_ready(rv_depack * pDepack);

HX_RESULT rv_depack_set_frame_buffer(rv_depack *pDepack, BYTE *bufAddr, UINT32 bufLen);

void rv_depack_cleanup_frame_buffer(rv_depack *pDepack);

void rv_depack_reset(rv_depack *pDepack);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RV_DEPACK_H */
