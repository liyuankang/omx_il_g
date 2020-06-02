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

#ifdef ANDROID

#define OMX_google_android_index_enableAndroidNativeBuffers  0x7FFF0000
#define OMX_google_android_index_getAndroidNativeBufferUsage 0x7FFF0001
#define OMX_google_android_index_useAndroidNativeBuffer      0x7FFF0002
#define OMX_google_android_index_prependSPSPPSToIDRFrames    0x7FFF0003


#ifdef USE_ANDROID_NATIVE_BUFFER
//#include <hardware/libhardware/include/hardware/gralloc.h>
#include <hardware/gralloc.h>
// cannot include because C++ header ?
//
//
//#include "media/stagefright/HardwareAPI.h"

// A pointer to this struct is passed to the OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.enableAndroidNativeBuffers' extension
// is given.
//
// When Android native buffer use is disabled for a port (the default state),
// the OMX node should operate as normal, and expect UseBuffer calls to set its
// buffers.  This is the mode that will be used when CPU access to the buffer is
// required.
//
// When Android native buffer use has been enabled for a given port, the video
// color format for the port is to be interpreted as an Android pixel format
// rather than an OMX color format.  The node should then expect to receive
// UseAndroidNativeBuffer calls (via OMX_SetParameter) rather than UseBuffer
// calls for that port.
struct EnableAndroidNativeBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL enable;
};

// A pointer to this struct is passed to OMX_SetParameter() when the extension
// index "OMX.google.android.index.storeMetaDataInBuffers"
// is given.
//
// When meta data is stored in the video buffers passed between OMX clients
// and OMX components, interpretation of the buffer data is up to the
// buffer receiver, and the data may or may not be the actual video data, but
// some information helpful for the receiver to locate the actual data.
// The buffer receiver thus needs to know how to interpret what is stored
// in these buffers, with mechanisms pre-determined externally. How to
// interpret the meta data is outside of the scope of this method.
//
// Currently, this is specifically used to pass meta data from video source
// (camera component, for instance) to video encoder to avoid memcpying of
// input video frame data. To do this, bStoreMetaDta is set to OMX_TRUE.
// If bStoreMetaData is set to false, real YUV frame data will be stored
// in the buffers. In addition, if no OMX_SetParameter() call is made
// with the corresponding extension index, real YUV data is stored
// in the buffers.
struct StoreMetaDataInBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
};

// A pointer to this struct is passed to OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.useAndroidNativeBuffer' extension is
// given.  This call will only be performed if a prior call was made with the
// 'OMX.google.android.index.enableAndroidNativeBuffers' extension index,
// enabling use of Android native buffers.
struct UseAndroidNativeBufferParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_PTR pAppPrivate;
    OMX_BUFFERHEADERTYPE **bufferHeader;
//    const sp<ANativeWindowBuffer>& nativeBuffer;
};

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.getAndroidNativeBufferUsage'
// extension is given.  The usage bits returned from this query will be used to
// allocate the Gralloc buffers that get passed to the useAndroidNativeBuffer
// command.
struct GetAndroidNativeBufferUsageParams {
    OMX_U32 nSize;              // IN
    OMX_VERSIONTYPE nVersion;   // IN
    OMX_U32 nPortIndex;         // IN
    OMX_U32 nUsage;             // OUT
};

// An enum OMX_COLOR_FormatAndroidOpaque to indicate an opaque colorformat
// is declared in media/stagefright/openmax/OMX_IVCommon.h
// This will inform the encoder that the actual
// colorformat will be relayed by the GRalloc Buffers.
// OMX_COLOR_FormatAndroidOpaque  = 0x7F000001,

#endif //USE_ANDROID_NATIVE_BUFFER

// A pointer to this struct is passed to OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.prependSPSPPSToIDRFrames' extension
// is given.
// A successful result indicates that future IDR frames will be prefixed by
// SPS/PPS.
struct PrependSPSPPSToIDRFramesParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
};

#endif //ANDROID
