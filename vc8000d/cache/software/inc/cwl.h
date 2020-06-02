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
--  Abstract : Cache Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/

#ifndef __CWL_H__
#define __CWL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "base_type.h"
#include "cwl_common.h"
#include "cacheapi.h"

/* Return values */
#define CWL_OK                      0
#define CWL_ERROR                  -1

#define CWL_HW_WAIT_OK              CWL_OK
#define CWL_HW_WAIT_ERROR           CWL_ERROR
#define CWL_HW_WAIT_TIMEOUT         1

#define CACHE_ABORTED               2
#define CACHE_ERROR_RE              4
#define CACHE_ERROR_UNRE            8

 typedef struct
{
 i32 core_id;
 u32 regSize;             /* IO mem size */
 u32 regBase;
 volatile u32 *pRegBase;  /* IO mem base */
 u32 regMirror[CACHE_SWREG_MAX];
 volatile u32 *pComRegBase;  /* common base of L2*/
}regMapping;

typedef struct {
  int id;
  int is_read;
} CWLHwConfig_t;
/* CWL internal information for Linux */
typedef struct
{
    u32 clientType; /*indicate which client use*/
    int fd_mem;              /* /dev/mem */
    int fd_cache;            /* /dev/hantro_cache */

    regMapping reg[2];
    u32 core_num[2];  /*indicate how many cores in HW*/
    u32 channel_num[2]; /*indicate how many channels supported by HW*/
    u32 configured_channel_num[2]; /*indicated the configured avaliable channel*/
    u32 valid_ch_num[2];/*indicate valid channels  set by user*/
    u32 used_ch_num[2];/*indicate how many channels valid set by user*/
    CWLChannelConf_t *cfg[2];
    u32 cache_all; //indiate cache_all feature enable
    u32 exception_list_amount;//indicate how many exception addrs have been added
    u32 exception_addr_max;//indicate the max num of exception addrs supported by HW
    u32 reference_count;//indicate is any instance using CWL 
    u32 first_channel_num;
    u32 first_channel_flag;

} cache_cwl_t;

 /*------------------------------------------------------------------------------
      Function defination
  ------------------------------------------------------------------------------*/
void *CWLInit(client_type client);
i32 CWLRelease(const void *inst);
void CWLWriteReg(const void *inst, u32 offset, u32 val);
void CWLEnableCache(const void *inst,cache_dir dir);
void CWLDisableCache(const void *inst,cache_dir dir);
u32 CWLReadReg(const void *inst, u32 offset);
i32 CWLReserveHw(const void *inst,client_type client,cache_dir dir);
void CWLReleaseHw(const void *inst,cache_dir dir);
i32 CWLWaitChannelAborted(const void *inst,u32* status_register,cache_dir dir);
void CWLAsicSetRegisterValue(void *reg,u32 *regMirror, regName name, u32 value,u32 write_asic);
u32 CWLAsicGetRegisterValue(const void *reg, u32 *regMirror, regName name,u32 read_asic);
void *CWLmalloc(u32 n);
void CWLfree(void *p);
void *CWLcalloc(u32 n, u32 s);
void *CWLmemcpy(void *d, const void *s, u32 n);
void *CWLmemset(void *d, i32 c, u32 n);
int CWLmemcmp(const void *s1, const void *s2, u32 n);
void CacheRegisterDumpAfter(cache_cwl_t *cwl,cache_dir dir);
void cachePrintInfo(cache_cwl_t *cwl,cache_dir dir);
u32 CWLReadAsicID();
#ifdef __cplusplus
}
#endif
#endif
