#include "p2p_net_service.h"
#include <cassert>
#include "server/server.h"
#include "config/conf_manager.h"
#include "network/tcp_acceptor.h"
#include "network/protocol/net_message_def.h"
#include "socket_channel_handler/client_socket_channel_handler.h"
#include "socket_channel_handler/server_socket_channel_handler.h"
#include "network/channel/tcp_socket_channel.h"
#include "timer/timer_def.h"
#include "db/db_types/db_peer_candidate_types.h"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "network/protocol/thrift_binary.h"
#include "util/system_info.h"
#include "config/BareMetalNodeManager.h"

#define CHECK_PEER_CANDIDATES_TIMER                 "p2p_timer_check_peer_candidates"
#define DYANMIC_ADJUST_NETWORK_TIMER                "p2p_timer_dynamic_adjust_network"
#define PEER_INFO_EXCHANGE_TIMER                    "p2p_timer_peer_info_exchange"
#define DUMP_PEER_CANDIDATES_TIMER                  "p2p_timer_dump_peer_candidates"

const uint32_t max_reconnect_times = 1;
const uint32_t max_connected_peer_nodes_count = 16;
const uint32_t max_peer_candidate_per_check = 16;
const uint32_t max_peer_candidates_count = 64;
const uint32_t min_peer_candidates_count = 8;
const uint32_t max_broadcast_peer_nodes_count = 16;

#define MIN_NORMAL_AVAILABLE_NODE_COUNT          2
#define DISCONNECT_NODE_PER_MINUTES              5

namespace fs = boost::filesystem;

ERRCODE p2p_net_service::init() {
    service_module::init();

    m_rand_seed = uint256();
    m_rand_ctx = FastRandomContext(m_rand_seed);

    if (ERR_SUCCESS != init_conf()) {
        LOG_ERROR << "init conf error";
        return ERR_ERROR;
    }

    if (ERR_SUCCESS != init_db()) {
        LOG_ERROR << "init db error";
        return ERR_ERROR;
    }
    
	if (ERR_SUCCESS != start_acceptor()) {
		LOG_ERROR << "start acceptor error";
		return ERR_ERROR;
	}

	if (ERR_SUCCESS != start_connector()) {
		LOG_ERROR << "start connector error";
		return ERR_ERROR;
	}

    return ERR_SUCCESS;
}

void p2p_net_service::exit() {
    service_module::exit();
}

void p2p_net_service::init_timer() {
    // 30s
    add_timer(CHECK_PEER_CANDIDATES_TIMER, 5 * 1000, 30 * 1000, ULLONG_MAX, "",
        CALLBACK_1(p2p_net_service::on_timer_check_peer_candidates, this));

    // 1min
    add_timer(PEER_INFO_EXCHANGE_TIMER, 60 * 1000, 60 * 1000, ULLONG_MAX, "",
        CALLBACK_1(p2p_net_service::on_timer_peer_info_exchange, this));

    // 1min
    add_timer(DYANMIC_ADJUST_NETWORK_TIMER, 60 * 1000, 60 * 1000, ULLONG_MAX, "",
        CALLBACK_1(p2p_net_service::on_timer_dyanmic_adjust_network, this));

    // 2min
    add_timer(DUMP_PEER_CANDIDATES_TIMER, 60 * 1000, 2 * 60 * 1000, ULLONG_MAX, "",
        CALLBACK_1(p2p_net_service::on_timer_peer_candidate_dump, this));
}

void p2p_net_service::init_invoker() {
    reg_msg_handle(TCP_CHANNEL_ERROR, CALLBACK_1(p2p_net_service::on_tcp_channel_error, this));
    reg_msg_handle(CLIENT_CONNECT_NOTIFICATION, CALLBACK_1(p2p_net_service::on_client_tcp_connect_notify, this));
    reg_msg_handle(VER_REQ, CALLBACK_1(p2p_net_service::on_ver_req, this));
    reg_msg_handle(VER_RESP, CALLBACK_1(p2p_net_service::on_ver_resp, this));
    reg_msg_handle(P2P_GET_PEER_NODES_RESP, CALLBACK_1(p2p_net_service::on_broadcast_peer_nodes, this));
}

