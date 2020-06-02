/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Description : Internal traces
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "enctrace.h"
#include "queue.h"
#include "error.h"
#include "instance.h"
#include "sw_slice.h"
#include "hevcencapi.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
static FILE *fileAsic = NULL;
static FILE *filePreProcess = NULL;
static FILE *fRegs = NULL;
static FILE *fRecon = NULL;
static FILE *fTrace = NULL;
static FILE *fTraceProbs = NULL;
static FILE *fTraceSegments = NULL;
static FILE *fDump = NULL;

#define HSWREG(n)       ((n)*4)

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
struct enc_sw_trace
{
  struct container *container;
  struct queue files;         /* Open files */
  struct queue stream_trace;      /* Stream trace buffer store */

  FILE *stream_trace_fp;          /* Stream trace file */
  FILE *deblock_fp;           /* deblock.yuv file */
  FILE *cutree_ctrl_flow_fp;          /* cutree control flow trace file */
  //    FILE *prof_fp;              /* profile.yuv file, record the bitrate and PSNR */

  int trace_frame_id;
  int cur_frame_idx;
  int cur_frame_enqueue_idx; /* frame idx before enqueue */
  int parallelCoreNum;
  bool bTraceCurFrame;
  bool bTraceCuInfo;
  int trace_pass;
};

struct enc_sw_open_file
{
  struct node *next;
  FILE *file;
};

static struct enc_sw_trace ctrl_sw_trace;

i32 HEVCRefBufferRecord[VCENC_MAX_REF_FRAMES * 6 + 1] = {0};
i32 HEVCIOBufferIdx = 0;

static FILE *Enc_sw_open_file(FILE *file, char *name);

/*------------------------------------------------------------------------------
  open_file
------------------------------------------------------------------------------*/
int Enc_get_param(FILE *file, char *name)
{
  char buffer[FILENAME_MAX];
  char bufferT[FILENAME_MAX];


  int val;

  ASSERT(file && name);
  rewind(file);
  while (fgets(buffer, FILENAME_MAX, file))
  {
    sscanf(buffer, "%s %d\n", bufferT, &val);   /* Pick first word */
    if (!strcmp(name, bufferT))
    {
      return val;
    }
  }

  return -1;
}

/*------------------------------------------------------------------------------
  test_data_init
------------------------------------------------------------------------------*/
#define DEFAULT_TB_CFG_FILENAME "tb.cfg"

i32 Enc_test_data_init(i32 parallelCoreNum)
{
  FILE *fp;

  memset(&ctrl_sw_trace, 0, sizeof(struct enc_sw_trace));

  ctrl_sw_trace.cur_frame_idx = 0;
  ctrl_sw_trace.cur_frame_enqueue_idx = 0;
  ctrl_sw_trace.parallelCoreNum = parallelCoreNum;
  HEVCIOBufferIdx = 0;

  if ((getenv("TEST_DATA_FILES") == NULL))
  {
    fp = fopen(DEFAULT_TB_CFG_FILENAME, "r");
    //fprintf(stderr, "Generating traces from default file <%s>.\n", DEFAULT_TB_CFG_FILENAME);
  }
  else
  {
    fp = fopen(getenv("TEST_DATA_FILES"), "r");
    //fprintf(stderr, "Generating traces from <%s>\n", getenv("TEST_DATA_FILES"));
  }

  if (fp == NULL)
  {
    //fprintf(stderr, "Cannot open trace configuration file.\n");
    //Error(4, ERR, "tb.cfg", ", ", SYSERR);
    return NOK;
  }

  if ((getenv("TEST_DATA_FILES") == NULL))
  {
    printf("Generating traces by <%s>\n", DEFAULT_TB_CFG_FILENAME);
  }
  else
  {
    printf("Generating traces by <%s>\n", getenv("TEST_DATA_FILES"));
  }

  ctrl_sw_trace.stream_trace_fp = Enc_sw_open_file(fp, "stream.trc");
  ctrl_sw_trace.cutree_ctrl_flow_fp = Enc_sw_open_file(fp, "trace_CUTREE_ctrl_flow.trc");
  ctrl_sw_trace.trace_frame_id = Enc_get_param(fp, "trace_frame_id");
  ctrl_sw_trace.bTraceCurFrame = ((ctrl_sw_trace.trace_frame_id == -1) || (ctrl_sw_trace.trace_frame_id == ctrl_sw_trace.cur_frame_idx));
  ctrl_sw_trace.bTraceCuInfo = Enc_get_param(fp, "cuInfo.txt") != -1;
  ctrl_sw_trace.trace_pass = Enc_get_param(fp, "trace_pass");
  if(ctrl_sw_trace.trace_pass == -1)
    ctrl_sw_trace.trace_pass = 2;

  fclose(fp);

  return OK;
}

