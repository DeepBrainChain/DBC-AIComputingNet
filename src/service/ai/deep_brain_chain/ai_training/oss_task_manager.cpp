/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   oss_task_manager.cpp
* description    :   oss_task_manager for implementation
* date                  :   2018.10.17
* author            :   Regulus
**********************************************************************************/
#include "common.h"
#include "oss_task_manager.h"
#include "util.h"
#include "utilstrencodings.h"
#include "task_common_def.h"
#include "server.h"
#include <boost/format.hpp>
#include "time_util.h"
#include "url_validator.h"

namespace ai
{
    namespace dbc
    {
        oss_task_manager::oss_task_manager(std::shared_ptr<idle_task> & idle_task_ptr)
        {
            m_idle_task_ptr = idle_task_ptr;
        }

        int32_t  oss_task_manager::init()
        {
            if (E_SUCCESS != load_oss_config())
            {
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t oss_task_manager::load_oss_config()
        {
            m_enable_idle_task = CONF_MANAGER->get_enable_idle_task();
            m_enable_billing = CONF_MANAGER->get_enable_billing();

            if (!m_enable_idle_task && !m_enable_billing)
            {
                return E_SUCCESS;
            }

            std::string url = CONF_MANAGER->get_oss_url();
            if (url.empty())
            {
                m_enable_idle_task = false;
                m_enable_billing = false;
                return E_SUCCESS;
            }
            variable_value val;
            val.value() = url;
            if (true != url_validator().validate(val))
            {
                LOG_ERROR << "url config error";
                return E_DEFAULT;
            }

            std::string crt = CONF_MANAGER->get_oss_crt();
            if (!crt.empty() && !fs::exists(crt))
            {
                LOG_ERROR << "crt path error";
                return E_DEFAULT;
            }

            if (!url.empty())
            {
                m_oss_client = std::make_shared<oss_client>(url, crt);
                if (nullptr == m_oss_client)
                {
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        //1. m_next_update_interval=0, when dbc is booted. At this time, ai ai_power_provider_service.on_training_task_timer call update_idle_task, will fetch idle task
        //2. if fetch success, then dbc will try call fetch_idle_task after 24h
        //3. if fetch failed, then dbc will try call fetch_idle_task after 5min
        int32_t oss_task_manager::update_idle_task()
        {
            if (time_util::get_time_stamp_ms() >= m_next_update_interval)
            {
                fetch_idle_task();
            }
            return E_SUCCESS;
        }

        int32_t oss_task_manager::fetch_idle_task()
        {
            LOG_DEBUG << "fetch idle task from oss";
            if (nullptr == m_oss_client)
            {
                LOG_DEBUG << "oss client is null. pls check oss_url in core.conf";
                return E_SUCCESS;
            }
            std::shared_ptr<idle_task_req> req = std::make_shared<idle_task_req>();
            req->mining_node_id = CONF_MANAGER->get_node_id();
            req->time_stamp = boost::str(boost::format("%d") % time_util::get_time_stamp_ms());
            req->sign_type = ECDSA;
            std::string message = req->mining_node_id + req->time_stamp;
            req->sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());

            std::shared_ptr<idle_task_resp> resp = m_oss_client->post_idle_task(req);
            
            if (nullptr == resp || resp->status != OSS_SUCCESS)
            {
                //try fetch, after 5min
                m_next_update_interval = time_util::get_time_stamp_ms() + m_get_idle_task_interval;
                return E_DEFAULT;
            }
            //try update idle task, after 24h
            m_idle_task_ptr->set_task(resp);
            m_next_update_interval = time_util::get_time_stamp_ms() + m_get_idle_task_day_interval;
            return E_SUCCESS;
        }

        std::shared_ptr<auth_task_req> oss_task_manager::create_auth_task_req(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return nullptr;
            }

            std::shared_ptr<auth_task_req> req = std::make_shared<auth_task_req>();
            req->ai_user_node_id = task->ai_user_node_id;
            req->mining_node_id = CONF_MANAGER->get_node_id();
            req->task_id = task->task_id;

            req->start_time = boost::str(boost::format("%d") % task->start_time);
            int8_t status = task->status;
            if (task_queueing == status || task_pulling_image == status)
            {
                status = task_running;
            }

            if (status >= task_abnormally_closed)
            {
                status = task_abnormally_closed;
            }

            req->task_state = to_training_task_status_string(status);
            req->sign_type = ECDSA;

            req->end_time = boost::str(boost::format("%d") % task->end_time);
            std::string message = req->mining_node_id + req->ai_user_node_id + req->task_id + req->start_time + req->task_state + req->end_time;

            LOG_DEBUG << "sign message:" << message;

            req->sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());
            return req;
        }

        int32_t oss_task_manager::auth_task(std::shared_ptr<ai_training_task> task)
        {
            //dbc is setted to not need authentication
            if (!m_enable_billing)
            {
                return E_SUCCESS;
            }

            if (false == task_need_auth(task))
            {
                return E_SUCCESS;
            }

            LOG_INFO << "auth task:" << task->task_id;

            if (0 == task->start_time)
            {
                task->__set_start_time(time_util::get_time_stamp_ms());
            }

            task->__set_end_time(time_util::get_time_stamp_ms());

            if (nullptr == m_oss_client)
            {
                LOG_WARNING << "bill system is not config.";
                return E_SUCCESS;
            }

            std::shared_ptr<auth_task_req> task_req = create_auth_task_req(task);
            if (nullptr == task_req)
            {
                return E_SUCCESS;
            }

            if (task_req->sign.empty())
            {
                LOG_DEBUG << "sign error";
                return E_DEFAULT;
            }

            m_auth_time_interval = DEFAULT_AUTH_REPORT_CYTLE * 60 * 1000;
            if (m_oss_client != nullptr)
            {
                std::shared_ptr<auth_task_resp> resp = m_oss_client->post_auth_task(task_req);
                if (nullptr == resp)
                {
                    LOG_WARNING << "bill system can not arrive." << " Next auth time:" << DEFAULT_AUTH_REPORT_CYTLE << "m";
                    return E_SUCCESS;
                }

                if (OSS_SUCCESS == resp->status  && resp->contract_state == "Active"
                    && (task->status < task_stopped))
                {
                    LOG_INFO << "auth success " << " next auth time:" << resp->report_cycle << "m";
                    m_auth_time_interval = resp->report_cycle * 60 * 1000;
                    return E_SUCCESS;
                }

                if (OSS_SUCCESS == resp->status  && "Active" == resp->contract_state)
                {
                    return E_SUCCESS;
                }

                //if status=-1, means the rsp message is not real auth_resp
                if (OSS_NET_ERROR == resp->status)
                {
                    LOG_WARNING << "bill system can not arrive." << " Next auth time:" << DEFAULT_AUTH_REPORT_CYTLE << "m";
                    return E_SUCCESS;
                }

                LOG_ERROR << "auth failed. auth_status:" << resp->status << " contract_state:" << resp->contract_state;

            }
            else
            {
                return E_SUCCESS;
            }

            return E_DEFAULT;
        }

        bool oss_task_manager::task_need_auth(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return false;
            }

            if (task->status < task_stopped)
            {
                ////dbc maybe restart and the task is  running state, at this time, dbc should send auth req
                if (0 == m_auth_time_interval)
                {
                    return true;
                }

                int64_t interval = time_util::get_time_stamp_ms() - task->end_time;
                if (interval >= m_auth_time_interval)
                {
                    return true;
                }

                return false;
            }

            //final state should report
            if (task->status >= task_stopped)
            {
                return true;
            }

            return false;
        }

    }

}