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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "pptestbench.h"
#include "ppapi.h"
#include "tb_cfg.h"

#if defined (PP_EVALUATION)
extern u32 g_hw_ver;
#endif

struct TBCfg tb_cfg;

extern u32 force_tiled_input;

int main(int argc, char **argv) {
  int ret = 0, frame, i = 1;
  char *cfg_file = "pp.cfg";
  FILE *f_tbcfg;

#if defined(PP_EVALUATION_8170)
  g_hw_ver = 8170;
#elif defined(PP_EVALUATION_8190)
  g_hw_ver = 8190;
#elif defined(PP_EVALUATION_9170)
  g_hw_ver = 9170;
#elif defined(PP_EVALUATION_9190)
  g_hw_ver = 9190;
#elif defined(PP_EVALUATION_G1)
  g_hw_ver = 10000;
#endif

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */

  /* expiry stuff */
  {
    u8 tm_buf[7];
    time_t sys_time;
    struct tm * tm;
    u32 tmp1;

    /* Check expiry date */
    time(&sys_time);
    tm = localtime(&sys_time);
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000+atoi(tm_buf);
    if (tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1 ) {
      fprintf(stderr,
              "EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
      return -1;
    }
  }

  {
    PPApiVersion pp_api;
    PPBuild pp_build;

    /* Print API version number */
    pp_api = PPGetAPIVersion();
    pp_build = PPGetBuild();

    printf("\nX170 PostProcessor API v%d.%d - SW build: %d\n",
           pp_api.major, pp_api.minor, pp_build.sw_build);
    printf("X170 ASIC build: %x\n\n", pp_build.hw_build);

  }


  /* Parse cmdline arguments */
  if(argc == 1) {
    printf("Usage: %s [<config-file>]\n", argv[0]);
    return 1;
  }

  for (i = 1; i < (u32)(argc-1); i++) {
    if (strncmp(argv[i], "-E", 2) == 0)
      force_tiled_input = 1;
    else {
      printf("Unknown option: %s\n", argv[i]);
      return 2;
    }
  }

  cfg_file = argv[argc-1];

  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if (f_tbcfg == NULL) {
    printf("UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n");
    printf("USING DEFAULT CONFIGURATION\n");
  } else {
    fclose(f_tbcfg);
    if (TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if (TBCheckCfg(&tb_cfg) != 0)
      return -1;
  }
  /*TBPrintCfg(&tb_cfg);*/

  ret = pp_external_run(cfg_file, &tb_cfg);

  return ret;
}
