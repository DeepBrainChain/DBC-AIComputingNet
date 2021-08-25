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

node_request_service::node_request_service() {

}

int32_t node_request_service::init(bpo::variables_map &options) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_computing_node = true;

        std::string node_info_filename = env_manager::instance().get_home_path().generic_string() + "/.dbc_bash.sh";
        auto rtn = m_node_info_collection.init(node_info_filename);
        if (rtn != E_SUCCESS) {
            return rtn;
        }

        add_self_to_servicelist(options);
    }

    return service_module::init(options);
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

    const char* ATTRS[] = {
            "state",
            "version"
    };

    std::map<std::string, std::string> kvs;
    int num_of_attrs = sizeof(ATTRS) / sizeof(char*);
    for(int i = 0; i < num_of_attrs; i++) {
        auto k = ATTRS[i];
        kvs[k] = m_node_info_collection.get(k);
    }
    kvs["pub_key"] = conf_manager::instance().get_pub_key();
    info.__set_kvs(kvs);

    service_info_collection::instance().add(conf_manager::instance().get_node_id(), info);
}

void node_request_service::init_timer() {
    // 配置文件：timer_ai_training_task_schedule_in_second（15秒）
    m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&node_request_service::on_training_task_timer,
        this, std::placeholders::_1);
    m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER,
        conf_manager::instance().get_timer_ai_training_task_schedule_in_second() *
        1000, ULLONG_MAX, DEFAULT_STRING);

    // 10分钟
    m_timer_invokers[AI_PRUNE_TASK_TIMER] = std::bind(&node_request_service::on_prune_task_timer, this,
        std::placeholders::_1);
    m_prune_task_timer_id = this->add_timer(AI_PRUNE_TASK_TIMER, AI_PRUNE_TASK_TIMER_INTERVAL, ULLONG_MAX,
        DEFAULT_STRING);

    // 定期更新本地node_info (cpu usage、mem_usage、state(task_size))
    m_timer_invokers[NODE_INFO_COLLECTION_TIMER] = std::bind(&node_request_service::on_timer_node_info_collection, this, std::placeholders::_1);
    add_timer(NODE_INFO_COLLECTION_TIMER, TIMER_INTERVAL_NODE_INFO_COLLECTION, ULLONG_MAX, DEFAULT_STRING);

    // broadcast service list
    m_timer_invokers[SERVICE_BROADCAST_TIMER] = std::bind(&node_request_service::on_timer_service_broadcast, this, std::placeholders::_1);
    add_timer(SERVICE_BROADCAST_TIMER, conf_manager::instance().get_timer_service_broadcast_in_second() * 1000, ULLONG_MAX, DEFAULT_STRING);
}

void node_request_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_REQ, &node_request_service::on_node_create_task_req);
    BIND_MESSAGE_INVOKER(NODE_START_TASK_REQ, &node_request_service::on_node_start_task_req);
    BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_REQ, &node_request_service::on_node_restart_task_req);
    BIND_MESSAGE_INVOKER(NODE_STOP_TASK_REQ, &node_request_service::on_node_stop_task_req);
    BIND_MESSAGE_INVOKER(NODE_RESET_TASK_REQ, &node_request_service::on_node_reset_task_req);
    BIND_MESSAGE_INVOKER(NODE_DELETE_TASK_REQ, &node_request_service::on_node_delete_task_req);
    BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_REQ, &node_request_service::on_node_task_logs_req);
    BIND_MESSAGE_INVOKER(NODE_LIST_TASK_REQ, &node_request_service::on_node_list_task_req);

    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_REQ, &node_request_service::on_node_query_node_info_req)

    BIND_MESSAGE_INVOKER(SERVICE_BROADCAST_REQ, &node_request_service::on_net_service_broadcast_req)

    BIND_MESSAGE_INVOKER(typeid(get_task_queue_size_req_msg).name(), &node_request_service::on_get_task_queue_size_req)
    BIND_MESSAGE_INVOKER(typeid(get_task_queue_size_resp_msg).name(), &node_request_service::on_get_task_queue_size_resp)
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

    // broadcast service list
    SUBSCRIBE_BUS_MESSAGE(SERVICE_BROADCAST_REQ);

    SUBSCRIBE_BUS_MESSAGE(typeid(get_task_queue_size_req_msg).name());
    SUBSCRIBE_BUS_MESSAGE(typeid(get_task_queue_size_resp_msg).name());
}

