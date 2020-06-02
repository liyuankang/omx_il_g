#-------------------------------------------------------------------------------
#- Copyright (c) 2019, VeriSilicon Inc. or its affiliates. All rights reserved--
#                                                                             --
#- Permission is hereby granted, free of charge, to any person obtaining a    --
#- copy of this software and associated documentation files (the "Software"), --
#- to deal in the Software without restriction, including without limitation  --
#- the rights to use copy, modify, merge, publish, distribute, sublicense,    --
#- and/or sell copies of the Software, and to permit persons to whom the      --
#- Software is furnished to do so, subject to the following conditions:       --
#-                                                                            --
#- The above copyright notice and this permission notice shall be included in --
#- all copies or substantial portions of the Software.                        --
#-                                                                            --
#- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR --
#- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   --
#- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE--
#- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER     --
#- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    --
#- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        --
#- DEALINGS IN THE SOFTWARE.                                                  --
#-------------------------------------------------------------------------------
#-------------------------------------------------------------------------------

# Maybe the FreeRTOS Kernel has already been installed on the coreessponding hardware platform,
# so could still need the following header declare and c source code
FREERTOS_KERNEL_DIR = software/linux/dwl/osal/freertos/FreeRTOS_Kernel/Source
#should add the related portable for your platform
INCLUDE += -I$(FREERTOS_KERNEL_DIR)/include \
           -I$(FREERTOS_KERNEL_DIR)/portable/GCC/Posix

#FreeRTOS kernel, should add the related portable for your platform
FREERTOS_C_SRCS += $(FREERTOS_KERNEL_DIR)/croutine.c \
                   $(FREERTOS_KERNEL_DIR)/event_groups.c \
                   $(FREERTOS_KERNEL_DIR)/list.c \
                   $(FREERTOS_KERNEL_DIR)/queue.c \
                   $(FREERTOS_KERNEL_DIR)/stream_buffer.c \
                   $(FREERTOS_KERNEL_DIR)/tasks.c \
                   $(FREERTOS_KERNEL_DIR)/timers.c \
                   $(FREERTOS_KERNEL_DIR)/portable/GCC/Posix/port.c \
                   $(FREERTOS_KERNEL_DIR)/portable/MemMang/heap_3.c \
				   $(FREERTOS_KERNEL_DIR)/portable/GCC/Posix/stub_function.c

# list of used header files
# for FreeRTOS-Plus-POSIX/include/portable/pc/linux, should be revised according to the related platform
FREERTOS_POSIX_DIR = software/linux/dwl/osal/freertos/lib
INCLUDE += -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include/portable/pc/linux \
           -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include/portable \
           -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include \
           -I$(FREERTOS_POSIX_DIR)/include/private \
           -I$(FREERTOS_POSIX_DIR)/include

# Could contain header files about the file system, like FreeRTOS+FAT

# list of used source files
#FreeRTOS-POSIX Plus(pthread and ...)
FREERTOS_POSIX_SOURCE_DIR = $(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/source
FREERTOS_C_SRCS += $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_clock.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_mqueue.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_pthread.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_pthread_barrier.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_pthread_cond.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_pthread_mutex.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_sched.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_semaphore.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_timer.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_unistd.c \
                   $(FREERTOS_POSIX_SOURCE_DIR)/FreeRTOS_POSIX_utils.c
# Could contain used source filesabout the file system, like FreeRTOS+FAT

ifeq ($(strip $(ENV)),FreeRTOS)
  INCLUDE += -I$(FREERTOS_KERNEL_DIR)/include \
             -I$(FREERTOS_KERNEL_DIR)/portable/GCC/Posix
  OMX_LIB_FREERTOS		= libfreertos.a
endif


