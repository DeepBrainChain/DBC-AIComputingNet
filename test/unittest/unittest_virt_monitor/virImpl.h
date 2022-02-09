#ifndef DBCPROJ_UNITTEST_VIR_VIRIMPL_H
#define DBCPROJ_UNITTEST_VIR_VIRIMPL_H

#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

std::shared_ptr<virError> getLastError();

namespace test {
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

class virDomainImpl {
public:
    virDomainImpl() = delete;
    explicit virDomainImpl(virDomainPtr domain);
    ~virDomainImpl();

    int32_t startDomain();

    int32_t suspendDomain();

    int32_t resumeDomain();

    int32_t rebootDomain(int flag = 0);

    int32_t shutdownDomain();

    int32_t destroyDomain();

    int32_t resetDomain();

    int32_t undefineDomain();

    int32_t deleteDomain();

    /**
     * @brief Determine if the domain is currently running
     * 
     * @return 1 if running, 0 if inactive, -1 on error
     */
    int32_t isDomainActive();

    int32_t getDomainDisks(std::vector<std::string> &disks);

    int32_t getDomainInterfaceAddress(std::vector<test::virDomainInterface> &difaces);

    int32_t setDomainUserPassword(const char *user, const char *password);

    int32_t getDomainInfo(virDomainInfoPtr info);

    int32_t getDomainCPUStats(virTypedParameterPtr params, unsigned int nparams, int start_cpu, unsigned int ncpus, unsigned int flags);

    int32_t getDomainMemoryStats(virDomainMemoryStatPtr stats, unsigned int nr_stats, unsigned int flags);

    int32_t getDomainBlockInfo(const char *disk, virDomainBlockInfoPtr info, unsigned int flags);

    int32_t getDomainBlockStats(const char *disk, virDomainBlockStatsPtr stats, size_t size);

    int32_t getDomainNetworkStats(const char *device, virDomainInterfaceStatsPtr stats, size_t size);

protected:
    std::shared_ptr<virDomain> domain_;
};

class virTool {
public:
    virTool();
    ~virTool();

    bool openConnect(const char *name);

    std::shared_ptr<virDomainImpl> defineDomain(const char *xml_content);

    std::shared_ptr<virDomainImpl> openDomain(const char *domain_name);

protected:
    void DefaultThreadFunc();

protected:
    std::shared_ptr<virConnect> conn_;
    int dom_event_lifecycle_callback_id_;
    int dom_event_agent_callback_id_;
    int thread_quit_;
    std::thread *thread_event_loop_;
};

#endif
