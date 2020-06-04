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
--------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif
#include "base_type.h"
#include "error.h"
#include "hevcencapi.h"
#include "HevcTestBench.h"
#ifdef TEST_DATA
#include "enctrace.h"
#endif
#ifdef INTERNAL_TEST
#include "sw_test_id.h"
#endif
#include "encinputlinebuffer.h"
#include "enccommon.h"
#include "test_bench_utils.h"
#include "get_option.h"

#include "test_bench_pic_config.h"

#include "av1_obu.h"

#ifdef _WIN32
typedef unsigned long(__stdcall * THREAD_ROUTINE)(void * Argument);
#endif

#define HEVCERR_OUTPUT stdout
#define MAX_GOP_LEN 300

/*The 3 defines below should be reconsidered. */
#ifndef CUTREE_BUFFER_CNT
#define CUTREE_BUFFER_CNT(depth) ((depth)+MAX_GOP_SIZE-1)
#endif
#ifndef LEAST_MONITOR_FRAME
#define LEAST_MONITOR_FRAME       3
#endif
#ifndef ENCH2_SLICE_READY_INTERRUPT
#define ENCH2_SLICE_READY_INTERRUPT                      1
#endif

static void *thread_main(void *arg);
static int run_instance(commandLine_s  *cml);
static i32 encode(struct test_bench *tb, VCEncInst encoder, commandLine_s *cml);
static int OpenEncoder(commandLine_s *cml, VCEncInst *pEnc, struct test_bench *tb);
static void CloseEncoder(VCEncInst encoder, struct test_bench *tb);
static int AllocRes(commandLine_s *cmdl, VCEncInst enc, struct test_bench *tb);
static void FreeRes(VCEncInst enc, struct test_bench *tb);
static void HEVCSliceReady(VCEncSliceReady *slice);
static i32 CheckArea(VCEncPictureArea *area, commandLine_s *cml);
static i32 InitInputLineBuffer(inputLineBufferCfg * lineBufCfg, commandLine_s * cml, VCEncIn * encIn, VCEncInst inst, struct test_bench *tb);
static void EncStreamSegmentReady(void *cb_data);

/*-----------
  * main
  *-----------*/
