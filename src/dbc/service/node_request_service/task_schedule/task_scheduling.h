#ifndef DBC_TASK_SCHEDULER_H
#define DBC_TASK_SCHEDULER_H

#include "container_client.h"
#include <memory>
#include <string>
#include <set>
#include "oss_client.h"
#include "db/ai_db_types.h"
#include <leveldb/db.h>
#include <chrono>
#include "rw_lock.h"
#include "image_manager.h"
#include "service_module.h"
#include "container_worker.h"
#include "vm_worker.h"
#include "db/ai_provider_task_db.h"
#include <boost/process.hpp>
#include "nio_loop_group.h"
#include "image_manager.h"
#include "gpu_pool.h"
#include "server_initiator.h"

#define CONTAINER_WORKER_IF m_container_worker->get_worer_if()
#define VM_WORKER_IF m_vm_worker->get_worker_if()

using namespace std;
using namespace matrix::core;
namespace bp = boost::process;

#define MAX_TASK_COUNT 200000
#define MAX_PRUNE_TASK_COUNT 160000
extern std::map<std::string, std::shared_ptr<ai::dbc::ai_training_task> > m_running_tasks;

namespace ai {
    namespace dbc {
        enum TASK_STATE {
            DBC_TASK_RUNNING,
            DBC_TASK_NULL,
            DBC_TASK_NOEXIST,
            DBC_TASK_STOPPED
        };

        class task_scheduling {
        public:
            task_scheduling() = default;

            task_scheduling(std::shared_ptr<container_worker> container_worker, std::shared_ptr<vm_worker> vm_worker);

            virtual ~task_scheduling() = default;

            int32_t init(bpo::variables_map &options);

            int32_t process_reboot_task(std::shared_ptr<ai_training_task> task);

            size_t get_user_cur_task_size() { return m_queueing_tasks.size() + m_running_tasks.size(); }

            int32_t process_task();

            void update_gpu_info_from_proc();

            std::string get_gpu_state();

            std::string get_active_tasks();

            std::string get_gpu_spec(const std::string &s);

            TASK_STATE get_task_state(std::shared_ptr<ai_training_task> task, bool is_docker);

            size_t get_total_user_task_size() { return m_training_tasks.size(); }

            std::shared_ptr<ai_training_task> find_task(std::string task_id);

            int32_t add_task(std::shared_ptr<ai_training_task> task);

            int32_t add_update_task(std::shared_ptr<ai_training_task> task);

            int32_t create_task_from_image(std::shared_ptr<ai_training_task> task, std::string autodbcimage_version);

            int32_t start_task(std::shared_ptr<ai_training_task> task, bool is_docker);

            int32_t start_task_from_new_image(std::shared_ptr<ai_training_task> task, std::string autodbcimage_version,
                                              std::string training_engine_new);

            int32_t restart_task(std::shared_ptr<ai_training_task> task, bool is_docker);

            int32_t stop_task(std::shared_ptr<ai_training_task> task);

            int32_t stop_task_only_id(std::string task_id, bool is_docker);

            int32_t stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status);

            int32_t update_task(std::shared_ptr<ai_training_task> task);

            int32_t update_task_commit_image(std::shared_ptr<ai_training_task> task);

            int32_t prune_task();

            int32_t prune_task(int16_t interval);

            std::string get_pull_log(const std::string &training_engine) {
                return m_pull_image_mng->get_out_log(training_engine);
            }

        protected:
            int32_t init_db(const std::string &db_name);

            int32_t exec_task();

            int32_t change_gpu_id(std::shared_ptr<ai_training_task> task);

            int32_t
            commit_change_gpu_id_bash(std::string change_gpu_id_file_name, std::string task_id, std::string old_gpu_id,
                                      std::string new_gpu_id, std::string container_id, std::string cpu_shares,
                                      std::string cpu_quota, std::string memory, std::string memory_swap,
                                      std::string docker_dir);

            int32_t check_training_task_status(const std::shared_ptr<ai_training_task>& task);

            int32_t check_pull_image_state(const std::shared_ptr<ai_training_task>& task);

            int32_t start_pull_image(std::shared_ptr<ai_training_task> task);

            int32_t stop_pull_image(std::shared_ptr<ai_training_task> task);

            int32_t create_task(std::shared_ptr<ai_training_task> task, bool is_docker);

            int32_t delete_task(std::shared_ptr<ai_training_task> task, bool is_docker);

        protected:
            bool m_is_computing_node = false;

            ai_provider_task_db m_task_db;
            std::shared_ptr<image_manager> m_pull_image_mng = nullptr;
            std::shared_ptr<container_worker> m_container_worker = nullptr;
            std::shared_ptr<vm_worker> m_vm_worker = nullptr;

            // all task
            std::unordered_map<std::string, std::shared_ptr<ai_training_task>> m_training_tasks;
            // task_status_queueing | task_status_pulling_image | task_status_creating_image
            std::list<std::shared_ptr<ai_training_task>> m_queueing_tasks;

            int16_t m_prune_intervel = 0;
            gpu_pool m_gpu_pool;
        };
    }
}

#endif