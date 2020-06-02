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
--  Abstract : Cache Wrapper Layer, read parts
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "cwl.h"

#include "cwl_common.h"
#include "hantro_cache.h"
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
#include <time.h>

#ifdef ASIC_ONL_SIM
#include "vpu_dwl_dpi.h"
extern long int *HW_TB_DPB_BASE;
extern u64  HW_TB_DPB_LUMA_BASE;
extern u64  HW_TB_DPB_CHROMA_BASE;
extern u64  HW_TB_TILE_BASE;
extern u64  HW_TB_SCALELIST_BASE;
extern u64  HW_TB_STRM_BASE;
extern u64  HW_TB_QTABLE_BASE;
extern u64  HW_TB_FILT_VER_BASE;
extern u64  HW_TB_SAO_VER_BASE;
extern u64  HW_TB_BSD_CTRL_BASE;
extern u64  HW_TB_ALFTABLE_BASE;
extern u64  HW_TB_PP_LUMA_BASE;
#include "tb_cfg.h"
extern struct TBCfg tb_cfg;
#endif

//volatile u32 asic_status = 0;
FILE *fp_rd = NULL;
FILE *fp_wr = NULL;

//static i32 regSize[2]={-1,-1};           
//static long regBase[2]={-1,-1};
extern u64 exception_addr[64][2];
#define  H264  0
#define  JPEG  3
#define  HEVC 12
#define  H264_HIGH10 15
const regField_s CacheRegisterDesc[] =
{
#include "registertable.h"
};

char *addr_type_print[]=
{
 "BaseInLum",
 "BaseInCb",
 "BaseRefLum1",
 "BaseRefChr1",
 "BaseRefLum0",
 "BaseRefChr0",
 "BaseRef4nLum1",
 "BaseRef4nLum0",
 "BottomBaseRefLum1",
 "BottomBaseRefChr1",
 "BaseRefLum2",
 "BottomBaseRefLum2",
 "BaseRefChr2",
 "BottomBaseRefChr2",
 "BaseRefLum3",
 "BottomBaseRefLum3",
 "BaseRefChr3",
 "BottomBaseRefChr3",
 "BaseRef4nLum2",
 "BaseRef4nLum3",
 "BottomBaseRef4nLum1",
 "BottomBaseRef4nLum2",
 "BottomBaseRef4nLum3",
 "BaseStream"
};

char *addr_end_print[]=
{
 "endBaseInLum",
 "endBaseInCb",
 "endBaseRefLum1",
 "endBaseRefChr1",
 "endBaseRefLum0",
 "endBaseRefChr0",
 "endBaseRef4nLum1",
 "endBaseRef4nLum0",
 "endBottomBaseRefLum1",
 "endBottomBaseRefChr1",
 "endBaseRefLum2",
 "endBottomBaseRefLum2",
 "endBaseRefChr2",
 "endBottomBaseRefChr2",
 "endBaseRefLum3",
 "endBottomBaseRefLum3",
 "endBaseRefChr3",
 "endBottomBaseRefChr3",
 "endBaseRef4nLum2",
 "endBaseRef4nLum3",
 "endBottomBaseRef4nLum1",
 "endBottomBaseRef4nLum2",
 "endBottomBaseRef4nLum3",
 "endBaseStream"
};


void CacheRegisterDump(cache_cwl_t *cwl,FILE *file, char *filename, i32 picNum,cache_dir dir);

int CWLMapAsicRegisters(cache_cwl_t * cwl,cache_dir dir)
{
    unsigned long base;
    unsigned int size;
    size_t map_size;
    const int page_size = getpagesize();
    const int page_alignment = page_size - 1;
    const char *io = MAP_FAILED;
#if 0
    if (regSize[dir]==-1||regBase[dir]==-1)
    {
     base = size = cwl->reg[dir].core_id;
 
     ioctl(cwl->fd_cache, CACHE_IOCGHWOFFSET, &base);
     //ioctl(cwl->fd_cache, CACHE_IOCGHWIOSIZE, &size);
     regSize[dir] = L2_SWREG_TOTAL;
     size = regSize[dir];
     regBase[dir] = base;
    }
    else
    {
     base = regBase[dir];
     size = regSize[dir];
    }
#else
    base = size = cwl->reg[dir].core_id;
 
    ioctl(cwl->fd_cache, CACHE_IOCGHWOFFSET, &base);
    ioctl(cwl->fd_cache, CACHE_IOCGHWIOSIZE, &size);
    map_size = size + (base & page_alignment);
#endif
    /* map hw registers to user space */
    io =
        (char *) mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                     cwl->fd_mem, (base & ~page_alignment));
    if(io == MAP_FAILED)
    {
     PTRACE("CWLInit: Failed to mmap regs\n");
     return -1;
    } else {
      io += (base & page_alignment);
    }
    cwl->reg[dir].pComRegBase = (u32 *)io;

    cwl->reg[dir].regSize = size;
    cwl->reg[dir].regBase = base;
    cwl->reg[dir].pRegBase = (u32 *)io;

    return 0;
}

#if 1
/*******************************************************************************
 Function name   : CWLReadAsicID
 Description     : Read ASIC ID register, static implementation
 Return type     : u32 ID
 Argument        : void
*******************************************************************************/
u32 CWLReadAsicID()
{
    u32 id = 0;
    int fd_enc = -1;

    static CWLHwConfig_t cfg_info;

    if (cfg_info.is_read != 0) {
      id = cfg_info.id;
      goto end;
    }
    cfg_info.is_read = 1;
    fd_enc = open(CACHE_MODULE_PATH, O_RDWR);
    if(fd_enc == -1)
    {
        PTRACE("CWLReadAsicID: failed to open: %s\n", CACHE_MODULE_PATH);
        goto end;
    }
    /* ask module for hw id */
    if(ioctl(fd_enc, CACHE_IOCGHWID, &id) == -1)
    {
        PTRACE("ioctl failed\n");
        goto end;
    }
    cfg_info.id = id; 

  end:
    if(fd_enc != -1)
        close(fd_enc);

    PTRACE("CWLReadAsicID: 0x%08x at 0x%08lx\n", id, base);

    return id;
}

#endif
/*******************************************************************************
 Function name   : CWLInit
 Description     : Allocate resources and setup the wrapper module
 Return type     : cwl_ret 
 Argument        : void
*******************************************************************************/
void *CWLInit(client_type client)
{
    cache_cwl_t *cwl = NULL;
    u32 i=0;
    PTRACE("CWLInit: Start\n");

    /* Allocate instance */
    if((cwl = (cache_cwl_t *) CWLmalloc(sizeof(cache_cwl_t))) == NULL)
    {
        PTRACE("CWLInit: failed to alloc cache_cwl_t struct\n");
        return NULL;
    }

    memset(cwl,0,sizeof(cache_cwl_t));

    cwl->clientType = client;
    cwl->fd_mem = cwl->fd_cache = -1;
    for(i=0;i<CACHE_BI;i++)
    {
     cwl->reg[i].core_id = -1;
     cwl->valid_ch_num[i] = 0;
     cwl->used_ch_num[i] = 0;
    }
	
#ifndef CACHE_SHAPER_SIM
    /* New instance allocated */
    cwl->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if(cwl->fd_mem == -1)
    {
        PTRACE("CWLInit: failed to open: %s\n", "/dev/mem");
        goto err;
    }

    cwl->fd_cache = open(CACHE_MODULE_PATH, O_RDWR);
    if(cwl->fd_cache == -1)
    {
        PTRACE("CWLInit: failed to open: %s\n", CACHE_MODULE_PATH);
        goto err;
    }
#endif
    PTRACE("CWLInit: Return %p\n", (void*) cwl);
    return cwl;

  err:
    CWLRelease(cwl);
    PTRACE("CWLInit: Return NULL\n");
    return NULL;
}

/*******************************************************************************
 Function name   : CWLRelease
 Description     : Release the wrapper module by freeing all the resources
 Return type     : cwl_ret 
 Argument        : void
*******************************************************************************/
i32 CWLRelease(const void *inst)
{
    cache_cwl_t *cwl = (cache_cwl_t *) inst;
    const int page_size = getpagesize();
    const int page_alignment = page_size - 1;
    u32 i;

    assert(cwl != NULL);

    if(cwl == NULL)
        return CWL_OK;

#ifndef CACHE_SHAPER_SIM
    /* Release the register mapping info of each core */ 
    for (i = 0; i< CACHE_BI; i++)
    {
      if(cwl->reg[i].pRegBase != NULL)
      {
        munmap((void *) ((long)cwl->reg[i].pComRegBase&(~page_alignment)), cwl->reg[i].regSize+((long)cwl->reg[i].pComRegBase&page_alignment));
        cwl->reg[i].pRegBase = NULL;
      }

      if(cwl->cfg[i]!= NULL)
      {
        free(cwl->cfg[i]);
        cwl->cfg[i]= NULL;
      }
    }
	
    if(cwl->fd_mem != -1)
        close(cwl->fd_mem);
    if(cwl->fd_cache != -1)
        close(cwl->fd_cache);
#endif
    CWLfree(cwl);

    PTRACE("CWLRelease: instance freed\n");
  
    return CWL_OK;

}

/*******************************************************************************
 Function name   : CWLWriteReg
 Description     : Set the content of a hardware register
 Return type     : void 
 Argument        : u32 offset
 Argument        : u32 val
*******************************************************************************/
void CWLWriteReg(const void *inst, u32 offset, u32 val)
{
    regMapping *reg = (regMapping *) inst;

    assert(reg != NULL && offset < reg->regSize);

    offset = offset / 4;
    *(reg->pRegBase + offset) = val;

    PTRACE("CWLWriteReg 0x%02x with value %08x\n", offset * 4, val);
}

/*------------------------------------------------------------------------------
    Function name   : CWLEnableCache
    Description     : 
    Return type     : void 
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void CWLEnableCache(const void *inst,cache_dir dir)
{
    cache_cwl_t *cwl = (cache_cwl_t *) inst;
    static u32 picNum_cache = 0;
    static u32 picNum_shaper = 0;
    u32 i;
    CWLChannelConf_t *cfg_temp = NULL;

    if(cwl == NULL)
      return;
    
    cfg_temp = cwl->cfg[dir];
    if (dir == CACHE_RD)
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ENABLE,1,1);
    else
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_ENABLE,1,1);
 
    PTRACE("CWLEnableCache with value %08x\n",0x01);
#ifdef CACHE_SHAPER_SIM  
#ifdef ASIC_ONL_SIM  
//next line will remove after updata cmdbuf dpi function
if (tb_cfg.cmdbuf_params.cmd_en) {printf("debug:l2:tb_cfg.cmdbuf_params.cmd_en=1, should not run CacheRegisterDump\n");}
#endif

    if (dir == CACHE_RD) {
      if (cwl->clientType == ENCODER_VC8000E)
        CacheRegisterDump(cwl,NULL,"cache_swreg_dump",picNum_cache++,dir);
      else {
        if (cfg_temp->cache_enable && (cwl->cfg[dir]->hw_dec_pic_count >= cwl->cfg[dir]->trace_start_pic)) {
          CacheRegisterDump(cwl,NULL,"cache_swreg_dump",picNum_cache++,dir);
        }
      }
    } else {
      cfg_temp = cwl->cfg[CACHE_WR] + cwl->first_channel_num;
      if (cwl->clientType == DECODER_G2_0 || cwl->clientType == DECODER_G2_1) {
        if (cfg_temp->shaper_enable && (cfg_temp->hw_dec_pic_count >= cfg_temp->trace_start_pic)) {
            CacheRegisterDump(cwl,NULL,"shaper_swreg_dump",picNum_shaper,dir);
          if (cfg_temp->tile_id == (cfg_temp->tile_num - 1))
            picNum_shaper++;
        }
      } else {
        if (cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
          if (cfg_temp->shaper_enable && (cfg_temp->hw_dec_pic_count >= cfg_temp->trace_start_pic)) {
            CacheRegisterDump(cwl,NULL,"shaper_swreg_dump",picNum_shaper++,dir);
          }
        } else
          CacheRegisterDump(cwl,NULL,"shaper_swreg_dump",picNum_shaper++,dir);
      }
    }
#endif

    return;
}

/*------------------------------------------------------------------------------
    Function name   : CWLDisableCache
    Description     : 
    Return type     : void 
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void CWLDisableCache(const void *inst,cache_dir dir)
{
    cache_cwl_t *cwl = (cache_cwl_t *) inst;

    if(cwl == NULL)
      return;
    
    if (dir == CACHE_RD)
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ENABLE,0,1);
    else
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_ENABLE,0,1);

    PTRACE("CWLDisableCache with value %08x\n", 0x00);
    return;
}

/*******************************************************************************
 Function name   : CWLReadReg
 Description     : Retrive the content of a hadware register
 Return type     : u32 
 Argument        : u32 offset
*******************************************************************************/
u32 CWLReadReg(const void *inst, u32 offset)
{
    u32 val;
    regMapping *reg = (regMapping *) inst;

    assert(offset < reg->regSize);
    
    offset = offset / 4;
    val = *(reg->pRegBase + offset);

    PTRACE("CWLReadReg 0x%02x --> %08x\n", offset * 4, val);

    return val;
}

