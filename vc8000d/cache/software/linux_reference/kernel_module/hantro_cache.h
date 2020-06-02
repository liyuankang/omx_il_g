/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Abstract : Cache device driver (kernel module)
--
------------------------------------------------------------------------------*/

#ifndef _CACHE_H_
#define _CACHE_H_
#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */

/*
 * Macros to help debugging
 */

#undef PDEBUG   /* undef it, just in case */
#ifdef CACHE_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
    #define PDEBUG printk
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

#define PCI_BUS

/*Define Cache&Shaper Offset from common base*/
#define SHAPER_OFFSET                    (0x8<<2)
#define CACHE_ONLY_OFFSET                (0x8<<2)
#define CACHE_WITH_SHAPER_OFFSET         (0x80<<2)

/*IOCTL define*/

/* Use 'k' as magic number */
#define CACHE_IOC_MAGIC  'c'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
 /*
  * #define CACHE_IOCGBUFBUSADDRESS _IOR(CACHE_IOC_MAGIC,  1, unsigned long *)
  * #define CACHE_IOCGBUFSIZE       _IOR(CACHE_IOC_MAGIC,  2, unsigned int *)
  */
#define CACHE_IOCGHWOFFSET      _IOR(CACHE_IOC_MAGIC,  3, unsigned long *)
#define CACHE_IOCGHWIOSIZE      _IOR(CACHE_IOC_MAGIC,  4, unsigned int *)
#define CACHE_IOCGHWID          _IOR(CACHE_IOC_MAGIC,  6, unsigned int *)
#define CACHE_IOCHARDRESET      _IO(CACHE_IOC_MAGIC, 8)   /* debugging tool */

#define CACHE_IOCH_HW_RESERVE   _IOR(CACHE_IOC_MAGIC, 11,unsigned int *)
#define CACHE_IOCH_HW_RELEASE   _IOR(CACHE_IOC_MAGIC, 12,unsigned int *)
#define CACHE_IOCG_CORE_NUM      _IOR(CACHE_IOC_MAGIC, 13,unsigned int *)

#define CACHE_IOCG_ABORT_WAIT     _IOR(CACHE_IOC_MAGIC, 14, unsigned int *)
#define CACHE_IOC_MAXNR 30

typedef enum
{
 VC8000E,
 VC8000D_0,
 VC8000D_1
}cache_client_type;

typedef enum
{
 DIR_RD,
 DIR_WR,
 DIR_BI
}driver_cache_dir;

typedef struct
{
  cache_client_type client;
  unsigned long base_addr;  
  u32 iosize;
  int irq;
  driver_cache_dir dir;
}CORE_CONFIG;


#endif /* !_CACHE_H_ */

