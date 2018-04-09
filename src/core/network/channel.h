/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºchannel.h
* description    £ºchannle is abstract concept for network transmission
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include <memory>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include "service_message.h"


#define MAX_SEND_QUEUE_MSG_COUNT            102400

using namespace boost::asio;

namespace matrix
{
    namespace core
    {

        class channel
        {
        public:

            virtual ~channel() = default;

            virtual int32_t start() = 0;

            virtual int32_t stop() = 0;

            virtual int32_t read() = 0;

            virtual int32_t write(std::shared_ptr<message> msg) = 0;

            virtual void on_error() = 0;

            virtual socket_id id() = 0;

            virtual io_service *get_io_service() = 0;

        };
    }

}
