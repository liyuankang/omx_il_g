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
------------------------------------------------------------------------------*/

#include "encswhwregisters.h"
#include <stdio.h>
#include <stdlib.h>

const regField_s asicRegisterDesc[] =
{
#include "registertable.h"
};



void show_register(unsigned int offset, unsigned int value, char type)
{
    unsigned int lsb, msb, mask;
    const regField_s *field;
    int i;
    
    for (i = 0; i < HEncRegisterAmount; i++)
    {
        if (i>=HWIF_ENC_CUTREE_MODE) 
          break;
        
        field = &asicRegisterDesc[i];
        if (offset != field->base)
            continue;
            
        mask = (field->mask >> field->lsb);
        msb = field->lsb;
        lsb = field->lsb;

        while (mask != 0)
        {
            mask = mask >> 1;
            msb++;
        }
        msb--;

        printf("%c swreg%3d; 0x%03x; %2d; %2d; %s; 0x%08x; %s\n", type,
               field->base / 4, field->base, msb, lsb,
               (field->rw == RO) ? "RO" : (field->rw == WO) ? "WO" : "RW", (value&field->mask)>>(lsb), field->description);

    }

}

int main(int argc, char *argv[])
{
    int i, j;
    FILE *fd;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char type;
    unsigned int offset, value;
    int ret;

    if (argc<2)
    {
        printf("Usage: %s input.txt\n", argv[0]);
        exit(-1);
    }

    fd = fopen(argv[1], "rt");
    if (fd==NULL)
    {
        printf("Cannot open file %s\n", argv[1]);
        exit(0);
    }

    while ((read = getline(&line, &len, fd))!=-1) {
        if (((line[0]=='W')||(line[0]=='R'))&&(line[1]==' '))
        {
            ret = sscanf(line, "%c %8x/%8x\n", &type, &offset, &value);
            if (ret != 3)
                break;
            offset = offset & 0xfff;
            show_register(offset, value, type);
        }
        if (((line[0]=='[')||(line[11]==']'))&&(line[12]==':'))
        {
            ret = sscanf(line, "[0x%8x]: 0x%8x\n", &offset, &value);
            if (ret != 2)
                break;
            offset = offset & 0xfff;
            show_register(offset, value, 'N');
        }
        if ((line[0]=='r')&&(line[1]=='e')&&(line[2]=='g')&&(line[3]=='['))
        {
          /* reg[2] = 0x802 */
          ret = sscanf(line, "reg[%d] = 0x%x\n", &offset, &value);
          if (ret != 2)
              break;
          offset = offset & 0xfff;
          offset -= 1;
          show_register(offset*4, value, 'N');
        }
    }

    return 0;
}
