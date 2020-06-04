#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Verisilicon.					                          --
#-                                                                            --
#-                   (C) COPYRIGHT 2014 VERISILICON.                          --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------
#-
#--  Abstract : Makefile for encoder testbench
#--
#-------------------------------------------------------------------------------

# Comment/uncomment the following line to disable/enable debugging
DEBUG = y
USE_COVERAGE = n

# Include API extension and test ID for Hantro internal testing
INTERNAL_TEST = n

# set this to 'y' for Electric Fence checking
USE_EFENCE = n

cmbase = ../../../..
CMBASE = $(shell cd $(cmbase); pwd )

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O0 -g -DDEBUG -DTEST_DATA
  DEBFLAGS += -DTRACE_RECON     # deblock.yuv tiled yuv dump
else
  DEBFLAGS = -O2 -DNDEBUG
  DEBFLAGS += -DTRACE_RECON     # deblock.yuv tiled yuv dump
endif

#DEBFLAGS+=-DNO_OUTPUT_WRITE

# Architecture flags for gcc
#ARCH = 
#CROSS_COMPILE =

ifeq ($(shell uname -m),x86_64)
	ifneq (,$(findstring pclinux,$(MAKECMDGOALS)))
		export ARCH = -m32
	endif
	ifneq (,$(findstring system,$(MAKECMDGOALS)))
		export ARCH = -m32
	endif
	ifneq (,$(findstring testdata,$(MAKECMDGOALS)))
		export ARCH = -m32
	endif
	ifneq (,$(findstring eval,$(MAKECMDGOALS)))
		export ARCH = -m32
	endif
endif


# the path where to find header files
INCFLAGS = -I../../../inc -I../../../source/vp9 \
           -I../../../source/common -I../../../source/camstab \
           -I../../debug_trace

ifeq ($(USE_EFENCE), y)
        EFENCE= -DUSE_EFENCE -I/afs/hantro.com/projects/adder/users/atna/efence_2_4_13 \
                -L/afs/hantro.com/projects/adder/users/atna/efence_2_4_13 \
                -lefence -lpthread
endif

CC = $(CROSS_COMPILE)gcc

# compiler switches
CFLAGS  = $(ARCH) -Wall -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE \
           -D_FILE_OFFSET_BITS=64 $(DEBFLAGS) $(INCFLAGS)

GCC_GTEQ_440 := $(shell expr `gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/'` \>= 40400)
ifeq "$(GCC_GTEQ_440)" "1"
    CFLAGS += -Wno-unused-result
endif

# list of used sourcefiles
SRCS = vp9testbench.c vp9getoption.c

ifeq ($(INTERNAL_TEST),y)
# for internal tests
CFLAGS += -DINTERNAL_TEST
endif

ifeq ($(USE_COVERAGE), y)
  CFLAGS += -coverage -fprofile-arcs -ftest-coverage
endif

vpath %.c
vpath %.c ../../../source/vp9 

OBJS = $(SRCS:.c=.o)

# name of the control code library 
LIB = ../../libh2enc.a

# System model library
MODELLIB = $(CMBASE)/system/models/ench2_asic_model.a

# name of the output executable
TARGET = vp9_testenc

# MACRO for cleaning object -files
RM  = rm -f


#Here are rules for building codes and generating executable
all:    version.h tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | eval | versatile ]"
	@echo "system     - PC system model (== pclinux)"
	@echo "eval       - PC system model for evaluation with frame limit"
	@echo "integrator - ARM integrator with FPGA HW"
	@echo "versatile  - ARM versatile with FPGA HW"
	@echo "NOTE! Make sure to do 'make libclean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

# Use other makefiles to build the libraries
$(MODELLIB):
	$(MAKE) -w -C ../../../../system/models

$(LIB):
	$(MAKE) -w -C ../.. $(TARGETENV) INCLUDE_JPEG=n \
                INCLUDE_VIDSTAB=n USE_EFENCE=$(USE_EFENCE)

version.h: ../../../../.git/HEAD ../../../../.git/index
	echo "const char *git_version = \"$(shell git rev-parse HEAD)\";" > $@

