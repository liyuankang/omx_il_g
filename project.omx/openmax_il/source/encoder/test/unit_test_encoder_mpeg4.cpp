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
#include "encoder_mpeg4.h"
#include "codec.h"
#include "util.h" // FLOAT_Q16
#include "mp4encapi.h"
#include <OSAL.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////////////////
// decoder stub implementations
////////////////////////////////////////////////////////////

static MP4EncApiVersion EncApiVersion =
{ 1, 0 };
static MP4EncBuild EncBuild =
{ 2, 3 };
static MP4EncRet EncInitStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncReleaseStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncSetCodingCtrlStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncGetCodingCtrlStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncSetRateCtrlStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncGetRateCtrlStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncSetPreProcessingStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncGetPreProcessingStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncStrmStartStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncStrmEncodeStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncStrmEndStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncSetStreamInfoStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncGetStreamInfoStatus = ENC_INSTANCE_ERROR;
static MP4EncRet EncSetUsrDataStatus = ENC_INSTANCE_ERROR;

MP4EncApiVersion MP4EncGetApiVersion(void)
{
    return EncApiVersion;
}

MP4EncBuild MP4EncGetBuild(void)
{
    return EncBuild;
}

MP4EncRet MP4EncInit(const MP4EncCfg * pEncCfg, MP4EncInst * instAddr)
{
    if (EncInitStatus == ENC_OK)
    {
        *instAddr = OMX_OSAL_Malloc(sizeof(MP4EncInst));
    }

    return EncInitStatus;
}

MP4EncRet MP4EncRelease(MP4EncInst inst)
{
    if (EncInitStatus == ENC_OK)
    {
        OMX_OSAL_Free( const_cast<void *>(inst) );
    }

    return EncReleaseStatus;
}

/* Encoder configuration */
MP4EncRet MP4EncSetStreamInfo(MP4EncInst inst,
        const MP4EncStreamInfo * pStreamInfo)
{
    return EncSetStreamInfoStatus;
}

MP4EncRet MP4EncGetStreamInfo(MP4EncInst inst, MP4EncStreamInfo * pStreamInfo)
{
    return EncGetStreamInfoStatus;
}

MP4EncRet MP4EncSetCodingCtrl(MP4EncInst inst,
        const MP4EncCodingCtrl * pCodeParams)
{
    return EncSetCodingCtrlStatus;
}

MP4EncRet MP4EncGetCodingCtrl(MP4EncInst inst, MP4EncCodingCtrl * pCodeParams)
{
    return EncGetCodingCtrlStatus;
}

MP4EncRet MP4EncSetRateCtrl(MP4EncInst inst, const MP4EncRateCtrl * pRateCtrl)
{
    return EncSetRateCtrlStatus;
}

MP4EncRet MP4EncGetRateCtrl(MP4EncInst inst, MP4EncRateCtrl * pRateCtrl)
{
    return EncGetRateCtrlStatus;
}

MP4EncRet MP4EncSetUsrData(MP4EncInst inst, const u8 * pBuf, u32 length,
        MP4EncUsrDataType type)
{
    return EncSetUsrDataStatus;
}

/* Stream generation */
MP4EncRet MP4EncStrmStart(MP4EncInst inst, const MP4EncIn * pEncIn,
        MP4EncOut * pEncOut)
{
    return EncStrmStartStatus;
}

MP4EncRet MP4EncStrmEncode(MP4EncInst inst, const MP4EncIn * pEncIn,
        MP4EncOut * pEncOut)
{
    pEncOut->vopType = pEncIn->vopType;

    return EncStrmEncodeStatus;
}

MP4EncRet MP4EncStrmEnd(MP4EncInst inst, MP4EncIn * pEncIn, MP4EncOut * pEncOut)
{
    return EncStrmEndStatus;
}

/* Pre processing */
MP4EncRet MP4EncSetPreProcessing(MP4EncInst inst,
        const MP4EncPreProcessingCfg * pPreProcCfg)
{
    return EncSetPreProcessingStatus;
}

MP4EncRet MP4EncGetPreProcessing(MP4EncInst inst,
        MP4EncPreProcessingCfg * pPreProcCfg)
{
    return EncGetPreProcessingStatus;
}

////////////////////////////////////////////////////////////
// test cases
////////////////////////////////////////////////////////////
void InitStatuses()
{
    EncInitStatus = ENC_INSTANCE_ERROR;
    EncReleaseStatus = ENC_INSTANCE_ERROR;
    EncSetCodingCtrlStatus = ENC_INSTANCE_ERROR;
    EncGetCodingCtrlStatus = ENC_INSTANCE_ERROR;
    EncSetRateCtrlStatus = ENC_INSTANCE_ERROR;
    EncGetRateCtrlStatus = ENC_INSTANCE_ERROR;
    EncSetPreProcessingStatus = ENC_INSTANCE_ERROR;
    EncGetPreProcessingStatus = ENC_INSTANCE_ERROR;
    EncStrmStartStatus = ENC_INSTANCE_ERROR;
    EncStrmEncodeStatus = ENC_INSTANCE_ERROR;
    EncStrmEndStatus = ENC_INSTANCE_ERROR;
    EncSetStreamInfoStatus = ENC_INSTANCE_ERROR;
    EncGetStreamInfoStatus = ENC_INSTANCE_ERROR;
}

