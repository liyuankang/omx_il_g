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
* File name: vlc.c
* Function: VLC support functions
*
*************************************************************************************
*/

#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "commonVariables.h"
#include "avs2_vlc.h"
#include "header.h"
#include "sw_util.h"

// A little trick to avoid those horrible #if TRACE all over the source code
#if TRACE
#define SYMTRACESTRING(s) strncpy(sym->tracestring, s, TRACESTRING_SIZE)
#else
#define SYMTRACESTRING(s)  // do nothing
#endif

extern void tracebits(const char *trace_str, int len, int info, int value1);
static int readSyntaxElement_VLC(struct StrmData *curStream,
                                 SyntaxElement *sym);
static int GetVLCSymbol(struct StrmData *curStream, int *info);

static int readSyntaxElement_FLC(struct StrmData *curStream,
                                 SyntaxElement *sym);

/*
*************************************************************************
* Function:ue_v, reads an ue(v) syntax element, the length in bits is stored in
the global UsedBits variable
* Input:
tracestring
the string for the trace file
bitstream
the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int ue_v(struct StrmData *curStream, char *tracestring) {
  SyntaxElement symbol, *sym = &symbol;

  assert(curStream->strm_buff_start != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_ue;  // Mapping rule
  SYMTRACESTRING(tracestring);
  readSyntaxElement_VLC(curStream, sym);

  return sym->value1;
}

/*
*************************************************************************
* Function:ue_v, reads an se(v) syntax element, the length in bits is stored in
the global UsedBits variable
* Input:
tracestring
the string for the trace file
bitstream
the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int se_v(struct StrmData *curStream, char *tracestring) {
  SyntaxElement symbol, *sym = &symbol;

  assert(curStream->strm_buff_start != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_se;  // Mapping rule: signed integer
  SYMTRACESTRING(tracestring);
  readSyntaxElement_VLC(curStream, sym);

  return sym->value1;
}

/*
*************************************************************************
* Function:ue_v, reads an u(v) syntax element, the length in bits is stored in
the global UsedBits variable
* Input:
tracestring
the string for the trace file
bitstream
the stream to be read from
* Output:
* Return: the value of the coded syntax element
* Attention:
*************************************************************************
*/

int u_v(struct StrmData *curStream, int LenInBits, char *tracestring) {
  SyntaxElement symbol, *sym = &symbol;

  assert(curStream->strm_buff_start != NULL);
  sym->type = SE_HEADER;
  sym->mapping = linfo_ue;  // Mapping rule
  sym->len = LenInBits;
  SYMTRACESTRING(tracestring);
  readSyntaxElement_FLC(curStream, sym);

  return sym->inf;
}

/*
*************************************************************************
* Function:mapping rule for ue(v) syntax elements
* Input:lenght and info
* Output:number in the code table
* Return:
* Attention:
*************************************************************************
*/

void linfo_ue(int len, int info, int *value1, int *dummy) {
  //*value1 = (int) pow(2, (len / 2)) + info - 1;        // *value1 =
  //(int)(2<<(len>>1))+info-1;
  *value1 = (int)(1 << (len >> 1)) + info - 1;
}

/*
*************************************************************************
* Function:mapping rule for se(v) syntax elements
* Input:lenght and info
* Output:signed mvd
* Return:
* Attention:
*************************************************************************
*/

void linfo_se(int len, int info, int *value1, int *dummy) {
  int n;
  // n = (int) pow(2, (len / 2)) + info - 1;
  n = (int)(1 << (len >> 1)) + info - 1;
  *value1 = (n + 1) / 2;

  if ((n & 0x01) == 0) {  // lsb is signed bit
    *value1 = -*value1;
  }
}

