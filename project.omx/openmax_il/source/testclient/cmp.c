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

#include <stdio.h>

#define CMP_BUFF_SIZE 10*1024

int compare_output(const char *temp, const char *reference)
{
    printf("comparing files\n");
    printf("temporary file:%s\nreference file:%s\n", temp, reference);

    char *buff_tmp = NULL;

    char *buff_ref = NULL;

    FILE *file_tmp = fopen(temp, "rb");

    FILE *file_ref = fopen(reference, "rb");

    if(!file_tmp)
    {
        printf("failed to open temp file\n");
        goto FAIL;
    }
    if(!file_ref)
    {
        printf("failed to open reference file\n");
        goto FAIL;
    }
    fseek(file_tmp, 0, SEEK_END);
    fseek(file_ref, 0, SEEK_END);
    int tmp_size = ftell(file_tmp);

    int ref_size = ftell(file_ref);

    int min_size = tmp_size;

    if(tmp_size != ref_size)
    {
        min_size = tmp_size < ref_size ? tmp_size : ref_size;
        printf("file sizes do not match: temp: %d reference: %d bytes\n",
               tmp_size, ref_size);
        printf("comparing first %d bytes\n", min_size);
    }
    fseek(file_tmp, 0, SEEK_SET);
    fseek(file_ref, 0, SEEK_SET);

    buff_tmp = (char *) malloc(CMP_BUFF_SIZE);
    buff_ref = (char *) malloc(CMP_BUFF_SIZE);

    int pos = 0;

    int min = 0;

    int rem = 0;

    int success = 1;

    while(pos < min_size)
    {
        rem = min_size - pos;
        min = rem < CMP_BUFF_SIZE ? rem : CMP_BUFF_SIZE;
        fread(buff_tmp, min, 1, file_tmp);
        fread(buff_ref, min, 1, file_ref);

        if(memcmp(buff_tmp, buff_ref, min))
        {
            success = 0;
        }
        pos += min;
    }
    printf("file cmp done, %s\n", success ? "SUCCESS!" : "FAIL");
    return !success;
  FAIL:
    if(file_tmp)
        fclose(file_tmp);
    if(file_ref)
        fclose(file_ref);
    if(buff_tmp)
        free(buff_tmp);
    if(buff_ref)
        free(buff_ref);
    return 1;
}

int main(int argc, const char *argv[])
{
    int ret = compare_output(argv[1], argv[2]);

    printf("returning: %d\n", ret);
    return ret;
    //return compare_output(argv[1], argv[2]);
}
