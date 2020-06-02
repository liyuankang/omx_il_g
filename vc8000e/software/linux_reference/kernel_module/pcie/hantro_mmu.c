#include <linux/version.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <stddef.h>
#include "hantrommu.h"

MODULE_DESCRIPTION("Verisilicon VPU Driver");
MODULE_LICENSE("GPL");

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#define HXDEC_MAX_CORES          4

#define MMU_REG_OFFSET              512*4
#define MMU_REG_FLUSH               MMU_REG_OFFSET + 4*4
#define MMU_REG_PAGE_TABLE_ID       MMU_REG_OFFSET + 5*4
#define MMU_REG_CONTROL             MMU_REG_OFFSET + 8*4
#define MMU_REG_ADDRESS             MMU_REG_OFFSET + 9*4

/*******************************************************************************
***** New MMU Defination *******************************************************/
#define MMU_MTLB_SHIFT           22
#define MMU_STLB_4K_SHIFT        12
#define MMU_STLB_64K_SHIFT       16

#define MMU_MTLB_BITS            (32 - MMU_MTLB_SHIFT)
#define MMU_PAGE_4K_BITS         MMU_STLB_4K_SHIFT
#define MMU_STLB_4K_BITS         (32 - MMU_MTLB_BITS - MMU_PAGE_4K_BITS)
#define MMU_PAGE_64K_BITS        MMU_STLB_64K_SHIFT
#define MMU_STLB_64K_BITS        (32 - MMU_MTLB_BITS - MMU_PAGE_64K_BITS)

#define MMU_MTLB_ENTRY_NUM       (1 << MMU_MTLB_BITS)
#define MMU_MTLB_SIZE            (MMU_MTLB_ENTRY_NUM << 2)
#define MMU_STLB_4K_ENTRY_NUM    (1 << MMU_STLB_4K_BITS)
#define MMU_STLB_4K_SIZE         (MMU_STLB_4K_ENTRY_NUM << 2)
#define MMU_PAGE_4K_SIZE         (1 << MMU_STLB_4K_SHIFT)
#define MMU_STLB_64K_ENTRY_NUM   (1 << MMU_STLB_64K_BITS)
#define MMU_STLB_64K_SIZE        (MMU_STLB_64K_ENTRY_NUM << 2)
#define MMU_PAGE_64K_SIZE        (1 << MMU_STLB_64K_SHIFT)

#define MMU_MTLB_MASK            (~((1U << MMU_MTLB_SHIFT)-1))
#define MMU_STLB_4K_MASK         ((~0U << MMU_STLB_4K_SHIFT) ^ MMU_MTLB_MASK)
#define MMU_PAGE_4K_MASK         (MMU_PAGE_4K_SIZE - 1)
#define MMU_STLB_64K_MASK        ((~((1U << MMU_STLB_64K_SHIFT)-1)) ^ MMU_MTLB_MASK)
#define MMU_PAGE_64K_MASK        (MMU_PAGE_64K_SIZE - 1)

/* Page offset definitions. */
#define MMU_OFFSET_4K_BITS       (32 - MMU_MTLB_BITS - MMU_STLB_4K_BITS)
#define MMU_OFFSET_4K_MASK       ((1U << MMU_OFFSET_4K_BITS) - 1)
#define MMU_OFFSET_16K_BITS      (32 - MMU_MTLB_BITS - MMU_STLB_16K_BITS)
#define MMU_OFFSET_16K_MASK      ((1U << MMU_OFFSET_16K_BITS) - 1)

#define MMU_MTLB_ENTRY_HINTS_BITS 6
#define MMU_MTLB_ENTRY_STLB_MASK  (~((1U << MMU_MTLB_ENTRY_HINTS_BITS) - 1))

#define MMU_MTLB_PRESENT         0x00000001
#define MMU_MTLB_EXCEPTION       0x00000002
#define MMU_MTLB_4K_PAGE         0x00000000

#define MMU_STLB_PRESENT         0x00000001
#define MMU_STLB_EXCEPTION       0x00000002
#define MMU_STLB_4K_PAGE         0x00000000

#define MMU_FALSE                0
#define MMU_TRUE                 1

#define MMU_ERR_OS_FAIL          (0xffff)
#define MMU_EFAULT               MMU_ERR_OS_FAIL
#define MMU_ENOTTY               MMU_ERR_OS_FAIL

#define MMU_INFINITE             ((u32) ~0U)

#define MAX_NOPAGED_SIZE         0x20000
#define MMU_SUPPRESS_OOM_MESSAGE 1

#if MMU_SUPPRESS_OOM_MESSAGE
#define MMU_NOWARN __GFP_NOWARN
#else
#define MMU_NOWARN 0
#endif

#define MMU_IS_ERROR(status)         (status < 0)
#define MMU_NO_ERROR(status)         (status >= 0)
#define MMU_IS_SUCCESS(status)       (status == MMU_STATUS_OK)

#undef MMUDEBUG
#ifdef HANTROMMU_DEBUG
#  ifdef __KERNEL__
#    define MMUDEBUG(fmt, args...) printk( KERN_INFO "hantrommu: " fmt, ## args)
#  else
#    define MMUDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define MMUDEBUG(fmt, args...)
#endif

#define MMU_ON_ERROR(func) \
  do { \
    status = func; \
    if (MMU_IS_ERROR(status)){ \
      goto onerror; \
    } \
  }while (MMU_FALSE)

#define WritePageEntry(page_entry, entry_value) \
    *(unsigned int *)(page_entry) =(unsigned int)(entry_value)

#define ReadPageEntry(page_entry) *(unsigned int *)(page_entry)

enum MMURegion {
  MMU_REGION_IN,
  MMU_REGION_OUT,
  MMU_REGION_PRIVATE,
  MMU_REGION_PUB,

  MMU_REGION_COUNT
};

struct MMUNode {
  void                       *buf_virtual_address;
  int                        mtlb_start;
  int                        stlb_start;
  int                        mtlb_end;
  int                        stlb_end;
  unsigned int               page_count;
  int                        process_id;
  struct file*               filp;

  struct MMUNode             *next;
  struct MMUNode             *prev;
};

struct MMUDDRRegion {
  unsigned long long         physical_address;
  unsigned long long         virtual_address;
  unsigned int               page_count;

  void                       *node_mutex;
  struct MMUNode             *free_map_head;
  struct MMUNode             *map_head;
  struct MMUNode             *free_map_tail;
  struct MMUNode             *map_tail;
};

struct MMU {
  void                        *page_table_mutex;
  /* Master TLB information. */
  unsigned int                mtlb_size;
  unsigned long long          mtlb_physical;
  void                        *mtlb_virtual;
  unsigned int                mtlb_entries;

  int                         enabled;
  void                        *stlb_virtual[MAX_NOPAGED_SIZE/MMU_PAGE_4K_SIZE];
  struct MMUDDRRegion         region[MMU_REGION_COUNT];
  void                        *page_table_array;
};

static struct MMU *g_mmu       = NULL;
extern unsigned long gBaseDDRHw;
static unsigned int mmu_enable = MMU_FALSE;
extern unsigned int pcie;
static unsigned int region_in_mmu_start = REGION_IN_MMU_START;
static unsigned int region_in_mmu_end = REGION_IN_MMU_END;
static unsigned int region_out_mmu_start = REGION_OUT_MMU_START;
static unsigned int region_out_mmu_end = REGION_OUT_MMU_END;
static unsigned int region_private_mmu_start = REGION_PRIVATE_MMU_START;
static unsigned int region_private_mmu_end = REGION_PRIVATE_MMU_END;

static enum MMUStatus ZeroMemory(void *memory, unsigned int bytes) {
  memset(memory, 0, bytes);

  return MMU_STATUS_OK;
}

static enum MMUStatus AllocateMemory(unsigned int bytes, void **memory){
  void *pointer;
  enum MMUStatus status;

  if (bytes > MAX_NOPAGED_SIZE) {
    pointer = (void*) vmalloc(bytes);
    MMUDEBUG(" *****VMALLOC size*****%d\n", bytes);
  } else {
    pointer = (void*) kmalloc(bytes, GFP_KERNEL | MMU_NOWARN | __GFP_DMA32);
    MMUDEBUG(" *****KMALLOC size*****%d\n", bytes);
  }

  if (pointer == NULL) {
    /* Out of memory. */
    status = MMU_STATUS_OUT_OF_MEMORY;
    goto onerror;
  }

  /* Return pointer to the memory allocation. */
  *memory = pointer;

  return MMU_STATUS_OK;

onerror:
  /* Return the status. */
  return status;
}

static enum MMUStatus FreeMemory(void *memory) {
  /* Free the memory from the OS pool. */
  if (is_vmalloc_addr(memory)) {
    MMUDEBUG(" *****VFREE*****%p\n", memory);
    vfree(memory);
  } else {
    MMUDEBUG(" *****KFREE*****%p\n", memory);
    kfree(memory);
  }
  return MMU_STATUS_OK;
}

static enum MMUStatus DeleteNode(struct MMUNode **pp) {
  (*pp)->prev->next = (*pp)->next;
  (*pp)->next->prev = (*pp)->prev;

  MMUDEBUG(" *****DeleteNode size*****%d\n", (*pp)->page_count);
  FreeMemory(*pp);

  return MMU_STATUS_OK;
}

