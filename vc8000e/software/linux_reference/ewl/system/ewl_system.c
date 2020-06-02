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
--  Abstract : Encoder Wrapper Layer system model adapter
--
------------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "tools.h"

/* Common EWL interface */
#include "ewl.h"
#include "encasiccontroller.h"

/* HW register definitions */
#include "enc_core.h"

//#define CORE_NUM 4  /*move to enc_core.h*/
//#define MAX_CORE_NUM 4  /*move to enc_core.h*/

/*#define TRACE_MEM_USAGE*/

/* Macro for debug printing
 * Note that double parenthesis has to be used, i.e.
 * PTRACE(("Debug printing %d\n",%d)) */
#undef PTRACE

#ifdef TRACE_EWL
#   include <stdio.h>
#   define PTRACE(args) printf("%s:%d: ", __FILE__, __LINE__); printf args
#else
#   define PTRACE(args) /* no trace */
#endif

#define ASIC_STATUS_IRQ_INTERVAL        0x100

/* Mask fields */
#define mask_1b         (u32)0x00000001
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_8b         (u32)0x000000FF
#define mask_9b         (u32)0x000001FF
#define mask_10b        (u32)0x000003FF
#define mask_18b        (u32)0x0003FFFF


/* Page size and align for linear memory chunks. */
#define LINMEM_ALIGN    4096

#define NEXT_ALIGNED(x) ((((ptr_t)x) + LINMEM_ALIGN-1)/LINMEM_ALIGN*LINMEM_ALIGN)



typedef struct {
  struct node *next;
  int core_id;
} EWLWorker;
#define FIRST_CORE(inst) (((EWLWorker *)(inst->workers.tail))->core_id)
#define LAST_CORE(inst)  (((EWLWorker *)(inst->workers.head))->core_id)
#define LAST_WAIT_CORE(inst)  (((EWLWorker *)(inst->freelist.head))->core_id)

#define BUS_WIDTH_128_BITS 128
#define BUS_WRITE_U32_CNT_MAX   (BUS_WIDTH_128_BITS/(8*sizeof(u32)))
#define BUS_WRITE_U8_CNT_MAX   (BUS_WIDTH_128_BITS/(8*sizeof(u8)))


/* Maximum number of chunks allowed. */
#define MEM_CHUNKS      720

struct qualityProfile
{
    i32 frame_number;
    float total_bits;
    float total_Y_PSNR;
    float total_U_PSNR;
    float total_V_PSNR;
    float Y_PSNR_Max;
    float Y_PSNR_Min;
    float U_PSNR_Max;
    float U_PSNR_Min;
    float V_PSNR_Max;
    float V_PSNR_Min;
    double total_ssim;
    double total_ssim_y;
    double total_ssim_u;
    double total_ssim_v;
    i32 QP_Max;
    i32 QP_Min;
};

typedef struct
{
  u32 refFrmAlloc[MEM_CHUNKS];
  u32 linMemAlloc[MEM_CHUNKS];
  u32 refFrmChunks;
  u32 linMemChunks;
  u32 totalChunks;                /* Total malloc's */
  u32 *chunks[MEM_CHUNKS];        /* Returned by malloc */
  u32 *alignedChunks[MEM_CHUNKS]; /* Aligned */
  struct queue freelist;
  struct queue workers;
  u8 *streamBase;
  u8 *streamTempBuffer;
  u32 streamLength;
  u8 segmentAmount;
  i8 frameRdy;
  u32 clientType;

  struct qualityProfile prof;

  WorkInst winst;
  
  SysCore *core[MAX_CORE_NUM];

} ewlSysInstance;


/* Global variables */
static u32 vopNum;
static u32 memAlloc[MEM_CHUNKS];
static u32 memAllocTotal;
static u32 memChunks;

/* Function to test input line buffer with hardware handshake, should be set by app. (invalid for system) */
u32 (*pollInputLineBufTestFunc)(void) = NULL;

/*------------------------------------------------------------------------------

------------------------------------------------------------------------------*/
u32 EWLReadAsicID(u32 core_id)
{
  SysCore *core;
  
  if (core_id>=CORE_NUM)
    return 0;
    
  core = CoreEncSetup(core_id);
  
  return CoreEncGetRegister(core, 0);
}

