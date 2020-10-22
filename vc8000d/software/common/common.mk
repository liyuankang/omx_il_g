#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
#-         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
#-                                                                            --
#- This software is confidential and proprietary and may be used only as      --
#-   expressly authorized by VeriSilicon in a written licensing agreement.    --
#-                                                                            --
#-         This entire notice must be reproduced on all copies                --
#-                       and may not be removed.                              --
#-                                                                            --
#-------------------------------------------------------------------------------
#- Redistribution and use in source and binary forms, with or without         --
#- modification, are permitted provided that the following conditions are met:--
#-   * Redistributions of source code must retain the above copyright notice, --
#-       this list of conditions and the following disclaimer.                --
#-   * Redistributions in binary form must reproduce the above copyright      --
#-       notice, this list of conditions and the following disclaimer in the  --
#-       documentation and/or other materials provided with the distribution. --
#-   * Neither the names of Google nor the names of its contributors may be   --
#-       used to endorse or promote products derived from this software       --
#-       without specific prior written permission.                           --
#-------------------------------------------------------------------------------
#- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
#- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
#- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
#- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
#- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
#- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
#- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
#- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
#- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
#- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
#- POSSIBILITY OF SUCH DAMAGE.                                                --
#-------------------------------------------------------------------------------
#-----------------------------------------------------------------------------*/

#
# Abstract : Makefile containing common settings and rules used to compile
#            Hantro G2 decoder software.

# General off-the-shelf environment settings
ENV ?= x86_linux  # default
RESOLUTION_1080P ?= n
STATIC ?= n
USE_CUSTOM_FMT ?= n
# Randomly corrupt RFC buffer/table for HW robustness testing
USE_RFC_CORRUPT ?= n
# Disable SW support for given format in g2dec.
# By default all these formats (HEVC/VP9/H264/AVS2) are supported.
DISABLE_HEVC ?= n
DISABLE_VP9 ?= n
DISABLE_H264 ?= n
DISABLE_AVS2 ?= n

OSWIDTH = $(shell getconf LONG_BIT)
ifeq ($(strip $(OSWIDTH)), 64)
  USE_64BIT_ENV ?= y
else
  USE_64BIT_ENV ?= n
endif

ifeq ($(strip $(ENV)),x86_linux)
  ARCH ?=
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  ifeq ($(strip $(USE_64BIT_ENV)),n)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  CFLAGS += -fpic
  USE_MODEL_SIMULATION ?= y
  DEFINES += -DFAKE_RFC_TBL_LITTLE_ENDIAN
endif
ifeq ($(strip $(ENV)),FreeRTOS)
  ARCH ?=
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  DISABLE_AVS2 = y
  USE_MODEL_SIMULATION ?= n
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/pcidriver/
  DEFINES += -DDEC_MODULE_PATH=\"/tmp/dev/hantrodec\" \
             -DMEMALLOC_MODULE_PATH=\"/tmp/dev/memalloc\"
  DEFINES += -D__FREERTOS__
  include software/linux/dwl/osal/freertos/freertos.mk
endif
ifeq ($(strip $(ENV)),arm_linux)
  ARCH ?=
  #CROSS ?= arm-none-linux-gnueabi-
  CROSS ?= aarch64-linux-gnu-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/pcidriver/
  DEFINES += -DDEC_MODULE_PATH=\"/tmp/dev/hantrodec\" \
             -DMEMALLOC_MODULE_PATH=\"/tmp/dev/memalloc\"
  USE_MODEL_SIMULATION ?= n
endif
ifeq ($(strip $(ENV)),arm_pclinux)
  ARCH ?=
  CROSS ?= aarch64-linux-gnu-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  ifeq ($(strip $(USE_64BIT_ENV)),n)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  CFLAGS += -fpic
  USE_MODEL_SIMULATION ?= y
endif
ifeq ($(strip $(ENV)),x86_linux_pci)
  ARCH ?=
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/pcidriver
  DEFINES += -DDEC_MODULE_PATH=\"/tmp/dev/hantrodec\" \
             -DMEMALLOC_MODULE_PATH=\"/tmp/dev/memalloc\"
  USE_MODEL_SIMULATION ?= n
endif

ifneq (,$(findstring h264dec_osfree,$(MAKECMDGOALS)))
  DEFINES += -DNON_PTHREAD_H
