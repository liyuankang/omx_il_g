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
--  Description :  Encoder system model
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Function prototypes

------------------------------------------------------------------------------*/
#ifndef ENC_CORE_H
#define ENC_CORE_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "encbasetype.h"
#include "encswhwregisters.h"

#include <pthread.h>
#include <stdio.h>

#define CORE_NUM 4
#define MAX_CORE_NUM 4

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
typedef struct {
  struct picture *hevc;
  struct JpegEncContainer_t *jpeg;
  struct cuTreeAsicCtl *cutree;
} WorkInst;

typedef struct {
  /* hw version */
  u32 asic_id;
  /* features */
  u32 fuse1;
  u32 fuse2;
  u32 fuse3;
  u32 fuse4;
} encHwCfg;

typedef struct {
  i32 core_id;
  encHwCfg cfg;
  u32 asicRegs[ASIC_SWREG_AMOUNT]; /* ASIC emulation register table */
  
  /* for access some special registers */
  pthread_mutex_t lock;
} SysCore;


/*------------------------------------------------------------------------------
    4.  Function prototypes
------------------------------------------------------------------------------*/

u32 CoreEncGetCoreNum();

SysCore *CoreEncSetup(u32 core_id);
i32 CoreEncSetHwCfg(SysCore *core, encHwCfg *cfg);
i32 CoreEncTryReserveHw(SysCore *core);
i32 CoreEncReleaseHw(SysCore *core);
i32 CoreEncRun(SysCore *core, u32 offset, u32 val, WorkInst *inst);
i32 CoreEncWaitHwRdy(SysCore *core);
i32 CoreEncRelease(SysCore *core, WorkInst *inst, u32 clientType);

/* Functions for get/set whole register (32bits) */
u32 CoreEncGetRegister(SysCore *core, u32 base);
void CoreEncSetRegister(SysCore *core, u32 base, u32 value);

/* Functions for get/set one field in a register */
u32 CoreEncGetRegisterValue(SysCore *core, regName name);
i32 CoreEncGetRegisterValueSigned(SysCore *core, regName name);

void CoreEncSetRegisterValue(SysCore *core, regName name, u32 value);
void CoreEncSetRegisterValueSigned(SysCore *core, regName name, i32 value);
void CoreEncGetAsicConfig(i32 core_id, u32 *regs, EWLHwConfig_t *cfg);

#if defined(__LP64__) || defined(_WIN64) || defined(_WIN32)

#define CoreEncSetAddrRegisterValue(inst, REGBASE, addr) do {\
    CoreEncSetRegisterValue((inst), REGBASE, (u32)(addr));  \
    CoreEncSetRegisterValue((inst), REGBASE##_MSB, (u32)((addr) >> 32)); \
  } while (0)

#define CoreEncGetAddrRegisterValue(inst, REGBASE)  \
  (((ptr_t)CoreEncGetRegisterValue((inst), REGBASE)) |  \
  (((ptr_t)CoreEncGetRegisterValue((inst), REGBASE##_MSB)) << 32))

#else

#define CoreEncSetAddrRegisterValue(inst, REGBASE, addr) do {\
    CoreEncSetRegisterValue((inst), REGBASE, (u32)(addr));  \
  } while (0)

#define CoreEncGetAddrRegisterValue(inst, REGBASE)  \
  ((ptr_t)CoreEncGetRegisterValue((inst), REGBASE))

#endif

#endif /* ENC_CORE_H */
