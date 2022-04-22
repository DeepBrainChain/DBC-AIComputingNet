#include "vm_client.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "server/server.h"
#include <boost/property_tree/json_parser.hpp>
#include "tinyxml2.h"
#include <uuid/uuid.h>
#include "message/message_id.h"
#include "message/vm_task_result_types.h"
#include "db/db_types/task_snapshotinfo_types.h"
#include "util/utils.h"
#include <libvirt/libvirt-qemu.h>
#include "task/detail/VxlanManager.h"

static const std::string qemu_tcp_url = "qemu+tcp://localhost:16509/system";

static std::string createXmlStr(const std::string& uuid, const std::string& domain_name,
                         int64_t memory, int32_t cpunum, int32_t sockets, int32_t cores, int32_t threads,
                         const std::string& vedio_pci, const std::string & disk_system,
                         const std::vector<std::string>& disk_data, int32_t vnc_port, const std::string& vnc_pwd,
                         const std::vector<std::string>& multicast, const std::string& bridge_name,
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
    os_sub_node->SetAttribute("machine", is_windows ? "q35" : "pc");
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
    if (is_windows) {
        // add at 2022.01.06 -- 使windows虚拟机中任务管理器的cpu显示不再是虚拟机
        tinyxml2::XMLElement* cpu_feature = doc.NewElement("feature");
        cpu_feature->SetAttribute("policy", "disable");
        cpu_feature->SetAttribute("name", "hypervisor");
        cpu_node->LinkEndChild(cpu_feature);
    }
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
    node_suspend_to_mem->SetAttribute("enabled", "no");
    node_pm->LinkEndChild(node_suspend_to_mem);
    tinyxml2::XMLElement* node_suspend_to_disk = doc.NewElement("suspend-to-disk");
    node_suspend_to_disk->SetAttribute("enabled", "no");
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
            if (is_windows) {
                tinyxml2::XMLElement *rom_node = doc.NewElement("rom");
                rom_node->SetAttribute("bar", "off");
                hostdev_node->LinkEndChild(rom_node);
            }
            dev_node->LinkEndChild(hostdev_node);
        }
    }

    // disk system
    tinyxml2::XMLElement* image_node = doc.NewElement("disk");
    image_node->SetAttribute("type", "file");
    image_node->SetAttribute("device", "disk");

    tinyxml2::XMLElement* driver_node = doc.NewElement("driver");
    driver_node->SetAttribute("name", "qemu");
    driver_node->SetAttribute("type", "qcow2");
    image_node->LinkEndChild(driver_node);

    tinyxml2::XMLElement* source_node = doc.NewElement("source");
    source_node->SetAttribute("file", disk_system.c_str());
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

    // disk data
    for (int i = 0; i < disk_data.size(); i++) {
        tinyxml2::XMLElement* disk_data_node = doc.NewElement("disk");
        disk_data_node->SetAttribute("type", "file");
        disk_data_node->SetAttribute("device", "disk");

        tinyxml2::XMLElement* disk_driver_node = doc.NewElement("driver");
        disk_driver_node->SetAttribute("name", "qemu");
        disk_driver_node->SetAttribute("type", "qcow2");
        disk_data_node->LinkEndChild(disk_driver_node);

        tinyxml2::XMLElement* disk_source_node = doc.NewElement("source");
        disk_source_node->SetAttribute("file", disk_data[i].c_str());
        disk_data_node->LinkEndChild(disk_source_node);

        tinyxml2::XMLElement* disk_target_node = doc.NewElement("target");
		char buf[10] = { 0 };
		snprintf(buf, 10, "%c", 'b' + i);
        disk_target_node->SetAttribute("dev", std::string("vd").append(buf).c_str());
        disk_target_node->SetAttribute("bus", "virtio");
        disk_data_node->LinkEndChild(disk_target_node);

        dev_node->LinkEndChild(disk_data_node);
    }

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

    // bridge
    if (!bridge_name.empty()) {
        tinyxml2::XMLElement* bridge_node = doc.NewElement("interface");
        bridge_node->SetAttribute("type", "bridge");
        tinyxml2::XMLElement* bridge_source_node = doc.NewElement("source");
        bridge_source_node->SetAttribute("bridge", bridge_name.c_str());
        bridge_node->LinkEndChild(bridge_source_node);
        tinyxml2::XMLElement* bridge_model_node = doc.NewElement("model");
        bridge_model_node->SetAttribute("type", "virtio");
        bridge_node->LinkEndChild(bridge_model_node);
        tinyxml2::XMLElement* bridge_mtu_node = doc.NewElement("mtu");
        bridge_mtu_node->SetAttribute("size", "1450");
        bridge_node->LinkEndChild(bridge_mtu_node);
        dev_node->LinkEndChild(bridge_node);
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

    tinyxml2::XMLElement* memballoon_node = doc.NewElement("memballoon");
    memballoon_node->SetAttribute("model", "virtio");
    tinyxml2::XMLElement* memballoon_period_node = doc.NewElement("stats");
    memballoon_period_node->SetAttribute("period", "10");
    memballoon_node->LinkEndChild(memballoon_period_node);
    dev_node->LinkEndChild(memballoon_node);

    root->LinkEndChild(dev_node);

    // cpu (qemu:commandline)
    tinyxml2::XMLElement* node_qemu_commandline = doc.NewElement("qemu:commandline");
    if (!is_windows) {
        tinyxml2::XMLElement* node_qemu_arg1 = doc.NewElement("qemu:arg");
        node_qemu_arg1->SetAttribute("value", "-cpu");
        node_qemu_commandline->LinkEndChild(node_qemu_arg1);
        tinyxml2::XMLElement* node_qemu_arg2 = doc.NewElement("qemu:arg");
        node_qemu_arg2->SetAttribute("value", "host,kvm=off,hv_vendor_id=null");
        node_qemu_commandline->LinkEndChild(node_qemu_arg2);
    }
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

VmClient::VmClient() {

}

VmClient::~VmClient() {
    if (m_connPtr != nullptr) {
        virConnectClose(m_connPtr);
    }
}

bool VmClient::init() {
    if (m_connPtr == nullptr) {
        m_connPtr = virConnectOpen(qemu_tcp_url.c_str());
    }

    return m_connPtr != nullptr;
}

void VmClient::exit() {
    if (m_connPtr != nullptr) {
        virConnectClose(m_connPtr);
    }
}

int32_t VmClient::CreateDomain(const std::shared_ptr<dbc::TaskInfo>& taskinfo,
                               const std::shared_ptr<TaskResource>& task_resource) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(taskinfo->image_name, "connPtr is nullptr");
        return ERR_ERROR;
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
    TASK_LOG_INFO(taskinfo->task_id, "create domain with vga_pci: " << vga_pci << ", cpu: " << cpuNumTotal
        << ", mem: " << memoryTotal << "KB, uuid: " << buf_uuid);

    // 系统盘: 创建增量镜像
    std::string from_image_file = "/data/" + taskinfo->image_name;
    std::string image_fname = util::GetFileNameWithoutExt(from_image_file);
    std::string image_fext = util::GetFileExt(from_image_file);
    std::string disk_system = "/data/";
    disk_system += "vm_" + std::to_string(rand() % 100000) + "_" + util::time2str(time(nullptr)) + "_"
                  + taskinfo->custom_image_name + ".qcow2";

    std::string cmd_back_system_image = "qemu-img create -f qcow2 -F qcow2 -b " + from_image_file + " " + disk_system;
    std::string create_system_image_ret = run_shell(cmd_back_system_image);
    TASK_LOG_INFO(taskinfo->task_id, "create vm, cmd: " << cmd_back_system_image << ", result: " << create_system_image_ret);

    // 数据盘：
    std::string data_file = "/data/data_" + std::to_string(rand() % 100000) + "_" +
            util::time2str(time(nullptr)) + "_" + taskinfo->custom_image_name + ".qcow2";
    uint64_t data_size = task_resource->disks.begin()->second / 1024L / 1024L; // GB
    TASK_LOG_INFO(taskinfo->task_id, "data_file: " << data_file << ", data_size:" << data_size << "G");
    if (taskinfo->data_file_name.empty()) {
        std::string cmd_create_img = "qemu-img create -f qcow2 " + data_file + " " + std::to_string(data_size) + "G";
        std::string create_ret = run_shell(cmd_create_img);
        TASK_LOG_INFO(taskinfo->task_id, "create data: " << cmd_create_img << ", result: " << create_ret);
    } else {
        boost::filesystem::copy_file("/data/" + taskinfo->data_file_name, data_file);
        TASK_LOG_INFO(taskinfo->task_id, "copy data: " << "/data/" << taskinfo->data_file_name
            << " to: " << data_file);
    }
    std::vector<std::string> disk_data;
    disk_data.push_back(data_file);

    if (!taskinfo->multicast.empty()) {
        for (const auto & mcast : taskinfo->multicast) {
            TASK_LOG_INFO(taskinfo->task_id, "add multicast address: " << mcast);
        }
    }

    std::string bridge_name;
    if (!taskinfo->network_name.empty()) {
        std::shared_ptr<dbc::networkInfo> networkInfo = VxlanManager::instance().GetNetwork(taskinfo->network_name);
        if (networkInfo) bridge_name = networkInfo->bridgeName;
    }

    // vnc
    TASK_LOG_INFO(taskinfo->task_id, "vnc port: " << task_resource->vnc_port << ", password: " << task_resource->vnc_password);

    std::string xml_content = createXmlStr(buf_uuid, taskinfo->task_id, memoryTotal,
                                           cpuNumTotal, task_resource->cpu_sockets,
                                           task_resource->cpu_cores, task_resource->cpu_threads,
                                           vga_pci, disk_system, disk_data,
                                           task_resource->vnc_port, task_resource->vnc_password,
                                           taskinfo->multicast, bridge_name,
                                           taskinfo->operation_system.find("win") != std::string::npos,
                                           taskinfo->bios_mode == "uefi");

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;
    do {
        domainPtr = virDomainDefineXML(m_connPtr, xml_content.c_str());
        if (nullptr == domainPtr) {
            errorNum = E_VIRT_DOMAIN_EXIST;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(taskinfo->task_id, "virDomainDefineXML error: " << (error ? error->message : ""));
            break;
        }

        if (virDomainCreate(domainPtr) < 0) {
            errorNum = E_VIRT_INTERNAL_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(taskinfo->task_id, "virDomainCreate error: " << (error ? error->message : ""));
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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_NOSTATE || info.state == VIR_DOMAIN_SHUTOFF) {
            if (virDomainCreate(domainPtr) < 0) {
                errorNum = ERR_ERROR;

                virErrorPtr error = virGetLastError();
                TASK_LOG_ERROR(domain_name, "virDomainCreate error: " << (error ? error->message : ""));
            }
        } else if (info.state == VIR_DOMAIN_PMSUSPENDED) {
            if (virDomainPMWakeup(domainPtr, 0) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainSuspend(domainPtr) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_PMSUSPENDED) {
            if (virDomainResume(domainPtr) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainReboot(domainPtr, VIR_DOMAIN_REBOOT_DEFAULT) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainShutdown(domainPtr) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state != VIR_DOMAIN_SHUTOFF) {
            if (virDomainDestroy(domainPtr) < 0) {
                errorNum = ERR_ERROR;

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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        if (virDomainUndefine(domainPtr) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainUndefine error: " << (error ? error->message : ""));
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    return errorNum;
}

FResult VmClient::RedefineDomain(const std::shared_ptr<dbc::TaskInfo>& taskinfo, 
                                 const std::shared_ptr<TaskResource>& task_resource, bool increase_disk) {
	if (m_connPtr == nullptr) {
		return FResult(ERR_ERROR, "libvirt disconnect");
	}

	virDomainPtr domainPtr = virDomainLookupByName(m_connPtr, taskinfo->task_id.c_str());
	if (nullptr == domainPtr) {
        return FResult(ERR_ERROR, "task:" + taskinfo->task_id + " not exist");
	}

	// new gpu
	std::map<std::string, std::list<std::string>> mpGpu = task_resource->gpus;
	std::string vga_pci;
	for (auto& it : mpGpu) {
		for (auto& it2 : it.second) {
			vga_pci += it2 + "|";
		}
	}

	// new cpu
	int32_t cpuNumTotal = task_resource->total_cores();
    int32_t cpuSockets = task_resource->cpu_sockets;
    int32_t cpuCores = task_resource->cpu_cores;
    int32_t cpuThreads = task_resource->cpu_threads;
 
	// new mem
	uint64_t memoryTotal = task_resource->mem_size; // KB
 
	// new disk
	std::string data_file = "/data/data_" + std::to_string(rand() % 100000) + "_" +
		util::time2str(time(nullptr)) + "_" + taskinfo->custom_image_name + ".qcow2";
    uint64_t data_size = task_resource->disks[task_resource->disks.size() - 1] / 1024L / 1024L; // GB
    if (increase_disk) {
        std::string cmd_create_img = "qemu-img create -f qcow2 " + data_file + " " + std::to_string(data_size) + "G";
        run_shell(cmd_create_img);
    }

	char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE);
    if (pContent != nullptr) {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            virDomainFree(domainPtr);
            return FResult(ERR_ERROR, "task:" + taskinfo->task_id + " parse domain xml failed");
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        // memory
        tinyxml2::XMLElement* ele_memory = root->FirstChildElement("memory");
        if (ele_memory != nullptr) {
            ele_memory->SetText(memoryTotal);
        }
		tinyxml2::XMLElement* ele_current_memory = root->FirstChildElement("currentMemory");
		if (ele_current_memory != nullptr) {
            ele_current_memory->SetText(memoryTotal);
		}
        // cpu
		tinyxml2::XMLElement* ele_vcpu = root->FirstChildElement("vcpu");
		if (ele_vcpu != nullptr) {
            ele_vcpu->SetText(cpuNumTotal);
		}
		tinyxml2::XMLElement* ele_cpu = root->FirstChildElement("cpu");
		if (ele_cpu != nullptr) {
            tinyxml2::XMLElement* ele_topology = ele_cpu->FirstChildElement("topology");
            ele_topology->SetAttribute("sockets", cpuSockets);
            ele_topology->SetAttribute("cores", cpuCores);
            ele_topology->SetAttribute("threads", cpuThreads);
		}
        tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
        // disk
        if (increase_disk) {
            if (ele_devices != nullptr) {
                tinyxml2::XMLElement* ele_disk = ele_devices->LastChildElement("disk");
                tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
                std::string strDev = ele_target->Attribute("dev");
                char idx = strDev.back() + 1;

                tinyxml2::XMLElement* disk_data_node = doc.NewElement("disk");
                disk_data_node->SetAttribute("type", "file");
                disk_data_node->SetAttribute("device", "disk");
                tinyxml2::XMLElement* disk_driver_node = doc.NewElement("driver");
                disk_driver_node->SetAttribute("name", "qemu");
                disk_driver_node->SetAttribute("type", "qcow2");
                disk_data_node->LinkEndChild(disk_driver_node);
                tinyxml2::XMLElement* disk_source_node = doc.NewElement("source");
                disk_source_node->SetAttribute("file", data_file.c_str());
                disk_data_node->LinkEndChild(disk_source_node);
                tinyxml2::XMLElement* disk_target_node = doc.NewElement("target");
                char buf[10] = { 0 };
                snprintf(buf, 10, "%c", idx);
                disk_target_node->SetAttribute("dev", std::string("vd").append(buf).c_str());
                disk_target_node->SetAttribute("bus", "virtio");
                disk_data_node->LinkEndChild(disk_target_node);

                ele_devices->InsertAfterChild(ele_disk, disk_data_node);
            }
        }
        // gpu
        if (ele_devices != nullptr) {
            tinyxml2::XMLElement* ele_hostdev = ele_devices->FirstChildElement("hostdev");
            while (ele_hostdev != nullptr) {
                ele_devices->DeleteChild(ele_hostdev);
                ele_hostdev = ele_devices->FirstChildElement("hostdev");
            }

			if (!vga_pci.empty()) {
				std::vector<std::string> vedios = util::split(vga_pci, "|");
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

					tinyxml2::XMLElement* hostdev_node = doc.NewElement("hostdev");
					hostdev_node->SetAttribute("mode", "subsystem");
					hostdev_node->SetAttribute("type", "pci");
					hostdev_node->SetAttribute("managed", "yes");
					tinyxml2::XMLElement* source_node = doc.NewElement("source");
					tinyxml2::XMLElement* address_node = doc.NewElement("address");
					address_node->SetAttribute("type", "pci");
					address_node->SetAttribute("domain", "0x0000");
					address_node->SetAttribute("bus", ("0x" + infos[0]).c_str());
					address_node->SetAttribute("slot", ("0x" + infos2[0]).c_str());
					address_node->SetAttribute("function", ("0x" + infos2[1]).c_str());
					source_node->LinkEndChild(address_node);
					hostdev_node->LinkEndChild(source_node);

                    ele_devices->LinkEndChild(hostdev_node);
				}
			}
        }

		tinyxml2::XMLPrinter printer;
		doc.Print(&printer);
		const char* xml_content = printer.CStr();
         
		domainPtr = virDomainDefineXML(m_connPtr, xml_content);
		if (domainPtr == nullptr) {
			virErrorPtr error = virGetLastError();
			std::string errmsg = std::string("defineXML failed: ") + error->message;
			virDomainFree(domainPtr);
			return FResult(ERR_ERROR, errmsg);
		}
    }

	virDomainFree(domainPtr);
	return FResultOk;
}

int32_t VmClient::DestroyAndUndefineDomain(const std::string &domain_name, unsigned int undefineFlags) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state != VIR_DOMAIN_SHUTOFF) {
            if (virDomainDestroy(domainPtr) < 0) {
                errorNum = ERR_ERROR;

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
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainUndefine error: " << (error ? error->message : ""));
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
        return ERR_ERROR;
    }

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;

    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
            errorNum = E_VIRT_DOMAIN_NOT_FOUND;
            break;
        }

        virDomainInfo info;
        if (virDomainGetInfo(domainPtr, &info) < 0) {
            errorNum = ERR_ERROR;

            virErrorPtr error = virGetLastError();
            TASK_LOG_ERROR(domain_name, "virDomainGetInfo error: " << (error ? error->message : ""));
            break;
        }

        if (info.state == VIR_DOMAIN_RUNNING) {
            if (virDomainReset(domainPtr, VIR_DOMAIN_REBOOT_DEFAULT) < 0) {
                errorNum = ERR_ERROR;

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
    virDomainPtr* pdomains = nullptr;
    int count = virConnectListAllDomains(m_connPtr, &pdomains, 0);
    if (count < 0) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        virDomainPtr dom = pdomains[i];
        const char* dom_name = virDomainGetName(dom);
        domains.push_back(dom_name);
        virDomainFree(dom);
    }
}

void VmClient::ListAllRunningDomains(std::vector<std::string> &domains) {
    if (m_connPtr == nullptr) {
        LOG_INFO << "connPtr is nullptr";
        return;
    }

    int ids[1024] = {0};
    int num = virConnectListDomains(m_connPtr, ids, 1024);
    if (num < 0) {
        LOG_INFO << "list domains failed";
        return;
    }

    for (int i = 0; i < num; i++) {
        virDomainPtr domainPtr = nullptr;
        do {
            domainPtr = virDomainLookupByID(m_connPtr, ids[i]);
            if (nullptr == domainPtr) {
                LOG_INFO << "lookup domain_id:" << ids[i] << " is nullptr";
                break;
            }
            
            virDomainInfo info;
            if (virDomainGetInfo(domainPtr, &info) < 0) {
                LOG_INFO << "get domain_info failed";
                break;
            }

            virDomainState vm_status = (virDomainState) info.state;
            if (vm_status == VIR_DOMAIN_RUNNING) {
                std::string domain_name = virDomainGetName(domainPtr);
                domains.push_back(domain_name);
            }
        } while (0);

        if (nullptr != domainPtr) {
            virDomainFree(domainPtr);
        }
    }
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

FResult VmClient::GetDomainLog(const std::string &domain_name, QUERY_LOG_DIRECTION direction, int32_t linecount, std::string &log_content) {
    LOG_INFO << "get domain " << domain_name << " log, direction: " << (int32_t) direction << ", line: " << linecount;
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
        return FResult(ERR_ERROR, std::string("log file error: ").append(e.what()));
    }
    catch (const boost::exception & e) {
        return FResult(ERR_ERROR, "log file error: " + diagnostic_information(e));
    }
    catch (...) {
        return FResult(ERR_ERROR, "unknowned log file error");
    }
    
    if (latest_log.empty() || max_num < 0) {
        return FResult(ERR_ERROR, "task log not exist");
    }

    log_content.clear();
    std::ifstream file(latest_log, std::ios_base::in);
    if (file.is_open()) {
        if (direction == QUERY_LOG_DIRECTION::Head) {
            file.seekg(0, std::ios_base::beg);
        } else {
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
        return {ERR_SUCCESS, ""};
    }
    
    return FResult(ERR_ERROR, "open log file error");
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
                    LOG_ERROR << "retry destroy domain " << domain_name << " error: " << (error ? error->message : "");
                    break;
                }
                if (virDomainCreate(domainPtr) < 0) {
                    virErrorPtr error = virGetLastError();
                    LOG_ERROR << "retry create domain " << domain_name << " error: " << (error ? error->message : "");
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

int32_t VmClient::GetDomainInterfaceAddress(const std::string& domain_name, std::vector<dbc::virDomainInterface> &difaces, unsigned int source) {
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

        if (!virDomainIsActive(domainPtr)) {
            break;
        }

        ifaces_count = virDomainInterfaceAddresses(domainPtr, &ifaces, source, 0);
        for (int i = 0; i < ifaces_count; i++) {
            dbc::virDomainInterface diface;
            diface.name = ifaces[i]->name;
            if (ifaces[i]->hwaddr)
                diface.hwaddr = ifaces[i]->hwaddr;

            for (int j = 0; j < ifaces[i]->naddrs; j++) {
                virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
                // printf("[addr: %s prefix: %d type: %d]", ip_addr->addr, ip_addr->prefix, ip_addr->type);
                diface.addrs.push_back(dbc::virDomainIPAddress{ip_addr->type, ip_addr->addr, ip_addr->prefix});
            }
            difaces.push_back(diface);
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
    int32_t ret_code = ERR_ERROR;
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
            break;
        }
        ret_code = ERR_SUCCESS;
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

std::string VmClient::QemuAgentCommand(const std::string& domain_name, const std::string& cmd, int timeout, unsigned int flags) {
    std::string ret;
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return ret;
    }

    virDomainPtr domain_ptr = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            break;
        }
        char *result = virDomainQemuAgentCommand(domain_ptr, cmd.c_str(), timeout, flags);
        if (result) {
            ret = result;
            free(result);
        }
    } while (0);

    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }
    return ret;
}

bool VmClient::GetDomainMonitorData(const std::string& domain_name, dbcMonitor::domMonitorData& data) {
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return false;
    }
    bool bRet = false;

    std::map<std::string, domainDiskInfo> disks;
    ListDomainDiskInfo(domain_name, disks);
    std::vector<dbc::virDomainInterface> difaces;
    GetDomainInterfaceAddress(domain_name, difaces, VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_LEASE);

    virDomainPtr domain_ptr = nullptr;
    do {
        domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
            break;
        }

        // domain basic info
        virDomainInfo info;
        if (virDomainGetInfo(domain_ptr, &info) < 0) {
            virErrorPtr error = virGetLastError();
            LOG_ERROR << domain_name << " virDomainGetInfo error: " << (error ? error->message : "");
        } else {
            clock_gettime(CLOCK_REALTIME, &data.domInfo.realTime);
            data.domInfo.state = vm_status_string(static_cast<virDomainState>(info.state));
            data.domInfo.maxMem = info.maxMem;
            data.domInfo.memory = info.memory;
            data.domInfo.nrVirtCpu = info.nrVirtCpu;
            data.domInfo.cpuTime = info.cpuTime;
            data.domInfo.cpuUsage = 0.0f;
        }

        bool isActive = virDomainIsActive(domain_ptr);

        // memory info
        clock_gettime(CLOCK_REALTIME, &data.memStats.realTime);
        virDomainMemoryStatStruct stats[20] = {0};
        if (!isActive || virDomainMemoryStats(domain_ptr, stats, 20, 0) < 0) {
            virErrorPtr error = virGetLastError();
            LOG_ERROR << domain_name << " virDomainMemoryStats error: " << (error ? error->message : "");
            data.memStats.total = info.maxMem;
            data.memStats.unused = info.maxMem;
            data.memStats.available = info.memory;
            data.memStats.usage = 0.0f;
        } else {
            std::map<int, unsigned long long> mapstats;
            for (int i = 0; i < 20; ++i) {
                if (mapstats.find(stats[i].tag) == mapstats.end())
                    mapstats.insert(std::make_pair(stats[i].tag, stats[i].val));
            }
            data.memStats.total = mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON];
            data.memStats.unused = mapstats[VIR_DOMAIN_MEMORY_STAT_UNUSED];
            data.memStats.available = mapstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE];
            data.memStats.usage = 0.0f;
        }

        // disk info
        for (const auto& disk : disks) {
            struct dbcMonitor::diskInfo dsInfo;
            dsInfo.name = disk.first;
            virDomainBlockInfo info;
            if (virDomainGetBlockInfo(domain_ptr, disk.first.c_str(), &info, 0) < 0) {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << domain_name << " virDomainGetBlockInfo error: " << (error ? error->message : "");
            } else {
                dsInfo.capacity = info.capacity;
                dsInfo.allocation = info.allocation;
                dsInfo.physical = info.physical;
            }
            virDomainBlockStatsStruct stats;
            if (!isActive || virDomainBlockStats(domain_ptr, disk.first.c_str(), &stats, sizeof(stats)) < 0) {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << domain_name << " virDomainBlockStats error: " << (error ? error->message : "");
                dsInfo.rd_req = dsInfo.rd_bytes = dsInfo.wr_req = dsInfo.wr_bytes = dsInfo.errs = 0;
            } else {
                dsInfo.rd_req = stats.rd_req;
                dsInfo.rd_bytes = stats.rd_bytes;
                dsInfo.wr_req = stats.wr_req;
                dsInfo.wr_bytes = stats.wr_bytes;
                dsInfo.errs = stats.errs;
            }
            clock_gettime(CLOCK_REALTIME, &dsInfo.realTime);
            dsInfo.rd_speed = 0.0f;
            dsInfo.wr_speed = 0.0f;
            data.diskStats[dsInfo.name] = dsInfo;
        }

        // network info
        for (const auto& diface: difaces) {
            dbcMonitor::networkInfo netInfo;
            netInfo.name = diface.name;
            virDomainInterfaceStatsStruct stats;
            if (virDomainInterfaceStats(domain_ptr, diface.name.c_str(), &stats, sizeof(stats)) < 0) {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << domain_name << " virDomainInterfaceStats error: " << (error ? error->message : "");
            } else {
                clock_gettime(CLOCK_REALTIME, &netInfo.realTime);
                netInfo.rx_bytes = stats.rx_bytes;
                netInfo.rx_packets = stats.rx_packets;
                netInfo.rx_errs = stats.rx_errs;
                netInfo.rx_drop = stats.rx_drop;
                netInfo.tx_bytes = stats.tx_bytes;
                netInfo.tx_packets = stats.tx_packets;
                netInfo.tx_errs = stats.tx_errs;
                netInfo.tx_drop = stats.tx_drop;
                netInfo.rx_speed = 0.0f;
                netInfo.tx_speed = 0.0f;
            }
            data.netStats[netInfo.name] = netInfo;
        }

        // gpu info
        if (isActive) {
            char *result = virDomainQemuAgentCommand(domain_ptr, "{\"execute\":\"guest-get-gpus\"}", VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
            if (result) {
                rapidjson::Document doc;
                doc.Parse(result);
                if (!doc.IsObject()) {
                    LOG_ERROR << domain_name << " parse guest agent result data error";
                } else {
                    if (doc.HasMember("return") && doc["return"].IsObject()) {
                        const rapidjson::Value& obj_ret = doc["return"];
                        if (obj_ret.HasMember("driver-version") && obj_ret["driver-version"].IsString()) {
                            data.graphicsDriverVersion = obj_ret["driver-version"].GetString();
                        }
                        if (obj_ret.HasMember("cuda-version") && obj_ret["cuda-version"].IsString()) {
                            data.cudaVersion = obj_ret["cuda-version"].GetString();
                        }
                        if (obj_ret.HasMember("nvml-version") && obj_ret["nvml-version"].IsString()) {
                            data.nvmlVersion = obj_ret["nvml-version"].GetString();
                        }
                        if (obj_ret.HasMember("gpus") && obj_ret["gpus"].IsArray()) {
                            const rapidjson::Value& obj_gpus = obj_ret["gpus"];
                            for (size_t i = 0; i < obj_gpus.Size(); i++) {
                                const rapidjson::Value& obj_gpu = obj_gpus[i];
                                if (!obj_gpu.IsObject()) continue;
                                dbcMonitor::gpuInfo gpuInfo;
                                clock_gettime(CLOCK_REALTIME, &gpuInfo.realTime);
                                gpuInfo.memTotal = gpuInfo.memFree = gpuInfo.memUsed = 0;
                                gpuInfo.gpuUtilization = gpuInfo.memUtilization = 0;
                                gpuInfo.powerUsage = gpuInfo.powerCap = gpuInfo.temperature = 0;
                                if (obj_gpu.HasMember("name") && obj_gpu["name"].IsString())
                                    gpuInfo.name = obj_gpu["name"].GetString();
                                if (obj_gpu.HasMember("bus-id") && obj_gpu["bus-id"].IsString())
                                    gpuInfo.busId = obj_gpu["bus-id"].GetString();
                                if (obj_gpu.HasMember("mem-total") && obj_gpu["mem-total"].IsUint64())
                                    gpuInfo.memTotal = obj_gpu["mem-total"].GetUint64();
                                if (obj_gpu.HasMember("mem-free") && obj_gpu["mem-free"].IsUint64())
                                    gpuInfo.memFree = obj_gpu["mem-free"].GetUint64();
                                if (obj_gpu.HasMember("mem-used") && obj_gpu["mem-used"].IsUint64())
                                    gpuInfo.memUsed = obj_gpu["mem-used"].GetUint64();
                                if (obj_gpu.HasMember("gpu-utilization") && obj_gpu["gpu-utilization"].IsUint())
                                    gpuInfo.gpuUtilization = obj_gpu["gpu-utilization"].GetUint();
                                if (obj_gpu.HasMember("mem-utilization") && obj_gpu["mem-utilization"].IsUint())
                                    gpuInfo.memUtilization = obj_gpu["mem-utilization"].GetUint();
                                if (obj_gpu.HasMember("pwr-usage") && obj_gpu["pwr-usage"].IsUint())
                                    gpuInfo.powerUsage = obj_gpu["pwr-usage"].GetUint();
                                if (obj_gpu.HasMember("pwr-cap") && obj_gpu["pwr-cap"].IsUint())
                                    gpuInfo.powerCap = obj_gpu["pwr-cap"].GetUint();
                                if (obj_gpu.HasMember("temperature") && obj_gpu["temperature"].IsUint())
                                    gpuInfo.temperature = obj_gpu["temperature"].GetUint();
                                data.gpuStats[gpuInfo.busId] = gpuInfo;
                            }
                        }
                    } else {
                        LOG_ERROR << domain_name << " guest agent result has no return node";
                    }
                }
                free(result);
            } else {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << domain_name << " virDomainQemuAgentCommand error: " << (error ? error->message : "");
            }
        }
        bRet = true;
    } while (0);

    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }
    return bRet;
}
