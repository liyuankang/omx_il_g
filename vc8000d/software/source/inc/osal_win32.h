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
* This file will be only included by the osal.h when test for Windows platform.
* Note: the FreeRTOS simulator can't be included in Windows.
********************************************************************************/
#ifndef _OSAL_WIN32_H_
#define _OSAL_WIN32_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _HAVE_PTHREAD_H
#include "win32/include/pthread.h"
#include "win32/include/semaphore.h"
#include "win32/include/sched.h"
#endif /* _HAVE_PTHREAD_H */

#include "getopt.h"
#include <io.h>
#include <process.h>
#include <windows.h>
#include <time.h>

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN64
#define OSAL_WIDTH_64
#endif
#if defined(_MSC_VER)
typedef __int64 off_t;
typedef __int64 off64_t;
#define fseeko _fseeki64
#define ftello _ftelli64
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
#elif defined(_WIN32)
/* MinGW defines off_t as long and uses f{seek,tell}o64/off64_t for large files. */
#define fseeko fseeko64
#define ftello ftello64
#define off_t off64_t
#endif
#define snprintf _snprintf
#define open _open
#define close _close
#define read _read
#define write _write
//#define lseek _lseeki64
//#define fsync _commit
//#define tell _telli64

#define NAME_MAX 255
#undef fseek
#define fseek _fseeki64
#define int16 __int16
#define int64 __int64

#ifdef _DWL_DEBUG
#define DWL_DEBUG printf
#else
#define DWL_DEBUG(fmt, _VA_ARGS__) \
    do {                          \
    } while (0)
#endif /* _DWL_DEBUG */

#ifndef CMODEL_DLL_API
#ifdef CMODEL_DLL_SUPPORT  /* cmodel in dll */
#ifdef _CMODEL_   /* This macro will be enabled when compiling cmodel libs. */
#define CMODEL_DLL_API __declspec(dllexport)
#else
#define CMODEL_DLL_API __declspec(dllimport)
#endif
#else
/* win non-dll. */
#define CMODEL_DLL_API
#endif
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
#if defined(VP9_TOKENS_H) || defined(APF_H)
  //TODO...
  return 0;
#else
  return sched_yield();
#endif
}

#endif /* _HAVE_PTHREAD_H */

static av_unused int osal_usleep(unsigned long usec)
{
  if(usec < 1000)
  	Sleep(1);
  else
    Sleep(usec/1000);

  return 0;
}

typedef struct timeval              osal_timeval;
static av_unused int osal_gettimeofday(osal_timeval *tp, void *tzp)
{
  time_t clock;
  struct tm tm;
  SYSTEMTIME wtm;

  GetLocalTime(&wtm);
  tm.tm_year = wtm.wYear - 1900;
  tm.tm_mon = wtm.wMonth - 1;
  tm.tm_mday = wtm.wDay;
  tm.tm_hour = wtm.wHour;
  tm.tm_min = wtm.wMinute;
  tm.tm_sec = wtm.wSecond;
  tm.tm_isdst = -1;
  clock = mktime(&tm);
  tp->tv_sec = (long)clock;
  tp->tv_usec = wtm.wMilliseconds * 1000;

  return 0;
}

static av_unused void * osal_aligned_malloc(unsigned int boundary, unsigned int memory_size)
{
  return _aligned_malloc(memory_size, boundary);
}

static av_unused void osal_aligned_free(void * aligned_momory)
{
  _aligned_free(aligned_momory);
}

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_WIN32_H_ */
