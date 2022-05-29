#include "TaskGpuManager.h"
#include "tinyxml2.h"
#include "task/vm/vm_client.h"
#include "log/log.h"
#include "util/system_info.h"

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
                    
                    auto& gpus = m_task_gpus[task_id];
                    if (atoi(str_function.c_str()) == 0) {
                        std::shared_ptr<GpuInfo> ptr = std::make_shared<GpuInfo>();
                        ptr->setId(device_id);
                        gpus.insert({ device_id, ptr });
                        cur_gpu_id = device_id;
                    }
                    
                    gpus[cur_gpu_id]->addDeviceId(device_id);
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

void TaskGpuManager::del(const std::string& task_id, const std::string& gpu_id) {
    RwMutex::WriteLock wlock(m_mtx);
    auto iter = m_task_gpus.find(task_id);
    if (iter != m_task_gpus.end()) {
        iter->second.erase(gpu_id);
    }
}

int32_t TaskGpuManager::getTaskGpusCount(const std::string& task_id) {
    RwMutex::ReadLock rlock(m_mtx);
	auto iter = m_task_gpus.find(task_id);
	if (iter != m_task_gpus.end()) {
		return iter->second.size();
	}
	else {
        return 0;
	}
}

FResult TaskGpuManager::checkXmlGpu(const std::shared_ptr<TaskInfo>& taskinfo) {
    std::map<std::string, std::shared_ptr<GpuInfo>> task_gpus;
    do {
        RwMutex::ReadLock rlock(m_mtx);
        auto iter = m_task_gpus.find(taskinfo->getTaskId());
        if (iter == m_task_gpus.end()) {
            return FResult(ERR_ERROR, "task_id not exist");
        }

        task_gpus = iter->second;
    } while (0);

    const std::map<std::string, gpu_info>& sys_gpus = SystemInfo::instance().GetGpuInfo();

    std::list<std::shared_ptr<GpuInfo>> lost_gpus;

    for (auto& iter : task_gpus) {
        auto it = sys_gpus.find(iter.first);
        if (it == sys_gpus.end()) {
            // ¼ì²âµ½µô¿¨
            lost_gpus.push_back(iter.second);
        }
    }

    // É¾³ý
    for (auto& iter : lost_gpus) {
        del(taskinfo->getTaskId(), iter->getId());
    }

    // ¸üÐÂxml 
    if (!lost_gpus.empty()) {
        VmClient::instance().RedefineDomain(taskinfo);
    }

    return FResultOk;
}