/*******************************************************************************
 Function name   : EWLReadAsicConfig
 Description     : Reads ASIC capability register, static implementation
 Return type     : EWLHwConfig_t
 Argument        : void
*******************************************************************************/
EWLHwConfig_t EWLReadAsicConfig(u32 core_id)
{
  SysCore *core = CoreEncSetup(core_id);
  u32 cfg_register;
  EWLHwConfig_t cfg_info;
  u32 hw_modules_cfg1 = CoreEncGetRegister(core, HWIF_REG_CFG1*4);
  u32 hw_modules_cfg2 = CoreEncGetRegister(core, HWIF_REG_CFG2*4);
  u32 hw_modules_cfg3 = CoreEncGetRegister(core, HWIF_REG_CFG3*4);
  u32 hw_modules_cfg4 = CoreEncGetRegister(core, HWIF_REG_CFG4*4);

  memset(&cfg_info, 0, sizeof(EWLHwConfig_t));
  
  //HEncSetRegister(inst->asicRegs, HWIF_REG_CFG1*4, hw_modules_cfg1);
  //cfg_register=HEncGetRegister(inst->asicRegs, HWIF_REG_CFG1*4);
  cfg_register=hw_modules_cfg1;
  cfg_info.h264Enabled = (cfg_register >> 31) & 1;
  cfg_info.scalingEnabled = (cfg_register >> 30) & 1;
  cfg_info.bFrameEnabled = (cfg_register >> 29) & 1;
  cfg_info.rgbEnabled = (cfg_register >> 28) & 1;
  cfg_info.hevcEnabled = (cfg_register >> 27) & 1;
  cfg_info.vp9Enabled = (cfg_register >> 26) & 1;
  cfg_info.deNoiseEnabled=(cfg_register >> 25) & 1;
  cfg_info.main10Enabled=(cfg_register >> 24) & 1;
  cfg_info.busType = (cfg_register >> 21) & 7;
  cfg_info.cavlcEnable = (cfg_register >> 20) & 1;
  cfg_info.lineBufEnable = (cfg_register >> 19) & 1; 
  cfg_info.progRdoEnable = (cfg_register >> 18) & 1;
  cfg_info.rfcEnable = (cfg_register >> 17) & 1;        
  cfg_info.tu32Enable = (cfg_register >> 16) & 1; 
  cfg_info.jpegEnabled=(cfg_register >> 15) & 1;
  cfg_info.busWidth = (cfg_register >> 13) & 3;
  cfg_info.maxEncodedWidthH264 =
  cfg_info.maxEncodedWidthJPEG =
  cfg_info.maxEncodedWidthHEVC = cfg_register & ((1 << 13) - 1);
  
  if (EWLReadAsicID(0)>=0x80006001)
  {
    //HEncSetRegister(inst->asicRegs, HWIF_REG_CFG2*4, hw_modules_cfg2);
    //cfg_register=HEncGetRegister(inst->asicRegs, HWIF_REG_CFG2*4);
    cfg_register=hw_modules_cfg2;
    cfg_info.ljpegSupport = (cfg_register >> 31) & 1;
    cfg_info.roiAbsQpSupport = (cfg_register >> 30) & 1;
    cfg_info.intraTU32Enable = (cfg_register >> 29) & 1;    
    cfg_info.roiMapVersion = (cfg_register >> 26) & 7;
    if (EWLReadAsicID(0) >= 0x80006200) {
      cfg_info.maxEncodedWidthHEVC <<= 3;
      cfg_info.maxEncodedWidthH264 = ((cfg_register >> 13) & ((1 << 13) - 1)) << 3;
      cfg_info.maxEncodedWidthJPEG = (cfg_register & ((1 << 13) - 1)) << 3;
    }
  }  

  if (EWLReadAsicID(0)>=0x80006200)
  {
    //CoreEncSetRegister(inst->asicRegs, HWIF_REG_CFG3*4, hw_modules_cfg3);
    //cfg_register=CoreEncGetRegister(inst->asicRegs, HWIF_REG_CFG3*4);
    cfg_register=hw_modules_cfg3;
    cfg_info.ssimSupport = (cfg_register >> 31) & 1;
    cfg_info.P010RefSupport = (cfg_register >> 30) & 1;
    cfg_info.cuInforVersion = (cfg_register >> 27) & 7;
    cfg_info.meVertSearchRangeHEVC= (cfg_register >> 21) & 0x3f;
    cfg_info.meVertSearchRangeH264= (cfg_register >> 15) & 0x3f;
    cfg_info.ctbRcVersion = (cfg_register >> 12) & 7;
    cfg_info.jpeg422Support = (cfg_register >> 11) & 1;   
    cfg_info.gmvSupport = (cfg_register >> 10) & 1;
    cfg_info.ROI8Support = (cfg_register >> 9) & 1;
    cfg_info.meHorSearchRangeBframe = (cfg_register >> 7) & 3;
    cfg_info.RDOQSupportHEVC = (cfg_register >> 6) & 1;
    cfg_info.bMultiPassSupport = (cfg_register >> 5) & 1;
    cfg_info.inLoopDSRatio = (cfg_register >> 4) & 1;    
    cfg_info.streamBufferChain = (cfg_register >> 3) & 1;
    cfg_info.streamMultiSegment = (cfg_register >> 2) & 1;
    cfg_info.IframeOnly = (cfg_register >> 1) & 1;
    cfg_info.dynamicMaxTuSize = (cfg_register & 1);

    cfg_register=hw_modules_cfg4;
    cfg_info.videoHeightExt = (cfg_register >> 31) & 1;
    cfg_info.cscExtendSupport = (cfg_register >> 30) & 1;
    cfg_info.scaled420Support = (cfg_register >> 29) & 1;
    cfg_info.cuTreeSupport = (cfg_register >> 28) & 1;
    cfg_info.maxAXIAlignment = (cfg_register >> 24) & 0xf;
    cfg_info.meVertRangeProgramable = (cfg_register >> 22) & 1;
    cfg_info.MonoChromeSupport  = (cfg_register >> 21) & 1;
    cfg_info.ExtSramSupport  = (cfg_register >> 20) & 1;
    cfg_info.vsSupport = (cfg_register >> 19) & 1;
    cfg_info.RDOQSupportH264 = (cfg_register >> 18) & 1;
    cfg_info.disableRecWtSupport = (cfg_register >> 17) & 1;
    cfg_info.OSDSupport = (cfg_register >> 16) & 1;
	cfg_info.H264NalRefIdc2bit = (cfg_register >> 15) & 1;
  }
  
  return cfg_info;
}

