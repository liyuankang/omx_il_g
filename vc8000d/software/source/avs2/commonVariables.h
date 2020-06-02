/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2002-2016, Audio Video coding Standard Workgroup of China
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of Audio Video coding Standard Workgroup of China
*    nor the names of its contributors maybe used to endorse or promote products
*    derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
*************************************************************************************
* File name: commonVariables.h
* Function:  common variable definitions for for AVS encoder and decoder.
*
*************************************************************************************
*/
#include <stdio.h>  //!< for FILE
#include "defines.h"
#include "commonStructures.h"
#ifndef _GLOBAL_COM_H_
#define _GLOBAL_COM_H_

extern CameraParamters *camera;
extern SNRParameters *snr;
extern ImageParameters *img;

//avs2_frame_t *fref[REF_MAXBUFFER];

#define ET_SIZE 300  //!< size of error text buffer

extern int g_Left_Down_Avail_Matrix64[16][16];
extern int g_Left_Down_Avail_Matrix32[8][8];
extern int g_Left_Down_Avail_Matrix16[4][4];
extern int g_Left_Down_Avail_Matrix8[2][2];
extern int g_Up_Right_Avail_Matrix64[16][16];
extern int g_Up_Right_Avail_Matrix32[8][8];
extern int g_Up_Right_Avail_Matrix16[4][4];
extern int g_Up_Right_Avail_Matrix8[2][2];

/* ------------------------------------------------------
* common data
*/
typedef struct {
  int Bframe_ctr;

  FILE *p_log;    //!< SNR file
  FILE *p_trace;  //!< Trace file

  int tot_time;

  // M3178 PKU Reference Manage
  int coding_order;
  avs2_frame_t *f_rec;  //!< current encoding/decoding frame pointer

  int seq_header;

  // byte   **imgY;               //!< Encoded/decoded luma images
  // byte  ***imgUV;              //!< Encoded/decoded croma images
  // byte   **imgY_sao;
  // byte   **imgUV_sao[2];
  // byte    *imgY_alf_Rec;
  // byte   **imgUV_alf_Rec;
  struct DWLLinearMem *imgY;      //!< Encoded/decoded luma images
  struct DWLLinearMem *imgUV[2];  //!< Encoded/decoded croma images
  struct DWLLinearMem imgY_sao;
  struct DWLLinearMem imgUV_sao[2];
  struct DWLLinearMem imgY_alf_Rec;
  struct DWLLinearMem imgUV_alf_Rec[2];
  u16 *imgY_alf_Org;
  u16 **imgUV_alf_Org;
  // byte   **imgYPrev;
  // byte  ***imgUVPrev;
  struct DWLLinearMem imgYPrev;
  struct DWLLinearMem imgUVPrev[2];

  struct DWLLinearMem *currentFrame;  //[yuv][height][width]

  struct DWLLinearMem backgroundReferenceFrame[3];
  struct DWLLinearMem *background_ref;

  int total_frames;

  // mv_range, 20071009
  int Min_V_MV;
  int Max_V_MV;
  int Min_H_MV;
  int Max_H_MV;

  char errortext[ET_SIZE];  //!< buffer for error message for exit with error()
  char str_list_reference[128];

  /// encoder only
  int **tmp_block_88_inv;
} Video_Com_data;
extern Video_Com_data *hc;

#endif
