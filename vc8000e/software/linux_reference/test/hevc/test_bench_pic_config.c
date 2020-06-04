
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "base_type.h"
#include "hevcencapi.h"
#include "HevcTestBench.h"
#include "test_bench_utils.h"
//#include "instance.h"

#include "test_bench_pic_config.h"

/*------------------------------------------------------------------------------
Function name : InitPicConfig
Description   : initial pic reference configure
Return type   : void
Argument      : VCEncIn *pEncIn
------------------------------------------------------------------------------*/
void InitPicConfig(VCEncIn *pEncIn, struct test_bench *tb, commandLine_s *cml)
{
    i32 i, j, k, i32Poc;
    i32 i32MaxpicOrderCntLsb = 1 << 16;

    ASSERT(pEncIn != NULL);

    pEncIn->gopCurrPicConfig.codingType = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.numRefPics = NUMREFPICS_RESERVED;
    pEncIn->gopCurrPicConfig.poc = -1;
    pEncIn->gopCurrPicConfig.QpFactor = QPFACTOR_RESERVED;
    pEncIn->gopCurrPicConfig.QpOffset = QPOFFSET_RESERVED;
    pEncIn->gopCurrPicConfig.temporalId = 0;
    pEncIn->i8SpecialRpsIdx = -1;
    for (k = 0; k < VCENC_MAX_REF_FRAMES; k++)
    {
        pEncIn->gopCurrPicConfig.refPics[k].ref_pic     = INVALITED_POC;
        pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = 0;
    }

    for (k = 0; k < VCENC_MAX_LT_REF_FRAMES; k++)
        pEncIn->long_term_ref_pic[k] = INVALITED_POC;

    pEncIn->bIsPeriodUsingLTR = HANTRO_FALSE;
    pEncIn->bIsPeriodUpdateLTR = HANTRO_FALSE;

    for (i = 0; i < pEncIn->gopConfig.special_size; i++)
    {
        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Interval <= 0)
            continue;

        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr == 0)
            pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
        else
        {
            pEncIn->bIsPeriodUpdateLTR = HANTRO_TRUE;
            
            for (k = 0; k < (i32)pEncIn->gopConfig.pGopPicSpecialCfg[i].numRefPics; k++)
            {
                i32 i32LTRIdx = pEncIn->gopConfig.pGopPicSpecialCfg[i].refPics[k].ref_pic;
                if ((IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) && ((pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr-1) == LONG_TERM_REF_DELTAPOC2ID(i32LTRIdx)))
                {
                    pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
                }
            }
        }
    }

    if(( IS_AV1(cml->codecFormat)) && (pEncIn->gopConfig.special_size == 0))
    {
        i32 tmp_gopsize = cml->gopSize > 0? cml->gopSize : MAX_GOP_SIZE;
        for(i32 gopSize=1; gopSize <= tmp_gopsize ; gopSize++){
            if((pEncIn->gopConfig.gopCfgOffset[gopSize] == pEncIn->gopConfig.gopCfgOffset[gopSize+1]) && (gopSize < MAX_GOP_SIZE))
                continue;
            for(i = 0; i < gopSize; i++){
                i32 offset = pEncIn->gopConfig.gopCfgOffset[gopSize] + i;
                for (k = 0; k < (i32)pEncIn->gopConfig.pGopPicCfg[offset].numRefPics; k++)
                {
                    i32 i32LTRIdx = pEncIn->gopConfig.pGopPicCfg[offset].refPics[k].ref_pic;
                    if ((!IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) && (ABS(i32LTRIdx) > gopSize))
                    {
                        pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
                        break;
                    }
                }

                if(pEncIn->bIsPeriodUsingLTR == HANTRO_TRUE)
                    break;
            }
            if(pEncIn->bIsPeriodUsingLTR == HANTRO_TRUE)
                break;
        }

        if((cml->gopLowdelay) && (tmp_gopsize == 1))
            pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
    }


    memset(pEncIn->bLTR_need_update, 0, sizeof(bool)*VCENC_MAX_LT_REF_FRAMES);
    pEncIn->bIsIDR       = HANTRO_TRUE;

    i32Poc = 0;
    /* check current picture encoded as LTR*/
    pEncIn->u8IdxEncodedAsLTR = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++)
    {
        if (pEncIn->bIsPeriodUsingLTR == HANTRO_FALSE)
            break;

        if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) || (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
            continue;

        i32Poc = i32Poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

        if (i32Poc < 0)
        {
            i32Poc += i32MaxpicOrderCntLsb;
            if (i32Poc >(i32MaxpicOrderCntLsb >> 1))
                i32Poc = -1;
        }

        if ((i32Poc >= 0) && (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
        {
            /* more than one LTR at the same frame position */
            if (0 != pEncIn->u8IdxEncodedAsLTR)
            {
                // reuse the same POC LTR
                pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1] = HANTRO_TRUE;
                continue;
            }

            pEncIn->gopCurrPicConfig.codingType = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) ? pEncIn->gopCurrPicConfig.codingType : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
            pEncIn->gopCurrPicConfig.numRefPics = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED) ? pEncIn->gopCurrPicConfig.numRefPics : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
            pEncIn->gopCurrPicConfig.QpFactor = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) ? pEncIn->gopCurrPicConfig.QpFactor : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
            pEncIn->gopCurrPicConfig.QpOffset = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED) ? pEncIn->gopCurrPicConfig.QpOffset : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
            pEncIn->gopCurrPicConfig.temporalId = (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED) ? pEncIn->gopCurrPicConfig.temporalId : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

            if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics != NUMREFPICS_RESERVED))
            {
                for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++)
                {
                    pEncIn->gopCurrPicConfig.refPics[k].ref_pic = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
                    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
                }
            }

            pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
            pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
        }
    }
    u32 gopSize = tb->gopSize;
    bool adaptiveGop = (gopSize == 0);

    pEncIn->timeIncrement = 0;
    pEncIn->vui_timing_info_enable = cml->vui_timing_info_enable;
    pEncIn->hashType = cml->hashtype;
    pEncIn->poc = 0;
    //default gop size as IPPP
    pEncIn->gopSize =  (adaptiveGop ? (cml->lookaheadDepth ? 4 : 1) : gopSize);
    pEncIn->last_idr_picture_cnt = pEncIn->picture_cnt = 0;
}
