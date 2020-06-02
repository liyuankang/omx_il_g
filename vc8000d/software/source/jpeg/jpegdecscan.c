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

#include "jpegdecutils.h"
#include "jpegdecinternal.h"
#include "jpegdecmarkers.h"
#include "jpegdecscan.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

_DTRACE:   define this flag to print trace information to stdout

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecodeScanHeader(JpegDecContainer *);

/*------------------------------------------------------------------------------
    5. Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: JpegDecDecodeScan

        Functional description:
          Handles the top level control of scan decoding

        Inputs:
          JpegDecContainer *p_dec_data      Pointer to JpegDecContainer structure

        Outputs:
          OK  (0)
          NOK (-1)

------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecodeScan(JpegDecContainer * p_dec_data) {
  JpegDecRet ret_code; /* Returned code container */

  ret_code = JPEGDEC_ERROR;

  ret_code = JpegDecDecodeScanHeader(p_dec_data);
  if(ret_code != JPEGDEC_OK) {
    return (ret_code);
  }

  JPEGDEC_TRACE_INTERNAL(("SCAN: Allocate residual buffer\n"));

  /* Allocate buffers if not done already */
  if(!p_dec_data->info.allocated) {
    ret_code = JpegDecAllocateResidual(p_dec_data);
    if(ret_code != JPEGDEC_OK) {
      JPEGDEC_TRACE_INTERNAL(("SCAN: ALLOCATE ERROR\n"));

      return (ret_code);
    }
    /* update */
    p_dec_data->info.allocated = 1;
  }
  JPEGDEC_TRACE_INTERNAL(("SCAN: Scan rlc data\n"));
  JPEGDEC_TRACE_INTERNAL(("SCAN: MODE: %d\n", p_dec_data->frame.coding_type));

  return (JPEGDEC_OK);

}

