/* Register interface based on the document version 1.00 */
    //H2REG(HWIF_PRODUCT_ID                         , 0  ,0xffff0000,       16,        0,RO,"Product ID"),
    /*Cache read register define*/
    H2REG(HWIF_CACHE_ENABLE                       , 0x04  ,0x00000001,        0,        0,RW,"Cache enable"),
    H2REG(HWIF_CACHE_IRQ                       , 0x04  ,0x00000002,        1,        0,RW,"Cache IRQ"),
    H2REG(HWIF_CACHE_ABORT_ALL               , 0x04  ,0x00000004,        2,        0,RW,"Abort all valid channels"),
    H2REG(HWIF_CACHE_IRQ_TIME_OUT                , 0x04  ,0x00000008,        3,        0,RW,"Bus timeout IRQ"),
    H2REG(HWIF_CACHE_IRQ_ABORTED               , 0x04  ,0x00000020,        5,        0,RW,"Abort channel IRQ"),
    H2REG(HWIF_CACHE_SW_REORDER_E              , 0x04  ,0x00000080,        7,        0,RW,"Reorder enable"),

    H2REG(HWIF_CACHE_ALL                  , 0x08  ,0x00000001,        0,        0,RW,"Cache all"),
    H2REG(HWIF_CACHE_EXP_WR_E              , 0x08  ,0x00000002,       1,        0,RW,"cache_excpt_wr_e"),
    H2REG(HWIF_AXI_RD_ID_E                 , 0x08  ,0x00000008,        3,        0,RW,"AXI master RID can be configured"),
    H2REG(HWIF_AXI_RD_ID                   , 0x08  ,0x00000FF0,        4,        0,RW,"AXI master RID"),
    H2REG(HWIF_CACHE_TIME_OUT_THR          , 0x08  ,0xFFFF0000,        16,        0,RW,"Bus timeout threshold"),

    H2REG(HWIF_CACHE_EXP_LIST              , 0x0C  ,0xFFFFFFFF,   0,       0,WO,"exception addr"),

    H2REG(HWIF_CACHE_NUM_CHANNEL           , 0x10  ,0x0000000F,   0,        0,RO,"How many work channels supported by HW"),
    H2REG(HWIF_CACHE_NUM_EXCPT             , 0x10  ,0x000003F0,     4,     0,RO,"How many exception addr supported by HW"),
    H2REG(HWIF_CACHE_LINE_SIZE             , 0x10  ,0x0003FC00,    10,  0,RO,"How many bus width data matched with CACHE_LINE_SIZE"),
    H2REG(HWIF_CACHE_BUS_IDEL_S            , 0x10  ,0x40000000,   30,   0,RO,"AXI slave bus busy"),
    H2REG(HWIF_CACHE_BUS_IDEL_M            , 0x10  ,0x80000000,   31,   0,RO,"AXI master bus busy"),

    H2REG(HWIF_CACHE_IRQ_MASK_TIME_OUT     , 0x14  ,0x00000001,     0,        0,RW,"abort IRQ MASK"),
    H2REG(HWIF_CACHE_IRQ_MASK_ABORT        , 0x14  ,0x00000002,     1,        0,RW,"time out IRQ MASK"),
 
    H2REG(HWIF_CACHE_CHANNEL_0_VALILD               , 0x18  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_0_PREFETCH_E               , 0x18  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_0_PREFETCH_TH                , 0x18  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_0_START_ADDR                , 0x18  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_0_END_ADDR              , 0x1C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_0_LINE_SIZE                , 0x20  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_0_LINE_STRIDE               , 0x20  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_0_LINE_CNT               , 0x24  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_0_LINE_SHIFT_H               , 0x24  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_0_LINE_RANGE_H               , 0x24  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_1_VALILD               , 0x28  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_1_PREFETCH_E               , 0x28  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_1_PREFETCH_TH                , 0x28  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_1_START_ADDR                , 0x28  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_1_END_ADDR              , 0x2C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_1_LINE_SIZE                , 0x30  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_1_LINE_STRIDE               , 0x30  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_1_LINE_CNT               , 0x34  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_1_LINE_SHIFT_H               , 0x34  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_1_LINE_RANGE_H               , 0x34  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_2_VALILD               , 0x38  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_2_PREFETCH_E               , 0x38  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_2_PREFETCH_TH                , 0x38  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_2_START_ADDR                , 0x38  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_2_END_ADDR              , 0x3C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_2_LINE_SIZE                , 0x40  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_2_LINE_STRIDE               , 0x40  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_2_LINE_CNT               , 0x44  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_2_LINE_SHIFT_H               , 0x44  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_2_LINE_RANGE_H               , 0x44  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_3_VALILD               , 0x48  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_3_PREFETCH_E               , 0x48  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_3_PREFETCH_TH                , 0x48  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_3_START_ADDR                , 0x48  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_3_END_ADDR              , 0x4C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_3_LINE_SIZE                , 0x50  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_3_LINE_STRIDE               , 0x50  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_3_LINE_CNT               , 0x54  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_3_LINE_SHIFT_H               , 0x54  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_3_LINE_RANGE_H               , 0x54  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_4_VALILD               , 0x58  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_4_PREFETCH_E               , 0x58  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_4_PREFETCH_TH                , 0x58  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_4_START_ADDR                , 0x58  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_4_END_ADDR              , 0x5C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_4_LINE_SIZE                , 0x60  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_4_LINE_STRIDE               , 0x60  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_4_LINE_CNT               , 0x64  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_4_LINE_SHIFT_H               , 0x64  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_4_LINE_RANGE_H               , 0x64  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_5_VALILD               , 0x68  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_5_PREFETCH_E               , 0x68  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_5_PREFETCH_TH                , 0x68  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_5_START_ADDR                , 0x68  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_5_END_ADDR              , 0x6C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_5_LINE_SIZE                , 0x70  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_5_LINE_STRIDE               , 0x70  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_5_LINE_CNT               , 0x74  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_5_LINE_SHIFT_H               , 0x74  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_5_LINE_RANGE_H               , 0x74  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_6_VALILD               , 0x78  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_6_PREFETCH_E               , 0x78  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_6_PREFETCH_TH                , 0x78  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_6_START_ADDR                , 0x78  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_6_END_ADDR              , 0x7C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_6_LINE_SIZE                , 0x80  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_6_LINE_STRIDE               , 0x80  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_6_LINE_CNT               , 0x84  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_6_LINE_SHIFT_H               , 0x84  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_6_LINE_RANGE_H               , 0x84  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),
    H2REG(HWIF_CACHE_CHANNEL_7_VALILD               , 0x88  ,0x00000001,        0,        0,RW,"channel is valid"),
    H2REG(HWIF_CACHE_CHANNEL_7_PREFETCH_E               , 0x88  ,0x00000002,        1,        0,RW,"channel prefetch is valid"),
    H2REG(HWIF_CACHE_CHANNEL_7_PREFETCH_TH                , 0x88  ,0x0000000C,        2,        0,RW,"channel prefetch threshold"),
    H2REG(HWIF_CACHE_CHANNEL_7_START_ADDR                , 0x88  ,0xFFFFFFF0,        4,        0,RW,"Start addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_7_END_ADDR              , 0x8C  ,0xFFFFFFF0,        4,        0,RW,"End addr>>4"),
    H2REG(HWIF_CACHE_CHANNEL_7_LINE_SIZE                , 0x90  ,0x0000FFFF,        0,        0,RW,"line size in unit of byte"),
    H2REG(HWIF_CACHE_CHANNEL_7_LINE_STRIDE               , 0x90  ,0xFFFF0000,        16,        0,RW,"line stride in unit of 16 bytes"),
    H2REG(HWIF_CACHE_CHANNEL_7_LINE_CNT               , 0x94  ,0x0000FFFF,        0,        0,RW,"vertical direction amount of lines"),
    H2REG(HWIF_CACHE_CHANNEL_7_LINE_SHIFT_H               , 0x94  ,0x00FF0000,        16,        0,RW,"vertical change step when prefetch block scan change block row. Unit is one line"),
    H2REG(HWIF_CACHE_CHANNEL_7_LINE_RANGE_H               , 0x94  ,0xFF000000,        24,        0,RW,"1/2 Block height in unit of one line"),


    /*Cache write register define*/
    H2REG(HWIF_CACHE_WR_ENABLE                       , 0x00  ,0x00000001,        0,        0,RW,"Cache write enable"),
    
    H2REG(HWIF_CACHE_WR_BASE_ID                      , 0x04  ,0x00FF0000,        16,        0,RW,"Cache write Bus base id"),
    H2REG(HWIF_CACHE_WR_TIME_OUT                     , 0x04  ,0xFF000000,        24,        0,RW,"Cache write time out threshold"),

    H2REG(HWIF_CACHE_WR_CHN_NUM                     , 0x08  ,0x0000000F,        0,        0,RO,"Cache write supported work channel num"),
    H2REG(HWIF_CACHE_WR_NUM_IDS                     , 0x08  ,0x000000F0,        4,        0,RO,"Cache write the value of LOG2_NUM_IDS"),
    H2REG(HWIF_CACHE_WR_ALIGNMENT                   , 0x08  ,0x0000FF00,        8,        0,RO,"the value of LOG2_ALIGNMENT - 5"),
    H2REG(HWIF_CACHE_WR_S_BUS_BUSY                  , 0x08  ,0x00010000,        16,        0,RO,"slave bus busy"),
    H2REG(HWIF_CACHE_WR_M_BUS_BUSY                  , 0x08  ,0x00020000,        17,        0,RO,"master bus busy"),

    H2REG(HWIF_CACHE_WR_IRQ_TIMEOUT                 , 0x0C  ,0x00000001,        0,        0,RW,"Cache write bus timeout irq"),
    H2REG(HWIF_CACHE_WR_IRQ_ABORTED                 , 0x0C  ,0x00000002,        1,        0,RW,"Cache write aborted irq"),

    H2REG(HWIF_CACHE_WR_IRQ_TIMEOUT_MASK            , 0x10  ,0x00000001,        0,        0,RW,"Cache write bus timeout irq mask"),
    H2REG(HWIF_CACHE_WR_IRQ_ABORTED_MASK            , 0x10  ,0x00000002,        1,        0,RW,"Cache write aborted irq mask"),

    H2REG(HWIF_CACHE_WR_CH_0_VALID                  , 0x14  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_0_STRIPE_E               , 0x14  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_0_PAD_E                  , 0x14  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_0_RFC_E                  , 0x14  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_0_START_ADDR             , 0x14  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_0_BLOCK_E                , 0x18  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_0_START_ADDR_MSB         , 0x18  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_0_LINE_SIZE              , 0x1C  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_0_LINE_STRIDE            , 0x1C  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_0_LINE_CNT               , 0x20  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_0_MAX_H                  , 0x20  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_0_LN_CNT_START           , 0x24  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_0_LN_CNT_MID             , 0x24  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_0_LN_CNT_END             , 0x24  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_0_LN_CNT_STEP            , 0x24  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_1_VALID                  , 0x28  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_1_STRIPE_E               , 0x28  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_1_PAD_E                  , 0x28  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_1_RFC_E                  , 0x28  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_1_START_ADDR             , 0x28  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_1_BLOCK_E                , 0x2C  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_1_START_ADDR_MSB         , 0x2C  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_1_LINE_SIZE              , 0x30  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_1_LINE_STRIDE            , 0x30  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_1_LINE_CNT               , 0x34  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_1_MAX_H                  , 0x34  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_1_LN_CNT_START           , 0x38  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_1_LN_CNT_MID             , 0x38  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_1_LN_CNT_END             , 0x38  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_1_LN_CNT_STEP            , 0x38  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_2_VALID                  , 0x3C  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_2_STRIPE_E               , 0x3C  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"), 
    H2REG(HWIF_CACHE_WR_CH_2_PAD_E                  , 0x3C  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_2_RFC_E                  , 0x3C  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_2_START_ADDR             , 0x3C  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_2_BLOCK_E                , 0x40  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_2_START_ADDR_MSB         , 0x40  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_2_LINE_SIZE              , 0x44  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_2_LINE_STRIDE            , 0x44  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_2_LINE_CNT               , 0x48  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_2_MAX_H                  , 0x48  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_2_LN_CNT_START           , 0x4C  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_2_LN_CNT_MID             , 0x4C  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_2_LN_CNT_END             , 0x4C  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_2_LN_CNT_STEP            , 0x4C  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_3_VALID                  , 0x50  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_3_STRIPE_E               , 0x50  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_3_PAD_E                  , 0x50  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_3_RFC_E                  , 0x50  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_3_START_ADDR             , 0x50  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_3_BLOCK_E                , 0x54  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_3_START_ADDR_MSB         , 0x54  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_3_LINE_SIZE              , 0x58  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_3_LINE_STRIDE            , 0x58  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_3_LINE_CNT               , 0x5C  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_3_MAX_H                  , 0x5C  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_3_LN_CNT_START           , 0x60  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_3_LN_CNT_MID             , 0x60  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_3_LN_CNT_END             , 0x60  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_3_LN_CNT_STEP            , 0x60  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_4_VALID                  , 0x64  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_4_STRIPE_E               , 0x64  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_4_PAD_E                  , 0x64  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_4_RFC_E                  , 0x64  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_4_START_ADDR             , 0x64  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_4_BLOCK_E                , 0x68  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_4_START_ADDR_MSB         , 0x68  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_4_LINE_SIZE              , 0x6C  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_4_LINE_STRIDE            , 0x6C  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_4_LINE_CNT               , 0x70  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_4_MAX_H                  , 0x70  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_4_LN_CNT_START           , 0x74  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_4_LN_CNT_MID             , 0x74  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_4_LN_CNT_END             , 0x74  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_4_LN_CNT_STEP            , 0x74  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_5_VALID                  , 0x78  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_5_STRIPE_E               , 0x78  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_5_PAD_E                  , 0x78  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_5_RFC_E                  , 0x78  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_5_START_ADDR             , 0x78  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_5_BLOCK_E                , 0x7C  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_5_START_ADDR_MSB         , 0x7C  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_5_LINE_SIZE              , 0x80  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_5_LINE_STRIDE            , 0x80  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_5_LINE_CNT               , 0x84  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_5_MAX_H                  , 0x84  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_5_LN_CNT_START           , 0x88  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_5_LN_CNT_MID             , 0x88  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_5_LN_CNT_END             , 0x88  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_5_LN_CNT_STEP            , 0x88  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_6_VALID                  , 0x8C  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_6_STRIPE_E               , 0x8C  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_6_PAD_E                  , 0x8C  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_6_RFC_E                  , 0x8C  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_6_START_ADDR             , 0x8C  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_6_BLOCK_E                , 0x90  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_6_START_ADDR_MSB         , 0x90  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_6_LINE_SIZE              , 0x94  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_6_LINE_STRIDE            , 0x94  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_6_LINE_CNT               , 0x98  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_6_MAX_H                  , 0x98  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_6_LN_CNT_START           , 0x9C  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_6_LN_CNT_MID             , 0x9C  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_6_LN_CNT_END             , 0x9C  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_6_LN_CNT_STEP            , 0x9C  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_7_VALID                  , 0xA0  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_7_STRIPE_E               , 0xA0  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_7_PAD_E                  , 0xA0  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_7_RFC_E                  , 0xA0  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_7_START_ADDR             , 0xA0  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_7_BLOCK_E                , 0xA4  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_7_START_ADDR_MSB         , 0xA4  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_7_LINE_SIZE              , 0xA8  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_7_LINE_STRIDE            , 0xA8  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_7_LINE_CNT               , 0xAC  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_7_MAX_H                  , 0xAC  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_7_LN_CNT_START           , 0xB0  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_7_LN_CNT_MID             , 0xB0  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_7_LN_CNT_END             , 0xB0  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_7_LN_CNT_STEP            , 0xB0  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_8_VALID                  , 0xB4  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_8_STRIPE_E               , 0xB4  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_8_PAD_E                  , 0xB4  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_8_RFC_E                  , 0xB4  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_8_START_ADDR             , 0xB4  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_8_BLOCK_E                , 0xB8  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_8_START_ADDR_MSB         , 0xB8  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_8_LINE_SIZE              , 0xBC  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_8_LINE_STRIDE            , 0xBC  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_8_LINE_CNT               , 0xC0  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_8_MAX_H                  , 0xC0  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_8_LN_CNT_START           , 0xC4  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_8_LN_CNT_MID             , 0xC4  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_8_LN_CNT_END             , 0xC4  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_8_LN_CNT_STEP            , 0xC4  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_9_VALID                  , 0xC8  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_9_STRIPE_E               , 0xC8  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_9_PAD_E                  , 0xC8  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_9_RFC_E                  , 0xC8  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_9_START_ADDR             , 0xC8  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_9_BLOCK_E                , 0xCC  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_9_START_ADDR_MSB         , 0xCC  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_9_LINE_SIZE              , 0xD0  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_9_LINE_STRIDE            , 0xD0  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_9_LINE_CNT               , 0xD4  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_9_MAX_H                  , 0xD4  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_9_LN_CNT_START           , 0xD8  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_9_LN_CNT_MID             , 0xD8  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_9_LN_CNT_END             , 0xD8  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_9_LN_CNT_STEP            , 0xD8  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_10_VALID                 , 0xDC  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_10_STRIPE_E              , 0xDC  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_10_PAD_E                 , 0xDC  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_10_RFC_E                 , 0xDC  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_10_START_ADDR            , 0xDC  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_10_BLOCK_E               , 0xE0  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_10_START_ADDR_MSB        , 0xE0  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_10_LINE_SIZE             , 0xE4  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_10_LINE_STRIDE           , 0xE4  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_10_LINE_CNT              , 0xE8  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_10_MAX_H                 , 0xE8  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_10_LN_CNT_START          , 0xEC  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_10_LN_CNT_MID            , 0xEC  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_10_LN_CNT_END            , 0xEC  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_10_LN_CNT_STEP           , 0xEC  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_11_VALID                 , 0xF0  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_11_STRIPE_E              , 0xF0  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_11_PAD_E                 , 0xF0  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_11_RFC_E                 , 0xF0  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_11_START_ADDR            , 0xF0  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_11_BLOCK_E               , 0xF4  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_11_START_ADDR_MSB        , 0xF4  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_11_LINE_SIZE             , 0xF8  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_11_LINE_STRIDE           , 0xF8  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_11_LINE_CNT              , 0xFC  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_11_MAX_H                 , 0xFC  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_11_LN_CNT_START          , 0x100  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_11_LN_CNT_MID            , 0x100  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_11_LN_CNT_END            , 0x100  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_11_LN_CNT_STEP           , 0x100  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_12_VALID                 , 0x104  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_12_STRIPE_E              , 0x104  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_12_PAD_E                 , 0x104  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_12_RFC_E                 , 0x104  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_12_START_ADDR            , 0x104  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_12_BLOCK_E               , 0x108  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_12_START_ADDR_MSB        , 0x108  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_12_LINE_SIZE             , 0x10C  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_12_LINE_STRIDE           , 0x10C  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_12_LINE_CNT              , 0x110  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_12_MAX_H                 , 0x110  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_12_LN_CNT_START          , 0x114  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_12_LN_CNT_MID            , 0x114  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_12_LN_CNT_END            , 0x114  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_12_LN_CNT_STEP           , 0x114  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_13_VALID                 , 0x118  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_13_STRIPE_E              , 0x118  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_13_PAD_E                 , 0x118  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_13_RFC_E                 , 0x118  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_13_START_ADDR            , 0x118  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_13_BLOCK_E               , 0x11C  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_13_START_ADDR_MSB        , 0x11C  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_13_LINE_SIZE             , 0x120  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_13_LINE_STRIDE           , 0x120  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_13_LINE_CNT              , 0x124  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_13_MAX_H                 , 0x124  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_13_LN_CNT_START          , 0x128  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_13_LN_CNT_MID            , 0x128  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_13_LN_CNT_END            , 0x128  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_13_LN_CNT_STEP           , 0x128  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_14_VALID                 , 0x12C  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_14_STRIPE_E              , 0x12C  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_14_PAD_E                 , 0x12C  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_14_RFC_E                 , 0x12C  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_14_START_ADDR            , 0x12C  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_14_BLOCK_E               , 0x130  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_14_START_ADDR_MSB        , 0x130  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_14_LINE_SIZE             , 0x134  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_14_LINE_STRIDE           , 0x134  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_14_LINE_CNT              , 0x138  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_14_MAX_H                 , 0x138  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_14_LN_CNT_START          , 0x13C  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_14_LN_CNT_MID            , 0x13C  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_14_LN_CNT_END            , 0x13C  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_14_LN_CNT_STEP           , 0x13C  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),

    H2REG(HWIF_CACHE_WR_CH_15_VALID                 , 0x140  ,0x00000001,        0,        0,RW,"Cache write channel valid"),
    H2REG(HWIF_CACHE_WR_CH_15_STRIPE_E              , 0x140  ,0x00000002,        1,        0,RW,"Cache write channel stripe enable"),
    H2REG(HWIF_CACHE_WR_CH_15_PAD_E                 , 0x140  ,0x00000004,        2,        0,RW,"Cache write channel padding enable"),
    H2REG(HWIF_CACHE_WR_CH_15_RFC_E                 , 0x140  ,0x00000008,        3,        0,RW,"Cache write channel rfc enable"),
    H2REG(HWIF_CACHE_WR_CH_15_START_ADDR            , 0x140  ,0xFFFFFFF0,        4,        0,RW,"Cache write channel start addr"),

    H2REG(HWIF_CACHE_WR_CH_15_BLOCK_E               , 0x144  ,0x00000001,        0,        0,RW,"Cache write channel block enable"),
    H2REG(HWIF_CACHE_WR_CH_15_START_ADDR_MSB        , 0x144  ,0xFFFFFFFF,        0,        0,RW,"Cache write channel start addr MSB"),

    H2REG(HWIF_CACHE_WR_CH_15_LINE_SIZE             , 0x148  ,0x0000FFFF,        0,        0,RW,"Cache write channel line size"),
    H2REG(HWIF_CACHE_WR_CH_15_LINE_STRIDE           , 0x148  ,0xFFFF0000,        16,        0,RW,"Cache write channel line stride"),

    H2REG(HWIF_CACHE_WR_CH_15_LINE_CNT              , 0x14C  ,0x0000FFFF,        0,        0,RW,"Cache write channel line cnt"),
    H2REG(HWIF_CACHE_WR_CH_15_MAX_H                 , 0x14C  ,0xFFFF0000,        16,        0,RW,"Cache write channel max_h"),

    H2REG(HWIF_CACHE_WR_CH_15_LN_CNT_START          , 0x150  ,0x000000FF,        0,        0,RW,"Cache write channel line cnt start"),
    H2REG(HWIF_CACHE_WR_CH_15_LN_CNT_MID            , 0x150  ,0x0000FF00,        8,        0,RW,"Cache write channel line cnt mid"),
    H2REG(HWIF_CACHE_WR_CH_15_LN_CNT_END            , 0x150  ,0x00FF0000,        16,       0,RW,"Cache write channel line cnt end"),
    H2REG(HWIF_CACHE_WR_CH_15_LN_CNT_STEP           , 0x150  ,0xFF000000,        24,       0,RW,"Cache write channel line cnt step"),
