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
--  Abstract : Encoder Wrapper Layer, common parts
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "ewl.h"
#include "ewl_linux_lock.h"
#include "ewl_x280_common.h"

#include "hx280enc.h"
#include "memalloc.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#ifdef USE_EFENCE
#include "efence.h"
#endif

#ifndef MEMALLOC_MODULE_PATH
#define MEMALLOC_MODULE_PATH        "/tmp/dev/memalloc"
#endif

#ifndef ENC_MODULE_PATH
#define ENC_MODULE_PATH             "/tmp/dev/hx280"
#endif

#ifndef SDRAM_LM_BASE
#define SDRAM_LM_BASE               0x00000000
#endif

/* macro to convert CPU bus address to ASIC bus address */
#ifdef PC_PCI_FPGA_DEMO
//#define BUS_CPU_TO_ASIC(address)    (((address) & (~0xff000000)) | SDRAM_LM_BASE)
#define BUS_CPU_TO_ASIC(address,offset)    ((address) - (offset))
#else
#define BUS_CPU_TO_ASIC(address,offset)    ((address) | SDRAM_LM_BASE)
#endif

#ifdef TRACE_EWL
static const char *busTypeName[7] = { "UNKNOWN", "AHB", "OCP", "AXI", "PCI", "AXIAHB", "AXIAPB" };
static const char *synthLangName[3] = { "UNKNOWN", "VHDL", "VERILOG" };
#endif

volatile u32 asic_status = 0;

FILE *fEwl = NULL;

/* Function to test input line buffer with hardware handshake, should be set by app.
    only for internal test purpose
    expected to return written mb lines */
u32 (*pollInputLineBufTestFunc)(void) = NULL;

/* get the address and size of on-chip SRAM used for loopback linebuffer */
static i32 EWLInitLineBufSram(hx280ewl_t *enc)
{
    if (!enc)
        return EWL_ERROR;

    /* By default, VC8000E doesn't contain such on-chip SRAM.
           In the special case of FPGA verification for line buffer,
           there is a SRAM with some registers,
           used for loopback line buffer and emulation of HW handshake*/
    enc->lineBufSramBase = 0;
    enc->lineBufSramSize = 0;
    enc->pLineBufSram = MAP_FAILED;

#ifdef PCIE_FPGA_VERI_LINEBUF
    if(ioctl(enc->fd_enc, HX280ENC_IOCGSRAMOFFSET,  &enc->lineBufSramBase) == -1)
    {
        PTRACE("ioctl HX280ENC_IOCGSRAMOFFSET failed\n");
        return EWL_ERROR;
    }
    if(ioctl(enc->fd_enc, HX280ENC_IOCGSRAMEIOSIZE,  &enc->lineBufSramSize) == -1)
    {
        PTRACE("ioctl HX280ENC_IOCGSRAMEIOSIZE failed\n");
        return EWL_ERROR;
    }

    /* map srame address to user space */
    enc->pLineBufSram = (u32 *) mmap(0, enc->lineBufSramSize, PROT_READ | PROT_WRITE, MAP_SHARED, enc->fd_mem, enc->lineBufSramBase);
    if(enc->pLineBufSram == MAP_FAILED)
    {
        PTRACE("EWLInit: Failed to mmap SRAM Address!\n");
        return EWL_ERROR;
    }
    enc->lineBufSramBase = 0x3FE00000;
#endif

    return EWL_OK;
}

int MapAsicRegisters(void * dev)
{
    unsigned long base;
    unsigned int size;
    u32 *pRegs;
    hx280ewl_t * ewl = (hx280ewl_t *)dev;
    u32 i;
    subsysReg *reg;
    SUBSYS_CORE_INFO info;
    u32 core_id;

    for(i=0 ;i<EWLGetCoreNum(); i++)
    {
      reg = &ewl->reg_all_cores[i];
      base = size = i;
      ioctl(ewl->fd_enc, HX280ENC_IOCGHWOFFSET, &base);
      ioctl(ewl->fd_enc, HX280ENC_IOCGHWIOSIZE, &size);

      /* map hw registers to user space */
      pRegs =
          (u32 *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                       ewl->fd_mem, base);
      if(pRegs == MAP_FAILED)
      {
          PTRACE("EWLInit: Failed to mmap regs\n");
          return -1;
      }
      reg->pRegBase = pRegs;
      reg->regSize = size;
      reg->subsys_id = i;

      info.type_info = i;
      ioctl(ewl->fd_enc, HX280ENC_IOCG_CORE_INFO, &info);
      for (core_id = 0; core_id < CORE_MAX; core_id++)
      {
        if(info.type_info & (1 << core_id))
        {
          reg->core[core_id].core_id = core_id;
          reg->core[core_id].regSize = info.regSize[core_id];
          reg->core[core_id].regBase = base;
          reg->core[core_id].pRegBase = (u32*)((u8*)pRegs + info.offset[core_id]);
        }
        else
          reg->core[core_id].core_id = -1;
      }
      PTRACE("EWLInit: mmap regs %d bytes --> %p\n", size, pRegs);
    }

    return 0;
}

