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

#include <dlfcn.h>   /* For dynamic loading */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_ComponentRegistry.h"

#ifdef HANTRO_PARSER
/** determine capabilities of a component before acually using it */
#include "hantro_omx_config_parser.h"
#endif

#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "Hantro_omx_core"
#define LOG_NDEBUG 0

#ifndef ALOGI
#define ALOGI LOGI
#endif
#ifndef ALOGD
#define ALOGD LOGD
#endif
#ifndef ALOGE
#define ALOGE LOGE
#endif
#ifndef ALOGW
#define ALOGW LOGW
#endif

/** size for the array of allocated components.  Sets the maximum
 * number of components that can be allocated at once */
#define MAXCOMP (15)
#define MAXNAMESIZE (130)
#define EMPTY_STRING "\0"

/** Determine the number of elements in an array */
#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))

/** Array to hold the DLL pointers for each allocated component */
static void* pModules[MAXCOMP] = {0};

/** Array to hold the component handles for each allocated component */
static void* pComponents[COUNTOF(pModules)] = {0};

/** count will be used as a reference counter for OMX_Init()
    so all changes to count should be mutex protected */
int count = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

OMX_U32 tableCount = 0;
ComponentTable componentTable[MAX_TABLE_SIZE];
char * sRoleArray[60][20];
char compName[60][200];

/*  Remember to use correct component names and roles
    based on your product configuration.
    Hantro video codec component names:
    G1 decoder:     OMX.hantro.G1.video.decoder
    H1 encoder:     OMX.hantro.H1.video.encoder
    8290 encoder:   OMX.hantro.8290.video.encoder
    8270 encoder:   OMX.hantro.8270.video.encoder
    7280 encoder:   OMX.hantro.7280.video.encoder

    Names and roles are defined in version.h and
    encoder_version.h
*/
/* video and image components */ //CJ TODO update to SE1000
char *tComponentName[MAXCOMP][2] = {
    {"OMX.hantro.G1.video.decoder", "video_decoder.mpeg4"},
    {"OMX.hantro.G1.video.decoder", "video_decoder.avc"},
    {"OMX.hantro.G1.video.decoder", "video_decoder.h263"},
    {"OMX.hantro.G1.video.decoder", "video_decoder.vp8"},
    {"OMX.hantro.G1.image.decoder", "image_decoder.jpeg"},
    {"OMX.hantro.G1.image.decoder", "image_decoder.webp"},
    {"OMX.hantro.H2.video.encoder", "video_encoder.avc"},
    {"OMX.hantro.H1.video.encoder", "video_encoder.vp8"},
    {"OMX.hantro.H1.image.encoder", "image_encoder.jpeg"},
    {"OMX.hantro.H1.image.encoder", "image_encoder.webp"},
    {NULL, NULL}
};

/******************************Public*Routine******************************\
* OMX_Init()
*
* Description:This method will initialize the OMX Core. It is the
* responsibility of the application to call OMX_Init to ensure the proper
* set up of core resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
//CJ TODO change to OMX_Init()
OMX_ERRORTYPE hantro_OMX_Init()
{
    ALOGD("hantro_OMX_Init... in");
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if(pthread_mutex_lock(&mutex) != 0)
    {
        ALOGE("Error in Mutex lock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    count++;
    ALOGD("init count = %d", count);

    if (count == 1)
    {
        eError = hantro_OMX_BuildComponentTable();
    }

    if(pthread_mutex_unlock(&mutex) != 0)
    {
        ALOGE("Error in Mutex unlock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    ALOGD("hantro_OMX_Init... out");
    return eError;
}
/******************************Public*Routine******************************\
* OMX_GetHandle
*
* Description: This method will create the handle of the COMPONENTTYPE
* If the component is currently loaded, this method will return the
* hadle of existingcomponent or create a new instance of the component.
* It will call the OMX_ComponentInit function and then the setcallback
* method to initialize the callback functions
* Parameters:
* @param[out] pHandle           Handle of the loaded components
* @param[in] cComponentName     Name of the component to load
* @param[in] pAppData           Used to identify the callbacks of component
* @param[in] pCallBacks         Application callbacks
*
* @retval OMX_ErrorUndefined
* @retval OMX_ErrorInvalidComponentName
* @retval OMX_ErrorInvalidComponent
* @retval OMX_ErrorInsufficientResources
* @retval OMX_NOERROR                      Successful
*
* Note
*
\**************************************************************************/

