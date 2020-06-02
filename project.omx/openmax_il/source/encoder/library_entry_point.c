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

#include <st_static_component_loader.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "encoder_version.h"
#include "encoder_constructor.h"

int omx_component_library_Setup(stLoaderComponentType** stComponents)
{
    if (stComponents == NULL)
        return 1; //Number of components

#ifdef OMX_ENCODER_VIDEO_DOMAIN
#ifdef OMX_ENCODER_IMAGE_DOMAIN
#error Both VIDEO and IMAGE domain defined! Only one is allowed!
#endif
#endif

    /** Component 1 - H1 Video / Image Encoder */
    stComponents[0]->componentVersion.s.nVersionMajor = COMPONENT_VERSION_MAJOR;
    stComponents[0]->componentVersion.s.nVersionMinor = COMPONENT_VERSION_MINOR;
    stComponents[0]->componentVersion.s.nRevision =     COMPONENT_VERSION_REVISION;
    stComponents[0]->componentVersion.s.nStep =         COMPONENT_VERSION_STEP;

#ifdef OMX_ENCODER_VIDEO_DOMAIN
int roles = 1; // H264 or HEVC

#ifdef ENCH1
roles++; // VP8
#endif

#ifdef ENC7280
roles += 2; // H263 and Mpeg4
#endif

#ifdef ENCVC8000E
roles++; // H264 and HEVC
#endif

#ifdef ENCH2V41
roles++; // H264 and HEVC
#endif
#endif // OMX_ENCODER_VIDEO_DOMAIN

#ifdef OMX_ENCODER_IMAGE_DOMAIN
int roles = 0;

#ifdef ENABLE_JPEG
roles++;
#endif

#ifdef ENCH1
roles++; // Webp
#endif

#endif // OMX_ENCODER_IMAGE_DOMAIN

const int ROLES = roles;

#ifdef OMX_ENCODER_VIDEO_DOMAIN
    stComponents[0]->constructor = HantroHwEncOmx_hantro_encoder_video_constructor;
    //Component name
    stComponents[0]->name = (char*) calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->name == NULL)
        return OMX_ErrorInsufficientResources;
    strncpy(stComponents[0]->name, VIDEO_COMPONENT_NAME, OMX_MAX_STRINGNAME_SIZE-1);
#endif // OMX_ENCODER_VIDEO_DOMAIN

#ifdef OMX_ENCODER_IMAGE_DOMAIN
    stComponents[0]->constructor = HantroHwEncOmx_hantro_encoder_image_constructor;
    //Component name
    stComponents[0]->name = (char*) calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->name == NULL)
        return OMX_ErrorInsufficientResources;
    strncpy(stComponents[0]->name, IMAGE_COMPONENT_NAME, OMX_MAX_STRINGNAME_SIZE-1);
#endif // OMX_ENCODER_IMAGE_DOMAIN

    //Set amount of component roles and allocate list of role names and roles
    stComponents[0]->name_specific_length = ROLES;
    stComponents[0]->name_specific = (char **) calloc(1, ROLES * sizeof(char *));
    stComponents[0]->role_specific = (char **) calloc(1, ROLES *sizeof(char *));
    if (!stComponents[0]->name_specific || !stComponents[0]->role_specific)
    {
        return OMX_ErrorInsufficientResources;
    }

        // allocate the array members
    int i;

    for(i = 0; i < ROLES; ++i)
    {
        stComponents[0]->name_specific[i] =
            (char *) calloc(1, OMX_MAX_STRINGNAME_SIZE);
        stComponents[0]->role_specific[i] =
            (char *) calloc(1, OMX_MAX_STRINGNAME_SIZE);
        if (stComponents[0]->role_specific[i] == 0 ||
           stComponents[0]->name_specific[i] == 0)
            return OMX_ErrorInsufficientResources;
    }
    int j = 0;
#ifdef OMX_ENCODER_VIDEO_DOMAIN
#if !defined (ENCH2)&& !defined(ENCVC8000E) && !defined(ENCH2V41)

    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_H264,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_H264,
            OMX_MAX_STRINGNAME_SIZE - 1);
    j++;
#endif
#ifdef ENC7280
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_H263,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_H263,
            OMX_MAX_STRINGNAME_SIZE - 1);
    j++;
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_MPEG4,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_MPEG4,
            OMX_MAX_STRINGNAME_SIZE - 1);
#endif
#ifdef ENCH1
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_VP8,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_VP8,
            OMX_MAX_STRINGNAME_SIZE - 1);
#endif
#ifdef ENCH2
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_HEVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_HEVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
#endif
#if defined (ENCVC8000E) || defined (ENCH2V41)
    
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_AVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_AVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
    j++;

    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_HEVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_HEVC,
            OMX_MAX_STRINGNAME_SIZE - 1);
   
#endif

#endif // OMX_ENCODER_VIDEO_DOMAIN

#ifdef OMX_ENCODER_IMAGE_DOMAIN
    j = 0;
#ifdef ENABLE_JPEG
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_JPEG,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_JPEG,
            OMX_MAX_STRINGNAME_SIZE - 1);
    j++;
#endif
#ifdef ENCH1
    strncpy(stComponents[0]->name_specific[j], COMPONENT_NAME_WEBP,
            OMX_MAX_STRINGNAME_SIZE - 1);
    strncpy(stComponents[0]->role_specific[j], COMPONENT_ROLE_WEBP,
            OMX_MAX_STRINGNAME_SIZE - 1);
#endif
#endif // OMX_ENCODER_IMAGE_DOMAIN

    return 1;
}
