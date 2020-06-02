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
* This file will be only included by the osal.h when FreeRTOS simulator in
* Windows, Linux and the rea FreeRTOS env. At the same time, need to define the
* macro "__FREERTOS__". All the macros included _WIN32 and __linux__ are just
* used to simulate for FreeRTOS.  
********************************************************************************/

#ifndef _OSAL_FREERTOS_H_
#define _OSAL_FREERTOS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _HAVE_PTHREAD_H
#ifdef _WIN32
#include "FreeRTOS_POSIX.h"
#include "FreeRTOS_POSIX/pthread.h"
#include "FreeRTOS_POSIX/semaphore.h"
#include "FreeRTOS_POSIX/sched.h"
#include "FreeRTOS_POSIX/unistd.h"
#include "FreeRTOS_POSIX/errno.h"

#elif defined(__linux__) //Linux Simulator
/*************************************************************************************************** 
Need to include some basic FreeRTOS header files when Freertos simulator in Linux, at the same time,
the related makefile files need to include the FreeRTOS path, like the below:

Exists variale FreeRTOSDir = software/linux/dwl/osal/freertos/FreeRTOS_Kernel in common.mk
ifeq ($(USE_FREERTOS_SIMULATOR), y)
  INCLUDE += -I$(FreeRTOSDir)/Source/include \
             -I$(FreeRTOSDir)/Source/portable/Linux/GCC/Posix \
             -I$(FreeRTOSDir)/Source/portable/Linux 
endif 
And need to define the macro "__FREERTOS__"
***************************************************************************************************/
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "task.h"

#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#else //For the real FreeRTOS environment
#include "FreeRTOS_POSIX.h"
#include "FreeRTOS_POSIX/pthread.h"
#include "FreeRTOS_POSIX/semaphore.h"
#include "FreeRTOS_POSIX/sched.h"
#include "FreeRTOS_POSIX/unistd.h"
#include "FreeRTOS_POSIX/errno.h"
#endif /* _WIN32 */
#endif /* _HAVE_PTHREAD_H */

#ifdef _WIN32
#include "getopt.h"
#include <io.h>
#include <process.h>
#include <windows.h>
#include <time.h>
#elif defined(__linux__) //Linux simulator
#include <sys/time.h>
#include <sys/types.h>
//#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE
#include <sys/mman.h>
//#endif
#include <getopt.h>

#else //FreeRTOS and other os
//TODO... maybe like this
//#include "FreeRTOS_POSIX/sys/types.h"
#include "FreeRTOS_POSIX/time.h"
#include "getopt.h"
//maybe file header.h will be included here
#endif /* _WIN32 */

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

/* the bus width for the related is */
#if defined(_WIN32) || defined(__linux__)
#if defined(_WIN64) || defined(__LP64__)
#define OSAL_WIDTH_64
#endif

#else //defined() //FreeRTOS the 64bit 
//#define OSAL_WIDTH_64
#endif

//file operations
#if defined(_MSC_VER)
/* MSVS doesn't define off_t, and uses _f{seek,tell}i64. */
//typedef __int64 off_t;
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

#elif defined(__linux__) 
 //Nothing to do
 
#else //For FreeRTOS, close all the file operations, maybe can use FreeRTOS+FAT
//typedef long long off_t;
#define off64_t off_t
#endif /* _MSC_VER */

#ifdef _WIN32
#define snprintf _snprintf
#define open _open
#define close _close
#define read _read
#define write _write
//#define lseek _lseeki64
//#define fsync _commit
//#define tell _telli64

#elif defined(__linux__)
  //Nothing to do
  
#else //For FreeRTOS, close all the file operations, maybe can use FreeRTOS+FAT
#endif /* _WIN32 */

#ifdef __linux__
  //Nothing for linux
#elif defined(_WIN32)
#define NAME_MAX 255
#else //
 //Nothing for other os, like FreeROTS
#endif /* _WIN32 */

#ifdef _WIN32
//#undef fseek
#define fseek _fseeki64
#define int16 __int16
#define int64 __int64

#elif defined(__linux__)
#define _FILE_OFFSET_BITS 64  // for 64 bit fseeko
#define fseek fseeko
#define int16 int16_t
#define int64 int64_t
#else //For FreeRTOS, close all the file operations, maybe can use FreeRTOS+FAT
//#define fseek(a, b, c) 
#define int16 short
#define int64 long long
#endif /* _WIN32 */

#if defined(__linux__) || defined(__FREERTOS__)
#define __int64 long long
#endif

