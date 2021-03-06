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

#ifndef RV_DEPACK_INTERNAL_H
#define RV_DEPACK_INTERNAL_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "rm_error.h"
#include "rv_depack.h"
#include "stream_hdr_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RM_DEPACK_CALL		0x00000000		// call back
#define RM_DEPACK_FIFO		0x00000001		// send to fifo
#define RM_DEPACK_SWAP		0x00000002		// hold by pReadyFrame

#define RM_DEPACK_PACKET	0x00000000		// packet by packet
#define RM_DEPACK_FRAME		0x00000004		// frame by frame

#define RM_DEPACK_MASK1		0x00000003
#define RM_DEPACK_MASK2		0x00000004

#define RM_DEPACK_IS_CALLBACK(mode) ((mode&RM_DEPACK_MASK1)==RM_DEPACK_CALL?1:0)
#define RM_DEPACK_IS_FIFO(mode)		((mode&RM_DEPACK_MASK1)==RM_DEPACK_FIFO?1:0)
#define RM_DEPACK_IS_SWAP(mode)		((mode&RM_DEPACK_MASK1)==RM_DEPACK_SWAP?1:0)

#define RM_DEPACK_IS_PACKET_BY_PACKET(mode) ((mode&RM_DEPACK_MASK2)==RM_DEPACK_PACKET?1:0)
#define RM_DEPACK_IS_FRAME_BY_FRAME(mode)	((mode&RM_DEPACK_MASK2)==RM_DEPACK_FRAME?1:0)

/* Frame type enum */
typedef enum
{
    RVFrameTypePartial,
    RVFrameTypeWhole,
    RVFrameTypeLastPartial,
    RVFrameTypeMultiple
} RVFrameType;

/* Picture coded type enum */
typedef enum
{
	RV_INTRAPIC,		/* 0 (00) */
	RV_FORCED_INTRAPIC,	/* 1 (01) */
	RV_INTERPIC,		/* 2 (10) */
	RV_TRUEBPIC			/* 3 (11) */
} RVPicCodType;

#define RV_IS_KEY_FRAME(PicCodeType) (((PicCodeType)==RV_INTRAPIC) \
	|| ((PicCodeType)==RV_FORCED_INTRAPIC))

/* Struct which holds frame header info */
typedef struct rv_frame_hdr_struct
{
    RVFrameType eType;
    UINT32      ulPacketNum;
    UINT32      ulNumPackets;
    UINT32      ulFrameSize;
    UINT32      ulPartialFrameSize;
    UINT32      ulPartialFrameOffset;
    UINT32      ulTimestamp;
    UINT32      ulSeqNum;
    UINT32      ulHeaderSize;
    UINT32      ulHeaderOffset;
    HXBOOL      bBrokenUpByUs;
} rv_frame_hdr;

#define KEY_FRAME_TIMESTAMP_TABLE_COUNT		(1024*32)
#define RV_DEPACK_FIFO_LENGTH				(1024*32)

/*
 * Internal rv_depack struct
 */