/*------------------------------------------------------------------------------

    System model adapter for EWL.

------------------------------------------------------------------------------*/
u32 EWLReadReg(const void *inst, u32 offset)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  assert(inst != NULL);
  assert(offset < ASIC_SWREG_AMOUNT * 4);
  u32 core_id = FIRST_CORE(instance);
  SysCore *core = instance->core[core_id];
  (void) inst;

  return CoreEncGetRegister(core, offset);
}

void EWLWriteCoreReg(const void *inst, u32 offset, u32 val, u32 core_id)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  assert(inst != NULL);
  assert(offset < ASIC_SWREG_AMOUNT * 4);
  SysCore *core = instance->core[core_id];
  (void) inst;

  PTRACE(("EWLWriteReg 0x%02x with value %08x\n", offset, val));

  CoreEncSetRegister(core, offset, val);

}
void EWLWriteReg(const void *inst, u32 offset, u32 val)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  u32 core_id = LAST_CORE(instance);
  EWLWriteCoreReg(inst, offset, val, core_id);
}
void EWLWriteBackReg(const void *inst, u32 offset, u32 val)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  u32 core_id = FIRST_CORE(instance);
  EWLWriteCoreReg(inst, offset, val, core_id);
}

u32 EWLGetCoreTypeByClientType(u32 client_type)
{
  return 0;
}

u32 EWLChangeClientType(const void *inst, u32 client_type)
{
  return 0;
}

i32 EWLCheckCutreeValid(const void *inst)
{
  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLEnableHW
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void EWLEnableHW(const void *instance, u32 offset, u32 val)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  assert(inst != NULL);
  (void) inst;
  (void) val;
  PTRACE(("EWLEnableHW 0x%02x with value %08x\n", offset, val));
  u32 core_id = LAST_CORE(inst);
  SysCore *core = inst->core[core_id];
  u32 width = 0, height = 0;

  CoreEncSetRegister(core, offset, val);

  if (offset == (ASIC_REG_INDEX_STATUS * 4))
  {
    if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_EN))
    {
      if (CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_HEVC || 
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_H264 ||
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_AV1  ||
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_VP9)
      {
        width = (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH) 
               | (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH_MSB) << 10) 
               | (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH_MSB2) << 12)) * 8;
        height = CoreEncGetRegisterValue(core, HWIF_ENC_PIC_HEIGHT) * 8;
      }
      else
      {
        width = (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_WIDTH) 
                | (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_WIDTH_MSB)<<12))*8;
        height = (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_HEIGHT) 
                | (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_HEIGHT_MSB)<<12))*8;
      }
      
      inst->streamTempBuffer = calloc(width*height*2*4, sizeof(u8));

      inst->streamLength = 0;
      inst->frameRdy = -1;
      inst->segmentAmount = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT)/CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SIZE);
      inst->streamBase = (u8 *)CoreEncGetAddrRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BASE);
      CoreEncSetAddrRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BASE, (ptr_t)inst->streamTempBuffer);
      CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR, 0);
    }
  }
    
  CoreEncRun(inst->core[core_id], offset, val, &inst->winst);
  
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

  assert(inst != NULL);
  (void) inst;
  (void) val;
  if (offset != (4 * 4))
  {
    assert(0);
  }

  PTRACE(("EWLDisableHW 0x%02x with value %08x\n", offset * 4, val));
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetPerformance
    Description     : 
    Return type     : void 
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
u32 EWLGetPerformance(const void *instance)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 core_id = LAST_WAIT_CORE(inst);
  SysCore *core = inst->core[core_id];
  return CoreEncGetRegister(core, 82*4);
}