static enum MMUStatus MergeNode(struct MMUNode *h,
                                struct MMUNode **pp) {
  struct MMUNode *tmp0 = h->next;
  struct MMUNode *tmp1 = h->next;
  while(tmp0) {
    /* 1th step: find front contiguous memory node */
    if(tmp0->mtlb_end == (*pp)->mtlb_start &&
       tmp0->stlb_end == (*pp)->stlb_start) {
      tmp0->mtlb_end = (*pp)->mtlb_end;
      tmp0->stlb_end = (*pp)->stlb_end;
      tmp0->page_count += (*pp)->page_count;
      DeleteNode(pp);
      MMUDEBUG(" *****first merge to front. node size*****%d\n", tmp0->page_count);
      /* after merge to front contiguous memory node,
         find if there is behind contiguous memory node */
      while(tmp1) {
        /* merge */
        if(tmp1->mtlb_start == tmp0->mtlb_end &&
           tmp1->stlb_start == tmp0->stlb_end) {
          tmp1->mtlb_start  = tmp0->mtlb_start;
          tmp1->stlb_start  = tmp0->stlb_start;
          tmp1->page_count += tmp0->page_count;
          MMUDEBUG(" *****second merge to behind. node size*****%d\n", tmp1->page_count);
          DeleteNode(&tmp0);
          return MMU_STATUS_OK;
        }
        tmp1 = tmp1->next;
      }
      return MMU_STATUS_OK;
    /* 1th step: find behind contiguous memory node */
    } else if(tmp0->mtlb_start == (*pp)->mtlb_end &&
              tmp0->stlb_start == (*pp)->stlb_end) {
      tmp0->mtlb_start = (*pp)->mtlb_start;
      tmp0->stlb_start = (*pp)->stlb_start;
      tmp0->page_count += (*pp)->page_count;
      DeleteNode(pp);
      MMUDEBUG(" *****first merge to behind. node size*****%d\n", tmp0->page_count);
      /* after merge to behind contiguous memory node,
         find if there is front contiguous memory node */
      while(tmp1) {
        /* merge */
        if(tmp1->mtlb_end == tmp0->mtlb_start &&
           tmp1->stlb_end == tmp0->stlb_start) {
          tmp1->mtlb_end    = tmp0->mtlb_end;
          tmp1->stlb_end    = tmp0->stlb_end;
          tmp1->page_count += tmp0->page_count;
          MMUDEBUG(" *****second merge to front. node size*****%d\n", tmp1->page_count);
          DeleteNode(&tmp0);
          return MMU_STATUS_OK;
        }
        tmp1 = tmp1->next;
      }
      return MMU_STATUS_OK;
    }
    tmp0 = tmp0->next;
  }
  return MMU_STATUS_FALSE;
}

static enum MMUStatus InsertNode(enum MMURegion e,
                                 struct MMUNode **pp,
                                 unsigned int free) {
  enum MMUStatus status;
  struct MMUNode *h, *b;

  if(free) {
    h = g_mmu->region[e].free_map_head;
    b = g_mmu->region[e].map_head;
    status = MergeNode(h, pp);
    MMUDEBUG(" *****insert free*****%d\n", (*pp)->page_count);
    if(MMU_IS_ERROR(status)) {
      /* remove from map*/
      if((*pp)->prev != NULL && (*pp)->next != NULL) {
        (*pp)->prev->next = (*pp)->next;
        (*pp)->next->prev = (*pp)->prev;
      }
      /* insert to free map */
      h->next->prev     = *pp;
      (*pp)->next       = h->next;
      (*pp)->prev       = h;
      h->next           = *pp;
    }
  } else {
    h = g_mmu->region[e].map_head;

    h->next->prev     = *pp;
    (*pp)->next       = h->next;
    (*pp)->prev       = h;
    h->next           = *pp;
    MMUDEBUG(" *****insert unfree*****%d\n", (*pp)->page_count);
  }

  return MMU_STATUS_OK;
}

static enum MMUStatus CreateNode(void) {
  struct MMUNode *free_map_head, *map_head, *p, **pp;
  struct MMUNode *free_map_tail, *map_tail;
  int i;
  unsigned int page_count;
  unsigned int prev_stlb = 0, prev_mtlb = 0;

  /* Init each region map node */
  for (i = 0; i < MMU_REGION_COUNT ; i++) {
    free_map_head = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
    map_head      = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
    free_map_tail = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
    map_tail      = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);

    free_map_head->mtlb_start = map_head->mtlb_start = -1;
    free_map_head->stlb_start = map_head->stlb_start = -1;
    free_map_head->mtlb_end   = map_head->mtlb_end   = -1;
    free_map_head->stlb_end   = map_head->stlb_end   = -1;
    free_map_head->process_id = map_head->process_id = 0;
    free_map_head->filp       = map_head->filp       = NULL;
    free_map_head->page_count = map_head->page_count = 0;
    free_map_head->prev       = map_head->prev       = NULL;
    free_map_head->next       = free_map_tail;
    map_head->next            = map_tail;

    free_map_tail->mtlb_start = map_tail->mtlb_start = -1;
    free_map_tail->stlb_start = map_tail->stlb_start = -1;
    free_map_tail->mtlb_end   = map_tail->mtlb_end   = -1;
    free_map_tail->stlb_end   = map_tail->stlb_end   = -1;
    free_map_tail->process_id = map_tail->process_id = 0;
    free_map_tail->filp       = map_tail->filp       = NULL;
    free_map_tail->page_count = map_tail->page_count = 0;
    free_map_tail->prev       = free_map_head;
    map_tail->prev            = map_head;
    free_map_tail->next       = map_tail->next       = NULL;

    g_mmu->region[i].free_map_head = free_map_head;
    g_mmu->region[i].map_head = map_head;
    g_mmu->region[i].free_map_tail = free_map_tail;
    g_mmu->region[i].map_tail = map_tail;

    p  = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
    pp = &p;

