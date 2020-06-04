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

#ifndef _CHALLENGE_H_
#define _CHALLENGE_H_

#include "helix_types.h"
#include "helix_utils.h"
#include "rm_memory.h"



/*
 * The original Challange structure and methods. Used in the
 * Old PNA and as the first round in the new RTSP Challenge.
 */
struct Challenge
{
    BYTE text[33];
    BYTE response[33];
};

struct Challenge* CreateChallenge(INT32 k1,
                                  INT32 k2,
                                  BYTE* k3,
                                  BYTE* k4);

struct Challenge* CreateChallengeFromPool(INT32 k1,
                                          INT32 k2,
                                          BYTE* k3,
                                          BYTE* k4,
                                          rm_malloc_func_ptr fpMalloc,
                                          void* pMemoryPool);
    
BYTE* ChallengeResponse1(BYTE* k1, BYTE* k2,
                         INT32 k3, INT32 k4,
                         struct Challenge* ch);
BYTE* ChallengeResponse2(BYTE* k1, BYTE* k2,
                         INT32 k3, INT32 k4,
                         struct Challenge* ch);
    


/*
 * The new RTSP Challenge structure and methods
 */
struct RealChallenge
{
    BYTE challenge[33];
    BYTE response[41];
    BYTE trap[9];
};


struct RealChallenge* CreateRealChallenge();
struct RealChallenge* CreateRealChallengeFromPool(rm_malloc_func_ptr fpMalloc,
                                                  void* pMemoryPool);
BYTE* RealChallengeResponse1(BYTE* k1, BYTE* k2,
                             INT32 k3, INT32 k4,
                             struct RealChallenge* rch);
BYTE* RealChallengeResponse2(BYTE* k1, BYTE* k2,
                             INT32 k3, INT32 k4,
                             struct RealChallenge* rch);


void CalcCompanyIDKey(const char* companyID,
                      const char* starttime,
                      const char* guid,
                      const char* challenge,
                      const char* copyright,
                      UCHAR*      outputKey);

INT32 BinTo64(const BYTE* pInBuf, INT32 len, char* pOutBuf);


/* Support constants and values */
extern const INT32 G2_BETA_EXPIRATION;

extern const INT32 RC_MAGIC1;
extern const INT32 RC_MAGIC2;
extern const INT32 RC_MAGIC3;
extern const INT32 RC_MAGIC4;

extern const unsigned char pRCMagic1[];
extern const unsigned char pRCMagic2[];


// This UUID was added in the 5.0 player to allow us to identify
// players from Progressive Networks.  Other companies licensing this
// code must be assigned a different UUID.
#define HX_COMPANY_ID "92c4d14a-fa51-4bcb-8a67-7ac286f0ff7e"
#define HX_COMPANY_ID_KEY_SIZE  16


extern const unsigned char HX_MAGIC_TXT_1[];
extern const char pMagic2[];



#endif /*_CHALLENGE_H_*/
