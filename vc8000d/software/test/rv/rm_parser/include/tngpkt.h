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
#include "rm_memory.h"

#ifndef PMC_PREDEFINED_TYPES
#define PMC_PREDEFINED_TYPES

typedef char* pmc_string;

typedef struct _buffer {
	 UINT32 len;
	 INT8* data;
} buffer;
#endif/*PMC_PREDEFINED_TYPES*/


#ifndef _TNGPKT_H_
#define _TNGPKT_H_

struct TNGDataPacket
{
    UINT8 length_included_flag;
    UINT8 need_reliable_flag;
    UINT8 stream_id;
    UINT8 is_reliable;
    UINT16 seq_no;
    UINT16 _packlenwhendone;
    UINT8 back_to_back_packet;
    UINT8 slow_data;
    UINT8 asm_rule_number;
    UINT32 timestamp;
    UINT16 stream_id_expansion;
    UINT16 total_reliable;
    UINT16 asm_rule_number_expansion;
    buffer data;
};

const UINT32 TNGDataPacket_static_size();
UINT8* TNGDataPacket_pack(UINT8* buf,
                          UINT32 len,
                          struct TNGDataPacket* pkt);
UINT8* TNGDataPacket_unpack(UINT8* buf,
                            UINT32 len,
                            struct TNGDataPacket* pkt);

struct TNGMultiCastDataPacket
{
    UINT8 length_included_flag;
    UINT8 need_reliable_flag;
    UINT8 stream_id;
    UINT8 is_reliable;
    UINT16 seq_no;
    UINT16 length;
    UINT8 back_to_back_packet;
    UINT8 slow_data;
    UINT8 asm_rule_number;
    UINT8 group_seq_no;
    UINT32 timestamp;
    UINT16 stream_id_expansion;
    UINT16 total_reliable;
    UINT16 asm_rule_number_expansion;
    buffer data;
};

const UINT32 TNGMultiCastDataPacket_static_size();

UINT8* TNGMultiCastDataPacket_pack(UINT8* buf,
                                   UINT32 len,
                                   struct TNGMultiCastDataPacket* pkt);
UINT8* TNGMultiCastDataPacket_unpack(UINT8* buf,
                                     UINT32 len,
                                     struct TNGMultiCastDataPacket* pkt);


struct TNGASMActionPacket
{
    UINT8 length_included_flag;
    UINT8 stream_id;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT16 packet_type;
    UINT16 reliable_seq_no;
    UINT16 length;
    UINT16 stream_id_expansion;
    buffer data;
};

const UINT32 TNGASMActionPacket_static_size();
UINT8* TNGASMActionPacket_pack(UINT8* buf,
                               UINT32 len,
                               struct TNGASMActionPacket* pkt);

UINT8* TNGASMActionPacket_unpack(UINT8* buf,
                                 UINT32 len,
                                 struct TNGASMActionPacket* pkt);



struct TNGBandwidthReportPacket
{
    UINT8 length_included_flag;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT16 packet_type;
    UINT16 length;
    UINT16 interval;
    UINT32 bandwidth;
    UINT8 sequence;
};

const UINT32 TNGBandwidthReportPacket_static_size();
UINT8* TNGBandwidthReportPacket_pack(UINT8* buf,
                                     UINT32 len,
                                     struct TNGBandwidthReportPacket* pkt);
UINT8* TNGBandwidthReportPacket_unpack(UINT8* buf,
                                       UINT32 len,
                                       struct TNGBandwidthReportPacket* pkt);



struct TNGReportPacket
{
    UINT8 length_included_flag;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT16 packet_type;
    UINT16 length;
    buffer data;
};

const UINT32 TNGReportPacket_static_size();
UINT8* TNGReportPacket_pack(UINT8* buf,
                            UINT32 len,
                            struct TNGReportPacket* pkt);
UINT8* TNGReportPacket_unpack(UINT8* buf,
                              UINT32 len,
                              struct TNGReportPacket* pkt);



struct TNGACKPacket
{
    UINT8 length_included_flag;
    UINT8 lost_high;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT16 packet_type;
    UINT16 length;
    buffer data;
};

const UINT32 TNGACKPacket_static_size();
UINT8* TNGACKPacket_pack(UINT8* buf,
                         UINT32 len,
                         struct TNGACKPacket* pkt);
UINT8* TNGACKPacket_unpack(UINT8* buf,
                           UINT32 len,
                           struct TNGACKPacket* pkt);


struct TNGRTTRequestPacket
{
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT8 dummy3;
    UINT16 packet_type;
};

const UINT32 TNGRTTRequestPacket_static_size();
UINT8* TNGRTTRequestPacket_pack(UINT8* buf,
                                UINT32 len,
                                struct TNGRTTRequestPacket* pkt);
UINT8* TNGRTTRequestPacket_unpack(UINT8* buf,
                                  UINT32 len,
                                  struct TNGRTTRequestPacket* pkt);

struct TNGRTTResponsePacket
{
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT8 dummy3;
    UINT16 packet_type;
    UINT32 timestamp_sec;
    UINT32 timestamp_usec;
};

const UINT32 TNGRTTResponsePacket_static_size();
UINT8* TNGRTTResponsePacket_pack(UINT8* buf,
                                 UINT32 len,
                                 struct TNGRTTResponsePacket* pkt);
