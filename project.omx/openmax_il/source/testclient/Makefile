#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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
ENCODER_API_VERSION = vc8000e
#ENCODER_API_VERSION = h2v41
#ENCODER_API_VERSION = h2
#ENCODER_API_VERSION = h1v7
#ENCODER_API_VERSION = h1v6
#ENCODER_API_VERSION = h1v5
#ENCODER_API_VERSION = h1
#ENCODER_API_VERSION = 8290
#ENCODER_API_VERSION = 7280

ifeq ($(ENCODER_API_VERSION), vc8000e)
	ENCODER_BASE_PATH = ../../../../../H2/VC8000E6.2_DEV
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
	CFLAGS += -DENCVC8000E
else ifeq ($(ENCODER_API_VERSION), h2v41)
	ENCODER_BASE_PATH = ../../../../../H2/H2V4.1_H264
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
	CFLAGS += -DENCH2V41
else
	ENCODER_RELEASE  = ../../../$(ENCODER_VERSION)_encoder
	MEMALLOC_RELEASE = ../../../$(ENCODER_VERSION)_encoder/software/linux_reference/memalloc
endif

BELLAGIO_ROOT = ../../../libomxil-bellagio-0.9.3
INSTALL_DIR = $(BELLAGIO_ROOT)/build/bin

MEMALLOC_RELEASE = ../../../vc8000e_encoder/software/linux_reference/memalloc

## Define encoder product version
#CFLAGS += -DENCH2
#CFLAGS += -DENCH1
#CFLAGS += -DENCVC8000E
#CFLAGS += -DENC8290
#CFLAGS += -DENC7280

## Default build is for 32-bit platform
## Set this to 'y' when target is 64-bit platform
64BIT_BUILD = n

# If the machine is 64-bit and we are compiling for PC, force 32-bit compilation
ifeq ($(shell uname -m),x86_64)
#	ifneq (,$(findstring arm,$(MAKECMDGOALS)))
		export M32 = -m32
#	endif
endif

ifeq ($(64BIT_BUILD), y)
    export M32 = -m64
    CFLAGS += -DADDRESS_WIDTH_64
endif

CFLAGS += -g -fno-inline -W -Wall -pthread -Wall
CFLAGS += -I. \
	-I.. \
	-I../encoder/ \
	-I../../headers \
	-I$(MEMALLOC_RELEASE)

base_SRCS = ../msgque.c ../OSAL.c ../basecomp.c ../port.c ../util.c

omxdec_HDRS = omxtestcommon.h
omxdec_SRCS = omxdectest.c omxtestcommon.c
omxdec_OBJS = $(base_SRCS:.c=.o) $(omxdec_SRCS:.c=.o)

omxenc_HDRS = omxencparameters.h omxtestcommon.h
omxenc_SRCS = omxencparameters.c omxenctest.c omxtestcommon.c
omxenc_OBJS = $(base_SRCS:.c=.o) $(omxenc_SRCS:.c=.o)

all:
	@echo ""
	@echo "MAKE for OpenMAX IL Decoder Component Test Client"
	@echo ""
	@echo " make [target] [variables]"
	@echo ""
	@echo "MAKE for OpenMAX IL Encoder Component Test Client"
	@echo ""
	@echo " make [target] ENCODER_API_VERSION=[version] [variables]"
	@echo ""
	@echo " Available targets:"
	@echo ""
	@echo "  pclinux          build image AND video decoder & encoder omxil test client for HW model testing"
	@echo "  arm              build image AND video decoder & encoder omxil test client for ARM platform"
	@echo "  install          installs omxil component with Bellagio"
	@echo "  clean            deletes generated output"
	@echo ""
	@echo " Available encoder api versions:"
	@echo ""
	@echo "  vc8000e {default}"
	@echo "  h2v41"
	@echo "  h2"
	@echo "  h1v7"
	@echo "  h1v6"
	@echo "  h1v5"
	@echo "  h1"
	@echo "  8290"
	@echo "  7280"
	@echo ""
	@echo " Available variables:"
	@echo ""
	@echo "  64BIT_BUILD = [y/n] -- enables or disables 64-bit compiling"
	@echo ""
	@echo " Current settings:"
	@echo ""
	@echo "  Encoder API Version:           $(ENCODER_API_VERSION)"
	@echo "  Encoder Directory:             $(ENCODER_RELEASE)"
	@echo "  Bellagio Root:                 $(BELLAGIO_ROOT)"
	@echo "  Install Dir:                   $(INSTALL_DIR)"
	@echo "  64BIT_BUILD:                   $(strip $(64BIT_BUILD))"
	@echo ""


.PHONY: pclinux
pclinux: CC = gcc
pclinux: CFLAGS +=  $(M32)
pclinux: BELLAGIO_LIB = $(BELLAGIO_ROOT)/build/lib/libomxil-bellagio.a
pclinux: omxdectest omxenctest

.PHONY: arm
arm: CC = arm-none-linux-gnueabi-gcc
arm: CFLAGS += -march=armv6
arm: BELLAGIO_LIB = $(BELLAGIO_ROOT)/arm/lib/libomxil-bellagio.a
arm: omxdectest omxenctest

clean:
	rm -f $(omxdec_OBJS) $(omxenc_OBJS) omxenctest omxdectest \
	rm -f $(INSTALL_DIR)/omxenctest


install: omxdectest omxenctest
	cp -vf omxenctest $(INSTALL_DIR)
	cp -vf omxdectest $(INSTALL_DIR)

omxdectest: $(omxdec_OBJS)
	$(CC) -o omxdectest $(omxdec_OBJS) $(BELLAGIO_LIB) -ldl -lpthread $(M32)

omxenctest: $(omxenc_OBJS)
	$(CC) -o omxenctest $(omxenc_OBJS) $(BELLAGIO_LIB) -ldl -lpthread $(M32)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@


