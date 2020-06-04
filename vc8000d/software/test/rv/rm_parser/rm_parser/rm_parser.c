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

#include <stdio.h>
//#include <memory.h>
#include "string.h"
#include "helix_types.h"
#include "helix_result.h"
#include "rm_parse.h"
#include "rm_parser_internal.h"
#include "rm_memory_default.h"
#include "rm_error_default.h"
#include "rm_io_default.h"
#include "memory_utils.h"
#include "pack_utils.h"

//#include "rv_depack.h"
//#include "codec_defines.h"
//#include "helix_utils.h"

/*
 * We will create a buffer of this size to handle
 * reading of headers. The buffer will get increased
 * in size if we encounter a header bigger than this
 * size. Therefore, we would like to pick a reasonable
 * size so that the we don't have lots of little 
 * allocations. This size should be the maximum
 * of the expected size of the headers in the .rm file.
 *
 * XXXMEH - run lots of .rm files to see what a good
 * size to pick.
 */
#define RM_PARSER_INITIAL_READ_BUFFER_SIZE 256

HXBOOL rm_parser_is_rm_file(BYTE* pBuf, UINT32 ulSize)
{
    HXBOOL bRet = FALSE;

    /* Look for magic number ".RMF" at the beginning */
    if (ulSize >= 4)
    {
        UINT32 ulID = rm_unpack32(&pBuf, &ulSize);
        if (ulID == RM_HEADER_OBJECT)
        {
            bRet = TRUE;
        }
    }

    return bRet;
}

rm_parser* rm_parser_create(void*             pError,
                            rm_error_func_ptr fpError)
{
    return rm_parser_create2(pError,
                             fpError,
                             HXNULL,
                             rm_memory_default_malloc,
                             rm_memory_default_free);
}

rm_parser* rm_parser_create2(void*              pError,
                             rm_error_func_ptr  fpError,
                             void*              pMem,
                             rm_malloc_func_ptr fpMalloc,
                             rm_free_func_ptr   fpFree)
{
    rm_parser* pRet = HXNULL;

    if (fpMalloc)
    {
        /* Allocate space for the rm_parser_internal struct
         * by using the passed-in malloc function
         */
        rm_parser_internal* pInt = (rm_parser_internal*) fpMalloc(pMem, sizeof(rm_parser_internal));
        if (pInt)
        {
            /* Zero out the struct */
            memset((void*) pInt, 0, sizeof(rm_parser_internal));
            /*
             * Assign the error members. If the user did not
             * provide an error callback, then use the default
             * rm_error_default().
             */
            if (fpError)
            {
                pInt->fpError    = fpError;
                pInt->pUserError = pError;
            }
            else
            {
                pInt->fpError    = rm_error_default;
                pInt->pUserError = HXNULL;
            }
            /* Assign the memory functions */
            pInt->fpMalloc = fpMalloc;
            pInt->fpFree   = fpFree;
            pInt->pUserMem = pMem;
            /* Assign the return value */
            pRet = (rm_parser*) pInt;
        }
    }

    return pRet;
}

HX_RESULT rm_parser_init_stdio(rm_parser* pParser,
                               FILE*      fp)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser && fp)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Assign the default io functions */
        pInt->fpRead    = rm_io_default_read;
        pInt->fpSeek    = rm_io_default_seek;
        pInt->pUserRead = (void*) fp;
        /* Clear the return value */
        retVal = HXR_OK;
    }

    return retVal;
}

HX_RESULT rm_parser_init_io(rm_parser*        pParser,
                            void*             pUserRead,
                            rm_read_func_ptr  fpRead,
                            rm_seek_func_ptr  fpSeek)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser && fpRead && fpSeek)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Assign the default io functions */
        pInt->fpRead    = fpRead;
        pInt->fpSeek    = fpSeek;
        pInt->pUserRead = pUserRead;
        /* Clear the return value */
        retVal = HXR_OK;
    }

    return retVal;
}

#ifdef ME
HX_RESULT rm_parser_init_io2(rm_parser * pParser,
							 void *fp,
							 rm_read_func_ptr fpread,
							 rm_seek_func_ptr fpseek,
							 rm_read2_func_ptr fpread2)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser && fp)
	{
		/* Get the internal parser struct */
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		/* Assign the default io functions */
		pInt->fpRead = fpread;
		pInt->fpSeek = fpseek;
		pInt->fpRead2 = fpread2;
		pInt->pUserRead = (void *)fp;
		/* Clear the return value */
		retVal = HXR_OK;
	}

	return retVal;
}
#endif

