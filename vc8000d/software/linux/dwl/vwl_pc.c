/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2020 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vwl_pc.h"
#include "asic.h"
#include "dwlthread.h"
#include "fifo.h"

#define DWL_CLIENT_TYPE_H264_DEC     1U
#define DWL_CLIENT_TYPE_MPEG4_DEC    2U
#define DWL_CLIENT_TYPE_JPEG_DEC     3U
#define DWL_CLIENT_TYPE_PP           4U
#define DWL_CLIENT_TYPE_VC1_DEC      5U
#define DWL_CLIENT_TYPE_MPEG2_DEC    6U
#define DWL_CLIENT_TYPE_VP6_DEC      7U
#define DWL_CLIENT_TYPE_AVS_DEC      8U
#define DWL_CLIENT_TYPE_RV_DEC       9U
#define DWL_CLIENT_TYPE_VP8_DEC      10U
#define DWL_CLIENT_TYPE_VP9_DEC      11U
#define DWL_CLIENT_TYPE_HEVC_DEC     12U
#define DWL_CLIENT_TYPE_ST_PP        14U
#define DWL_CLIENT_TYPE_H264_MAIN10  15U
#define DWL_CLIENT_TYPE_AVS2_DEC     16U
#define DWL_CLIENT_TYPE_AV1_DEC      17U
#define DWL_CLIENT_TYPE_NONE         30U


#define VWL_CLIENT_TYPE_H264_DEC     0x00000001
#define VWL_CLIENT_TYPE_MPEG4_DEC    0x00000002
#define VWL_CLIENT_TYPE_JPEG_DEC     0x00000004
#define VWL_CLIENT_TYPE_PP           0x00000008
#define VWL_CLIENT_TYPE_VC1_DEC      0x00000010
#define VWL_CLIENT_TYPE_MPEG2_DEC    0x00000020
#define VWL_CLIENT_TYPE_VP6_DEC      0x00000040
#define VWL_CLIENT_TYPE_AVS_DEC      0x00000080
#define VWL_CLIENT_TYPE_RV_DEC       0x00000100
#define VWL_CLIENT_TYPE_VP8_DEC      0x00000200
#define VWL_CLIENT_TYPE_VP9_DEC      0x00000400
#define VWL_CLIENT_TYPE_HEVC_DEC     0x00000800
#define VWL_CLIENT_TYPE_ST_PP        0x00001000
#define VWL_CLIENT_TYPE_H264_MAIN10  0x00002000
#define VWL_CLIENT_TYPE_AVS2_DEC     0x00004000
#define VWL_CLIENT_TYPE_AV1_DEC      0x00008000
#define VWL_CLIENT_TYPE_ALL          0xFFFFFFFF

//video decoder vcmd configuration
#define VCMD_DEC_IO_ADDR_0                      0x600000   /*customer specify according to own platform*/
#define VCMD_DEC_IO_SIZE_0                      (ASIC_VCMD_SWREG_AMOUNT * 4)    /* bytes */
#define VCMD_DEC_INT_PIN_0                      -1
#define VCMD_DEC_MODULE_TYPE_0                  2
#define VCMD_DEC_MODULE_MAIN_ADDR_0             0x1000    /*customer specify according to own platform*/
#define VCMD_DEC_MODULE_DEC400_ADDR_0           0XFFFF    /*0xffff means no such kind of submodule*/
#define VCMD_DEC_MODULE_L2CACHE_ADDR_0          0X2000
#define VCMD_DEC_MODULE_MMU_ADDR_0              0XFFFF


#define VCMD_DEC_IO_ADDR_1                      0x700000   /*customer specify according to own platform*/
#define VCMD_DEC_IO_SIZE_1                      (ASIC_VCMD_SWREG_AMOUNT * 4)    /* bytes */
#define VCMD_DEC_INT_PIN_1                      -1
#define VCMD_DEC_MODULE_TYPE_1                  2
#define VCMD_DEC_MODULE_MAIN_ADDR_1             0x1000    /*customer specify according to own platform*/
#define VCMD_DEC_MODULE_DEC400_ADDR_1           0XFFFF    /*0xffff means no such kind of submodule*/
#define VCMD_DEC_MODULE_L2CACHE_ADDR_1          0X2000
#define VCMD_DEC_MODULE_MMU_ADDR_1              0XFFFF

#define VCMD_DEC_IO_ADDR_2                      0xb2000   /*customer specify according to own platform*/
#define VCMD_DEC_IO_SIZE_2                      (ASIC_VCMD_SWREG_AMOUNT * 4)    /* bytes */
#define VCMD_DEC_INT_PIN_2                      -1
#define VCMD_DEC_MODULE_TYPE_2                  2
#define VCMD_DEC_MODULE_MAIN_ADDR_2             0x0000    /*customer specify according to own platform*/
#define VCMD_DEC_MODULE_DEC400_ADDR_2           0XFFFF    /*0xffff means no such kind of submodule*/
#define VCMD_DEC_MODULE_L2CACHE_ADDR_2          0XFFFF
#define VCMD_DEC_MODULE_MMU_ADDR_2              0XFFFF

#define VCMD_DEC_IO_ADDR_3                      0xb3000   /*customer specify according to own platform*/
#define VCMD_DEC_IO_SIZE_3                      (ASIC_VCMD_SWREG_AMOUNT * 4)    /* bytes */
#define VCMD_DEC_INT_PIN_3                      -1
#define VCMD_DEC_MODULE_TYPE_3                  2
#define VCMD_DEC_MODULE_MAIN_ADDR_3             0x0000    /*customer specify according to own platform*/
#define VCMD_DEC_MODULE_DEC400_ADDR_3           0XFFFF    /*0xffff means no such kind of submodule*/
#define VCMD_DEC_MODULE_L2CACHE_ADDR_3          0XFFFF
#define VCMD_DEC_MODULE_MMU_ADDR_3              0XFFFF


#define OPCODE_WREG               (0x01<<27)
#define OPCODE_END                (0x02<<27)
#define OPCODE_NOP                (0x03<<27)
#define OPCODE_RREG               (0x16<<27)
#define OPCODE_INT                (0x18<<27)
#define OPCODE_JMP                (0x19<<27)
#define OPCODE_STALL              (0x09<<27)
#define OPCODE_CLRINT             (0x1a<<27)
#define OPCODE_JMP_RDY0           (0x19<<27)
#define OPCODE_JMP_RDY1           ((0x19<<27)|(1<<26))
#define JMP_IE_1                  (1<<25)
#define JMP_RDY_1                 (1<<26)

#define CMDBUF_PRIORITY_NORMAL            0
#define CMDBUF_PRIORITY_HIGH              1

#define EXECUTING_CMDBUF_ID_ADDR       26
#define VCMD_EXE_CMDBUF_COUNT           3


#define WORKING_STATE_IDLE          0
#define WORKING_STATE_WORKING       1
#define CMDBUF_EXE_STATUS_OK        0
#define CMDBUF_EXE_STATUS_CMDERR        1
#define CMDBUF_EXE_STATUS_BUSERR        2

#define IRQ_HANDLED 1

/********variables declaration related with race condition**********/

#define MAX_CORE_NUMBER       4

#define CMDBUF_MAX_SIZE       (512*4*4)

#define CMDBUF_POOL_TOTAL_SIZE                (2*1024*1024)        //approximately=128x(320x240)=128x2k=128x8kbyte=1Mbytes
#define TOTAL_DISCRETE_CMDBUF_NUM             (CMDBUF_POOL_TOTAL_SIZE/CMDBUF_MAX_SIZE)
#define MAX_CMDBUF_INT_NUMBER                 1
#define INT_MIN_SUM_OF_IMAGE_SIZE             (4096*2160*MAX_CORE_NUMBER*MAX_CMDBUF_INT_NUMBER)
#define CMDBUF_VCMD_REGISTER_TOTAL_SIZE       9*1024*1024-CMDBUF_POOL_TOTAL_SIZE*2
#define VCMD_REGISTER_SIZE                    (128*4)

static void vcmd_reset_asic(struct hantrovcmd_dev * dev);
static void vcmd_reset_current_asic(struct hantrovcmd_dev * dev);

static int hantrovcmd_isr(int irq, void *dev_id);

//#define VCMD_DEBUG_INTERNAL

//#define CMODEL_VCMD_DRIVER_DEBUG
#undef PDEBUG   /* undef it, just in case */
#ifdef CMODEL_VCMD_DRIVER_DEBUG
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define PDEBUG(fmt, ...)  /* not debugging: nothing */


#endif

/*for all vcmds, the core info should be listed here for subsequent use*/
struct vcmd_config vcmd_core_array[]= {
    //decoder configuration
    {VCMD_DEC_IO_ADDR_0,
    VCMD_DEC_IO_SIZE_0,
    VCMD_DEC_INT_PIN_0,
    VCMD_DEC_MODULE_TYPE_0,
    VCMD_DEC_MODULE_MAIN_ADDR_0,
    VCMD_DEC_MODULE_DEC400_ADDR_0,
    VCMD_DEC_MODULE_L2CACHE_ADDR_0,
    VCMD_DEC_MODULE_MMU_ADDR_0},


    {VCMD_DEC_IO_ADDR_1,
    VCMD_DEC_IO_SIZE_1,
    VCMD_DEC_INT_PIN_1,
    VCMD_DEC_MODULE_TYPE_1,
    VCMD_DEC_MODULE_MAIN_ADDR_1,
    VCMD_DEC_MODULE_DEC400_ADDR_1,
    VCMD_DEC_MODULE_L2CACHE_ADDR_1,
    VCMD_DEC_MODULE_MMU_ADDR_1},

#if 0

    {VCMD_DEC_IO_ADDR_2,
    VCMD_DEC_IO_SIZE_2,
    VCMD_DEC_INT_PIN_2,
    VCMD_DEC_MODULE_TYPE_2,
    VCMD_DEC_MODULE_MAIN_ADDR_2,
    VCMD_DEC_MODULE_DEC400_ADDR_2,
    VCMD_DEC_MODULE_L2CACHE_ADDR_2,
    VCMD_DEC_MODULE_MMU_ADDR_2},

    {VCMD_DEC_IO_ADDR_3,
    VCMD_DEC_IO_SIZE_3,
    VCMD_DEC_INT_PIN_3,
    VCMD_DEC_MODULE_TYPE_3,
    VCMD_DEC_MODULE_MAIN_ADDR_3,
    VCMD_DEC_MODULE_DEC400_ADDR_3,
    VCMD_DEC_MODULE_L2CACHE_ADDR_3,
    VCMD_DEC_MODULE_MMU_ADDR_3},
#endif
};

/*these size need to be modified according to hw config.*/
#define VCMD_ENCODER_REGISTER_SIZE              (500 * 4)
#define VCMD_DECODER_REGISTER_SIZE              (500 * 4)
#define VCMD_IM_REGISTER_SIZE                   (500 * 4)
#define VCMD_JPEG_ENCODER_REGISTER_SIZE         (500 * 4)
#define VCMD_JPEG_DECODER_REGISTER_SIZE         (500 * 4)


static struct VwlMem vcmd_buf_mem_pool;
static struct VwlMem vcmd_status_buf_mem_pool;
static struct VwlMem vcmd_registers_mem_pool;

static u16 cmdbuf_used[TOTAL_DISCRETE_CMDBUF_NUM];
/* use this to avoid post cmdbuf 1 sem repeatedly */
static u16 cmdbuf1_release = 0;

/* use this sem to manage each cmdbuf */
static sem_t cmdbuf_sem[TOTAL_DISCRETE_CMDBUF_NUM];
/* use this sem to notice which cmdbuf done in mc mode. */
static sem_t mc_sem;
/* use this sem to judge if there is available cmdbuf. */
static sem_t reserved_sem;
//for reserve
sem_t vcmd_reserve_cmdbuf_sem[MAX_VCMD_TYPE];


