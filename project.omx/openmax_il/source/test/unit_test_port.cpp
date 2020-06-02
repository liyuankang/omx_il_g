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
#include <vector>

#include <port.h>

using namespace std;

/*
 * Synopsis: Verify port initialization.
 *
 * Expected: Port is initialized to proper values. 
 *
*/
void test0()
{
    OMX_ERRORTYPE err;
    PORT p = {};
    err = HantroOmx_port_init(&p, 3, 3, 5, 8*1024);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    BOOST_REQUIRE(p.def.nBufferCountActual == 3);
    BOOST_REQUIRE(p.def.nBufferCountMin    == 3);
    BOOST_REQUIRE(p.def.nBufferSize        == 8*1024);


    //  initialize to "some" value and suppress valgrind noise
    OMX_BUFFERHEADERTYPE* junk = reinterpret_cast<OMX_BUFFERHEADERTYPE*>(0xDEADBEEF);
    BOOST_REQUIRE(HantroOmx_port_find_buffer(&p, junk) == NULL);

    for (int i=0; i<5; ++i)
    {
        OMX_BOOL ret;
        BUFFER* buff = NULL;
        ret = HantroOmx_port_allocate_next_buffer(&p, &buff);
        
        BOOST_REQUIRE(ret == OMX_TRUE);
        BOOST_REQUIRE(buff);
        BOOST_REQUIRE(buff->flags == BUFFER_FLAG_IN_USE);
            
        OMX_BUFFERHEADERTYPE* header = buff->header;

        BOOST_REQUIRE(HantroOmx_port_find_buffer(&p, header) == buff);
    }

    BOOST_REQUIRE(p.def.nBufferCountActual == 3);
    BOOST_REQUIRE(p.def.nBufferCountMin    == 3);
        
    HantroOmx_port_destroy(&p);              
}

/*
 * Synopsis: Verify port buffer allocation and buffer queueing.
 *
 * Expected: Buffers are allocated and queued for the port. All buffers are 
 *           allocated and queued as expected.
 *
*/
void test1() 
{
    OMX_ERRORTYPE err;
    
    PORT p = {};
    
    err = HantroOmx_port_init(&p, 3, 3, 7, 1024);
    BOOST_REQUIRE(err == OMX_ErrorNone);
    
    vector<BUFFER*> vec;

    for (int i=0; i<10; ++i)
    {
        BUFFER* buff = NULL;
        OMX_BOOL ret = HantroOmx_port_allocate_next_buffer(&p, &buff);
        BOOST_REQUIRE(ret  == OMX_TRUE);
        BOOST_REQUIRE(buff != NULL);
        
        HantroOmx_port_push_buffer(&p, buff);
        
        vec.push_back(buff);
    }

    for (int i=0; i<10; ++i)
    {
        BUFFER* buff = NULL;
        OMX_BOOL ret = OMX_FALSE;
        ret = HantroOmx_port_get_buffer_at(&p, &buff, i);
        BOOST_REQUIRE(ret == OMX_TRUE);
        BOOST_REQUIRE(buff);
        BOOST_REQUIRE(buff == vec[i]);
    }

    BUFFER* buff = NULL;
    BOOST_REQUIRE(HantroOmx_port_get_buffer_at(&p, &buff, 10) == OMX_FALSE);
    BOOST_REQUIRE(buff == NULL);

    for (int i=0; i<10; ++i)
    {
        BUFFER* buff = NULL;
        OMX_BOOL ret = HantroOmx_port_get_buffer(&p, &buff);
        BOOST_REQUIRE(ret == OMX_TRUE);
        BOOST_REQUIRE(buff != NULL);
        BOOST_REQUIRE(vec[i] == buff);
        
        ret = HantroOmx_port_pop_buffer(&p);
        BOOST_REQUIRE(ret == OMX_TRUE);
    }

    HantroOmx_port_destroy(&p);
}

void test2()
{
    PORT p = {};
    
    p.def.bEnabled   = OMX_FALSE;
    p.def.bPopulated = OMX_FALSE;
    BOOST_REQUIRE(HantroOmx_port_is_ready(&p) == OMX_TRUE);

    p.def.bEnabled = OMX_TRUE;
    BOOST_REQUIRE(HantroOmx_port_is_ready(&p) == OMX_FALSE);
    p.def.bPopulated = OMX_TRUE;
    BOOST_REQUIRE(HantroOmx_port_is_ready(&p) == OMX_TRUE);
}

/*
 * Synopsis: Verify that bufferlist can grow dynamically without loosing any data.
 * 
 * Expected: bufferlist is grown at runtime and existing data in the buffer is not lost.
 *
 */
void test3()
{
    BUFFERLIST list = {};
    
    vector<BUFFER> vec;
    vec.resize(1346);

    HantroOmx_bufferlist_init(&list, 5);
    
    BOOST_REQUIRE(HantroOmx_bufferlist_get_size(&list) == 0);
    BOOST_REQUIRE(HantroOmx_bufferlist_get_capacity(&list) >= 5);

    for (vector<BUFFER>::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        BUFFER* ptr = &(*it);
        
        OMX_BOOL ret = HantroOmx_bufferlist_push_back(&list, ptr);
        if (ret == OMX_FALSE)
        {
            OMX_U32 size     = HantroOmx_bufferlist_get_size(&list);
            OMX_U32 capacity = HantroOmx_bufferlist_get_capacity(&list);
            HantroOmx_bufferlist_reserve(&list, capacity * 2);
            BOOST_REQUIRE(HantroOmx_bufferlist_get_size(&list) == size);
            BOOST_REQUIRE(HantroOmx_bufferlist_get_capacity(&list) >= capacity);
            
            HantroOmx_bufferlist_push_back(&list, ptr);
        }
    }
    
    BOOST_REQUIRE(HantroOmx_bufferlist_get_size(&list) == vec.size());
    
    for (vector<BUFFER>::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        BUFFER* ptr = &(*it);
        BUFFER* val = *HantroOmx_bufferlist_at(&list, std::distance(vec.begin(), it));
        BOOST_REQUIRE(val == ptr);
    }
    
    HantroOmx_bufferlist_destroy(&list);

}

/*
 * Synopsis: Create port and allocate some buffers. Try clearing them all.
 * 
 * Expected: All allocated buffers are freed.
 *
 */
void test4()
{
    vector<BUFFER*> vec;
    
    PORT p = {};
    HantroOmx_port_init(&p, 3, 3, 10, 1024);
    
    for (int i=0; i<25; ++i)
    {
        BUFFER* buff = NULL;
        HantroOmx_port_allocate_next_buffer(&p, &buff);
        BOOST_REQUIRE(buff);
        vec.push_back(buff);
    }
    
    BOOST_REQUIRE(HantroOmx_port_buffer_count(&p) == 25);
    HantroOmx_port_release_all_allocated(&p);
    BOOST_REQUIRE(HantroOmx_port_buffer_count(&p) == 0);
    
    HantroOmx_port_destroy(&p);
}


int test_main(int, char* [])
{
    cout << "running " << __FILE__ << " tests\n";

    test0();
    test1();
    test2();
    test3();
    test4();
    return 0;
}
