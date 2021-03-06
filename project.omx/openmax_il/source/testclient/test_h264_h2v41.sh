#!/bin/bash

# This script runs a single test case or all test cases
# The tests can be run with ARM simulator or executable binary
# For simulator, the input is ASIC reference data generated by system model

#set -x

# Output file when running all cases
# .log and .txt are created
resultfile=results_h264
#H264DEC=h264dec
# Current date
date=`date +"%d.%m.%y %k:%M"`

#if [ -s "test_data_parameter_h264.sh" ] ; then
    TEST_PARAMETER=test_data_parameter_h264.sh
#else
#    TEST_PARAMETER=customer_data_parameter_h264.sh
#fi

# Check parameter
if [ $# -eq 0 ]; then
    echo "Usage: $0 <test_case_number>"
    echo "   or: $0 <all>"
    echo "   or: $0 <first_case> <last_case>"
    exit
fi

enc_bin=./omxenctest
if [ ! -e $enc_bin ]; then
    echo "Executable $enc_bin not found. Compile with 'make'"
    exit
fi

ref_enc_bin=$SYSTEM_MODEL_HOME/h264_testenc
if [ ! -e $ref_enc_bin ]; then
    echo "Executable $ref_enc_bin not found. Compile with 'make'"
    exit
fi

# Reference encoder command
refencode() {
    if [ -s "rc.trc" ]; then
        rm rc.trc
    fi
    if [ -s "ref_rc.trc" ]; then
        rm ref_rc.trc
    fi
    omxnPFrames=0
    if [ ${intraVopRate[${set_nro}]} -gt 0 ] ; then
        let "omxnPFrames=${intraVopRate[${set_nro}]}"
    else
        omxnPFrames=150
    fi

    denom=$((90000/${outputRateNumer[${set_nro}]}))

  echo "${ref_enc_bin} \
	--input=${input[${set_nro}]} \
	--output=${output[${set_nro}]} \
	--lumWidthSrc=${lumWidthSrc[${set_nro}]} \
	--lumHeightSrc=${lumHeightSrc[${set_nro}]} \
	--width=${width[${set_nro}]} \
	--height=${height[${set_nro}]} \
	--horOffsetSrc=${horOffsetSrc[${set_nro}]} \
	--verOffsetSrc=${verOffsetSrc[${set_nro}]} \
	--outputRateNumer=90000 \
	--outputRateDenom=$denom \
	--inputRateNumer=${inputRateNumer[${set_nro}]} \
	--inputRateDenom=${inputRateDenom[${set_nro}]} \
	--intraPicRate=$omxnPFrames \
	--bitPerSecond=${bitPerSecond[${set_nro}]} \
	--picRc=${vopRc[${set_nro}]} \
	--picSkip=${vopSkip[${set_nro}]} \
	--qpHdr=${qpHdr[${set_nro}]} \
  --cpbSize=${cpbSize[${set_nro}]} \
	--level=${level[${set_nro}]} \
	--hrdConformance=${hrdConformance[${set_nro}]} \
	--rotation=${rotation[${set_nro}]} \
	--inputFormat=${inputFormat[${set_nro}]} \
	--firstPic=${firstVop[${set_nro}]} \
	--lastPic=${lastVop[${set_nro}]} \
    --intraArea=${intraArea[${set_nro}]} \
    --roi1Area=${roi1Area[${set_nro}]} \
    --roi1DeltaQp=${roiDeltaQP[${set_nro}]} \
    --disableDeblocking=${disableDeblocking[${set_nro}]}"

	${ref_enc_bin} \
	--input=${input[${set_nro}]} \
	--output=${output[${set_nro}]} \
	--lumWidthSrc=${lumWidthSrc[${set_nro}]} \
	--lumHeightSrc=${lumHeightSrc[${set_nro}]} \
	--width=${width[${set_nro}]} \
	--height=${height[${set_nro}]} \
	--horOffsetSrc=${horOffsetSrc[${set_nro}]} \
	--verOffsetSrc=${verOffsetSrc[${set_nro}]} \
	--outputRateNumer=90000 \
	--outputRateDenom=$denom \
	--inputRateNumer=${inputRateNumer[${set_nro}]} \
	--inputRateDenom=${inputRateDenom[${set_nro}]} \
	--intraPicRate=$omxnPFrames \
	--bitPerSecond=${bitPerSecond[${set_nro}]} \
	--picRc=${vopRc[${set_nro}]} \
	--picSkip=${vopSkip[${set_nro}]} \
	--qpHdr=${qpHdr[${set_nro}]} \
  --cpbSize=${cpbSize[${set_nro}]} \
	--level=${level[${set_nro}]} \
	--hrdConformance=${hrdConformance[${set_nro}]} \
	--rotation=${rotation[${set_nro}]} \
	--inputFormat=${inputFormat[${set_nro}]} \
	--firstPic=${firstVop[${set_nro}]} \
	--lastPic=${lastVop[${set_nro}]} \
    --intraArea=${intraArea[${set_nro}]} \
    --roi1Area=${roi1Area[${set_nro}]} \
    --roi1DeltaQp=${roiDeltaQP[${set_nro}]} \
    --disableDeblocking=${disableDeblocking[${set_nro}]}

    if [ -s "rc.trc" ]; then
        mv rc.trc ref_rc.trc
    fi
}

# Encoder command
# For armsd-simulator the command line must be less than 300 characters
encode() {

#    if [ ${bitPerSecond[${set_nro}]} -eq 0 ]; then
#        bitPerSecond[${set_nro}]=64000
#    fi

    if [ ${disableDeblocking[${set_nro}]} -eq 0 ] ; then
        deblocking=-d
    fi

    if [ ${mbRc[${set_nro}]} -eq 1 ] ; then
        omxRateControl='-C constant'
    fi

    omxRotation=0
    case "${rotation[$set_nro]}" in
        1) {
                omxRotation=90
            };;
        2) {
                omxRotation=270
            };;
        * )
    esac


    if [ ${stabilization[${set_nro}]} -eq 1 ] ; then
        omxStabilization=-Z
