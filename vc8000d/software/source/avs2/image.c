/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2002-2016, Audio Video coding Standard Workgroup of China
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of Audio Video coding Standard Workgroup of China
*    nor the names of its contributors maybe used to endorse or promote products
*    derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
*************************************************************************************
* File name: image.c
* Function: Decode a Slice
*
*************************************************************************************
*/

#include "contributors.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "header.h"
#include "annexb.h"
//#include "AEC.h"
//#include "loop-filter.h"
//#include "inter-prediction.h"
#include "md5.h"
//#include "codingUnit.h"
//#include "g2hw.h"
//#include "ctbctrl.h"
//#include "filterd.h"

//extern struct G2Hw *g2hw;

void Copy_frame_for_ALF();
#if ROI_M3264
//#include "pos_info.h"
#endif

// Adaptive frequency weighting quantization
#if FREQUENCY_WEIGHTING_QUANTIZATION
#include "wquant.h"
#endif

#include "avs2hwd_api.h"
#include "avs2hwd_asic.h"
//#include "regif.h"

void DecideMvRange();  // rm52k

/* 08.16.2007--for user data after pic header */
unsigned char *temp_slice_buf;
int first_slice_length;
int first_slice_startpos;
/* 08.16.2007--for user data after pic header */

extern StatBits *StatBitsPtr;

extern unsigned int MD5val[4];
extern char MD5str[33];

static int alfParAllcoated = 0;

#if SEQ_CHANGE_CHECKER
byte *seq_checker_buf = NULL;
int seq_checker_length = 0;
#endif

#if RD170_FIX_BG
void delete_trbuffer(outdata *data, int pos);
void flushDPB(void) {
  int j, tmp_min, i, pos = -1;
  int search_times = outprint.buffer_num;

  tmp_min = 1 << 20;
  i = 0, j = 0;
  pos = -1;

  for (j = 0; j < search_times; j++) {
    pos = -1;
    tmp_min = (1 << 20);
    // search for min poi picture to display
    for (i = 0; i < outprint.buffer_num; i++) {
      if (outprint.stdoutdata[i].tr < tmp_min) {
        pos = i;
        tmp_min = outprint.stdoutdata[i].tr;
      }
    }

    if (pos != -1) {
      hd->last_output = outprint.stdoutdata[pos].tr;
      report_frame(outprint, pos);
      if (outprint.stdoutdata[pos].typeb == BACKGROUND_IMG &&
          outprint.stdoutdata[pos].background_picture_output_flag == 0) {
        write_GB_frame(hd->p_out_background);
      } else {
        write_frame(hd->p_out, outprint.stdoutdata[pos].tr);
      }

      delete_trbuffer(&outprint, pos);
    }
  }

  // clear dpb info
  for (j = 0; j < REF_MAXBUFFER; j++) {
    fref[j]->img_poi = -256;
    fref[j]->img_coi = -257;
    fref[j]->temporal_id = -1;
    fref[j]->refered_by_others = 0;
  }
}
#endif

#if M3480_TEMPORAL_SCALABLE
void cleanRefMVBufRef(int pos) {
  int k, x, y;
#if 0
  // re-init mvbuf
  for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
    for (x = 0; x < img->width / MIN_BLOCK_SIZE; x++) {
      fref[pos]->mvbuf[y * (img->width / MIN_BLOCK_SIZE) + x].hor = 0;
      fref[pos]->mvbuf[y * (img->width / MIN_BLOCK_SIZE) + x].ver = 0;
      fref[pos]->mvbuf[y * (img->width / MIN_BLOCK_SIZE) + x].ref_idx = -1;
    }
  }
#endif
#if 0
  //re-init refbuf
  for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
    for (x = 0; x < img->width / MIN_BLOCK_SIZE ; x++) {
      fref[pos]->refbuf[y][x] = -1;
    }
  }
#endif
}
#endif

// extern gb_frame_num;
extern struct Avs2PicParam pic_data;

/*
 *  Function: get reference list string.
 *     Input:
 *    Output: string in "[%d %d %d %d]" format
 *    Return:
 * Attention:
 *    Author: Falei LUO, 2016.01.30
 */
void get_reference_list_info(char *str) {
  char str_tmp[16];
  int i;
  // int poc = hc->f_rec->img_poi;  //fred.chiu@mediatek.com

  if (img->num_of_references > 0) {
    strcpy(str, "[");
    for (i = 0; i < img->num_of_references; i++) {
#if RD1510_FIX_BG
      if (img->type == B_IMG) {
        sprintf(str_tmp, "%4d    ",
                hc->f_rec->ref_poc[img->num_of_references - 1 - i]);
      } else {
        sprintf(str_tmp, "%4d    ", hc->f_rec->ref_poc[i]);
      }
#else
      sprintf(str_tmp, "%4d     ", fref[i]->img_poi);
#endif

      str_tmp[5] = '\0';
      strcat(str, str_tmp);
    }
    strcat(str, "]");
  } else {
    str[0] = '\0';
  }
}

int decode_one_frame(SNRParameters *snr) {
  int current_header;
  int N8_SizeScale;
  time_t ltime1;  // for time measurement
  time_t ltime2;

#ifdef WIN32
  struct _timeb tstruct1;
  struct _timeb tstruct2;
#else
  struct timeb tstruct1;
  struct timeb tstruct2;
#endif

  double framerate[8] = {24000.0 / 1001.0, 24.0, 25.0,
                         30000.0 / 1001.0, 30.0, 50.0,
                         60000.0 / 1001.0, 60.0};

#ifdef WIN32
  _ftime(&tstruct1);  // start time ms
#else
  ftime(&tstruct1);  // start time ms
#endif
  time(&ltime1);  // start time s

  // gb_frame_num++;
  current_header = Header();
  printf("Stream Information\n");
  printf("  Profile:%d, Level:%d\n", input->profile_id, input->level_id);
  printf("  Resolution: %dx%d\n", hd->horizontal_size, hd->vertical_size);

  DecideMvRange();  // rm52k

  if (current_header == EOS) {
    return EOS;
  }

  {
    N8_SizeScale = 1;

    if (hd->horizontal_size % (MIN_CU_SIZE * N8_SizeScale) != 0) {
      img->auto_crop_right =
          (MIN_CU_SIZE * N8_SizeScale) -
          (hd->horizontal_size % (MIN_CU_SIZE * N8_SizeScale));
    } else {
      img->auto_crop_right = 0;
    }
#if !INTERLACE_CODING
    if (hd->progressive_sequence)
#endif
    {
      if (hd->vertical_size % (MIN_CU_SIZE * N8_SizeScale) != 0) {
        img->auto_crop_bottom =
            (MIN_CU_SIZE * N8_SizeScale) -
            (hd->vertical_size % (MIN_CU_SIZE * N8_SizeScale));
      } else {
        img->auto_crop_bottom = 0;
      }
    }

    // Reinit parameters (NOTE: need to do before init_frame //
    img->width = (hd->horizontal_size + img->auto_crop_right);
    img->height = (hd->vertical_size + img->auto_crop_bottom);
    img->width_cr = (img->width >> 1);

    if (input->chroma_format == 1) {
      img->height_cr = (img->height >> 1);
    }

    img->PicWidthInMbs = img->width / MIN_CU_SIZE;
    img->PicHeightInMbs = img->height / MIN_CU_SIZE;
    img->PicSizeInMbs = img->PicWidthInMbs * img->PicHeightInMbs;
    img->max_mb_nr = (img->width * img->height) / (MIN_CU_SIZE * MIN_CU_SIZE);
  }

  if (img->new_sequence_flag && img->sequence_end_flag) {
#if 0  // RD170_FIX_BG //
    int k;
    flushDPB();
        for (k = 0; k < REF_MAXBUFFER; k++) {
        cleanRefMVBufRef(k);
         }
    free_global_buffers();
    img->number=0;
#endif

    hd->end_SeqTr = img->tr;
    img->sequence_end_flag = 0;
  }
  if (img->new_sequence_flag) {
    hd->next_IDRtr = img->tr;
    hd->next_IDRcoi = img->coding_order;
    img->new_sequence_flag = 0;
  }

#if 0  // RD170_FIX_BG
  if (hd->vec_flag) {
    int k;
    flushDPB();
    for (k = 0; k < REF_MAXBUFFER; k++) {
      cleanRefMVBufRef(k);
    }
    hd->vec_flag = 0;
    free_global_buffers();
    img->number=0;
  }
#endif

  // allocate memory for frame buffers

  if (img->number == 0) {
    init_global_buffers();
  }

  init_frame();

  img->types = img->type;  // jlzheng 7.15

  if (img->type != B_IMG) {
    hd->pre_img_type = img->type;
    hd->pre_img_types = img->types;
  }

  u8 *background_tbl_address;
  background_tbl_address = hc->f_rec->tbl_buf_y;  // used for background
  {
    struct Avs2RefsParam *refs = &avs2dec_cont->storage.refs;
    int i, j;
    for (i = 0; i < MAXREF; i++) {
      refs->ref[i].y = fref[i]->ref[0];
      refs->ref[i].y_tbl.bus_address = fref[i]->tbl_buf_y;
      refs->ref[i].c = fref[i]->ref[1];
      refs->ref[i].c_tbl.bus_address = fref[i]->tbl_buf_c;
      refs->ref[i].img_poi = fref[i]->img_poi;
      for (j = 0; j < MAXREF; j++) {
        refs->ref[i].ref_poc[j] = fref[i]->ref_poc[j];
      }
    }
    refs->ref[0].mv.bus_address = (addr_t)fref[0]->mvbuf;

    refs->background.y = hd->background_frame[0];
    refs->background.c = hd->background_frame[1];
    refs->background.y_tbl.bus_address = (size_t)hd->background_tbl_y;
    refs->background.c_tbl.bus_address = (size_t)hd->background_tbl_c;
    Avs2HwdSetParams(&avs2dec_cont->hwdec, ATTRIB_REFS, refs);
  }

  {
    struct Avs2StreamParam *stream = &avs2dec_cont->storage.input;

    memcpy(currStream->strm_buff_start, temp_slice_buf, first_slice_length);
    free(temp_slice_buf);
    currStream->strm_buff_size = currStream->strm_data_size =
        first_slice_length;
    currStream->strm_buff_read_bits = (first_slice_startpos) * 8;
    currStream->bit_pos_in_word = currStream->strm_buff_read_bits % 8;
    currStream->strm_curr_pos =
        currStream->strm_buff_start + first_slice_startpos;
    currStream->remove_emul3_byte = 1;
    currStream->remove_avs_fake_2bits = 1;
    currStream->emul_byte_count = 0;
    currStream->is_rb = 0;

    stream->is_rb = 0;
    stream->stream_bus_addr = (addr_t)currStream->strm_buff_start;
    stream->stream_length = first_slice_length;
    stream->stream_offset = first_slice_startpos;
    memset(&stream->ring_buffer, 0, sizeof(struct DWLLinearMem));
    Avs2HwdSetParams(&avs2dec_cont->hwdec, ATTRIB_STREAM, stream);
  }

  RunAsic(NULL);

// printf("image decoding finish. total decoded frames=%d\n",gb_frame_num);
// tile map
#ifdef TILEOUTPUT
  if (img->typeb == BACKGROUND_IMG && hd->background_picture_enable) {
    int l;
    int row_bytes, tbl_bytes;
    u8 *bgfy = (u8 *)hd->background_frame[0].virtual_address;
    u8 *bgfuv = (u8 *)hd->background_frame[1].virtual_address;
    u8 *imgY = (u8 *)hc->imgY->virtual_address;
    u8 *imgUV = (u8 *)hc->imgUV[0]->virtual_address;

    row_bytes = (img->width * input->sample_bit_depth) / 8;

    for (l = 0; l < img->height; l++) {
      memcpy(bgfy + l * row_bytes, imgY + l * row_bytes, row_bytes);
    }
    for (l = 0; l < img->height_cr; l++) {
      memcpy(bgfuv + l * row_bytes, imgUV + l * row_bytes, row_bytes);
    }

    // copy tbl table to background
    tbl_bytes = (img->height / 4) * (img->width / 4);
    memcpy(hd->background_tbl_y, background_tbl_address, tbl_bytes);

    {
      struct Avs2RefsParam *refs = &avs2dec_cont->storage.refs;
      refs->background.y = hd->background_frame[0];
      refs->background.c = hd->background_frame[1];
      refs->background.y_tbl.bus_address = (size_t)hd->background_tbl_y;
      refs->background.c_tbl.bus_address = (size_t)hd->background_tbl_c;
    }
  }
#endif

  if (img->typeb == BACKGROUND_IMG && hd->background_picture_output_flag == 0) {
    hd->background_number++;
  }
#if 0
    if (img->type == B_IMG) {
        fref[0]->img_poi = hd->trtmp;   //restore fref[0] distance (POI)
    }
#endif
#if 0
    img->height = (hd->vertical_size + img->auto_crop_bottom);
    img->height_cr =  img->height / (input->chroma_format == 1 ? 2 : 1);
    img->PicWidthInMbs  = img->width / MIN_CU_SIZE;
    img->PicHeightInMbs = img->height / MIN_CU_SIZE;
    img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
#endif

#ifdef WIN32
  _ftime(&tstruct2);  // end time ms
#else
  ftime(&tstruct2);  // end time ms
#endif

  time(&ltime2);  // end time sec
  hd->tmp_time = (int)((ltime2 * 1000 + tstruct2.millitm) -
                       (ltime1 * 1000 + tstruct1.millitm));
  hc->tot_time = hc->tot_time + hd->tmp_time;

  // rm52k_r2
  StatBitsPtr->curr_frame_bits = StatBitsPtr->curr_frame_bits * 8 +
                                 StatBitsPtr->emulate_bits -
                                 StatBitsPtr->last_unit_bits;
  StatBitsPtr->bitrate += StatBitsPtr->curr_frame_bits;
  StatBitsPtr->coded_pic_num++;

  if ((int)(StatBitsPtr->coded_pic_num -
            (StatBitsPtr->time_s + 1) * framerate[hd->frame_rate_code - 1] +
            0.5) == 0) {
    StatBitsPtr->total_bitrate[StatBitsPtr->time_s++] = StatBitsPtr->bitrate;
    StatBitsPtr->bitrate = 0;
  }

  // record the reference list information
  get_reference_list_info(hc->str_list_reference);

  frame_postprocessing();

  StatBitsPtr->curr_frame_bits = 0;
  StatBitsPtr->emulate_bits = 0;
  StatBitsPtr->last_unit_bits = 0;

#if FIX_PROFILE_LEVEL_DPB_RPS_1
  //////////////////////////////////////////////////////////////////////////
  // delete the frame that will never be used
  {
    int i, j;
    for (i = 0; i < hd->curr_RPS.num_to_remove; i++) {
      for (j = 0; j < REF_MAXBUFFER; j++) {

        if (fref[j]->img_coi >= -256 &&
            fref[j]->img_coi ==
                img->coding_order - hd->curr_RPS.remove_pic[i]) {
          break;
        }
      }
      if (j < REF_MAXBUFFER) {
#if FIX_RPS_PICTURE_REMOVE
        /* Label new frames as "un-referenced" */
        fref[j]->refered_by_others = 0;

        /* remove frames which have been outputted */
        if (fref[j]->is_output == -1) {
          fref[j]->img_poi = -256;
          fref[j]->img_coi = -257;
          fref[j]->temporal_id = -1;
        }
#else
        fref[j]->img_coi = -257;
#if M3480_TEMPORAL_SCALABLE
        fref[j]->temporal_id = -1;
#endif
        if (fref[j]->is_output == -1) {
          fref[j]->img_poi = -256;
        }
#endif
      }
    }
  }
#endif

  //! TO 19.11.2001 Known Problem: for init_frame we have to know the picture
  //type of the actual frame
  //! in case the first slice of the P-Frame following the I-Frame was lost we
  //decode this P-Frame but//! do not write it because it was assumed to be an
  //I-Frame in init_frame.So we force the decoder to
  //! guess the right picture type. This is a hack a should be removed by the
  //time there is a clean
  //! solution where we do not have to know the picture type for the function
  //init_frame.
  //! End TO 19.11.2001//Lou

  {
    if (img->type == I_IMG || img->type == P_IMG || img->type == F_IMG) {
      img->number++;
    } else {
      hc->Bframe_ctr++;  // B pictures
    }
  }

  return (SOP);
}

