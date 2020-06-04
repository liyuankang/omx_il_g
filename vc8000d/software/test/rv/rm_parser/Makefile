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


# show the path to compiler, where to find header files and libraries
INCLUDE = -Iinclude -Icommon/pub -Irv_depack -Irm_parser

# compiler switches
CFLAGS  = -g -Wall $(ARCH) -D_GNU_SOURCE $(DEBFLAGS) $(INCLUDE) $(SETTING_FLAGS) $(M32)

# list of used sourcefiles
SRCS := \
helix_date.c \
memory_utils.c \
pack_utils.c \
rm_error_default.c \
rm_io_default.c \
rm_memory_default.c \
rm_packet.c \
rm_property.c \
rm_stream.c \
stream_hdr_utils.c \
string_utils.c \
rm_parser.c \
rm_parser_internal.c \
rv_decode.c \
rv_depack.c \
rv_depack_internal.c

#source search path
vpath %.c
vpath %.c common rm_parser rv_decode rv_depack

vpath %.o
vpath %.o obj

# object files will be generated from .c sourcefiles
OBJS    = $(SRCS:.c=.o)

# name of the outputfile (library)
DECLIB = librvparser.a

# MACRO for cleaning object -files
RM  = rm -f

# MACRO for creating library that includes all the object files
#AR  = ar rcv
CROSS ?=
ARCH ?=
CC = $(CROSS)gcc
AR  = $(CROSS)ar rcs


OBJDIR := obj

#Here are rules for building codes and generating object library.
all: $(DECLIB)

.PHONY: versatile
versatile: CFLAGS += -O -g -fpic $(INCLUDE) $(MD5SUM) $(ARCH)
versatile: M32=
versatile: $(DECLIB)

.PHONY: pcie
pcie: $(DECLIB)

.PHONY: integrator
integrator: CC=arm-linux-gcc
integrator: CFLAGS = -O -g -mtune=arm920tdmi -fpic  $(INCLUDE) $(MD5SUM)
integrator: $(DECLIB)


.PHONY: ads11
ads11: CC=armcc
ads11: CFLAGS = -g --cpu ARM1136J-S --no_inline -O3 -Otime -DRVDEC_EXTERNAL_ALLOC_DISABLE $(INCLUDE)
ads11: $(DECLIB)

.PHONY: ads9
ads9: CC=armcc
ads9: CFLAGS = -g --cpu ARM926EJ-S --no_inline -O3 -Otime -DRVDEC_EXTERNAL_ALLOC_DISABLE $(INCLUDE)
ads9: $(DECLIB)

.PHONY: verification
verification: $(DECLIB)

.PHONY: coverage
coverage: $(DECLIB)

.PHONY: pclinux
pclinux: $(DECLIB)

.PHONY: arm
arm: CROSS_COMPILER = aarch64-linux-gnu-
arm: CC = aarch64-linux-gnu-gcc
arm: M32=
arm: $(DECLIB)

.PHONY: pclinux_eval
pclinux_eval: $(DECLIB)

$(OBJDIR):
	mkdir $@

$(DECLIB): depend $(OBJDIR) $(OBJS)
	$(AR) $(DECLIB) $(patsubst %,$(OBJDIR)/%, $(OBJS))

$(OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $(ENVSET) $< -o $(OBJDIR)/$@


.PHONY: clean
clean:
	$(RM) $(DECLIB)
	$(RM) .depend
	$(RM) -r $(OBJDIR)

depend: $(SRCS)
	$(CC) $(CFLAGS) -M  $^ > .depend

#ifeq (.depend, $(wildcard .depend))
#include .depend
#endif

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif
