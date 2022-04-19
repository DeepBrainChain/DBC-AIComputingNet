#include "VxlanManager.h"
#include <boost/asio/ip/address.hpp>
#include "log/log.h"
#include "network/protocol/thrift_binary.h"
#include "server/server.h"
#include "service/peer_request_service/p2p_lan_service.h"

// VXLAN网络标识VNI（VXLAN Network Identifier），由24比特组成，支持多达16M的VXLAN段，有效得解决了云计算中海量租户隔离的问题。
#define VNI_MAX 16777216

#define NATIVE_FLAGS_DEVICE        (1 << 0)
#define NATIVE_FLAGS_DHCPSERVER    (1 << 1)

inline std::string random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                        //    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[static_cast<size_t>(rand()) % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

inline bool check_network_name(const std::string& name) {
    if (name.length() < 6 || name.length() > 10) return false;
    for (const auto & ch : name) {
        if (!isalnum(ch)) return false;
    }
    return true;
}

struct in_addr_helper {
union {
    struct { u_char s_b1,s_b2,s_b3,s_b4;} S_un_b;
    struct { u_short s_w1,s_w2;} S_un_w;
    u_long S_addr;//成员s_addr为长整形结构
} S_un;
};

class ipRangeHelper {
public:
    ipRangeHelper(const std::string &ip, unsigned int bits) {
        inet_pton(AF_INET, ip.c_str(), &ip_);
        ip_.s_addr = ntohl(ip_.s_addr);
        bits_ = bits;
    }

    std::string getIpStart() const {
        struct in_addr ip;
        ip.s_addr = (ip_.s_addr & netMask()) + 1;
        ip.s_addr = htonl(ip.s_addr);
        char str[16] = {0};//INET_ADDRSTRLEN
        return inet_ntop(AF_INET, &ip, str, sizeof(str));
    }

    std::string getIpEnd() const {
        struct in_addr ip;
        ip.s_addr = (ip_.s_addr | ~netMask()) - 1;
        ip.s_addr = htonl(ip.s_addr);
        char str[16] = {0};//INET_ADDRSTRLEN
        return inet_ntop(AF_INET, &ip, str, sizeof(str));
    }

    std::string getSubnetMask() const {
        struct in_addr ip;
        ip.s_addr = htonl(netMask());
        char str[16] = {0};//INET_ADDRSTRLEN
        return inet_ntop(AF_INET, &ip, str, sizeof(str));
    }

    int32_t getMaxDHCPLeases() const {
        u_long maximum = (0xFFFFFFFF >> bits_);
        return maximum - 1;
    }

    // 判断两个网络是否有交集
    // 有A、B两地址，掩码分别为x,y，分两步判断
    // 1、算A和B是否在同一网段时，分别都用A的掩码算，如果得到的两个网络地址一样，则说明A和B在同一网段（可以理解为A到B方向）
    // 2、算B和A是否在同一网段时，分别都用B的掩码算，如果的到的两个网络地址一致，说明B和A在同一网段（可以理解为B到A方向）
    bool hasIntersection(const ipRangeHelper &other) const {
        in_addr_t mask1 = netMask();
        if ((ip_.s_addr & mask1) == (other.ip_.s_addr & mask1)) return true;
        in_addr_t mask2 = other.netMask();
        if ((ip_.s_addr & mask2) == (other.ip_.s_addr & mask2)) return true;
        return false;
    }

private:
    int32_t netMask() const {
        return (0xFFFFFFFF << (32 - bits_));
    }

private:
    struct in_addr ip_;
    unsigned int bits_;
};

VxlanManager::VxlanManager() {

}

ERRCODE VxlanManager::Init() {
    if (ERR_SUCCESS != InitDb()) {
        LOG_ERROR << "init_db error";
        return E_DEFAULT;
    }

    if (ERR_SUCCESS != LoadNetworkInfoFromDb()) {
        LOG_ERROR << "load network info from db error";
        return E_DEFAULT;
    }
    return ERR_SUCCESS;
}

void VxlanManager::Exit() {
    // RwMutex::ReadLock rlock(m_mtx);
    RwMutex::WriteLock wlock(m_mtx);
    for (auto& iter : m_networks) {
        if (iter.second->machineId != ConfManager::instance().GetNodeId())
            continue;
        if (iter.second->ipCidr.empty())
            continue;
        StopDhcpServer(iter.second->bridgeName, iter.second->vxlanName);
        p2p_lan_service::instance().send_network_move_request(iter.second);
        iter.second->__set_machineId(std::string());
        iter.second->__set_nativeFlags(iter.second->nativeFlags & ~NATIVE_FLAGS_DHCPSERVER);
        UpdateNetworkDb(iter.second);
    }
}

FResult VxlanManager::CreateVxlanDevice(const std::string &bridgeName, const std::string &vxlanName, const std::string &vxlanVni) {
    FResult fret = {ERR_ERROR, "parse shell result error"};
    bfs::path shell_path = EnvManager::instance().get_shell_path();
    shell_path /= "network/create_bridge.sh";
    std::string cmd = shell_path.generic_string() + " " + bridgeName + " " + vxlanName + " " + vxlanVni;
    std::string cmdRet = run_shell(cmd);
    LOG_INFO << "run shell $(" << cmd << ") result: " << cmdRet;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(cmdRet.c_str());
    if (!ok) return fret;
    if (doc.HasMember("errcode") && doc["errcode"].IsInt()) {
        fret.errcode = doc["errcode"].GetInt();
    }
    if (doc.HasMember("message") && doc["message"].IsString()) {
        fret.errmsg = doc["message"].GetString();
    }
    return fret;
}

void VxlanManager::DeleteVxlanDevice(const std::string &bridgeName, const std::string &vxlanName) {
    bfs::path shell_path = EnvManager::instance().get_shell_path() / "network/delete_bridge.sh";
    std::string cmd = shell_path.generic_string() + " " + bridgeName + " " + vxlanName;
    std::string cmdRet = run_shell(cmd);
    LOG_INFO << "run shell $(" << cmd << ") result: " << cmdRet;
}

FResult VxlanManager::StartDhcpServer(const std::string &bridgeName, const std::string &vxlanName, const std::string &dhcpName,
        const std::string &netMask, const std::string &ipStart, const std::string &ipEnd, const std::string &ipLeaseNum) {
    FResult fret = {ERR_ERROR, "parse shell result error"};
    bfs::path shell_path = EnvManager::instance().get_shell_path();
    shell_path /= "network/start_dhcp.sh";
    std::string cmd = shell_path.generic_string() + " " + bridgeName + " " + vxlanName + " " + dhcpName;
    cmd.append(" " + netMask + " " + ipStart + " " + ipEnd + " " + ipLeaseNum);
    std::string cmdRet = run_shell(cmd);
    LOG_INFO << "run shell $(" << cmd << ") result: " << cmdRet;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(cmdRet.c_str());
    if (!ok) return fret;
    if (doc.HasMember("errcode") && doc["errcode"].IsInt()) {
        fret.errcode = doc["errcode"].GetInt();
    }
    if (doc.HasMember("message") && doc["message"].IsString()) {
        fret.errmsg = doc["message"].GetString();
    }
    return fret;
}

void VxlanManager::StopDhcpServer(const std::string &bridgeName, const std::string &vxlanName) {
    bfs::path shell_path = EnvManager::instance().get_shell_path() / "network/stop_dhcp.sh";
    std::string cmd = shell_path.generic_string() + " " + bridgeName + " " + vxlanName;
    std::string cmdRet = run_shell(cmd);
    LOG_INFO << "run shell $(" << cmd << ") result: " << cmdRet;
}

FResult VxlanManager::CreateNetworkServer(const std::string &networkName, const std::string &ipCidr, const std::string &wallet) {
    std::string random = random_string(10);

    if (networkName.empty())
        return FResult(ERR_ERROR, "network name can not be empty");
    if (!check_network_name(networkName))
        return FResult(ERR_ERROR, "network name requires a combination of 6 to 10 letters or numbers");
    if (GetNetwork(networkName))
        return FResult(ERR_ERROR, "network name already existed");

    std::string vni = RandomVni();
    if (vni.empty())
        return FResult(ERR_ERROR, "network vni is full");
    if (ipCidr.empty())
        return FResult(ERR_ERROR, "ip cidr can not be empty");

    std::shared_ptr<dbc::networkInfo> info = std::make_shared<dbc::networkInfo>();
    info->__set_networkId(networkName);
    info->__set_bridgeName("br" + networkName);
    info->__set_vxlanName("vx" + networkName);
    info->__set_vxlanVni(vni);

    if (ipCidr.find("192.168.122.") != std::string::npos)
        return FResult(ERR_ERROR, "ip cidr already exist");
    std::vector<std::string> vecSplit = util::split(ipCidr, "/");
    if (vecSplit.size() != 2)
        return FResult(ERR_ERROR, "invalid ip cidr");
    boost::asio::ip::address addr = boost::asio::ip::address::from_string(vecSplit[0]);
    if (!addr.is_v4())
        return FResult(ERR_ERROR, "invalid ip cidr");
    // if (CheckIpCidr(ipCidr))
    //     return FResult(ERR_ERROR, "ip cidr already exist");

    ipRangeHelper ipHelper(vecSplit[0], atoi(vecSplit[1].c_str()));

    info->__set_ipCidr(ipCidr);
    info->__set_ipStart(ipHelper.getIpStart());
    info->__set_ipEnd(ipHelper.getIpEnd());
    info->__set_dhcpInterface("tap" + random);

    FResult fret = CreateVxlanDevice(info->bridgeName, info->vxlanName, info->vxlanVni);
    if (fret.errcode != 0) {
        DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        return fret;
    }

    fret = StartDhcpServer(info->bridgeName, info->vxlanName, info->dhcpInterface,
        ipHelper.getSubnetMask(), info->ipStart, info->ipEnd, std::to_string(ipHelper.getMaxDHCPLeases()));
    if (fret.errcode != 0) {
        DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        StopDhcpServer(info->bridgeName, info->vxlanName);
        return fret;
    }

    info->__set_machineId(ConfManager::instance().GetNodeId());
    info->__set_rentWallet(wallet);
    std::vector<std::string> members;
    info->__set_members(members);
    info->__set_lastUseTime(time(NULL));
    info->__set_nativeFlags(NATIVE_FLAGS_DEVICE | NATIVE_FLAGS_DHCPSERVER);
    info->__set_lastUpdateTime(time(NULL));

    if (!UpdateNetworkDb(info)) {
        DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        StopDhcpServer(info->bridgeName, info->vxlanName);
        return FResult(ERR_ERROR, "write network db failed");
    }

    {
        RwMutex::WriteLock wlock(m_mtx);
        m_networks[networkName] = info;
    }

    p2p_lan_service::instance().send_network_create_request(info);

    LOG_INFO << "create network server: " << networkName << ", ip range: " << ipCidr << " successfully";
    return FResultOk;
}

FResult VxlanManager::CreateNetworkClient(const std::string &networkName) {
    std::shared_ptr<dbc::networkInfo> info = GetNetwork(networkName);
    if (!info) return FResult(ERR_ERROR, "can not find network info");
    if (info->nativeFlags & NATIVE_FLAGS_DEVICE) return FResultOk;
    if (p2p_lan_service::instance().is_same_host(info->machineId)) return FResultOk;
    if (info->bridgeName.empty() || info->vxlanName.empty() || info->vxlanVni.empty())
        return FResult(ERR_ERROR, "invalid network info");
 
    FResult fret = CreateVxlanDevice(info->bridgeName, info->vxlanName, info->vxlanVni);
    if (fret.errcode != 0) {
        DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        return fret;
    }

    if (!UpdateNetworkDb(info)) {
        DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        return FResult(ERR_ERROR, "update network db failed");
    }

    {
        RwMutex::WriteLock wlock(m_mtx);
        info->__set_nativeFlags(info->nativeFlags | NATIVE_FLAGS_DEVICE);
        m_networks[networkName] = info;
    }

    LOG_INFO << "create network client: " << networkName << " successfully";
    return FResultOk;
}

