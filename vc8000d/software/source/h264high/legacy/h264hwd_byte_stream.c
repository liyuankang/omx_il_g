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

#include "h264hwd_byte_stream.h"
#include "h264hwd_util.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define BYTE_STREAM_ERROR  0xFFFFFFFF

#ifdef USE_WORDACCESS
/* word width */
#define WORDWIDTH_64BIT
//#define WORDWIDTH_32BIT

#if defined (WORDWIDTH_64BIT)
typedef u64 uword;
#define WORDWIDTH                 64
#define WORDADDR_REMAINDER_MASK   0x07

#elif defined(WORDWIDTH_32BIT)
typedef u32 uword;
#define WORDWIDTH                 32
#define WORDADDR_REMAINDER_MASK   0x03

#else
#error "Please specify word length!"
#endif
#endif


/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

#ifdef USE_WORDACCESS
static u32 IsLittleEndian()
{
  uword i = 1;
  if (*((u8 *)&i))
    return 1;
  else
    return 0;
}
#endif


/*------------------------------------------------------------------------------

    Function name: ExtractNalUnit

        Functional description:
            Extracts one NAL unit from the byte stream buffer. Removes
            emulation prevention bytes if present. The original stream buffer
            is used directly and is therefore modified if emulation prevention
            bytes are present in the stream.

            Stream buffer is assumed to contain either exactly one NAL unit
            and nothing else, or one or more NAL units embedded in byte
            stream format described in the Annex B of the standard. Function
            detects which one is used based on the first bytes in the buffer.

        Inputs:
            p_byte_stream     pointer to byte stream buffer
            len             length of the stream buffer (in bytes)

        Outputs:
            p_strm_data       stream information is stored here
            read_bytes       number of bytes "consumed" from the stream buffer

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      error in byte stream

------------------------------------------------------------------------------*/

u32 h264bsdExtractNalUnit(const u8 * p_byte_stream, u32 len,
                          u8 *strm_buf, u32 buf_len, strmData_t * p_strm_data,
                          u32 * read_bytes, u32 rlc_mode, u32 *start_code_detected) {

  /* Variables */
  u32 data_count = 0;
  u32 zero_count = 0;
  u32 byte = 0;
  u32 invalid_stream = HANTRO_FALSE;
  u32 has_emulation = HANTRO_FALSE;
  const u8 *read_ptr;
  strmData_t tmp_strm;
  /* Code */

  ASSERT(p_byte_stream);
  ASSERT(len);
  ASSERT(len < BYTE_STREAM_ERROR);
  ASSERT(p_strm_data);

  /* from strm to buf end */
  p_strm_data->p_strm_buff_start = strm_buf;
  p_strm_data->strm_curr_pos = p_byte_stream;
  p_strm_data->bit_pos_in_word = 0;
  p_strm_data->strm_buff_read_bits = 0;
  p_strm_data->strm_buff_size = buf_len;
  p_strm_data->strm_data_size = len;

  p_strm_data->remove_emul3_byte = 1;

  /* byte stream format if starts with 0x000001 or 0x000000. Force using
   * byte stream format if start codes found earlier. */
  if (*start_code_detected || (h264bsdShowBits(p_strm_data, 24) <= 0x01 && len > 3)) {
    *start_code_detected = 1;
    DEBUG_PRINT(("BYTE STREAM detected\n"));

    /* search for NAL unit start point, i.e. point after first start code
     * prefix in the stream */
    while (h264bsdShowBits(p_strm_data, 24) != 0x01) {
      if (h264bsdFlushBits(p_strm_data, 8) == END_OF_STREAM) {
        *read_bytes = len;
        p_strm_data->remove_emul3_byte = 0;
        return HANTRO_NOK;
      }
      data_count++;
    }
    if (h264bsdFlushBits(p_strm_data, 24) == END_OF_STREAM) {
      *read_bytes = len;
      p_strm_data->remove_emul3_byte = 0;
      return HANTRO_NOK;
    }
    data_count += 3;

    if(!rlc_mode) {
      p_strm_data->remove_emul3_byte = 0;
      *read_bytes = p_strm_data->strm_buff_read_bits / 8;
      return (HANTRO_OK);
    }

    /* determine szie of the NAL unit. Search for next start code prefix
     * or end of stream and ignore possible trailing zero bytes */
    tmp_strm = *p_strm_data;
#if 0
    while (1) {
      if(h264bsdShowBits(&tmp_strm, 24) <= 0x01) {
        p_strm_data->strm_data_size = data_count;
        break;
      }
      data_count++;
      if (h264bsdFlushBits(&tmp_strm, 8) == END_OF_STREAM) {
        break;
      }
    }
#else
    zero_count = 0;
    /*lint -e(716) while(1) used consciously */
    while(1) {
      byte = h264bsdGetBits(&tmp_strm, 8);
      if (byte == END_OF_STREAM) {
        p_strm_data->strm_data_size = len - zero_count;
        break;
      }

      data_count++;
      if(!byte)
        zero_count++;
      else {
        if((byte == 0x03) && (zero_count == 2)) {
          has_emulation = HANTRO_TRUE;
        } else if((byte == 0x01) && (zero_count >= 2)) {
          p_strm_data->strm_data_size =
            data_count - zero_count - 1;
          zero_count -= MIN(zero_count, 3);
          break;
        }

        if(zero_count >= 3)
          invalid_stream = HANTRO_TRUE;

        zero_count = 0;
      }
    }
#endif
  } else {
  /* separate NAL units as input -> just set stream params */
    DEBUG_PRINT(("SINGLE NAL unit detected\n"));
    has_emulation = HANTRO_TRUE;
  }

  /* return number of bytes "consumed" */
  *read_bytes = p_strm_data->strm_data_size + zero_count;

  /* TODO: Ring buffer in RLC mode is not implemented yet,
   * sw needs reset ringbuffer=0*/
  if (p_strm_data->is_rb && rlc_mode) {
    DEBUG_PRINT(("ring buffer is not supported in RLC mode\n"));
    //return (HANTRO_NOK);
  }

  if(invalid_stream) {

    ERROR_PRINT("INVALID STREAM");
    return (HANTRO_NOK);
  }

  /* remove emulation prevention bytes before rbsp processing */
  if(has_emulation && p_strm_data->remove_emul3_byte) {
    i32 i = p_strm_data->strm_data_size - p_strm_data->strm_buff_read_bits/8;
    u8 *write_ptr = (u8 *) p_strm_data->strm_curr_pos;

    read_ptr = p_strm_data->strm_curr_pos;

    zero_count = 0;
    while(i--) {
      if((zero_count == 2) && (h264ReadByte(read_ptr) == 0x03)) {
        /* emulation prevention byte shall be followed by one of the
         * following bytes: 0x00, 0x01, 0x02, 0x03. This implies that
         * emulation prevention 0x03 byte shall not be the last byte
         * of the stream. */
        if((i == 0) || (h264ReadByte(read_ptr + 1) > 0x03))
          return (HANTRO_NOK);

        DEBUG_PRINT(("EMULATION PREVENTION 3 BYTE REMOVED\n"));

        /* do not write emulation prevention byte */
        read_ptr++;
        zero_count = 0;
      } else {
        /* NAL unit shall not contain byte sequences 0x000000,
         * 0x000001 or 0x000002 */
        if((zero_count == 2) && (h264ReadByte(read_ptr) <= 0x02))
          return (HANTRO_NOK);

        if(h264ReadByte(read_ptr) == 0)
          zero_count++;
        else
          zero_count = 0;

        *write_ptr++ = h264ReadByte(read_ptr);
        read_ptr++;
      }
    }

    /* (read_ptr - write_ptr) indicates number of "removed" emulation
     * prevention bytes -> subtract from stream buffer size */
    p_strm_data->strm_data_size -= (u32) (read_ptr - write_ptr);
  }

  return (HANTRO_OK);
}

