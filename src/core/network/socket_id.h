/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºsocket_id.hpp
* description    £ºsocket id for each socket channel
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <map>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/serialization/singleton.hpp>
#include "common.h"


using namespace boost::serialization;


namespace matrix
{
    namespace core
    {

        enum socket_type
        {
            SERVER_SOCKET = 0,
            CLIENT_SOCKET = 1
        };

        class socket_id
        {
        public:

            socket_id() : m_type(SERVER_SOCKET), m_id(0) {}

            socket_id(socket_type type, uint64_t id) : m_type(type), m_id(id) {}

            socket_id(const socket_id &sid) { (*this) = sid; }

            ~socket_id() = default;

            uint64_t get_id() const { return m_id; }

            socket_type get_type() const { return m_type; }

            socket_id & operator=(const socket_id &sid) { this->m_id = sid.m_id; this->m_type = sid.m_type; return *this; }

            std::string to_string() const 
            { 
                std::stringstream str_stream;
                str_stream << "socket type: " << ((m_type == CLIENT_SOCKET) ? "client " : "server ") << " socket id: " << m_id;              
                return  str_stream.str();
            }

        protected:

            uint64_t m_id;

            socket_type m_type;
        };

        inline bool operator==(const socket_id &s1, const socket_id &s2)
        {
            return (s1.get_id() == s2.get_id()) && (s1.get_type() && s2.get_type());
        }

        class cmp_key
        {
        public:

            bool operator() (const socket_id &s1, const socket_id &s2) const
            {
                return (s1.get_type() < s2.get_type()) || ((s1.get_type() == s2.get_type()) && (s1.get_id() < s2.get_id()));
            }
        };

        class socket_id_allocator : public singleton<socket_id_allocator>
        {
        public:

            socket_id_allocator() : m_cur_idx(0) {}

            socket_id alloc_server_socket_id()
            {
                //id will repeat when 8925649.15 years passed 
                //with 65535 repeated connection per second
                return socket_id(SERVER_SOCKET, ++m_cur_idx);
            }

            socket_id alloc_client_socket_id()
            {
                return socket_id(CLIENT_SOCKET, ++m_cur_idx);
            }

        protected:

            std::atomic_uint64_t m_cur_idx;

        };

    }

}