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
--  Abstract : H2 Encoder device driver (kernel module)
--
------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range 
    SetPageReserved
    ClearPageReserved
*/
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>

#include <linux/moduleparam.h>
/* request_irq(), free_irq() */
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/semaphore.h>
#include <linux/spinlock.h>
/* needed for virt_to_phys() */
#include <asm/io.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <asm/irq.h>

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/uaccess.h>


/* our own stuff */
#include "hx280enc.h"

/********variables declaration related with race condition**********/

struct semaphore enc_core_sem;
DECLARE_WAIT_QUEUE_HEAD(hw_queue);
DEFINE_SPINLOCK(owner_lock);
DECLARE_WAIT_QUEUE_HEAD(enc_wait_queue);

/*------------------------------------------------------------------------
*****************************PORTING LAYER********************************
-------------------------------------------------------------------------*/
#define RESOURCE_SHARED_INTER_SUBSYS        0     /*0:no resource sharing inter subsystems 1: existing resource sharing*/
#define SUBSYS_0_IO_ADDR                 0x90000   /*customer specify according to own platform*/
#define SUBSYS_0_IO_SIZE                 (40000 * 4)    /* bytes */

#define SUBSYS_1_IO_ADDR                 0xA0000   /*customer specify according to own platform*/
#define SUBSYS_1_IO_SIZE                 (20000 * 4)    /* bytes */

#define INT_PIN_SUBSYS_0_VC8000E                    -1
#define INT_PIN_SUBSYS_0_CUTREE                     -1
#define INT_PIN_SUBSYS_0_DEC400                     -1
#define INT_PIN_SUBSYS_1_VC8000E                    -1
#define INT_PIN_SUBSYS_1_CUTREE                     -1
#define INT_PIN_SUBSYS_1_DEC400                     -1


