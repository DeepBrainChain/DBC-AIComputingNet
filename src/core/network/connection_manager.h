/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºconnection_manager.h
* description    £ºconnection manager as controller class for dbc core connection
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include "module.h"
#include "nio_loop_group.h"
#include "tcp_acceptor.h"
#include "tcp_connector.h"
#include "rw_lock.h"
#include "socket_channel_handler.h"
#include "channel.h"

using namespace std;


#define DEFAULT_ACCEPTOR_THREAD_COUNT            1
#define DEFAULT_CONNECTOR_THREAD_COUNT        1
#define DEFAULT_WORKER_THREAD_COUNT               8

namespace matrix
{
    namespace core
    {
        class connection_manager : public module
        {

            using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

        public:

            connection_manager();

            virtual ~connection_manager() = default;
            
            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t exit();

            virtual std::string module_name() const { return connection_manager_name; }

        public:

            //listen
            int32_t start_listen(tcp::endpoint ep, handler_create_functor func);

            int32_t stop_listen(tcp::endpoint ep);

            //connect
            int32_t start_connect(tcp::endpoint connect_addr, handler_create_functor func);

            int32_t stop_connect(tcp::endpoint connect_addr);

            int32_t add_channel(socket_id sid, shared_ptr<channel> channel);

            void remove_channel(socket_id sid);

            int32_t send_message(socket_id sid, std::shared_ptr<message> msg);

        protected:

            //io_services
            virtual int32_t start_io_services();

            virtual int32_t stop_io_services();

            //all listen and connect
            virtual int32_t stop_all_listen();

            virtual int32_t stop_all_connect();

            virtual int32_t stop_all_channel();

        protected:

            rw_lock m_lock;

            //io service group
            nio_loop_ptr m_worker_group;

            nio_loop_ptr m_acceptor_group;

            nio_loop_ptr m_connector_group;

            //acceptor
            list<shared_ptr<tcp_acceptor>> m_acceptors;

            //connector
            list<shared_ptr<tcp_connector>> m_connectors;

            //socket channel
            std::map<socket_id, shared_ptr<channel>, cmp_key> m_channels;

        };

    }

}