void trace_sw_cutree_ctrl_flow(int size, int out_cnt, int pop_cnt, int *qpoutidx)
{
  int i;
  FILE *fp = ctrl_sw_trace.cutree_ctrl_flow_fp;

  if (!fp) return;

  fprintf(fp, "cutree size %d output %d pop %d qpoutidx", size, out_cnt, pop_cnt);
  for(i = 0; i < out_cnt; i++)
    fprintf(fp, " %d", qpoutidx[i]);
  fprintf(fp, "\n");
}

bool EncTraceCuInfoCheck(void)
{
  return ctrl_sw_trace.bTraceCuInfo && ctrl_sw_trace.bTraceCurFrame;
}

/*------------------------------------------------------------------------------
  test_data_release
------------------------------------------------------------------------------*/
void Enc_test_data_release(void)
{
  struct node *n;


  /* Close open files */
  while ((n = queue_get(&ctrl_sw_trace.files)))
  {
    fclose(((struct enc_sw_open_file *)n)->file);
    free(n);
  }

  return;
}

static FILE *Enc_sw_open_file(FILE *file, char *name)
{
  char buffer[FILENAME_MAX];
  FILE *fp;

  ASSERT(file && name);
  rewind(file);
  while (fgets(buffer, FILENAME_MAX, file))
  {
    sscanf(buffer, "%s\n", buffer);   /* Pick first word */
    if (!strcmp(name, buffer) || !strcmp("ALL", buffer))
    {
      struct enc_sw_open_file *n;   /* Close file node */
      if (!(n = malloc(sizeof(struct enc_sw_open_file))))
      {
        Error(2, ERR, SYSERR);
        return NULL;
      }
      if (!(fp = fopen(name, "wb")))
      {
        Error(4, ERR, name, ", ", SYSERR);
        free(n);
        return NULL;
      }
      n->file = fp;
      queue_put(&ctrl_sw_trace.files, (struct node *)n);
      return fp;
    }
  }

  return NULL;
}


#ifdef TEST_DATA
/*------------------------------------------------------------------------------
  open_stream_trace
------------------------------------------------------------------------------*/
i32 Enc_open_stream_trace(struct buffer *b)
{
  struct stream_trace *stream_trace;

  ASSERT(b);

  if (!ctrl_sw_trace.stream_trace_fp) return OK;

  stream_trace = malloc(sizeof(struct stream_trace));
  if (!stream_trace) goto error;

  memset(stream_trace, 0, sizeof(struct stream_trace));
#ifdef _WIN32
  /* TODO on windows platform*/
  int fd;

  HANDLE fm, h;
  fd = _fileno(stream_trace->fp);
  h = (HANDLE)_get_osfhandle(fd);

  fm = CreateFileMapping(h, NULL, PAGE_READWRITE | SEC_RESERVE, 0, 16 * 1024 * 1024, NULL);
  if (fm == NULL)
  {
	  fprintf(stderr, "Could not access memory space! %sn", strerror(GetLastError()));
	  exit(GetLastError());
  }

  GetFileSize(h, stream_trace->size);

  stream_trace->buffer = MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (stream_trace->buffer = NULL)
  {
	  fprintf(stderr, "Could not fill memory space! %sn", strerror(GetLastError()));
	  exit(GetLastError());
  }

#else
  stream_trace->fp = open_memstream(&stream_trace->buffer,
                                    &stream_trace->size);
#endif
  if (!stream_trace->fp) goto error;

  b->stream_trace = stream_trace;
  fprintf(stream_trace->fp, "Next buffer\n");
  queue_put(&ctrl_sw_trace.stream_trace, (struct node *)stream_trace);
  return OK;

error:
  free(stream_trace);
  return NOK;
}