i32 EWLGetDec400Coreid(const void *inst)
{
  return -1;
}
/*------------------------------------------------------------------------------
    Function name   : EWLInit
    Description     :
    Return type     : void
    Argument        : const void *inst
------------------------------------------------------------------------------*/
const void *EWLInit(EWLInitParam_t *param)
{
  int i;
  ewlSysInstance *inst;

  if (param == NULL || param->clientType > EWL_CLIENT_TYPE_VIDEOSTAB) return NULL;

  inst = (ewlSysInstance *)malloc(sizeof(ewlSysInstance));
  if (inst == NULL) return NULL;
  memset(inst,0,sizeof(ewlSysInstance));
  
  for (i = 0; i < MAX_CORE_NUM; i++)
    inst->core[i] = CoreEncSetup(i);

  inst->clientType = param->clientType;
  inst->linMemChunks = 0;
  inst->refFrmChunks = 0;
  inst->totalChunks = 0;
  inst->streamTempBuffer = NULL;

  vopNum = 0;
  memAllocTotal = 0;
  memChunks = 0;

  inst->prof.frame_number = 0;
  inst->prof.total_bits = 0;
  inst->prof.total_Y_PSNR = 0.0;
  inst->prof.total_U_PSNR = 0.0;
  inst->prof.total_V_PSNR = 0.0;
  inst->prof.Y_PSNR_Max = -1;
  inst->prof.Y_PSNR_Min = 1000.0;
  inst->prof.U_PSNR_Max = -1;
  inst->prof.U_PSNR_Min = 1000.0;
  inst->prof.V_PSNR_Max = -1;
  inst->prof.V_PSNR_Min = 1000.0;
  inst->prof.total_ssim = 0.0;
  inst->prof.total_ssim_y = 0.0;
  inst->prof.total_ssim_u = 0.0;
  inst->prof.total_ssim_v = 0.0;
  inst->prof.QP_Max = 0;
  inst->prof.QP_Min = 52;

  queue_init(&inst->freelist);
  queue_init(&inst->workers);
  for(i = 0; i < (param->clientType == EWL_CLIENT_TYPE_JPEG_ENC ? 1 : (int)EWLGetCoreNum()); i++) {
    EWLWorker *worker = malloc(sizeof(EWLWorker));
    worker->core_id = i;
    worker->next = NULL;
    queue_put(&inst->freelist, (struct node *)worker);
  }
  return (void *) inst;
}

/*------------------------------------------------------------------------------
    Function name   : EWLRelease
    Description     :
    Return type     : void
    Argument        : const void *inst
------------------------------------------------------------------------------*/
i32 EWLRelease(const void *instance)
{
  u32 i, tothw, totswhw;
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 core_id = LAST_WAIT_CORE(inst);
  SysCore *core = inst->core[core_id];

  assert(inst != NULL);

  CoreEncRelease(core, &inst->winst, inst->clientType);
  
  if (inst->streamTempBuffer)
    free(inst->streamTempBuffer);

  for (i = 0, tothw = 0; i < inst->refFrmChunks; i++)
  {
    tothw += inst->refFrmAlloc[i];
#ifdef TRACE_MEM_USAGE
    printf("\n      HW Memory:   %u", inst->refFrmAlloc[i]);
#endif
  }
  printf("\nTotal HW Memory:   %u", tothw);
  for (i = 0, totswhw = 0; i < inst->linMemChunks; i++)
  {
    totswhw += inst->linMemAlloc[i];
#ifdef TRACE_MEM_USAGE
    printf("\n      SWHW Memory: %u", inst->linMemAlloc[i]);
#endif
  }
  printf("\nTotal SWHW Memory: %u", totswhw);
  for (i = 0; i < memChunks; i++)
  {
#ifdef TRACE_MEM_USAGE
    printf("\n      SW Memory:   %u", memAlloc[i]);
#endif
  }
  printf("\nTotal SW Memory:   %u", memAllocTotal);

  printf("\nTotal Memory:      %u\n",
         tothw + totswhw + memAllocTotal);

  free_nodes(inst->workers.tail);
  free_nodes(inst->freelist.tail);
  free(inst);

  return EWL_OK;
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
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  assert(instance != NULL);
  assert(inst->refFrmChunks < MEM_CHUNKS);
  assert(inst->totalChunks < MEM_CHUNKS);

  inst->refFrmAlloc[inst->refFrmChunks++] = size;
  PTRACE(("EWLMallocRefFrm: %8d bytes (aligned %8ld)\n", size, NEXT_ALIGNED(size)));

  size = NEXT_ALIGNED(size);

  inst->chunks[inst->totalChunks] = (u32 *)malloc(size + LINMEM_ALIGN);
  if (inst->chunks[inst->totalChunks] == NULL)
    return EWL_ERROR;
  inst->alignedChunks[inst->totalChunks] = (u32 *)NEXT_ALIGNED(inst->chunks[inst->totalChunks]);
  PTRACE(("EWLMallocLinear: %p, aligned %p\n",
          (void *)(inst->chunks[inst->totalChunks]), (void *)(inst->alignedChunks[inst->totalChunks])));

  info->virtualAddress = inst->alignedChunks[inst->totalChunks++];
  info->busAddress = (ptr_t) info->virtualAddress;
  info->size = size;

  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        EWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - frame buffer memory information
------------------------------------------------------------------------------*/
void EWLFreeRefFrm(const void *instance, EWLLinearMem_t *info)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 i;
  assert(instance != NULL);
  assert(info->virtualAddress != NULL);

  for (i = 0; i < inst->totalChunks; i++)
  {
    if (inst->alignedChunks[i] == info->virtualAddress)
    {
      PTRACE(("EWLFreeRefFrm\t%p\n", (void *)info->virtualAddress));

      free(inst->chunks[i]);
      inst->chunks[i] = NULL;
      inst->alignedChunks[i] = NULL;
      info->virtualAddress = NULL;
      info->size = 0;
      return;
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : EWLLinearMem_t *info - place where the allocated
                        memory buffer parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocLinear(const void *instance, u32 size, u32 alignment,EWLLinearMem_t * info)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  assert(instance != NULL);
  assert(inst->linMemChunks < MEM_CHUNKS);
  assert(inst->totalChunks < MEM_CHUNKS);

  inst->linMemAlloc[inst->linMemChunks++] = size;
  PTRACE(("EWLMallocLinear: %8d bytes (aligned %8ld)\n", size, NEXT_ALIGNED(size)));

  size = NEXT_ALIGNED(size);

  inst->chunks[inst->totalChunks] = (u32 *)malloc(size + LINMEM_ALIGN);
  if (inst->chunks[inst->totalChunks] == NULL)
    return EWL_ERROR;
  inst->alignedChunks[inst->totalChunks] = (u32 *)NEXT_ALIGNED(inst->chunks[inst->totalChunks]);
  PTRACE(("EWLMallocLinear: %p, aligned %p\n",
          (void *)inst->chunks[inst->totalChunks], (void *)inst->alignedChunks[inst->totalChunks]));

  info->virtualAddress = inst->alignedChunks[inst->totalChunks++];
  info->busAddress = (ptr_t) info->virtualAddress;
  info->size = size;

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
void EWLFreeLinear(const void *instance, EWLLinearMem_t *info)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 i;
  assert(instance != NULL);
  assert(info->virtualAddress != NULL);

  for (i = 0; i < inst->totalChunks; i++)
  {
    if (inst->alignedChunks[i] == info->virtualAddress)
    {
      PTRACE(("EWLFreeLinear \t%p\n", (void *)info->virtualAddress));

      free(inst->chunks[i]);
      inst->chunks[i] = NULL;
      inst->alignedChunks[i] = NULL;
      info->virtualAddress = NULL;
      info->size = 0;
      return;
    }
  }
}

void EWLDCacheRangeFlush(const void *instance, EWLLinearMem_t *info)
{
  assert(instance != NULL);
  (void) instance;
  assert(info != NULL);
  (void) info;
}

void EWLDCacheRangeRefresh(const void *instance, EWLLinearMem_t *info)
{
  assert(instance != NULL);
  (void) instance;
  assert(info != NULL);
  (void) info;
}
#define HSWREG(n)       ((n)*4)

i32 EWLWaitHwRdy(const void *instance, u32 *slicesReady,u32 totalsliceNumber,u32* status_register)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  assert(inst != NULL);
  u32 core_id = FIRST_CORE(inst);
  SysCore *core = inst->core[core_id];

  CoreEncWaitHwRdy(inst->core[core_id]);

  if (slicesReady)
  {
    /* The model has finished encoding the whole frame but here
     * we can emulate the slice ready interrupt by outputting
     * different values of slicesReady. */
    *slicesReady = *slicesReady + 1;
    /* Set the frame ready status so that SW thinks a slice
     * is ready but frame encoding continues. This function
     * will be called again for every slice. */
    if (*slicesReady < CoreEncGetRegisterValue(core, HWIF_ENC_NUM_SLICES_READY))
    {
        CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 0);
        CoreEncSetRegisterValue(core, HWIF_ENC_SLICE_RDY_STATUS, 1);
    }
    else
    {
        CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 1);
    }
  }
  /*simulate stream segment interrupt when frame_rdy_irq is triggered*/
  if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_EN))
  {
    if (inst->frameRdy == -1)
      inst->frameRdy = (CoreEncGetRegister(core, HSWREG(1))&0x04) == 0x04;

    if (inst->frameRdy == 0)
      goto end;
  
    if (inst->streamLength == 0)
      inst->streamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
    u32 segmentSize = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SIZE);
    
    u32 rd = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RD_PTR);
    u32 wr = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR);
    u8 *streamBase = inst->streamBase + segmentSize * (wr%inst->segmentAmount);
    u8 *streamTmpBase = inst->streamTempBuffer + segmentSize * wr;
    
    if ((CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SW_SYNC_EN) == 0) 
        || ( (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SW_SYNC_EN) == 1) 
            && (wr >= rd && wr - rd < inst->segmentAmount)))
    {
      memcpy(streamBase, streamTmpBase, segmentSize);
      CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR, ++wr);
      
      if (inst->streamLength > segmentSize)
      {
        printf("---->trigger segment IRQ\n");
        CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 0);
        CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RDY_INT, 1);
      }
      else
      {
        printf("---->trigger frame ready IRQ\n");
        CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 1);
        CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RDY_INT, 0);
      }
      
      inst->streamLength -= segmentSize;    
    }
  }

