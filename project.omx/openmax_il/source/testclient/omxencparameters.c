/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include <OMX_Component.h>
#include <OMX_Video.h>
#include <OMX_Image.h>

#include <string.h>
#include <stdlib.h>

#include <util.h>

#include "omxencparameters.h"
#include "omxtestcommon.h"

/*
    Macro that is used specifically to check parameter values
    in parameter checking loop.
 */
#define OMXENCODER_CHECK_NEXT_VALUE(i, args, argc, msg) \
    if(++(i) == argc || (*((args)[i])) == '-')  { \
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, msg); \
        return OMX_ErrorBadParameter; \
    }

#define OMXENCODER_CHECK_NEXT_VALUE_SIGNED(i, args, argc, msg) \
    if(++(i) == argc)  { \
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, msg); \
        return OMX_ErrorBadParameter; \
    }

int ParseDelim(char *optArg, char delim)
{
    OMX_S32 i;

    for (i = 0; i < (OMX_S32)strlen(optArg); i++)
        if (optArg[i] == delim)
        {
            optArg[i] = 0;
            return i;
        }

    return -1;
}

/*
    print_usage
 */
 
void print_vc8000e_usage();
void print_h2v41_usage();

void print_usage(OMX_STRING swname)
{
    printf("usage: %s [options]\n"
           "\n"
           "  Available options:\n"

#if defined (ENC6280) || defined (ENC7280)
           "    -O, --outputFormat               Compression formats; 'avc', 'mpeg4', 'h263' or 'jpeg'\n"
#elif defined (ENCH1)
           "    -O, --outputFormat               Compression formats; 'avc', 'vp8', 'webp' or 'jpeg'\n"
#elif defined (ENCH2)
           "    -O, --outputFormat               Compression format; 'hevc'\n"
#elif defined (ENCVC8000E)
           "    -O, --outputFormat               Compression format; 'avc', 'hevc' or 'jpeg'\n"
#elif defined (ENCH2V41)
           "    -O, --outputFormat               Compression format; 'avc' or 'hevc'\n"
#endif

           "    -l, --inputFormat                Color format for output\n"
           "                                     0  yuv420packedplanar    1  yuv420packedsemiplanar\n"
           "                                     3  ycbycr                4  cbycry\n"
           "                                     5  rgb565                6  bgr565\n"
           "                                     7  rgb555                9  rgb444\n"
#if defined (ENC6280) || defined (ENC7280)
           "                                     10  yuv422planar\n\n"
#endif
           "                                     11  rgb888\n\n"
           "    -o, --output                     File name of the output\n"
           "    -i, --input                      File name of the input\n"
           "    -e, --error-concealment          Use error concealment\n"
           "    -w, --lumWidthSrc                Width of source image\n"
           "    -h, --lumHeightSrc               Height of source image\n"
           "    -x, --height                     Height of output image\n"
           "    -y, --width                      Width of output image\n"
           "    -X, --horOffsetSrc               Output image horizontal offset. [0]\n"
           "    -Y, --verOffsetSrc               Output image vertical offset. [0]\n"
           "    -j, --inputRateNumer             Frame rate used for the input\n"
           "    -f, --outputRateNumer            Frame rate used for the output\n"
           "    -B, --bitsPerSecond              Bit-rate used for the output\n"
           "    -v, --input-variance-file        File containing variance numbers separated with newline\n"
           "    -a, --firstVop                   First vop of input file [0]\n"
           "    -b, --lastVop                    Last vop of input file  [EOF]\n"
           "    -s, --buffer-size                Size of allocated buffers (default provided by component) \n"
           "    -c, --buffer-count               Count of buffers allocated for each port (30)\n"
#if defined (ENC8290) || defined (ENCH1) || defined (ENCH2)|| defined (ENCVC8000E) || defined (ENCH2V41)
           "    -t, --intraArea                  left:top:right:bottom macroblock coordinates\n"
           "        --ipcm1Area                left:top:right:bottom CTB coordinates\n"
           "        --ipcm2Area                left:top:right:bottom CTB coordinates\n"
           "                                       specifying rectangular area of CTBs to\n"
           "                                       force encoding in IPCM mode.\n"
           "    -A1, --roi1Area                  left:top:right:bottom macroblock coordinates\n"
           "    -A2, --roi2Area                  left:top:right:bottom macroblock coordinates\n"
           "    -Q1, --roi1DeltaQp               QP delta value for 1st Region-Of-Interest\n"
           "    -Q2, --roi2DeltaQp               QP delta value for 2nd Region-Of-Interest\n"
           "    -CIR, --cyclicIntraRefresh       Number of macroblocks for cyclic intra refresh\n"
#endif
#ifdef ENCVC8000E
           "  \nVC8000E HEVC special configurations start\n"          
           "  -R[n] --intraPicRate             Intra picture rate in frames. [0]\n"          
           "                                   Forces every Nth frame to be encoded as intra frame.\n"
           "        --ssim                     0..1 Enable/Disable SSIM Calculation [1]\n"
           "                                   0 = Disable. \n"
           "        --preset                   0...4 for HEVC. 0..1 for H264. Trade off performance and compression efficiency\n"
           "                                     Higher value means high quality but worse performance. User need explict claim preset when use this option\n"
           "        --rdoLevel                 1..3 Programable HW RDO Level [2]\n"
           "                                   Lower value means lower quality but better performance.  \n"
           "                                   Higher value means higher quality but worse performance.  \n"
           "        --inputAlignmentExp        0..12 set alignment value [4]\n"
           "                                   0 = Disable alignment \n"
           "                                   4..12 = addr of input frame buffer and each line aligned to 2^inputAlignmentExp \n"
           "  -u[n] --ctbRc                    0...3 CTB QP adjustment mode for Rate Control and Subjective Quality.[0]\n"
           "                                   0 = No CTB QP adjustment.\n"
           "                                   1 = CTB QP adjustment for Subjective Quality only.\n"
           "                                   2 = CTB QP adjustment for Rate Control only. (For HwCtbRcVersion >= 1 only).\n"
           "                                   3 = CTB QP adjustment for both Subjective Quality and Rate Control. (For HwCtbRcVersion >= 1 only).\n"
           "        --blockRCSize              Block size in CTB QP adjustment for Subjective Quality [0]\n"
           "                                   0=64x64 ,1=32x32, 2=16x16\n"  
           "        --rcQpDeltaRange           Max absolute value of CU/MB Qp Delta to Frame QP by CTB RC [10]\n"
           "                                   0..15 for HwCtbRcVersion <= 1; 0..51 for HwCtbRcVersion > 1.\n"
           "        --rcBaseMBComplexity       0..31 MB complexity base in CTB QP adjustment for Subjective Quality [15]\n"
           "                                   MB with larger complexity offset to rcBaseMBComplexity, qpOffset larger \n"          
           "        --tolCtbRcInter            Tolerance of Ctb Rate Control for INTER frames. <float> [0.0]\n"
           "                                   Ctb Rc will try to limit INTER frame bits within the range of:\n"
           "                                       [targetPicSize/(1+tolCtbRcInter), targetPicSize*(1+tolCtbRcInter)].\n"
           "                                   Setting --tolCtbRcInter=-1.0 means no bit rate limit in Ctb Rc.\n"
           "        --tolCtbRcIntra            Tolerance of Ctb Rate Control for INTRA frames. <float> [-1.0]\n"
           "        --ctbRowQpStep             The maximum accumulated QP adjustment step per CTB Row allowed by Ctb Rate Control.\n"
           "                                   Default value is [4] for H264 and [16] for HEVC.\n"
           "                                   QP_step_per_CTB = (ctbRowQpStep / Ctb_per_Row) and limited by maximum = 4.\n"
           "        --picQpDeltaRange          Min:Max. Qp_Delta Range in Picture RC.\n"
           "                                   Min: -1..-10 Minimum Qp_Delta in Picture RC. [-2]\n"
           "                                   Max:  1..10  Maximum Qp_Delta in Picture RC. [3]\n"
           "                                   This range only applies to two neighbouring frames of same coding Type.\n"
           "                                   This range doesn't apply when HRD Overflow occurs.\n"
           "        --tolMovingBitRate         0...2000%% percent tolerance over target bitrate of moving bit rate [2000]\n"
           "        --monitorFrames            10...120, how many frames will be monitored for moving bit rate [frame rate]\n"
           "        --bitVarRangeI             10...10000%% percent variations over average bits per frame for I frame [10000]\n"
           "        --bitVarRangeP             10...10000%% percent variations over average bits per frame for P frame [10000]\n"
           "        --bitVarRangeB             10...10000%% percent variations over average bits per frame for B frame [10000]\n"
		   "        --cpbSize				   HRD Coded Picture Buffer size in bits. [0]\n"
		   "								   Buffer size used by the HRD model, 0 means max CPB for the level.\n"
           "        --bitrateWindow                1..300, Bitrate window length in frames[--intraPicRate]\n"
           "                                   Rate control allocates bits for one window and tries to\n"
           "                                   match the target bitrate at the end of each window.\n"
           "                                   Typically window begins with an intra frame, but this\n"
           "                                   is not mandatory.\n"
           "        --gopSize                  GOP Size. [0]\n"
           "                                   0..8, 0 for adaptive GOP size; 1~7 for fixed GOP size\n"
           "        --cuInfoVersion            cuInfo Version. [-1]\n"
           "        --gdrDuration              how many pictures(frames not fields) it will take to do GDR [0]\n"
           "                                   0 : disable GDR(Gradual decoder refresh), >0: enable GDR \n"
           "                                   the start point of GDR is the frame which type is set to VCENC_INTRA_FRAME \n"
           "                                   intraArea and roi1Area will be used to implement GDR function.\n"
           "        --streamMultiSegmentMode 0..2 stream multi-segment mode control. [-1]\n"
           "                                 0 = Disable stream multi-segment. \n"
           "                                 1 = Enable. NO SW handshaking. Loop back enabled.\n"
           "                                 2 = Enable. SW handshaking. Loop back enabled.\n"
           "        --streamMultiSegmentAmount 2..16. the total amount of segments to control loopback/sw-handshake/IRQ. [4]\n"
           "  -N[n] --byteStream               Stream type. [1]\n"
           "                                   0 - NAL units. Nal sizes returned in <nal_sizes.txt>\n"
           "                                   1 - byte stream according to Hevc Standard Annex B.\n"
           "  -e[n] --sliceSize                [0..height/ctu_size] slice size in number of CTU rows [0]\n"
           "                                   0 to encode each picture in one slice\n"
           "                                   [1..height/ctu_size] to each slice with N CTU row\n"
           "        --gopLowdelay              Use default lowDelay GOP configuration if --gopConfig not specified, only valid for GOP size <= 4. [0]\n"  
           "                                   0 = not enable gopLowDelay mode\n"
           "                                   1 = enable gopLowDelay mode\n"
           "  -K[n] --enableCabac              0=OFF (CAVLC), 1=ON (CABAC). [1]\n"
           "  -k[n] --videoRange               0..1 Video signal sample range value in Hevc stream. [0]\n"
           "                                   0 - Y range in [16..235] Cb,Cr in [16..240]\n"
           "                                   1 - Y,Cb,Cr range in [0..255]\n"
           "        --enableRdoQuant           0..1 Enable/Disable RDO Quant.\n"
           "                                   0 = Disable (Default if HW not support RDOQ).\n"
           "                                   1 = Enable (Default if HW support RDOQ).\n"
           "        --fieldOrder               Interlaced field order, 0=bottom first 1=top first[0]\n"          
           "        --cir                      start:interval for Cyclic Intra Refresh.\n"
           "                                   Forces ctbs in intra mode. [0:0]\n"
           "        --ipcmMapEnable            0-disable,1-enable. [0]\n"
           "        --roi1Qp                   0..51, absolute QP value for ROI 1 CTBs. [-1]. negative value is invalid. \n"
           "        --roi2Qp                   0..51, absolute QP value for ROI 2 CTBs. [-1]\n"
           "        --roi3Qp                   0..51, absolute QP value for ROI 3 CTBs. [-1]\n"
           "        --roi4Qp                   0..51, absolute QP value for ROI 4 CTBs. [-1]\n"
           "        --roi5Qp                   0..51, absolute QP value for ROI 5 CTBs. [-1]\n"
           "        --roi6Qp                   0..51, absolute QP value for ROI 6 CTBs. [-1]\n"
           "        --roi7Qp                   0..51, absolute QP value for ROI 7 CTBs. [-1]\n"
           "        --roi8Qp                   0..51, absolute QP value for ROI 8 CTBs. [-1]\n"
           "                                   roi1Qp/roi2Qp are only valid when absolute ROI QP supported. And please use either roiDeltaQp or roiQp.\n"
           "        --roiMapDeltaQpBlockUnit   0-64x64,1-32x32,2-16x16,3-8x8. [0]\n"
           "        --roiMapDeltaQpEnable      0-disable,1-enable. [0]\n"
           "        --RoiQpDeltaVer            1..3, Roi Qp Delta map version number, valid only if --roiMapInfoBinFile not empty.\n"
           "\n Parameters affecting noise reduction:\n"
           "        --noiseReductionEnable     enable noise reduction or not[0]\n"
           "                                   0 = disable NR. \n"
           "                                   1 = enable NR. \n"
           "        --noiseLow                 1..30 minimum noise value[10]. \n"
           "        --noiseFirstFrameSigma     1..30 noise estimation for start frames[11].\n"
           "        --tile=a:b:c               HEVC Tile setting: tile enabled when num_tile_columns * num_tile_rows > 1. [1:1:1]\n"
           "                                       a: num_tile_columns \n"
           "                                       b: num_tile_rows \n"
           "                                       c: loop_filter_across_tiles_enabled_flag \n"
           "\n Parameters affecting HDR10: \n"
           "        --HDR10_display=dx0:dy0:dx1:dy1:dx2:dy2:wx:wy:max:min    Mastering display colour volume SEI message\n"
           "                                   dx0 : 0...50000 component 0 normalized x chromaticity coordinates[0]\n"
           "                                   dy0 : 0...50000 component 0 normalized y chromaticity coordinates[0]\n"
           "                                   dx1 : 0...50000 component 1 normalized x chromaticity coordinates[0]\n"
           "                                   dy1 : 0...50000 component 1 normalized y chromaticity coordinates[0]\n"
           "                                   dx2 : 0...50000 component 2 normalized x chromaticity coordinates[0]\n"
           "                                   dy2 : 0...50000 component 2 normalized y chromaticity coordinates[0]\n"
           "                                   wx  : 0...50000 white point normalized x chromaticity coordinates[0]\n"
           "                                   wy  : 0...50000 white point normalized y chromaticity coordinates[0]\n"
           "                                   max : nominal maximum display luminance[0]\n"
           "                                   min : nominal minimum display luminance[0]\n"
           "        --HDR10_lightlevel=maxlevel:avglevel    Content light level info SEI message\n"
           "                                   maxlevel : max content light level\n"
           "                                   avglevel : max picture average light level\n"
           "        --HDR10_colordescription=primary:transfer:matrix    colour_description\n"
           "                                   primary : 9  index of chromaticity coordinates  in Table E.3 in HEVC SPEC[9]\n"
           "                                   transfer : 0...2 index of the reference opto - electronic transfer characteristic \n"
           "                                              function of the source picture in Table E.4 in HEVC SPEC[0]\n"
           "                                              0 ---- BT.2020, 1 ---- SMPTE ST 2084, 2 ---- ARIB STD-B67\n"
           "                                   matrix : 9  index of matrix coefficients used in deriving luma and chroma signals \n"
           "                                               from the green, blue and red or Y, Z and X primaries in Table E.5 in HEVC SPEC[9]\n"
           "        --RPSInSliceHeader         0..1 encoding rps in the slice header or not[0]\n"
           "                                   0 = not encoding rps in the slice header. \n"
           "                                   1 = encoding rps in the slice header. \n"
           "        --LTR=a:b:c:[:d]           a:b:c[:d] long term reference setting\n"
           "                                       a:    POC_delta of two frames which be used as LTR.\n"
           "                                       b:    POC_Frame_Use_LTR (0)- POC_LTR. POC_LTR is LTR's POC, POC_Frame_Use_LTR (0) is the first frame after LTR that use the LTR as reference.\n"
           "                                       c:    POC_Frame_Use_LTR(n+1) - POC_Frame_Use_LTR(n).  The POC delta between two continuous Frames that use the same LTR as reference.\n"
           "                                       d:    QP delta for frame using LTR. [0]\n"
           "  -A[n] --intraQpDelta             -51..51, Intra QP delta. [-5]\n"
           "                                   QP difference between target QP and intra frame QP.\n"
           "  -G[n] --fixedIntraQp             0..51, Fixed Intra QP, 0 = disabled. [0]\n"
           "                                   Use fixed QP value for every intra frame in stream.\n"
	       "  -V[n] --bFrameQpDelta            -1..51, BFrame QP Delta.[-1]\n"
	       "                                   QP difference between BFrame QP and target QP.\n"
	       "                                   If a GOP config file is specified by --gopConfig, it will be overrided\n"
	       "                                   -1 not take effect, 0-51 could take effect.\n"      
           "  -I[n] --chromaQpOffset           -12..12 Chroma QP offset. [0]\n"
           "        --vbr                      0=OFF, 1=ON. Variable Bit Rate Control by qpMin. [0]\n"
	       "        --sceneChange               Frame1:Frame2:...:Frame20. Specify scene change frames, seperated by ':'. Max number is 20.\n"      
           "        --smoothPsnrInGOP          0=disable, 1=enable Smooth PSNR for frames in one GOP. [0]\n"
           "                                   Only affects gopSize > 1.\n"
           "        --staticSceneIbitPercent  0...100 I frame bits percent of bitrate in static scene [80]\n"
           "        --picSkip                  0=OFF, 1=ON Picture skip rate control. [0]\n"
           "                                   Allows rate control to skip frames if needed.\n"
           "        --enableVuiTimingInfo       Write VUI timing info in SPS. [1]\n"
           "                                    0=disable. \n"
           "                                    1=enable. \n"
           "        --hashtype                 hash type for frame data hash. [0]\n"
           "                                   0=disable, 1=crc32, 2=checksum32. \n"
           "  \nExtra parameters: \n"
           "\n Parameters for MMU control:\n"
           "        --mmuEnable                  0=disable MMU if MMU exists, 1=enable MMU if MMU exists. [0]\n"
           "\n Parameters for external SRAM:\n"
           "        --extSramLumHeightBwd        0=no external SRAM, 1..16=The number of line count is 4*extSramLumHeightBwd. [hevc:16,h264:12]\n"
           "        --extSramChrHeightBwd        0=no external SRAM, 1..16=The number of line count is 4*extSramChrHeightBwd. [hevc:8,h264:6]\n"
           "        --extSramLumHeightFwd        0=no external SRAM, 1..16=The number of line count is 4*extSramLumHeightFwd. [hevc:16,h264:12]\n"
           "        --extSramChrHeightFwd        0=no external SRAM, 1..16=The number of line count is 4*extSramChrHeightFwd. [hevc:8,h264:6]\n"
           "\n Parameters for AXI alignment:\n"
           "        --AXIAlignment               AXI alignment setting (in hexadecimal format). [0]\n"
           "                                        bit[31:28] AXI_burst_align_wr_common\n"
           "                                        bit[27:24] AXI_burst_align_wr_stream\n"
           "                                        bit[23:20] AXI_burst_align_wr_chroma_ref\n"
           "                                        bit[19:16] AXI_burst_align_wr_luma_ref\n"
           "                                        bit[15:12] AXI_burst_align_rd_common\n"
           "                                        bit[11: 8] AXI_burst_align_rd_prp\n"
           "                                        bit[ 7: 4] AXI_burst_align_rd_ch_ref_prefetch\n"
           "                                        bit[ 3: 0] AXI_burst_align_rd_lu_ref_prefetch\n"
           "        --codedChromaIdc             coded chroma format idc.[1]\n"
           "                                        0-4:0:0\n"
           "                                        1-4:2:0\n"
           "        --aq_mode                    Mode for Adaptive Quantization - 0:none 1:uniform AQ 2:auto variance 3:auto variance with bias to dark scenes. Default 0\n"
           "        --aq_strength                Reduces blocking and blurring in flat and textured areas (0 to 3.0). Default 1.00\n"
           "\n Parameters for enable/disable recon write to DDR if it's pure I-frame encoding:\n"
           "        --writeReconToDDR            0..1 recon write to DDR enable [1]\n"
           "                                        0: disable recon write to DDR.\n"
           "                                        1: enable recon write to DDR.\n"
           "\n Parameters for AV1 TX type search:\n"
           "        --TxTypeSearchEnable         0=Tx type search disable, 1=Tx type search enable. [0]\n"
           "\n"
           "        --psyFactor                  0..4 Weight of psycho-visual encoding. [0]\n"
           "                                        0 = disable psy-rd.\n"
           "                                        1..4 = encode psy-rd, and set strength of psyFactor, larger favor better subjective quality.\n"
           "        --MEVertRange                ME vertical search range. Only valid if the function is supported in this version. [0]\n"
           "                                        Should be 24 or 48 for H.264; 40 or 64 for HEVC/AV1.\n"
           "                                        0 - The maximum vertical search range of this version will be used.\n"
           "        --layerInRefIdc              0..1 Enable/Disable h264 2bit nal_ref_idc [0]\n"
           "                                        0 = Disable. \n"
           "                                        1 = Enable. \n"
           "        --crf[n]                     -1..51 Constant rate factor mode, working with look-ahead turned on. -1=disable. [-1]\n"
           "                                        CRF mode is to keep a certain level of quality based on crf value, working as constant QP with complexity rate control.\n"
           "                                        CRF adjusts frame level QP within range of crf constant +-3 based on frame complexity. CRF will disable VBR mode if both enabled.\n"
           "  \nVC8000E HEVC special configurations end\n"          
#endif
#ifdef ENCH2V41
                      "  \nH2V41 HEVC special configurations start\n"          
                      "  -R[n] --intraPicRate             Intra picture rate in frames. [0]\n"          
                      "                                   Forces every Nth frame to be encoded as intra frame.\n"
                      "        --inputAlignmentExp        0..10 set alignment value [4]\n"
                      "                                   0 = Disable alignment \n"
                      "                                   4..10 = addr of input frame buffer and each line aligned to 2^inputAlignmentExp \n"
                      "  -u[n] --ctbRc                    0=OFF, 1=ON block rate control. [0]\n"
                      "                                   adaptive adjustment of QP inside frame.\n"
                      "        --blockRCSize              unit size of block Rate control [0]\n"
                      "                                   0=64x64 ,1=32x32, 2=16x16\n"  
                      "        --tolMovingBitRate         0...2000%% percent tolerance over target bitrate of moving bit rate [2000]\n"
                      "        --monitorFrames            10...120, how many frames will be monitored for moving bit rate [frame rate]\n"
                      "        --bitVarRangeI             10...10000%% percent variations over average bits per frame for I frame [10000]\n"
                      "        --bitVarRangeP             10...10000%% percent variations over average bits per frame for P frame [10000]\n"
                      "        --bitVarRangeB             10...10000%% percent variations over average bits per frame for B frame [10000]\n"
                      "        --gopSize                  GOP Size. [0]\n"
                      "                                   0..8, 0 for adaptive GOP size; 1~7 for fixed GOP size\n"
                      "        --cuInfoVersion            cuInfo Version. [-1]\n"
                      "        --gdrDuration              how many pictures(frames not fields) it will take to do GDR [0]\n"
                      "                                   0 : disable GDR(Gradual decoder refresh), >0: enable GDR \n"
                      "                                   the start point of GDR is the frame which type is set to VCENC_INTRA_FRAME \n"
                      "                                   intraArea and roi1Area will be used to implement GDR function.\n"
                      "  -e[n] --sliceSize                [0..height/ctu_size] slice size in number of CTU rows [0]\n"
                      "                                   0 to encode each picture in one slice\n"
                      "                                   [1..height/ctu_size] to each slice with N CTU row\n"
                      "        --gopLowdelay              Use default lowDelay GOP configuration if --gopConfig not specified, only valid for GOP size <= 4. [0]\n"  
                      "                                   0 = not enable gopLowDelay mode\n"
                      "                                   1 = enable gopLowDelay mode\n"
                      "  -K[n] --enableCabac              0=OFF (CAVLC), 1=ON (CABAC). [1]\n"
                      "  -k[n] --videoRange               0..1 Video signal sample range value in Hevc stream. [0]\n"
                      "                                   0 - Y range in [16..235] Cb,Cr in [16..240]\n"
                      "                                   1 - Y,Cb,Cr range in [0..255]\n"
                      "        --fieldOrder               Interlaced field order, 0=bottom first 1=top first[0]\n"          
                      "        --cir                      start:interval for Cyclic Intra Refresh.\n"
                      "                                   Forces ctbs in intra mode. [0:0]\n"
                      "        --ipcmMapEnable            0-disable,1-enable. [0]\n"
                      "        --roi1Qp                   0..51, absolute QP value for ROI 1 CTBs. [-1]. negative value is invalid. \n"
                      "        --roi2Qp                   0..51, absolute QP value for ROI 2 CTBs. [-1]\n"
                      "                                   roi1Qp/roi2Qp are only valid when absolute ROI QP supported. And please use either roiDeltaQp or roiQp.\n"
                      "        --roiMapDeltaQpBlockUnit   0-64x64,1-32x32,2-16x16,3-8x8. [0]\n"
                      "        --roiMapDeltaQpEnable      0-disable,1-enable. [0]\n"
                      "\n Parameters affecting noise reduction:\n"
                      "        --noiseReductionEnable     enable noise reduction or not[0]\n"
                      "                                   0 = disable NR. \n"
                      "                                   1 = enable NR. \n"
                      "        --noiseLow                 1..30 minimum noise value[10]. \n"
                      "        --noiseFirstFrameSigma     1..30 noise estimation for start frames[11].\n"
                      "  -A[n] --intraQpDelta             -51..51, Intra QP delta. [-5]\n"
                      "                                   QP difference between target QP and intra frame QP.\n"
                      "  -G[n] --fixedIntraQp             0..51, Fixed Intra QP, 0 = disabled. [0]\n"
                      "                                   Use fixed QP value for every intra frame in stream.\n"
                      "  -V[n] --bFrameQpDelta            -1..51, BFrame QP Delta.[-1]\n"
                      "                                   QP difference between BFrame QP and target QP.\n"
                      "                                   If a GOP config file is specified by --gopConfig, it will be overrided\n"
                      "                                   -1 not take effect, 0-51 could take effect.\n"      
                      "  -I[n] --chromaQpOffset           -12..12 Chroma QP offset. [0]\n"
                      "        --picSkip                  0=OFF, 1=ON Picture skip rate control. [0]\n"
                      "                                   Allows rate control to skip frames if needed.\n"
                      "  \nVC8000E HEVC special configurations end\n"          
#endif

#ifdef ENCH1
           "    -AR, --adaptiveRoi               QP delta for adaptive ROI\n"
           "    -RC, --adaptiveRoiColor          Color sensitivity for adaptive ROI [0]\n"
           "    -TL0, --layer0Bitrate            Enables temporal layers. Target bitrate for base layer [0]\n"
           "    -TL1, --layer1Bitrate            Target bitrate for layer 1\n"
           "    -TL2, --layer2Bitrate            Target bitrate for layer 2\n"
           "    -TL3, --layer3Bitrate            Target bitrate for layer 3\n"
#endif
           "\n"
           "    -r, --rotation                   Rotation value, angle in degrees\n"
#if !defined (ENCH2)&& !defined (ENCVC8000E)

           "    -Z, --videostab                  Use frame stabilization\n"
#endif
           "    -T, --write-frames               Write each frame additionally to a separate file\n"
           "    -D, --write-buffers              Write each buffer additionally to a separate\n"
           "                                     file (overrides -T)\n"
           "\n", swname);

#if !defined (ENCH2)&& !defined (ENCVC8000E)

    print_avc_usage();
#endif
#if defined (ENC6280) || defined (ENC7280)
    print_mpeg4_usage();
    print_h263_usage();
#endif
#ifdef ENCH1
    print_vp8_usage();
    print_webp_usage();
#endif
#ifdef ENCH2
    print_hevc_usage();
#endif
#ifdef ENCVC8000E
    print_vc8000e_usage();
#endif

#ifdef ENCH2V41
	print_h2v41_usage();
#endif

#if !defined (ENCH2) && !defined (ENCVC8000E) && !defined (ENCH2V41)
    print_jpeg_usage();
#endif

    printf("  Return value:\n"
           "    0 = OK; failures indicated as OMX error codes\n" "\n");
}

