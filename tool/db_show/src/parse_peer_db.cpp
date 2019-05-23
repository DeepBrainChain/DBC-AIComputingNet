#include "comm_def.h"
#include "peers_db_types.h"
using namespace matrix::service_core;


void parse_peer_db(const char *path)
{
    leveldb::DB      *db;
    leveldb::Options  options;

    options.create_if_missing = false;
    options.reuse_logs = true;
    std::shared_ptr<leveldb::DB> m_peer_db;

    try
    {
        // open 
        leveldb::Status status = leveldb::DB::Open(options, path, &db);
        if (db == nullptr)
        {
            exit(0);
        }
        m_peer_db.reset(db);
        leveldb::ReadOptions r_options;
        r_options.snapshot = db->GetSnapshot();
        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_peer_db->NewIterator(r_options));
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            //deserialization
            std::shared_ptr<byte_buf> buf(new byte_buf);
            buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
            binary_protocol proto(buf.get());
            db_peer_candidate db_candidate;
            db_candidate.read(&proto);
            /*std::string ip;
            int16_t port;
            int8_t net_state;
            int32_t reconn_cnt;
            int64_t last_conn_tm;
            int32_t score;
            std::string node_id;
            int8_t node_type;*/
            //cout << t_info.task_id << "," << t_info.create_time << "," << to_training_task_status_string(t_info.status) << "," << t_info.result << endl;
            cout << "ip:" << db_candidate.ip << " port:" << db_candidate.port << " state:" << to_net_state_string(db_candidate.net_state) << " reconn_cnt:" << db_candidate.reconn_cnt
                << " last_conn_tm:" << db_candidate.last_conn_tm << " score:" << db_candidate.score << " node_id:" << db_candidate.node_id << " node_type:"
                << to_node_type_string(db_candidate.node_type) << endl;
        }
        db->ReleaseSnapshot(r_options.snapshot);
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