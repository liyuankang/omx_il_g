#!/bin/bash

DEC_PATH=../../../../vc8000d
if [ $# -eq 0 ]
  then
    echo ""
    echo " Usage: build_dec_libs.sh <target options>"
    echo ""
    echo " Available targets:"
    echo "  clean       clean build"
    echo "  pclinux     build decoder libraries with PP support for SW model testing"
    echo "  arm_pclinux build decoder libraries with PP support for SW model testing at arm platform"
    echo "  arm_linux   build decoder libraries with PP support for arm platform"
    echo ""
    echo " Available options:"
    echo "  64          64-bit environment"
    echo "  rel         release mode"
    echo ""
    exit 1
fi

USE_64BIT_ENV=n
RELEASE=n

if [ "$2" == "64" ] || [ "$3" == "64" ] ; then
    USE_64BIT_ENV=y
fi

if [ "$2" == "rel" ] || [ "$3" == "rel" ] ; then
    RELEASE=y
fi

build_option="USE_OMXIL_BUFFER=y USE_64BIT_ENV=$USE_64BIT_ENV RELEASE=$RELEASE CLEAR_HDRINFO_IN_SEEK=y"

if [ "$1" == "pclinux" ] || [ "$1" == "arm_pclinux" ] ; then
    build_option+=" USE_MODEL_LIB=libvc8kd.a MODEL_LIB_DIR=system/models"
fi

if [ "$1" == "clean" ] ; then
    rm ${DEC_PATH}/out -rf
fi

if [ "$1" == "pclinux" ] ; then
    make -C ${DEC_PATH} g2dec    ENV=x86_linux $build_option
    make -C ${DEC_PATH} mpeg2dec ENV=x86_linux $build_option
    make -C ${DEC_PATH} mpeg4dec ENV=x86_linux $build_option CUSTOM_FMT_SUPPORT=y
    make -C ${DEC_PATH} vp6dec   ENV=x86_linux $build_option
    make -C ${DEC_PATH} vp8dec   ENV=x86_linux $build_option
    make -C ${DEC_PATH} vc1dec   ENV=x86_linux $build_option
    make -C ${DEC_PATH} rvdec    ENV=x86_linux $build_option
    make -C ${DEC_PATH} avsdec   ENV=x86_linux $build_option
    make -C ${DEC_PATH} jpegdec  ENV=x86_linux $build_option
    make -C ${DEC_PATH} ppdec    ENV=x86_linux $build_option
fi

if [ "$1" == "arm_pclinux" ] ; then
    make -C ${DEC_PATH} g2dec    ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} mpeg2dec ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} mpeg4dec ENV=arm_pclinux $build_option CUSTOM_FMT_SUPPORT=y
    make -C ${DEC_PATH} vp6dec   ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} vp8dec   ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} vc1dec   ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} rvdec    ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} avsdec   ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} jpegdec  ENV=arm_pclinux $build_option
    make -C ${DEC_PATH} ppdec    ENV=arm_pclinux $build_option
fi

if [ "$1" == "arm_linux" ] ; then
    make -C ${DEC_PATH} g2dec    ENV=arm_linux $build_option
    make -C ${DEC_PATH} mpeg2dec ENV=arm_linux $build_option
    make -C ${DEC_PATH} mpeg4dec ENV=arm_linux $build_option CUSTOM_FMT_SUPPORT=y
    make -C ${DEC_PATH} vp6dec   ENV=arm_linux $build_option
    make -C ${DEC_PATH} vp8dec   ENV=arm_linux $build_option
    make -C ${DEC_PATH} vc1dec   ENV=arm_linux $build_option
    make -C ${DEC_PATH} rvdec    ENV=arm_linux $build_option
    make -C ${DEC_PATH} avsdec   ENV=arm_linux $build_option
    make -C ${DEC_PATH} jpegdec  ENV=arm_linux $build_option
    make -C ${DEC_PATH} ppdec    ENV=arm_linux $build_option
fi