int main(i32 argc, char **argv)
{
  i32 ret = OK;
  commandLine_s  cml, *pcml;
  VCEncApiVersion apiVer;
  VCEncBuild encBuild;
  encBuild.swBuild = encBuild.hwBuild = 0;
#ifndef _WIN32
  pthread_attr_t attr;
  pthread_t tid;
#else
  DWORD threadID;
  HANDLE tid = 0;
#endif
  int pid;
  int status;
  int i;

  apiVer = VCEncGetApiVersion();

  fprintf(stdout, "HEVC Encoder API version %d.%d\n", apiVer.major,
          apiVer.minor);

  for (i = 0; i< EWLGetCoreNum(); i++)
  {
    encBuild = VCEncGetBuild(i);
    fprintf(stdout, "HW ID:  0x%08x\t SW Build: %u.%u.%u\n",
          encBuild.hwBuild, encBuild.swBuild / 1000000,
          (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
  }

  /*
    * Parse parameter
    */
  if (argc < 2)
  {
    help(argv[0]);
    exit(0);
  }

  default_parameter(&cml);

  if (Parameter_Get(argc, argv, &cml))
  {
    Error(2, ERR, "Input parameter error");
    return NOK;
  }

  /*
    * Multiple encoders in multiple processor/threads  testing
    * User could create multiple encoders getting from different input parameters files
    *
    */
  /*Multi-processor/threads encoder enabled*/
  if(cml.nstream > 0) {
    if(cml.multimode == 1) {
      /* multi thread */
      for(i = 0; i < cml.nstream; i++) {
        pcml = cml.streamcml[i] = (commandLine_s*)malloc(sizeof(commandLine_s));
        parse_stream_cfg(cml.streamcfg[i], pcml);
#ifndef _WIN32
        pthread_attr_init(&attr);
        pthread_create(&tid, &attr, &thread_main, pcml);
        cml.tid[i] = tid;
#else
	tid = CreateThread(NULL, 0, (THREAD_ROUTINE)thread_main, (void*)pcml, 0, &threadID);
	cml.tid[i] = threadID;
#endif // !_WIN32
      }
    } else if(cml.multimode == 2) {
      /* multi process */
      for(i = 0; i < cml.nstream; i++) {
        pcml = cml.streamcml[i] = (commandLine_s*)malloc(sizeof(commandLine_s));
        parse_stream_cfg(cml.streamcfg[i], pcml);
        pcml->recon = malloc(255);
        sprintf(pcml->recon, "deblock%d.yuv", i+1);
#ifndef _WIN32
        if(0 == (pid = fork())) {
          return run_instance(pcml);
        } else if(pid > 0) {
          cml.pid[i] = pid;
        } else {
          perror("failed to fork new process to process streams");
          exit(pid);
        }
#else
	tid = CreateThread(NULL, 0, (THREAD_ROUTINE)run_instance, (void*)pcml, 0, &threadID);
	cml.pid[i] = threadID;
#endif
      }
    } else {
      if(cml.multimode == 0) {
        printf("multi-stream disabled, ignore extra stream configs\n");
      } else {
        printf("Invalid multi stream mode\n");
        exit(-1);
      }
    }
  }
  cml.argc = argc;
  cml.argv = argv;
  ret = run_instance(&cml);
  if(cml.multimode == 1) {
    for(i = 0; i < cml.nstream; i++)
      if(cml.tid[i] > 0)
#ifndef _WIN32
        pthread_join(cml.tid[i],NULL);
#else
	WaitForSingleObject(tid, INFINITE);
	CloseHandle(tid);
#endif
  }
  else if(cml.multimode == 2) {
    for(i = 0; i < cml.nstream; i++)
      if(cml.pid[i] > 0)
#ifndef _WIN32
        waitpid(cml.pid[i], &status, 0);
#else
	WaitForSingleObject(tid, INFINITE);
	CloseHandle(tid);
#endif
  }
  if(cml.multimode != 0) {
    for(i = 0; i < cml.nstream; i++)
      free(cml.streamcml[i]);
  }

  return ret;
}

void *thread_main(void *arg) {
  run_instance((commandLine_s *)arg);
#ifndef _WIN32
  pthread_exit(NULL);
#else
  ExitThread(0);
#endif
}

int run_instance(commandLine_s  *cml) {

  struct test_bench  tb;
  VCEncInst hevc_encoder;
  VCEncGopPicConfig gopPicCfg[MAX_GOP_PIC_CONFIG_NUM];
  VCEncGopPicConfig gopPicCfgPass2[MAX_GOP_PIC_CONFIG_NUM];
  VCEncGopPicSpecialConfig gopPicSpecialCfg[MAX_GOP_SPIC_CONFIG_NUM];
  i32 ret = OK;

  /* Check that input file exists */
  tb.yuvFile = fopen(cml->input, "rb");

  if (tb.yuvFile == NULL) {
    fprintf(HEVCERR_OUTPUT, "Unable to open input file: %s\n", cml->input);
    return -1;
  }
  else
  {
    fclose(tb.yuvFile);
    tb.yuvFile = NULL;
  }

  /* Check overlay input exist*/
  if(cml->overlayEnables){
    FILE *tmp_file;
    int i;
    for(i = 0; i<MAX_OVERLAY_NUM;i++)
    {
      if((cml->overlayEnables >> i) & 1)
      {
        tmp_file = fopen(cml->olInput[i], "rb");
        if(tmp_file == NULL){
          fprintf(HEVCERR_OUTPUT, "Unable to open overlay input file %d: %s\n", i+1, cml->olInput[i]);
          return -1;
        }else{
          fclose(tmp_file);
        }
      }
    }
  }

  if(cml->lookaheadDepth && cml->halfDsInput) {
    FILE *dsFile = fopen(cml->halfDsInput, "rb");
    if (dsFile == NULL) {
      fprintf(HEVCERR_OUTPUT, "Unable to open downsample input file: %s\n", cml->input);
      return -1;
    }
    else
    {
      fclose(dsFile);
      dsFile = NULL;
    }
  }

  memset(&tb, 0, sizeof(struct test_bench));
  tb.argc = cml->argc;
  tb.argv = cml->argv;

  /* the number of output stream buffers */
  tb.streamBufNum = cml->streamBufChain ? 2 : 1;

  /* GOP configuration */
  tb.gopSize = MIN(cml->gopSize, MAX_GOP_SIZE);
  if (tb.gopSize==0 && cml->gopLowdelay)
  {
    tb.gopSize = 4;
  }
  memset (gopPicCfg, 0, sizeof(gopPicCfg));
  tb.encIn.gopConfig.pGopPicCfg = gopPicCfg;
  memset(gopPicSpecialCfg, 0, sizeof(gopPicSpecialCfg));
  tb.encIn.gopConfig.pGopPicSpecialCfg = gopPicSpecialCfg;
  if (InitGopConfigs (tb.gopSize, cml, &(tb.encIn.gopConfig), tb.encIn.gopConfig.gopCfgOffset, HANTRO_FALSE) != 0)
  {
    return -ret;
  }
  if(cml->lookaheadDepth) {
    memset (gopPicCfgPass2, 0, sizeof(gopPicCfgPass2));
    tb.encIn.gopConfig.pGopPicCfg = gopPicCfgPass2;
    tb.encIn.gopConfig.size = 0;
    memset(gopPicSpecialCfg, 0, sizeof(gopPicSpecialCfg));
    tb.encIn.gopConfig.pGopPicSpecialCfg = gopPicSpecialCfg;
    cml->gopLowdelay = 0; // reset gopLowdelay for pass2
    if (InitGopConfigs (tb.gopSize, cml, &(tb.encIn.gopConfig), tb.encIn.gopConfig.gopCfgOffset, HANTRO_TRUE) != 0)
    {
      return -ret;
    }
    tb.encIn.gopConfig.pGopPicCfgPass1 = gopPicCfg;
    tb.encIn.gopConfig.pGopPicCfg = tb.encIn.gopConfig.pGopPicCfgPass2 = gopPicCfgPass2;
  }

  /* Set y and uv stride for overlay region */
  int i;
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    //Set default stride
    if(cml->olYStride[i] == 0)
    {
      switch(cml->olFormat[i])
      {
        case 0: //ARGB
          cml->olYStride[i] = cml->olWidth[i]*4;
          break;
        case 1: //NV12
          cml->olYStride[i] = cml->olWidth[i];
          break;
        case 2: //Bitmap
          cml->olYStride[i] = cml->olWidth[i]/8;
          break;
        default: cml->olYStride[i] = cml->olWidth[i];
      }
    }
    if(cml->olUVStride[i] == 0) cml->olUVStride[i] = cml->olYStride[i];

    //Set default cropping info
    if(cml->olCropWidth[i] == 0) cml->olCropWidth[i] = cml->olWidth[i];
    if(cml->olCropHeight[i] == 0) cml->olCropHeight[i] = cml->olHeight[i];

    tb.olEnable[i] = (cml->overlayEnables >> i) & 1;
  }

  /* Encoder initialization */
  if ((ret = OpenEncoder(cml, &hevc_encoder, &tb)) != 0)
  {
    return -ret;
  }
  tb.input = cml->input;
  tb.halfDsInput = cml->halfDsInput;
  tb.output     = cml->output;
  tb.dec400TableFile     = cml->dec400CompTableinput;
  tb.firstPic    = cml->firstPic;
  tb.lastPic   = cml->lastPic;
  tb.inputRateNumer      = cml->inputRateNumer;
  tb.inputRateDenom      = cml->inputRateDenom;
  tb.outputRateNumer      = cml->outputRateNumer;
  tb.outputRateDenom      = cml->outputRateDenom;
  tb.test_data_files    = cml->test_data_files;
  tb.width      = cml->width;
  tb.height     = cml->height;
  tb.input_alignment = (cml->exp_of_input_alignment==0?0:(1<<cml->exp_of_input_alignment));
  tb.ref_alignment = (cml->exp_of_ref_alignment==0?0:(1<<cml->exp_of_ref_alignment));
  tb.ref_ch_alignment = (cml->exp_of_ref_ch_alignment==0?0:(1<<cml->exp_of_ref_ch_alignment));
  tb.formatCustomizedType = cml->formatCustomizedType;
  tb.idr_interval   = cml->intraPicRate;
  tb.byteStream   = cml->byteStream;
  tb.interlacedFrame = cml->interlacedFrame;
  tb.parallelCoreNum  = cml->parallelCoreNum;
  tb.buffer_cnt = tb.frame_delay = tb.parallelCoreNum;
  if(cml->lookaheadDepth) {
    i32 delay = CUTREE_BUFFER_CNT(cml->lookaheadDepth)-1;
    tb.frame_delay += MIN(delay, cml->lastPic-cml->firstPic+1); /* lookahead depth */
    /* consider gop8->gop4 reorder: 8 4 2 1 3 6 5 7 -> 4 2 1 3 8 6 5 7
     * at least 4 more buffers are needed to avoid buffer overwrite in pass1 before consumed in pass2*/
    tb.buffer_cnt = tb.frame_delay + 4;
  }
  tb.encIn.gopConfig.idr_interval = tb.idr_interval;
  tb.encIn.gopConfig.gdrDuration = cml->gdrDuration;
  tb.encIn.gopConfig.firstPic = tb.firstPic;
  tb.encIn.gopConfig.lastPic = tb.lastPic;
  tb.encIn.gopConfig.outputRateNumer = tb.outputRateNumer;      /* Output frame rate numerator */
  tb.encIn.gopConfig.outputRateDenom = tb.outputRateDenom;      /* Output frame rate denominator */
  tb.encIn.gopConfig.inputRateNumer = tb.inputRateNumer;      /* Input frame rate numerator */
  tb.encIn.gopConfig.inputRateDenom = tb.inputRateDenom;      /* Input frame rate denominator */
  tb.encIn.gopConfig.gopLowdelay = cml->gopLowdelay;
  tb.encIn.gopConfig.interlacedFrame = tb.interlacedFrame;

  /* Set the test ID for internal testing,
   * the SW must be compiled with testing flags */
  VCEncSetTestId(hevc_encoder, cml->testId);

  /* Allocate input and output buffers */
  if (AllocRes(cml, hevc_encoder, &tb) != 0)
  {
    FreeRes(hevc_encoder, &tb);
    CloseEncoder(hevc_encoder, &tb);
    return VCENC_MEMORY_ERROR;
  }
#ifdef TEST_DATA
  Enc_test_data_init(tb.parallelCoreNum);
#endif

  ret = encode(&tb, hevc_encoder, cml);

#ifdef TEST_DATA
  Enc_test_data_release();
#endif

  FreeRes(hevc_encoder, &tb);

  CloseEncoder(hevc_encoder, &tb);

  return ret;
}

/*------------------------------------------------------------------------------
  processFrame
------------------------------------------------------------------------------*/
void processFrame(struct test_bench *tb, VCEncInst encoder, commandLine_s *cml, VCEncOut *pEncOut, SliceCtl_s *ctl,
    u64 *streamSize, u32 *maxSliceSize, ma_s *ma, u64 *total_bits, VCEncRateCtrl *rc, u32 *frameCntOutput)
{
  VCEncIn *pEncIn = &(tb->encIn);
  if(cml->lookaheadDepth && pEncOut->codingType == VCENC_INTRA_FRAME)
    (*frameCntOutput) = 0;

  //VCEncTraceProfile(encoder);

#ifdef TEST_DATA
  //show profile data, only for c-model;
  //write_recon_sw(encoder, encIn.poc);
  EncTraceUpdateStatus();
  if((cml->outReconFrame==1) && (cml->writeReconToDDR ||cml->intraPicRate != 1))
    while (EncTraceRecon(encoder, *frameCntOutput/*encIn.poc*/, cml->recon))
      (*frameCntOutput) ++;
#endif

  VCEncGetRateCtrl(encoder, rc);
  /* Write scaled encoder picture to file, packed yuyv 4:2:2 format */
  WriteScaled((u32 *)pEncOut->scaledPicture, cml);

  /* write cu encoding information to file <cuInfo.txt> */
  WriteCuInformation(tb, encoder, pEncOut, cml, tb->picture_enc_cnt-1, pEncIn->poc);

  u32 ivfHeaderSize = 0;
  if (pEncOut->streamSize != 0)
  {
    //multi-core: output bitstream has (tb->parallelCoreNum-1) delay
    i32 coreIdx = (tb->picture_enc_cnt -1 - (tb->frame_delay-1)) % tb->parallelCoreNum;
    i32 iBuf;
    for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
      tb->outbufMem[iBuf] = &(tb->outbufMemFactory[coreIdx][iBuf]);

	if(IS_AV1(cml->codecFormat)){
	  for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
          tb->Av1PreoutbufMem[iBuf] = &(tb->Av1PreoutbufMemFactory[coreIdx][iBuf]);
    }

    if((cml->streamMultiSegmentMode != 0)||(ctl->multislice_encoding==0)||(ENCH2_SLICE_READY_INTERRUPT==0)||(cml->hrdConformance==1))
    {
      VCEncStrmBufs bufs;
      getStreamBufs (&bufs, tb, cml, HANTRO_TRUE);

      if (tb->streamSegCtl.streamMultiSegEn)
      {
        u8 *streamBase = tb->streamSegCtl.streamBase + (tb->streamSegCtl.streamRDCounter % tb->streamSegCtl.segmentAmount) * tb->streamSegCtl.segmentSize;
        WriteStrm(tb->streamSegCtl.outStreamFile, (u32 *)streamBase, pEncOut->streamSize - tb->streamSegCtl.streamRDCounter * tb->streamSegCtl.segmentSize, 0);
        tb->streamSegCtl.streamRDCounter = 0;
      }
      else if (cml->byteStream == 0)
      {
        //WriteNalSizesToFile(nalfile, pEncOut->pNaluSizeBuf, pEncOut->numNalus);
        writeNalsBufs(tb->out, &bufs, pEncOut->pNaluSizeBuf, pEncOut->numNalus, 0, pEncOut->sliceHeaderSize, 0);
      }
      else
      {
        if(pEncOut->sliceHeaderSize > 0)
        {
          ASSERT(cml->tiles_enabled_flag);
          writeStrmBufs(tb->out, &bufs, pEncOut->streamSize - pEncOut->sliceHeaderSize, pEncOut->sliceHeaderSize, 0);
          writeStrmBufs(tb->out, &bufs, 0, pEncOut->streamSize - pEncOut->sliceHeaderSize, 0);
        }
        else
        {
            if(IS_VP9(cml->codecFormat) || (IS_AV1(cml->codecFormat) && cml->ivf)){
                u32 offset = 0;
                for(i32 i = 0; i < pEncOut->numNalus; i++){
                    writeIvfFrame(tb->out, &bufs, pEncOut->pNaluSizeBuf[i], offset, tb->encIn.picture_cnt);
                    offset += pEncOut->pNaluSizeBuf[i];
					ivfHeaderSize += 12;
                }
            }
            else
                writeStrmBufs(tb->out, &bufs, 0, pEncOut->streamSize, 0);
        }
      }
    }
    pEncIn->timeIncrement = tb->outputRateDenom;

    *total_bits += pEncOut->streamSize * 8;
	*total_bits += ivfHeaderSize * 8;  //IVF frame header size
    *streamSize += pEncOut->streamSize;
    tb->validencodedframenumber++;
    MaAddFrame(ma, pEncOut->streamSize*8);
    *maxSliceSize = MAX(*maxSliceSize, pEncOut->maxSliceStreamSize);
    printf("=== Encoded %i bits=%d TotalBits=%lu averagebitrate=%lu HWCycles=%d Time(us %d HW +SW)",  tb->picture_enc_cnt-1 - (tb->frame_delay-1), pEncOut->streamSize*8, (long unsigned int)(*total_bits), (long unsigned int)(*total_bits * tb->outputRateNumer) / ((tb->picture_enc_cnt - (tb->frame_delay-1)) * tb->outputRateDenom),
                                                                                                      VCEncGetPerformance(encoder), uTimeDiff(tb->timeFrameEnd, tb->timeFrameStart));
    printf(" maxSliceBytes=%d", pEncOut->maxSliceStreamSize);
    if(cml->picRc&&tb->validencodedframenumber>=ma->length)
    {
      tb->numbersquareoferror++;
      if(tb->maxerrorovertarget<(Ma(ma)- cml->bitPerSecond))
        tb->maxerrorovertarget=(Ma(ma)- cml->bitPerSecond);
      if(tb->maxerrorundertarget<(cml->bitPerSecond-Ma(ma)))
        tb->maxerrorundertarget=(cml->bitPerSecond-Ma(ma));
      tb->sumsquareoferror+=((float)(ABS(Ma(ma)- cml->bitPerSecond))*100/cml->bitPerSecond);
      tb->averagesquareoferror=(tb->sumsquareoferror/tb->numbersquareoferror);
      printf("   RateControl(movingBitrate=%d MaxOvertarget=%d%% MaxUndertarget=%d%% AveDeviationPerframe=%f%%) ",Ma(ma),tb->maxerrorovertarget*100/cml->bitPerSecond,tb->maxerrorundertarget*100/cml->bitPerSecond,tb->averagesquareoferror);
    }
    printf("\n");
  }
}


static void tb_init_pic(struct test_bench *tb, commandLine_s *cml, ma_s *ma, adapGopCtr *agop)
{
  tb->validencodedframenumber=0;

  //Adaptive Gop variables
  agop->last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
  agop->gop_frm_num = 0;
  agop->sum_intra_vs_interskip = 0;
  agop->sum_skip_vs_interskip = 0;
  agop->sum_intra_vs_interskipP = 0;
  agop->sum_intra_vs_interskipB = 0;
  agop->sum_costP = 0;
  agop->sum_costB = 0;

  ma->pos = ma->count = 0;
  ma->frameRateNumer = cml->outputRateNumer;
  ma->frameRateDenom = cml->outputRateDenom;
  if (cml->outputRateDenom)
      ma->length = MAX(LEAST_MONITOR_FRAME, MIN(cml->monitorFrames,
                              MOVING_AVERAGE_FRAMES));
  else
      ma->length = MOVING_AVERAGE_FRAMES;
}

/*------------------------------------------------------------------------------
  encode
------------------------------------------------------------------------------*/
i32 encode(struct test_bench *tb, VCEncInst encoder, commandLine_s *cml)
{
  VCEncIn *pEncIn = &(tb->encIn);
  VCEncOut encOut;
  VCEncRet ret;
  i32 encRet = NOK;
  u32 frameCntTotal = 0;
  u64 streamSize = 0;
  u32 maxSliceSize = 0;
  ma_s ma;
  i32 cnt = 1;
  u32 i, tmp;
  i32 p;
  u64 total_bits = 0;
  u8 *pUserData;
  u32 src_img_size;
  VCEncRateCtrl rc;
  u32 frameCntOutput = 0;
  u32 gopSize = tb->gopSize;
  i32 nextGopSize = 0;
  VCEncPictureCodingType nextCodingType = VCENC_INTRA_FRAME;
  bool adaptiveGop = (gopSize == 0);
  adapGopCtr agop;

  /* Output/input streams */
  if (!(tb->in = open_file(tb->input, "rb"))) goto error;
  if (!(tb->out = open_file(tb->output, "wb"))) goto error;
  if (tb->halfDsInput && !(tb->inDS = open_file(tb->halfDsInput, "rb"))) goto error;
  tb->dec400Table = fopen(tb->dec400TableFile, "rb");

  SetupOutputBuffer(tb, pEncIn);

  InitSliceCtl(tb, cml);
  InitStreamSegmentCrl(tb, cml);

  if (cml->inputLineBufMode)
  {
    if (InitInputLineBuffer(&(tb->inputCtbLineBuf), cml, pEncIn, encoder, tb))
    {
      fprintf(HEVCERR_OUTPUT, "Fail to Init Input Line Buffer: virt_addr=%p, bus_addr=%08x\n",
              tb->inputCtbLineBuf.sram, (u32)(tb->inputCtbLineBuf.sramBusAddr));
      goto error;
    }
  }

  /* before VCEncStrmStart called */
  tb_init_pic(tb, cml, &ma, &agop);
  InitPicConfig(pEncIn, tb, cml);
  nextGopSize = pEncIn->gopSize;

  /* Video, sequence and picture parameter sets */
  for (p = 0; p < cnt; p++)
  {
    if (VCEncStrmStart(encoder, pEncIn, &encOut))
    {
      Error(2, ERR, "hevc_set_parameter() fails");
      goto error;
    }

    if(IS_VP9(cml->codecFormat) || (IS_AV1(cml->codecFormat) && cml->ivf)){
      writeIvfHeader(tb->out, cml->width, cml->height, cml->outputRateNumer, cml->outputRateDenom, IS_VP9(cml->codecFormat));
	  total_bits += 32 * 8; //IVF Stream header size
	}
    VCEncStrmBufs bufs;
    getStreamBufs (&bufs, tb, cml, HANTRO_FALSE);
    if (cml->byteStream == 0)
    {
      //WriteNalSizesToFile(nalfile, encOut.pNaluSizeBuf, encOut.numNalus);
      writeNalsBufs(tb->out, &bufs, encOut.pNaluSizeBuf, encOut.numNalus, 0, 0, 0);
    }
    else
      writeStrmBufs(tb->out, &bufs, 0, encOut.streamSize, 0);

    total_bits += encOut.streamSize * 8;
    streamSize += encOut.streamSize;
  }
  VCEncGetRateCtrl(encoder, &rc);

  /* Allocate a buffer for user data and read data from file */
  pUserData = ReadUserData(encoder, cml->userData);

  /* Read configuration files for ROI/CuTree/IPCM/GMV/Overlay ... */
  if (readConfigFiles(tb, cml)) goto error;

  ChangeInputToTransYUV(tb,encoder,cml,pEncIn);

  while (HANTRO_TRUE)
  {
    /* IO buffer */
    GetFreeIOBuffer(tb);
    SetupSliceCtl(tb);

    /* Setup encoder input */
    src_img_size = SetupInputBuffer(tb, cml, pEncIn);

    SetupOutputBuffer(tb, pEncIn);

    /*Setup external SRAM if needed*/
    SetupExtSRAM(tb, pEncIn);

    /*Setup dec400 compress table if needed*/
    SetupDec400CompTable(tb, pEncIn);

    if ((cml->videoStab != DEFAULT) && (cml->videoStab))
    {
        i32 ret;
        /* read next frame for video stabilization*/
        ret = read_stab_picture(tb, cml->inputFormat, src_img_size, cml->lumWidthSrc, cml->lumHeightSrc);
        if (ret == NOK)
        {
            // TODO: should stop stab now. use old data without change
        }
        pEncIn->busLumaStab = tb->pictureStabMem.busAddress;
    }

    /* Read YUV to input buffer */
    if (read_picture(tb, cml->inputFormat, src_img_size, cml->lumWidthSrc, cml->lumHeightSrc, 0))
    {
      /* Next input stream (if any) */
      if (change_input(tb)) break;
      fclose(tb->in);
      if (!(tb->in = open_file(tb->input, "rb"))) goto error;
      tb->encIn.picture_cnt = 0;
      tb->picture_enc_cnt = 0;
      tb->validencodedframenumber=0;
      continue;
    }
    if (tb->halfDsInput && read_picture(tb, cml->inputFormat, tb->src_img_size_ds, cml->lumWidthSrc/2, cml->lumHeightSrc/2, 1)) {
      Error(2, ERR, "read down-sampled input fails");
      goto error;
    }

    /*if dec400 is used, read dec400 data from file*/
    if (tb->dec400Table == NULL)
    {
      pEncIn->dec400Enable = 0;
    }
    else if (read_dec400Data(tb,cml->inputFormat,cml->lumWidthSrc, cml->lumHeightSrc) == NOK)
    {
      printf("DEC400 data read fail!!\n");
      goto error;
    }
    else
      pEncIn->dec400Enable = 1;

    frameCntTotal++;

    /* Format to some specific customized format*/
    FormatCustomizedYUV(tb, cml);

    /*
      * per-frame test functions
      */

    /* 1. scene changed frames from usr*/
    pEncIn->sceneChange = 0;
    tmp = next_picture(tb, tb->encIn.picture_cnt) + tb->firstPic;
    for (i = 0; i < MAX_SCENE_CHANGE; i ++)
    {
      if (cml->sceneChange[i] == 0)
      {
        break;
      }
      if (cml->sceneChange[i] == tmp)
      {
        pEncIn->sceneChange = 1;
        printf ("    Input a Scene Changed Frame! \n");
        break;
      }
    }

    /* 2. GMV setting from user*/
    readGmv(tb, pEncIn, cml);

    pEncIn->codingType = (pEncIn->poc == 0) ? VCENC_INTRA_FRAME : nextCodingType;

    if (pEncIn->codingType == VCENC_INTRA_FRAME&&cml->gdrDuration == 0 && ((pEncIn->poc == 0)|| (pEncIn->bIsIDR)))
    {
      if(!cml->lookaheadDepth)
        frameCntOutput = 0;
    }

    /* 3. On-fly bitrate setting */
    for (i = 0; i < MAX_BPS_ADJUST; i++)
      if (cml->bpsAdjustFrame[i] &&
          (tb->encIn.picture_cnt == cml->bpsAdjustFrame[i]))
      {
        rc.bitPerSecond = cml->bpsAdjustBitrate[i];
        printf("Adjusting bitrate target: %d\n", rc.bitPerSecond);
        if ((ret = VCEncSetRateCtrl(encoder, &rc)) != VCENC_OK)
        {
          //PrintErrorValue("VCEncSetRateCtrl() failed.", ret);
        }
      }

    /* 4. SetupROI-Map */
    SetupROIMapBuffer(tb, cml, pEncIn, encoder);

    /* 5. encoding specific frame from user: all CU/MB are SKIP*/
    pEncIn->bSkipFrame = cml->skip_frame_enabled_flag && (pEncIn->poc == cml->skip_frame_poc);

    /* 6. low latency */
    if (cml->inputLineBufMode)
    {
      tb->inputCtbLineBuf.lumSrc = tb->lum;
      tb->inputCtbLineBuf.cbSrc  = tb->cb;
      tb->inputCtbLineBuf.crSrc  = tb->cr;
      pEncIn->lineBufWrCnt = VCEncStartInputLineBuffer(&(tb->inputCtbLineBuf), HANTRO_TRUE);
    }

    /* 7. Setup Overlay input buffer */
    SetupOverlayBuffer(tb, cml, pEncIn);

    gettimeofday(&tb->timeFrameStart, 0);

    printf("=== Encoding %i %s codeType=%d ...\n", tb->picture_enc_cnt, tb->input, pEncIn->codingType);

    ret = VCEncStrmEncode(encoder, pEncIn, &encOut, &HEVCSliceReady, tb->sliceCtl);

    gettimeofday(&tb->timeFrameEnd, 0);

    switch (ret)
    {
      case VCENC_FRAME_ENQUEUE:
        tb->picture_enc_cnt++;
        //Adaptive GOP size decision
        if (adaptiveGop && cml->lookaheadDepth)
          getNextGopSize(tb, pEncIn, encoder, &nextGopSize, &agop, cml->lookaheadDepth, &encOut);
        else if(cml->lookaheadDepth) // for sync only, not update gopSize
          VCEncGetPass1UpdatedGopSize(encoder);
        nextCodingType = VCEncFindNextPic (encoder, pEncIn, nextGopSize, tb->encIn.gopConfig.gopCfgOffset, false);
        pEncIn->timeIncrement = tb->outputRateDenom;
        break;
      case VCENC_FRAME_READY:
        if(encOut.codingType != VCENC_NOTCODED_FRAME)
          tb->picture_enc_cnt++;
        if (encOut.streamSize == 0)
        {
          tb->encIn.picture_cnt ++;
          break;
        }

        processFrame(tb, encoder, cml, &encOut, tb->sliceCtlOut, &streamSize, &maxSliceSize, &ma, &total_bits, &rc, &frameCntOutput);

        //Adaptive GOP size decision
        if (adaptiveGop)
          getNextGopSize(tb, pEncIn, encoder, &nextGopSize, &agop, cml->lookaheadDepth, &encOut);
        else if(cml->lookaheadDepth) // for sync only, not update gopSize
          VCEncGetPass1UpdatedGopSize(encoder);

        nextCodingType = VCEncFindNextPic (encoder, pEncIn, nextGopSize, tb->encIn.gopConfig.gopCfgOffset, false);

        if (pUserData)
        {
          /* We want the user data to be written only once so
           * we disable the user data and free the memory after
           * first frame has been encoded. */
          VCEncSetSeiUserData(encoder, NULL, 0);
          free(pUserData);
          pUserData = NULL;
        }
        break;
      case VCENC_OUTPUT_BUFFER_OVERFLOW:
        tb->encIn.picture_cnt ++;
        break;
      default:
        goto error;
        break;
    }
    if(cml->profile==VCENC_HEVC_MAIN_STILL_PICTURE_PROFILE)
      break;
  }

  /* Flush encoder internal buffered frames
    * In Multicore enabled or 2-pass enabled, encoder internal cached frames
    * must be flushed
    */
  while((ret = VCEncFlush(encoder, pEncIn, &encOut, &HEVCSliceReady)) != VCENC_OK)
  {
    switch (ret)
    {
      case VCENC_FRAME_READY:
        SetupOutputBuffer(tb, pEncIn);
        SetupSliceCtl(tb);
        tb->picture_enc_cnt++;
        if (encOut.streamSize == 0)
        {
          tb->encIn.picture_cnt ++;
          break;
        }
        processFrame(tb, encoder, cml, &encOut, tb->sliceCtlOut, &streamSize, &maxSliceSize, &ma, &total_bits, &rc, &frameCntOutput);
        break;
      case VCENC_FRAME_ENQUEUE:
        //ASSERT(0);
        continue;
      case VCENC_OUTPUT_BUFFER_OVERFLOW:
        tb->encIn.picture_cnt ++;
        break;
      default:
        goto error;
        break;
    }
  }

    /* End stream */
#ifdef TEST_DATA
    EncTraceReconEnd();
#endif

  SetupOutputBuffer(tb, pEncIn);

  ret = VCEncStrmEnd(encoder, pEncIn, &encOut);
  if (ret == VCENC_OK)
  {
    VCEncStrmBufs bufs;
    getStreamBufs (&bufs, tb, cml, HANTRO_FALSE);
    if (cml->byteStream == 0)
    {
      //WriteNalSizesToFile(nalfile, encOut.pNaluSizeBuf,encOut.numNalus);
      writeNalsBufs(tb->out, &bufs, encOut.pNaluSizeBuf, encOut.numNalus, 0, 0, 0);
    }
    else
    {
      if(IS_VP9(cml->codecFormat) || (IS_AV1(cml->codecFormat) && cml->ivf)){
        u32 offset = 0;
        for(i32 i = 0; i < encOut.numNalus; i++){
            writeIvfFrame(tb->out, &bufs, encOut.pNaluSizeBuf[i], offset, tb->encIn.picture_cnt);
            offset += encOut.pNaluSizeBuf[i];
        }
      }
      else
        writeStrmBufs(tb->out, &bufs, 0, encOut.streamSize, 0);
    }
    streamSize += encOut.streamSize;
  }
  encRet = OK;

  printf("Total of %d frames processed, %d frames encoded, %lu bytes, maxSliceBytes=%d\n",
         frameCntTotal, tb->picture_enc_cnt - (tb->frame_delay-1), (long unsigned int)streamSize, maxSliceSize);
#ifdef STATS_RDO_COUNT
  extern u32 rdo_refine_cnt[4];
  printf("rdo64=%d, rdo32=%d, rdo16=%d, rdo8=%d\n",
         rdo_refine_cnt[0], rdo_refine_cnt[1], rdo_refine_cnt[2], rdo_refine_cnt[3]);
#endif
error:

  if (tb->in) fclose(tb->in);
  if (tb->inDS) fclose(tb->inDS);
  if (tb->out) fclose(tb->out);
  if (tb->roiMapFile) fclose(tb->roiMapFile);
  if (tb->roiMapBinFile) fclose(tb->roiMapBinFile);
  if (tb->ipcmMapFile) fclose(tb->ipcmMapFile);
  if (tb->fmv) fclose(tb->fmv);
  if (tb->gmvFile[0]) fclose(tb->gmvFile[0]);
  if (tb->gmvFile[1]) fclose(tb->gmvFile[1]);
  if (tb->skipMapFile) fclose(tb->skipMapFile);
  if (tb->roiMapInfoBinFile) fclose(tb->roiMapInfoBinFile);
  if (tb->RoimapCuCtrlInfoBinFile) fclose(tb->RoimapCuCtrlInfoBinFile);
  if (tb->RoimapCuCtrlIndexBinFile) fclose(tb->RoimapCuCtrlIndexBinFile);
  if (tb->dec400Table) fclose(tb->dec400Table);
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    if (tb->overlayFile[i]) fclose(tb->overlayFile[i]);
  }

  if (encRet != OK)
    Error(2, ERR, "encode() fails");

  return encRet;
}

