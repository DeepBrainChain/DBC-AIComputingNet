#include "node_request_service.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/all.hpp>
#include "server/server.h"
#include "config/conf_manager.h"
#include "service_module/service_message_id.h"
#include "../message/matrix_types.h"
#include "../message/protocol_coder/matrix_coder.h"
#include <boost/thread/thread.hpp>
#include "../message/service_topic.h"
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include "../message/message_id.h"
#include "data/resource/SystemResourceManager.h"
#include "service_module/service_name.h"
#include "util/base64.h"
#include "tweetnacl/tools.h"
#include "tweetnacl/randombytes.h"
#include "tweetnacl/tweetnacl.h"

std::string get_gpu_spec(const std::string& s) {
    if (s.empty()) {
        return "";
    }

    std::string rt;
    std::stringstream ss;
    ss << s;
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(ss, pt);
        rt = pt.get<std::string>("env.NVIDIA_VISIBLE_DEVICES");
        if (!rt.empty()) {
            util::trim(rt);
        }
    }
    catch (...) {

    }

    return rt;
}

std::string get_is_update(const std::string& s) {
    if (s.empty()) return "";

    std::string operation;
    std::stringstream ss;
    ss << s;
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_json(ss, pt);
        if (pt.count("operation") != 0) {
            operation = pt.get<std::string>("operation");
        }
    }
    catch (...) {

    }

    return operation;
}

node_request_service::~node_request_service() {
    if (m_is_computing_node) {
        remove_timer(m_training_task_timer_id);
        remove_timer(m_prune_task_timer_id);
    }
    m_websocket_client.stop();
}

int32_t node_request_service::init(bpo::variables_map &options) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_computing_node = true;
        add_self_to_servicelist(options);
    }

    service_module::init(options);

    if (m_is_computing_node) {
        auto fresult = m_task_scheduler.Init();
        int32_t ret = std::get<0>(fresult);
        if (ret != E_SUCCESS) {
            return E_DEFAULT;
        } else {
            ret = m_sessionid_db.init_db(env_manager::instance().get_db_path(), "sessionid.db");
            if (!ret) {
                LOG_ERROR << "init sessionid_db failed";
                return E_DEFAULT;
            }
            std::map<std::string, std::shared_ptr<dbc::owner_sessionid>> wallet_sessionid;
            m_sessionid_db.load(wallet_sessionid);
            for (auto &it: wallet_sessionid) {
                m_wallet_sessionid.insert({it.first, it.second->session_id});
                m_wallet_sessionid.insert({it.second->session_id, it.first});
            }

            std::string ws_url = "wss://" + conf_manager::instance().get_dbc_chain_domain();
            m_websocket_client.init(ws_url);
            m_websocket_client.enable_auto_reconnection();
            m_websocket_client.set_message_callback(std::bind(&node_request_service::on_ws_msg, this, std::placeholders::_1, std::placeholders::_2));
            m_websocket_client.start();

            return E_SUCCESS;
        }
    }

    return E_SUCCESS;
}

void node_request_service::add_self_to_servicelist(bpo::variables_map &options) {
    dbc::node_service_info info;
    info.service_list.emplace_back(SERVICE_NAME_AI_TRAINING);

    if (options.count("name")) {
        auto name_str = options["name"].as<std::string>();
        info.__set_name(name_str);
    } else {
        info.__set_name("null");
    }

    auto tnow = std::time(nullptr);
    info.__set_time_stamp(tnow);

    std::map<std::string, std::string> kvs;
    kvs["version"] = SystemInfo::instance().get_version();

    int32_t count = m_task_scheduler.GetRunningTaskSize();
    std::string state;
    if (count <= 0) {
        state = "idle";
    }
    else {
        state = "busy(" + std::to_string(count) + ")";
    }
    kvs["state"] = state;

    kvs["pub_key"] = conf_manager::instance().get_pub_key();
    info.__set_kvs(kvs);

    service_info_collection::instance().add(conf_manager::instance().get_node_id(), info);
}

