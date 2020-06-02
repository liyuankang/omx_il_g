#ifndef _SUBSYS_H_
#define _SUBSYS_H_

#include <linux/fs.h>
#include "hantrodec.h"

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

/* Functions provided by all other subsystem IP - hantrodec_xxx.c */

/******************************************************************************/
/* subsys level */
/******************************************************************************/

#define MAX_SUBSYS_NUM  4   /* up to 4 subsystem (temporary) */

/* SubsysDesc & CoreDesc are used for configuration */
struct SubsysDesc {
  int slice_index;    /* slice this subsys belongs to */
  int index;   /* subsystem index */
  long base;
};

struct CoreDesc {
  int slice;
  int subsys;     /* subsys this core belongs to */
  enum CoreType core_type;
  int offset;     /* offset to subsystem base */
  int iosize;
  int irq;
};

/* internal config struct (translated from SubsysDesc & CoreDesc) */
struct subsys_config {
  unsigned long base_addr;
  int irq;
  u32 subsys_type;  /* identifier for each subsys vc8000e=0,IM=1,vc8000d=2,jpege=3,jpegd=4 */
  u16 submodule_offset[HW_CORE_MAX]; /* in bytes */
  u16 submodule_iosize[HW_CORE_MAX]; /* in bytes */
  volatile u8 *submodule_hwregs[HW_CORE_MAX]; /* virtual address */
};

void CheckSubsysCoreArray(struct subsys_config *subsys, int *vcmd);

/******************************************************************************/
/* VCMD */
/******************************************************************************/
#define OPCODE_WREG               (0x01<<27)
#define OPCODE_END                (0x02<<27)
#define OPCODE_NOP                (0x03<<27)
#define OPCODE_RREG               (0x16<<27)
#define OPCODE_INT                (0x18<<27)
#define OPCODE_JMP                (0x19<<27)
#define OPCODE_STALL              (0x09<<27)
#define OPCODE_CLRINT             (0x1a<<27)
#define OPCODE_JMP_RDY0           (0x19<<27)
#define OPCODE_JMP_RDY1           ((0x19<<27)|(1<<26))
#define JMP_IE_1                  (1<<25)
#define JMP_RDY_1                 (1<<26)

/* Used in vcmd initialization in hantro_vcmd.c. */
/* May be unified in next step. */
struct vcmd_config {
  unsigned long vcmd_base_addr;
  u32 vcmd_iosize;
  int vcmd_irq;
  u32 sub_module_type;  /*input vc8000e=0,IM=1,vc8000d=2,jpege=3,jpegd=4*/
  u16 submodule_main_addr; // in byte
  u16 submodule_dec400_addr;//if submodule addr == 0xffff, this submodule does not exist.// in byte
  u16 submodule_L2Cache_addr; // in byte
  u16 submodule_MMU_addr; // in byte
};


int hantrovcmd_open(struct inode *inode, struct file *filp);
int hantrovcmd_release(struct inode *inode, struct file *filp);
long hantrovcmd_ioctl(struct file *filp,
                      unsigned int cmd, unsigned long arg);
int hantrovcmd_init(void);
void hantrovcmd_cleanup(void);

/******************************************************************************/
/* MMU */
/******************************************************************************/

/* Init MMU, should be called in driver init function. */
enum MMUStatus MMUInit(volatile unsigned char *hwregs);
/* Clean up all data in MMU, should be called in driver cleanup function
   when rmmod driver*/
enum MMUStatus MMUCleanup(volatile unsigned char *hwregs);
/* The function should be called in driver realease function
   when driver exit unnormally */
enum MMUStatus MMURelease(void *filp, volatile unsigned char *hwregs);

long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg,
              volatile unsigned char *hwregs);

/******************************************************************************/
/* L2Cache */
/******************************************************************************/

/******************************************************************************/
/* DEC400 */
/******************************************************************************/


/******************************************************************************/
/* AXI FE */
/******************************************************************************/


#endif

