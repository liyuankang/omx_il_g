/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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
------------------------------------------------------------------------------*/

#include <OMX_Core.h>
#include "encoder_version.h"
#include "encoder_constructor.h"
#include "encoder.h"

/**
 * Component construction function, executed when OMX_GetHandle()
 * is called.
*/
OMX_ERRORTYPE HantroHwEncOmx_hantro_encoder_video_constructor(OMX_COMPONENTTYPE *openmaxStandComp,
        OMX_STRING cComponentName)
{
    OMX_ERRORTYPE err;

    if (strcmp(cComponentName, VIDEO_COMPONENT_NAME))
    {
        DBGT_ERROR("Invalid component name: %s", cComponentName);
        return OMX_ErrorInvalidComponentName;
    }

    openmaxStandComp->nSize = sizeof(OMX_COMPONENTTYPE);
    openmaxStandComp->pComponentPrivate = NULL;
    openmaxStandComp->pApplicationPrivate = NULL;
    openmaxStandComp->nVersion.s.nVersionMajor = COMPONENT_VERSION_MAJOR;
    openmaxStandComp->nVersion.s.nVersionMinor = COMPONENT_VERSION_MINOR;
    openmaxStandComp->nVersion.s.nRevision = COMPONENT_VERSION_REVISION;
    openmaxStandComp->nVersion.s.nStep = COMPONENT_VERSION_STEP;

    err = HantroHwEncOmx_encoder_init(openmaxStandComp);

    return err;
}