/*for all subsystem, the subsys info should be listed here for subsequent use*/
/*base_addr, iosize, resource_shared*/
SUBSYS_CONFIG subsys_array[]= {
    {SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_0
    //{SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_1
};

/*here config every core in all subsystem*/
/*subsys_idx, core_type, offset, reg_size, irq*/
CORE_CONFIG core_array[]= {
    {0, CORE_VC8000E, 0, 500 * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_VC8000E
    //{0, CORE_DEC400, 0x10000, 1600 * 4, INT_PIN_SUBSYS_0_DEC400}, //subsys_0_DEC400
    //{0, CORE_CUTREE, 0x20000, 500 * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_0_CUTREE
    //{1, CORE_VC8000E, 0, 500 * 4, INT_PIN_SUBSYS_1_VC8000E} //subsys_1_VC8000E

};
/*------------------------------END-------------------------------------*/

/***************************TYPE AND FUNCTION DECLARATION****************/

/* here's all the must remember stuff */
typedef struct
{
    SUBSYS_DATA subsys_data; //config of each core,such as base addr, iosize,etc
    u32 hw_id; //VC8000E/VC8000EJ hw id to indicate project
    u32 subsys_id; //subsys id for driver and sw internal use
    u32 is_valid; //indicate this subsys is hantro's core or not
    int pid[CORE_MAX]; //indicate which process is occupying the subsys
    u32 is_reserved[CORE_MAX]; //indicate this subsys is occupied by user or not
    u32 irq_received[CORE_MAX]; //indicate which core receives irq
    u32 irq_status[CORE_MAX]; //IRQ status of each core 
    char *buffer;
    unsigned int buffsize;
    volatile u8 *hwregs;
    struct fasync_struct *async_queue;
} hantroenc_t;

static int ReserveIO(void);
static void ReleaseIO(void);
//static void ResetAsic(hantroenc_t * dev);
int CheckCoreOccupation(hantroenc_t *dev, u8 core_type);

#ifdef hantroenc_DEBUG
static void dump_regs(unsigned long data);
#endif

/* IRQ handler */
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
static irqreturn_t hantroenc_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hantroenc_isr(int irq, void *dev_id);
#endif

/*********************local variable declaration*****************/
unsigned long sram_base = 0;
unsigned int sram_size = 0;
/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hantroenc_major = 0;
static int total_subsys_num = 0;
static int total_core_num = 0;
volatile unsigned int asic_status = 0;
/* dynamic allocation*/
static hantroenc_t* hantroenc_data = NULL;
unsigned int pcie = 0;          /* used in hantro_mmu.c*/

/******************************************************************************/
static int CheckEncIrq(hantroenc_t *dev,u32 *core_info,u32 *irq_status)
{
    unsigned long flags;
    int rdy = 0;
    u8 core_type = 0;
    u8 subsys_idx = 0;
  
    core_type = (u8)(*core_info & 0x0F);
    subsys_idx = (u8)(*core_info >> 4);
    
    if (subsys_idx > total_subsys_num-1)
    {
      *core_info = -1;
      *irq_status = 0;
      return 1;
    }
    
    spin_lock_irqsave(&owner_lock, flags);

    if(dev[subsys_idx].irq_received[core_type])
    {
     /* reset the wait condition(s) */
     PDEBUG("check subsys[%d][%d] irq ready\n", subsys_idx, core_type);
     dev[subsys_idx].irq_received[core_type] = 0;
     rdy = 1;
     *core_info = subsys_idx;
     *irq_status = dev[subsys_idx].irq_status[core_type];
    }

    spin_unlock_irqrestore(&owner_lock, flags);
    
    return rdy;
}
unsigned int WaitEncReady(hantroenc_t *dev,u32 *core_info,u32 *irq_status)
{
   PDEBUG("WaitEncReady\n");

   if(wait_event_interruptible(enc_wait_queue, CheckEncIrq(dev,core_info,irq_status)))
   {
       PDEBUG("ENC wait_event_interruptible interrupted\n");
       return -ERESTARTSYS;
   }

   return 0;
}

int CheckCoreOccupation(hantroenc_t *dev, u8 core_type) 
{
  int ret = 0;
  unsigned long flags;

  spin_lock_irqsave(&owner_lock, flags);
  if(!dev->is_reserved[core_type]) {
    dev->is_reserved[core_type] = 1;
    dev->pid[core_type] = current->pid;
    ret = 1;
    PDEBUG("CheckCoreOccupation pid=%d\n",dev->pid[core_type]);
  }

  spin_unlock_irqrestore(&owner_lock, flags);

  return ret;
}

int GetWorkableCore(hantroenc_t *dev,u32 *core_info,u32 *core_info_tmp) 
{
  int ret = 0;
  u32 i = 0;
  u32 cores;
  u8 core_type = 0;
  u32 required_num = 0;
  /*input core_info[32 bit]: mode[1bit](1:all 0:specified)+amount[3bit](the needing amount -1)+reserved+core_type[8bit]
  
    output core_info[32 bit]: the reserved core info to user space and defined as below.
    mode[1bit](1:all 0:specified)+amount[3bit](reserved total core num)+reserved+subsys_mapping[8bit]
   */
  cores = *core_info;
  required_num = ((cores >> CORE_INFO_AMOUNT_OFFSET)& 0x7)+1;
  core_type = (u8)(cores&0xFF);

  if (*core_info_tmp == 0)
    *core_info_tmp = required_num << CORE_INFO_AMOUNT_OFFSET;
  else
    required_num = (*core_info_tmp >> CORE_INFO_AMOUNT_OFFSET);
  
  PDEBUG("GetWorkableCore:required_num=%d,core_info=%x\n",required_num,*core_info);

  if(required_num)
  {
    /* a valid free Core with specified core type */
    for (i = 0; i < total_subsys_num; i++)
    {
      if (dev[i].subsys_data.core_info.type_info & (1 << core_type))
      {
         if(dev[i].is_valid && CheckCoreOccupation(&dev[i], core_type))
         {
          *core_info_tmp = ((((*core_info_tmp >> CORE_INFO_AMOUNT_OFFSET)-1)<<CORE_INFO_AMOUNT_OFFSET)|(*core_info_tmp & 0x0FF)); 
          *core_info_tmp = (*core_info_tmp | (1 << i));
          if ((*core_info_tmp >> CORE_INFO_AMOUNT_OFFSET)==0)
          {
           ret = 1;
           *core_info = (*core_info&0xFFFFFF00)|(*core_info_tmp & 0xFF);
           *core_info_tmp = 0;
           required_num = 0;
           break;
          }
         } 
      }
    }
  }
  else
    ret = 1;
  
  PDEBUG("*core_info = %x\n",*core_info);
  return ret;
}

long ReserveEncoder(hantroenc_t *dev,u32 *core_info) 
{
  u32 core_info_tmp = 0;
  /*If HW resources are shared inter cores, just make sure only one is using the HW*/
  if (dev[0].subsys_data.cfg.resouce_shared)
    {
     if (down_interruptible(&enc_core_sem))
       return -ERESTARTSYS;
    }
  
  /* lock a core that has specified core id*/
  if(wait_event_interruptible(hw_queue,
                              GetWorkableCore(dev,core_info,&core_info_tmp) != 0 ))
    return -ERESTARTSYS;

  return 0;
}

void ReleaseEncoder(hantroenc_t * dev,u32 *core_info)
{
  unsigned long flags;
  u8 core_type = 0, subsys_idx = 0;

  subsys_idx = (u8)((*core_info&0xF0) >> 4);
  core_type = (u8)(*core_info&0x0F);

  PDEBUG("ReleaseEncoder:subsys_idx=%d,core_type=%x\n",subsys_idx,core_type);
  /* release specified subsys and core type */

  if (dev[subsys_idx].subsys_data.core_info.type_info & (1 << core_type))
  {
     spin_lock_irqsave(&owner_lock, flags);
     PDEBUG("subsys[%d].pid[%d]=%d,current->pid=%d\n",subsys_idx, core_type, dev[subsys_idx].pid[core_type],current->pid);
     if(dev[subsys_idx].is_reserved[core_type] && dev[subsys_idx].pid[core_type] == current->pid)
      {
       dev[subsys_idx].pid[core_type] = -1;
       dev[subsys_idx].is_reserved[core_type] = 0;
       dev[subsys_idx].irq_received[core_type] = 0;
       dev[subsys_idx].irq_status[core_type] = 0;
      }
     else if (dev[subsys_idx].pid[core_type] != current->pid)
       printk(KERN_ERR "WARNING:pid(%d) is trying to release core reserved by pid(%d)\n",current->pid,dev[subsys_idx].pid[core_type]);
   
     spin_unlock_irqrestore(&owner_lock, flags);
   
     //wake_up_interruptible_all(&hw_queue);
  }
  
  wake_up_interruptible_all(&hw_queue);
  
  if(dev->subsys_data.cfg.resouce_shared)
    up(&enc_core_sem);

  return;
}

static long hantroenc_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int err = 0;

    unsigned int tmp;
    PDEBUG("ioctl cmd 0x%08ux\n", cmd);
    /*
     * extract the type and number bitfields, and don't encode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if(_IOC_TYPE(cmd) != HX280ENC_IOC_MAGIC 
#ifdef HANTROMMU_SUPPORT
        &&_IOC_TYPE(cmd) != HANTRO_IOC_MMU
#endif
       )
        return -ENOTTY;
    if((_IOC_TYPE(cmd) == HX280ENC_IOC_MAGIC &&
        _IOC_NR(cmd) > HX280ENC_IOC_MAXNR) 
#ifdef HANTROMMU_SUPPORT
       ||(_IOC_TYPE(cmd) == HANTRO_IOC_MMU &&
        _IOC_NR(cmd) > HANTRO_IOC_MMU_MAXNR)
#endif
        )
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
    case HX280ENC_IOCGHWOFFSET:
        {
         u32 id;
         __get_user(id, (u32*)arg);

         if(id >= total_subsys_num) 
         {
          return -EFAULT;
         }
         __put_user(hantroenc_data[id].subsys_data.cfg.base_addr, (unsigned long *) arg);
         break;
        }

    case HX280ENC_IOCGHWIOSIZE:
        {
         u32 id;
         u32 io_size;
         __get_user(id, (u32*)arg);
         
         if(id >= total_subsys_num) 
         {
          return -EFAULT;
         }
         io_size = hantroenc_data[id].subsys_data.cfg.iosize;
         __put_user(io_size, (u32 *) arg);
   
         return 0;
        }
    case HX280ENC_IOCGSRAMOFFSET:
        __put_user(sram_base, (unsigned long *) arg);
        break;
    case HX280ENC_IOCGSRAMEIOSIZE:
        __put_user(sram_size, (unsigned int *) arg);
        break;
    case HX280ENC_IOCG_CORE_NUM:
        __put_user(total_subsys_num, (unsigned int *) arg);
        break;
    case HX280ENC_IOCG_CORE_INFO:
        {
          u32 idx;
          SUBSYS_CORE_INFO in_data;
          if (0 != copy_from_user(&in_data, (void*)arg, sizeof(SUBSYS_CORE_INFO)))
            return -1; /* CJ add check return value */
          idx = in_data.type_info;
          if (idx > total_subsys_num - 1)
            return -1;

          if (0 != copy_to_user((void*)arg, &hantroenc_data[idx].subsys_data.core_info, sizeof(SUBSYS_CORE_INFO)))
            return -1; /*CJ add check return value */
          break;
        }
    case HX280ENC_IOCH_ENC_RESERVE: 
        {
         u32 core_info;
         int ret;
         PDEBUG("Reserve ENC Cores\n");
         __get_user(core_info, (u32*)arg);
         ret = ReserveEncoder(hantroenc_data,&core_info);
         if (ret == 0)
            __put_user(core_info, (u32 *) arg);
         return ret;
        }
    case HX280ENC_IOCH_ENC_RELEASE: 
        {
         u32 core_info;
         __get_user(core_info, (u32*)arg);
     
         PDEBUG("Release ENC Core\n");
     
         ReleaseEncoder(hantroenc_data,&core_info);
     
         break;
        }
        
    case HX280ENC_IOCG_CORE_WAIT:
        {
         u32 core_info;
         u32 irq_status;
         __get_user(core_info, (u32*)arg);
  
         tmp = WaitEncReady(hantroenc_data,&core_info,&irq_status);
         if (tmp==0)
         {
           __put_user(irq_status, (unsigned int *)arg);
           return core_info;//return core_id
         }
         else
         {
           __put_user(0, (unsigned int *)arg);  
           return -1;
         }
         
         break;
        }
    default:
        {
#ifdef HANTROMMU_SUPPORT
         u32 VC8000E_core_idx = GET_ENCODER_IDX(hantroenc_data[0].subsys_data.core_info.type_info);
         if(_IOC_TYPE(cmd) == HANTRO_IOC_MMU) 
           return (MMUIoctl(cmd, filp, arg, hantroenc_data[0].hwregs + hantroenc_data[0].subsys_data.core_info.offset[VC8000E_core_idx]));
#endif
        }
    }
    return 0;
}

