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
#--  Abstract : Makefile for encoder library
#--
#-------------------------------------------------------------------------------

include ../build/globaldefs
include ../build/globalrules

cmbase = ../..
CMBASE = $(shell cd $(cmbase); pwd )

ifeq (pci,$(findstring pci, $(MAKECMDGOALS)))
# this is just for DEMO
include Baseaddress
endif

# System model library
MODELLIB = ../../system/models/ench2_asic_model.a

# show the path to compiler, where to find header files
INCLUDE = -I../source/hevc -I../source/vp9 -I../source/jpeg -I../source/camstab -I../inc -I../source/common \
          -Idebug_trace
ifeq (pci,$(findstring pci, $(MAKECMDGOALS)))
  INCLUDE += -Ikernel_module/pcie -Imemalloc/pcie
else
  INCLUDE += -Ikernel_module -Imemalloc
endif

# list of used sourcefiles
SRC_ENC_COMMON := encasiccontroller_v2.c \
                  encasiccontroller.c \
                  queue.c \
                  checksum.c \
                  crc.c \
                  hash.c \
            	  sw_put_bits.c \
                  tools.c \
                  encpreprocess.c \
                  encdec400.c \
                  rate_control_picture.c \
                  encinputlinebuffer.c \
                  error.c

SRC_HEVC := hevcencapi.c\
            sw_picture.c \
            sw_parameter_set.c \
            sw_nal_unit.c \
            sw_slice.c \
            hevcSei.c \
            sw_cu_tree.c \
            cutreeasiccontroller.c \
            hevcenccache.c \
            av1_obu.c \
            vp9_bitstream.c \
            vp9_api.c \
            vp9_subexponential.c \
            av1_prob_init.c


SRC_VP9  := vp9codeframe.c\
            vp9init.c\
            vp9putbits.c\
            vp9header.c\
            vp9picturebuffer.c\
            vp9picparameterset.c\
            vp9entropy.c\
            vp9macroblocktools.c\
            vp9ratecontrol.c\
            vp9encapi.c

SRC_JPEG := EncJpeg.c\
            EncJpegInit.c\
            EncJpegCodeFrame.c\
            EncJpegPutBits.c\
            JpegEncApi.c \
            MjpegEncApi.c

SRC_VS   := vidstabalg.c\
            vidstabapi.c\
            vidstabcommon.c\
            vidstabinternal.c


SRC_TRACE = enctrace.c

#test_data.c

# Source files for test case specific test IDs, compiler flag INTERNAL_TEST
# If these are not included some tests will fail

SRC_TESTING = sw_test_id.c

ifeq ($(INCLUDE_VP9),y)
SRC_TESTING += vp9testid.c
endif

SRC_EWL_PC := ewl_x280_file.c

SRC_EWL_ARM = ewl_x280_common.c ewl_linux_lock.c

ifeq ($(POLLING),y)
    SRC_EWL_ARM += ewl_x280_polling.c
    CFLAGS += -DENCH2_IRQ_DISABLE=1
else
    SRC_EWL_ARM += ewl_x280_irq.c
endif

SRC_EWL_SYS := ewl_system.c

SRCS = encswhwregisters.c

# common parts only for encoder and not for video stab
INCLUDE_ENC_COMMON=y


# Combine the lists of all the source files included in the library build
ifeq ($(INCLUDE_HEVC),y)
    SRCS += $(SRC_HEVC)
endif
ifeq ($(INCLUDE_VP9),y)
    SRCS += $(SRC_VP9)
endif
ifeq ($(INCLUDE_JPEG),y)
    SRCS += $(SRC_JPEG)
endif
ifeq ($(INCLUDE_VS),y)
    SRCS += $(SRC_VS)
    CFLAGS += -DVIDEOSTAB_ENABLED
endif
ifeq ($(INTERNAL_TEST),y)
    SRCS += $(SRC_TESTING)
endif

# add common encoder files
ifeq ($(INCLUDE_ENC_COMMON),y)
    SRCS += $(SRC_ENC_COMMON)
endif

# if tracing flags are defined we need to compile the tracing functions
ifeq ($(TRACE),y)
    SRCS += $(SRC_TRACE)
endif

ifneq ($(target),)
  ifneq ($(target),"default")
    CFLAGS += -DTARGET -D$(target)
  endif
endif

# choose EWL source, system model uses its own EWL
ifneq (,$(findstring pclinux, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring system, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring testdata, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring eval, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl/system
    EWL_DIR = system
else
    SRCS += $(SRC_EWL_ARM)
    INCLUDE += -Iewl
    EWL_DIR =
endif
endif
endif
endif

#source search path
vpath %.c
vpath %.c ../source/common ../source/hevc ../source/vp9 ../source/jpeg ../source/camstab ./ewl/$(EWL_DIR) ./debug_trace

# name of the outputfile (library)
ENCLIB = libh2enc.a
ENCSLIB = libh2enc.so

#Here are rules for building codes and generating object library.
all:    tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | testdata | versatile | integrator | android]"
	@echo "system     - PC system model (== pclinux)"
	@echo "testdata   - PC system model for test data creation"
	@echo "eval       - PC system model for evaluation with frame limit"
	@echo "versatile  - ARM versatile with FPGA HW"
	@echo "integrator - ARM integrator with FPGA HW"
	@echo "android    - ARM android"
	@echo "NOTE! Make sure to do 'make clean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

.PHONY: pclinux system_lib system testdata integrator versatile android clean tags depend

evaluation: eval
eval: system

pclinux: testdata
system_static: testdata
system: testdata

# for libva
system_lib: system

testdata: .depend $(ENCLIB)

integrator: $(ENCLIB)
versatile: $(ENCLIB)

android: $(ENCLIB)

versatile_tb: versatile
	$(MAKE) -w -C test/vp9 versatile
	$(MAKE) -w -C test/hevc versatile



pcdemo: $(SRC_HEVC:.c=.o) $(SRC_ENC_COMMON:.c=.o)
	$(AR) $(ENCLIB) $+


.PHONY: pci
pci:
pci: $(ENCLIB) $(ENCSLIB)


system_cov: $(ENCLIB)
system_cov: CC = covc --retain -t!debug_trace/enctrace.c,!debug_trace/enctracestream.c g++


$(ENCLIB): $(OBJS)
	$(AR) $(ENCLIB) $(OBJS)

$(ENCSLIB): $(OBJS)
	$(CC) -fPIC -Wl,-no-undefined -shared $(CFLAGS) $(OBJS) -lm -lpthread -o libh2enc.so

%.o: %.c
	$(CC) -c $(CFLAGS) $(ENVSET) $(CONFFLAGS) $< -o $@

clean:
	$(RM) $(ENCLIB)
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda

tags:
	ctags ../inc/*h ../source/common/*[ch] ../source/hevc/*[ch] ../source/vp9/*[ch]
	#../source/camstab/*[ch]\


lint: $(SRC_HEVC) $(SRC_VP9) $(SRC_JPEG) $(SRC_VS) # take out any not relevant sources
        # -e537 removes warnings about multiple include
        # -e641 removes warnings about converting enum to int
	-lint-nt -w2 -e537 -e641 $(LINT_DEF) $(INCLUDE) $^ > pc-lint-report.txt

depend .depend: $(SRCS)
	$(CC) $(INCLUDE) $(ENVSET) $(CONFFLAGS) -M  $^ > .depend

ifneq (clean, $(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif
