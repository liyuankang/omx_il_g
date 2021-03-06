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

//#include <memory.h>
#include "helix_types.h"
#include "helix_result.h"
#include "ra_decode.h"
#include "rm_memory.h"
#include "rm_error.h"
#include "rm_memory_default.h"

/*
#include "ra8hbr_decode.h"
#include "ra8lbr_decode.h"
#include "sipro_decode.h"
#include "aac_decode.h"
*/

#include "hal_io.h"

ra_decode *ra_decode_create2(void *pUserError,
	rm_error_func_ptr fpError, void *pUserMem, rm_malloc_func_ptr fpMalloc, rm_free_func_ptr fpFree)
{
	ra_decode *pRet = HXNULL;

	if (fpMalloc && fpFree)
	{
		pRet = (ra_decode *) fpMalloc(pUserMem, sizeof(ra_decode));

		if (pRet)
		{
			/* Zero out the struct */
			HALMemset((void *)pRet, 0, sizeof(ra_decode));
			/* Assign the error function */
			pRet->fpError = fpError;
			pRet->pUserError = pUserError;
			/* Assign the memory functions */
			pRet->fpMalloc = fpMalloc;
			pRet->fpFree = fpFree;
			pRet->pUserMem = pUserMem;
		}
	}

	return pRet;
}

/* ra_decode_destroy()
 * Deletes the decoder backend and frontend instances. */
void ra_decode_destroy(ra_decode * pFrontEnd)
{
	rm_free_func_ptr fpFree;
	void *pUserMem;

	if (pFrontEnd && pFrontEnd->fpFree)
	{
		/* Save a pointer to fpFree and pUserMem */
		fpFree = pFrontEnd->fpFree;
		pUserMem = pFrontEnd->pUserMem;

		if (pFrontEnd->pDecode && pFrontEnd->fpClose)
		{
			/* Free the decoder instance and backend */
			pFrontEnd->fpClose(pFrontEnd->pDecode, pUserMem, fpFree);
		}

		/* Free the ra_decode struct memory */
		fpFree(pUserMem, pFrontEnd);
	}
}