/*
*************************************************************************
* Function:read next UVLC codeword from UVLC-partition and
map it to the corresponding syntax element
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

static int readSyntaxElement_VLC(struct StrmData *curStream,
                                 SyntaxElement *sym) {
  sym->len = GetVLCSymbol(curStream, &(sym->inf));

  if (sym->len == -1) {
    return -1;
  }
  sym->mapping(sym->len, sym->inf, &(sym->value1), &(sym->value2));

#if TRACE
  tracebits(sym->tracestring, sym->len, sym->inf, sym->value1);
#endif

  return 1;
}

static int GetVLCSymbol(struct StrmData *curStream, int *info) {

  register int inf = 0;
  int ctr_bit = 0;  // control bit for current bit posision
  int bitcounter = 1;
  int len = 0;
  int info_bit = 0;

  ctr_bit = SwGetBits(curStream, 1);
  if (ctr_bit == END_OF_STREAM) return -1;

  len = 1;

  while (ctr_bit == 0) {
    // find leading 1 bit
    len++;
    bitcounter++;
    // bitoffset -= 1;
    ctr_bit = SwGetBits(curStream, 1);
    if (ctr_bit == END_OF_STREAM) return -1;
  }

  // make infoword
  inf = 0;  // shortest possible code is 1, then info is always 0

  for (info_bit = 0; (info_bit < (len - 1)); info_bit++) {
    bitcounter++;

    inf = (inf << 1);

    ctr_bit = SwGetBits(curStream, 1);
    if (ctr_bit == END_OF_STREAM) return -1;

    if (ctr_bit) {
      inf |= 1;
    }
  }

  *info = inf;

  return bitcounter;  // return absolute offset in bit from start of frame
}

/*
*************************************************************************
* Function:read FLC codeword from UVLC-partition
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

static int readSyntaxElement_FLC(struct StrmData *curStream,
                                 SyntaxElement *sym) {
  int high16 = 0, low16 = 0;
  if (sym->len >= 32) {
    high16 = SwGetBits(curStream, 16);
    low16 = SwGetBits(curStream, sym->len - 16);
    sym->inf = (high16 << (sym->len - 16)) | low16;
  } else
    sym->inf = SwGetBits(curStream, sym->len);

  sym->value1 = sym->inf;

#if TRACE
  tracebits2(sym->tracestring, sym->len, sym->inf);
#endif
  if (sym->inf == END_OF_STREAM) return -1;

  return 1;
}

#if TRACE

#define TRACEFILE "trace_dec.txt"

static void trace_error(char *text, int code) {
  fprintf(stderr, "%s\n", text);
  exit(code);
}

struct syntax_ctx *trace_get_ctx() {
  static struct syntax_ctx ctx = {NULL, 0, ""};

  if (ctx.p_trace == NULL) {
    ctx.p_trace = fopen(TRACEFILE, "w");
    if (ctx.p_trace == NULL) {  // append new statistic at the end
      snprintf(ctx.errortext, ET_SIZE, "Error open file %s!", TRACEFILE);
      trace_error(ctx.errortext, 500);
    }
  }
  return &ctx;
}

void trace_close() {
  struct syntax_ctx *ctx = trace_get_ctx();
  fclose(ctx->p_trace);
  ctx->p_trace = NULL;
  ctx->bitcounter = 0;
}

/**
 * Function:Tracing bitpatterns for symbols
 *  A code word has the following format: 0 Xn...0 X2 0 X1 0 X0 1
 * Input:
 * Output:
 * Return:
 * Attention:
 */
void tracebits(const char *trace_str,  //!< tracing information, char array
                                       //describing the symbol
               int len,                //!< length of syntax element in bits
               int info,               //!< infoword of syntax element
               int value1) {
  struct syntax_ctx *ctx = trace_get_ctx();
  int i, chars;

  if (len >= 34) {
    snprintf(ctx->errortext, ET_SIZE,
             "Length argument to put too long for trace to work");
    trace_error(ctx->errortext, 600);
  }

  putc('@', ctx->p_trace);
  chars = fprintf(ctx->p_trace, "%i", ctx->bitcounter);

  while (chars++ < 6) {
    putc(' ', ctx->p_trace);
  }

  chars += fprintf(ctx->p_trace, "%s", trace_str);

  while (chars++ < 55) {
    putc(' ', ctx->p_trace);
  }

  // Align bitpattern
  if (len < 15) {
    for (i = 0; i < 15 - len; i++) {
      fputc(' ', ctx->p_trace);
    }
  }

  // Print bitpattern
  for (i = 0; i < len / 2; i++) {
    fputc('0', ctx->p_trace);
  }

  // put 1
  fprintf(ctx->p_trace, "1");

  // Print bitpattern
  for (i = 0; i < len / 2; i++) {
    if (0x01 & (info >> ((len / 2 - i) - 1))) {
      fputc('1', ctx->p_trace);
    } else {
      fputc('0', ctx->p_trace);
    }
  }

  fprintf(ctx->p_trace, "  (%3d)\n", value1);
  ctx->bitcounter += len;

  fflush(ctx->p_trace);
}

/**
 * Function:Tracing bitpatterns
 */
void tracebits2(const char *trace_str,  //!< tracing information, char array
                                        //describing the symbol
                int len,                //!< length of syntax element in bits
                int info) {
  struct syntax_ctx *ctx = trace_get_ctx();
  int i, chars;

  if (len >= 45) {
    snprintf(ctx->errortext, ET_SIZE,
             "Length argument to put too long for trace to work");
    trace_error(ctx->errortext, 600);
  }

  putc('@', ctx->p_trace);
  chars = fprintf(ctx->p_trace, "%i", ctx->bitcounter);

  while (chars++ < 6) {
    putc(' ', ctx->p_trace);
  }

  chars += fprintf(ctx->p_trace, "%s", trace_str);

  while (chars++ < 55) {
    putc(' ', ctx->p_trace);
  }

  // Align bitpattern
  if (len < 15)
    for (i = 0; i < 15 - len; i++) {
      fputc(' ', ctx->p_trace);
    }

  ctx->bitcounter += len;

  while (len >= 32) {
    for (i = 0; i < 8; i++) {
      fputc('0', ctx->p_trace);
    }

    len -= 8;
  }

  // Print bitpattern
  for (i = 0; i < len; i++) {
    if (0x01 & (info >> (len - i - 1))) {
      fputc('1', ctx->p_trace);
    } else {
      fputc('0', ctx->p_trace);
    }
  }

  fprintf(ctx->p_trace, "  (%3d)\n", info);

  fflush(ctx->p_trace);
}

#endif
