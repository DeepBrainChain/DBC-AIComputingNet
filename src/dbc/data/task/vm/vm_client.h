#ifndef DBC_VM_CLIENT_H
#define DBC_VM_CLIENT_H

#include "util/utils.h"
#include "virt_client.h"
#include "service_module/service_module.h"
#include "../TaskResourceManager.h"
#include "../db/task_db_types.h"

class VMTask {
public:
    VMTask(const std::string& domain_name, int32_t operation, const std::string& image_name) {
        this->domain_name = domain_name;
        this->operation = operation;
        this->need_reply = true;
        this->image_name = image_name;
    }
    VMTask(const std::string& domain_name, int32_t operation, const std::string& image_name,
           const TaskResource& task_resource, const std::string& user_name, const std::string& password) {
        this->domain_name = domain_name;
        this->operation = operation;
        this->need_reply = true;
        this->image_name = image_name;
        this->task_resource = task_resource;
        this->user_name = user_name;
        this->password = password;
    }
    std::string domain_name;
    int32_t operation;
    bool need_reply;
    std::string image_name; // used when create vm
    TaskResource task_resource; // used when create vm
    std::string local_ip; // used when create vm
    std::string user_name; // used when create vm
    std::string password; // used when create vm
};

class VmClient {
public:
    VmClient();

    virtual ~VmClient();

protected:
    int32_t CreateDomain(const std::string& domain_name, const std::string& image, const TaskResource& task_resource);

    int32_t StartDomain(const std::string& domain_name);

    int32_t SuspendDomain(const std::string& domain_name);

    int32_t ResumeDomain(const std::string& domain_name);

    int32_t RebootDomain(const std::string& domain_name);

    int32_t ShutdownDomain(const std::string& domain_name);

    int32_t DestoryDomain(const std::string& domain_name);

    int32_t ResetDomain(const std::string& domain_name);

    int32_t UndefineDomain(const std::string& domain_name);

public:
    void ListAllDomains(std::vector<std::string>& domains);

    void ListAllRunningDomains(std::vector<std::string>& domains);

    EVmStatus GetDomainStatus(const std::string& domain_name);

    std::string GetDomainLog(const std::string& domain_name, ETaskLogDirection direction, int32_t n);

protected:
    std::string GetDomainLocalIP(const std::string &domain_name);

    bool SetDomainUserPassword(const std::string &domain_name, const std::string &username, const std::string &pwd);

    void DeleteDiskSystemFile(const std::string &task_id, const std::string& disk_system_file_name);
    void DeleteDiskDataFile(const std::string &task_id);

public:
    void Start();

    void Stop();

    void AddTask(const std::string& domain_name, int32_t operation, const std::string& image);
    void AddTask(const std::string& domain_name, int32_t operation, const std::string& image,
                 const TaskResource& task_resource, const std::string& user_name, const std::string& password);
protected:
    std::shared_ptr<VMTask> PopTask();

    FResult ProcessTask(std::shared_ptr<VMTask>);

    void thread_func();

private:
    std::queue<std::shared_ptr<VMTask> > m_process_tasks;
    bool m_thread_running;
    std::thread* m_thread = nullptr;
    std::mutex m_task_mutex;
    std::condition_variable m_cond;

};

/*
class vm_worker
{
public:
    vm_worker();

    ~vm_worker() = default;

    int32_t init() { return load_vm_config(); }
    std::shared_ptr<vm_config> get_vm_config(std::shared_ptr<ai_training_task> task);
    std::shared_ptr<virt_client> get_worker_if() { return m_vm_client; }

    std::string get_operation(std::shared_ptr<ai_training_task> task);
    std::string get_old_gpu_id(std::shared_ptr<ai_training_task> task);
    std::string get_new_gpu_id(std::shared_ptr<ai_training_task> task);
    std::shared_ptr<update_vm_config> get_update_vm_config(std::shared_ptr<ai_training_task> task);
    std::shared_ptr<vm_config> get_vm_config_from_image(std::shared_ptr<ai_training_task> task);
    std::string get_autodbcimage_version(std::shared_ptr<ai_training_task> task);
    int64_t get_sleep_time(std::shared_ptr<ai_training_task> task);
    std::string get_old_memory(std::shared_ptr<ai_training_task> task);
    std::string get_new_memory(std::shared_ptr<ai_training_task> task);
    std::string get_old_memory_swap(std::shared_ptr<ai_training_task> task);
    std::string get_new_memory_swap(std::shared_ptr<ai_training_task> task);
    int32_t get_old_cpu_shares(std::shared_ptr<ai_training_task> task);
    int32_t get_new_cpu_shares(std::shared_ptr<ai_training_task> task);
    int32_t get_old_cpu_quota(std::shared_ptr<ai_training_task> task);
    int32_t get_new_cpu_quota(std::shared_ptr<ai_training_task> task);

private:
    int32_t load_vm_config();
    int32_t check_cpu_config(const int16_t& cpu_info);
    int32_t check_memory_config(const int16_t& memory, const int16_t& memory_swap, const int64_t& shm_size);

private:
    variables_map m_vm_args;
    const int64_t m_nano_cpu = 1000000000;
    const int64_t m_g_bytes = 1073741824;

    bool m_auto_pull_image = true;

    std::string m_vm_ip;
    uint16_t m_vm_port;
    std::string m_vm_root_dir = "";

    //min disk_free 1024MB
    const uint32_t m_min_disk_free = 1024;

    std::shared_ptr<virt_client> m_vm_client = nullptr;

    int64_t m_memory = 0;
    int64_t m_memory_swap = 0;
    int64_t m_shm_size = 0;
    int64_t m_nano_cpus = 0;
};
*/

#endif // !DBC_VM_CLIENT_H