    switch(i) {
    case MMU_REGION_IN:
      page_count = (REGION_IN_END - REGION_IN_START + 1)/PAGE_SIZE;
      p->stlb_start = region_in_mmu_start >> 12 & 0x3FF;  //hold mmu addr: 0x0
      p->mtlb_start = region_in_mmu_start >> 22;
      //end point next region start: +1; for remainder: +1
      p->stlb_end = prev_stlb = region_in_mmu_end >> 12 & 0x3FF;
      p->mtlb_end = prev_mtlb = region_in_mmu_end >> 22;
      p->page_count = page_count - 1;  //hold mmu addr: 0x0
      break;
    case MMU_REGION_OUT:
      page_count = (REGION_OUT_END - REGION_OUT_START + 1)/PAGE_SIZE;
      p->stlb_start = region_out_mmu_start >> 12 & 0x3FF;
      p->mtlb_start = region_out_mmu_start >> 22;
      p->stlb_end = prev_stlb = region_out_mmu_end >> 12 & 0x3FF;
      p->mtlb_end = prev_mtlb = region_out_mmu_end >> 22;
      p->page_count = page_count;
      break;
    case MMU_REGION_PRIVATE:
      page_count = (REGION_PRIVATE_END - REGION_PRIVATE_START + 1)/PAGE_SIZE;
      p->stlb_start = region_private_mmu_start >> 12 & 0x3FF;
      p->mtlb_start = region_private_mmu_start >> 22;
      p->stlb_end = prev_stlb = region_private_mmu_end >> 12 & 0x3FF;
      p->mtlb_end = prev_mtlb = region_private_mmu_end >> 22;
      p->page_count = page_count;
      break;
    case MMU_REGION_PUB:
      p->stlb_start = prev_stlb;
      p->mtlb_start = prev_mtlb;
      p->stlb_end = prev_stlb = MMU_STLB_4K_ENTRY_NUM - 1;
      p->mtlb_end = prev_mtlb = MMU_MTLB_ENTRY_NUM - 1;
      p->page_count = (p->mtlb_end - p->mtlb_start) * MMU_STLB_4K_ENTRY_NUM +
                      p->stlb_end - p->stlb_start + 1;
      break;
    default:
      printk(KERN_NOTICE " *****MMU Region Error*****\n");
      break;
    }

    p->process_id = 0;
    p->filp       = NULL;
    p->next       = p->prev = NULL;

    InsertNode(i, pp, 1);
  }

  return MMU_STATUS_OK;
}

static enum MMUStatus FindFreeNode(enum MMURegion e,
                                   struct MMUNode **node,
                                   unsigned int page_count) {
  struct MMUNode *p;
  p = g_mmu->region[e].free_map_head->next;

  while(p) {
    if(p->page_count >= page_count) {
      *node = p;
      return MMU_STATUS_OK;
    }
    p = p->next;
  }
  return MMU_STATUS_FALSE;
}

static enum MMUStatus SplitFreeNode(enum MMURegion e,
                                   struct MMUNode **node,
                                   unsigned int page_count) {
  struct MMUNode *p, **new;

  p = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
  new = &p;

  **new = **node;

  (*new)->mtlb_start = (*node)->mtlb_start;
  (*new)->stlb_start = (*node)->stlb_start;
  (*new)->mtlb_end   = (page_count + (*node)->stlb_start) /
                       MMU_STLB_4K_ENTRY_NUM +
                       (*node)->mtlb_start;
  (*new)->stlb_end   = (page_count + (*node)->stlb_start) %
                       MMU_STLB_4K_ENTRY_NUM;
  (*new)->process_id = (*node)->process_id;
  (*new)->page_count = page_count;

  MMUDEBUG(" *****new mtlb_start*****%d\n", (*new)->mtlb_start);
  MMUDEBUG(" *****new stlb_start*****%d\n", (*new)->stlb_start);
  MMUDEBUG(" *****new mtlb_end*****%d\n", (*new)->mtlb_end);
  MMUDEBUG(" *****new stlb_end*****%d\n", (*new)->stlb_end);
  /* Insert a new node in map */
  InsertNode(e, new, 0);

  /* Update free node in free map*/
  (*node)->page_count -= page_count;
  if((*node)->page_count == 0) {
    DeleteNode(node);
    MMUDEBUG(" *****old node deleted*****\n");
  } else {
    (*node)->mtlb_start  = (*new)->mtlb_end;
    (*node)->stlb_start  = (*new)->stlb_end;

    MMUDEBUG(" *****old mtlb_start*****%d\n", (*node)->mtlb_start);
    MMUDEBUG(" *****old stlb_start*****%d\n", (*node)->stlb_start);
    MMUDEBUG(" *****old mtlb_end*****%d\n", (*node)->mtlb_end);
    MMUDEBUG(" *****old stlb_end*****%d\n", (*node)->stlb_end);
  }
  /* return a new node for map buffer */
  *node = *new;

  return MMU_STATUS_OK;
}

static enum MMUStatus RemoveNode(enum MMURegion e,
                                 void *buf_virtual_address,
                                 unsigned int process_id) {
  struct MMUNode *p, **pp;
  p = g_mmu->region[e].map_head->next;
  pp = &p;

  while(*pp) {
    if((*pp)->buf_virtual_address == buf_virtual_address &&
       (*pp)->process_id == process_id) {
      InsertNode(e, pp, 1);
      break;
    }
    *pp = (*pp)->next;
  }

  return MMU_STATUS_OK;
}

static enum MMUStatus Delay(unsigned int delay) {
  if(delay > 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
    ktime_t dl = ktime_set((delay / MSEC_PER_SEC),
                      (delay % MSEC_PER_SEC) * NSEC_PER_MSEC);
    __set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_hrtimeout(&dl, HRTIMER_MODE_REL);
#else
    msleep(delay);
#endif
  }

  return MMU_STATUS_OK;
}

static enum MMUStatus CreateMutex(void **mtx) {
  enum MMUStatus status;