/*------------------------------------------------------------------------------
  close_stream_trace
------------------------------------------------------------------------------*/
i32 Enc_close_stream_trace(void)
{
  struct stream_trace *stream_trace;
  struct node *n;
  size_t cnt;
  i32 ret = OK;

  while ((n = queue_get(&ctrl_sw_trace.stream_trace)))
  {
    stream_trace = (struct stream_trace *)n;
    fclose(stream_trace->fp);
    cnt = fwrite(stream_trace->buffer, sizeof(char),
                 stream_trace->size, ctrl_sw_trace.stream_trace_fp);
    fflush(ctrl_sw_trace.stream_trace_fp);
    if (cnt != stream_trace->size)
    {
      Error(2, ERR, "write_stream_trace()");
      ret = NOK;
    }
    free(stream_trace->buffer);
    free(stream_trace);
  }
  return ret;
}


/*------------------------------------------------------------------------------
  Enc_add_comment
------------------------------------------------------------------------------*/
void Enc_add_comment(struct buffer *b, i32 value, i32 number, char *comment)
{
  FILE *fp;
  i32 i;

  if (!b->stream_trace) return;

  fp = b->stream_trace->fp;
  if (!comment)
  {
    fprintf(fp, "      %4i%2i ", value, number);
    comment = b->stream_trace->comment;
  }
  else
  {
    fprintf(fp, "%6i    %02X ", b->stream_trace->cnt, value);
    b->stream_trace->cnt++;
  }

  if (buffer_full(b))
    comment = "FAIL: BUFFER FULL";

  for (i = number; i; i--)
  {
    if (value & (1 << (i - 1)))
    {
      fprintf(fp, "1");
    }
    else
    {
      fprintf(fp, "0");
    }
  }
  for (i = number; i < 10; i++)
  {
    fprintf(fp, " ");
  }
  fprintf(fp, "%s\n", comment);
  b->stream_trace->comment[0] = '\0';
}
#endif

void EncTraceUpdateStatus()
{
  //Update trace frame count
  ctrl_sw_trace.cur_frame_idx ++;
  ctrl_sw_trace.bTraceCurFrame = ((ctrl_sw_trace.trace_frame_id == -1) || (ctrl_sw_trace.trace_frame_id == ctrl_sw_trace.cur_frame_idx));
}


/*------------------------------------------------------------------------------
  write
------------------------------------------------------------------------------*/
static void write(FILE *fp, u8 *data, i32 width, i32 height, i32 stripe)
{
  i32 i;

  for (i = 0; i < height; i++)
  {
    if (fwrite(data, sizeof(u8), width, fp) < (size_t)width)
    {
      Error(2, ERR, SYSERR);
      return;
    }
    data += stripe;
  }
}

/*------------------------------------------------------------------------------

    EncTraceRegs

------------------------------------------------------------------------------*/
void EncTraceRegs(const void *ewl, u32 readWriteFlag, u32 mbNum)
{
  i32 i;
  i32 lastRegAddr = ASIC_SWREG_AMOUNT * 4; //0x48C;
  char rw = 'W';
  static i32 frame = 0;

  if (fRegs == NULL)
    fRegs = fopen("sw_reg.trc", "w");

  if (fRegs == NULL)
    fRegs = stderr;

  fprintf(fRegs, "pic=%d\n", frame);
  fprintf(fRegs, "mb=%d\n", mbNum);

  /* After frame is finished, registers are read */
  if (readWriteFlag)
  {
    rw = 'R';
    frame++;
  }

  /* Dump registers in same denali format as the system model */
  for (i = 0; i < lastRegAddr; i += 4)
  {
    /* DMV penalty tables not possible to read from ASIC: 0x180-0x27C */
    //if ((i != 0xA0) && (i != 0x38) && (i < 0x180 || i > 0x27C))
    if (i != HSWREG(ASIC_REG_INDEX_STATUS))
      fprintf(fRegs, "%c %08x/%08x\n", rw, i, EWLReadReg(ewl, i));
  }

  /* Regs with enable bits last, force encoder enable high for frame start */
  fprintf(fRegs, "%c %08x/%08x\n", rw, HSWREG(ASIC_REG_INDEX_STATUS),
          EWLReadReg(ewl, HSWREG(ASIC_REG_INDEX_STATUS)) | (readWriteFlag == 0));

  fprintf(fRegs, "\n");

  /*fclose(fRegs);
   * fRegs = NULL; */

}

