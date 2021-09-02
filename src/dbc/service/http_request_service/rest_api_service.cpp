#include "rest_api_service.h"
#include <boost/exception/all.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"
#include "rest_handler.h"
#include "log/log.h"
#include "service_module/service_message_id.h"
#include "timer/time_tick_notification.h"
#include "../message/protocol_coder/matrix_coder.h"
#include "../message/message_id.h"
#include "data/service_info/service_info_collection.h"

#define SUBSCRIBE_RESP_MSG(cmd)  \
topic_manager::instance().subscribe(typeid(cmd).name(),[this](std::shared_ptr<cmd> &rsp) {  \
    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>(); \
    msg->set_name(typeid(cmd).name()); \
    msg->set_content(rsp);\
    this->send(msg);\
});

extern std::chrono::high_resolution_clock::time_point server_start_time;

#define HTTP_REQUEST_TIMEOUT_EVENT   "http_request_timeout_event"
#define HTTP_REQUEST_KEY             "hreq_context"

int32_t rest_api_service::init(bpo::variables_map &options) {
    const dbc::network::http_path_handler uri_prefixes[] = {
            {"/tasks", false, std::bind(&rest_api_service::rest_task, this, std::placeholders::_1, std::placeholders::_2)},
            {"/peers",        false, std::bind(&rest_api_service::rest_peers, this, std::placeholders::_1, std::placeholders::_2)},
            {"/stat",         false, std::bind(&rest_api_service::rest_stat, this, std::placeholders::_1, std::placeholders::_2)},
            {"/mining_nodes", false, std::bind(&rest_api_service::rest_mining_nodes, this, std::placeholders::_1, std::placeholders::_2)},
            {"/config",       false, std::bind(&rest_api_service::rest_config, this, std::placeholders::_1, std::placeholders::_2)},
            {"",              true,  std::bind(&rest_api_service::rest_api_version, this, std::placeholders::_1, std::placeholders::_2)},
    };

    for (const auto &uri_prefixe : uri_prefixes) {
        m_path_handlers.emplace_back(REST_API_URI + uri_prefixe.m_prefix,
                                     uri_prefixe.m_exact_match, uri_prefixe.m_handler);
    }

    const dbc::network::response_msg_handler rsp_handlers[] = {
            {     NODE_CREATE_TASK_RSP, std::bind(&rest_api_service::on_node_create_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_START_TASK_RSP, std::bind(&rest_api_service::on_node_start_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {       NODE_STOP_TASK_RSP, std::bind(&rest_api_service::on_node_stop_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {    NODE_RESTART_TASK_RSP, std::bind(&rest_api_service::on_node_restart_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_RESET_TASK_RSP, std::bind(&rest_api_service::on_node_reset_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_DELETE_TASK_RSP, std::bind(&rest_api_service::on_node_delete_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {       NODE_TASK_LOGS_RSP, std::bind(&rest_api_service::on_node_task_logs_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {       NODE_LIST_TASK_RSP, std::bind(&rest_api_service::on_node_list_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_MODIFY_TASK_RSP, std::bind(&rest_api_service::on_node_modify_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_QUERY_NODE_INFO_RSP, std::bind(&rest_api_service::on_node_query_node_info_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {typeid(cmd_get_peer_nodes_rsp).name(), std::bind(&rest_api_service::on_cmd_get_peer_nodes_rsp, this, std::placeholders::_1, std::placeholders::_2)},
    };

    for (const auto &rsp_handler : rsp_handlers) {
        std::string name = rsp_handler.name;
        m_rsp_handlers[name] = rsp_handler.handler;
    }

    service_module::init(options);
    return E_SUCCESS;
}

void rest_api_service::init_timer() {
    m_timer_invokers[NODE_CREATE_TASK_TIMER] = std::bind(&rest_api_service::on_node_create_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_START_TASK_TIMER] = std::bind(&rest_api_service::on_node_start_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_STOP_TASK_TIMER] = std::bind(&rest_api_service::on_node_stop_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_RESTART_TASK_TIMER] = std::bind(&rest_api_service::on_node_restart_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_RESET_TASK_TIMER] = std::bind(&rest_api_service::on_node_reset_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_DELETE_TASK_TIMER] = std::bind(&rest_api_service::on_node_delete_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_TASK_LOGS_TIMER] = std::bind(&rest_api_service::on_node_task_logs_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_LIST_TASK_TIMER] = std::bind(&rest_api_service::on_node_list_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_MODIFY_TASK_TIMER] = std::bind(&rest_api_service::on_node_modify_task_timer, this, std::placeholders::_1);
    m_timer_invokers[NODE_QUERY_NODE_INFO_TIMER] = std::bind(&rest_api_service::on_node_query_node_info_timer, this, std::placeholders::_1);
}

void rest_api_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_START_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_STOP_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_RESET_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_DELETE_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_LIST_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_MODIFY_TASK_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_RSP, &rest_api_service::on_call_rsp_handler)
    BIND_MESSAGE_INVOKER(BINARY_FORWARD_MSG, &rest_api_service::on_binary_forward)
}

void rest_api_service::init_subscription() {
    SUBSCRIBE_BUS_MESSAGE(NODE_CREATE_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_START_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_STOP_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_RESTART_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_RESET_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_DELETE_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_MODIFY_TASK_RSP)
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_RSP)
    SUBSCRIBE_BUS_MESSAGE(BINARY_FORWARD_MSG)
}

void rest_api_service::on_http_request_event(std::shared_ptr<dbc::network::http_request> &hreq) {
    std::string str_uri = hreq->get_uri();
    if (check_invalid_http_request(hreq)) {
        LOG_ERROR << "the request is invalid,so reject it," << str_uri;
        return;
    }

    std::string path;
    bool match;
    std::vector<dbc::network::http_path_handler>::const_iterator i = m_path_handlers.begin();
    std::vector<dbc::network::http_path_handler>::const_iterator iend = m_path_handlers.end();
    for (; i != iend; ++i) {
        match = false;
        if (i->m_exact_match) {
            match = (str_uri == i->m_prefix);
        } else {
            match = (str_uri.substr(0, i->m_prefix.size()) == i->m_prefix);
        }

        if (match) {
            path = str_uri.substr(i->m_prefix.size());
            break;
        }
    }

    if (i == iend) {
        hreq->reply_comm_rest_err(HTTP_NOTFOUND, RPC_METHOD_NOT_FOUND, "not support RESTful api");
        return;
    }

    i->m_handler(hreq, path);
}

bool rest_api_service::check_invalid_http_request(std::shared_ptr<dbc::network::http_request> &hreq) {
    if (hreq->get_request_method() == dbc::network::http_request::UNKNOWN) {
        hreq->reply_comm_rest_err(HTTP_BADMETHOD, RPC_METHOD_DEPRECATED, "not support method");
        return true;
    }

    auto time_span_ms = duration_cast<milliseconds>(high_resolution_clock::now() - server_start_time);

    if (time_span_ms.count() < MIN_INIT_HTTP_SERVER_TIME) {

        hreq->reply_comm_rest_err(HTTP_OK, RPC_REQUEST_INTERRUPTED, "Request is interrupted.Try again later.");

        return true;
    }

    std::string str_uri = hreq->get_uri();
    if (str_uri.substr(0, REST_API_URI.size()) != REST_API_URI) {

        hreq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "The api version is invalid. Current "
                                                                        "version is " + std::string(REST_API_URI) + ".");
        return true;
    }

    return false;
}

void rest_api_service::on_call_rsp_handler(const std::shared_ptr<dbc::network::message> &rsp_msg) {
    if (rsp_msg == nullptr) {
        LOG_ERROR << "rsp_msg is nullptr";
        return;
    }

    auto node_rsp_msg = rsp_msg->content;
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::string &name = rsp_msg->get_name();
    const std::string &session_id = node_rsp_msg->header.session_id;

    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_DEBUG << "rsp name: " << name << ",but cannot find  session_id:" << session_id;
        dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
        return;
    }

    remove_timer(session->get_timer_id());

    try {
        variables_map &vm = session->get_context().get_args();
        if (0 == vm.count(HTTP_REQUEST_KEY)) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null hreq_key";
            remove_session(session_id);
            return;
        }

        auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<dbc::network::http_request_context>>();
        if (nullptr == hreq_context) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null hreq_context";
            remove_session(session_id);
            return;
        }

        auto it = m_rsp_handlers.find(name);
        if (it == m_rsp_handlers.end()) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null rsp_handler";
            remove_session(session_id);
            return;
        }

        LOG_INFO << "rsp name: " << name << ",session_id:" << session_id << ", and call rsp_handler";

        auto func = it->second;
        func(hreq_context, rsp_msg);

        session->clear();
        remove_session(session_id);
    } catch (std::exception &e) {
        LOG_ERROR << e.what();
    }
}

int32_t rest_api_service::get_startup_time() {
    auto time_span_ms = duration_cast<milliseconds>(high_resolution_clock::now() - server_start_time);
    int32_t fractional_seconds = (int32_t) (time_span_ms.count() / 1000);
    return fractional_seconds;
}

bool rest_api_service::check_rsp_header(const std::shared_ptr<dbc::network::message> &rsp_msg) {
    if (!rsp_msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<dbc::network::msg_base> base = rsp_msg->content;
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
        return false;
    }

    return true;
}

bool rest_api_service::check_nonce(const std::string& nonce) {
    if (!util::check_id(nonce)) {
        return false;
    }

    if (m_nonceCache.contains(nonce)) {
        return false;
    }
    else {
        m_nonceCache.insert(nonce, 1);
        return true;
    }
}

int32_t rest_api_service::create_request_session(const std::string& timer_id,
                                              const std::shared_ptr<dbc::network::http_request>& hreq,
                                              const std::shared_ptr<dbc::network::message>& req_msg,
                                              const std::string& session_id) {
    auto hreq_context = std::make_shared<dbc::network::http_request_context>();
    hreq_context->m_hreq = hreq;
    hreq_context->m_req_msg = req_msg;

    do {
        std::string str_uri = hreq->get_uri();

        if (get_session_count() >= MAX_SESSION_COUNT) {
            LOG_ERROR << "str_uri:" << str_uri << ";m_sessions.size() exceed size";
            hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "session pool is full");
            return E_DEFAULT;
        }

        uint32_t id = add_timer(timer_id, MAX_WAIT_HTTP_RESPONSE_TIME, ONLY_ONE_TIME, session_id);
        if (INVALID_TIMER_ID == id) {
            LOG_ERROR << "str_uri:" << str_uri << ";add_timer failed";
            hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "System error.");
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = std::make_shared<service_session>(id, session_id);
        variable_value val;
        val.value() = hreq_context;
        session->get_context().add(HTTP_REQUEST_KEY, val);

        int32_t ret = this->add_session(session_id, session);
        if (E_SUCCESS != ret) {
            remove_timer(id);
            LOG_ERROR << "str_uri:" << str_uri << ";add_session failed";
            hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "Session error..");
            return E_DEFAULT;
        }

        LOG_INFO << "str_uri:" << str_uri << ";add_session:" << session_id << ";msg_name:" << req_msg->get_name();
    } while (0);

    return E_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

// /tasks
void rest_api_service::rest_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    // GET /tasks 列出所有task列表
    if (path_list.empty()) {
        rest_list_task(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        // create
        if (first_param == "start") {
            rest_create_task(httpReq, path);
        } else {
            rest_list_task(httpReq, path);
        }
        return;
    }

    if (path_list.size() == 2) {
        const std::string &second_param = path_list[1];
        // start <task_id>
        if (second_param == "start") {
            rest_start_task(httpReq, path);
        }
        // /tasks/<task_id>/restart
        else if (second_param == "restart") {
            rest_restart_task(httpReq, path);
        }
        // /tasks/<task_id>/stop 停止某个task
        else if (second_param == "stop") {
            rest_stop_task(httpReq, path);
        }
        // /tasks/<task_id>/reset
        else if (second_param == "reset") {
            rest_reset_task(httpReq, path);
        }
        // /tasks/<task_id>/destroy
        else if (second_param == "delete") {
            rest_delete_task(httpReq, path);
        }
        // /tasks/<task_id>/logs
        else if (second_param == "logs") {
            rest_task_logs(httpReq, path);
        }
        // /tasks/<task_id>/modify
        else if (second_param == "modify") {
            rest_modify_task(httpReq, path);
        }
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

// create task
void rest_api_service::rest_create_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, use /api/v1/tasks/start");
        return;
    }

    req_body body;

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, use /api/v1/tasks/start");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "http request body is invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is invalid json");
        return;
    }

    // peer_nodes_list
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            uint32_t list_size = doc["peer_nodes_list"].Size();
            if (list_size < 1) {
                LOG_ERROR << "http request's <peer_nodes_list> size < 1";
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <peer_nodes_list> size < 1");
                return;
            }

            // 暂时只支持一次操作1个节点
            std::string node(doc["peer_nodes_list"][0].GetString());
            body.peer_nodes_list.push_back(node);
        } else {
            LOG_ERROR << "http request's <peer_nodes_list> is not array";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <peer_nodes_list> is not array");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <peer_nodes_list>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <peer_nodes_list>");
        return;
    }
    // additional
    if (doc.HasMember("additional")) {
        if (doc["additional"].IsObject()) {
            const rapidjson::Value &obj = doc["additional"];
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            obj.Accept(writer);
            body.additional = buffer.GetString();
        } else {
            LOG_ERROR << "http request's <additional> is not object";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <additional> is not object");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <additional>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <additional>");
        return;
    }
    // sign
    if (doc.HasMember("sign")) {
        if (doc["sign"].IsString()) {
            body.sign = doc["sign"].GetString();
        } else {
            LOG_ERROR << "http request's <sign> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <sign> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <sign>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <sign>");
        return;
    }
    // nonce
    if (doc.HasMember("nonce")) {
        if (doc["nonce"].IsString()) {
            body.nonce = doc["nonce"].GetString();
        } else {
            LOG_ERROR << "http request's <nonce> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <nonce> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <nonce>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <nonce>");
        return;
    }
    // wallet
    if (doc.HasMember("wallet")) {
        if (doc["wallet"].IsString()) {
            body.wallet = doc["wallet"].GetString();
        } else {
            LOG_ERROR << "http request's <wallet> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <wallet> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <wallet>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <wallet>");
        return;
    }
    // session_id
    if (doc.HasMember("session_id")) {
        if (doc["session_id"].IsString()) {
            body.session_id = doc["session_id"].GetString();
        } else {
            LOG_ERROR << "http request's <session_id> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <session_id> is not string");
            return;
        }
    }
    // session_id_sign
    if (doc.HasMember("session_id_sign")) {
        if (doc["session_id_sign"].IsString()) {
            body.session_id_sign = doc["session_id_sign"].GetString();
        } else {
            LOG_ERROR << "http request's <session_id_sign> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <session_id_sign> is not string");
            return;
        }
    }
    // pub_key
    body.pub_key = conf_manager::instance().get_pub_key();
    if (body.pub_key.empty()) {
        LOG_ERROR << "pub_key is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "pub_key is empty");
        return;
    }

    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_create_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        LOG_ERROR << "creaate node request failed";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "creaate node request failed");
        return;
    }

    if (E_SUCCESS != create_request_session(NODE_CREATE_TASK_TIMER, httpReq, node_req_msg, head_session_id)) {
        LOG_ERROR << "create request session and timer(NODE_CREATE_TASK_TIMER) failed";
        return;
    }

    if (dbc::network::connection_manager::instance().broadcast_message(node_req_msg) != E_SUCCESS) {
        LOG_ERROR << "broadcast failed";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast failed");
        return;
    }
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_create_task_req_msg(const std::string &head_session_id, const req_body& body) {
    if (body.peer_nodes_list.empty()) {
        LOG_ERROR << "create failed: no peer_nodes_list!";
        return nullptr;
    }

    auto req_content = std::make_shared<dbc::node_create_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_CREATE_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    exten_info["sign"] = body.sign;
    exten_info["nonce"] = body.nonce;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);

    // body
    dbc::node_create_task_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    dbc::network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    dbc::node_service_info service_info;
    bool bfound = service_info_collection::instance().find(body.peer_nodes_list[0], service_info);
    if (bfound) {
        std::string pub_key = service_info.kvs.count("pub_key") ? service_info.kvs["pub_key"] : "";
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        }
    }

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_CREATE_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_create_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(rsp_msg)) {
        LOG_ERROR << "rsp header check failed";
        return;
    }

    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_create_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    if (!check_nonce(node_rsp_msg->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return;
    }

    int32_t rsp_result = node_rsp_msg->body.result;
    std::string rsp_result_msg = node_rsp_msg->body.result_msg;

    std::string sign_msg = node_rsp_msg->header.nonce + node_rsp_msg->header.session_id;
    if (!util::verify_sign(node_rsp_msg->header.exten_info["sign"], sign_msg, node_rsp_msg->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    const std::shared_ptr<dbc::network::http_request> &httpReq = hreq_context->m_hreq;
    if (rsp_result != 0) {
        httpReq->reply_comm_rest_err2(HTTP_OK, rsp_result_msg);
    } else {
        httpReq->reply_comm_rest_succ2(rsp_result_msg);
    }
}

void rest_api_service::on_node_create_task_timer(const std::shared_ptr<core_timer>& timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "rsp name: on_node_create_task_timer, session_id:" << session_id << ", but get null hreq_key";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    try {
        auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<dbc::network::http_request_context>>();
        if (nullptr != hreq_context) {
            std::stringstream ss;
            ss << "{";
            ss << "\"error_code\":" << -1;
            ss << ", \"error_message\":\"create task timeout\"";
            ss << "}";
            hreq_context->m_hreq->reply_comm_rest_err2(HTTP_INTERNAL, ss.str());
        }
    } catch (std::exception &e) {
        LOG_ERROR << "error: " << e.what();
    }

    session->clear();
    this->remove_session(session_id);
}

// start task
void rest_api_service::rest_start_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, use /api/v1/tasks/<task_id>/start");
        return;
    }

    req_body body;

    body.task_id = path_list[0];

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, use /api/v1/tasks/start");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "http request body is invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is invalid json");
        return;
    }

    // peer_nodes_list
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            uint32_t list_size = doc["peer_nodes_list"].Size();
            if (list_size < 1) {
                LOG_ERROR << "http request's <peer_nodes_list> size < 1";
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <peer_nodes_list> size < 1");
                return;
            }

            // 暂时只支持一次操作1个节点
            std::string node(doc["peer_nodes_list"][0].GetString());
            body.peer_nodes_list.push_back(node);
        } else {
            LOG_ERROR << "http request's <peer_nodes_list> is not array";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <peer_nodes_list> is not array");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <peer_nodes_list>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <peer_nodes_list>");
        return;
    }
    // additional
    if (doc.HasMember("additional")) {
        if (doc["additional"].IsObject()) {
            const rapidjson::Value &obj = doc["additional"];
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            obj.Accept(writer);
            body.additional = buffer.GetString();
        } else {
            LOG_ERROR << "http request's <additional> is not object";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <additional> is not object");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <additional>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <additional>");
        return;
    }
    // sign
    if (doc.HasMember("sign")) {
        if (doc["sign"].IsString()) {
            body.sign = doc["sign"].GetString();
        } else {
            LOG_ERROR << "http request's <sign> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <sign> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <sign>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <sign>");
        return;
    }
    // nonce
    if (doc.HasMember("nonce")) {
        if (doc["nonce"].IsString()) {
            body.nonce = doc["nonce"].GetString();
        } else {
            LOG_ERROR << "http request's <nonce> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <nonce> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <nonce>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <nonce>");
        return;
    }
    // wallet
    if (doc.HasMember("wallet")) {
        if (doc["wallet"].IsString()) {
            body.wallet = doc["wallet"].GetString();
        } else {
            LOG_ERROR << "http request's <wallet> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <wallet> is not string");
            return;
        }
    } else {
        LOG_ERROR << "http request has no <wallet>";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request has no <wallet>");
        return;
    }
    // session_id
    if (doc.HasMember("session_id")) {
        if (doc["session_id"].IsString()) {
            body.session_id = doc["session_id"].GetString();
        } else {
            LOG_ERROR << "http request's <session_id> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <session_id> is not string");
            return;
        }
    }
    // session_id_sign
    if (doc.HasMember("session_id_sign")) {
        if (doc["session_id_sign"].IsString()) {
            body.session_id_sign = doc["session_id_sign"].GetString();
        } else {
            LOG_ERROR << "http request's <session_id_sign> is not string";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request's <session_id_sign> is not string");
            return;
        }
    }
    // pub_key
    body.pub_key = conf_manager::instance().get_pub_key();
    if (body.pub_key.empty()) {
        LOG_ERROR << "pub_key is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "pub_key is empty");
        return;
    }

    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_start_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        LOG_ERROR << "creaate node request failed";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "creaate node request failed");
        return;
    }

    if (E_SUCCESS != create_request_session(NODE_START_TASK_TIMER, httpReq, node_req_msg, head_session_id)) {
        LOG_ERROR << "create request session and timer(NODE_START_TASK_TIMER) failed";
        return;
    }

    if (dbc::network::connection_manager::instance().broadcast_message(node_req_msg) != E_SUCCESS) {
        LOG_ERROR << "broadcast failed";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast failed");
        return;
    }
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_start_task_req_msg(const std::string &head_session_id, const req_body& body) {
    if (body.task_id.empty()) {
        LOG_ERROR << "start failed: task_id is empty!";
        return nullptr;
    }

    if (body.peer_nodes_list.empty()) {
        LOG_ERROR << "start failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    auto req_content = std::make_shared<dbc::node_start_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_START_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = body.task_id + req_content->header.nonce + body.additional;
    std::string signature = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    if (signature.empty()) return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_task_id(body.task_id);
    req_content->body.__set_peer_nodes_list(body.peer_nodes_list);
    req_content->body.__set_additional(body.additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_START_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_start_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                              const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(rsp_msg)) {
        LOG_ERROR << "rsp header check failed";
        return;
    }

    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_start_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "rsp is nullptr";
        return;
    }

    if (!check_nonce(node_rsp_msg->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return;
    }

    int32_t rsp_result = node_rsp_msg->body.result;
    std::string rsp_result_msg = node_rsp_msg->body.result_msg;

    std::string sign_msg = node_rsp_msg->header.nonce + node_rsp_msg->header.session_id;
    if (!util::verify_sign(node_rsp_msg->header.exten_info["sign"], sign_msg, node_rsp_msg->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return;
    }

    const std::shared_ptr<dbc::network::http_request> &httpReq = hreq_context->m_hreq;
    if (rsp_result != 0) {
        httpReq->reply_comm_rest_err2(HTTP_OK, rsp_result_msg);
    } else {
        httpReq->reply_comm_rest_succ2(rsp_result_msg);
    }
}

void rest_api_service::on_node_start_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "rsp name: on_node_create_task_timer, session_id:" << session_id << ", but get null hreq_key";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    try {
        auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<dbc::network::http_request_context>>();
        if (nullptr != hreq_context) {
            std::stringstream ss;
            ss << "{";
            ss << "\"error_code\":" << -1;
            ss << ", \"error_message\":\"start task timeout\"";
            ss << "}";
            hreq_context->m_hreq->reply_comm_rest_err2(HTTP_INTERNAL, ss.str());
        }
    } catch (std::exception &e) {
        LOG_ERROR << "error: " << e.what();
    }

    session->clear();
    this->remove_session(session_id);
}

// stop task
void rest_api_service::rest_stop_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list. POST /api/v1/tasks/<task_id>/stop")
                    return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::shared_ptr<cmd_stop_task_req> cmd_req = std::make_shared<cmd_stop_task_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_stop_task_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_stop_task_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "restart failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    std::shared_ptr<dbc::node_stop_task_req> req_content = std::make_shared<dbc::node_stop_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_STOP_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    if (signature.empty()) return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_task_id(cmd_req->task_id);
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_STOP_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_STOP_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);
    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);
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

