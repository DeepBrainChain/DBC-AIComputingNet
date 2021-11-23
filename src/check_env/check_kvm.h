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

    std::string preCreateDomain(const std::string& domain_name, const std::string& image,
                         int32_t sockets, int32_t cores_per_socket, int32_t threads_per_core,
                         const std::map<std::string, DeviceGpu>& mpGpu, int64_t mem, int32_t vnc_port, const std::string& vnc_pwd);

    void test_kvm(int argc, char** argv);
}

#endif //DBCPROJ_CHECK_KVM_H
