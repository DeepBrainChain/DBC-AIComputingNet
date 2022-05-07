#include "TaskGpuManager.h"
#include "tinyxml2.h"
#include "task/vm/vm_client.h"
#include "log/log.h"

FResult TaskGpuManager::init(const std::vector<std::string>& task_ids) {
    for (size_t i = 0; i < task_ids.size(); i++) {
        std::string task_id = task_ids[i];
        std::string strXml = VmClient::instance().GetDomainXML(task_id);

        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(strXml.c_str());
        if (err != tinyxml2::XML_SUCCESS) {
            LOG_ERROR << " parse xml desc failed";
            continue;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
        if (!ele_devices) {
            LOG_ERROR << "not found devices node";
            continue;
        }
         
        tinyxml2::XMLElement* ele_hostdev = ele_devices->FirstChildElement("hostdev");
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
                    
                    auto& gpus = m_task_gpus[task_id];
                    if (atoi(str_function.c_str()) == 0) {
                        std::shared_ptr<GpuInfo> ptr = std::make_shared<GpuInfo>();
                        ptr->setId(device_id);
                        gpus.insert({ device_id, ptr });
                    }
                    gpus[device_id]->addDeviceId(device_id);
                }
            }

            ele_hostdev = ele_hostdev->NextSiblingElement("hostdev");
        } 
    }

    return FResultOk;
}

void TaskGpuManager::add(const std::string& task_id, const std::shared_ptr<GpuInfo>& gpu) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_gpus[task_id].insert({ gpu->getId(), gpu });
}

void TaskGpuManager::del(const std::string& task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_gpus.erase(task_id);
}
