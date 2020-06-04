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

#ifndef RM_PARSER_INTERNAL_H
#define RM_PARSER_INTERNAL_H

#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "rm_error.h"
#include "rm_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Defines */
#define RM_HEADER_OBJECT           0x2E524D46   /* '.RMF' */
#define RM_PROPERTIES_OBJECT       0x50524F50   /* 'PROP' */
#define RM_MEDIA_PROPERTIES_OBJECT 0x4D445052   /* 'MDPR' */
#define RM_CONTENT_OBJECT          0x434F4E54   /* 'CONT' */
#define RM_DATA_OBJECT             0x44415441   /* 'DATA' */
#define RM_INDEX_OBJECT            0x494E4458   /* 'INDX' */
#define RM_MULTIHEADER_OBJECT      0x4D4C5449   /* 'MLTI' */

#define RM_NO_STREAM_SET           0xFFFFFFFF
#define RM_INDEX_MIN_ENTRIES               32
#define RM_INDEX_MAX_ENTRIES             2048    
#define RM_INDEX_MIN_TIME_GRANULARITY      50  /* in milliseconds */
#define RM_INDEX_MAX_TIME_GRANULARITY   15000  /* in milliseconds */

/*
 * We explicitly define the size for structs
 * to be read. That's because we can't use sizeof()
 * to determine how much to read since struct 
 * alignment will make that value different.
 */
#define RM_PARSER_GENERIC_HDR_SIZE       10
#define RM_PARSER_INDEX_HEADER_SIZE      20
#define RM_PARSER_INDEX_RECORD_SIZE      14
#define RM_PARSER_PACKET_HEADER_SIZE     12
#define RM_PARSER_DATA_CHUNK_HEADER_SIZE 18

/* generic header -- most headers start like this */
struct rm_generic_hdr
{
    UINT32 id;          /* unique identifier for this header */
    UINT32 size;        /* size of header in file */
    UINT16 version;     /* struct version */
};

/* rm file header -- all real media files start with this header */
struct rm_file_hdr
{
    UINT32 id;           /* unique identifier for this header */
    UINT32 size;         /* size of header in file */
    UINT16 version;      /* struct version */
    UINT32 file_version; /* revision number for realmedia file */
    UINT32 num_headers;  /* num headers in file (including this one) */
};

/* rm properties header -- summarizes data for entire clip */
struct rm_properties_hdr
{
    UINT32 id;           /* unique identifier for this header */
    UINT32 size;         /* size of header in file */
    UINT16 version;      /* struct version */
    UINT32 max_bit_rate; /* maximum bit rate of clip */
    UINT32 avg_bit_rate; /* average bit rate of clip */
    UINT32 max_pkt_size; /* max length of packet in clip (bytes) */
    UINT32 avg_pkt_size; /* averge length of packet in clip (bytes) */
    UINT32 num_pkts;     /* total packets in clip (all streams) */
    UINT32 duration;     /* duration of clip (milliseconds) */
    UINT32 preroll;      /* preroll time (milliseconds) */
    UINT32 index_offset; /* offset to beginning of index data */
    UINT32 data_offset;  /* offset to beginning of packet data */
    UINT16 num_streams;  /* total number of streams in clip */
    UINT16 flags;        /* various and sundry */
};

/* rm content header -- title, author, copyright */
struct rm_content_hdr
{
    UINT32 id;               /* unique identifier for this header */
    UINT32 size;             /* size of header in file */
    UINT16 version;          /* struct version */
    UINT16 title_sz;         /* length of title field */
    char*  title;            /* title of clip */
    UINT16 author_sz;        /* length of author field */
    char*  author;           /* author of clip */
    UINT16 copyright_sz;     /* length of copyright field */
    char*  copyright;        /* copyright of clip */
    UINT16 comment_sz;       /* length of comment field */
    char*  comment;          /* comment */
};