typedef struct rv_depack_internal_struct
{
    void*                   pAvail;		/* only for callback mode, NOT used in media engine */
    rv_frame_avail_func_ptr fpAvail;	/* only for callback mode, NOT used in media engine */
    rm_error_func_ptr       fpError;
    void*                   pUserError;
    rm_malloc_func_ptr      fpMalloc;
    rm_free_func_ptr        fpFree;
    void*                   pUserMem;
    UINT32                  ulZeroTimeOffset;
    HX_BITFIELD             bHasRelativeTimeStamps;
    rm_rule_map             rule2Flag;
    rm_multistream_hdr      multiStreamHdr;
    rv_format_info*         pSubStreamHdr;
    HXBOOL*                 bIgnoreSubStream;
    HXBOOL                  bStreamSwitchable;
    UINT32                  ulActiveSubStream;
    rv_frame*               pCurFrame;	/* Points to the depacking rv_frame */
    HXBOOL                  bBrokenUpByUs;
    HXBOOL                  bCreatedFirstFrame;
    UINT32                  ulLastSeqNumIn;
    UINT32                  ulLastSeqNumOut;

	/* NCNB memory operation support -caijin@2008.06.03 */
	rm_malloc_func_ptr      fpMalloc_ncnb;
	rm_free_func_ptr        fpFree_ncnb;


	/* NOTE: following defined for media engine */
	/* Zhang Xuecheng,2008-3-4 18:29:05 */
	UINT32                  pRvDepackFifo[RV_DEPACK_FIFO_LENGTH];
	UINT32                  uRvFifoHead;				// point to current head frame
	UINT32                  uRvFifoTail;				// point to current tail frame
	UINT32                  uRvFifoCnt;				// total number of frames in fifo

	/* Key Frame Timestamp Table,zhang xuecheng,2008-2-25 13:27:21 */
	UINT32                  pKeyFrameTable[KEY_FRAME_TIMESTAMP_TABLE_COUNT];
	UINT32                  ulKeyFrameTableOffset;
	UINT32                  ulLastReqTime;
	UINT32                  ulLastReqTimeOffset;
	UINT32                  ulKeyFrameTableCount;

	//UINT32                  ulLastValidTimestamp;	/* timestamp of last VALID frame -caijin @2008.05.27 */
	rv_frame*	            pReadyFrame;	/* Points to the recently depacked rv_frame whose informations 
											* are all ready including the duration of this frame */
	BYTE*                   pFrameBufAddr;	/* if BufferAddr != NULL, the buffer will be used by pCurFrame. 
											* Otherwise, pCurFrame will allocate a new buffer. The later is 
											* the default mode. 
											*/
	UINT32                  ulFrameBufLen;	/* The length of the buffer pointed by BufferAddr */
} rv_depack_internal;

/*
 * Internal rv_depack functions
 */
void*     rv_depacki_malloc(rv_depack_internal* pInt, UINT32 ulSize);
void      rv_depacki_free(rv_depack_internal* pInt, void* pMem);
HX_RESULT rv_depacki_init(rv_depack_internal* pInt, rm_stream_header* hdr);
HX_RESULT rv_depacki_unpack_rule_map(rv_depack_internal* pInt,
                                     rm_rule_map*        pMap,
                                     BYTE**              ppBuf,
                                     UINT32*             pulLen);
HX_RESULT rv_depacki_unpack_multistream_hdr(rv_depack_internal* pInt,
                                            BYTE**              ppBuf,
                                            UINT32*             pulLen);
HX_RESULT rv_depacki_unpack_opaque_data(rv_depack_internal* pInt,
                                        BYTE*               pBuf,
                                        UINT32              ulLen);
void      rv_depacki_cleanup_format_info(rv_depack_internal* pInt,
                                         rv_format_info*     pInfo);
void      rv_depacki_cleanup_format_info_array(rv_depack_internal* pInt);
HX_RESULT rv_depacki_unpack_format_info(rv_depack_internal* pInt,
                                        rv_format_info*     pInfo,
                                        BYTE**              ppBuf,
                                        UINT32*             pulLen);
HX_RESULT rv_depacki_check_rule_book(rv_depack_internal* pInt,
                                     rm_stream_header*   hdr);
HX_RESULT rv_depacki_copy_format_info(rv_depack_internal* pInt,
                                      rv_format_info*     pSrc,
                                      rv_format_info*     pDst);
HX_RESULT rv_depacki_add_packet(rv_depack_internal* pInt,
                                rm_packet*          pPacket);
UINT32    rv_depacki_rule_to_flags(rv_depack_internal* pInt, UINT32 ulRule);
UINT32    rv_depacki_rule_to_substream(rv_depack_internal* pInt, UINT32 ulRule);
HX_RESULT rv_depacki_parse_frame_header(rv_depack_internal*  pInt,
                                        BYTE**               ppBuf,
                                        UINT32*              pulLen,
                                        rm_packet*           pPacket,
                                        rv_frame_hdr* pFrameHdr);
HX_RESULT rv_depacki_read_14_or_30(BYTE**  ppBuf,
                                   UINT32* pulLen,
                                   HXBOOL* pbHiBit,
                                   UINT32* pulValue);
HX_RESULT rv_depacki_handle_partial(rv_depack_internal*  pInt,
                                    BYTE**               ppBuf,
                                    UINT32*              pulLen,
                                    rm_packet*           pPacket,
                                    rv_frame_hdr* pFrameHdr);