ERRCODE p2p_net_service::init_conf() {
    m_listen_ip = ConfManager::instance().GetNetListenIp();
    m_listen_port = (uint16_t) ConfManager::instance().GetNetListenPort();

    auto& conf_dns_seeds = ConfManager::instance().GetInternalDnsSeeds();
    m_dns_seeds.insert(m_dns_seeds.begin(), conf_dns_seeds.begin(), conf_dns_seeds.end());

    auto& conf_ip_seeds = ConfManager::instance().GetInternalIpSeeds();
    m_ip_seeds.insert(m_ip_seeds.begin(), conf_ip_seeds.begin(), conf_ip_seeds.end());

    return ERR_SUCCESS;
}

ERRCODE p2p_net_service::init_db() {
    fs::path db_file_path = EnvManager::instance().get_db_path();
    if (!m_peers_candidates_db.init(db_file_path, "peers.db")) {
        LOG_ERROR << "init peer_candidate_db failed";
        return ERR_ERROR;
    }

    load_peer_candidates_from_db();

    return ERR_SUCCESS;
}

void p2p_net_service::load_peer_candidates_from_db() {
    m_peer_candidates.clear();

    std::map<std::string, std::shared_ptr<dbc::db_peer_candidate>> datas;
    m_peers_candidates_db.load_datas(datas);

    for (auto& it : datas) {
        ip_validator ip_vdr;
        port_validator port_vdr;

        variable_value val_ip(it.second->ip, false);
        if (!ip_vdr.validate(val_ip)) {
            m_peers_candidates_db.delete_data(it.second->ip);
            continue;
        }

        variable_value val_port(std::to_string((uint16_t)it.second->port), false);
        if (!port_vdr.validate(val_port)) {
            m_peers_candidates_db.delete_data(it.second->ip);
            continue;
        }

        if (it.second->node_id.empty()) {
            m_peers_candidates_db.delete_data(it.second->ip);
            continue;
        }

        std::shared_ptr<peer_candidate> candidate = std::make_shared<peer_candidate>();
        candidate->tcp_ep = tcp::endpoint(ip::address::from_string(it.second->ip), (uint16_t)it.second->port);
        candidate->net_st = ns_idle;
        candidate->reconn_cnt = 0;
        candidate->last_conn_tm = it.second->last_conn_tm;
        candidate->score = it.second->score;
        candidate->node_id = it.second->node_id;
        candidate->node_type = (peer_node_type)it.second->node_type;

        m_peer_candidates.push_back(candidate);
    }
}

ERRCODE p2p_net_service::start_acceptor() {
    tcp::endpoint ep(ip::address::from_string(m_listen_ip), m_listen_port);
    int32_t ret = network::connection_manager::instance().start_listen(ep, &matrix_server_socket_channel_handler::create);
    if (ERR_SUCCESS != ret) {
        LOG_ERROR << "start acceptor error: listen_ip=" << m_listen_ip << ", listen_port=" << m_listen_port;
        return ret;
    }

    return ERR_SUCCESS;
}

ERRCODE p2p_net_service::start_connector() {
    std::vector<std::string> peers;
    auto& conf_peers = ConfManager::instance().GetPeers();
    peers.insert(peers.begin(), conf_peers.begin(), conf_peers.end());

    for (auto it = peers.begin(); it != peers.end(); it++) {
        std::vector<std::string> vec;
        util::split(*it, ":", vec);
        std::string str_ip = vec[0];
        util::trim(str_ip);
        std::string str_port = vec[1];
        util::trim(str_port);
        uint16_t port = (uint16_t) atoi(str_port);
        
        tcp::endpoint ep(ip::address::from_string(str_ip), port);
        if (exist_peer_node(ep)) {
            continue;
        }

        LOG_INFO << "connect peer: addr=" << str_ip << " : " << str_port;

        ERRCODE ret = network::connection_manager::instance().start_connect(ep, 
            &matrix_client_socket_channel_handler::create);
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "connect peer error " << str_ip << " : " << str_port;
            continue;
        }

        if (is_peer_candidate_exist(ep)) {
            update_peer_candidate_state(ep, ns_in_use);
        } else {
            add_peer_candidate(ep, ns_in_use, PEER_NORMAL_NODE);
        }
    }

    return ERR_SUCCESS;
}


bool p2p_net_service::is_peer_candidate_exist(tcp::endpoint &ep) {
    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end(),
                           [=](std::shared_ptr<peer_candidate> &pc) -> bool { return ep == pc->tcp_ep; });
    return it != m_peer_candidates.end();
}

