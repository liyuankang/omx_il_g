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
--  Abstract : Cache Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/
#ifndef __CWL_COMMON_H__
#define __CWL_COMMON_H__

#include <stdio.h>

extern FILE *fCwl;

/* Macro for debug printing */
#undef PTRACE
#ifdef TRACE_CWL
#   include <stdio.h>
#   define PTRACE printf
#else
#   define PTRACE(...)  /* no trace */
#endif

/* the cache device driver node */
#ifndef CACHE_MODULE_PATH
#define CACHE_MODULE_PATH             "/tmp/dev/hantro_cache"
#endif

enum reg_amount
{
 CACHE_RD_SWREG_AMOUNT = 6+4*8,
 CACHE_WR_SWREG_AMOUNT = 5+5*16,
 CACHE_SWREG_MAX = 200,
 L2_SWREG_TOTAL = 0x400
};

/* Flags for read-only, write-only and read-write */
#define RO 1
#define WO 2
#define RW 3


/* HW Register field names */
typedef enum
{

#include "registernum.h"

  CacheRegisterAmount

} regName;



/* HW Register field descriptions */
typedef struct
{
  u32 name;               /* Register name and index  */
  i32 base;               /* Register base address  */
  u32 mask;               /* Bitmask for this field */
  i32 lsb;                /* LSB for this field [31..0] */
  i32 trace;              /* Enable/disable writing in swreg_params.trc */
  i32 rw;                 /* 1=Read-only 2=Write-only 3=Read-Write */
  char *description;      /* Field description */
} regField_s;

#define H2REG(name, base, mask, lsb, trace, rw, desc) \
        {name, base, mask, lsb, trace, rw, desc}

#endif
