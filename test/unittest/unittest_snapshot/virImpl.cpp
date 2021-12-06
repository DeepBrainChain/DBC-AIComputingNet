#include "virImpl.h"
#include <iostream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include "tinyxml2.h"

#define __DEBUG__ 1

#ifdef __DEBUG__
#define DebugPrintf(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DebugPrintf(format, ...)
#endif

// enum virDomainState
static const char* arrayDomainState[] = {"no state", "running", "blocked", "paused", "shutdown", "shutoff", "crashed", "pmsuspended", "last"};
// enum virDomainEventType
static const char* arrayEventType[] = {"VIR_DOMAIN_EVENT_DEFINED", "VIR_DOMAIN_EVENT_UNDEFINED", "VIR_DOMAIN_EVENT_STARTED",
  "VIR_DOMAIN_EVENT_SUSPENDED", "VIR_DOMAIN_EVENT_RESUMED", "VIR_DOMAIN_EVENT_STOPPED",
  "VIR_DOMAIN_EVENT_SHUTDOWN", "VIR_DOMAIN_EVENT_PMSUSPENDED", "VIR_DOMAIN_EVENT_CRASHED"};
// enum event agent state
static const char* arrayEventAgentState[] = {"no state", "agent connected", "agent disconnected", "last"};
// enum event agent lifecycle reason
static const char* arrayEventAgentReason[] = {"unknown state change reason", "state changed due to domain start", "channel state changed", "last"};

int domain_event_cb(virConnectPtr conn, virDomainPtr dom, int event, int detail, void *opaque) {
  DebugPrintf("event lifecycle cb called, event=%d, detail=%d\n", event, detail);
  if (event >= 0 && event <= virDomainEventType::VIR_DOMAIN_EVENT_CRASHED) {
    const char* name = virDomainGetName(dom);
    int domain_state = 0;
    if (virDomainGetState(dom, &domain_state, NULL, 0) < 0) {
      domain_state = 0;
    }
    DebugPrintf("domain %s %s, state %s\n", name, arrayEventType[event], arrayDomainState[domain_state]);
  }
  else {
    DebugPrintf("unknowned event\n");
  }
}

void domain_event_agent_cb(virConnectPtr conn, virDomainPtr dom, int state, int reason, void *opaque) {
  DebugPrintf("event agent lifecycle cb called, state=%d, reason=%d\n", state, reason);
  if (state >= 0 && state <= virConnectDomainEventAgentLifecycleState::VIR_CONNECT_DOMAIN_EVENT_AGENT_LIFECYCLE_STATE_DISCONNECTED) {
    const char* name = virDomainGetName(dom);
    int domain_state = 0;
    if (virDomainGetState(dom, &domain_state, NULL, 0) < 0) {
      domain_state = 0;
    }
    DebugPrintf("event agent state: %s, reason: %s, domain state: %s\n", arrayEventAgentState[state], arrayEventAgentReason[reason], arrayDomainState[domain_state]);
  }
  else {
    DebugPrintf("unknowned event agent state\n");
  }
}

void virConnectDeleter(virConnectPtr conn) {
    virConnectClose(conn);
    DebugPrintf("vir connect close\n");
}

void virDomainDeleter(virDomainPtr domain) {
    virDomainFree(domain);
    DebugPrintf("vir domain free\n");
}

void virDomainSnapshotDeleter(virDomainSnapshotPtr snapshot) {
    virDomainSnapshotFree(snapshot);
    DebugPrintf("vir domain snapshot free\n");
}

/////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& out, const DomainSnapshotInfo& obj) {
    if (!obj.name.empty()) {
        std::cout << " ";
        std::cout << std::setw(8) << std::setfill(' ') << std::left << obj.name;
        boost::posix_time::ptime ctime = boost::posix_time::from_time_t(obj.creationTime);
        // std::cout << std::setw(28) << std::setfill(' ') << std::left << boost::posix_time::to_simple_string(ctime);
        // 时间格式 2021-11-30 11:03:55 +0800
        boost::posix_time::ptime  now = boost::posix_time::second_clock::local_time();
        boost::posix_time::ptime  utc = boost::posix_time::second_clock::universal_time();
        boost::posix_time::time_duration tz_offset = (now - utc);

        std::stringstream   ss;
        boost::local_time::local_time_facet* output_facet = new boost::local_time::local_time_facet();
        ss.imbue(std::locale(std::locale::classic(), output_facet));

        output_facet->format("%H:%M:%S");
        ss.str("");
        ss << tz_offset;

        boost::local_time::time_zone_ptr    zone(new boost::local_time::posix_time_zone(ss.str().c_str()));
        boost::local_time::local_date_time  ldt(ctime, zone);
        output_facet->format("%Y-%m-%d %H:%M:%S %Q");
        ss.str("");
        ss << ldt;
        std::cout << std::setw(28) << std::setfill(' ') << std::left << ss.str();
        delete output_facet;
        std::cout << std::setw(16) << std::setfill(' ') << std::left << obj.state;
        std::cout << std::setw(50) << std::setfill(' ') << std::left << obj.description;
        #if 0
        std::cout << std::endl;
        for (int i = 0; i < obj.disks.size(); i++) {
            std::cout << " disk name=" << obj.disks[i].name << ", snapshot=" << obj.disks[i].snapshot
                      << ", driver_type=" << obj.disks[i].driver_type << ", source_file=" << obj.disks[i].source_file;
            if (i != obj.disks.size() - 1)
                std::cout << std::endl;
        }
        #endif
    }
    return out;
}

int getDomainSnapshotInfo(virDomainSnapshotPtr snapshot, DomainSnapshotInfo &info) {
    if (snapshot == nullptr) return -1;
    info.name = virDomainSnapshotGetName(snapshot);
    if (info.name.empty()) return -1;
    char *xmlDesc = virDomainSnapshotGetXMLDesc(snapshot, 0);
    if (xmlDesc) {
        do {
            tinyxml2::XMLDocument doc;
            tinyxml2::XMLError err = doc.Parse(xmlDesc);
            if (err != tinyxml2::XML_SUCCESS) {
                std::cout << "parse domain snapshot xml desc error: " << err << std::endl;
                break;
            }
            tinyxml2::XMLElement *root = doc.RootElement();
            tinyxml2::XMLElement *desc = root->FirstChildElement("description");
            if (desc) {
                info.description = desc->GetText();
            }
            tinyxml2::XMLElement *state = root->FirstChildElement("state");
            if (state) {
                info.state = state->GetText();
            }
            tinyxml2::XMLElement *creationTime = root->FirstChildElement("creationTime");
            if (creationTime) {
                info.creationTime = atoll(creationTime->GetText());
            }
            tinyxml2::XMLElement *disks = root->FirstChildElement("disks");
            if (disks) {
                tinyxml2::XMLElement *disk = disks->FirstChildElement("disk");
                while (disk) {
                    DomainSnapshotDiskInfo dsinfo;
                    dsinfo.name = disk->Attribute("name");
                    dsinfo.snapshot = disk->Attribute("snapshot");
                    tinyxml2::XMLElement *driver = disk->FirstChildElement("driver");
                    if (driver) {
                        dsinfo.driver_type = driver->Attribute("type");
                    }
                    tinyxml2::XMLElement *source = disk->FirstChildElement("source");
                    if (source) {
                        dsinfo.source_file = source->Attribute("file");
                    }
                    info.disks.push_back(dsinfo);
                    disk = disk->NextSiblingElement("disk");
                }
            }
        } while(0);
        free(xmlDesc);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////

virDomainSnapshotImpl::virDomainSnapshotImpl(virDomainSnapshotPtr snapshot)
    : snapshot_(std::shared_ptr<virDomainSnapshot>(snapshot, virDomainSnapshotDeleter)) {

}

virDomainSnapshotImpl::~virDomainSnapshotImpl() {

}

int32_t virDomainSnapshotImpl::revertDomainToThisSnapshot(int flags) {
    if (!snapshot_) return -1;
    return virDomainRevertToSnapshot(snapshot_.get(), flags);
}

int32_t virDomainSnapshotImpl::deleteSnapshot(int flags) {
    if (!snapshot_) return -1;
    return virDomainSnapshotDelete(snapshot_.get(), flags);
}

int32_t virDomainSnapshotImpl::getSnapshotName(std::string &name) {
    if (!snapshot_) return -1;
    name = virDomainSnapshotGetName(snapshot_.get());
    return name.empty() ? -1 : 0;
}

std::shared_ptr<virDomainSnapshotImpl> virDomainSnapshotImpl::getSnapshotParent() {
    if (!snapshot_) return nullptr;
    virDomainSnapshotPtr snap = virDomainSnapshotGetParent(snapshot_.get(), 0);
    if (snap == nullptr) {
        return nullptr;
    }
    return std::make_shared<virDomainSnapshotImpl>(snap);
}

int32_t virDomainSnapshotImpl::getSnapshotXMLDesc(std::string &desc) {
    if (!snapshot_) return -1;
    char *xml = virDomainSnapshotGetXMLDesc(snapshot_.get(), 0);
    desc = xml;
    free(xml);
    return desc.empty() ? -1 : 0;
}

int32_t virDomainSnapshotImpl::listAllSnapshotChilden() {
    if (!snapshot_) return -1;
    virDomainSnapshotPtr *snaps = nullptr;
    int snaps_count = 0;
    if ((snaps_count = virDomainSnapshotListAllChildren(snapshot_.get(), &snaps, 1 << 10)) < 0)
        goto cleanup;
    for (int i = 0; i < snaps_count; i++) {
        const char *name = virDomainSnapshotGetName(snaps[i]);
        if (name) {
            DebugPrintf("list all childen snapshots names[%d]: %s\n", i, name);
            // free(name);
        }
    }
cleanup:
    if (snaps && snaps_count > 0) {
        for (int i = 0; i < snaps_count; i++) {
            virDomainSnapshotFree(snaps[i]);
        }
    }
    if (snaps)
        free(snaps);
    return snaps_count;
}

int32_t virDomainSnapshotImpl::listAllSnapshotChildenNames(std::vector<std::string> *names) {
    return -1;
}

int virDomainSnapshotImpl::getSnapshotChildenNums() {
    if (!snapshot_) return -1;
    return virDomainSnapshotNumChildren(snapshot_.get(), 1 << 10);
}

int32_t virDomainSnapshotImpl::getSnapshotInfo(DomainSnapshotInfo &info) {
    if (!snapshot_) return -1;
    return getDomainSnapshotInfo(snapshot_.get(), info);
}

/////////////////////////////////////////////////////////////////////////////////

virDomainImpl::virDomainImpl(virDomainPtr domain)
    : domain_(std::shared_ptr<virDomain>(domain, virDomainDeleter)) {
}

virDomainImpl::~virDomainImpl() {

}

int32_t virDomainImpl::startDomain() {
    if (!domain_) return -1;
    return virDomainCreate(domain_.get());
}

int32_t virDomainImpl::suspendDomain() {
    if (!domain_) return -1;
    return virDomainSuspend(domain_.get());
}

int32_t virDomainImpl::resumeDomain() {
    if (!domain_) return -1;
    return virDomainResume(domain_.get());
}

int32_t virDomainImpl::rebootDomain(int flag) {
    if (!domain_) return -1;
    return virDomainReboot(domain_.get(), flag);
}

int32_t virDomainImpl::shutdownDomain() {
    if (!domain_) return -1;
    return virDomainShutdown(domain_.get());
}

int32_t virDomainImpl::destroyDomain() {
    if (!domain_) return -1;
    return virDomainDestroy(domain_.get());
}

int32_t virDomainImpl::resetDomain() {
    if (!domain_) return -1;
    return virDomainReset(domain_.get(), 0);
}

int32_t virDomainImpl::undefineDomain() {
    if (!domain_) return -1;
    return virDomainUndefine(domain_.get());
}

int32_t virDomainImpl::deleteDomain() {
    if (!domain_) return -1;
    virDomainInfo info;
    int ret = virDomainGetInfo(domain_.get(), &info);
    if (ret < 0) return ret;
    std::vector<std::string> disks;
    getDomainDisks(disks);
    if (info.state == VIR_DOMAIN_RUNNING) {
        ret = destroyDomain();
        if (ret < 0) return ret;
    }
    ret = undefineDomain();
    if (ret < 0) return ret;
    for (const auto& disk_file : disks) {
        boost::system::error_code error_code;
        if (boost::filesystem::exists(disk_file, error_code) && !error_code) {
            remove(disk_file.c_str());
            std::cout << "delete file: " << disk_file << std::endl;
        }
    }
    return 0;
}

int32_t virDomainImpl::getDomainDisks(std::vector<std::string> &disks) {
    if (!domain_) return -1;
    char* pContent = virDomainGetXMLDesc(domain_.get(), VIR_DOMAIN_XML_SECURE);
    if (!pContent) return -1;
    do {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) break;
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* devices_node = root->FirstChildElement("devices");
        if (!devices_node) break;
        tinyxml2::XMLElement* disk_node = devices_node->FirstChildElement("disk");
        while (disk_node) {
            tinyxml2::XMLElement* disk_source_node = disk_node->FirstChildElement("source");
            std::string disk_file = disk_source_node->Attribute("file");
            // std::cout << "disk file " << disk_file << std::endl;
            disks.push_back(disk_file);
            disk_node = disk_node->NextSiblingElement("disk");
        }
    } while(0);

    free(pContent);
    return 0;
}

int32_t virDomainImpl::getDomainInterfaceAddress(std::string &local_ip) {
    if (!domain_) return -1;
    virDomainInterfacePtr *ifaces = nullptr;
    int ifaces_count = 0;
    size_t i, j;

    if ((ifaces_count = virDomainInterfaceAddresses(domain_.get(), &ifaces,
            VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_LEASE, 0)) < 0)
        goto cleanup;

    for (i = 0; i < ifaces_count; i++) {
        DebugPrintf("name: %s", ifaces[i]->name);
        if (ifaces[i]->hwaddr)
            DebugPrintf(" hwaddr: %s", ifaces[i]->hwaddr);

        for (j = 0; j < ifaces[i]->naddrs; j++) {
            virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
            DebugPrintf("[addr: %s prefix: %d type: %d]",
                ip_addr->addr, ip_addr->prefix, ip_addr->type);
            local_ip = ip_addr->addr;
            if (!local_ip.empty()) break;
        }
        DebugPrintf("\n");
        if (!local_ip.empty()) break;
    }

cleanup:
    if (ifaces && ifaces_count > 0) {
        for (i = 0; i < ifaces_count; i++) {
            virDomainInterfaceFree(ifaces[i]);
        }
    }
    if (ifaces)
        free(ifaces);
    return ifaces_count;
}

int32_t virDomainImpl::setDomainUserPassword(const char *user, const char *password) {
    if (!domain_) return -1;
    return virDomainSetUserPassword(domain_.get(), user, password, 0);
}

std::shared_ptr<virDomainSnapshotImpl> virDomainImpl::createSnapshot(const char *xmlDesc, unsigned int flags) {
    if (!domain_) return nullptr;
    virDomainSnapshotPtr snapshot = virDomainSnapshotCreateXML(domain_.get(), xmlDesc, flags);
    if (snapshot == nullptr) {
        // print last error
        return nullptr;
    }
    return std::make_shared<virDomainSnapshotImpl>(snapshot);
}

std::shared_ptr<virDomainSnapshotImpl> virDomainImpl::getSnapshotByName(const char *name) {
    if (!domain_) return nullptr;
    virDomainSnapshotPtr snapshot = virDomainSnapshotLookupByName(domain_.get(), name, 0);
    if (snapshot == nullptr) {
        // print last error
        return nullptr;
    }
    return std::make_shared<virDomainSnapshotImpl>(snapshot);
}

int32_t virDomainImpl::listAllSnapshots(std::vector<std::shared_ptr<virDomainSnapshotImpl>> &snapshots, unsigned int flags) {
    if (!domain_) return -1;
    virDomainSnapshotPtr *snaps = nullptr;
    int snaps_count = 0;
    if ((snaps_count = virDomainListAllSnapshots(domain_.get(), &snaps, flags)) < 0)
        goto cleanup;
    std::cout << " ";
    std::cout << std::setw(8) << std::setfill(' ') << std::left << "Name";
    std::cout << std::setw(28) << std::setfill(' ') << std::left << "Creation Time";
    std::cout << std::setw(16) << std::setfill(' ') << std::left << "State";
    std::cout << std::setw(50) << std::setfill(' ') << std::left << "description";
    std::cout << std::endl;
    std::cout << std::setw(1 + 8 + 28 + 16 + 50) << std::setfill('-') << std::left << "" << std::endl;
    // DebugPrintf(" Name    Creation Time               State\n");
    // DebugPrintf("----------------------------------------------------\n");
    for (int i = 0; i < snaps_count; i++) {
        DomainSnapshotInfo dsInfo;
        getDomainSnapshotInfo(snaps[i], dsInfo);
        std::cout << dsInfo << std::endl;
    }
cleanup:
    if (snaps && snaps_count > 0) {
        for (int i = 0; i < snaps_count; i++) {
            // virDomainSnapshotFree(snaps[i]);
            snapshots.push_back(std::make_shared<virDomainSnapshotImpl>(snaps[i]));
        }
    }
    if (snaps)
        free(snaps);
    return snaps_count;
}

int32_t virDomainImpl::listSnapshotNames(std::vector<std::string> &names, int nameslen, unsigned int flags) {
    if (!domain_ && nameslen < 1) return -1;
    char **ns = (char**)malloc(sizeof(char*) * nameslen);
    int nlen = 0;
    // libvirt官网不鼓励使用此API，而是推荐使用virDomainListAllSnapshots ()。
    if ((nlen = virDomainSnapshotListNames(domain_.get(), ns, nameslen, flags)) < 0)
        goto cleanup;
    // for (int i = 0; i < nlen; i++) {
    //     DebugPrintf("domain snapshot names[%d]: %s\n", i, ns[i]);
    // }
cleanup:
    if (ns && nlen > 0) {
        for (int i = 0; i < nlen; i++) {
            names.push_back(ns[i]);
            free(ns[i]);
        }
    }
    if (ns)
        free(ns);
    return nlen;
}

int32_t virDomainImpl::getSnapshotNums(unsigned int flags) {
    if (!domain_) return -1;
    return virDomainSnapshotNum(domain_.get(), flags);
}

/////////////////////////////////////////////////////////////////////////////////

virTool::virTool() : conn_(nullptr), callback_id_(-1), agent_callback_id_(-1), thread_quit_(1), thread_event_loop_(nullptr) {
    int ret = virEventRegisterDefaultImpl();
    if (ret < 0) {
        DebugPrintf("virEventRegisterDefaultImpl failed\n");
    }
}

virTool::~virTool() {
    thread_quit_ = 1;
    if (thread_event_loop_) {
        if (thread_event_loop_->joinable())
            thread_event_loop_->join();
        delete thread_event_loop_;
    }
    if (conn_) {
        virConnectDomainEventDeregisterAny(conn_.get(), callback_id_);
        virConnectDomainEventDeregisterAny(conn_.get(), agent_callback_id_);
    }
}

bool virTool::openConnect(const char *name) {
    virConnectPtr connectPtr = virConnectOpen(name);
    if (connectPtr == nullptr) {
        return false;
    }
    conn_ = std::shared_ptr<virConnect>(connectPtr, virConnectDeleter);
    if (connectPtr) {
        callback_id_ = virConnectDomainEventRegisterAny(connectPtr, NULL,
            virDomainEventID::VIR_DOMAIN_EVENT_ID_LIFECYCLE, VIR_DOMAIN_EVENT_CALLBACK(domain_event_cb), NULL, NULL);
        agent_callback_id_ = virConnectDomainEventRegisterAny(connectPtr, NULL,
            virDomainEventID::VIR_DOMAIN_EVENT_ID_AGENT_LIFECYCLE, VIR_DOMAIN_EVENT_CALLBACK(domain_event_agent_cb), NULL, NULL);
        thread_quit_ = 0;
        thread_event_loop_ = new std::thread(&virTool::DefaultThreadFunc, this);
    }
    return !!conn_;
}

std::shared_ptr<virDomainImpl> virTool::defineDomain(const char *xml_content) {
    if (!conn_) return nullptr;
    virDomainPtr domainPtr = virDomainDefineXML(conn_.get(), xml_content);
    if (nullptr == domainPtr) {
        return nullptr;
    }
    return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virTool::openDomain(const char *domain_name) {
    if (!conn_) return nullptr;
    virDomainPtr domainPtr = virDomainLookupByName(conn_.get(), domain_name);
    if (nullptr == domainPtr) {
        return nullptr;
    }
    return std::make_shared<virDomainImpl>(domainPtr);
}

void virTool::DefaultThreadFunc() {
  DebugPrintf("vir event loop thread begin\n");
  while (thread_quit_ == 0) {
    if (virEventRunDefaultImpl() < 0) {
      virErrorPtr err = virGetLastError();
      if (err) {
        DebugPrintf("virEventRunDefaultImpl failed: %s\n", err->message);
        virFreeError(err);
      }
    }
  }
  DebugPrintf("vir event loop thread end\n");
}

