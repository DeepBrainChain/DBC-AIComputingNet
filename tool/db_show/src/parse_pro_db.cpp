#include "comm_def.h"
#include "ai_db_types.h"

void parse_provide_db(const char *path)
{
    leveldb::DB      *db;
    leveldb::Options  options;

    options.create_if_missing = false;
    options.reuse_logs = true;
    std::shared_ptr<leveldb::DB> m_prov_training_task_db;

    try
    {
        // open 
        leveldb::Status status = leveldb::DB::Open(options, path, &db);
        if (db == nullptr)
        {
            exit(0);
        }

        leveldb::ReadOptions r_options;
        r_options.snapshot = db->GetSnapshot();

        m_prov_training_task_db.reset(db);

        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_prov_training_task_db->NewIterator(r_options));

        //std::unordered_map<std::string, std::shared_ptr<ai_training_task>> m_training_tasks;
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            ai::dbc::ai_training_task task;
            //deserialization
            std::shared_ptr<matrix::core::byte_buf> task_buf(new matrix::core::byte_buf(10240, true));
            task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception
            binary_protocol proto(task_buf.get());
            task.read(&proto);             //may exception

            cout << "taskid:" << task.task_id << ","
                << "select_mode:" << task.select_mode << ","
                << "master:" << task.master << ","
                << "peer_node_list:" << task.peer_nodes_list.data() << ","
                << "server_specification:" << task.server_specification << ","
                << "server_coount" << task.server_count << ","
                << "engine:" << task.training_engine << ","
                << "code_dir:" << task.code_dir << ","
                << "entry_file:" << task.entry_file << ","
                << "data_dir:" << task.data_dir << ","
                << "check_point_dir:" << task.checkpoint_dir << ","
                << "hyper_parameters:" << task.hyper_parameters << ","
                << "error_times:" << (int)task.error_times << ","
                << "container_id" << task.container_id << ","
                << "recv_time_stamp" << task.received_time_stamp << ","
                << "status:" << to_training_task_status_string(task.status) << ","
                << "ai_user_id:" << task.ai_user_node_id << ","
                << "start_time:" << task.start_time << ","
                <<"end_time:" << task.end_time << endl;

            //cout << "ai power provider service, task id: " << task->task_id << " container_id: " << task->container_id << " task status: " << task->status;


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
