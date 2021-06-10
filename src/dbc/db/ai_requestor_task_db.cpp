//
// Created by Jianmin Kuang on 2019-05-14.
//

#include "ai_requestor_task_db.h"
#include "log.h"
#include "core/memory/byte_buf.h"
#include "task_common_def.h"


using namespace matrix::core;

namespace ai
{
    namespace dbc
    {
        int32_t ai_requestor_task_db::init_db(boost::filesystem::path task_db_path)
        {
            leveldb::DB *db = nullptr;
            leveldb::Options options;
            options.create_if_missing = true;

            //get db path
//            fs::path task_db_path = env_manager::get_db_path();
            try
            {
                if (false == fs::exists(task_db_path))
                {
                    LOG_DEBUG << "db directory path does not exist and create db directory";
                    fs::create_directory(task_db_path);
                }

                //check db directory
                if (false == fs::is_directory(task_db_path))
                {
                    LOG_ERROR << "db directory path does not exist and exit";
                    return E_DEFAULT;
                }

                task_db_path /= fs::path("req_training_task.db");
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);

                if (false == status.ok())
                {
                    LOG_ERROR << "ai power requestor service init training task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_task_db.reset(db);
                LOG_INFO << "ai power requestor training db path:" << task_db_path;
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "create task req db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception &e)
            {
                LOG_ERROR << "create task req db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }



        bool ai_requestor_task_db::read_task_info_from_db(std::list<std::string> task_ids, std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status/* = 0*/)
        {
            if (!m_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }
            task_infos.clear();
            if (task_ids.empty())
            {
                LOG_WARNING << "not specify task id.";
                return false;
            }

            //read from db
            for (auto task_id : task_ids)
            {
                std::string task_value;
                leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
                if (!status.ok())
                {
                    LOG_ERROR << "read task(" << task_id << ") failed: " << status.ToString();
                    continue;
                }

                //deserialization
                std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
                binary_protocol proto(buf.get());
                ai::dbc::cmd_task_info t_info;
                t_info.read(&proto);
                if (0 == (filter_status & t_info.status))
                {
                    task_infos.push_back(std::move(t_info));
                    LOG_DEBUG << "ai power requestor service read task: " << t_info.task_id;
                }
            }

            return !task_infos.empty();
        }

        bool ai_requestor_task_db::read_task_info_from_db(std::string task_id, ai::dbc::cmd_task_info &task_info)
        {
            if (task_id.empty() || !m_task_db)
            {
                return false;
            }

            try
            {
                std::string task_value;
                leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
                if (!status.ok() || status.IsNotFound())
                {
                    LOG_ERROR << "read task(" << task_id << ") failed: " << status.ToString();
                    return false;
                }

                //deserialization
                std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
                binary_protocol proto(buf.get());
                task_info.read(&proto);
            }
            catch (...)
            {
                LOG_ERROR << "failed to read task info from db";
                return false;
            }

            return true;
        }

        bool ai_requestor_task_db::is_task_exist_in_db(std::string task_id)
        {
            if (task_id.empty() || !m_task_db)
            {
                return false;
            }

            std::string task_value;
            leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
            if (!status.ok() || status.IsNotFound())
            {
                return false;
            }

            return true;
        }

        std::string ai_requestor_task_db::get_node_id_from_db(std::string task_id)
        {
            if (task_id.empty() || !m_task_db)
            {
                return "";
            }

            std::string task_value;
            leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
            if (!status.ok() || status.IsNotFound())
            {
                return "";
            }


            //deserialization
            std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
            buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
            binary_protocol proto(buf.get());
            ai::dbc::cmd_task_info t_info;
            t_info.read(&proto);


            std::string s = t_info.description;

            // description format:  "<node id> : ..."
            std::string delimiter = " : ";

            return s.substr(0, s.find(delimiter));
        }


        void ai_requestor_task_db::reset_task_status_to_db(std::string task_id)
        {
            if (!m_task_db || task_id.empty())
            {
                LOG_ERROR << "null ptr or null task_id.";
                return;
            }

            ai::dbc::cmd_task_info task_info_in_db;
            if(read_task_info_from_db(task_id, task_info_in_db))
            {
                if(task_info_in_db.status != task_status_unknown)
                {
                    task_info_in_db.__set_status(task_status_unknown);
                    write_task_info_to_db(task_info_in_db);
                }
            }
            else
            {
                LOG_ERROR << "not exist in task db: " << task_id;
            }
        }

        bool ai_requestor_task_db::write_task_info_to_db(ai::dbc::cmd_task_info &task_info)
        {
            if (!m_task_db || task_info.task_id.empty())
            {
                LOG_ERROR << "null ptr or null task_id.";
                return false;
            }

            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            task_info.write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            leveldb::Status s = m_task_db->Put(write_options, task_info.task_id, slice);
            if (!s.ok())
            {
                LOG_ERROR << "ai power requestor service write task to db, task id: " << task_info.task_id << " failed: " << s.ToString();
                return false;
            }

            LOG_INFO << "ai power requestor service write task to db, task id: " << task_info.task_id;
            return true;
        }

        bool ai_requestor_task_db::read_task_info_from_db(std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status/* = 0*/)
        {
            if (!m_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }
            task_infos.clear();

            //read from db
            try
            {
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    //deserialization
                    std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                    buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
                    binary_protocol proto(buf.get());
                    ai::dbc::cmd_task_info t_info;
                    t_info.read(&proto);

                    if (0 == (filter_status & t_info.status))
                    {
                        task_infos.push_back(std::move(t_info));
                        LOG_DEBUG << "ai power requestor service read task: " << t_info.task_id;
                    }
                }
            }
            catch (...)
            {
                LOG_ERROR << "ai power requestor service read task: broken data format";
                return false;
            }

            return !task_infos.empty();
        }

        void ai_requestor_task_db::delete_task(std::string task_id)
        {
            m_task_db->Delete(leveldb::WriteOptions(), task_id);
        }

    }
}