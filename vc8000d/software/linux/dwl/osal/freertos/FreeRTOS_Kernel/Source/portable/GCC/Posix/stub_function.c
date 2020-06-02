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

///////////////////////////////////////////////////////////////////////////////////////
#ifndef USE_FREERTOS_SIMULATOR
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_POSIX/unistd.h"
#endif
///////////////////////////////////////////////////////////////////////////////////////

/* This is a stub function for FreeRTOS_Kernel */
void vMainQueueSendPassed( void )
{
    return;
}

/* This is a stub function for FreeRTOS_Kernel */
void vApplicationIdleHook( void )
{
    return;
}

///////////////////////////////////////////////////////////////////////////////////////
#ifndef USE_FREERTOS_SIMULATOR
/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
* implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
* used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t ** ppxIdleTaskTCBBuffer,
	StackType_t ** ppxIdleTaskStackBuffer,
	uint32_t * pulIdleTaskStackSize)
{
  return;
}
#endif
///////////////////////////////////////////////////////////////////////////////////////

