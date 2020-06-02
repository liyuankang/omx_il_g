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

#ifndef VPU_DWL_DPI_H
#define VPU_DWL_DPI_H
#include "svdpi.h"
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define SWAP_U32(w) (((w) << 24) | (((w) & 0xFF00) << 8) | \
                     ((w) >> 24) | (((w) & 0xFF0000) >> 8))
#ifdef USE_64BIT_ENV
#define MAKE_ADDR(msb, lsb) ((addr_t)(lsb)+(((addr_t)(msb))<<32))
#else
#define MAKE_ADDR(msb, lsb) ((addr_t)(lsb))
#endif
#include "base_type.h"
#include "commonvp9.h"

#define LUM_CB_WIDTH 8
#define LUM_CB_HEIGHT 8

#define CH_CB_WIDTH 8
#define CH_CB_HEIGHT 4

#define LUM_CB_GROUP_NUM 1
#define CH_CB_GROUP_NUM 2   //must be 2x due to cb and cr together
#define ONCE_WRITE_CBS_NUM 2

#define CB_BIT_ALIGN 8
#define CBS_BIT_ALIGN 8

#define LUM_CBS_WIDTH (LUM_CB_WIDTH * LUM_CB_GROUP_NUM) //8
#define CH_CBS_WIDTH (CH_CB_WIDTH * CH_CB_GROUP_NUM)  //16

#define TRACING_BUS_WIDTH 16

#ifdef SUPPORT_DEC400
#define gcregAHBDECWriteConfig 			0x980
#define gcregAHBDECWriteBufferBase		0xD80
#define gcregAHBDECWriteCacheBase		0x1180
#define gcregAHBDECControl			    0x800
#define gcregAHBDECIntrAcknowledge		0x818
#define gcregAHBDECIntrEnbl			    0x80C
#define gcregAHBDECControlEx			0x804
#define gcregAHBDECWriteExConfig		0xA00
#define gcregAHBDECWriteBufferEnd		0xE80
#define gcregAHBDECWriteBufferBaseEx    0xE00
#define gcregAHBDECWriteCacheBaseEx     0x1200
#define gcregAHBDECWriteBufferEndEx     0xF00
#define gcregAHBDECIntrAcknowledgeEx    0x81C
#define gcregAHBDECIntrEnblEx           0x810
#define gcregAHBDECControlEx2           0x808
#define gcregAHBDECStatus 			    0x878
#define gcregAHBDECIntrEnblEx2          0x814
#endif

/*------------------------------------------------------------------------------
    HW register operatoin, read or write
------------------------------------------------------------------------------*/
extern void dpi_l2_apb_op(int addr, int data, int rw);
extern void dpi_apb_op(int addr, int data, int rw);
#ifdef SUPPORT_DEC400
extern void dpi_dec400_apb_op(int addr, int data, int rw);
#endif

/*------------------------------------------------------------------------------
    Update all HW Ext Buffer Base Address
------------------------------------------------------------------------------*/
void dpi_l2_update_buf_base(u32 *reg_base);

/*------------------------------------------------------------------------------
    from c side display info to transcript file
------------------------------------------------------------------------------*/
extern void dpi_display_info(const char* info);

/*------------------------------------------------------------------------------
    store frame info to TB, such as total pic num and MBs
------------------------------------------------------------------------------*/
extern void dpi_store_frame_info(int data, const char* buf_name);

/*------------------------------------------------------------------------------
    Terminate simulation.
------------------------------------------------------------------------------*/
extern void dpi_hs_status(int status);

/*------------------------------------------------------------------------------
    Terminate simulation.
------------------------------------------------------------------------------*/
extern void dpi_hs_status_c1(int status);


/*------------------------------------------------------------------------------
    HW TB external mem read
------------------------------------------------------------------------------*/
extern void dpi_mem_rd(long long int addr, int* data);

/*------------------------------------------------------------------------------
    HW TB external mem check
------------------------------------------------------------------------------*/
extern void dpi_mem_check(long long int addr, int data, const char* buf_name);

/*------------------------------------------------------------------------------
    HW TB external mem write
------------------------------------------------------------------------------*/
extern void dpi_mem_flush(long long int addr, int size);

/*------------------------------------------------------------------------------
    HW TB external mem write
------------------------------------------------------------------------------*/
extern void dpi_mem_wr(long long int addr, int data);

/*------------------------------------------------------------------------------
    Load HW input stream buffer.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_stream(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load qtable.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_qtable(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load tiles info.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_tiles(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load vp9 probtabs.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_probtabs(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load segment in data.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_seg_in(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load input scalelist.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_scalelist(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    load avs2 alftable.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_alftable(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
    Set HW swregs.
------------------------------------------------------------------------------*/
void dpi_mc_load_input_swreg(u32 *reg_base,i32 core_id);

/*------------------------------------------------------------------------------
     
    CHECK HW output recon buffer.
------------------------------------------------------------------------------*/
void dpi_mc_check_recon_buf(u32 * reg_base, const char* op);

/*------------------------------------------------------------------------------
     
    CHECK PPUs output buffer.
------------------------------------------------------------------------------*/

void dpi_mc_check_pp_buf(u32 * reg_base, int i, const char * op);

/*------------------------------------------------------------------------------
    wait HW interrupt, and also do the ISR job.
------------------------------------------------------------------------------*/
extern void dpi_wait_int(int data);

/*------------------------------------------------------------------------------
    wait HW interrupt, do not do the ISR job.
------------------------------------------------------------------------------*/
#ifdef SUPPORT_MULTI_CORE
extern void dpi_wait_mc_int(int* data);
#endif

#ifdef SUPPORT_DEC400

void dpi_DWLDec400DisableAll(const void *instance, i32 core_id);
void dpi_DWLDecF1Configure(u32 * reg_base,const void *instance, i32 core_id);
int  dpi_DWLDecF1Fuse(const void *instance, i32 core_id);
#endif

#endif /* VPU_DWL_DPI_H */
