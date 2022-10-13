#include "TaskGpuManager.h"

#include "log/log.h"
#include "task/detail/info/TaskInfoManager.h"
#include "task/detail/rent_order/RentOrderManager.h"
#include "task/vm/vm_client.h"
#include "tinyxml2.h"
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

        tinyxml2::XMLElement* ele_hostdev =
            ele_devices->FirstChildElement("hostdev");
        std::string cur_gpu_id;
        while (ele_hostdev != nullptr) {
            tinyxml2::XMLElement* ele_source =
                ele_hostdev->FirstChildElement("source");
            if (ele_source != nullptr) {
                tinyxml2::XMLElement* ele_address =
                    ele_source->FirstChildElement("address");
                if (ele_address != nullptr) {
                    std::string str_bus = ele_address->Attribute("bus");
                    std::string str_slot = ele_address->Attribute("slot");
                    std::string str_function =
                        ele_address->Attribute("function");

                    auto pos1 = str_bus.find("0x");
                    if (pos1 != std::string::npos) str_bus = str_bus.substr(2);

                    auto pos2 = str_slot.find("0x");
                    if (pos2 != std::string::npos)
                        str_slot = str_slot.substr(2);

                    auto pos3 = str_function.find("0x");
                    if (pos3 != std::string::npos)
                        str_function = str_function.substr(2);

                    std::string device_id =
                        str_bus + ":" + str_slot + "." + str_function;

                    auto& gpus = m_task_gpus[task_id];
                    if (atoi(str_function.c_str()) == 0) {
                        std::shared_ptr<GpuInfo> ptr =
                            std::make_shared<GpuInfo>();
                        ptr->setId(device_id);
                        gpus.insert({device_id, ptr});
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

void TaskGpuManager::add(const std::string& task_id,
                         const std::shared_ptr<GpuInfo>& gpu) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_gpus[task_id].insert({gpu->getId(), gpu});
}

void TaskGpuManager::del(const std::string& task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    m_task_gpus.erase(task_id);
}

void TaskGpuManager::del(const std::string& task_id,
                         const std::string& gpu_id) {
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
    } else {
        return 0;
    }
}

void TaskGpuManager::resetGpusByIndex(const std::string& task_id,
                                      const std::vector<int32_t>& gpu_index) {
    if (gpu_index.empty()) return;

    del(task_id);
    TASK_LOG_INFO(task_id, "begin to reset gpus by rent order");

    const std::map<std::string, gpu_info>& sys_gpus =
        SystemInfo::instance().GetGpuInfo();
    std::map<std::string, std::list<std::string>> ordered_gpus;
    int index = 0;
    for (const auto& it : sys_gpus) {
        auto ifind = std::find(gpu_index.begin(), gpu_index.end(), index);
        if (ifind != gpu_index.end()) {
            ordered_gpus[it.first] = it.second.devices;
            TASK_LOG_INFO(task_id, "add gpu bus id: " << it.first);
        }
        index++;
    }
    if (ordered_gpus.empty()) return;

    for (auto iter_gpu : ordered_gpus) {
        std::shared_ptr<GpuInfo> gpuinfo = std::make_shared<GpuInfo>();
        gpuinfo->setId(iter_gpu.first);
        for (auto& iter_device : iter_gpu.second) {
            gpuinfo->addDeviceId(iter_device);
        }
        add(task_id, gpuinfo);
    }
    TASK_LOG_INFO(task_id, "reset gpus by rent order successful");
}

FResult TaskGpuManager::checkXmlGpu(const std::shared_ptr<TaskInfo>& taskinfo,
                                    const std::string& rent_order) {
    bool need_redefine_vm = false;
    if (!taskinfo->getOrderId().empty() && !rent_order.empty() &&
        rent_order != taskinfo->getOrderId() &&
        RentOrderManager::instance().GetRentStatus(
            rent_order, taskinfo->getRenterWallet()) ==
            RentOrder::RentStatus::Renting &&
        RentOrderManager::instance().GetRentStatus(
            taskinfo->getOrderId(), taskinfo->getRenterWallet()) !=
            RentOrder::RentStatus::Renting) {
        auto order_gpu_index = RentOrderManager::instance().GetRentedGpuIndex(
            rent_order, taskinfo->getRenterWallet());
        if (!order_gpu_index.empty() &&
            order_gpu_index !=
                RentOrderManager::instance().GetRentedGpuIndex(
                    taskinfo->getOrderId(), taskinfo->getRenterWallet())) {
            need_redefine_vm = true;
            resetGpusByIndex(taskinfo->getTaskId(), order_gpu_index);
        }
        taskinfo->setOrderId(rent_order);
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_INFO(taskinfo->getTaskId(),
                      "rent order update to " << rent_order);
    }

    std::map<std::string, std::shared_ptr<GpuInfo>> task_gpus;
    do {
        RwMutex::ReadLock rlock(m_mtx);
        auto iter = m_task_gpus.find(taskinfo->getTaskId());
        if (iter == m_task_gpus.end()) {
            return FResult(ERR_ERROR, "task_id not exist");
        }

        task_gpus = iter->second;
    } while (0);

    const std::map<std::string, gpu_info>& sys_gpus =
        SystemInfo::instance().GetGpuInfo();

    std::list<std::shared_ptr<GpuInfo>> lost_gpus;

    for (auto& iter : task_gpus) {
        auto it = sys_gpus.find(iter.first);
        if (it == sys_gpus.end()) {
            // 检测到掉卡
            lost_gpus.push_back(iter.second);
        }
    }

    // 删除
    for (auto& iter : lost_gpus) {
        del(taskinfo->getTaskId(), iter->getId());
    }

    if (!lost_gpus.empty()) need_redefine_vm = true;

    // 更新xml
    if (need_redefine_vm) {
        VmClient::instance().RedefineDomain(taskinfo);
    }

    return FResultOk;
}