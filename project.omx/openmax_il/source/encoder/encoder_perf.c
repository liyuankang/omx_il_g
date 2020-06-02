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

#include <time.h>
#include "encoder_perf.h"
#include "dbgtrace.h"

void perf_start(OMX_ENCODER* pEnc)
{
    if (!pEnc->perfStarted)
    {
        struct timespec tv = {0, 0};
        //OMX_U64 interval = 0;

        clock_gettime(CLOCK_MONOTONIC, &tv);
        pEnc->prev_frame_start_time = pEnc->curr_frame_start_time;
        pEnc->curr_frame_start_time = ((((OMX_U64)tv.tv_sec * 1000000000)+ ((OMX_U64)tv.tv_nsec)) / 1000);
    }
    pEnc->perfStarted = OMX_TRUE;
    // Rate at which decoder is invoked in unit of 0.1ms.
    /*interval = dec->curr_frame_dec_begin_time - dec->prev_frame_dec_begin_time;
    interval = interval / 100;
    dec->min_input_frame_interval = ((dec->min_input_frame_interval > 0) ?
                     (MIN(interval, dec->min_input_frame_interval)): interval);
    dec->max_input_frame_interval = MAX(interval, dec->max_input_frame_interval);*/
}

void perf_stop(OMX_ENCODER* pEnc)
{
    struct timespec tv = {0, 0};
    OMX_U64 duration = 0;
    OMX_U64 end_time = 0;

    if(!pEnc->frameCounter)
        pEnc->max_time = 0;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    end_time = ((((OMX_U64)tv.tv_sec * 1000000000)+ ((OMX_U64)tv.tv_nsec)) / 1000);

    // Time to decode a frame in unit of 0.1ms
    duration = end_time - pEnc->curr_frame_start_time;
    duration = duration / 100;

    // Sum of time to decode all the frames.
    pEnc->total_time += duration;
    if (duration)
        pEnc->min_time = ((pEnc->min_time > 0) ? (MIN(duration, pEnc->min_time)) : duration);

    pEnc->max_time = MAX(duration, pEnc->max_time);
    pEnc->frameCounter++;

    pEnc->perfStarted = OMX_FALSE;
    DBGT_PDEBUG("Duration %.1f ms", ((double)duration)/10);
}

void perf_show(OMX_ENCODER* pEnc)
{
    DBGT_PDEBUG("---    Performance    ---");
    DBGT_PDEBUG("Number of frames encoded %d", (int)pEnc->frameCounter);

    if (pEnc->frameCounter > 0)
    {
        DBGT_PDEBUG("Total time %lld ms", pEnc->total_time/10);
        DBGT_PDEBUG("Average time %.1f ms", ((double)pEnc->total_time)/pEnc->frameCounter/10);
        DBGT_PDEBUG("Min time %.1f ms, Max time %.1f ms", ((double)pEnc->min_time)/10, ((double)pEnc->max_time)/10);
    }
    DBGT_PDEBUG("-------------------------");
}