static int hantroenc_open(struct inode *inode, struct file *filp)
{
    int result = 0;
    hantroenc_t *dev = hantroenc_data;

    filp->private_data = (void *) dev;

    PDEBUG("dev opened\n");
    return result;
}
static int hantroenc_release(struct inode *inode, struct file *filp)
{
    hantroenc_t *dev = (hantroenc_t *) filp->private_data;
    u32 core_id = 0, i = 0;
    
#ifdef hantroenc_DEBUG
    dump_regs((unsigned long) dev); /* dump the regs */
#endif
    unsigned long flags;

    PDEBUG("dev closed\n");

    for (i = 0;i < total_subsys_num; i++)
    {
      for (core_id = 0; core_id < CORE_MAX; core_id++)
      {
        spin_lock_irqsave(&owner_lock, flags);
        if (dev[i].is_reserved[core_id] == 1 && dev[i].pid[core_id] == current->pid)
        {
          dev[i].pid[core_id] = -1;
          dev[i].is_reserved[core_id] = 0;
          dev[i].irq_received[core_id] = 0;
          dev[i].irq_status[core_id] = 0;
          PDEBUG("release reserved core\n");
        }
        spin_unlock_irqrestore(&owner_lock, flags);
      }
    }   
    
#ifdef HANTROMMU_SUPPORT
    u32 VC8000E_core_idx = GET_ENCODER_IDX(hantroenc_data[0].subsys_data.core_info.type_info);
    MMURelease(filp,hantroenc_data[0].hwregs + hantroenc_data[0].subsys_data.core_info.offset[VC8000E_core_idx]));