HX_RESULT rm_parser_init_io_ex(rm_parser * pParser,
							 void *fp,
							 rm_read_func_ptr fpread,
							 rm_write_func_ptr fpwrite,
							 rm_seek_func_ptr fpseek,
							 rm_tell_func_ptr fptell)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser && fp)
	{
		/* Get the internal parser struct */
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		/* Assign the default io functions */
		pInt->fpRead = fpread;
		pInt->fpWrite = fpwrite;
		pInt->fpSeek = fpseek;
		pInt->fpTell = fptell;
		pInt->pUserRead = (void *)fp;
		/* Clear the return value */
		retVal = HXR_OK;
	}

	return retVal;
}

HX_RESULT rm_parser_read_headers(rm_parser* pParser)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Clear the return value */
        retVal = HXR_OK;
        /* Allocate the read buffer if necessary */
        if (!pInt->pReadBuffer)
        {
            retVal = rm_enforce_buffer_min_size(pInt->pUserMem,
                                                pInt->fpMalloc,
                                                pInt->fpFree,
                                                &pInt->pReadBuffer,
                                                &pInt->ulReadBufferSize,
                                                RM_PARSER_INITIAL_READ_BUFFER_SIZE);
            if (retVal == HXR_OK)
            {
                /* Set the number of bytes read to zero */
                pInt->ulNumBytesRead = 0;
            }
        }
        if (retVal == HXR_OK)
        {
            retVal = rm_parseri_read_all_headers(pInt);
        }
    }

    return retVal;
}


UINT32 rm_parser_get_max_bit_rate(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.max_bit_rate;
    }

    return ulRet;
}

UINT32 rm_parser_get_avg_bit_rate(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.avg_bit_rate;
    }

    return ulRet;
}

UINT32 rm_parser_get_max_packet_size(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.max_pkt_size;
    }

    return ulRet;
}

UINT32 rm_parser_get_avg_packet_size(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.avg_pkt_size;
    }

    return ulRet;
}

UINT32 rm_parser_get_num_packets(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.num_pkts;
    }

    return ulRet;
}

UINT32 rm_parser_get_duration(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.duration;
    }

    return ulRet;
}

UINT32 rm_parser_get_preroll(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->propHdr.preroll;
    }

    return ulRet;
}

const char* rm_parser_get_title(rm_parser* pParser)
{
    const char* pRet = HXNULL;

    if (pParser)
    {
        pRet = (const char*) ((rm_parser_internal*) pParser)->contHdr.title;
    }

    return pRet;
}

const char* rm_parser_get_author(rm_parser* pParser)
{
    const char* pRet = HXNULL;

    if (pParser)
    {
        pRet = (const char*) ((rm_parser_internal*) pParser)->contHdr.author;
    }

    return pRet;
}

const char* rm_parser_get_copyright(rm_parser* pParser)
{
    const char* pRet = HXNULL;

    if (pParser)
    {
        pRet = (const char*) ((rm_parser_internal*) pParser)->contHdr.copyright;
    }

    return pRet;
}

const char* rm_parser_get_comment(rm_parser* pParser)
{
    const char* pRet = HXNULL;

    if (pParser)
    {
        pRet = (const char*) ((rm_parser_internal*) pParser)->contHdr.comment;
    }

    return pRet;
}

UINT32 rm_parser_get_num_streams(rm_parser* pParser)
{
    UINT32 ulRet = 0;

    if (pParser)
    {
        ulRet = ((rm_parser_internal*) pParser)->ulNumStreams;
    }

    return ulRet;
}

HX_RESULT rm_parser_get_file_properties(rm_parser*    pParser,
                                        rm_property** ppProp,
                                        UINT32*       pulNumProps)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser && ppProp && pulNumProps)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        if (pInt->fpMalloc && pInt->fpFree)
        {
            /* Get the number of file properties */
            UINT32 ulNum = rm_parseri_get_num_file_properties(pInt);
            if (ulNum)
            {
                /* Compute the size of the rm_property array */
                UINT32 ulSize = ulNum * sizeof(rm_property);
                /* Allocate an array of this size */
				rm_property *pPropArr = (rm_property*) rm_parseri_malloc(pInt, ulSize);
                if (pPropArr)
                {
                    /* NULL out the memory */
                    memset(pPropArr, 0, ulSize);
                    /* Collect the properties */
                    retVal = rm_parseri_get_file_properties(pInt, pPropArr, ulNum);
                    if (retVal == HXR_OK)
                    {
                        /* Assign the out parameters */
                        *ppProp      = pPropArr;
                        *pulNumProps = ulNum;
                    }
                    else
                    {
                        /* Free the array */
                        rm_parseri_free(pInt, pPropArr);
                    }
                }
            }
        }
    }

    return retVal;
}

