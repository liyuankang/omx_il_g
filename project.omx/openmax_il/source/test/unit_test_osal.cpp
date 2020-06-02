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
#include <boost/thread/condition.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>
#include <OSAL.h>


using namespace std;
using namespace boost;


void test0()
{
    OMX_HANDLETYPE event1 = NULL;
    OMX_HANDLETYPE event2 = NULL;
    OMX_ERRORTYPE     err = OMX_ErrorNone;
    
    err = OMX_OSAL_EventCreate(&event1);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    BOOST_REQUIRE(event1 != NULL);

    err = OMX_OSAL_EventCreate(&event2);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    BOOST_REQUIRE(event2 != NULL);


    enum { MESSAGES = 1032324 };

    int count1 = 0;
    int count2 = 0;
    int result1 = 0;
    int result2 = 0;

    for (int i=0; i<MESSAGES; ++i)
    {
        OMX_OSAL_EventReset(event1);
        OMX_OSAL_EventReset(event2);
        
        bool signal_set = false;

        if ((i%4) == 0)
        {
            ++count1;
            signal_set = true;
            BOOST_REQUIRE(OMX_OSAL_EventSet(event1) == OMX_ErrorNone);
        }
        if ((i%3) == 0)
        {
            ++count2;
            signal_set = true;
            BOOST_REQUIRE(OMX_OSAL_EventSet(event2) == OMX_ErrorNone);
        }
        
        if (signal_set)
        {
            OMX_HANDLETYPE handles[2] = {event1, event2};
            OMX_BOOL       signals[2] = {OMX_FALSE, OMX_FALSE};
            OMX_BOOL       timeout    = OMX_FALSE;
            err = OMX_OSAL_EventWaitMultiple(
                handles,
                signals,
                2,
                INFINITE_WAIT,
                &timeout);
            BOOST_REQUIRE(err == OMX_ErrorNone);
              
            if (signals[0] == OMX_TRUE)
                ++result1;
            if (signals[1] == OMX_TRUE)
                ++result2;
        }
    }
    
    BOOST_REQUIRE(count1 == result1);
    BOOST_REQUIRE(count2 == result2);
    
    OMX_OSAL_EventDestroy(event1);
    OMX_OSAL_EventDestroy(event2);
}

class test_signal : boost::noncopyable
{
public:
    test_signal() : e_(NULL), i_(0)
    {
        OMX_ERRORTYPE err = OMX_OSAL_EventCreate(&e_);
        BOOST_REQUIRE(err == OMX_ErrorNone);
    }
   ~test_signal()
    {
        OMX_ERRORTYPE err = OMX_OSAL_EventDestroy(e_);
        BOOST_REQUIRE(err == OMX_ErrorNone);
    }
    void set()
    {
        boost::mutex::scoped_lock l(m_);
        
        ++i_;
        
        c_.notify_one();

        BOOST_REQUIRE(OMX_OSAL_EventSet(e_) == OMX_ErrorNone);
    }
    
    void wait()
    {
        boost::mutex::scoped_lock l(m_);
        
        if (i_==0)
            c_.wait(l);
        
        assert(i_ > 0);

        OMX_BOOL timeout = OMX_FALSE;
        OMX_BOOL signals = OMX_FALSE;
        OMX_ERRORTYPE err = OMX_OSAL_EventWaitMultiple(
            &e_,
            &signals,
            1,
            INFINITE_WAIT,
            &timeout);
        BOOST_REQUIRE(err == OMX_ErrorNone);
        BOOST_REQUIRE(timeout == OMX_FALSE);
        
        if (--i_ == 0)
            OMX_OSAL_EventReset(e_);
    }
private:
    boost::mutex     m_;
    boost::condition c_;
    
    OMX_HANDLETYPE   e_;
    int              i_;
};





void test1_thread_main(volatile int* result, test_signal* s, volatile bool* run)
{
    while (*run)
    {
        s->wait();

        ++(*result);
    }
}


void test1()
{
    
    test_signal s;

    int count  = 0;
    volatile int result = 0;
    volatile bool run   = true;
    
    thread t(boost::bind(test1_thread_main, &result, &s, &run));
    
    const int max = 1234;

    for (int i=0; i<max; ++i)
    {
        if ((i % 5) == 0)
        {
            s.set();
            ++count;
            

        }
        cout << "\r" << i << "/" <<  max;
        
        usleep(10);
    }
    
    run = false;
    ++count;
    s.set();

    t.join();
    
    BOOST_REQUIRE(count == result);
}

void test2()
{
    OSAL_ALLOCATOR alloc = {};

    OMX_ERRORTYPE err = OMX_OSAL_AllocatorInit(&alloc);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    
    OMX_U8* data           = NULL;
    OMX_U32 size           = 1024*5;
    OSAL_BUS_WIDTH address = 0;
    
    err = OMX_OSAL_AllocatorAllocMem(&alloc, &size, &data, &address);
    
    BOOST_REQUIRE(err == OMX_ErrorNone);
    BOOST_REQUIRE(data != NULL);
    BOOST_REQUIRE(address != 0);
    
    OMX_OSAL_AllocatorFreeMem(&alloc, size, data, address);
    OMX_OSAL_AllocatorDestroy(&alloc);
}

int test_main(int, char* [])
{
    cout << "running " << __FILE__ << " tests\n";
    
    test0();
    test1();
    test2();
    
    return 0;
    
}
