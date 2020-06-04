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

G1_SRCS += $(DWL_SRCS)
G1_SRCS += $(COMMON_SRCS)
G1_SRCS += $(TEST_SRCS)
#G1_SRCS += $(SYS_SRCS)
G1_SRCS += $(TBCOMMON_SRCS)

G2_SRCS += $(DWL_SRCS)
G2_SRCS += $(COMMON_SRCS)
G2_SRCS += $(TEST_SRCS)

OMX_LIB_DWL             = libdwl.a
OMX_LIB_COMMON          = libcommon.a
OMX_LIB_TBCOMMON        = libtbcommon.a
OMX_LIB_G1_AVS          = libdecavs.a
OMX_LIB_G1_H264         = libdech264.a
OMX_LIB_G1_JPEG         = libdecjpeg.a
OMX_LIB_G1_MPEG2        = libdecmpeg2.a
OMX_LIB_G1_MPEG4        = libdecmpeg4.a
OMX_LIB_G1_RV           = libdecrv.a
OMX_LIB_G1_VC1          = libdecvc1.a
OMX_LIB_G1_VP6          = libdecvp6.a
OMX_LIB_G1_VP8          = libdecvp8.a
OMX_LIB_VC8000D_HW      = libvc8kd.a

OMX_LIB_G2_HEVC         = libdechevc.a
OMX_LIB_G2_VP9          = libdecvp9.a
OMX_LIB_G2_H264         = libdech10p.a
OMX_LIB_G2_AVS2         = libdecavs2.a
OMX_LIB_PP              = libpp.a

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  OMX_LIB_FREERTOS		= libfreertos.a
endif

ifeq ($(findstring arm,$(ENV)), arm)
    RV_TARGET=arm
else
    RV_TARGET=
endif

ifeq ($(findstring avsdec,$(MAKECMDGOALS)),avsdec)
  G1_SRCS += $(AVS_SRCS)
  DEC_SRCS += software/test/avs/avsdectest.c
  LIBS += $(OMX_LIB_G1_AVS)
  SHARED_LIBS += $(OMX_LIB_G1_AVS)
  SHARED_LIB = lib_vc8000d_avs.so