void node_request_service::init_timer() {
    if (m_is_computing_node) {
        // 10s
        m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&node_request_service::on_training_task_timer, this, std::placeholders::_1);
        m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER, 10 * 1000, ULLONG_MAX, "");

        // 1min
        m_timer_invokers[AI_PRUNE_TASK_TIMER] = std::bind(&node_request_service::on_prune_task_timer, this, std::placeholders::_1);
        m_prune_task_timer_id = this->add_timer(AI_PRUNE_TASK_TIMER, 60 * 1000, ULLONG_MAX, "");
    }

    // 10s
    m_timer_invokers[SERVICE_BROADCAST_TIMER] = std::bind(&node_request_service::on_timer_service_broadcast, this, std::placeholders::_1);
    add_timer(SERVICE_BROADCAST_TIMER, 10 * 1000, ULLONG_MAX, "");
}

void node_request_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_REQ, &node_request_service::on_node_create_task_req)
    BIND_MESSAGE_INVOKER(NODE_START_TASK_REQ, &node_request_service::on_node_start_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_REQ, &node_request_service::on_node_restart_task_req)
    BIND_MESSAGE_INVOKER(NODE_STOP_TASK_REQ, &node_request_service::on_node_stop_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESET_TASK_REQ, &node_request_service::on_node_reset_task_req)
    BIND_MESSAGE_INVOKER(NODE_DELETE_TASK_REQ, &node_request_service::on_node_delete_task_req)
    BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_REQ, &node_request_service::on_node_task_logs_req)
    BIND_MESSAGE_INVOKER(NODE_LIST_TASK_REQ, &node_request_service::on_node_list_task_req)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_REQ, &node_request_service::on_node_query_node_info_req)
    BIND_MESSAGE_INVOKER(SERVICE_BROADCAST_REQ, &node_request_service::on_net_service_broadcast_req)
}

void node_request_service::init_subscription() {
    SUBSCRIBE_BUS_MESSAGE(NODE_CREATE_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_START_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_RESTART_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_STOP_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_RESET_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_DELETE_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_REQ)
    SUBSCRIBE_BUS_MESSAGE(SERVICE_BROADCAST_REQ);
}

bool node_request_service::hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id) {
    bool hit = false;
    auto it = peer_node_list.begin();
    for (; it != peer_node_list.end(); it++) {
        if ((*it) == node_id) {
            hit = true;
            break;
        }
    }

    return hit;
}

bool node_request_service::check_nonce(const std::string& nonce) {
    if (!util::check_id(nonce)) {
        return false;
    }

    if (m_nonceCache.contains(nonce)) {
        return false;
    }
    else {
        m_nonceCache.insert(nonce, 1);
    }

    return true;
}

bool node_request_service::check_req_header(const std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<dbc::network::msg_base> base = msg->content;
    if (!base) {
        LOG_ERROR << "msg.content is nullptr";
        return false;
    }

    if (base->header.nonce.empty()) {
        LOG_ERROR << "header.nonce is empty";
        return false;
    }

    if (!check_nonce(base->header.nonce)) {
        LOG_ERROR << "nonce check failed, nonce:" << base->header.nonce;
        return E_DEFAULT;
    }

    if (!util::check_id(base->header.session_id)) {
        LOG_ERROR << "header.session_id check failed";
        return false;
    }

    if (base->header.path.empty()) {
        LOG_ERROR << "header.path size <= 0";
        return false;
    }

    if (base->header.exten_info.size() < 3) {
        LOG_ERROR << "header.exten_info size < 3";
        return false;
    }

    return true;
}

void node_request_service::on_node_create_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_create_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_create_task_req_data> data = std::make_shared<dbc::node_create_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_create(node_req_msg->header, data);
    }
}

void node_request_service::on_node_start_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_start_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_start_task_req_data> data = std::make_shared<dbc::node_start_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_start(node_req_msg->header, data);
    }
}

void node_request_service::on_node_stop_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_stop_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_stop_task_req_data> data = std::make_shared<dbc::node_stop_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_stop(node_req_msg->header, data);
    }
}

void node_request_service::on_node_restart_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_restart_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_restart_task_req_data> data = std::make_shared<dbc::node_restart_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_restart(node_req_msg->header, data);
    }
}

void node_request_service::on_node_reset_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_reset_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_reset_task_req_data> data = std::make_shared<dbc::node_reset_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_reset(node_req_msg->header, data);
    }
}

void node_request_service::on_node_delete_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_delete_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg id nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_delete_task_req_data> data = std::make_shared<dbc::node_delete_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_delete(node_req_msg->header, data);
    }
}

void node_request_service::on_node_task_logs_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_task_logs_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_task_logs_req_data> data = std::make_shared<dbc::node_task_logs_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    int16_t req_head_or_tail = data->head_or_tail;
    int32_t req_number_of_lines = data->number_of_lines;

    if (GET_LOG_HEAD != req_head_or_tail && GET_LOG_TAIL != req_head_or_tail) {
        LOG_ERROR << "req_head_or_tail is invalid:" << req_head_or_tail;
        return;
    }

    if (req_number_of_lines > MAX_NUMBER_OF_LINES || req_number_of_lines < 0) {
        LOG_ERROR << "req_number_of_lines is invalid:" << req_number_of_lines;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_logs(node_req_msg->header, data);
    }
}

void node_request_service::on_node_list_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_list_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_list_task_req_data> data = std::make_shared<dbc::node_list_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    if (!data->task_id.empty() && !util::check_id(data->task_id)) {
        LOG_ERROR << "task_id check failed, task_id:" << data->task_id;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_list(node_req_msg->header, data);
    }
}

void node_request_service::on_node_query_node_info_req(const std::shared_ptr<dbc::network::message> &msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_query_node_info_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_query_node_info_req_data> data = std::make_shared<dbc::node_query_node_info_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        query_node_info(node_req_msg->header, data);
    }
}

