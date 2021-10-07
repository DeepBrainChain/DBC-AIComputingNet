#ifndef DBC_TASK_MANAGER_H
#define DBC_TASK_MANAGER_H

#include "util/utils.h"
#include <leveldb/db.h>
#include "vm/image_manager.h"
#include "service_module/service_module.h"
#include <boost/process.hpp>
#include "network/nio_loop_group.h"
#include "data/resource/gpu_pool.h"
#include "service/message/matrix_types.h"
#include "util/websocket_client.h"
#include "db/task_db.h"
#include "db/iptable_db.h"
#include "db/sessionid_db.h"
#include "TaskResourceManager.h"
#include "vm/vm_client.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "util/httplib.h"

namespace bp = boost::process;

class TaskManager {
public:
    TaskManager();

    virtual ~TaskManager();

    FResult Init();

    FResult CreateTask(const std::string& wallet, const std::string &task_id,
                       const std::string &login_password, const std::string &additional,
                       int64_t rent_end, USER_ROLE role);

    FResult StartTask(const std::string& wallet, const std::string &task_id);

    FResult StopTask(const std::string& wallet, const std::string &task_id);

    FResult RestartTask(const std::string& wallet, const std::string &task_id);

    FResult ResetTask(const std::string& wallet, const std::string &task_id);

    FResult DeleteTask(const std::string& wallet, const std::string &task_id);

    FResult
    GetTaskLog(const std::string &task_id, ETaskLogDirection direction, int32_t nlines, std::string &log_content);

    std::shared_ptr<dbc::TaskInfo> FindTask(const std::string& wallet, const std::string &task_id);

    void ListAllTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    void ListRunningTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    int32_t GetRunningTaskSize(const std::string& wallet);

    ETaskStatus GetTaskStatus(const std::string &task_id);

    const TaskResourceManager& GetTaskResourceManager() {
        return m_task_resource_mgr;
    }

    void DeleteOtherCheckTask(const std::string& wallet);

    // 删除所有验证人创建的task
    void DeleteAllCheckTasks();

    // 检查session_id
    std::string CheckSessionId(const std::string& session_id, const std::string& session_id_sign);

    // 创建session_id
    std::string CreateSessionId(const std::string& wallet);

    // 获取session_id
    std::string GetSessionId(const std::string& wallet);

    void ProcessTask();

    void PruneTask();

protected:
    FResult init_db();

    FResult remove_invalid_tasks();

    FResult restore_tasks();

    void start_task(virConnectPtr connPtr, const std::string& task_id);

    void close_task(virConnectPtr connPtr, const std::string& task_id);

    void delete_task_data(const std::string& task_id);

    void close_and_delete_task(virConnectPtr connPtr, const std::string& task_id);

    void delete_disk_system_file(const std::string& task_id, const std::string& disk_system_file_name);

    void delete_disk_data_file(const std::string& task_id);

    void shell_delete_iptable(const std::string &public_ip, const std::string &transform_port,
                                     const std::string &vm_local_ip);

    void shell_add_iptable(const std::string &public_ip, const std::string &transform_port,
                              const std::string &vm_local_ip);

    void remove_reject_iptable();

    void remove_iptable(const std::string& task_id);

    void restore_iptable(const std::string& task_id);

    void add_task_resource(const std::string& task_id, int32_t cpu_count, float mem_rate, int32_t gpu_count);

    bool check_cpu(int32_t cpu_count);

    bool check_gpu(int32_t gpu_count);

    bool check_mem(float mem_rate);

    // 获取内网地址
    std::string get_vm_local_ip(const std::string &domain_name);

    bool create_task_iptable(const std::string &domain_name, const std::string &transform_port);

    bool set_vm_password(const std::string &domain_name, const std::string &username, const std::string &pwd);

protected:
    // task
    TaskDB m_task_db;
    std::map<std::string, std::shared_ptr<dbc::TaskInfo> > m_tasks;
    TaskResourceManager m_task_resource_mgr;

    std::list<std::shared_ptr<dbc::TaskInfo> > m_process_tasks;

    // iptable
    IpTableDB m_iptable_db;
    std::map<std::string, std::shared_ptr<dbc::task_iptable> > m_iptables;

    // session_id
    SessionIdDB m_sessionid_db;
    std::map<std::string, std::string> m_wallet_sessionid;
    std::map<std::string, std::string> m_sessionid_wallet;

    // wallet task
    WalletTaskDB m_wallet_task_db;
    std::map<std::string, std::shared_ptr<dbc::rent_task> > m_wallet_tasks;

    VmClient m_vm_client;
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
