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

#ifndef HANTRO_DECODER_H
#define HANTRO_DECODER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "OSAL.h"
#include "dbgmacros.h"
#include "vsi_vendor_ext.h"
#include "port.h"

/* Error handling */
#define USE_VIDEO_FREEZE_CONCEALMENT 0
#define ERROR_HANDLING DEC_EC_PICTURE_FREEZE

/* Maximum video resolutions for different HW congfigurations*/
#define MAX_VIDEO_RESOLUTION_D1    720 * 576
#define MAX_VIDEO_RESOLUTION_720P  1280 * 720
#define MAX_VIDEO_RESOLUTION_1080P 1920 * 1088
#define MAX_VIDEO_RESOLUTION_4K2K  4096 * 2160
#define MAX_VIDEO_RESOLUTION_4K4K  4096 * 4096
/* Set maximum video resolution */
#define MAX_VIDEO_RESOLUTION MAX_VIDEO_RESOLUTION_4K4K

/* Maximum image resolutions supported by different HW versions */
#define MAX_IMAGE_RESOLUTION_16MPIX   4672 * 3504
#define MAX_IMAGE_RESOLUTION_67MPIX   8176 * 8176
#define MAX_IMAGE_RESOLUTION_256MPIX  16383 * 16383
/* Set maximum image resolution */
#define MAX_IMAGE_RESOLUTION MAX_IMAGE_RESOLUTION_256MPIX

/* Default buffer sizes */
#define DEFAULT_INPUT_BUFFER_SIZE   691200
#define DEFAULT_OUTPUT_BUFFER_SIZE  1382400
#define MIN_BUFFER_SIZE             256000

/* G2 decoder specific configurations */
#define USE_RING_BUFFER         0  // Not supported

    typedef enum MPEG4_FORMAT
    {
        MPEG4FORMAT_MPEG4,
        MPEG4FORMAT_SORENSON,
        MPEG4FORMAT_CUSTOM_1,
        MPEG4FORMAT_CUSTOM_1_3,
        MPEG4FORMAT_H263
    } MPEG4_FORMAT;

    typedef struct STREAM_BUFFER
    {
        OMX_U8 *bus_data;
        OMX_U8 *buf_data;
        OSAL_BUS_WIDTH bus_address;     // use this for HW
        OSAL_BUS_WIDTH buf_address;     // use this for HW
        OMX_U32 streamlen;
        OMX_U32 sliceInfoNum;
        OMX_U8 *pSliceInfo;
        OMX_U32 allocsize;
        OMX_U32 picId;
    } STREAM_BUFFER;

    typedef struct FRAME_BUFFER_INFO
    {
        OMX_U32 bufferSize;
        OMX_U32 numberOfBuffers;
        OMX_U64 fb_bus_address;         // memory buffer address on the bus
                                        // (Only used in VP9 for dynamic port reconfiguration)
    } FRAME_BUFFER_INFO;

    typedef struct METADATA_HDR10
    {
        OMX_U32 redPrimary[2];  // Index 0 contains the X coordinate and index 1 contains the Y coordinate.
        OMX_U32 greenPrimary[2];
        OMX_U32 bluePrimary[2];
        OMX_U32 whitePoint[2];
        OMX_U32 maxMasteringLuminance;
        OMX_U32 minMasteringLuminance;
        OMX_U32 maxContentLightLevel;
        OMX_U32 maxFrameAverageLightLevel;
    } METADATA_HDR10;

    typedef struct STREAM_INFO
    {
        OMX_COLOR_FORMATTYPE format;    // stream color format
        OMX_U32 framesize;              // framesize in bytes
        OMX_U32 width;                  // picture display width
        OMX_U32 height;                 // picture display height
        OMX_U32 sliceheight;            // picture slice height
        OMX_U32 stride;                 // picture scan line width
        OMX_BOOL interlaced;            // sequence is interlaced
        OMX_U32 imageSize;              // size of image in memory
        OMX_BOOL isVc1Stream;           // sequence is VC1 stream
        OMX_BOOL crop_available;        // crop information
        OMX_U32 crop_width;
        OMX_U32 crop_height;
        OMX_U32 crop_left;
        OMX_U32 crop_top;
        OMX_U32 frame_buffers;
        OMX_U32 bit_depth;
        OMX_BOOL hdr10_available;
        METADATA_HDR10 hdr10_metadata;   // HDR10 metadata
        OMX_U32 video_full_range_flag;   // black level and range of luma chroma signals
        OMX_BOOL colour_desc_available;  // indicate colour_primaries/transfer_characteristics/matrix_coeffs present or not
        OMX_U32 colour_primaries;        // chromaticity coordinates of the source primaries
        OMX_U32 transfer_characteristics;// opto-electronic transfer function
        OMX_U32 matrix_coeffs;           // matrix coefficients used in deriving luma and chroma signals
        OMX_BOOL chroma_loc_info_available; // indicate chroma_sample_loc_type_top_field or bottom field are present or not
        OMX_U32 chroma_sample_loc_type_top_field; // specify the location of chroma samples
        OMX_U32 chroma_sample_loc_type_bottom_field; // specify the location of chroma samples
    } STREAM_INFO;

    typedef struct FRAME
    {
        OMX_U8 *fb_bus_data;            // pointer to DMA accesible output buffer.
        OSAL_BUS_WIDTH fb_bus_address;  // memory buffer address on the bus
        OMX_U32 fb_size;                // buffer size
        OMX_U32 size;                   // output frame size in bytes
        OMX_U32 MB_err_count;           // decoding macroblock error count
#ifdef ENABLE_CODEC_VP8
        OMX_BOOL isIntra;
        OMX_BOOL isGoldenOrAlternate;
#endif
        OMX_U32 viewId;                 // identifies the view in MVC stream
        OUTPUT_BUFFER_PRIVATE outBufPrivate;
    } FRAME;

    typedef enum ROTATION
    {
        ROTATE_NONE,
        ROTATE_RIGHT_90 = 90,
        ROTATE_LEFT_90 = -90,
        ROTATE_180 = 180,
        ROTATE_FLIP_VERTICAL,
        ROTATE_FLIP_HORIZONTAL
    } ROTATION;