/*******************************************************************************
 Function name   : CWLReserveHw
 Description     : Reserve HW resource for currently running codec
*******************************************************************************/
i32 CWLReserveHw(const void *inst,client_type client,cache_dir dir)
{
    cache_cwl_t *cwl = (cache_cwl_t *) inst;
    u32 c = 0;
    u32 i = 0,valid_num =0;
    i32 ret;

    if (client == DECODER_G1_0 || client == DECODER_G2_0)
      client = DECODER_VC8000D_0;
    if (client == DECODER_G1_1 || client == DECODER_G2_1)
      client = DECODER_VC8000D_1;

    u32 temp = (u32)((client<<1)|(dir&0x01));//client_id(2bits)_dir(1bit)
    u32 filed = 0,reg_amount = 0;
    
#ifndef CACHE_SHAPER_SIM 
    /* Check invalid parameters */
    if(cwl == NULL)
      return CWL_ERROR;
    
    PTRACE("CWLReserveHw: PID %d trying to reserve ...\n", getpid());

    ret = ioctl(cwl->fd_cache, CACHE_IOCH_HW_RESERVE, &temp);
    
    if (ret < 0)
    {
     PTRACE("CWLReserveHw failed\n");
     return CWL_ERROR;
    }
    else
    {
     PTRACE("CWLReserveHw successed\n");
    }
    cwl->reg[dir].core_id = temp;
     
    /* map hw registers to user space.*/
    if(CWLMapAsicRegisters((void *)cwl,dir) != 0)
    {
     PTRACE("CWLReserveHw map register failed\n");
     return CWL_ERROR;
    }

    CWLDisableCache((void *)cwl,dir);
    
    cwl->valid_ch_num[dir] = 0;
    cwl->used_ch_num[dir] = 0;
    
    filed = (dir==CACHE_RD ? HWIF_CACHE_NUM_CHANNEL : HWIF_CACHE_WR_CHN_NUM);
    cwl->configured_channel_num[dir] = CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,filed,1);
    if (dir == CACHE_RD)
      cwl->channel_num[dir] = cwl->configured_channel_num[dir];
    else
      cwl->channel_num[dir] = 16;
    cwl->cfg[dir] = (CWLChannelConf_t *)malloc(cwl->channel_num[dir]*sizeof(CWLChannelConf_t));
    if (dir == CACHE_RD)
      cwl->exception_addr_max = CWLAsicGetRegisterValue(&cwl->reg,cwl->reg[dir].regMirror,HWIF_CACHE_NUM_EXCPT,1);
#endif

    memset(&cwl->reg[dir].regMirror,0,sizeof(CACHE_SWREG_MAX*sizeof(u32)));
#ifdef CACHE_SHAPER_SIM
    cwl->valid_ch_num[dir] = 0; 
    cwl->used_ch_num[dir] = 0; 
    cwl->configured_channel_num[dir] = 16;
    cwl->channel_num[dir] = 16;   
    cwl->cfg[dir] = (CWLChannelConf_t *)malloc(cwl->channel_num[dir]*sizeof(CWLChannelConf_t));
    cwl->exception_addr_max = 64;
    cwl->reg[dir].core_id = 0;
#endif
    
    PTRACE("CWLReserveHw: ENC HW locked by PID %d\n", getpid());

    return CWL_OK;
}

/*******************************************************************************
 Function name   : CWLReleaseHw
 Description     : Release HW resource when codec is done
*******************************************************************************/
void CWLReleaseHw(const void *inst,cache_dir dir)
{
    u32 val,i;
    cache_cwl_t *cwl = (cache_cwl_t *) inst;
    void *reg;
    u32 core_id = 0;
    const int page_size = getpagesize();
    const int page_alignment = page_size - 1;
	
    assert(cwl != NULL);
    core_id = cwl->reg[dir].core_id;
#ifndef CACHE_SHAPER_SIM
    reg = (void *)&cwl->reg[dir];
    val = CWLReadReg(reg, 0x04);
    CWLWriteReg(reg, 0x04, val & (~0x00)); /* clear IRQ*/
    CWLWriteReg(reg, 0x04, 0x00); /* reset ASIC */
    munmap((void *) ((long)cwl->reg[dir].pComRegBase&(~page_alignment)), cwl->reg[dir].regSize+((long)cwl->reg[dir].pComRegBase&page_alignment));
    
    cwl->reg[dir].pRegBase = NULL;

    PTRACE("CWLReleaseHw: PID %d trying to release ...\n", getpid());

    ioctl(cwl->fd_cache, CACHE_IOCH_HW_RELEASE, &core_id);
#endif
    free(cwl->cfg[dir]);
    cwl->cfg[dir] = NULL;
    cwl->reg[dir].core_id = -1;

    PTRACE("CWLReleaseHw: HW released by PID %d\n", getpid());
}

i32 CWLWaitChannelAborted(const void *inst,u32* status_register,cache_dir dir)
{
    cache_cwl_t *cwl = (cache_cwl_t *) inst;
    u32 ret = 0;
    u32 axi_master = 0;
    int loop = 10000;         /* How many times to poll before timeout */
    const unsigned int usec = 1000; /* 1 ms polling interval */
#ifdef CWL_PRINT
    u32 i;
#endif 
    PTRACE("CWLWaitChannelAborted: Start\n");
		
    /* Check invalid parameters */
    if(cwl == NULL)
    {
      assert(0);
      return CWL_HW_WAIT_ERROR;
    }

#ifndef PCI_BUS
    ret = cwl->reg[dir].core_id;
    if (ioctl(cwl->fd_cache, CACHE_IOCG_ABORT_WAIT, &ret))
    {
      PTRACE("ioctl CACHE_IOCG_CORE_WAIT failed\n");
      ret = 0;
    }
#else
#ifdef CWL_PRINT
    for (i = 0; i < 5; i++) {   
      ret = CWLReadReg(&cwl->reg[CACHE_WR], 4*i);
      printf("R swreg%d/%08x\n",8200+i,ret);
    }
#endif
    if (dir == CACHE_WR)
      axi_master = ((CWLReadReg(&cwl->reg[dir], 0x08)) >> 17) & 0x01;
    do
    {
      ret = 0;
      if (dir == CACHE_RD)
      {
        ret = CWLReadReg(&cwl->reg[dir], 0x04);
        if ((ret&0x28)!=0)//((ret&0x20)==0x20)//received abort irq
        {
	      CWLWriteReg(&cwl->reg[dir],0x04,ret); //clear all irq 
	      break;
        }
      }
      else
      {
        ret = CWLReadReg(&cwl->reg[dir], 0x0C);
        if (ret!=0)//((ret&0x02)==0x02)//received abort irq
        {
          CWLWriteReg(&cwl->reg[dir],0x0C,ret); //clear all irq 
	      break;
        }
      }
      usleep(usec);
    }while(loop--);
    if (loop == -1) {
      if (axi_master)
        ret = CACHE_ERROR_UNRE;
      else
        ret = CACHE_ERROR_RE;
      printf("CWLWaitChannelAborted: timeout!\n");
    }
#endif    
    *status_register = ret;
    PTRACE("CWLWaitChannelAborted: OK!\n");

    return CWL_OK;
}