/*------------------------------------------------------------------------------
  EncTraceReferences
------------------------------------------------------------------------------*/
void EncTraceReferences(struct container *c, struct sw_picture *pic, int pass)
{
    struct sw_picture *p;
    struct rps *r = pic->rps;
    i32 idx = 0, i = 0;
    i32 poc[VCENC_MAX_REF_FRAMES];
    int cnt = r->before_cnt + r->after_cnt + r->follow_cnt + r->lt_current_cnt + r->lt_follow_cnt;
    if (cnt > VCENC_MAX_REF_FRAMES)
      cnt = VCENC_MAX_REF_FRAMES;
    if(pass && pass != ctrl_sw_trace.trace_pass)
      return;

    pic->trace_pic_cnt = ctrl_sw_trace.cur_frame_enqueue_idx; 
    HEVCIOBufferIdx = ctrl_sw_trace.cur_frame_enqueue_idx % ctrl_sw_trace.parallelCoreNum;
    ctrl_sw_trace.cur_frame_enqueue_idx++;

    for (i = 0; i < r->before_cnt; i++)
      poc[idx ++] = r->before[i];

    for (i = 0; i < r->after_cnt; i++)
      poc[idx ++] = r->after[i];

    for (i = 0; i < r->follow_cnt; i++)
      poc[idx ++] = r->follow[i];

    for (i = 0; i < r->lt_current_cnt; i++)
      poc[idx ++] = r->lt_current[i];

    for (i = 0; i < r->lt_follow_cnt; i++)
      poc[idx ++] = r->lt_follow[i];

    idx = 0;
    for (i = 0; i < cnt; i++)
    {
      if ((p = get_picture(c, poc[i])))
      {
        HEVCRefBufferRecord[idx ++] = p->recon.lum;
        HEVCRefBufferRecord[idx ++] = p->recon.cb;
        HEVCRefBufferRecord[idx ++] = p->recon_4n_base;
        HEVCRefBufferRecord[idx ++] = p->recon_compress.lumaCompressed ? p->recon_compress.lumaTblBase : 0;
        HEVCRefBufferRecord[idx ++] = p->recon_compress.chromaCompressed ? p->recon_compress.chromaTblBase : 0;
        HEVCRefBufferRecord[idx ++] = p->trace_pic_cnt;
      }
    }
    HEVCRefBufferRecord[idx] = 0;
}

void EncTraceReconEnd()
{
  if(fRecon)
  {
    fclose(fRecon);
    fRecon = NULL;
  }
}


/*------------------------------------------------------------------------------
  SwapSWBytes()       For each two bytes swap byte positions
------------------------------------------------------------------------------*/
void SwapSWBytes(u8 * buf, u32 bytes)
{
    u32 i;

    for (i = 0; i < bytes; i += 2)
    {
        u8 val = buf[i];
        buf[i] = buf[i+1];
        buf[i+1] = val;
    }
}

/*------------------------------------------------------------------------------
  SwapSW16()          For each 32-bits swap 16-bit word positions
------------------------------------------------------------------------------*/
void SwapSW16(u16 * buf, u32 bytes)
{
    u32 i, words = (bytes + 1) / 2;

    for (i = 0; i < words; i += 2)
    {
        u16 val = buf[i];
        buf[i] = buf[i+1];
        buf[i+1] = val;
    }
}

/*------------------------------------------------------------------------------
  SwapSW32()          For each 64-bits swap 32-bit word positions
------------------------------------------------------------------------------*/
void SwapSW32(u32 * buf, u32 bytes)
{
    u32 i, words = (bytes + 3) / 4;

    for (i = 0; i < words; i += 2)
    {
        u32 val = buf[i];
        buf[i] = buf[i+1];
        buf[i+1] = val;
    }
}

/*------------------------------------------------------------------------------
  SwapSW64()          For each 128-bits swap 64-bit word positions
------------------------------------------------------------------------------*/
void SwapSW64(u64 * buf, u64 bytes)
{
    u64 i, words = (bytes + 7) / 8;

    for (i = 0; i < words; i += 2)
    {
        u64 val = buf[i];
        buf[i] = buf[i+1];
        buf[i+1] = val;
    }
}
/*
  copy to Pel from src, only srcValidBitsDepth valid in src
 */

static void memcpyToPelByBits (u8 *dst_u8, u8 *src, int width, int height, int dst_stride, int src_stride, int srcValidBitsDepth)
{
  int i, j;
  u8 bitsDepth = srcValidBitsDepth;
  u32 mask = (1<<bitsDepth)-1;
  u16 *dst = (u16*)dst_u8;

  //fast for byte write
  if (bitsDepth == 8)
  {
    for (j = 0; j < height; j ++)
    {
      for (i = 0; i< width; i ++)
        dst_u8[i] = src[i] & 0xff;
    
      src += src_stride;
      dst_u8 += dst_stride;
    }
    return;
  }

  //for arbitrary bits
  for (j = 0; j < height; j ++)
  {
    u8 *src_line = src;
    u32 cache = 0, cache_bits = 0;
    for (i = 0; i< width; i ++)
    {
      while(cache_bits < bitsDepth)
      {
        cache |= ((*src_line++) << cache_bits);
        cache_bits += 8;
      }

      if (cache_bits >= bitsDepth)
      {
        dst[i] = cache & mask;
        cache >>= bitsDepth;
        cache_bits -= bitsDepth;
      }
    }

    src += src_stride;
    dst += dst_stride;
  }
}

