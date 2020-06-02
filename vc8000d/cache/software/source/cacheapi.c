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
--                 The entire notice above must be reproduced                                                  --
--                  on all copies and should not be removed.                                                     --
--                                                                                                                                --
--------------------------------------------------------------------------------
--
--  Cache API source code
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cacheapi.h"
#include "base_type.h"
#include "cwl.h"

u64 exception_addr[64][2];
/*----------------------------------------------------------------------------
    function define
------------------------------------------------------------------------------*/
/*Enable one channel of cache with configuration and return status and channel id*/
i32 EnableCacheChannel(void **dev,u32 *channel, CWLChannelConf_t *cfg,client_type client,cache_dir dir)
{
  u32 core_id = 0;
  u32 name = 0;
  CWLChannelConf_t *cfg_temp = NULL;
  
  if (dev == NULL)
    return CACHE_ERROR;
  
  cache_cwl_t *cwl = (cache_cwl_t *)*dev;

  if (cwl == NULL && (cwl = CWLInit(client)) == NULL)
  {
    *dev = NULL;
    return CACHE_ERROR;
  }
  
  if (cwl->reg[dir].core_id < 0)
  {
    if (CWL_ERROR == CWLReserveHw((void *)cwl,client,dir))
    {
      printf("Enable cache failed due to HW reservation\n");
      goto error;
    }
  }

  if (dir == CACHE_RD && cfg->cache_all == 1)
  {
    cwl->cache_all = 1;
    *dev = cwl;
    cfg_temp = cwl->cfg[dir];
    memcpy(&cfg_temp[cwl->valid_ch_num[dir]],cfg,sizeof(CWLChannelConf_t));
    return CACHE_OK;
  }
  if (cfg->cache_version >= 0x4 && cfg->pp_buffer)
    cwl->valid_ch_num[dir] = 4 + 3 * cfg->ppu_index + cfg->ppu_sub_index;

  if (cwl->used_ch_num[dir] >= cwl->configured_channel_num[dir])
    return CACHE_ERROR;

  if (dir == CACHE_RD)
  {
    name = HWIF_CACHE_CHANNEL_0_VALILD + cwl->valid_ch_num[dir]*10;
  }
  else
  {
    name = HWIF_CACHE_WR_CH_0_VALID + cwl->valid_ch_num[dir]*15;
  }
  
  CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,name,1,0);

  cfg_temp = cwl->cfg[dir];
#ifdef CACHE_SHAPER_SIM
  if (client == ENCODER_VC8000E)
  {
    cfg->start_addr = cfg->trace_start_addr;
	cfg->end_addr = cfg->trace_end_addr;
  }
#endif
  memcpy(&cfg_temp[cwl->valid_ch_num[dir]],cfg,sizeof(CWLChannelConf_t));

  if (!cwl->first_channel_flag) {
    cwl->first_channel_num = cwl->valid_ch_num[dir];
    cwl->first_channel_flag = 1;
  }
  *channel = cwl->valid_ch_num[dir];
  cwl->valid_ch_num[dir]++;
  cwl->used_ch_num[dir]++;
  *dev = cwl;
  
  return CACHE_OK;

error:
  *dev = NULL;
  *channel = ~0;
  CWLRelease((void *)cwl);
  return CACHE_ERROR;

}

/*When all the config is done, call this function to make cache work*/
i32 EnableCacheWork(void *dev)
{
  cache_cwl_t *cwl = (cache_cwl_t *) dev;
  u32 reg_name = 0;
  u32 i = 0;
  CWLChannelConf_t *cfg_temp = NULL;
  cache_dir dir;

  if (dev == NULL)
    return CACHE_ERROR;

  for (dir = CACHE_RD; dir<CACHE_BI; dir++)
  {
    if (cwl->reg[dir].core_id < 0)
    {
      //printf("No any workable reserved HW in dir\n");
      continue;
    }
  
    if (dir == CACHE_RD)
    {
      if (1 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ENABLE,1))
        continue;

      if (cwl->cache_all == 0 && cwl->valid_ch_num[dir]==0)
        continue;
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ALL,cwl->cache_all,0);
#ifndef CACHE_SHAPER_SIM
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_AXI_RD_ID, 0x10, 0);
#endif
      if (cwl->cfg[dir]->cache_version >= 0x5)
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_AXI_RD_ID_E,1,0);
      if (cwl->exception_list_amount==0)
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_EXP_WR_E,0,0);//exception list disable
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_TIME_OUT_THR,0,0);

      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_SW_REORDER_E,1,0);//reorder fixed to 1
      if (cwl->cache_all == 0 && cwl->valid_ch_num[dir]>0)
      {
        cfg_temp = cwl->cfg[dir];
      
        for (i=0; i<cwl->channel_num[dir]; i++)
        {
         reg_name = HWIF_CACHE_CHANNEL_0_VALILD + 10*i;
         if (1 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name,0))
         {
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+3,cfg_temp[i].start_addr,0);//start addr
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+4,cfg_temp[i].end_addr,0);//end addr   
         }
        }
      }
    #ifndef CACHE_SHAPER_SIM
      for (i=2; i<CACHE_RD_SWREG_AMOUNT; i++)
      {
       CWLWriteReg(&cwl->reg[dir],i*4,cwl->reg[dir].regMirror[i]);
      }
    #endif