static u16 cmdbuf_used_pos;
static u16 cmdbuf_used_residual;

static struct hantrovcmd_dev* vcmd_manager[MAX_VCMD_TYPE][MAX_CORE_NUMBER];
bi_list_node* global_cmdbuf_node[TOTAL_DISCRETE_CMDBUF_NUM];

static u16 vcmd_position[MAX_VCMD_TYPE];
static int vcmd_core_num[MAX_VCMD_TYPE];
//this variable used for mc model. when get 0xffffffff in CmodelIoctlWaitCmdbuf(),
//we return the cmdbuf which finished firstly.
FifoInst fifo_cmdbuf;
pthread_t vcmd_thread[MAX_CORE_NUMBER];
int vcmd_thread_run[MAX_CORE_NUMBER] = { 0 };
int vcmd_run_once[MAX_CORE_NUMBER] = {0};

sem_t vcmd_rdy[MAX_CORE_NUMBER];

/* store cores info */
static struct hantrovcmd_dev* hantrovcmd_data = NULL;
static int total_vcmd_core_num = 0;
static int vcmd_index[] = {0, 1, 2, 3};

/* NOTE: Don't use ',' in descriptions, because it is used as separator in csv
 * parsing. */
const regVcmdField_s asicVcmdRegisterDesc[] =
{
#include "../vcmd_pcidriver/vcmdregistertable.h"
};

void init_bi_list(bi_list* list);
bi_list_node* bi_list_create_node(void);
void bi_list_free_node(bi_list_node* node);
void bi_list_insert_node_tail(bi_list* list,bi_list_node* current_node);
void bi_list_insert_node_before(bi_list* list,bi_list_node* base_node,bi_list_node* new_node);
void bi_list_remove_node(bi_list* list,bi_list_node* current_node);

/*******************************************************************************
 Function name   : vcmd_read_reg
 Description     : 
 Return type     : u32 
 Argument        : u32 offset
*******************************************************************************/
u32 vwl_read_reg(void *hwregs, u32 offset) {
  u32 val;
  u32 *reg_base  = hwregs;

  val = reg_base[offset >> 2];

  PDEBUG("vcmd_read_reg 0x%02x --> %08x\n", offset, val);

  return val;
}

/*******************************************************************************
 Function name   : vwl_write_reg
 Description     : 
 Return type     : void 
 Argument        : u32 offset
 Argument        : u32 val
*******************************************************************************/
void vwl_write_reg(void *hwregs, u32 offset, u32 value) {
  u32 *reg_base  = hwregs;

  reg_base[offset >> 2] = value;

  PDEBUG("vwl_write_reg 0x%02x with value %08x\n", offset, value);
}

static inline void vwl_set_register_mirror_value(u32 *reg_mirror, regVcmdName name, u32 value) {
  const regVcmdField_s *field;
  u32 regVal;

  field = &asicVcmdRegisterDesc[name];

  /* Check that value fits in field */
  PDEBUG("field->name == name=%d/n",field->name == name);
  PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d/n",((field->mask >> field->lsb) << field->lsb) == field->mask);
  PDEBUG("(field->mask >> field->lsb) >= value=%d/n",(field->mask >> field->lsb) >= value);
  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d/n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  /* Clear previous value of field in register */
  regVal = reg_mirror[field->base / 4] & ~(field->mask);

  /* Put new value of field in register */
  reg_mirror[field->base / 4] = regVal | ((value << field->lsb) & field->mask);
}

static inline u32 vwl_get_register_mirror_value(u32 *reg_mirror, regVcmdName name) {
  const regVcmdField_s *field;
  u32 regVal;

  field = &asicVcmdRegisterDesc[name];

  /* Check that value fits in field */
  PDEBUG("field->name == name=%d/n",field->name == name);
  PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d/n",((field->mask >> field->lsb) << field->lsb) == field->mask);
  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d/n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  regVal = reg_mirror[field->base / 4];
  regVal = (regVal & field->mask) >> field->lsb;

  return regVal;
}


void vwl_write_register_value(void *hwregs,u32* reg_mirror,regVcmdName name, u32 value) {
  const regVcmdField_s *field;
  u32 regVal;

  field = &asicVcmdRegisterDesc[name];

  /* Check that value fits in field */
  PDEBUG("field->name == name=%d\n",field->name == name);
  PDEBUG("((field->mask >> field->lsb) << field->lsb) == field->mask=%d\n",((field->mask >> field->lsb) << field->lsb) == field->mask);
  PDEBUG("(field->mask >> field->lsb) >= value=%d\n",(field->mask >> field->lsb) >= value);
  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT*4=%d\n",field->base < ASIC_VCMD_SWREG_AMOUNT*4);

  /* Clear previous value of field in register */
  regVal = reg_mirror[field->base/4] & ~(field->mask);

  /* Put new value of field in register */
  reg_mirror[field->base/4] = regVal | ((value << field->lsb) & field->mask);

  /* write it into HW registers */
  vwl_write_reg(hwregs, field->base,reg_mirror[field->base/4]);
}


u32 vwl_get_register_value(void *hwregs, u32* reg_mirror,regVcmdName name) {
  const regVcmdField_s *field;
  u32 value;

  field = &asicVcmdRegisterDesc[name];

  PDEBUG("field->base < ASIC_VCMD_SWREG_AMOUNT * 4=%d\n",field->base < ASIC_VCMD_SWREG_AMOUNT * 4);

  value = reg_mirror[field->base / 4] = vwl_read_reg(hwregs, field->base);
  value = (value & field->mask) >> field->lsb;

  return value;
}

#if defined(__LP64__) || defined(_WIN64) || defined(_WIN32)

#define vwl_set_addr_register_value(reg_base, reg_mirror, name, value) do {\
    vwl_write_register_value((reg_base), (reg_mirror),name, (u32)(value));  \
    vwl_write_register_value((reg_base), (reg_mirror),name##_MSB, (u32)((value) >> 32)); \
  } while (0)

#define VWLGetAddrRegisterValue(reg_base, reg_mirror,name)  \
  (((ptr_t)vwl_get_register_value((reg_base),(reg_mirror), name)) |  \
  (((ptr_t)vwl_get_register_value((reg_base), (reg_mirror),name##_MSB)) << 32))

#else 

#define vwl_set_addr_register_value(reg_base, reg_mirror,name, value) do {\
    vwl_write_register_value((reg_base),(reg_mirror), name, (u32)(value));  \
  } while (0)

#define VWLGetAddrRegisterValue(reg_base, reg_mirror,name)  \
  ((ptr_t)vwl_get_register_value((reg_base),(reg_mirror), (name)))

#endif


void init_bi_list(bi_list* list) {
  list->head = NULL;
  list->tail = NULL; 
}

bi_list_node* bi_list_create_node(void) {
  bi_list_node* node = NULL;
  node = malloc(sizeof(bi_list_node));
  if(node == NULL) {
    PDEBUG ("%s\n","vmalloc for node fail!");
    return node;
  }
  memset(node, 0, sizeof(bi_list_node)); 
  return node;
}
void bi_list_free_node(bi_list_node* node) {
  //free current node
  free(node);
  return;
}

void bi_list_insert_node_tail(bi_list* list, bi_list_node* current_node) {
  if(current_node == NULL) {
    PDEBUG ("%s\n","insert node tail  NULL");
    return;
  }
  if(list->tail) {
   current_node->previous = list->tail;
   list->tail->next = current_node;
   list->tail = current_node;
   list->tail->next = NULL;
  } else {
   list->head = current_node;
   list->tail = current_node;
   current_node->next = NULL;
   current_node->previous = NULL;
  }
  return;
}

void bi_list_insert_node_before(bi_list* list, bi_list_node* base_node, bi_list_node* new_node) {
  bi_list_node* temp_node_previous = NULL;
  if(new_node == NULL) {
    PDEBUG ("%s\n","insert node before new node NULL");
    return;
  }
  if(base_node) {
    if(base_node->previous) {
      //at middle position
      temp_node_previous = base_node->previous;
      temp_node_previous->next = new_node;
      new_node->next = base_node;
      base_node->previous = new_node;
      new_node->previous = temp_node_previous;
    } else {
      //at head
      base_node->previous = new_node;
      new_node->next = base_node;
      list->head = new_node;
      new_node->previous = NULL;
    }
  } else {
   //at tail
   bi_list_insert_node_tail(list, new_node);
  }
  return;
}

void bi_list_remove_node(bi_list* list, bi_list_node* current_node) {
  bi_list_node* temp_node_previous = NULL;
  bi_list_node* temp_node_next = NULL;
  if(current_node == NULL) {
    PDEBUG ("%s\n","remove node NULL");
    return;
  }
  temp_node_next = current_node->next;
  temp_node_previous = current_node->previous;
  
  if(temp_node_next == NULL && temp_node_previous ==  NULL ) {
    //there is only one node.
    list->head = NULL;
    list->tail = NULL;
  } else if(temp_node_next == NULL) {
    //at tail
    list->tail = temp_node_previous;
    temp_node_previous->next = NULL;
  } else if(temp_node_previous == NULL) {
    //at head
    list->head = temp_node_next;
    temp_node_next->previous = NULL;
  } else {
    //at middle position
    temp_node_previous->next = temp_node_next;
    temp_node_next->previous = temp_node_previous;
  }
  return;
}

static void *vc8kd_vcmd_thread(void* arg) {
  u32 core_id = *(u32 *)arg;
  int i;
  struct cmdbuf_obj* new_cmdbuf_obj = NULL;

  while (vcmd_thread_run[core_id]) {
    sem_wait(&vcmd_rdy[core_id]);
    vcmd_run_once[core_id] = 1;
    for (i = 1; i < TOTAL_DISCRETE_CMDBUF_NUM; i++) {
      if ((cmdbuf_used[i] == 1 && i != 1) ||
          (cmdbuf_used[i] == 1 && i == 1 && cmdbuf1_release == 0)) {
        new_cmdbuf_obj = (struct cmdbuf_obj*)global_cmdbuf_node[i]->data;
        if (new_cmdbuf_obj->core_id == core_id &&
            new_cmdbuf_obj->cmdbuf_run_done == 1 &&
            new_cmdbuf_obj->cmdbuf_posted != 1) {  //not post sem repeatedly
          new_cmdbuf_obj->cmdbuf_posted = 1;
          if (i == 1)
            cmdbuf1_release = 1;
          PDEBUG ("post cmdbuf %d\n",new_cmdbuf_obj->cmdbuf_id);
          sem_post(&cmdbuf_sem[new_cmdbuf_obj->cmdbuf_id]);
          if (i != 1) {
            FifoPush(fifo_cmdbuf, (FifoObject)(addr_t)new_cmdbuf_obj->cmdbuf_id, FIFO_EXCEPTION_DISABLE);
            sem_post(&mc_sem);
          }
        }
      }
    }
  }
  sem_post(&mc_sem);

  return NULL;
}

void PrintInstr(u32 i, u32 instr, u32 *size) {
  if ((instr & 0xF8000000) == OPCODE_WREG) {
    int length = ((instr >> 16) & 0x3FF);
    printf("current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n", i, instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
           length, (instr & 0xFFFF));
    *size = ((length+2)>>1)<<1;
  } else if ((instr & 0xF8000000) == OPCODE_END) {
    printf("current cmdbuf data %d = 0x%08x => [%s]\n", i, instr, "END");
    *size = 2;
  } else if ((instr & 0xF8000000) == OPCODE_NOP) {
    printf("current cmdbuf data %d = 0x%08x => [%s]\n", i, instr, "NOP");
    *size = 2;
  } else if ((instr & 0xF8000000) == OPCODE_RREG) {
    int length = ((instr >> 16) & 0x3FF);
    printf("current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n", i, instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
           length, (instr & 0xFFFF));
    *size = 4;
  } else if ((instr & 0xF8000000) == OPCODE_JMP) {
    printf("current cmdbuf data %d = 0x%08x => [%s %s %s]\n", i, instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
           ((instr >> 25) & 0x1) ? "IE" : "");
    *size = 4;
  } else if ((instr & 0xF8000000) == OPCODE_STALL) {
    printf("current cmdbuf data %d = 0x%08x => [%s %s 0x%x]\n", i, instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
           (instr & 0xFFFF));
    *size = 2;
  } else if ((instr & 0xF8000000) == OPCODE_CLRINT) {
    printf("current cmdbuf data %d = 0x%08x => [%s %d 0x%x]\n", i, instr, "CLRINT", (instr >> 25) & 0x3,
           (instr & 0xFFFF));
    *size = 2;
  } else
    *size = 1;
}

/**********************************************************************************************************\
*cmdbuf object management
\***********************************************************************************************************/
static struct cmdbuf_obj* create_cmdbuf_obj(void) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  cmdbuf_obj = malloc(sizeof(struct cmdbuf_obj));
  if(cmdbuf_obj == NULL) {
    PDEBUG ("%s\n","malloc for cmdbuf_obj fail!");
    return cmdbuf_obj; 
  }
  memset(cmdbuf_obj, 0, sizeof(struct cmdbuf_obj)); 
  return cmdbuf_obj; 
}

static void free_cmdbuf_obj(struct cmdbuf_obj* cmdbuf_obj) {
  if(cmdbuf_obj == NULL) {
    PDEBUG ("%s\n","remove_cmdbuf_obj NULL");
    return;
  }
  //free current cmdbuf_obj
  free(cmdbuf_obj);
  return; 
}

//calculate executing_time of each vcmd
static u32 calculate_executing_time_after_node(bi_list_node* exe_cmdbuf_node)
{
  u32 time_run_all=0;
  struct cmdbuf_obj* cmdbuf_obj_temp=NULL;
  while(1)
  {
    if(exe_cmdbuf_node==NULL)
      break;
    cmdbuf_obj_temp=(struct cmdbuf_obj* )exe_cmdbuf_node->data;
    time_run_all += cmdbuf_obj_temp->executing_time;
    exe_cmdbuf_node = exe_cmdbuf_node->next;
  }
  return time_run_all;
}
static u32 calculate_executing_time_after_node_high_priority(bi_list_node* exe_cmdbuf_node)
{
  u32 time_run_all=0;
  struct cmdbuf_obj* cmdbuf_obj_temp=NULL;
  if(exe_cmdbuf_node==NULL)
    return time_run_all;
  cmdbuf_obj_temp=(struct cmdbuf_obj* )exe_cmdbuf_node->data;
  time_run_all += cmdbuf_obj_temp->executing_time;
  exe_cmdbuf_node = exe_cmdbuf_node->next;
  while(1)
  {
    if(exe_cmdbuf_node==NULL)
      break;
    cmdbuf_obj_temp=(struct cmdbuf_obj* )exe_cmdbuf_node->data;
    if(cmdbuf_obj_temp->priority==CMDBUF_PRIORITY_NORMAL)
      break;
    time_run_all += cmdbuf_obj_temp->executing_time;
    exe_cmdbuf_node = exe_cmdbuf_node->next;
  }
  return time_run_all;
}


static i32 allocate_cmdbuf(struct VwlMem* new_cmdbuf_addr, struct VwlMem* new_status_cmdbuf_addr) {

  new_cmdbuf_addr->size = CMDBUF_MAX_SIZE;

  /* if there is not available cmdbuf, lock the function. */
  sem_wait(&reserved_sem);

  while(1) {
    if(cmdbuf_used[cmdbuf_used_pos] == 0 && (global_cmdbuf_node[cmdbuf_used_pos] == NULL )) {
      cmdbuf_used[cmdbuf_used_pos] = 1;
      cmdbuf_used_residual -= 1;
      new_cmdbuf_addr->cmdbuf_id = cmdbuf_used_pos;
      new_cmdbuf_addr->virtual_address = vcmd_buf_mem_pool.virtual_address + cmdbuf_used_pos*CMDBUF_MAX_SIZE/4;
      new_cmdbuf_addr->bus_address = (addr_t)new_cmdbuf_addr->virtual_address + cmdbuf_used_pos*CMDBUF_MAX_SIZE;

      new_status_cmdbuf_addr->virtual_address = vcmd_status_buf_mem_pool.virtual_address + cmdbuf_used_pos*CMDBUF_MAX_SIZE/4;
      new_status_cmdbuf_addr->bus_address = (addr_t)new_status_cmdbuf_addr->virtual_address + cmdbuf_used_pos*CMDBUF_MAX_SIZE;
      new_status_cmdbuf_addr->size = CMDBUF_MAX_SIZE;
      new_status_cmdbuf_addr->cmdbuf_id = cmdbuf_used_pos;
      cmdbuf_used_pos++;
      if(cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        cmdbuf_used_pos = 0;
      return 0;
    } else {
      cmdbuf_used_pos++;
      if(cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        cmdbuf_used_pos = 0;
    }
  }

  return -1;
}

static void free_cmdbuf_mem(u16 cmdbuf_id ) {

  cmdbuf_used[cmdbuf_id] = 0;
  cmdbuf_used_residual += 1;
  sem_post(&reserved_sem);

}

static bi_list_node* create_cmdbuf_node(void) {
  bi_list_node* current_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  struct VwlMem new_cmdbuf_addr;
  struct VwlMem new_status_cmdbuf_addr;

  allocate_cmdbuf(&new_cmdbuf_addr, &new_status_cmdbuf_addr);

  cmdbuf_obj = create_cmdbuf_obj();
  if(cmdbuf_obj == NULL) {
    PDEBUG ("%s\n","create_cmdbuf_obj fail!");
    free_cmdbuf_mem(new_cmdbuf_addr.cmdbuf_id);
    return NULL;
  }
  cmdbuf_obj->cmdbuf_bus_address = new_cmdbuf_addr.bus_address;
  cmdbuf_obj->cmdbuf_virtual_address = new_cmdbuf_addr.virtual_address;
  cmdbuf_obj->cmdbuf_size = new_cmdbuf_addr.size;
  cmdbuf_obj->cmdbuf_id = new_cmdbuf_addr.cmdbuf_id;
  cmdbuf_obj->status_bus_address = new_status_cmdbuf_addr.bus_address;
  cmdbuf_obj->status_virtual_address = new_status_cmdbuf_addr.virtual_address;
  cmdbuf_obj->status_size = new_status_cmdbuf_addr.size;
  current_node = bi_list_create_node();
  if(current_node == NULL) {
    PDEBUG ("%s\n","bi_list_create_node fail!");
    free_cmdbuf_mem(new_cmdbuf_addr.cmdbuf_id);
    free_cmdbuf_obj(cmdbuf_obj);
    return NULL;
  }
  current_node->data = (void*)cmdbuf_obj;
  current_node->next = NULL;
  current_node->previous = NULL;
  return current_node;
}

static void free_cmdbuf_node(bi_list_node* cmdbuf_node) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  if(cmdbuf_node == NULL) {
    PDEBUG ("%s\n","remove_cmdbuf_node NULL");
    return;
  }
  cmdbuf_obj = (struct cmdbuf_obj*)cmdbuf_node->data;
  //free cmdbuf mem in pool
  free_cmdbuf_mem(cmdbuf_obj->cmdbuf_id);
  
  //free struct cmdbuf_obj
  free_cmdbuf_obj(cmdbuf_obj);
  //free current cmdbuf_node entity.
  bi_list_free_node(cmdbuf_node);
  return; 
}

//just remove, not free the node.
static bi_list_node* remove_cmdbuf_node_from_list(bi_list* list,bi_list_node* cmdbuf_node) {
  if(cmdbuf_node == NULL) {
    PDEBUG ("%s\n","remove_cmdbuf_node_from_list  NULL");
    return NULL;
  }
  
  if(cmdbuf_node->next) {
    bi_list_remove_node(list, cmdbuf_node);
    return cmdbuf_node;
  } else {
    //the last one, should not be removed.
    return NULL;
  }
}

static bi_list_node* get_cmdbuf_node_in_list_by_addr(size_t  cmdbuf_addr,bi_list* list)
{
  bi_list_node* new_cmdbuf_node=NULL;
  struct cmdbuf_obj* cmdbuf_obj=NULL;
  new_cmdbuf_node=list->head;
  while(1)
  {
    if(new_cmdbuf_node==NULL)
      return NULL;
    cmdbuf_obj=(struct cmdbuf_obj*)new_cmdbuf_node->data;
    if(((cmdbuf_obj->cmdbuf_bus_address) <= cmdbuf_addr)&&(((cmdbuf_obj->cmdbuf_bus_address+cmdbuf_obj->cmdbuf_size) >cmdbuf_addr)) )
    {
      return new_cmdbuf_node;
    }
    new_cmdbuf_node=new_cmdbuf_node->next;
  }
  return NULL;
}

static i32 select_vcmd(bi_list_node* new_cmdbuf_node, u32 dwl_client_type) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  struct hantrovcmd_dev *dev = NULL;
  struct hantrovcmd_dev*smallest_dev = NULL;
  bi_list* list = NULL;
  bi_list_node* curr_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj_temp = NULL;
  int counter = 0;
  u32 executing_time = 0xffffffff;
  u32 vwl_client_type;
  u32 hw_rdy_cmdbuf_num = 0;
  size_t exe_cmdbuf_addr = 0;
  u32 cmdbuf_id = 0;

  cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;

  switch (dwl_client_type) {
  case DWL_CLIENT_TYPE_H264_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_H264_DEC;
    break;
  case DWL_CLIENT_TYPE_MPEG4_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_MPEG4_DEC;
    break;
  case DWL_CLIENT_TYPE_JPEG_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_JPEG_DEC;
    break;
  case DWL_CLIENT_TYPE_PP:
    vwl_client_type = VWL_CLIENT_TYPE_PP;
    break;
  case DWL_CLIENT_TYPE_VC1_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_VC1_DEC;
    break;
  case DWL_CLIENT_TYPE_MPEG2_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_MPEG2_DEC;
    break;
  case DWL_CLIENT_TYPE_VP6_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_VP6_DEC;
    break;
  case DWL_CLIENT_TYPE_AVS_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_AVS_DEC;
    break;
  case DWL_CLIENT_TYPE_RV_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_RV_DEC;
    break;
  case DWL_CLIENT_TYPE_VP8_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_VP8_DEC;
    break;
  case DWL_CLIENT_TYPE_VP9_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_VP9_DEC;
    break;
  case DWL_CLIENT_TYPE_HEVC_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_HEVC_DEC;
    break;
  case DWL_CLIENT_TYPE_ST_PP:
    vwl_client_type = VWL_CLIENT_TYPE_ST_PP;
    break;
  case DWL_CLIENT_TYPE_H264_MAIN10:
    vwl_client_type = VWL_CLIENT_TYPE_H264_MAIN10;
    break;
  case DWL_CLIENT_TYPE_AVS2_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_AVS2_DEC;
    break;
  case DWL_CLIENT_TYPE_AV1_DEC:
    vwl_client_type = VWL_CLIENT_TYPE_AV1_DEC;
    break;
  case DWL_CLIENT_TYPE_NONE:
    vwl_client_type = VWL_CLIENT_TYPE_ALL;
    break;
  default:
    return -1;
  }

  //FIXME :...
  (void)(vwl_client_type);

  //there is an empty vcmd to be used
  while(1) {
    dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
    list=&dev->list_manager;
    //FIXME:...
    if(/*(dev->formats & vwl_client_type) && */list->tail==NULL) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    } else {
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      counter++;
    }
    if(counter >= vcmd_core_num[cmdbuf_obj->module_type])
      break;
  }

  //there is a vcmd which tail node -> cmdbuf_run_done == 1. It means this vcmd has nothing to do, so we select it.
  counter = 0;
  while(1) {
    dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
    list = &dev->list_manager;
    curr_cmdbuf_node = list->tail;
    if(curr_cmdbuf_node == NULL) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }
    cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
    if(cmdbuf_obj_temp->cmdbuf_run_done == 1) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    } else {
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      counter++;
    }
    if(counter >= vcmd_core_num[cmdbuf_obj->module_type])
      break;
  }

  //another case, tail = executing node, and vcmd=pend state (finish but not generate interrupt)
  counter =0;
  while(1) {
    dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
    list=&dev->list_manager;
    //read executing cmdbuf address
    if(dev->hw_version_id <= VCMD_HW_ID_1_0_C )
      hw_rdy_cmdbuf_num = vwl_get_register_value((void *)dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT);
    else {
      hw_rdy_cmdbuf_num = *(dev->vcmd_reg_mem_virtual_address+VCMD_EXE_CMDBUF_COUNT);
      if(hw_rdy_cmdbuf_num!=dev->sw_cmdbuf_rdy_num)
        hw_rdy_cmdbuf_num += 1;
    }
    curr_cmdbuf_node = list->tail;
    if(curr_cmdbuf_node == NULL)
    {
      bi_list_insert_node_tail(list,new_cmdbuf_node);
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type]=0;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }

    if((dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num))
    {
      bi_list_insert_node_tail(list,new_cmdbuf_node);
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type]=0;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }
    else
    {
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      counter++;
    }
    if(counter >= vcmd_core_num[cmdbuf_obj->module_type])
      break;
  }

  //there is no idle vcmd,if low priority,calculate exe time, select the least one.
  // or if high priority, calculate the exe time, select the least one and abort it.
  if(cmdbuf_obj->priority==CMDBUF_PRIORITY_NORMAL)
  {
    counter =0;
    //calculate total execute time of all devices
    while(1)
    {
      dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
      //read executing cmdbuf address
      if(dev->hw_version_id <= VCMD_HW_ID_1_0_C )
      {
        exe_cmdbuf_addr = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
        list=&dev->list_manager;
        //get the executing cmdbuf node.
        curr_cmdbuf_node=get_cmdbuf_node_in_list_by_addr(exe_cmdbuf_addr,list);

        //calculate total execute time of this device
        dev->total_exe_time=calculate_executing_time_after_node(curr_cmdbuf_node);
      }
      else
      {
        //cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
        cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address+EXECUTING_CMDBUF_ID_ADDR+1);
        if(cmdbuf_id>=TOTAL_DISCRETE_CMDBUF_NUM||(cmdbuf_id == 0 && vcmd_run_once[dev->core_id]))
        {
          printf("cmdbuf_id greater than the ceiling !!\n");
          return -1;
        }
        //get the executing cmdbuf node.
        curr_cmdbuf_node=global_cmdbuf_node[cmdbuf_id];
        if(curr_cmdbuf_node==NULL)
        {
          list=&dev->list_manager;
          curr_cmdbuf_node = list->head;
          while(1)
          {
            if(curr_cmdbuf_node == NULL)
              break;
            cmdbuf_obj_temp =(struct cmdbuf_obj*) curr_cmdbuf_node->data;
            if(cmdbuf_obj_temp->cmdbuf_data_linked&&cmdbuf_obj_temp->cmdbuf_run_done==0)
              break;
            curr_cmdbuf_node = curr_cmdbuf_node->next;
          }

        }

        //calculate total execute time of this device
        dev->total_exe_time=calculate_executing_time_after_node(curr_cmdbuf_node);

      }
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >=vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type]=0;
      counter++;
      if(counter>=vcmd_core_num[cmdbuf_obj->module_type])
        break;
    }
    //find the device with the least total_exe_time.
    counter =0;
    executing_time=0xffffffff;
    while(1)
    {
      dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
      if(dev->total_exe_time <= executing_time)
      {
        executing_time = dev->total_exe_time;
        smallest_dev = dev;
      }
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type]>=vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type]=0;
      counter++;
      if(counter>=vcmd_core_num[cmdbuf_obj->module_type])
        break;
    }
    //insert list
    list = &smallest_dev->list_manager;
    bi_list_insert_node_tail(list,new_cmdbuf_node);
    cmdbuf_obj->core_id = smallest_dev->core_id;
    return 0;
  }
  else
  {
    //CMDBUF_PRIORITY_HIGH
    counter =0;
    //calculate total execute time of all devices
    while(1)
    {
      dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
      if(dev->hw_version_id <= VCMD_HW_ID_1_0_C )
      {
        //read executing cmdbuf address
        exe_cmdbuf_addr = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
        list = &dev->list_manager;
        //get the executing cmdbuf node.
        curr_cmdbuf_node = get_cmdbuf_node_in_list_by_addr(exe_cmdbuf_addr,list);

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node_high_priority(curr_cmdbuf_node);
      }
      else
      {
        //cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
        cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address+EXECUTING_CMDBUF_ID_ADDR);
        if(cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM||(cmdbuf_id == 0 && vcmd_run_once[dev->core_id]))
        {
          printf("cmdbuf_id greater than the ceiling !!\n");
          return -1;
        }
        //get the executing cmdbuf node.
        curr_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
        if(curr_cmdbuf_node == NULL)
        {
          list = &dev->list_manager;
          curr_cmdbuf_node = list->head;
          while(1)
          {
            if(curr_cmdbuf_node == NULL)
              break;
            cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
            if(cmdbuf_obj_temp->cmdbuf_data_linked && cmdbuf_obj_temp->cmdbuf_run_done == 0)
              break;
            curr_cmdbuf_node = curr_cmdbuf_node->next;
          }
        }

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node(curr_cmdbuf_node);
      }
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      counter++;
      if(counter >= vcmd_core_num[cmdbuf_obj->module_type])
        break;
    }
    //find the smallest device.
    counter = 0;
    executing_time = 0xffffffff;
    while(1)
    {
      dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
      if(dev->total_exe_time <= executing_time)
      {
        executing_time = dev->total_exe_time;
        smallest_dev = dev;
      }
      vcmd_position[cmdbuf_obj->module_type]++;
      if(vcmd_position[cmdbuf_obj->module_type] >= vcmd_core_num[cmdbuf_obj->module_type])
        vcmd_position[cmdbuf_obj->module_type] = 0;
      counter++;
      if(counter >= vcmd_core_num[cmdbuf_obj->module_type])
        break;
    }
    //abort the vcmd and wait
    vwl_write_register_value((void *)smallest_dev->hwregs, smallest_dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
    //FIXME
    //if(wait_event_interruptible(*smallest_dev->wait_abort_queue, wait_abort_rdy(dev)) )
    //  return -ERESTARTSYS;

    //need to select inserting position again because hw maybe have run to the next node.
    //CMDBUF_PRIORITY_HIGH
    curr_cmdbuf_node = smallest_dev->list_manager.head;
    while(1)
    {
     //if list is empty or tail,insert to tail
     if(curr_cmdbuf_node == NULL)
      break;
     cmdbuf_obj_temp = (struct cmdbuf_obj*)curr_cmdbuf_node->data;
     //if find the first node which priority is normal, insert node prior to  the node
     if((cmdbuf_obj_temp->priority == CMDBUF_PRIORITY_NORMAL) && (cmdbuf_obj_temp->cmdbuf_run_done == 0))
      break;
     curr_cmdbuf_node = curr_cmdbuf_node->next;
    }
    bi_list_insert_node_before(list, curr_cmdbuf_node, new_cmdbuf_node);
    cmdbuf_obj->core_id = smallest_dev->core_id;

    return 0;
  }

  return 0;
}