bool p2p_net_service::add_peer_candidate(tcp::endpoint &ep, net_state ns, peer_node_type ntype, 
    const std::string& node_id) {
    if (m_peer_candidates.size() >= max_peer_candidates_count) {
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

void p2p_net_service::remove_peer_candidate(const tcp::endpoint& ep) {
    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end(),
        [=](std::shared_ptr<peer_candidate>& candidate) -> bool {
            return ep == candidate->tcp_ep;
        });

    if (it != m_peer_candidates.end()) {
        m_peer_candidates.erase(it);
    }
}

bool p2p_net_service::update_peer_candidate_state(tcp::endpoint &ep, net_state state) {
    auto candidate = get_peer_candidate(ep);
    if (nullptr != candidate) {
        candidate->net_st = state;
        return true;
    }

    return false;
}

int32_t p2p_net_service::get_maybe_available_peer_candidates_count() {
    int32_t count = 0;

    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (ns_idle == (*it)->net_st || ns_in_use == (*it)->net_st || ns_available == (*it)->net_st
            || ((*it)->net_st == ns_failed && (*it)->reconn_cnt < max_reconnect_times)) {
            count++;
        }
    }

    return count;
}


bool p2p_net_service::exist_peer_node(tcp::endpoint ep) {
    network::net_address addr(ep);
    if (addr.get_ip() == m_listen_ip && addr.get_port() == m_listen_port) {
        return true;
    }

    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
        if (it->second->m_peer_addr == addr)
            return true;
    }

    return false;
}

bool p2p_net_service::exist_peer_node(std::string& node_id) {
    if (node_id.empty())
        return false;

    auto it = m_peer_nodes_map.find(node_id);
    if (it != m_peer_nodes_map.end())
        return true;
    else
        return false;
}

bool p2p_net_service::add_peer_node(const std::shared_ptr<network::message>& msg) {
    if (!msg) {
        return false;
    }

    std::string peer_node_id;
    network::socket_id sid = msg->header.src_sid;
    int32_t core_version, protocol_version;
    dbc::network_address peer_addr;

    if (msg->get_name() == VER_REQ) {
        auto req_content = std::dynamic_pointer_cast<dbc::ver_req>(msg->content);
        if (!req_content) {
            LOG_ERROR << "ver_req req_content is nullptr";
            return false;
        }

        peer_node_id = req_content->body.node_id;
        core_version = req_content->body.core_version;
        protocol_version = req_content->body.protocol_version;
        peer_addr = req_content->body.addr_me;
    }
    else if (msg->get_name() == VER_RESP) {
        auto rsp_content = std::dynamic_pointer_cast<dbc::ver_resp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "ver_resp, rsp_content is nullptr";
            return false;
        }
        peer_node_id = rsp_content->body.node_id;
        core_version = rsp_content->body.core_version;
        protocol_version = rsp_content->body.protocol_version;
    }
    else {
        LOG_ERROR << "add peer node unknown msg: " << msg->get_name();
        return false;
    }

    if (peer_node_id.empty()) {
        return false;
    }

    if (ConfManager::instance().GetNodeId() == peer_node_id) {
        LOG_INFO << "peer is  my own, node_id: " << peer_node_id;
        return false;
    }

    if (m_peer_nodes_map.find(peer_node_id) != m_peer_nodes_map.end()) {
        LOG_WARNING << "peer_node is already exist, node_id: " << peer_node_id;
        return false;
    }

    tcp::endpoint ep;
    auto ptr_ch = network::connection_manager::instance().get_channel(sid);
    if (!ptr_ch) {
        LOG_ERROR << "not find in connected channels: " << sid.to_string();
        return false;
    }

    auto ptr_tcp_ch = std::dynamic_pointer_cast<network::tcp_socket_channel>(ptr_ch);
    if (ptr_tcp_ch) {
        // TODO: remote_addr 不对
        ep = ptr_tcp_ch->get_remote_addr();
    }
    else {
        LOG_ERROR << peer_node_id << "not find in connected channels.";
        return false;
    }

    ptr_tcp_ch->set_remote_node_id(peer_node_id);

    std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
    node->m_id = peer_node_id;
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

    LOG_INFO << "add a new peer_node: node_id=" << peer_node_id << ", remote_addr=" << ep;

    return true;
}

void p2p_net_service::remove_peer_node(const std::string& node_id) {
    m_peer_nodes_map.erase(node_id);
}

std::shared_ptr<peer_node> p2p_net_service::get_peer_node(const std::string& node_id) {
    auto it = m_peer_nodes_map.find(node_id);
    if (it == m_peer_nodes_map.end()) {
        return nullptr;
    }

    return it->second;
}


