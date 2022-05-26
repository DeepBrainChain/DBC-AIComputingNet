#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

#include "vir_helper.h"
#include "TaskMonitorInfo.h"
#include "zabbixSender.h"
#include "common/common.h"

using namespace vir_helper;

static const char* qemu_url = "qemu+tcp://localhost:16509/system";
// enum virDomainState
static const char* arrayDomainState[] = {"no state", "running", "blocked", "paused", "shutdown", "shutoff", "crashed", "pmsuspended", "last"};
// default zabbix server
static const char* default_zabbix_host = "116.169.53.132";
static const char* default_zabbix_port = "10051";

std::ostream& operator <<(std::ostream& out, const dbcMonitor::domMonitorData& obj) {
    out << "domain_name: " << obj.domainName << std::endl;
    out << "delay: " << obj.delay << std::endl;
    out << "domain_state: " << obj.domInfo.state << std::endl;
    out << "cpuTime: " << obj.domInfo.cpuTime << std::endl;
    out << "real time sec: " << obj.domInfo.realTime.tv_sec << std::endl;
    out << "real time usec: " << obj.domInfo.realTime.tv_nsec << std::endl;
    return out;
}

namespace dbcMonitor{
void getDomainInfo(domMonitorData& data, std::shared_ptr<virDomainImpl> domain) {
    virDomainInfo info;
    if (domain->getDomainInfo(&info) > -1) {
        data.domInfo.state = arrayDomainState[info.state];
        data.domInfo.maxMem = info.maxMem;
        data.domInfo.memory = info.memory;
        data.domInfo.nrVirtCpu = info.nrVirtCpu;
        data.domInfo.cpuTime = info.cpuTime;
        clock_gettime(CLOCK_REALTIME, &data.domInfo.realTime);
        data.domInfo.cpuUsage = 0.0f;
    }
}

void getDomainMemoryInfo(domMonitorData& data, std::shared_ptr<virDomainImpl> domain) {
    virDomainMemoryStatStruct stats[20] = {0};
    if (domain->getDomainMemoryStats(stats, 20, 0) > -1) {
        clock_gettime(CLOCK_REALTIME, &data.memStats.realTime);
        std::map<int, unsigned long long> mapstats;
        for (int i = 0; i < 20; ++i) {
            if (mapstats.find(stats[i].tag) == mapstats.end())
                mapstats.insert(std::make_pair(stats[i].tag, stats[i].val));
        }
        data.memStats.total = mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON];
        data.memStats.unused = mapstats[VIR_DOMAIN_MEMORY_STAT_UNUSED];
        data.memStats.available = mapstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE];
        data.memStats.usage = (float)(data.memStats.total - data.memStats.unused) / data.memStats.total;
    }
}

void getDomainDiskInfo(domMonitorData& data, std::shared_ptr<virDomainImpl> domain) {
    std::vector<domainDiskInfo> disks;
    if (domain->getDomainDisks(disks) > -1) {
        for (const auto& disk : disks) {
            struct diskInfo dsInfo;
            dsInfo.name = disk.target_dev;
            virDomainBlockInfo info;
            domain->getDomainBlockInfo(disk.target_dev.c_str(), &info, 0);
            virDomainBlockStatsStruct stats;
            dsInfo.capacity = info.capacity;
            dsInfo.allocation = info.allocation;
            dsInfo.physical = info.physical;
            domain->getDomainBlockStats(disk.target_dev.c_str(), &stats, sizeof(stats));
            clock_gettime(CLOCK_REALTIME, &dsInfo.realTime);
            dsInfo.rd_req = stats.rd_req;
            dsInfo.rd_bytes = stats.rd_bytes;
            dsInfo.wr_req = stats.wr_req;
            dsInfo.wr_bytes = stats.wr_bytes;
            dsInfo.errs = stats.errs;
            dsInfo.rd_speed = 0.0;
            dsInfo.wr_speed = 0.0;
            data.diskStats[disk.target_dev] = dsInfo;
        }
    }
}

void getDomainNetworkInfo(domMonitorData& data, std::shared_ptr<virDomainImpl> domain) {
    std::vector<domainInterface> difaces;
    if (domain->getDomainInterfaceAddress(difaces) > 0) {
        for (const auto& diface: difaces) {
            networkInfo netInfo;
            netInfo.name = diface.name;
            virDomainInterfaceStatsStruct stats;
            domain->getDomainNetworkStats(diface.name.c_str(), &stats, sizeof(stats));
            clock_gettime(CLOCK_REALTIME, &netInfo.realTime);
            netInfo.rx_bytes = stats.rx_bytes;
            netInfo.rx_packets = stats.rx_packets;
            netInfo.rx_errs = stats.rx_errs;
            netInfo.rx_drop = stats.rx_drop;
            netInfo.tx_bytes = stats.tx_bytes;
            netInfo.tx_packets = stats.tx_packets;
            netInfo.tx_errs = stats.tx_errs;
            netInfo.tx_drop = stats.tx_drop;
            netInfo.rx_speed = 0.0;
            netInfo.tx_speed = 0.0;
            data.netStats[diface.name] = netInfo;
        }
    }
}

void getDomainGpuInfo(domMonitorData& data, std::shared_ptr<virDomainImpl> domain) {
    std::string result = domain->QemuAgentCommand("{\"execute\":\"guest-get-gpus\"}", VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
    if (!result.empty()) {
        rapidjson::Document doc;
        doc.Parse(result.c_str());
        if (!doc.IsObject()) {
            std::cout << " parse guest agent result data error" << std::endl;
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
                std::cout << " guest agent result has no return node" << std::endl;
            }
        }
    } else {
        std::cout << "qemu guest command guest-get-gpus return empty" << std::endl;
    }
}
}

int main(int argc, char* argv[]) {
    std::string domain_name;
    std::string zabbix_host = default_zabbix_host;
    std::string zabbix_port = default_zabbix_port;

    boost::program_options::options_description opts("collectMonitorData command options");
    opts.add_options()
        ("domain_name", boost::program_options::value<std::string>(), "specify domain name")
        ("zabbix", boost::program_options::value<std::string>(), "zabbix server, such as 116.169.53.132:10051")
        ("help", "command options help");

    try {
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
        boost::program_options::notify(vm);

        if (vm.count("domain_name")) {
            domain_name = vm["domain_name"].as<std::string>();
        }
        if (vm.count("zabbix")) {
            std::string server = vm["zabbix"].as<std::string>();
            std::size_t pos = server.find(":");
            if (pos == std::string::npos) {
                zabbix_host = server;
            } else {
                zabbix_host = server.substr(0, pos);
                zabbix_port = server.substr(pos + 1);
            }
            std::cout << "zabbix host:" << zabbix_host << ", port:" << zabbix_port << std::endl;
        }
        if (vm.count("help") > 0) {
            std::cout << opts;
            return 0;
        }
    }
    catch (const std::exception &e) {
        std::cout << "invalid command option " << e.what() << std::endl;
        std::cout << opts;
        return 0;
    }

    virHelper virt_helper;
    if (!virt_helper.openConnect(qemu_url)) {
        std::cout << "open libvirt connect failed" << std::endl;
        return -1;
    }

    std::shared_ptr<virDomainImpl> domain = virt_helper.openDomainByName(domain_name.c_str());
    if (!domain) {
        std::cout << "open domain: " << domain_name << " failed" << std::endl;
        return -1;
    }

    dbcMonitor::domMonitorData dmData;
    dmData.domainName = domain_name;
    dmData.delay = 5;
    dmData.version = dbcversion();

    // get domain info
    dbcMonitor::getDomainInfo(dmData, domain);
    dbcMonitor::getDomainMemoryInfo(dmData, domain);
    dbcMonitor::getDomainDiskInfo(dmData, domain);
    dbcMonitor::getDomainNetworkInfo(dmData, domain);
    dbcMonitor::getDomainGpuInfo(dmData, domain);

    // calculator

    // sender monitor data
    // std::cout << dmData.toZabbixString(domain_name) << std::endl;
    zabbixSender zs(zabbix_host, zabbix_port);
    if (zs.is_server_want_monitor_data(domain_name)) {
        if (!zs.sendJsonData(dmData.toZabbixString())) {
            std::cout << "send monitor data of task(" << domain_name << ") to server(" << zabbix_host << ") error" << std::endl;
        } else {
            std::cout << "send monitor data of task(" << domain_name << ") to server(" << zabbix_host << ") success" << std::endl;
        }
    } else {
        std::cout << "server: " << zabbix_host << " does not need monitor data of vm: " << domain_name << std::endl;
    }
    return 0;
}