/*******************************************************************************
 Function name   : EWLReadAsicID
 Description     : Read ASIC ID register, static implementation
 Return type     : u32 ID
 Argument        : void
*******************************************************************************/
u32 EWLReadAsicID(u32 core_id)
{
    u32 id = ~0;
    int fd_mem = -1, fd_enc = -1;
    unsigned long base = ~0;
    unsigned int size;
    u32 *pRegs = MAP_FAILED;
    u32 core_num = 0;
    SUBSYS_CORE_INFO info;

    if(fEwl == NULL)
      fEwl = fopen("ewl.trc", "w");

    fd_mem = open("/dev/mem", O_RDONLY);
    if(fd_mem == -1)
    {
        PTRACE("EWLReadAsicID: failed to open: %s\n", "/dev/mem");
        goto end;
    }

    fd_enc = open(ENC_MODULE_PATH, O_RDONLY);
    if(fd_enc == -1)
    {
        PTRACE("EWLReadAsicID: failed to open: %s\n", ENC_MODULE_PATH);
        goto end;
    }

    core_num = EWLGetCoreNum();

    if (core_id > core_num-1)
      goto end;

    /* ask module for base */
    base = core_id;
    if(ioctl(fd_enc, HX280ENC_IOCGHWOFFSET, &base) == -1)
    {
      PTRACE("ioctl failed\n");
      goto end;
    }

    size = core_id;
    if(ioctl(fd_enc, HX280ENC_IOCGHWIOSIZE, &size) == -1)
    {
      PTRACE("ioctl failed\n");
      goto end;
    }

    /* map hw registers to user space */
    pRegs = (u32 *) mmap(0, size, PROT_READ, MAP_SHARED, fd_mem, base);

    if(pRegs == MAP_FAILED)
    {
       PTRACE("EWLReadAsicID: Failed to mmap regs\n");
       goto end;
    }

    info.type_info = core_id;
    if(ioctl(fd_enc, HX280ENC_IOCG_CORE_INFO, &info) == -1)
    {
      PTRACE("ioctl failed\n");
      goto end;
    }

    core_id = GET_ENCODER_IDX(info.type_info);
    id = *((u32 *)((u8*)pRegs + info.offset[core_id]));
    
  end:
    if(pRegs != MAP_FAILED)
        munmap(pRegs, size);
    if(fd_mem != -1)
        close(fd_mem);
    if(fd_enc != -1)
        close(fd_enc);

    PTRACE("EWLReadAsicID: 0x%08x at 0x%08lx\n", id, base);

    return id;
}

/*******************************************************************************
 Function name   : EWLGetCoreNum
 Description     : Get the total num of cores
 Return type     : u32 ID
 Argument        : void
*******************************************************************************/
u32 EWLGetCoreNum(void)
{
    int fd_mem = -1, fd_enc = -1;
    static u32 core_num = 0;

    if (core_num == 0)
    {
      if(fEwl == NULL)
        fEwl = fopen("ewl.trc", "w");

      fd_mem = open("/dev/mem", O_RDONLY);
      if(fd_mem == -1)
      {
        PTRACE("EWLGetCoreNum: failed to open: %s\n", "/dev/mem");
        goto end;
      }

      fd_enc = open(ENC_MODULE_PATH, O_RDONLY);
      if(fd_enc == -1)
      {
        PTRACE("EWLGetCoreNum: failed to open: %s\n", ENC_MODULE_PATH);
        goto end;
      }

      ioctl(fd_enc, HX280ENC_IOCG_CORE_NUM, &core_num);

      end:
      if(fd_mem != -1)
        close(fd_mem);
      if(fd_enc != -1)
        close(fd_enc);
    }

    PTRACE("EWLGetCoreNum: %d\n",core_num);

    return core_num;
}


