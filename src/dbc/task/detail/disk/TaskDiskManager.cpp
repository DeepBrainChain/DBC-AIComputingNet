#include "TaskDiskManager.h"
#include "task/vm/vm_client.h"
#include "task/detail/info/TaskInfoManager.h"
#include "util/system_info.h"
#include "task/TaskManager.h"
#include "log/log.h"
#include "task/detail/image/ImageManager.h"
#include "tinyxml2.h"

FResult TaskDiskManager::init(const std::vector<std::string>& task_ids) {
    // disk
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
        tinyxml2::XMLElement* devices_node = root->FirstChildElement("devices");
        if (!devices_node) {
            LOG_ERROR << "not found devices node";
            continue;
        }
        tinyxml2::XMLElement* disk_node = devices_node->FirstChildElement("disk");
        while (disk_node) {
            std::shared_ptr<DiskInfo> diskinfo = std::make_shared<DiskInfo>();
            tinyxml2::XMLElement* disk_source_node = disk_node->FirstChildElement("source");
            if (disk_source_node) {
                diskinfo->setSourceFile(disk_source_node->Attribute("file"));
            }
            tinyxml2::XMLElement* disk_target_node = disk_node->FirstChildElement("target");
            if (disk_target_node) {
                diskinfo->setName(disk_target_node->Attribute("dev"));
            }
            diskinfo->setVirtualSize(VmClient::instance().GetDiskVirtualSize(task_id, diskinfo->getName()));

            auto& disks = m_task_disks[task_id];
            disks.insert({ diskinfo->getName(), diskinfo});
            
            disk_node = disk_node->NextSiblingElement("disk");
        }
    }

    // snapshot
	bool ret = m_snapshot_db.init_db(EnvManager::instance().get_db_path(), "snapshot.db");
	if (!ret) {
		return FResult(ERR_ERROR, "init snapshot_db failed");
	}

    std::map<std::string, std::shared_ptr<dbc::db_snapshot_info>> mp_snapshots;
	m_snapshot_db.load_datas(mp_snapshots);

    for (auto& iter : mp_snapshots) {
        std::shared_ptr<SnapshotInfo> ptr = std::make_shared<SnapshotInfo>();
        ptr->setTaskId(iter.first);
        ptr->m_db_info = iter.second;
        for (auto& it_snapshot : iter.second->snapshots) {
            ptr->setSnapshotStatus(it_snapshot.name, SnapshotStatus::SS_None);
        }
        m_task_snapshots.insert({ ptr->getTaskId(), ptr });
    }

    return FResultOk;
}

void TaskDiskManager::addDiskInfo(const std::string& task_id, const std::shared_ptr<DiskInfo>& diskinfo) {
    RwMutex::WriteLock wlock(m_disk_mtx);
    m_task_disks[task_id].insert({ diskinfo->getName(), diskinfo});
}

std::shared_ptr<DiskInfo> TaskDiskManager::getDiskInfo(const std::string& task_id, const std::string& disk_name) {
	RwMutex::ReadLock rlock(m_disk_mtx);
	auto iter = m_task_disks.find(task_id);
	if (iter != m_task_disks.end()) {
		auto& mpdisks = iter->second;
		auto iter_disk = mpdisks.find(disk_name);
		if (iter_disk != mpdisks.end()) {
            return iter_disk->second;
		}
	}

    return nullptr;
}

bool TaskDiskManager::isExistDisk(const std::string& task_id, const std::string& disk_name) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        auto& mpdisks = iter->second;
        auto iter_disk = mpdisks.find(disk_name);
        if (iter_disk != mpdisks.end()) {
            return true;
        }
    }

    return false;
}

void TaskDiskManager::delDisks(const std::string& task_id) {
	std::map<std::string, std::shared_ptr<DiskInfo> > disks;
	this->listDisks(task_id, disks);

	do {
		RwMutex::WriteLock wlock(m_disk_mtx);
		m_task_disks.erase(task_id);
	} while (0);

	for (auto& iter : disks) {
		std::string source_file = iter.second->getSourceFile();
		if (bfs::is_regular_file(source_file)) {
			bfs::remove(source_file.c_str());
			TASK_LOG_INFO(task_id, "remove disk file: " + source_file);
		}
	}
}

void TaskDiskManager::listDisks(const std::string& task_id, std::map<std::string, std::shared_ptr<DiskInfo> >& disks) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        for (auto& it : iter->second) {
            disks.insert({ it.first, it.second });
        }
    }
}

FResult TaskDiskManager::resizeDisk(const std::string& task_id, const std::string& disk_name, int64_t size_k) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        auto iter_disks = m_task_disks[task_id];
        auto iter_disk = iter_disks.find(disk_name);
        if (iter_disk != iter_disks.end()) {
            if (size_k <= 0) {
                return FResult(ERR_ERROR, "size is invalid (usage: size > 0)");
            }

            virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
            if (vm_status != VIR_DOMAIN_SHUTOFF) {
                return FResult(ERR_ERROR, "please close task first and try again");
            }

            auto diskinfo = iter_disks[disk_name];

            auto ev = std::make_shared<ResizeDiskEvent>(task_id);
            ev->disk_name = disk_name;
            ev->size_k = size_k;
            ev->source_file = diskinfo->getSourceFile();
            TaskMgr::instance().pushTaskEvent(ev);

            return FResultOk;
        }
        else {
            return FResult(ERR_ERROR, "task have no disk:" + disk_name);
        }
    }
    else {
        return FResult(ERR_ERROR, "task have no disk");
    }
}

FResult TaskDiskManager::addNewDisk(const std::string& task_id, int64_t size_k, const std::string& mount_dir) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        if (size_k <= 0) {
            return FResult(ERR_ERROR, "size is invalid (usage: size > 0)");
        }

        if (mount_dir.empty() || !bfs::exists(mount_dir)) {
            return FResult(ERR_ERROR, "not exist mount directory: " + mount_dir);
        }

        virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
        if (vm_status != VIR_DOMAIN_SHUTOFF) {
            return FResult(ERR_ERROR, "please close task first and try again");
        }
         
        disk_info dinfo;
        SystemInfo::instance().GetDiskInfo(mount_dir, dinfo);
        if (dinfo.free <= 1024L * 1024L) { // 1MB
            return FResult(ERR_ERROR, "mount directory:" + mount_dir + " have no enough space");
        }

        auto ev = std::make_shared<AddDiskEvent>(task_id);
        ev->size_k = size_k;
        ev->mount_dir = mount_dir;
        TaskMgr::instance().pushTaskEvent(ev);

        return FResultOk;
    }
    else {
        return FResult(ERR_ERROR, "task have no disk");
    }
}

FResult TaskDiskManager::deleteDisk(const std::string& task_id, const std::string& disk_name) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        auto iter_disks = m_task_disks[task_id];
        auto iter_disk = iter_disks.find(disk_name);
        if (iter_disk != iter_disks.end()) {
            virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
            if (vm_status != VIR_DOMAIN_SHUTOFF) {
                return FResult(ERR_ERROR, "please close task first and try again");
            }

            if (disk_name == "vda") {
                return FResult(ERR_ERROR, "can not allowed to delete disk:'vda'");
            }

            std::shared_ptr<DeleteDiskEvent> ev = std::make_shared<DeleteDiskEvent>(task_id);
            ev->disk_name = disk_name;
            TaskMgr::instance().pushTaskEvent(ev);

            return FResultOk;
        }
        else {
            return FResult(ERR_ERROR, "task have no disk:" + disk_name);
        }
    }
    else {
        return FResult(ERR_ERROR, "task no disk");
    }
}


void TaskDiskManager::delSnapshots(const std::string& task_id) {
	RwMutex::WriteLock wlock(m_snapshot_mtx);
	auto iter = m_task_snapshots.find(task_id);
    if (iter != m_task_snapshots.end()) {
        auto iter_snapshot = m_task_snapshots[task_id];
        m_task_snapshots.erase(task_id);
        iter_snapshot->deleteFromDB(m_snapshot_db);
        
        auto vecSnapshots = iter_snapshot->getSnapshots();
        for (auto& it : vecSnapshots) {
            bfs::remove(it.file);
        }
    }
}

void TaskDiskManager::listSnapshots(const std::string& task_id, std::vector<dbc::snapshot_info>& snapshots) {
    RwMutex::ReadLock rlock(m_snapshot_mtx);
    auto iter = m_task_snapshots.find(task_id);
    if (iter != m_task_snapshots.end()) {
        std::shared_ptr<SnapshotInfo> ptr = iter->second;
        snapshots = ptr->getSnapshots();
    }
}

bool TaskDiskManager::getSnapshot(const std::string& task_id, const std::string& snapshot_name, dbc::snapshot_info& snapshot) {
	RwMutex::ReadLock rlock(m_snapshot_mtx);
    bool found = false;
	auto iter = m_task_snapshots.find(task_id);
	if (iter != m_task_snapshots.end()) {
		std::shared_ptr<SnapshotInfo> ptr = iter->second;
		auto snapshots = ptr->getSnapshots();
        for (auto& it : snapshots) {
            if (it.name == snapshot_name) {
                snapshot = it;
                found = true;
                break;
            }
        }
    }
    
    return found;
}

std::shared_ptr<SnapshotInfo> TaskDiskManager::getSnapshotInfo(const std::string& task_id) {
    RwMutex::ReadLock rlock(m_snapshot_mtx);
    auto iter = m_task_snapshots.find(task_id);
    if (iter != m_task_snapshots.end()) {
        return iter->second;
    }
    else {
        return nullptr;
    }
}

FResult TaskDiskManager::createAndUploadSnapshot(const std::string& task_id, const std::string& snapshot_name, 
    const ImageServer& image_server, const std::string& desc) {
    RwMutex::ReadLock rlock(m_disk_mtx);
    auto iter = m_task_disks.find(task_id);
    if (iter != m_task_disks.end()) {
        auto iter_disks = m_task_disks[task_id];
        auto iter_disk = iter_disks.find("vda");
        if (iter_disk != iter_disks.end()) {
            virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
            if (vm_status != VIR_DOMAIN_SHUTOFF) {
                return FResult(ERR_ERROR, "please close task first and try again");
            }

            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
            if (taskinfo == nullptr || taskinfo->getVdaRootBackfile().empty()) {
                return FResult(ERR_ERROR, "not found vda's root backfile");
            }
            
            auto diskinfo = iter_disk->second;
			time_t tnow = time(nullptr);
			std::string snapshot_file = "/tmp/dbc_snapshots/snap_" + std::to_string(rand() % 100000) + "_" + util::time2str(tnow) +
				"_" + "vda.qcow2";

            // add snapshot
            do {
                RwMutex::WriteLock wlock(m_snapshot_mtx);
                auto iter_snapshotinfo = m_task_snapshots.find(task_id);
                if (iter_snapshotinfo == m_task_snapshots.end()) {
                    std::shared_ptr<SnapshotInfo> ptr = std::make_shared<SnapshotInfo>();
                    ptr->setTaskId(task_id);
                    ptr->addSnapshot(snapshot_name, snapshot_file, tnow, desc);
                    ptr->setSnapshotStatus(snapshot_name, SnapshotStatus::SS_Creating);
                    m_task_snapshots.insert({ task_id, ptr });
                    ptr->updateToDB(m_snapshot_db);
                }
                else {
                    iter_snapshotinfo->second->addSnapshot(snapshot_name, snapshot_file, tnow, desc);
                    iter_snapshotinfo->second->setSnapshotStatus(snapshot_name, SnapshotStatus::SS_Creating);
                    iter_snapshotinfo->second->updateToDB(m_snapshot_db);
                }
            } while (0);
            
            std::shared_ptr<CreateAndUploadSnapshotEvent> ev = std::make_shared<CreateAndUploadSnapshotEvent>(task_id);
            ev->snapshot_name = snapshot_name;
            ev->root_backfile = taskinfo->getVdaRootBackfile();
            ev->source_file = diskinfo->getSourceFile();
            ev->snapshot_file = snapshot_file;
            ev->create_time = tnow;
            ev->image_server = image_server;
            TaskMgr::instance().pushTaskEvent(ev);

            return FResultOk;
        }
        else {
            return FResult(ERR_ERROR, "task have no disk:'vda'");
        }
    }
    else {
        return FResult(ERR_ERROR, "task have no disk");
    }
}

float TaskDiskManager::uploadSnapshotProgress(const std::string& task_id, const std::string& snapshot_name) {
    RwMutex::ReadLock rlock(m_snapshot_mtx);
    auto iter = m_task_snapshots.find(task_id);
    if (iter != m_task_snapshots.end()) {
        bool found = false;
        dbc::snapshot_info snapshotinfo;
        auto vecSnapshots = iter->second->getSnapshots();
        for (auto& iter_snapshot : vecSnapshots) {
            if (iter_snapshot.name == snapshot_name) {
                snapshotinfo = iter_snapshot;
                found = true;
                break;
            }
        }

        if (found) {
			SnapshotStatus status = iter->second->getSnapshotStatus(snapshot_name);
            if (status == SnapshotStatus::SS_Creating) {
                return 0.f;
            }
            else if (status == SnapshotStatus::SS_Uploading) {
                return ImageManager::instance().uploadProgress(snapshotinfo.file);
            }
            else {
                return 1.f;
            }
        }
        else {
            return 0.f;
        } 
    }
    else {
        return 0.f;
    }
}

void TaskDiskManager::terminateUploadSnapshot(const std::string& task_id, const std::string& snapshot_name) {
    RwMutex::WriteLock wlock(m_snapshot_mtx);
	auto iter = m_task_snapshots.find(task_id);
    if (iter != m_task_snapshots.end()) {
		bool found = false;
		dbc::snapshot_info snapshotinfo;
		auto vecSnapshots = iter->second->getSnapshots();
		for (auto& iter_snapshot : vecSnapshots) {
			if (iter_snapshot.name == snapshot_name) {
				snapshotinfo = iter_snapshot;
				found = true;
				break;
			}
		}

        if (found) {
            ImageManager::instance().terminateUpload(snapshotinfo.file);
            if (bfs::exists(snapshotinfo.file))
                bfs::remove(snapshotinfo.file);
            m_task_snapshots[task_id]->delSnapshot(snapshot_name);
            m_task_snapshots[task_id]->updateToDB(m_snapshot_db);
        }
    }
}

void TaskDiskManager::process_resize_disk(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<ResizeDiskEvent> ev_resizedisk = std::dynamic_pointer_cast<ResizeDiskEvent>(ev);
    if (ev_resizedisk == nullptr) return;

    std::string cmd = "qemu-img resize " + ev_resizedisk->source_file + " +" + std::to_string(ev_resizedisk->size_k) + "K";
    TASK_LOG_INFO(ev->task_id, "resize disk cmd: " << cmd);
    run_shell(cmd);

    RwMutex::WriteLock wlock(m_disk_mtx);
    auto iter = m_task_disks.find(ev_resizedisk->task_id);
    if (iter != m_task_disks.end()) {
        auto iter_disks = m_task_disks[ev_resizedisk->task_id];
        auto iter_disk = iter_disks.find(ev_resizedisk->disk_name);
        if (iter_disk != iter_disks.end()) {
            auto diskinfo = iter_disks[ev_resizedisk->disk_name];
            diskinfo->setVirtualSize(VmClient::instance().GetDiskVirtualSize(ev_resizedisk->task_id, ev_resizedisk->disk_name));
            TASK_LOG_INFO(ev_resizedisk->task_id, "disk '" << ev_resizedisk->disk_name << "' after resize: " 
                << scale_size(diskinfo->getVirtualSize() / 1024L));
        }
        else {
            TASK_LOG_ERROR(ev_resizedisk->task_id, "task have no disk:" + ev_resizedisk->disk_name);
        }
    }
    else {
        TASK_LOG_ERROR(ev_resizedisk->task_id, "task have no disk");
    }
}

void TaskDiskManager::process_add_disk(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<AddDiskEvent> ev_adddisk = std::dynamic_pointer_cast<AddDiskEvent>(ev);
    if (ev_adddisk == nullptr) return;

    std::string disk_name = "vd";
    do {
        RwMutex::ReadLock rlock(m_disk_mtx);
        auto iter_disks = m_task_disks[ev_adddisk->task_id];
        auto iter_disk = iter_disks.rbegin();
        char ch = iter_disk->second->getName().back() + 1;
        disk_name.push_back(ch);
    } while (0);
    
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev_adddisk->task_id);
    std::string source_file = ev_adddisk->mount_dir + "/data_" + std::to_string(rand() % 100000) + "_" +
        util::time2str(time(nullptr)) + ".qcow2";
    std::string cmd = "qemu-img create -f qcow2 " + source_file + " " + std::to_string(ev_adddisk->size_k) + "K";
    TASK_LOG_INFO(ev->task_id, "add disk: " << source_file << ", size:" << scale_size(ev_adddisk->size_k));
    run_shell(cmd);

    FResult ret = VmClient::instance().AttachDisk(ev_adddisk->task_id, disk_name, source_file);

    if (ret.errcode != ERR_SUCCESS) {
        bfs::remove(source_file);
        TASK_LOG_ERROR(ev_adddisk->task_id, "vm attach disk failed:" + ret.errmsg);
        return;
    }

    RwMutex::WriteLock wlock(m_disk_mtx);
    std::shared_ptr<DiskInfo> diskinfo = std::make_shared<DiskInfo>();
    diskinfo->setName(disk_name);
    diskinfo->setSourceFile(source_file);
    diskinfo->setVirtualSize(ev_adddisk->size_k * 1024L);
    m_task_disks[ev_adddisk->task_id].insert({ disk_name, diskinfo });
}

void TaskDiskManager::process_delete_disk(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<DeleteDiskEvent> ev_deletedisk = std::dynamic_pointer_cast<DeleteDiskEvent>(ev);
    if (ev_deletedisk == nullptr) return;

    FResult ret = VmClient::instance().DetachDisk(ev_deletedisk->task_id, ev_deletedisk->disk_name);

    if (ret.errcode != ERR_SUCCESS) {
        TASK_LOG_ERROR(ev_deletedisk->task_id, "detach disk failed:" + ret.errmsg);
        return;
    }

    RwMutex::WriteLock wlock(m_disk_mtx);
    auto iter = m_task_disks.find(ev_deletedisk->task_id);
    if (iter != m_task_disks.end()) {
        auto& iter_disks = m_task_disks[ev_deletedisk->task_id];
        auto iter_disk = iter_disks.find(ev_deletedisk->disk_name);
        if (iter_disk != iter_disks.end()) {
            auto diskinfo = iter_disks[ev_deletedisk->disk_name];
            bfs::remove(diskinfo->getSourceFile());
            iter_disks.erase(ev_deletedisk->disk_name);
        }
        else {
            TASK_LOG_ERROR(ev_deletedisk->task_id, "task have no disk:" + ev_deletedisk->disk_name);
        }
    }
    else {
        TASK_LOG_ERROR(ev_deletedisk->task_id, "task have no disk");
    }
}

void TaskDiskManager::process_create_snapshot(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<CreateAndUploadSnapshotEvent> ev_createsnapshot = std::dynamic_pointer_cast<CreateAndUploadSnapshotEvent>(ev);
    if (ev_createsnapshot == nullptr) return;

    run_shell("mkdir -p /tmp/dbc_snapshots");

    std::string cmd = "qemu-img convert -f qcow2 -O qcow2 -B " + ev_createsnapshot->root_backfile 
        + " " + ev_createsnapshot->source_file + " " + ev_createsnapshot->snapshot_file;
    TASK_LOG_INFO(ev_createsnapshot->task_id, "create snapshot cmd:" + cmd);
    run_shell(cmd);
    
    std::string task_id = ev_createsnapshot->task_id;
    std::string snapshot_name = ev_createsnapshot->snapshot_name;
    FResult ret = ImageManager::instance().upload(ev_createsnapshot->snapshot_file, ev_createsnapshot->image_server, 
        [this, task_id, snapshot_name]() {
            RwMutex::WriteLock wlock(m_snapshot_mtx);
            m_task_snapshots[task_id]->setSnapshotStatus(snapshot_name, SnapshotStatus::SS_None);
        });
    if (ret.errcode == ERR_SUCCESS) {
		RwMutex::WriteLock wlock(m_snapshot_mtx);
		auto ptr = m_task_snapshots[task_id];
		ptr->setSnapshotStatus(snapshot_name, SnapshotStatus::SS_Uploading);
    }
    else {
        terminateUploadSnapshot(task_id, snapshot_name);
        TASK_LOG_ERROR(ev_createsnapshot->task_id, "upload snapshot:" + ev_createsnapshot->snapshot_name
            + " file:" + ev_createsnapshot->snapshot_file + " failed");
    }
}