void rest_api_service::on_node_stop_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                             const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_stop_task_rsp> rsp_content = std::dynamic_pointer_cast<dbc::node_stop_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg);
        return E_SUCCESS;
    }

    std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_stop_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_stop_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp = std::make_shared<::cmd_stop_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    cmd_rsp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"stop task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// restart task
void rest_api_service::rest_restart_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/start")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid api. Use /api/v1/tasks/<task_id>/start")
        return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::shared_ptr<cmd_restart_task_req> cmd_req = std::make_shared<cmd_restart_task_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_restart_task_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_restart_task_req_msg(const std::string &head_session_id,
                    const req_body& body) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "restart failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    auto req_content = std::make_shared<dbc::node_restart_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_RESTART_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    if (signature.empty()) return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_task_id(cmd_req->task_id);
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_RESTART_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_RESTART_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);
    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);
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

void rest_api_service::on_node_restart_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                   const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_restart_task_rsp> rsp_content = std::dynamic_pointer_cast<dbc::node_restart_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg);
        return E_SUCCESS;
    }

    std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_restart_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_restart_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp = std::make_shared<::cmd_restart_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    cmd_rsp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"restart task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// reset task
void rest_api_service::rest_reset_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/reset")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list. POST /api/v1/tasks/<task_id>/reset")
                    return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::shared_ptr<cmd_reset_task_req> cmd_req = std::make_shared<cmd_reset_task_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_reset_task_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_reset_task_req_msg(const std::string &head_session_id,
                                                                                        const req_body& body) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "restart failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    std::shared_ptr<dbc::node_reset_task_req> req_content = std::make_shared<dbc::node_reset_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_RESET_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    if (signature.empty()) return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_task_id(cmd_req->task_id);
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_RESET_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_RESET_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);
    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);
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

