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

#ifndef RM_PACKET_H
#define RM_PACKET_H

#include "helix_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/*
 * Packet struct
 *
 * Users are strongly encouraged to use the
 * accessor functions below to retrieve information
 * from the packet, since the definition of this
 * struct may change in the future.
 */
typedef struct rm_packet_struct
{
    UINT32 ulTime;
    UINT16 usStream;
    UINT16 usASMFlags;
    BYTE   ucASMRule;
    BYTE   ucLost;
    UINT16 usDataLen;
    BYTE*  pData;

	/* following added to support 
	 * depacketization mode frame by frame,
	 * instead of packet-by-packet mode
	 * -caijin 2008.05.27
	 */
	HXBOOL bIsValid;	
	UINT16 usCurDataLen;
	BYTE*  pCurData;
	//UINT32 ulNextTimestamp;	/* timestamp of next packet */
} rm_packet;

/*
 * Packet Accessor functions
 */
UINT32 rm_packet_get_timestamp(rm_packet* packet);
UINT32 rm_packet_get_stream_number(rm_packet* packet);
UINT32 rm_packet_get_asm_flags(rm_packet* packet);
UINT32 rm_packet_get_asm_rule_number(rm_packet* packet);
UINT32 rm_packet_get_data_length(rm_packet* packet);
BYTE*  rm_packet_get_data(rm_packet* packet);
HXBOOL rm_packet_is_lost(rm_packet* packet);

/* following added to support 
* depacketization mode frame by frame,
* instead of packet-by-packet mode
* -caijin 2008.05.27
*/
HXBOOL rm_packet_is_valid(rm_packet* packet);
UINT32 rm_packet_get_cur_data_length(rm_packet* packet);
BYTE*  rm_packet_get_cur_data(rm_packet* packet);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RM_PACKET_H */
