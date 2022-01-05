#include "vm_client.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "server/server.h"
#include <boost/property_tree/json_parser.hpp>
#include "tinyxml2.h"
#include <uuid/uuid.h>
#include "service/message/message_id.h"
#include "service/message/vm_task_result_types.h"
#include "../db/snapshotinfo_types.h"

static std::string createXmlStr(const std::string& uuid, const std::string& domain_name,
                         int64_t memory, int32_t cpunum, int32_t sockets, int32_t cores, int32_t threads,
                         const std::string& vedio_pci, const std::string & image_file,
                         const std::string& disk_file, int32_t vnc_port, const std::string& vnc_pwd,
                         const std::vector<std::string>& multicast,
                         bool is_windows = false, bool uefi = false)
{
    tinyxml2::XMLDocument doc;

    // <domain>
    tinyxml2::XMLElement* root = doc.NewElement("domain");
    root->SetAttribute("type", "kvm");
    root->SetAttribute("xmlns:qemu", "http://libvirt.org/schemas/domain/qemu/1.0");
    doc.InsertEndChild(root);

    tinyxml2::XMLElement* name_node = doc.NewElement("name");
    name_node->SetText(domain_name.c_str());
    root->LinkEndChild(name_node);

    tinyxml2::XMLElement* uuid_node = doc.NewElement("uuid");
    uuid_node->SetText(uuid.c_str());
    root->LinkEndChild(uuid_node);

    // memory
    tinyxml2::XMLElement* max_memory_node = doc.NewElement("memory");
    max_memory_node->SetAttribute("unit", "KiB");
    max_memory_node->SetText(std::to_string(memory).c_str());
    root->LinkEndChild(max_memory_node);

    tinyxml2::XMLElement* memory_node = doc.NewElement("currentMemory");
    memory_node->SetAttribute("unit", "KiB");
    memory_node->SetText(std::to_string(memory).c_str());
    root->LinkEndChild(memory_node);

    // vcpu
    tinyxml2::XMLElement* vcpu_node = doc.NewElement("vcpu");
    vcpu_node->SetAttribute("placement", "static");
    vcpu_node->SetText(std::to_string(cpunum).c_str());
    root->LinkEndChild(vcpu_node);

    // <os>
    tinyxml2::XMLElement* os_node = doc.NewElement("os");
    tinyxml2::XMLElement* os_sub_node = doc.NewElement("type");
    os_sub_node->SetAttribute("arch", "x86_64");
    os_sub_node->SetAttribute("machine", is_windows ? "q35" : "pc-1.2");
    os_sub_node->SetText("hvm");
    os_node->LinkEndChild(os_sub_node);

    if (!is_windows) {
        tinyxml2::XMLElement* os_sub_node2 = doc.NewElement("boot");
        os_sub_node2->SetAttribute("dev", "hd");
        os_node->LinkEndChild(os_sub_node2);

        tinyxml2::XMLElement* os_sub_node3 = doc.NewElement("boot");
        os_sub_node3->SetAttribute("dev", "cdrom");
        os_node->LinkEndChild(os_sub_node3);
    } else {
        // uefi引导设置
        if (uefi) {
            tinyxml2::XMLElement *os_loader = doc.NewElement("loader");
            os_loader->SetAttribute("readonly", "yes");
            // 如果加载程序路径指向 UEFI 映像，则类型应为pflash。
            os_loader->SetAttribute("type", "pflash");
            // os_loader->SetAttribute("type", "rom");
            os_loader->SetText("/usr/share/OVMF/OVMF_CODE.fd");
            os_node->LinkEndChild(os_loader);
        }
        tinyxml2::XMLElement *os_bootmenu_node = doc.NewElement("bootmenu");
        os_bootmenu_node->SetAttribute("enable", "yes");
        os_node->LinkEndChild(os_bootmenu_node);
    }
    root->LinkEndChild(os_node);

    // <features>
    tinyxml2::XMLElement* features_node = doc.NewElement("features");
    tinyxml2::XMLElement* features_sub_node1 = doc.NewElement("acpi");
    features_node->LinkEndChild(features_sub_node1);

    tinyxml2::XMLElement* features_sub_node2 = doc.NewElement("apic");
    features_node->LinkEndChild(features_sub_node2);

    tinyxml2::XMLElement* features_sub_node3 = doc.NewElement("hyperv");
    tinyxml2::XMLElement* node_relaxed = doc.NewElement("relaxed");
    node_relaxed->SetAttribute("state", "on");
    features_sub_node3->LinkEndChild(node_relaxed);
    tinyxml2::XMLElement* node_vapic = doc.NewElement("vapic");
    node_vapic->SetAttribute("state", "on");
    features_sub_node3->LinkEndChild(node_vapic);
    tinyxml2::XMLElement* node_spinlocks = doc.NewElement("spinlocks");
    node_spinlocks->SetAttribute("state", "on");
    node_spinlocks->SetAttribute("retries", "8191");
    features_sub_node3->LinkEndChild(node_spinlocks);
    tinyxml2::XMLElement* node_vendor_id = doc.NewElement("vendor_id");
    node_vendor_id->SetAttribute("state", "on");
    node_vendor_id->SetAttribute("value", "1234567890ab");
    features_sub_node3->LinkEndChild(node_vendor_id);
    features_node->LinkEndChild(features_sub_node3);

    tinyxml2::XMLElement* features_sub_node4 = doc.NewElement("kvm");
    tinyxml2::XMLElement* node_hidden = doc.NewElement("hidden");
    node_hidden->SetAttribute("state", "on");
    features_sub_node4->LinkEndChild(node_hidden);
    features_node->LinkEndChild(features_sub_node4);

    tinyxml2::XMLElement* node_vmport = doc.NewElement("vmport");
    node_vmport->SetAttribute("state", "off");
    features_node->LinkEndChild(node_vmport);

    tinyxml2::XMLElement* node_ioapic = doc.NewElement("ioapic");
    node_ioapic->SetAttribute("driver", "kvm");
    features_node->LinkEndChild(node_ioapic);

    root->LinkEndChild(features_node);

    // <cpu>
    tinyxml2::XMLElement* cpu_node = doc.NewElement("cpu");
    cpu_node->SetAttribute("mode", "host-passthrough");
    cpu_node->SetAttribute("check", "none");
    tinyxml2::XMLElement* node_topology = doc.NewElement("topology");
    node_topology->SetAttribute("sockets", std::to_string(sockets).c_str());
    node_topology->SetAttribute("cores", std::to_string(cores).c_str());
    node_topology->SetAttribute("threads", std::to_string(threads).c_str());
    cpu_node->LinkEndChild(node_topology);
    tinyxml2::XMLElement* node_cache = doc.NewElement("cache");
    node_cache->SetAttribute("mode", "passthrough");
    cpu_node->LinkEndChild(node_cache);
    root->LinkEndChild(cpu_node);

    tinyxml2::XMLElement* clock_node = doc.NewElement("clock");
    if (!is_windows) {
        clock_node->SetAttribute("offset", "utc");
    } else {
        clock_node->SetAttribute("offset", "localtime");
        tinyxml2::XMLElement *clock_rtc_node = doc.NewElement("timer");
        clock_rtc_node->SetAttribute("name", "rtc");
        clock_rtc_node->SetAttribute("tickpolicy", "catchup");
        clock_node->LinkEndChild(clock_rtc_node);
        tinyxml2::XMLElement *clock_pit_node = doc.NewElement("timer");
        clock_pit_node->SetAttribute("name", "pit");
        clock_pit_node->SetAttribute("tickpolicy", "delay");
        clock_node->LinkEndChild(clock_pit_node);
        tinyxml2::XMLElement *clock_hpet_node = doc.NewElement("timer");
        clock_hpet_node->SetAttribute("name", "hpet");
        clock_hpet_node->SetAttribute("present", "no");
        clock_node->LinkEndChild(clock_hpet_node);
        tinyxml2::XMLElement *clock_hypervclock_node = doc.NewElement("timer");
        clock_hypervclock_node->SetAttribute("name", "hypervclock");
        clock_hypervclock_node->SetAttribute("present", "yes");
        clock_node->LinkEndChild(clock_hypervclock_node);
    }
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

    tinyxml2::XMLElement* dev_node = doc.NewElement("devices");
    if (is_windows) {
        tinyxml2::XMLElement* vedio_node = doc.NewElement("video");
        tinyxml2::XMLElement* model_node = doc.NewElement("model");
        // model_node->SetAttribute("type", "vga"); //default "cirrus"
        model_node->SetAttribute("type", "qxl");
        model_node->SetAttribute("ram", "65536");
        model_node->SetAttribute("vram", "65536");
        model_node->SetAttribute("vgamem", "16384");
        model_node->SetAttribute("heads", "1");
        model_node->SetAttribute("primary", "yes");
        vedio_node->LinkEndChild(model_node);
        dev_node->LinkEndChild(vedio_node);

        tinyxml2::XMLElement* input_node = doc.NewElement("input");
        input_node->SetAttribute("type", "tablet");
        input_node->SetAttribute("bus", "usb");
        dev_node->LinkEndChild(input_node);
    }

    if(vedio_pci != "") {
        std::vector<std::string> vedios = util::split(vedio_pci, "|");
        for (int i = 0; i < vedios.size(); ++i) {
            std::vector<std::string> infos = util::split(vedios[i], ":");
            if (infos.size() != 2) {
                std::cout << vedios[i] << "  error" << std::endl;
                continue;
            }

            std::vector<std::string> infos2 = util::split(infos[1], ".");
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
    tinyxml2::XMLElement* image_node = doc.NewElement("disk");
    image_node->SetAttribute("type", "file");
    image_node->SetAttribute("device", "disk");

    tinyxml2::XMLElement* driver_node = doc.NewElement("driver");
    driver_node->SetAttribute("name", "qemu");
    driver_node->SetAttribute("type", "qcow2");
    image_node->LinkEndChild(driver_node);

    tinyxml2::XMLElement* source_node = doc.NewElement("source");
    source_node->SetAttribute("file", image_file.c_str());
    image_node->LinkEndChild(source_node);

    tinyxml2::XMLElement* target_node = doc.NewElement("target");
    target_node->SetAttribute("dev", "vda");
    target_node->SetAttribute("bus", "virtio");
    image_node->LinkEndChild(target_node);

    if (is_windows) {
        // set boot order
        tinyxml2::XMLElement *boot_order_node = doc.NewElement("boot");
        boot_order_node->SetAttribute("order", "1");
        image_node->LinkEndChild(boot_order_node);
    }
    dev_node->LinkEndChild(image_node);

    // disk (data)
    tinyxml2::XMLElement* disk_data_node = doc.NewElement("disk");
    disk_data_node->SetAttribute("type", "file");

    tinyxml2::XMLElement* disk_driver_node = doc.NewElement("driver");
    disk_driver_node->SetAttribute("name", "qemu");
    disk_driver_node->SetAttribute("type", "qcow2");
    disk_data_node->LinkEndChild(disk_driver_node);

    tinyxml2::XMLElement* disk_source_node = doc.NewElement("source");
    disk_source_node->SetAttribute("file", disk_file.c_str());
    disk_data_node->LinkEndChild(disk_source_node);

    tinyxml2::XMLElement* disk_target_node = doc.NewElement("target");
    disk_target_node->SetAttribute("dev", "vdb");
    disk_target_node->SetAttribute("bus", "virtio");
    disk_data_node->LinkEndChild(disk_target_node);

    dev_node->LinkEndChild(disk_data_node);

    // qemu_guest_agent
    tinyxml2::XMLElement* agent_node = doc.NewElement("channel");
    agent_node->SetAttribute("type", "unix");
    tinyxml2::XMLElement* agent_source_node = doc.NewElement("source");
    agent_source_node->SetAttribute("mode", "bind");
    agent_source_node->SetAttribute("path", ("/tmp/channel_" + uuid + ".sock").c_str());
    agent_node->LinkEndChild(agent_source_node);
    tinyxml2::XMLElement* agent_target_node = doc.NewElement("target");
    agent_target_node->SetAttribute("type", "virtio");
    agent_target_node->SetAttribute("name", "org.qemu.guest_agent.0");
    agent_node->LinkEndChild(agent_target_node);
    dev_node->LinkEndChild(agent_node);

    // network
    tinyxml2::XMLElement* interface_node = doc.NewElement("interface");
    interface_node->SetAttribute("type", "network");
    tinyxml2::XMLElement* interface_source_node = doc.NewElement("source");
    interface_source_node->SetAttribute("network", "default");
    interface_node->LinkEndChild(interface_source_node);
    tinyxml2::XMLElement* interface_model_node = doc.NewElement("model");
    interface_model_node->SetAttribute("type", "virtio");
    interface_node->LinkEndChild(interface_model_node);
    dev_node->LinkEndChild(interface_node);

    // multicast
    if (!multicast.empty()) {
        for (const auto &address : multicast) {
            std::vector<std::string> vecSplit = util::split(address, ":");
            tinyxml2::XMLElement* mcast_node = doc.NewElement("interface");
            mcast_node->SetAttribute("type", "mcast");
            tinyxml2::XMLElement* mcast_source_node = doc.NewElement("source");
            mcast_source_node->SetAttribute("address", vecSplit[0].c_str());
            mcast_source_node->SetAttribute("port", vecSplit[1].c_str());
            mcast_node->LinkEndChild(mcast_source_node);
            dev_node->LinkEndChild(mcast_node);
        }
    }

    // vnc
    tinyxml2::XMLElement* graphics_node = doc.NewElement("graphics");
    graphics_node->SetAttribute("type", "vnc");
    graphics_node->SetAttribute("port", std::to_string(vnc_port).c_str());
    graphics_node->SetAttribute("autoport", vnc_port == -1 ? "yes" : "no");
    graphics_node->SetAttribute("listen", "0.0.0.0");
    graphics_node->SetAttribute("keymap", "en-us");
    graphics_node->SetAttribute("passwd", vnc_pwd.c_str());
    tinyxml2::XMLElement* listen_node = doc.NewElement("listen");
    listen_node->SetAttribute("type", "address");
    listen_node->SetAttribute("address", "0.0.0.0");
    graphics_node->LinkEndChild(listen_node);
    dev_node->LinkEndChild(graphics_node);

    if (is_windows) {
        tinyxml2::XMLElement *memballoon_node = doc.NewElement("memballoon");
        memballoon_node->SetAttribute("model", "none");
        dev_node->LinkEndChild(memballoon_node);
    }
    root->LinkEndChild(dev_node);

    // cpu (qemu:commandline)
    tinyxml2::XMLElement* node_qemu_commandline = doc.NewElement("qemu:commandline");
    tinyxml2::XMLElement* node_qemu_arg1 = doc.NewElement("qemu:arg");
    node_qemu_arg1->SetAttribute("value", "-cpu");
    node_qemu_commandline->LinkEndChild(node_qemu_arg1);
    tinyxml2::XMLElement* node_qemu_arg2 = doc.NewElement("qemu:arg");
    node_qemu_arg2->SetAttribute("value", "host,kvm=off,hv_vendor_id=null");
    node_qemu_commandline->LinkEndChild(node_qemu_arg2);
    tinyxml2::XMLElement* node_qemu_arg3 = doc.NewElement("qemu:arg");
    node_qemu_arg3->SetAttribute("value", "-machine");
    node_qemu_commandline->LinkEndChild(node_qemu_arg3);
    tinyxml2::XMLElement* node_qemu_arg4 = doc.NewElement("qemu:arg");
    node_qemu_arg4->SetAttribute("value", "kernel_irqchip=on");
    node_qemu_commandline->LinkEndChild(node_qemu_arg4);
    root->LinkEndChild(node_qemu_commandline);

    //doc.SaveFile("domain.xml");
    tinyxml2::XMLPrinter printer;
    doc.Print( &printer );
    return printer.CStr();
}

static std::string createSnapshotXml(const std::shared_ptr<dbc::snapshotInfo>& info) {
    tinyxml2::XMLDocument doc;
    // <domainsnapshot>
    tinyxml2::XMLElement *root = doc.NewElement("domainsnapshot");
    doc.InsertEndChild(root);

    // description
    tinyxml2::XMLElement *desc = doc.NewElement("description");
    desc->SetText(info->description.c_str());
    root->LinkEndChild(desc);

    // name
    tinyxml2::XMLElement *name = doc.NewElement("name");
    name->SetText(info->name.c_str());
    root->LinkEndChild(name);

    // disks
    tinyxml2::XMLElement *disks = doc.NewElement("disks");
    // // system disk
    // tinyxml2::XMLElement *disk1 = doc.NewElement("disk");
    // disk1->SetAttribute("name", "hda");
    // disk1->SetAttribute("snapshot", "external");
    // tinyxml2::XMLElement *disk1driver = doc.NewElement("driver");
    // disk1driver->SetAttribute("type", "qcow2");
    // disk1->LinkEndChild(disk1driver);
    // disks->LinkEndChild(disk1);
    // // data disk
    // tinyxml2::XMLElement *disk2 = doc.NewElement("disk");
    // disk2->SetAttribute("name", "vda");
    // disk2->SetAttribute("snapshot", "no");
    // disks->LinkEndChild(disk2);
    // root->LinkEndChild(disks);
    for (const auto& diskinfo : info->disks) {
        tinyxml2::XMLElement *disk = doc.NewElement("disk");
        disk->SetAttribute("name", diskinfo.name.c_str());
        disk->SetAttribute("snapshot", diskinfo.snapshot.c_str());
        if (diskinfo.snapshot != "no") {
            tinyxml2::XMLElement *diskdriver = doc.NewElement("driver");
            diskdriver->SetAttribute("type", diskinfo.driver_type.c_str());
            disk->LinkEndChild(diskdriver);
            if (!diskinfo.source_file.empty() && diskinfo.snapshot == "external") {
                tinyxml2::XMLElement *sourcefile = doc.NewElement("source");
                sourcefile->SetAttribute("file", diskinfo.source_file.c_str());
                disk->LinkEndChild(sourcefile);
            }
        }
        disks->LinkEndChild(disk);
    }
    root->LinkEndChild(disks);

    // doc.SaveFile((info->name + ".xml").c_str());
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}

std::string getUrl() {
    std::stringstream sstream;
    sstream << "qemu+tcp://";
    sstream << "localhost";
    sstream << ":";
    sstream << 16509;
    sstream << "/system";
    return sstream.str();
}

VmClient::VmClient() {

}

VmClient::~VmClient() {
    if (m_connPtr != nullptr) {
        virConnectClose(m_connPtr);
    }
}

bool VmClient::Init() {
    if (m_connPtr == nullptr) {
        std::string url = getUrl();
        m_connPtr = virConnectOpen(url.c_str());
    }
    return m_connPtr != nullptr;
}

int32_t VmClient::CreateDomain(const std::string& domain_name, const std::string& image_name, const std::string& data_file_name,
                               const std::shared_ptr<TaskResource>& task_resource, const std::vector<std::string>& multicast,
                               bool is_windows, bool uefi) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    // gpu
    std::map<std::string, std::list<std::string>> mpGpu = task_resource->gpus;
    std::string vga_pci;
    for (auto& it : mpGpu) {
        for (auto& it2 : it.second) {
            vga_pci += it2 + "|";
        }
    }
    LOG_INFO << "vga_pci: " << vga_pci;

    // cpu
    long cpuNumTotal = task_resource->total_cores();
    LOG_INFO << "cpu: " << cpuNumTotal;

    // mem
    uint64_t memoryTotal = task_resource->mem_size; // KB
    LOG_INFO << "mem: " << memoryTotal << "KB";

    // uuid
    uuid_t uu;
    char buf_uuid[1024] = {0};
    uuid_generate(uu);
    uuid_unparse(uu, buf_uuid);
    LOG_INFO << "uuid: " << buf_uuid;
    TASK_LOG_INFO(domain_name, "create domain with vga_pci: " << vga_pci << ", cpu: " << cpuNumTotal
        << ", mem: " << memoryTotal << "KB, uuid: " << buf_uuid);

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
    LOG_INFO << "image_copy_file: " << to_image_path;
    TASK_LOG_INFO(domain_name, "image_copy_file: " << to_image_path);
    // boost::filesystem::copy_file(from_image_path, to_image_path);
    std::string cmd_back_system_image = "qemu-img create -f qcow2 -F qcow2 -b " + from_image_path + " " + to_image_path;
    std::string create_system_image_ret = run_shell(cmd_back_system_image);
    LOG_INFO << "create system image back cmd: " << cmd_back_system_image << ", result: " << create_system_image_ret;
    TASK_LOG_INFO(domain_name, "create system image back cmd: " << cmd_back_system_image << ", result: " << create_system_image_ret);

    // 创建虚拟磁盘（数据盘）
    std::string data_file = "/data/data_1_" + domain_name + ".qcow2";
    uint64_t disk_total_size = task_resource->disks.begin()->second / 1024L / 1024L; // GB
    LOG_INFO << "data_file: " << data_file;
    TASK_LOG_INFO(domain_name, "data_file: " << data_file);
    if (data_file_name.empty()) {
        std::string cmd_create_img = "qemu-img create -f qcow2 " + data_file + " " + std::to_string(disk_total_size) + "G";
        LOG_INFO << "create qcow2 image(data): " << cmd_create_img;
        std::string create_ret = run_shell(cmd_create_img.c_str());
        LOG_INFO << "create qcow2 image(data) result: " << create_ret;
        TASK_LOG_INFO(domain_name, "create qcow2 image(data): " << cmd_create_img << ", result: " << create_ret);
    } else {
        boost::filesystem::copy_file("/data/" + data_file_name, data_file);
    }

    if (!multicast.empty()) {
        for (const auto & mcast : multicast) {
            TASK_LOG_INFO(domain_name, "add multicast address: " << mcast);
        }
    }

    // vnc
    LOG_INFO << "vnc port: " << task_resource->vnc_port << ", password: " << task_resource->vnc_password;
    TASK_LOG_INFO(domain_name, "vnc port: " << task_resource->vnc_port << ", password: " << task_resource->vnc_password);

    std::string xml_content = createXmlStr(buf_uuid, domain_name, memoryTotal,
                                           cpuNumTotal, task_resource->cpu_sockets,
                                           task_resource->cpu_cores, task_resource->cpu_threads,
                                           vga_pci, to_image_path, data_file,
                                           task_resource->vnc_port, task_resource->vnc_password,
                                           multicast,
                                           is_windows, uefi);

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;
    do {
        domainPtr = virDomainDefineXML(m_connPtr, xml_content.c_str());
        if (nullptr == domainPtr) {
            errorNum = E_VIRT_DOMAIN_EXIST;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainDefineXML error: " << (error ? error->message : ""));
            break;
        }

        if (virDomainCreate(domainPtr) < 0) {
            errorNum = E_VIRT_INTERNAL_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainCreate error: " << (error ? error->message : ""));
        }
    } while (0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::StartDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_NOSTATE || info.state == VIR_DOMAIN_SHUTOFF) {
            if (virDomainCreate(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainCreate error: " << (error ? error->message : ""));
            }
        } else if (info.state == VIR_DOMAIN_PMSUSPENDED) {
            if (virDomainPMWakeup(domainPtr, 0) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainPMWakeup error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::SuspendDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainSuspend(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainSuspend error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::ResumeDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_PMSUSPENDED) {
            if (virDomainResume(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainResume error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::RebootDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainReboot(domainPtr, VIR_DOMAIN_REBOOT_DEFAULT) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainReboot error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::ShutdownDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainShutdown(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainShutdown error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::DestroyDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state != VIR_DOMAIN_SHUTOFF) {
            if (virDomainDestroy(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainDestroy error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::UndefineDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        if (virDomainUndefine(domainPtr) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainUndefine error: " << (error ? error->message : ""));
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::DestroyAndUndefineDomain(const std::string &domain_name, unsigned int undefineFlags) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state != VIR_DOMAIN_SHUTOFF) {
            if (virDomainDestroy(domainPtr) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainDestroy error: " << (error ? error->message : ""));
            }
        }

        int virRet = 0;
        if (undefineFlags == 0) {
            virRet = virDomainUndefine(domainPtr);
        } else {
            virRet = virDomainUndefineFlags(domainPtr, undefineFlags);
        }
        if (virRet < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainUndefine error: " << (error ? error->message : ""));
            if (error) {
                virFreeError(error);
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

int32_t VmClient::ResetDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return E_DEFAULT;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = E_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = E_DEFAULT;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainReset(domainPtr, VIR_DOMAIN_REBOOT_DEFAULT) < 0) {
                errorNum = E_DEFAULT;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainReset error: " << (error ? error->message : ""));
            }
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

void VmClient::ListAllDomains(std::vector<std::string> &domains) {

}

void VmClient::ListAllRunningDomains(std::vector<std::string> &domains) {

}

virDomainState VmClient::GetDomainStatus(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return VIR_DOMAIN_NOSTATE;
    }

    virDomainState vm_status = VIR_DOMAIN_NOSTATE;
    virDomainPtr domainPtr = nullptr;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        vm_status = (virDomainState) info.state;
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return vm_status;
}

FResult VmClient::GetDomainLog(const std::string &domain_name, ETaskLogDirection direction, int32_t linecount, std::string &log_content) {
    LOG_INFO << "get domain " << domain_name << " log, direction: " << direction << ", line: " << linecount;
    std::string latest_log;
    int max_num = -1;
    try {
        boost::filesystem::path logpath = util::get_exe_dir() /= "logs";
        boost::filesystem::directory_iterator iterend;
        for (boost::filesystem::directory_iterator iter(logpath); iter != iterend; iter++) {
            if (boost::filesystem::is_regular_file(iter->path()) && iter->path().string().find(domain_name) != std::string::npos) {
                size_t pos1 = iter->path().string().find_last_of("_");
                size_t pos2 = iter->path().string().find_last_of(".");
                int num = atoi(iter->path().string().substr(pos1+1, pos2 - pos1 - 1).c_str());
                if (num > max_num) {
                    latest_log = iter->path().string();
                    max_num = num;
                }
            }
        }
    }
    catch (const std::exception & e) {
        return {E_DEFAULT, std::string("log file error: ").append(e.what())};
    }
    catch (const boost::exception & e) {
        return {E_DEFAULT, "log file error: " + diagnostic_information(e)};
    }
    catch (...) {
        return {E_DEFAULT, "unknowned log file error"};
    }
    
    if (latest_log.empty() || max_num < 0) {
        return {E_DEFAULT, "task log not exist"};
    }

    log_content.clear();
    std::ifstream file(latest_log, std::ios_base::in);
    if (file.is_open()) {
        if (direction == GET_LOG_HEAD) {
            file.seekg(0, std::ios_base::beg);
        }
        else {
            file.seekg(-2, std::ios_base::end);
            for (int i = 0; i < linecount; i++) {
                // seek to the previous line
                while (file.peek() != file.widen('\n')) {
                    if (file.tellg() == 0) break;
                    file.seekg(-1, std::ios::cur);
                }
                if (file.tellg() == 0) break;
                file.seekg(-1, std::ios::cur);
            }
            if (file.tellg() != 0) {
                file.seekg(2, std::ios::cur);
            }
        }
        // read lines
        std::string  line;
        while (getline(file, line) && linecount-- > 0) {
            if (log_content.size() + line.size() >= MAX_LOG_CONTENT_SIZE) {
                break;
            }
            if (!log_content.empty()) {
                log_content.append(",");
            }
            log_content.append("\"" + line + "\"");
        }
        file.close();
        return {E_SUCCESS, ""};
    }
    
    return {E_DEFAULT, "open log file error"};
}

std::string VmClient::GetDomainLocalIP(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return "";
    }

    std::string vm_local_ip;
    virDomainPtr domainPtr = nullptr;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            break;
        }

        int32_t try_count = 0;
        while (vm_local_ip.empty() && try_count < 1000) {
            LOG_INFO << "transform_port try_count: " << (try_count + 1);
            if ((try_count + 1) % 300 == 0) { // Approximately 15 minutes
                LOG_INFO << "retry destroy and start domain " << domain_name;
                if (virDomainDestroy(domainPtr) < 0) {
                    virErrorPtr error = virGetLastError();
                    LOG_ERROR << "retry destroy domain " << domain_name << " error: " << error ? error->message : "";
                    if (error) virFreeError(error);
                    break;
                }
                if (virDomainCreate(domainPtr) < 0) {
                    virErrorPtr error = virGetLastError();
                    LOG_ERROR << "retry create domain " << domain_name << " error: " << error ? error->message : "";
                    if (error) virFreeError(error);
                    break;
                }
                sleep(3);
            }
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
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return vm_local_ip;
}

int32_t VmClient::GetDomainAgentInterfaceAddress(const std::string& domain_name, std::vector<std::tuple<std::string, std::string>> &address) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return -1;
    }

    virDomainPtr domainPtr = nullptr;
    virDomainInterfacePtr *ifaces = nullptr;
    int ifaces_count = -1;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            break;
        }

        ifaces_count = virDomainInterfaceAddresses(domainPtr, &ifaces,
            VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_AGENT, 0);
        for (int i = 0; i < ifaces_count; i++) {
            /*
            std::string if_name = ifaces[i]->name;
            std::string if_hwaddr;
            if (ifaces[i]->hwaddr)
                if_hwaddr = ifaces[i]->hwaddr;
            */
            for (int j = 0; j < ifaces[i]->naddrs; j++) {
                virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
                //printf("[addr: %s prefix: %d type: %d]", ip_addr->addr, ip_addr->prefix, ip_addr->type);
                if (ip_addr->type == 0 && strncmp(ip_addr->addr, "127.", 4) != 0 && strncmp(ip_addr->addr, "192.168.", 8) != 0) {
                    address.push_back(std::make_tuple(ifaces[i]->hwaddr, ip_addr->addr));
                }
            }
        }
    } while(0);

    if (ifaces && ifaces_count > 0) {
        for (int i = 0; i < ifaces_count; i++) {
            virDomainInterfaceFree(ifaces[i]);
        }
    }
    if (ifaces)
        free(ifaces);
    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }
    return ifaces_count;
}

bool VmClient::SetDomainUserPassword(const std::string &domain_name, const std::string &username, const std::string &pwd) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return false;
    }

    bool ret = false;
    virDomainPtr domain_ptr = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            ret = false;
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            break;
        }

        int try_count = 0;
        int succ = -1;
        // max: 5min
        while (succ != 0 && try_count < 100) {
            TASK_LOG_ERROR(domain_name, "set_vm_password try_count: " << (try_count + 1));
            succ = virDomainSetUserPassword(domain_ptr, username.c_str(), pwd.c_str(), 0);
            if (succ != 0) {
                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainSetUserPassword error: " << (error ? error->message : ""));
            }

            try_count++;
            sleep(3);
        }

        if (succ == 0) {
            ret = true;
            TASK_LOG_INFO(domain_name, "set vm user password successful, user:" << username << ", pwd:" << pwd);
        } else {
            ret = false;
            TASK_LOG_ERROR(domain_name, "set vm user password failed, user:" << username << ", pwd:" << pwd);
        }
    } while(0);

    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }

    return ret;
}