end:  
  *status_register=EWLReadReg(inst, HSWREG(1));

  return EWL_OK;
}

i32 EWLReserveHw(const void *inst, u32 *core_info)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  i32 core_num = EWLGetCoreNum();
  u32 core_bits = 0;
  i32 ret, core_idx;
  u32 client_type = instance->clientType;
  EWLHwConfig_t hw_cfg;
  
  assert(inst != NULL);
  (void) inst;
  PTRACE(("EWLReserveHw\n"));
  EWLWorker *worker = (EWLWorker *)queue_get(&instance->freelist);
  if (worker == NULL)
    return EWL_ERROR;

  for(core_idx = 0; core_idx < core_num; core_idx++)
  {
    hw_cfg = EWLReadAsicConfig(core_idx);
    if(hw_cfg.hevcEnabled == 1 && client_type == EWL_CLIENT_TYPE_HEVC_ENC)
    {
      core_bits |= 1<<core_idx;
      continue;
    }
    else if(hw_cfg.h264Enabled == 1 && client_type == EWL_CLIENT_TYPE_H264_ENC)
    {
      core_bits |= 1<<core_idx;
      continue;
    }
    else if(hw_cfg.jpegEnabled == 1 && client_type == EWL_CLIENT_TYPE_JPEG_ENC)
    {
      core_bits |= 1<<core_idx;
      continue;
    }
    else if(hw_cfg.cuTreeSupport == 1 && client_type == EWL_CLIENT_TYPE_CUTREE)
    {
      core_bits |= 1<<core_idx;
      continue;
    }
    else if(hw_cfg.vsSupport == 1 && client_type == EWL_CLIENT_TYPE_VIDEOSTAB)
    {
      core_bits |= 1<<core_idx;
      continue;
    }
  }

  core_idx=0;
  while(1)
  {
    if ((core_bits>>core_idx)&0x1) {
      ret = CoreEncTryReserveHw(instance->core[core_idx]);
      if (ret==0) 
      {
        PTRACE(("Reserve: inst %p %p %p, core=%d\n", instance->winst.hevc, 
                instance->winst.jpeg, instance->winst.cutree, core_idx));
        break;
      }
      else
      {
        usleep(10);
      }
    }
    core_idx = (core_idx+1)%core_num;
  }
  worker->core_id = core_idx;
  
  //queue_remove(&instance->freelist, (struct node *)worker);
  queue_put(&instance->workers, (struct node *)worker);

  return EWL_OK;
}

