/*------------------------------------------------------------------------------
-- Copyright (c) 2019, VeriSilicon Inc. or its affiliates. All rights reserved--
--                                                                            --
-- Permission is hereby granted, free of charge, to any person obtaining a    --
-- copy of this software and associated documentation files (the "Software"), --
-- to deal in the Software without restriction, including without limitation  --
-- the rights to use copy, modify, merge, publish, distribute, sublicense,    --
-- and/or sell copies of the Software, and to permit persons to whom the      --
-- Software is furnished to do so, subject to the following conditions:       --
--                                                                            --
-- The above copyright notice and this permission notice shall be included in --
-- all copies or substantial portions of the Software.                        --
--                                                                            --
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR --
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   --
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE--
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER     --
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    --
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        --
-- DEALINGS IN THE SOFTWARE.                                                  --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

/********************************************************************************
* This file is only included in the dwlthread.h, which is the entrance file about
* Operating System Abstraction Layer.
*
* And all the related code about OS will be contained here, especially the 
* thread-related code. It will includes test bench, API, DWL layer and some 
* codes in cmodel layer for all codec format except rv
********************************************************************************/
#ifndef _OSAL_H_
#define _OSAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(_WIN32) || defined(__FREERTOS__)

#if defined(__GNUC__) || defined(__clang__)
#define av_unused __attribute__((unused))
#else
#define av_unused
#endif

#ifdef __FREERTOS__
#include "osal_freertos.h"
#elif defined(__linux__)
#include "osal_linux.h"
#elif defined(_WIN32)
#include "osal_win32.h"
#endif

#ifdef _HAVE_PTHREAD_H
#define pthread_mutexattr_init      osal_mutexattr_init
#define pthread_mutex_init          osal_mutex_init
#define pthread_mutexattr_destroy   osal_mutexattr_destroy 
#define pthread_mutex_destroy       osal_mutex_destroy
#define pthread_mutex_lock          osal_mutex_lock
#define pthread_mutex_unlock        osal_mutex_unlock
#define pthread_condattr_init       osal_condattr_init
#define pthread_cond_init           osal_cond_init
#define pthread_condattr_destroy    osal_condattr_destroy
#define pthread_cond_destroy        osal_cond_destroy
#define pthread_cond_wait           osal_cond_wait
#define pthread_cond_signal         osal_cond_signal

#define pthread_attr_init               osal_attr_init
#define pthread_attr_setinheritsched    osal_attr_setinheritsched
#define pthread_attr_setdetachstate     osal_attr_setdetachstate
#define pthread_attr_setschedpolicy     osal_attr_setschedpolicy
#define pthread_attr_setschedparam      osal_attr_setschedparam
#define pthread_attr_destroy            osal_attr_destroy
#define pthread_create                  osal_thread_create
#define pthread_join                    osal_thread_join
#define pthread_exit                    osal_thread_exit
#define pthread_self                    osal_thread_self

#define sem_init                    osal_sem_init
#define sem_destroy                 osal_sem_destroy
#define sem_getvalue                osal_sem_getvalue
#define sem_wait                    osal_sem_wait
#define sem_post                    osal_sem_post
#define sched_yield                 osal_sched_yield

//For static link pthread in win32, need init some global variables to prevent crash
#ifdef PTW32_STATIC_LIB
static void detach_ptw32(void) {
  pthread_win32_thread_detach_np();
  pthread_win32_process_detach_np();
}
#endif

static av_unused void osal_thread_init()
{
#ifdef PTW32_STATIC_LIB
  pthread_win32_process_attach_np();
  pthread_win32_thread_attach_np();
  atexit(detach_ptw32);
#endif
}

#endif /* _HAVE_PTHREAD_H */

#endif /* defined(__FREERTOS__) || defined(__linux__) || defined(_WIN32) */

#define gettimeofday                osal_gettimeofday
#define usleep                      osal_usleep

//The definition when fprintf fpos_t for different platform
#ifdef _WIN32
#define OSAL_STRM_POS strm_pos
#else
#define OSAL_STRM_POS strm_pos.__pos
#endif

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_H_ */
