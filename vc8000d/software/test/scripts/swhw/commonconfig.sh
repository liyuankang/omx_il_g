#!/bin/bash
#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Hantro Products Oy.                             --
#-                                                                            --
#-                   (C) COPYRIGHT 2011 HANTRO PRODUCTS OY                    --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------
#-
#--   Abstract     : Common configuration script for the execution of         --
#--                  the SW/HW  verification test cases. To be used on set-up -- 
#--                  time.                                                    --
#--                                                                           --
#-------------------------------------------------------------------------------

#=====--------------------------------------------------------------------=====#

#=====---- Common configuration parameters for various sub configurations
# Product
PRODUCT="g1"

# Component versions
export SWTAG="sw${PRODUCT}"
export HWBASETAG="hw${PRODUCT}"
export HWCONFIGTAG="hw${PRODUCT}"

# Test cases restrictions
FOURK_CASES_ONLY="DISABLED"
#MULTICORE_CASES_ONLY="DISABLED"

# Values: DISABLED & path to decoder_sw directory
USE_DIR_MODEL="DISABLED"

# Directory where test data can be found
TEST_DATA_HOME="/export/work/testdata/${PRODUCT}_testdata"
TEST_CASE_LIST="${TEST_DATA_HOME}/testcase_list.${PRODUCT}"

# formats which are supported by the decoder (needed by external execution)
if [ "$PRODUCT" == "8170" ] || [ "$PRODUCT" == "8190" ]
then
    RUN_FORMATS_EXT="h264 mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 pp"
    
elif [ "$PRODUCT" == "9170" ]
then
    RUN_FORMATS_EXT="h264 mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx pp"
    
elif [ "$PRODUCT" == "9190" ]
then
    RUN_FORMATS_EXT="h264 svc mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx vp6 pp"

elif [ "$PRODUCT" == "g1" ]
then
    RUN_FORMATS_EXT="h264 svc mvc mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx vp6 vp7 vp8 avs webp pp"
fi

# formats which test results are checked in check all
if [ "$PRODUCT" == "8170" ] || [ "$PRODUCT" == "8190" ]
then
    CHECK_FORMATS="h264 mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 pp"
    
elif [ "$PRODUCT" == "9170" ]
then
    CHECK_FORMATS="h264 mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx pp"
    
elif [ "$PRODUCT" == "9190" ]
then
    CHECK_FORMATS="h264 svc mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx vp6 pp"

elif [ "$PRODUCT" == "g1" ]
then
    CHECK_FORMATS="h264 svc mvc mpeg4 h263 sorenson mpeg2 mpeg1 jpeg vc1 rv divx vp6 vp7 vp8 avs webp pp"
fi

# Compiler settings
#COMPILER_SETTINGS="/export/tools/i386_linux24/usr/arm/arm-2009q1-203-arm-none-linux-gnueabi-i686-pc-linux-gnu/settings.sh"
COMPILER_SETTINGS="/export/tools/i386_linux24/usr/arm/arm-2011.03-arm-none-linux-gnueabi/settings.sh"

# HW base address
HW_BASE_ADDRESS="0xC0000000"
#HW_BASE_ADDRESS="0xC4000000"
#HW_BASE_ADDRESS="0x20000000"
#HW_BASE_ADDRESS="0xFC010000"

# 2nd HW base adress (multicore), "" means no multicore
HW_BASE_ADDRESS_2=""
#HW_BASE_ADDRESS_2="0x21000000"
#HW_BASE_ADDRESS_2="0xFC020000"

MULTICORE_DECODE="DISABLED"
if [ ! -z "$HW_BASE_ADDRESS_2" ]
then
    MULTICORE_DECODE="ENABLED"
fi

# Memory base address
MEM_BASE_ADDRESS="0x02000000"
#MEM_BASE_ADDRESS="0x80000000"
#128MB for socle
#MEM_BASE_ADDRESS="0x48000000"
#192MB for socle
#MEM_BASE_ADDRESS="0x44000000"
#224MB for socle
#MEM_BASE_ADDRESS="0x42000000"
# 448MB for vexpress
#MEM_BASE_ADDRESS="0x84000000"

# Memory size in megabytes for memmalloc.
MEM_SIZE_MB="96"
#MEM_SIZE_MB="384"

# Kernel directory for kernel modules compilation
KDIR="/export/Testing/Board_Version_Control/Realview_PB/PB926EJS/SW/Linux/linux-2.6.28-arm1/v0_1/linux-2.6.28-arm1"
#KDIR="/export/Testing/Board_Version_Control/SW_Common/ARM_realview_v6/2.6.28-arm1/v0_0-v6/linux-2.6.28-arm1"
#KDIR="/export/Testing/Board_Version_Control/SW_Common/SOCLE_MDK-3D/openlinux/2.6.29/v0_5/android_linux-2.6.29"
#KDIR="/export/Testing/Board_Version_Control/SW_Common/VExpress/linux-linaro-3.2-2012.01-0"

# IRQ line; Use -1 for polling mode
IRQ_LINE="-1"
#IRQ_LINE="30"
#IRQ_LINE="72"
# socle
#IRQ_LINE="36"
# vexpress
#IRQ_LINE="70"

# Values: LITTLE_ENDIAN & BIG_ENDIAN
# System endianess
SYSTEM_ENDIANESS="LITTLE_ENDIAN"

# Value: 32_BIT & 64_BIT
# HW data bus width
HW_DATA_BUS_WIDTH="32_BIT"
#HW_DATA_BUS_WIDTH="64_BIT"

# Value: 32_BIT & 64_BIT
# Test env data bus width
TEST_ENV_DATA_BUS_WIDTH="$HW_DATA_BUS_WIDTH"
#TEST_ENV_BUS_WIDTH="32_BIT"

# Values: POLLING & IRQ
# DWL implementation to use
if [ "$IRQ_LINE" == "-1" ]
then
    DWL_IMPLEMENTATION="POLLING"
    
else
    DWL_IMPLEMENTATION="IRQ"
fi

# Values: ENABLED & DISABLED
# HW timeout support
DEC_HW_TIMEOUT="ENABLED"

# Values: ENABLED & DISABLED
# Second chrominance table
# NOTE! remember to load memaaloc with "alloc_method=2"
DEC_2ND_CHROMA_TABLE="DISABLED"

# Values: ENABLED & DISABLED
# HW operates in tiled mode
DEC_TILED_MODE="DISABLED"
if [ "$PRODUCT" == "8170" ] || [ "$PRODUCT" == "8190" ] || [ "$PRODUCT" == "9170" ] || [ "$PRODUCT" == "9190" ]
then
    DEC_TILED_MODE="DISABLED"
fi

# Values: ENABLED & DISABLED
# Build in test support
BUILD_IN_TEST="DISABLED"

# The maximum picture width for decoder (only for video) test case(s) to be executed
# Use -1 not to restrict cases to be executed
TB_MAX_DEC_OUTPUT="-1"

# The maximum picture width for post-processor test case(s) to be executed
# Use -1 not to restrict cases to be executed
TB_MAX_PP_OUTPUT="-1"

# IP address to be used in remote test execution
# LOCAL_HOST disables remote execution
TB_REMOTE_TEST_EXECUTION_IP="LOCAL_HOST"

# Values: SYSTEM & DIR
# Source for reference data
TB_TEST_DATA_SOURCE="SYSTEM"

# Values: ENABLED & DISABLED
# MD5SUM are written instead of YUV
TB_MD5SUM="ENABLED"

# Seed for stream corruption (for internal use only)
TB_SEED_RND="0"

