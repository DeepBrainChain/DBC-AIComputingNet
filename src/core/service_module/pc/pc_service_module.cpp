/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºpc_service_module.cpp
* description    £ºservice module on PC platform
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#if defined(WIN32) || defined(__linux__) || defined(MAC_OSX)

#include <cassert>
#include "service_bus.h"
#include "service_module.h"
#include "service_message_id.h"
#include "timer_def.h"
#include "service_proto_filter.h"


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
            m_timer_manager->add_timer(TIMER_NAME_FILTER_CLEAN, TIMER_INTERV_SEC_FILTER_CLEAN);
            return service_init(options);
        }

        int32_t service_module::start()
        {
            m_thread = std::make_shared<std::thread> (m_module_task, this);
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
                    m_cond.wait(lock, [this]()->bool {return !(this->is_empty());});

					m_msg_queue.swap(tmp_msg_queue);
                }

				//invoke each
				std::shared_ptr<message> msg;
				while(!tmp_msg_queue.empty())
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
                m_thread->join();
            }

            return E_SUCCESS;
        }

        int32_t service_module::exit()
        {
            return service_exit();
        }

        int32_t service_module::on_invoke(std::shared_ptr<message> &msg)
        {
            //timer click notification
            if (!msg->get_name().compare(TIMER_CLICK_MESSAGE))
            {
                m_timer_manager->process();
                return E_SUCCESS;
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
                LOG_DEBUG << "service module message queue full";
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
            switch (timer->get_name())
            {
            case DEFAULT_TIMER:
                break;
            case TIMER_NAME_FILTER_CLEAN:
                service_proto_filter::get_mutable_instance().regular_clean();
                break;
            default:
                break;
            }

            return E_SUCCESS;
        }

    }

}

#endif