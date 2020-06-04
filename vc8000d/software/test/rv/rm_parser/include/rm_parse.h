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

#ifndef RM_PARSE_H
#define RM_PARSE_H

#include <stdio.h>
#include "helix_types.h"
#include "helix_result.h"
#include "rm_memory.h"
#include "rm_error.h"
#include "rm_property.h"
#include "rm_stream.h"
#include "rm_packet.h"

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/*
 * Seek origin definitions - these are
 * exactly the same as the respective
 * definitions in <stdio.h>
 */
#define HX_SEEK_ORIGIN_SET 0
#define HX_SEEK_ORIGIN_CUR 1
#define HX_SEEK_ORIGIN_END 2

/* Function pointer definitions */
typedef UINT32 (*rm_read_func_ptr) (void*   pUserRead,
                                    BYTE*   pBuf, /* Must be at least ulBytesToRead long */
                                    UINT32  ulBytesToRead);
#ifdef ME
typedef UINT32 (*rm_read2_func_ptr) (void*   pUserRead,
									 BYTE**  ppBuf,
									 UINT32  ulBytesToRead);
#endif
typedef void (*rm_seek_func_ptr) (void*  pUserRead,
                                  UINT32 ulOffset,
                                  UINT32 ulOrigin);

typedef UINT32 (*rm_write_func_ptr)(void *pUserRead,
									BYTE *pBuf,
									UINT32 ulBytesToRead);
typedef INT32 (*rm_tell_func_ptr)(void *pUserRead);

/*
 * rm_parser definition. This is opaque to the user.
 */
typedef void rm_parser;

/*
 * rm_parser_is_rm_file()
 *
 * Function which examines pBuf[0] up through pBuf[ulSize-1] and
 * tries to determine if the buffer contains the beginning of an .rm file
 */
HXBOOL rm_parser_is_rm_file(BYTE* pBuf, UINT32 ulSize);

/*
 * rm_parser_create
 *
 * This creates an rm_parser object. The user may provide an
 * optional error reporting function as well as a void* 
 * which will be passed into fpError. If fpError is NULL, then
 * the parser will not print out any error messages by default
 * and the user will have to interpret the error by the
 * return value.
 */
rm_parser* rm_parser_create(void*             pError,
                            rm_error_func_ptr fpError);

/*
 * rm_parser_create2
 *
 * This creates an rm_parser object, using custom memory
 * allocation and free functions that the user provides.
 * If fpMalloc or fpFree are NULL, then the default
 * allocation and free functions will be used.
 */
rm_parser* rm_parser_create2(void*              pError,
                             rm_error_func_ptr  fpError,
                             void*              pMem,
                             rm_malloc_func_ptr fpMalloc,
                             rm_free_func_ptr   fpFree);

/*
 * rm_parser_init_stdio
 *
 * Users would call this is they wanted to use stdio. This file
 * would need to already be fopen()'d and would need to be fclose()'d
 * after the library was finished. The parsing library would only
 * call fread(), fseek(), and perhaps ftell() on it.
 */
HX_RESULT rm_parser_init_stdio(rm_parser* pParser,
							   FILE*      fp);

/*
 * rm_parser_init_io
 *
 * If the user didn't want to use stdio, they would call
 * this function to set up their own I/O.
 */
HX_RESULT rm_parser_init_io(rm_parser*        pParser,
                            void*             pUserRead,
                            rm_read_func_ptr  fpRead,
                            rm_seek_func_ptr  fpSeek);

#ifdef ME
HX_RESULT rm_parser_init_io2(rm_parser* pParser,
							 void*      fp,
							 rm_read_func_ptr fpread,
							 rm_seek_func_ptr fpseek,
							 rm_read2_func_ptr fpread2);
#endif
/*
* rm_parser_init_io_ex
*
* Users would call this is they wanted to use stdio. This file
* would need to already be fopen()'d and would need to be fclose()'d
* after the library was finished.
*/
HX_RESULT rm_parser_init_io_ex(rm_parser* pParser,
							   void*      fp,
							   rm_read_func_ptr fpread,
							   rm_write_func_ptr fpwrite,
							   rm_seek_func_ptr fpseek,
							   rm_tell_func_ptr fptell);

/*
 * rm_parser_read_headers
 *
 * This function reads all the headers at the beginning
 * of an .rm file. After this function is called,
 * then the rm_get_xxx accessor functions can be
 * called to get information about the file as a whole.
 */
HX_RESULT rm_parser_read_headers(rm_parser* pParser);

