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
--  Cache API header
--
------------------------------------------------------------------------------*/

#ifndef CACAPI_H
#define CACAPI_H

#include "base_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CACHE_OK  0
#define CACHE_TIMEOUT 1
#define CACHE_ERROR -1
#define CACHE_HW_FETAL_RECOVERY -2
#define CACHE_HW_FETAL_UNRECOVERY -3

typedef enum
{
 CACHE_RD,
 CACHE_WR,
 CACHE_BI
}cache_dir;

typedef enum
{
 ENCODER_VC8000E,
 DECODER_VC8000D_0,
 DECODER_VC8000D_1,
 DECODER_G1_0,
 DECODER_G1_1,
 DECODER_G2_0,
 DECODER_G2_1
}client_type;

enum addr_type
{
 BaseInLum,
 BaseInCb,
 BaseRefLum1,
 BaseRefChr1,
 BaseRefLum0,
 BaseRefChr0,
 BaseRef4nLum1,
 BaseRef4nLum0,
 BottomBaseRefLum1,
 BottomBaseRefChr1,
 BaseRefLum2,
 BottomBaseRefLum2,
 BaseRefChr2,
 BottomBaseRefChr2,
 BaseRefLum3,
 BottomBaseRefLum3,
 BaseRefChr3,
 BottomBaseRefChr3,
 BaseRef4nLum2,
 BaseRef4nLum3,
 BottomBaseRef4nLum1,
 BottomBaseRef4nLum2,
 BottomBaseRef4nLum3,
 BaseStream
};


typedef struct
{
 /*both for cache read and write*/
 u64 start_addr;//equal to physical addr>>4 and the start_addr<<4 must be aligned to 1<< LOG2_ALIGNMENT which is fixed in HW
 u32 trace_start_addr;//start addr for simulation trace file
 u64 base_offset;
 u64 mc_offset[16];
 u64 tb_offset;
 /*only for cache read*/
 u32 no_chroma;
 u32 shaper_bypass;
 u32 cache_enable;
 u32 shaper_enable;
 u32 first_tile;
 u32 prefetch_enable;
 u32 prefetch_threshold;
 u32 shift_h;
 u32 range_h;
 u32 cache_all;
 u32 end_addr;
 u32 trace_end_addr;//end addr for simulation trace file
 /*only for cache write*/
 u32 line_size;//the valid width of the input frame/reference frame
 u32 line_stride;//equal to line_size aligment to 1<< LOG2_ALIGNMENT then >>4 
 u32 line_cnt;//the valid height of the input frame/reference frame
 u32 stripe_e;
 u32 pad_e;
 u32 block_e;
 u32 rfc_e;
 u32 max_h;
 u32 ln_cnt_start;
 u32 ln_cnt_mid;
 u32 ln_cnt_end;
 u32 ln_cnt_step;
 u32 tile_id;
 u32 tile_num;
 u32 tile_by_tile;
 u32 width;
 u32 height;
 u32 num_tile_cols;
 u32 num_tile_rows;
 u32 tile_size[2048];
 FILE* file_fid;
 u32 hw_dec_pic_count;
 u32 stream_buffer_id;
 u32 dec_mode;
 u32 hw_id;
 u32 pp_buffer;
 u32 trace_start_pic;
 u32 cache_version;
 u32 ppu_index;
 u32 ppu_sub_index;
} CWLChannelConf_t;


i32 EnableCacheChannel(void **dev,u32 *channel, CWLChannelConf_t *cfg,client_type client,cache_dir dir);
i32 EnableCacheWork(void *dev);
i32 EnableCacheWorkDumpRegs(void *dev, cache_dir dir,
                            u32 *cache_regs, u32 *cache_reg_size,
                            u32 *shaper_regs, u32 *shaper_reg_size);
i32 SetCacheExpAddr(void *dev,u64 start_addr,u64 end_addr);
i32 GetCacheTimeoutSt(void *dev);
i32 DisableCacheChannelALL(void **dev,cache_dir dir);
i32 printInfo(void *dev, CWLChannelConf_t *cfg);
u32 ReadCacheVersion();
#ifdef __cplusplus
}
#endif

#endif
