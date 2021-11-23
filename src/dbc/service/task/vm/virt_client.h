#ifndef DBC_VIRT_CLIENT_H
#define DBC_VIRT_CLIENT_H

#include "util/utils.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

const double DEFAULT_PERCENTAGE = 0.95;
enum vm_status {
    vm_unknown = 0,
    vm_running,
    vm_closed,
    vm_suspended,
};

typedef struct MEMPACKED         //¶¨ÒåÒ»¸ömem occupyµÄ½á¹¹Ìå  
{
    char name1[20];      //¶¨ÒåÒ»¸öcharÀàÐÍµÄÊý×éÃûnameÓÐ20¸öÔªËØ  
    unsigned long MemTotal;
    char name2[20];
    unsigned long MemFree;
    char name3[20];
    unsigned long Buffers;
    char name4[20];
    unsigned long Cached;
    char name5[20];
    unsigned long SwapCached;
} MEM_OCCUPY;

class virt_client {
public:
    virt_client(std::string ip, uint16_t port);

    virtual ~virt_client() = default;

    void set_address(const std::string& ip, uint16_t port);

    int32_t createDomain(const std::string &name, const std::string& host_ip,
                         const std::string &transform_port, const std::string &image_path);

    int32_t startDomain(const std::string &domainName);

    int32_t suspendDomain(const std::string &domainName);

    int32_t resumeDomain(const std::string &domainName);

    int32_t rebootDomain(const std::string &domainName);

    int32_t shutdownDomain(const std::string &domainName);

    int32_t destoryDomain(const std::string &domainName, bool force = true);

    int32_t undefineDomain(const std::string &domainName);

    int32_t listAllDomain(std::vector<std::string> &nameArray);

    int32_t listAllRunningDomain(std::vector<std::string> &nameArray);

    bool existDomain(std::string &domainName);

    vm_status getDomainStatus(std::string &domainName);

protected:
    std::string getUrl();

    int32_t createDomainByXML(const std::string &filepath);

    int32_t listDomainByFlags(unsigned int flags, std::vector<std::string> &nameArray);

    void shell_transform_port(const std::string &host_ip, const std::string &transform_port);

    std::string shell_vga_pci_list();

private:
    std::string m_ip;
    uint16_t m_port = 0;
};

#endif
