#include "vir_helper.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <tinyxml2.h>

// #define __DEBUG__ 1

#ifdef __DEBUG__
#define DebugPrintf(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DebugPrintf(format, ...)
#endif

// void virConnectDeleter(virConnectPtr conn) {
//   virConnectClose(conn);
//   DebugPrintf("vir connect close\n");
// }

// void virDomainDeleter(virDomainPtr domain) {
//   virDomainFree(domain);
//   DebugPrintf("vir domain free\n");
// }

// void virDomainSnapshotDeleter(virDomainSnapshotPtr snapshot) {
//   virDomainSnapshotFree(snapshot);
//   DebugPrintf("vir domain snapshot free\n");
// }

// void virErrorDeleter(virErrorPtr error) {
//   virFreeError(error);
//   DebugPrintf("vir error free\n");
// }

struct virConnectDeleter {
  inline void operator()(virConnectPtr conn) {
    virConnectClose(conn);
    DebugPrintf("vir connect close\n");
  }
};

struct virDomainDeleter {
  inline void operator()(virDomainPtr domain) {
    virDomainFree(domain);
    DebugPrintf("vir domain free\n");
  }
};

struct virDomainSnapshotDeleter {
  inline void operator()(virDomainSnapshotPtr snapshot) {
    virDomainSnapshotFree(snapshot);
    DebugPrintf("vir domain snapshot free\n");
  }
};

struct virNWFilterDeleter {
  inline void operator()(virNWFilterPtr nwfilter) {
    virNWFilterFree(nwfilter);
    DebugPrintf("vir nwfilter free\n");
  }
};

struct virErrorDeleter {
  inline void operator()(virErrorPtr error) {
    virFreeError(error);
    DebugPrintf("vir error free\n");
  }
};

namespace vir_helper {

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
// block job type
static const char* arrayBlockJobType[] = {"VIR_DOMAIN_BLOCK_JOB_TYPE_UNKNOWN", "VIR_DOMAIN_BLOCK_JOB_TYPE_PULL", "VIR_DOMAIN_BLOCK_JOB_TYPE_COPY",
  "VIR_DOMAIN_BLOCK_JOB_TYPE_COMMIT", "VIR_DOMAIN_BLOCK_JOB_TYPE_ACTIVE_COMMIT", "VIR_DOMAIN_BLOCK_JOB_TYPE_BACKUP", "VIR_DOMAIN_BLOCK_JOB_TYPE_LAST"};
// block job status
static const char* arrayBlockJobStatus[] = {"VIR_DOMAIN_BLOCK_JOB_COMPLETED", "VIR_DOMAIN_BLOCK_JOB_FAILED", "VIR_DOMAIN_BLOCK_JOB_CANCELED",
  "VIR_DOMAIN_BLOCK_JOB_READY", "VIR_DOMAIN_BLOCK_JOB_LAST"};

int domain_event_lifecycle_cb(virConnectPtr conn, virDomainPtr dom, int event, int detail, void *opaque) {
  DebugPrintf("event lifecycle cb called, event=%d, detail=%d\n", event, detail);
  if (event >= 0 && event <= virDomainEventType::VIR_DOMAIN_EVENT_CRASHED) {
    const char* name = virDomainGetName(dom);
    int domain_state = 0;
    if (virDomainGetState(dom, &domain_state, NULL, 0) < 0) {
      domain_state = 0;
    }
    DebugPrintf("domain %s %s, state %s\n", name, arrayEventType[event], arrayDomainState[domain_state]);
  } else {
    DebugPrintf("unknowned event lifecycle\n");
  }
  return 0;
}

void domain_event_agent_cb(virConnectPtr conn, virDomainPtr dom, int state, int reason, void *opaque) {
  DebugPrintf("event agent lifecycle cb called, state=%d, reason=%d\n", state, reason);
  if (state >= 0 && state <= virConnectDomainEventAgentLifecycleState::VIR_CONNECT_DOMAIN_EVENT_AGENT_LIFECYCLE_STATE_DISCONNECTED) {
    const char* name = virDomainGetName(dom);
    int domain_state = 0;
    if (virDomainGetState(dom, &domain_state, NULL, 0) < 0) {
      domain_state = 0;
    }
    DebugPrintf("domain: %s, event agent state: %s, reason: %s, domain state: %s\n", name,
      arrayEventAgentState[state], arrayEventAgentReason[reason], arrayDomainState[domain_state]);
  } else {
    DebugPrintf("unknowned event agent state\n");
  }
}

void domain_event_block_job_cb(virConnectPtr conn, virDomainPtr dom, const char *disk, int type, int status, void *opaque) {
  DebugPrintf("domain event block job cb called, disk=%s, type=%d, status=%d\n", disk, type, status);
  if (status >= 0 && status <= virConnectDomainEventBlockJobStatus::VIR_DOMAIN_BLOCK_JOB_READY) {
    const char* name = virDomainGetName(dom);
    int domain_state = 0;
    if (virDomainGetState(dom, &domain_state, NULL, 0) < 0) {
      domain_state = 0;
    }
    DebugPrintf("domain: %s, disk: %s, state: %s, block job type: %s, status: %s\n", name, disk,
      arrayDomainState[domain_state], arrayBlockJobType[type], arrayBlockJobStatus[status]);
  } else {
    DebugPrintf("unknowned block job state\n");
  }
}

////////////////////////////////////////////////////////////////////////////////

static inline void PrintTypedParameter(virTypedParameterPtr params, int nparams) {
  for (int i = 0; i < nparams; i++) {
    DebugPrintf("parameter[%d]: field=%s, type=%d, value=", i, params[i].field, params[i].type);
    switch (params[i].type)
    {
    case 1:
      DebugPrintf("%d", params[i].value.i);
      break;
    case 2:
      DebugPrintf("%ud", params[i].value.ui);
      break;
    case 3:
      DebugPrintf("%lld", params[i].value.l);
      break;
    case 4:
      DebugPrintf("%llu", params[i].value.ul);
      break;
    case 5:
      DebugPrintf("%lf", params[i].value.d);
      break;
    case 6:
      DebugPrintf("%c", params[i].value.b);
      break;
    case 7:
      DebugPrintf("%s", params[i].value.s);
      break;

    default:
      DebugPrintf("unknown value");
      break;
    }
    DebugPrintf("\n");
  }
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& out, const domainDiskInfo& obj) {
  std::cout << " ";
  std::cout << std::setw(8) << std::setfill(' ') << std::left << obj.target_dev;
  std::cout << std::setw(12) << std::setfill(' ') << std::left << obj.driver_name;
  std::cout << std::setw(12) << std::setfill(' ') << std::left << obj.driver_type;
  std::cout << std::setw(50) << std::setfill(' ') << std::left << obj.source_file;
  return out;
}

std::ostream& operator<<(std::ostream& out, const domainSnapshotInfo& obj) {
  if (!obj.name.empty()) {
    std::cout << " ";
    std::cout << std::setw(8) << std::setfill(' ') << std::left << obj.name;
    // boost::posix_time::ptime ctime = boost::posix_time::from_time_t(obj.creationTime);
    // // std::cout << std::setw(28) << std::setfill(' ') << std::left << boost::posix_time::to_simple_string(ctime);
    // // 时间格式 2021-11-30 11:03:55 +0800
    // boost::posix_time::ptime  now = boost::posix_time::second_clock::local_time();
    // boost::posix_time::ptime  utc = boost::posix_time::second_clock::universal_time();
    // boost::posix_time::time_duration tz_offset = (now - utc);

    // std::stringstream   ss;
    // boost::local_time::local_time_facet* output_facet = new boost::local_time::local_time_facet();
    // ss.imbue(std::locale(std::locale::classic(), output_facet));

    // output_facet->format("%H:%M:%S");
    // ss.str("");
    // ss << tz_offset;

    // boost::local_time::time_zone_ptr    zone(new boost::local_time::posix_time_zone(ss.str().c_str()));
    // boost::local_time::local_date_time  ldt(ctime, zone);
    // output_facet->format("%Y-%m-%d %H:%M:%S %Q");
    // ss.str("");
    // ss << ldt;
    // std::cout << std::setw(28) << std::setfill(' ') << std::left << ss.str();
    // delete output_facet;
    // C语言方法
    // struct tm _tm{};
    // localtime_r(&obj.creationTime, &_tm);
    // char buf[256] = {0};
    // strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S %z", &_tm);
    // std::cout << std::setw(28) << std::setfill(' ') << std::left << buf;
    std::stringstream   ss;
    ss.str("");
    ss << std::put_time(std::localtime(&obj.creationTime), "%Y-%m-%d %H:%M:%S %z");
    std::cout << std::setw(28) << std::setfill(' ') << std::left << ss.str();
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

int getDomainSnapshotInfo(virDomainSnapshotPtr snapshot, domainSnapshotInfo &info) {
  int ret = -1;
  if (snapshot == nullptr) return ret;
  info.name = virDomainSnapshotGetName(snapshot);
  if (info.name.empty()) return ret;
  char *xmlDesc = virDomainSnapshotGetXMLDesc(snapshot, 0);
  if (xmlDesc) {
    do {
      tinyxml2::XMLDocument doc;
      tinyxml2::XMLError err = doc.Parse(xmlDesc);
      if (err != tinyxml2::XML_SUCCESS) {
        DebugPrintf("parse domain snapshot xml desc error: %d\n", err);
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
          domainSnapshotDiskInfo dsinfo;
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
      ret = 0;
    } while(0);
    free(xmlDesc);
  }
  return ret;
}

std::shared_ptr<virError> getLastError() {
  return std::shared_ptr<virError>(virGetLastError(), virErrorDeleter());
}

////////////////////////////////////////////////////////////////////////////////

virNWFilterImpl::virNWFilterImpl(virNWFilterPtr nwfilter)
  : nwfilter_(std::shared_ptr<virNWFilter>(nwfilter, virNWFilterDeleter())) {

}

int virNWFilterImpl::getNWFilterName(std::string &name) {
  if (!nwfilter_) return -1;
  name = virNWFilterGetName(nwfilter_.get());
  return name.empty() ? -1 : 0;
}

int virNWFilterImpl::getNWFilterUUID(unsigned char * uuid) {
  if (!nwfilter_) return -1;
  return virNWFilterGetUUID(nwfilter_.get(), uuid);
}

int virNWFilterImpl::getNWFilterUUIDString(char * buf) {
  if (!nwfilter_) return -1;
  return virNWFilterGetUUIDString(nwfilter_.get(), buf);
}

int virNWFilterImpl::getNWFilterXMLDesc(std::string &desc, unsigned int flags) {
  if (!nwfilter_) return -1;
  char *xml = virNWFilterGetXMLDesc(nwfilter_.get(), flags);
  desc = xml;
  free(xml);
  return desc.empty() ? -1 : 0;
}

int virNWFilterImpl::undefineNWFilter() {
  if (!nwfilter_) return -1;
  return virNWFilterUndefine(nwfilter_.get());
}

////////////////////////////////////////////////////////////////////////////////

virDomainSnapshotImpl::virDomainSnapshotImpl(virDomainSnapshotPtr snapshot)
  : snapshot_(std::shared_ptr<virDomainSnapshot>(snapshot, virDomainSnapshotDeleter())) {

}

int virDomainSnapshotImpl::revertDomainToThisSnapshot(int flags) {
  if (!snapshot_) return -1;
  return virDomainRevertToSnapshot(snapshot_.get(), flags);
}

int virDomainSnapshotImpl::deleteSnapshot(int flags) {
  if (!snapshot_) return -1;
  return virDomainSnapshotDelete(snapshot_.get(), flags);
}

int virDomainSnapshotImpl::getSnapshotName(std::string &name) {
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

int virDomainSnapshotImpl::getSnapshotXMLDesc(std::string &desc) {
  if (!snapshot_) return -1;
  char *xml = virDomainSnapshotGetXMLDesc(snapshot_.get(), 0);
  desc = xml;
  free(xml);
  return desc.empty() ? -1 : 0;
}

int virDomainSnapshotImpl::listAllSnapshotChilden() {
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

int virDomainSnapshotImpl::listAllSnapshotChildenNames(std::vector<std::string> *names) {
  return -1;
}

int virDomainSnapshotImpl::getSnapshotChildenNums() {
  if (!snapshot_) return -1;
  return virDomainSnapshotNumChildren(snapshot_.get(), 1 << 10);
}

int virDomainSnapshotImpl::getSnapshotInfo(domainSnapshotInfo &info) {
  if (!snapshot_) return -1;
  return getDomainSnapshotInfo(snapshot_.get(), info);
}

////////////////////////////////////////////////////////////////////////////////

virDomainImpl::virDomainImpl(virDomainPtr domain)
  : domain_(std::shared_ptr<virDomain>(domain, virDomainDeleter())) {
}

int virDomainImpl::startDomain() {
  if (!domain_) return -1;
  return virDomainCreate(domain_.get());
}

int virDomainImpl::suspendDomain() {
  if (!domain_) return -1;
  return virDomainSuspend(domain_.get());
}

int virDomainImpl::resumeDomain() {
  if (!domain_) return -1;
  return virDomainResume(domain_.get());
}

int virDomainImpl::rebootDomain(int flag) {
  if (!domain_) return -1;
  return virDomainReboot(domain_.get(), flag);
}

int virDomainImpl::shutdownDomain() {
  if (!domain_) return -1;
  return virDomainShutdown(domain_.get());
}

int virDomainImpl::destroyDomain() {
  if (!domain_) return -1;
  return virDomainDestroy(domain_.get());
}

int virDomainImpl::resetDomain() {
  if (!domain_) return -1;
  return virDomainReset(domain_.get(), 0);
}

int virDomainImpl::undefineDomain() {
  if (!domain_) return -1;
  return virDomainUndefine(domain_.get());
}

int virDomainImpl::deleteDomain() {
  if (!domain_) return -1;
  virDomainInfo info;
  int ret = virDomainGetInfo(domain_.get(), &info);
  if (ret < 0) return ret;
  std::vector<domainDiskInfo> disks;
  getDomainDisks(disks);
  if (info.state == VIR_DOMAIN_RUNNING) {
    ret = destroyDomain();
    if (ret < 0) return ret;
  }
  ret = undefineDomain();
  if (ret < 0) return ret;
  for (const auto& disk_file : disks) {
    if (access(disk_file.source_file.c_str(), F_OK) != -1) {
      remove(disk_file.source_file.c_str());
      DebugPrintf("delete file: %s\n", disk_file.source_file.c_str());
    }
  }
  return 0;
}

int virDomainImpl::getDomainDisks(std::vector<domainDiskInfo> &disks) {
  int ret = -1;
  if (!domain_) return ret;
  char* pContent = virDomainGetXMLDesc(domain_.get(), VIR_DOMAIN_XML_SECURE);
  if (!pContent) return ret;
  do {
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError err = doc.Parse(pContent);
    if (err != tinyxml2::XML_SUCCESS) break;
    tinyxml2::XMLElement* root = doc.RootElement();
    tinyxml2::XMLElement* devices_node = root->FirstChildElement("devices");
    if (!devices_node) break;
    tinyxml2::XMLElement* disk_node = devices_node->FirstChildElement("disk");
    while (disk_node) {
      domainDiskInfo ddInfo;
      tinyxml2::XMLElement* disk_driver_node = disk_node->FirstChildElement("driver");
      if (disk_driver_node) {
        ddInfo.driver_name = disk_driver_node->Attribute("name");
        ddInfo.driver_type = disk_driver_node->Attribute("type");
      }
      tinyxml2::XMLElement* disk_source_node = disk_node->FirstChildElement("source");
      if (disk_source_node) {
        ddInfo.source_file = disk_source_node->Attribute("file");
      }
      tinyxml2::XMLElement* disk_target_node = disk_node->FirstChildElement("target");
      if (disk_target_node) {
        ddInfo.target_dev = disk_target_node->Attribute("dev");
        ddInfo.target_bus = disk_target_node->Attribute("bus");
      }
      disks.push_back(ddInfo);
      disk_node = disk_node->NextSiblingElement("disk");
    }
    ret = 0;
  } while(0);

  free(pContent);
  return ret;
}

int virDomainImpl::getDomainInterfaceAddress(std::vector<domainInterface> &difaces, unsigned int source) {
  if (!domain_) return -1;
  virDomainInterfacePtr *ifaces = nullptr;
  int ifaces_count = 0;
  size_t i, j;
  domainInterface diface;

  if ((ifaces_count = virDomainInterfaceAddresses(domain_.get(), &ifaces, source, 0)) < 0)
    goto cleanup;

  for (i = 0; i < ifaces_count; i++) {
    DebugPrintf("name: %s", ifaces[i]->name);
    diface.name = ifaces[i]->name;
    if (ifaces[i]->hwaddr) {
      DebugPrintf(" hwaddr: %s", ifaces[i]->hwaddr);
      diface.hwaddr = ifaces[i]->hwaddr;
    } else {
      diface.hwaddr.clear();
    }

    diface.addrs.clear();
    for (j = 0; j < ifaces[i]->naddrs; j++) {
      virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
      DebugPrintf("[addr: %s prefix: %d type: %d]", ip_addr->addr, ip_addr->prefix, ip_addr->type);
      diface.addrs.push_back(domainIPAddress{ip_addr->type, ip_addr->addr, ip_addr->prefix});
    }
    DebugPrintf("\n");
    difaces.push_back(diface);
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

int virDomainImpl::setDomainUserPassword(const char *user, const char *password) {
  if (!domain_) return -1;
  return virDomainSetUserPassword(domain_.get(), user, password, 0);
}

int virDomainImpl::isDomainActive() {
  if (!domain_) return -1;
  return virDomainIsActive(domain_.get());
}

int virDomainImpl::getDomainInfo(virDomainInfoPtr info) {
  if (!domain_) return -1;
  return virDomainGetInfo(domain_.get(), info);
}

int virDomainImpl::getDomainCPUStats(virTypedParameterPtr params, unsigned int nparams, int start_cpu, unsigned int ncpus, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainGetCPUStats(domain_.get(), params, nparams, start_cpu, ncpus, flags);
}

int virDomainImpl::getDomainMemoryStats(virDomainMemoryStatPtr stats, unsigned int nr_stats, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainMemoryStats(domain_.get(), stats, nr_stats, flags);
}

int virDomainImpl::getDomainBlockInfo(const char *disk, virDomainBlockInfoPtr info, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainGetBlockInfo(domain_.get(), disk, info, flags);
}

int virDomainImpl::getDomainBlockStats(const char *disk, virDomainBlockStatsPtr stats, size_t size) {
  if (!domain_) return -1;
  return virDomainBlockStats(domain_.get(), disk, stats, size);
}

int virDomainImpl::getDomainNetworkStats(const char *device, virDomainInterfaceStatsPtr stats, size_t size) {
  if (!domain_) return -1;
  return virDomainInterfaceStats(domain_.get(), device, stats, size);
}

std::string virDomainImpl::QemuAgentCommand(const char *cmd, int timeout, unsigned int flags) {
  std::string ret;
  char *result = virDomainQemuAgentCommand(domain_.get(), cmd, timeout, flags);
  if (result) {
    ret = result;
    free(result);
  }
  return ret;
}

int virDomainImpl::getDomainState(int *state, int *reason, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainGetState(domain_.get(), state, reason, flags);
}

int virDomainImpl::getDomainStatsList(unsigned int stats) {
  if (!domain_) return -1;
  virDomainStatsRecordPtr *retStats = NULL;
  virDomainPtr doms[] = {domain_.get(), NULL};
  int retStats_count = virDomainListGetStats(doms, stats, &retStats, VIR_CONNECT_GET_ALL_DOMAINS_STATS_ENFORCE_STATS);
  if (retStats_count < 0)
    goto error;
  for (int i = 0; i < retStats_count; i++) {
    // virTypedParameterPtr params = retStats[i]->params;
    // for (int j = 0; j < retStats[i]->nparams; j++) {
    //   DebugPrintf("block parameter[%d]: field=%s, type=%d\n", j, params[j].field, params[j].type);
    // }
    PrintTypedParameter(retStats[i]->params, retStats[i]->nparams);
  }
error:
  if (retStats) {
    virDomainStatsRecordListFree(retStats);
  }
  return retStats_count;
}

int virDomainImpl::getDomainBlockInfo(const char *disk) {
  if (!domain_) return -1;
  virDomainBlockInfo info;
  int ret = virDomainGetBlockInfo(domain_.get(), disk, &info, 0);
  if (ret < 0) return ret;
  if (info.allocation == info.physical) {
    // If the domain is no longer active,
    // then the defaults are being returned.
    if (!virDomainIsActive(domain_.get())) {
      DebugPrintf("domain is not active, block info is default\n");
      return ret;
    }
  }
  // Do something with the allocation and physical values
  DebugPrintf("device:%s, block capacity:%llu, allocation:%llu, physical:%llu\n", disk, info.capacity, info.allocation, info.physical);
  return ret;
}

int virDomainImpl::getDomainBlockParameters() {
  if (!domain_) return -1;
  virTypedParameterPtr params = nullptr;
  int nparams = 0;
  if (virDomainGetBlkioParameters(domain_.get(), NULL, &nparams, 0) == 0 && nparams != 0) {
    if ((params = (virTypedParameterPtr)malloc(sizeof(*params) * nparams)) == NULL)
      goto error;
    memset(params, 0, sizeof(*params) * nparams);
    if (virDomainGetMemoryParameters(domain_.get(), params, &nparams, 0) < 0)
      goto error;
    // for (int i = 0; i < nparams; i++) {
    //   DebugPrintf("block parameter[%d]: field=%s, type=%d\n", i, params[i].field, params[i].type);
    // }
    PrintTypedParameter(params, nparams);
  }
error:
  if (params)
    free(params);
  return nparams;
}

int virDomainImpl::getDomainBlockIoTune(const char *disk) {
  if (!domain_) return -1;
  virTypedParameterPtr params = nullptr;
  int nparams = 0;
  if (virDomainGetBlockIoTune(domain_.get(), disk, NULL, &nparams, 0) == 0 && nparams != 0) {
    if ((params = (virTypedParameterPtr)malloc(sizeof(*params) * nparams)) == NULL)
      goto error;
    memset(params, 0, sizeof(*params) * nparams);
    if (virDomainGetBlockIoTune(domain_.get(), disk, params, &nparams, 0) < 0)
      goto error;
    // for (int i = 0; i < nparams; i++) {
    //   DebugPrintf("block parameter[%d]: field=%s, type=%d\n", i, params[i].field, params[i].type);
    // }
    PrintTypedParameter(params, nparams);
  }
error:
  if (params)
    free(params);
  return nparams;
}

int virDomainImpl::getDomainFSInfo() {
  if (!domain_) return -1;
  virDomainFSInfoPtr *fsInfos = nullptr;
  int fsInfos_count = virDomainGetFSInfo(domain_.get(), &fsInfos, 0);
  if (fsInfos_count < 0)
    goto cleanup;
  for (int i = 0; i < fsInfos_count; i++) {
    DebugPrintf("device name:%s, mountpoint:%s, fstype:%s", fsInfos[i]->name, fsInfos[i]->mountpoint, fsInfos[i]->fstype);
    for (size_t j = 0; j < fsInfos[i]->ndevAlias; j++) {
      DebugPrintf(", devAlias%lu:%s", j, fsInfos[i]->devAlias[j]);
    }
    DebugPrintf("\n");
  }
cleanup:
  if (fsInfos && fsInfos_count > 0) {
    for (int i = 0; i < fsInfos_count; i++) {
      virDomainFSInfoFree(fsInfos[i]);
    }
  }
  free(fsInfos);
  return fsInfos_count;
}

int virDomainImpl::getDomainGuestInfo() {
  if (!domain_) return -1;
//   virTypedParameterPtr *params = nullptr;
//   int nparams = 0;
//   // https://libvirt.org/news.html
//   // v5.7.0 (2019-09-03) add virDomainGetGuestInfo method
//   int ret = virDomainGetGuestInfo(domain_.get(), VIR_DOMAIN_GUEST_INFO_DISKS, &params, &nparams, 0);
//   if (ret < 0)
//     goto cleanup;
//   for (int i = 0; i < nparams; i++) {
//     // TODO
//     printf("\n");
//   }
// cleanup:
//   if (params && nparams > 0) {
//     for (int i = 0; i < nparams; i++) {
//       virTypedParamsFree(params[i]);
//     }
//   }
//   free(params);
  return 0;
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

int virDomainImpl::listAllSnapshots(std::vector<std::shared_ptr<virDomainSnapshotImpl>> &snapshots, unsigned int flags) {
  if (!domain_) return -1;
  virDomainSnapshotPtr *snaps = nullptr;
  int snaps_count = 0;
  if ((snaps_count = virDomainListAllSnapshots(domain_.get(), &snaps, flags)) < 0)
    goto cleanup;
  // DebugPrintf(" Name    Creation Time               State\n");
  // DebugPrintf("----------------------------------------------------\n");
  // for (int i = 0; i < snaps_count; i++) {
  //   domainSnapshotInfo dsInfo;
  //   getDomainSnapshotInfo(snaps[i], dsInfo);
  //   std::cout << dsInfo << std::endl;
  // }
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

int virDomainImpl::listSnapshotNames(std::vector<std::string> &names, int nameslen, unsigned int flags) {
  if (!domain_ && nameslen < 1) return -1;
  char **ns = (char**)malloc(sizeof(char*) * nameslen);
  int nlen = 0;
  // libvirt官网不鼓励使用此API，而是推荐使用virDomainListAllSnapshots ()。
  if ((nlen = virDomainSnapshotListNames(domain_.get(), ns, nameslen, flags)) < 0)
    goto cleanup;
  // for (int i = 0; i < nlen; i++) {
  //   DebugPrintf("domain snapshot names[%d]: %s\n", i, ns[i]);
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

int virDomainImpl::getSnapshotNums(unsigned int flags) {
  if (!domain_) return -1;
  return virDomainSnapshotNum(domain_.get(), flags);
}

int virDomainImpl::blockCommit(const char *disk, const char *base, const char *top, unsigned long bandwith, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainBlockCommit(domain_.get(), disk, base, top, bandwith, flags);
}

int virDomainImpl::blockPull(const char *disk, unsigned long bandwith, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainBlockPull(domain_.get(), disk, bandwith, flags);
}

int virDomainImpl::blockRebase(const char *disk, const char *base, unsigned long bandwith, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainBlockRebase(domain_.get(), disk, base, bandwith, flags);
}

int virDomainImpl::getBlockJobInfo(const char *disk, virDomainBlockJobInfoPtr info, unsigned int flags) {
  if (!domain_) return -1;
  return virDomainGetBlockJobInfo(domain_.get(), disk, info, flags);
}

////////////////////////////////////////////////////////////////////////////////

virHelper::virHelper(bool enableEvent)
  : conn_(nullptr)
  , enable_event_(enableEvent)
  , dom_event_lifecycle_callback_id_(-1)
  , dom_event_agent_callback_id_(-1)
  , dom_event_block_job_callback_id_(-1)
  , thread_quit_(1)
  , thread_event_loop_(nullptr) {
  if (enableEvent) {
    int ret = virEventRegisterDefaultImpl();
    if (ret < 0) {
      DebugPrintf("virEventRegisterDefaultImpl failed\n");
    }
  }
}

virHelper::~virHelper() {
  thread_quit_ = 1;
  if (thread_event_loop_) {
    if (thread_event_loop_->joinable())
      thread_event_loop_->join();
    delete thread_event_loop_;
  }
  if (conn_ && enable_event_) {
    virConnectDomainEventDeregisterAny(conn_.get(), dom_event_lifecycle_callback_id_);
    virConnectDomainEventDeregisterAny(conn_.get(), dom_event_agent_callback_id_);
    virConnectDomainEventDeregisterAny(conn_.get(), dom_event_block_job_callback_id_);
  }
}

int virHelper::getVersion(unsigned long *libVer, const char *type, unsigned long *typeVer) {
  return virGetVersion(libVer, type, typeVer);
}

int virHelper::getConnectVersion(unsigned long *hvVer) {
  if (!conn_) return -1;
  return virConnectGetVersion(conn_.get(), hvVer);
}

int virHelper::getConnectLibVersion(unsigned long *libVer) {
  if (!conn_) return -1;
  return virConnectGetLibVersion(conn_.get(), libVer);
}

bool virHelper::openConnect(const char *name) {
  // if (conn_) return true;
  virConnectPtr connectPtr = virConnectOpen(name);
  if (connectPtr == nullptr) {
      return false;
  }
  conn_.reset(connectPtr, virConnectDeleter());
  if (connectPtr && enable_event_) {
    dom_event_lifecycle_callback_id_ = virConnectDomainEventRegisterAny(connectPtr, NULL,
      virDomainEventID::VIR_DOMAIN_EVENT_ID_LIFECYCLE, VIR_DOMAIN_EVENT_CALLBACK(domain_event_lifecycle_cb), NULL, NULL);
    dom_event_agent_callback_id_ = virConnectDomainEventRegisterAny(connectPtr, NULL,
      virDomainEventID::VIR_DOMAIN_EVENT_ID_AGENT_LIFECYCLE, VIR_DOMAIN_EVENT_CALLBACK(domain_event_agent_cb), NULL, NULL);
    dom_event_block_job_callback_id_ = virConnectDomainEventRegisterAny(connectPtr, NULL,
      virDomainEventID::VIR_DOMAIN_EVENT_ID_BLOCK_JOB_2, VIR_DOMAIN_EVENT_CALLBACK(domain_event_block_job_cb), NULL, NULL);
    thread_quit_ = 0;
    thread_event_loop_ = new std::thread(&virHelper::DefaultThreadFunc, this);
  }
  return !!conn_;
}

bool virHelper::openConnectReadOnly(const char *name) {
  // virConnectPtr connectPtr = virConnectOpenReadOnly(name);
  // return !!connectPtr;
  return false;
}

int virHelper::listAllDomains(virDomainPtr **domains, unsigned int flags) {
  if (!conn_) return -1;
  return virConnectListAllDomains(conn_.get(), domains, flags);
}

int virHelper::listDomains(int *ids, int maxids) {
  if (!conn_) return -1;
  return virConnectListDomains(conn_.get(), ids, maxids);
}

int virHelper::numOfDomains() {
  if (!conn_) return -1;
  return virConnectNumOfDomains(conn_.get());
}

std::shared_ptr<virDomainImpl> virHelper::openDomainByID(int id) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainLookupByID(conn_.get(), id);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virHelper::openDomainByName(const char *domain_name) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainLookupByName(conn_.get(), domain_name);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virHelper::openDomainByUUID(const unsigned char *uuid) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainLookupByUUID(conn_.get(), uuid);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virHelper::openDomainByUUIDString(const char *uuid) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainLookupByUUIDString(conn_.get(), uuid);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virHelper::defineDomain(const char *xml_content) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainDefineXML(conn_.get(), xml_content);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  return std::make_shared<virDomainImpl>(domainPtr);
}

std::shared_ptr<virDomainImpl> virHelper::createDomain(const char *xml_content) {
  if (!conn_) return nullptr;
  virDomainPtr domainPtr = virDomainDefineXML(conn_.get(), xml_content);
  if (nullptr == domainPtr) {
    return nullptr;
  }
  std::shared_ptr<virDomainImpl> dom = std::make_shared<virDomainImpl>(domainPtr);
  if (dom && dom->startDomain() < 0) {
    return nullptr;
  }
  return std::move(dom);
}

std::shared_ptr<virNWFilterImpl> virHelper::defineNWFilter(const char * xmlDesc) {
  if (!conn_) return nullptr;
  virNWFilterPtr nwfilterPtr = virNWFilterDefineXML(conn_.get(), xmlDesc);
  if (nullptr == nwfilterPtr) return nullptr;
  return std::make_shared<virNWFilterImpl>(nwfilterPtr);
}

std::shared_ptr<virNWFilterImpl> virHelper::openNWFilterByName(const char * name) {
  if (!conn_) return nullptr;
  virNWFilterPtr nwfilterPtr = virNWFilterLookupByName(conn_.get(), name);
  if (nullptr == nwfilterPtr) return nullptr;
  return std::make_shared<virNWFilterImpl>(nwfilterPtr);
}

std::shared_ptr<virNWFilterImpl> virHelper::openNWFilterByUUID(const unsigned char * uuid) {
  if (!conn_) return nullptr;
  virNWFilterPtr nwfilterPtr = virNWFilterLookupByUUID(conn_.get(), uuid);
  if (nullptr == nwfilterPtr) return nullptr;
  return std::make_shared<virNWFilterImpl>(nwfilterPtr);
}

std::shared_ptr<virNWFilterImpl> virHelper::openNWFilterByUUIDString(const char * uuidstr) {
  if (!conn_) return nullptr;
  virNWFilterPtr nwfilterPtr = virNWFilterLookupByUUIDString(conn_.get(), uuidstr);
  if (nullptr == nwfilterPtr) return nullptr;
  return std::make_shared<virNWFilterImpl>(nwfilterPtr);
}

int virHelper::numOfNWFilters() {
  if (!conn_) return -1;
  return virConnectNumOfNWFilters(conn_.get());
}

int virHelper::listNWFilters(std::vector<std::string> &names, int maxnames) {
  if (!conn_ || maxnames < 1) return -1;
  char **ns = (char**)malloc(sizeof(char*) * maxnames);
  int nlen = 0;
  // libvirt官网不鼓励使用此API，而是推荐使用virConnectListAllNWFilters()。
  if ((nlen = virConnectListNWFilters(conn_.get(), ns, maxnames)) < 0)
    goto cleanup;
  // for (int i = 0; i < nlen; i++) {
  //   DebugPrintf("connect nwfilter names[%d]: %s\n", i, ns[i]);
  // }
cleanup:
  if (ns && nlen > 0) {
    for (int i = 0; i < nlen; i++) {
      names.push_back(ns[i]);
      // free(ns[i]);
    }
  }
  if (ns)
    free(ns);
  return nlen;
}

int virHelper::listAllNWFilters(std::vector<std::shared_ptr<virNWFilterImpl>> &filters, unsigned int flags) {
  if (!conn_) return -1;
  virNWFilterPtr *nwfilters = nullptr;
  int filters_count = 0;
  if ((filters_count = virConnectListAllNWFilters(conn_.get(), &nwfilters, flags)) < 0)
    goto cleanup;

cleanup:
  if (nwfilters && filters_count > 0) {
    for (int i = 0; i < filters_count; i++) {
      filters.push_back(std::make_shared<virNWFilterImpl>(nwfilters[i]));
    }
  }
  if (nwfilters)
    free(nwfilters);
  return filters_count;
}

void virHelper::DefaultThreadFunc() {
  DebugPrintf("vir event loop thread begin\n");
  while (thread_quit_ == 0) {
    if (virEventRunDefaultImpl() < 0) {
      std::shared_ptr<virError> err = getLastError();
      DebugPrintf("virEventRunDefaultImpl failed: %s\n", err ? err->message : "");
    }
  }
  DebugPrintf("vir event loop thread end\n");
}

}
