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

RGB_SEQUENCE_HOME=${YUV_SEQUENCE_HOME}/rgb

# Input YUV files
in18=${YUV_SEQUENCE_HOME}/qcif/metro2_25fps_qcif.yuv
in22=${YUV_SEQUENCE_HOME}/synthetic/kuuba_random_w352h288.uyvy422
in84=${YUV_SEQUENCE_HOME}/synthetic/rangetest_w352h288.yuv
in85=${YUV_SEQUENCE_HOME}/vga/kuuba_maisema_25fps_vga.yuv
in86=${YUV_SEQUENCE_HOME}/cif/foreman_cif.yuv

# Intra release case
in100=${YUV_SEQUENCE_HOME}/cif/metro_25fps_cif.yuv

# RGB input
in270=${RGB_SEQUENCE_HOME}/1080p/rush_hour_25fps_w1920h1080.rgb565
in271=${RGB_SEQUENCE_HOME}/vga/shields_60fps_vga.rgb888

# Dummy
in999=/dev/zero

# VP8 stream name, do not change
out=stream.ivf

# Intra area parameters
intraA='7:4:17:16'

# ROI area parameters
roiA='7:4:17:16'

case "$1" in
3000|3001|3002|3003|3004) {
	valid=(			    0	    0	    0	    0	    0	    )
	input=(			    $in100	$in18	$in270	$in84	$in22   )
	output=(		    $out	$out	$out	$out	$out	)
	firstVop=(		    0	    0	    0	    0	    0	    )
	lastVop=(		    100	    30	    5	    30	    100	    )
	lumWidthSrc=(	    352	    176	    1920	352	    352	    )
	lumHeightSrc=(	    288	    144	    1080	288	    288	    )
	width=(			    352	    176	    1280	144	    144	    )
	height=(		    288	    144	    1024	96	    96	    )
	horOffsetSrc=(		0	    0	    19	    120	    3	    )
	verOffsetSrc=(		0	    0	    17	    96	    1	    )
	outputRateNumer=(	25	    30	    30	    30	    30	    )
	outputRateDenom=(	1	    1	    1	    1	    1	    )
	inputRateNumer=(	25	    30	    30	    30	    30	    )
	inputRateDenom=(	1	    1	    1	    1	    1	    )
	intraVopRate=(		1	    0	    1	    0	    30	    )
	dctPartitions=(		0	    0	    0	    0	    0	    )
	errorResilient=(	0	    0	    0	    0	    0	    )
	ipolFilter=(    	0	    0	    0	    0	    0	    )
	filterType=(    	0	    0	    0	    0	    0	    )
	bitPerSecond=(		512000	384000	1024000	384000	384000  )
	vopRc=(			    1	    1	    0	    0	    0	    )
	vopSkip=(		    0	    0	    0	    0	    0	    )
	qpHdr=(			    51	    25	    35	    35	    10	    )
	qpMin=(			    0	    0	    0	    0	    0	    )
	qpMax=(			    51	    51	    51	    51	    51	    )
	level=(			    1	    1	    1	    1	    1	    )
	mbRc=(			    0	    0	    0	    0	    0	    )
	inputFormat=(		0	    0	    5	    0	    4	    )
	rotation=(		    0	    0	    1	    0	    0	    )
	stabilization=(		0	    0	    0	    1	    0	    )
	testId=(		    0	    0	    0	    0	    0	    )
	intraArea=(		    0	    0	    0	    0	    0	    )
	roi1Area=(		    0	    0	    0	    0	    0	    )
	roiDeltaQP=(	    0	    0	    0	    0	    0	    )
	adaptiveRoi=(	    0	    0       0       0       0       )
	adaptiveRoiColor=(  0	    0       0       0       0       )
	baseLayerBitrate=(  0	    0       0       0       0       )
	layer1Bitrate=(     0	    0       0       0       0       )
	layer2Bitrate=(     0	    0       0       0       0       )
	layer3Bitrate=(     0	    0       0       0       0       )
        category=("VP8_intra"
                  "VP8_rate_control"
                  "pre_processing"
                  "VP8_stabilization"
                  "VP8_cropping")
	desc=(	"VP8 Intra release test. Size 352x288, begins with QP 51. VopRc, 512 Kbps."
		"Test rate control. Normal operation with Qcif @ 384 Kbps."
		"Test RGB input. Max Frame size SXGA, rot. 0, crop 19x17."
		"Test stabilization. Frame size 144x96. IF 0, rot. 0."
		"Test cropping. Offset 3x1. Frame size 144x96. IF 3, rot. 0.")
	};;

    3005|3006|3007|3008|3009) {
	valid=(			    0	    0       0       0       0)
	input=(			    $in271	$in85   $in86   $in86   $in86)
	output=(		    $out	$out    $out    $out    $out)
	firstVop=(		    0       0       0       0       0)
	lastVop=(		    30	    58      49      49      49)
	lumWidthSrc=(	    640	    640     352     352     352)
	lumHeightSrc=(	    480	    480     288     288     288)
	width=(			    640	    608     352     352     352)
	height=(		    480	    448     288     288     288)
	horOffsetSrc=(		0       0       0       0       0)
	verOffsetSrc=(		0       0       0       0       0)
	outputRateNumer=(	30	    25      25      30      30)
	outputRateDenom=(	1	    1       1       1       1)
	inputRateNumer=(	30	    25      25      30      30)
	inputRateDenom=(	1	    1       1       1       1)
	intraVopRate=(		0	    15      0       20      20)
	dctPartitions=(		0	    0       0       0       0)
	errorResilient=(	0	    0       0       0       0)
	ipolFilter=(    	0	    0       0       0       0)
	filterType=(    	0	    0       0       0       0)
	bitPerSecond=(		1024000	1600000 512000  384000  256000)
	vopRc=(			    1	    1       1       1       1)
	vopSkip=(		    0	    0       0       0       0)
	qpHdr=(			    35	    25      30      30      36)
	qpMin=(			    0	    10      10      10      10)
	qpMax=(			    51	    51      51      51      127)
	level=(			    1	    1       1       1       1)
	mbRc=(			    0	    0       0       0       0)
	inputFormat=(		11	    0       0       0       0)
	rotation=(		    0	    0       0       0       0)
	stabilization=(		0	    1       0       0       0)
	testId=(		    0	    0       0       0       0)
	intraArea=(		    0	    0       $intraA 0       0)
	roi1Area=(		    0	    0       0       $roiA   $roiA)
	roiDeltaQP=(	    0	    0       0       -6      -8)
	adaptiveRoi=(	    0	    0       0       -10     0)
	adaptiveRoiColor=(  0	    0       0       10      0)
	baseLayerBitrate=(  0	    0       0       0       96000)
	layer1Bitrate=(     0	    0       0       0       128000)
	layer2Bitrate=(     0	    0       0       0       196000)
	layer3Bitrate=(     0	    0       0       0       0)
        category=( "pre_processing"
                   "stabilization"
                   "intra area"
                   "ROI area")
	desc=(	"Test 32 bit RGB input. Frame size VGA, rot. 0."
            "Test stabilization"
            "Test intra area"
            "Test ROI area")
	};;

	* )
	valid=(			-1	-1	-1	-1	-1	);;
esac
