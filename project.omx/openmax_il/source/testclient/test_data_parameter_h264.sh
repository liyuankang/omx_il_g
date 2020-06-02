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
#--  $RCSfile: test_data_parameter_h264.sh,v $
#--  $Date: 2010-10-05 05:26:27 $
#--  $Revision: 1.12 $
#--
#-------------------------------------------------------------------------------

RGB_SEQUENCE_HOME=${YUV_SEQUENCE_HOME}/../rgb

# Input YUV files
in1=${YUV_SEQUENCE_HOME}/synthetic/randomround_w96h96.yuv
in2=${YUV_SEQUENCE_HOME}/synthetic/nallikari_random_w1016h488.yuv
in3=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w640h480.yuv
in4=${YUV_SEQUENCE_HOME}/synthetic/black_and_white_w1920h1080.yuv
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
in74=${YUV_SEQUENCE_HOME}/1080p/riverbed_25fps_w1920h1080.yuv
in75=${YUV_SEQUENCE_HOME}/1080p/intersection_30fps_w1920h1036.yuv
in76=${YUV_SEQUENCE_HOME}/1080p/station2_25fps_w1920h1080.yuv
in77=${YUV_SEQUENCE_HOME}/synthetic/shields_static_w1920h1080.yuv

in81=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuvsp
in82=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.yuyv422
in83=${YUV_SEQUENCE_HOME}/synthetic/noisetest_w200h200.uyvy422
in84=${YUV_SEQUENCE_HOME}/synthetic/rangetest_w352h288.yuv
in85=${YUV_SEQUENCE_HOME}/cif/bus_stop_25fps_w352h288.yuv
in86=${YUV_SEQUENCE_HOME}/cif/run_w352h288.yuv
in87=${YUV_SEQUENCE_HOME}/vga/buildings_w640h480.yuv
in88=${YUV_SEQUENCE_HOME}/720p/shields_50fps_w1280h720.yuv
in89=${YUV_SEQUENCE_HOME}/synthetic/shields_motion_w352h288.yuv

in90=${YUV_SEQUENCE_HOME}/qcif/monitor_qcif.yuv
in91=${YUV_SEQUENCE_HOME}/4cif/city_30fps_4cif.yuv
in92=${YUV_SEQUENCE_HOME}/misc/sunflower_25fps_w2304h1296.yuv
in93=${YUV_SEQUENCE_HOME}/misc/gradient_w4096h2048.yuyv422
in94=${YUV_SEQUENCE_HOME}/qcif/ice_30fps_w176h144.yuv
in95=${YUV_SEQUENCE_HOME}/misc/mosaic_w8176h8176.yuv

# Intra release case
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
in271=${RGB_SEQUENCE_HOME}/vga/shields_60fps_vga.rgb888
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

# Dummy
in999=/dev/zero

# H.264 stream name, do not change
out=stream.h264

# Default parameters
colorConversion=( 0 0 0 0 0 )

