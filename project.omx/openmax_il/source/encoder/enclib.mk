LOCAL_PATH := $(call my-dir)
ENCODER_RELEASE := ../../../vc8000e_encoder/software

############################################################
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libh1enc
LOCAL_MODULE_TAGS := optional

INCLUDES := $(LOCAL_PATH)/$(ENCODER_RELEASE)

LOCAL_SRC_FILES := \
    $(ENCODER_RELEASE)/source/common/encswhwregisters.c\
    $(ENCODER_RELEASE)/source/hevc/hevcencapi.c\
    $(ENCODER_RELEASE)/source/hevc/sw_picture.c\
    $(ENCODER_RELEASE)/source/hevc/sw_parameter_set.c\
    $(ENCODER_RELEASE)/source/hevc/rate_control_picture.c\
    $(ENCODER_RELEASE)/source/hevc/sw_nal_unit.c\
    $(ENCODER_RELEASE)/source/hevc/sw_slice.c\
    $(ENCODER_RELEASE)/source/hevc/hevcSei.c\
    $(ENCODER_RELEASE)/source/hevc/sw_cu_tree.c\
    $(ENCODER_RELEASE)/source/hevc/cutreeasiccontroller.c\
    $(ENCODER_RELEASE)/source/hevc/hevcenccache.c\
    $(ENCODER_RELEASE)/source/hevc/av1_obu.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpeg.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegInit.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegCodeFrame.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegPutBits.c\
    $(ENCODER_RELEASE)/source/jpeg/JpegEncApi.c\
    $(ENCODER_RELEASE)/source/jpeg/MjpegEncApi.c\
    $(ENCODER_RELEASE)/source/hevc/sw_test_id.c\
    $(ENCODER_RELEASE)/source/common/encasiccontroller_v2.c\
    $(ENCODER_RELEASE)/source/common/encasiccontroller.c\
    $(ENCODER_RELEASE)/source/common/queue.c\
    $(ENCODER_RELEASE)/source/common/checksum.c\
    $(ENCODER_RELEASE)/source/common/crc.c\
    $(ENCODER_RELEASE)/source/common/hash.c\
    $(ENCODER_RELEASE)/source/common/sw_put_bits.c\
    $(ENCODER_RELEASE)/source/common/tools.c\
    $(ENCODER_RELEASE)/source/common/encpreprocess.c\
    $(ENCODER_RELEASE)/source/common/encinputlinebuffer.c\
    $(ENCODER_RELEASE)/source/common/error.c\
    $(ENCODER_RELEASE)/linux_reference/debug_trace/enctrace.c

LOCAL_C_INCLUDES := \
    $(INCLUDES)/inc \
    $(INCLUDES)/source/hevc \
    $(INCLUDES)/source/vp9 \
    $(INCLUDES)/source/jpeg \
    $(INCLUDES)/source/common \
    $(INCLUDES)/linux_reference/ewl \
    $(INCLUDES)/linux_reference/debug_trace \
    $(INCLUDES)/linux_reference/kernel_module \
    $(INCLUDES)/linux_reference/memalloc

LOCAL_CFLAGS += \
    -DCTBRC_STRENGTH

include $(BUILD_STATIC_LIBRARY)

############################################################