FResult VxlanManager::AddNetworkFromMulticast(std::shared_ptr<dbc::networkInfo> info) {
    if (info->machineId == ConfManager::instance().GetNodeId()) return FResultOk;
    {
        std::shared_ptr<dbc::networkInfo> old = GetNetwork(info->networkId);
        if (old) {
            int64_t time = info->lastUpdateTime;
            info->__set_nativeFlags(old->nativeFlags);
            info->__set_lastUpdateTime(old->lastUpdateTime);
            if (*info == *old) {
                RwMutex::WriteLock wlock(m_mtx);
                old->__set_lastUpdateTime(time);
                return FResultOk;
            }
            info->__set_lastUpdateTime(time);
        }
    }
    
    UpdateNetworkDb(info);

    LOG_INFO << "add network: " << info->networkId << " from multicast successfully";
    RwMutex::WriteLock wlock(m_mtx);
    m_networks[info->networkId] = info;
    return FResultOk;
}

FResult VxlanManager::DeleteNetwork(const std::string &networkName, const std::string &wallet) {
    {
        RwMutex::WriteLock wlock(m_mtx);
        auto iter = m_networks.find(networkName);
        if (iter != m_networks.end()) {
            if (iter->second->rentWallet != wallet) {
                return FResult(ERR_ERROR, "wallet error, you are not the owner of the network");
            }
            if (!iter->second->members.empty()) {
                return FResult(ERR_ERROR, "network is in used, please delete task in the network first");
            }
            StopDhcpServer(iter->second->bridgeName, iter->second->vxlanName);
            DeleteVxlanDevice(iter->second->bridgeName, iter->second->vxlanName);
            m_networks.erase(iter);
        } else {
            return FResult(ERR_ERROR, "network name not exist");
        }
    }

    DeleteNetworkDb(networkName);

    p2p_lan_service::instance().send_network_delete_request(networkName);

    LOG_INFO << "delete network: " << networkName << " successfully";
    return FResultOk;
}

void VxlanManager::DeleteNetworkFromMulticast(const std::string &networkName, const std::string &machineId) {
    {
        RwMutex::WriteLock wlock(m_mtx);
        auto iter = m_networks.find(networkName);
        if (iter != m_networks.end()) {
            StopDhcpServer(iter->second->bridgeName, iter->second->vxlanName);
            DeleteVxlanDevice(iter->second->bridgeName, iter->second->vxlanName);
            m_networks.erase(iter);
        } else {
            return;
        }
    }

    DeleteNetworkDb(networkName);
    LOG_INFO << "delete network: " << networkName << " from multicast successfully";
}

const std::map<std::string, std::shared_ptr<dbc::networkInfo> > VxlanManager::GetNetworks() const {
    RwMutex::ReadLock rlock(m_mtx);
    return m_networks;
}

std::shared_ptr<dbc::networkInfo> VxlanManager::GetNetwork(const std::string &networkName) const {
    if (networkName.empty()) return nullptr;
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_networks.find(networkName);
    if (it != m_networks.end()) return it->second;
    return nullptr;
}

FResult VxlanManager::MoveNetwork(const std::string &networkName, const std::string &newMachineId) {
    std::shared_ptr<dbc::networkInfo> info = GetNetwork(networkName);
    if (!info) return FResult(ERR_ERROR, "network name not exist");

    if (newMachineId != ConfManager::instance().GetNodeId())
        return FResult(ERR_ERROR, "not my machine id");
    
    bool isSameHost = p2p_lan_service::instance().is_same_host(info->machineId);

    if (!isSameHost && (info->nativeFlags & NATIVE_FLAGS_DEVICE) == 0) {
        FResult fret = CreateVxlanDevice(info->bridgeName, info->vxlanName, info->vxlanVni);
        if (fret.errcode != 0) {
            DeleteVxlanDevice(info->bridgeName, info->vxlanName);
            return fret;
        }
    }

    std::vector<std::string> vecSplit = util::split(info->ipCidr, "/");
    if (vecSplit.size() != 2) return FResult(ERR_ERROR, "invalid ip cidr");
    boost::asio::ip::address addr = boost::asio::ip::address::from_string(vecSplit[0]);
    if (!addr.is_v4()) return FResult(ERR_ERROR, "invalid ip cidr");

    ipRangeHelper ipHelper(vecSplit[0], atoi(vecSplit[1].c_str()));
    FResult fret = StartDhcpServer(info->bridgeName, info->vxlanName, info->dhcpInterface,
        ipHelper.getSubnetMask(), info->ipStart, info->ipEnd, std::to_string(ipHelper.getMaxDHCPLeases()));
    if (fret.errcode != 0) {
        if (!isSameHost && (info->nativeFlags & NATIVE_FLAGS_DEVICE) == 0)
            DeleteVxlanDevice(info->bridgeName, info->vxlanName);
        StopDhcpServer(info->bridgeName, info->vxlanName);
        return fret;
    }

    std::string oldMachineId = info->machineId;
    {
        RwMutex::WriteLock wlock(m_mtx);
        info->__set_nativeFlags(info->nativeFlags | NATIVE_FLAGS_DEVICE | NATIVE_FLAGS_DHCPSERVER);
        info->__set_machineId(newMachineId);
        m_networks[networkName] = info;
    }

    p2p_lan_service::instance().send_network_move_ack_request(networkName, newMachineId);

    UpdateNetworkDb(info);

    LOG_INFO << "network: " << networkName << " move from " << oldMachineId << " to " << newMachineId;
    return FResultOk;
}

