/*------------------------------------------------------------------------------
-- Copyright (c) 2019, VeriSilicon Inc. or its affiliates. All rights reserved--
--                                                                            --
-- Permission is hereby granted, free of charge, to any person obtaining a    --
-- copy of this software and associated documentation files (the "Software"), --
-- to deal in the Software without restriction, including without limitation  --
-- the rights to use copy, modify, merge, publish, distribute, sublicense,    --
-- and/or sell copies of the Software, and to permit persons to whom the      --
-- Software is furnished to do so, subject to the following conditions:       --
--                                                                            --
-- The above copyright notice and this permission notice shall be included in --
-- all copies or substantial portions of the Software.                        --
--                                                                            --
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR --
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   --
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE--
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER     --
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    --
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        --
-- DEALINGS IN THE SOFTWARE.                                                  --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include "user_freertos.h"
#include "memalloc_freertos.h"
#ifndef SUPPORT_VCMD
#include "hantrodec_freertos.h"
#else
#include "vc8000_vcmd_freertos_driver.h"
#endif

#ifdef SUPPORT_VCMD
pthread_mutex_t vcmd_dev_open_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static u32 vcmd_dev_open_count = 0;
#endif

//all drivers's probe should be added and be called by main() explicitly
int Platform_init() {
  //First init the mem for IO device
  if (memalloc_init() != 0) {
    PDEBUG("memalloc_init error\n");
    assert(0);
  }
#ifndef SUPPORT_VCMD
  if (hantrodec_init() != 0) {
    PDEBUG("hantroenc_init error\n");
    assert(0);
  }
#else
  if (hantrovcmd_init() != 0) {
    PDEBUG("hantroenc_vcmd_init error\n");
    assert(0);
  }
#endif
  return 0;
}
int freertos_open(const char* dev_name, int flag) {
  //do a hash operator to gain the only dev fd
  //int fd = hash(name);
  int fd = 0; //the default is error id
  if(!strcmp(dev_name, DEC_MODULE_PATH)) {
#ifndef SUPPORT_VCMD    
    fd = DEC_FD << 28;
    hantrodec_open(NULL, fd);
#endif   
  }
  else if(!strcmp(dev_name, VCMD_MODULE_PATH)) {
    fd = DEC_FD << 28;
    //u32, MSB 4bit for device type, LSB 28bit for reference count
    pthread_mutex_lock(&vcmd_dev_open_count_mutex);
    vcmd_dev_open_count++;
    fd += vcmd_dev_open_count;
    pthread_mutex_unlock(&vcmd_dev_open_count_mutex);
    hantrovcmd_open(NULL, fd);
  }
  else if(!strcmp(dev_name, MEMALLOC_MODULE_PATH)) {
    fd = MEM_FD << 28;
    memalloc_open(NULL, fd);
  }
  return fd;
}

void freertos_close(int fd) {
  //u32, MSB 4bit for device type
  u32 fd_tmp = fd;
  fd_tmp = fd_tmp >> 28;

  switch(fd_tmp) {
    case DEC_FD: {
#ifndef SUPPORT_VCMD
      hantrodec_release(NULL, fd);
#else
      //u32, MSB 4bit for device type, LSB 28bit for reference count
      hantrovcmd_release(NULL, fd);
      pthread_mutex_lock(&vcmd_dev_open_count_mutex);
      vcmd_dev_open_count--;
      pthread_mutex_unlock(&vcmd_dev_open_count_mutex);
#endif
      break;
    }
    case MEM_FD: {
      //so if low in special paltform, could comment it when test, but in normal, it will be opened
      memalloc_release(NULL, fd);
      break;
    }
    case 0: //for /dev/mem, it it not used, so shouldn't go to default
    	break;
    default: {
      PDEBUG("freertos_release fd 0x%x error\n", fd);
      break;
    }
  }
}

long freertos_ioctl(int fd, unsigned int cmd, unsigned long arg) {
  long ret = -1;
  //u32, MSB 4bit for device type
  u32 fd_tmp = fd;
  fd_tmp = fd_tmp >> 28;
  switch(fd_tmp) {
    case DEC_FD: {
#ifndef SUPPORT_VCMD
      ret = hantrodec_ioctl(fd, cmd, arg);
#else
      ret = hantrovcmd_ioctl(fd, cmd, arg);
#endif
      break;
    }
    case MEM_FD: {
      ret = memalloc_ioctl(fd, cmd, arg);
      break;
    }
    default: {
      PDEBUG("freertos_ioctl fd 0x%x error\n", fd);
      break;
    }
  }
  
  return ret;
}