endif

ifeq ($(strip $(USE_64BIT_ENV)),y)
  DEFINES += -DUSE_64BIT_ENV
endif

ifeq ($(strip $(RESOLUTION_1080P)),y)
  DEFINES += -DRESOLUTION_1080P
endif

USE_HW_PIC_DIMENSIONS ?= n
ifeq ($(USE_HW_PIC_DIMENSIONS), y)
  DEFINES += -DHW_PIC_DIMENSIONS
endif

# Define for using prebuilt library
# If not set, build system model lib from source files.
USE_MODEL_LIB ?=

DEFINES += -DFIFO_DATATYPE=void*
# Common error flags for all targets
CFLAGS  += -Wall -ansi -std=c99 -pedantic
# DWL uses variadic macros for debug prints
CFLAGS += -Wno-variadic-macros

# Common libraries
LDFLAGS += -L$(OBJDIR) -pthread

# MACRO for cleaning object -files
RM  = rm -f

# Common configuration settings
RELEASE ?= n
USE_COVERAGE ?= n
USE_PROFILING ?= n
USE_SW_PERFORMANCE ?= n
# set this to 'y' for enabling IRQ mode for the decoder. You will need
# the hx170dec kernel driver loaded and a /dev/hx170 device node created
USE_DEC_IRQ ?= n
# set this 'y' for enabling sw reg tracing. NOTE! not all sw reagisters are
# traced; only hw "return" values.
USE_INTERNAL_TEST ?= n
# set this to 'y' to enable asic traces
USE_ASIC_TRACE ?= n
# set this to 'y' to enable on-line sim mode
USE_ONL_SIM ?= n
# set this to 'y' to enable freertos sim mode
USE_FREERTOS_SIMULATOR ?= n
# set this tot 'y' to enable webm support, needs nestegg lib
WEBM_ENABLED ?= n
NESTEGG ?= $(HOME)/nestegg
# set this to 'y' to enable SDL support, needs sdl lib
USE_SDL ?= n
SDL_CFLAGS ?= $(shell sdl-config --cflags)
SDL_LDFLAGS ?= $(shell sdl-config --libs)
USE_TB_PP ?= y
USE_FAST_EC ?= y
USE_VP9_EC ?= y
CLEAR_HDRINFO_IN_SEEK ?= n
USE_PICTURE_DISCARD ?= n
USE_RANDOM_TEST ?= n
USE_NON_BLOCKING ?= y
USE_ONE_THREAD_WAIT ?= y
USE_OMXIL_BUFFER ?= n
# set this to 'y' to enable multicore support, needs libav lib
USE_MULTI_CORE ?= n
LIBAV ?=
GEN_LIBVA_LIB ?= n
# where system model lib from/to?
MODEL_LIB_DIR ?= system/models
LDFLAGS += -L$(MODEL_LIB_DIR)
USE_VCMD ?= n
# Force RFC output of each CBS is word-aligned.
USE_RFC_WORD_ALIGN ?= n

# Flags for >2GB file support.
DEFINES += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_DWL_DEBUG
# Input buffer contains multiple frames?
# If yes, open it. If no, close it to improve sw performance.
#DEFINES += -DHEVC_INPUT_MULTI_FRM

ifeq ($(strip $(USE_SDL)),y)
  DEFINES += -DSDL_ENABLED
  CFLAGS += $(SDL_CFLAGS)
  LDFLAGS += $(SDL_LDFLAGS)
  LIBS += -lSDL -ldl -lrt -lm
endif

ifeq ($(strip $(RELEASE)),n)
  CFLAGS += -g -O0
ifeq (,$(findstring osfree, $(MAKECMDGOALS)))
  CFLAGS += -Werror
endif
  DEFINES += -DDEBUG -D_ASSERT_USED -D_RANGE_CHECK -D_ERROR_PRINT
  BUILDCONFIG = debug
  USE_STRIP ?= n
else
  CFLAGS   += -O2
  DEFINES += -DNDEBUG
  BUILDCONFIG = release
  USE_STRIP ?= y
endif

# Directory where object and library files are placed.
BUILDROOT=out
OBJDIR=$(strip $(BUILDROOT))/$(strip $(ENV))/$(strip $(BUILDCONFIG))

ifeq ($(USE_ASIC_TRACE), y)
  DEFINES += -DASIC_TRACE_SUPPORT
