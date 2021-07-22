#include "SystemResourceManager.h"

void SystemResourceManager::Init() {
    init_os();
    init_cpu();
    init_gpu();
    init_mem();
    init_disk();
}

void SystemResourceManager::init_os() {
    m_os.name = os_name();
    m_os.version = os_version();
}

std::string SystemResourceManager::os_name() {
    std::string os_name = run_shell("cat /etc/os-release | grep VERSION | awk -F '=' '{print $2}'| tr -d '\"'");
    os_name = util::rtrim(os_name, '\n');
    return os_name;
}

std::string SystemResourceManager::os_version() {
    std::string os_version = run_shell("uname -o -r");
    os_version = util::rtrim(os_version, '\n');
    return os_version;
}

void SystemResourceManager::init_cpu() {
    m_cpu.sockets = physical_cpu();
    m_cpu.cores_per_socket = cpu_cores_per_physical();
    m_cpu.threads_per_core = cpu_siblings_per_physical() / cpu_cores_per_physical();
}

std::string SystemResourceManager::cpu_type() {
    std::string cpu_type = run_shell("cat /proc/cpuinfo |grep \"model name\" | awk -F \":\" '{print $2}'| uniq | awk -F \"@\" '{print $1}' |xargs");
    cpu_type = util::rtrim(cpu_type, '\n');
    return cpu_type;
}

int32_t SystemResourceManager::cpu_cores() {
    std::string cpu_cores = run_shell("cat /proc/cpuinfo |grep \"model name\" | awk -F \":\" '{print $2}'| wc -l");
    cpu_cores = util::rtrim(cpu_cores, '\n');
    return atoi(cpu_cores.c_str());
}

int32_t SystemResourceManager::physical_cpu() {
    std::string s_physical_cpu = run_shell("cat /proc/cpuinfo| grep \"physical id\"| sort| uniq| wc -l");
    s_physical_cpu = util::rtrim(s_physical_cpu, '\n');
    int32_t num_physical_cpu = atoi(s_physical_cpu.c_str());
    return num_physical_cpu;
}

int32_t SystemResourceManager::cpu_cores_per_physical() {
    std::string s_cpu_cores_per_physical = run_shell("cat /proc/cpuinfo| grep \"cpu cores\"| uniq");
    s_cpu_cores_per_physical = util::rtrim(s_cpu_cores_per_physical, '\n');
    std::vector<std::string> vec1;
    util::split(s_cpu_cores_per_physical, ":", vec1);
    s_cpu_cores_per_physical = vec1[1];
    s_cpu_cores_per_physical = util::ltrim(s_cpu_cores_per_physical, ' ');
    int32_t num_cpu_cores_per_physical = atoi(s_cpu_cores_per_physical.c_str());
    return num_cpu_cores_per_physical;
}

int32_t SystemResourceManager::cpu_siblings_per_physical() {
    std::string s_cpu_siblings_per_physical = run_shell("cat /proc/cpuinfo | grep 'siblings' | uniq");
    s_cpu_siblings_per_physical = util::rtrim(s_cpu_siblings_per_physical, '\n');
    std::vector<std::string> vec2;
    util::split(s_cpu_siblings_per_physical, ":", vec2);
    s_cpu_siblings_per_physical = vec2[1];
    s_cpu_siblings_per_physical = util::ltrim(s_cpu_siblings_per_physical, ' ');
    int32_t num_cpu_siblings_per_physical = atoi(s_cpu_siblings_per_physical.c_str());
    return num_cpu_siblings_per_physical;
}

int32_t SystemResourceManager::cpu_threads() {
    int32_t siblings = cpu_siblings_per_physical();
    int32_t cores = cpu_cores_per_physical();
    return siblings / cores;
}

void SystemResourceManager::init_gpu() {
    std::string cmd = "lspci -nnv |grep NVIDIA |awk '{print $2\",\"$1}' |tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    std::vector<std::string> vec;
    util::split(str, "|", vec);
    std::string cur_id;
    for (int i = 0; i < vec.size(); i++) {
        std::vector<std::string> vec2;
        util::split(vec[i], ",", vec2);
        if (vec2[0] == "VGA") {
            cur_id = vec2[1];
            m_gpu[cur_id].id = cur_id;
        }
        m_gpu[cur_id].devices.push_back(vec2[1]);
    }
}

void SystemResourceManager::init_mem() {
    m_mem.total = mem_total();
    m_mem.available = mem_available();
}

int64_t SystemResourceManager::mem_total() {
    std::string mem_total = run_shell("free -m | grep \"Mem:\" | awk '{print $2}'");
    mem_total = util::rtrim(mem_total, '\n');
    return atoll(mem_total.c_str());
}

