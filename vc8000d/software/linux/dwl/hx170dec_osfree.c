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
#include <stdint.h>

#include "hantrodec_osfree.h"
#include "dwl_defs.h"
#include "dwlthread.h"
#include "dwl_syncobj_osfree.h"
#ifdef _SYSTEM_MODEL_
#include "asic.h"
#define SYSMODEL_MAX_CORES 1

u32 g_sysmodel_num_cores = SYSMODEL_MAX_CORES;
void *g_sysmodel_core[MAX_ASIC_CORES];
u32 *g_sysmodel_reg_base[MAX_ASIC_CORES];
#endif

#define HXDEC_MAX_CORES                 4

#define HANTRO_DEC_ORG_REGS             60
#define HANTRO_PP_ORG_REGS              41

#define HANTRO_DEC_EXT_REGS             27
#define HANTRO_PP_EXT_REGS              9

#define HANTRO_DEC_TOTAL_REGS           (HANTRO_DEC_ORG_REGS + HANTRO_DEC_EXT_REGS)
#define HANTRO_PP_TOTAL_REGS            (HANTRO_PP_ORG_REGS + HANTRO_PP_EXT_REGS)
#define HANTRO_TOTAL_REGS               437

#define HANTRO_DEC_ORG_FIRST_REG        0
#define HANTRO_DEC_ORG_LAST_REG         59
#define HANTRO_DEC_EXT_FIRST_REG        119
#define HANTRO_DEC_EXT_LAST_REG         145
#define HANTRO_DEC_EXT_G1V8_FIRST_REG   155
#define HANTRO_DEC_EXT_G1V8_LAST_REG    319
#define HANTRO_DEC_UNIFIED_PP_FIRST_REG  320
#define HANTRO_DEC_UNIFIED_PP_LAST_REG   336

#define HANTRO_PP_ORG_FIRST_REG         60
#define HANTRO_PP_ORG_LAST_REG          100
#define HANTRO_PP_EXT_FIRST_REG         146
#define HANTRO_PP_EXT_LAST_REG          154

/*hantro G2 reg config*/
#define HANTRO_G2_DEC_REGS              337 /*G2 total regs*/
#define HANTRO_G2_DEC_FIRST_REG         0
#define HANTRO_G2_DEC_LAST_REG          HANTRO_G2_DEC_REGS-1

/* hantro VC8000D reg config */
#define HANTRO_VC8000D_REGS             437 /*VC8000D total regs*/
#define HANTRO_VC8000D_FIRST_REG        0
#define HANTRO_VC8000D_LAST_REG         HANTRO_VC8000D_REGS-1

/* Logic module base address */
#define SOCLE_LOGIC_0_BASE              0x20000000
#define SOCLE_LOGIC_1_BASE              0x21000000

#define VEXPRESS_LOGIC_0_BASE           0xFC010000
#define VEXPRESS_LOGIC_1_BASE           0xFC020000

/* Logic module IRQs */
#define HXDEC_NO_IRQ                    -1
#define VP_PB_INT_LT                    30
#define SOCLE_INT                       36

/* module defaults */
#define DEC_IO_BASE             SOCLE_LOGIC0_BASE

#define DEC_IO_SIZE             ((HANTRO_TOTAL_REGS) * 4) /* bytes */

#define DEC_IRQ                 HXDEC_NO_IRQ

static av_unused const int DecHwId[] = {
  0x8190,
  0x8170,
  0x9170,
  0x9190,
  0x6731
};

unsigned long base_port = -1;

unsigned long multicorebase[HXDEC_MAX_CORES] = {
  SOCLE_LOGIC_0_BASE,
  SOCLE_LOGIC_1_BASE,
  -1,
  -1
};

int irq = DEC_IRQ;
int elements = 0;
int hantrodec_inited = 0;

static int hantrodec_major = 0; /* dynamic allocation */

/* here's all the must remember stuff */
typedef struct {
  char *buffer;
  unsigned int iosize;
  volatile u8 *hwregs[HXDEC_MAX_CORES];
  int irq;
  int cores;
} hantrodec_t;

static hantrodec_t hantrodec_data; /* dynamic allocation? */

static int ReserveIO(void);
static void ReleaseIO(void);

static void ResetAsic(hantrodec_t * dev);

#ifdef HANTRODEC_DEBUG
static void dump_regs(hantrodec_t *dev);
#endif

/* Interrupt */
static void IntEnableIRQ(u32 irq);
static void IntDisableIRQ(u32 irq);
static void IntClearIRQStatus(u32 irq, u32 type);
static u32 IntGetIRQStatus(u32 irq);
static int RegisterIRQ(i32 i, IRQHandler isr, i32 flag, const char* name, void* data);

/* IRQ handler */
static irqreturn_t hantrodec_isr(int irq, void *dev_id);

static u32 dec_regs[HXDEC_MAX_CORES][512];
Semaphore dec_core_sem;
Semaphore pp_core_sem;

static int dec_irq = 0;
static int pp_irq = 0;

atomic_t irq_rx = ATOMIC_INIT(0);
atomic_t irq_tx = ATOMIC_INIT(0);

static /*struct file* */ int dec_owner[HXDEC_MAX_CORES];
static /*struct file* */ int pp_owner[HXDEC_MAX_CORES];
#define NULL_OWNER  (0)

