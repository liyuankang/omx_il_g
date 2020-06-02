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

#include "hantrodec.h"
#include "dwl_defs.h"

#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/mod_devicetable.h>
#include "subsys.h"
#ifdef SUPPORT_AXIFE
#include "hantro_axife.h"
#endif

/******************* add by wfy *********************/
#define PCI_VENDOR_ID_HANTRO            0x10ee// 0x1ae0//0x16c3
#define PCI_DEVICE_ID_HANTRO_PCI        0x8014//0x001a// 0xabcd

/* Base address DDR register */
#define PCI_DDR_BAR 0

/* Base address got control register */
#define PCI_CONTROL_BAR                 4

/* PCIe hantro driver offset in control register */
#define HANTRO_REG_OFFSET0               0x600000
#define HANTRO_REG_OFFSET1               0x700000

/* TODO(mheikkinen) Implement multicore support. */
struct pci_dev *gDev = NULL;     /* PCI device structure. */
unsigned long gBaseHdwr;         /* PCI base register address (Hardware address) */
unsigned long gBaseDDRHw;        /* PCI base register address (memalloc) */
u32  gBaseLen;                   /* Base register address Length */

#define HXDEC_MAX_CORES MAX_SUBSYS_NUM

/* hantro G1 regs config including dec and pp */
//#define HANTRO_DEC_ORG_REGS             60
//#define HANTRO_PP_ORG_REGS              41

#define HANTRO_DEC_EXT_REGS             27
#define HANTRO_PP_EXT_REGS              9

//#define HANTRO_G1_DEC_TOTAL_REGS        (HANTRO_DEC_ORG_REGS + HANTRO_DEC_EXT_REGS)
#define HANTRO_PP_TOTAL_REGS            (HANTRO_PP_ORG_REGS + HANTRO_PP_EXT_REGS)
#define HANTRO_G1_DEC_REGS              155 /*G1 total regs*/

//#define HANTRO_DEC_ORG_FIRST_REG        0
//#define HANTRO_DEC_ORG_LAST_REG         59
//#define HANTRO_DEC_EXT_FIRST_REG        119
//#define HANTRO_DEC_EXT_LAST_REG         145

#define HANTRO_PP_ORG_FIRST_REG         60
#define HANTRO_PP_ORG_LAST_REG          100
#define HANTRO_PP_EXT_FIRST_REG         146
#define HANTRO_PP_EXT_LAST_REG          154

/* hantro G2 reg config */
#define HANTRO_G2_DEC_REGS              337 /*G2 total regs*/
#define HANTRO_G2_DEC_FIRST_REG         0
#define HANTRO_G2_DEC_LAST_REG          HANTRO_G2_DEC_REGS-1

/* hantro VC8000D reg config */
#define HANTRO_VC8000D_REGS             472 /*VC8000D total regs*/
#define HANTRO_VC8000D_FIRST_REG        0
#define HANTRO_VC8000D_LAST_REG         HANTRO_VC8000D_REGS-1

#define HANTRO_BIGOCEAN_REGS            304 /* 1216 bytes for BigOcean */

/* Logic module IRQs */
#define HXDEC_NO_IRQ                    -1

#define MAX(a, b)                       (((a) > (b)) ? (a) : (b))

#define DEC_IO_SIZE_MAX                 (MAX(MAX(HANTRO_G2_DEC_REGS, HANTRO_G1_DEC_REGS), HANTRO_VC8000D_REGS) * 4)

/* User should modify these configuration if do porting to own platform. */
/* Please guarantee the base_addr, io_size, dec_irq belong to same core. */

/* Defines use kernel clk cfg or not**/
//#define CLK_CFG
#ifdef CLK_CFG
#define CLK_ID                          "hantrodec_clk"  /*this id should conform with platform define*/
#endif

/* Logic module base address */
#define SOCLE_LOGIC_0_BASE              0x38300000
#define SOCLE_LOGIC_1_BASE              0x38310000

#define VEXPRESS_LOGIC_0_BASE           0xFC010000
#define VEXPRESS_LOGIC_1_BASE           0xFC020000

#define DEC_IO_SIZE_0                   DEC_IO_SIZE_MAX /* bytes */
#define DEC_IO_SIZE_1                   DEC_IO_SIZE_MAX /* bytes */

#define DEC_IRQ_0                       HXDEC_NO_IRQ
#define DEC_IRQ_1                       HXDEC_NO_IRQ

#define IS_G1(hw_id)                    (((hw_id) == 0x6731)? 1 : 0)
#define IS_G2(hw_id)                    (((hw_id) == 0x6732)? 1 : 0)
#define IS_VC8000D(hw_id)               (((hw_id) == 0x8001)? 1 : 0)
#define IS_BIGOCEAN(hw_id)              (((hw_id) == 0xB16D)? 1 : 0)

static const int DecHwId[] = {
  0x6731, /* G1 */
  0x6732, /* G2 */
  0xB16D, /* BigOcean */
  0x8001  /* VC8000D */
};

unsigned long base_port = -1;
unsigned int pcie = 1;
volatile unsigned char *reg = NULL;
unsigned int reg_access_opt = 0;
unsigned int vcmd = 0;

unsigned long multicorebase[HXDEC_MAX_CORES] = {
  HANTRO_REG_OFFSET0,
  HANTRO_REG_OFFSET1,
  0,
  0
};

int irq[HXDEC_MAX_CORES] = {
  DEC_IRQ_0,
  DEC_IRQ_1,
  -1,
  -1
};

unsigned int iosize[HXDEC_MAX_CORES] = {
  DEC_IO_SIZE_0,
  DEC_IO_SIZE_1,
  -1,
  -1
};

/* TODO(min): register number to be flushed/refreshed for each core. */
int reg_count[HXDEC_MAX_CORES] = {
  0,
  0,
  0,
  0
};


/* Because one core may contain multi-pipeline, so multicore base may be changed */
unsigned long multicorebase_actual[HXDEC_MAX_CORES];

struct subsys_config vpu_subsys[MAX_SUBSYS_NUM];

int elements = 2;

#ifdef CLK_CFG
struct clk *clk_cfg;
int is_clk_on;
struct timer_list timer;
#endif

/* module_param(name, type, perm) */
module_param(base_port, ulong, 0);
module_param(pcie, uint, 0);
module_param_array(irq, int, &elements, 0644);
module_param_array(multicorebase, ulong, &elements, 0644);
module_param(reg_access_opt, uint, 0);
module_param(vcmd, uint, 0);

static int hantrodec_major = 0; /* dynamic allocation */

/* here's all the must remember stuff */
typedef struct {
  char *buffer;
  volatile unsigned int iosize[HXDEC_MAX_CORES];
  /* mapped address to different HW cores regs*/
  volatile u8 *hwregs[HXDEC_MAX_CORES][HW_CORE_MAX];
  volatile int irq[HXDEC_MAX_CORES];
  int hw_id[HXDEC_MAX_CORES];
  int cores;
  struct fasync_struct *async_queue_dec;
  struct fasync_struct *async_queue_pp;
} hantrodec_t;

typedef struct {
  u32 cfg[HXDEC_MAX_CORES];              /* indicate the supported format */
  u32 cfg_backup[HXDEC_MAX_CORES];       /* back up of cfg */
  int its_main_core_id[HXDEC_MAX_CORES]; /* indicate if main core exist */
  int its_aux_core_id[HXDEC_MAX_CORES];  /* indicate if aux core exist */
} core_cfg;

static hantrodec_t hantrodec_data; /* dynamic allocation? */

static int ReserveIO(void);
static void ReleaseIO(void);

static void ResetAsic(hantrodec_t * dev);

#ifdef HANTRODEC_DEBUG
static void dump_regs(hantrodec_t *dev);
#endif

/* IRQ handler */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
static irqreturn_t hantrodec_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hantrodec_isr(int irq, void *dev_id);
#endif

static u32 dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX/4];
/* shadow_regs used to compare whether it's necessary to write to registers */
static u32 shadow_dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX/4];

