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

#ifndef RV_FORMAT_INFO_H
#define RV_FORMAT_INFO_H

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

#include "helix_types.h"
#include "rm_memory.h"

typedef struct rv_format_info_struct
{
    UINT32   ulLength;
    UINT32   ulMOFTag;
    UINT32   ulSubMOFTag;
    UINT16   usWidth;
    UINT16   usHeight;
    UINT16   usBitCount;
    UINT16   usPadWidth;
    UINT16   usPadHeight;
    UFIXED32 ufFramesPerSecond;
    UINT32   ulOpaqueDataSize;
    BYTE*    pOpaqueData;
} rv_format_info;

typedef struct rv8_info_struct
{
	UINT32 ulpctszSize;
	UINT32 ulMpSize[2*(8+1)];
} rv8_info_ex;

/*
 * RV frame struct.
 */
typedef struct rv_segment_struct
{
    HXBOOL bIsValid;
    UINT32 ulOffset;
} rv_segment;

typedef struct rv_frame_struct
{
    UINT32			ulDataLen;
    BYTE*			pData;
    UINT32			ulTimestamp;
    UINT16			usSequenceNum;
    UINT16			usFlags;
    HXBOOL			bLastPacket;
    UINT32			ulNumSegments;
    rv_segment*		pSegment;

	/* added for NCNB support -caijin @2008.06.04 */
	rm_linear_memory*	pStream;		/* same as pData, but NCNB mode support */
	G1_ADDR_T		ulDataBusAddress;
		
	/* added by caijin @2008.05.28 */
	//HXBOOL			bValid;				/* all data are prepared for the current frame */
	//UINT32			ulFrameDuration;	/* difference between the ulTimestamp's of two adjacent frames */
	UINT8			ucCodType;			/* determined by first byte of pData */
	HXBOOL			bIsKeyframe;		/* determined by usFlags & HX_KEYFRAME_FLAG */
	HXBOOL			bIsExternalBuf;		/* flag to whether the stream buffer is allocated externally or not */
} rv_frame;

#define BYTE_SWAP_UINT16(A)  ((((UINT16)(A) & 0xff00) >> 8) | \
                              (((UINT16)(A) & 0x00ff) << 8))
#define BYTE_SWAP_UINT32(A)  ((((UINT32)(A) & 0xff000000) >> 24) | \
                              (((UINT32)(A) & 0x00ff0000) >> 8)  | \
                              (((UINT32)(A) & 0x0000ff00) << 8)  | \
                              (((UINT32)(A) & 0x000000ff) << 24))

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RV_FORMAT_INFO_H */
