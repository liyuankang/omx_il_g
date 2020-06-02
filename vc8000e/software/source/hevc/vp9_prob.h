#ifndef VP9_PROB_H_
#define VP9_PROB_H_ 

#include "vp9_enums.h"

#define COEFF_CONTEXTS 6
#define DIFF_UPDATE_PROB 252

#if 0
typedef vp9_prob vp9_coeff_probs_model[REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][UNCONSTRAINED_NODES];

struct tx_probs {
  vp9_prob p32x32[VP9_TX_SIZE_CONTEXTS][TX_SIZES - 1];
  vp9_prob p16x16[VP9_TX_SIZE_CONTEXTS][TX_SIZES - 2];
  vp9_prob p8x8[VP9_TX_SIZE_CONTEXTS][TX_SIZES - 3];
};

typedef struct {
  vp9_prob sign;
  vp9_prob classes[MV_CLASSES - 1];
  vp9_prob class0[CLASS0_SIZE - 1];
  vp9_prob bits[MV_OFFSET_BITS];
  vp9_prob class0_fp[CLASS0_SIZE][MV_FP_SIZE - 1];
  vp9_prob fp[MV_FP_SIZE - 1];
  vp9_prob class0_hp;
  vp9_prob hp;
} nmv_component;


typedef struct {
  vp9_prob joints[MV_JOINTS - 1];
  nmv_component comps[2];
} nmv_context;


typedef struct {
  unsigned int sign[2];
  unsigned int classes[MV_CLASSES];
  unsigned int class0[CLASS0_SIZE];
  unsigned int bits[MV_OFFSET_BITS][2];
  unsigned int class0_fp[CLASS0_SIZE][MV_FP_SIZE];
  unsigned int fp[MV_FP_SIZE];
  unsigned int class0_hp[2];
  unsigned int hp[2];
} nmv_component_counts;


typedef struct {
  unsigned int joints[MV_JOINTS];
  nmv_component_counts comps[2];
} nmv_context_counts;


struct tx_counts {
 unsigned int p32x32[VP9_TX_SIZE_CONTEXTS][TX_SIZES];
  unsigned int p16x16[VP9_TX_SIZE_CONTEXTS][TX_SIZES - 1];
  unsigned int p8x8[VP9_TX_SIZE_CONTEXTS][TX_SIZES - 2];
  unsigned int tx_totals[TX_SIZES];
}; 

typedef struct frame_contexts {
  vp9_prob y_mode_prob[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
  vp9_prob uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
  vp9_prob partition_prob[PARTITION_CONTEXTS][PARTITION_TYPES - 1];
  vp9_coeff_probs_model coef_probs[TX_SIZES][PLANE_TYPES];
  vp9_prob switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS]
                                 [SWITCHABLE_FILTERS - 1];
  vp9_prob inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1];
  vp9_prob intra_inter_prob[INTRA_INTER_CONTEXTS];
  vp9_prob comp_inter_prob[COMP_INTER_CONTEXTS];
  vp9_prob single_ref_prob[REF_CONTEXTS][2];
  vp9_prob comp_ref_prob[REF_CONTEXTS];
  struct tx_probs tx_probs;
  vp9_prob skip_probs[SKIP_CONTEXTS];
  nmv_context nmvc;
  int initialized;
} FRAME_CONTEXT;


typedef struct FRAME_COUNTS {
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
  unsigned int uv_mode[INTRA_MODES][INTRA_MODES];
  unsigned int partition[PARTITION_CONTEXTS][PARTITION_TYPES];
  vp9_coeff_count_model coef[TX_SIZES][PLANE_TYPES];
  unsigned int eob_branch[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS]
                         [COEFF_CONTEXTS];
  unsigned int switchable_interp[SWITCHABLE_FILTER_CONTEXTS]
                                [SWITCHABLE_FILTERS];
  unsigned int inter_mode[INTER_MODE_CONTEXTS][INTER_MODES];
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
  unsigned int single_ref[REF_CONTEXTS][2][2];
  unsigned int comp_ref[REF_CONTEXTS][2];
  struct tx_counts tx;
  unsigned int skip[SKIP_CONTEXTS][2];
  nmv_context_counts mv;
} FRAME_COUNTS;



