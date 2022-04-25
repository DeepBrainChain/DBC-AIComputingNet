// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for virt monitor"
#include <boost/test/unit_test.hpp>
#include "vir_helper.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include "tinyxml2.h"

using namespace vir_helper;

static const char* qemu_url = "qemu+tcp://localhost:16509/system";
static const char* default_domain_name = "domain_test";
// enum virDomainState
static const char* arrayDomainState[] = {"no state", "running", "blocked", "paused", "shutdown", "shutoff", "crashed", "pmsuspended", "last"};

std::ostream& operator<<(std::ostream& out, const virDomainInfo& obj) {
    std::cout << " 状态:";
    std::cout << std::setw(11) << std::setfill(' ') << std::left << arrayDomainState[obj.state];
    std::cout << " 最大内存:";
    std::cout << std::setw(10) << std::setfill(' ') << std::left << (obj.maxMem / 1024 / 1024);
    std::cout << " 内存:";
    std::cout << std::setw(10) << std::setfill(' ') << std::left << (obj.memory / 1024 / 1024);
    std::cout << " cpu个数:";
    std::cout << std::setw(5) << std::setfill(' ') << std::left << obj.nrVirtCpu;
    std::cout << " cpu时间:";
    std::cout << std::setw(20) << std::setfill(' ') << std::left << obj.cpuTime;
}

//全局测试夹具
struct global_fixture
{
    static global_fixture*& instance() {
        static global_fixture *g_inst = nullptr;
        return g_inst;
    }

    global_fixture() {
        instance() = this;
        std::cout << "command format: snapshot_test -- [domain name]" << std::endl;
        std::cout << "开始准备测试数据------->" << std::endl;
        // BOOST_REQUIRE(vir_helper_.openConnect(qemu_url));
    }
    virtual~global_fixture() {
        std::cout << "清理测试环境<---------" << std::endl;
    }
    virHelper vir_helper_;
};

BOOST_GLOBAL_FIXTURE(global_fixture);

struct assign_fixture
{
    assign_fixture() {
        std::cout << "suit setup\n";
        domain_name_ = default_domain_name;

        using namespace boost::unit_test;
        if (framework::master_test_suite().argc > 1) {
            domain_name_ = framework::master_test_suite().argv[1];
        }
    }
    ~assign_fixture() {
        std::cout << "suit teardown\n";
    }

    std::string domain_name_;
};

BOOST_AUTO_TEST_CASE(testConnect) {
    std::cout << "test connect" << std::endl;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.openConnect(qemu_url));
}

// BOOST_AUTO_TEST_SUITE(vir_monitor)
BOOST_FIXTURE_TEST_SUITE(vir_monitor, assign_fixture)

BOOST_AUTO_TEST_CASE(testCommandParams) {
    std::cout << "domain name = \"" << domain_name_ << "\"" << std::endl;
}

BOOST_AUTO_TEST_CASE(testGetDomainInfo) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    virDomainInfo info;
    BOOST_REQUIRE(domain->getDomainInfo(&info) > -1);
    std::cout << info << std::endl;
}

BOOST_AUTO_TEST_CASE(testCpuUsage1) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    virDomainInfo info_s, info_e;
    struct timeval real_time_s, real_time_e;
    int cpu_diff, real_diff;
    float usage;
    BOOST_REQUIRE(domain->getDomainInfo(&info_s) > -1);
    BOOST_REQUIRE(gettimeofday(&real_time_s, NULL) == 0);
    sleep(1);
    BOOST_REQUIRE(domain->getDomainInfo(&info_e) > -1);
    BOOST_REQUIRE(gettimeofday(&real_time_e, NULL) == 0);
    //转换成微秒
    cpu_diff = (info_e.cpuTime - info_s.cpuTime) / 1000;
    real_diff = 1000000 * (real_time_e.tv_sec - real_time_s.tv_sec) + (real_time_e.tv_usec - real_time_s.tv_usec);
    //是否要考虑多核的情况？
    usage = cpu_diff / (float) (real_diff);
    std::cout << " cpu_diff:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << cpu_diff;
    std::cout << " real_diff:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << real_diff;
    std::cout << " cpu usage:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << usage;
    std::cout << " cpu average usage:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << (usage / info_s.nrVirtCpu);
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(testCpuUsage2) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    unsigned int nparams = domain->getDomainCPUStats(NULL, 0, -1, 1, 0);
    virTypedParameterPtr params_s = (virTypedParameterPtr)calloc(nparams, sizeof(virTypedParameter));
    virTypedParameterPtr params_e = (virTypedParameterPtr)calloc(nparams, sizeof(virTypedParameter));
    struct timeval real_time_s, real_time_e;
    int cpu_diff, real_diff;
    float usage;
    BOOST_REQUIRE(gettimeofday(&real_time_s, NULL) == 0);
    domain->getDomainCPUStats(params_s, nparams, -1, 1, 0);
    sleep(1);
    BOOST_REQUIRE(gettimeofday(&real_time_e, NULL) == 0);
    domain->getDomainCPUStats(params_e, nparams, -1, 1, 0);
    cpu_diff = (params_e->value.ul - params_s->value.ul) / 1000;
    real_diff = 1000000 * (real_time_e.tv_sec - real_time_s.tv_sec) + (real_time_e.tv_usec - real_time_s.tv_usec);
    usage = cpu_diff / (float)(real_diff);
    free(params_s);
    free(params_e);
    std::cout << " cpu_diff:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << cpu_diff;
    std::cout << " real_diff:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << real_diff;
    std::cout << " cpu usage:";
    std::cout << std::setw(12) << std::setfill(' ') << std::left << usage;
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(testMemoryStats) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    virDomainMemoryStatStruct stats[20] = {0};
    BOOST_REQUIRE(domain->getDomainMemoryStats(stats, 20, 0) > -1);
    std::map<int, unsigned long long> mapstats;
    for (int i = 0; i < 20; ++i) {
        // std::cout << stats[i].tag << "    " << stats[i].val << std::endl;
        if (mapstats.find(stats[i].tag) == mapstats.end())
            mapstats.insert(std::make_pair(stats[i].tag, stats[i].val));
    }
    float total = mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON] / 1024 / (float)1024;
    float unused = mapstats[VIR_DOMAIN_MEMORY_STAT_UNUSED] / 1024 / (float)1024;
    float available = mapstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE] / 1024 / (float)1024;
    float used = (mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON] - mapstats[VIR_DOMAIN_MEMORY_STAT_UNUSED]) / 1024 / (float)1024;
    float usage = (float)(mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON] - mapstats[VIR_DOMAIN_MEMORY_STAT_UNUSED]) / mapstats[VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON];
    std::cout << " total:";
    std::cout << std::setw(6) << std::setfill(' ') << std::left << total;
    std::cout << " unused:";
    std::cout << std::setw(6) << std::setfill(' ') << std::left << unused;
    std::cout << " available:";
    std::cout << std::setw(6) << std::setfill(' ') << std::left << available;
    std::cout << " used:";
    std::cout << std::setw(6) << std::setfill(' ') << std::left << used;
    std::cout << " usage:";
    std::cout << std::setw(6) << std::setfill(' ') << std::left << usage;
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(testDiskInfo) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);

    std::vector<domainDiskInfo> disks;
    BOOST_REQUIRE(domain->getDomainDisks(disks) > -1);

    for (const auto& disk : disks) {
        virDomainBlockInfo info;
        BOOST_REQUIRE(domain->getDomainBlockInfo(disk.target_dev.c_str(), &info, 0) > -1);
        std::cout << " disk_name:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << disk.target_dev;
        std::cout << " capacity:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << info.capacity / 1024 / 1024 / (float)1024;
        std::cout << " allocation:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << info.allocation / 1024 / 1024 / (float)1024;
        std::cout << " physical:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << info.physical / 1024 / 1024 / (float)1024;
        std::cout << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(testDiskStats) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    long long rd_bytes, wr_bytes;
    virDomainBlockStatsStruct stats;
    BOOST_REQUIRE(domain->getDomainBlockStats("vda", &stats, sizeof(stats)) > -1);
    // std::cout << " rd_req:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.rd_req;
    // std::cout << " rd_bytes:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.rd_bytes;
    // std::cout << " wr_req:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.wr_req;
    // std::cout << " wr_bytes:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.wr_bytes;
    // std::cout << " errs:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.errs;
    rd_bytes = stats.rd_bytes;
    wr_bytes = stats.wr_bytes;
    for (int i = 0; i < 10; ++i) {
        sleep(1);
        BOOST_REQUIRE(domain->getDomainBlockStats("vda", &stats, sizeof(stats)) > -1);
        std::cout << " rd speed:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << ((stats.rd_bytes - rd_bytes) / (float)1024);
        std::cout << " wr speed:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << ((stats.wr_bytes - wr_bytes) / (float)1024);
        std::cout << std::endl;
        rd_bytes = stats.rd_bytes;
        wr_bytes = stats.wr_bytes;
    }
}

// BOOST_AUTO_TEST_CASE(testNetworkList) {
//     std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
//     BOOST_REQUIRE(domain);
//     std::vector<domainInterface> difaces;
//     BOOST_REQUIRE(domain->getDomainInterfaceAddress(difaces) > 0);
//     BOOST_CHECK(!difaces.empty());
// }

BOOST_AUTO_TEST_CASE(testNetworkStats) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);

    // get network device name, exp vnet0.
    std::vector<domainInterface> difaces;
    BOOST_REQUIRE(domain->getDomainInterfaceAddress(difaces) > 0);
    BOOST_REQUIRE(!difaces.empty());
    std::string network_device_name = difaces[0].name;
    BOOST_REQUIRE(!network_device_name.empty());
    std::cout << " find network interface name:" << network_device_name << std::endl;

    // get network stats
    long long rx_bytes, tx_bytes;
    virDomainInterfaceStatsStruct stats;
    // BOOST_REQUIRE(domain->getDomainNetworkStats("vnet0", &stats, sizeof(stats)) > -1);
    // BOOST_REQUIRE(domain->getDomainNetworkStats("52:54:00:b3:5d:b8", &stats, sizeof(stats)) > -1);
    BOOST_REQUIRE(domain->getDomainNetworkStats(network_device_name.c_str(), &stats, sizeof(stats)) > -1);
    // std::cout << " rx_bytes:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.rx_bytes;
    // std::cout << " rx_packets:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.rx_packets;
    // std::cout << " rx_errs:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.rx_errs;
    // std::cout << " rx_drop:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.rx_drop;
    // std::cout << " tx_bytes:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.tx_bytes;
    // std::cout << " tx_packets:";
    // std::cout << std::setw(12) << std::setfill(' ') << std::left << stats.tx_packets;
    // std::cout << " tx_errs:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.tx_errs;
    // std::cout << " tx_drop:";
    // std::cout << std::setw(6) << std::setfill(' ') << std::left << stats.tx_drop;
    rx_bytes = stats.rx_bytes;
    tx_bytes = stats.tx_bytes;
    for (int i = 0; i < 10; ++i) {
        sleep(1);
        BOOST_REQUIRE(domain->getDomainNetworkStats(network_device_name.c_str(), &stats, sizeof(stats)) > -1);
        std::cout << " in speed:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << ((stats.rx_bytes - rx_bytes) / (float)1024);
        std::cout << " out speed:";
        std::cout << std::setw(6) << std::setfill(' ') << std::left << ((stats.tx_bytes - tx_bytes) / (float)1024);
        std::cout << std::endl;
        rx_bytes = stats.rx_bytes;
        tx_bytes = stats.tx_bytes;
    }
}

BOOST_AUTO_TEST_CASE(testQemuAgentCommand) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    std::string cmd = "{\"execute\":\"guest-info\"}";
    std::string ret = domain->QemuAgentCommand(cmd.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
    BOOST_REQUIRE(!ret.empty());
    std::cout << ret << std::endl;
}

BOOST_AUTO_TEST_CASE(testGpuStats) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_helper_.openDomainByName(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    std::string cmd = "{\"execute\":\"guest-get-gpus\"}";
    std::string ret = domain->QemuAgentCommand(cmd.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
    BOOST_REQUIRE(!ret.empty());
    std::cout << ret << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