case "$1" in
	1000|1001|1002|1003|1004) {
	valid=(			-1	-1	-1	-1	-1	)
	input=(			$in4	$in4	$in4	$in4	$in13	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	30	14	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	176	176	176	96	)
	height=(		144	144	144	144	128	)
	horOffsetSrc=(		0	0	0	0	32	)
	verOffsetSrc=(		0	0	0	0	16	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		1	0	1	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	12	)
	bitPerSecond=(		64000	64000	64000	64000	128000	)
	vopRc=(			0	0	0	0	1	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			25	25	25	10	30	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	0	)
	sei=(			0	0	0	0	0	)
	testId=(		0	1	1	0	0	)
        category=("h264_me"
                  "h264_me"
                  "h264_intra"
                  "h264_me"
                  "h264_stream")
	desc=(	"Test ME and overfill. MV x and y component shall have values [-64, 60]."
		"Test ME, IPE and mode decision using Qp adaptive favor values. Qp is increased [0,...,51]."
		"As previous, but with intraVopRate = 1."
		""
		"OMX-not supported (Nal Unit Stream without Annex B stuff.)")
	};;

	1010|1011|1012|1013|1014) {
	valid=(			0	0	-1	-1	-1	)
	input=(			$in100	$in100	$in16	$in4	$in13	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		39	30	30	30	30	)
	lumWidthSrc=(		352	352	128	176	176	)
	lumHeightSrc=(		288	288	96	144	144	)
	width=(			352	288	128	176	176	)
	height=(		288	224	96	144	144	)
	horOffsetSrc=(		0	64	0	0	0	)
	verOffsetSrc=(		0	32	0	0	0	)
	outputRateNumer=(	25	25	30	30	25	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	25	25	30	30	25	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		1	1	1	0	6	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	11	)
	bitPerSecond=(		384000	768000	0	0	256000	)
	vopRc=(			1	1	0	0	1	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			51	51	0	10	36	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	30	)
	hrdConformance=(	0	0	0	0	1	)
	cpbSize=(		384000	768000	0	0	30000	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	1	)
	testId=(		0	0	1	0	0	)
        category=("h264_intra"
                  "h264_intra"
                  "h264_intra"
                  "h264_intra"
                  "h264_hrd")
	desc=(	"H.264 Intra release test. Size 352x288, begins with QP 51. VopRc, 384 Kbps."
		"H.264 Intra release test. Size 288x224, begins with QP 51. VopRc, 768 Kbps."
		""
		""
		""
		"Unsupported for OMX rate control types(H.264 HRD testing. Causing frame skips. SEI enabled.)"	)
	};;

	1100|1101|1102|1103|1104) {
	valid=(			0	0	-1	-1	-1	)
	input=(			$in17	$in75	$in22	$in23	$in23	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		20	7	15	15	15	)
	lumWidthSrc=(		176	1920	704	704	704	)
	lumHeightSrc=(		144	1036	576	576	576	)
	width=(			176	1280	704	704	704	)
	height=(		144	1024	576	576	576	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	1	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	32	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		2	2	0	0	0	)
        category=("h264_slice"
                  "h264_slice"
                  "h264_slice"
                  "h264_slice"
                  "h264_slice")
	desc=(	"Test arbitary slice sizes with a qcif frame."
		"Test arbitary slice sizes with maximum resolution."
		""
		""
		""	)
	};;

	1155|1156|1157|1158|1159) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			132	132	132	132	132	)
	height=(		96	96	96	132	132	)
	horOffsetSrc=(		5	6	7	0	1	)
	verOffsetSrc=(		2	3	3	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	1	1	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping")
	desc=(	"Test cropping. Offset 5x2. Frame size 132x96. IF 0, rot. 0."
		"Test cropping. Offset 6x3. Frame size 132x96. IF 0, rot. 0."
		"Test cropping. Offset 7x3. Frame size 132x96. IF 0, rot. 0."
		"Test cropping. Offset 0x0. Frame size 132x132. IF 0, rot. 1."
		"Test cropping. Offset 1x0. Frame size 132x132. IF 0, rot. 1.")
	};;

	1165|1166|1167|1168|1169) {
	valid=(			0	0	0	0	0	)
	input=(			$in19	$in19	$in19	$in19	$in19	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			132	132	132	132	132	)
	height=(		132	132	132	132	132	)
	horOffsetSrc=(		7	0	1	2	3	)
	verOffsetSrc=(		3	0	0	1	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		1	2	2	2	2	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping")
	desc=(	"Test cropping. Offset 7x3. Frame size 112x96. IF 0, rot. 1."
		"Test cropping. Offset 0x0. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 1x0. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 2x1. Frame size 112x96. IF 0, rot. 2."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 0, rot. 2.")
	};;

	1225|1226|1227|1228|1229) {
	valid=(			0	0	0	0	0	)
	input=(			$in22	$in22	$in22	$in22	$in22	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			132	132	132	132	132	)
	height=(		96	96	96	96	96	)
	horOffsetSrc=(		3	4	5	6	7	)
	verOffsetSrc=(		1	2	2	3	3	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		3	3	3	3	3	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping")
	desc=(	"Test cropping. Offset 3x1. Frame size 112x96. IF 3, rot. 0."
		"Test cropping. Offset 4x2. Frame size 132x96. IF 3, rot. 0."
		"Test cropping. Offset 5x2. Frame size 132x96. IF 3, rot. 0."
		"Test cropping. Offset 6x3. Frame size 132x96. IF 3, rot. 0."
		"Test cropping. Offset 7x3. Frame size 132x96. IF 3, rot. 0.")
	};;


	1265|1266|1267|1268|1269) {
	valid=(			0	0	0	0	0	)
	input=(			$in60	$in60	$in60	$in60	$in60	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		3	3	3	3	3	)
	lumWidthSrc=(		1280	1280	1280	1280	1280	)
	lumHeightSrc=(		720	720	720	720	720	)
	width=(			720	720	720	720	720	)
	height=(		576	576	576	576	576	)
	horOffsetSrc=(		7	0	1	2	3	)
	verOffsetSrc=(		3	0	0	1	1	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		1	2	2	2	2	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping"
                  "h264_cropping")
	desc=(	"Test cropping. Offset 7x3. Frame size 720x576. IF 0, rot. 1."
		"Test cropping. Offset 0x0. Frame size 720x576. IF 0, rot. 2."
		"Test cropping. Offset 1x0. Frame size 720x576. IF 0, rot. 2."
		"Test cropping. Offset 2x1. Frame size 720x576. IF 0, rot. 2."
		"Test cropping. Offset 3x1. Frame size 720x576. IF 0, rot. 2.")
	};;

	1445|1446|1447|1448|1449) {
	valid=(			    0	    0	-1	-1	-1	)
	input=(			    $in73	$in73	$in73	$in73	$in73	)
	output=(		    $out	$out	$out	$out	$out	)
	firstVop=(		    0	    0	0	0	0	)
	lastVop=(		    1	    1	1	1	1	)
	lumWidthSrc=(	    1920	1920	1920	1920	1920	)
	lumHeightSrc=(	    1080	1080	1080	1080	1080	)
	width=(			    1280	1920	1280	1280	1280	)
	height=(		    720	    1080	720	720	720	)
	horOffsetSrc=(		7	    0	4	5	6	)
	verOffsetSrc=(		3	    0	2	2	3	)
	outputRateNumer=(	30	    30	30	30	30	)
	outputRateDenom=(	1	    1	1	1	1	)
	inputRateNumer=(	30	    30	30	30	30	)
	inputRateDenom=(	1	    1	1	1	1	)
	constIntraPred=(	0	    0	0	0	0	)
	disableDeblocking=(	0	    0	0	0	0	)
	filterOffsetA=(		0	    0	0	0	0	)
	filterOffsetB=(		0	    0	0	0	0	)
	intraVopRate=(		0	    0	0	0	0	)
	mvRange=(		    0	    0   0	0	0	)
	mbPerSlice=(		0	    0	0	0	0	)
	bitPerSecond=(		0	    0	0	0	0	)
	vopRc=(			    0	    0	0	0	0	)
	vopSkip=(		    0	    0	0	0	0	)
	qpHdr=(			    10	    10	10	10	10	)
	qpMin=(			    0	    0	0	0	0	)
	qpMax=(			    51	    51	51	51	51	)
	level=(			    31	    41	31	31	31	)
	hrdConformance=(	0	    0	0	0	0	)
	cpbSize=(		    0	    0	0	0	0	)
	mbRc=(			    0	    0	0	0	0	)
	chromaQpOffset=(	0	    0	0	0	0	)
	inputFormat=(		3	    3	3	3	3	)
	rotation=(		    2	    0	2	2	2	)
	stabilization=(		0	    0	0	0	0	)
	byteStream=(		1	    1	1	1	1	)
	sei=(			    0	    0	0	0	0	)
	testId=(		    0	    0	0	0	0	)
        category=("h264_cropping"
                  ""
                  ""
                  ""
                  "")
	desc=(	"Test cropping. Offset 7x3. Frame size 1280x720. IF 3, rot. 2."
		""
		""
		""
		"")
	};;


	1500|1501|1502|1503|1504) {
	valid=(			0	0	0	0	0	)
	input=(			$in3	$in18	$in60	$in60	$in13	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		25	30	30	30	30	)
	lumWidthSrc=(		640	176	1280	1280	176	)
	lumHeightSrc=(		480	144	720	720	144	)
	width=(			132	176	720	1280	176	)
	height=(		96	144	576	720	144	)
	horOffsetSrc=(		272	0	0	0	0	)
	verOffsetSrc=(		192	0	0	0	0	)
	outputRateNumer=(	30	30	30	25	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	25	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	4	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		64000	512000	4000000	8000000	128000	)
	vopRc=(			1	1	1	1	1	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			30	25	25	25	25	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	31	0	)
	hrdConformance=(	1	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			1	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		6	0	0	0	0	)
        category=("h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control")
	desc=(	"Test rate control. Forces Qp change between [qpMin,...,qpMax] "
		"Test rate control. Normal operation with Qcif @ 512 Kbps."
		"Test rate control. Normal operation with D1 @ 4 Mbps."
		"Test rate control. Normal operation with 1280x720 @ 8 Mbps."
		"Test rate control. Intra rate")
	};;

	1505|1506|1507|1508|1509) {
	valid=(			0	0	0	0	0	)
	input=(			$in6	$in10	$in6	$in90	$in91	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		20	20	20	315	5	)
	lumWidthSrc=(		640	352	640	176	704	)
	lumHeightSrc=(		480	288	480	144	576	)
	width=(			132	176	132	176	704	)
	height=(		96	144	96	144	576	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	1	30	30	25	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	1	30	30	25	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		2	0	5	0	30	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		76800	153800	100000	64000	700000	)
	vopRc=(			1	1	1	1	1	)
	vopSkip=(		0	1	0	0	0	)
	qpHdr=(			10	10	10	25	30	)
	qpMin=(			9	10	1	0	0	)
	qpMax=(			51	10	51	51	51	)
	level=(			10	11	11	11	0	)
	hrdConformance=(	1	1	1	1	1	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			1	1	1	1	1	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control")
	desc=(	"Test h264 rate control with HRD"
		"Test h264 rate control with HRD overflow and underflow"
		"Test h264 rate control with HRD overflow and underflow"
		"Test h264 rate control problem situations"
		"Test h264 rate control problem situations"	)
	};;

        1515|1516|1517|1518|1519) {
	valid=(			0	0	0	0	0	)
	input=(			$in17	$in17	$in17	$in17	$in17	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	25	25	25	25	)
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
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		10	30	30	30	30	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		12000000 100000 100000	100000	100000	)
	vopRc=(			1	1	1	1	1	)
	vopSkip=(		0	1	1	1	1	)
	qpHdr=(			20	36	36	36	36	)
	qpMin=(			1	1	1	1	1	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	1	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	1	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control"
                  "h264_rate_control")
	desc=(	"Test rate control overflow"
		"Test rate control frame skip with frame RC"
		"Test rate control frame skip without frame RC"
   	"Test rate control frame skip with VBV"
   	"Test rate control frame skip without VBV")
	};;

	1600|1601|1602|1603|1604) {
	valid=(			0	0	0	0	0	)
	input=(			$in4	$in4	$in4	$in4	$in4	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	176	176	176	176	)
	height=(		144	144	144	144	144	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	1	1	)
	rotation=(		0	1	2	0	1	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing")
	desc=(	"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144.")
	};;

	1605|1606|1607|1608|1609) {
	valid=(			0	0	0	0	0	)
	input=(			$in4	$in4	$in4	$in4	$in4	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	176	176	176	176	)
	height=(		144	144	144	144	144	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		1	2	2	2	3	)
	rotation=(		2	0	1	2	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing")
	desc=(	"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144.")
	};;

	1610|1611|1612|1613|1614) {
	valid=(			0	0	0	0	0	)
	input=(			$in4	$in4	$in5	$in5	$in5	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		1	1	1	1	1	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			176	176	176	176	176	)
	height=(		144	144	144	144	144	)
	horOffsetSrc=(		0	0	0	0	0	)
	verOffsetSrc=(		0	0	0	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			10	10	10	10	10	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		3	3	0	0	0	)
	rotation=(		1	2	0	1	2	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
        category=("pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing"
                  "pre_processing")
	desc=(	"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size 176x144."
		"Test sampling and rotation combinations. Frame size QCIF."
		"Test sampling and rotation combinations. Frame size QCIF."
		"Test sampling and rotation combinations. Frame size QCIF.")
	};;

	1695|1696|1697|1698|1699) {
    valid=( -1	-1	-1	-1	0	)
    input=(	$in34	$in93	$in93	$in93	$in95	)
    output=(	$out    $out    $out    $out    $out    )
    firstVop=(              0       0       0       0       0       )
    lastVop=(               1       0       0       0       0       )
    lumWidthSrc=(           364     4096    4096    4096    8176    )
    lumHeightSrc=(          300     2048    2048    2048    8176    )
    width=(                 364     1920    4080    4080    4080    )
    height=(                300     1080    1080    1080    4080    )
    horOffsetSrc=(		    0	    0	    0	    0	    0	    )
	verOffsetSrc=(		    0	    0	    0	    0	    0	    )
    outputRateNumer=(       30      2       2       2       1       )
    outputRateDenom=(       1       1       1       1       1       )
    inputRateNumer=(        30      1       1       1       1       )
    inputRateDenom=(        1       2       2       2       1       )
    constIntraPred=(	    0	0	0	0	0	)
	disableDeblocking=(	    0	0	0	0	0	)
	filterOffsetA=(		    0	0	0	0	0	)
	filterOffsetB=(		    0	0	0	0	0	)
	intraVopRate=(		    0	0	0	0	0	)
	mvRange=(		        0	    0	0	0	0	)
	mbPerSlice=(            0       0       0       0       254     )
	bitPerSecond=(		    0	0	0	0	0	)
	vopRc=(			        0	0	0	0	0	)
	vopSkip=(		        0	0	0	0	0	)
    qpHdr=(                 10      10      10      10      25      )
    qpMin=(			        0	0	0	0	0	)
	qpMax=(			        51	51	51	51	51	)
	level=(                 0       40      51      51      51      )
	hrdConformance=(	    0	0	0	0	0	)
	cpbSize=(		        0	0	0	0	0	)
	mbRc=(			        0	0	0	0	0	)
	chromaQpOffset=(	    0	0	0	0	0	)
    inputFormat=(           3       2       2       2       0       )
    rotation=(              2       0       1       2       0       )
    stabilization=(		    0	0	0	0	0	)
	byteStream=(		    1	1	1	1	1	)
	sei=(			        0	0	0	0	0	)
	testId=(		        0	0	0	0	0	)
    category=("pre_processing"
              "pre_processing"
              "pre_processing"
              "pre_processing"
              "pre_processing")
    desc=(  "Sampling and rotation, UYVY422, rot=2. Hor. and ver. overfill 4 pixels."
            "Sampling and rotation, YUYV422, rot=0, input 4096x2048"
            "Sampling and rotation, YUYV422, rot=1, input 4096x2048"
            "Sampling and rotation, YUYV422, rot=2, input 4096x2048"
            "Sampling and rotation, YUV420, max input 8176x8176, max enc 4080x4080")
        };;

	1710|1711|1712|1713|1714) {
	valid=(			0	0	0	0	0	)
	input=(			$in213	$in213	$in213	$in213	$in213	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			132	132	132	132	132	)
	height=(		96	96	96	96	96	)
	horOffsetSrc=(		0	1	2	3	4	)
	verOffsetSrc=(		0	0	1	1	2	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		4	4	4	4	4	)
	colorConversion=(	0	0	1	2	0	)
	rotation=(		0	0	0	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
	category=("pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing")
	desc=(	"Test RGB input. Frame size 96x96, rot. 0, crop 0x0."
		"Test RGB input. Frame size 96x96, rot. 0, crop 1x0."
		"Test RGB input. Frame size 96x96, rot. 0, crop 2x1."
		"Test RGB input. Frame size 96x96, rot. 0, crop 3x1."
		"Test RGB input. Frame size 96x96, rot. 0, crop 4x2.")
	};;

	1730|1731|1732|1733|1734) {
	valid=(			0	0	0	0	0	)
	input=(			$in213	$in213	$in213	$in213	$in214	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			132	132	132	132	176	)
	height=(		132	132	132	132	144	)
	horOffsetSrc=(		4	5	6	7	0	)
	verOffsetSrc=(		2	2	3	3	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		4	4	4	4	5	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		2	2	2	2	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
	category=("pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing")
	desc=(	"Test RGB input. Frame size 96x96, rot. 2, crop 4x2."
		"Test RGB input. Frame size 96x96, rot. 2, crop 5x2."
		"Test RGB input. Frame size 96x96, rot. 2, crop 6x3."
		"Test RGB input. Frame size 96x96, rot. 2, crop 7x3."
		"Test RGB input. QCIF BGR565 format")
	};;

	1735|1736|1737|1738|1739) {
	valid=(			0	0	-1	0	0	)
	input=(			$in215	$in215	$in216	$in217	$in217	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	5	5	)
	lumWidthSrc=(		176	176	176	176	176	)
	lumHeightSrc=(		144	144	144	144	144	)
	width=(			132	132	124	172	172	)
	height=(		100	132	104	140	140	)
	horOffsetSrc=(		1	1	1	1	1	)
	verOffsetSrc=(		3	3	3	3	3	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		6	6	7	8	8	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		0	1	0	0	2	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
	category=("pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing")
	desc=(	"Test RGB input. Frame size 120x100, RGB555 format"
		"Test RGB input. Frame size 120x100, RGB555 format, rotation"
		"Test RGB input. Frame size 124x104, BGR555 format"
		"Test RGB input. Frame size 172x140, RGB444 format"
		"Test RGB input. Frame size 172x140, RGB444 format, rotation")
	};;

	1740|1741|1742|1743|1744) {
	valid=(			-1	0	0	0	-1	)
	input=(			$in218	$in270	$in270	$in250	$in290	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		5	5	5	29	29	)
	lumWidthSrc=(		176	1920	1920	640	352	)
	lumHeightSrc=(		144	1080	1080	480	288	)
	width=(			176	1280	1280	132	352	)
	height=(		144	1024	1024	96	288	)
	horOffsetSrc=(		0	19	19	0	0	)
	verOffsetSrc=(		0	17	17	0	0	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	30	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	384000	)
	vopRc=(			0	0	0	0	1	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	32	32	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	10	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		9	4	4	4	4	)
	colorConversion=(	0	0	0	0	0	)
	rotation=(		0	0	1	0	0	)
	stabilization=(		0	0	0	0	0	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	19	0	)
	category=("pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "pre_processing"
	          "")
	desc=(	"OMX unsupported: BRG444 (Test RGB input. Frame size QCIF, BGR444 format)"
		"Test RGB input. Max Frame size SXGA, rot. 0, crop 19x17."
		"Test RGB input. Max Frame size SXGA, rot. 1, crop 19x17."
		"Test RGB input. All RGB mask positions"
		"")
	};;

		1750|1751|1752|1753|1754) {
	valid=(			0	0	0	0	0	)
	input=(			$in84	$in84	$in84	$in84	$in81	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	30	30	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			336	132	132	132	132	)
	height=(		272	96	132	132	96	)
	horOffsetSrc=(		8	120	120	120	44	)
	verOffsetSrc=(		8	96	96	96	52	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		0	0	0	0	1	)
	rotation=(		0	0	1	2	0	)
	stabilization=(		1	1	1	1	1	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
	category=("h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization")
	desc=(	"Test stabilization. Frame size 336x272. IF 0, rot. 0."
		"Test stabilization. Frame size 132x96. IF 0, rot. 0."
		"Test stabilization. Frame size 132x132. IF 0, rot. 1."
		"Test stabilization. Frame size 132x132. IF 0, rot. 2."
		"Test stabilization. Frame size 132x96. IF 1, rot. 0.")
	};;

	1755|1756|1757|1758|1759) {
	valid=(			0	0	0	0	0	)
	input=(			$in81	$in81	$in82	$in82	$in82	)
	output=(		$out	$out	$out	$out	$out	)
	firstVop=(		0	0	0	0	0	)
	lastVop=(		30	30	30	30	30	)
	lumWidthSrc=(		352	352	352	352	352	)
	lumHeightSrc=(		288	288	288	288	288	)
	width=(			132	132	132	132	132	)
	height=(		132	132	96	132	132	)
	horOffsetSrc=(		44	44	44	44	44	)
	verOffsetSrc=(		52	52	52	52	52	)
	outputRateNumer=(	30	30	30	30	30	)
	outputRateDenom=(	1	1	1	1	1	)
	inputRateNumer=(	30	30	30	30	30	)
	inputRateDenom=(	1	1	1	1	1	)
	constIntraPred=(	0	0	0	0	0	)
	disableDeblocking=(	0	0	0	0	0	)
	filterOffsetA=(		0	0	0	0	0	)
	filterOffsetB=(		0	0	0	0	0	)
	intraVopRate=(		0	0	0	0	0	)
	mvRange=(		0	0	0	0	0	)
	mbPerSlice=(		0	0	0	0	0	)
	bitPerSecond=(		0	0	0	0	0	)
	vopRc=(			0	0	0	0	0	)
	vopSkip=(		0	0	0	0	0	)
	qpHdr=(			35	35	35	35	35	)
	qpMin=(			0	0	0	0	0	)
	qpMax=(			51	51	51	51	51	)
	level=(			0	0	0	0	0	)
	hrdConformance=(	0	0	0	0	0	)
	cpbSize=(		0	0	0	0	0	)
	mbRc=(			0	0	0	0	0	)
	chromaQpOffset=(	0	0	0	0	0	)
	inputFormat=(		1	1	2	2	2	)
	rotation=(		1	2	0	1	2	)
	stabilization=(		1	1	1	1	1	)
	byteStream=(		1	1	1	1	1	)
	sei=(			0	0	0	0	0	)
	testId=(		0	0	0	0	0	)
	category=("h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization"
	          "h264_stabilization")
	desc=(	"Test stabilization. Frame size 132x132. IF 1, rot. 1."
		"Test stabilization. Frame size 132x132. IF 1, rot. 2."
		"Test stabilization. Frame size 132x96. IF 2, rot. 0."
		"Test stabilization. Frame size 132x132. IF 2, rot. 1."
		"Test stabilization. Frame size 132x132. IF 2, rot. 2.")
	};;

