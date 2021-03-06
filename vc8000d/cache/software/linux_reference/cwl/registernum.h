/* Register interface based on the document version 1.00 */
    //H2REG(HWIF_PRODUCT_ID                         , 0  ,0xffff0000,       16,        0,RO,"Product ID"),
    HWIF_CACHE_ENABLE,
    HWIF_CACHE_IRQ,
    HWIF_CACHE_ABORT_ALL,
    HWIF_CACHE_IRQ_TIME_OUT,
    HWIF_CACHE_IRQ_ABORTED,
    HWIF_CACHE_SW_REORDER_E,
    HWIF_CACHE_ALL,
    HWIF_CACHE_EXP_WR_E,
    HWIF_AXI_RD_ID_E,
    HWIF_AXI_RD_ID,
    HWIF_CACHE_TIME_OUT_THR,
    HWIF_CACHE_EXP_LIST,
    HWIF_CACHE_NUM_CHANNEL,
    HWIF_CACHE_NUM_EXCPT,
    HWIF_CACHE_LINE_SIZE,
    HWIF_CACHE_BUS_IDEL_S,
    HWIF_CACHE_BUS_IDEL_M,
    HWIF_CACHE_IRQ_MASK_TIME_OUT,
    HWIF_CACHE_IRQ_MASK_ABORT,
    HWIF_CACHE_CHANNEL_0_VALILD,
    HWIF_CACHE_CHANNEL_0_PREFETCH_E,
    HWIF_CACHE_CHANNEL_0_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_0_START_ADDR,
    HWIF_CACHE_CHANNEL_0_END_ADDR,
    HWIF_CACHE_CHANNEL_0_LINE_SIZE,
    HWIF_CACHE_CHANNEL_0_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_0_LINE_CNT,
    HWIF_CACHE_CHANNEL_0_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_0_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_1_VALILD,
    HWIF_CACHE_CHANNEL_1_PREFETCH_E,
    HWIF_CACHE_CHANNEL_1_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_1_START_ADDR,
    HWIF_CACHE_CHANNEL_1_END_ADDR,
    HWIF_CACHE_CHANNEL_1_LINE_SIZE,
    HWIF_CACHE_CHANNEL_1_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_1_LINE_CNT,
    HWIF_CACHE_CHANNEL_1_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_1_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_2_VALILD,
    HWIF_CACHE_CHANNEL_2_PREFETCH_E,
    HWIF_CACHE_CHANNEL_2_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_2_START_ADDR,
    HWIF_CACHE_CHANNEL_2_END_ADDR,
    HWIF_CACHE_CHANNEL_2_LINE_SIZE,
    HWIF_CACHE_CHANNEL_2_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_2_LINE_CNT,
    HWIF_CACHE_CHANNEL_2_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_2_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_3_VALILD,
    HWIF_CACHE_CHANNEL_3_PREFETCH_E,
    HWIF_CACHE_CHANNEL_3_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_3_START_ADDR,
    HWIF_CACHE_CHANNEL_3_END_ADDR,
    HWIF_CACHE_CHANNEL_3_LINE_SIZE,
    HWIF_CACHE_CHANNEL_3_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_3_LINE_CNT,
    HWIF_CACHE_CHANNEL_3_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_3_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_4_VALILD,
    HWIF_CACHE_CHANNEL_4_PREFETCH_E,
    HWIF_CACHE_CHANNEL_4_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_4_START_ADDR,
    HWIF_CACHE_CHANNEL_4_END_ADDR,
    HWIF_CACHE_CHANNEL_4_LINE_SIZE,
    HWIF_CACHE_CHANNEL_4_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_4_LINE_CNT,
    HWIF_CACHE_CHANNEL_4_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_4_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_5_VALILD,
    HWIF_CACHE_CHANNEL_5_PREFETCH_E,
    HWIF_CACHE_CHANNEL_5_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_5_START_ADDR,
    HWIF_CACHE_CHANNEL_5_END_ADDR,
    HWIF_CACHE_CHANNEL_5_LINE_SIZE,
    HWIF_CACHE_CHANNEL_5_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_5_LINE_CNT,
    HWIF_CACHE_CHANNEL_5_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_5_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_6_VALILD,
    HWIF_CACHE_CHANNEL_6_PREFETCH_E,
    HWIF_CACHE_CHANNEL_6_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_6_START_ADDR,
    HWIF_CACHE_CHANNEL_6_END_ADDR,
    HWIF_CACHE_CHANNEL_6_LINE_SIZE,
    HWIF_CACHE_CHANNEL_6_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_6_LINE_CNT,
    HWIF_CACHE_CHANNEL_6_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_6_LINE_RANGE_H,
    HWIF_CACHE_CHANNEL_7_VALILD,
    HWIF_CACHE_CHANNEL_7_PREFETCH_E,
    HWIF_CACHE_CHANNEL_7_PREFETCH_TH,
    HWIF_CACHE_CHANNEL_7_START_ADDR,
    HWIF_CACHE_CHANNEL_7_END_ADDR,
    HWIF_CACHE_CHANNEL_7_LINE_SIZE,
    HWIF_CACHE_CHANNEL_7_LINE_STRIDE,
    HWIF_CACHE_CHANNEL_7_LINE_CNT,
    HWIF_CACHE_CHANNEL_7_LINE_SHIFT_H,
    HWIF_CACHE_CHANNEL_7_LINE_RANGE_H,

    HWIF_CACHE_WR_ENABLE,
    
    HWIF_CACHE_WR_BASE_ID,
    HWIF_CACHE_WR_TIME_OUT,

    HWIF_CACHE_WR_CHN_NUM,
    HWIF_CACHE_WR_NUM_IDS,
    HWIF_CACHE_WR_ALIGNMENT,
    HWIF_CACHE_WR_S_BUS_BUSY,
    HWIF_CACHE_WR_M_BUS_BUSY,

    HWIF_CACHE_WR_IRQ_TIMEOUT,
    HWIF_CACHE_WR_IRQ_ABORTED,

    HWIF_CACHE_WR_IRQ_TIMEOUT_MASK,
    HWIF_CACHE_WR_IRQ_ABORTED_MASK,

    HWIF_CACHE_WR_CH_0_VALID,
    HWIF_CACHE_WR_CH_0_STRIPE_E,
    HWIF_CACHE_WR_CH_0_PAD_E,
    HWIF_CACHE_WR_CH_0_RFC_E,
    HWIF_CACHE_WR_CH_0_START_ADDR,

    HWIF_CACHE_WR_CH_0_BLOCK_E,
    HWIF_CACHE_WR_CH_0_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_0_LINE_SIZE,
    HWIF_CACHE_WR_CH_0_LINE_STRIDE,

    HWIF_CACHE_WR_CH_0_LINE_CNT,
    HWIF_CACHE_WR_CH_0_MAX_H,

    HWIF_CACHE_WR_CH_0_LN_CNT_START,
    HWIF_CACHE_WR_CH_0_LN_CNT_MID,
    HWIF_CACHE_WR_CH_0_LN_CNT_END,
    HWIF_CACHE_WR_CH_0_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_1_VALID,
    HWIF_CACHE_WR_CH_1_STRIPE_E,
    HWIF_CACHE_WR_CH_1_PAD_E,
    HWIF_CACHE_WR_CH_1_RFC_E,
    HWIF_CACHE_WR_CH_1_START_ADDR,

    HWIF_CACHE_WR_CH_1_BLOCK_E,
    HWIF_CACHE_WR_CH_1_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_1_LINE_SIZE,
    HWIF_CACHE_WR_CH_1_LINE_STRIDE,

    HWIF_CACHE_WR_CH_1_LINE_CNT,
    HWIF_CACHE_WR_CH_1_MAX_H,

    HWIF_CACHE_WR_CH_1_LN_CNT_START,
    HWIF_CACHE_WR_CH_1_LN_CNT_MID,
    HWIF_CACHE_WR_CH_1_LN_CNT_END,
    HWIF_CACHE_WR_CH_1_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_2_VALID,
    HWIF_CACHE_WR_CH_2_STRIPE_E,
    HWIF_CACHE_WR_CH_2_PAD_E,
    HWIF_CACHE_WR_CH_2_RFC_E,
    HWIF_CACHE_WR_CH_2_START_ADDR,

    HWIF_CACHE_WR_CH_2_BLOCK_E,
    HWIF_CACHE_WR_CH_2_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_2_LINE_SIZE,
    HWIF_CACHE_WR_CH_2_LINE_STRIDE,

    HWIF_CACHE_WR_CH_2_LINE_CNT,
    HWIF_CACHE_WR_CH_2_MAX_H,

    HWIF_CACHE_WR_CH_2_LN_CNT_START,
    HWIF_CACHE_WR_CH_2_LN_CNT_MID,
    HWIF_CACHE_WR_CH_2_LN_CNT_END,
    HWIF_CACHE_WR_CH_2_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_3_VALID,
    HWIF_CACHE_WR_CH_3_STRIPE_E,
    HWIF_CACHE_WR_CH_3_PAD_E,
    HWIF_CACHE_WR_CH_3_RFC_E,
    HWIF_CACHE_WR_CH_3_START_ADDR,

    HWIF_CACHE_WR_CH_3_BLOCK_E,
    HWIF_CACHE_WR_CH_3_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_3_LINE_SIZE,
    HWIF_CACHE_WR_CH_3_LINE_STRIDE,

    HWIF_CACHE_WR_CH_3_LINE_CNT,
    HWIF_CACHE_WR_CH_3_MAX_H,

    HWIF_CACHE_WR_CH_3_LN_CNT_START,
    HWIF_CACHE_WR_CH_3_LN_CNT_MID,
    HWIF_CACHE_WR_CH_3_LN_CNT_END,
    HWIF_CACHE_WR_CH_3_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_4_VALID,
    HWIF_CACHE_WR_CH_4_STRIPE_E,
    HWIF_CACHE_WR_CH_4_PAD_E,
    HWIF_CACHE_WR_CH_4_RFC_E,
    HWIF_CACHE_WR_CH_4_START_ADDR,

    HWIF_CACHE_WR_CH_4_BLOCK_E,
    HWIF_CACHE_WR_CH_4_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_4_LINE_SIZE,
    HWIF_CACHE_WR_CH_4_LINE_STRIDE,

    HWIF_CACHE_WR_CH_4_LINE_CNT,
    HWIF_CACHE_WR_CH_4_MAX_H,

    HWIF_CACHE_WR_CH_4_LN_CNT_START,
    HWIF_CACHE_WR_CH_4_LN_CNT_MID,
    HWIF_CACHE_WR_CH_4_LN_CNT_END,
    HWIF_CACHE_WR_CH_4_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_5_VALID,
    HWIF_CACHE_WR_CH_5_STRIPE_E,
    HWIF_CACHE_WR_CH_5_PAD_E,
    HWIF_CACHE_WR_CH_5_RFC_E,
    HWIF_CACHE_WR_CH_5_START_ADDR,

    HWIF_CACHE_WR_CH_5_BLOCK_E,
    HWIF_CACHE_WR_CH_5_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_5_LINE_SIZE,
    HWIF_CACHE_WR_CH_5_LINE_STRIDE,

    HWIF_CACHE_WR_CH_5_LINE_CNT,
    HWIF_CACHE_WR_CH_5_MAX_H,

    HWIF_CACHE_WR_CH_5_LN_CNT_START,
    HWIF_CACHE_WR_CH_5_LN_CNT_MID,
    HWIF_CACHE_WR_CH_5_LN_CNT_END,
    HWIF_CACHE_WR_CH_5_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_6_VALID,
    HWIF_CACHE_WR_CH_6_STRIPE_E,
    HWIF_CACHE_WR_CH_6_PAD_E,
    HWIF_CACHE_WR_CH_6_RFC_E,
    HWIF_CACHE_WR_CH_6_START_ADDR,

    HWIF_CACHE_WR_CH_6_BLOCK_E,
    HWIF_CACHE_WR_CH_6_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_6_LINE_SIZE,
    HWIF_CACHE_WR_CH_6_LINE_STRIDE,

    HWIF_CACHE_WR_CH_6_LINE_CNT,
    HWIF_CACHE_WR_CH_6_MAX_H,

    HWIF_CACHE_WR_CH_6_LN_CNT_START,
    HWIF_CACHE_WR_CH_6_LN_CNT_MID,
    HWIF_CACHE_WR_CH_6_LN_CNT_END,
    HWIF_CACHE_WR_CH_6_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_7_VALID,
    HWIF_CACHE_WR_CH_7_STRIPE_E,
    HWIF_CACHE_WR_CH_7_PAD_E,
    HWIF_CACHE_WR_CH_7_RFC_E,
    HWIF_CACHE_WR_CH_7_START_ADDR,

    HWIF_CACHE_WR_CH_7_BLOCK_E,
    HWIF_CACHE_WR_CH_7_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_7_LINE_SIZE,
    HWIF_CACHE_WR_CH_7_LINE_STRIDE,

    HWIF_CACHE_WR_CH_7_LINE_CNT,
    HWIF_CACHE_WR_CH_7_MAX_H,

    HWIF_CACHE_WR_CH_7_LN_CNT_START,
    HWIF_CACHE_WR_CH_7_LN_CNT_MID,
    HWIF_CACHE_WR_CH_7_LN_CNT_END,
    HWIF_CACHE_WR_CH_7_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_8_VALID,
    HWIF_CACHE_WR_CH_8_STRIPE_E,
    HWIF_CACHE_WR_CH_8_PAD_E,
    HWIF_CACHE_WR_CH_8_RFC_E,
    HWIF_CACHE_WR_CH_8_START_ADDR,

    HWIF_CACHE_WR_CH_8_BLOCK_E,
    HWIF_CACHE_WR_CH_8_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_8_LINE_SIZE,
    HWIF_CACHE_WR_CH_8_LINE_STRIDE,

    HWIF_CACHE_WR_CH_8_LINE_CNT,
    HWIF_CACHE_WR_CH_8_MAX_H,

    HWIF_CACHE_WR_CH_8_LN_CNT_START,
    HWIF_CACHE_WR_CH_8_LN_CNT_MID,
    HWIF_CACHE_WR_CH_8_LN_CNT_END,
    HWIF_CACHE_WR_CH_8_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_9_VALID,
    HWIF_CACHE_WR_CH_9_STRIPE_E,
    HWIF_CACHE_WR_CH_9_PAD_E,
    HWIF_CACHE_WR_CH_9_RFC_E,
    HWIF_CACHE_WR_CH_9_START_ADDR,

    HWIF_CACHE_WR_CH_9_BLOCK_E,
    HWIF_CACHE_WR_CH_9_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_9_LINE_SIZE,
    HWIF_CACHE_WR_CH_9_LINE_STRIDE,

    HWIF_CACHE_WR_CH_9_LINE_CNT,
    HWIF_CACHE_WR_CH_9_MAX_H,

    HWIF_CACHE_WR_CH_9_LN_CNT_START,
    HWIF_CACHE_WR_CH_9_LN_CNT_MID,
    HWIF_CACHE_WR_CH_9_LN_CNT_END,
    HWIF_CACHE_WR_CH_9_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_10_VALID,
    HWIF_CACHE_WR_CH_10_STRIPE_E,
    HWIF_CACHE_WR_CH_10_PAD_E,
    HWIF_CACHE_WR_CH_10_RFC_E,
    HWIF_CACHE_WR_CH_10_START_ADDR,

    HWIF_CACHE_WR_CH_10_BLOCK_E,
    HWIF_CACHE_WR_CH_10_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_10_LINE_SIZE,
    HWIF_CACHE_WR_CH_10_LINE_STRIDE,

    HWIF_CACHE_WR_CH_10_LINE_CNT,
    HWIF_CACHE_WR_CH_10_MAX_H,

    HWIF_CACHE_WR_CH_10_LN_CNT_START,
    HWIF_CACHE_WR_CH_10_LN_CNT_MID,
    HWIF_CACHE_WR_CH_10_LN_CNT_END,
    HWIF_CACHE_WR_CH_10_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_11_VALID,
    HWIF_CACHE_WR_CH_11_STRIPE_E,
    HWIF_CACHE_WR_CH_11_PAD_E,
    HWIF_CACHE_WR_CH_11_RFC_E,
    HWIF_CACHE_WR_CH_11_START_ADDR,

    HWIF_CACHE_WR_CH_11_BLOCK_E,
    HWIF_CACHE_WR_CH_11_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_11_LINE_SIZE,
    HWIF_CACHE_WR_CH_11_LINE_STRIDE,

    HWIF_CACHE_WR_CH_11_LINE_CNT,
    HWIF_CACHE_WR_CH_11_MAX_H,

    HWIF_CACHE_WR_CH_11_LN_CNT_START,
    HWIF_CACHE_WR_CH_11_LN_CNT_MID,
    HWIF_CACHE_WR_CH_11_LN_CNT_END,
    HWIF_CACHE_WR_CH_11_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_12_VALID,
    HWIF_CACHE_WR_CH_12_STRIPE_E,
    HWIF_CACHE_WR_CH_12_PAD_E,
    HWIF_CACHE_WR_CH_12_RFC_E,
    HWIF_CACHE_WR_CH_12_START_ADDR,

    HWIF_CACHE_WR_CH_12_BLOCK_E,
    HWIF_CACHE_WR_CH_12_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_12_LINE_SIZE,
    HWIF_CACHE_WR_CH_12_LINE_STRIDE,

    HWIF_CACHE_WR_CH_12_LINE_CNT,
    HWIF_CACHE_WR_CH_12_MAX_H,

    HWIF_CACHE_WR_CH_12_LN_CNT_START,
    HWIF_CACHE_WR_CH_12_LN_CNT_MID,
    HWIF_CACHE_WR_CH_12_LN_CNT_END,
    HWIF_CACHE_WR_CH_12_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_13_VALID,
    HWIF_CACHE_WR_CH_13_STRIPE_E,
    HWIF_CACHE_WR_CH_13_PAD_E,
    HWIF_CACHE_WR_CH_13_RFC_E,
    HWIF_CACHE_WR_CH_13_START_ADDR,

    HWIF_CACHE_WR_CH_13_BLOCK_E,
    HWIF_CACHE_WR_CH_13_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_13_LINE_SIZE,
    HWIF_CACHE_WR_CH_13_LINE_STRIDE,

    HWIF_CACHE_WR_CH_13_LINE_CNT,
    HWIF_CACHE_WR_CH_13_MAX_H,

    HWIF_CACHE_WR_CH_13_LN_CNT_START,
    HWIF_CACHE_WR_CH_13_LN_CNT_MID,
    HWIF_CACHE_WR_CH_13_LN_CNT_END,
    HWIF_CACHE_WR_CH_13_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_14_VALID,
    HWIF_CACHE_WR_CH_14_STRIPE_E,
    HWIF_CACHE_WR_CH_14_PAD_E,
    HWIF_CACHE_WR_CH_14_RFC_E,
    HWIF_CACHE_WR_CH_14_START_ADDR,

    HWIF_CACHE_WR_CH_14_BLOCK_E,
    HWIF_CACHE_WR_CH_14_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_14_LINE_SIZE,
    HWIF_CACHE_WR_CH_14_LINE_STRIDE,

    HWIF_CACHE_WR_CH_14_LINE_CNT,
    HWIF_CACHE_WR_CH_14_MAX_H,

    HWIF_CACHE_WR_CH_14_LN_CNT_START,
    HWIF_CACHE_WR_CH_14_LN_CNT_MID,
    HWIF_CACHE_WR_CH_14_LN_CNT_END,
    HWIF_CACHE_WR_CH_14_LN_CNT_STEP,

    HWIF_CACHE_WR_CH_15_VALID,
    HWIF_CACHE_WR_CH_15_STRIPE_E,
    HWIF_CACHE_WR_CH_15_PAD_E,
    HWIF_CACHE_WR_CH_15_RFC_E,
    HWIF_CACHE_WR_CH_15_START_ADDR,

    HWIF_CACHE_WR_CH_15_BLOCK_E,
    HWIF_CACHE_WR_CH_15_START_ADDR_MSB,

    HWIF_CACHE_WR_CH_15_LINE_SIZE,
    HWIF_CACHE_WR_CH_15_LINE_STRIDE,

    HWIF_CACHE_WR_CH_15_LINE_CNT,
    HWIF_CACHE_WR_CH_15_MAX_H,

    HWIF_CACHE_WR_CH_15_LN_CNT_START,
    HWIF_CACHE_WR_CH_15_LN_CNT_MID,
    HWIF_CACHE_WR_CH_15_LN_CNT_END,
    HWIF_CACHE_WR_CH_15_LN_CNT_STEP,
