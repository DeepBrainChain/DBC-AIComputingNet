// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for virt connect"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <iomanip>
#include "vir_helper.h"

using namespace vir_helper;

static const char* qemu_url = "qemu+tcp://localhost:16509/system";


std::string rtrim(std::string &s, char c) {
    if (s.size() == 0) {
        return "";
    }

    int i = ((int) s.length()) - 1;

    for (; i >= 0; i--) {
        if (s[i] != c) {
            break;
        }
    }

    return s.substr(0, i + 1);
}

std::string run_shell(const std::string& cmd, const char* modes) {
    if (cmd.empty()) return "";

    std::string ret;
    std::string strcmd = cmd + " 2>&1";
    FILE * fp = nullptr;
    char buffer[1024] = {0};
    fp = popen(strcmd.c_str(), modes);
    if (fp != nullptr) {
        while(fgets(buffer, sizeof(buffer), fp)) {
            ret += std::string(buffer);
        }
        pclose(fp);
    } else {
        ret = std::string("run_shell failed! ") + strcmd + std::string(" ") + std::to_string(errno) + ":" + strerror(errno);
    }

    return rtrim(ret, '\n');
}

struct assign_fixture
{
    assign_fixture() {
        std::cout << "suit setup\n";
    }
    ~assign_fixture() {
        std::cout << "suit teardown\n";
    }
};

// BOOST_AUTO_TEST_SUITE(vir_connect)
BOOST_FIXTURE_TEST_SUITE(vir_connect, assign_fixture)

BOOST_AUTO_TEST_CASE(testCase1) {
    virHelper vir_helper(true);
    vir_helper.openConnect(qemu_url);
    unsigned long libVer = 0;
    BOOST_REQUIRE(vir_helper.getVersion(&libVer, NULL, NULL) == 0);
    std::cout << "getVersion libVer = " << libVer << std::endl;
    libVer = 0;
    BOOST_REQUIRE(vir_helper.getConnectVersion(&libVer) == 0);
    std::cout << "getConnectVersion hypervisor version = " << libVer << std::endl;
    libVer = 0;
    BOOST_REQUIRE(vir_helper.getConnectLibVersion(&libVer) == 0);
    std::cout << "getConnectLibVersion libvirt version = " << libVer << std::endl;
    sleep(1);
}

BOOST_AUTO_TEST_CASE(testCase2) {
    virHelper vir_helper(true);
    vir_helper.openConnect(qemu_url);

    sleep(3);

    std::cout << "connect is alive: " << vir_helper.isConnectAlive() << std::endl;
    std::cout << "connect is encrypted: " << vir_helper.isConnectEncrypted() << std::endl;
    std::cout << "connect is secure: " << vir_helper.isConnectSecure() << std::endl;

    std::string cmd, cmd_ret;
    cmd = "sudo systemctl stop libvirtd";
    cmd_ret = run_shell(cmd, "r");
    std::cout << "run shell " << cmd << " return " << cmd_ret << std::endl;

    sleep(3);

    // cmd = "systemctl status libvirtd";
    // cmd_ret = run_shell(cmd, "r");
    // std::cout << "run shell " << cmd << " return " << std::endl << cmd_ret << std::endl;

    // std::cout << "connect is alive: " << vir_helper.isConnectAlive() << std::endl;
    // std::cout << "connect is encrypted: " << vir_helper.isConnectEncrypted() << std::endl;
    // std::cout << "connect is secure: " << vir_helper.isConnectSecure() << std::endl;

    cmd = "sudo systemctl start libvirtd";
    cmd_ret = run_shell(cmd, "r");
    std::cout << "run shell " << cmd << " return " << cmd_ret << std::endl;
    sleep(3);
    // cmd = "systemctl status libvirtd";
    // cmd_ret = run_shell(cmd, "r");
    // std::cout << "run shell " << cmd << " return " << std::endl << cmd_ret << std::endl;

    BOOST_REQUIRE_MESSAGE(vir_helper.openConnect(qemu_url), "reconnect failed");
    std::cout << "reconnect to libvirt success" << std::endl;

    std::cout << "connect is alive: " << vir_helper.isConnectAlive() << std::endl;
    std::cout << "connect is encrypted: " << vir_helper.isConnectEncrypted() << std::endl;
    std::cout << "connect is secure: " << vir_helper.isConnectSecure() << std::endl;

    sleep(2);
}

BOOST_AUTO_TEST_SUITE_END()