/*------------------------------------------------------------------------------

    OpenEncoder
        Create and configure an encoder instance.

    Params:
        cml     - processed comand line options
        pEnc    - place where to save the new encoder instance
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int OpenEncoder(commandLine_s *cml, VCEncInst *pEnc, struct test_bench *tb)
{
  VCEncRet ret;
  VCEncConfig cfg;
  VCEncCodingCtrl codingCfg;
  VCEncRateCtrl rcCfg;
  VCEncPreProcessingCfg preProcCfg;
  VCEncInst encoder;
  i32 i;
  const void *ewl_inst = NULL;

  /* Default resolution, try parsing input file name */
  if (cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
  {
    if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
    {
      /* No dimensions found in filename, using default QCIF */
      cml->lumWidthSrc = 176;
      cml->lumHeightSrc = 144;
    }
  }

  /* Encoder initialization */
  if (cml->width == DEFAULT)
    cml->width = cml->lumWidthSrc;

  if (cml->height == DEFAULT)
    cml->height = cml->lumHeightSrc;

  ChangeCmlCustomizedFormat(cml);

  /* outputRateNumer */
  if (cml->outputRateNumer == DEFAULT)
    cml->outputRateNumer = cml->inputRateNumer;

  /* outputRateDenom */
  if (cml->outputRateDenom == DEFAULT)
    cml->outputRateDenom = cml->inputRateDenom;

  /*cfg.ctb_size = cml->max_cu_size;*/
  if (cml->rotation&&cml->rotation!=3)
  {
    cfg.width = cml->height;
    cfg.height = cml->width;
  }
  else
  {
    cfg.width = cml->width;
    cfg.height = cml->height;
  }

  cfg.frameRateDenom = cml->outputRateDenom;
  cfg.frameRateNum = cml->outputRateNumer;

  /* intra tools in sps and pps */
  cfg.strongIntraSmoothing = cml->strong_intra_smoothing_enabled_flag;

  cfg.streamType = (cml->byteStream) ? VCENC_BYTE_STREAM : VCENC_NAL_UNIT_STREAM;

  cfg.level = (IS_H264(cml->codecFormat) ? VCENC_H264_LEVEL_5_1 : VCENC_HEVC_LEVEL_6);

  /* hard coded level for H.264 */
  if (cml->level != DEFAULT && cml->level != 0)
    cfg.level = (VCEncLevel)cml->level;

  cfg.tier = VCENC_HEVC_MAIN_TIER;
  if (cml->tier != DEFAULT)
    cfg.tier = (VCEncTier)cml->tier;

  cfg.profile = (IS_H264(cml->codecFormat) ? VCENC_H264_HIGH_PROFILE : VCENC_HEVC_MAIN_PROFILE);

  cfg.codecFormat = cml->codecFormat;

  if (cml->profile != DEFAULT && cml->profile != 0)
    cfg.profile = (VCEncProfile)cml->profile;

  /* convert between H.264/HEVC profiles for testing purpose */
  if(IS_H264(cml->codecFormat)) {
    if((int)cfg.profile >= VCENC_HEVC_MAIN_PROFILE && cfg.profile < VCENC_HEVC_MAIN_10_PROFILE)
      cfg.profile = VCENC_H264_HIGH_PROFILE;
  } else {
    if(cfg.profile >= VCENC_H264_BASE_PROFILE && cfg.profile <= VCENC_H264_HIGH_PROFILE)
      cfg.profile = VCENC_HEVC_MAIN_PROFILE;
  }

  cfg.bitDepthLuma = 8;
  if (cml->bitDepthLuma != DEFAULT)
    cfg.bitDepthLuma = cml->bitDepthLuma;

  cfg.bitDepthChroma= 8;
  if (cml->bitDepthChroma != DEFAULT)
    cfg.bitDepthChroma = cml->bitDepthChroma;

  if ((cml->interlacedFrame && cml->gopSize != 1) || IS_H264(cml->codecFormat))
  {
    //printf("OpenEncoder: treat interlace to progressive for gopSize!=1 case\n");
    cml->interlacedFrame = 0;
  }

  //default maxTLayer
  cfg.maxTLayers = 1;

  /* Find the max number of reference frame */
  if (cml->intraPicRate == 1)
  {
    cfg.refFrameAmount = 0;
  }
  else
  {
    u32 maxRefPics = 0;
    u32 maxTemporalId = 0;
    int idx;
    for (idx = 0; idx < tb->encIn.gopConfig.size; idx ++)
    {
      VCEncGopPicConfig *cfg = &(tb->encIn.gopConfig.pGopPicCfg[idx]);
      if (cfg->codingType != VCENC_INTRA_FRAME)
      {
        if (maxRefPics < cfg->numRefPics)
          maxRefPics = cfg->numRefPics;

        if (maxTemporalId < cfg->temporalId)
          maxTemporalId = cfg->temporalId;
      }
    }
    cfg.refFrameAmount = maxRefPics + cml->interlacedFrame + tb->encIn.gopConfig.ltrcnt;
    cfg.maxTLayers = maxTemporalId +1;
  }
  cfg.compressor = cml->compressor;
  cfg.interlacedFrame = cml->interlacedFrame;
  cfg.enableOutputCuInfo = (cml->enableOutputCuInfo > 0) ? 1 : 0;
  cfg.rdoLevel = CLIP3(1, 3, cml->rdoLevel) - 1;
  cfg.verbose = cml->verbose;
  cfg.exp_of_input_alignment = cml->exp_of_input_alignment;
  cfg.exp_of_ref_alignment = cml->exp_of_ref_alignment;
  cfg.exp_of_ref_ch_alignment = cml->exp_of_ref_ch_alignment;
  cfg.exteralReconAlloc = 0;
  cfg.P010RefEnable = cml->P010RefEnable;
  cfg.enableSsim = cml->ssim;
  cfg.ctbRcMode = (cml->ctbRc != DEFAULT) ? cml->ctbRc : 0;
  cfg.parallelCoreNum = cml->parallelCoreNum;
  cfg.pass = (cml->lookaheadDepth?2:0);
  cfg.bPass1AdaptiveGop = (cml->gopSize == 0);
  cfg.picOrderCntType = cml->picOrderCntType;
  cfg.dumpRegister = cml->dumpRegister;
  cfg.rasterscan = cml->rasterscan;
  cfg.log2MaxPicOrderCntLsb = cml->log2MaxPicOrderCntLsb;
  cfg.log2MaxFrameNum = cml->log2MaxFrameNum;
  cfg.lookaheadDepth = cml->lookaheadDepth;
  cfg.extDSRatio = (cml->lookaheadDepth && cml->halfDsInput ? 1 : 0);
  cfg.cuInfoVersion = cml->cuInfoVersion;
  cfg.extSramLumHeightBwd = cml->extSramLumHeightBwd;
  cfg.extSramChrHeightBwd = cml->extSramChrHeightBwd;
  cfg.extSramLumHeightFwd = cml->extSramLumHeightFwd;
  cfg.extSramChrHeightFwd = cml->extSramChrHeightFwd;
  cfg.AXIAlignment = cml->AXIAlignment;
  cfg.mmuEnable = cml->mmuEnable;
  if (cml->parallelCoreNum > 1 && cfg.width * cfg.height < 256*256) {
    printf("Disable multicore for small resolution (< 255*255)\n");
    cfg.parallelCoreNum = cml->parallelCoreNum = 1;
  }
  cfg.codedChromaIdc = cml->codedChromaIdc;
  cfg.aq_mode = cml->aq_mode;
  cfg.aq_strength = cml->aq_strength;
  cfg.writeReconToDDR = (cml->writeReconToDDR ||cml->intraPicRate != 1);
  cfg.TxTypeSearchEnable = cml->TxTypeSearchEnable;

  if ((ret = VCEncInit(&cfg, pEnc)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncInit() failed.", ret);
    return (int)ret;
  }

  encoder = *pEnc;
  ewl_inst = VCEncGetEwl(encoder);

  /* Encoder setup: coding control */
  if ((ret = VCEncGetCodingCtrl(encoder, &codingCfg)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncGetCodingCtrl() failed.", ret);
    CloseEncoder(encoder, tb);
    return -1;
  }
  else
  {

    if (cml->sliceSize != DEFAULT)
      codingCfg.sliceSize = cml->sliceSize;
    if (cml->enableCabac != DEFAULT)
      codingCfg.enableCabac = cml->enableCabac;
    if (cml->cabacInitFlag != DEFAULT)
      codingCfg.cabacInitFlag = cml->cabacInitFlag;
    codingCfg.videoFullRange = 0;
    if (cml->videoRange != DEFAULT)
      codingCfg.videoFullRange = cml->videoRange;
    if (cml->enableRdoQuant != DEFAULT)
      codingCfg.enableRdoQuant = cml->enableRdoQuant;
	if(cml->layerInRefIdc != DEFAULT)
	  codingCfg.layerInRefIdcEnable = cml->layerInRefIdc;

    codingCfg.disableDeblockingFilter = (cml->disableDeblocking != 0);
    codingCfg.tc_Offset = cml->tc_Offset;
    codingCfg.beta_Offset = cml->beta_Offset;
    codingCfg.enableSao = cml->enableSao;
    codingCfg.enableDeblockOverride = cml->enableDeblockOverride;
    codingCfg.deblockOverride = cml->deblockOverride;

    if (cml->sei)
      codingCfg.seiMessages = 1;
    else
      codingCfg.seiMessages = 0;

    codingCfg.gdrDuration = cml->gdrDuration;
    codingCfg.fieldOrder = cml->fieldOrder;

    codingCfg.cirStart = cml->cirStart;
    codingCfg.cirInterval = cml->cirInterval;

    if(codingCfg.gdrDuration == 0)
    {
      codingCfg.intraArea.top = cml->intraAreaTop;
      codingCfg.intraArea.left = cml->intraAreaLeft;
      codingCfg.intraArea.bottom = cml->intraAreaBottom;
      codingCfg.intraArea.right = cml->intraAreaRight;
      codingCfg.intraArea.enable = CheckArea(&codingCfg.intraArea, cml);
    }
    else
    {
      //intraArea will be used by GDR, customer can not use intraArea when GDR is enabled.
      codingCfg.intraArea.enable = 0;
    }

    codingCfg.pcm_loop_filter_disabled_flag = cml->pcm_loop_filter_disabled_flag;

    codingCfg.ipcm1Area.top = cml->ipcm1AreaTop;
    codingCfg.ipcm1Area.left = cml->ipcm1AreaLeft;
    codingCfg.ipcm1Area.bottom = cml->ipcm1AreaBottom;
    codingCfg.ipcm1Area.right = cml->ipcm1AreaRight;
    codingCfg.ipcm1Area.enable = CheckArea(&codingCfg.ipcm1Area, cml);

    codingCfg.ipcm2Area.top = cml->ipcm2AreaTop;
    codingCfg.ipcm2Area.left = cml->ipcm2AreaLeft;
    codingCfg.ipcm2Area.bottom = cml->ipcm2AreaBottom;
    codingCfg.ipcm2Area.right = cml->ipcm2AreaRight;
    codingCfg.ipcm2Area.enable = CheckArea(&codingCfg.ipcm2Area, cml);
    codingCfg.ipcmMapEnable = cml->ipcmMapEnable;
    codingCfg.pcm_enabled_flag = (codingCfg.ipcm1Area.enable || codingCfg.ipcm2Area.enable || codingCfg.ipcmMapEnable);

    if(codingCfg.gdrDuration == 0)
    {
      codingCfg.roi1Area.top = cml->roi1AreaTop;
      codingCfg.roi1Area.left = cml->roi1AreaLeft;
      codingCfg.roi1Area.bottom = cml->roi1AreaBottom;
      codingCfg.roi1Area.right = cml->roi1AreaRight;
      if (CheckArea(&codingCfg.roi1Area, cml) && (cml->roi1DeltaQp || (cml->roi1Qp >= 0)))
        codingCfg.roi1Area.enable = 1;
      else
        codingCfg.roi1Area.enable = 0;
    }
    else
    {
      codingCfg.roi1Area.enable = 0;
    }

    codingCfg.roi2Area.top = cml->roi2AreaTop;
    codingCfg.roi2Area.left = cml->roi2AreaLeft;
    codingCfg.roi2Area.bottom = cml->roi2AreaBottom;
    codingCfg.roi2Area.right = cml->roi2AreaRight;
    if (CheckArea(&codingCfg.roi2Area, cml) && (cml->roi2DeltaQp || (cml->roi2Qp >= 0)))
      codingCfg.roi2Area.enable = 1;
    else
      codingCfg.roi2Area.enable = 0;

    codingCfg.roi3Area.top = cml->roi3AreaTop;
    codingCfg.roi3Area.left = cml->roi3AreaLeft;
    codingCfg.roi3Area.bottom = cml->roi3AreaBottom;
    codingCfg.roi3Area.right = cml->roi3AreaRight;
    if (CheckArea(&codingCfg.roi3Area, cml) && (cml->roi3DeltaQp || (cml->roi3Qp >= 0)))
      codingCfg.roi3Area.enable = 1;
    else
      codingCfg.roi3Area.enable = 0;

    codingCfg.roi4Area.top = cml->roi4AreaTop;
    codingCfg.roi4Area.left = cml->roi4AreaLeft;
    codingCfg.roi4Area.bottom = cml->roi4AreaBottom;
    codingCfg.roi4Area.right = cml->roi4AreaRight;
    if (CheckArea(&codingCfg.roi4Area, cml) && (cml->roi4DeltaQp || (cml->roi4Qp >= 0)))
      codingCfg.roi4Area.enable = 1;
    else
      codingCfg.roi4Area.enable = 0;

    codingCfg.roi5Area.top = cml->roi5AreaTop;
    codingCfg.roi5Area.left = cml->roi5AreaLeft;
    codingCfg.roi5Area.bottom = cml->roi5AreaBottom;
    codingCfg.roi5Area.right = cml->roi5AreaRight;
    if (CheckArea(&codingCfg.roi5Area, cml) && (cml->roi5DeltaQp || (cml->roi5Qp >= 0)))
      codingCfg.roi5Area.enable = 1;
    else
      codingCfg.roi5Area.enable = 0;

    codingCfg.roi6Area.top = cml->roi6AreaTop;
    codingCfg.roi6Area.left = cml->roi6AreaLeft;
    codingCfg.roi6Area.bottom = cml->roi6AreaBottom;
    codingCfg.roi6Area.right = cml->roi6AreaRight;
    if (CheckArea(&codingCfg.roi6Area, cml) && (cml->roi6DeltaQp || (cml->roi6Qp >= 0)))
      codingCfg.roi6Area.enable = 1;
    else
      codingCfg.roi6Area.enable = 0;

    codingCfg.roi7Area.top = cml->roi7AreaTop;
    codingCfg.roi7Area.left = cml->roi7AreaLeft;
    codingCfg.roi7Area.bottom = cml->roi7AreaBottom;
    codingCfg.roi7Area.right = cml->roi7AreaRight;
    if (CheckArea(&codingCfg.roi7Area, cml) && (cml->roi7DeltaQp || (cml->roi7Qp >= 0)))
      codingCfg.roi7Area.enable = 1;
    else
      codingCfg.roi7Area.enable = 0;

    codingCfg.roi8Area.top = cml->roi8AreaTop;
    codingCfg.roi8Area.left = cml->roi8AreaLeft;
    codingCfg.roi8Area.bottom = cml->roi8AreaBottom;
    codingCfg.roi8Area.right = cml->roi8AreaRight;
    if (CheckArea(&codingCfg.roi8Area, cml) && (cml->roi8DeltaQp || (cml->roi8Qp >= 0)))
      codingCfg.roi8Area.enable = 1;
    else
      codingCfg.roi8Area.enable = 0;

    codingCfg.roi1DeltaQp = cml->roi1DeltaQp;
    codingCfg.roi2DeltaQp = cml->roi2DeltaQp;
    codingCfg.roi3DeltaQp = cml->roi3DeltaQp;
    codingCfg.roi4DeltaQp = cml->roi4DeltaQp;
    codingCfg.roi5DeltaQp = cml->roi5DeltaQp;
    codingCfg.roi6DeltaQp = cml->roi6DeltaQp;
    codingCfg.roi7DeltaQp = cml->roi7DeltaQp;
    codingCfg.roi8DeltaQp = cml->roi8DeltaQp;
    codingCfg.roi1Qp = cml->roi1Qp;
    codingCfg.roi2Qp = cml->roi2Qp;
    codingCfg.roi3Qp = cml->roi3Qp;
    codingCfg.roi4Qp = cml->roi4Qp;
    codingCfg.roi5Qp = cml->roi5Qp;
    codingCfg.roi6Qp = cml->roi6Qp;
    codingCfg.roi7Qp = cml->roi7Qp;
    codingCfg.roi8Qp = cml->roi8Qp;

    if (codingCfg.cirInterval)
      printf("  CIR: %d %d\n",
             codingCfg.cirStart, codingCfg.cirInterval);

    if (codingCfg.intraArea.enable)
      printf("  IntraArea: %dx%d-%dx%d\n",
             codingCfg.intraArea.left, codingCfg.intraArea.top,
             codingCfg.intraArea.right, codingCfg.intraArea.bottom);

    if (codingCfg.ipcm1Area.enable)
      printf("  IPCM1Area: %dx%d-%dx%d\n",
             codingCfg.ipcm1Area.left, codingCfg.ipcm1Area.top,
             codingCfg.ipcm1Area.right, codingCfg.ipcm1Area.bottom);

    if (codingCfg.ipcm2Area.enable)
      printf("  IPCM2Area: %dx%d-%dx%d\n",
             codingCfg.ipcm2Area.left, codingCfg.ipcm2Area.top,
             codingCfg.ipcm2Area.right, codingCfg.ipcm2Area.bottom);

    if (codingCfg.roi1Area.enable)
      printf("  ROI 1: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi1Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi1Qp >= 0 ? codingCfg.roi1Qp : codingCfg.roi1DeltaQp,
             codingCfg.roi1Area.left, codingCfg.roi1Area.top,
             codingCfg.roi1Area.right, codingCfg.roi1Area.bottom);

    if (codingCfg.roi2Area.enable)
      printf("  ROI 2: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi2Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi2Qp >= 0 ? codingCfg.roi2Qp : codingCfg.roi2DeltaQp,
             codingCfg.roi2Area.left, codingCfg.roi2Area.top,
             codingCfg.roi2Area.right, codingCfg.roi2Area.bottom);

    if (codingCfg.roi3Area.enable)
      printf("  ROI 3: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi3Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi3Qp >= 0 ? codingCfg.roi3Qp : codingCfg.roi3DeltaQp,
             codingCfg.roi3Area.left, codingCfg.roi3Area.top,
             codingCfg.roi3Area.right, codingCfg.roi3Area.bottom);

    if (codingCfg.roi4Area.enable)
      printf("  ROI 4: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi4Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi4Qp >= 0 ? codingCfg.roi4Qp : codingCfg.roi4DeltaQp,
             codingCfg.roi4Area.left, codingCfg.roi4Area.top,
             codingCfg.roi4Area.right, codingCfg.roi4Area.bottom);

    if (codingCfg.roi5Area.enable)
      printf("  ROI 5: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi5Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi5Qp >= 0 ? codingCfg.roi5Qp : codingCfg.roi5DeltaQp,
             codingCfg.roi5Area.left, codingCfg.roi5Area.top,
             codingCfg.roi5Area.right, codingCfg.roi5Area.bottom);

    if (codingCfg.roi6Area.enable)
      printf("  ROI 6: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi6Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi6Qp >= 0 ? codingCfg.roi6Qp : codingCfg.roi6DeltaQp,
             codingCfg.roi6Area.left, codingCfg.roi6Area.top,
             codingCfg.roi6Area.right, codingCfg.roi6Area.bottom);

    if (codingCfg.roi7Area.enable)
      printf("  ROI 7: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi7Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi7Qp >= 0 ? codingCfg.roi7Qp : codingCfg.roi7DeltaQp,
             codingCfg.roi7Area.left, codingCfg.roi7Area.top,
             codingCfg.roi7Area.right, codingCfg.roi7Area.bottom);

    if (codingCfg.roi8Area.enable)
      printf("  ROI 8: %s %d  %dx%d-%dx%d\n",
             codingCfg.roi8Qp >= 0 ? "QP" : "QP Delta",
             codingCfg.roi8Qp >= 0 ? codingCfg.roi8Qp : codingCfg.roi8DeltaQp,
             codingCfg.roi8Area.left, codingCfg.roi8Area.top,
             codingCfg.roi8Area.right, codingCfg.roi8Area.bottom);

    codingCfg.roiMapDeltaQpEnable = cml->roiMapDeltaQpEnable;
    codingCfg.roiMapDeltaQpBlockUnit=cml->roiMapDeltaQpBlockUnit;

    codingCfg.RoimapCuCtrl_index_enable = (cml->RoimapCuCtrlIndexBinFile != NULL);
    codingCfg.RoimapCuCtrl_enable       = (cml->RoimapCuCtrlInfoBinFile != NULL);
    codingCfg.roiMapDeltaQpBinEnable    = (cml->roiMapInfoBinFile != NULL);
    codingCfg.RoimapCuCtrl_ver          = cml->RoiCuCtrlVer;
    codingCfg.RoiQpDelta_ver            = cml->RoiQpDeltaVer;

    /* SKIP map */
    codingCfg.skipMapEnable = cml->skipMapEnable;

    codingCfg.enableScalingList = cml->enableScalingList;
    codingCfg.chroma_qp_offset= cml->chromaQpOffset;

    /* low latency */
    codingCfg.inputLineBufEn = (cml->inputLineBufMode>0) ? 1 : 0;
    codingCfg.inputLineBufLoopBackEn = (cml->inputLineBufMode==1||cml->inputLineBufMode==2) ? 1 : 0;
    if (cml->inputLineBufDepth != DEFAULT) codingCfg.inputLineBufDepth = cml->inputLineBufDepth;
    codingCfg.amountPerLoopBack = cml->amountPerLoopBack;
    codingCfg.inputLineBufHwModeEn = (cml->inputLineBufMode==2||cml->inputLineBufMode==4) ? 1 : 0;
    codingCfg.inputLineBufCbFunc = VCEncInputLineBufDone;
    codingCfg.inputLineBufCbData = &(tb->inputCtbLineBuf);

    /*stream multi-segment*/
    codingCfg.streamMultiSegmentMode = cml->streamMultiSegmentMode;
    codingCfg.streamMultiSegmentAmount = cml->streamMultiSegmentAmount;
    codingCfg.streamMultiSegCbFunc = &EncStreamSegmentReady;
    codingCfg.streamMultiSegCbData = &(tb->streamSegCtl);

    /* denoise */
    codingCfg.noiseReductionEnable = cml->noiseReductionEnable;//0: disable noise reduction; 1: enable noise reduction
    if(cml->noiseLow == 0)
    {
        codingCfg.noiseLow = 10;
    }
    else
    {
        codingCfg.noiseLow = CLIP3(1, 30, cml->noiseLow);//0: use default value; valid value range: [1, 30]
    }

    if(cml->firstFrameSigma == 0)
    {
        codingCfg.firstFrameSigma = 11;
    }
    else
    {
        codingCfg.firstFrameSigma = CLIP3(1, 30, cml->firstFrameSigma);
    }

    /* smart */
    codingCfg.smartModeEnable = cml->smartModeEnable;
    codingCfg.smartH264LumDcTh = cml->smartH264LumDcTh;
    codingCfg.smartH264CbDcTh = cml->smartH264CbDcTh;
    codingCfg.smartH264CrDcTh = cml->smartH264CrDcTh;
    for(i = 0; i < 3; i++) {
      codingCfg.smartHevcLumDcTh[i] = cml->smartHevcLumDcTh[i];
      codingCfg.smartHevcChrDcTh[i] = cml->smartHevcChrDcTh[i];
      codingCfg.smartHevcLumAcNumTh[i] = cml->smartHevcLumAcNumTh[i];
      codingCfg.smartHevcChrAcNumTh[i] = cml->smartHevcChrAcNumTh[i];
    }
    codingCfg.smartH264Qp = cml->smartH264Qp;
    codingCfg.smartHevcLumQp = cml->smartHevcLumQp;
    codingCfg.smartHevcChrQp = cml->smartHevcChrQp;
    for(i = 0; i < 4; i++)
      codingCfg.smartMeanTh[i] = cml->smartMeanTh[i];
    codingCfg.smartPixNumCntTh = cml->smartPixNumCntTh;

    /* tile */
    codingCfg.tiles_enabled_flag = cml->tiles_enabled_flag && !IS_H264(cml->codecFormat);
    codingCfg.num_tile_columns = cml->num_tile_columns;
    codingCfg.num_tile_rows       = cml->num_tile_rows;
    codingCfg.loop_filter_across_tiles_enabled_flag = cml->loop_filter_across_tiles_enabled_flag;

    /* HDR10 */
    codingCfg.Hdr10Display.hdr10_display_enable = cml->hdr10_display_enable;
    if (cml->hdr10_display_enable)
    {
    	codingCfg.Hdr10Display.hdr10_dx0 = cml->hdr10_dx0;
    	codingCfg.Hdr10Display.hdr10_dy0 = cml->hdr10_dy0;
    	codingCfg.Hdr10Display.hdr10_dx1 = cml->hdr10_dx1;
    	codingCfg.Hdr10Display.hdr10_dy1 = cml->hdr10_dy1;
    	codingCfg.Hdr10Display.hdr10_dx2 = cml->hdr10_dx2;
    	codingCfg.Hdr10Display.hdr10_dy2 = cml->hdr10_dy2;
    	codingCfg.Hdr10Display.hdr10_wx  = cml->hdr10_wx;
    	codingCfg.Hdr10Display.hdr10_wy  = cml->hdr10_wy;
    	codingCfg.Hdr10Display.hdr10_maxluma = cml->hdr10_maxluma;
    	codingCfg.Hdr10Display.hdr10_minluma = cml->hdr10_minluma;
    }

    codingCfg.Hdr10LightLevel.hdr10_lightlevel_enable = cml->hdr10_lightlevel_enable;
    if (cml->hdr10_lightlevel_enable)
    {
    	codingCfg.Hdr10LightLevel.hdr10_maxlight = cml->hdr10_maxlight;
    	codingCfg.Hdr10LightLevel.hdr10_avglight = cml->hdr10_avglight;
    }

    codingCfg.Hdr10Color.hdr10_color_enable = cml->hdr10_color_enable;
    if (cml->hdr10_color_enable)
    {
    	codingCfg.Hdr10Color.hdr10_matrix   = cml->hdr10_matrix;
    	codingCfg.Hdr10Color.hdr10_primary  = cml->hdr10_primary;

    	if (cml->hdr10_transfer == 1)
    		codingCfg.Hdr10Color.hdr10_transfer = VCENC_HDR10_ST2084;
    	else if (cml->hdr10_transfer == 2)
    		codingCfg.Hdr10Color.hdr10_transfer = VCENC_HDR10_STDB67;
    	else
    		codingCfg.Hdr10Color.hdr10_transfer = VCENC_HDR10_BT2020;
    }

    codingCfg.RpsInSliceHeader = cml->RpsInSliceHeader;

    /* Assign ME vertical search range */
    codingCfg.meVertSearchRange = cml->MEVertRange;

    codingCfg.PsyFactor = cml->PsyFactor;

    if ((ret = VCEncSetCodingCtrl(encoder, &codingCfg)) != VCENC_OK)
    {
      //PrintErrorValue("VCEncSetCodingCtrl() failed.", ret);
      CloseEncoder(encoder, tb);
      return -1;
    }

  }

  /* Encoder setup: rate control */
  if ((ret = VCEncGetRateCtrl(encoder, &rcCfg)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncGetRateCtrl() failed.", ret);
    CloseEncoder(encoder, tb);
    return -1;
  }
  else
  {
    printf("Get rate control: qp %2d qpRange I[%2d, %2d] PB[%2d, %2d] %8d bps  "
           "pic %d skip %d  hrd %d  cpbSize %d bitrateWindow %d "
           "intraQpDelta %2d\n",
           rcCfg.qpHdr, rcCfg.qpMinI, rcCfg.qpMaxI, rcCfg.qpMinPB, rcCfg.qpMaxPB, rcCfg.bitPerSecond,
           rcCfg.pictureRc, rcCfg.pictureSkip, rcCfg.hrd,
           rcCfg.hrdCpbSize, rcCfg.bitrateWindow, rcCfg.intraQpDelta);

    if (cml->qpHdr != DEFAULT)
      rcCfg.qpHdr = cml->qpHdr;
    else
      rcCfg.qpHdr = -1;
    if (cml->qpMin != DEFAULT)
      rcCfg.qpMinI = rcCfg.qpMinPB = cml->qpMin;
    if (cml->qpMax != DEFAULT)
      rcCfg.qpMaxI = rcCfg.qpMaxPB = cml->qpMax;
    if (cml->qpMinI != DEFAULT)
      rcCfg.qpMinI = cml->qpMinI;
    if (cml->qpMaxI != DEFAULT)
      rcCfg.qpMaxI = cml->qpMaxI;
    if (cml->picSkip != DEFAULT)
      rcCfg.pictureSkip = cml->picSkip;
    if (cml->picRc != DEFAULT)
      rcCfg.pictureRc = cml->picRc;
    if (cml->ctbRc != DEFAULT)
    {
      if(cml->ctbRc == 4 || cml->ctbRc == 6)
      {
        rcCfg.ctbRc = cml->ctbRc - 3;
        rcCfg.ctbRcQpDeltaReverse = 1;
      }
      else
      {
        rcCfg.ctbRc = cml->ctbRc;
        rcCfg.ctbRcQpDeltaReverse = 0;
      }
    }

    rcCfg.blockRCSize = 0;
    if (cml->blockRCSize != DEFAULT)
      rcCfg.blockRCSize = cml->blockRCSize;

    rcCfg.rcQpDeltaRange = 10;
    if (cml->rcQpDeltaRange != DEFAULT)
      rcCfg.rcQpDeltaRange = cml->rcQpDeltaRange;

    rcCfg.rcBaseMBComplexity = 15;
    if (cml->rcBaseMBComplexity != DEFAULT)
      rcCfg.rcBaseMBComplexity = cml->rcBaseMBComplexity;

    if (cml->picQpDeltaMax != DEFAULT)
      rcCfg.picQpDeltaMax = cml->picQpDeltaMax;
    if (cml->picQpDeltaMin != DEFAULT)
      rcCfg.picQpDeltaMin = cml->picQpDeltaMin;
    if (cml->bitPerSecond != DEFAULT)
      rcCfg.bitPerSecond = cml->bitPerSecond;
    if (cml->bitVarRangeI != DEFAULT)
      rcCfg.bitVarRangeI = cml->bitVarRangeI;
    if (cml->bitVarRangeP != DEFAULT)
      rcCfg.bitVarRangeP = cml->bitVarRangeP;
    if (cml->bitVarRangeB != DEFAULT)
      rcCfg.bitVarRangeB = cml->bitVarRangeB;

    if (cml->tolMovingBitRate != DEFAULT)
      rcCfg.tolMovingBitRate = cml->tolMovingBitRate;

    if (cml->tolCtbRcInter != DEFAULT)
      rcCfg.tolCtbRcInter = cml->tolCtbRcInter;

    if (cml->tolCtbRcIntra != DEFAULT)
      rcCfg.tolCtbRcIntra = cml->tolCtbRcIntra;

    if (cml->ctbRcRowQpStep != DEFAULT)
      rcCfg.ctbRcRowQpStep = cml->ctbRcRowQpStep;

    rcCfg.longTermQpDelta = cml->longTermQpDelta;

    if (cml->monitorFrames != DEFAULT)
      rcCfg.monitorFrames = cml->monitorFrames;
    else
    {
      rcCfg.monitorFrames =(cml->outputRateNumer+cml->outputRateDenom-1) / cml->outputRateDenom;
      cml->monitorFrames = (cml->outputRateNumer+cml->outputRateDenom-1) / cml->outputRateDenom;
    }

    if(rcCfg.monitorFrames>MOVING_AVERAGE_FRAMES)
      rcCfg.monitorFrames=MOVING_AVERAGE_FRAMES;

    if (rcCfg.monitorFrames < 10)
    {
      rcCfg.monitorFrames = (cml->outputRateNumer > cml->outputRateDenom)? 10:LEAST_MONITOR_FRAME;
    }

    if (cml->hrdConformance != DEFAULT)
      rcCfg.hrd = cml->hrdConformance;

    if (cml->cpbSize != DEFAULT)
      rcCfg.hrdCpbSize = cml->cpbSize;

    if (cml->intraPicRate != 0)
      rcCfg.bitrateWindow = MIN(cml->intraPicRate, MAX_GOP_LEN);

    if (cml->bitrateWindow != DEFAULT)
      rcCfg.bitrateWindow = cml->bitrateWindow;

    if (cml->intraQpDelta != DEFAULT)
      rcCfg.intraQpDelta = cml->intraQpDelta;

    if (cml->vbr != DEFAULT)
      rcCfg.vbr = cml->vbr;

    if (cml->crf != DEFAULT)
      rcCfg.crf = cml->crf;

    rcCfg.fixedIntraQp = cml->fixedIntraQp;
    rcCfg.smoothPsnrInGOP = cml->smoothPsnrInGOP;
    rcCfg.u32StaticSceneIbitPercent = cml->u32StaticSceneIbitPercent;

    printf("Set rate control: qp %2d qpRange I[%2d, %2d] PB[%2d, %2d] %9d bps  "
           "pic %d skip %d  hrd %d"
           "  cpbSize %d bitrateWindow %d intraQpDelta %2d "
           "fixedIntraQp %2d\n",
           rcCfg.qpHdr, rcCfg.qpMinI, rcCfg.qpMaxI, rcCfg.qpMinPB, rcCfg.qpMaxPB, rcCfg.bitPerSecond,
           rcCfg.pictureRc, rcCfg.pictureSkip, rcCfg.hrd,
           rcCfg.hrdCpbSize, rcCfg.bitrateWindow, rcCfg.intraQpDelta,
           rcCfg.fixedIntraQp);

    if ((ret = VCEncSetRateCtrl(encoder, &rcCfg)) != VCENC_OK)
    {
      //PrintErrorValue("VCEncSetRateCtrl() failed.", ret);
      CloseEncoder(encoder, tb);
      return -1;
    }
  }

  /* Optional scaled image output */
  if (cml->scaledWidth * cml->scaledHeight > 0)
  {
    i32 dsFrmSize = cml->scaledWidth * cml->scaledHeight * 2;
    /* the scaled image size changes frame by frame when testing down-scaling */
    if (cml->testId == 34)
    {
      dsFrmSize = cml->width * cml->height * 2;
    }
    if((cml->bitDepthLuma!=8) || (cml->bitDepthChroma!=8))
      dsFrmSize <<= 1;

    if (cml->scaledOutputFormat == 1)
      dsFrmSize = dsFrmSize *3 /4;

    if (EWLMallocRefFrm(ewl_inst, dsFrmSize, 0, &tb->scaledPictureMem) != EWL_OK)
    {
      tb->scaledPictureMem.virtualAddress = NULL;
      tb->scaledPictureMem.busAddress = 0;
    }
  }

  /* PreP setup */
  if ((ret = VCEncGetPreProcessing(encoder, &preProcCfg)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncGetPreProcessing() failed.", ret);
    CloseEncoder(encoder, tb);
    return -1;
  }
  printf
  ("Get PreP: input %4dx%d : offset %4dx%d : format %d : rotation %d"
   "cc %d : scaling %d\n",
   preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
   preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
   preProcCfg.colorConversion.type,
   preProcCfg.scaledOutput);

  preProcCfg.inputType = (VCEncPictureType)cml->inputFormat;
  preProcCfg.rotation = (VCEncPictureRotation)cml->rotation;
  preProcCfg.mirror = (VCEncPictureMirror)cml->mirror;

  preProcCfg.origWidth = cml->lumWidthSrc;
  preProcCfg.origHeight = cml->lumHeightSrc;
  if (cml->interlacedFrame) preProcCfg.origHeight /= 2;

  if (cml->horOffsetSrc != DEFAULT)
    preProcCfg.xOffset = cml->horOffsetSrc;
  if (cml->verOffsetSrc != DEFAULT)
    preProcCfg.yOffset = cml->verOffsetSrc;

  /* stab setup */
  preProcCfg.videoStabilization = 0;
  if (cml->videoStab != DEFAULT)
  {
    preProcCfg.videoStabilization = cml->videoStab;
  }

  if (cml->colorConversion != DEFAULT)
    preProcCfg.colorConversion.type =
      (VCEncColorConversionType)cml->colorConversion;
  if (preProcCfg.colorConversion.type == VCENC_RGBTOYUV_USER_DEFINED)
  {
    preProcCfg.colorConversion.coeffA = 20000;
    preProcCfg.colorConversion.coeffB = 44000;
    preProcCfg.colorConversion.coeffC = 5000;
    preProcCfg.colorConversion.coeffE = 35000;
    preProcCfg.colorConversion.coeffF = 38000;
    preProcCfg.colorConversion.coeffG = 35000;
    preProcCfg.colorConversion.coeffH = 38000;
    preProcCfg.colorConversion.LumaOffset = 0;
  }

  if (cml->rotation&&cml->rotation!=3)
  {
    preProcCfg.scaledWidth = cml->scaledHeight;
    preProcCfg.scaledHeight = cml->scaledWidth;
  }
  else
  {
    preProcCfg.scaledWidth = cml->scaledWidth;
    preProcCfg.scaledHeight = cml->scaledHeight;
  }
  preProcCfg.busAddressScaledBuff = tb->scaledPictureMem.busAddress;
  preProcCfg.virtualAddressScaledBuff = tb->scaledPictureMem.virtualAddress;
  preProcCfg.sizeScaledBuff = tb->scaledPictureMem.size;
  preProcCfg.input_alignment = 1<<cml->exp_of_input_alignment;
  preProcCfg.scaledOutputFormat = cml->scaledOutputFormat;

  printf
  ("Set PreP: input %4dx%d : offset %4dx%d : format %d : rotation %d"
   "cc %d : scaling %d : scaling format %d\n",
   preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
   preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
   preProcCfg.colorConversion.type,
   preProcCfg.scaledOutput, preProcCfg.scaledOutputFormat);

  if(cml->scaledWidth*cml->scaledHeight > 0)
    preProcCfg.scaledOutput=1;

   /* constant chroma control */
   preProcCfg.constChromaEn = cml->constChromaEn;
   if (cml->constCb != DEFAULT)
     preProcCfg.constCb = cml->constCb;
   if (cml->constCr != DEFAULT)
     preProcCfg.constCr = cml->constCr;

  ChangeToCustomizedFormat(cml,&preProcCfg);

  /* Set overlay area*/
  for(i = 0; i < MAX_OVERLAY_NUM; i++)
  {
    preProcCfg.overlayArea[i].xoffset = cml->olXoffset[i];
    preProcCfg.overlayArea[i].cropXoffset = cml->olCropXoffset[i];
    preProcCfg.overlayArea[i].yoffset = cml->olYoffset[i];
    preProcCfg.overlayArea[i].cropYoffset = cml->olCropYoffset[i];
    preProcCfg.overlayArea[i].width = cml->olWidth[i];
    preProcCfg.overlayArea[i].cropWidth = cml->olCropWidth[i];
    preProcCfg.overlayArea[i].height = cml->olHeight[i];
    preProcCfg.overlayArea[i].cropHeight = cml->olCropHeight[i];
    preProcCfg.overlayArea[i].format = cml->olFormat[i];
    preProcCfg.overlayArea[i].alpha = cml->olAlpha[i];
    preProcCfg.overlayArea[i].enable = (cml->overlayEnables >> i) & 1;
    preProcCfg.overlayArea[i].Ystride = cml->olYStride[i];
    preProcCfg.overlayArea[i].UVstride = cml->olUVStride[i];
    preProcCfg.overlayArea[i].bitmapY = cml->olBitmapY[i];
    preProcCfg.overlayArea[i].bitmapU = cml->olBitmapU[i];
    preProcCfg.overlayArea[i].bitmapV = cml->olBitmapV[i];
  }

  if ((ret = VCEncSetPreProcessing(encoder, &preProcCfg)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncSetPreProcessing() failed.", ret);
    CloseEncoder(encoder, tb);
    return (int)ret;
  }
  return 0;
}
/*------------------------------------------------------------------------------

    CloseEncoder
       Release an encoder insatnce.

   Params:
        encoder - the instance to be released
------------------------------------------------------------------------------*/
void CloseEncoder(VCEncInst encoder, struct test_bench *tb)
{
  VCEncRet ret;
  const void *ewl_inst = VCEncGetEwl(encoder);

  if (tb->scaledPictureMem.virtualAddress != NULL)
    EWLFreeLinear(ewl_inst, &tb->scaledPictureMem);
  if ((ret = VCEncRelease(encoder)) != VCENC_OK)
  {
    //PrintErrorValue("VCEncRelease() failed.", ret);
  }
}
/*------------------------------------------------------------------------------

    AllocRes

    Allocation of the physical memories used by both SW and HW:
    the input pictures and the output stream buffer.

    NOTE! The implementation uses the EWL instance from the encoder
          for OS independence. This is not recommended in final environment
          because the encoder will release the EWL instance in case of error.
          Instead, the memories should be allocated from the OS the same way
          as inside EWLMallocLinear().

------------------------------------------------------------------------------*/
int AllocRes(commandLine_s *cmdl, VCEncInst enc, struct test_bench *tb)
{
  i32 ret;
  u32 pictureSize = 0;
  u32 pictureDSSize = 0;
  u32 streamBufTotalSize;
  u32 outbufSize[2] = {0, 0};
  u32 block_size;
  u32 transform_size=0;
  u32 lumaSize = 0,chromaSize = 0;
  u32 coreIdx=0;
  i32 iBuf;
  u32 alignment = 0;
  u32 extSramWidthAlignment = 0;
  u32 bitDepthLuma = 8, bitDepthChroma = 8, extSramTotal = 0;
  const void *ewl_inst =  VCEncGetEwl(enc);
  const EWLHwConfig_t asicCfg = VCEncGetAsicConfig(cmdl->codecFormat);

  alignment = (cmdl->formatCustomizedType != -1? 0 : tb->input_alignment);
  getAlignedPicSizebyFormat(cmdl->inputFormat,cmdl->lumWidthSrc,cmdl->lumHeightSrc,alignment,&lumaSize,&chromaSize,&pictureSize);

  tb->lumaSize = lumaSize;
  tb->chromaSize = chromaSize;

  if (cmdl->formatCustomizedType != -1)
  {
    u32 trans_format = VCENC_FORMAT_MAX;
    u32 input_alignment = 0;
    switch(cmdl->formatCustomizedType)
    {
      case 0:
        if (cmdl->inputFormat==VCENC_YUV420_PLANAR)
        {
          if (IS_HEVC(cmdl->codecFormat))//Dahua hevc
            trans_format = VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC;
          else//Dahua h264
            trans_format = VCENC_YUV420_PLANAR_8BIT_DAHUA_H264;
        }
        break;
      case 1:
        {
          if (cmdl->inputFormat==VCENC_YUV420_SEMIPLANAR || cmdl->inputFormat==VCENC_YUV420_SEMIPLANAR_VU)
            trans_format = VCENC_YUV420_SEMIPLANAR_8BIT_FB;
          else if (cmdl->inputFormat==VCENC_YUV420_PLANAR_10BIT_P010)
            trans_format = VCENC_YUV420_PLANAR_10BIT_P010_FB;
          input_alignment = tb->input_alignment;
          break;
        }
      case 2:
        trans_format = VCENC_YUV420_SEMIPLANAR_101010;
        break;
      case 3:
      case 4:
        trans_format = VCENC_YUV420_8BIT_TILE_64_4;
        break;
      case 5:
        trans_format = VCENC_YUV420_10BIT_TILE_32_4;
        break;
      case 6:
      case 7:
        trans_format = VCENC_YUV420_10BIT_TILE_48_4;
        break;
      case 8:
      case 9:
        trans_format = VCENC_YUV420_8BIT_TILE_128_2;
        break;
      case 10:
      case 11:
        trans_format = VCENC_YUV420_10BIT_TILE_96_2;
        break;
      case 12:
        trans_format = VCENC_YUV420_8BIT_TILE_8_8;
        break;
      case 13:
        trans_format = VCENC_YUV420_10BIT_TILE_8_8;
        break;
      default:
        break;
    }

    getAlignedPicSizebyFormat(trans_format,cmdl->lumWidthSrc,cmdl->lumHeightSrc,input_alignment,NULL,NULL,&transform_size);
    tb->transformedSize = transform_size;

  }
  /* Here we use the EWL instance directly from the encoder
   * because it is the easiest way to allocate the linear memories */

  for(coreIdx =0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    ret = EWLMallocLinear(ewl_inst, pictureSize,alignment, &tb->pictureMemFactory[coreIdx]);
    if (ret != EWL_OK)
    {
      tb->pictureMemFactory[coreIdx].virtualAddress = NULL;
      return 1;
    }

    if (cmdl->formatCustomizedType != -1 && transform_size!=0)
    {
      ret = EWLMallocLinear(ewl_inst, transform_size,tb->input_alignment, &tb->transformMemFactory[coreIdx]);
      if (ret != EWL_OK)
      {
        tb->transformMemFactory[coreIdx].virtualAddress = NULL;
        return 1;
      }
    }

    tb->pictureDSMemFactory[coreIdx].virtualAddress = NULL;
    if(cmdl->halfDsInput) {
      getAlignedPicSizebyFormat(cmdl->inputFormat,cmdl->lumWidthSrc/2,cmdl->lumHeightSrc/2,alignment,NULL,NULL,&pictureDSSize);
      ret = EWLMallocLinear(ewl_inst, pictureDSSize,alignment, &tb->pictureDSMemFactory[coreIdx]);
      if (ret != EWL_OK)
      {
        tb->pictureDSMemFactory[coreIdx].virtualAddress = NULL;
        return 1;
      }
    }

  }

  if ((cmdl->videoStab != DEFAULT) && (cmdl->videoStab))
  {
    ret = EWLMallocLinear(ewl_inst, pictureSize, alignment, &tb->pictureStabMem);
    if (ret != EWL_OK)
    {
      printf("ERROR: Cannot allocate enough memory for stabilization!\n");
      tb->pictureStabMem.virtualAddress = NULL;
      return 1;
    }
  }

  getDec400CompTablebyFormat(cmdl->inputFormat,cmdl->lumWidthSrc,cmdl->lumHeightSrc,alignment,&tb->dec400LumaTableSize,&tb->dec400ChrTableSize,NULL);
  if ((tb->dec400LumaTableSize + tb->dec400ChrTableSize) != 0)
  {
    ret = EWLMallocLinear(ewl_inst, (tb->dec400LumaTableSize + tb->dec400ChrTableSize), 16, &tb->Dec400CmpTableMem);
    if (ret != EWL_OK)
    {
      tb->Dec400CmpTableMem.virtualAddress = NULL;
      return 1;
    }

  }

  /* Limited amount of memory on some test environment */
  if (cmdl->outBufSizeMax == 0) cmdl->outBufSizeMax = 12;
  streamBufTotalSize = 4 * tb->pictureMemFactory[0].size;
  streamBufTotalSize = CLIP3(VCENC_STREAM_MIN_BUF0_SIZE, (u32)cmdl->outBufSizeMax * 1024 * 1024, streamBufTotalSize);
  streamBufTotalSize = (cmdl->streamMultiSegmentMode != 0? tb->pictureMemFactory[0].size/16 : streamBufTotalSize);
  outbufSize[0] = streamBufTotalSize;
  if (tb->streamBufNum > 1)
  {
    /* set small stream buffer0 to test two stream buffers */
    outbufSize[0] = MAX(tb->pictureMemFactory[0].size >> 10, VCENC_STREAM_MIN_BUF0_SIZE);
    outbufSize[1] = (streamBufTotalSize - outbufSize[0]) / (tb->streamBufNum-1);
  }
  for(coreIdx = 0; coreIdx < tb->parallelCoreNum; coreIdx++)
  {
    for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
    {
      i32 size = outbufSize[iBuf ? 1 : 0];
      ret = EWLMallocLinear(ewl_inst, size, tb->input_alignment, &tb->outbufMemFactory[coreIdx][iBuf]);
      if (ret != EWL_OK)
      {
        tb->outbufMemFactory[coreIdx][iBuf].virtualAddress = NULL;
        return 1;
      }
    }
  }

  if(IS_AV1(cmdl->codecFormat)){
  	for(coreIdx = 0; coreIdx < tb->parallelCoreNum; coreIdx++)
    {
      for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
      {
        i32 size = outbufSize[iBuf ? 1 : 0] << 1;
        ret = EWLMallocLinear(ewl_inst, size, tb->input_alignment, &tb->Av1PreoutbufMemFactory[coreIdx][iBuf]);
        if (ret != EWL_OK)
        {
          tb->Av1PreoutbufMemFactory[coreIdx][iBuf].virtualAddress = NULL;
          return 1;
        }
      }
    }
  }

  //allocate delta qp map memory.
  // 4 bits per block.
  block_size=((cmdl->width+cmdl->max_cu_size-1)& (~(cmdl->max_cu_size - 1)))*((cmdl->height+cmdl->max_cu_size-1)& (~(cmdl->max_cu_size - 1)))/(8*8*2);
  // 8 bits per block if ipcm map/absolute roi qp is supported
  if (asicCfg.roiMapVersion >= 1)
    block_size *= 2;
  block_size = ((block_size+63)&(~63));
  if (EWLMallocLinear(ewl_inst, block_size*tb->buffer_cnt + ROIMAP_PREFETCH_EXT_SIZE,0,&tb->roiMapDeltaQpMemFactory[0]) != EWL_OK)
  {
    tb->roiMapDeltaQpMemFactory[0].virtualAddress = NULL;
    return 1;
  }
  i32 total_size = tb->roiMapDeltaQpMemFactory[0].size;
  for(coreIdx = 0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    tb->roiMapDeltaQpMemFactory[coreIdx].virtualAddress = (u32*)((ptr_t)tb->roiMapDeltaQpMemFactory[0].virtualAddress + coreIdx * block_size);
    tb->roiMapDeltaQpMemFactory[coreIdx].busAddress = tb->roiMapDeltaQpMemFactory[0].busAddress + coreIdx * block_size;
    tb->roiMapDeltaQpMemFactory[coreIdx].size = (coreIdx < tb->buffer_cnt-1 ? block_size : total_size - (tb->buffer_cnt-1)*block_size);
    memset(tb->roiMapDeltaQpMemFactory[coreIdx].virtualAddress, 0, block_size);

    if(asicCfg.roiMapVersion == 3)
    {
        if(cmdl->RoimapCuCtrlInfoBinFile != NULL)
        {
            u8 u8CuInfoSize;
            if(cmdl->RoiCuCtrlVer == 3)
              u8CuInfoSize = 1;
            else if(cmdl->RoiCuCtrlVer == 4)
              u8CuInfoSize = 2;
            else if(cmdl->RoiCuCtrlVer == 5)
              u8CuInfoSize = 6;
            else if(cmdl->RoiCuCtrlVer == 6)
              u8CuInfoSize = 12;
            else // if((cmdl->RoiCuCtrlVer == 7)
              u8CuInfoSize = 14;
            if (EWLMallocLinear(ewl_inst, (block_size*u8CuInfoSize),0,&tb->RoimapCuCtrlInfoMemFactory[coreIdx]) != EWL_OK)
            {
                tb->RoimapCuCtrlInfoMemFactory[coreIdx].virtualAddress = NULL;
                return 1;
            }
            memset(tb->RoimapCuCtrlInfoMemFactory[coreIdx].virtualAddress, 0, block_size);
        }

        if(cmdl->RoimapCuCtrlIndexBinFile != NULL)
        {
            block_size = 1 << (IS_H264(cmdl->codecFormat) ? 4 : 6);
            block_size=((cmdl->width+block_size-1)& (~(block_size - 1)))*((cmdl->height+block_size-1)& (~(block_size - 1)))/(block_size*block_size);
            if (EWLMallocLinear(ewl_inst, block_size,0,&tb->RoimapCuCtrlIndexMemFactory[coreIdx]) != EWL_OK)
            {
                tb->RoimapCuCtrlIndexMemFactory[coreIdx].virtualAddress = NULL;
                return 1;
            }
            memset(tb->RoimapCuCtrlIndexMemFactory[coreIdx].virtualAddress, 0, block_size);
        }
    }

  }

  if (cmdl->bitDepthLuma != DEFAULT)
    bitDepthLuma = cmdl->bitDepthLuma;

  if (cmdl->bitDepthChroma != DEFAULT)
    bitDepthChroma = cmdl->bitDepthChroma;

  if (IS_HEVC(cmdl->codecFormat) && bitDepthLuma == 8)//hevc main8
    extSramWidthAlignment = 8;
  else if (IS_HEVC(cmdl->codecFormat) && bitDepthLuma == 10)//hevc main10
    extSramWidthAlignment = 16;
  else if (IS_H264(cmdl->codecFormat))//h264
    extSramWidthAlignment = 16;

  u32 stride = STRIDE((cmdl->rotation&&cmdl->rotation!=3?cmdl->height : cmdl->width), extSramWidthAlignment);
  tb->extSramLumBwdSize = cmdl->extSramLumHeightBwd * 4 * stride*10/(bitDepthLuma == 10?8:10);
  tb->extSramLumFwdSize = cmdl->extSramLumHeightFwd * 4 * stride*10/(bitDepthLuma == 10?8:10);
  tb->extSramChrBwdSize = cmdl->extSramChrHeightBwd * 4 * stride*10/(bitDepthChroma == 10?8:10);
  tb->extSramChrFwdSize = cmdl->extSramChrHeightFwd * 4 * stride*10/(bitDepthChroma == 10?8:10);

  if (asicCfg.ExtSramSupport)
    extSramTotal = tb->extSramLumBwdSize + tb->extSramLumFwdSize + tb->extSramChrBwdSize + tb->extSramChrFwdSize;

  for(coreIdx = 0; coreIdx < tb->parallelCoreNum; coreIdx++)
  {
    if (extSramTotal != 0)
    {
      ret = EWLMallocLinear(ewl_inst, extSramTotal, 16, &tb->extSRAMMemFactory[coreIdx]);
      if (ret != EWL_OK)
      {
        tb->extSRAMMemFactory[coreIdx].virtualAddress = NULL;
        return 1;
      }
    }
  }

  /*Overlay input buffer*/
  for(coreIdx = 0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    int i = 0;
    for(i = 0; i < 8; i++)
    {
      if((cmdl->overlayEnables >> i)&1)
      {
        switch(cmdl->olFormat[i])
        {
          case 0: //ARGB
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i];
            break;
          case 1: //NV12
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i] + cmdl->olUVStride[i]*cmdl->olHeight[i]/2;
            break;
          case 2: //Bitmap
            block_size = cmdl->olYStride[i]*cmdl->olHeight[i];
            break;
          default: //3
            block_size = 0;
        }
        if(EWLMallocLinear(ewl_inst, block_size, tb->input_alignment, &tb->overlayInputMemFactory[coreIdx][i]) != EWL_OK)
        {
          tb->overlayInputMemFactory[coreIdx][i].virtualAddress = NULL;
          return 1;
        }
        memset(tb->overlayInputMemFactory[coreIdx][i].virtualAddress, 0, block_size);
      }
      else
      {
        tb->overlayInputMemFactory[coreIdx][i].virtualAddress = NULL;
      }
    }
  }


  for(coreIdx =0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    printf("Input buffer[%d] size:          %d bytes\n", coreIdx, tb->pictureMemFactory[coreIdx].size);
    printf("Input buffer[%d] bus address:   %p\n", coreIdx, (void *)tb->pictureMemFactory[coreIdx].busAddress);
    printf("Input buffer[%d] user address:  %p\n", coreIdx, tb->pictureMemFactory[coreIdx].virtualAddress);
  }

  for(coreIdx =0; coreIdx < tb->parallelCoreNum; coreIdx++)
  {
    for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
    {
      printf("Output buffer[%d] size:         %d bytes\n", coreIdx, tb->outbufMemFactory[coreIdx][iBuf].size);
      printf("Output buffer[%d] bus address:  %p\n", coreIdx, (void *)tb->outbufMemFactory[coreIdx][iBuf].busAddress);
      printf("Output buffer[%d] user address: %p\n",  coreIdx, tb->outbufMemFactory[coreIdx][iBuf].virtualAddress);
    }
  }
  return 0;
}

/*------------------------------------------------------------------------------

    FreeRes

    Release all resources allcoated byt AllocRes()

------------------------------------------------------------------------------*/
void FreeRes(VCEncInst enc, struct test_bench *tb)
{
  u32 coreIdx=0;
  i32 iBuf;
  const void *ewl_inst = VCEncGetEwl(enc);

  if (tb->Dec400CmpTableMem.virtualAddress != NULL)
  {
    EWLFreeLinear(ewl_inst, &tb->Dec400CmpTableMem);
  }

  if (tb->pictureStabMem.virtualAddress != NULL)
  {
    EWLFreeLinear(ewl_inst, &tb->pictureStabMem);
  }

  if (tb->roiMapDeltaQpMemFactory[0].virtualAddress != NULL)
  {
    for(coreIdx = 1; coreIdx < tb->buffer_cnt; coreIdx++)
      tb->roiMapDeltaQpMemFactory[0].size += tb->roiMapDeltaQpMemFactory[coreIdx].size;
    EWLFreeLinear(ewl_inst, &tb->roiMapDeltaQpMemFactory[0]);
  }
  for(coreIdx =0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    if (tb->pictureMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->pictureMemFactory[coreIdx]);
    if (tb->pictureDSMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->pictureDSMemFactory[coreIdx]);

    if (tb->transformMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->transformMemFactory[coreIdx]);
    if (tb->RoimapCuCtrlInfoMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->RoimapCuCtrlInfoMemFactory[coreIdx]);
    if (tb->RoimapCuCtrlIndexMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->RoimapCuCtrlIndexMemFactory[coreIdx]);
  }
  for(coreIdx =0; coreIdx < tb->parallelCoreNum; coreIdx++)
  {
    for (iBuf = 0; iBuf < tb->streamBufNum; iBuf ++)
    {
      if (tb->outbufMemFactory[coreIdx][iBuf].virtualAddress != NULL)
        EWLFreeLinear(ewl_inst, &tb->outbufMemFactory[coreIdx][iBuf]);

	  if (tb->Av1PreoutbufMemFactory[coreIdx][iBuf].virtualAddress != NULL)
          EWLFreeLinear(ewl_inst, &tb->Av1PreoutbufMemFactory[coreIdx][iBuf]);
    }

    if (tb->extSRAMMemFactory[coreIdx].virtualAddress != NULL)
      EWLFreeLinear(ewl_inst, &tb->extSRAMMemFactory[coreIdx]);
  }

  for(coreIdx =0; coreIdx < tb->buffer_cnt; coreIdx++)
  {
    for(iBuf = 0; iBuf < MAX_OVERLAY_NUM; iBuf++)
    {
      if(tb->overlayInputMemFactory[coreIdx][iBuf].virtualAddress != NULL)
	  {
	    EWLFreeLinear(ewl_inst, &tb->overlayInputMemFactory[coreIdx][iBuf]);
      }
    }
  }
}

/*------------------------------------------------------------------------------

    CheckArea

------------------------------------------------------------------------------*/
i32 CheckArea(VCEncPictureArea *area, commandLine_s *cml)
{
  i32 w = (cml->width + cml->max_cu_size - 1) / cml->max_cu_size;
  i32 h = (cml->height + cml->max_cu_size - 1) / cml->max_cu_size;

  if ((area->left < (u32)w) && (area->right < (u32)w) &&
      (area->top < (u32)h) && (area->bottom < (u32)h)) return 1;

  return 0;
}

/*    Callback function called by the encoder SW after "slice ready"
    interrupt from HW. Note that this function is not necessarily called
    after every slice i.e. it is possible that two or more slices are
    completed between callbacks.
------------------------------------------------------------------------------*/
void HEVCSliceReady(VCEncSliceReady *slice)
{
  u32 i;
  u32 streamSize;
  u32 pos;
  SliceCtl_s *ctl = (SliceCtl_s*)slice->pAppData;
  /* Here is possible to implement low-latency streaming by
   * sending the complete slices before the whole frame is completed. */
  if(ctl->multislice_encoding && (ENCH2_SLICE_READY_INTERRUPT))
  {
    pos = slice->slicesReadyPrev ? ctl->streamPos:  /* Here we store the slice pointer */
                                   0;               /* Pointer to beginning of frame */
    streamSize=0;
    for(i = slice->nalUnitInfoNumPrev; i < slice->nalUnitInfoNum; i++)
    {
      streamSize += *(slice->sliceSizes+i);
    }

    if (ctl->output_byte_stream == 0)
    {
      writeNalsBufs(ctl->outStreamFile, &slice->streamBufs,
                slice->sliceSizes + slice->nalUnitInfoNumPrev,
                slice->nalUnitInfoNum - slice->nalUnitInfoNumPrev, pos, 0, 0);
    }
    else
    {
      writeStrmBufs(ctl->outStreamFile, &slice->streamBufs, pos, streamSize, 0);
    }

    pos += streamSize;
    /* Store the slice pointer for next callback */
    ctl->streamPos = pos;
  }
}

/*------------------------------------------------------------------------------

    InitInputLineBuffer
    -get line buffer params for IRQ handle
    -get address of input line buffer
------------------------------------------------------------------------------*/
i32 InitInputLineBuffer(inputLineBufferCfg * lineBufCfg,
                           commandLine_s * cml,
                           VCEncIn * encIn,
                           VCEncInst inst,
                           struct test_bench *tb)
{
    VCEncCodingCtrl codingCfg;
    u32 stride,chroma_stride;
    VCEncGetAlignedStride(cml->lumWidthSrc,cml->inputFormat,&stride,&chroma_stride,tb->input_alignment);
    VCEncGetCodingCtrl(inst, &codingCfg);

    memset(lineBufCfg, 0, sizeof(inputLineBufferCfg));
    lineBufCfg->depth  =  codingCfg.inputLineBufDepth;
    lineBufCfg->hwHandShake = codingCfg.inputLineBufHwModeEn;
    lineBufCfg->loopBackEn = codingCfg.inputLineBufLoopBackEn;
    lineBufCfg->inst   = (void *)inst;
    //lineBufCfg->asic   = &(((struct vcenc_instance *)inst)->asic);
    lineBufCfg->wrCnt  = 0;
    lineBufCfg->inputFormat = cml->inputFormat;
    lineBufCfg->pixOnRow = stride;
    lineBufCfg->encWidth  = cml->width;
    lineBufCfg->encHeight = cml->height;
    lineBufCfg->srcHeight = cml->lumHeightSrc;
    lineBufCfg->srcVerOffset = cml->verOffsetSrc;
    lineBufCfg->getMbLines = &VCEncGetEncodedMbLines;
    lineBufCfg->setMbLines = &VCEncSetInputMBLines;
    lineBufCfg->ctbSize = IS_H264(cml->codecFormat) ? 16 : 64;
    lineBufCfg->lumSrc = tb->lum;
    lineBufCfg->cbSrc  = tb->cb;
    lineBufCfg->crSrc  = tb->cr;

    if (VCEncInitInputLineBuffer(lineBufCfg))
      return -1;

    /* loopback mode */
    if (lineBufCfg->loopBackEn && lineBufCfg->lumBuf.buf)
    {
        VCEncPreProcessingCfg preProcCfg;
        encIn->busLuma = lineBufCfg->lumBuf.busAddress;
        encIn->busChromaU = lineBufCfg->cbBuf.busAddress;
        encIn->busChromaV = lineBufCfg->crBuf.busAddress;

        /* In loop back mode, data in line buffer start from the line to be encoded*/
        VCEncGetPreProcessing(inst, &preProcCfg);
        preProcCfg.yOffset = 0;
        VCEncSetPreProcessing(inst, &preProcCfg);
    }

    return 0;
}

/* Callback function called by the encoder SW after "segment ready"
    interrupt from HW. Note that this function is called after every segment is ready.
------------------------------------------------------------------------------*/
void EncStreamSegmentReady(void *cb_data)
{
  u8 *streamBase;
  SegmentCtl_s *ctl = (SegmentCtl_s*)cb_data;

  if(ctl->streamMultiSegEn)
  {
    streamBase = ctl->streamBase + (ctl->streamRDCounter % ctl->segmentAmount) * ctl->segmentSize;

    if (ctl->output_byte_stream == 0 && ctl->startCodeDone == 0)
    {
      const u8 start_code_prefix[4] = {0x0, 0x0, 0x0, 0x1};
      fwrite(start_code_prefix, 1, 4, ctl->outStreamFile);
      ctl->startCodeDone = 1;
    }
    printf("<----receive segment irq %d\n",ctl->streamRDCounter);
    WriteStrm(ctl->outStreamFile, (u32 *)streamBase, ctl->segmentSize, 0);

    ctl->streamRDCounter++;
  }
}

