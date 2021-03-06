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

#include "sw_util.h"
#include "sw_stream.h"
#include "sw_debug.h"

extern volatile struct strmInfo stream_info;
u32 SwGetBits(struct StrmData *stream, u32 num_bits) {

  u32 out;

  ASSERT(stream);
  ASSERT(num_bits < 32);

  if (num_bits == 0) return 0;

  out = SwShowBits(stream, 32) >> (32 - num_bits);

  if (SwFlushBits(stream, num_bits) == HANTRO_OK) {
    return (out);
  } else {
    return (END_OF_STREAM);
  }
}

u32 SwGetBitsUnsignedMax(struct StrmData *stream, u32 max_value) {
  i32 bits = 0;
  u32 num_values = max_value;
  u32 value;

  /* Bits for unsigned value */
  if (num_values > 1) {
    num_values--;
    while (num_values > 0) {
      bits++;
      num_values >>= 1;
    }
  }

  value = SwGetBits(stream, bits);
  return (value > max_value) ? max_value : value;
}

u32 SwShowBits(struct StrmData *stream, u32 num_bits) {

  i32 bits;
  u32 out, out_bits;
  u32 tmp_read_bits;
  u32 turn = 0;
  const u8 *strm;
  /* used to copy stream data when ringbuffer turnaround */
  u8 tmp_strm_buf[32], *tmp;

  ASSERT(stream);
  ASSERT(stream->strm_curr_pos);
  ASSERT(stream->bit_pos_in_word < 8);
  ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));
  ASSERT(num_bits <= 32);

  strm = stream->strm_curr_pos;

  /* bits left in the buffer */
  if (stream_info.low_latency && stream_info.last_flag)
    stream->strm_data_size = stream_info.send_len;
  bits = (i32)stream->strm_data_size * 8 - (i32)stream->strm_buff_read_bits;
  if (!bits) {
    return (0);
  }

  tmp = SwTurnAround(stream->strm_curr_pos, stream->strm_buff_start,
                     tmp_strm_buf, stream->strm_buff_size,
                     MIN(bits, num_bits + stream->bit_pos_in_word + 32));

  if(tmp != NULL) {
    strm = tmp;
    turn = 1;
  }
  if (!stream->remove_emul3_byte) {

    out = out_bits = 0;
    tmp_read_bits = stream->strm_buff_read_bits;

    if (stream->bit_pos_in_word) {
      out = (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << (24 + stream->bit_pos_in_word);
      strm++;
      out_bits = 8 - stream->bit_pos_in_word;
      bits -= out_bits;
      tmp_read_bits += out_bits;
    }

    while (bits && out_bits < num_bits) {

      /* check emulation prevention byte */
      if (tmp_read_bits >= 16 &&
          (turn ? DWLPrivateAreaReadByte(strm -2) : DWLLowLatencyReadByte(strm -2, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm -1) : DWLLowLatencyReadByte(strm -1, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x3) {
        strm++;
        tmp_read_bits += 8;
        bits -= 8;
        /* emulation prevention byte shall not be the last byte of the
         * stream */
        if (bits <= 0) break;
      }

      tmp_read_bits += 8;

      if (out_bits <= 24) {
        out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << (24 - out_bits);
        strm++;
      } else {
        out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) >> (out_bits - 24);
        strm++;
      }

      out_bits += 8;
      bits -= 8;
    }

    return (out >> (32 - num_bits));

  }
  else if(stream->remove_avs_fake_2bits)
  {
    out = out_bits = 0;
    tmp_read_bits = stream->strm_buff_read_bits;

    if (stream->bit_pos_in_word) {
      out = (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << (24 + stream->bit_pos_in_word);


      /* check emulation prevention byte */
      if (tmp_read_bits >= 16 &&
          (turn ? DWLPrivateAreaReadByte(strm -2) : DWLLowLatencyReadByte(strm -2, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm -1) : DWLLowLatencyReadByte(strm -1, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x2) {
        strm++;
        tmp_read_bits += 8 - stream->bit_pos_in_word;
        bits -= 8 - stream->bit_pos_in_word;

        out_bits = 8 - stream->bit_pos_in_word - 2;

        out &=~((1<<(32-out_bits))-1);
      }
      else
      {
        strm++;
        tmp_read_bits += 8 - stream->bit_pos_in_word;
        bits -= 8 - stream->bit_pos_in_word;
        out_bits = 8 - stream->bit_pos_in_word;
      }

    }

    while (bits && out_bits < num_bits) {

      /* check emulation prevention byte */
      if (tmp_read_bits >= 16 &&
          (turn ? DWLPrivateAreaReadByte(strm -2) : DWLLowLatencyReadByte(strm -2, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm -1) : DWLLowLatencyReadByte(strm -1, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x2) {

        if (out_bits <= 24) {
          out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << (24 - out_bits);
        } else {
          out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) >> (out_bits - 24);
        }

        strm++;
        tmp_read_bits += 8;
        bits -= 8;
        out_bits += 6;
        out &=~((1<<(32-out_bits))-1);
      }
      else
      {
          if (out_bits <= 24) {
            out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << (24 - out_bits);
          } else {
            out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) >> (out_bits - 24);
          }

          strm++;
          tmp_read_bits += 8;
          bits -= 8;
          out_bits += 8;
      }
    }

    return (out >> (32 - num_bits));
  }
  else {
    u32 shift;

    /* at least 32-bits in the buffer */
    if (bits >= 32) {
      u32 bit_pos_in_word = stream->bit_pos_in_word;

      out = ((u32)(turn ? DWLPrivateAreaReadByte(strm + 3) : DWLLowLatencyReadByte(strm + 3, stream->strm_buff_size))) |
            ((u32)(turn ? DWLPrivateAreaReadByte(strm + 2) : DWLLowLatencyReadByte(strm + 2, stream->strm_buff_size)) << 8) |
            ((u32)(turn ? DWLPrivateAreaReadByte(strm + 1) : DWLLowLatencyReadByte(strm + 1, stream->strm_buff_size)) << 16) |
            ((u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << 24);

      if (bit_pos_in_word) {
        out <<= bit_pos_in_word;
        out |= (u32)(turn ? DWLPrivateAreaReadByte(strm + 4) : DWLLowLatencyReadByte(strm + 4, stream->strm_buff_size)) >> (8 - bit_pos_in_word);
      }

      return (out >> (32 - num_bits));
    }
    /* at least one bit in the buffer */
    else if (bits > 0) {
      shift = (i32)(24 + stream->bit_pos_in_word);
      out = (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << shift;
      strm++;
      bits -= (i32)(8 - stream->bit_pos_in_word);
      while (bits > 0) {
        shift -= 8;
        out |= (u32)(turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) << shift;
        strm++;
        bits -= 8;
      }
      return (out >> (32 - num_bits));
    } else
      return (0);
  }
}

u32 SwFlushBits(struct StrmData *stream, u32 num_bits) {

  u32 bytes_left;
  const u8 *strm, *strm_bak;
  u8 tmp_strm_buf[32], *tmp;
  u32 turn = 0;
  i32 bits;
  u32 out_bits;
  u32 tmp_read_bits;

  ASSERT(stream);
  ASSERT(stream->strm_buff_start);
  ASSERT(stream->strm_curr_pos);
  ASSERT(stream->bit_pos_in_word < 8);
  ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));
  if (stream_info.low_latency && stream_info.last_flag)
    stream->strm_data_size = stream_info.send_len;

  bits = (i32)stream->strm_data_size * 8 - (i32)stream->strm_buff_read_bits;

  if (!bits) {
    return (END_OF_STREAM);
  }

  /* used to copy stream data when ringbuffer turnaround */
  tmp = SwTurnAround(stream->strm_curr_pos, stream->strm_buff_start,
                     tmp_strm_buf, stream->strm_buff_size,
                     MIN(bits, num_bits + stream->bit_pos_in_word + 32));
  if (tmp != NULL)
    turn = 1;

  if (!stream->remove_emul3_byte) {
    if ((stream->strm_buff_read_bits + num_bits) >
        (8 * stream->strm_data_size)) {
      stream->strm_buff_read_bits = 8 * stream->strm_data_size;
      stream->bit_pos_in_word = 0;
      stream->strm_curr_pos = stream->strm_buff_start + stream->strm_buff_size;
      return (END_OF_STREAM);
    } else {
      bytes_left =
        (8 * stream->strm_data_size - stream->strm_buff_read_bits) / 8;
      if(tmp != NULL)
        strm = tmp;
      else
        strm = stream->strm_curr_pos;
      strm_bak = strm;

      if (stream->bit_pos_in_word) {
        if (num_bits < 8 - stream->bit_pos_in_word) {
          stream->strm_buff_read_bits += num_bits;
          stream->bit_pos_in_word += num_bits;
          return (HANTRO_OK);
        }
        num_bits -= 8 - stream->bit_pos_in_word;
        stream->strm_buff_read_bits += 8 - stream->bit_pos_in_word;
        stream->bit_pos_in_word = 0;
        strm++;

        if (stream->strm_buff_read_bits >= 16 && bytes_left &&
            (turn ? DWLPrivateAreaReadByte(strm - 2) : DWLLowLatencyReadByte(strm - 2, stream->strm_buff_size)) == 0x0 &&
            (turn ? DWLPrivateAreaReadByte(strm - 1) : DWLLowLatencyReadByte(strm - 1, stream->strm_buff_size)) == 0x0 &&
            (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x3) {
          strm++;
          stream->strm_buff_read_bits += 8;
          bytes_left--;
          stream->emul_byte_count++;
        }
      }

      while (num_bits >= 8 && bytes_left) {
        if (bytes_left > 2 && (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0 &&
            (turn ? DWLPrivateAreaReadByte(strm + 1) : DWLLowLatencyReadByte(strm + 1, stream->strm_buff_size)) == 0 &&
            (turn ? DWLPrivateAreaReadByte(strm + 2) : DWLLowLatencyReadByte(strm + 2, stream->strm_buff_size)) <= 1) {
          /* trying to flush part of start code prefix -> error */
          return (HANTRO_NOK);
        }

        strm++;
        stream->strm_buff_read_bits += 8;
        bytes_left--;

        /* check emulation prevention byte */
        if (stream->strm_buff_read_bits >= 16 && bytes_left &&
            (turn ? DWLPrivateAreaReadByte(strm - 2) : DWLLowLatencyReadByte(strm - 2, stream->strm_buff_size)) == 0x0 &&
            (turn ? DWLPrivateAreaReadByte(strm - 1) : DWLLowLatencyReadByte(strm - 1, stream->strm_buff_size))== 0x0 &&
            (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x3) {
          strm++;
          stream->strm_buff_read_bits += 8;
          bytes_left--;
          stream->emul_byte_count++;
        }
        num_bits -= 8;
      }

      if (num_bits && bytes_left) {
        if (bytes_left > 2 && (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0 &&
            (turn ? DWLPrivateAreaReadByte(strm + 1) : DWLLowLatencyReadByte(strm + 1, stream->strm_buff_size)) == 0 &&
            (turn ? DWLPrivateAreaReadByte(strm + 2) : DWLLowLatencyReadByte(strm + 2, stream->strm_buff_size)) <= 1) {
          /* trying to flush part of start code prefix -> error */
          return (HANTRO_NOK);
        }

        stream->strm_buff_read_bits += num_bits;
        stream->bit_pos_in_word = num_bits;
        num_bits = 0;
      }

      stream->strm_curr_pos += strm - strm_bak;
      if (stream->is_rb && stream->strm_curr_pos >= (stream->strm_buff_start + stream->strm_buff_size))
        stream->strm_curr_pos -= stream->strm_buff_size;

      if (num_bits)
        return (END_OF_STREAM);
      else
        return (HANTRO_OK);
    }
  }
  else if(stream->remove_avs_fake_2bits)
  {
    u32 bit_shift=0;
    u32 bytes_shift = 0;
    u32 bit_left = 0;
    out_bits = 0;
    tmp_read_bits = stream->strm_buff_read_bits;
    strm = stream->strm_curr_pos;

    do
    {
      /* check emulation prevention byte */
      if (tmp_read_bits >= 16 &&
          (turn ? DWLPrivateAreaReadByte(strm -2) : DWLLowLatencyReadByte(strm -2, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm -1) : DWLLowLatencyReadByte(strm -1, stream->strm_buff_size)) == 0x0 &&
          (turn ? DWLPrivateAreaReadByte(strm) : DWLLowLatencyReadByte(strm, stream->strm_buff_size)) == 0x2) {
        bit_left = (8 - (tmp_read_bits%8));
        if(num_bits>=out_bits+bit_left-2)
        {
            bit_shift +=2;
            out_bits += bit_left-2;
            strm++;
            tmp_read_bits += bit_left;
            bits -= bit_left;
        }
        else
        {
            tmp_read_bits += num_bits-out_bits;
            bits -= num_bits-out_bits;
            out_bits = num_bits;
        }

      }
      else
      {
        bit_left = (8 - (tmp_read_bits%8));
        if(num_bits>=out_bits+bit_left)
        {
            out_bits += bit_left;
            strm++;
            tmp_read_bits += bit_left;
            bits -= bit_left;
        }
        else
        {
            tmp_read_bits += num_bits-out_bits;
            bits -= num_bits-out_bits;
            out_bits = num_bits;
        }
      }

    }while(out_bits < num_bits && bits > 0);

    bytes_shift = (stream->bit_pos_in_word + num_bits + bit_shift) >> 3;
    stream->strm_buff_read_bits += num_bits + bit_shift;
    stream->bit_pos_in_word = stream->strm_buff_read_bits & 0x7;
    if ((stream->strm_buff_read_bits) <= (8 * stream->strm_data_size)) {
      stream->strm_curr_pos += bytes_shift;
      if (stream->is_rb && stream->strm_curr_pos >= (stream->strm_buff_start + stream->strm_buff_size))
        stream->strm_curr_pos -= stream->strm_buff_size;
      return (HANTRO_OK);
    } else
      return (END_OF_STREAM);
  }
  else {
    u32 bytes_shift = (stream->bit_pos_in_word + num_bits) >> 3;
    stream->strm_buff_read_bits += num_bits;
    stream->bit_pos_in_word = stream->strm_buff_read_bits & 0x7;
    if ((stream->strm_buff_read_bits) <= (8 * stream->strm_data_size)) {
      stream->strm_curr_pos += bytes_shift;
      if (stream->is_rb && stream->strm_curr_pos >= (stream->strm_buff_start + stream->strm_buff_size))
        stream->strm_curr_pos -= stream->strm_buff_size;
      return (HANTRO_OK);
    } else
      return (END_OF_STREAM);
  }
}

u32 SwIsByteAligned(const struct StrmData *stream) {

  if (!stream->bit_pos_in_word)
    return (HANTRO_TRUE);
  else
    return (HANTRO_FALSE);
}
