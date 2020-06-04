# VPU OpenMX IL software and drivers
=====================================
## File list:
----------
     |- project.omx
        |- libomxil-bellagio-0.9.3 OMXIL bellagio	V.0.8.3 source code
        |- openmax_il 					OpenMax IL on Android
     |- vc800d						VC8000 decoder driver and source code
     |- vc800e 						VC8000 encoder driver and source code

## Gerrit commands:
----------------
* clone:

        export project=omx_il
        git clone "ssh://<name>@gerrit.<domian>:29418/$project"
        scp -p -P 29418 cj.chang@gerrit.<domian>:hooks/commit-msg "$project/.git/hooks/"
* commit:

        cd $project
        git add .
        git commit -s -m"<comments>"
        git push origin HEAD:refs/for/master

## Build Process:
------------------


0. Preparation:
--------------

        export PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin
        export KDIR=~/linux_4.14

1. DECODER
--------------
        cd ~/omx_il/vc8000d
* 1.1 Build Cache

        cd cache/software/linux_reference
(a) cache driver

        cd kernel_module
        make ARCH=arm64
        > hantro_cache.ko
 
(b) cache lib

        cd ..
        make versatile   
        > libcache.a

* 1.2 Build library and testbench for ARM+CModel

        cd vc8000d/
        export TARGET=hevcdec 
        make clean $TARGET ENV=arm_pclinux USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib_arm64/libvc8kd.a SUPPORT_CACHE=y CACHE_DIR=~/omx_il_g/vc8000d/cache
        #To support Cache, add SUPPORT_CACHE and CACHE_DIR

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

* 1.3 Build Driver

(a) hantrodec.ko

        cd software/linux/ldriver
        make ARCH=arm64

(b) memalloc.ko

        cd software/linux/ldriver
        make ARCH=arm64




2. ENCODER
------------

* 2.1 Build Driver

        vi vc8000e/software/build/globalrules
        vi vc8000e/software/linux_reference/Baseaddress

(a) hx280Enc.ko

        cd vc8000e/software/linux_reference/kernel_moudle
        make ARCH=arm64

(b) memolloc.ko

        cd vc8000e/software/linux_reference/memalloc
        make ARCH=arm64

* 2.2 Build Test
        export CMBASE=~/omx_il/vc8000e #if ench2_asic_model.a_hevc is in CMBASE/system/models
(a)build libh2enc.a

        cd vc8000e/software/linux_reference/
        make versatile
        > libh2enc.a

(a)hevc_testenc

        cd test/hevc/
        make versatile
        > hevc_testenc

(b) jpeg_testenc

        cd test/jpeg/
        make versatile
        > hevc_testenc

3. OpenMX project
-------------------

* 3.1 OMX_IL decoder:	

        cd ~/omx_il/project.omx/openmax_il/source/decoder
(0). Find libvc8kd.a

	> DECODER_RELEASE=~/omx_il/vc8000d
        cp ~/omx_il/vc8000d/cmodel_lib_arm64/libvc8kd.a $(DECODER_RELEASE)/system/models/libvc8kd.a

(1). build libraries

        build_dec_libs.sh arm_pclinux 64
        > ~/omx_il/vc8000d/out/arm_linux/debug/avsdec,g2dec, jpegdec, libdwl.a ....

(2). build OMX IL Decoder component

        #vi vc8000d/software/linux/dwl/dwl_linux.h: #include "../pcilinux/hantrovcmd.h"
        make arm_pclinux
        > libOMX.hantro.VC8000D.video.decoder.so
        > libOMX.hantro.VC8000D.image.decoder.so

* 3.2 OMX_IL encoder

        cd ~/omx_il/project.omx/openmax_il/source/encoder
        vi Makefile: ENCODER_BASE_PATH = /data/cj.chang/omx_il_g/vc8000e
        make arm ENCODER_API_VERSION=vc8000e
        > libOMX.hantro.H2.video.encoder.so
        > libOMX.hantro.H2.image.encoder.so


## Test
--------


## History:
---------
2020/05/26

    Duplicated from 20200521 released packages:

        - _SW_VC8000E_OMX_ctrlSW_CmodelLib_CL244132_266903_20200514.tar
        - _SW_VPU_VC8000D_OMX_IL_CL267662_266903_20200513.tar

2020/06/01

   Update cache driver in decoder.


