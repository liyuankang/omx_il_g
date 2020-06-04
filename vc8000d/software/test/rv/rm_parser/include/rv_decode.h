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

#ifndef RV_DECODE_H__
#define RV_DECODE_H__

/* Simple unified decoder frontend for RealVideo */

#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "rm_error.h"
#include "rv_format_info.h"
#include "rv_decode_message.h"
#include "rv_backend.h"
#include "rv_backend_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The rv_decode struct contains the RealVideo decoder frontend
 * state variables and backend instance pointer. */

typedef struct rv_decode_struct
{
    rm_error_func_ptr          fpError;
        /* User defined error function.                                   */

    void*                      pUserError;
        /* User defined parameter for error function.                     */

    rm_malloc_func_ptr         fpMalloc;
        /* User defined malloc function.                                  */

    rm_free_func_ptr           fpFree;
        /* User defined free function.                                    */

    void*                      pUserMem;
        /* User defined parameter for malloc and free functions.          */

    UINT32                     ulSPOExtra;
        /* The SPO Extra bits. Opaque data appended to bitstream header.  */

    UINT32                     ulStreamVersion;
        /* The stream version. Opaque data following the SPO Extra bits.  */

    UINT32                     ulMajorBitstreamVersion;
        /* The major bitstream version. Indicates RV7, RV8, RV9, etc.     */

    UINT32                     ulMinorBitstreamVersion;
        /* The minor bitstream version. Indicates minor revision or RAW.  */

    UINT32                     ulNumResampledImageSizes;
        /* The number of RPR sizes. Optional RV7 and RV8 opaque data.     */

    UINT32                     ulEncodeSize;
        /* The maximum encoded frame dimensions. Optional RV9 opaque data.*/

    UINT32                     ulLargestPels;
        /* The maximum encoded frame width.                               */

    UINT32                     ulLargestLines;
        /* The maximum encoded frame height.                              */

    UINT32                     pDimensions[2*(8+1)];
        /* Table of encoded dimensions, including RPR sizes.              */

	HXBOOL                     bIsRV8;
		/* The flag indicating the stream is RV8 or not.        -caijin   */

    UINT32                     ulNumImageSizes;
        /* The number of encoded sizes, including RPR sizes.    -caijin   */

	UINT32                     ulPctszSize;
		/* The bit width of the number of encoded sizes.        -caijin   */

	UINT32                     ulOutSize;             
        /* The maximum size of the output frame in bytes.                 */

    UINT32                     ulECCMask;
        /* Mask for identifying ECC packets.                              */

    UINT32                     bInputFrameIsReference;
        /* Identifies whether input frame is a key frame or not.          */

    UINT32                     ulInputFrameQuant;
        /* The input frame quantization parameter.                        */

    rv_format_info            *pBitstreamHeader;
        /* The bitstream header.                                          */

    rv_frame                  *pInputFrame;      
        /* Pointer to the input frame struct.                             */

    void                      *pDecodeState;        
        /* Pointer to decoder backend state.                              */

    rv_backend                *pDecode; 
        /* Decoder backend function pointers.                             */

    rv_backend_init_params     pInitParams;    
        /* Initialization parameters for the decoder backend.             */

    rv_backend_in_params       pInputParams;
        /* The decoder backend input parameter struct.                    */

    rv_backend_out_params      pOutputParams;
        /* The decoder backend output parameter struct.                   */

} rv_decode;

/* rv_decode_create()
 * Creates RV decoder frontend struct, copies memory utilities.
 * Returns struct pointer on success, NULL on failure. */
rv_decode* rv_decode_create(void* pUserError, rm_error_func_ptr fpError);

rv_decode* rv_decode_create2(void* pUserError, rm_error_func_ptr fpError,
                             void* pUserMem, rm_malloc_func_ptr fpMalloc,
                             rm_free_func_ptr fpFree);

/* rv_decode_destroy()
 * Deletes decoder backend instance, followed by frontend. */
void rv_decode_destroy(rv_decode* pFrontEnd);

/* rv_decode_init()
 * Reads bitstream header, selects and initializes decoder backend.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT rv_decode_init(rv_decode* pFrontEnd, rv_format_info* pHeader);

/* rv_decode_stream_input()
 * Reads frame header and fills decoder input parameters struct. If there
 * is packet loss and ECC packets exist, error correction is attempted.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT rv_decode_stream_input(rv_decode* pFrontEnd, rv_frame* pFrame);

/* rv_decode_stream_flush()
 * Flushes the latency frame from the decoder backend after the last frame
 * is delivered and decoded before a pause or the end-of-file.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT rv_decode_stream_flush(rv_decode* pFrontEnd, UINT8* pOutput);

/* rv_decode_custom_message()
 * Sends a custom message to the decoder backend.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT rv_decode_custom_message(rv_decode* pFrontEnd, RV_Custom_Message_ID *pMsg_id);


/**************** Accessor Functions *******************/
/* rv_decode_max_output_size()
 * Returns maximum size of YUV 4:2:0 output buffer in bytes. */
UINT32 rv_decode_max_output_size(rv_decode* pFrontEnd);

/* rv_decode_get_output_size()
 * Returns size of most recent YUV 4:2:0 output buffer in bytes. */
UINT32 rv_decode_get_output_size(rv_decode* pFrontEnd);

/* rv_decode_get_output_dimensions()
 * Returns width and height of most recent YUV output buffer. */
HX_RESULT rv_decode_get_output_dimensions(rv_decode* pFrontEnd, UINT32* pWidth,
                                          UINT32* pHeight);

/* rv_decode_frame_valid()
 * Checks decoder output parameters to see there is a valid output frame.
 * Returns non-zero value if a valid output frame exists, else zero. */
UINT32 rv_decode_frame_valid(rv_decode* pFrontEnd);

/* rv_decode_more_frames()
 * Checks decoder output parameters to see if more output frames can be
 * produced without additional input frames.
 * Returns non-zero value if more frames can be
 * produced without additional input, else zero. */
UINT32 rv_decode_more_frames(rv_decode* pFrontEnd);

#ifdef __cplusplus
}
#endif

#endif /* RV_DECODE_H__ */
