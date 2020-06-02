#ifndef _HANTROMMU_H_
#define _HANTROMMU_H_

#include <linux/fs.h>

#define REGION_IN_START          0x0
#define REGION_IN_END            0x40000000
#define REGION_OUT_START         0x40000000
#define REGION_OUT_END           0x80000000
#define REGION_PRIVATE_START     0x80000000
#define REGION_PRIVATE_END       0xc0000000

#define REGION_IN_MMU_START      0x1000
#define REGION_IN_MMU_END        0x40002000
#define REGION_OUT_MMU_START     0x40002000
#define REGION_OUT_MMU_END       0x80001000
#define REGION_PRIVATE_MMU_START 0x80001000
#define REGION_PRIVATE_MMU_END   0xc0000000

enum MMUStatus {
  MMU_STATUS_OK                          =    0,

  MMU_STATUS_FALSE                       =   -1,
  MMU_STATUS_INVALID_ARGUMENT            =   -2,
  MMU_STATUS_INVALID_OBJECT              =   -3,
  MMU_STATUS_OUT_OF_MEMORY               =   -4,
  MMU_STATUS_NOT_FOUND                   =   -19,
};

struct addr_desc {
  void                       *virtual_address;  /* buffer virtual address */
  unsigned int               bus_address;  /* buffer physical address */
  unsigned int               size;  /* physical size */
};

#define HANTRO_IOC_MMU  'm'

#define HANTRO_IOCS_MMU_MEM_MAP    _IOWR(HANTRO_IOC_MMU, 1, struct addr_desc *)
#define HANTRO_IOCS_MMU_MEM_UNMAP  _IOWR(HANTRO_IOC_MMU, 2, struct addr_desc *)
#define HANTRO_IOCS_MMU_ENABLE     _IOWR(HANTRO_IOC_MMU, 3, unsigned int *)
#define HANTRO_IOC_MMU_MAXNR 3

#endif

