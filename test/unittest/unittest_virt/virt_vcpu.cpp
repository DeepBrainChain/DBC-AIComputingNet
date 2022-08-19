// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for virt monitor"
#include <boost/test/unit_test.hpp>
#include "vir_helper.h"
#include <bitset>
#include <iostream>
#include <iomanip>
#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include "tinyxml2.h"

using namespace vir_helper;

static const char* qemu_url = "qemu+tcp://localhost:16509/system";
static const char* default_domain_name = "domain_test";

std::ostream& operator<<(std::ostream& out, const virVcpuInfo& obj) {
    std::cout << " virtual number:";
    std::cout << std::setw(10) << std::setfill(' ') << std::left << obj.number;
    std::cout << " state:";
    std::cout << std::setw(10) << std::setfill(' ') << std::left << obj.state;
    std::cout << " cpuTime:";
    std::cout << std::setw(20) << std::setfill(' ') << std::left << obj.cpuTime;
    std::cout << " cpu:";
    std::cout << std::setw(11) << std::setfill(' ') << std::left << obj.cpu;
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
        std::cout << "command format: vcpu_test -- [domain name]" << std::endl;
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
        domain_name_ = default_domain_name;

        using namespace boost::unit_test;
        if (framework::master_test_suite().argc > 1) {
            domain_name_ = framework::master_test_suite().argv[1];
        }
        std::cout << "suit setup of " <<
            framework::current_test_case().full_name() << std::endl;
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

// BOOST_AUTO_TEST_SUITE(vir_vcpu)
BOOST_FIXTURE_TEST_SUITE(vir_vcpu, assign_fixture)

BOOST_AUTO_TEST_CASE(testCommandParams) {
    std::cout << "domain name = \"" << domain_name_ << "\"" << std::endl;
}

BOOST_AUTO_TEST_CASE(testGetCPUModelNames) {
    std::vector<std::string> cpu_models;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.getCPUModelNames(
        "x86_64", cpu_models) >= 0);
    std::cout << "get cpu model name of x86_64 return:";
    for (const auto& model : cpu_models) {
        std::cout << " " << model;
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(testGetMaxVcpus) {
    static const char kvm[] = "kvm";
    std::cout << "connect get max vcpus of type " << kvm
        << " return "
        << global_fixture::instance()->vir_helper_.getMaxVcpus("kvm")
        << std::endl;
}

BOOST_AUTO_TEST_CASE(testGetNodeInfo) {
    virNodeInfo info;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.getNodeInfo(&info) >= 0);
    std::cout << "model: " << info.model << std::endl;
    std::cout << "memory: " << info.memory << std::endl;
    std::cout << "cpus: " << info.cpus << std::endl;
    std::cout << "mhz: " << info.mhz << std::endl;
    std::cout << "nodes: " << info.nodes << std::endl;
    std::cout << "sockets: " << info.sockets << std::endl;
    std::cout << "cores: " << info.cores << std::endl;
    std::cout << "threads: " << info.threads << std::endl;
}

static inline unsigned int getHostCPUCount() {
    virNodeInfo info;
    if (global_fixture::instance()->vir_helper_.getNodeInfo(&info) >= 0)
        return info.cpus;
    return 0;
}

BOOST_AUTO_TEST_CASE(testGetNodeCPUMap) {
    std::vector<unsigned char> cpumaps;
    unsigned int online = 0;
    int cpus = global_fixture::instance()->vir_helper_.getNodeCPUMap(
        cpumaps, online);
    BOOST_REQUIRE(cpus > 0);
    std::cout << "virNodeGetCPUMap return " << cpus <<
        ", and online cpu " << online << std::endl;
    std::cout << "NodeCPUMap:";
    for (const auto& ch : cpumaps) {
        std::cout << " " << std::bitset<8>((unsigned int)ch);
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(testDomainEmulatorPinInfo) {
    std::shared_ptr<virDomainImpl> domain =
        global_fixture::instance()->vir_helper_.openDomainByName(
            domain_name_.c_str());
    BOOST_REQUIRE(domain);
    // 假设 lscpu 中的 CPU(s) 是 32，需要 32 / 8 = 4 个字节
    unsigned int cpus = getHostCPUCount();
    BOOST_REQUIRE(cpus > 0);
    const int maplen = VIR_CPU_MAPLEN(cpus);
    BOOST_REQUIRE(maplen > 0);

    unsigned int flags1 = virDomainModificationImpact::VIR_DOMAIN_AFFECT_CURRENT;
    if (domain->isDomainActive())
        flags1 |= virDomainModificationImpact::VIR_DOMAIN_AFFECT_LIVE;
    unsigned int flags2 = flags1 | virDomainModificationImpact::VIR_DOMAIN_AFFECT_CONFIG;

    // Get
    {
        std::vector<unsigned char> cpumap;
        BOOST_REQUIRE(domain->getDomainEmulatorPinInfo(cpumap, maplen, flags1) >= 0);
        std::cout << "GetDomainEmulatorPinInfo:";
        for (const auto& ch : cpumap) {
            std::cout << " " << std::bitset<8>(ch);
        }
        std::cout << std::endl;
    }

    // Set
    {
        unsigned char* cpumap = new unsigned char[maplen];
        memset(cpumap, 0, maplen);
        VIR_USE_CPU(cpumap, 0);
        VIR_USE_CPU(cpumap, 16);
        int ret = domain->domainPinEmulator(cpumap, maplen, flags2);
        free(cpumap);
        std::cout << "SetDomainPinEmulator 0,16 return: " << ret << std::endl;
    }

    // Get
    {
        std::vector<unsigned char> cpumap;
        BOOST_REQUIRE(domain->getDomainEmulatorPinInfo(cpumap, maplen, flags1) >= 0);
        std::cout << "GetDomainEmulatorPinInfo:";
        for (const auto& ch : cpumap) {
            std::cout << " " << std::bitset<8>(ch);
        }
        std::cout << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(testLittleEndian) {
    unsigned int num = 3;
    std::cout << "num 3 bitset: " << std::bitset<8>(num) << std::endl;
    char bytes[] = "00000011";
    std::bitset<8> bs1(bytes);
    std::cout << bytes << " bitset: " << bs1 <<
        ", to_string: " << bs1.to_string() <<
        ", to_ulong: " << bs1.to_ulong() << std::endl;
    std::cout << bs1 << " [0] = " << bs1[0] << std::endl;
    std::cout << bs1 << " flip = " << bs1.flip() << std::endl;
}

BOOST_AUTO_TEST_CASE(testDomainVcpuPinInfo) {
    std::shared_ptr<virDomainImpl> domain =
        global_fixture::instance()->vir_helper_.openDomainByName(
            domain_name_.c_str());
    BOOST_REQUIRE(domain);
    // 假设 lscpu 中的 CPU(s) 是 32，需要 32 / 8 = 4 个字节
    unsigned int cpus = getHostCPUCount();
    BOOST_REQUIRE(cpus > 0);
    const int maplen = VIR_CPU_MAPLEN(cpus);
    BOOST_REQUIRE(maplen > 0);

    unsigned int flags1 = virDomainModificationImpact::VIR_DOMAIN_AFFECT_CURRENT;
    if (domain->isDomainActive())
        flags1 |= virDomainModificationImpact::VIR_DOMAIN_AFFECT_LIVE;
    unsigned int flags2 = flags1 | virDomainModificationImpact::VIR_DOMAIN_AFFECT_CONFIG;

    // Get
    {
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpuPinInfo(cpumaps, maplen, flags1) >= 0);
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>((unsigned int)cpumaps[i]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 1]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 2]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }

    // Set
    {
        // c++ 方案
        std::bitset<8> bs;
        bs.set(0, 1);
        std::vector<unsigned char> cpumap;
        cpumap.push_back(0);
        cpumap.push_back((unsigned char)bs.to_ulong());
        cpumap.push_back(0);
        cpumap.push_back((unsigned char)bs.to_ulong());
        std::cout << "cpumaps: ";
        for (const auto& ch : cpumap) std::cout << ch;
        std::cout << std::endl;
        std::cout << "reinterpret_cast<unsigned int>(cpumap) = " <<
            *reinterpret_cast<unsigned int*>(&cpumap[0]) << std::endl;
        // int ret = domain->domainPinVcpu(0, &cpumap[0], maplen);
        int ret = domain->domainPinVcpuFlags(0, &cpumap[0], maplen, flags2);
        std::cout << "domainPinVcpu 8,24 return " << ret << std::endl;
    }

    // Get
    {
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpuPinInfo(cpumaps, maplen, flags1) >= 0);
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>((unsigned int)cpumaps[i]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 1]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 2]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }

    // Set
    {
        // c 语言或者Libvirt提供的方案
        unsigned char* cpumap = new unsigned char[maplen];
        memset(cpumap, 0, maplen);
        VIR_USE_CPU(cpumap, 0);
        VIR_USE_CPU(cpumap, 16);
        // int ret = domain->domainPinVcpu(0, cpumap, maplen);
        int ret = domain->domainPinVcpuFlags(0, cpumap, maplen, flags2);
        free(cpumap);
        std::cout << "domainPinVcpu 0,16 return " << ret << std::endl;
    }

    // Get
    {
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpuPinInfo(cpumaps, maplen, flags1) >= 0);
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>((unsigned int)cpumaps[i]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 1]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 2]) << " " <<
                std::bitset<8>((unsigned int)cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }
}

BOOST_AUTO_TEST_CASE(testDomainVcpus) {
    std::shared_ptr<virDomainImpl> domain =
        global_fixture::instance()->vir_helper_.openDomainByName(
            domain_name_.c_str());
    BOOST_REQUIRE(domain);
    // 假设 lscpu 中的 CPU(s) 是 32，需要 32 / 8 = 4 个字节
    unsigned int cpus = getHostCPUCount();
    BOOST_REQUIRE(cpus > 0);
    const int maplen = VIR_CPU_MAPLEN(cpus);
    BOOST_REQUIRE(maplen > 0);

    unsigned int flags = virDomainModificationImpact::VIR_DOMAIN_AFFECT_CURRENT;
    flags |= virDomainModificationImpact::VIR_DOMAIN_AFFECT_CONFIG;
    if (domain->isDomainActive())
        flags |= virDomainModificationImpact::VIR_DOMAIN_AFFECT_LIVE;

    // Get
    {
        std::vector<virVcpuInfo> infos;
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpus(infos, cpumaps, maplen) >= 0);
        for (const auto& info : infos) {
            std::cout << info << std::endl;
        }
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>(cpumaps[i]) << " " <<
                std::bitset<8>(cpumaps[i + 1]) << " " <<
                std::bitset<8>(cpumaps[i + 2]) << " " <<
                std::bitset<8>(cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }

    // Set
    {// api 只能修改vcpu 0 并且参数传错了，暂时不知道怎么传参
        char* vcpumap = new char[maplen];
        memset(vcpumap, 0, maplen);
        VIR_USE_CPU(vcpumap, 1);
        VIR_USE_CPU(vcpumap, 17);
        int ret = domain->setDomainVcpu(vcpumap, 1, flags);
        free(vcpumap);
        std::cout << "setDomainVcpu 1,17 return " << ret << std::endl;
    }

    // Get
    {
        std::vector<virVcpuInfo> infos;
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpus(infos, cpumaps, maplen) >= 0);
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>(cpumaps[i]) << " " <<
                std::bitset<8>(cpumaps[i + 1]) << " " <<
                std::bitset<8>(cpumaps[i + 2]) << " " <<
                std::bitset<8>(cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }

    // Set
    {// api 只能修改vcpu 0 并且参数传错了，暂时不知道怎么传参
        int vcpu = 2;
        char* vcpumaps = new char[maplen * vcpu];
        memset(vcpumaps, 0, maplen * vcpu);
        for (int i = 0, numa1 = 0, numa2 = 16; i < vcpu; i++) {
            char* vcpumapi = VIR_GET_CPUMAP(vcpumaps, maplen, i);
            VIR_USE_CPU(vcpumapi, numa1);
            VIR_USE_CPU(vcpumapi, numa2);
            if (i % 2 == 1) {
                numa1++;
                numa2++;
            }
        }
        int ret = domain->setDomainVcpu(vcpumaps, 1, flags);
        free(vcpumaps);
        std::cout << "setDomainVcpu 0,16 0,16 return " << ret << std::endl;
    }

    // Get
    {
        std::vector<virVcpuInfo> infos;
        std::vector<unsigned char> cpumaps;
        BOOST_REQUIRE(domain->getDomainVcpus(infos, cpumaps, maplen) >= 0);
        for (int i = 0; i < cpumaps.size(); i += 4) {
            std::cout <<
                std::bitset<8>(cpumaps[i]) << " " <<
                std::bitset<8>(cpumaps[i + 1]) << " " <<
                std::bitset<8>(cpumaps[i + 2]) << " " <<
                std::bitset<8>(cpumaps[i + 3]);
            std::cout << std::endl;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
