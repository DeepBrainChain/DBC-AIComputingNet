#include "data_query_service.h"
#include "server/server.h"
#include "service_module/service_message_id.h"
#include "../message/matrix_types.h"
#include "service_module/service_module.h"
#include "data/node_info/node_info_collection.h"
#include "data/node_info/service_info_collection.h"
#include "service_module/service_name.h"
#include "../message/service_topic.h"
#include <ctime>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>
#include "leveldb/db.h"
#include <boost/format.hpp>
#include "../message/cmd_message.h"
#include "../message/message_id.h"

int32_t data_query_service::init(bpo::variables_map &options) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_computing_node = true;

        std::string node_info_filename = env_manager::get_home_path().generic_string() + "/.dbc_bash.sh";
        auto rtn = m_node_info_collection.init(node_info_filename);
        if (rtn != E_SUCCESS) {
            return rtn;
        }

        add_self_to_servicelist(options);
    }

    return service_module::init(options);
}

void data_query_service::add_self_to_servicelist(bpo::variables_map &options) {
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
    info.__set_kvs(kvs);

    m_service_info_collection.add(CONF_MANAGER->get_node_id(), info);
}

void data_query_service::init_timer()
{
    // 定期更新本地node_info (cpu usage、mem_usage、state(task_size))
    m_timer_invokers[NODE_INFO_COLLECTION_TIMER] = std::bind(&data_query_service::on_timer_node_info_collection,
                                                             this, std::placeholders::_1);
    add_timer(NODE_INFO_COLLECTION_TIMER, TIMER_INTERVAL_NODE_INFO_COLLECTION, ULLONG_MAX, DEFAULT_STRING);

    // 定期广播自己的节点给其它节点 (30s)
    m_timer_invokers[SERVICE_BROADCAST_TIMER] = std::bind(&data_query_service::on_timer_service_broadcast, this,
                                                          std::placeholders::_1);
    add_timer(SERVICE_BROADCAST_TIMER, CONF_MANAGER->get_timer_service_broadcast_in_second() * 1000, ULLONG_MAX, DEFAULT_STRING);

    // 超时定时器（查询node_info）
    m_timer_invokers[NODE_QUERY_NODE_INFO_TIMER] = std::bind(&data_query_service::on_node_query_node_info_timer, this, std::placeholders::_1);
}

int32_t data_query_service::on_timer_node_info_collection(const std::shared_ptr<core_timer>& timer) {
    if(m_is_computing_node)
        m_node_info_collection.refresh();

    return E_SUCCESS;
}

void data_query_service::init_subscription() {
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_list_node_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_REQ)
    SUBSCRIBE_BUS_MESSAGE(SERVICE_BROADCAST_REQ);
    SUBSCRIBE_BUS_MESSAGE(typeid(get_task_queue_size_resp_msg).name());
}

void data_query_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(typeid(::cmd_list_node_req).name(), &data_query_service::on_cmd_query_node_info_req)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_RSP, &data_query_service::on_node_query_node_info_rsp)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_REQ, &data_query_service::on_node_query_node_info_req)
    BIND_MESSAGE_INVOKER(SERVICE_BROADCAST_REQ, &data_query_service::on_net_service_broadcast_req)
    BIND_MESSAGE_INVOKER(typeid(get_task_queue_size_resp_msg).name(), &data_query_service::on_get_task_queue_size_resp);
}

bool data_query_service::check_req_header(std::shared_ptr<dbc::network::message> &msg) {
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

    if (base->header.exten_info.size() < 4) {
        LOG_ERROR << "header.exten_info size < 4";
        return E_DEFAULT;
    }

    return true;
}

bool data_query_service::check_rsp_header(std::shared_ptr<dbc::network::message> &msg) {
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

bool data_query_service::hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id) {
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

bool data_query_service::check_nonce(const std::string& nonce) {
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


// list node
std::shared_ptr<dbc::network::message> data_query_service::create_node_query_node_info_req_msg(const std::shared_ptr<::cmd_list_node_req> &cmd_req) {
    auto req_content = std::make_shared<dbc::node_query_node_info_req>();
    // header
    req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
    req_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(CONF_MANAGER->get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_message, CONF_MANAGER->get_node_private_key());
    if (signature.empty())  return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = CONF_MANAGER->get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    auto req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_QUERY_NODE_INFO_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_QUERY_NODE_INFO_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);
    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);
    variable_value v1;
    v1.value() = req_msg;
    session->get_context().add("req_msg", v1);

    int32_t ret = this->add_session(session->get_session_id(), session);
    if (E_SUCCESS != ret) {
        this->remove_timer(timer_id);
        LOG_ERROR << "add session failed";
        return nullptr;
    }

    return req_msg;
}

int32_t data_query_service::on_cmd_query_node_info_req(std::shared_ptr<dbc::network::message> &msg)
{
    std::shared_ptr<::cmd_list_node_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_list_node_req>(msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_DEFAULT;
    }

    if (cmd_req_msg->peer_nodes_list.empty()) {
        auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
        cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_resp->id_2_services = m_service_info_collection.get_all();

        TOPIC_MANAGER->publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
        return E_SUCCESS;
    }
    else {
        auto node_req_msg = create_node_query_node_info_req_msg(cmd_req_msg);
        if(node_req_msg == nullptr) {
            auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "create node request failed";
            cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);

            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }

        if (CONNECTION_MANAGER->broadcast_message(node_req_msg) != E_SUCCESS) {
            auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "broadcast failed";
            cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);

            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }
    }

    return E_SUCCESS;
}

