/*------------------------------------------------------------------------------
-- Copyright (c) 2019, VeriSilicon Inc. or its affiliates. All rights reserved--
--                                                                            --
-- Permission is hereby granted, free of charge, to any person obtaining a    --
-- copy of this software and associated documentation files (the "Software"), --
-- to deal in the Software without restriction, including without limitation  --
-- the rights to use copy, modify, merge, publish, distribute, sublicense,    --
-- and/or sell copies of the Software, and to permit persons to whom the      --
-- Software is furnished to do so, subject to the following conditions:       --
--                                                                            --
-- The above copyright notice and this permission notice shall be included in --
-- all copies or substantial portions of the Software.                        --
--                                                                            --
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR --
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   --
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE--
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER     --
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    --
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        --
-- DEALINGS IN THE SOFTWARE.                                                  --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
#include "osal.h"
#include "memalloc_freertos.h"

/* Dev Memory Map for NETNT */
#define DEV_MEM_BANK_0           (0)
#define DEV_MEM_BANK_1           (1)

#define SHARED_MEM_DEV_ADDRESS   (0x800000000LL)
#define SHARED_MEM_CPU_ADDRESS   (0x80000000)
#define SHARED_MEM_SIZE          (2*1024*1024*1024LL)
#define SHARED_MEM_BANK          (DEV_MEM_BANK_0)

#define MEM_0_DEV_ADDRESS        (SHARED_MEM_DEV_ADDRESS + SHARED_MEM_SIZE)
#define MEM_0_CPU_ADDRESS        (0)
#define MEM_0_SIZE               (16*1024*1024*1024LL - SHARED_MEM_SIZE)
#define MEM_0_BANK               (DEV_MEM_BANK_0)

#define MEM_1_DEV_ADDRESS        (0xC00000000LL)
#define MEM_1_CPU_ADDRESS        (0)
#define MEM_1_SIZE               (16*1024*1024*1024LL)
#define MEM_1_BANK               (DEV_MEM_BANK_1)

/* the size of chunk in MEMALLOC_DYNAMIC */
#define CHUNK_SIZE    (16*1024)

/*
  memory type enum
*/
typedef enum {
  MEM_TYPE_SHARED = 0,
  MEM_TYPE_DEVICE,
}_mem_type;

/*
  memory region id enum, also as index of array "_regions" and array "hlina_mgr"
*/
typedef enum {
  MEM_ID_SHARED = 0,
  MEM_ID_0,
  MEM_ID_1,
  MAX_MEM_REGION
}_mem_region_id;

/*
  memory region properties struct
*/
typedef struct __mem_region {
  unsigned long long bus_addr;          /* Physical address of dev memory region */
  char *             virt_addr;         /* virtual address mapping to CPU, 0 means invisible to CPU */
  unsigned long long size;              /* size of dev memory region */
  unsigned short     bank;              /* DDR RAM bank id */
  _mem_type          type;
  unsigned long      translation_offset; /* user space SW will substract HLINA_TRANSL_OFFSET from the bus address
                                          * and decoder HW will use the result as the address translated base
                                          * address. The SW needs the original host memory bus address for memory
                                          * mapping to virtual address. */
} _mem_region_t;

/*
  descriptor for each chunk
*/
typedef struct hlinc {
  unsigned short chunks_occupied;       /* indicate number of occupied chunks which start from current chunk */
  int filp;                             /* Client that allocated this chunk */
} hlina_chunk_t;

/*
  memory region management struct
*/
typedef struct __hlina_mgr {
	hlina_chunk_t *hlina_chunks;        /* point to descriptor of each chunks */
	unsigned long chunks;               /* total chunks in this mem region */
	unsigned long reserved_chunks;      /* number of reserved chunks in this mem rtegion. 
                                         * The reserved chunks is located at the beginning of corresponding mem region, application can't use them */
	_mem_region_t *mem_prop;
} _hlina_mgr_t;


/* 
  memory regions perporties, indexed by _mem_region_id.
*/
static _mem_region_t _regions[MAX_MEM_REGION] = {
  /*MEM_ID_SHARED*/ { SHARED_MEM_DEV_ADDRESS, (char *)SHARED_MEM_CPU_ADDRESS, SHARED_MEM_SIZE, SHARED_MEM_BANK,  MEM_TYPE_SHARED,  0 },
  /*MEM_ID_0     */ { MEM_0_DEV_ADDRESS,      (char *)MEM_0_CPU_ADDRESS,      MEM_0_SIZE,      MEM_0_BANK,       MEM_TYPE_DEVICE,  0 },
  /*MEM_ID_1     */ { MEM_1_DEV_ADDRESS,      (char *)MEM_1_CPU_ADDRESS,      MEM_1_SIZE,      MEM_1_BANK,       MEM_TYPE_DEVICE,  0 }
};

/* 
  memory regions hlina manager, indexed by _mem_region_id.
*/
static _hlina_mgr_t hlina_mgr[MAX_MEM_REGION];