/*
*************************************************************************
* Function:
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void report_frame(outdata data, int pos) {
#if WRITE_LOGS
  FILE *file;
#endif
  char *Frmfld;
  char Frm[] = "FRM";
  char Fld[] = "FLD";
  STDOUT_DATA *p_stdoutdata = &data.stdoutdata[pos];
  const char *typ;
#if WRITE_LOGS
  file = fopen("stat.dat", "at");
#endif
  if (input->MD5Enable & 0x02) {
    sprintf(MD5str, "%08X%08X%08X%08X\0", p_stdoutdata->DecMD5Value[0],
            p_stdoutdata->DecMD5Value[1], p_stdoutdata->DecMD5Value[2],
            p_stdoutdata->DecMD5Value[3]);
  } else {
    memset(MD5val, 0, 16);
    memset(MD5str, 0, 33);
  }

  if (p_stdoutdata->picture_structure) {
    Frmfld = Frm;
  } else {
    Frmfld = Fld;
  }
#if INTERLACE_CODING
  if (img->is_field_sequence) {  // rcs??
    Frmfld = Fld;
  }
#endif
  if ((p_stdoutdata->tr + hc->total_frames * 256) ==
      hd->end_SeqTr) {  // I picture
    // if ( img->new_sequence_flag == 1 )
    {
      img->sequence_end_flag = 0;
      fprintf(stdout, "Sequence End\n\n");
    }
  }
  if ((p_stdoutdata->tr + hc->total_frames * 256) == hd->next_IDRtr) {
#if !RD170_FIX_BG
    if (hd->vec_flag)
#endif
    {
      hd->vec_flag = 0;
      fprintf(stdout, "Video Edit Code\n");
    }
  }

  if (p_stdoutdata->typeb == BACKGROUND_IMG) {
    typ = (hd->background_picture_output_flag != 0) ? "G" : "GB";
  } else {
#if REMOVE_UNUSED
    typ = (p_stdoutdata->type == INTRA_IMG)
              ? "I"
              : (p_stdoutdata->type == P_IMG)
                    ? ((p_stdoutdata->typeb == BP_IMG) ? "S" : "P")
                    : (p_stdoutdata->type == F_IMG ? "F" : "B");
#else
    typ = (p_stdoutdata->type == INTRA_IMG)
              ? "I"
              : (p_stdoutdata->type == P_IMG)
                    ? ((p_stdoutdata->type == BP_IMG) ? "S" : "P")
                    : (p_stdoutdata->type == F_IMG ? "F" : "B");
#endif
  }

#if WRITE_LOGS
  fprintf(file, "%3d(%s)  %3d %5d %7.4f %7.4f %7.4f %5d\t\t%s %8d %6d\t%s",
          p_stdoutdata->framenum + hc->total_frames * 256, typ,
          p_stdoutdata->tr + hc->total_frames * 256, p_stdoutdata->qp,
          p_stdoutdata->snr_y, p_stdoutdata->snr_u, p_stdoutdata->snr_v,
          p_stdoutdata->tmp_time, Frmfld, p_stdoutdata->curr_frame_bits,
          p_stdoutdata->emulate_bits, MD5str);
  fprintf(file, " %s\n", p_stdoutdata->str_reference_list);
  fclose(file);
#endif
  printf("%3d(%s)  %3d %5d %7.4f %7.4f %7.4f %5d\t\t%s %8d %6d\t%s",
         p_stdoutdata->framenum + hc->total_frames * 256, typ,
         p_stdoutdata->tr + hc->total_frames * 256, p_stdoutdata->qp,
         p_stdoutdata->snr_y, p_stdoutdata->snr_u, p_stdoutdata->snr_v,
         p_stdoutdata->tmp_time, Frmfld, p_stdoutdata->curr_frame_bits,
         p_stdoutdata->emulate_bits, MD5str);
  printf(" %s\n", p_stdoutdata->str_reference_list);

  fflush(stdout);
  hd->FrameNum++;
}

/*
*************************************************************************
* Function:Find PSNR for all three components.Compare decoded frame with
the original sequence. Read input->jumpd frames to reflect frame skipping.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void find_snr(SNRParameters *snr,
              FILE *p_ref)  //!< filestream to reference YUV file
{
  int i, j;
  long long int diff_y, diff_u, diff_v;
  int uv;
  int uvformat = input->chroma_format == 1 ? 4 : 2;

  int nOutputSampleSize = (input->output_bit_depth > 8) ? 2 : 1;
  unsigned char chTemp[2];
  int nBitDepthDiff = input->sample_bit_depth -
                      input->output_bit_depth;  // assume coding bit depth no
                                                // less than output bit depth

  long long int status;

  int img_width = (img->width - img->auto_crop_right);
  int img_height = (img->height - img->auto_crop_bottom);
  int img_width_cr = (img_width / 2);
  int img_height_cr = (img_height / (input->chroma_format == 1 ? 2 : 1));
  const unsigned int bytes_y = img_width * img_height;
  const unsigned int bytes_uv = img_width_cr * img_height_cr;
  const long long int framesize_in_bytes =
      (long long int)(bytes_y + 2 * bytes_uv);
  byte *imgYRef, *imgUVRef[2];
  unsigned char *buf;
  int offset_units;
  byte *imgY;
  byte *imgUV[2];
  imgY = (byte *)hc->imgY->virtual_address;
  imgUV[0] = (byte *)hc->imgUV[0]->virtual_address;
  imgUV[1] = (byte *)hc->imgUV[1]->virtual_address;

  imgYRef = (byte *)hd->imgYRef.virtual_address;
  imgUVRef[0] = (byte *)hd->imgUVRef[0].virtual_address;
  imgUVRef[1] = (byte *)hd->imgUVRef[1].virtual_address;
  if (!input->yuv_structure) {
    buf = malloc(nOutputSampleSize * bytes_y);
  } else {
    buf = malloc(nOutputSampleSize * (bytes_y + 2 * bytes_uv));
  }

  if (NULL == buf) {
    no_mem_exit("find_snr: buf");
  }

  snr->snr_y = 0.0;
  snr->snr_u = 0.0;
  snr->snr_v = 0.0;

  fseek(p_ref, 0, SEEK_SET);

  if (hd->RefPicExist) {
    if (!input->ref_pic_order) {  // ref order
      status = fseek(
          p_ref,
          (long long int)(nOutputSampleSize * framesize_in_bytes *
                          (img->tr +
                           hc->total_frames *
                               256) /*( img->type == B_IMG ? img->tr : FrameNum )*/),
          0);
    } else {
      status =
          fseek(p_ref, (long long int)(nOutputSampleSize * framesize_in_bytes *
                                       hd->dec_ref_num),
                0);
    }

    if (status != 0) {
      snprintf(hc->errortext, ET_SIZE, "Error in seeking img->tr: %d", img->tr);
      hd->RefPicExist = 0;
    }
  }

  if (!input->yuv_structure) {
    fread(buf, sizeof(unsigned char), nOutputSampleSize * bytes_y, p_ref);
  } else {
    fread(buf, sizeof(unsigned char),
          nOutputSampleSize * (bytes_y + 2 * bytes_uv), p_ref);
  }

  if (!input->yuv_structure) {  // YUV
    for (j = 0; j < img_height; j++) {
      if (input->output_bit_depth == 8) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = buf[j * img_width + i];
          imgYRef[j * img_width + i] &= 0xff;  // reset high 8 bits
        }
      } else if (input->output_bit_depth == 10) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = ((byte *)buf)[j * img_width + i] & 0x3ff;
        }
      } else if (input->output_bit_depth == 12) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = ((byte *)buf)[j * img_width + i] & 0xfff;
        }
      }
    }
  }
// TODO: Not support UYVY for G2
#if 0
    else { //U0Y0 V1Y1
        for (j = 0; j < img_height; j++) {
            for (i = 0; i < img_width; i++) {
                if (input->output_bit_depth == 8) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = buf[2 * offset_units + 1];
                    hd->imgYRef[j][i] &= 0xff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = buf[2 * offset_units];
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = buf[2 * offset_units];
                    }
                    hd->imgYRef[j][i] = fgetc(p_ref);
                } else if (input->output_bit_depth == 10) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = ((byte *)buf)[2 * offset_units + 1];
                    hd->imgYRef[j][i] = hd->imgYRef[j][i] & 0x3ff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[0][j][i / 2] &= 0x3ff;
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[1][j][i / 2] &= 0x3ff;
                    }
                    chTemp[0] = fgetc(p_ref);
                    chTemp[1] = fgetc(p_ref);
                    hd->imgYRef[j][i] = ((byte *)(&(chTemp[0])))[0] & 0x3ff;
                } else if (input->output_bit_depth == 12) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = ((byte *)buf)[2 * offset_units + 1];
                    hd->imgYRef[j][i] = hd->imgYRef[j][i] & 0xfff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[0][j][i / 2] &= 0xfff;
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[1][j][i / 2] &= 0xfff;
                    }
                    chTemp[0] = fgetc(p_ref);
                    chTemp[1] = fgetc(p_ref);
                    hd->imgYRef[j][i] = ((byte *)(&(chTemp[0])))[0] & 0xfff;
                }

            }
        }
    }
#endif

  for (uv = 0; uv < 2; uv++) {
    if (!input->yuv_structure) {
      fread(buf, sizeof(unsigned char),
            nOutputSampleSize * img_height_cr * img_width_cr, p_ref);

      for (j = 0; j < img_height_cr; j++) {
        for (i = 0; i < img_width_cr; i++) {
          if (input->output_bit_depth == 8) {
            imgUVRef[uv][j * img_width_cr + i] = buf[j * img_width_cr + i];
            imgUVRef[uv][j * img_width_cr + i] &= 0xff;
          } else if (input->output_bit_depth == 10) {
            imgUVRef[uv][j * img_width_cr + i] =
                ((byte *)buf)[j * img_width_cr + i] & 0x3ff;
          } else if (input->output_bit_depth == 12) {
            imgUVRef[uv][j * img_width_cr + i] =
                ((byte *)buf)[j * img_width_cr + i] & 0xfff;
          }
        }
      }
    }
  }

  img->quad[0] = 0;
  diff_y = 0;

  for (j = 0; j < img_height; ++j) {
    for (i = 0; i < img_width; ++i) {
      if (nBitDepthDiff == 0) {
        diff_y +=
            img->quad
                [abs(imgY[j * img_width + i] - imgYRef[j * img_width + i])];
      } else if (nBitDepthDiff > 0) {
        diff_y += img->quad[abs(
            Clip1((imgY[j * img_width + i] + (1 << (nBitDepthDiff - 1))) >>
                  nBitDepthDiff) -
            imgYRef[j * img_width + i])];
      }
    }
  }

  // Chroma
  diff_u = 0;
  diff_v = 0;

  for (j = 0; j < img_height_cr; ++j) {
    for (i = 0; i < img_width_cr; ++i) {
      if (nBitDepthDiff == 0) {
        diff_u += img->quad[abs(imgUVRef[0][j * img_width_cr + i] -
                                imgUV[0][j * img_width_cr + i])];
        diff_v += img->quad[abs(imgUVRef[1][j * img_width_cr + i] -
                                imgUV[1][j * img_width_cr + i])];
      } else if (nBitDepthDiff > 0) {
        diff_u += img->quad[abs(imgUVRef[0][j * img_width_cr + i] -
                                Clip1((imgUV[0][j * img_width_cr + i] +
                                       (1 << (nBitDepthDiff - 1))) >>
                                      nBitDepthDiff))];
        diff_v += img->quad[abs(imgUVRef[1][j * img_width_cr + i] -
                                Clip1((imgUV[1][j * img_width_cr + i] +
                                       (1 << (nBitDepthDiff - 1))) >>
                                      nBitDepthDiff))];
      }
    }
  }

  // Collecting SNR statistics
  if (diff_y != 0) {
    snr->snr_y =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)diff_y));  // luma snr for current frame
  }

  if (diff_u != 0) {
    snr->snr_u =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_u)));  //  chroma snr
                                                                // for current
                                                                // frame,422
  }

  if (diff_v != 0) {
    snr->snr_v =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_v)));  //  chroma snr
                                                                // for current
                                                                // frame,422
  }

  if (img->number == 0) {  // first
    snr->snr_y1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)diff_y));  // keep luma snr for first frame
    snr->snr_u1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_u)));  // keep chroma
                                                                // snr for first
                                                                // frame,422
    snr->snr_v1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_v)));  // keep chroma
                                                                // snr for first
                                                                // frame,422
    snr->snr_ya = snr->snr_y1;
    snr->snr_ua = snr->snr_u1;
    snr->snr_va = snr->snr_v1;

    if (diff_y == 0) {
      snr->snr_ya = /*50*/ 0;
    }

    if (diff_u == 0) {
      snr->snr_ua = /*50*/ 0;
    }

    if (diff_v == 0) {
      snr->snr_va = /*50*/ 0;
    }
  } else {
    snr->snr_ya =
        (snr->snr_ya * (double)(img->number + hc->Bframe_ctr) + snr->snr_y) /
        (double)(img->number + hc->Bframe_ctr +
                 1);  // average snr lume for all frames inc. first
    snr->snr_ua =
        (snr->snr_ua * (double)(img->number + hc->Bframe_ctr) + snr->snr_u) /
        (double)(img->number + hc->Bframe_ctr +
                 1);  // average snr u croma for all frames inc. first
    snr->snr_va =
        (snr->snr_va * (double)(img->number + hc->Bframe_ctr) + snr->snr_v) /
        (double)(img->number + hc->Bframe_ctr +
                 1);  // average snr v croma for all frames inc. first
  }

  free(buf);
}