#ifdef CWL_PRINT
      for (i = 2; i < 5; i++)
        printf("W swreg%d/%08x\n",8320+i,cwl->reg[dir].regMirror[i]);
      printf("W swreg%d/%08x\n",8321,(cwl->reg[dir].regMirror[1]|0x1));
#endif
      cwl->reference_count++;
      CWLEnableCache(cwl,dir); //last, enable cache
    }
    else
    {
      if (1 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_ENABLE,1))
        continue;

      if (cwl->valid_ch_num[dir]==0)
        continue;
	  
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_BASE_ID,0,0);//provided by HW
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_TIME_OUT,0xFF,0);
      cfg_temp = cwl->cfg[dir];
    
      for (i=0; i<cwl->channel_num[dir]; i++)
      {
       reg_name = HWIF_CACHE_WR_CH_0_VALID + 15*i;
       if (1 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name,0))
       {
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+1,cfg_temp[i].stripe_e,0);//stripe enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+2,cfg_temp[i].pad_e,0);//pad enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+3,cfg_temp[i].rfc_e,0);//rfc enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+4,(cfg_temp[i].start_addr & 0xFFFFFFF),0);//start addr
        if (cwl->cfg[dir]->cache_version < 0x3)
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+5,cfg_temp[i].block_e,0);//block_en
        else
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+6,((cfg_temp[i].start_addr >> 28) & 0xFFFFFFFF),0);//start addr
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+7,cfg_temp[i].line_size>65535?65535:cfg_temp[i].line_size,0);//line size
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+8,cfg_temp[i].line_stride,0);//line stride
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+9,cfg_temp[i].line_cnt,0);//line count
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+10,cfg_temp[i].max_h,0);//max_h
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+11,cfg_temp[i].ln_cnt_start,0);//ln_cnt_start
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+12,cfg_temp[i].ln_cnt_mid,0);//ln_cnt_mid
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+13,cfg_temp[i].ln_cnt_end,0);//ln_cnt_end
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+14,cfg_temp[i].ln_cnt_step,0);//ln_cnt_step
       }
      }
  
#ifndef CACHE_SHAPER_SIM
      for (i=1; i<CACHE_WR_SWREG_AMOUNT;i++)
      {
        CWLWriteReg(&cwl->reg[dir],i*4,cwl->reg[dir].regMirror[i]);
      }
#endif
#ifdef CWL_PRINT
      for (i = 1; i < 85; i++)
        printf("W swreg%d/%08x\n",8200+i,cwl->reg[dir].regMirror[i]);
      printf("W swreg%d/%08x\n",8200,(cwl->reg[dir].regMirror[0]|0x1));
#endif
      cwl->reference_count++;    
      CWLEnableCache(cwl,dir); //last, enable cache
  
    }
  }

  return CACHE_OK;

}

/*Disable specified channel of cache and return status*/
i32 DisableCacheChannel(void *dev,cache_dir dir)
{
  u32 core_id = 0;
  cache_cwl_t *cwl = (cache_cwl_t *) dev;
  u32 name = 0;
  u32 i=0;
  u32 irq_status;
  i32 ret = CACHE_OK;

  if (dev == NULL)
    return CACHE_ERROR;

  if (cwl->reg[dir].core_id < 0)
  {
    printf("No any workable reserved HW\n");
    return CACHE_ERROR;
  }
  if (dir == CACHE_RD)
  {
    if (0 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ENABLE,1))
    {
      printf("Cache read is not enabled\n");
      return CACHE_ERROR;
    }
  }
  else
  {
    if (0 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_ENABLE,1))
    {
      printf("Cache write is not enabled\n");
      return CACHE_ERROR;
    }
  }
  if (dir == CACHE_RD && cwl->cache_all == 1)
  {
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_EXP_WR_E,0,1);//exception list disable
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_SW_REORDER_E,0,1);
    cwl->exception_list_amount = 0;
    goto out;
  }

  cwl->used_ch_num[dir] = 0;
