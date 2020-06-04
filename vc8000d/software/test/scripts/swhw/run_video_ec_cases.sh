#!/bin/bash

# Base paths
BASE_HOME=$PWD
VIDEO_HOME=$BASE_HOME/../../
MODEL_HOME=$BASE_HOME/../../system/models/golden/

#-------------------------------------------------------------------------------
#-                                                                            --
#- Check script arguments                                                     --
#-                                                                            --
#-------------------------------------------------------------------------------
check_args()
{
    echo ""
    echo ""
    echo "Usage:"
    echo "./run_video_ec_cases.sh -F [video format or all formats] -E [EC type]"
    echo " "
    echo "where"
    echo -e "flags :  -F [AVS, H264, MPEG2, MPEG4, RV, VC1, VP6, VP7, VP8, ALL]"
    echo -e "         -E  0 = PICTURE FREEZE"
    echo -e "             1 = VIDEO FREEZE"
    echo -e "             2 = PARTIAL FREEZE"
    echo -e "             3 = PARTIAL IGNORE"
    echo ""
    echo -e "             4 = NORMAL MODE (no stream violation on model)"
    echo -e "                 * NOTE! Type 4 used only for generating"
    echo -e "                         unbroken output for reference"
    echo ""
    echo ""
    echo -e "            NOTE! stetenv STREAM_HOME and DATA_OUT paths"
    echo ""
    echo -e "            setenv STREAM_HOME /path/to/test/cases/"
    echo -e "            setenv DATA_OUT /path/to/where/data/will/be/generated"
    exit 0
}
#-------------------------------------------------------------------------------
#-                                                                            --
#- Create binaries                                                            --
#-                                                                            --
#-------------------------------------------------------------------------------
generateEcData()
{
    cd $VIDEO_HOME/
    tb=$1
    bin_pclinux=$2
    case=$3
    ec_format=$4

    echo ""
    echo "--------------------------------------------------------------------"
    echo "Configuration : $tb, $case, $ec_format"

    # set if case vp7
    if [ "$tb" == "vp7" ]
    then
        tb_dest=vp8
    else
        tb_dest=$1
    fi

    # generate tb.cfg
    generateTbConfig "$ec_format" "$tb"

    # goto folder (also handle vp7 ==> running under vp8 folder)
    if [ "$tb" == "vp7" ]
    then
        cd vp8/
    else
        cd $1/
    fi

    # remove yuv's
    rm -rf *.yuv

    echo -n "Compiling ${tb} API test driver for pclinux... "
    make clean >> $BUILD_LOG 2>&1
    make libclean >> $BUILD_LOG 2>&1
    if [ "$ec_format" == 4 ]
    then
        make pclinux USE_MD5SUM=n >> $BUILD_LOG 2>&1
    else
        make pclinux EXTENDED_EC_TEST=y USE_MD5SUM=n >> $BUILD_LOG 2>&1
    fi
    # check if succeeded
    if [ ! -f "$bin_pclinux" ]
    then
        echo "FAILED"
    else
        echo "OK"
    rm -f build.log
    fi
  
    echo "Running ${tb} EC test case ${case}... "
    ./$bin_pclinux $STREAM_HOME/$case/stream.* &> /dev/null

    # remove extra yuv if RV
    if [ "$tb" == "rv" ]
    then
        rm frame.yuv
    fi

    #handle extra cases, new naming
    if [ "$case" == "case_3504" ]
    then
        tb="mpeg4_asp"
    fi

    if [ "$case" == "case_3502" ]
    then
        tb="h264high_hp"
    fi

    if [ "$case" == "case_3506" ]
    then
        tb="vc1_adv"
    fi
  
    #copy to certain folder
    if [ "$ec_format" == "0" ]
    then
        echo -n "Copying to folder PICTURE_FREEZE/ ..."
        mv *.yuv $DATA_OUT/ext_ec_test/picture_freeze/$tb.yuv &> /dev/null
    elif [ "$ec_format" == "1" ]
    then
        echo -n "Copying to folder VIDEO_FREEZE/ ..."
        mv *.yuv $DATA_OUT/ext_ec_test/video_freeze/$tb.yuv &> /dev/null
    elif [ "$ec_format" == "2" ]
    then
        echo -n "Copying to folder PARTIAL_FREEZE/ ..."
        mv *.yuv $DATA_OUT/ext_ec_test/partial_freeze/$tb.yuv &> /dev/null
    elif [ "$ec_format" == "3" ]
    then
        echo -n "Copying to folder PARTIAL_IGNORE/ ..."
        mv *.yuv $DATA_OUT/ext_ec_test/partial_ignore/$tb.yuv &> /dev/null
    elif [ "$ec_format" == "4" ]
    then
        echo -n "Copying to folder ORIGINAL/ ..."
    mv *.yuv $DATA_OUT/ext_ec_test/original/$tb.yuv &> /dev/null
    else
        echo "invalid EC folder"
    exit 0
    fi

    echo "DONE"

    # return to base
    cd $BASE_HOME
}

#-------------------------------------------------------------------------------
#-                                                                            --
#- Create tb.cfg
#-                                                                            --
#-------------------------------------------------------------------------------
generateTbConfig()
{
    # create tb.cfg in to video test folder
    if [ "$2" == "vp7" ]
    then
        cd vp8/
    else
        cd $2/
    fi

    format=$1

    #remove old config
    rm -f tb.cfg

    # tb.cfg
    echo "DecParams {" >> tb.cfg

    # select tb.cfg/model Makefile (because we need to "violate" stream)
    if [ "$format" == "0" ]
    then
        echo "    ErrorConcealment = PICTURE_FREEZE;" >> tb.cfg
    elif [ "$format" == "1" ]
    then
        echo "    ErrorConcealment = INTRA_FREEZE;" >> tb.cfg
    elif [ "$format" == "2" ]
    then
        echo "    ErrorConcealment = PARTIAL_FREEZE;" >> tb.cfg
    elif [ "$format" == "3" ]
    then
        echo "    ErrorConcealment = PARTIAL_IGNORE;" >> tb.cfg
    else
        echo ""
    fi

    # set rest of needed parameters
    echo "    HwVersion = 10000;" >> tb.cfg
    echo "    HwBuild = 1042;" >> tb.cfg
    echo "}" >> tb.cfg

    cd ..
}