#endif

    wake_up_interruptible_all(&hw_queue);
  
    if(dev->subsys_data.cfg.resouce_shared)
      up(&enc_core_sem);
  
    return 0;
}

/* VFS methods */
static struct file_operations hantroenc_fops = {
    .owner= THIS_MODULE,
    .open = hantroenc_open,
    .release = hantroenc_release,
    .unlocked_ioctl = hantroenc_ioctl,
    .fasync = NULL,
};

int __init hantroenc_init(void)
{
    int result = 0;
    int i, j;

    total_subsys_num = sizeof(subsys_array)/sizeof(SUBSYS_CONFIG);
    for (i = 0; i< total_subsys_num; i++)
    {
      printk(KERN_INFO "hantroenc: module init - subsys[%d] addr =%p\n",i,
             (void *)subsys_array[i].base_addr);
    }

    hantroenc_data = (hantroenc_t *)vmalloc(sizeof(hantroenc_t)*total_subsys_num);
    if (hantroenc_data == NULL)
      goto err1;
    memset(hantroenc_data,0,sizeof(hantroenc_t)*total_subsys_num);

    for(i = 0; i < total_subsys_num; i++)
    {
      hantroenc_data[i].subsys_data.cfg = subsys_array[i];
      hantroenc_data[i].async_queue = NULL;
      hantroenc_data[i].hwregs = NULL;
      hantroenc_data[i].subsys_id = i;
      for(j = 0; j < CORE_MAX; j++)
        hantroenc_data[i].subsys_data.core_info.irq[j] = -1;
    }

    total_core_num = sizeof(core_array)/sizeof(CORE_CONFIG);
    for (i = 0; i < total_core_num; i++)
    {
      hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.type_info |= (1<<(core_array[i].core_type));
      hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.offset[core_array[i].core_type] = core_array[i].offset;
      hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.regSize[core_array[i].core_type] = core_array[i].reg_size;
      hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.irq[core_array[i].core_type] = core_array[i].irq;
    }

    result = register_chrdev(hantroenc_major, "hx280enc", &hantroenc_fops);
    if(result < 0)
    {
        printk(KERN_INFO "hantroenc: unable to get major <%d>\n",
               hantroenc_major);
        goto err1;
    }
    else if(result != 0)    /* this is for dynamic major */
    {
        hantroenc_major = result;
    }

    result = ReserveIO();
    if(result < 0)
    {
        goto err;
    }

    //ResetAsic(hantroenc_data);  /* reset hardware */

    sema_init(&enc_core_sem, 1);

#ifdef HANTROMMU_SUPPORT
    u32 VC8000E_core_idx = GET_ENCODER_IDX(hantroenc_data[0].subsys_data.core_info.type_info);
    result = MMUInit(hantroenc_data[0].hwregs + hantroenc_data[i].subsys_data.core_info.offset[VC8000E_core_idx]);
    if(result == MMU_STATUS_NOT_FOUND)
      printk(KERN_INFO "MMU does not exist!\n");
    else if(result != MMU_STATUS_OK)
    {
      ReleaseIO();
      goto err;
    }
#endif

    /* get the IRQ line */
    for (i=0;i<total_subsys_num;i++)
    {
      if (hantroenc_data[i].is_valid==0)
        continue;

      for (j = 0; j < CORE_MAX; j++)
      {
        if(hantroenc_data[i].subsys_data.core_info.irq[j]!= -1)
        {
            result = request_irq(hantroenc_data[i].subsys_data.core_info.irq[j], hantroenc_isr,
   #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
                               SA_INTERRUPT | SA_SHIRQ,
   #else
                               IRQF_SHARED,
   #endif
                               "hx280enc", (void *) &hantroenc_data[i]);
          if(result == -EINVAL)
          {
              printk(KERN_ERR "hantroenc: Bad irq number or handler\n");
              ReleaseIO();
              goto err;
          }
          else if(result == -EBUSY)
          {
              printk(KERN_ERR "hantroenc: IRQ <%d> busy, change your config\n",
                       hantroenc_data[i].subsys_data.core_info.irq[j]);
              ReleaseIO();
              goto err;
          }
        }
        else
        {
          printk(KERN_INFO "hantroenc: IRQ not in use!\n");
        }
      }
    }
    printk(KERN_INFO "hantroenc: module inserted. Major <%d>\n", hantroenc_major);

    return 0;

  err:
    unregister_chrdev(hantroenc_major, "hx280enc");
  err1:
    if (hantroenc_data != NULL)
      vfree(hantroenc_data);
    printk(KERN_INFO "hantroenc: module not inserted\n");
    return result;
}

