#ifndef DBCPROJ_CHECK_ENV_VIRIMPL_H
#define DBCPROJ_CHECK_ENV_VIRIMPL_H

#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

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

    int32_t hasNvram();

    int32_t getDomainDisks(std::vector<std::string> &disks);

    int32_t getDomainInterfaceAddress(std::string &local_ip);

    int32_t setDomainUserPassword(const char *user, const char *password);

    int32_t getSnapshotNums(unsigned int flags);

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
    int callback_id_;
    int agent_callback_id_;
    int thread_quit_;
    std::thread *thread_event_loop_;
};

#endif
