//
// Created by Jianmin Kuang on 2019-05-14.
//

#include "ai_provider_task_db.h"
#include "log.h"
#include "core/memory/byte_buf.h"
#include "task_common_def.h"


using namespace matrix::core;

namespace ai
{
    namespace dbc
    {
        int32_t ai_provider_task_db::init_db(boost::filesystem::path task_db_path, std::string db_name)
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;
            try
            {
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

                task_db_path /= fs::path(db_name);
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
                if (false == status.ok())
                {
                    LOG_ERROR << "init task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_task_db.reset(db);

                LOG_INFO << "task db path:" << task_db_path;
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create task db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create task db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        std::shared_ptr<ai_training_task>  ai_provider_task_db::load_idle_task()
        {
            std::shared_ptr<ai_training_task> idle_task=nullptr;
            try
            {
                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    if (nullptr == idle_task)
                    {
                        idle_task = std::make_shared<ai_training_task>();
                    }

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
                    task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
                    binary_protocol proto(task_buf.get());
                    idle_task->read(&proto);

                    break;
                }
            }
            catch (...)
            {
                LOG_ERROR << "load idle task from db exception";
                return nullptr;
            }
            return idle_task;
        }


        /**
         *
         * @param training_tasks
         * @return
         */
        int32_t ai_provider_task_db::load_user_task(
                std::unordered_map<std::string, std::shared_ptr<ai_training_task>> & training_tasks)
        {
            try
            {
                std::shared_ptr<ai_training_task> task;

                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    task = std::make_shared<ai_training_task>();

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
                    task_buf->write_to_byte_buf(it->value().data(),
                                                (uint32_t) it->value().size());
                    binary_protocol proto(task_buf.get());
                    task->read(&proto);

                    if (0 != training_tasks.count(task->task_id))
                    {
                        LOG_ERROR << "user training task duplicated: " << task->task_id;
                        continue;
                    }

                    //insert training task
                    training_tasks.insert({task->task_id, task});
                    LOG_DEBUG << "user task scheduling insert ai training task to task map, task id: " << task->task_id
                              << " container_id: " << task->container_id << " task status: "
                              << to_training_task_status_string(task->status);

                }
            }

            catch (...)
            {
                LOG_ERROR << "user task scheduling load task from db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        int32_t ai_provider_task_db::delete_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_SUCCESS;
            }

            try
            {
                LOG_INFO << "delete task from db " << task->task_id;
                m_task_db->Delete(leveldb::WriteOptions(), task->task_id);
            }
            catch (...)
            {
                LOG_ERROR << "delete task abnormal";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        int32_t ai_provider_task_db::write_task_to_db(std::shared_ptr<ai_training_task> task)
        {
            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_task_db->Put(write_options, task->task_id, slice);

            return E_SUCCESS;
        }

    }
}