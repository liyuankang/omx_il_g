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
--                 The entire notice above must be reproduced                                                  --
--                  on all copies and should not be removed.                                                     --
--                                                                                                                                --
--------------------------------------------------------------------------------
--
--  Abstract : VC Encoder DEC400 Interface Implementation
--
------------------------------------------------------------------------------*/

#include <string.h>
#include <unistd.h>
#include "instance.h"
#include "encdec400.h"


#ifdef SUPPORT_DEC400
void EncDec400getAlignedPicSizebyFormat(u32 type,u32 width, u32 height, u32 alignment,
                                       u32 *luma_Size,u32 *chroma_Size,u32 *picture_Size)
{
  u32 luma_stride=0, chroma_stride = 0;
  u32 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  EncGetAlignedByteStride(width,type,&luma_stride,&chroma_stride,alignment);
  switch(type)
  {
    case VCENC_YUV420_PLANAR:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
     lumaSize = luma_stride * height;
     chromaSize = 0;
     break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2*2;
     break;
    case VCENC_YUV420_PLANAR_10BIT_P010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
     lumaSize = luma_stride *10/8 * height;
     chromaSize = chroma_stride *10/8* height/2*2;
     break;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
     lumaSize = luma_stride *2*2* height/2;
     chromaSize = 0;
     break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_HEVC:
     lumaSize = luma_stride * ((height + 32 - 1) & (~(32 - 1)));
     chromaSize = lumaSize/2;
     break;
    case VCENC_YUV420_PLANAR_8BIT_DAHUA_H264:
     lumaSize = luma_stride * height * 2* 12/ 8;
     chromaSize = 0;
     break; 
    case VCENC_YUV420_SEMIPLANAR_8BIT_FB:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_FB:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_PLANAR_10BIT_P010_FB:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_SEMIPLANAR_101010:
     lumaSize = luma_stride * height;
     chromaSize = chroma_stride * height/2;
     break;
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
     lumaSize = luma_stride *  ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_32_4:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
     lumaSize = luma_stride * ((height+3)/4);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
     lumaSize = luma_stride * ((height+1)/2);
     chromaSize = chroma_stride * (((height/2)+1)/2);
     break;
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
     lumaSize = luma_stride * ((height+1)/2);
     chromaSize = chroma_stride * (((height/2)+1)/2);
     break;
    case VCENC_YUV420_8BIT_TILE_8_8:
     lumaSize = luma_stride * ((height+7)/8);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    case VCENC_YUV420_10BIT_TILE_8_8:
     lumaSize = luma_stride * ((height+7)/8);
     chromaSize = chroma_stride * (((height/2)+3)/4);
     break;
    default:
     printf("not support this format\n");
     chromaSize = lumaSize = 0;
     break;
  }
  
  pictureSize = lumaSize + chromaSize;
  if (luma_Size != NULL)
    *luma_Size = lumaSize;
  if (chroma_Size != NULL)
    *chroma_Size = chromaSize;
  if (picture_Size != NULL)
    *picture_Size = pictureSize;
  
}


void VCEncSetReadChannel(u32 ts_size_pix,u32 offset,const void *ewl)
{
  if (ts_size_pix == 256)
    EWLWriteReg(ewl, offset, 0x12030029);
  else if (ts_size_pix == 128)
    EWLWriteReg(ewl, offset, 0x14030029);
  else if (ts_size_pix == 64)
    EWLWriteReg(ewl, offset, 0x1E030029);
}

void VCEncSetReadChannel_2(u32 ts_size_pix,u32 offset,const void *ewl)
{
  if (ts_size_pix == 256)
    EWLWriteReg(ewl, offset, 0x14030031);
  else if (ts_size_pix == 128)
    EWLWriteReg(ewl, offset, 0x1E030031);
  else if (ts_size_pix == 64)
    EWLWriteReg(ewl, offset, 0x2C030031);
}

void VCEncSetReadChannel_3(u32 ts_size_pix,u32 offset,const void *ewl)
{
  if (ts_size_pix == 64)
    EWLWriteReg(ewl, offset, 0x1E030009);
  else if (ts_size_pix == 32)
    EWLWriteReg(ewl, offset, 0x2C030009);
}

void VCEncSetReadChannel_4(u32 ts_size_pix,u32 offset,const void *ewl)
{
  if (ts_size_pix == 64)
    EWLWriteReg(ewl, offset, 0x1E030079);
  else if (ts_size_pix == 32)
    EWLWriteReg(ewl, offset, 0x2C030079);
}
#endif