static std::string generate_pwd() {
    char chr[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G',
                   'H', 'I', 'J', 'K', 'L', 'M', 'N',
                   'O', 'P', 'Q', 'R', 'S', 'T',
                   'U', 'V', 'W', 'X', 'Y', 'Z',
                   'a', 'b', 'c', 'd', 'e', 'f', 'g',
                   'h', 'i', 'j', 'k', 'l', 'm', 'n',
                   'o', 'p', 'q', 'r', 's', 't',
                   'u', 'v', 'w', 'x', 'y', 'z',
                   '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    srand(time(NULL));
    std::string strpwd;
    int nlen = 10; //10位密码
    char buf[3] = { 0 };
    int idx0 = rand() % 52;
    sprintf(buf, "%c", chr[idx0]);
    strpwd.append(buf);

    int idx_0 = rand() % nlen;
    int idx_1 = rand() % nlen;
    int idx_2 = rand() % nlen;

    for (int i = 1; i < nlen; i++) {
        int idx;
        if (i == idx_0 || i == idx_1 || i == idx_2) {
            idx = rand() % 62;
        }
        else {
            idx = rand() % 62;
        }
        sprintf(buf, "%c", chr[idx]);
        strpwd.append(buf);
    }

    return strpwd;
}

void node_request_service::on_ws_msg(int32_t err_code, const std::string& msg) {
    LOG_INFO << "on_ws_msg(" << err_code << ") " << msg;

    int32_t ret = E_DEFAULT;
    std::string ret_msg;
    std::string task_id;
    std::string login_password;

    do {
        if (err_code != 0) {
            ret = E_DEFAULT;
            ret_msg = msg;
            break;
        }

        rapidjson::Document doc;
        doc.Parse(msg.c_str());
        if (!doc.IsObject()) {
            ret = E_DEFAULT;
            ret_msg = "rsp_json parse error";
            break;
        }

        if (!doc.HasMember("result")) {
            ret = E_DEFAULT;
            ret_msg = "rsp_json has not result";
            break;
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            ret = E_DEFAULT;
            ret_msg = "rsp_json result is not object";
            break;
        }

        if (v_result.HasMember("machineStatus")) {
            if (!v_result.HasMember("machineStatus")) {
                ret = E_DEFAULT;
                ret_msg = "rsp_json has not machineStatus";
                break;
            }

            const rapidjson::Value &v_machineStatus = v_result["machineStatus"];
            if (!v_machineStatus.IsString()) {
                ret = E_DEFAULT;
                ret_msg = "rsp_json machineStatus is not string";
                break;
            }
            std::string machine_status = v_machineStatus.GetString();

            //machine_status = "creating";

            if (machine_status != "creating" && machine_status != "rented") {
                ret = E_DEFAULT;
                ret_msg = "rent check failed";
                LOG_ERROR << "rent check failed";
                break;
            }

            if (m_websocket_client.is_connected()) {
                std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
                        + m_create_data->wallet + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
                m_websocket_client.send(str_send);
                LOG_INFO << "ws_send: " << str_send;
                return;
            } else {
                ret = E_DEFAULT;
                ret_msg = "verify renter failed";
                LOG_ERROR << "websocket is disconnect";
                break;
            }
        }
        else if (v_result.HasMember("rentEnd")) {
            const rapidjson::Value &v_rentEnd = v_result["rentEnd"];
            if (!v_rentEnd.IsNumber()) {
                ret = E_DEFAULT;
                ret_msg = "rsp_json rentEnd is not number";
                break;
            }

            uint64_t rentEnd = v_rentEnd.GetUint64();
            //rentEnd = 123;

            bool can_do = false;
            if (rentEnd > 0) {
                can_do = true;

                auto it = m_wallet_sessionid.find(m_create_data->wallet);
                if (it == m_wallet_sessionid.end()) {
                    std::string id = util::create_session_id();
                    m_wallet_sessionid[m_create_data->wallet] = id;
                    m_wallet_sessionid[id] = m_create_data->wallet;
                    std::shared_ptr<dbc::owner_sessionid> sessionid(new dbc::owner_sessionid());
                    sessionid->wallet = m_create_data->wallet;
                    sessionid->session_id = id;
                    m_sessionid_db.write(sessionid);
                }
            } else if (!m_create_data->session_id.empty() && !m_create_data->session_id_sign.empty()) {
                std::string sign_msg = m_create_data->session_id;
                auto it = m_sessionid_wallet.find(sign_msg);
                if (it == m_sessionid_wallet.end()) {
                    ret = E_DEFAULT;
                    ret_msg = "session_id is invalid";
                    LOG_ERROR << "session_id is invalid";
                    break;
                }

                std::string owner_wallet = it->second;
                if (util::verify_sign(m_create_data->session_id_sign, sign_msg, owner_wallet)) {
                    can_do = true;
                } else {
                    ret = E_DEFAULT;
                    ret_msg = "session_id is invalid";
                    LOG_ERROR << "session_id is invalid";
                    break;
                }
            } else {
                ret = E_DEFAULT;
                ret_msg = "req error";
                LOG_ERROR << "wallet and session_id  error";
                break;
            }

            if (can_do) {
                task_id = util::create_task_id();
                login_password = generate_pwd();
                auto fresult = m_task_scheduler.CreateTask(task_id, login_password, m_create_data->additional);
                ret = std::get<0>(fresult);
                ret_msg = std::get<1>(fresult);
            }
        }
    } while(0);

    std::shared_ptr<dbc::node_create_task_rsp> rsp_content = std::make_shared<dbc::node_create_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_CREATE_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(m_create_header.session_id);
    rsp_content->header.__set_path(m_create_header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"data\":" << "{";
        ss << "\"task_id\":" << "\"" << task_id << "\"";

        auto taskinfo = m_task_scheduler.FindTask(task_id);
        struct tm _tm{};
        time_t tt = taskinfo == nullptr ? 0 : taskinfo->create_time;
        localtime_r(&tt, &_tm);
        char buf[256] = {0};
        memset(buf, 0, sizeof(char) * 256);
        strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
        ss << ", \"create_time\":" << "\"" << buf << "\"";

        ss << "}";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_name(NODE_CREATE_TASK_RSP);
    resp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(resp_msg);
}

void node_request_service::task_create(const dbc::network::base_header& header,
                                       const std::shared_ptr<dbc::node_create_task_req_data>& data) {
    m_create_header = header;
    m_create_data = data;

    if (m_websocket_client.is_connected()) {
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
                               + conf_manager::instance().get_node_id() + R"("]})";
        m_websocket_client.send(str_send);
        LOG_INFO << "ws_send: " << str_send;
    } else {
        LOG_ERROR << "websocket is disconnect";
    }
}

