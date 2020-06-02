LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    HantroOMXPlugin.cpp

#    stagefright_overlay_output.cpp \
#    HantroHardwareRenderer.cpp \

LOCAL_CFLAGS := $(PV_CFLAGS_MINUS_VISIBILITY)

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/frameworks/native/include/media/hardware \
    $(HANTRO_TOP)/source

#        $(TOP)/frameworks/base/include/media/stagefright/openmax

LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libui                   \
        libdl

#        libsurfaceflinger_client \
#        libstagefright_color_conversion \
#        liblog

LOCAL_MODULE := libstagefrighthw

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