int32_t node_request_service::service_init(bpo::variables_map& options) {
    auto fresult = m_task_scheduler.Init();
    int32_t ret = std::get<0>(fresult);

    if (ret != E_SUCCESS) {
        return E_DEFAULT;
    }
    else {
        ret = m_sessionid_db.init_db(env_manager::instance().get_db_path(), "sessionid.db");
        if (!ret) {
            LOG_ERROR << "init sessionid_db failed";
            return E_DEFAULT;
        }
        std::map<std::string, std::shared_ptr<dbc::owner_sessionid>> wallet_sessionid;
        m_sessionid_db.load(wallet_sessionid);
        for (auto& it : wallet_sessionid) {
            m_wallet_sessionid.insert({it.first, it.second->session_id});
        }

        std::string ws_url = "wss://" + conf_manager::instance().get_dbc_chain_domain();
        m_websocket_client.init(ws_url);
        m_websocket_client.enable_auto_reconnection();
        m_websocket_client.set_message_callback(std::bind(&node_request_service::on_ws_msg, this, std::placeholders::_1, std::placeholders::_2));
        m_websocket_client.start();
        return E_SUCCESS;
    }
}

int32_t node_request_service::service_exit() {
    remove_timer(m_training_task_timer_id);
    remove_timer(m_prune_task_timer_id);
    return E_SUCCESS;
}

