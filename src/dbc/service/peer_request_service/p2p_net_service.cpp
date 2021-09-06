#include "p2p_net_service.h"
#include <cassert>
#include "server/server.h"
#include "config/conf_manager.h"
#include "network/tcp_acceptor.h"
#include "service_module/service_message_id.h"
#include "network/protocol/service_message_def.h"
#include "socket_channel_handler/matrix_client_socket_channel_handler.h"
#include "socket_channel_handler/matrix_server_socket_channel_handler.h"
#include "../message/cmd_message.h"
#include "network/tcp_socket_channel.h"
#include "timer/timer_def.h"
#include "peers_db_types.h"
#include <leveldb/write_batch.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include "network/protocol/thrift_binary.h"
#include "network/net_address.h"
#include "data/resource/system_info.h"

#define CHECK_PEER_CANDIDATES_TIMER                 "p2p_timer_check_peer_candidates"
#define DYANMIC_ADJUST_NETWORK_TIMER                "p2p_timer_dynamic_adjust_network"
#define PEER_INFO_EXCHANGE_TIMER                    "p2p_timer_peer_info_exchange"
#define DUMP_PEER_CANDIDATES_TIMER                  "p2p_timer_dump_peer_candidates"

const uint32_t max_reconnect_times = 1;
const uint32_t max_connected_cnt = 1024;
const uint32_t max_connect_per_check = 16;

namespace fs = boost::filesystem;

p2p_net_service::~p2p_net_service() {
    remove_timer(m_timer_check_peer_candidates);
    remove_timer(m_timer_dyanmic_adjust_network);
    remove_timer(m_timer_peer_info_exchange);
    remove_timer(m_timer_dump_peer_candidates);
}