static const uint8_t vp9_norm[256] = {
  0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static INLINE vp9_prob get_binary_prob(unsigned int n0, unsigned int n1) {
  const unsigned int den = n0 + n1;
  if (den == 0) return 128u;
  return get_prob(n0, den);
}
#endif

struct NmvContext {
  /* last bytes of address 41 */
  u8 joints[MV_JOINTS - 1];
  u8 sign[2];
  /* address 42 */
  u8 class0[2][CLASS0_SIZE - 1];
  u8 fp[2][4 - 1];
  u8 class0_hp[2];
  u8 hp[2];
  u8 classes[2][MV_CLASSES - 1];
  /* address 43 */
  u8 class0_fp[2][CLASS0_SIZE][4 - 1];
  u8 bits[2][MV_OFFSET_BITS];
};


struct Vp9AdaptiveEntropyProbs {
  /* address 32 */
  u8 inter_mode_prob[VP9_INTER_MODE_CONTEXTS][4];
  u8 intra_inter_prob[INTRA_INTER_CONTEXTS];

  /* address 33 */
  u8 uv_mode_prob[VP9_INTRA_MODES][8];
  u8 tx8x8_prob[VP9_TX_SIZE_CONTEXTS][TX_SIZE_MAX - 3];
  u8 tx16x16_prob[VP9_TX_SIZE_CONTEXTS][TX_SIZE_MAX - 2];
  u8 tx32x32_prob[VP9_TX_SIZE_CONTEXTS][TX_SIZE_MAX - 1];
  u8 sb_ymode_prob_b[BLOCK_SIZE_GROUPS][1];
  u8 sb_ymode_prob[BLOCK_SIZE_GROUPS][8];

  /* address 37 */
  u8 partition_prob[NUM_FRAME_TYPES][NUM_PARTITION_CONTEXTS][PARTITION_TYPES];

  /* address 41 */
  u8 uv_mode_prob_b[VP9_INTRA_MODES][1];
  u8 switchable_interp_prob[VP9_SWITCHABLE_FILTERS + 1][VP9_SWITCHABLE_FILTERS -1];
  u8 comp_inter_prob[COMP_INTER_CONTEXTS];
  u8 mbskip_probs[MBSKIP_CONTEXTS];
  //VP9HWPAD(pad1, 1);
  u8 pad1[1];

  struct NmvContext nmvc;

  /* address 44 */
  u8 single_ref_prob[VP9_REF_CONTEXTS][2];
  u8 comp_ref_prob[VP9_REF_CONTEXTS];
  //VP9HWPAD(pad2, 17);
  u8 pad2[17];

  /* address 45 */
  u8 prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD]
                [ENTROPY_NODES_PART1];
  u8 prob_coeffs8x8[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD]
                   [ENTROPY_NODES_PART1];
  u8 prob_coeffs16x16[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD]
                     [ENTROPY_NODES_PART1];
  u8 prob_coeffs32x32[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS_PAD]
                     [ENTROPY_NODES_PART1];
};

struct Vp9EntropyProbs {
  /* Default keyframe probs */
  /* Table formatted for 256b memory, probs 0to7 for all tables followed by
   * probs 8toN for all tables.
   */

  u8 kf_bmode_prob[VP9_INTRA_MODES][VP9_INTRA_MODES][8];

  /* Address 25 */
  u8 kf_bmode_prob_b[VP9_INTRA_MODES][VP9_INTRA_MODES][1];
  u8 ref_pred_probs[PREDICTION_PROBS];
  u8 mb_segment_tree_probs[MB_SEG_TREE_PROBS];
  u8 segment_pred_probs[PREDICTION_PROBS];
  u8 ref_scores[MAX_REF_FRAMES];
  u8 prob_comppred[COMP_PRED_CONTEXTS];
  //VP9HWPAD(pad1, 9);
  u8 pad1[9];

  /* Address 29 */
  u8 kf_uv_mode_prob[VP9_INTRA_MODES][8];
  u8 kf_uv_mode_prob_b[VP9_INTRA_MODES][1];
  //VP9HWPAD(pad2, 6);
  u8 pad2[6];

  struct Vp9AdaptiveEntropyProbs a; /* Probs with backward adaptation */
};

#endif