/* ra_decode_init()
 * Selects decoder backend with fourCC code.
 * Calls decoder backend init function with init params.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT
ra_decode_init(ra_decode * pFrontEnd,
	UINT32 ulFourCC, void *pInitParams, UINT32 ulInitParamsSize, ra_format_info * pStreamInfo)
{
	HX_RESULT retVal = HXR_OK;
//
//	/* Assign the backend function pointers */
//	if (ulFourCC == 0x61747263)			/* 'atrc' */
//	{
//		pFrontEnd->fpInit = ra8hbr_decode_init;
//		pFrontEnd->fpReset = ra8hbr_decode_reset;
//		pFrontEnd->fpConceal = ra8hbr_decode_conceal;
//		pFrontEnd->fpDecode = ra8hbr_decode_decode;
//		pFrontEnd->fpGetMaxSize = ra8hbr_decode_getmaxsize;
//		pFrontEnd->fpGetChannels = ra8hbr_decode_getchannels;
//		pFrontEnd->fpGetChannelMask = ra8hbr_decode_getchannelmask;
//		pFrontEnd->fpGetSampleRate = ra8hbr_decode_getrate;
//		pFrontEnd->fpMaxSamp = ra8hbr_decode_getdelay;
//		pFrontEnd->fpClose = ra8hbr_decode_close;
//		//retVal = HXR_DEC_NOT_FOUND;
//		//printf("ra8hbr decoder not supported\n");
//	}
//	else if (ulFourCC == 0x636F6F6B)	/* 'cook' */
//	{
//		pFrontEnd->fpInit = ra8lbr_decode_init;
//		pFrontEnd->fpReset = ra8lbr_decode_reset;
//		pFrontEnd->fpConceal = ra8lbr_decode_conceal;
//		pFrontEnd->fpDecode = ra8lbr_decode_decode;
//		pFrontEnd->fpGetMaxSize = ra8lbr_decode_getmaxsize;
//		pFrontEnd->fpGetChannels = ra8lbr_decode_getchannels;
//		pFrontEnd->fpGetChannelMask = ra8lbr_decode_getchannelmask;
//		pFrontEnd->fpGetSampleRate = ra8lbr_decode_getrate;
//		pFrontEnd->fpMaxSamp = ra8lbr_decode_getdelay;
//		pFrontEnd->fpClose = ra8lbr_decode_close;
//		//HPX
//		//retVal = HXR_DEC_NOT_FOUND;
//		//printf("ra8lbr decoder supported\n");
//
//	}
//	else if (ulFourCC == 0x73697072)	/* 'sipr' */
//	{
//		//HPX
//		/*
//		 * pFrontEnd->fpInit = sipro_decode_init;
//		 * pFrontEnd->fpReset = sipro_decode_reset;
//		 * pFrontEnd->fpConceal = sipro_decode_conceal;
//		 * pFrontEnd->fpDecode = sipro_decode_decode;
//		 * pFrontEnd->fpGetMaxSize = sipro_decode_getmaxsize;
//		 * pFrontEnd->fpGetChannels = sipro_decode_getchannels;
//		 * pFrontEnd->fpGetChannelMask = sipro_decode_getchannelmask;
//		 * pFrontEnd->fpGetSampleRate = sipro_decode_getrate;
//		 * pFrontEnd->fpMaxSamp = sipro_decode_getdelay;
//		 * pFrontEnd->fpClose = sipro_decode_close;
//		 */
//		retVal = HXR_DEC_NOT_FOUND;
//		printf("sipro decoder not supported\n");
//
//	}
//	else if (ulFourCC == 0x72616163 ||	/* 'raac' */
//		ulFourCC == 0x72616370)			/* 'racp' */
//	{
//		//HPX
//		pFrontEnd->fpInit = aac_decode_init;
//		pFrontEnd->fpReset = aac_decode_reset;
//		pFrontEnd->fpConceal = aac_decode_conceal;
//		pFrontEnd->fpDecode = aac_decode_decode;
//		pFrontEnd->fpGetMaxSize = aac_decode_getmaxsize;
//		pFrontEnd->fpGetChannels = aac_decode_getchannels;
//		pFrontEnd->fpGetChannelMask = aac_decode_getchannelmask;
//		pFrontEnd->fpGetSampleRate = aac_decode_getrate;
//		pFrontEnd->fpMaxSamp = aac_decode_getdelay;
//		pFrontEnd->fpClose = aac_decode_close;
//		//retVal = HXR_DEC_NOT_FOUND;
//		printf("AAC decoder supported\n");
//	}
//	else
//	{
//		/* error - codec not supported */
//		retVal = HXR_DEC_NOT_FOUND;
//		//HPX
//		printf("Other decoder not supported\n");
//	}
//
//	if (retVal == HXR_OK && pFrontEnd && pFrontEnd->fpInit && pStreamInfo)
//	{
//		retVal = pFrontEnd->fpInit(pInitParams, ulInitParamsSize, pStreamInfo,
//			&pFrontEnd->pDecode, pFrontEnd->pUserMem, pFrontEnd->fpMalloc, pFrontEnd->fpFree);
//	}
//
	return retVal;
}