bool VmClient::IsExistDomain(const std::string &domain_name) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return false;
    }

    virDomainPtr domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
    if (nullptr == domainPtr) {
        return false;
    } else {
        virDomainFree(domainPtr);
        return true;
    }
}

bool VmClient::ListDomainDiskInfo(const std::string& domain_name, std::map<std::string, domainDiskInfo>& disks){
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return false;
    }

    bool ret = false;
    virDomainPtr domain_ptr = nullptr;
    char* pContent = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            break;
        }
        pContent = virDomainGetXMLDesc(domain_ptr, VIR_DOMAIN_XML_SECURE);
        if (!pContent) {
            LOG_ERROR << domain_name << " get domain xml desc failed";
            break;
        }
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            LOG_ERROR << domain_name << " parse xml desc failed";
            break;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* devices_node = root->FirstChildElement("devices");
        if (!devices_node) {
            LOG_ERROR << domain_name << "not find devices node";
            break;
        }
        tinyxml2::XMLElement* disk_node = devices_node->FirstChildElement("disk");
        while (disk_node) {
            domainDiskInfo ddInfo;
            tinyxml2::XMLElement* disk_driver_node = disk_node->FirstChildElement("driver");
            if (disk_driver_node) {
                ddInfo.driverName = disk_driver_node->Attribute("name");
                ddInfo.driverType = disk_driver_node->Attribute("type");
            }
            tinyxml2::XMLElement* disk_source_node = disk_node->FirstChildElement("source");
            if (disk_source_node) {
                ddInfo.sourceFile = disk_source_node->Attribute("file");
            }
            tinyxml2::XMLElement* disk_target_node = disk_node->FirstChildElement("target");
            if (disk_target_node) {
                ddInfo.targetDev = disk_target_node->Attribute("dev");
                ddInfo.targetBus = disk_target_node->Attribute("bus");
            }
            disks[ddInfo.targetDev] = ddInfo;
            disk_node = disk_node->NextSiblingElement("disk");
        }
        ret = true;
    } while(0);

    if (pContent) {
        free(pContent);
    }
    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }
    return ret;
}

