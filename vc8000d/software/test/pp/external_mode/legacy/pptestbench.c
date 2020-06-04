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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __arm__
#include <fcntl.h>
#include <sys/mman.h>
#include "memalloc.h"
#endif
#include "pptestbench.h"

#include "defs.h"
#include "frame.h"
#include "pp.h"
#include "cfg.h"
#include "dwl.h"

#include "ppapi.h"
#include "ppinternal.h"

#ifdef MD5SUM
#include "tb_md5.h"
#endif

#define PP_BLEND_COMPONENT0_FILE "pp_ablend1_in.raw"
#define PP_BLEND_COMPONENT1_FILE "pp_ablend2_in.raw"

typedef struct {
  PpParams *pp;
  PpParams *curr_pp;
  u32 pp_param_idx;
  u32 num_pp_params;
} ppContainer;

static ppContainer pp = { NULL, NULL, 0, 0 };

/* Kernel driver for linear memory allocation of SW/HW shared memories */
const char *memdev = "/tmp/dev/memalloc";
static int memdev_fd = -1;
static int fd_mem = -1;

static u32 in_pic_size = 8 * 2048 * 1536;
addr_t in_pic_ba = 0;
#ifndef __arm__
u32 *in_pic_va = NULL;
#else
u32 *in_pic_va = MAP_FAILED;
#endif

struct DWLLinearMem buffer;
struct DWLLinearMem buffer_out;

FILE *fbddata0 = NULL;       /* blend component 0 file */
FILE *fbddata1 = NULL;       /* blend component 1 file */
char blendfile[2][256] = { "\n", "\n" };

static u32 out_pic_size = (4096 * 4096 * 4 * 2 * 4);
addr_t out_pic_ba = 0;
#ifndef __arm__
u32 *out_pic_va = NULL;
#else
u32 *out_pic_va = MAP_FAILED;
#endif

u32 multibuffer = 0;
u32 force_tiled_input = 0;
u32 planar_to_semiplanar_input = 0;

u32 number_of_buffers = 2;
PPOutputBuffers output_buffer;
u32 *p_multibuffer_va[17];

addr_t display_buff_ba = 0;
u32 *display_buff_va = NULL;
PPOutput last_out;

u32 *interlaced_frame_va = NULL;
#ifdef ASIC_TRACE_SUPPORT
extern u8 *ablend_base[2];
extern u32 ablend_width[2];
extern u32 ablend_height[2];
#endif /* ASIC_TRACE_SUPPORT */

static u32 blend_size[2] = { 0, 0 };
addr_t blend_ba[2] = { 0, 0 };
#ifndef __arm__
u32 *blend_va[2] = { NULL, NULL };
#else
u32 *blend_va[2] = { MAP_FAILED, MAP_FAILED };
#endif
u32 *blend_tmp[2] = { NULL, NULL };
u32 *blend_tmp444[2] = { NULL, NULL };
u32 blendpos[2] = { 0, 0 };

u32 second_field = 0;        /* output is second field of a frame */

static PPInst pp_inst = NULL;
static const void *decoder;

static PPConfig pp_conf;

static u32 input_endian_big = 0;
static u32 input_word_swap  = 0;
static u32 output_endian_big = 0;
static u32 output_rgb_swap = 0;
static u32 output_word_swap = 0;
static u32 output_word_swap_16 = 0;
static u32 blend_endian_change = 0;
static u32 output_tiled_4x4 = 0;

static out_pic_pixels;
static decodertype;          /* store type of decoder here */

u32 vc1_multires;             /* multi resolution is use */
u32 vc1_rangered;             /* was picture range reduced */

u32 interlaced = 0;

static int pp_read_input(i32 frame);

static void pp_api_release(PPInst pp);
static void pp_release_buffers(PpParams * pp);

static void ReleasePp(PpParams * pp);
static int WritePicture(u32 * image, i32 size, const char *fname, int frame);
static void MakeTiled(u8 * data, u32 w, u32 h);
static void MakeTiled8x4(u8 * data, u32 w, u32 h, u32);
static void Tiled4x4ToPlanar(u8 * data, u32 w, u32 h);

static void swap_in_out_pointers(u32 width, u32 height);
static void reset_vc1_specific(void);

static int read_mask_file(u32 id, u32);

static int pp_alloc_blend_components(PpParams * pp, u32 ablend_crop);

static void toggle_endian_yuv(u32 width, u32 height, u32 * virtual);

static void Yuv2Rgb(u32 * p, u32 w, u32 h, ColorConversion * cc);

static int pp_load_cfg_file(char *cfg_file, ppContainer * pp);
static void pp_print_result(PPResult ret);

static void TBOverridePpContainer(PPContainer *pp_c, const struct TBCfg * tb_cfg);
u32 pp_setup_multibuffer(PPInst pp, const void *dec_inst,
                         u32 dec_type, PpParams * params);
/*------------------------------------------------------------------------------
    Function name   : pp_external_run
    Description     : Runs the post processor in a stand alone mode
    Return type     : int
    Argument        : char *cfg_file, const struct TBCfg* tb_cfg
------------------------------------------------------------------------------*/

#if defined(ASIC_TRACE_SUPPORT) && !defined(PP_VP6DEC_PIPELINE_SUPPORT)
extern u32 hw_pp_pic_count;
extern u32 g_hw_ver;
#endif

#if defined (PP_EVALUATION)
extern u32 g_hw_ver;
#endif

