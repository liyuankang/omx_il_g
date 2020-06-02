/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#include "basetype.h"
#include "sw_activity_trace.h"
#include "dwl.h"

u32 SwActivityTraceInit(struct ActivityTrace* inst) {
  if (inst == NULL) return 1;

  DWLmemset(inst, 0, sizeof(struct ActivityTrace));
  (void)inst;
  return 0;
}

u32 SwActivityTraceStartDec(struct ActivityTrace* inst) {
  if (inst == NULL) return 1;
  gettimeofday(&inst->start, NULL);

  if (inst->stop.tv_usec + inst->stop.tv_sec) {
    unsigned long idle = 1000000 * inst->start.tv_sec + inst->start.tv_usec -
                         1000000 * inst->stop.tv_sec - inst->stop.tv_usec;
    inst->idle_time += idle / 10;
  }
  inst->start_count++;
  (void)inst;
  return 0;
}

u32 SwActivityTraceStopDec(struct ActivityTrace* inst) {

  unsigned long active;
  if (inst == NULL) return 1;

  gettimeofday(&inst->stop, NULL);

  active = 1000000 * inst->stop.tv_sec + inst->stop.tv_usec -
           1000000 * inst->start.tv_sec - inst->start.tv_usec;
  inst->active_time += active / 10;
  (void)inst;
  return 0;
}

u32 SwActivityTraceRelease(struct ActivityTrace* inst) {
  if (inst == NULL) return 1;

  if (inst->active_time || inst->idle_time) {
    printf("\n active/idle statistics:\n");
    printf("Active: %9llu msec\n", inst->active_time / 100);
    printf("Idle: %11llu msec\n", inst->idle_time / 100);
    if (inst->active_time + inst->idle_time)
      printf("Decoder utilization: %llu %%\n",
             inst->active_time / ((inst->active_time + inst->idle_time) / 100));
  }
  (void)inst;
  return 0;
}