int32_t p2p_net_service::init(bpo::variables_map &options) {
    service_module::init(options);

    init_rand();

    if (E_SUCCESS != init_conf()) {
        LOG_ERROR << "init_conf error";
        return E_DEFAULT;
    }

    if (E_SUCCESS != init_db()) {
        LOG_ERROR << "init_db error";
        return E_DEFAULT;
    }

    load_peer_candidates();

    if (E_SUCCESS != init_acceptor()) {
        LOG_ERROR << "init_acceptor error";
        return E_DEFAULT;
    }

    if (E_SUCCESS != init_connector(options)) {
        LOG_ERROR << "init_connector error";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

void p2p_net_service::init_timer() {
    // 1min
    m_timer_invokers[CHECK_PEER_CANDIDATES_TIMER] = std::bind(&p2p_net_service::on_timer_check_peer_candidates, this, std::placeholders::_1);
    add_timer(CHECK_PEER_CANDIDATES_TIMER, 5 * 1000, 1, "");
    m_timer_check_peer_candidates = add_timer(CHECK_PEER_CANDIDATES_TIMER, 60 * 1000, ULLONG_MAX, "");

    // 1min
    m_timer_invokers[DYANMIC_ADJUST_NETWORK_TIMER] = std::bind(&p2p_net_service::on_timer_dyanmic_adjust_network, this, std::placeholders::_1);
    m_timer_dyanmic_adjust_network = add_timer(DYANMIC_ADJUST_NETWORK_TIMER, 60 * 1000, ULLONG_MAX, "");

    // 3min
    m_timer_invokers[PEER_INFO_EXCHANGE_TIMER] = std::bind(&p2p_net_service::on_timer_peer_info_exchange, this, std::placeholders::_1);
    m_timer_peer_info_exchange = add_timer(PEER_INFO_EXCHANGE_TIMER, 3 * 60 * 1000, ULLONG_MAX, "");

    // 10min
    m_timer_invokers[DUMP_PEER_CANDIDATES_TIMER] = std::bind(&p2p_net_service::on_timer_peer_candidate_dump, this, std::placeholders::_1);
    m_timer_dump_peer_candidates = add_timer(DUMP_PEER_CANDIDATES_TIMER, 10 * 60 * 1000, ULLONG_MAX, DEFAULT_STRING);
}

void p2p_net_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(TCP_CHANNEL_ERROR, &p2p_net_service::on_tcp_channel_error)
    BIND_MESSAGE_INVOKER(CLIENT_CONNECT_NOTIFICATION, &p2p_net_service::on_client_tcp_connect_notification)

    BIND_MESSAGE_INVOKER(VER_REQ, &p2p_net_service::on_ver_req)
    BIND_MESSAGE_INVOKER(VER_RESP, &p2p_net_service::on_ver_resp)

    BIND_MESSAGE_INVOKER(P2P_GET_PEER_NODES_REQ, &p2p_net_service::on_get_peer_nodes_req)
    BIND_MESSAGE_INVOKER(P2P_GET_PEER_NODES_RESP, &p2p_net_service::on_get_peer_nodes_resp)

    BIND_MESSAGE_INVOKER(typeid(cmd_get_peer_nodes_req).name(), &p2p_net_service::on_cmd_get_peer_nodes_req)
}

void p2p_net_service::init_subscription() {
    SUBSCRIBE_BUS_MESSAGE(TCP_CHANNEL_ERROR);
    SUBSCRIBE_BUS_MESSAGE(CLIENT_CONNECT_NOTIFICATION);
    SUBSCRIBE_BUS_MESSAGE(VER_REQ);
    SUBSCRIBE_BUS_MESSAGE(VER_RESP);
    SUBSCRIBE_BUS_MESSAGE(P2P_GET_PEER_NODES_REQ);
    SUBSCRIBE_BUS_MESSAGE(P2P_GET_PEER_NODES_RESP);

    SUBSCRIBE_BUS_MESSAGE(typeid(cmd_get_peer_nodes_req).name());
}

void p2p_net_service::init_rand() {
    m_rand_seed = uint256();
    m_rand_ctx = FastRandomContext(m_rand_seed);
}

int32_t p2p_net_service::init_conf() {
    variable_value val;
    ip_validator ip_vdr;
    port_validator port_vdr;

    const std::string &host_ip = conf_manager::instance().get_host_ip();
    val.value() = host_ip;
    if (!ip_vdr.validate(val)) {
        LOG_ERROR << "listen ip is invalid: " << host_ip;
        return E_DEFAULT;
    } else {
        m_listen_ip = host_ip;
    }

    std::string s_port = conf_manager::instance().get_net_listen_port();
    val.value() = s_port;
    if (!port_vdr.validate(val)) {
        LOG_ERROR << "listen port is invalid: " << s_port;
        return E_DEFAULT;
    } else {
        m_listen_port = (uint16_t) atoi(s_port);
    }

    m_dns_seeds.insert(m_dns_seeds.begin(), conf_manager::instance().get_dns_seeds().begin(),
                       conf_manager::instance().get_dns_seeds().end());

    m_hard_code_seeds.insert(m_hard_code_seeds.begin(), conf_manager::instance().get_hard_code_seeds().begin(),
                             conf_manager::instance().get_hard_code_seeds().end());

    return E_SUCCESS;
}

int32_t p2p_net_service::init_db() {
    leveldb::DB *db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    try {
        fs::path task_db_path = env_manager::instance().get_db_path();
        if (false == fs::exists(task_db_path)) {
            LOG_DEBUG << "db directory path does not exist and create db directory";
            fs::create_directory(task_db_path);
        }

        if (false == fs::is_directory(task_db_path)) {
            LOG_ERROR << "db directory path does not exist and exit";
            return E_DEFAULT;
        }

        task_db_path /= fs::path("peers.db");
        leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
        if (false == status.ok()) {
            LOG_ERROR << "p2p net service init peers db error: " << status.ToString();
            return E_DEFAULT;
        }

        m_peers_candidates_db.reset(db);
    }
    catch (const std::exception &e) {
        LOG_ERROR << "create p2p db error: " << e.what();
        return E_DEFAULT;
    }
    catch (const boost::exception &e) {
        LOG_ERROR << "create p2p db error" << diagnostic_information(e);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::load_peer_candidates() {
    m_peer_candidates.clear();

    try {
        ip_validator ip_vdr;
        port_validator port_vdr;

        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_peers_candidates_db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::shared_ptr<dbc::db_peer_candidate> db_candidate = std::make_shared<dbc::db_peer_candidate>();

            std::shared_ptr<byte_buf> peer_candidate_buf = std::make_shared<byte_buf>();
            peer_candidate_buf->write_to_byte_buf(it->value().data(), (uint32_t) it->value().size());
            dbc::network::binary_protocol proto(peer_candidate_buf.get());
            db_candidate->read(&proto);             //may exception

            variable_value val_ip(db_candidate->ip, false);
            if (!ip_vdr.validate(val_ip)) {
                LOG_ERROR << "load peer candidate error, ip is invalid: " << db_candidate->ip;
                continue;
            }

            variable_value val_port(std::to_string((unsigned int) (uint16_t) db_candidate->port), false);
            if (!port_vdr.validate(val_port)) {
                LOG_ERROR << "load peer candidate error, port is invalid: " << (uint16_t) db_candidate->port;
                continue;
            }

            if (db_candidate->node_id.empty()) {
                LOG_ERROR << "load peer candidate error, node_id is empty";
                continue;
            }

            std::shared_ptr<peer_candidate> candidate = std::make_shared<peer_candidate>();
            boost::asio::ip::address addr = boost::asio::ip::make_address(db_candidate->ip);
            candidate->tcp_ep = tcp::endpoint(addr, (uint16_t) db_candidate->port);
            candidate->net_st = ns_idle;
            candidate->reconn_cnt = 0;
            candidate->last_conn_tm = db_candidate->last_conn_tm;
            candidate->score = db_candidate->score;
            candidate->node_id = db_candidate->node_id;
            candidate->node_type = (peer_node_type) db_candidate->node_type;

            m_peer_candidates.push_back(candidate);
        }
    }
    catch (std::exception& e) {
        LOG_ERROR << "load peer candidates from db exception: " << e.what();
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::init_acceptor() {
    LOG_INFO << "p2p net service init net, ip: " << m_listen_ip << " port: " << m_listen_port;
    tcp::endpoint ep(ip::address::from_string(m_listen_ip), m_listen_port);

    int32_t ret = dbc::network::connection_manager::instance().start_listen(ep, &matrix_server_socket_channel_handler::create);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "p2p net service init net error, ip: " << m_listen_ip << " port: " << m_listen_port;
        return ret;
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::init_connector(bpo::variables_map &options) {
    std::vector<std::string> peer_addresses;
    const std::vector<std::string> &peer_conf_addresses = conf_manager::instance().get_peers();
    peer_addresses.insert(peer_addresses.begin(), peer_conf_addresses.begin(), peer_conf_addresses.end());

    ip_validator ip_vdr;
    port_validator port_vdr;

    for (auto it = peer_addresses.begin(); it != peer_addresses.end(); it++) {
        std::string addr = *it;
        util::trim(addr);
        size_t pos = addr.find_last_of(':');
        if (pos == std::string::npos) {
            LOG_ERROR << "peer ip invalid format: " << addr;
            continue;
        }

        std::string str_ip = addr.substr(0, pos);
        std::string str_port = addr.substr(pos + 1, std::string::npos);

        variable_value val;
        val.value() = str_ip;
        if (str_ip.empty() || !ip_vdr.validate(val)) {
            LOG_ERROR << "invalid ip: " << str_ip;
            continue;
        }

        val.value() = str_port;
        if (str_port.empty() || !port_vdr.validate(val)) {
            LOG_ERROR << "invalid port: " << str_port;
            continue;
        }

        uint16_t port = (uint16_t) atoi(str_port);

        try {
            LOG_INFO << "connect peer, ip: " << str_ip << " port: " << str_port;

            tcp::endpoint ep(ip::address::from_string(str_ip), port);
            if (exist_peer_node(ep)) {
                LOG_DEBUG << "peer is already exist: " << ep.address().to_string();
                continue;
            }

            int32_t ret = dbc::network::connection_manager::instance().start_connect(ep, &matrix_client_socket_channel_handler::create);
            if (E_SUCCESS != ret) {
                LOG_ERROR << "connect peer error, ip: " << str_ip << " port: " << str_port;
                continue;
            }

            if (is_peer_candidate_exist(ep)) {
                update_peer_candidate_state(ep, ns_in_use);
            } else {
                add_peer_candidate(ep, ns_in_use, NORMAL_NODE);
            }
        }
        catch (const std::exception &e) {
            LOG_ERROR << "connect peer exception: " << str_ip << ":" << str_port << ", " << e.what();
            continue;
        }
    }

    return E_SUCCESS;
}

bool p2p_net_service::is_peer_candidate_exist(tcp::endpoint &ep) {
    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end(),
                           [=](std::shared_ptr<peer_candidate> &pc) -> bool { return ep == pc->tcp_ep; });
    return it != m_peer_candidates.end();
}

bool p2p_net_service::add_peer_candidate(tcp::endpoint &ep, net_state ns, peer_node_type ntype, const std::string& node_id) {
    if (m_peer_candidates.size() >= MAX_PEER_CANDIDATES_CNT) {
        return false;
    }

    auto candidate = get_peer_candidate(ep);
    if (nullptr == candidate) {
        auto c = std::make_shared<peer_candidate>(ep, ns, ntype, 0, time(nullptr), 0, node_id);
        m_peer_candidates.emplace_back(c);
        return true;
    }

    return false;
}

std::shared_ptr<peer_candidate> p2p_net_service::get_peer_candidate(const tcp::endpoint &ep) {
    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end(),
                           [=](std::shared_ptr<peer_candidate> &candidate) -> bool {
                               return ep == candidate->tcp_ep;});

    return (it != m_peer_candidates.end()) ? *it : nullptr;
}

bool p2p_net_service::update_peer_candidate_state(tcp::endpoint &ep, net_state ns) {
    auto candidate = get_peer_candidate(ep);
    if (nullptr != candidate) {
        candidate->net_st = ns;
        return true;
    }

    return false;
}

// 检查与种子节点的连接
void p2p_net_service::on_timer_check_peer_candidates(const std::shared_ptr<core_timer>& timer) {
    // remove ns_failed status candidates
    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end();) {
        if (ns_failed == (*it)->net_st && (*it)->reconn_cnt >= max_reconnect_times) {
            it = m_peer_candidates.erase(it);
            continue;
        }

        it++;
    }

    if (get_maybe_available_peer_candidates_count() < MIN_PEER_CANDIDATES_COUNT) {
        add_dns_seeds();
    }

    if (get_maybe_available_peer_candidates_count() < MIN_PEER_CANDIDATES_COUNT) {
        add_hard_code_seeds();
    }

    uint32_t new_conn_cnt = 0;
    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (new_conn_cnt > max_connect_per_check) {
            break;
        }

        std::shared_ptr<peer_candidate> candidate = *it;
        if ((ns_idle == candidate->net_st)
            || ((ns_failed == candidate->net_st) && candidate->reconn_cnt < max_reconnect_times)) {
            try {
                candidate->last_conn_tm = time(nullptr);
                candidate->net_st = ns_in_use;
                candidate->reconn_cnt++;

                if (exist_peer_node(candidate->node_id) || exist_peer_node(candidate->tcp_ep)) {
                    candidate->reconn_cnt = 0;
                    continue;
                }

                if (conf_manager::instance().get_node_id() == candidate->node_id) {
                    candidate->net_st = ns_zombie;
                    continue;
                }

                LOG_INFO << "p2p net service connect peer candidate, peer_node_id: " << candidate->node_id
                         << ", peer_addr: " << candidate->tcp_ep;

                new_conn_cnt++;
                int32_t ret = dbc::network::connection_manager::instance().start_connect(candidate->tcp_ep,
                                                                &matrix_client_socket_channel_handler::create);
                if (E_SUCCESS != ret) {
                    LOG_ERROR << "peer connect error: " << candidate->tcp_ep;
                    candidate->net_st = ns_failed;
                }
            }
            catch (const std::exception &e) {
                candidate->net_st = ns_failed;
                LOG_ERROR << "peer connect exception: " << candidate->tcp_ep;
            }
        }
    }
}

void p2p_net_service::on_timer_dyanmic_adjust_network(const std::shared_ptr<core_timer>& timer) {
    uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(dbc::network::CLIENT_SOCKET);
    if (client_peer_nodes_count < max_connected_cnt) {
        uint32_t get_count = (uint32_t) (max_connected_cnt - client_peer_nodes_count);
        std::vector<std::shared_ptr<peer_candidate>> available_candidates;
        if (E_SUCCESS != get_available_peer_candidates(get_count, available_candidates)) {
            LOG_ERROR << "get available peer candidates error";
            return;
        }

        for (auto it : available_candidates) {
            if (E_SUCCESS != start_connect(it->tcp_ep)) {
                LOG_ERROR << "start connect peer_candidate error, addr:"
                          << it->tcp_ep.address().to_string() << ":" << it->tcp_ep.port();
                it->net_st = ns_failed;
            } else {
                it->net_st = ns_in_use;
            }
        }
    }
    else {
        uint32_t available_normal_nodes_count = get_available_peer_candidates_count_by_node_type(NORMAL_NODE);
        if (available_normal_nodes_count >= MIN_NORMAL_AVAILABLE_NODE_COUNT) {
            std::shared_ptr<peer_node> node = get_dynamic_disconnect_peer_node();
            if (nullptr == node) {
                LOG_DEBUG << "p2p net service does not find dynamic disconnect peer nodes";
                return;
            }

            m_peer_nodes_map.erase(node->m_id);
            dbc::network::connection_manager::instance().stop_channel(node->m_sid);

            auto connect_candidate = get_dynamic_connect_peer_candidate();
            if (nullptr == connect_candidate) {
                return;
            }

            //update net state to available after get connect candidate
            tcp::endpoint node_ep(address_v4::from_string(node->m_peer_addr.get_ip()),
                                  (uint16_t) node->m_peer_addr.get_port());
            auto candidate = get_peer_candidate(node_ep);
            if (nullptr != candidate) {
                candidate->net_st = ns_available;
                remove_peer_candidate(node_ep);
                m_peer_candidates.push_back(candidate);
            }

            if (E_SUCCESS != start_connect(connect_candidate->tcp_ep)) {
                LOG_ERROR << "start connect error: " << connect_candidate->tcp_ep.address() << ":"
                          << connect_candidate->tcp_ep.port();
                connect_candidate->net_st = ns_failed;
                return;
            } else {
                connect_candidate->net_st = ns_in_use;
                return;
            }
        }
    }
}

void p2p_net_service::on_timer_peer_info_exchange(const std::shared_ptr<core_timer>& timer) {
    send_put_peer_nodes(nullptr);
}

void p2p_net_service::on_timer_peer_candidate_dump(const std::shared_ptr<core_timer>& timer) {
    save_peer_candidates();
}

uint32_t p2p_net_service::get_maybe_available_peer_candidates_count() {
    uint32_t count = 0;

    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (ns_idle == (*it)->net_st
            || ns_in_use == (*it)->net_st
            || ns_available == (*it)->net_st
            || (((*it)->net_st == ns_failed) && ((*it)->reconn_cnt < max_reconnect_times))) {
            count++;
        }
    }

    return count;
}

