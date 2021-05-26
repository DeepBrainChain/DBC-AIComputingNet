/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   virt_client.cpp
* description    :   virt client for definition
* date                  :   2021-04-15
* author            :    yang
**********************************************************************************/

#include "virt_client.h"
#include <string>
#include <sstream>
#include <iostream>
#include "common.h"
#include "error/en.h"
#include "tinyxml2.h"
#include <fstream>
#include <utility>
#include <uuid/uuid.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>

namespace matrix {
    namespace core {
        virt_client::virt_client(std::string ip, uint16_t port)
                : m_ip(std::move(ip)), m_port(port) {

        }

        void virt_client::set_address(const std::string &ip, uint16_t port) {
            m_ip = ip;
            m_port = port;
        }

        vector<string> SplitStr(const string &s, const char &c) {
            string buff;
            vector<string> v;
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

        std::string createXmlStr(const std::string & uuid, const std::string & name,
                                 uint64_t memeory, uint64_t max_memory, long cpunum,
                                 const std::string &  vedio_pci, const std::string & image_path)
        {
            //生成xml格式配置文件
            tinyxml2::XMLDocument doc;
            //2.创建根节点
            tinyxml2::XMLElement* root = doc.NewElement("domain");
            root->SetAttribute("type", "kvm");
            doc.InsertEndChild(root);

            tinyxml2::XMLElement* name_node = doc.NewElement("name");
            name_node->SetText(name.c_str());
            root->LinkEndChild(name_node);

            tinyxml2::XMLElement* uuid_node = doc.NewElement("uuid");
            uuid_node->SetText(uuid.c_str());
            root->LinkEndChild(uuid_node);

            if(max_memory > 0)
            {
                tinyxml2::XMLElement* max_memory_node = doc.NewElement("memory");
                max_memory_node->SetAttribute("unit", "KiB");
                max_memory_node->SetText(std::to_string(max_memory).c_str());
                root->LinkEndChild(max_memory_node);
            }

            if(memeory > 0)
            {
                tinyxml2::XMLElement* memory_node = doc.NewElement("currentMemory");
                memory_node->SetAttribute("unit", "KiB");
                memory_node->SetText(std::to_string(memeory).c_str());
                root->LinkEndChild(memory_node);
            }

            if(cpunum > 0)
            {
                tinyxml2::XMLElement* cpu_node = doc.NewElement("vcpu");
                cpu_node->SetAttribute("placement", "static");
                cpu_node->SetText(std::to_string(cpunum).c_str());
                root->LinkEndChild(cpu_node);
            }

            tinyxml2::XMLElement* os_node = doc.NewElement("os");
            tinyxml2::XMLElement* os_sub_node = doc.NewElement("type");
            os_sub_node->SetAttribute("arch", "x86_64");
            os_sub_node->SetAttribute("machine", "pc-1.2");
            os_sub_node->SetText("hvm");
            os_node->LinkEndChild(os_sub_node);

            tinyxml2::XMLElement* os_sub_node2 = doc.NewElement("boot");
            os_sub_node2->SetAttribute("dev", "hd");
            os_node->LinkEndChild(os_sub_node2);

            tinyxml2::XMLElement* os_sub_node3 = doc.NewElement("boot");
            os_sub_node3->SetAttribute("dev", "cdrom");
            os_node->LinkEndChild(os_sub_node3);
            root->LinkEndChild(os_node);

            tinyxml2::XMLElement* clock_node = doc.NewElement("clock");
            clock_node->SetAttribute("offset", "utc");
            root->LinkEndChild(clock_node);

            if(vedio_pci != "")
            {
                tinyxml2::XMLElement* dev_node = doc.NewElement("devices");
                /*
                tinyxml2::XMLElement* vedio_node = doc.NewElement("video");
                tinyxml2::XMLElement* model_node = doc.NewElement("model");
                model_node->SetAttribute("type", "vga");
                model_node->SetAttribute("vram", "16384");
                model_node->SetAttribute("heads", "1");
                vedio_node->LinkEndChild(model_node);
                dev_node->LinkEndChild(vedio_node);
                */

                std::vector<std::string> vedios = SplitStr(vedio_pci, '|');
                for(int i=0; i<vedios.size(); ++i)
                {
                    std::vector<std::string> infos = SplitStr(vedios[i], ':');
                    if(infos.size() != 2)
                    {
                        std::cout << vedios[i]<< "  error" << std::endl;
                        continue;
                    }

                    std::vector<std::string> infos2 = SplitStr(infos[1], '.');
                    if(infos2.size() != 2)
                    {
                        std::cout << vedios[i]<< "  error" << std::endl;
                        continue;
                    }

                    tinyxml2::XMLElement* hostdev_node = doc.NewElement("hostdev");
                    hostdev_node->SetAttribute("mode", "subsystem");
                    hostdev_node->SetAttribute("type", "pci");
                    hostdev_node->SetAttribute("managed", "yes");

                    tinyxml2::XMLElement* source_node = doc.NewElement("source");
                    tinyxml2::XMLElement* address_node = doc.NewElement("address");
                    address_node->SetAttribute("type", "pci");
                    address_node->SetAttribute("domain", "0x0000");
                    address_node->SetAttribute("bus", ("0x"+infos[0]).c_str());
                    address_node->SetAttribute("slot", ("0x"+infos2[0]).c_str());
                    address_node->SetAttribute("function", ("0x"+infos2[1]).c_str());
                    source_node->LinkEndChild(address_node);

                    hostdev_node->LinkEndChild(source_node);
                    dev_node->LinkEndChild(hostdev_node);
                }

                tinyxml2::XMLElement* image_node = doc.NewElement("disk");
                image_node->SetAttribute("type", "file");
                image_node->SetAttribute("device", "disk");

                tinyxml2::XMLElement* driver_node = doc.NewElement("driver");
                driver_node->SetAttribute("name", "qemu");
                driver_node->SetAttribute("type", "qcow2");
                image_node->LinkEndChild(driver_node);

                tinyxml2::XMLElement* source_node = doc.NewElement("source");
                source_node->SetAttribute("file", image_path.c_str());
                image_node->LinkEndChild(source_node);

                tinyxml2::XMLElement* target_node = doc.NewElement("target");
                target_node->SetAttribute("dev", "hda");
                target_node->SetAttribute("bus", "ide");
                image_node->LinkEndChild(target_node);
                dev_node->LinkEndChild(image_node);

                tinyxml2::XMLElement* interface_node = doc.NewElement("interface");
                interface_node->SetAttribute("type", "network");
                tinyxml2::XMLElement* interface_source_node = doc.NewElement("source");
                interface_source_node->SetAttribute("network", "default");
                interface_node->LinkEndChild(interface_source_node);
                dev_node->LinkEndChild(interface_node);

                tinyxml2::XMLElement* graphics_node = doc.NewElement("graphics");
                graphics_node->SetAttribute("type", "vnc");
                graphics_node->SetAttribute("port", "-1");
                graphics_node->SetAttribute("autoport", "yes");
                graphics_node->SetAttribute("listen", "0.0.0.0");
                graphics_node->SetAttribute("keymap", "en-us");
                tinyxml2::XMLElement* listen_node = doc.NewElement("listen");
                listen_node->SetAttribute("type", "address");
                listen_node->SetAttribute("address", "0.0.0.0");
                graphics_node->LinkEndChild(listen_node);
                dev_node->LinkEndChild(graphics_node);

                root->LinkEndChild(dev_node);
            }

            //doc.SaveFile("domain.xml");
            tinyxml2::XMLPrinter printer;
            doc.Print( &printer );//将Print打印到Xmlprint类中 即保存在内存中

            return printer.CStr();//转换成const char*类型
        }

        std::string virt_client::getUrl() {
            std::stringstream sstream;
            sstream << "qemu+tcp://";
            sstream << m_ip;
            sstream << ":";
            sstream << m_port;
            sstream << "/system";
            return sstream.str();
        }

        void virt_client::shell_transform_port(const std::string &host_ip, const std::string &transform_port) {
            std::string cmd = "";
            cmd += "sudo iptables --table nat --append PREROUTING --protocol tcp --destination " + host_ip + " --destination-port " + transform_port + " --jump DNAT --to-destination 192.168.122.2:22";
            cmd += "&& sudo iptables -t nat -A PREROUTING -p tcp --dport " + transform_port + " -j DNAT --to-destination 192.168.122.2:22";
            cmd += "&& sudo iptables -t nat -A POSTROUTING -p tcp --dport " + transform_port + " -d 192.168.122.2 -j SNAT --to 192.168.122.1";
            cmd += "&& sudo iptables -t nat -A PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination 192.168.122.2:20000-60000";
            cmd += "&& sudo iptables -t nat -A PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination 192.168.122.2:20000-60000";
            cmd += "&& sudo iptables -t nat -A POSTROUTING -d 192.168.122.2 -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + host_ip;
            cmd += "&& sudo iptables -t nat -A POSTROUTING -d 192.168.122.2 -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + host_ip;

            FILE * fp;
            char buffer[1024] = {0};
            fp = popen(cmd.c_str(), "r");
            if (fp != nullptr) {
                fgets(buffer, sizeof(buffer), fp);
                pclose(fp);
            }
        }

        std::string virt_client::shell_vga_pci_list() {
            const char* cmd = "lspci |grep NVIDIA |grep VGA |awk '{print $1}' |tr \"\n\" \"|\"";

            FILE * fp;
            char buffer[1024] = {0};
            fp = popen(cmd, "r");
            if (fp != nullptr) {
                fgets(buffer, sizeof(buffer), fp);
                pclose(fp);
            }

            return std::string(buffer);
        }

        // vedio_pci格式： a1:b1.c1|a2:b2.c2|...
        // image_path: /data/**.qcow2
        int32_t virt_client::createDomain(const std::string &name, const std::string &host_ip,
                                          const std::string &transform_port, const std::string &image_path) {
            // 设置端口转发
            if (!host_ip.empty() && !transform_port.empty())
                shell_transform_port(host_ip, transform_port);

            // 获取宿主机显卡列表
            std::string vedio_pci = shell_vga_pci_list();

            long cpuNumTotal = sysconf(_SC_NPROCESSORS_CONF);

            struct sysinfo info{};
            int iRetVal = sysinfo(&info);
            if (iRetVal != 0) {
                return E_VIRT_INTERNAL_ERROR;
            }

            uint64_t memoryTotal = info.totalram / 1024; //kb
            memoryTotal = memoryTotal > 1000000000 ? 1000000000 : memoryTotal;

            uuid_t uu;
            char buf_uuid[1024];
            uuid_generate(uu);
            uuid_unparse(uu, buf_uuid);

            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;
            std::string xml_content = createXmlStr(buf_uuid, name, memoryTotal, memoryTotal, cpuNumTotal, vedio_pci, image_path);

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                virDomainPtr domainPtr = virDomainDefineXML(connPtr, xml_content.c_str());
                if (nullptr == domainPtr) {
                    virErrorPtr error = virGetLastError();
                    if (std::string(error->message).find("already exists with") != std::string::npos) {
                        domainPtr = virDomainLookupByName(connPtr, name.c_str());
                        if (domainPtr == nullptr) {
                            //cout << error->message << endl;
                            virFreeError(error);
                            errorNum = E_VIRT_DOMAIN_EXIST;
                            break;
                        }
                    } else {
                        //cout << error->message << endl;
                        virFreeError(error);
                        errorNum = E_VIRT_DOMAIN_EXIST;
                        break;
                    }
                }

                if (virDomainCreate(domainPtr) < 0) {
                    //std::cout << "Unable to boot guest configuration." << std::endl;
                    errorNum = E_VIRT_INTERNAL_ERROR;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::startDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainCreate(domainPtr);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::suspendDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainSuspend(domainPtr);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::resumeDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainResume(domainPtr);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(domainPtr);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::rebootDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainReboot(domainPtr, VIR_DOMAIN_REBOOT_DEFAULT);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::shutdownDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainShutdown(domainPtr);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::destoryDomain(const std::string &domainName, bool force) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                unsigned int flags = VIR_DOMAIN_DESTROY_GRACEFUL;
                if (force) {
                    flags = VIR_DOMAIN_DESTROY_DEFAULT;
                }
                int ret = virDomainDestroyFlags(domainPtr, flags);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::undefineDomain(const std::string &domainName) {
            virConnectPtr connPtr = nullptr;
            virDomainPtr domainPtr = nullptr;
            int32_t errorNum = E_SUCCESS;

            do {
                std::string url = getUrl();
                connPtr = virConnectOpen(url.c_str());
                if (nullptr == connPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }

                domainPtr = virDomainLookupByName(connPtr, domainName.c_str());
                if (nullptr == domainPtr) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

                int ret = virDomainUndefine(domainPtr);
                if (0 != ret) {
                    //virErrorPtr error = virGetLastError();
                    //std::cout << error->message << endl;
                    //virFreeError(error);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while (0);

            if (nullptr != domainPtr) {
                virDomainFree(domainPtr);
            }

            if (nullptr != connPtr) {
                virConnectClose(connPtr);
            }

            return errorNum;
        }

        int32_t virt_client::listDomainByFlags(unsigned int flags, std::vector<std::string> &nameArray) {
            virConnectPtr conn = nullptr;
            int32_t errorNum = E_SUCCESS;
            virDomainPtr *domains;

            do {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (nullptr == conn) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                int ret = virConnectListAllDomains(conn, &domains, flags);
                if (ret < 0) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                for (int i = 0; i < ret; i++) {
                    std::string name = virDomainGetName(domains[i]);
                    nameArray.push_back(name);
                    //here or in a separate loop if needed
                    virDomainFree(domains[i]);
                }
                free(domains);

            } while (0);
            if (nullptr != conn) {
                virConnectClose(conn);
            }
            return errorNum;
        }

        int32_t virt_client::listAllRunningDomain(std::vector<std::string> &nameArray) {
            unsigned int flags = VIR_CONNECT_LIST_DOMAINS_RUNNING;
            return listDomainByFlags(flags, nameArray);
        }

        int32_t virt_client::listAllDomain(std::vector<std::string> &nameArray) {
            unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_INACTIVE |
                                 VIR_CONNECT_LIST_DOMAINS_PERSISTENT |
                                 VIR_CONNECT_LIST_DOMAINS_TRANSIENT |
                                 VIR_CONNECT_LIST_DOMAINS_RUNNING |
                                 VIR_CONNECT_LIST_DOMAINS_PAUSED |
                                 VIR_CONNECT_LIST_DOMAINS_SHUTOFF |
                                 VIR_CONNECT_LIST_DOMAINS_OTHER |
                                 VIR_CONNECT_LIST_DOMAINS_MANAGEDSAVE |
                                 VIR_CONNECT_LIST_DOMAINS_NO_MANAGEDSAVE |
                                 VIR_CONNECT_LIST_DOMAINS_AUTOSTART |
                                 VIR_CONNECT_LIST_DOMAINS_NO_AUTOSTART |
                                 VIR_CONNECT_LIST_DOMAINS_HAS_SNAPSHOT |
                                 VIR_CONNECT_LIST_DOMAINS_NO_SNAPSHOT;
            return listDomainByFlags(flags, nameArray);
        }

        int32_t virt_client::createDomainByXML(const std::string &filepath) {
            virConnectPtr conn = nullptr;
            virDomainPtr domain = nullptr;
            int32_t errorNum = E_SUCCESS;
            std::ifstream file(filepath, std::ifstream::in | std::ifstream::binary);
            if (!file) {
                cout << "Cannot open file, permission denied." << filepath << endl;
                return E_FILE_FAILURE;
            }
            stringstream buffer;
            buffer << file.rdbuf();
            string xmlcontents = buffer.str();
            file.close();
            do {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (nullptr == conn) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                domain = virDomainDefineXML(conn, xmlcontents.c_str());
                if (nullptr == domain) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_EXIST;
                    break;
                } else {
                    cout << "define persistent domain success." << std::endl;
                    if (virDomainCreate(domain) < 0) {
                        std::cout << "Unable to boot guest configuration." << std::endl;
                        errorNum = E_VIRT_INTERNAL_ERROR;
                    } else {
                        std::cout << "Boot the persistent defined guest ..." << std::endl;
                    }
                    return 0;
                }

            } while (0);
            if (nullptr != domain) {
                virDomainFree(domain);
            }
            if (nullptr != conn) {
                virConnectClose(conn);
            }
            return errorNum;
        }

        bool virt_client::existDomain(std::string &domainName) {
            std::string url = getUrl();
            virConnectPtr conn_ptr = virConnectOpen(url.c_str());
            if (conn_ptr == nullptr) {
                //virErrorPtr error = virGetLastError();
                //printf("connect virt error: %s\n", error->message);
                //virFreeError(error);
                return false;
            }

            virDomainPtr domain_ptr = virDomainLookupByName(conn_ptr, domainName.c_str());
            if (domain_ptr == nullptr) {
                virConnectClose(conn_ptr);
                return false;
            } else {
                virDomainFree(domain_ptr);
                virConnectClose(conn_ptr);
                return true;
            }
        }

        vm_status virt_client::getDomainStatus(std::string &domainName) {
            vm_status status = vm_unknown;
            virConnectPtr conn_ptr = nullptr;
            virDomainPtr domain_ptr = nullptr;

            do {
                std::string url = getUrl();
                conn_ptr = virConnectOpen(url.c_str());
                if (conn_ptr == nullptr) {
                    //virErrorPtr error = virGetLastError();
                    //printf("connect virt error: %s\n", error->message);
                    //virFreeError(error);
                    break;
                }

                domain_ptr = virDomainLookupByName(conn_ptr, domainName.c_str());
                if (domain_ptr == NULL) {
                    //virErrorPtr error = virGetLastError();
                    //fprintf(stderr, "virDomainLookupByName failed: %s\n", error->message);
                    //virFreeError(error);
                    break;
                }

                virDomainInfo domain_info;
                if (virDomainGetInfo(domain_ptr, &domain_info) < 0) {
                    //virErrorPtr error = virGetLastError();
                    //fprintf(stderr, "virDomainGetInfo failed: %s\n", error->message);
                    //virFreeError(error);
                    break;
                }

                // domain 状态
                //VIR_DOMAIN_NOSTATE = 0,     /* no state */
                //VIR_DOMAIN_RUNNING = 1,     /* the domain is running */
                //VIR_DOMAIN_BLOCKED = 2,     /* the domain is blocked on resource */
                //VIR_DOMAIN_PAUSED  = 3,     /* the domain is paused by user */
                //VIR_DOMAIN_SHUTDOWN= 4,     /* the domain is being shut down */
                //VIR_DOMAIN_SHUTOFF = 5,     /* the domain is shut off */
                //VIR_DOMAIN_CRASHED = 6,     /* the domain is crashed */
                //VIR_DOMAIN_PMSUSPENDED = 7, /* the domain is suspended by guest
                //                               power management */

                if (domain_info.state == VIR_DOMAIN_RUNNING) {
                    status = vm_running;
                } else if (domain_info.state == VIR_DOMAIN_SHUTOFF) {
                    status = vm_closed;
                } else if (domain_info.state == VIR_DOMAIN_PAUSED) {
                    status = vm_suspended;
                }
            } while(0);

            if (nullptr != domain_ptr) {
                virDomainFree(domain_ptr);
            }

            if (nullptr != conn_ptr) {
                virConnectClose(conn_ptr);
            }

            return status;
        }
    }
}