/*------------------------------------------------------------------------------

    Function name: JpegDecDecodeScanHeader

        Functional description:
          Decodes scan headers

        Inputs:
          JpegDecContainer *p_dec_data       Pointer to JpegDecContainer structure

        Outputs:
          OK  (0)
          NOK (-1)

------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecodeScanHeader(JpegDecContainer * p_dec_data) {
  u32 i, j;
  u32 tmp;

  StreamStorage *stream = &(p_dec_data->stream);

  p_dec_data->scan.Ls = JpegDecGet2Bytes(stream);

  /* check if there is enough data */
  if(((stream->read_bits / 8) + p_dec_data->scan.Ls) > stream->stream_length)
    return (JPEGDEC_STRM_ERROR);

  p_dec_data->scan.Ns = JpegDecGetByte(stream);
  if (p_dec_data->scan.Ns > MAX_NUMBER_OF_COMPONENTS)
    return (JPEGDEC_STRM_ERROR);

  p_dec_data->info.fill_x = p_dec_data->info.fill_y = 0;
  if(p_dec_data->scan.Ns == 1) {
    /* Reset to non-interleaved baseline operation type */
    if(p_dec_data->info.operation_type == JPEGDEC_BASELINE &&
        p_dec_data->info.y_cb_cr_mode != JPEGDEC_YUV400)
      p_dec_data->info.operation_type = JPEGDEC_NONINTERLEAVED;

    tmp = JpegDecGetByte(&(p_dec_data->stream));
    p_dec_data->frame.c_index = tmp - 1;
    if (p_dec_data->frame.c_index > MAX_NUMBER_OF_COMPONENTS)
      return (JPEGDEC_STRM_ERROR);
    p_dec_data->info.component_id = p_dec_data->frame.c_index;
    p_dec_data->scan.Cs[p_dec_data->frame.c_index] = tmp;
    tmp = JpegDecGetByte(stream);
    p_dec_data->scan.Td[p_dec_data->frame.c_index] = tmp >> 4;
    p_dec_data->scan.Ta[p_dec_data->frame.c_index] = tmp & 0x0F;

    /* check/update component info */
    if(p_dec_data->frame.Nf == 3) {
      //p_dec_data->info.fill_right = 0;
      //p_dec_data->info.fill_bottom = 0;
      if(p_dec_data->scan.Cs[p_dec_data->frame.c_index] == 2 ||
          p_dec_data->scan.Cs[p_dec_data->frame.c_index] == 3) {
        if(p_dec_data->info.operation_type == JPEGDEC_PROGRESSIVE ||
            p_dec_data->info.operation_type == JPEGDEC_NONINTERLEAVED ||
            p_dec_data->info.non_interleaved_scan_ready) {
          p_dec_data->info.X = p_dec_data->frame.hw_x;
          p_dec_data->info.Y = p_dec_data->frame.hw_y;
        }

        p_dec_data->info.y_cb_cr_mode = 0;
      } else if(p_dec_data->scan.Cs[p_dec_data->frame.c_index] == 1) { /* YCbCr 4:2:0 */
        p_dec_data->info.X = p_dec_data->frame.hw_x;
        p_dec_data->info.Y = p_dec_data->frame.hw_y;
        if(p_dec_data->info.y_cb_cr_mode == JPEGDEC_YUV420) {
          p_dec_data->info.y_cb_cr_mode = 1;
        } else if(p_dec_data->info.y_cb_cr_mode == JPEGDEC_YUV422) {
          p_dec_data->info.y_cb_cr_mode = 0;
        } else if(p_dec_data->info.y_cb_cr_mode == JPEGDEC_YUV444) {
          p_dec_data->info.y_cb_cr_mode = 0;
        }
      } else {
        p_dec_data->info.y_cb_cr_mode = 0;
        return (JPEGDEC_UNSUPPORTED);
      }
#if 0
      if((p_dec_data->scan.Cs[p_dec_data->frame.c_index] == 1 ||
                 p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV440 ||
                 p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV444) &&
                (p_dec_data->frame.X & 0xF) && (p_dec_data->frame.X & 0xF) <= 8) {
        p_dec_data->info.fill_right = 1;
      }

      if((p_dec_data->scan.Cs[p_dec_data->frame.c_index] == 1 ||
                 p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV422 ||
                 p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV411 ||
                 p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV444) &&
                (p_dec_data->frame.Y & 0xF) && (p_dec_data->frame.Y & 0xF) <= 8) {
        p_dec_data->info.fill_bottom = 1;
      }
#endif
    }

    /* decoding info */
    if(p_dec_data->info.operation_type == JPEGDEC_PROGRESSIVE ||
        p_dec_data->info.operation_type == JPEGDEC_NONINTERLEAVED) {
      p_dec_data->info.y_cb_cr_mode = 0;
    }
  } else {
    for(i = 0; i < p_dec_data->scan.Ns; i++) {
      p_dec_data->scan.Cs[i] = JpegDecGetByte(&(p_dec_data->stream));
      tmp = JpegDecGetByte(stream);
      p_dec_data->scan.Td[i] = tmp >> 4;    /* which DC table */
      p_dec_data->scan.Ta[i] = tmp & 0x0F;  /* which AC table */
    }
    p_dec_data->info.X = p_dec_data->frame.hw_x;
    p_dec_data->info.Y = p_dec_data->frame.hw_y;
    p_dec_data->frame.c_index = 0;
    p_dec_data->info.y_cb_cr_mode = p_dec_data->info.y_cb_cr_mode_orig;
  }

  p_dec_data->scan.Ss = JpegDecGetByte(stream);
  p_dec_data->scan.Se = JpegDecGetByte(stream);
  tmp = JpegDecGetByte(stream);
  p_dec_data->scan.Ah = tmp >> 4;
  p_dec_data->scan.Al = tmp & 0x0F;

  if(p_dec_data->frame.coding_type == SOF0) {
    /* baseline */
    if(p_dec_data->scan.Ss != 0)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Se != 63)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Ah != 0)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Al != 0)
      return (JPEGDEC_UNSUPPORTED);

    /* update scan decoding parameters */
    /* interleaved/non-interleaved */
    if(p_dec_data->info.operation_type == JPEGDEC_BASELINE)
      p_dec_data->info.non_interleaved = 0;
    else
      p_dec_data->info.non_interleaved = 1;
    /* decoding info */
    if((p_dec_data->frame.Nf == 3 && p_dec_data->scan.Ns == 1) ||
        (p_dec_data->frame.Nf == 1 && p_dec_data->scan.Ns == 1))
      p_dec_data->info.amount_of_qtables = 1;
    else
      p_dec_data->info.amount_of_qtables = 3;
  }

  if(p_dec_data->frame.coding_type == SOF2) {
    /* progressive */
    if(p_dec_data->scan.Ss == 0 && p_dec_data->scan.Se != 0)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Ss > 63 || p_dec_data->scan.Se > 63)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Ah > 13)
      return (JPEGDEC_UNSUPPORTED);
    if(p_dec_data->scan.Al > 13)
      return (JPEGDEC_UNSUPPORTED);

    /* update scan decoding parameters */
    /* TODO! What if 2 components, possible??? */
    /* interleaved/non-interleaved */
    if(p_dec_data->scan.Ns == 1) {
      p_dec_data->info.non_interleaved = 1;
      /* component ID */
      p_dec_data->info.component_id = p_dec_data->frame.c_index;
      if (p_dec_data->frame.Nf == 3)
        p_dec_data->info.amount_of_qtables = 3;
      else
        p_dec_data->info.amount_of_qtables = 1;
    } else {
      p_dec_data->info.non_interleaved = 0;
      /* component ID ==> set to luma ==> interleaved */
      p_dec_data->info.component_id = 0;
      p_dec_data->info.amount_of_qtables = 3;
    }

    p_dec_data->info.fill_right = 0;
    p_dec_data->info.fill_bottom = 0;
    if((p_dec_data->frame.X & 0xF) && (p_dec_data->frame.X & 0xF) <= 8) {
        p_dec_data->info.fill_right = 1;
      }

    if((p_dec_data->frame.Y & 0xF) && (p_dec_data->frame.Y & 0xF) <= 8) {
      p_dec_data->info.fill_bottom = 1;
    }
  }

  if (p_dec_data->last_scan) return(JPEGDEC_OK);

  u32 last_scan = 1;
  if (p_dec_data->frame.coding_type == SOF2) {
    for (i = p_dec_data->scan.Ss; i <= p_dec_data->scan.Se; i++) {
      if (p_dec_data->scan.Ah == 0) {
        if (p_dec_data->info.non_interleaved)
          p_dec_data->pjpeg_coeff_bit_map[p_dec_data->info.component_id][i] = 
              p_dec_data->pjpeg_coeff_bit_map[p_dec_data->info.component_id][i] |
              (0xFFFF << p_dec_data->scan.Al);
        else {
          p_dec_data->pjpeg_coeff_bit_map[0][i] = 
              p_dec_data->pjpeg_coeff_bit_map[0][i] | (0xFFFF << p_dec_data->scan.Al);
          p_dec_data->pjpeg_coeff_bit_map[1][i] =
              p_dec_data->pjpeg_coeff_bit_map[1][i] | (0xFFFF << p_dec_data->scan.Al);
          p_dec_data->pjpeg_coeff_bit_map[2][i] =
              p_dec_data->pjpeg_coeff_bit_map[2][i] | (0xFFFF << p_dec_data->scan.Al);
        }
      } else {
        if (p_dec_data->info.non_interleaved)
          p_dec_data->pjpeg_coeff_bit_map[p_dec_data->info.component_id][i] =
              p_dec_data->pjpeg_coeff_bit_map[p_dec_data->info.component_id][i] |
              (1 << p_dec_data->scan.Al);
        else {
          p_dec_data->pjpeg_coeff_bit_map[0][i] =
              p_dec_data->pjpeg_coeff_bit_map[0][i] | (1 << p_dec_data->scan.Al);
          p_dec_data->pjpeg_coeff_bit_map[1][i] =
              p_dec_data->pjpeg_coeff_bit_map[1][i] | (1 << p_dec_data->scan.Al);
          p_dec_data->pjpeg_coeff_bit_map[2][i] =
              p_dec_data->pjpeg_coeff_bit_map[2][i] | (1 << p_dec_data->scan.Al);
        }
      }
    }

    for (i = 0; i < 64; i++) {
      for (j = 0; j < (p_dec_data->info.y_cb_cr_mode_orig == JPEGDEC_YUV400 ? 1 : 3); j++) {
        if (p_dec_data->pjpeg_coeff_bit_map[j][i] != 0xFFFF) {
          last_scan = 0;
          break;
        }
      }

      if (last_scan == 0)
        break;
    }
    p_dec_data->last_scan = last_scan;

    if (last_scan && p_dec_data->vcmd_used) {
      /* Restore to enable pp when the last scan is detected. */
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
        p_dec_data->ppu_cfg[i].enabled = p_dec_data->ppu_enabled_orig[i];
      }
    }
  }

  return (JPEGDEC_OK);
}