int32_t node_request_service::on_timer_node_info_collection(const std::shared_ptr<core_timer>& timer) {
    if(m_is_computing_node)
        m_node_info_collection.refresh();

    return E_SUCCESS;
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

bool node_request_service::check_req_header(std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<dbc::network::msg_base> base = msg->content;
    if (!base) {
        LOG_ERROR << "msg.containt is nullptr";
        return false;
    }

    if (!util::check_id(base->header.nonce)) {
        LOG_ERROR << "header.nonce check failed";
        return false;
    }

    if (!util::check_id(base->header.session_id)) {
        LOG_ERROR << "header.session_id check failed";
        return false;
    }

    if (base->header.path.size() <= 0) {
        LOG_ERROR << "header.path size <= 0";
        return false;
    }

    if (base->header.exten_info.size() < 1) {
        LOG_ERROR << "header.exten_info size < 4";
        return false;
    }

    return true;
}

bool node_request_service::check_rsp_header(std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<dbc::network::msg_base> base = msg->content;
    if (!base) {
        LOG_ERROR << "msg.containt is nullptr";
        return false;
    }

    if (!util::check_id(base->header.nonce)) {
        LOG_ERROR << "header.nonce check failed";
        return false;
    }

    if (!util::check_id(base->header.session_id)) {
        LOG_ERROR << "header.session_id check failed";
        return false;
    }

    if (base->header.exten_info.size() < 4) {
        LOG_ERROR << "header.exten_info size < 4";
        return E_DEFAULT;
    }

    return true;
}

int32_t node_request_service::on_node_create_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_create_task_req> req =
        std::dynamic_pointer_cast<dbc::node_create_task_req>(msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    // decrypt
    std::string pub_key = req->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        LOG_ERROR << "pub_key or priv_key is empty";
        return E_DEFAULT;
    }

    std::string ori_message = decrypt_data(req->body.data, pub_key, priv_key);

    std::shared_ptr<dbc::node_create_task_req_data> data = std::make_shared<dbc::node_create_task_req_data>();
    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
    task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
    dbc::network::binary_protocol proto(task_buf.get());
    data->read(&proto);

    if (!check_nonce(data->nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    std::string sign_msg = data->nonce;
    if (!util::verify_sign(data->sign, sign_msg, data->wallet)) {
        LOG_ERROR << "verify sign error";
        return E_DEFAULT;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_create(req->header, data);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_start_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_start_task_req> req =
        std::dynamic_pointer_cast<dbc::node_start_task_req>(msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_task_id = req->body.task_id;
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "ai power provider service task_id error ";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req->header.nonce + req_additional;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_start(req);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_stop_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_stop_task_req> req = std::dynamic_pointer_cast<dbc::node_stop_task_req>(
        msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_task_id = req->body.task_id;
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "ai power provider service on_stop_training_req task_id error ";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req->header.nonce + req_additional;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_stop(req);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_restart_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_restart_task_req> req =
            std::dynamic_pointer_cast<dbc::node_restart_task_req>(msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_task_id = req->body.task_id;
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "ai power provider service task_id error ";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req->header.nonce + req_additional;
    if (req->header.exten_info.size() < 3) {
        LOG_ERROR << "exten info error.";
        return E_DEFAULT;
    }
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_restart(req);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_reset_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_reset_task_req> req = std::dynamic_pointer_cast<dbc::node_reset_task_req>(
            msg->get_content());
    if (req == nullptr) return E_DEFAULT;

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_task_id = req->body.task_id;
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "ai power provider service on_stop_training_req task_id error ";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req->header.nonce + req_additional;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_reset(req);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_delete_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_delete_task_req> req = std::dynamic_pointer_cast<dbc::node_delete_task_req>(
            msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req id nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_task_id = req->body.task_id;
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "ai power provider service on_stop_training_req task_id error ";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req->header.nonce + req_additional;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_delete(req);
    }
    else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_task_logs_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_task_logs_req> req_content = std::dynamic_pointer_cast<dbc::node_task_logs_req>(msg->get_content());
    if (req_content == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req_content->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    // check req body
    std::string req_task_id;
    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    int8_t req_head_or_tail;
    int16_t req_number_of_lines;
    try {
        req_task_id = req_content->body.task_id;
        req_additional = req_content->body.additional;
        req_peer_nodes = req_content->body.peer_nodes_list;
        req_head_or_tail = req_content->body.head_or_tail;
        req_number_of_lines = req_content->body.number_of_lines;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!util::check_id(req_task_id)) {
        LOG_ERROR << "taskid error ";
        return E_DEFAULT;
    }

    if (GET_LOG_HEAD != req_head_or_tail && GET_LOG_TAIL != req_head_or_tail) {
        LOG_ERROR << "ai power provider service on logs req log direction error";
        return E_DEFAULT;
    }

    if (req_number_of_lines > MAX_NUMBER_OF_LINES || req_number_of_lines < 0) {
        LOG_ERROR << "ai power provider service on logs req number of lines error: "
            << req_number_of_lines;
        return E_DEFAULT;
    }

    std::string sign_req_msg =
        req_task_id + req_content->header.nonce + req_content->header.session_id + req_additional;
    if (!util::verify_sign(req_content->header.exten_info["sign"], sign_req_msg, req_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "fake message. " << req_content->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_logs(req_content);
    }
    else {
        req_content->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_list_task_req(std::shared_ptr<dbc::network::message>& msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_list_task_req> req_content = std::dynamic_pointer_cast<dbc::node_list_task_req>(
        msg->get_content());
    if (req_content == nullptr) return E_DEFAULT;

    if (!check_nonce(req_content->header.nonce)) {
        LOG_ERROR << "ai power provider service nonce error ";
        return E_DEFAULT;
    }

    //check req.body
    std::string req_task_id;
    std::vector<std::string> req_peer_nodes;
    std::string req_additional;
    try {
        req_task_id = req_content->body.task_id;
        req_peer_nodes = req_content->body.peer_nodes_list;
        req_additional = req_content->body.additional;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    if (!req_task_id.empty() && !util::check_id(req_task_id)) {
        LOG_ERROR << "task_id check failed";
        return E_DEFAULT;
    }

    std::string sign_msg = req_task_id + req_content->header.nonce + req_content->header.session_id + req_additional;
    if (!util::verify_sign(req_content->header.exten_info["sign"], sign_msg, req_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "fake message. " << req_content->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    // 检查是否命中当前节点
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return task_list(req_content);
    } else {
        req_content->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::on_node_query_node_info_req(std::shared_ptr<dbc::network::message> &msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_query_node_info_req> req =
            std::dynamic_pointer_cast<dbc::node_query_node_info_req>(msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "nonce check error";
        return E_DEFAULT;
    }

    std::string req_additional;
    std::vector<std::string> req_peer_nodes;
    try {
        req_additional = req->body.additional;
        req_peer_nodes = req->body.peer_nodes_list;
    } catch (...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    std::string sign_msg = req->header.nonce + req_additional;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        return query_node_info(req);
    } else {
        req->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t node_request_service::query_node_info(const std::shared_ptr<dbc::node_query_node_info_req> &req) {
    if (req == nullptr) return E_DEFAULT;

    std::map<std::string, std::string> kvs;
    std::vector<std::string> vec_keys = m_node_info_collection.get_all_attributes();
    for (auto &key : vec_keys) {
        kvs[key] = m_node_info_collection.get(key);
    }

    std::shared_ptr<dbc::node_query_node_info_rsp> rsp_content = std::make_shared<dbc::node_query_node_info_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_kvs(kvs);

    //rsp msg
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_name(NODE_QUERY_NODE_INFO_RSP);
    resp_msg->set_content(rsp_content);
    dbc::network::connection_manager::instance().send_resp_message(resp_msg);
    return E_SUCCESS;
}

int32_t node_request_service::on_get_task_queue_size_req(std::shared_ptr<dbc::network::message>& msg) {
    auto resp = std::make_shared<get_task_queue_size_resp_msg>();

    auto task_num = m_task_scheduler.GetRunningTaskSize();
    resp->set_task_size(task_num);

    auto resp_msg = std::dynamic_pointer_cast<dbc::network::message>(resp);

    topic_manager::instance().publish<int32_t>(typeid(get_task_queue_size_resp_msg).name(), resp_msg);

    return E_SUCCESS;
}

int32_t node_request_service::on_get_task_queue_size_resp(std::shared_ptr<dbc::network::message> &msg)
{
    std::shared_ptr<get_task_queue_size_resp_msg> resp =
            std::dynamic_pointer_cast<get_task_queue_size_resp_msg>(msg);
    if (nullptr == resp) {
        LOG_ERROR<< "on_get_task_queue_size_resp null ptr";
        return E_NULL_POINTER;
    }

    m_node_info_collection.set("state", std::to_string(resp->get_task_size()));
    return E_SUCCESS;
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
    exten_info["sign_algo"] = ECDSA;
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

int32_t node_request_service::on_timer_service_broadcast(const std::shared_ptr<core_timer>& timer)
{
    auto s_map_size = service_info_collection::instance().size();
    if (s_map_size == 0) {
        return E_SUCCESS;
    }

    std::string v;
    v = m_node_info_collection.get("state");
    service_info_collection::instance().update(conf_manager::instance().get_node_id(),"state", v);
    v = m_node_info_collection.get("version");
    service_info_collection::instance().update(conf_manager::instance().get_node_id(),"version", v);
    service_info_collection::instance().update_own_node_time_stamp(conf_manager::instance().get_node_id());
    service_info_collection::instance().remove_unlived_nodes(conf_manager::instance().get_timer_service_list_expired_in_second());

    auto service_info_map = service_info_collection::instance().get_change_set();
    if(!service_info_map.empty()) {
        auto service_broadcast_req = create_service_broadcast_req_msg(service_info_map);
        if (service_broadcast_req != nullptr) {
            dbc::network::connection_manager::instance().broadcast_message(service_broadcast_req);
        }
    }

    return E_SUCCESS;
}

int32_t node_request_service::on_net_service_broadcast_req(std::shared_ptr<dbc::network::message> &msg) {
    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::service_broadcast_req> req =
            std::dynamic_pointer_cast<dbc::service_broadcast_req>(msg->get_content());
    if (req == nullptr) {
        LOG_ERROR << "req is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(req->header.nonce)) {
        LOG_ERROR << "nonce check error";
        return E_DEFAULT;
    }

    std::string sign_msg = req->header.nonce + req->header.session_id;
    if (!util::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign error." << req->header.exten_info["origin_id"];
        return E_DEFAULT;
    }

    service_info_map mp;
    try {
        mp = req->body.node_service_info_map;
    } catch(...) {
        LOG_ERROR << "req body error";
        return E_DEFAULT;
    }

    service_info_collection::instance().add(mp);
    return E_SUCCESS;
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

        const rapidjson::Value &v_machineOwner = v_result["machineOwner"];
        if (!v_machineOwner.IsString()) {
            ret = E_DEFAULT;
            ret_msg = "rsp_json machineOwner is not string";
            break;
        }
        std::string owner_wallet = v_machineOwner.GetString();

        //owner_wallet = "e8a1ce95c614d55ea28730123e6b2dff3f5cf70d3a7bfe73f408f6513afc0c19";

        bool can_do = false;
        if (owner_wallet == m_create_data->wallet) {
            can_do = true;

            auto it = m_wallet_sessionid.find(owner_wallet);
            if (it == m_wallet_sessionid.end()) {
                std::string id = util::create_session_id();
                m_wallet_sessionid[owner_wallet] = id;
                std::shared_ptr<dbc::owner_sessionid> sessionid(new dbc::owner_sessionid());
                sessionid->wallet = owner_wallet;
                sessionid->session_id = id;
                m_sessionid_db.write(sessionid);
            }
        } else {
            if (!m_create_data->session_id.empty() && !m_create_data->session_id_sign.empty()) {
                std::string sign_msg = m_create_data->session_id;
                if (util::verify_sign(m_create_data->session_id_sign, sign_msg, owner_wallet)) {
                    auto it = m_wallet_sessionid.find(owner_wallet);
                    if (it != m_wallet_sessionid.end()) {
                        can_do = true;
                    } else {
                        ret = E_DEFAULT;
                        ret_msg = "session_id is invalid";
                        LOG_ERROR << "session_id is invalid";
                        break;
                    }
                } else {
                    ret = E_DEFAULT;
                    ret_msg = "session_id is invalid";
                    LOG_ERROR << "session_id is invalid";
                    break;
                }
            }
        }

        if (can_do) {
            task_id = util::create_task_id();
            login_password = generate_pwd();
            auto fresult = m_task_scheduler.CreateTask(task_id, login_password, m_create_data->additional);
            ret = std::get<0>(fresult);
            ret_msg = std::get<1>(fresult);
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
    exten_info["sign_algo"] = ECDSA;
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

int32_t node_request_service::task_create(const dbc::network::base_header& header, const std::shared_ptr<dbc::node_create_task_req_data>& data) {
    m_create_header = header;
    m_create_data = data;

    if (m_websocket_client.is_connected()) {
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
                               + conf_manager::instance().get_node_id() + R"("]})";
        m_websocket_client.send(str_send);
        LOG_INFO << "ws_send: " << str_send;
    } else {
        LOG_ERROR << "websocket is disconnect";
        return E_SUCCESS;
    }

    return E_SUCCESS;
}

int32_t node_request_service::task_start(const std::shared_ptr<dbc::node_start_task_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    auto fresult = m_task_scheduler.StartTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_start_task_rsp> rsp_content = std::make_shared<dbc::node_start_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_START_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_stop(const std::shared_ptr<dbc::node_stop_task_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    auto fresult = m_task_scheduler.StopTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_stop_task_rsp> rsp_content = std::make_shared<dbc::node_stop_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_STOP_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_restart(const std::shared_ptr<dbc::node_restart_task_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    auto fresult = m_task_scheduler.RestartTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_restart_task_rsp> rsp_content = std::make_shared<dbc::node_restart_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_RESTART_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_reset(const std::shared_ptr<dbc::node_reset_task_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    auto fresult = m_task_scheduler.ResetTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_reset_task_rsp> rsp_content = std::make_shared<dbc::node_reset_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_RESET_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_delete(const std::shared_ptr<dbc::node_delete_task_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    auto fresult = m_task_scheduler.DeleteTask(task_id);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    std::shared_ptr<dbc::node_delete_task_rsp> rsp_content = std::make_shared<dbc::node_delete_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_DELETE_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_logs(const std::shared_ptr<dbc::node_task_logs_req>& req) {
    std::string task_id = req->body.task_id;
    if (!util::check_id(task_id)) return E_DEFAULT;

    std::string log_content;
    auto fresult = m_task_scheduler.GetTaskLog(req->body.task_id, (ETaskLogDirection) req->body.head_or_tail,
                                               req->body.number_of_lines, log_content);
    int32_t ret = std::get<0>(fresult);
    std::string ret_msg = std::get<1>(fresult);

    if (GET_LOG_HEAD == req->body.head_or_tail) {
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
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id + log_content;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::task_list(const std::shared_ptr<dbc::node_list_task_req> &req) {
    int32_t result = E_SUCCESS;
    std::string result_msg = "task list successful";

    std::string task_id = req->body.task_id;
    if (!task_id.empty() && !util::check_id(task_id)) {
        result = E_DEFAULT;
        result_msg = "task_id check failed";
    }

    std::stringstream ss_tasks;
    if (result == E_SUCCESS) {
        if (req->body.task_id.empty()) {
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
                ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024) << "\"";
                ss_tasks << ", \"disk_system\":" << "\"" << size_to_string(g_image_size, 1024 * 1024 * 1024) << "\"";
                ss_tasks << ", \"disk_data\":" << "\"" << size_to_string(disk_data, 1024 * 1024) << "\"";

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
            auto task = m_task_scheduler.FindTask(req->body.task_id);
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
                ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024) << "\"";
                ss_tasks << ", \"disk_system\":" << "\"" << size_to_string(g_image_size, 1024 * 1024 * 1024) << "\"";
                ss_tasks << ", \"disk_data\":" << "\"" << size_to_string(disk_data, 1024 * 1024) << "\"";

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
    }

    std::shared_ptr<dbc::node_list_task_rsp> rsp_content = std::make_shared<dbc::node_list_task_rsp>();
    // header
    rsp_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_content->header.__set_msg_name(NODE_LIST_TASK_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
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
    return E_SUCCESS;
}

int32_t node_request_service::on_training_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.ProcessTask();

    static int count = 0;
    count++;
    if ((count % 10) == 0) {
        //m_task_schedule->update_gpu_info_from_proc();
    }

    return E_SUCCESS;
}

int32_t node_request_service::on_prune_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.PruneTask();
    return E_SUCCESS;
}

int32_t node_request_service::check_sign(const std::string& message, const std::string& sign,
                                         const std::string& origin_id, const std::string& sign_algo) {
    if (sign_algo != ECDSA) {
        LOG_ERROR << "sign_algorithm error.";
        return E_DEFAULT;
    }

    if (origin_id.empty() || sign.empty()) {
        LOG_ERROR << "sign error.";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

std::string node_request_service::get_task_id(const std::string& server_specification) {
    if (server_specification.empty()) {
        return "";
    }

    std::string task_id;
    if (!server_specification.empty()) {
        std::stringstream ss;
        ss << server_specification;
        boost::property_tree::ptree pt;

        try {
            boost::property_tree::read_json(ss, pt);
            if (pt.count("task_id") != 0) {
                task_id = pt.get<std::string>("task_id");
            }
        }
        catch (...) {

        }
    }

    return task_id;
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
