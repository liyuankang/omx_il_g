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

#include "mp4dechwd_strmdec.h"
#include "mp4dechwd_utils.h"
#include "mp4dechwd_headers.h"
#include "mp4dechwd_custom.h"
#include "mp4dechwd_generic.h"
#include "mp4debug.h"
#include "mp4decdrv.h"

/*------------------------------------------------------------------------------
    2. External identifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Module indentifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

   5.1  Function name: StrmDec_DecodeCustomHeadersGeneric

        Purpose: initialize stream decoding related parts of DecContainer

        Input:
            Pointer to DecContainer structure
                -initializes StrmStorage

        Output:
            HANTRO_OK/NOK

------------------------------------------------------------------------------*/
u32 StrmDec_DecodeCustomHeadersGeneric(DecContainer * dec_cont) {
  UNUSED(dec_cont);
  return DEC_VOP_HDR_RDY_ERROR;
}

/*------------------------------------------------------------------------------

   Function name:
                ProcessUserDataGeneric

        Purpose:

        Input:
                pointer to DecContainer

        Output:
                status (HANTRO_OK/NOK/END_OF_STREAM)

------------------------------------------------------------------------------*/

void ProcessUserDataGeneric(DecContainer * dec_cont) {

  u32 tmp;
  u32 SusI = 1147762264;
  tmp = StrmDec_ShowBits(dec_cont, 32);
  if(tmp == SusI) {
    dec_cont->StrmStorage.unsupported_features_present = HANTRO_TRUE;
  }

}


/*------------------------------------------------------------------------------

    Function: SetConformanceFlagsGeneric

        Functional description:
            Set some flags to get best conformance to different Susi versions.

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

void SetConformanceFlagsGeneric( DecContainer * dec_cont ) {

  /* Variables */

  /* Code */

  if( dec_cont->StrmStorage.custom_strm_ver )
    dec_cont->StrmStorage.unsupported_features_present =
      HANTRO_TRUE;

}



/*------------------------------------------------------------------------------

    Function: ProcessHwOutputGeneric

        Functional description:
            Read flip-flop rounding type for Susi3

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

void ProcessHwOutputGeneric( DecContainer * dec_cont ) {
  /* Nothing */
  UNUSED(dec_cont);
}

/*------------------------------------------------------------------------------

    Function: SetCustomInfoGeneric

        Functional description:

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

void SetCustomInfoGeneric(DecContainer * dec_cont, u32 width, u32 height ) {

  u32 mb_width, mb_height;

  mb_width = (width+15)>>4;
  mb_height = (height+15)>>4;

  dec_cont->VopDesc.total_mb_in_vop = mb_width * mb_height;
  dec_cont->VopDesc.vop_width = mb_width;
  dec_cont->VopDesc.vop_height = mb_height;

  dec_cont->StrmStorage.custom_strm_ver = CUSTOM_STRM_0;

  SetConformanceFlags( dec_cont );

}


/*------------------------------------------------------------------------------

    Function: SetConformanceRegsGeneric

        Functional description:

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/

void SetConformanceRegsGeneric( DecContainer * dec_cont ) {

  /* Set correct transform */
  SetDecRegister(dec_cont->mp4_regs, HWIF_DIVX_IDCT_E, 0 );

}


/*------------------------------------------------------------------------------

    Function: SetStrmFmtGeneric

        Functional description:

        Inputs:
            none

        Outputs:
            none

        Returns:
            API version

------------------------------------------------------------------------------*/
void SetStrmFmtGeneric( DecContainer * dec_cont, u32 strm_fmt ) {
  switch(strm_fmt) {
  case MP4DEC_MPEG4:
    /* nothing special here */
    break;
  case MP4DEC_SORENSON:
    dec_cont->StrmStorage.sorenson_spark = 1;
    break;
  default:
    dec_cont->StrmStorage.unsupported_features_present = HANTRO_TRUE;
    break;
  }
}

u32 StrmDec_DecodeCustomHeaders(DecContainer * dec_cont) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    return StrmDec_DecodeCustomHeadersCustom(dec_cont);
  else
    return StrmDec_DecodeCustomHeadersGeneric(dec_cont);
}

void ProcessUserData(DecContainer * dec_cont) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    ProcessUserDataCustom(dec_cont);
  else
    ProcessUserDataGeneric(dec_cont);
}

void SetConformanceFlags(DecContainer * dec_cont) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    SetConformanceFlagsCustom(dec_cont);
  else
    SetConformanceFlagsGeneric(dec_cont);
}

void ProcessHwOutput(DecContainer * dec_cont) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    ProcessHwOutputCustom(dec_cont);
  else
    ProcessHwOutputGeneric(dec_cont);
}

void SetCustomInfo(DecContainer * dec_cont, u32 width, u32 height) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    SetCustomInfoCustom(dec_cont, width, height);
  else
    SetCustomInfoGeneric(dec_cont, width, height);
}

void SetConformanceRegs(DecContainer * dec_cont) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    SetConformanceRegsCustom(dec_cont);
  else
    SetConformanceRegsGeneric(dec_cont);
}

void SetStrmFmt(DecContainer * dec_cont, u32 strm_fmt) {

  DWLHwConfig hw_cfg;
  u32 hw_build_id;
  struct DecHwFeatures hw_feature;

  /* check that custom mpeg4 supported in HW */
  (void) DWLmemset(&hw_cfg, 0, sizeof(DWLHwConfig));
  DWLReadAsicConfig(&hw_cfg, DWL_CLIENT_TYPE_MPEG4_DEC);
  hw_build_id = DWLReadHwBuildID(DWL_CLIENT_TYPE_MPEG4_DEC);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  if(hw_feature.custom_mpeg4_support)
    SetStrmFmtCustom(dec_cont, strm_fmt);
  else
    SetStrmFmtGeneric(dec_cont, strm_fmt);
}

