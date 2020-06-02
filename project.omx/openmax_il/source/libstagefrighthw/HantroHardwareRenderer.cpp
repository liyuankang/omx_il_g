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

#define LOG_TAG "HantroHardwareRenderer"
#include <utils/Log.h>

#include "HantroHardwareRenderer.h"

#include <binder/MemoryHeapBase.h>
#include <binder/MemoryHeapPmem.h>
#include <media/stagefright/MediaDebug.h>
#include <surfaceflinger/ISurface.h>

namespace android {

HantroHardwareRenderer::HantroHardwareRenderer(
        const sp<ISurface> &surface,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        OMX_COLOR_FORMATTYPE colorFormat,
        int32_t rotationDegrees)
    : mISurface(surface),
      mDisplayWidth(displayWidth),
      mDisplayHeight(displayHeight),
      mDecodedWidth(decodedWidth),
      mDecodedHeight(decodedHeight),
      mColorFormat(colorFormat),
      mConverter(colorFormat, OMX_COLOR_Format16bitRGB565),
      mFrameSize(displayWidth * displayHeight * 2),
      mIndex(0) {

    LOGD("mFrameSize %d", mFrameSize);

    mMemoryHeap = new MemoryHeapBase("/dev/pmem_adsp", 2 * mFrameSize);
    if (mMemoryHeap->heapID() < 0) {
        LOGI("Creating physical memory heap failed, reverting to regular heap.");
        mMemoryHeap = new MemoryHeapBase(2 * mFrameSize);
    } else {
        sp<MemoryHeapPmem> pmemHeap = new MemoryHeapPmem(mMemoryHeap);
        pmemHeap->slap();
        mMemoryHeap = pmemHeap;
    }

    LOGD("Heap Size %d", mMemoryHeap->getSize() );
    /*uint32_t pmem_fd;

    sp<MemoryHeapBase> master =
        reinterpret_cast<MemoryHeapBase *>(pmem_fd);

    master->setDevice("/dev/pmem");

    uint32_t heap_flags = master->getFlags() & MemoryHeapBase::NO_CACHING;
    mMemoryHeap = new MemoryHeapPmem(master, heap_flags);
    mMemoryHeap->slap();*/

    CHECK(mISurface.get() != NULL);
    CHECK(mDecodedWidth > 0);
    CHECK(mDecodedHeight > 0);
    CHECK(mMemoryHeap->heapID() >= 0);
    CHECK(mConverter.isValid());

    uint32_t orientation;
    switch (rotationDegrees) {
        case 0: orientation = ISurface::BufferHeap::ROT_0; break;
        case 90: orientation = ISurface::BufferHeap::ROT_90; break;
        case 180: orientation = ISurface::BufferHeap::ROT_180; break;
        case 270: orientation = ISurface::BufferHeap::ROT_270; break;
        default: orientation = ISurface::BufferHeap::ROT_0; break;
    }
LOGD("mDisplayWidth %d mDisplayHeight %d mDecodedWidth %d mDecodedHeight %d", mDisplayWidth, mDisplayHeight, mDecodedWidth, mDecodedHeight);
    ISurface::BufferHeap bufferHeap(
            mDisplayWidth, mDisplayHeight,
            mDisplayWidth, mDisplayHeight,
            PIXEL_FORMAT_RGB_565,
            //HAL_PIXEL_FORMAT_YCrCb_420_SP,
            orientation, 0,
            mMemoryHeap);

    status_t err = mISurface->registerBuffers(bufferHeap);
    CHECK_EQ(err, OK);
}

HantroHardwareRenderer::~HantroHardwareRenderer() {
    mISurface->unregisterBuffers();
}

void HantroHardwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {
    size_t offset = mIndex * mFrameSize;
    uint8_t *dst = (uint8_t *)mMemoryHeap->getBase() + offset;

    mConverter.convert(
            mDecodedWidth, mDecodedHeight,
            data, 0, dst, 2 * mDecodedWidth);
   
/*    uint8_t *Y, *U, *V;
    uint32_t picSize = mDisplayWidth * mDisplayHeight;
    uint32_t chromaSize = picSize >> 2;
LOGD("render...3");
    Y = (uint8_t*)data;
    U = Y + picSize;
    V = U + chromaSize;

    memcpy(dst, Y, picSize);
    memcpy(dst + picSize, U, chromaSize);
    memcpy(dst + picSize + chromaSize, V, chromaSize);
*/
    mISurface->postBuffer(offset);
    mIndex = 1 - mIndex;
}

}  // namespace android

