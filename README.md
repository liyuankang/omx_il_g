VPU OpenMX IL software and drivers
=====================================
Build Process:
--------------
0. Preparation:
  export PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin
  export KDIR=~/linux_4.14

1. ENCODER
--------------
1.1 Build library and testbench for ARM+CModel
  make clean $TARGET ENV=arm_pclinux USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib_arm64/libvc8kd.a
 Available targets:
  vp9dec          - VP9 decoder command line binary
  hevcdec         - HEVC decoder command line binary
  libg2hw.a       - G2 decoder system model library
  g2dec           - G2 (include hevc, vp9, h264high,avs2)
  avsdec          - AVS decoder command line binary
  h264dec         - H264 decoder command line binary
  h264dec_osfree  - H264 OS Free  decoder command line binary
  jpegdec         - JPEG decoder command line binary
  mpeg2dec        - MPEG2/MPEG1 decoder command line binary
  mpeg4dec        - MPEG4/H263 decoder command line binary
  rvdec           - RealVideo decoder command line binary
  vc1dec          - VC1/WMV9 decoder command line binary
  vp6dec          - VP6 decoder command line binary
  vp8dec          - VP7/8 decoder command line binary

1.2 Build Driver
(a) hantrodec.ko
  cd software/linux/ldriver
  make ARCH=arm64
(b) memalloc.ko
  cd software/linux/ldriver
  make ARCH=arm64

2.DECODER
------------
Build Driver
===============
vi vc8000e/software/build/globalrules
vi vc8000e/software/linux_reference/Baseaddress

(a) hx280Enc.ko
cd vc8000e/software/linux_reference/kernel_moudle
make ARCH=arm64
(b) memolloc.ko
cd vc8000e/software/linux_reference/memalloc
make ARCH=arm64

Build Test
-------------------
(a)hevc_testenc
cd vc8000e/software/linux_reference/
make versatile
(b) jpeg_testenc
make versatile

3. OpenMX project
-------------------
3.1 OMX_IL decoder:
--------------		
cd ~/omx_il/project.omx/openmax_il/source/decoder
(1). build libraries
build_dec_libs.sh arm_pclinux 64
> ~/omx_il/vc8000d/out/arm_linux/debug/avsdec,libdwl.a ....
(2). build OMX IL Decoder component
> libOMX.hantro.VC8000D.video.decoder.so
> libOMX.hantro.VC8000D.image.decoder.so

OMX_IL encoder
--------------
make arm ENCODER_API_VERSION=vc8000e [variables]"



File list:
----------
     |- project.omx
        |- libomxil-bellagio-0.9.3 OMXIL bellagio	V.0.8.3 source code
        |- openmax_il 					OpenMax IL on Android
     |- vc800d										VC8000 decoder driver and source code
     |- vc800e 										VC8000 encoder driver and source code

Gerrit commands:
----------------
* clone:
        export project=omx_il
        git clone "ssh://cj.chang@gerrit.siengine.com:29418/$project"
        scp -p -P 29418 cj.chang@gerrit.siengine.com:hooks/commit-msg "$project/.git/hooks/"
* commit:
        cd $project
        git add .
        git commit -s -m"<comments>"
        git push origin HEAD:refs/for/master

History:
---------
2020/05/26
    Duplicated from 20200521 released packages:
        - _SW_VC8000E_OMX_ctrlSW_CmodelLib_CL244132_266903_20200514.tar
        - _SW_VPU_VC8000D_OMX_IL_CL267662_266903_20200513.tar
2020/06/01
   Update cache driver in decoder.

