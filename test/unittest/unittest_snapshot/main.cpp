// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for vir snapshot"
#include <boost/test/unit_test.hpp>
#include "virImpl.h"
#include <iostream>
#include "tinyxml2.h"

static const char* qemu_url = "qemu+tcp://localhost:16509/system";
static const char* default_domain_name = "domain_test";
static const char* default_snapshot_name = "snap1";

std::string createSnapshotXML(const std::string &snapName) {
    tinyxml2::XMLDocument doc;
    // <domainsnapshot>
    tinyxml2::XMLElement *root = doc.NewElement("domainsnapshot");
    doc.InsertEndChild(root);

    // description
    tinyxml2::XMLElement *desc = doc.NewElement("description");
    desc->SetText("Snapshot of OS install and updates");
    root->LinkEndChild(desc);

    // name
    tinyxml2::XMLElement *name = doc.NewElement("name");
    name->SetText(snapName.c_str());
    root->LinkEndChild(name);

    // disks
    tinyxml2::XMLElement *disks = doc.NewElement("disks");
    // system disk
    tinyxml2::XMLElement *disk1 = doc.NewElement("disk");
    disk1->SetAttribute("name", "hda");
    disk1->SetAttribute("snapshot", "external");
    tinyxml2::XMLElement *disk1driver = doc.NewElement("driver");
    disk1driver->SetAttribute("type", "qcow2");
    disk1->LinkEndChild(disk1driver);
    disks->LinkEndChild(disk1);
    // data disk
    tinyxml2::XMLElement *disk2 = doc.NewElement("disk");
    disk2->SetAttribute("name", "vda");
    disk2->SetAttribute("snapshot", "no");
    disks->LinkEndChild(disk2);
    root->LinkEndChild(disks);

    doc.SaveFile((snapName + ".xml").c_str());
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
        std::cout << "command format: snapshot_test -- [domain name] [snapshot name]" << std::endl;
        std::cout << "开始准备测试数据------->" << std::endl;
        // BOOST_REQUIRE(vir_tool_.openConnect(qemu_url));
    }
    virtual~global_fixture() {
        std::cout << "清理测试环境<---------" << std::endl;
    }
    virTool vir_tool_;
};

BOOST_GLOBAL_FIXTURE(global_fixture);

struct assign_fixture
{
    assign_fixture() {
        std::cout << "suit setup\n";
        domain_name_ = default_domain_name;
        snapshot_name_ = default_snapshot_name;

        using namespace boost::unit_test;
        if (framework::master_test_suite().argc > 1) {
            domain_name_ = framework::master_test_suite().argv[1];
        }
        if (framework::master_test_suite().argc > 2) {
            snapshot_name_ = framework::master_test_suite().argv[2];
        }
    }
    ~assign_fixture() {
        std::cout << "suit teardown\n";
    }

    std::string domain_name_;
    std::string snapshot_name_;
};

BOOST_AUTO_TEST_CASE(testConnect) {
    std::cout << "test connect" << std::endl;
    BOOST_REQUIRE(global_fixture::instance()->vir_tool_.openConnect(qemu_url));
}

// BOOST_AUTO_TEST_SUITE(vir_snapshot)
BOOST_FIXTURE_TEST_SUITE(vir_snapshot, assign_fixture)

BOOST_AUTO_TEST_CASE(testCommandParams) {
    std::cout << "domain name = \"" << domain_name_ << "\"" << std::endl;
    std::cout << "snapshot name = \"" << snapshot_name_ << "\"" << std::endl;
}

BOOST_AUTO_TEST_CASE(testCreateSnapshot) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_tool_.openDomain(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    std::string snapXMLDesc = createSnapshotXML(snapshot_name_.c_str());
    BOOST_CHECK(!snapXMLDesc.empty());
    unsigned int createFlags = VIR_DOMAIN_SNAPSHOT_CREATE_HALT | VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY |
                               VIR_DOMAIN_SNAPSHOT_CREATE_QUIESCE | VIR_DOMAIN_SNAPSHOT_CREATE_ATOMIC;
    std::shared_ptr<virDomainSnapshotImpl> snapshot = domain->createSnapshot(snapXMLDesc.c_str(), createFlags);
    BOOST_REQUIRE(snapshot);
    std::string xmlDesc;
    BOOST_CHECK(snapshot->getSnapshotXMLDesc(xmlDesc) != -1);
    // std::cout << "snapshot xml desc: " << xmlDesc << std::endl;
    std::cout << "create snapshot successful" << std::endl;
}