void node_request_service::task_start(const dbc::network::base_header& header,
                                      const std::shared_ptr<dbc::node_start_task_req_data>& data) {
    std::string task_id = data->task_id;

    auto fresult = m_task_scheduler.StartTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_start_task_rsp> rsp_content = std::make_shared<dbc::node_start_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_START_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"result\":" << "\"ok\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_START_TASK_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_stop(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_stop_task_req_data>& data) {
    std::string task_id = data->task_id;

    auto fresult = m_task_scheduler.StopTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_stop_task_rsp> rsp_content = std::make_shared<dbc::node_stop_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_STOP_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"result\":" << "\"ok\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_STOP_TASK_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_restart(const dbc::network::base_header& header,
                                        const std::shared_ptr<dbc::node_restart_task_req_data>& data) {
    std::string task_id = data->task_id;

    auto fresult = m_task_scheduler.RestartTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_restart_task_rsp> rsp_content = std::make_shared<dbc::node_restart_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_RESTART_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"result\":" << "\"ok\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_RESTART_TASK_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_reset(const dbc::network::base_header& header,
                                      const std::shared_ptr<dbc::node_reset_task_req_data>& data) {
    std::string task_id = data->task_id;

    auto fresult = m_task_scheduler.ResetTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_reset_task_rsp> rsp_content = std::make_shared<dbc::node_reset_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_RESET_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"result\":" << "\"ok\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_RESET_TASK_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_delete(const dbc::network::base_header& header,
                                       const std::shared_ptr<dbc::node_delete_task_req_data>& data) {
    std::string task_id = data->task_id;

    auto fresult = m_task_scheduler.DeleteTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_delete_task_rsp> rsp_content = std::make_shared<dbc::node_delete_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_DELETE_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"result\":" << "\"ok\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_DELETE_TASK_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_logs(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_task_logs_req_data>& data) {
    std::string task_id = data->task_id;
    int16_t head_or_tail = data->head_or_tail;
    int32_t number_of_lines = data->number_of_lines;

    std::string log_content;
    auto fresult = m_task_scheduler.GetTaskLog(task_id, (ETaskLogDirection) head_or_tail,
                                               number_of_lines, log_content);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    if (GET_LOG_HEAD == (ETaskLogDirection) head_or_tail) {
        log_content = log_content.substr(0, MAX_LOG_CONTENT_SIZE);
    }
    else {
        size_t log_lenth = log_content.length();
        if (log_lenth > MAX_LOG_CONTENT_SIZE) {
            log_content = log_content.substr(log_lenth - MAX_LOG_CONTENT_SIZE, MAX_LOG_CONTENT_SIZE);
        }
    }

    std::shared_ptr<dbc::node_task_logs_rsp> rsp_content = std::make_shared<dbc::node_task_logs_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_TASK_LOGS_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(ret);
    std::stringstream ss;
    ss << "{";
    if (ret != E_SUCCESS) {
        ss << "\"error_code\":" << ret;
        ss << ", \"error_message\":" << "\"" << ret_msg << "\"";
    } else {
        ss << "\"error_code\":" << ret;
        ss << ", \"log_content\":" << "\"" << log_content << "\"";
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(NODE_TASK_LOGS_RSP);
    rsp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

void node_request_service::task_list(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_list_task_req_data>& data) {
    int32_t result = E_SUCCESS;
    std::string result_msg = "task list successful";

    std::string task_id = data->task_id;

    std::stringstream ss_tasks;
    if (task_id.empty()) {
        ss_tasks << "[";
        std::vector<std::shared_ptr<dbc::TaskInfo>> task_list;
        m_task_scheduler.ListAllTask(task_list);
        int idx = 0;
        for (auto &task : task_list) {
            if (idx > 0)
                ss_tasks << ",";

            ss_tasks << "{";
            ss_tasks << "\"task_id\":" << "\"" << task->task_id << "\"";
            ss_tasks << ", \"ssh_ip\":" << "\"" << get_public_ip() << "\"";
            ss_tasks << ", \"ssh_port\":" << "\"" << task->ssh_port << "\"";
            ss_tasks << ", \"user_name\":" << "\"" << g_vm_login_username << "\"";
            ss_tasks << ", \"login_password\":" << "\"" << task->login_password << "\"";

            const TaskResourceManager& res_mgr = m_task_scheduler.GetTaskResourceManager();
            const std::map<int32_t, DeviceDisk>& task_disk_list = res_mgr.GetTaskDisk(task->task_id);
            int64_t disk_data = 0;
            if (!task_disk_list.empty()) {
                auto it_disk = task_disk_list.find(1);
                if (it_disk != task_disk_list.end())
                    disk_data = it_disk->second.total;
            }
            const DeviceCpu& task_cpu = res_mgr.GetTaskCpu(task->task_id);
            int32_t cpu_cores = task_cpu.sockets * task_cpu.cores_per_socket * task_cpu.threads_per_core;
            const std::map<std::string, DeviceGpu>& task_gpu_list = res_mgr.GetTaskGpu(task->task_id);
            uint32_t gpu_count = task_gpu_list.size();
            const DeviceMem& task_mem = res_mgr.GetTaskMem(task->task_id);
            int64_t mem_size = task_mem.total;

            ss_tasks << ", \"cpu_cores\":" << cpu_cores;
            ss_tasks << ", \"gpu_count\":" << gpu_count;
            ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024L) << "\"";
            ss_tasks << ", \"disk_system\":" << "\"" << size_to_string(g_image_size, 1024L * 1024L * 1024L) << "\"";
            ss_tasks << ", \"disk_data\":" << "\"" << size_to_string(disk_data, 1024L * 1024L) << "\"";

            struct tm _tm{};
            time_t tt = task->create_time;
            localtime_r(&tt, &_tm);
            char buf[256] = {0};
            memset(buf, 0, sizeof(char) * 256);
            strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
            ss_tasks << ", \"create_time\":" << "\"" << buf << "\"";

            ss_tasks << ", \"status\":" << "\"" << task_status_string(m_task_scheduler.GetTaskStatus(task->task_id)) << "\"";
            ss_tasks << "}";

            idx++;
        }
        ss_tasks << "]";
    } else {
        auto task = m_task_scheduler.FindTask(task_id);
        if (nullptr != task) {
            ss_tasks << "{";
            ss_tasks << "\"task_id\":" << "\"" << task->task_id << "\"";
            ss_tasks << ", \"ssh_ip\":" << "\"" << get_public_ip() << "\"";
            ss_tasks << ", \"ssh_port\":" << "\"" << task->ssh_port << "\"";
            ss_tasks << ", \"user_name\":" << "\"" << g_vm_login_username << "\"";
            ss_tasks << ", \"login_password\":" << "\"" << task->login_password << "\"";

            const TaskResourceManager& res_mgr = m_task_scheduler.GetTaskResourceManager();
            const std::map<int32_t, DeviceDisk>& task_disk_list = res_mgr.GetTaskDisk(task_id);
            int64_t disk_data = 0;
            if (!task_disk_list.empty()) {
                auto it_disk = task_disk_list.find(1);
                if (it_disk != task_disk_list.end())
                    disk_data = it_disk->second.total;
            }
            const DeviceCpu& task_cpu = res_mgr.GetTaskCpu(task_id);
            int32_t cpu_cores = task_cpu.sockets * task_cpu.cores_per_socket * task_cpu.threads_per_core;
            const std::map<std::string, DeviceGpu>& task_gpu_list = res_mgr.GetTaskGpu(task_id);
            uint32_t gpu_count = task_gpu_list.size();
            const DeviceMem& task_mem = res_mgr.GetTaskMem(task_id);
            int64_t mem_size = task_mem.total;

            ss_tasks << ", \"cpu_cores\":" << cpu_cores;
            ss_tasks << ", \"gpu_count\":" << gpu_count;
            ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024L) << "\"";
            ss_tasks << ", \"disk_system\":" << "\"" << size_to_string(g_image_size, 1024L * 1024L * 1024L) << "\"";
            ss_tasks << ", \"disk_data\":" << "\"" << size_to_string(disk_data, 1024L * 1024L) << "\"";

            struct tm _tm{};
            time_t tt = task->create_time;
            localtime_r(&tt, &_tm);
            char buf[256] = {0};
            memset(buf, 0, sizeof(char) * 256);
            strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
            ss_tasks << ", \"create_time\":" << "\"" << buf << "\"";

            ss_tasks << ", \"status\":" << "\"" << task_status_string(m_task_scheduler.GetTaskStatus(task->task_id)) << "\"";
            ss_tasks << "}";
        } else {
            result = E_DEFAULT;
            result_msg = "task_id not exist";
        }
    }

    std::shared_ptr<dbc::node_list_task_rsp> rsp_content = std::make_shared<dbc::node_list_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_LIST_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(result);
    std::stringstream ss;
    ss << "{";
    if (result != E_SUCCESS) {
        ss << "\"error_code\":" << result;
        ss << ", \"error_message\":" << "\"" << result_msg << "\"";
    } else {
        ss << "\"error_code\":" << result;
        ss << ", \"data\":" << ss_tasks.str();
    }
    ss << "}";
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_name(NODE_LIST_TASK_RSP);
    resp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(resp_msg);
}

