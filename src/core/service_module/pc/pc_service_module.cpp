/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   pc_service_module.cpp
* description    :   service module on PC platform
* date                  :   2018.01.20
* author             :   Bruce Feng
**********************************************************************************/
#if defined(WIN32) || defined(__linux__) || defined(MAC_OSX)

#include <cassert>
#include "service_bus.h"
#include "service_module.h"
#include "service_message_id.h"
#include "timer_matrix_manager.h"
#include "server.h"
#include "time_tick_notification.h"
#include "timer_def.h"
//#include "service_proto_filter.h"

namespace matrix
{
    namespace core
    {

        void module_task(void *argv)
        {
            assert(argv);

            service_module *module = (service_module *)argv;
            module->run();
        }

        service_module::service_module()
            : m_exited(false),
            m_module_task(module_task),
            m_timer_manager(new timer_manager(this))
        {

        }

        int32_t service_module::init(bpo::variables_map &options)
        {
            init_timer();

            init_invoker();

            init_subscription();

            init_time_tick_subscription();

            return service_init(options);
        }

        void service_module::init_time_tick_subscription()
        {
            TOPIC_MANAGER->subscribe(TIMER_TICK_NOTIFICATION, [this](std::shared_ptr<message> &msg) {return send(msg); });
        }

        int32_t service_module::start()
        {
            m_thread = std::make_shared<std::thread>(m_module_task, this);
            return E_SUCCESS;
        }

        int32_t service_module::run()
        {
            queue_type tmp_msg_queue;
            while (!m_exited)
            {
                //check memory leak

                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    std::chrono::milliseconds ms(500);
                    m_cond.wait_for(lock, ms, [this]()->bool {return !(this->is_empty());});
                    if (is_empty())
                        continue;
                    m_msg_queue.swap(tmp_msg_queue);
                }

                //invoke each
                std::shared_ptr<message> msg;
                while (!tmp_msg_queue.empty())
                {
                    //pop msg
                    msg = tmp_msg_queue.front();
                    tmp_msg_queue.pop();
                    on_invoke(msg);
                }
            }

            return E_SUCCESS;
        }

        int32_t service_module::stop()
        {
            m_exited = true;

            //have a rest           
            if (m_thread != nullptr)
            {
                try
                {
                    m_thread->join();
                }
                catch (const std::system_error &e)
                {
                    LOG_ERROR << "join thread(" << m_thread->get_id() << ")error: " << e.what();
                    return E_DEFAULT;
                }
                catch (...)
                {
                    LOG_ERROR << "join thread(" << m_thread->get_id() << ")error: unknown";
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        int32_t service_module::exit()
        {
            return service_exit();
        }

        int32_t service_module::on_invoke(std::shared_ptr<message> &msg)
        {
            //timer point notification trigger timer process
            if (msg->get_name() == TIMER_TICK_NOTIFICATION)
            {
                std::shared_ptr<time_tick_notification> content = std::dynamic_pointer_cast<time_tick_notification>(msg->get_content());
                assert(nullptr != content);

                return m_timer_manager->process(content->time_tick);

            }
            else
            {
                return service_msg(msg);
            }
        }

        int32_t service_module::send(std::shared_ptr<message> msg)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_msg_queue.size() < DEFAULT_MESSAGE_COUNT)
            {
                m_msg_queue.push(msg);          //right value ref
            }
            else
            {
                LOG_ERROR << "service module message queue full";
                return E_MSG_QUEUE_FULL;
            }

            if (!m_msg_queue.empty())
            {
                m_cond.notify_all();
            }

            return E_SUCCESS;
        }

        int32_t service_module::service_msg(std::shared_ptr<message> &msg)
        {
            auto it = m_invokers.find(msg->get_name());
            if (it == m_invokers.end())
            {
                LOG_ERROR << this->module_name() << " received unknown message: " << msg->get_name();
                return E_DEFAULT;
            }

            auto func = it->second;
            return func(msg);
        }

        int32_t service_module::on_time_out(std::shared_ptr<core_timer> timer)
        {
            auto it = m_timer_invokers.find(timer->get_name());
            if (it == m_timer_invokers.end())
            {
                LOG_ERROR << this->module_name() << " received unknown timer: " << timer->get_name();
                return E_DEFAULT;
            }

            auto func = it->second;
            return func(timer);
        }


        uint32_t service_module::add_timer(std::string name, uint32_t period, uint64_t repeat_times, const std::string & session_id)
        {
            return m_timer_manager->add_timer(name, period, repeat_times, session_id);
        }

        void service_module::remove_timer(uint32_t timer_id)
        {
            m_timer_manager->remove_timer(timer_id);
        }

        int32_t service_module::add_session(std::string session_id, std::shared_ptr<service_session> session)
        {
            auto it = m_sessions.find(session_id);
            if (it != m_sessions.end())
            {
                return E_DEFAULT;
            }

            m_sessions.insert({ session_id, session });
            return E_SUCCESS;
        }

        std::shared_ptr<service_session> service_module::get_session(std::string session_id)
        {
            auto it = m_sessions.find(session_id);
            if (it == m_sessions.end())
            {
                return nullptr;
            }

            return it->second;
        }

        void service_module::remove_session(std::string session_id)
        {
            m_sessions.erase(session_id);
        }

    }

}

#endif