void __exit hantroenc_cleanup(void)
{
  int i=0, j = 0;
  for(i=0;i<total_subsys_num;i++)
  {
    if (hantroenc_data[i].is_valid==0)
      continue;
    //writel(0, hantroenc_data[i].hwregs + 0x14); /* disable HW */
    //writel(0, hantroenc_data[i].hwregs + 0x04); /* clear enc IRQ */

    /* free the core IRQ */
    for (j = 0; j < total_core_num; j++)
    {
      if(hantroenc_data[i].subsys_data.core_info.irq[j] != -1)
      {
        free_irq(hantroenc_data[i].subsys_data.core_info.irq[j], (void *)&hantroenc_data[i]);
      }
    }
   }

#ifdef HANTROMMU_SUPPORT
  u32 VC8000E_core_idx = GET_ENCODER_IDX(hantroenc_data[0].subsys_data.core_info.type_info);
  MMUCleanup(hantroenc_data[0].hwregs + hantroenc_data[i].subsys_data.core_info.offset[VC8000E_core_idx]);
#endif

  ReleaseIO();
  vfree(hantroenc_data);

  unregister_chrdev(hantroenc_major, "hx280enc");

  printk(KERN_INFO "hantroenc: module removed\n");
  return;
}

static int ReserveIO(void)
{
    u32 hwid;
    int i;
    u32 found_hw = 0;
    u32 VC8000E_core_idx;

    for (i=0;i<total_subsys_num;i++)
    {
      if(!request_mem_region
         (hantroenc_data[i].subsys_data.cfg.base_addr, hantroenc_data[i].subsys_data.cfg.iosize, "hx280enc"))
      {
          printk(KERN_INFO "hantroenc: failed to reserve HW regs\n");
          continue;
      }
  
      hantroenc_data[i].hwregs =
          (volatile u8 *) ioremap_nocache(hantroenc_data[i].subsys_data.cfg.base_addr,
                                          hantroenc_data[i].subsys_data.cfg.iosize);
  
      if(hantroenc_data[i].hwregs == NULL)
      {
          printk(KERN_INFO "hantroenc: failed to ioremap HW regs\n");
          ReleaseIO();
          continue;
      }

      /*read hwid and check validness and store it*/
      VC8000E_core_idx = GET_ENCODER_IDX(hantroenc_data[0].subsys_data.core_info.type_info);
      hwid = (u32)ioread32((void *)hantroenc_data[i].hwregs + hantroenc_data[i].subsys_data.core_info.offset[VC8000E_core_idx]);
      printk(KERN_INFO"hwid=0x%08x\n", hwid);
 
      /* check for encoder HW ID */
      if( ((((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID1 >> 16) & 0xFFFF))) &&
       ((((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID2 >> 16) & 0xFFFF))))
      {
          printk(KERN_INFO "hantroenc: HW not found at %p\n",
                 (void *)hantroenc_data[i].subsys_data.cfg.base_addr);
  #ifdef hantroenc_DEBUG
          dump_regs((unsigned long) &hantroenc_data);
  #endif
          hantroenc_data[i].is_valid = 0;
          ReleaseIO();
          continue;
      }
      hantroenc_data[i].hw_id = hwid;
      hantroenc_data[i].is_valid = 1;
      found_hw = 1;
      
      printk(KERN_INFO
             "hantroenc: HW at base <%p> with ID <0x%08x>\n",
             (void *)hantroenc_data[i].subsys_data.cfg.base_addr, hwid);

    }

    if (found_hw == 0)
    {
      printk(KERN_ERR "hantroenc: NO ANY HW found!!\n");
      return -1;
    }

    return 0;
}