OMX_ERRORTYPE hantro_OMX_GetHandle( OMX_HANDLETYPE* pHandle, OMX_STRING cComponentName,
    OMX_PTR pAppData, OMX_CALLBACKTYPE* pCallBacks)
{
    ALOGD("hantro_OMX_GetHandle... in");
    static const char prefix[] = "lib";
    static const char postfix[] = ".so";
    OMX_ERRORTYPE (*pComponentInit)(OMX_COMPONENTTYPE*, OMX_STRING);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_COMPONENTTYPE *componentType;
    OMX_S32 i;
    const char* pErr = dlerror();

    ALOGD("Component name: %s", cComponentName);

    if(pthread_mutex_lock(&mutex) != 0)
    {
        ALOGE("Core: Error in Mutex lock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    if ((NULL == cComponentName) || (NULL == pHandle) || (NULL == pCallBacks)) {
        err = OMX_ErrorBadParameter;
        goto UNLOCK_MUTEX;
    }

    /* Verify that the name is not too long and could cause a crash.  Notice
     * that the comparison is a greater than or equals.  This is to make
     * sure that there is room for the terminating NULL at the end of the
     * name. */
    if(strlen(cComponentName) >= MAXNAMESIZE) {
        err = OMX_ErrorInvalidComponentName;
        goto UNLOCK_MUTEX;
    }
    /* Locate the first empty slot for a component.  If no slots
     * are available, error out */
    for(i=0; i< (int)COUNTOF(pModules); i++) {
        if(pModules[i] == NULL) break;
    }
    if(i == COUNTOF(pModules)) {
        err = OMX_ErrorInsufficientResources;
        goto UNLOCK_MUTEX;
    }

    int refIndex = 0;
    for (refIndex=0; refIndex < MAX_TABLE_SIZE; refIndex++) {
        //get the index for the component in the table
        if (strcmp(componentTable[refIndex].name, cComponentName) == 0) {
            ALOGD("Found component %s with refCount %d",
                  cComponentName, componentTable[refIndex].refCount);

            /* check if the component is already loaded */
            if (componentTable[refIndex].refCount >= MAX_CONCURRENT_INSTANCES) {
                err = OMX_ErrorInsufficientResources;
                ALOGE("Max instances of component %s already created", cComponentName);
                goto UNLOCK_MUTEX;
            } else {  // we have not reached the limit yet
                /* do what was done before need to limit concurrent instances of each component */

                /* load the component and check for an error.  If filename is not an
                 * absolute path (i.e., it does not  begin with a "/"), then the
                 * file is searched for in the following locations:
                 *
                 *     The LD_LIBRARY_PATH environment variable locations
                 *     The library cache, /etc/ld.so.cache.
                 *     /lib
                 *     /usr/lib
                 *
                 * If there is an error, we can't go on, so set the error code and exit */

                /* the lengths are defined herein or have been
                 * checked already, so strcpy and strcat are
                 * are safe to use in this context. */
                char buf[sizeof(prefix) + MAXNAMESIZE + sizeof(postfix)];
                strcpy(buf, prefix);
                strcat(buf, cComponentName);
                strcat(buf, postfix);

                pModules[i] = dlopen(buf, RTLD_LAZY | RTLD_GLOBAL);
                if( pModules[i] == NULL ) {
                    ALOGE("dlopen %s failed because %s", buf, dlerror());
                    err = OMX_ErrorComponentNotFound;
                    goto UNLOCK_MUTEX;
                }

                /* Get a function pointer to the "OMX_ComponentInit" function.  If
                 * there is an error, we can't go on, so set the error code and exit */
                if (strcmp("OMX.hantro.G1.video.decoder", cComponentName) == 0) {
                    pComponentInit = dlsym(pModules[i], "HantroHwDecOmx_video_constructor");
                    ALOGD("pComponentInit = HantroHwDecOmx_video_constructor");
                }
                else if (strcmp("OMX.hantro.G1.image.decoder", cComponentName) == 0) {
                    pComponentInit = dlsym(pModules[i], "HantroHwDecOmx_image_constructor");
                    ALOGD("pComponentInit = HantroHwDecOmx_image_constructor");
                }
                else if (strcmp("OMX.hantro.H1.video.encoder", cComponentName) == 0) {
                    pComponentInit = dlsym(pModules[i], "HantroHwEncOmx_hantro_encoder_video_constructor");
                    ALOGD("pComponentInit = HantroHwEncOmx_hantro_encoder_video_constructor");
                }
                else if (strcmp("OMX.hantro.H1.image.encoder", cComponentName) == 0) {
                    pComponentInit = dlsym(pModules[i], "HantroHwEncOmx_hantro_encoder_image_constructor");
                    ALOGD("pComponentInit = HantroHwEncOmx_hantro_encoder_image_constructor");
                }
                else {
                    ALOGE("Could not get function pointer: Unknown component");
                    pComponentInit = NULL;
                }

                pErr = dlerror();
                if( (pErr != NULL) || (pComponentInit == NULL) ) {
                    ALOGE("dlsym failed for module %p (%d)", pModules[i], __LINE__);
                    err = OMX_ErrorInvalidComponent;
                    goto CLEAN_UP;
                }

               /* We now can access the dll.  So, we need to call the "OMX_ComponentInit"
                * method to load up the "handle" (which is just a list of functions to
                * call) and we should be all set.*/
                *pHandle = calloc(1, sizeof(OMX_COMPONENTTYPE));
                if(*pHandle == NULL) {
                    err = OMX_ErrorInsufficientResources;
                    ALOGE("malloc of pHandle* failed (%d)", __LINE__);
                    goto CLEAN_UP;
                }

                pComponents[i] = *pHandle;
                componentType = (OMX_COMPONENTTYPE*) *pHandle;
                componentType->nSize = sizeof(OMX_COMPONENTTYPE);
            //    err = (*pComponentInit)(componentType, "OMX.hantro.G1.video.decoder");
                err = (*pComponentInit)(componentType, cComponentName);
                if (OMX_ErrorNone == err) {
                    err = (componentType->SetCallbacks)(*pHandle, pCallBacks, pAppData);
                    if (err != OMX_ErrorNone) {
                        ALOGE("SetCallBack failed %d (%d)", err, __LINE__);
                        goto CLEAN_UP;
                    }
                    /* finally, OMX_ComponentInit() was successful and
                       SetCallbacks was successful, we have a valid instance,
                       so no we increment refCount */
                    componentTable[refIndex].pHandle[componentTable[refIndex].refCount] = *pHandle;
                    componentTable[refIndex].refCount += 1;
                    goto UNLOCK_MUTEX;  // Component is found, and thus we are done
                }
                else if (err == OMX_ErrorInsufficientResources) {
                        ALOGE("Insufficient Resources for Component %d (%d)", err, __LINE__);
                        goto CLEAN_UP;
                }
            }
        }
    }
    // If we are here, we have not found the component
    ALOGE("OMX_ErrorComponentNotFound");
    err = OMX_ErrorComponentNotFound;
    goto UNLOCK_MUTEX;
CLEAN_UP:
    ALOGD("Clean up");
    if(*pHandle != NULL)
    /* cover the case where we error out before malloc'd */
    {
        free(*pHandle);
        *pHandle = NULL;
    }
    pComponents[i] = NULL;
    dlclose(pModules[i]);
    pModules[i] = NULL;

UNLOCK_MUTEX:
    ALOGD("Unlock mutex");
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        ALOGE("Error in Mutex unlock (%d)", __LINE__);
        err = OMX_ErrorUndefined;
    }
    ALOGD("hantro_OMX_GetHandle... out");
    return (err);
}