i32 VCEncEnableDec400(asicData_s *asic,VCDec400data *dec400_data)
{
#ifdef SUPPORT_DEC400
  u32 ts_size_pix = 0;
  u32 format = dec400_data->format;
  u32 lumWidthSrc = dec400_data->lumWidthSrc;
  u32 lumHeightSrc = dec400_data->lumHeightSrc; 
  u32 input_alignment = dec400_data->input_alignment;
  ptr_t dec400LumTableBase = dec400_data->dec400LumTableBase;
  ptr_t dec400CbTableBase = dec400_data->dec400CbTableBase;
  ptr_t dec400CrTableBase = dec400_data->dec400CrTableBase;
  const void *ewl = asic->ewl;
  i32 core_id = EWLGetDec400Coreid(ewl);
  u32 luma_Size = 0, chroma_Size = 0, U_size = 0, V_size = 0;
  u32 client_type_previous;

  if (core_id == -1)
    return VCENC_INVALID_ARGUMENT;

  EncDec400getAlignedPicSizebyFormat(format,lumWidthSrc,lumHeightSrc,input_alignment,&luma_Size,&chroma_Size,NULL);

  switch(format)
  {
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
      ts_size_pix = 256;
      break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
    case VCENC_YUV420_PLANAR_10BIT_P010:
      ts_size_pix = 128;
      break;
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
      ts_size_pix = 64;
      break;
    default:
      return VCENC_INVALID_ARGUMENT;
  }

  client_type_previous = EWLChangeClientType(ewl, EWL_CLIENT_TYPE_DEC400);
  
  EWLWriteReg(ewl, gcregAHBDECControl, 0x00000000);
  EWLWriteReg(ewl, gcregAHBDECControlEx, 0x00000000);

  switch(format)
  {
    //V component
    case VCENC_YUV420_PLANAR:
    case VCENC_YUV420_PLANAR_10BIT_I010:
      V_size = chroma_Size/2;
      
      if (format == VCENC_YUV420_PLANAR)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig2, 0x00000000);
      }
      else
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig2, 0x00030000);
      }
      
      VCEncSetReadChannel(ts_size_pix,gcregAHBDECReadConfig2,ewl);
      
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferBase2, asic->regs.inputCrBase);
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferEnd2, asic->regs.inputCrBase+V_size-1);
      
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadCacheBase2, dec400CrTableBase);
      
    //U component
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV420_PLANAR_10BIT_P010:
      if (format == VCENC_YUV420_PLANAR)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig1, 0x00000000);
        VCEncSetReadChannel(ts_size_pix,gcregAHBDECReadConfig1,ewl);
        U_size = chroma_Size/2;
      }
      else if (format == VCENC_YUV420_PLANAR_10BIT_I010)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig1, 0x00030000);
        VCEncSetReadChannel(ts_size_pix,gcregAHBDECReadConfig1,ewl);
        U_size = chroma_Size/2;
      }
      else if (format == VCENC_YUV420_PLANAR_10BIT_P010)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig1, 0x00010000);
        VCEncSetReadChannel_2(ts_size_pix,gcregAHBDECReadConfig1,ewl);
        U_size = chroma_Size;
      }
      else//NV12,NV21
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig1, 0x00000000);
        VCEncSetReadChannel_2(ts_size_pix,gcregAHBDECReadConfig1,ewl);
        U_size = chroma_Size;
      }
    
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferBase1, asic->regs.inputCbBase);
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferEnd1, asic->regs.inputCbBase+U_size-1);

      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadCacheBase1, dec400CbTableBase);
      
    //Y component
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
      if (format == VCENC_YUV420_PLANAR)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig0, 0x00000000);
      }
      else if (format == VCENC_YUV420_PLANAR_10BIT_I010)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig0, 0x00030000);
      }
      else if (format == VCENC_YUV420_PLANAR_10BIT_P010)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig0, 0x00010000);
      }
      else if (format == VCENC_YUV420_SEMIPLANAR || format == VCENC_YUV420_SEMIPLANAR_VU)
      {
        EWLWriteReg(ewl, gcregAHBDECReadExConfig0, 0x00000000);
      }
      
      if (format == VCENC_RGB888 || format == VCENC_BGR888)
        VCEncSetReadChannel_3(ts_size_pix,gcregAHBDECReadConfig0,ewl);
      else if (format == VCENC_RGB101010 || format == VCENC_BGR101010)
        VCEncSetReadChannel_4(ts_size_pix,gcregAHBDECReadConfig0,ewl);
      else
        VCEncSetReadChannel(ts_size_pix,gcregAHBDECReadConfig0,ewl);
      
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferBase0, asic->regs.inputLumBase);
      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadBufferEnd0, asic->regs.inputLumBase+luma_Size-1);

      EncDec400SetAddrRegisterValue(ewl, gcregAHBDECReadCacheBase0, dec400LumTableBase);
      break;
    default:
      EWLChangeClientType(ewl, client_type_previous);
      return VCENC_INVALID_ARGUMENT;
  }
  
  EWLChangeClientType(ewl, client_type_previous);
#endif
  return VCENC_OK;

}

void VCEncDisableDec400(const void *ewl)
{
#ifdef SUPPORT_DEC400
    u32 client_type_previous;
    i32 core_id = EWLGetDec400Coreid(ewl);
    if (core_id == -1)
      return;
    
    client_type_previous = EWLChangeClientType(ewl, EWL_CLIENT_TYPE_DEC400);

    //do SW reset after one frame and wait it done by HW if needed
    EWLWriteReg(ewl, gcregAHBDECControl, 0x00000010);
    EWLChangeClientType(ewl, client_type_previous);
#ifdef PC_PCI_FPGA_DEMO
    usleep(80000);
#endif
#endif
}

void VCEncSetDec400StreamBypass(const void *ewl)
{
#ifdef SUPPORT_DEC400
    u32 client_type_previous;
    i32 core_id = EWLGetDec400Coreid(ewl);
    if (core_id == -1)
      return;
    
    client_type_previous = EWLChangeClientType(ewl, EWL_CLIENT_TYPE_DEC400);

    //do SW reset after one frame and wait it done by HW if needed
    EWLWriteReg(ewl, gcregAHBDECControl, 0x00000010);
#ifdef PC_PCI_FPGA_DEMO
    usleep(80000);
#endif
    //disable global bypass to enable stream bypass
    EWLWriteReg(ewl, gcregAHBDECControl, 0x02010088);

    EWLChangeClientType(ewl, client_type_previous);
#endif
}