# Changes to corrupt a bit in a stream (i.e., bit value is swapped)
# E.g., "1 : 10", 0 is for disabling  (for internal use only)
TB_STREAM_BIT_SWAP="0"

# Changes to lose a packet in a stream
# E.g., "1 : 100", 0 is for disabling  (for internal use only)
# If TB_STREAM_PACKET_LOSS != 0, TB_PACKET_BY_PACKET must be set to ENABLED
TB_STREAM_PACKET_LOSS="0"

# Values: ENABLED & DISABLED
# If ENABLED, bits in stream headers are also corrupted  (for internal use only)
# If ENABLED, TB_STREAM_BIT_SWAP must be set to != 0
TB_STREAM_HEADER_CORRUPT="DISABLED"

# Values: ENABLED & DISABLED
# If enabled, bits in end of stream headers are removed  (for internal use only)
TB_STREAM_TRUNCATE="DISABLED"

# Values: 0 - 63
# The maximum DEC_LATENCY_COMPENSATION used in random configuration
TB_MAX_DEC_LATENCY_COMPENSATION="63"

# Maximum number of pictures to be decoded (0 == do not restrict the decoding)
TB_MAX_NUM_PICS="0"

# Values: ENABLED & DISABLED
# Internal tracing support
if [ "$MULTICORE_DECODE" == "ENABLED" ]
then
    TB_INTERNAL_TEST="DISABLED"
else
    TB_INTERNAL_TEST="ENABLED"
fi

# Values: DISABLED & ENABLED
# If TB_TEST_DB is ENABLED, test results are written to data a base (in addition to CSV file)
TB_TEST_DB="ENABLED"

# Values: VERILOG or VHDL
# HW implementation language
HW_LANGUAGE="VERILOG"

# Values: AHB, OCP, or AXI
# HW bus
HW_BUS="AHB"

# CVS report
CSV_FILE_PATH="${PWD}/test_reports/"
CSV_FILE_NAME_COMMON='integrationreport_${HWBASETAG}_${SWTAG}_${USER}_${REPORTIME}'
CSV_FILE_NAME_SUFFIX=""
CSV_FILE_COMMON="${CSV_FILE_PATH}${CSV_FILE_NAME_COMMON}${CSV_FILE_NAME_SUFFIX}"

