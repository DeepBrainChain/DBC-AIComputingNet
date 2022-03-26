#include "TaskSnapshotInfoManager.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "log/log.h"
#include "tinyxml2.h"

bool SnapshotManager::init(const std::vector<std::string>& taskids) {
    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        LOG_ERROR << "virConnectOpen error: " << url;
        return false;
    }

    for (const auto& id : taskids) {
        virDomainPtr domainPtr = nullptr;
        virDomainSnapshotPtr *snaps = nullptr;
        int snaps_count = 0;

        do {
            domainPtr = virDomainLookupByName(connPtr, id.c_str());
            if (nullptr == domainPtr) {
                break;
            }
            
            if ((snaps_count = virDomainListAllSnapshots(domainPtr, &snaps, 1 << 10)) < 0) {
                virErrorPtr err = virGetLastError();
                LOG_ERROR << "virDomainListAllSnapshots error: " << err ? err->message : "";
                if (err) virFreeError(err);
                break;
            }

            for (int i = 0; i < snaps_count; i++) {
                std::shared_ptr<dbc::snapshotInfo> info = std::make_shared<dbc::snapshotInfo>();
                info->__set_name(virDomainSnapshotGetName(snaps[i]));

                char* pContent = virDomainSnapshotGetXMLDesc(snaps[i], 0);
                if (pContent != nullptr) {
                    tinyxml2::XMLDocument doc;
                    tinyxml2::XMLError err = doc.Parse(pContent);
                    if (err == tinyxml2::XML_SUCCESS) {
                        tinyxml2::XMLElement* root = doc.RootElement();
                        tinyxml2::XMLElement *desc = root->FirstChildElement("description");
                        if (desc) {
                            info->__set_description(desc->GetText());
                        }
                        tinyxml2::XMLElement *state = root->FirstChildElement("state");
                        if (state) {
                            info->__set_state(state->GetText());
                        }
                        tinyxml2::XMLElement *creationTime = root->FirstChildElement("creationTime");
                        if (creationTime) {
                            info->__set_creationTime(atoll(creationTime->GetText()));
                        }
                        tinyxml2::XMLElement *disks = root->FirstChildElement("disks");
                        if (disks) {
                            std::vector<dbc::snapshotDiskInfo> vecDisk;
                            tinyxml2::XMLElement *disk = disks->FirstChildElement("disk");
                            while (disk) {
                                dbc::snapshotDiskInfo dsinfo;
                                dsinfo.__set_name(disk->Attribute("name"));
                                dsinfo.__set_snapshot(disk->Attribute("snapshot"));
                                tinyxml2::XMLElement *driver = disk->FirstChildElement("driver");
                                if (driver) {
                                    dsinfo.__set_driver_type(driver->Attribute("type"));
                                }
                                tinyxml2::XMLElement *source = disk->FirstChildElement("source");
                                if (source) {
                                    dsinfo.__set_source_file(source->Attribute("file"));
                                }
                                vecDisk.push_back(dsinfo);
                                disk = disk->NextSiblingElement("disk");
                            }
                            if (!vecDisk.empty()) {
                                info->__set_disks(vecDisk);
                            }
                        }
                        info->__set_error_code(0);
                        info->__set_error_message("");

                        m_task_snapshot[{id, info->name}] = info;
                    } else {
                        LOG_ERROR << "parse domain snapshot xml desc error: " << err << ", domain: " << id << ", snap: " << info->name;
                    }
                    free(pContent);
                } else {
                    LOG_ERROR << "snapshot:" << info->name << " virDomainSnapshotGetXMLDesc return nullptr";
                }
            }
        } while(0);

        if (snaps && snaps_count > 0) {
            for (int i = 0; i < snaps_count; i++) {
                virDomainSnapshotFree(snaps[i]);
            }
        }
        if (snaps) {
            free(snaps);
        }

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }
    }

    if (nullptr != connPtr) {
        virConnectClose(connPtr);
    }
    return true;
}

std::shared_ptr<dbc::snapshotInfo> SnapshotManager::getCreatingSnapshot(const std::string &task_id) const {
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_creating_snapshot.find(task_id);
    if (it != m_creating_snapshot.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

void SnapshotManager::addCreatingSnapshot(const std::string &task_id, const std::shared_ptr<dbc::snapshotInfo> &snapshot) {
    RwMutex::WriteLock wlock(m_mtx);
    m_creating_snapshot[task_id] = snapshot;
}

void SnapshotManager::delCreatingSnapshot(const std::string& task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    m_creating_snapshot.erase(task_id);
}

void SnapshotManager::updateCreatingSnapshot(const std::string& task_id, int32_t error_code, const std::string& error_message) {
    RwMutex::WriteLock wlock(m_mtx);
    auto it = m_creating_snapshot.find(task_id);
    if (it != m_creating_snapshot.end()) {
        it->second->__set_error_code(error_code);
        it->second->__set_error_message(error_message);
        it->second->__set_creationTime(time(nullptr));
    }
}

void SnapshotManager::listTaskSnapshot(const std::string& task_id, std::vector<std::shared_ptr<dbc::snapshotInfo>>& snaps, bool includeCreating) const {
    RwMutex::ReadLock rlock(m_mtx);
    for (const auto& iter : m_task_snapshot) {
        if (iter.first.task_id == task_id) {
            snaps.push_back(iter.second);
        }
    }
    if (includeCreating) {
        auto it = m_creating_snapshot.find(task_id);
        if (it != m_creating_snapshot.end()) {
            snaps.push_back(it->second);
        }
    }
}

int32_t SnapshotManager::getTaskSnapshotCount(const std::string& task_id) const {
    int32_t count = 0;
    RwMutex::ReadLock rlock(m_mtx);
    for (const auto& iter : m_task_snapshot) {
        if (iter.first.task_id == task_id) {
            ++count;
        }
    }
    return count;
}

std::shared_ptr<dbc::snapshotInfo> SnapshotManager::getTaskSnapshot(const std::string& task_id, const std::string& snapshot_name, bool includeCreating) const {
    RwMutex::ReadLock rlock(m_mtx);
    taskSnapshotIndex index = {task_id, snapshot_name};
    auto it = m_task_snapshot.find(index);
    if (it != m_task_snapshot.end()) {
        return it->second;
    }
    if (includeCreating) {
        auto it2 = m_creating_snapshot.find(task_id);
        if (it2 != m_creating_snapshot.end() && it2->second->name == snapshot_name) {
            return it2->second;
        }
    }
    return nullptr;
}

void SnapshotManager::addTaskSnapshot(const std::string& task_id, const std::shared_ptr<dbc::snapshotInfo>& snapshot) {
    RwMutex::WriteLock wlock(m_mtx);
    taskSnapshotIndex index = {task_id, snapshot->name};
    m_task_snapshot[index] = snapshot;
}

void SnapshotManager::delTaskSnapshot(const std::string &task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    m_creating_snapshot.erase(task_id);
    for (auto iter = m_task_snapshot.begin(); iter != m_task_snapshot.end();) {
        if (iter->first.task_id == task_id) {
            iter = m_task_snapshot.erase(iter);
        }
        else {
            iter++;
        }
    }
}