bool p2p_net_service::exist_peer_node(tcp::endpoint ep) {
    dbc::network::endpoint_address addr(ep);

    if (addr.get_ip() == m_listen_ip && addr.get_port() == m_listen_port) {
        return true;
    }

    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
        if (it->second->m_peer_addr == addr)
            return true;
    }

    return false;
}

bool p2p_net_service::exist_peer_node(std::string &nid) {
    if (nid.empty())
        return false;

    auto it = m_peer_nodes_map.find(nid);
    if (it != m_peer_nodes_map.end())
        return true;

    return false;
}

bool p2p_net_service::add_peer_node(const std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "null ptr of msg";
        return false;
    }

    std::string nid;
    dbc::network::socket_id sid = msg->header.src_sid;
    int32_t core_version, protocol_version;
    dbc::network_address peer_addr;

    if (msg->get_name() == VER_REQ) {
        auto req_content = std::dynamic_pointer_cast<dbc::ver_req>(msg->content);
        if (!req_content) {
            LOG_ERROR << "ver_req req_content is nullptr";
            return false;
        }

        nid = req_content->body.node_id;
        core_version = req_content->body.core_version;
        protocol_version = req_content->body.protocol_version;
    } else if (msg->get_name() == VER_RESP) {
        auto rsp_content = std::dynamic_pointer_cast<dbc::ver_resp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "ver_resp, rsp_content is nullptr";
            return false;
        }
        nid = rsp_content->body.node_id;
        core_version = rsp_content->body.core_version;
        protocol_version = rsp_content->body.protocol_version;
    } else {
        LOG_ERROR << "add peer node unknown msg: " << msg->get_name();
        return false;
    }

    if (nid.empty()) {
        return false;
    }

    if (conf_manager::instance().get_node_id() == nid) {
        LOG_INFO << "peer is  my own, node_id: " << nid;
        return false;
    }

    if (m_peer_nodes_map.find(nid) != m_peer_nodes_map.end()) {
        LOG_WARNING << "peer_node is already exist, node_id: " << nid;
        return false;
    }

    tcp::endpoint ep;
    auto ptr_ch = dbc::network::connection_manager::instance().get_channel(sid);
    if (!ptr_ch) {
        LOG_ERROR << "not find in connected channels: " << sid.to_string();
        return false;
    }

    auto ptr_tcp_ch = std::dynamic_pointer_cast<dbc::network::tcp_socket_channel>(ptr_ch);
    if (ptr_tcp_ch) {
        ep = ptr_tcp_ch->get_remote_addr();
    } else {
        LOG_ERROR << nid << "not find in connected channels.";
        return false;
    }

    ptr_tcp_ch->set_remote_node_id(nid);
    //set remote node id of the tcp channel, and the node id will be used for efficient resp message transport.

    std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
    node->m_id = nid;
    node->m_sid = sid;
    node->m_core_version = core_version;
    node->m_protocol_version = protocol_version;
    node->m_connected_time = std::time(nullptr);
    node->m_live_time = 0;
    node->m_connection_status = CONNECTED;
    node->m_peer_addr = ep;
    node->m_local_addr = ptr_tcp_ch->get_local_addr();

    if (msg->get_name() == VER_RESP) {
        auto candidate = get_peer_candidate(ep);
        if (candidate) {
            node->m_node_type = candidate->node_type;
        }
    }

    m_peer_nodes_map[node->m_id] = node;

    LOG_INFO << "add a new peer_node: " << node->m_id << ", remote_addr: " << ep.address().to_string() << ":"
              << ep.port();

    return true;
}

void p2p_net_service::remove_peer_node(const std::string &id) {
    m_peer_nodes_map.erase(id);
    LOG_INFO << "remove node: " << id;
}

std::shared_ptr<peer_node> p2p_net_service::get_peer_node(const std::string &id) {
    auto it = m_peer_nodes_map.find(id);
    if (it == m_peer_nodes_map.end()) {
        return nullptr;
    }

    return it->second;
}

void p2p_net_service::on_tcp_channel_error(const std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "msg is nullptr";
        return;
    }

    //find and update peer candidate
    dbc::network::socket_id sid = msg->header.src_sid;
    auto err_msg = std::dynamic_pointer_cast<dbc::network::tcp_socket_channel_error_msg>(msg);
    if (!err_msg) {
        LOG_ERROR << "err_msg is nullptr: " << sid.to_string();
        return;
    }

    LOG_ERROR << "p2p net service received tcp channel error msg"
              << ", src_sid: " << sid.to_string()
              << ", addr:" << err_msg->ep.address().to_string() << ":" << err_msg->ep.port()
              << ", msg_name:" << err_msg->header.msg_name;

    auto candidate = get_peer_candidate(err_msg->ep);
    if (nullptr != candidate) {
        if (candidate->net_st != ns_zombie && candidate->net_st != ns_available) {
            candidate->last_conn_tm = time(nullptr);
            candidate->net_st = ns_failed;
            // move peer to the tail of candidate list
            remove_peer_candidate(err_msg->ep);
            m_peer_candidates.push_back(candidate);
        }
    }

    // rm peer_node
    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
        if (it->second->m_sid == sid) {
            LOG_INFO << "remove peer node, node_id: " << it->first
                     << ", addr: " << it->second->m_peer_addr.get_ip() << ":" << it->second->m_peer_addr.get_port()
                     << ", sid: " << sid.to_string();
            m_peer_nodes_map.erase(it);
            break;
        }
    }
}

