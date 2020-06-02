/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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

#include <boost/test/minimal.hpp>
#include "encoder_jpeg.h"
#include "codec.h"
#include "util.h"
#include "jpegencapi.h"
#include <OSAL.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////////////////
// decoder stub implementations
// Use stubs to check that encoder passes all required 
// parameters to codec.
////////////////////////////////////////////////////////////

static JpegEncApiVersion EncApiVersion = { 1, 0 };
static JpegEncBuild EncBuild = { 2, 3 };
static JpegEncRet EncInitStatus = JPEGENC_INSTANCE_ERROR;
static JpegEncRet EncReleaseStatus = JPEGENC_INSTANCE_ERROR;
static JpegEncRet EncSetFullResolutionModeStatus = JPEGENC_INSTANCE_ERROR;
static JpegEncRet EncSetThumbnailModeStatus = JPEGENC_INSTANCE_ERROR;
static JpegEncRet EncEncodeStatus = JPEGENC_INSTANCE_ERROR;
static JpegEncRet EncStrmEndStatus = JPEGENC_INSTANCE_ERROR;

JpegEncApiVersion JpegEncGetApiVersion(void)
{
	return EncApiVersion;
}

JpegEncBuild JpegEncGetBuild(void)
{
	return EncBuild;
}

JpegEncRet JpegEncInit(JpegEncCfg * pEncCfg, JpegEncInst * instAddr)
{
	if (EncInitStatus == JPEGENC_OK)
	{
		*instAddr = OMX_OSAL_Malloc(sizeof(JpegEncInst));
	}

	return EncInitStatus;
}

JpegEncRet JpegEncRelease(JpegEncInst inst)
{
	if (EncInitStatus == JPEGENC_OK)
	{
		OMX_OSAL_Free( const_cast<void *>(inst) );
	}

	return EncReleaseStatus;
}

JpegEncRet JpegEncEncode(JpegEncInst inst, JpegEncIn * pEncIn,
						 JpegEncOut * pEncOut)
{
	return EncEncodeStatus;
}

JpegEncRet JpegEncSetFullResolutionMode(JpegEncInst inst, JpegEncCfg * pEncCfg)
{
	return EncSetFullResolutionModeStatus;
}

JpegEncRet JpegEncSetThumbnailMode(JpegEncInst inst, JpegEncCfg * pEncCfg)
{
	return EncSetThumbnailModeStatus;
}

//
// create fails
//
void test0()
{
	EncInitStatus = JPEGENC_MEMORY_ERROR;
	JPEG_CONFIG config;
	ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_jpeg(&config);
	BOOST_REQUIRE(i == NULL);
}

void test1()
{
	JPEG_CONFIG config;

	config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
	config.pp_config.angle = 0;
	config.qLevel = 5;
	config.sliceHeight = 0;
	config.pp_config.origWidth = 320;
	config.pp_config.origHeight = 640;
	config.pp_config.xOffset = 100;
	config.pp_config.yOffset = 100;
	config.codingWidth = 320;
	config.codingHeight = 640;

	EncInitStatus = JPEGENC_OK;
	EncSetFullResolutionModeStatus = JPEGENC_OK;

	ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_jpeg(&config);
	BOOST_REQUIRE(i != NULL);
	
	STREAM_BUFFER stream;

	CODEC_STATE s;
	s = i->stream_start(i, &stream);
	BOOST_REQUIRE(s == CODEC_OK);

	EncEncodeStatus = JPEGENC_FRAME_READY;
	FRAME buf;
	s = i->encode(i, &buf, &stream);
	BOOST_REQUIRE(s == CODEC_OK);

	EncStrmEndStatus = JPEGENC_OK;
	s = i->stream_end(i, &stream);
	BOOST_REQUIRE(s == CODEC_OK);

	EncReleaseStatus = JPEGENC_OK;
	i->destroy(i);
}

int test_main(int, char* [])
{
	cout << "Running " <<__FILE__ << " tests\n";

	test0();
	test1();

	return 0;
}
