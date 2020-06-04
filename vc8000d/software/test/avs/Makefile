#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
#-         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
#-         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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
#--  Abstract : Makefile for decoder testbench
#--
#-------------------------------------------------------------------------------

# Open below compiling options by default
USE_64BIT_ENV = y
USE_NON_BLOCKING = y
USE_ONE_THREAD_WAIT = y

#source search path
vpath %.c
SRCS = ../common/command_line_parser.c avsdectest.c
OBJS    = $(SRCS:.c=.o)

INCLUDE = -I../../source/inc -I../../source/avs/ -I../../source/config \
          -I../../source/common  -I../common/swhw -I../common/utils \
          -I../../../system/models -I../common

DECLIBDIR = -L../../linux/avs -L../../linux/dwl -L../common/swhw \
            -L../../../system/models/g1hw -L../common/utils -L../common/
DECLIB =  libdecx170a.a
DWLLIB = libdwlx170.a
TBLIB = libtbcommon.a
MODLIB = lib8170hw.a

CACHE_LIB =

TARGET_ENV=

ARCH ?=
CROSS ?=

ifeq ($(USE_MD5SUM), y)
        MD5SUM = -DMD5SUM
endif

ifeq ($(shell uname -m),x86_64)
	ifneq (,$(findstring pclinux,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
	ifneq (,$(findstring coverage,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
	ifneq (,$(findstring verification,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
endif

# set for 64bit compilation
ifeq ($(USE_64BIT_ENV), y)
	export M32 = -m64
	CFLAGS += -DUSE_64BIT_ENV
endif

CFLAGS += -DFIFO_DATATYPE=addr_t
export FIFO_DATATYPE = addr_t

export CLEAR_HDRINFO_IN_SEEK
ifeq ($(CLEAR_HDRINFO_IN_SEEK), y)
        CFLAGS+=-DCLEAR_HDRINFO_IN_SEEK
endif

export ASIC_TRACE_SUPPORT
ifeq ($(ASIC_TRACE_SUPPORT), y)
	CFLAGS += -DASIC_TRACE_SUPPORT
endif

export USE_PICTURE_DISCARD
ifeq ($(USE_PICTURE_DISCARD), y)
	CFLAGS += -DUSE_PICTURE_DISCARD
endif

export USE_NON_BLOCKING
ifeq ($(USE_NON_BLOCKING), y)
	CFLAGS += -DGET_FREE_BUFFER_NON_BLOCK
endif

export USE_ONE_THREAD_WAIT
ifeq ($(USE_ONE_THREAD_WAIT), y)
	CFLAGS += -DGET_OUTPUT_BUFFER_NON_BLOCK
endif

export SUPPORT_CACHE
ifeq ($(SUPPORT_CACHE), y)
#       CACHE_LIB = ../../../cache/software/linux_reference/libcache.a
        CACHE_LIB = $(CACHE_DIR)/software/linux_reference/libcache.a
        CFLAGS+=-DSUPPORT_CACHE
endif
all: versatile

.PHONY: versatile
versatile: CC = $(CROSS)gcc
versatile: CFLAGS += -O -g -fpic $(ARCH) $(INCLUDE) $(MD5SUM)  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
versatile: TESTDEC=ax170dec_versatile
versatile: TARGET_ENV=versatile
versatile: LIBS = $(DECLIBDIR) -lpthread -ldecx170a -ldwlx170 -ltbcommon -lutils $(EFENCE) $(MD5SUM) $(CACHE_LIB)
versatile: test

.PHONY: pcie
pcie: CFLAGS += -O -g -fpic $(INCLUDE) $(MD5SUM) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
pcie: TESTDEC=ax170dec_pcie
pcie: TARGET_ENV=pcie
pcie: LIBS = $(DECLIBDIR) -pthread -ldecx170a -ldwlx170 -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
pcie: test


.PHONY: integrator
integrator: CC=arm-linux-gcc
integrator: CFLAGS = -O -g -mtune=arm920tdmi -fpic  $(INCLUDE) $(MD5SUM)
integrator: TESTDEC=ax170dec_integrator
integrator: TARGET_ENV=integrator
integrator: LIBS = $(DECLIBDIR) -lpthread -ldecx170a -ldwlx170 -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
integrator: test


.PHONY: ads11
ads11: CC=armcc
ads11: CFLAGS = -g --cpu ARM1136J-S --no_inline -O3 -Otime $(INCLUDE) -DADS_PERFORMANCE_SIMULATION
ads11: TESTDEC=ax170dec_ads11
ads11: TARGET_ENV=ads11
ads11: LIBS = -L../../linux/avs/$(DECLIB) -L../../linux/dwl/$(DWLLIB) -L../../../system/models/$(MODLIB) -L../common/swhw/$(TBLIB) -D_ADS $(CACHE_LIB)
ads11: test

.PHONY: ads9
ads9: CC=armcc
ads9: CFLAGS = -g --cpu ARM926EJ-S --no_inline -O3 -Otime $(INCLUDE) -DADS_PERFORMANCE_SIMULATION
ads9: TESTDEC=ax170dec_ads9
ads9: TARGET_ENV=ads9
ads9: LIBS = -L../../linux/avs/$(DECLIB) -L../../linux/dwl/$(DWLLIB) -L../../../system/models/$(MODLIB) -L../common/swhw/$(TBLIB) -D_ADS $(CACHE_LIB)
ads9: test

.PHONY: ads
ads: CC=armcc
ads: CFLAGS = -g --cpu ARM1136J-S --no_inline -O3 -Otime $(INCLUDE) -DADS_PERFORMANCE_SIMULATION
ads: TESTDEC=ax170dec_ads
ads: TARGET_ENV=ads
ads: LIBS = -L../../linux/avs/$(DECLIB) -L../../linux/dwl/$(DWLLIB) -L../../../system/models/$(MODLIB) -L../common/swhw/$(TBLIB) -D_ADS $(CACHE_LIB)
ads: test

.PHONY: verification
verification: SYSTEM_VERIFICATION_TEST = -DSYSTEM_VERIFICATION
verification: pclinux

.PHONY: pclinux
pclinux: CC=gcc
pclinux: CFLAGS+= -g $(INCLUDE) $(MD5SUM) $(M32) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -DEXPIRY_DATE=1$(expiry) -DMODEL_SIMULATION
pclinux: TESTDEC=ax170dec_pclinux
pclinux: TARGET_ENV=pclinux
pclinux: LIBS = $(DECLIBDIR) -pthread -ldecx170a -ldwlx170 -l8170hw -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
pclinux: test

.PHONY: arm_pclinux
arm_pclinux: CC=aarch64-linux-gnu-gcc
arm_pclinux: CFLAGS+= -g $(INCLUDE) $(MD5SUM) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -DEXPIRY_DATE=1$(expiry)
arm_pclinux: TESTDEC=ax170dec_arm_pclinux
arm_pclinux: TARGET_ENV=arm_pclinux
arm_pclinux: LIBS = $(DECLIBDIR) -pthread -ldecx170a -ldwlx170 -l8170hw -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
arm_pclinux: test

.PHONY: coverage
coverage: CC=covc --abs --retain gcc
coverage: CFLAGS = -O -g $(INCLUDE) $(MD5SUM) $(M32)
coverage: TESTDEC=ax170dec_coverage
coverage: TARGET_ENV=coverage
coverage: LIBS = $(DECLIBDIR) -ldecx170a -ldwlx170 -l8170hw -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
coverage: test

.PHONY: pclinux_eval_G1
pclinux_eval_G1: CFLAGS += -DAVS_EVALUATION_G1
pclinux_eval_G1: pclinux_eval

.PHONY: pclinux_eval
pclinux_eval: CC=gcc
pclinux_eval: CFLAGS += $(INCLUDE) $(MD5SUM) $(M32) -DAVS_EVALUATION -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -DEXPIRY_DATE=1$(expiry)
pclinux_eval: TESTDEC=ax170dec_pclinux_eval
pclinux_eval: TARGET_ENV=pclinux_eval
pclinux_eval: LIBS = $(DECLIBDIR) -ldecx170a -ldwlx170 -l8170hw -ltbcommon -lutils $(EFENCE) $(CACHE_LIB)
pclinux_eval: DEBFLAGS = -O3 -DNDEBUG
pclinux_eval: test

export SYSTEM_VERIFICATION_TEST
export CACHE_DIR
test: lib  $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(TESTDEC)

lib:
	make -C ../../linux/avs $(TARGET_ENV)
	make -C ../../linux/dwl $(TARGET_ENV)
	make -C ../common/swhw $(TARGET_ENV)
	make -C ../common/utils $(TARGET_ENV)

clean:
	rm -f $(OBJS)
	rm -f .depend
	rm -f ax170dec_*

libclean:
	make -C ../../linux/dwl clean
	make -C ../../linux/avs clean
	make -C ../common/swhw clean
	make -C ../common/utils clean

depend .depend dep: $(SRCS)
	$(CC) $(CFLAGS) -M  $^ > .depend

ifeq (.depend, $(wildcard .depend))
include .depend
endif