/*------------------------------------------------------------------------------

    CWLAsicSetRegisterValue

    Set a value into a defined register field

------------------------------------------------------------------------------*/
void CWLAsicSetRegisterValue(void *reg,u32 *regMirror, regName name, u32 value,u32 write_asic)
{
  const regField_s *field;
  u32 regVal;

  field = &CacheRegisterDesc[name];

  PTRACE("CWLAsicSetRegisterValue 0x%2x  0x%08x  Value: %10d  %s\n",
         field->base, field->mask, value, field->description);

  /* Check that value fits in field */
  ASSERT(field->name == name);
  ASSERT(((field->mask >> field->lsb) << field->lsb) == field->mask);
  ASSERT((field->mask >> field->lsb) >= value);
  ASSERT(field->base < CACHE_SWREG_MAX * 4);

  /* Clear previous value of field in register */
  regVal = regMirror[field->base / 4] & ~(field->mask);

  /* Put new value of field in register */
  regMirror[field->base / 4] = regVal | ((value << field->lsb) & field->mask);
#ifndef CACHE_SHAPER_SIM
  if (write_asic)
    CWLWriteReg(reg,field->base,regMirror[field->base / 4]);
#endif
}

/*------------------------------------------------------------------------------

    CWLAsicGetRegisterValue

    Get an unsigned value from the ASIC registers

------------------------------------------------------------------------------*/
u32 CWLAsicGetRegisterValue(const void *reg, u32 *regMirror, regName name,u32 read_asic)
{
  const regField_s *field;
  u32 value;

  field = &CacheRegisterDesc[name];

  ASSERT(field->base < CACHE_SWREG_MAX * 4);

#ifndef CACHE_SHAPER_SIM
  if (read_asic)
    value = regMirror[field->base / 4] = CWLReadReg(reg, field->base);
  else
#endif
    value = regMirror[field->base / 4];
  
  value = (value & field->mask) >> field->lsb;

  return value;
}

