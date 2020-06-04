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

#ifndef RM_PROPERTY_H
#define RM_PROPERTY_H

#include "helix_types.h"

#define RM_PROPERTY_TYPE_UINT32  0
#define RM_PROPERTY_TYPE_BUFFER  1
#define RM_PROPERTY_TYPE_CSTRING 2

#ifdef __cplusplus
extern "C" {
#endif  /* #ifdef __cplusplus */

/*
 * Property struct
 *
 * This struct can hold a UINT32 property, a CString
 * property, or a buffer property. The members
 * of this struct are as follows:
 *
 * pName:        NULL-terminated string which holds the name
 * ulType:       This can be either RM_PROPERTY_TYPE_UINT32,
 *                 RM_PROPERTY_TYPE_BUFFER, or RM_PROPERTY_TYPE_CSTRING.
 * pValue and ulValueLen:
 *     1) For type RM_PROPERTY_TYPE_UINT32, the value is held in
 *        the pValue pointer itself and ulValueLen is 0.
 *     2) For type RM_PROPERTY_TYPE_BUFFER, the value is held in
 *        the buffer pointed to by pValue and the length of
 *        the buffer is ulValueLen.
 *     3) For type RM_PROPERTY_TYPE_CSTRING, the value is a
 *        NULL terminated string in pValue. Therefore the pValue
 *        pointer can be re-cast as a (const char*) and the value
 *        string read from it. ulValueLen holds a length that
 *        is AT LEAST strlen((const char*) pValue) + 1. It may
 *        be more than that, so do not rely on ulValueLen being
 *        equal to strlen((const char*) pValue) + 1.
 */

typedef struct rm_property_struct
{
    char*  pName;
    UINT32 ulType;
    BYTE*  pValue;
    UINT32 ulValueLen;
} rm_property;

/*
 * rm_property Accessor Functions
 *
 * Users are strongly encouraged to use these accessor
 * functions to retrieve information from the 
 * rm_property struct, since the definition of rm_property
 * may change in the future.
 *
 * When retrieving the property name, the user should first
 * call rm_property_get_name_length() to see that the name
 * length is greater than 0. After that, the user can get
 * read-only access to the name by calling rm_property_get_name().
 */
const char* rm_property_get_name(rm_property* prop);
UINT32      rm_property_get_type(rm_property* prop);
UINT32      rm_property_get_value_uint32(rm_property* prop);
const char* rm_property_get_value_cstring(rm_property* prop);
UINT32      rm_property_get_value_buffer_length(rm_property* prop);
BYTE*       rm_property_get_value_buffer(rm_property* prop);

#ifdef __cplusplus
}
#endif  /* #ifdef __cplusplus */

#endif /* #ifndef RM_PROPERTY_H */