struct semaphore dec_core_sem;
struct semaphore pp_core_sem;

static int dec_irq = 0;
static int pp_irq = 0;

atomic_t irq_rx = ATOMIC_INIT(0);
atomic_t irq_tx = ATOMIC_INIT(0);

static struct file* dec_owner[HXDEC_MAX_CORES];
static struct file* pp_owner[HXDEC_MAX_CORES];
static int CoreHasFormat(const u32 *cfg, int core, u32 format);

/* spinlock_t owner_lock = SPIN_LOCK_UNLOCKED; */
DEFINE_SPINLOCK(owner_lock);

DECLARE_WAIT_QUEUE_HEAD(dec_wait_queue);
DECLARE_WAIT_QUEUE_HEAD(pp_wait_queue);
DECLARE_WAIT_QUEUE_HEAD(hw_queue);
#ifdef CLK_CFG
DEFINE_SPINLOCK(clk_lock);
#endif

#define DWL_CLIENT_TYPE_H264_DEC        1U
#define DWL_CLIENT_TYPE_MPEG4_DEC       2U
#define DWL_CLIENT_TYPE_JPEG_DEC        3U
#define DWL_CLIENT_TYPE_PP              4U
#define DWL_CLIENT_TYPE_VC1_DEC         5U
#define DWL_CLIENT_TYPE_MPEG2_DEC       6U
#define DWL_CLIENT_TYPE_VP6_DEC         7U
#define DWL_CLIENT_TYPE_AVS_DEC         8U
#define DWL_CLIENT_TYPE_RV_DEC          9U
#define DWL_CLIENT_TYPE_VP8_DEC         10U
#define DWL_CLIENT_TYPE_VP9_DEC         11U
#define DWL_CLIENT_TYPE_HEVC_DEC        12U
#define DWL_CLIENT_TYPE_ST_PP           14U
#define DWL_CLIENT_TYPE_H264_MAIN10     15U
#define DWL_CLIENT_TYPE_AVS2_DEC        16U
#define DWL_CLIENT_TYPE_AV1_DEC         17U

#define BIGOCEANDEC_CFG 1
#define BIGOCEANDEC_AV1_E 5

static core_cfg config;