int memalloc_major = -1;
//static DEFINE_SPINLOCK(mem_lock);
pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;
/*
  Functions declaration
*/
static int AllocMemory(unsigned long long *busaddr, unsigned int size, unsigned int region, const int filp);
static int FreeMemory(unsigned long long busaddr, const int filp);
//static void * DirectMemoryMap(unsigned long long busaddr, unsigned long map_size);
static void CalculateChunks(_mem_region_id id, unsigned long *chunks, unsigned long *chunks_mgr_size);
static void ResetMems(void);

/*
  To specified mem region, calculate total chunks number and aligned buffer size to store chunks' descriptors.
*/
static void CalculateChunks(_mem_region_id id, unsigned long *chunks, unsigned long *chunks_mgr_size)
{
  *chunks = 0;
  *chunks_mgr_size = 0;

  if(id < MAX_MEM_REGION) {
    *chunks = _regions[id].size / CHUNK_SIZE;
    *chunks_mgr_size = (((unsigned int)chunks) * sizeof(hlina_chunk_t) + CHUNK_SIZE - 1) & (~(CHUNK_SIZE - 1));
  }
}

/*
  To specified bus address, convert it to virtual (CPU) address according to mem region properties.
*/
void * DirectMemoryMap(unsigned long long busaddr, unsigned long map_size) {
  int i;
  unsigned long long offset;
  void *virtaddr = NULL;

  if(map_size == 0) {
    return NULL;
  }

  for(i=0; i<MAX_MEM_REGION; i++) {
    if(_regions[i].virt_addr == NULL)
      continue;

    if(busaddr >= _regions[i].bus_addr) {
      offset = busaddr - _regions[i].bus_addr;
      if ((offset + map_size) <= _regions[i].size) {
        virtaddr = (void *)(_regions[i].virt_addr + offset);
        break;
      }
    }
  }
  if(virtaddr == NULL) {
    return (void *)((unsigned long)busaddr);
  }
  return virtaddr;
}

/*
  ioctl
*/
long memalloc_ioctl(int filp, unsigned int cmd,unsigned long arg) {
  int ret = 0;
  MemallocParams memparams;
  unsigned long busaddr;

  PDEBUG("memalloc ioctl cmd 0x%08x\n", cmd);

  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC) return -ENOTTY;
  if (_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR) return -ENOTTY;

  if (_IOC_DIR(cmd) & _IOC_READ)
    ret = !access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    ret = !access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd));
  if (ret) return -EFAULT;

  spin_lock(&mem_lock);

  switch (cmd) {
  case MEMALLOC_IOCHARDRESET:
    PDEBUG("memalloc HARDRESET\n");
    ResetMems();
    break;
  case MEMALLOC_IOCXGETBUFFER:
    PDEBUG("memalloc GETBUFFER");

    ret = copy_from_user(&memparams, (MemallocParams *)arg,
                         sizeof(MemallocParams));
    if (ret) break;

    ret = AllocMemory(&memparams.bus_address, memparams.size, MEM_ID_SHARED, filp);

    memparams.translation_offset = _regions[MEM_ID_SHARED].translation_offset;

    ret |= copy_to_user((MemallocParams *)arg, &memparams,
                        sizeof(MemallocParams));

    break;
  case MEMALLOC_IOCSFREEBUFFER:
    PDEBUG("memalloc FREEBUFFER\n");

    __get_user(busaddr, (unsigned long long*)arg);
    ret = FreeMemory(busaddr, filp);
    break;
  }

  spin_unlock(&mem_lock);

  return ret ? -EFAULT : 0;
}

int memalloc_open(int *inode, int filp) {
  PDEBUG("memalloc dev opened\n");
  return 0;
}

int memalloc_release(int *inode, int filp) {
  int i = 0, j = 0;
  hlina_chunk_t *p_hlina;

  for (i = 0; i < MAX_MEM_REGION; i++) {
  	p_hlina = hlina_mgr[i].hlina_chunks;
    for (j = hlina_mgr[i].reserved_chunks; j < hlina_mgr[i].chunks; j++) {
      spin_lock(&mem_lock);
      if (p_hlina[j].filp == filp) {
        printk(KERN_WARNING "memalloc: Found unfreed memory at release time!\n");

        p_hlina[j].filp = 0;
        p_hlina[j].chunks_occupied = 0;
      }
      spin_unlock(&mem_lock);
    }
  }
  PDEBUG("memalloc dev closed\n");
  return 0;
}