/******************************Public*Routine******************************\
* OMX_FreeHandle()
*
* Description:This method will unload the OMX component pointed by
* OMX_HANDLETYPE. It is the responsibility of the calling method to ensure that
* the Deinit method of the component has been called prior to unloading component
*
* Parameters:
* @param[in] hComponent the component to unload
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE hantro_OMX_FreeHandle (OMX_HANDLETYPE hComponent)
{
    ALOGD("hantro_OMX_FreeHandle... in");

    OMX_ERRORTYPE retVal = OMX_ErrorUndefined;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)hComponent;
    OMX_S32 i;

    if(pthread_mutex_lock(&mutex) != 0)
    {
        ALOGE("Error in Mutex lock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    /* Locate the component handle in the array of handles */
    for(i=0; i< (int)COUNTOF(pModules); i++) {
        if(pComponents[i] == hComponent) break;
    }

    if(i == COUNTOF(pModules)) {
        ALOGE("Component %p is not found (%d)", hComponent, __LINE__);
        retVal = OMX_ErrorBadParameter;
        goto EXIT;
    }

    retVal = pHandle->ComponentDeInit(hComponent);
    if (retVal != OMX_ErrorNone) {
        ALOGE("ComponentDeInit failed %d (%d)", retVal, __LINE__);
        goto EXIT;
    }

    int refIndex = 0, handleIndex = 0;
    for (refIndex=0; refIndex < MAX_TABLE_SIZE; refIndex++) {
        for (handleIndex=0; handleIndex < MAX_CONCURRENT_INSTANCES; handleIndex++){
//        for (handleIndex=0; handleIndex < componentTable[refIndex].refCount; handleIndex++){
            /* get the position for the component in the table */
            if (componentTable[refIndex].pHandle[handleIndex] == hComponent){
                ALOGD("Found matching pHandle(%p) at index %d with refCount %d",
                      hComponent, refIndex, componentTable[refIndex].refCount);
                if (componentTable[refIndex].refCount) {
                    componentTable[refIndex].refCount -= 1;
                }
                componentTable[refIndex].pHandle[handleIndex] = NULL;
                dlclose(pModules[i]);
                pModules[i] = NULL;
                free(pComponents[i]);
                pComponents[i] = NULL;
                retVal = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

    // If we are here, we have not found the matching component
    retVal = OMX_ErrorComponentNotFound;

EXIT:
    /* The unload is now complete, so set the error code to pass and exit */
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        ALOGE("Error in Mutex unlock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    ALOGD("hantro_OMX_FreeHandle... out");
    return retVal;
}

/******************************Public*Routine******************************\
* OMX_DeInit()
*
* Description:This method will release the resources of the OMX Core. It is the
* responsibility of the application to call OMX_DeInit to ensure the clean up of these
* resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE hantro_OMX_Deinit()
{
    ALOGD("hantro_OMX_Deinit... in");

    if(pthread_mutex_lock(&mutex) != 0){
        ALOGE("Error in Mutex lock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    if (count) {
        count--;
    }

    ALOGD("deinit count = %d", count);

    if(pthread_mutex_unlock(&mutex) != 0) {
        ALOGE("Error in Mutex unlock (%d)", __LINE__);
        return OMX_ErrorUndefined;
    }

    ALOGD("hantro_OMX_Deinit... out");
    return OMX_ErrorNone;
}

/*************************************************************************
* OMX_SetupTunnel()
*
* Description: Setup the specified tunnel the two components
*
* Parameters:
* @param[in] hOutput     Handle of the component to be accessed
* @param[in] nPortOutput Source port used in the tunnel
* @param[in] hInput      Component to setup the tunnel with.
* @param[in] nPortInput  Destination port used in the tunnel
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
/* OMX_SetupTunnel */
OMX_API OMX_ERRORTYPE OMX_APIENTRY hantro_OMX_SetupTunnel(
    OMX_IN  OMX_HANDLETYPE hOutput,
    OMX_IN  OMX_U32 nPortOutput,
    OMX_IN  OMX_HANDLETYPE hInput,
    OMX_IN  OMX_U32 nPortInput)
{
    ALOGD("hantro_OMX_SetupTunnel... in");

    OMX_ERRORTYPE eError = OMX_ErrorNotImplemented;
    OMX_COMPONENTTYPE *pCompIn, *pCompOut;
    OMX_TUNNELSETUPTYPE oTunnelSetup;

    if (hOutput == NULL && hInput == NULL)
    {
        ALOGE("No input handle and output handle");
        return OMX_ErrorBadParameter;
    }

    oTunnelSetup.nTunnelFlags = 0;
    oTunnelSetup.eSupplier = OMX_BufferSupplyUnspecified;

    pCompOut = (OMX_COMPONENTTYPE*)hOutput;

    if (hOutput)
    {
        eError = pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, hInput, nPortInput, &oTunnelSetup);
    }


    if (eError == OMX_ErrorNone && hInput)
    {
        pCompIn = (OMX_COMPONENTTYPE*)hInput;
        eError = pCompIn->ComponentTunnelRequest(hInput, nPortInput, hOutput, nPortOutput, &oTunnelSetup);
        if (eError != OMX_ErrorNone && hOutput)
        {
            /* cancel tunnel request on output port since input port failed */
            ALOGE("Input port failed");
            pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, NULL);
        }
    }

    ALOGD("hantro_OMX_SetupTunnel... out ");
    return eError;
}

/*************************************************************************
* OMX_ComponentNameEnum()
*
* Description: This method will provide the name of the component at the given nIndex
*
*Parameters:
* @param[out] cComponentName       The name of the component at nIndex
* @param[in] nNameLength           The length of the component name
* @param[in] nIndex                The index number of the component
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY hantro_OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    ALOGD("hantro_OMX_ComponentNameEnum... in");
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (nIndex >= tableCount)
    {
        eError = OMX_ErrorNoMore;
    }
    else
    {
        ALOGD("cComponentName %s", componentTable[nIndex].name);
        strcpy(cComponentName, componentTable[nIndex].name);
    }

    ALOGD("hantro_OMX_ComponentNameEnum... out ");
    return eError;
}


/*************************************************************************
* OMX_GetRolesOfComponent()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] cComponentName     The name of the component to query
* @param[in] pNumRoles          The number of roles supported by the component
* @param[in] roles		        The roles of the component
*
* Returns:    OMX_NOERROR          Successful
*             OMX_ErrorBadParameter		Failure due to a bad input parameter
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE hantro_OMX_GetRolesOfComponent (
    OMX_IN      OMX_STRING cComponentName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles)
{
    ALOGD("hantro_OMX_GetRolesOfComponent... in");

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 j = 0;
    OMX_BOOL bFound = OMX_FALSE;

    if (cComponentName == NULL || pNumRoles == NULL)
    {
        if (cComponentName == NULL)
        {
            ALOGE("cComponentName is NULL");
        }
        if (pNumRoles == NULL)
        {
            ALOGE("pNumRoles is NULL");
        }
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    while (i < tableCount)
    {
        if (strcmp(cComponentName, componentTable[i].name) == 0)
        {
            bFound = OMX_TRUE;
            break;
        }
        i++;
    }
    if (!bFound)
    {
        eError = OMX_ErrorComponentNotFound;
        ALOGE("Component %s not found", cComponentName);
        goto EXIT;
    }
    if (roles == NULL)
    {
        *pNumRoles = componentTable[i].nRoles;
    }
    else
    {
        /* must be second of two calls,
           pNumRoles is input in this context.
           If pNumRoles is < actual number of roles
           than we return an error */
        if (*pNumRoles >= componentTable[i].nRoles)
        {
            for (j = 0; j<componentTable[i].nRoles; j++)
            {
                strcpy((OMX_STRING)roles[j], componentTable[i].pRoleArray[j]);
            }
            *pNumRoles = componentTable[i].nRoles;
        }
        else
        {
            eError = OMX_ErrorBadParameter;
            ALOGE("pNumRoles (%d) is less than actual number (%d) of roles \
                   for this component %s", (int)*pNumRoles, componentTable[i].nRoles, cComponentName);
        }
    }
    EXIT:
    ALOGD("hantro_OMX_GetRolesOfComponent... out");
    return eError;
}

