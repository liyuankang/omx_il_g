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
#--  Abstract : Makefile for encoder kernel driver
#--
#-------------------------------------------------------------------------------
SUPPORT_VCMD = y

ifeq ($(obj),)
obj = .
endif

#################################################
# configuration

MDIR		:= hantro

# drivers objects
# list-multi	:= hantro_mmu.o

# # what to build

ifeq ($(SUPPORT_VCMD),y)
 vc8000_vcmd-objs :=  vc8000_vcmd_driver.o bidirect_list.o vcmdswhwregisters.o
endif
ifeq ($(SUPPORT_VCMD),y)
obj-m         += vc8000_vcmd.o
endif

tardest		:= .

#################################################
# compile modules

ifneq ($(KERNELRELEASE),)
# recursive call from kernel build system
dummy		:= $(shell echo $(KERNELRELEASE) > $(obj)/.version)

ifeq ($(VERSION).$(PATCHLEVEL),2.6)
 export-objs	:=
 list-multi	:=
else
 multi-m	:= $(filter $(list-multi), $(obj-m))
 int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))
 export-objs	:= $(filter $(int-m) $(obj-m),$(export-objs))
endif

CC += -I$(obj)
EXTRA_CFLAGS += -g

# Print debugging messages from the device 
#EXTRA_CFLAGS   += -DHANTROMMU_DEBUG

snapshot := $(wildcard $(obj)/.snapshot)
ifneq ($(snapshot),)
SNAPSHOT_CFLAGS	:= -DSNAPSHOT='$(shell cat $(snapshot))'
EXTRA_CFLAGS	+= $(SNAPSHOT_CFLAGS)
endif

ifeq ($(SUPPORT_MMU),y)
EXTRA_CFLAGS   += -DHANTROMMU_SUPPORT
endif

#EXTRA_CFLAGS += -DHANTRO_VCMD_DRIVER_DEBUG

-include $(TOPDIR)/Rules.make
else
# take version info from last module build if available
KERNELRELEASE	:= $(shell cat $(obj)/.version 2>/dev/null || uname -r)
endif
KDIR_BASE := /afs/hantro.com/projects/Testing/Board_Version_Control

KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build

PWD		:= $(shell pwd)
DEST		:= /lib/modules/$(KERNELRELEASE)/$(MDIR)

# which files to install?
inst-m		:= $(wildcard *.ko)
ifeq ($(inst-m),)
  inst-m	:= $(obj-m)
endif

# locales seem to cause trouble sometimes.
LC_ALL = POSIX
export LC_ALL

default::
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install::
	strip --strip-debug $(inst-m)
	-su -c "mkdir -p $(DEST); cp -v $(inst-m) $(DEST); depmod -a"

clean::
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	-rm -f .version
        
#################################################
# build tarballs

thisdir	:= $(notdir $(PWD))
name	:= $(shell echo $(thisdir) | sed 's/-.*//')
ver	:= $(shell echo $(thisdir) | sed 's/.*-//')
date    := $(shell date +%Y%m%d)
tardest	?= .

snapdir := $(HOME)/snapshot
snap	?= $(name)

release: clean
	rm -f .snapshot
	(cd ..; tar cvzf $(tardest)/$(name)-$(ver).tar.gz $(thisdir))

snapshot snap tarball: clean
	echo $(date) > .snapshot
	(cd ..; tar czf $(snapdir)/$(snap)-$(date).tar.gz $(thisdir))
	$(MAKE) -C $(snapdir)

#################################################
# other stuff

%.asm: %.o
	objdump -S $< > $@

