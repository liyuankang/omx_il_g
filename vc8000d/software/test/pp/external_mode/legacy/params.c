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

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "defs.h"
#include "cfg.h"

/*------------------------------------------------------------------------------
    Blocks
------------------------------------------------------------------------------*/
#define BLOCK_PP_PARAMS         ("PpParams")
#define BLOCK_INPUT             ("Input")
#define BLOCK_OUTPUT            ("Output")
#define BLOCK_P_I_P             ("PIP")
#define BLOCK_ZOOM              ("Zoom")
#define BLOCK_TRACE             ("Trace")
#define BLOCK_COLOR_CONVERSION  ("ColorConversion")
#define BLOCK_MASK              ("Mask")
#define BLOCK_FIR_FILTER        ("FIRFilter")
#define BLOCK_SCALING           ("Scaling")
#define BLOCK_OVERLAY           ("Overlay")
#define BLOCK_DEINTERLACING     ("Deinterlacing")
#define BLOCK_RANGE_MAPPING     ("RangeMapping")

/*------------------------------------------------------------------------------
    Keys
------------------------------------------------------------------------------*/
#define KEY_ROTATION            ("Rotation")
#define KEY_WIDTH               ("Width")
#define KEY_HEIGHT              ("Height")
#define KEY_X_ORIGIN            ("x0")
#define KEY_Y_ORIGIN            ("y0")
#define KEY_LEFT                ("Left")
#define KEY_RIGHT               ("Right")
#define KEY_TOP                 ("Top")
#define KEY_BOTTOM              ("Bottom")
#define KEY_SEQUENCE            ("Sequence")
#define KEY_FIELD_OUTPUT        ("FieldOutput")
#define KEY_DITHER_ENA          ("DitherEna")
#define KEY_BACKGROUND          ("Background")
#define KEY_FRAMES              ("Frames")
#define KEY_FIRST_FRAME         ("FirstFrame")
#define KEY_VIDEO_RANGE         ("VideoRange")
#define KEY_FORMAT              ("Format")
#define KEY_TRACE               ("Trace")
#define KEY_OFFSET1             ("Offset1")
#define KEY_OFFSET2             ("Offset2")
#define KEY_THRESHOLD1          ("Threshold1")
#define KEY_THRESHOLD2          ("Threshold2")
#define KEY_A1                  ("a1")
#define KEY_A2                  ("a2")
#define KEY_A                   ("a")
#define KEY_B                   ("b")
#define KEY_C                   ("c")
#define KEY_D                   ("d")
#define KEY_E                   ("e")
#define KEY_F                   ("f")
#define KEY_BRIGHTNESS          ("Brightness")
#define KEY_CONTRAST            ("Contrast")
#define KEY_SATURATION          ("Saturation")
#define KEY_COLOR_CONVERSION    ("ColorConversion")
#define KEY_SCALING             ("Scaling")
#define KEY_CHANNELS            ("Channels")
#define KEY_COEFFICIENTS        ("Coeffs")
#define KEY_DIVISOR             ("Divisor")
#define KEY_RGB_FORMAT          ("RgbFormat")
#define KEY_RGB_ORDER           ("RgbOrder")
#define KEY_BYTE_ORDER          ("ByteOrder")
/*#define KEY_VERTICAL            ("Vertical")*/
/*#define KEY_HORIZONTAL          ("Horizontal")*/
#define KEY_INPUT               ("Input")
#define KEY_OUTPUT              ("Output")
/*#define KEY_DOWNSAMPLING        ("Downsampling")*/
#define KEY_PIP                 ("PIP")
#define KEY_WORD_WIDTH          ("WordWidth")
#define KEY_PIPELINE            ("Pipeline")
#define KEY_FILTER_ENABLED      ("FilterEnabled")
#define KEY_JPEG                ("JPEG")
#define KEY_SOURCE              ("Source")
#define KEY_ALPHA_SOURCE        ("AlphaSource")
#define KEY_BLEND_ENABLE        ("BlendEnable")
#define KEY_EDGE_DETECT         ("EdgeDetect")
#define KEY_THRESHOLD           ("Threshold")
#define KEY_ENABLE_Y            ("EnableY")
#define KEY_ENABLE_C            ("EnableC")
#define KEY_COEFF_Y             ("CoeffY")
#define KEY_COEFF_C             ("CoeffC")
#define KEY_STRUCT              ("Struct")
#define KEY_CROP_8_RIGHT        ("Crop8Rightmost")
#define KEY_CROP_8_DOWN         ("Crop8Downmost")

static void InitBlock(PpParams *pp);

/*------------------------------------------------------------------------------
    Implement reading interer parameter
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_INTEGER( b, k, tgt) \
    if(!strcmp(block, b) && !strcmp(key, k)) { \
        char *endptr; \
        tgt = strtol( value, &endptr, 10 ); \
        if(*endptr) return CFG_INVALID_VALUE; \
    }
/*------------------------------------------------------------------------------
    Implement reading interer parameter if condition applies
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_INTEGER_COND( b, k, tgt, cond) \
    if((cond) && !strcmp(block, b) && !strcmp(key, k)) { \
        char *endptr; \
        tgt = strtol( value, &endptr, 10 ); \
        if(*endptr) return CFG_INVALID_VALUE; \
    }
/*------------------------------------------------------------------------------
    Implement reading string parameter
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_STRING( b, k, tgt) \
    if(!strcmp(block, b) && !strcmp(key, k)) { \
        strncpy(tgt, value, sizeof(tgt)); \
    }

/*------------------------------------------------------------------------------
    Implement reading string parameter if condition applies
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_STRING_COND( b, k, tgt, cond) \
    if((cond) && !strcmp(block, b) && !strcmp(key, k)) { \
        strncpy(tgt, value, sizeof(tgt)); \
    }

/*------------------------------------------------------------------------------
    Implement reading code parameter; Code parsing is handled by supplied
    function fn.
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_CODE( b, k, tgt, fn) \
    if(block && key && !strcmp(block, b) && !strcmp(key, k)) { \
        if((tgt = fn(value)) == PP_INVALID_CODE) \
            return CFG_INVALID_CODE; \
    }
/*------------------------------------------------------------------------------
    Implement reading code parameter; Code parsing is handled by supplied
    function fn.
------------------------------------------------------------------------------*/
#define IMPLEMENT_PARAM_CODE_COND( b, k, tgt, fn, cond) \
    if((cond) && block && key && !strcmp(block, b) && !strcmp(key, k)) { \
        if((tgt = fn(value)) == PP_INVALID_CODE) \
            return CFG_INVALID_CODE; \
    }
/*------------------------------------------------------------------------------
    Implement structure allocation upon parsing a specific block.
------------------------------------------------------------------------------*/
#define IMPLEMENT_ALLOC_BLOCK( b, tgt, type ) \
    if(key && !strcmp(key, b)) { \
        register type ** t = (type **)&tgt; \
        if(!*t) { \
            *t = (type *)malloc(sizeof(type)); \
            ASSERT(*t); \
            memset(*t, 0, sizeof(type)); \
        } else return CFG_DUPLICATE_BLOCK; \
    }
/*------------------------------------------------------------------------------
    Implement structure allocation upon parsing a specific key.
------------------------------------------------------------------------------*/
#define IMPLEMENT_ALLOC_KEY( b, k, tgt, type ) \
    if(block && key && !strcmp(block, b) && !strcmp(key, k)) { \
        register type ** t = (type **)&tgt; \
        if(!*t) { \
            *t = (type *)malloc(sizeof(type)); \
            ASSERT(*t); \
            memset(*t, 0, sizeof(type)); \
        } else return CFG_DUPLICATE_BLOCK; \
    }

/*------------------------------------------------------------------------------
    Implement trace file parameter; allocating resources upon encountering
    the block
------------------------------------------------------------------------------*/
/*#define IMPLEMENT_TRACE_FILE( b, k, tgt ) \
    IMPLEMENT_ALLOC_KEY(b,k,tgt,TraceFile); \
    IMPLEMENT_PARAM_STRING(b,k,tgt->filename);*/

/*------------------------------------------------------------------------------
    Implement code parsing for given code.
------------------------------------------------------------------------------*/
#define DECLARE_CODE( c ) \
    if(!strcmp(value, #c)) return c;

/*------------------------------------------------------------------------------
    Implement code parsing for given code.
------------------------------------------------------------------------------*/
#define DECLARE_CODE_2( c, v ) \
    if(!strcmp(value, c)) return v;

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ParseAlphaSource

        Functional description:
          Parse alpha source code.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
AlphaSource ParseAlphaSource(char *value) {
  DECLARE_CODE_2("Sequential", ALPHA_SEQUENTIAL);
  DECLARE_CODE_2("Luma", ALPHA_CHANNEL_0);
  DECLARE_CODE_2("Cb", ALPHA_CHANNEL_1);
  DECLARE_CODE_2("Cr", ALPHA_CHANNEL_2);
  DECLARE_CODE_2("Red", ALPHA_CHANNEL_0);
  DECLARE_CODE_2("Green", ALPHA_CHANNEL_1);
  DECLARE_CODE_2("Blue", ALPHA_CHANNEL_2);
  DECLARE_CODE_2("Alpha", ALPHA_CHANNEL_3);
  if(!strncmp(value, "Constant(", 9)) {
    i32 tmp;
    char *endptr;
    tmp = strtol( value+9, &endptr, 10 );
    if(*endptr != ')' ||
        *(endptr+1)) return PP_INVALID_CODE;
    if(tmp < 0 || tmp > 255 )
      return PP_INVALID_CODE;
    return (AlphaSource)((u32)ALPHA_CONSTANT+tmp);
  }
  return PP_INVALID_CODE;
}


/*------------------------------------------------------------------------------

   <++>.<++>  Function: ParsePixelFormat

        Functional description:
          Parse pixel format code.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
PixelFormat ParsePixelFormat(char *value) {
  DECLARE_CODE(YUV420);
  DECLARE_CODE(YUV400);
  DECLARE_CODE(YUV422);
  DECLARE_CODE(YUYV422);
  DECLARE_CODE(YVYU422);
  DECLARE_CODE(UYVY422);
  DECLARE_CODE(VYUY422);
  DECLARE_CODE(YUV444);
  DECLARE_CODE(YUV420C);
  DECLARE_CODE(YUV422C);
  DECLARE_CODE(YUV444C);
  DECLARE_CODE(YUV422CR);
  DECLARE_CODE(YUV411C);
  DECLARE_CODE(YUYV422T4);
  DECLARE_CODE(YVYU422T4);
  DECLARE_CODE(UYVY422T4);
  DECLARE_CODE(VYUY422T4);
  DECLARE_CODE(AYCBCR);
  DECLARE_CODE(RGB);
  DECLARE_CODE(RGBP);
  return PP_INVALID_CODE;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ParseRotation

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
u32 ParseRotation(char *value) {
  DECLARE_CODE(-90);
  DECLARE_CODE(0);
  DECLARE_CODE(90);
  DECLARE_CODE(180);
  DECLARE_CODE_2("HorizontalFlip", 1);
  DECLARE_CODE_2("VerticalFlip", 2);
  return PP_INVALID_CODE;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ParseTraceMode

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
#if !defined(PP_EVALUATION_VERSION)
u32 ParseInputTraceMode(char *value) {
  DECLARE_CODE_2("semiplanar", 1);
  DECLARE_CODE_2("tiled", 2);
  return PP_INVALID_CODE;
}
#endif /* !defined(PP_EVALUATION_VERSION) */

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ReadParam

        Functional description:
          Read parameter callback function.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
CfgCallbackResult ReadParam( char * block, char * key, char * value,
                             CfgCallbackParam state, void **cb_param ) {
  PpParams * pp = (PpParams *)(*cb_param);
  u32 i, j;
  static u32 idx = -1;

  ASSERT(key);

  pp += idx;

  switch(state) {
  case CFG_CALLBACK_BLK_START:
    IMPLEMENT_ALLOC_BLOCK( BLOCK_P_I_P, pp->pip, PixelData );
    IMPLEMENT_ALLOC_BLOCK( BLOCK_ZOOM, pp->zoom, PixelData );
    IMPLEMENT_ALLOC_BLOCK( BLOCK_MASK, pp->mask[pp->mask_count++], Area );
#if !defined(PP_EVALUATION_VERSION)
    IMPLEMENT_ALLOC_BLOCK( BLOCK_FIR_FILTER, pp->fir_filter, FilterData );
#endif /* !defined(PP_EVALUATION_VERSION) */
    if (key && !strcmp(key, BLOCK_PP_PARAMS)) {
      idx++;
      *cb_param = realloc(*cb_param, sizeof(PpParams)*(idx+1));
      pp = (PpParams*)(*cb_param);
      InitBlock(pp+idx);
    }
    if (key && !strcmp(key, BLOCK_DEINTERLACING))
      pp->deint.enable = 1;
    break;

  case CFG_CALLBACK_VALUE:
    /* Data under block pp params */
    IMPLEMENT_PARAM_CODE( BLOCK_PP_PARAMS, KEY_ROTATION, pp->rotation, ParseRotation );
#if !defined(PP_EVALUATION_VERSION)
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_PIPELINE, pp->pipeline );
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_FILTER_ENABLED, pp->filter_enabled );
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_JPEG, pp->jpeg );
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_CROP_8_RIGHT, pp->crop8_right );
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_CROP_8_DOWN, pp->crop8_down );
#endif /* !defined(PP_EVALUATION_VERSION) */
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_FRAMES, pp->frames );
    IMPLEMENT_PARAM_INTEGER( BLOCK_PP_PARAMS, KEY_FIRST_FRAME, pp->first_frame );
    /* Data under sub-block 'Input' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_INPUT, KEY_WIDTH, pp->input.width );
    IMPLEMENT_PARAM_INTEGER( BLOCK_INPUT, KEY_HEIGHT, pp->input.height );

    IMPLEMENT_PARAM_INTEGER( BLOCK_INPUT, KEY_HEIGHT, pp->input.height );
    IMPLEMENT_PARAM_CODE( BLOCK_INPUT, KEY_FORMAT, pp->input_format, ParsePixelFormat );
    IMPLEMENT_PARAM_INTEGER( BLOCK_INPUT, KEY_STRUCT, pp->input_struct );
#if !defined(PP_EVALUATION_VERSION)
    IMPLEMENT_PARAM_CODE( BLOCK_INPUT, KEY_TRACE, pp->input_trace_mode, ParseInputTraceMode );
#endif /* !defined(PP_EVALUATION_VERSION) */
    IMPLEMENT_PARAM_STRING( BLOCK_INPUT, KEY_SEQUENCE, pp->input_file );
    IMPLEMENT_PARAM_INTEGER( BLOCK_INPUT, KEY_WORD_WIDTH, pp->byte_fmt_input.word_width );
    if(!strcmp(block, BLOCK_INPUT)) {
      /* Parse ByteOrder string */
      if(!strcmp(key, KEY_BYTE_ORDER)) {
        pp->byte_fmt_input.bytes =
          sscanf(value,"%d-%d-%d-%d-%d-%d-%d-%d",
                 &pp->byte_fmt_input.byte_order[0], &pp->byte_fmt_input.byte_order[1],
                 &pp->byte_fmt_input.byte_order[2], &pp->byte_fmt_input.byte_order[3],
                 &pp->byte_fmt_input.byte_order[4], &pp->byte_fmt_input.byte_order[5],
                 &pp->byte_fmt_input.byte_order[6], &pp->byte_fmt_input.byte_order[7] );
        /* Check domain and duplicates */
        for( i = 0 ; i < pp->byte_fmt_input.bytes ; ++i ) {
          if(pp->byte_fmt_input.byte_order[i] < 0 ||
              pp->byte_fmt_input.byte_order[i] >= pp->byte_fmt_input.bytes ) {
            fprintf(stderr, "invalid byte index in ByteOrder: %d\n",
                    pp->byte_fmt_input.byte_order[i] );
            return CFG_INVALID_VALUE;
          }
          for( j = 0 ; j < pp->byte_fmt_input.bytes ; ++j ) {
            if( i == j ) continue;
            if(pp->byte_fmt_input.byte_order[i] == pp->byte_fmt_input.byte_order[j]) {
              fprintf(stderr,
                      "duplicate byte index specified in ByteOrder: %d\n",
                      pp->byte_fmt_input.byte_order[i] );
              return CFG_INVALID_VALUE;
            }
          }
        }
      }
    }
    /* Data under sub-block 'Output' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_OUTPUT, KEY_WIDTH, pp->output.width );
    IMPLEMENT_PARAM_INTEGER( BLOCK_OUTPUT, KEY_HEIGHT, pp->output.height );
    IMPLEMENT_PARAM_INTEGER( BLOCK_OUTPUT, KEY_FIELD_OUTPUT, pp->output.field_output );
    IMPLEMENT_PARAM_INTEGER( BLOCK_OUTPUT, KEY_DITHER_ENA, pp->output.dithering_ena );
    IMPLEMENT_PARAM_INTEGER( BLOCK_OUTPUT, KEY_WORD_WIDTH, pp->byte_fmt.word_width );
    IMPLEMENT_PARAM_CODE( BLOCK_OUTPUT, KEY_FORMAT, pp->output_format, ParsePixelFormat);
    IMPLEMENT_PARAM_STRING( BLOCK_OUTPUT, KEY_SEQUENCE, pp->output_file );
    if(!strcmp(block, BLOCK_OUTPUT)) {
      /* Parse ByteOrder string */
      if(!strcmp(key, KEY_BYTE_ORDER)) {
        pp->byte_fmt.bytes =
          sscanf(value,"%d-%d-%d-%d-%d-%d-%d-%d",
                 &pp->byte_fmt.byte_order[0], &pp->byte_fmt.byte_order[1],
                 &pp->byte_fmt.byte_order[2], &pp->byte_fmt.byte_order[3],
                 &pp->byte_fmt.byte_order[4], &pp->byte_fmt.byte_order[5],
                 &pp->byte_fmt.byte_order[6], &pp->byte_fmt.byte_order[7] );
        /* Check domain and duplicates */
        for( i = 0 ; i < pp->byte_fmt.bytes ; ++i ) {
          if(pp->byte_fmt.byte_order[i] < 0 ||
              pp->byte_fmt.byte_order[i] >= pp->byte_fmt.bytes ) {
            fprintf(stderr, "invalid byte index in ByteOrder: %d\n",
                    pp->byte_fmt.byte_order[i] );
            return CFG_INVALID_VALUE;
          }
          for( j = 0 ; j < pp->byte_fmt.bytes ; ++j ) {
            if( i == j ) continue;
            if(pp->byte_fmt.byte_order[i] == pp->byte_fmt.byte_order[j]) {
              fprintf(stderr,
                      "duplicate byte index specified in ByteOrder: %d\n",
                      pp->byte_fmt.byte_order[i] );
              return CFG_INVALID_VALUE;
            }
          }
        }
      }
      /* Parse RgbFormat string */
      else if(!strcmp(key, KEY_RGB_FORMAT)) {
        if(sscanf(value,"%d-%d-%d-%d-%d-%d-%d-%d-%d",
                  &pp->out_rgb_fmt.pad[0], &pp->out_rgb_fmt.chan[0],
                  &pp->out_rgb_fmt.pad[1], &pp->out_rgb_fmt.chan[1],
                  &pp->out_rgb_fmt.pad[2], &pp->out_rgb_fmt.chan[2],
                  &pp->out_rgb_fmt.pad[3], &pp->out_rgb_fmt.chan[3],
                  &pp->out_rgb_fmt.pad[4] ) != 9) {
          /* DROP THIS CODE WHEN OK */
          if(sscanf(value,"%d-%d-%d-%d-%d-%d-%d",
                    &pp->out_rgb_fmt.pad[0], &pp->out_rgb_fmt.chan[0],
                    &pp->out_rgb_fmt.pad[1], &pp->out_rgb_fmt.chan[1],
                    &pp->out_rgb_fmt.pad[2], &pp->out_rgb_fmt.chan[2],
                    &pp->out_rgb_fmt.pad[3] ) != 7) {
            return CFG_INVALID_VALUE;
          }
          fprintf(stderr, "WARNING! RgbFormat lacks alpha channel. You should add it...\n");
          pp->out_rgb_fmt.chan[3] = 0;
          pp->out_rgb_fmt.pad[4] = 0;
          if((pp->out_rgb_fmt.bytes =
                ( pp->out_rgb_fmt.pad[0] + pp->out_rgb_fmt.chan[0] +
                  pp->out_rgb_fmt.pad[1] + pp->out_rgb_fmt.chan[1] +
                  pp->out_rgb_fmt.pad[2] + pp->out_rgb_fmt.chan[2] +
                  pp->out_rgb_fmt.pad[3] + 7 ) >> 3 ) > PP_MAX_RGB_BYTES ) {
            fprintf(stderr, "RgbFormat specifies over %d bytes per pel.\n",
                    PP_MAX_RGB_BYTES );
            return CFG_INVALID_VALUE;
          }
          /*return CFG_INVALID_VALUE;*/
        }
        if((pp->out_rgb_fmt.bytes =
              ( pp->out_rgb_fmt.pad[0] + pp->out_rgb_fmt.chan[0] +
                pp->out_rgb_fmt.pad[1] + pp->out_rgb_fmt.chan[1] +
                pp->out_rgb_fmt.pad[2] + pp->out_rgb_fmt.chan[2] +
                pp->out_rgb_fmt.pad[3] + pp->out_rgb_fmt.chan[3] +
                pp->out_rgb_fmt.pad[4] + 7 ) >> 3 ) > PP_MAX_RGB_BYTES ) {
          fprintf(stderr, "RgbFormat specifies over %d bytes per pel.\n",
                  PP_MAX_RGB_BYTES );
          return CFG_INVALID_VALUE;
        }
      }
      /* Parse RgbOrder string */
      else if(!strcmp(key, KEY_RGB_ORDER)) {
        /* Length must be exactly channel amount (incl.alpha) */
        switch(strlen(value)) {
        case PP_MAX_CHANNELS - 1: /* no alpha */
          pp->out_rgb_fmt.order[3] = 3;
          fprintf(stderr, "WARNING! RgbOrder lacks alpha channel. You should add it...\n");
        /* Fall through */
        case PP_MAX_CHANNELS:
          for( i = 0 ; i < strlen(value) ; ++i ) {
            switch(value[i]) {
            case 'R':
              pp->out_rgb_fmt.order[i] = 0;
              break;
            case 'G':
              pp->out_rgb_fmt.order[i] = 1;
              break;
            case 'B':
              pp->out_rgb_fmt.order[i] = 2;
              break;
            case 'A':
              pp->out_rgb_fmt.order[i] = 3;
              break;
            default:
              return CFG_INVALID_VALUE;
            }
          }
          break;
        default:
          return CFG_INVALID_VALUE;
        }
        /* Cross-check for duplicates */
        if( pp->out_rgb_fmt.order[0] == pp->out_rgb_fmt.order[1] ||
            pp->out_rgb_fmt.order[1] == pp->out_rgb_fmt.order[2] ||
            pp->out_rgb_fmt.order[2] == pp->out_rgb_fmt.order[3] ||
            pp->out_rgb_fmt.order[0] == pp->out_rgb_fmt.order[2] ||
            pp->out_rgb_fmt.order[1] == pp->out_rgb_fmt.order[3] ||
            pp->out_rgb_fmt.order[0] == pp->out_rgb_fmt.order[3] ) {
          return CFG_INVALID_VALUE;
        }
      }
    }

    /* Data under sub-block 'PIP' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_P_I_P, KEY_X_ORIGIN, pp->pipx0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_P_I_P, KEY_Y_ORIGIN, pp->pipy0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_P_I_P, KEY_WIDTH, pp->pip->width );
    IMPLEMENT_PARAM_INTEGER( BLOCK_P_I_P, KEY_HEIGHT, pp->pip->height );
    IMPLEMENT_PARAM_CODE( BLOCK_P_I_P, KEY_FORMAT, pp->pip->fmt, ParsePixelFormat );
    IMPLEMENT_PARAM_STRING( BLOCK_P_I_P, KEY_BACKGROUND, pp->pip_background );
    /* Data under sub-block 'Zoom' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_ZOOM, KEY_X_ORIGIN, pp->zoomx0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_ZOOM, KEY_Y_ORIGIN, pp->zoomy0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_ZOOM, KEY_WIDTH, pp->zoom->width );
    IMPLEMENT_PARAM_INTEGER( BLOCK_ZOOM, KEY_HEIGHT, pp->zoom->height );
    /* Data under sub-block 'Masks' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_MASK, KEY_LEFT, pp->mask[pp->mask_count-1]->x0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_MASK, KEY_TOP, pp->mask[pp->mask_count-1]->y0 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_MASK, KEY_RIGHT, pp->mask[pp->mask_count-1]->x1 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_MASK, KEY_BOTTOM, pp->mask[pp->mask_count-1]->y1 );
    /* Data under sub-block 'Overlay' */
    IMPLEMENT_PARAM_STRING_COND( BLOCK_OVERLAY, KEY_SOURCE,
                                 pp->mask[pp->mask_count-1]->overlay.file_name, pp->mask_count);
    IMPLEMENT_PARAM_CODE_COND( BLOCK_OVERLAY, KEY_FORMAT,
                               pp->mask[pp->mask_count-1]->overlay.pixel_format,
                               ParsePixelFormat, pp->mask_count);
    IMPLEMENT_PARAM_CODE_COND( BLOCK_OVERLAY, KEY_ALPHA_SOURCE,
                               pp->mask[pp->mask_count-1]->overlay.alpha_source,
                               ParseAlphaSource, pp->mask_count);
    IMPLEMENT_PARAM_INTEGER_COND( BLOCK_OVERLAY, KEY_X_ORIGIN,
                                  pp->mask[pp->mask_count-1]->overlay.x0, pp->mask_count);
    IMPLEMENT_PARAM_INTEGER_COND( BLOCK_OVERLAY, KEY_Y_ORIGIN,
                                  pp->mask[pp->mask_count-1]->overlay.y0, pp->mask_count);
    IMPLEMENT_PARAM_INTEGER_COND( BLOCK_OVERLAY, KEY_WIDTH,
                                  pp->mask[pp->mask_count-1]->overlay.data.width, pp->mask_count);
    IMPLEMENT_PARAM_INTEGER_COND( BLOCK_OVERLAY, KEY_HEIGHT,
                                  pp->mask[pp->mask_count-1]->overlay.data.height, pp->mask_count);
    /* Data under sub-block 'ColorConversion' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_VIDEO_RANGE, pp->cnv.video_range );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_OFFSET1, pp->cnv.off1 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_OFFSET2, pp->cnv.off2 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_THRESHOLD1, pp->cnv.thr1 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_THRESHOLD2, pp->cnv.thr2 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_A, pp->cnv.a );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_A1, pp->cnv.a1 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_A2, pp->cnv.a2 );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_B, pp->cnv.b );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_C, pp->cnv.c );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_D, pp->cnv.d );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_E, pp->cnv.e );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_F, pp->cnv.f );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_BRIGHTNESS, pp->cnv.brightness );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_CONTRAST, pp->cnv.contrast );
    IMPLEMENT_PARAM_INTEGER( BLOCK_COLOR_CONVERSION, KEY_SATURATION, pp->cnv.saturation );
    /* Data under sub-block 'FirFilter' */
#if !defined(PP_EVALUATION_VERSION)
    if(!strcmp(block, BLOCK_FIR_FILTER)) {
      /* Parse coefficients */
      if(!strcmp(key, KEY_COEFFICIENTS)) {
        char *endptr = value;
        while( *endptr && pp->fir_filter->size < PP_MAX_FILTER_SIZE ) {
          pp->fir_filter->coeffs[pp->fir_filter->size++]
            = strtol( endptr, &endptr, 10 );
          while(*endptr == ' ') endptr++;
        }
        if(pp->fir_filter->size != 3*3 &&
            pp->fir_filter->size != 5*5 &&
            pp->fir_filter->size != 7*7 &&
            pp->fir_filter->size != 9*9 &&
            pp->fir_filter->size != 11*11 &&
            pp->fir_filter->size != 13*13 &&
            pp->fir_filter->size != 15*15 ) {
          fprintf(stderr, "Filter mask coefficient amount %d is invalid.\n",
                  pp->fir_filter->size );
          return CFG_INVALID_VALUE;
        }
      } else if (!strcmp(key, KEY_DIVISOR)) {
        char *endptr;
        pp->fir_filter->divisor = strtol( value, &endptr, 10 );
        if(*endptr || !pp->fir_filter->divisor)
          return CFG_INVALID_VALUE;
      }
    }