#-------------------------------------------------------------------------------
#-                                                                            --
#- Create test data folders
#-                                                                            --
#-------------------------------------------------------------------------------
generateTestFolders()
{
    # type of EC
    ec_format=$1

    # create major dir/ for generated data
    mkdir -p $DATA_OUT/ext_ec_test

    # select tb.cfg/model Makefile (because we need to "violate" stream)
    if [ "$ec_format" == "0" ]
    then
        mkdir -p $DATA_OUT/ext_ec_test/picture_freeze
    elif [ "$ec_format" == "1" ]
    then
        mkdir -p $DATA_OUT/ext_ec_test/video_freeze
    elif [ "$ec_format" == "2" ]
    then
        mkdir -p $DATA_OUT/ext_ec_test/partial_freeze
    elif [ "$ec_format" == "3" ]
    then
        mkdir -p $DATA_OUT/ext_ec_test/partial_ignore
    elif [ "$ec_format" == "4" ]
    then
        mkdir -p $DATA_OUT/ext_ec_test/original
    else
        echo "invalid EC type"
    exit 0
    fi
}

#-------------------------------------------------------------------------------
#-                                                                            --
#- Main                                                                       --
#-                                                                            --
#-------------------------------------------------------------------------------

# check that environment variables are set
if [ -z $DATA_OUT ] || [ -z $STREAM_HOME ]
then
    echo "Need to set DATA_OUT and/or STREAM_HOME"
    exit 1
fi

# get params
if [ $# -lt 4 ] || [ "$1" == "help" ]
then
    check_args
else
    # cycle through all command line options
    while [ $# -gt 0 ]
    do
        # format
        if [ "$1" == "-F" ]
        then
            MODE=$2
            # ec type
            if [ "$3" == "-E" ]
            then
                EC_TYPE=$4
            fi
        fi
        # change to next argument
        shift
    done
fi

# create test data folders
generateTestFolders "$EC_TYPE"

# set/remove build.log
BUILD_LOG=$BASE_HOME/"build.log"
rm -rf $BUILD_LOG

# Main functions
if [ "$MODE" == "AVS" ]
then
    generateEcData "avs" "ax170dec_pclinux" "case_3537" "$EC_TYPE"
elif [ "$MODE" == "H264" ]
then
    generateEcData "h264high" "hx170dec_pclinux" "case_3501" "$EC_TYPE"
    generateEcData "h264high" "hx170dec_pclinux" "case_3502" "$EC_TYPE"
elif [ "$MODE" == "MPEG2" ]
then
    generateEcData "mpeg2" "m2x170dec_pclinux" "case_3507" "$EC_TYPE"
elif [ "$MODE" == "MPEG4" ]
then
    generateEcData "mpeg4" "mx170dec_pclinux" "case_3503" "$EC_TYPE"
    generateEcData "mpeg4" "mx170dec_pclinux" "case_3504" "$EC_TYPE"
elif [ "$MODE" == "RV" ]
then
    generateEcData "rv" "rvx170dec_pclinux" "case_3500" "$EC_TYPE"
elif [ "$MODE" == "VC1" ]
then
    generateEcData "vc1" "vx170dec_pclinux" "case_3505" "$EC_TYPE"
    generateEcData "vc1" "vx170dec_pclinux" "case_3506" "$EC_TYPE"
elif [ "$MODE" == "VP6" ]
then
    generateEcData "vp6" "vp6dec_pclinux" "case_3534" "$EC_TYPE"
elif [ "$MODE" == "VP7" ]
then
    generateEcData "vp7" "vp8x170dec_pclinux" "case_3536" "$EC_TYPE"
elif [ "$MODE" == "VP8" ]
then
    generateEcData "vp8" "vp8x170dec_pclinux" "case_3535" "$EC_TYPE"
elif [ "$MODE" == "ALL" ]
then
    generateEcData "avs" "ax170dec_pclinux" "case_3537" "$EC_TYPE"
    generateEcData "h264high" "hx170dec_pclinux" "case_3501" "$EC_TYPE"
    generateEcData "h264high" "hx170dec_pclinux" "case_3502" "$EC_TYPE"    
    generateEcData "mpeg2" "m2x170dec_pclinux" "case_3507" "$EC_TYPE"
    generateEcData "mpeg4" "mx170dec_pclinux" "case_3503" "$EC_TYPE"
    generateEcData "mpeg4" "mx170dec_pclinux" "case_3504" "$EC_TYPE"
    generateEcData "rv" "rvx170dec_pclinux" "case_3500" "$EC_TYPE"
    generateEcData "vc1" "vx170dec_pclinux" "case_3505" "$EC_TYPE"
    generateEcData "vc1" "vx170dec_pclinux" "case_3506" "$EC_TYPE"
    generateEcData "vp6" "vp6dec_pclinux" "case_3534" "$EC_TYPE"
    generateEcData "vp7" "vp8x170dec_pclinux" "case_3536" "$EC_TYPE"
    generateEcData "vp8" "vp8x170dec_pclinux" "case_3535" "$EC_TYPE"
else
    echo "invalid video format"
    exit 0
fi

#echo "See build.log for more information on building."

echo "Test completed"
echo "--------------------------------------------------------------------"
exit 0