void p2p_net_service::on_timer_check_peer_candidates(const std::shared_ptr<core_timer>& timer) {
    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end();) {
        if (ns_failed == (*it)->net_st && (*it)->reconn_cnt >= max_reconnect_times) {
            it = m_peer_candidates.erase(it);
            continue;
        }

        it++;
    }

    if (get_maybe_available_peer_candidates_count() < min_peer_candidates_count) {
        add_dns_seeds();
    }

    if (get_maybe_available_peer_candidates_count() < min_peer_candidates_count) {
        add_ip_seeds();
    }

    uint32_t new_conn_cnt = 0;
    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (new_conn_cnt > max_peer_candidate_per_check) {
            break;
        }

        std::shared_ptr<peer_candidate> candidate = *it;

        if (ns_idle == candidate->net_st
            || (ns_failed == candidate->net_st && candidate->reconn_cnt < max_reconnect_times)) {
            if (exist_peer_node(candidate->node_id) || exist_peer_node(candidate->tcp_ep)) {
                candidate->reconn_cnt = 0;
                continue;
            }

            if (ConfManager::instance().GetNodeId() == candidate->node_id) {
                candidate->net_st = ns_zombie;
                continue;
            }

            candidate->net_st = ns_in_use;
            candidate->reconn_cnt++;
            candidate->last_conn_tm = time(nullptr);

            LOG_INFO << "connect peer_candidate: addr=" << candidate->tcp_ep 
                << ", node_id=" << candidate->node_id
                << ", reconn_cnt=" << candidate->reconn_cnt;

            new_conn_cnt++;
            ERRCODE ret = network::connection_manager::instance().start_connect(candidate->tcp_ep,
                                                    &matrix_client_socket_channel_handler::create);
            if (ERR_SUCCESS != ret) {
                LOG_ERROR << "connect peer_candidate error: " << candidate->tcp_ep
                    << ", node_id=" << candidate->node_id
                    << ", reconn_cnt=" << candidate->reconn_cnt;
                candidate->net_st = ns_failed;
            }
        }
    }
}

void p2p_net_service::on_timer_peer_info_exchange(const std::shared_ptr<core_timer>& timer) {
    broadcast_peer_nodes(nullptr);
}