static i32 reserve_cmdbuf(struct exchange_parameter* input_para) {
  bi_list_node* new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj = NULL;

  input_para->cmdbuf_id = 0;
  if(input_para->cmdbuf_size > CMDBUF_MAX_SIZE) {
    return -1;
  }

  new_cmdbuf_node = create_cmdbuf_node();
  if(new_cmdbuf_node == NULL)
    return -1;

  //set info from dwl
  cmdbuf_obj = (struct cmdbuf_obj* )new_cmdbuf_node->data;
  cmdbuf_obj->module_type = input_para->module_type;
  cmdbuf_obj->priority = input_para->priority;
  cmdbuf_obj->executing_time = input_para->executing_time;
  cmdbuf_obj->cmdbuf_size = CMDBUF_MAX_SIZE;
  cmdbuf_obj->cmdbuf_posted = 0;
  input_para->cmdbuf_size = CMDBUF_MAX_SIZE;

  //return cmdbuf_id to dwl
  input_para->cmdbuf_id = cmdbuf_obj->cmdbuf_id;
  global_cmdbuf_node[input_para->cmdbuf_id] = new_cmdbuf_node;
  PDEBUG ("reserve cmdbuf %d\n",input_para->cmdbuf_id);
  return 0;
}

static long release_cmdbuf_node_cleanup(bi_list* list) {
  bi_list_node* new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  while(1) {
    new_cmdbuf_node = list->head;
    if(new_cmdbuf_node == NULL)
      return 0;
    //remove node from list
    bi_list_remove_node(list, new_cmdbuf_node);
    //free node
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
    free_cmdbuf_node(new_cmdbuf_node);
  }
  return 0;
}

static bi_list_node* find_last_linked_cmdbuf(bi_list_node* current_node) {
  bi_list_node* new_cmdbuf_node = current_node;
  bi_list_node* last_cmdbuf_node;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  if(current_node == NULL)
    return NULL;
  last_cmdbuf_node = new_cmdbuf_node;
  new_cmdbuf_node = new_cmdbuf_node->previous;
  while(1) {
    if(new_cmdbuf_node == NULL)
      return last_cmdbuf_node;
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    if(cmdbuf_obj->cmdbuf_data_linked) {
      return new_cmdbuf_node;
    }
    last_cmdbuf_node = new_cmdbuf_node;
    new_cmdbuf_node = new_cmdbuf_node->previous;
  }
  return NULL;
}

static i32 release_cmdbuf(u16 cmdbuf_id) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  bi_list_node* last_cmdbuf_node = NULL;
  bi_list_node* new_cmdbuf_node = NULL;
  bi_list* list = NULL;
  u32 module_type;

  struct hantrovcmd_dev* dev = NULL;
  /*get cmdbuf object according to cmdbuf_id*/
  new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
  if(new_cmdbuf_node == NULL) {
    //should not happen
    PDEBUG ("%s\n","ERROR cmdbuf_id !!\n");
    return -1;
  }
  cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
  module_type = cmdbuf_obj->module_type;
  sem_wait(&vcmd_reserve_cmdbuf_sem[module_type]);
  //TODO
  dev = &hantrovcmd_data[cmdbuf_obj->core_id];

  list = &dev->list_manager;
  cmdbuf_obj->cmdbuf_need_remove = 1;
  last_cmdbuf_node = new_cmdbuf_node->previous;
  while(1) {
    //remove current node
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    if(cmdbuf_obj->cmdbuf_need_remove == 1) {
      new_cmdbuf_node = remove_cmdbuf_node_from_list(list,new_cmdbuf_node);
      if(new_cmdbuf_node) {
        //free node
        global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
        dev->total_exe_time -= cmdbuf_obj->executing_time;
        free_cmdbuf_node(new_cmdbuf_node);
      }
    }
    if(last_cmdbuf_node == NULL)
      break;
    new_cmdbuf_node = last_cmdbuf_node;
    last_cmdbuf_node = new_cmdbuf_node->previous;
  }
  sem_post(&vcmd_reserve_cmdbuf_sem[module_type]);
  return 0;
}