/* rm media properties header -- stream info, one for each stream */
struct rm_media_props_hdr
{
    UINT32 id;                  /* unique identifier for this header */
    UINT32 size;                /* size of header in file */
    UINT16 version;             /* struct version */
    UINT16 stream_num;          /* stream number */
    UINT32 max_bit_rate;        /* maximum bit rate of stream */
    UINT32 avg_bit_rate;        /* average bit rate of stream */
    UINT32 max_pkt_size;        /* max length of packet in stream (bytes) */
    UINT32 avg_pkt_size;        /* averge length of packet in stream (bytes) */
    UINT32 start_time;          /* start time of stream -- clip relative */
    UINT32 preroll;             /* preroll time (milliseconds) */
    UINT32 duration;            /* duration of stream (milliseconds) */
    BYTE   stream_name_sz;      /* length of stream_name field */
    char*  stream_name;         /* string name of stream */
    BYTE   mime_type_sz;        /* length of mime_type field */
    char*  mime_type;           /* MIME type of stream */
    UINT32 type_spec_sz;        /* length of type_spec field */
    BYTE*  type_spec;           /* type specific data buffer */
};

/* rm name value header -- implements name/value pairs */
struct rm_name_value_map
{
    UINT32 size;      /* size of header in file */
    UINT16 version;   /* struct version */
    BYTE   name_sz;   /* length of name field */
    char*  name;      /* name of property */
    UINT32 type;      /* type of property */
    UINT16 value_sz;  /* length of value field */
    BYTE*  value;     /* value of property */
};

/* rm logical stream header -- used for surestream */
struct rm_logical_stream_hdr
{
    UINT32  size;                    /* size of header in file */
    UINT16  version;                 /* struct version */ 
    UINT16  num_physical_streams;    /* number of actual streams */
    UINT16* physical_stream_num;     /* array of stream numbers */
    UINT32* data_offsets;            /* array of offsets to stream packets */
    UINT16  num_rules;               /* number of ASM rules */
    UINT16* rule_stream_map;         /* maps rules to streams */
    UINT16  num_props;               /* number of name value properties */
    struct rm_name_value_map* props; /* array of name value properties */
};

/* rm meta information -- clip meta data */
struct rm_meta_data_hdr
{
    UINT32 size;                     /* size of header in file */
    UINT16 version;                  /* struct version */  
    UINT16 num_props;                /* number of name value properties */
    struct rm_name_value_map* props; /* array of name value properties */
};

/* rm data header -- this occurs before the start of packet data */
struct rm_data_hdr
{
    UINT32 id;                  /* unique identifier for this header */
    UINT32 size;                /* size of header in file */
    UINT16 version;             /* struct version */
    UINT32 num_pkts;            /* total number of packets in segment */
    UINT32 next_data_hdr;       /* offset of next segment */
};

/* rm index header -- this occurs before the start of index data */
struct rm_index_hdr
{
    UINT32 id;                  /* unique identifier for this header */
    UINT32 size;                /* size of header in file */
    UINT16 version;             /* struct version */
    UINT32 num_recs;            /* number of index records */
    UINT16 stream_num;          /* stream number being indexed */
    UINT32 next_index_hdr;      /* offset of next header */
};

/* rm index record -- describes an index entry */
struct rm_index_rec
{
    UINT16 version;             /* record version */
    UINT32 timestamp;           /* packet timestamp */
    UINT32 offset;              /* offset of packet in file */
    UINT32 num_pkts;            /* number of packets before this one in clip */
};

/* rm packet header */
struct rm_pkt_hdr_base
{
    UINT16 version;             /* struct version */
    UINT16 length;              /* length of header */
};

struct rm_pkt_hdr
{
    UINT16 version;             /* struct version */
    UINT16 length;              /* length of header */
    UINT16 stream_num;          /* stream number */
    UINT32 timestamp;           /* packet timestamp */
    UINT16 flags;               /* keyframe, etc. */
};

struct rm_pkt_hdr1
{
    UINT16 version;             /* struct version */
    UINT16 length;              /* length of header */
    UINT16 stream_num;          /* stream number */
    UINT32 timestamp;           /* packet timestamp */
    UINT16 asm_rule;            /* for surestream */
    BYTE   asm_flags;
};

/*
 * Internal struct which tracks stream info
 */
struct rm_seek_table_entry
{
    UINT32 ulTime;
    UINT32 ulOffset;
};

