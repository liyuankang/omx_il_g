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
#ifndef _HANTRODEC_FREERTOS_H_
#define _HANTRODEC_FREERTOS_H_

#include "dwl_defs.h"
#include "dwl_linux.h"
#include "user_freertos.h"
#include "dev_common_freertos.h"

#undef PDEBUG
#ifdef HANTRODEC_DEBUG
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#define PDEBUG(fmt, args...)
#endif

struct core_desc {
  __u32 id; /* id of the core */
  __u32 *regs; /* pointer to user registers */
  __u32 size; /* size of register space */
  __u32 reg_id;
};

/* Use 'k' as magic number */
#define HANTRODEC_IOC_MAGIC  'k'

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

#define HANTRODEC_PP_INSTANCE       _IO(HANTRODEC_IOC_MAGIC, 1)
#define HANTRODEC_HW_PERFORMANCE    _IO(HANTRODEC_IOC_MAGIC, 2)
#define HANTRODEC_IOCGHWOFFSET      _IOR(HANTRODEC_IOC_MAGIC,  3, unsigned long *)
#define HANTRODEC_IOCGHWIOSIZE      _IOR(HANTRODEC_IOC_MAGIC,  4, unsigned int *)

#define HANTRODEC_IOC_CLI           _IO(HANTRODEC_IOC_MAGIC,  5)
#define HANTRODEC_IOC_STI           _IO(HANTRODEC_IOC_MAGIC,  6)
#define HANTRODEC_IOC_MC_OFFSETS    _IOR(HANTRODEC_IOC_MAGIC, 7, unsigned long *)
#define HANTRODEC_IOC_MC_CORES      _IOR(HANTRODEC_IOC_MAGIC, 8, unsigned int *)


#define HANTRODEC_IOCS_DEC_PUSH_REG  _IOW(HANTRODEC_IOC_MAGIC, 9, struct core_desc *)
#define HANTRODEC_IOCS_PP_PUSH_REG   _IOW(HANTRODEC_IOC_MAGIC, 10, struct core_desc *)

#define HANTRODEC_IOCH_DEC_RESERVE   _IO(HANTRODEC_IOC_MAGIC, 11)
#define HANTRODEC_IOCT_DEC_RELEASE   _IO(HANTRODEC_IOC_MAGIC, 12)
#define HANTRODEC_IOCQ_PP_RESERVE    _IO(HANTRODEC_IOC_MAGIC, 13)
#define HANTRODEC_IOCT_PP_RELEASE    _IO(HANTRODEC_IOC_MAGIC, 14)

#define HANTRODEC_IOCX_DEC_WAIT      _IOWR(HANTRODEC_IOC_MAGIC, 15, struct core_desc *)
#define HANTRODEC_IOCX_PP_WAIT       _IOWR(HANTRODEC_IOC_MAGIC, 16, struct core_desc *)

#define HANTRODEC_IOCS_DEC_PULL_REG  _IOWR(HANTRODEC_IOC_MAGIC, 17, struct core_desc *)
#define HANTRODEC_IOCS_PP_PULL_REG   _IOWR(HANTRODEC_IOC_MAGIC, 18, struct core_desc *)

#define HANTRODEC_IOCG_CORE_WAIT     _IOR(HANTRODEC_IOC_MAGIC, 19, int *)

#define HANTRODEC_IOX_ASIC_ID        _IOWR(HANTRODEC_IOC_MAGIC, 20, __u32 *)

#define HANTRODEC_IOCG_CORE_ID       _IOR(HANTRODEC_IOC_MAGIC, 21, unsigned long)

#define HANTRODEC_IOCS_DEC_WRITE_REG  _IOW(HANTRODEC_IOC_MAGIC, 22, struct core_desc *)

#define HANTRODEC_IOCS_DEC_READ_REG   _IOWR(HANTRODEC_IOC_MAGIC, 23, struct core_desc *)

#define HANTRODEC_IOX_ASIC_BUILD_ID   _IOWR(HANTRODEC_IOC_MAGIC, 24, __u32 *)

#define HANTRODEC_IOCS_MMU_MEM_MAP    _IOWR(HANTRODEC_IOC_MAGIC, 25, struct addr_desc *)

#define HANTRODEC_IOCS_MMU_MEM_UNMAP  _IOWR(HANTRODEC_IOC_MAGIC, 26, struct addr_desc *)

#define HANTRODEC_IOCS_MMU_ENABLE     _IOWR(HANTRODEC_IOC_MAGIC, 27, unsigned int *)

#define HANTRODEC_DEBUG_STATUS       _IO(HANTRODEC_IOC_MAGIC, 29)

#define HANTRODEC_IOC_MAXNR 29

//Dec
int __init hantrodec_init(void);
int hantrodec_open(int *inode, int filp);
int hantrodec_release(int *inode, int filp);
long hantrodec_ioctl(int filp, unsigned int cmd, unsigned long arg);
//typedef int (*IRQHandler)(i32 i, void* data);
typedef void (*IRQHandler)(void* data);

#endif /* _HANTRODEC_FREERTOS_H_ */
