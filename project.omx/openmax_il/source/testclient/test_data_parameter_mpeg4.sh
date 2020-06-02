#!/bin/bash
#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Hantro Products Oy.                             --
#-                                                                            --
#-                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------
#-
#--  Abstract : Test script
#--
#-------------------------------------------------------------------------------
#--
#--  Version control information, please leave untouched.
#--
#--  $RCSfile: test_data_parameter_mpeg4.sh,v $
#--  $Date: 2009-11-30 14:03:37 $
#--  $Revision: 1.11 $
#--
#-------------------------------------------------------------------------------

RGB_SEQUENCE_HOME=${YUV_SEQUENCE_HOME}/../rgb

# Input YUV files
in1=${YUV_SEQUENCE_HOME}/synthetic/randomround_w96h96.yuv
in2=${YUV_SEQUENCE_HOME}/synthetic/nallikari_random_w1016h488.yuv
in3=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w640h480.yuv
in4=${YUV_SEQUENCE_HOME}/synthetic/black_and_white_w640h480.yuv
in5=${YUV_SEQUENCE_HOME}/synthetic/random.yuv
in6=${YUV_SEQUENCE_HOME}/vga/jaakiekko_25fps_vga.yuv
in7=${YUV_SEQUENCE_HOME}/synthetic/rlc_intra_w352h288.yuv
in8=${YUV_SEQUENCE_HOME}/synthetic/sikari_rlc_intra_w352h288.yuv
in9=${YUV_SEQUENCE_HOME}/synthetic/rlc_inter_w352h288.yuv
in10=${YUV_SEQUENCE_HOME}/synthetic/ruutu_w352h288.yuv
in11=${YUV_SEQUENCE_HOME}/vga/juna_25fps_vga.yuv
in12=${YUV_SEQUENCE_HOME}/misc/kuuba_maisema_25fps_w104h104.yuv
in13=${YUV_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.yuv
in14=${YUV_SEQUENCE_HOME}/synthetic/checkerboard_w640h480.yuv
in15=${YUV_SEQUENCE_HOME}/synthetic/headache_w1024h768.yuv
in16=${YUV_SEQUENCE_HOME}/synthetic/sikari_random_w128h96.yuv
in17=${YUV_SEQUENCE_HOME}/qcif/tractor_25fps_w176h144.yuv
in18=${YUV_SEQUENCE_HOME}/qcif/metro2_25fps_qcif.yuv
in19=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuv
in20=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuvsp
in21=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuyv422
in22=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.uyvy422
in23=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w356h292.yuv
in24=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w356h292.yuvsp
in25=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w356h292.yuyv422
in26=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w356h292.uyvy422
in27=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w360h296.yuv
in28=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w360h296.yuvsp
in29=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w360h296.yuyv422
in30=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w360h296.uyvy422
in31=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w364h300.yuv
in32=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w364h300.yuvsp
in33=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w364h300.yuyv422
in34=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w364h300.uyvy422
in35=${YUV_SEQUENCE_HOME}/synthetic/tractor_w720h576.yuv

in40=${YUV_SEQUENCE_HOME}/cif/torielamaa_25fps_cif.yuv
in41=${YUV_SEQUENCE_HOME}/cif/koski_25fps_cif.yuv
in42=${YUV_SEQUENCE_HOME}/cif/jaanmurtaja_cif_25fps.yuv
in43=${YUV_SEQUENCE_HOME}/cif/jaakiekko_25fps_cif.yuv
in44=${YUV_SEQUENCE_HOME}/cif/kelkka_25fps_cif.yuv

in50=${YUV_SEQUENCE_HOME}/vga/torielamaa_25fps_vga.yuv
in51=${YUV_SEQUENCE_HOME}/vga/koski_25fps_vga.yuv
in52=${YUV_SEQUENCE_HOME}/vga/nallikari_15fps_vga.yuv
in53=${YUV_SEQUENCE_HOME}/vga/jaakiekko_25fps_vga.yuv
in54=${YUV_SEQUENCE_HOME}/vga/kelkka_25fps_vga.yuv

in60=${YUV_SEQUENCE_HOME}/720p/stockholm_50fps_w1280h720.yuv
in61=${YUV_SEQUENCE_HOME}/720p/stockholm_50fps_w1280h720.yuvsp
in62=${YUV_SEQUENCE_HOME}/720p/stockholm_50fps_w1280h720.yuyv422
in63=${YUV_SEQUENCE_HOME}/720p/stockholm_50fps_w1280h720.uyvy422

in70=${YUV_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.yuv
in71=${YUV_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.yuvsp
in72=${YUV_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.yuyv422
in73=${YUV_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.uyvy422

in81=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuvsp
in82=${YUV_SEQUENCE_HOME}/synthetic/noisetest_w200h200.yuyv422
in83=${YUV_SEQUENCE_HOME}/synthetic/noisetest_w200h200.uyvy422
in84=${YUV_SEQUENCE_HOME}/synthetic/rangetest_w352h288.yuv
in85=${YUV_SEQUENCE_HOME}/cif/bus_stop_25fps_w352h288.yuv
in86=${YUV_SEQUENCE_HOME}/cif/run_w352h288.yuv
in87=${YUV_SEQUENCE_HOME}/vga/buildings_w640h480.yuv
in88=${YUV_SEQUENCE_HOME}/720p/shields_50fps_w1280h720.yuv
in89=${YUV_SEQUENCE_HOME}/synthetic/shields_motion_w352h288.yuv

in100=${YUV_SEQUENCE_HOME}/cif/metro_25fps_cif.yuv
# RGB input
in213=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.rgb565
in214=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.bgr565
in215=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.rgb555
in216=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.bgr555
in217=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.rgb444
in218=${RGB_SEQUENCE_HOME}/qcif/kuuba_maisema_25fps_qcif.bgr444
in250=${RGB_SEQUENCE_HOME}/vga/torielamaa_25fps_vga.rgb565
in270=${RGB_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.rgb565
in290=${RGB_SEQUENCE_HOME}/cif/shields_60fps_cif.rgb565
in291=${RGB_SEQUENCE_HOME}/vga/shields_60fps_vga.rgb565

# Performance cases
in900=${YUV_SEQUENCE_HOME}/qcif/stockholm_30fps_w176h144.yuv
in901=${YUV_SEQUENCE_HOME}/qcif/stockholm_30fps_w176h144.yuyv422
in902=${YUV_SEQUENCE_HOME}/cif/stockholm_30fps_w352h288.yuv
in903=${YUV_SEQUENCE_HOME}/cif/stockholm_30fps_w352h288.yuyv422
in904=${YUV_SEQUENCE_HOME}/vga/stockholm_30fps_w640h480.yuv
in905=${YUV_SEQUENCE_HOME}/vga/stockholm_30fps_w640h480.yuyv422
in906=${YUV_SEQUENCE_HOME}/d1/stockholm_30fps_w720h480.yuv
in907=${YUV_SEQUENCE_HOME}/d1/stockholm_30fps_w720h480.yuyv422
in909=${YUV_SEQUENCE_HOME}/cif/shields_60fps_cif.yuyv422
in910=${YUV_SEQUENCE_HOME}/vga/shields_60fps_vga.yuyv422

# User data
ud1=${USER_DATA_HOME}/userDataVos.txt
ud2=${USER_DATA_HOME}/userDataVisObj.txt
ud3=${USER_DATA_HOME}/userDataVol.txt
ud4=${USER_DATA_HOME}/userDataGov.txt
ud5=${USER_DATA_HOME}/null.txt

# Mpeg4 stream, do not change
out=stream.mpeg4

# Default parameters
colorConversion=( 0 0 0 0 0 )

case "$1" in
	0|1|2|3|4) {
	valid=(			0	0	0	0	-1	)
	input=(			$in1	$in16	$in16	$in1	$in2	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	40	0	)
	lastVop=(		127	32	32	50	10	)
	lumWidthSrc=(		96	128	128	96	1016	)
	lumHeightSrc=(		96	96	96	96	488	)
	width=(			96	128	128	96	160	)
	height=(		96	96	96	96	128	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	3	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	1	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	40	50	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	4	5	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	1	1	0	0	)
        category=("mpeg4_me_quant"
                  "mpeg4_me_quant"
                  "mpeg4_me_quant"
                  ""
                  "")
	desc=(	"Test ME and Hor. and ver. overfill, MVs mostly between [-32...30]"
		"Test ME, use all penalty values and quantization parameters"
		"Test ME, use all penalty values and quantization parameters"
		""
		"")
	};;

        150|151|152|153|154) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		0	0	1	1	2	)
	horOffsetSrc=(		0	1	2	3	4	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 0x0. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 4x2. Frame size 112x96. IF 0, rot. 0.")
	};;

        155|156|157|158|159) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		2	3	3	0	0	)
	horOffsetSrc=(		5	6	7	0	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 5x2. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 6x3. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 0, rot. 1.")
	};;

        160|161|162|163|164) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		1	1	2	2	3	)
	horOffsetSrc=(		2	3	4	5	6	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		1	1	1	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 2x1. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 4x2. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 6x3. Frame size 112x96. IF 0, rot. 1.")
	};;

        165|166|167|168|169) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		3	0	0	1	1	)
	horOffsetSrc=(		7	0	1	2	3	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		1	2	2	2	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 7x3. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 0, rot. 2.")
	};;

        170|171|172|173|174) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in20	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		2	2	3	3	0	)
	horOffsetSrc=(		4	5	6	7	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	1	)
	rotation=(		2	2	2	2	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 4x2. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 6x3. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 1, rot. 0.")
	};;


        175|176|177|178|179) {
	valid=(			0	0	0	0	0	)
	input=(			$in20	$in20	$in20	$in20	$in20	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		0	1	1	2	2	)
	horOffsetSrc=(		1	2	3	4	5	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	1	1	1	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 1x0. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 4x2. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 1, rot. 0.")
	};;

        180|181|182|183|184) {
	valid=(			0	0	0	0	0	)
	input=(			$in20	$in20	$in20	$in20	$in20	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		3	3	0	0	1	)
	horOffsetSrc=(		6	7	0	1	2	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	1	1	1	)
	rotation=(		0	0	1	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 6x3. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 1, rot. 0."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 1, rot. 1."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 1, rot. 1."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 1, rot. 1.")
	};;

        210|211|212|213|214) {
	valid=(			0	0	0	0	0	)
	input=(			$in21	$in21	$in21	$in21	$in21	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		2	2	3	3	0	)
	horOffsetSrc=(		4	5	6	7	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	2	2	2	)
	rotation=(		1	1	1	1	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 4x2. Frame size 112x96. IF 2, rot. 1."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 2, rot. 1."
		"Test cropping. Offset 6x3. Frame size 112x96. IF 2, rot. 1."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 2, rot. 1."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 2, rot. 2.")
	};;

        215|216|217|218|219) {
	valid=(			0	0	0	0	0	)
	input=(			$in21	$in21	$in21	$in21	$in21	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		0	1	1	2	2	)
	horOffsetSrc=(		1	2	3	4	5	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	2	2	2	)
	rotation=(		2	2	2	2	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 1x0. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 4x2. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 2, rot. 2.")
	};;

        220|221|222|223|224) {
	valid=(			0	0	0	0	0	)
	input=(			$in21	$in21	$in22	$in22	$in22	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		3	3	0	0	1	)
	horOffsetSrc=(		6	7	0	1	2	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	3	3	3	)
	rotation=(		2	2	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 6x3. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 2, rot. 2."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 3, rot. 0.")
	};;

        225|226|227|228|229) {
	valid=(			0	0	0	0	0	)
	input=(			$in22	$in22	$in22	$in22	$in22	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		1	2	2	3	3	)
	horOffsetSrc=(		3	4	5	6	7	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		3	3	3	3	3	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 3x1. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 4x2. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 5x2. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 6x3. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 7x3. Frame size 112x96. IF 3, rot. 0.")
	};;

        245|246|247|248|249) {
	valid=(			0	-1	-1	-1	-1	)
	input=(			$in22	$in22	$in22	$in22	$in22	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	verOffsetSrc=(		3	1	2	2	3	)
	horOffsetSrc=(		7	3	4	5	6	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		3	3	3	3	3	)
	rotation=(		2	2	2	2	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  ""
                  ""
                  ""
                  "")
	desc=(	"Test cropping. Offset 7x3. Frame size 112x96. IF 3, rot. 2."
		""
		""
		""
		"")
	};;

        250|251|252|253|254) {
	valid=(			0	0	0	0	0	)
	input=(			$in60	$in60	$in60	$in60	$in60	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		0	0	1	1	2	)
	horOffsetSrc=(		0	1	2	3	4	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 0x0. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 4x2. Frame size 720x576. IF 0, rot. 0.")
	};;

        255|256|257|258|259) {
	valid=(			0	0	0	0	0	)
	input=(			$in60	$in60	$in60	$in60	$in60	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		2	3	3	0	0	)
	horOffsetSrc=(		5	6	7	0	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 5x2. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 6x3. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 7x3. Frame size 720x576. IF 0, rot. 0."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 0, rot. 1."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 0, rot. 1.")
	};;

        275|276|277|278|279) {
	valid=(			0	0	0	0	0	)
	input=(			$in61	$in61	$in61	$in61	$in61	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		0	1	1	2	2	)
	horOffsetSrc=(		1	2	3	4	5	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	1	1	1	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 1x0. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 4x2. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 5x2. Frame size 720x576. IF 1, rot. 0.")
	};;

        280|281|282|283|284) {
	valid=(			0	0	0	0	0	)
	input=(			$in61	$in61	$in61	$in61	$in61	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		3	3	0	0	1	)
	horOffsetSrc=(		6	7	0	1	2	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	1	1	1	)
	rotation=(		0	0	1	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 6x3. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 7x3. Frame size 720x576. IF 1, rot. 0."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 1, rot. 1."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 1, rot. 1."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 1, rot. 1.")
	};;

        300|301|302|303|304) {
	valid=(			0	0	0	0	0	)
	input=(			$in62	$in62	$in62	$in62	$in62	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		1	1	2	2	3	)
	horOffsetSrc=(		2	3	4	5	6	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	2	2	2	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 2x1. Frame size 720x576. IF 2, rot. 0."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 2, rot. 0."
		"Test cropping. Offset 4x2. Frame size 720x576. IF 2, rot. 0."
		"Test cropping. Offset 5x2. Frame size 720x576. IF 2, rot. 0."
		"Test cropping. Offset 6x3. Frame size 720x576. IF 2, rot. 0.")
	};;

        305|306|307|308|309) {
	valid=(			0	0	0	0	0	)
	input=(			$in62	$in62	$in62	$in62	$in62	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		3	0	0	1	1	)
	horOffsetSrc=(		7	0	1	2	3	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	2	2	2	)
	rotation=(		0	1	1	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 7x3. Frame size 720x576. IF 2, rot. 0."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 2, rot. 1.")
	};;

        310|311|312|313|314) {
	valid=(			0	0	0	0	0	)
	input=(			$in62	$in62	$in62	$in62	$in62	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		2	2	3	3	0	)
	horOffsetSrc=(		4	5	6	7	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		2	2	2	2	2	)
	rotation=(		1	1	1	1	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 4x2. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 5x2. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 6x3. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 7x3. Frame size 720x576. IF 2, rot. 1."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 2, rot. 2.")
	};;

        330|331|332|333|334) {
	valid=(			0	0	0	0	0	)
	input=(			$in63	$in63	$in63	$in63	$in63	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		0	0	1	1	2	)
	horOffsetSrc=(		0	1	2	3	4	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		3	3	3	3	3	)
	rotation=(		1	1	1	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 0x0. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 4x2. Frame size 720x576. IF 3, rot. 1.")
	};;

        335|336|337|338|339) {
	valid=(			0	0	0	0	0	)
	input=(			$in63	$in63	$in63	$in63	$in63	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		2	2	2	2	2	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	verOffsetSrc=(		2	3	3	0	0	)
	horOffsetSrc=(		5	6	7	0	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		3	3	3	3	3	)
	rotation=(		1	1	1	2	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 5x2. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 6x3. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 7x3. Frame size 720x576. IF 3, rot. 1."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 3, rot. 2."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 3, rot. 2.")
	};;

        355|356|357|358|359) {
	valid=(			0	0	0	0	0	)
	input=(			$in70	$in70	$in70	$in70	$in70	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		1920	1920	1920	1920	1920	)
	lumHeightSrc=(		1080	1080	1080	1080	1080	)
	width=(			1280	1280	1280	1280	1280	)
	height=(		720	720	720	720	720	)
	verOffsetSrc=(		2	3	3	0	0	)
	horOffsetSrc=(		5	6	7	0	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		52	52	52	52	52	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	1	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping"
                  "mpeg4_cropping")
	desc=(	"Test cropping. Offset 5x2. Frame size 1280x720. IF 0, rot. 0."
		"Test cropping. Offset 6x3. Frame size 1280x720. IF 0, rot. 0."
		"Test cropping. Offset 7x3. Frame size 1280x720. IF 0, rot. 0."
		"Test cropping. Offset 0x0. Frame size 1280x720. IF 0, rot. 1."
		"Test cropping. Offset 1x0. Frame size 1280x720. IF 0, rot. 1.")
	};;


	450|451|452|453|454) {
	valid=(			0	0	0	0	-0	)
	input=(			$in1	$in16	$in3	$in3	$in3	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		127	32	5	5	5	)
	lumWidthSrc=(		96	128	640	640	640	)
	lumHeightSrc=(		96	96	480	480	480	)
	width=(			96	128	176	176	176	)
	height=(		96	96	144	144	144	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30000	30000	30000	30000	30000	)
	outputRateDenom=(	1001	1001	1001	1001	1001	)
	inputRateNumer=(	30000	30000	30000	30000	30000	)
	inputRateDenom=(	1001	1001	1001	1001	1001	)
	scheme=(		3	3	3	3	3	)
	profile=(		1005	1002	1005	1005	1005	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0   0   0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	10	10	10	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	1	2	)
	stabilization=(		0	0	0	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(	0  0	0	0	0	)
        category=("H263"
                  "H263"
                  "H263"
                  "H263"
                  "H263")
	desc=(	"Test motion estimation and MVs mostly between[-32...30]"
		"Test ME, use all penalty values and quantization parameters"
		"cropping"
		"cropping, rotation=1, stabilization"
		"cropping, rotation=2, stabilization")
	};;


	500|501|502|503|504) {
	valid=(			0	0	0	0	0	)
	input=(			$in3	$in18	$in60	$in60	$in13	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		25	30	30	30	30	)
	lumWidthSrc=(		640	176	1280	1280	176	)
	lumHeightSrc=(		480	144	720	720	144	)
	width=(			96	176	720	1280	176	)
	height=(		96	144	576	720	144	)
	horOffsetSrc=(		272	0	0	0	0	)
	verOffsetSrc=(		192	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	245	245	52	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		2	0	0	0	4	)
	goVopRate=(		0	0   0   0   0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		64000	512000	4000000	8000000	128000	)
	vopRc=(			1	1	1	1	1	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			15	15	15	15	15	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		6	0	0	0	0	)
        category=("mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control")
	desc=(	"Test rate control, checkpoint target and error min/max"
		"Test rate control, QCIF at 512 Kb/s"
		"Test rate control, D1 at 4 Mb/s"
		"Test rate control, 1280x576 at 8 Mb/s"
		"TEst rate control, initial QP and GOV")
	};;

        505|506|507|508|509) {
	valid=(			-1	0	0	0	-1	)
	input=(			$in6	$in10	$in6	$in10	$in70	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	1	)
	lumWidthSrc=(		640	352	640	352	1920	)
	lumHeightSrc=(		480	288	480	288	1080	)
	width=(			96	176	96	96	1280	)
	height=(		96	144	96	96	1024	)
	horOffsetSrc=(		0	0	0	100	0	)
	verOffsetSrc=(		0	0	0	100	0	)
	outputRateNumer=(	1	30	1	30000	1	)
	outputRateDenom=(	1	1	1	1001	1	)
	inputRateNumer=(	1	30	1	30000	1	)
	inputRateDenom=(	1	1	1	1001	1	)
	scheme=(		0	0	0	3	0	)
	profile=(		2	52	52	1007	52	)
	videoRange=(		0	1	0	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		2	2	0	2	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	80	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	1	0	0	)
	gobPlace=(		0	0	0	3	0	)
	bitPerSecond=(		100000	3000000	100000	100000	12000000	)
	vopRc=(			1	0	1	1	1	)
	mbRc=(			1	0	1	1	0	)
	videoBufferSize=(	10	10	10	10	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	9	10	31	-1	)
	qpMin=(			1	9	1	1	1	)
	qpMax=(			31	9	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "H263"
                  "mpeg4_rate_control")
	desc=(	"GO VOP rate =2. Test rate control with VBV, buffer overflow and underflow"
		"Test rate control with VBV, buffer overflow and underflow"
		"Test rate control with VBV, buffer overflow and underflow"
		"Test rate control with VBV, buffer overflow and underflow"
		"Test rate control initial QP overflow")
	};;

        510|511|512|513|514) {
	valid=(			-1	0	-1	0	0	)
	input=(			$in17	$in17	$in17	$in17	$in17	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		100	30	30	30	30	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	176	176	176	176	)
	height=(		144	144	144	144	144	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	1	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	1	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		52	3	3	3	3	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		10	30	30	30	30	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		12000000 100000 100000	100000	100000	)
	vopRc=(			1	1	0	1	1	)
	mbRc=(			0	0	0	1	1	)
	videoBufferSize=(	0	0	0	10	10	)
	vopSkip=(		0	1	1	1	1	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		-0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control"
                  "mpeg4_rate_control")
	desc=(	"Test rate control overflow"
		"Test rate control frame skip with frame RC"
		"Test rate control frame skip without frame RC"
		"Test rate control frame skip with VBV"
		"Test rate control frame skip without VBV")
	};;

