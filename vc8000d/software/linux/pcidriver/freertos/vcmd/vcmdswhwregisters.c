/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2019 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description :  Vcmd SW/HW interface register definitions
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines

------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "vcmdswhwregisters.h"

/* NOTE: Don't use ',' in descriptions, because it is used as separator in csv
 * parsing. */
const regVcmdField_s asicVcmdRegisterDesc[] =
{
#include "vcmdregistertable.h"
};

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Define this to print debug info for every register write.
#define DEBUG_PRINT_REGS */

/*--------------------------------------------------------*/
inline u32 ioread32(volatile void* addr) {
  return *(volatile u32*)(addr);
}

inline void iowrite32(u32 val,volatile void *addr) {
  *(volatile u32*)addr = val;
}

#if 0
static inline u16 ioread16(volatile void* addr) {
  return *(volatile u16*)(addr);
}

static inline void iowrite16(u16 val,volatile void *addr) {
  *(volatile u16*)addr = val;
}

static inline u8 ioread8(volatile void* addr) {
  return *(volatile u8*)(addr);
}

static inline void iowrite8(u8 val,volatile void *addr) {
  *(volatile u8*)addr = val;
}

static inline u32 readl(volatile void* addr) {
  return *(volatile u32*)(addr);
}

static inline void writel(unsigned int v, volatile void *addr) {
  *(volatile unsigned int *)addr = /*cpu_to_le32*/(v);
}
#endif
/*--------------------------------------------------------*/

/*******************************************************************************
 Function name   : vcmd_read_reg
 Description     : Retrive the content of a hadware register
                    Note: The status register will be read after every MB
                    so it may be needed to buffer it's content if reading
                    the HW register is slow.
 Return type     : u32 
 Argument        : u32 offset
*******************************************************************************/
u32 vcmd_read_reg(const void *hwregs, u32 offset)
{
    u32 val;

    val =(u32) ioread32((void*)hwregs + offset);

    PDEBUG("vcmd_read_reg 0x%02x --> %08x\n", offset, val);

    return val;
}

/*******************************************************************************
 Function name   : vcmd_write_reg
 Description     : Set the content of a hadware register
 Return type     : void 
 Argument        : u32 offset
 Argument        : u32 val
*******************************************************************************/
void vcmd_write_reg(const void *hwregs, u32 offset, u32 val)
{
    iowrite32(val,(void*)hwregs + offset);

    PDEBUG("vcmd_write_reg 0x%02x with value %08x\n", offset, val);
}

/*------------------------------------------------------------------------------

    vcmd_write_register_value

    Write a value into a defined register field (write will happens actually).

------------------------------------------------------------------------------*/
void vcmd_write_register_value(const void *hwregs,u32* reg_mirror,regVcmdName name, u32 value)
{
    const regVcmdField_s *field;
    u32 regVal;

    field = &asicVcmdRegisterDesc[name];

#ifdef DEBUG_PRINT_REGS
    PDEBUG("vcmd_write_register_value 0x%2x  0x%08x  Value: %10d  %s\n",
            field->base, field->mask, value, field->description);
#endif

    /* Check that value fits in field */
    PDEBUG("field->name == name=%d\n",field->name == name);
    PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d\n",((field->mask >> field->lsb) << field->lsb) == field->mask);
    PDEBUG("(field->mask >> field->lsb) >= value=%d\n",(field->mask >> field->lsb) >= value);
    PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT*4=%d\n",field->base < ASIC_VCMD_SWREG_AMOUNT*4);

    /* Clear previous value of field in register */
    regVal = reg_mirror[field->base/4] & ~(field->mask);

    /* Put new value of field in register */
    reg_mirror[field->base/4] = regVal | ((value << field->lsb) & field->mask);

    /* write it into HW registers */
    vcmd_write_reg(hwregs, field->base,reg_mirror[field->base/4]);
}

/*------------------------------------------------------------------------------

    vcmd_get_register_value

    Get an unsigned value from the ASIC registers

------------------------------------------------------------------------------*/
u32 vcmd_get_register_value(const void *hwregs, u32* reg_mirror,regVcmdName name)
{
  const regVcmdField_s *field;
  u32 value;

  field = &asicVcmdRegisterDesc[name];

  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d\n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  value = reg_mirror[field->base / 4] = vcmd_read_reg(hwregs, field->base);
  value = (value & field->mask) >> field->lsb;

  return value;
}
