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

#include <boost/test/minimal.hpp>
#include <iostream>

#include <basecomp.h>

using namespace std;

BASECOMP* thread_base_ptr;
OMX_U32   loop_count;

OMX_ERRORTYPE test_send_command_args(OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR data, OMX_PTR arg)
{
    BOOST_REQUIRE(cmd   == OMX_CommandStateSet);
    BOOST_REQUIRE(param == 0x01);
    BOOST_REQUIRE(data  == reinterpret_cast<OMX_PTR>(0xDEADBEEF));
    BOOST_REQUIRE(arg   == reinterpret_cast<OMX_PTR>(0xFFEEFFEE));

    return OMX_ErrorNone;
}

OMX_U32 test0_thread_main(BASECOMP* self, OMX_PTR arg)
{
    BOOST_REQUIRE(self == thread_base_ptr);
    BOOST_REQUIRE(arg  == reinterpret_cast<OMX_PTR>(0xC0FFEE00));
    BOOST_REQUIRE(loop_count == 0);
    
    while (true)
    {
        CMD cmd;
        OMX_ERRORTYPE err;
        OMX_BOOL      timeout = OMX_FALSE;
        err = OMX_OSAL_EventWait(self->queue.event, INFINITE_WAIT, &timeout);

        //err = msgque_wait_for_message(&self->queue); // could multiplex here if needed
        
        BOOST_REQUIRE(err == OMX_ErrorNone);

        err = HantroOmx_basecomp_recv_command(self, &cmd);
        BOOST_REQUIRE(err == OMX_ErrorNone);
        
        if (cmd.type == CMD_EXIT_LOOP)
            break;
        
        if (cmd.type == CMD_SEND_COMMAND)
        {
            BOOST_REQUIRE(cmd.arg.fun == test_send_command_args);
        }
        

        err = HantroOmx_cmd_dispatch(&cmd, reinterpret_cast<OMX_PTR>(0xFFEEFFEE));
        BOOST_REQUIRE(err == OMX_ErrorNone);
        
        ++loop_count;
    }
    return 0;
}


void test0()
{
    BASECOMP b;
    OMX_ERRORTYPE err;
    
    loop_count      = 0;
    thread_base_ptr = &b;


    err = HantroOmx_basecomp_init(&b, test0_thread_main, reinterpret_cast<OMX_PTR>(0xC0FFEE00));
    BOOST_REQUIRE(err == OMX_ErrorNone);
    
    enum { count = 123455 };

    for (int i=0; i<count; ++i)
    {
        CMD cmd;
        INIT_SEND_CMD(cmd, OMX_CommandStateSet, 0x01, reinterpret_cast<OMX_PTR>(0xDEADBEEF), test_send_command_args);
        
        HantroOmx_basecomp_send_command(&b, &cmd);
    }
    
    CMD c;
    INIT_EXIT_CMD(c);
    
    HantroOmx_basecomp_send_command(&b, &c);

    err = HantroOmx_basecomp_destroy(&b);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    BOOST_REQUIRE(loop_count == count);
}



int test_main(int, char*  [])
{
    cout << "running " << __FILE__ << " tests\n";
    
    test0();
    return 0;
}