HX_RESULT rv_depacki_handle_one_frame(rv_depack_internal*  pInt,
                                      BYTE**               ppBuf,
                                      UINT32*              pulLen,
                                      rm_packet*           pPacket,
                                      rv_frame_hdr* pFrameHdr);
HX_RESULT rv_depacki_send_current_frame(rv_depack_internal* pInt);
void      rv_depacki_cleanup_frame(rv_depack_internal* pInt,
                                   rv_frame**   ppFrame);
HX_RESULT rv_depacki_create_frame(rv_depack_internal*  pInt,
                                  rm_packet*           pPacket,
                                  rv_frame_hdr* pFrameHdr,
                                  rv_frame**    ppFrame);
void      rv_depacki_check_to_clear_keyframe_flag(BYTE*                pBuf,
                                                  UINT32               ulLen,
                                                  rm_packet*           pkt,
                                                  rv_frame_hdr* hdr);
HX_RESULT rv_depacki_seek(rv_depack_internal* pInt, UINT32 ulTime);


void* rv_depacki_create_ncnb(rv_depack_internal* pInt, UINT32 ulSize);
void rv_depacki_destroy_ncnb(rv_depack_internal* pInt, void* pMem);

/* following added for media engine -caijin @2008.05.28*/
HX_RESULT rv_depacki_add_packet_ex(rv_depack_internal* pInt,
									rm_packet*          pPacket,
									UINT32				ulDepackMode);
HX_RESULT rv_depacki_handle_partial_ex(rv_depack_internal*	pInt,
										BYTE**				ppBuf,
										UINT32*				pulLen,
										rm_packet*			pPacket,
										rv_frame_hdr*		pFrameHdr,
										UINT32				ulDepackMode
										);
HX_RESULT rv_depacki_handle_one_frame_ex(rv_depack_internal*	pInt,
										  BYTE**				ppBuf,
										  UINT32*				pulLen,
										  rm_packet*			pPacket,
										  rv_frame_hdr*			pFrameHdr,
										  UINT32				ulDepackMode);
HX_RESULT rv_depacki_send_current_frame_ex(rv_depack_internal * pInt, UINT32 ulDepackMode);

void rv_depacki_cleanup_frame_ex(rv_depack_internal* pInt,
								 rv_frame**          ppFrame);
HX_RESULT rv_depacki_create_frame_ex(rv_depack_internal* pInt,
									 rm_packet*          pPacket,
									 rv_frame_hdr*       pFrameHdr,
									 rv_frame**          ppFrame);

UINT8 rv_depacki_get_pictype(rv_depack_internal *pInt, rv_frame *pFrame);

HX_RESULT rv_depacki_get_current_frame(rv_depack_internal *pInt, rv_frame **ppFrame);
HX_RESULT rv_depacki_get_ready_frame(rv_depack_internal *pInt, rv_frame **ppFrame);

HX_RESULT rv_depacki_read_oneframe(rv_depack_internal * pInt, rm_packet * pPacket, rv_frame * pFrame);
HX_RESULT rv_depacki_read_frames(rv_depack_internal * pInt, rm_packet * pPacket, rv_frame * pFrame);
HX_RESULT rv_depacki_copy_rvframe(UINT8 * buf, rv_frame * pFrame);

void rv_depacki_cleanup_frame_buffer(rv_depack_internal *pInt);

HX_RESULT rv_depacki_set_frame_buffer(rv_depack_internal *pInt, BYTE *bufAddr, UINT32 bufLen);

#if 0
HX_RESULT rv_depacki_handle_partial_fifomode(rv_depack_internal*  pInt,
											 BYTE**               ppBuf,
											 UINT32*              pulLen,
											 rm_packet*           pPacket,
											 rv_frame_hdr* pFrameHdr);
HX_RESULT rv_depacki_handle_one_frame_fifomode(rv_depack_internal*  pInt,
											   BYTE**               ppBuf,
											   UINT32*              pulLen,
											   rm_packet*           pPacket,
											   rv_frame_hdr* pFrameHdr);
HX_RESULT rv_depacki_send_current_frame_fifomode(rv_depack_internal * pInt);

#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef RV_DEPACK_INTERNAL_H */
