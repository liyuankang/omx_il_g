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

BELLAGIO_ROOT = ../../../libomxil-bellagio-0.9.3
INSTALL_DIR = $(BELLAGIO_ROOT)/build/lib/bellagio

## Set correct encoder API version
#ENCODER_API_VERSION = vc8000e
ENCODER_API_VERSION = h2v41
#ENCODER_API_VERSION = h2
#ENCODER_API_VERSION = h1v7
#ENCODER_API_VERSION = h1v6
#ENCODER_API_VERSION = h1v5
#ENCODER_API_VERSION = h1
#ENCODER_API_VERSION = 8290
#ENCODER_API_VERSION = 7280


## Default build is for 32-bit platform
## Set this to 'y' when target is 64-bit platform
64BIT_BUILD = y

# Enable C-Model to have tracefile generated
#  Used for HW verification purpose
TRACE = y

## Enable Jpeg encoder
CODECS += -DENABLE_JPEG

## Debug trace flags
#CFLAGS += -DENABLE_DBGT_TRACE
CFLAGS += -DDBGT_CONFIG_AUTOVAR 

ifeq ($(TRACE),y)
  CFLAGS += -DTEST_DATA
endif

## Use EWL for linear memory allocation
CFLAGS += -DUSE_EWL

## OMX tunneling support
#CFLAGS +=-DOMX_ENCODER_TUNNELING_SUPPORT

## Enable only for OMX IL conformance testing
#CFLAGS += -DCONFORMANCE

## Enable if encoder is not able to write output stream directly to output buffer
#CFLAGS += -DUSE_TEMP_OUTPUTBUFFER

## Video stabilization requires two consecutive input pictures. The first is
## encoded and the second picture goes to stabilization.
## Enable if the component has to wait for the second buffer from client
#CFLAGS += -DUSE_STAB_TEMP_INPUTBUFFER

## These flags disable dynamic calculation of OMX port buffer size.
## Default sizes are defined in codec.h
#CFLAGS += -DUSE_DEFAULT_INPUT_BUFFER_SIZE
#CFLAGS += -DUSE_DEFAULT_OUTPUT_BUFFER_SIZE

# If the machine is 64-bit and we are compiling for PC, force 32-bit compilation
ifeq ($(shell uname -m),x86_64)
	ifeq (,$(findstring pclinux,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
	ifeq (,$(findstring install,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
endif

ifeq ($(ENCODER_API_VERSION), vc8000e)
	ENCODER_BASE_PATH = /data/cj.chang/omx_il_g/vc8000e
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
else ifeq ($(ENCODER_API_VERSION), h2v41)
	ENCODER_BASE_PATH = ../../../../../H2/H2V4.1_H264
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
else
	ENCODER_RELEASE  = ../../../$(ENCODER_VERSION)_encoder
	MEMALLOC_RELEASE = ../../../$(ENCODER_VERSION)_encoder/software/linux_reference/memalloc
endif


INCDIR = software/inc

CFLAGS += -I.. \
    -I../../headers \
    -I$(ENCODER_RELEASE)/$(INCDIR) \
    -I$(MEMALLOC_RELEASE)

#    -I$(BELLAGIO_ROOT)/src \

base_SRCS = ../msgque.c ../OSAL.c ../basecomp.c ../port.c ../util.c encoder_perf.c

libhantrovideoenc_SRCS = encoder_constructor_video.c
libhantroimageenc_SRCS = encoder_constructor_image.c

ifeq ($(ENCODER_API_VERSION), vc8000e)
	ENCODER_VERSION = h2
	CFLAGS += -DENCVC8000E
	libhantrovideoenc_SRCS += encoder_vc8000e.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so
	libhantroimageenc_SRCS += encoder_jpeg_vc8000e.c
	IMAGELIB = libOMX.hantro.H2.image.encoder.so

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h2v41)
	ENCODER_VERSION = h2
	CFLAGS += -I$(ENCODER_RELEASE)/software/source/common
	CFLAGS += -I$(ENCODER_RELEASE)/software/source/hevc
	CFLAGS += -DENCH2V41
	libhantrovideoenc_SRCS += encoder_h2v41.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h2)
	ENCODER_VERSION = h2
	CFLAGS += -DENCH2
	libhantrovideoenc_SRCS += encoder_hevc.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v7)
	ENCODER_VERSION = h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V7
	libhantrovideoenc_SRCS += encoder_vp8.c encoder_h264.c
	libhantroimageenc_SRCS += encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v6)
	ENCODER_VERSION = h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V6
	libhantrovideoenc_SRCS += encoder_vp8.c encoder_h264.c
	libhantroimageenc_SRCS += encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v5)
	ENCODER_VERSION = h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V5
	libhantrovideoenc_SRCS += encoder_vp8.c encoder_h264.c
	libhantroimageenc_SRCS += encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1)
	ENCODER_VERSION = h1
	CFLAGS += -DENCH1
	libhantrovideoenc_SRCS += encoder_vp8.c encoder_h264.c
	libhantroimageenc_SRCS += encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), 8290)
	ENCODER_VERSION = 8290
	CFLAGS += -DENC8290
	libhantrovideoenc_SRCS += encoder_h264.c
	libhantroimageenc_SRCS += encoder_jpeg.c
	VIDEOLIB = libOMX.hantro.8290.video.encoder.so
	IMAGELIB = libOMX.hantro.8290.image.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), 7280)
	ENCODER_VERSION = 7280
	CFLAGS += -DENC7280
	libhantrovideoenc_SRCS += encoder_mpeg4.c encoder_h263.c encoder_h264.c
	libhantroimageenc_SRCS += encoder_jpeg.c
	VIDEOLIB = libOMX.hantro.7280.video.encoder.so
	IMAGELIB = libOMX.hantro.7280.image.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.