#ifdef USE_OPENCORE
    /* Opencore specific */
    /* OMX COMPONENT CAPABILITY RELATED MEMBERS */
    typedef struct PV_OMXComponentCapabilityFlagsType
    {
        OMX_BOOL iIsOMXComponentMultiThreaded;
        OMX_BOOL iOMXComponentSupportsExternalOutputBufferAlloc;
        OMX_BOOL iOMXComponentSupportsExternalInputBufferAlloc;
        OMX_BOOL iOMXComponentSupportsMovableInputBuffers;
        OMX_BOOL iOMXComponentSupportsPartialFrames;
        OMX_BOOL iOMXComponentUsesNALStartCode;
        OMX_BOOL iOMXComponentCanHandleIncompleteFrames;
        OMX_BOOL iOMXComponentUsesFullAVCFrames;
    } PV_OMXComponentCapabilityFlagsType;
#endif


/* Stride alignment: aligned to 8/16/.../512 bytes */
    typedef enum DEC_ALIGN
    {
        ALIGN_1B = 0,
        ALIGN_8B = 3,
        ALIGN_16B,
        ALIGN_32B,
        ALIGN_64B,
        ALIGN_128B,
        ALIGN_256B,
        ALIGN_512B,
        ALIGN_1024B,
        ALIGN_2048B,
    } DEC_ALIGN;

//
// Post processor configuration arguments. Use 0 for default or "don't care".
//
    typedef struct PP_UNIT_CFG
    {
        OMX_U32 enabled;    /* PP unit enabled */
        OMX_U32 tiled_e;    /* PP unit tiled4x4 output enabled */
        OMX_U32 cr_first;   /* CrCb instead of CbCr */
        OMX_U32 planar;     /* Planar output */
        DEC_ALIGN align;  /* pp output alignment */
        OMX_U32 ystride;
        OMX_U32 cstride;
        struct scale
        {
            OMX_U32 enabled;    /* whether scaling is enabled */
            OMX_S32 width;
            OMX_S32 height;
        } scale;

        // Input video cropping.
        struct crop
        {
            OMX_U32 enabled;  /* whether cropping is enabled */
            OMX_U32 left;    // cropping start X offset.
            OMX_U32 top;     // cropping start Y offset.
            OMX_U32 width;
            OMX_U32 height;
        } crop;

        OMX_U32 monochrome; /* PP output monochrome (luma only) */
        OMX_U32 out_p010;
        OMX_U32 out_1010;
        OMX_U32 out_I010;
        OMX_U32 out_L010;
        OMX_U32 out_be;
        OMX_U32 out_cut_8bits;
        OMX_U32 out_format;
        // Specificy output color format.
        // Allowed values are:
        // OMX_COLOR_FormatYUV420PackedPlanar
        // OMX_COLOR_FormatYCbYCr
        // OMX_COLOR_Format32bitARGB8888
        // OMX_COLOR_Format32bitBGRA8888
        // OMX_COLOR_Format16bitARGB1555
        // OMX_COLOR_Format16bitARGB4444
        // OMX_COLOR_Format16bitRGB565
        // OMX_COLOR_Format16bitBGR565
        OMX_COLOR_FORMATTYPE format;
    } PP_UNIT_CFG;

    typedef enum CODEC_STATE
    {
        CODEC_NEED_MORE,
        CODEC_HAS_FRAME,
        CODEC_HAS_INFO,
        CODEC_OK,
        CODEC_PIC_SKIPPED,
        CODEC_END_OF_STREAM,
        CODEC_WAITING_FRAME_BUFFER,
        CODEC_ABORTED,
        CODEC_FLUSHED,
        CODEC_BUFFER_EMPTY,
        CODEC_PENDING_FLUSH,
        CODEC_NO_DECODING_BUFFER,
        CODEC_ERROR_HW_TIMEOUT = -1,
        CODEC_ERROR_HW_BUS_ERROR = -2,
        CODEC_ERROR_SYS = -3,
        CODEC_ERROR_DWL = -4,
        CODEC_ERROR_UNSPECIFIED = -5,
        CODEC_ERROR_STREAM = -6,
        CODEC_ERROR_INVALID_ARGUMENT = -7,
        CODEC_ERROR_NOT_INITIALIZED = -8,
        CODEC_ERROR_INITFAIL = -9,
        CODEC_ERROR_HW_RESERVED = -10,
        CODEC_ERROR_MEMFAIL = -11,
        CODEC_ERROR_STREAM_NOT_SUPPORTED = -12,
        CODEC_ERROR_FORMAT_NOT_SUPPORTED = -13,
        CODEC_ERROR_NOT_ENOUGH_FRAME_BUFFERS = -14,
        CODEC_ERROR_BUFFER_SIZE = -15
    } CODEC_STATE;

    typedef struct CODEC_PROTOTYPE CODEC_PROTOTYPE;

// internal CODEC interface, which wraps up Hantro API
    struct CODEC_PROTOTYPE
    {
        //
        // Destroy the codec instance
        //
        void (*destroy) (CODEC_PROTOTYPE *);
        //
        // Decode n bytes of data given in the stream buffer object.
        // On return consumed should indicate how many bytes were consumed from the buffer.
        //
        // The function should return one of following:
        //
        //    CODEC_NEED_MORE  - nothing happened, codec needs more data.
        //    CODEC_HAS_INFO   - headers were parsed and information about stream is ready.
        //    CODEC_HAS_FRAME  - codec has one or more headers ready
        //    less than zero   - one of the enumerated error values
        //
        // Parameters:
        //
        //    CODEC_PROTOTYPE  - this codec instance
        //    STREAM_BUFFER    - data to be decoded
        //    OMX_U32          - pointer to an integer that should on return indicate how many bytes were used from the input buffer
        //    FRAME            - where to store any frames that are ready immediately
         CODEC_STATE(*decode) (CODEC_PROTOTYPE *, STREAM_BUFFER *, OMX_U32 *,
                               FRAME *);

        //
        // Get info about the current stream. On return stream information is stored in STREAM_INFO object.
        //
        // The function should return one of the following:
        //
        //   CODEC_OK         - got information succesfully
        //   less than zero   - one of the enumerated error values
        //
         CODEC_STATE(*getinfo) (CODEC_PROTOTYPE *, STREAM_INFO *);

        //
        // Get a frame from the decoder. On return the FRAME object contains the frame data. If codec does internal
        // buffering this means that codec needs to copy the data into the specied buffer. Otherwise it might be
        // possible for a codec implementation to store the frame directly into the frame's buffer.
        //
        // The function should return one of the following:
        //
        //  CODEC_OK         - everything OK but no frame was available.
        //  CODEC_HAS_FRAME  - got frame ok.
        //  less than zero   - one of the enumerated error values
        //
        // Parameters:
        //
        //  CODEC_PROTOTYPE  - this codec instance
        //  FRAME            - location where frame data is to be written
        //  OMX_BOOL         - end of stream (EOS) flag
        //
         CODEC_STATE(*getframe) (CODEC_PROTOTYPE *, FRAME *, OMX_BOOL);

        //
        // Scan for complete frames in the stream buffer. First should be made to
        // give an offset to the first frame within the buffer starting from the start
        // of the buffer. Respectively last should be made to give an offset to the
        // start of the last frame in the buffer.
        //
        // Note that this does not tell whether the last frame is complete or not. Just that
        // complete frames possibly exists between first and last offsets.
        //
        // The function should return one of the following.
        //
        //   -1             - no frames found. Value of first and last undefined.
        //    1             - frames were found.
        //
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //  STREAM_BUFFER   - frame data pointer
        //  OMX_U32         - first offset pointer
        //  OMX_U32         - last offset pointer
        //
        OMX_S32 (*scanframe) (CODEC_PROTOTYPE *, STREAM_BUFFER *, OMX_U32 * first,
                          OMX_U32 * last);

        //
        // Set decoder configuration including PP parameters.
        //
        // The function should return one of the following:
        // CODEC_OK         - parameters were set succesfully.
        // CODEC_ERROR_INVALID_ARGUMENT - an invalid post-processor argument was given.
        //
        CODEC_STATE (*setinfo)(CODEC_PROTOTYPE *, OMX_VIDEO_PARAM_CONFIGTYPE *, PP_UNIT_CFG *);

        // 
        void *appData;

        // Send end of stream
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //
        CODEC_STATE(*endofstream) (CODEC_PROTOTYPE *);

        // Send picture consumed to decoder
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //  BUFFER          - pointer to output frame
        //
        CODEC_STATE(*pictureconsumed) (CODEC_PROTOTYPE *, BUFFER *);

        // Set decoder's frame buffer
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //  BUFFER          - pointer to output frame
        //  OMX_U32         - number of available buffers
        //
        CODEC_STATE(*setframebuffer) (CODEC_PROTOTYPE *, BUFFER *, OMX_U32);

        // Get frame buffer info from decoder
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //
        FRAME_BUFFER_INFO(*getframebufferinfo) (CODEC_PROTOTYPE *);

        // abort the decoder
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //
        CODEC_STATE(*abort) (CODEC_PROTOTYPE *);

        // clear internal parameters in decoder after abort.
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        //
        CODEC_STATE(*abortafter) (CODEC_PROTOTYPE *);

        // set decoder to no reorder mode
        // Parameters:
        //
        //  CODEC_PROTOTYPE - this codec instance
        // OMX_BOOL          - enable no_reoder mode or not.

        CODEC_STATE(*setnoreorder) (CODEC_PROTOTYPE *, OMX_BOOL);
    };

#ifdef __cplusplus
}
#endif
#endif                       // HANTRO_DECODER_H
