/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   nio_loop_group.h
* description      :   nio thread loop group for network connection
* date             :   2018.01.20
* author           :   Bruce Feng
**********************************************************************************/
#pragma once


#include <thread>
#include <functional>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "io_service_wrapper.h"


using namespace std;

#define MAX_LOOP_GROUP_THREAD_SIZE                         64

namespace matrix
{
    namespace core
    {
        class nio_loop_group
        {
        public:

            nio_loop_group() : m_cur_io_service(0) {}

            virtual ~nio_loop_group();

            virtual int32_t init(size_t thread_size);

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t exit();

            virtual std::shared_ptr<io_service> get_io_service();

        protected:

            size_t m_thread_size;

            std::mutex m_mutex;

            std::size_t m_cur_io_service;

            std::vector<shared_ptr<std::thread>> m_threads;

            std::vector<shared_ptr<io_service_wrapper>> m_io_services;
        };

    }

}