  /* Allocate the mutex structure. */
  status = AllocateMemory(sizeof(struct mutex), mtx);
  if (MMU_IS_SUCCESS(status)) {
    /* Initialize the mutex. */
    mutex_init(*(struct mutex **)mtx);
  }

  return status;
}

static enum MMUStatus DeleteMutex(void *mtx) {
    /* Destroy the mutex. */
    mutex_destroy((struct mutex *)mtx);

    /* Free the mutex structure. */
    FreeMemory(mtx);

    return MMU_STATUS_OK;
}

static enum MMUStatus AcquireMutex(void *mtx, unsigned int timeout) {
  if (timeout == MMU_INFINITE)
  {
    /* Lock the mutex. */
    mutex_lock(mtx);

    /* Success. */
    return MMU_STATUS_OK;
  }

  for (;;) {
    /* Try to acquire the mutex. */
    if (mutex_trylock(mtx)) {
      /* Success. */
      return MMU_STATUS_OK;
    }

    if (timeout-- == 0) {
      break;
    }

    /* Wait for 1 millisecond. */
    Delay(1);
  }

  return MMU_STATUS_OK;
}

static enum MMUStatus ReleaseMutex(void *mtx) {
  /* Release the mutex. */
  mutex_unlock(mtx);

  return MMU_STATUS_OK;
}


static inline enum MMUStatus QueryProcessPageTable(void  *logical,
                               unsigned long long *address) {
  unsigned long lg = (unsigned long)logical;
  unsigned long offset = lg & ~PAGE_MASK;
  struct vm_area_struct *vma;
  spinlock_t *ptl;
  pgd_t *pgd;
  pud_t *pud;
  pmd_t *pmd;
  pte_t *pte;

  if (is_vmalloc_addr(logical)) {
    /* vmalloc area. */
    *address = page_to_phys(vmalloc_to_page(logical)) | offset;
    return MMU_STATUS_OK;
  } else if (virt_addr_valid(lg)) {
    /* Kernel logical address. */
    *address = virt_to_phys(logical);
    return MMU_STATUS_OK;
  } else {
    /* Try user VM area. */
    if (!current->mm)
      return MMU_STATUS_NOT_FOUND;

    down_read(&current->mm->mmap_sem);
    vma = find_vma(current->mm, lg);
    up_read(&current->mm->mmap_sem);

    /* To check if mapped to user. */
    if (!vma)
      return MMU_STATUS_NOT_FOUND;

    pgd = pgd_offset(current->mm, lg);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
      return MMU_STATUS_NOT_FOUND;

#if (defined(CONFIG_CPU_CSKYV2) || defined(CONFIG_X86)) \
    && LINUX_VERSION_CODE >= KERNEL_VERSION (4,12,0)
    pud = pud_offset((p4d_t*)pgd, lg);
#elif (defined(CONFIG_CPU_CSKYV2)) \
    && LINUX_VERSION_CODE >= KERNEL_VERSION (4,11,0)
    pud = pud_offset((p4d_t*)pgd, lg);
#else
    pud = pud_offset(pgd, lg);
#endif
    if (pud_none(*pud) || pud_bad(*pud))
      return MMU_STATUS_NOT_FOUND;

    pmd = pmd_offset(pud, lg);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
      return MMU_STATUS_NOT_FOUND;

    pte = pte_offset_map_lock(current->mm, pmd, lg, &ptl);
    if (!pte) {
      spin_unlock(ptl);
      return MMU_STATUS_NOT_FOUND;
    }

    if (!pte_present(*pte)) {
      pte_unmap_unlock(pte, ptl);
      return MMU_STATUS_NOT_FOUND;
    }

    *address = (pte_pfn(*pte) << PAGE_SHIFT) | offset;
    pte_unmap_unlock(pte, ptl);

    *address -= gBaseDDRHw;
	//MMUDEBUG(" QueryProcessPageTable map: virt %p -> %p\n", logical, (void *)*address);

    return MMU_STATUS_OK;
  }
}

