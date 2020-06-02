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

#include <assert.h>
#include <stdio.h>
#include "dwlthread.h"

#ifndef _HAVE_PTHREAD_H
u32 CPUFreq = 50000000;

//int usleep(int n) {
#if 0
  int i, j, tick_in_us;
  unsigned long long freq = CPUFreq;

  tick_in_us = freq * 17ul >> 24; /* 17 / 2 ^ 24 = 1000000L */
  for (i = 0; i < n; i++) {
    for (j = 0; j < tick_in_us; j++);
  }
#endif
//  return 0;
//}

int MutexInit(PMutex m, PAttribute a) {
  if(m == NULL) return -1;

  m->locked = FALSE;
  if (a) {
    m->attrib = *a;
  } else {
    m->attrib.a = 0;
  }
  return 0;
}

int MutexDestroy(PMutex m) {
  if(m == NULL) return -1;
  if(m->locked) return -1;

  m->locked = FALSE;
  m->attrib.a = 0;
  return 0;
}

int MutexLock(PMutex m) {
  if(m == NULL) return -1;

  if (m->locked == FALSE) {
    m->locked = TRUE;
    return 0;
  }

//  ASSERT(0);
  return -1;
}

int MutexUnlock(PMutex m) {
  if(m == NULL) return -1;

  if (m->locked == TRUE) {
    m->locked = FALSE;
    return 0;
  }

  return 0;
}

int EventInit(PEvent e, PAttribute a) {
  if(e == NULL) return -1;

  e->singaled = FALSE;
  if (a) {
    e->attrib = *a;
  } else {
    e->attrib.a = 0;
  }
  return 0;
}

int EventDestroy(PEvent e) {
  if(e == NULL) return -1;
//  ASSERT(e->singaled == FALSE);

  e->singaled = FALSE;
  e->attrib.a = 0;
  return 0;
}

int EventWait(PEvent e, PMutex m) {
  if(e == NULL) return -1;
  if(m == NULL) return -1;

  if (m) {
    MutexUnlock(m);
  }

  if (e->singaled == TRUE) {
    e->singaled = FALSE;
    return 0;
  }

//  ASSERT(0);
  return -1;
}

int EventSignal(PEvent e) {
  if(e == NULL) return -1;

  if (e->singaled == FALSE) {
    e->singaled = TRUE;
    return 0;
  }

  return 0;
}

int SemaphoreInit(PSemaphore s, u32 value) {
  if(s == NULL) return -1;

  s->val = (i32)value;
  return 0;
}

int SemaphoreDestroy(PSemaphore s) {
  if(s == NULL) return -1;

  s->val = 0;
  return 0;
}

int SemaphoreWait(PSemaphore s) {
  if(s == NULL) return -1;

  if (s->val > 0) {
    s->val--;
    return 0;
  }

  return -1;
}

int SemaphorePost(PSemaphore s) {
  if(s == NULL) return -1;

  s->val++;
  return 0;
}

int SemaphoreGetValue(PSemaphore s) {
  if(s == NULL) return -1;

  return s->val;
}
#endif //_HAVE_PTHREAD_H