void rest_api_service::on_node_reset_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                 const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_reset_task_rsp> rsp_content = std::dynamic_pointer_cast<dbc::node_reset_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg);
        return E_SUCCESS;
    }

    std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_reset_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_reset_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp = std::make_shared<::cmd_reset_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    cmd_rsp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"reset task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// delete task
void rest_api_service::rest_delete_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list. POST /api/v1/tasks/<task_id>/stop")
                    return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::shared_ptr<cmd_delete_task_req> cmd_req = std::make_shared<cmd_delete_task_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_delete_task_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_delete_task_req_msg(const std::string &head_session_id,
                                                                                         const req_body& body) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "restart failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    std::shared_ptr<dbc::node_delete_task_req> req_content = std::make_shared<dbc::node_delete_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_DELETE_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_msg, conf_manager::instance().get_node_private_key());
    if (signature.empty()) return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_task_id(cmd_req->task_id);
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_DELETE_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_DELETE_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);
    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);
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

void rest_api_service::on_node_delete_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_delete_task_rsp> rsp_content = std::dynamic_pointer_cast<dbc::node_delete_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg);
        return E_SUCCESS;
    }

    std::shared_ptr<::cmd_delete_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_delete_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_delete_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_delete_task_rsp> cmd_rsp = std::make_shared<::cmd_delete_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"delete task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// list task
