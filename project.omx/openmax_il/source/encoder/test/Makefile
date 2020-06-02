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

# If the machine is 64-bit and we are compiling for PC, force 32-bit compilation
ifeq ($(shell uname -m),x86_64)
#       ifneq (,$(findstring arm,$(MAKECMDGOALS)))
                export M32 = -m32
#       endif
endif

CC=gcc# arm-none-linux-gnueabi-gcc
#CFLAGS =-mtune=arm926ej-s -fpic -DMEMALLOCHW

CFLAGS=-DTEST_PATH='"."'
CFLAGS+=-O2 -g -I"../.." -I"../../../headers" $(M32)
LDFLAGS=-Wl,-R -Wl,".." -Wl,-rpath-link -Wl,".." $(M32)

INSTALL_DIR=.


all: video_encoder image_encoder

clean: 
	rm -f $(OBJS) video_encoder image_encoder

SRCS = video_encoder.c image_encoder.c
OBJS = $(SRCS:.c=.o)


video_encoder: video_encoder.o ../libhantrovideoenc.so
	$(CC) $(LDFLAGS) -Wl,--start-group $^ -lrt -Wl,--end-group -pthread -o $@

image_encoder: image_encoder.o ../libhantroimageenc.so
	$(CC) $(LDFLAGS) -Wl,--start-group $^ -lrt -Wl,--end-group -pthread -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

install:
	cp video_encoder image_encoder $(INSTALL_DIR)