void rm_parser_destroy_properties(rm_parser*    pParser,
                                  rm_property** ppProp,
                                  UINT32*       pulNumProps)
{
    if (pParser && ppProp && pulNumProps && *ppProp && *pulNumProps)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        if (pInt->fpFree)
        {
            /* Get the pointer to the array */
            rm_property* pProp = *ppProp;
            /* Clean up the properties */
            UINT32 i = 0;
            for (i = 0; i < *pulNumProps; i++)
            {
                rm_parseri_cleanup_rm_property(pInt, &pProp[i]);
            }
            /* Free the memory */
            rm_parseri_free(pInt, pProp);
            /* NULL out the pointer */
            *ppProp = HXNULL;
            /* Zero out the number of properties */
            *pulNumProps = 0;
        }
    }
}

HX_RESULT rm_parser_get_stream_header(rm_parser*         pParser,
                                      UINT32             ulStreamNum,
                                      rm_stream_header** ppHdr)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser && ppHdr)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        if (pInt->fpMalloc)
        {
            rm_stream_header* pHdr = (rm_stream_header*) rm_parseri_malloc(pInt, sizeof(rm_stream_header));
            if (pHdr)
            {
                /* NULL out the memory */
                memset(pHdr, 0, sizeof(rm_stream_header));
                /* Copy the stream header */
                retVal = rm_parseri_copy_stream_header(pInt, ulStreamNum, pHdr);
                if (retVal == HXR_OK)
                {
                    /* Assign the out parameter */
                    *ppHdr = pHdr;
                }
                else
                {
                    /* Clean up the stream header */
                    rm_parseri_cleanup_stream_header(pInt, pHdr);
                    /* Free the memory we allocated */
                    rm_parseri_free(pInt, pHdr);
                }
            }
        }
    }

    return retVal;
}

void rm_parser_destroy_stream_header(rm_parser*         pParser,
                                     rm_stream_header** ppHdr)
{
    if (pParser && ppHdr && *ppHdr)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Cleanup the stream header */
        rm_parseri_cleanup_stream_header(pInt, *ppHdr);
        /* Free the memory associated with this header */
        rm_parseri_free(pInt, *ppHdr);
        /* NULL out the pointer */
        *ppHdr = HXNULL;
    }
}

/*
* Return:	HXR_OK
*			HXR_FAIL
*			HXR_UNEXPECTED
*			HXR_READ_ERROR
*			HXR_CORRUPT_FILE
*			HXR_OUTOFMEMORY
*			HXR_AT_END
*/
HX_RESULT rm_parser_get_packet(rm_parser*  pParser,
                               rm_packet** ppPacket)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser && ppPacket)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Read the next packet */
        retVal = rm_parseri_read_next_packet(pInt, ppPacket);
    }

    return retVal;
}

void rm_parser_destroy_packet(rm_parser*  pParser,
                              rm_packet** ppPacket)
{
    if (pParser && ppPacket && *ppPacket)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        if (pInt->fpFree)
        {
            /* Free the packet data */
            if ((*ppPacket)->pData)
            {
                rm_parseri_free(pInt, (*ppPacket)->pData);
                (*ppPacket)->pData = HXNULL;
            }
            /* Free the packet */
            rm_parseri_free(pInt, *ppPacket);
            *ppPacket = HXNULL;
        }
    }
}

#ifdef ME
HX_RESULT rm_parser_get_packet2(rm_parser*  pParser,
							   rm_packet** ppPacket)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser && ppPacket)
	{
		/* Get the internal parser struct */
		rm_parser_internal* pInt = (rm_parser_internal*) pParser;
		/* Read the next packet */
		retVal = rm_parseri_read_next_packet2(pInt, ppPacket);
	}

	return retVal;
}

void rm_parser_destroy_packet2(rm_parser*  pParser,
							  rm_packet** ppPacket)
{
	if (pParser && ppPacket && *ppPacket)
	{
		/* Get the internal parser struct */
		rm_parser_internal* pInt = (rm_parser_internal*) pParser;
		if (pInt->fpFree)
		{
			/* Free the packet */
			rm_parseri_free(pInt, *ppPacket);
			*ppPacket = HXNULL;
		}
	}
}
#endif