//
// create fails
//
void test0()
{
    EncInitStatus = ENC_MEMORY_ERROR;
    MPEG4_CONFIG config;
    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_mpeg4(&config);
    BOOST_REQUIRE(i == NULL);
}

//
// create, decode, destroy
//
void test1()
{
    MPEG4_CONFIG config;
    
    config.nVideoRange = 0;

    config.mp4_config.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    config.mp4_config.eLevel = OMX_VIDEO_MPEG4Level2;

    // This will override any error control settings if set to OMX_TRUE
    // and set stream type to MP4_SVH_STR
    config.mp4_config.bSVH = OMX_FALSE;

    // If enabled will always set stream type to MP4_VP_DP_RVLC_STR
    config.mp4_config.bReversibleVLC = OMX_FALSE;
    config.mp4_config.bGov = OMX_FALSE;
    config.mp4_config.nPFrames = 0; // GOV interval
    config.mp4_config.nTimeIncRes = 30; // frmRateNum
    //config.mp4_config.nHeaderExtension = 0;
    config.mp4_config.nMaxPacketSize = 0;

    // the following used to set stream type if bSvh or bReversibleVLC are
    // not enabled.
    config.error_ctrl_config.bEnableDataPartitioning = OMX_FALSE;
    config.error_ctrl_config.bEnableResync = OMX_FALSE;
    config.error_ctrl_config.nResynchMarkerSpacing = 0;

    config.common_config.nOutputWidth = 112;
    config.common_config.nOutputHeight = 96;
    config.common_config.nInputFramerate = FLOAT_Q16(30);

    config.rate_config.eRateControl = OMX_Video_ControlRateVariable;
    config.rate_config.nPictureRcEnabled = 0; // default, enabled
    config.rate_config.nMbRcEnabled = 0; // default, enabled
    config.rate_config.nQpDefault = 1; // default
    config.rate_config.nQpMin = 1; // default
    config.rate_config.nQpMax = 31; // default
    config.rate_config.nVbvEnabled = 1; // default, enabled. Should be always 1 (ON)
    config.rate_config.nTargetBitrate = 0; // 128000

    config.pp_config.origWidth = 320;
    config.pp_config.origHeight = 244;
    config.pp_config.xOffset = 3;
    config.pp_config.yOffset = 6;
    config.pp_config.formatType = OMX_COLOR_FormatYUV420PackedSemiPlanar;
    config.pp_config.angle = 90;
    config.pp_config.frameStabilization = OMX_FALSE;

    EncInitStatus = ENC_OK;
    EncGetCodingCtrlStatus = ENC_OK;
    EncSetCodingCtrlStatus = ENC_OK;
    EncGetRateCtrlStatus = ENC_OK;
    EncSetRateCtrlStatus = ENC_OK;
    EncGetPreProcessingStatus = ENC_OK;
    EncSetPreProcessingStatus = ENC_OK;
    EncSetStreamInfoStatus = ENC_OK;
    EncGetStreamInfoStatus = ENC_OK;
    
    ENCODER_PROTOTYPE* i = HantroHwEncOmx_encoder_create_mpeg4(&config);
    BOOST_REQUIRE(i != NULL);

    STREAM_BUFFER stream;

    EncStrmStartStatus = ENC_OK;
    CODEC_STATE s;
    s = i->stream_start(i, &stream);
    BOOST_REQUIRE(s == CODEC_OK);

    EncStrmEncodeStatus = ENC_VOP_READY;
    FRAME buf;
    buf.frame_type = INTRA_FRAME;
    s = i->encode(i, &buf, &stream);
    BOOST_REQUIRE(s == CODEC_CODED_INTRA);

    buf.frame_type = PREDICTED_FRAME;
    EncStrmEncodeStatus = ENC_VOP_READY;
    s = i->encode(i, &buf, &stream);
    BOOST_REQUIRE(s == CODEC_CODED_PREDICTED);

    EncStrmEndStatus = ENC_OK;
    s = i->stream_end(i, &stream);
    BOOST_REQUIRE(s == CODEC_OK);

    EncReleaseStatus = ENC_OK;
    i->destroy(i);
}

int test_main(int, char* [])
{
    cout << "running " <<__FILE__ << " tests\n";

    //test0();
    test1();

    return 0;
}
