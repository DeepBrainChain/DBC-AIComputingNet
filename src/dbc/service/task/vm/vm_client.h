#ifndef DBC_VM_CLIENT_H
#define DBC_VM_CLIENT_H

#include "util/utils.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "../TaskResourceManager.h"

class VmClient {
public:
    VmClient();

    virtual ~VmClient();

    int32_t CreateDomain(const std::string& domain_name, const std::string& image,
                         const std::shared_ptr<TaskResource>& task_resource);

    int32_t StartDomain(const std::string& domain_name);

    int32_t SuspendDomain(const std::string& domain_name);

    int32_t ResumeDomain(const std::string& domain_name);

    int32_t RebootDomain(const std::string& domain_name);

    int32_t ShutdownDomain(const std::string& domain_name);

    int32_t DestoryDomain(const std::string& domain_name);

    int32_t ResetDomain(const std::string& domain_name);

    int32_t UndefineDomain(const std::string& domain_name);

    EVmStatus GetDomainStatus(const std::string& domain_name);

    void ListAllDomains(std::vector<std::string>& domains);

    void ListAllRunningDomains(std::vector<std::string>& domains);

    FResult GetDomainLog(const std::string& domain_name, ETaskLogDirection direction, int32_t linecount, std::string &log_content);

    std::string GetDomainLocalIP(const std::string &domain_name);

    bool SetDomainUserPassword(const std::string &domain_name, const std::string &username, const std::string &pwd);

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
