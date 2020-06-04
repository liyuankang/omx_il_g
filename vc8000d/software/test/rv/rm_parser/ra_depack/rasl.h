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

#ifndef RASL_H
#define RASL_H

#include "helix_types.h"

/* Must regenerate table if these are changed! */
#define RA50_NCODEBYTES         18.5	/* bytes per 5.0kbps sl frame */
#define RA65_NCODEBYTES         14.5	/* bytes per 6.5kbps sl frame */
#define RA85_NCODEBYTES         19		/* bytes per 8.5kbps sl frame */
#define RA160_NCODEBYTES        20		/* bytes per 16.0kbps sl frame */

#define RA50_NCODEBITS          148		/* bits per 5.0kbps sl frame */
#define RA65_NCODEBITS          116		/* bits per 6.5kbps sl frame */
#define RA85_NCODEBITS          152		/* bits per 8.5kbps sl frame */
#define RA160_NCODEBITS         160		/* bits per 16.0kbps sl frame */

#define RASL_MAXCODEBYTES       20		/* max bytes per  sl frame */
#define RASL_NFRAMES            16		/* frames per block - must match codec */
#define RASL_NBLOCKS            6		/* blocks to interleave across */
#define RASL_BLOCK_NUM(i) ((i) >> 4)	/* assumes RASL_NFRAMES = 16 */
#define RASL_BLOCK_OFF(i) ((i) & 0xf)	/* assumes RASL_NFRAMES = 16 */

void RASL_DeInterleave(char *buf, unsigned long ulBufSize, int type, ULONG32 * pFlags);

void ra_bitcopy(unsigned char *toPtr,
	unsigned long ulToBufSize,
	unsigned char *fromPtr,
	unsigned long ulFromBufSize, int bitOffsetTo, int bitOffsetFrom, int numBits);

#endif									/* #ifndef RASL_H */
