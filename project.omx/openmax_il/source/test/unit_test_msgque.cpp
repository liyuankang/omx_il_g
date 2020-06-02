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
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <iostream>
#include <vector>

#include <msgque.h>
#include <OSAL.h>

using namespace boost;
using namespace std;

struct amsg 
{
  int val1;
  int val2;
};


void test0_thread_main(msgque* queue, int msgs, int* ret)
{
    for (int i=0; i<msgs; ++i)
    {
        amsg* m = NULL;
        
        OMX_BOOL timeout  = OMX_FALSE;
        OMX_ERRORTYPE err = OMX_OSAL_EventWait(queue->event, INFINITE_WAIT, &timeout);

        BOOST_REQUIRE(err == OMX_ErrorNone);

        err = HantroOmx_msgque_get_front(queue, (void**)&m);
        BOOST_REQUIRE(err == OMX_ErrorNone);
        *ret += m->val1 + m->val2;
    }

}



void test0()
{
    msgque q;
    HantroOmx_msgque_init(&q);
    
    BOOST_REQUIRE(q.size == 0);
    BOOST_REQUIRE(q.head == NULL);
    BOOST_REQUIRE(q.tail == NULL);

    OMX_U32 size = 0;
    HantroOmx_msgque_get_size(&q, &size);
    BOOST_REQUIRE(size == 0);
  
    enum { TEST_MSGS = 1000000 };

    vector<amsg> vec;
    vec.resize(TEST_MSGS);
  
    // stick all these messages into the queue
    for (unsigned i=0; i<vec.size(); ++i)
    {
        amsg* m = &vec[i];
        m->val1 = 0xDEADBEEF;
        m->val2 = i;
        HantroOmx_msgque_push_back(&q, m);
    }

    HantroOmx_msgque_get_size(&q, &size);
    BOOST_REQUIRE(size == TEST_MSGS);

    
    for (unsigned i=0; i<vec.size(); ++i)
    {
        amsg* m = &vec[i];
        void* t = NULL;
        
        OMX_ERRORTYPE err = HantroOmx_msgque_get_front(&q, &t);
        BOOST_REQUIRE(err == OMX_ErrorNone);
        BOOST_REQUIRE(t == m);
    }
    
    HantroOmx_msgque_get_size(&q, &size);
    BOOST_REQUIRE(size == 0);
    
    HantroOmx_msgque_destroy(&q);
}

//
// Test parallel access to the queue. Verify that queue and its data is not corrupted.
//
void test1()
{
    msgque q;
    HantroOmx_msgque_init(&q);

    enum { TEST_MSGS = 100000 };

    int ret = 0;
    int sum = 0;

    // consumer thread
    thread t(bind(test0_thread_main, &q, (int)TEST_MSGS, &ret));

    vector<amsg> vec;
    vec.resize(TEST_MSGS);

    for (int i=0; i<TEST_MSGS; ++i)
    {
        amsg* m  = &vec[i];
        m->val1  = i;
        m->val2  = i * 2;
        sum +=   m->val1 + m->val2;
        
        HantroOmx_msgque_push_back(&q, m);
    }

    t.join();

    BOOST_REQUIRE(sum == ret);

}


int test_main(int, char* [])
{

    cout << "running " << __FILE__ " tests\n";
    
    test0();
    test1();

    return 0;
}
