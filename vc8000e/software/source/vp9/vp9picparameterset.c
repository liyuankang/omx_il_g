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
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vp9picparameterset.h"
#include "enccommon.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  PicParameterSetAlloc
------------------------------------------------------------------------------*/
i32 PicParameterSetAlloc(ppss *ppss, i32 mbPerPicture)
{
  i32 status = ENCHW_OK;

  ppss->size = 1;
  ppss->store = (pps *)EWLcalloc(ppss->size, sizeof(pps));
  if (ppss->store == NULL) return ENCHW_NOK;

  if (status != ENCHW_OK)
  {
    PicParameterSetFree(ppss);
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------
  PicParameterSetFree
------------------------------------------------------------------------------*/
void PicParameterSetFree(ppss *ppss)
{
  if (ppss->store == NULL) return;

  EWLfree(ppss->store);
  ppss->store = NULL;
}
