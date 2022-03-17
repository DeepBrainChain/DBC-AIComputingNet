#include "VxlanManager.h"
#include <boost/asio/ip/address.hpp>
#include "log/log.h"
#include "network/protocol/thrift_binary.h"
#include "server/server.h"

inline std::string random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[static_cast<size_t>(rand()) % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

inline bool check_network_name(const std::string& name) {
    if (name.length() < 6) return false;
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
        bits_ = bits;
    }

    std::string getIpStart() const {
        struct in_addr_helper ip;
        ip.S_un.S_addr = ip_.s_addr & ~(0xFFFFFF << bits_);
        ip.S_un.S_un_b.s_b4 += 1;
        char str[16] = {0};//INET_ADDRSTRLEN
        return inet_ntop(AF_INET, &ip, str, sizeof(str));
    }

    std::string getIpEnd() const {
        struct in_addr_helper ip;
        ip.S_un.S_addr = ip_.s_addr | (0xFFFFFF << bits_);
        ip.S_un.S_un_b.s_b4 -= 1;
        char str[16] = {0};//INET_ADDRSTRLEN
        return inet_ntop(AF_INET, &ip, str, sizeof(str));
    }

    // 判断两个网络是否有交集
    // 有A、B两地址，掩码分别为x,y，分两步判断
    // 1、算A和B是否在同一网段时，分别都用A的掩码算，如果得到的两个网络地址一样，则说明A和B在同一网段（可以理解为A到B方向）
    // 2、算B和A是否在同一网段时，分别都用B的掩码算，如果的到的两个网络地址一致，说明B和A在同一网段（可以理解为B到A方向）
    bool hasIntersection(const ipRangeHelper &other) const {
        in_addr_t mask1 = ~(0xFFFFFF << bits_);
        if ((ip_.s_addr & mask1) == (other.ip_.s_addr & mask1)) return true;
        in_addr_t mask2 = ~(0xFFFFFF << other.bits_);
        if ((ip_.s_addr & mask2) == (other.ip_.s_addr & mask2)) return true;
        return false;
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

    LoadNetworkInfoFromDb();
    return ERR_SUCCESS;
}

FResult VxlanManager::CreateNetwork(const std::string &networkId, const std::string &bridgeName, const std::string &vxlanName, const std::string &vni, const std::string &ipCidr) {
    if (networkId.empty()) return FResult(ERR_ERROR, "network name can not be empty");
    if (!check_network_name(networkId)) return FResult(ERR_ERROR, "network name requires a combination of more than 6 letters or numbers");
    if (GetNetwork(networkId)) return FResult(ERR_ERROR, "network name already existed");
    if (vni.empty()) return FResult(ERR_ERROR, "vni can not be empty");
    if (CheckVni(vni)) return FResult(ERR_ERROR, "vni already exist");
    
    std::shared_ptr<dbc::networkInfo> info = std::make_shared<dbc::networkInfo>();
    info->__set_networkId(networkId);
    info->__set_bridgeName(bridgeName);
    info->__set_vxlanName(vxlanName);
    info->__set_vxlanVni(vni);

    bfs::path shell_path = EnvManager::instance().get_shell_path();
    if (Server::NodeType == DBC_NODE_TYPE::DBC_CLIENT_NODE) {
        if (ipCidr.empty()) return FResult(ERR_ERROR, "ip cidr can not be empty");
        if (ipCidr.find("192.168.122.") != std::string::npos) return FResult(ERR_ERROR, "ip cidr already exist");
        std::vector<std::string> vecSplit = util::split(ipCidr, "/");
        if (vecSplit.size() != 2) return FResult(ERR_ERROR, "invalid ip cidr");
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(vecSplit[0]);
        if (!addr.is_v4()) return FResult(ERR_ERROR, "invalid ip cidr");
        if (CheckIpCidr(ipCidr)) return FResult(ERR_ERROR, "ip cidr already exist");

        ipRangeHelper ipHelper(vecSplit[0], atoi(vecSplit[1].c_str()));

        info->__set_ipCidr(ipCidr);
        info->__set_ipStart(ipHelper.getIpStart());
        info->__set_ipEnd(ipHelper.getIpEnd());

        shell_path /= "network/create_network_client.sh";
        std::string cmd = shell_path.generic_string() + " " + info->bridgeName + " " + info->vxlanName + " " + info->vxlanVni;
        cmd.append(" " + info->ipCidr + " " + info->ipStart + " " + info->ipEnd);
        std::string cmdRet = run_shell(cmd);
        rapidjson::Document doc;
        rapidjson::ParseResult ok = doc.Parse(cmdRet.c_str());
        if (!ok) return FResult(ERR_ERROR, "parse shell result error");
        FResult fret = {ERR_ERROR, "parse shell result error"};
        if (doc.HasMember("errcode") && doc["errcode"].IsInt()) {
            fret.errcode = doc["errcode"].GetInt();
        }
        if (fret.errcode != 0) {
            if (doc.HasMember("message") && doc["message"].IsString()) {
                fret.errmsg = doc["message"].GetString();
            }
            return fret;
        }
    } else if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        shell_path /= "network/create_network_mining.sh";
        std::string cmd = shell_path.generic_string() + " " + info->bridgeName + " " + info->vxlanName + " " + info->vxlanVni;
        std::string cmdRet = run_shell(cmd);
    }

    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    dbc::network::binary_protocol proto(out_buf.get());
    info->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, networkId, slice);
    if (!status.ok()) {
        LOG_INFO << "network db put info failed: " << networkId;
        return FResult(ERR_ERROR, "write network db failed");
    }
    
    RwMutex::WriteLock wlock(m_mtx);
    m_networks[networkId] = info;
    return FResultSuccess;
}

