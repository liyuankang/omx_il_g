#!/bin/bash

# OMX CASE 1 - 10

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_10/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 10: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_10/out_w720h576.yuv 
if [ $? -eq 0 ]
then
    echo -e "case 10: OK" >> test_decoder.log
else
    echo -e "case 10: NOK" >> test_decoder.log
fi

# OMX CASE 1 - 12

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_12/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 12: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_12/out_w176h144.yuv

if [ $? -eq 0 ]
then
    echo -e "case 12: OK" >> test_decoder.log
else
    echo -e "case 12: NOK" >> test_decoder.log
fi

# OMX CASE 1 - 34

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_34/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 34: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_34/out_w640h480.yuv
if [ $? -eq 0 ]
then
    echo -e "case 34: OK" >> test_decoder.log
else
    echo -e "case 34: NOK" >> test_decoder.log
fi


# OMX CASE 1 - 46

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_46/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 46: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_46/out_w176h144.yuv
if [ $? -eq 0 ]
then
    echo -e "case 46: OK" >> test_decoder.log
else
    echo -e "case 46: NOK" >> test_decoder.log
fi


# OMX CASE 1 - 703

./omxdectest -I avc -i testdata_decoder/errorfree/case_703/stream.h264 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 703: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_703/out_w352h288.yuv
if [ $? -eq 0 ]
then
    echo -e "case 703: OK" >> test_decoder.log
else
    echo -e "case 703: NOK" >> test_decoder.log
fi


# OMX CASE 1 - 1510

./omxdectest -I wmv -i testdata_decoder/errorfree/case_1510/stream.rcv -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 1510: "
cmp output.yuv testdata_decoder/output/omx_case_1/case_1510/out_w352h288.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1510: OK" >> test_decoder.log
else
    echo -e "case 1510: NOK" >> test_decoder.log
fi


# OMX CASE 2

# testdata_decoder/errorfree/case_1000
# testdata_decoder/output/omx_case_2/case_1000
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1000/stream.jpg -O yuv420semiplanar -o output.yuv -s 3000000 -D
echo -n "case 1000: "
cmp output.yuv testdata_decoder/output/omx_case_2/case_1000/out_w1280h864.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1000: OK" >> test_decoder.log
    rm output.yuv
    for nro in 0 ; do
        echo -n "case 1000: frame number ${nro}: "
        cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1000/out_w1280h864.yuv.${nro}
        if [ $? -eq 0 ]
        then
            echo -e "case 1000 frame $nro: OK" >> test_decoder.log
            rm output.yuv.${nro}
        else
            echo -e "case 1000 frame $nro: NOK" >> test_decoder.log
        fi 
    done
else
    echo -e "case 1000: NOK" >> test_decoder.log
fi

# testdata_decoder/errorfree/case_1001
# testdata_decoder/output/omx_case_2/case_1001
# out.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1001/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000
echo -n "case 1001: "
cmp output.yuv testdata_decoder/output/omx_case_2/case_1001/out.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1001: OK" >> test_decoder.log
else
    echo -e "case 1001: NOK" >> test_decoder.log
fi

# testdata_decoder/errorfree/case_1009
# testdata_decoder/output/omx_case_2/case_1009
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1009/stream.jpg -O yuv420semiplanar -o output.yuv -s 3000000 -D -c 2
echo -n "case 1009: "
cmp output.yuv testdata_decoder/output/omx_case_2/case_1009/out_w1280h855.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1009: OK" >> test_decoder.log
    rm output.yuv
    for nro in 0 ; do
        echo -n "case 1009: frame number ${nro}: "
        cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1009/out_w1280h855.yuv.${nro}
        if [ $? -eq 0 ]
        then
            echo -e "case 1009 frame $nro: OK" >> test_decoder.log
            rm output.yuv.${nro}
        else
            echo -e "case 1009 frame $nro: NOK" >> test_decoder.log
        fi 
    done
else
    echo -e "case 1009: NOK" >> test_decoder.log
fi

# testdata_decoder/errorfree/case_1013
# testdata_decoder/output/omx_case_2/case_1013
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1013/stream.jpg -O yuv420semiplanar -o output.yuv -s 3000000 -D
echo -n "case 1013: "
cmp output.yuv testdata_decoder/output/omx_case_2/case_1013/out_w1280h864.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1013: OK" >> test_decoder.log
    rm output.yuv
    for nro in 0 ; do
        echo -n "case 1013: frame number ${nro}: "
        cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1013/out_w1280h864.yuv.${nro}
        if [ $? -eq 0 ]
        then
            echo -e "case 1013 frame $nro: OK" >> test_decoder.log
            rm output.yuv.${nro}
        else
            echo -e "case 1013 frame $nro: NOK" >> test_decoder.log
        fi 
    done
