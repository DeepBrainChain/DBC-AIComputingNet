#include "TaskResourceManager.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "log/log.h"
#include "tinyxml2.h"

static DeviceCpu g_default_cpu;
static std::map<std::string, DeviceGpu> g_default_gpu;
static DeviceMem g_default_mem;
static std::map<int32_t, DeviceDisk> g_default_disk;

void TaskResourceManager::init(const std::vector<std::string> &tasks) {
    for (auto& id : tasks) {
        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;

        do {
            std::string url = "qemu+tcp://localhost:16509/system";

            connPtr = virConnectOpen(url.c_str());
            if (nullptr == connPtr) {
                LOG_ERROR << "virConnectOpen error: " << url;
                break;
            }

            domainPtr = virDomainLookupByName(connPtr, id.c_str());
            if (nullptr == domainPtr) {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << "virDomainLookupByName " << id << " error: " << error->message;
                virFreeError(error);
                break;
            }

            char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE);
            if (pContent != nullptr) {
                tinyxml2::XMLDocument doc;
                tinyxml2::XMLError err = doc.Parse(pContent);
                if (err != tinyxml2::XML_SUCCESS) {
                    LOG_ERROR << "parse domain xml error: " << err << ", domain: " << id;
                    break;
                }

                tinyxml2::XMLElement* root = doc.RootElement();
                // cpu
                tinyxml2::XMLElement* ele_root = root->FirstChildElement("cpu");
                if (ele_root != nullptr) {
                    tinyxml2::XMLElement* ele_topology = ele_root->FirstChildElement("topology");
                    if (ele_topology != nullptr) {
                        DeviceCpu cpuinfo;
                        cpuinfo.sockets = ele_topology->IntAttribute("sockets");
                        cpuinfo.cores_per_socket = ele_topology->IntAttribute("cores");
                        cpuinfo.threads_per_core = ele_topology->IntAttribute("threads");
                        m_mpTaskCpu[id] = cpuinfo;
                    }
                }
                // mem
                tinyxml2::XMLElement* ele_memory = root->FirstChildElement("memory");
                if (ele_memory != nullptr) {
                    const char* str_memory = ele_memory->GetText();
                    DeviceMem meminfo;
                    meminfo.total = atol(str_memory);
                    meminfo.available = atol(str_memory);
                    m_mpTaskMem[id] = meminfo;
                }
                //gpu
                tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
                if (ele_devices != nullptr) {
                    tinyxml2::XMLElement* ele_hostdev = ele_devices->FirstChildElement("hostdev");
                    std::string cur_gpu_id;
                    while (ele_hostdev != nullptr) {
                        tinyxml2::XMLElement* ele_source = ele_hostdev->FirstChildElement("source");
                        if (ele_source != nullptr) {
                            tinyxml2::XMLElement* ele_address = ele_source->FirstChildElement("address");
                            if (ele_address != nullptr) {
                                std::string str_bus = ele_address->Attribute("bus");
                                std::string str_slot = ele_address->Attribute("slot");
                                std::string str_function = ele_address->Attribute("function");

                                auto pos1 = str_bus.find("0x");
                                if (pos1 != std::string::npos)
                                    str_bus = str_bus.substr(2);

                                auto pos2 = str_slot.find("0x");
                                if (pos2 != std::string::npos)
                                    str_slot = str_slot.substr(2);

                                auto pos3 = str_function.find("0x");
                                if (pos3 != std::string::npos)
                                    str_function = str_function.substr(2);

                                std::string device_id = str_bus + ":" + str_slot + "." + str_function;

                                if (atoi(str_function.c_str()) == 0) {
                                    DeviceGpu gpuinfo;
                                    gpuinfo.id = device_id;
                                    m_mpTaskGpu[id][device_id] = gpuinfo;
                                    cur_gpu_id = device_id;
                                }

                                DeviceGpu& itr_gpu = m_mpTaskGpu[id][cur_gpu_id];
                                itr_gpu.devices.push_back(device_id);
                            }
                        }

                        ele_hostdev = ele_hostdev->NextSiblingElement("hostdev");
                    }
                }
                // disk
                if (ele_devices != nullptr) {
                    tinyxml2::XMLElement* ele_disk = ele_devices->FirstChildElement("disk");
                    while (ele_disk != nullptr) {
                        tinyxml2::XMLElement* ele_source = ele_disk->FirstChildElement("source");
                        std::string disk_file = ele_source->Attribute("file");
                        auto pos = disk_file.rfind('/');
                        if (pos != std::string::npos) {
                            disk_file = disk_file.substr(pos + 1);
                            auto pos1 = disk_file.find('.');
                            std::string ext = disk_file.substr(pos1 + 1);
                            std::string fname = disk_file.substr(0, pos1);
                            if (ext == "qcow2") {
                                std::vector<std::string> vec;
                                util::split(fname, "_", vec);
                                if (vec.size() >= 3) {
                                    int nindex = atoi(vec[1].c_str());
                                    DeviceDisk diskinfo;
                                    tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
                                    if (ele_target != nullptr) {
                                        std::string vdevice = ele_target->Attribute("dev");
                                        virDomainBlockInfo blockinfo;
                                        if (virDomainGetBlockInfo(domainPtr, vdevice.c_str(), &blockinfo, 0) >= 0) {
                                            std::string str_type = disk_type("/data");
                                            diskinfo.type = (str_type == "SSD" ? DiskType::DT_SSD : DiskType::DT_HDD);
                                            int64_t disk_size = blockinfo.capacity;
                                            diskinfo.total = disk_size / 1024 / 1024;
                                            diskinfo.available = disk_size / 1024 / 1024;
                                            m_mpTaskDisk[id][nindex] = diskinfo;
                                        }
                                    }
                                }
                            }
                        }

                        ele_disk = ele_disk->NextSiblingElement("disk");
                    }
                }

                free(pContent);
            }
        } while(0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }
    }
}

const DeviceCpu & TaskResourceManager::GetTaskCpu(const std::string &task_id) const {
    auto it = m_mpTaskCpu.find(task_id);
    if (it == m_mpTaskCpu.end()) {
        return g_default_cpu;
    } else {
        return it->second;
    }
}

const std::map<std::string, DeviceGpu> & TaskResourceManager::GetTaskGpu(const std::string &task_id) const {
    auto it = m_mpTaskGpu.find(task_id);
    if (it == m_mpTaskGpu.end()) {
        return g_default_gpu;
    } else {
        return it->second;
    }
}

const DeviceMem & TaskResourceManager::GetTaskMem(const std::string &task_id) const {
    auto it = m_mpTaskMem.find(task_id);
    if (it == m_mpTaskMem.end()) {
        return g_default_mem;
    } else {
        return it->second;
    }
}

const std::map<int32_t, DeviceDisk> & TaskResourceManager::GetTaskDisk(const std::string &task_id) const {
    auto it = m_mpTaskDisk.find(task_id);
    if (it == m_mpTaskDisk.end()) {
        return g_default_disk;
    } else {
        return it->second;
    }
}

void TaskResourceManager::AddTaskCpu(const std::string &task_id, const DeviceCpu &cpu) {
    m_mpTaskCpu[task_id] = cpu;
}

void TaskResourceManager::AddTaskMem(const std::string &task_id, const DeviceMem &mem) {
    m_mpTaskMem[task_id] = mem;
}

void TaskResourceManager::AddTaskGpu(const std::string &task_id, const std::map<std::string, DeviceGpu> &gpus) {
    m_mpTaskGpu[task_id] = gpus;
}

void TaskResourceManager::AddTaskDisk(const std::string &task_id, const std::map<int32_t, DeviceDisk> &disks) {
    m_mpTaskDisk[task_id] = disks;
}

void TaskResourceManager::Clear(const std::string &task_id) {
    m_mpTaskCpu.erase(task_id);
    m_mpTaskMem.erase(task_id);
    m_mpTaskGpu.erase(task_id);
    m_mpTaskDisk.erase(task_id);
}

std::string TaskResourceManager::disk_type(const std::string& path) {
    std::string cmd1 = "df -l -m " + path + " | tail -1 | awk '{print $1}' | awk -F\"/\" '{print $3}'";
    std::string disk = run_shell(cmd1.c_str());
    disk = util::rtrim(disk, '\n');
    std::string cmd2 = "lsblk -o name,rota | grep " + disk + " | awk '{if($2==\"1\")print \"HDD\"; else print \"SSD\"}'";
    std::string disk_type = run_shell(cmd2.c_str());
    disk_type = util::rtrim(disk_type, '\n');
    return disk_type;
}

void TaskResourceManager::print_cpu(const std::string& task_id) {
    const DeviceCpu& cpuinfo = m_mpTaskCpu[task_id];
    std::cout << "cpu: " << std::endl
              << "sockets: " << cpuinfo.sockets << ", cores_per_socket: " << cpuinfo.cores_per_socket
              << ", threads_per_core: " << cpuinfo.threads_per_core << std::endl;
}

void TaskResourceManager::print_mem(const std::string& task_id) {
    const DeviceMem& meminfo = m_mpTaskMem[task_id];
    std::cout << "memory: " << std::endl
              << "total: " << (meminfo.total / 1024) << "MB"
              << ", available: " << (meminfo.available / 1024) << "MB" << std::endl;
}

void TaskResourceManager::print_gpu(const std::string& task_id) {
    const std::map<std::string, DeviceGpu>& mp = m_mpTaskGpu[task_id];
    std::cout << "gpu:" << std::endl;
    for (auto &it : mp) {
        std::cout << it.first << std::endl;
        for (auto &device : it.second.devices) {
            std::cout << "  " << device << std::endl;
        }
    }
}

void TaskResourceManager::print_disk(const std::string &task_id) {
    const std::map<int32_t, DeviceDisk>& mp = m_mpTaskDisk[task_id];
    std::cout << "disk:" << std::endl;
    for (auto &it : mp) {
        std::cout << it.first << ", " << (it.second.type == DiskType::DT_SSD ? "SSD" : "HDD") << ", " << it.second.total << "MB" << std::endl;
    }
}