static void vcmd_link_cmdbuf(struct hantrovcmd_dev *dev,bi_list_node* last_linked_cmdbuf_node) {
  bi_list_node* new_cmdbuf_node = NULL;
  bi_list_node* next_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  struct cmdbuf_obj* next_cmdbuf_obj = NULL;
  u32 * jmp_addr = NULL;
  u32 operation_code;
  new_cmdbuf_node = last_linked_cmdbuf_node;

  //for the first cmdbuf.
  if(new_cmdbuf_node != NULL) {
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    if(cmdbuf_obj->cmdbuf_data_linked == 0) {
      dev->sw_cmdbuf_rdy_num++;
      cmdbuf_obj->cmdbuf_data_linked = 1;
      dev->duration_without_int = 0;
      if(cmdbuf_obj->has_end_cmdbuf == 0) {
        if(cmdbuf_obj->has_nop_cmdbuf == 1) {
          dev->duration_without_int = cmdbuf_obj->executing_time;
          //maybe nop is modified, so write back. 
          if(dev->duration_without_int >= INT_MIN_SUM_OF_IMAGE_SIZE) {
            jmp_addr = cmdbuf_obj->cmdbuf_virtual_address + (cmdbuf_obj->cmdbuf_size / 4);
            operation_code = *(jmp_addr - 4);
            operation_code = JMP_IE_1 | operation_code;
            *(jmp_addr - 4) = operation_code;
            dev->duration_without_int = 0;
          }
        }
      }
    }
  }

  while(1) {
    if(new_cmdbuf_node == NULL)
      break;
    /* first cmdbuf has been added.*/
    if(new_cmdbuf_node->next == NULL)
      break;
    next_cmdbuf_node = new_cmdbuf_node->next;
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    next_cmdbuf_obj = (struct cmdbuf_obj*)next_cmdbuf_node->data;
    if(cmdbuf_obj->has_end_cmdbuf == 0) {
      //need to link， current cmdbuf link to next cmdbuf
      jmp_addr = cmdbuf_obj->cmdbuf_virtual_address + (cmdbuf_obj->cmdbuf_size/4);
      if(dev->hw_version_id > VCMD_HW_ID_1_0_C )
      {
        //set next cmdbuf id
        *(jmp_addr-1) = next_cmdbuf_obj->cmdbuf_id;
      }
      if(sizeof(addr_t) == 8) {
        *(jmp_addr-2) = (u32)(((u64)(addr_t)next_cmdbuf_obj->cmdbuf_virtual_address >> 32));
      } else {
        assert(((u64)(addr_t)next_cmdbuf_obj->cmdbuf_virtual_address >> 32) == 0);
        *(jmp_addr-2) = 0;
      }
      *(jmp_addr-3) = (u32)((addr_t)next_cmdbuf_obj->cmdbuf_virtual_address);
      operation_code = *(jmp_addr-4);
      operation_code >>= 16;
      operation_code <<= 16;
      *(jmp_addr-4) = (u32)(operation_code |JMP_RDY_1|((next_cmdbuf_obj->cmdbuf_size+7)/8));
      next_cmdbuf_obj->cmdbuf_data_linked = 1;
      dev->sw_cmdbuf_rdy_num++;
      //modify nop code of next cmdbuf
      if(next_cmdbuf_obj->has_end_cmdbuf == 0) {
        if(next_cmdbuf_obj->has_nop_cmdbuf == 1) {
          dev->duration_without_int += next_cmdbuf_obj->executing_time;
         
          //maybe we see the modified nop before abort, so need to write back. 
          if(dev->duration_without_int >= INT_MIN_SUM_OF_IMAGE_SIZE) {
            jmp_addr = next_cmdbuf_obj->cmdbuf_virtual_address + (next_cmdbuf_obj->cmdbuf_size/4);
            operation_code = *(jmp_addr-4);
            operation_code = JMP_IE_1|operation_code;
            *(jmp_addr-4) = operation_code;
            dev->duration_without_int = 0;
          }
        }
      } else {
        dev->duration_without_int = 0;
      }
      
#ifdef VCMD_DEBUG_INTERNAL
      {
         u32 i;
         printf("vcmd link, last cmdbuf content\n");
         for(i=cmdbuf_obj->cmdbuf_size/4 -8;i<cmdbuf_obj->cmdbuf_size/4;i++)
         {
           printf("current linked cmdbuf data %d =0x%x\n",i,*(cmdbuf_obj->cmdbuf_virtual_address+i));
         }
      }
#endif
    } else {
      break;
    }
    
    new_cmdbuf_node = new_cmdbuf_node->next;
  }
  return;
}

static void vcmd_delink_cmdbuf(struct hantrovcmd_dev *dev,bi_list_node* last_linked_cmdbuf_node)
{
  bi_list_node* new_cmdbuf_node=NULL;
  struct cmdbuf_obj* cmdbuf_obj=NULL;

  new_cmdbuf_node = last_linked_cmdbuf_node;
  while(1)
  {
    if(new_cmdbuf_node==NULL)
      break;
    cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
    if(cmdbuf_obj->cmdbuf_data_linked)
    {
      cmdbuf_obj->cmdbuf_data_linked = 0;
    }
    else
      break;
    new_cmdbuf_node = new_cmdbuf_node->next;
  }
  dev->sw_cmdbuf_rdy_num=0;
}

static void vcmd_start(struct hantrovcmd_dev *dev, bi_list_node* first_linked_cmdbuf_node) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  
  if(dev->working_state == WORKING_STATE_IDLE) { 
    if((first_linked_cmdbuf_node != NULL) && dev->sw_cmdbuf_rdy_num) {
      cmdbuf_obj = (struct cmdbuf_obj*)first_linked_cmdbuf_node->data;
      //0x40
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE, 0);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1);//this bit should be set 1 only when need to reset dec400
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_CORE_CLK_GATE_DISABLE, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_CORE, 0);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_ALL, 0);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
      //0x48
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_INTCMD_EN, 0xffff);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_RESET_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
      //0x4c
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_TIMEOUT_EN, 1);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_TIMEOUT_CYCLES, 0x1dcd6500);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR, (u32)((addr_t)cmdbuf_obj->cmdbuf_virtual_address));
      if(sizeof(addr_t) == 8) {
        vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, (u32)(((u64)(addr_t)cmdbuf_obj->cmdbuf_virtual_address)>>32));
      } else {
        vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
      }
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH, (u32)((cmdbuf_obj->cmdbuf_size+7)/8));
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT, dev->sw_cmdbuf_rdy_num);
      vwl_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);

      if(dev->hw_version_id > VCMD_HW_ID_1_0_C ) {
        vwl_write_register_value((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID,(u32)cmdbuf_obj->cmdbuf_id);
      }

      vwl_write_reg((void *)dev->hwregs, 0x44, vwl_read_reg((void *)dev->hwregs, 0x44));
      vwl_write_reg((void *)dev->hwregs, 0x48, dev->reg_mirror[0x48/4]);
      vwl_write_reg((void *)dev->hwregs, 0x4c, dev->reg_mirror[0x4c/4]);
      vwl_write_reg((void *)dev->hwregs, 0x50, dev->reg_mirror[0x50/4]);
      vwl_write_reg((void *)dev->hwregs, 0x54, dev->reg_mirror[0x54/4]);
      vwl_write_reg((void *)dev->hwregs, 0x58, dev->reg_mirror[0x58/4]);
      vwl_write_reg((void *)dev->hwregs, 0x5c, dev->reg_mirror[0x5c/4]);
      vwl_write_reg((void *)dev->hwregs, 0x60, dev->reg_mirror[0x60/4]);
      
      dev->working_state = WORKING_STATE_WORKING;
      //start
      vwl_set_register_mirror_value(dev->reg_mirror,HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE,0);//this bit should be set 1 only when need to reset dec400
      vwl_set_register_mirror_value(dev->reg_mirror,HWIF_VCMD_START_TRIGGER,1);
      vwl_write_reg((void *)dev->hwregs, 0x40, dev->reg_mirror[0x40/4]);

    }
  }

}

static i32 link_and_run_cmdbuf(struct exchange_parameter* input_para) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  bi_list_node* new_cmdbuf_node = NULL;
  u16 cmdbuf_id = input_para->cmdbuf_id;
  int ret;
  u32* jmp_addr = NULL;
  u32 opCode;
  u32 tempOpcode;
  struct hantrovcmd_dev* dev = NULL;
  bi_list_node* last_cmdbuf_node;
  u32 record_last_cmdbuf_rdy_num;

  new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
  if(new_cmdbuf_node == NULL) {
    //should not happen
    PDEBUG ("%s\n","vcmd: ERROR cmdbuf_id !!\n");
    return -1;
  }

  cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
  cmdbuf_obj->cmdbuf_data_loaded = 1;
  cmdbuf_obj->cmdbuf_size = input_para->cmdbuf_size;

#ifdef VCMD_DEBUG_INTERNAL
  {
     u32 i, inst = 0, size = 0;
     printf("vcmd link, current cmdbuf content\n");
     for(i=0;i<cmdbuf_obj->cmdbuf_size/4;i++)
     {
       if (i == inst) {
         PrintInstr(i, *(cmdbuf_obj->cmdbuf_virtual_address+i), &size);
         inst += size;
       } else {
         printf("current cmdbuf data %d =0x%x\n",i,*(cmdbuf_obj->cmdbuf_virtual_address+i));
       }
     }
  }
#endif
  

  //test nop and end opcode, then assign value.
  cmdbuf_obj->has_end_cmdbuf = 0; //0: has jmp opcode,1 has end code
  cmdbuf_obj->no_normal_int_cmdbuf = 0; //0: interrupt when JMP,1 not interrupt when JMP
  jmp_addr = cmdbuf_obj->cmdbuf_virtual_address + (cmdbuf_obj->cmdbuf_size/4);
  opCode = tempOpcode = *(jmp_addr-4);
  opCode >>= 27;
  opCode <<= 27;
  //we can't identify END opcode or JMP opcode, so we don't support END opcode in control sw and driver.
  if(opCode == OPCODE_JMP)
  {
    //jmp
    opCode=tempOpcode;
    opCode &=0x02000000;
    if(opCode == JMP_IE_1)
    {
      cmdbuf_obj->no_normal_int_cmdbuf=0;
    }
    else
    {
      cmdbuf_obj->no_normal_int_cmdbuf=1;
    }
  }
  else
  {
    //not support other opcode
    return -1;
  }

  sem_wait(&vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);

  ret = select_vcmd(new_cmdbuf_node, input_para->client_type);
  if(ret)
    return ret;

  dev = &hantrovcmd_data[cmdbuf_obj->core_id];
  input_para->core_id = cmdbuf_obj->core_id;
  dev->total_exe_time += cmdbuf_obj->executing_time;
  PDEBUG ("Allocate cmd buffer [%d] to core [%d]\n", cmdbuf_id, input_para->core_id);

  if(dev->hw_version_id > VCMD_HW_ID_1_0_C ) {
    //read vcmd executing register into ddr memory.
    //now core id is got and output ddr address of vcmd register can be filled in.
    //each core has its own fixed output ddr address of vcmd registers.
    jmp_addr = cmdbuf_obj->cmdbuf_virtual_address;
    if(sizeof(addr_t) == 8) {
      *(jmp_addr + 2) = (u32)((u64)(dev->vcmd_reg_mem_bus_address + (EXECUTING_CMDBUF_ID_ADDR+1)*4)>>32);
    } else {
      *(jmp_addr + 2) = 0;
    }
    *(jmp_addr+1) = (u32)((dev->vcmd_reg_mem_bus_address + (EXECUTING_CMDBUF_ID_ADDR+1)*4));

    jmp_addr = cmdbuf_obj->cmdbuf_virtual_address + (cmdbuf_obj->cmdbuf_size/4);
    //read vcmd all registers into ddr memory.
    //now core id is got and output ddr address of vcmd registers can be filled in.
    //each core has its own fixed output ddr address of vcmd registers.
    if(sizeof(addr_t) == 8) {
      *(jmp_addr-6) = (u32)((u64)dev->vcmd_reg_mem_bus_address>>32);
    } else {
      *(jmp_addr-6) = 0;
    }

    *(jmp_addr-7) = (u32)(dev->vcmd_reg_mem_bus_address);
  }


  last_cmdbuf_node = find_last_linked_cmdbuf(new_cmdbuf_node);
  record_last_cmdbuf_rdy_num = dev->sw_cmdbuf_rdy_num;
  vcmd_link_cmdbuf(dev, last_cmdbuf_node);
  if(dev->working_state == WORKING_STATE_IDLE) {
    //run
    vcmd_start(dev, last_cmdbuf_node);
  } else {
    //just update cmdbuf ready number 
    if(record_last_cmdbuf_rdy_num != dev->sw_cmdbuf_rdy_num)
      vwl_write_register_value((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_RDY_CMDBUF_COUNT,dev->sw_cmdbuf_rdy_num);
  }

  sem_post(&vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);

  return 0;
}