int __init memalloc_init(void) {
  int result;
  int i;
  unsigned long chunks, mgr_size, reserved_size;
  void *p;


  PDEBUG("memalloc module init\n");
  printk("memalloc: Linear Memory Allocator\n");

  /*
    Allocate memory for hlina_mgr
    In NETINT platform, there is no big heap memory to store hlina_mgr, so it is put to beginning of shared memory region directly
  */
  reserved_size = 0;
  for (i=0; i<MAX_MEM_REGION; i++) {
    CalculateChunks(i, &chunks, &mgr_size);

    /* all hlina_chunks are put to SHARED memory region */
    p = DirectMemoryMap(_regions[MEM_ID_SHARED].bus_addr + reserved_size, mgr_size);
    hlina_mgr[i].hlina_chunks    = (hlina_chunk_t *)p;
    hlina_mgr[i].chunks          = chunks;
    hlina_mgr[i].reserved_chunks = 0;
    hlina_mgr[i].mem_prop        = &_regions[i];

    reserved_size += mgr_size;
	printk("memalloc: Linear memory base = 0x%08llx\n", _regions[i].bus_addr);
    printk(KERN_INFO
           "memalloc: Region[%d] size %ld MB; %d chunks"
           " of size %lu\n", i,
           (unsigned long)(_regions[i].size/(1024*1024)), (int)chunks, CHUNK_SIZE);
  }
  hlina_mgr[MEM_ID_SHARED].reserved_chunks = reserved_size / CHUNK_SIZE;

  result = register_chrdev(memalloc_major, "memalloc", &memalloc_fops);
  if (result < 0) {
    PDEBUG("memalloc: unable to get major %d\n", memalloc_major);
    goto err;
  } else if (result != 0) {/* this is for dynamic major */
    memalloc_major = result;
  }

  ResetMems();

  printf("memalloc init successful...\n");
  return 0;

err:
#if 0
  if (hlina_chunks != NULL) vfree(hlina_chunks);
#endif

  return result;
}

/* 
  Cycle through the buffers in specified region, give the first free one.
  TBD: need to check if specifying _mem_type is better than region.
*/
static int AllocMemory(unsigned long long *busaddr, unsigned int size, unsigned int region, const int filp) {
  int i = 0;
  int j = 0;
  unsigned int skip_chunks = 0;
  unsigned long total_chunks;
  hlina_chunk_t *p_hlina;

  /* calculate how many chunks we need; round up to chunk boundary */
  unsigned int alloc_chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

  *busaddr = 0;

  if(region >= MAX_MEM_REGION) {
    printk("memalloc: Allocation FAILED: unknown region = %ld\n", region);
    return -EFAULT;
  }

  p_hlina = hlina_mgr[region].hlina_chunks;
  total_chunks = hlina_mgr[region].chunks;

  /* run through the chunk table */
  for (i = hlina_mgr[region].reserved_chunks; i < total_chunks;) {
    skip_chunks = 0;
    /* if this chunk is available */
    if (!p_hlina[i].chunks_occupied) {
      /* check that there is enough memory left */
      if (i + alloc_chunks > total_chunks) break;

      /* check that there is enough consecutive chunks available */
      for (j = i; j < i + alloc_chunks; j++) {
        if (p_hlina[j].chunks_occupied) {
          skip_chunks = 1;
          /* skip the used chunks */
          i = j + p_hlina[j].chunks_occupied;
          break;
        }
      }

      /* if enough free memory found */
      if (!skip_chunks) {
        *busaddr = hlina_mgr[region].mem_prop->bus_addr + i * CHUNK_SIZE;
        p_hlina[i].filp = filp;
        p_hlina[i].chunks_occupied = alloc_chunks;
        break;
      }
    } else {
      /* skip the used chunks */
      i += p_hlina[i].chunks_occupied;
    }
  }

  if (*busaddr == 0) {
    printk("memalloc: Allocation FAILED: size = %ld\n", size);
    return -EFAULT;
  } else {
    PDEBUG("MEMALLOC OK: size: %d, reserved: %d\n", size,
           alloc_chunks * CHUNK_SIZE);
  }

  return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned long long busaddr, const int filp) {
  unsigned int i, id;
  unsigned long long addr;
  hlina_chunk_t *p_hlina;

  for(i = 0; i < MAX_MEM_REGION; i++) {
    addr = busaddr + _regions[i].translation_offset;
	if ((addr >= _regions[i].bus_addr) && (addr < _regions[i].bus_addr + _regions[i].size)) {
      id = (addr - _regions[i].bus_addr) / CHUNK_SIZE;
      p_hlina = &hlina_mgr[i].hlina_chunks[id];
      if ((p_hlina->chunks_occupied) && (p_hlina->filp == filp)) {
        p_hlina->filp = 0;
        p_hlina->chunks_occupied = 0;
        return 0;
      }
    }
  }

  printk(KERN_WARNING "memalloc: Owner mismatch while freeing memory!\n");
  return -1;
}


/* Reset "used" status */
static void ResetMems(void) {
  int i, j = 0;
  hlina_chunk_t *p_hlina;

  for (i=0; i<MAX_MEM_REGION; i++) {
    p_hlina = hlina_mgr[i].hlina_chunks;
    for(j=hlina_mgr[i].reserved_chunks; j<hlina_mgr[i].chunks; j++) {
      p_hlina[j].filp = 0;
      p_hlina[j].chunks_occupied = 0;
    }
  }
}

//memory type is shared
//return busAddr and virtrulAddr
ptr_t GetBusAddrForIODevide(unsigned int size) {
  ptr_t bus_addr = 0; 
  AllocMemory(&bus_addr, size, MEM_ID_SHARED , MEM_FD);
  return bus_addr;
}
