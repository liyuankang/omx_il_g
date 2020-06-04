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

#ifndef HELIX_CONFIG_H
#define HELIX_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */


/********************************************************************
 *
 * This file contains system wide configuration options.  Please look
 * through all the config options below to make sure the SDK is tuned
 * best for your system.
 *
 ********************************************************************
 */

    
/*
 * Endian'ness.
 *
 * This package supports both compile-time and run-time determination
 * of CPU byte ordering.  If ARCH_IS_BIG_ENDIAN is defined as 0, the
 * code will be compiled to run only on little-endian CPUs; if
 * ARCH_IS_BIG_ENDIAN is defined as non-zero, the code will be
 * compiled to run only on big-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * not defined, the code will be compiled to run on either big- or
 * little-endian CPUs, but will run slightly less efficiently on
 * either one than if ARCH_IS_BIG_ENDIAN is defined.
 *
 * If you run on a BIG engian box uncomment this:
 *
 *    #define ARCH_IS_BIG_ENDIAN 1
 * 
 * If you are on a LITTLE engian box uncomment this:
 *
 *    #define ARCH_IS_BIG_ENDIAN 0
 * 
 * Or you can just leave it undefined and have it determined
 * at runtime.
 *
 */







    
#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef HELIX_CONFIG_H */
