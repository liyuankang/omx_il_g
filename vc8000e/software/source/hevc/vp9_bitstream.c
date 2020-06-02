#include "base_type.h"
#include "sw_parameter_set.h"
#include "vp9_bitstream.h"
#include "instance.h"
#include "tools.h"
#include "enccommon.h"
#include "vp9_prob.h"
#include "vp9_subexponential.h"
#include "string.h"

/*------------------------------------------------------------------------------
  buffer_full
------------------------------------------------------------------------------*/
i32 vp9_buffer_full(struct buffer *b)
{
  if (b->size < *b->cnt + 8) return HANTRO_TRUE;

  return HANTRO_FALSE;
}

void vp9_uc_write_bit(struct buffer *b, i32 value, i32 number)
{
    i32 left;

    ASSERT((number <= 8) && (number > 0));
    ASSERT(!(value & (~0 << number)));

//#ifdef TEST_DATA
//    Enc_add_comment(b, value, number, NULL);
//#endif

    if (vp9_buffer_full(b)) return;

    b->bit_cnt += number;
    left = 32 - b->bit_cnt;
    if (left > 0)
    {
        b->cache |= value << left;
        return;
    }

    /* Flush next byte to stream */
    *b->stream++ = b->cache >> 24;
    (*b->cnt)++;
    b->cache <<= 8;
    b->cache |= value << (left + 8);
    b->bit_cnt -= 8;
}

void put_bit_vp9_32(struct buffer *b, i32 value, i32 number)
{
  i32 tmp, bits_left = 24;

  ASSERT(number <= 32);
  while (number)
  {
    if (number > bits_left)
    {
      tmp = number  - bits_left;
      vp9_uc_write_bit(b, (value >> bits_left) & 0xFF, tmp);
      number -= tmp;
    }
    bits_left -= 8;
  }
}


void vp9_write_profile(u32 profile, struct buffer* b)
{
    switch(profile){
        case VCENC_VP9_MAIN_PROFILE: vp9_uc_write_bit(b, 0, 2); break;
        case VCENC_VP9_MSRGB_PROFILE: vp9_uc_write_bit(b, 2, 2); break;
        case VCENC_VP9_HIGH_PROFILE: vp9_uc_write_bit(b, 1, 2); break;
        case VCENC_VP9_HSRGB_PROFILE: vp9_uc_write_bit(b, 6, 3); break; //If profile == 3, reserved 0 append
    }
}

void vp9_write_sync_code(struct buffer* b)
{
    vp9_uc_write_bit(b, VP9_SYNC_CODE_0, 8);
    vp9_uc_write_bit(b, VP9_SYNC_CODE_1, 8);
    vp9_uc_write_bit(b, VP9_SYNC_CODE_2, 8);
}

void vp9_write_bitdepth_colorspace_sampling(struct vcenc_instance *vcenc_instance, struct buffer *b)
{
    if(vcenc_instance->sps->general_profile_idc >= VCENC_VP9_HIGH_PROFILE){
        //Do not support 12 bit depth currently
        vp9_uc_write_bit(b, 0, 1);
    }
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.color_space, 3);
    if(vcenc_instance->vp9_inst.color_space != VP9_CS_SRGB){
        vp9_uc_write_bit(b, vcenc_instance->vp9_inst.color_range, 1);
        if(vcenc_instance->sps->general_profile_idc == VCENC_VP9_MSRGB_PROFILE || vcenc_instance->sps->general_profile_idc == VCENC_VP9_HSRGB_PROFILE){
            ASSERT(vcenc_instance->vp9_inst.subsampling_x != 1 || vcenc_instance->vp9_inst.subsampling_y != 1);
            vp9_uc_write_bit(b, vcenc_instance->vp9_inst.subsampling_x, 1);
            vp9_uc_write_bit(b, vcenc_instance->vp9_inst.subsampling_x, 1);
            vp9_uc_write_bit(b, 0, 1); //Reserved 0
        }else{
            ASSERT(vcenc_instance->vp9_inst.subsampling_x == 1 && vcenc_instance->vp9_inst.subsampling_y == 1);
        }
    }else{
        ASSERT(vcenc_instance->sps->general_profile_idc == VCENC_VP9_MSRGB_PROFILE || vcenc_instance->sps->general_profile_idc == VCENC_VP9_HSRGB_PROFILE);
        vp9_uc_write_bit(b, 0, 1); //Reserved 0
    }
}

void vp9_write_frame_size(struct vcenc_instance *vcenc_instance, struct buffer *b)
{
    //vp9_uc_write_bit(b, vcenc_instance->width - 1, 16);
    //vp9_uc_write_bit(b, vcenc_instance->height - 1, 16);
    put_bit_vp9_32(b, vcenc_instance->width - 1, 16);
    put_bit_vp9_32(b, vcenc_instance->height - 1, 16);

    //Write scaling info: currently hard coded
    //int scaling_active = vcenc_instance->width != vcenc_instance->
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.render_and_frame_size_different, 1);
    if(vcenc_instance->vp9_inst.render_and_frame_size_different){
        //vp9_uc_write_bit(b, render_w-1, 16);
        //vp9_uc_write_bit(b, render_h-1, 16);
    }
}

int vp9_get_refresh_mask(struct vcenc_instance *vcenc_instance)
{
    if(!vcenc_instance->vp9_inst.multi_arf_allowed && vcenc_instance->vp9_inst.refresh_golden_frame && !vcenc_instance->vp9_inst.use_svc)
    {
        return (vcenc_instance->vp9_inst.refresh_last_frame << vcenc_instance->vp9_inst.lst_fb_idx) | 
            (vcenc_instance->vp9_inst.refresh_golden_frame << vcenc_instance->vp9_inst.alt_fb_idx);
    }
    else
    {
        int arf_idx = vcenc_instance->vp9_inst.alt_fb_idx;
        return (vcenc_instance->vp9_inst.refresh_last_frame << vcenc_instance->vp9_inst.lst_fb_idx) |
             (vcenc_instance->vp9_inst.refresh_golden_frame << vcenc_instance->vp9_inst.gld_fb_idx) |
             (vcenc_instance->vp9_inst.refresh_alt_ref_frame << arf_idx);
    }
}

static INLINE int get_ref_frame_map_idx(vcenc_instance_vp9 vp9_inst, int8_t ref_frame)
{
    if (ref_frame == LAST_FRAME) {
        return vp9_inst.lst_fb_idx;
    } else if (ref_frame == GOLDEN_FRAME) {
        return vp9_inst.gld_fb_idx;
    } else {
        return vp9_inst.alt_fb_idx;
    }
}

void vp9_write_frame_size_with_refs(struct vcenc_instance *vcenc_instance, struct buffer *b)
{
    int found = 0;
    int8_t ref_frame;

    for(ref_frame = LAST_FRAME;ref_frame <= ALTREF_FRAME; ++ref_frame)
    {
        //Assume always found in buffer pool unless using svc
        if(vcenc_instance->vp9_inst.use_svc)
            found = 0;
        else
            found = 1;

        vp9_uc_write_bit(b, found, 1);
        if(found) break;
    }

    if(!found)
    {
        vp9_write_frame_size(vcenc_instance, b);
    }

}

void fix_interp_filter(struct vcenc_instance *vcenc_instance)
{
  if (vcenc_instance->vp9_inst.interp_filter == SWITCHABLE) {
    // Check to see if only one of the filters is actually used
    int count[SWITCHABLE_FILTERS];
    int i, j, c = 0;
    for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
      count[i] = 0;
      for (j = 0; j < VP9_SWITCHABLE_FILTER_CONTEXTS; ++j)
	  {
        //  count[i] += counts->switchable_interp[j][i];
        c += (count[i] > 0);
	  }
    }
    if (c == 1) {
      // Only one filter is used. So set the filter at frame level
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        if (count[i]) {
          vcenc_instance->vp9_inst.interp_filter = i;
          break;
        }
      }
    }
  }
}

void vp9_write_interp_filter(u8 filter, struct buffer *b)
{
    int filter_to_literal[] = {1,0,2,3};
    vp9_uc_write_bit(b, filter == SWITCHABLE, 1);
    if(filter != SWITCHABLE)
        vp9_uc_write_bit(b, filter_to_literal[filter], 2);
}

void vp9_encode_loopfilter(vp9_loopfilter * lf, struct buffer *b)
{
    int i;

    //Encode loop filter level and type
    vp9_uc_write_bit(b, lf->filter_level, 6);
    vp9_uc_write_bit(b, lf->sharpness_level, 3);

    // Write out loop filter deltas applied at the MB level based on mode or
    // ref frame (if they are enabled).
    vp9_uc_write_bit(b, lf->mode_ref_delta_enabled, 1);

    if(lf->mode_ref_delta_enabled)
    {
        vp9_uc_write_bit(b, lf->mode_ref_delta_update, 1);
        if(lf->mode_ref_delta_update)
        {
            for(i=0; i< MAX_REF_LF_DELTAS; i++)
            {
                const int delta = lf->ref_deltas[i];
                const int changed = delta != lf->last_ref_deltas[i];
                vp9_uc_write_bit(b, changed, 1);
                if(changed)
                {
                    int us_delta = (delta>0)?delta:-delta;
                    lf->last_ref_deltas[i] = delta;
                    vp9_uc_write_bit(b, us_delta & 0x3F, 6);
                    vp9_uc_write_bit(b, delta<0, 1);
                }
            }

            for(i=0;i<MAX_MODE_LF_DELTAS;i++)
            {
                const int delta = lf->mode_deltas[i];
                const int changed = delta != lf->last_mode_deltas[i];
                vp9_uc_write_bit(b, changed, 1);
                if(changed)
                {
                    int us_delta = (delta>0)?delta:-delta;
                    lf->last_mode_deltas[i] = delta;
                    vp9_uc_write_bit(b, us_delta & 0x3F, 6);
                    vp9_uc_write_bit(b, delta<0, 1);
                }
            }
        }
    }
}

void vp9_write_delta_q(struct buffer* b, int delta_q)
{
    if(delta_q != 0)
    {
        vp9_uc_write_bit(b, 1, 1);
        vp9_uc_write_bit(b, delta_q>0?delta_q:-delta_q, 4);
        vp9_uc_write_bit(b, delta_q<0, 1);
    }
    else
    {
        vp9_uc_write_bit(b, 0, 1);
    }
}

void vp9_encode_quantization(struct vcenc_instance *vcenc_instance, struct buffer * b)
{
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.base_qindex, QINDEX_BITS);
    vp9_write_delta_q(b, vcenc_instance->vp9_inst.y_dc_delta_q);
    vp9_write_delta_q(b, vcenc_instance->vp9_inst.uv_dc_delta_q);
    vp9_write_delta_q(b, vcenc_instance->vp9_inst.uv_ac_delta_q);
}

void vp9_encode_segmentation(struct vcenc_instance *vcenc_instance, struct buffer * b)
{
    ASSERT(!vcenc_instance->vp9_inst.segmentation_enable);
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.segmentation_enable, 1);
    //Currently turned off
    if(!vcenc_instance->vp9_inst.segmentation_enable)
        return;
}

static INLINE int mi_cols_aligned_to_sb(int n_mis)
{
    return ALIGN_POWER_OF_TWO(n_mis, MI_BLOCK_SIZE_LOG2);
}

static int get_min_log2_tile_cols(const int sb64_cols)
{
    int min_log2 = 0;
    while ((MAX_TILE_WIDTH_B64 << min_log2) < sb64_cols) ++min_log2;
    return min_log2;
}

static int get_max_log2_tile_cols(const int sb64_cols)
{
    int max_log2 = 1;
    while ((sb64_cols >> max_log2) >= MIN_TILE_WIDTH_B64) ++max_log2;
    return max_log2 - 1;
}


void vp9_get_tile_n_bits(int mi_cols, int * min_log2_tile_cols, int * max_log2_tile_cols)
{
    const int sb64_cols = mi_cols_aligned_to_sb(mi_cols) >> MI_BLOCK_SIZE_LOG2;
    *min_log2_tile_cols = get_min_log2_tile_cols(sb64_cols);
    *max_log2_tile_cols = get_max_log2_tile_cols(sb64_cols);
    ASSERT(*min_log2_tile_cols <= *max_log2_tile_cols);
}

void vp9_write_tile_info(struct vcenc_instance *vcenc_instance, struct buffer *b)
{
    int min_log2_tile_cols,max_log2_tile_cols,ones;
    vp9_get_tile_n_bits(vcenc_instance->vp9_inst.mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

    //columns
    ones = vcenc_instance->vp9_inst.log2_tile_cols - min_log2_tile_cols;
    while(ones--) vp9_uc_write_bit(b, 1, 1);

    if(vcenc_instance->vp9_inst.log2_tile_cols < max_log2_tile_cols)
        vp9_uc_write_bit(b, 0, 1);

    //rows
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.log2_tile_rows != 0, 1);
    if(vcenc_instance->vp9_inst.log2_tile_rows != 0)
        vp9_uc_write_bit(b, vcenc_instance->vp9_inst.log2_tile_rows != 1, 1);
}



static void vp9_write_uncompressed_header(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn, struct buffer *b, i32 trailing_bits, VCEncPictureCodingType codingType)
{
    //Frame marker: 2 bits = 0x2
    vp9_uc_write_bit(b, VP9_FRAME_MARKER, 2);

    vp9_write_profile(vcenc_instance->sps->general_profile_idc, b);

    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.show_existing_frame, 1);
	if(vcenc_instance->vp9_inst.show_existing_frame){
		vp9_uc_write_bit(b, vcenc_instance->vp9_inst.show_existing_idx, 3);
		return;
	}
    ASSERT(codingType < VCENC_NOTCODED_FRAME);

    //Frame type: 0 key frame, 1 inter frame
    int frame_type = pEncIn->bIsIDR?KEY_FRAME:INTER_FRAME;
    vp9_uc_write_bit(b, frame_type, 1);
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.show_frame, 1);
    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.error_resilient_mode, 1);

    if(frame_type == KEY_FRAME)
    {
        vp9_write_sync_code(b);
        vp9_write_bitdepth_colorspace_sampling(vcenc_instance, b);
        vp9_write_frame_size(vcenc_instance, b);
    }
    else
    {
        if(!vcenc_instance->vp9_inst.show_frame)
            vp9_uc_write_bit(b, vcenc_instance->vp9_inst.intra_only, 1);

        if(!vcenc_instance->vp9_inst.error_resilient_mode)
            vp9_uc_write_bit(b, vcenc_instance->vp9_inst.reset_frame_context, 2);

        if(vcenc_instance->vp9_inst.intra_only)
        {
            vp9_write_sync_code(b);
            //For profile 0, 420 8bpp is assumed
            if(vcenc_instance->sps->general_profile_idc > VCENC_VP9_MAIN_PROFILE)
              vp9_write_bitdepth_colorspace_sampling(vcenc_instance, b);
            
            vp9_uc_write_bit(b, vp9_get_refresh_mask(vcenc_instance), REF_FRAMES);
            vp9_write_frame_size(vcenc_instance, b);
        }
        else
        {
            int8_t ref_frame;
            vp9_uc_write_bit(b, vp9_get_refresh_mask(vcenc_instance), REF_FRAMES);
            for(ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame)
            {
                ASSERT(get_ref_frame_map_idx(vcenc_instance->vp9_inst, ref_frame) != -1);
                vp9_uc_write_bit(b, get_ref_frame_map_idx(vcenc_instance->vp9_inst, ref_frame), REF_FRAMES_LOG2);
                vp9_uc_write_bit(b, vcenc_instance->vp9_inst.ref_frame_sign_bias[ref_frame], 1);
            }
            vp9_write_frame_size_with_refs(vcenc_instance, b);

            vp9_uc_write_bit(b, vcenc_instance->vp9_inst.allow_high_precision_mv, 1);
            //Default not switchable now
            //Todo: filter switchable support
            fix_interp_filter(vcenc_instance);
            vp9_write_interp_filter(vcenc_instance->vp9_inst.interp_filter, b);
        }
    }

    if(!vcenc_instance->vp9_inst.error_resilient_mode)
    {
        vp9_uc_write_bit(b, vcenc_instance->vp9_inst.refresh_frame_context, 1);
        vp9_uc_write_bit(b, vcenc_instance->vp9_inst.frame_parallel_decoding_mode, 1);
    }

    vp9_uc_write_bit(b, vcenc_instance->vp9_inst.frame_context_idx, FRAME_CONTEXTS_LOG2);
    vp9_encode_loopfilter(&vcenc_instance->vp9_inst.lf, b);
    vp9_encode_quantization(vcenc_instance, b);
    vp9_encode_segmentation(vcenc_instance, b);

    vp9_write_tile_info(vcenc_instance, b);
    
}

void vp9_add_trailling(struct buffer *b)
{
    while (b->bit_cnt % 8)
    {
        vp9_uc_write_bit(b, 0, 1);
    }

    while (b->bit_cnt)
    {
        *b->stream++ = b->cache >> 24;
        (*b->cnt)++;
        b->cache <<= 8;
        b->bit_cnt -= 8;
    }
}


void vp9_put_bool(vp9_hbuffer * buffer, i32 prob, i32 boolValue)
{
    i32 split = 1 + (((buffer->range - 1) * prob) >> 8);
    i32 lengthBits = 0;
    i32 bits = 0;

    if (boolValue) {
        buffer->bottom += split;
        buffer->range -= split;
    } else {
        buffer->range = split;
    }

    while (buffer->range < 128) {
        /* Detect carry and add carry bit to already written
         * buffer->data if needed */
        if (buffer->bottom < 0) {
            u8 *data = buffer->buf->stream;
            while (*--data == 255) {
                *data = 0;
            }
            (*data)++;
        }
        buffer->range <<= 1;
        buffer->bottom <<= 1;

        if (!--buffer->bitsLeft) {
            lengthBits += 8;
            bits <<= 8;
            bits |= (buffer->bottom >> 24) & 0xff;
            *buffer->buf->stream++ = (buffer->bottom >> 24) & 0xff;
            buffer->buf->byteCnt++;
            buffer->bottom &= 0xffffff;     /* Keep 3 bytes */
            buffer->bitsLeft = 8;
            /* TODO use big enough buffer and check buffer status
             * for example in the beginning of mb row */
        }
    }
}

void vp9_put_bool_128(vp9_hbuffer *buffer, i32 boolValue)
{
    i32 split = 1 + ((buffer->range - 1) >> 1);
    i32 lengthBits = 0;
    i32 bits = 0;

    if (boolValue) {
        buffer->bottom += split;
        buffer->range -= split;
    } else {
        buffer->range = split;
    }

    while (buffer->range < 128) {
        /* Detect carry and add carry bit to already written
         * buffer->data if needed */
        if (buffer->bottom < 0) {
            u8 *data = buffer->buf->stream;
            while (*--data == 255) {
                *data = 0;
            }
            (*data)++;
        }
        buffer->range <<= 1;
        buffer->bottom <<= 1;

        if (!--buffer->bitsLeft) {
            lengthBits += 8;
            bits <<= 8;
            bits |= (buffer->bottom >> 24) & 0xff;
            *buffer->buf->stream++ = (buffer->bottom >> 24) & 0xff;
            buffer->buf->byteCnt++;
            buffer->bottom &= 0xffffff;     /* Keep 3 bytes */
            buffer->bitsLeft = 8;
            /* TODO use big enough buffer and check buffer status
             * for example in the beginning of mb row */
        }
    }
}

void vp9_put_lit(vp9_hbuffer *buffer, i32 value, i32 number)
{
    ASSERT(number < 32 && number > 0);
    ASSERT(((value & (-1 << number)) == 0));

     while (number--) {
        vp9_put_bool_128(buffer, (value >> number) & 0x1);
    }
}

void vp9_update_txProb(struct vcenc_instance *vcenc_instance)
{
    u32 i,j;

    //8x8
    for (i = 0; i < VP9_TX_SIZE_CONTEXTS; i++) {
        for (j = 0; j <  TX_SIZE_MAX - 3; j++)
            vp9_put_bool(&vcenc_instance->vp9_inst.hd_buffer, VP9_DEF_UPDATE_PROB, 0);
    }
    // 16x16
    for (i = 0; i < VP9_TX_SIZE_CONTEXTS; i++) {
        for (j = 0; j <  TX_SIZE_MAX - 2; j++)
            vp9_put_bool(&vcenc_instance->vp9_inst.hd_buffer, VP9_DEF_UPDATE_PROB, 0);
    }
    // 32x32
    for (i = 0; i < VP9_TX_SIZE_CONTEXTS; i++) {
        for (j = 0; j <  TX_SIZE_MAX - 1; j++)
            vp9_put_bool(&vcenc_instance->vp9_inst.hd_buffer, VP9_DEF_UPDATE_PROB, 0);
    }
}

int vp9_memcmp(const void *s1, const void *s2, u32 n) {
    return memcmp(s1, s2, (unsigned int) n);
}


void vp9_update_prob(vp9_hbuffer *b, u8 update, u8 old)
{
    i32 value, number;

    if (old == update) {
        vp9_put_bool(b, VP9_DEF_UPDATE_PROB, 0x0);
        return;
    }
    //DEBUG_PRINT( "\nupdate_prob %i", old != update);
    vp9_put_bool(b, VP9_DEF_UPDATE_PROB, 0x1);

    //DEBUG_PRINT( "\nsub-exponential code");
    get_se(update, old, &value, &number);
    vp9_put_lit(b, value, number);
}

void vp9_coeff_bands(vp9_hbuffer *b,
    u8 update[COEF_BANDS][PREV_COEF_CONTEXTS_PAD][ENTROPY_NODES_PART1],
    u8 old[COEF_BANDS][PREV_COEF_CONTEXTS_PAD][ENTROPY_NODES_PART1])
{
    i32 k, l, m;

    for (k = 0; k < COEF_BANDS; k++) {
        for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
            if (k > 0 || l < 3) {
                for (m = 0; m < UNCONSTRAINED_NODES; m++) {
                    vp9_update_prob(b, update[k][l][m],
                                  old[k][l][m]);
                }
            }
        }
    }
}

void vp9_update_coeff_prob(vp9_hbuffer *b,
             u8 update[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD][ENTROPY_NODES_PART1],
             u8 old[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD][ENTROPY_NODES_PART1]) 
{
  i32 j, k, cmp;

  cmp = vp9_memcmp(old, update, sizeof(u8[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD][ENTROPY_NODES_PART1]));

  vp9_put_lit(b, cmp ? 0x1 : 0x0, 1);
  if (!cmp) return;
  for (j = 0; j < BLOCK_TYPES; j++) {
      for (k = 0; k < REF_TYPES; k++) {
          vp9_coeff_bands(b, update[j][k], old[j][k]);
      }
  }
}

void vp9_update_coeff_probs(struct vcenc_instance* vcenc_instance)
{
    vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
    struct Vp9EntropyProbs *old, *update;
    i32 max_tx_size;
    
    old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
    update = &vcenc_instance->vp9_inst.entropy;

    vp9_update_coeff_prob(b, update->a.prob_coeffs, old->a.prob_coeffs);
    max_tx_size = vcenc_instance->vp9_inst.tx_mode;

    if(max_tx_size > TX_4X4)
        vp9_update_coeff_prob(b, update->a.prob_coeffs8x8, old->a.prob_coeffs8x8);
    if(max_tx_size > TX_8X8)
        vp9_update_coeff_prob(b, update->a.prob_coeffs16x16, old->a.prob_coeffs16x16);
    if(max_tx_size > TX_16X16)
        vp9_update_coeff_prob(b, update->a.prob_coeffs32x32, old->a.prob_coeffs32x32);
}

void vp9_update_mbskip_probs(struct vcenc_instance* vcenc_instance)
{
    vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
    struct Vp9EntropyProbs *old, *update;
    i32 i;

    old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
    update = &vcenc_instance->vp9_inst.entropy;

    for (i = 0; i < MBSKIP_CONTEXTS; i++) {
        vp9_update_prob(b, update->a.mbskip_probs[i], old->a.mbskip_probs[i]);
    }
}

void vp9_flush_buffer(vp9_hbuffer *buffer)
{
    vp9_put_lit(buffer, 0x0, 16);
    vp9_put_lit(buffer, 0x0, 16);

    /* Be sure that last byte is not superframe index */
    if ((buffer->buf->stream[-1] & 0xe0) == 0xc0) {
        *buffer->buf->stream = 0;
        buffer->buf->byteCnt++;
    }
}

void vp9_update_intermode_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i, j;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  for(i = 0; i < VP9_INTER_MODE_CONTEXTS; i++){
    for(j = 0; j < VP9_INTER_MODES - 1; j++){
	  vp9_update_prob(b, update->a.inter_mode_prob[i][j], old->a.inter_mode_prob[i][j]);
    }
  }
}

void vp9_update_intrainter_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  for(i = 0; i < INTRA_INTER_CONTEXTS; i++){
    vp9_update_prob(b, update->a.intra_inter_prob[i], old->a.intra_inter_prob[i]);
  }
}

void vp9_update_comppred_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  //If compound pred support
  if(vcenc_instance->vp9_inst.allow_comp_inter_inter){
    //ASSERT(vcenc_instance->vp9_inst.allow_comp_inter_inter == 0);
	const i32 use_compound_pred = 1;
	vp9_put_lit(b, use_compound_pred, 1);
    if(use_compound_pred){
	  //const i32 use_hybrid_pred = vcenc_instance->vp9_inst.sw_hybrid_pred_enable?1:0;
	  const i32 use_hybrid_pred = 0;
	  vp9_put_lit(b, use_hybrid_pred, 1);
	  if(use_hybrid_pred){
	    for(i = 0; i < COMP_INTER_CONTEXTS; i++){
		  vp9_update_prob(b, update->a.comp_inter_prob[i], old->a.comp_inter_prob[i]);
	    }
	  }
    }
  }
  
}

void vp9_update_singleref_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  if(!vcenc_instance->vp9_inst.allow_comp_inter_inter){// || sw_hybrid_pred_enable
    for(i = 0; i< VP9_REF_CONTEXTS; i++){
	  vp9_update_prob(b, update->a.single_ref_prob[i][0], old->a.single_ref_prob[i][0]);
	  vp9_update_prob(b, update->a.single_ref_prob[i][1], old->a.single_ref_prob[i][1]);
    }
  }
}

void vp9_update_compref_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  if(vcenc_instance->vp9_inst.allow_comp_inter_inter){
    for(i = 0; i < VP9_REF_CONTEXTS; i++)
	  vp9_update_prob(b, update->a.comp_ref_prob[i], old->a.comp_ref_prob[i]);
  }
}

void vp9_update_sb_ymode_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i, j;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  for(i = 0; i < BLOCK_SIZE_GROUPS; i++){
    for(j = 0; j < 8; j++){
	  vp9_update_prob(b, update->a.sb_ymode_prob[i][j], old->a.sb_ymode_prob[i][j]);
    }
	vp9_update_prob(b, update->a.sb_ymode_prob_b[i][0], old->a.sb_ymode_prob_b[i][0]);
  }
}

void vp9_update_partition_prob(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct Vp9EntropyProbs *old, *update;
  i32 i, j;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  update = &vcenc_instance->vp9_inst.entropy;

  for(i = 0; i < NUM_PARTITION_CONTEXTS; i++){
    for(j = 0; j < PARTITION_TYPES; j++){
	  vp9_update_prob(b, update->a.partition_prob[INTER_FRAME][i][j], old->a.partition_prob[INTER_FRAME][i][j]);
    }
  }
}

void vp9_update_mv_prob(vp9_hbuffer *b, u8 update, u8 old){
  //ASSERT(update & 1); seems to be wrong assert from BIGSEA

  if(old == update){
    vp9_put_bool(b, VP9_NMV_UPDATE_PROB, 0x0);
	return;
  }

  vp9_put_bool(b, VP9_NMV_UPDATE_PROB, 0x1);
  vp9_put_lit(b, update >> 1, 7);
}

void vp9_update_mv_probs(struct vcenc_instance *vcenc_instance){
  vp9_hbuffer *b = &vcenc_instance->vp9_inst.hd_buffer;
  struct NmvContext *old, *update;
  i32 i, j, k;

  old = &vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx].a.nmvc;
  update = &vcenc_instance->vp9_inst.entropy.a.nmvc;

  for(i = 0; i < MV_JOINTS - 1; i++){
    vp9_update_mv_prob(b, update->joints[i], old->joints[i]);
  }

  for (i = 0; i < 2; i++) {
    vp9_update_mv_prob(b, update->sign[i], old->sign[i]);
    for (j = 0; j < MV_CLASSES - 1; j++)
      vp9_update_mv_prob(b, update->classes[i][j], old->classes[i][j]);
    for (j = 0; j < CLASS0_SIZE - 1; j++)
      vp9_update_mv_prob(b, update->class0[i][j], old->class0[i][j]);
    for (j = 0; j < MV_OFFSET_BITS; j++)
      vp9_update_mv_prob(b, update->bits[i][j], old->bits[i][j]);
  }

  for (i = 0; i < 2; i++) {
    for (j = 0; j < CLASS0_SIZE; j++) {
      for (k = 0; k < 3; k++)
        vp9_update_mv_prob(b, update->class0_fp[i][j][k], old->class0_fp[i][j][k]);
    }
    for (j = 0; j < 3; j++)
      vp9_update_mv_prob(b, update->fp[i][j], old->fp[i][j]);
  }
      
  if (vcenc_instance->vp9_inst.allow_high_precision_mv) {
    for (i = 0; i < 2; i++) {
      vp9_update_mv_prob(b, update->class0_hp[i], old->class0_hp[i]);
      vp9_update_mv_prob(b, update->hp[i], old->hp[i]);
    }
  }
}

i32 vp9_write_compressed_header(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn, struct buffer *b, i32 trailing_bits, VCEncPictureCodingType codingType)
{

    vp9_hbuffer *buffer = &vcenc_instance->vp9_inst.hd_buffer;
    u32 savedByte = buffer->buf->byteCnt;

    vp9_put_lit(buffer, 0x0, 1); //Marker bit

    if(vcenc_instance->vp9_inst.lossless)
        vcenc_instance->vp9_inst.tx_mode = VP9_ONLY_4X4;
    else
    {
        if(vcenc_instance->vp9_inst.tx_mode < ALLOW_32X32)
            vp9_put_lit(buffer, vcenc_instance->vp9_inst.tx_mode, 2);
        else
        {
            vp9_put_lit(buffer, vcenc_instance->vp9_inst.tx_mode == ALLOW_32X32?0x6: 0x7, 3);
            if(vcenc_instance->vp9_inst.tx_mode == VP9_TX_MODE_SELECT)
                vp9_update_txProb(vcenc_instance);
        }
    }

    //Update coefficients
    vp9_update_coeff_probs(vcenc_instance);
    vp9_update_mbskip_probs(vcenc_instance);

    if(!vcenc_instance->vp9_inst.intra_only)
    {
        //Update Inter and other probs
        //ASSERT(vcenc_instance->vp9_inst.intra_only);
        vp9_update_intermode_prob(vcenc_instance);

		/*Currently not support switchable filter
		if(!reallocate && filter_type == SWITCHABLE)
			vp9_update_switchable_interp_prob();
			*/

		vp9_update_intrainter_prob(vcenc_instance);
		vp9_update_comppred_prob(vcenc_instance);
		vp9_update_singleref_prob(vcenc_instance);
		vp9_update_compref_prob(vcenc_instance);

		vp9_update_sb_ymode_prob(vcenc_instance);
		vp9_update_partition_prob(vcenc_instance);
		vp9_update_mv_probs(vcenc_instance);
    }

    vp9_flush_buffer(buffer);
    return buffer->buf->byteCnt - savedByte;
}


i32 VCEncFrameHeaderVP9(VCEncInst inst, const VCEncIn *pEncIn, u32 *pStrmLen, VCEncPictureCodingType codingType)
{
    u32 data_size;
    u32 saved_byte, saved_bit;
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    int i;
    i32 compressed_size;

    struct buffer p, *b;
    p.bit_cnt = 0;
    p.byteCnt = 0;
    p.cache = 0;
    p.cnt = &p.byteCnt;
    p.stream = vcenc_instance->stream.stream + (*pStrmLen);
    p.size = vcenc_instance->stream.size;
    b = &p;

    //VCEncVP9SetBuffer(vcenc_instance, &p);
    vcenc_instance->vp9_inst.hd_buffer.buf = &p;

    vcenc_instance->vp9_inst.hd_buffer.range = 255;
    vcenc_instance->vp9_inst.hd_buffer.bottom = 0;
    vcenc_instance->vp9_inst.hd_buffer.bitsLeft = 24;

    vp9_write_uncompressed_header(vcenc_instance,pEncIn,b,1,codingType);
    //Save current position
    saved_byte = b->byteCnt + b->bit_cnt/8;
    saved_bit = b->bit_cnt % 8;
    
    put_bit_vp9_32(b, 0, 16);//Reserve position for compressed header size in bytes
    vp9_add_trailling(b);


    //Compressed header
    compressed_size = vp9_write_compressed_header(vcenc_instance,pEncIn,b,1,codingType);

    //Write back compressed header size
    int bits = 16;
    u32 tmp_sb = saved_bit;
    i = saved_byte;
    while(bits)
    {
        u32 partial_bits = 8-tmp_sb;
        if(bits < partial_bits)
        {
            *(vcenc_instance->stream.stream+ i +(*pStrmLen)) |= (compressed_size << (8 - bits)) & 0xFF;
            break;
        }
        *(vcenc_instance->stream.stream+i + (*pStrmLen)) |= (compressed_size >> (bits - partial_bits)) & ((1 << partial_bits) -1);
        bits -= partial_bits;
        tmp_sb = 0;
        i++;
    }

    *pStrmLen += b->byteCnt;
    return VCENC_OK;
}