static void ReleaseIO(void)
{
    u32 i;
    for (i=0;i<=total_subsys_num;i++)
    {
     if (hantroenc_data[i].is_valid == 0)
        continue;
     if(hantroenc_data[i].hwregs)
         iounmap((void *) hantroenc_data[i].hwregs);
     release_mem_region(hantroenc_data[i].subsys_data.cfg.base_addr, hantroenc_data[i].subsys_data.cfg.iosize);
    }
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
irqreturn_t hantroenc_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t hantroenc_isr(int irq, void *dev_id)
#endif
{
    unsigned int handled = 0;
    hantroenc_t *dev = (hantroenc_t *) dev_id;
    u32 irq_status;
    unsigned long flags;
    u32 core_type = 0, i = 0;
    unsigned long reg_offset = 0;
    u32 hwId, majorId, wClr;

    /*get core id by irq from subsys config*/
    for (i = 0; i < CORE_MAX; i++)
    {
      if (dev->subsys_data.core_info.irq[i] == irq)
      {
        core_type = i;
        reg_offset = dev->subsys_data.core_info.offset[i];
        break;
      }
    }

    /*If core is not reserved by any user, but irq is received, just clean it*/
    spin_lock_irqsave(&owner_lock, flags);
    if (!dev->is_reserved[core_type])
    {
      printk(KERN_DEBUG "hantroenc_isr:received IRQ but core is not reserved!\n");
      irq_status = (u32)ioread32((void *)(dev->hwregs + reg_offset + 0x04));
      if(irq_status & 0x01)
      {
          /*  Disable HW when buffer over-flow happen
            *  HW behavior changed in over-flow
            *    in-pass, HW cleanup HWIF_ENC_E auto
            *    new version:  ask SW cleanup HWIF_ENC_E when buffer over-flow
            */
          if(irq_status & 0x20)
              iowrite32(0, (void *)(dev->hwregs + reg_offset + 0x14));

        /* clear all IRQ bits. (hwId >= 0x80006100) means IRQ is cleared by writting 1 */
        hwId = ioread32((void *)dev->hwregs + reg_offset);
        majorId = (hwId & 0x0000FF00) >> 8; 
        wClr = (majorId >= 0x61) ? irq_status: (irq_status & (~0x1FD));
        iowrite32(wClr, (void *)(dev->hwregs + reg_offset + 0x04));
      }
      spin_unlock_irqrestore(&owner_lock, flags);
      return IRQ_HANDLED;
    }
    spin_unlock_irqrestore(&owner_lock, flags);

    printk(KERN_DEBUG "hantroenc_isr:received IRQ!\n");
    irq_status = (u32)ioread32((void *)(dev->hwregs + reg_offset + 0x04));
    printk(KERN_DEBUG "irq_status of subsys %d core %d is:%x\n",dev->subsys_id,core_type,irq_status);
    if(irq_status & 0x01)
    {
        /*  Disable HW when buffer over-flow happen
          *  HW behavior changed in over-flow
          *    in-pass, HW cleanup HWIF_ENC_E auto
          *    new version:  ask SW cleanup HWIF_ENC_E when buffer over-flow
          */
        if(irq_status & 0x20)
            iowrite32(0, (void *)(dev->hwregs + reg_offset + 0x14));

        /* clear all IRQ bits. (hwId >= 0x80006100) means IRQ is cleared by writting 1 */
        hwId = ioread32((void *)dev->hwregs + reg_offset);
        majorId = (hwId & 0x0000FF00) >> 8; 
        wClr = (majorId >= 0x61) ? irq_status: (irq_status & (~0x1FD));
        iowrite32(wClr, (void *)(dev->hwregs + reg_offset + 0x04));
        
        spin_lock_irqsave(&owner_lock, flags);
        dev->irq_received[core_type] = 1;
        dev->irq_status[core_type] = irq_status & (~0x01);
        spin_unlock_irqrestore(&owner_lock, flags);
        
        wake_up_interruptible_all(&enc_wait_queue);
        handled++;
    }
    if(!handled)
    {
        PDEBUG("IRQ received, but not hantro's!\n");
    }
    return IRQ_HANDLED;
}

#ifdef hantroenc_DEBUG
void ResetAsic(hantroenc_t * dev)
{
    int i,n;
    for (n=0;n<total_subsys_num;n++)
    {
     if (dev[n].is_valid==0)
        continue;
     iowrite32(0, (void *)(dev[n].hwregs + 0x14));
     for(i = 4; i < dev[n].subsys_data.cfg.iosize; i += 4)
     {
         iowrite32(0, (void *)(dev[n].hwregs + i));
     }
    }
}

void dump_regs(unsigned long data)
{
    hantroenc_t *dev = (hantroenc_t *) data;
    int i;

    PDEBUG("Reg Dump Start\n");
    for(i = 0; i < dev->iosize; i += 4)
    {
        PDEBUG("\toffset %02X = %08X\n", i, ioread32(dev->hwregs + i));
    }
    PDEBUG("Reg Dump End\n");
}
#endif

module_init(hantroenc_init);
module_exit(hantroenc_cleanup);

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("Hantro Encoder driver");