#endif /* !defined(PP_EVALUATION_VERSION) */
    /* Data under sub-block 'Deinterlacing' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_DEINTERLACING, KEY_BLEND_ENABLE, pp->deint.blend_enable );
    IMPLEMENT_PARAM_INTEGER( BLOCK_DEINTERLACING, KEY_EDGE_DETECT, pp->deint.edge_detect_value );
    IMPLEMENT_PARAM_INTEGER( BLOCK_DEINTERLACING, KEY_THRESHOLD, pp->deint.threshold );
    /* Data under sub-block 'RangeMapping' */
    IMPLEMENT_PARAM_INTEGER( BLOCK_RANGE_MAPPING, KEY_ENABLE_Y, pp->range.enable_y );
    IMPLEMENT_PARAM_INTEGER( BLOCK_RANGE_MAPPING, KEY_COEFF_Y, pp->range.coeff_y );
    IMPLEMENT_PARAM_INTEGER( BLOCK_RANGE_MAPPING, KEY_ENABLE_C, pp->range.enable_c );
    IMPLEMENT_PARAM_INTEGER( BLOCK_RANGE_MAPPING, KEY_COEFF_C, pp->range.coeff_c );
    break;

  }
  return CFG_OK;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: InitBlock

        Functional description:

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void InitBlock(PpParams *pp) {

  memset(pp, 0, sizeof(PpParams));

  /* Set default values */
  pp->byte_fmt_input.word_width =
    pp->byte_fmt_input.bytes = pp->byte_fmt.word_width = pp->byte_fmt.bytes = 8;

  pp->byte_fmt.byte_order[0] = 0;
  pp->byte_fmt.byte_order[1] = 1;
  pp->byte_fmt.byte_order[2] = 2;
  pp->byte_fmt.byte_order[3] = 3;
  pp->byte_fmt.byte_order[4] = 4;
  pp->byte_fmt.byte_order[5] = 5;
  pp->byte_fmt.byte_order[6] = 6;
  pp->byte_fmt.byte_order[7] = 7;
  pp->byte_fmt_input.byte_order[0] = 0;
  pp->byte_fmt_input.byte_order[1] = 1;
  pp->byte_fmt_input.byte_order[2] = 2;
  pp->byte_fmt_input.byte_order[3] = 3;
  pp->byte_fmt_input.byte_order[4] = 4;
  pp->byte_fmt_input.byte_order[5] = 5;
  pp->byte_fmt_input.byte_order[6] = 6;
  pp->byte_fmt_input.byte_order[7] = 7;
  pp->std_byte_fmt.word_width = pp->std_byte_fmt.bytes = 1;
  pp->std_byte_fmt.byte_order[0] = 0;

  /* Default RgbOrder = ARGB */
  pp->out_rgb_fmt.order[0] = 3;
  pp->out_rgb_fmt.order[1] = 0;
  pp->out_rgb_fmt.order[2] = 1;
  pp->out_rgb_fmt.order[3] = 2;

  /* Init the following color conversion parameters to INT_MAX, so that we
   * may later determine whether they were specified in the configuration
   * file. */
  pp->cnv.brightness = pp->cnv.contrast = pp->cnv.saturation =
      pp->cnv.a = pp->cnv.b = pp->cnv.c = pp->cnv.d = pp->cnv.e = INT_MAX;

}
