#include "ResourceManager.h"
#include "comm.h"
#include "common/util.h"

std::string DeviceGpu::parse_bus(const std::string& id) {
    std::vector<std::string> vec;
    matrix::core::string_util::split(id, ":", vec);
    if (vec.size() > 0)
        return vec[0];
    else
        return "";
}

std::string DeviceGpu::parse_slot(const std::string& id) {
    std::vector<std::string> vec;
    matrix::core::string_util::split(id, ":", vec);
    if (vec.size() > 1)
        return vec[1];
    else
        return "";
}

std::string DeviceGpu::parse_function(const std::string& id) {
    std::vector<std::string> vec;
    matrix::core::string_util::split(id, ".", vec);
    if (vec.size() > 1)
        return vec[1];
    else
        return "";
}

void HardwareManager::Init() {
    init_gpu();
}

void HardwareManager::init_gpu() {
    // list all gpu
    std::string cmd = "lspci -nnv |grep NVIDIA |awk '{print $2\",\"$1}' |tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    std::vector<std::string> vec;
    matrix::core::string_util::split(str, "|", vec);
    std::string cur_id;
    for (int i = 0; i < vec.size(); i++) {
        std::vector<std::string> vec2;
        matrix::core::string_util::split(vec[i], ",", vec2);
        if (vec2[0] == "VGA") {
            cur_id = vec2[1];
            m_gpu[cur_id].id = cur_id;
        }
        m_gpu[cur_id].devices.push_back(vec2[1]);
    }

    // list task gpu

}

void HardwareManager::print_gpu() {
    for (auto& it : m_gpu) {
        std::cout << it.first << " : " << "bus=" << DeviceGpu::parse_bus(it.first) << ","
                                       << "slot=" << DeviceGpu::parse_slot(it.first) << ","
                                       << "function=" << DeviceGpu::parse_function(it.first) << std::endl;
        for (auto& it2 : it.second.devices) {
            std::cout << "  " << it2 << std::endl;
        }
    }
}


using namespace matrix::core;

std::string ResourceManager::os_name() {
    std::string os_name = run_shell("cat /etc/os-release | grep VERSION | awk -F '=' '{print $2}'| tr -d '\"'");
    os_name = string_util::rtrim(os_name, '\n');
    std::string os_version = run_shell("uname -o -r");
    os_version = string_util::rtrim(os_version, '\n');
    return os_name + " " + os_version;
}

std::string ResourceManager::cpu_type() {
    std::string cpu_type = run_shell("cat /proc/cpuinfo |grep \"model name\" | awk -F \":\" '{print $2}'| uniq | awk -F \"@\" '{print $1}' |xargs");
    cpu_type = string_util::rtrim(cpu_type, '\n');
    return cpu_type;
}

int32_t ResourceManager::cpu_cores() {
    std::string cpu_cores = run_shell("cat /proc/cpuinfo |grep \"model name\" | awk -F \":\" '{print $2}'| wc -l");
    cpu_cores = string_util::rtrim(cpu_cores, '\n');
    return atoi(cpu_cores.c_str());
}

int32_t ResourceManager::physical_cpu() {
    std::string s_physical_cpu = run_shell("cat /proc/cpuinfo| grep \"physical id\"| sort| uniq| wc -l");
    s_physical_cpu = string_util::rtrim(s_physical_cpu, '\n');
    int32_t num_physical_cpu = atoi(s_physical_cpu.c_str());
    return num_physical_cpu;
}

int32_t ResourceManager::cpu_cores_per_physical() {
    std::string s_cpu_cores_per_physical = run_shell("cat /proc/cpuinfo| grep \"cpu cores\"| uniq");
    s_cpu_cores_per_physical = string_util::rtrim(s_cpu_cores_per_physical, '\n');
    std::vector<std::string> vec1;
    string_util::split(s_cpu_cores_per_physical, ":", vec1);
    s_cpu_cores_per_physical = vec1[1];
    s_cpu_cores_per_physical = string_util::ltrim(s_cpu_cores_per_physical, ' ');
    int32_t num_cpu_cores_per_physical = atoi(s_cpu_cores_per_physical.c_str());
    return num_cpu_cores_per_physical;
}

int32_t ResourceManager::cpu_siblings_per_physical() {
    std::string s_cpu_siblings_per_physical = run_shell("cat /proc/cpuinfo | grep 'siblings' | uniq");
    s_cpu_siblings_per_physical = string_util::rtrim(s_cpu_siblings_per_physical, '\n');
    std::vector<std::string> vec2;
    string_util::split(s_cpu_siblings_per_physical, ":", vec2);
    s_cpu_siblings_per_physical = vec2[1];
    s_cpu_siblings_per_physical = string_util::ltrim(s_cpu_siblings_per_physical, ' ');
    int32_t num_cpu_siblings_per_physical = atoi(s_cpu_siblings_per_physical.c_str());
    return num_cpu_siblings_per_physical;
}

int32_t ResourceManager::cpu_threads() {
    int32_t siblings = cpu_siblings_per_physical();
    int32_t cores = cpu_cores_per_physical();
    return siblings / cores;
}

int32_t ResourceManager::mem_total() {
    std::string mem_total = run_shell("free  -g | grep \"Mem:\" | awk '{print $2}'");
    mem_total = string_util::rtrim(mem_total, '\n');
    return atoi(mem_total.c_str());
}

int32_t ResourceManager::mem_used() {
    std::string mem_used = run_shell("free  -g | grep \"Mem:\" | awk '{print $3}'");
    mem_used = string_util::rtrim(mem_used, '\n');
    return atoi(mem_used.c_str());
}

int32_t ResourceManager::mem_free() {
    std::string mem_free = run_shell("free  -g | grep \"Mem:\" | awk '{print $4}'");
    mem_free = string_util::rtrim(mem_free, '\n');
    return atoi(mem_free.c_str());
}

int32_t ResourceManager::mem_buff_cache() {
    std::string mem_buff_cache = run_shell("free  -g | grep \"Mem:\" | awk '{print $6}'");
    mem_buff_cache = string_util::rtrim(mem_buff_cache, '\n');
    return atoi(mem_buff_cache.c_str());
}

int32_t ResourceManager::disk_size(const std::string& path) {
    std::string cmd = "df -l -k " + path + " | tail -1 | awk '{print $2}'";
    std::string disk_size = run_shell(cmd.c_str());
    disk_size = string_util::rtrim(disk_size, '\n');
    return atoi(disk_size.c_str()) / 1024 / 1024;
}

int32_t ResourceManager::disk_used(const std::string& path) {
    std::string cmd = "df -l -k " + path + " | tail -1 | awk '{print $3}'";
    std::string disk_used = run_shell(cmd.c_str());
    disk_used = string_util::rtrim(disk_used, '\n');
    return atoi(disk_used.c_str()) / 1024 / 1024;
}

int32_t ResourceManager::disk_free(const std::string& path) {
    std::string cmd = "df -l -k " + path + " | tail -1 | awk '{print $4}'";
    std::string disk_free = run_shell(cmd.c_str());
    disk_free = string_util::rtrim(disk_free, '\n');
    return atoi(disk_free.c_str()) / 1024 / 1024;
}

std::string ResourceManager::disk_type(const std::string& path) {
    std::string cmd1 = "df -l -k " + path + " | tail -1 | awk '{print $1}' | awk -F\"/\" '{print $3}'";
    std::string disk = run_shell(cmd1.c_str());
    disk = string_util::rtrim(disk, '\n');
    std::string cmd2 = "lsblk -o name,rota | grep " + disk + " | awk '{if($2==\"1\")print \"HDD\"; else print \"SSD\"}'";
    std::string disk_type = run_shell(cmd2.c_str());
    disk_type = string_util::rtrim(disk_type, '\n');
    return disk_type;
}

int32_t ResourceManager::gpu_count() {
    std::string cmd = "lspci -nnv | grep NVIDIA |grep VGA | wc -l";
    std::string gpu = run_shell(cmd.c_str());
    return atoi(gpu.c_str());
}
