/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2019 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Description :  vcmd SW/HW interface register definitions
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines

------------------------------------------------------------------------------*/
#ifndef VCMD_SWHWREGISTERS_H
#define VCMD_SWHWREGISTERS_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#ifndef MODEL_SIMULATION
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#endif

#undef PDEBUG   /* undef it, just in case */
#ifdef REGISTER_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "memalloc: " fmt, ## args)
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, ...)  /* not debugging: nothing */
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define ASIC_VCMD_SWREG_AMOUNT                  27
#define VCMD_REGISTER_CONTROL_OFFSET            0X40
#define VCMD_REGISTER_INT_STATUS_OFFSET         0X44
#define VCMD_REGISTER_INT_CTL_OFFSET            0X48
//typedef int           i32;
typedef size_t    ptr_t;



/* HW Register field names */
typedef enum
{

#include "vcmdregisterenum.h"

  VcmdRegisterAmount

} regVcmdName;


/* HW Register field descriptions */
typedef struct
{
  u32 name;               /* Register name and index  */
  int base;               /* Register base address  */
  u32 mask;               /* Bitmask for this field */
  int lsb;                /* LSB for this field [31..0] */
  int trace;              /* Enable/disable writing in swreg_params.trc */
  int rw;                 /* 1=Read-only 2=Write-only 3=Read-Write */
  char *description;      /* Field description */
} regVcmdField_s;

/* Flags for read-only, write-only and read-write */
#define RO 1
#define WO 2
#define RW 3

#define REGBASE(reg) (asicVcmdRegisterDesc[reg].base)

/* Description field only needed for system model build. */
#ifdef TEST_DATA
#define VCMDREG(name, base, mask, lsb, trace, rw, desc) \
        {name, base, mask, lsb, trace, rw, desc}
#else
#define VCMDREG(name, base, mask, lsb, trace, rw, desc) \
        {name, base, mask, lsb, trace, rw, ""}
#endif


/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
extern const regVcmdField_s asicVcmdRegisterDesc[];

/*------------------------------------------------------------------------------

    EncAsicSetRegisterValue

    Set a value into a defined register field

------------------------------------------------------------------------------*/
static inline void vcmd_set_register_mirror_value(u32 *reg_mirror, regVcmdName name, u32 value)
{
  const regVcmdField_s *field;
  u32 regVal;

  field = &asicVcmdRegisterDesc[name];

#ifdef DEBUG_PRINT_REGS
  printf("vcmd_set_register_mirror_value 0x%2x  0x%08x  Value: %10d  %s\n",
         field->base, field->mask, value, field->description);
#endif

  /* Check that value fits in field */
  PDEBUG("field->name == name=%d/n",field->name == name);
  PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d/n",((field->mask >> field->lsb) << field->lsb) == field->mask);
  PDEBUG("(field->mask >> field->lsb) >= value=%d/n",(field->mask >> field->lsb) >= value);
  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d/n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  /* Clear previous value of field in register */
  regVal = reg_mirror[field->base / 4] & ~(field->mask);

  /* Put new value of field in register */
  reg_mirror[field->base / 4] = regVal | ((value << field->lsb) & field->mask);
}
static inline u32 vcmd_get_register_mirror_value(u32 *reg_mirror, regVcmdName name)
{
  const regVcmdField_s *field;
  u32 regVal;

  field = &asicVcmdRegisterDesc[name];


  /* Check that value fits in field */
  PDEBUG("field->name == name=%d/n",field->name == name);
  PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d/n",((field->mask >> field->lsb) << field->lsb) == field->mask);
  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d/n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  regVal = reg_mirror[field->base / 4];
  regVal = (regVal & field->mask) >> field->lsb;
  
#ifdef DEBUG_PRINT_REGS
  PDEBUG("vcmd_get_register_mirror_value 0x%2x  0x%08x  Value: %10d  %s\n",
         field->base, field->mask, regVal, field->description);
#endif
  return regVal;
}

u32 vcmd_read_reg(const void *hwregs, u32 offset);

void vcmd_write_reg(const void *hwregs, u32 offset, u32 val);



void vcmd_write_register_value(const void *hwregs,u32* reg_mirror,regVcmdName name, u32 value);


u32 vcmd_get_register_value(const void *hwregs, u32* reg_mirror,regVcmdName name);



#if defined(__LP64__) || defined(_WIN64) || defined(_WIN32)

#define vcmd_set_addr_register_value(reg_base, reg_mirror, name, value) do {\
    vcmd_write_register_value((reg_base), (reg_mirror),name, (u32)(value));  \
    vcmd_write_register_value((reg_base), (reg_mirror),name##_MSB, (u32)((value) >> 32)); \
  } while (0)

#define VCMDGetAddrRegisterValue(reg_base, reg_mirror,name)  \
  (((ptr_t)vcmd_get_register_value((reg_base),(reg_mirror), name)) |  \
  (((ptr_t)vcmd_get_register_value((reg_base), (reg_mirror),name##_MSB)) << 32))

#else 

#define vcmd_set_addr_register_value(reg_base, reg_mirror,name, value) do {\
    vcmd_write_register_value((reg_base),(reg_mirror), name, (u32)(value));  \
  } while (0)

#define VCMDGetAddrRegisterValue(reg_base, reg_mirror,name)  \
  ((ptr_t)vcmd_get_register_value((reg_base),(reg_mirror), (name)))

#endif

#endif
