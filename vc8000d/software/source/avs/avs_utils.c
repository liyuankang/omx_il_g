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

#include "avs_utils.h"

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

   5.6  Function name: AvsStrmDec_NextStartCode

------------------------------------------------------------------------------*/
u32 AvsStrmDec_NextStartCode(DecContainer * dec_cont) {
  u32 tmp;

  ASSERT(dec_cont);

  if (dec_cont->StrmDesc.bit_pos_in_word & 0x7) {
    tmp = AvsStrmDec_FlushBits(dec_cont,
                               8 - (dec_cont->StrmDesc.bit_pos_in_word & 0x7));
    if(tmp != HANTRO_OK)
      return END_OF_STREAM;
  }

  do {
    tmp = AvsStrmDec_ShowBits32(dec_cont);
    if((tmp >> 8) == 0x1) {
      if(AvsStrmDec_FlushBits(dec_cont, 32) != HANTRO_OK) {
        return END_OF_STREAM;
      } else {
        return (tmp & 0xFF);
      }
    }
  } while(AvsStrmDec_FlushBits(dec_cont, 8) == HANTRO_OK);

  return (END_OF_STREAM);

}

/*------------------------------------------------------------------------------

   5.8  Function name: AvsStrmDec_NumBits

        Purpose: computes number of bits needed to represent value given as
        argument

        Input:
            u32 value [0,2^32)

        Output:
            Number of bits needed to represent input value

------------------------------------------------------------------------------*/

u32 AvsStrmDec_NumBits(u32 value) {

  u32 num_bits = 0;

  while(value) {
    value >>= 1;
    num_bits++;
  }

  if(!num_bits) {
    num_bits = 1;
  }

  return (num_bits);

}

/*------------------------------------------------------------------------------

    Function: AvsStrmDec_GetBits

        Functional description:
            Read and remove bits from the stream buffer.

        Input:
            pStrmData   pointer to stream data structure
            num_bits     number of bits to read

        Output:
            none

        Returns:
            bits read from stream
            END_OF_STREAM if not enough bits left

------------------------------------------------------------------------------*/
u32 AvsStrmDec_GetBits(DecContainer * dec_cont, u32 num_bits) {

  u32 out;

  ASSERT(dec_cont);
  ASSERT(num_bits < 32);

  out = AvsStrmDec_ShowBits32(dec_cont) >> (32 - num_bits);

  if(AvsStrmDec_FlushBits(dec_cont, num_bits) == HANTRO_OK) {
    return (out);
  } else {
    return (END_OF_STREAM);
  }
}

/*------------------------------------------------------------------------------

    Function: mAvsStrmDec_ShowBits32

        Functional description:
            Read 32 bits from the stream buffer. Buffer is left as it is, i.e.
            no bits are removed. First bit read from the stream is the MSB of
            the return value. If there is not enough bits in the buffer ->
            bits beyong the end of the stream are set to '0' in the return
            value.

        Input:
            pStrmData   pointer to stream data structure

        Output:
            none

        Returns:
            bits read from stream

------------------------------------------------------------------------------*/
u32 AvsStrmDec_ShowBits32(DecContainer * dec_cont) {

  i32 bits, shift;
  u32 out;
  u8 *p_strm;

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmDesc.strm_curr_pos);
  ASSERT(dec_cont->StrmDesc.bit_pos_in_word < 8);
  ASSERT(dec_cont->StrmDesc.bit_pos_in_word ==
         (dec_cont->StrmDesc.strm_buff_read_bits & 0x7));

  p_strm = dec_cont->StrmDesc.strm_curr_pos;

  /* number of bits left in the buffer */
  bits =
    (i32) dec_cont->StrmDesc.strm_buff_size * 8 -
    (i32) dec_cont->StrmDesc.strm_buff_read_bits;

  /* at least 32-bits in the buffer */
  if(bits >= 32) {
    u32 bit_pos_in_word = dec_cont->StrmDesc.bit_pos_in_word;

    out = ((u32) p_strm[3]) | ((u32) p_strm[2] << 8) |
          ((u32) p_strm[1] << 16) | ((u32) p_strm[0] << 24);

    if(bit_pos_in_word) {
      out <<= bit_pos_in_word;
      out |= (u32) p_strm[4] >> (8 - bit_pos_in_word);
    }
    return (out);
  }
  /* at least one bit in the buffer */
  else if(bits > 0) {
    shift = (i32) (24 + dec_cont->StrmDesc.bit_pos_in_word);
    out = (u32) (*p_strm++) << shift;
    bits -= (i32) (8 - dec_cont->StrmDesc.bit_pos_in_word);
    while(bits > 0) {
      shift -= 8;
      out |= (u32) (*p_strm++) << shift;
      bits -= 8;
    }
    return (out);
  } else
    return (0);

}

/*------------------------------------------------------------------------------

    Function: AvsStrmDec_FlushBits

        Functional description:
            Remove bits from the stream buffer

        Input:
            pStrmData       pointer to stream data structure
            num_bits         number of bits to remove

        Output:
            none

        Returns:
            HANTRO_OK       success
            END_OF_STREAM   not enough bits left

------------------------------------------------------------------------------*/
u32 AvsStrmDec_FlushBits(DecContainer * dec_cont, u32 num_bits) {

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmDesc.p_strm_buff_start);
  ASSERT(dec_cont->StrmDesc.strm_curr_pos);
  ASSERT(dec_cont->StrmDesc.bit_pos_in_word < 8);
  ASSERT(dec_cont->StrmDesc.bit_pos_in_word ==
         (dec_cont->StrmDesc.strm_buff_read_bits & 0x7));

  dec_cont->StrmDesc.strm_buff_read_bits += num_bits;
  dec_cont->StrmDesc.bit_pos_in_word =
    dec_cont->StrmDesc.strm_buff_read_bits & 0x7;
  if((dec_cont->StrmDesc.strm_buff_read_bits) <=
      (8 * dec_cont->StrmDesc.strm_buff_size)) {
    dec_cont->StrmDesc.strm_curr_pos =
      dec_cont->StrmDesc.p_strm_buff_start +
      (dec_cont->StrmDesc.strm_buff_read_bits >> 3);
    return (HANTRO_OK);
  } else {
    dec_cont->StrmDesc.strm_curr_pos =
      dec_cont->StrmDesc.p_strm_buff_start +
      dec_cont->StrmDesc.strm_buff_size;
    dec_cont->StrmDesc.strm_buff_read_bits =
      8 * dec_cont->StrmDesc.strm_buff_size;
    dec_cont->StrmDesc.bit_pos_in_word = 0;
    return (END_OF_STREAM);
  }
}

/*------------------------------------------------------------------------------

   5.2  Function name: AvsStrmDec_ShowBits

        Purpose: read bits from mpeg-2 input stream. Bits are located right
        aligned in the 32-bit output word. In case stream ends,
        function fills the word with zeros. For example, num_bits = 18 and
        there are 7 bits left in the stream buffer -> return
            00000000000000xxxxxxx00000000000,
        where 'x's represent actual bits read from buffer.

        Input:
            Pointer to DecContainer structure
                -uses but does not update StrmDesc
            Number of bits to read [0,32]

        Output:
            u32 containing bits read from stream

------------------------------------------------------------------------------*/
u32 AvsStrmDec_ShowBits(DecContainer * dec_cont, u32 num_bits) {

  u32 i;
  i32 bits, shift;
  u32 out;
  u8 *pstrm = dec_cont->StrmDesc.strm_curr_pos;

  ASSERT(num_bits <= 32);

  /* bits left in the buffer */
  bits = (i32) dec_cont->StrmDesc.strm_buff_size * 8 -
         (i32) dec_cont->StrmDesc.strm_buff_read_bits;

  if(!num_bits || !bits) {
    return (0);
  }

  /* at least 32-bits in the buffer -> get 32 bits and drop extra bits out */
  if(bits >= 32) {
    out = ((u32) pstrm[0] << 24) | ((u32) pstrm[1] << 16) |
          ((u32) pstrm[2] << 8) | ((u32) pstrm[3]);
    if(dec_cont->StrmDesc.bit_pos_in_word) {
      out <<= dec_cont->StrmDesc.bit_pos_in_word;
      out |= (u32) pstrm[4] >> (8 - dec_cont->StrmDesc.bit_pos_in_word);
    }
  } else {
    shift = 24 + dec_cont->StrmDesc.bit_pos_in_word;
    out = (u32) pstrm[0] << shift;
    bits -= 8 - dec_cont->StrmDesc.bit_pos_in_word;
    i = 1;
    while(bits > 0) {
      shift -= 8;
      out |= (u32) pstrm[i] << shift;
      bits -= 8;
      i++;
    }
  }

  return (out >> (32 - num_bits));

}

/*------------------------------------------------------------------------------

   5.3  Function name: AvsStrmDec_ShowBitsAligned

        Purpose: read bits from mpeg-2 input stream at byte aligned position
        given as parameter (offset from current stream position).

        Input:
            Pointer to DecContainer structure
                -uses but does not update StrmDesc
            Number of bits to read [1,32]
            Byte offset from current stream position

        Output:
            u32 containing bits read from stream

------------------------------------------------------------------------------*/

u32 AvsStrmDec_ShowBitsAligned(DecContainer * dec_cont, u32 num_bits,
                               u32 byte_offset) {

  u32 i;
  u32 out;
  u32 bytes, shift;
  u8 *pstrm = dec_cont->StrmDesc.strm_curr_pos + byte_offset;

  ASSERT(num_bits);
  ASSERT(num_bits <= 32);
  out = 0;

  /* at least four bytes available starting byte_offset bytes ahead */
  if((dec_cont->StrmDesc.strm_buff_size >= (4 + byte_offset)) &&
      ((dec_cont->StrmDesc.strm_buff_read_bits >> 3) <=
       (dec_cont->StrmDesc.strm_buff_size - byte_offset - 4))) {
    out = ((u32) pstrm[0] << 24) | ((u32) pstrm[1] << 16) |
          ((u32) pstrm[2] << 8) | ((u32) pstrm[3]);
    out >>= (32 - num_bits);
  } else {
    bytes = dec_cont->StrmDesc.strm_buff_size -
            (dec_cont->StrmDesc.strm_buff_read_bits >> 3);
    if(bytes > byte_offset) {
      bytes -= byte_offset;
    } else {
      bytes = 0;
    }

    shift = 24;
    i = 0;
    while(bytes) {
      out |= (u32) pstrm[i] << shift;
      i++;
      shift -= 8;
      bytes--;
    }

    out >>= (32 - num_bits);
  }

  return (out);

}

/*------------------------------------------------------------------------------

   Function name: AvsStrmDec_CountLeadingZeros

        Purpose:

        Input:

        Output:

------------------------------------------------------------------------------*/

u32 AvsStrmDec_CountLeadingZeros(u32 value, u32 len) {

  u32 zeros = 0;
  u32 mask = 1 << (len - 1);

  ASSERT(len <= 32);

  while (mask && !(value & mask)) {
    zeros++;
    mask >>= 1;
  }

  return (zeros);

}

u32 AvsGetRefFrmSize(DecContainer *dec_cont) {
  u32 pic_size_in_mbs = 0;
  u32 pic_size;

  if( dec_cont->tiled_mode_support && dec_cont->allow_dpb_field_ordering) {
    dec_cont->tiled_stride_enable = 1;
  } else {
    dec_cont->tiled_stride_enable = 0;
  }
  if (!dec_cont->tiled_stride_enable) {
    pic_size_in_mbs = dec_cont->StrmStorage.total_mbs_in_frame;
    pic_size = pic_size_in_mbs * 384;
  } else {
    u32 out_w, out_h;
    out_w = NEXT_MULTIPLE(4 * dec_cont->StrmStorage.frame_width * 16,
                            ALIGN(dec_cont->align));
    out_h = dec_cont->StrmStorage.frame_height * 4;
    pic_size = out_w * out_h * 3 / 2;
  }
  return pic_size;
}
