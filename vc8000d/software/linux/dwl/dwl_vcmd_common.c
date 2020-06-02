#include "dwl_vcmd_common.h"

void CWLCollectWriteRegData(u32* src, u32* dst,u16 reg_start, u32 reg_length,u32* total_length)
{
  u32  i;
  u32 data_length=0;

  {
    //opcode
    *dst++=(OPCODE_WREG << 27) |(reg_length<<16)|(reg_start*4);
    data_length++;
    //data
    for(i=0;i<reg_length;i++)
    {
      *dst++ = *src++;
      data_length++;
    }
    //alignment
    if(data_length%2)
    {
      *dst = 0;
      data_length++;
    }
    *total_length = data_length;
  }
}

void CWLCollectStallData(u32* dst,u32* total_length,u32 interruput_mask)
{
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_STALL<<27)|(0<<16)|(interruput_mask);
    data_length++;
    //alignment
    *dst = 0;
    data_length++;
    *total_length = data_length;
  }
}


void CWLCollectReadRegData(u32* dst,u16 reg_start, u32 reg_length,u32* total_length, addr_t status_data_base_addr)
{
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_RREG<<27)|(reg_length<<16)|(reg_start*4);
    data_length++;
    //data
    *dst++=(u32)status_data_base_addr;
    data_length++;
    if(sizeof(addr_t) == 8) {
      *dst++=(u32)((u64)status_data_base_addr>>32);
      data_length++;
    } else {
      *dst++=0;
      data_length++;
    }

    //alignment

    *dst = 0;
    data_length++;

    *total_length = data_length;
  }
}


void CWLCollectNopData(u32* dst,u32* total_length)
{
  //u32  i;
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_NOP<<27)|(0);
    data_length++;
    //alignment
    *dst = 0;
    data_length++;
    *total_length = data_length;
  }
}

void CWLCollectIntData(u32* dst,u16 int_vecter,u32* total_length)
{
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_INT<<27)|(int_vecter);
    data_length++;
    //alignment
    *dst = 0;
    data_length++;
    *total_length = data_length;
  }
}

void CWLCollectJmpData(u32* dst,u32* total_length,u16 cmdbuf_id)
{
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_JMP_RDY0<<27)|(0);
    data_length++;
    //addr
    *dst++=(0);
    data_length++;
    *dst++=(0);
    data_length++;
    *dst++=(cmdbuf_id);
    data_length++;
    //alignment, do not do this step for driver will use this data to identify JMP and End opcode.
    *total_length = data_length;
  }
}

void CWLCollectClrIntData(u32* dst,u32 clear_type,u16 interrupt_reg_addr,u32 bitmask,u32* total_length)
{
  u32 data_length=0;
  {
    //opcode
    *dst++=(OPCODE_CLRINT<<27)|(clear_type<<25)|interrupt_reg_addr*4;
    data_length++;
    //bitmask
    *dst = bitmask;
    data_length++;
    *total_length = data_length;
  }
}