else
    echo -e "case 1013: NOK" >> test_decoder.log
fi

# testdata_decoder/errorfree/case_1026
# testdata_decoder/output/omx_case_2/case_1026
# out_0.yuv
# out_31.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1026/stream.jpg -O yuv420semiplanar -o output.yuv -x 1280 -y 720 -s 65482752 -D
echo -n "case 1026: "
cmp output.yuv testdata_decoder/output/omx_case_2/case_1026/out_w4672h3504.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1026: OK" >> test_decoder.log
    rm output.yuv
    for nro in 0 ; do
        echo -n "case 1026: frame number ${nro}: "
        cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1026/out_w4672h3504.yuv.${nro}
        if [ $? -eq 0 ]
        then
            echo -e "case 1026 frame $nro: OK" >> test_decoder.log
            rm output.yuv.${nro}
        else
            echo -e "case 1026 frame $nro: NOK" >> test_decoder.log
        fi 
    done
else
    echo -e "case 1026: NOK" >> test_decoder.log
fi
# OMX CASE 10 - 4106

./omxdectest -I mpeg2 -i testdata_81x0/errorfree/case_4106/stream.mpeg2 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4106: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4106/out_w128h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4106: OK" >> test_decoder.log
else
    echo -e "case 4106: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 4113

./omxdectest -I mpeg2 -i testdata_81x0/errorfree/case_4113/stream.mpeg2 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4113: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4113/out_w48h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4113: OK" >> test_decoder.log
else
    echo -e "case 4113: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 2114

./omxdectest -I mpeg4 -i testdata_81x0/errorfree/case_2114/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 2114: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_2114/out_w176h144.yuv
if [ $? -eq 0 ]
then
    echo -e "case 2114: OK" >> test_decoder.log
else
    echo -e "case 2114: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 428

#./omxdectest -I avc -i testdata_81x0/errorfree/case_428/stream.h264 -O yuv420semiplanar -o output.yuv -s 2200000
#echo -n "case 428: "
#cmp output.yuv testdata_81x0/output/omx_case_10/case_428/out_w1920h1088.yuv
#if [ $? -eq 0 ]
#then
#    echo -e "case 428: OK" >> test_decoder.log
#else
#    echo -e "case 428: NOK" >> test_decoder.log
#fi

# OMX CASE 10 - 4507

./omxdectest -I mpeg4 -i testdata_81x0/errorfree/case_4507/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4507: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4507/out_w96h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4507: OK" >> test_decoder.log
else
    echo -e "case 4507: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 7057

./omxdectest -I avc -i testdata_81x0/errorfree/case_7057/stream.h264 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 7057: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_7057/out_w128h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 7057: OK" >> test_decoder.log
else
    echo -e "case 7057: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 7371

./omxdectest -I avc -i testdata_81x0/errorfree/case_7371/stream.h264 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 7371: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_7371/out_w128h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 7371: OK" >> test_decoder.log
else
    echo -e "case 7371: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 4105

./omxdectest -I mpeg2 -i testdata_81x0/errorfree/case_4105/stream.mpeg2 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4105: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4105/out_w128h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4105: OK" >> test_decoder.log
else
    echo -e "case 4105: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 4112

./omxdectest -I mpeg2 -i testdata_81x0/errorfree/case_4112/stream.mpeg2 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4112: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4112/out_w48h48.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4112: OK" >> test_decoder.log
else
    echo -e "case 4112: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 7266

./omxdectest -I avc -i testdata_81x0/errorfree/case_7266/stream.h264 -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 7266: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_7266/out_w128h96.yuv
if [ $? -eq 0 ]
then
    echo -e "case 7266: OK" >> test_decoder.log
else
    echo -e "case 7266: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 4300

./omxdectest -I wmv -i testdata_81x0/errorfree/case_4300/stream.rcv -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4300: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4300/out_w48h48.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4300: OK" >> test_decoder.log
else
    echo -e "case 4300: NOK" >> test_decoder.log
fi

# OMX CASE 10 - 4322

./omxdectest -I wmv -i testdata_81x0/errorfree/case_4322/stream.rcv -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4322: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_4322/out_w176h144.yuv
if [ $? -eq 0 ]
then
    echo -e "case 4322: OK" >> test_decoder.log
else
    echo -e "case 4322: NOK" >> test_decoder.log
fi

./omxdectest -I wmv -i testdata_81x0/errorfree/case_1804/stream.rcv -O yuv420semiplanar -o output.yuv -s 2200000
echo -n "case 4322: "
cmp output.yuv testdata_81x0/output/omx_case_10/case_1804/out_w176h144.yuv
if [ $? -eq 0 ]
then
    echo -e "case 1804: OK" >> test_decoder.log
else
    echo -e "case 1804: NOK" >> test_decoder.log
fi


echo "done!"
