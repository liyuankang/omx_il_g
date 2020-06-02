/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract  : 
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "EncJpeg.h"
#include "EncJpegDhtTables.h"
#include "EncJpegMarkers.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

static const u8 zigzag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void EncJpegAPP0Header(stream_s * stream, jpegData_s * data);
static void EncJpegDQTHeader(stream_s * stream, jpegData_s *);
static void EncJpegCOMHeader(stream_s * stream, jpegData_s *);
static void EncJpegRestartInterval(stream_s * stream, jpegData_s *);
static void EncJpegSOFOHeader(stream_s * stream, jpegData_s *);
static void EncJpegDHTHeader(stream_s * stream, jpegData_s *);
static void EncJpegSOSHeader(stream_s * stream, jpegData_s *);

/*------------------------------------------------------------------------------

    EncJpegInit

------------------------------------------------------------------------------*/
void EncJpegInit(jpegData_s * jpeg)
{
    jpeg->header = ENCHW_NO;
    jpeg->restart.Ri = 0;   /* Restart interval, number of MCUs */
    jpeg->frame.Nf = 3; /* Number of color components in a frame */
    jpeg->rstCount = 0;
    jpeg->sliceNum = 0;
    jpeg->codingMode = JPEGENC_420_MODE;
    jpeg->codingType = ENC_WHOLE_FRAME;
    jpeg->markerType = ENC_SINGLE_MARKER;
    jpeg->appn.units = ENC_NO_UNITS;
    jpeg->appn.Xdensity = 1;
    jpeg->appn.Ydensity = 1;
    jpeg->com.comEnable = 0;
}

/*------------------------------------------------------------------------------

    Function name: EncJpegHdr

    Functional description:

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
u32 EncJpegHdr(stream_s * stream, jpegData_s * data)
{
    data->frame.Y = (u32) data->height;
    data->frame.X = (u32) data->width;

    if(data->frame.header == ENCHW_YES)
    {
        /* SOI */
        EncJpegHeaderPutBits(stream, SOI, 16);
        COMMENT(stream,"Start-Of-Image");
    }

    /* APP0 header */
    EncJpegAPP0Header(stream, data);

    if(data->frame.header == ENCHW_YES)
    {
        if(data->com.comEnable)
        {
            /* Com header */
            EncJpegCOMHeader(stream, data);
        }

        /* Quant header */
        if (!data->losslessEn)
            EncJpegDQTHeader(stream, data);

        /* Frame header */
        EncJpegSOFOHeader(stream, data);

        /* Restart interval */
        EncJpegRestartInterval(stream, data);

        /* Huffman header */
        EncJpegDHTHeader(stream, data);
    }

    /* Scan header */
    EncJpegSOSHeader(stream, data);

    return (ENCHW_OK);
}
#if 0
/*------------------------------------------------------------------------------

    Function name: EncJpegImageEnd

    Functional description:

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegImageEnd(stream_s * stream, jpegData_s * data)
{
    /* write EOI to stream */
    stream->stream[0] = 0xFF;
    stream->stream[1] = 0xD9;
    stream->byteCnt += 2;
    stream->stream++;
    stream->stream++;
    stream->bitCnt += 16;

    /* take care of next putbits */
    if(data->appn.thumbMode)
    {
        stream->stream[0] = 0x0;
        stream->stream[1] = 0x0;
    }

    COMMENT(stream,"EOI");
}
#endif
#if 0
/*------------------------------------------------------------------------------

    Function name: EncJpegImageEndReplaceRst

    Functional description: write EOI over hw set RST

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegImageEndReplaceRst(stream_s * stream, jpegData_s * data)
{
    COMMENT(stream,"Replace RST with EOI (slice mode)");

    /* write EOI to stream */
    stream->stream[0] = 0xFF;
    stream->stream[1] = 0xD9;
    stream->stream++;
    stream->stream++;

    /* take care of next putbits */
    if(data->appn.thumbMode)
    {
        stream->stream[0] = 0x0;
        stream->stream[1] = 0x0;
    }

    COMMENT(stream,"RST ==> EOI");
}
#endif
/*------------------------------------------------------------------------------

    Function name: JpegEncAPP0Header

    Functional description: Sets APP0 header data

    Inputs: 

    Outputs: 

------------------------------------------------------------------------------*/
void EncJpegAPP0Header(stream_s * stream, jpegData_s * data)
{
    EncJpegHeaderPutBits(stream, APP0, 16);
    COMMENT(stream,"APP0");

    EncJpegHeaderPutBits(stream, 0x0010, 16);
    COMMENT(stream,"Length");

    /* "JFIF" ID */
    EncJpegHeaderPutBits(stream, 0x4A46, 16);
    COMMENT(stream,"Ident1");
    EncJpegHeaderPutBits(stream, 0x4946, 16);
    COMMENT(stream,"Ident2");
    EncJpegHeaderPutBits(stream, 0x00, 8);
    COMMENT(stream,"Ident3");
    EncJpegHeaderPutBits(stream, 0x0102, 16);
    COMMENT(stream,"Version");
    if(data->appn.Xdensity && data->appn.Ydensity)
    {
        EncJpegHeaderPutBits(stream, data->appn.units, 8);
        COMMENT(stream,"Units");
        EncJpegHeaderPutBits(stream, data->appn.Xdensity, 16);
        COMMENT(stream,"Xdensity");
        EncJpegHeaderPutBits(stream, data->appn.Ydensity, 16);
        COMMENT(stream,"Ydensity");
    }
    else
    {
        EncJpegHeaderPutBits(stream, 0x00, 8);
        COMMENT(stream,"Units");
        EncJpegHeaderPutBits(stream, 0x0001, 16);
        COMMENT(stream,"Xdensity");
        EncJpegHeaderPutBits(stream, 0x0001, 16);
        COMMENT(stream,"Ydensity");
    }

    EncJpegHeaderPutBits(stream, 0x00, 8);
    COMMENT(stream,"XThumbnail");
    EncJpegHeaderPutBits(stream, 0x00, 8);
    COMMENT(stream,"YThumbnail");

    /* APP0 extension header ==> thumbnail */
    if(data->appn.thumbEnable)
    {
        u32 length;       
        const JpegEncThumb * thumb = &data->thumbnail;
        const u8 * thumbData = (u8*)thumb->data;

        EncJpegHeaderPutBits(stream, APP0, 16);
        COMMENT(stream,"APP0 Extended");

        /* Length of APP0 field */
        length = 8 + thumb->dataLength;
        if(thumb->format != JPEGENC_THUMB_JPEG)
        {
            length += 2; /* 2 bytes for the size */
        }

        EncJpegHeaderPutBits(stream, length, 16);
        COMMENT(stream,"Length");
        /* "JFXX" ID */
        EncJpegHeaderPutBits(stream, 0x4A46, 16);
        COMMENT(stream,"Ident1");
        EncJpegHeaderPutBits(stream, 0x5858, 16);
        COMMENT(stream,"Ident2");
        EncJpegHeaderPutBits(stream, 0x00, 8);
        COMMENT(stream,"Ident3");

        EncJpegHeaderPutBits(stream, (u32)thumb->format, 8);
        COMMENT(stream,"Extension code");

        if(thumb->format != JPEGENC_THUMB_JPEG)
        {            
            EncJpegHeaderPutBits(stream, thumb->width, 8);
            COMMENT(stream,"Xthumbnail");
            EncJpegHeaderPutBits(stream, thumb->height, 8);
            COMMENT(stream,"Ythumbnail");
        }

        for(length = thumb->dataLength; length > 0; length--)
        {
            EncJpegHeaderPutBits(stream, *thumbData, 8);
            thumbData++;
        }
        COMMENT(stream,"Extension data");

        data->appn.thumbEnable = 0; /* was valid only for one picture */
    }    
}