static unsigned int wait_cmdbuf_ready(u16 *cmd_buf_id) {
  addr_t i;
  i32 ret;

  if (*cmd_buf_id == ANY_CMDBUF_ID) {
    sem_wait(&mc_sem);
    if ((ret = FifoPop(fifo_cmdbuf, (FifoObject *)&i, FIFO_EXCEPTION_ENABLE)) != FIFO_ABORT) {
      if (ret == FIFO_EMPTY)
        return -1;
      *cmd_buf_id = i;
      return sem_wait(&cmdbuf_sem[i]);
    } else {
      return -1;
    }
  } else {
    return sem_wait(&cmdbuf_sem[*cmd_buf_id]);
  }
  return -1;
}

static void create_read_all_registers_cmdbuf(struct exchange_parameter* input_para)
{
    u32 register_range[] = {VCMD_ENCODER_REGISTER_SIZE,
                    VCMD_IM_REGISTER_SIZE,
                    VCMD_DECODER_REGISTER_SIZE,
                    VCMD_JPEG_ENCODER_REGISTER_SIZE,
                    VCMD_JPEG_DECODER_REGISTER_SIZE};
    u32 counter_cmdbuf_size=0;
    u32 * set_base_addr=vcmd_buf_mem_pool.virtual_address + input_para->cmdbuf_id*CMDBUF_MAX_SIZE/4;
    //u32 *status_base_virt_addr=vcmd_status_buf_mem_pool.virtualAddress + input_para->cmdbuf_id*CMDBUF_MAX_SIZE/4+(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr/4+0);
    ptr_t status_base_phy_addr=vcmd_status_buf_mem_pool.bus_address + input_para->cmdbuf_id*CMDBUF_MAX_SIZE+(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_main_addr+0);

    if(vcmd_manager[input_para->module_type][0]->hw_version_id > VCMD_HW_ID_1_0_C)
    {
      PDEBUG("vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",vcmd_manager[input_para->module_type][0]->hw_version_id);

      //read vcmd executing cmdbuf id registers to ddr for balancing core load.
      *(set_base_addr+0) = (OPCODE_RREG) |(1<<16) |(EXECUTING_CMDBUF_ID_ADDR*4);
      counter_cmdbuf_size += 4;
      *(set_base_addr+1) = (u32)0; //will be changed in link stage
      counter_cmdbuf_size += 4;
      *(set_base_addr+2) = (u32)0; //will be changed in link stage
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+3) = 0;
      counter_cmdbuf_size += 4;

      //read all registers
      *(set_base_addr+4) = (OPCODE_RREG) |((register_range[input_para->module_type]/4)<<16) |(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_main_addr+0);
      counter_cmdbuf_size += 4;
      *(set_base_addr+5) = (u32)(status_base_phy_addr);
      counter_cmdbuf_size += 4;
      if(sizeof(addr_t) == 8) {
        *(set_base_addr+6) = (u32)((u64)(status_base_phy_addr)>>32);
      } else {
        *(set_base_addr+6) = 0;
      }
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+7) = 0;
      counter_cmdbuf_size += 4;
#if 0
      //INT code, interrupt immediately
      *(set_base_addr+4) = (OPCODE_INT) |0 |input_para->cmdbuf_id;
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+5) = 0;
      counter_cmdbuf_size += 4;
#endif
      //read vcmd registers to ddr
      *(set_base_addr+8) = (OPCODE_RREG) |(27<<16) |(0);
      counter_cmdbuf_size += 4;
      *(set_base_addr+9) = (u32)0; //will be changed in link stage
      counter_cmdbuf_size += 4;
      *(set_base_addr+10) = (u32)0; //will be changed in link stage
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+11) = 0;
      counter_cmdbuf_size += 4;
      //JMP RDY = 0
      *(set_base_addr +12)= (OPCODE_JMP_RDY0) |0 |JMP_IE_1|0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +13) = 0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +14) = 0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +15) = input_para->cmdbuf_id;
      //don't add the last alignment DWORD in order to  identify END command or JMP command.
      //counter_cmdbuf_size += 4;
      input_para->cmdbuf_size=16*4;
    }
    else
    {
      PDEBUG("vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",vcmd_manager[input_para->module_type][0]->hw_version_id);
      //read all registers
      *(set_base_addr+0) = (OPCODE_RREG) |((register_range[input_para->module_type]/4)<<16) |(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_main_addr+0);
      counter_cmdbuf_size += 4;
      *(set_base_addr+1) = (u32)(status_base_phy_addr);
      counter_cmdbuf_size += 4;
      if(sizeof(addr_t) == 8) {
        *(set_base_addr+2) = (u32)((u64)(status_base_phy_addr)>>32);
      } else {
        *(set_base_addr+2) = 0;
      }
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+3) = 0;
      counter_cmdbuf_size += 4;
#if 0
      //INT code, interrupt immediately
      *(set_base_addr+4) = (OPCODE_INT) |0 |input_para->cmdbuf_id;
      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr+5) = 0;
      counter_cmdbuf_size += 4;
#endif
      //JMP RDY = 0
      *(set_base_addr +4)= (OPCODE_JMP_RDY0) |0 |JMP_IE_1|0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +5) = 0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +6) = 0;
      counter_cmdbuf_size += 4;
      *(set_base_addr +7) = input_para->cmdbuf_id;
      //don't add the last alignment DWORD in order to  identify END command or JMP command.
      //counter_cmdbuf_size += 4;
      input_para->cmdbuf_size=8*4;
    }
}