endif
ifeq ($(findstring h264dec,$(MAKECMDGOALS)),h264dec)
  G1_SRCS += $(H264_SRCS)
  DEC_SRCS += software/test/h264high/bytestream_parser.c \
              software/test/h264high/libav-wrapper.c
  ifeq ($(findstring h264dec_osfree,$(MAKECMDGOALS)),h264dec_osfree)
    DEC_SRCS += software/test/h264high/osfreedectestbench.c
  else
    DEC_SRCS += software/test/h264high/dectestbench.c
  endif
  LIBS += $(OMX_LIB_G1_H264)
  SHARED_LIBS += $(OMX_LIB_G1_H264)
  SHARED_LIB = lib_vc8000d_h264.so
  # libav stuff
  ifeq ($(USE_LIBAV), y)
    PKG_CONFIG_LIBAV_DIR=$(LIBAV)
    PKG_CONFIG_LIBDIR="$(LIBAV)/lib/pkgconfig"
    LDFLAGS += -L$(LIBAV)/lib
    CFLAGS += -I$(LIBAV)/include
    LIBAV_CFLAGS = $(shell PKG_CONFIG_LIBAV_DIR="$(PKG_CONFIG_LIBAV_DIR)" PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" pkg-config --cflags libavcodec libavformat libavutil)
    LIBAV_LIBS = $(shell PKG_CONFIG_LIBAV_DIR="$(PKG_CONFIG_LIBAV_DIR)" PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" pkg-config --libs libavcodec libavformat libavutil)
  endif
endif
ifeq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
  G1_SRCS += $(JPEG_SRCS)
  G1_SRCS += $(UT_SRCS)
  DEC_SRCS += software/test/jpeg/dectestbench.c
  LIBS += $(OMX_LIB_G1_JPEG)
  SHARED_LIBS += $(OMX_LIB_G1_JPEG)
  SHARED_LIB = lib_vc8000d_jpeg.so
endif

ifeq ($(findstring mpeg4dec,$(MAKECMDGOALS)),mpeg4dec)
  G1_SRCS += $(MPEG4_SRCS)
  DEC_SRCS += software/test/mpeg4/mpeg4dectest.c \
              software/test/mpeg4/rm_length.c
  LIBS += $(OMX_LIB_G1_MPEG4)
  SHARED_LIBS += $(OMX_LIB_G1_MPEG4)
  SHARED_LIB = lib_vc8000d_mpeg4.so
endif

ifeq ($(findstring rvdec,$(MAKECMDGOALS)),rvdec)
  G1_SRCS += $(RV_SRCS)
  DEC_SRCS += software/test/rv/hantro_rv_test.c
  LIBS += $(OMX_LIB_G1_RV)
  SHARED_LIBS += $(OMX_LIB_G1_RV)
  SHARED_LIB = lib_vc8000d_rv.so
  LIBS += librvparser.a
  LDFLAGS += -Lsoftware/test/rv/rm_parser


ifeq ($(USE_64BIT_ENV), y)
    export M32 = -m64
else
    export M32 = -m32
endif
ifeq ($(STATIC), n)
    export SETTING_FLAGS = -fPIC
endif

librvparser.a:
	make -C software/test/rv/rm_parser clean
	make -C software/test/rv/rm_parser $(RV_TARGET)
endif

ifeq ($(SUPPORT_DEC400), y)
  DEFINES += -DSUPPORT_DEC400
endif

ifeq ($(findstring vc1dec,$(MAKECMDGOALS)),vc1dec)
  G1_SRCS += $(VC1_SRCS)
  DEC_SRCS += software/test/vc1/dectestbench.c
  LIBS += $(OMX_LIB_G1_VC1)
  SHARED_LIBS += $(OMX_LIB_G1_VC1)
  SHARED_LIB = lib_vc8000d_vc1.so
endif

ifeq ($(findstring vp6dec,$(MAKECMDGOALS)),vp6dec)
  G1_SRCS += $(VP6_SRCS)
  DEC_SRCS += software/test/vp6/on2vp6dec.c
  LIBS += $(OMX_LIB_G1_VP6)
  SHARED_LIBS += $(OMX_LIB_G1_VP6)
  SHARED_LIB = lib_vc8000d_vp6.so
endif

ifeq ($(findstring vp8dec,$(MAKECMDGOALS)),vp8dec)
  G1_SRCS += $(VP8_SRCS)
  DEC_SRCS += software/test/vp8/dectestbench.c \
              software/test/vp8/ivf.c \
              software/test/vp8/vp8filereader.c \
              software/test/vp8/vp8writeoutput.c \
              software/test/vp8/vp8bufferalloc.c
  LIBS += $(OMX_LIB_G1_VP8)
  SHARED_LIBS += $(OMX_LIB_G1_VP8)
  SHARED_LIB = lib_vc8000d_vp8.so
endif

ifeq ($(findstring ppdec,$(MAKECMDGOALS)),ppdec)
  G2_SRCS += $(PP_SRCS)
  LIBS += $(OMX_LIB_PP)
  SHARED_LIBS += $(OMX_LIB_PP)
  SHARED_LIB = lib_vc8000d_pp.so
endif

ifeq ($(findstring hevcdec_tb,$(MAKECMDGOALS)),hevcdec_tb)
  G2_SRCS += $(HEVC_SRCS)
  DEFINES += -DENABLE_HEVC_SUPPORT
else
ifeq ($(findstring hevcdec,$(MAKECMDGOALS)),hevcdec)
  G2_SRCS += $(HEVC_SRCS)
  LIBS += $(OMX_LIB_G2_HEVC)
  DEFINES += -DENABLE_HEVC_SUPPORT
  SHARED_LIBS += $(OMX_LIB_G2_HEVC)
  SHARED_LIB = lib_vc8000d_hevc.so
else
ifeq ($(findstring vp9dec_tb,$(MAKECMDGOALS)),vp9dec_tb)
  G2_SRCS += $(VP9_SRCS)
  DEFINES += -DENABLE_VP9_SUPPORT
else
ifeq ($(findstring vp9dec,$(MAKECMDGOALS)),vp9dec)
  G2_SRCS += $(VP9_SRCS)
  LIBS += $(OMX_LIB_G2_VP9)
  DEFINES += -DENABLE_VP9_SUPPORT
  SHARED_LIBS += $(OMX_LIB_G2_VP9)
  SHARED_LIB = lib_vc8000d_vp9.so
else
ifeq ($(findstring avs2dec,$(MAKECMDGOALS)),avs2dec)
  G2_SRCS += $(AVS2_SRCS)
  LIBS += $(OMX_LIB_G2_AVS2)
  DEFINES += -DENABLE_AVS2_SUPPORT
  SHARED_LIBS += $(OMX_LIB_G2_AVS2)
  SHARED_LIB = lib_vc8000d_avs2.so
endif # avs2dec
endif # vp9dec
endif # vp9dec_tb
endif # hevcdec
endif # hevcdec_tb

# Avoid the confliction: mpeg2dec, g2dec have same suffix "g2dec"
ifeq ($(findstring mpeg2dec,$(MAKECMDGOALS)),mpeg2dec)
  G1_SRCS += $(MPEG2_SRCS)
  DEC_SRCS += software/test/mpeg2/mpeg2dectest.c
  LIBS += $(OMX_LIB_G1_MPEG2)
  SHARED_LIBS += $(OMX_LIB_G1_MPEG2)
  SHARED_LIB = lib_vc8000d_mpeg2.so
else
ifeq ($(findstring g2dec,$(MAKECMDGOALS)),g2dec)
  G2_SRCS += $(HEVC_SRCS)
  G2_SRCS += $(VP9_SRCS)
  G2_SRCS += $(H264_SRCS)
  G2_SRCS += $(AVS2_SRCS)

  LIBS += $(OMX_LIB_G2_HEVC)
  LIBS += $(OMX_LIB_G2_VP9)
  LIBS += $(OMX_LIB_G2_H264)
  LIBS += $(OMX_LIB_G2_AVS2)

  SHARED_LIBS += $(OMX_LIB_G2_HEVC)
  SHARED_LIBS += $(OMX_LIB_G2_VP9)
  SHARED_LIBS += $(OMX_LIB_G2_H264)
  SHARED_LIBS += $(OMX_LIB_G2_AVS2)
  SHARED_LIB = lib_vc8000d_h26x.so

ifeq ($(DISABLE_HEVC),n)
  DEFINES += -DENABLE_HEVC_SUPPORT
endif
ifeq ($(DISABLE_VP9),n)
  DEFINES += -DENABLE_VP9_SUPPORT
endif
ifeq ($(DISABLE_H264),n)
  DEFINES += -DENABLE_H264_SUPPORT
endif
ifeq ($(DISABLE_AVS2),n)
  DEFINES += -DENABLE_AVS2_SUPPORT
endif

# libav stuff
ifeq ($(USE_LIBAV), y)
  PKG_CONFIG_LIBAV_DIR=$(LIBAV)
  PKG_CONFIG_LIBDIR="$(LIBAV)/lib/pkgconfig"
  LDFLAGS += -L$(LIBAV)/lib
  CFLAGS += -I$(LIBAV)/include
  LIBAV_CFLAGS = $(shell PKG_CONFIG_LIBAV_DIR="$(PKG_CONFIG_LIBAV_DIR)" PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" pkg-config --cflags libavcodec libavformat libavutil)
  LIBAV_LIBS = $(shell PKG_CONFIG_LIBAV_DIR="$(PKG_CONFIG_LIBAV_DIR)" PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" pkg-config --libs libavcodec libavformat libavutil)
endif
endif
endif

ifeq ($(strip $(USE_ONL_SIM)),y)
ifeq ($(strip $(SUPPORT_DEC400)),y)
    LDFLAGS += -Ldec400/sample/VSICompress -lstdc++ -ldec400c
endif
endif

G2_SRCS += $(TB_COMMON)

ifeq ($(findstring hevcdec_tb,$(MAKECMDGOALS)),hevcdec_tb)
  DEC_TB_SRCS = software/test/hevc/dectestbench.c
else
  ifeq ($(findstring vp9dec_tb,$(MAKECMDGOALS)),vp9dec_tb)
    DEC_TB_SRCS = software/test/vp9/dectestbench.c
  else
    ifeq ($(findstring ppdec,$(MAKECMDGOALS)),ppdec)
      DEC_TB_SRCS = software/test/common/md5_sink.c \
                    software/test/common/swhw/tb_out.c \
                    software/test/pp/external_mode/pptestbench.c
    else
      DEC_TB_SRCS = software/test/common/bytestream_parser.c \
                    software/test/common/error_simulator.c \
                    software/test/common/file_sink.c \
                    software/test/common/md5_sink.c \
                    software/test/common/null_sink.c \
                    software/test/common/vpxfilereader.c \
                    software/test/common/yuvfilters.c \
                    software/test/common/libav-wrapper.c \
                    software/test/common/dectestbench.c
    endif
    ifeq ($(strip $(USE_SDL)),y)
      DEC_TB_SRCS += software/test/common/sdl_sink.c
    endif
    ifeq ($(strip $(USE_ASIC_TRACE)),y)
      DEC_TB_SRCS += software/test/common/simulation_trace.c
    else
      DEC_TB_SRCS += software/test/common/null_trace.c
    endif
  endif
endif

DEFINES += -DFIFO_DATATYPE=void*

ifeq ($(strip $(ENV)),FreeRTOS) 
  override G2_SRCS:=$(filter-out $(AVS2_SRCS),$(G2_SRCS))
endif
SRCS += $(G1_SRCS)
SRCS += $(G2_SRCS)

G1_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(G1_SRCS:.c=.o)))
DEC_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(DEC_SRCS:.c=.o)))

