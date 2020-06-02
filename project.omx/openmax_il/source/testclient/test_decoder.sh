#!/bin/bash

# OMX CASE 1 - 10

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_10/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 10: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_10/out_w720h576.yuv 


# OMX CASE 1 - 12

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_12/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 12: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_12/out_w176h144.yuv

# OMX CASE 1 - 34

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_34/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 34: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_34/out_w640h480.yuv

# OMX CASE 1 - 46

./omxdectest -I mpeg4 -i testdata_decoder/errorfree/case_46/stream.mpeg4 -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 46: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_46/out_w176h144.yuv

# OMX CASE 1 - 703

./omxdectest -I avc -i testdata_decoder/errorfree/case_703/stream.h264 -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 703: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_703/out_w352h288.yuv

# OMX CASE 1 - 1510

./omxdectest -I wmv -i testdata_decoder/errorfree/case_1510/stream.rcv -O yuv420semiplanar -o output.yuv -s 1100000
echo -n "case 1510: "
./cmp output.yuv testdata_decoder/output/omx_case_1/case_1510/out_w352h288.yuv

# OMX CASE 2

# testdata_decoder/errorfree/case_1000
# testdata_decoder/output/omx_case_2/case_1000
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1000/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000 -D
for nro in 0 1 ; do
    echo -n "case 1000: frame number ${nro}: "
    ./cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1000/out_${nro}.yuv
    rm output.yuv.${nro}
done

# testdata_decoder/errorfree/case_1001
# testdata_decoder/output/omx_case_2/case_1001
# out.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1001/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000
echo -n "case 1001: "
./cmp output.yuv testdata_decoder/output/omx_case_2/case_1001/out.yuv

# testdata_decoder/errorfree/case_1007
# testdata_decoder/output/omx_case_2/case_1007
# out_0.yuv
# out_15.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1007/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000 -D
for nro in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ; do
    echo -n "case 1007: frame number ${nro}: "
 	./cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1007/out_${nro}.yuv
    rm output.yuv.${nro}
done

# testdata_decoder/errorfree/case_1009
# testdata_decoder/output/omx_case_2/case_1009
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1009/stream.jpg -O yuv420semiplanar -o output.yuv -s 2088960 -D -c 2
for nro in 0 1 ; do
   echo -n "case 1009: frame number ${nro}: "
  ./cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1009/out_${nro}.yuv
    rm output.yuv.${nro}
done

# testdata_decoder/errorfree/case_1013
# testdata_decoder/output/omx_case_2/case_1013
# out_0.yuv
# out_1.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1013/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000 -D
for nro in 0 1 ; do
    echo -n "case 1013: frame number ${nro}: "
    ./cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1013/out_${nro}.yuv
    rm output.yuv.${nro}
done

# testdata_decoder/errorfree/case_1026
# testdata_decoder/output/omx_case_2/case_1026
# out_0.yuv
# out_31.yuv

./omxdectest -I jpeg -i testdata_decoder/errorfree/case_1026/stream.jpg -O yuv420semiplanar -o output.yuv -s 1600000 -D
for nro in 0 1 2 3 4 5 6 7 8 9 10 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 ; do
    echo -n "case 1013: frame number ${nro}: "
    ./cmp output.yuv.${nro} testdata_decoder/output/omx_case_2/case_1026/out_${nro}.yuv
    rm output.yuv.${nro}
done

echo "done!"