#        omxLastVop="$[${lastVop[${set_nro}]}+1]"
        omxLastVop="$[${lastVop[${set_nro}]}]"
    else
        omxStabilization=""
        omxLastVop=${lastVop[${set_nro}]}
    fi

    omxnPFrames=0
    if [ ${intraVopRate[${set_nro}]} -gt 0 ] ; then
        let "omxnPFrames=${intraVopRate[${set_nro}]}"
    else
        omxnPFrames=150
    fi

    omxOutputRate=${outputRateNumer[${set_nro}]}
    omxInputRate=${inputRateNumer[${set_nro}]}

    videoBufferSize[${set_nro}]=0

    if [ ${mbRc[${set_nro}]} -eq 0 ] ; then
        if [ ${hrdConformance[${set_nro}]} -eq 0 ]; then
            if [ ${vopRc[${set_nro}]} -eq 1 ]; then
                if [ ${vopSkip[${set_nro}]}  -eq 0 ]; then
                    if [ ${vopSkip[${set_nro}]}  -eq 0 ]; then
                        omxRateControl='-C variable'
                    fi
                fi
            fi
        fi
    fi

    if [ ${mbRc[${set_nro}]} -eq 1 ] ; then
        echo 11
        if [ ${hrdConformance[${set_nro}]} -gt 0 ]; then
            if [ ${vopSkip[${set_nro}]}  -eq 0 ]; then
                if [ ${vopRc[${set_nro}]} -eq 1 ]; then
                    omxRateControl='-C constant'
                fi
            fi
        fi
    fi

    if [ ${mbRc[${set_nro}]} -eq 0 ] ; then
        if [ ${hrdConformance[${set_nro}]} -eq 0 ]; then
            if [ ${vopRc[${set_nro}]} -eq 0 ]; then
                if [ ${vopSkip[${set_nro}]}  -eq 0 ]; then
                   omxRateControl='-C disable'
                fi
            fi
        fi
    fi


    if [ ${mbRc[${set_nro}]} -eq 1 ] ; then
        if [ ${hrdConformance[${set_nro}]} -eq 1 ]; then
            if [ ${vopRc[${set_nro}]} -eq 1 ]; then
                if [ ${vopSkip[${set_nro}]}  -eq 1 ]; then
                    omxRateControl='-C constant-skipframes'
                fi
            fi
        fi
    fi


    if [ ${mbRc[${set_nro}]} -eq 0 ] ; then
        if [ ${hrdConformance[${set_nro}]} -eq 0  ]; then
            if [ ${vopRc[${set_nro}]} -eq 1 ]; then
                if [ ${vopSkip[${set_nro}]}  -eq 1 ]; then
                    omxRateControl='-C variable-skipframes'
                fi
            fi
        fi
    fi

    case "${inputFormat[$set_nro]}" in
        0|1|2) {
            #yuv420 planar | yuv420 semi planar
            let bufferSize=${lumWidthSrc[${set_nro}]}*${lumHeightSrc[${set_nro}]}*3/2
              };;
        3|4|5|6|7|8|9|10) {
            #ycbycr | cbycry | rgb565 | bgr565 | rgb555 | rgb444
            let bufferSize=${lumWidthSrc[${set_nro}]}*${lumHeightSrc[${set_nro}]}*2
              };;
        11|12) {
            #rgb888 | bgr888 
            let bufferSize=${lumWidthSrc[${set_nro}]}*${lumHeightSrc[${set_nro}]}*4
              };;
        * )
    esac

    minBufSize=384
    if [ $bufferSize -lt $minBufSize ]; then
        bufferSize=$minBufSize
    fi

    echo "    ${enc_bin} \
    -O avc \
    -s ${bufferSize} \
    -i ${input[${set_nro}]} \
    -o ${output[${set_nro}]} \
    -w ${lumWidthSrc[${set_nro}]} \
    -h ${lumHeightSrc[${set_nro}]} \
    -f ${omxOutputRate} \
    -j ${omxInputRate} \
       ${deblocking} \
    -B ${bitPerSecond[${set_nro}]} \
    -q ${qpHdr[${set_nro}]} \
    -l ${inputFormat[${set_nro}]} \
    -r ${omxRotation} \
       ${omxStabilization} \
    -x ${width[${set_nro}]} \
    -y ${height[${set_nro}]} \
    -X ${horOffsetSrc[${set_nro}]} \
    -Y ${verOffsetSrc[${set_nro}]} \
       ${omxRateControl} \
    -L ${level[${set_nro}]} \
    -a ${firstVop[${set_nro}]} \
    -b $omxLastVop \
    -n $omxnPFrames \
    -t ${intraArea[${set_nro}]} \
    -A1 ${roi1Area[${set_nro}]} \
    -Q1 ${roiDeltaQP[${set_nro}]} \
    -AR ${adaptiveRoi[${set_nro}]} \
    -RC ${adaptiveRoiColor[${set_nro}]}"

    ${enc_bin} \
    -O avc \
    -s ${bufferSize} \
    -i ${input[${set_nro}]} \
    -o ${output[${set_nro}]} \
    -w ${lumWidthSrc[${set_nro}]} \
    -h ${lumHeightSrc[${set_nro}]} \
    -f ${omxOutputRate} \
    -j ${omxInputRate} \
       ${deblocking} \
    -B ${bitPerSecond[${set_nro}]} \
    -q ${qpHdr[${set_nro}]} \
    -l ${inputFormat[${set_nro}]} \
    -r ${omxRotation} \
       ${omxStabilization} \
    -x ${width[${set_nro}]} \
    -y ${height[${set_nro}]} \
    -X ${horOffsetSrc[${set_nro}]} \
    -Y ${verOffsetSrc[${set_nro}]} \
       ${omxRateControl} \
    -L ${level[${set_nro}]} \
    -a ${firstVop[${set_nro}]} \
    -b $omxLastVop \
    -n $omxnPFrames \
    -t ${intraArea[${set_nro}]} \
    -A1 ${roi1Area[${set_nro}]} \
    -Q1 ${roiDeltaQP[${set_nro}]} \
    -AR ${adaptiveRoi[${set_nro}]} \
    -RC ${adaptiveRoiColor[${set_nro}]}
}

compare() {
    if [ -s "$case_dir/output.h264" ]; then
        if cmp $case_dir/output.h264 $case_dir/reference.h264; then
            echo "Case $case_nro OK"
            echo "E case_$case_nro;SW/HW Integration;;$date;OK;;$hwtag;$USER;;;$swtag;$systag"
        else
            echo "Case $case_nro FAILED"
            echo "E case_$case_nro;SW/HW Integration;;$date;Failed;;$hwtag;$USER;;;$swtag;$systag"

            if [ -s "$H264DEC" ]; then
                $H264DEC -Oref.yuv $case_dir/reference.h264 >> ref.log
                $H264DEC -Oomx.yuv $case_dir/output.h264 >> omx.log

                if cmp ref.yuv omx.yuv; then
                    echo "failed, Decoded outputs match"
                else
                    echo "failed, decoded outputs differ"
                fi
            fi
        fi
    else
        echo "Case $case_nro FAILED"
        echo "E case_$case_nro;SW/HW Integration;;$date;Failed;;$hwtag;$USER;;;$swtag;$systag"
        echo "failed, $case_dir/output.h264 size 0 or file not found"
    fi
}


test_set() {
    # Test data dir
    let "set_nro=${case_nro}-${case_nro}/5*5"
    case_dir=$TEST_DATA_HOME/case_$case_nro

    # Do nothing if test case is not valid
    if [ ${valid[${set_nro}]} -eq -1 ]; then
        echo "case_$case_nro is not valid."
    else
        # Do nothing if test data doesn't exists
        if [ ! -e $case_dir ]; then
            mkdir $case_dir
        fi

        echo "========================================================="
        echo "Run case $case_nro"

        # Check/Adjust parameters
        odd=`expr ${horOffsetSrc[${set_nro}]} % 2`
        if [ ${odd} -eq 1 ]; then
            horOffsetSrc[${set_nro}]=`expr ${horOffsetSrc[${set_nro}]} + 1`
            echo "horOffsetSrc is changed to ${horOffsetSrc[${set_nro}]} -- it must be even number"
        fi

        odd=`expr ${verOffsetSrc[${set_nro}]} % 2`
        if [ ${odd} -eq 1 ]; then
            verOffsetSrc[${set_nro}]=`expr ${verOffsetSrc[${set_nro}]} + 1`
            echo "verOffsetSrc is changed to ${verOffsetSrc[${set_nro}]} -- it must be even number"
        fi

        if [ ${#intraArea[${set_nro}]} -eq 0 ]; then
            intraArea[${set_nro}]=0
        fi
        if [ ${#roi1Area[${set_nro}]} -eq 0 ]; then
            roi1Area[${set_nro}]=0
        fi
        if [ ${#roiDeltaQP[${set_nro}]} -eq 0 ]; then
            roiDeltaQP[${set_nro}]=0
        fi

        rm -f $case_dir/*.h264 $casedir/*.log $casedir/*.output $casedir/*.trc

        # Run reference encoding
        echo "STEP 1: Encoding reference data with reference encoder"
        refencode
        mv stream.h264 $case_dir/reference.h264

        # Run encoder
        echo "STEP 2: Encoding output with OMX IL encoder component"
        encode
        mv stream.h264 $case_dir/output.h264

        # Compare stream to reference
        echo "STEP 3: Comparing reference to the output"

        if [ -s "$case_dir/output.h264" ]; then
            cmp $case_dir/output.h264 $case_dir/reference.h264
            comparison_result=$?

            if [ $comparison_result -gt 0 ]; then
                if [ -s "$H264DEC" ]; then
                    $H264DEC -Oref.yuv $case_dir/reference.h264 >> ref.log
                    $H264DEC -Oomx.yuv $case_dir/output.h264 >> omx.log

                    if cmp ref.yuv omx.yuv; then
                        echo "failed, Decoded outputs match"
                    else
                        echo "failed, decoded outputs differ"
                    fi
                fi
            fi

        else
            comparison_result=1
            echo "failed, $case_dir/output.h264 size 0 or file not found"
        fi

        # Print the case result
        result_str="OK"
        comparison_comment=""
        if [ ! $comparison_result -eq 0 ]; then
            result_str="NOK"
            comparison_comment="STRMS_DIFFER"
        fi
        comment_str="$comparison_comment"
        echo "Case $case_nro $result_str ($comment_str)"
        echo "E case_$case_nro;${category[${set_nro}]};${desc[${set_nro}]}; Hantro Encoder OMX IL Component;$date;$result_str;$comment_str;$USER"
    fi
}

run_cases() {

    echo "Running test cases $first_case..$last_case"
    echo "This will take several minutes"
    echo "Output is stored in $resultfile.log and $resultfile.txt"
    rm -f $resultfile.log $resultfile.txt
    for ((case_nro=$first_case; case_nro<=$last_case; case_nro++))
    do
        . $TEST_PARAMETER  "$case_nro"
        echo -en "\rCase $case_nro\t"
        test_set >> $resultfile.log
    done
    echo -e "\nAll done!"
    grep "E case_" $resultfile.log > $resultfile.csv
    grep "Case" $resultfile.log > $resultfile.txt
    cat $resultfile.txt
}

run_case() {

    . $TEST_PARAMETER "$case_nro"
    test_set
}


# Encoder test set.
trigger=-1 # NO Logic analyzer trigger by default

case "$1" in
    all)
    first_case=1000;
    last_case=1999;
    run_cases
    ;;
    *)
    if [ $# -eq 3 ]; then trigger=$3; fi
    if [ $# -eq 1 ]; then
        case_nro=$1;
        run_case;
    else
        first_case=$1;
        last_case=$2;
        run_cases;
    fi
    ;;
esac