.PHONY: eval
evaluation: eval
eval: TARGET = vp9_enc_h2_eval
eval: TARGETENV = eval
eval: CFLAGS += -DEVALUATION_LIMIT=1000 -DPSNR
eval: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(MODELLIB) $(LIB) -lm -o $(TARGET)

pclinux: system

.PHONY: system
system: TARGETENV = system
system: CFLAGS += -DPSNR -DTRUE_MSE
system: version.h .depend $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS)  $(MODELLIB) $(LIB) -lm -o $(TARGET)

.PHONY: system_multifile
system_multifile: TARGET = vp9_testenc_multifile
system_multifile: TARGETENV = system
system_multifile: CFLAGS += -DPSNR
system_multifile: CFLAGS += -DMULTIFILEINPUT
system_multifile: version.h .depend $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS)  $(MODELLIB) $(LIB) -lm -o $(TARGET)

.PHONY: system_static
system_static: TARGETENV = system
system_static: CFLAGS += -DPSNR
system_static: version.h .depend $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS)  $(MODELLIB) $(LIB) -lm -o $(TARGET) -static

.PHONY: versatile
versatile: TARGETENV = versatile
versatile: CROSS_COMPILE = arm-none-linux-gnueabi-
versatile: ARCH = -mcpu=arm926ej-s -mtune=arm926ej-s
versatile: LIB += -lpthread
versatile: $(OBJS)
	$(MAKE) -w -C ../.. $@ INCLUDE_JPEG=n INCLUDE_VIDSTAB=y \
            USE_EFENCE=$(USE_EFENCE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(EFENCE) -o $(TARGET) 

.PHONY: versatile_multifile
versatile_multifile: TARGETENV = versatile
versatile_multifile: CROSS_COMPILE = arm-none-linux-gnueabi-
versatile_multifile: ARCH = -mcpu=arm926ej-s -mtune=arm926ej-s
versatile_multifile: LIB += -lpthread
versatile_multifile: CFLAGS += -DMULTIFILEINPUT
versatile_multifile: TARGET = vp9_testenc_multifile
versatile_multifile: $(OBJS)
	$(MAKE) -w -C ../.. versatile INCLUDE_JPEG=n INCLUDE_VIDSTAB=y \
            USE_EFENCE=$(USE_EFENCE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(EFENCE) -o $(TARGET)

.PHONY: integrator
integrator: TARGETENV = integrator
integrator: CROSS_COMPILE= arm-linux-
integrator: ARCH = -mcpu=arm9tdmi -mtune=arm9tdmi
integrator: LIB += -lpthread
integrator: $(OBJS)
	$(MAKE) -w -C ../.. $@ INCLUDE_JPEG=n INCLUDE_VIDSTAB=y \
            USE_EFENCE=$(USE_EFENCE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(EFENCE) -o $(TARGET)

.PHONY: pci
pci: TARGETENV = pci
pci: CROSS_COMPILE=
pci: ARCH=
pci: CFLAGS+=  -DSDRAM_LM_BASE=$(CHW_BASE_ADDRESS) \
               -DENC_MODULE_PATH=\"/tmp/dev/hx280\" \
               -DMEMALLOC_MODULE_PATH=\"/tmp/dev/memalloc\"
pci: version.h .depend $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)


system_cov: CC = covc --retain -t!vp9testbench.c,!vp9getoption.c g++
system_cov: TARGETENV = system_cov
system_cov: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) -o $(TARGET)

        
.PHONY: clean
clean:
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda core* *~ $(TARGET) $(TARGET).* version.h

.PHONY: libclean
libclean: clean
	$(MAKE) -w -C ../.. clean

.PHONY: tags
tags:
	ctags ../../../inc/*.h *.c ../../../source/vp9/*.[ch] \
            ../../../source/common/*.[ch] ../../ewl/*c
            
depend .depend: $(SRCS)
	@echo -e "\n [DEP]\t $^"
	@$(CC) -M $(DEBFLAGS) $(INCFLAGS) $^ > .depend

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif

