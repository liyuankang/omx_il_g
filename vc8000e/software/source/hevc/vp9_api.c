#include "vp9_api.h"
#include "av1enccommon.h"
#include "vp9_enums.h"
#include "vp9_default_probs.h"
#include "string.h"

void *CWLmemcpy(void *d, const void *s, u32 n) {
  return memcpy(d, s, (size_t) n);
}


static const u8 vp9_default_inter_mode_prob[VP9_INTER_MODE_CONTEXTS][4] = {
    {2, 173, 34, 0},  // 0 = both zero mv
    {7, 145, 85, 0},  // 1 = one zero mv + one a predicted mv
    {7, 166, 63, 0},  // 2 = two predicted mvs
    {7, 94, 66, 0},   // 3 = one predicted/zero and one new mv
    {8, 64, 46, 0},   // 4 = two new mvs
    {17, 81, 31, 0},  // 5 = one intra neighbour + x
    {25, 29, 30, 0},  // 6 = two intra neighbours
};

static const vp9_prob default_if_y_probs[BLOCK_SIZE_GROUPS][VP9_INTRA_MODES - 1] = {
  {  65,  32,  18, 144, 162, 194,  41,  51,  98 },  // block_size < 8x8
  { 132,  68,  18, 165, 217, 196,  45,  40,  78 },  // block_size < 16x16
  { 173,  80,  19, 176, 240, 193,  64,  35,  46 },  // block_size < 32x32
  { 221, 135,  38, 194, 248, 121,  96,  85,  29 }   // block_size >= 32x32
};

static const vp9_prob default_if_uv_probs[VP9_INTRA_MODES][VP9_INTRA_MODES - 1] = {
  { 120,   7,  76, 176, 208, 126,  28,  54, 103 },  // y = dc
  {  48,  12, 154, 155, 139,  90,  34, 117, 119 },  // y = v
  {  67,   6,  25, 204, 243, 158,  13,  21,  96 },  // y = h
  {  97,   5,  44, 131, 176, 139,  48,  68,  97 },  // y = d45
  {  83,   5,  42, 156, 111, 152,  26,  49, 152 },  // y = d135
  {  80,   5,  58, 178,  74,  83,  33,  62, 145 },  // y = d117
  {  86,   5,  32, 154, 192, 168,  14,  22, 163 },  // y = d153
  {  85,   5,  32, 156, 216, 148,  19,  29,  73 },  // y = d207
  {  77,   7,  64, 116, 132, 122,  37, 126, 120 },  // y = d63
  { 101,  21, 107, 181, 192, 103,  19,  67, 125 }   // y = tm
};

static const vp9_prob
    default_kf_uv_probs[VP9_INTRA_MODES][VP9_INTRA_MODES - 1] = {
        {144, 11, 54, 157, 195, 130, 46, 58, 108} /* y = dc */,
        {118, 15, 123, 148, 131, 101, 44, 93, 131} /* y = v */,
        {113, 12, 23, 188, 226, 142, 26, 32, 125} /* y = h */,
        {120, 11, 50, 123, 163, 135, 64, 77, 103} /* y = d45 */,
        {113, 9, 36, 155, 111, 157, 32, 44, 161} /* y = d135 */,
        {116, 9, 55, 176, 76, 96, 37, 61, 149} /* y = d117 */,
        {115, 9, 28, 141, 161, 167, 21, 25, 193} /* y = d153 */,
        {120, 12, 32, 145, 195, 142, 32, 38, 86} /* y = d27 */,
        {116, 12, 64, 120, 140, 125, 49, 115, 121} /* y = d63 */,
        {102, 19, 66, 162, 182, 122, 35, 59, 128} /* y = tm */
};


/*------------------------------------------------------------------------------
  log2i
------------------------------------------------------------------------------*/
static i32 log2i(i32 x, i32 *result)
{
  i32 tmp = 0;

  if (x < 0) return NOK;

  while (x >> ++tmp);

  *result = tmp - 1;

  return (x == 1 << (tmp - 1)) ? OK : NOK;
}

static i32 tile_log2(i32 blk_size, i32 target) {
    i32 k;
    for (k = 0; (blk_size << k) < target; k++) {
    }
    return k;
}


void VCEncInitVP9(const VCEncConfig *config, VCEncInst inst)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    int i;
    vcenc_instance->vp9_inst.show_existing_frame = 0;
    vcenc_instance->vp9_inst.frame_to_show_map_idx = 0;
    vcenc_instance->vp9_inst.display_frame_id = 0;
    vcenc_instance->vp9_inst.show_frame = 1;
    vcenc_instance->vp9_inst.error_resilient_mode = 0;
    vcenc_instance->vp9_inst.render_and_frame_size_different = 0;

    vcenc_instance->vp9_inst.color_space = VP9_CS_BT_709;
    vcenc_instance->vp9_inst.color_range = VP9_CR_STUDIO_RANGE;

	//TODO: support other sampling
    vcenc_instance->vp9_inst.subsampling_x = 1;
    vcenc_instance->vp9_inst.subsampling_y = 1;

    vcenc_instance->vp9_inst.use_svc = 0;

    vcenc_instance->vp9_inst.intra_only = 0;
    // Flag signaling that the frame context should be reset to default values.
    // 0 or 1 implies don't reset, 2 reset just the context specified in the
    // frame header, 3 reset all contexts.
    vcenc_instance->vp9_inst.reset_frame_context = 0;

    for(i=0;i<MAX_REF_FRAMES;i++)
        vcenc_instance->vp9_inst.ref_frame_sign_bias[i] = 0;

    vcenc_instance->vp9_inst.lst_fb_idx = 0;
    vcenc_instance->vp9_inst.gld_fb_idx = 0;
    vcenc_instance->vp9_inst.alt_fb_idx = 0;

    vcenc_instance->vp9_inst.refresh_last_frame = 0;
    vcenc_instance->vp9_inst.refresh_golden_frame = 0;
    vcenc_instance->vp9_inst.refresh_alt_ref_frame = 0;
    vcenc_instance->vp9_inst.frame_context_idx = 0;

    vcenc_instance->vp9_inst.refresh_frame_context = 0;
    vcenc_instance->vp9_inst.frame_parallel_decoding_mode = 0;

    vcenc_instance->vp9_inst.interp_filter = 0;

    vcenc_instance->vp9_inst.multi_arf_allowed = 0;
    vcenc_instance->vp9_inst.allow_high_precision_mv = 0;

    //Loop filter init
    vcenc_instance->vp9_inst.lf.filter_level = 0;
    vcenc_instance->vp9_inst.lf.last_filter_level = 0;
    vcenc_instance->vp9_inst.lf.sharpness_level = 0;
    vcenc_instance->vp9_inst.lf.last_sharpness_level = 0;
    vcenc_instance->vp9_inst.lf.mode_ref_delta_enabled = 0;
    vcenc_instance->vp9_inst.lf.mode_ref_delta_update = 0;

    //Quantization info
    vcenc_instance->vp9_inst.base_qindex = 0;
    vcenc_instance->vp9_inst.y_dc_delta_q = 0;
    vcenc_instance->vp9_inst.uv_dc_delta_q = 0;
    vcenc_instance->vp9_inst.uv_ac_delta_q = 0;

    vcenc_instance->vp9_inst.segmentation_enable = 0;

    //Tile info
    // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
    // MODE_INFO (8-pixel) units.
    
    vcenc_instance->vp9_inst.mi_rows = ALIGN_POWER_OF_TWO(vcenc_instance->height, 3) >> VP9_MI_SIZE_LOG2;
    vcenc_instance->vp9_inst.mi_cols = ALIGN_POWER_OF_TWO(vcenc_instance->width, 3) >> VP9_MI_SIZE_LOG2;
    vcenc_instance->vp9_inst.mb_rows = (vcenc_instance->vp9_inst.mi_rows + 1) >> 1;
    vcenc_instance->vp9_inst.mb_cols = (vcenc_instance->vp9_inst.mi_cols + 1) >> 1;
    vcenc_instance->vp9_inst.MBs = vcenc_instance->vp9_inst.mb_cols * vcenc_instance->vp9_inst.mb_rows;
    vcenc_instance->vp9_inst.mi_stride = vcenc_instance->vp9_inst.mi_cols + MI_BLOCK_SIZE;


    vcenc_instance->vp9_inst.tile_cols = 1; // log2 number of tile columns
    vcenc_instance->vp9_inst.tile_rows = 1; // log2 number of tile rows

    i32 mi_cols = ALIGN_POWER_OF_TWO(vcenc_instance->vp9_inst.mi_cols, vcenc_instance->sps->mib_size_log2);
    i32 mi_rows = ALIGN_POWER_OF_TWO(vcenc_instance->vp9_inst.mi_rows, vcenc_instance->sps->mib_size_log2);
    i32 sb_cols = mi_cols >> vcenc_instance->sps->mib_size_log2;
    i32 sb_rows = mi_rows >> vcenc_instance->sps->mib_size_log2;

    i32 sb_size_log2 = vcenc_instance->sps->mib_size_log2 + VP9_MI_SIZE_LOG2;
    vcenc_instance->vp9_inst.max_tile_width_sb = VP9_MAX_TILE_WIDTH >> sb_size_log2;
    i32 max_tile_area_sb = VP9_MAX_TILE_AREA >> (2 * sb_size_log2);

    vcenc_instance->vp9_inst.min_log2_tile_cols = tile_log2(vcenc_instance->vp9_inst.max_tile_width_sb, sb_cols);
    vcenc_instance->vp9_inst.max_log2_tile_cols = tile_log2(1, MIN(sb_cols, SW_MAX_TILE_COLS));
    vcenc_instance->vp9_inst.max_log2_tile_rows = tile_log2(1, MIN(sb_rows, SW_MAX_TILE_ROWS));
    vcenc_instance->vp9_inst.min_log2_tiles = tile_log2(max_tile_area_sb, sb_cols * sb_rows);
    vcenc_instance->vp9_inst.min_log2_tiles = MAX(vcenc_instance->vp9_inst.min_log2_tiles, vcenc_instance->vp9_inst.min_log2_tile_cols);

    log2i(vcenc_instance->vp9_inst.tile_cols, (i32*)&vcenc_instance->vp9_inst.log2_tile_cols);
    vcenc_instance->vp9_inst.log2_tile_cols = MAX(vcenc_instance->vp9_inst.log2_tile_cols, vcenc_instance->vp9_inst.min_log2_tile_cols);
    vcenc_instance->vp9_inst.log2_tile_cols = MIN(vcenc_instance->vp9_inst.log2_tile_cols, vcenc_instance->vp9_inst.max_log2_tile_cols);

    log2i(vcenc_instance->vp9_inst.tile_rows, (i32*)&vcenc_instance->vp9_inst.log2_tile_rows);
    vcenc_instance->vp9_inst.log2_tile_rows = MAX(vcenc_instance->vp9_inst.log2_tile_rows, vcenc_instance->vp9_inst.min_log2_tile_rows);
    vcenc_instance->vp9_inst.log2_tile_rows = MIN(vcenc_instance->vp9_inst.log2_tile_rows, vcenc_instance->vp9_inst.max_log2_tile_rows);

    vcenc_instance->vp9_inst.allow_comp_inter_inter = 0;

	vcenc_instance->vp9_inst.is_RefIdx_inited = ENCHW_NO;
}

void vp9_find_pic_to_display(VCEncInst inst, const VCEncIn *pEncIn) {
    i32 i;
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    vcenc_instance->vp9_inst.show_existing_frame   = ENCHW_NO;

    for (i = 0; i < 8; i++)
    {
        if (vcenc_instance->vp9_inst.ref_frame_map[i].showable_frame == ENCHW_NO)
            continue;
        if (vcenc_instance->vp9_inst.ref_frame_map[i].poc == vcenc_instance->vp9_inst.POCtobeDisplayVP9)
        {
            vcenc_instance->vp9_inst.show_existing_frame   = ENCHW_YES;
            vcenc_instance->vp9_inst.frame_to_show_map_idx = i;
            vcenc_instance->vp9_inst.ref_frame_map[i].showable_frame = ENCHW_NO;

            vcenc_instance->vp9_inst.POCtobeDisplayVP9 = (vcenc_instance->vp9_inst.POCtobeDisplayVP9 + 1) % vcenc_instance->vp9_inst.MaxPocDisplayVP9;
            if(pEncIn->bIsIDR)
                vcenc_instance->vp9_inst.POCtobeDisplayVP9 = 0;
            return;
        }
    }
}

i32 vp9_poc_idx(const i32 *pPocList, i32 gopSize, i32 looked_poc){
    for(i32 i = 0; i < gopSize; i++){
        if(pPocList[i] == looked_poc)
            return i;
    }
    return 0;
}

// Define the reference buffers that will be updated post encode.
void vp9_configure_buffer_updates(VCEncInst inst, VCEncIn *pEncIn) {
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    VP9_FRAME_UPDATE_TYPE updateType = pEncIn->bIsIDR ? VP9_KF_UPDATE : vcenc_instance->vp9_inst.upDateRefFrame[pEncIn->gopSize -1][pEncIn->gopPicIdx];

    vcenc_instance->vp9_inst.refresh_last_frame     = 0;
    vcenc_instance->vp9_inst.refresh_golden_frame   = 0;
    vcenc_instance->vp9_inst.refresh_alt_ref_frame  = 0;


    if(pEncIn->u8IdxEncodedAsLTR){
		vcenc_instance->vp9_inst.refresh_golden_frame = 1;
    }

	switch (updateType) {
    case VP9_KF_UPDATE:
      vcenc_instance->vp9_inst.refresh_last_frame = 1;
      vcenc_instance->vp9_inst.refresh_golden_frame = 1;
      vcenc_instance->vp9_inst.refresh_alt_ref_frame = 1;
      break;
    case VP9_LF_UPDATE:
      vcenc_instance->vp9_inst.refresh_last_frame = 1;
      vcenc_instance->vp9_inst.refresh_golden_frame = 0;
      vcenc_instance->vp9_inst.refresh_alt_ref_frame = 0;
      break;
    case VP9_GF_UPDATE:
      vcenc_instance->vp9_inst.refresh_last_frame = 1;
      vcenc_instance->vp9_inst.refresh_golden_frame = 1;
      vcenc_instance->vp9_inst.refresh_alt_ref_frame = 0;
      break;
    case VP9_OVERLAY_UPDATE:
      vcenc_instance->vp9_inst.refresh_last_frame = 0;
      vcenc_instance->vp9_inst.refresh_golden_frame = 1;
      vcenc_instance->vp9_inst.refresh_alt_ref_frame = 0;
      //vcenc_instance->vp9_inst.rc.is_src_frame_alt_ref = 1;
      break;
    case VP9_ARF_UPDATE:
      vcenc_instance->vp9_inst.refresh_last_frame = 0;
      vcenc_instance->vp9_inst.refresh_golden_frame = 0;
      vcenc_instance->vp9_inst.refresh_alt_ref_frame = 1;
      break;
    default: ASSERT(0); break;
  }
}


void vp9_ref_idx(const VCEncIn *pEncIn, VCEncInst inst){
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    
    if(vcenc_instance->vp9_inst.is_RefIdx_inited == ENCHW_YES)
        return;
    
    true_e useing_ref[MAX_GOP_SIZE];
    true_e poc_skip[MAX_GOP_SIZE];
    i32 gopSize, descendNum, index, curr_id, ref_poc;
    i32 poc[MAX_GOP_SIZE];

    for(i32 gopSize=1; gopSize <= MAX_GOP_SIZE ; gopSize++){
        for(index = 0; index < gopSize; index++){
            curr_id           = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
            useing_ref[index] = ENCHW_NO;
            poc_skip[index]   = ENCHW_NO;
            poc[index]        = pEncIn->gopConfig.pGopPicCfg[curr_id].poc;
        }

        descendNum = 0;
        for(index = 1; index < gopSize; index++){
            if(poc[index] < poc[index-1])
                descendNum = index;
            else
                break;
        }

        for(index = descendNum; index < (gopSize-1); index++){
            if((poc[index]+1) != poc[index+1])
                poc_skip[index]   = ENCHW_YES;
        }

        if(poc[gopSize-1] < poc[0])
            poc_skip[gopSize-1] = ENCHW_YES;
        
        for(index = 0; index < gopSize; index++){
            curr_id = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
            for(i32 i = 0; i < pEncIn->gopConfig.pGopPicCfg[curr_id].numRefPics; i++)
            {
                ref_poc = pEncIn->gopConfig.pGopPicCfg[curr_id].refPics[i].ref_pic;
                if(!IS_LONG_TERM_REF_DELTAPOC(ref_poc)){
                    ref_poc += poc[index];
                    if(ref_poc == 0)
                        ref_poc = gopSize;
                    if((ref_poc > 0) && (ref_poc <= gopSize))
                        useing_ref[vp9_poc_idx(poc, gopSize, ref_poc)] = ENCHW_YES;
                }
            }
        }

		//for(index = 0; index < gopSize; index++){
			//vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = VP9_LF_UPDATE;
		//}

        /*
        if(descendNum == 3){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = ARF_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][1] = ARF2_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][2] = BRF_UPDATE;
        }
        else if(descendNum == 2){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = ARF_UPDATE;
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][1] = BRF_UPDATE;
        }
        if(descendNum == 1){
            vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][0] = BRF_UPDATE;
        }

        for(index = descendNum; index < gopSize; index++){
            if(useing_ref[index]){
                if(poc_skip[index])
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = BRF_UPDATE;
                else
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = LF_UPDATE;
            }
            else{
                if(poc_skip[index]){
                    curr_id = index + pEncIn->gopConfig.gopCfgOffset[gopSize];
                    ref_poc = poc[index]+1;
                    if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][vp9_poc_idx(poc, gopSize, ref_poc)] == ARF_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = ARF_TO_LAST_UPDATE;
                    else if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][vp9_poc_idx(poc, gopSize, ref_poc)] == ARF2_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = ARF2_TO_LAST_UPDATE;
                    else if(vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][vp9_poc_idx(poc, gopSize, ref_poc)] == BRF_UPDATE)
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = BWD_TO_LAST_UPDATE;
                    else
                        vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = COMMON_NO_UPDATE;
                }
                else
                    vcenc_instance->av1_inst.upDateRefFrame[gopSize-1][index] = COMMON_NO_UPDATE;
            }
        }*/
    }

    vcenc_instance->vp9_inst.is_RefIdx_inited = ENCHW_YES;
}

void vp9_setup_compound_reference(struct vcenc_instance *vcenc_instance){
  if(vcenc_instance->vp9_inst.ref_frame_sign_bias[LAST_FRAME] == vcenc_instance->vp9_inst.ref_frame_sign_bias[GOLDEN_FRAME]){
    vcenc_instance->vp9_inst.comp_fixed_ref = ALTREF_FRAME;
	vcenc_instance->vp9_inst.comp_var_ref[0] = LAST_FRAME;
    vcenc_instance->vp9_inst.comp_var_ref[1] = GOLDEN_FRAME;
  }else if(vcenc_instance->vp9_inst.ref_frame_sign_bias[LAST_FRAME] == vcenc_instance->vp9_inst.ref_frame_sign_bias[ALTREF_FRAME]){
    vcenc_instance->vp9_inst.comp_fixed_ref = GOLDEN_FRAME;
	vcenc_instance->vp9_inst.comp_var_ref[0] = LAST_FRAME;
    vcenc_instance->vp9_inst.comp_var_ref[1] = ALTREF_FRAME;
  }else{
    vcenc_instance->vp9_inst.comp_fixed_ref = LAST_FRAME;
	vcenc_instance->vp9_inst.comp_var_ref[0] = GOLDEN_FRAME;
    vcenc_instance->vp9_inst.comp_var_ref[1] = ALTREF_FRAME;
  }
}

void VCEncGenVP9Config(VCEncInst inst, const VCEncIn *pEncIn, struct sw_picture *pic, VCEncPictureCodingType codingType)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
	
  if(codingType < VCENC_BIDIR_PREDICTED_FRAME){
    vcenc_instance->vp9_inst.show_frame = 1;
  }

  vcenc_instance->vp9_inst.show_existing_frame = 0;
    
  if(pEncIn->bIsIDR)
  {
    vcenc_instance->vp9_inst.key_frame = 1;
    vcenc_instance->vp9_inst.intra_only = 1;
  }else if(codingType == VCENC_INTRA_FRAME)
  {
    vcenc_instance->vp9_inst.key_frame = 0;
    vcenc_instance->vp9_inst.intra_only = 1;
  }else
  {
    vcenc_instance->vp9_inst.key_frame = 0;
    vcenc_instance->vp9_inst.intra_only = 0;
  }
  
  if(codingType == VCENC_INTRA_FRAME || vcenc_instance->vp9_inst.error_resilient_mode){
    vcenc_instance->vp9_inst.frame_context_idx = 0;
  }
  
  
  if(pEncIn->bIsIDR){
    if((pEncIn->gopConfig.idr_interval == 0) || (pEncIn->bIsPeriodUsingLTR == HANTRO_TRUE) || (pEncIn->gopConfig.idr_interval > ( (1<< 8)>>1))){
      vcenc_instance->sps->frame_id_numbers_present_flag = ENCHW_YES;
      vcenc_instance->sps->enable_order_hint             = ENCHW_NO;
      
      vcenc_instance->vp9_inst.MaxPocDisplayVP9  = pEncIn->gopConfig.idr_interval ? pEncIn->gopConfig.idr_interval : (1 << 15);//Use Macro later
    }else{
      vcenc_instance->sps->frame_id_numbers_present_flag = ENCHW_NO;
      vcenc_instance->sps->enable_order_hint             = ENCHW_YES;
      
      vcenc_instance->vp9_inst.MaxPocDisplayVP9  = pEncIn->gopConfig.idr_interval ? pEncIn->gopConfig.idr_interval : (1 << 8);
      vcenc_instance->vp9_inst.MaxPocDisplayVP9  = MIN(vcenc_instance->vp9_inst.MaxPocDisplayVP9, (1 << 8));
    }
  }
  
  vp9_find_pic_to_display(vcenc_instance, pEncIn);
  
  if(pEncIn->bIsIDR)
    vcenc_instance->vp9_inst.POCtobeDisplayVP9 = 0;
  i32 pocDisplay = pEncIn->poc % vcenc_instance->vp9_inst.MaxPocDisplayVP9;
  if (pocDisplay == vcenc_instance->vp9_inst.POCtobeDisplayVP9)
  {
    vcenc_instance->vp9_inst.show_frame          = ENCHW_YES;
    vcenc_instance->vp9_inst.showable_frame      = ENCHW_NO;
    vcenc_instance->vp9_inst.POCtobeDisplayVP9   = (vcenc_instance->vp9_inst.POCtobeDisplayVP9 + 1)%vcenc_instance->vp9_inst.MaxPocDisplayVP9;
  }
  else
  {
    vcenc_instance->vp9_inst.show_frame          = ENCHW_NO;
    vcenc_instance->vp9_inst.showable_frame      = ENCHW_YES;
  }
  vp9_ref_idx(pEncIn, inst);
  
  vp9_configure_buffer_updates(vcenc_instance, (VCEncIn *)pEncIn);
  
  vcenc_instance->vp9_inst.lst_fb_idx        = 0;
  vcenc_instance->vp9_inst.gld_fb_idx         = 0;
  vcenc_instance->vp9_inst.alt_fb_idx         = 0;
  
  vcenc_instance->vp9_inst.ref_frame_sign_bias[1] = 0;
  vcenc_instance->vp9_inst.ref_frame_sign_bias[2] = 0;
  vcenc_instance->vp9_inst.ref_frame_sign_bias[3] = 0;
  
  if(codingType == VCENC_PREDICTED_FRAME){
    vcenc_instance->vp9_inst.reference_mode = VP9_SINGLE_REFERENCE;
    //vcenc_instance->av1_inst.list0_ref_frame      = pic->rpl[0][0]->long_term_flag ? SW_GOLDEN_FRAME : vp9_look_for_ref(vcenc_instance, pic->rpl[0][0]->poc);
  }
  else if (codingType == VCENC_BIDIR_PREDICTED_FRAME){
    vcenc_instance->vp9_inst.reference_mode = VP9_REFERENCE_MODE_SELECT;
  }
  
  vp9_setup_compound_reference(vcenc_instance);
  
  //Quantization config
  const int num_planes = vcenc_instance->sps->chroma_format_idc ? 3:1;
  int qp_y = (vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS);
  int qp_uv = MAX(0, MIN(51, qp_y + vcenc_instance->chromaQpOffset));
  int delta_uv_idx = quantizer_to_qindex[qp_uv] - quantizer_to_qindex[qp_y];
  
  vcenc_instance->vp9_inst.base_qindex = qp_y;
  //Currently set to 0
  //Todo: user configurable
  vcenc_instance->vp9_inst.y_dc_delta_q = 0;
  vcenc_instance->vp9_inst.uv_ac_delta_q = delta_uv_idx;
  vcenc_instance->vp9_inst.uv_dc_delta_q = delta_uv_idx;
  
  vcenc_instance->vp9_inst.entropy = vcenc_instance->vp9_inst.entropy_last[vcenc_instance->vp9_inst.frame_context_idx];
  
  //TX mode decision: currently tx select mode
  vcenc_instance->vp9_inst.tx_mode = VP9_TX_MODE_SELECT;

}

void VCEncVP9SetBuffer(struct vcenc_instance *vcenc_instance, struct buffer *b)
{
    vcenc_instance->vp9_inst.hd_buffer.buf = b;
    
    vcenc_instance->vp9_inst.hd_buffer.range = 255;
    vcenc_instance->vp9_inst.hd_buffer.bottom = 0;
    vcenc_instance->vp9_inst.hd_buffer.bitsLeft = 24;
}

void vp9_init_ModeContexts(struct vcenc_instance *vcenc_instance)
{
    CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.inter_mode_prob, vp9_default_inter_mode_prob,
         sizeof(vp9_default_inter_mode_prob));
}

void vp9_init_MbmodeProbs(struct vcenc_instance *vcenc_instance)
{
    int i, j;

  for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
    for (j = 0; j < 8; j++)
      vcenc_instance->vp9_inst.entropy.a.sb_ymode_prob[i][j] = default_if_y_probs[i][j];
    vcenc_instance->vp9_inst.entropy.a.sb_ymode_prob_b[i][0] = default_if_y_probs[i][8];
  }

  for (i = 0; i < VP9_INTRA_MODES; i++) {
    for (j = 0; j < 8; j++)
      vcenc_instance->vp9_inst.entropy.kf_uv_mode_prob[i][j] = default_kf_uv_probs[i][j];
    vcenc_instance->vp9_inst.entropy.kf_uv_mode_prob_b[i][0] = default_kf_uv_probs[i][8];

    for (j = 0; j < 8; j++)
     vcenc_instance->vp9_inst.entropy.a.uv_mode_prob[i][j] = default_if_uv_probs[i][j];
    vcenc_instance->vp9_inst.entropy.a.uv_mode_prob_b[i][0] = default_if_uv_probs[i][8];
  }

  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.switchable_interp_prob, vp9_switchable_interp_prob,
         sizeof(vp9_switchable_interp_prob));

  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.partition_prob, vp9_partition_probs,
         sizeof(vp9_partition_probs));

  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.intra_inter_prob, default_intra_inter_p,
         sizeof(default_intra_inter_p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.comp_inter_prob, default_comp_inter_p,
         sizeof(default_comp_inter_p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.comp_ref_prob, default_comp_ref_p,
         sizeof(default_comp_ref_p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.single_ref_prob, default_single_ref_p,
         sizeof(default_single_ref_p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.tx32x32_prob, vp9_default_tx_probs_32x32p,
         sizeof(vp9_default_tx_probs_32x32p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.tx16x16_prob, vp9_default_tx_probs_16x16p,
         sizeof(vp9_default_tx_probs_16x16p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.tx8x8_prob, vp9_default_tx_probs_8x8p,
         sizeof(vp9_default_tx_probs_8x8p));
  CWLmemcpy(vcenc_instance->vp9_inst.entropy.a.mbskip_probs, vp9_default_mbskip_probs,
         sizeof(vp9_default_mbskip_probs));

  for (i = 0; i < VP9_INTRA_MODES; i++) {
    for (j = 0; j < VP9_INTRA_MODES; j++) {
      CWLmemcpy(vcenc_instance->vp9_inst.entropy.kf_bmode_prob[i][j], vp9_kf_default_bmode_probs[i][j],
             8);
      vcenc_instance->vp9_inst.entropy.kf_bmode_prob_b[i][j][0] = vp9_kf_default_bmode_probs[i][j][8];
    }
  }
}

void vp9_init_MvProbs(struct vcenc_instance *vcenc_instance)
{
    CWLmemcpy(&vcenc_instance->vp9_inst.entropy.a.nmvc, &vp9_default_nmv_context,
         sizeof(struct NmvContext));
}

void VCEncVP9ResetEntropy(struct vcenc_instance *vcenc_instance)
{

  u32 i, j, k, l, m;
  vp9_init_ModeContexts(vcenc_instance);
  vp9_init_MbmodeProbs(vcenc_instance);
  vp9_init_MvProbs(vcenc_instance);

  /* Copy the default probs into two separate prob tables: part1 and part2. */
  for (i = 0; i < BLOCK_TYPES; i++) {
    for (j = 0; j < REF_TYPES; j++) {
      for (k = 0; k < COEF_BANDS; k++) {
        for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
          if (l >= 3 && k == 0) continue;
          for (m = 0; m < UNCONSTRAINED_NODES; m++) {
            vcenc_instance->vp9_inst.entropy.a.prob_coeffs[i][j][k][l][m] =
                default_coef_probs_4x4[i][j][k][l][m];
            vcenc_instance->vp9_inst.entropy.a.prob_coeffs8x8[i][j][k][l][m] =
                default_coef_probs_8x8[i][j][k][l][m];
            vcenc_instance->vp9_inst.entropy.a.prob_coeffs16x16[i][j][k][l][m] =
                default_coef_probs_16x16[i][j][k][l][m];
            vcenc_instance->vp9_inst.entropy.a.prob_coeffs32x32[i][j][k][l][m] =
                default_coef_probs_32x32[i][j][k][l][m];
          }
        }
      }
    }
  }

  for (i = 0; i < VP9_FRAME_CONTEXTS; i++) {
    CWLmemcpy(&vcenc_instance->vp9_inst.entropy_last[i], &vcenc_instance->vp9_inst.entropy, sizeof(struct Vp9EntropyProbs));
  }

  //inst->sign_bias[0] = inst->sign_bias[1] = inst->sign_bias[2] = 0; // could be in a better place, maybe
}