/*
 * File Info Accessor functions
 *
 * The most commonly-used properties of an .rm file can
 * be accessed via the rm_parser_get_xxx accessor functions
 * below. These functions return information about the .rm file as
 * a whole, as opposed to stream header properties, which give
 * information about a particular stream (i.e. - audio stream
 * or video stream).
 *
 * For the title meta-data, the user can determine if this information
 * is present by calling rm_parser_get_title_length(). This length
 * is the length in bytes of the string INCLUDING the null
 * terminator. If the title meta-data IS present, then the user
 * can get read-only access to the title by calling rm_parser_get_title().
 * The user must not alter the memory that is returned; otherwise undefined
 * behavior will result.
 *
 * The same applies for author, copyright, and comment meta-data.
 */
UINT32      rm_parser_get_max_bit_rate(rm_parser* pParser);
UINT32      rm_parser_get_avg_bit_rate(rm_parser* pParser);
UINT32      rm_parser_get_max_packet_size(rm_parser* pParser);
UINT32      rm_parser_get_avg_packet_size(rm_parser* pParser);
UINT32      rm_parser_get_num_packets(rm_parser* pParser);
UINT32      rm_parser_get_duration(rm_parser* pParser);
UINT32      rm_parser_get_preroll(rm_parser* pParser);
const char* rm_parser_get_title(rm_parser* pParser);
const char* rm_parser_get_author(rm_parser* pParser);
const char* rm_parser_get_copyright(rm_parser* pParser);
const char* rm_parser_get_comment(rm_parser* pParser);
UINT32      rm_parser_get_num_streams(rm_parser* pParser);

/*
 * General file property accessors
 *
 * The most commonly-used properties of an .rm file
 * can be obtained with the rm_parser_get_xxx accessors above.
 * Alternatively, all of the file-level properties may
 * be obtained as an array of rm_property structs. This
 * array will include all of the common-used properties
 * above as well as any other properties that may be
 * present in the file.
 */
HX_RESULT rm_parser_get_file_properties(rm_parser*    pParser,
                                        rm_property** ppProp,
                                        UINT32*       pulNumProps);

/*
 * This function destroys an array of properties created
 * by either rm_parser_get_file_properties or
 * by rm_parser_get_stream_properties.
 */
void rm_parser_destroy_properties(rm_parser*    pParser,
                                  rm_property** ppProp,
                                  UINT32*       pulNumProps);

/*
 * rm_parser_get_stream_header
 *
 * This retrieves a stream header from the file. The number
 * of stream headers in the file can be found by calling
 * rm_get_num_streams(). Stream header properties may
 * be obtained by using the accessor functions found
 * in rm_stream.h. These accessors cover the most
 * commonly used properties. The entire set of properties
 * may be retrieved using rm_parser_get_stream_properties().
 */
HX_RESULT rm_parser_get_stream_header(rm_parser*         pParser,
                                      UINT32             ulStreamNum,
                                      rm_stream_header** ppHdr);

/*
 * rm_parser_destroy_stream_header
 *
 * This function frees any memory associated with
 * a stream header obtained by calling rm_parser_get_stream_header().
 */
void rm_parser_destroy_stream_header(rm_parser*         pParser,
                                     rm_stream_header** ppHdr);

/*
 * rm_parser_get_packet
 *
 * After the headers had been read, the user can repeatedly
 * call this function to retrieve packets from the .rm file.
 */
HX_RESULT rm_parser_get_packet(rm_parser*  pParser,
                               rm_packet** ppPacket);

#ifdef ME
HX_RESULT rm_parser_get_packet2(rm_parser*  pParser,
							   rm_packet** ppPacket);
#endif

/*
 * rm_parser_destroy_packet
 *
 * This is called to clean up packets created by
 * calls to rm_parser_get_packet().
 */
void rm_parser_destroy_packet(rm_parser*  pParser,
                              rm_packet** ppPacket);

#ifdef ME
void rm_parser_destroy_packet2(rm_parser*  pParser,
							  rm_packet** ppPacket);
#endif

/*
 * rm_parser_seek
 *
 * The user can call this function to seek to a
 * certain time in the .rm file
 */
HX_RESULT rm_parser_seek(rm_parser* pParser,
                         UINT32     ulTime);

/*
 * rm_parser_destroy
 *
 * The user calls this to clean up
 */
void rm_parser_destroy(rm_parser** ppParser);

/* The following functions for media engine -caijin @2008.05.28 */
HX_RESULT rm_parser_set_fileoffset(rm_parser* pParser, UINT32 ulCurFileOffset);

HX_RESULT rm_parser_get_fileoffset(rm_parser * pParser, UINT32 *ulCurFileOffset);

HX_RESULT rm_parser_get_timestamp_after_seek_time(rm_parser * pParser, UINT16 usStreamNum, UINT32 *ulTimeStamp);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RM_PARSE_H */