int32_t data_query_service::on_node_query_node_info_rsp(std::shared_ptr<dbc::network::message> &msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_query_node_info_rsp> rsp_content =
            std::dynamic_pointer_cast<dbc::node_query_node_info_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check error";
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        CONNECTION_MANAGER->send_resp_message(msg);
        return E_SUCCESS;
    }

    int32_t result = E_DEFAULT;
    std::string result_msg;
    std::map<std::string, std::string> kvs;
    try {
        result = rsp_content->body.result;
        result_msg = rsp_content->body.result_msg;
        kvs = rsp_content->body.kvs;
    } catch (...) {
        LOG_ERROR << "rsp body error";

        result = E_DEFAULT;
        result_msg = "rsp_body error";
    }

    std::shared_ptr<::cmd_list_node_rsp> cmd_rsp_msg = std::make_shared<::cmd_list_node_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = result;
    cmd_rsp_msg->result_info = result_msg;
    cmd_rsp_msg->kvs = kvs;
    TOPIC_MANAGER->publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

int32_t data_query_service::on_node_query_node_info_timer(const std::shared_ptr<core_timer>& timer)
{
    if (nullptr == timer) {
        LOG_ERROR << "null ptr";
        return E_NULL_POINTER;
    }

    //get session
    std::string session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "on_node_query_node_info_timer get session null: " << session_id;
        return E_NULL_POINTER;
    }

    auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
    cmd_resp->header.__set_session_id(session_id);
    cmd_resp->result = E_DEFAULT;
    cmd_resp->result_info = "list node timeout";
    TOPIC_MANAGER->publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);

    session->clear();
    remove_session(session_id);
    return E_SUCCESS;
}


int32_t data_query_service::on_node_query_node_info_req(std::shared_ptr<dbc::network::message> &msg) {
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

    bool hit_self = hit_node(req_peer_nodes, CONF_MANAGER->get_node_id());
    if (hit_self) {
        return query_node_info(req);
    } else {
        req->header.path.push_back(CONF_MANAGER->get_node_id());
        CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
        return E_SUCCESS;
    }
}

int32_t data_query_service::query_node_info(const std::shared_ptr<dbc::node_query_node_info_req> &req) {
    if (req == nullptr) return E_DEFAULT;

    std::map<std::string, std::string> kvs;
    std::vector<std::string> vec_keys = m_node_info_collection.get_all_attributes();
    for (auto &key : vec_keys) {
        kvs[key] = m_node_info_collection.get(key);
    }

    std::shared_ptr<dbc::node_query_node_info_rsp> rsp_content = std::make_shared<dbc::node_query_node_info_rsp>();
    // header
    rsp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
    rsp_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_RSP);
    rsp_content->header.__set_nonce(util::create_nonce());
    rsp_content->header.__set_session_id(req->header.session_id);
    rsp_content->header.__set_path(req->header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = rsp_content->header.nonce + rsp_content->header.session_id;
    std::string sign = util::sign(sign_message, CONF_MANAGER->get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = CONF_MANAGER->get_node_id();
    rsp_content->header.__set_exten_info(exten_info);
    // body
    rsp_content->body.__set_kvs(kvs);

    //rsp msg
    std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
    resp_msg->set_name(NODE_QUERY_NODE_INFO_RSP);
    resp_msg->set_content(rsp_content);
    CONNECTION_MANAGER->send_resp_message(resp_msg);
    return E_SUCCESS;
}


std::shared_ptr<dbc::network::message> data_query_service::create_service_broadcast_req_msg(const service_info_map& mp) {
    auto req_content = std::make_shared<dbc::service_broadcast_req>();
    // header
    req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
    req_content->header.__set_msg_name(SERVICE_BROADCAST_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(util::create_session_id());
    std::vector<std::string> path;
    path.push_back(CONF_MANAGER->get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = req_content->header.nonce + req_content->header.session_id;
    std::string signature = util::sign(sign_message, CONF_MANAGER->get_node_private_key());
    if (signature.empty())  return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = CONF_MANAGER->get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_node_service_info_map(mp);

    auto req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(SERVICE_BROADCAST_REQ);
    req_msg->set_content(req_content);
    return req_msg;
}

int32_t data_query_service::on_timer_service_broadcast(const std::shared_ptr<core_timer>& timer)
{
    auto s_map_size = m_service_info_collection.size();
    if (s_map_size == 0) {
        return E_SUCCESS;
    }

    std::string v;
    v = m_node_info_collection.get("state");
    m_service_info_collection.update(CONF_MANAGER->get_node_id(),"state", v);
    v = m_node_info_collection.get("version");
    m_service_info_collection.update(CONF_MANAGER->get_node_id(),"version", v);
    m_service_info_collection.update_own_node_time_stamp(CONF_MANAGER->get_node_id());
    m_service_info_collection.remove_unlived_nodes(CONF_MANAGER->get_timer_service_list_expired_in_second());

    auto service_info_map = m_service_info_collection.get_change_set();
    if(!service_info_map.empty()) {
        auto service_broadcast_req = create_service_broadcast_req_msg(service_info_map);
        if (service_broadcast_req != nullptr) {
            CONNECTION_MANAGER->broadcast_message(service_broadcast_req);
        }
    }

    return E_SUCCESS;
}

int32_t data_query_service::on_net_service_broadcast_req(std::shared_ptr<dbc::network::message> &msg) {
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

    m_service_info_collection.add(mp);
    return E_SUCCESS;
}


int32_t data_query_service::on_get_task_queue_size_resp(std::shared_ptr<dbc::network::message> &msg)
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
