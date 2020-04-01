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
        int32_t  oss_task_manager::init()
        {
            if (E_SUCCESS != load_oss_config())
            {
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        void oss_task_manager::set_auth_mode(std::string auth_mode_str)
        {
            if(auth_mode_str == std::string("no_auth"))
            {
                m_auth_mode = DBC_NO_AUTH;
            }
            else if (auth_mode_str == std::string("offline"))
            {
                m_auth_mode = DBC_OFFLINE_AUTH;
            }
            else if (auth_mode_str == std::string("online"))
            {
                m_auth_mode = DBC_ONLINE_AUTH;
            }
            else
            {
                // by default
                m_auth_mode = DBC_ONLINE_AUTH;
            }
        }

        int32_t oss_task_manager::load_oss_config()
        {
            m_enable_idle_task = CONF_MANAGER->get_enable_idle_task();

            set_auth_mode(CONF_MANAGER->get_auth_mode());

            //load offline trust node id list
            auto v = CONF_MANAGER->get_trust_node_ids();
            m_trust_nodes = std::set<std::string> (v.begin(), v.end());

//            m_enable_billing = CONF_MANAGER->get_enable_billing();
            m_enable_billing = (m_auth_mode == DBC_ONLINE_AUTH);

            if (!m_enable_idle_task && !m_enable_billing)
            {
                return E_SUCCESS;
            }

            std::string url = CONF_MANAGER->get_oss_url();
            LOG_INFO << "url config:" << url;
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

            m_get_idle_task_cycle = CONF_MANAGER->get_update_idle_task_cycle()*60*1000;
            if (m_get_idle_task_cycle < UPDATE_IDLE_TASK_MIN_CYCLE)
            {
                m_get_idle_task_cycle = UPDATE_IDLE_TASK_MIN_CYCLE;
            }
            return E_SUCCESS;
        }

        //1. m_next_update_interval=0, when dbc is booted. At this time, call update_idle_task immediately
        //2. if fetch success, then dbc will try call fetch_idle_task after m_get_idle_task_cycle
        //3. if fetch failed, then dbc will try call fetch_idle_task after DEFAULT_UPDATE_IDLE_TASK_CYCLE
        std::shared_ptr<idle_task_resp> oss_task_manager::fetch_idle_task()
        {
            if (!m_enable_idle_task)
            {
                return nullptr;
            }
            
            if (nullptr == m_oss_client)
            {
                LOG_DEBUG << "oss client is null. pls check oss_url in core.conf";
                return nullptr;
            }

            if (time_util::get_time_stamp_ms() < m_next_update_interval)
            {
                return nullptr;
            }

            LOG_DEBUG << "fetch idle task from oss";
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
                m_next_update_interval = time_util::get_time_stamp_ms() + DEFAULT_UPDATE_IDLE_TASK_CYCLE;
                return nullptr;
            }
           
            m_next_update_interval = time_util::get_time_stamp_ms() + m_get_idle_task_cycle;
            return resp;
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
        int32_t oss_task_manager::can_stop_this_task(std::string task_id)
        {
            LOG_DEBUG << "can_stop_this_task:" <<task_id;


            if (nullptr == m_oss_client)
            {
                LOG_WARNING << "auth system is not config.";
                return E_DEFAULT;
            }

            std::shared_ptr<auth_task_req> task_req =  std::make_shared<auth_task_req>();


            task_req->mining_node_id = CONF_MANAGER->get_node_id();
            task_req->task_id = task_id;

            if (nullptr == task_req)
            {
                return E_DEFAULT;
            }


            if (m_oss_client != nullptr)
            {
                int32_t status = m_oss_client->post_auth_stop_task(task_req);
                if (OSS_SUCCESS_TASK == status)
                {
                    return E_SUCCESS;
                }
                LOG_ERROR << "auth failed. auth_status:" << status ;
            }


            return E_DEFAULT;

        }

        int32_t oss_task_manager::can_restart_this_task(std::string task_id)
        {
            LOG_DEBUG << "can_restart_this_task:" <<task_id;


            if (nullptr == m_oss_client)
            {
                LOG_WARNING << "auth system is not config.";
                return E_DEFAULT;
            }

            std::shared_ptr<auth_task_req> task_req =  std::make_shared<auth_task_req>();


            task_req->mining_node_id = CONF_MANAGER->get_node_id();
            task_req->task_id = task_id;

            if (nullptr == task_req)
            {
                return E_DEFAULT;
            }


            if (m_oss_client != nullptr)
            {
                int32_t status = m_oss_client->post_auth_restart_task(task_req);
                if (OSS_SUCCESS_TASK == status)
                {
                    return E_SUCCESS;
                }
                LOG_ERROR << "auth failed. auth_status:" << status ;
            }


            return E_DEFAULT;

        }

        int32_t oss_task_manager::can_start_this_task()
        {



            if (nullptr == m_oss_client)
            {
                LOG_WARNING << "auth system is not config.";
                return E_DEFAULT;
            }

            std::shared_ptr<auth_task_req> task_req =  std::make_shared<auth_task_req>();


            task_req->mining_node_id = CONF_MANAGER->get_node_id();


            if (nullptr == task_req)
            {
                return E_DEFAULT;
            }


            if (m_oss_client != nullptr)
            {
                int32_t status = m_oss_client->post_auth_start_task(task_req);
                if (OSS_SUCCESS_TASK == status)
                {
                    return E_SUCCESS;
                }
                LOG_ERROR << "auth failed. auth_status:" << status ;
            }


            return E_DEFAULT;

        }


        int32_t oss_task_manager::can_update_this_task(std::string task_id)
        {
            LOG_DEBUG << "can_restart_this_task:" <<task_id;


            if (nullptr == m_oss_client)
            {
                LOG_WARNING << "auth system is not config.";
                return E_DEFAULT;
            }

            std::shared_ptr<auth_task_req> task_req =  std::make_shared<auth_task_req>();


            task_req->mining_node_id = CONF_MANAGER->get_node_id();
            task_req->task_id = task_id;

            if (nullptr == task_req)
            {
                return E_DEFAULT;
            }


            if (m_oss_client != nullptr)
            {
                int32_t status = m_oss_client->post_auth_update_task(task_req);
                if (OSS_SUCCESS_TASK == status)
                {
                    return E_SUCCESS;
                }
                LOG_ERROR << "auth failed. auth_status:" << status ;
            }


            return E_DEFAULT;

        }



        int32_t oss_task_manager::auth_online(std::shared_ptr<ai_training_task> task)
        {
            LOG_DEBUG << "online auth task:" << task->task_id;

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

            if (task->status < task_stopped)
            {
                //if oss is abnormal, the value of m_auth_time_interval is default value.
                m_auth_time_interval = DEFAULT_AUTH_REPORT_INTERVAL;
            }

            if (m_oss_client != nullptr)
            {
                std::shared_ptr<auth_task_resp> resp = m_oss_client->post_auth_task(task_req);
                if (nullptr == resp)
                {
                    LOG_WARNING << "bill system can not arrive." << " Next auth time:" << DEFAULT_AUTH_REPORT_CYTLE << "m";
//                    return E_SUCCESS;
                    return E_NETWORK_FAILURE;
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
                    return E_NETWORK_FAILURE;
//                    return E_SUCCESS;
                }

                LOG_ERROR << "auth failed. auth_status:" << resp->status << " contract_state:" << resp->contract_state;
            }
            else
            {
                return E_SUCCESS;
            }

            return E_DEFAULT;


        }

        int32_t oss_task_manager::auth_offline(std::shared_ptr<ai_training_task> task)
        {
            //
            if( m_trust_nodes.empty())
            {
                LOG_DEBUG << "no trust nodes, fall back to no auth mode";
                return E_SUCCESS;
            }

            //
            if (m_trust_nodes.find(task->ai_user_node_id) != m_trust_nodes.end())
            {
                LOG_DEBUG << "task's user id is in trust node list";
                return E_SUCCESS;
            }

            LOG_DEBUG << "task's user id is not in trust node list";
            return E_DEFAULT;

        }


        int32_t oss_task_manager::auth_task(std::shared_ptr<ai_training_task> task)
        {
            //dbc is setted to not need authentication
//            if (!m_enable_billing)
            if (m_auth_mode == DBC_NO_AUTH)
            {
//                return E_SUCCESS;
                return E_BILL_DISABLE;
            }

            if (false == task_need_auth(task))
            {
                return E_SUCCESS;
            }

            int32_t rtn = E_SUCCESS;
            dbc_auth_mode mode = m_auth_mode;

            // online auth
      //      if( mode == DBC_ONLINE_AUTH)
      //      {
       //         rtn = auth_online(task);
      //          if (rtn == E_NETWORK_FAILURE)
      //          {
                    //fallback to offline auth
      //              mode = DBC_OFFLINE_AUTH;
      //          }
      //      }

            // offline auth
     //       if ( mode == DBC_OFFLINE_AUTH)
     //       {
     //           rtn = auth_offline(task);
     //       }

            return rtn;

//            LOG_DEBUG << "auth task:" << task->task_id;
//
//            if (0 == task->start_time)
//            {
//                task->__set_start_time(time_util::get_time_stamp_ms());
//            }
//
//            task->__set_end_time(time_util::get_time_stamp_ms());
//
//            if (nullptr == m_oss_client)
//            {
//                LOG_WARNING << "bill system is not config.";
//                return E_SUCCESS;
//            }
//
//            std::shared_ptr<auth_task_req> task_req = create_auth_task_req(task);
//            if (nullptr == task_req)
//            {
//                return E_SUCCESS;
//            }
//
//            if (task_req->sign.empty())
//            {
//                LOG_DEBUG << "sign error";
//                return E_DEFAULT;
//            }
//
//            if (task->status < task_stopped)
//            {
//                //if oss is abnormal, the value of m_auth_time_interval is default value.
//                m_auth_time_interval = DEFAULT_AUTH_REPORT_INTERVAL;
//            }
//
//            if (m_oss_client != nullptr)
//            {
//                std::shared_ptr<auth_task_resp> resp = m_oss_client->post_auth_task(task_req);
//                if (nullptr == resp)
//                {
//                    LOG_WARNING << "bill system can not arrive." << " Next auth time:" << DEFAULT_AUTH_REPORT_CYTLE << "m";
////                    return E_SUCCESS;
//                    return E_NETWORK_FAILURE;
//                }
//
//                if (OSS_SUCCESS == resp->status  && resp->contract_state == "Active"
//                    && (task->status < task_stopped))
//                {
//                    LOG_INFO << "auth success " << " next auth time:" << resp->report_cycle << "m";
//                    m_auth_time_interval = resp->report_cycle * 60 * 1000;
//                    return E_SUCCESS;
//                }
//
//                if (OSS_SUCCESS == resp->status  && "Active" == resp->contract_state)
//                {
//                    return E_SUCCESS;
//                }
//
//                //if status=-1, means the rsp message is not real auth_resp
//                if (OSS_NET_ERROR == resp->status)
//                {
//                    LOG_WARNING << "bill system can not arrive." << " Next auth time:" << DEFAULT_AUTH_REPORT_CYTLE << "m";
//                    return E_NETWORK_FAILURE;
////                    return E_SUCCESS;
//                }
//
//                LOG_ERROR << "auth failed. auth_status:" << resp->status << " contract_state:" << resp->contract_state;
//            }
//            else
//            {
//                return E_SUCCESS;
//            }

//            return E_DEFAULT;
        }

        bool oss_task_manager::task_need_auth(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return false;
            }

            //jimmy: design change:
            //  1) auth the task before run
            //  2) stop a task by owner from external
            //  3) no more send billing action when m_auth_time_interval expired

            if (task_queueing == task->status)
            {
                return true;
            }

/*
            if (task->status < task_stopped)
            {
                ////dbc maybe restart and the task is  running state, at this time, dbc should send auth req
                if (0 == m_auth_time_interval)
                {
                    return true;
                }

                if (task_queueing == task->status)
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

 */
            return false;
        }
    }
}