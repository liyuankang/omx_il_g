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

#ifndef RDTPACKET_H
#define RDTPACKET_H

#include <string.h>
#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"

#ifdef __cplusplus
extern "C" {
#endif
    

/*
 * 
 * Packet util functions and types
 *
 */
    
#define RDT_ASM_ACTION_PKT       0xFF00
#define RDT_BW_REPORT_PKT        0xFF01
#define RDT_ACK_PKT              0xFF02
#define RDT_RTT_REQUEST_PKT      0xFF03
#define RDT_RTT_RESPONSE_PKT     0xFF04
#define RDT_CONGESTION_PKT       0xFF05
#define RDT_STREAM_END_PKT       0xFF06
#define RDT_REPORT_PKT           0xFF07
#define RDT_LATENCY_REPORT_PKT   0xFF08
#define RDT_TRANS_INFO_RQST_PKT  0xFF09
#define RDT_TRANS_INFO_RESP_PKT  0xFF0A
#define RDT_BANDWIDTH_PROBE_PKT  0xFF0B

#define RDT_DATA_PACKET          0xFFFE
#define RDT_UNKNOWN_TYPE         0xFFFF

    

/*
 *
 * The tngpkt.h file included below is auto-generated from the master
 * header file which contains the bit by bit definitions of the packet
 * structures(tngpkt.pm). The structures defined in tngpkt.h are
 * easier to use compared to the actual packet definitions in
 * tngpkt.pm. tngpkt.h also includes pack and unpack functions that
 * can convert these structures to and from the binary wire format of
 * the different packet types.
 *
 */
#include "tngpkt.h"


/* All seq numbers will be marked as lost [beg, end]. */
/* Caller is responsible for free'ing the bit-field that is allocated
 * ( "pkt->data.data").
 */
void createNAKPacket( struct TNGACKPacket* pkt,
                      UINT16 unStreamNum,
                      UINT16 unBegSeqNum,
                      UINT16 unEndSeqNum
                      );

void createNAKPacketFromPool( struct TNGACKPacket* pkt,
                              UINT16 unStreamNum,
                              UINT16 unBegSeqNum,
                              UINT16 unEndSeqNum,
                              rm_malloc_func_ptr fpMalloc,
                              void*  pMemoryPool
                              );



/* All seq numbers will be marked as received [beg, end].  The caller
 * must free the 'pkt->data.data' malloc'ed memory.
 */
void createACKPacket( struct TNGACKPacket* pkt,
                      UINT16 unStreamNum,
                      UINT16 unBegSeqNum,
                      UINT16 unEndSeqNum
                      );

void createACKPacketFromPool( struct TNGACKPacket* pkt,
                              UINT16 unStreamNum,
                              UINT16 unBegSeqNum,
                              UINT16 unEndSeqNum,
                              rm_malloc_func_ptr fpMalloc,
                              void*  pMemoryPool
                              );

/*
 * Returns the packet type of the packet in the buffer that was just
 * read off of the wire
 */
UINT16 GetRDTPacketType(UINT8* pBuf, UINT32 nLen);



/* 
 * Marshalling inline funcs.
 */

UINT8 getbyte(UINT8* data);
UINT16 getshort(UINT8* data);
INT32 getlong(UINT8* data);                           
void putbyte(UINT8* data, INT8 v);
void putshort(UINT8* data, UINT16 v);
void putlong(UINT8* data, UINT32 v);
UINT8* addbyte(UINT8* cp, UINT8 data);
UINT8* addshort(UINT8* cp, UINT16 data);
UINT8* addlong(UINT8* cp, UINT32 data);
UINT8* addstring(UINT8* cp, const UINT8* string, int len);

#ifdef __cplusplus
}
#endif 

#endif /* RDTPACKET_H */