int pp_external_run(char *cfg_file, const struct TBCfg * tb_cfg) {
  int ret = 0, frame, tmp_frame;
  PPResult res;

#if defined(ASIC_TRACE_SUPPORT) && !defined(PP_VP6DEC_PIPELINE_SUPPORT)
  OpenAsicTraceFiles();
  g_hw_ver = 8190;
#endif

#if defined(PP_EVALUATION_8170)
  g_hw_ver = 8170;
#elif defined(PP_EVALUATION_8190)
  g_hw_ver = 8190;
#elif defined(PP_EVALUATION_9170)
  g_hw_ver = 9170;
#elif defined(PP_EVALUATION_9190)
  g_hw_ver = 9190;
#elif defined(PP_EVALUATION_G1)
  g_hw_ver = 10000;
#endif

  if(pp_startup(cfg_file, NULL, PP_PIPELINE_DISABLED, tb_cfg) != 0) {
    ret = 1;
    goto end;
  }

  /* Start process */
  for(frame = 0, tmp_frame = 0;; ++frame, ++tmp_frame) {
    ret = pp_update_config(NULL, PP_PIPELINE_DISABLED, tb_cfg);
    if(ret == CFG_UPDATE_NEW)
      tmp_frame = 0;
    else if(ret == CFG_UPDATE_FAIL)
      goto end;

    if(pp_read_input(tmp_frame) != 0)
      break;
    if(frame) {
      (void) pp_read_blend_components( pp_inst );
    }

    if(pp.curr_pp->input_trace_mode == 2) { /* tiled input */
      MakeTiled((u8 *) in_pic_va, pp.curr_pp->input.width,
                pp.curr_pp->input.height);
    }

    res = PPGetResult(pp_inst);
    pp_print_result(res);

    if(res != PP_OK) {
      printf("ERROR: PPGetResult returned %d\n", res);
      break;
    }
#ifdef PP_WRITE_BMP /* enable for writing BMP headers for rgb images */
    /* For internal debug use only */
    if(pp_conf.pp_out_img.pix_format & 0x040000) {
      u32 w, h;

      w = pp.curr_pp->pip ? pp.curr_pp->pip->width : pp_conf.pp_out_img.width;
      h = pp.curr_pp->pip ? pp.curr_pp->pip->height : pp_conf.pp_out_img.
          height;

      WriteBmp(pp.curr_pp->output_file,
               pp_conf.pp_out_img.pix_format, (u8 *) out_pic_va, w, h);
    } else
#endif
      pp_write_output(frame, 0, 0);

  }

end:

  pp_close();

#if defined(ASIC_TRACE_SUPPORT) && !defined(PP_VP6DEC_PIPELINE_SUPPORT)
  TraceSequenceCtrlPp(hw_pp_pic_count);
  CloseAsicTraceFiles();
#endif

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : pp_close
    Description     : Clean up
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/
void pp_close(void) {
  pp_api_release(pp_inst);
  if(pp.curr_pp)
    pp_release_buffers(pp.curr_pp);
  if(pp.pp)
    free(pp.pp);
}

/*------------------------------------------------------------------------------
    Function name   : pp_read_input
    Description     : Read the input frame, and perform input word swap and
                      endian changes, if needed.
    Return type     : int
    Argument        : int frame
------------------------------------------------------------------------------*/
int pp_read_input(int frame) {
  /* Read input frame and resample it to YUV444 or RGB */
  if(!ReadFrame(pp.curr_pp->input_file, &pp.curr_pp->input,
                pp.curr_pp->first_frame + frame, pp.curr_pp->input_format,
                pp.curr_pp->input_struct == PP_PIC_TOP_AND_BOT_FIELD)) {
    printf("EOF at frame %d\n", pp.curr_pp->first_frame + frame);
    return 1;
  }
#ifndef ASIC_TRACE_SUPPORT
  if(input_word_swap) {
    PixelData *p = &pp.curr_pp->input;
    u32 x, bytes = FrameBytes(p, NULL);

    for(x = 0; x < bytes; x += 8) {
      u32 tmp = 0;
      u32 *word = (u32 *) (p->base + x);
      u32 *word2 = (u32 *) (p->base + x + 4);

      tmp = *word;
      *word = *word2;
      *word2 = tmp;
    }
  }
  if(input_endian_big) {
    PixelData *p = &pp.curr_pp->input;
    u32 x, bytes = FrameBytes(p, NULL);

    for(x = 0; x < bytes; x += 4) {
      u32 tmp = 0;
      u32 *word = (u32 *) (p->base + x);

      tmp |= (*word & 0xFF) << 24;
      tmp |= (*word & 0xFF00) << 8;
      tmp |= (*word & 0xFF0000) >> 8;
      tmp |= (*word & 0xFF000000) >> 24;
      *word = tmp;
    }
  }
#endif

  /* Make conversion to tiled if input mode forced, so as to make
   * a bit more visually pleasing output. Note that this doesn't
   * perform planar-->semiplanar so planar YUV inputs will still
   * have faulty color channels*/
  if(force_tiled_input &&
      (pp.curr_pp->input_format == YUV420C )) {
    MakeTiled8x4( pp.curr_pp->input.base,
                  pp.curr_pp->input.width,
                  pp.curr_pp->input.height,
                  planar_to_semiplanar_input );
  }

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_write_output
    Description     : Write the output frame, and perform output word swap and
                      endian changes, if needed.
    Return type     : void
    Argument        : int frame
------------------------------------------------------------------------------*/

void pp_write_output(int frame, u32 field_output, u32 top) {
  u32 bytes;
  PPOutput tmpout;
  PPResult ret;
  u32 * p_out_va = NULL;

  if(decoder != NULL && multibuffer) {
    /* currently API struct for multibuffers does not contain virtual
     * addresses. loop goes through list of buffers. when match for
     * current out is found, same index used to get virtual for
     * virtual buffer list */
    u32 k;
    ret = PPGetNextOutput(pp_inst, &tmpout);
    ASSERT(ret == PP_OK);

    for(k=0; k<number_of_buffers; k++) {
      if(tmpout.buffer_bus_addr == output_buffer.pp_output_buffers[k].buffer_bus_addr) {
        p_out_va = p_multibuffer_va[k];
        break;
      }
    }

    if(tmpout.buffer_bus_addr == display_buff_ba) {
      /* it's our spare buffer */
      p_out_va = display_buff_va;
    }

    ASSERT(p_out_va != NULL);

    /* do we double buffer for display? */
    if(display_buff_ba) {
      /* for field output swap just after second field is ready */
      if((!field_output) || (field_output && second_field)) {
        /* swap the output with a new buffer */
        ret = PPDecSwapLastOutputBuffer(pp_inst, &tmpout, &last_out);

        /* save current output for next swap */
        last_out = tmpout;
      }
    }

  } else {
    p_out_va = out_pic_va;
  }

  switch (pp.curr_pp->output_format) {
  case YUV420C:
    bytes = (out_pic_pixels * 3) / 2;
    break;
  case YUYV422:
  case YVYU422:
  case UYVY422:
  case VYUY422:
  case YUYV422T4:
  case YVYU422T4:
  case UYVY422T4:
  case VYUY422T4:
    bytes = out_pic_pixels * 2;
    break;
  case RGB:
    bytes = out_pic_pixels * pp.curr_pp->out_rgb_fmt.bytes;

  }

  /* if picture upside down, write fields in inverted order. looks better */
  top = top ? 1 : 0;
  if(pp.curr_pp->rotation == 180 || pp.curr_pp->rotation == -180 ||
      pp.curr_pp->rotation == 1)
    top = 1 - top;


#ifndef ASIC_TRACE_SUPPORT

  if(output_tiled_4x4) {
    Tiled4x4ToPlanar( (u8*)p_out_va,
                      pp_conf.pp_out_img.width,
                      pp_conf.pp_out_img.height );
  }

  if(((PPContainer*)pp_inst)->hw_endian_ver == 0) { /* HWIF_PPD_OEN_VERSION */

    if(output_endian_big) {
      /*printf("output big endian\n"); */
      u32 x;

      for(x = 0; x < bytes; x += 4) {
        u32 tmp = 0;
        u32 *word = (u32 *) (((addr_t) p_out_va) + x);

        tmp |= (*word & 0xFF) << 24;
        tmp |= (*word & 0xFF00) << 8;
        tmp |= (*word & 0xFF0000) >> 8;
        tmp |= (*word & 0xFF000000) >> 24;
        *word = tmp;
      }
    }

    if(output_rgb_swap) {
      u32 x;

      /*printf("output rgb swap\n"); */

      for(x = 0; x < bytes; x += 4) {
        u32 tmp = 0;
        u32 *word = (u32 *) (((addr_t) p_out_va) + x);

        tmp |= (*word & 0xFF) << 8;
        tmp |= (*word & 0xFF00) >> 8;
        tmp |= (*word & 0xFF0000) << 8;
        tmp |= (*word & 0xFF000000) >> 8;
        *word = tmp;
      }
    }

    if(output_word_swap) {
      u32 x;

      /*printf("output word swap\n"); */

      for(x = 0; x < bytes; x += 4 * 2) {
        u32 tmp1 = 0;
        u32 tmp2 = 0;
        u32 *word1 = (u32 *) (((addr_t) p_out_va) + x);
        u32 *word2 = (u32 *) (((addr_t) p_out_va) + x + 4);

        tmp1 = *word1;
        tmp2 = *word2;
        *word1 = tmp2;
        *word2 = tmp1;
      }
    }
  } else { /* HWIF_PPD_OEN_VERSION == 1 */

    if(output_word_swap) {
      u32 x;

      /*printf("output word swap\n"); */

      for(x = 0; x < bytes; x += 4 * 2) {
        u32 tmp1 = 0;
        u32 tmp2 = 0;
        u32 *word1 = (u32 *) (((addr_t) p_out_va) + x);
        u32 *word2 = (u32 *) (((addr_t) p_out_va) + x + 4);

        tmp1 = *word1;
        tmp2 = *word2;
        *word1 = tmp2;
        *word2 = tmp1;
      }
    }

    if(output_word_swap_16) {
      u32 x;

      /*printf("output word swap\n"); */

      for(x = 0; x < bytes; x += 4 ) {
        u32 tmp1 = 0;
        u32 tmp2 = 0;
        u32 *word = (u32 *) (((addr_t) p_out_va) + x);

        tmp1 = ((*word) >> 16) & 0xFFFF;
        tmp2 = (*word) & 0xFFFF;
        *word = (tmp2 << 16) | (tmp1) ;
      }
    }

    if(output_endian_big) {
      /*printf("output big endian\n"); */
      u32 x;

      for(x = 0; x < bytes; x += 4) {
        u32 tmp = 0;
        u32 *word = (u32 *) (((addr_t) p_out_va) + x);

        tmp |= (*word & 0xFF) << 24;
        tmp |= (*word & 0xFF00) << 8;
        tmp |= (*word & 0xFF0000) >> 8;
        tmp |= (*word & 0xFF000000) >> 24;
        *word = tmp;
      }
    }
  }
#endif
  /* combine YUV 420 fields back into frames for testing */
  if(field_output && (pp.curr_pp->output_format == YUV420C)) {
    u32 x;
    u32 start_of_ch;
    u8 *p, *pfi;
    u32 widt = pp.curr_pp->output.width;
    u32 heig = pp.curr_pp->output.height;

    if(pp.curr_pp->pip)
      heig = pp.curr_pp->pip->height;

    if(interlaced_frame_va == NULL)
      interlaced_frame_va = malloc(widt * heig * 3);

    ASSERT(interlaced_frame_va);

    pfi = (u8 *) p_out_va;
    p = (u8 *) interlaced_frame_va;

    if(!top)
      p += widt;

    /* luma */
    for(x = 0; x < heig; x++, p += (widt * 2))
      memcpy(p, pfi + (x * widt), widt);

    start_of_ch = (widt * heig);

    /* chroma */
    for(x = 0; x < heig / 2; x++, p += (widt * 2))
      memcpy(p, pfi + start_of_ch + (x * widt), widt);

    if(second_field) {
      WritePicture(interlaced_frame_va, bytes * 2, pp.curr_pp->output_file,
                   frame);
    }
    second_field ^= 1;
  } else {
    WritePicture(p_out_va, bytes, pp.curr_pp->output_file, frame);

    if(multibuffer)
      memset(p_out_va, 0, bytes);

    /* Reset vc1 picture specific information */
    reset_vc1_specific();
  }

}

/*------------------------------------------------------------------------------
    Function name   : pp_write_output_plain
    Description     : Write output picture without considering endiannness modes.
                      This is useful if the endianness is already handled
                      (not coded picture)

    Return type     : void
    Argument        : int frame
------------------------------------------------------------------------------*/
void pp_write_output_plain(int frame) {
  u32 bytes;

  switch (pp.curr_pp->output_format) {
  case YUV420C:
    bytes = (out_pic_pixels * 3) / 2;
    break;
  case YUYV422:
    bytes = out_pic_pixels * 2;
    break;
  case RGB:
    bytes = out_pic_pixels * pp.curr_pp->out_rgb_fmt.bytes;

  }

  WritePicture(out_pic_va, bytes, pp.curr_pp->output_file, frame);

}

/*------------------------------------------------------------------------------
    Function name   : pp_startup
    Description     : Prepare the post processor according to the post processor
                      configuration file. Allocate input and output picture
                      buffers.
    Return type     : int
    Argument        : char *pp_cfg_file
    Argument        : const void *dec_inst
    Argument        : u32 dec_type
------------------------------------------------------------------------------*/
int pp_startup(char *pp_cfg_file, const void *dec_inst, u32 dec_type,
               const struct TBCfg * tb_cfg) {
  int ret = 0;

  if(pp_load_cfg_file(pp_cfg_file, &pp) != 0) {
    ret = 1;
    goto end;
  }

  printf("---init PP API---\n");

  if(pp_api_init(&pp_inst, dec_inst, dec_type) != 0) {
    printf("\t\tFailed\n");
    ret = 1;
    goto end;
  }

end:

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : pp_alloc_buffers
    Description     : Allocate input and output picture buffers.
    Return type     : int
------------------------------------------------------------------------------*/
int pp_alloc_buffers(PpParams * pp, u32 ablend_crop) {
  int ret = 0;
  int max_dimension = 0;

#ifdef __arm__
  MemallocParams params;

  memdev_fd = open(memdev, O_RDWR);
  if(memdev_fd == -1) {
    printf("Failed to open dev: %s\n", memdev);
    return -1;
  }

  /* open mem device for memory mapping */
  fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
  if(fd_mem == -1) {
    printf("Failed to open: %s\n", "/dev/mem");
    ret = 1;
    goto err;
  }
#endif

  if(pp_alloc_blend_components(pp, ablend_crop)) {
    printf("failed to alloc blend components\n");
    return -1;
  }

#ifndef __arm__
  /* Allocations for sw/sw testing */

#ifndef PP_PIPELINE_ENABLED
  if(in_pic_va == NULL)
    in_pic_va = malloc(in_pic_size);
  ASSERT(in_pic_va != NULL);
#endif

  if(out_pic_va == NULL)
    out_pic_va = malloc(out_pic_size);

  ASSERT(out_pic_va != NULL);

  in_pic_ba = (addr_t) in_pic_va;
  out_pic_ba = (addr_t) out_pic_va;

  /* allocate extra buffer for swapping the multibuffer output */
  if (multibuffer) {
    if(display_buff_va == NULL) {
      u32 s;

      if(pp->pip == NULL) {
        s = pp->output.width * pp->output.height * 4;
      } else {
        s = pp->pip->width * pp->pip->height * 4;
      }

      display_buff_va = (u32 *)malloc(s);
    }

    ASSERT(display_buff_va != NULL);

    display_buff_ba = (addr_t)display_buff_va;
  }

#else
  /* allocation for sw/hw testing on fpga */

  /* if PIP used, alloc based on that, otherwise output size */
  if(pp->pip == NULL) {
    params.size = pp->output.width * pp->output.height * 4;
    max_dimension = MAX(pp->output.width, pp->output.height);
  } else {
    params.size = pp->pip->width * pp->pip->height * 4;
    max_dimension = MAX(pp->pip->width, pp->pip->height);
  }

  /* Allocate buffers */
  if(multibuffer != 0) {
    /* Allocate more memory if running 4K case (must be run with a board
     * with a larger memory config than 128M */
    if(max_dimension > 1920)
      params.size = 4096 * 4096 * 4 * 2;
    else
      /* params.size = 4096*4096; */ /* allocate one big buffer that will be split */
      /* Buffer increased for cases 2215 & 8412 */
      params.size = 1920 * 1920 * 4 * 2;
  }

  out_pic_size = params.size;

  /* Allocate buffer and get the bus address */
  ioctl(memdev_fd, MEMALLOC_IOCXGETBUFFER, &params);

  if(params.bus_address == 0) {
    printf("Output allocation failed\n");
    goto err;

  }
  out_pic_ba = params.bus_address;

#ifndef PP_PIPELINE_ENABLED
  /* input buffer allocation */
  in_pic_size = params.size = pp->input.width * pp->input.height * 2;

  /* Allocate buffer and get the bus address */
  ioctl(memdev_fd, MEMALLOC_IOCXGETBUFFER, &params);

  if(params.bus_address == 0) {
    printf("Input allocation failed\n");
    goto err;
  }
  in_pic_ba = params.bus_address;

  /* Map the bus address to virtual address */
  in_pic_va = MAP_FAILED;
  in_pic_va = (u32 *) mmap(0, in_pic_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd_mem, in_pic_ba);

  if(in_pic_va == MAP_FAILED) {
    printf("Failed to alloc input image\n");
    ret = 1;
    goto err;
  }
#endif

  out_pic_va = MAP_FAILED;
  out_pic_va = (u32 *) mmap(0, out_pic_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd_mem, out_pic_ba);

  if(out_pic_va == MAP_FAILED) {
    printf("Failed to alloc input image\n");
    ret = 1;
    goto err;
  }

  printf("Input buffer %ld bytes       Output buffer %ld bytes\n",
         in_pic_size, out_pic_size);
  printf("Input buffer bus address:\t\t0x%16lx\n", in_pic_ba);
  printf("Output buffer bus address:\t\t0x%16lx\n", out_pic_ba);
  printf("Input buffer user address:\t\t0x%16lx\n",  (addr_t)in_pic_va);
  printf("Output buffer user address:\t\t0x%16lx\n",  (addr_t)out_pic_va);
#endif

  memset(out_pic_va, 0x0, out_pic_size);

err:
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : pp_release_buffers
    Description     : Release input and output picture buffers.
    Return type     : void
------------------------------------------------------------------------------*/
void pp_release_buffers(PpParams * pp) {

#ifndef __arm__

  if(in_pic_va != NULL)
    free(in_pic_va);
  if(out_pic_va != NULL)
    free(out_pic_va);
  if(display_buff_va != NULL)
    free(display_buff_va);

  in_pic_va = NULL;
  out_pic_va = NULL;
  display_buff_va = NULL;

#else
  if(in_pic_va != MAP_FAILED)
    munmap(in_pic_va, in_pic_size);

  in_pic_va = MAP_FAILED;

  if(out_pic_va != MAP_FAILED)
    munmap(out_pic_va, out_pic_size);

  out_pic_va = MAP_FAILED;

  if(in_pic_ba != 0)
    ioctl(memdev_fd, MEMALLOC_IOCSFREEBUFFER, &in_pic_ba);

  in_pic_ba = 0;

  if(out_pic_ba != 0)
    ioctl(memdev_fd, MEMALLOC_IOCSFREEBUFFER, &out_pic_ba);

  out_pic_ba = 0;

  if(memdev_fd != -1)
    close(memdev_fd);

  if(fd_mem != -1)
    close(fd_mem);

  fd_mem = memdev_fd = -1;
#endif

  /* release  blend buffers */
  if(blend_tmp[0]) {
    free(blend_tmp[0]);
    blend_tmp[0] = NULL;
  }
  if(blend_tmp[1]) {
    free(blend_tmp[1]);
    blend_tmp[1] = NULL;
  }
  if(blend_tmp444[0]) {
    free(blend_tmp444[0]);
    blend_tmp444[0] = NULL;
  }
  if(blend_tmp444[1]) {
    free(blend_tmp444[1]);
    blend_tmp444[1] = NULL;
  }

#ifndef __arm__
  if(blend_va[0]) {
    free(blend_va[0]);
    blend_va[0] = NULL;
  }
  if(blend_va[1]) {
    free(blend_va[1]);
    blend_va[1] = NULL;
  }
#else
  if(blend_va[0] != MAP_FAILED) {
    munmap(blend_va[0], blend_size[0]);
    blend_va[0] = MAP_FAILED;
  }
  if(blend_va[1] != MAP_FAILED) {
    munmap(blend_va[1], blend_size[1]);
    blend_va[1] = MAP_FAILED;
  }

  if(blend_ba[0] != 0) {
    ioctl(memdev_fd, MEMALLOC_IOCSFREEBUFFER, &blend_ba[0]);
    blend_ba[0] = 0;
  }

  if(blend_ba[1] != 0) {
    ioctl(memdev_fd, MEMALLOC_IOCSFREEBUFFER, &blend_ba[1]);
    blend_ba[1] = 0;
  }
#endif

  ReleaseFrame(&pp->input);

  ReleaseFrame(&pp->output);

  if(interlaced_frame_va != NULL) {
    free(interlaced_frame_va);
    interlaced_frame_va = NULL;
  }

  ReleasePp(pp);
}

/*------------------------------------------------------------------------------
    Function name   : pp_api_init
    Description     : Initialize the post processor. Enables joined mode if needed.
    Return type     : int
    Argument        : PPInst * pp, const void *dec_inst, u32 dec_type
------------------------------------------------------------------------------*/
int pp_api_init(PPInst * pp, const void *dec_inst, u32 dec_type) {
  PPResult res;

  res = PPInit(pp);

  if(res != PP_OK) {
    printf("Failed to init the PP: %d\n", res);

    *pp = NULL;

    return 1;
  }

  decoder = NULL;

  decodertype = dec_type;

  if(dec_inst != NULL && dec_type != PP_PIPELINE_DISABLED) {
    res = PPDecCombinedModeEnable(*pp, dec_inst, dec_type);

    if(res != PP_OK) {
      printf("Failed to enable PP-DEC pipeline: %d\n", res);

      *pp = NULL;

      return 1;
    }
    decoder = dec_inst;
  }

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_api_release
    Description     : Release the post processor. Disable pipeline if needed.
    Return type     : void
    Argument        : PPInst * pp
------------------------------------------------------------------------------*/
void pp_api_release(PPInst pp) {
  if(pp != NULL && decoder != NULL)
    PPDecCombinedModeDisable(pp, decoder);

  if(pp != NULL)
    PPRelease(pp);
}

/*------------------------------------------------------------------------------
    Function name   : pp_api_cfg
    Description     : Set the post processor according to the post processor
                      configuration.
    Return type     : int
    Argument        : PPInst pp
    Argument        : const void *dec_inst
    Argument        : u32 dec_type
    Argument        : PpParams * params
    Argument        : const struct TBCfg* tb_cfg
------------------------------------------------------------------------------*/
int pp_api_cfg(PPInst pp, const void *dec_inst, u32 dec_type, PpParams * params,
               const struct TBCfg * tb_cfg) {
  int ret = 0;
  PPResult res;

  u32 clock_gating = TBGetPPClockGating(tb_cfg);
  u32 output_endian = TBGetPPOutputPictureEndian(tb_cfg);
  u32 input_endian = TBGetPPInputPictureEndian(tb_cfg);
  u32 bus_burst_length = tb_cfg->pp_params.bus_burst_length;
  u32 word_swap = TBGetPPWordSwap(tb_cfg);
  u32 word_swap_16 = TBGetPPWordSwap16(tb_cfg);
  u32 data_discard = TBGetPPDataDiscard(tb_cfg);
  u32 dec_out_format = TBGetDecOutputFormat(tb_cfg);

  u32 *reg_base;
  multibuffer = TBGetTBMultiBuffer(tb_cfg);
  if(multibuffer &&
      (dec_type == PP_PIPELINED_DEC_TYPE_VP6 ||
       dec_type == PP_PIPELINED_DEC_TYPE_VP8 ||
       dec_type == PP_PIPELINED_DEC_TYPE_WEBP ||
       dec_type == PP_PIPELINE_DISABLED)) {
    multibuffer = 0;
  }

  reg_base = ((PPContainer *) pp)->pp_regs;

  if(interlaced) {
    dec_out_format = 0;
  }


#ifndef ASIC_TRACE_SUPPORT

  /* set up pp device configuration */
  {

    printf("---Clock Gating %d ---\n", clock_gating);
    SetPpRegister(reg_base, HWIF_PP_CLK_GATE_E, clock_gating);

    printf("---Amba Burst Length %d ---\n", bus_burst_length);
    SetPpRegister(reg_base, HWIF_PP_MAX_BURST, bus_burst_length);

    printf("---Data discard %d ---\n", data_discard);
    SetPpRegister(reg_base, HWIF_PP_DATA_DISC_E, data_discard);

    blend_endian_change = 0;
    input_endian_big = 0;
    output_endian_big = 0;
    output_rgb_swap = 0;
    output_word_swap = 0;
    output_word_swap_16 = 0;
    output_tiled_4x4 = 0;

    if(PP_IS_TILED_4(params->output_format)) {
      printf("---tiled 4x4 output---\n");
      output_tiled_4x4 = 1;
    }

    /* Override the configuration endianess (i.e., randomize is on) */
    if(0 == input_endian) {
      printf("---input endian BIG---\n");
      SetPpRegister(reg_base, HWIF_PP_IN_ENDIAN, 0);
      input_endian_big = 1;
      blend_endian_change = 1;
    } else if(1 == input_endian) {
      printf("---input endian LITTLE---\n");
      SetPpRegister(reg_base, HWIF_PP_IN_ENDIAN, 1);
      input_endian_big = 0;
    }

    if(0 == output_endian) {
      printf("---output endian BIG---\n");
      SetPpRegister(reg_base, HWIF_PP_OUT_ENDIAN, 0);

      if(RGB == params->output_format && 2 != params->out_rgb_fmt.bytes) {
        /* endian does not affect to 32 bit RGB */
        if(((PPContainer*)pp_inst)->hw_endian_ver == 0)
          output_endian_big = 0;
        else
          output_endian_big = 1;
      } else if(RGB == params->output_format && 2 == params->out_rgb_fmt.bytes) {
        output_endian_big = 1;
        /* swap also the 16 bits words for 16 bit RGB */
        if(((PPContainer*)pp_inst)->hw_endian_ver == 0)
          output_rgb_swap = 1;
      } else {
        output_endian_big = 1;
      }
    } else if(1 == output_endian) {
      printf("---output endian LITTLE---\n");
      SetPpRegister(reg_base, HWIF_PP_OUT_ENDIAN, 1);
      output_endian_big = 0;
    }

    if(0 == word_swap) {
      SetPpRegister(reg_base, HWIF_PP_OUT_SWAP32_E, 0);
      output_word_swap = 0;
    } else if(1 == word_swap) {
      SetPpRegister(reg_base, HWIF_PP_OUT_SWAP32_E, 1);
      output_word_swap = 1;
    }

    if(((PPContainer*)pp_inst)->hw_endian_ver > 0) {
      if( word_swap_16 == 1 ) {
        SetPpRegister(reg_base, HWIF_PP_OUT_SWAP16_E, 1);
        output_word_swap_16 = 1;
      } else if ( word_swap_16 == 0 ) {
        SetPpRegister(reg_base, HWIF_PP_OUT_SWAP16_E, 0);
        output_word_swap_16 = 0;
      }
    }
  }
#endif

  if(force_tiled_input &&
      (params->input_format == YUV420 ||
       params->input_format == YUV420C )) {
    SetPpRegister( reg_base, HWIF_PP_TILED_MODE, force_tiled_input);
  }

  res = PPGetConfig(pp, &pp_conf);

  if(params->zoom) {
    pp_conf.pp_in_crop.enable = 1;
    pp_conf.pp_in_crop.origin_x = params->zoomx0;
    pp_conf.pp_in_crop.origin_y = params->zoomy0;
    pp_conf.pp_in_crop.width = params->zoom->width;

    if(params->crop8_right) {
      pp_conf.pp_in_crop.width -= 8;
    }

    pp_conf.pp_in_crop.height = params->zoom->height;

    if(params->crop8_down) {
      pp_conf.pp_in_crop.height -= 8;
    }
  } else if(params->crop8_right || params->crop8_down) {
    pp_conf.pp_in_crop.enable = 1;
    pp_conf.pp_in_crop.origin_x = 0;
    pp_conf.pp_in_crop.origin_y = 0;

    pp_conf.pp_in_crop.width = params->input.width;
    pp_conf.pp_in_crop.height = params->input.height;

    if(params->crop8_right) {
      pp_conf.pp_in_crop.width -= 8;
    } else {
      pp_conf.pp_in_crop.width = params->input.width;
    }

    if(params->crop8_down) {
      pp_conf.pp_in_crop.height -= 8;
    } else {
      pp_conf.pp_in_crop.height = params->input.height;
    }
  }

  /* pp_conf.pp_in_img.crop8Rightmost = params->crop8_right; */
  /* pp_conf.pp_in_img.crop8Downmost = params->crop8_down; */

  /* if progressive or deinterlace (progressive output */
  pp_conf.pp_out_deinterlace.enable = 0;
  if(!params->output.field_output || params->deint.enable) {

    if(params->deint.enable) {
      pp_conf.pp_out_deinterlace.enable = params->deint.enable;
    }

    pp_conf.pp_in_img.width = params->input.width;
    pp_conf.pp_in_img.height = params->input.height;

  }
  /* if field output */
  else if(params->output.field_output /* && !params->deint.enable */ ) {

    pp_conf.pp_in_img.width = params->input.width;
    pp_conf.pp_in_img.height = params->input.height;

  } else {

    pp_conf.pp_in_img.width = params->input.width;
    pp_conf.pp_in_img.height = params->input.height;

  }

  pp_conf.pp_in_img.pic_struct = params->input_struct;

  if(params->input_struct != PP_PIC_BOT_FIELD) {
    pp_conf.pp_in_img.buffer_bus_addr = in_pic_ba;

    pp_conf.pp_in_img.buffer_cb_bus_addr =
      in_pic_ba + pp_conf.pp_in_img.width * pp_conf.pp_in_img.height;

    pp_conf.pp_in_img.buffer_cr_bus_addr =
      pp_conf.pp_in_img.buffer_cb_bus_addr +
      (pp_conf.pp_in_img.width * pp_conf.pp_in_img.height) / 4;
  } else {
    ASSERT(params->input_format == YUV420C ||
           params->input_format == YUYV422 ||
           params->input_format == YVYU422 ||
           params->input_format == UYVY422 ||
           params->input_format == VYUY422);
    pp_conf.pp_in_img.buffer_bus_addr_bot = in_pic_ba;

    pp_conf.pp_in_img.buffer_bus_addr_ch_bot =
      in_pic_ba + pp_conf.pp_in_img.width * pp_conf.pp_in_img.height;
  }

  if(params->input_struct == PP_PIC_TOP_AND_BOT_FIELD_FRAME) {
    ASSERT(params->input_format == YUV420C ||
           params->input_format == YUYV422 ||
           params->input_format == YVYU422 ||
           params->input_format == UYVY422 ||
           params->input_format == VYUY422);
    if(params->input_format == YUV420C) {
      pp_conf.pp_in_img.buffer_bus_addr_bot =
        pp_conf.pp_in_img.buffer_bus_addr + params->input.width;
      pp_conf.pp_in_img.buffer_bus_addr_ch_bot =
        pp_conf.pp_in_img.buffer_cb_bus_addr + params->input.width;
    } else
      pp_conf.pp_in_img.buffer_bus_addr_bot =
        pp_conf.pp_in_img.buffer_bus_addr + 2 * params->input.width;
  } else if(params->input_struct == PP_PIC_TOP_AND_BOT_FIELD) {
    ASSERT(params->input_format == YUV420C ||
           params->input_format == YUYV422 ||
           params->input_format == YVYU422 ||
           params->input_format == UYVY422 ||
           params->input_format == VYUY422);
    if(params->input_format == YUV420C) {
      pp_conf.pp_in_img.buffer_bus_addr_bot =
        pp_conf.pp_in_img.buffer_bus_addr +
        params->input.width * params->input.height / 2;
      pp_conf.pp_in_img.buffer_bus_addr_ch_bot =
        pp_conf.pp_in_img.buffer_cb_bus_addr +
        params->input.width * params->input.height / 4;
    } else
      pp_conf.pp_in_img.buffer_bus_addr_bot =
        pp_conf.pp_in_img.buffer_bus_addr +
        2 * params->input.width * params->input.height / 2;
  }

  pp_conf.pp_in_img.vc1_multi_res_enable = vc1_multires;
  pp_conf.pp_in_img.vc1_range_red_frm = vc1_rangered;
  pp_conf.pp_in_img.vc1_range_map_yenable = params->range.enable_y;
  pp_conf.pp_in_img.vc1_range_map_ycoeff = params->range.coeff_y;
  pp_conf.pp_in_img.vc1_range_map_cenable = params->range.enable_c;
  pp_conf.pp_in_img.vc1_range_map_ccoeff = params->range.coeff_c;

  switch (params->input_format) {
  case YUV420:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_0_PLANAR;
    break;
  case YUV420C:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
    break;
  case YUYV422:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED;
    break;
  case YVYU422:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED;
    break;
  case UYVY422:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED;
    break;
  case VYUY422:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED;
    break;
  case YUV422C:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR;
    break;
  case YUV400:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_0_0;
    break;
  case YUV422CR:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_4_0;
    break;
  case YUV411C:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR;
    break;
  case YUV444C:
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR;
    break;
  default:
    ret = 1;
    goto end;
  }

  if(params->input_trace_mode) {
    pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_0_TILED;
  }

  if(dec_inst != NULL && dec_type != PP_PIPELINED_DEC_TYPE_JPEG) {
    if( dec_type != PP_PIPELINED_DEC_TYPE_H264 ||
        pp_conf.pp_in_img.pix_format != PP_PIX_FMT_YCBCR_4_0_0 ) {
#ifdef __arm__
      /* tiled in combined mode supported only in FPGA env */
      pp_conf.pp_in_img.pix_format = dec_out_format == 0 ?
                                     PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR : PP_PIX_FMT_YCBCR_4_2_0_TILED;
#else
      pp_conf.pp_in_img.pix_format = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
#endif
    }
  }

  pp_conf.pp_in_img.video_range = params->cnv.video_range;

  pp_conf.pp_out_img.width = params->output.width;
  pp_conf.pp_out_img.height = params->output.height;

  /* Set output buffer */
  if(multibuffer && (dec_type == PP_PIPELINED_DEC_TYPE_MPEG4 ||
                     dec_type == PP_PIPELINED_DEC_TYPE_H264 ||
                     dec_type == PP_PIPELINED_DEC_TYPE_MPEG2 ||
                     dec_type == PP_PIPELINED_DEC_TYPE_VC1 ||
                     dec_type == PP_PIPELINED_DEC_TYPE_RV ||
                     dec_type == PP_PIPELINED_DEC_TYPE_AVS) ) {
    pp_conf.pp_out_img.buffer_bus_addr = 0;
    if(pp_setup_multibuffer(pp, dec_inst, dec_type, params))
      return 1;
  } else {
    pp_conf.pp_out_img.buffer_bus_addr = out_pic_ba;
  }

  pp_conf.pp_out_img.buffer_chroma_bus_addr =
    out_pic_ba+ pp_conf.pp_out_img.width * pp_conf.pp_out_img.height;

  out_pic_pixels = pp_conf.pp_out_img.width * pp_conf.pp_out_img.height;

  switch (params->output_format) {
  case YUV420C:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
    break;
  case YUYV422:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED;
    break;
  case YVYU422:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED;
    break;
  case UYVY422:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED;
    break;
  case VYUY422:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED;
    break;
  case YUYV422T4:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_YCBCR_4_2_2_TILED_4X4;
    break;
  case YVYU422T4:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_YCRYCB_4_2_2_TILED_4X4;
    break;
  case UYVY422T4:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_CBYCRY_4_2_2_TILED_4X4;
    break;
  case VYUY422T4:
    pp_conf.pp_out_img.pix_format = PP_PIX_FMT_CRYCBY_4_2_2_TILED_4X4;
    break;
  case RGB:
    if(params->out_rgb_fmt.bytes == 2) {
      pp_conf.pp_out_img.pix_format = PP_PIX_FMT_RGB16_CUSTOM;
      pp_conf.pp_out_rgb.rgb_bitmask.mask_r = params->out_rgb_fmt.mask[0] >> 16;
      pp_conf.pp_out_rgb.rgb_bitmask.mask_g = params->out_rgb_fmt.mask[1] >> 16;
      pp_conf.pp_out_rgb.rgb_bitmask.mask_b = params->out_rgb_fmt.mask[2] >> 16;
      pp_conf.pp_out_rgb.rgb_bitmask.mask_alpha =
        params->out_rgb_fmt.mask[3] >> 16;
    } else {
      pp_conf.pp_out_img.pix_format = PP_PIX_FMT_RGB32_CUSTOM;

      pp_conf.pp_out_rgb.rgb_bitmask.mask_r = params->out_rgb_fmt.mask[0];
      pp_conf.pp_out_rgb.rgb_bitmask.mask_g = params->out_rgb_fmt.mask[1];
      pp_conf.pp_out_rgb.rgb_bitmask.mask_b = params->out_rgb_fmt.mask[2];
      pp_conf.pp_out_rgb.rgb_bitmask.mask_alpha = params->out_rgb_fmt.mask[3];

      pp_conf.pp_out_rgb.alpha = 255 /*params->out_rgb_fmt.mask[3] */ ;
    }

    pp_conf.pp_out_rgb.dithering_enable = params->output.dithering_ena;

    break;
  default:
    ret = 1;
    goto end;
  }

  switch (params->rotation) {
  case 90:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_RIGHT_90;
    break;
  case -90:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_LEFT_90;
    break;
  case 1:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_HOR_FLIP;
    break;
  case 2:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_VER_FLIP;
    break;
  case 180:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_180;
    break;
  default:
    pp_conf.pp_in_rotation.rotation = PP_ROTATION_NONE;

  }

  if(params->pip) {
    pp_conf.pp_out_frm_buffer.enable = 1;
    pp_conf.pp_out_frm_buffer.frame_buffer_width = params->pip->width;
    pp_conf.pp_out_frm_buffer.frame_buffer_height = params->pip->height;
    pp_conf.pp_out_frm_buffer.write_origin_x = params->pipx0;
    pp_conf.pp_out_frm_buffer.write_origin_y = params->pipy0;

    pp_conf.pp_out_img.buffer_chroma_bus_addr =
      out_pic_ba +
      pp_conf.pp_out_frm_buffer.frame_buffer_width *
      pp_conf.pp_out_frm_buffer.frame_buffer_height;

    out_pic_pixels =
      pp_conf.pp_out_frm_buffer.frame_buffer_width *
      pp_conf.pp_out_frm_buffer.frame_buffer_height;
  }

  if(params->mask_count != 0) {
    pp_conf.pp_out_mask1.enable = 1;
    pp_conf.pp_out_mask1.origin_x = params->mask[0]->x0;
    pp_conf.pp_out_mask1.origin_y = params->mask[0]->y0;
    pp_conf.pp_out_mask1.width = params->mask[0]->x1 - params->mask[0]->x0;
    pp_conf.pp_out_mask1.height = params->mask[0]->y1 - params->mask[0]->y0;

    /* blending */
    if(params->mask[0]->overlay.enabled /*blend_size[0] != 0*/) {

      Overlay *overlay = &params->mask[0]->overlay;

      overlay->enabled = TRUE;

      /* formulate filename for blending component */
      strcpy(blendfile[0], params->mask[0]->overlay.file_name);

      if(read_mask_file(0, ((PPContainer *) pp)->blend_crop_support)) {
        return 1;
      }

      /* set up configuration */
      pp_conf.pp_out_mask1.alpha_blend_ena = 1;
      pp_conf.pp_out_mask1.blend_component_base = blend_ba[0];

      if(((PPContainer *) pp)->blend_crop_support) {
        pp_conf.pp_out_mask1.blend_origin_x = params->mask[0]->overlay.x0;
        pp_conf.pp_out_mask1.blend_origin_y = params->mask[0]->overlay.y0;
        pp_conf.pp_out_mask1.blend_width = params->mask[0]->overlay.data.width;
        pp_conf.pp_out_mask1.blend_height = params->mask[0]->overlay.data.height;
      }

    }
  }

  if(params->mask_count > 1) {
    pp_conf.pp_out_mask2.enable = 1;
    pp_conf.pp_out_mask2.origin_x = params->mask[1]->x0;
    pp_conf.pp_out_mask2.origin_y = params->mask[1]->y0;
    pp_conf.pp_out_mask2.width = params->mask[1]->x1 - params->mask[1]->x0;
    pp_conf.pp_out_mask2.height = params->mask[1]->y1 - params->mask[1]->y0;

    /* blending */
    if(params->mask[1]->overlay.enabled /*blend_size[1] != 0*/) {

      Overlay *overlay = &params->mask[1]->overlay;

      overlay->enabled = TRUE;

      /* formulate filename for blending component */
      strcpy(blendfile[1], params->mask[1]->overlay.file_name);

      if(read_mask_file(1, ((PPContainer *) pp)->blend_crop_support)) {
        return 1;
      }

      /* set up configuration */
      pp_conf.pp_out_mask2.alpha_blend_ena = 1;
      pp_conf.pp_out_mask2.blend_component_base = blend_ba[1];

      if(((PPContainer *) pp)->blend_crop_support) {
        pp_conf.pp_out_mask2.blend_origin_x = params->mask[1]->overlay.x0;
        pp_conf.pp_out_mask2.blend_origin_y = params->mask[1]->overlay.y0;
        pp_conf.pp_out_mask2.blend_width = params->mask[1]->overlay.data.width;
        pp_conf.pp_out_mask2.blend_height = params->mask[1]->overlay.data.height;
      }
    }
  }

  pp_conf.pp_out_rgb.rgb_transform = PP_YCBCR2RGB_TRANSFORM_CUSTOM;

  pp_conf.pp_out_rgb.rgb_transform_coeffs.a = params->cnv.a;
  pp_conf.pp_out_rgb.rgb_transform_coeffs.b = params->cnv.b;
  pp_conf.pp_out_rgb.rgb_transform_coeffs.c = params->cnv.c;
  pp_conf.pp_out_rgb.rgb_transform_coeffs.d = params->cnv.d;
  pp_conf.pp_out_rgb.rgb_transform_coeffs.e = params->cnv.e;

  pp_conf.pp_out_rgb.brightness = params->cnv.brightness;
  pp_conf.pp_out_rgb.saturation = params->cnv.saturation;
  pp_conf.pp_out_rgb.contrast = params->cnv.contrast;

  res = PPSetConfig(pp, &pp_conf);

  /* Restore the PP setup */
  /*    memcpy(&pp_conf, &tmpConfig, sizeof(PPConfig));
  */
  if(res != PP_OK) {
    printf("Failed to setup the PP\n");
    pp_print_result(res);
    ret = 1;
  }

  /* write deinterlacing parameters */
  if(params->deint.enable) {
    SetPpRegister(reg_base, HWIF_DEINT_BLEND_E, params->deint.blend_enable);
    SetPpRegister(reg_base, HWIF_DEINT_THRESHOLD, params->deint.threshold);
    SetPpRegister(reg_base, HWIF_DEINT_EDGE_DET,
                  params->deint.edge_detect_value);
  }
end:
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PrintBinary
    Description     :
    Return type     : void
    Argument        : u32 d
------------------------------------------------------------------------------*/
void PrintBinary(u32 d) {
  u32 v = 1 << 31;

  while(v) {
    printf("%d", (d & v) ? 1 : 0);
    v >>= 1;
  }
  printf("\n");
}

/*------------------------------------------------------------------------------

   .<++>  Function: CheckParams

        Functional description:
          Check parameter consistency

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
bool CheckParams(PpParams * pp) {
  bool ret = TRUE;
  u32 i, j;

  ASSERT(pp);

#define CHECK_IMAGE_DIMENSION( v, min, max, mask, t) \
    if(v<min || v>max || (v&mask)) { \
        fprintf( stderr, t, v ); \
        ret = FALSE; \
    }

  if(pp->zoom) {
    if(pp->zoomx0 + pp->zoom->width > pp->input.width ||
        pp->zoomy0 + pp->zoom->height > pp->input.height ||
        pp->zoomx0 < 0 || pp->zoomy0 < 0) {
      fprintf(stderr,
              "Zoom area [%d.%d]...[%d.%d] exceeds input dimensions [%d.%d].\n",
              pp->zoomx0, pp->zoomy0, pp->zoomx0 + pp->zoom->width,
              pp->zoomy0 + pp->zoom->height, pp->input.width,
              pp->input.height);
      ret = FALSE;
    }
  }

  /* 2. Rotation */
  if(pp->rotation != 0 && pp->rotation != 90 && pp->rotation != -90 &&
      pp->rotation != 1 && pp->rotation != 2 && pp->rotation != 180) {
    fprintf(stderr, "Rotation '%d' is invalid.\n", pp->rotation);
    ret = FALSE;
  }
  /* 3. Frame count */
  if(pp->frames == 0) {
    fprintf(stderr, "Frame count must be specified >= 1.\n");
    ret = FALSE;
  }
  /* 4. Input sequence must be specified */
  if(!*pp->input_file) {
    fprintf(stderr, "Input sequence must be specified.\n");
    ret = FALSE;
  }
  /* 5. Check color conversion parameters */
  if(pp->cnv.video_range != 0 && pp->cnv.video_range != 1) {
    fprintf(stderr, "Video range must be inside [0,1].\n");
    ret = FALSE;
  }
  /* 6. byte format specification */
  if(pp->byte_fmt.bytes != pp->byte_fmt.word_width) {
    fprintf(stderr, "Inconsistent output byte format specification.\n");
    ret = FALSE;
  } else {
    /* Process the byte order.. */
    pp->big_endian_fmt.word_width = pp->byte_fmt.word_width;
    pp->big_endian_fmt.bytes = pp->byte_fmt.bytes;
    for(i = 0; i < pp->byte_fmt.word_width; ++i) {
      pp->big_endian_fmt.byte_order[i] = i;
    }
  }
  if(pp->byte_fmt_input.bytes != pp->byte_fmt_input.word_width) {
    fprintf(stderr, "Inconsistent input byte format specification.\n");
    ret = FALSE;
  } else {
    /* Process the byte order.. */
    pp->big_endian_fmt_input.word_width = pp->byte_fmt_input.word_width;
    pp->big_endian_fmt_input.bytes = pp->byte_fmt_input.bytes;
    for(i = 0; i < pp->byte_fmt_input.word_width; ++i) {
      pp->big_endian_fmt_input.byte_order[i] = i;
    }
  }

#undef CHECK_IMAGE_DIMENSION

  /* Prepare channel masks and offsets for trace module */
  if(PP_IS_RGB(pp->output_format)) {
    u32 offs_adjust = 0;

    if(PP_IS_INTERLEAVED(pp->output_format)) {
      u32 bits =
        pp->out_rgb_fmt.pad[0] + pp->out_rgb_fmt.chan[0] +
        pp->out_rgb_fmt.pad[1] + pp->out_rgb_fmt.chan[1] +
        pp->out_rgb_fmt.pad[2] + pp->out_rgb_fmt.chan[2] +
        pp->out_rgb_fmt.pad[3] + pp->out_rgb_fmt.chan[3] +
        pp->out_rgb_fmt.pad[4];
      u32 target = (bits > 16) ? 32 : 16;

      if(target > bits) {
        pp->out_rgb_fmt.pad[4] += (target - bits);
        pp->out_rgb_fmt.bytes = target / 8;
        printf("%d bit(s) extra padding added to RGB output.\n",
               target - bits);
      }

      if(pp->out_rgb_fmt.chan[0] > 8 || pp->out_rgb_fmt.chan[1] > 8 ||
          pp->out_rgb_fmt.chan[2] > 8 || pp->out_rgb_fmt.chan[3] > 8) {
        fprintf(stderr,
                "Single RGB channel may not contain more than 8 bits.\n");
        ret = FALSE;
      }
    }

    offs_adjust = (pp->out_rgb_fmt.bytes & 1) ? 8 : 0; /* Use only 32/16 bits in padding calcs */

    for(i = 0; i < PP_MAX_CHANNELS; ++i) {
      u32 offs = 0;
      u32 mask = 0;

      for(j = 0; j < PP_MAX_CHANNELS; ++j) {
        offs += pp->out_rgb_fmt.pad[j];
        /* jth output channel is internal channel i */
        if(pp->out_rgb_fmt.order[j] == i) {
          if(pp->out_rgb_fmt.chan[j])
            mask = ((1 << pp->out_rgb_fmt.chan[j]) - 1);
          pp->out_rgb_fmt.offs[i] = offs + offs_adjust;
          pp->out_rgb_fmt.mask[i] =
            (mask <<
             (8 * pp->out_rgb_fmt.bytes -
              pp->out_rgb_fmt.chan[j])) >> offs;
          if(pp->out_rgb_fmt.bytes <= 2)    /* double mask for smaller values */
            pp->out_rgb_fmt.mask[i] |= (pp->out_rgb_fmt.mask[i] << 16);
        } else {
          offs += pp->out_rgb_fmt.chan[j];
        }
      }
    }
  }

  if(pp->input_struct > PP_PIC_TOP_AND_BOT_FIELD_FRAME)
    ret = FALSE;

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : pp_load_cfg_file
    Description     :
    Return type     : int
    Argument        : char * cfg_file
    Argument        : PpParams *pp
------------------------------------------------------------------------------*/
int pp_load_cfg_file(char *cfg_file, ppContainer * pp) {

  u32 i, tmp;
  PpParams *p;

  /* Parse configuration file... */
  printf("---parse config file...---\n");
  tmp = ParseConfig(cfg_file, ReadParam, (void *) pp);
  if(tmp == 0)
    return 1;

  pp->num_pp_params = tmp;
  for(i = 0, p = pp->pp; i < tmp; i++, p++) {
    if(!CheckParams(p))
      return 1;

    /* Map color conversion parameters */
    MapColorConversion(&p->cnv);

    p->pp_orig_width = (p->input.width + 15) / 16;
    p->pp_orig_height = (p->input.height + 15) / 16;
    p->pp_in_width = p->zoom ? (p->zoom->width + 15) / 16 : p->pp_orig_width;
    p->pp_in_height = p->zoom ? (p->zoom->height + 15) / 16 : p->pp_orig_height;
  }

  /* Use default output file name if not specified. Output file name
   * decided based on first PpParams block, same file written even if
   * parts of the output are in RGB and others in YUV format */
  if(!*pp->pp->output_file) {
    strcpy(pp->pp->output_file,
           PP_IS_RGB(pp->pp->output_format) ?
           PP_DEFAULT_OUT_RGB : PP_DEFAULT_OUT_YUV);
  }
  for(i = 1; i < tmp; i++)
    strcpy(pp->pp[i].output_file, pp->pp[0].output_file);

  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: MapColorConversion

        Functional description:
          Map from "intuitive" values brightness, contrast and saturation
          into color conversion parameters a1, a2, b, ..., f, thr1, thr2.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void MapColorConversion(ColorConversion * cc) {
  i32 th1y, th2y;
  i32 s;

  ASSERT(cc);

  /* Defaults for b,c,d,e parameters */
  if(cc->video_range == 0) {
    /* 16-235, 16-240 video range */
    if(cc->b == INT_MAX)
      cc->b = 403;
    if(cc->c == INT_MAX)
      cc->c = 120;
    if(cc->d == INT_MAX)
      cc->d = 48;
    if(cc->e == INT_MAX)
      cc->e = 475;
  } else {
    /* 0-255 video range */
    if(cc->b == INT_MAX)
      cc->b = 409;
    if(cc->c == INT_MAX)
      cc->c = 208;
    if(cc->d == INT_MAX)
      cc->d = 100;
    if(cc->e == INT_MAX)
      cc->e = 516;
  }

  /* Perform mapping if one or more of the "intuitive" values is specified,
   * i.e. differs from INT_MAX. In this case, missing values (equal to
   * INT_MAX) are treated as zeroes. */
  if(cc->contrast != INT_MAX ||
      cc->brightness != INT_MAX ||
      cc->a != INT_MAX || cc->saturation != INT_MAX) {

    /* Use zero for missing values */
    if(cc->contrast == INT_MAX)
      cc->contrast = 0;
    if(cc->brightness == INT_MAX)
      cc->brightness = 0;
    if(cc->saturation == INT_MAX)
      cc->saturation = 0;
    if(cc->a == INT_MAX)
      cc->a = 298;

    s = 64 + cc->saturation;

    /* Map to color conversion parameters */

    if(cc->video_range == 0) {
      /* 16-235, 16-240 video range */

      i32 tmp1, tmp2;

      /* Contrast */
      if(cc->contrast == 0) {
        cc->thr1 = cc->thr2 = 0;
        cc->off1 = cc->off2 = 0;
        cc->a = cc->a1 = cc->a2 = 298;
      } else {

        cc->thr1 = (219 * (cc->contrast + 128)) / 512;
        th1y = (219 - 2 * cc->thr1) / 2;
        cc->thr2 = 219 - cc->thr1;
        th2y = 219 - th1y;

        tmp1 = (th1y * 256) / cc->thr1;
        tmp2 = ((th2y - th1y) * 256) / (cc->thr2 - cc->thr1);
        cc->off1 = ((th1y - ((tmp2 * cc->thr1) / 256)) * cc->a) / 256;
        cc->off2 = ((th2y - ((tmp1 * cc->thr2) / 256)) * cc->a) / 256;

        tmp1 = (64 * (cc->contrast + 128)) / 128;
        tmp2 = 256 * (128 - tmp1);
        cc->a1 = (tmp2 + cc->off2) / cc->thr1;
        cc->a2 =
          cc->a1 + (256 * (cc->off2 - 1)) / (cc->thr2 - cc->thr1);

      }

      cc->b = SATURATE(0, 1023, (s * cc->b) >> 6);
      cc->c = SATURATE(0, 1023, (s * cc->c) >> 6);
      cc->d = SATURATE(0, 1023, (s * cc->d) >> 6);
      cc->e = SATURATE(0, 1023, (s * cc->e) >> 6);

      /* Brightness */
      cc->f = cc->brightness;

    } else {
      /* 0-255 video range */

      /* Contrast */
      if(cc->contrast == 0) {
        cc->thr1 = cc->thr2 = 0;
        cc->off1 = cc->off2 = 0;
        cc->a = cc->a1 = cc->a2 = 256;
      } else {
        cc->thr1 = (64 * (cc->contrast + 128)) / 128;
        th1y = 128 - cc->thr1;
        cc->thr2 = 256 - cc->thr1;
        th2y = 256 - th1y;
        cc->a1 = (th1y * 256) / cc->thr1;
        cc->a2 = ((th2y - th1y) * 256) / (cc->thr2 - cc->thr1);
        cc->off1 = th1y - (cc->a2 * cc->thr1) / 256;
        cc->off2 = th2y - (cc->a1 * cc->thr2) / 256;
      }

      cc->b = SATURATE(0, 1023, (s * cc->b) >> 6);
      cc->c = SATURATE(0, 1023, (s * cc->c) >> 6);
      cc->d = SATURATE(0, 1023, (s * cc->d) >> 6);
      cc->e = SATURATE(0, 1023, (s * cc->e) >> 6);

      /* Brightness */
      cc->f = cc->brightness;
    }
  }

  /* Use zero for missing values */
  if(cc->contrast == INT_MAX)
    cc->contrast = 0;
  if(cc->brightness == INT_MAX)
    cc->brightness = 0;
  if(cc->saturation == INT_MAX)
    cc->saturation = 0;

  if(cc->a == INT_MAX) {
    if(cc->a1 != INT_MAX)
      cc->a = cc->a1;
    else if(cc->video_range == 0)
      cc->a = cc->a1 = cc->a2 = 298;
    else
      cc->a = cc->a1 = cc->a2 = 256;
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: ReleasePp

        Functional description:
          Release memory alloc'd by PP descriptor

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void ReleasePp(PpParams * pp) {
  u32 i;

  ASSERT(pp);
  /* Free pip block */
  if(pp->pip) {
    free(pp->pip);
  }
  /* Free pip block */
  if(pp->pip_tmp) {
    free(pp->pip_tmp);
  }
  /* Free pip YUV block */
  if(pp->pip_yuv) {
    free(pp->pip_yuv);
  }
  /* Free zoom block */
  if(pp->zoom) {
    free(pp->zoom);
  }
  /* Free masks */
  for(i = 0; i < pp->mask_count; ++i) {
    free(pp->mask[i]);
  }

  blend_size[0] = blend_size[1] = blendpos[0] = blendpos[1] = 0;

  /* Free filters */
  if(pp->fir_filter) {
    free(pp->fir_filter);
    pp->fir_filter = NULL;
  }

}

/*------------------------------------------------------------------------------
    Function name   : WritePicture
    Description     :
    Return type     : int
    Argument        : u32 * image
    Argument        : i32 size
    Argument        : const char *fname
    Argument        : int frame
------------------------------------------------------------------------------*/
int WritePicture(u32 * image, i32 size, const char *fname, int frame) {
  FILE *file = NULL;

  if(frame == 0)
    file = fopen(fname, "wb");
  else
    file = fopen(fname, "ab");

  if(file == NULL) {
    fprintf(stderr, "Unable to open output file: %s\n", fname);
    return -1;
  }

#ifdef MD5SUM
  TBWriteFrameMD5Sum(file, image, size, frame);
#else
  fwrite(image, 1, size, file);
#endif

  fclose(file);

  return 0;
}

/*------------------------------------------------------------------------------

    Function name:  PPTrace

    Purpose:
        Example implementation of PPTrace function. Prototype of this
        function is given in ppapi.h. This implementation appends
        trace messages to file named 'pp_api.trc'.

------------------------------------------------------------------------------*/
void PPTrace(const char *string) {
  printf("%s", string);
}

#if 0
void PPTrace(const char *string) {
  FILE *fp;

  fp = fopen("pp_api.trc", "at");

  if(!fp)
    return;

  fwrite(string, 1, strlen(string), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);
}
#endif
/*------------------------------------------------------------------------------

   <++>.<++>  Function: WriteBmp

        Functional description:
          Write raw RGB data into BMP file.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/

#ifdef PP_WRITE_BMP

struct BITMAPINFOHEADER {
  u32 bi_size;
  u32 bi_width;
  u32 bi_height;
  u16 bi_planes;
  u16 bi_bit_count;
  u32 bi_compression;
  u32 bi_size_image;
  u32 bi_xpels_per_meter;
  u32 bi_ypels_per_meter;
  u32 bi_clr_used;
  u32 bi_clr_important;
} __attribute__ ((__packed__));

struct BITMAPFILEHEADER {
  u16 bf_type;
  u32 bf_size;
  u16 bf_reserved1;
  u16 bf_reserved2;
  u32 bf_off_bits;
} __attribute__ ((__packed__));

typedef struct BITMAPFILEHEADER BITMAPFILEHEADER_t;
typedef struct BITMAPINFOHEADER BITMAPINFOHEADER_t;

#define BI_RGB        0L
#define BI_BITFIELDS  3L

i32 WriteBmp(const char *filename, u32 pix_fmt, u8 * rgb, u32 w, u32 h) {
  FILE *fp;
  u32 rgb_size;
  u32 mask;
  u32 bpp;

  BITMAPFILEHEADER_t bmpfh;
  BITMAPINFOHEADER_t bmpih;

  fp = fopen(filename, "wb");
  if(!fp) {
    fprintf(stderr, "Error: Unable to open file: %s\n", filename);
    return -1;
  }

  if(pix_fmt & 0x01000) {
    rgb_size = w * h * 4;
    bpp = 32;
  } else {
    rgb_size = w * h * 2;
    bpp = 16;
  }

  bmpfh.bf_type = 'MB';
  bmpfh.bf_size =
    sizeof(BITMAPFILEHEADER_t) + sizeof(BITMAPINFOHEADER_t) + 3 * 4 +
    rgb_size;
  bmpfh.bf_reserved1 = 0;
  bmpfh.bf_reserved2 = 0;
  bmpfh.bf_off_bits =
    sizeof(BITMAPFILEHEADER_t) + sizeof(BITMAPINFOHEADER_t) + 3 * 4;

  bmpih.bi_size = sizeof(BITMAPINFOHEADER_t);
  bmpih.bi_width = w;
  bmpih.bi_height = h;
  bmpih.bi_planes = 1;
  bmpih.bi_bit_count = bpp;
  bmpih.bi_compression = /* BI_RGB */ BI_BITFIELDS;
  bmpih.bi_size_image = rgb_size;
  bmpih.bi_xpels_per_meter = 0;
  bmpih.bi_ypels_per_meter = 0;
  bmpih.bi_clr_used = 0;
  bmpih.bi_clr_important = 0;

  /* write BMP header */

  fwrite(&bmpfh, sizeof(BITMAPFILEHEADER_t), 1, fp);
  fwrite(&bmpih, sizeof(BITMAPINFOHEADER_t), 1, fp);

  if(pix_fmt & 0x01000) {
    mask = 0x00FF0000;  /* R */
    mask = pp.curr_pp->out_rgb_fmt.mask[0];
    fwrite(&mask, 4, 1, fp);

    mask = 0x0000FF00;  /* G */
    mask = pp.curr_pp->out_rgb_fmt.mask[1];
    fwrite(&mask, 4, 1, fp);

    mask = 0x000000FF;  /* B */
    mask = pp.curr_pp->out_rgb_fmt.mask[2];
    fwrite(&mask, 4, 1, fp);
  } else {
    mask = 0x0000F800;  /* R */

    mask = pp.curr_pp->out_rgb_fmt.mask[0] >> 16;
    fwrite(&mask, 4, 1, fp);

    printf("mask_r %08x\n", mask);

    mask = 0x000007E0;  /* G */

    mask = pp.curr_pp->out_rgb_fmt.mask[1] >> 16;;
    printf("mask_g %08x\n", mask);
    fwrite(&mask, 4, 1, fp);

    mask = 0x0000001F;  /* B */

    mask = pp.curr_pp->out_rgb_fmt.mask[2] >> 16;
    printf("mask_b %08x\n", mask);
    fwrite(&mask, 4, 1, fp);

  }

#if 0
  {
    /* vertical flip bitmap data */
    i32 x, y;
    u8 *img = (u8 *) rgb;

    for(y = 0; y < h; y++) {
      for(x = 0; x < w; x++) {
        u8 tmp = *img;

        *img = *(img + 1);
        *(img + 1) = tmp;
        img += 2;
      }

    }
  }
#endif

  fwrite(rgb, rgb_size, 1, fp);

  fclose(fp);

  return 0;
}
#endif

/*------------------------------------------------------------------------------

   <++>.<++>  Function: MakeTiled

        Functional description:
          Perform conversion YUV420C to TILED YUV420.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void MakeTiled(u8 * data, u32 w, u32 h) {
  u32 i, j;
  u8 *orig = data;
  u8 *tmp_img = (u8 *) calloc((w * h * 3) / 2, 1);
  u8 *pt;

  printf("Performing conversion YUV420C to TILED YUV420!\n");

  pt = tmp_img;

  for(i = 0; i < h / 16; i++) {
    for(j = 0; j < w / 16; j++) {
      u32 z;
      u8 *p_read = data + (i * 16 * w) + (j * 16);

      for(z = 0; z < 16; z++) {
        memcpy(pt, p_read, 16);
        pt += 16;
        p_read += w;
      }
    }
  }

  data += w * h;

  for(i = 0; i < h / 16; i++) {
    for(j = 0; j < w / 16; j++) {
      u32 z;
      u8 *p_read = data + (i * 8 * w) + j * 16;

      for(z = 0; z < 8; z++) {
        memcpy(pt, p_read, 16);
        pt += 16;
        p_read += w;
      }
    }
  }

  memcpy(orig, tmp_img, (w * h * 3) / 2);
  free(tmp_img);
}


/*------------------------------------------------------------------------------

   <++>.<++>  Function: MakeTiled

        Functional description:
          Perform conversion YUV420C to TILED YUV420.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void MakeTiled8x4(u8 * data, u32 w, u32 h, u32 planar_to_semiplanar) {
  u32 i, j;
  u8 *orig = data;
  u8 *tmp_img = (u8 *) calloc((w * h * 3) / 2, 1);
  u8 *pt;
  u32 tiles_w;
  u32 tiles_h;
  u32 skip;
  u32 *p_in0 = (u32*)(data),
       *p_in1 = (u32*)(data +   w),
        *p_in2 = (u32*)(data + 2*w),
         *p_in3 = (u32*)(data + 3*w);
  u32 *outp = (u32*)tmp_img;

  tiles_w = w/8;
  tiles_h = h/4;
  skip = w-w/4;

  for( i = 0 ; i < tiles_h ; ++i) {
    for( j = 0 ; j < tiles_w ; ++j ) {
      *outp++ = *p_in0++;
      *outp++ = *p_in0++;
      *outp++ = *p_in1++;
      *outp++ = *p_in1++;
      *outp++ = *p_in2++;
      *outp++ = *p_in2++;
      *outp++ = *p_in3++;
      *outp++ = *p_in3++;
    }

    p_in0 += skip;
    p_in1 += skip;
    p_in2 += skip;
    p_in3 += skip;
  }

  if(planar_to_semiplanar) {
    u8 * p_cb, * p_cr;
    u8 * p_out_u8 = (u8*)outp;
    p_cb = data + w*h;
    p_cr = p_cb + ((w*h)/4);
    u32 r;
    u32 wc = w/2; /* chroma width in pels */

    printf("Convert input data from PLANAR to SEMIPLANAR\n");

    for( i = 0 ; i < tiles_h/2 ; ++i) {
      r = 0;
      for( j = 0 ; j < tiles_w ; ++j ) {
        *p_out_u8++ = p_cb[r+0];
        *p_out_u8++ = p_cr[r+0];
        *p_out_u8++ = p_cb[r+1];
        *p_out_u8++ = p_cr[r+1];
        *p_out_u8++ = p_cb[r+2];
        *p_out_u8++ = p_cr[r+2];
        *p_out_u8++ = p_cb[r+3];
        *p_out_u8++ = p_cr[r+3];

        *p_out_u8++ = p_cb[wc+r+0];
        *p_out_u8++ = p_cr[wc+r+0];
        *p_out_u8++ = p_cb[wc+r+1];
        *p_out_u8++ = p_cr[wc+r+1];
        *p_out_u8++ = p_cb[wc+r+2];
        *p_out_u8++ = p_cr[wc+r+2];
        *p_out_u8++ = p_cb[wc+r+3];
        *p_out_u8++ = p_cr[wc+r+3];

        *p_out_u8++ = p_cb[2*wc+r+0];
        *p_out_u8++ = p_cr[2*wc+r+0];
        *p_out_u8++ = p_cb[2*wc+r+1];
        *p_out_u8++ = p_cr[2*wc+r+1];
        *p_out_u8++ = p_cb[2*wc+r+2];
        *p_out_u8++ = p_cr[2*wc+r+2];
        *p_out_u8++ = p_cb[2*wc+r+3];
        *p_out_u8++ = p_cr[2*wc+r+3];

        *p_out_u8++ = p_cb[3*wc+r+0];
        *p_out_u8++ = p_cr[3*wc+r+0];
        *p_out_u8++ = p_cb[3*wc+r+1];
        *p_out_u8++ = p_cr[3*wc+r+1];
        *p_out_u8++ = p_cb[3*wc+r+2];
        *p_out_u8++ = p_cr[3*wc+r+2];
        *p_out_u8++ = p_cb[3*wc+r+3];
        *p_out_u8++ = p_cr[3*wc+r+3];

        r += 4;
      }

      p_cb += wc*4;
      p_cr += wc*4;
    }
  } else {
    for( i = 0 ; i < tiles_h/2 ; ++i) {
      for( j = 0 ; j < tiles_w ; ++j ) {
        *outp++ = *p_in0++;
        *outp++ = *p_in0++;
        *outp++ = *p_in1++;
        *outp++ = *p_in1++;
        *outp++ = *p_in2++;
        *outp++ = *p_in2++;
        *outp++ = *p_in3++;
        *outp++ = *p_in3++;
      }

      p_in0 += skip;
      p_in1 += skip;
      p_in2 += skip;
      p_in3 += skip;
    }
  }

  memcpy(orig, tmp_img, (w * h * 3) / 2);
  free(tmp_img);
}


/*------------------------------------------------------------------------------

   <++>.<++>  Function: Tiled4x4ToPlanar

        Functional description:
          Perform conversion from 4:2:2 TILED 4x4 to 4:2:2 INTERLEAVED

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void Tiled4x4ToPlanar(u8 * data, u32 w, u32 h) {
  u32 i, j;
  u8 *orig = data;
  u8 *tmp_img = (u8 *) calloc((w * ((h+3)&~3) * 2), 1);
  u32 *pt = NULL;
  u32 *p_read = NULL;
  u32 *p_write = NULL;
  u32 stride = 0;

#if 0
  static u32 header = 0;
  static u32 frame_num = 0;
  static FILE * trc_file = NULL;
#endif

  printf("Performing conversion 4:2:2 TILED 4x4 to 4:2:2 INTERLEAVED!\n");

#if 0
  if( header == 0 ) {
    header = 1;
    trc_file = fopen("tiled_to_planar.trc", "wt");
  }
#endif

  pt = (u32*)tmp_img;

  p_read = (u32*)data;

  stride = w/2; /* one row in 32-bit addresses */

#if 0
  fprintf(trc_file, "--picture=%d\n", frame_num++);
  fprintf(trc_file, "  stride = %d\n", stride);
#endif

  for(i = 0; i < h ; i+=4 ) {
    u32 w_offset;
    /* Set write pointer to correct 4x4 block row */
    p_write = pt + i*stride;

    w_offset = i*stride;

#if 0
    fprintf(trc_file, "  tile row %d\n", i/4 );
    fprintf(trc_file, "  w offset %d words\n", i*stride);
    fprintf(trc_file, "      ptr %p\n", p_write );
#endif

    for(j = 0; j < w ; j+=4 ) {
#if 0
      fprintf(trc_file, "    tile nbr %d\n", j/4 );
      fprintf(trc_file, "      read ptr %p\n", p_read );
      fprintf(trc_file, "                    %8.8x %8.8x %8.8x %8.8x   %8.8x %8.8x %8.8x %8.8x\n",
              p_read[0],
              p_read[1],
              p_read[2],
              p_read[3],
              p_read[4],
              p_read[5],
              p_read[6],
              p_read[7] );

      fprintf(trc_file, "    write to offset %8d %8d %8d %8d   %8d %8d %8d %8d\n",
              w_offset+0,
              w_offset+1,
              w_offset+stride+0,
              w_offset+stride+1,
              w_offset+stride*2+0,
              w_offset+stride*2+1,
              w_offset+stride*3+0,
              w_offset+stride*3+1 );
#endif

      /* left pixel group */
      p_write[0]           = p_read[0];
      p_write[stride]      = p_read[2];
      p_write[2*stride]    = p_read[4];
      p_write[3*stride]    = p_read[6];

      /* right pixel group */
      if( j + 2 < w ) {
        p_write[1]           = p_read[1];
        p_write[stride+1]    = p_read[3];
        p_write[2*stride+1]  = p_read[5];
        p_write[3*stride+1]  = p_read[7];
      }

      p_read += 8; /* skip to next block */
      p_write += 2;

#if 0
      w_offset += 2;
#endif
    }
  }

  memcpy(orig, tmp_img, (w * ((h+3)&~3) * 2) );
  free(tmp_img);
}


/*------------------------------------------------------------------------------

   <++>.<++>  Function: pipeline_disable

        Functional description:
          Disable the pipeline.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/

void pipeline_disable(void) {

  PPDecCombinedModeDisable(pp_inst, decoder);
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: swap_in_out_pointers

        Functional description:
          Swap input and output pointers if external mode used
          during pipeline decoding (for rotation, cropping)

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
static void swap_in_out_pointers(u32 width, u32 height) {

  addr_t tmpbus;

  tmpbus = pp_conf.pp_in_img.buffer_bus_addr;

  pp_conf.pp_in_img.buffer_bus_addr = pp_conf.pp_out_img.buffer_bus_addr;

  pp_conf.pp_in_img.buffer_cb_bus_addr =
    pp_conf.pp_in_img.buffer_bus_addr + width * height;

  pp_conf.pp_in_img.buffer_cr_bus_addr =
    pp_conf.pp_in_img.buffer_cb_bus_addr + (width * height) / 4;

  pp_conf.pp_out_img.buffer_bus_addr = tmpbus;

  pp_conf.pp_out_img.buffer_chroma_bus_addr =
    pp_conf.pp_out_img.buffer_bus_addr +
    pp_conf.pp_out_img.width * pp_conf.pp_out_img.height;

}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: pp_vc1_specific

        Functional description:
          Set the VC-1 specific flags

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void pp_vc1_specific(u32 multires_enable, u32 rangered) {
  vc1_multires = multires_enable;
  vc1_rangered = rangered;

}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: reset_vc1_specific

        Functional description:
          Initialize the VC-1 specific flags

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
static void reset_vc1_specific(void) {
  vc1_multires = 0;
  vc1_rangered = 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: pp_change_resolution

        Functional description:
            Updates pp input frame resolution in multiresolution sequences

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
int pp_change_resolution(u32 width, u32 height, const struct TBCfg * tb_cfg) {
  pp.curr_pp->input.width = width;
  pp.curr_pp->input.height = height;

  /* Disable mask while updating the resolution, otherwise there will be */
  /* an additional read from mask file. */
  pp.curr_pp->mask_count = 0;

  /* Override selected PP features */
  TBOverridePpContainer((PPContainer *)pp_inst, tb_cfg );

  if(pp_api_cfg(pp_inst, decoder, decodertype, pp.curr_pp, tb_cfg) != 0) {
    printf("PP CHANGE RESOLUTION FAILED\n");
    return CFG_UPDATE_FAIL;
  }

  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: Resample

        Functional description:
          Resample channels of frame.

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void Resample(PixelData * p, PixelFormat fmt_new) {
  i32 x, y;

  ASSERT(p);

  /* Check if no resampling required */
  if(p->fmt == fmt_new)
    return;

  if(p->fmt == PP_INTERNAL_FMT(fmt_new)) {
    p->fmt = fmt_new;
    return;
  }

  /* YUV 4:0:0 --> YUV 4:4:4 */
  if(p->fmt == YUV400 && fmt_new == YUV444) {
    for(y = 0; y < p->height; ++y) {
      for(x = 0; x < p->width; ++x) {
        p->yuv.Cb[y][x] = 128;
        p->yuv.Cr[y][x] = 128;
      }
    }
  }
  /* YUV 4:2:0 --> YUV 4:4:4 */
  else if(p->fmt == YUV420 && fmt_new == YUV444) {
    for(y = p->height - 1; y >= 0; --y) {
      for(x = p->width - 1; x >= 0; --x) {
        p->yuv.Cb[y][x] = p->yuv.Cb[y >> 1][x >> 1];
        p->yuv.Cr[y][x] = p->yuv.Cr[y >> 1][x >> 1];
      }
    }
  }
  /* YUV 4:2:2 --> YUV 4:4:4 */
  else if(p->fmt == YUV422 && fmt_new == YUV444) {
    for(y = 0; y < p->height; ++y) {
      for(x = p->width - 1; x >= 0; --x) {
        p->yuv.Cb[y][x] = p->yuv.Cb[y][x >> 1];
        p->yuv.Cr[y][x] = p->yuv.Cr[y][x >> 1];
      }
    }
  }
  /* YUV 4:4:4 --> YUV 4:2:0 */
  else if(p->fmt == YUV444 && PP_INTERNAL_FMT(fmt_new) == YUV420) {
    for(y = 0; y < p->height; y += 2) {
      for(x = 0; x < p->width; x += 2) {
        p->yuv.Cb[y >> 1][x >> 1] = p->yuv.Cb[y][x];
        p->yuv.Cr[y >> 1][x >> 1] = p->yuv.Cr[y][x];
      }
    }
  }
  /* YUV 4:4:4 --> YUV 4:0:0 */
  else if(p->fmt == YUV444 && PP_INTERNAL_FMT(fmt_new) == YUV400) {
    for(y = 0; y < p->height; y++) {
      for(x = 0; x < p->width; x++) {
        p->yuv.Cb[y][x] = 128;
        p->yuv.Cr[y][x] = 128;
      }
    }
  }
  /* YUV 4:4:4 --> YUV 4:2:2 */
  else if(p->fmt == YUV444 && PP_INTERNAL_FMT(fmt_new) == YUV422) {
    for(y = 0; y < p->height; y++) {
      for(x = 0; x < p->width; x += 2) {
        p->yuv.Cb[y][x >> 1] = p->yuv.Cb[y][x];
        p->yuv.Cr[y][x >> 1] = p->yuv.Cr[y][x];
      }
    }
  }
  /* Unsupported resampling */
  else {
    ASSERT(0);
  }

  p->fmt = fmt_new;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: pp_alloc_blend_components

        Functional description:
          alloc mem for blending components

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/

int pp_alloc_blend_components(PpParams * pp, u32 ablend_crop) {
#ifdef __arm__
  MemallocParams params;
#endif
  Overlay *blend1 = NULL;
  Overlay *blend0 = &pp->mask[0]->overlay;

  blend1 = &pp->mask[1]->overlay;

  if(pp->mask[0] != NULL)
    if(blend0 != NULL)
      if(*(blend0->file_name)) {
        blend0->enabled = 1;

        /* larger buffer required if cropping supported */
        if(ablend_crop)
          blend_size[0] = blend0->data.width * blend0->data.height * 4;
        else
          blend_size[0] = (pp->mask[0]->x1 - pp->mask[0]->x0)
                          * (pp->mask[0]->y1 - pp->mask[0]->y0) * 4;

        blend_tmp[0] =
          malloc(blend0->data.width * blend0->data.height * 3 / 2);
        blend_tmp444[0] =
          malloc(blend0->data.width * blend0->data.height * 4);
#ifndef __arm__
        blend_va[0] = malloc(blend_size[0]);
        blend_ba[0] = (addr_t)blend_va[0];
        if(blend_va[0] == NULL)
          goto blend_err;
#else

        /* input buffer allocation */

        params.size = blend_size[0];

        /* Allocate buffer and get the bus address */
        ioctl(memdev_fd, MEMALLOC_IOCXGETBUFFER, &params);

        if(params.bus_address == 0) {
          printf("Input allocation failed\n");
          goto blend_err;
        }
        blend_ba[0] = params.bus_address;
        /* Map the bus address to virtual address */
        blend_va[0] = MAP_FAILED;
        blend_va[0] =
          (u32 *) mmap(0, blend_size[0], PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd_mem, blend_ba[0]);

        if(blend_va[0] == MAP_FAILED) {
          printf("Failed to alloc input image\n");
          goto blend_err;
        }
#endif

#ifdef ASIC_TRACE_SUPPORT
        ablend_base[0] = (u8*)blend_va[0];
        if(ablend_crop) {
          ablend_width[0] = blend0->data.width;
          ablend_height[0] = blend0->data.height;
        } else {
          ablend_width[0] = pp->mask[0]->x1 - pp->mask[0]->x0;
          ablend_height[0] = pp->mask[0]->y1 - pp->mask[0]->y0;
        }
#endif

      }

  if(pp->mask[1] != NULL)
    if(blend1 != NULL)
      if(*(blend1->file_name)) {

        blend1->enabled = 1;

        /* larger buffer required if cropping supported */
        if(ablend_crop)
          blend_size[1] = blend1->data.width * blend1->data.height * 4;
        else
          blend_size[1] = (pp->mask[1]->x1 - pp->mask[1]->x0)
                          * (pp->mask[1]->y1 - pp->mask[1]->y0) * 4;

        blend_tmp[1] =
          malloc(blend1->data.width * blend1->data.height * 3 / 2);
        blend_tmp444[1] =
          malloc(blend1->data.width * blend1->data.height * 4);
#ifndef __arm__
        blend_va[1] = malloc(blend_size[1]);
        blend_ba[1] = (addr_t)blend_va[1];
        if(blend_va[1] == NULL)
          goto blend_err;

#else
        /* input buffer allocation */
        params.size = blend_size[1];

        /* Allocate buffer and get the bus address */
        ioctl(memdev_fd, MEMALLOC_IOCXGETBUFFER, &params);

        if(params.bus_address == 0) {
          printf("Input allocation failed\n");
          goto blend_err;
        }
        blend_ba[1] = params.bus_address;
        /* Map the bus address to virtual address */
        blend_va[1] = MAP_FAILED;
        blend_va[1] =
          (u32 *) mmap(0, blend_size[1], PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd_mem, blend_ba[1]);

        if(blend_va[1] == MAP_FAILED) {
          printf("Failed to alloc input image\n");
          goto blend_err;
        }
#endif

#ifdef ASIC_TRACE_SUPPORT
        ablend_base[1] = (u8*)blend_va[1];
        if(ablend_crop) {
          ablend_width[1] = blend1->data.width;
          ablend_height[1] = blend1->data.height;
        } else {
          ablend_width[1] = pp->mask[1]->x1 - pp->mask[1]->x0;
          ablend_height[1] = pp->mask[1]->y1 - pp->mask[1]->y0;
        }
#endif

      }

  return 0;
blend_err:
  return 1;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: read_mask_file

        Functional description:
          read blend component from file

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/

int read_mask_file(u32 id, const u32 ablend_crop_support) {
  u32 i, j, tmp;
  u32 top_skip, left_skip, right_skip;
  u32 height, width;
  u32 size;
  u8 *s_y, *s_cb, *s_cr, *d;
  u32 *s32, *d32;
  u32 alpha = 0;
  Area *mask = pp.curr_pp->mask[id];
  FILE *fid;

  if(!mask)
    return 0;

  fid = fopen(blendfile[id], "r");
  if(fid == NULL) {
    printf("Unable to open blend file (id %d):%s\n", id, blendfile[id]);
    return 1;
  }

  fseek(fid, blendpos[id], SEEK_SET);
  if(mask->overlay.pixel_format == YUV420) {
    size = mask->overlay.data.width * mask->overlay.data.height * 3 / 2;
    fread(blend_tmp[id], size, 1, fid);
  } else {
    ASSERT(mask->overlay.pixel_format == AYCBCR);
    size = mask->overlay.data.width * mask->overlay.data.height * 4;
    fread(blend_tmp444[id], size, 1, fid);
  }
  blendpos[id] += size;
  fclose(fid);

  /* resample to AYCbCr */
  if(mask->overlay.pixel_format == YUV420) {
    if(mask->overlay.alpha_source >= ALPHA_CONSTANT)
      alpha = mask->overlay.alpha_source - ALPHA_CONSTANT;

    size = size * 2 / 3;
    s_y = (u8 *) blend_tmp[id];
    s_cb = s_y + size;
    s_cr = s_cb + size / 4;
    d = (u8 *) blend_tmp444[id];

    width = mask->overlay.data.width;
    height = mask->overlay.data.height;
    if(mask->overlay.pixel_format == YUV420) {
      for(i = 0; i < height; i++) {
        for(j = 0; j < width; j++) {
          *d++ = alpha;
          *d++ = s_y[i * width + j];
          *d++ = s_cb[(i / 2) * width / 2 + j / 2];
          *d++ = s_cr[(i / 2) * width / 2 + j / 2];
        }
      }
    }
  }

  /* color conversion if needed */
  if(PP_IS_RGB(pp.curr_pp->output_format)) {
    Yuv2Rgb(blend_tmp444[id], mask->overlay.data.width,
            mask->overlay.data.height, &pp.curr_pp->cnv);
  }

  width = mask->x1 - mask->x0;
  height = mask->y1 - mask->y0;

  s32 = blend_tmp444[id];
  d32 = blend_va[id];

  if(!ablend_crop_support) {
    top_skip = mask->overlay.data.width * mask->overlay.y0;
    left_skip = mask->overlay.x0;
    right_skip = mask->overlay.data.width - width - left_skip;
  } else {
    top_skip = left_skip = right_skip = 0;
    width = mask->overlay.data.width;
    height = mask->overlay.data.height;
  }

  s32 += top_skip + left_skip;
  for(i = 0; i < height; i++) {
    for(j = 0; j < width; j++) {
      if(mask->overlay.alpha_source == ALPHA_SEQUENTIAL) {
        *d32 = *s32++;
        *((u8 *) d32) = alpha;
        alpha = (alpha + 1) & 0xFF;
        d32++;
      } else
        *d32++ = *s32++;
    }
    s32 += right_skip + left_skip;
  }

#ifndef ASIC_TRACE_SUPPORT
  if(blend_endian_change) {
    for(i = 0; i < (blend_size[id] / 4); i++) {
      tmp = blend_va[id][i];
      blend_va[id][i] = ((tmp & 0x000000FF) << 24) |
                        ((tmp & 0x0000FF00) << 8) |
                        ((tmp & 0x00FF0000) >> 8) | ((tmp & 0xFF000000) >> 24);
    }
  }
#endif

  return 0;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: pp_read_blend_components

        Functional description:
          read blend component from file

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/

int pp_read_blend_components(const void *pp_inst) {
  i32 ret = 0;

  ret = read_mask_file(0, ((PPContainer *) pp_inst)->blend_crop_support);
  ret |= read_mask_file(1, ((PPContainer *) pp_inst)->blend_crop_support);

  return ret;
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: toggle_endian_yuv

        Functional description:
            Changes the endianness of a yuv image

        Inputs: width, height, virtual pointer

        Outputs:

------------------------------------------------------------------------------*/
static void toggle_endian_yuv(u32 width, u32 height, u32 * virtual) {
  u32 i, tmp;

  printf("toggle_endian\n");

  ASSERT(virtual != NULL);
  for(i = 0; i < ((width * height * 3) / 2) / 4; i++) /* the input is YUV,
                                                         * hence the size */
  {

    tmp = virtual[i];
    virtual[i] = ((tmp & 0x000000FF) << 24) |
                 ((tmp & 0x0000FF00) << 8) |
                 ((tmp & 0x00FF0000) >> 8) | ((tmp & 0xFF000000) >> 24);
  }
}

/*------------------------------------------------------------------------------

   <++>.<++>  Function: Yuv2Rgb

        Functional description:
            Color conversion for alpha blending component

        Inputs:

        Outputs:

------------------------------------------------------------------------------*/
void Yuv2Rgb(u32 * p, u32 w, u32 h, ColorConversion * cc) {
  u32 x, y;
  u32 tmp;
  i32 Y, Cb, Cr;
  i32 R, G, B;
  i32 a, off;
  i32 adj = 0;
  u8 *p_comp;

  ASSERT(p);
  ASSERT(cc);

  p_comp = (u8 *) p;
  /* Color conversion */
  for(y = 0; y < h; y++) {
    for(x = 0; x < w; x++) {
      p_comp++;
      Y = p_comp[0];
      Cb = p_comp[1];
      Cr = p_comp[2];

      /* Range is normalized to start from 0 */
      if(cc->video_range == 0) {
        Y = Y - 16;
        Y = MAX(Y, 0);
        adj = 0;
      }

      if(Y < cc->thr1) {
        a = cc->a1;
        off = 0;
      } else if(Y <= cc->thr2) {
        a = cc->a2;
        off = cc->off1;
      } else {
        a = cc->a1;
        off = cc->off2;
      }

      R = (((a * Y) + (cc->b * (Cr - 128))) >> 8) + cc->f + off;

      G = (((a * Y) - (cc->c * (Cr - 128)) -
            (cc->d * (Cb - 128))) >> 8) + cc->f + off;

      B = (((a * Y) + (cc->e * (Cb - 128))) >> 8) + cc->f + off;

      R = MIN(MAX(0, R), 255);
      G = MIN(MAX(0, G), 255);
      B = MIN(MAX(0, B), 255);

      *p_comp++ = R;
      *p_comp++ = G;
      *p_comp++ = B;
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : pp_update_config
    Description     : Prepare the post processor according to the post processor
                      configuration file. Allocate input and output picture
                      buffers.
    Return type     : int
    Argument        : const void *dec_inst
    Argument        : u32 dec_type
------------------------------------------------------------------------------*/
#ifdef ASIC_TRACE_SUPPORT
extern u8 *p_display_y;
extern u8 *p_display_c;
extern u32 display_height;
extern i32 left_offset;
extern i32 top_offset;
extern u32 input_orig_height;
#endif

int pp_update_config(const void *dec_inst, u32 dec_type, const struct TBCfg * tb_cfg) {
  u32 ret_val = CFG_UPDATE_OK;
  static i32 frame = 0;

  if(pp.curr_pp == NULL ||
      (++frame == pp.curr_pp->frames && pp.pp_param_idx < pp.num_pp_params - 1)) {
    if(!pp.curr_pp) {
      pp.curr_pp = pp.pp;
      pp.pp_param_idx = 0;
    } else {
      pp_release_buffers(pp.curr_pp);
      pp.pp_param_idx++;
      pp.curr_pp++;
    }

    frame = 0;

    if(force_tiled_input &&
        pp.curr_pp->input_format == YUV420 ) {
      planar_to_semiplanar_input = 1;
      pp.curr_pp->input_format =
        pp.curr_pp->input.fmt = YUV420C;
    }

    /* Check multibuffer status before buffer allocation */
    multibuffer = TBGetTBMultiBuffer(tb_cfg);
    if(multibuffer &&
        (dec_type == PP_PIPELINED_DEC_TYPE_VP6 ||
         dec_type == PP_PIPELINED_DEC_TYPE_VP8 ||
         dec_type == PP_PIPELINED_DEC_TYPE_WEBP ||
         dec_type == PP_PIPELINE_DISABLED)) {
      multibuffer = 0;
    }

    printf("Multibuffer %s\n", multibuffer ? "ENABLED" : "DISABLED");

    printf("---alloc frames---\n");

    if(pp_alloc_buffers(pp.curr_pp,
                        ((PPContainer *) pp_inst)->blend_crop_support) != 0) {
      printf("\t\tFailed\n");
      return CFG_UPDATE_FAIL;
    }

#ifndef PP_PIPELINE_ENABLED
    pp.curr_pp->input.base = (u8 *) in_pic_va;

    AllocFrame(&pp.curr_pp->input);
#endif
    pp.curr_pp->output.base = (u8 *) out_pic_va;

    AllocFrame(&pp.curr_pp->output);

    printf("---config PP---\n");

    /* Override selected PP features */
    TBOverridePpContainer((PPContainer *)pp_inst, tb_cfg);

    if(pp_api_cfg(pp_inst, dec_inst, dec_type, pp.curr_pp, tb_cfg) != 0) {
      printf("\t\tFailed\n");
      return CFG_UPDATE_FAIL;
    }

    /* Print model parameters */
    printf("input image: %d %d\n", pp.curr_pp->input.width,
           pp.curr_pp->input.height);
    printf("output image: %d %d\n", pp.curr_pp->output.width,
           pp.curr_pp->output.height);
    if(pp.curr_pp->zoom) {
      printf("zoom enabled, input area size %d %d\n",
             pp.curr_pp->zoom->width - pp.curr_pp->crop8_right * 8,
             pp.curr_pp->zoom->height - pp.curr_pp->crop8_down * 8);
      printf("scaling factors X %.2f  Y %.2f\n",
             (float) pp.curr_pp->output.width /
             (float) (pp.curr_pp->rotation ? pp.curr_pp->zoom->height -
                      pp.curr_pp->crop8_down * 8 : pp.curr_pp->zoom->width -
                      pp.curr_pp->crop8_right * 8),
             (float) pp.curr_pp->output.height /
             (float) (pp.curr_pp->rotation ? pp.curr_pp->zoom->width -
                      pp.curr_pp->crop8_right *
                      8 : pp.curr_pp->zoom->height -
                      pp.curr_pp->crop8_down * 8));
    } else {
      printf("input area size %d %d\n",
             pp.curr_pp->output.width - pp.curr_pp->crop8_right * 8,
             pp.curr_pp->output.height - pp.curr_pp->crop8_down * 8);
      printf("scaling factors X %.2f  Y %.2f\n",
             (float) pp.curr_pp->output.width /
             (float) (pp.curr_pp->rotation ? pp.curr_pp->input.height -
                      pp.curr_pp->crop8_down * 8 : pp.curr_pp->input.width -
                      pp.curr_pp->crop8_right * 8),
             (float) pp.curr_pp->output.height /
             (float) (pp.curr_pp->rotation ? pp.curr_pp->input.width -
                      pp.curr_pp->crop8_right *
                      8 : pp.curr_pp->input.height -
                      pp.curr_pp->crop8_down * 8));
    }

    if(PP_IS_RGB(pp.curr_pp->output_format)) {
      if(PP_IS_INTERLEAVED(pp.curr_pp->output_format)) {
        printf("RGB output width: %d bytes per pel.\n",
               pp.curr_pp->out_rgb_fmt.bytes);
      } else {
        printf("use planar RGB.\n");
      }
    }
    if(pp.curr_pp->pip) {
      printf("PiP enabled, frame size %d %d.\n", pp.curr_pp->pip->width,
             pp.curr_pp->pip->height);
    }
    if(pp.curr_pp->fir_filter) {
      printf("FIR filter enabled.\n");
    }
    if(pp.curr_pp->pipeline) {
      printf("pipeline enabled.\n");
    }
    if(pp.curr_pp->filter_enabled) {
      printf("filter enabled.\n");
    }

    /* config updated */
    ret_val = CFG_UPDATE_NEW;
  }
  /* stand-alone test -> stop if all configs handled */
  else if(frame == pp.curr_pp->frames && dec_inst == NULL)
    return CFG_UPDATE_FAIL;

#ifdef ASIC_TRACE_SUPPORT
  p_display_y = (u8 *) out_pic_va;
  p_display_c = p_display_y +
                (pp.curr_pp->pip ? pp.curr_pp->pip->width : pp_conf.pp_out_img.width) *
                (pp.curr_pp->pip ? pp.curr_pp->pip->height : pp_conf.pp_out_img.height);
  display_height = pp.curr_pp->pip ? pp.curr_pp->pip->height :
                   pp_conf.pp_out_img.height;
  input_orig_height = pp.curr_pp->input.height / 16;

  /* Fix original input height for vertical downscale shortcut */
  {
    PPContainer *ppc = (PPContainer *)pp_inst;
    if(ppc->fast_vertical_downscale &&
        ( pp_conf.pp_in_img.pix_format == PP_PIX_FMT_YCBCR_4_2_0_PLANAR ||
          pp_conf.pp_in_img.pix_format == PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR ||
          pp_conf.pp_in_img.pix_format == PP_PIX_FMT_YCBCR_4_2_0_TILED ||
          pp_conf.pp_in_img.pix_format == PP_PIX_FMT_YCBCR_4_0_0 )) {
      input_orig_height = (input_orig_height+1)/2;
    }
  }

  left_offset = pp.curr_pp->pipx0;
  top_offset = pp.curr_pp->pipy0;
#endif

  return ret_val;

}

/*------------------------------------------------------------------------------
    Function name   : pp_set_input_interlaced
    Description     : decoder sets up interlaced input type
    Return type     : void
    Argument        : is interlaced or not
------------------------------------------------------------------------------*/
void pp_set_input_interlaced(u32 is_interlaced) {
  interlaced = is_interlaced ? 1 : 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_check_combined_status
    Description     : Check return value of post-processing
    Return type     : void
    Argument        : is interlaced or not
------------------------------------------------------------------------------*/
PPResult pp_check_combined_status(void) {
  PPResult ret;

  ret = PPGetResult(pp_inst);
  pp_print_result(ret);
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : pp_set_input_interlaced
    Description     : decoder sets up interlaced input type
    Return type     : void
    Argument        : is interlaced or not
------------------------------------------------------------------------------*/
void pp_print_result(PPResult ret) {
  printf("PPGetResult: ");

  switch (ret) {
  case PP_OK:
    printf("PP_OK\n");
    break;
  case PP_PARAM_ERROR:
    printf("PP_PARAM_ERROR\n");
    break;
  case PP_MEMFAIL:
    printf("PP_MEMFAIL\n");
    break;
  case PP_SET_IN_SIZE_INVALID:
    printf("PP_SET_IN_SIZE_INVALID\n");
    break;
  case PP_SET_IN_ADDRESS_INVALID:
    printf("PP_SET_IN_ADDRESS_INVALID\n");
    break;
  case PP_SET_IN_FORMAT_INVALID:
    printf("PP_SET_IN_FORMAT_INVALID\n");
    break;
  case PP_SET_CROP_INVALID:
    printf("PP_SET_CROP_INVALID\n");
    break;
  case PP_SET_ROTATION_INVALID:
    printf("PP_SET_ROTATION_INVALID\n");
    break;
  case PP_SET_OUT_SIZE_INVALID:
    printf("PP_SET_OUT_SIZE_INVALID\n");
    break;
  case PP_SET_OUT_ADDRESS_INVALID:
    printf("PP_SET_OUT_ADDRESS_INVALID\n");
    break;
  case PP_SET_OUT_FORMAT_INVALID:
    printf("PP_SET_OUT_FORMAT_INVALID\n");
    break;
  case PP_SET_VIDEO_ADJUST_INVALID:
    printf("PP_SET_VIDEO_ADJUST_INVALID\n");
    break;
  case PP_SET_RGB_BITMASK_INVALID:
    printf("PP_SET_RGB_BITMASK_INVALID\n");
    break;
  case PP_SET_FRAMEBUFFER_INVALID:
    printf("PP_SET_FRAMEBUFFER_INVALID\n");
    break;
  case PP_SET_MASK1_INVALID:
    printf("PP_SET_MASK1_INVALID\n");
    break;
  case PP_SET_MASK2_INVALID:
    printf("PP_SET_MASK2_INVALID\n");
    break;
  case PP_SET_DEINTERLACE_INVALID:
    printf("PP_SET_DEINTERLACE_INVALID\n");
    break;
  case PP_SET_IN_STRUCT_INVALID:
    printf("PP_SET_IN_STRUCT_INVALID\n");
    break;
  case PP_SET_IN_RANGE_MAP_INVALID:
    printf("PP_SET_IN_RANGE_MAP_INVALID\n");
    break;
  case PP_SET_ABLEND_UNSUPPORTED:
    printf("PP_SET_ABLEND_UNSUPPORTED\n");
    break;
  case PP_SET_DEINTERLACING_UNSUPPORTED:
    printf("PP_SET_DEINTERLACING_UNSUPPORTED\n");
    break;
  case PP_SET_DITHERING_UNSUPPORTED:
    printf("PP_SET_DITHERING_UNSUPPORTED\n");
    break;
  case PP_SET_SCALING_UNSUPPORTED:
    printf("PP_SET_SCALING_UNSUPPORTED\n");
    break;
  case PP_BUSY:
    printf("PP_BUSY\n");
    break;
  case PP_HW_BUS_ERROR:
    printf("PP_HW_BUS_ERROR\n");
    break;
  case PP_HW_TIMEOUT:
    printf("PP_HW_TIMEOUT\n");
    break;
  case PP_DWL_ERROR:
    printf("PP_DWL_ERROR\n");
    break;
  case PP_SYSTEM_ERROR:
    printf("PP_SYSTEM_ERROR\n");
    break;
  case PP_DEC_COMBINED_MODE_ERROR:
    printf("PP_DEC_COMBINED_MODE_ERROR\n");
    break;
  case PP_DEC_RUNTIME_ERROR:
    printf("PP_DEC_RUNTIME_ERROR\n");
    break;
  default:
    printf("other %d\n", ret);
    break;
  }
}

/*------------------------------------------------------------------------------
    Function name   : pp_rotation_used
    Description     : used to ask from pp if rotation enabled
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 pp_rotation_used(void) {
  return pp.curr_pp->rotation ? 1 : 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_cropping_used
    Description     : used to ask from pp if cropping enabled
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 pp_cropping_used(void) {
  return pp_conf.pp_in_crop.enable ? 1 : 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_rotation_used
    Description     : used to ask from pp if mpeg4 filter enabled
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 pp_mpeg4_filter_used(void) {
  return pp.curr_pp->filter_enabled ? 1 : 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_deinterlace_used
    Description     : used to ask from pp if deinterlace enabled
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 pp_deinterlace_used(void) {
  return pp_conf.pp_out_deinterlace.enable ? 1 : 0;
}

/*
    typedef struct PPoutput_
    {
        u32 buffer_bus_addr;
        u32 buffer_chroma_bus_addr;
    } PPOutput;

    typedef struct PPOutputBuffers_
    {
        u32 nbr_of_buffers;
        PPOutput PPoutputBuffer[17];
    } PPOutputBuffers;
*/

/*------------------------------------------------------------------------------
    Function name   : pp_setup_multibuffer
    Description     : setup pipeline multiple buffers for full pipeline
    Return type     : u32
    Argument        : pp instance, dec instance, decoder type, pp parameters
------------------------------------------------------------------------------*/
u32 pp_setup_multibuffer(PPInst pp, const void *dec_inst,
                         u32 dec_type, PpParams * params) {
  PPResult ret;
  u32 luma_size, pic_size, i;
  u32 bytes_per_pixel_mul2;
  output_buffer.nbr_of_buffers = number_of_buffers;

  /* TODO fix for even more buffers */

  if(!number_of_buffers)
    return 1;

  if(number_of_buffers > 17)
    output_buffer.nbr_of_buffers = 17;

  /*
      output_buffer.PPoutputBuffer[0].buffer_bus_addr = out_pic_ba;
      p_multibuffer_va[0] = out_pic_va;

      output_buffer.PPoutputBuffer[0].buffer_chroma_bus_addr =
      output_buffer.PPoutputBuffer[0].buffer_bus_addr
      + pp_conf.pp_out_img.width * pp_conf.pp_out_img.height;
      if(number_of_buffers > 1){
          output_buffer.PPoutputBuffer[1].buffer_bus_addr = out_pic_ba + (out_pic_size/2);

          p_multibuffer_va[1] = out_pic_va + out_pic_size/2/4;
          output_buffer.PPoutputBuffer[1].buffer_chroma_bus_addr =
          output_buffer.PPoutputBuffer[1].buffer_bus_addr
         + pp_conf.pp_out_img.width * pp_conf.pp_out_img.height;
      }
  */
  /* if PIP used, alloc based on that, otherwise output size */

  /* note: 4x4 tiled mode requires dimensions to be multiples of 4, however
   * only 2 bytes per pixel are then used so the current assumption of 4 bytes
   * per pixel times the actual dimensions should suffice */
  if(params->pip == NULL) {
    luma_size = params->output.width * params->output.height;
  } else {
    luma_size = params->pip->width * params->pip->height;
  }

  if(PP_IS_RGB(params->output_format)) {
    bytes_per_pixel_mul2 = 2*params->out_rgb_fmt.bytes;
  } else {
    switch(params->output_format) {
    case YUV420C:
      bytes_per_pixel_mul2 = 3;
      break;
    case YUYV422:
    case YVYU422:
    case UYVY422:
    case VYUY422:
    case YUYV422T4:
    case YVYU422T4:
    case UYVY422T4:
    case VYUY422T4:
      bytes_per_pixel_mul2 = 4;
      break;
    default:
      bytes_per_pixel_mul2 = 8;
      break;
    }
  }
  pic_size = luma_size * bytes_per_pixel_mul2 / 2;

  ASSERT(out_pic_size >= (number_of_buffers * pic_size));

  output_buffer.pp_output_buffers[0].buffer_bus_addr = out_pic_ba;
  output_buffer.pp_output_buffers[0].buffer_chroma_bus_addr =
    output_buffer.pp_output_buffers[0].buffer_bus_addr + luma_size;

  p_multibuffer_va[0] = out_pic_va;

  for(i=1; i <  number_of_buffers; i++) {
    output_buffer.pp_output_buffers[i].buffer_bus_addr = output_buffer.pp_output_buffers[i-1].buffer_bus_addr + pic_size;
    output_buffer.pp_output_buffers[i].buffer_chroma_bus_addr =
      output_buffer.pp_output_buffers[i].buffer_bus_addr + luma_size;

    p_multibuffer_va[i] = (u32*)((char *)p_multibuffer_va[i-1] + pic_size);
  }

  ret = PPDecSetMultipleOutput(pp, &output_buffer);
  if( ret != PP_OK )
    return 1;

  /* setup our double buffering for display */
  last_out.buffer_bus_addr = display_buff_ba;
  last_out.buffer_chroma_bus_addr = last_out.buffer_bus_addr + luma_size;

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : pp_number_of_buffers
    Description     : decoder uses this function to say how many buffers needed
    Return type     :
    Argument        : amount of buffers
------------------------------------------------------------------------------*/
void pp_number_of_buffers(u32 amount) {
  if(multibuffer) {
    number_of_buffers = amount;
    pp_setup_multibuffer(pp_inst, NULL,0, pp.curr_pp);
  }
}

char *pp_get_out_name(void) {
  return pp.curr_pp->output_file;
}


/*------------------------------------------------------------------------------
    Function name   : TBOverridePpContainer
    Description     : Override register settings generated by PPSetupHW() based
                      on tb.cfg settings
    Return type     :
    Argument        : PPContainer *
                      struct TBCfg *
------------------------------------------------------------------------------*/
void TBOverridePpContainer(PPContainer *pp_c, const struct TBCfg * tb_cfg) {
  /* Override fast downscale if tb.cfg says so */
  if(tb_cfg->pp_params.fast_hor_down_scale_disable)
    pp_c->fast_horizontal_downscale_disable = 1;
  if(tb_cfg->pp_params.fast_ver_down_scale_disable)
    pp_c->fast_vertical_downscale_disable = 1;
}