G2_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(G2_SRCS:.c=.o)))
DEC_TB_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(DEC_TB_SRCS:.c=.o)))

LIBS += $(OMX_LIB_DWL)
LIBS += $(OMX_LIB_COMMON)
SHARED_LIBS += $(OMX_LIB_DWL)
SHARED_LIBS += $(OMX_LIB_COMMON)

ifneq ($(strip $(USE_MODEL_LIB)),)
LDFLAGS += -L$(dir $(USE_MODEL_LIB))
  MODEL_LIB += $(patsubst lib%, -l%, $(basename $(notdir $(USE_MODEL_LIB))))
endif

LIBS += $(OMX_LIB_TBCOMMON)

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  LIBS += $(OMX_LIB_FREERTOS)
  SHARED_LIBS += $(OMX_LIB_FREERTOS)
endif

#for split the library building with the testbench's
ifeq ($(strip $(ENV)),FreeRTOS)  
  LIBS_FORMAT += $(OMX_LIB_G2_HEVC)
  LIBS_FORMAT += $(OMX_LIB_G2_VP9)
  LIBS_FORMAT += $(OMX_LIB_G2_H264)
  LIBS_FORMAT += $(OMX_LIB_G1_JPEG)
  LIBS += $(OMX_LIB_FREERTOS)
  SHARED_LIBS += $(OMX_LIB_FREERTOS)

  override LIBS:=$(filter-out $(OMX_LIB_G2_AVS2),$(LIBS))
  override SHARED_LIBS:=$(filter-out $(OMX_LIB_G2_AVS2),$(SHARED_LIBS))
  
  override LDFLAGS:=$(filter-out -pthread,$(LDFLAGS))
  override CFLAGS:=$(filter-out -pedantic,$(CFLAGS))
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
endif