FResult VxlanManager::CreateClientNetwork(const std::string &networkId, const std::string &vni, const std::string &ipCidr) {
    std::string random = random_string(8);
    return CreateNetwork(networkId, "br" + random, "vx" + random, vni, ipCidr);
}

FResult VxlanManager::CreateMiningNetwork(const std::string &networkId, const std::string &bridgeName, const std::string &vxlanName, const std::string &vni) {
    return CreateNetwork(networkId, bridgeName, vxlanName, vni, "");
}

void VxlanManager::DeleteNetwork(const std::string &networkId) {
    RwMutex::WriteLock wlock(m_mtx);
    auto iter = m_networks.find(networkId);
    if (iter != m_networks.end()) {
        bfs::path shell_path = EnvManager::instance().get_shell_path() / "network/delete_network.sh";
        std::string cmd = shell_path.generic_string() + " " + iter->second->bridgeName + " " + iter->second->vxlanName;
        std::string cmdRet = run_shell(cmd);
        m_networks.erase(iter);
    }
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = m_db->Delete(write_options, networkId);
    if (!status.ok()) {
        LOG_INFO << "network db delete info failed: " << networkId;
    }
}

const std::map<std::string, std::shared_ptr<dbc::networkInfo> > VxlanManager::GetNetworks() const {
    RwMutex::ReadLock rlock(m_mtx);
    return m_networks;
}

std::shared_ptr<dbc::networkInfo> VxlanManager::GetNetwork(const std::string &networkId) const {
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_networks.find(networkId);
    if (it != m_networks.end()) return it->second;
    return nullptr;
}

bool VxlanManager::CheckVni(const std::string &vni) const {
    RwMutex::ReadLock rlock(m_mtx);
    for (const auto &iter : m_networks) {
        if (vni == iter.second->vxlanVni) return true;
    }
    return false;
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

int32_t VxlanManager::InitDb() {
    leveldb::DB *db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    try {
        bfs::path db_path = EnvManager::instance().get_db_path();
        if (false == bfs::exists(db_path)) {
            LOG_DEBUG << "db directory path does not exist and create db directory";
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
            dbc::network::binary_protocol proto(item_buf.get());
            db_item->read(&proto);             //may exception

            if (db_item->networkId.empty()) {
                LOG_ERROR << "load network error, id is empty";
                continue;
            }

            m_networks[db_item->networkId] = db_item;
        }
    }
    catch (std::exception& e) {
        LOG_ERROR << "load network info from db exception: " << e.what();
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}