/*******************************************************************************
 Function name   : EWLReadAsicConfig
 Description     : Reads ASIC capability register, static implementation
 Return type     : EWLHwConfig_t
 Argument        : void
*******************************************************************************/
EWLHwConfig_t EWLReadAsicConfig(u32 core_id)
{
    int fd_mem = -1, fd_enc = -1;
    unsigned long base;
    unsigned int size;
    u32 *pRegs = MAP_FAILED, cfgval, *pCoreRegs = NULL; 
    u32 id = ~0;
    EWLHwConfig_t cfg_info;
    SUBSYS_CORE_INFO info;

    memset(&cfg_info, 0, sizeof(cfg_info));

    if (core_id > EWLGetCoreNum()-1)
      goto end;

    fd_mem = open("/dev/mem", O_RDONLY);
    if(fd_mem == -1)
    {
        PTRACE("EWLReadAsicConfig: failed to open: %s\n", "/dev/mem");
        goto end;
    }

    fd_enc = open(ENC_MODULE_PATH, O_RDONLY);
    if(fd_enc == -1)
    {
        PTRACE("EWLReadAsicConfig: failed to open: %s\n", ENC_MODULE_PATH);
        goto end;
    }

    base = core_id;
    size = core_id;
    ioctl(fd_enc, HX280ENC_IOCGHWOFFSET, &base);
    ioctl(fd_enc, HX280ENC_IOCGHWIOSIZE, &size);

    /* map hw registers to user space */
    pRegs = (u32 *) mmap(0, size, PROT_READ, MAP_SHARED, fd_mem, base);

    if(pRegs == MAP_FAILED)
    {
        PTRACE("EWLReadAsicConfig: Failed to mmap regs\n");
        goto end;
    }

    info.type_info = core_id;
    if(ioctl(fd_enc, HX280ENC_IOCG_CORE_INFO, &info) == -1)
    {
      PTRACE("ioctl failed\n");
      goto end;
    }

    core_id = GET_ENCODER_IDX(info.type_info);
    pCoreRegs = (u32 *)((u8*)pRegs + info.offset[core_id]);
    
    id = pCoreRegs[0];
    
    cfgval = pCoreRegs[80];

    cfg_info.h264Enabled = (cfgval >> 31) & 1;
    cfg_info.scalingEnabled = (cfgval >> 30) & 1;
    cfg_info.bFrameEnabled = (cfgval >> 29) & 1;
    cfg_info.rgbEnabled = (cfgval >> 28) & 1;
    cfg_info.hevcEnabled = (cfgval >> 27) & 1;
    cfg_info.vp9Enabled = (cfgval >> 26) & 1;
    cfg_info.deNoiseEnabled = (cfgval >> 25) & 1;
    cfg_info.main10Enabled = (cfgval >> 24) & 1;
    cfg_info.busType = (cfgval >> 21) & 7;
    cfg_info.cavlcEnable = (cfgval >> 20) & 1;
    cfg_info.lineBufEnable = (cfgval >> 19) & 1;
    cfg_info.progRdoEnable = (cfgval >> 18) & 1;
    cfg_info.rfcEnable = (cfgval >> 17) & 1;
    cfg_info.tu32Enable = (cfgval >> 16) & 1;
    cfg_info.jpegEnabled=(cfgval >> 15) & 1;
    cfg_info.busWidth = (cfgval >> 13) & 3;
    cfg_info.maxEncodedWidthH264 =
    cfg_info.maxEncodedWidthJPEG =
    cfg_info.maxEncodedWidthHEVC = cfgval & ((1 << 13) - 1);

    if (id >= 0x80006001)
    {
      cfgval = pCoreRegs[214];
      cfg_info.ljpegSupport = (cfgval >> 31) & 1;
      cfg_info.roiAbsQpSupport = (cfgval >> 30) & 1;
      cfg_info.intraTU32Enable =  (cfgval >> 29) & 1;
      cfg_info.roiMapVersion = (cfgval >> 26) & 7;
      if (id >= 0x80006200) {
        cfg_info.maxEncodedWidthHEVC <<= 3;
        cfg_info.maxEncodedWidthH264 = ((cfgval >> 13) & ((1 << 13) - 1)) << 3;
        cfg_info.maxEncodedWidthJPEG = (cfgval & ((1 << 13) - 1)) << 3;
      }
    }

    if (id >= 0x80006200)
    {
      cfgval = pCoreRegs[226];
      cfg_info.ssimSupport = (cfgval >> 31) & 1;
      cfg_info.P010RefSupport = (cfgval >> 30) & 1;
      cfg_info.cuInforVersion = (cfgval >> 27) & 7;
      cfg_info.meVertSearchRangeHEVC = (cfgval >> 21) & 0x3f;
      cfg_info.meVertSearchRangeH264 = (cfgval >> 15) & 0x3f;
      cfg_info.ctbRcVersion = (cfgval >> 12) & 7;
      cfg_info.jpeg422Support = (cfgval >> 11) & 1;
      cfg_info.gmvSupport = (cfgval >> 10) & 1;
      cfg_info.ROI8Support = (cfgval >> 9) & 1;
      cfg_info.meHorSearchRangeBframe = (cfgval >> 7) & 3;
      cfg_info.RDOQSupportHEVC = (cfgval >> 6) & 1;
      cfg_info.bMultiPassSupport = (cfgval >> 5) & 1;
      cfg_info.inLoopDSRatio = (cfgval >> 4) & 1;
      cfg_info.streamBufferChain = (cfgval >> 3) & 1;
      cfg_info.streamMultiSegment = (cfgval >> 2) & 1;
      cfg_info.IframeOnly = (cfgval >> 1) & 1;
      cfg_info.dynamicMaxTuSize = (cfgval & 1);

      cfgval = pCoreRegs[287];
      cfg_info.videoHeightExt = (cfgval >> 31) & 1;
      cfg_info.cscExtendSupport = (cfgval >> 30) & 1;
      cfg_info.scaled420Support = (cfgval >> 29) & 1;
      cfg_info.cuTreeSupport = (cfgval >> 28) & 1;
      cfg_info.maxAXIAlignment = (cfgval >> 24) & 0xf;
      cfg_info.meVertRangeProgramable = (cfgval >> 22) & 1;
      cfg_info.MonoChromeSupport  = (cfgval >> 21) & 1;
      cfg_info.ExtSramSupport  = (cfgval >> 20) & 1;
      cfg_info.vsSupport = (cfgval >> 19) & 1;
      cfg_info.RDOQSupportH264 = (cfgval >> 18) & 1;
      cfg_info.disableRecWtSupport = (cfgval >> 17) & 1;
      cfg_info.OSDSupport = (cfgval >> 16) & 1;
	  cfg_info.H264NalRefIdc2bit = (cfgval >> 15) & 1;
    }

    PTRACE("EWLReadAsicConfig:\n"
           "    maxEncodedWidthHEVC   = %d\n"
           "    maxEncodedWidthH264   = %d\n"
           "    maxEncodedWidthJPEG   = %d\n"
           "    hevcEnabled       = %s\n"
           "    h264Enabled       = %s\n"
           "    vp9Enabled        = %s\n"
           "    rgbEnabled        = %s\n"
           "    scalingEnabled    = %s\n"
           "    busType           = %s\n"
           "    busWidth          = %d\n"
           "    bFrameEnabled     = %s\n"
           "    deNoiseEnabled       = %s\n"
           "    main10Enabled       = %s\n"
           "    cavlcEnable       = %s\n"
           "    lineBufEnable       = %s\n"
           "    progRdoEnable       = %s\n"
           "    rfcEnable       = %s\n"
           "    tu32Enable       = %s\n"
           "    jpegEnabled       = %s\n"
           "    ljpegEnabled       = %s\n"
           "    absqpEnabled       = %s\n"
           "    IntraTU32x32Enabled       = %s\n"
           "    roiMapVersion       = %d\n"
           "    ssimEnabled         = %s\n"
           "    P010RefEnabled      = %s\n"
           "    cuInforVersion      = %d\n"
           "    meVerticalSearchRangeHEVC      = +- %d\n"
           "    meVerticalSearchRangeH264        = +- %d\n"
           "    ctbRcVersion        = %d\n"
           "    jpeg422Enabled       = %s\n"
           "    gmvSupport          = %s\n"
           "    ROI8Support         = %s\n"
           "    me4nHorizontalSearchRange      = +- %d\n"
           "    RDOQSupportHEVC     = %s\n"
           "    MultiPassSupport     = %s\n"
           "    inLoopDSRatio        = %s\n"
           "    streamBufferChain    = %s\n"
           "    streamMultiSegment   = %s\n"
           "    IframeOnly        = %s\n"
           "    dynamicMaxTuSize     = %s\n"
           "    videoHeightExt       = %s\n"
           "    colorConversionExt   = %s\n"
           "    scaled420Support    = %s\n"
           "    cuTreeSupport    = %s\n"
           "    maxAXIAlignment    = %d\n"
           "    meVertRangeProgramable    = %s\n"
           "    ExtSramSupport    = %s\n"
           "    RDOQSupportH264   = %s\n"
           "    disableRecWtSupport   = %s\n",
           cfg_info.maxEncodedWidthHEVC,
           cfg_info.maxEncodedWidthH264,
           cfg_info.maxEncodedWidthJPEG,
           cfg_info.hevcEnabled == 1 ? "YES" : "NO",
           cfg_info.h264Enabled == 1 ? "YES" : "NO",
           cfg_info.vp9Enabled == 1 ? "YES" : "NO",
           cfg_info.rgbEnabled == 1 ? "YES" : "NO",
           cfg_info.scalingEnabled == 1 ? "YES" : "NO",
           cfg_info.busType < 7 ? busTypeName[cfg_info.busType] : "UNKNOWN",
           (cfg_info.busWidth+1) * 32,
           cfg_info.bFrameEnabled == 1 ? "YES" : "NO",
           cfg_info.deNoiseEnabled == 1 ? "YES" : "NO",
           cfg_info.main10Enabled == 1 ? "YES" : "NO",
           cfg_info.cavlcEnable == 1 ? "YES" : "NO",
           cfg_info.lineBufEnable == 1 ? "YES" : "NO",
           cfg_info.progRdoEnable == 1 ? "YES" : "NO",
           cfg_info.rfcEnable == 1 ? "YES" : "NO",
           cfg_info.tu32Enable == 1 ? "YES" : "NO",
           cfg_info.jpegEnabled == 1 ? "YES" : "NO",
           cfg_info.ljpegSupport == 1 ? "YES" : "NO",
           cfg_info.roiAbsQpSupport == 1 ? "YES" : "NO",
           cfg_info.intraTU32Enable == 1 ? "YES" : "NO",
           cfg_info.roiMapVersion,
           cfg_info.ssimSupport == 1 ? "YES" : "NO",
           cfg_info.P010RefSupport == 1 ? "YES" : "NO",
           cfg_info.cuInforVersion,
           (cfg_info.meVertSearchRangeHEVC==0)? 40: (cfg_info.meVertSearchRangeHEVC<<3),
           (cfg_info.meVertSearchRangeH264==0)? 24: (cfg_info.meVertSearchRangeH264<<3),
           cfg_info.ctbRcVersion,
           cfg_info.jpeg422Support == 1 ? "YES" : "NO",
           cfg_info.gmvSupport == 1 ? "YES" : "NO",
           cfg_info.ROI8Support == 1 ?"YES" : "NO",
           ((cfg_info.meHorSearchRangeBframe+1)<<6),
           cfg_info.RDOQSupportHEVC == 1 ? "YES" : "NO",
           cfg_info.bMultiPassSupport == 1 ? "YES" : "NO",
           cfg_info.inLoopDSRatio == 1 ? "YES" : "NO",
           cfg_info.streamBufferChain == 1 ? "YES" : "NO",
           cfg_info.streamMultiSegment == 1 ? "YES" : "NO",
           cfg_info.IframeOnly == 1 ? "YES" : "NO",
           cfg_info.dynamicMaxTuSize == 1 ? "YES" : "NO",
           cfg_info.videoHeightExt == 1 ? "YES" : "NO",
           cfg_info.cscExtendSupport == 1 ? "YES" : "NO",
           cfg_info.scaled420Support == 1 ? "YES" : "NO",
           cfg_info.cuTreeSupport == 1 ? "YES" : "NO",
           cfg_info.maxAXIAlignment,
           cfg_info.meVertRangeProgramable == 1 ? "YES" : "NO",
           cfg_info.ExtSramSupport == 1 ? "YES" : "NO",
           cfg_info.RDOQSupportH264 == 1 ? "YES" : "NO",
           cfg_info.disableRecWtSupport == 1 ? "YES" : "NO"
           );
  end:
    if(pRegs != MAP_FAILED)
        munmap(pRegs, size);
    if(fd_mem != -1)
        close(fd_mem);
    if(fd_enc != -1)
        close(fd_enc);
    return cfg_info;
}

