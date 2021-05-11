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
#include <uuid/uuid.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>

namespace matrix
{
    namespace core
    {

        virt_client::virt_client(std::string ip, uint16_t remote_port):
         m_ip(ip),m_remote_port(remote_port)
        {

        }

		void virt_client::set_address(std::string remote_ip, uint16_t remote_port)
		{
			m_ip = remote_ip;
			m_remote_port = remote_port;
		}

        std::string virt_client::getUrl()
        {
            std::stringstream sstream;
            sstream << "qemu+tcp://";
            sstream << m_ip;
            sstream<< ":";
            sstream << m_remote_port;
            sstream << "/system";
            return sstream.str();
        }

        int32_t  virt_client::shutdownDomain(const std::string&  domainName) 
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }


                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                int ret = virDomainShutdown(domain);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;
        }


        int32_t  virt_client::rebootDomain(const std::string&  domainName)
        {

            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }


                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                int ret = virDomainReboot(domain, VIR_DOMAIN_REBOOT_DEFAULT);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;
        }

        int32_t  virt_client::suspendDomain(const std::string&  domainName)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }


                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                int ret = virDomainSuspend(domain);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;        
        }

        int32_t  virt_client::resumeDomain(const std::string&  domainName)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
        
        
                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                int ret = virDomainResume(domain);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;        
        }



        int32_t  virt_client::startDomain(const std::string&  domainName)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }


                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                int ret = virDomainCreate(domain);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;
        }


        int32_t virt_client::listDomainByFlags( unsigned int flags, std::vector<std::string> & nameArray)
        {
            virConnectPtr conn = NULL;
            int32_t errorNum = E_SUCCESS;
            virDomainPtr *domains;
            
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                int ret = virConnectListAllDomains(conn, &domains, flags);
                if(ret < 0)
                {
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

            } while(0);
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;
        }

        int32_t  virt_client::listAllRunningDomain (std::vector<std::string> & nameArray)
        {
            unsigned int flags = VIR_CONNECT_LIST_DOMAINS_RUNNING;
            return listDomainByFlags(flags, nameArray);
        }


        int32_t  virt_client::listAllDomain(std::vector<std::string> & nameArray)
        {
            unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |VIR_CONNECT_LIST_DOMAINS_INACTIVE|
                                    VIR_CONNECT_LIST_DOMAINS_PERSISTENT|
                                    VIR_CONNECT_LIST_DOMAINS_TRANSIENT|
                                    VIR_CONNECT_LIST_DOMAINS_RUNNING|
                                    VIR_CONNECT_LIST_DOMAINS_PAUSED|
                                    VIR_CONNECT_LIST_DOMAINS_SHUTOFF|
                                    VIR_CONNECT_LIST_DOMAINS_OTHER|
                                    VIR_CONNECT_LIST_DOMAINS_MANAGEDSAVE|
                                    VIR_CONNECT_LIST_DOMAINS_NO_MANAGEDSAVE|
                                    VIR_CONNECT_LIST_DOMAINS_AUTOSTART|
                                    VIR_CONNECT_LIST_DOMAINS_NO_AUTOSTART|
                                    VIR_CONNECT_LIST_DOMAINS_HAS_SNAPSHOT|
                                    VIR_CONNECT_LIST_DOMAINS_NO_SNAPSHOT;
            return listDomainByFlags(flags, nameArray);
        }


        int32_t  virt_client::destoryDomain(const std::string&  domainName, bool force)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }


                domain = virDomainLookupByName(conn, domainName.c_str());
                if(NULL == domain) 
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }
                unsigned int flags = VIR_DOMAIN_DESTROY_GRACEFUL;
                if (force)
                {
                    flags = VIR_DOMAIN_DESTROY_DEFAULT;
                }
                int ret = virDomainDestroyFlags(domain, flags);
                if(0 != ret)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    free(domain);
                    errorNum = E_VIRT_DOMAIN_NOT_FOUND;
                    break;
                }

            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
           return  errorNum;        
        }

        vector<string> SplitStr(const string &s,const char &c)
        {
            string buff;
            vector<string> v;
            char tmp;
            for (int i=0;i<s.size();i++)
            {
            	tmp = s[i];
            	if (tmp!=c){
            		buff+=tmp;
            	}else {
            		if(tmp==c && buff !=""){
            			v.push_back(buff);
            			buff = "";
            		}
            	}//endif
            }
            if (buff !="")
            {
            	v.push_back(buff);
            }
            return v;
        }


        std::string createXmlStr(const std::string & uuid,
                              const std::string & name,
                              int32_t memeory,
                              int32_t max_memory,
                              int32_t cpunum,  
                              const std::string &  vedio_pci,
                              const std::string & image_path)
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
                max_memory_node->SetText(to_string(max_memory).c_str());
                root->LinkEndChild(max_memory_node);

            }


            if(memeory > 0)
            {
                tinyxml2::XMLElement* memory_node = doc.NewElement("currentMemory");
                memory_node->SetAttribute("unit", "KiB");
                memory_node->SetText(to_string(memeory).c_str());
                root->LinkEndChild(memory_node);

            }

            if(cpunum > 0)
            {
                tinyxml2::XMLElement* cpu_node = doc.NewElement("vcpu");
                cpu_node->SetAttribute("placement", "static");
                cpu_node->SetText(to_string(cpunum).c_str());
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
                tinyxml2::XMLElement* vedio_node = doc.NewElement("video");
                tinyxml2::XMLElement* model_node = doc.NewElement("model");
                model_node->SetAttribute("type", "vga");
                model_node->SetAttribute("vram", "16384");
                model_node->SetAttribute("heads", "1");
                vedio_node->LinkEndChild(model_node);
                
                std::vector<std::string> vedios = SplitStr(vedio_pci, '|');
                for(int i=0; i<vedios.size(); ++i)
                {
                    std::vector<std::string> infos = SplitStr(vedios[i], ':');
                    if(infos.size() != 2)
                    {
                       std::cout << vedios[i]<< "  error" << endl;
                       continue;
                    }
                    std::vector<std::string> infos2 = SplitStr(infos[1], '.');
                    if(infos2.size() != 2)
                    {
                       std::cout << vedios[i]<< "  error" << endl;
                       continue;
                    }
                    tinyxml2::XMLElement* address_node = doc.NewElement("address");
                    address_node->SetAttribute("type", "pci");
                    address_node->SetAttribute("domain", "0x0000");
                    address_node->SetAttribute("bus", ("0x"+infos[0]).c_str());
                    address_node->SetAttribute("slot", ("0x"+infos2[0]).c_str());
                    address_node->SetAttribute("function", ("0x"+infos2[1]).c_str());
                    vedio_node->LinkEndChild(address_node);
                }

                dev_node->LinkEndChild(vedio_node);


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

                
                root->LinkEndChild(dev_node);
            }
            
            //doc.SaveFile("domain.xml");
            tinyxml2::XMLPrinter printer;
            doc.Print( &printer );//将Print打印到Xmlprint类中 即保存在内存中
            return printer.CStr();//转换成const char*类型
        }

        int32_t virt_client::createDomain(const std::string& name, const std::string&  vedio_pci, const std::string& image_path)
        {
            int cpuNumTotal = sysconf(_SC_NPROCESSORS_CONF);

            struct sysinfo info; 
            int iRetVal = sysinfo(&info); 
            if(iRetVal != 0)
            { 
               return E_VIRT_INTERNAL_ERROR;
            }
            int64_t memoryTotal = info.totalram/1024;

            uuid_t uu;
            char buf[1024];
            uuid_generate(uu);
            uuid_unparse(uu,buf);
            
            return createDomainImp(std::string(buf),
                        name, 
                        memoryTotal*DEFAULT_PERCENTAGE, 
                        memoryTotal*DEFAULT_PERCENTAGE,
                        cpuNumTotal * DEFAULT_PERCENTAGE,
                        vedio_pci,
                        image_path);
        }

        int32_t  virt_client::createDomainImp(std::string uuid,
                              std::string  name,
                              int32_t memeory,
                              int32_t max_memory,
                              int32_t cpunum,  
                              const std::string&  vedio_pci,
                              const std::string& image_path)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
            int32_t errorNum = E_SUCCESS;
            std::string xmlStr = createXmlStr(uuid, name, memeory, max_memory, cpunum, vedio_pci, image_path);

            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                virDomainPtr domain = virDomainDefineXML(conn, xmlStr.c_str());
                if (NULL == domain)
                {
                    virErrorPtr error = virGetLastError();
                    if(std::string(error->message).find("already exists with") != std::string::npos)
                    {
                        domain = virDomainLookupByName(conn, name.c_str());
                        if(domain  == NULL)
                        {
                    cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_EXIST;
                    break;
                }
                    }
                    else
                    {
                        cout << error->message << endl;
                        errorNum = E_VIRT_DOMAIN_EXIST;
                        break;
                    }
                }
                
                    std::cout << "define persistent domain success." << std::endl;
                    if (virDomainCreate(domain) < 0) 
                    {
                        std::cout << "Unable to boot guest configuration." << std::endl;
                        errorNum = E_VIRT_INTERNAL_ERROR;
                    } 
                    else 
                    {
                        std::cout << "Boot the persistent defined guest ..." << std::endl;
                    }

                 
            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
            return errorNum;
        }

        
        
        int32_t  virt_client::createDomainByXML(const std::string&  filepath)
        {
            virConnectPtr conn = NULL;
            virDomainPtr domain = NULL;
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
            do
            {
                std::string url = getUrl();
                conn = virConnectOpen(url.c_str());
                if (NULL == conn ) {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_CONNECT_ERROR;
                    break;
                }
                domain = virDomainDefineXML(conn, xmlcontents.c_str());
                if (NULL == domain)
                {
                    virErrorPtr error = virGetLastError();
                    std::cout << error->message << endl;
                    errorNum = E_VIRT_DOMAIN_EXIST;
                    break;
                }
                else
                {
                    cout << "define persistent domain success." << std::endl;
                    if (virDomainCreate(domain) < 0) 
                    {                   
                        std::cout << "Unable to boot guest configuration." << std::endl;
                        errorNum = E_VIRT_INTERNAL_ERROR;
                    } 
                    else 
                    {
                        std::cout << "Boot the persistent defined guest ..." << std::endl;
                    }
                    return 0;
                }
                 
            } while(0);
           if (NULL != domain)
           {
              virDomainFree(domain);
           }
           if (NULL != conn)
           {
              virConnectClose(conn);
           }
            return errorNum;
        }

		bool virt_client::existDomain(std::string&  domainName) 
		{
			std::vector<std::string> nameArray;
			listAllDomain(nameArray);

			for (auto it : nameArray)
			{
				if (it.compare(domainName) == 0)
					return true;
			}

			return false;
		}

		vm_status virt_client::getDomainStatus(std::string&  domainName) 
		{
			std::vector<std::string> nameArray;
			listAllRunningDomain(nameArray);

			for (auto it : nameArray)
			{
				if (it.compare(domainName) == 0)
					return vm_running;
			}
			return vm_unknown;
		}
    }

}

