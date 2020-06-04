/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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
--                                                                            --
--  Abstract : Video stabilization stnadalone testbench
--
------------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "EncGetOption.h"

#include "vidstbapi.h"

/* For accessing the EWL instance inside the video stabilizer instance */
#include "vidstabinternal.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define DEFAULT -1

/* Global variables */


/* SW/HW shared memories for input/output buffers */
static EWLLinearMem_t pictureMem;
static EWLLinearMem_t nextPictureMem;

static FILE *yuvFile = NULL;

i32 trigger_point = -1;      /* Logic Analyzer trigger point */

static option_s option[] = {
    {"help", '?', 0},
    {"firstPic", 'a', 1},
    {"lastPic", 'b', 1},
    {"width", 'W', 1},
    {"height", 'H', 1},
    {"lumWidthSrc", 'w', 1},
    {"lumHeightSrc", 'h', 1},
    {"inputFormat", 'l', 1},    /* Input image format */
    {"input", 'i', 1},          /* Input filename */
    {"output", 'o', 1},         /* Output filename */
    {"traceresult", 'T', 0},    /* trace result to file <video_stab_result.log>, no armument */
    {"burstSize", 'N', 1},  /* Coded Picture Buffer Size */
    {"trigger", 'P', 1},
    {NULL, 0, 0}
};

const char *strFormat[14] =
    {"YUV 4:2:0 planar", "YUV 4:2:0 semiplanar", "YUV 4:2:2 (YUYV)",
     "YUV 4:2:2 (UYVY)", "RGB 565", "BGR 565", "RGB 555", "BGR 555",
     "RGB 444", "BGR 444", "RGB 888", "BGR 888", "RGB 101010", "BGR 101010"
};

/* Structure for command line options */
typedef struct
{
    char *input;
    char *output;
    i32 inputFormat;
    i32 firstPic;
    i32 lastPic;
    i32 width;
    i32 height;
    i32 lumWidthSrc;
    i32 lumHeightSrc;
    i32 horOffsetSrc;
    i32 verOffsetSrc;
    i32 burst;
    i32 trace;
} commandLine_s;

#ifdef TEST_DATA
/* This is defined because system model needs this. Not needed otherwise. */
char * H1EncTraceFileConfig = NULL;
int H1EncTraceFirstFrame    = 0;
#endif

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static int AllocRes(commandLine_s * cmdl, VideoStbInst inst);
static void FreeRes(VideoStbInst inst);
static int ReadPic(u8 * image, i32 nro, char *name, i32 width,
                   i32 height, i32 format);
static int Parameter(i32 argc, char **argv, commandLine_s * ep);
static void Help(char *app);
static void VSOutputStabilizedData(ptr_t image, FILE *output, i32 src_stride, i32 src_height,
                                           i32 stb_width, i32 stb_height, VideoStbResult result);


