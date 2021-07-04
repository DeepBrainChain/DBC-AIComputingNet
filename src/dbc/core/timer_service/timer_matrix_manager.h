/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   time_tick_notification.h
* description    :   time_tick_notification for all service module
* date                  :   2018.04.04
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include <memory>
#include <chrono>
#include "module.h"
#include "nio_loop_group.h"
#include "service_message.h"


#define DEFAULT_TIMER_MATRIX_THREAD_COUNT           1
#define DEFAULT_TIMER_INTERVAL                      100                   //unit:ms for one tick


#ifdef WIN32
typedef std::atomic_uint64_t tick_type;
#else
typedef std::atomic<std::uint64_t> tick_type;
#endif


namespace matrix
{
    namespace core
    {

        class timer_matrix_manager : public module, public std::enable_shared_from_this<timer_matrix_manager>, boost::noncopyable
        {

            typedef void (timer_handler_type)(const boost::system::error_code &);

        public:

            timer_matrix_manager();

            ~timer_matrix_manager() = default;

            virtual std::string module_name() const { return "timer matrix manager"; };

            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t exit();

            static uint64_t get_cur_tick() { return m_cur_tick; }

        protected:

            void start_timer();

            void stop_timer();

            std::shared_ptr<dbc::network::message> make_time_tick_notification();

            void on_timer_expired(const boost::system::error_code& error);

        protected:

            std::shared_ptr<dbc::network::nio_loop_group> m_timer_group;

            //std::function<timer_handler_type> m_timer_handler;

            std::shared_ptr<steady_timer> m_timer;

            static tick_type m_cur_tick;

        };

    }

}