/*------------------------------------------------------------------------------

    Function name: EncJpegCOMHeader

    Functional description: Sets COM header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegCOMHeader(stream_s * stream, jpegData_s * data)
{
    u32 j;

    EncJpegHeaderPutBits(stream, COM, 16);
    COMMENT(stream,"COM");

    EncJpegHeaderPutBits(stream, 2 + data->com.comLen, 16);
    COMMENT(stream,"Lc");

    for(j = 0; j < data->com.comLen; j++)
    {
        /* Qk table 0 */
        EncJpegHeaderPutBits(stream, data->com.pComment[j], 8);
        COMMENT(stream,"COM data");
    }
}

/*------------------------------------------------------------------------------

    Function name: EncJpegDQTHeader

    Functional description: Sets DQT header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegDQTHeader(stream_s * stream, jpegData_s * data)
{
    u32 j;

    EncJpegHeaderPutBits(stream, DQT, 16);
    COMMENT(stream,"DQT");

    if(!data->markerType || data->frame.Nf == 1)
    {
        EncJpegHeaderPutBits(stream, 2 + 65, 16);
        COMMENT(stream,"Lq");
    }
    else
    {
        EncJpegHeaderPutBits(stream, (2 + (65 * 2)), 16);
        COMMENT(stream,"Lq");
    }

    EncJpegHeaderPutBits(stream, 0, 4);
    COMMENT(stream,"Pq");
    EncJpegHeaderPutBits(stream, 0, 4);
    COMMENT(stream,"Tq");

    for(j = 0; j < 64; j++)
    {
        /* Qk table 0 */
        EncJpegHeaderPutBits(stream, data->qTable.pQlumi[zigzag[j]], 8);
        COMMENT(stream,"Qk");
    }

    if(data->frame.Nf > 1)
    {
        if(!data->markerType)
        {
            EncJpegHeaderPutBits(stream, DQT, 16);
            COMMENT(stream,"DQT");

            EncJpegHeaderPutBits(stream, 2 + 65, 16);
            COMMENT(stream,"Lq");
        }

        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"Pq");
        EncJpegHeaderPutBits(stream, 1, 4);
        COMMENT(stream,"Tq");

        for(j = 0; j < 64; j++)
        {
            /* Qk table 1 */
            EncJpegHeaderPutBits(stream, data->qTable.pQchromi[zigzag[j]], 8);
            COMMENT(stream,"Qk");
        }
    }
}