BOOST_AUTO_TEST_CASE(testListSnapshot) {
    std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_tool_.openDomain(domain_name_.c_str());
    BOOST_REQUIRE(domain);
    // int32_t nums = domain->getSnapshotNums(1 << 10);
    // std::cout << domain_name_ << " has " << nums << " snapshots" << std::endl;
    // BOOST_REQUIRE_MESSAGE(nums > 0, "domain has no snapshot");
    // std::vector<std::string> names;
    // BOOST_CHECK(domain->listSnapshotNames(names, nums, 1 << 10) >= 0);
    // for (int i = 0; i < names.size(); i++) {
    //     std::cout << domain_name_ << " snapshot names[" << i << "]=" << names[i] << std::endl;
    // }
    std::vector<std::shared_ptr<virDomainSnapshotImpl>> snaps;
    BOOST_REQUIRE(domain->listAllSnapshots(snaps, 1 << 10) >= 0);
    for (const auto& snap : snaps) {
        DomainSnapshotInfo dsInfo;
        BOOST_CHECK(snap->getSnapshotInfo(dsInfo) == 0);
        std::cout << "get snap [" << dsInfo.name << "] disk details here:" << std::endl;
        for (int i = 0; i < dsInfo.disks.size(); i++) {
            std::cout << "disk name=" << dsInfo.disks[i].name << ", snapshot=" << dsInfo.disks[i].snapshot
                      << ", driver_type=" << dsInfo.disks[i].driver_type << ", source_file=" << dsInfo.disks[i].source_file;
            std::cout << std::endl;
        }
    }
}

// error: unsupported configuration: revert to external snapshot not supported yet
// BOOST_AUTO_TEST_CASE(testRevertSnapshot) {
//     std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_tool_.openDomain(domain_name_.c_str());
//     BOOST_REQUIRE(domain);
//     std::shared_ptr<virDomainSnapshotImpl> snapshot = domain->getSnapshotByName("snap2");
//     BOOST_REQUIRE(snapshot);
//     unsigned int revertFlags = VIR_DOMAIN_SNAPSHOT_REVERT_RUNNING | VIR_DOMAIN_SNAPSHOT_REVERT_FORCE;
//     BOOST_CHECK(snapshot->revertDomainToThisSnapshot(revertFlags) >= 0);
// }

// need see blockcommit and blockpull before delete a snapshot
// BOOST_AUTO_TEST_CASE(testDeleteAllSnapshot) {
//     std::shared_ptr<virDomainImpl> domain = global_fixture::instance()->vir_tool_.openDomain(domain_name_.c_str());
//     BOOST_REQUIRE(domain);
//     std::vector<std::shared_ptr<virDomainSnapshotImpl>> snaps;
//     // BOOST_REQUIRE(domain->listAllSnapshots(snaps, VIR_DOMAIN_SNAPSHOT_LIST_ROOTS) >= 0);
//     // for (const auto& snap : snaps) {
//     //     snap->deleteSnapshot(VIR_DOMAIN_SNAPSHOT_DELETE_CHILDREN);
//     // }
//     BOOST_REQUIRE(domain->listAllSnapshots(snaps, 1 << 10) >= 0);
//     for (auto snap = snaps.rbegin(); snap != snaps.rend(); snap++) {
//         (*snap)->deleteSnapshot(VIR_DOMAIN_SNAPSHOT_DELETE_CHILDREN | VIR_DOMAIN_SNAPSHOT_DELETE_METADATA_ONLY);
//     }
//     {
//         // clear vector
//         std::vector<std::shared_ptr<virDomainSnapshotImpl>> tmp;
//         snaps.swap(tmp);
//     }
//     std::cout << "list all snapshot after delete" << std::endl;
//     BOOST_REQUIRE(domain->listAllSnapshots(snaps, 1 << 10) >= 0);
// }

BOOST_AUTO_TEST_SUITE_END()