/*******************************************************************************
 Function name   : EWLInit
 Description     : Allocate resources and setup the wrapper module
 Return type     : ewl_ret
 Argument        : void
*******************************************************************************/
const void *EWLInit(EWLInitParam_t * param)
{
    hx280ewl_t *enc = NULL;
    int i;


    PTRACE("EWLInit: Start\n");

    /* Check for NULL pointer */
    if(param == NULL || param->clientType > 4)
    {

        PTRACE(("EWLInit: Bad calling parameters!\n"));
        return NULL;
    }

    /* Allocate instance */
    if((enc = (hx280ewl_t *) EWLmalloc(sizeof(hx280ewl_t))) == NULL)
    {
        PTRACE("EWLInit: failed to alloc hx280ewl_t struct\n");
        return NULL;
    }
    memset(enc, 0, sizeof(hx280ewl_t));

    enc->clientType = param->clientType;
    enc->fd_mem = enc->fd_enc = enc->fd_memalloc = -1;
    enc->mmuEnable = param->mmuEnable;

    /* New instance allocated */
    enc->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if(enc->fd_mem == -1)
    {
        PTRACE("EWLInit: failed to open: %s\n", "/dev/mem");
        goto err;
    }

    enc->fd_enc = open(ENC_MODULE_PATH, O_RDWR);
    if(enc->fd_enc == -1)
    {
        PTRACE("EWLInit: failed to open: %s\n", ENC_MODULE_PATH);
        goto err;
    }

    enc->fd_memalloc = open(MEMALLOC_MODULE_PATH, O_RDWR);
    if(enc->fd_memalloc == -1)
    {
        PTRACE("EWLInit: failed to open: %s\n", MEMALLOC_MODULE_PATH);
        goto err;
    }

    enc->reg_all_cores = malloc(EWLGetCoreNum()* sizeof(subsysReg));
    enc->coreAmout = EWLGetCoreNum();

    /* map hw registers to user space.*/
    if(MapAsicRegisters((void *)enc) != 0)
    {
     PTRACE("EWLReserveHw map register failed\n");
     goto err;
    }

    if (EWLInitLineBufSram(enc) != EWL_OK)
    {
        PTRACE(("EWLInit: PCIE FPGA Verification Fail!\n"));
        goto err;
    }

    queue_init(&enc->freelist);
    queue_init(&enc->workers);
    for(i = 0; i < (int)EWLGetCoreNum(); i++) {
      EWLWorker *worker = malloc(sizeof(EWLWorker));
      worker->core_id = i;
      worker->next = NULL;
      queue_put(&enc->freelist, (struct node *)worker);
    }

#ifdef HANTROMMU_SUPPORT
    if(enc->mmuEnable == 1)
    {
      unsigned int enable = 1;
      int ioctl_req;
      ioctl_req = (int)HANTRO_IOCS_MMU_ENABLE;
      if (ioctl(enc->fd_enc, ioctl_req, &enable))
      {
        printf("ioctl HANTRO_IOCS_MMU_ENABLE failed\n");
        ASSERT(0);
      }
    }
#endif
    PTRACE("EWLInit: Return %0xd\n", (u32) enc);
    return enc;

  err:
    EWLRelease(enc);
    PTRACE("EWLInit: Return NULL\n");
    return NULL;
}

