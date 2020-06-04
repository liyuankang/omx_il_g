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
#--  Abstract : Makefile for cache library
#--
#-------------------------------------------------------------------------------

# Comment/uncomment the following lines to define which control codes to 
# include in the library build

# Comment/uncomment the following line to disable/enable debugging
DEBUG=y
TRACE=y
FORCE_BUILD_32BIT=n
CWL_PRINT=n
# Add your debugging flags (or not)
ifeq ($(DEBUG),y)
  DEBFLAGS = -O0 -g -DDEBUG -D_DEBUG_PRINT -D_ASSERT_USED -Werror -Wno-unused
  
else
	
  DEBFLAGS = -O2 -DNDEBUG -Werror -Wno-unused
endif

# Architecture flags for gcc
#ARCH= 
#CROSS_COMPILE=

# C -compiler name, can be replaced by another compiler(replace gcc)
CC = $(CROSS_COMPILE)gcc

ifeq ($(shell uname -m),x86_64)
      ifneq (,$(filter $(MAKECMDGOALS),pci system versatile))
        ifeq ($(FORCE_BUILD_32BIT), y)
          export ARCH = -m32
        endif
      endif
endif



# compiler switches
CFLAGS  = $(ARCH) -Wall -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE \
          $(DEBFLAGS) $(INCLUDE)
CFLAGS += -Wsign-compare -Wmissing-field-initializers -std=c99

GCC_GTEQ_435 := $(shell expr `gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/'` \>= 40305)
ifeq "$(GCC_GTEQ_435)" "1"
    CFLAGS += -Wempty-body -Wtype-limits
endif

ifeq ($(CWL_PRINT), y)
CFLAGS += -DCWL_PRINT
endif
# Cache compiler flags

# show the path to compiler, where to find header files
INCLUDE = -I../inc -Ikernel_module -Icwl 

# list of used sourcefiles
SRC_CACHE_COMMON := cacheapi.c


SRC_CWL_ARM = cwl_common.c


SRCS += $(SRC_CACHE_COMMON)

SRCS += $(SRC_CWL_ARM)

#source search path
vpath %.c
vpath %.c ../source  ./cwl 

# name of the outputfile (library)
CACHELIB = libcache.a

# MACRO for cleaning object -files
RM  = rm -f

# MACRO for creating library that includes all the object files
AR  = $(CROSS_COMPILE)ar rcs

# object files will be generated from .c sourcefiles
OBJS    = $(SRCS:.c=.o)


#Here are rules for building codes and generating object library.
all:    tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | pci | versatile ]"
	@echo "system     - PC system model (== pclinux)"
	@echo "pci        - FPGA HW with PCIE"
	@echo "versatile  - ARM versatile with FPGA HW"
	@echo "NOTE! Make sure to do 'make clean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

.PHONY: pclinux system versatile clean tags depend

ifeq ($(USE_ONL_SIM),y)
system: CFLAGS += -DASIC_ONL_SIM
INCLUDE += -I../../../software/linux/dwl/dwl_dpi
INCLUDE += -I../../../software/source/common
INCLUDE += -I../../../software/source/inc
INCLUDE += -I/ux/Tools/cad/syn/vcs_vK-2015.09-SP2/include
INCLUDE += -I../../../software/test/common/swhw
ifeq ($(USE_ASIC_TRACE), y)
system: CFLAGS += -DASIC_TRACE_SUPPORT
endif
endif

system: CFLAGS += -DCACHE_SHAPER_SIM
system: $(CACHELIB)

pci: CROSS_COMPILE=
pci: ENVSET  = -DPC_PCI_FPGA_DEMO
pci: CFLAGS  += $(TRACEFLAGS)
pci: $(CACHELIB)

versatile: ENVSET =

#versatile: CROSS_COMPILE = arm-none-linux-gnueabi-
#versatile: ARCH = -mcpu=arm926ej-s -mtune=arm926ej-s
versatile: $(CACHELIB)

pcdemo: CFLAGS  = -O2 -g -Wall -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE \
                  -DDEBUG -D_ASSERT_USED $(INCLUDE) 
pcdemo: $(SRC_CACHE_COMMON:.c=.o)
	$(AR) $(ENCLIB) $+


$(CACHELIB): $(OBJS)
	$(AR) $(CACHELIB) $(OBJS)

%.o: %.c
	$(CC) -fPIC -c $(CFLAGS) $(ENVSET) $(CONFFLAGS) $< -o $@

clean:
	$(RM) $(CACHELIB)
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda       

tags: 
	ctags ../inc/*h ../source/*[ch] 
	#../source/camstab/*[ch]\
            
 
depend .depend: $(SRCS)
	$(CC) $(INCLUDE) $(ENVSET) $(CONFFLAGS) -M  $^ > .depend

ifneq (clean, $(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif
