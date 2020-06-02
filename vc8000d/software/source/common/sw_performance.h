/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef __SW_PERFORMANCE_H__
#define __SW_PERFORMANCE_H__

#ifdef __linux
#include <sys/syscall.h>
#endif
#define gettid() syscall(__NR_gettid)
#ifdef PERFORMANCE_TEST
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#define RECORD_MAX 5
#define DECLARE_PERFORMANCE_STATIC(name) \
    u32 name##_count=0; \
    u64 name##_total=0; \
    u32 name##_val[RECORD_MAX]; \
    pthread_t name##_threadid; \
    struct timeval name##_start={0}, name##_end={0}; \
    int name##thread_warning=0; \
    int name##need_continue=0; \
    int name##need_end=0;

#define PERFORMANCE_STATIC_START(name) { \
    extern u32 name##_count; \
    extern pthread_t name##_threadid; \
    extern  struct timeval name##_start; \
    extern int name##need_continue; \
    extern int name##need_end; \
    if (!name##need_continue) { \
        name##_threadid = gettid(); \
        if (0) {printf("[%s(%d)]"#name" %d start(%lu)\n",__FILE__,__LINE__,name##_count,name##_threadid);fflush(stdout);} \
        gettimeofday(&name##_start, NULL); \
        name##need_continue = 1; \
        name##need_end = 1; \
    } else { \
        if (0) {printf("[%s(%d)]"#name" continue\n",__FILE__,__LINE__);fflush(stdout);} \
    } \
    }

#define PERFORMANCE_STATIC_END(name) { \
    extern u32 name##_count; \
    extern u64 name##_total; \
    extern pthread_t name##_threadid; \
    extern u32 name##_val[]; \
    extern  struct timeval name##_start, name##_end; \
    extern int name##thread_warning; \
    extern int name##need_continue; \
    extern int name##need_end; \
    gettimeofday(&name##_end, NULL); \
    name##need_continue = 0; \
    if (!name##thread_warning && (gettid() != name##_threadid)) { \
        if (0) {printf("[%s(%d)]"#name" end thread is not the same as start thread (%lu vs %lu)\n",__FILE__,__LINE__,gettid(),name##_threadid);fflush(stdout);} \
        name##thread_warning = 1; \
    } else if (name##need_end && (gettid() == name##_threadid)) { \
        if (0) {printf("[%s(%d)]"#name" %d end\n",__FILE__,__LINE__,name##_count);fflush(stdout);} \
        u32 name##_value = (name##_end.tv_sec-name##_start.tv_sec)*1000000 \
        +(name##_end.tv_usec-name##_start.tv_usec); \
        if (name##_count < RECORD_MAX) \
            name##_val[name##_count] = name##_value; \
        name##_count++; \
        name##_total+= name##_value; \
        name##need_end = 0; \
    } else { \
        if (0) {printf("[%s(%d)]"#name" end thread is early as start thread (%lu vs %lu)\n",__FILE__,__LINE__,gettid(),name##_threadid);fflush(stdout);} \
    }\
    }

#define PERFORMANCE_STATIC_REPORT(name) { \
    extern u32 name##_count; \
    extern u64 name##_total; \
    extern pthread_t name##_threadid; \
    extern u32 name##_val[]; \
    printf(#name"(%lu) count: %d, total: %lld us, %lld us pertime\n", \
            name##_threadid, name##_count, name##_total, \
            name##_count ? name##_total/name##_count : 0); \
    if (0) { \
        int i; \
        for (i = 0; i < ((name##_count < RECORD_MAX)? name##_count : RECORD_MAX); i++) { \
            printf(#name"[%03d] = %d\n", i, name##_val[i]); \
        } \
    } \
    }
#else
#define DECLARE_PERFORMANCE_STATIC(name)
#define PERFORMANCE_STATIC_START(name)
#define PERFORMANCE_STATIC_END(name)
#define PERFORMANCE_STATIC_REPORT(name)
#endif

#endif /* __SW_PERFORMANCE_H__ */