void p2p_net_service::on_client_tcp_connect_notification(const std::shared_ptr<dbc::network::message> &msg) {
    auto notification_content = std::dynamic_pointer_cast<dbc::network::client_tcp_connect_notification>(msg);
    if (!notification_content) {
        LOG_ERROR << "notification_content is nullptr";
        return;
    }

    auto candidate = get_peer_candidate(notification_content->ep);
    if (nullptr == candidate) {
        LOG_ERROR << "a client tcp connection established, but not in m_peer_candidates, src_sid:"
                  << msg->header.src_sid.to_string();
        dbc::network::connection_manager::instance().release_connector(msg->header.src_sid);
        dbc::network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    LOG_INFO << "on_client_tcp_connect, addr: " << candidate->tcp_ep.address().to_string() << ":" << candidate->tcp_ep.port();

    if (dbc::network::CLIENT_CONNECT_SUCCESS == notification_content->status) {
        std::shared_ptr<dbc::ver_req> ver_req_content = std::make_shared<dbc::ver_req>();

        //header
        ver_req_content->header.__set_magic(conf_manager::instance().get_net_flag());
        ver_req_content->header.__set_msg_name(VER_REQ);
        ver_req_content->header.__set_nonce(util::create_nonce());
        std::map<std::string, std::string> exten_info;
        exten_info["capacity"] = conf_manager::instance().get_proto_capacity().to_string();
        std::string sign_msg = ver_req_content->header.nonce + conf_manager::instance().get_node_id();
        std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
        exten_info["sign"] = sign;
        exten_info["sign_algo"] = "ecdsa";
        time_t cur = std::time(nullptr);
        exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
        exten_info["origin_id"] = conf_manager::instance().get_node_id();
        ver_req_content->header.__set_exten_info(exten_info);

        //body
        ver_req_content->body.__set_node_id(conf_manager::instance().get_node_id());
        ver_req_content->body.__set_core_version(CORE_VERSION);
        ver_req_content->body.__set_protocol_version(PROTOCO_VERSION);
        ver_req_content->body.__set_time_stamp(std::time(nullptr));
        dbc::network_address addr_me;
        addr_me.__set_ip(SystemInfo::instance().get_publicip());
        addr_me.__set_port(m_listen_port);
        ver_req_content->body.__set_addr_me(addr_me);
        tcp::endpoint ep = std::dynamic_pointer_cast<dbc::network::client_tcp_connect_notification>(msg)->ep;
        dbc::network_address addr_you;
        addr_you.__set_ip(ep.address().to_string());
        addr_you.__set_port(ep.port());
        ver_req_content->body.__set_addr_you(addr_you);
        ver_req_content->body.__set_start_height(0);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_content(ver_req_content);
        req_msg->set_name(VER_REQ);
        req_msg->header.dst_sid = msg->header.src_sid;

        LOG_INFO << "send ver_req to peer, addr: " << addr_you.ip << ":" << addr_you.port
                 << ", local_sid: " << msg->header.dst_sid.to_string()
                 << ", remote_sid: " << msg->header.src_sid.to_string();

        dbc::network::connection_manager::instance().send_message(req_msg->header.dst_sid, req_msg);
    } else {
        candidate->last_conn_tm = time(nullptr);
        candidate->net_st = ns_failed;
    }

    dbc::network::connection_manager::instance().release_connector(msg->header.src_sid);
}

void p2p_net_service::on_ver_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto req_content = std::dynamic_pointer_cast<dbc::ver_req>(msg->content);
    if (!req_content) {
        LOG_ERROR << "req_content is nullptr";
        return;
    }

    if (!util::check_id(req_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return;
    }

    LOG_INFO << "recv ver_req"
             << ", from: " << req_content->body.addr_me.ip << ":" << req_content->body.addr_me.port
             << ", node_id:" << req_content->body.node_id;

    if (!add_peer_node(msg)) {
        LOG_ERROR << "add peer node failed";
        dbc::network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (req_content->header.exten_info.count("capacity")) {
        LOG_DEBUG << "save remote peer's capacity " << req_content->header.exten_info["capacity"];
        dbc::network::connection_manager::instance().set_proto_capacity(msg->header.src_sid,
                                                                        req_content->header.exten_info["capacity"]);
    }

    std::string sign_req_msg = req_content->header.nonce + req_content->body.node_id;
    if (!util::verify_sign(req_content->header.exten_info["sign"], sign_req_msg, req_content->body.node_id)) {
        LOG_ERROR << "verify sign failed, node_id:" << req_content->body.node_id;
        return;
    }

    std::shared_ptr<dbc::ver_resp> resp_content = std::make_shared<dbc::ver_resp>();
    //header
    resp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    resp_content->header.__set_msg_name(VER_RESP);
    resp_content->header.__set_nonce(util::create_nonce());
    std::map<std::string, std::string> exten_info;
    exten_info["capacity"] = conf_manager::instance().get_proto_capacity().to_string();
    std::string sign_msg = resp_content->header.nonce + conf_manager::instance().get_node_id();
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    resp_content->header.__set_exten_info(exten_info);

    //body
    resp_content->body.__set_node_id(conf_manager::instance().get_node_id());
    resp_content->body.__set_core_version(CORE_VERSION);
    resp_content->body.__set_protocol_version(PROTOCO_VERSION);

    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_content(resp_content);
    resp_msg->set_name(VER_RESP);
    resp_msg->header.dst_sid = msg->header.src_sid;

    dbc::network::connection_manager::instance().send_message(resp_msg->header.dst_sid, resp_msg);
}

void p2p_net_service::on_ver_resp(const std::shared_ptr<dbc::network::message> &msg) {
    auto resp_content = std::dynamic_pointer_cast<dbc::ver_resp>(msg->content);
    if (!resp_content) {
        LOG_ERROR << "resp_content is nullptr";
        return;
    }

    if (!util::check_id(resp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return;
    }

    std::string sign_msg = resp_content->header.nonce + resp_content->body.node_id;
    if (!util::verify_sign(resp_content->header.exten_info["sign"], sign_msg, resp_content->body.node_id)) {
        LOG_ERROR << "verify sign failed, node_id: " << resp_content->body.node_id;
        return;
    }

    LOG_INFO << "received ver_resp, node id: " << resp_content->body.node_id
              << ", core_ver=" << resp_content->body.core_version
              << ", protocol_ver=" << resp_content->body.protocol_version;

    auto ch = dbc::network::connection_manager::instance().get_channel(msg->header.src_sid);
    auto tcp_ch = std::dynamic_pointer_cast<dbc::network::tcp_socket_channel>(ch);
    if (nullptr == ch || nullptr == tcp_ch) {
        LOG_ERROR << "p2p net service on ver resp get channel error," << msg->header.src_sid.to_string()
                  << "node id: " << resp_content->body.node_id;
        return;
    }

    auto candidate = get_peer_candidate(tcp_ch->get_remote_addr());
    if (nullptr != candidate) {
        candidate->node_id = resp_content->body.node_id;
    } else {
        LOG_ERROR << "recv a ver resp, but it it NOT found in peer candidates:" << resp_content->body.node_id
                  << msg->header.src_sid.to_string();
    }

    //larger than max connected count, so just tag its status with available and stop channel
    uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(dbc::network::CLIENT_SOCKET);
    if (client_peer_nodes_count >= max_connected_cnt) {
        if (nullptr != candidate) {
            candidate->net_st = ns_available;
        }

        dbc::network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (!add_peer_node(msg)) {
        LOG_ERROR << "add peer node error, node_id: " << resp_content->body.node_id;
        dbc::network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (candidate) {
        candidate->reconn_cnt = 0;
    }

    if (resp_content->header.exten_info.count("capacity")) {
        dbc::network::connection_manager::instance().set_proto_capacity(msg->header.src_sid,
                                                                        resp_content->header.exten_info["capacity"]);
    }

    tcp::endpoint local_ep = tcp_ch->get_local_addr();
    advertise_local(local_ep, msg->header.src_sid);  //advertise local self address to neighbor peer node
}

void p2p_net_service::get_all_peer_nodes(peer_list_type &nodes) {
    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++) {
        nodes.push_back(it->second);
    }
}

void p2p_net_service::on_cmd_get_peer_nodes_req(const std::shared_ptr<dbc::network::message> &msg)
{
    auto cmd_resp = std::make_shared<cmd_get_peer_nodes_rsp>();
    cmd_resp->result = E_SUCCESS;
    cmd_resp->result_info = "";

    std::shared_ptr<dbc::network::base> content = msg->get_content();
    auto req = std::dynamic_pointer_cast<cmd_get_peer_nodes_req>(content);
    assert(nullptr != req && nullptr != content);
    COPY_MSG_HEADER(req,cmd_resp);
    if (!req || !content)
    {
        LOG_ERROR << "null ptr of cmd_get_peer_nodes_req";
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "internal error";
        topic_manager::instance().publish<void>(typeid(cmd_get_peer_nodes_rsp).name(), cmd_resp);

        return;
    }

    if(req->flag == flag_active)
    {
        for (auto itn = m_peer_nodes_map.begin(); itn != m_peer_nodes_map.end(); ++itn)
        {
            cmd_peer_node_info node_info;
            node_info.peer_node_id = itn->second->m_id;
            node_info.live_time_stamp = itn->second->m_live_time;
            node_info.addr.ip = itn->second->m_peer_addr.get_ip();
            node_info.addr.port = itn->second->m_peer_addr.get_port();
            node_info.node_type = (int8_t)itn->second->m_node_type;
            node_info.service_list.clear();
            node_info.service_list.push_back(std::string("ai_training"));
            cmd_resp->peer_nodes_list.push_back(std::move(node_info));
        }
    }
    else if(req->flag == flag_global)
    {
        //case: not too many peers
        for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
        {
            cmd_peer_node_info node_info;
            node_info.peer_node_id = (*it)->node_id;
            node_info.live_time_stamp = 0;
            node_info.net_st = (int8_t)(*it)->net_st;
            node_info.addr.ip = (*it)->tcp_ep.address().to_string();
            node_info.addr.port = (*it)->tcp_ep.port();
            node_info.node_type = (int8_t)(*it)->node_type;
            node_info.service_list.clear();
            node_info.service_list.push_back(std::string("ai_training"));
            cmd_resp->peer_nodes_list.push_back(std::move(node_info));
        }
    }

    topic_manager::instance().publish<void>(typeid(cmd_get_peer_nodes_rsp).name(), cmd_resp);
}

void p2p_net_service::on_get_peer_nodes_req(const std::shared_ptr<dbc::network::message> &msg) {
#if 0
    if (m_peer_nodes_map.size() == 0)
    {
        //ignore request
        return E_SUCCESS;
    }
    std::shared_ptr<matrix::service_core::get_peer_nodes_req> req = std::dynamic_pointer_cast<matrix::service_core::get_peer_nodes_req>(msg->content);
    if (!req)
    {
        LOG_ERROR << "recv get_peer_nodes_req, but req_content is null.";
        return E_DEFAULT;
    }

    if (id_generator::check_base58_id(req->header.nonce) != true)
    {
        LOG_DEBUG << "p2p_net_service on_get_peer_nodes_req. nonce error ";
        return E_DEFAULT;
    }

    const uint32_t max_peer_cnt_per_pack = 50;
    const uint32_t max_pack_cnt = 10;
    auto it = m_peer_nodes_map.begin();
    for (uint32_t i = 0; (i <= m_peer_nodes_map.size() / max_peer_cnt_per_pack) && (i < max_pack_cnt); i++)
    {
        //common header
        std::shared_ptr<message> resp_msg = std::make_shared<message>();
        resp_msg->header.msg_name  = P2P_GET_PEER_NODES_RESP;
        resp_msg->header.msg_priority = 0;
        resp_msg->header.dst_sid = msg->header.src_sid;
        auto resp_content = std::make_shared<matrix::service_core::get_peer_nodes_resp>();
        //header
        resp_content->header.__set_magic(conf_manager::instance().get_net_flag());
        resp_content->header.__set_msg_name(P2P_GET_PEER_NODES_RESP);
        resp_content->header.__set_nonce(id_generator::generate_nonce());

        for (uint32_t peer_cnt = 0; (it != m_peer_nodes_map.end()) && (peer_cnt < max_peer_cnt_per_pack); ++it)
        {
            //body
            if (it->second->m_id == conf_manager::instance().get_node_id())
            {
                resp_msg->header.src_sid = it->second->m_sid;
                continue;
            }
            matrix::service_core::peer_node_info info;
            assign_peer_info(info, it->second);
            info.service_list.push_back(std::string("ai_training"));
            resp_content->body.peer_nodes_list.push_back(std::move(info));
            ++peer_cnt;
        }

        resp_msg->set_content(resp_content);

        dbc::network::connection_manager::instance().send_message(msg->header.src_sid, resp_msg);
    }
#endif

    return;
}

void p2p_net_service::on_get_peer_nodes_resp(const std::shared_ptr<dbc::network::message> &msg) {
    if (m_peer_candidates.size() >= MAX_PEER_CANDIDATES_CNT) {
        return;
    }

    auto rsp = std::dynamic_pointer_cast<dbc::get_peer_nodes_resp>(msg->content);
    if (!rsp) {
        LOG_ERROR << "recv get_peer_nodes_resp, but req_content is nullptr";
        return;
    }

    if (!util::check_id(rsp->header.nonce)) {
        LOG_DEBUG << "nonce check failed";
        return;
    }

    //if advertise local ip, peer nodes list is just one and should relay to neighbor node
    if (1 == rsp->body.peer_nodes_list.size()) {
        const dbc::peer_node_info &node = rsp->body.peer_nodes_list[0];

        try {
            tcp::endpoint ep(address_v4::from_string(node.addr.ip), (uint16_t) node.addr.port);
            if (dbc::network::net_address::is_rfc1918(ep)) {
                LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
                return;
            }

            //add to peer candidates
            if (!add_peer_candidate(ep, ns_available, NORMAL_NODE, node.peer_node_id)) {
                LOG_ERROR << "add peer candidate error: " << ep;
            } else {
                LOG_DEBUG << "add peer candidate successfull: " << ep;
            }

            return;
        }
        catch (...) {
            LOG_ERROR << "recv a peer error: " << node.addr.ip << ", port: " << (uint16_t) node.addr.port
                      << ", node_id: " << node.peer_node_id << msg->header.src_sid.to_string();
            return;
        }
    }

    for (auto it = rsp->body.peer_nodes_list.begin(); it != rsp->body.peer_nodes_list.end(); ++it) {
        try {
            tcp::endpoint ep(address_v4::from_string(it->addr.ip), (uint16_t) it->addr.port);
            if (dbc::network::net_address::is_rfc1918(ep)) {
                LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
                continue;
            }

            auto candidate = get_peer_candidate(ep);
            if (nullptr == candidate) {
                if (!add_peer_candidate(ep, ns_idle, NORMAL_NODE)) {
                    LOG_ERROR << "add peer candidate error: " << ep;
                }

                LOG_INFO << "add peer candidate: "
                         << "addr: " << it->addr.ip << ":" << (uint16_t) it->addr.port
                         << ", node_id: " << it->peer_node_id;
            } else if (candidate->node_id.empty() && !it->peer_node_id.empty()) {
                candidate->node_id = it->peer_node_id;
            }
        }
        catch (...) {
            LOG_ERROR << "recv a peer(" << it->addr.ip << ":" << (uint16_t) it->addr.port << ")"
                      << ", but failed to parse.";
        }
    }
}

int32_t p2p_net_service::send_get_peer_nodes() {
    std::shared_ptr<dbc::get_peer_nodes_req> req_content = std::make_shared<dbc::get_peer_nodes_req>();
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(P2P_GET_PEER_NODES_REQ);
    req_content->header.__set_nonce(util::create_nonce());

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(P2P_GET_PEER_NODES_REQ);
    req_msg->set_content(req_content);
    dbc::network::connection_manager::instance().broadcast_message(req_msg);

    return E_SUCCESS;
}

int32_t p2p_net_service::send_put_peer_nodes(std::shared_ptr<peer_node> node) {
    //common header
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->header.msg_name = P2P_GET_PEER_NODES_RESP;
    resp_msg->header.msg_priority = 0;

    std::shared_ptr<dbc::get_peer_nodes_resp> resp_content = std::make_shared<dbc::get_peer_nodes_resp>();

    //header
    resp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    resp_content->header.__set_msg_name(P2P_GET_PEER_NODES_RESP);
    resp_content->header.__set_nonce(util::create_nonce());

    if (node)//broadcast one node
    {
        //body
        dbc::peer_node_info info;
        assign_peer_info(info, node);
        info.service_list.push_back(std::string("ai_training"));
        resp_content->body.peer_nodes_list.push_back(std::move(info));
        resp_msg->set_content(resp_content);

        dbc::network::connection_manager::instance().send_message(node->m_sid, resp_msg);
    } else// broadcast all nodes
    {
        int count = 0;
        for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
            if (nullptr == it->second || dbc::network::SERVER_SOCKET == it->second->m_sid.get_type() ||
                SEED_NODE == it->second->m_node_type) {
                continue;
            }

            dbc::peer_node_info info;
            assign_peer_info(info, it->second);
            info.service_list.push_back(std::string("ai_training"));
            resp_content->body.peer_nodes_list.push_back(std::move(info));

            if (++count >= MAX_SEND_PEER_NODES_COUNT) {
                LOG_DEBUG << "send peer nodes too many and break: " << m_peer_nodes_map.size();
                break;
            }
        }

        std::list<std::shared_ptr<peer_candidate> > tmp_candi_list;
        for (auto it = m_peer_candidates.begin(); (it != m_peer_candidates.end()) && (count < MAX_SEND_PEER_NODES_COUNT);) {
            if (ns_available == (*it)->net_st && NORMAL_NODE == (*it)->node_type) {
                dbc::peer_node_info info;
                info.peer_node_id = (*it)->node_id;
                info.live_time_stamp = (*it)->last_conn_tm;
                info.addr.ip = (*it)->tcp_ep.address().to_string();
                info.addr.port = (*it)->tcp_ep.port();
                info.service_list.push_back(std::string("ai_training"));
                resp_content->body.peer_nodes_list.push_back(std::move(info));
                ++count;
                tmp_candi_list.push_back(*it);
                it = m_peer_candidates.erase(it);
            } else {
                ++it;
            }
        }
        m_peer_candidates.insert(m_peer_candidates.end(), tmp_candi_list.begin(), tmp_candi_list.end());

        //case: make sure msg len not exceed MAX_BYTE_BUF_LEN(MAX_MSG_LEN)
        if (resp_content->body.peer_nodes_list.size() > 0) {
            resp_msg->set_content(resp_content);
            dbc::network::connection_manager::instance().broadcast_message(resp_msg);
        } else {
            return E_DEFAULT;
        }
    }

    return E_SUCCESS;
}

uint32_t p2p_net_service::get_peer_nodes_count_by_socket_type(dbc::network::socket_type type) {
    uint32_t count = 0;

    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++) {
        if (nullptr != it->second && type == it->second->m_sid.get_type()) {
            count++;
        }
    }

    return count;
}

int32_t p2p_net_service::get_available_peer_candidates(uint32_t count,
                                                       std::vector<std::shared_ptr<peer_candidate>> &available_candidates) {
    if (count == 0) {
        return E_SUCCESS;
    }

    available_candidates.clear();
    uint32_t i = 0;

    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (ns_available == (*it)->net_st && i < count) {
            available_candidates.push_back(*it);
            i++;
        }
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::save_peer_candidates() {
    if (m_peer_candidates.empty()) {
        return E_SUCCESS;
    }

    clear_peer_candidates_db();

    try {
        leveldb::WriteBatch batch;

        for (auto it : m_peer_candidates) {
            std::shared_ptr<dbc::db_peer_candidate> db_candidate = std::make_shared<dbc::db_peer_candidate>();

            db_candidate->__set_ip(it->tcp_ep.address().to_string());
            db_candidate->__set_port(it->tcp_ep.port());
            db_candidate->__set_net_state(it->net_st);
            db_candidate->__set_reconn_cnt(it->reconn_cnt);
            db_candidate->__set_last_conn_tm(it->last_conn_tm);
            db_candidate->__set_score(it->score);
            db_candidate->__set_node_id(it->node_id);
            db_candidate->__set_node_type(it->node_type);

            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            dbc::network::binary_protocol proto(out_buf.get());
            db_candidate->write(&proto);

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            batch.Put(db_candidate->ip, slice);
        }

        //flush to db
        leveldb::WriteOptions write_options;
        //write_options.sync = true;                    //no need sync

        leveldb::Status status = m_peers_candidates_db->Write(write_options, &batch);
        if (!status.ok()) {
            LOG_ERROR << "p2p net service save peer candidates error: " << status.ToString();
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }
    catch (...) {
        LOG_ERROR << "p2p net service save peer candidates error";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::clear_peer_candidates_db() {
    try {
        //iterate task in db
        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_peers_candidates_db->NewIterator(leveldb::ReadOptions()));

        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            m_peers_candidates_db->Delete(leveldb::WriteOptions(), it->key());
        }
    }
    catch (...) {
        LOG_ERROR << "p2p net service clear peer candidates db exception";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::add_dns_seeds() {
    try {
        if (m_dns_seeds.empty()) {
            m_dns_seeds.insert(m_dns_seeds.begin(), conf_manager::instance().get_dns_seeds().begin(),
                               conf_manager::instance().get_dns_seeds().end());
        }

        //get dns seeds
        const char *dns_seed = m_dns_seeds.front();
        m_dns_seeds.pop_front();

        if (nullptr == dns_seed) {
            LOG_ERROR << "p2p net service resolve dns nullptr";
            return E_DEFAULT;
        }

        io_service ios;
        ip::tcp::resolver rslv(ios);
        ip::tcp::resolver::query qry(dns_seed, boost::lexical_cast<std::string>(80));
        ip::tcp::resolver::iterator it = rslv.resolve(qry);
        ip::tcp::resolver::iterator end;

        for (; it != end; it++) {
            tcp::endpoint ep(it->endpoint().address(), conf_manager::instance().get_net_default_port());
            add_peer_candidate(ep, ns_idle, SEED_NODE);
        }
    }
    catch (const boost::exception &e) {
        LOG_ERROR << "p2p net service resolve dns error: " << diagnostic_information(e);
    }

    return E_SUCCESS;
}

int32_t p2p_net_service::add_hard_code_seeds() {
    //get hard code seeds
    for (auto it = m_hard_code_seeds.begin(); it != m_hard_code_seeds.end(); it++) {
        tcp::endpoint ep(ip::address::from_string(it->seed), it->port);
        add_peer_candidate(ep, ns_idle, SEED_NODE);
    }

    return E_SUCCESS;
}

uint32_t p2p_net_service::start_connect(const tcp::endpoint tcp_ep) {
    try {
        LOG_DEBUG << "matrix connect peer address; ip: " << tcp_ep.address() << " port: " << tcp_ep.port();

        if (exist_peer_node(tcp_ep)) {
            LOG_DEBUG << "tcp channel exist to: " << tcp_ep.address().to_string() << ":" << tcp_ep.port();
            return E_DEFAULT;
        }

        //start connect
        if (E_SUCCESS !=
            dbc::network::connection_manager::instance().start_connect(tcp_ep, &matrix_client_socket_channel_handler::create)) {
            LOG_ERROR << "matrix init connector invalid peer address, ip: " << tcp_ep.address() << " port: "
                      << tcp_ep.port();
            return E_DEFAULT;
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR << "timer connect ip catch exception. addr info: " << tcp_ep.address() << " port: "
                  << tcp_ep.port() << ", " << e.what();
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

uint32_t p2p_net_service::get_available_peer_candidates_count_by_node_type(peer_node_type node_type) {
    uint32_t count = 0;

    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (ns_available == (*it)->net_st && node_type == (*it)->node_type) {
            count++;
        }
    }

    return count;
}

std::shared_ptr<peer_node> p2p_net_service::get_dynamic_disconnect_peer_node() {
    if (m_peer_nodes_map.empty()) {
        return nullptr;
    }

    static uint32_t DISCONNECT_INTERVAL = 0;

    std::vector<std::shared_ptr<peer_node>> client_connect_peer_nodes;
    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++) {
        //seed node is disconnected prior
        if (SEED_NODE == it->second->m_node_type && dbc::network::CLIENT_SOCKET == it->second->m_sid.get_type()) {
            LOG_DEBUG << "p2p net service get disconnect seed peer node: " << it->second->m_id
                      << it->second->m_sid.to_string();
            return it->second;
        }

        //add to client nodes for later use
        if (dbc::network::CLIENT_SOCKET == it->second->m_sid.get_type()) {
            client_connect_peer_nodes.push_back(it->second);
        }
    }

    if (client_connect_peer_nodes.empty()) {
        return nullptr;
    }

    //disconnect one normal node per five minutes
    if (++DISCONNECT_INTERVAL % DISCONNECT_NODE_PER_MINUTES) {
        LOG_DEBUG << "it is not time to disconnect normal node and have a rest: " << DISCONNECT_INTERVAL;
        return nullptr;
    }

    //random choose client connect peer nodes to disconnect
    uint32_t rand_num = get_rand32() % client_connect_peer_nodes.size();
    std::shared_ptr<peer_node> disconnect_node = client_connect_peer_nodes[rand_num];
    LOG_DEBUG << "p2p net service get disconnect normal peer node: " << disconnect_node->m_id
              << disconnect_node->m_sid.to_string();

    return disconnect_node;
}

std::shared_ptr<peer_candidate> p2p_net_service::get_dynamic_connect_peer_candidate() {
    std::vector<std::shared_ptr<peer_candidate>> seed_node_candidates;
    std::vector<std::shared_ptr<peer_candidate>> normal_node_candidates;

    for (auto it : m_peer_candidates) {
        if (nullptr != get_peer_node(it->node_id)) {
            continue;
        }

        if (ns_available == it->net_st && SEED_NODE == it->node_type) {
            seed_node_candidates.push_back(it);
        }

        if (ns_available == it->net_st && NORMAL_NODE == it->node_type) {
            normal_node_candidates.push_back(it);
        }
    }

    //normal good candidate prior
    if (normal_node_candidates.size() > 0) {
        uint32_t rand_num = get_rand32() % normal_node_candidates.size();
        std::shared_ptr<peer_candidate> connect_candidate = normal_node_candidates[rand_num];
        return connect_candidate;
    }

    //seed good candidate next
    if (seed_node_candidates.size() > 0) {
        uint32_t rand_num = get_rand32() % seed_node_candidates.size();
        std::shared_ptr<peer_candidate> connect_candidate = seed_node_candidates[rand_num];
        return connect_candidate;
    }

    return nullptr;
}

void p2p_net_service::advertise_local(tcp::endpoint tcp_ep, dbc::network::socket_id sid) {
    if (dbc::network::net_address::is_rfc1918(tcp_ep)) {
        LOG_DEBUG << "ip address is RFC1918 private network ip and will not advertise local: "
                  << tcp_ep.address().to_string();
        return;
    }

    std::shared_ptr<peer_node> node = std::make_shared<peer_node>();

    node->m_id = conf_manager::instance().get_node_id();
    node->m_live_time = time(nullptr);
    node->m_peer_addr = tcp::endpoint(tcp_ep.address(), m_listen_port);
    node->m_sid = sid;

    LOG_DEBUG << "p2p net service advertise local self address: " << tcp_ep.address().to_string() << ", port: "
              << m_listen_port;
    send_put_peer_nodes(node);
}

void p2p_net_service::remove_peer_candidate(const tcp::endpoint &ep) {
    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end(),
                           [=](std::shared_ptr<peer_candidate> &candidate) -> bool {
                               return ep == candidate->tcp_ep;
                           });

    if (it != m_peer_candidates.end()) {
        m_peer_candidates.erase(it);
        LOG_DEBUG << "p2p net service remove peer candidate: " << ep;
    }
}
