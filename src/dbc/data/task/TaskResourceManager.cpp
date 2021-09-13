#include "TaskResourceManager.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "log/log.h"
#include "tinyxml2.h"

static TaskResource g_default_task_resource;

int32_t TaskResource::total_cores() const {
    return physical_cpu * physical_cores_per_cpu * threads_per_cpu;
}

std::string TaskResource::parse_gpu_device_bus(const std::string& id) {
    auto pos = id.find(':');
    if (pos != std::string::npos) {
        return id.substr(0, pos);
    } else {
        return "";
    }
}

std::string TaskResource::parse_gpu_device_slot(const std::string& id) {
    auto pos1 = id.find(':');
    auto pos2 = id.find('.');
    if (pos1 != std::string::npos && pos2 != std::string::npos) {
        return id.substr(pos1 + 1, pos2 - pos1 - 1);
    } else {
        return "";
    }
}

std::string TaskResource::parse_gpu_device_function(const std::string& id) {
    auto pos = id.find('.');
    if (pos != std::string::npos) {
        return id.substr(pos + 1);
    } else {
        return "";
    }
}

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
                TaskResource task_resource;
                // cpu
                tinyxml2::XMLElement* ele_root = root->FirstChildElement("cpu");
                if (ele_root != nullptr) {
                    tinyxml2::XMLElement* ele_topology = ele_root->FirstChildElement("topology");
                    if (ele_topology != nullptr) {
                        task_resource.physical_cpu = ele_topology->IntAttribute("sockets");
                        task_resource.physical_cores_per_cpu = ele_topology->IntAttribute("cores");
                        task_resource.threads_per_cpu = ele_topology->IntAttribute("threads");
                    }
                }
                // mem
                tinyxml2::XMLElement* ele_memory = root->FirstChildElement("memory");
                if (ele_memory != nullptr) {
                    const char* str_memory = ele_memory->GetText();
                    task_resource.mem_size = atol(str_memory);
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
                                    cur_gpu_id = device_id;
                                }

                                task_resource.gpus[cur_gpu_id].push_back(device_id);
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
                                    tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
                                    if (ele_target != nullptr) {
                                        std::string vdevice = ele_target->Attribute("dev");
                                        virDomainBlockInfo blockinfo;
                                        if (virDomainGetBlockInfo(domainPtr, vdevice.c_str(), &blockinfo, 0) >= 0) {
                                            int64_t disk_size = blockinfo.capacity;
                                            task_resource.disks_data[nindex] = disk_size / 1024 / 1024;
                                        }
                                    }
                                } else if (fname.find("ubuntu") != std::string::npos) {
                                    tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
                                    if (ele_target != nullptr) {
                                        std::string vdevice = ele_target->Attribute("dev");
                                        virDomainBlockInfo blockinfo;
                                        if (virDomainGetBlockInfo(domainPtr, vdevice.c_str(), &blockinfo, 0) >= 0) {
                                            int64_t disk_size = blockinfo.capacity;
                                            task_resource.disk_system_size = disk_size / 1024 / 1024;
                                        }
                                    }
                                }
                            }
                        }

                        ele_disk = ele_disk->NextSiblingElement("disk");
                    }
                }

                m_mpTaskResource[id] = task_resource;
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

const TaskResource & TaskResourceManager::GetTaskResource(const std::string &task_id) const {
    auto it = m_mpTaskResource.find(task_id);
    if (it == m_mpTaskResource.end()) {
        return g_default_task_resource;
    } else {
        return it->second;
    }
}

void TaskResourceManager::AddTaskResource(const std::string &task_id, const TaskResource &resource) {
    m_mpTaskResource[task_id] = resource;
}

void TaskResourceManager::Delete(const std::string &task_id) {
    m_mpTaskResource.erase(task_id);
}