static inline int GetProcessID(void) {
  return current->tgid;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
static inline int is_vmalloc_addr(void *addr) {
  unsigned long long addr = (unsigned long long)Addr;

  return addr >= VMALLOC_START && addr < VMALLOC_END;
}
#endif

static enum MMUStatus GetPhysicalAddress(void *logical,
                        unsigned long long *address) {
  enum MMUStatus status;

  status = QueryProcessPageTable(logical, address);

  return status;
}

static enum MMUStatus GetPageEntry(struct MMUNode *node,
                                   unsigned int **page_table_entry,
                                   unsigned int i) {
  int num = node->mtlb_start * MMU_STLB_4K_ENTRY_NUM +
            node->stlb_start + i;
  if (pcie) {
    *page_table_entry = (unsigned int*)g_mmu->stlb_virtual[0] + num;
  } else {
    *page_table_entry = g_mmu->stlb_virtual[num / MAX_NOPAGED_SIZE] +
                        num % MAX_NOPAGED_SIZE;
  }
  return MMU_STATUS_OK;
}

static enum MMUStatus SetupDynamicSpace(void) {
  int i, j;
  enum MMUStatus status;
  unsigned int stlb_entry;
  void *pointer;
  unsigned long long address;
  unsigned int num_entries = MMU_MTLB_ENTRY_NUM;
  unsigned int *mtlb_virtual = (unsigned int *)g_mmu->mtlb_virtual;
  
  AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
  if(pcie) {
   pointer = ioremap(gBaseDDRHw+0x0300000, num_entries*MMU_STLB_4K_SIZE);
    g_mmu->stlb_virtual[0] = pointer;
    MMUDEBUG(" *****stlb_virtual[0] = %p**%d\n", pointer, num_entries*MMU_STLB_4K_SIZE);
    address = 0x00300000;
    for(i = 0; i < num_entries; i++){
        stlb_entry = address
                    /* 4KB page size */
                    | (0 << 2)
                    /* Ignore exception */
                    | (0 << 1)
                    /* Present */
                    | (1 << 0);
        WritePageEntry(mtlb_virtual++, stlb_entry);
        address += MMU_STLB_4K_SIZE;
      }

  } else {
    for(i = 0; i < num_entries/(MAX_NOPAGED_SIZE/PAGE_SIZE); i++) {
      MMU_ON_ERROR(AllocateMemory(MAX_NOPAGED_SIZE, &pointer));
      g_mmu->stlb_virtual[i] = pointer;
      address = virt_to_phys(pointer);
      for(j = 0; j < MAX_NOPAGED_SIZE/PAGE_SIZE; j++){
        stlb_entry = address
                    /* 4KB page size */
                    | (0 << 2)
                    /* Ignore exception */
                    | (0 << 1)
                    /* Present */
                    | (1 << 0);
        WritePageEntry(mtlb_virtual++, stlb_entry);
        address += MMU_STLB_4K_SIZE;
      }
    }
  }
  ReleaseMutex(g_mmu->page_table_mutex);

  /* Initial map info. */
  CreateNode();

  return MMU_STATUS_OK;
onerror:
    /* Return status. */
    return status;
}


enum MMUStatus MMUInit(volatile unsigned char *hwregs) {
  enum MMUStatus status;
  unsigned i;
  void *pointer;

  if((ioread32((void*)(hwregs + MMU_REG_OFFSET))>>16) != 0x4D4D)
    return MMU_STATUS_NOT_FOUND;

  printk(KERN_NOTICE " *****MMU Init*****\n");

  /* Allocate memory for the MMU object. */
  MMU_ON_ERROR(AllocateMemory(sizeof(struct MMU), &pointer));
  ZeroMemory(pointer, sizeof(struct MMU));
  
  g_mmu = pointer;

  g_mmu->page_table_mutex   = NULL;

  /* Create the page table mutex. */
  MMU_ON_ERROR(CreateMutex(&g_mmu->page_table_mutex));

  for (i = 0; i < MMU_REGION_COUNT;i++) {
    MMU_ON_ERROR(CreateMutex(&g_mmu->region[i].node_mutex));
  }

  return MMU_STATUS_OK;

onerror:
  printk(KERN_NOTICE " *****MMU Init Error*****\n");
  return status;
}

enum MMUStatus MMURelease(void *filp, volatile unsigned char *hwregs) {
  int i, j;
  struct MMUNode *p, *tmp;
  unsigned long long address;
  unsigned int *page_table_entry;

  if((ioread32((void*)(hwregs + MMU_REG_OFFSET))>>16) != 0x4D4D)
    return MMU_STATUS_FALSE;

  /* if mmu or TLB not enabled, return */
  if(g_mmu == NULL || g_mmu->region[0].map_head == NULL)
    return MMU_STATUS_OK;

  printk(KERN_NOTICE " *****MMU Release*****\n");

  AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

  for (i = 0; i < MMU_REGION_COUNT; i++) {
    p = g_mmu->region[i].map_head->next;

    while(p) {
      tmp = p->next;
      if(p->filp == (struct file *)filp) {

        for(j = 0;j < p->page_count; j++) {
          GetPageEntry(p, &page_table_entry, j);
          address = 0;
          WritePageEntry(page_table_entry, address);
        }

        RemoveNode(i, p->buf_virtual_address, p->process_id);
      }
      p = tmp;
    }
  }
  ReleaseMutex(g_mmu->page_table_mutex);

  return MMU_STATUS_OK;
}

enum MMUStatus MMUCleanup(volatile unsigned char *hwregs) {
  int i;
  struct MMUNode *fp, *p, *tmp;

  if((ioread32((void*)(hwregs + MMU_REG_OFFSET))>>16) != 0x4D4D)
    return MMU_STATUS_FALSE;

  printk(" *****MMU cleanup*****\n");
  if(pcie) {
    if (g_mmu->stlb_virtual[0])
      iounmap(g_mmu->stlb_virtual[0]);
    if (g_mmu->mtlb_virtual)
      iounmap(g_mmu->mtlb_virtual);
    if (g_mmu->page_table_array)
      iounmap(g_mmu->page_table_array);
  } else {
    free_page((unsigned long)g_mmu->mtlb_virtual);
    FreeMemory(g_mmu->page_table_array);

    for (i = 0; i < MAX_NOPAGED_SIZE/PAGE_SIZE; i++) {
      FreeMemory(g_mmu->stlb_virtual[i]);
    }
  }
  DeleteMutex(g_mmu->page_table_mutex);

  for (i = 0; i < MMU_REGION_COUNT; i++) {
    DeleteMutex(g_mmu->region[i].node_mutex);

    fp = g_mmu->region[i].free_map_head;
    p  = g_mmu->region[i].map_head;
    while(fp) {
      tmp = fp->next;
      FreeMemory(fp);
      fp  = tmp;
      MMUDEBUG(" *****clean free node*****\n");
    }

    while(p) {
      tmp = p->next;
      FreeMemory(p);
      p   = tmp;
      MMUDEBUG(" *****clean node*****\n");
    }
  }

  FreeMemory(g_mmu);

  //iowrite32(0, (void*)(hwregs + MMU_REG_CONTROL));
  //mmu_enable = 0;

  return MMU_STATUS_OK;
}

/*------------------------------------------------------------------------------
    Function name: MMUEnable
        Description:
            Create TLB, set registers and enable MMU

    For pcie, TLB buffers come from FPGA memory and The distribution is as follows
           MTLB:              start from: 0x00100000, size: 4K bits
           page table array:              0x00200000        64 bits
           STLB:                          0x00300000        4M bits
 ------------------------------------------------------------------------------*/
static enum MMUStatus MMUEnable(volatile unsigned char *hwregs) {
  enum MMUStatus status;
  unsigned int address;
  unsigned int mutex = MMU_FALSE;

  if(mmu_enable == MMU_TRUE) {
    printk(" *****MMU Enabled*****\n");
    return MMU_STATUS_OK;
  }

  printk(" *****MMU Enable*****\n");

  AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
  mutex = MMU_TRUE;
  if(pcie) {
    g_mmu->mtlb_size = MMU_MTLB_SIZE;
    g_mmu->mtlb_virtual = ioremap(gBaseDDRHw+0x0100000, g_mmu->mtlb_size);
    g_mmu->mtlb_physical = 0x00100000;

    g_mmu->page_table_array = ioremap(gBaseDDRHw+0x0200000, 64);
  } else {
    /* Allocate the 4K mode MTLB table. */
    g_mmu->mtlb_size = MMU_MTLB_SIZE;
    g_mmu->mtlb_virtual = (void *)get_zeroed_page(GFP_KERNEL | MMU_NOWARN);
    g_mmu->mtlb_physical = virt_to_phys(g_mmu->mtlb_virtual);

    MMU_ON_ERROR(AllocateMemory(64, &g_mmu->page_table_array));
  }
  *((unsigned long long *)g_mmu->page_table_array) = 
                             (g_mmu->mtlb_physical & 0xFFFFFC00)
                             | (0 << 0);
  MMUDEBUG(" Page table array[0]: lsb = 0x%08x\n", ((int *)g_mmu->page_table_array)[0]);
  MMUDEBUG("                      msb = 0x%08x\n", ((int *)g_mmu->page_table_array)[1]);

  ZeroMemory(g_mmu->mtlb_virtual, g_mmu->mtlb_size);

  ReleaseMutex(g_mmu->page_table_mutex);

  MMU_ON_ERROR(SetupDynamicSpace());

  if(pcie) {
    address = 0x00200000;
  } else {
    address = virt_to_phys(g_mmu->page_table_array);
  }
  iowrite32(address, (void*)(hwregs + MMU_REG_ADDRESS));

  iowrite32(0x10000, (void*)(hwregs + MMU_REG_PAGE_TABLE_ID));
  iowrite32(0x00000, (void*)(hwregs + MMU_REG_PAGE_TABLE_ID));

  iowrite32(1, (void*)(hwregs + MMU_REG_CONTROL));
  mmu_enable = MMU_TRUE;
  return MMU_STATUS_OK;

onerror:
  if (mutex) {
    ReleaseMutex(g_mmu->page_table_mutex);
  }
  MMUDEBUG(" *****MMU Enable Error*****\n");
  return status;
}

static enum MMUStatus MMUMemNodeMap(struct addr_desc *addr, struct file *filp,
                             volatile unsigned char *hwregs) {
  enum MMUStatus status;
  unsigned int  page_count = 0;
  unsigned int i = 0;
  struct MMUNode *p;
  unsigned long long address = 0x0;
  unsigned int *page_table_entry;
  enum MMURegion e;
  unsigned int mutex = MMU_FALSE;

  MMUDEBUG(" *****MMU Map*****\n");
  AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
  mutex = MMU_TRUE;

  page_count = (addr->size - 1)/PAGE_SIZE + 1;

  GetPhysicalAddress(addr->virtual_address, &address);
  if(address >= REGION_IN_START &&
     address + addr->size < REGION_IN_END)
    e = MMU_REGION_IN;
  else if(address >= REGION_OUT_START &&
          address + addr->size < REGION_OUT_END)
    e = MMU_REGION_OUT;
  else if(address >= REGION_PRIVATE_START &&
          address + addr->size < REGION_PRIVATE_END)
    e = MMU_REGION_PRIVATE;
  else
    e = MMU_REGION_PUB;

  MMU_ON_ERROR(FindFreeNode(e, &p, page_count));

  SplitFreeNode(e, &p, page_count);
  MMUDEBUG(" *****Node map size*****%d\n", p->page_count);

  p->buf_virtual_address = addr->virtual_address;
  p->process_id          = GetProcessID();
  p->filp                = filp;

  for(i = 0;i < page_count; i++) {
    GetPhysicalAddress(addr->virtual_address + i * PAGE_SIZE, &address);
    GetPageEntry(p, &page_table_entry, i);
    address = (address & 0xFFFFF000)
                      /* writable */
                      | (1 << 2)
                      /* Ignore exception */
                      | (0 << 1)
                      /* Present */
                      | (1 << 0);
    WritePageEntry(page_table_entry, address);
  }
  addr->bus_address = p->mtlb_start << MMU_MTLB_SHIFT
                      | p->stlb_start << MMU_STLB_4K_SHIFT;

  MMUDEBUG(" **%d\n", MMU_MTLB_SHIFT);
  MMUDEBUG(" **%d\n", MMU_STLB_4K_SHIFT);
  MMUDEBUG(" MMUMemNodeMap map %d pages: MTLB/STLB starts %d/%d\n", page_count, p->mtlb_start, p->stlb_start);
  MMUDEBUG(" MMUMemNodeMap map %p -> 0x%08x\n", addr->virtual_address, addr->bus_address);
  ReleaseMutex(g_mmu->page_table_mutex);

  iowrite32(1, (void*)(hwregs + MMU_REG_FLUSH));
  iowrite32(0, (void*)(hwregs + MMU_REG_FLUSH));

  return MMU_STATUS_OK;

onerror:
  if (mutex) {
    ReleaseMutex(g_mmu->page_table_mutex);
  }
  MMUDEBUG(" *****MMU Map Error*****\n");
  return status;
} 

static enum MMUStatus MMUMemNodeUnmap(struct addr_desc *addr,
                               volatile unsigned char *hwregs) {
  unsigned int i;
  unsigned long long address = 0x0;
  unsigned int *page_table_entry;
  int process_id = GetProcessID();
  enum MMURegion e = MMU_REGION_COUNT;
  enum MMUStatus status = MMU_STATUS_OUT_OF_MEMORY;
  struct MMUNode *p;
  unsigned int mutex = MMU_FALSE;

  MMUDEBUG(" *****MMU Unmap*****\n");
  AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
  mutex = MMU_TRUE;

  GetPhysicalAddress(addr->virtual_address, &address);
  if(address >= REGION_IN_START &&
     address < REGION_IN_END)
    e = MMU_REGION_IN;
  else if(address >= REGION_OUT_START &&
          address < REGION_OUT_END)
    e = MMU_REGION_OUT;
  else if(address >= REGION_PRIVATE_START &&
          address < REGION_PRIVATE_END)
    e = MMU_REGION_PRIVATE;
  else
    e = MMU_REGION_PUB;

  p = g_mmu->region[e].map_head->next;
  /* Reset STLB of the node */
  while(p) {
    if(p->buf_virtual_address == addr->virtual_address &&
        p->process_id == process_id) {
      for(i = 0;i < p->page_count; i++) {
        GetPageEntry(p, &page_table_entry, i);
        address = 0;
        WritePageEntry(page_table_entry, address);
      }
      break;
    }
    p = p->next;
  }
  if(!p)
    goto onerror;

  RemoveNode(e, addr->virtual_address, process_id);

  ReleaseMutex(g_mmu->page_table_mutex);

  iowrite32(1, (void*)(hwregs + MMU_REG_FLUSH));
  iowrite32(0, (void*)(hwregs + MMU_REG_FLUSH));

  return MMU_STATUS_OK;

onerror:
  if (mutex) {
    ReleaseMutex(g_mmu->page_table_mutex);
  }
  MMUDEBUG(" *****MMU Unmap Error*****\n");
  return status;
}

static long MMUCtlBufferMap(struct file *filp, unsigned long arg,
                  volatile unsigned char *hwregs) {
  struct addr_desc addr;
  long tmp;

  tmp = copy_from_user(&addr, (void*)arg, sizeof(struct addr_desc));
  if (tmp) {
    MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
    return -MMU_EFAULT;
  }

  MMUMemNodeMap(&addr, filp, hwregs);

  tmp = copy_to_user((void*) arg, &addr, sizeof(struct addr_desc));
  if (tmp) {
    MMUDEBUG("copy_to_user failed, returned %li\n", tmp);
    return -MMU_EFAULT;
  }
  return 0;
}

static long MMUCtlBufferUnmap(unsigned long arg,
                    volatile unsigned char *hwregs) {
  struct addr_desc addr;
  long tmp;

  tmp = copy_from_user(&addr, (void*)arg, sizeof(struct addr_desc));
  if (tmp) {
    MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
    return -MMU_EFAULT;
  }

  MMUMemNodeUnmap(&addr, hwregs);
  return 0;
}

static long MMUCtlEnable(unsigned long arg, volatile unsigned char *hwregs) {
  unsigned int enable;
  long tmp;

  tmp = copy_from_user(&enable, (void*)arg, sizeof(unsigned int));
  if (tmp) {
    MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
    return -MMU_EFAULT;
  }

  MMUEnable(hwregs);

  return 0;
}

long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg,
                  volatile unsigned char *hwregs) {

  if((ioread32((void*)(hwregs + MMU_REG_OFFSET))>>16) != 0x4D4D)
    return -MMU_ENOTTY;

  switch (cmd) {
  case HANTRO_IOCS_MMU_MEM_MAP: {
    return (MMUCtlBufferMap((struct file *)filp, arg, hwregs));
  }
  case HANTRO_IOCS_MMU_MEM_UNMAP: {
    return (MMUCtlBufferUnmap(arg, hwregs));
  }
  case HANTRO_IOCS_MMU_ENABLE: {
    return (MMUCtlEnable(arg, hwregs));
  }
  default:
    return -MMU_ENOTTY;
  }
}