void rest_api_service::rest_list_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks")
                    return nullptr;
    }

    std::shared_ptr<cmd_list_task_req> cmd_req = std::make_shared<cmd_list_task_req>();

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() == 1) {
        cmd_req->task_id = path_list[0];
    } else {
        cmd_req->task_id = "";
    }

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_list_task_req).name());
    msg->set_content(cmd_req);

    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_list_task_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    auto req_content = std::make_shared<dbc::node_list_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_LIST_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = cmd_req->task_id + req_content->header.nonce +
            req_content->header.session_id + cmd_req->additional;
    std::string sign = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    if (!cmd_req->task_id.empty()) {
        req_content->body.__set_task_id(cmd_req->task_id);
    } else {
        req_content->body.__set_task_id("");
    }
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_LIST_TASK_REQ);
    req_msg->set_content(req_content);

    //add timer
    uint32_t timer_id = add_timer(NODE_LIST_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                  ONLY_ONE_TIME, req_content->header.session_id);

    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);

    variable_value val;
    val.value() = req_msg;
    session->get_context().add("req_msg", val);

    int32_t ret = this->add_session(session->get_session_id(), session);
    if (E_SUCCESS != ret) {
        remove_timer(timer_id);
        LOG_ERROR << "internal error while processing this cmd";
        return nullptr;
    }

    return req_msg;
}