#ifdef __linux__ //Linux simulator
#ifdef _DWL_DEBUG
#define DWL_DEBUG(fmt, args...) \
    printf(__FILE__ ":%d:%s() " fmt, __LINE__, __func__, ##args)
#else
#define DWL_DEBUG(fmt, args...) \
	do {                        \
	} while (0)
#endif/* _DWL_DEBUG */

#elif defined(_WIN32)
#ifdef _DWL_DEBUG
#define DWL_DEBUG printf
#else
#define DWL_DEBUG(fmt, _VA_ARGS__) \
    do {                            \
    } while (0)
#endif /* _DWL_DEBUG */

#elif defined(__FREERTOS__)
#ifdef _DWL_DEBUG
#define DWL_DEBUG printf
#else
#define DWL_DEBUG(fmt, args...) \
    do {                        \
    } while (0)
#endif /* _DWL_DEBUG */
#else
//TODO...
#endif /* _WIN32 */

#ifdef _WIN32
#ifndef CMODEL_DLL_API
#ifdef CMODEL_DLL_SUPPORT  /* cmodel in dll */
#ifdef _CMODEL_   /* This macro will be enabled when compiling cmodel libs. */
#define CMODEL_DLL_API __declspec(dllexport)
#else
#define CMODEL_DLL_API __declspec(dllimport)
#endif /* _CMODEL_ */
#else
/* win non-dll. */
#define CMODEL_DLL_API
#endif /* CMODEL_DLL_SUPPORT */
#endif /* CMODEL_DLL_API */

#else //Linux and FreeRTOS, etc.

#ifndef CMODEL_DLL_API
#define CMODEL_DLL_API
#endif
#endif /* _WIN32 */

#ifdef _HAVE_PTHREAD_H
typedef pthread_attr_t              osal_attr_t;
typedef pthread_mutexattr_t         osal_mutexattr_t;
typedef pthread_mutex_t             osal_mutex_t;
typedef pthread_condattr_t          osal_condattr_t;
typedef pthread_cond_t              osal_cond_t;
typedef pthread_t                   osal_thread_t;
typedef sem_t                       osal_sem_t;
typedef struct sched_param          osal_sched_param;

#ifdef _WIN32
#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER   {0, 0, 0, 0}
/*
 * pthread_attr_{set}inheritsched
 */
#define PTHREAD_INHERIT_SCHED   0
#define PTHREAD_EXPLICIT_SCHED  1 /* Default */

enum {
  //SCHED_OTHER = 0,
  SCHED_FIFO = 1,
  SCHED_RR,
  SCHED_MIN = SCHED_OTHER,
  SCHED_MAX = SCHED_RR
};
#elif defined(__linux__)
//Nothing, exists the following macro definition
#else //other os like FreeRTOS
/*
* pthread_attr_{set}inheritsched
*/
#define PTHREAD_INHERIT_SCHED   0
#define PTHREAD_EXPLICIT_SCHED  1 /* Default */

enum {
  //SCHED_OTHER = 0,
  SCHED_FIFO = 1,
  SCHED_RR,
  SCHED_MIN = SCHED_OTHER,
  SCHED_MAX = SCHED_RR
};
#endif /* _WIN32 */

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
#if defined(_WIN32)
  (void)attr;

  return 0;
#elif defined(__linux__)
  return pthread_condattr_init(attr);
#else //FreeRTOS and other os need to implement by oneself
  //TODO...
  (void)attr;

  return 0;
#endif
}

static av_unused int osal_cond_init(osal_cond_t *cond, const osal_condattr_t *attr)
{
    return pthread_cond_init(cond, attr);
}

static av_unused int osal_condattr_destroy(osal_condattr_t *attr)
{
#if defined(_WIN32)
  (void)attr;

  return 0;
#elif defined(__linux__)
  return pthread_condattr_destroy(attr);
#else //FreeRTOS and other os need to implement by oneself
  //TODO...
  (void)attr;

  return 0;
#endif
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
#if defined(_WIN32)
  (void)attr;
  (void)inheritsched;
  
  return 0;
#elif defined(__linux__)
	return pthread_attr_setinheritsched(attr, inheritsched);
#else //FreeRTOS and other os need to implement by oneself
  //TODO...
  (void)attr;
  (void)inheritsched;
  
  return 0;
#endif
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
#ifdef _WIN32
#if defined(VP9_TOKENS_H) || defined(APF_H)
  //TODO...
  return 0;
#else
  return sched_yield();
#endif

#else //Linux and FreeRTOS, etc
  return sched_yield();
#endif
}

#endif /* _HAVE_PTHREAD_H */

static av_unused int osal_usleep(unsigned long usec)
{
  return usleep(usec);
}

#if defined(_WIN32) || defined(__linux__)
typedef struct timeval osal_timeval;
#else //FreeRTOS
typedef struct osal_timeval_
{
  ssize_t tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
}osal_timeval;
#define timeval osal_timeval_
#endif /* defined(_WIN32) || defined(__linux__) */
/*------------------------------------------------------------------------------
 Function name   : osal_gettimeofday
 Description     : open method
 Parameters      : osal_timeval *tp - Used to store  time of the current time 
                     included the seconds and nanosecnods
                   void *tzp - not used
 Return type     : int
------------------------------------------------------------------------------*/
static av_unused int osal_gettimeofday(osal_timeval *tp, void *tzp)
{
#ifdef _WIN32
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
#elif defined(__linux__)
  return gettimeofday(tp, tzp);
#else //FreeRTOS and other os need to implement  by oneself
  //TODO...
  return 0;
#endif
}

/* For cmodel */
static av_unused void * osal_aligned_malloc(unsigned int boundary, unsigned int memory_size)
{
#ifdef _WIN32
  return _aligned_malloc(memory_size, boundary);
#elif defined(__linux__)
  return memalign(boundary, memory_size);
#else //FreeRTOS and other os need to implement by oneself
  return memalign(boundary, memory_size);
#endif
}

/* For cmodel */
static av_unused void osal_aligned_free(void * aligned_momory)
{
#ifdef _WIN32
  _aligned_free(aligned_momory);
#elif defined(__linux__)
  free(aligned_momory);
#else //FreeRTOS and other os need to implement by oneself
  free(aligned_momory);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _OSAL_FREERTOS_H_ */