struct rm_seek_table
{
    struct rm_seek_table_entry* pEntry;
    UINT32                      ulMaxEntries;
    UINT32                      ulNumEntries;
    UINT32                      ulLastTime;
    UINT32                      ulRangeTime;
    UINT32                      ulTimeGranularity;
};

struct rm_keyframe_packet
{
    UINT32 ulTimestamp;
    UINT32 ulFileOffset;
    HXBOOL bValid;
};

struct rm_stream_info
{
   UINT32                    ulLastTimeStamp;
   UINT32                    ulLastRule;
   struct rm_keyframe_packet keyFramePacket;
   struct rm_seek_table      seekTable;
   HX_BITFIELD               bIsRealAudio              : 1;
   HX_BITFIELD               bIsRealVideo              : 1;
   HX_BITFIELD               bIsRealEvent              : 1;
   HX_BITFIELD               bStreamDone               : 1;
   HX_BITFIELD               bNeedKeyframe             : 1;
   HX_BITFIELD               bSeenFirstPacketTimeStamp : 1;
};

/*
 * Parser struct
 */
typedef struct rm_parser_internal_struct
{
    rm_error_func_ptr             fpError;
    void*                         pUserError;
    rm_malloc_func_ptr            fpMalloc;
    rm_free_func_ptr              fpFree;
    void*                         pUserMem;
	rm_read_func_ptr              fpRead;
#ifdef ME
	rm_read2_func_ptr             fpRead2;
#endif
	rm_write_func_ptr             fpWrite;
	rm_seek_func_ptr              fpSeek;
	rm_tell_func_ptr              fpTell;
    void*                         pUserRead;
    struct rm_file_hdr            fileHdr;
    struct rm_properties_hdr      propHdr;
    struct rm_content_hdr         contHdr;
    UINT32                        ulNumStreams;
    UINT32                        ulNumMediaPropsHdrs;
    UINT32                        ulNumMediaPropsHdrsAlloc;
    struct rm_media_props_hdr*    pMediaPropsHdr;
    UINT32                        ulNumLogicalStreamHdrs;
    struct rm_logical_stream_hdr* pLogicalStreamHdr;
    struct rm_logical_stream_hdr* pLogicalFileInfo;
    BYTE*                         pReadBuffer;            /* Read buffer */
    UINT32                        ulReadBufferSize;       /* Allocated size of the read buffer */
    UINT32                        ulNumBytesRead;         /* Current number of bytes read into the buffer */
    UINT32                        ulCurFileOffset;        /* Current read offset into the file */
    UINT32                        ulStreamNumMapSize;
    UINT32*                       pulStreamNumMap;
    UINT32                        ulMaxDuration;
    struct rm_stream_info*        pStreamInfo;
    struct rm_data_hdr            dataHdr;
    HXBOOL                        bIsRealDataType;
    UINT32                        ulMinFirstPacketTime;
    rm_stream_header*             pStreamHdr;
    UINT32                        ulKeyframesNeeded;
	UINT32 ulRvTimeStampAfterSeek;	//Zhang Xuecheng,2008-3-9 0:54:19
	UINT32 ulRaTimeStampAfterSeek;	//Zhang Xuecheng,2008-3-9 0:54:19
} rm_parser_internal;

/*
 * Internal parser functions
 */
