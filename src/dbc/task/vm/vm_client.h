#ifndef DBC_VM_CLIENT_H
#define DBC_VM_CLIENT_H

#include "util/utils.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "task/detail/TaskMonitorInfo.h"
#include "task/detail/info/TaskInfo.h"

namespace dbc {
typedef struct virDomainInterfaceIPAddress virDomainIPAddress;
struct virDomainInterfaceIPAddress {
    int type;                /* virIPAddrType */
    std::string addr;        /* IP address */
    unsigned int prefix;     /* IP address prefix */
};
struct virDomainInterface {
    std::string name;               /* interface name */
    std::string hwaddr;             /* hardware address, may be NULL */
    std::vector<virDomainIPAddress> addrs;    /* array of IP addresses */
};
}

struct domainDiskInfo {
	std::string driverName;
	std::string driverType;
	std::string sourceFile;
	std::string targetDev;
	std::string targetBus;
};

struct domainInterface {
    std::string name;
    std::string type;
    std::string mac;
};

class VmClient : public Singleton<VmClient> {
public:
    VmClient();

    virtual ~VmClient();

    bool init();

    void exit();

    // domain
	bool IsExistDomain(const std::string& domain_name);

    int32_t CreateDomain(const std::shared_ptr<TaskInfo>& taskinfo);

    int32_t StartDomain(const std::string& domain_name);

    int32_t SuspendDomain(const std::string& domain_name);

    int32_t ResumeDomain(const std::string& domain_name);

    int32_t RebootDomain(const std::string& domain_name);

    int32_t ShutdownDomain(const std::string& domain_name);

    int32_t DestroyDomain(const std::string& domain_name);

    int32_t ResetDomain(const std::string& domain_name);

    int32_t UndefineDomain(const std::string& domain_name);

    FResult RedefineDomain(const std::shared_ptr<TaskInfo>& taskinfo);

    int32_t DestroyAndUndefineDomain(const std::string& domain_name, unsigned int undefineFlags = 0);

    virDomainState GetDomainStatus(const std::string& domain_name);

    void ListAllDomains(std::vector<std::string>& domains);

    void ListAllRunningDomains(std::vector<std::string>& domains);

    FResult GetDomainLog(const std::string& domain_name, QUERY_LOG_DIRECTION direction, int32_t linecount, std::string &log_content);

	FResult SetDomainUserPassword(const std::string& domain_name, const std::string& username, const std::string& pwd, int max_retry_count = 100);

    bool ListDomainDiskInfo(const std::string& domain_name, std::map<std::string, domainDiskInfo>& disks);

    // network
    std::string GetDomainLocalIP(const std::string &domain_name);

    int32_t GetDomainInterfaceAddress(const std::string& domain_name, std::vector<dbc::virDomainInterface> &difaces, unsigned int source = 0);
    
    int32_t IsDomainHasNvram(const std::string& domain_name);

    FResult ListDomainInterface(const std::string& domain_name, std::vector<domainInterface>& interfaces);

    // xml
    std::string GetDomainXML(const std::string& domain_name);

    // disk
    int64_t GetDiskVirtualSize(const std::string& domain_name, const std::string& disk_name);

    FResult AttachDisk(const std::string& domain_name, const std::string& disk_name, const std::string& source_file);

    FResult DetachDisk(const std::string& domain_name, const std::string& disk_name);

    // qemu command
    std::string QemuAgentCommand(const std::string& domain_name, const std::string& cmd, int timeout, unsigned int flags);

    // monitor data
    bool GetDomainMonitorData(const std::string& domain_name, dbcMonitor::domMonitorData& data);

    // nwfilter
    int32_t DefineNWFilter(const std::string& nwfilter_name, const std::vector<std::string>& nwfilters);

    int32_t UndefineNWFilter(const std::string& nwfilter_name);

private:
    virConnectPtr m_connPtr = nullptr;
};

#endif // !DBC_VM_CLIENT_H