void node_request_service::query_node_info(const dbc::network::base_header& header,
                                           const std::shared_ptr<dbc::node_query_node_info_req_data>& data) {
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << 0;
    ss << ",\"data\":" << "{";
    ss << "\"ip\":" << "\"" << SystemInfo::instance().get_publicip() << "\"";
    ss << ",\"os\":" << "\"" << SystemInfo::instance().get_osname() << "\"";
    cpu_info tmp_cpuinfo = SystemInfo::instance().get_cpuinfo();
    ss << ",\"cpu\":" << "{";
    ss << "\"type\":" << "\"" << tmp_cpuinfo.cpu_name << "\"";
    ss << ",\"cores\":" << "\"" << tmp_cpuinfo.total_cores << "\"";
    ss << ",\"used_usage\":" << "\"" << (SystemInfo::instance().get_cpu_usage() * 100) << "%" << "\"";
    ss << "}";
    mem_info tmp_meminfo = SystemInfo::instance().get_meminfo();
    ss << ",\"mem\":" <<  "{";
    ss << "\"size\":" << "\"" << scale_size(tmp_meminfo.mem_total) << "\"";
    ss << ",\"free\":" << "\"" << scale_size(tmp_meminfo.mem_total - tmp_meminfo.mem_used) << "\"";
    ss << ",\"used_usage\":" << "\"" << (tmp_meminfo.mem_usage * 100) << "%" << "\"";
    ss << "}";
    disk_info tmp_diskinfo = SystemInfo::instance().get_diskinfo();
    ss << ",\"disk\":" << "{";
    ss << "\"type\":" << "\"" << (tmp_diskinfo.disk_type == DISK_SSD ? "SSD" : "HDD") << "\"";
    ss << ",\"size\":" << "\"" << scale_size(tmp_diskinfo.disk_total) << "\"";
    ss << ",\"free\":" << "\"" << scale_size(tmp_diskinfo.disk_awalible) << "\"";
    ss << ",\"used_usage\":" << "\"" << (tmp_diskinfo.disk_usage * 100) << "%" << "\"";
    ss << "}";
    int32_t count = m_task_scheduler.GetRunningTaskSize();
    std::string state;
    if (count <= 0) {
        state = "idle";
    }
    else {
        state = "busy(" + std::to_string(count) + ")";
    }
    ss << ",\"state\":" << "\"" << state << "\"";
    ss << ",\"version\":" << "\"" << SystemInfo::instance().get_version() << "\"";
    ss << "}";
    ss << "}";

    std::shared_ptr<dbc::node_query_node_info_rsp> rsp_content = std::make_shared<dbc::node_query_node_info_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(header.session_id);
    rsp_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = "ecdsa";
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_result(E_SUCCESS);
    rsp_content->body.__set_result_msg(ss.str());

    //rsp msg
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_name(NODE_QUERY_NODE_INFO_RSP);
    resp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(resp_msg);
}