1900|1901|1902|1903|1904) {
	valid=(			    0	    0	    0	    0	    0	)
	input=(			    $in100	$in18	$in270	$in84	$in22   )
	output=(		    $out	$out	$out	$out	$out	)
	firstVop=(		    0	    0	    0	    0	    0	)
	lastVop=(		    100	    30	    5	    30	    1	)
	lumWidthSrc=(	    352	    176	    1920	352	    352	)
	lumHeightSrc=(	    288	    144	    1080	288	    288	)
	width=(			    352	    176	    1280	132	    132	)
	height=(		    288	    144	    1024	96	    96	)
	horOffsetSrc=(		0	    0	    19	    120	    3	)
	verOffsetSrc=(		0	    0	    17	    96	    1	)
	outputRateNumer=(	25	    30	    30	    30	    30	)
	outputRateDenom=(	1	    1	    1	    1	    1	)
	inputRateNumer=(	25	    30	    30	    30	    30	)
	inputRateDenom=(	1	    1	    1	    1	    1	)
	constIntraPred=(	0	    0	    0	    0	    0	)
	disableDeblocking=(	0	    0	    0	    0	    0	)
	filterOffsetA=(		0	    0	    0	    0	    0	)
	filterOffsetB=(		0	    0	    0	    0	    0	)
	intraVopRate=(		1	    0	    1	    0	    0	)
	mvRange=(		    0	    0	    0	    0	    0	)
	mbPerSlice=(		0	    0	    0	    0	    0	)
	bitPerSecond=(		384000	512000	0	    0	    0	)
	vopRc=(			    1	    1	    0	    0	    0	)
	vopSkip=(		    0	    0	    0	    0	    0	)
	qpHdr=(			    51	    25	    35	    35	    10	)
	qpMin=(			    0	    0	    0	    0	    0	)
	qpMax=(			    51	    51	    51	    51	    51	)
	level=(			    0	    0	    32	    0	    0	)
	hrdConformance=(	0	    0	    0	    0	    0	)
	cpbSize=(		    30000000	0	    0	    0	    0	)
	mbRc=(			    0	    0	    0	    0	    0	)
	chromaQpOffset=(	0	    0	    0	    0	    0	)
	inputFormat=(		0	    0	    5	    0	    4	)
	rotation=(		    0	    0	    1	    0	    0	)
	stabilization=(		0	    0	    0	    1	    0	)
	byteStream=(		1	    1	    1	    1	    1	)
	sei=(			    0	    0	    0	    0	    0	)
	testId=(		    0	    0	    0	    0	    0	)
        category=("h264_intra"
                  "h264_rate_control"
                  "pre_processing"
                  "h264_stabilization"
                  "h264_cropping")
	desc=(	"H.264 Intra release test. Size 352x288, begins with QP 51. VopRc, 384 Kbps."
		"Test rate control. Normal operation with Qcif @ 512 Kbps."
		"Test RGB input. Max Frame size SXGA, rot. 0, crop 19x17."
		"Test stabilization. Frame size 112x96. IF 0, rot. 0."
		"Test cropping. Offset 3x1. Frame size 112x96. IF 3, rot. 0."	)
	};;

    1905) {
	valid=(			    0	)
	input=(			    $in271	)
	output=(		    $out	)
	firstVop=(		    0   )
	lastVop=(		    30	)
	lumWidthSrc=(	    640	)
	lumHeightSrc=(	    480	)
	width=(			    640	)
	height=(		    480	)
	horOffsetSrc=(		0   )
	verOffsetSrc=(		0   )
	outputRateNumer=(	30	)
	outputRateDenom=(	1	)
	inputRateNumer=(	30	)
	inputRateDenom=(	1	)
	constIntraPred=(	0	)
	disableDeblocking=(	0	)
	filterOffsetA=(		0	)
	filterOffsetB=(		0	)
	intraVopRate=(		0	)
	mvRange=(		    0	)
	mbPerSlice=(		0	)
	bitPerSecond=(		1024000	)
	vopRc=(			    1	)
	vopSkip=(		    0	)
	qpHdr=(			    35	)
	qpMin=(			    0	)
	qpMax=(			    51	)
	level=(			    0	)
	hrdConformance=(	0	)
	cpbSize=(		    0	)
	mbRc=(			    0	)
	chromaQpOffset=(	0   )
	inputFormat=(		11	)
	rotation=(		    0	)
	stabilization=(		0	)
	byteStream=(		1   )
	sei=(			    0   )
	testId=(		    0	)
        category=( "pre_processing")
	desc=(	"Test 32 bit RGB input. Frame size VGA, rot. 0.")
	};;

	* )
	valid=(			-1	-1	-1	-1	-1	);;
esac