void rest_api_service::on_node_list_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                             const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_list_task_rsp> rsp_content =
            std::dynamic_pointer_cast<dbc::node_list_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check error ";
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg, msg->header.src_sid);
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::shared_ptr<::cmd_list_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_list_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_list_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_list_task_rsp> cmd_resp = std::make_shared<::cmd_list_task_rsp>();

    if (nullptr == timer) {
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "null ptr of timer.";
        topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);
        return E_NULL_POINTER;
    }

    std::string session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "ai power requester service list training timer get session null: " + session_id;
        topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);
        return E_DEFAULT;
    }

    cmd_resp->header.__set_session_id(session_id);
    cmd_resp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"get task list timeout\"";
    ss << "}";
    cmd_resp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// modify task
void rest_api_service::rest_modify_task(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list. POST /api/v1/tasks/<task_id>/modify")
                    return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::shared_ptr<cmd_modify_task_req> cmd_req = std::make_shared<cmd_modify_task_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_modify_task_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_modify_task_req_msg(const std::string &head_session_id,
                                                                                         const req_body& body) {

    return nullptr;
}

void rest_api_service::on_node_modify_task_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<dbc::network::message>& rsp_msg) {

    return E_SUCCESS;
}

void rest_api_service::on_node_modify_task_timer(const std::shared_ptr<core_timer> &timer) {

    return E_SUCCESS;
}