void node_request_service::on_training_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.ProcessTask();
}

void node_request_service::on_prune_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.PruneTask();
}

void node_request_service::on_timer_service_broadcast(const std::shared_ptr<core_timer>& timer)
{
    auto s_map_size = service_info_collection::instance().size();
    if (s_map_size == 0) {
        return;
    }

    if (m_is_computing_node) {
        int32_t count = m_task_scheduler.GetRunningTaskSize();
        std::string state;
        if (count <= 0) {
            state = "idle";
        } else {
            state = "busy(" + std::to_string(count) + ")";
        }

        service_info_collection::instance().update(conf_manager::instance().get_node_id(), "state", state);
        service_info_collection::instance().update(conf_manager::instance().get_node_id(), "version", SystemInfo::instance().get_version());
        service_info_collection::instance().update_own_node_time_stamp(conf_manager::instance().get_node_id());
    }

    service_info_collection::instance().remove_unlived_nodes(conf_manager::instance().get_timer_service_list_expired_in_second());

    auto service_info_map = service_info_collection::instance().get_change_set();
    if(!service_info_map.empty()) {
        auto service_broadcast_req = create_service_broadcast_req_msg(service_info_map);
        if (service_broadcast_req != nullptr) {
            dbc::network::connection_manager::instance().broadcast_message(service_broadcast_req);
        }
    }
}