out:
#ifdef CACHE_SHAPER_SIM
  CWLDisableCache(cwl,dir);
#endif
  cwl->reference_count--;
#ifndef CACHE_SHAPER_SIM
  if (dir == CACHE_RD)
  {
    CWLDisableCache(cwl,dir);
  }
  else
  {
    CWLDisableCache(cwl,dir);
    CWLWaitChannelAborted(cwl,&irq_status,dir);
    if (irq_status == CACHE_ERROR_RE)
      ret = CACHE_HW_FETAL_RECOVERY;
    if (irq_status == CACHE_ERROR_UNRE)
      ret = CACHE_HW_FETAL_UNRECOVERY;
  }
#endif
  CWLReleaseHw(cwl,dir);
  return ret;
}

/*Disable all channels of cache*/
i32 DisableCacheChannelALL(void **dev, cache_dir dir_specified)
{
  cache_cwl_t *cwl = (cache_cwl_t *) *dev;
  u32 i = 0;
  i32 ret = CACHE_OK;
  cache_dir dir = CACHE_RD;
  u32 ch_num = 0;
  cache_dir start_dir,end_dir;

  if (cwl == NULL)
    return CACHE_ERROR;
  if (dir_specified == CACHE_BI)
  {
        start_dir = CACHE_RD;
        end_dir = CACHE_WR;
  }
  else
        start_dir = end_dir = dir_specified;

  for (dir = start_dir; dir <= end_dir; dir++)
  {
    if (dir == CACHE_RD && cwl->cache_all == 1)
    {
#ifdef CACHE_SHAPER_SIM
      if (cwl->cfg[CACHE_RD]->hw_dec_pic_count >= cwl->cfg[CACHE_RD]->trace_start_pic)
        CacheRegisterDumpAfter(cwl,CACHE_RD);
#endif
      DisableCacheChannel(cwl,CACHE_RD);
    }
    else if (dir == CACHE_WR)
    {
#ifdef CACHE_SHAPER_SIM
      if (cwl->valid_ch_num[CACHE_WR] && ((cwl->cfg[CACHE_WR]+cwl->first_channel_num)->hw_dec_pic_count >= (cwl->cfg[CACHE_WR]+cwl->first_channel_num)->trace_start_pic))
        CacheRegisterDumpAfter(cwl,CACHE_WR);
#endif
      ret = DisableCacheChannel(cwl,dir);
      if (ret != CACHE_OK)
      {
        printf("cache diable channel failed!!\n");
        goto end;
      }
    }
  }
  if (cwl->reference_count!=0)
    return CACHE_OK;
end:
  CWLRelease(cwl);
  *dev = NULL;
  return ret;
}

/*Set exception addr of cache. Each exception buffer is specified by input 32-bit start addr
    and 32-bit end addr and return status*/
i32 SetCacheExpAddr(void *dev,u64 start_addr,u64 end_addr)
{
  u32 core_id = 0;
  cache_cwl_t *cwl = (cache_cwl_t *) dev;
  u32 exp_list_amount = 0;
  
  if (dev == NULL)
    return CACHE_ERROR;

  if (cwl->reg[CACHE_RD].core_id < 0)
  {
    printf("No any workable reserved HW\n");
    return CACHE_ERROR;
  }

  if (cwl->exception_list_amount == cwl->exception_addr_max)
  {
    printf("exception list is full\n");
    return CACHE_ERROR;
  }
  
  CWLAsicSetRegisterValue(&cwl->reg[CACHE_RD],cwl->reg[CACHE_RD].regMirror,HWIF_CACHE_EXP_WR_E,1,1);
  CWLAsicSetRegisterValue(&cwl->reg[CACHE_RD],cwl->reg[CACHE_RD].regMirror,HWIF_CACHE_EXP_LIST,(start_addr & 0xFFFFFFFF),1);
  CWLAsicSetRegisterValue(&cwl->reg[CACHE_RD],cwl->reg[CACHE_RD].regMirror,HWIF_CACHE_EXP_LIST,(end_addr & 0xFFFFFFFF),1);
  CWLAsicSetRegisterValue(&cwl->reg[CACHE_RD],cwl->reg[CACHE_RD].regMirror,HWIF_CACHE_EXP_LIST,((start_addr>>32) & 0xFFFFFFFF),1);
  CWLAsicSetRegisterValue(&cwl->reg[CACHE_RD],cwl->reg[CACHE_RD].regMirror,HWIF_CACHE_EXP_LIST,((end_addr>>32) & 0xFFFFFFFF),1);

  exception_addr[cwl->exception_list_amount][0] = start_addr;
  exception_addr[cwl->exception_list_amount][1] = end_addr;
  cwl->exception_list_amount++;

  return CACHE_OK;

}