/* ra_decode_reset()
 * Calls decoder backend reset function.
 * Depending on which codec is in use, *pNumSamplesOut samples may
 * be flushed. After reset, the decoder returns to its initial state.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT
ra_decode_reset(ra_decode * pFrontEnd,
	UINT16 * pSamplesOut, UINT32 ulNumSamplesAvail, UINT32 * pNumSamplesOut)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpReset)
	{
		retVal = pFrontEnd->fpReset(pFrontEnd->pDecode, pSamplesOut,
			ulNumSamplesAvail, pNumSamplesOut);
	}

	return retVal;
}

/* ra_decode_conceal()
 * Calls decoder backend conceal function.
 * On successive calls to ra_decode_decode(), the decoder will attempt
 * to conceal ulNumSamples. No input data should be sent while concealed
 * frames are being produced. Once the decoder has exhausted the concealed
 * samples, it can proceed normally with decoding valid input data.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_conceal(ra_decode * pFrontEnd, UINT32 ulNumSamples)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpConceal)
	{
		retVal = pFrontEnd->fpConceal(pFrontEnd->pDecode, ulNumSamples);
	}

	return retVal;
}

/* ra_decode_decode()
 * Calls decoder backend decode function.
 * pData             : input data (compressed frame).
 * ulNumBytes        : input data size in bytes.
 * pNumBytesConsumed : amount of input data consumed by decoder.
 * pSamplesOut       : output data (uncompressed frame).
 * ulNumSamplesAvail : size of output buffer.
 * pNumSamplesOut    : amount of ouput data produced by decoder.
 * ulFlags           : control flags for decoder.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT
ra_decode_decode(ra_decode * pFrontEnd,
	UINT8 * pData,
	UINT32 ulNumBytes,
	UINT32 * pNumBytesConsumed,
	UINT16 * pSamplesOut, UINT32 ulNumSamplesAvail, UINT32 * pNumSamplesOut, UINT32 ulFlags)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpDecode)
	{
		retVal = pFrontEnd->fpDecode(pFrontEnd->pDecode, pData, ulNumBytes,
			pNumBytesConsumed, pSamplesOut, ulNumSamplesAvail, pNumSamplesOut, ulFlags);
	}

	return retVal;
}

/**************** Accessor Functions *******************/
/* ra_decode_getmaxsize()
 * pNumSamples receives the maximum number of samples produced
 * by the decoder in response to a call to ra_decode_decode().
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_getmaxsize(ra_decode * pFrontEnd, UINT32 * pNumSamples)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpGetMaxSize)
	{
		retVal = pFrontEnd->fpGetMaxSize(pFrontEnd->pDecode, pNumSamples);
	}

	return retVal;
}

/* ra_decode_getchannels()
 * pNumChannels receives the number of audio channels in the bitstream.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_getchannels(ra_decode * pFrontEnd, UINT32 * pNumChannels)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpGetChannels)
	{
		retVal = pFrontEnd->fpGetChannels(pFrontEnd->pDecode, pNumChannels);
	}

	return retVal;
}

/* ra_decode_getchannelmask()
 * pChannelMask receives the 32-bit mapping of the audio output channels.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_getchannelmask(ra_decode * pFrontEnd, UINT32 * pChannelMask)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpGetChannelMask)
	{
		retVal = pFrontEnd->fpGetChannelMask(pFrontEnd->pDecode, pChannelMask);
	}

	return retVal;
}

/* ra_decode_getrate()
 * pSampleRate receives the sampling rate of the output samples.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_getrate(ra_decode * pFrontEnd, UINT32 * pSampleRate)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpGetSampleRate)
	{
		retVal = pFrontEnd->fpGetSampleRate(pFrontEnd->pDecode, pSampleRate);
	}

	return retVal;
}

/* ra_decode_getdelay()
 * pNumSamples receives the number of invalid output samples
 * produced by the decoder at startup.
 * If non-zero, it is up to the user to discard these samples.
 * Returns zero on success, negative result indicates failure. */
HX_RESULT ra_decode_getdelay(ra_decode * pFrontEnd, UINT32 * pNumSamples)
{
	HX_RESULT retVal = HXR_FAIL;

	if (pFrontEnd && pFrontEnd->fpMaxSamp)
	{
		retVal = pFrontEnd->fpMaxSamp(pFrontEnd->pDecode, pNumSamples);
	}

	return retVal;
}
