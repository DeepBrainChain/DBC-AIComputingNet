#ifndef DBC_TASK_MANAGER_H
#define DBC_TASK_MANAGER_H

#include "util/utils.h"
#include "service_module/service_module.h"
#include <boost/process.hpp>
#include "message/matrix_types.h"
#include "message/vm_task_result_types.h"
#include "vm/vm_client.h"
#include "HttpDBCChainClient.h"
#include "detail/TaskInfoManager.h"
#include "detail/TaskIptableManager.h"
#include "detail/WalletSessionIDManager.h"
#include "detail/WalletRentTaskManager.h"
#include "detail/TaskResourceManager.h"
#include "detail/TaskSnapshotInfoManager.h"
#include "detail/ImageManager.h"

namespace bp = boost::process;

class TaskManager {
public:
    TaskManager() = default;

    virtual ~TaskManager() = default;

    FResult init();
    
    void exit();

    FResult listImages(const std::shared_ptr<dbc::node_list_images_req_data>& data,
                       const AuthoriseResult& result, std::vector<ImageFile> &images);

    FResult downloadImage(const std::string& wallet, const std::shared_ptr<dbc::node_download_image_req_data>& data);

    FResult downloadImageProgress(const std::string& wallet, const std::shared_ptr<dbc::node_download_image_progress_req_data>& data);

    FResult stopDownloadImage(const std::string& wallet, const std::shared_ptr<dbc::node_stop_download_image_req_data>& data);

    FResult uploadImage(const std::string& wallet, const std::shared_ptr<dbc::node_upload_image_req_data>& data);

    FResult uploadImageProgress(const std::string& wallet, const std::shared_ptr<dbc::node_upload_image_progress_req_data>& data);

    FResult stopUploadImage(const std::string& wallet, const std::shared_ptr<dbc::node_stop_upload_image_req_data>& data);

    FResult deleteImage(const std::string& wallet, const std::shared_ptr<dbc::node_delete_image_req_data>& data);

    FResult createTask(const std::string& wallet, const std::shared_ptr<dbc::node_create_task_req_data>& data,
                       int64_t rent_end, USER_ROLE role, std::string& task_id);

    FResult startTask(const std::string& wallet, const std::string &task_id);

    FResult stopTask(const std::string& wallet, const std::string &task_id);

    FResult restartTask(const std::string& wallet, const std::string &task_id, bool force_reboot = false);

    FResult resetTask(const std::string& wallet, const std::string &task_id);

    FResult deleteTask(const std::string& wallet, const std::string &task_id);

    FResult modifyTask(const std::string& wallet, const std::shared_ptr<dbc::node_modify_task_req_data>& data);

    FResult
    getTaskLog(const std::string &task_id, QUERY_LOG_DIRECTION direction, int32_t nlines, std::string &log_content);

    void listAllTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    void listRunningTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec);

    std::shared_ptr<dbc::TaskInfo> findTask(const std::string& wallet, const std::string &task_id);

    int32_t getRunningTasksSize(const std::string& wallet);

    ETaskStatus getTaskStatus(const std::string &task_id);

    int32_t getTaskAgentInterfaceAddress(const std::string &task_id, std::vector<std::tuple<std::string, std::string>> &address);

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

    void broadcast_message(const std::string& msg);

protected:
    bool restore_tasks();

    void start_task(const std::string &task_id);

    void close_task(const std::string &task_id);

    void remove_iptable_from_system(const std::string& task_id);

    void add_iptable_to_system(const std::string& task_id);

    void shell_remove_iptable_from_system(const std::string& task_id, const std::string &public_ip,
                                          const std::string &ssh_port, const std::string &vm_local_ip);

    void shell_add_iptable_to_system(const std::string& task_id, const std::string &public_ip,
                                     const std::string &ssh_port, const std::string &rdp_port,
                                     const std::vector<std::string>& custom_port,
                                     const std::string &vm_local_ip);

    void shell_remove_reject_iptable_from_system();

    void delete_task(const std::string& task_id);

    void delete_disk_file(const std::string& task_id, const std::map<std::string, domainDiskInfo>& diskfiles);

    FResult parse_create_params(const std::string &additional, USER_ROLE role, TaskCreateParams& params);

    bool allocate_cpu(int32_t& total_cores, int32_t& sockets, int32_t& cores_per_socket, int32_t& threads);

    bool allocate_gpu(int32_t gpu_count, std::map<std::string, std::list<std::string>>& gpus);

    bool allocate_mem(uint64_t mem_size);

    bool allocate_disk(uint64_t disk_size);

    FResult parse_vm_xml(const std::string& xml_file_path, ParseVmXmlParams& params);

    bool check_iptables_port_occupied(const std::string& port);

    FResult check_image(const std::string& image_name);

    FResult check_data_image(const std::string& data_image_name);

    FResult check_cpu(int32_t sockets, int32_t cores, int32_t threads);

    FResult check_gpu(const std::map<std::string, std::list<std::string>>& gpus);

    FResult check_disk(const std::map<int32_t, uint64_t>& disks);

    FResult check_operation_system(const std::string& os);

    FResult check_bios_mode(const std::string& bios_mode);

    FResult check_multicast(const std::vector<std::string>& multicast);

    FResult parse_create_snapshot_params(const std::string &additional, const std::string &task_id, std::shared_ptr<dbc::snapshotInfo> &info);

    void process_task_thread_func();

    void add_process_task(const ETaskEvent& ev);

    void process_task(const ETaskEvent& ev);

    void process_create(const ETaskEvent& ev);

    bool create_task_iptable(const std::string &domain_name, const std::string &ssh_port,
                             const std::string& rdp_port, const std::vector<std::string>& custom_port,
                             const std::string &vm_local_ip);

    static void getNeededBackingImage(const std::string &image_name, std::vector<std::string> &backing_images);

    void process_start(const ETaskEvent& ev);

    void process_stop(const ETaskEvent& ev);

    void process_restart(const ETaskEvent& ev);

    void process_force_reboot(const ETaskEvent& ev);

    void process_reset(const ETaskEvent& ev);

    void process_delete(const ETaskEvent& ev);

    void process_create_snapshot(const ETaskEvent& ev);

    void prune_task_thread_func();

protected:    
    std::atomic<bool> m_running {false};
    std::thread* m_process_thread = nullptr;
	std::mutex m_process_mtx;
	std::condition_variable m_process_cond;
	std::queue<ETaskEvent> m_process_tasks; // <task_id>

    std::thread* m_prune_thread = nullptr;
    std::mutex m_prune_mtx;
    std::condition_variable m_prune_cond;

    int32_t m_udp_fd = -1;
};

typedef TaskManager TaskMgr;

#endif