600|601|602|603|604) {
	valid=(			0	0	0	0	0	)
	input=(			$in5	$in5	$in5	$in5	$in5	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		96	96	96	96	96	)
	lumHeightSrc=(		96	96	96	96	96	)
	width=(			96	96	96	96	96	)
	height=(		96	96	96	96	96	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	1	1	)
	rotation=(		0	1	2	0	1	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_preprocessing"
                  "mpeg4_preprocessing"
                  "mpeg4_preprocessing"
                  "mpeg4_preprocessing"
                  "mpeg4_preprocessing")
	desc=(	"Test sampling and rotation combinations. Frame size 96x96"
		"Test sampling and rotation combinations. Frame size 96x96"
		"Test sampling and rotation combinations. Frame size 96x96"
		"Test sampling and rotation combinations. Frame size 96x96"
		"Test sampling and rotation combinations. Frame size 96x96")
	};;



	710|711|712|713|714) {
	valid=(			0	0	0	0	0	)
	input=(			$in213	$in213	$in213	$in213	$in213	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	96	96	96	96	)
	height=(		144	96	96	96	96	)
	horOffsetSrc=(		0	1	2	3	4	)
	verOffsetSrc=(		0	0	1	1	2	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			5	5	5	5	5	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		4	4	4	4	4	)
	colorConversion=(	0	0	1	2	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
	category=("mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing")
	desc=(	"Test RGB input 4. Frame size 176x144, rot. 0, crop 0x0."
		"Test RGB input 4. Frame size 96x96, rot. 0, crop 1x0."
		"Test RGB input 4. Frame size 96x96, rot. 0, crop 2x1."
		"Test RGB input 4. Frame size 96x96, rot. 0, crop 3x1."
		"Test RGB input4. Frame size 96x96, rot. 0, crop 4x2.")
	};;

730|731|732|733|734) {
	valid=(			0	0	0	0	0	)
	input=(			$in213	$in213	$in213	$in213	$in214	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			96	96	96	96	176	)
	height=(		96	96	96	96	144	)
	horOffsetSrc=(		4	5	6	7	0	)
	verOffsetSrc=(		2	2	3	3	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			5	5	5	5	5	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		4	4	4	4	5	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		2	2	2	2	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
	category=("mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing")
	desc=(	"Test RGB input 4. Frame size 96x96, rot. 2, crop 4x2."
		"Test RGB input 4. Frame size 96x96, rot. 2, crop 5x2."
		"Test RGB input 4. Frame size 96x96, rot. 2, crop 6x3."
		"Test RGB input 4. Frame size 96x96, rot. 2, crop 7x3."
		"Test RGB input 5. QCIF BGR565 format")
	};;

	735|736|737|738|739) {
	valid=(			0	0	-1	0	0	)
	input=(			$in215	$in215	$in216	$in217	$in217	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			120	120	124	172	172	)
	height=(		100	100	104	140	140	)
	horOffsetSrc=(		1	1	1	1	1	)
	verOffsetSrc=(		3	3	3	3	3	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			5	5	5	5	5	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		6	6	7	8	8	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		0	1	0	0	2	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
	category=("mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing"
	          "mpeg4_preprocessing")
	desc=(	"(Test RGB input. Frame size 120x100, RGB555 format)"
		"Test RGB input. Frame size 120x100, RGB555 format, rotation"
		"Test RGB input. Frame size 124x104, BGR555 format"
		"Test RGB input 8. Frame size 172x140, RGB444 format"
		"Test RGB input 8. Frame size 172x140, RGB444 format, rotation")
	};;



	750|751|752|753|754) {
	valid=(			0	0	0	0	0	)
	input=(			$in84	$in84	$in84	$in84	$in81	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		100	30	30	30	30	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			336	112	112	112	112	)
	height=(		272	96	96	96	96	)
	horOffsetSrc=(		8	120	120	120	48	)
	verOffsetSrc=(		8	96	96	96	64	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	1	)
	rotation=(		0	0	1	2	0	)
	stabilization=(		1	1	1	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		12	12	12	12	12	)
        category=("mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization")
	desc=(	"Test stabilization. Frame size 336x272. IF 0, rot. 0."
		"Test stabilization. Frame size 112x96. IF 0, rot. 0."
		"Test stabilization. Frame size 112x96. IF 0, rot. 1."
		"Test stabilization. Frame size 112x96. IF 0, rot. 2."
		"Test stabilization semi-planar input. Frame size 112x96. IF 1, rot. 0.")
	};;

    750|751|752|753|754) {
	valid=(			0	0	0	0	0	)
	input=(			$in81	$in81	$in21	$in21	$in21	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	30	30	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	horOffsetSrc=(		44	44	44	44	44	)
	verOffsetSrc=(		52	52	52	52	52	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		3	3	3	3	3	)
	profile=(		1005	1005	1005	1005	1005	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	2	2	2	)
	rotation=(		1	2	0	1	2	)
	stabilization=(		1	1	1	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		12	12	12	12	12	)
        category=("mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization")
	desc=(	"Test stabilization, semi-planar. Frame size 112x96. IF 1, rot. 1."
		"Test stabilization, semi-planar. Frame size 112x96. IF 1, rot. 2."
		"Test stabilization. Frame size 112x96. IF 2, rot. 0."
		"Test stabilization. Frame size 112x96. IF 2, rot. 1."
		"Test stabilization. Frame size 112x96. IF 2, rot. 2.")
	};;

	755|756|757|758|759) {
	valid=(			0	0	0	0	0	)
	input=(			$in81	$in81	$in21	$in21	$in21	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	30	30	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	112	112	)
	height=(		96	96	96	96	96	)
	horOffsetSrc=(		44	44	44	44	44	)
	verOffsetSrc=(		52	52	52	52	52	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		1	1	2	2	2	)
	rotation=(		1	2	0	1	2	)
	stabilization=(		1	1	1	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		12	12	12	12	12	)
        category=("mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization")
	desc=(	"Test stabilization, semi-planar. Frame size 112x96. IF 1, rot. 1."
		"Test stabilization, semi-planar. Frame size 112x96. IF 1, rot. 2."
		"Test stabilization. Frame size 112x96. IF 2, rot. 0."
		"Test stabilization. Frame size 112x96. IF 2, rot. 1."
		"Test stabilization. Frame size 112x96. IF 2, rot. 2.")
	};;

	760|761|762|763|764) {
	valid=(			0	0	0	0	0	)
	input=(			$in22	$in22	$in22	$in84	$in85	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	85	40	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			112	112	112	320	320	)
	height=(		96	96	96	240	256	)
	horOffsetSrc=(		44	44	44	16	16	)
	verOffsetSrc=(		52	52	52	24	16	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	245	245	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		3	3	3	0	0	)
	rotation=(		0	1	2	0	0	)
	stabilization=(		1	1	1	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		12	12	12	12	12	)
        category=("mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization")
	desc=(	"Test stabilization. Frame size 112x96. IF 3, rot. 0."
		"Test stabilization. Frame size 112x96. IF 3, rot. 1."
		"Test stabilization. Frame size 112x96. IF 3, rot. 2."
		"Test stabilization. Frame size 320x240. IF 0, rot. 0."
		"Test stabilization. Frame size 320x256. IF 0, rot. 0.")
	};;

	765|766|767|768|769) {
	valid=(			0	0	0	0	0	)
	input=(			$in86	$in87	$in88	$in70	$in89	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		29	99	20	20	115	)
	lumWidthSrc=(		352	640	1280	1920	352	)
	lumHeightSrc=(		288	480	720	1080	288	)
	width=(			320	608	720	1280	320	)
	height=(		256	448	576	720	256	)
	horOffsetSrc=(		16	16	280	320	16	)
	verOffsetSrc=(		16	16	72	180	16	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	52	245	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	1	1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		1	1	1	1	1	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		12	12	12	12	12	)
        category=("mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization"
                  "mpeg4_stabilization")
	desc=(	"Test stabilization. Frame size 352x288. IF 0, rot. 0."
		"Test stabilization. Frame size 608x448. IF 0, rot. 0."
		"Test stabilization. Frame size 720x576. IF 0, rot. 0."
		"Test stabilization. Frame size 1280x720. IF 0, rot. 0."
		"Test stabilization. Frame size 352x288. All possible GMV")
	};;

	770|771|772|773|774) {
	valid=(			0	0	-1	0	0	)
	input=(			$in250	$in250	$in218	$in213	$in213	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	8	56	40	)
	lumWidthSrc=(		640	640	176	352	352	)
	lumHeightSrc=(		480	480	144	288	288	)
	width=(			608	608	160	320	320	)
	height=(		448	448	128	240	256	)
	horOffsetSrc=(		16	16	8	16	16	)
	verOffsetSrc=(		16	16	8	24	16	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			5	5	5	5	5	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		4	4	9	4	4	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		0	1	0	0	0	)
	stabilization=(		1	1	1	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
	category=("mpeg4_stabilization"
	          "mpeg4_stabilization"
	          "mpeg4_stabilization BGR555 - OMX UNSUPPORTED"
	          ""
	          "")
	desc=(	"Test stabilization. Frame size 608x448, IF 4. Rot. 0."
		"Test stabilization. Frame size 608x448, IF 4. Rot. 1."
		"Test stabilization. Frame size 160x128, IF 9. Rot. 0."
		""
		"")
	};;

	800|801|802|803|804) {
	valid=(			-1	0	0	-1	-1	)
	input=(			$in5	$in17	$in17	$in5	$in5	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		10	10	10	1	1	)
	lumWidthSrc=(		96	176	176	720	720	)
	lumHeightSrc=(		96	144	144	576	576	)
	width=(			96	176	176	720	720	)
	height=(		96	144	144	576	576	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		2	2	2	2	2	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		2	2	2	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	99	99	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			1	1	1	10	10	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		3	4	5	0	0	)
        category=("mpeg4_interrupt"
                  "mpeg4_interrupt"
                  "mpeg4_interrupt"
                  ""
                  "")
	desc=(	"Test IRQ. Exeeding Interrupt Interval."
		"Test IRQ. Exeeding Stream Buffer Limit."
		"Test IRQ. Exeeding Interrupt Interval and/or Stream Buffer Limit."
		""
		""	)
	};;

    880|881|882|883|884) {
	valid=(			0	0	0	0	0	)
	input=(			$in15	$in15	$in15	$in15	$in15	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		10	10	10	10	10	)
	lumWidthSrc=(		1024	1024	1024	1024	1024	)
	lumHeightSrc=(		768	768	768	768	768	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	horOffsetSrc=(		0	2	6	4	8	)
	verOffsetSrc=(		0	2	6	4	8	)
	outputRateNumer=(	30000	30000	30000	30000	30000	)
	outputRateDenom=(	1001	1001	1001	1001	1001	)
	inputRateNumer=(	30000	30000	30000	30000	30000	)
	inputRateDenom=(	1001	1001	1001	1001	1001	)
	scheme=(		3	3	3	3	3	)
	profile=(		1007	1007	1007	1007	1007	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		4194303	2796202	1198372	4194303	2796202	)
	bitPerSecond=(		5000000	5000000	5000000	5000000	5000000	)
	vopRc=(			1	1	1	1	1	)
	mbRc=(			1	1	1	1	1	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			15	15	15	15	15	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("H263"
                  "H263"
                  "H263"
                  "H263"
                  "H263")
	desc=(	"Test software error tools, rate control is on, parameters and quantization is tested comprehensively"
		"Test software error tools, rate control is on, parameters and quantization is tested comprehensively"
		"Test software error tools, rate control is on, parameters and quantization is tested comprehensively"
		"Test software error tools, rate control is on, parameters and quantization is tested comprehensively"
		"Test software error tools, rate control is on, parameters and quantization is tested comprehensively")
	};;

	890|891|892|893|894) {
	valid=(			0	-1	-1	-1	-1	)
	input=(			$in15	$in15	$in15	$in15	$in15	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		10	10	10	10	10	)
	lumWidthSrc=(		1024	1024	1024	1024	1024	)
	lumHeightSrc=(		768	768	768	768	768	)
	width=(			720	720	720	720	720	)
	height=(		480	480	480	480	480	)
	horOffsetSrc=(		304	0	0	0	0	)
	verOffsetSrc=(		288	0	0	0	0	)
	outputRateNumer=(	30000	30	30	30	30	)
	outputRateDenom=(	1001	1	1	1	1	)
	inputRateNumer=(	30000	30	30	30	30	)
	inputRateDenom=(	1001	1	1	1	1	)
	scheme=(		3	0	0	0	0	)
	profile=(		1007	245	245	245	245	)
	videoRange=(		1	1	1	1	1	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		2796202	0	0	0	0	)
	bitPerSecond=(		8000000	0	0	0	0	)
	vopRc=(			1	0	0	0	0	)
	mbRc=(			1	0	0	0	0	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			15	15	15	15	15	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("H263"
                  ""
                  ""
                  ""
                  "")
	desc=(	"Test software error tools, rate control is on, parameters and quantization is tested comprehensively"
		""
		""
		""
		"")
	};;

	980|981|982|983|984) {
	valid=(			-1	-1	-1	-1	-1	)
	input=(			$in50	$in70	$in53	$in43	$in906	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		100	0	0	0	0	)
	lastVop=(		12	20	10	10	10	)
	lumWidthSrc=(		640	1920	640	352	720	)
	lumHeightSrc=(		480	1080	480	288	480	)
	width=(			400	1280	640	352	720	)
	height=(		240	1024	480	288	480	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		4	52	0	0	0	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		10	25	0	0	0	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		0	0	0	0	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		150000	5000000	512000	358000	1000000	)
	vopRc=(			1	1	1	1	1	)
	mbRc=(			1	1	1	1	1	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			-1	-1	-1	-1	-1	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
	category=("mpeg4_custom_framesize"
	          "mpeg4_custom_framesize"
	          ""
	          ""
	          "")
	desc=(	"Testing MPEG-4 frame size 400x240."
		"Testing MPEG-4 frame size 1280x1024."
		""
		""
		"")
	};;

                        990|    991|    992|    993|    994) {
	valid=(			    0	    0	    0	    0	    0	)
	input=(			    $in100	$in18	$in84	$in22	$in100	)
	output=(		    $out    $out	$out	$out	$out	)
	firstVop=(		    0	    0	    0	    0	    0	)
	lastVop=(		    10	    30	    100	    10	    5	)
	lumWidthSrc=(	    352	    176	    352	    352	    352	)
	lumHeightSrc=(	    288	    144	    288	    288	    288	)
	width=(			    112	    176	    336	    112	    96	)
	height=(		    96	    144	    272	    96	    96	)
	horOffsetSrc=(		0	    0	    8	    0	    100	)
	verOffsetSrc=(		1	    0	    8	    0	    100	)
	outputRateNumer=(	30	    30	    30	    30	    30000)
	outputRateDenom=(	1	    1	    1	    1		1001)
	inputRateNumer=(	30	    30	    30	    30	    30000)
	inputRateDenom=(	1		1	    1	    1	    1001)
	scheme=(		    0		0	    0		0	    3	)
	profile=(		    2		3	    2		2	    1007)
	videoRange=(		1		0	    0	    1	    1	)
	intraDcVlcThr=(		0		0	    0	    0	    0	)
	acPred=(		    0		0	    0		0	    0	)
	intraVopRate=(		0		30	    0   	0	    2	)
	goVopRate=(		    0		0	    0		0	    0	)
	cir=(			    0		0	    0		0	    0	)
	vpSize=(		    0		0	    0		0	    0	)
	dataPart=(		    0		0	    0		0	    0	)
	rvlc=(			    0		0	    0		0	    0	)
	hec=(			    0		0	    0		0	    0	)
	gobPlace=(		    0		0	    0		0	    3	)
	bitPerSecond=(      0		100000  0	    0		100000	)
	vopRc=(			    0		1	    0		0	    1	)
	mbRc=(			    0		0	    0		0	    1	)
	videoBufferSize=(	0		0	    0	    0	    10	)
	vopSkip=(		    0		1	    0	    0	    0	)
	qpHdr=(			    1		10	    1	    1	    31	)
	qpMin=(			    1		1	    1	    1	    1	)
	qpMax=(			    31	    31	    31	    31	    31	)
	inputFormat=(		0	    0	    0	    3	    0	)
	rotation=(		    1	    0	    0	    0	    0	)
	stabilization=(		0	    0	    1	    0	    0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_cropping"
                  "mpeg4_rate_control"
                  "mpeg4 stabilization"
                  "mpeg4 Input uyvy422 (interleaved)"
                  "H263")
	desc=("Test cropping. Offset 1x0. Frame size 112x96. IF 0, rotation +90."
		"Test rate control frame skip with frame RC"
		"Test stabilization. Frame size 336x272. IF 0, rot. 0."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 3, rot. 0."
		"Test rate control")
	};;




	4400|4401|4402|4403|4404) {
	valid=(			-1	-1	-1	-1	-1	)
	input=(			$in50	$in51	$in52	$in53	$in54	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		640	650	390	370	660	)
	lumWidthSrc=(		640	640	640	640	640	)
	lumHeightSrc=(		480	480	480	480	480	)
	width=(			640	640	640	640	640	)
	height=(		480	480	480	480	480	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	scheme=(		0	0	0	0	0	)
	profile=(		245	245	245	245	245	)
	videoRange=(		0	0	0	0	0	)
	intraDcVlcThr=(		0	0	0	0	0	)
	acPred=(		0	0	0	0	0	)
	intraVopRate=(		30	30	30	30	30	)
	goVopRate=(		0	0	0	0	0	)
	cir=(			0	0	0	0	0	)
	vpSize=(		5000	0	0	5000	0	)
	dataPart=(		0	0	0	0	0	)
	rvlc=(			0	0	0	0	0	)
	hec=(			0	0	0	0	0	)
	gobPlace=(		0	0	0	0	0	)
	bitPerSecond=(		1500000	2000000	2000000	2000000	2000000	)
	vopRc=(			1	1	1	1	1	)
	mbRc=(			1	1	1	1	1	)
	videoBufferSize=(	0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			15	15	15	15	15	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			31	31	31	31	31	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	userDataVos=(		$ud1	$ud1	$ud1	$ud1	$ud1	)
	userDataVisObj=(	$ud2	$ud2	$ud2	$ud2	$ud2	)
	userDataVol=(		$ud3	$ud3	$ud3	$ud3	$ud3	)
	userDataGov=(		$ud4	$ud4	$ud4	$ud4	$ud4	)
	testId=(		0	0	0	0	0	)
        category=("mpeg4_lab"
                  "mpeg4_lab"
                  "mpeg4_lab"
                  "mpeg4_lab"
                  "mpeg4_lab")
	desc=(	"Lab test, normal videosequence with hundreds frames"
		"Lab test, normal videosequence with hundreds frames."
		"Lab test, normal videosequence with hundreds frames."
		"Lab test, normal videosequence with hundreds frames"
		"Lab test, normal videosequence with hundreds frames")
	};;


	* )
	valid=(			-1	-1	-1	-1	-1	);;
esac