/*************************************************************************
* OMX_GetComponentsOfRole()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] role          The role name to query for
* @param[in] pNumComps     The number of components supporting the given role
* @param[in] compNames     The names of the components supporting the given role
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE hantro_OMX_GetComponentsOfRole (
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames)
{
    ALOGD("hantro_OMX_GetComponentsOfRole... in ");
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 j = 0;
    OMX_U32 k = 0;
    OMX_U32 compOfRoleCount = 0;

    if (role == NULL || pNumComps == NULL)
    {
       if (role == NULL)
       {
           ALOGE("role is NULL");
       }
       if (pNumComps == NULL)
       {
           ALOGE("pNumComps is NULL");
       }
       eError = OMX_ErrorBadParameter;
       goto EXIT;
    }

   /* This implies that the componentTable is not filled */
    if (!tableCount)
    {
        eError = OMX_ErrorUndefined;
        ALOGE("Component table is empty. Please reload OMX Core");
        goto EXIT;
    }

    /* no matter, we always want to know number of matching components
       so this will always run */
    for (i = 0; i < tableCount; i++)
    {
        for (j = 0; j < componentTable[i].nRoles; j++)
        {
            if (strcmp(componentTable[i].pRoleArray[j], role) == 0)
            {
                /* the first call to this function should only count the number
                   of roles
                */
                compOfRoleCount++;
            }
        }
    }
    if (compOfRoleCount == 0)
    {
        eError = OMX_ErrorComponentNotFound;
        ALOGE("Component supporting role %s was not found", role);
    }
    if (compNames == NULL)
    {
        /* must be the first of two calls */
        *pNumComps = compOfRoleCount;
    }
    else
    {
        /* must be the second of two calls */
        if (*pNumComps < compOfRoleCount)
        {
            /* pNumComps is input in this context,
               it can not be less, this would indicate
               the array is not large enough
            */
            eError = OMX_ErrorBadParameter;
            ALOGE("pNumComps (%d) is less than the actual number (%d) of components \
                  supporting role %s", (int)*pNumComps, (int)compOfRoleCount, role);
        }
        else
        {
            k = 0;
            for (i = 0; i < tableCount; i++)
            {
                for (j = 0; j < componentTable[i].nRoles; j++)
                {
                    if (strcmp(componentTable[i].pRoleArray[j], role) == 0)
                    {
                        /*  the second call compNames can be allocated
                            with the proper size for that number of roles.
                        */
                        compNames[k] = (OMX_U8*)componentTable[i].name;
                        k++;
                        if (k == compOfRoleCount)
                        {
                            /* there are no more components of this role
                               so we can exit here */
                            *pNumComps = k;
                            goto EXIT;
                        }
                    }
                }
            }
        }
    }

    EXIT:
    ALOGD("hantro_OMX_GetComponentsOfRole... out ");
    return eError;
}

