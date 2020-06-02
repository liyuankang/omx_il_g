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
#ifndef _MEMALLOC_FREERTOS_H_
#define _MEMALLOC_FREERTOS_H_

#include "user_freertos.h"
#include "dev_common_freertos.h"

#undef PDEBUG   /* undef it, just in case */
#ifdef MEMALLOC_DEBUG
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif
/*
 * Ioctl definitions
 */
/* Use 'k' as magic number */
#define MEMALLOC_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
#define MEMALLOC_IOCXGETBUFFER    _IOWR(MEMALLOC_IOC_MAGIC, 1, MemallocParams*)
#define MEMALLOC_IOCSFREEBUFFER   _IOW(MEMALLOC_IOC_MAGIC, 2, unsigned long*)
#define MEMALLOC_IOCGMEMBASE	  _IOR(MEMALLOC_IOC_MAGIC, 3, unsigned long *)

/* ... more to come */
#define MEMALLOC_IOCHARDRESET     _IO(MEMALLOC_IOC_MAGIC, 15) /* debugging tool */
#define MEMALLOC_IOC_MAXNR 15


typedef struct {
        unsigned long long bus_address;
        unsigned int size;
        unsigned long translation_offset;
} MemallocParams;

//Mem device
int __init memalloc_init(void);
int memalloc_open(int *inode, int filp);
int memalloc_release(int *inode, int filp);
long memalloc_ioctl(int filp, unsigned int cmd, unsigned long arg);
void * DirectMemoryMap(unsigned long long busaddr, unsigned long map_size);
ptr_t GetBusAddrForIODevide(unsigned int size);

#endif /* _MEMALLOC_FREERTOS_H_ */