void VxlanManager::MoveNetworkAck(const std::string &networkName, const std::string &newMachineId) {
    std::shared_ptr<dbc::networkInfo> info = GetNetwork(networkName);
    if (!info) return ;

    if (newMachineId == ConfManager::instance().GetNodeId()) return ;
    
    bool isSameHost = p2p_lan_service::instance().is_same_host(newMachineId);

    {
        if (!isSameHost/* && (info->nativeFlags & NATIVE_FLAGS_DHCPSERVER) != 0*/) {
            StopDhcpServer(info->bridgeName, info->vxlanName);
        }
        RwMutex::WriteLock wlock(m_mtx);
        info->__set_machineId(newMachineId);
        info->__set_nativeFlags(info->nativeFlags & ~NATIVE_FLAGS_DHCPSERVER);
        m_networks[networkName] = info;
    }

    UpdateNetworkDb(info);

    LOG_INFO << "network: " << networkName << " move to " << newMachineId;
}

void VxlanManager::JoinNetwork(const std::string &networkName, const std::string &taskId) {
    std::shared_ptr<dbc::networkInfo> info = GetNetwork(networkName);
    if (!info) return;
    // if (info->machineId != ConfManager::instance().GetNodeId()) return;

    {
        RwMutex::WriteLock wlock(m_mtx);
        info->members.push_back(taskId);
        info->__set_lastUseTime(time(NULL));
        m_networks[networkName] = info;
    }

    UpdateNetworkDb(info);
    LOG_INFO << "task: " << taskId << " join network: " << networkName;
}

void VxlanManager::LeaveNetwork(const std::string &networkName, const std::string &taskId) {
    std::shared_ptr<dbc::networkInfo> info = GetNetwork(networkName);
    if (!info) return;
    // if (info->machineId != ConfManager::instance().GetNodeId()) return;

    {
        RwMutex::WriteLock wlock(m_mtx);
        for (auto iter = info->members.begin(); iter != info->members.end();) {
            if (*iter == taskId)
                iter = info->members.erase(iter);
            else
                iter++;
        }
        info->__set_lastUseTime(time(NULL));
        m_networks[networkName] = info;
    }

    UpdateNetworkDb(info);
    LOG_INFO << "task: " << taskId << " leave network: " << networkName;
}

void VxlanManager::ClearEmptyNetwork() {
    std::vector<std::pair<std::string, std::string>> networks;
    {
        int64_t cur_time = time(NULL);
        RwMutex::ReadLock rlock(m_mtx);
        for (const auto& iter : m_networks) {
            if (iter.second->machineId != ConfManager::instance().GetNodeId())
                continue;
            if (!iter.second->members.empty())
                continue;
            if (difftime(cur_time, iter.second->lastUseTime) > 259200)// 60s * 60 * 24 * 3
                networks.push_back(std::make_pair(iter.first, iter.second->rentWallet));
        }
    }
    for (const auto& iter : networks) {
        LOG_INFO << "Network " << iter.first <<
            " has not been used by the virtual machine for a long time and will be deleted";
        DeleteNetwork(iter.first, iter.second);
    }
}