int64_t SystemResourceManager::mem_used() {
    std::string mem_used = run_shell("free -m | grep \"Mem:\" | awk '{print $3}'");
    mem_used = util::rtrim(mem_used, '\n');
    return atoll(mem_used.c_str());
}

int64_t SystemResourceManager::mem_free() {
    std::string mem_free = run_shell("free -m | grep \"Mem:\" | awk '{print $4}'");
    mem_free = util::rtrim(mem_free, '\n');
    return atoll(mem_free.c_str());
}

int64_t SystemResourceManager::mem_buff_cache() {
    std::string mem_buff_cache = run_shell("free -m | grep \"Mem:\" | awk '{print $6}'");
    mem_buff_cache = util::rtrim(mem_buff_cache, '\n');
    return atoll(mem_buff_cache.c_str());
}

int64_t SystemResourceManager::mem_available() {
    std::string mem_available = run_shell("free -m | grep \"Mem:\" | awk '{print $7}'");
    mem_available = util::rtrim(mem_available, '\n');
    return atoll(mem_available.c_str());
}

void SystemResourceManager::init_disk() {
    m_disk.type = disk_type() == "SSD" ? DiskType::DT_SSD : DiskType::DT_HDD;
    m_disk.total = disk_total();
    m_disk.available = disk_available();
}

std::string SystemResourceManager::disk_type(const std::string& path) {
    std::string cmd1 = "df -l -m " + path + " | tail -1 | awk '{print $1}' | awk -F\"/\" '{print $3}'";
    std::string disk = run_shell(cmd1.c_str());
    disk = util::rtrim(disk, '\n');
    std::string cmd2 = "lsblk -o name,rota | grep " + disk + " | awk '{if($2==\"1\")print \"HDD\"; else print \"SSD\"}'";
    std::string disk_type = run_shell(cmd2.c_str());
    disk_type = util::rtrim(disk_type, '\n');
    return disk_type;
}

int64_t SystemResourceManager::disk_total(const std::string& path) {
    std::string cmd = "df -l -m " + path + " | tail -1 | awk '{print $2}'";
    std::string disk_size = run_shell(cmd.c_str());
    disk_size = util::rtrim(disk_size, '\n');
    return atoll(disk_size.c_str());
}

int64_t SystemResourceManager::disk_used(const std::string& path) {
    std::string cmd = "df -l -m " + path + " | tail -1 | awk '{print $3}'";
    std::string disk_used = run_shell(cmd.c_str());
    disk_used = util::rtrim(disk_used, '\n');
    return atoll(disk_used.c_str());
}

int64_t SystemResourceManager::disk_available(const std::string& path) {
    std::string cmd = "df -l -m " + path + " | tail -1 | awk '{print $4}'";
    std::string disk_available = run_shell(cmd.c_str());
    disk_available = util::rtrim(disk_available, '\n');
    return atoll(disk_available.c_str());
}

void SystemResourceManager::print_os() {
    std::cout << "os:" << std::endl
              << "name: " << m_os.name << std::endl
              << "version: " << m_os.version << std::endl;
}

void SystemResourceManager::print_cpu() {
    std::cout << "cpu:" << std::endl;
    std::cout << "total_cores: " << m_cpu.total_cores() << std::endl
              << "sockets: " << m_cpu.sockets << ", cores_per_socket: " << m_cpu.cores_per_socket << ", threads_per_core: " << m_cpu.threads_per_core << std::endl;
}

void SystemResourceManager::print_gpu() {
    std::cout << "gpu:" << std::endl;
    for (auto& it : m_gpu) {
        std::cout << it.first << " : " << "bus=" << DeviceGpu::parse_bus(it.first) << ","
                                       << "slot=" << DeviceGpu::parse_slot(it.first) << ","
                                       << "function=" << DeviceGpu::parse_function(it.first) << std::endl;
        for (auto& it2 : it.second.devices) {
            std::cout << "  " << it2 << std::endl;
        }
    }
}

void SystemResourceManager::print_mem() {
    std::cout << "mem:" << std::endl
              << "total: " << size_to_string(m_mem.total, 1024 * 1024)
              << ", available: " << size_to_string(m_mem.available, 1024 * 1024) << std::endl;
}

void SystemResourceManager::print_disk() {
    std::cout << "disk:" << std::endl
              << "type: " << (m_disk.type == DiskType::DT_SSD ? "SSD" : "HDD")
              << ", total: " << size_to_string(m_disk.total, 1024 * 1024)
              << ", available: " << size_to_string(m_disk.available, 1024 * 1024) << std::endl;
}
