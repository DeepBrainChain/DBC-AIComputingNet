/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºbyte_buf.h
* description    £ºbyte_buf for network transport
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <memory>
#include "common.h"

/*
 *           byte_buf is as following  :-)

              read_ptr  ---------                              ---------  write_ptr
                                           \                        /
                                            ---------------------------------------------------------
                buf-->                |    |    |    |    |    |                                              |
                                            ---------------------------------------------------------
 *
*/


#define MAX_BYTE_BUF_LEN                (16 * 1024 * 1024)                       //max 16M bytes
#define DEFAULT_BUF_LEN     10240 



namespace matrix
{
    namespace core
    {
        class byte_buf
        {
            using char_ptr = typename std::shared_ptr<char>;

        public:

            byte_buf(int32_t len = DEFAULT_BUF_LEN, bool auto_alloc = true);

            char * get_buf() { return m_buf.get(); }

            int32_t get_buf_len() const { return m_len; }

            int32_t  write_to_byte_buf(const char * in_buf, uint32_t buf_len);

            int32_t read_from_byte_buf(char * out_buf, uint32_t buf_len);

            char * get_read_ptr() { return m_read_ptr; }

            uint32_t get_valid_read_len();

            char * get_write_ptr() { return m_write_ptr; }

            uint32_t get_valid_write_len();

            int32_t move_write_ptr(uint32_t move_len);

            int32_t move_read_ptr(uint32_t move_len);

            //move to buffer header
            void move_buf();

            void reset();

            std::string to_string();

        protected:

            uint32_t m_len;

            char_ptr m_buf;
            
            char *m_write_ptr;                      //move pointer when serialization from message object to byte buffer (dbc --> network)

            char *m_write_bound;

            bool m_auto_alloc;                      //if buffer is not enough, alloc automaticallu

            char *m_read_ptr;                       //move pointer when deserialization from byte buffer to message object (network --> dbc)

            char *m_read_bound;

        };

    }

}