int main(int argc, char *argv[])
{
    VideoStbInst videoStab;
    VideoStbParam stabParam;
    commandLine_s cmdl;
    VideoStbRet ret;

    i32 nextPict;

    VideoStbApiVersion apiVer;
    VideoStbBuild csBuild;

    FILE *fTrc = NULL;
    FILE *fp_output = NULL;

    apiVer = VideoStbGetApiVersion();
    csBuild = VideoStbGetBuild();

    fprintf(stdout, "VideoStb API version %u.%u\n", apiVer.major, apiVer.minor);
    fprintf(stdout, "HW ID: 0x%08x\t SW Build: %u.%u.%u\n\n",
            csBuild.hwBuild, csBuild.swBuild / 1000000,
            (csBuild.swBuild / 1000) % 1000, csBuild.swBuild % 1000);

    if (argc < 2)
    {
        Help(argv[0]);
        exit(0);
    }

    /* Parse command line parameters */
    if (Parameter(argc, argv, &cmdl) != 0)
    {
        printf("Input parameter error\n");
        return VIDEOSTB_ERROR;
    }

    stabParam.format = (VideoStbInputFormat)cmdl.inputFormat;
    stabParam.inputWidth  = cmdl.lumWidthSrc;
    stabParam.inputHeight = cmdl.lumHeightSrc;
    stabParam.stabilizedWidth  = cmdl.width;
    stabParam.stabilizedHeight = cmdl.height;

    stabParam.stride = (cmdl.lumWidthSrc + 7) & (~7);   /* 8 multiple */
    stabParam.client_type = EWL_CLIENT_TYPE_VIDEOSTAB;
    if ((ret = VideoStbInit(&videoStab, &stabParam)) != VIDEOSTB_OK)
    {
        printf("VideoStbInit ERROR: %d\n", ret);
        goto end;
    }

    /* Allocate input and output buffers */
    if (AllocRes(&cmdl, videoStab) != 0)
    {
        printf("Failed to allocate the external resources!\n");
        FreeRes(videoStab);
        VideoStbRelease(videoStab);
        return VIDEOSTB_MEMORY_ERROR;
    }

    if (cmdl.trace)
    {
        fTrc = fopen("video_stab_result.log", "w");
        if (fTrc == NULL)
            printf("\nWARNING! Could not open 'video_stab_result.log'\n\n");
        else
            fprintf(fTrc, "offX, offY\n");
    }

    if (cmdl.output != NULL)
    {
        fp_output = fopen(cmdl.output, "wb");
        if (fp_output == NULL)
        {
            printf("Unable to create file %s\n", cmdl.output);
            FreeRes(videoStab);
            VideoStbRelease(videoStab);
            return VIDEOSTB_ERROR;
        }
    }

    nextPict = cmdl.firstPic;
    while (nextPict <= cmdl.lastPic)
    {
        VideoStbResult result;

        if (ReadPic((u8 *)pictureMem.virtualAddress, nextPict, cmdl.input,
                   cmdl.lumWidthSrc, cmdl.lumHeightSrc, cmdl.inputFormat) != 0)
            break;

        if (ReadPic((u8 *)nextPictureMem.virtualAddress, nextPict + 1, cmdl.input,
                   cmdl.lumWidthSrc, cmdl.lumHeightSrc, cmdl.inputFormat) != 0)
            break;

        ret = VideoStbStabilize(videoStab, &result, pictureMem.busAddress,
                                nextPictureMem.busAddress);

        if (cmdl.output != NULL)
        {
            VSOutputStabilizedData(nextPictureMem.busAddress, fp_output, stabParam.stride, stabParam.inputHeight,
                                  stabParam.stabilizedWidth, stabParam.stabilizedHeight, result);
        }

        if (ret != VIDEOSTB_OK)
        {
            printf("VideoStbStabilize ERROR: %d\n", ret);
            break;
        }

        printf("PIC %d: (%2u, %2u)\n\n", nextPict, result.stabOffsetX,
               result.stabOffsetY);

        if (cmdl.trace)
        {
            if (fTrc != NULL)
                fprintf(fTrc, "%4u, %4u\n", result.stabOffsetX,
                        result.stabOffsetY);
        }

        nextPict++;
    }

    if (fTrc != NULL)
        fclose(fTrc);

    if (yuvFile != NULL)
        fclose(yuvFile);

    if (fp_output != NULL)
        fclose(fp_output);

  end:
    FreeRes(videoStab);

    VideoStbRelease(videoStab);

    return 0;
}

/*------------------------------------------------------------------------------

    AllocRes

    OS dependent implementation for allocating the physical memories
    used by both SW and HW: the input picture.

    To access the memory HW uses the physical linear address (bus address)
    and SW uses virtual address (user address).

    In Linux the physical memories can only be allocated with sizes
    of power of two times the page size.

------------------------------------------------------------------------------*/
int AllocRes(commandLine_s * cmdl, VideoStbInst inst)
{
    u32 pictureSize;
    u32 ret;

    if (cmdl->inputFormat <= VIDEOSTB_YUV420_SEMIPLANAR)
    {
        /* Input picture in planar/semiplanar YUV 4:2:0 format */
        pictureSize = ((cmdl->lumWidthSrc + 7) & (~7)) * cmdl->lumHeightSrc * 3 / 2;
    }
    else if (cmdl->inputFormat < VIDEOSTB_RGB888)
    {
        /* Input picture in YUYV 4:2:2 format or 16-bit RGB format */
        pictureSize = ((cmdl->lumWidthSrc + 7) & (~7)) * cmdl->lumHeightSrc * 2;
    }
    else
    {
        /* Input picture in 24/30-bit RGB format (each pixel occupies 32-bit) */
        pictureSize = ((cmdl->lumWidthSrc + 7) & (~7)) * cmdl->lumHeightSrc * 4;
    }

    printf("Input size %dx%d Stabilized %dx%d\n", cmdl->lumWidthSrc,
           cmdl->lumHeightSrc, cmdl->width, cmdl->height);

    printf("Input format: %d = %s\n", cmdl->inputFormat,
           ((unsigned) cmdl->inputFormat) <
           14 ? strFormat[cmdl->inputFormat] : "Unknown");

    /* Here we use the EWL instance directly from the stabilizer instance
     * because it is the easiest way to allocate the linear memories */
    if (((VideoStb *)inst)->ewl == NULL) return 1;

    ret = EWLMallocLinear(((VideoStb *)inst)->ewl, pictureSize, 0, &pictureMem);
    if (ret != EWL_OK) {
        pictureMem.virtualAddress = NULL;
        return 1;
    }

    ret = EWLMallocLinear(((VideoStb *)inst)->ewl, pictureSize, 0, &nextPictureMem);
    if (ret != EWL_OK) {
        nextPictureMem.virtualAddress = NULL;
        return 1;
    }

    printf("Input buffer size: %d bytes\n", pictureSize);
    printf("Input buffer bus address:       0x%08lx\n", pictureMem.busAddress);
    printf("Input buffer user address:      0x%08lx\n", (ptr_t)pictureMem.virtualAddress);
    printf("Next input buffer bus address:  0x%08lx\n", nextPictureMem.busAddress);
    printf("Next input buffer user address: 0x%08lx\n", (ptr_t)nextPictureMem.virtualAddress);
    return 0;
}

/*------------------------------------------------------------------------------

    FreeRes

    Release all resources allcoated byt AllocRes()

------------------------------------------------------------------------------*/
void FreeRes(VideoStbInst inst)
{
    if (pictureMem.virtualAddress)
        EWLFreeLinear(((VideoStb *)inst)->ewl, &pictureMem);
    if (nextPictureMem.virtualAddress)
        EWLFreeLinear(((VideoStb *)inst)->ewl, &nextPictureMem);
}

/*------------------------------------------------------------------------------

    Parameter
        Process the testbench calling arguments.

    Params:
        argc    - number of arguments to the application
        argv    - argument list as provided to the application
        cml     - processed comand line options
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int Parameter(i32 argc, char **argv, commandLine_s * cml)
{
    static char *input = "input.yuv";

    i32 ret;
    char *optArg;
    argument_s argument;
    i32 status = 0;

    memset(cml, DEFAULT, sizeof(commandLine_s));

    cml->input = input;
    cml->output = NULL;
    cml->firstPic = 0;
    cml->lastPic = 100;

    cml->lumWidthSrc = 176;
    cml->lumHeightSrc = 144;
    cml->width = DEFAULT;
    cml->height = DEFAULT;
    cml->inputFormat = 0;
    cml->horOffsetSrc = 0;
    cml->verOffsetSrc = 0;

    cml->burst = 16;
    cml->trace = 0;

    argument.optCnt = 1;
    while ((ret = EncGetOption(argc, argv, option, &argument)) != -1)
    {
        if (ret == -2)
        {
            status = 1;
        }
        optArg = argument.optArg;
        switch (argument.shortOpt)
        {
        case 'i':
            cml->input = optArg;
            break;
        case 'o':
            cml->output = optArg;
            break;
        case 'a':
            cml->firstPic = atoi(optArg);
            break;
        case 'b':
            cml->lastPic = atoi(optArg);
            break;
        case 'W':
            cml->width = atoi(optArg);
            break;
        case 'H':
            cml->height = atoi(optArg);
            break;
        case 'w':
            cml->lumWidthSrc = atoi(optArg);
            break;
        case 'h':
            cml->lumHeightSrc = atoi(optArg);
            break;
        case 'N':
            cml->burst = atoi(optArg);
            break;
        case 'T':
            cml->trace = 1;
            break;
        case 'l':
            cml->inputFormat = atoi(optArg);
            break;
        case 'P':
            trigger_point = atoi(optArg);
            break;

        default:
            break;
        }
    }

    if (cml->width == DEFAULT)
        cml->width = cml->lumWidthSrc - 16;

    if (cml->height == DEFAULT)
        cml->height = cml->lumHeightSrc - 16;

    return status;
}

/*------------------------------------------------------------------------------

    ReadPic

    Read raw YUV 4:2:0 or 4:2:2 image data from file

    Params:
        image   - buffer where the image will be saved
        nro     - picture number to be read
        name    - name of the file to read
        width   - image width in pixels
        height  - image height in pixels
        format  - image format (YUV 420 planar or semiplanar, or YUV 422)

    Returns:
        0 - for success
        non-zero error code
------------------------------------------------------------------------------*/
int ReadPic(u8 * image, i32 nro, char *name, i32 width, i32 height, i32 format)
{
    static off_t file_size = 0;
    i32 src_img_size;
    i32 luma_size;
    i32 read_size;

    luma_size = width * height;

    if (format > VIDEOSTB_YUV420_SEMIPLANAR_VU)
    {
        if (format < VIDEOSTB_RGB888)  /* 16 bpp */
        {
            src_img_size = luma_size * 2;
        }
        else  /* 32 bpp */
        {
            src_img_size = luma_size * 4;  /* 24-bit or 30-bit RGB, each pixel occupies 32-bit */
        }
    }
    else  /* VIDEOSTB_YUV420_PLANAR or VIDEOSTB_YUV420_SEMIPLANAR */
    {
        src_img_size = (luma_size * 3) / 2;
    }

#ifdef VIDEOSTB_SIMULATION
    return 0;
#endif

    /* Read input from file frame by frame */
    printf("Reading frame %d (%d bytes) from %s... ", nro, src_img_size, name);

    if (yuvFile == NULL)
    {
        yuvFile = fopen(name, "rb");

        if (yuvFile == NULL)
        {
            printf("Unable to open file %s\n", name);
            return -1;
        }

        fseeko(yuvFile, (off_t)0, SEEK_END);
        file_size = ftello(yuvFile);
    }

    /* Stop if the position is over the last frame of the file. */
    if ( ((off_t)src_img_size * (off_t)(nro + 1)) > file_size )
    {
        printf("Can't read frame %d, EOF\n", nro);
        return -2;
    }

    if (fseeko(yuvFile, (off_t)src_img_size * (off_t)nro, SEEK_SET) != 0)
    {
        printf("Can't seek to frame %d\n", nro);
        return -3;
    }

    if ((width & 7) == 0)
    {
        read_size = fread(image, 1, src_img_size, yuvFile);
		if (read_size != src_img_size)
        {
            printf("ReadPic ERROR! %s, line %d\n", __FILE__, __LINE__);
            return -4;
        }
    }
    else
    {
        i32 i;
        u8 *buf = image;
        i32 stride = (width + 7) & (~7);  /* we need 8 multiple stride */

        if ((format == VIDEOSTB_YUV420_PLANAR) || (format == VIDEOSTB_YUV420_SEMIPLANAR) || (format == VIDEOSTB_YUV420_SEMIPLANAR_VU))  /* YCbCr 4:2:0 8-bit planar/semiplanar */
        {
            /* luma */
            for (i = 0; i < height; i++)
            {
                read_size = fread(buf, 1, width, yuvFile);
                if (read_size != width)
                {
                    printf("ReadPic ERROR! %s, line %d\n", __FILE__, __LINE__);
                    return -5;
                }
                buf += stride;
            }

            /* chroma */
            for (i = 0; i < height/2; i++)
            {
                read_size = fread(buf, 1, width, yuvFile);
                if (read_size != width)
                {
                    printf("ReadPic ERROR! %s, line %d\n", __FILE__, __LINE__);
                    return -6;
                }
                buf += stride;
            }
        }
        else if (format < VIDEOSTB_RGB888)  /* 16-bit per pixel */
        {
            for (i = 0; i < height; i++)
            {
                read_size = fread(buf, 1, width * 2, yuvFile);
                if (read_size != width * 2)
                {
                    printf("ReadPic ERROR! %s, line %d\n", __FILE__, __LINE__);
                    return -7;
                }
                buf += stride * 2;
            }
        }
        else  /* 32-bit per pixel */
        {
            for (i = 0; i < height; i++)
            {
                read_size = fread(buf, 1, width * 4, yuvFile);
                if (read_size != width * 4)
                {
                    printf("ReadPic ERROR! %s, line %d\n", __FILE__, __LINE__);
                    return -8;
                }
                buf += stride * 4;
            }
        }

    }

    printf("OK\n");
    return 0;
}

/*------------------------------------------------------------------------------

    Help
        Print out some instructions about usage.
------------------------------------------------------------------------------*/
void Help(char *app)
{
    fprintf(stdout, "Usage:  %s [options] -i inputfile\n\n", app);

    fprintf(stdout,
            "  -i[s] --input             Read input from file. [input.yuv]\n"
            "  -o[s] --output            Write output to file (planar YCbCr 4:2:0 8-bit only).\n"
            "  -a[n] --firstPic          First picture of input file. [0]\n"
            "  -b[n] --lastPic           Last picture of input file. [100]\n"
            "  -w[n] --lumWidthSrc       Width of source image. [176]\n"
            "  -h[n] --lumHeightSrc      Height of source image. [144]\n");
    fprintf(stdout,
            "  -W[n] --width             Width of output image. [lumWidthSrc - 16]\n"
            "  -H[n] --height            Height of output image. [lumHeightSrc - 16]\n");

    fprintf(stdout,
            "  -l[n] --inputFormat       Input YUV format. [0]\n"
            "                                0 - YUV420\n"
            "                                1 - YUV420 semiplanar CbCr (NV12)\n"
            "                                2 - YUV420 semiplanar CrCb (NV21)\n"
            "                                3 - YUYV422\n"
            "                                4 - UYVY422\n"
            "                                5 - RGB565\n"
            "                                6 - BGR565\n"
            "                                7 - RGB555\n"
            "                                8 - BGR555\n"
            "                                9 - RGB444\n"
            "                                10 - BGR444\n"
            "                                11 - RGB888\n"
            "                                12 - BGR888\n"
            "                                13 - RGB101010\n"
            "                                14 - BGR101010\n\n");

    fprintf(stdout,
            "  -T    --traceresult       Write output to file <video_stab_result.log>\n");

    fprintf(stdout,
            "\nTesting parameters that are not supported for end-user:\n"
            "  -N[n] --burstSize         0..63 HW bus burst size. [16]\n"
            "  -P[n] --trigger           Logic Analyzer trigger at picture <n>. [-1]\n"
            "\n");
    ;
}


/* Only planar YCbCr 4:2:0 8-bit is supported now. */
static void VSOutputStabilizedData(ptr_t image, FILE *output, i32 src_stride, i32 src_height,
                                           i32 stb_width, i32 stb_height, VideoStbResult result)
{
    u8 *p;
    i32 bytes_written = 0;
    i32 y = 0;
    i32 offsetX = result.stabOffsetX;
    i32 offsetY = result.stabOffsetY;
    i32 offset = offsetY * src_stride + offsetX;
//    printf("VSOutputStabilizedData: offset = %d, src_stride = %d, src_height = %d, stb_width = %d, stb_height = %d\n", offset,
//           src_stride, src_height, stb_width, stb_height);

    /* luma */
    p = (u8 *)image + offset;
    for (y=0; y<stb_height; y++)
    {
        bytes_written = fwrite(p, 1, stb_width, output);
        p += src_stride;

        if (bytes_written != stb_width)
        {
            printf("VSOutputStabilizedData: Write output YUV file ERROR! %s, line %d\n", __FILE__, __LINE__);
        }
    }

    /* Cb */
    offset = offsetY / 2 * (src_stride/2) + offsetX / 2;
    p = (u8 *)image + src_stride * src_height + offset;
    for (y=0; y<stb_height/2; y++)
    {
        bytes_written = fwrite(p, 1, stb_width/2, output);
        p += src_stride/2;

        if (bytes_written != stb_width/2)
        {
            printf("VSOutputStabilizedData: Write output YUV file ERROR! %s, line %d\n", __FILE__, __LINE__);
        }
    }

    /* Cr */
    p = (u8 *)image + src_stride * src_height + (src_stride/2) * (src_height/2) + offset;
    for (y=0; y<stb_height/2; y++)
    {
        bytes_written = fwrite(p, 1, stb_width/2, output);
        p += src_stride/2;

        if (bytes_written != stb_width/2)
        {
            printf("VSOutputStabilizedData: Write output YUV file ERROR! %s, line %d\n", __FILE__, __LINE__);
        }
    }
}