static void trace_recon_tile2raster(FILE *fRecon, u8 *mem, int width, int height,
                                    int leftOffset, int topOffset,
                                    int src_stride, int pixDepth)
{
  int i;
  int bytesPerPix = (pixDepth == 8 ? 1 : 2);
  u8 *tmp_mem = (u8 *)malloc(bytesPerPix*src_stride);
  int num_tiles = (leftOffset + width + 3) / 4 - leftOffset/4;
  if(tmp_mem == NULL)
    return;
  mem += topOffset/4*4*src_stride + leftOffset/4*16*pixDepth/8;
  for(i = topOffset/4*4; i < topOffset + height; i += 4) {
    memcpyToPelByBits(tmp_mem, mem, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
    if(i >= topOffset && i < topOffset + height)
      write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
    memcpyToPelByBits(tmp_mem, mem+4*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
    if(i+1 >= topOffset && i+1 < topOffset + height)
      write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
    memcpyToPelByBits(tmp_mem, mem+8*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
    if(i+2 >= topOffset && i+2 < topOffset + height)
      write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
    memcpyToPelByBits(tmp_mem, mem+12*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
    if(i+3 >= topOffset && i+3 < topOffset + height)
      write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
    mem += 4*src_stride;
  }
  free(tmp_mem);
}

static void trace_recon_tile2raster_uv(FILE *fRecon, u8 *mem_uv, int width, int height,
                                    int leftOffset, int topOffset,
                                    int src_stride, int pixDepth)
{
  int i, j, k;
  int bytesPerPix = (pixDepth == 8 ? 1 : 2);
  u8 *tmp_mem = (u8 *)malloc(bytesPerPix*src_stride*2);
  int num_tiles = 2*((leftOffset + width + 3) / 4 - leftOffset/4);
  if(tmp_mem == NULL)
    return;
  u8 *mem;
  int uv;
  for(uv = 0; uv <= 1; uv ++) {
    mem = mem_uv + topOffset/4*8*src_stride + leftOffset/4*32*pixDepth/8;
    for(i = topOffset/4*4; i < topOffset + height; i += 4) {
      if(i >= topOffset && i < topOffset + height) {
        memcpyToPelByBits(tmp_mem, mem, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
        for(j = uv, k = 0; j < 4 * num_tiles; j += 2, k++) {
          if(bytesPerPix == 1)
            tmp_mem[k] = tmp_mem[j];
          else
            ((u16*)tmp_mem)[k] = ((u16*)tmp_mem)[j];
        }
        write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
      }
      if(i+1 >= topOffset && i+1 < topOffset + height) {
        memcpyToPelByBits(tmp_mem, mem+4*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
        for(j = uv, k = 0; j < 4 * num_tiles; j += 2, k++) {
          if(bytesPerPix == 1)
            tmp_mem[k] = tmp_mem[j];
          else
            ((u16*)tmp_mem)[k] = ((u16*)tmp_mem)[j];
        }
        write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
      }
      if(i+2 >= topOffset && i+2 < topOffset + height) {
        memcpyToPelByBits(tmp_mem, mem+8*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
        for(j = uv, k = 0; j < 4 * num_tiles; j += 2, k++) {
          if(bytesPerPix == 1)
            tmp_mem[k] = tmp_mem[j];
          else
            ((u16*)tmp_mem)[k] = ((u16*)tmp_mem)[j];
        }
        write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
      }
      memcpyToPelByBits(tmp_mem, mem+12*pixDepth/8, 4, num_tiles, 4, 16*pixDepth/8, pixDepth);
      if(i+3 >= topOffset && i+3 < topOffset + height) {
        for(j = uv, k = 0; j < 4 * num_tiles; j += 2, k++) {
          if(bytesPerPix == 1)
            tmp_mem[k] = tmp_mem[j];
          else
            ((u16*)tmp_mem)[k] = ((u16*)tmp_mem)[j];
        }
        write(fRecon, tmp_mem + (leftOffset & 3)*bytesPerPix, width*bytesPerPix, 1, src_stride);
      }
      mem += 8*src_stride;
    }
  }
  free(tmp_mem);
}
int EncTraceRecon(VCEncInst inst, i32 poc, char *f_recon)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  struct container *c;
  struct sw_picture *pic;
  asicData_s *asic = &vcenc_instance->asic;
  i32 stride_lum, stride_chroma;
  u8 *lum_mem, *cb_mem, *cr_mem;

  if (fRecon == NULL)
    fRecon = fopen(f_recon, "wb");
  if (!fRecon) return 0;

  c = get_container(vcenc_instance);
  pic = get_picture(c, poc);
  if (!pic) return 0;

  if (vcenc_instance->rateControl.frameCoded == ENCHW_NO)
  {
    printf("frame skipped \n");
    return 1;
  }

  lum_mem = (u8 *)(asic->internalreconLuma[pic->picture_memeory_id].virtualAddress);
  cb_mem = (u8 *)(asic->internalreconChroma[pic->picture_memeory_id].virtualAddress);
  cr_mem = ((u8 *)(asic->internalreconChroma[pic->picture_memeory_id].virtualAddress)) + asic->regs.recon_chroma_half_size;
  stride_lum = pic->recon.lum_width;
  stride_chroma = pic->recon.ch_width;

  int lumPixDepth = pic->sps->bit_depth_luma_minus8 + 8;
  int chPixDepth  = pic->sps->bit_depth_chroma_minus8 + 8;
  if (vcenc_instance->asic.regs.P010RefEnable) {
    if(lumPixDepth > 8 && !pic->recon_compress.lumaCompressed)
      lumPixDepth = 16;
    if(chPixDepth > 8 && !pic->recon_compress.chromaCompressed)
      chPixDepth = 16;
  }
#ifdef TRACE_RECON_BYSYSTEM
extern void trace_recon_yuv(FILE *fRecon, u8 *lum_mem, u8 *cb_mem, u8 *cr_mem,
                           int width, int height,
                           int leftOffset, int topOffset,
                           int stride_lum, int stride_chroma, 
                           int lumPixDepth, int chPixDepth,
                           int lum_compressed, int ch_compressed,
                           u8 *lum_table, u8 *chroma_table, int log2_size, u32 chromaFormatIdc);


  int stride_bytes_lum = pic->recon.lum_width * lumPixDepth / 8;
  int stride_bytes_chroma = pic->recon.ch_width * chPixDepth / 8;
  u8 *lum_table = NULL;
  u8 *chroma_table = NULL;

  if (pic->recon_compress.lumaCompressed)
    lum_table = (u8 *)(asic->compressTbl[pic->picture_memeory_id].virtualAddress);
 
  if ((pic->recon_compress.chromaCompressed) && (pic->sps->chroma_format_idc > 0))
  {
    u32 lumaTblSize = 0;
    if (pic->recon_compress.lumaCompressed)
    {
      lumaTblSize = ((pic->sps->width + 63) / 64) * ((pic->sps->height + 63) / 64) * 8;
      lumaTblSize = ((lumaTblSize + 15) >> 4) << 4;
    }
    chroma_table = ((u8 *)(asic->compressTbl[pic->picture_memeory_id].virtualAddress)) + lumaTblSize;
  }

  trace_recon_yuv(fRecon, lum_mem, cb_mem, cr_mem, pic->sps->width, pic->sps->height,
                  pic->sps->frameCropLeftOffset, pic->sps->frameCropTopOffset,
                  pic->recon.lum_width, pic->recon.ch_width,
                  lumPixDepth, chPixDepth,
                  pic->recon_compress.lumaCompressed, pic->recon_compress.chromaCompressed,
                  lum_table, chroma_table, IS_H264(vcenc_instance->codecFormat) ? 4 : 3, pic->sps->chroma_format_idc);

#else
  int offset_lum = (pic->sps->frameCropTopOffset * 2) * stride_lum + (pic->sps->frameCropLeftOffset * 2);
  int offset_chroma = pic->sps->frameCropTopOffset * stride_chroma + pic->sps->frameCropLeftOffset;
  int width = pic->sps->width;
  int height = pic->sps->height;

  if(!pic->recon_compress.lumaCompressed && !pic->recon_compress.chromaCompressed && vcenc_instance->rasterscan) {
    // byte swap
    u32 lum_sz = (2*pic->sps->frameCropTopOffset + height + 3) / 4 * pic->recon.lum_width * 4;
    u32 chr_sz = (pic->sps->frameCropTopOffset + height/2 + 1) / 2 * pic->recon.ch_width * 4;
    SwapSWBytes(lum_mem, lum_sz);
    SwapSWBytes(cb_mem, chr_sz);
    SwapSW16((u16*)lum_mem, lum_sz);
    SwapSW16((u16*)cb_mem, chr_sz);
    SwapSW32((u32*)lum_mem, lum_sz);
    SwapSW32((u32*)cb_mem, chr_sz);
    SwapSW64((u64*)lum_mem, lum_sz);
    SwapSW64((u64*)cb_mem, chr_sz);
    trace_recon_tile2raster(fRecon, lum_mem, width, height,
        2*pic->sps->frameCropLeftOffset, 2*pic->sps->frameCropTopOffset,
        pic->recon.lum_width, lumPixDepth);
    width = (width + 1) >> 1;
    height = (height + 1) >> 1;
    trace_recon_tile2raster_uv(fRecon, cb_mem, width, height,
        pic->sps->frameCropLeftOffset, pic->sps->frameCropTopOffset,
        pic->recon.ch_width, chPixDepth);
    SwapSWBytes(lum_mem, lum_sz);
    SwapSWBytes(cb_mem, chr_sz);
    SwapSW16((u16*)lum_mem, lum_sz);
    SwapSW16((u16*)cb_mem, chr_sz);
    SwapSW32((u32*)lum_mem, lum_sz);
    SwapSW32((u32*)cb_mem, chr_sz);
    SwapSW64((u64*)lum_mem, lum_sz);
    SwapSW64((u64*)cb_mem, chr_sz);
  } else {
    write(fRecon, lum_mem + offset_lum, width, height, stride_lum);

    if(vcenc_instance->sps->chroma_format_idc > 0){
      width = (width + 1) >> 1;
      height = (height + 1) >> 1;
      write(fRecon, cb_mem + offset_chroma, width, height, stride_chroma);
      write(fRecon, cr_mem + offset_chroma, width, height, stride_chroma);
    }
  }
#endif

  return 1;
}