std::shared_ptr<dbc::network::message> node_request_service::create_service_broadcast_req_msg(const service_info_map& mp) {
    auto req_content = std::make_shared<dbc::service_broadcast_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(SERVICE_BROADCAST_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(util::create_session_id());
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = req_content->header.nonce + req_content->header.session_id;
    std::string signature = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    if (signature.empty())  return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = "ecdsa";
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_node_service_info_map(mp);

    auto req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(SERVICE_BROADCAST_REQ);
    req_msg->set_content(req_content);
    return req_msg;
}

void node_request_service::on_net_service_broadcast_req(const std::shared_ptr<dbc::network::message> &msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    auto node_req_msg = std::dynamic_pointer_cast<dbc::service_broadcast_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    std::string sign_msg = node_req_msg->header.nonce + node_req_msg->header.session_id;
    if (!util::verify_sign(node_req_msg->header.exten_info["sign"], sign_msg, node_req_msg->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign error." << node_req_msg->header.exten_info["origin_id"];
        return;
    }

    service_info_map mp = node_req_msg->body.node_service_info_map;

    service_info_collection::instance().add(mp);
}

std::string node_request_service::format_logs(const std::string& raw_logs, uint16_t max_lines) {
    //docker logs has special format with each line of log:
    // 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
    //we should remove it
    //and ends with 0x30 0x0d 0x0a
    max_lines = (max_lines == 0) ? MAX_NUMBER_OF_LINES : max_lines;
    size_t size = raw_logs.size();
    std::vector<unsigned char> log_vector(size);

    int push_char_count = 0;
    const char* p = raw_logs.c_str();

    uint16_t line_count = 1;

    for (size_t i = 0; i < size;) {
        //0x30 0x0d 0x0a
        if ((i + 2 < size)
            && (0x30 == *p)
            && (0x0d == *(p + 1))
            && (0x0a == *(p + 2))) {
            break;
        }

        if (max_lines != 0 && line_count > max_lines) {
            break;
        }

        //skip: 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
        if ((i + 7 < size)
            && ((0x01 == *p) || (0x02 == *p))
            && (0x00 == *(p + 1))
            && (0x00 == *(p + 2))
            && (0x00 == *(p + 3))
            && (0x00 == *(p + 4))
            && (0x00 == *(p + 5))) {
            i += 8;
            p += 8;
            continue;
        }

        log_vector[push_char_count] = *p++;

        if (log_vector[push_char_count] == '\n') {
            line_count++;
        }

        ++push_char_count;
        i++;
    }

    std::string formatted_str;
    formatted_str.reserve(push_char_count);

    int i = 0;
    while (i < push_char_count) {
        formatted_str += log_vector[i++];
    }

    return formatted_str;
}
