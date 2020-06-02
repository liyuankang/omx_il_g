/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
--                                                                            --
-  Description : Video stabilization common stuff for standalone and pipeline
-
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "vidstabcommon.h"

/*------------------------------------------------------------------------------
    Function name   : VSReadStabData
    Description     : 
    Return type     : void 
    Argument        : const u32 * regMirror
    Argument        : HWStabData * hwStabData
------------------------------------------------------------------------------*/
void VSReadStabData(const u32 * regMirror, HWStabData * hwStabData)
{
    i32 i;
    u32 *matrix;
    const u32 *reg;

    hwStabData->rMotionMin = (regMirror[367] & ((1 << 26) - 1));
    hwStabData->rMotionSum = regMirror[368];
    if (hwStabData->rMotionSum*32 > hwStabData->rMotionSum)  /* no overflow */
        hwStabData->rMotionSum *= 32;
    else
        hwStabData->rMotionSum = 0xFFFFFFFF;

    hwStabData->rGmvX = ((i32) (regMirror[369]) >> 26);
    hwStabData->rGmvY = ((i32) (regMirror[370]) >> 26);

#ifdef TRACE_VIDEOSTAB_INTERNAL
    DEBUG_PRINT(("%8d %6d %4d %4d", hwStabData->rMotionSum,
                 hwStabData->rMotionMin, hwStabData->rGmvX, hwStabData->rGmvY));
#endif

    matrix = hwStabData->rMatrixVal;
    reg = &regMirror[369];

    for (i = 9; i > 0; i--)
    {
        *matrix++ = (*reg++) & ((1 << 26) - 1);

#ifdef TRACE_VIDEOSTAB_INTERNAL
        DEBUG_PRINT((" %6d", matrix[-1]));
#endif

    }

#ifdef TRACE_VIDEOSTAB_INTERNAL
    DEBUG_PRINT(("\n"));
#endif

}