static void ReadCoreConfig(hantrodec_t *dev) {
  int c;
  u32 reg, tmp, mask;

  memset(config.cfg, 0, sizeof(config.cfg));

  for(c = 0; c < dev->cores; c++) {
    /* Decoder configuration */
    if (IS_G1(dev->hw_id[c])) {
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG * 4));

      tmp = (reg >> DWL_H264_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has H264\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

      tmp = (reg >> DWL_JPEG_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has JPEG\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

      tmp = (reg >> DWL_HJPEG_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has HJPEG\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

      tmp = (reg >> DWL_MPEG4_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG4\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

      tmp = (reg >> DWL_VC1_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VC1\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC: 0;

      tmp = (reg >> DWL_MPEG2_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG2\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

      tmp = (reg >> DWL_VP6_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP6\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG_2 * 4));

      /* VP7 and WEBP is part of VP8 */
      mask =  (1 << DWL_VP8_E) | (1 << DWL_VP7_E) | (1 << DWL_WEBP_E);
      tmp = (reg & mask);
      if(tmp & (1 << DWL_VP8_E))
        printk(KERN_INFO "hantrodec: core[%d] has VP8\n", c);
      if(tmp & (1 << DWL_VP7_E))
        printk(KERN_INFO "hantrodec: core[%d] has VP7\n", c);
      if(tmp & (1 << DWL_WEBP_E))
        printk(KERN_INFO "hantrodec: core[%d] has WebP\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC : 0;

      tmp = (reg >> DWL_AVS_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AVS\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC: 0;

      tmp = (reg >> DWL_RV_E) & 0x03U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has RV\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

      /* Post-processor configuration */
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTROPP_SYNTH_CFG * 4));

      tmp = (reg >> DWL_G1_PP_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has PP\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;
    } else if((IS_G2(dev->hw_id[c]))) {
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_CFG_STAT * 4));

      tmp = (reg >> DWL_G2_HEVC_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has HEVC\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_HEVC_DEC : 0;

      tmp = (reg >> DWL_G2_VP9_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP9\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP9_DEC : 0;

      /* Post-processor configuration */
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODECPP_SYNTH_CFG * 4));

      tmp = (reg >> DWL_G2_PP_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has PP\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;
    } else if((IS_VC8000D(dev->hw_id[c])) && config.its_main_core_id[c] < 0) {
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG * 4));

      tmp = (reg >> DWL_H264_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has H264\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

      tmp = (reg >> DWL_H264HIGH10_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has H264HIGH10\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

      tmp = (reg >> DWL_AVS2_E) & 0x03U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AVS2\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS2_DEC : 0;

      tmp = (reg >> DWL_JPEG_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has JPEG\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

      tmp = (reg >> DWL_HJPEG_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has HJPEG\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

      tmp = (reg >> DWL_MPEG4_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG4\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

      tmp = (reg >> DWL_VC1_E) & 0x3U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VC1\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC: 0;

      tmp = (reg >> DWL_MPEG2_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has MPEG2\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

      tmp = (reg >> DWL_VP6_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP6\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG_2 * 4));

      /* VP7 and WEBP is part of VP8 */
      mask =  (1 << DWL_VP8_E) | (1 << DWL_VP7_E) | (1 << DWL_WEBP_E);
      tmp = (reg & mask);
      if(tmp & (1 << DWL_VP8_E))
        printk(KERN_INFO "hantrodec: core[%d] has VP8\n", c);
      if(tmp & (1 << DWL_VP7_E))
        printk(KERN_INFO "hantrodec: core[%d] has VP7\n", c);
      if(tmp & (1 << DWL_WEBP_E))
        printk(KERN_INFO "hantrodec: core[%d] has WebP\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC : 0;

      tmp = (reg >> DWL_AVS_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AVS\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC: 0;

      tmp = (reg >> DWL_RV_E) & 0x03U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has RV\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG_3 * 4));

      tmp = (reg >> DWL_HEVC_E) & 0x07U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has HEVC\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_HEVC_DEC : 0;

      tmp = (reg >> DWL_VP9_E) & 0x07U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has VP9\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP9_DEC : 0;

      /* Post-processor configuration */
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODECPP_CFG_STAT * 4));

      tmp = (reg >> DWL_PP_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has PP\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;

      config.cfg[c] |= 1 << DWL_CLIENT_TYPE_ST_PP;

      if (config.its_aux_core_id[c] >= 0) {
        /* set main_core_id and aux_core_id */
        reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + HANTRODEC_SYNTH_CFG_2 * 4));

        tmp = (reg >> DWL_H264_PIPELINE_E) & 0x01U;
        if(tmp) printk(KERN_INFO "hantrodec: core[%d] has pipeline H264\n", c);
        config.cfg[config.its_aux_core_id[c]] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

        tmp = (reg >> DWL_JPEG_PIPELINE_E) & 0x01U;
        if(tmp) printk(KERN_INFO "hantrodec: core[%d] has pipeline JPEG\n", c);
        config.cfg[config.its_aux_core_id[c]] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;
      }
    } else if (IS_BIGOCEAN(dev->hw_id[c])) {
      reg = ioread32((void*)(dev->hwregs[c][HW_VC8000D] + BIGOCEANDEC_CFG * 4));

      tmp = (reg >> BIGOCEANDEC_AV1_E) & 0x01U;
      if(tmp) printk(KERN_INFO "hantrodec: core[%d] has AV1\n", c);
      config.cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AV1_DEC : 0;
    }
  }
  memcpy(config.cfg_backup, config.cfg, sizeof(config.cfg));
}

static int CoreHasFormat(const u32 *cfg, int core, u32 format) {
  return (cfg[core] & (1 << format)) ? 1 : 0;
}

int GetDecCore(long core, hantrodec_t *dev, struct file* filp, unsigned long format) {
  int success = 0;
  unsigned long flags;

  spin_lock_irqsave(&owner_lock, flags);
  if(CoreHasFormat(config.cfg, core, format) && dec_owner[core] == NULL /*&& config.its_main_core_id[core] >= 0*/) {
    dec_owner[core] = filp;
    success = 1;

    /* If one main core takes one format which doesn't supported by aux core, set aux core's cfg to none video format support */
    if (config.its_aux_core_id[core] >= 0 &&
        !CoreHasFormat(config.cfg, config.its_aux_core_id[core], format)) {
      config.cfg[config.its_aux_core_id[core]] = 0;
    }
    /* If one aux core takes one format, set main core's cfg to aux core supported video format */
    else if (config.its_main_core_id[core] >= 0) {
      config.cfg[config.its_main_core_id[core]] = config.cfg[core];
    }
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  return success;
}

int GetDecCoreAny(long *core, hantrodec_t *dev, struct file* filp,
                  unsigned long format) {
  int success = 0;
  long c;

  *core = -1;

  for(c = 0; c < dev->cores; c++) {
    /* a free core that has format */
    if(GetDecCore(c, dev, filp, format)) {
      success = 1;
      *core = c;
      break;
    }
  }

  return success;
}

int GetDecCoreID(hantrodec_t *dev, struct file* filp,
                 unsigned long format) {
  long c;
  unsigned long flags;

  int core_id = -1;

  for(c = 0; c < dev->cores; c++) {
    /* a core that has format */
    spin_lock_irqsave(&owner_lock, flags);
    if(CoreHasFormat(config.cfg, c, format)) {
      core_id = c;
      spin_unlock_irqrestore(&owner_lock, flags);
      break;
    }
    spin_unlock_irqrestore(&owner_lock, flags);
  }
  return core_id;
}

#if 0
static int hantrodec_choose_core(int is_g1) {
  volatile unsigned char *reg = NULL;
  unsigned int blk_base = 0x38320000;

  PDEBUG("hantrodec_choose_core\n");
  if (!request_mem_region(blk_base, 0x1000, "blk_ctl")) {
    printk(KERN_INFO "blk_ctl: failed to reserve HW regs\n");
    return -EBUSY;
  }

  reg = (volatile u8 *) ioremap_nocache(blk_base, 0x1000);

  if (reg == NULL ) {
    printk(KERN_INFO "blk_ctl: failed to ioremap HW regs\n");
    if (reg)
      iounmap((void *)reg);
    release_mem_region(blk_base, 0x1000);
    return -EBUSY;
  }

  // G1 use, set to 1; G2 use, set to 0, choose the one you are using
  if (is_g1)
    iowrite32(0x1, (void*)(reg + 0x14));  // VPUMIX only use G1, user should modify the reg according to platform design
  else
    iowrite32(0x0, (void*)(reg + 0x14)); // VPUMIX only use G2, user should modify the reg according to platform design

  if (reg)
    iounmap((void *)reg);
  release_mem_region(blk_base, 0x1000);
  PDEBUG("hantrodec_choose_core OK!\n");
  return 0;
}
#endif

long ReserveDecoder(hantrodec_t *dev, struct file* filp, unsigned long format) {
  long core = -1;

  /* reserve a core */
  if (down_interruptible(&dec_core_sem))
    return -ERESTARTSYS;

  /* lock a core that has specific format*/
  if(wait_event_interruptible(hw_queue,
                              GetDecCoreAny(&core, dev, filp, format) != 0 ))
    return -ERESTARTSYS;

#if 0
  if(IS_G1(dev->hw_id[core])) {
    if (0 == hantrodec_choose_core(1))
      printk("G1 is reserved\n");
    else
      return -1;
  } else {
    if (0 == hantrodec_choose_core(0))
      printk("G2 is reserved\n");
    else
      return -1;
  }
#endif

  return core;
}

void ReleaseDecoder(hantrodec_t *dev, long core) {
  u32 status;
  unsigned long flags;

  status = ioread32((void*)(dev->hwregs[core][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));

  /* make sure HW is disabled */
  if(status & HANTRODEC_DEC_E) {
    printk(KERN_INFO "hantrodec: DEC[%li] still enabled -> reset\n", core);

    /* abort decoder */
    status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
    iowrite32(status, (void*)(dev->hwregs[core][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
  }

  spin_lock_irqsave(&owner_lock, flags);

  /* If aux core released, revert main core's config back */
  if (config.its_main_core_id[core] >= 0) {
    config.cfg[config.its_main_core_id[core]] = config.cfg_backup[config.its_main_core_id[core]];
  }

  /* If main core released, revert aux core's config back */
  if (config.its_aux_core_id[core] >= 0) {
    config.cfg[config.its_aux_core_id[core]] = config.cfg_backup[config.its_aux_core_id[core]];
  }

  dec_owner[core] = NULL;

  spin_unlock_irqrestore(&owner_lock, flags);

  up(&dec_core_sem);

  wake_up_interruptible_all(&hw_queue);

  wake_up_interruptible_all(&dec_wait_queue);
}

long ReservePostProcessor(hantrodec_t *dev, struct file* filp) {
  unsigned long flags;

  long core = 0;

  /* single core PP only */
  if (down_interruptible(&pp_core_sem))
    return -ERESTARTSYS;

  spin_lock_irqsave(&owner_lock, flags);

  pp_owner[core] = filp;

  spin_unlock_irqrestore(&owner_lock, flags);

  return core;
}

void ReleasePostProcessor(hantrodec_t *dev, long core) {
  unsigned long flags;

  u32 status = ioread32((void*)(dev->hwregs[core][HW_VC8000D] + HANTRO_IRQ_STAT_PP_OFF));

  /* make sure HW is disabled */
  if(status & HANTRO_PP_E) {
    printk(KERN_INFO "hantrodec: PP[%li] still enabled -> reset\n", core);

    /* disable IRQ */
    status |= HANTRO_PP_IRQ_DISABLE;

    /* disable postprocessor */
    status &= (~HANTRO_PP_E);
    iowrite32(0x10, (void*)(dev->hwregs[core][HW_VC8000D] + HANTRO_IRQ_STAT_PP_OFF));
  }

  spin_lock_irqsave(&owner_lock, flags);

  pp_owner[core] = NULL;

  spin_unlock_irqrestore(&owner_lock, flags);

  up(&pp_core_sem);
}

long ReserveDecPp(hantrodec_t *dev, struct file* filp, unsigned long format) {
  /* reserve core 0, DEC+PP for pipeline */
  unsigned long flags;

  long core = 0;

  /* check that core has the requested dec format */
  if(!CoreHasFormat(config.cfg, core, format))
    return -EFAULT;

  /* check that core has PP */
  if(!CoreHasFormat(config.cfg, core, DWL_CLIENT_TYPE_PP))
    return -EFAULT;

  /* reserve a core */
  if (down_interruptible(&dec_core_sem))
    return -ERESTARTSYS;

  /* wait until the core is available */
  if(wait_event_interruptible(hw_queue,
                              GetDecCore(core, dev, filp, format) != 0)) {
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

  return core;
}

#ifdef HANTRODEC_DEBUG
static u32 flush_count = 0; /* times of calling of DecFlushRegs */
static u32 flush_regs = 0;  /* total number of registers flushed */
#endif

long DecFlushRegs(hantrodec_t *dev, struct core_desc *core) {
  long ret = 0, i;
#ifdef HANTRODEC_DEBUG
  int reg_wr = 2;
#endif
  u32 id = core->id;
  u32 type = core->type;

  PDEBUG("hantrodec: DecFlushRegs\n");
  PDEBUG("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
                    core->id, core->type, core->size, core->reg_id);
  PDEBUG("hantrodec: submodule_iosize = %d\n", vpu_subsys[id].submodule_iosize[type]);
  PDEBUG("hantrodec: reg count = %d\n", reg_count[id]);

  ret = copy_from_user(dec_regs[id], core->regs, vpu_subsys[id].submodule_iosize[type]);
  if (ret) {
    PDEBUG("copy_from_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  if (core->type == HW_VC8000D) {
    /* write all regs but the status reg[1] to hardware */
    if (reg_access_opt) {
      for(i = 3; i < reg_count[id]; i++) {
        /* check whether register value is updated. */
        if (dec_regs[id][i] != shadow_dec_regs[id][i]) {
          iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id][type] + i*4));
          shadow_dec_regs[id][i] = dec_regs[id][i];
#ifdef HANTRODEC_DEBUG
          reg_wr++;
#endif
        }
      }
    } else {
      for(i = 3; i < reg_count[id]; i++) {
        iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id][type] + i*4));
#ifdef VALIDATE_REGS_WRITE
        if (dec_regs[id][i] != ioread32((void*)(dev->hwregs[id][type] + i*4)))
          printk(KERN_INFO "hantrodec: swreg[%ld]: read %08x != write %08x *\n",
                 i, ioread32((void*)(dev->hwregs[id][type] + i*4)), dec_regs[id][i]);
#endif
      }
#ifdef HANTRODEC_DEBUG
      reg_wr = reg_count[id] - 1;
#endif
    }

    /* write swreg2 for AV1, in which bit0 is the start bit */
    iowrite32(dec_regs[id][2], (void*)(dev->hwregs[id][type] + 8));
    shadow_dec_regs[id][2] = dec_regs[id][2];

    /* write the status register, which may start the decoder */
    iowrite32(dec_regs[id][1], (void*)(dev->hwregs[id][type] + 4));
    shadow_dec_regs[id][1] = dec_regs[id][1];

#ifdef HANTRODEC_DEBUG
    flush_count++;
    flush_regs += reg_wr;
#endif

    PDEBUG("flushed registers on core %d\n", id);
    PDEBUG("%d DecFlushRegs: flushed %d/%d registers (dec_mode = %d, avg %d regs per flush)\n",
           flush_count, reg_wr, flush_regs, dec_regs[id][3]>>27, flush_regs/flush_count);
  } else {
    /* write all regs but the status reg[1] to hardware */
    for(i = 0; i < vpu_subsys[id].submodule_iosize[type]/4; i++) {
      iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id][type] + i*4));
#ifdef VALIDATE_REGS_WRITE
      if (dec_regs[id][i] != ioread32((void*)(dev->hwregs[id][type] + i*4)))
        printk(KERN_INFO "hantrodec: swreg[%ld]: read %08x != write %08x *\n",
               i, ioread32((void*)(dev->hwregs[id][type] + i*4)), dec_regs[id][i]);
#endif
    }
  }

  return 0;
}


long DecWriteRegs(hantrodec_t *dev, struct core_desc *core)
{
  long ret = 0;
  u32 i = core->reg_id;
  u32 id = core->id;

  PDEBUG("hantrodec: DecWriteRegs\n");
  PDEBUG("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
          core->id, core->type, core->size, core->reg_id);

  if (id >= MAX_SUBSYS_NUM ||
      !vpu_subsys[id].base_addr ||
      !vpu_subsys[id].submodule_hwregs[core->type] ||
      (core->size & 0x3) ||
      core->reg_id * 4 + core->size > vpu_subsys[id].submodule_iosize[core->type]/4)
    return -EINVAL;

  ret = copy_from_user(dec_regs[id], core->regs, core->size);
  if (ret) {
    PDEBUG("copy_from_user failed, returned %li\n", ret);
    return -EFAULT;
  }

  for (i = core->reg_id; i < core->reg_id + core->size/4; i++) {
    PDEBUG("hantrodec: write %08x to reg[%d]\n", dec_regs[id][i], i);
    iowrite32(dec_regs[id][i-core->reg_id], (void*)(dev->hwregs[id][core->type] + i*4));
    if (core->type == HW_VC8000D)
      shadow_dec_regs[id][i] = dec_regs[id][i-core->reg_id];
  }
  return 0;
}

long DecReadRegs(hantrodec_t *dev, struct core_desc *core)
{
  long ret;
  u32 id = core->id;
  u32 i = core->reg_id;

  PDEBUG("hantrodec: DecReadRegs\n");
  PDEBUG("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
          core->id, core->type, core->size, core->reg_id);

  if (id >= MAX_SUBSYS_NUM ||
      !vpu_subsys[id].base_addr ||
      !vpu_subsys[id].submodule_hwregs[core->type] ||
      (core->size & 0x3) ||
      core->reg_id * 4 + core->size > vpu_subsys[id].submodule_iosize[core->type]/4)
    return -EINVAL;

  /* read specific registers from hardware */
  for (i = core->reg_id; i < core->reg_id + core->size/4; i++) {
    dec_regs[id][i-core->reg_id] = ioread32((void*)(dev->hwregs[id][core->type] + i*4));
    //PDEBUG("hantrodec: read %08x from reg[%d]\n", dec_regs[id][i-core->reg_id], i);
    printk(KERN_INFO "hantrodec: read %08x from reg[%d]\n", dec_regs[id][i-core->reg_id], i);
    if (core->type == HW_VC8000D)
      shadow_dec_regs[id][i] = dec_regs[id][i];
  }

  /* put registers to user space*/
  ret = copy_to_user(core->regs, dec_regs[id], core->size);
  if (ret) {
    PDEBUG("copy_to_user failed, returned %li\n", ret);
    return -EFAULT;
  }
  return 0;
}

long DecRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  long ret, i;
  u32 id = core->id;

  PDEBUG("hantrodec: DecRefreshRegs\n");
  PDEBUG("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
                    core->id, core->type, core->size, core->reg_id);
  PDEBUG("hantrodec: submodule_iosize = %d\n", vpu_subsys[id].submodule_iosize[core->type]);
  PDEBUG("hantrodec: reg count = %d\n", reg_count[id]);

  if (!reg_access_opt) {
    for(i = 0; i < reg_count[id]; i++) {
      dec_regs[id][i] = ioread32((void*)(dev->hwregs[id][HW_VC8000D] + i*4));
    }
  } else {
    // only need to read swreg1,62(?),63,168,169
#define REFRESH_REG(idx) i = (idx); shadow_dec_regs[id][i] = dec_regs[id][i] = ioread32((void*)(dev->hwregs[id][HW_VC8000D] + i*4))
    REFRESH_REG(0);
    REFRESH_REG(1);
    REFRESH_REG(62);
    REFRESH_REG(63);
    REFRESH_REG(168);
    REFRESH_REG(169);
#undef REFRESH_REG
  }

  ret = copy_to_user(core->regs, dec_regs[id], reg_count[id]*4);
  if (ret) {
    PDEBUG("copy_to_user failed, returned %li\n", ret);
    return -EFAULT;
  }
  return 0;
}

