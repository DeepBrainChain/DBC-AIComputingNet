#include "p2p_lan_service.h"
#include "server/server.h"
#include "config/conf_manager.h"
#include "util/system_info.h"
#include "task/detail/VxlanManager.h"

// multicast message type
#define REQUEST_MACHINE_INFO      "machine info"
#define REQUEST_MACHINE_EXIT      "machine exit"
#define REQUEST_NETWORK_CREATE    "network create"
#define REQUEST_NETWORK_DELETE    "network delete"
#define REQUEST_NETWORK_QUERY     "network query"
#define REQUEST_NETWORK_LIST      "network list"
#define REQUEST_NETWORK_MOVE      "network move"
#define REQUEST_NETWORK_MOVE_ACK  "network move ack"
#define REQUEST_NETWORK_JOIN      "network join"
#define REQUEST_NETWORK_LEAVE     "network leave"

class TopN {
public:
    TopN(int n) : num(n) {}

    const std::multimap<int64_t, std::string>& GetData() const {
        return mm;
    }

    void Insert(int64_t first, const std::string &str) {
        mm.insert(std::make_pair(first, str));
        if (mm.size() > num && num > 0) {
            mm.erase(mm.begin());
            // mm.erase(mm.rbegin()->first);
        }
    }

    void Output() const {
        std::cout << "output map:" << std::endl;
        for (const auto &iter : mm) {
            std::cout << iter.first << "  " << iter.second << std::endl;
        }
    }
private:
    std::multimap<int64_t, std::string> mm;
    int num;
};

bool lan_machine_info::validate() const {
    if (machine_id.empty() || local_address.empty() || local_port == 0) return false;
    if (net_type.empty() || net_flag == 0) return false;
    if (node_type != NODE_TYPE::ComputeNode && node_type != NODE_TYPE::ClientNode && node_type != NODE_TYPE::SeedNode) return false;
    if (cur_time == 0) return false;
    ip_validator ip_vdr;
    variable_value val_ip(local_address, false);
    if (!ip_vdr.validate(val_ip)) return false;
    port_validator port_vdr;
    variable_value val_port(std::to_string((uint16_t)local_port), false);
    if (!port_vdr.validate(val_port)) return false;
    return true;
}

std::string lan_machine_info::to_request_json() const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("request");
    write.String(REQUEST_MACHINE_INFO);
    write.Key("data");
    write.StartObject();
    write.Key("machine_id");
    write.String(machine_id.c_str());
    write.Key("net_type");
    write.String(net_type.c_str());
    write.Key("net_flag");
    write.Int(net_flag);
    // write.Key("local_address");
    // write.String(local_address.c_str());
    write.Key("local_port");
    write.Uint(local_port);
    write.Key("node_type");
    write.Int((int32_t) node_type);
    // struct timespec ts;
    // clock_gettime(CLOCK_REALTIME, &ts);
    // write.Key("cur_time");
    // write.Int64(ts.tv_sec);
    write.EndObject();
    write.EndObject();
    return strBuf.GetString();
}

ERRCODE p2p_lan_service::init() {
    service_module::init();

    if (Server::NodeType != NODE_TYPE::BareMetalNode) {
        try {
            m_io_service_pool = std::make_shared<network::io_service_pool>();
            ERRCODE err = m_io_service_pool->init(1);
            if (ERR_SUCCESS != err) {
                LOG_ERROR << "init multicast io service failed";
                return err;
            }

            m_receiver = std::make_shared<multicast_receiver>(*m_io_service_pool->get_io_service().get(),
                boost::asio::ip::make_address(ConfManager::instance().GetNetListenIp()),
                boost::asio::ip::make_address(ConfManager::instance().GetMulticastAddress()),
                ConfManager::instance().GetMulticastPort());
            err = m_receiver->start();
            if (ERR_SUCCESS != err) {
                LOG_ERROR << "init multicast receiver failed";
                return err;
            }

            m_sender = std::make_shared<multicast_sender>(*m_io_service_pool->get_io_service().get(),
                boost::asio::ip::make_address(ConfManager::instance().GetMulticastAddress()),
                ConfManager::instance().GetMulticastPort());

            err = m_io_service_pool->start();
            if (ERR_SUCCESS != err) {
                LOG_ERROR << "multicast io service run failed";
                return err;
            }

            init_local_machine_info();

            send_network_query_request();
        }
        catch (std::exception& e)
        {
            LOG_ERROR << "init multicast receiver Exception: " << e.what();
            return -1;
        }
    }

    return ERR_SUCCESS;
}

void p2p_lan_service::exit() {
	service_module::exit();
    if (Server::NodeType != NODE_TYPE::BareMetalNode) {
        m_receiver->stop();
        send_machine_exit_request();
        sleep(2);
        // m_sender->stop();
        m_io_service_pool->stop();
        m_io_service_pool->exit();
    }
}

const std::map<std::string, lan_machine_info>& p2p_lan_service::get_lan_nodes() const {
    RwMutex::ReadLock rlock(m_mtx);
    return m_lan_nodes;
}

void p2p_lan_service::add_lan_node(const lan_machine_info& lm_info) {
    if (!lm_info.validate()) return;
    if (lm_info.machine_id == ConfManager::instance().GetNodeId()) return;
    LOG_INFO << "update machine info of node " << lm_info.machine_id;
    RwMutex::WriteLock wlock(m_mtx);
    m_lan_nodes[lm_info.machine_id] = lm_info;
}

void p2p_lan_service::delete_lan_node(const std::string& machine_id) {
    if (machine_id.empty()) return ;
    {
        RwMutex::ReadLock rlock(m_mtx);
        auto iter = m_lan_nodes.find(machine_id);
        if (iter == m_lan_nodes.end()) return;
    }
    LOG_INFO << "delete machine info of node " << machine_id;
    RwMutex::WriteLock wlock(m_mtx);
    m_lan_nodes.erase(machine_id);
}

bool p2p_lan_service::is_same_host(const std::string& machine_id) const {
    if (machine_id == ConfManager::instance().GetNodeId()) return true;
    RwMutex::ReadLock rlock(m_mtx);
    auto iter = m_lan_nodes.find(machine_id);
    if (iter != m_lan_nodes.end()) {
        return iter->second.local_address == m_local_machine_info.local_address;
    }
    return false;
}

void p2p_lan_service::on_multicast_receive(const std::string& data, const std::string& addr) {
    rapidjson::Document doc;
    doc.Parse(data.c_str());
    if (!doc.IsObject()) {
        LOG_ERROR << "parse multicast message failed";
        return;
    }

    std::string request_type;
    JSON_PARSE_STRING(doc, "request", request_type);
    if (!doc.HasMember("data")) {
        LOG_ERROR << "multicast message has none data";
        return;
    }
    const rapidjson::Value& v_data = doc["data"];
    if (!v_data.IsObject()) {
        LOG_ERROR << "multicast message data is not an object";
        return;
    }

    LOG_INFO << "receive multicast data from " << addr << ", request_type: " << request_type;

    if (request_type == REQUEST_MACHINE_INFO) {
        lan_machine_info lm_info;
        JSON_PARSE_STRING(v_data, "machine_id", lm_info.machine_id);
        JSON_PARSE_STRING(v_data, "net_type", lm_info.net_type);
        JSON_PARSE_INT(v_data, "net_flag", lm_info.net_flag);
        lm_info.local_address = addr;
        JSON_PARSE_UINT(v_data, "local_port", lm_info.local_port);
        int32_t tmp_node_type = 0;
        JSON_PARSE_INT(v_data, "node_type", tmp_node_type);
        lm_info.node_type = (NODE_TYPE) tmp_node_type;
        // JSON_PARSE_INT64(v_data, "cur_time", lm_info.cur_time);
        lm_info.cur_time = time(NULL);
        add_lan_node(lm_info);
    } else if (request_type == REQUEST_MACHINE_EXIT) {
        std::string machine_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        delete_lan_node(machine_id);
    } else if (request_type == REQUEST_NETWORK_CREATE) {
        std::string machine_id, network_name, bridge_name, vxlan_name, vxlan_vni,
            ip_cidr, ip_start, ip_end, dhcp_interface, rent_wallet;
        int64_t lastUseTime;
        std::vector<std::string> members;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        JSON_PARSE_STRING(v_data, "bridge_name", bridge_name);
        JSON_PARSE_STRING(v_data, "vxlan_name", vxlan_name);
        JSON_PARSE_STRING(v_data, "vxlan_vni", vxlan_vni);
        JSON_PARSE_STRING(v_data, "ip_cidr", ip_cidr);
        JSON_PARSE_STRING(v_data, "ip_start", ip_start);
        JSON_PARSE_STRING(v_data, "ip_end", ip_end);
        JSON_PARSE_STRING(v_data, "dhcp_interface", dhcp_interface);
        JSON_PARSE_STRING(v_data, "rent_wallet", rent_wallet);
        JSON_PARSE_INT64(v_data, "lastUseTime", lastUseTime);
        std::shared_ptr<dbc::networkInfo> info = std::make_shared<dbc::networkInfo>();
        info->__set_networkId(network_name);
        info->__set_bridgeName(bridge_name);
        info->__set_vxlanName(vxlan_name);
        info->__set_vxlanVni(vxlan_vni);
        info->__set_ipCidr(ip_cidr);
        info->__set_ipStart(ip_start);
        info->__set_ipEnd(ip_end);
        info->__set_dhcpInterface(dhcp_interface);
        info->__set_machineId(machine_id);
        info->__set_rentWallet(rent_wallet);
        info->__set_members(members);
        info->__set_lastUseTime(lastUseTime);
        info->__set_nativeFlags(0);
        info->__set_lastUpdateTime(time(NULL));
        FResult fret = VxlanManager::instance().AddNetworkFromMulticast(std::move(info));
    } else if (request_type == REQUEST_NETWORK_DELETE) {
        std::string machine_id, network_name;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        VxlanManager::instance().DeleteNetworkFromMulticast(network_name, machine_id);
    } else if (request_type == REQUEST_NETWORK_QUERY) {
        std::string machine_id, network_name;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        if (machine_id == m_local_machine_info.machine_id) return;
        // JSON_PARSE_STRING(v_data, "network_name", network_name);
        bool bFind = false;
        if (v_data.HasMember("networks") && v_data["networks"].IsArray()) {
            const rapidjson::Value& v_networks = v_data["networks"];
            for (size_t i = 0; i < v_networks.Size(); i++) {
                const rapidjson::Value& v_network = v_networks[i];
                if (v_network.IsString())
                    network_name = v_network.GetString();
                std::shared_ptr<dbc::networkInfo> info = VxlanManager::instance().GetNetwork(network_name);
                if (info && info->machineId == m_local_machine_info.machine_id) {
                    bFind = true;
                    break;
                }
            }
        }
        if (bFind) send_network_list_request();
    } else if (request_type == REQUEST_NETWORK_LIST) {
        std::string machine_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        if (v_data.HasMember("networks") && v_data["networks"].IsArray()) {
            const rapidjson::Value& v_networks = v_data["networks"];
            for (size_t i = 0; i < v_networks.Size(); i++) {
                const rapidjson::Value& v_network = v_networks[i];
                if (!v_network.IsObject()) continue;
                std::string network_name, bridge_name, vxlan_name, vxlan_vni,
                    ip_cidr, ip_start, ip_end, dhcp_interface, rent_wallet;
                std::vector<std::string> members;
                int64_t lastUseTime;
                JSON_PARSE_STRING(v_network, "network_name", network_name);
                JSON_PARSE_STRING(v_network, "bridge_name", bridge_name);
                JSON_PARSE_STRING(v_network, "vxlan_name", vxlan_name);
                JSON_PARSE_STRING(v_network, "vxlan_vni", vxlan_vni);
                JSON_PARSE_STRING(v_network, "ip_cidr", ip_cidr);
                JSON_PARSE_STRING(v_network, "ip_start", ip_start);
                JSON_PARSE_STRING(v_network, "ip_end", ip_end);
                JSON_PARSE_STRING(v_network, "dhcp_interface", dhcp_interface);
                JSON_PARSE_STRING(v_network, "rent_wallet", rent_wallet);
                if (v_network.HasMember("members") && v_network["members"].IsArray()) {
                    const rapidjson::Value& v_ms = v_network["members"];
                    for (size_t j = 0; j < v_ms.Size(); j++) {
                        const rapidjson::Value& member = v_ms[j];
                        if (member.IsString()) {
                            members.push_back(member.GetString());
                        }
                    }
                }
                JSON_PARSE_INT64(v_network, "lastUseTime", lastUseTime)
                std::shared_ptr<dbc::networkInfo> info = std::make_shared<dbc::networkInfo>();
                info->__set_networkId(network_name);
                info->__set_bridgeName(bridge_name);
                info->__set_vxlanName(vxlan_name);
                info->__set_vxlanVni(vxlan_vni);
                info->__set_ipCidr(ip_cidr);
                info->__set_ipStart(ip_start);
                info->__set_ipEnd(ip_end);
                info->__set_dhcpInterface(dhcp_interface);
                info->__set_machineId(machine_id);
                info->__set_rentWallet(rent_wallet);
                info->__set_members(members);
                info->__set_lastUseTime(lastUseTime);
                info->__set_nativeFlags(0);
                info->__set_lastUpdateTime(time(NULL));
                FResult fret = VxlanManager::instance().AddNetworkFromMulticast(std::move(info));

                RwMutex::WriteLock wlock(m_mtx_timer);
                auto iter = m_network_move_timer_id.find(network_name);
                if (iter != m_network_move_timer_id.end()) {
                    remove_timer(iter->second);
                    m_network_move_timer_id.erase(network_name);
                }
            }
        }
    } else if (request_type == REQUEST_NETWORK_MOVE) {
        std::string machine_id, network_name, new_machine_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        // 2022-04-19 新版本的REQUEST_NETWORK_MOVE消息中已删除new_machine_id字段
        JSON_PARSE_STRING(v_data, "new_machine_id", new_machine_id);
        if (new_machine_id == ConfManager::instance().GetNodeId()) {
            FResult fret = VxlanManager::instance().MoveNetwork(network_name, new_machine_id);
            if (fret.errcode != 0)
                send_network_move_request(network_name, machine_id);
        }
        if (v_data.HasMember("candidate_queue") && v_data["candidate_queue"].IsArray()) {
            const rapidjson::Value& v_candidates = v_data["candidate_queue"];
            for (size_t i = 0; i < v_candidates.Size(); i++) {
                new_machine_id.clear();
                const rapidjson::Value& candidate = v_candidates[i];
                if (candidate.IsString()) {
                    new_machine_id = candidate.GetString();
                }
                if (new_machine_id == ConfManager::instance().GetNodeId()) {
                    RwMutex::WriteLock wlock(m_mtx_timer);
                    switch (i) {
                        case 0:
                            m_network_move_timer_id[network_name] = add_timer(network_name, 1000, 100, 1, "",
                                std::bind(&p2p_lan_service::on_multicast_network_move_task_timer,
                                    this, std::placeholders::_1));
                            break;
                        case 1:
                            m_network_move_timer_id[network_name] = add_timer(network_name, 30 * 1000, 100, 1, "",
                                std::bind(&p2p_lan_service::on_multicast_network_move_task_timer,
                                    this, std::placeholders::_1));
                            break;
                        case 2:
                            m_network_move_timer_id[network_name] = add_timer(network_name, 60 * 1000, 100, 1, "",
                                std::bind(&p2p_lan_service::on_multicast_network_move_task_timer,
                                    this, std::placeholders::_1));
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    } else if (request_type == REQUEST_NETWORK_MOVE_ACK) {
        std::string machine_id, network_name, new_machine_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        JSON_PARSE_STRING(v_data, "new_machine_id", new_machine_id);
        VxlanManager::instance().MoveNetworkAck(network_name, new_machine_id);
        RwMutex::WriteLock wlock(m_mtx_timer);
        auto iter = m_network_move_timer_id.find(network_name);
        if (iter != m_network_move_timer_id.end()) {
            remove_timer(iter->second);
            m_network_move_timer_id.erase(network_name);
        }
    } else if (request_type == REQUEST_NETWORK_JOIN) {
        std::string machine_id, network_name, task_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        JSON_PARSE_STRING(v_data, "task_id", task_id);
        VxlanManager::instance().JoinNetwork(network_name, task_id);
    } else if (request_type == REQUEST_NETWORK_LEAVE) {
        std::string machine_id, network_name, task_id;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        JSON_PARSE_STRING(v_data, "network_name", network_name);
        JSON_PARSE_STRING(v_data, "task_id", task_id);
        VxlanManager::instance().LeaveNetwork(network_name, task_id);
    }
}

void p2p_lan_service::send_network_create_request(std::shared_ptr<dbc::networkInfo> info) {
    if (m_sender) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_CREATE);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(info->networkId.c_str());
        write.Key("bridge_name");
        write.String(info->bridgeName.c_str());
        write.Key("vxlan_name");
        write.String(info->vxlanName.c_str());
        write.Key("vxlan_vni");
        write.String(info->vxlanVni.c_str());
        write.Key("ip_cidr");
        write.String(info->ipCidr.c_str());
        write.Key("ip_start");
        write.String(info->ipStart.c_str());
        write.Key("ip_end");
        write.String(info->ipEnd.c_str());
        write.Key("dhcp_interface");
        write.String(info->dhcpInterface.c_str());
        // write.Key("machine_id");
        // write.String(info->machineId.c_str());
        write.Key("rent_wallet");
        write.String(info->rentWallet.c_str());
        write.Key("lastUseTime");
        write.Int64(info->lastUseTime);
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_delete_request(const std::string& network_name) {
    if (m_sender) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_DELETE);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(network_name.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_query_request() {
    if (m_sender) {
        int count = 0;
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_QUERY);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("networks");
        write.StartArray();
        const std::map<std::string, std::shared_ptr<dbc::networkInfo>> networks = VxlanManager::instance().GetNetworks();
        for (const auto& iter : networks) {
            // if (iter.second->machineId.empty()) {
            if (iter.second->machineId != m_local_machine_info.machine_id) {
                write.String(iter.first.c_str());
                count++;
            }
        }
        write.EndArray();
        write.EndObject();
        write.EndObject();
        if (count > 0)
            m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_list_request() {
    if (m_sender) {
        int count = 0;
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_LIST);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("networks");
        write.StartArray();
        const std::map<std::string, std::shared_ptr<dbc::networkInfo>> networks = VxlanManager::instance().GetNetworks();
        for (const auto& iter : networks) {
            if (!iter.second->ipCidr.empty() && iter.second->machineId == m_local_machine_info.machine_id) {
                write.StartObject();
                write.Key("network_name");
                write.String(iter.second->networkId.c_str());
                write.Key("bridge_name");
                write.String(iter.second->bridgeName.c_str());
                write.Key("vxlan_name");
                write.String(iter.second->vxlanName.c_str());
                write.Key("vxlan_vni");
                write.String(iter.second->vxlanVni.c_str());
                write.Key("ip_cidr");
                write.String(iter.second->ipCidr.c_str());
                write.Key("ip_start");
                write.String(iter.second->ipStart.c_str());
                write.Key("ip_end");
                write.String(iter.second->ipEnd.c_str());
                write.Key("dhcp_interface");
                write.String(iter.second->dhcpInterface.c_str());
                // write.Key("machine_id");
                // write.String(iter.second->machineId.c_str());
                write.Key("rent_wallet");
                write.String(iter.second->rentWallet.c_str());
                write.Key("members");
                write.StartArray();
                for (const auto& node : iter.second->members) {
                    write.String(node.c_str());
                }
                write.EndArray();
                write.Key("lastUseTime");
                write.Int64(iter.second->lastUseTime);
                write.EndObject();
                count++;
            }
        }
        write.EndArray();
        write.EndObject();
        write.EndObject();
        if (count > 0)
            m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_move_request(const std::string& network_name, const std::string& old_machine_id) {
    if (m_sender) {
        // 在符合要求的机器列表中，取时间最近的3个，作为候选的转移目标
        TopN topn(3);
        {
            RwMutex::ReadLock rlock(m_mtx);
            for (const auto& iter : m_lan_nodes) {
                if (iter.second.net_flag != m_local_machine_info.net_flag) continue;
                if (iter.first == old_machine_id) continue;
                if (iter.first == m_local_machine_info.machine_id) continue;
                topn.Insert(iter.second.cur_time, iter.first);
            }
        }
        const std::multimap<int64_t, std::string>& topmap = topn.GetData();
        if (topmap.empty()) {
            LOG_ERROR << "can not find other machine to move network";
            return;
        }
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_MOVE);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(network_name.c_str());
        // write.Key("new_machine_id");
        // write.String(new_machine_id.c_str());
        write.Key("candidate_queue");
        write.StartArray();
        for (auto iter = topmap.rbegin(); iter != topmap.rend(); iter++)
            write.String(iter->second.c_str());
        write.EndArray();
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_move_ack_request(const std::string& network_name, const std::string& machine_id) {
    if (m_sender && !network_name.empty() && !machine_id.empty()) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_MOVE_ACK);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(network_name.c_str());
        write.Key("new_machine_id");
        write.String(machine_id.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_join_request(const std::string& network_name, const std::string& task_id) {
    if (m_sender && !network_name.empty() && !task_id.empty()) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_JOIN);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(network_name.c_str());
        write.Key("task_id");
        write.String(task_id.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::send_network_leave_request(const std::string& network_name, const std::string& task_id) {
    if (m_sender && !network_name.empty() && !task_id.empty()) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_NETWORK_LEAVE);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.Key("network_name");
        write.String(network_name.c_str());
        write.Key("task_id");
        write.String(task_id.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}

void p2p_lan_service::init_timer() {
    if (Server::NodeType != NODE_TYPE::BareMetalNode) {
        // 50 second
        add_timer(AI_MULTICAST_MACHINE_TIMER, 5 * 1000, 50 * 1000, ULLONG_MAX, "",
            std::bind(&p2p_lan_service::on_multicast_machine_task_timer, this, std::placeholders::_1));
        // 2 min
        add_timer(AI_MULTICAST_NETWORK_TIMER, 90 * 1000, 120 * 1000, ULLONG_MAX, "",
            std::bind(&p2p_lan_service::on_multicast_network_task_timer, this, std::placeholders::_1));
        // 2 min latter
        add_timer(AI_MULTICAST_NETWORK_RESUME_TIMER, 120 * 1000, 1000, 1, "",
            std::bind(&p2p_lan_service::on_multicast_network_resume_task_timer, this, std::placeholders::_1));
    }
}

void p2p_lan_service::init_invoker() {

}

void p2p_lan_service::init_local_machine_info() {
    m_local_machine_info.machine_id = ConfManager::instance().GetNodeId();
    m_local_machine_info.net_type = ConfManager::instance().GetNetType();
    m_local_machine_info.net_flag = ConfManager::instance().GetNetFlag();
    m_local_machine_info.local_address = SystemInfo::instance().GetDefaultRouteIp();
    m_local_machine_info.local_port = ConfManager::instance().GetNetListenPort();
    m_local_machine_info.node_type = Server::NodeType;
    m_local_machine_message = m_local_machine_info.to_request_json();
}

void p2p_lan_service::on_multicast_machine_task_timer(const std::shared_ptr<core_timer>& timer) {
    if (!m_local_machine_message.empty() && m_sender) {
        m_sender->send(m_local_machine_message);
    }
    std::vector<std::string> machines;
    time_t cur_time = time(NULL);
    {
        RwMutex::ReadLock rlock(m_mtx);
        for (auto iter = m_lan_nodes.begin(); iter != m_lan_nodes.end(); iter++) {
            // 如果5min没有收到任何REQUEST_MACHINE_INFO消息，则判断为退出
            if (difftime(cur_time, iter->second.cur_time) > 300) {
                LOG_INFO << "No heartbeat received for a long time, so " << iter->first << " exit";
                machines.push_back(iter->first);
            }
        }
    }
    for (const auto& iter : machines) {
        delete_lan_node(iter);
    }
}

void p2p_lan_service::on_multicast_network_task_timer(const std::shared_ptr<core_timer>& timer) {
    send_network_list_request();
    // 删除长时间没有虚拟机使用的网络
    VxlanManager::instance().ClearEmptyNetwork();
    VxlanManager::instance().ClearExpiredNetwork();
}

void p2p_lan_service::on_multicast_network_move_task_timer(const std::shared_ptr<core_timer>& timer) {
    FResult fret = VxlanManager::instance().MoveNetwork(timer->get_name(), m_local_machine_info.machine_id);
    if (fret.errcode == ERR_SUCCESS) {
        send_network_list_request();
    } else {
        LOG_ERROR << "move network " << timer->get_name() << " to myself failed: " << fret.errmsg;
    }
    RwMutex::WriteLock wlock(m_mtx_timer);
    auto iter = m_network_move_timer_id.find(timer->get_name());
    if (iter != m_network_move_timer_id.end()) {
        // remove_timer(iter->second);
        m_network_move_timer_id.erase(timer->get_name());
    }
}

// 如果启动了一段时间后，发现有网络的所在machineId为空，就自动认领。
void p2p_lan_service::on_multicast_network_resume_task_timer(const std::shared_ptr<core_timer>& timer) {
    std::vector<std::string> network_resumes;
    {
        const std::map<std::string, std::shared_ptr<dbc::networkInfo>> networks = VxlanManager::instance().GetNetworks();
        for (const auto& iter : networks) {
            if (iter.second->machineId.empty())
                network_resumes.push_back(iter.first);
        }
    }
    for (const auto &network_name : network_resumes) {
        FResult fret = VxlanManager::instance().MoveNetwork(network_name, m_local_machine_info.machine_id);
        if (fret.errcode == ERR_SUCCESS)
            send_network_list_request();
    }
}

void p2p_lan_service::send_machine_exit_request() const {
    if (m_sender) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_MACHINE_EXIT);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}
