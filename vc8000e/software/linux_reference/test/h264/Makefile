#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Verisilicon.					                          --
#-                                                                            --
#-                   (C) COPYRIGHT 2014 VERISILICON.                          --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------
#-
#--  Abstract : Makefile for encoder testbench
#--
#-------------------------------------------------------------------------------

include ../hevc/Makefile

ifneq ($(target),)
  ifneq ($(target),"default")
    CFLAGS += -DTARGET -D$(target)
  endif
endif

INCFLAGS += -I../hevc
CFLAGS += -DSUPPORT_H264
vpath %.c ../hevc
TARGET = h264_testenc
eval: TARGET = h264_enc_h2_eval
system_multifile: TARGET = h264_testenc_multifile
versatile_multifile: TARGET = h264_testenc_multifile
