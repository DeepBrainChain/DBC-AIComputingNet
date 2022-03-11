#include "TaskResourceManager.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "log/log.h"
#include "tinyxml2.h"

int32_t TaskResource::total_cores() const {
    return cpu_sockets * cpu_cores * cpu_threads;
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

bool TaskResourceManager::init(const std::vector<std::string> &taskids) {
    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        LOG_ERROR << "virConnectOpen error: " << url;
        return false;
    }

    for (auto& id : taskids) {
        virDomainPtr domainPtr = nullptr;

        do {
            domainPtr = virDomainLookupByName(connPtr, id.c_str());
            if (nullptr == domainPtr) {
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
                std::shared_ptr<TaskResource> task_resource = std::make_shared<TaskResource>();
                // cpu
                tinyxml2::XMLElement* ele_cpu = root->FirstChildElement("cpu");
                if (ele_cpu != nullptr) {
                    tinyxml2::XMLElement* ele_topology = ele_cpu->FirstChildElement("topology");
                    if (ele_topology != nullptr) {
                        task_resource->cpu_sockets = ele_topology->IntAttribute("sockets");
                        task_resource->cpu_cores = ele_topology->IntAttribute("cores");
                        task_resource->cpu_threads = ele_topology->IntAttribute("threads");
                    }
                }
                // mem
                tinyxml2::XMLElement* ele_memory = root->FirstChildElement("memory");
                if (ele_memory != nullptr) {
                    const char* str_memory = ele_memory->GetText();
                    task_resource->mem_size = atol(str_memory);
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

                                task_resource->gpus[cur_gpu_id].push_back(device_id);
                            }
                        }

                        ele_hostdev = ele_hostdev->NextSiblingElement("hostdev");
                    }
                }
                // disk
                if (ele_devices != nullptr) {
                    tinyxml2::XMLElement* ele_disk = ele_devices->FirstChildElement("disk");
                    int disk_count = 0;
                    while (ele_disk != nullptr) {
                        tinyxml2::XMLElement* ele_source = ele_disk->FirstChildElement("source");
                        std::string disk_file = ele_source->Attribute("file");
                     
						if (disk_count == 0) {
							tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
							if (ele_target != nullptr) {
								std::string vdevice = ele_target->Attribute("dev");
								virDomainBlockInfo blockinfo;
								if (virDomainGetBlockInfo(domainPtr, vdevice.c_str(), &blockinfo, 0) >= 0) {
									int64_t disk_size = blockinfo.capacity;
									task_resource->disk_system_size = disk_size / 1024L;
								}
							}
						}
						else {
							int nindex = disk_count;
							tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
							if (ele_target != nullptr) {
								std::string vdevice = ele_target->Attribute("dev");
								virDomainBlockInfo blockinfo;
								if (virDomainGetBlockInfo(domainPtr, vdevice.c_str(), &blockinfo, 0) >= 0) {
									int64_t disk_size = blockinfo.capacity;
									task_resource->disks[nindex] = disk_size / 1024L;
								}
							}
						}
                        
                        ++disk_count;
                        ele_disk = ele_disk->NextSiblingElement("disk");
                    }
                }
                // vnc
                if (ele_devices != nullptr) {
                    tinyxml2::XMLElement* ele_graphics = ele_devices->FirstChildElement("graphics");
                    while (ele_graphics != nullptr) {
                        std::string graphics_type = ele_graphics->Attribute("type");
                        if (graphics_type == "vnc") {
                            std::string vnc_port = ele_graphics->Attribute("port");
                            std::string vnc_pwd = ele_graphics->Attribute("passwd");
                            task_resource->vnc_port = vnc_port.empty() ? -1 : atoi(vnc_port.c_str());
                            task_resource->vnc_password = vnc_pwd;
                        }
                        ele_graphics = ele_graphics->NextSiblingElement("graphics");
                    }
                }

                m_task_resource[id] = task_resource;
                free(pContent);
            }
        } while(0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }
    }

    if (nullptr != connPtr) {
        virConnectClose(connPtr);
    }

    return true;
}

std::shared_ptr<TaskResource> TaskResourceManager::getTaskResource(const std::string &task_id) const {
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_task_resource.find(task_id);
    if (it != m_task_resource.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

void TaskResourceManager::addTaskResource(const std::string &task_id, const std::shared_ptr<TaskResource> &resource) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_resource[task_id] = resource;
}

void TaskResourceManager::delTaskResource(const std::string &task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_resource.erase(task_id);
}
