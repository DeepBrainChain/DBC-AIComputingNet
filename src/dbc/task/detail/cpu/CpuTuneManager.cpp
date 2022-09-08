#include "CpuTuneManager.h"
#include "log/log.h"
#include "task/vm/vm_client.h"
#include "tinyxml2.h"

// 统计一个数的二进制中1的个数
static inline unsigned int CountBit1(int num) {
    unsigned int count = 0;
    while (num) {
        num &= (num -1);
        ++count;
    }
    return count;
}

CpuTuneManager::CpuTuneManager() = default;

CpuTuneManager::~CpuTuneManager() = default;

FResult CpuTuneManager::Init() {
    std::string xml = VmClient::instance().GetCapabilities();

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError err = doc.Parse(xml.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        return FResult(ERR_ERROR, "parse host capabilities xml failed");
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    // host
    tinyxml2::XMLElement* ele_host = root->FirstChildElement("host");
    if (!ele_host) {
        return FResult(ERR_ERROR, "can not find host node in capabilities");
    }

    // host -> cpu
    tinyxml2::XMLElement* ele_host_cpu = ele_host->FirstChildElement("cpu");
    if (!ele_host_cpu) {
        return FResult(ERR_ERROR, "can not find host cpu node in capabilities");
    }

    // host -> cpu -> topology
    tinyxml2::XMLElement* ele_cpu_topology = ele_host_cpu->FirstChildElement("topology");
    if (!ele_cpu_topology) {
        return FResult(ERR_ERROR, "can not find host cpu topology in capabilities");
    }
    is_hyper_threading_ = ele_cpu_topology->UnsignedAttribute("threads") == 2;

    // host -> topology
    tinyxml2::XMLElement* ele_host_topology = ele_host->FirstChildElement("topology");
    if (!ele_host_topology) {
        return FResult(ERR_ERROR, "can not find host topology node in capabilities");
    }

    // host -> topology -> cells
    tinyxml2::XMLElement* ele_topology_cells = ele_host_topology->FirstChildElement("cells");
    if (!ele_topology_cells) {
        return FResult(ERR_ERROR, "can not find host topology cells in capabilities");
    }

    // host -> topology -> cells -> cell
    tinyxml2::XMLElement* ele_numa = ele_topology_cells->FirstChildElement("cell");
    while (ele_numa) {
        unsigned int numa_id = ele_numa->UnsignedAttribute("id");
        CpuTune::NumaNode numa_node;
        tinyxml2::XMLElement* ele_cpus = ele_numa->FirstChildElement("cpus");
        if (ele_cpus) {
            tinyxml2::XMLElement* ele_cpu = ele_cpus->FirstChildElement("cpu");
            while (ele_cpu) {
                CpuTune::Cpu cpu;
                cpu.id = ele_cpu->UnsignedAttribute("id");
                cpu.socket_id = ele_cpu->UnsignedAttribute("socket_id");
                cpu.core_id = ele_cpu->UnsignedAttribute("core_id");
                // LOG_INFO << cpu.id << " " << cpu.socket_id << " " << cpu.core_id;
                std::string siblings = ele_cpu->Attribute("siblings");
                std::vector<std::string> split = util::split(siblings, ",");
                for (const auto& sibling : split) {
                    cpu.siblings.push_back(atoi(sibling.c_str()));
                    // LOG_INFO << atoi(sibling.c_str());
                }
                numa_node.cpus[cpu.id] = cpu;
                ele_cpu = ele_cpu->NextSiblingElement("cpu");
            }
        }
        if (numa_node.cpus.size() > 0)
            topology_.numas.insert(std::make_pair(numa_id, numa_node));
        ele_numa = ele_numa->NextSiblingElement("cell");
    }

    host_cpu_count_ = 0;
    for (const auto& numa : topology_.numas) {
        host_cpu_count_ += numa.second.cpus.size();
    }

    if (host_cpu_count_ == 0) {
        return FResult(ERR_ERROR, "parse host cpu topology error");
    }

    cpu_maplen_ = VIR_CPU_MAPLEN(host_cpu_count_);
    cpumap_.resize(cpu_maplen_);
    memset(&cpumap_[0], 0, cpu_maplen_);

    // LOG_INFO << "host cpu count: " << host_cpu_count_
    //     << ", and cpu maplen: " << cpu_maplen_;
    // for (const auto& ch : cpumap_) {
    //     LOG_INFO << std::bitset<8>(ch);
    // }

    return FResultOk;
}

void CpuTuneManager::Exit() {
    //
}

void CpuTuneManager::UseCpus(const std::vector<unsigned int>& cpus) {
    RwMutex::WriteLock wlock(mutex_);
    for (const auto& cpu : cpus) {
        if (cpu >= host_cpu_count_) continue;
        VIR_USE_CPU(&cpumap_[0], cpu);
    }
}

void CpuTuneManager::UnuseCpus(const std::vector<unsigned int>& cpus) {
    RwMutex::WriteLock wlock(mutex_);
    for (const auto& cpu : cpus) {
        if (cpu >= host_cpu_count_) continue;
        VIR_UNUSE_CPU(&cpumap_[0], cpu);
    }
}

int CpuTuneManager::CpuUsed(const unsigned int cpu) const {
    if (cpu >= host_cpu_count_) return -1;
    RwMutex::ReadLock rlock(mutex_);
    return VIR_CPU_USED(&cpumap_[0], cpu);
}

unsigned int CpuTuneManager::UsedCpuCount() const {
    RwMutex::ReadLock rlock(mutex_);
    unsigned int count = 0;
    for (const auto& ch : cpumap_) {
        count += CountBit1(ch);
    }
    return count;
}

bool CpuTuneManager::TunableCpus(unsigned int vcpus, std::vector<unsigned int>& cpus) {
    if (vcpus < 1) return false;
    if (vcpus + UsedCpuCount() > host_cpu_count_) return false;
    if (IsHyperThreading() && vcpus % 2 == 1) ++vcpus;
    for (const auto& numa : topology_.numas) {
        for (const auto& cpu : numa.second.cpus) {
            for (const auto& id : cpu.second.siblings) {
                if (CpuUsed(id)) {
                    break;
                } else if (vcpus > cpus.size()) {
                    auto itf = std::find(cpus.begin(), cpus.end(), id);
                    if (itf == cpus.end()) cpus.push_back(id);
                }
            }
            if (cpus.size() >= vcpus) break;
        }
        if (cpus.size() >= vcpus) break;
    }
    return true;
}

void CpuTuneManager::UpdateCpuMap(const std::vector<unsigned char>& cpumap) {
    if (cpumap.size() == cpumap_.size()) {
        RwMutex::WriteLock wlock(mutex_);
        std::copy(cpumap.begin(), cpumap.end(), cpumap_.begin());
    }
}
