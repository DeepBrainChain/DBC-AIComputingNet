#ifndef DBCPROJ_CHECK_KVM_H
#define DBCPROJ_CHECK_KVM_H

#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

namespace check_kvm {
    struct DeviceCpu {
        int32_t sockets = 0;
        int32_t cores_per_socket = 0;
        int32_t threads_per_core = 0;
    };

    struct DeviceGpu {
        std::string id;
        std::list<std::string> devices;
    };

    enum EVmStatus {
        VS_SHUT_OFF,
        VS_PAUSED,
        VS_RUNNING
    };

    bool CreateDomain(const std::string& domain_name, const std::string& image,
                         int32_t sockets, int32_t cores_per_socket, int32_t threads_per_core,
                         const std::map<std::string, DeviceGpu>& mpGpu, int64_t mem);

    int32_t StartDomain(const std::string& domain_name);

    int32_t SuspendDomain(const std::string& domain_name);

    int32_t ResumeDomain(const std::string& domain_name);

    int32_t RebootDomain(const std::string& domain_name);

    int32_t ShutdownDomain(const std::string& domain_name);

    bool DestoryDomain(const std::string& domain_name);

    int32_t ResetDomain(const std::string& domain_name);

    bool UndefineDomain(const std::string& domain_name);

    void ListAllDomains(std::vector<std::string>& domains);

    void ListAllRunningDomains(std::vector<std::string>& domains);

    EVmStatus GetDomainStatus(const std::string& domain_name);

    void test_kvm();
}

#endif //DBCPROJ_CHECK_KVM_H