/*
    process_parameters
 */
OMX_ERRORTYPE process_encoder_parameters(int argc, char **args,
                                         OMXENCODER_PARAMETERS * params)
{
    OMX_S32 i, j, fcount;
    OMX_STRING files[3] = { 0 };    // input, output, input-variance

    fcount = 0;
    params->lastvop = 100;

    i = 1;
    while(i < argc)
    {

        if(strcmp(args[i], "-i") == 0 || strcmp(args[i], "--input") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input file missing.\n");
            files[0] = args[i];
        }
        /* input compression format */
        else if(strcmp(args[i], "-O") == 0 ||
                strcmp(args[i], "--output-compression-format") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for output format is missing.\n");
#if 0
            if((strcasecmp(args[i], "jpeg") == 0) || (strcasecmp(args[i], "webp") == 0))
                params->image_output = OMX_TRUE;
            else
                params->image_output = OMX_FALSE;
#else
            if(strcasecmp(args[i], "avc") == 0)
            {
                params->image_output = OMX_FALSE;
                params->cRole = "video_encoder.avc";
            }
            else if(strcasecmp(args[i], "mpeg4") == 0)
            {
                params->image_output = OMX_FALSE;
                params->cRole = "video_encoder.mpeg4";
            }
            else if(strcasecmp(args[i], "h263") == 0)
            {
                params->image_output = OMX_FALSE;
                params->cRole = "video_encoder.h263";
            }
            else if(strcasecmp(args[i], "vp8") == 0)
            {
                params->image_output = OMX_FALSE;
                params->cRole = "video_encoder.vp8";
            }
            else if(strcasecmp(args[i], "hevc") == 0)
            {
                params->image_output = OMX_FALSE;
                params->cRole = "video_encoder.hevc";
            }
            else if(strcasecmp(args[i], "jpeg") == 0)
            {
                params->image_output = OMX_TRUE;
                params->cRole = "image_encoder.jpeg";
            }
            else if(strcasecmp(args[i], "webp") == 0)
            {
                params->image_output = OMX_TRUE;
                params->cRole = "image_encoder.webp";
            }
            else
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Unknown compression format.\n");
                return OMX_ErrorBadParameter;
            }
#endif
        }
        else if(strcmp(args[i], "-o") == 0 || strcmp(args[i], "--output") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for output file missing.\n");
            files[1] = args[i];
        }
        else if(strcmp(args[i], "-v") == 0 ||
                strcmp(args[i], "--input-variance-file") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input variance file missing.\n");
            files[2] = args[i];
        }
        else if(strcmp(args[i], "-a") == 0 ||
                strcmp(args[i], "--firstVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for first vop is missing.\n");
            params->firstvop = atoi(args[i]);
        }
        else if(strcmp(args[i], "-b") == 0 || strcmp(args[i], "--lastVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for last vop is missing.\n");
            params->lastvop = atoi(args[i]);
        }
        else if(strcmp(args[i], "-s") == 0 ||
                strcmp(args[i], "--buffer-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer allocation size is missing.\n");
            params->buffer_size = atoi(args[i]);
        }
        else if(strcmp(args[i], "-S") == 0 ||
                strcmp(args[i], "--slice-height") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for slice height is missing.\n");
            params->sliced = OMX_TRUE;
        }
        else if(strcmp(args[i], "-T") == 0 ||
                strcmp(args[i], "--write-frames") == 0)
        {
            params->framed_output = OMX_TRUE;
        }
        else if(strcmp(args[i], "-D") == 0 ||
                strcmp(args[i], "--write-buffers") == 0)
        {
            params->sliced_output = OMX_TRUE;
        }
        else if(strcmp(args[i], "-c") == 0 ||
                strcmp(args[i], "--buffer-count") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer count is missing.\n");
            params->buffer_count = atoi(args[i]);
        }
        else if(strcmp(args[i], "-r") == 0 ||
                strcmp(args[i], "--rotation") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for rotation is missing.\n");
                return OMX_ErrorBadParameter;
            }
            params->rotation = atoi(args[i]);
        }
        else if(strcmp(args[i], "-Z") == 0 ||
                strcmp(args[i], "--videostab") == 0)
        {
            params->stabilization = OMX_TRUE;
        }
        else if(strcmp(args[i], "-x") == 0 || strcmp(args[i], "--width") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for output crop width missing.\n");
            params->cropping = OMX_TRUE;
            params->cwidth = atoi(args[i]);
        }
        else if(strcmp(args[i], "-y") == 0 || strcmp(args[i], "--height") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for output crop height missing.\n");
            params->cropping = OMX_TRUE;
            params->cheight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-X") == 0 ||
                strcmp(args[i], "--horOffsetSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for horizontal crop offset is missing.\n");
            params->cropping = OMX_TRUE;
            params->cleft = atoi(args[i]);
        }
        else if(strcmp(args[i], "-Y") == 0 ||
                strcmp(args[i], "--verOffsetSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for vertical crop offset is missing.\n");
            params->cropping = OMX_TRUE;
            params->ctop = atoi(args[i]);
        }
        else if(strcmp(args[i], "-t") == 0 ||
                strcmp(args[i], "--intraArea") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for intra area is missing.\n");
            if(atoi(args[i]) == 0)
            {
                params->intraArea.enable = OMX_FALSE;
            }
            else
            {
                //printf("\n%s\n", args[i]);
                params->intraArea.enable = OMX_TRUE;
                /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->intraArea.left = atoi(args[i]);
                /* yy is top coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->intraArea.top = atoi(args[i]);
                /* XX is right coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->intraArea.right = atoi(args[i]);
                /* YY is bottom coordinate */
                args[i] += j+1;
                params->intraArea.bottom = atoi(args[i]);
                OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "IntraArea enable %d, top %d left %d bottom %d right %d\n", params->intraArea.enable,
                    params->intraArea.top, params->intraArea.left, params->intraArea.bottom, params->intraArea.right);
            }
        }
        else if(strcmp(args[i], "--ipcm1Area") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for IPCM1 mode is missing.\n");
            if(atoi(args[i]) == 0)
            {
                params->ipcm1Area.enable = OMX_FALSE;
            }
            else
            {
                //printf("\n%s\n", args[i]);
                params->ipcm1Area.enable = OMX_TRUE;
                /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm1Area.left = atoi(args[i]);
                /* yy is top coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm1Area.top = atoi(args[i]);
                /* XX is right coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm1Area.right = atoi(args[i]);
                /* YY is bottom coordinate */
                args[i] += j+1;
                params->ipcm1Area.bottom = atoi(args[i]);
                OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "IPCM1 enable %d, top %d left %d bottom %d right %d\n", params->ipcm1Area.enable,
                    params->ipcm1Area.top, params->ipcm1Area.left, params->ipcm1Area.bottom, params->ipcm1Area.right);
            }
        }
        else if(strcmp(args[i], "--ipcm2Area") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for IPCM2 mode is missing.\n");
            if(atoi(args[i]) == 0)
            {
                params->ipcm2Area.enable = OMX_FALSE;
            }
            else
            {
                //printf("\n%s\n", args[i]);
                params->ipcm2Area.enable = OMX_TRUE;
                /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm2Area.left = atoi(args[i]);
                /* yy is top coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm2Area.top = atoi(args[i]);
                /* XX is right coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->ipcm2Area.right = atoi(args[i]);
                /* YY is bottom coordinate */
                args[i] += j+1;
                params->ipcm2Area.bottom = atoi(args[i]);
                OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "IPCM1 enable %d, top %d left %d bottom %d right %d\n", params->ipcm2Area.enable,
                    params->ipcm2Area.top, params->ipcm2Area.left, params->ipcm2Area.bottom, params->ipcm2Area.right);
            }
        }
        else if(strcmp(args[i], "-A1") == 0 ||
                strcmp(args[i], "--roi1Area") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ROI area 1 is missing.\n");
            if(atoi(args[i]) == 0)
            {
                params->roi1Area.enable = OMX_FALSE;
            }
            else
            {
                //printf("\n%s\n", args[i]);
                params->roi1Area.enable = OMX_TRUE;
                /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi1Area.left = atoi(args[i]);
                /* yy is top coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi1Area.top = atoi(args[i]);
                /* XX is right coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi1Area.right = atoi(args[i]);
                /* YY is bottom coordinate */
                args[i] += j+1;
                params->roi1Area.bottom = atoi(args[i]);
                OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "Roi1Area enable %d, top %d left %d bottom %d right %d\n", params->roi1Area.enable,
                    params->roi1Area.top, params->roi1Area.left, params->roi1Area.bottom, params->roi1Area.right);
            }
        }
        else if(strcmp(args[i], "-A2") == 0 ||
                strcmp(args[i], "--roi2Area") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ROI area 2 is missing.\n");
            if(atoi(args[i]) == 0)
            {
                params->roi2Area.enable = OMX_FALSE;
            }
            else
            {
                //printf("\n%s\n", args[i]);
                params->roi2Area.enable = OMX_TRUE;
                /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi2Area.left = atoi(args[i]);
                /* yy is top coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi2Area.top = atoi(args[i]);
                /* XX is right coordinate */
                args[i] += j+1;
                if ((j = ParseDelim(args[i], ':')) == -1) break;
                params->roi2Area.right = atoi(args[i]);
                /* YY is bottom coordinate */
                args[i] += j+1;
                params->roi2Area.bottom = atoi(args[i]);
                OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "Roi2Area enable %d, top %d left %d bottom %d right %d\n", params->roi2Area.enable,
                    params->roi2Area.top, params->roi2Area.left, params->roi2Area.bottom, params->roi2Area.right);
            }
        }
        else if(strcmp(args[i], "-Q1") == 0 ||
                strcmp(args[i], "--roi1DeltaQp") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for ROI delta QP is missing.\n");
                return OMX_ErrorBadParameter;
            }
            params->roi1DeltaQP = atoi(args[i]);
        }
        else if(strcmp(args[i], "-Q2") == 0 ||
                strcmp(args[i], "--roi2DeltaQp") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for ROI 2 delta QP is missing.\n");
                return OMX_ErrorBadParameter;
            }
            params->roi2DeltaQP = atoi(args[i]);
        }
        else if(strcmp(args[i], "-AR") == 0 ||
                strcmp(args[i], "--adaptiveRoi") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for adaptive ROI is missing.\n");
                return OMX_ErrorBadParameter;
            }
            params->adaptiveRoi = atoi(args[i]);
        }
        else if(strcmp(args[i], "-RC") == 0 ||
                strcmp(args[i], "--adaptiveRoiColor") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for adaptive ROI color is missing.\n");
                return OMX_ErrorBadParameter;
            }
            params->adaptiveRoiColor = atoi(args[i]);
        }
        else if(strcmp(args[i], "-TL0") == 0 ||
                strcmp(args[i], "--layer0Bitrate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for base layer bitrate is missing.\n");
            params->baseLayerBitrate = atoi(args[i]);
        }
        else if(strcmp(args[i], "-TL1") == 0 ||
                strcmp(args[i], "--layer1Bitrate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for layer 1 bitrate is missing.\n");
            params->layer1Bitrate = atoi(args[i]);
        }
        else if(strcmp(args[i], "-TL2") == 0 ||
                strcmp(args[i], "--layer2Bitrate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for layer 2 bitrate is missing.\n");
            params->layer2Bitrate = atoi(args[i]);
        }
        else if(strcmp(args[i], "-TL3") == 0 ||
                strcmp(args[i], "--layer3Bitrate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for layer 3 bitrate is missing.\n");
            params->layer3Bitrate = atoi(args[i]);
        }
        else if(strcmp(args[i], "-CIR") == 0 ||
                strcmp(args[i], "--cyclicIntraRefresh") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for cyclic intra refresh is missing.\n");
            params->cir = atoi(args[i]);
        }
        else
        {
            /* do nothing, paramter may be needed by subsequent parameter readers */
        }
        ++i;
    }

    if(files[0] == 0)
    {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "No input file.\n");
        return OMX_ErrorBadParameter;
    }

    if(files[1] == 0)
    {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "No output file.\n");
        return OMX_ErrorBadParameter;
    }

    params->infile = files[0];
    params->outfile = files[1];
    params->varfile = files[2];

    return OMX_ErrorNone;
}

/*
 */
#if defined (ENC6280)
static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatYUV422Planar
    , OMX_COLOR_Format32bitARGB8888
};
#elif defined (ENC7280)
static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_FormatYUV422Planar
};
#elif defined (ENCVC8000E)
static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_FormatYUV422Planar
    , OMX_COLOR_Format32bitARGB8888
};

/*static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatYUV420SemiPlanarVU
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_Format16bitBGR555
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format32bitARGB8888
};*/
#elif defined (ENCH2V41)
static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatYUV420SemiPlanarVU
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_Format16bitBGR555
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format32bitARGB8888
};
#elif
static OMX_COLOR_FORMATTYPE inputPixelFormats[] = 
{
    OMX_COLOR_FormatYUV420PackedPlanar
    , OMX_COLOR_FormatYUV420PackedSemiPlanar
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_FormatYCbYCr
    , OMX_COLOR_FormatCbYCrY
    , OMX_COLOR_Format16bitRGB565
    , OMX_COLOR_Format16bitBGR565
    , OMX_COLOR_Format16bitARGB1555
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format16bitARGB4444
    , OMX_COLOR_FormatUnused
    , OMX_COLOR_Format32bitARGB8888
};
#endif
OMX_ERRORTYPE process_encoder_input_parameters(int argc, char **args,
                                               OMX_PARAM_PORTDEFINITIONTYPE *
                                               params)
{
    OMX_S32 i;

    params->format.video.nSliceHeight = 0;
    params->format.video.nFrameWidth = 0;

    i = 1;
    while(i < argc)
    {

        /* input color format */
        if(strcmp(args[i], "-l") == 0 || strcmp(args[i], "--inputFormat") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for input color format missing.\n");
                return OMX_ErrorBadParameter;
            }

            {
                int fmtTabSize = sizeof(inputPixelFormats) / sizeof(OMX_COLOR_FORMATTYPE);
                int fmtIdx = atoi(args[i]);

                params->format.video.eColorFormat = OMX_COLOR_FormatUnused;
                if(fmtIdx >= 0 && fmtIdx < fmtTabSize)
                {
                    params->format.video.eColorFormat = inputPixelFormats[fmtIdx];
                }

                if(params->format.video.eColorFormat == OMX_COLOR_FormatUnused)
                {
                    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "Unknown color format.\n");
                    return OMX_ErrorBadParameter;
                }
            }
        }
        else if(strcmp(args[i], "-e") == 0 ||
                strcmp(args[i], "--error-concealment") == 0)
        {
            params->format.video.bFlagErrorConcealment = OMX_TRUE;
        }
        else if(strcmp(args[i], "-h") == 0 ||
                strcmp(args[i], "--lumHeightSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input height missing.\n");
            params->format.video.nFrameHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-w") == 0 ||
                strcmp(args[i], "--lumWidthSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input width missing.\n");
            params->format.video.nStride = atoi(args[i]);
        }
        else if(strcmp(args[i], "-j") == 0 ||
                strcmp(args[i], "--inputRateNumer") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input frame rate is missing.\n");
            params->format.video.xFramerate = FLOAT_Q16(strtod(args[i], 0));
        }
        else if(strcmp(args[i], "-s") == 0 ||
                strcmp(args[i], "--buffer-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer allocation size is missing.\n");
            params->nBufferSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-c") == 0 ||
                strcmp(args[i], "--buffer-count") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer count is missing.\n");
            params->nBufferCountActual = atoi(args[i]);
        }
        else
        {
            /* do nothing, paramter may be needed by subsequent parameter readers */
        }

        ++i;
    }

    return OMX_ErrorNone;
}

/*
 */
OMX_ERRORTYPE process_encoder_image_input_parameters(int argc, char **args,
                                                    OMX_PARAM_PORTDEFINITIONTYPE
                                                    * params)
{
    OMX_S32 i;

    params->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    params->format.image.nSliceHeight = 0;
    params->format.image.nFrameWidth = 0;

    i = 1;
    while(i < argc)
    {

        /* input color format */
        if(strcmp(args[i], "-l") == 0 || strcmp(args[i], "--inputFormat") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input color format missing.\n");
            switch (atoi(args[i]))
            {
            case 0:
                params->format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
                break;
            case 1:
                params->format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
                break;
#if defined (ENC7280) || defined (ENCVC8000E)
            case 2:
                params->format.image.eColorFormat = OMX_COLOR_FormatYCbYCr;
                break;
#endif
            case 3:
                params->format.image.eColorFormat = OMX_COLOR_FormatYCbYCr;
#if defined (ENC7280) || defined (ENCVC8000E)
                params->format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
#endif
                break;
            case 4:
                params->format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
#if defined (ENC7280) || defined (ENCVC8000E)
                params->format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;
#endif
                break;
            case 5:
                params->format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;
#if defined (ENC7280) || defined (ENCVC8000E)
                params->format.image.eColorFormat = OMX_COLOR_Format16bitBGR565;
#endif
                break;
            case 6:
                params->format.image.eColorFormat = OMX_COLOR_Format16bitBGR565;
#if defined (ENC7280) || defined (ENCVC8000E)
                params->format.image.eColorFormat = OMX_COLOR_Format16bitARGB1555;
#endif
                break;
#ifndef ENC7280
            case 7:
                params->format.image.eColorFormat = OMX_COLOR_Format16bitARGB1555;
                break;
#endif
#if defined (ENC7280) || defined (ENCVC8000E)
            case 8:
                params->format.image.eColorFormat = OMX_COLOR_Format16bitARGB4444;
                break;
#endif
#ifndef ENC7280
            case 9:
                params->format.image.eColorFormat = OMX_COLOR_Format16bitARGB4444;
                break;
#endif
#if defined (ENC6280) || defined (ENC7280) || defined (ENCVC8000E)
            case 10:
                params->format.image.eColorFormat = OMX_COLOR_FormatYUV422Planar;
                break;
#endif
#ifndef ENC7280
            case 11:
                params->format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;
                break;
#endif
            default:
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "Unknown color format.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "-e") == 0 ||
                strcmp(args[i], "--error-concealment") == 0)
        {
            params->format.image.bFlagErrorConcealment = OMX_TRUE;
        }
        else if(strcmp(args[i], "-h") == 0 ||
                strcmp(args[i], "--lumHeightSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input height missing.\n");
            params->format.image.nFrameHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-w") == 0 ||
                strcmp(args[i], "--lumWidthSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input width missing.\n");
            params->format.image.nStride = atoi(args[i]);
        }
        else if(strcmp(args[i], "-s") == 0 ||
                strcmp(args[i], "--buffer-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer allocation size is missing.\n");
            params->nBufferSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-S") == 0 ||
                strcmp(args[i], "--slice-height") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for slice height is missing.\n");
            params->format.image.nSliceHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-c") == 0 ||
                strcmp(args[i], "--buffer-count") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer count is missing.\n");
            params->nBufferCountActual = atoi(args[i]);
        }
        else
        {
            /* do nothing, paramter may be needed by subsequent parameter readers */
        }

        ++i;
    }

    return OMX_ErrorNone;
}

/*
 */
OMX_ERRORTYPE process_encoder_output_parameters(int argc, char **args,
                                                OMX_PARAM_PORTDEFINITIONTYPE *
                                                params)
{
    OMX_S32 i;

    params->format.video.nSliceHeight = 0;
    params->format.video.nStride = 0;

    i = 1;
    while(i < argc)
    {

        /* input compression format */
        if(strcmp(args[i], "-O") == 0 ||
           strcmp(args[i], "--output-compression-format") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for output compression format missing.\n");
                return OMX_ErrorBadParameter;
            }

            if(strcasecmp(args[i], "avc") == 0)
                params->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
            else if(strcasecmp(args[i], "mpeg4") == 0)
                params->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
            else if(strcasecmp(args[i], "h263") == 0)
                params->format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
            else if(strcasecmp(args[i], "vp8") == 0)
                params->format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
            else if(strcasecmp(args[i], "hevc") == 0)
                params->format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
            else
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Unknown compression format.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "-B") == 0 ||
                strcmp(args[i], "--bitsPerSecond") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bit-rate missing.\n");
            params->format.video.nBitrate = atoi(args[i]);
        }
        else if(strcmp(args[i], "-h") == 0 ||
                strcmp(args[i], "--lumHeightSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input height missing.\n");
            params->format.video.nFrameHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-w") == 0 ||
                strcmp(args[i], "--lumWidthSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input width missing.\n");
            params->format.video.nFrameWidth = atoi(args[i]);
        }
        else if(strcmp(args[i], "-f") == 0 ||
                strcmp(args[i], "--outputRateNumer") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for output frame rate is missing.\n");
            params->format.video.xFramerate = FLOAT_Q16(strtod(args[i], 0));
        }
        else if(strcmp(args[i], "-s") == 0 ||
                strcmp(args[i], "--buffer-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer allocation size is missing.\n");
            params->nBufferSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-c") == 0 ||
                strcmp(args[i], "--buffer-count") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer count is missing.\n");
            params->nBufferCountActual = atoi(args[i]);
        }
        else
        {
            /* do nothing, paramter may be needed by subsequent parameter readers */
        }
        ++i;
    }

    return OMX_ErrorNone;
}

/*
 */
OMX_ERRORTYPE process_encoder_image_output_parameters(int argc, char **args,
                                                     OMX_PARAM_PORTDEFINITIONTYPE
                                                     * params)
{
    OMX_S32 i;

    params->format.image.nSliceHeight = 0;
    params->format.image.nStride = 0;
    params->format.image.eColorFormat = OMX_COLOR_FormatUnused;

    i = 1;
    while(i < argc)
    {
        /* input compression format */
        if(strcmp(args[i], "-O") == 0 ||
           strcmp(args[i], "--output-compression-format") == 0)
        {
            if(++i == argc)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Parameter for output compression format missing.\n");
                return OMX_ErrorBadParameter;
            }

            if(strcmp(args[i], "jpeg") == 0)
                params->format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            else if(strcmp(args[i], "webp") == 0)
                params->format.image.eCompressionFormat = OMX_IMAGE_CodingWEBP;
            else
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Unknown compression format.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "-h") == 0 || strcmp(args[i], "--lumHeightSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input height missing.\n");
            params->format.image.nFrameHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-w") == 0 ||
                strcmp(args[i], "--lumWidthSrc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for input width missing.\n");
            params->format.image.nFrameWidth = atoi(args[i]);
        }
        else if(strcmp(args[i], "-s") == 0 ||
                strcmp(args[i], "--buffer-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer allocation size is missing.\n");
            params->nBufferSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-S") == 0 ||
                strcmp(args[i], "--slice-height") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for slice height is missing.\n");
            params->format.image.nSliceHeight = atoi(args[i]);
        }
        else if(strcmp(args[i], "-c") == 0 ||
                strcmp(args[i], "--buffer-count") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for buffer count is missing.\n");
            params->nBufferCountActual = atoi(args[i]);
        }
        else
        {
            /* do nothing, paramter may be needed by subsequent parameter readers */
        }
        ++i;
    }

    return OMX_ErrorNone;
}

/*
*/
void print_mpeg4_usage()
{
    printf("  MPEG4 parameters:\n"
           "    -p, --profile                    Sum of following:\n"
           "                                      0x0001    Simple Profile, Levels 1-3 (def)\n"
           "                                      0x0002    Simple Scalable Profile, Levels 1-2\n"
           "                                      0x0004    Core Profile, Levels 1-2\n"
           "                                      0x0008    Main Profile, Levels 2-4\n"
           "                                      0x0010    N-bit Profile, Level 2\n"
           "                                      0x0020    Scalable Texture Profile, Level 1\n"
           "                                      0x0040    Simple Face Animation Profile, Levels 1-2\n"
           "                                      0x0080    Simple Face and Body Animation (FBA) Profile,\n"
           "                                                Levels 1-2\n"
           "                                      0x0100    Basic Animated Texture Profile, Levels 1-2\n"
           "                                      0x0200    Hybrid Profile, Levels 1-2\n"
           "                                      0x0400    Advanced Real Time Simple Profiles, Levels 1-4\n"
           "                                      0x0800    Core Scalable Profile, Levels 1-3\n"
           "                                      0x1000    Advanced Coding Efficiency Profile, Levels 1-4\n"
           "                                      0x2000    Advanced Core Profile, Levels 1-2\n"
           "                                      0x4000    Advanced Scalable Texture, Levels 2-3\n"
           "\n"
           "    -L, --level                      Sum of following:\n"
           "                                      0x01  Level 0      0x10  Level 3\n"
           "                                      0x02  Level 0b     0x20  Level 4\n"
           "                                      0x04  Level 1      0x40  Level 4a\n"
           "                                      0x08  Level 2      0x80  Level 5 (def)\n"
           "\n"
           "    -U, --picture-types              Picture types allowed in the bitstream\n"
           "                                     i, p, b, s\n"
           "    -G, --svh-mode                   Enable Short Video Header mode\n"
           "    -g, --gov                        Enable GOV\n"
           "    -n, --npframes                   Number of P frames between each I frame\n"
           "    -M, --max-packet-size            Maximum size of packet in bytes.\n"
           "    -V, --time-inc-resolution        VOP time increment resolution for MPEG4.\n"
           "                                     Interpreted as described in MPEG4 standard.\n"
           "    -E, --hec                        Specifies the number of consecutive video packet\n"
           "    -R, --rvlc                       Use variable length coding\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -q, --qpp                        QP value to use for P frames\n"
           "\n");

}

/*
*/
void print_avc_usage()
{
    printf("  AVC parameters:\n"
           "    -p, --profile                    Sum of following: \n"
           "                                      0x01    baseline (def)  0x10    high10\n"
           "                                      0x02    main            0x20    high422\n"
           "                                      0x04    extended        0x40    high444\n"
           "                                      0x08    high\n"
           "\n"
           "    -L, --level                      Sum of following: \n"
           "                                      0x0001   level1         0x0100    level3\n"
           "                                      0x0002   level1b        0x0200    level31\n"
           "                                      0x0004   level11        0x0400    level32\n"
           "                                      0x0008   level12        0x0800    level4 (def)\n"
           "                                      0x0010   level13        0x1000    level41\n"
           "                                      0x0020   level2         0x2000    level42\n"
           "                                      0x0040   level21        0x4000    level5\n"
           "                                      0x0080   level22        0x8000    level51\n"
           "\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -n, --npframes                   Number of P frames between each I frame\n"
           "    -d, --deblocking                 Set deblocking\n"
           "    -q, --qpp                        QP value to use for P frames\n"
           "\n");

}

/*
*/
void print_hevc_usage()
{
    printf("  HEVC parameters:\n"
           "    -p, --profile                    1    main (def)\n"
           "\n"
           "    -L, --level                      30    level1\n"
           "                                     60    level2\n"
           "                                     63    level2.1\n"
           "                                     90    level3\n"
           "                                     93    level3.1\n"
           "                                     120   level4\n"
           "                                     123   level4.1\n"
           "                                     150   level5\n"
           "                                     153   level5.1\n"
           "\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -n, --npframes                   Number of P frames between each I frame\n"
           "    -d, --deblocking                 Set deblocking\n"
           "    -q, --qpp                        QP value to use for P frames\n\n"

           "        --nTcOffset                  Deblocking filter tc offset\n"
           "        --nBetaOffset                Deblocking filter beta offset\n"
           "        --bEnableDeblockOverride     Deblocking override enable flag\n"
           "        --bDeblockOverride           Deblocking override flag\n"
           "        --bEnableSAO                 Disable or Enable SAO Filter\n"
		   "		--bEnableScalingList 	     0..1 using default scaling list or not[0]\n"
		   "								     0 = using average scaling list. \n"
		   "								     1 = using default scaling list. \n"
           "        --bCabacInitFlag             Set cabac init flag\n"
           "\n");

}

void print_vc8000e_usage()
{
    printf("  HEVC parameters:\n"
           "    -p, --profile                    1    main (def)\n"
           "\n"
           "    -L, --level                      30    level1\n"
           "                                     60    level2\n"
           "                                     63    level2.1\n"
           "                                     90    level3\n"
           "                                     93    level3.1\n"
           "                                     120   level4\n"
           "                                     123   level4.1\n"
           "                                     150   level5\n"
           "                                     153   level5.1\n"
           "\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -n, --npframes                   Number of P frames between each I frame\n"
           "    -d, --deblocking                 Set deblocking\n"
           "    -q, --qpp                        QP value to use for P frames\n\n"

           "        --nTcOffset                  Deblocking filter tc offset\n"
           "        --nBetaOffset                Deblocking filter beta offset\n"
           "        --bEnableDeblockOverride     Deblocking override enable flag\n"
           "        --bDeblockOverride           Deblocking override flag\n"
           "        --bEnableSAO                 Disable or Enable SAO Filter\n"
		   "		--bEnableScalingList 	     0..1 using default scaling list or not[0]\n"
		   "								     0 = using average scaling list. \n"
		   "								     1 = using default scaling list. \n"
           "        --bCabacInitFlag             Set cabac init flag\n"
           "\n");

}

void print_h2v41_usage()
{
    printf("  HEVC parameters:\n"
           "    -p, --profile                    1    main (def)\n"
           "\n"
           "    -L, --level                      30    level1\n"
           "                                     60    level2\n"
           "                                     63    level2.1\n"
           "                                     90    level3\n"
           "                                     93    level3.1\n"
           "                                     120   level4\n"
           "                                     123   level4.1\n"
           "                                     150   level5\n"
           "                                     153   level5.1\n"
           "\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -n, --npframes                   Number of P frames between each I frame\n"
           "    -d, --deblocking                 Set deblocking\n"
           "    -q, --qpp                        QP value to use for P frames\n\n"

           "        --nTcOffset                  Deblocking filter tc offset\n"
           "        --nBetaOffset                Deblocking filter beta offset\n"
           "        --bEnableDeblockOverride     Deblocking override enable flag\n"
           "        --bDeblockOverride           Deblocking override flag\n"
           "        --bEnableSAO                 Disable or Enable SAO Filter\n"
		   "		--bEnableScalingList 	     0..1 using default scaling list or not[0]\n"
		   "								     0 = using average scaling list. \n"
		   "								     1 = using default scaling list. \n"
           "        --bCabacInitFlag             Set cabac init flag\n"
           "\n");

}

/*
*/
void print_vp8_usage()
{
    printf("  VP8 parameters:\n"
           "    -L, --level                      1   level version0 (def)\n"
           "                                     2   level version1\n"
           "                                     4   level version2\n"
           "                                     8   level version3\n"
           "\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -d, --dctPartitions              0=1, 1=2, 2=4, 3=8, Amount of DCT partitions to create\n"
           "    -R, --errorResilient             Enable error resilient stream mode\n"
           "    -q, --qpp                        QP value to use for P frames\n"
           "    -V, --vp8IntraPicRate            Intra picture rate in frames\n"
           "\n");

}

/*
*/
void print_h263_usage()
{
    printf("  H263 parameters:\n"
           "    -p, --profile                    Sum of following: \n"
           "                                (def) Baseline            0x01   HighCompression     0x20\n"
           "                                      H320Coding          0x02   Internet            0x40\n"
           "                                      BackwardCompatible  0x04   Interlace           0x80\n"
           "                                      ISWV2               0x08   HighLatency         0x100\n"
           "                                      ISWV3               0x10\n"
           "\n"
           "    -L, --level                      Sum of following: \n"
           "                                      Level10    0x01    Level45    0x10\n"
           "                                      Level20    0x02    Level50    0x20\n"
           "                                      Level30    0x04    Level60    0x40\n"
           "                                      Level40    0x08    Level70    0x80 (def)\n"
           "\n"
           "    -U, --picture-types              Picture types allowed in the bitstream\n"
           "                                     i, p, b, I (si), P (sp)\n"
           "    -P, --plus-type                  Use of PLUSPTYPE is allowed\n"
           "    -C, --control-rate               disable, variable, constant, variable-skipframes,\n"
           "                                     constant-skipframes\n"
           "    -q, --qpp                        QP value to use for P frames\n"
           "\n");

}

/*
*/
void print_jpeg_usage()
{
    printf("  JPEG parameters:\n"
           "    -q, --qLevel                     JPEG Q factor value in the range of 0-10\n"
           "    -S, --slice-height               Height of slice\n" "\n");
}

/*
*/
void print_webp_usage()
{
    printf("  WEBP parameters:\n"
           "    -q, --qLevel                     WEBP Q factor value in the range of 0-9\n\n");
}

/*
*/
OMX_ERRORTYPE process_mpeg4_parameters(int argc, char **args,
                                       OMX_VIDEO_PARAM_MPEG4TYPE *
                                       mpeg4parameters)
{
    int i, j;

    mpeg4parameters->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    mpeg4parameters->eLevel = OMX_VIDEO_MPEG4Level5;

    i = 0;
    while(++i < argc)
    {

        if(strcmp(args[i], "-p") == 0 || strcmp(args[i], "--profile") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for profile is missing.\n");
            mpeg4parameters->eProfile = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");
            mpeg4parameters->eLevel = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-G") == 0 ||
                strcmp(args[i], "--svh-mode") == 0)
        {
            mpeg4parameters->bSVH = OMX_TRUE;
        }
        else if(strcmp(args[i], "-g") == 0 || strcmp(args[i], "--gov") == 0)
        {
            mpeg4parameters->bGov = OMX_TRUE;
        }
        else if(strcmp(args[i], "-n") == 0 ||
                strcmp(args[i], "--npframes") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of P frames is missing.\n");
            mpeg4parameters->nPFrames = atoi(args[i]);
        }
        else if(strcmp(args[i], "-R") == 0 || strcmp(args[i], "--rvlc") == 0)
        {
            mpeg4parameters->bReversibleVLC = OMX_TRUE;
        }
        else if(strcmp(args[i], "-A") == 0 ||
                strcmp(args[i], "--ac-prediction") == 0)
        {
            mpeg4parameters->bACPred = OMX_TRUE;
        }
        else if(strcmp(args[i], "-M") == 0 ||
                strcmp(args[i], "--max-packet-size") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for max packet size is missing.\n");
            mpeg4parameters->nMaxPacketSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-V") == 0 ||
                strcmp(args[i], "--time-inc-resolution") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for VOP time increment resolution is missing.\n");
            mpeg4parameters->nTimeIncRes = atoi(args[i]);
        }
        else if(strcmp(args[i], "-U") == 0 ||
                strcmp(args[i], "--picture-types") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for allowed picture types is missing.\n");

            j = strlen(args[i]);
            do
            {
                switch (args[i][--j])
                {
                case 'I':
                case 'i':
                    mpeg4parameters->nAllowedPictureTypes |=
                        OMX_VIDEO_PictureTypeI;
                    break;
                case 'P':
                case 'p':
                    mpeg4parameters->nAllowedPictureTypes |=
                        OMX_VIDEO_PictureTypeP;
                    break;
                case 'B':
                case 'b':
                    mpeg4parameters->nAllowedPictureTypes |=
                        OMX_VIDEO_PictureTypeB;
                    break;
                case 'S':
                case 's':
                    mpeg4parameters->nAllowedPictureTypes |=
                        OMX_VIDEO_PictureTypeS;
                    break;
                default:
                    return OMX_ErrorBadParameter;
                }
            }
            while(j);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_avc_parameters(int argc, char **args,
                                     OMX_VIDEO_PARAM_AVCTYPE * parameters)
{
    int i = 0;

#if defined (ENCVC8000E) || defined (ENCH2V41)
    parameters->eProfile = OMX_VIDEO_AVCProfileHigh;
    parameters->eLevel = OMX_VIDEO_AVCLevel51;
#else
    parameters->eProfile = OMX_VIDEO_AVCProfileBaseline;
#if defined (ENC8290) || defined (ENCH1)
    parameters->eLevel = OMX_VIDEO_AVCLevel4;
#else
    parameters->eLevel = OMX_VIDEO_AVCLevel3;
#endif
#endif

    while(++i < argc)
    {

        if(strcmp(args[i], "-p") == 0 || strcmp(args[i], "--profile") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for profile is missing.\n");
            parameters->eProfile = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");

            switch (atoi(args[i]))
            {
                /*
                 * H264ENC_BASELINE_LEVEL_1 = 10,
                 * H264ENC_BASELINE_LEVEL_1_b = 99,
                 * H264ENC_BASELINE_LEVEL_1_1 = 11,
                 * H264ENC_BASELINE_LEVEL_1_2 = 12,
                 * H264ENC_BASELINE_LEVEL_1_3 = 13,
                 * H264ENC_BASELINE_LEVEL_2 = 20,
                 * H264ENC_BASELINE_LEVEL_2_1 = 21,
                 * H264ENC_BASELINE_LEVEL_2_2 = 22,
                 * H264ENC_BASELINE_LEVEL_3 = 30,
                 * H264ENC_BASELINE_LEVEL_3_1 = 31,
                 * H264ENC_BASELINE_LEVEL_3_2 = 32
                 */
            case 10:
                parameters->eLevel = OMX_VIDEO_AVCLevel1;
                break;

            case 99:
                parameters->eLevel = OMX_VIDEO_AVCLevel1b;
                break;

            case 11:
                parameters->eLevel = OMX_VIDEO_AVCLevel11;
                break;

            case 12:
                parameters->eLevel = OMX_VIDEO_AVCLevel12;
                break;

            case 13:
                parameters->eLevel = OMX_VIDEO_AVCLevel13;
                break;

            case 20:
                parameters->eLevel = OMX_VIDEO_AVCLevel2;
                break;

            case 21:
                parameters->eLevel = OMX_VIDEO_AVCLevel21;
                break;

            case 22:
                parameters->eLevel = OMX_VIDEO_AVCLevel22;
                break;

            case 30:
                parameters->eLevel = OMX_VIDEO_AVCLevel3;
                break;

            case 31:
                parameters->eLevel = OMX_VIDEO_AVCLevel31;
                break;

            case 32:
                parameters->eLevel = OMX_VIDEO_AVCLevel32;
                break;
            case 40:
                parameters->eLevel = OMX_VIDEO_AVCLevel4;
                break;
            case 41:
                parameters->eLevel = OMX_VIDEO_AVCLevel41;
                break;
#if defined (ENC8290) || defined (ENCH1)
            case 42:
                parameters->eLevel = OMX_VIDEO_AVCLevel42;
                break;
            case 50:
                parameters->eLevel = OMX_VIDEO_AVCLevel5;
                break;
            case 51:
                parameters->eLevel = OMX_VIDEO_AVCLevel51;
                break;
            default:
                parameters->eLevel = OMX_VIDEO_AVCLevel4;
                break;
#elif defined (ENCVC8000E) || defined (ENCH2V41)
			default:
                parameters->eLevel = OMX_VIDEO_AVCLevel51;
                break;
#else
			default:
                parameters->eLevel = OMX_VIDEO_AVCLevel3;
                break;
#endif
            }
//            parameters->eLevel = OMX_VIDEO_AVCLevel51;
        }
        else if(strcmp(args[i], "-n") == 0 ||
                strcmp(args[i], "--npframes") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of P frames is missing.\n");
            parameters->nPFrames = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_parameters_bitrate(int argc, char **args,
                                         OMX_VIDEO_PARAM_BITRATETYPE * bitrate)
{
    int i = 0;

    bitrate->eControlRate = OMX_Video_ControlRateDisable;

    while(++i < argc)
    {

        if(strcmp(args[i], "-C") == 0 || strcmp(args[i], "--control-rate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for control rate is missing.\n");

            if(strcasecmp(args[i], "disable") == 0)
                bitrate->eControlRate = OMX_Video_ControlRateDisable;
            else if(strcasecmp(args[i], "variable") == 0)
                bitrate->eControlRate = OMX_Video_ControlRateVariable;
            else if(strcasecmp(args[i], "constant") == 0)
                bitrate->eControlRate = OMX_Video_ControlRateConstant;
            else if(strcasecmp(args[i], "variable-skipframes") == 0)
                bitrate->eControlRate = OMX_Video_ControlRateVariableSkipFrames;
            else if(strcasecmp(args[i], "constant-skipframes") == 0)
                bitrate->eControlRate = OMX_Video_ControlRateConstantSkipFrames;
            else
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                               "Invalid control rate type\n");
                return OMX_ErrorBadParameter;
            }
        }
        /* duplicate for common parameters */
        if(strcmp(args[i], "-B") == 0 ||
           strcmp(args[i], "--bitsPerSecond") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bit rate is missing.\n");
            bitrate->nTargetBitrate = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }
    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_avc_parameters_deblocking(int argc, char **args,
                                                OMX_PARAM_DEBLOCKINGTYPE *
                                                deblocking)
{
    int i = 0;
    deblocking->bDeblocking = OMX_TRUE; //OMX_FALSE;

    while(++i < argc)
    {

        if(strcmp(args[i], "-d") == 0 || strcmp(args[i], "--deblocking") == 0)
        {
            deblocking->bDeblocking = OMX_TRUE;
            break;
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_parameters_quantization(int argc, char **args,
                                              OMX_VIDEO_PARAM_QUANTIZATIONTYPE *
                                              quantization)
{
    int i = 0;

    while(++i < argc)
    {

        if(strcmp(args[i], "-q") == 0 || strcmp(args[i], "--qpp") == 0 ||
           strcmp(args[i], "-qLevel") == 0 )
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for QPP is missing.\n");
            quantization->nQpI = atoi(args[i]);
            break;
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_parameters_avc_extension(int argc, char **args,
                                              OMX_VIDEO_PARAM_AVCEXTTYPE *
                                              extensions)
{
    int i = 0;

    while(++i < argc)
    {
        if(strcmp(args[i], "-a") == 0 ||
            strcmp(args[i], "--firstVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for firstVop is missing.\n");
            extensions->firstPic = atoi(args[i]);
        }
        else if(strcmp(args[i], "-b") == 0 ||
                 strcmp(args[i], "--lastVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for lastVop is missing.\n");
            extensions->lastPic = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gopSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for adaptive GOP size is missing.\n");
            extensions->gopSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cpbSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for hrd cpbSize is missing.\n");
            extensions->hrdCpbSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--mmuEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for mmuEnable is missing.\n");
            extensions->mmuEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramLumHeightBwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramLumHeightBwd is missing.\n");
            extensions->extSramLumHeightBwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramChrHeightBwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramChrHeightBwd is missing.\n");
            extensions->extSramChrHeightBwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramLumHeightFwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramLumHeightFwd is missing.\n");
            extensions->extSramLumHeightFwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramChrHeightFwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramChrHeightFwd is missing.\n");
            extensions->extSramChrHeightFwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--AXIAlignment") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for AXIAlignment is missing.\n");
            extensions->AXIAlignment = atoi(args[i]);
        }
        else if(strcmp(args[i], "--codedChromaIdc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for codedChromaIdc is missing.\n");
            extensions->codedChromaIdc = atoi(args[i]);
        }
        else if(strcmp(args[i], "--aq_mode") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for aq_mode is missing.\n");
            extensions->aq_mode = atoi(args[i]);
        }
        else if(strcmp(args[i], "--aq_strength") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for aq_strength is missing.\n");
            extensions->aq_strength = FLOAT_Q16(strtod(args[i], 0));
        }
        else if(strcmp(args[i], "--writeReconToDDR") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for writeReconToDDR is missing.\n");
            extensions->writeReconToDDR = atoi(args[i]);
        }
        else if(strcmp(args[i], "--TxTypeSearchEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for TxTypeSearchEnable is missing.\n");
            extensions->TxTypeSearchEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--PsyFactor") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for PsyFactor is missing.\n");
            extensions->PsyFactor = atoi(args[i]);
        }
        else if(strcmp(args[i], "--MEVertRange") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for MEVertRange is missing.\n");
            extensions->meVertSearchRange = atoi(args[i]);
        }
        else if(strcmp(args[i], "--layerInRefIdc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for layerInRefIdc is missing.\n");
            extensions->layerInRefIdcEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--rdoLevel") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Programable HW RDO Level is missing.\n");
            extensions->rdoLevel = atoi(args[i]);
        }
        else if(strcmp(args[i], "--crf") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for crf is missing.\n");
            extensions->crf = atoi(args[i]);
        }
        else if(strcmp(args[i], "--preset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for preset is missing.\n");
            extensions->preset = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE process_parameters_image_qfactor(int argc, char **args,
                                               OMX_IMAGE_PARAM_QFACTORTYPE *
                                               quantization)
{
    int i = 0;

    while(++i < argc)
    {

        if(strcmp(args[i], "-q") == 0 || strcmp(args[i], "--qfactor") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for QFactor is missing.\n");
            quantization->nQFactor = atoi(args[i]);

            break;
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE process_h263_parameters(int argc, char **args,
                                      OMX_VIDEO_PARAM_H263TYPE * parameters)
{
    int i, j;

    parameters->eProfile = OMX_VIDEO_H263ProfileBaseline;
    parameters->eLevel = OMX_VIDEO_H263Level70;

    i = 0;
    while(++i < argc)
    {

        if(strcmp(args[i], "-p") == 0 || strcmp(args[i], "--profile") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for profile is missing.\n");
            parameters->eProfile = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");
            parameters->eLevel = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-P") == 0 ||
                strcmp(args[i], "--plus-type") == 0)
        {
            parameters->bPLUSPTYPEAllowed = OMX_TRUE;
        }
        else if(strcmp(args[i], "-U") == 0 ||
                strcmp(args[i], "--picture-types") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for allowed picture types is missing.\n");

            j = strlen(args[i]);
            do
            {
                switch (args[i][--j])
                {
                case 'i':
                    parameters->nAllowedPictureTypes |= OMX_VIDEO_PictureTypeI;
                    break;
                case 'p':
                    parameters->nAllowedPictureTypes |= OMX_VIDEO_PictureTypeP;
                    break;
                case 'b':
                    parameters->nAllowedPictureTypes |= OMX_VIDEO_PictureTypeB;
                    break;
                case 'I':
                    parameters->nAllowedPictureTypes |= OMX_VIDEO_PictureTypeSI;
                    break;
                case 'P':
                    parameters->nAllowedPictureTypes |= OMX_VIDEO_PictureTypeSP;
                    break;
                default:
                    return OMX_ErrorBadParameter;
                }
            }
            while(j);
        }
        else if(strcmp(args[i], "-n") == 0 ||
                strcmp(args[i], "--npframes") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of P frames is missing.\n");
            parameters->nPFrames = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_vp8_parameters(int argc, char **args,
                                     OMX_VIDEO_PARAM_VP8TYPE * parameters,
                                     OMXCLIENT * appdata)
{
    int i;

    parameters->eProfile = OMX_VIDEO_VP8ProfileMain;
    parameters->eLevel = OMX_VIDEO_VP8Level_Version0;
    parameters->nDCTPartitions = 0;
    parameters->bErrorResilientMode = 0;

    i = 0;
    while(++i < argc)
    {

        if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");
            parameters->eLevel = atoi(args[i]);
        }
        else if(strcmp(args[i], "-d") == 0 ||
                strcmp(args[i], "--dctPartitions") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of DCT partitions is missing.\n");
            parameters->nDCTPartitions = atoi(args[i]);
        }
        else if(strcmp(args[i], "-R") == 0 ||
                strcmp(args[i], "--errorResilient") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for error resilient mode is missing.\n");
            parameters->bErrorResilientMode = atoi(args[i]);
        }
        else if(strcmp(args[i], "-V") == 0 ||
                strcmp(args[i], "--vp8IntraPicRate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for VP8 intra picture rate is missing.\n");
            appdata->vp8IntraPicRate = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}

#define DEFAULT -255
/*
*/
#ifdef ENCVC8000E
OMX_ERRORTYPE process_hevc_parameters(int argc, char **args,
                                     OMX_VIDEO_PARAM_HEVCTYPE * parameters)
{
    int i;
    char *endp;

    i = 0;
    while(++i < argc)
    {

        if(strcmp(args[i], "-p") == 0 || strcmp(args[i], "--profile") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for profile is missing.\n");
            parameters->eProfile = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");

            switch (atoi(args[i]))
            {
            case 30:
                parameters->eLevel = OMX_VIDEO_HEVCLevel1;
                break;

            case 60:
                parameters->eLevel = OMX_VIDEO_HEVCLevel2;
                break;

            case 63:
                parameters->eLevel = OMX_VIDEO_HEVCLevel21;
                break;

            case 90:
                parameters->eLevel = OMX_VIDEO_HEVCLevel3;
                break;

            case 93:
                parameters->eLevel = OMX_VIDEO_HEVCLevel31;
                break;

            case 120:
                parameters->eLevel = OMX_VIDEO_HEVCLevel4;
                break;

            case 123:
                parameters->eLevel = OMX_VIDEO_HEVCLevel41;
                break;

            case 150:
                parameters->eLevel = OMX_VIDEO_HEVCLevel5;
                break;

            case 153:
                parameters->eLevel = OMX_VIDEO_HEVCLevel51;
                break;

            case 156:
                parameters->eLevel = OMX_VIDEO_HEVCLevel52;
                break;
            case 180:
                parameters->eLevel = OMX_VIDEO_HEVCLevel6;
                break;
            case 183:
                parameters->eLevel = OMX_VIDEO_HEVCLevel61;
                break;
            case 186:
                parameters->eLevel = OMX_VIDEO_HEVCLevel62;
                break;
			default:
                parameters->eLevel = OMX_VIDEO_HEVCLevel6;
                break;
            }
        }
        else if(strcmp(args[i], "-n") == 0 ||
                strcmp(args[i], "--npframes") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of P frames is missing.\n");
            parameters->nPFrames = atoi(args[i]);
        }
        else if(strcmp(args[i], "--nTcOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE_SIGNED(i, args, argc,
                                        "Parameter for nTcOffset is missing.\n");
            parameters->nTcOffset = strtol(args[i], &endp, 10);

            if (*endp)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                    "Parameter for nTcOffset is missing.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "--nBetaOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE_SIGNED(i, args, argc,
                                        "Parameter for nBetaOffset is missing.\n");
            parameters->nBetaOffset = strtol(args[i], &endp, 10);

            if (*endp)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                    "Parameter for nBetaOffset is missing.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "--bEnableDeblockOverride") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableDeblockOverride is missing.\n");
            parameters->bEnableDeblockOverride = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bDeblockOverride") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bDeblockOverride is missing.\n");
            parameters->bDeblockOverride = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bEnableSAO") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableSAO is missing.\n");
            parameters->bEnableSAO = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bEnableScalingList") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableScalingList is missing.\n");
            parameters->bEnableScalingList = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bCabacInitFlag") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bCabacInitFlag is missing.\n");
            parameters->bCabacInitFlag = atoi(args[i]);
        }
        else if(strcmp(args[i], "-R") == 0 ||
                strcmp(args[i], "--intraPicRate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Intra picture rate in frames is missing.\n");
            parameters->intraPicRate = atoi(args[i]);
        }
        else if(strcmp(args[i], "--ssim") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Enable/Disable SSIM Calculation is missing.\n");
            parameters->ssim = atoi(args[i]);
        }
        else if(strcmp(args[i], "--rdoLevel") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Programable HW RDO Level is missing.\n");
            parameters->rdoLevel = atoi(args[i]);
        }
        else if(strcmp(args[i], "--inputAlignmentExp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Set alignment value is missing.\n");
            parameters->exp_of_input_alignment = atoi(args[i]);
        }
        else if(strcmp(args[i], "-u") == 0 ||
                strcmp(args[i], "--ctbRc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for CTB QP adjustment mode for Rate Control and Subjective Quality is missing.\n");
            parameters->ctbRc = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gopSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for adaptive GOP size is missing.\n");
            parameters->gopSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cuInfoVersion") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for cuInfoVersion is missing.\n");
            parameters->cuInfoVersion = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gdrDuration") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for how many pictures(frames not fields) it will take to do GDR is missing.\n");
            parameters->gdrDuration = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roiMapDeltaQpBlockUnit") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roiMapDeltaQpBlockUnit is missing.\n");
            parameters->roiMapDeltaQpBlockUnit = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roiMapDeltaQpEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roiMapDeltaQpEnable is missing.\n");
            parameters->roiMapDeltaQpEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--RoiQpDeltaVer") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Roi Qp Delta map version number is missing.\n");
            parameters->RoiQpDelta_ver = atoi(args[i]);
        }
        else if(strcmp(args[i], "-N") == 0 ||
                strcmp(args[i], "--byteStream") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Stream type is missing.\n");
            parameters->byteStream = atoi(args[i]);
        }
        else if(strcmp(args[i], "-e") == 0 ||
                strcmp(args[i], "--sliceSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Slice Size is missing.\n");
            parameters->sliceSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gopLowdelay") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Use default lowDelay GOP configuration is missing.\n");
            parameters->gopLowdelay = atoi(args[i]);
        }
#if 0
        else if(strcmp(args[i], "-p") == 0 ||
                strcmp(args[i], "--cabacInitFlag") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Initialization value for CABAC is missing.\n");
            parameters->cabacInitFlag = atoi(args[i]);
        }
#endif
        else if(strcmp(args[i], "-K") == 0 ||
                strcmp(args[i], "--enableCabac") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for CABAC enable is missing.\n");
            parameters->enableCabac = atoi(args[i]);
        }
        else if(strcmp(args[i], "-k") == 0 ||
                strcmp(args[i], "--videoRange") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Video signal sample range value is missing.\n");
            parameters->videoRange = atoi(args[i]);
        }
        else if(strcmp(args[i], "--enableRdoQuant") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Enable/Disable RDO Quant is missing.\n");
            parameters->enableRdoQuant = atoi(args[i]);
        }
        else if(strcmp(args[i], "--fieldOrder") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Interlaced field order is missing.\n");
            parameters->fieldOrder = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cir") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for interval for Cyclic Intra Refresh is missing.\n");
            /* Argument must be "xx:yy", replace ':' with 0 */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->cirStart = atoi(args[i]);
            /* xx is cir start */
            args[i] += j+1;
            /* yy is cir interval */
            parameters->cirInterval = atoi(args[i]);
            OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "Cyclic Intra Refresh: start %d, interval %d \n", parameters->cirStart,
                parameters->cirInterval);
        }
        else if(strcmp(args[i], "--tile") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for HEVC Tile setting is missing.\n");
            /* Argument must be "xx:yy:zz" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            /* xx is num_tile_columns*/
            parameters->num_tile_columns = atoi(args[i]);
            args[i] += j+1;

            /* yy is num_tile_rows */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->num_tile_rows = atoi(args[i]);
            args[i] += j+1;

            /* zz is num_tile_rows */
            parameters->loop_filter_across_tiles_enabled_flag = atoi(args[i]);
            OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "HEVC Tile setting: columns %d, rows %d, filter across tiles %d \n", parameters->num_tile_columns,
                parameters->num_tile_rows, parameters->loop_filter_across_tiles_enabled_flag);
        }
        else if(strcmp(args[i], "--HDR10_display") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for HDR10_display is missing.\n");

            parameters->hdr10_display_enable = 1;
            /* Argument must be "xx:yy:zz" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dx0 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dy0 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dx1 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dy1 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dx2 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_dy2 = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_wx = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_wy = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_maxluma = atoi(args[i]);
            args[i] += j+1;

            parameters->hdr10_minluma = atoi(args[i]);
        }
        else if(strcmp(args[i], "--HDR10_lightlevel") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for HDR10_lightlevel is missing.\n");

            parameters->hdr10_lightlevel_enable = 1;
            /* Argument must be "xx:yy:zz" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_maxlight = atoi(args[i]);
            args[i] += j+1;

            parameters->hdr10_avglight = atoi(args[i]);
        }
        else if(strcmp(args[i], "--HDR10_colordescription") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for HDR10_colordescription is missing.\n");

            parameters->hdr10_color_enable = 1;
            /* Argument must be "xx:yy:zz" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_primary = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->hdr10_transfer = atoi(args[i]);
            args[i] += j+1;

            parameters->hdr10_matrix = atoi(args[i]);
        }
        else if(strcmp(args[i], "--ipcmFilterDisable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ipcmFilterDisable is missing.\n");
            parameters->pcm_loop_filter_disabled_flag = atoi(args[i]);
        }
        else if(strcmp(args[i], "--ipcmMapEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ipcmMapEnable is missing.\n");
            parameters->ipcmMapEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi1Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi1Qp is missing.\n");
            parameters->roiQp[0] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi2Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi2Qp is missing.\n");
            parameters->roiQp[1] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi3Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi3Qp is missing.\n");
            parameters->roiQp[2] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi4Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi4Qp is missing.\n");
            parameters->roiQp[3] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi5Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi5Qp is missing.\n");
            parameters->roiQp[4] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi6Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi6Qp is missing.\n");
            parameters->roiQp[5] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi7Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi7Qp is missing.\n");
            parameters->roiQp[6] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi8Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi8Qp is missing.\n");
            parameters->roiQp[7] = atoi(args[i]);
        }
        else if(strcmp(args[i], "-I") == 0 ||
                strcmp(args[i], "--chromaQpOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for chromaQpOffset is missing.\n");
            parameters->chromaQpOffset = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseReductionEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseReductionEnable is missing.\n");
            parameters->noiseReductionEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseLow") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseLow is missing.\n");
            parameters->noiseLow = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseFirstFrameSigma") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseFirstFrameSigma is missing.\n");
            parameters->noiseFirstFrameSigma = atoi(args[i]);
        }
        else if(strcmp(args[i], "--RPSInSliceHeader") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for RPSInSliceHeader is missing.\n");
            parameters->RpsInSliceHeader = atoi(args[i]);
        }
        else if(strcmp(args[i], "--blockRCSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for blockRCSize is missing.\n");
            parameters->blockRCSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--rcQpDeltaRange") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for rcQpDeltaRange is missing.\n");
            parameters->rcQpDeltaRange = atoi(args[i]);
        }
        else if(strcmp(args[i], "--rcBaseMBComplexity") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for rcBaseMBComplexity is missing.\n");
            parameters->rcBaseMBComplexity = atoi(args[i]);
        }
        else if(strcmp(args[i], "--picQpDeltaRange") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for picQpDeltaRange is missing.\n");

            /* Argument must be "xx:yy" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->picQpDeltaMin = atoi(args[i]);
            args[i] += j+1;

            parameters->picQpDeltaMax = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeI") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeI is missing.\n");
            parameters->bitVarRangeI = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeP") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeP is missing.\n");
            parameters->bitVarRangeP = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeB") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeB is missing.\n");
            parameters->bitVarRangeB = atoi(args[i]);
        }
        else if(strcmp(args[i], "--tolMovingBitRate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for tolMovingBitRate is missing.\n");
            parameters->tolMovingBitRate = atoi(args[i]);
        }
        else if(strcmp(args[i], "--tolCtbRcInter") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for tolCtbRcInter is missing.\n");
            parameters->tolCtbRcInter = FLOAT_Q16(atof(args[i]));
        }
        else if(strcmp(args[i], "--tolCtbRcIntra") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for tolCtbRcIntra is missing.\n");
            parameters->tolCtbRcIntra = atoi(args[i]);
        }
        else if(strcmp(args[i], "--ctbRowQpStep") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ctbRowQpStep is missing.\n");
            parameters->ctbRowQpStep = atoi(args[i]);
        }
        else if(strcmp(args[i], "--LTR") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for LTR is missing.\n");

            /* Argument must be "ww:xx:yy[:zz]" */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->ltrInterval = atoi(args[i]);
            args[i] += j+1;

            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->longTermGapOffset = atoi(args[i]);
            args[i] += j+1;

            j = ParseDelim(args[i], ':');
            parameters->longTermGap = atoi(args[i]);
            if (j >= 0)
            {
                args[i] += j+1;
                parameters->longTermQpDelta = atoi(args[i]);
            }
        }
        else if(strcmp(args[i], "--monitorFrames") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for monitorFrames is missing.\n");
            parameters->monitorFrames = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitrateWindow") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitrateWindow is missing.\n");
            parameters->bitrateWindow = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cpbSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for hrd-cpbSize is missing.\n");
            parameters->hrdCpbSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-A") == 0 ||
                strcmp(args[i], "--intraQpDelta") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for intraQpDelta is missing.\n");
            parameters->intraQpDelta = atoi(args[i]);
        }
        else if(strcmp(args[i], "-G") == 0 ||
                strcmp(args[i], "--fixedIntraQp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for fixedIntraQp is missing.\n");
            parameters->fixedIntraQp = atoi(args[i]);
        }
        else if(strcmp(args[i], "--vbr") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for vbr is missing.\n");
            parameters->vbr = atoi(args[i]);
        }
        else if(strcmp(args[i], "--smoothPsnrInGOP") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for smoothPsnrInGOP is missing.\n");
            parameters->smoothPsnrInGOP = atoi(args[i]);
        }
        else if(strcmp(args[i], "--staticSceneIbitPercent") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for staticSceneIbitPercent is missing.\n");
            parameters->staticSceneIbitPercent = atoi(args[i]);
        }
        else if(strcmp(args[i], "--picSkip") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for picSkip is missing.\n");
            parameters->picSkip = atoi(args[i]);
        }
        else if(strcmp(args[i], "--enableVuiTimingInfo") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for enableVuiTimingInfo is missing.\n");
            parameters->vui_timing_info_enable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--hashtype") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for hashtype is missing.\n");
            parameters->hashtype = atoi(args[i]);
        }
        else if(strcmp(args[i], "-a") == 0 ||
                strcmp(args[i], "--firstVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for firstVop is missing.\n");
            parameters->firstPic = atoi(args[i]);
        }
        else if(strcmp(args[i], "-b") == 0 ||
                strcmp(args[i], "--lastVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for lastVop is missing.\n");
            parameters->lastPic = atoi(args[i]);
        }
        else if(strcmp(args[i], "--mmuEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for mmuEnable is missing.\n");
            parameters->mmuEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramLumHeightBwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramLumHeightBwd is missing.\n");
            parameters->extSramLumHeightBwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramChrHeightBwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramChrHeightBwd is missing.\n");
            parameters->extSramChrHeightBwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramLumHeightFwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramLumHeightFwd is missing.\n");
            parameters->extSramLumHeightFwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--extSramChrHeightFwd") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for extSramChrHeightFwd is missing.\n");
            parameters->extSramChrHeightFwd = atoi(args[i]);
        }
        else if(strcmp(args[i], "--AXIAlignment") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for AXIAlignment is missing.\n");
            parameters->AXIAlignment = atoi(args[i]);
        }
        else if(strcmp(args[i], "--codedChromaIdc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for codedChromaIdc is missing.\n");
            parameters->codedChromaIdc = atoi(args[i]);
        }
        else if(strcmp(args[i], "--aq_mode") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for aq_mode is missing.\n");
            parameters->aq_mode = atoi(args[i]);
        }
        else if(strcmp(args[i], "--aq_strength") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for aq_strength is missing.\n");
            parameters->aq_strength = FLOAT_Q16(strtod(args[i], 0));
        }
        else if(strcmp(args[i], "--writeReconToDDR") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for writeReconToDDR is missing.\n");
            parameters->writeReconToDDR = atoi(args[i]);
        }
        else if(strcmp(args[i], "--TxTypeSearchEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for TxTypeSearchEnable is missing.\n");
            parameters->TxTypeSearchEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--PsyFactor") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for PsyFactor is missing.\n");
            parameters->PsyFactor = atoi(args[i]);
        }
        else if(strcmp(args[i], "--MEVertRange") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for MEVertRange is missing.\n");
            parameters->meVertSearchRange = atoi(args[i]);
        }
        else if(strcmp(args[i], "--layerInRefIdc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for layerInRefIdc is missing.\n");
            parameters->layerInRefIdcEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--crf") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for crf is missing.\n");
            parameters->crf = atoi(args[i]);
        }
        else if(strcmp(args[i], "--preset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for preset is missing.\n");
            parameters->preset = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}
#endif //ENCVC8000E

#ifdef ENCH2V41
OMX_ERRORTYPE process_hevc_parameters(int argc, char **args,
                                     OMX_VIDEO_PARAM_HEVCTYPE * parameters)
{
    int i;
    char *endp;

    i = 0;
    while(++i < argc)
    {

        if(strcmp(args[i], "-p") == 0 || strcmp(args[i], "--profile") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for profile is missing.\n");
            parameters->eProfile = strtol(args[i], 0, 16);
        }
        else if(strcmp(args[i], "-L") == 0 || strcmp(args[i], "--level") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for level is missing.\n");

            switch (atoi(args[i]))
            {
            case 30:
                parameters->eLevel = OMX_VIDEO_HEVCLevel1;
                break;

            case 60:
                parameters->eLevel = OMX_VIDEO_HEVCLevel2;
                break;

            case 63:
                parameters->eLevel = OMX_VIDEO_HEVCLevel21;
                break;

            case 90:
                parameters->eLevel = OMX_VIDEO_HEVCLevel3;
                break;

            case 93:
                parameters->eLevel = OMX_VIDEO_HEVCLevel31;
                break;

            case 120:
                parameters->eLevel = OMX_VIDEO_HEVCLevel4;
                break;

            case 123:
                parameters->eLevel = OMX_VIDEO_HEVCLevel41;
                break;

            case 150:
                parameters->eLevel = OMX_VIDEO_HEVCLevel5;
                break;

            case 153:
                parameters->eLevel = OMX_VIDEO_HEVCLevel51;
                break;

            case 156:
                parameters->eLevel = OMX_VIDEO_HEVCLevel52;
                break;
            case 180:
                parameters->eLevel = OMX_VIDEO_HEVCLevel6;
                break;
            case 183:
                parameters->eLevel = OMX_VIDEO_HEVCLevel61;
                break;
            case 186:
                parameters->eLevel = OMX_VIDEO_HEVCLevel62;
                break;
			default:
                parameters->eLevel = OMX_VIDEO_HEVCLevel6;
                break;
            }
        }
        else if(strcmp(args[i], "-n") == 0 ||
                strcmp(args[i], "--npframes") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for number of P frames is missing.\n");
            parameters->nPFrames = atoi(args[i]);
        }
        else if(strcmp(args[i], "--nTcOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE_SIGNED(i, args, argc,
                                        "Parameter for nTcOffset is missing.\n");
            parameters->nTcOffset = strtol(args[i], &endp, 10);

            if (*endp)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                    "Parameter for nTcOffset is missing.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "--nBetaOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE_SIGNED(i, args, argc,
                                        "Parameter for nBetaOffset is missing.\n");
            parameters->nBetaOffset = strtol(args[i], &endp, 10);

            if (*endp)
            {
                OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                    "Parameter for nBetaOffset is missing.\n");
                return OMX_ErrorBadParameter;
            }
        }
        else if(strcmp(args[i], "--bEnableDeblockOverride") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableDeblockOverride is missing.\n");
            parameters->bEnableDeblockOverride = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bDeblockOverride") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bDeblockOverride is missing.\n");
            parameters->bDeblockOverride = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bEnableSAO") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableSAO is missing.\n");
            parameters->bEnableSAO = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bEnableScalingList") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bEnableScalingList is missing.\n");
            parameters->bEnableScalingList = atoi(args[i]);
        }
        else if(strcmp(args[i], "-R") == 0 ||
                strcmp(args[i], "--intraPicRate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Intra picture rate in frames is missing.\n");
            parameters->intraPicRate = atoi(args[i]);
        }
        else if(strcmp(args[i], "--inputAlignmentExp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Set alignment value is missing.\n");
            parameters->exp_of_input_alignment = atoi(args[i]);
        }
        else if(strcmp(args[i], "-u") == 0 ||
                strcmp(args[i], "--ctbRc") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for CTB QP adjustment mode for Rate Control and Subjective Quality is missing.\n");
            parameters->ctbRc = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gopSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for adaptive GOP size is missing.\n");
            parameters->gopSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cuInfoVersion") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for cuInfoVersion is missing.\n");
            parameters->cuInfoVersion = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gdrDuration") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for how many pictures(frames not fields) it will take to do GDR is missing.\n");
            parameters->gdrDuration = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roiMapDeltaQpBlockUnit") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roiMapDeltaQpBlockUnit is missing.\n");
            parameters->roiMapDeltaQpBlockUnit = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roiMapDeltaQpEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roiMapDeltaQpEnable is missing.\n");
            parameters->roiMapDeltaQpEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "-N") == 0 ||
                strcmp(args[i], "--byteStream") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Stream type is missing.\n");
            parameters->byteStream = atoi(args[i]);
        }
        else if(strcmp(args[i], "-e") == 0 ||
                strcmp(args[i], "--sliceSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Slice Size is missing.\n");
            parameters->sliceSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--gopLowdelay") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Use default lowDelay GOP configuration is missing.\n");
            parameters->gopLowdelay = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bCabacInitFlag") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bCabacInitFlag is missing.\n");
            parameters->bCabacInitFlag = atoi(args[i]);
        }
        else if(strcmp(args[i], "-K") == 0 ||
                strcmp(args[i], "--enableCabac") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for CABAC enable is missing.\n");
            parameters->enableCabac = atoi(args[i]);
        }
        else if(strcmp(args[i], "-k") == 0 ||
                strcmp(args[i], "--videoRange") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Video signal sample range value is missing.\n");
            parameters->videoRange = atoi(args[i]);
        }
        else if(strcmp(args[i], "--fieldOrder") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for Interlaced field order is missing.\n");
            parameters->fieldOrder = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cir") == 0)
        {
            int j;
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for interval for Cyclic Intra Refresh is missing.\n");
            /* Argument must be "xx:yy", replace ':' with 0 */
            if ((j = ParseDelim(args[i], ':')) == -1) break;
            parameters->cirStart = atoi(args[i]);
            /* xx is cir start */
            args[i] += j+1;
            /* yy is cir interval */
            parameters->cirInterval = atoi(args[i]);
            OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO, "Cyclic Intra Refresh: start %d, interval %d \n", parameters->cirStart,
                parameters->cirInterval);
        }
        else if(strcmp(args[i], "--ipcmFilterDisable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ipcmFilterDisable is missing.\n");
            parameters->pcm_loop_filter_disabled_flag = atoi(args[i]);
        }
        else if(strcmp(args[i], "--ipcmMapEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for ipcmMapEnable is missing.\n");
            parameters->ipcmMapEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi1Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi1Qp is missing.\n");
            parameters->roiQp[0] = atoi(args[i]);
        }
        else if(strcmp(args[i], "--roi2Qp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for roi2Qp is missing.\n");
            parameters->roiQp[1] = atoi(args[i]);
        }
        else if(strcmp(args[i], "-I") == 0 ||
                strcmp(args[i], "--chromaQpOffset") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for chromaQpOffset is missing.\n");
            parameters->chromaQpOffset = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseReductionEnable") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseReductionEnable is missing.\n");
            parameters->noiseReductionEnable = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseLow") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseLow is missing.\n");
            parameters->noiseLow = atoi(args[i]);
        }
        else if(strcmp(args[i], "--noiseFirstFrameSigma") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for noiseFirstFrameSigma is missing.\n");
            parameters->noiseFirstFrameSigma = atoi(args[i]);
        }
        else if(strcmp(args[i], "--blockRCSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for blockRCSize is missing.\n");
            parameters->blockRCSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeI") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeI is missing.\n");
            parameters->bitVarRangeI = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeP") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeP is missing.\n");
            parameters->bitVarRangeP = atoi(args[i]);
        }
        else if(strcmp(args[i], "--bitVarRangeB") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for bitVarRangeB is missing.\n");
            parameters->bitVarRangeB = atoi(args[i]);
        }
        else if(strcmp(args[i], "--tolMovingBitRate") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for tolMovingBitRate is missing.\n");
            parameters->tolMovingBitRate = atoi(args[i]);
        }
        else if(strcmp(args[i], "--monitorFrames") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for monitorFrames is missing.\n");
            parameters->monitorFrames = atoi(args[i]);
        }
        else if(strcmp(args[i], "--cpbSize") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for hrd-cpbSize is missing.\n");
            parameters->hrdCpbSize = atoi(args[i]);
        }
        else if(strcmp(args[i], "-A") == 0 ||
                strcmp(args[i], "--intraQpDelta") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for intraQpDelta is missing.\n");
            parameters->intraQpDelta = atoi(args[i]);
        }
        else if(strcmp(args[i], "-G") == 0 ||
                strcmp(args[i], "--fixedIntraQp") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for fixedIntraQp is missing.\n");
            parameters->fixedIntraQp = atoi(args[i]);
        }
        else if(strcmp(args[i], "--picSkip") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for picSkip is missing.\n");
            parameters->picSkip = atoi(args[i]);
        }
        else if(strcmp(args[i], "-a") == 0 ||
                strcmp(args[i], "--firstVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for firstVop is missing.\n");
            parameters->firstPic = atoi(args[i]);
        }
        else if(strcmp(args[i], "-b") == 0 ||
                strcmp(args[i], "--lastVop") == 0)
        {
            OMXENCODER_CHECK_NEXT_VALUE(i, args, argc,
                                        "Parameter for lastVop is missing.\n");
            parameters->lastPic = atoi(args[i]);
        }
        else
        {
            /* Do nothing, purpose is to traverse all options and to find known ones */
        }
    }

    return OMX_ErrorNone;
}
#endif //ENCH2V41
