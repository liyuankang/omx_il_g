#ifndef _SW_VP9_BITSTREAM_H_
#define _SW_VP9_BITSTREAM_H_

#include "vp9_enums.h"
#include "hevcencapi.h"
#include "sw_put_bits.h"


#define ALIGN_POWER_OF_TWO(value, n) \
    (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))


typedef enum vp9_color_space {
  VP9_CS_UNKNOWN = 0,   /**< Unknown */
  VP9_CS_BT_601 = 1,    /**< BT.601 */
  VP9_CS_BT_709 = 2,    /**< BT.709 */
  VP9_CS_SMPTE_170 = 3, /**< SMPTE.170 */
  VP9_CS_SMPTE_240 = 4, /**< SMPTE.240 */
  VP9_CS_BT_2020 = 5,   /**< BT.2020 */
  VP9_CS_RESERVED = 6,  /**< Reserved */
  VP9_CS_SRGB = 7       /**< sRGB */
} vp9_color_space_t;

typedef enum vp9_color_range {
  VP9_CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
  VP9_CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
} vp9_color_range_t;


typedef struct {
  u64 left_y[TX_SIZE_MAX];
  u64 above_y[TX_SIZE_MAX];
  u64 int_4x4_y;
  uint16_t left_uv[TX_SIZE_MAX];
  uint16_t above_uv[TX_SIZE_MAX];
  uint16_t int_4x4_uv;
  uint8_t lfl_y[64];
} LOOP_FILTER_MASK;


typedef struct loopfilter {
  int filter_level;
  int last_filter_level;

  int sharpness_level;
  int last_sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, GF, ARF
  signed char ref_deltas[MAX_REF_LF_DELTAS];
  signed char last_ref_deltas[MAX_REF_LF_DELTAS];

  // 0 = ZERO_MV, MV
  signed char mode_deltas[MAX_MODE_LF_DELTAS];
  signed char last_mode_deltas[MAX_MODE_LF_DELTAS];

  LOOP_FILTER_MASK *lfm;
  int lfm_stride;
}vp9_loopfilter;


typedef struct vpx_buffer{
    struct buffer* buf;

    i32 range;      /* Bool encoder range [128, 255] */
    i32 bottom;     /* Bool encoder left endpoint */
    i32 bitsLeft;       /* Bool encoder bits left before flush bottom */
} vp9_hbuffer;



i32 VCEncFrameHeaderVP9(VCEncInst inst, const VCEncIn *pEncIn, u32 *pStrmLen, VCEncPictureCodingType codingType);




#endif

