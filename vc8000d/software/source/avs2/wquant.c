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
* File name: wquant.h
* Function:  Frequency Weighting Quantization,include
*               a). Frequency weighting model, quantization
*               b). Picture level user-defined frequency weighting
*               c). Macroblock level adaptive frequency weighting mode Decision
*            According to adopted proposals: m1878,m2148,m2331
* Author:    Jianhua Zheng, Hisilicon
*************************************************************************************
*/

#include "memalloc.h"
#include "wquant.h"
#include "commonStructures.h"
#include "global.h"
#include "avs2_decoder.h"

int WeightQuantEnable;

#if !WQ_MATRIX_FCD
short wq_param_default[2][6] = {{135, 143, 143, 160, 160, 213},
                                {128, 98, 106, 116, 116, 128}};
#else
short wq_param_default[2][6] = {{67, 71, 71, 80, 80, 106},
                                {64, 49, 53, 58, 58, 64}};
#endif

// short wq_matrix[2][2][64];  //wq_matrix[matrix_id][detail/undetail][coef]
short seq_wq_matrix[2][64];
short pic_user_wq_matrix[2][64];

int *GetDefaultWQM(int sizeId) {
  const static int g_WqMDefault4x4[16] = {64, 64, 64, 68, 64, 64, 68, 72,
                                          64, 68, 76, 80, 72, 76, 84, 96};

  const static int g_WqMDefault8x8[64] = {
      64,  64,  64,  64,  68,  68,  72,  76,  64,  64,  64,  68,  72,
      76,  84,  92,  64,  64,  68,  72,  76,  80,  88,  100, 64,  68,
      72,  80,  84,  92,  100, 112, 68,  72,  80,  84,  92,  104, 112,
      128, 76,  80,  84,  92,  104, 116, 132, 152, 96,  100, 104, 116,
      124, 140, 164, 188, 104, 108, 116, 128, 152, 172, 192, 216};

  int *src = 0;

  if (sizeId == 0) {
    src = (int *)g_WqMDefault4x4;
  } else if (sizeId == 1) {
    src = (int *)g_WqMDefault8x8;
  }
  return src;
}

