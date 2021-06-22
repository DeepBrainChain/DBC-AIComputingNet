/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   net_address.h
* description      :   p2p network address
* date             :   2018.07.29
* author           :   Regulus Chan
**********************************************************************************/

#pragma once
#include "common.h"
#include "nio_loop_group.h"
namespace matrix
{
    namespace core
    {
        class flow_ctrl: public std::enable_shared_from_this<flow_ctrl>, public boost::noncopyable
        {
        public:
            flow_ctrl() = default;
            ~flow_ctrl() = default;
            bool over_speed(int32_t in_num);
            flow_ctrl(int32_t max_num, int32_t cycle, int32_t slice, io_service *io);
            void start();
            void stop();
        private:
            void start_flow_ctrl_timer();
            void on_flow_ctrl_timer_expired(const boost::system::error_code& error);
        private:
            int32_t m_max_num;
            int32_t m_cycle;
            int32_t m_window;
            int32_t m_cycle_slice;
            std::atomic<int32_t> m_free_window;
        private:
            steady_timer m_flow_ctrl_timer;
            //std::shared_ptr<steady_timer> m_flow_ctrl_timer;
        };
    }
}