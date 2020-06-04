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
--  Abstract : Cache device driver (kernel module)
--
------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "hantro_cache.h"

/********variables declaration related with race condition**********/

//struct semaphore enc_core_sem;
DECLARE_WAIT_QUEUE_HEAD(hw_queue);
DEFINE_SPINLOCK(owner_lock);
DECLARE_WAIT_QUEUE_HEAD(enc_wait_queue);

/*******************PCIE CONFIG*************************/
#ifdef PCI_BUS
#define PCI_VENDOR_ID_HANTRO            0x10ee//0x16c3 
#define PCI_DEVICE_ID_HANTRO_PCI      0x8014// 0x7011// 0xabcd 

/* Base address got control register */
#define PCI_H2_BAR              4 

struct pci_dev *gDev = NULL;    /* PCI device structure. */
unsigned long gBaseHdwr;        /* PCI base register address (Hardware address) */
u32 gBaseLen;                   /* Base register address Length */
unsigned long base_port = 0;
#endif
static int g_hwid = -1;		/* SE1000 */
/*------------------------------------------------------------------------
*****************************PORTING LAYER********************************
-------------------------------------------------------------------------*/
#define RESOURCE_SHARED_INTER_CORES        0
#define CORE_0_IO_BASE                 0xe7810000//the base addr of core. for PCIE, it refers to offset from bar
#define CORE_1_IO_BASE                 0xe7810000//the base addr of core. for PCIE, it refers to offset from bar

#define CORE_2_IO_BASE                 0x740000//the base addr of core. for PCIE, it refers to offset from bar
#define CORE_3_IO_BASE                 0x740000//the base addr of core. for PCIE, it refers to offset from bar

#define CORE_0_IO_SIZE                 ((6+4*8) * 4)    /* bytes *///cache register size
#define CORE_1_IO_SIZE                 ((5+5*16) * 4)    /* bytes *///shaper register size


#define CORE_2_IO_SIZE                 ((6+4*8) * 4)    /* bytes *///cache register size
#define CORE_3_IO_SIZE                 ((5+5*16) * 4)    /* bytes *///shaper register size


#define INT_PIN_CORE_0                    409 /* for SE1000 */
#define INT_PIN_CORE_1                    409

#define INT_PIN_CORE_2                    -1
#define INT_PIN_CORE_3                    -1
/*for all cores, the core info should be listed here for later use*/
/*base_addr, iosize, irq*/
CORE_CONFIG core_array[]= {
    //{VC8000E, CORE_0_IO_BASE, CORE_0_IO_SIZE, INT_PIN_CORE_0,DIR_RD}, //core_0 just support cache read for VC8000E
    //{VC8000E, CORE_1_IO_BASE, CORE_1_IO_SIZE, INT_PIN_CORE_1,DIR_WR}, //core_1 just support cache write for VC8000E
    {VC8000D_0, CORE_0_IO_BASE, CORE_0_IO_SIZE, INT_PIN_CORE_0,DIR_RD}, //core_0 just support cache read for VC8000D_0
    {VC8000D_0, CORE_1_IO_BASE, CORE_1_IO_SIZE, INT_PIN_CORE_1,DIR_WR}, //core_1 just support cache write for VC8000D_0
    //{VC8000D_1, CORE_2_IO_BASE, CORE_2_IO_SIZE, INT_PIN_CORE_2,DIR_RD}, //core_0 just support cache read for VC8000D_1
    //{VC8000D_1, CORE_3_IO_BASE, CORE_3_IO_SIZE, INT_PIN_CORE_3,DIR_WR} //core_1 just support cache write for VC8000D_1
};

/*------------------------------END-------------------------------------*/

/***************************TYPE AND FUNCTION DECLARATION****************/