OMX_ERRORTYPE hantro_OMX_BuildComponentTable()
{
    ALOGD("hantro_OMX_BuildComponentTable... in");

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CALLBACKTYPE sCallbacks;
    int j = 0;
    int numFiles = 0;
    int i;

    for (i = 0, numFiles = 0; i < MAXCOMP; i ++) {
        if (tComponentName[i][0] == NULL) {
            break;
        }
        if (numFiles <= MAX_TABLE_SIZE){
            for (j = 0; j < numFiles; j ++) {
                if (!strcmp(componentTable[j].name, tComponentName[i][0])) {
                    /* insert the role */
                    if (tComponentName[i][1] != NULL)
                    {
                        componentTable[j].pRoleArray[componentTable[j].nRoles] = tComponentName[i][1];
                        componentTable[j].pHandle[componentTable[j].nRoles] = NULL; //initilize the pHandle element
                        componentTable[j].nRoles ++;
                    }
                    break;
                }
            }
            if (j == numFiles) { /* new component */
                if (tComponentName[i][1] != NULL){
                    componentTable[numFiles].pRoleArray[0] = tComponentName[i][1];
                    componentTable[numFiles].nRoles = 1;
                }
                strcpy(compName[numFiles], tComponentName[i][0]);
                componentTable[numFiles].name = compName[numFiles];
                componentTable[numFiles].refCount = 0; //initialize reference counter.
                numFiles ++;
            }
        }
    }
    tableCount = numFiles;
    if (eError != OMX_ErrorNone){
        ALOGE("Could not build Component Table");
    }

    ALOGD("hantro_OMX_BuildComponentTable... out");
    return eError;
}

#ifdef HANTRO_PARSER
OMX_BOOL hantro_OMXConfigParserRedirect(
    OMX_PTR aInputParameters,
    OMX_PTR aOutputParameters)

{
    OMX_BOOL Status = OMX_FALSE;

    Status = hantro_OMXConfigParser(aInputParameters, aOutputParameters);

    ALOGD("OMX core: hantro_OMXConfigParser status %d", Status);
    return Status;
}
#endif
