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
--  Description : Encoder Wrapper Layer (user space module)
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "ewl.h"
#include "hx280enc.h"   /* This EWL uses the kernel module */
#include "ewl_x280_common.h"
#include "ewl_linux_lock.h"

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>

#include <semaphore.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

ENC_MODULE_PATH     defines the path for encoder device driver nod.
                        e.g. "/tmp/dev/hx280"
MEMALLOC_MODULE_PATH defines the path for memalloc device driver nod.
                        e.g. "/tmp/dev/memalloc"
ENC_IO_BASE         defines the IO base address for encoder HW registers
                        e.g. 0xC0000000
SDRAM_LM_BASE       defines the base address for SDRAM as seen by HW
                        e.g. 0x80000000

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*******************************************************************************
 Function name   : EWLWaitHwRdy
 Description     : Wait for the encoder semaphore
 Return type     : i32 
 Argument        : void
*******************************************************************************/
i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady,u32 totalsliceNumber,u32* status_register)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    i32 ret = EWL_HW_WAIT_OK;
    u32 prevSlicesReady = 0;
    u32 core_info = 0;
    i32 core_id = 0;
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);

    PTRACE("EWLWaitHw: Start\n");
        
    if (slicesReady)
        prevSlicesReady = *slicesReady;
        
    /* Check invalid parameters */
    if(enc == NULL)
    {
        assert(0);
        return EWL_HW_WAIT_ERROR;
    }

    core_info |= (FIRST_CORE(enc) << 4);
    core_info |= core_type;
   
    ret = core_info;
    if ((core_id = ioctl(enc->fd_enc, HX280ENC_IOCG_CORE_WAIT, &ret))==-1)
    {
        PTRACE("ioctl HX280ENC_IOCG_CORE_WAIT failed\n");
        ret = EWL_HW_WAIT_ERROR;
        goto out;
    }
    
    if (slicesReady)
       *slicesReady = (enc->reg_all_cores[FIRST_CORE(enc)].core[core_type].pRegBase[7] >> 17) & 0xFF;
    
    if (FIRST_CORE(enc) != core_id)
       ret = EWL_HW_WAIT_ERROR;
out:
    *status_register = ret;
    PTRACE("EWLWaitHw: OK!\n");
    
    return EWL_OK;
}