/* here's all the must remember stuff */
typedef struct
{
    CORE_CONFIG  core_cfg; //config of each core,such as base addr, irq,etc
    unsigned long hw_id; //hw id to indicate project
    u32 core_id; //core id for driver and sw internal use
    u32 is_valid; //indicate this core is hantro's core or not
    u32 is_reserved; //indicate this core is occupied by user or not
    int pid; //indicate which process is occupying the core
    u32 irq_received; //indicate this core receives irq
    u32 irq_status;
    char *buffer;
    unsigned int buffsize;
    volatile u8 *hwregs;
    unsigned long com_base_addr;//common base addr of each L2
} cache_dev_t;

static int ReserveIO(void);
static void ReleaseIO(void);
static void ResetAsic(cache_dev_t * dev);

int CheckCoreOccupation(cache_dev_t *dev);

//static void dump_regs(unsigned long data);

/* IRQ handler */
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
static irqreturn_t cache_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t cache_isr(int irq, void *dev_id);
#endif

/*********************local variable declaration*****************/
/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int cache_dev_major = 0;
static int total_core_num = 0;
volatile unsigned int asic_status = 0;
/* dynamic allocation*/
static cache_dev_t* cache_data = NULL;

static struct file* cache_owner[6];

/******************************************************************************/
static int CheckCacheIrq(cache_dev_t *dev)
{
    unsigned long flags;
    int rdy = 0;
    spin_lock_irqsave(&owner_lock, flags);

    if(dev->irq_received)
    {
     /* reset the wait condition(s) */
     dev->irq_received = 0;
     rdy = 1;
    }

    spin_unlock_irqrestore(&owner_lock, flags);

    return rdy;
}
unsigned int WaitCacheReady(cache_dev_t *dev)
{
   PDEBUG("WaitCacheReady\n");

   if(wait_event_interruptible(enc_wait_queue, CheckCacheIrq(dev)))
   {
       PDEBUG("Cache wait_event_interruptible interrupted\n");
       return -ERESTARTSYS;
   }

   return 0;
}