#====---- Functions to write config.sh for sub configurations
# For each sub configuration there should be a write function
integration_default()
{
    mv config.sh tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_default.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_pbyp()
{
    mv config.sh tmpcfg
    #  Write TB_PACKET_BY_PACKET
    sed 's/export TB_PACKET_BY_PACKET=.*/export TB_PACKET_BY_PACKET=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_pbyp.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_wholestrm()
{
    mv config.sh tmpcfg
    #  Write TB_WHOLE_STREAM
    sed 's/export TB_WHOLE_STREAM=.*/export TB_WHOLE_STREAM=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_wholestrm.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_nal()
{
    mv config.sh tmpcfg
    # Write TB_PACKET_BY_PACKET
    sed 's/export TB_PACKET_BY_PACKET=.*/export TB_PACKET_BY_PACKET=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_NAL_UNIT_STREAM
    sed 's/export TB_NAL_UNIT_STREAM=.*/export TB_NAL_UNIT_STREAM=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_nal.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_vc1ud()
{
    mv config.sh tmpcfg
    # Write TB_PACKET_BY_PACKET
    sed 's/export TB_PACKET_BY_PACKET=.*/export TB_PACKET_BY_PACKET=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_VC1_UD
    sed 's/export TB_VC1_UD=.*/export TB_VC1_UD=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_vc1ud.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_memext()
{
    mv config.sh tmpcfg
    # Write DEC_MEMORY_ALLOCATION
    sed 's/export DEC_MEMORY_ALLOCATION=.*/export DEC_MEMORY_ALLOCATION=EXTERNAL/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_memext.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_stride()
{
    mv config.sh tmpcfg
    # Write VP8_STRIDE
    sed 's/export TB_VP8_STRIDE=.*/export TB_VP8_STRIDE=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_stride.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_memext_stride()
{
    mv config.sh tmpcfg
    # Write DEC_MEMORY_ALLOCATION
    sed 's/export DEC_MEMORY_ALLOCATION=.*/export DEC_MEMORY_ALLOCATION=EXTERNAL/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write VP8_STRIDE
    sed 's/export TB_VP8_STRIDE=.*/export TB_VP8_STRIDE=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_memext_stride.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rlc()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export DEC_RLC_MODE_FORCED=.*/export DEC_RLC_MODE_FORCED=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rlc.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rlc_pbyp()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export DEC_RLC_MODE_FORCED=.*/export DEC_RLC_MODE_FORCED=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    #  Write TB_PACKET_BY_PACKET
    sed 's/export TB_PACKET_BY_PACKET=.*/export TB_PACKET_BY_PACKET=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rlc_pbyp.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rlc_nal()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export DEC_RLC_MODE_FORCED=.*/export DEC_RLC_MODE_FORCED=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    #  Write TB_PACKET_BY_PACKET
    sed 's/export TB_PACKET_BY_PACKET=.*/export TB_PACKET_BY_PACKET=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_NAL_UNIT_STREAM
    sed 's/export TB_NAL_UNIT_STREAM=.*/export TB_NAL_UNIT_STREAM=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rlc_nal.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rlc_wholestrm()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export DEC_RLC_MODE_FORCED=.*/export DEC_RLC_MODE_FORCED=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    #  Write TB_WHOLE_STREAM
    sed 's/export TB_WHOLE_STREAM=.*/export TB_WHOLE_STREAM=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rlc_wholestrm.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rndcfg()
{
    mv config.sh tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rndcfg.csv\"," tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_RND_CFG
    sed 's/export TB_RND_CFG=.*/export TB_RND_CFG=ENABLED/' tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rndtc()
{
    mv config.sh tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rndtc.csv\"," tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_RND_TC
    sed 's/export TB_RND_TC=.*/export TB_RND_TC=ENABLED/' tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_rndtc_rlc()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export DEC_RLC_MODE_FORCED=.*/export DEC_RLC_MODE_FORCED=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_RND_TC
    sed 's/export TB_RND_TC=.*/export TB_RND_TC=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_rndtc_rlc.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_mbuffer()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export PP_MULTI_BUFFER=.*/export PP_MULTI_BUFFER=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_mbuffer.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_mbuffer_memext()
{
    mv config.sh tmpcfg
    # Write DEC_RLC_MODE_FORCED
    sed 's/export PP_MULTI_BUFFER=.*/export PP_MULTI_BUFFER=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
     # Write DEC_MEMORY_ALLOCATION
    sed 's/export DEC_MEMORY_ALLOCATION=.*/export DEC_MEMORY_ALLOCATION=EXTERNAL/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_mbuffer_memext.csv\"," tmpcfg > tmp
    mv tmp config.sh
    return 0
}

integration_relset()
{
    mv config.sh tmpcfg
    # Write CSV_FILE
    sed "s,export CSV_FILE=.*,export CSV_FILE=\"${CSV_FILE_COMMON}_relset.csv\"," tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_RND_ALL
    sed 's/export TB_RND_ALL=.*/export TB_RND_ALL=ENABLED/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_TIMEOUT
    sed 's/export TB_TIMEOUT=.*/export TB_TIMEOUT=-1/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_TEST_EXECUTION_TIME
    sed 's/export TB_TEST_EXECUTION_TIME=.*/export TB_TEST_EXECUTION_TIME=-1/' tmpcfg > tmp
    mv tmp tmpcfg
    # Write TB_RND_TC
    sed 's/export TB_RND_TC=.*/export TB_RND_TC=ENABLED/' tmpcfg > tmp
    mv tmp config.sh
    return 0
}