/*******************************************************************************
 Function name   : EWLRelease
 Description     : Release the wrapper module by freeing all the resources
 Return type     : ewl_ret
 Argument        : void
*******************************************************************************/
i32 EWLRelease(const void *inst)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 i;

    assert(enc != NULL);

    if(enc == NULL)
        return EWL_OK;

    /* Release the register mapping info of each core */
    for (i=0; i<EWLGetCoreNum(); i++)
    {
     if(enc->reg_all_cores!=NULL&&enc->reg_all_cores[i].pRegBase != MAP_FAILED)
        munmap((void *) enc->reg_all_cores[i].pRegBase, enc->reg_all_cores[i].regSize);
    }

    free(enc->reg_all_cores);
    enc->reg_all_cores = NULL;

    /* Release the sram */
    if(enc->pLineBufSram != MAP_FAILED)
        munmap((void *) enc->pLineBufSram, enc->lineBufSramSize);

    if(enc->fd_mem != -1)
        close(enc->fd_mem);
    if(enc->fd_enc != -1)
        close(enc->fd_enc);
    if(enc->fd_memalloc != -1)
        close(enc->fd_memalloc);

    free_nodes(enc->workers.tail);
    free_nodes(enc->freelist.tail);
    EWLfree(enc);

    PTRACE("EWLRelease: instance freed\n");

    if(fEwl != NULL) {
        fclose(fEwl);
        fEwl = NULL;
    }

    return EWL_OK;
}

/*******************************************************************************
 Function name   : EWLWriteReg
 Description     : Set the content of a hadware register
 Return type     : void
 Argument        : u32 offset
 Argument        : u32 val
*******************************************************************************/
void EWLWriteCoreReg(const void *inst, u32 offset, u32 val, u32 core_id)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);
    regMapping *reg = &(enc->reg_all_cores[core_id].core[core_type]);

    assert(reg != NULL && offset < reg->regSize);

    if(offset == 0x04)
    {
        //asic_status = val;
    }

    offset = offset / 4;
    *(reg->pRegBase + offset) = val;

    PTRACE("EWLWriteReg 0x%02x with value %08x\n", offset * 4, val);
}

void EWLWriteReg(const void *inst, u32 offset, u32 val)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_id = LAST_CORE(enc);
    EWLWriteCoreReg(inst, offset, val, core_id);
}

void EWLWriteBackReg(const void *inst, u32 offset, u32 val)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_id = FIRST_CORE(enc);
    EWLWriteCoreReg(inst, offset, val, core_id);
}