/*
spinlock_t owner_lock = SPIN_LOCK_UNLOCKED;

DECLARE_WAIT_QUEUE_HEAD(dec_wait_queue);
DECLARE_WAIT_QUEUE_HEAD(pp_wait_queue);

DECLARE_WAIT_QUEUE_HEAD(hw_queue);
*/
#define DWL_CLIENT_TYPE_H264_DEC         1U
#define DWL_CLIENT_TYPE_MPEG4_DEC        2U
#define DWL_CLIENT_TYPE_JPEG_DEC         3U
#define DWL_CLIENT_TYPE_PP               4U
#define DWL_CLIENT_TYPE_VC1_DEC          5U
#define DWL_CLIENT_TYPE_MPEG2_DEC        6U
#define DWL_CLIENT_TYPE_VP6_DEC          7U
#define DWL_CLIENT_TYPE_AVS_DEC          8U
#define DWL_CLIENT_TYPE_RV_DEC           9U
#define DWL_CLIENT_TYPE_VP8_DEC          10U

static u32 cfg[HXDEC_MAX_CORES];

/*--------------------------------------------------------*/
static inline u32 ioread32(volatile void* addr) {
  return *(volatile u32*)(addr);
}

static inline void iowrite32(u32 val,volatile void *addr) {
  *(volatile u32*)addr = val;
}

static inline u16 ioread16(volatile void* addr) {
  return *(volatile u16*)(addr);
}

static inline void iowrite16(u16 val,volatile void *addr) {
  *(volatile u16*)addr = val;
}

static inline u8 ioread8(volatile void* addr) {
  return *(volatile u8*)(addr);
}

static inline void iowrite8(u8 val,volatile void *addr) {
  *(volatile u8*)addr = val;
}

static inline u32 readl(volatile void* addr) {
  return *(volatile u32*)(addr);
}

static inline void writel(unsigned int v, volatile void *addr) {
  *(volatile unsigned int *)addr = /*cpu_to_le32*/(v);
}

#define read_mreg32(addr) ioread32((void*)(addr))
#define write_mreg32(addr,val) iowrite32(val, (void*)(addr))
#define read_mreg16(addr) ioread16((void*)addr)
#define write_mreg16(addr,val) iowrite16(val, (void*)(addr))
#define read_mreg8(addr) ioread8((void*)addr)
#define write_mreg8(addr,val) iowrite8(val, (void*)(addr))
/*--------------------------------------------------------*/