/*------------------------------------------------------------------------------

    Function name: EncJpegRestartInterval

    Functional description: Sets DRI header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegRestartInterval(stream_s * stream, jpegData_s * data)
{
    if(data->restart.Ri != 0)
    {
        EncJpegHeaderPutBits(stream, DRI, 16);
        COMMENT(stream,"DRI");

        data->restart.Lr = 4;

        EncJpegHeaderPutBits(stream, data->restart.Lr, 16);
        COMMENT(stream,"Lr");
        EncJpegHeaderPutBits(stream, data->restart.Ri, 16);
        COMMENT(stream,"Rq");
    }
}

/*------------------------------------------------------------------------------

    Function name: EncJpegFrameHeader

    Functional description: Sets Frame header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegSOFOHeader(stream_s * stream, jpegData_s * data)
{
    u32 i;

    ASSERT(data->frame.Nf <= MAX_NUMBER_OF_COMPONENTS);

    
    if (data->losslessEn)
    {
        /* SOF3  */
        EncJpegHeaderPutBits(stream, SOF3, 16);
        COMMENT(stream,"SOF3");
    }
    else
    {
        /* SOF0  */
        EncJpegHeaderPutBits(stream, SOF0, 16);
        COMMENT(stream,"SOF0");
    }
    
    /* Frame header */
    data->frame.Lf = (8 + (3 * data->frame.Nf));
    data->frame.P = 8;

    EncJpegHeaderPutBits(stream, data->frame.Lf, 16);
    COMMENT(stream,"Lf");
    EncJpegHeaderPutBits(stream, data->frame.P, 8);
    COMMENT(stream,"P");
    EncJpegHeaderPutBits(stream, data->frame.Y, 16);
    COMMENT(stream,"Y");
    EncJpegHeaderPutBits(stream, data->frame.X, 16);
    COMMENT(stream,"X");
    EncJpegHeaderPutBits(stream, data->frame.Nf, 8);
    COMMENT(stream,"Nf");

    /* Only 1 component, grayscale */
    if(data->frame.Nf == 1)
    {
        data->frame.Ci[0] = 1;
        data->frame.Hi[0] = 1;
        data->frame.Vi[0] = 1;
        data->frame.Tqi[0] = 0;
    }

    /* 3 components */
    if(data->frame.Nf == 3)
    {
        if (data->codingMode == JPEGENC_420_MODE)
        {
            /* YUV 4:2:0 */
            data->frame.Ci[0] = 1;
            data->frame.Hi[0] = 2;
            data->frame.Vi[0] = 2;

            data->frame.Ci[1] = 2;
            data->frame.Hi[1] = 1;
            data->frame.Vi[1] = 1;

            data->frame.Ci[2] = 3;
            data->frame.Hi[2] = 1;
            data->frame.Vi[2] = 1;
            if (data->losslessEn)
            {
                data->frame.Tqi[0] = 0;
                data->frame.Tqi[1] = 0;
                data->frame.Tqi[2] = 0;
            }
            else
            {
                data->frame.Tqi[0] = 0;
                data->frame.Tqi[1] = 1;
                data->frame.Tqi[2] = 1;
            }
        }
        else
        {
            /* YUV 4:2:2, MCU = 2 luma blocks + cb block + cr block */
            data->frame.Ci[0] = 1;
            data->frame.Hi[0] = 2;
            data->frame.Vi[0] = 1;

            data->frame.Ci[1] = 2;
            data->frame.Hi[1] = 1;
            data->frame.Vi[1] = 1;

            data->frame.Ci[2] = 3;
            data->frame.Hi[2] = 1;
            data->frame.Vi[2] = 1;
            if (data->losslessEn)
            {
                data->frame.Tqi[0] = 0;
                data->frame.Tqi[1] = 0;
                data->frame.Tqi[2] = 0;
            }
            else
            {
                data->frame.Tqi[0] = 0;
                data->frame.Tqi[1] = 1;
                data->frame.Tqi[2] = 1;
            }
        }
    }

    for(i = 0; i < data->frame.Nf; i++)
    {
        EncJpegHeaderPutBits(stream, data->frame.Ci[i], 8);
        COMMENT(stream,"Ci");
        EncJpegHeaderPutBits(stream, data->frame.Hi[i], 4);
        COMMENT(stream,"Hi");
        EncJpegHeaderPutBits(stream, data->frame.Vi[i], 4);
        COMMENT(stream,"Vi");
        EncJpegHeaderPutBits(stream, data->frame.Tqi[i], 8);
        COMMENT(stream,"Tqi");
    }
}

