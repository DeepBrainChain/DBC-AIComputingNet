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
        oss_task_manager::oss_task_manager(std::shared_ptr<container_client> & container_client_ptr)
        {
            m_container_client = container_client_ptr;
            
        }

        int32_t  oss_task_manager::init()
        {
            if (E_SUCCESS != load_oss_config())
            {
                return E_DEFAULT;
            }

            if (m_allow_idle_task)
            {
                LOG_DEBUG << "dbc can run idle task";
                if (E_SUCCESS != init_db())
                {
                    return E_DEFAULT;
                }

                m_oss_timer_group = std::make_shared<nio_loop_group>();
                
                int ret = m_oss_timer_group->init(1);
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "timer matrix manager init timer thread group error";
                    return ret;
                }

                //set timer to fetch idle task from oss
                m_get_idle_task_timer = std::make_shared<steady_timer>(*(m_oss_timer_group->get_io_service()));
                m_oss_timer_group->start();
                start_get_idle_task_timer(m_boot_get_idle_task_interval);
            }
            return E_SUCCESS;
        }

        int32_t oss_task_manager::load_oss_config()
        {
            std::cout << "load_oss_config";
            m_allow_idle_task = CONF_MANAGER->get_allow_idle_task();
            m_allow_auth_task = CONF_MANAGER->get_allow_auth_task();

            if (!m_allow_idle_task && !m_allow_auth_task)
            {
                return E_SUCCESS;
            }

            std::string url = CONF_MANAGER->get_bill_url();
            if (url.empty())
            {
                return E_SUCCESS;
            }
            variable_value val;
            val.value() = url;
            if (true != url_validator().validate(val))
            {
                LOG_ERROR << "url config error";
                return E_DEFAULT;
            }

            std::string crt = CONF_MANAGER->get_bill_crt();
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
        
        int32_t oss_task_manager::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;
            try
            {
                //get db path
                fs::path task_db_path = env_manager::get_db_path();
                if (false == fs::exists(task_db_path))
                {
                    LOG_DEBUG << "db directory path does not exist and create db directory" << task_db_path;
                    fs::create_directory(task_db_path);
                }

                //check db directory
                if (false == fs::is_directory(task_db_path))
                {
                    LOG_ERROR << "db directory path does not exist and exit" << task_db_path;
                    return E_DEFAULT;
                }

                task_db_path /= fs::path("idle_task.db");
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
                if (false == status.ok())
                {
                    LOG_ERROR << "ai power provider service init training task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_idle_task_db.reset(db);

                //load task
                load_idle_task();

                LOG_INFO << "ai power provider training db path:" << task_db_path;
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create task provider db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create task provider db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t oss_task_manager::start_get_idle_task_timer(int32_t interval)
        {
            m_get_idle_task_timer->expires_from_now(std::chrono::seconds(interval));
            //m_timer->async_wait(m_timer_handler);
            m_get_idle_task_timer->async_wait(boost::bind(&oss_task_manager::on_get_idle_task,
                shared_from_this(), boost::asio::placeholders::error));
            return E_SUCCESS;
        }

        void oss_task_manager::stop()
        {
            stop_idle_task_timer();
            if (nullptr != m_oss_timer_group)
            {
                m_oss_timer_group->stop();
                m_oss_timer_group->exit();
            }            
        }

        void oss_task_manager::stop_idle_task_timer()
        {
            if (nullptr != m_get_idle_task_timer)
            {
                boost::system::error_code error;

                //cancel tick timer
                m_get_idle_task_timer->cancel(error);
                if (error)
                {
                    LOG_ERROR << "oss_task_manager cancel idle task timer error: " << error;
                }
                else
                {
                    LOG_DEBUG << "oss_task_manager stop timer";
                }
            }
        }

        int32_t oss_task_manager::on_get_idle_task(const boost::system::error_code& error)
        {
            if (boost::asio::error::operation_aborted == error)
            {
                LOG_DEBUG << "timer matrix manager timer error: aborted";
                return E_DEFAULT;
            }
            
            if (E_SUCCESS != get_idle_task())
            {
                //try again, until fetch success
                start_get_idle_task_timer(m_get_idle_task_interval);
            }
            else
            {   //try fetch idle task, next day
                start_get_idle_task_timer(m_get_idle_task_day_interval);
            }

            return E_SUCCESS;
        }

        int32_t oss_task_manager::get_idle_task()
        {
            LOG_DEBUG << "get idle task";
            std::shared_ptr<idle_task_req> req = std::make_shared<idle_task_req>();
            req->mining_node_id = CONF_MANAGER->get_node_id();
            req->time_stamp = boost::str(boost::format("%d") % time_util::get_time_stamp_ms());
            req->sign_type = ECDSA;
            std::string message = req->mining_node_id + req->time_stamp;
            req->sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());
            std::shared_ptr<idle_task_resp> resp = m_oss_client->post_idle_task(req);
            
            if (nullptr == resp || resp->status != OSS_SUCCESS)
            {
                return E_DEFAULT;
            }

            if (nullptr == m_idle_task)
            {
                m_idle_task = std::make_shared<ai_training_task>();
            }

            if (resp->idle_task_id == "0" && !m_idle_task->task_id.empty())
            {
                //idle_task_id=="0", means delete local idle task
                write_lock_guard<rw_lock> lock(m_lock_idle_task);
                delete_idle_task();
                m_idle_task.reset();
            }
            else if (m_idle_task->task_id != resp->idle_task_id)
            {
                //update local idle task
                write_lock_guard<rw_lock> lock(m_lock_idle_task);
                LOG_DEBUG << "update local idle task:" << resp->idle_task_id;
                m_idle_task->__set_task_id(resp->idle_task_id);
                m_idle_task->__set_code_dir(resp->code_dir);
                m_idle_task->__set_entry_file(resp->entry_file);
                m_idle_task->__set_training_engine(resp->training_engine);
                m_idle_task->__set_data_dir(resp->data_dir);
                m_idle_task->__set_hyper_parameters(resp->hyper_parameters);

                write_task_to_db();
            }

            return E_SUCCESS;
        }

        int32_t oss_task_manager::write_task_to_db()
        {
            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            m_idle_task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_idle_task_db->Put(write_options, m_idle_task->task_id, slice);

            return E_SUCCESS;
        }

        int32_t oss_task_manager::load_idle_task()
        {
            try
            {
                if (nullptr == m_idle_task)
                {
                    m_idle_task = std::make_shared<ai_training_task>();
                }

                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_idle_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    m_idle_task = std::make_shared<ai_training_task>();

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
                    task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception
                    binary_protocol proto(task_buf.get());
                    m_idle_task->read(&proto);            //may exception
                }
            }
            catch (...)
            {
                LOG_ERROR << "ai power provider service load task from db exception";
                return E_DEFAULT;
            }
            return E_SUCCESS;
        }

        int32_t oss_task_manager::create_idle_task(get_idle_task_container_config get_config)
        {
            if (nullptr == m_idle_task)
            {
                return E_DEFAULT;
            }

            std::shared_ptr<container_config> config = get_config(m_idle_task);
            std::shared_ptr<container_create_resp> resp = m_container_client->create_container(config, m_idle_task->task_id);
            if (resp != nullptr && !resp->container_id.empty())
            {
                m_idle_task->__set_container_id(resp->container_id);
                LOG_DEBUG << "create idle task success. idle task id:" << m_idle_task->task_id << " container id:" << m_idle_task->container_id;

                return E_SUCCESS;
            }
            else
            {
                LOG_ERROR << "create idle task failed. idle task id:" << m_idle_task->task_id;
            }
            return E_DEFAULT;
        }

        int32_t oss_task_manager::exec_idle_task(get_idle_task_container_config get_config)
        {
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }

            if (IDLE_TASK_RUNNING == get_idle_task_state())
            {
                return E_SUCCESS;
            }

            //first, set the current time to m_idle_state_begin
            if (0 == m_idle_state_begin)
            {
                m_idle_state_begin = time_util::get_time_stamp_ms();
                return E_SUCCESS;
            }

            //if dbc is in the idle state more  3 min, the call idle task.
            if (time_util::get_time_stamp_ms() - m_idle_state_begin > m_max_idle_state_interval)
            {
                write_lock_guard<rw_lock> lock(m_lock_idle_task);
                if (m_idle_task->container_id.empty())
                {
                    //if container_id does not exist, means dbc need to create container
                    if (E_SUCCESS != create_idle_task(get_config))
                    {
                        LOG_ERROR << "create idle task error";
                        return E_DEFAULT;
                    }
                }
                
                int32_t ret = m_container_client->start_container(m_idle_task->container_id);
                
                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "start idle task error";
                    return E_SUCCESS;
                }

                LOG_INFO << "start idle task success. Idle task id:" << m_idle_task->task_id;
                m_idle_task->__set_start_time(time_util::get_time_stamp_ms());
                write_task_to_db();
            }

            return E_SUCCESS;
        }

        int32_t oss_task_manager::stop_idle_task()
        {
            m_idle_state_begin = 0;

            write_lock_guard<rw_lock> lock(m_lock_idle_task);
            if (nullptr == m_idle_task ||  m_idle_task->container_id.empty())
            {
                return E_SUCCESS;
            }
            LOG_INFO << "stop idle task " << m_idle_task->task_id;
            m_idle_task->__set_end_time(time_util::get_time_stamp_ms());
            write_task_to_db();
            return m_container_client->stop_container(m_idle_task->container_id);
        }

        int32_t oss_task_manager::delete_idle_task()
        {
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }

            try
            {
                LOG_INFO << "delete idle task " << m_idle_task->task_id;
                m_idle_task_db->Delete(leveldb::WriteOptions(), m_idle_task->task_id);
            }
            catch (...)
            {
                LOG_ERROR << "p2p net service clear peer candidates db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        idle_task_state oss_task_manager::get_idle_task_state()
        {
            write_lock_guard<rw_lock> lock(m_lock_idle_task);
            if (nullptr == m_idle_task || m_idle_task->container_id.empty())
            {
                return IDLE_TASK_STOPPED;
            }
            //inspect container
            std::shared_ptr<container_inspect_response> resp = m_container_client->inspect_container(m_idle_task->container_id);
            if (nullptr == resp)
            {
                m_idle_task->__set_container_id("");
                return IDLE_TASK_STOPPED;
            }

            if (true == resp->state.running)
            {
                return IDLE_TASK_RUNNING;
            }
            return IDLE_TASK_STOPPED;
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