void InitFrameQuantParam(struct Avs2DecContainer *dec) {
  struct Avs2PicParam *pps = &dec->storage.pps;
  u8 *cur_wq_matrix = (u8 *)dec->cmems.wqm_tbl.virtual_address;
  bool weight_quant_enable = pps->pic_weight_quant_enable_flag;
  int i, j, k;
  int uiWQMSizeId, uiBlockSize;
  short wq_param[2][6];

  static const unsigned char WeightQuantModel[4][64] = {
      //   l a b c d h
      //   0 1 2 3 4 5
      {
          // Mode 0
          0, 0, 0, 4, 4, 4, 5, 5, 0, 0, 3, 3, 3, 3, 5, 5, 0, 3, 2, 2, 1, 1,
          5, 5, 4, 3, 2, 2, 1, 5, 5, 5, 4, 3, 1, 1, 5, 5, 5, 5, 4, 3, 1, 5,
          5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
      {
          // Mode 1
          0, 0, 0, 4, 4, 4, 5, 5, 0, 0, 4, 4, 4, 4, 5, 5, 0, 3, 2, 2, 2, 1,
          5, 5, 3, 3, 2, 2, 1, 5, 5, 5, 3, 3, 2, 1, 5, 5, 5, 5, 3, 3, 1, 5,
          5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
      {
          // Mode 2
          0, 0, 0, 4, 4, 3, 5, 5, 0, 0, 4, 4, 3, 2, 5, 5, 0, 4, 4, 3, 2, 1,
          5, 5, 4, 4, 3, 2, 1, 5, 5, 5, 4, 3, 2, 1, 5, 5, 5, 5, 3, 2, 1, 5,
          5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
      {
          // Mode 3
          0, 0, 0, 3, 2, 1, 5, 5, 0, 0, 4, 3, 2, 1, 5, 5, 0, 4, 4, 3, 2, 1,
          5, 5, 3, 3, 3, 3, 2, 5, 5, 5, 2, 2, 2, 2, 5, 5, 5, 5, 1, 1, 1, 5,
          5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}};

  static const unsigned char WeightQuantModel4x4[4][16] = {
      //   l a b c d h
      //   0 1 2 3 4 5
      {
          // Mode 0
          0, 4, 3, 5, 4, 2, 1, 5, 3, 1, 1, 5, 5, 5, 5, 5},
      {
          // Mode 1
          0, 4, 4, 5, 3, 2, 2, 5, 3, 2, 1, 5, 5, 5, 5, 5},
      {
          // Mode 2
          0, 4, 3, 5, 4, 3, 2, 5, 3, 2, 1, 5, 5, 5, 5, 5},
      {
          // Mode 3
          0, 3, 1, 5, 3, 4, 2, 5, 1, 2, 2, 5, 5, 5, 5, 5}};

  // img->cur_wq_matrix = (short * )DWLcalloc(4*64, sizeof(short));

  for (uiWQMSizeId = 0; uiWQMSizeId < 4; uiWQMSizeId++)
    for (i = 0; i < 64; i++) {
      cur_wq_matrix[uiWQMSizeId * 64 + i] = 1 << WQ_FLATBASE_INBIT;
    }

  if (!weight_quant_enable) {
    for (uiWQMSizeId = 0; uiWQMSizeId < 2; uiWQMSizeId++) {
      uiBlockSize = 1 << (uiWQMSizeId + 2);
      for (k = 0; k < 2; k++)
        for (j = 0; j < uiBlockSize; j++)
          for (i = 0; i < uiBlockSize; i++) {
            dec->wq_matrix[uiWQMSizeId][k][j * uiBlockSize + i] =
                1 << WQ_FLATBASE_INBIT;
          }
    }
  } else {
    for (i = 0; i < 2; i++)
      for (j = 0; j < 6; j++) {
        wq_param[i][j] = 1 << WQ_FLATBASE_INBIT;
      }

    if (pps->weighting_quant_param == 0) {
      for (i = 0; i < 6; i++) {
        wq_param[DETAILED][i] = wq_param_default[DETAILED][i];
      }

    } else if (pps->weighting_quant_param == 1) {
      for (i = 0; i < 6; i++) {
        wq_param[UNDETAILED][i] = pps->quant_param_undetail[i];
      }
    }
    if (pps->weighting_quant_param == 2) {
      for (i = 0; i < 6; i++) {
        wq_param[DETAILED][i] = pps->quant_param_detail[i];
      }
    }
    // Reconstruct the Weighting matrix
    for (k = 0; k < 2; k++)
      for (j = 0; j < 8; j++)
        for (i = 0; i < 8; i++) {
          // wq_matrix[1][k][j * 8 + i] =
          // (wq_param[k][WeightQuantModel[hd->CurrentSceneModel][j * 8 + i]]);
          dec->wq_matrix[1][k][j * 8 + i] =
              (wq_param
                   [k]
                   [WeightQuantModel[pps->weighting_quant_model][j * 8 + i]]);
        }

    for (k = 0; k < 2; k++)
      for (j = 0; j < 4; j++)
        for (i = 0; i < 4; i++) {
          // wq_matrix[0][k][j * 4 + i] =
          // (wq_param[k][WeightQuantModel4x4[hd->CurrentSceneModel][j * 4 +
          // i]]);
          dec->wq_matrix[0][k][j * 4 + i] =
              (wq_param[k][WeightQuantModel4x4[pps->weighting_quant_model]
                                              [j * 4 + i]]);
        }
  }
}

void FrameUpdateWQMatrix(struct Avs2DecContainer *dec) {
  struct Avs2SeqParam *sps = &dec->storage.sps;
  struct Avs2PicParam *pps = &dec->storage.pps;
  u8 *wq_matrix_tbl = (u8 *)dec->cmems.wqm_tbl.virtual_address;
  u8 *cur_wq_matrix;
  int i,j;
  int uiWQMSizeId, uiWMQId;
  int uiBlockSize;

  if (pps->pic_weight_quant_enable_flag) {
    for (uiWQMSizeId = 0; uiWQMSizeId < 2; uiWQMSizeId++)
    {
      uiBlockSize = min(1 << (uiWQMSizeId + 2), 8);
      uiWMQId = (uiWQMSizeId < 2) ? uiWQMSizeId : 1;
      cur_wq_matrix = (uiWQMSizeId==0)?(wq_matrix_tbl):(wq_matrix_tbl+16);
      if (pps->pic_weight_quant_data_index == 0) {

        for (i = 0; i < uiBlockSize; i++) {
          for(j = 0; j < uiBlockSize; j++)
            cur_wq_matrix[i+j*uiBlockSize] = sps->seq_wq_matrix[uiWMQId][i*uiBlockSize+j];
        }
      }  // pic_weight_quant_data_index == 0
      else if (pps->pic_weight_quant_data_index == 1) {
        if (pps->weighting_quant_param == 0) {
          for (i = 0; i < uiBlockSize; i++) {
            for(j = 0; j < uiBlockSize; j++)
              // Detailed weighted matrix
              cur_wq_matrix[i+j*uiBlockSize] = dec->wq_matrix[uiWMQId][DETAILED][i*uiBlockSize+j];
          }
        } else if (pps->weighting_quant_param == 1) {

          for (i = 0; i < uiBlockSize; i++) {
            for(j = 0; j < uiBlockSize; j++)
              // unDetailed weighted matrix
              cur_wq_matrix[i+j*uiBlockSize] =   dec->wq_matrix[uiWMQId][0][i*uiBlockSize+j];
          }
        }
        if (pps->weighting_quant_param == 2) {

          for (i = 0; i < uiBlockSize; i++) {
            for(j = 0; j < uiBlockSize; j++)
              // Detailed weighted matrix
              cur_wq_matrix[i+j*uiBlockSize] = dec->wq_matrix[uiWMQId][1][i*uiBlockSize+j];
          }
        }
      }  // pic_weight_quant_data_index == 1
      else if (pps->pic_weight_quant_data_index == 2) {
        for (i = 0; i < uiBlockSize; i++) {
          for(j = 0; j < uiBlockSize; j++)
            cur_wq_matrix[i+j*uiBlockSize] = pps->wq_matrix[uiWMQId][i*uiBlockSize+j];
        }
      }  // pic_weight_quant_data_index == 2
    }
  }
}
