/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºpc_service_module.h
* description    £ºservice module on PC platform
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include <thread>
#include <queue>
#include <unordered_map>
#include <limits>
#include "module.h"
#include "timer.h"
#include "common.h"
#include "timer_manager.h"
#include "service_message.h"
#include "rw_lock.h"

#if defined(WIN32) || defined(__linux__) || defined(MAC_OSX)


using namespace std;


#define DEFAULT_MESSAGE_COUNT               102400                     //default message count


namespace matrix
{
    namespace core
    {

        using invoker_type = typename std::function<int32_t(std::shared_ptr<message> &msg)>;

        using timer_invoker_type = typename std::function<int32_t(std::shared_ptr<core_timer> timer)>;

        class service_module : public module
        {
        public:

            typedef std::function<void(void *)> task_functor;
            typedef std::queue<std::shared_ptr<message>> queue_type;

            friend class timer_manager;

            service_module();

            virtual ~service_module() {}

            virtual std::string module_name() const { return ""; }

            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t exit();

            virtual int32_t run();

            virtual int32_t send(std::shared_ptr<message> msg);

            bool is_empty() const { return m_msg_queue.empty(); }

        protected:

            virtual int32_t on_invoke(std::shared_ptr<message> &msg);

            virtual void init_invoker() {}

            virtual void init_timer() {}

            virtual void init_subscription() {}

            virtual void init_time_tick_subscription();

        protected:

            //override by service layer
            virtual int32_t service_init(bpo::variables_map &options) { return E_SUCCESS; }         //derived class should declare which topic is subscribed in service_init

            virtual int32_t service_exit() { return E_SUCCESS; }

            virtual int32_t service_msg(std::shared_ptr<message> &msg);

            virtual int32_t on_time_out(std::shared_ptr<core_timer> timer) override final;

            uint32_t add_timer(std::string name, uint32_t period, uint64_t repeat_times = ULLONG_MAX);                         //period, unit: ms

            void remove_timer(uint32_t timer_id);

        protected:

            bool m_exited;

            mutex m_mutex;

            std::shared_ptr<std::thread> m_thread;

            queue_type m_msg_queue;

            condition_variable m_cond;

            task_functor m_module_task;

            std::shared_ptr<timer_manager> m_timer_manager;

            std::unordered_map<std::string, invoker_type> m_invokers;

            std::unordered_map<std::string, timer_invoker_type> m_timer_invokers;
        };

    }

}

#endif


