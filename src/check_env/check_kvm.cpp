#include "check_kvm.h"
#include "tinyxml2.h"
#include <sstream>
#include <uuid/uuid.h>
#include <boost/filesystem.hpp>
#include "util/SystemResourceManager.h"
#include <sys/sysinfo.h>

namespace check_kvm {
    static std::vector<std::string> SplitStr(const std::string &s, const char &c) {
        std::string buff;
        std::vector<std::string> v;
        char tmp;
        for (int i = 0; i < s.size(); i++) {
            tmp = s[i];
            if (tmp != c) {
                buff += tmp;
            } else {
                if (tmp == c && buff != "") {
                    v.push_back(buff);
                    buff = "";
                }
            }//endif
        }
        if (buff != "") {
            v.push_back(buff);
        }
        return v;
    }

    static std::string createXmlStr(const std::string &uuid, const std::string &domain_name,
                                    int64_t memory, int32_t cpunum, int32_t sockets, int32_t cores, int32_t threads,
                                    const std::string &vedio_pci, const std::string &image_file,
                                    const std::string &disk_file) {
        tinyxml2::XMLDocument doc;

        // <domain>
        tinyxml2::XMLElement *root = doc.NewElement("domain");
        root->SetAttribute("type", "kvm");
        root->SetAttribute("xmlns:qemu", "http://libvirt.org/schemas/domain/qemu/1.0");
        doc.InsertEndChild(root);

        tinyxml2::XMLElement *name_node = doc.NewElement("name");
        name_node->SetText(domain_name.c_str());
        root->LinkEndChild(name_node);

        tinyxml2::XMLElement *uuid_node = doc.NewElement("uuid");
        uuid_node->SetText(uuid.c_str());
        root->LinkEndChild(uuid_node);

        // memory
        tinyxml2::XMLElement *max_memory_node = doc.NewElement("memory");
        max_memory_node->SetAttribute("unit", "KiB");
        max_memory_node->SetText(std::to_string(memory).c_str());
        root->LinkEndChild(max_memory_node);

        tinyxml2::XMLElement *memory_node = doc.NewElement("currentMemory");
        memory_node->SetAttribute("unit", "KiB");
        memory_node->SetText(std::to_string(memory).c_str());
        root->LinkEndChild(memory_node);

        // vcpu
        tinyxml2::XMLElement *vcpu_node = doc.NewElement("vcpu");
        vcpu_node->SetAttribute("placement", "static");
        vcpu_node->SetText(std::to_string(cpunum).c_str());
        root->LinkEndChild(vcpu_node);

        // <os>
        tinyxml2::XMLElement *os_node = doc.NewElement("os");
        tinyxml2::XMLElement *os_sub_node = doc.NewElement("type");
        os_sub_node->SetAttribute("arch", "x86_64");
        os_sub_node->SetAttribute("machine", "pc-1.2");
        os_sub_node->SetText("hvm");
        os_node->LinkEndChild(os_sub_node);

        tinyxml2::XMLElement *os_sub_node2 = doc.NewElement("boot");
        os_sub_node2->SetAttribute("dev", "hd");
        os_node->LinkEndChild(os_sub_node2);

        tinyxml2::XMLElement *os_sub_node3 = doc.NewElement("boot");
        os_sub_node3->SetAttribute("dev", "cdrom");
        os_node->LinkEndChild(os_sub_node3);
        root->LinkEndChild(os_node);

        // <features>
        tinyxml2::XMLElement *features_node = doc.NewElement("features");
        tinyxml2::XMLElement *features_sub_node1 = doc.NewElement("acpi");
        features_node->LinkEndChild(features_sub_node1);
        tinyxml2::XMLElement *features_sub_node2 = doc.NewElement("apic");
        features_node->LinkEndChild(features_sub_node2);
        tinyxml2::XMLElement *features_sub_node3 = doc.NewElement("kvm");
        tinyxml2::XMLElement *node_hidden = doc.NewElement("hidden");
        node_hidden->SetAttribute("state", "on");
        features_sub_node3->LinkEndChild(node_hidden);
        features_node->LinkEndChild(features_sub_node3);
        root->LinkEndChild(features_node);

        // <cpu>
        tinyxml2::XMLElement *cpu_node = doc.NewElement("cpu");
        cpu_node->SetAttribute("mode", "host-passthrough");
        cpu_node->SetAttribute("check", "none");
        tinyxml2::XMLElement *node_topology = doc.NewElement("topology");
        node_topology->SetAttribute("sockets", std::to_string(sockets).c_str());
        node_topology->SetAttribute("cores", std::to_string(cores).c_str());
        node_topology->SetAttribute("threads", std::to_string(threads).c_str());
        cpu_node->LinkEndChild(node_topology);
        tinyxml2::XMLElement *node_cache = doc.NewElement("cache");
        node_cache->SetAttribute("mode", "passthrough");
        cpu_node->LinkEndChild(node_cache);
        root->LinkEndChild(cpu_node);

        tinyxml2::XMLElement *clock_node = doc.NewElement("clock");
        clock_node->SetAttribute("offset", "utc");
        root->LinkEndChild(clock_node);

        // <pm>
        /*
        tinyxml2::XMLElement* node_pm = doc.NewElement("pm");
        tinyxml2::XMLElement* node_suspend_to_mem = doc.NewElement("suspend-to-mem");
        node_suspend_to_mem->SetAttribute("enabled", "on");
        node_pm->LinkEndChild(node_suspend_to_mem);
        tinyxml2::XMLElement* node_suspend_to_disk = doc.NewElement("suspend-to-disk");
        node_suspend_to_disk->SetAttribute("enabled", "on");
        node_pm->LinkEndChild(node_suspend_to_disk);
        root->LinkEndChild(node_pm);
        */

        tinyxml2::XMLElement *dev_node = doc.NewElement("devices");
        /*
        tinyxml2::XMLElement* vedio_node = doc.NewElement("video");
        tinyxml2::XMLElement* model_node = doc.NewElement("model");
        model_node->SetAttribute("type", "vga");
        model_node->SetAttribute("vram", "16384");
        model_node->SetAttribute("heads", "1");
        vedio_node->LinkEndChild(model_node);
        dev_node->LinkEndChild(vedio_node);
        */

        if (vedio_pci != "") {
            std::vector<std::string> vedios = SplitStr(vedio_pci, '|');
            for (int i = 0; i < vedios.size(); ++i) {
                std::vector<std::string> infos = SplitStr(vedios[i], ':');
                if (infos.size() != 2) {
                    std::cout << vedios[i] << "  error" << std::endl;
                    continue;
                }

                std::vector<std::string> infos2 = SplitStr(infos[1], '.');
                if (infos2.size() != 2) {
                    std::cout << vedios[i] << "  error" << std::endl;
                    continue;
                }

                tinyxml2::XMLElement *hostdev_node = doc.NewElement("hostdev");
                hostdev_node->SetAttribute("mode", "subsystem");
                hostdev_node->SetAttribute("type", "pci");
                hostdev_node->SetAttribute("managed", "yes");

                tinyxml2::XMLElement *source_node = doc.NewElement("source");
                tinyxml2::XMLElement *address_node = doc.NewElement("address");
                address_node->SetAttribute("type", "pci");
                address_node->SetAttribute("domain", "0x0000");
                address_node->SetAttribute("bus", ("0x" + infos[0]).c_str());
                address_node->SetAttribute("slot", ("0x" + infos2[0]).c_str());
                address_node->SetAttribute("function", ("0x" + infos2[1]).c_str());
                source_node->LinkEndChild(address_node);

                hostdev_node->LinkEndChild(source_node);
                dev_node->LinkEndChild(hostdev_node);
            }
        }

        // disk (image)
        tinyxml2::XMLElement *image_node = doc.NewElement("disk");
        image_node->SetAttribute("type", "file");
        image_node->SetAttribute("device", "disk");

        tinyxml2::XMLElement *driver_node = doc.NewElement("driver");
        driver_node->SetAttribute("name", "qemu");
        driver_node->SetAttribute("type", "qcow2");
        image_node->LinkEndChild(driver_node);

        tinyxml2::XMLElement *source_node = doc.NewElement("source");
        source_node->SetAttribute("file", image_file.c_str());
        image_node->LinkEndChild(source_node);

        tinyxml2::XMLElement *target_node = doc.NewElement("target");
        target_node->SetAttribute("dev", "hda");
        target_node->SetAttribute("bus", "ide");
        image_node->LinkEndChild(target_node);
        dev_node->LinkEndChild(image_node);

        // disk (data)
        tinyxml2::XMLElement *disk_data_node = doc.NewElement("disk");
        disk_data_node->SetAttribute("type", "file");

        tinyxml2::XMLElement *disk_driver_node = doc.NewElement("driver");
        disk_driver_node->SetAttribute("name", "qemu");
        disk_driver_node->SetAttribute("type", "qcow2");
        disk_data_node->LinkEndChild(disk_driver_node);

        tinyxml2::XMLElement *disk_source_node = doc.NewElement("source");
        disk_source_node->SetAttribute("file", disk_file.c_str());
        disk_data_node->LinkEndChild(disk_source_node);

        tinyxml2::XMLElement *disk_target_node = doc.NewElement("target");
        disk_target_node->SetAttribute("dev", "vda");
        disk_target_node->SetAttribute("bus", "virtio");
        disk_data_node->LinkEndChild(disk_target_node);

        dev_node->LinkEndChild(disk_data_node);

        // qemu_guest_agent
        tinyxml2::XMLElement *agent_node = doc.NewElement("channel");
        agent_node->SetAttribute("type", "unix");
        tinyxml2::XMLElement *agent_source_node = doc.NewElement("source");
        agent_source_node->SetAttribute("mode", "bind");
        agent_source_node->SetAttribute("path", "/tmp/channel.sock");
        agent_node->LinkEndChild(agent_source_node);
        tinyxml2::XMLElement *agent_target_node = doc.NewElement("target");
        agent_target_node->SetAttribute("type", "virtio");
        agent_target_node->SetAttribute("name", "org.qemu.guest_agent.0");
        agent_node->LinkEndChild(agent_target_node);
        dev_node->LinkEndChild(agent_node);

        // network
        tinyxml2::XMLElement *interface_node = doc.NewElement("interface");
        interface_node->SetAttribute("type", "network");
        tinyxml2::XMLElement *interface_source_node = doc.NewElement("source");
        interface_source_node->SetAttribute("network", "default");
        interface_node->LinkEndChild(interface_source_node);
        dev_node->LinkEndChild(interface_node);

        // vnc
        tinyxml2::XMLElement *graphics_node = doc.NewElement("graphics");
        graphics_node->SetAttribute("type", "vnc");
        graphics_node->SetAttribute("port", "-1");
        graphics_node->SetAttribute("autoport", "yes");
        graphics_node->SetAttribute("listen", "0.0.0.0");
        graphics_node->SetAttribute("keymap", "en-us");
        graphics_node->SetAttribute("passwd", "dbtu@supper2017");
        tinyxml2::XMLElement *listen_node = doc.NewElement("listen");
        listen_node->SetAttribute("type", "address");
        listen_node->SetAttribute("address", "0.0.0.0");
        graphics_node->LinkEndChild(listen_node);
        dev_node->LinkEndChild(graphics_node);
        root->LinkEndChild(dev_node);

        // cpu (qemu:commandline)
        tinyxml2::XMLElement *node_qemu_commandline = doc.NewElement("qemu:commandline");
        tinyxml2::XMLElement *node_qemu_arg1 = doc.NewElement("qemu:arg");
        node_qemu_arg1->SetAttribute("value", "-cpu");
        node_qemu_commandline->LinkEndChild(node_qemu_arg1);
        tinyxml2::XMLElement *node_qemu_arg2 = doc.NewElement("qemu:arg");
        node_qemu_arg2->SetAttribute("value", "host");
        node_qemu_commandline->LinkEndChild(node_qemu_arg2);
        root->LinkEndChild(node_qemu_commandline);

        //doc.SaveFile("domain.xml");
        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        return printer.CStr();
    }

    static std::string getUrl() {
        std::stringstream sstream;
        sstream << "qemu+tcp://";
        sstream << "localhost";
        sstream << ":";
        sstream << 16509;
        sstream << "/system";
        return sstream.str();
    }

    bool CreateDomain(const std::string &domain_name, const std::string &image_name,
                         int32_t sockets, int32_t cores_per_socket, int32_t threads_per_core,
                         const std::string& vga_pci, int64_t mem) {
        bool ret = true;

        std::cout << "domain_name: " << domain_name << std::endl;
        std::cout << "image: " << image_name << std::endl;

        // cpu
        long cpuNumTotal = sockets * cores_per_socket * threads_per_core;
        std::cout << "cpu: " << cpuNumTotal << "(" << sockets << ", " << cores_per_socket << ", " << threads_per_core
                  << ")" << std::endl;

        // gpu
        std::cout << "gpu: " << vga_pci << std::endl;

        // mem
        uint64_t memoryTotal = mem > 1000000000 ? 1000000000 : mem; // KB
        std::cout << "mem: " << memoryTotal << "KB" << std::endl;

        // uuid
        uuid_t uu;
        char buf_uuid[1024] = {0};
        uuid_generate(uu);
        uuid_unparse(uu, buf_uuid);
        std::cout << "uuid: " << buf_uuid << std::endl;

        // 复制一份镜像（系统盘）
        std::string from_image_path = "/data/" + image_name;
        auto pos = image_name.find('.');
        std::string to_image_name = image_name;
        std::string to_ext;
        if (pos != std::string::npos) {
            to_image_name = image_name.substr(0, pos);
            to_ext = image_name.substr(pos + 1);
        }
        std::string to_image_path = "/data/" + to_image_name + "_" + domain_name + "." + to_ext;
        std::cout << "image_file: " << to_image_path << std::endl;
        boost::filesystem::copy_file(from_image_path, to_image_path);

        // 创建虚拟磁盘（数据盘）
        std::string data_file = "/data/data_1_" + domain_name + ".qcow2";
        int64_t disk_total_size = SystemResourceMgr::instance().GetDisk().total / 1024; // GB
        disk_total_size = (disk_total_size - 350) * 0.1;
        std::cout << "data_file: " << data_file << std::endl;
        std::string cmd_create_img =
                "qemu-img create -f qcow2 " + data_file + " " + std::to_string(disk_total_size) + "G";
        std::cout << "create qcow2 image(data): " << cmd_create_img << std::endl;
        std::string create_ret = run_shell(cmd_create_img.c_str());
        std::cout << create_ret << std::endl;

        std::string xml_content = createXmlStr(buf_uuid, domain_name, memoryTotal,
                                               cpuNumTotal, sockets, cores_per_socket, threads_per_core,
                                               vga_pci, to_image_path, data_file);

        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;
        int32_t errorNum = 0;
        do {
            std::string url = getUrl();
            connPtr = virConnectOpen(url.c_str());
            if (nullptr == connPtr) {
                ret = false;
                std::cout << "virConnectOpen error: " << url << std::endl;
                break;
            }

            domainPtr = virDomainDefineXML(connPtr, xml_content.c_str());
            if (nullptr == domainPtr) {
                ret = false;

                virErrorPtr error = virGetLastError();
                std::cout << "virDomainDefineXML error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            if (virDomainCreate(domainPtr) < 0) {
                ret = false;

                virErrorPtr error = virGetLastError();
                std::cout << "virDomainCreate error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }
        } while (0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }

        return ret;
    }

    EVmStatus GetDomainStatus(const std::string &domain_name) {
        EVmStatus vm_status = VS_SHUT_OFF;
        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;

        do {
            std::string url = getUrl();
            connPtr = virConnectOpen(url.c_str());
            if (nullptr == connPtr) {
                virErrorPtr error = virGetLastError();
                std::cout << "virConnectOpen error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            domainPtr = virDomainLookupByName(connPtr, domain_name.c_str());
            if (nullptr == domainPtr) {
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            virDomainInfo info;
            if (virDomainGetInfo(domainPtr, &info) < 0) {
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainGetInfo error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            if (info.state == VIR_DOMAIN_RUNNING) {
                vm_status = VS_RUNNING;
            } else if (info.state == VIR_DOMAIN_PAUSED) {
                vm_status = VS_PAUSED;
            } else if (info.state == VIR_DOMAIN_SHUTOFF) {
                vm_status = VS_SHUT_OFF;
            } else {
                vm_status = VS_SHUT_OFF;
            }
        } while(0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }


        return vm_status;
    }

    bool DestoryDomain(const std::string &domain_name) {
        bool ret = true;
        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;

        do {
            std::string url = getUrl();
            connPtr = virConnectOpen(url.c_str());
            if (nullptr == connPtr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainGetInfo error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            domainPtr = virDomainLookupByName(connPtr, domain_name.c_str());
            if (nullptr == domainPtr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            if (virDomainDestroy(domainPtr) < 0) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }
        } while(0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }

        return ret;
    }

    bool UndefineDomain(const std::string &domain_name) {
        bool ret = true;
        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;

        do {
            std::string url = getUrl();
            connPtr = virConnectOpen(url.c_str());
            if (nullptr == connPtr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virConnectOpen error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            domainPtr = virDomainLookupByName(connPtr, domain_name.c_str());
            if (nullptr == domainPtr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            if (virDomainUndefine(domainPtr) < 0) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainUndefine error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }
        } while(0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }

        return ret;
    }


    //////////////////////////////////////////////////////////////////////////////////
    std::string get_vm_local_ip(const std::string& domain_name) {
        std::string vm_local_ip;
        virConnectPtr connPtr = nullptr;
        virDomainPtr domainPtr = nullptr;
        do {
            connPtr = virConnectOpen("qemu+tcp://localhost:16509/system");
            if (nullptr == connPtr) {
                virErrorPtr error = virGetLastError();
                std::cout << "virConnectOpen error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            domainPtr = virDomainLookupByName(connPtr, domain_name.c_str());
            if (nullptr == domainPtr) {
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            int32_t try_count = 0;
            // max: 15min
            while (vm_local_ip.empty() && try_count < 300) {
                std::cout << "get vm_local_ip try_count: " << (try_count + 1) << std::endl;
                virDomainInterfacePtr *ifaces = nullptr;
                int ifaces_count = virDomainInterfaceAddresses(domainPtr, &ifaces,
                                                               VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_LEASE, 0);
                for (int i = 0; i < ifaces_count; i++) {
                    /*
                    std::string if_name = ifaces[i]->name;
                    std::string if_hwaddr;
                    if (ifaces[i]->hwaddr)
                        if_hwaddr = ifaces[i]->hwaddr;
                    */
                    for (int j = 0; j < ifaces[i]->naddrs; j++) {
                        virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
                        vm_local_ip = ip_addr->addr;
                        if (!vm_local_ip.empty()) break;
                        //printf("[addr: %s prefix: %d type: %d]", ip_addr->addr, ip_addr->prefix, ip_addr->type);
                    }

                    if (!vm_local_ip.empty()) break;
                }

                try_count += 1;
                sleep(3);
            }
        } while (0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }

        if (nullptr != connPtr) {
            virConnectClose(connPtr);
        }

        return vm_local_ip;
    }

    void transform_port(const std::string &public_ip, const std::string &transform_port,
                        const std::string &vm_local_ip) {
        std::string cmd;
        cmd += "sudo iptables --table nat -I PREROUTING --protocol tcp --destination " + public_ip +
                " --destination-port " + transform_port + " --jump DNAT --to-destination " + vm_local_ip + ":22";
        cmd += " && sudo iptables -t nat -I PREROUTING -p tcp --dport " + transform_port +
                " -j DNAT --to-destination " + vm_local_ip + ":22";
        auto pos = vm_local_ip.rfind('.');
        std::string ip = vm_local_ip.substr(0, pos) + ".1";
        cmd += " && sudo iptables -t nat -I POSTROUTING -p tcp --dport " + transform_port + " -d " + vm_local_ip +
                " -j SNAT --to " + ip;
        cmd += " && sudo iptables -t nat -I PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -I PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -I POSTROUTING -d " + vm_local_ip +
                " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + public_ip;
        cmd += " && sudo iptables -t nat -I POSTROUTING -d " + vm_local_ip +
                " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + public_ip;

        run_shell(cmd.c_str());
    }

    bool set_vm_password(const std::string &domain_name, const std::string &username, const std::string &pwd) {
        bool ret = false;
        virConnectPtr conn_ptr = nullptr;
        virDomainPtr domain_ptr = nullptr;
        do {
            conn_ptr = virConnectOpen("qemu+tcp://localhost/system");
            if (conn_ptr == nullptr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "connect virt error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            domain_ptr = virDomainLookupByName(conn_ptr, domain_name.c_str());
            if (domain_ptr == nullptr) {
                ret = false;
                virErrorPtr error = virGetLastError();
                std::cout << "virDomainLookupByName error: " << error->message << std::endl;
                virFreeError(error);
                break;
            }

            int try_count = 0;
            int succ = -1;
            // max: 5min
            while (succ != 0 && try_count < 100) {
                std::cout << "set_vm_password try_count: " << (try_count + 1) << std::endl;
                succ = virDomainSetUserPassword(domain_ptr, username.c_str(), pwd.c_str(), 0);
                if (succ != 0) {
                    virErrorPtr error = virGetLastError();
                    std::cout << "virDomainSetUserPassword error: " << error->message << std::endl;
                    virFreeError(error);
                }

                try_count++;
                sleep(3);
            }

            if (succ == 0) {
                ret = true;
                std::cout << "set vm password successful, task_id:" << domain_name << ", user:" << username << ", pwd:"
                << pwd << std::endl;
            } else {
                ret = false;
                std::cout << "set vm password failed, task_id:" << domain_name << ", user:" << username << ", pwd:"
                << pwd << std::endl;
            }
        } while(0);

        if (domain_ptr != nullptr) {
            virDomainFree(domain_ptr);
        }

        if (conn_ptr != nullptr) {
            virConnectClose(conn_ptr);
        }

        return ret;
    }

    void delete_image_file(const std::string &task_id, const std::string& image_name) {
        std::string image = image_name;
        auto pos = image.find('.');
        std::string real_image_name = image;
        std::string ext;
        if (pos != std::string::npos) {
            real_image_name = image.substr(0, pos);
            ext = image.substr(pos + 1);
        }
        std::string real_image_path = "/data/" + real_image_name + "_" + task_id + "." + ext;
        std::cout << "remove image file: " << real_image_path << std::endl;
        remove(real_image_path.c_str());
    }

    void delete_disk_file(const std::string &task_id) {
        std::string disk_file = "/data/data_1_" + task_id + ".qcow2";
        std::cout << "remove disk file: " << disk_file << std::endl;
        remove(disk_file.c_str());
    }

    void delete_iptable(const std::string &public_ip, const std::string &transform_port,
                        const std::string &vm_local_ip) {
        std::string cmd;
        cmd += "sudo iptables --table nat -D PREROUTING --protocol tcp --destination " + public_ip +
                " --destination-port " + transform_port + " --jump DNAT --to-destination " + vm_local_ip + ":22";
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp --dport " + transform_port +
                " -j DNAT --to-destination " + vm_local_ip + ":22";
        auto pos = vm_local_ip.rfind('.');
        std::string ip = vm_local_ip.substr(0, pos) + ".1";
        cmd += " && sudo iptables -t nat -D POSTROUTING -p tcp --dport " + transform_port + " -d " + vm_local_ip +
                " -j SNAT --to " + ip;
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -D PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
                " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + public_ip;
        cmd += " && sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
                " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + public_ip;

        run_shell(cmd.c_str());
    }

    void delete_domain(const std::string& domain_name, const std::string& image_name,
                       const std::string& public_ip, const std::string& ssh_port, const std::string& vm_local_ip) {
        EVmStatus vm_status = GetDomainStatus(domain_name);
        if (vm_status == VS_SHUT_OFF) {
            if (UndefineDomain(domain_name)) {
                delete_image_file(domain_name, image_name);
                delete_disk_file(domain_name);
                delete_iptable(public_ip, ssh_port, vm_local_ip);

                std::cout << "delete task " << domain_name << " successful" << std::endl;
            } else {
                std::cout << "delete task " << domain_name << " failed" << std::endl;
            }
        } else if (vm_status == VS_RUNNING) {
            if (DestoryDomain(domain_name)) {
                sleep(1);
                if (UndefineDomain(domain_name)) {
                    delete_image_file(domain_name, image_name);
                    delete_disk_file(domain_name);
                    delete_iptable(public_ip, ssh_port, vm_local_ip);

                    std::cout << "delete task " << domain_name << " successful" << std::endl;
                } else {
                    std::cout << "delete task " << domain_name << " failed" << std::endl;
                }
            } else {
                std::cout << "delete task " << domain_name << " failed" << std::endl;
            }
        }
    }

    void test_kvm() {
        std::string domain_name = "dbc_check_env_vm_x";
        std::string image_name = "ubuntu.qcow2";

        std::string ssh_port = "6789";

        int sockets = 2;
        int cores = 5;
        int threads = 2;

        struct sysinfo info{};
        sysinfo(&info);
        int64_t memory_total = info.totalram/1024;
        memory_total *= 0.1;

        std::string cmd = "lspci |grep NVIDIA |grep -E 'VGA|Audio|USB|Serial bus' | awk '{print $1}' |tr \"\n\" \"|\"";
        std::string vga_gpu = run_shell(cmd.c_str());

        std::string public_ip = get_public_ip();
        if (public_ip.empty()) {
            std::cout << "public_ip is empty" << std::endl;
            return;
        }
        std::cout << "public_ip: " << public_ip << std::endl;

        std::string vm_local_ip;

        if (CreateDomain(domain_name, image_name, sockets, cores, threads,
                              vga_gpu, memory_total)) {
            vm_local_ip = get_vm_local_ip(domain_name);
            if (vm_local_ip.empty()) {
                std::cout << "get vm_local_ip is empty" << std::endl;
                delete_image_file(domain_name, image_name);
                delete_disk_file(domain_name);
                return;
            }
            std::cout << "vm_local_ip: " << vm_local_ip << std::endl;

            transform_port(public_ip, ssh_port, vm_local_ip);

            if (!set_vm_password(domain_name, "dbc", "vm123456")) {
                std::cout << "set_vm_password failed" << std::endl;
                delete_image_file(domain_name, image_name);
                delete_disk_file(domain_name);
                delete_iptable(public_ip, ssh_port, vm_local_ip);
                return;
            }

            print_green("create vm %s successful", domain_name.c_str());
        } else {
            print_red("create vm %s failed", domain_name.c_str());
        }

        sleep(15);

        delete_domain(domain_name, image_name, public_ip, ssh_port, vm_local_ip);
    }
}