/*------------------------------------------------------------------------------
    Function name   : EWLEnableHW
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void EWLEnableHW(const void *inst, u32 offset, u32 val)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_id = LAST_CORE(enc);
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);
    regMapping *reg = &(enc->reg_all_cores[core_id].core[core_type]);

    assert(reg != NULL && offset < reg->regSize);

    offset = offset / 4;
    *(reg->pRegBase + offset) = val;

    PTRACE("EWLEnableHW 0x%02x with value %08x\n", offset * 4, val);
}

/*------------------------------------------------------------------------------
    Function name   : EWLDisableHW
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void EWLDisableHW(const void *inst, u32 offset, u32 val)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_id = FIRST_CORE(enc);
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);
    regMapping *reg = &(enc->reg_all_cores[core_id].core[core_type]);

    assert(reg != NULL && offset < reg->regSize);

    offset = offset / 4;
    *(reg->pRegBase + offset) = val;

    PTRACE("EWLDisableHW 0x%02x with value %08x\n", offset * 4, val);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetPerformance
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
u32 EWLGetPerformance(const void *inst)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    return enc->performance;
}

/*******************************************************************************
 Function name   : EWLReadReg
 Description     : Retrive the content of a hadware register
                    Note: The status register will be read after every MB
                    so it may be needed to buffer it's content if reading
                    the HW register is slow.
 Return type     : u32
 Argument        : u32 offset
*******************************************************************************/
u32 EWLReadReg(const void *inst, u32 offset)
{
    u32 val;
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_id = FIRST_CORE(enc);
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);
    regMapping *reg = &(enc->reg_all_cores[core_id].core[core_type]);

    assert(offset < reg->regSize);

    offset = offset / 4;
    val = *(reg->pRegBase + offset);

    PTRACE("EWLReadReg 0x%02x --> %08x\n", offset * 4, val);

    return val;
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : EWLLinearMem_t *info - place where the allocated memory
                        buffer parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocRefFrm(const void *instance, u32 size, u32 alignment,EWLLinearMem_t * info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;
    EWLLinearMem_t *buff = (EWLLinearMem_t *) info;
    i32 ret;

    assert(enc_ewl != NULL);
    assert(buff != NULL);

    PTRACE("EWLMallocRefFrm\t%8d bytes\n", size);

    ret = EWLMallocLinear(enc_ewl, size,alignment,buff);

    PTRACE("EWLMallocRefFrm %08x --> %p\n", buff->busAddress,
           buff->virtualAddress);

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : EWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        EWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - frame buffer memory information
------------------------------------------------------------------------------*/
void EWLFreeRefFrm(const void *instance, EWLLinearMem_t * info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;
    EWLLinearMem_t *buff = (EWLLinearMem_t *) info;

    assert(enc_ewl != NULL);
    assert(buff != NULL);

    EWLFreeLinear(enc_ewl, buff);

    PTRACE("EWLFreeRefFrm\t%p\n", buff->virtualAddress);
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : EWLLinearMem_t *info - place where the allocated memory
                        buffer parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocLinear(const void *instance, u32 size, u32 alignment,EWLLinearMem_t * info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;
    EWLLinearMem_t *buff = (EWLLinearMem_t *) info;

    MemallocParams params;
#ifdef HANTROMMU_SUPPORT
    int ioctl_req;
    struct addr_desc addr;
#endif

    u32 pgsize = getpagesize();

    assert(enc_ewl != NULL);
    assert(buff != NULL);

    PTRACE("EWLMallocLinear\t%8d bytes\n", size);

    if (alignment == 0)
      alignment = 1;

    buff->size = (((size + (alignment -1))& (~(alignment-1)))+ (pgsize - 1)) & (~(pgsize - 1));

    params.size = (size + (alignment -1))& (~(alignment-1));

    buff->virtualAddress = 0;
    buff->busAddress = 0;
    buff->allocVirtualAddr = 0;
    buff->allocBusAddr = 0;
    /* get memory linear memory buffers */
    ioctl(enc_ewl->fd_memalloc, MEMALLOC_IOCXGETBUFFER, &params);
    if(params.busAddress == 0)
    {
        PTRACE("EWLMallocLinear: Linear buffer not allocated\n");
        return EWL_ERROR;
    }

    /* Map the bus address to virtual address */
    buff->allocVirtualAddr = MAP_FAILED;
    buff->allocVirtualAddr = (u32 *) mmap(0, buff->size, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, enc_ewl->fd_mem,
                                        params.busAddress);

    if(buff->allocVirtualAddr == MAP_FAILED)
    {
        PTRACE("EWLInit: Failed to mmap busAddress: %p\n",
               (void *)params.busAddress);
        return EWL_ERROR;
    }

    /* ASIC might be in different address space */
    buff->allocBusAddr = BUS_CPU_TO_ASIC(params.busAddress, params.translationOffset);
    buff->busAddress = (buff->allocBusAddr + (alignment -1))& (~(((u64)alignment)-1));
    buff->virtualAddress = buff->allocVirtualAddr + (buff->busAddress - buff->allocBusAddr);

#ifdef HANTROMMU_SUPPORT
    if (enc_ewl->mmuEnable == 1)
    {
      addr.virtual_address = buff->allocVirtualAddr;
      addr.size            = params.size;

      mlock(addr.virtual_address, addr.size);
      ioctl_req = (int)HANTRO_IOCS_MMU_MEM_MAP;
      ioctl(enc_ewl->fd_enc, ioctl_req, &addr);
      buff->busAddress = addr.bus_address;
      buff->busAddress = (buff->busAddress + (alignment -1))& (~(((u64)alignment)-1));
      buff->virtualAddress = buff->allocVirtualAddr + (buff->busAddress - addr.bus_address);
    }
#endif
    if (sizeof(buff->busAddress) == 8 && (buff->busAddress >> 32)!=0)
    {
      PTRACE("EWLInit: allocated busAddress overflow 32 bit: (%p), please ensure HW support 64bits address space\n", (void *)params.busAddress);
    }

    PTRACE("EWLMallocLinear %p (CPU) %p (ASIC) --> %p\n",
           params.busAddress, buff->busAddress, buff->virtualAddress);

    return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        EWLMallocLinear.

    Return type     : void

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - linear buffer memory information
------------------------------------------------------------------------------*/
void EWLFreeLinear(const void *instance, EWLLinearMem_t * info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;
    EWLLinearMem_t *buff = (EWLLinearMem_t *) info;
#ifdef HANTROMMU_SUPPORT
    struct addr_desc addr;
    int ioctl_req;
#endif

    assert(enc_ewl != NULL);
    assert(buff != NULL);

#ifdef HANTROMMU_SUPPORT
    if(enc_ewl->mmuEnable == 1)
    {
      addr.virtual_address = buff->allocVirtualAddr;
      ioctl_req = (int)HANTRO_IOCS_MMU_MEM_UNMAP;
      ioctl(enc_ewl->fd_enc, ioctl_req, &addr);
    }
#endif

    if(buff->allocBusAddr != 0)
        ioctl(enc_ewl->fd_memalloc, MEMALLOC_IOCSFREEBUFFER, &buff->allocBusAddr);

    if(buff->allocVirtualAddr != MAP_FAILED)
        munmap(buff->allocVirtualAddr, buff->size);

    PTRACE("EWLFreeLinear\t%p\n", buff->allocVirtualAddr);
}

/*******************************************************************************
 Function name   : EWLGetDec400Coreid
 Description     : get one dec400 core id
*******************************************************************************/
i32 EWLGetDec400Coreid(const void *inst)
{
    hx280ewl_t *ewl = (hx280ewl_t *) inst;
    u32 i = 0;

    /* Check invalid parameters */
    if(ewl == NULL)
      return EWL_ERROR;

    PTRACE("EWLGetDec400Coreid: PID %d try to get dec400 core id ...\n", getpid());

    u32 core_id = LAST_CORE(ewl);
    if(ewl->reg_all_cores[core_id].core[CORE_DEC400].core_id != -1)
      return core_id;
    else
      return -1;
}

/*******************************************************************************
 Function name   : EWLCheckCutreeValid
 Description     : check it's cutree or not
*******************************************************************************/
i32 EWLCheckCutreeValid(const void *inst)
{
    hx280ewl_t *ewl = (hx280ewl_t *) inst;
    u32 i = 0, val = 0;

    /* Check invalid parameters */
    if(ewl == NULL)
      return EWL_ERROR;
    
    PTRACE("EWLGetDec400Coreid: PID %d try to get dec400 core id ...\n", getpid());

    u32 core_id = LAST_CORE(ewl);
    u32 core_type = EWLGetCoreTypeByClientType(ewl->clientType);
    regMapping *reg = &(ewl->reg_all_cores[core_id].core[core_type]);

    val = *(reg->pRegBase + 287);
    if(val & 0x10000000)
      return EWL_OK;
    else
      return EWL_ERROR;
}

/*******************************************************************************
 Function name   : EWLReserveHw
 Description     : Reserve HW resource for currently running codec
*******************************************************************************/
i32 EWLReserveHw(const void *inst, u32 *core_info)
{
    hx280ewl_t *ewl = (hx280ewl_t *) inst;
    u32 c = 0;
    u32 i = 0,valid_num =0;
    i32 ret;
    u32 temp = 0;
    u8  subsys_mapping = 0;
    u32 core_info_hw = *core_info;
    u32 core_type;
    
    /* Check invalid parameters */
    if(ewl == NULL)
      return EWL_ERROR;

    PTRACE("EWLReserveHw: PID %d trying to reserve ...\n", getpid());

    core_type = EWLGetCoreTypeByClientType(ewl->clientType);

    core_info_hw |= (core_type & 0xFF);
    ret = ioctl(ewl->fd_enc, HX280ENC_IOCH_ENC_RESERVE, &core_info_hw);

    if (ret < 0)
    {
     PTRACE("EWLReserveHw failed\n");
     return EWL_ERROR;
    }
    else
    {
     PTRACE("EWLReserveHw successed\n");
     temp = core_info_hw;
    }

    subsys_mapping = (u8)temp&0xFF;
    i = 0;
    while(subsys_mapping)
    {
     if (subsys_mapping & 0x1)
     {
       ewl->reg.core_id = i;
       ewl->reg.regSize = ewl->reg_all_cores[i].core[core_type].regSize;
       ewl->reg.regBase = ewl->reg_all_cores[i].core[core_type].regBase;
       ewl->reg.pRegBase = ewl->reg_all_cores[i].core[core_type].pRegBase;
       PTRACE("core %d is reserved\n",i);
       break;
     }
     subsys_mapping = subsys_mapping >> 1;
     i++;
    }
    EWLWorker *worker = (EWLWorker *)queue_get(&ewl->freelist);
    while(worker && worker->core_id != ewl->reg.core_id)
    {
      queue_put_tail(&ewl->freelist, (struct node *)worker);
      worker = (EWLWorker *)worker->next;
    }
    queue_remove(&ewl->freelist, (struct node *)worker);
    queue_put(&ewl->workers, (struct node *)worker);

    EWLWriteReg(ewl, 0x14, 0);//disable encoder

    PTRACE("EWLReserveHw: ENC HW locked by PID %d\n", getpid());

    return EWL_OK;
}

/*******************************************************************************
 Function name   : EWLReleaseHw
 Description     : Release HW resource when frame is ready
*******************************************************************************/
void EWLReleaseHw(const void *inst)
{
    u32 val;
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    u32 core_info = 0;
    u32 core_id = FIRST_CORE(enc);
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);

    assert(enc != NULL);

    enc->performance = EWLReadReg(inst, 82*4);//save the performance before release hw

    core_info |= core_id << 4;
    core_info |= core_type;

    val = EWLReadReg(inst, 0x14);
    EWLWriteBackReg(inst, 0x14, val & (~0x01)); /* reset ASIC */
    enc->reg.core_id = -1;
    enc->reg.regSize = 0;
    enc->reg.regBase = 0;
    enc->reg.pRegBase = NULL;

    PTRACE("EWLReleaseHw: PID %d trying to release ...\n", getpid());

    ioctl(enc->fd_enc, HX280ENC_IOCH_ENC_RELEASE, &core_info);
    EWLWorker *worker = (EWLWorker *)queue_get(&enc->workers);
    queue_remove(&enc->workers, (struct node *)worker);
    queue_put(&enc->freelist, (struct node *)worker);

    PTRACE("EWLReleaseHw: HW released by PID %d\n", getpid());
    return ;
}

/* SW/SW shared memory */
/*------------------------------------------------------------------------------
    Function name   : EWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *EWLmalloc(u32 n)
{

    void *p = malloc((size_t) n);

    PTRACE("EWLmalloc\t%8d bytes --> %p\n", n, p);

    return p;
}

/*------------------------------------------------------------------------------
    Function name   : EWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()

    Return type     : void

    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void EWLfree(void *p)
{
    PTRACE("EWLfree\t%p\n", p);
    if(p != NULL)
        free(p);
}

/*------------------------------------------------------------------------------
    Function name   : EWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *EWLcalloc(u32 n, u32 s)
{
    void *p = calloc((size_t) n, (size_t) s);

    PTRACE("EWLcalloc\t%8d bytes --> %p\n", n * s, p);

    return p;
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()

    Return type     : The value of destination d

    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *EWLmemcpy(void *d, const void *s, u32 n)
{
    return memcpy(d, s, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()

    Return type     : The value of destination d

    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
void *EWLmemset(void *d, i32 c, u32 n)
{
    return memset(d, (int) c, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemcmp
    Description     : Compares two buffers. Same functionality
                      as the ANSI C memcmp()

    Return type     : Zero if the first n bytes of s1 match s2

    Argument        : const void *s1 - Buffer to compare
    Argument        : const void *s2 - Buffer to compare
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
int EWLmemcmp(const void *s1, const void *s2, u32 n)
{
    return memcmp(s1, s2, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetLineBufSram
    Description        : Get the base address of on-chip sram used for input line buffer.

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the sram parameters are returned
------------------------------------------------------------------------------*/
i32 EWLGetLineBufSram (const void *instance, EWLLinearMem_t *info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;

    assert(enc_ewl != NULL);
    assert(info != NULL);

    if (enc_ewl->pLineBufSram != MAP_FAILED)
    {
        info->virtualAddress = (u32 *)enc_ewl->pLineBufSram;
        info->busAddress = enc_ewl->lineBufSramBase;
        info->size = enc_ewl->lineBufSramSize;
    }
    else
    {
        info->virtualAddress = NULL;
        info->busAddress = 0;
        info->size = 0;
    }

    PTRACE("EWLMallocLinear 0x%08x (ASIC) --> %p\n", info->busAddress, info->virtualAddress);

    return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLoopbackLineBuf
    Description        : allocate loopback line buffer in memory, mainly used when there is no on-chip sram

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the mem parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocLoopbackLineBuf (const void *instance, u32 size, EWLLinearMem_t *info)
{
    hx280ewl_t *enc_ewl = (hx280ewl_t *) instance;
    EWLLinearMem_t *buff = (EWLLinearMem_t *) info;
    i32 ret;

    assert(enc_ewl != NULL);
    assert(buff != NULL);

    PTRACE("EWLMallocLoopbackLineBuf\t%8d bytes\n", size);

    ret = EWLMallocLinear(enc_ewl, size,0, buff);

    PTRACE("EWLMallocLoopbackLineBuf %08x --> %p\n", buff->busAddress, buff->virtualAddress);

    return ret;
}

u32 EWLGetCoreTypeByClientType(u32 client_type)
{
  u32 core_type;
  switch (client_type)
  {
    case EWL_CLIENT_TYPE_H264_ENC:
    case EWL_CLIENT_TYPE_HEVC_ENC:
      core_type = CORE_VC8000E;
      break;
    case EWL_CLIENT_TYPE_JPEG_ENC:
      core_type = CORE_VC8000EJ;
      break;
    case EWL_CLIENT_TYPE_CUTREE:
      core_type = CORE_CUTREE;
      break;
    case EWL_CLIENT_TYPE_DEC400:
      core_type = CORE_DEC400;
      break;
    default:
      core_type = CORE_VC8000E;
      break;
  }
  
  return core_type;
  
}

u32 EWLChangeClientType(const void *inst, u32 client_type)
{
  u32 client_type_previous;
  hx280ewl_t *enc = (hx280ewl_t *) inst;

  client_type_previous = enc->clientType;
  enc->clientType = client_type; 

  return client_type_previous;
}
void EWLTraceProfile(const void *instance)
{
}
