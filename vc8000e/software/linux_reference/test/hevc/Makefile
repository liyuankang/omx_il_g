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

include ../../../build/globaldefs
include ../../../build/globalrules

cmbase = ../../../..
CMBASE = $(shell cd $(cmbase); pwd )

# the path where to find header files
INCFLAGS = -I../../../inc
ifeq ($(TRACE),y)
  INCFLAGS += -I../../debug_trace \
      -I../../../source/common \
      -I../../../source/camstab \
      -I../../../source/hevc
else ifeq ($(INTERNAL_TEST),y)
  INCFLAGS += -I../../../source/common \
      -I../../../source/hevc
endif

ifneq ($(target),)
  ifneq ($(target),"default")
    CFLAGS += -DTARGET -D$(target)
  endif
endif

# list of used sourcefiles
SRCS = test_bench.c test_bench_utils.c get_option.c test_bench_pic_config.c

ifeq ($(INTERNAL_TEST),y)
# for internal tests
#SRCS += HEVCencapi_ext.c
endif

vpath %.c
vpath %.c ../../../source/hevc 

# name of the control code library 
LIB = ../../libh2enc.a

# System model library
MODELLIB = $(CMBASE)/system/models/ench2_asic_model.a

# name of the output executable
TARGET = hevc_testenc


#Here are rules for building codes and generating executable
all:    $(VERSION_H) tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | eval | versatile | android ]"
	@echo "system     - PC system model (== pclinux)"
	@echo "eval       - PC system model for evaluation with frame limit"
	@echo "pci        - PC with FPGA HW"
	@echo "integrator - ARM integrator with FPGA HW"
	@echo "versatile  - ARM versatile with FPGA HW"
	@echo "android    - ARM android"
	@echo "NOTE! Make sure to do 'make libclean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

# Use other makefiles to build the libraries
$(MODELLIB):
	$(MAKE) -w -C ../../../../system/models

$(LIB):
	$(MAKE) -w -C ../.. $(TARGETENV) \
                PCIE_FPGA_VERI_LINEBUF=$(PCIE_FPGA_VERI_LINEBUF)

version.h:../../../../.git/HEAD ../../../../.git/index
	echo "const char *git_version = \"$(shell git rev-parse HEAD)\";" > $@



.PHONY: eval
evaluation: eval
eval: TARGET = hevc_enc_h2_eval
eval: TARGETENV = eval
eval: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) -lm -lpthread -o $(TARGET)

pclinux: system

.PHONY: system
system: TARGETENV = system
system: $(VERSION_H) .depend $(MODELLIB) $(LIB) $(OBJS) $(CACHELIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(CACHELIB) -lm -lpthread -o $(TARGET)

.PHONY: system_multifile
system_multifile: TARGET = hevc_testenc_multifile
system_multifile: TARGETENV = system
system_multifile: $(VERSION_H) .depend $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) -lm -lpthread -o $(TARGET)

.PHONY: system_static
system_static: TARGETENV = system
system_static: $(VERSION_H) .depend $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) -lm -lpthread -o $(TARGET) -static

.PHONY: versatile
versatile: TARGETENV = versatile
versatile: $(VERSION_H) $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)

.PHONY: versatile_multifile
versatile_multifile: TARGETENV = versatile
versatile_multifile: TARGET = hevc_testenc_multifile
versatile_multifile: $(OBJS)
	$(MAKE) -w -C ../.. versatile \
            CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)

.PHONY: integrator
integrator: TARGETENV = integrator
integrator: $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)

.PHONY: pci
pci: TARGETENV = pci
pci: $(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)


system_cov: CC = covc --retain -t!HEVCTestBench.c,!EncGetOption.c g++
system_cov: TARGETENV = system_cov
system_cov: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) -o $(TARGET)

.PHONY: android
android: TARGETENV = android
android: $(VERSION_H) $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) ARCH="$(ARCH)"
	$(CC) $(CFLAGS) $(OBJS)  $(LIB)  -o $(TARGET)
        
.PHONY: clean
clean:
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda core* *~ $(TARGET) $(TARGET).* $(VERSION_H)

.PHONY: libclean
libclean: clean
	$(MAKE) -w -C ../.. clean

.PHONY: tags
tags:
	ctags ../../../inc/*.h *.c ../../../source/hevc/*.[ch] \
            ../../../source/common/*.[ch] ../../ewl/*c
            
depend .depend: $(SRCS)
	@echo -e "\n [DEP]\t $^"
	@$(CC) -M $(DEBFLAGS) $(INCFLAGS) $^ > .depend

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif

