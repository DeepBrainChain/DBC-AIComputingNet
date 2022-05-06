// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for virt nwfilter"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <iomanip>
#include <uuid/uuid.h>
#include <tinyxml2.h>
#include "vir_helper.h"

using namespace vir_helper;

static const char* qemu_url = "qemu+tcp://localhost:16509/system";
static const char* default_domain_name = "domain_test";

std::string createNWFilterXML(const std::string &filterName) {
    tinyxml2::XMLDocument doc;
    // <filter>
    tinyxml2::XMLElement *root = doc.NewElement("filter");
    root->SetAttribute("name", filterName.c_str());
    root->SetAttribute("chain", "root");
    doc.InsertEndChild(root);

    // filterref
    tinyxml2::XMLElement *filterref1 = doc.NewElement("filterref");
    filterref1->SetAttribute("filter", "clean-traffic");
    root->LinkEndChild(filterref1);

    // rule
    tinyxml2::XMLElement *rule1 = doc.NewElement("rule");
    rule1->SetAttribute("action", "accept");
    rule1->SetAttribute("direction", "in");
    tinyxml2::XMLElement *tcp1 = doc.NewElement("tcp");
    tcp1->SetAttribute("dstportstart", "22");
    rule1->LinkEndChild(tcp1);
    root->LinkEndChild(rule1);

    // <!-- 丢弃所有其他in流量 -->
    tinyxml2::XMLElement *rulein = doc.NewElement("rule");
    rulein->SetAttribute("action", "drop");
    rulein->SetAttribute("direction", "in");
    tinyxml2::XMLElement *allin = doc.NewElement("all");
    rulein->LinkEndChild(allin);
    root->LinkEndChild(rulein);

    // <!-- 允许所有out流量 -->
    tinyxml2::XMLElement *ruleout = doc.NewElement("rule");
    ruleout->SetAttribute("action", "accept");
    ruleout->SetAttribute("direction", "out");
    tinyxml2::XMLElement *allout = doc.NewElement("all");
    ruleout->LinkEndChild(allout);
    root->LinkEndChild(ruleout);

    // doc.SaveFile((filterName + ".xml").c_str());
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
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
        // BOOST_REQUIRE(vir_tool_.openConnect(qemu_url));
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
        // domain_name_ = default_domain_name;

        // using namespace boost::unit_test;
        // if (framework::master_test_suite().argc > 1) {
        //     domain_name_ = framework::master_test_suite().argv[1];
        // }
    }
    ~assign_fixture() {
        std::cout << "suit teardown\n";
    }

    // std::string domain_name_;
};

BOOST_AUTO_TEST_CASE(testConnect) {
    std::cout << "test connect" << std::endl;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.openConnect(qemu_url));
}

// BOOST_AUTO_TEST_SUITE(vir_nwfilter)
BOOST_FIXTURE_TEST_SUITE(vir_nwfilter, assign_fixture)

// BOOST_AUTO_TEST_CASE(testCommandParams) {
//     std::cout << "domain name = \"" << domain_name_ << "\"" << std::endl;
// }

BOOST_AUTO_TEST_CASE(testlistNWFilters) {
    int nums = global_fixture::instance()->vir_helper_.numOfNWFilters();
    BOOST_REQUIRE(nums > 0);
    std::cout << " virConnectNumOfNWFilters return " << nums << std::endl;
    std::vector<std::string> names;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.listNWFilters(names, nums) > 0);
    for (const auto& name : names) {
        std::cout << " " << name << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(testListAllNWFilters) {
    std::cout << " " << std::setw(36) << std::setfill(' ') << std::left << "UUID";
    std::cout << "   " << std::setw(25) << std::setfill(' ') << std::left << "Name" << std::endl;
    std::cout << std::setw(65) << std::setfill('-') << std::left << "-" << std::endl;
    std::vector<std::shared_ptr<virNWFilterImpl>> filters;
    BOOST_REQUIRE(global_fixture::instance()->vir_helper_.listAllNWFilters(filters, 0) > 0);
    for (const auto &filter : filters) {
        // unsigned char uuid[VIR_UUID_BUFLEN] = {0};
        // BOOST_CHECK(filter->getNWFilterUUID(uuid)  == 0);
        // char uuid_trans[256] = {0};
        // uuid_unparse(uuid, uuid_trans);
        // std::cout << " " << std::setw(36) << std::setfill(' ') << std::left << uuid_trans;
        char buf[VIR_UUID_STRING_BUFLEN] = {0};
        BOOST_CHECK(filter->getNWFilterUUIDString(buf) == 0);
        std::cout << " " << std::setw(36) << std::setfill(' ') << std::left << buf;
        std::string name;
        BOOST_CHECK(filter->getNWFilterName(name) == 0);
        std::cout << "   " << std::setw(25) << std::setfill(' ') << std::left << name << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(testDefineNWFilter) {
    std::string nwfilterXMLDesc = createNWFilterXML("virt-nwfilter-test");
    std::shared_ptr<virNWFilterImpl> nwfilter = global_fixture::instance()->vir_helper_.defineNWFilter(nwfilterXMLDesc.c_str());
    BOOST_REQUIRE(nwfilter);
    char buf[VIR_UUID_STRING_BUFLEN] = {0};
    BOOST_CHECK(nwfilter->getNWFilterUUIDString(buf) == 0);
    std::cout << " " << std::setw(36) << std::setfill(' ') << std::left << buf;
    std::string name;
    BOOST_CHECK(nwfilter->getNWFilterName(name) == 0);
    std::cout << "   " << std::setw(25) << std::setfill(' ') << std::left << name << std::endl;
    std::string xmlDesc = nwfilter->getNWFilterXMLDesc();
    BOOST_REQUIRE(!xmlDesc.empty());
    std::cout << xmlDesc << std::endl;
}

BOOST_AUTO_TEST_CASE(testUndefineNWFilter) {
    std::shared_ptr<virNWFilterImpl> nwfilter = global_fixture::instance()->vir_helper_.openNWFilterByName("virt-nwfilter-test");
    BOOST_REQUIRE(nwfilter);
    char buf[VIR_UUID_STRING_BUFLEN] = {0};
    BOOST_CHECK(nwfilter->getNWFilterUUIDString(buf) == 0);
    std::cout << " " << std::setw(36) << std::setfill(' ') << std::left << buf;
    std::string name;
    BOOST_CHECK(nwfilter->getNWFilterName(name) == 0);
    std::cout << "   " << std::setw(25) << std::setfill(' ') << std::left << name << std::endl;
    BOOST_REQUIRE(nwfilter->undefineNWFilter() == 0);
    std::cout << " undefine nwfilter " << name << " successfully" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