static void ReadCoreConfig(hantrodec_t *dev) {
  int c = 0;
  u32 tmp = 0;
  memset(cfg, 0, sizeof(cfg));
#ifdef _SYSTEM_MODEL_
  DWLHwConfig sysmodel_cfg;
  DWLReadAsicConfig(&sysmodel_cfg, DWL_CLIENT_TYPE_H264_DEC);
  for(c = 0; c < dev->cores; c++) {
    /* Decoder configuration */
    tmp = sysmodel_cfg.h264_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has H264\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

    tmp = sysmodel_cfg.jpeg_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has JPEG\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

    tmp = sysmodel_cfg.mpeg4_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG4\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

    tmp = sysmodel_cfg.vc1_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VC1\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC: 0;

    tmp = sysmodel_cfg.mpeg2_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG2\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

    tmp = sysmodel_cfg.vp6_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP6\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

    if(sysmodel_cfg.vp8_support)
      printk(KERN_INFO "hantrodec: core[%d] has VP8\n", c);
    if(sysmodel_cfg.vp7_support)
      printk(KERN_INFO "hantrodec: core[%d] has VP7\n", c);
    if(sysmodel_cfg.webp_support)
      printk(KERN_INFO "hantrodec: core[%d] has WebP\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC : 0;

    tmp = sysmodel_cfg.avs_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AVS\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC: 0;

    tmp = sysmodel_cfg.rv_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has RV\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

    /* Post-processor configuration */
    tmp = sysmodel_cfg.pp_support;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has PP\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;
  }

#else  
  u32 reg = 0, mask = 0;
  for(c = 0; c < dev->cores; c++) {
    /* Decoder configuration */
    reg = ioread32(dev->hwregs[c] + HANTRODEC_SYNTH_CFG * 4);

    tmp = (reg >> DWL_H264_E) & 0x3U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has H264\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

    tmp = (reg >> DWL_JPEG_E) & 0x01U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has JPEG\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

    tmp = (reg >> DWL_MPEG4_E) & 0x3U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG4\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

    tmp = (reg >> DWL_VC1_E) & 0x3U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VC1\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC: 0;

    tmp = (reg >> DWL_MPEG2_E) & 0x01U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG2\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

    tmp = (reg >> DWL_VP6_E) & 0x01U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP6\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

    reg = ioread32(dev->hwregs[c] + HANTRODEC_SYNTH_CFG_2 * 4);

    /* VP7 and WEBP is part of VP8 */
    mask =  (1 << DWL_VP8_E) | (1 << DWL_VP7_E) | (1 << DWL_WEBP_E);
    tmp = (reg & mask);
    if(tmp & (1 << DWL_VP8_E))
      printk(KERN_INFO "hantrodec: core[%d] has VP8\n", c);
    if(tmp & (1 << DWL_VP7_E))
      printk(KERN_INFO "hantrodec: core[%d] has VP7\n", c);
    if(tmp & (1 << DWL_WEBP_E))
      printk(KERN_INFO "hantrodec: core[%d] has WebP\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC : 0;

    tmp = (reg >> DWL_AVS_E) & 0x01U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AVS\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC: 0;

    tmp = (reg >> DWL_RV_E) & 0x03U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has RV\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

    /* Post-processor configuration */
    reg = ioread32(dev->hwregs[c] + HANTROPP_SYNTH_CFG * 4);

    tmp = (reg >> DWL_G1_PP_E) & 0x01U;
    if(tmp) printk(KERN_INFO "hantrodec: core[%d] has PP\n", c);
    cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;
  }
#endif
}

static int CoreHasFormat(const u32 *cfg, int core, u32 format) {
  return (cfg[core] & (1 << format)) ? 1 : 0;
}

int GetDecCore(long core, hantrodec_t *dev, /*struct file* */ int filp) {
  int success = 0;
  unsigned long flags = 0;

  spin_lock_irqsave(&owner_lock, flags);
  if(dec_owner[core] == NULL_OWNER ) {
    dec_owner[core] = filp;
    success = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  (void)flags;
  return success;
}

int GetDecCoreAny(long *core, hantrodec_t *dev, /*struct file* */ int filp,
                  unsigned long format) {
  int success = 0;
  long c;

  *core = -1;

  for(c = 0; c < dev->cores; c++) {
    /* a free core that has format */
    if(CoreHasFormat(cfg, c, format) && GetDecCore(c, dev, filp)) {
      success = 1;
      *core = c;
      break;
    }
  }

  return success;
}

long ReserveDecoder(hantrodec_t *dev, /*struct file* */ int filp, unsigned long format) {
  long core = -1;

  /* reserve a core */
  if (down_interruptible(&dec_core_sem))
    return -ERESTARTSYS;

  /* lock a core that has specific format*/
  if(wait_event_interruptible(hw_queue,
                              GetDecCoreAny(&core, dev, filp, format) != 0 ))
    return -ERESTARTSYS;

  return core;
}

void ReleaseDecoder(hantrodec_t *dev, long core) {
  u32 status;
  unsigned long flags = 0;

  status = ioread32(dev->hwregs[core] + HANTRO_IRQ_STAT_DEC_OFF);

  /* make sure HW is disabled */
  if(status & HANTRO_DEC_E) {
    printk(KERN_INFO "hantrodec: DEC[%li] still enabled -> reset\n", core);

    /* abort decoder */
    status |= HANTRO_DEC_ABORT | HANTRO_DEC_IRQ_DISABLE;
    iowrite32(status, dev->hwregs[core] + HANTRO_IRQ_STAT_DEC_OFF);
  }

  spin_lock_irqsave(&owner_lock, flags);

  dec_owner[core] = NULL_OWNER;

  spin_unlock_irqrestore(&owner_lock, flags);

  up(&dec_core_sem);

  wake_up_interruptible_all(&hw_queue);
  
  (void)flags;
}

long ReservePostProcessor(hantrodec_t *dev, /*struct file* */int filp) {
  unsigned long flags = 0;

  long core = 0;

  /* single core PP only */
  if (down_interruptible(&pp_core_sem))
    return -ERESTARTSYS;

  spin_lock_irqsave(&owner_lock, flags);

  pp_owner[core] = filp;

  spin_unlock_irqrestore(&owner_lock, flags);

  (void)flags;
  return core;
}

void ReleasePostProcessor(hantrodec_t *dev, long core) {
  unsigned long flags = 0;

  u32 status = ioread32(dev->hwregs[core] + HANTRO_IRQ_STAT_PP_OFF);

  /* make sure HW is disabled */
  if(status & HANTRO_PP_E) {
    printk(KERN_INFO "hantrodec: PP[%li] still enabled -> reset\n", core);

    /* disable IRQ */
    status |= HANTRO_PP_IRQ_DISABLE;

    /* disable postprocessor */
    status &= (~HANTRO_PP_E);
    iowrite32(0x10, dev->hwregs[core] + HANTRO_IRQ_STAT_PP_OFF);
  }

  spin_lock_irqsave(&owner_lock, flags);

  pp_owner[core] = NULL_OWNER;

  spin_unlock_irqrestore(&owner_lock, flags);

  up(&pp_core_sem);
  (void)flags;
}

long ReserveDecPp(hantrodec_t *dev, /*struct file* */int filp, unsigned long format) {
  /* reserve core 0, DEC+PP for pipeline */
  unsigned long flags = 0;

  long core = 0;

  /* check that core has the requested dec format */
  if(!CoreHasFormat(cfg, core, format))
    return -EFAULT;

  /* check that core has PP */
  if(!CoreHasFormat(cfg, core, DWL_CLIENT_TYPE_PP))
    return -EFAULT;

  /* reserve a core */
  if (down_interruptible(&dec_core_sem))
    return -ERESTARTSYS;

  /* wait until the core is available */
  if(wait_event_interruptible(hw_queue,
                              GetDecCore(core, dev, filp) != 0)) {
    up(&dec_core_sem);
    return -ERESTARTSYS;
  }

  if (down_interruptible(&pp_core_sem)) {
    ReleaseDecoder(dev, core);
    return -ERESTARTSYS;
  }

  spin_lock_irqsave(&owner_lock, flags);
  pp_owner[core] = filp;
  spin_unlock_irqrestore(&owner_lock, flags);

  (void)flags;
  return core;
}

long DecFlushRegs(hantrodec_t *dev, struct core_desc *core) {
  long ret = 0, i = 0;

  u32 id = core->id;

  /* copy original dec regs to kernal space*/
  ret = copy_from_user(dec_regs[id], core->regs, HANTRO_TOTAL_REGS*4);
#if 0
#ifdef USE_64BIT_ENV
  /* copy extended dec regs to kernal space*/
  ret = copy_from_user(dec_regs[id] + HANTRO_DEC_EXT_FIRST_REG,
                       core->regs + HANTRO_DEC_EXT_FIRST_REG,
                       HANTRO_DEC_EXT_REGS*4);
#endif
#endif
  if (ret) {
    PDEBUG("copy_from_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  /* write dec regs but the status reg[1] to hardware */
  /* both original and extended regs need to be written */
#if 1
  for(i = 2; i <= HANTRO_VC8000D_LAST_REG; i++) {
    iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id] + i*4));
  }
#else
  for(i = 2; i <= HANTRO_DEC_ORG_LAST_REG; i++)
    iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
  if (sizeof(void *) == 8) {
    for(i = HANTRO_DEC_EXT_FIRST_REG; i <= HANTRO_DEC_EXT_LAST_REG; i++)
      iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
  }
  for(i = HANTRO_DEC_EXT_G1V8_FIRST_REG; i <= HANTRO_DEC_EXT_G1V8_LAST_REG; i++)
    iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
  for (i = HANTRO_DEC_UNIFIED_PP_FIRST_REG; i <=  HANTRO_DEC_UNIFIED_PP_LAST_REG; i++)
    iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
#endif
  /* write the status register, which may start the decoder */
  iowrite32(dec_regs[id][1], dev->hwregs[id] + 4);

  PDEBUG("flushed registers on core %d\n", id);
  return 0;
}

long DecWriteRegs(hantrodec_t *dev, struct core_desc *core)
{
  long ret = 0, i;
  u32 id = core->id;
  i = core->reg_id;

  ret = copy_from_user(dec_regs[id] + core->reg_id, core->regs + core->reg_id, 4);

  if (ret) {
    PDEBUG("copy_from_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id] + i*4));
  return 0;
}

long DecReadRegs(hantrodec_t *dev, struct core_desc *core)
{
  long ret, i;
  u32 id = core->id;
  i = core->reg_id;

  /* user has to know exactly what they are asking for */
  //if(core->size != (HANTRO_VC8000D_REGS * 4))
  //  return -EFAULT;

  /* read specific registers from hardware */
  i = core->reg_id;
  dec_regs[id][i] = ioread32((void*)(dev->hwregs[id] + i*4));

  /* put registers to user space*/
  ret = copy_to_user(core->regs + core->reg_id, dec_regs[id] + core->reg_id, 4);
  if (ret) {
    PDEBUG("copy_to_user failed, returned %li\n", ret);
    return -EFAULT;
  }
  return 0;
}

long DecRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  long ret, i;
  u32 id = core->id;

  /* read all registers from hardware */
  /* both original and extended regs need to be read */
  for(i = 0; i <= HANTRO_VC8000D_LAST_REG; i++)
    dec_regs[id][i] = ioread32(dev->hwregs[id] + i*4);
  /* put registers to user space*/
  /* put original registers to user space*/
  ret = copy_to_user(core->regs, dec_regs[id], HANTRO_G2_DEC_REGS*4);

  if (ret) {
    PDEBUG("copy_to_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  return 0;
}

static int CheckDecIrq(hantrodec_t *dev, int id) {
  unsigned long flags = 0;
  int rdy = 0;

  const u32 irq_mask = (1 << id);

  spin_lock_irqsave(&owner_lock, flags);

  if(dec_irq & irq_mask) {
    /* reset the wait condition(s) */
    dec_irq &= ~irq_mask;
    rdy = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  (void)flags;
  return rdy;
}

long WaitDecReadyAndRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  u32 id = core->id;

  PDEBUG("wait_event_interruptible DEC[%d]\n", id);

  if(wait_event_interruptible(dec_wait_queue, CheckDecIrq(dev, id))) {
    PDEBUG("DEC[%d]  wait_event_interruptible interrupted\n", id);
    return -ERESTARTSYS;
  }

  atomic_inc(&irq_tx);

  /* refresh registers */
  return DecRefreshRegs(dev, core);
}

long PPFlushRegs(hantrodec_t *dev, struct core_desc *core) {
  long ret = 0;
  u32 id = core->id;
  u32 i;

  /* copy original dec regs to kernal space*/
  ret = copy_from_user(dec_regs[id] + HANTRO_PP_ORG_FIRST_REG,
                       core->regs + HANTRO_PP_ORG_FIRST_REG,
                       HANTRO_PP_ORG_REGS*4);
  if (sizeof(void *) == 8) {
    /* copy extended dec regs to kernal space*/
    ret = copy_from_user(dec_regs[id] + HANTRO_PP_EXT_FIRST_REG,
                       core->regs + HANTRO_PP_EXT_FIRST_REG,
                       HANTRO_PP_EXT_REGS*4);
  }
  if (ret) {
    PDEBUG("copy_from_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  /* write all regs but the status reg[1] to hardware */
  /* both original and extended regs need to be written */
  for(i = HANTRO_PP_ORG_FIRST_REG + 1; i <= HANTRO_PP_ORG_LAST_REG; i++)
    iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
  if (sizeof(void *) == 8) {
    for(i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
      iowrite32(dec_regs[id][i], dev->hwregs[id] + i*4);
  }
  /* write the stat reg, which may start the PP */
  iowrite32(dec_regs[id][HANTRO_PP_ORG_FIRST_REG],
            dev->hwregs[id] + HANTRO_PP_ORG_FIRST_REG * 4);

  return 0;
}

long PPRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  long i = 0, ret = 0;
  u32 id = core->id;
  if (sizeof(void *) == 8) {
    /* user has to know exactly what they are asking for */
    if(core->size != (HANTRO_PP_TOTAL_REGS * 4))
      return -EFAULT;
  } else {
    /* user has to know exactly what they are asking for */
    if(core->size != (HANTRO_PP_ORG_REGS * 4))
      return -EFAULT;
  }

  /* read all registers from hardware */
  /* both original and extended regs need to be read */
  for(i = HANTRO_PP_ORG_FIRST_REG; i <= HANTRO_PP_ORG_LAST_REG; i++)
    dec_regs[id][i] = ioread32(dev->hwregs[id] + i*4);
  if (sizeof(void *) == 8) {
    for(i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
      dec_regs[id][i] = ioread32(dev->hwregs[id] + i*4);
  }
  /* put registers to user space*/
  /* put original registers to user space*/
  ret = copy_to_user(core->regs + HANTRO_PP_ORG_FIRST_REG,
                     dec_regs[id] + HANTRO_PP_ORG_FIRST_REG,
                     HANTRO_PP_ORG_REGS*4);
  if (sizeof(void *) == 8) {
    /* put extended registers to user space*/
    ret = copy_to_user(core->regs + HANTRO_PP_EXT_FIRST_REG,
                       dec_regs[id] + HANTRO_PP_EXT_FIRST_REG,
                       HANTRO_PP_EXT_REGS * 4);
  }
  if (ret) {
    PDEBUG("copy_to_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  return 0;
}

static int CheckPPIrq(hantrodec_t *dev, int id) {
  unsigned long flags = 0;
  int rdy = 0;

  const u32 irq_mask = (1 << id);

  spin_lock_irqsave(&owner_lock, flags);

  if(pp_irq & irq_mask) {
    /* reset the wait condition(s) */
    pp_irq &= ~irq_mask;
    rdy = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  (void)flags;
  return rdy;
}

long WaitPPReadyAndRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  u32 id = core->id;

  PDEBUG("wait_event_interruptible PP[%d]\n", id);

  if(wait_event_interruptible(pp_wait_queue, CheckPPIrq(dev, id))) {
    PDEBUG("PP[%d]  wait_event_interruptible interrupted\n", id);
    return -ERESTARTSYS;
  }

  atomic_inc(&irq_tx);

  /* refresh registers */
  return PPRefreshRegs(dev, core);
}

static int CheckCoreIrq(hantrodec_t *dev, const /*struct file * */ int filp, int *id) {
  unsigned long flags = 0;
  int rdy = 0, n = 0;

  do {
    u32 irq_mask = (1 << n);

    spin_lock_irqsave(&owner_lock, flags);

    if(dec_irq & irq_mask) {
      if (dec_owner[n] == filp) {
        /* we have an IRQ for our client */

        /* reset the wait condition(s) */
        dec_irq &= ~irq_mask;

        /* signal ready core no. for our client */
        *id = n;

        rdy = 1;

        break;
      } else if(dec_owner[n] == NULL_OWNER) {
        /* zombie IRQ */
        printk(KERN_INFO "IRQ on core[%d], but no owner!!!\n", n);

        /* reset the wait condition(s) */
        dec_irq &= ~irq_mask;
      }
    }

    spin_unlock_irqrestore(&owner_lock, flags);

    n++; /* next core */
  } while(n < dev->cores);

  (void)flags;
  return rdy;
}

long WaitCoreReady(hantrodec_t *dev, const /*struct file * */ int filp, int *id) {
  PDEBUG("wait_event_interruptible CORE\n", "");

  if(wait_event_interruptible(dec_wait_queue, CheckCoreIrq(dev, filp, id))) {
    PDEBUG("CORE  wait_event_interruptible interrupted\n", "");
    return -ERESTARTSYS;
  }

  atomic_inc(&irq_tx);

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_ioctl
 Description     : communication method to/from the user space

 Return type     : long
------------------------------------------------------------------------------*/

long hantrodec_ioctl(/*struct file * */ int filp, unsigned int cmd,
                                       unsigned long arg) {
  int err = 0;
  long tmp = 0;

#ifdef HW_PERFORMANCE
  struct timeval *end_time_arg;
#endif

  PDEBUG("ioctl cmd 0x%08x\n", cmd);
  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != HANTRODEC_IOC_MAGIC)
    return -ENOTTY;
  if (_IOC_NR(cmd) > HANTRODEC_IOC_MAXNR)
    return -ENOTTY;

  /*
   * the direction is a bitmask, and VERIFY_WRITE catches R/W
   * transfers. `Type' is user-oriented, while
   * access_ok is kernel-oriented, so the concept of "read" and
   * "write" is reversed
   */
  if (_IOC_DIR(cmd) & _IOC_READ)
    err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));

  if (err)
    return -EFAULT;

  switch (cmd) {
  case HANTRODEC_IOC_CLI:
    disable_irq(hantrodec_data.irq);
    break;
  case HANTRODEC_IOC_STI:
    enable_irq(hantrodec_data.irq);
    break;
  case HANTRODEC_IOCGHWOFFSET:
    __put_user(multicorebase[0], (unsigned long *) arg);
    break;
  case HANTRODEC_IOCGHWIOSIZE:
    __put_user(hantrodec_data.iosize, (unsigned int *) arg);
    break;
  case HANTRODEC_IOC_MC_OFFSETS: {
    tmp = copy_to_user((unsigned long *) arg, multicorebase, sizeof(multicorebase));
    if (err) {
      PDEBUG("copy_to_user failed, returned %li\n", tmp);
      return -EFAULT;
    }
    break;
  }
  case HANTRODEC_IOC_MC_CORES:
    __put_user(hantrodec_data.cores, (unsigned int *) arg);
    break;
  case HANTRODEC_IOCS_DEC_PUSH_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    DecFlushRegs(&hantrodec_data, &core);
    break;
  }
  case HANTRODEC_IOCS_PP_PUSH_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    PPFlushRegs(&hantrodec_data, &core);
    break;
  }
  case HANTRODEC_IOCS_DEC_PULL_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return DecRefreshRegs(&hantrodec_data, &core);
  }
  case HANTRODEC_IOCS_DEC_READ_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return DecReadRegs(&hantrodec_data, &core);
  }
  case HANTRODEC_IOCS_PP_PULL_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return PPRefreshRegs(&hantrodec_data, &core);
  }
  case HANTRODEC_IOCH_DEC_RESERVE: {
    PDEBUG("Reserve DEC core, format = %li\n", arg);
    return ReserveDecoder(&hantrodec_data, filp, arg);
  }
  case HANTRODEC_IOCT_DEC_RELEASE: {
    if(arg >= hantrodec_data.cores || dec_owner[arg] != filp) {
      PDEBUG("bogus DEC release, core = %li\n", arg);
      return -EFAULT;
    }

    PDEBUG("Release DEC, core = %li\n", arg);

    ReleaseDecoder(&hantrodec_data, arg);

    break;
  }
  case HANTRODEC_IOCQ_PP_RESERVE:
    return ReservePostProcessor(&hantrodec_data, filp);
  case HANTRODEC_IOCT_PP_RELEASE: {
    if(arg != 0 || pp_owner[arg] != filp) {
      PDEBUG("bogus PP release %li\n", arg);
      return -EFAULT;
    }

    ReleasePostProcessor(&hantrodec_data, arg);

    break;
  }
  case HANTRODEC_IOCX_DEC_WAIT: {
    struct core_desc core;

    /* get registers from user space */
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return WaitDecReadyAndRefreshRegs(&hantrodec_data, &core);
  }
  case HANTRODEC_IOCX_PP_WAIT: {
    struct core_desc core;

    /* get registers from user space */
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return WaitPPReadyAndRefreshRegs(&hantrodec_data, &core);
  }
  case HANTRODEC_IOCG_CORE_WAIT: {
    int id;
    tmp = WaitCoreReady(&hantrodec_data, filp, &id);
    __put_user(id, (int *) arg);
    return tmp;
  }
  case HANTRODEC_IOX_ASIC_ID: {
    u32 id;
    __get_user(id, (u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }
    id = ioread32(hantrodec_data.hwregs[id]);
    __put_user(id, (u32 *) arg);
  }
  case HANTRODEC_DEBUG_STATUS: {
    printk(KERN_INFO "hantrodec: dec_irq     = 0x%08x \n", dec_irq);
    printk(KERN_INFO "hantrodec: pp_irq      = 0x%08x \n", pp_irq);

    printk(KERN_INFO "hantrodec: IRQs received/sent2user = %d / %d \n",
           atomic_read(&irq_rx), atomic_read(&irq_tx));

    for (tmp = 0; tmp < hantrodec_data.cores; tmp++) {
      printk(KERN_INFO "hantrodec: dec_core[%li] %s\n",
             tmp, dec_owner[tmp] == NULL_OWNER ? "FREE" : "RESERVED");
      printk(KERN_INFO "hantrodec: pp_core[%li]  %s\n",
             tmp, pp_owner[tmp] == NULL_OWNER ? "FREE" : "RESERVED");
    }
  }
  default:
    return -ENOTTY;
  }

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_open
 Description     : open method

 Return type     : int
------------------------------------------------------------------------------*/

static av_unused int hantrodec_open(/*struct inode * */int inode, /*struct file * */ int filp) {
  PDEBUG("dev opened\n", "");
  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_release
 Description     : Release driver

 Return type     : int
------------------------------------------------------------------------------*/

int hantrodec_release(/*struct inode * */int *inode, /*struct file * */ int filp) {
  int n;
  hantrodec_t *dev = &hantrodec_data;

  PDEBUG("closing ...\n", "");

  for(n = 0; n < dev->cores; n++) {
    if(dec_owner[n] == filp) {
      PDEBUG("releasing dec core %i lock\n", n);
      ReleaseDecoder(dev, n);
    }
  }

  for(n = 0; n < 1; n++) {
    if(pp_owner[n] == filp) {
      PDEBUG("releasing pp core %i lock\n", n);
      ReleasePostProcessor(dev, n);
    }
  }

  PDEBUG("closed\n", "");

#ifdef _SYSTEM_MODEL_
  for(n=0; n<dev->cores; n++) {
    AsicHwCoreRelease(g_sysmodel_core[n]);
  }
#endif
  hantrodec_inited = 0;
  return 0;
}

/* VFS methods */
/*
static struct file_operations hantrodec_fops = {
  .owner = THIS_MODULE,
  .open = hantrodec_open,
  .release = hantrodec_release,
  .unlocked_ioctl = hantrodec_ioctl,
  .fasync = NULL
};
*/

/*------------------------------------------------------------------------------
 Function name   : hantrodec_init
 Description     : Initialize the driver

 Return type     : int
------------------------------------------------------------------------------*/

int __init hantrodec_init(void) {
  int result = 0, i = 0;

  if(hantrodec_inited) return 0;

#ifdef _SYSTEM_MODEL_
  /* create system model decoder cores, for simulation only*/
  for(i=0; i<g_sysmodel_num_cores; i++) {
    g_sysmodel_core[i] = (void *)AsicHwCoreInit();
    g_sysmodel_reg_base[i] = AsicHwCoreGetBase(g_sysmodel_core[i]);
  }
#endif

  PDEBUG("module init\n", "");

  printk(KERN_INFO "hantrodec: dec/pp kernel module.\n", "");

#ifdef _SYSTEM_MODEL_
  for(i=0; i<HXDEC_MAX_CORES; i++) {
    if(i < g_sysmodel_num_cores) {
      multicorebase[i] = (unsigned long)g_sysmodel_reg_base[i];
    } else {
      multicorebase[i] = -1;
    }
  }
#endif

  /* If base_port is set at load, use that for single core legacy mode */
  if(base_port != -1) {
    multicorebase[0] = base_port;
    elements = 1;
    printk(KERN_INFO "hantrodec: Init single core at 0x%16lx IRQ=%i\n",
           multicorebase[0], irq);
  } else {
    printk(KERN_INFO "hantrodec: Init multi core[0] at 0x%16lx\n"
           "                     core[1] at 0x%16lx\n"
           "                     core[2] at 0x%16lx\n"
           "                     core[3] at 0x%16lx\n"
           "          IRQ=%i\n",
           multicorebase[0], multicorebase[1],
           multicorebase[2], multicorebase[3],
           irq);
  }

  hantrodec_data.iosize = DEC_IO_SIZE;
  hantrodec_data.irq = irq;

  for(i=0; i< HXDEC_MAX_CORES; i++) {
    hantrodec_data.hwregs[i] = 0;
    /* If user gave less core bases that we have by default,
     * invalidate default bases
     */
    if(elements && i>=elements) {
      multicorebase[i] = -1;
    }
  }

  result = register_chrdev(hantrodec_major, "hantrodec", &hantrodec_fops);
  if(result < 0) {
    printk(KERN_INFO "hantrodec: unable to get major %d\n", hantrodec_major);
    goto err;
  } else if(result != 0) { /* this is for dynamic major */
    hantrodec_major = result;
  }

  result = ReserveIO();
  if(result < 0) {
    goto err;
  }

  memset(dec_owner, 0, sizeof(dec_owner));
  memset(pp_owner, 0, sizeof(pp_owner));

  sema_init(&dec_core_sem, hantrodec_data.cores);
  sema_init(&pp_core_sem, 1);

  /* read configuration fo all cores */
  ReadCoreConfig(&hantrodec_data);

  /* reset hardware */
  ResetAsic(&hantrodec_data);

  /* get the IRQ line */
  if(irq > 0) {
    result = request_irq(irq, hantrodec_isr,
                         /*#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
                                                  SA_INTERRUPT | SA_SHIRQ,
                         #else */
                         IRQF_SHARED,
                         /*#endif*/
                         "hantrodec", (void *) &hantrodec_data);
    if(result != 0) {
      if(result == -EINVAL) {
        printk(KERN_ERR "hantrodec: Bad irq number or handler\n", "");
      } else if(result == -EBUSY) {
        printk(KERN_ERR "hantrodec: IRQ <%d> busy, change your config\n",
               hantrodec_data.irq);
      }

      ReleaseIO();
      goto err;
    }
  } else {
    printk(KERN_INFO "hantrodec: IRQ not in use!\n", "");
  }

  printk(KERN_INFO "hantrodec: module inserted. Major = %d\n", hantrodec_major);

  hantrodec_inited = 1;
  return 0;

err:
  printk(KERN_INFO "hantrodec: module not inserted\n", "");
  unregister_chrdev(hantrodec_major, "hantrodec");
  return result;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_cleanup
 Description     : clean up

 Return type     : int
------------------------------------------------------------------------------*/

void __exit hantrodec_cleanup(void) {
  hantrodec_t *dev = &hantrodec_data;

  /* reset hardware */
  ResetAsic(dev);

  /* free the IRQ */
  if(dev->irq != -1) {
    free_irq(dev->irq, (void *) dev);
  }

  ReleaseIO();

  unregister_chrdev(hantrodec_major, "hantrodec");

  printk(KERN_INFO "hantrodec: module removed\n", "");
  return;
}

/*------------------------------------------------------------------------------
 Function name   : CheckHwId
 Return type     : int
------------------------------------------------------------------------------*/
static int CheckHwId(hantrodec_t * dev) {

#ifdef _SYSTEM_MODEL_
  return 1;

#else
  long int hwid = 0;
  int i = 0;
  size_t num_hw = sizeof(DecHwId) / sizeof(*DecHwId);

  int found = 0;

  for (i = 0; i < dev->cores; i++) {
    if (dev->hwregs[i] != NULL ) {
      hwid = readl(dev->hwregs[i]);
      printk(KERN_INFO "hantrodec: Core %d HW ID=0x%08lx\n", i, hwid);
      hwid = (hwid >> 16) & 0xFFFF; /* product version only */

      while (num_hw--) {
        if (hwid == DecHwId[num_hw]) {
          printk(KERN_INFO "hantrodec: Supported HW found at 0x%08x\n",
                 multicorebase[i]);
          found++;
          break;
        }
      }
      if (!found) {
        printk(KERN_INFO "hantrodec: Unknown HW found at 0x%08x\n",
               multicorebase[i]);
        return 0;
      }
      found = 0;
      num_hw = sizeof(DecHwId) / sizeof(*DecHwId);
    }
  }

  return 1;
#endif
}

/*------------------------------------------------------------------------------
 Function name   : ReserveIO
 Description     : IO reserve

 Return type     : int
------------------------------------------------------------------------------*/
static int ReserveIO(void) {
  int i;

  hantrodec_data.cores = 0;
  for (i = 0; i < HXDEC_MAX_CORES; i++) {
    if (multicorebase[i] != -1) {
      if (!request_mem_region(multicorebase[i], hantrodec_data.iosize,
                              "hantrodec0")) {
        printk(KERN_INFO "hantrodec: failed to reserve HW regs\n", "");
        return -EBUSY;
      }

      hantrodec_data.hwregs[i] = (volatile u8 *) ioremap_nocache(multicorebase[i],
                                hantrodec_data.iosize);

      if (hantrodec_data.hwregs[i] == NULL ) {
        printk(KERN_INFO "hantrodec: failed to ioremap HW regs\n", "");
        ReleaseIO();
        return -EBUSY;
      }
      hantrodec_data.cores++;
    }
  }

  /* check for correct HW */
  if (!CheckHwId(&hantrodec_data)) {
    ReleaseIO();
    return -EBUSY;
  }

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : releaseIO
 Description     : release

 Return type     : void
------------------------------------------------------------------------------*/

static void ReleaseIO(void) {
  int i = 0;
  for (i = 0; i < hantrodec_data.cores; i++) {
    if (hantrodec_data.hwregs[i])
      iounmap((void *) hantrodec_data.hwregs[i]);
    release_mem_region(multicorebase[i], hantrodec_data.iosize);
  }
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_isr
 Description     : interrupt handler

 Return type     : irqreturn_t
------------------------------------------------------------------------------*/
irqreturn_t hantrodec_isr(int irq, void *dev_id) {
  unsigned long flags = 0;
  unsigned int handled = 0;
  int i = 0;
  volatile u8 *hwregs = NULL;

  hantrodec_t *dev = (hantrodec_t *) dev_id;
  u32 irq_status_dec = 0;
  u32 irq_status_pp = 0;

  spin_lock_irqsave(&owner_lock, flags);

  for(i=0; i<dev->cores; i++) {
    volatile u8 *hwregs = dev->hwregs[i];

    /* interrupt status register read */
    irq_status_dec = ioread32(hwregs + HANTRO_IRQ_STAT_DEC_OFF);

    if(irq_status_dec & HANTRO_DEC_IRQ) {
      /* clear dec IRQ */
      irq_status_dec &= (~HANTRO_DEC_IRQ);
      iowrite32(irq_status_dec, hwregs + HANTRO_IRQ_STAT_DEC_OFF);

      PDEBUG("decoder IRQ received! core %d\n", i);

      atomic_inc(&irq_rx);

      dec_irq |= (1 << i);

      wake_up_interruptible_all(&dec_wait_queue);
      handled++;
    }
  }

  /* check PP also */
  hwregs = dev->hwregs[0];
  irq_status_pp = ioread32(hwregs + HANTRO_IRQ_STAT_PP_OFF);
  if(irq_status_pp & HANTRO_PP_IRQ) {
    /* clear pp IRQ */
    irq_status_pp &= (~HANTRO_PP_IRQ);
    iowrite32(irq_status_pp, hwregs + HANTRO_IRQ_STAT_PP_OFF);

    PDEBUG("post-processor IRQ received!\n", "");

    atomic_inc(&irq_rx);

    pp_irq |= 1;

    wake_up_interruptible_all(&pp_wait_queue);
    handled++;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  if(!handled) {
    PDEBUG("IRQ received, but not x170's!\n", "");
  }
  (void)flags;
  return IRQ_RETVAL(handled);
}

/*------------------------------------------------------------------------------
 Function name   : ResetAsic
 Description     : reset asic

 Return type     :
------------------------------------------------------------------------------*/
void ResetAsic(hantrodec_t * dev) {
  int i, j;
  u32 status;

  for (j = 0; j < dev->cores; j++) {
    status = ioread32(dev->hwregs[j] + HANTRO_IRQ_STAT_DEC_OFF);

    if( status & HANTRO_DEC_E) {
      /* abort with IRQ disabled */
      status = HANTRO_DEC_ABORT | HANTRO_DEC_IRQ_DISABLE;
      iowrite32(status, dev->hwregs[j] + HANTRO_IRQ_STAT_DEC_OFF);
    }

    /* reset PP */
    iowrite32(0, dev->hwregs[j] + HANTRO_IRQ_STAT_PP_OFF);

    for (i = 4; i < dev->iosize; i += 4) {
      iowrite32(0, dev->hwregs[j] + i);
    }
  }
}

/*------------------------------------------------------------------------------
 Function name   : dump_regs
 Description     : Dump registers

 Return type     :
------------------------------------------------------------------------------*/
#ifdef HANTRODEC_DEBUG
void dump_regs(hantrodec_t *dev) {
  int i,c;

  PDEBUG("Reg Dump Start\n", "");
  for(c = 0; c < dev->cores; c++) {
    for(i = 0; i < dev->iosize; i += 4*4) {
      PDEBUG("\toffset %04X: %08X  %08X  %08X  %08X\n", i,
             ioread32(dev->hwregs[c] + i),
             ioread32(dev->hwregs[c] + i + 4),
             ioread32(dev->hwregs[c] + i + 16),
             ioread32(dev->hwregs[c] + i + 24));
    }
  }
  PDEBUG("Reg Dump End\n", "");
}
#endif


av_unused void IntEnableIRQ(u32 irq) {
}

av_unused void IntDisableIRQ(u32 irq) {
}

av_unused void IntClearIRQStatus(u32 irq, u32 type) {
}

av_unused u32 IntGetIRQStatus(u32 irq) {
  return 0;
}

av_unused int RegisterIRQ(i32 i, IRQHandler isr, i32 flag, const char* name, void* data) {
  return 0;
}

