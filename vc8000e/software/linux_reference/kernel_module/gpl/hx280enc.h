/*
 *  H2 Encoder device driver (kernel module)
 *  
 *  COPYRIGHT(C) 2014 VERISILICON
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 2 
 * of the License, or (at your option) any later version. 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, 
 * USA. 
 */

#ifndef _HX280ENC_H_
#define _HX280ENC_H_
#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */
#ifdef HANTROMMU_SUPPORT
#include "hantrommu.h"
#endif

/*
 * Macros to help debugging
 */

#undef PDEBUG   /* undef it, just in case */
#ifdef HX280ENC_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "hmp4e: " fmt, ## args)
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

/*
 * Ioctl definitions
 */

#define ENC_HW_ID1                  0x48320100
#define ENC_HW_ID2                  0x80006000
#define CORE_INFO_MODE_OFFSET       31
#define CORE_INFO_AMOUNT_OFFSET     28

/* Use 'k' as magic number */
#define HX280ENC_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
 /*
  * #define HX280ENC_IOCGBUFBUSADDRESS _IOR(HX280ENC_IOC_MAGIC,  1, unsigned long *)
  * #define HX280ENC_IOCGBUFSIZE       _IOR(HX280ENC_IOC_MAGIC,  2, unsigned int *)
  */
#define HX280ENC_IOCGHWOFFSET      _IOR(HX280ENC_IOC_MAGIC,  3, unsigned long *)
#define HX280ENC_IOCGHWIOSIZE      _IOR(HX280ENC_IOC_MAGIC,  4, unsigned int *)
#define HX280ENC_IOC_CLI           _IO(HX280ENC_IOC_MAGIC,  5)
#define HX280ENC_IOC_STI           _IO(HX280ENC_IOC_MAGIC,  6)
#define HX280ENC_IOCXVIRT2BUS      _IOWR(HX280ENC_IOC_MAGIC,  7, unsigned long *)

#define HX280ENC_IOCHARDRESET      _IO(HX280ENC_IOC_MAGIC, 8)   /* debugging tool */

#define HX280ENC_IOCGSRAMOFFSET    _IOR(HX280ENC_IOC_MAGIC,  9, unsigned long *)
#define HX280ENC_IOCGSRAMEIOSIZE    _IOR(HX280ENC_IOC_MAGIC,  10, unsigned int *)
#define HX280ENC_IOCH_ENC_RESERVE   _IOR(HX280ENC_IOC_MAGIC, 11,unsigned int *)
#define HX280ENC_IOCH_ENC_RELEASE   _IOR(HX280ENC_IOC_MAGIC, 12,unsigned int *)
#define HX280ENC_IOCG_CORE_NUM      _IOR(HX280ENC_IOC_MAGIC, 13,unsigned int *)
#define HX280ENC_IOCG_CORE_INFO     _IOR(HX280ENC_IOC_MAGIC, 14,SUBSYS_CORE_INFO *)

#define HX280ENC_IOCG_CORE_WAIT     _IOR(HX280ENC_IOC_MAGIC, 19, unsigned int *)
#define HX280ENC_IOC_MAXNR 30

#define GET_ENCODER_IDX(type_info)  ((type_info & (1<<CORE_VC8000E)) > 0 ? CORE_VC8000E : CORE_VC8000EJ);


enum 
{
  CORE_VC8000E = 0,
  CORE_VC8000EJ = 1,
  CORE_CUTREE = 2,
  CORE_DEC400 = 3,
  CORE_MMU = 4,
  CORE_VCMD_VC8000E = 5,
  CORE_VCMD_CUTREE = 6,
  CORE_MAX
};

typedef struct
{
  u32 subsys_idx;
  u32 core_type;
  unsigned long offset;
  u32 reg_size;
  int irq;
}CORE_CONFIG;

typedef struct
{
  unsigned long base_addr;
  u32 iosize;
  u32 resouce_shared; //indicate the core share resources with other cores or not.If 1, means cores can not work at the same time.
}SUBSYS_CONFIG;

typedef struct
{
  u32 type_info; //indicate which IP is contained in this subsystem and each uses one bit of this variable
  unsigned long offset[CORE_MAX];
  unsigned long regSize[CORE_MAX];
  int irq[CORE_MAX];
}SUBSYS_CORE_INFO;

typedef struct
{
  SUBSYS_CONFIG cfg;
  SUBSYS_CORE_INFO core_info;
}SUBSYS_DATA;
#endif /* !_HX280ENC_H_ */