// task logs
void rest_api_service::rest_task_logs(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                    "Only support POST requests. POST /api/v1/tasks/<task_id>/logs")
                    return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list. GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100")
                    return nullptr;
    }

    const std::string &task_id = path_list[0];

    std::map<std::string, std::string> query_table;
    util::split_path_into_kvs(path, query_table);

    std::string head_or_tail = query_table["flag"];
    std::string number_of_lines = query_table["line_num"];

    if (head_or_tail.empty()) {
        head_or_tail = "tail";
        number_of_lines = "100";
    } else if (head_or_tail == "head" || head_or_tail == "tail") {
        if (number_of_lines.empty()) {
            number_of_lines = "100";
        }
    } else {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "Invalid path_list.GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100")
                    return nullptr;
    }

    std::shared_ptr<cmd_task_logs_req> cmd_req = std::make_shared<cmd_task_logs_req>();
    cmd_req->task_id = task_id;

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
        return nullptr;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    auto lines = (uint64_t) std::stoull(number_of_lines);
    if (lines > MAX_NUMBER_OF_LINES) {
        lines = MAX_NUMBER_OF_LINES - 1;
    }
    cmd_req->number_of_lines = (uint16_t) lines;

    //tail or head
    if (head_or_tail == "tail") {
        cmd_req->head_or_tail = GET_LOG_TAIL;
    } else if (head_or_tail == "head") {
        cmd_req->head_or_tail = GET_LOG_HEAD;
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_task_logs_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_task_logs_req_msg(const std::string &head_session_id,
                                                   const req_body& body) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "restart failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "restart failed: no peer_nodes_list!";
        return nullptr;
    }

    if (GET_LOG_HEAD != cmd_req->head_or_tail && GET_LOG_TAIL != cmd_req->head_or_tail) {
        LOG_ERROR << "log direction error";
        return nullptr;
    }

    if (cmd_req->number_of_lines > MAX_NUMBER_OF_LINES) {
        LOG_ERROR << "number of lines error: should less than " + std::to_string(cmd_req->number_of_lines);
        return nullptr;
    }

    auto req_content = std::make_shared<dbc::node_task_logs_req>();
    //header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_TASK_LOGS_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sig_msg = cmd_req->task_id + req_content->header.nonce + req_content->header.session_id
            + cmd_req->additional;
    std::string signature = util::sign(sig_msg, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    time_t cur = std::time(nullptr);
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    //body
    req_content->body.__set_task_id(cmd_req->task_id);
    req_content->body.__set_head_or_tail(cmd_req->head_or_tail);
    req_content->body.__set_number_of_lines(cmd_req->number_of_lines);
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_TASK_LOGS_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_TASK_LOGS_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
                                        ONLY_ONE_TIME, req_content->header.session_id);

    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                 req_content->header.session_id);

    variable_value val;
    val.value() = req_msg;
    session->get_context().add("req_msg", val);

    variable_value v2;
    v2.value() = cmd_req->task_id;
    session->get_context().add("task_id", v2);

    int32_t ret = this->add_session(session->get_session_id(), session);
    if (E_SUCCESS != ret) {
        this->remove_timer(timer_id);
        LOG_ERROR << "internal error while processing this cmd";
        return nullptr;
    }

    return req_msg;
}