endif

ifeq ($(USE_ONL_SIM),y)
  DEFINES += -DASIC_ONL_SIM
ifeq ($(SUPPORT_DEC400),y)
  DEFINES += -DSUPPORT_DEC400
  INCLUDE += -Idec400/sample/VSICompress/
  LIBS    += dec400/sample/VSICompress/libdec400c.a
endif
endif

ifeq ($(USE_COVERAGE), y)
  CFLAGS += -coverage -fprofile-arcs -ftest-coverage
  LDFLAGS += -coverage
endif

ifeq ($(USE_PROFILING), y)
  CFLAGS += -pg
  LDFLAGS += -pg
endif

ifeq ($(STATIC), y)
  LDFLAGS += -static
else
  CFLAGS += -fPIC -shared
endif

ifeq ($(USE_MODEL_SIMULATION), y)
  DEFINES += -DMODEL_SIMULATION
endif

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  FreeRTOSDir = software/linux/dwl/osal/freertos/FreeRTOS_Kernel
  DEFINES += -D__FREERTOS__ -DUSE_FREERTOS_SIMULATOR
endif

ifeq ($(USE_DEC_IRQ), y)
  DEFINES +=  -DDWL_USE_DEC_IRQ
endif

ifeq ($(USE_TB_PP), y)
  DEFINES += -DTB_PP
endif

ifeq ($(CLEAR_HDRINFO_IN_SEEK), y)
  DEFINES += -DCLEAR_HDRINFO_IN_SEEK
endif

DEFINES += -DUSE_FAKE_RFC_TABLE

ifeq ($(USE_FAST_EC), y)
  DEFINES += -DUSE_FAST_EC
endif

ifeq ($(USE_PICTURE_DISCARD), y)
  DEFINES += -DUSE_PICTURE_DISCARD
endif

ifeq ($(USE_RANDOM_TEST), y)
  DEFINES += -DUSE_RANDOM_TEST
endif

ifeq ($(USE_VP9_EC), y)
  DEFINES += -DUSE_VP9_EC
endif

ifeq ($(USE_OMXIL_BUFFER), y)
  DEFINES += -DUSE_OMXIL_BUFFER
endif

ifeq ($(USE_MULTI_CORE), y)
  DEFINES += -DSUPPORT_MULTI_CORE
endif

ifeq ($(USE_RFC_CORRUPT),y)
  DEFINES += -DRANDOM_CORRUPT_RFC
endif

# If decoder can not get free buffer, return DEC_NO_DECODING_BUFFER.
ifeq ($(USE_NON_BLOCKING), y)
  DEFINES += -DGET_FREE_BUFFER_NON_BLOCK
endif

ifeq ($(USE_ONE_THREAD_WAIT), y)
  DEFINES += -DGET_OUTPUT_BUFFER_NON_BLOCK
endif

ifneq ($(DEF_HWBUILD_ID),)
  DEFINES += -DDEC_HW_BUILD_ID=0x$(DEF_HWBUILD_ID)
endif

ifeq ($(strip $(USE_VCMD)),y)
  DEFINES += -DSUPPORT_VCMD
endif

ifeq ($(strip $(USE_RFC_WORD_ALIGN)),y)
  DEFINES += -DRFC_WORD_ALIGN
endif

#DEFINES += -DENABLE_FPGA_VERIFICATION

# HW/SW performance test
#DEFINES += -DPERFORMANCE_TEST

# Update data elements from repeat sequence header and repeat sequence extension header
# This is not allowed by the mpeg2 specification
DEFINES += -DENABLE_NON_STANDARD_FEATURES

# Define the decoder output format.
# TODO(vmr): List the possible values.
DEFINES += -DDEC_X170_OUTPUT_FORMAT=0 # raster scan output

# Set length of SW timeout in milliseconds. default: infinite wait (-1)
# This is just a parameter passed to the wrapper layer, so the real
# waiting happens there and is based on that implementation
DEFINES += -DDEC_X170_TIMEOUT_LENGTH=-1

DEFINES += -DRV_RAW_STREAM_SUPPORT

ifeq ($(USE_SW_PERFORMANCE), y)
  DEFINES += -DSW_PERFORMANCE
endif

