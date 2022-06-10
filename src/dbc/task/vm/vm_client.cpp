#include "vm_client.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "server/server.h"
#include <boost/property_tree/json_parser.hpp>
#include "tinyxml2.h"
#include <uuid/uuid.h>
#include "message/message_id.h"
#include "message/vm_task_result_types.h"
#include "util/utils.h"
#include <libvirt/libvirt-qemu.h>
#include "task/detail/VxlanManager.h"
#include "task/detail/disk/TaskDiskManager.h"
#include "task/detail/gpu/TaskGpuManager.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <stdlib.h>
#include <linux/ethtool.h>

static const std::string qemu_tcp_url = "qemu+tcp://localhost:16509/system";
static const char* arrayConnectCloseReason[] = {"VIR_CONNECT_CLOSE_REASON_ERROR", "VIR_CONNECT_CLOSE_REASON_EOF",
  "VIR_CONNECT_CLOSE_REASON_KEEPALIVE", "VIR_CONNECT_CLOSE_REASON_CLIENT", "VIR_CONNECT_CLOSE_REASON_LAST"};

#define ETHTOOL_GDRVINFO 0x00000003
#define SIOCETHTOOL 0x8946

struct virDomainDeleter {
    inline void operator()(virDomainPtr domain) {
        virDomainFree(domain);
    }
};

// libvirt connect close callback
void connect_close_cb(virConnectPtr conn, int reason, void *opaque) {
    std::string reason_msg = "unknowned reason";
    if (reason >= 0 && reason <= virConnectCloseReason::VIR_CONNECT_CLOSE_REASON_CLIENT) {
        reason_msg = arrayConnectCloseReason[reason];
    }
    LOG_INFO << "connect close cb called, reason: " << reason << ", detail: " << reason_msg;
    VmClient* vm_client = (VmClient*)opaque;
    vm_client->ConnectCloseCb();
}

static int send_ioctl(int fd, struct ifreq* ifr, void* cmd)
{
	ifr->ifr_data = (char*)cmd;
	return ioctl(fd, SIOCETHTOOL, ifr);
}

static std::string do_gbus_info(const char* name)
{
	struct ifreq ifr;
	int err;
	struct ethtool_drvinfo drvinfo;
	int fd;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		//printf("create socket failed\n");
		return "";
	}

	strcpy(ifr.ifr_name, name);
	drvinfo.cmd = ETHTOOL_GDRVINFO;
	err = send_ioctl(fd, &ifr, &drvinfo);
	if (err < 0) {
		//printf("%s cannot get driver information\n", name);
		return "";
	}

	return std::string(drvinfo.bus_info);
}

struct InterfaceItem {
    bool default_route = false;
	std::string name;
	std::string bus;
	std::vector<std::string> ipv4;
	std::vector<std::string> ipv6;
};

void list_interface(std::map<std::string, InterfaceItem>& mpifa)
{
	struct ifaddrs* ifap, * ifa;
	struct sockaddr_in6* sa;
	struct sockaddr_in* sa4;
	char addr6[INET6_ADDRSTRLEN];
	char addr4[INET_ADDRSTRLEN];

	if (0 != getifaddrs(&ifap)) return;

	std::string nic_name = run_shell("ip route|awk 'NR==1{print $5}'");
	nic_name = util::rtrim(nic_name, '\n');

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		std::string name = ifa->ifa_name;
        if (name == nic_name) continue;

        std::string bus = do_gbus_info(name.c_str());
        if (bus.empty() || bus == "N/A") continue;
        if (!((bus.size() == 12) && (bus[4] == ':') && (bus[7] == ':') && (bus[10] == '.'))) continue;
        
		mpifa[name].name = name;
        mpifa[name].bus = bus;

		if (ifa->ifa_addr->sa_family == AF_INET6) {
			sa = (struct sockaddr_in6*)ifa->ifa_addr;
			getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), addr6,
				sizeof(addr6), NULL, 0, NI_NUMERICHOST);

			mpifa[name].ipv6.push_back(addr6);
		}
		else if (ifa->ifa_addr->sa_family == AF_INET) {
			sa4 = (struct sockaddr_in*)ifa->ifa_addr;
			getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), addr4,
				sizeof(addr4), NULL, 0, NI_NUMERICHOST);
			/*inet_ntop(AF_INET, sa4, addr4, INET_ADDRSTRLEN);*/

			mpifa[name].ipv4.push_back(addr4);
		}
	}

	freeifaddrs(ifap);
}

static std::string createXmlStr(const std::string& uuid, const std::string& domain_name,
    int64_t memory, int32_t cpunum, int32_t sockets, int32_t cores, int32_t threads,
    const std::string& vedio_pci, const std::string& disk_system,
    const std::string& disk_data, int32_t vnc_port, const std::string& vnc_pwd,
    const std::vector<std::string>& multicast, const std::string& bridge_name,
    const std::string& bios_mode, bool is_windows = false)
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

    if (bios_mode == "pxe") {
        tinyxml2::XMLElement* os_sub_node2 = doc.NewElement("boot");
        os_sub_node2->SetAttribute("dev", "network");
        os_node->LinkEndChild(os_sub_node2);
    }
    else {
        if (!is_windows) {
            tinyxml2::XMLElement* os_sub_node2 = doc.NewElement("boot");
            os_sub_node2->SetAttribute("dev", "hd");
            os_node->LinkEndChild(os_sub_node2);

            tinyxml2::XMLElement* os_sub_node3 = doc.NewElement("boot");
            os_sub_node3->SetAttribute("dev", "cdrom");
            os_node->LinkEndChild(os_sub_node3);
        }
        else {
            if (bios_mode == "uefi") { // uefi引导设置
                tinyxml2::XMLElement* os_loader = doc.NewElement("loader");
                os_loader->SetAttribute("readonly", "yes");
                // 如果加载程序路径指向 UEFI 映像，则类型应为pflash。
                os_loader->SetAttribute("type", "pflash");
                // os_loader->SetAttribute("type", "rom");
                os_loader->SetText("/usr/share/OVMF/OVMF_CODE.fd");
                os_node->LinkEndChild(os_loader);
            }

            tinyxml2::XMLElement* os_bootmenu_node = doc.NewElement("bootmenu");
            os_bootmenu_node->SetAttribute("enable", "yes");
            os_node->LinkEndChild(os_bootmenu_node);
        }
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
    }
    else {
        clock_node->SetAttribute("offset", "localtime");
        tinyxml2::XMLElement* clock_rtc_node = doc.NewElement("timer");
        clock_rtc_node->SetAttribute("name", "rtc");
        clock_rtc_node->SetAttribute("tickpolicy", "catchup");
        clock_node->LinkEndChild(clock_rtc_node);
        tinyxml2::XMLElement* clock_pit_node = doc.NewElement("timer");
        clock_pit_node->SetAttribute("name", "pit");
        clock_pit_node->SetAttribute("tickpolicy", "delay");
        clock_node->LinkEndChild(clock_pit_node);
        tinyxml2::XMLElement* clock_hpet_node = doc.NewElement("timer");
        clock_hpet_node->SetAttribute("name", "hpet");
        clock_hpet_node->SetAttribute("present", "no");
        clock_node->LinkEndChild(clock_hpet_node);
        tinyxml2::XMLElement* clock_hypervclock_node = doc.NewElement("timer");
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
        if (bios_mode == "pxe") {
			tinyxml2::XMLElement* vedio_node = doc.NewElement("video");
			tinyxml2::XMLElement* model_node = doc.NewElement("model");
			model_node->SetAttribute("type", "none");
			vedio_node->LinkEndChild(model_node);
			dev_node->LinkEndChild(vedio_node);
        }
        else {
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
    }

    if (vedio_pci != "") {
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
            if (is_windows) {
                tinyxml2::XMLElement* rom_node = doc.NewElement("rom");
                rom_node->SetAttribute("bar", "off");
                hostdev_node->LinkEndChild(rom_node);
            }
            dev_node->LinkEndChild(hostdev_node);
        }
    }

    // disk
    if (bios_mode != "pxe") {
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
            tinyxml2::XMLElement* boot_order_node = doc.NewElement("boot");
            boot_order_node->SetAttribute("order", "1");
            image_node->LinkEndChild(boot_order_node);
        }
        dev_node->LinkEndChild(image_node);

        // disk data
        tinyxml2::XMLElement* disk_data_node = doc.NewElement("disk");
        disk_data_node->SetAttribute("type", "file");
        disk_data_node->SetAttribute("device", "disk");

        tinyxml2::XMLElement* disk_driver_node = doc.NewElement("driver");
        disk_driver_node->SetAttribute("name", "qemu");
        disk_driver_node->SetAttribute("type", "qcow2");
        disk_data_node->LinkEndChild(disk_driver_node);

        tinyxml2::XMLElement* disk_source_node = doc.NewElement("source");
        disk_source_node->SetAttribute("file", disk_data.c_str());
        disk_data_node->LinkEndChild(disk_source_node);

        tinyxml2::XMLElement* disk_target_node = doc.NewElement("target");
        disk_target_node->SetAttribute("dev", "vdb");
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
    if (bios_mode == "pxe") {
        std::string pxe_bridge_name;
        std::map<std::string, InterfaceItem> mpifa;
        list_interface(mpifa);
        if (!mpifa.empty()) {
            pxe_bridge_name = mpifa.begin()->second.name;
        }

		tinyxml2::XMLElement* bridge_node = doc.NewElement("interface");
		bridge_node->SetAttribute("type", "direct");
		tinyxml2::XMLElement* bridge_source_node = doc.NewElement("source");
		bridge_source_node->SetAttribute("dev", pxe_bridge_name.c_str());
        bridge_source_node->SetAttribute("mode", "bridge");
		bridge_node->LinkEndChild(bridge_source_node);
		tinyxml2::XMLElement* bridge_model_node = doc.NewElement("model");
		bridge_model_node->SetAttribute("type", "e1000");
		bridge_node->LinkEndChild(bridge_model_node);
		dev_node->LinkEndChild(bridge_node);
    }
    else {
        tinyxml2::XMLElement* interface_node = doc.NewElement("interface");
        interface_node->SetAttribute("type", "network");
        tinyxml2::XMLElement* interface_source_node = doc.NewElement("source");
        interface_source_node->SetAttribute("network", "default");
        interface_node->LinkEndChild(interface_source_node);
        tinyxml2::XMLElement* interface_model_node = doc.NewElement("model");
        interface_model_node->SetAttribute("type", "virtio");
        interface_node->LinkEndChild(interface_model_node);
        tinyxml2::XMLElement* interface_filter_node = doc.NewElement("filterref");
        interface_filter_node->SetAttribute("filter", domain_name.c_str());
        interface_node->LinkEndChild(interface_filter_node);
        dev_node->LinkEndChild(interface_node);

        // multicast
        if (!multicast.empty()) {
            for (const auto& address : multicast) {
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


static std::string createNWFilterXml(const std::string& nwfilter_name, const std::string& uuid, const std::vector<std::string>& nwfilters) {
    tinyxml2::XMLDocument doc;
    // <filter>
    tinyxml2::XMLElement *root = doc.NewElement("filter");
    root->SetAttribute("name", nwfilter_name.c_str());
    root->SetAttribute("chain", "root");
    doc.InsertEndChild(root);

    if (!uuid.empty()) {
        // uuid
        tinyxml2::XMLElement *uuid_node = doc.NewElement("uuid");
        uuid_node->SetText(uuid.c_str());
        root->LinkEndChild(uuid_node);
    }

    // filterref
    tinyxml2::XMLElement *filterref1 = doc.NewElement("filterref");
    filterref1->SetAttribute("filter", "clean-traffic");
    root->LinkEndChild(filterref1);

    // allow dhcp
    tinyxml2::XMLElement *dhcp_out = doc.NewElement("rule");
    dhcp_out->SetAttribute("action", "accept");
    dhcp_out->SetAttribute("direction", "out");
    dhcp_out->SetAttribute("priority", "100");
    tinyxml2::XMLElement *dhcp_udp1 = doc.NewElement("udp");
    dhcp_udp1->SetAttribute("srcipaddr", "0.0.0.0");
    dhcp_udp1->SetAttribute("dstipaddr", "255.255.255.255");
    dhcp_udp1->SetAttribute("srcportstart", "68");
    dhcp_udp1->SetAttribute("dstportstart", "67");
    dhcp_out->LinkEndChild(dhcp_udp1);
    root->LinkEndChild(dhcp_out);

    tinyxml2::XMLElement *dhcp_in = doc.NewElement("rule");
    dhcp_in->SetAttribute("action", "accept");
    dhcp_in->SetAttribute("direction", "in");
    dhcp_in->SetAttribute("priority", "100");
    tinyxml2::XMLElement *dhcp_udp2 = doc.NewElement("udp");
    dhcp_udp2->SetAttribute("srcportstart", "67");
    dhcp_udp2->SetAttribute("dstportstart", "68");
    dhcp_out->LinkEndChild(dhcp_udp2);
    root->LinkEndChild(dhcp_in);

    // rule
    auto rule_func = [&](const std::vector<std::string> v_nwfilter_protocol) -> int {
        //"nwfilter": [
        //    "in,tcp,22,0.0.0.0/0,accept",
        //    "out,all,all,0.0.0.0/0,accept",
        //    "in,all,all,0.0.0.0/0,drop"
        //]
        // std::vector<std::string> v_nwfilter_protocol = util::split(nwfilter_item, ",");
        // if (v_nwfilter_protocol.size() != 5) return -1;
        if (v_nwfilter_protocol[0] != "in" && v_nwfilter_protocol[0] != "out" && v_nwfilter_protocol[0] != "inout") return -1;
        // check ipCidr
        std::vector<std::string> vec_ipCidr = util::split(v_nwfilter_protocol[3], "/");
        if (vec_ipCidr.empty()) return -1;
        std::string ip_addr = vec_ipCidr[0];
        {
            ip_validator ip_vdr;
            variable_value val_ip(ip_addr, false);
            if (!ip_vdr.validate(val_ip)) return -1;
            if (ip_addr == "0.0.0.0") ip_addr.clear();
        }
        // rdp ftp icmp ssh http https 最终还是在xml中配置tcp或者udp端口
        if (v_nwfilter_protocol[4] != "accept" && v_nwfilter_protocol[4] != "drop") return -1;
        tinyxml2::XMLElement *rule = doc.NewElement("rule");
        rule->SetAttribute("action", v_nwfilter_protocol[4].c_str());
        rule->SetAttribute("direction", v_nwfilter_protocol[0].c_str());
        // 优先级可以不写，libvirt默认500
        rule->SetAttribute("priority", "500");
        tinyxml2::XMLElement *protocol_node = nullptr;
        if (v_nwfilter_protocol[1] == "all") {
            protocol_node = doc.NewElement("all");
        } else if (v_nwfilter_protocol[1] == "tcp" || v_nwfilter_protocol[1] == "udp") {
            std::vector<std::string> v_tcp_range = util::split(v_nwfilter_protocol[2], "-");
            if (v_tcp_range.empty()) return -1;
            protocol_node = doc.NewElement(v_nwfilter_protocol[1].c_str());
            protocol_node->SetAttribute("dstportstart", v_tcp_range[0].c_str());
            if (v_tcp_range.size() == 2)
                protocol_node->SetAttribute("dstportend", v_tcp_range[1].c_str());
        } else if (v_nwfilter_protocol[1] == "ssh") {
            protocol_node = doc.NewElement("tcp");
            protocol_node->SetAttribute("dstportstart", "22");
        } else if (v_nwfilter_protocol[1] == "rdp") {
            protocol_node = doc.NewElement("tcp");
            protocol_node->SetAttribute("dstportstart", "3389");
        } else if (v_nwfilter_protocol[1] == "http") {
            protocol_node = doc.NewElement("tcp");
            protocol_node->SetAttribute("dstportstart", "80");
        } else if (v_nwfilter_protocol[1] == "https") {
            protocol_node = doc.NewElement("tcp");
            protocol_node->SetAttribute("dstportstart", "443");
        } else if (v_nwfilter_protocol[1] == "icmp") {
            protocol_node = doc.NewElement("icmp");
        } else if (v_nwfilter_protocol[1] == "ftp") {
            protocol_node = doc.NewElement("ftp");
            protocol_node->SetAttribute("dstportstart", "20");
            protocol_node->SetAttribute("dstportend", "21");
        } else if (v_nwfilter_protocol[1] == "dns(udp)") {
            protocol_node = doc.NewElement("udp");
            protocol_node->SetAttribute("dstportstart", "53");
        } else if (v_nwfilter_protocol[1] == "dns(tcp)") {
            protocol_node = doc.NewElement("tcp");
            protocol_node->SetAttribute("dstportstart", "53");
        }
        if (!ip_addr.empty()) {
            if (v_nwfilter_protocol[0] == "out")
                protocol_node->SetAttribute("dstipaddr", ip_addr.c_str());
            else
                protocol_node->SetAttribute("srcipaddr", ip_addr.c_str());
        }
        if (protocol_node)
            rule->LinkEndChild(protocol_node);
        root->LinkEndChild(rule);
        return 0;
    };

    std::vector<std::vector<std::string>> vec_rule_all;
    for (const auto& nwfilter_item : nwfilters) {
        std::vector<std::string> v_nwfilter_protocol = util::split(nwfilter_item, ",");
        if (v_nwfilter_protocol.size() != 5) continue;
        if (v_nwfilter_protocol[1] == "all" && v_nwfilter_protocol[2] == "all" && v_nwfilter_protocol[3] == "0.0.0.0/0") {
            if (v_nwfilter_protocol[4] == "drop")
                vec_rule_all.push_back(v_nwfilter_protocol);
            else if (v_nwfilter_protocol[4] == "accept")
                vec_rule_all.insert(vec_rule_all.begin(), v_nwfilter_protocol);
        } else {
            rule_func(v_nwfilter_protocol);
        }
    }
    // "out,all,all,0.0.0.0/0,accept" 要写在 "in,all,all,0.0.0.0/0,drop" 前面
    for (const auto& v_nwfilter_protocol : vec_rule_all) {
        rule_func(v_nwfilter_protocol);
    }
    // tinyxml2::XMLElement *rule1 = doc.NewElement("rule");
    // rule1->SetAttribute("action", "accept");
    // rule1->SetAttribute("direction", "in");
    // tinyxml2::XMLElement *tcp1 = doc.NewElement("tcp");
    // tcp1->SetAttribute("dstportstart", "22");
    // rule1->LinkEndChild(tcp1);
    // root->LinkEndChild(rule1);

    // <!-- 允许所有out流量 -->
    // tinyxml2::XMLElement *ruleout = doc.NewElement("rule");
    // ruleout->SetAttribute("action", "accept");
    // ruleout->SetAttribute("direction", "out");
    // tinyxml2::XMLElement *allout = doc.NewElement("all");
    // ruleout->LinkEndChild(allout);
    // root->LinkEndChild(ruleout);

    // <!-- 丢弃所有其他in流量 -->
    // tinyxml2::XMLElement *rulein = doc.NewElement("rule");
    // rulein->SetAttribute("action", "drop");
    // rulein->SetAttribute("direction", "in");
    // tinyxml2::XMLElement *allin = doc.NewElement("all");
    // rulein->LinkEndChild(allin);
    // root->LinkEndChild(rulein);

    // doc.SaveFile((filterName + ".xml").c_str());
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}

VmClient::VmClient() {

}

VmClient::~VmClient() {

}

FResult VmClient::init() {
    if (virEventRegisterDefaultImpl() < 0) {
        return FResult(ERR_ERROR, "register libvirt event impl failed");
    }

    return OpenConnect();
}

void VmClient::exit() {
    CleanConnect();
    StopEventLoop();
    LOG_INFO << "VmClient exited";
}

FResult VmClient::OpenConnect() {
    if (m_connPtr) CleanConnect();
    StopEventLoop();

    m_connPtr = virConnectOpen(qemu_tcp_url.c_str());
    if (m_connPtr == nullptr) {
        return FResult(ERR_ERROR, "connect libvirt tcp service failed");
    }

    if (virConnectRegisterCloseCallback(m_connPtr, connect_close_cb, this, NULL) < 0) {
        return FResult(ERR_ERROR, "register libvirt connect_close_callback failed");
    }

    if (virConnectSetKeepAlive(m_connPtr, 20, 1) < 0) {
        return FResult(ERR_ERROR, "set libvirt keepalive failed");
    }

    StartEventLoop();

    LOG_INFO << "open connect with libvirt successfully";

    return FResultOk;
}

void VmClient::CleanConnect() {
    if (m_connPtr) {
        virConnectUnregisterCloseCallback(m_connPtr, connect_close_cb);
        virConnectClose(m_connPtr);
        m_connPtr = nullptr;
    }
}

bool VmClient::IsConnectAlive() {
    // return !!m_connPtr;
    if (!m_connPtr) return false;
    return virConnectIsAlive(m_connPtr) == 1;
}

void VmClient::ConnectCloseCb() {
    m_event_loop_run = 0;
    if (m_connPtr) {
        // virConnectClose 在某些情况下可能会把 event loop 线程卡死无法结束
        // virConnectClose(m_connPtr);
        m_connPtr = nullptr;
    }
}

int32_t VmClient::CreateDomain(const std::shared_ptr<TaskInfo>& taskinfo) {
    if (m_connPtr == nullptr) {
        TASK_LOG_ERROR(taskinfo->getTaskId(), "connPtr is nullptr");
        return ERR_ERROR;
    }

    std::string domain_name = taskinfo->getTaskId();

    // gpu
    auto gpus = TaskGpuMgr::instance().getTaskGpus(taskinfo->getTaskId());
    std::string vga_pci;
    for (auto& it : gpus) {
        auto ids = it.second->getDeviceIds();
        for (auto& it2 : ids) {
            vga_pci += it2 + "|";
        }
    }
    LOG_INFO << "vga_pci: " << vga_pci;

    // cpu
    long cpuNumTotal = taskinfo->getTotalCores();
    LOG_INFO << "cpu: " << cpuNumTotal;

    // mem
    int64_t memoryTotal = taskinfo->getMemSize(); // KB
    LOG_INFO << "mem: " << memoryTotal << "KB";

    // uuid
    uuid_t uu;
    char buf_uuid[1024] = {0};
    uuid_generate(uu);
    uuid_unparse(uu, buf_uuid);
    TASK_LOG_INFO(domain_name, "create domain with vga_pci: " << vga_pci << ", cpu: " << cpuNumTotal
        << ", mem: " << memoryTotal << "KB, uuid: " << buf_uuid);

    std::map<std::string, std::shared_ptr<DiskInfo>> mpdisks;
    std::string disk_vda, disk_vdb;
    if (taskinfo->getBiosMode() != "pxe") {
        TaskDiskMgr::instance().listDisks(domain_name, mpdisks);
        // 系统盘: 创建增量镜像
        std::string from_image_file = "/data/" + taskinfo->getImageName();
        std::string cmd_back_system_image = "qemu-img create -f qcow2 -F qcow2 -b " + from_image_file + " " + mpdisks["vda"]->getSourceFile();
        std::string create_system_image_ret = run_shell(cmd_back_system_image);
        disk_vda = mpdisks["vda"]->getSourceFile();
        TASK_LOG_INFO(domain_name, "create vm, cmd: " << cmd_back_system_image << ", result: " << create_system_image_ret);
        
        // 数据盘：
        auto diskinfo_vdb = mpdisks["vdb"];
        std::string vdbfile = diskinfo_vdb->getSourceFile();
        if (!bfs::exists(vdbfile)) {
            int64_t disk_size = diskinfo_vdb->getVirtualSize() / 1024L / 1024L / 1024L;
            std::string cmd_create_img = "qemu-img create -f qcow2 " + vdbfile + " " + std::to_string(disk_size) + "G";
            std::string create_ret = run_shell(cmd_create_img);
            TASK_LOG_INFO(domain_name, "create data: " << cmd_create_img << ", result: " << create_ret);
        }
        disk_vdb = mpdisks["vdb"]->getSourceFile();
    }

    if (!taskinfo->getMulticast().empty()) {
        auto multicasts = taskinfo->getMulticast();
        for (const auto & mcast : multicasts) {
            TASK_LOG_INFO(domain_name, "add multicast address: " << mcast);
        }
    }

    std::string bridge_name;
    if (!taskinfo->getNetworkName().empty()) {
        std::shared_ptr<dbc::networkInfo> networkInfo = VxlanManager::instance().GetNetwork(taskinfo->getNetworkName());
        if (networkInfo) {
            bridge_name = networkInfo->bridgeName;
            TASK_LOG_INFO(domain_name, "join network: " << taskinfo->getNetworkName());
        }
    }

    // vnc
    TASK_LOG_INFO(domain_name, "vnc port: " << taskinfo->getVncPort() << ", password: " << taskinfo->getVncPassword());

    std::string xml_content = createXmlStr(buf_uuid, domain_name,
        memoryTotal, cpuNumTotal, 
        taskinfo->getCpuSockets(), taskinfo->getCpuCoresPerSocket(), taskinfo->getCpuThreadsPerCore(),
        vga_pci, disk_vda, disk_vdb,
        taskinfo->getVncPort(), taskinfo->getVncPassword(),
        taskinfo->getMulticast(), bridge_name,
        taskinfo->getBiosMode(),
        isWindowsOS(taskinfo->getOperationSystem()));

    virDomainPtr domainPtr = nullptr;
    int32_t errorNum = ERR_SUCCESS;
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

FResult VmClient::RedefineDomain(const std::shared_ptr<TaskInfo>& taskinfo) {
	if (m_connPtr == nullptr) {
		return FResult(ERR_ERROR, "libvirt disconnect");
	}

	virDomainPtr domainPtr = virDomainLookupByName(m_connPtr, taskinfo->getTaskId().c_str());
	if (nullptr == domainPtr) {
        return FResult(ERR_ERROR, "task:" + taskinfo->getTaskId() + " not exist");
	}

	// new gpu
    auto gpus = TaskGpuMgr::instance().getTaskGpus(taskinfo->getTaskId());
	std::string vga_pci;
	for (auto& it : gpus) {
        auto ids = it.second->getDeviceIds();
		for (auto& it2 : ids) {
			vga_pci += it2 + "|";
		}
	}

	// new cpu
	int32_t cpuNumTotal = taskinfo->getTotalCores();
    int32_t cpuSockets = taskinfo->getCpuSockets();
    int32_t cpuCores = taskinfo->getCpuCoresPerSocket();
    int32_t cpuThreads = taskinfo->getCpuThreadsPerCore();
 
	// new mem
	int64_t memoryTotal = taskinfo->getMemSize(); // KB

	char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE | VIR_DOMAIN_XML_INACTIVE);
    if (pContent != nullptr) {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            virDomainFree(domainPtr);
            return FResult(ERR_ERROR, "task:" + taskinfo->getTaskId() + " parse domain xml failed");
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
        if (ele_devices != nullptr) {
            // gpu
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

            // vnc
            tinyxml2::XMLElement* ele_graphics = ele_devices->FirstChildElement("graphics");
            while (ele_graphics) {
                std::string graphics_type = ele_graphics->Attribute("type");
                if (graphics_type == "vnc") {
                    ele_graphics->SetAttribute("port", taskinfo->getVncPort());
                    ele_graphics->SetAttribute("autoport", taskinfo->getVncPort() == -1 ? "yes" : "no");
                    break;
                }
                ele_graphics = ele_graphics->NextSiblingElement("graphics");
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

bool VmClient::ListDomainDiskInfo(const std::string& domain_name, std::map<std::string, domainDiskInfo>& disks) {
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
	} while (0);

	if (pContent) {
		free(pContent);
	}
	if (domain_ptr != nullptr) {
		virDomainFree(domain_ptr);
	}
	return ret;
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
    } catch (const std::exception & e) {
        return FResult(ERR_ERROR, std::string("log file error: ").append(e.what()));
    } catch (const boost::exception & e) {
        return FResult(ERR_ERROR, "log file error: " + diagnostic_information(e));
    } catch (...) {
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
    std::string vm_local_ip;

    int32_t try_count = 0;
    while (vm_local_ip.empty() && try_count < 1000) {
        LOG_INFO << "transform_port try_count: " << (try_count + 1);

        do {        
            if (m_connPtr == nullptr) {
                TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
                break;
            }

            virDomainPtr domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
            if (nullptr == domainPtr) {
                TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
                break;
            }
            std::shared_ptr<virDomain> temp(domainPtr, virDomainDeleter());

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
        } while(0);

        try_count += 1;
        sleep(3);
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

FResult VmClient::SetDomainUserPassword(const std::string &domain_name, const std::string &username, const std::string &pwd, int max_retry_count) {
    int ret = ERR_ERROR;
    std::string errmsg;
    int try_count = 0;
    int succ = -1;
    // max: 5min
    while (succ != 0 && try_count < max_retry_count) {
        LOG_INFO << "set_user_password try_count: " << (try_count + 1);
        do {
            if (m_connPtr == nullptr) {
                TASK_LOG_ERROR(domain_name, "connPtr is nullptr");
                errmsg = "libvirt disconnect";
                break;
            }

            virDomainPtr domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
            if (domain_ptr == nullptr) {
                errmsg = "task not existed";
                TASK_LOG_ERROR(domain_name, "lookup domain:" << domain_name << " is nullptr");
                break;
            }
            std::shared_ptr<virDomain> temp(domain_ptr, virDomainDeleter());

            LOG_INFO << domain_name << " set_vm_password try_count: " << (try_count + 1);
            succ = virDomainSetUserPassword(domain_ptr, username.c_str(), pwd.c_str(), 0);
            if (succ != 0) {
                virErrorPtr error = virGetLastError();
                errmsg = error ? error->message : "unknown error";
                LOG_ERROR << domain_name << " virDomainSetUserPassword error: " << errmsg;
            }
        } while(0);

        try_count++;
        sleep(3);
    }

    if (succ == 0) {
        ret = ERR_SUCCESS;
        TASK_LOG_INFO(domain_name, "set vm user password successful, user:" << username << ", pwd:" << pwd);
    } else {
        TASK_LOG_ERROR(domain_name, "set vm user password failed, user:" << username
                << ", pwd:" << pwd << ", error:" << errmsg);
    }

    return FResult(ret, errmsg);
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

    std::map<std::string, std::shared_ptr<DiskInfo>> disks;
    TaskDiskMgr::instance().listDisks(domain_name, disks);
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
        auto gpus = TaskGpuMgr::instance().getTaskGpus(domain_name);
        if (isActive && gpus.size() > 0) {
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

int64_t VmClient::GetDiskVirtualSize(const std::string& domain_name, const std::string& disk_name) {
    virDomainPtr domain_ptr = nullptr;
    domain_ptr = virDomainLookupByName(m_connPtr, domain_name.c_str());
    if (domain_ptr == nullptr) {
        LOG_ERROR << " lookup domain:" << domain_name << " is nullptr";
        return 0;
    }

    int64_t disk_size = 0;
    virDomainBlockInfo blockinfo;
    if (virDomainGetBlockInfo(domain_ptr, disk_name.c_str(), &blockinfo, 0) >= 0) {
        disk_size = blockinfo.capacity;
    }

    if (domain_ptr != nullptr)
        virDomainFree(domain_ptr);

    return disk_size;
}

FResult VmClient::AttachDisk(const std::string& domain_name, const std::string& disk_name, const std::string& source_file) {
    FResult ret = FResultOk;
    virDomainPtr domainPtr = nullptr;
    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domainPtr == nullptr) {
            ret = FResult(ERR_ERROR, "task not exist");
            break;
        }

        char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE);
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            ret = FResult(ERR_ERROR, "task parse domain xml failed");
            break;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
        tinyxml2::XMLElement* ele_disk = ele_devices->LastChildElement("disk");

        tinyxml2::XMLElement* disk_data_node = doc.NewElement("disk");
        disk_data_node->SetAttribute("type", "file");
        disk_data_node->SetAttribute("device", "disk");
        tinyxml2::XMLElement* disk_driver_node = doc.NewElement("driver");
        disk_driver_node->SetAttribute("name", "qemu");
        disk_driver_node->SetAttribute("type", "qcow2");
        disk_data_node->LinkEndChild(disk_driver_node);
        tinyxml2::XMLElement* disk_source_node = doc.NewElement("source");
        disk_source_node->SetAttribute("file", source_file.c_str());
        disk_data_node->LinkEndChild(disk_source_node);
        tinyxml2::XMLElement* disk_target_node = doc.NewElement("target");
        disk_target_node->SetAttribute("dev", disk_name.c_str());
        disk_target_node->SetAttribute("bus", "virtio");
        disk_data_node->LinkEndChild(disk_target_node);

        ele_devices->InsertAfterChild(ele_disk, disk_data_node);

        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        const char* xml_content = printer.CStr();

        domainPtr = virDomainDefineXML(m_connPtr, xml_content);
        if (domainPtr == nullptr) {
            ret = FResult(ERR_ERROR, "domain defineXML failed");
            break;
        }
    } while (0);

    if (domainPtr != nullptr)
        virDomainFree(domainPtr);

    return ret;
}

FResult VmClient::DetachDisk(const std::string& domain_name, const std::string& disk_name) {
    FResult ret = FResultOk;
    virDomainPtr domainPtr = nullptr;
    do {
        domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
        if (domainPtr == nullptr) {
            ret = FResult(ERR_ERROR, "task not exist");
            break;
        }

        char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE);
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(pContent);
        if (err != tinyxml2::XML_SUCCESS) {
            ret = FResult(ERR_ERROR, "task parse domain xml failed");
            break;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
        tinyxml2::XMLElement* ele_disk = ele_devices->FirstChildElement("disk");
        while (ele_disk != nullptr) {
            tinyxml2::XMLElement* ele_target = ele_disk->FirstChildElement("target");
            std::string str_dev = ele_target->Attribute("dev");
            if (str_dev == disk_name) {
                ele_devices->DeleteChild(ele_disk);
                break;
            }
            ele_disk = ele_disk->NextSiblingElement("disk");
        }
 
        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        const char* xml_content = printer.CStr();

        domainPtr = virDomainDefineXML(m_connPtr, xml_content);
        if (domainPtr == nullptr) {
            ret = FResult(ERR_ERROR, "domain defineXML failed");
            break;
        }
    } while (0);

    if (domainPtr != nullptr)
        virDomainFree(domainPtr);

    return ret;
}

std::string VmClient::GetDomainXML(const std::string& domain_name) {
    if (m_connPtr == nullptr) {
        LOG_ERROR << domain_name << " connPtr is nullptr";
        return "";
    }

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
            LOG_ERROR << domain_name << " get domain xml failed";
            break;
        }
    } while (0);

    std::string str;
    if (pContent != nullptr) {
        str = pContent;
    }

    if (pContent) {
        free(pContent);
    }

    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }

    return std::move(str);
}

int32_t VmClient::DefineNWFilter(const std::string& nwfilter_name, const std::vector<std::string>& nwfilters) {
    int32_t ret = -1;
    if (m_connPtr == nullptr) {
        LOG_ERROR << " connPtr is nullptr";
        return ret;
    }

    std::string xmlDesc, uuid;
    virNWFilterPtr nwfilter = nullptr;
    do {
        nwfilter = virNWFilterLookupByName(m_connPtr, nwfilter_name.c_str());
        if (nwfilter != nullptr) {
            char buf[VIR_UUID_STRING_BUFLEN] = { 0 };
            if (virNWFilterGetUUIDString(nwfilter, buf) < 0) {
                LOG_ERROR << " get uuid of nwfilter:" << nwfilter_name << " error";
                break;
            }
            uuid = buf;
            virNWFilterFree(nwfilter);
            nwfilter = nullptr;
        }
        xmlDesc = createNWFilterXml(nwfilter_name, uuid, nwfilters);
        nwfilter = virNWFilterDefineXML(m_connPtr, xmlDesc.c_str());
        if (nwfilter == nullptr) {
            LOG_ERROR << " define nwfilter:" << nwfilter_name << " error";
            break;
        }
        ret = ERR_SUCCESS;
    } while (0);

    if (nwfilter != nullptr) {
        virNWFilterFree(nwfilter);
    }
    return ret;
}

int32_t VmClient::UndefineNWFilter(const std::string& nwfilter_name) {
    int32_t ret = -1;
    if (m_connPtr == nullptr) {
        LOG_ERROR << " connPtr is nullptr";
        return ret;
    }

    virNWFilterPtr nwfilter = nullptr;
    do {
        nwfilter = virNWFilterLookupByName(m_connPtr, nwfilter_name.c_str());
        if (nwfilter == nullptr) {
            LOG_ERROR << " lookup nwfilter:" << nwfilter_name << " is nullptr";
            break;
        }
        ret = virNWFilterUndefine(nwfilter);
        if (ret < 0) {
            LOG_ERROR << " undefine nwfilter:" << nwfilter_name << " error";
            break;
        }
    } while (0);

    if (nwfilter != nullptr) {
        virNWFilterFree(nwfilter);
    }
    return ret;
}

FResult VmClient::ListDomainInterface(const std::string& domain_name, std::vector<domainInterface>& interfaces) {
	FResult ret = FResultOk;
	virDomainPtr domainPtr = nullptr;
	do {
		domainPtr = virDomainLookupByName(m_connPtr, domain_name.c_str());
		if (domainPtr == nullptr) {
			ret = FResult(ERR_ERROR, "task not exist");
			break;
		}

		char* pContent = virDomainGetXMLDesc(domainPtr, VIR_DOMAIN_XML_SECURE);
		tinyxml2::XMLDocument doc;
		tinyxml2::XMLError err = doc.Parse(pContent);
		if (err != tinyxml2::XML_SUCCESS) {
			ret = FResult(ERR_ERROR, "task parse domain xml failed");
			break;
		}
		tinyxml2::XMLElement* root = doc.RootElement();
		tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
		tinyxml2::XMLElement* ele_interface = ele_devices->LastChildElement("interface");
        while (ele_interface) {
            domainInterface domain_interface;

            std::string interface_type = ele_interface->Attribute("type");
            if (interface_type == "network") {
                tinyxml2::XMLElement* ele_source = ele_interface->FirstChildElement("source");
                std::string i_name = ele_source->Attribute("network");
                
				tinyxml2::XMLElement* ele_model = ele_interface->FirstChildElement("model");
				std::string i_type = ele_model->Attribute("type");

				tinyxml2::XMLElement* ele_mac = ele_interface->FirstChildElement("mac");
				std::string i_mac = ele_mac->Attribute("address");

                domain_interface.name = i_name;
                domain_interface.type = i_type;
                domain_interface.mac = i_mac;
                interfaces.push_back(domain_interface);
            }
            else if (interface_type == "bridge") {
				tinyxml2::XMLElement* ele_source = ele_interface->FirstChildElement("source");
				std::string i_name = ele_source->Attribute("bridge");

				tinyxml2::XMLElement* ele_model = ele_interface->FirstChildElement("model");
				std::string i_type = ele_model->Attribute("type");

				tinyxml2::XMLElement* ele_mac = ele_interface->FirstChildElement("mac");
				std::string i_mac = ele_mac->Attribute("address");

				domain_interface.name = i_name;
				domain_interface.type = i_type;
				domain_interface.mac = i_mac;
                interfaces.push_back(domain_interface);
            }
            
            ele_interface = ele_interface->NextSiblingElement("interface");
        }
	} while (0);

	if (domainPtr != nullptr)
		virDomainFree(domainPtr);

	return ret;
}

void VmClient::DefaultEventThreadFunc() {
    LOG_INFO << "vir event loop thread begin";
    while (m_event_loop_run == 1) {
        if (virEventRunDefaultImpl() < 0) {
            // virErrorPtr err = virGetLastError();
            // LOG_INFO << "virEventRunDefaultImpl failed: " << err ? err->message : "";
        }
    }
    LOG_INFO << "vir event loop thread end";
}

void VmClient::StartEventLoop() {
    if (!m_thread_event_loop) {
        m_event_loop_run = 1;
        m_thread_event_loop = new std::thread(&VmClient::DefaultEventThreadFunc, this);
    }
}

void VmClient::StopEventLoop() {
    m_event_loop_run = 0;
    if (m_thread_event_loop) {
        if (m_thread_event_loop->joinable())
            m_thread_event_loop->join();
        delete m_thread_event_loop;
        m_thread_event_loop = nullptr;
    }
}