endif

ifeq ($(64BIT_BUILD), y)
export M32 = -m64
CFLAGS += -DADDRESS_WIDTH_64
endif

libhantrovideoenc_OBJS = $(base_SRCS:.c=.o) $(libhantrovideoenc_SRCS:.c=.o) encoder_video.o library_entry_point_video.o
libhantroimageenc_OBJS = $(base_SRCS:.c=.o) $(libhantroimageenc_SRCS:.c=.o) encoder_image.o library_entry_point_image.o

ENCODER_SW = $(ENCODER_RELEASE)/software/linux_reference/lib$(ENCODER_VERSION)enc.a
ENCODER_SYS = $(ENCODER_RELEASE)/system/models/enc$(ENCODER_VERSION)_asic_model.a

all:
	@echo ""
	@echo "MAKE for OpenMAX IL Encoder Component"
	@echo ""
	@echo " make [target] ENCODER_API_VERSION=[verion] [variables]"
	@echo ""
	@echo " Available targets:"
	@echo ""
	@echo "  pclinux          build image AND video encoder omxil component for HW model testing"
	@echo "  arm              build image AND video encoder omxil component for ARM platform"
	@echo "  install          installs omxil component with Bellagio"
	@echo "  clean            deletes generated output"
	@echo ""
	@echo " Available versions:"
	@echo ""
	@echo "  vc8000e"
	@echo "  h2v41 {default}"
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
pclinux: CFLAGS += $(M32) -g -O0 -finline-functions -W -Wno-inline -Wall -fPIC -Wunused-variable -DNDEBUG -pthread
pclinux: LDFLAGS = $(M32)
pclinux: HANTRO_LIBS = $(ENCODER_SW) $(ENCODER_SYS)
pclinux: videolib imagelib

.PHONY: arm
arm: CC = aarch64-linux-gnu-gcc
#arm: CFLAGS += -O2 -mtune=arm926ej-s -fpic -DMEMALLOCHW
#arm: LDFLAGS = -Wl
arm: HANTRO_LIBS = $(ENCODER_SW)
arm: videolib imagelib
ifeq ($(ENCODER_API_VERSION), h2)
install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)
	
else ifeq ($(ENCODER_API_VERSION), vc8000e)

install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	cp -vf $(IMAGELIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)
	rm -f $(libhantroimageenc_OBJS) $(IMAGELIB) $(INSTALL_DIR)/$(IMAGELIB)
	
else ifeq ($(ENCODER_API_VERSION), h2v41)

install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)

else

install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	cp -vf $(IMAGELIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)
	rm -f $(libhantroimageenc_OBJS) $(IMAGELIB) $(INSTALL_DIR)/$(IMAGELIB)
#	make -C $(ENCODER_RELEASE)/system $@

endif

#$(ENCODER_SW):
#	make -C $(ENCODER_RELEASE)/system system

#$(ENCODER_SYS):
#	make -C $(ENCODER_RELEASE)/system system

videolib: $(libhantrovideoenc_OBJS)
	echo "[L] $@"
	$(CC) $(LDFLAGS) -o $(VIDEOLIB) -Wl,-h -Wl,$(VIDEOLIB) -shared -Wl,--start-group $(libhantrovideoenc_OBJS) $(HANTRO_LIBS) -lrt -Wl,--end-group -pthread -lm

ifeq ($(ENCODER_API_VERSION), h2)
imagelib:
else ifeq ($(ENCODER_API_VERSION), h2v41)
imagelib:
else ifeq ($(ENCODER_API_VERSION), vc8000e)
imagelib: $(libhantroimageenc_OBJS)
	echo "[L] $@"
	$(CC) $(LDFLAGS) -o $(IMAGELIB) -Wl,-h -Wl,$(IMAGELIB) -shared -Wl,--start-group $(libhantroimageenc_OBJS) $(HANTRO_LIBS) -lrt -Wl,--end-group -pthread -lm
else
imagelib: $(libhantroimageenc_OBJS)
	echo "[L] $@"
	$(CC) $(LDFLAGS) -o $(IMAGELIB) -Wl,-h -Wl,$(IMAGELIB) -shared -Wl,--start-group $(libhantroimageenc_OBJS) $(HANTRO_LIBS) -lrt -Wl,--end-group -pthread -lm
endif


encoder_video.o: encoder.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_VIDEO_DOMAIN -c $< -o $@

encoder_image.o: encoder.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_IMAGE_DOMAIN -c $< -o $@

library_entry_point_video.o: library_entry_point.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_VIDEO_DOMAIN -c $< -o $@

library_entry_point_image.o: library_entry_point.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_IMAGE_DOMAIN -c $< -o $@

%.o : %.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -c $< -o $@