/*------------------------------------------------------------------------------
    Function name   : h264bsdFindNextStartCode
    Description     :
    Return type     : const u8 *
    Argument        : const u8 * p_byte_stream
    Argument        : u32 len
------------------------------------------------------------------------------*/
const u8 *h264bsdFindNextStartCode(const u8 * p_byte_stream, u32 len) {
#ifndef USE_WORDACCESS
  u32 byte_count = 0;
  u32 zero_count = 0;
  const u8 *start = NULL;

  /* determine size of the NAL unit. Search for next start code prefix
   * or end of stream  */

  while(byte_count++ < len) {
    u32 byte = DWLPrivateAreaReadByte(p_byte_stream);
    if(byte == BYTE_STREAM_ERROR)
      break;

    p_byte_stream++;
    if(byte == 0)
      zero_count++;
    else {
      if((byte == 0x01) && (zero_count >= 2)) {
        start = p_byte_stream - MIN(zero_count, 3) - 1;
        break;
      }

      zero_count = 0;
    }
  }

  return start;
#else
  const u8 * strm = p_byte_stream;
  const u8 * start;
  u32 found = 0;
  u32 data = 0xff;
  u32 byte_count = 0;
  u32 zero_count = 0; // run zero
  uword data_in_word = 0;
  i32 bit_pos = 0;
  u32 align = (WORDWIDTH / 8) - ((addr_t)p_byte_stream & WORDADDR_REMAINDER_MASK);
  u32 is_little_e = IsLittleEndian();

  if (align == WORDWIDTH / 8) align = 0;
  if (align > len) align = len;

  /* prologue */
  while(align > byte_count) {
    data = DWLPrivateAreaReadByte(strm);
    if(data == BYTE_STREAM_ERROR)
      break;
    strm++;
    byte_count++;

    if(data == 0) zero_count++;
    else if((data == 0x01) && (zero_count >= 2)) {
      strm = strm - MIN(zero_count, 3) - 1;
      found = 1;
      break;
    } else
      zero_count = 0;
  }

  /* word access search */
  if(found == 0) {
    while(len >= (byte_count + WORDWIDTH / 8)) {
      /* Make sure word access is valid */
#ifdef WORDWIDTH_64BIT
      if (DWLPrivateAreaReadByte(strm + 7) == BYTE_STREAM_ERROR)
#else
      if (DWLPrivateAreaReadByte(strm + 3) == BYTE_STREAM_ERROR)
#endif
        break;
      data_in_word = *((uword *)strm);

      if(data_in_word == 0x00) {
        /* boost zero stuffing skip performance */
        zero_count += (WORDWIDTH / 8);
        byte_count += (WORDWIDTH / 8);
        strm += (WORDWIDTH / 8);
      } else {
        bit_pos = 0;
        do {
          /* big endian or small endian */
          if(is_little_e)
            data = (u8)(data_in_word >> bit_pos);
          else
            data = (u8)(data_in_word >> ((WORDWIDTH - 8) - bit_pos));

          bit_pos += 8; // 8bit shift

          if(data == 0x0)
            zero_count++;
          else if(data == 0x01 && zero_count >= 2) {
            found = 1;
            strm = strm + bit_pos/8 - MIN(zero_count, 3) - 1;
            break;
          } else
            zero_count = 0;

        } while(bit_pos < WORDWIDTH);

        if(found) break;

        strm += (bit_pos >> 3); // bit to byte
        byte_count += (bit_pos >> 3);
      }
    }
  } // word access search end

  /* epilogue */
  if(found == 0) {
    while(len > byte_count) {
      data = DWLPrivateAreaReadByte(strm);
      if(data == BYTE_STREAM_ERROR)
        break;
      strm++;
      byte_count++;

      if(data == 0x0)
        zero_count++;
      else if(data == 0x01 && zero_count >= 2) {
        strm = strm - MIN(zero_count, 3) - 1;
        found = 1;
        break;
      } else
        zero_count = 0;
    }
  }

  /* update status & retrun code*/
  if(found && (len > byte_count))
    start = strm;
  else
    /* not found, discard all */
    start = NULL;

  return start;
#endif
}