/*generate trace file of cache for HW simulation*/
void CacheRegisterDump(cache_cwl_t *cwl,FILE *file, char *filename, i32 picNum,cache_dir dir)
{
  const regField_s *field;
  u32 i, j;
  i32 value;
  FILE *fp, *fp1;
  u32 base_id;
  u32 index;
  CWLChannelConf_t *cfg_temp = NULL;
  cfg_temp = cwl->cfg[dir];
  if (dir == CACHE_WR) {
    cfg_temp = cwl->cfg[CACHE_WR] + cwl->first_channel_num;
    index = cwl->first_channel_num;
  }
  if (cwl->clientType == ENCODER_VC8000E) {
    if (dir == CACHE_RD)
    {
     if (fp_rd == NULL)
     {
      if (file)
        fp_rd = file;
      else if (filename)
        fp_rd = fopen(filename, "w");
     }
     fp = fp_rd;
    }

    if (dir == CACHE_WR)
    {
     if (fp_wr == NULL)
     {
      if (file)
        fp_wr = file;
      else if (filename)
        fp_wr = fopen(filename, "w");
     }
     fp = fp_wr;
    }
  }  else {
    if (dir == CACHE_RD)
      fp = cfg_temp->file_fid;
    if (dir == CACHE_WR)
      fp = cfg_temp->file_fid;
  }
  if (cwl->clientType == DECODER_G2_0 || cwl->clientType == DECODER_G2_1) {
    if (fp_wr == NULL)
    {
      if (file)
        fp_wr = file;
      else if (filename)
        fp_wr = fopen(filename, "w");
    }
    fp1 = fp_wr;
  }
  if (!fp)
    return;
  if (!fp1 && (cwl->clientType == DECODER_G2_0 || cwl->clientType == DECODER_G2_1))
    return;

  if (cwl->clientType != ENCODER_VC8000E) {
    if (dir == CACHE_RD) {
      if (cfg_temp->cache_enable) {
        fprintf(fp, "#######################################################\n");
        fprintf(fp, "#picture=%d,stream_buffer=%d,output_slice=%d\n",
                cfg_temp->hw_dec_pic_count, cfg_temp->stream_buffer_id, 0);
        fprintf(fp, "#######################################################\n");
      }
    } else {
      if (!cfg_temp->cache_enable && cfg_temp->shaper_enable && cfg_temp->first_tile) {
        fprintf(fp, "#######################################################\n");
        fprintf(fp, "#picture=%d,stream_buffer=%d,output_slice=%d\n",
                cfg_temp->hw_dec_pic_count, cfg_temp->stream_buffer_id, 0);
        fprintf(fp, "#######################################################\n");
      }
    }
  }
  if (dir == CACHE_RD)
  {
    printf("cache dump register picNum=%d\n",picNum);
#if 0
    fprintf(fp, "pic=%d\n", picNum);
#endif
    if (!cfg_temp->shaper_enable && !(cwl->clientType == ENCODER_VC8000E)) {
      for (i =0; i < 8; i++) {
        fprintf(fp, "W swreg%d/%08x\n",i+8192, 0);
        #ifdef ASIC_ONL_SIM
        dpi_l2_apb_op(i*4,0,1);
        #endif
      }
    }
    if (cwl->clientType == ENCODER_VC8000E) {
      fprintf(fp, "pic=%d\n", picNum);
      fprintf(fp, "W 00008200/12345678\n");
    } else {
      if (!cfg_temp->shaper_enable)
        fprintf(fp, "W swreg8320/12345678\n");
      else{
        fprintf(fp, "W swreg8320/12345678\n");
      }
      #ifdef ASIC_ONL_SIM
      dpi_l2_apb_op(128*4,0x12345678,1);
      #endif
    }
    if (!cfg_temp->shaper_enable)
      base_id = 8320;
    else
      base_id = 8320;
    if (cwl->clientType == ENCODER_VC8000E) 
    {
      for (i = 1; i < CACHE_RD_SWREG_AMOUNT; i++)
      {
        if (i==6||i==10||i==14||i==18||i==22||i==26||i==30||i==34)
        {
         fprintf(fp, "W %08x/%08x+%s\n",i*4+0x00008200,(cwl->reg[CACHE_RD].regMirror[i])&0x0f,addr_type_print[((cwl->reg[CACHE_RD].regMirror[i])&0xff0)>>4]);
         i++;
         fprintf(fp, "W %08x/%08x+%s\n",i*4+0x00008200,(cwl->reg[CACHE_RD].regMirror[i]),addr_end_print[((cwl->reg[CACHE_RD].regMirror[i-1])&0xff0)>>4]);
        }
        else
         fprintf(fp, "W %08x/%08x\n",i*4+0x00008200,(u32)cwl->reg[CACHE_RD].regMirror[i]);
      }
    } else {
      for (i = 1; i < CACHE_RD_SWREG_AMOUNT; i++)
      {
        if (i == 1) {
          fprintf(fp, "W swreg%d/%08x\n",i + base_id,(u32)((cwl->reg[CACHE_RD].regMirror[i] | 0x80) & ~0x1));
          #ifdef ASIC_ONL_SIM
          dpi_l2_apb_op((128+i)*4,(u32)((cwl->reg[CACHE_RD].regMirror[i] | 0x80) & ~0x1),1);
          #endif
        } else if ( i == 2) {
          fprintf(fp, "W swreg%d/%08x\n",i + base_id,(u32)cwl->reg[CACHE_RD].regMirror[i]);
          #ifdef ASIC_ONL_SIM
	  if (((tb_cfg.axi_params.rd_axi_id == 1) | (tb_cfg.axi_params.axi_rd_id_e == 1)) & tb_cfg.cache_enable) {
            dpi_l2_apb_op((128+i)*4,((u32)cwl->reg[CACHE_RD].regMirror[i] + 8),1);
          } else {
            dpi_l2_apb_op((128+i)*4,(u32)cwl->reg[CACHE_RD].regMirror[i],1);
          }
          #endif
        } else if (i == 3) {
          if(cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
            for(j = 0; j < cwl->exception_list_amount; j++) {
     	      if (cfg_temp->dec_mode == H264) {
                if (j == 0) {
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_INDATA",cfg_temp->base_offset);
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_INDATA",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->base_offset);
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((128+i)*4,HW_TB_STRM_BASE+cfg_temp->base_offset,1);
                  dpi_l2_apb_op((128+i)*4,HW_TB_STRM_BASE+exception_addr[j][1] - exception_addr[j][0]+cfg_temp->base_offset,1);
                  #endif
                } else {
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-1] - 32);
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-1] - 32);
                }
	      }
              if (cfg_temp->dec_mode == H264_HIGH10) {
                fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j] - 32);
                fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j] - 32);
              }
              if (cfg_temp->dec_mode == JPEG) {
	        fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_PJPEG_COFF",cfg_temp->base_offset);
                fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_PJPEG_COFF",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->base_offset);
              }
            }
          } else {
            for(j = 0; j < cwl->exception_list_amount; j++) {
              if (cfg_temp->num_tile_cols < 2) {
                fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j] - 32);
                fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j]-32);
              } else {
                if (j == 0) {
                  fprintf(fp, "W swreg%d/%s+%X\n",i+base_id,"BASE_DEC_FILT_VER",0);
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_FILT_VER",exception_addr[j][1] - exception_addr[j][0]);
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((128+i)*4,HW_TB_FILT_VER_BASE,1);
                  dpi_l2_apb_op((128+i)*4,HW_TB_FILT_VER_BASE+exception_addr[j][1] - exception_addr[j][0],1);
                  #endif
                } else if (j == 1) {
                  fprintf(fp, "W swreg%d/%s+%X\n",i+base_id,"BASE_DEC_BSD_CTRL",0);
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_BSD_CTRL",exception_addr[j][1] - exception_addr[j][0]);
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((128+i)*4,HW_TB_BSD_CTRL_BASE,1);
                  dpi_l2_apb_op((128+i)*4,HW_TB_BSD_CTRL_BASE+exception_addr[j][1] - exception_addr[j][0],1);
                  #endif
                } else if (j == 2) {
                  if (cfg_temp->dec_mode == HEVC) {
                    fprintf(fp, "W swreg%d/%s+%X\n",i+base_id,"BASE_DEC_SAO_VER",0);
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_SAO_VER",exception_addr[j][1] - exception_addr[j][0]);
                    #ifdef ASIC_ONL_SIM
                    dpi_l2_apb_op((128+i)*4,HW_TB_SAO_VER_BASE,1);
                    dpi_l2_apb_op((128+i)*4,HW_TB_SAO_VER_BASE+exception_addr[j][1] - exception_addr[j][0],1);
                    #endif
                  } else {
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[0] - 32);
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[0]-32);
                  }
                } else {
                  if (cfg_temp->dec_mode == HEVC) {
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-3] - 32);
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-3]-32);
                  } else {
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-2] - 32);
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-2]-32);
                  }
                }
              }
            }
          }
        } else{
          fprintf(fp, "W swreg%d/%08x\n",i+base_id,(u32)cwl->reg[CACHE_RD].regMirror[i]);
          #ifdef ASIC_ONL_SIM
          dpi_l2_apb_op((128+i)*4,(u32)cwl->reg[CACHE_RD].regMirror[i],1);
          #endif
        }
      }
      fprintf(fp, "W swreg%d/%08x\n",1+base_id,(u32)((cwl->reg[CACHE_RD].regMirror[1] | 0x80) | 0x1));
      #ifdef ASIC_ONL_SIM
      dpi_l2_apb_op((128+1)*4,(u32)((cwl->reg[CACHE_RD].regMirror[1] | 0x80) | 0x1),1);
      #endif
    }
    fprintf(fp, "\n");
   
    if (!cfg_temp->shaper_enable && !(cwl->clientType == ENCODER_VC8000E)) {
      for (i =0; i < 8; i++) {
        fprintf(fp, "R swreg%d/%08x\n",i+8192, 0);
      }
    } 
    //fprintf(fp, "pic=%d\n", picNum);
    if (cwl->clientType == ENCODER_VC8000E) {
      fprintf(fp, "pic=%d\n", picNum);
      fprintf(fp, "R 00008200/12345678\n");
    } else {
      if (!cfg_temp->shaper_enable)
        fprintf(fp, "R swreg8320/12345678\n");
      else
        fprintf(fp, "R swreg8320/12345678\n");
    }
    
    if (cwl->clientType == ENCODER_VC8000E)
      i = 1;
    else
      i = 1;
  
    for (; i < CACHE_RD_SWREG_AMOUNT; i++)
    {
      if (i == 3) {
        if (cwl->clientType == ENCODER_VC8000E)
          fprintf(fp, "R %08x/%08x\n",i*4+0x00008200,cwl->reg[CACHE_RD].regMirror[i]);
        else if(cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
          for(j = 0; j < cwl->exception_list_amount; j++) {
            if (cfg_temp->dec_mode == H264) {
              if (j == 0) {
	        fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_INDATA",cfg_temp->base_offset);                 
	        fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_INDATA",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->base_offset);
              } else {
                fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-1]-32);
                fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-1]-32);
              }
	    }
            if (cfg_temp->dec_mode == H264_HIGH10) {
              fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j]-32);
              fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j]-32);
            }
	    if (cfg_temp->dec_mode == JPEG) {
	      fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_PJPEG_COFF",cfg_temp->base_offset);
              fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_PJPEG_COFF",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->base_offset);
            }
          }
        } else {
          for(j = 0; j < cwl->exception_list_amount; j++) {
            if (cfg_temp->num_tile_cols < 2) {
              fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j]-32);
              fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j]-32);
            } else {
              if (j == 0) {
                fprintf(fp, "R swreg%d/%s+%X\n",i+base_id,"BASE_DEC_FILT_VER",0);
                fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_FILT_VER",exception_addr[j][1] - exception_addr[j][0]);
              } else if (j == 1) {
                fprintf(fp, "R swreg%d/%s+%X\n",i+base_id,"BASE_DEC_BSD_CTRL",0);
                fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_BSD_CTRL",exception_addr[j][1] - exception_addr[j][0]);
              } else if (j == 2) {
                if (cfg_temp->dec_mode == HEVC) {
                  fprintf(fp, "R swreg%d/%s+%X\n",i+base_id,"BASE_DEC_SAO_VER",0);
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_SAO_VER",exception_addr[j][1] - exception_addr[j][0]);
                } else {
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[0]-32);
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[0]-32);
                } 
              } else {
                if (cfg_temp->dec_mode == HEVC) {
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-3]-32);
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-3]-32);
                } else {
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",cfg_temp->mc_offset[j-2]-32);
                  fprintf(fp, "R swreg%d/%s+%llX\n",i+base_id,"BASE_DEC_DPB",exception_addr[j][1] - exception_addr[j][0]+cfg_temp->mc_offset[j-2]-32);
                }
              }
            }
          }
        }
      } else if (i == 1) {
        fprintf(fp, "R swreg%d/%08x\n",i+base_id,cwl->reg[CACHE_RD].regMirror[i]);
      } else {
        fprintf(fp, "R swreg%d/%08x\n",i+base_id,cwl->reg[CACHE_RD].regMirror[i]);
      }
    }
    fprintf(fp, "\n");
  
  }
  else {
    if (cwl->clientType == DECODER_G2_0 || cwl->clientType == DECODER_G2_1) {
      if (!cfg_temp->tile_id) {
        fprintf(fp1, "pic=%d, num_tiles=%d, num_tile_cols=%d, num_tile_rows=%d\n", picNum, cfg_temp->tile_num, cfg_temp->num_tile_cols, cfg_temp->num_tile_rows);
        for (i = 0; i < cfg_temp->num_tile_rows; i++) {
          for (j = 0; j < cfg_temp->num_tile_cols; j++) {
            fprintf(fp1, "(%d x %d)\t",cfg_temp->tile_size[2 * i * cfg_temp->num_tile_cols + 2 * j], cfg_temp->tile_size[2 * i * cfg_temp->num_tile_cols + 2 * j + 1]);
          }
          fprintf(fp1, "\n");
        }
      }
      fprintf(fp1, "pic=%d, num_tiles=%d, tile_id=%d, width=%d, height=%d\n", picNum, cfg_temp->tile_num, cfg_temp->tile_id, cfg_temp->width, cfg_temp->height);
    }// else
     // fprintf(fp1, "pic=%d\n", picNum);
    if (cwl->clientType == ENCODER_VC8000E) 
    {
      fprintf(fp, "pic=%d\n", picNum);
    } else {
      for (i =0; i < 8; i++) {
        fprintf(fp, "W swreg%d/%08x\n",i+8192, 0);
        #ifdef ASIC_ONL_SIM
        dpi_l2_apb_op(i*4,0x0,1);
        #endif
      }
    }  
    for (i = 0; i < CACHE_WR_SWREG_AMOUNT; i++) 
    {
      if (cwl->clientType == ENCODER_VC8000E) 
      {
        if (i==5||i==10||i==15||i==20||i==25||i==30||i==35||i==40) 
        {
          fprintf(fp, "W %08x/%08x+%s\n",i*4+0x00008020,cwl->reg[CACHE_WR].regMirror[i],((cwl->reg[CACHE_WR].regMirror[i])&0xfffffff0)>>4>23?"NULL":addr_type_print[((cwl->reg[CACHE_WR].regMirror[i])&0xfff0)>>4]);
        } else 
        {
          fprintf(fp, "W %08x/%08x\n",i*4+0x00008020,(u32)cwl->reg[CACHE_WR].regMirror[i]);
        }
      } else if (cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
          if (i == 0) {
            fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)(cwl->reg[CACHE_WR].regMirror[i] & ~0x1));
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,(u32)(cwl->reg[CACHE_WR].regMirror[i] & ~0x1),1);
            #endif
          }
          else if (i==5||i==10||i==15||i==20||i==25||i==30||i==35||i==40||i==45||i==50||i==55||i==60||i==65||i==70||i==75||i==80) {
          if (i / 5 - 1 <= cwl->valid_ch_num[CACHE_WR]) {
	    if (cwl->reg[CACHE_WR].regMirror[i] & 0x1) {
              if (cfg_temp->shaper_bypass) {
                if (!cfg_temp[i / 5 - 1 - index].pp_buffer){
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+8200,"BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((8+i)*4,*HW_TB_DPB_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                  #endif
                }
                else{
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+8200,"BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((8+i)*4,HW_TB_PP_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                  #endif
                }
              } else {
                if (!cfg_temp[i / 5 - 1 - index].pp_buffer){
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+8200,"BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((8+i)*4,*HW_TB_DPB_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                  #endif
                }
                else{
                  fprintf(fp, "W swreg%d/%s+%llX\n",i+8200,"BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  #ifdef ASIC_ONL_SIM
                  dpi_l2_apb_op((8+i)*4,HW_TB_PP_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                  #endif
                }
              }
	    } else {
	      fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
              #ifdef ASIC_ONL_SIM
              dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
              #endif
            }
          } else{
            fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
            #endif
          }
        } else {
          if (i==6||i==11||i==16||i==21||i==26||i==31||i==36||i==41||i==46||i==51||i==56||i==61||i==66||i==71||i==76||i==81){
            fprintf(fp, "W swreg%d/%08x\n",i+8200,0);
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,0x0,1);
            #endif
          }
          else{
            fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
            #endif
          }
         }
      } else {
        if (i == 0) {
          fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)(cwl->reg[CACHE_WR].regMirror[i] & ~0x1));
          #ifdef ASIC_ONL_SIM
          dpi_l2_apb_op((8+i)*4,(u32)(cwl->reg[CACHE_WR].regMirror[i] & ~0x1),1);
          #endif
        } else if (i==5||i==10||i==15||i==20||i==25||i==30||i==35||i==40||i==45||i==50||i==55||i==60||i==65||i==70||i==75||i==80) {
          if (i / 5 - 1 <= cwl->valid_ch_num[CACHE_WR]) {
	    if (cwl->reg[CACHE_WR].regMirror[i] & 0x1) {
              if (!cfg_temp->shaper_bypass) {
                if (cfg_temp->hw_id == 18001) {
                  if (!cfg_temp[i / 5 - 1 - index].pp_buffer){
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    #ifdef ASIC_ONL_SIM
                    dpi_l2_apb_op((8+i)*4,*HW_TB_DPB_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                    #endif
                  }
                  else{
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    #ifdef ASIC_ONL_SIM
                    dpi_l2_apb_op((8+i)*4,HW_TB_PP_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                    #endif
                  }
                } else {
                  if (cwl->valid_ch_num[CACHE_WR] == 2) {
                    if (i == 5){
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                      #endif
                    }
                    else{
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_CHROMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                      #endif
                    }
                  } else {
                    if (i == 5 || i == 10){
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                      #endif
                    }
                    else{
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_CHROMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                      #endif
                    }
                  }
                } 
              } else {
                if (cfg_temp->hw_id == 18001) {
                  if (!cfg_temp[i / 5 - 1 - index].pp_buffer){
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                    #ifdef ASIC_ONL_SIM
                    dpi_l2_apb_op((8+i)*4,*HW_TB_DPB_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                    #endif
                  }
                  else{
                    fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    #ifdef ASIC_ONL_SIM
                    dpi_l2_apb_op((8+i)*4,HW_TB_PP_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)),1);
                    #endif
                  }
                } else {
                  if (cwl->valid_ch_num[CACHE_WR] == 2) {
                    if (i == 5){
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                      #endif
                    }
                    else{
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_CHROMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                      #endif
                    }
                  } else {
                    if (i == 5 || i == 10){
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_LUMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                      #endif
                    }
                    else{
                      fprintf(fp, "W swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      #ifdef ASIC_ONL_SIM
                      dpi_l2_apb_op((8+i)*4,HW_TB_DPB_CHROMA_BASE+(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)),1);
                      #endif
                    }
                  }
                }
              }  
            } else {
	      fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
              #ifdef ASIC_ONL_SIM
              dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
              #endif
            }		    
	  } else {
            fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
            #endif
	  }
        } else {
          if (cfg_temp->cache_version > 0x2) {
            if (i==6||i==11||i==16||i==21||i==26||i==31||i==36||i==41||i==46||i==51||i==56||i==61||i==66||i==71||i==76||i==81){
              fprintf(fp, "W swreg%d/%08x\n",i+8200,0);
              #ifdef ASIC_ONL_SIM
              dpi_l2_apb_op((8+i)*4,0x0,1);
              #endif
            }
            else{
              fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
              #ifdef ASIC_ONL_SIM
              dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
              #endif
            }
          } else {
            fprintf(fp, "W swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
            #ifdef ASIC_ONL_SIM
            dpi_l2_apb_op((8+i)*4,(u32)cwl->reg[CACHE_WR].regMirror[i],1);
            #endif
          }
        }
      } 
    }
	if(cwl->clientType != ENCODER_VC8000E){
      fprintf(fp, "W swreg%d/%08x\n",8200,(u32)(cwl->reg[CACHE_WR].regMirror[0] | 0x1));
      #ifdef ASIC_ONL_SIM
      dpi_l2_apb_op(8*4,(u32)(cwl->reg[CACHE_WR].regMirror[0] | 0x1),1);
      #endif
	}
    fprintf(fp, "\n");