void VxlanManager::ClearExpiredNetwork() {
    std::vector<std::string> networks;
    {
        int64_t cur_time = time(NULL);
        RwMutex::ReadLock rlock(m_mtx);
        for (const auto& iter : m_networks) {
            if (iter.second->machineId == ConfManager::instance().GetNodeId())
                continue;
            if (!iter.second->members.empty())
                continue;
            if (difftime(cur_time, iter.second->lastUpdateTime) > 259200)// 60s * 60 * 24 * 3
                networks.push_back(iter.first);
        }
    }
    for (const auto& iter : networks) {
        LOG_INFO << "Network " << iter <<
            " has not been updated for a long time and will be deleted";
        DeleteNetworkFromMulticast(iter, "");
    }
}

int32_t VxlanManager::InitDb() {
    leveldb::DB *db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    try {
        bfs::path db_path = EnvManager::instance().get_db_path();
        if (false == bfs::exists(db_path)) {
            LOG_ERROR << "db directory path does not exist and create db directory";
            bfs::create_directory(db_path);
        }

        if (false == bfs::is_directory(db_path)) {
            LOG_ERROR << "db directory path does not exist and exit";
            return E_DEFAULT;
        }

        db_path /= bfs::path("network.db");
        leveldb::Status status = leveldb::DB::Open(options, db_path.generic_string(), &db);
        if (false == status.ok()) {
            LOG_ERROR << "init vxlan network db error: " << status.ToString();
            return E_DEFAULT;
        }

        m_db.reset(db);
    }
    catch (const std::exception &e) {
        LOG_ERROR << "create vxlan network db error: " << e.what();
        return E_DEFAULT;
    }
    catch (const boost::exception &e) {
        LOG_ERROR << "create vxlan network db error" << diagnostic_information(e);
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

int32_t VxlanManager::LoadNetworkInfoFromDb() {
    m_networks.clear();

    try {
        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::shared_ptr<dbc::networkInfo> db_item = std::make_shared<dbc::networkInfo>();

            std::shared_ptr<byte_buf> item_buf = std::make_shared<byte_buf>();
            item_buf->write_to_byte_buf(it->value().data(), (uint32_t) it->value().size());
            network::binary_protocol proto(item_buf.get());
            db_item->read(&proto);             //may exception

            if (db_item->networkId.empty()) {
                LOG_ERROR << "load network error, id is empty";
                continue;
            }

            db_item->__set_lastUpdateTime(time(NULL));

            m_networks[db_item->networkId] = db_item;
        }
    }
    catch (std::exception& e) {
        LOG_ERROR << "load network info from db exception: " << e.what();
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

bool VxlanManager::UpdateNetworkDb(std::shared_ptr<dbc::networkInfo> info) {
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    info->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, info->networkId, slice);
    if (!status.ok()) {
        LOG_ERROR << "network db put info failed: " << info->networkId;
    }
    return status.ok();
}

bool VxlanManager::DeleteNetworkDb(const std::string &networkName) {
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = m_db->Delete(write_options, networkName);
    if (!status.ok()) {
        LOG_ERROR << "network db delete info failed: " << networkName;
    }
    return status.ok();
}

bool VxlanManager::CheckVni(const std::string &vni) const {
    RwMutex::ReadLock rlock(m_mtx);
    for (const auto &iter : m_networks) {
        if (vni == iter.second->vxlanVni) return true;
    }
    return false;
}

std::string VxlanManager::RandomVni() const {
    std::string vni;
    int tryCount = 0;
    do {
        if (tryCount > 10) break;
        int randVni = rand() % VNI_MAX;
        std::string temp = std::to_string(randVni);
        if (!CheckVni(temp)) vni = temp;
    }
    while (vni.empty());
    return vni;
}

bool VxlanManager::CheckIpCidr(const std::string &ipCidr) const {
    std::vector<std::string> vecSplit = util::split(ipCidr, "/");
    ipRangeHelper ipHelper(vecSplit[0], atoi(vecSplit[1].c_str()));

    RwMutex::ReadLock rlock(m_mtx);
    for (const auto &iter : m_networks) {
        std::vector<std::string> vecSplit2 = util::split(iter.second->ipCidr, "/");
        ipRangeHelper ipHelper2(vecSplit2[0], atoi(vecSplit2[1].c_str()));
        if (ipHelper.hasIntersection(ipHelper2)) return true;
    }
    return false;
}