void EWLReleaseHw(const void *inst)
{
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  assert(inst != NULL);
  (void) inst;
  PTRACE(("EWLReleaseHw\n"));
  EWLWorker *worker = (EWLWorker *)queue_get(&instance->workers);
  if(worker == NULL)
    return;
  //queue_remove(&instance->workers, (struct node *)worker);
  CoreEncReleaseHw(instance->core[worker->core_id]);
  queue_put(&instance->freelist, (struct node *)worker);
  PTRACE(("Release: inst %p %p %p, core=%d\n", instance->winst.hevc, 
        instance->winst.jpeg, instance->winst.cutree, worker->core_id));
}

u32 EWLGetCoreNum(void)
{
   return CoreEncGetCoreNum();
}
/* SW/SW shared memory */
void *EWLmalloc(u32 n)
{
  assert(memChunks < MEM_CHUNKS);
  memAlloc[memChunks++] = n;
  memAllocTotal += n;

  PTRACE(("Malloc: %d bytes, total of %d bytes\n", n, memAllocTotal));

  return malloc((size_t) n);
}

void EWLfree(void *p)
{

  PTRACE(("Freeing memory\n"));

  free(p);
}

void *EWLcalloc(u32 n, u32 s)
{
  assert(memChunks < MEM_CHUNKS);
  memAlloc[memChunks++] = n * s;
  memAllocTotal += n * s;

  PTRACE(("Calloc: %d bytes, total of %d bytes\n", n * s, memAllocTotal));

  return calloc((size_t) n, (size_t) s);
}

void *EWLmemcpy(void *d, const void *s, u32 n)
{
  return memcpy(d, s, (size_t) n);
}

void *EWLmemset(void *d, i32 c, u32 n)
{
  return memset(d, (int) c, (size_t) n);
}

int EWLmemcmp(const void *s1, const void *s2, u32 n)
{
  return memcmp(s1, s2, (size_t) n);
}

#if 0
/*------------------------------------------------------------------------------

    Swap endianess of buffer

    wordCnt is the amount of 32-bit words that are swapped

------------------------------------------------------------------------------*/
void EWL_TEST_SwapEndian(u32 *buf, u32 wordCnt)
{
  u32 i = 0;

  while (wordCnt != 0)
  {
    u32 val = buf[i];
    u32 tmp = 0;

    tmp |= (val & 0xFF) << 24;
    tmp |= (val & 0xFF00) << 8;
    tmp |= (val & 0xFF0000) >> 8;
    tmp |= (val & 0xFF000000) >> 24;
    buf[i] = tmp;
    i++;
    wordCnt--;
  }
}
#endif