/*------------------------------------------------------------------------------

    Function name: EncJpegDHTHeader

    Functional description: Sets DHT header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegDHTHeader(stream_s * stream, jpegData_s * data)
{
    u32 dc_lum, dc_chrom;
    u32 ac_lum, ac_chrom;
    u32 dc_vij, ac_vij;
    u32 length;

    ASSERT(data->frame.Nf <= MAX_NUMBER_OF_COMPONENTS);

    if(data->frame.Nf == 1)
    {
        /* DHT  */
        EncJpegHeaderPutBits(stream, DHT, 16);
        COMMENT(stream,"DHT");

        /* Lh */
        if (data->losslessEn)
        {
            length = 2 + ((17 * 1) + ((1 * 12) ));
        }
        else
        {
            length = 2 + ((17 * 2) + ((1 * 12) + (1 * 162)));
        }
        EncJpegHeaderPutBits(stream, length, 16);
        COMMENT(stream,"Lh");

        /* Huffman tables for luminance DC and AC components */

        /* TC */
        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"TC");
        /* TH */
        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"TH");

        for(dc_lum = 0; dc_lum < 16; dc_lum++)
        {
            EncJpegHeaderPutBits(stream, Dc_Li[dc_lum].DcLumLi, 8);
            COMMENT(stream,"Dc_Li");
        }

        for(dc_vij = 0; dc_vij < 12; dc_vij++)
        {
            EncJpegHeaderPutBits(stream, Vij_Dc[dc_vij].DcLumVij, 8);
            COMMENT(stream,"Vij_Dc");
        }

        if (!data->losslessEn)
        {
            /* TC */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"TC");
            /* TH */
            EncJpegHeaderPutBits(stream, 0, 4);
            COMMENT(stream,"TH");

            for(ac_lum = 0; ac_lum < 16; ac_lum++)
            {
                EncJpegHeaderPutBits(stream, Ac_Li[ac_lum].AcLumLi, 8);
                COMMENT(stream,"Ac_Li");
            }

            for(ac_vij = 0; ac_vij < 162; ac_vij++)
            {
                EncJpegHeaderPutBits(stream, Vij_Ac[ac_vij].AcLumVij, 8);
                COMMENT(stream,"Vij_Ac");
            }
        }
    }
    else
    {
        /* DHT  */
        EncJpegHeaderPutBits(stream, DHT, 16);
        COMMENT(stream,"DHT");

        /* Huffman table for luminance DC components */

        /* Lh */
        if(!data->markerType)
        {
            EncJpegHeaderPutBits(stream, 2 + ((17 * 1) + ((1 * 12))), 16);
            COMMENT(stream,"Lh");
        }
        else
        {
            if (data->losslessEn)
            {
                length = (2 + ((17 * 2) + ((2 * 12) )));
            }
            else
            {
                length = (2 + ((17 * 4) + ((2 * 12) + (2 * 162))));
            }
            EncJpegHeaderPutBits(stream, length, 16);
            COMMENT(stream,"Lh");
        }

        /* TC */
        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"TC");
        /* TH */
        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"TH");

        for(dc_lum = 0; dc_lum < 16; dc_lum++)
        {
            EncJpegHeaderPutBits(stream, Dc_Li[dc_lum].DcLumLi, 8);
            COMMENT(stream,"Dc_Li");
        }

        for(dc_vij = 0; dc_vij < 12; dc_vij++)
        {
            EncJpegHeaderPutBits(stream, Vij_Dc[dc_vij].DcLumVij, 8);
            COMMENT(stream,"Vij_Dc");
        }

        if (!data->losslessEn)
        {
            if (!data->markerType)
            {
                /* DHT  */
                EncJpegHeaderPutBits(stream, DHT, 16);
                COMMENT(stream,"DHT");

                /* Huffman table for luminance AC components */

                /* Lh */
                EncJpegHeaderPutBits(stream, 2 + ((17 * 1) + ((1 * 162))), 16);
                COMMENT(stream,"Lh");
            }

            /* TC */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"TC");
            /* TH */
            EncJpegHeaderPutBits(stream, 0, 4);
            COMMENT(stream,"TH");

            for(ac_lum = 0; ac_lum < 16; ac_lum++)
            {
                EncJpegHeaderPutBits(stream, Ac_Li[ac_lum].AcLumLi, 8);
                COMMENT(stream,"Ac_Li");
            }

            for(ac_vij = 0; ac_vij < 162; ac_vij++)
            {
                EncJpegHeaderPutBits(stream, Vij_Ac[ac_vij].AcLumVij, 8);
                COMMENT(stream,"Vij_Ac");
            }
        }

        /* Huffman table for chrominance DC components */

        if(!data->markerType)
        {
            /* DHT  */
            EncJpegHeaderPutBits(stream, DHT, 16);
            COMMENT(stream,"DHT");

            /* Lh */
            EncJpegHeaderPutBits(stream, 2 + ((17 * 1) + ((1 * 12))), 16);
            COMMENT(stream,"Lh");
        }

        /* TC */
        EncJpegHeaderPutBits(stream, 0, 4);
        COMMENT(stream,"TC");
        /* TH */
        EncJpegHeaderPutBits(stream, 1, 4);
        COMMENT(stream,"TH");

        for(dc_chrom = 0; dc_chrom < 16; dc_chrom++)
        {
            EncJpegHeaderPutBits(stream, Dc_Li[dc_chrom].DcChromLi, 8);
            COMMENT(stream,"Dc_Li");
        }

        for(dc_vij = 0; dc_vij < 12; dc_vij++)
        {
            EncJpegHeaderPutBits(stream, Vij_Dc[dc_vij].DcChromVij, 8);
            COMMENT(stream,"Vij_Dc");
        }

        /* Huffman table for chrominance AC components */
        if (!data->losslessEn)
        {
            if(!data->markerType)
            {
                /* DHT  */
                EncJpegHeaderPutBits(stream, DHT, 16);
                COMMENT(stream,"DHT");

                /* Lh */
                EncJpegHeaderPutBits(stream, 2 + ((17 * 1) + ((1 * 162))), 16);
                COMMENT(stream,"Lh");
            }

            /* TC */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"TC");
            /* TH */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"TH");

            for(ac_chrom = 0; ac_chrom < 16; ac_chrom++)
            {
                EncJpegHeaderPutBits(stream, Ac_Li[ac_chrom].AcChromLi, 8);
                COMMENT(stream,"Ac_Li");
            }

            for(ac_vij = 0; ac_vij < 162; ac_vij++)
            {
                EncJpegHeaderPutBits(stream, Vij_Ac[ac_vij].AcChromVij, 8);
                COMMENT(stream,"Vij_Ac");
            }
        }
    }
}

