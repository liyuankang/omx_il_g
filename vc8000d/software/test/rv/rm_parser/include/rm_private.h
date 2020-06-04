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
------------------------------------------------------------------------------*/

#ifndef _RM_PRIVATE_H_
#define _RM_PRIVATE_H_

#include "helix_types.h"
//#include "rm_memory.h"
//#include "rm_parse.h"
#include "rm_stream.h"
#include "rm_packet.h"

#include "ra_decode.h"
#include "ra_depack.h"
#include "ra_format_info.h"
#include "rv_decode.h"
#include "rv_depack.h"
#include "rv_format_info.h"

#include "rm_types.h"

/*=============== file descriptor passed to all routines ===============*/
typedef struct{
	UINT32		buffer_address;		/* buffer pointer */
   	UINT32 		buffer_size;			/* buffer size*/
	UINT32		data_length;			/* bytes in buffer */
	UINT32		cursor_read;		/* read cursor in buffer */
	UINT32		cursor_write;		/* write cursor in buffer */
	UINT32		frame_count;		/* frame count */
	UINT32		frame_size;			/* frame size */
	UINT32		frame_duration;		/* frame duration */
	UINT32		isnot_empty;		/* flag to identify buffer status, 0: empty, others: not empty */
	UINT32		is_over;				/* flag to identify buffer data finsh, 0: is not last block data, 1: is last data block */
}audio_buffer_t;

typedef struct {
	UINT32		buffer_address;		/* buffer pointer */
   	UINT32 		buffer_size;			/* buffer size*/
	UINT32		data_length;			/* bytes in buffer */
	UINT32		cursor_read;		/* read cursor in buffer */
	UINT32		cursor_write;		/* write cursor in buffer */
	UINT32		frame_count;		/* frame count */
	UINT32		frame_size[30];		/* frame size */
	UINT32		frame_synflag[30];	/* I frame flag */
	UINT32		frame_duration[30];	/* frame duration */
	UINT32		isnot_empty;		/* flag to identify buffer status, 0: empty, others: not empty */
	UINT32		is_over;				/* flag to identify buffer data finsh, 0: is not last block data, 1: is last data block */
}video_buffer_t;

typedef struct
{
	void*		stream;

	INT32		(*read_func)(void *fs_filehandle, char* buffer, INT32 readbytes);
	INT32		(*write_func)(void *fs_filehandle, char* buffer, INT32 writebytes);
	INT32		(*seek_func)(void *fs_filehandle, INT32 offset, INT32 mode);
	INT32		(*tell_func)(void *fs_filehandle);

	UINT32 		offset;				/* stream offset */
	UINT32		ulNumStreams;		/* stream number in the file */
	UINT32		ulFirstDataOffset;	/* zhang xuecheng , 2008-1-17 15:39:11 */

	rm_parser*			pParser;
	rm_packet*			pPacket;
	rm_stream_header*  	pHdr;

	ra_depack*			pRADepack;
	ra_format_info*    	pRAInfo;
	ra_decode*			pRADecode;

	rv_depack*			pRVDepack;
	rv_format_info*    	pRVInfo;
	rv_decode*			pRVDecode;
	rv_frame*			pRVFrame;

	UINT32				v_isrv8;
	rv8_info_ex*		v_rv8InfoEx;
	//UINT32			v_rv8pctszSize;
	//UINT32			v_rv8MpSize;

	UINT32		a_offset;
	UINT32		v_offset;
	UINT32		a_cur;
	UINT32		v_cur;

	/* The following two used to track rv frame info while depack -caijin @2008.05.28*/
	UINT32		v_frame_duration;	/* duration of current frame (ms) */
	UINT32		v_frame_synflag;	/* frame synflag: 1 = keyframe, 0 = nonkeyframe */

	UINT32		a_isvalid;			/* 1: have audio, 0 no audio */
	UINT32		a_bits;				/* bit length */
	UINT32		a_chn;				/* 1: mono, 2: stereo */
	UINT32		a_smplrate;			/* sample rate */
	UINT32		a_smpllen;			/* byte number of one audio sample */
	UINT32		a_frmsize;			/* frame size(if changeless frame size, else = 0) */
	UINT32		a_duration;			/* frame duration(if changeless frame rate or first frame duration) */
	UINT32		a_totaltime;			/* audio total play time (ms)*/
	UINT32		a_totalframe;		/* audio total frame*/
	UINT32		a_bitrate;
	char		a_codec[4];			/* string "ra" */
	UINT32		a_ulCodec4CC;

	UINT32		v_isvalid;			/* video is valid */
	UINT32		v_width;			/* video width */
	UINT32		v_height;			/* video height */
	UINT32		v_fps;				/* frame number per second(if changeless frame size, else =0) */
	UINT32		v_tickps;			/* tick number per second (timescale)*/
	UINT32		v_duration;			/* frame duration(if changeless frame rate or first frame duration) */
	UINT32		v_totaltime;		/* video total play time (ms)*/
	UINT32		v_totalframe;		/* video total frame*/
	char		v_codec[4];			/* string "rv" */
	UINT32		v_ulCodec4CC;		/* 
									 * 0x52563130 (¡°RV10¡±): RealVideo 1
									 * 0x52563230 (¡°RV20¡±): RealVideo G2
									 * 0x52563330 (¡°RV30¡±): RealVideo 8
									 * 0x52563430 (¡°RV40¡±): RealVideo 9 and 10
									 */
	
	UINT32		a_usStreamNum;		/* stream number of real audio */
	UINT32		v_usStreamNum;		/* stream number of real video */
} rm_infor_t;
#endif