UINT8* TNGRTTResponsePacket_unpack(UINT8* buf,
                                   UINT32 len,
                                   struct TNGRTTResponsePacket* pkt);


struct TNGCongestionPacket
{
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT8 dummy3;
    UINT16 packet_type;
    INT32 xmit_multiplier;
    INT32 recv_multiplier;
};

const UINT32 TNGCongestionPacket_static_size();
UINT8* TNGCongestionPacket_pack(UINT8* buf,
                                UINT32 len,
                                struct TNGCongestionPacket* pkt);
UINT8* TNGCongestionPacket_unpack(UINT8* buf,
                                  UINT32 len,
                                  struct TNGCongestionPacket* pkt);


struct TNGStreamEndPacket
{
    UINT8 need_reliable_flag;
    UINT8 stream_id;
    UINT8 packet_sent;
    UINT8 ext_flag;
    UINT16 packet_type;
    UINT16 seq_no;
    UINT32 timestamp;
    UINT16 stream_id_expansion;
    UINT16 total_reliable;
    UINT8 reason_dummy[3];
    UINT32 reason_code;
    pmc_string reason_text;
};

const UINT32 TNGStreamEndPacket_static_size();
UINT8* TNGStreamEndPacket_pack(UINT8* buf,
                               UINT32 len,
                               struct TNGStreamEndPacket* pkt);

/* TNGStreamEndPacket_unpack will malloc room for the reason_text.
 * You must free this memory or it will be leaked */
UINT8* TNGStreamEndPacket_unpack(UINT8* buf,
                                 UINT32 len,
                                 struct TNGStreamEndPacket* pkt);

UINT8* TNGStreamEndPacket_unpack_fromPool(UINT8* buf,
                                         UINT32 len,
                                         struct TNGStreamEndPacket* pkt,
                                         rm_malloc_func_ptr fpMalloc,
                                         void* pMemoryPool);




struct TNGLatencyReportPacket
{
    UINT8 length_included_flag;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT16 packet_type;
    UINT16 length;
    UINT32 server_out_time;
};

const UINT32 TNGLatencyReportPacket_static_size();
UINT8* TNGLatencyReportPacket_pack(UINT8* buf,
                                   UINT32 len,
                                   struct TNGLatencyReportPacket* pkt);
UINT8* TNGLatencyReportPacket_unpack(UINT8* buf,
                                     UINT32 len,
                                     struct TNGLatencyReportPacket* pkt);




    /*
     * RDTFeatureLevel 3 packets
     */

struct RDTTransportInfoRequestPacket
{
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 request_rtt_info;
    UINT8 request_buffer_info;
    UINT16 packet_type;
    UINT32 request_time_ms;
};

const UINT32 RDTTransportInfoRequestPacket_static_size();
UINT8* RDTTransportInfoRequestPacket_pack(UINT8* buf,
                                          UINT32 len,
                                          struct RDTTransportInfoRequestPacket* pkt);
UINT8* RDTTransportInfoRequestPacket_unpack(UINT8* buf,
                                            UINT32 len,
                                            struct RDTTransportInfoRequestPacket* pkt);




struct RDTBufferInfo
{
    UINT16 stream_id;
    UINT32 lowest_timestamp;
    UINT32 highest_timestamp;
    UINT32 bytes_buffered;
};

const UINT32 RDTBufferInfo_static_size();
UINT8* RDTBufferInfo_pack(UINT8* buf,
                          UINT32 len, struct RDTBufferInfo* pkt);
UINT8* RDTBufferInfo_unpack(UINT8* buf,
                            UINT32 len, struct RDTBufferInfo* pkt);




struct RDTTransportInfoResponsePacket
{
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 has_rtt_info;
    UINT8 is_delayed;
    UINT8 has_buffer_info;
    UINT16 packet_type;
    UINT32 request_time_ms;
    UINT32 response_time_ms;
    UINT16 buffer_info_count;
    struct RDTBufferInfo *buffer_info;
};

const UINT32 RDTTransportInfoResponsePacket_static_size();
UINT8* RDTTransportInfoResponsePacket_pack(UINT8* buf,
                                           UINT32 len,
                                           struct RDTTransportInfoResponsePacket* pkt);
UINT8* RDTTransportInfoResponsePacket_unpack(UINT8* buf,
                                             UINT32 len,
                                             struct RDTTransportInfoResponsePacket* pkt);

UINT8* RDTTransportInfoResponsePacket_unpack_fromPool(UINT8* buf,
                                                      UINT32 len,
                                                      struct RDTTransportInfoResponsePacket* pkt,
                                                      rm_malloc_func_ptr fpMalloc,
                                                      void* pMemoryPool);






struct TNGBWProbingPacket
{
    UINT8 length_included_flag;
    UINT8 dummy0;
    UINT8 dummy1;
    UINT8 dummy2;
    UINT16 packet_type;
    UINT16 length;
    UINT8 seq_no;
    UINT32 timestamp;
    buffer data;
};

const UINT32 TNGBWProbingPacket_static_size();
UINT8* TNGBWProbingPacket_pack(UINT8* buf,
                               UINT32 len,
                               struct TNGBWProbingPacket* pkt);
UINT8* TNGBWProbingPacket_unpack(UINT8* buf,
                                 UINT32 len,
                                 struct TNGBWProbingPacket* pkt);
#endif /* _TNGPKT_H_ */
