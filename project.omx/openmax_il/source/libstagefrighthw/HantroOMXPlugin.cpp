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

//#define LOG_NDEBUG 0
#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "HantroOMXPlugin"

#include <dlfcn.h>
#include <media/stagefright/HardwareAPI.h>
#include <media/stagefright/MediaDebug.h>

#include "HantroOMXPlugin.h"

#define LIBOMXCORE "libhantro_omx_core.so"
namespace android {

extern OMXPluginBase *createOMXPlugin() {
    return new HantroOMXPlugin;
}

extern void destroyOMXPlugin(OMXPluginBase *plugin) {
    delete plugin;
}

HantroOMXPlugin::HantroOMXPlugin()
    : mLibHandle(dlopen(LIBOMXCORE, RTLD_NOW)),
        mInit(NULL),
        mDeinit(NULL),
        mComponentNameEnum(NULL),
        mGetHandle(NULL),
        mFreeHandle(NULL),
        mGetRolesOfComponentHandle(NULL) {

    if (mLibHandle != NULL) {
        mInit = (InitFunc)dlsym(mLibHandle, "hantro_OMX_Init");
        mDeinit = (DeinitFunc)dlsym(mLibHandle, "hantro_OMX_DeInit");

        mComponentNameEnum =
            (ComponentNameEnumFunc)dlsym(mLibHandle, "hantro_OMX_ComponentNameEnum");

        mGetHandle = (GetHandleFunc)dlsym(mLibHandle, "hantro_OMX_GetHandle");
        mFreeHandle = (FreeHandleFunc)dlsym(mLibHandle, "hantro_OMX_FreeHandle");

        mGetRolesOfComponentHandle =
            (GetRolesOfComponentFunc)dlsym(
                    mLibHandle, "hantro_OMX_GetRolesOfComponent");

        (*mInit)();
    }
    else
        ALOGE("Failed to load %s", LIBOMXCORE);
}

HantroOMXPlugin::~HantroOMXPlugin() {
    if (mLibHandle != NULL) {
        (*mDeinit)();

        dlclose(mLibHandle);
        mLibHandle = NULL;
    }
}

OMX_ERRORTYPE HantroOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component) {
    if (mLibHandle == NULL) {
        ALOGE("makeComponentInstance: mLibHandle = NULL");
        return OMX_ErrorUndefined;
    }

    return (*mGetHandle)(
            reinterpret_cast<OMX_HANDLETYPE *>(component),
            const_cast<char *>(name),
            appData, const_cast<OMX_CALLBACKTYPE *>(callbacks));
}

OMX_ERRORTYPE HantroOMXPlugin::destroyComponentInstance(
        OMX_COMPONENTTYPE *component) {
    if (mLibHandle == NULL) {
        ALOGE("destroyComponentInstance: mLibHandle = NULL");
        return OMX_ErrorUndefined;
    }

    return (*mFreeHandle)(reinterpret_cast<OMX_HANDLETYPE *>(component));
}

OMX_ERRORTYPE HantroOMXPlugin::enumerateComponents(
        OMX_STRING name,
        size_t size,
        OMX_U32 index) {
    if (mLibHandle == NULL) {
        ALOGE("enumerateComponents: mLibHandle = NULL");
        return OMX_ErrorUndefined;
    }

    return (*mComponentNameEnum)(name, size, index);
}

OMX_ERRORTYPE HantroOMXPlugin::getRolesOfComponent(
        const char *name,
        Vector<String8> *roles) {
    roles->clear();

    if (mLibHandle == NULL) {
        ALOGE("getRolesOfComponent: mLibHandle = NULL");
        return OMX_ErrorUndefined;
    }

    OMX_U32 numRoles;
    OMX_ERRORTYPE err = (*mGetRolesOfComponentHandle)(
            const_cast<OMX_STRING>(name), &numRoles, NULL);
    ALOGD("Name %s, roles %d", name, (int)numRoles);
    if (err != OMX_ErrorNone) {
        ALOGE("mGetRolesOfComponentHandle: err %d", err);
        return err;
    }

    if (numRoles > 0) {
        OMX_U8 **array = new OMX_U8 *[numRoles];
        for (OMX_U32 i = 0; i < numRoles; ++i) {
            array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        }

        OMX_U32 numRoles2;
        err = (*mGetRolesOfComponentHandle)(
                const_cast<OMX_STRING>(name), &numRoles2, array);

        if (err == OMX_ErrorNone && numRoles != numRoles2) {
            ALOGE("mGetRolesOfComponentHandle: numRoles %d != numRoles2 %d", (int)numRoles, (int)numRoles2);
            err = OMX_ErrorUndefined;
        }

        for (OMX_U32 i = 0; i < numRoles; ++i) {
            if (err == OMX_ErrorNone) {
                String8 s((const char *)array[i]);
                roles->push(s);
            }

            delete[] array[i];
            array[i] = NULL;
        }

        delete[] array;
        array = NULL;
    }

    return err;
}

}  // namespace android
