#ifndef DBC_TASK_MANAGER_H
#define DBC_TASK_MANAGER_H

#include "util/utils.h"
#include "service_module/service_module.h"
#include <boost/process.hpp>
#include "network/nio_loop_group.h"
#include "service/message/matrix_types.h"
#include "service/message/vm_task_result_types.h"
#include "vm/vm_client.h"
#include "HttpChainClient.h"
#include "TaskInfoManager.h"
#include "TaskIptableManager.h"
#include "WalletSessionIDManager.h"
#include "WalletRentTaskManager.h"
#include "TaskResourceManager.h"
#include "SnapshotManager.h"

namespace bp = boost::process;

class TaskManager {
public:
    TaskManager();

    virtual ~TaskManager();

    FResult init();

    void start();

    void stop();

    FResult createTask(const std::string& wallet, const std::string &additional,
                       int64_t rent_end, USER_ROLE role, std::string& task_id);

    FResult startTask(const std::string& wallet, const std::string &task_id);

    FResult stopTask(const std::string& wallet, const std::string &task_id);

    FResult restartTask(const std::string& wallet, const std::string &task_id);

    FResult resetTask(const std::string& wallet, const std::string &task_id);

    FResult deleteTask(const std::string& wallet, const std::string &task_id);

    FResult
    getTaskLog(const std::string &task_id, ETaskLogDirection direction, int32_t nlines, std::string &log_content);

    void listAllTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    void listRunningTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    std::shared_ptr<dbc::TaskInfo> findTask(const std::string& wallet, const std::string &task_id);

    int32_t getRunningTasksSize(const std::string& wallet);

    ETaskStatus getTaskStatus(const std::string &task_id);

    void deleteAllCheckTasks();

    void deleteOtherCheckTasks(const std::string& wallet);

    std::string createSessionId(const std::string& wallet, const std::vector<std::string>& multisig_signers = std::vector<std::string>());

    std::string getSessionId(const std::string& wallet);

    std::string checkSessionId(const std::string& session_id, const std::string& session_id_sign);

    // this function do not check renter's wallet of task
    void listTaskDiskInfo(const std::string& task_id, std::map<std::string, domainDiskInfo>& disks);

    // snapshot
    FResult createSnapshot(const std::string& wallet, const std::string &additional, const std::string& task_id);

    FResult deleteSnapshot(const std::string& wallet, const std::string &task_id, const std::string& snapshot_name);

    FResult listTaskSnapshot(const std::string& wallet, const std::string& task_id, std::vector<std::shared_ptr<dbc::snapshotInfo>>& snaps);

    std::shared_ptr<dbc::snapshotInfo> getTaskSnapshot(const std::string& wallet, const std::string& task_id, const std::string& snapshot_name);

    std::shared_ptr<dbc::snapshotInfo> getCreatingSnapshot(const std::string& wallet, const std::string& task_id);

protected:
    bool restore_tasks();

    void start_task(const std::string &task_id);

    void close_task(const std::string &task_id);

    void remove_iptable_from_system(const std::string& task_id);

    void add_iptable_to_system(const std::string& task_id);

    void shell_remove_iptable_from_system(const std::string &public_ip, const std::string &transform_port,
                                          const std::string &vm_local_ip);

    void shell_add_iptable_to_system(const std::string &public_ip, const std::string &transform_port,
                                     const std::string &vm_local_ip);

    void shell_remove_reject_iptable_from_system();

    void delete_task(const std::string& task_id);

    void delete_disk_system_file(const std::string &task_id, const std::string& disk_system_file_name);

    void delete_disk_data_file(const std::string &task_id);

    FResult parse_create_params(const std::string &additional, USER_ROLE role, TaskCreateParams& params);

    bool allocate_cpu(int32_t& total_cores, int32_t& sockets, int32_t& cores_per_socket, int32_t& threads);

    bool allocate_gpu(int32_t gpu_count, std::map<std::string, std::list<std::string>>& gpus);

    bool allocate_mem(uint64_t mem_size);

    bool allocate_disk(uint64_t disk_size);

    FResult parse_vm_xml(const std::string& xml_file_path, ParseVmXmlParams& params);

    FResult check_image(const std::string& image_name);

    FResult check_data_image(const std::string& data_image_name);

    FResult check_cpu(int32_t sockets, int32_t cores, int32_t threads);

    FResult check_gpu(const std::map<std::string, std::list<std::string>>& gpus);

    FResult check_disk(const std::map<int32_t, uint64_t>& disks);

    FResult check_operation_system(const std::string& os);

    FResult check_bios_mode(const std::string& bios_mode);

    FResult parse_create_snapshot_params(const std::string &additional, const std::string &task_id, std::shared_ptr<dbc::snapshotInfo> &info);

    void process_task_thread_func();

    void add_process_task(const ETaskEvent& ev);

    void process_task(const ETaskEvent& ev);

    void process_create(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    bool create_task_iptable(const std::string &domain_name, const std::string &transform_port, const std::string &vm_local_ip);

    static void getNeededBackingImage(const std::string &image_name, std::vector<std::string> &backing_images);

    void process_start(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void process_stop(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void process_restart(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void process_reset(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void process_delete(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void process_create_snapshot(const std::shared_ptr<dbc::TaskInfo>& taskinfo);

    void prune_task_thread_func();

protected:
    std::mutex m_process_mtx;
    std::condition_variable m_process_cond;
    std::queue<ETaskEvent> m_process_tasks; // <task_id>

    std::atomic<bool> m_running {false};
    std::thread* m_process_thread = nullptr;
    std::thread* m_prune_thread = nullptr;

    VmClient m_vm_client;
    HttpChainClient m_httpclient;
};

typedef TaskManager TaskMgr;

/*
class task_scheduling {
public:
    task_scheduling() = default;

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

    int32_t add_task(const std::shared_ptr<ai_training_task>& task);

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

    ////////////////////////////////////////////////////////////////////////////////////
    int32_t add_request(const std::shared_ptr<TaskRequest>& req);


    void appendAllTaskStatus(std::vector<matrix::service_core::task_status>& vec);

protected:
    int32_t init_db(const std::string &db_name);

    int32_t exec_task(const std::shared_ptr<ai_training_task>& task);

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
*/

#endif // !DBC_TASK_MANAGER_H
