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
* This file will be only included by the osal.h when test in Linux platform.
* Note: the FreeRTOS simulator can't be included in Linux.
********************************************************************************/

#ifndef _OSAL_LINUX_H_
#define _OSAL_LINUX_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/time.h>
#include <sys/types.h>
#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE
#include <sys/mman.h>
#endif
#include <getopt.h>

#ifdef _HAVE_PTHREAD_H
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#endif /* _HAVE_PTHREAD_H */

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __LP64__
#define OSAL_WIDTH_64
#endif

//! for Linux
#define _FILE_OFFSET_BITS 64  // for 64 bit fseeko
#define fseek fseeko
#define int16 int16_t
#define int64 int64_t
#define __int64 long long

#ifdef _DWL_DEBUG
#define DWL_DEBUG(fmt, args...) \
    printf(__FILE__ ":%d:%s() " fmt, __LINE__, __func__, ##args)
#else
#define DWL_DEBUG(fmt, args...) \
	do {                        \
	} while (0)
#endif

#ifndef CMODEL_DLL_API
#define CMODEL_DLL_API
#endif

#ifdef _HAVE_PTHREAD_H
typedef pthread_attr_t              osal_attr_t;
typedef pthread_mutexattr_t         osal_mutexattr_t;
typedef pthread_mutex_t             osal_mutex_t;
typedef pthread_condattr_t          osal_condattr_t;
typedef pthread_cond_t              osal_cond_t;
typedef pthread_t                   osal_thread_t;
typedef sem_t                       osal_sem_t;
typedef struct sched_param          osal_sched_param;

static av_unused int osal_mutexattr_init(osal_mutexattr_t *attr)
{
  return pthread_mutexattr_init(attr);
}

static av_unused int osal_mutex_init(osal_mutex_t *mutex, const osal_mutexattr_t *attr)
{
  return pthread_mutex_init(mutex, attr);
}

static av_unused int osal_mutexattr_destroy(osal_mutexattr_t *attr)
{
  return pthread_mutexattr_destroy(attr);
}

static av_unused int osal_mutex_destroy(osal_mutex_t *mutex)
{
  return pthread_mutex_destroy(mutex);
}

static av_unused int osal_mutex_lock(osal_mutex_t *mutex)
{
  return pthread_mutex_lock(mutex);
}

static av_unused int osal_mutex_unlock(osal_mutex_t *mutex)
{
  return pthread_mutex_unlock(mutex);
}

static av_unused int osal_condattr_init(osal_condattr_t *attr)
{
  return pthread_condattr_init(attr);
}

static av_unused int osal_cond_init(osal_cond_t *cond, const osal_condattr_t *attr)
{
  return pthread_cond_init(cond, attr);
}

static av_unused int osal_condattr_destroy(osal_condattr_t *attr)
{
  return pthread_condattr_destroy(attr);
}

static av_unused int osal_cond_wait(osal_cond_t *cond, osal_mutex_t *mutex)
{
  return pthread_cond_wait(cond, mutex);
}
static av_unused int osal_cond_signal(osal_cond_t *cond)
{
  return pthread_cond_signal(cond);
}

static av_unused int osal_cond_destroy(osal_cond_t *cond)
{
  return pthread_cond_destroy(cond);
}

static av_unused int osal_thread_join(osal_thread_t thread, void **status)
{
  return pthread_join(thread, status);
}

static av_unused int osal_attr_init(osal_attr_t *attr)
{
  return pthread_attr_init(attr);
}

static av_unused int osal_attr_setinheritsched(osal_attr_t *attr, int inheritsched)
{
  return pthread_attr_setinheritsched(attr, inheritsched);
}

static av_unused int osal_attr_setdetachstate(osal_attr_t *attr, int detachstate)
{
  return pthread_attr_setdetachstate(attr, detachstate);
}

static av_unused int osal_attr_setschedpolicy(osal_attr_t *attr, int policy)
{
  return pthread_attr_setschedpolicy(attr, policy);
}
static av_unused int osal_attr_setschedparam(osal_attr_t *attr, const osal_sched_param *param)
{
  return pthread_attr_setschedparam(attr, param);
}

static av_unused int osal_thread_create(osal_thread_t *thread, const osal_attr_t *attr,
  void*(*start_routine)(void*), void *arg)
{
  return pthread_create(thread, attr, start_routine, arg);
}

static av_unused int osal_attr_destroy(osal_attr_t *attr)
{
  return pthread_attr_destroy(attr);
}

static av_unused void osal_thread_exit(void *retval)
{
  pthread_exit(retval);
}

static av_unused osal_thread_t osal_thread_self()
{
  return pthread_self();
}

static av_unused int osal_sem_init(osal_sem_t *sem, int shared, unsigned value)
{
  return sem_init(sem, shared, value);
}

static av_unused int osal_sem_destroy(osal_sem_t *sem)
{
  return sem_destroy(sem);
}

static av_unused int osal_sem_getvalue(osal_sem_t *sem, int *sval)
{
  return sem_getvalue(sem, sval);
}

static av_unused int osal_sem_wait(osal_sem_t *sem)
{
  return sem_wait(sem);
}

static av_unused int osal_sem_post(osal_sem_t *sem)
{
    return sem_post(sem);
}

static av_unused int osal_sched_yield(void)
{
  return sched_yield();
}

#endif /* _HAVE_PTHREAD_H */

static av_unused int osal_usleep(unsigned long usec)
{
#if defined(H264BSDDEC_UTIL_H) || defined(VP78_TOKENS_H)\
  	|| defined(H264_MVD_H) || defined(CODING_TREE_H) || defined(BUSIFD_H)

#ifdef _HAVE_PTHREAD_H
  return sched_yield();
#else
#define sched_yield()
  return 0;
#endif

#else

#ifdef _HAVE_PTHREAD_H
  return usleep(usec);
#else
//osfree for usleep
#if 0
  int i, j, tick_in_us;
  unsigned long long freq = CPUFreq;

  tick_in_us = freq * 17ul >> 24; /* 17 / 2 ^ 24 = 1000000L */
  for (i = 0; i < n; i++) {
    for (j = 0; j < tick_in_us; j++);
  }
#endif
  return 0;
#endif

#endif
}

typedef struct timeval    osal_timeval;
typedef struct timezone   osal_timezone;
static av_unused int osal_gettimeofday(osal_timeval *tp, void *tzp)
{
  return gettimeofday(tp, (osal_timezone *)tzp);
}

static av_unused void * osal_aligned_malloc(unsigned int boundary, unsigned int memory_size)
{
  return memalign(boundary, memory_size);
}

static av_unused void osal_aligned_free(void * aligned_momory)
{
  free(aligned_momory);
}

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_LINUX_H_ */
