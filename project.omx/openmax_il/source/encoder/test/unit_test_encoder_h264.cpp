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
#include "encoder_h264.h"
#include "codec.h"
#include "util.h" // FLOAT_Q16
#include "h264encapi.h"
#include <OSAL.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////////////////
// decoder stub implementations
////////////////////////////////////////////////////////////

static H264EncApiVersion EncApiVersion =
{ 1, 0 };
static H264EncBuild EncBuild =
{ 2, 3 };
static H264EncRet EncInitStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncReleaseStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncSetCodingCtrlStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncGetCodingCtrlStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncSetRateCtrlStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncGetRateCtrlStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncSetPreProcessingStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncGetPreProcessingStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncStrmStartStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncStrmEncodeStatus = H264ENC_INSTANCE_ERROR;
static H264EncRet EncStrmEndStatus = H264ENC_INSTANCE_ERROR;

H264EncApiVersion H264EncGetApiVersion(void)
{

	return EncApiVersion;
}

H264EncBuild H264EncGetBuild(void)
{
	return EncBuild;
}

H264EncRet H264EncInit(const H264EncConfig * pEncConfig, H264EncInst * instAddr)
{
	if (EncInitStatus == H264ENC_OK)
	{
		*instAddr = OMX_OSAL_Malloc(sizeof(H264EncInst));
	}

	return EncInitStatus;
}

H264EncRet H264EncRelease(H264EncInst inst)
{
	if (EncInitStatus == H264ENC_OK)
	{
		OMX_OSAL_Free( const_cast<void *>(inst) );
	}

	return EncReleaseStatus;
}

H264EncRet H264EncSetCodingCtrl(H264EncInst inst, const H264EncCodingCtrl *
pCodingParams)
{
	return EncSetCodingCtrlStatus;
}

H264EncRet H264EncGetCodingCtrl(H264EncInst inst, H264EncCodingCtrl *
pCodingParams)
{
	return EncGetCodingCtrlStatus;
}

H264EncRet H264EncSetRateCtrl(H264EncInst inst,
		const H264EncRateCtrl * pRateCtrl)
{
	return EncSetRateCtrlStatus;
}

H264EncRet H264EncGetRateCtrl(H264EncInst inst, H264EncRateCtrl * pRateCtrl)
{
	return EncGetRateCtrlStatus;
}

H264EncRet H264EncSetPreProcessing(H264EncInst inst,
		const H264EncPreProcessingCfg *
		pPreProcCfg)
{
	return EncSetPreProcessingStatus;
}

H264EncRet H264EncGetPreProcessing(H264EncInst inst,
		H264EncPreProcessingCfg * pPreProcCfg)
{
	return EncGetPreProcessingStatus;
}

H264EncRet H264EncStrmStart(H264EncInst inst, const H264EncIn * pEncIn,
		H264EncOut * pEncOut)
{
	return EncStrmStartStatus;
}

H264EncRet H264EncStrmEncode(H264EncInst inst, const H264EncIn * pEncIn,
		H264EncOut * pEncOut)
{

	pEncOut->codingType = pEncIn->codingType;

	return EncStrmEncodeStatus;
}

H264EncRet H264EncStrmEnd(H264EncInst inst, const H264EncIn * pEncIn,
		H264EncOut * pEncOut)
{
	return EncStrmEndStatus;
}

////////////////////////////////////////////////////////////
// test cases
////////////////////////////////////////////////////////////
void InitStatuses()
{
	EncInitStatus = H264ENC_INSTANCE_ERROR;
	EncReleaseStatus = H264ENC_INSTANCE_ERROR;
	EncSetCodingCtrlStatus = H264ENC_INSTANCE_ERROR;
	EncGetCodingCtrlStatus = H264ENC_INSTANCE_ERROR;
	EncSetRateCtrlStatus = H264ENC_INSTANCE_ERROR;
	EncGetRateCtrlStatus = H264ENC_INSTANCE_ERROR;
	EncSetPreProcessingStatus = H264ENC_INSTANCE_ERROR;
	EncGetPreProcessingStatus = H264ENC_INSTANCE_ERROR;
	EncStrmStartStatus = H264ENC_INSTANCE_ERROR;
	EncStrmEncodeStatus = H264ENC_INSTANCE_ERROR;
	EncStrmEndStatus = H264ENC_INSTANCE_ERROR;
}

//
// create fails
//
void test0()
{
	EncInitStatus = H264ENC_MEMORY_ERROR;
	H264_CONFIG config;
	ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
	BOOST_REQUIRE(i == NULL);
}

//
// create, decode, destroy
//
void test1()
{
	H264_CONFIG config;

	config.h264_config.eProfile = OMX_VIDEO_AVCProfileBaseline;
	config.h264_config.eLevel = OMX_VIDEO_AVCLevel3;

	config.common_config.nOutputWidth = 112;
	config.common_config.nOutputHeight = 96;
	config.common_config.nInputFramerate = FLOAT_Q16(30);

	config.nSliceHeight = 0; // no slices
	config.bDisableDeblocking = OMX_FALSE;
	config.bSeiMessages = OMX_FALSE; // not supported???

	//config.rate_config.eRateControl = OMX_Video_ControlRateVariableSkipFrames;
	config.rate_config.eRateControl = OMX_Video_ControlRateVariable; // no skip
	config.rate_config.nQpDefault = 10;
	config.rate_config.nQpMin = 0;
	config.rate_config.nQpMax = 51;
	config.rate_config.nTargetBitrate = 0; // use default. Used to be 128000
	config.rate_config.nPictureRcEnabled = 0;
	config.rate_config.nMbRcEnabled = 0;
	config.rate_config.nHrdEnabled = 0;

	config.pp_config.origWidth = 320;
	config.pp_config.origHeight = 244;
	config.pp_config.xOffset = 3;
	config.pp_config.yOffset = 1;
	config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedPlanar;
	config.pp_config.angle = 0;
	config.pp_config.frameStabilization = OMX_FALSE;

	config.nPFrames = 0;

	EncInitStatus = H264ENC_OK;
	EncGetCodingCtrlStatus = H264ENC_OK;
	EncSetCodingCtrlStatus = H264ENC_OK;
	EncGetRateCtrlStatus = H264ENC_OK;
	EncSetRateCtrlStatus = H264ENC_OK;
	EncGetPreProcessingStatus = H264ENC_OK;
	EncSetPreProcessingStatus = H264ENC_OK;
	ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_h264(&config);
	BOOST_REQUIRE(i != NULL);

	STREAM_BUFFER stream;

	EncStrmStartStatus = H264ENC_OK;
	CODEC_STATE s;
	s = i->stream_start(i, &stream);
	BOOST_REQUIRE(s == CODEC_OK);

	EncStrmEncodeStatus = H264ENC_FRAME_READY;
	FRAME buf;
	buf.frame_type = INTRA_FRAME;
	s = i->encode(i, &buf, &stream);
	BOOST_REQUIRE(s == CODEC_CODED_INTRA);

	buf.frame_type = PREDICTED_FRAME;
	EncStrmEncodeStatus = H264ENC_FRAME_READY;
	s = i->encode(i, &buf, &stream);
	//BOOST_REQUIRE(s == CODEC_CODED_PREDICTED);

	EncStrmEndStatus = H264ENC_OK;
	s = i->stream_end(i, &stream);
	BOOST_REQUIRE(s == CODEC_OK);

	EncReleaseStatus = H264ENC_OK;
	i->destroy(i);
}

int test_main(int, char* [])
{
	cout << "running " <<__FILE__ << " tests\n";

	test0();
	test1();

	return 0;
}