ifeq ($(DISABLE_PIC_FREEZE_FLAG), y)
  DISABLE_PIC_FREEZE=-D_DISABLE_PIC_FREEZE
endif
ifeq ($(WEBM_ENABLED), y)
  DEFINES += -DWEBM_ENABLED
  INCLUDE += -I$(NESTEGG)/include
  LIBS    += $(NESTEGG)/lib/libnestegg.a
endif

ifeq ($(SUPPORT_CACHE), y)
  DEFINES += -DSUPPORT_CACHE
  INCLUDE += -I$(CACHE_DIR)/software/linux_reference
  LIBS    += $(CACHE_DIR)/software/linux_reference/libcache.a
endif

ifeq ($(SUPPORT_DEC400), y)
  DEFINES += -DSUPPORT_DEC400
endif

ifeq ($(SUPPORT_MMU), y)
  DEFINES += -DSUPPORT_MMU
endif

ifeq ($(ENABLE_2ND_CHROMA_FLAG), y)
  ENABLE_2ND_CHROMA=-D_ENABLE_2ND_CHROMA
  CFLAGS += $(ENABLE_2ND_CHROMA)
endif

ifeq ($(DPB_REALLOC_DISABLE_FLAG), y)
   CFLAGS+=-DDPB_REALLOC_DISABLE
   DPB_REALLOC_DISABLE=-D_DPB_REALLOC_DISABLE
endif

ifeq ($(USE_EC_MC), y)
   CFLAGS+=-DUSE_EC_MC
endif

ifneq ($(TB_WATCHDOG), )
   CFLAGS+=-DTESTBENCH_WATCHDOG
endif

ifneq ($(SET_EMPTY_PICTURE_DATA),)
        DEBFLAGS += -DSET_EMPTY_PICTURE_DATA=$(SET_EMPTY_PICTURE_DATA)
endif

ifneq ($(USE_DEC_IRQ), )
ifeq ($(USE_DEC_IRQ), y)
        DEBFLAGS += -DDEC_X170_USING_IRQ=1
else
        DEBFLAGS += -DDEC_X170_USING_IRQ=0
endif
endif

# Enalbe libav when LIBAV is set in command line.
ifeq ($(strip $(LIBAV)),)
  USE_LIBAV = n
else
  USE_LIBAV = y
  DEFINES += -DUSE_LIBAV
endif

DEFINES += -DDWL_DISABLE_REG_PRINTS # Do not trace all register accesses
#DEFINES += -D_DWL_DEBUG            # DWL debug info
DEFINES += -DUSE_DDD_DEBUGGER # Enable when debugging with DDD debugger
# 2 cores supported by default
CORES ?= 2
# disable missing field initializer from causing error since it is interpreted
# wrongly by some older GCC versions.
#CFLAGS += -Wno-missing-field-initializers
DEFINES += -DCORES=$(CORES)

ifdef DWL_PRESET_FAILING_ALLOC
        DEFINES += -DDWL_PRESET_FAILING_ALLOC=$(DWL_PRESET_FAILING_ALLOC)
endif

ifeq ($(DISABLE_REFBUFFER),y)
        DEFINES += -DDWL_REFBUFFER_DISABLE # Disable reference frame buffering
endif

#set these for evaluation
ifeq ($(_PRODUCT_8170), y)
    DEFINES += -DDWL_EVALUATION_8170
endif

ifeq ($(_PRODUCT_8190), y)
    DEFINES += -DDWL_EVALUATION_8190
endif

ifeq ($(_PRODUCT_9170), y)
    DEFINES += -DDWL_EVALUATION_9170
endif

ifeq ($(_PRODUCT_9190), y)
    DEFINES += -DDWL_EVALUATION_9190
endif

ifeq ($(_PRODUCT_G1), y)
    DEFINES += -DDWL_EVALUATION_G1
endif

ifeq ($(EXTENDED_EC_TEST), y)
     DEFINES += -DEXTENDED_EC_TEST
endif

ifeq ($(ERROR_RESILIENCE),y)
  DEBFLAGS += -DJPEGDEC_ERROR_RESILIENCE
endif

DEFINES += -DWRITE_TRACES
DEFINES += -D_GNU_SOURCE
DEFINES += -DSKIP_OPENB_FRAME
DEFINES += -DENABLE_DPB_RECOVER
DEFINES += -DREORDER_ERROR_FIX