int CheckCoreOccupation(cache_dev_t *dev) 
{
  int ret = 0;
  unsigned long flags;

  spin_lock_irqsave(&owner_lock, flags);
  if(!dev->is_reserved) 
  {
   dev->is_reserved = 1;
   dev->pid = current->pid;
   ret = 1;
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  return ret;
}

int GetWorkableCore(cache_dev_t *dev,u32 *core_id) 
{
  int ret = 0;
  u32 i = 0;
  
  driver_cache_dir dir = (*core_id)&0x01;
  cache_client_type client = ((*core_id)&0x06)>>1;
  PDEBUG("dir=%d,client=%d\n",dir,client);
  for(i = 0; i < total_core_num; i++)
  {
   /* a valid free Core*/
   if(dev[i].is_valid && dev[i].core_cfg.client == client && dev[i].core_cfg.dir == dir && CheckCoreOccupation(&dev[i]))
   {
    ret = 1;
    *core_id = i;
    break;
   } 
  }
  return ret;
}

long ReserveCore(cache_dev_t *dev, struct file* filp, u32 *core_id) 
{
  driver_cache_dir dir = (*core_id)&0x01;
  cache_client_type client = ((*core_id)&0x06)>>1;
  int i=0,status = 0;
  for(i = 0; i < total_core_num; i++)
  {
    /* a valid core supports such client and dir*/
    if(dev[i].is_valid && dev[i].core_cfg.client == client && dev[i].core_cfg.dir == dir)
    {
      status = 1;
      break;
    } 
  }

  if (status == 0)
  {
    printk(KERN_INFO "NO any core support client:%d,dir:%d.\n",dir,client);
    return -1;
  }

  /* lock a core that has specified core id*/
  if(wait_event_interruptible(hw_queue,
        GetWorkableCore(dev,core_id) != 0 ))
    return -ERESTARTSYS;
  cache_owner[*core_id] = filp;
  return 0;
}

void ReleaseCore(cache_dev_t * dev,u32 core_id)
{
  unsigned long flags;

  /* release specified core id */
  spin_lock_irqsave(&owner_lock, flags);
  if(dev[core_id].is_reserved /*&& dev[core_id].pid == current->pid*/)
  {
    dev[core_id].pid = -1;
    dev[core_id].is_reserved = 0;
  }
  
  dev[core_id].irq_received = 0;
  dev[core_id].irq_status = 0;

  cache_owner[core_id] = NULL;

  spin_unlock_irqrestore(&owner_lock, flags);
  
  wake_up_interruptible_all(&hw_queue);

  return;
}

static long cache_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int err = 0;

    unsigned int tmp;
    PDEBUG("ioctl cmd 0x%08ux\n", cmd);
    /*
     * extract the type and number bitfields, and don't process
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if(_IOC_TYPE(cmd) != CACHE_IOC_MAGIC)
        return -ENOTTY;
    if(_IOC_NR(cmd) > CACHE_IOC_MAXNR)
        return -ENOTTY;

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
    else if(_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
    if(err)
        return -EFAULT;

    switch (cmd)
    {
     case CACHE_IOCGHWOFFSET:
        {
         u32 id;
         __get_user(id, (u32*)arg);

         if(id >= total_core_num || cache_data[id].is_valid==0) 
         {
          return -EFAULT;
         }
         __put_user(cache_data[id].core_cfg.base_addr, (unsigned long *) arg);
         break;
        }

     case CACHE_IOCGHWIOSIZE:
        {
         u32 id;
         u32 io_size;
         __get_user(id, (u32*)arg);
         
         if(id >= total_core_num || cache_data[id].is_valid==0) 
         {
          return -EFAULT;
         }
         io_size = cache_data[id].core_cfg.iosize;
         __put_user(io_size, (u32 *) arg);
   
         break;
        }
     case CACHE_IOCGHWID:
        {
         __put_user(g_hwid, (int *)arg);
         break;
        }
     case CACHE_IOCG_CORE_NUM:
        __put_user(total_core_num, (unsigned int *) arg);
        break;
     case CACHE_IOCH_HW_RESERVE: 
        {
         u32 core_id;
         int ret;
         PDEBUG("Reserve cache core\n");
         
         __get_user(core_id, (u32*)arg);//get client and direction info
		 
         ret = ReserveCore(cache_data, filp, &core_id);
         if (ret == 0)
            __put_user(core_id, (unsigned int *) arg);
         return ret;
        }
     case CACHE_IOCH_HW_RELEASE: 
        {
         u32 core_id;
         __get_user(core_id, (u32*)arg);

         if(cache_owner[core_id] != filp) {
           PDEBUG("bogus cache release, core = %d\n", core_id);
           return -EFAULT;
         }

         PDEBUG("Release cache core\n");
     
         ReleaseCore(cache_data,core_id);
     
         break;
        }
        
     case CACHE_IOCG_ABORT_WAIT:
        {
         u32 id;
         __get_user(id, (u32*)arg);
         tmp = WaitCacheReady(&cache_data[id]);
         if (tmp==0)
           __put_user(cache_data[id].irq_status, (unsigned int *)arg);  
         else
           __put_user(0, (unsigned int *)arg);  
         
         break;
        }
    }
    return 0;
}

static int cache_open(struct inode *inode, struct file *filp)
{
    int result = 0;
    cache_dev_t *dev = cache_data;

    filp->private_data = (void *) dev;

    PDEBUG("dev opened\n");
    return result;
}
static int cache_release(struct inode *inode, struct file *filp)
{
    int core_id = 0;
    
    for (core_id = 0; core_id < total_core_num; core_id++)
    {
      if (cache_owner[core_id] == filp)
        ReleaseCore(cache_data,core_id);
    }

    PDEBUG("dev closed\n");
    return 0;
}

/* VFS methods */
static struct file_operations cache_fops = {
    .owner= THIS_MODULE,
    .open = cache_open,
    .release = cache_release,
    .unlocked_ioctl = cache_ioctl,
    .fasync = NULL,
};
#ifdef PCI_BUS
/*------------------------------------------------------------------------------
 Function name   : PcieInit
 Description     : Initialize PCI Hw access

 Return type     : int
 ------------------------------------------------------------------------------*/
static int PcieInit(void)
{   
    int i = 0;
    
    gDev = pci_get_device(PCI_VENDOR_ID_HANTRO, PCI_DEVICE_ID_HANTRO_PCI, gDev);
    if (NULL == gDev) {
        printk("Init: Hardware not found.\n");
        goto out;
    }

    if (0 > pci_enable_device(gDev)) {
        printk("Init: Device not enabled.\n");
        goto out;
    }

    gBaseHdwr = pci_resource_start (gDev, PCI_H2_BAR);
    if (0 > gBaseHdwr) {
        printk(KERN_INFO "Init: Base Address not set.\n");
        goto out_pci_disable_device;
    }
    printk(KERN_INFO "Base hw val 0x%X\n", (unsigned int)gBaseHdwr);

    gBaseLen = pci_resource_len (gDev, PCI_H2_BAR);
    printk(KERN_INFO "Base hw len 0x%d\n", (unsigned int)gBaseLen);

    for (i=0; i<total_core_num; i++)
    {
      core_array[i].base_addr  = gBaseHdwr + core_array[i].base_addr;//the offset is based on which bus interface is chosen
    }
 
    return 0;

out_pci_disable_device:
    pci_disable_device(gDev);   
out:    
    return -1;
}
#endif
int __init cache_init(void)
{
    int result;
    int i;

    total_core_num = sizeof(core_array)/sizeof(CORE_CONFIG);
    
#ifdef PCI_BUS  
    result = PcieInit();
    if(result)
        goto err1;  
#endif
    cache_data = (cache_dev_t *)vmalloc(sizeof(cache_dev_t)*total_core_num);
    if (cache_data == NULL) { /* CJ: fixed result used uninitilaized issue. */
        result = -ENOMEM;
        goto err1;
    }
    memset(cache_data,0,sizeof(cache_dev_t)*total_core_num);

    for(i=0;i<total_core_num;i++)
    {
      cache_data[i].core_cfg = core_array[i];
      cache_data[i].hwregs = NULL;
      cache_data[i].core_id = i;
      cache_data[i].is_valid = 0;
      cache_data[i].com_base_addr = core_array[i].base_addr;
    }

    result = register_chrdev(cache_dev_major, "hantro_cache", &cache_fops);
    if(result < 0)
    {
        printk(KERN_INFO "hantro_cache: unable to get major <%d>\n",
               cache_dev_major);
        goto err1;
    }
    else if(result != 0)    /* this is for dynamic major */
    {
        cache_dev_major = result;
    }

    result = ReserveIO();
    if(result < 0)
    {
        goto err;
    }

    memset(cache_owner, 0, sizeof(cache_owner));

    ResetAsic(cache_data);  /* reset hardware */

    //sema_init(&enc_core_sem, 1);

#ifndef PCI_BUS
    /* get the IRQ line */
    for (i=0;i<total_core_num;i++)
    {
      if (cache_data[i].is_valid==0)
        continue;
      if(cache_data[i].core_cfg.irq!= -1)
      {
          result = request_irq(cache_data[i].core_cfg.irq, cache_isr,
  #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
                               SA_INTERRUPT | SA_SHIRQ,
  #else
                               IRQF_SHARED,
  #endif
                               "hantro_cache", (void *) &cache_data[i]);
          if(result == -EINVAL)
          {
              printk(KERN_ERR "hantro_cache: Bad irq number or handler\n");
              ReleaseIO();
              goto err;
          }
          else if(result == -EBUSY)
          {
              printk(KERN_ERR "hantro_cache: IRQ <%d> busy, change your config\n",
                     cache_data[i].core_cfg.irq);
              ReleaseIO();
              goto err;
          }
      }
      else
      {
          printk(KERN_INFO "hantro_cache: IRQ not in use!\n");
      }
    }
#endif

    return 0;

  err:
    unregister_chrdev(cache_dev_major, "hantro_cache");
  err1:
    printk(KERN_INFO "cache: module not inserted\n");
    return result;
}

void __exit cache_cleanup(void)
{
  int i=0;
  for(i=0;i<total_core_num;i++)
  {
    if (cache_data[i].is_valid==0)
      continue;
    writel(0, cache_data[i].hwregs + 0x04); /* disable HW */
    writel(0xF, cache_data[i].hwregs + 0x14); /* clear IRQ */

    /* free the encoder IRQ */
    if(cache_data[i].core_cfg.irq != -1)
    {
      free_irq(cache_data[i].core_cfg.irq, (void *)cache_data);
    }
   }

  ReleaseIO();
  vfree(cache_data);

  unregister_chrdev(cache_dev_major, "hantro_cache");

  printk(KERN_INFO "hantro_cache: module removed\n");
  return;
}

int cache_get_hwid(unsigned long base_addr, int *hwid)
{
    u8 *hwregs = NULL;
    
    if(!request_mem_region
         (base_addr, 4, "hantro_cache"))
    {
        printk(KERN_INFO "hantr_cache: failed to reserve HW regs,base_addr:%p\n",(void *)base_addr);
        return -1;
    }
  
    hwregs = (u8 *) ioremap_nocache(base_addr, 4);
  
    if(hwregs == NULL)
    {
        printk(KERN_INFO "hantr_cache: failed to ioremap HW regs\n");
        release_mem_region(base_addr, 4);
        return -1;
    }

    *hwid = readl(hwregs + 0x00);
    printk(KERN_INFO "hantro_cache: hwid = %x, base_addr= %p\n",(int)*hwid,(void *)base_addr);

    if(hwregs)
       iounmap((void *)hwregs);
    release_mem_region(base_addr, 4);

    return 0;
}

static int ReserveIO(void)
{
    int hwid;
    int i;
    int hw_found = -1;
    int version_found = 0; /*CJ: useless */ 

    for (i=0;i<total_core_num;i++)
    {
      int hw_cfg = 0;
      if (cache_get_hwid(cache_data[i].core_cfg.base_addr, &hwid) < 0)
        continue;

      if ((((hwid & 0xF000) >> 12) == 0) || (((hwid & 0xF000) >> 12) > 0x5)) {
        ReleaseIO();
        printk(KERN_INFO "hantr_cache: Unknown HW found at 0x%16lx\n",
               cache_data[i].core_cfg.base_addr); 
        return -1;
      }
      hw_cfg = (hwid & 0xF0000)>>16;
      if (!version_found) {
        g_hwid = hwid;
        version_found = 1;
      }
      if (hw_cfg > 2)
        continue;

      if (hw_cfg == 1 && cache_data[i].core_cfg.dir == DIR_WR)//cache only
         cache_data[i].is_valid = 0;
      else if (hw_cfg == 2 && cache_data[i].core_cfg.dir == DIR_RD)//shaper only
         cache_data[i].is_valid = 0;
      else
         cache_data[i].is_valid = 1;

      if (cache_data[i].is_valid == 0)
         continue;

      if (hwid == 0 && cache_data[i].core_cfg.dir == DIR_RD)
         cache_data[i].core_cfg.base_addr += CACHE_WITH_SHAPER_OFFSET;
      else if (hwid != 0)
      {
        if (cache_data[i].core_cfg.dir == DIR_WR)
          cache_data[i].core_cfg.base_addr += SHAPER_OFFSET;
        else if (cache_data[i].core_cfg.dir == DIR_RD && hw_cfg == 0)
          cache_data[i].core_cfg.base_addr += CACHE_WITH_SHAPER_OFFSET;
        else if (cache_data[i].core_cfg.dir == DIR_RD && hw_cfg == 1)
          cache_data[i].core_cfg.base_addr += CACHE_ONLY_OFFSET;
      }
      
      if(!request_mem_region
         (cache_data[i].core_cfg.base_addr, cache_data[i].core_cfg.iosize, "hantro_cache"))
      {
          printk(KERN_INFO "hantr_cache: failed to reserve HW regs,core:%d\n",i);
          cache_data[i].is_valid = 0;
          continue;
      }
      cache_data[i].hwregs =
          (volatile u8 *) ioremap_nocache(cache_data[i].core_cfg.base_addr,
                                          cache_data[i].core_cfg.iosize);
  
      if(cache_data[i].hwregs == NULL)
      {
          printk(KERN_INFO "hantr_cache: failed to ioremap HW regs,core:%d\n",i);
          release_mem_region(cache_data[i].core_cfg.base_addr, cache_data[i].core_cfg.iosize);
          cache_data[i].is_valid = 0;
          continue;
      }

      if (cache_data[i].core_cfg.dir == DIR_RD)
        PDEBUG("cache  reg[0x10]=%08x\n",readl(cache_data[i].hwregs + 0x10));
      else
        PDEBUG("shaper reg[0x08]=%08x\n",readl(cache_data[i].hwregs + 0x08));

      hw_found = 1;
    }

    return hw_found;
}

static void ReleaseIO(void)
{
    u32 i;
    for (i=0;i<=total_core_num;i++)
    {
     if (cache_data[i].is_valid == 0)
       continue;
     if(cache_data[i].hwregs)
       iounmap((void *) cache_data[i].hwregs);
     release_mem_region(cache_data[i].core_cfg.base_addr, cache_data[i].core_cfg.iosize);
    }
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
irqreturn_t cache_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t cache_isr(int irq, void *dev_id)
#endif
{
    unsigned int handled = 0;
    cache_dev_t *dev = (cache_dev_t *) dev_id;
    u32 irq_status;
    unsigned long flags;
    u32 irq_triggered = 0;

    /*If core is not reserved by any user, but irq is received, just ignore it*/
    spin_lock_irqsave(&owner_lock, flags);
    if (!dev->is_reserved)
      return IRQ_HANDLED;
    spin_unlock_irqrestore(&owner_lock, flags);
    
    if (dev->core_cfg.dir == DIR_RD)
    {
      irq_status = readl(dev->hwregs + 0x04);
      if (irq_status&0x28)
      {
        irq_triggered = 1;
        writel(irq_status,dev->hwregs + 0x04);//clear irq
      }
    }
    else
    {
      irq_status = readl(dev->hwregs + 0x0C);
      if (irq_status)
      {
        irq_triggered = 1;
        writel(irq_status,dev->hwregs + 0x0C);//clear irq
      }
    }
    if(irq_triggered == 1)//irq has been triggered
    {
      /* clear all IRQ bits. IRQ is cleared by writting 1 */
      spin_lock_irqsave(&owner_lock, flags);
      dev->irq_received = 1;
      dev->irq_status = irq_status;
      spin_unlock_irqrestore(&owner_lock, flags);

      wake_up_interruptible_all(&enc_wait_queue);
      handled++;
    }
    if(!handled)
    {
      PDEBUG("IRQ received, but not cache's!\n");
    }
    return IRQ_HANDLED;
}

void ResetAsic(cache_dev_t * dev)
{
    int i,n;
    for (n=0;n<total_core_num;n++)
    {
     if (dev[n].is_valid==0)
        continue;
     for(i = 0; i < dev[n].core_cfg.iosize; i += 4)
     {
         writel(0, dev[n].hwregs + i);
     }
    }
}
#if 0
void dump_regs(unsigned long data)
{
    cache_dev_t *dev = (cache_dev_t *) data;
    int i;

    PDEBUG("Reg Dump Start\n");
    for(i = 0; i < dev->core_cfg.iosize; i += 4)
    {
        PDEBUG("\toffset %02X = %08X\n", i, readl(dev->hwregs + i));
    }
    PDEBUG("Reg Dump End\n");
}
#endif
module_init(cache_init);
module_exit(cache_cleanup);

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("Hantro Cache driver");