###############################################################################
# lib generation
###############################################################################

libg1hw.a: $(MODEL_LIB_DIR)/$(OMX_LIB_G1_HW)

libg2hw.a: $(MODEL_LIB_DIR)/$(OMX_LIB_G2_HW)

libpphw.a: $(MODEL_LIB_DIR)/$(OMX_LIB_PP_HW)

libvc8kd.a: $(MODEL_LIB_DIR)/$(OMX_LIB_VC8000D_HW)

libdwl.a: $(sort $(patsubst %,$(OBJDIR)/%,$(DWL_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libcommon.a: $(sort $(patsubst %,$(OBJDIR)/%,$(COMMON_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecavs.a: $(sort $(patsubst %,$(OBJDIR)/%,$(AVS_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdech264.a: $(sort $(patsubst %,$(OBJDIR)/%,$(H264_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecjpeg.a: $(sort $(patsubst %,$(OBJDIR)/%,$(JPEG_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecmpeg2.a: $(sort $(patsubst %,$(OBJDIR)/%,$(MPEG2_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecmpeg4.a: $(sort $(patsubst %,$(OBJDIR)/%,$(MPEG4_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecrv.a: $(sort $(patsubst %,$(OBJDIR)/%,$(RV_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvc1.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VC1_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp6.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP6_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp8.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP8_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libtbcommon.a: $(sort $(patsubst %,$(OBJDIR)/%,$(TBCOMMON_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdechevc.a: $(sort $(patsubst %,$(OBJDIR)/%,$(HEVC_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp9.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP9_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdech10p.a: $(sort $(patsubst %,$(OBJDIR)/%,$(H264_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecavs2.a: $(sort $(patsubst %,$(OBJDIR)/%,$(AVS2_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libpp.a: $(sort $(patsubst %,$(OBJDIR)/%,$(PP_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
libfreertos.a: $(sort $(patsubst %,$(OBJDIR)/%,$(FREERTOS_C_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
endif

ifeq ($(strip $(ENV)),FreeRTOS)
libfreertos.a: $(sort $(patsubst %,$(OBJDIR)/%,$(FREERTOS_C_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
endif

.phony:lib rtos_lib
lib: $(LIBS)
ifeq ($(strip $(ENV)),FreeRTOS)
rtos_lib: $(LIBS) $(LIBS_FORMAT)
endif
###############################################################################
# execuatables
###############################################################################
avsdec h264dec h264dec_osfree jpegdec mpeg2dec mpeg4dec rvdec vc1dec vp6dec vp8dec: env depend lib $(LIBS) $(G1_OBJS) $(DEC_OBJS)
ifeq ($(STATIC), n)
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)
endif
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@echo -e "[LINK]\t$(LIBS:lib%.a=%)"


ifeq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
ifeq ($(strip $(USE_ONL_SIM)),y)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -shared -o $(OBJDIR)/$@.so
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP1]\t$(OBJDIR)/$@.so"
	@$(STRIP) --strip-all $(OBJDIR)/$@.so
endif
else
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm  $(LIBAV_LIBS) -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP2]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif
endif



ifneq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif

hevcdec vp9dec ppdec: env depend $(LIBS) $(G2_OBJS) $(DEC_TB_OBJS)
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	@echo "$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -o $(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@echo -e "[LINK]\t$(LIBS:lib%.a=%)"
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_TB_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif

hevcdec_tb: env depend $(LIBS) $(G2_OBJS) $(DEC_TB_OBJS)
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@$(CC) $(LDFLAGS) $(filter %.o, $^) $(LIBS) -lm -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif

avs2dec: env depend $(LIBS) $(TB_AVS2_OBJS)
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	$(CC) $(LDFLAGS) $(filter %.o, $^) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) -lm -Wl,--end-group -o $(OBJDIR)/$@
ifeq ($(strip $(STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@strip --strip-all $(OBJDIR)/$@
endif

vp9dec_tb: env depend $(LIBS) $(G2_OBJS) $(DEC_TB_OBJS)
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@$(CC) $(LDFLAGS) $(filter %.o, $^) $(LIBS) -lm -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif

g2dec: env depend lib $(LIBS) $(G2_OBJS) $(DEC_TB_OBJS)
ifeq ($(STATIC), n)
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	@echo "$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -o $(OBJDIR)/$(SHARED_LIB)"
	@$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)
endif
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@echo -e "[LINK]\t$(LIBS:lib%.a=%)"
ifeq ($(strip $(USE_ONL_SIM)),y)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_TB_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -shared -o $(OBJDIR)/$@.so
else
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_TB_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -o $(OBJDIR)/$@
endif
ifeq ($(strip $(SYMLINKS)),y)
	@ln -fs $@ $(OBJDIR)/hevcdec_tb
	@ln -fs $@ $(OBJDIR)/vp9dec
endif
ifeq ($(strip $(USE_STRIP)),y)
ifeq ($(strip $(USE_ONL_SIM)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@.so"
	@$(STRIP) --strip-all $(OBJDIR)/$@.so
else
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif
