#include "comm_def.h"
#include "ai_db_types.h"

void parse_req_db(const char *path)
{
    leveldb::DB      *db;
    leveldb::Options  options;

    options.create_if_missing = false;
    std::shared_ptr<leveldb::DB> m_req_training_task_db;

    try
    {
        // open 
        leveldb::Status status = leveldb::DB::Open(options, path, &db);
        if (db == nullptr)
        {
            exit(0);
        }
        m_req_training_task_db.reset(db);

        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_req_training_task_db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            //deserialization
            std::shared_ptr<byte_buf> buf(new byte_buf);
            buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
            binary_protocol proto(buf.get());
            ai::dbc::cmd_task_info t_info;
            t_info.read(&proto);
            cout << t_info.task_id << "," << t_info.create_time << "," << to_training_task_status_string(t_info.status) << "," << t_info.result << endl;
        }
    }
    catch (const std::exception & e)
    {
        cout << e.what();
    }
    catch (...)
    {
        cout << "eeor";
    }
}