u32 H264NextStartCode(strmData_t * p_byte_stream) {
#ifndef USE_WORDACCESS
  u32 tmp;

  if (p_byte_stream->bit_pos_in_word)
    h264bsdGetBits(p_byte_stream, 8 - p_byte_stream->bit_pos_in_word);

  p_byte_stream->remove_emul3_byte = 1;

  while (1) {
    if (p_byte_stream->strm_data_size - 4 <
          p_byte_stream->strm_buff_read_bits / 8)
      return HANTRO_NOK;
    tmp = h264bsdShowBits(p_byte_stream, 32);
    if (tmp <= 0x01 || (tmp >> 8) == 0x01) {
      p_byte_stream->remove_emul3_byte = 0;
      return HANTRO_OK;
    }

    if (h264bsdFlushBits(p_byte_stream, 8) == END_OF_STREAM) {
      p_byte_stream->remove_emul3_byte = 0;
      return END_OF_STREAM;
    }
  }
  return HANTRO_OK;
#else
  const u8 * strm;
  u32 found = 0;
  u8 data = 0xff;
  u32 byte_count = 0;
  u32 zero_count = 0; // run zero
  uword data_in_word = 0;
  i32 bit_pos = 0;
  u32 align = 0;
  u32 len = 0;
  u32 is_little_e = IsLittleEndian();

  /* byte align */
  if (stream->bit_pos_in_word) SwGetBits(p_byte_stream, 8 - p_byte_stream->bit_pos_in_word);

  ASSERT(p_byte_stream->strm_curr_pos <=
         (p_byte_stream->strm_buff_start + p_byte_stream->strm_buff_size));
  ASSERT(p_byte_stream->strm_buff_read_bits <= p_byte_stream->strm_data_size * 8);

  strm = p_byte_stream->strm_curr_pos;
  align = (WORDWIDTH / 8) - ((addr_t)(p_byte_stream->strm_curr_pos) &
           WORDADDR_REMAINDER_MASK);

  /* data left in buffer */
  len = p_byte_stream->strm_data_size - p_byte_stream->strm_buff_read_bits/8;

  if (align == WORDWIDTH / 8)
    align = 0;

  if(align > len)
    align = len;

    /* prologue */
  while(align > byte_count) {
    data = h264bsdShowBits(p_byte_stream, 32);
    if(data == BYTE_STREAM_ERROR)
      break;
    h264bsdFlushBits(p_byte_stream, 8);
    byte_count++;

    if(data == 0)
      zero_count++;
    else if((data == 0x01) && (zero_count >= 2)) {
      found = 1;
      break;
    }
    else
      zero_count = 0;
  }
    /* word access search */
  if(found == 0) {
    while(len >= (byte_count + WORDWIDTH / 8)) {
      data_in_word = *((uword *)strm);

      if(data_in_word == 0x00) {
        /* boost zero stuffing skip performance */
        zero_count += (WORDWIDTH / 8);
        byte_count += (WORDWIDTH / 8);
        strm += (WORDWIDTH / 8);
      }
      else {
        bit_pos = 0;
        do {
          /* big endian or small endian */
          if(is_little_e)
            data = (u8)(data_in_word >> bit_pos);
          else
            data = (u8)(data_in_word >> ((WORDWIDTH - 8) - bit_pos));

          bit_pos += 8; // 8bit shift

          if(data == 0x0)
            zero_count++;
          else if(data == 0x01 && zero_count >= 2) {
            found = 1;
            strm += bit_pos/8;
            byte_count += bit_pos/8;
            break;
          }
          else
            zero_count = 0;

        } while(bit_pos < WORDWIDTH);

        if(found)
          break;

        strm += (bit_pos >> 3); // bit to byte
        byte_count += (bit_pos >> 3);
      }
    }
  } // word access search end
    /* epilogue */
  if(found == 0) {
    while(len > byte_count) {
      data = h264bsdShowBits(p_byte_stream, 32);
      if(data == BYTE_STREAM_ERROR)
        break;
      h264bsdFlushBits(p_byte_stream, 8);
      byte_count++;

      if(data == 0x0)
        zero_count++;
      else if(data == 0x01 && zero_count >= 2) {
        found = 1;
        break;
      }
      else
        zero_count = 0;
    }
  }

  p_byte_stream->bit_pos_in_word = 0;
  p_byte_stream->remove_emul3_byte = 0;
//  stream->emul_byte_count = 0;

  /* update status & retrun code*/
  if(found && (len > byte_count)) {
    p_byte_stream->strm_buff_read_bits += (byte_count - 3) * 8;
    return HANTRO_OK;
  }
  else {
    /* not found, discard all */
    p_byte_stream->strm_buff_read_bits = p_byte_stream->strm_data_size * 8;

    //return HANTRO_OK;
    return END_OF_STREAM;
  }
#endif
}