i32 printInfo(void *dev, CWLChannelConf_t *cfg)
{
  cache_cwl_t *cwl = (cache_cwl_t *) dev;
  u32 i;
  CWLChannelConf_t *cfg_temp = NULL;
  if (dev == NULL)
    return CACHE_ERROR;

  if (cwl->reg[CACHE_RD].core_id < 0)
  {
    printf("No any workable reserved HW\n");
    return CACHE_ERROR;
  }
  cfg_temp = cwl->cfg[CACHE_WR] + cwl->first_channel_num;
  cfg_temp->hw_dec_pic_count = cfg->hw_dec_pic_count;
  cfg_temp->stream_buffer_id = cfg->stream_buffer_id;
  if (cfg_temp->hw_dec_pic_count >= cwl->cfg[CACHE_WR]->trace_start_pic)
    cachePrintInfo(cwl, CACHE_WR);
  return CACHE_OK;
}

u32 ReadCacheVersion()
{
  u32 id = CWLReadAsicID();
  return ((id & 0xF000) >> 12);
}

i32 EnableCacheWorkDumpRegs(void *dev, cache_dir dir,
                            u32 *cache_regs, u32 *cache_reg_size,
                            u32 *shaper_regs, u32 *shaper_reg_size) {
  cache_cwl_t *cwl = (cache_cwl_t *) dev;
  u32 reg_name = 0;
  u32 i = 0;
  u32 index = 0;
  CWLChannelConf_t *cfg_temp = NULL;

  if (dev == NULL)
    return CACHE_ERROR;

  if (dir == CACHE_RD) {
  
    if (cwl->cache_all == 0 && cwl->valid_ch_num[dir]==0)
      return CACHE_ERROR;
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ALL,cwl->cache_all,0);
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_AXI_RD_ID, 0x10, 0);
    if (cwl->cfg[dir]->cache_version >= 0x5)
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_AXI_RD_ID_E,1,0);
    if (cwl->exception_list_amount==0)
      CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_EXP_WR_E,0,0);//exception list disable
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_TIME_OUT_THR,0,0);

    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_SW_REORDER_E,1,0);//reorder fixed to 1
    cwl->reference_count++;
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_ENABLE,1,0);
    for (i = 0; i < 3; i++)
      cache_regs[i] = cwl->reg[dir].regMirror[i];
    *cache_reg_size = 3;
  } else {
    if (cwl->valid_ch_num[dir]==0)
      return CACHE_ERROR;

    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_BASE_ID,0,0);//provided by HW
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_TIME_OUT,0xFF,0);
    cfg_temp = cwl->cfg[dir];

    for (i=0; i<cwl->channel_num[dir]; i++)
    {
      reg_name = HWIF_CACHE_WR_CH_0_VALID + 15*i;
      if (1 == CWLAsicGetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name,0))
      {
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+1,cfg_temp[i].stripe_e,0);//stripe enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+2,cfg_temp[i].pad_e,0);//pad enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+3,cfg_temp[i].rfc_e,0);//rfc enable
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+4,(cfg_temp[i].start_addr & 0xFFFFFFF),0);//start addr
        if (cwl->cfg[dir]->cache_version < 0x3)
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+5,cfg_temp[i].block_e,0);//block_en
        else
          CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+6,((cfg_temp[i].start_addr >> 28) & 0xFFFFFFFF),0);//start addr
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+7,cfg_temp[i].line_size>65535?65535:cfg_temp[i].line_size,0);//line size
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+8,cfg_temp[i].line_stride,0);//line stride
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+9,cfg_temp[i].line_cnt,0);//line count
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+10,cfg_temp[i].max_h,0);//max_h
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+11,cfg_temp[i].ln_cnt_start,0);//ln_cnt_start
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+12,cfg_temp[i].ln_cnt_mid,0);//ln_cnt_mid
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+13,cfg_temp[i].ln_cnt_end,0);//ln_cnt_end
        CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,reg_name+14,cfg_temp[i].ln_cnt_step,0);//ln_cnt_step
        index = i;
      }
    }
    cwl->reference_count++;
    CWLAsicSetRegisterValue(&cwl->reg[dir],cwl->reg[dir].regMirror,HWIF_CACHE_WR_ENABLE,1,0);
    for (i = 0; i < 85; i++)
      shaper_regs[i] = cwl->reg[dir].regMirror[i];
    *shaper_reg_size = 85;
  }
  return CACHE_OK;
}