void rest_api_service::on_node_task_logs_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                const std::shared_ptr<dbc::network::message>& rsp_msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_task_logs_rsp> rsp_content =
            std::dynamic_pointer_cast<dbc::node_task_logs_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    //check rsp body
    int32_t rsp_result = rsp_content->body.result;
    std::string rsp_result_msg = rsp_content->body.result_msg;
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(rsp_result_msg.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        return E_DEFAULT;
    }

    std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
    if (!util::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
        LOG_ERROR << "verify sign failed";
        return E_DEFAULT;
    }

    std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
    if (nullptr == session) {
        dbc::network::connection_manager::instance().send_resp_message(msg);
        return E_SUCCESS;
    }

    std::shared_ptr<::cmd_task_logs_rsp> cmd_rsp_msg = std::make_shared<::cmd_task_logs_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_task_logs_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_task_logs_rsp> cmd_resp = std::make_shared<::cmd_task_logs_rsp>();
    if (nullptr == timer) {
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);
        return E_NULL_POINTER;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);
        return E_DEFAULT;
    }

    cmd_resp->header.__set_session_id(session_id);
    cmd_resp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"get task log timeout\"";
    ss << "}";
    cmd_resp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// list mining node
void rest_api_service::rest_mining_nodes(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != dbc::network::http_request::POST) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "Only support POST requests")
        return nullptr;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() > 1) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "No nodeid count specified. Use /api/v1/mining_nodes/{nodeid}/{key}")
                    return nullptr;
    }

    // parse body
    std::string body = httpReq->read_body();
    if (body.empty()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks")
        return nullptr;
    }

    std::shared_ptr<cmd_list_node_req> cmd_req = std::make_shared<cmd_list_node_req>();

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str())
        return nullptr;
    }

    if (!doc.IsObject()) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON")
        return nullptr;
    }

    JSON_PARSE_OBJECT_TO_STRING(doc, "additional", cmd_req->additional)
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < doc["peer_nodes_list"].Size(); i++) {
                std::string node(doc["peer_nodes_list"][i].GetString());
                cmd_req->peer_nodes_list.push_back(node);

                // todo: 暂时只支持一次操作1个节点
                break;
            }
        }
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_list_node_req).name());
    msg->set_content(cmd_req);
    return msg;
}

std::shared_ptr<dbc::network::message> rest_api_service::create_node_query_node_info_req_msg(const std::string &head_session_id,
                                                                                             const req_body& body) {
    auto req_content = std::make_shared<dbc::node_query_node_info_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(cmd_req->header.session_id);
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = req_content->header.nonce + cmd_req->additional;
    std::string signature = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    if (signature.empty())  return nullptr;
    exten_info["sign"] = signature;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
    exten_info["origin_id"] = conf_manager::instance().get_node_id();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
    req_content->body.__set_additional(cmd_req->additional);

    auto req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_QUERY_NODE_INFO_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_QUERY_NODE_INFO_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
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

void reply_node_info(const std::shared_ptr<cmd_list_node_rsp> &resp, rapidjson::Value &data,
                     rapidjson::Document::AllocatorType &allocator) {
    for (auto &kv: resp->kvs) {
        if (kv.second.length() > 0 && (kv.second[0] == '{' || kv.second[0] == '[')) {
            rapidjson::Document doc;
            if (!doc.Parse<0>(kv.second.c_str()).HasParseError()) {
                rapidjson::Value v = rapidjson::Value(doc, allocator);
                data.AddMember(STRING_DUP(kv.first), v, allocator);
            }
        } else {
            fill_json_filed_string(data, allocator, kv.first, kv.second);
        }
    }
}

void reply_node_list(const std::shared_ptr<cmd_list_node_rsp> &resp, rapidjson::Value &data,
                     rapidjson::Document::AllocatorType &allocator) {
    std::map<std::string, dbc::node_service_info> s_in_order;
    for (auto &it : *(resp->id_2_services)) {
        s_in_order[it.first] = it.second;
    }

    rapidjson::Value mining_nodes(rapidjson::kArrayType);
    for (auto &it : s_in_order) {
        rapidjson::Value node_info(rapidjson::kObjectType);
        std::string node_id = it.first;
        node_id = util::rtrim(node_id, '\n');
        std::string ver = it.second.kvs.count("version") ? it.second.kvs["version"] : "N/A";

        std::string service_list = resp->to_string(it.second.service_list);
        node_info.AddMember("service_list", STRING_DUP(service_list), allocator);
        node_info.AddMember("nodeid", STRING_DUP(node_id), allocator);
        node_info.AddMember("name", STRING_DUP(it.second.name), allocator);
        node_info.AddMember("version", STRING_DUP(ver), allocator);
        node_info.AddMember("state", STRING_DUP(it.second.kvs["state"]), allocator);

        mining_nodes.PushBack(node_info, allocator);
    }

    data.AddMember("mining_nodes", mining_nodes, allocator);
}

int32_t RestHandler::on_cmd_list_node_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
    INIT_RSP_CONTEXT(cmd_list_node_req, cmd_list_node_rsp)

    if (resp->result != 0) {
        ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
        return E_DEFAULT;
    }

    if (req->peer_nodes_list.empty()) {
        reply_node_list(resp, data, allocator);
    } else {
        reply_node_info(resp, data, allocator);
    }

    SUCC_REPLY(data)
    return E_SUCCESS;
}

void rest_api_service::on_node_query_node_info_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                      const std::shared_ptr<dbc::network::message>& rsp_msg) {
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
        dbc::network::connection_manager::instance().send_resp_message(msg);
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
    topic_manager::instance().publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

void rest_api_service::on_node_query_node_info_timer(const std::shared_ptr<core_timer>& timer)
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
    topic_manager::instance().publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);

    session->clear();
    remove_session(session_id);
    return E_SUCCESS;
}