static void read_main_module_all_registers(u32 main_module_type)
{
  int ret;
  struct exchange_parameter input_para;
  u32 *status_base_virt_addr;
  u16 cmd_buf_id;

  input_para.executing_time = 0;
  input_para.priority = CMDBUF_PRIORITY_NORMAL;
  input_para.module_type = main_module_type;
  input_para.client_type = DWL_CLIENT_TYPE_NONE; //just for get info
  input_para.cmdbuf_size = 0;
  ret = reserve_cmdbuf(&input_para);
  (void)(ret);
  vcmd_manager[main_module_type][0]->status_cmdbuf_id = input_para.cmdbuf_id;
  create_read_all_registers_cmdbuf(&input_para);
  link_and_run_cmdbuf(&input_para);
  //sleep(1);
  //hantrovcmd_isr(input_para.core_id, &hantrovcmd_data[input_para.core_id]);
  cmd_buf_id = input_para.cmdbuf_id;
  wait_cmdbuf_ready(&cmd_buf_id);
  status_base_virt_addr=vcmd_status_buf_mem_pool.virtual_address + input_para.cmdbuf_id*CMDBUF_MAX_SIZE/4+(vcmd_manager[main_module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_main_addr/4+0);
  printf ("vc8000_vcmd_driver: main module register 0:0x%x\n",*status_base_virt_addr);
  printf ("vc8000_vcmd_driver: main module register 50:0x%x\n",*(status_base_virt_addr+50));
  printf ("vc8000_vcmd_driver: main module register 54:0x%x\n",*(status_base_virt_addr+54));
  printf ("vc8000_vcmd_driver: main module register 56:0x%x\n",*(status_base_virt_addr+56));
  printf ("vc8000_vcmd_driver: main module register 309:0x%x\n",*(status_base_virt_addr+309));
  //don't release cmdbuf because it can be used repeatedly
  //release_cmdbuf(input_para.cmdbuf_id);
}


/* simulate ioctl reserve cmdbuf */
i32 CmodelIoctlReserveCmdbuf(struct exchange_parameter *param) {
  return reserve_cmdbuf(param);
}

i32 CmodelIoctlEnableCmdbuf(struct exchange_parameter *param) {
  return link_and_run_cmdbuf(param);
}

i32 CmodelIoctlWaitCmdbuf(u16 *cmd_buf_id) {
  return wait_cmdbuf_ready(cmd_buf_id);
}

i32 CmodelIoctlReleaseCmdbuf(u32 cmd_buf_id) {
  PDEBUG ("release cmdbuf %d\n",cmd_buf_id);
  return release_cmdbuf(cmd_buf_id);
}

i32 CmodelIoctlGetCmdbufParameter(struct cmdbuf_mem_parameter *vcmd_mem_params) {
  //get cmd buffer info
  vcmd_mem_params->cmdbuf_unit_size = CMDBUF_MAX_SIZE;
  vcmd_mem_params->status_cmdbuf_unit_size = CMDBUF_MAX_SIZE;
  vcmd_mem_params->cmdbuf_total_size = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_mem_params->status_cmdbuf_total_size = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_mem_params->virt_status_cmdbuf_addr = vcmd_status_buf_mem_pool.virtual_address;
  vcmd_mem_params->phy_status_cmdbuf_addr = vcmd_status_buf_mem_pool.bus_address;
  vcmd_mem_params->virt_cmdbuf_addr = vcmd_buf_mem_pool.virtual_address;
  vcmd_mem_params->phy_cmdbuf_addr = vcmd_buf_mem_pool.bus_address;

  return 0;
}

i32 CmodelIoctlGetVcmdParameter(struct config_parameter *vcmd_params) {
  //get vcmd info
  if(vcmd_core_num[vcmd_params->module_type]) {
    vcmd_params->submodule_main_addr = vcmd_manager[vcmd_params->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_main_addr;
    vcmd_params->submodule_dec400_addr = vcmd_manager[vcmd_params->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_dec400_addr;
    vcmd_params->submodule_L2Cache_addr = vcmd_manager[vcmd_params->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_L2Cache_addr;
    vcmd_params->submodule_MMU_addr = vcmd_manager[vcmd_params->module_type][0]->vcmd_core_cfg.vcmd_core_config_ptr->submodule_MMU_addr;
    vcmd_params->config_status_cmdbuf_id = vcmd_manager[vcmd_params->module_type][0]->status_cmdbuf_id;
    vcmd_params->vcmd_core_num = vcmd_core_num[vcmd_params->module_type];
    vcmd_params->vcmd_hw_version_id = vcmd_manager[vcmd_params->module_type][0]->vcmd_core_cfg.vcmd_hw_version_id;
  } else {
    vcmd_params->submodule_main_addr = 0xffff;
    vcmd_params->submodule_dec400_addr = 0xffff;
    vcmd_params->submodule_L2Cache_addr = 0xffff;
    vcmd_params->submodule_MMU_addr = 0xffff;
    vcmd_params->config_status_cmdbuf_id = 0;
    vcmd_params->vcmd_core_num = 0;
  }

  return 0;
}



i32 CmodelVcmdInit(){
  int i, k;
  pthread_attr_t attr;

  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  total_vcmd_core_num = sizeof(vcmd_core_array)/sizeof(struct vcmd_config);
  hantrovcmd_data = (struct hantrovcmd_dev *)malloc(sizeof(struct hantrovcmd_dev)*total_vcmd_core_num);
  if (hantrovcmd_data == NULL) {
    fprintf(stderr,
            "error allocating memory for hantrovcmd data!\n");
    return -1;
  }
  
  memset(hantrovcmd_data, 0, sizeof(struct hantrovcmd_dev)*total_vcmd_core_num);
  
  vcmd_buf_mem_pool.virtual_address = (u32 *)malloc(CMDBUF_POOL_TOTAL_SIZE);
  if (!vcmd_buf_mem_pool.virtual_address) {
    fprintf(stderr,
            "error allocating memory for vcmdbuf!\n");
    free(hantrovcmd_data);
    return -1;
  }
  vcmd_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_buf_mem_pool.bus_address = (addr_t)vcmd_buf_mem_pool.virtual_address;
  vcmd_buf_mem_pool.cmdbuf_id = 0xFFFF;

  vcmd_status_buf_mem_pool.virtual_address = (u32 *)malloc(CMDBUF_POOL_TOTAL_SIZE);
  if (!vcmd_status_buf_mem_pool.virtual_address) {
    fprintf(stderr,
            "error allocating memory for vcmd status buf!\n");
    free(hantrovcmd_data);
    free(vcmd_buf_mem_pool.virtual_address);
    return -1;
  }

  if (FifoInit(TOTAL_DISCRETE_CMDBUF_NUM - 1, &fifo_cmdbuf) != FIFO_OK) {
    fprintf(stderr,
        "error fifo init!\n");
    free(hantrovcmd_data);
    free(vcmd_buf_mem_pool.virtual_address);
    return -1;
  }

  vcmd_status_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_status_buf_mem_pool.bus_address = (addr_t)vcmd_status_buf_mem_pool.virtual_address;
  vcmd_status_buf_mem_pool.cmdbuf_id = 0xFFFF;

  vcmd_registers_mem_pool.virtual_address = (u32 *)malloc(CMDBUF_VCMD_REGISTER_TOTAL_SIZE);
  if (!vcmd_registers_mem_pool.virtual_address) {
    fprintf(stderr,
            "error allocating memory for vcmd status buf!\n");
    free(hantrovcmd_data);
    free(vcmd_buf_mem_pool.virtual_address);
    free(vcmd_status_buf_mem_pool.virtual_address);
    return -1;
  }
  vcmd_registers_mem_pool.size = CMDBUF_VCMD_REGISTER_TOTAL_SIZE;
  vcmd_registers_mem_pool.bus_address = (addr_t)vcmd_registers_mem_pool.virtual_address;
  vcmd_registers_mem_pool.cmdbuf_id = 0xFFFF;

  for(k = 0; k < MAX_VCMD_TYPE; k++) {
    vcmd_core_num[k]=0;
    vcmd_position[k]=0;
    for(i=0;i<MAX_CORE_NUMBER;i++) {
      vcmd_manager[k][i]=NULL;
    }
  }


  for(i = 0; i < total_vcmd_core_num; i++) {
    hantrovcmd_data[i].vcmd_core_cfg.vcmd_core_config_ptr = &vcmd_core_array[i];
    hantrovcmd_data[i].vcmd_core_cfg.normal_int_callback = hantrovcmd_isr;
    hantrovcmd_data[i].vcmd_core_cfg.abnormal_int_callback = hantrovcmd_isr;
    hantrovcmd_data[i].vcmd_core_cfg.core_id = i;
    hantrovcmd_data[i].vcmd_core_cfg.vcmd_hw_version_id = VCMD_HW_ID_1_1_1;
    hantrovcmd_data[i].vcmd_core_cfg.dev_id = &hantrovcmd_data[i];
    hantrovcmd_data[i].hwregs = NULL;
    hantrovcmd_data[i].core_id = i;
    hantrovcmd_data[i].working_state= WORKING_STATE_IDLE;
    hantrovcmd_data[i].sw_cmdbuf_rdy_num = 0;
    hantrovcmd_data[i].total_exe_time = 0;
    //FIXME: support all formats 
    hantrovcmd_data[i].formats = VWL_CLIENT_TYPE_ALL;
    init_bi_list(&hantrovcmd_data[i].list_manager);
    hantrovcmd_data[i].duration_without_int = 0;
    vcmd_manager[vcmd_core_array[i].sub_module_type][i] = &hantrovcmd_data[i];
    vcmd_manager[vcmd_core_array[i].sub_module_type][i]->vcmd_core = AsicHwVcmdCoreInit(&vcmd_manager[vcmd_core_array[i].sub_module_type][i]->vcmd_core_cfg);
    vcmd_manager[vcmd_core_array[i].sub_module_type][i]->hwregs = (u8 *)AsicHwVcmdCoreGetBase(vcmd_manager[vcmd_core_array[i].sub_module_type][i]->vcmd_core);
    hantrovcmd_data[i].hw_version_id = *(u32*)vcmd_manager[vcmd_core_array[i].sub_module_type][i]->hwregs;
    hantrovcmd_data[i].vcmd_reg_mem_bus_address = vcmd_registers_mem_pool.bus_address + i*VCMD_REGISTER_SIZE;
    hantrovcmd_data[i].vcmd_reg_mem_virtual_address = vcmd_registers_mem_pool.virtual_address + i*VCMD_REGISTER_SIZE/4;
    hantrovcmd_data[i].vcmd_reg_mem_size = VCMD_REGISTER_SIZE;
    vcmd_core_num[vcmd_core_array[i].sub_module_type]++;
  }

  vcmd_reset_asic(hantrovcmd_data);

  //for cmdbuf management
  cmdbuf_used_pos = 0;
  for(k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
    cmdbuf_used[k] = 0;
    global_cmdbuf_node[k] = NULL;
  }

  cmdbuf_used_residual = TOTAL_DISCRETE_CMDBUF_NUM;
  cmdbuf_used_pos=1;
  cmdbuf_used[0]=1;
  cmdbuf_used_residual -=1;

  for (i = 0; i < TOTAL_DISCRETE_CMDBUF_NUM; i++) {
    sem_init(&cmdbuf_sem[i], 0, 0);
  }
  sem_init(&mc_sem, 0, 0);
  //donot use cmdbuf 0 in default.
  sem_init(&reserved_sem, 0, TOTAL_DISCRETE_CMDBUF_NUM - 1);

  for (i = 0; i < total_vcmd_core_num; i++) {
    sem_init(&vcmd_rdy[i], 0, 0);
  }

  for (i = 0; i < total_vcmd_core_num; i++) {
    pthread_mutex_lock(&mutex);
    if(!vcmd_thread_run[i]) {
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
      vcmd_thread_run[i] = 1;
      pthread_create(&vcmd_thread[i], NULL, vc8kd_vcmd_thread, &vcmd_index[i]);
      pthread_attr_destroy(&attr);
    }
    pthread_mutex_unlock(&mutex);
  }

  for(i = 0; i < MAX_VCMD_TYPE; i++) {
    if(vcmd_core_num[i] == 0)
      continue;
    sem_init(&vcmd_reserve_cmdbuf_sem[i], 0, 1);
  }

  /*read all registers for analyzing configuration in cwl*/
  for(i = 0; i < MAX_VCMD_TYPE; i++) {
    if(vcmd_core_num[i]==0)
      continue;
    read_main_module_all_registers(i);
  }

  return 0;
}

void CmodelVcmdRelease() {
  int i;
  
  for(i = 0; i < total_vcmd_core_num; i++) {
    release_cmdbuf_node_cleanup(&hantrovcmd_data[i].list_manager);
  }

  for(i = 0; i < total_vcmd_core_num; i++) {
    AsicHwVcmdCoreRelease(vcmd_manager[vcmd_core_array[i].sub_module_type][i]->vcmd_core);
  }

  for(i = 0; i < MAX_VCMD_TYPE; i++) {
    if(vcmd_core_num[i] == 0)
      continue;
    sem_destroy(&vcmd_reserve_cmdbuf_sem[i]);
  }


  free(vcmd_status_buf_mem_pool.virtual_address);
  free(vcmd_registers_mem_pool.virtual_address);
  free(vcmd_buf_mem_pool.virtual_address);
  free(hantrovcmd_data);

  for(i = 0; i < total_vcmd_core_num; i++) {
    if (vcmd_thread_run[i]) {
      vcmd_thread_run[i] = 0;
      /* sem_post() exit listen thread vc8kd_vcmd_thread() */
      sem_post(&vcmd_rdy[i]);
      vcmd_thread_run[i] = 0;
    }
  }

  if (fifo_cmdbuf)
    FifoRelease(fifo_cmdbuf);

  for (i = 0; i < total_vcmd_core_num; i++) {
    sem_destroy(&vcmd_rdy[i]);
  }

  for (i = 0; i < TOTAL_DISCRETE_CMDBUF_NUM; i++) {
    sem_destroy(&cmdbuf_sem[i]);
  }
  sem_destroy(&mc_sem);
  sem_destroy(&reserved_sem);
  cmdbuf1_release = 0;

  return;
}

static int hantrovcmd_isr(int irq, void *dev_id) {
  unsigned int handled = 0;
  struct hantrovcmd_dev *dev = (struct hantrovcmd_dev *) dev_id;
  u32 irq_status = 0;
  bi_list_node* new_cmdbuf_node = NULL;
  bi_list_node* base_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  size_t exe_cmdbuf_bus_address;
  u32 cmdbuf_processed_num = 0;
  u32 cmdbuf_id=0;

  /*If core is not reserved by any user, but irq is received, just clean it*/
  if (dev->list_manager.head == NULL) {
    PDEBUG("%s\n", "hantrovcmd_isr:received IRQ but core has nothing to do.\n");
    irq_status = vwl_read_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET);
    vwl_write_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET,irq_status);
    return IRQ_HANDLED;
  }

  PDEBUG("%s\n", "hantrovcmd_isr:received IRQ!\n");
  irq_status = vwl_read_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET);
#ifdef VCMD_DEBUG_INTERNAL
  {
    u32 i, fordebug;
    for(i=0;i<ASIC_VCMD_SWREG_AMOUNT;i++) {
      fordebug=vwl_read_reg ((void *)dev->hwregs, i*4);
      printf("vcmd register %d:0x%x\n",i,fordebug);
    }
  }
#endif

  if(!irq_status) {
    //printk(KERN_INFO"hantrovcmd_isr error,irq_status :0x%x",irq_status);
    return IRQ_HANDLED;
  }

  PDEBUG("irq_status of %d is:%x\n", dev->core_id,irq_status);
  vwl_write_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET,irq_status);
  dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET/4] = irq_status;

  if((dev->hw_version_id > VCMD_HW_ID_1_0_C) && (irq_status&0x3f)) {
    //if error,read from register directly.
    cmdbuf_id = vwl_get_register_value((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
    if(cmdbuf_id>=TOTAL_DISCRETE_CMDBUF_NUM) {
      printf("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
      return IRQ_HANDLED;
    }
  } else if((dev->hw_version_id > VCMD_HW_ID_1_0_C )) {
    //read cmdbuf id from ddr
#ifdef VCMD_DEBUG_INTERNAL
    {
       u32 i, fordebug;
       printf("ddr vcmd register phy_addr=0x%lx\n",dev->vcmd_reg_mem_bus_address);
       printf("ddr vcmd register virt_addr=0x%lx\n",(long unsigned int)dev->vcmd_reg_mem_virtual_address);
       for(i=0;i<ASIC_VCMD_SWREG_AMOUNT;i++) {
         fordebug=*(dev->vcmd_reg_mem_virtual_address+i);
         printf("ddr vcmd register %d:0x%x\n",i,fordebug);
       }
    }
#endif

    cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address+EXECUTING_CMDBUF_ID_ADDR);
    if(cmdbuf_id>=TOTAL_DISCRETE_CMDBUF_NUM) {
      printf("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
      return IRQ_HANDLED;
    }
  }


  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_RESET)) {
    //reset error,all cmdbuf that is not  done will be run again.
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    //find the first run_done=0
    while(1)
    {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done == 0))
        break;
      new_cmdbuf_node = new_cmdbuf_node->next;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    vcmd_link_cmdbuf(dev,base_cmdbuf_node);
    if(dev->sw_cmdbuf_rdy_num !=0) {
      //restart new command
      vcmd_start(dev,base_cmdbuf_node);
    }
    handled++;
    return IRQ_HANDLED;
  }

  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_ABORT)) {
    //abort error,don't need to reset
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    if(dev->hw_version_id > VCMD_HW_ID_1_0_C) {
      new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
      if(new_cmdbuf_node==NULL) {
        printf("hantrovcmd_isr error cmdbuf_id !!\n");
        return IRQ_HANDLED;
      }
    } else {
      exe_cmdbuf_bus_address = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
      //find the cmderror cmdbuf
      while(1) {
        if(new_cmdbuf_node==NULL) {
          return IRQ_HANDLED;
        }
        cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
        if((((cmdbuf_obj->cmdbuf_bus_address) <= exe_cmdbuf_bus_address)&&(((cmdbuf_obj->cmdbuf_bus_address+cmdbuf_obj->cmdbuf_size) >exe_cmdbuf_bus_address)) ) &&(cmdbuf_obj->cmdbuf_run_done==0))
          break;
        new_cmdbuf_node = new_cmdbuf_node->next;
      }
    }

    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1) {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done==0)) {
        cmdbuf_obj->cmdbuf_run_done=1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    base_cmdbuf_node=base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    /*if(cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    //to let high priority cmdbuf be inserted
    wake_up_interruptible_all(dev->wait_abort_queue);*/
    handled++;
    return IRQ_HANDLED;
  }

  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_BUSERR)) {
    //bus error ,don't need to reset ， where to record status?
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    if(dev->hw_version_id > VCMD_HW_ID_1_0_C) {
      new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
      if(new_cmdbuf_node==NULL) {
        printf("hantrovcmd_isr error cmdbuf_id !!\n");
        return IRQ_HANDLED;
      }
    } else {
      exe_cmdbuf_bus_address = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
      //find the buserr cmdbuf
      while(1) {
        if(new_cmdbuf_node==NULL) {
          return IRQ_HANDLED;
        }
        cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
        if((((cmdbuf_obj->cmdbuf_bus_address) <=exe_cmdbuf_bus_address)&&(((cmdbuf_obj->cmdbuf_bus_address+cmdbuf_obj->cmdbuf_size) >exe_cmdbuf_bus_address)) ) &&(cmdbuf_obj->cmdbuf_run_done==0))
          break;
        new_cmdbuf_node = new_cmdbuf_node->next;
      }
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1) {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done==0)) {
        cmdbuf_obj->cmdbuf_run_done=1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    new_cmdbuf_node = base_cmdbuf_node;
    if(new_cmdbuf_node!=NULL) {
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_BUSERR;
    }
    base_cmdbuf_node=base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    vcmd_link_cmdbuf(dev,base_cmdbuf_node);
    if(dev->sw_cmdbuf_rdy_num !=0) {
      //restart new command
      vcmd_start(dev,base_cmdbuf_node);
    }
    //if(cmdbuf_processed_num)
      //wake_up_interruptible_all(dev->wait_queue);
    handled++;
    return IRQ_HANDLED;
  }

  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_TIMEOUT)) {
    //time out,need to reset
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    if(dev->hw_version_id > VCMD_HW_ID_1_0_C ) {
      new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
      if(new_cmdbuf_node==NULL) {
        printf("hantrovcmd_isr error cmdbuf_id !!\n");
        return IRQ_HANDLED;
      }
    } else {
      exe_cmdbuf_bus_address = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
      //find the timeout cmdbuf
      while(1) {
        if(new_cmdbuf_node==NULL) {
          return IRQ_HANDLED;
        }
        cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
        if((((cmdbuf_obj->cmdbuf_bus_address) <=exe_cmdbuf_bus_address)&&(((cmdbuf_obj->cmdbuf_bus_address+cmdbuf_obj->cmdbuf_size) >exe_cmdbuf_bus_address)) ) &&(cmdbuf_obj->cmdbuf_run_done==0))
          break;
        new_cmdbuf_node = new_cmdbuf_node->next;
      }
    }
    base_cmdbuf_node = new_cmdbuf_node;
    new_cmdbuf_node = new_cmdbuf_node->previous;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1)
    {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done==0))
      {
        cmdbuf_obj->cmdbuf_run_done=1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      }
      else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    vcmd_link_cmdbuf(dev,base_cmdbuf_node);
    if(dev->sw_cmdbuf_rdy_num !=0)
    {
      //reset
      vcmd_reset_current_asic(dev);
      //restart new command
      vcmd_start(dev,base_cmdbuf_node);
    }
    //if(cmdbuf_processed_num)
      //wake_up_interruptible_all(dev->wait_queue);
    handled++;
    return IRQ_HANDLED;
  }

  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_CMDERR))
  {
    //command error,don't need to reset
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    if(dev->hw_version_id > VCMD_HW_ID_1_0_C )
    {
      new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
      if(new_cmdbuf_node==NULL)
      {
        printf("hantrovcmd_isr error cmdbuf_id !!\n");
        return IRQ_HANDLED;
      }
    }
    else
    {
      exe_cmdbuf_bus_address = VWLGetAddrRegisterValue((void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_EXECUTING_CMD_ADDR);
      //find the cmderror cmdbuf
      while(1)
      {
        if(new_cmdbuf_node==NULL)
        {
          return IRQ_HANDLED;
        }
        cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
        if((((cmdbuf_obj->cmdbuf_bus_address) <=exe_cmdbuf_bus_address)&&(((cmdbuf_obj->cmdbuf_bus_address+cmdbuf_obj->cmdbuf_size) >exe_cmdbuf_bus_address)) ) &&(cmdbuf_obj->cmdbuf_run_done==0))
          break;
        new_cmdbuf_node = new_cmdbuf_node->next;
      }
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1)
    {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done==0))
      {
        cmdbuf_obj->cmdbuf_run_done=1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      }
      else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    new_cmdbuf_node = base_cmdbuf_node;
    if(new_cmdbuf_node!=NULL)
    {
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_CMDERR;//cmderr
    }
    base_cmdbuf_node=base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    vcmd_link_cmdbuf(dev,base_cmdbuf_node);
    if(dev->sw_cmdbuf_rdy_num !=0)
    {
      //restart new command
      vcmd_start(dev,base_cmdbuf_node);
    }
    //if(cmdbuf_processed_num)
      //wake_up_interruptible_all(dev->wait_queue);
    handled++;
    return IRQ_HANDLED;
  }

  if(vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_ENDCMD))
  {
    //end command interrupt
    new_cmdbuf_node = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    if(dev->hw_version_id > VCMD_HW_ID_1_0_C )
    {
      new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
      if(new_cmdbuf_node==NULL)
      {
        printf("hantrovcmd_isr error cmdbuf_id !!\n");
        return IRQ_HANDLED;
      }
    }
    else
    {
      //find the end cmdbuf
      while(1)
      {
        if(new_cmdbuf_node==NULL)
        {
          return IRQ_HANDLED;
        }
        cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
        if((cmdbuf_obj->has_end_cmdbuf == 1)&&(cmdbuf_obj->cmdbuf_run_done==0))
          break;
        new_cmdbuf_node = new_cmdbuf_node->next;
      }
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1)
    {
      if(new_cmdbuf_node==NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done==0))
      {
        cmdbuf_obj->cmdbuf_run_done=1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      }
      else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    base_cmdbuf_node=base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev,base_cmdbuf_node);
    vcmd_link_cmdbuf(dev,base_cmdbuf_node);
    if(dev->sw_cmdbuf_rdy_num !=0)
    {
      //restart new command
      vcmd_start(dev,base_cmdbuf_node);
    }
    //if(cmdbuf_processed_num)
      //wake_up_interruptible_all(dev->wait_queue);
    handled++;
    return IRQ_HANDLED;
  }

  if(dev->hw_version_id <= VCMD_HW_ID_1_0_C )
    cmdbuf_id = vwl_get_register_mirror_value(dev->reg_mirror,HWIF_VCMD_IRQ_INTCMD);
  if(cmdbuf_id)
  {
    if(dev->hw_version_id <= VCMD_HW_ID_1_0_C )
    {
      if(cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM)
      {
        printf("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
        return IRQ_HANDLED;
      }
    }
    new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
    if(new_cmdbuf_node == NULL)
    {
      printf("hantrovcmd_isr error cmdbuf_id !!\n");
      return IRQ_HANDLED;
    }
    // interrupt cmdbuf and cmdbufs prior to itself, run_done = 1
    while(1) {
      if(new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*)new_cmdbuf_node->data;
      if((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
        sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
      } else {
        break;
      }
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    handled++;
  }
#if 0
  if(cmdbuf_processed_num)
    sem_post(&vcmd_rdy[((struct cmdbuf_obj*)(global_cmdbuf_node[cmdbuf_id]->data))->core_id]);
#endif    
    //wake_up_interruptible_all(dev->wait_queue);
  if(!handled) {
    PDEBUG("%s\n","IRQ received, but not hantro's!");
  }
  return IRQ_HANDLED;
}

static void vcmd_reset_asic(struct hantrovcmd_dev * dev)
{
    int i,n;
    u32 result;
    for (n=0;n<total_vcmd_core_num;n++)
    {
      if(dev[n].hwregs!=NULL)
      {
        //disable interrupt at first
        vwl_write_reg((void *)dev[n].hwregs,VCMD_REGISTER_INT_CTL_OFFSET,0x0000);
        //reset all
        vwl_write_reg((void *)dev[n].hwregs,VCMD_REGISTER_CONTROL_OFFSET,0x0002);
        //read status register
        result =vwl_read_reg((void *)dev[n].hwregs,VCMD_REGISTER_INT_STATUS_OFFSET);
        //clean status register
        vwl_write_reg((void *)dev[n].hwregs,VCMD_REGISTER_INT_STATUS_OFFSET,result);
        for(i = VCMD_REGISTER_CONTROL_OFFSET; i < dev[n].vcmd_core_cfg.vcmd_core_config_ptr->vcmd_iosize; i += 4)
        {
          //set all register 0
          vwl_write_reg((void *)dev[n].hwregs,i,0x0000);
        }
        //enable all interrupt
        vwl_write_reg((void *)dev[n].hwregs,VCMD_REGISTER_INT_CTL_OFFSET,0xffffffff);
      }
    }
}

static void vcmd_reset_current_asic(struct hantrovcmd_dev * dev)
{
  u32 result;

  if(dev->hwregs!=NULL)
  {
    //disable interrupt at first
    vwl_write_reg((void *)dev->hwregs,VCMD_REGISTER_INT_CTL_OFFSET,0x0000);
    //reset all
    vwl_write_reg((void *)dev->hwregs,VCMD_REGISTER_CONTROL_OFFSET,0x0002);
    //read status register
    result =vwl_read_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET);
    //clean status register
    vwl_write_reg((void *)dev->hwregs,VCMD_REGISTER_INT_STATUS_OFFSET,result);
  }

}

