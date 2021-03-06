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

#ifndef __MPEG2DEBUG_H__
#define __MPEG2DEBUG_H__

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#ifdef _MPEG2APITRACE
#include <stdio.h>
#endif

#ifdef _MPEG2_DEBUG_TRACE
#include <stdio.h>
#endif

#ifdef  _DEBUG_PRINT
#include <stdio.h>
#endif

#ifdef _MPEG2APITRACE
#define MPEG2FLUSH fflush(stdout)
#endif
#ifdef _DEBUG_PRINT
#define MPEG2FLUSH fflush(stdout)
#endif
#ifndef MPEG2FLUSH
#define MPEG2FLUSH
#endif
/* macro for debug printing. Note that double parenthesis has to be used, i.e.
 * DEBUG(("Debug printing %d\n",%d)) */
#ifdef _MPEG2APITRACE
#define MPEG2DEC_API_DEBUG(args) printf args
#else
#define MPEG2DEC_API_DEBUG(args)
#endif

#ifdef _DEBUG_PRINT
#define MPEG2DEC_DEBUG(args) printf args
#else
#define MPEG2DEC_DEBUG(args)
#endif

#ifdef _DEC_PP_USAGE
#define DECPP_STAND_ALONE 0
#define DECPP_PARALLEL 1
#define DECPP_PIPELINED 2
#define DECPP_UNSPECIFIED 3

void Mpeg2DecPpUsagePrint(Mpeg2DecContainer * dec_cont,
                          u32 ppmode, u32 pic_index, u32 dec_status, u32 pic_id);
#endif

#endif /* #ifndef __MPEG2DEBUG_H__ */