// /peers/
void rest_api_service::rest_peers(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() != 1) {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                    "No option count specified. Use /api/v1/peers/{option}")
                    return nullptr;
    }

    const std::string &option = path_list[0];
    std::shared_ptr<cmd_get_peer_nodes_req> req = std::make_shared<cmd_get_peer_nodes_req>();
    req->flag = flag_active;

    if (option == "active") {
        req->flag = flag_active;
    } else if (option == "global") {
        req->flag = flag_global;
    } else {
        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid option. The option is active or global.")
        return nullptr;
    }

    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_name(typeid(cmd_get_peer_nodes_req).name());
    msg->set_content(req);
    return msg;
}

void rest_api_service::on_cmd_get_peer_nodes_rsp(const std::shared_ptr<dbc::network::http_request_context>& hreq_context,
                                                 const std::shared_ptr<dbc::network::message>& rsp_msg) {
    INIT_RSP_CONTEXT(cmd_get_peer_nodes_req, cmd_get_peer_nodes_rsp)

    if (resp->result != 0) {
        ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
        return E_DEFAULT;
    }

    rapidjson::Value peer_nodes_list(rapidjson::kArrayType);
    for (auto it = resp->peer_nodes_list.begin(); it != resp->peer_nodes_list.end(); it++) {
        rapidjson::Value cmd_peer_node_info(rapidjson::kObjectType);
        cmd_peer_node_info.AddMember("peer_node_id", STRING_REF(it->peer_node_id), allocator);
        cmd_peer_node_info.AddMember("live_time_stamp", it->live_time_stamp, allocator);
        cmd_peer_node_info.AddMember("net_st", STRING_DUP(net_state_2_string(it->net_st)), allocator);
        cmd_peer_node_info.AddMember("ip", STRING_DUP(util::fuzz_ip(it->addr.ip)), allocator);
        cmd_peer_node_info.AddMember("port", it->addr.port, allocator);
        cmd_peer_node_info.AddMember("node_type", STRING_DUP(node_type_2_string(it->node_type)), allocator);

        rapidjson::Value service_list(rapidjson::kArrayType);
        for (auto & s : it->service_list) {
            service_list.PushBack(rapidjson::StringRef(s.c_str(), s.length()), allocator);
        }
        cmd_peer_node_info.AddMember("service_list", service_list, allocator);

        peer_nodes_list.PushBack(cmd_peer_node_info, allocator);
    }

    data.AddMember("peer_nodes_list", peer_nodes_list, allocator);

    httpReq->reply_comm_rest_succ(data);

    return E_SUCCESS;
}

// /stat/
void rest_api_service::rest_stat(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    rapidjson::Document document;
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    rapidjson::Value data(rapidjson::kObjectType);
    std::string node_id = conf_manager::instance().get_node_id();

    data.AddMember("node_id", STRING_REF(node_id), allocator);
    data.AddMember("session_count", rest_api_service::instance().get_session_count(), allocator);
    data.AddMember("startup_time", rest_api_service::instance().get_startup_time(), allocator);
    SUCC_REPLY(data)
    return nullptr;
}

// /config/
void rest_api_service::rest_config(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {

        std::string body = httpReq->read_body();
        if (body.empty()) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/config")
            return nullptr;
        }

        rapidjson::Document document;
        try {
            document.Parse(body.c_str());
            if (!document.IsObject()) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON. Use /api/v1/config")
                return nullptr;
            }
        } catch (...) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_MISC_ERROR, "Parse JSON Error. Use /api/v1/config")
            return nullptr;
        }

        std::string log_level;


        JSON_PARSE_STRING(document, "log_level", log_level)

        std::map<std::string, uint32_t> log_level_to_int = {

                {"trace",   0},
                {"debug",   1},
                {"info",    2},
                {"warning", 3},
                {"error",   4},
                {"fatal",   5}
        };

        if (log_level_to_int.count(log_level)) {

            LOG_ERROR << "set log level " << log_level;
            uint32_t log_level_int = log_level_to_int[log_level];
            log::set_filter_level((boost::log::trivial::severity_level)
            log_level_int);
        } else {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_MISC_ERROR, "illegal log level value")
            return nullptr;
        }
    }


    rapidjson::Document document;
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    rapidjson::Value data(rapidjson::kObjectType);

    data.AddMember("result", "ok", allocator);
    SUCC_REPLY(data)

    return nullptr;
}

// /
void rest_api_service::rest_api_version(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path) {
    rapidjson::Document document;
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    rapidjson::Value data(rapidjson::kObjectType);

    data.AddMember("version", STRING_REF(REST_API_VERSION), allocator);

    std::string node_id = conf_manager::instance().get_node_id();

    data.AddMember("node_id", STRING_REF(node_id), allocator);
    SUCC_REPLY(data)
    return nullptr;
}

int32_t rest_api_service::on_binary_forward(std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "recv logs_resp but msg is nullptr";
        return E_DEFAULT;
    }


    std::shared_ptr<dbc::network::msg_forward> content = std::dynamic_pointer_cast<dbc::network::msg_forward>(
            msg->content);

    if (!content) {
        LOG_ERROR << "not a valid binary forward msg";
        return E_DEFAULT;
    }

    // support request message name end with "_req" postfix
    auto &msg_name = msg->content->header.msg_name;

    if (msg_name.substr(msg_name.size() - 4) == std::string("_req")) {
        // add path
        msg->content->header.path.push_back(conf_manager::instance().get_node_id());

        LOG_INFO << "broadcast_message binary forward msg";
        dbc::network::connection_manager::instance().broadcast_message(msg);
    } else {
        dbc::network::connection_manager::instance().send_resp_message(msg);
    }

    return E_SUCCESS;
}