int32_t VmClient::IsDomainHasNvram(const std::string& domain_name) {
    int32_t ret = -1;
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return ret;
    }

    virDomainPtr domain_ptr = nullptr;
    char* pContent = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            break;
        }
        pContent = virDomainGetXMLDesc(domain_ptr, 0);
        if (!pContent) {
            LOG_ERROR << domain_name << " get domain xml desc failed";
            break;
        }
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            LOG_ERROR << domain_name << " parse xml desc failed";
            break;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* os_node = root->FirstChildElement("os");
        if (os_node) {
            tinyxml2::XMLElement* os_nvram_node = os_node->FirstChildElement("nvram");
            if (os_nvram_node && os_nvram_node->GetText() != NULL) {
                ret = 1;
                break;
            }
        }
        ret = 0;
    } while(0);

    if (pContent) {
        free(pContent);
    }
    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }
    return ret;
}

FResult VmClient::CreateSnapshot(const std::string& domain_name, const std::shared_ptr<dbc::snapshotInfo>& info) {
    int32_t ret_code = E_DEFAULT;
    std::string ret_msg;
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        ret_msg = "connPtr is nullptr";
        return {ret_code, ret_msg};
    }

    std::string snapXMLDesc = createSnapshotXml(info);
    if (snapXMLDesc.empty()) {
        LOG_ERROR << domain_name << " snapshot xml is empty";
        ret_msg = "domain snapshot xml is empty";
        return {ret_code, ret_msg};
    }

    virDomainPtr domainPtr = nullptr;
    virDomainSnapshotPtr snapshotPtr = nullptr;
    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domainPtr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            ret_msg = "lookup domain:" + domain_name + " is nullptr";
            break;
        }

        unsigned int createFlags = VIR_DOMAIN_SNAPSHOT_CREATE_HALT | VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY |
                                   VIR_DOMAIN_SNAPSHOT_CREATE_QUIESCE | VIR_DOMAIN_SNAPSHOT_CREATE_ATOMIC;
        bool bExternal = false;
        for (const auto& disk : info->disks) {
            if (disk.snapshot == "external") {
                bExternal = true;
                break;
            }
        }
        snapshotPtr = virDomainSnapshotCreateXML(domainPtr, snapXMLDesc.c_str(), bExternal ? createFlags : 0);
        if (snapshotPtr == nullptr) {
            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainSnapshotCreateXML error: " << (error ? error->message : ""));
            ret_code = error ? error->code : E_VIRT_INTERNAL_ERROR;
            ret_msg = error ? error->message : "virDomainSnapshotCreateXML error";
            if (error) {
                virFreeError(error);
            }
            break;
        }
        ret_code = E_SUCCESS;
    } while (0);

    if (nullptr != snapshotPtr) {
        virDomainSnapshotFree(snapshotPtr);
    }
    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return {ret_code, ret_msg};
}

std::shared_ptr<dbc::snapshotInfo> VmClient::GetDomainSnapshot(const std::string& domain_name, const std::string& snapshot_name) {
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return nullptr;
    }

    virDomainPtr domain_ptr = nullptr;
    virDomainSnapshotPtr snapshot_ptr = nullptr;
    char* pContent = nullptr;
    std::shared_ptr<dbc::snapshotInfo> ssInfo = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            break;
        }

        snapshot_ptr = virDomainSnapshotLookupByName(domain_ptr, snapshot_name.c_str(), 0);
        if (snapshot_ptr == nullptr) {
            LOG_ERROR << " lookup snapshot:" << snapshot_name << " is nullptr";
            break;
        }

        pContent = virDomainSnapshotGetXMLDesc(snapshot_ptr, 0);
        if (pContent == nullptr) {
            LOG_ERROR << "snapshot:" << snapshot_name << " virDomainSnapshotGetXMLDesc return nullptr";
            break;
        }

        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            LOG_ERROR << "parse domain snapshot xml desc error: " << err << ", snap: " << snapshot_name;
            break;
        }

        ssInfo = std::make_shared<dbc::snapshotInfo>();
        ssInfo->__set_name(snapshot_name);
        
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement *desc = root->FirstChildElement("description");
        if (desc) {
            ssInfo->__set_description(desc->GetText());
        }
        tinyxml2::XMLElement *state = root->FirstChildElement("state");
        if (state) {
            ssInfo->__set_state(state->GetText());
        }
        tinyxml2::XMLElement *creationTime = root->FirstChildElement("creationTime");
        if (creationTime) {
            ssInfo->__set_creationTime(atoll(creationTime->GetText()));
        }
        tinyxml2::XMLElement *disks = root->FirstChildElement("disks");
        if (disks) {
            std::vector<dbc::snapshotDiskInfo> vecDisk;
            tinyxml2::XMLElement *disk = disks->FirstChildElement("disk");
            while (disk) {
                dbc::snapshotDiskInfo dsinfo;
                dsinfo.__set_name(disk->Attribute("name"));
                dsinfo.__set_snapshot(disk->Attribute("snapshot"));
                tinyxml2::XMLElement *driver = disk->FirstChildElement("driver");
                if (driver) {
                    dsinfo.__set_driver_type(driver->Attribute("type"));
                }
                tinyxml2::XMLElement *source = disk->FirstChildElement("source");
                if (source) {
                    dsinfo.__set_source_file(source->Attribute("file"));
                }
                vecDisk.push_back(dsinfo);
                disk = disk->NextSiblingElement("disk");
            }
            if (!vecDisk.empty()) {
                ssInfo->__set_disks(vecDisk);
            }
        }
        ssInfo->__set_error_code(0);
        ssInfo->__set_error_message("");
    } while(0);

    if (pContent) {
        free(pContent);
    }
    if (snapshot_ptr) {
        virDomainSnapshotFree(snapshot_ptr);
    }
    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }
    return ssInfo;
}

/*
vm_worker::vm_worker()
        : m_vm_ip(DEFAULT_LOCAL_IP), m_vm_port((uint16_t) std::stoi(DEFAULT_VM_LISTEN_PORT)),
          m_vm_client(std::make_shared<virt_client>(m_vm_ip, m_vm_port)) {
}

int32_t vm_worker::load_vm_config() {
    ip_validator ip_vdr;
    port_validator port_vdr;

    //virt ip
    const std::string &vm_ip = CONF_MANAGER->get_vm_ip();

    variable_value val;
    val.value() = vm_ip;

    if (false == ip_vdr.validate(val)) {
        LOG_ERROR << "ai power provider init conf invalid virt ip: " << vm_ip;
        return E_DEFAULT;
    }

    m_vm_ip = vm_ip;

    //vm port
    std::string s_port = CONF_MANAGER->get_vm_port();
    val.value() = s_port;

    if (false == port_vdr.validate(val)) {
        LOG_ERROR << "ai power provider init conf invalid virt port: " << s_port;
        return E_DEFAULT;
    } else {
        try {
            m_vm_port = (uint16_t) std::stoi(s_port);
        }
        catch (const std::exception &e) {
            LOG_ERROR << "ai power provider service init conf virt port: " << s_port << ", " << e.what();
            return E_DEFAULT;
        }
    }

    m_vm_client->set_address(m_vm_ip, m_vm_port);

    //file path
    const fs::path &vm_path = env_manager::get_vm_path();
    bpo::options_description vm_opts("vm file options");

    vm_opts.add_options()
            ("memory", bpo::value<int16_t>()->default_value(90), "")
            ("memory_swap", bpo::value<int16_t>()->default_value(90), "")
            ("cpus", bpo::value<int16_t>()->default_value(95), "")
            ("shm_size", bpo::value<int64_t>()->default_value(0), "")
            ("host_volum_dir", bpo::value<std::string>()->default_value(""), "")
            ("auto_pull_image", bpo::value<bool>()->default_value(true), "")
            ("engine_reg", bpo::value<std::string>()->default_value(""), "");

    try {
        //vm.conf
        std::ifstream conf_ifs(vm_path.generic_string());
        bpo::store(bpo::parse_config_file(conf_ifs, vm_opts), m_vm_args);

        bpo::notify(m_vm_args);

        std::string host_dir = m_vm_args["host_volum_dir"].as<std::string>();

        if (!host_dir.empty() && !fs::exists(host_dir)) {
            LOG_ERROR << "host volum dir is not exist. pls check";
            return E_DEFAULT;
        }

        if (!host_dir.empty() && !fs::is_empty(host_dir)) {
            LOG_WARNING << "dangerous: host_volum_dir contain files. pls check";
        }

        int32_t ret = check_cpu_config(m_vm_args["cpus"].as<int16_t>());

        if (ret != E_SUCCESS) {
            return ret;
        }

        ret = check_memory_config(m_vm_args["memory"].as<int16_t>(), m_vm_args["memory_swap"].as<int16_t>(),
                                  m_vm_args["shm_size"].as<int64_t>());
        if (ret != E_SUCCESS) {
            return ret;
        }
        m_auto_pull_image = m_vm_args["auto_pull_image"].as<bool>();
        set_task_engine(m_vm_args["engine_reg"].as<std::string>());
    }
    catch (const boost::exception &e) {
        LOG_ERROR << "parse vm.conf error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t vm_worker::check_cpu_config(const int16_t &cpu_info) {
    if (cpu_info < 0 || cpu_info > 100) {
        LOG_ERROR << "cpus config error. cpus value is in [0,100]";
        return E_DEFAULT;
    }
    uint16_t cpu_num = std::thread::hardware_concurrency();
    m_nano_cpus = cpu_info * m_nano_cpu * cpu_num / 100;

    return E_SUCCESS;
}

int32_t vm_worker::check_memory_config(const int16_t &memory, const int16_t &memory_swap, const int64_t &shm_size) {
    if (memory < 0 || memory > 100) {
        LOG_ERROR << "memory config error. memeory value is in [0,100]";
        return E_DEFAULT;
    }

    if (memory_swap < 0 || memory_swap > 100) {
        LOG_ERROR << "memory_swap config error. memory_swap value is in [0,100]";
        return E_DEFAULT;
    }

    int64_t sys_mem = 0;
    int64_t sys_swap = 0;
    get_sys_mem(sys_mem, sys_swap);
    LOG_DEBUG << "system memory:" << sys_mem << " system swap memory" << sys_swap;

    if (0 == sys_mem || 0 == sys_swap) {
        return E_SUCCESS;
    }

    m_memory = memory * sys_mem / 100;
    m_memory_swap = memory_swap * sys_swap / 100;
    m_shm_size = shm_size * m_g_bytes;

    if (m_shm_size > sys_mem) {
        LOG_ERROR << "check shm_size failed.";
        return E_DEFAULT;
    }

    if (m_memory_swap != 0 && m_memory > m_memory_swap) {
        LOG_ERROR << "memory is bigger than memory_swap, pls check vm.conf.system memory=" << sys_mem
                  << " sys_swap=" << sys_swap
                  << " vm memory:" << m_memory << " vm memory_swap:" << m_memory_swap;
        return E_DEFAULT;
    }

    if (m_memory != 0 && m_shm_size > m_memory) {
        LOG_ERROR << "check shm_size failed. shm_size is bigger than memory";
        return E_DEFAULT;
    }

    LOG_DEBUG << "If vm memory is 0, use default.vm memory:" << m_memory << " vm swap memory" << m_memory_swap;

    return E_SUCCESS;
}

std::string vm_worker::get_operation(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }
    auto customer_setting = task->server_specification;
    std::string operation = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"operation\"):" << pt.count("operation");
            if (pt.count("operation") != 0) {
                operation = pt.get<std::string>("operation");
                LOG_INFO << "operation: " << operation;

            }


        }
        catch (...) {
            LOG_INFO << "operation: " << "error";
        }
    }

    return operation;
}

std::string vm_worker::get_old_gpu_id(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }
    auto customer_setting = task->server_specification;
    std::string old_gpu_id = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"old_gpu_id\"):" << pt.count("old_gpu_id");
            if (pt.count("old_gpu_id") != 0) {
                old_gpu_id = pt.get<std::string>("old_gpu_id");
                LOG_INFO << "old_gpu_id: " << old_gpu_id;

            }


        }
        catch (...) {
            LOG_INFO << "old_gpu_id: " << "error";
        }
    }

    return old_gpu_id;
}

std::string vm_worker::get_new_gpu_id(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }
    auto customer_setting = task->server_specification;
    std::string new_gpu_id = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"new_gpu_id\"):" << pt.count("new_gpu_id");
            if (pt.count("new_gpu_id") != 0) {
                new_gpu_id = pt.get<std::string>("new_gpu_id");
                LOG_INFO << "new_gpu_id: " << new_gpu_id;

            }


        }
        catch (...) {
            LOG_INFO << "new_gpu_id: " << "error";
        }
    }

    return new_gpu_id;
}

std::string vm_worker::get_autodbcimage_version(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }
    auto customer_setting = task->server_specification;
    std::string autodbcimage_version = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;

            LOG_INFO << "pt.count(\"autodbcimage_version\"):" << pt.count("autodbcimage_version");
            if (pt.count("autodbcimage_version") != 0) {


                autodbcimage_version = pt.get<std::string>("autodbcimage_version");

                LOG_INFO << "autodbcimage_version: " << autodbcimage_version;
            }


        }
        catch (...) {
            LOG_INFO << "operation: " << "error";
        }
    }

    return autodbcimage_version;
}


std::shared_ptr<update_vm_config> vm_worker::get_update_vm_config(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }

    std::shared_ptr<update_vm_config> config = std::make_shared<update_vm_config>();


    //  auto nv_docker_ver = get_nv_docker_version();

    //   if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
    //    {
    LOG_DEBUG << "get common attributes of nvidia docker";

    // jimmy: support NVIDIA GPU env configure from task spec
    std::map<std::string, std::string> env_map;
    env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
    env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

    auto customer_setting = task->server_specification;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);

            for (const auto &kv: pt.get_child("env")) {
                env_map[kv.first.data()] = kv.second.data();
                LOG_INFO << "[env] " << kv.first.data() << "=" << env_map[kv.first.data()];
            }
        }
        catch (...) {

        }
    }


    for (auto &kv: env_map) {
        config->env.push_back(kv.first + "=" + kv.second);
    }



//memory
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "pt.count(\"memory\"):" << pt.count("memory");
            if (pt.count("memory") != 0) {
                int64_t memory = pt.get<int64_t>("memory");
                config->memory = memory;


                LOG_INFO << "memory: " << memory;

            }
            LOG_INFO << "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
            if (pt.count("memory_swap") != 0) {


                int64_t memory_swap = pt.get<int64_t>("memory_swap");
                config->memory_swap = memory_swap;
                LOG_INFO << "memory_swap: " << memory_swap;
            }

            LOG_INFO << "pt.count(\"disk_quota\"):" << pt.count("disk_quota");
            if (pt.count("disk_quota") != 0) {


                int64_t disk_quota = pt.get<int64_t>("disk_quota");
                config->disk_quota = disk_quota;
                LOG_INFO << "disk_quota: " << disk_quota;
            }

            LOG_INFO << "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
            if (pt.count("cpu_shares") != 0) {


                int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                config->cpu_shares = cpu_shares;
                LOG_INFO << "cpu_shares: " << cpu_shares;
            }

            LOG_INFO << "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
            if (pt.count("cpu_period") != 0) {


                int32_t cpu_period = pt.get<int32_t>("cpu_period");
                config->cpu_period = cpu_period;
                LOG_INFO << "cpu_period: " << cpu_period;
            }

            LOG_INFO << "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
            if (pt.count("cpu_quota") != 0) {


                int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                config->cpu_quota = cpu_quota;
                LOG_INFO << "cpu_quota: " << cpu_quota;
            }


            LOG_INFO << "pt.count(\"gpus\"):" << pt.count("gpus");
            if (pt.count("gpus") != 0) {


                int32_t gpus = pt.get<int32_t>("gpus");
                config->gpus = gpus;
                LOG_INFO << "gpus: " << gpus;
            }

            LOG_INFO << "pt.count(\"storage\"):" << pt.count("storage");
            if (pt.count("storage") != 0) {


                std::string storage = pt.get<std::string>("storage");
                config->storage = storage;
                LOG_INFO << "storage: " << storage;
            }


        }
        catch (...) {

        }
    }



    //      }



    return config;
}

int64_t vm_worker::get_sleep_time(std::shared_ptr<ai_training_task> task) {

    int64_t sleep_time = 120;
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return sleep_time;
    }


    auto customer_setting = task->server_specification;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "pt.count(\"sleep_time\"):" << pt.count("sleep_time");
            if (pt.count("sleep_time") != 0) {


                sleep_time = pt.get<int64_t>("sleep_time");

                LOG_INFO << "sleep_time: " << sleep_time;

            }

        }
        catch (...) {

        }
    }

    return sleep_time;


}


std::shared_ptr<vm_config> vm_worker::get_vm_config_from_image(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }

    std::shared_ptr<vm_config> config = std::make_shared<vm_config>();

    //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
    //dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py hyperparameters
    //download file + exec training
    std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
    exec_cmd += AI_TRAINING_TASK_SCRIPT;

    std::string start_cmd = task->entry_file + " " + task->hyper_parameters;

    vm_mount mount1;
    mount1.target = "/proc/cpuinfo";
    mount1.source = "/var/lib/lxcfs/proc/cpuinfo";
    mount1.type = "bind";
    mount1.read_only = false;
    mount1.consistency = "consistent";

    vm_mount mount2;
    mount2.target = "/proc/diskstats";
    mount2.source = "/var/lib/lxcfs/proc/diskstats";
    mount2.type = "bind";
    mount2.read_only = false;
    mount2.consistency = "consistent";

    vm_mount mount3;
    mount3.target = "/proc/meminfo";
    mount3.source = "/var/lib/lxcfs/proc/meminfo";
    mount3.type = "bind";
    mount3.read_only = false;
    mount3.consistency = "consistent";

    vm_mount mount4;
    mount4.target = "/proc/swaps";
    mount4.source = "/var/lib/lxcfs/proc/swaps";
    mount4.type = "bind";
    mount4.read_only = false;
    mount4.consistency = "consistent";

    config->host_config.mounts.push_back(mount1);
    config->host_config.mounts.push_back(mount2);
    config->host_config.mounts.push_back(mount3);
    config->host_config.mounts.push_back(mount4);
    //  config->volumes.dests.push_back("/proc/stat");
    //   config->volumes.binds.push_back("/var/lib/lxcfs/proc/stat");
    //  config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/swaps");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/swaps");
    //  config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/uptime");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/uptime");
    //  config->volumes.modes.push_back("rw");

    config->image = task->training_engine;
    config->cmd.push_back(exec_cmd);
    config->cmd.push_back(task->data_dir);
    config->cmd.push_back(task->code_dir);
    config->cmd.push_back(start_cmd);

    config->host_config.binds.push_back(AI_TRAINING_BIND_LOCALTIME);
    config->host_config.binds.push_back(AI_TRAINING_BIND_TIMEZONE);

    // config->host_config.memory = m_memory;
    // config->host_config.memory_swap = m_memory_swap;

    // config->host_config.memory =task->memory;
    //  config->host_config.memory_swap = task->memory_swap;
    config->host_config.nano_cpus = m_nano_cpus;

    std::string mount_dbc_data_dir = m_vm_args["host_volum_dir"].as<std::string>();

    if (!mount_dbc_data_dir.empty()) {
        mount_dbc_data_dir = mount_dbc_data_dir + ":" + "/dbc";
        config->host_config.binds.push_back(mount_dbc_data_dir);
    }

    std::string mount_dbc_utils_dir = env_manager::get_home_path().generic_string();

    if (!mount_dbc_utils_dir.empty()) {
        // read only
        mount_dbc_utils_dir = mount_dbc_utils_dir + "/vm:" + "/home/dbc_utils:ro";
        config->host_config.binds.push_back(mount_dbc_utils_dir);
    }


    //    auto nv_docker_ver = get_nv_docker_version();

    //     if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
    //     {
    LOG_DEBUG << "get common attributes of nvidia docker";

    // jimmy: support NVIDIA GPU env configure from task spec
    std::map<std::string, std::string> env_map;
    env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
    env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

    auto customer_setting = task->server_specification;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);

            for (const auto &kv: pt.get_child("env")) {
                env_map[kv.first.data()] = kv.second.data();
                LOG_INFO << "[env] " << kv.first.data() << "=" << env_map[kv.first.data()];
            }
        }
        catch (...) {

        }
    }

//                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
//                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
    for (auto &kv: env_map) {
        config->env.push_back(kv.first + "=" + kv.second);
    }

    config->host_config.share_memory = m_shm_size;
    config->host_config.ulimits.push_back(vm_ulimits("memlock", -1, -1));

    // port binding
    vm_port port;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);

            for (const auto &kv: pt.get_child("port")) {

                vm_port p;
                p.scheme = "tcp";
                p.port = kv.first.data();
                p.host_ip = "";
                p.host_port = kv.second.data();

                config->host_config.port_bindings.ports[p.port] = p;

                LOG_DEBUG << "[port] " << kv.first.data() << " = " << p.host_port;
            }
        }
        catch (...) {

        }
    }
//memory
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "pt.count(\"memory\"):" << pt.count("memory");
            if (pt.count("memory") != 0) {
                int64_t memory = pt.get<int64_t>("memory");
                config->host_config.memory = memory;


                LOG_INFO << "memory: " << memory;

            }
            LOG_INFO << "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
            if (pt.count("memory_swap") != 0) {


                int64_t memory_swap = pt.get<int64_t>("memory_swap");
                config->host_config.memory_swap = memory_swap;
                LOG_INFO << "memory_swap: " << memory_swap;
            }

            LOG_INFO << "pt.count(\"storage\"):" << pt.count("storage");
            if (pt.count("storage") != 0) {


                std::string storage = pt.get<std::string>("storage");
                config->host_config.storage = storage;
                LOG_INFO << "storage: " << storage;
            }

            LOG_INFO << "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
            if (pt.count("cpu_shares") != 0) {


                int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                config->host_config.cpu_shares = cpu_shares;
                LOG_INFO << "cpu_shares: " << cpu_shares;
            }


            LOG_INFO << "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
            if (pt.count("cpu_period") != 0) {


                int32_t cpu_period = pt.get<int32_t>("cpu_period");
                config->host_config.cpu_period = cpu_period;
                LOG_INFO << "cpu_period: " << cpu_period;
            }

            LOG_INFO << "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
            if (pt.count("cpu_quota") != 0) {


                int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                config->host_config.cpu_quota = cpu_quota;
                LOG_INFO << "cpu_quota: " << cpu_quota;
            }


            LOG_INFO << "pt.count(\"autodbcimage_version\"):" << pt.count("autodbcimage_version");
            if (pt.count("autodbcimage_version") != 0) {


                int32_t autodbcimage_version = pt.get<int32_t>("autodbcimage_version");
                config->host_config.autodbcimage_version = autodbcimage_version;
                LOG_INFO << "autodbcimage_version: " << autodbcimage_version;
            }


            LOG_INFO << "pt.count(\"privileged\"):" << pt.count("privileged");
            if (pt.count("privileged") != 0) {


                std::string privileged = pt.get<std::string>("privileged");
                if (privileged.compare("false") == 0) {
                    config->host_config.privileged = false;
                } else {

                    config->host_config.privileged = true;

                }

            }


        }
        catch (...) {

        }
    }

    // LOG_INFO << "config->host_config.memory"+config->host_config.memory;

    //    }

    //    switch (nv_docker_ver)
    //    {
    //        case NVIDIA_DOCKER_TWO:
    //        {
    //            LOG_INFO << "nvidia docker 2.x";
    //            config->host_config.runtime = RUNTIME_NVIDIA;
    //            break;
    //        }
    //        default:
    //        {
    //            LOG_INFO << "not find nvidia docker";
    //        }
    //    }

    config->host_config.runtime = RUNTIME_NVIDIA;
    return config;
}


std::shared_ptr<vm_config> vm_worker::get_vm_config(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        LOG_ERROR << "ai power provider service get vm config task or nv_config is nullptr";
        return nullptr;
    }

    std::shared_ptr<vm_config> config = std::make_shared<vm_config>();

    //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
    //dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py hyperparameters
    //download file + exec training
    std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
    exec_cmd += AI_TRAINING_TASK_SCRIPT;

    std::string start_cmd = task->entry_file + " " + task->hyper_parameters;


    //  config->volumes.dests.push_back("/proc/cpuinfo");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/cpuinfo");
    //   config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/diskstats");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/diskstats");
    //  config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/meminfo");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/meminfo");
    //  config->volumes.modes.push_back("rw");

    vm_mount mount1;
    mount1.target = "/proc/cpuinfo";
    mount1.source = "/var/lib/lxcfs/proc/cpuinfo";
    mount1.type = "bind";
    mount1.read_only = false;
    mount1.consistency = "consistent";

    vm_mount mount2;
    mount2.target = "/proc/diskstats";
    mount2.source = "/var/lib/lxcfs/proc/diskstats";
    mount2.type = "bind";
    mount2.read_only = false;
    mount2.consistency = "consistent";

    vm_mount mount3;
    mount3.target = "/proc/meminfo";
    mount3.source = "/var/lib/lxcfs/proc/meminfo";
    mount3.type = "bind";
    mount3.read_only = false;
    mount3.consistency = "consistent";

    vm_mount mount4;
    mount4.target = "/proc/swaps";
    mount4.source = "/var/lib/lxcfs/proc/swaps";
    mount4.type = "bind";
    mount4.read_only = false;
    mount4.consistency = "consistent";

    config->host_config.mounts.push_back(mount1);
    config->host_config.mounts.push_back(mount2);
    config->host_config.mounts.push_back(mount3);
    config->host_config.mounts.push_back(mount4);
    //  config->volumes.dests.push_back("/proc/stat");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/stat");
    //  config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/swaps");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/swaps");
    //  config->volumes.modes.push_back("rw");

    //  config->volumes.dests.push_back("/proc/uptime");
    //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/uptime");
    //  config->volumes.modes.push_back("rw");

    config->image = task->training_engine;
    config->cmd.push_back(exec_cmd);
    config->cmd.push_back(task->data_dir);
    config->cmd.push_back(task->code_dir);
    config->cmd.push_back(start_cmd);

    config->host_config.binds.push_back(AI_TRAINING_BIND_LOCALTIME);
    config->host_config.binds.push_back(AI_TRAINING_BIND_TIMEZONE);

    // config->host_config.memory = m_memory;
    // config->host_config.memory_swap = m_memory_swap;

    // config->host_config.memory =task->memory;
    //  config->host_config.memory_swap = task->memory_swap;
    config->host_config.nano_cpus = m_nano_cpus;

    std::string mount_dbc_data_dir = m_vm_args["host_volum_dir"].as<std::string>();

    if (!mount_dbc_data_dir.empty()) {
        mount_dbc_data_dir = mount_dbc_data_dir + ":" + "/dbc";
        config->host_config.binds.push_back(mount_dbc_data_dir);
    }

    std::string mount_dbc_utils_dir = env_manager::get_home_path().generic_string();

    if (!mount_dbc_utils_dir.empty()) {
        // read only
        mount_dbc_utils_dir = mount_dbc_utils_dir + "/vm:" + "/home/dbc_utils:ro";
        config->host_config.binds.push_back(mount_dbc_utils_dir);
    }


    LOG_DEBUG << "get common attributes of nvidia docker";

    // jimmy: support NVIDIA GPU env configure from task spec
    std::map<std::string, std::string> env_map;
    env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
    env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

    auto customer_setting = task->server_specification;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);

            for (const auto &kv: pt.get_child("env")) {
                env_map[kv.first.data()] = kv.second.data();
                LOG_INFO << "[env] " << kv.first.data() << "=" << env_map[kv.first.data()];
            }
        }
        catch (...) {

        }
    }

//                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
//                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
    for (auto &kv: env_map) {
        config->env.push_back(kv.first + "=" + kv.second);
    }

    config->host_config.share_memory = m_shm_size;
    config->host_config.ulimits.push_back(vm_ulimits("memlock", -1, -1));

    // port binding
    vm_port port;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);

            for (const auto &kv: pt.get_child("port")) {

                vm_port p;
                p.scheme = "tcp";
                p.port = kv.first.data();
                p.host_ip = "";
                p.host_port = kv.second.data();

                config->host_config.port_bindings.ports[p.port] = p;

                LOG_DEBUG << "[port] " << kv.first.data() << " = " << p.host_port;
            }
        }
        catch (...) {

        }
    }
//memory
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "pt.count(\"memory\"):" << pt.count("memory");
            if (pt.count("memory") != 0) {
                int64_t memory = pt.get<int64_t>("memory");
                config->host_config.memory = memory;


                LOG_INFO << "memory: " << memory;

            }
            LOG_INFO << "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
            if (pt.count("memory_swap") != 0) {


                int64_t memory_swap = pt.get<int64_t>("memory_swap");
                config->host_config.memory_swap = memory_swap;
                LOG_INFO << "memory_swap: " << memory_swap;
            }

            LOG_INFO << "pt.count(\"storage\"):" << pt.count("storage");
            if (pt.count("storage") != 0) {


                std::string storage = pt.get<std::string>("storage");
                config->host_config.storage = storage;
                LOG_INFO << "storage: " << storage;
            }

            LOG_INFO << "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
            if (pt.count("cpu_shares") != 0) {


                int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                config->host_config.cpu_shares = cpu_shares;
                LOG_INFO << "cpu_shares: " << cpu_shares;
            }

            LOG_INFO << "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
            if (pt.count("cpu_period") != 0) {


                int32_t cpu_period = pt.get<int32_t>("cpu_period");
                config->host_config.cpu_period = cpu_period;
                LOG_INFO << "cpu_period: " << cpu_period;
            }

            LOG_INFO << "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
            if (pt.count("cpu_quota") != 0) {


                int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                config->host_config.cpu_quota = cpu_quota;
                LOG_INFO << "cpu_quota: " << cpu_quota;
            }

            LOG_INFO << "pt.count(\"disk_quota\"):" << pt.count("disk_quota");
            if (pt.count("disk_quota") != 0) {


                int64_t disk_quota = pt.get<int64_t>("disk_quota");
                config->host_config.disk_quota = disk_quota;
                LOG_INFO << "disk_quota: " << disk_quota;
            }

            LOG_INFO << "pt.count(\"privileged\"):" << pt.count("privileged");
            if (pt.count("privileged") != 0) {


                std::string privileged = pt.get<std::string>("privileged");
                if (privileged.compare("false") == 0) {
                    config->host_config.privileged = false;
                } else {

                    config->host_config.privileged = true;
                    // config->entry_point.push_back("/sbin/init;");

                }

            }

        }
        catch (...) {

        }
    }

    // LOG_INFO << "config->host_config.memory"+config->host_config.memory;

    //     }

    //  switch (nv_docker_ver)
    //  {
    //  case NVIDIA_DOCKER_TWO:
    //  {
    //      LOG_INFO << "nvidia docker 2.x";
    //      config->host_config.runtime = RUNTIME_NVIDIA;
    //      break;
    //  }
    //  default:
    //  {
    //      LOG_INFO << "not find nvidia vm";
    //  }
    //  }

    config->host_config.runtime = RUNTIME_NVIDIA;
    return config;
}


std::string vm_worker::get_old_memory(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return "";
    }
    auto customer_setting = task->server_specification;
    std::string old_memory = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"old_memory\"):" << pt.count("old_memory");
            if (pt.count("old_memory") != 0) {
                old_memory = pt.get<std::string>("old_memory");
                LOG_INFO << "old_memory: " << old_memory;

            }
        }
        catch (...) {
            LOG_INFO << "old_memory: " << "error";
        }
    }
    return old_memory;
}

std::string vm_worker::get_new_memory(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return "";
    }
    auto customer_setting = task->server_specification;
    std::string memory = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"memory\"):" << pt.count("memory");
            if (pt.count("memory") != 0) {
                memory = pt.get<std::string>("memory");
                LOG_INFO << "memory: " << memory;

            }


        }
        catch (...) {
            LOG_INFO << "memory: " << "error";
        }
    }

    return memory;
}


std::string vm_worker::get_old_memory_swap(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return "";
    }
    auto customer_setting = task->server_specification;
    std::string old_memory_swap = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"old_memory_swap\"):" << pt.count("old_memory_swap");
            if (pt.count("old_memory_swap") != 0) {
                old_memory_swap = pt.get<std::string>("old_memory_swap");
                LOG_INFO << "old_memory_swap: " << old_memory_swap;

            }


        }
        catch (...) {
            LOG_INFO << "old_memory_swap: " << "error";
        }
    }

    return old_memory_swap;
}

std::string vm_worker::get_new_memory_swap(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return "";
    }
    auto customer_setting = task->server_specification;
    std::string memory_swap = "";
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
            if (pt.count("memory_swap") != 0) {
                memory_swap = pt.get<std::string>("memory_swap");
                LOG_INFO << "memory_swap: " << memory_swap;

            }


        }
        catch (...) {
            LOG_INFO << "memory_swap: " << "error";
        }
    }

    return memory_swap;
}


int32_t vm_worker::get_old_cpu_shares(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return 0;
    }
    auto customer_setting = task->server_specification;
    int32_t old_cpu_shares = 0;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"old_cpu_shares\"):" << pt.count("old_cpu_shares");
            if (pt.count("old_cpu_shares") != 0) {
                old_cpu_shares = pt.get<int32_t>("old_cpu_shares");
                LOG_INFO << "old_cpu_shares: " << old_cpu_shares;

            }


        }
        catch (...) {
            LOG_INFO << "old_cpu_shares: " << "error";
        }
    }

    return old_cpu_shares;
}

int32_t vm_worker::get_new_cpu_shares(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return 0;
    }
    auto customer_setting = task->server_specification;
    int32_t cpu_shares = 0;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
            if (pt.count("cpu_shares") != 0) {
                cpu_shares = pt.get<int32_t>("cpu_shares");
                LOG_INFO << "cpu_shares: " << cpu_shares;

            }


        }
        catch (...) {
            LOG_INFO << "cpu_shares: " << "error";
        }
    }

    return cpu_shares;
}


int32_t vm_worker::get_old_cpu_quota(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return 0;
    }
    auto customer_setting = task->server_specification;
    int32_t old_cpu_quota = 0;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"old_cpu_quota\"):" << pt.count("old_cpu_quota");
            if (pt.count("old_cpu_quota") != 0) {
                old_cpu_quota = pt.get<int32_t>("old_cpu_quota");
                LOG_INFO << "old_cpu_quota: " << old_cpu_quota;

            }


        }
        catch (...) {
            LOG_INFO << "old_cpu_quota: " << "error";
        }
    }

    return old_cpu_quota;
}

int32_t vm_worker::get_new_cpu_quota(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {

        return 0;
    }
    auto customer_setting = task->server_specification;
    int32_t cpu_quota = 0;
    if (!customer_setting.empty()) {
        std::stringstream ss;
        ss << customer_setting;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            LOG_INFO << "task->server_specification" << task->server_specification;
            LOG_INFO << "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
            if (pt.count("cpu_quota") != 0) {
                cpu_quota = pt.get<int32_t>("cpu_quota");
                LOG_INFO << "cpu_quota: " << cpu_quota;

            }


        }
        catch (...) {
            LOG_INFO << "cpu_quota: " << "error";
        }
    }

    return cpu_quota;
}
*/