HX_RESULT rm_parseri_unpack_generic_hdr(rm_parser_internal* pInt, struct rm_generic_hdr* h);
HX_RESULT rm_parseri_unpack_file_hdr(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_properties_hdr(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_content_hdr(rm_parser_internal* pInt);
void      rm_parseri_cleanup_content_hdr(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_media_props_hdr(rm_parser_internal* pInt,
                                            struct rm_media_props_hdr* h);
void      rm_parseri_cleanup_media_props_hdr(rm_parser_internal* pInt,
                                             struct rm_media_props_hdr* h);
void      rm_parseri_cleanup_all_media_props_hdrs(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_name_value_map(rm_parser_internal* pInt,
                                           struct rm_name_value_map* pMap,
                                           BYTE** ppBuf, UINT32* pulLen);
void      rm_parseri_cleanup_name_value_map(rm_parser_internal* pInt,
                                            struct rm_name_value_map* h);
void      rm_parseri_cleanup_all_name_value_maps(rm_parser_internal* pInt,
                                                 struct rm_logical_stream_hdr* h);
HX_RESULT rm_parseri_unpack_logical_stream_hdr(rm_parser_internal* pInt,
                                               struct rm_media_props_hdr* mh,
                                               struct rm_logical_stream_hdr* h);
UINT32    rm_parseri_get_num_logical_streams(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_all_logical_stream_hdrs(rm_parser_internal* pInt);
void      rm_parseri_cleanup_logical_stream_hdr(rm_parser_internal* pInt,
                                                struct rm_logical_stream_hdr* h);
void      rm_parseri_cleanup_all_logical_stream_hdrs(rm_parser_internal* pInt);
HXBOOL    rm_parseri_is_logical_fileinfo_present(rm_parser_internal* pInt, UINT32* pulIndx);
void      rm_parseri_cleanup_logical_fileinfo_hdr(rm_parser_internal* pInt);
HX_RESULT rm_parseri_allocate_media_props_hdrs(rm_parser_internal* pInt);
void      rm_parseri_cleanup_read_buffer(rm_parser_internal* pInt);
UINT32    rm_parseri_get_media_props_hdr_stream_num(rm_parser_internal* pInt);
HXBOOL    rm_parseri_is_logical_stream_mime_type(const char* pszMimeType);
HXBOOL    rm_parseri_is_logical_fileinfo_mime_type(const char* pszMimeType);
HX_RESULT rm_parseri_read_next_header(rm_parser_internal* pInt, UINT32* pulID);
HX_RESULT rm_parseri_setup_interleaved_streams(rm_parser_internal* pInt);
HX_RESULT rm_parseri_read_all_headers(rm_parser_internal* pInt);
UINT32    rm_parseri_get_num_file_properties(rm_parser_internal* pInt);
HX_RESULT rm_parseri_get_file_properties(rm_parser_internal* pInt,
                                         rm_property* pProp, UINT32 ulNumProp);
HXBOOL    rm_parseri_has_real_data_type(rm_parser_internal* pInt);
HX_RESULT rm_parseri_set_rm_property(rm_parser_internal* pInt, rm_property* pProp,
                                     const char* pszName, UINT32 ulType,
                                     void* pValue, UINT32 ulValueLen);
void      rm_parseri_cleanup_rm_property(rm_parser_internal* pInt, rm_property* pProp);
void      rm_parseri_cleanup_stream_num_map(rm_parser_internal* pInt);
void      rm_parseri_cleanup_stream_info(rm_parser_internal* pInt, struct rm_stream_info* pInfo);
void      rm_parseri_cleanup_stream_info_array(rm_parser_internal* pInt);
HX_RESULT rm_parseri_create_stream_structures(rm_parser_internal* pInt);
HX_RESULT rm_parseri_examine_initial_packets(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_data_hdr(rm_parser_internal* pInt);
HX_RESULT rm_parseri_unpack_pkt_hdr(rm_parser_internal* pInt,
                                    struct rm_pkt_hdr*  pPktHdr);
HX_RESULT rm_parseri_unpack_index_hdr(rm_parser_internal* pInt,
                                      struct rm_index_hdr* hdr);
HX_RESULT rm_parseri_unpack_index_rec(rm_parser_internal* pInt,
                                      struct rm_index_rec* rec);
HX_RESULT rm_parseri_read_next_index_rec(rm_parser_internal* pInt,
                                         struct rm_index_rec* rec);
UINT32    rm_parseri_translate_stream_number(rm_parser_internal* pInt, UINT32 ulNum);
HX_RESULT rm_parseri_create_all_stream_headers(rm_parser_internal* pInt);
HX_RESULT rm_parseri_create_stream_header(rm_parser_internal* pInt, UINT32 i,
                                          rm_stream_header* hdr);
void      rm_parseri_cleanup_all_stream_headers(rm_parser_internal* pInt);
void      rm_parseri_cleanup_stream_header(rm_parser_internal* pInt, rm_stream_header* hdr);
UINT32    rm_parseri_count_set_stream_header_props(rm_parser_internal* pInt, UINT32 i,
                                                   rm_stream_header* hdr,
                                                   rm_property* pProp, UINT32 ulNumProps);
char*     rm_parseri_create_asm_rulebook(rm_parser_internal* pInt, HXBOOL bIsVideo,
                                         HXBOOL bIsEvent, HXBOOL bHasOpaqueData,
                                         UINT32 ulMaxBitRate, UINT32 ulAvgBitRate,
                                         HXBOOL* pbHasPreDataProps);
HX_RESULT rm_parseri_read_next_packet(rm_parser_internal* pInt, rm_packet** ppPacket);
UINT32    rm_parseri_file_read(rm_parser_internal* pInt, UINT32 ulBytesToRead, UINT32 ulReadBufferOffset);
UINT32    rm_parseri_file_read_buffer(rm_parser_internal* pInt, BYTE* pBuf, UINT32 ulBytesToRead);
void      rm_parseri_file_seek(rm_parser_internal* pInt, UINT32 ulOffset, UINT32 ulOrigin);
HX_RESULT rm_parseri_copy_stream_header(rm_parser_internal* pInt, UINT32 i, rm_stream_header* pHdr);
HX_RESULT rm_parseri_update_seek_table(rm_parser_internal* pInt, UINT32 ulStreamNum,
                                       UINT32 ulTime, UINT32 ulOffset, UINT32 ulFlags);
HX_RESULT rm_parseri_update_time_range(rm_parser_internal* pInt, UINT32 ulStreamNum,
                                       UINT32 ulTime);
HX_RESULT rm_parseri_search_all_seek_tables(rm_parser_internal* pInt, UINT32 ulSeekTime,
                                            UINT32* pulFoundTime, UINT32* pulFoundOffset);
HX_RESULT rm_parseri_search_seek_table(struct rm_seek_table* pTable, UINT32 ulSeekTime,
                                       UINT32* pulFoundTime, UINT32* pulFoundOffset);
HX_RESULT rm_parseri_search_index_chunk(rm_parser_internal* pInt, UINT32 ulSeekTime);
HX_RESULT rm_parseri_seek(rm_parser_internal* pInt, UINT32 ulSeekTime);

#ifdef ME
HX_RESULT rm_parseri_read_next_packet2(rm_parser_internal* pInt, rm_packet** ppPacket);
UINT32    rm_parseri_file_read_buffer2(rm_parser_internal* pInt, BYTE** ppBuf, UINT32 ulBytesToRead);
#endif

// Wangxiaocheng 2007.06.21
HXBOOL    rm_parseri_is_all_keyframes_found(rm_parser_internal* pInt);
HXBOOL    rm_parseri_is_a_keyframe_found(rm_parser_internal* pInt);
HX_RESULT rm_parseri_find_first_packet_after_seek_time(rm_parser_internal* pInt,
                                                       UINT32 ulSeekTime,
                                                       UINT32 ulInitialOffset);
HX_RESULT rm_parseri_find_first_keyframe(rm_parser_internal* pInt);
void*     rm_parseri_malloc(rm_parser_internal* pInt, UINT32 ulSize);
void      rm_parseri_free(rm_parser_internal* pInt, void* pMem);
void      rm_parseri_error(rm_parser_internal* pInt, HX_RESULT err, const char* pszMsg);

HX_RESULT rm_parseri_set_fileoffset(rm_parser_internal * pInt, UINT32 ulCurFileOffset);
HX_RESULT rm_parseri_get_fileoffset(rm_parser_internal * pInt, UINT32 *ulCurFileOffset);
HX_RESULT rm_parseri_check_next_packet_timestamp(rm_parser_internal * pInt, UINT32 ulStreamNum, UINT32 * ulTimestamp);

HX_RESULT rm_parseri_get_timestamp_after_seek_time(rm_parser_internal * pInt, UINT16 usStreamNum, UINT32 *ulTimeStamp);
#ifdef __cplusplus
}
#endif

#endif /* #ifndef RM_PARSER_INTERNAL_H */