static int CheckDecIrq(hantrodec_t *dev, int id) {
  unsigned long flags;
  int rdy = 0;

  const u32 irq_mask = (1 << id);

  spin_lock_irqsave(&owner_lock, flags);

  if(dec_irq & irq_mask) {
    /* reset the wait condition(s) */
    dec_irq &= ~irq_mask;
    rdy = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  return rdy;
}

long WaitDecReadyAndRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  u32 id = core->id;
  long ret;

  PDEBUG("wait_event_interruptible DEC[%d]\n", id);
#ifdef USE_SW_TIMEOUT
  u32 status;
  ret = wait_event_interruptible_timeout(dec_wait_queue, CheckDecIrq(dev, id), msecs_to_jiffies(2000));
  if(ret < 0) {
    PDEBUG("DEC[%d]  wait_event_interruptible interrupted\n", id);
    return -ERESTARTSYS;
  } else if (ret == 0) {
    PDEBUG("DEC[%d]  wait_event_interruptible timeout\n", id);
    status = ioread32((void*)(dev->hwregs[id][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
    /* check if HW is enabled */
    if(status & HANTRODEC_DEC_E) {
      printk(KERN_INFO "hantrodec: DEC[%d] reset becuase of timeout\n", id);

      /* abort decoder */
      status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
      iowrite32(status, (void*)(dev->hwregs[id][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
    }
  }
#else
  ret = wait_event_interruptible(dec_wait_queue, CheckDecIrq(dev, id));
  if(ret) {
    PDEBUG("DEC[%d]  wait_event_interruptible interrupted\n", id);
    return -ERESTARTSYS;
  }
#endif
  atomic_inc(&irq_tx);

  /* refresh registers */
  return DecRefreshRegs(dev, core);
}

#if 0
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
    iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id] + i*4));
  if (sizeof(void *) == 8) {
    for(i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
      iowrite32(dec_regs[id][i], (void*)(dev->hwregs[id] + i*4));
  }
  /* write the stat reg, which may start the PP */
  iowrite32(dec_regs[id][HANTRO_PP_ORG_FIRST_REG],
            (void*)(dev->hwregs[id] + HANTRO_PP_ORG_FIRST_REG * 4));

  return 0;
}

long PPRefreshRegs(hantrodec_t *dev, struct core_desc *core) {
  long i, ret;
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
    dec_regs[id][i] = ioread32((void*)(dev->hwregs[id] + i*4));
  if (sizeof(void *) == 8) {
    for(i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
      dec_regs[id][i] = ioread32((void*)(dev->hwregs[id] + i*4));
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
  unsigned long flags;
  int rdy = 0;

  const u32 irq_mask = (1 << id);

  spin_lock_irqsave(&owner_lock, flags);

  if(pp_irq & irq_mask) {
    /* reset the wait condition(s) */
    pp_irq &= ~irq_mask;
    rdy = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

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
#endif

static int CheckCoreIrq(hantrodec_t *dev, const struct file *filp, int *id) {
  unsigned long flags;
  int rdy = 0, n = 0;
  int owner_found = 0;

  do {
    u32 irq_mask = (1 << n);

    spin_lock_irqsave(&owner_lock, flags);

    if (dec_owner[n] == filp) owner_found = 1;

    if(dec_irq & irq_mask) {
      if (dec_owner[n] == filp) {
        /* we have an IRQ for our client */

        /* reset the wait condition(s) */
        dec_irq &= ~irq_mask;

        /* signal ready core no. for our client */
        *id = n;

        rdy = 1;

        spin_unlock_irqrestore(&owner_lock, flags);
        break;
      } else if(dec_owner[n] == NULL) {
        /* zombie IRQ */
        printk(KERN_INFO "IRQ on core[%d], but no owner!!!\n", n);

        /* reset the wait condition(s) */
        dec_irq &= ~irq_mask;
      }
    }

    spin_unlock_irqrestore(&owner_lock, flags);

    n++; /* next core */
  } while(n < dev->cores);

  if (!owner_found) {
    rdy = -EFAULT;
    *id = -1;
  }

  return rdy;
}

long WaitCoreReady(hantrodec_t *dev, const struct file *filp, int *id) {
  long ret;
  PDEBUG("wait_event_interruptible CORE\n");
#ifdef USE_SW_TIMEOUT
  u32 i, status;
  ret = wait_event_interruptible_timeout(dec_wait_queue, CheckCoreIrq(dev, filp, id), msecs_to_jiffies(2000));
  if(ret < 0) {
    PDEBUG("CORE  wait_event_interruptible interrupted\n");
    return -ERESTARTSYS;
  } else if (ret == 0) {
    PDEBUG("CORE  wait_event_interruptible timeout\n");
    for(i = 0; i < dev->cores; i++) {
      status = ioread32((void*)(dev->hwregs[i][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
      /* check if HW is enabled */
      if((status & HANTRODEC_DEC_E) && dec_owner[i] == filp) {
        printk(KERN_INFO "hantrodec: CORE[%d] reset becuase of timeout\n", i);
        *id = i;
        /* abort decoder */
        status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
        iowrite32(status, (void*)(dev->hwregs[i][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
        break;
      }
    }
  }
#else
  ret = wait_event_interruptible(dec_wait_queue, CheckCoreIrq(dev, filp, id));
  if(ret) {
    PDEBUG("CORE  wait_event_interruptible interrupted\n");
    return -ERESTARTSYS;
  }
  if (*id == -1) return -EFAULT;
#endif
  atomic_inc(&irq_tx);

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_ioctl
 Description     : communication method to/from the user space

 Return type     : long
------------------------------------------------------------------------------*/

static long hantrodec_ioctl(struct file *filp, unsigned int cmd,
                            unsigned long arg) {
  int err = 0;
  long tmp;
#ifdef CLK_CFG
  unsigned long flags;
#endif

#ifdef HW_PERFORMANCE
  struct timeval *end_time_arg;
#endif

  PDEBUG("ioctl cmd 0x%08x\n", cmd);
  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != HANTRODEC_IOC_MAGIC &&
      _IOC_TYPE(cmd) != HANTRO_IOC_MMU &&
      _IOC_TYPE(cmd) != HANTRO_VCMD_IOC_MAGIC)
    return -ENOTTY;
  if ((_IOC_TYPE(cmd) == HANTRODEC_IOC_MAGIC &&
      _IOC_NR(cmd) > HANTRODEC_IOC_MAXNR) ||
      (_IOC_TYPE(cmd) == HANTRO_IOC_MMU &&
      _IOC_NR(cmd) > HANTRO_IOC_MMU_MAXNR) ||
      (_IOC_TYPE(cmd) == HANTRO_VCMD_IOC_MAGIC &&
      _IOC_NR(cmd) > HANTRO_VCMD_IOC_MAXNR))
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

#ifdef CLK_CFG
  spin_lock_irqsave(&clk_lock, flags);
  if (clk_cfg!=NULL && !IS_ERR(clk_cfg)&&(is_clk_on==0)) {
    printk("turn on clock by user\n");
    if (clk_enable(clk_cfg)) {
      spin_unlock_irqrestore(&clk_lock, flags);
      return -EFAULT;
    } else
      is_clk_on=1;
  }
  spin_unlock_irqrestore(&clk_lock, flags);
  mod_timer(&timer, jiffies + 10*HZ); /*the interval is 10s*/
#endif

  switch (cmd) {
  case HANTRODEC_IOC_CLI: {
    __u32 id;
    __get_user(id, (__u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }
    disable_irq(hantrodec_data.irq[id]);
    break;
  }
  case HANTRODEC_IOC_STI: {
    __u32 id;
    __get_user(id, (__u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }
    enable_irq(hantrodec_data.irq[id]);
    break;
  }
  case HANTRODEC_IOCGHWOFFSET: {
    __u32 id;
    __get_user(id, (__u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }

    __put_user(multicorebase_actual[id], (unsigned long *) arg);
    break;
  }
  case HANTRODEC_IOCGHWIOSIZE: {
    __u32 id;
    __u32 io_size;
    __get_user(id, (__u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }
    io_size = hantrodec_data.iosize[id];
    __put_user(io_size, (u32 *) arg);

    return 0;
  }
  case HANTRODEC_IOC_MC_OFFSETS: {
    tmp = copy_to_user((unsigned long *) arg, multicorebase_actual, sizeof(multicorebase_actual));
    if (err) {
      PDEBUG("copy_to_user failed, returned %li\n", tmp);
      return -EFAULT;
    }
    break;
  }
  case HANTRODEC_IOC_MC_CORES:
    __put_user(hantrodec_data.cores, (unsigned int *) arg);
    PDEBUG("hantrodec_data.cores=%d\n", hantrodec_data.cores);
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
  case HANTRODEC_IOCS_DEC_WRITE_REG: {
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    DecWriteRegs(&hantrodec_data, &core);
    break;
  }
  case HANTRODEC_IOCS_PP_PUSH_REG: {
#if 0
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    PPFlushRegs(&hantrodec_data, &core);
#else
    return EINVAL;
#endif
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
#if 0
    struct core_desc core;

    /* get registers from user space*/
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return PPRefreshRegs(&hantrodec_data, &core);
#else
    return EINVAL;
#endif
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
#if 0
    return ReservePostProcessor(&hantrodec_data, filp);
#else
    return EINVAL;
#endif
  case HANTRODEC_IOCT_PP_RELEASE: {
#if 0
    if(arg != 0 || pp_owner[arg] != filp) {
      PDEBUG("bogus PP release %li\n", arg);
      return -EFAULT;
    }

    ReleasePostProcessor(&hantrodec_data, arg);
    break;
#else
    return EINVAL;
#endif
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
#if 0
    struct core_desc core;

    /* get registers from user space */
    tmp = copy_from_user(&core, (void*)arg, sizeof(struct core_desc));
    if (tmp) {
      PDEBUG("copy_from_user failed, returned %li\n", tmp);
      return -EFAULT;
    }

    return WaitPPReadyAndRefreshRegs(&hantrodec_data, &core);
#else
    return EINVAL;
#endif
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
    id = ioread32((void*)(hantrodec_data.hwregs[id][HW_VC8000D]));
    __put_user(id, (u32 *) arg);
    return 0;
  }
  case HANTRODEC_IOCG_CORE_ID: {
    PDEBUG("Get DEC Core_id, format = %li\n", arg);
    return GetDecCoreID(&hantrodec_data, filp, arg);
  }
  case HANTRODEC_IOX_ASIC_BUILD_ID: {
    u32 id, hw_id;
    __get_user(id, (u32*)arg);

    if(id >= hantrodec_data.cores) {
      return -EFAULT;
    }
    hw_id = ioread32((void*)(hantrodec_data.hwregs[id][HW_VC8000D]));
    if (IS_G1(hw_id >> 16) || IS_G2(hw_id >> 16) ||
        (IS_VC8000D(hw_id >> 16) && ((hw_id & 0xFFFF) == 0x6010)) ||
        (IS_BIGOCEAN(hw_id & 0xFFFF)))
      __put_user(hw_id, (u32 *) arg);
    else {
      hw_id = ioread32((void*)(hantrodec_data.hwregs[id][HW_VC8000D] + HANTRODEC_HW_BUILD_ID_OFF));
      __put_user(hw_id, (u32 *) arg);
    }
    return 0;
  }
  case HANTRODEC_DEBUG_STATUS: {
    printk(KERN_INFO "hantrodec: dec_irq     = 0x%08x \n", dec_irq);
    printk(KERN_INFO "hantrodec: pp_irq      = 0x%08x \n", pp_irq);

    printk(KERN_INFO "hantrodec: IRQs received/sent2user = %d / %d \n",
           atomic_read(&irq_rx), atomic_read(&irq_tx));

    for (tmp = 0; tmp < hantrodec_data.cores; tmp++) {
      printk(KERN_INFO "hantrodec: dec_core[%li] %s\n",
             tmp, dec_owner[tmp] == NULL ? "FREE" : "RESERVED");
      printk(KERN_INFO "hantrodec: pp_core[%li]  %s\n",
             tmp, pp_owner[tmp] == NULL ? "FREE" : "RESERVED");
    }
    return 0;
  }
  case HANTRODEC_IOX_SUBSYS: {
    struct subsys_desc subsys = {0};
    /* TODO(min): check all the subsys */
    if (vcmd) {
      subsys.subsys_vcmd_num = 1;
      subsys.subsys_num = subsys.subsys_vcmd_num;
    } else {
      subsys.subsys_num = hantrodec_data.cores;
      subsys.subsys_vcmd_num = 0;
    }
    copy_to_user((u32 *) arg, &subsys, sizeof(struct subsys_desc));
    return 0;
  }
  case HANTRODEC_IOCX_POLL: {
    hantrodec_isr(0, &hantrodec_data);
    return 0;
  }
  default: {
    if(_IOC_TYPE(cmd) == HANTRO_IOC_MMU) {
      return (MMUIoctl(cmd, filp, arg, hantrodec_data.hwregs[0][HW_VC8000D]));
    } else if (_IOC_TYPE(cmd) == HANTRO_VCMD_IOC_MAGIC) {
      return (hantrovcmd_ioctl(filp, cmd, arg));
    }
    return -ENOTTY;
  }
  }

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_open
 Description     : open method

 Return type     : int
------------------------------------------------------------------------------*/

static int hantrodec_open(struct inode *inode, struct file *filp) {
  PDEBUG("dev opened\n");
  if (vcmd)
    hantrovcmd_open(inode, filp);

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_release
 Description     : Release driver

 Return type     : int
------------------------------------------------------------------------------*/

static int hantrodec_release(struct inode *inode, struct file *filp) {
  int n;
  hantrodec_t *dev = &hantrodec_data;

  PDEBUG("closing ...\n");

  if (vcmd) {
    hantrovcmd_release(inode, filp);
    return 0;
  }

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

  MMURelease(filp, hantrodec_data.hwregs[0][HW_MMU]);

  PDEBUG("closed\n");
  return 0;
}

#ifdef CLK_CFG
void hantrodec_disable_clk(unsigned long value) {
  unsigned long flags;
  /*entering this function means decoder is idle over expiry.So disable clk*/
  if (clk_cfg!=NULL && !IS_ERR(clk_cfg)) {
    spin_lock_irqsave(&clk_lock, flags);
    if (is_clk_on==1) {
      clk_disable(clk_cfg);
      is_clk_on = 0;
      printk("turned off hantrodec clk\n");
    }
    spin_unlock_irqrestore(&clk_lock, flags);
  }
}
#endif

/* VFS methods */
static struct file_operations hantrodec_fops = {
  .owner = THIS_MODULE,
  .open = hantrodec_open,
  .release = hantrodec_release,
  .unlocked_ioctl = hantrodec_ioctl,
  .fasync = NULL
};

static int PcieInit(void) {
  int i;

  gDev = pci_get_device(PCI_VENDOR_ID_HANTRO, PCI_DEVICE_ID_HANTRO_PCI, gDev);
  if (NULL == gDev) {
    printk("Init: Hardware not found.\n");
    goto out;
  }

  if (0 > pci_enable_device(gDev)) {
    printk("PcieInit: Device not enabled.\n");
    goto out;
  }

  gBaseHdwr = pci_resource_start (gDev, PCI_CONTROL_BAR);
  if (0 == gBaseHdwr) {
    printk(KERN_INFO "PcieInit: Base Address not set.\n");
    goto out_pci_disable_device;
  }
  printk(KERN_INFO "Base hw val 0x%X\n", (unsigned int)gBaseHdwr);

  gBaseLen = pci_resource_len (gDev, PCI_CONTROL_BAR);
  printk(KERN_INFO "Base hw len 0x%d\n", (unsigned int)gBaseLen);

  //multicorebase[0] = gBaseHdwr + HANTRO_REG_OFFSET0;
  //multicorebase[1] = gBaseHdwr + HANTRO_REG_OFFSET1;
  multicorebase[0] += gBaseHdwr;
  multicorebase[1] += gBaseHdwr;
  for (i = 0; i < MAX_SUBSYS_NUM; i++) {
    if (vpu_subsys[i].base_addr)
      vpu_subsys[i].base_addr += gBaseHdwr;
  }

  gBaseDDRHw = pci_resource_start (gDev, PCI_DDR_BAR);
  if (0 == gBaseDDRHw) {
    printk(KERN_INFO "PcieInit: Base Address not set.\n");
    goto out_pci_disable_device;
  }
  printk(KERN_INFO "Base memory val 0x%08x\n", (unsigned int)gBaseDDRHw);

  gBaseLen = pci_resource_len (gDev, PCI_DDR_BAR);
  printk(KERN_INFO "Base memory len 0x%d\n", (unsigned int)gBaseLen);

  return 0;

out_pci_disable_device:
  pci_disable_device(gDev);

out:
  return -1;
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_init
 Description     : Initialize the driver

 Return type     : int
------------------------------------------------------------------------------*/

int __init hantrodec_init(void) {
  int result, i;
  enum MMUStatus status = 0;

  PDEBUG("module init\n");

  CheckSubsysCoreArray(vpu_subsys, &vcmd);

  if (vcmd) {
    result = hantrovcmd_init();
    if (result) return result;

    result = register_chrdev(hantrodec_major, "hantrodec", &hantrodec_fops);
    if(result < 0) {
      printk(KERN_INFO "hantrodec: unable to get major %d\n", hantrodec_major);
      goto err;
    } else if(result != 0) { /* this is for dynamic major */
      hantrodec_major = result;
    }
    return 0;
  }

  if (pcie) {
    result = PcieInit();
    if(result)
      goto err;
  }

  printk(KERN_INFO "hantrodec: dec/pp kernel module. \n");

  /* If base_port is set when insmod, use that for single core legacy mode. */
  if (base_port != -1) {
    multicorebase[0] = base_port;
	  if (pcie)
	    multicorebase[0] += HANTRO_REG_OFFSET0;
    elements = 1;
    printk(KERN_INFO "hantrodec: Init single core at 0x%08lx IRQ=%i\n",
           multicorebase[0], irq[0]);
  } else {
    printk(KERN_INFO "hantrodec: Init multi core[0] at 0x%16lx\n"
           "                      core[1] at 0x%16lx\n"
           "                      core[2] at 0x%16lx\n"
           "                      core[3] at 0x%16lx\n"
           "           IRQ_0=%i\n"
           "           IRQ_1=%i\n",
           multicorebase[0], multicorebase[1],
           multicorebase[2], multicorebase[3],
           irq[0],irq[1]);
  }

  hantrodec_data.cores = 0;

  hantrodec_data.iosize[0] = DEC_IO_SIZE_0;
  hantrodec_data.irq[0] = irq[0];
  hantrodec_data.iosize[1] = DEC_IO_SIZE_1;
  hantrodec_data.irq[1] = irq[1];

  for(i=0; i< HXDEC_MAX_CORES; i++) {
    int j;
    for (j = 0; j < HW_CORE_MAX; j++)
      hantrodec_data.hwregs[i][j] = 0;
    /* If user gave less core bases that we have by default,
     * invalidate default bases
     */
    if(elements && i>=elements) {
      multicorebase[i] = 0;
    }
  }

  hantrodec_data.async_queue_dec = NULL;
  hantrodec_data.async_queue_pp = NULL;

  result = register_chrdev(hantrodec_major, "hantrodec", &hantrodec_fops);
  if(result < 0) {
    printk(KERN_INFO "hantrodec: unable to get major %d\n", hantrodec_major);
    goto err;
  } else if(result != 0) { /* this is for dynamic major */
    hantrodec_major = result;
  }

#ifdef CLK_CFG
  /* first get clk instance pointer */
  clk_cfg = clk_get(NULL, CLK_ID);
  if (!clk_cfg||IS_ERR(clk_cfg)) {
    printk("get handrodec clk failed!\n");
    goto err;
  }

  /* prepare and enable clk */
  if(clk_prepare_enable(clk_cfg)) {
    printk("try to enable handrodec clk failed!\n");
    goto err;
  }
  is_clk_on = 1;

  /*init a timer to disable clk*/
  init_timer(&timer);
  timer.function = &hantrodec_disable_clk;
  timer.expires =  jiffies + 100*HZ; //the expires time is 100s
  add_timer(&timer);
#endif

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

  /* register irq for each core */
  if(irq[0] > 0) {
    result = request_irq(irq[0], hantrodec_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
                         SA_INTERRUPT | SA_SHIRQ,
#else
                         IRQF_SHARED,
#endif
                         "hantrodec", (void *) &hantrodec_data);

    if(result != 0) {
      if(result == -EINVAL) {
        printk(KERN_ERR "hantrodec: Bad irq number or handler\n");
      } else if(result == -EBUSY) {
        printk(KERN_ERR "hantrodec: IRQ <%d> busy, change your config\n",
               hantrodec_data.irq[0]);
      }

      ReleaseIO();
      goto err;
    }
  } else {
    printk(KERN_INFO "hantrodec: IRQ not in use!\n");
  }

  if(irq[1] > 0) {
    result = request_irq(irq[1], hantrodec_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
                         SA_INTERRUPT | SA_SHIRQ,
#else
                         IRQF_SHARED,
#endif
                         "hantrodec", (void *) &hantrodec_data);

    if(result != 0) {
      if(result == -EINVAL) {
        printk(KERN_ERR "hantrodec: Bad irq number or handler\n");
      } else if(result == -EBUSY) {
        printk(KERN_ERR "hantrodec: IRQ <%d> busy, change your config\n",
               hantrodec_data.irq[1]);
      }

      ReleaseIO();
      goto err;
    }
  } else {
    printk(KERN_INFO "hantrodec: IRQ not in use!\n");
  }

  if (hantrodec_data.hwregs[0][HW_MMU]) {
    status = MMUInit(hantrodec_data.hwregs[0][HW_MMU]);
    if(status == MMU_STATUS_NOT_FOUND)
      printk(KERN_INFO "MMU does not exist!\n");
    else if(status != MMU_STATUS_OK)
      goto err;
    else
      printk(KERN_INFO "MMU detected!\n");
  }

#ifdef SUPPORT_AXIFE
  AXIFEEnable(hantrodec_data.hwregs[0][HW_AXIFE]);
#endif

  printk(KERN_INFO "hantrodec: module inserted. Major = %d\n", hantrodec_major);

  /* Please call the TEE functions to set VC80000D DRM relative registers here */

  return 0;

err:
  ReleaseIO();
  printk(KERN_INFO "hantrodec: module not inserted\n");
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
  int n =0;

  if (vcmd) {
    hantrovcmd_cleanup();
    unregister_chrdev(hantrodec_major, "hantrodec");
    return;
  }

  /* reset hardware */
  ResetAsic(dev);

  MMUCleanup(dev->hwregs[0][HW_VC8000D]);

  /* free the IRQ */
  for (n = 0; n < dev->cores; n++) {
    if(dev->irq[n] != -1) {
      free_irq(dev->irq[n], (void *) dev);
    }
  }

  ReleaseIO();

#ifdef CLK_CFG
  if (clk_cfg!=NULL && !IS_ERR(clk_cfg)) {
    clk_disable_unprepare(clk_cfg);
    is_clk_on = 0;
    printk("turned off hantrodec clk\n");
  }

  /*delete timer*/
  del_timer(&timer);
#endif

  unregister_chrdev(hantrodec_major, "hantrodec");

  printk(KERN_INFO "hantrodec: module removed\n");
  return;
}

/*------------------------------------------------------------------------------
 Function name   : CheckHwId
 Return type     : int
------------------------------------------------------------------------------*/
static int CheckHwId(hantrodec_t * dev) {
  int hwid;
  int i;
  size_t num_hw = sizeof(DecHwId) / sizeof(*DecHwId);

  int found = 0;

  for (i = 0; i < dev->cores; i++) {
    if (dev->hwregs[i][HW_VC8000D] != NULL ) {
      hwid = readl(dev->hwregs[i][HW_VC8000D]);
      printk(KERN_INFO "hantrodec: core %d HW ID=0x%08x\n", i, hwid);
      if (IS_BIGOCEAN(hwid & 0xFFFF)) {
        hwid = (hwid & 0xFFFF);
        reg_count[i] = HANTRO_BIGOCEAN_REGS;
      } else {
        hwid = (hwid >> 16) & 0xFFFF; /* product version only */
        reg_count[i] = HANTRO_VC8000D_REGS;
      }

      while (num_hw--) {
        if (hwid == DecHwId[num_hw]) {
          printk(KERN_INFO "hantrodec: Supported HW found at 0x%16lx\n",
                 multicorebase_actual[i]);
          found++;
          dev->hw_id[i] = hwid;
          break;
        }
      }
      if (!found) {
        printk(KERN_INFO "hantrodec: Unknown HW found at 0x%16lx\n",
               multicorebase_actual[i]);
        return 0;
      }
      found = 0;
      num_hw = sizeof(DecHwId) / sizeof(*DecHwId);
    }
  }

  return 1;
}

/*------------------------------------------------------------------------------
 Function name   : ReserveIO
 Description     : IO reserve

 Return type     : int
------------------------------------------------------------------------------*/
static int ReserveIO(void) {
  int i, j;
  long int hwid;
  u32 reg;

  memcpy(multicorebase_actual, multicorebase, HXDEC_MAX_CORES * sizeof(unsigned long));
  memcpy((unsigned int*)(hantrodec_data.iosize), iosize, HXDEC_MAX_CORES * sizeof(unsigned int));
  memcpy((unsigned int*)(hantrodec_data.irq), irq, HXDEC_MAX_CORES * sizeof(int));

  for (i = 0; i < MAX_SUBSYS_NUM; i++) {
    if (!vpu_subsys[i].base_addr) continue;

    for (j = 0; j < HW_CORE_MAX; j++) {
      if (vpu_subsys[i].submodule_iosize[j]) {
        printk(KERN_INFO "hantrodec: base=0x%16lx, iosize=%d\n",
                          vpu_subsys[i].base_addr + vpu_subsys[i].submodule_offset[j],
                          vpu_subsys[i].submodule_iosize[j]);

        if (!request_mem_region(vpu_subsys[i].base_addr + vpu_subsys[i].submodule_offset[j],
                                vpu_subsys[i].submodule_iosize[j],
                                "hantrodec0")) {
          printk(KERN_INFO "hantrodec: failed to reserve HW %d regs\n", j);
          return -EBUSY;
        }

        vpu_subsys[i].submodule_hwregs[j] =
          hantrodec_data.hwregs[i][j] =
            (volatile u8 *) ioremap_nocache(vpu_subsys[i].base_addr + vpu_subsys[i].submodule_offset[j],
                                            hantrodec_data.iosize[i]);

        if (hantrodec_data.hwregs[i][j] == NULL) {
          printk(KERN_INFO "hantrodec: failed to ioremap HW %d regs\n", j);
          release_mem_region(vpu_subsys[i].base_addr + vpu_subsys[i].submodule_offset[j],
                             vpu_subsys[i].submodule_iosize[j]);
          return -EBUSY;
        }
        config.its_main_core_id[i] = -1;
        config.its_aux_core_id[i] = -1;

        printk(KERN_INFO "hantrodec: HW %d reg[0]=0x%08x\n", j, readl(hantrodec_data.hwregs[i][j]));
        hwid = ((readl(hantrodec_data.hwregs[i][HW_VC8000D])) >> 16) & 0xFFFF; /* product version only */

        /* if (IS_VC8000D(hwid)) { */
        if (0) {
          /*TODO(min): DO NOT support 2nd pipeline. */
          reg = readl(hantrodec_data.hwregs[i][HW_VC8000D] + HANTRODEC_SYNTH_CFG_2_OFF);
          if (((reg >> DWL_H264_PIPELINE_E) & 0x01U) || ((reg >> DWL_JPEG_PIPELINE_E) & 0x01U)) {
            i++;
            config.its_aux_core_id[i-1] = i;
            config.its_main_core_id[i] = i-1;
            config.its_aux_core_id[i] = -1;
            multicorebase_actual[i] = multicorebase_actual[i-1] + 0x800;
            hantrodec_data.iosize[i] = hantrodec_data.iosize[i-1];
            memcpy(multicorebase_actual+i+1, multicorebase+i,
                   (HXDEC_MAX_CORES - i - 1) * sizeof(unsigned long));
            memcpy((unsigned int*)hantrodec_data.iosize+i+1, iosize+i,
                   (HXDEC_MAX_CORES - i - 1) * sizeof(unsigned int));
            if (!request_mem_region(multicorebase_actual[i], hantrodec_data.iosize[i],
                                "hantrodec0")) {
              printk(KERN_INFO "hantrodec: failed to reserve HW regs\n");
              return -EBUSY;
            }

            hantrodec_data.hwregs[i][HW_VC8000D] = (volatile u8 *) ioremap_nocache(multicorebase_actual[i],
                                       hantrodec_data.iosize[i]);

            if (hantrodec_data.hwregs[i][HW_VC8000D] == NULL ) {
              printk(KERN_INFO "hantrodec: failed to ioremap HW regs\n");
              ReleaseIO();
                return -EBUSY;
            }
            hantrodec_data.cores++;
          }
        }
      }
    }    
    hantrodec_data.cores++;
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
  int i, j;
  for (i = 0; i < hantrodec_data.cores; i++) {
    for (j = 0; j < HW_CORE_MAX; j++) {
      if (hantrodec_data.hwregs[i][j]) {
        iounmap((void *) hantrodec_data.hwregs[i][j]);
        release_mem_region(vpu_subsys[i].base_addr + vpu_subsys[i].submodule_offset[j],
                           vpu_subsys[i].submodule_iosize[j]);
        hantrodec_data.hwregs[i][j] = 0;
      }
    }
  }
}

/*------------------------------------------------------------------------------
 Function name   : hantrodec_isr
 Description     : interrupt handler

 Return type     : irqreturn_t
------------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
irqreturn_t hantrodec_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t hantrodec_isr(int irq, void *dev_id)
#endif
{
  unsigned long flags;
  unsigned int handled = 0;
  int i;
  volatile u8 *hwregs;

  hantrodec_t *dev = (hantrodec_t *) dev_id;
  u32 irq_status_dec;

  spin_lock_irqsave(&owner_lock, flags);

  for(i=0; i<dev->cores; i++) {
    volatile u8 *hwregs = dev->hwregs[i][HW_VC8000D];

    /* interrupt status register read */
    irq_status_dec = ioread32((void*)(hwregs + HANTRODEC_IRQ_STAT_DEC_OFF));

    if(irq_status_dec & HANTRODEC_DEC_IRQ) {
      /* clear dec IRQ */
      irq_status_dec &= (~HANTRODEC_DEC_IRQ);
      iowrite32(irq_status_dec, (void*)(hwregs + HANTRODEC_IRQ_STAT_DEC_OFF));

      PDEBUG("decoder IRQ received! core %d\n", i);

      atomic_inc(&irq_rx);

      dec_irq |= (1 << i);

      wake_up_interruptible_all(&dec_wait_queue);
      handled++;
    }
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  if(!handled) {
    PDEBUG("IRQ received, but not hantrodec's!\n");
  }

  (void)hwregs;
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
    status = ioread32((void*)(dev->hwregs[j][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));

    if( status & HANTRODEC_DEC_E) {
      /* abort with IRQ disabled */
      status = HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
      iowrite32(status, (void*)(dev->hwregs[j][HW_VC8000D] + HANTRODEC_IRQ_STAT_DEC_OFF));
    }

    if (IS_G1(dev->hw_id[j]))
      /* reset PP */
      iowrite32(0, (void*)(dev->hwregs[j][HW_VC8000D] + HANTRO_IRQ_STAT_PP_OFF));

    for (i = 4; i < dev->iosize[j]; i += 4) {
      iowrite32(0, (void*)(dev->hwregs[j][HW_VC8000D] + i));
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

  PDEBUG("Reg Dump Start\n");
  for(c = 0; c < dev->cores; c++) {
    for(i = 0; i < dev->iosize[c]; i += 4*4) {
      PDEBUG("\toffset %04X: %08X  %08X  %08X  %08X\n", i,
             ioread32(dev->hwregs[c][HW_VC8000D] + i),
             ioread32(dev->hwregs[c][HW_VC8000D] + i + 4),
             ioread32(dev->hwregs[c][HW_VC8000D] + i + 16),
             ioread32(dev->hwregs[c][HW_VC8000D] + i + 24));
    }
  }
  PDEBUG("Reg Dump End\n");
}
#endif


module_init( hantrodec_init);
module_exit( hantrodec_cleanup);

/* module description */
//MODULE_LICENSE("Proprietary");
//MODULE_AUTHOR("Google Finland Oy");
//MODULE_DESCRIPTION("Driver module for Hantro Decoder/Post-Processor");