/*
*************************************************************************
* Function:Find PSNR for BACKGROUND PICTURE.Compare decoded frame with
the original sequence. Read input->jumpd frames to reflect frame skipping.
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void find_snr_background(SNRParameters *snr,
                         FILE *p_ref)  //!< filestream to reference YUV file
{
  int i, j;
  long long int diff_y, diff_u, diff_v;
  int uv;
  int uvformat = input->chroma_format == 1 ? 4 : 2;

  int nOutputSampleSize = (input->output_bit_depth > 8) ? 2 : 1;
  int nBitDepthDiff = input->sample_bit_depth -
                      input->output_bit_depth;  // assume coding bit depth no
                                                // less than output bit depth

  long long int status;

  int img_width = (img->width - img->auto_crop_right);
  int img_height = (img->height - img->auto_crop_bottom);
  int img_width_cr = (img_width / 2);
  int img_height_cr = (img_height / (input->chroma_format == 1 ? 2 : 1));
  const unsigned int bytes_y = img_width * img_height;
  const unsigned int bytes_uv = img_width_cr * img_height_cr;
  const long long int framesize_in_bytes =
      (long long int)(bytes_y + 2 * bytes_uv);

  unsigned char *buf;

  int offset_units;
  byte *imgYRef, *imgUVRef[2];
  byte *imgY;
  byte *imgUV[2];
  imgY = (byte *)hc->imgY->virtual_address;
  imgUV[0] = (byte *)hc->imgUV[0]->virtual_address;
  imgUV[1] = (byte *)hc->imgUV[1]->virtual_address;

  imgYRef = (byte *)hd->imgYRef.virtual_address;
  imgUVRef[0] = (byte *)hd->imgUVRef[0].virtual_address;
  imgUVRef[1] = (byte *)hd->imgUVRef[1].virtual_address;

  if (!input->yuv_structure) {
    buf = malloc(nOutputSampleSize * bytes_y);
  } else {
    buf = malloc(nOutputSampleSize * (bytes_y + 2 * bytes_uv));
  }

  if (NULL == buf) {
    no_mem_exit("find_snr_background: buf");
  }

  snr->snr_y = 0.0;
  snr->snr_u = 0.0;
  snr->snr_v = 0.0;

  rewind(p_ref);

  if (hd->BgRefPicExist) {
    if (!input->ref_pic_order) {  // ref order
      status =
          fseek(p_ref, (long long int)(nOutputSampleSize * framesize_in_bytes *
                                       (hd->background_number - 1)),
                0);
    } else {
      status =
          fseek(p_ref, (long long int)(nOutputSampleSize * framesize_in_bytes *
                                       (hd->background_number - 1)),
                0);
    }

    if (status != 0) {
      snprintf(hc->errortext, ET_SIZE, "Error in seeking img->tr: %d", img->tr);
      hd->BgRefPicExist = 0;
    }
  }

  if (!input->yuv_structure) {
    fread(buf, sizeof(unsigned char), nOutputSampleSize * bytes_y, p_ref);
  } else {
    fread(buf, sizeof(unsigned char),
          nOutputSampleSize * (bytes_y + 2 * bytes_uv), p_ref);
  }

  if (!input->yuv_structure) {  // YUV
    for (j = 0; j < img_height; j++) {
      if (input->output_bit_depth == 8) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = buf[j * img_width + i];
          imgYRef[j * img_width + i] &= 0xff;  // reset high 8 bits
        }
      } else if (input->output_bit_depth == 10) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = ((byte *)buf)[j * img_width + i] & 0x3ff;
        }
      } else if (input->output_bit_depth == 12) {
        for (i = 0; i < img_width; i++) {
          imgYRef[j * img_width + i] = ((byte *)buf)[j * img_width + i] & 0xfff;
        }
      }
    }
  }
// TODO: Not support UYVY for G2
#if 0
    else { //U0Y0 V1Y1
        for (j = 0; j < img_height; j++) {
            for (i = 0; i < img_width; i++) {
                if (input->output_bit_depth == 8) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = buf[2 * offset_units + 1];
                    hd->imgYRef[j][i] &= 0xff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = buf[2 * offset_units];
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = buf[2 * offset_units];
                    }
                } else if (input->output_bit_depth == 10) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = ((byte *)buf)[2 * offset_units + 1];
                    hd->imgYRef[j][i] = hd->imgYRef[j][i] & 0x3ff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[0][j][i / 2] &= 0x3ff;
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[1][j][i / 2] &= 0x3ff;
                    }
                } else if (input->output_bit_depth == 12) {
                    offset_units = j * img_width + i;
                    hd->imgYRef[j][i] = ((byte *)buf)[2 * offset_units + 1];
                    hd->imgYRef[j][i] = hd->imgYRef[j][i] & 0xfff;
                    if (offset_units % 2 == 0) {   //U component
                        hd->imgUVRef[0][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[0][j][i / 2] &= 0xfff;
                    } else {              //V component
                        hd->imgUVRef[1][j][i / 2] = ((byte *)buf)[2 * offset_units];
                        hd->imgUVRef[1][j][i / 2] &= 0xfff;
                    }
                }

            }
        }
    }
#endif

  for (uv = 0; uv < 2; uv++) {
    if (!input->yuv_structure) {
      fread(buf, sizeof(unsigned char),
            nOutputSampleSize * img_height_cr * img_width_cr, p_ref);

      for (j = 0; j < img_height_cr; j++) {
        for (i = 0; i < img_width_cr; i++) {
          if (input->output_bit_depth == 8) {
            imgUVRef[uv][j * img_width_cr + i] = buf[j * img_width_cr + i];
            imgUVRef[uv][j * img_width_cr + i] &= 0xff;
          } else if (input->output_bit_depth == 10) {
            imgUVRef[uv][j * img_width_cr + i] =
                ((byte *)buf)[j * img_width_cr + i] & 0x3ff;
          } else if (input->output_bit_depth == 12) {
            imgUVRef[uv][j * img_width_cr + i] =
                ((byte *)buf)[j * img_width_cr + i] & 0xfff;
          }
        }
      }
    }
  }

  img->quad[0] = 0;
  diff_y = 0;

  for (j = 0; j < img_height; ++j) {
    for (i = 0; i < img_width; ++i) {
      if (nBitDepthDiff == 0) {
        diff_y +=
            img->quad
                [abs(imgY[j * img_width + i] - imgYRef[j * img_width + i])];
      } else if (nBitDepthDiff > 0) {
        diff_y += img->quad[abs(
            Clip1((imgY[j * img_width + i] + (1 << (nBitDepthDiff - 1))) >>
                  nBitDepthDiff) -
            imgYRef[j * img_width + i])];
      }
    }
  }

  // Chroma
  diff_u = 0;
  diff_v = 0;

  for (j = 0; j < img_height_cr; ++j) {
    for (i = 0; i < img_width_cr; ++i) {
      if (nBitDepthDiff == 0) {
        diff_u += img->quad[abs(imgUVRef[0][j * img_width_cr + i] -
                                imgUV[0][j * img_width_cr + i])];
        diff_v += img->quad[abs(imgUVRef[1][j * img_width_cr + i] -
                                imgUV[1][j * img_width_cr + i])];
      } else if (nBitDepthDiff > 0) {
        diff_u += img->quad[abs(imgUVRef[0][j * img_width_cr + i] -
                                Clip1((imgUV[0][j * img_width_cr + i] +
                                       (1 << (nBitDepthDiff - 1))) >>
                                      nBitDepthDiff))];
        diff_v += img->quad[abs(imgUVRef[1][j * img_width_cr + i] -
                                Clip1((imgUV[1][j * img_width_cr + i] +
                                       (1 << (nBitDepthDiff - 1))) >>
                                      nBitDepthDiff))];
      }
    }
  }

  // Collecting SNR statistics
  if (diff_y != 0) {
    snr->snr_y =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)diff_y));  // luma snr for current frame
  }

  if (diff_u != 0) {
    snr->snr_u =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_u)));  //  chroma snr
                                                                // for current
                                                                // frame,422
  }

  if (diff_v != 0) {
    snr->snr_v =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_v)));  //  chroma snr
                                                                // for current
                                                                // frame,422
  }

  if (img->number == /*0*/ 1) {  // first
    snr->snr_y1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)diff_y));  // keep luma snr for first frame
    snr->snr_u1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_u)));  // keep chroma
                                                                // snr for first
                                                                // frame,422
    snr->snr_v1 =
        (float)(10 * log10(((1 << input->output_bit_depth) - 1) *
                           ((1 << input->output_bit_depth) - 1) *
                           (float)(img_width /*img->width*/) *
                           (img_height /*img->height*/) /
                           (float)(/*4*/ uvformat * diff_v)));  // keep chroma
                                                                // snr for first
                                                                // frame,422
    snr->snr_ya = snr->snr_y1;
    snr->snr_ua = snr->snr_u1;
    snr->snr_va = snr->snr_v1;

    if (diff_y == 0) {
      snr->snr_ya = /*50*/ 0;
    }

    if (diff_u == 0) {
      snr->snr_ua = /*50*/ 0;
    }

    if (diff_v == 0) {
      snr->snr_va = /*50*/ 0;
    }
  } else {
    snr->snr_ya =
        (float)(snr->snr_ya * (img->number + hc->Bframe_ctr - 1) + snr->snr_y) /
        (img->number +
         hc->Bframe_ctr);  // average snr chroma for all frames 20080721
    snr->snr_ua =
        (float)(snr->snr_ua * (img->number + hc->Bframe_ctr - 1) + snr->snr_u) /
        (img->number +
         hc->Bframe_ctr);  // average snr luma for all frames 20080721
    snr->snr_va =
        (float)(snr->snr_va * (img->number + hc->Bframe_ctr - 1) + snr->snr_v) /
        (img->number +
         hc->Bframe_ctr);  // average snr luma for all frames 20080721
  }

  free(buf);
}

void prepare_RefInfo() {
  int i, j;
  int flag = 0;
  avs2_frame_t *tmp_fref;

  //////////////////////////////////////////////////////////////////////////
  // update IDR frame
  if (img->tr > hd->next_IDRtr && hd->curr_IDRtr != hd->next_IDRtr) {
    hd->curr_IDRtr = hd->next_IDRtr;
    hd->curr_IDRcoi = hd->next_IDRcoi;
  }
  //////////////////////////////////////////////////////////////////////////
  // re-order the ref buffer according to RPS
  img->num_of_references = hd->curr_RPS.num_of_ref;

  for (i = 0; i < hd->curr_RPS.num_of_ref; i++) {
    int accumulate = 0;
    /* copy tmp_fref from fref[i] */
    tmp_fref = fref[i];

#if 1
    for (j = i; j < REF_MAXBUFFER; j++) {  ///////////////to be modified  IDR
      if (fref[j]->img_coi ==
          img->coding_order -
              hd->curr_RPS.ref_pic[i]) {  // doi specified by header
        break;
      }
    }
#else

    for (j = i; j < REF_MAXBUFFER; j++) {  ///////////////to be modified  IDR
      int k, tmp_tr;
      for (k = 0; k < REF_MAXBUFFER; k++) {
        if (((int)img->coding_order - (int)hd->curr_RPS.ref_pic[i]) ==
                fref[k]->imgcoi_ref &&
            fref[k]->imgcoi_ref >= -256) {
          break;
        }
      }
      if (k == REF_MAXBUFFER) {
        tmp_tr = -1;
      } else {
        tmp_tr = fref[k]->imgtr_fwRefDistance;
      }
      if (tmp_tr < hd->curr_IDRtr) {
        hd->curr_RPS.ref_pic[i] = img->coding_order - hd->curr_IDRcoi;

        for (k = 0; k < i; k++) {
          if (hd->curr_RPS.ref_pic[k] == hd->curr_RPS.ref_pic[i]) {
            accumulate++;
            break;
          }
        }
      }
      if (fref[j]->imgcoi_ref == img->coding_order - hd->curr_RPS.ref_pic[i]) {
        break;
      }
    }
    if (j == REF_MAXBUFFER || accumulate) {
      img->num_of_references--;
    }
#endif
    if (j != REF_MAXBUFFER) {
      /* copy fref[i] from fref[j] */
      fref[i] = fref[j];
      /* copy fref[j] from ferf[tmp] */
      fref[j] = tmp_fref;
    }
  }
  if (img->type == B_IMG &&
      (fref[0]->img_poi <= img->tr || fref[1]->img_poi >= img->tr)) {

    printf("wrong reference configuration for B frame");
    exit(-1);
    //******************************************//
  }

#if !1
  //////////////////////////////////////////////////////////////////////////
  // delete the frame that will never be used
  for (i = 0; i < hd->curr_RPS.num_to_remove; i++) {
    for (j = 0; j < REF_MAXBUFFER; j++) {
      if (fref[j]->imgcoi_ref >= -256 &&
          fref[j]->imgcoi_ref ==
              img->coding_order - hd->curr_RPS.remove_pic[i]) {
        break;
      }
    }
    if (j < REF_MAXBUFFER && j >= img->num_of_references) {
      fref[j]->imgcoi_ref = -257;
#if M3480_TEMPORAL_SCALABLE
      fref[j]->temporal_id = -1;
#endif
      if (fref[j]->is_output == -1) {
        fref[j]->img_poi = -256;
      }
    }
  }
#endif

  //////////////////////////////////////////////////////////////////////////
  // add inter-view reference picture

  //////////////////////////////////////////////////////////////////////////
  //   add current frame to ref buffer
  for (i = 0; i < REF_MAXBUFFER; i++) {
    // check fref buffer, if invalid, break, 9.2.2.d,e
    if ((fref[i]->img_coi < -256 || abs(fref[i]->img_poi - img->tr) >= 128) &&
        fref[i]->is_output == -1) {
      break;
    }
  }
  if (i == REF_MAXBUFFER) {
    i--;
  }
  // use invalid buffer
  hc->f_rec = fref[i];  // set current frame buffer
  hc->currentFrame = hc->f_rec->ref;
  hc->f_rec->img_poi = img->tr;  // POI distance store to img_poi
  hc->f_rec->img_coi = img->coding_order;  // DOI store to imgcoi_ref
#if M3480_TEMPORAL_SCALABLE
  hc->f_rec->temporal_id = hd->cur_layer;
#endif
  hc->f_rec->is_output = 1;
  hc->f_rec->refered_by_others = hd->curr_RPS.referd_by_others;

  if (img->type != B_IMG) {
    for (j = 0; j < img->num_of_references; j++) {
      hc->f_rec->ref_poc[j] = fref[j]->img_poi;  // store poi to ref_poc[j] ,POI
    }
  } else {
    hc->f_rec->ref_poc[0] = fref[0]->img_poi;  // B frame reference is 2
    hc->f_rec->ref_poc[1] = fref[1]->img_poi;
  }

#if M3480_TEMPORAL_SCALABLE

  for (j = img->num_of_references; j < MAXREF; j++) {
    hc->f_rec->ref_poc[j] = 0;  // clear unused reference frame POI
  }

  if (img->type == INTRA_IMG) {
    int l;
    for (l = 0; l < MAXREF; l++) {
      hc->f_rec->ref_poc[l] = img->tr;
    }
  }
  cleanRefMVBufRef(i);
#endif

  //////////////////////////////////////////////////////////////////////////
  // updata ref pointer

  if (img->type != I_IMG) {
    for (j = 0; j < MAXREF; j++) {  // ref_index = 0
      hd->integerRefY[j] = &fref[j]->ref[0];
      hd->integerRefUV[j][0] = &fref[j]->ref[1];
      hd->integerRefUV[j][1] = &fref[j]->ref[2];
    }

    // forward/backward reference buffer
    // f_ref[ref_index][yuv][height(height/2)][width] ref_index=0 for B frame,
    // ref_index = 0,1 for B field
    hd->f_ref[0] = fref[1]->ref;
    // b_ref[ref_index][yuv][height(hei ght/2)][width] ref_index=0 for B frame,
    // ref_index = 0,1 for B field
    hd->b_ref[0] = fref[0]->ref;

    for (j = 0; j < 1; j++) {  // ref_index = 0 luma = 0
      hd->integerRefY_fref[j] = hd->f_ref[j][0];
      hd->integerRefY_bref[j] = hd->b_ref[j][0];
    }
    // chroma for backward
    for (j = 0; j < 1; j++) {  // ref_index = 0
      for (i = 0; i < 2; i++) {  // chroma uv =0,1; 1,2 for referenceFrame
        hd->integerRefUV_fref[j][i] = hd->f_ref[j][i + 1];
        hd->integerRefUV_bref[j][i] = hd->b_ref[j][i + 1];
      }
    }
#if 0
		//img->imgtr_next_P: ref[0] POI(B frame), or current POI
        img->imgtr_next_P = img->type == B_IMG ? fref[0]->imgtr_fwRefDistance : img->tr;
        if (img->type == B_IMG) {
            hd->trtmp = fref[0]->imgtr_fwRefDistance;   //tmp store = backward POI
            // fref[0]->imgtr_fwRefDistance: froward POI
            fref[0]->imgtr_fwRefDistance = fref[1]->imgtr_fwRefDistance;
        }
#endif
  }

#if !1
  {
    int k, x, y, ii;
#if B_BACKGROUND_Fix
    for (ii = 0; ii < REF_MAXBUFFER; ii++) {
#else
    for (ii = 0; ii < 8; ii++) {
#endif
      for (k = 0; k < 2; k++) {
        for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
          for (x = 0; x < img->width / MIN_BLOCK_SIZE; x++) {
            if (img->typeb == BP_IMG) {
              fref[ii]->mvbuf[y][x][k] = 0;
              fref[ii]->refbuf[y][x] = 0;
            }
          }
        }
      }
    }
  }
#endif
}
#if Mv_Rang  // M3959 he-yuan.lin@mstarsemi.com
void check_mv_boundary(int pos_x, int pos_y, int dx, int dy, int block_width,
                       int block_height) {
  int is_out_of_boundary = 0;
  int extend_x_left = (dx != 0) ? 3 : 0;
  int extend_y_top = (dy != 0) ? 3 : 0;
  int extend_x_right = (dx != 0) ? 4 : 0;
  int extend_y_down = (dy != 0) ? 4 : 0;

  // left
  if ((pos_x - extend_x_left) < -64) {
    is_out_of_boundary = 1;
  }
  // right
  if (((pos_x + block_width - 1) + extend_x_right) >=
      (hd->horizontal_size + 64)) {
    is_out_of_boundary = 1;
  }
  // up
  if ((pos_y - extend_y_top) < -64) {
    is_out_of_boundary = 1;
  }
  // bottom
  if (((pos_y + block_height - 1) + extend_y_down) >=
      (hd->vertical_size + 64)) {
    is_out_of_boundary = 1;
  }

  if (is_out_of_boundary) {
    printf(
        "Non-conformance stream: invalid reference block (x, y) = (%d, %d)\n",
        pos_x, pos_y);
  }
}
#endif
#if 0
void Read_ALF_param(Bitstream *curStream,char *buf, int startcodepos, int length)
{
    int compIdx;
    if (input->alf_enable) {
        img->pic_alf_on[0] = u_v(curStream,1, "alf_pic_flag_Y");
        img->pic_alf_on[1] = u_v(curStream,1, "alf_pic_flag_Cb");
        img->pic_alf_on[2] = u_v(curStream,1, "alf_pic_flag_Cr");
        Dec_ALF->m_alfPictureParam[ALF_Y]->alf_flag  = img->pic_alf_on[ALF_Y];
        Dec_ALF->m_alfPictureParam[ALF_Cb]->alf_flag = img->pic_alf_on[ALF_Cb];
        Dec_ALF->m_alfPictureParam[ALF_Cr]->alf_flag = img->pic_alf_on[ALF_Cr];
        if (img->pic_alf_on[0] || img->pic_alf_on[1] || img->pic_alf_on[2]) {
            for (compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++) {
                if (img->pic_alf_on[compIdx]) {
                    readAlfCoeff(curStream,Dec_ALF->m_alfPictureParam[compIdx]);
                }
            }
        }
    }

}
#endif
/*
*************************************************************************
* Function:Reads the content of two successive startcode from the bitstream
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

int Header() {
  unsigned char *Buf;
  int startcodepos, length;

  if ((Buf = (char *)calloc(MAX_CODED_FRAME_SIZE, sizeof(char))) == NULL) {
    no_mem_exit("GetAnnexbNALU: Buf");
  }

  while (1) {
    // hd->StartCodePosition = GetOneUnit(Buf, &startcodepos, &length);
    // //jlzheng  7.5
    hd->StartCodePosition =
        GetOneFrame(Buf, &startcodepos, &length);  // jlzheng  7.5

    switch (Buf[startcodepos]) {
      case SEQUENCE_HEADER_CODE:

        img->new_sequence_flag = 1;
#if SEQ_CHANGE_CHECKER
        if (seq_checker_buf == NULL) {
          seq_checker_buf = malloc(length);
          seq_checker_length = length;
          memcpy(seq_checker_buf, Buf, length);
        } else {
          if ((seq_checker_length != length) ||
              (memcmp(seq_checker_buf, Buf, length) != 0)) {
            free(seq_checker_buf);
            fprintf(
                stdout,
                "Non-conformance stream: sequence header cannot change !!\n");
#if RD170_FIX_BG
            seq_checker_buf = NULL;
            seq_checker_length = 0;
            seq_checker_buf = malloc(length);
            seq_checker_length = length;
            memcpy(seq_checker_buf, Buf, length);
#endif
          }
        }
#endif
#if 0  // RD170_FIX_BG
      if (input->alf_enable&& alfParAllcoated == 1) {
        ReleaseAlfGlobalBuffer();
        alfParAllcoated = 0;
      }
#endif
#if FIX_FLUSH_DPB_BY_LF
        if (hd->vec_flag) {
          int k;
          flushDPB();
          for (k = 0; k < REF_MAXBUFFER; k++) {
            cleanRefMVBufRef(k);
          }
          hd->vec_flag = 0;
          free_global_buffers();
          img->number = 0;
          img->PrevPicDistanceLsb = 0;
        }
#endif

#if FIX_SEQ_END_FLUSH_DPB_BY_LF
        if (img->new_sequence_flag && img->sequence_end_flag) {
          int k;
          flushDPB();
          for (k = 0; k < REF_MAXBUFFER; k++) {
            cleanRefMVBufRef(k);
          }
          free_global_buffers();
          img->number = 0;
          img->PrevPicDistanceLsb = 0;
        }
#endif
        SequenceHeader(Buf, startcodepos, length);

#if 0
            if (input->alf_enable && alfParAllcoated == 0) {
                CreateAlfGlobalBuffer();
                alfParAllcoated = 1;
            }
#endif

        img->seq_header_indicate = 1;
        break;
      case EXTENSION_START_CODE:
        extension_data(Buf, startcodepos, length);
        break;
      case USER_DATA_START_CODE:
        user_data(Buf, startcodepos, length);
        break;
      case VIDEO_EDIT_CODE:
        video_edit_code_data(Buf, startcodepos, length);
#if SEQ_CHANGE_CHECKER
        if (seq_checker_buf != NULL) {
          free(seq_checker_buf);
          seq_checker_buf = NULL;
          seq_checker_length = 0;
        }
#endif
        break;
      case I_PICTURE_START_CODE:
        I_Picture_Header(Buf, startcodepos, length);
        calc_picture_distance();

        // Read_ALF_param(currStream, Buf, startcodepos, length);

        if (!img->seq_header_indicate) {
          img->B_discard_flag = 1;
          fprintf(stdout, "    I   %3d\t\tDIDSCARD!!\n", img->tr);
          break;
        }

        break;
      case PB_PICTURE_START_CODE:
        PB_Picture_Header(Buf, startcodepos, length);
        calc_picture_distance();

        // Read_ALF_param(currStream,Buf, startcodepos, length);
        // xiaozhen zheng, 20071009
        if (!img->seq_header_indicate) {
          img->B_discard_flag = 1;

          if (img->type == P_IMG) {
            fprintf(stdout, "    P   %3d\t\tDIDSCARD!!\n", img->tr);
          }
          if (img->type == F_IMG) {
            fprintf(stdout, "    F   %3d\t\tDIDSCARD!!\n", img->tr);
          } else {
            fprintf(stdout, "    B   %3d\t\tDIDSCARD!!\n", img->tr);
          }

          break;
        }

        if (img->seq_header_indicate == 1 && img->type != B_IMG) {
          img->B_discard_flag = 0;
        }
        if (img->type == B_IMG && img->B_discard_flag == 1 &&
            !img->random_access_decodable_flag) {
          fprintf(stdout, "    B   %3d\t\tDIDSCARD!!\n", img->tr);
          break;
        }

        break;
      case SEQUENCE_END_CODE:
#if SEQ_CHANGE_CHECKER
        if (seq_checker_buf != NULL) {
          free(seq_checker_buf);
          seq_checker_buf = NULL;
          seq_checker_length = 0;
        }
#endif

        img->new_sequence_flag = 1;
        img->sequence_end_flag = 1;
        free(Buf);
        return EOS;
        break;
      default:

        if ((Buf[startcodepos] >= SLICE_START_CODE_MIN &&
             Buf[startcodepos] <= SLICE_START_CODE_MAX) &&
            ((!img->seq_header_indicate) ||
             (img->type == B_IMG && img->B_discard_flag == 1 &&
              !img->random_access_decodable_flag))) {
          break;
        } else if (Buf[startcodepos] >=
                   SLICE_START_CODE_MIN) {  // modified by jimmy 2009.04.01
          if ((temp_slice_buf =
                   (char *)calloc(MAX_CODED_FRAME_SIZE, sizeof(char))) ==
              NULL) {  // modified 08.16.2007
            no_mem_exit("GetAnnexbNALU: Buf");
          }

          first_slice_length = length;
          first_slice_startpos = startcodepos;
          memcpy(temp_slice_buf, Buf, length);
          free(Buf);
          return SOP;
        } else {
          printf("Can't find start code");
          free(Buf);
          return EOS;
        }
    }
  }
}

/*
*************************************************************************
* Function:Initializes the parameters for a new frame
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/

void init_frame() {
  int i;

#if RD1510_FIX_BG
  if (img->type == I_IMG && img->typeb == BACKGROUND_IMG) {  // G/GB frame
    img->num_of_references = 0;
  } else if (img->type == P_IMG &&
             img->typeb ==
                 BP_IMG) {  // only one reference frame(G\GB) for S frame
    img->num_of_references = 1;
  }
#endif

  if (img->typeb == BACKGROUND_IMG && hd->background_picture_output_flag == 0) {
    hc->currentFrame = hc->background_ref;
  } else {
    prepare_RefInfo();
  }

#if FIX_CHROMA_FIELD_MV_BK_DIST
  if (img->typeb == BACKGROUND_IMG && img->is_field_sequence) {
    bk_img_is_top_field = img->is_top_field;
  }
#endif

  {  // Set recon frame
    hc->imgY = &hc->currentFrame[0];
    hc->imgUV[0] = &hc->currentFrame[1];
    hc->imgUV[1] = &hc->currentFrame[2];
    struct Avs2ReconParam *recon = &avs2dec_cont->storage.recon;
    recon->y = hc->currentFrame[0];
    recon->c = hc->currentFrame[1];
    recon->y_tbl.bus_address = (size_t)hc->f_rec->tbl_buf_y;
    recon->c_tbl.bus_address = (size_t)hc->f_rec->tbl_buf_c;
    recon->mv.bus_address = (addr_t)hc->f_rec->mvbuf;
    Avs2HwdSetParams(&avs2dec_cont->hwdec, ATTRIB_RECON, recon);
  }

#if RD160_FIX_BG  // fred.chiu@mediatek.com
  if (hd->weight_quant_enable_flag && hd->pic_weight_quant_enable_flag) {
    WeightQuantEnable = 1;
  } else {
    WeightQuantEnable = 0;
  }
#endif
// Adaptive frequency weighting quantization
#if FREQUENCY_WEIGHTING_QUANTIZATION
  if (hd->weight_quant_enable_flag && hd->pic_weight_quant_enable_flag) {
    InitFrameQuantParam(avs2dec_cont);
    FrameUpdateWQMatrix(avs2dec_cont);  // M2148 2007-09
  }
#endif
}

/**
 * alf: constructed filter coefficients
 */
static void alfReconstructCoefficients(ALFParam *alfParam, int *filterCoeff) {
  int g, sum, i, coeffPred;
  for (g = 0; g < alfParam->filters_per_group; g++) {
    sum = 0;
    for (i = 0; i < alfParam->num_coeff - 1; i++) {
      sum += (2 * alfParam->coeffmulti[g][i]);
      filterCoeff[g * ALF_MAX_NUM_COEF + i] = alfParam->coeffmulti[g][i];
    }
    coeffPred = (1 << ALF_NUM_BIT_SHIFT) - sum;
    filterCoeff[g * ALF_MAX_NUM_COEF + alfParam->num_coeff - 1] =
        coeffPred + alfParam->coeffmulti[g][alfParam->num_coeff - 1];
  }
}

/**
 * alf: constructed filter coefficients and map table
 */
static void alfReconstructAlfCoefInfo(int compIdx, ALFParam *alfParam,
                                      int *filterCoeff, int *varIndTab) {
  int i;
  if (compIdx == ALF_Y) {
    memset(varIndTab, 0, NO_VAR_BINS * sizeof(int));
    if (alfParam->filters_per_group > 1) {
      for (i = 1; i < NO_VAR_BINS; ++i) {
        if (alfParam->filterPattern.virtual_address[i]) {
          varIndTab[i] = varIndTab[i - 1] + 1;
        } else {
          varIndTab[i] = varIndTab[i - 1];
        }
      }
    }
  }
  alfReconstructCoefficients(alfParam, filterCoeff);
}
#if 0
/**
 * alf: prepare registers for alf filter.
 */
static void alfPrepareParams(ALFParam **alfPictureParam)
{
    int compIdx;
    int *filterCoeffSym;
    int auto_crop_bottom, auto_crop_right;
    int alfWidth, alfHeight;
    int lcuWidth, lcuHeight;
    int currAlfEnable;

    /* flag */
    //sw_ctrl->alf_flag[ALF_Y] = img->pic_alf_on[ALF_Y];
    //sw_ctrl->alf_flag[ALF_Cb] = img->pic_alf_on[ALF_Cb];
    //sw_ctrl->alf_flag[ALF_Cr] = img->pic_alf_on[ALF_Cr];
    currAlfEnable = !(sw_ctrl->alf_flag[ALF_Y] == 0 &&
                      sw_ctrl->alf_flag[ALF_Cb] == 0 &&
                      sw_ctrl->alf_flag[ALF_Cr] == 0);
    if (!currAlfEnable)
        return;

    /* region interval */
    if (hd->horizontal_size % (1<<sw_ctrl->min_cb_size) != 0) {
        auto_crop_right =  (1<<sw_ctrl->min_cb_size) - (hd->horizontal_size % (1<<sw_ctrl->min_cb_size));
    } else {
        auto_crop_right = 0;
    }

    if (hd->vertical_size %  (1<<sw_ctrl->min_cb_size) != 0) {
        auto_crop_bottom =  (1<<sw_ctrl->min_cb_size) - (hd->vertical_size % (1<<sw_ctrl->min_cb_size));
    } else {
        auto_crop_bottom = 0;
    }
    alfWidth          = (hd->horizontal_size + auto_crop_right);
    alfHeight         = (hd->vertical_size + auto_crop_bottom);
    lcuWidth = lcuHeight = 1 << (sw_ctrl->max_cb_size);
    sw_ctrl->alf_interval_x = ((((alfWidth + lcuWidth - 1) / lcuWidth) + 1) / 4 * lcuWidth) ;
    sw_ctrl->alf_interval_y = ((((alfHeight + lcuHeight - 1) / lcuHeight) + 1) / 4 * lcuHeight) ;

    /* coeeficients */
    for (compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++) {
        filterCoeffSym = (compIdx==ALF_Y)?(sw_ctrl->alf_coeff_ytable)
                       : ((compIdx==ALF_Cb)?(sw_ctrl->alf_coeff_utable)
                       : (sw_ctrl->alf_coeff_vtable));
        alfReconstructAlfCoefInfo(compIdx, alfPictureParam[compIdx],
                                     filterCoeffSym, sw_ctrl->alf_merge_ytable);
    }
}

struct CtbCtrl outStruct;
#endif
// this row should be removed if bind together with G2.

void FrameAvs2Filter(struct SwCtrl *sw_ctrl, u16 *y, u16 *u, u16 *v);

#if 0
/* write filter result to external YUV buffer */
void Avs2FilterOutputCheck(struct Filterd *filterd, struct SwCtrl *sw_ctrl,
                  struct CtbCtrl *in_ctrl, u16 *out_data,
                  u16 *y, u16 *u, u16 *v) {

  /* const */
  const int ALFOFFSET=4;

  /* Variables */

  struct Location LOC, *loc=&LOC;
  u32 i, j, comp;
  u16 *out;//, *in;
  u32 top_count; //, top_offset = 0;
  u32 curr_count;
  u32 w, valid_w, valid_h;
  u32 s_rows, s_cols;
  u32 ctb_size = 1 << sw_ctrl->max_cb_size;
  u16 *current, *data;
  int offset, stride, height;
  int px, py; /* ctu position in picture */

  /* Code */

  LOC.row = in_ctrl->row;
  LOC.col = in_ctrl->col;
  LOC.ctb_size = 1 << sw_ctrl->max_cb_size;
  LOC.last_row = LOC.row == sw_ctrl->pic_height_in_ctbs - 1;
  LOC.last_col = LOC.col == sw_ctrl->pic_width_in_ctbs - 1;
  LOC.w_left_y = 8;
  LOC.w_left_c = 4;
  LOC.o_left_y = LOC.ctb_size * LOC.row * LOC.w_left_y + LOC.w_left_y - 1;
  LOC.o_left_c = LOC.ctb_size / 2 * LOC.row * LOC.w_left_c + LOC.w_left_c - 1;
  LOC.h_top_y = 4;
  LOC.o_top_y = LOC.ctb_size * LOC.col +
                (LOC.h_top_y - 1) * LOC.ctb_size * sw_ctrl->pic_width_in_ctbs;
  LOC.w_top_y = LOC.ctb_size * sw_ctrl->pic_width_in_ctbs;
  LOC.h_top_c = 4;
  LOC.o_top_c = LOC.ctb_size / 2 * LOC.col + (LOC.h_top_c - 1) * LOC.ctb_size /
                2 * sw_ctrl->pic_width_in_ctbs;
  LOC.w_top_c = LOC.ctb_size / 2 * sw_ctrl->pic_width_in_ctbs;

  ASSERT(filterd);
  ASSERT(loc);

  (void)in_ctrl;

  s_rows = loc->h_top_y;
  s_cols = loc->w_left_y+ALFOFFSET;

  w = ctb_size - s_cols + (loc->col ? s_cols : 0) +
      (loc->last_col ? s_cols : 0);

  out = out_data;
  top_count = 0;
  curr_count = ctb_size - (loc->row ? 0 : s_rows) + (loc->last_row ? s_rows : 0);
  if (loc->row) {
    top_count = s_rows;
    //top_offset = ctb_size * loc->col - (loc->col ? s_cols : 0);
  }

  /* rows from the above storage */
  px = in_ctrl->col<<sw_ctrl->max_cb_size;
  py = in_ctrl->row<<sw_ctrl->max_cb_size;
  stride = sw_ctrl->pic_width_in_cbs<<sw_ctrl->min_cb_size;
  height = sw_ctrl->pic_height_in_cbs<<sw_ctrl->min_cb_size;
  offset = (py)*stride + (px);
  data = y + offset - top_count * stride;
  if (loc->col) data -= s_cols;

  if (py+ctb_size>height)
      valid_h = curr_count - (py+ctb_size - height);
  else
      valid_h = curr_count;
  if (px+ctb_size>stride)
      valid_w = w - (px+ctb_size - stride);
  else
      valid_w = w;

  for (i = 0; i < curr_count; i++) {
    for (j = 0; j < w; j++) {
      if ((j>=valid_w)||(i>=valid_h))
        out++;
      else
        data[j] = *out++;
    }
    data += stride;
  }

  s_rows = loc->h_top_c;
  s_cols = loc->w_left_c+ALFOFFSET;

  /* chroma */
  ctb_size /= 2;
  stride /= 2;
  height /=2;
  px /= 2;
  py /= 2;
  curr_count = ctb_size - (loc->row ? 0 : s_rows) + (loc->last_row ? s_rows : 0);
  if (loc->row) {
    top_count = s_rows;
    //top_offset = ctb_size * loc->col - (loc->col ? s_cols : 0);
  }

  w = ctb_size - s_cols + (loc->col ? s_cols : 0) +
      (loc->last_col ? s_cols : 0);

  if (py+ctb_size>height)
      valid_h = curr_count - (py+ctb_size - height);
  else
      valid_h = curr_count;
  if (px+ctb_size>stride)
      valid_w = w - (px+ctb_size - stride);
  else
      valid_w = w;

  offset = (in_ctrl->row*ctb_size)*stride + (in_ctrl->col*ctb_size);
  for (comp = 0; comp < 2; comp++) {
    current = ((comp==0)?(u):(v)) +   offset;

    /* rows from the top storage */
    //in = filterd->above_c[comp] + top_offset;
    data = current - top_count*stride;
    if (loc->col) data -= s_cols;
    for (i = 0; i < curr_count; i++) {
      /* data from the left storage */
    for (j = 0; j < w; j++) {
      if ((j>=valid_w)||(i>=valid_h))
        out++;
      else
        data[j] = *out++;
    }
      data += stride;
    }
  }
}
#endif

void TBTiledToRasterUV(u8 *in, u16 *out_u, u16 *out_v, u32 pic_width,
                       u32 pic_height, u32 tile_stride) {
  u32 i, j;
  u32 k, l;
  u32 s, t;

  const u32 tile_width = 4, tile_height = 4;

  for (i = 0; i < pic_height; i += tile_height) {
    t = 0;
    s = 0;
    for (j = 0; j < pic_width; j += tile_width / 2) {
      /* copy this tile */
      for (k = 0; k < tile_height; ++k) {
        // for (l = 0; l < tile_width; ++l) {
        out_u[k * pic_width + 0 + s] = (u16)in[t++];
        out_v[k * pic_width + 0 + s] = (u16)in[t++];
        out_u[k * pic_width + 1 + s] = (u16)in[t++];
        out_v[k * pic_width + 1 + s] = (u16)in[t++];
        //}
      }

      /* move to next horizontal tile */
      s += tile_width / 2;
    }
    out_u += pic_width * tile_height;
    out_v += pic_width * tile_height;
    in += tile_stride;
  }
}
void TBTiledToRasterY(u8 *in, u16 *out, u32 pic_width, u32 pic_height,
                      u32 tile_stride) {
  u32 i, j;
  u32 k, l;
  u32 s, t;

  const u32 tile_width = 4, tile_height = 4;

  for (i = 0; i < pic_height; i += tile_height) {
    t = 0;
    s = 0;
    for (j = 0; j < pic_width; j += tile_width) {
      /* copy this tile */
      for (k = 0; k < tile_height; ++k) {
        for (l = 0; l < tile_width; ++l) {
          out[k * pic_width + l + s] = (u16)in[t++];
        }
      }

      /* move to next horizontal tile */
      s += tile_width;
    }
    out += pic_width * tile_height;
    in += tile_stride;
  }
}
#if 0
extern void PredRunAvs2(struct Pred *block, struct SwCtrl *sw_ctrl,
             struct CtbCtrl *in_ctrl, i32 *res, u16 *out_data);
#endif
#if 0
void showAlf(struct SwCtrl *sw_ctrl)
{
  int i,j;
  static FILE *fp=NULL;
  if (fp==NULL) {
    fp = fopen("traceAlfParam.txt", "wt");
  }
  fprintf(fp, "====\n");
  fprintf(fp, "alf_flag = %d, %d, %d\n", sw_ctrl->alf_flag[0],
            sw_ctrl->alf_flag[1],sw_ctrl->alf_flag[2]);
  fprintf(fp, "alf_enable = %d\n", sw_ctrl->alf_enable);
  fprintf(fp, "alf_interval_x = %d\n", sw_ctrl->alf_interval_x);
  fprintf(fp, "alf_interval_y = %d\n", sw_ctrl->alf_interval_y);
  fprintf(fp, "alf_merge_ytable[16]\n ");
  for(i=0;i<16;i++) fprintf(fp, "%d ", sw_ctrl->alf_merge_ytable[i]);
  fprintf(fp, "\n");
  fprintf(fp, "alf_coeff_ytable[16*ALF_MAX_NUM_COEF]\n ", sw_ctrl->alf_enable);
  for(j=0;j<16;j++) {
    for(i=0;i<16;i++) fprintf(fp, "%d ", sw_ctrl->alf_merge_ytable[i+9*j]);
    fprintf(fp, "\n");
  }
  fprintf(fp, "alf_coeff_utable[ALF_MAX_NUM_COEF]\n ");
  for(i=0;i<16;i++) fprintf(fp, "%d ", sw_ctrl->alf_coeff_utable[i]);
  fprintf(fp, "\n");
  fprintf(fp, "alf_coeff_vtable[ALF_MAX_NUM_COEF]\n ");
  for(i=0;i<16;i++) fprintf(fp, "%d ", sw_ctrl->alf_coeff_vtable[i]);
  fprintf(fp, "\n");

}

void showRegs(u32 *regs, int num)
{
  int i;

  static FILE *fp=NULL;
  if (fp==NULL) {
    fp = fopen("traceRegs.txt", "wt");
  }
  fprintf(fp, "====\n");
  for(i=0; i< num; i++)
    fprintf(fp, "swreg%3d %8x\n", i, regs[i]);
  fprintf(fp, "\n");

}
#else
#define showRegs(...)
#define showAlf(...)
#endif
/*
*************************************************************************
* Function:decodes one picture
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void RunAsic(void *inst) {
  struct Avs2Hwd *hwd = &avs2dec_cont->hwdec;
  struct G2Hw *g2hw = (struct G2Hw *)inst;

  Avs2HwdSetParams(hwd, ATTRIB_SEQ, &avs2dec_cont->storage.sps);
  Avs2HwdSetParams(hwd, ATTRIB_PIC, &avs2dec_cont->storage.pps);

  /* run asic */
  Avs2HwdRun(hwd);
  /* wait finish */
  Avs2HwdSync(hwd, 0);
}

void min_tr(outdata data, int *pos) {
  int i, tmp_min;
  tmp_min = data.stdoutdata[0].tr;
  *pos = 0;
  for (i = 1; i < data.buffer_num; i++) {
    if (data.stdoutdata[i].tr < tmp_min) {
      tmp_min = data.stdoutdata[i].tr;
      *pos = i;
    }
  }
}
void delete_trbuffer(outdata *data, int pos) {
  int i;
  for (i = pos; i < data->buffer_num - 1; i++) {
    data->stdoutdata[i] = data->stdoutdata[i + 1];
  }
  data->buffer_num--;
}

// copy current mv and ref to hc->f_rec
#if 0
void addCurrMvtoBuf()
{
    int k, x, y;

    for (k = 0; k < 2; k++) {
        for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
            for (x = 0; x < img->width / MIN_BLOCK_SIZE ; x++) {
                hc->f_rec->mvbuf[y][x][k] = ((img->type != B_IMG) ? img->tmp_mv : img->fw_mv)[y][x][k];
            }
        }
    }

    for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
        for (x = 0; x < img->width / MIN_BLOCK_SIZE ; x++) {
            hc->f_rec->refbuf[y][x] = img->fw_refFrArr.virtual_address[y * img->width / MIN_BLOCK_SIZE + x];
        }
    }
}

// Sub-sampling of the stored reference index and motion vector

void compressMotion()
{
    int x, y;
    int width = img->width;
    int height = img->height;
    int xPos, yPos;

    for (y = 0; y < height / MIN_BLOCK_SIZE; y ++) {
        for (x = 0; x < width / MIN_BLOCK_SIZE; x ++) {
            yPos = y / MV_DECIMATION_FACTOR * MV_DECIMATION_FACTOR + 2; //  (y /4) * 4 + 2
            xPos = x / MV_DECIMATION_FACTOR * MV_DECIMATION_FACTOR + 2; //  (x /4) * 4 + 2

            if (yPos >= height / MIN_BLOCK_SIZE) {
                yPos = (y / MV_DECIMATION_FACTOR * MV_DECIMATION_FACTOR + height / MIN_BLOCK_SIZE) / 2;
            }
            if (xPos >= width / MIN_BLOCK_SIZE) {
                xPos = (x / MV_DECIMATION_FACTOR * MV_DECIMATION_FACTOR + width / MIN_BLOCK_SIZE) / 2;
            }
            hc->f_rec->mvbuf[y][x][0] = hc->f_rec->mvbuf[yPos][xPos][0];
            hc->f_rec->mvbuf[y][x][1] = hc->f_rec->mvbuf[yPos][xPos][1];
            hc->f_rec->refbuf[y][x]   = hc->f_rec->refbuf[yPos][xPos];
        }
    }
}
#endif
/*
*************************************************************************
* Function:Prepare field and frame buffer after frame decoding
* Input:
* Output:
* Return:
* Attention:
*************************************************************************
*/
void frame_postprocessing() {
  int pointer_tmp = outprint.buffer_num;
  int i;
  STDOUT_DATA *p_outdata;
#if RD160_FIX_BG
  int j, tmp_min, output_cur_dec_pic, pos = -1;
  int search_times = outprint.buffer_num;
#endif
  byte *imgY;
  byte *imgUV[2];
  imgY = (byte *)hc->imgY->virtual_address;
  imgUV[0] = (byte *)hc->imgUV[0]->virtual_address;
  imgUV[1] = (byte *)hc->imgUV[1]->virtual_address;

  // pic dist by Grandview Semi. @ [06-07-20 15:25]
  img->PrevPicDistanceLsb = (img->coding_order % 256);
  if (hd->p_ref && !input->ref_pic_order) {
    if (img->typeb == BACKGROUND_IMG &&
        hd->background_picture_output_flag == 0) {
      find_snr_background(snr, hd->p_ref_background);
    } else
      find_snr(snr, hd->p_ref);  // if ref sequence exist
  }

  pointer_tmp = outprint.buffer_num;
  p_outdata = &outprint.stdoutdata[pointer_tmp];

  p_outdata->type = img->type;
  p_outdata->typeb = img->typeb;
  p_outdata->framenum = img->tr;
  p_outdata->tr = img->tr;
  p_outdata->qp = img->qp;
  p_outdata->snr_y = snr->snr_y;
  p_outdata->snr_u = snr->snr_u;
  p_outdata->snr_v = snr->snr_v;
  p_outdata->tmp_time = hd->tmp_time;
  p_outdata->picture_structure = img->picture_structure;
  p_outdata->curr_frame_bits = StatBitsPtr->curr_frame_bits;
  p_outdata->emulate_bits = StatBitsPtr->emulate_bits;
#if RD1501_FIX_BG
  p_outdata->background_picture_output_flag =
      hd->background_picture_output_flag;  // Longfei.Wang@mediatek.com
#endif

#if RD160_FIX_BG
  p_outdata->picture_reorder_delay = hd->picture_reorder_delay;
#endif
  outprint.buffer_num++;

#if RD170_FIX_BG
  search_times = outprint.buffer_num;
#endif
  // record the reference list
  strcpy(p_outdata->str_reference_list, hc->str_list_reference);

#if 0
    if (input->MD5Enable & 0x02) {
        int j, k;
        int img_width = (img->width - img->auto_crop_right);
        int img_height = (img->height - img->auto_crop_bottom);
        int img_width_cr = (img_width / 2);
        int img_height_cr = (img_height / (input->chroma_format == 1 ? 2 : 1));
        int nSampleSize = input->output_bit_depth == 8 ? 1 : 2;
        int shift1 = sw_ctrl->bit_depth_luma - input->output_bit_depth;
        unsigned char *pbuf;
        unsigned char *md5buf;
        md5buf = (unsigned char *)malloc(img_height * img_width * nSampleSize + img_height_cr * img_width_cr * nSampleSize * 2);

        if (md5buf != NULL) {
            if (!shift1 && input->output_bit_depth == 8) { // 8bit input -> 8bit encode
                pbuf = md5buf;
                for (j = 0; j < img_height; j++)
                    for (k = 0; k < img_width; k++) {
                        pbuf[j * img_width + k] = (unsigned char)imgY[j * img->width + k];
                    }

                pbuf = md5buf + img_height * img_width * sizeof(unsigned char);
                for (j = 0; j < img_height_cr; j++)
                    for (k = 0; k < img_width_cr; k++) {
                        pbuf[j * img_width_cr + k] = (unsigned char)imgUV[0][j * img->width_cr + k];
                    }

                pbuf = md5buf + img_height * img_width * sizeof(unsigned char) + img_height_cr * img_width_cr * sizeof(unsigned char);
                for (j = 0; j < img_height_cr; j++)
                    for (k = 0; k < img_width_cr; k++) {
                        pbuf[j * img_width_cr + k] = (unsigned char)imgUV[1][j * img->width_cr + k];
                    }
            } else if (!shift1 && input->output_bit_depth > 8) { // 10/12bit input -> 10/12bit encode
                pbuf = md5buf;
                for (j = 0; j < img_height; j++) {
                    memcpy(pbuf + j * img_width * nSampleSize, &(imgY[j*img->width+0]), img_width * sizeof(byte));
                }

                pbuf = md5buf + img_height * img_width * sizeof(byte);
                for (j = 0; j < img_height_cr; j++) {
                    memcpy(pbuf + j * img_width_cr * nSampleSize, &(imgUV[0][j*img->width_cr+0]), img_width_cr * sizeof(byte));
                }

                pbuf = md5buf + img_height * img_width * sizeof(byte) + img_height_cr * img_width_cr * sizeof(byte);
                for (j = 0; j < img_height_cr; j++) {
                    memcpy(pbuf + j * img_width_cr * nSampleSize, &(imgUV[1][j*img->width_cr+0]), img_width_cr * sizeof(byte));
                }
            } else if (shift1 && input->output_bit_depth == 8) { // 8bit input -> 10/12bit encode
                pbuf = md5buf;
                for (j = 0; j < img_height; j++)
                    for (k = 0; k < img_width; k++) {
                        pbuf[j * img_width + k] = (unsigned char)Clip1((imgY[j * img->width + k] + (1 << (shift1 - 1))) >> shift1);
                    }

                pbuf = md5buf + img_height * img_width * sizeof(unsigned char);
                for (j = 0; j < img_height_cr; j++)
                    for (k = 0; k < img_width_cr; k++) {
                        pbuf[j * img_width_cr + k] = (unsigned char)Clip1((imgUV[0][j * img->width_cr + k] + (1 << (shift1 - 1))) >> shift1);
                    }

                pbuf = md5buf + img_height * img_width * sizeof(unsigned char) + img_height_cr * img_width_cr * sizeof(unsigned char);
                for (j = 0; j < img_height_cr; j++)
                    for (k = 0; k < img_width_cr; k++) {
                        pbuf[j * img_width_cr + k] = (unsigned char)Clip1((imgUV[1][j * img->width_cr + k] + (1 << (shift1 - 1))) >> shift1);
                    }
            }
            BufferMD5(md5buf, img_height * img_width * nSampleSize + img_height_cr * img_width_cr * nSampleSize * 2, MD5val);
        } else {
            printf("malloc md5 buffer error!\n");
            memset(MD5val, 0, 16);
        }
        if (md5buf) {
            free(md5buf);
        }
    } else {
        memset(MD5val, 0, 16);
    }
#endif
  {
    int j;
    for (j = 0; j < 4; j++) {
      p_outdata->DecMD5Value[j] = MD5val[j];
    }
  }
#if 0
    mv_print_avs2();
#endif

#if !REF_OUTPUT
  for (i = 0; i < outprint.buffer_num; i++) {
    min_tr(outprint, &pos);
    if (outprint.stdoutdata[pos].tr < img->tr ||
        outprint.stdoutdata[pos].tr == (hd->last_output + 1)) {
      hd->last_output = outprint.stdoutdata[pos].tr;
      report_frame(outprint, pos);
      write_frame(hd->p_out, outprint.stdoutdata[pos].tr);
      delete_trbuffer(&outprint, pos);
      i--;
    } else {
      break;
    }
  }
#else
#if RD160_FIX_BG  // Longfei.Wang@mediatek.com
  tmp_min = 1 << 20;
  i = 0, j = 0;
  output_cur_dec_pic = 0;
  pos = -1;
  for (j = 0; j < search_times; j++) {
    pos = -1;
    tmp_min = (1 << 20);
    // search for min poi picture to display
    for (i = 0; i < outprint.buffer_num; i++) {
      if ((outprint.stdoutdata[i].tr < tmp_min) &&
          ((outprint.stdoutdata[i].tr +
            outprint.stdoutdata[i].picture_reorder_delay) <=
           (int)img->coding_order)) {
        pos = i;
        tmp_min = outprint.stdoutdata[i].tr;
      }
    }

    if ((0 == hd->displaydelay) && (0 == output_cur_dec_pic)) {  // 9.2.4.a.1
      if (img->tr <= tmp_min) {  ////fred.chiu@mediatek.com
        // output current decode picture right now
        pos = outprint.buffer_num - 1;
        output_cur_dec_pic = 1;
      }
    }

    if (pos != -1) {
      hd->last_output = outprint.stdoutdata[pos].tr;
      report_frame(outprint, pos);
      if (outprint.stdoutdata[pos].typeb == BACKGROUND_IMG &&
          outprint.stdoutdata[pos].background_picture_output_flag == 0) {
        write_GB_frame(hd->p_out_background);
      } else {
        write_frame(hd->p_out, outprint.stdoutdata[pos].tr);
      }
      delete_trbuffer(&outprint, pos);
    }
  }

#else
  if (img->coding_order + (unsigned int)hc->total_frames * 256 >=
      (unsigned int)hd->picture_reorder_delay) {
    int tmp_min, pos = -1;
    tmp_min = 1 << 20;

    for (i = 0; i < outprint.buffer_num; i++) {
      if (outprint.stdoutdata[i].tr < tmp_min &&
          outprint.stdoutdata[i].tr >=
              hd->last_output) {  // GB has the same "tr" with "last_output"

        pos = i;
        tmp_min = outprint.stdoutdata[i].tr;
      }
    }

    if (pos != -1) {
      hd->last_output = outprint.stdoutdata[pos].tr;
      report_frame(outprint, pos);
#if RD1501_FIX_BG
      if (outprint.stdoutdata[pos].typeb == BACKGROUND_IMG &&
          outprint.stdoutdata[pos].background_picture_output_flag == 0) {
#else
      if (outprint.stdoutdata[pos].typeb == BACKGROUND_IMG &&
          hd->background_picture_output_flag == 0) {
#endif
        write_GB_frame(hd->p_out_background);
      } else {
        write_frame(hd->p_out, outprint.stdoutdata[pos].tr);
      }
      delete_trbuffer(&outprint, pos);
    }
  }
#endif
#endif
}

/*
******************************************************************************
*  Function: Determine the MVD's value (1/4 pixel) is legal or not.
*  Input:
*  Output:
*  Return:   0: out of the legal mv range; 1: in the legal mv range
*  Attention:
*  Author: Xiaozhen ZHENG, rm52k
******************************************************************************
*/
void DecideMvRange() {

  if (input->profile_id == BASELINE_PROFILE ||
      input->profile_id == BASELINE10_PROFILE) {
    switch (input->level_id) {
      case 0x10:
        hc->Min_V_MV = -512;
        hc->Max_V_MV = 511;
        hc->Min_H_MV = -8192;
        hc->Max_H_MV = 8191;
        break;
      case 0x20:
        hc->Min_V_MV = -1024;
        hc->Max_V_MV = 1023;
        hc->Min_H_MV = -8192;
        hc->Max_H_MV = 8191;
        break;
      case 0x22:
        hc->Min_V_MV = -1024;
        hc->Max_V_MV = 1023;
        hc->Min_H_MV = -8192;
        hc->Max_H_MV = 8191;
        break;
      case 0x40:
        hc->Min_V_MV = -2048;
        hc->Max_V_MV = 2047;
        hc->Min_H_MV = -8192;
        hc->Max_H_MV = 8191;
        break;
      case 0x42:
        hc->Min_V_MV = -2048;
        hc->Max_V_MV = 2047;
        hc->Min_H_MV = -8192;
        hc->Max_H_MV = 8191;
        break;
    }
  }
}