#if 1
    if (cwl->clientType == DECODER_G2_0 || cwl->clientType == DECODER_G2_1)
      fprintf(fp1, "pic=%d, num_tiles=%d, tile_id=%d, width=%d, height=%d\n", picNum, cfg_temp->tile_num, cfg_temp->tile_id, cfg_temp->width, cfg_temp->height); 
    //else
    //  fprintf(fp1, "pic=%d\n", picNum);
#endif
    if (cwl->clientType == ENCODER_VC8000E) {
      fprintf(fp, "pic=%d\n", picNum); 
      for (i = 0; i < 8; i++)
      {
        fprintf(fp, "R %08x/%08x\n",i*4+0x00008000,0);
      }
   
      for (i = 0; i < CACHE_WR_SWREG_AMOUNT-8; i++) 
      {
        fprintf(fp, "R %08x/%08x\n",i*4+0x00008000+0x20,cwl->reg[CACHE_WR].regMirror[i]);
      }
    } else {
      for (i = 0; i < 8; i++)
      {
        fprintf(fp, "R swreg%d/%08x\n",i+8192,0);
      }

      for (i = 0; i < CACHE_WR_SWREG_AMOUNT; i++)
      {
        if(cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
          if (i==5||i==10||i==15||i==20||i==25||i==30||i==35||i==40||i==45||i==50||i==55||i==60||i==65||i==70||i==75||i==80) {
            if (i / 5 - 1 <= cwl->valid_ch_num[CACHE_WR]) {
	      if (cwl->reg[CACHE_WR].regMirror[i] & 0x1) {
                if (cfg_temp->shaper_bypass) {
                  if (!cfg_temp[i / 5 - 1 - index].pp_buffer)
                    fprintf(fp, "R swreg%d/%s+%llX\n",i+8200,"BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                  else
                    fprintf(fp, "R swreg%d/%s+%llX\n",i+8200,"BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                } else {
                  if (!cfg_temp[i / 5 - 1 - index].pp_buffer)
                    fprintf(fp, "R swreg%d/%s+%llX\n",i+8200,"BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  else
                    fprintf(fp, "R swreg%d/%s+%llX\n",i+8200,"BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                }
              } else
	        fprintf(fp, "R swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
            } else
              fprintf(fp, "R swreg%d/%08x\n",i+8200,(u32)cwl->reg[CACHE_WR].regMirror[i]);
          } else {
            if (i==6||i==11||i==16||i==21||i==26||i==31||i==36||i==41||i==46||i==51||i==56||i==61||i==66||i==71||i==76||i==81)
              fprintf(fp, "R swreg%d/%08x\n",i+8200,0); 
            else
              fprintf(fp, "R swreg%d/%08x\n",i+8200,cwl->reg[CACHE_WR].regMirror[i]);
          }
        } else {
          if (i==5||i==10||i==15||i==20||i==25||i==30||i==35||i==40||i==45||i==50||i==55||i==60||i==65||i==70||i==75||i==80) {
            if (i / 5 - 1 <= cwl->valid_ch_num[CACHE_WR]) {
	      if (cwl->reg[CACHE_WR].regMirror[i] & 0x1) {
                if (!cfg_temp->shaper_bypass) {
                  if (cfg_temp->hw_id == 18001) {
                    if (!cfg_temp[i / 5 - 1 - index].pp_buffer)
                      fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    else
                      fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  } else {
                    if (cwl->valid_ch_num[CACHE_WR] == 2) {
                      if (i == 5)
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      else
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    } else {
                      if (i == 5 || i == 10)
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                      else
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                    }
                  }
                } else {
                  if (cfg_temp->hw_id == 18001) {
                    if (!cfg_temp[i / 5 - 1 - index].pp_buffer)
                      fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                    else
                      fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_PP_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xf)));
                  } else {
                    if (cwl->valid_ch_num[CACHE_WR] == 2) {
                      if (i == 5)
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      else
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                    } else {
                      if (i == 5 || i == 10)
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_LUMA",(cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                      else
                        fprintf(fp, "R swreg%d/%s+%llX\n",i+8200, "BASE_DEC_DPB_CHROMA", (cfg_temp[i / 5 - 1 - index].base_offset + (cwl->reg[CACHE_WR].regMirror[i] & 0xe)));
                    }
                  }
                }
              } else
	        fprintf(fp, "R swreg%d/%08x\n",i+8200,cwl->reg[CACHE_WR].regMirror[i]);
	    } else
	      fprintf(fp, "R swreg%d/%08x\n",i+8200,cwl->reg[CACHE_WR].regMirror[i]);
          } else {
            if (i==6||i==11||i==16||i==21||i==26||i==31||i==36||i==41||i==46||i==51||i==56||i==61||i==66||i==71||i==76||i==81)
              fprintf(fp, "R swreg%d/%08x\n",i+8200,0);
            else
              fprintf(fp, "R swreg%d/%08x\n",i+8200,cwl->reg[CACHE_WR].regMirror[i]);
          }
        }
      }
    }
    fprintf(fp, "\n");
  }
}


void cachePrintInfo(cache_cwl_t *cwl,cache_dir dir)
{
  FILE *fp;
  u32 i = 0;
  CWLChannelConf_t *cfg_temp = NULL;
  cfg_temp = cwl->cfg[dir];
  if (dir == CACHE_WR) {
    cfg_temp = cwl->cfg[CACHE_WR] + cwl->first_channel_num;
  }
  if (cwl->clientType != ENCODER_VC8000E) {
    if (dir == CACHE_RD)
      fp = cfg_temp->file_fid;
    if (dir == CACHE_WR)
      fp = cfg_temp->file_fid;
  }
  if (!fp)
    return;
  if (cwl->clientType == DECODER_G1_0 || cwl->clientType == DECODER_G1_1) {
    if (dir == CACHE_WR) {
      if (!cfg_temp->cache_enable && cfg_temp->shaper_enable) {
        fprintf(fp, "#######################################################\n");
        fprintf(fp, "#picture=%d,stream_buffer=%d,output_slice=%d\n",
                cfg_temp->hw_dec_pic_count, cfg_temp->stream_buffer_id, 0);
        fprintf(fp, "#######################################################\n");
      }
    }
  }
}

void CacheRegisterDumpAfter(cache_cwl_t *cwl,cache_dir dir)
{
  FILE *fp;
  u32 i = 0;
  CWLChannelConf_t *cfg_temp = NULL;
  cfg_temp = cwl->cfg[dir];
  if (dir == CACHE_WR) {
    cfg_temp = cwl->cfg[CACHE_WR] + cwl->first_channel_num;
  }
  if (cwl->clientType != ENCODER_VC8000E) {
    if (dir == CACHE_RD)
      fp = cfg_temp->file_fid;
    if (dir == CACHE_WR)
      fp = cfg_temp->file_fid;
  }
  else
    return;
  
  if (!fp)
    return;
  if (cfg_temp->cache_enable) {
    if (dir == CACHE_RD)
      fprintf(fp, "W swreg8321/00000000\n");
  }
  if (cfg_temp->shaper_enable) {
    if (dir == CACHE_WR) {
      fprintf(fp, "W swreg8200/00000000\n");
      fprintf(fp, "B swreg8203/00000002 POLL_CYCLE TB_TIMEOUT\n");
      fprintf(fp, "W swreg8203/00000002\n");
      fprintf(fp, "C\n");
    }
  }
}
/* SW/SW shared memory */
/*------------------------------------------------------------------------------
    Function name   : CWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()
    
    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available
    
    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *CWLmalloc(u32 n)
{

    void *p = malloc((size_t) n);

    PTRACE("CWLmalloc\t%8d bytes --> %p\n", n, p);

    return p;
}

/*------------------------------------------------------------------------------
    Function name   : CWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()
    
    Return type     : void 
    
    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void CWLfree(void *p)
{
    PTRACE("CWLfree\t%p\n", p);
    if(p != NULL)
        free(p);
}

/*------------------------------------------------------------------------------
    Function name   : CWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()
    
    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available
    
    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *CWLcalloc(u32 n, u32 s)
{
    void *p = calloc((size_t) n, (size_t) s);

    PTRACE("CWLcalloc\t%8d bytes --> %p\n", n * s, p);

    return p;
}

/*------------------------------------------------------------------------------
    Function name   : CWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()
    
    Return type     : The value of destination d
    
    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *CWLmemcpy(void *d, const void *s, u32 n)
{
    return memcpy(d, s, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : CWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()
    
    Return type     : The value of destination d
    
    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
void *CWLmemset(void *d, i32 c, u32 n)
{
    return memset(d, (int) c, (size_t) n);
}

/*------------------------------------------------------------------------------
    Function name   : CWLmemcmp
    Description     : Compares two buffers. Same functionality
                      as the ANSI C memcmp()

    Return type     : Zero if the first n bytes of s1 match s2

    Argument        : const void *s1 - Buffer to compare
    Argument        : const void *s2 - Buffer to compare
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
int CWLmemcmp(const void *s1, const void *s2, u32 n)
{
    return memcmp(s1, s2, (size_t) n);
}