/*------------------------------------------------------------------------------

    Function name: EncJpegSOSHeader

    Functional description: Sets SOS header data

    Inputs:

    Outputs:

------------------------------------------------------------------------------*/
void EncJpegSOSHeader(stream_s * stream, jpegData_s * data)
{
    u32 i;
    u32 Ns, Ls;

    /* SOS  */
    EncJpegHeaderPutBits(stream, SOS, 16);
    COMMENT(stream,"SOS");

    Ns = data->frame.Nf;
    Ls = (6 + (2 * Ns));

    EncJpegHeaderPutBits(stream, Ls, 16);
    COMMENT(stream,"Ls");
    EncJpegHeaderPutBits(stream, Ns, 8);
    COMMENT(stream,"Ns");

    for(i = 0; i < Ns; i++)
    {
        /* Csj */
        EncJpegHeaderPutBits(stream, i + 1, 8);
        COMMENT(stream,"Csj");

        if(i == 0)
        {
            /* Tdj */
            EncJpegHeaderPutBits(stream, 0, 4);
            COMMENT(stream,"Tdj");
            /* Taj */
            EncJpegHeaderPutBits(stream, 0, 4);
            COMMENT(stream,"Taj");
        }
        else
        {
            /* Tdj */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"Tdj");
            /* Taj */
            EncJpegHeaderPutBits(stream, 1, 4);
            COMMENT(stream,"Taj");
        }
    }

    /* Ss */
    if (data->losslessEn)
    {
        EncJpegHeaderPutBits(stream, data->predictMode, 8);
    }
    else
    {
        EncJpegHeaderPutBits(stream, 0, 8);
    }
    COMMENT(stream,"Ss");
    /* Se */
    if (data->losslessEn)
    {
        EncJpegHeaderPutBits(stream, 0, 8);
    }
    else
    {
        EncJpegHeaderPutBits(stream, 63, 8);
    }
    COMMENT(stream,"Se");
    /* Ah */
    EncJpegHeaderPutBits(stream, 0, 4);
    COMMENT(stream,"Ah");
    /* Al */
    if (data->losslessEn)
    {
        EncJpegHeaderPutBits(stream, data->ptransValue, 4);
    }
    else
    {
        EncJpegHeaderPutBits(stream, 0, 4);
    }
    COMMENT(stream,"Al");
}