/*------------------------------------------------------------------------------
    Function name   : EWLTraceProfile
    Description     : print the PSNR and SSIM data, only valid for c-model.
    Return type     : void
    Argument        : none
------------------------------------------------------------------------------*/
void EWLTraceProfile(const void *instance)
{
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  float bit_rate_avg;
  float Y_PSNR_avg;
  float U_PSNR_avg;
  float V_PSNR_avg;
  u32 core_id = LAST_CORE(inst);
  SysCore *core = inst->core[core_id];

  i32 type;
  i32 qp;

  // MUST be the same as the struct in "pictur".
  struct {
    i32 bitnum;
    float psnr_y, psnr_u, psnr_v;
    double ssim;
    double ssim_y, ssim_u, ssim_v;
  } prof;
  
  void *prof_data;
  i32 poc;

  prof_data = (void*)CoreEncGetAddrRegisterValue(core, HWIF_ENC_COMPRESSEDCOEFF_BASE);
  qp = CoreEncGetRegisterValue(core, HWIF_ENC_PIC_QP);
  poc = CoreEncGetRegisterValue(core, HWIF_ENC_POC);

  memcpy(&prof, prof_data, sizeof(prof));

  if (inst->prof.QP_Max < qp)
    inst->prof.QP_Max = qp;
  if (inst->prof.QP_Min > qp)
    inst->prof.QP_Min = qp;
  if (inst->prof.Y_PSNR_Max < prof.psnr_y)
    inst->prof.Y_PSNR_Max = prof.psnr_y;
  if (inst->prof.Y_PSNR_Min > prof.psnr_y)
    inst->prof.Y_PSNR_Min = prof.psnr_y;
  if (inst->prof.U_PSNR_Max < prof.psnr_y)
    inst->prof.U_PSNR_Max = prof.psnr_y;
  if (inst->prof.U_PSNR_Min > prof.psnr_y)
    inst->prof.U_PSNR_Min = prof.psnr_y;
  if (inst->prof.V_PSNR_Max < prof.psnr_y)
    inst->prof.V_PSNR_Max = prof.psnr_y;
  if (inst->prof.V_PSNR_Min > prof.psnr_y)
    inst->prof.V_PSNR_Min = prof.psnr_y;
  inst->prof.frame_number++;
  inst->prof.total_bits += prof.bitnum;
  bit_rate_avg = (inst->prof.total_bits /inst->prof.frame_number) * 30;
  inst->prof.total_Y_PSNR += prof.psnr_y;
  Y_PSNR_avg = inst->prof.total_Y_PSNR / inst->prof.frame_number;
  inst->prof.total_U_PSNR += prof.psnr_u;
  U_PSNR_avg = inst->prof.total_U_PSNR / inst->prof.frame_number;
  inst->prof.total_V_PSNR += prof.psnr_v;
  V_PSNR_avg = inst->prof.total_V_PSNR / inst->prof.frame_number;

  inst->prof.total_ssim += prof.ssim;
  inst->prof.total_ssim_y += prof.ssim_y;
  inst->prof.total_ssim_u += prof.ssim_u;
  inst->prof.total_ssim_v += prof.ssim_v;

  printf("    CModel::POC %3d QP %3d %9d bits [Y %.4f dB  U %.4f dB  V %.4f dB] [SSIM %.4f average_SSIM %.4f] [SSIM Y %.4f U %.4f V %.4f average_SSIM Y %.4f U %.4f V %.4f]\n",
         poc, qp, prof.bitnum, prof.psnr_y, prof.psnr_u, prof.psnr_v, prof.ssim,inst->prof.total_ssim/inst->prof.frame_number, prof.ssim_y, prof.ssim_u, prof.ssim_v, 
         inst->prof.total_ssim_y/inst->prof.frame_number, inst->prof.total_ssim_u/inst->prof.frame_number, inst->prof.total_ssim_v/inst->prof.frame_number);
  printf("    CModel::POC %3d QPMin/QPMax %d/%d Y_PSNR_Min/Max %.4f/%.4f dB U_PSNR_Min/Max %.4f/%.4f dB V_PSNR_Min/Max %.4f/%.4f \n", 
          poc, inst->prof.QP_Min, inst->prof.QP_Max, inst->prof.Y_PSNR_Min, inst->prof.Y_PSNR_Max, inst->prof.U_PSNR_Min, inst->prof.U_PSNR_Max, inst->prof.V_PSNR_Min, inst->prof.V_PSNR_Max);
  printf("    CModel::POC %3d frame %d Y_PSNR_avg %.4f dB  U_PSNR_avg %.4f dB V_PSNR_avg %.4f dB \n", 
          poc, inst->prof.frame_number-1, Y_PSNR_avg, U_PSNR_avg, V_PSNR_avg);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetLineBufSram 
    Description        : Get the base address of on-chip sram used for input MB line buffer.
    
    Return type     : i32 - 0 for success or a negative error code  
    
    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the sram parameters are returned
------------------------------------------------------------------------------*/
i32 EWLGetLineBufSram (const void *instance, EWLLinearMem_t *info)
{
    ewlSysInstance *inst = (ewlSysInstance *)instance;
    assert(inst != NULL);
    assert(info != NULL);

    info->virtualAddress = NULL;
    info->busAddress = 0;
    info->size = 0;

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
    ewlSysInstance *inst = (ewlSysInstance *)instance;
    assert(inst != NULL);
    assert(info != NULL);

    info->virtualAddress = NULL;
    info->busAddress = 0;
    info->size = 0;

    return EWL_OK;
}