void p2p_net_service::on_timer_dyanmic_adjust_network(const std::shared_ptr<core_timer>& timer) {
    uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(network::CLIENT_SOCKET);
    if (client_peer_nodes_count < max_connected_peer_nodes_count) {
        uint32_t can_connect_count = (uint32_t) (max_connected_peer_nodes_count - client_peer_nodes_count);
        std::vector<std::shared_ptr<peer_candidate>> available_candidates;
        if (ERR_SUCCESS != get_available_peer_candidates(can_connect_count, available_candidates)) {
            LOG_ERROR << "get available peer candidates error";
            return;
        }

        for (auto it : available_candidates) {
            if (ERR_SUCCESS != start_connect(it->tcp_ep)) {
                LOG_ERROR << "start connect peer_candidate error, addr:" << it->tcp_ep;
                it->net_st = ns_failed;
            } else {
                it->net_st = ns_in_use;
            }
        }
    }
    else {
        uint32_t available_normal_nodes_count = get_available_peer_candidates_count_by_node_type(PEER_NORMAL_NODE);
        if (available_normal_nodes_count >= MIN_NORMAL_AVAILABLE_NODE_COUNT) {
            std::shared_ptr<peer_node> node = get_dynamic_disconnect_peer_node();
            if (nullptr == node) {
                LOG_DEBUG << "p2p net service does not find dynamic disconnect peer nodes";
                return;
            }

            m_peer_nodes_map.erase(node->m_id);
            network::connection_manager::instance().stop_channel(node->m_sid);

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

            if (ERR_SUCCESS != start_connect(connect_candidate->tcp_ep)) {
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

void p2p_net_service::on_timer_peer_candidate_dump(const std::shared_ptr<core_timer>& timer) {
    save_peer_candidates();
}


uint32_t p2p_net_service::start_connect(const tcp::endpoint tcp_ep) {
    try {
        LOG_DEBUG << "matrix connect peer address; ip: " << tcp_ep.address() << " port: " << tcp_ep.port();

        if (exist_peer_node(tcp_ep)) {
            LOG_DEBUG << "tcp channel exist to: " << tcp_ep.address().to_string() << ":" << tcp_ep.port();
            return E_DEFAULT;
        }

        //start connect
        if (ERR_SUCCESS !=
            network::connection_manager::instance().start_connect(tcp_ep, &matrix_client_socket_channel_handler::create)) {
            LOG_ERROR << "matrix init connector invalid peer address, ip: " << tcp_ep.address() << " port: "
                << tcp_ep.port();
            return E_DEFAULT;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR << "timer connect ip catch exception. addr info: " << tcp_ep.address() << " port: "
            << tcp_ep.port() << ", " << e.what();
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}


void p2p_net_service::add_dns_seeds() {
    try {
        if (m_dns_seeds.empty()) {
            return;
        }

        std::string dns_seed = m_dns_seeds.front();
        m_dns_seeds.pop_front();
        
        io_service ios;
        ip::tcp::resolver rslv(ios);
        ip::tcp::resolver::query qry(dns_seed, boost::lexical_cast<std::string>(80));
        ip::tcp::resolver::iterator it = rslv.resolve(qry);
        ip::tcp::resolver::iterator end;
        for (; it != end; it++) {
            tcp::endpoint ep(it->endpoint().address(), it->endpoint().port());
            add_peer_candidate(ep, ns_idle, PEER_SEED_NODE);
        }
    }
    catch (const boost::exception& e) {
        LOG_ERROR << "add_dns_seeds exception: " << diagnostic_information(e);
    }
}

void p2p_net_service::add_ip_seeds() {
    for (auto it = m_ip_seeds.begin(); it != m_ip_seeds.end(); it++) {
        std::vector<std::string> vec;
        util::split(*it, ":", vec);
        if (vec.size() != 2) continue;

        std::string str_ip = vec[0];
        util::trim(str_ip);
        std::string str_port = vec[1];
        util::trim(str_port);

        tcp::endpoint ep(ip::address::from_string(str_ip), (uint16_t) atoi(str_port.c_str()));
        add_peer_candidate(ep, ns_idle, PEER_SEED_NODE);
    }
}


void p2p_net_service::on_client_tcp_connect_notify(const std::shared_ptr<network::message> &msg) {
    auto notify_content = std::dynamic_pointer_cast<network::client_tcp_connect_notification>(msg);
    if (!notify_content) {
        return;
    }

    auto candidate = get_peer_candidate(notify_content->ep);
    if (nullptr == candidate) {
        network::connection_manager::instance().release_connector(msg->header.src_sid);
        network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (network::CLIENT_CONNECT_SUCCESS == notify_content->status) {
        send_ver_req(notify_content->ep, msg->header.src_sid);
    } else {
        candidate->net_st = ns_failed;
    }

    network::connection_manager::instance().release_connector(msg->header.src_sid);
}

void p2p_net_service::on_tcp_channel_error(const std::shared_ptr<network::message> &msg) {
    if (!msg) {
        return;
    }

    //find and update peer candidate
    network::socket_id sid = msg->header.src_sid;
    auto err_msg = std::dynamic_pointer_cast<network::tcp_socket_channel_error_msg>(msg);
    if (!err_msg) {
        return;
    }

    LOG_INFO << "tcp_channel_error "
             << " remote_addr: " << err_msg->ep.address().to_string() << ":" << err_msg->ep.port();

    auto candidate = get_peer_candidate(err_msg->ep);
    if (nullptr != candidate) {
        if (candidate->net_st != ns_zombie && candidate->net_st != ns_available) {
            candidate->last_conn_tm = time(nullptr);
            candidate->net_st = ns_failed;
            // move error peer_candidate to the tail of candidate list
            remove_peer_candidate(err_msg->ep);
            m_peer_candidates.push_back(candidate);
        }
    }

    // rm error peer_node
    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
        if (it->second->m_sid == sid) {
            m_peer_nodes_map.erase(it);
            break;
        }
    }
}


void p2p_net_service::send_ver_req(const tcp::endpoint& ep, const network::socket_id& sid) {
    std::shared_ptr<dbc::ver_req> ver_req_content = std::make_shared<dbc::ver_req>();
    //header
    ver_req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    ver_req_content->header.__set_msg_name(VER_REQ);
    ver_req_content->header.__set_nonce(util::create_nonce());
    std::map<std::string, std::string> exten_info;
    exten_info["capacity"] = ConfManager::instance().get_proto_capacity().to_string();
    std::string sign_msg = ver_req_content->header.nonce + ConfManager::instance().GetNodeId();
    std::string sign = util::sign(sign_msg, ConfManager::instance().GetNodePrivateKey());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = ConfManager::instance().GetNodeId();
    ver_req_content->header.__set_exten_info(exten_info);
    //body
    ver_req_content->body.__set_node_id(ConfManager::instance().GetNodeId());
    ver_req_content->body.__set_core_version(CORE_VERSION);
    ver_req_content->body.__set_protocol_version(PROTOCO_VERSION);
    ver_req_content->body.__set_time_stamp(std::time(nullptr));
    dbc::network_address addr_me;
    addr_me.__set_ip(SystemInfo::instance().GetPublicip());
    addr_me.__set_port(m_listen_port);
    ver_req_content->body.__set_addr_me(addr_me);
    dbc::network_address addr_you;
    addr_you.__set_ip(ep.address().to_string());
    addr_you.__set_port(ep.port());
    ver_req_content->body.__set_addr_you(addr_you);
    ver_req_content->body.__set_start_height(0);

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_content(ver_req_content);
    req_msg->set_name(VER_REQ);
    req_msg->header.dst_sid = sid;

    LOG_INFO << "send ver_req to peer_node " << addr_you.ip << ":" << addr_you.port;

    network::connection_manager::instance().send_message(sid, req_msg);
}

void p2p_net_service::on_ver_req(const std::shared_ptr<network::message> &msg) {
    auto req_content = std::dynamic_pointer_cast<dbc::ver_req>(msg->content);
    if (!req_content) {
        return;
    }

    if (!util::check_id(req_content->header.nonce)) {
        LOG_ERROR << "nonce check_id failed";
        return;
    }

    LOG_INFO << "recv ver_req"
             << " from: " << req_content->body.addr_me.ip << ":" << req_content->body.addr_me.port
             << ", node_id:" << req_content->body.node_id
             << ", core_ver: " << req_content->body.core_version
             << ", protocol_ver: " << req_content->body.protocol_version;

    if (!add_peer_node(msg)) {
        LOG_ERROR << "add peer node failed";
        network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (req_content->header.exten_info.count("capacity")) {
        LOG_DEBUG << "save remote peer's capacity " << req_content->header.exten_info["capacity"];
        network::connection_manager::instance().set_proto_capacity(msg->header.src_sid,
                                                                   req_content->header.exten_info["capacity"]);
    }

    std::string sign_req_msg = req_content->header.nonce + req_content->body.node_id;
    if (!util::verify_sign(req_content->header.exten_info["sign"], sign_req_msg, req_content->body.node_id)) {
        LOG_ERROR << "verify sign failed, node_id:" << req_content->body.node_id;
        return;
    }

    send_ver_resp(msg->header.src_sid);
}

void p2p_net_service::send_ver_resp(const network::socket_id& sid) {
    std::shared_ptr<dbc::ver_resp> resp_content = std::make_shared<dbc::ver_resp>();
    //header
    resp_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    resp_content->header.__set_msg_name(VER_RESP);
    resp_content->header.__set_nonce(util::create_nonce());
    std::map<std::string, std::string> exten_info;
    exten_info["capacity"] = ConfManager::instance().get_proto_capacity().to_string();
    std::string sign_msg = resp_content->header.nonce + ConfManager::instance().GetNodeId();
    std::string sign = util::sign(sign_msg, ConfManager::instance().GetNodePrivateKey());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = ConfManager::instance().GetNodeId();
    resp_content->header.__set_exten_info(exten_info);

    //body
    resp_content->body.__set_node_id(ConfManager::instance().GetNodeId());
    resp_content->body.__set_core_version(CORE_VERSION);
    resp_content->body.__set_protocol_version(PROTOCO_VERSION);

    std::shared_ptr<network::message> resp_msg = std::make_shared<network::message>();
    resp_msg->set_content(resp_content);
    resp_msg->set_name(VER_RESP);
    resp_msg->header.dst_sid = sid;

    network::connection_manager::instance().send_message(resp_msg->header.dst_sid, resp_msg);
}

void p2p_net_service::on_ver_resp(const std::shared_ptr<network::message> &msg) {
    auto resp_content = std::dynamic_pointer_cast<dbc::ver_resp>(msg->content);
    if (!resp_content) {
        return;
    }

    if (!util::check_id(resp_content->header.nonce)) {
        LOG_ERROR << "nonce check_id failed";
        return;
    }

    std::string sign_msg = resp_content->header.nonce + resp_content->body.node_id;
    if (!util::verify_sign(resp_content->header.exten_info["sign"], sign_msg, resp_content->body.node_id)) {
        LOG_ERROR << "verify sign failed, node_id: " << resp_content->body.node_id;
        return;
    }

    auto ch = network::connection_manager::instance().get_channel(msg->header.src_sid);
    auto tcp_ch = std::dynamic_pointer_cast<network::tcp_socket_channel>(ch);
    if (nullptr == ch || nullptr == tcp_ch) {
        LOG_ERROR << "p2p net service on ver resp get channel error," << msg->header.src_sid.to_string()
                  << "node id: " << resp_content->body.node_id;
        return;
    }

    LOG_INFO << "received ver_resp "
        << " from " << tcp_ch->get_remote_addr()
        << ", node_id: " << resp_content->body.node_id
        << ", core_ver=" << resp_content->body.core_version
        << ", protocol_ver=" << resp_content->body.protocol_version;
    
    // TODO: remote_addr 不对
    auto candidate = get_peer_candidate(tcp_ch->get_remote_addr());
    if (nullptr != candidate) {
        candidate->node_id = resp_content->body.node_id;
    }

    //larger than max connected count, so just tag its status with available and stop channel
    uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(network::CLIENT_SOCKET);
    if (client_peer_nodes_count >= max_connected_peer_nodes_count) {
        if (nullptr != candidate) {
            candidate->net_st = ns_available;
        }

        network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (!add_peer_node(msg)) {
        LOG_ERROR << "add peer node error, node_id: " << resp_content->body.node_id;
        network::connection_manager::instance().stop_channel(msg->header.src_sid);
        return;
    }

    if (candidate) {
        candidate->reconn_cnt = 0;
    }

    if (resp_content->header.exten_info.count("capacity")) {
        network::connection_manager::instance().set_proto_capacity(msg->header.src_sid,
                                                                        resp_content->header.exten_info["capacity"]);
    }

    tcp::endpoint local_ep = tcp_ch->get_local_addr();
    advertise_local(local_ep, msg->header.src_sid);
}


void p2p_net_service::advertise_local(tcp::endpoint tcp_ep, network::socket_id sid) {
    if (network::net_address::is_rfc1918(tcp_ep)) {
        LOG_DEBUG << "ip address is RFC1918 private network ip and will not advertise local: "
            << tcp_ep.address().to_string();
        return;
    }

    std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
    node->m_id = ConfManager::instance().GetNodeId();
    node->m_live_time = time(nullptr);
    node->m_peer_addr = tcp::endpoint(tcp_ep.address(), m_listen_port);
    node->m_sid = sid;

    broadcast_peer_nodes(node);

    if (Server::NodeType == NODE_TYPE::BareMetalNode) {
        const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> bare_metal_nodes =
            BareMetalNodeManager::instance().getBareMetalNodes();
        for (const auto& iter : bare_metal_nodes) {
            std::shared_ptr<peer_node> enode = std::make_shared<peer_node>();
            enode->m_id = iter.first;
            enode->m_live_time = time(nullptr);
            enode->m_peer_addr = tcp::endpoint(tcp_ep.address(), m_listen_port);
            enode->m_sid = sid;

            broadcast_peer_nodes(enode);
        }
    }
}

int32_t p2p_net_service::broadcast_peer_nodes(std::shared_ptr<peer_node> node) {
    std::shared_ptr<network::message> resp_msg = std::make_shared<network::message>();
    resp_msg->header.msg_name = P2P_GET_PEER_NODES_RESP;
    resp_msg->header.msg_priority = 0;

    std::shared_ptr<dbc::get_peer_nodes_resp> resp_content = std::make_shared<dbc::get_peer_nodes_resp>();
    resp_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    resp_content->header.__set_msg_name(P2P_GET_PEER_NODES_RESP);
    resp_content->header.__set_nonce(util::create_nonce());

    if (node) {
        dbc::peer_node_info info;
        assign_peer_info(info, node);
        info.service_list.push_back(std::string("ai_training"));
        resp_content->body.peer_nodes_list.push_back(std::move(info));
        resp_msg->set_content(resp_content);

        network::connection_manager::instance().send_message(node->m_sid, resp_msg);
    }
    else {
        int count = 0;
        for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it) {
            if (nullptr == it->second
                || network::SERVER_SOCKET == it->second->m_sid.get_type()
                || PEER_SEED_NODE == it->second->m_node_type) {
                continue;
            }

            dbc::peer_node_info info;
            assign_peer_info(info, it->second);
            info.service_list.push_back(std::string("ai_training"));
            resp_content->body.peer_nodes_list.push_back(std::move(info));

            if (++count >= max_broadcast_peer_nodes_count) {
                break;
            }
        }

        std::list<std::shared_ptr<peer_candidate> > tmp_candi_list;
        for (auto it = m_peer_candidates.begin(); (it != m_peer_candidates.end()) && (count < 100);) {
            if (ns_available == (*it)->net_st && PEER_NORMAL_NODE == (*it)->node_type) {
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
            }
            else {
                ++it;
            }
        }
        m_peer_candidates.insert(m_peer_candidates.end(), tmp_candi_list.begin(), tmp_candi_list.end());

        if (resp_content->body.peer_nodes_list.size() > 0) {
            resp_msg->set_content(resp_content);
            network::connection_manager::instance().broadcast_message(resp_msg);
        }
    }

    return ERR_SUCCESS;
}


void p2p_net_service::on_broadcast_peer_nodes(const std::shared_ptr<network::message>& msg) {
    if (m_peer_candidates.size() >= max_peer_candidates_count) {
        return;
    }

    auto rsp = std::dynamic_pointer_cast<dbc::get_peer_nodes_resp>(msg->content);
    if (!rsp) {
        return;
    }

    if (!util::check_id(rsp->header.nonce)) {
        LOG_DEBUG << "nonce check_id failed";
        return;
    }

    if (1 == rsp->body.peer_nodes_list.size()) {
        const dbc::peer_node_info& node = rsp->body.peer_nodes_list[0];
         
        tcp::endpoint ep(address_v4::from_string(node.addr.ip), (uint16_t)node.addr.port);
        if (network::net_address::is_rfc1918(ep)) {
            LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
            return;
        }

        if (!add_peer_candidate(ep, ns_available, PEER_NORMAL_NODE, node.peer_node_id)) {
            LOG_ERROR << "add peer candidate error: " << ep;
        }

        LOG_INFO << "add peer candidate addr:" << ep << ", node_id:" << node.peer_node_id;

        auto peer_candidate = get_peer_candidate(ep);
        if (peer_candidate != nullptr) {
            peer_candidate->node_id = node.peer_node_id;
        }

        return;
    }

    for (auto it = rsp->body.peer_nodes_list.begin(); it != rsp->body.peer_nodes_list.end(); ++it) {
        tcp::endpoint ep(address_v4::from_string(it->addr.ip), (uint16_t)it->addr.port);
        if (network::net_address::is_rfc1918(ep)) {
            LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
            continue;
        }

        auto candidate = get_peer_candidate(ep);
        if (nullptr == candidate) {
            if (!add_peer_candidate(ep, ns_idle, PEER_NORMAL_NODE)) {
                LOG_ERROR << "add peer candidate error: " << ep;
            }

            if (candidate != nullptr) {
                candidate->node_id = it->peer_node_id;
            }

            LOG_INFO << "add peer candidate: "
                << "addr=" << it->addr.ip << ":" << (uint16_t)it->addr.port
                << ", node_id=" << it->peer_node_id;
        }
        else if (candidate->node_id.empty() && !it->peer_node_id.empty()) {
            candidate->node_id = it->peer_node_id;
        }
    }
}


uint32_t p2p_net_service::get_peer_nodes_count_by_socket_type(network::socket_type type) {
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
        return ERR_SUCCESS;
    }

    available_candidates.clear();

    uint32_t i = 0; 
    for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it) {
        if (ns_available == (*it)->net_st && i < count) {
            available_candidates.push_back(*it);
            i++;
        }
    }

    return ERR_SUCCESS;
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
        if (PEER_SEED_NODE == it->second->m_node_type && network::CLIENT_SOCKET == it->second->m_sid.get_type()) {
            LOG_DEBUG << "p2p net service get disconnect seed peer node: " << it->second->m_id
                      << it->second->m_sid.to_string();
            return it->second;
        }

        //add to client nodes for later use
        if (network::CLIENT_SOCKET == it->second->m_sid.get_type()) {
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

        if (ns_available == it->net_st && PEER_SEED_NODE == it->node_type) {
            seed_node_candidates.push_back(it);
        }

        if (ns_available == it->net_st && PEER_NORMAL_NODE == it->node_type) {
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


int32_t p2p_net_service::save_peer_candidates() {
    if (m_peer_candidates.empty()) {
        return ERR_SUCCESS;
    }

    m_peers_candidates_db.clear();

    std::list<std::shared_ptr<dbc::db_peer_candidate> > list_peer_candidates;

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
        list_peer_candidates.push_back(db_candidate);
    }

    if (!m_peers_candidates_db.write_datas(list_peer_candidates)) {
        LOG_ERROR << "save db_peer_candidates failed";
        return ERR_ERROR;
    }

    return ERR_SUCCESS;
}