HX_RESULT rm_parser_seek(rm_parser* pParser,
                         UINT32     ulTime)
{
    HX_RESULT retVal = HXR_FAIL;

    if (pParser)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) pParser;
        /* Read the next packet */
        retVal = rm_parseri_seek(pInt, ulTime);
    }

    return retVal;
}

void rm_parser_destroy(rm_parser** ppParser)
{
    if (ppParser)
    {
        /* Get the internal parser struct */
        rm_parser_internal* pInt = (rm_parser_internal*) *ppParser;
        if (pInt && pInt->fpFree)
        {
            /* Save a pointer to fpFree and pUserMem */
            rm_free_func_ptr fpFree   = pInt->fpFree;
            void*            pUserMem = pInt->pUserMem;
            /* Clean up the content header */
            rm_parseri_cleanup_content_hdr(pInt);
            /* Clean up the media props headers */
            rm_parseri_cleanup_all_media_props_hdrs(pInt);
            /* Clean up all logical stream headers */
            rm_parseri_cleanup_all_logical_stream_hdrs(pInt);
            /* Clean up the logical fileinfo header */
            //rm_parseri_cleanup_logical_stream_hdr(pInt, pInt->pLogicalFileInfo);
			/*changed for memory lead,Zhang Xuecheng,2007.11.07 */
			rm_parseri_cleanup_logical_fileinfo_hdr(pInt);
            /* Clean up the read buffer */
            rm_parseri_cleanup_read_buffer(pInt);
            /* Free the stream num map */
            rm_parseri_cleanup_stream_num_map(pInt);
            /* Clean up the stream info array */
            rm_parseri_cleanup_stream_info_array(pInt);
            /* Clean up the stream header array */
            rm_parseri_cleanup_all_stream_headers(pInt);
            /* Null everything out */
            memset(pInt, 0, sizeof(rm_parser_internal));
            /* Free the rm_parser_internal struct memory */
            fpFree(pUserMem, pInt);
            /* NULL out the pointer */
            *ppParser = HXNULL;
        }
    }
}

UINT32 rm_parser_seek_rv_timestamp(rm_parser * pParser)
{
	UINT32 retVal = 0;

	if (pParser)
	{
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		retVal =  pInt->ulRvTimeStampAfterSeek;
	}

	return retVal;
}

UINT32 rm_parser_seek_ra_timestamp(rm_parser * pParser)
{
	UINT32 retVal = 0;

	if (pParser)
	{
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		retVal = pInt->ulRaTimeStampAfterSeek;
	}

	return retVal;
}

UINT32 rm_parser_get_chunk_index(rm_parser * pParser)
{
	UINT32 retVal = 0;

	if (pParser)
	{
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		retVal = pInt->propHdr.index_offset;
	}

	return retVal;
}

HXBOOL rm_parser_is_end(rm_parser * pParser)
{
	rm_parser_internal *pInt = (rm_parser_internal *) pParser;

	return pInt->ulCurFileOffset < (pInt->propHdr.data_offset + pInt->dataHdr.size)?0:1;
}

HX_RESULT rm_parser_set_fileoffset(rm_parser * pParser, UINT32 ulCurFileOffset)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser)
	{
		/* Get the internal parser struct */
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		/* Set the given CurFileOffset */
		retVal = rm_parseri_set_fileoffset(pInt, ulCurFileOffset);		
	}

	return retVal;
}

HX_RESULT rm_parser_get_fileoffset(rm_parser * pParser, UINT32 *ulCurFileOffset)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser && ulCurFileOffset)
	{
		/* Get the internal parser struct */
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		/* Set the given CurFileOffset */
		retVal = rm_parseri_get_fileoffset(pInt, ulCurFileOffset);		
	}

	return retVal;
}

HX_RESULT rm_parser_get_timestamp_after_seek_time(rm_parser * pParser, UINT16 usStreamNum, UINT32 *ulTimeStamp)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pParser && ulTimeStamp)
	{
		/* Get the internal parser struct */
		rm_parser_internal *pInt = (rm_parser_internal *) pParser;

		/* Set the given CurFileOffset */
		retVal = rm_parseri_get_timestamp_after_seek_time(pInt, usStreamNum, ulTimeStamp);		
	}

	return retVal;
}
