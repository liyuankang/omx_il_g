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

#ifndef ENC_DEC400_H
#define ENC_DEC400_H

#include "base_type.h"
#include "encasiccontroller.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define  gcregAHBDECControl              0xB00
#define  gcregAHBDECControlEx            0xB70
#define  gcregAHBDECReadExConfig0        0xC80
#define  gcregAHBDECReadExConfig1        0xC84
#define  gcregAHBDECReadExConfig2        0xC88

#define  gcregAHBDECReadConfig0          0x800
#define  gcregAHBDECReadConfig1          0x804
#define  gcregAHBDECReadConfig2          0x808

#define  gcregAHBDECReadBufferBase0      0x900
#define  gcregAHBDECReadBufferBase1      0x904
#define  gcregAHBDECReadBufferBase2      0x908

#define  gcregAHBDECReadBufferBase0Ex    0xF80
#define  gcregAHBDECReadBufferBase1Ex    0xF84
#define  gcregAHBDECReadBufferBase2Ex    0xF88

#define  gcregAHBDECReadBufferEnd0       0xE80
#define  gcregAHBDECReadBufferEnd1       0xE84
#define  gcregAHBDECReadBufferEnd2       0xE88

#define  gcregAHBDECReadBufferEnd0Ex     0x1180
#define  gcregAHBDECReadBufferEnd1Ex     0x1184
#define  gcregAHBDECReadBufferEnd2Ex     0x1188

#define  gcregAHBDECReadCacheBase0       0x980
#define  gcregAHBDECReadCacheBase1       0x984
#define  gcregAHBDECReadCacheBase2       0x988

#define  gcregAHBDECReadCacheBase0Ex     0x1000
#define  gcregAHBDECReadCacheBase1Ex     0x1004         
#define  gcregAHBDECReadCacheBase2Ex     0x1008


typedef struct
{
  u32 format;
  u32 lumWidthSrc;
  u32 lumHeightSrc;
  u32 input_alignment;
  ptr_t dec400LumTableBase;
  ptr_t dec400CbTableBase;
  ptr_t dec400CrTableBase;
}VCDec400data;

#if defined(__LP64__) || defined(_WIN64) || defined(_WIN32)
  
#define EncDec400SetAddrRegisterValue(ewl, name, value) do {\
      EWLWriteReg(ewl, name, value);\
      EWLWriteReg(ewl, name##Ex, (u32)((value) >> 32)); \
    } while (0)
#else 

#define EncDec400SetAddrRegisterValue(ewl, name, value) do {\
      EWLWriteReg(ewl, name, (u32)(value));\
    } while (0)
#endif


  /*------------------------------------------------------------------------------
      Function prototypes
  ------------------------------------------------------------------------------*/
static void EncDec400getAlignedPicSizebyFormat(u32 type,u32 width, u32 height, u32 alignment,
                                        u32 *luma_Size,u32 *chroma_Size,u32 *picture_Size);
static void VCEncSetReadChannel(u32 ts_size_pix,u32 offset,const void *ewl);

static void VCEncSetReadChannel_2(u32 ts_size_pix,u32 offset,const void *ewl);

static void VCEncSetReadChannel_3(u32 ts_size_pix,u32 offset,const void *ewl);

static void VCEncSetReadChannel_4(u32 ts_size_pix,u32 offset,const void *ewl);

i32 VCEncEnableDec400(asicData_s *asic,VCDec400data *dec400_data);

void VCEncDisableDec400(const void *ewl);

void VCEncSetDec400StreamBypass(const void *ewl);

#ifdef __cplusplus
}
#endif

#endif
